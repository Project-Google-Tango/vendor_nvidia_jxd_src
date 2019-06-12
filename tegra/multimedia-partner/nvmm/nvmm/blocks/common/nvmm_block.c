/*
* Copyright (c) 2007 - 2013 NVIDIA Corporation.  All rights reserved.
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

#include "nvmm.h"
#include "nvmm_event.h"
#include "nvrm_transport.h"
#include "nvmm_block.h"
#include "nvassert.h"
#include "nvmm_debug.h"
#include "nvmm_util.h"
#include "nvmm_logger.h"
#include "nvmm_common.h"

#if NV_IS_AVP
#pragma arm section code = "BLOCK_IRAM_PREF", rwdata = "BLOCK_IRAM_MAND", rodata = "BLOCK_IRAM_PREF", zidata ="BLOCK_IRAM_MAND"
#endif

#define NVLOG_CLIENT NVLOG_NVMM_TRANSPORT

static void    DestroyStream(NvMMStream* pStream);
static NvBool  NvMMBlockNegotiateBufferConfig(NvMMBlockHandle hBlock, NvMMStream* pStream);
static NvBool  NvMMBlockVerifyBufferConfig(NvMMBlockHandle hBlock, NvMMStream* pStream, NvMMNewBufferConfigurationInfo* pConfig);
static NvError NvMMBlockEventNewBufferRequirements(NvMMBlockHandle hBlock, NvMMStream* pStream, NvMMNewBufferRequirementsInfo* pEvent);
static NvError NvMMBlockEventNewBufferConfiguration(NvMMBlockHandle hBlock, NvMMStream* pStream, NvMMNewBufferConfigurationInfo* pEvent);
static NvError NvMMBlockEventReNegotiateBufferRequirements(NvMMBlockHandle hBlock, NvMMStream* pStream, NvMMNewBufferRequirementsInfo* pEvent);
static NvError NvMMSendBlockEvent( NvMMBlockContext* pContext, NvU32 EventType, NvU32 EventInfoSize,  void *pEventInfo);

NvError NvMMBlockOpen(NvMMBlockHandle *phBlock,
                      NvU32 SizeOfBlockContext,
                      NvMMInternalCreationParameters* pParams,
                      NvOsSemaphoreHandle hSemaphore,
                      NvMMBlockDoWorkFunction DoWorkFunction,
                      NvMMPrivateCloseFunction PrivateCloseFunction,
                      NvMMGetBufferRequirementsFunction GetBufferRequirements)
{
    NvError           Status = NvSuccess;
    NvMMBlock*        pBlock   = NULL; /* MM block referenced by handle */ 
    NvMMBlockContext* pContext = NULL; /* Context of this instance of reference block */

    NVMM_CHK_ARG(pParams);
    //
    // Init NvMMBlock and NvMMBlockContext structs with base class defaults
    //
    pBlock = NvOsAlloc(sizeof(NvMMBlock));
    NVMM_CHK_MEM(pBlock);
    NvOsMemset(pBlock, 0, sizeof(NvMMBlock));

    pContext = NvOsAlloc(SizeOfBlockContext);
    NVMM_CHK_MEM(pContext);
    NvOsMemset(pContext, 0, SizeOfBlockContext);
    pBlock->pContext = pContext;

    pContext->pStreams = NvOsAlloc(sizeof(NvMMStream*) * NVMMBLOCK_MAXSTREAMS);
    NVMM_CHK_MEM(pContext->pStreams);
    NvOsMemset(pContext->pStreams, 0, (sizeof(NvMMStream*) * NVMMBLOCK_MAXSTREAMS));

    pBlock->StructSize                = sizeof(NvMMBlock);
    pBlock->BlockSemaphoreTimeout     = NV_WAIT_INFINITE;

    pBlock->SetTransferBufferFunction = NvMMBlockSetTransferBufferFunction;
    pBlock->GetTransferBufferFunction = NvMMBlockGetTransferBufferFunction;
    pBlock->SetBufferAllocator        = NvMMBlockSetBufferAllocator;
    pBlock->SetSendBlockEventFunction = NvMMBlockSetSendBlockEventFunction;
    pBlock->SetState                  = NvMMBlockSetState;
    pBlock->GetState                  = NvMMBlockGetState;
    pBlock->AbortBuffers              = NvMMBlockAbortBuffers;
    pBlock->SetAttribute              = NvMMBlockSetAttribute;
    pBlock->GetAttribute              = NvMMBlockGetAttribute;



    NvOsMemcpy(&pContext->params, pParams, sizeof(NvMMInternalCreationParameters));

    pContext->State                   = NvMMState_Stopped;
    pContext->eLocale                 = pParams->Locale;
    pContext->hBlockEventSema         = hSemaphore;
    pContext->DoWork                  = DoWorkFunction;
    pContext->PrivateClose            = PrivateCloseFunction;
    pContext->GetBufferRequirements   = GetBufferRequirements;
    pContext->bEnableUlpMode          = pContext->params.SetULPMode;
    pContext->bSendStreamEventNotifications = NV_TRUE;
    pContext->bSendBlockEventNotifications =  NV_TRUE;
    pContext->bDelayedAlloc           =  NV_FALSE;


    NVMM_CHK_ERR(NvRmOpen(&pContext->hRmDevice, 0));


    NVMM_CHK_ERR(NvOsMutexCreate(&pContext->hBlockMutex));
    NVMM_CHK_ERR(NvOsMutexCreate(&pContext->hBlockCloseMutex));


    *phBlock = pBlock;
    NV_DEBUG_PRINTF(("NvMMBlockOpen: hBlock 0x%X \n", pBlock));

cleanup:
    if (Status != NvSuccess)
    {
        NvMMBlockClose(pBlock);
    }
    return Status;    
}

void NvMMBlockClose(NvMMBlockHandle hBlock)
{
    if (hBlock)
    {
        NvMMBlockContext* const pContext = hBlock->pContext;
        NV_DEBUG_PRINTF(("NvMMBlockClose: hBlock 0x%X \n", hBlock));
        if (pContext)
        {
            NvOsFree(pContext->pStreams);
            NvOsMutexDestroy(pContext->hBlockMutex);
            NvOsMutexDestroy(pContext->hBlockCloseMutex);
            NvRmClose(pContext->hRmDevice);
            NvOsFree(pContext);
        }
        NvOsFree(hBlock);
    }
}

void NvMMBlockTryClose(NvMMBlockHandle hBlock)
{
    NvMMBlockContext* const pContext = hBlock->pContext;
    NvU32 StreamIndex;
    NvU32 i;
    NvBool bInResult = NV_TRUE;
    NvMMStreamShutdownInfo streamShutDown;
#if NV_DEBUG
    NvError status;
#endif

    NvOsMutexLock(pContext->hBlockCloseMutex);
    NvOsMutexLock(pContext->hBlockMutex);

    NV_DEBUG_PRINTF(("NvMMBlockTryClose: hBlock 0x%X \n", hBlock));

    if(pContext->bAbnormalTermination == NV_TRUE)
    {
        for(StreamIndex=0; StreamIndex < pContext->StreamCount; StreamIndex++)
        {
            NvMMStream* pStream = pContext->pStreams[StreamIndex];
            if(pStream)
            {
                if(pStream->bBlockIsAllocator)
                {
                    for (i = 0; i < pStream->NumBuffers; i++)
                    {
                        if (pStream->pBuf[i])
                        {
                            #if NV_DEBUG
                            status = 
                            #endif
                            NvMMUtilDeallocateBlockSideBuffer(pStream->pBuf[i], 
                                                              pContext->params.hService,
                                                              pContext->bAbnormalTermination);
                            NV_ASSERT(status == NvSuccess);
                        }
                    }
                }
            }
        }
        NvOsMutexUnlock(pContext->hBlockMutex);
        NvOsMutexUnlock(pContext->hBlockCloseMutex);
        pContext->PrivateClose(hBlock); // really destroy the block
    }
    else
    {
    /* return buffers if we are not allocator , free them if we are allocator */
    for (StreamIndex=0; StreamIndex < pContext->StreamCount; StreamIndex++)
    {
        NvMMStream* pStream = pContext->pStreams[StreamIndex];

        if (pStream)
        {
            if (pStream->bBlockIsAllocator)
            {
                NvMMBlockDeallocateBuffers(hBlock, StreamIndex);
                bInResult = NV_TRUE;
            }
            else
            {
#if NV_DEBUG
                status = 
#endif
                    NvMMBlockReturnBuffersToAllocator(hBlock, StreamIndex);
                NV_ASSERT(status == NvSuccess);
                bInResult &= NvMMBlockAreAllBuffersReturned(hBlock, StreamIndex);
            }
            if (bInResult)
            {
                // Send stream shutdown event only if the connecting block has not sent
                // Check pStream->TransferBufferFromBlock because stream may not be connected
                if (!pStream->bStreamShuttingDown && pStream->TransferBufferFromBlock)
                {
                    streamShutDown.structSize  = sizeof(NvMMStreamShutdownInfo);
                    streamShutDown.event       = NvMMEvent_StreamShutdown;
                    streamShutDown.streamIndex = StreamIndex;

#if NV_DEBUG
                    status = 
#endif
                        pStream->TransferBufferFromBlock(pStream->pOutgoingTBFContext, 
                        pStream->OutgoingStreamIndex, 
                        NvMMBufferType_StreamEvent,
                        sizeof(NvMMStreamShutdownInfo),
                        &streamShutDown);
                }
            }
        }

        if(pStream && pStream->bTBFSet)
        {
            if((pStream->bNewConfigurationReceived == NV_FALSE) || 
               (pStream->bNewBufferRequirementsReceived == NV_FALSE) ||
               (pStream->bBufferAllocationFailed == NV_TRUE))
            {
                if(pStream->bBuffersNegotiationCompleteEventSent == NV_FALSE)
                {
                    NvMMEventBlockBufferNegotiationCompleteInfo   BlockBufferNegotiationComplete;
                    pStream->bBuffersNegotiationCompleteEventSent = NV_TRUE;
                    BlockBufferNegotiationComplete.structSize     = sizeof(NvMMEventBlockBufferNegotiationCompleteInfo);
                    BlockBufferNegotiationComplete.event          = NvMMEvent_BlockBufferNegotiationComplete;
                    BlockBufferNegotiationComplete.streamIndex    = pStream->StreamIndex;
                    BlockBufferNegotiationComplete.eBlockType     = pContext->BlockType;
                    NvMMSendBlockEvent(pContext, 
                        NvMMEvent_BlockBufferNegotiationComplete, 
                        sizeof(NvMMEventBlockBufferNegotiationCompleteInfo),  
                        &BlockBufferNegotiationComplete);
                }
            }
        }
    }

    NvOsMutexUnlock(pContext->hBlockMutex);
    NvOsMutexUnlock(pContext->hBlockCloseMutex);

    if (bInResult)
    {
        pContext->PrivateClose(hBlock); // really destroy the block
    }
    else
    {
        pContext->bDoBlockClose = NV_TRUE; // delayed close
    }
}
}


NvError NvMMBlockSetState(NvMMBlockHandle hBlock, NvMMState eState)
{
    NvError status = NvSuccess;
    unsigned int i;
    NvMMBlockContext* const pContext = hBlock->pContext;
    NvMMSetStateCompleteInfo SetStateCompleteEvent;

    pContext->State = eState;

    if (eState == NvMMState_Stopped)
    {
        // If stopped, rewind all streams
        for (i=0; i < pContext->StreamCount; i++)
        {
            NvMMStream* const pStream = pContext->pStreams[i];
            if (pStream)
                pStream->Position = 0;
        }
    }

    if((pContext->SendEvent) && (pContext->bAbnormalTermination == NV_FALSE))
    {
        SetStateCompleteEvent.structSize = sizeof(NvMMSetStateCompleteInfo);
        SetStateCompleteEvent.event = NvMMEvent_SetStateComplete;
        SetStateCompleteEvent.State = eState;

        status = NvMMSendBlockEvent(pContext,
            NvMMEvent_SetStateComplete,
            sizeof(NvMMSetStateCompleteInfo),
            &SetStateCompleteEvent);
        NV_ASSERT(status == NvSuccess);
    }

    if (eState == NvMMState_Running)
    {
        NvOsSemaphoreSignal(pContext->hBlockEventSema);
    }

    return status;
}

NvError NvMMBlockGetState(NvMMBlockHandle hBlock, NvMMState *peState)
{
    NvMMBlockContext* const pContext = hBlock->pContext;
    *peState = pContext->State;
    return NvSuccess;
}

NvError NvMMBlockSetAttribute(NvMMBlockHandle hBlock, NvU32 AttributeType, NvU32 SetAttrFlag, NvU32 AttributeSize, void *pAttribute)
{
    NvError status = NvSuccess;
    NvMMBlockContext* const pContext = hBlock->pContext;
    NvMMStream* pStream;
    NvU32 StreamIndex;

    switch(AttributeType)
    {
    case NvMMAttribute_Profile:
        NV_ASSERT(AttributeSize == sizeof(NvMMAttrib_ProfileInfo));
        pContext->ProfileStatus = (NvU8)((NvMMAttrib_ProfileInfo*)pAttribute)->ProfileType;
        break;
        //This enables the ULP mode for nvmm block
    case NvMMAttribute_UlpMode:
        NV_ASSERT(AttributeSize == sizeof(NvMMAttrib_EnableUlpMode));
        pContext->bEnableUlpMode = ((NvMMAttrib_EnableUlpMode*)pAttribute)->bEnableUlpMode;
        break;
    case NvMMAttribute_BufferNegotiationMode:
        NV_ASSERT(AttributeSize == sizeof(NvMMAttrib_BufferNegotiationMode));
        StreamIndex = ((NvMMAttrib_BufferNegotiationMode*)pAttribute)->StreamIndex;
        pStream = NvMMBlockGetStream(pContext, StreamIndex);
        pStream->bEnableBufferNegotiation = ((NvMMAttrib_BufferNegotiationMode*)pAttribute)->bEnableBufferNegotiation;
        if(pStream->bBlockIsAllocator == NV_FALSE)
            pStream->NumBuffers = ((NvMMAttrib_BufferNegotiationMode*)pAttribute)->NumBuffers;
        if(pStream->bStreamShuttingDown == NV_TRUE)
            pStream->bStreamShuttingDown = NV_FALSE;
        break;
    case NvMMAttribute_PowerState:
        NV_ASSERT(AttributeSize == sizeof(NvMMAttrib_PowerStateInfo));
        pContext->PowerState = ((NvMMAttrib_PowerStateInfo*)pAttribute)->PowerState;
        break;
    case NvMMAttribute_EventNotificationMode:
        NV_ASSERT(AttributeSize == sizeof(NvMMAttrib_EventNotificationInfo));
        pContext->bSendStreamEventNotifications = ((NvMMAttrib_EventNotificationInfo*)pAttribute)->bStreamEventNotification;
        pContext->bSendBlockEventNotifications = ((NvMMAttrib_EventNotificationInfo*)pAttribute)->bBlockEventNotification;
        break;   
    case NvMMAttribute_AbnormalTermination:
        NV_ASSERT(AttributeSize == sizeof(NvMMAttrib_AbnormalTermInfo));
        pContext->bAbnormalTermination = ((NvMMAttrib_AbnormalTermInfo*)pAttribute)->bAbnormalTermination;
        break;
    case NvMMAttribute_SetTunnelMode:
        NV_ASSERT(AttributeSize == sizeof(NvMMAttrib_SetTunnelModeInfo));
        StreamIndex = ((NvMMAttrib_SetTunnelModeInfo*)pAttribute)->StreamIndex;
        pStream = NvMMBlockGetStream(pContext, StreamIndex);
        pStream->bDirectTunnel = ((NvMMAttrib_SetTunnelModeInfo*)pAttribute)->bDirectTunnel ;
        break;
    }

    if ((SetAttrFlag & NvMMSetAttrFlag_Notification) && pContext->SendEvent)
    {
        NvMMEventSetAttributeCompleteInfo SetAttributeCompleteEvent;

        SetAttributeCompleteEvent.structSize = sizeof(SetAttributeCompleteEvent);
        SetAttributeCompleteEvent.event = NvMMEvent_SetAttributeComplete;

        status = NvMMSendBlockEvent(pContext,
            NvMMEvent_SetAttributeComplete,
            sizeof(SetAttributeCompleteEvent),
            &SetAttributeCompleteEvent);

        NV_ASSERT(status == NvSuccess);
    }

    return status;
}

NvError NvMMBlockGetAttribute(NvMMBlockHandle hBlock, NvU32 AttributeType, NvU32 AttributeSize, void* pAttribute)
{
    NvError status = NvSuccess;
    NvMMBlockContext* const pContext = hBlock->pContext;

    switch (AttributeType)
    {
    case NvMMAttribute_StreamPosition:
        {
            NvMMAttrib_StreamPosition* pPos = (NvMMAttrib_StreamPosition*) pAttribute;
            NvMMStream* pStream = NvMMBlockGetStream(pContext, pPos->StreamIndex);

            NV_ASSERT(AttributeSize == sizeof(NvMMAttrib_StreamPosition));

            if (pStream)
            {
                pPos->Position = pStream->Position;
            }
            else
            {
                NV_ASSERT(NV_FALSE); // bad stream index
            }

        }
        break;

    case NvMMAttribute_GetStreamInfo:
        {
            NvMMAttrib_StreamInfo *pStreamInfo = (NvMMAttrib_StreamInfo*)pAttribute;
            NvMMStream* pStream = NvMMBlockGetStream(pContext, pStreamInfo->StreamIndex);

            NV_ASSERT(AttributeSize == sizeof(NvMMAttrib_StreamInfo));

            if (pStream)
            {
                pStreamInfo->StreamIndex = pStream->StreamIndex;
                pStreamInfo->NumBuffers = pStream->NumBuffers;
                pStreamInfo->Direction = pStream->Direction;
                pStreamInfo->bBlockIsAllocator = pStream->bBlockIsAllocator;
            }
            else
            {
                NV_ASSERT(NV_FALSE); // bad stream index
            }
        }
        break;

    case NvMMAttribute_GetBlockInfo:
        {
            NvMMAttrib_BlockInfo *bInfo = (NvMMAttrib_BlockInfo*)pAttribute;
            NvU32 i;
            NvU32 StreamCount = 0;
            NvMMStream *pTempStream;
            NV_ASSERT(AttributeSize     == sizeof(NvMMAttrib_BlockInfo));
            for(i = 0; i < pContext->StreamCount; i++)
            {
                pTempStream = pContext->pStreams[i];
                if(pTempStream->TransferBufferFromBlock)
                    StreamCount++;
            }
            bInfo->eLocale              = pContext->eLocale;
            bInfo->NumStreams           = StreamCount;
        }
        break;

    default:
#if NV_DEBUG
        NV_DEBUG_PRINTF(("NvMMBlockGetAttribute: type 0x%X (%d) unknown\n", AttributeType, AttributeType));
#endif
        break;
    }

    return status;
}

void NvMMBlockSetSendBlockEventFunction(NvMMBlockHandle hBlock, void *pClientContext, SendBlockEventFunction SendEvent)
{
    NvMMBlockContext* const pContext = hBlock->pContext;
    pContext->SendEvent = SendEvent;
    pContext->pSendEventContext = pClientContext;
}

NvError NvMMBlockSetTransferBufferFunction(NvMMBlockHandle hBlock, 
                                           NvU32 StreamIndex,
                                           NvMMTransferBufferFunction TransferBuffer,
                                           void* pContextForTranferBuffer,
                                           NvU32 StreamIndexForTransferBuffer)
{
    NvError status;
    NvMMBlockContext* const pContext = hBlock->pContext;
    NvMMStream* pStream;
    NvMMNewBufferRequirementsInfo bufReq; 
    NvMMEventBlockBufferNegotiationCompleteInfo BlockBufferNegotiationComplete;

    NV_ASSERT(StreamIndex < pContext->StreamCount);
    pStream = pContext->pStreams[StreamIndex];
    NV_ASSERT(pStream);

    pStream->bBuffersNegotiationCompleteEventSent = NV_FALSE;
    //
    // Apply new transfer buffer function
    //
    pStream->TransferBufferFromBlock = TransferBuffer;
    pStream->pOutgoingTBFContext     = pContextForTranferBuffer;    
    pStream->OutgoingStreamIndex     = StreamIndexForTransferBuffer;
    pStream->bTBFSet = NV_TRUE;


    if((pStream->bEnableBufferNegotiation == NV_FALSE)  || (pStream->bStreamShuttingDown == NV_TRUE))
    {
        if(pStream->bBuffersNegotiationCompleteEventSent  == NV_FALSE)
        {
            pStream->bBuffersNegotiationCompleteEventSent = NV_TRUE;
            BlockBufferNegotiationComplete.structSize     = sizeof(NvMMEventBlockBufferNegotiationCompleteInfo);
            BlockBufferNegotiationComplete.event          = NvMMEvent_BlockBufferNegotiationComplete;
            BlockBufferNegotiationComplete.streamIndex    = StreamIndex;
            BlockBufferNegotiationComplete.eBlockType     = pContext->BlockType;
            BlockBufferNegotiationComplete.error          = NvSuccess;
            NvMMSendBlockEvent(pContext, 
                NvMMEvent_BlockBufferNegotiationComplete, 
                sizeof(NvMMEventBlockBufferNegotiationCompleteInfo),  
                &BlockBufferNegotiationComplete);
        }

        return NvSuccess;
    } 
    //
    // Initiate the negotiation by sending initial buffer requirements (retry count 0)
    //
    pStream->ConfigRetry = 0;
    pContext->GetBufferRequirements(hBlock, StreamIndex, 0, &bufReq); // get reqs from block implementation

    NV_ASSERT((bufReq.structSize == sizeof(NvMMNewBufferRequirementsInfo)) &&
        (bufReq.event == NvMMEvent_NewBufferRequirements));

    if (pContext->params.Locale == NvMMLocale_AVP)
        bufReq.bInSharedMemory = NV_TRUE;

    status = TransferBuffer(pContextForTranferBuffer, 
        StreamIndexForTransferBuffer, 
        NvMMBufferType_StreamEvent,
        sizeof(NvMMNewBufferRequirementsInfo),
        &bufReq);
    NV_ASSERT(status == NvSuccess);
    return status;
}

NvMMTransferBufferFunction NvMMBlockGetTransferBufferFunction(NvMMBlockHandle hBlock, NvU32 StreamIndex, void* pContextForTranferBuffer)
{
    return NvMMBlockTransferBufferToBlock;
}

NvError NvMMBlockAbortBuffers(NvMMBlockHandle hBlock, NvU32 StreamIndex)
{
    NvMMBlockContext* const pContext = hBlock->pContext;
    NvError status = NvSuccess;
    NvMMEventBlockAbortDoneInfo AbortBlockEvent;
    NvU32 i, QueueEntries;
    NvMMStream *pStream;
    NvMMBuffer *pBuffer;


    NV_DEBUG_PRINTF(("NvMMBlockAbortBuffers::In hBlock 0x%X\t StreamIndex = %x\n", hBlock, StreamIndex));


    NV_ASSERT(StreamIndex < pContext->StreamCount);
    pStream = pContext->pStreams[StreamIndex];
    NV_ASSERT(pStream);

    NvOsMutexLock(pContext->hBlockMutex);

    QueueEntries = NvMMQueueGetNumEntries(pStream->BufQ);

    /* Return the buffers to outgoing block */
    for(i = 0; i < QueueEntries; i++)
    {
        status = NvMMQueueDeQ(pStream->BufQ, &pBuffer);
        if(status != NvSuccess || !pStream->TransferBufferFromBlock)
            break;
        if(pBuffer->PayloadInfo.BufferFlags & NvMMBufferFlag_DRMPlayCount)
        {
            NvMMEventDRMPlayCntInfo DRMPlayCntInfo;
            DRMPlayCntInfo.structSize  = sizeof(NvMMEventDRMPlayCntInfo);
            DRMPlayCntInfo.event       = NvMMEvent_DRMPlayCount;
            DRMPlayCntInfo.coreContext = (NvU32)pBuffer->pCore;
            DRMPlayCntInfo.drmContext  = 0;
            DRMPlayCntInfo.bDoMetering = 0;
            NV_LOGGER_PRINT((NVLOG_WMDRM, NVLOG_DEBUG, "Found DRM Context in NvMMBlockAbortBuffers"));
            status = pStream->TransferBufferFromBlock(pStream->pOutgoingTBFContext,
                pStream->OutgoingStreamIndex,
                NvMMBufferType_StreamEvent,
                sizeof(NvMMEventDRMPlayCntInfo),
                &DRMPlayCntInfo);
        }
        if(pBuffer->PayloadInfo.BufferFlags & NvMMBufferFlag_EndOfTrack)
            pBuffer->PayloadInfo.BufferFlags &= 0xFFFFFFF7;

        if((pBuffer->PayloadType != NvMMPayloadType_SurfaceArray) && 
            (!(pBuffer->PayloadInfo.BufferFlags & NvMMBufferFlag_OffsetList)))
        {
            pBuffer->Payload.Ref.sizeOfValidDataInBytes = 0;
            pBuffer->Payload.Ref.startOfValidData = 0;
            pBuffer->PayloadInfo.TimeStamp = 0;
        }

        //NV_ASSERT(status == NvSuccess);
        status = pStream->TransferBufferFromBlock(pStream->pOutgoingTBFContext, 
            pStream->OutgoingStreamIndex, 
            NvMMBufferType_Payload,
            sizeof(NvMMBuffer),
            pBuffer);
        //NV_ASSERT(status == NvSuccess);
        if(status != NvSuccess)
            break;
    }
    pStream->bEndOfStream = NV_FALSE;
    pStream->bEndOfStreamEventSent = NV_FALSE;
    NvOsMutexUnlock(pContext->hBlockMutex);

    /* Send a Block event */
    if (pContext->SendEvent)
    {
        AbortBlockEvent.structSize = sizeof(NvMMEventBlockAbortDoneInfo);
        AbortBlockEvent.event = NvMMEvent_BlockAbortDone;
        AbortBlockEvent.streamIndex = StreamIndex;
        status = NvMMSendBlockEvent(pContext,
            NvMMEvent_BlockAbortDone,
            sizeof(NvMMEventBlockAbortDoneInfo),
            &AbortBlockEvent);
        NV_ASSERT(status == NvSuccess);
    }


    return status;
}

static NvBool NvMMBlockNegotiateBufferConfig(NvMMBlockHandle hBlock, NvMMStream* pStream)
{
    NvU32 val, retry;
    NvMMBlockContext*        const  pContext = hBlock->pContext;
    NvMMNewBufferRequirementsInfo*  pTheir = &pStream->OutgoingBufReqs;
    NvMMNewBufferRequirementsInfo   our;
    NvMMNewBufferConfigurationInfo* pConfig = &pStream->BufConfig;

    if (!pTheir->structSize) // other side's requirements not known yet
        return NV_FALSE;

    NvOsMemset(pConfig, 0, sizeof(NvMMNewBufferConfigurationInfo));
    pConfig->structSize  = sizeof(NvMMNewBufferConfigurationInfo);
    pConfig->event       = NvMMEvent_NewBufferConfiguration;

    for (retry=0;; ++retry)
    {
        NV_ASSERT(retry < 256); // only 256 configs supported

        //
        // Enumerate our supported buffer config sets
        //
        if (!pContext->GetBufferRequirements(hBlock, pStream->StreamIndex, retry, &our))
            return NV_FALSE;

        //
        // Try to negotiate current config with other side's requirements
        //

        // - Find common number of buffers
        val = (our.minBuffers + our.maxBuffers) >> 1;

        if (val < pTheir->minBuffers)
        {
            if ((val = pTheir->minBuffers) > our.maxBuffers)
                continue;
        }

        if (val > pTheir->maxBuffers)
        {
            if ((val = pTheir->maxBuffers) < our.minBuffers)
                continue;
        }

        pConfig->numBuffers = val;

        // - Find common buffer size
        val = (our.minBufferSize + our.maxBufferSize) >> 1;

        if (val < pTheir->minBufferSize)
        {
            if ((val = pTheir->minBufferSize) > our.maxBufferSize)
                continue;
        }

        if (val > pTheir->maxBufferSize)
        {
            if ((val = pTheir->maxBufferSize) < our.minBufferSize)
                continue;
        }

        pConfig->bufferSize = val;

        // - Find common byte alignment
        pConfig->byteAlignment = our.byteAlignment > pTheir->byteAlignment ? our.byteAlignment : pTheir->byteAlignment;

        // - If any side wants contiguous memory, use that
        pConfig->bPhysicalContiguousMemory = our.bPhysicalContiguousMemory | pTheir->bPhysicalContiguousMemory;

        // - Memory space must match
        if (our.memorySpace != pTheir->memorySpace)
            continue;
        pConfig->memorySpace = our.memorySpace;

        // - Endianess must match
        if (our.endianness != pTheir->endianness)
            continue;
        pConfig->endianness = our.endianness;

        // - If any side wants to use shared memory, use that
        pConfig->bInSharedMemory = our.bInSharedMemory | pTheir->bInSharedMemory;

        //xxx review design: block implementation specific fields cannot be negotiated

        pConfig->formatId = our.formatId;
        pConfig->format = our.format;

        if(pTheir->formatId == NvMMBufferFormatId_Video)
        {
            pConfig->format.videoFormat.bImage = pTheir->format.videoFormat.bImage;
        }

        pStream->ConfigRetry = (NvU8)retry;
        return NV_TRUE; // Negotiation succeeded
    }
}

static NvBool NvMMBlockVerifyBufferConfig(NvMMBlockHandle hBlock, NvMMStream* pStream, NvMMNewBufferConfigurationInfo* pConfig)
{
    NvMMBlockContext* const pContext = hBlock->pContext;
    NvMMNewBufferRequirementsInfo our;

#if NV_DEBUG
    NvBool ret =
#endif
        pContext->GetBufferRequirements(hBlock, pStream->StreamIndex, pStream->ConfigRetry, &our);
    NV_ASSERT(ret); // Must succeed, since pStream->ConfigRetry is valid

    //
    // Test if the config satisfies our requirements
    //
    if ((pConfig->numBuffers < our.minBuffers) ||
        (pConfig->numBuffers > our.maxBuffers) ||
        (pConfig->bufferSize < our.minBufferSize) ||
        (pConfig->bufferSize > our.maxBufferSize) ||
        (pConfig->byteAlignment < our.byteAlignment) ||
        (pConfig->bPhysicalContiguousMemory < our.bPhysicalContiguousMemory) ||
        (pConfig->memorySpace != our.memorySpace) ||
        (pConfig->endianness != our.endianness) ||
        (pConfig->bInSharedMemory < our.bInSharedMemory))
    {
        return NV_FALSE;
    }

    // xxx review: pConfig->format cannot be verified

    return NV_TRUE;
}

static NvError NvMMBlockEventNewBufferRequirements(NvMMBlockHandle hBlock, NvMMStream* pStream, NvMMNewBufferRequirementsInfo* pEvent)
{
    NvError                         status   = NvSuccess;
    NvMMBlockContext* const         pContext = hBlock->pContext;
    NvMMNewBufferConfigurationInfo  BufConfig;
    NvMMBufferNegotiationFailedInfo failedEvent;

    NvOsMutexLock(pContext->hBlockMutex);

    //
    // Store 'their' new requirements
    //
    pStream->OutgoingBufReqs = *pEvent;

    //
    // If we are allocator, we negotiate a new buffer config and propose it to the other side
    //
    if (pStream->bBlockIsAllocator)
    {
        if (NvMMBlockNegotiateBufferConfig(hBlock, pStream) && pStream->TransferBufferFromBlock)
        {
            BufConfig = pStream->BufConfig; // make local copy before unlocking mutex

            NvOsMutexUnlock(pContext->hBlockMutex);

            status = pStream->TransferBufferFromBlock(pStream->pOutgoingTBFContext, 
                pStream->OutgoingStreamIndex, 
                NvMMBufferType_StreamEvent,
                sizeof(NvMMNewBufferConfigurationInfo), 
                &BufConfig);
            NV_ASSERT(status == NvSuccess); //xxx fixme: how to handle errors in TransferBufferToBlock ?
            return status;
        }
        else
        {
            if (pContext->SendEvent)
            {
                failedEvent.structSize = sizeof(NvMMBufferNegotiationFailedInfo);
                failedEvent.event = NvMMEvent_BufferNegotiationFailed;
                failedEvent.StreamIndex = pStream->StreamIndex;

                status = NvMMSendBlockEvent(pContext,
                    NvMMEvent_BufferNegotiationFailed,
                    sizeof(NvMMBufferNegotiationFailedInfo),
                    &failedEvent);
            }
        }
    }

    NvOsMutexUnlock(pContext->hBlockMutex);
    return status;
}

static NvError NvMMBlockEventNewBufferConfiguration(NvMMBlockHandle hBlock, NvMMStream* pStream, NvMMNewBufferConfigurationInfo* pEvent)
{
    NvError                             status   = NvSuccess;
    NvMMBlockContext* const             pContext = hBlock->pContext;
    NvMMNewBufferConfigurationReplyInfo reply;
    NvMMNewBufferRequirementsInfo       bufReq; 
    NvMMBufferNegotiationFailedInfo     failedEvent;

    NvOsMutexLock(pContext->hBlockMutex);

    NvOsMemset(&reply, 0, sizeof(reply));
    reply.structSize = sizeof(NvMMNewBufferConfigurationReplyInfo);
    reply.event      = NvMMEvent_NewBufferConfigurationReply;
    reply.bAccepted  = NvMMBlockVerifyBufferConfig(hBlock, pStream, pEvent);

    if (!reply.bAccepted)
    {
        //
        // If we did not accept the proposed configuration, 
        // we reinitate the negotiation by sending the next alternate requirements set
        //
        if (!pContext->GetBufferRequirements(hBlock, pStream->StreamIndex, ++(pStream->ConfigRetry), &bufReq))
        {
            NvOsMutexUnlock(pContext->hBlockMutex);

            if (pContext->SendEvent)
            {
                failedEvent.structSize = sizeof(NvMMBufferNegotiationFailedInfo);
                failedEvent.event = NvMMEvent_BufferNegotiationFailed;
                failedEvent.StreamIndex = pStream->StreamIndex;

                status = NvMMSendBlockEvent(pContext,
                    NvMMEvent_BufferNegotiationFailed,
                    sizeof(NvMMBufferNegotiationFailedInfo),
                    &failedEvent);
            }

            return status;
        }
    }
    else
    {
        pStream->NumBuffers = pEvent->numBuffers;
    }

    NvOsMutexUnlock(pContext->hBlockMutex);

    if(pStream->TransferBufferFromBlock)
    {
        status = pStream->TransferBufferFromBlock(pStream->pOutgoingTBFContext,
            pStream->OutgoingStreamIndex, 
            NvMMBufferType_StreamEvent, 
            sizeof(NvMMNewBufferConfigurationReplyInfo),
            &reply);
        NV_ASSERT(status == NvSuccess); //xxx fixme: how to handle errors in TransferBufferToBlock ?
    }

    if (!reply.bAccepted && pStream->TransferBufferFromBlock)
    {
        NV_ASSERT((bufReq.structSize == sizeof(NvMMNewBufferRequirementsInfo)) &&
            (bufReq.event == NvMMEvent_NewBufferRequirements));

        status = pStream->TransferBufferFromBlock(pStream->pOutgoingTBFContext,
            pStream->OutgoingStreamIndex, 
            NvMMBufferType_StreamEvent, 
            sizeof(NvMMNewBufferRequirementsInfo),
            &bufReq);
        NV_ASSERT(status == NvSuccess);
    }

    /* if we are not the allocator send NvMMEvent_BlockBufferNegotiationComplete event now */
    if((reply.bAccepted == NV_TRUE) && (pStream->bBlockIsAllocator == NV_FALSE))
    {
        if(pStream->bBuffersNegotiationCompleteEventSent == NV_FALSE)
        {
            NvMMEventBlockBufferNegotiationCompleteInfo   BlockBufferNegotiationComplete;
            pStream->bBuffersNegotiationCompleteEventSent = NV_TRUE;
            BlockBufferNegotiationComplete.structSize     = sizeof(NvMMEventBlockBufferNegotiationCompleteInfo);
            BlockBufferNegotiationComplete.event          = NvMMEvent_BlockBufferNegotiationComplete;
            BlockBufferNegotiationComplete.streamIndex    = pStream->StreamIndex;
            BlockBufferNegotiationComplete.eBlockType     = pContext->BlockType;
            BlockBufferNegotiationComplete.error          = status;
            NvMMSendBlockEvent(pContext, 
                NvMMEvent_BlockBufferNegotiationComplete, 
                sizeof(NvMMEventBlockBufferNegotiationCompleteInfo),  
                &BlockBufferNegotiationComplete);
        }
    }

    return status;
}

static NvError NvMMBlockEventReNegotiateBufferRequirements(NvMMBlockHandle hBlock, NvMMStream* pStream, NvMMNewBufferRequirementsInfo* pEvent)
{
    NvError                         status   = NvSuccess;
    //    NvMMBlockContext* const         pContext = hBlock->pContext;

    if (!pStream->bWaitToRenegotiate)
    {
        // Store 'their' new requirements
        pStream->OutgoingBufReqs = *pEvent;
    }
    if (pStream->bBlockIsAllocator)
    {
        // Check all the buffers are in the queue
        if (pStream->NumBuffers == NvMMQueueGetNumEntries(pStream->BufQ))
        {
            NvU32 i;
            NvMMBuffer *pBuffer;

            pStream->bWaitToRenegotiate = NV_FALSE;
            // Dequeue all the existing buffers and delete them
            for(i = 0; i < pStream->NumBuffers; i++)
            {
                status = NvMMQueueDeQ(pStream->BufQ, &pBuffer);
            }
            NvMMBlockDeallocateBuffers(hBlock, pStream->StreamIndex);
            NvMMBlockEventNewBufferRequirements(hBlock,pStream, &pStream->OutgoingBufReqs);
        }
        else if (!pStream->bWaitToRenegotiate && pStream->TransferBufferFromBlock)
        {
            pStream->bWaitToRenegotiate = NV_TRUE;
        }
    }
    return status;
}

NvError NvMMBlockTransferBufferToBlock(void *hBlock, NvU32 StreamIndex, NvMMBufferType BufferType, NvU32 BufferSize, void* pBuffer)
{
    NvError                 status = NvSuccess;
    NvMMBlockContext* const pContext = ((NvMMBlock*)hBlock)->pContext;
    NvMMStream *            pStream;
    NvU32                   BufferID, EventType;
    NvU32  coreHandle;
    NvBool bEventHandled    = NV_FALSE;

    /* This TBF can be called both, from multiple threads at the same time, and recursively.
    Any operations that do non-atomic writes to the block's context must be mutex protected.
    Mutex protection is performed at the most inner level to avoid deadlocks for recursive calls.
    */
    if(!pContext)
        return NvError_BadParameter;

    if(StreamIndex >= pContext->StreamCount)
        return NvError_BadParameter;

    NV_ASSERT(StreamIndex < pContext->StreamCount);
    pStream = pContext->pStreams[StreamIndex];
    NV_ASSERT(pStream);   

    if(!pStream)
        return NvError_BadParameter;

    if (BufferType == NvMMBufferType_StreamEvent)
    {
        /* handle events specific to blocks */
        if(pContext->TransferBufferEventFunction)
        {
            NvOsMutexLock(pContext->hBlockCloseMutex);
            pContext->TransferBufferEventFunction(hBlock, StreamIndex, BufferType, BufferSize, pBuffer, &bEventHandled);
            NvOsMutexUnlock(pContext->hBlockCloseMutex);
        }

        if(bEventHandled == NV_FALSE)
        {
            EventType = (NvU32)(((NvMMStreamEventBase*)pBuffer)->event);
            switch (EventType)
            {
                /* 
                * TODO: handle midstream buffer requirements change case
                * (present code assumes initial buf requirements)
                */

            case NvMMEvent_NewBufferRequirements:
                NV_ASSERT(((NvMMNewBufferRequirementsInfo*)pBuffer)->structSize == sizeof(NvMMNewBufferRequirementsInfo));
                if (!pStream->bBuffersAllocated)
                {
                    status = NvMMBlockEventNewBufferRequirements(hBlock, pStream, (NvMMNewBufferRequirementsInfo*)pBuffer);
                }
                else
                {
                    status = NvMMBlockEventReNegotiateBufferRequirements(hBlock, pStream, (NvMMNewBufferRequirementsInfo*)pBuffer);
                }
                pStream->bNewBufferRequirementsReceived = NV_TRUE;
                break;

            case NvMMEvent_NewBufferConfiguration:
                NV_ASSERT(!pStream->bBlockIsAllocator); // there are 2 allocators in the chain
                NV_ASSERT(((NvMMNewBufferConfigurationInfo*)pBuffer)->structSize == sizeof(NvMMNewBufferConfigurationInfo));
                pStream->bNewConfigurationReceived = NV_TRUE;
                status = NvMMBlockEventNewBufferConfiguration(hBlock, pStream, (NvMMNewBufferConfigurationInfo*)pBuffer);
                break;

            case NvMMEvent_NewBufferConfigurationReply:
                NV_ASSERT(pStream->bBlockIsAllocator); // a non-allocator must not get this event
                NV_ASSERT(((NvMMNewBufferConfigurationReplyInfo*)pBuffer)->structSize == sizeof(NvMMNewBufferConfigurationReplyInfo));
                if(((NvMMNewBufferConfigurationReplyInfo*)pBuffer)->bAccepted)
                {
                    NvOsMutexLock(pContext->hBlockMutex);
                    if (!pContext->bDelayedAlloc)
                    {
                        status = NvMMBlockAllocateBuffers(hBlock, pStream);
                        NV_ASSERT(status == NvSuccess); //xxx fixme: how to handle errors in TransferBufferToBlock ?
                    }    
                    if(pStream->bBuffersNegotiationCompleteEventSent == NV_FALSE)
                    {
                        NvMMEventBlockBufferNegotiationCompleteInfo BlockBufferNegotiationComplete;
                        NvOsMemset(&BlockBufferNegotiationComplete, 0, sizeof(NvMMEventBlockBufferNegotiationCompleteInfo));
                        pStream->bBuffersNegotiationCompleteEventSent = NV_TRUE;
                        BlockBufferNegotiationComplete.structSize     = sizeof(NvMMEventBlockBufferNegotiationCompleteInfo);
                        BlockBufferNegotiationComplete.event          = NvMMEvent_BlockBufferNegotiationComplete;
                        BlockBufferNegotiationComplete.streamIndex    = StreamIndex;
                        BlockBufferNegotiationComplete.eBlockType     = pContext->BlockType;
                        BlockBufferNegotiationComplete.error          = status;
                        NvMMSendBlockEvent(pContext, 
                            NvMMEvent_BlockBufferNegotiationComplete, 
                            sizeof(NvMMEventBlockBufferNegotiationCompleteInfo),  
                            &BlockBufferNegotiationComplete);
                    }
                    NvOsMutexUnlock(pContext->hBlockMutex);
                }
                break;

            case NvMMEvent_StreamShutdown:
                NV_DEBUG_PRINTF(("StreamShutDown Event \n"));
                NvOsMutexLock(pContext->hBlockMutex);
                pStream->TransferBufferFromBlock = NULL;
                pStream->bStreamShuttingDown = NV_TRUE;
                NvOsMutexUnlock(pContext->hBlockMutex);
                break;    

            case NvMMEvent_ResetStreamEnd:
                NvOsMutexLock(pContext->hBlockMutex);
                pStream->bEndOfStream = NV_FALSE;
                pStream->bEndOfStreamEventSent = NV_FALSE;
                NvOsMutexUnlock(pContext->hBlockMutex);
                if(pStream->Direction == NvMMStreamDir_In)
                {
                    if(pContext->pStreams[1])
                    {
                        if(pContext->pStreams[1]->TransferBufferFromBlock)
                        {
                            status = pContext->pStreams[1]->TransferBufferFromBlock(pContext->pStreams[1]->pOutgoingTBFContext,
                                pContext->pStreams[1]->OutgoingStreamIndex, 
                                NvMMBufferType_StreamEvent,
                                sizeof(NvMMEventStreamEndInfo),
                                (NvMMEventStreamEndInfo*)pBuffer);
                        }
                    }
                }
                break;

            case NvMMEvent_StreamEnd:
                if(pStream->Direction == NvMMStreamDir_In)
                {
                    pStream->bEndOfStream = NV_TRUE;
                    NV_LOGGER_PRINT((NVLOG_TRACK_LIST, NVLOG_INFO, "Dir - In: EOS Received on %x block", pContext->BlockType));
                }
                else // From Mixer
                {
                    NV_LOGGER_PRINT((NVLOG_TRACK_LIST, NVLOG_INFO, "Dir - Out: EOS Sending from %x block to Stream zero", pContext->BlockType));
                    NvMMBlockSendEOS(hBlock, pContext->pStreams[0]);
                }
                break;

            case NvMMEvent_VideoStreamInit:
                NV_ASSERT(((NvMMEventVideoStreamInitInfo*)pBuffer)->structSize == sizeof(NvMMEventVideoStreamInitInfo));
                ((NvMMBlock*)hBlock)->SetAttribute(hBlock, NvMMEvent_VideoStreamInit, 0, ((NvMMEventVideoStreamInitInfo*)pBuffer)->structSize, pBuffer);
                break;

            case NvMMEvent_AudioStreamInit:
                NV_ASSERT(((NvMMEventAudioStreamInitInfo*)pBuffer)->structSize == sizeof(NvMMEventAudioStreamInitInfo));
                NV_LOGGER_PRINT((NVLOG_TRACK_LIST, NVLOG_DEBUG, "NvMMEvent_AudioStreamInit"));
                ((NvMMBlock*)hBlock)->SetAttribute(hBlock, NvMMEvent_AudioStreamInit, 0, ((NvMMEventAudioStreamInitInfo*)pBuffer)->structSize, pBuffer);
                break;

            case NvMMEvent_DRMPlayCount:
                coreHandle = ((NvMMEventDRMPlayCntInfo*)pBuffer)->coreContext;
                if (pContext->NvMMBlockTBTBCallBack)
                {
                    NvMMBuffer offsetListBuffer;
                    NvOsMemset(&offsetListBuffer, 0,sizeof(NvMMBuffer) );
                    offsetListBuffer.PayloadInfo.BufferFlags |= NvMMBufferFlag_DRMPlayCount;
                    offsetListBuffer.BufferID = ((NvMMEventDRMPlayCntInfo*)pBuffer)->bDoMetering;
                    offsetListBuffer.pCore = (void*)coreHandle;
                    pContext->NvMMBlockTBTBCallBack(pContext, ((NvMMEventDRMPlayCntInfo*)pBuffer)->drmContext,&offsetListBuffer);
                }
                break;

            case NvMMEvent_BufferAllocationFailed:
                /* Buffer negotiation was complete but no buffers could 
                be sent because buffer allocation failed at client, send 
                BufferNegotiationComplete event so that there is no hang 
                while waiting for negotiation complete event before closing */
                pStream->bBufferAllocationFailed = NV_TRUE;
                break;
            }
        }
    }
    else if (BufferType == NvMMBufferType_Payload) /* this call transferred a buffer */
    {
        NvOsMutexLock(pContext->hBlockMutex);

        if(pStream && (pStream->bBuffersNegotiationCompleteEventSent == NV_FALSE))
        {
            if(pStream->bTBFSet)
            {
                if((pStream->bNewConfigurationReceived == NV_FALSE) || 
                    (pStream->bNewBufferRequirementsReceived == NV_FALSE))
                {
                    NvMMEventBlockBufferNegotiationCompleteInfo   BlockBufferNegotiationComplete;
                    NvOsMemset(&BlockBufferNegotiationComplete, 0, sizeof(NvMMEventBlockBufferNegotiationCompleteInfo));
                    pStream->bBuffersNegotiationCompleteEventSent = NV_TRUE;
                    BlockBufferNegotiationComplete.structSize     = sizeof(NvMMEventBlockBufferNegotiationCompleteInfo);
                    BlockBufferNegotiationComplete.event          = NvMMEvent_BlockBufferNegotiationComplete;
                    BlockBufferNegotiationComplete.streamIndex    = pStream->StreamIndex;
                    BlockBufferNegotiationComplete.eBlockType     = pContext->BlockType;
                    NvMMSendBlockEvent(pContext, 
                        NvMMEvent_BlockBufferNegotiationComplete, 
                        sizeof(NvMMEventBlockBufferNegotiationCompleteInfo),  
                        &BlockBufferNegotiationComplete);
                }
            }
        }

        // Copy incoming buffer header
        BufferID = ((NvMMBuffer*)pBuffer)->BufferID;
        *pStream->pBuf[BufferID] = *(NvMMBuffer*)pBuffer; //xxx fix this: BufferID must not be used as index

        // Mark buffer as beyound end of stream
        if (pStream->bEndOfStream)
        {
            pStream->pBuf[BufferID]->PayloadInfo.BufferFlags |= NvMMBufferFlag_EndOfStream;
        }

        // If the stream is output, and the received buffer carries payload, it is an returned empty buffer,
        // and the payload size must be 0 (see NvMMDesign.doc, 1.1.2.5 2) )
        // This was previously an assert, but just set it to 0 here.
        if (pStream->Direction == NvMMStreamDir_Out &&
            (((NvMMBuffer*)pBuffer)->PayloadType == NvMMPayloadType_MemHandle ||
            ((NvMMBuffer*)pBuffer)->PayloadType == NvMMPayloadType_MemPointer))
        {
            ((NvMMBuffer*)pBuffer)->Payload.Ref.sizeOfValidDataInBytes = 0;
        }

#if NVMM_BUFFER_PROFILING_ENABLED
        if (((pContext->BlockType == NvMMBlockType_EncH264) ||
            (pContext->BlockType == NvMMBlockType_EncAAC))&&
            pContext->NvMMBlockTransferBufferToBlockCallBack)
        {
            pContext->NvMMBlockTransferBufferToBlockCallBack(pContext,
                StreamIndex);
        }
#endif

        if (pContext->NvMMBlockTBTBCallBack)
        {
            pContext->NvMMBlockTBTBCallBack(pContext, StreamIndex, pStream->pBuf[BufferID]);
        }

        status = NvMMQueueEnQ(pStream->BufQ, &pStream->pBuf[BufferID], 0); // Queue pointer to copied struct
        NV_ASSERT(status == NvSuccess);

        NvOsMutexUnlock(pContext->hBlockMutex);
    }
    if (pContext->bDoBlockClose)
    {
        NvBool bStreamBuffersReturned = NV_TRUE;
        NvU32 i;
        NvOsMutexLock(pContext->hBlockMutex);
        for (i=0; i<pContext->StreamCount; i++)
        {
            if (pContext->pStreams[i])
            {
                bStreamBuffersReturned &= pContext->pStreams[i]->bStreamBuffersReturned;
            }
        }

        if (bStreamBuffersReturned)
        {
            NvOsMutexUnlock(pContext->hBlockMutex);
            pContext->PrivateClose(hBlock);
            return NvSuccess; // must return here, the block object was destroyed
        }
        NvOsMutexUnlock(pContext->hBlockMutex);
    }
    if (pStream->bWaitToRenegotiate)
    {
        status = NvMMBlockEventReNegotiateBufferRequirements(hBlock, pStream, NULL);
    }
    else
    {
        NvOsSemaphoreSignal(pContext->hBlockEventSema);
    }

    return status;
}

NvError NvMMBlockSendEOS(NvMMBlockHandle hBlock, NvMMStream* pStream)
{
    NvError status = NvSuccess;
    NvMMEventStreamEndInfo streamEndEvent;

    if (pStream->TransferBufferFromBlock)
    {
        streamEndEvent.structSize = sizeof(NvMMEventStreamEndInfo);
        streamEndEvent.event = NvMMEvent_StreamEnd;

        status = pStream->TransferBufferFromBlock(pStream->pOutgoingTBFContext,
            pStream->OutgoingStreamIndex, 
            NvMMBufferType_StreamEvent,
            sizeof(NvMMEventStreamEndInfo),
            &streamEndEvent);
    }

    pStream->bEndOfStreamEventSent = NV_TRUE;
    return status;
}

NvError NvMMBlockDoWork(NvMMBlockHandle hBlock, NvMMDoWorkCondition condition, NvBool *pMoreWorkPending)
{
    NvError status = NvError_Busy;
    NvMMBlockContext* const pContext = hBlock->pContext;

    //NvOsMutexLock(pContext->hBlockMutex);
    if (!pContext->bBlockProcessingData) 
    {
        pContext->bBlockProcessingData = NV_TRUE; // Prevent reentrancy
        if (pContext->PowerState == NvMMPowerState_FullOn)
        {
            status = pContext->DoWork(hBlock, condition);
        }
        pContext->bBlockProcessingData = NV_FALSE;
    }
    //NvOsMutexUnlock(pContext->hBlockMutex);

    if (status == NvError_Busy)
    {
        status = NvSuccess;
        *pMoreWorkPending = NV_TRUE;
    }
    else
    {
        *pMoreWorkPending = NV_FALSE;
    }

    return status;
}

NvError NvMMBlockSetBufferAllocator(NvMMBlockHandle hBlock, NvU32 StreamIndex, NvBool bAllocateBuffers)
{
    NvError status = NvSuccess;
    NvMMBlockContext* const pContext = hBlock->pContext;
    NvMMStream*       const pStream  = NvMMBlockGetStream(pContext, StreamIndex);

    if (!pStream)
        return NvError_BadParameter;

    NvOsMutexLock(pContext->hBlockMutex);
    //
    // xxx clarify if this can happen here:
    // if NvMMStream::bBlockIsAllocator is changed from true to false, we get a memory leak
    //
    NV_ASSERT((!pStream->bBlockIsAllocator) || bAllocateBuffers);

    //
    // change the buffer allocator setting, if necessary allocate and transmit buffers
    //
    pStream->bBlockIsAllocator = (NvU8)bAllocateBuffers;

    //
    // if we're the allocator and we know all the requirements then 
    // initiate the buffer allocation sequence by sending the other side
    // an NvMMEvent_NewBufferConfiguration event
    //
    if (bAllocateBuffers)
    {
        if (NvMMBlockNegotiateBufferConfig(hBlock, pStream) && pStream->TransferBufferFromBlock)
        {
            NvOsMutexUnlock(pContext->hBlockMutex);
            status = pStream->TransferBufferFromBlock(pStream->pOutgoingTBFContext, 
                pStream->OutgoingStreamIndex, 
                NvMMBufferType_StreamEvent,
                sizeof(NvMMNewBufferConfigurationInfo), 
                &pStream->BufConfig);
            NV_ASSERT(status == NvSuccess); // xxx no error handling ?
            return status;
        }
    }
    NvOsMutexUnlock(pContext->hBlockMutex);

    return status;
}

NvError NvMMBlockCreateStream(NvMMBlockHandle hBlock, NvU32 StreamIndex, NvMMStreamDir Direction, NvU32 MaxBufferCount)
{
    NvMMBlockContext* const pContext = hBlock->pContext;
    NvError status;
    NvMMStream* pStream = NULL;
    int j;

    NV_ASSERT(StreamIndex < NVMMBLOCK_MAXSTREAMS); // sanity check
    NV_ASSERT(MaxBufferCount <= NVMMSTREAM_MAXBUFFERS);  // sanity check, for number of max buffers

    // If the stream already exist, delete it.
    if (pContext->pStreams[StreamIndex])
    {
        NvMMBlockDestroyStream(hBlock, StreamIndex);
    }

    // - Instanciate NvMMStream struct
    if ((pStream = NvOsAlloc(sizeof(NvMMStream))) == NULL)
    {
        status = NvError_InsufficientMemory;
        goto NvMMBlockCreateStream_err;
    }
    NvOsMemset(pStream, 0, sizeof(NvMMStream));

    pStream->StreamIndex = StreamIndex;
    pStream->Direction = Direction;
    pStream->OutgoingStreamIndex = 0xffffffffUL;
    pStream->bEnableBufferNegotiation = NV_TRUE;

    // - Create the stream buffer Qs, stream buffer headers
    if ((status = NvMMQueueCreate(&pStream->BufQ, MaxBufferCount, sizeof(NvMMBuffer*), NV_TRUE)) != NvSuccess)
        goto NvMMBlockCreateStream_err;

    for (j=0; j<(int)MaxBufferCount; j++)
    {
        if ((pStream->pBuf[j] = NvOsAlloc(sizeof(NvMMBuffer))) == NULL)
        {
            status = NvError_InsufficientMemory;
            goto NvMMBlockCreateStream_err;
        }
        NvOsMemset(pStream->pBuf[j], 0, sizeof(NvMMBuffer));
    }

    pContext->pStreams[StreamIndex] = pStream;
    pContext->StreamCount++;
    return NvSuccess;

NvMMBlockCreateStream_err:
    DestroyStream(pStream);
    return status;
}

static void DestroyStream(NvMMStream* pStream)
{
    int j;
    if (pStream)
    {
        for (j=NVMMSTREAM_MAXBUFFERS-1; j>=0; --j)
        {
            if(pStream->pBuf[j] != NULL)
                NvOsFree(pStream->pBuf[j]);
        }

        NvMMQueueDestroy(&pStream->BufQ); 

        NvOsFree(pStream);
    }
}

void NvMMBlockDestroyStream(NvMMBlockHandle hBlock, NvU32 StreamIndex)
{
    NvMMBlockContext* const pContext = hBlock->pContext;

    if (pContext->StreamCount)
    {
        if (pContext->pStreams[StreamIndex])
        {
            DestroyStream(pContext->pStreams[StreamIndex]);
            pContext->pStreams[StreamIndex] = NULL;
            pContext->StreamCount--;
        }
    }
}

NvError NvMMBlockAllocateBuffers(NvMMBlockHandle hBlock, NvMMStream* pStream)
{
    NvError status = NvSuccess;
    NvMMBlockContext* const pContext = hBlock->pContext;
    NvMMBuffer *pBuffer;
    NvU32 i;

    //
    // Negotiated configuration
    //
    NvMMNewBufferConfigurationInfo* pConfig = &pStream->BufConfig;


    //
    // check if buffer negotiation finished
    //
    if (!pConfig->structSize)
        return NvError_InvalidState;

    pStream->NumBuffers = pConfig->numBuffers;

    NV_DEBUG_PRINTF(("BlockType = %x\t StreamIndex = %x\tBufferSize = %x\tNumBuffers = %x\n", pContext->BlockType, pStream->StreamIndex, pConfig->bufferSize, pConfig->numBuffers));

    //
    // initialize each buffer
    //
    for (i = 0; i < pStream->NumBuffers; i++)
    {

        if (pConfig->formatId != NvMMBufferFormatId_Video)
        {

            status = NvMMUtilAllocateBuffer(pContext->hRmDevice,
                pConfig->bufferSize,
                pConfig->byteAlignment,
                pConfig->memorySpace,
                (pConfig->bInSharedMemory || (pContext->params.Locale == NvMMLocale_AVP)) ? NV_TRUE : NV_FALSE,
                &pStream->pBuf[i]);
        }
        else
        {
            status = NvMMUtilAllocateVideoBuffer(pContext->hRmDevice,
                pConfig->format.videoFormat,
                &pStream->pBuf[i]);
        }

        pBuffer = pStream->pBuf[i];

        if (status != NvSuccess)
        {
            NV_DEBUG_PRINTF(("NvMMBlockAllocateBuffers failed"));

            // Clean up
            while(i)
            {
                if (pConfig->formatId != NvMMBufferFormatId_Video)
                {
                    NvMMUtilDeallocateBlockSideBuffer(pStream->pBuf[--i], 
                                                      pContext->params.hService,
                                                      NV_FALSE);
                }
                else
                {
                    NvMMUtilDeallocateVideoBuffer(pStream->pBuf[--i]);
                }
            }
            return NvError_InsufficientMemory;
        }
        NV_DEBUG_PRINTF(("NvMMBlockAllocateBuffers::%d\t%x\n", pStream->StreamIndex, pBuffer->Payload.Ref.hMem));
        pBuffer->BufferID = i;
    }
    pStream->bBuffersAllocated = NV_TRUE;

    //
    // queue empty buffers
    //
    for (i = 0; i < pStream->NumBuffers; i++)
    {
        pBuffer = pStream->pBuf[i];

        /* Queue empty output buffers for internal use */
        status = NvMMQueueEnQ(pStream->BufQ, &pStream->pBuf[i], 0);
        NV_ASSERT(status == NvSuccess);
    }
    return status;
}

void NvMMBlockDeallocateBuffers(NvMMBlockHandle hBlock, NvU32 StreamIndex)
{
    NvMMBlockContext* const pContext = hBlock->pContext;
    NvMMStream* pStream;
    NvMMNewBufferConfigurationInfo* pConfig;
    NvU32 i;
#if NV_DEBUG
    NvError status;
#endif

    NV_ASSERT(StreamIndex < pContext->StreamCount);
    pStream = pContext->pStreams[StreamIndex];
    NV_ASSERT(pStream);

    NV_ASSERT(pStream->bBlockIsAllocator);
    pConfig = &pStream->BufConfig;
    //
    // Delete the buffers we had allocated
    //
    for (i = 0; i < pStream->NumBuffers; i++)
    {
        if (pStream->pBuf[i])
        {
            if (pConfig->formatId != NvMMBufferFormatId_Video)
            {
#if NV_DEBUG
                status =
#endif
                NvMMUtilDeallocateBlockSideBuffer(pStream->pBuf[i],
                                                  pContext->params.hService,
                                                  NV_FALSE);
            }
            else
            {
#if NV_DEBUG
                status =
#endif
                NvMMUtilDeallocateVideoBuffer(pStream->pBuf[i]);
                pStream->pBuf[i] = NULL;
            }
            NV_ASSERT(status == NvSuccess);
        }
    }
    pStream->bBuffersAllocated = NV_FALSE;
}

NvError NvMMBlockReturnBuffersToAllocator(NvMMBlockHandle hBlock, NvU32 StreamIndex)
{
    NvError status = NvSuccess;
    NvMMBlockContext* const pContext = hBlock->pContext;
    NvMMStream* pStream;
    NvMMBuffer* pBuffer;
    NvU32 i, uQueueEntries;

    NV_ASSERT(StreamIndex < pContext->StreamCount);
    pStream = pContext->pStreams[StreamIndex];
    NV_ASSERT(pStream);

    NV_ASSERT(!pStream->bBlockIsAllocator);

    if (pStream->NumBuffers == 0)
        return NvSuccess;

    //
    // Return all pending buffers to other side
    //
    uQueueEntries = NvMMQueueGetNumEntries(pStream->BufQ);
    for (i = 0; i < uQueueEntries; i++)
    {
        status = NvMMQueueDeQ(pStream->BufQ, &pBuffer);
        NV_ASSERT(status == NvSuccess);

        // Clear any data left
        if ((pBuffer->PayloadType == NvMMPayloadType_MemHandle) ||
            (pBuffer->PayloadType == NvMMPayloadType_MemPointer))
        {
            pBuffer->Payload.Ref.sizeOfValidDataInBytes = 0;
        }

        if(pStream->TransferBufferFromBlock)
        {
            status = pStream->TransferBufferFromBlock(pStream->pOutgoingTBFContext, 
                pStream->OutgoingStreamIndex, 
                NvMMBufferType_Payload,
                sizeof(NvMMBuffer),
                pBuffer);
            NV_ASSERT(status == NvSuccess);
        }
    }

    return status;
}

NvBool NvMMBlockAreAllBuffersDeleted(NvMMBlockHandle hBlock, NvU32 StreamIndex)
{
    NvMMBlockContext* const pContext = hBlock->pContext;
    NvMMStream* pStream;
    NvU32 i;

    NV_ASSERT(StreamIndex < pContext->StreamCount);
    pStream = pContext->pStreams[StreamIndex];
    NV_ASSERT(pStream);

    NV_ASSERT(pStream->bBlockIsAllocator);

    for (i = 0; i < pStream->NumBuffers; i++)
    {
        if (pStream->pBuf[i])
            return NV_FALSE;
    }

    return NV_TRUE;
}

NvBool NvMMBlockAreAllBuffersReturned(NvMMBlockHandle hBlock, NvU32 StreamIndex)
{
    NvMMBlockContext* const pContext = hBlock->pContext;
    NvMMStream* pStream;
    NvU32 uBufferCount;

    NV_ASSERT(StreamIndex < pContext->StreamCount);
    pStream = pContext->pStreams[StreamIndex];
    NV_ASSERT(pStream);

    NV_ASSERT(!pStream->bBlockIsAllocator);

    uBufferCount = NvMMQueueGetNumEntries(pStream->BufQ);

    return (uBufferCount == 0) ? NV_TRUE : NV_FALSE;
}

static NvError NvMMSendBlockEvent( NvMMBlockContext* pContext, NvU32 EventType, NvU32 EventInfoSize,  void *pEventInfo)
{
    NvError status   = NvSuccess;

    if(pContext->bSendBlockEventNotifications)
    {
        if((pContext->SendEvent) && (pContext->bAbnormalTermination == NV_FALSE))
        {
            status = pContext->SendEvent(pContext->pSendEventContext,
                EventType,
                EventInfoSize,
                pEventInfo);
        }
    }

    return status;
}

/* NvMMSendDataToMixer
* This routine is called to send data to the mixer.
* It is a generic routine and should not contain any variables that are codec specific.
* All codecs should use this routine to send data to the Mixer module.
*/
NvError NvMMSendDataToMixer
(
 NvMMBuffer *pNextOutBuf,
 NvMMStream *pOutStream,
 NvU32 rate,
 NvU32 entriesOutput,
 NvU32 sampleRate,
 NvU32 numChannels,
 NvU32 bitsPerSample,
 NvS32 *pAdjSampleRate,
 NvBool deQueueBuffer
 )
{
    NvError e = NvSuccess;
    NvS32 adjsampleRate = 0;

    if (pOutStream->TransferBufferFromBlock && sampleRate > 0 &&  numChannels > 0)
    {
        NvMMAudioMetadata *audioMeta = &(pNextOutBuf->PayloadInfo.BufferMetaData.AudioMetadata);

        pNextOutBuf->PayloadInfo.BufferMetaDataType = NvMMBufferMetadataType_Audio;
        audioMeta->nSampleRate = sampleRate;
        audioMeta->nChannels = numChannels;
        audioMeta->nBitsPerSample = bitsPerSample;

        NV_DEBUG_PRINTF(("MP3 send payload of size: %d\n", pNextOutBuf->Payload.Ref.sizeOfValidDataInBytes));

        // Pop next output buffer
        if(deQueueBuffer)
        {
            e = NvMMQueueDeQ(pOutStream->BufQ, &pNextOutBuf);
            NV_ASSERT((e == NvSuccess) && pNextOutBuf);
            NV_ASSERT((pNextOutBuf->PayloadType == NvMMPayloadType_MemHandle) ||
                (pNextOutBuf->PayloadType == NvMMPayloadType_MemPointer));
        }

        pOutStream->Position += pNextOutBuf->Payload.Ref.sizeOfValidDataInBytes;

        e = pOutStream->TransferBufferFromBlock(pOutStream->pOutgoingTBFContext,
            pOutStream->OutgoingStreamIndex,
            NvMMBufferType_Payload,
            sizeof(NvMMBuffer),
            pNextOutBuf);
        NV_ASSERT(e == NvSuccess);

        pNextOutBuf->Payload.Ref.sizeOfValidDataInBytes = 0;
    }

    if(pAdjSampleRate)
    {
        *pAdjSampleRate = adjsampleRate;
    }
    return e;
}


#if NV_IS_AVP
#pragma arm section code, rwdata, rodata, zidata
#endif

