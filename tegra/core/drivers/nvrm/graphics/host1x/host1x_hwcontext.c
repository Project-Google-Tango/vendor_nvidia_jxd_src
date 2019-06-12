/*
 * Copyright (c) 2007-2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvrm_drf.h"
#include "nvassert.h"
#include "host1x_hwcontext.h"
#include "arhost1x.h"
#include "arhost1x_uclass.h"
#include "arhost1x_channel.h"
#include "class_ids.h"
#include "arhost1x_sync.h"
#include "host1x_channel.h"

// FIXME: repeats code from ap15rm_channel.c.
#define CHANNEL_REGR( ch, reg ) \
    NV_READ32( (ch)->ChannelRegs + ((HOST1X_CHANNEL_##reg##_0)/4) )

typedef struct RmContextQueue_t
{
    NvRmContextHandle hContext;
    NvRmMemHandle hHostWait;
    NvU32 SyncPointID;
    struct RmContextQueue_t *next;
    NvRmChHw *HwState;
} RmContextQueue;

// FIXME: assuming single channel for now.
static RmContextQueue *s_ContextQueueHead = NULL;
static RmContextQueue *s_ContextQueueTail = NULL;

static RmContextQueue s_QueueElement;
static RmContextQueue *s_ContextQueueSingleElement = NULL;
static NvRmContext s_Context;

// Context queue mutex.
static NvOsMutexHandle s_HwContextMutex;

static void
NvRmPrivSetContextSingle( NvRmChannelHandle hChannel,
    NvRmContextHandle hContext, NvU32 SyncPointID, NvU32 SyncPointValue )
{
    if( s_ContextQueueSingleElement ) {
        NvRmContextHandle hCtx = s_ContextQueueSingleElement->hContext; (void)hCtx;
        NV_ASSERT(s_ContextQueueSingleElement->SyncPointID == SyncPointID);
        NV_ASSERT(s_ContextQueueSingleElement->HwState == hChannel->HwState);
    }
    else
    {
        RmContextQueue *ctx = s_ContextQueueSingleElement = &s_QueueElement;

        s_Context = *hContext;
        ctx->hContext = &s_Context;
        ctx->hHostWait = 0;
        ctx->SyncPointID = SyncPointID;
        ctx->HwState = hChannel->HwState;
    }
}

// SyncPointValue is the value we'll be waiting for.
static void
NvRmPrivAddContext( NvRmChannelHandle hChannel, NvRmContextHandle hContext,
    NvRmContextHandle newContext,
    NvRmMemHandle hHostWait,
    NvU32 SyncPointID, NvU32 SyncPointValue )
{
    RmContextQueue *ctx;

    ctx = NvOsAlloc( sizeof(RmContextQueue) );
    if( !ctx )
    {
        NV_ASSERT(!"NvRmPrivAddContext: NvOsAlloc failed");
        return;// NvError_InsufficientMemory;
    }

    ctx->hContext = hContext;
    ctx->hHostWait = hHostWait;
    ctx->SyncPointID = SyncPointID;
    ctx->HwState = hChannel->HwState;

    NvOsMutexLock( s_HwContextMutex );
    ctx->hContext->RefCount++;
    if ( s_ContextQueueTail )
    {
        s_ContextQueueTail->next = ctx;
        s_ContextQueueTail = ctx;
        s_ContextQueueTail->next = NULL;
    }
    else
    {
        ctx->next = NULL;
        s_ContextQueueHead = s_ContextQueueTail = ctx;
    }
    NvOsMutexUnlock( s_HwContextMutex );
}

static NvU32
NvRmPrivGetHostWaitCommands( NvRmChannelHandle hChannel, NvRmMemHandle *pMem,
    NvU32 ClassID, NvU32 SyncPointID, NvU32 SyncPointValue, NvRmModuleID ModuleID )
{
    NvError err;
    NvU32 base;
    NvU32 TotalSyncPointIncr = 0;

#define CONTEXT_SAVE_FIRST_CHUNK_SIZE 7
    // Second chunk is NvRmContest::hContestSave.
#define CONTEXT_SAVE_THIRD_CHUNK_SIZE 6
    NvU32 commands[CONTEXT_SAVE_FIRST_CHUNK_SIZE+CONTEXT_SAVE_THIRD_CHUNK_SIZE];
    NvU32 *cptr = commands;

    // Words 0-6 are executed right before register reads.

    // set class to the unit to flush
    *cptr++ = NVRM_CH_OPCODE_SET_CLASS( ClassID, 0, 0) ;
    // sync point increment
    *cptr++ = NVRM_CH_OPCODE_NONINCR( 0x0, 1 );
    *cptr++ = (1UL << 8) | (NvU8)(SyncPointID & 0xff);
    // set class to host
    *cptr++ = NVRM_CH_OPCODE_SET_CLASS( NV_HOST1X_CLASS_ID, 0, 0 );
    // wait for SyncPointValue+1
    *cptr++ = NVRM_CH_OPCODE_NONINCR( NV_CLASS_HOST_WAIT_SYNCPT_0, 1 );
    *cptr++ =
        NV_DRF_NUM( NV_CLASS_HOST, WAIT_SYNCPT, INDX, SyncPointID ) |
        NV_DRF_NUM( NV_CLASS_HOST, WAIT_SYNCPT, THRESH, SyncPointValue+1 );
    // set class back to the unit
    *cptr++ = NVRM_CH_OPCODE_SET_CLASS( ClassID, 0, 0 );
    NV_ASSERT( cptr-commands == CONTEXT_SAVE_FIRST_CHUNK_SIZE );

    *cptr++ = NVRM_CH_OPCODE_SET_CLASS( NV_HOST1X_CLASS_ID, 0, 0 );
    // wait for SyncPointValue+3
    *cptr++ = NVRM_CH_OPCODE_NONINCR( NV_CLASS_HOST_WAIT_SYNCPT_0, 1 );
    *cptr++ =
        NV_DRF_NUM( NV_CLASS_HOST, WAIT_SYNCPT, INDX, SyncPointID ) |
        NV_DRF_NUM( NV_CLASS_HOST, WAIT_SYNCPT, THRESH, SyncPointValue+TotalSyncPointIncr );

    // increment sync point base by three.
    base = NvRmChannelGetModuleWaitBase( hChannel, ModuleID, 0 );
            *cptr++ = NVRM_CH_OPCODE_NONINCR( NV_CLASS_HOST_INCR_SYNCPT_BASE_0, 1 );
            *cptr++ =
                NV_DRF_NUM(NV_CLASS_HOST, INCR_SYNCPT_BASE, BASE_INDX, base ) |
                NV_DRF_NUM(NV_CLASS_HOST, INCR_SYNCPT_BASE, OFFSET, TotalSyncPointIncr);

    // set class back to the unit
    *cptr++ = NVRM_CH_OPCODE_SET_CLASS( ClassID, 0, 0 );
    NV_ASSERT( cptr-commands ==
        CONTEXT_SAVE_FIRST_CHUNK_SIZE+CONTEXT_SAVE_THIRD_CHUNK_SIZE);

    err = NvRmMemHandleAlloc(hChannel->hDevice, NULL, 0, 32,
            NvOsMemAttribute_Uncached,
            sizeof(commands), NVRM_MEM_TAG_RM_MISC, 1, pMem);
    NV_ASSERT(err == NvSuccess);

    // FIXME: need better error handling.  This silly thing is here so that
    // ARM compiler does not complain about err being set and not used in the
    // release build.
    if (err != NvSuccess)
        return 0;

    NvRmMemPin( *pMem );

    NvRmMemWrite( *pMem, 0, commands, sizeof(commands) );

    return SyncPointValue+TotalSyncPointIncr;
}

NvU32
NvRmPrivCheckForContextChange( NvRmChannelHandle hChannel,
    NvRmContextHandle hContext, NvRmModuleID ModuleID,
    NvU32 SyncPointID, NvU32 SyncPointValue, NvBool DryRun )
{
    NvU32 ret = 0;
    NvRmContext *lastContext;

    // If the new context is NULL, it means we're not interested in storing
    // and restoring it.  We'll be able to switch back to the last context
    // without extra register saves and restores.
    if( !hContext )
        return ret;

    lastContext = NVRM_CHANNEL_CONTEXT_GET(hChannel, ModuleID);

    if( hContext == lastContext ) {
        // It can happen that the previous submit operation did not want to
        // store and restore context, but it did change the current class.
        // This restores the class back to what the current context expects.

        if( !DryRun ) {
            NvRmCommandBuffer cmdBufs[1];

            cmdBufs[0].hMem = hContext->hContextNoChange;
            cmdBufs[0].Offset = 0;
            cmdBufs[0].Words = hContext->ContextNoChangeSize;

            NvRmPrivChannelSubmitCommandBufs( hChannel, cmdBufs, 1,
                NULL, NULL, 0,
                SyncPointID, SyncPointValue, NV_FALSE );
        }
        return ret;
    }

    // Save the context if we care.
    if( lastContext && lastContext != FAKE_POWERDOWN_CONTEXT )
    {
        if( !DryRun ) {
            if( lastContext->Queueless )
            {
                NvRmCommandBuffer cmdBufs[2];

                SyncPointValue += lastContext->QueuelessSyncpointIncrement;
                NV_ASSERT(lastContext->QueuelessSyncpointIncrement == 3);
                NvRmPrivSetContextSingle( hChannel, lastContext, SyncPointID, SyncPointValue-1 );

                cmdBufs[0].hMem = lastContext->hContextSetAddress;
                cmdBufs[0].Offset = 0;
                cmdBufs[0].Words = lastContext->ContextSetAddressSize;

                cmdBufs[1].hMem = lastContext->hContextSave;
                cmdBufs[1].Offset = 0;
                cmdBufs[1].Words = lastContext->ContextSaveSize;

                NvRmPrivChannelSubmitCommandBufs( hChannel, cmdBufs,
                    NV_ARRAY_SIZE(cmdBufs), NULL, NULL, 0,
                    SyncPointID, SyncPointValue,
                    NV_FALSE );
            }
            else
            {
                NvRmMemHandle HostWaitCommands;
                NvRmCommandBuffer cmdBufs[3];

                SyncPointValue = NvRmPrivGetHostWaitCommands( hChannel,
                    &HostWaitCommands, lastContext->ClassID,
                    SyncPointID, SyncPointValue, ModuleID );
                NvRmPrivAddContext( hChannel, lastContext, hContext,
                    HostWaitCommands, SyncPointID, SyncPointValue-1 );

                cmdBufs[0].hMem = HostWaitCommands;
                cmdBufs[0].Offset = 0;
                cmdBufs[0].Words = CONTEXT_SAVE_FIRST_CHUNK_SIZE;

                cmdBufs[1].hMem = lastContext->hContextSave;
                cmdBufs[1].Offset = 0;
                cmdBufs[1].Words = lastContext->ContextSaveSize;

                cmdBufs[2].hMem = HostWaitCommands;
                cmdBufs[2].Offset = CONTEXT_SAVE_FIRST_CHUNK_SIZE*4;
                cmdBufs[2].Words = CONTEXT_SAVE_THIRD_CHUNK_SIZE;

                NvRmPrivChannelSubmitCommandBufs( hChannel, cmdBufs,
                    NV_ARRAY_SIZE(cmdBufs), NULL, NULL, 0,
                    SyncPointID, SyncPointValue,
                    NV_FALSE );
            }

            // Mark the context as valid.
            lastContext->IsValid = NV_TRUE;
            lastContext->SyncPointID = SyncPointID;
            lastContext->SyncPointValue = SyncPointValue;
        }
    }

    // If the new context is valid, restore it.
    NV_ASSERT(hContext);

    if( !DryRun )
        NVRM_CHANNEL_CONTEXT_SET(hChannel, ModuleID, hContext);

    return ret;
}
