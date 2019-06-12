/*
 * Copyright (c) 2007-2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NV_ENABLE_DEBUG_PRINTS
#define NV_ENABLE_DEBUG_PRINTS (0)
#endif

#include "nvassert.h"
#include "nvmmlite.h"
#include "nvmmlite_event.h"
#include "nvutil.h"
#include "nvmmlite_util.h"

// hardcode a list to prevent anyone adding their own library..
static const char *g_sLibraryNames[] =
    { "libnvmmlite_audio",
      "libnvmmlite_video",
      "libnvmmlite_image",
      "libnvmmlite_msaudio"
    };
#define NUM_LIBRARIES 4
#define MAX_BLOCKS_LIB 64

static NvMMLiteBlockType g_oBlockTypeMap[NUM_LIBRARIES][MAX_BLOCKS_LIB] =
    {{ NvMMLiteBlockType_UnKnown }};
static NvBool g_bBlockMapInit = NV_FALSE;

/*NvMMLiteTransport's  context */
typedef struct NvMMLiteContext
{
    NvMMLiteBlockHandle         hActualBlock;
    NvMMLiteBlockType           eType;
    NvMMLiteState               eShadowedBlockState;
    NvOsMutexHandle             BlockInfoLock;
    NvOsLibraryHandle           hLibraryHandle;
    NvMMLiteDoWorkFunction      pDoWork;
} NvMMLiteContext;

/* This assumes the transport open isn't ever called from multiple threads.
   OMX ensures this, so don't bother doing anything special here.
   Given that the results from a query should be static over time, and
   that this only overwrites, it shouldn't really matter either way. */
static void InitBlockMap(void)
{
    int i;

    if (g_bBlockMapInit)
        return;

    for (i = 0; i < NUM_LIBRARIES; i++)
    {
        NvOsLibraryHandle hLibHandle = NULL;

        if (NvSuccess == NvOsLibraryLoad(g_sLibraryNames[i], &hLibHandle))
        {
            NvMMLiteQueryBlocksFunction queryBlock = NULL;
            queryBlock = NvOsLibraryGetSymbol(hLibHandle,
                                              "NvMMLiteQueryBlocks");
            if (queryBlock)
            {
                int j;
                for (j = 0; j < MAX_BLOCKS_LIB; j++)
                {
                    g_oBlockTypeMap[i][j] = queryBlock(j);
                    if (g_oBlockTypeMap[i][j] == NvMMLiteBlockType_UnKnown)
                        break;
                }
            }

            NvOsLibraryUnload(hLibHandle);
        }
    }

    g_bBlockMapInit = NV_TRUE;
}

static NvError NvMMLiteBlockCreate(NvMMLiteContext *pContext,
                                   NvMMLiteCreationParameters *pParams)
{
    NvError status = NvSuccess;
    NvMMLiteInternalCreationParameters internalParams;
    NvMMLiteOpenBlockFunction openblock = NULL;
    int i, j;
    const char *pBlockPath = NULL;

    pContext->eType = pParams->Type;

    for (i = 0; i < NUM_LIBRARIES; i++)
    {
        for (j = 0; j < MAX_BLOCKS_LIB; j++)
        {
            if (g_oBlockTypeMap[i][j] == NvMMLiteBlockType_UnKnown)
                break;
            if (g_oBlockTypeMap[i][j] == pParams->Type)
            {
                pBlockPath = g_sLibraryNames[i];
                break;
            }
        }
    }

#if NVMMLITE_BUILT_DYNAMIC
    if (!pBlockPath)
    {
        NvOsDebugPrintf("Unable to find block: %x\n", pParams->Type);
        return NvError_BadParameter;
    }

    status = NvOsLibraryLoad(pBlockPath, &(pContext->hLibraryHandle));
    if (status != NvSuccess)
    {
        NvOsDebugPrintf("Unable to load nvmm library: %s\n", pBlockPath);
        pContext->hLibraryHandle = NULL;
        return status;
    }

    openblock = NvOsLibraryGetSymbol(pContext->hLibraryHandle,
                                     "NvMMLiteOpen");
    if (!openblock)
        return NvError_BadParameter;
#else
    openblock = NvMMLiteOpen;
#endif

    NvOsMemset(&internalParams, 0, sizeof(internalParams));
    internalParams.BlockSpecific = pParams->BlockSpecific;

    status = openblock(pContext->eType, &pContext->hActualBlock,
                       &internalParams, &pContext->pDoWork);
    NvOsDebugPrintf("%s : Block : BlockType = %d \n", __func__, pContext->eType);
    return status;
}

/* Wrapper functions */
static NvError SetTransferBufferFunc(NvMMLiteBlockHandle hBlock,
                                     NvU32 StreamIndex,
                                     LiteTransferBufferFunction TransferBuffer,
                                     void *pContextForTranferBuffer,
                                     NvU32 StreamIndexForTransferBuffer)
{
    NvError status = NvSuccess;
    NvMMLiteContext *pContext = ((NvMMLiteBlock *)hBlock)->pContext;
    NvMMLiteBlockHandle block = pContext->hActualBlock;

    NvOsMutexLock(pContext->BlockInfoLock);

    status = block->SetTransferBufferFunction(block,
                                              StreamIndex, TransferBuffer,
                                              pContextForTranferBuffer,
                                              StreamIndexForTransferBuffer);

    NvOsMutexUnlock(pContext->BlockInfoLock);
    return status;
}

static void SetSendBlockEventFunc(NvMMLiteBlockHandle hBlock,
                                  void *ClientContext,
                                  LiteSendBlockEventFunction SendBlockEvent)
{
    NvMMLiteContext *pContext = ((NvMMLiteBlock *)hBlock)->pContext;
    NvMMLiteBlockHandle block = pContext->hActualBlock;

    NvOsMutexLock(pContext->BlockInfoLock);

    block->SetSendBlockEventFunction(block, ClientContext, SendBlockEvent);

    NvOsMutexUnlock(pContext->BlockInfoLock);
}

static NvError GetAttribute(NvMMLiteBlockHandle hBlock,
                            NvU32 AttributeType, NvU32 AttributeSize,
                            void *pAttribute)
{
    NvError status = NvSuccess;
    NvMMLiteContext *pContext = ((NvMMLiteBlock *)hBlock)->pContext;
    NvMMLiteBlockHandle block = pContext->hActualBlock;

    NvOsMutexLock(pContext->BlockInfoLock);

    status = block->GetAttribute(block, AttributeType, AttributeSize,
                                 pAttribute);

    NvOsMutexUnlock(pContext->BlockInfoLock);
    return status;
}

static NvError AbortBuffers(NvMMLiteBlockHandle hBlock, NvU32 StreamIndex)
{
    NvError status = NvSuccess;
    NvMMLiteContext*pContext = ((NvMMLiteBlock *)hBlock)->pContext;
    NvMMLiteBlockHandle block = pContext->hActualBlock;

    NvOsMutexLock(pContext->BlockInfoLock);

    status = block->AbortBuffers(block, StreamIndex);

    NvOsMutexUnlock(pContext->BlockInfoLock);
    return status;
}

static NvError SetAttribute(NvMMLiteBlockHandle hBlock,
                            NvU32 AttributeType, NvU32 SetAttrFlag,
                            NvU32 AttributeSize, void *pAttribute)
{
    NvError status = NvSuccess;
    NvMMLiteContext *pContext = ((NvMMLiteBlock *)hBlock)->pContext;
    NvMMLiteBlockHandle block = pContext->hActualBlock;

    NvOsMutexLock(pContext->BlockInfoLock);

    status = block->SetAttribute(block, AttributeType, SetAttrFlag,
                                  AttributeSize, pAttribute);

    NvOsMutexUnlock(pContext->BlockInfoLock);
    return status;
}

static NvError SetState(NvMMLiteBlockHandle hBlock, NvMMLiteState State)
{
    NvError status = NvSuccess;
    NvMMLiteContext *pContext = ((NvMMLiteBlock *)hBlock)->pContext;
    NvMMLiteBlockHandle block = pContext->hActualBlock;

    NvOsMutexLock(pContext->BlockInfoLock);

    pContext->eShadowedBlockState = State;
    status = block->SetState(block, State);

    NvOsMutexUnlock(pContext->BlockInfoLock);
    return status;
}

static NvError Extension(NvMMLiteBlockHandle hBlock, NvU32 ExtensionIndex,
                         NvU32 SizeInput, void *pInputData, NvU32 SizeOutput,
                         void *pOutputData)
{
    NvError status = NvSuccess;
    NvMMLiteContext *pContext = ((NvMMLiteBlock *)hBlock)->pContext;
    NvMMLiteBlockHandle block = pContext->hActualBlock;

    NvOsMutexLock(pContext->BlockInfoLock);

    status = block->Extension(block, ExtensionIndex, SizeInput, pInputData,
                              SizeOutput, pOutputData);

    NvOsMutexUnlock(pContext->BlockInfoLock);
    return status;
}

static NvError GetState(NvMMLiteBlockHandle hBlock, NvMMLiteState *pState)
{
    NvError status = NvSuccess;
    NvMMLiteContext *pContext = ((NvMMLiteBlock *)hBlock)->pContext;

    /* Use our shadowed block state, no need to ask the block */
    *pState = pContext->eShadowedBlockState;

    return status;
}

static NvError TransferBufferToBlock(void *Context, NvU32 StreamIndex,
                                     NvMMBufferType BufferType,
                                     NvU32 BufferSize, void *pBuffer)
{
    NvError status = NvSuccess;
    NvMMLiteContext *pContext = ((NvMMLiteBlock *)Context)->pContext;
    NvMMLiteBlockHandle block = pContext->hActualBlock;

    /* Does this need locked? */
    status = block->TransferBufferToBlock(block, StreamIndex, BufferType,
                                          BufferSize, pBuffer);
    return status;
}

static NvError DoWork(NvMMLiteBlockHandle hBlock, NvMMLiteDoWorkCondition Flag,
                      NvBool *pMoreWorkPending)
{
    NvError status = NvSuccess;
    NvMMLiteContext *pContext = ((NvMMLiteBlock *)hBlock)->pContext;
    NvMMLiteBlockHandle block = pContext->hActualBlock;

    NvOsMutexLock(pContext->BlockInfoLock);

    status = pContext->pDoWork(block, Flag, pMoreWorkPending);

    NvOsMutexUnlock(pContext->BlockInfoLock);
    return status;
}

/** Constructor for the block which is used by NvMMLiteOpenMMBlock. */
NvError NvMMLiteOpenBlock(NvMMLiteBlockHandle *phBlock,
                          NvMMLiteCreationParameters *pParams)
{
    NvError status = NvSuccess;
    NvMMLiteBlock *pBlock;
    NvMMLiteContext *pContext = NULL;

    InitBlockMap();

    if (!phBlock || !pParams)
        return NvError_BadParameter;

    pBlock = NvOsAlloc(sizeof (NvMMLiteBlock));
    if (!pBlock)
    {
        status = NvError_InsufficientMemory;
        goto cleanup;
    }
    NvOsMemset(pBlock, 0, sizeof(NvMMLiteBlock));

    pContext = NvOsAlloc(sizeof(NvMMLiteContext));
    if (!pContext)
    {
        status = NvError_InsufficientMemory;
        goto cleanup;
    }
    NvOsMemset(pContext, 0, sizeof(NvMMLiteContext));

    pContext->eShadowedBlockState = NvMMLiteState_Stopped;

    status = NvOsMutexCreate(&pContext->BlockInfoLock);
    if (status != NvSuccess)
        goto cleanup;

    status = NvMMLiteBlockCreate(pContext, pParams);
    if (status != NvSuccess)
        goto cleanup;

    pBlock->StructSize                = sizeof(NvMMLiteBlock);
    pBlock->pContext                  = pContext;
    pBlock->GetAttribute              = GetAttribute;
    pBlock->AbortBuffers              = AbortBuffers;
    pBlock->SetAttribute              = SetAttribute;
    pBlock->SetState                  = SetState;
    pBlock->Extension                 = Extension;
    pBlock->GetState                  = GetState;
    pBlock->SetSendBlockEventFunction = SetSendBlockEventFunc;
    pBlock->SetTransferBufferFunction = SetTransferBufferFunc;
    pBlock->TransferBufferToBlock     = TransferBufferToBlock;
    pBlock->pDoWork                   = DoWork;

    *phBlock = pBlock;
    return NvSuccess;

cleanup:
    NvMMLiteCloseBlock(pBlock);
    return status;
}

void NvMMLiteCloseBlock(NvMMLiteBlockHandle hBlock)
{
    NvMMLiteContext* pContext;

    if (!hBlock)
        return;

    pContext = hBlock->pContext;

    if (pContext)
    {
        if (pContext->hActualBlock &&
            pContext->hActualBlock->Close)
        {
            pContext->hActualBlock->Close(pContext->hActualBlock);
        }
        if (pContext->BlockInfoLock)
            NvOsMutexDestroy(pContext->BlockInfoLock);

        if (pContext->hLibraryHandle)
            NvOsLibraryUnload(pContext->hLibraryHandle);
    }

    NvOsFree(pContext);
    NvOsFree(hBlock);
}

