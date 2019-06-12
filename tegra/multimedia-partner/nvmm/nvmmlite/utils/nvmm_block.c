/*
* Copyright (c) 2007-2014 NVIDIA Corporation.  All rights reserved.
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

#include "nvmmlite.h"
#include "nvmmlite_event.h"
#include "nvmmlite_block.h"
#include "nvassert.h"
#include "nvmmlite_util.h"
#include "nvmm_logger.h"
#include "nvmmlite_common.h"

#define NVLOG_CLIENT NVLOG_NVMMLITE_TRANSPORT

static void    DestroyStream(NvMMLiteStream* pStream);

NvError NvMMLiteBlockOpen(NvMMLiteBlockHandle *phBlock,
                      NvU32 SizeOfBlockContext,
                      NvMMLiteInternalCreationParameters* pParams,
                      NvMMLiteBlockDoWorkFunction DoWorkFunction,
                      NvMMLitePrivateCloseFunction PrivateCloseFunction,
                      NvMMLiteGetBufferRequirementsFunction GetBufferRequirements)
{
    NvError           Status = NvSuccess;
    NvMMLiteBlock*        pBlock   = NULL; /* MM block referenced by handle */
    NvMMLiteBlockContext* pContext = NULL; /* Context of this instance of reference block */

    NVMMLITE_CHK_ARG(pParams);
    //
    // Init NvMMLiteBlock and NvMMLiteBlockContext structs with base class defaults
    //
    pBlock = NvOsAlloc(sizeof(NvMMLiteBlock));
    NVMMLITE_CHK_MEM(pBlock);
    NvOsMemset(pBlock, 0, sizeof(NvMMLiteBlock));

    pContext = NvOsAlloc(SizeOfBlockContext);
    NVMMLITE_CHK_MEM(pContext);
    NvOsMemset(pContext, 0, SizeOfBlockContext);
    pBlock->pContext = pContext;

    pContext->pStreams = NvOsAlloc(sizeof(NvMMLiteStream*) * NVMMLITEBLOCK_MAXSTREAMS);
    NVMMLITE_CHK_MEM(pContext->pStreams);
    NvOsMemset(pContext->pStreams, 0, (sizeof(NvMMLiteStream*) * NVMMLITEBLOCK_MAXSTREAMS));

    pBlock->StructSize                = sizeof(NvMMLiteBlock);

    pBlock->SetTransferBufferFunction = NvMMLiteBlockSetTransferBufferFunction;
    pBlock->TransferBufferToBlock     = NvMMLiteBlockTransferBufferToBlock;
    pBlock->SetSendBlockEventFunction = NvMMLiteBlockSetSendBlockEventFunction;
    pBlock->SetState                  = NvMMLiteBlockSetState;
    pBlock->GetState                  = NvMMLiteBlockGetState;
    pBlock->AbortBuffers              = NvMMLiteBlockAbortBuffers;
    pBlock->SetAttribute              = NvMMLiteBlockSetAttribute;
    pBlock->GetAttribute              = NvMMLiteBlockGetAttribute;
    pBlock->Close                     = NvMMLiteBlockTryClose;

    NvOsMemcpy(&pContext->params, pParams, sizeof(NvMMLiteInternalCreationParameters));

    pContext->State                   = NvMMLiteState_Stopped;
    pContext->DoWork                  = DoWorkFunction;
    pContext->PrivateClose            = PrivateCloseFunction;
    pContext->GetBufferRequirements   = GetBufferRequirements;
    pContext->bSendStreamEventNotifications = NV_TRUE;
    pContext->bSendBlockEventNotifications =  NV_TRUE;

    NVMMLITE_CHK_ERR(NvRmOpen(&pContext->hRmDevice, 0));

    NVMMLITE_CHK_ERR(NvOsMutexCreate(&pContext->hBlockMutex));
    NVMMLITE_CHK_ERR(NvOsMutexCreate(&pContext->hBlockCloseMutex));


    *phBlock = pBlock;
    NV_DEBUG_PRINTF(("NvMMLiteBlockOpen: hBlock 0x%X \n", pBlock));

cleanup:
    if (Status != NvSuccess)
    {
        NvMMLiteBlockClose(pBlock);
    }
    return Status;
}

void NvMMLiteBlockClose(NvMMLiteBlockHandle hBlock)
{
    if (hBlock)
    {
        NvMMLiteBlockContext* const pContext = hBlock->pContext;
        NV_DEBUG_PRINTF(("NvMMLiteBlockClose: hBlock 0x%X \n", hBlock));
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

void NvMMLiteBlockTryClose(NvMMLiteBlockHandle hBlock)
{
    NvMMLiteBlockContext* const pContext = hBlock->pContext;
    NvU32 StreamIndex;
    NvBool bInResult = NV_TRUE;

    NvOsMutexLock(pContext->hBlockCloseMutex);
    NvOsMutexLock(pContext->hBlockMutex);

    NV_DEBUG_PRINTF(("NvMMLiteBlockTryClose: hBlock 0x%X \n", hBlock));

    /* return buffers if we are not allocator , free them if we are allocator */
    for (StreamIndex=0; StreamIndex < pContext->StreamCount; StreamIndex++)
    {
        NvMMLiteStream* pStream = pContext->pStreams[StreamIndex];

        if (pStream)
        {
            NvMMLiteBlockReturnBuffersToAllocator(hBlock, StreamIndex);
            bInResult &= NvMMLiteBlockAreAllBuffersReturned(hBlock, StreamIndex);

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


NvError NvMMLiteBlockSetState(NvMMLiteBlockHandle hBlock, NvMMLiteState eState)
{
    NvError status = NvSuccess;
    unsigned int i;
    NvMMLiteBlockContext* const pContext = hBlock->pContext;

    pContext->State = eState;

    if (eState == NvMMLiteState_Stopped)
    {
        // If stopped, rewind all streams
        for (i=0; i < pContext->StreamCount; i++)
        {
            NvMMLiteStream* const pStream = pContext->pStreams[i];
            if (pStream)
                pStream->Position = 0;
        }
    }

    return status;
}

NvError NvMMLiteBlockGetState(NvMMLiteBlockHandle hBlock, NvMMLiteState *peState)
{
    NvMMLiteBlockContext* const pContext = hBlock->pContext;
    *peState = pContext->State;
    return NvSuccess;
}

NvError NvMMLiteBlockSetAttribute(NvMMLiteBlockHandle hBlock, NvU32 AttributeType, NvU32 SetAttrFlag, NvU32 AttributeSize, void *pAttribute)
{
    NvError status = NvSuccess;
    NvMMLiteBlockContext* const pContext = hBlock->pContext;

    switch(AttributeType)
    {
    case NvMMLiteAttribute_PowerState:
        NV_ASSERT(AttributeSize == sizeof(NvMMLiteAttrib_PowerStateInfo));
        pContext->PowerState = ((NvMMLiteAttrib_PowerStateInfo*)pAttribute)->PowerState;
        break;
    case NvMMLiteAttribute_EventNotificationMode:
        NV_ASSERT(AttributeSize == sizeof(NvMMLiteAttrib_EventNotificationInfo));
        pContext->bSendStreamEventNotifications = ((NvMMLiteAttrib_EventNotificationInfo*)pAttribute)->bStreamEventNotification;
        pContext->bSendBlockEventNotifications = ((NvMMLiteAttrib_EventNotificationInfo*)pAttribute)->bBlockEventNotification;
        break;
    }
    return status;
}

NvError NvMMLiteBlockGetAttribute(NvMMLiteBlockHandle hBlock, NvU32 AttributeType, NvU32 AttributeSize, void* pAttribute)
{
    NvError status = NvSuccess;
    NvMMLiteBlockContext* const pContext = hBlock->pContext;

    switch (AttributeType)
    {
    case NvMMLiteAttribute_StreamPosition:
        {
            NvMMLiteAttrib_StreamPosition* pPos = (NvMMLiteAttrib_StreamPosition*) pAttribute;
            NvMMLiteStream* pStream = NvMMLiteBlockGetStream(pContext, pPos->StreamIndex);

            NV_ASSERT(AttributeSize == sizeof(NvMMLiteAttrib_StreamPosition));

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

    case NvMMLiteAttribute_GetStreamInfo:
        {
            NvMMLiteAttrib_StreamInfo *pStreamInfo = (NvMMLiteAttrib_StreamInfo*)pAttribute;
            NvMMLiteStream* pStream = NvMMLiteBlockGetStream(pContext, pStreamInfo->StreamIndex);

            NV_ASSERT(AttributeSize == sizeof(NvMMLiteAttrib_StreamInfo));

            if (pStream)
            {
                pStreamInfo->StreamIndex = pStream->StreamIndex;
                pStreamInfo->Direction = pStream->Direction;
            }
            else
            {
                NV_ASSERT(NV_FALSE); // bad stream index
            }
        }
        break;

    case NvMMLiteAttribute_GetBlockInfo:
        {
            NvMMLiteAttrib_BlockInfo *bInfo = (NvMMLiteAttrib_BlockInfo*)pAttribute;
            NvU32 i;
            NvU32 StreamCount = 0;
            NvMMLiteStream *pTempStream;
            NV_ASSERT(AttributeSize     == sizeof(NvMMLiteAttrib_BlockInfo));
            for(i = 0; i < pContext->StreamCount; i++)
            {
                pTempStream = pContext->pStreams[i];
                if(pTempStream->TransferBufferFromBlock)
                    StreamCount++;
            }
            bInfo->NumStreams           = StreamCount;
        }
        break;

    default:
        NV_DEBUG_PRINTF(("NvMMLiteBlockGetAttribute: type 0x%X (%d) unknown\n", AttributeType, AttributeType));
        break;
    }

    return status;
}

void NvMMLiteBlockSetSendBlockEventFunction(NvMMLiteBlockHandle hBlock, void *pClientContext, LiteSendBlockEventFunction SendEvent)
{
    NvMMLiteBlockContext* const pContext= hBlock->pContext;
    pContext->SendEvent = SendEvent;
    pContext->pSendEventContext = pClientContext;
}

NvError NvMMLiteBlockSetTransferBufferFunction(NvMMLiteBlockHandle hBlock,
                                           NvU32 StreamIndex,
                                           NvMMLiteTransferBufferFunction TransferBuffer,
                                           void* pContextForTranferBuffer,
                                           NvU32 StreamIndexForTransferBuffer)
{
    NvError status;
    NvMMLiteBlockContext* const pContext = hBlock->pContext;
    NvMMLiteStream* pStream;
    NvMMLiteNewBufferRequirementsInfo bufReq;

    NV_ASSERT(StreamIndex < pContext->StreamCount);
    pStream = pContext->pStreams[StreamIndex];
    NV_ASSERT(pStream);

    //
    // Apply new transfer buffer function
    //
    pStream->TransferBufferFromBlock = TransferBuffer;
    pStream->pOutgoingTBFContext     = pContextForTranferBuffer;
    pStream->OutgoingStreamIndex     = StreamIndexForTransferBuffer;
    pStream->bTBFSet = NV_TRUE;

    //
    // Initiate the negotiation by sending initial buffer requirements (retry count 0)
    //
    pContext->GetBufferRequirements(hBlock, StreamIndex, 0, &bufReq); // get reqs from block implementation

    NV_ASSERT((bufReq.structSize == sizeof(NvMMLiteNewBufferRequirementsInfo)) &&
        (bufReq.event == NvMMLiteEvent_NewBufferRequirements));

    // FIXME: Convert this to a query
    status = TransferBuffer(pContextForTranferBuffer,
        StreamIndexForTransferBuffer,
        NvMMBufferType_StreamEvent,
        sizeof(NvMMLiteNewBufferRequirementsInfo),
        &bufReq);
    NV_ASSERT(status == NvSuccess);
    return status;
}

NvError NvMMLiteBlockAbortBuffers(NvMMLiteBlockHandle hBlock, NvU32 StreamIndex)
{
    NvMMLiteBlockContext* const pContext = hBlock->pContext;
    NvError status = NvSuccess;
    NvU32 i, QueueEntries;
    NvMMLiteStream *pStream;
    NvMMBuffer *pBuffer;

    NV_DEBUG_PRINTF(("NvMMLiteBlockAbortBuffers::In hBlock 0x%X\t StreamIndex = %x\n", hBlock, StreamIndex));

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
            NvMMLiteEventDRMPlayCntInfo DRMPlayCntInfo;
            DRMPlayCntInfo.structSize  = sizeof(NvMMLiteEventDRMPlayCntInfo);
            DRMPlayCntInfo.event       = NvMMLiteEvent_DRMPlayCount;
            DRMPlayCntInfo.drmContext  = 0;
            DRMPlayCntInfo.bDoMetering = 0;
            NV_LOGGER_PRINT((NVLOG_WMDRM, NVLOG_DEBUG, "Found DRM Context in NvMMLiteBlockAbortBuffers"));
            status = pStream->TransferBufferFromBlock(pStream->pOutgoingTBFContext,
                pStream->OutgoingStreamIndex,
                NvMMBufferType_StreamEvent,
                sizeof(NvMMLiteEventDRMPlayCntInfo),
                &DRMPlayCntInfo);
        }

        if (pBuffer->PayloadType != NvMMPayloadType_SurfaceArray)
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

    return status;
}

NvError NvMMLiteBlockTransferBufferToBlock(void *hBlock, NvU32 StreamIndex, NvMMBufferType BufferType, NvU32 BufferSize, void* pBuffer)
{
    NvError                 status = NvSuccess;
    NvMMLiteBlockContext* const pContext = ((NvMMLiteBlock*)hBlock)->pContext;
    NvMMLiteStream *            pStream;
    NvU32                   BufferID, EventType;
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
            EventType = (NvU32)(((NvMMLiteStreamEventBase*)pBuffer)->event);
            switch (EventType)
            {

            case NvMMLiteEvent_ResetStreamEnd:
                NvOsMutexLock(pContext->hBlockMutex);
                pStream->bEndOfStream = NV_FALSE;
                pStream->bEndOfStreamEventSent = NV_FALSE;
                NvOsMutexUnlock(pContext->hBlockMutex);
                if(pStream->Direction == NvMMLiteStreamDir_In)
                {
                    if(pContext->pStreams[1])
                    {
                        if(pContext->pStreams[1]->TransferBufferFromBlock)
                        {
                            status = pContext->pStreams[1]->TransferBufferFromBlock(pContext->pStreams[1]->pOutgoingTBFContext,
                                pContext->pStreams[1]->OutgoingStreamIndex,
                                NvMMBufferType_StreamEvent,
                                sizeof(NvMMLiteEventStreamEndInfo),
                                (NvMMLiteEventStreamEndInfo*)pBuffer);
                        }
                    }
                }
                break;

            case NvMMLiteEvent_StreamEnd:
                if(pStream->Direction == NvMMLiteStreamDir_In)
                {
                    pStream->bEndOfStream = NV_TRUE;
                    NV_LOGGER_PRINT((NVLOG_TRACK_LIST, NVLOG_INFO, "Dir - In: EOS Received on %x block", pContext->BlockType));
                }
                else // From Mixer
                {
                    NV_LOGGER_PRINT((NVLOG_TRACK_LIST, NVLOG_INFO, "Dir - Out: EOS Sending from %x block to Stream zero", pContext->BlockType));
                    NvMMLiteBlockSendEOS(hBlock, pContext->pStreams[0]);
                }
                break;

            case NvMMLiteEvent_VideoStreamInit:
                NV_ASSERT(((NvMMLiteEventVideoStreamInitInfo*)pBuffer)->structSize == sizeof(NvMMLiteEventVideoStreamInitInfo));
                ((NvMMLiteBlock*)hBlock)->SetAttribute(hBlock, NvMMLiteEvent_VideoStreamInit, 0, ((NvMMLiteEventVideoStreamInitInfo*)pBuffer)->structSize, pBuffer);
                break;

            case NvMMLiteEvent_AudioStreamInit:
                NV_ASSERT(((NvMMLiteEventAudioStreamInitInfo*)pBuffer)->structSize == sizeof(NvMMLiteEventAudioStreamInitInfo));
                NV_LOGGER_PRINT((NVLOG_TRACK_LIST, NVLOG_DEBUG, "NvMMLiteEvent_AudioStreamInit"));
                ((NvMMLiteBlock*)hBlock)->SetAttribute(hBlock, NvMMLiteEvent_AudioStreamInit, 0, ((NvMMLiteEventAudioStreamInitInfo*)pBuffer)->structSize, pBuffer);
                break;

            case NvMMLiteEvent_DRMPlayCount:
                if (pContext->NvMMLiteBlockTBTBCallBack)
                {
                    NvMMBuffer offsetListBuffer;
                    NvOsMemset(&offsetListBuffer, 0,sizeof(NvMMBuffer) );
                    offsetListBuffer.PayloadInfo.BufferFlags |= NvMMBufferFlag_DRMPlayCount;
                    offsetListBuffer.BufferID = ((NvMMLiteEventDRMPlayCntInfo*)pBuffer)->bDoMetering;
                    pContext->NvMMLiteBlockTBTBCallBack(pContext, ((NvMMLiteEventDRMPlayCntInfo*)pBuffer)->drmContext,&offsetListBuffer);
                }
                break;
            default:
                break;
            }
        }
    }
    else if (BufferType == NvMMBufferType_Payload) /* this call transferred a buffer */
    {
        NvOsMutexLock(pContext->hBlockMutex);

        // Copy incoming buffer header
        BufferID = ((NvMMBuffer*)pBuffer)->BufferID;
        *pStream->pBuf[BufferID] = *(NvMMBuffer*)pBuffer; //xxx fix this: BufferID must not be used as index

        // Mark buffer as beyound end of stream
        if (pStream->bEndOfStream)
        {
            pStream->pBuf[BufferID]->PayloadInfo.BufferFlags |= NvMMBufferFlag_EndOfStream;
        }

        // If the stream is output, and the received buffer carries payload, it is an returned empty buffer,
        // and the payload size must be 0 (see NvMMLiteDesign.doc, 1.1.2.5 2) )
        // This was previously an assert, but just set it to 0 here.
        if (pStream->Direction == NvMMLiteStreamDir_Out &&
            (((NvMMBuffer*)pBuffer)->PayloadType == NvMMPayloadType_MemHandle ||
            ((NvMMBuffer*)pBuffer)->PayloadType == NvMMPayloadType_MemPointer))
        {
            ((NvMMBuffer*)pBuffer)->Payload.Ref.sizeOfValidDataInBytes = 0;
        }

        if (pContext->NvMMLiteBlockTBTBCallBack)
        {
            pContext->NvMMLiteBlockTBTBCallBack(pContext, StreamIndex, pStream->pBuf[BufferID]);
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

    return status;
}

NvError NvMMLiteBlockSendEOS(NvMMLiteBlockHandle hBlock, NvMMLiteStream* pStream)
{
    NvError status = NvSuccess;
    NvMMLiteEventStreamEndInfo streamEndEvent;

    if (pStream->TransferBufferFromBlock)
    {
        streamEndEvent.structSize = sizeof(NvMMLiteEventStreamEndInfo);
        streamEndEvent.event = NvMMLiteEvent_StreamEnd;

        status = pStream->TransferBufferFromBlock(pStream->pOutgoingTBFContext,
            pStream->OutgoingStreamIndex,
            NvMMBufferType_StreamEvent,
            sizeof(NvMMLiteEventStreamEndInfo),
            &streamEndEvent);
    }

    pStream->bEndOfStreamEventSent = NV_TRUE;
    return status;
}

NvError NvMMLiteBlockDoWork(NvMMLiteBlockHandle hBlock, NvMMLiteDoWorkCondition condition, NvBool *pMoreWorkPending)
{
    NvError status = NvSuccess;
    NvMMLiteBlockContext* const pContext = hBlock->pContext;

    //NvOsMutexLock(pContext->hBlockMutex);
    if (!pContext->bBlockProcessingData)
    {
        pContext->bBlockProcessingData = NV_TRUE; // Prevent reentrancy
        if (pContext->PowerState == NvMMLitePowerState_FullOn)
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

NvError NvMMLiteBlockCreateStream(NvMMLiteBlockHandle hBlock, NvU32 StreamIndex, NvMMLiteStreamDir Direction)
{
    NvMMLiteBlockContext* const pContext = hBlock->pContext;
    NvError status;
    NvMMLiteStream* pStream = NULL;
    int j;

    NV_ASSERT(StreamIndex < NVMMLITEBLOCK_MAXSTREAMS); // sanity check

    // If the stream already exist, delete it.
    if (pContext->pStreams[StreamIndex])
    {
        NvMMLiteBlockDestroyStream(hBlock, StreamIndex);
    }

    // - Instanciate NvMMLiteStream struct
    if ((pStream = NvOsAlloc(sizeof(NvMMLiteStream))) == NULL)
    {
        status = NvError_InsufficientMemory;
        goto NvMMLiteBlockCreateStream_err;
    }
    NvOsMemset(pStream, 0, sizeof(NvMMLiteStream));

    pStream->StreamIndex = StreamIndex;
    pStream->Direction = Direction;
    pStream->OutgoingStreamIndex = 0xffffffffUL;

    // - Create the stream buffer Qs, stream buffer headers
    if ((status = NvMMQueueCreate(&pStream->BufQ, NVMMLITESTREAM_MAXBUFFERS,
                                  sizeof(NvMMBuffer*), NV_TRUE)) != NvSuccess)
        goto NvMMLiteBlockCreateStream_err;

    for (j = 0; j < NVMMLITESTREAM_MAXBUFFERS; j++)
    {
        if ((pStream->pBuf[j] = NvOsAlloc(sizeof(NvMMBuffer))) == NULL)
        {
            status = NvError_InsufficientMemory;
            goto NvMMLiteBlockCreateStream_err;
        }
        NvOsMemset(pStream->pBuf[j], 0, sizeof(NvMMBuffer));
    }

    pContext->pStreams[StreamIndex] = pStream;
    pContext->StreamCount++;
    return NvSuccess;

NvMMLiteBlockCreateStream_err:
    DestroyStream(pStream);
    return status;
}

static void DestroyStream(NvMMLiteStream* pStream)
{
    int j;
    if (pStream)
    {
        for (j=NVMMLITESTREAM_MAXBUFFERS-1; j>=0; --j)
        {
            NvOsFree(pStream->pBuf[j]);
        }

        NvMMQueueDestroy(&pStream->BufQ);

        NvOsFree(pStream);
    }
}

void NvMMLiteBlockDestroyStream(NvMMLiteBlockHandle hBlock, NvU32 StreamIndex)
{
    NvMMLiteBlockContext* const pContext = hBlock->pContext;

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

NvError NvMMLiteBlockReturnBuffersToAllocator(NvMMLiteBlockHandle hBlock, NvU32 StreamIndex)
{
    NvError status = NvSuccess;
    NvMMLiteBlockContext* const pContext = hBlock->pContext;
    NvMMLiteStream* pStream;
    NvMMBuffer* pBuffer;
    NvU32 i, uQueueEntries;


    NV_ASSERT(StreamIndex < pContext->StreamCount);
    pStream = pContext->pStreams[StreamIndex];
    NV_ASSERT(pStream);

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

NvBool NvMMLiteBlockAreAllBuffersReturned(NvMMLiteBlockHandle hBlock, NvU32 StreamIndex)
{
    NvMMLiteBlockContext* const pContext = hBlock->pContext;
    NvMMLiteStream* pStream;
    NvU32 uBufferCount;

    NV_ASSERT(StreamIndex < pContext->StreamCount);
    pStream = pContext->pStreams[StreamIndex];
    NV_ASSERT(pStream);

    uBufferCount = NvMMQueueGetNumEntries(pStream->BufQ);

    return (uBufferCount == 0) ? NV_TRUE : NV_FALSE;
}

/* NvMMLiteSendDataToMixer
* This routine is called to send data to the mixer.
* It is a generic routine and should not contain any variables that are codec specific.
* All codecs should use this routine to send data to the Mixer module.
*/
NvError NvMMLiteSendDataToMixer
(
 NvMMBuffer *pNextOutBuf,
 NvMMLiteStream *pOutStream,
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

