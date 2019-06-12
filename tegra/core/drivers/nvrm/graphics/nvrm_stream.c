/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvassert.h"
#include "nvrm_channel.h"
#include "nvrm_rmctrace.h"
#include "nvrm_stream.h"
#include "nvrm_hardware_access.h"
#include "nvrm_drf.h"
#include "nvrm_disasm.h"

#include "t30/class_ids.h"
#include "t30/arhost1x.h"
#include "t30/arhost1x_uclass.h"

#define NVRM_CMDBUF_GATHER_SIZE     256
#define NVRM_CMDBUF_RELOC_SIZE      1024
#define NVRM_CMDBUF_WAIT_SIZE       256
#define NVRM_SYNCPOINT_3D           22

NV_CT_ASSERT(NVRM_CMDBUF_GATHER_SIZE >= NVRM_STREAM_GATHER_TABLE_SIZE*2 + 1);
NV_CT_ASSERT(NVRM_CMDBUF_RELOC_SIZE >= NVRM_STREAM_RELOCATION_TABLE_SIZE);
NV_CT_ASSERT(NVRM_CMDBUF_WAIT_SIZE >= NVRM_STREAM_WAIT_TABLE_SIZE);
NV_CT_ASSERT((NVRM_CMDBUF_GATHER_SIZE + NVRM_CMDBUF_RELOC_SIZE)<= NVRM_CHANNEL_SUBMIT_MAX_HANDLES);

/* 2 words for sync point increment, 8 words for the 3D wait base */
#define NVRM_CMDBUF_BOOKKEEPING (40)

NV_CT_ASSERT(NVRM_CMDBUF_SIZE_MIN >=
             2 * ((NVRM_STREAM_BEGIN_MAX_WORDS * sizeof(NvU32)) +
                  NVRM_CMDBUF_BOOKKEEPING));
NV_CT_ASSERT(NVRM_CMDBUF_SIZE_MIN % NVRM_CMDBUF_GRANULARITY == 0);

typedef struct NvRmCmdBuf
{
    NvRmMemHandle hMemPing;
    NvRmMemHandle hMemPong;
    void *pMemPing;
    void *pMemPong;

    NvU32 memSize; /* size of the entire memory region */
    NvU32 current; /* offset from memory base */
    NvU32 fence; /* size of the memory */
    NvU32 last; /* offset of the last submit() */
    NvU32 pong;

    /* used to track buffer consumption by hardware */
    NvOsSemaphoreHandle sem;

    /* flushing stuff (don't put this on the stack...too big) */
    NvRmCommandBuffer GatherTable[NVRM_CMDBUF_GATHER_SIZE];
    NvRmCommandBuffer *pCurrentGather;

    NvRmChannelSubmitReloc RelocTable[NVRM_CMDBUF_RELOC_SIZE];
    NvRmChannelSubmitReloc *pCurrentReloc;
    NvU32 RelocShiftTable[NVRM_CMDBUF_RELOC_SIZE];
    NvU32 *pCurrentRelocShift;

    NvRmChannelWaitChecks WaitTable[NVRM_CMDBUF_WAIT_SIZE];
    NvRmChannelWaitChecks *pCurrentWait;
    NvU32 SyncPointsWaited; /* bitmask of referenced WAIT syncpt IDs */

    NvRmDeviceHandle hDevice;
    NvRmChannelHandle hChannel;

    NvRmFence SyncPointFences[NVRM_MAX_SYNCPOINTS_PER_SUBMIT];
    NvU32 NumSyncPointFences;
    NvRmFence PongSyncPointFences[NVRM_MAX_SYNCPOINTS_PER_SUBMIT];
    NvU32 PongNumSyncPointFences;

    NvRmSyncPointDescriptor *SyncPointIncrs;
    NvS32 SyncPointFirst;
    NvS32 SyncPointLast;

    NvU32 NumHwSyncPoints;

} NvRmCmdBuf;

struct NvRmStreamPrivateRec
{
    NvRmCmdBuf CmdBuf;

    /* Opaque pointer to RM private data associated with this stream.
     * Anything that RM does not need high-performance access to
     * should be hidden behind here, so that we don't have to
     * recompile the world when it changes
     */
    NvRmStreamPrivate *pRmPriv;

    /* This points to the end of the region of the command buffer that we are
     * currently allowed to write to.
     * This could be the end of the memory allocation,
     * the point the HW is currently fetching from, or just an
     * artificial "auto-flush" point where we want to stop and kick off the
     * work we've queued up before continuing on
     */
    NvData32 *pFence;

    /* Stores the last explicit GATHER offset */
    NvData32 *pBeginGather;

    NvU32 CtxChanged;

    /* String buffer for command disassembly */
    char *DisasmBuffer;
    NvU32 DisasmBufferSize;
    NvBool DisasmLoaded;
};

static void NvRmPrivSetPong(NvRmStream *pStream,
    NvRmCmdBuf *cmdbuf, NvU32 pong)
{
    NvRmStreamPrivate *priv = pStream->pRmPriv;

    if (pong == 1)
    {
        cmdbuf->pong = 1;
        pStream->hMem = cmdbuf->hMemPong;
        pStream->pMem = cmdbuf->pMemPong;
    }
    else
    {
        NV_ASSERT(pong == 0);
        cmdbuf->pong = 0;
        pStream->hMem = cmdbuf->hMemPing;
        pStream->pMem = cmdbuf->pMemPing;
    }

    cmdbuf->current = 0;
    cmdbuf->fence = cmdbuf->memSize;

    pStream->pBase = (NvData32 *)pStream->pMem + cmdbuf->current;
    pStream->pCurrent = pStream->pBase;
    priv->pFence = (NvData32 *)pStream->pMem + cmdbuf->fence;

    priv->pBeginGather = pStream->pBase;
}

NvError NvRmStreamGetError(NvRmStream *pStream)
{
    NvError err = pStream->ErrorFlag;
    pStream->ErrorFlag = NvSuccess;
    return err;
}

void NvRmStreamSetError(NvRmStream *pStream, NvError err)
{
    if(pStream->ErrorFlag == NvSuccess)
        pStream->ErrorFlag = err;
}

void NvRmStreamInitParamsSetDefaults(NvRmStreamInitParams* pInitParams)
{
    NvOsMemset(pInitParams, 0, sizeof(*pInitParams));
    pInitParams->cmdBufSize = 32768;
}

NvError NvRmStreamInit(NvRmDeviceHandle hDevice, NvRmChannelHandle hChannel,
    NvRmStream *pStream)
{
    NvRmStreamInitParams initParams;
    NvRmStreamInitParamsSetDefaults(&initParams);
    return NvRmStreamInitEx(hDevice, hChannel, &initParams, pStream);
}

NvError NvRmStreamInitEx(NvRmDeviceHandle hDevice,
    NvRmChannelHandle hChannel, const NvRmStreamInitParams *pInitParams,
    NvRmStream *pStream)
{
    NvError err;
    NvRmStreamPrivate *priv = NULL;
    NvRmCmdBuf *cmdbuf = NULL;
    NvU32 i;

    /* handle NULL pInitParams case */
    NvRmStreamInitParams defaultParams;
    if (!pInitParams)
    {
        NvRmStreamInitParamsSetDefaults(&defaultParams);
        pInitParams = &defaultParams;
    }
    /* Validation of input params.
     * Check cmdBufSize is at least a multiple of 8KB.
     */
    if ((pInitParams->cmdBufSize < NVRM_CMDBUF_GRANULARITY) ||
        (0 != (pInitParams->cmdBufSize & (NVRM_CMDBUF_GRANULARITY-1))))
    {
        goto fail;
    }

    if (pInitParams->cmdBufSize < NVRM_CMDBUF_SIZE_MIN)
    {
        goto fail;
    }

    /* for simulation, each stream has a command buffer which is copied to at
     * GetSpace() time. Only when the command buffer is full or when Flush()
     * is called does the command buffer get submitted to the channel.
     *
     * the stream buffer (from GetSpace()) is operating system-allocated memory
     * since it must be dereferencable (mapped into the process' virtual
     * space).
     *
     * for cases where the command buffer is mappable (real hardware),
     * the stream pointers (pBase, pCurrent, etc) point into the mapped
     * command buffer.
     */

    /* zero out the stream */
    NvOsMemset(pStream, 0, sizeof(NvRmStream));

    priv = NvOsAlloc(sizeof(*priv));
    if (!priv)
        goto fail;

    cmdbuf = &priv->CmdBuf;

    NvOsMemset(priv, 0, sizeof(*priv));

    if (NvRmChannelNumSyncPoints(&cmdbuf->NumHwSyncPoints))
        cmdbuf->NumHwSyncPoints = NVRM_MAX_SYNCPOINTS;

    if (!(cmdbuf->SyncPointIncrs = NvOsAlloc(
        sizeof(NvRmSyncPointDescriptor) * cmdbuf->NumHwSyncPoints)))
        goto fail;

    for (i = 0; i < cmdbuf->NumHwSyncPoints; ++i) {
        cmdbuf->SyncPointIncrs[i].SyncPointID = i;
        cmdbuf->SyncPointIncrs[i].Value = 0;
        cmdbuf->SyncPointIncrs[i].Prev = -1;
        cmdbuf->SyncPointIncrs[i].Next = -1;
        cmdbuf->SyncPointIncrs[i].WaitBaseID = NVRM_INVALID_WAITBASE_ID;
    }
    cmdbuf->NumSyncPointFences = 0;
    cmdbuf->PongNumSyncPointFences = 0;
    cmdbuf->SyncPointFirst = -1;
    cmdbuf->SyncPointLast = -1;

    /* initialize necessary fields for proper cleanup
     * if fail/bail-out later
     */
    pStream->pRmPriv = (void *)priv;

    err = NvOsSemaphoreCreate(&cmdbuf->sem, 0);
    if (err != NvSuccess)
        goto fail;

    /* we create 2 command buffers of size = half of what client asked */
    cmdbuf->memSize = pInitParams->cmdBufSize;
    cmdbuf->memSize = cmdbuf->memSize/2;

    /* Allocate ping buffer */
    err = NvRmMemHandleAlloc(hDevice, NULL, 0, 32,
            NvOsMemAttribute_WriteCombined, cmdbuf->memSize,
            NVRM_MEM_TAG_RM_MISC, 1, &cmdbuf->hMemPing);
    if (err != NvSuccess)
        goto fail;

    err = NvRmMemMap(cmdbuf->hMemPing, 0, cmdbuf->memSize, NVOS_MEM_READ_WRITE,
        &cmdbuf->pMemPing);
    if (err != NvSuccess)
        cmdbuf->pMemPing = 0;

    /* Allocate pong buffer */
    err = NvRmMemHandleAlloc(hDevice, NULL, 0, 32,
            NvOsMemAttribute_WriteCombined, cmdbuf->memSize,
            NVRM_MEM_TAG_RM_MISC, 1, &cmdbuf->hMemPong);
    if (err != NvSuccess)
        goto fail;

    err = NvRmMemMap(cmdbuf->hMemPong, 0, cmdbuf->memSize, NVOS_MEM_READ_WRITE,
        &cmdbuf->pMemPong);
    if (err != NvSuccess)
        cmdbuf->pMemPong = 0;

    /* start off with first command buffer */
    NvRmPrivSetPong(pStream, cmdbuf, 0);

    cmdbuf->last = 0;
    cmdbuf->pCurrentGather = &cmdbuf->GatherTable[0];
    cmdbuf->pCurrentReloc = &cmdbuf->RelocTable[0];
    cmdbuf->pCurrentRelocShift = &cmdbuf->RelocShiftTable[0];
    cmdbuf->pCurrentWait = &cmdbuf->WaitTable[0];
    cmdbuf->hDevice= hDevice;
    cmdbuf->hChannel = hChannel;

    pStream->Flags = 0;
    pStream->SyncPointID = NVRM_INVALID_SYNCPOINT_ID;
    pStream->WaitBaseID = NvRmChannelGetModuleWaitBase(hChannel, 0, 0);
    pStream->SyncPointsUsed = 0;
    pStream->LastEngineUsed = (NvRmModuleID)0;
    pStream->UseImmediate = NV_FALSE;

    priv->DisasmBuffer = NULL;
    priv->DisasmBufferSize = 0;
    priv->DisasmLoaded = NV_FALSE;

    return NvSuccess;

fail:
    NvRmStreamFree(pStream);
    return NvError_RmStreamInitFailure;
}

void NvRmStreamFree(NvRmStream *pStream)
{
    NvRmStreamPrivate *priv;

    if (!pStream)
        return;

    priv = pStream->pRmPriv;

    if (priv)
    {
        NvRmCmdBuf *cmdbuf = &priv->CmdBuf;
        NvU32 i;
        /* make sure the stream is flushed */
        for (i = 0; i < cmdbuf->NumSyncPointFences; ++i)
            NvRmChannelSyncPointWait(cmdbuf->hDevice,
                cmdbuf->SyncPointFences[i].SyncPointID,
                cmdbuf->SyncPointFences[i].Value,
                cmdbuf->sem);
        NvOsSemaphoreDestroy(cmdbuf->sem);
        NvRmMemUnmap(cmdbuf->hMemPing, cmdbuf->pMemPing, cmdbuf->memSize);
        NvRmMemHandleFree(cmdbuf->hMemPing);
        NvRmMemUnmap(cmdbuf->hMemPong, cmdbuf->pMemPong, cmdbuf->memSize);
        NvRmMemHandleFree(cmdbuf->hMemPong);

        NvOsFree(priv->DisasmBuffer);
        if (priv->DisasmLoaded)
            NvRmDisasmLibraryUnload();

        NvOsFree(cmdbuf->SyncPointIncrs);
        NvOsFree(priv);
        pStream->pRmPriv = NULL;
    }
}

static void NvRmPrivDisasm(NvRmStream *pStream)
{
    char str[200];
    NvS32 offset;
    NvStreamDisasmContext *context = NULL;
    NvRmStreamPrivate *priv = pStream->pRmPriv;
    NvRmCmdBuf *cmdbuf = &priv->CmdBuf;
    NvRmCommandBuffer *buf = &cmdbuf->GatherTable[0];

    if (priv->DisasmLoaded == NV_FALSE)
    {
        priv->DisasmLoaded = NvRmDisasmLibraryLoad();
        if (priv->DisasmLoaded == NV_FALSE)
            goto fail;
    }

    if (priv->DisasmBuffer == NULL)
    {
        #define NVRM_DISASM_BUFFER_SIZE    2048
        priv->DisasmBufferSize = NVRM_DISASM_BUFFER_SIZE;
        priv->DisasmBuffer = NvOsAlloc(priv->DisasmBufferSize);
        if (priv->DisasmBuffer == NULL)
            goto fail;
    }

    offset = NvOsSnprintf(priv->DisasmBuffer, priv->DisasmBufferSize,
                          "NVRM STREAM DISASM:\n");

    if (offset < 0 || (NvU32) offset >= priv->DisasmBufferSize)
        goto fail;

    context = NvRmDisasmCreate();
    if (context == NULL)
        goto fail;

    NvRmDisasmInitialize(context);

    while (buf != cmdbuf->pCurrentGather)
    {
        NvU32 i;

        for (i = 0; i < buf->Words; i++)
        {
            NvS32 length;
            NvU32 data;

            data = NvRmMemRd32(buf->hMem, buf->Offset + i * 4);
            length = NvRmDisasmPrint(context,
                                     data,
                                     str,
                                     NV_ARRAY_SIZE(str));

            if (length < 0)
                return;

            if (priv->DisasmBufferSize < (NvU32) (offset + length + 2))
            {
                size_t newSize = priv->DisasmBufferSize * 2 + length + 2;
                char *newBuffer = NvOsAlloc(newSize);

                if (newBuffer == NULL)
                    return;

                NvOsMemcpy(newBuffer, priv->DisasmBuffer, offset);
                NvOsFree(priv->DisasmBuffer);
                priv->DisasmBufferSize = newSize;
                priv->DisasmBuffer = newBuffer;
            }

            NvOsMemcpy(priv->DisasmBuffer + offset, str, length);
            offset += length;
            priv->DisasmBuffer[offset++] = '\n';
            priv->DisasmBuffer[offset] = 0;
        }

        buf++;
    }

    NvOsDebugString(priv->DisasmBuffer);

fail:
    if (context != NULL)
        NvRmDisasmDestroy(context);
}

static NvBool NvRmPrivCheckSpace(NvRmStream *pStream,
    NvU32 Words, NvU32 Gathers, NvU32 Relocs, NvU32 Waits)
{
    NvRmStreamPrivate *priv = pStream->pRmPriv;
    NvRmCmdBuf *cmdbuf = &priv->CmdBuf;
    NvU32 size;

    NV_ASSERT(pStream);
    NV_ASSERT(cmdbuf);

    NV_ASSERT(cmdbuf->fence >= cmdbuf->current);

    /* Check for space in the cmdbuf gather, reloc and wait tables
     * Note that N gathers in the stream table can expand to 2 * N + 1
     * in the cmdbuf
     * Also need to save one for the syncpt increment
     */
    if (cmdbuf->pCurrentGather + Gathers*2 + 1 >
        &cmdbuf->GatherTable[NVRM_CMDBUF_GATHER_SIZE - 1])
        return NV_FALSE;

    if (cmdbuf->pCurrentReloc + Relocs >
        &cmdbuf->RelocTable[NVRM_CMDBUF_RELOC_SIZE])
        return NV_FALSE;

    if (cmdbuf->pCurrentWait + Waits >
        &cmdbuf->WaitTable[NVRM_CMDBUF_WAIT_SIZE])
        return NV_FALSE;

    size = (pStream->pCurrent - pStream->pBase) * sizeof(NvU32);
    size += NVRM_CMDBUF_BOOKKEEPING;
    size += Words * sizeof(NvU32);
    size += Relocs * sizeof(NvU32);
    if (cmdbuf->current + size > cmdbuf->fence)
        return NV_FALSE;

    if ((NvU32)(pStream->pCurrent - priv->pBeginGather) + Words > HCFGATHER_COUNT_FIELD)
        return NV_FALSE;

    return NV_TRUE;
}

static void NvRmPrivPushGather(NvRmStream *pStream, NvData32 *pCurrent,
    NvRmMemHandle hMem, NvU32 Offset, NvU32 Words)
{
    NvRmCmdBuf *cmdbuf = &pStream->pRmPriv->CmdBuf;
    NvRmCommandBuffer *pGather = cmdbuf->pCurrentGather;

    pGather->hMem = hMem;
    pGather->Offset = Offset;
    pGather->Words = Words;

    cmdbuf->pCurrentGather = pGather + 1;

    /* store the gather offset for future */
    pStream->pRmPriv->pBeginGather = pCurrent;
}

static void NvRmPrivPushPendingGather(NvRmStream *pStream,
    NvData32 *pCurrent)
{
    NvRmStreamPrivate *priv = pStream->pRmPriv;
    NvU32 Words = ((NvU32)pCurrent - (NvU32)priv->pBeginGather)/sizeof(NvData32);
    NvU32 Offset = ((NvU32)priv->pBeginGather - (NvU32)pStream->pBase);
    NvRmCmdBuf *cmdbuf = &pStream->pRmPriv->CmdBuf;

    /* Check if gather is pending */
    if (Words)
    {
        cmdbuf->current += Words;

        /* this means user doesn't push any explicit gather
         * after begining stream. so add that gather now */
        NvRmPrivPushGather(pStream, pCurrent, pStream->hMem,
            Offset, Words);
    }
}

static void NvRmPrivAppendSyncPointBaseIncr(NvRmCmdBuf *cmdbuf,
    NvRmStream *pStream, NvU32 WaitBaseID, NvU32 count)
{
    NvU32 ClassID;

    /* 3d submits use 3d class always */
    if (pStream->SyncPointID == NVRM_SYNCPOINT_3D)
        ClassID = NV_GRAPHICS_3D_CLASS_ID;
    else
        ClassID = pStream->LastClassUsed;

    NVRM_STREAM_PUSH_U(pStream, NVRM_CH_OPCODE_SET_CLASS(NV_HOST1X_CLASS_ID, 0, 0));
    NVRM_STREAM_PUSH_U(pStream, NVRM_CH_OPCODE_NONINCR(NV_CLASS_HOST_INCR_SYNCPT_BASE_0, 1));
    NVRM_STREAM_PUSH_U(pStream,
        NV_DRF_NUM(NV_CLASS_HOST, INCR_SYNCPT_BASE, BASE_INDX, WaitBaseID) |
        NV_DRF_NUM(NV_CLASS_HOST, INCR_SYNCPT_BASE, OFFSET, count));
    NVRM_STREAM_PUSH_U(pStream, NVRM_CH_OPCODE_SET_CLASS(ClassID, 0, 0));
}

static void NvRmPrivAppendIncr(NvRmStream *pStream)
{
    NvRmCmdBuf *cmdbuf = &pStream->pRmPriv->CmdBuf;
    NvData32 *OriginalCurrent = pStream->pCurrent;
    NvU32 Value = 0;
    NvU32 WaitBaseID;

    cmdbuf->SyncPointIncrs[pStream->SyncPointID].Value++;

    /* INCR_SYNCPT format is:
     *
     * 15:8 - condition
     *   OP_DONE is 1: module has reached bottom of pipe
     *   IMMEDIATE is 0: command buffer is at the top of the pipe
     * 7:0 - sync point id
     */
    if (pStream->UseImmediate)
    {
        Value = NV_DRF_DEF(NVRM, INCR_SYNCPT, COND, IMMEDIATE) |
                NV_DRF_NUM(NVRM, INCR_SYNCPT, INDX, (pStream->SyncPointID & 0xff) );
    }
    else
    {
        Value = NV_DRF_DEF(NVRM, INCR_SYNCPT, COND, OP_DONE) |
                NV_DRF_NUM(NVRM, INCR_SYNCPT, INDX, (pStream->SyncPointID & 0xff) );
    }

    /* Push sync-point increment */
    NVRM_STREAM_PUSH_U((NvData32 *)pStream, NVRM_CH_OPCODE_NONINCR(NVRM_INCR_SYNCPT_0, 1) );
    NVRM_STREAM_PUSH_U((NvData32 *)pStream, Value);

    WaitBaseID = cmdbuf->SyncPointIncrs[pStream->SyncPointID].WaitBaseID;

    /* Hack to keep compatibility with old software */
    if (WaitBaseID == NVRM_INVALID_WAITBASE_ID &&
        pStream->SyncPointID == NVRM_SYNCPOINT_3D)
        WaitBaseID = pStream->WaitBaseID;

    /* Increment waitbase if it is used */
    if (WaitBaseID != NVRM_INVALID_WAITBASE_ID)
        NvRmPrivAppendSyncPointBaseIncr(cmdbuf, pStream, WaitBaseID, 1);

    cmdbuf->current += ((NvU32)pStream->pCurrent - (NvU32)OriginalCurrent)/sizeof(NvData32);

    NV_ASSERT(cmdbuf->current <= cmdbuf->fence);

    /* Push the sync-point increment as a separate gather */
    NvRmPrivPushPendingGather(pStream, pStream->pCurrent);
}

/*
 * NvRmStreamTouchSyncPoint(cmdbuf, SyncPointID)
 *
 * Mark some syncpoint as used. This function maintains the linked list
 * of used syncpoints. This list is generated to allow fast traversal of
 * the used syncpoints.
 */

static void NvRmStreamTouchSyncPoint(NvRmCmdBuf *cmdbuf, NvS32 SyncPointID)
{
    NvRmSyncPointDescriptor *sp = cmdbuf->SyncPointIncrs + SyncPointID;
    NvRmSyncPointDescriptor *last = cmdbuf->SyncPointIncrs + cmdbuf->SyncPointLast;

    if (cmdbuf->SyncPointFirst == -1) {

        /* First incremented syncpoint */
        cmdbuf->SyncPointFirst = SyncPointID;
        cmdbuf->SyncPointLast = SyncPointID;

    } else if ((sp->Prev == -1) && (sp->Next == -1) && (sp != last)) {

        /* Add syncpoint to the list */
        last->Next = SyncPointID;
        sp->Prev = cmdbuf->SyncPointLast;
        cmdbuf->SyncPointLast = SyncPointID;
    }

}

static void NvRmPrivFlush(NvRmStream *pStream)
{
    NvRmStreamPrivate *priv = pStream->pRmPriv;
    NvRmCmdBuf *cmdbuf = &pStream->pRmPriv->CmdBuf;
    NvError streamError = pStream->ErrorFlag;
    NvU32 i;

    /* A squeezed array of syncpoint increments */
    NvRmSyncPointDescriptor SyncPointIncrs[NVRM_MAX_SYNCPOINTS_PER_SUBMIT];

    /* Local data for stream syncpoint */
    NvRmSyncPointDescriptor *StreamSyncPoint =
        &cmdbuf->SyncPointIncrs[pStream->SyncPointID];
    NvU32 StreamSyncPointIncrs = 0, StreamSyncPointFence = 0;

    /* For syncpoint table traversal */
    NvS32 SyncPointNode, SqueezedSyncPointNode, StreamSyncPointIdx;

    /* Check that we are not doing anything really stupid */
    NV_ASSERT(pStream->hMem);
    NV_ASSERT(cmdbuf->fence >= cmdbuf->current);
    NV_ASSERT(cmdbuf->sem);
    NV_ASSERT(cmdbuf->pCurrentGather >= &cmdbuf->GatherTable[0]);
    NV_ASSERT(cmdbuf->pCurrentGather <=
            &cmdbuf->GatherTable[NVRM_CMDBUF_GATHER_SIZE]);
    NV_ASSERT(cmdbuf->pCurrentReloc >= &cmdbuf->RelocTable[0]);
    NV_ASSERT(cmdbuf->pCurrentReloc <=
            &cmdbuf->RelocTable[NVRM_CMDBUF_RELOC_SIZE]);
    NV_ASSERT(cmdbuf->pCurrentWait >= &cmdbuf->WaitTable[0]);
    NV_ASSERT(cmdbuf->pCurrentWait <=
            &cmdbuf->WaitTable[NVRM_CMDBUF_WAIT_SIZE]);
    NV_ASSERT(pStream->SyncPointID != NVRM_INVALID_SYNCPOINT_ID);

    /* If there's nothing to submit, don't submit */
    if ((pStream->pCurrent == pStream->pRmPriv->pBeginGather) &&
        (cmdbuf->current == cmdbuf->last) &&
        (cmdbuf->pCurrentGather == &cmdbuf->GatherTable[0]) &&
        (cmdbuf->pCurrentReloc == &cmdbuf->RelocTable[0]) &&
        (cmdbuf->pCurrentWait == &cmdbuf->WaitTable[0]))
        return;

    /* Include increments made outside the stream library */
    StreamSyncPoint->Value += pStream->SyncPointsUsed;
    NvRmStreamTouchSyncPoint(cmdbuf, StreamSyncPoint->SyncPointID);

    /* If necessary, stick an increment syncpt-base on the end of the
     * command buffer */
    SyncPointNode = cmdbuf->SyncPointFirst;
    while (SyncPointNode != -1) {
        NvRmSyncPointDescriptor *sp = cmdbuf->SyncPointIncrs + SyncPointNode;
        NvU32 WaitBaseID = sp->WaitBaseID;

        /* 3d is an unfortunate exception. :-( */
        if (WaitBaseID == NVRM_INVALID_WAITBASE_ID &&
            pStream->SyncPointID == NVRM_SYNCPOINT_3D &&
            pStream->SyncPointID == (NvU32)SyncPointNode)
            WaitBaseID = pStream->WaitBaseID;

        /* If waitbase is set, add increments */
        if (WaitBaseID != NVRM_INVALID_WAITBASE_ID)
            NvRmPrivAppendSyncPointBaseIncr(cmdbuf, pStream, WaitBaseID,
                    sp->Value);

        SyncPointNode = sp->Next;
    }

    /* Push the pending gather, if any */
    NvRmPrivPushPendingGather(pStream, pStream->pCurrent);

    if (pStream->pPreFlushCallback)
        pStream->pPreFlushCallback(pStream);

    /* add a sync point increment for pushbuffer tracking.
     * this must not append if the client sets the disable bit.
     */
    if (!pStream->ClientManaged)
        NvRmPrivAppendIncr(pStream);

    priv->CtxChanged = 0;

    /* Go through all used syncpoints. */
    SyncPointNode = cmdbuf->SyncPointFirst;
    SqueezedSyncPointNode = 0;
    StreamSyncPointIdx = -1;
    StreamSyncPointIncrs = StreamSyncPoint->Value;
    while (SyncPointNode != -1) {
        NvRmSyncPointDescriptor *sp = cmdbuf->SyncPointIncrs + SyncPointNode;

        NV_ASSERT(SqueezedSyncPointNode < NVRM_MAX_SYNCPOINTS_PER_SUBMIT);

        /* Squeeze array */
        SyncPointIncrs[SqueezedSyncPointNode] = *sp;

        /* Capture stream syncpoint in the squeezed array */
        if (SyncPointNode == (NvS32)pStream->SyncPointID)
            StreamSyncPointIdx = SqueezedSyncPointNode;

        /* Initialize fence array */
        cmdbuf->SyncPointFences[SqueezedSyncPointNode].SyncPointID =
            SyncPointNode;

        /* Clean syncpoint */
        SyncPointNode = sp->Next;
        sp->Value = 0;
        sp->Prev = -1;
        sp->Next = -1;

        SqueezedSyncPointNode++;
    }

    /* Teardown the linked list */
    cmdbuf->NumSyncPointFences = SqueezedSyncPointNode;
    cmdbuf->SyncPointFirst = -1;
    cmdbuf->SyncPointLast = -1;

    NV_ASSERT(StreamSyncPointIdx != -1);

    /* After failed submission new submissions can lead to undefined results */
    if (streamError == NvSuccess)
    {
        NvU32 SyncPointValues[NVRM_MAX_SYNCPOINTS_PER_SUBMIT];

        /* Write back command buffer data */
        if (pStream->pMem)
            NvRmMemCacheMaint(pStream->hMem, (NvU8 *)pStream->pMem + cmdbuf->last,
                    cmdbuf->current - cmdbuf->last, NV_TRUE, NV_TRUE);

        if (pStream->Flags & NvRmStreamFlag_Disasm)
            NvRmPrivDisasm(pStream);

        /* finally submit data to the channel.
         * pinning/unpinning will be done by the channel submit logic.
         *
         * the number of buffers may be zero if the client needs to wait for
         * previous submission.
         *
         * If submission fails, we set error flag in stream.
         */
        streamError = NvRmChannelSubmit(cmdbuf->hChannel,
                cmdbuf->GatherTable,
                cmdbuf->pCurrentGather - &cmdbuf->GatherTable[0],
                cmdbuf->RelocTable,
                cmdbuf->RelocShiftTable,
                cmdbuf->pCurrentReloc - &cmdbuf->RelocTable[0],
                cmdbuf->WaitTable,
                cmdbuf->pCurrentWait - &cmdbuf->WaitTable[0],
                (pStream->NullKickoff) ? 0 : pStream->hContext,
                pStream->pContextShadowCopy, pStream->ContextShadowCopySize,
                pStream->NullKickoff, pStream->LastEngineUsed,
                StreamSyncPointIdx, cmdbuf->SyncPointsWaited,
                &priv->CtxChanged, SyncPointValues, SyncPointIncrs,
                cmdbuf->NumSyncPointFences);
        for (i = 0; i < cmdbuf->NumSyncPointFences; ++i)
            cmdbuf->SyncPointFences[i].Value = SyncPointValues[i];
        StreamSyncPointFence = SyncPointValues[StreamSyncPointIdx];
    }

    if (streamError == NvSuccess)
    {
        /* Inform our client what range of sync points they actually got */
        if (pStream->pSyncPointBaseCallback)
        {
            pStream->pSyncPointBaseCallback(pStream,
                    StreamSyncPointFence - StreamSyncPointIncrs,
                    StreamSyncPointIncrs);
        }
    }
    else
    {
        NvOsDebugPrintf("NvRmPrivFlush: NvRmChannelSubmit failed "
                "(err = %d, SyncPointValue = %d)\n", streamError,
                StreamSyncPointFence);

        NvRmStreamSetError(pStream, streamError);
        if (pStream->pSyncPointBaseCallback)
        {
            pStream->pSyncPointBaseCallback(pStream,
                    StreamSyncPointFence, 0);
        }
    }

    pStream->SyncPointsUsed = 0;
    cmdbuf->SyncPointsWaited = 0;

    /* Keep track of the last time we submitted to the hardware */
    cmdbuf->last = cmdbuf->current;

    /* Once we've called ChannelSubmit,
     * we can reset the cmdbuf gather and reloc tables
     */
    cmdbuf->pCurrentWait = &cmdbuf->WaitTable[0];
    cmdbuf->pCurrentGather = &cmdbuf->GatherTable[0];
    cmdbuf->pCurrentReloc = &cmdbuf->RelocTable[0];
    cmdbuf->pCurrentRelocShift = &cmdbuf->RelocShiftTable[0];
}

NvData32 *NvRmStreamBegin(NvRmStream *pStream,
    NvU32 Words, NvU32 Waits, NvU32 Relocs, NvU32 Gathers)
{
#if NV_DEBUG
    NvRmStreamPrivate *priv = pStream->pRmPriv;
#endif
    NvRmCmdBuf *cmdbuf = &pStream->pRmPriv->CmdBuf;

    NV_ASSERT(pStream);
    NV_ASSERT(Words + Gathers > 0);
    NV_ASSERT(Words < HCFGATHER_COUNT_FIELD);
    NV_ASSERT(Words + Relocs <= NVRM_STREAM_BEGIN_MAX_WORDS);
    NV_ASSERT(Relocs <= NVRM_STREAM_RELOCATION_TABLE_SIZE);
    NV_ASSERT(Gathers <= NVRM_STREAM_GATHER_TABLE_SIZE);
    NV_ASSERT(Waits <= NVRM_STREAM_WAIT_TABLE_SIZE);

    /* Do we have space */
    if (!NvRmPrivCheckSpace(pStream, Words, Gathers, Relocs, Waits))
    {
        /* No
         * First, submit the stuff already in the cmdbuf to the hardware
         */
        NvRmPrivFlush(pStream);
        if (!NvRmPrivCheckSpace(pStream, Words, Gathers, Relocs, Waits))
        {
            NvU32 i;

            /* We still don't have space.
             * This means we've run out of this half of the ping-pong
             * buffer and need to flip over to the other side
             */
            for (i = 0; i < cmdbuf->PongNumSyncPointFences; ++i)
            {
                NvRmChannelSyncPointWait(cmdbuf->hDevice,
                    cmdbuf->PongSyncPointFences[i].SyncPointID,
                    cmdbuf->PongSyncPointFences[i].Value,
                    cmdbuf->sem);
            }
            /* check if there is a pending gather before ping-pong
             * if yes, add it to gather table */
            NvRmPrivPushPendingGather(pStream, pStream->pCurrent);

            NvRmPrivSetPong(pStream, cmdbuf, !cmdbuf->pong);
            cmdbuf->last = cmdbuf->current;
            NvOsMemcpy(cmdbuf->PongSyncPointFences, cmdbuf->SyncPointFences,
                cmdbuf->NumSyncPointFences * sizeof(*cmdbuf->PongSyncPointFences));
            cmdbuf->PongNumSyncPointFences = cmdbuf->NumSyncPointFences;
        }
    }

    NV_ASSERT(priv->pBeginGather <= pStream->pCurrent);

    return (NvData32 *)pStream;
}

void NvRmStreamEnd(NvRmStream *pStream, NvData32 *pCurrent)
{
}

void NvRmStreamSetWaitBase(NvRmStream *pStream, NvU32 SyncPointID,
    NvU32 WaitBaseID)
{
    NvRmCmdBuf *cmdbuf = &pStream->pRmPriv->CmdBuf;

    NV_ASSERT(SyncPointID < cmdbuf->NumHwSyncPoints);
    cmdbuf->SyncPointIncrs[SyncPointID].WaitBaseID = WaitBaseID;
}

NvData32 *NvRmStreamPushSetClass(NvRmStream *pStream, NvData32 *pCurrent,
    NvRmModuleID ModuleID, NvU32 ClassID)
{
    NVRM_STREAM_PUSH_U(pCurrent, NVRM_CH_OPCODE_SET_CLASS(ClassID, 0, 0));
    pStream->LastEngineUsed = ModuleID;
    pStream->LastClassUsed = ClassID;
    return pCurrent;
}

NvData32 *NvRmStreamPushIncr(NvRmStream *pStream, NvData32 *pCurrent,
    NvU32 SyncPointID, NvU32 Reg, NvU32 Cond, NvBool StreamSyncPoint)
{
    NvRmCmdBuf *cmdbuf = &pStream->pRmPriv->CmdBuf;

    NV_ASSERT(pStream->LastEngineUsed != NvRmModuleID_GraphicsHost);
    NV_ASSERT(pStream->LastClassUsed != NV_HOST1X_CLASS_ID);

    NVRM_STREAM_PUSH_U(pCurrent,
        NVRM_CH_OPCODE_NONINCR(Reg, 1));
    NVRM_STREAM_PUSH_U(pCurrent,
        NV_DRF_NUM(NV_CLASS_HOST, INCR_SYNCPT, COND, Cond) |
        NV_DRF_NUM(NV_CLASS_HOST, INCR_SYNCPT, INDX, SyncPointID));

    if (StreamSyncPoint) {
        NvRmStreamTouchSyncPoint(cmdbuf, SyncPointID);
        cmdbuf->SyncPointIncrs[SyncPointID].Value++;
    }

    return pCurrent;
}

NvData32 *NvRmStreamPushWait(NvRmStream *pStream, NvData32 *pCurrent,
    NvRmFence Fence)
{
    NvRmModuleID moduleid = pStream->LastEngineUsed;
    NvU32 classid = pStream->LastClassUsed;

    pCurrent = NvRmStreamPushSetClass(pStream, pCurrent,
                    NvRmModuleID_GraphicsHost, NV_HOST1X_CLASS_ID);
    NVRM_STREAM_PUSH_U(pCurrent,
        NVRM_CH_OPCODE_NONINCR(NV_CLASS_HOST_WAIT_SYNCPT_0, 1));
    NVRM_STREAM_PUSH_U(pCurrent,
        NV_DRF_NUM(NV_CLASS_HOST, WAIT_SYNCPT, INDX, Fence.SyncPointID) |
        NV_DRF_NUM(NV_CLASS_HOST, WAIT_SYNCPT, THRESH, Fence.Value));
    pCurrent = NvRmStreamPushWaitCheck(pStream, pCurrent, Fence);

    if (classid && moduleid)
        pCurrent = NvRmStreamPushSetClass(pStream, pCurrent, moduleid, classid);

    return pCurrent;
}

NvData32 *NvRmStreamPushWaits(NvRmStream *pStream, NvData32 *pCurrent,
    int NumFences, NvRmFence *Fences)
{
    NvRmModuleID moduleid = pStream->LastEngineUsed;
    NvU32 classid = pStream->LastClassUsed;
    int i;

    pCurrent = NvRmStreamPushSetClass(pStream, pCurrent,
                    NvRmModuleID_GraphicsHost, NV_HOST1X_CLASS_ID);
    for (i = 0; i < NumFences; i++) {
        NvRmFence *fence = &Fences[i];
        NVRM_STREAM_PUSH_U(pCurrent,
            NVRM_CH_OPCODE_NONINCR(NV_CLASS_HOST_WAIT_SYNCPT_0, 1));
        NVRM_STREAM_PUSH_U(pCurrent,
            NV_DRF_NUM(NV_CLASS_HOST, WAIT_SYNCPT, INDX, fence->SyncPointID) |
            NV_DRF_NUM(NV_CLASS_HOST, WAIT_SYNCPT, THRESH, fence->Value));
        pCurrent = NvRmStreamPushWaitCheck(pStream, pCurrent, *fence);
    }
    if (classid && moduleid)
        pCurrent = NvRmStreamPushSetClass(pStream, pCurrent, moduleid, classid);

    return pCurrent;
}

NvData32 *NvRmStreamPushWaitLast(NvRmStream *pStream, NvData32 *pCurrent,
    NvU32 SyncPointID, NvU32 WaitBase, NvU32 Reg, NvU32 Cond)
{
    NvRmModuleID moduleid = pStream->LastEngineUsed;
    NvU32 classid = pStream->LastClassUsed;
    NvRmCmdBuf *cmdbuf = &pStream->pRmPriv->CmdBuf;
    NvU32 incrs;

    NV_ASSERT(pStream->LastEngineUsed != 0);
    NV_ASSERT(pStream->LastEngineUsed == NvRmModuleID_3D);

    pCurrent = NvRmStreamPushIncr(pStream, pCurrent, SyncPointID, Reg, Cond, 1);

    /* combine increments made inside and outside the library */
    incrs = cmdbuf->SyncPointIncrs[SyncPointID].Value + pStream->SyncPointsUsed;

    pCurrent = NvRmStreamPushSetClass(pStream, pCurrent,
        NvRmModuleID_GraphicsHost, NV_HOST1X_CLASS_ID);
    NVRM_STREAM_PUSH_U(pCurrent,
        NVRM_CH_OPCODE_NONINCR(NV_CLASS_HOST_WAIT_SYNCPT_BASE_0, 1));
    NVRM_STREAM_PUSH_U(pCurrent,
        NV_DRF_NUM(NV_CLASS_HOST, WAIT_SYNCPT_BASE, INDX, SyncPointID) |
        NV_DRF_NUM(NV_CLASS_HOST, WAIT_SYNCPT_BASE, BASE_INDX, WaitBase) |
        NV_DRF_NUM(NV_CLASS_HOST, WAIT_SYNCPT_BASE, OFFSET, incrs));
    if (classid && moduleid)
        pCurrent = NvRmStreamPushSetClass(pStream, pCurrent, moduleid, classid);

    return pCurrent;
}

NvData32 *NvRmStreamPushReloc(NvRmStream *pStream, NvData32 *pCurrent,
    NvRmMemHandle hMem, NvU32 Offset, NvU32 RelocShift)
{
    NvU32 offset;
    NvRmCmdBuf *cmdbuf = &pStream->pRmPriv->CmdBuf;
    NvRmChannelSubmitReloc *pReloc = cmdbuf->pCurrentReloc;
    NvU32 *rShift = cmdbuf->pCurrentRelocShift;

    offset = (NvU32)pStream->pCurrent - (NvU32)pStream->pBase;
    pReloc->hCmdBufMem = pStream->hMem;
    pReloc->CmdBufOffset = offset;
    pReloc->hMemTarget = hMem;
    pReloc->TargetOffset = Offset;
    *rShift = RelocShift;
    cmdbuf->pCurrentReloc = pReloc + 1;
    cmdbuf->pCurrentRelocShift = rShift + 1;
    if (pStream->pMem)
        pStream->pCurrent->u = 0xDEADBEEF;
    pStream->pCurrent++;

    return pCurrent;
}

NvData32 *NvRmStreamPushFdReloc(NvRmStream *pStream, NvData32 *pCurrent,
    int fd, NvU32 Offset, NvU32 RelocShift)
{
    return NvRmStreamPushReloc(pStream, pCurrent,
        (NvRmMemHandle)(fd*4+1), Offset, RelocShift);
}

NvData32 *NvRmStreamPushGather(NvRmStream *pStream, NvData32 *pCurrent,
    NvRmMemHandle hMem, NvU32 Offset, NvU32 Words)
{
    /* Push the pending gather, if any */
    NvRmPrivPushPendingGather(pStream, pStream->pCurrent);

    /* handle the current gather */
    NvRmPrivPushGather(pStream, pStream->pCurrent, hMem,
        Offset, Words);

    return pCurrent;
}

NvData32 *NvRmStreamPushFdGather(NvRmStream *pStream, NvData32 *pCurrent,
    int fd, NvU32 Offset, NvU32 Words)
{
    return NvRmStreamPushGather(pStream, pCurrent,
        (NvRmMemHandle)(fd*4+1), Offset, Words);
}

NvData32 *NvRmStreamPushGatherNonIncr(NvRmStream *pStream, NvData32 *pCurrent,
    NvU32 Reg, NvRmMemHandle hMem, NvU32 Offset, NvU32 Words)
{
    /* Push the pending gather, if any */
    NvRmPrivPushPendingGather(pStream, pStream->pCurrent);

    /* handle the current gather */
    NvRmPrivPushGather(pStream, pStream->pCurrent, hMem,
        Offset, Reg << 16 | HCFGATHER_TYPE_NONINCR << 28 | Words);

    return pCurrent;
}

NvData32 *NvRmStreamPushGatherIncr(NvRmStream *pStream, NvData32 *pCurrent,
    NvU32 Reg, NvRmMemHandle hMem, NvU32 Offset, NvU32 Words)
{
    /* Push the pending gather, if any */
    NvRmPrivPushPendingGather(pStream, pStream->pCurrent);

    /* handle the current gather */
    NvRmPrivPushGather(pStream, pStream->pCurrent, hMem,
        Offset, Reg << 16 | HCFGATHER_TYPE_INCR << 28 | Words);

    return pCurrent;
}

NvData32 *NvRmStreamPushWaitCheck(NvRmStream *pStream, NvData32 *pCurrent,
    NvRmFence Fence)
{
    NvU32 offset;
    NvRmCmdBuf *cmdbuf = &pStream->pRmPriv->CmdBuf;
    NvRmChannelWaitChecks *pWait = cmdbuf->pCurrentWait;
    NvData32 *pWaitCheck;

    /* wait check is 1 behind the current */
    pWaitCheck = pStream->pCurrent - 1;
    offset = (NvU32)pWaitCheck - (NvU32)pStream->pBase;
    pWait->hCmdBufMem = pStream->hMem;
    pWait->CmdBufOffset = offset;
    pWait->SyncPointID = Fence.SyncPointID;
    pWait->Threshold = Fence.Value;

    cmdbuf->pCurrentWait = pWait + 1;
    cmdbuf->SyncPointsWaited |= (1 << (Fence.SyncPointID));

    return pCurrent;
}

void NvRmStreamFlush(NvRmStream *pStream, NvRmFence *pFence)
{
    NvRmCmdBuf *cmdbuf = &pStream->pRmPriv->CmdBuf;

    NV_ASSERT(pStream->LastEngineUsed != (NvRmModuleID)0);

    NvRmPrivFlush(pStream);

    if (pFence)
    {
        NvU32 i;
        for (i = 0; i < cmdbuf->NumSyncPointFences; ++i) {
            pFence[i].SyncPointID = cmdbuf->SyncPointFences[i].SyncPointID;
            pFence[i].Value = cmdbuf->SyncPointFences[i].Value;
        }
    }
}

NvError NvRmStreamRead3DRegister(NvRmStream *pStream, NvU32 Offset, NvU32 *Value)
{
    NvRmCmdBuf *cmdbuf = &pStream->pRmPriv->CmdBuf;

    return NvRmChannelRead3DRegister(cmdbuf->hChannel, Offset, Value);
}

NvError NvRmStreamSetPriorityEx(NvRmStream *pStream, NvU32 Priority,
    NvU32 SyncPointIndex, NvU32 WaitBaseIndex,
    NvU32 *SyncPointID, NvU32 *WaitBase)
{
    NvError err;
    NvRmCmdBuf *cmdbuf = &pStream->pRmPriv->CmdBuf;

    err = NvRmChannelSetPriority(cmdbuf->hChannel, Priority,
        SyncPointIndex, WaitBaseIndex, SyncPointID, WaitBase);
    if (err == NvSuccess) {
        pStream->WaitBaseID =
            NvRmChannelGetModuleWaitBase(cmdbuf->hChannel, 0, 0);
    }

    return err;
}

NvError NvRmStreamSetPriority(NvRmStream *pStream, NvU32 Priority)
{
    return NvRmStreamSetPriorityEx(pStream, Priority, 0, 0, NULL, NULL);
}

NvError NvRmStreamSetContextSwitch(
    NvRmStream *pStream,
    NvRmMemHandle hSave,
    NvU32 SaveWords,
    NvU32 SaveOffset,
    NvRmMemHandle hRestore,
    NvU32 RestoreWords,
    NvU32 RestoreOffset,
    NvU32 RelocOffset,
    NvU32 SyncptId,
    NvU32 WaitBase,
    NvU32 SaveIncrs,
    NvU32 RestoreIncrs)
{
    NvError err;
    NvRmCmdBuf *cmdbuf = &pStream->pRmPriv->CmdBuf;

    err = NvRmChannelSetContextSwitch(cmdbuf->hChannel,
        hSave, SaveWords, SaveOffset,
        hRestore, RestoreWords, RestoreOffset, RelocOffset,
        SyncptId, WaitBase, SaveIncrs, RestoreIncrs);

    return err;
}
