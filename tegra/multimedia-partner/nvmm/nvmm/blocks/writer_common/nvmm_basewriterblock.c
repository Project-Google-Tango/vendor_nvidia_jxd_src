/*
 * Copyright (c) 2008-2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvmm_basewriterblock.h"
#include "nvmm_event.h"
#include "nvmm_queue.h"
#include "nvmm_debug.h"
#include "nvmm_3gpwriterblock.h"
#include "nvutil.h"

#define NVLOG_CLIENT NVLOG_NVMM_BASEWRITER

typedef struct NvMMBaseWriterContextRec
{
    NvMMBlockContext Block; //!< NvMMBlock base class, must be first member to allow down-casting
    NvBool WriterInitialized;
    char FileExt[MAX_FILE_EXT];
    NvU32 TotalStreams;
    NvMMWriterStreamConfig StreamConfig;
    NvmmWriterContext WriterContext;
    NvmmCoreWriterOps CoreWriterOps;
}NvMMBaseWriterContext;


static NvError
NvMMBaseWriterBlockSendEvent(
    NvMMBlockContext BlockContext,
    NvMMEventType EventType,
    NvError RetType,
    NvU32 StreamIndex)
{
    NvError Status = NvSuccess;
    NvMMBlockErrorInfo BlockError;
    NvMMEventBlockStreamEndInfo BlockStreamEndInfo;
    NvU32 EventInfoSize = 0;
    void *pEventInfo = NULL;

    NvOsMemset(&BlockError,0,sizeof(NvMMBlockErrorInfo));
    NvOsMemset(&BlockStreamEndInfo,0,sizeof(NvMMEventBlockStreamEndInfo));

   if(NvMMEvent_BlockStreamEnd == EventType)
   {
       BlockStreamEndInfo.event = NvMMEvent_BlockStreamEnd;
       BlockStreamEndInfo.streamIndex = StreamIndex;
       BlockStreamEndInfo.structSize = sizeof(NvMMEventBlockStreamEndInfo);
       EventInfoSize = sizeof(NvMMEventBlockStreamEndInfo);
       pEventInfo = &BlockStreamEndInfo;
    }
   else if(NvMMEvent_BlockError == EventType)
   {
       BlockError.structSize = sizeof(NvMMBlockErrorInfo);
       BlockError.event = NvMMEvent_BlockError;
       BlockError.error = RetType;
       EventInfoSize = sizeof(NvMMBlockErrorInfo);
       pEventInfo = &BlockError;
    }
    NVMM_CHK_ERR(BlockContext.SendEvent(
        BlockContext.pSendEventContext,
        EventType,
        EventInfoSize,
        pEventInfo));

cleanup:
   return Status;
}

static NvError
NvMMBaseWriterBlockSetAttribute(
    NvMMBlockHandle BlockHandle,
    NvU32 AttributeType,
    NvU32 SetAttrFlag,
    NvU32 AttributeSize,
    void *pAttribute)
{
    NvError Status = NvSuccess;
    NvMMBaseWriterContext * BaseWriterContext = NULL;
    char *FilePath = NULL;
    NvU32 i = 0;
    NvU32 FileExtLen = 0;
    static NvBool ConfigPending = NV_FALSE;
    NvBool NoFileExtFound = NV_FALSE;

    NVMM_CHK_ARG(BlockHandle && BlockHandle->pContext && pAttribute);
    BaseWriterContext = BlockHandle->pContext;

    NV_LOGGER_PRINT((NVLOG_NVMM_BASEWRITER, NVLOG_DEBUG,"++NvMMBaseWriterBlockSetAttribute"));

    switch(AttributeType)
    {
        case NvMMWriterAttribute_FileName:
        {

            if(!BaseWriterContext->CoreWriterOps.OpenWriter)
            {
                FilePath = ( (NvMMWriterAttrib_FileName*)pAttribute)->szURI +
                    NvOsStrlen(( (NvMMWriterAttrib_FileName*)pAttribute)->szURI);
                while(*FilePath != '.')
                {
                    if(FilePath == ( (NvMMWriterAttrib_FileName*)pAttribute)->szURI)
                    {
                        NoFileExtFound = NV_TRUE;
                        break;
                    }
                    FilePath--;
                }
                if(NoFileExtFound == NV_TRUE)
                    NvOsMemcpy(BaseWriterContext->FileExt, "mp4", NvOsStrlen("mp4") + 1);
                else
                {
                    FilePath++;
                    NvOsMemcpy(BaseWriterContext->FileExt, FilePath, NvOsStrlen(FilePath) + 1);
                }
                FileExtLen = NvOsStrlen(BaseWriterContext->FileExt);
                if(FileExtLen <= MAX_FILE_EXT)
                {
                    for(i=0; i < FileExtLen ; i++)
                        BaseWriterContext->FileExt[i] = NV_TO_LOWER(BaseWriterContext->FileExt[i]);

                    if (!NvOsStrcmp(BaseWriterContext->FileExt, "mp4") ||
                        !NvOsStrcmp(BaseWriterContext->FileExt, "3gp") ||
                        !NvOsStrcmp(BaseWriterContext->FileExt, "3gpp") ||
                        !NvOsStrcmp(BaseWriterContext->FileExt, "m4v") ||
                        !NvOsStrcmp(BaseWriterContext->FileExt, "mov") ||
                        !NvOsStrcmp(BaseWriterContext->FileExt, "3g2") ||
                        !NvOsStrcmp(BaseWriterContext->FileExt, "qt"))
                    {
                        NVMM_CHK_ERR(NvMMCreate3gpWriterContext(
                            &BaseWriterContext->CoreWriterOps,
                            &BaseWriterContext->WriterContext));
                    }
                    else
                        NVMM_CHK_ARG(0);
                    //Add other writers here

                    NVMM_CHK_ERR(BaseWriterContext->CoreWriterOps.OpenWriter(
                        BaseWriterContext->WriterContext) );
                    if(NV_TRUE ==ConfigPending )
                    {
                        NVMM_CHK_ERR(BaseWriterContext->CoreWriterOps.SetAttribute(
                            BaseWriterContext->WriterContext,NvMMWriterAttribute_StreamConfig,
                            (void *)&BaseWriterContext->StreamConfig));
                        ConfigPending = NV_FALSE;
                     }
                }
            }
        }
        break;
        case NvMMWriterAttribute_StreamCount:
        {
            // Streams related Inits
            BaseWriterContext->TotalStreams =  ( (NvMMWriterAttrib_StreamCount*)
             pAttribute)->streamCount;

            for (i=0; i<BaseWriterContext->TotalStreams; i++)
            {
                NVMM_CHK_ERR(NvMMBlockCreateStream(
                    BlockHandle,
                    i,
                    NvMMStreamDir_In,
                    NVMMSTREAM_MAXBUFFERS));
            }
        }
        break;
        case NvMMWriterAttribute_StreamConfig:
        {
            NvOsMemcpy(&BaseWriterContext->StreamConfig, pAttribute,
                sizeof(NvMMWriterStreamConfig) );
            ConfigPending = NV_TRUE;
        }
        break;
        default:
        // Handle unknown attribute types in writer base class
        break;
    }

    if(BaseWriterContext->CoreWriterOps.SetAttribute)
    {
        if(!( (AttributeType == NvMMWriterAttribute_FileName) && !NvOsStrcmp("mp4.mp4",
            ((NvMMWriterAttrib_FileName*)pAttribute)->szURI)) )
            NVMM_CHK_ERR(BaseWriterContext->CoreWriterOps.SetAttribute(
                BaseWriterContext->WriterContext,AttributeType,pAttribute));
    }

cleanup:
    // Call base class implementation
    //  for already handled AttributeType: to handle SetAttrFlag
    //  for not yet handled AttributeType: possibly base class attribute
        if(!( (AttributeType == NvMMWriterAttribute_FileName) && !NvOsStrcmp("mp4.mp4",
            ((NvMMWriterAttrib_FileName*)pAttribute)->szURI)) )
        (void)NvMMBlockSetAttribute(
               BlockHandle,
               AttributeType,
               SetAttrFlag,
               AttributeSize,
            pAttribute);

    if(Status != NvSuccess)
    {
        (void)NvMMBaseWriterBlockSendEvent(
            BaseWriterContext->Block,
            NvMMEvent_BlockError,
            Status,
            0);
    }
    NV_LOGGER_PRINT((NVLOG_NVMM_BASEWRITER, NVLOG_DEBUG, "--NvMMBaseWriterBlockSetAttribute"));
    NV_LOGGER_PRINT((NVLOG_NVMM_BASEWRITER, NVLOG_ERROR, "--NvMMBaseWriterBlockSetAttribute ->%x",Status));
    return Status;
}

static NvError
NvMMBaseWriterBlockGetAttribute(
    NvMMBlockHandle BlockHandle,
    NvU32 AttributeType,
    NvU32 AttributeSize,
    void *pAttribute)
{
     NvError Status = NvSuccess;
    NvMMBaseWriterContext * BaseWriterContext = NULL;

    NV_LOGGER_PRINT((NVLOG_NVMM_BASEWRITER, NVLOG_DEBUG, "++NvMMBaseWriterBlockGetAttribute"));

    NVMM_CHK_ARG(BlockHandle && BlockHandle->pContext && pAttribute);
    BaseWriterContext = BlockHandle->pContext;

    if(NvMMWriterAttribute_FrameCount == AttributeType)
    {
        NVMM_CHK_ARG(BaseWriterContext->CoreWriterOps.GetAttribute);
        NVMM_CHK_ERR(BaseWriterContext->CoreWriterOps.GetAttribute(
            BaseWriterContext->WriterContext,
            AttributeType,
            pAttribute));
    }
    else
    {
        // Handle unknown attribute types in base class
        NVMM_CHK_ERR(NvMMBlockGetAttribute(
            BlockHandle,
            AttributeType,
            AttributeSize,
            pAttribute));
    }
cleanup:
    if(Status != NvSuccess)
    {
        NVMM_CHK_ERR(NvMMBaseWriterBlockSendEvent(
            BaseWriterContext->Block,
            NvMMEvent_BlockError,
            Status,
            0));
    }
    NV_LOGGER_PRINT((NVLOG_NVMM_BASEWRITER, NVLOG_DEBUG, "--NvMMBaseWriterBlockGetAttribute"));
    NV_LOGGER_PRINT((NVLOG_NVMM_BASEWRITER, NVLOG_ERROR, "--NvMMBaseWriterBlockGetAttribute ->%x",Status));
    return Status;
}

// Destructor for the block.
//First block implementation specific resources are freed, and
//then the NvMMBlock base class close function is called.

static void
NvMMBaseWriterBlockPrivateClose(
    NvMMBlockHandle BlockHandle)
{
    NvMMBaseWriterContext * BaseWriterContext = NULL;
    NvU32 i = 0;

    if(BlockHandle && BlockHandle->pContext)
    {
        BaseWriterContext = BlockHandle->pContext;
        for (i=0; i<BaseWriterContext->TotalStreams; i++)
            NvMMBlockDestroyStream(BlockHandle, i);

       if(BaseWriterContext->CoreWriterOps.CloseWriter)
       {
           BaseWriterContext->CoreWriterOps.CloseWriter(
               BaseWriterContext->WriterContext);
        }

       // call base class close function at the end, will free memory for handles
       NvMMBlockClose(BlockHandle);
   }
}

static NvError
NvMMBaseWriterBlockSendEmptyBuffer(
    NvMMStream *pInStream,
    NvMMBuffer *pNextInBuf)
{
    NvError Status = NvSuccess;

    NVMM_CHK_ARG(pInStream && pNextInBuf);

    pNextInBuf->Payload.Ref.sizeOfValidDataInBytes = 0;

    NVMM_CHK_ERR( pInStream->TransferBufferFromBlock(pInStream->pOutgoingTBFContext,
        pInStream->OutgoingStreamIndex,
        NvMMBufferType_Payload,
        sizeof(NvMMBuffer),
        pNextInBuf));

cleanup:
    return Status;
}

static NvBool
NvMMBaseWriterBlockGetBufferRequirements(
    NvMMBlockHandle BlockHandle,
    NvU32 StreamIndex,
    NvU32 Retry,
    NvMMNewBufferRequirementsInfo *pBufReq)
{
    NvError Status = NvSuccess;
    NvMMBaseWriterContext * BaseWriterContext = NULL;
    NvMMWriterAttrib_FileName FileParams;

    NV_LOGGER_PRINT((NVLOG_NVMM_BASEWRITER, NVLOG_DEBUG, "++NvMMBaseWriterBlockGetBufferRequirements"));
    NVMM_CHK_ARG(BlockHandle && BlockHandle->pContext && pBufReq);
    BaseWriterContext = BlockHandle->pContext;
//    NVMM_CHK_ARG(BaseWriterContext->CoreWriterOps.GetBufferRequirements);
    //This is a WAR as DSHOW is not setting the type of writer via. filename
    //before getting buff requirements
    NvOsMemset(&FileParams, 0, sizeof(NvMMWriterAttrib_FileName));
    FileParams.szURI = "mp4.mp4";
    NvMMBaseWriterBlockSetAttribute(BlockHandle,
        NvMMWriterAttribute_FileName,
        NvMMSetAttrFlag_Notification,
        NvOsStrlen(FileParams.szURI),
        &FileParams
        );

    if (Retry > 0)
        return NV_FALSE;

    NvOsMemset (pBufReq, 0, sizeof(NvMMNewBufferRequirementsInfo));

    pBufReq->structSize    = sizeof(NvMMNewBufferRequirementsInfo);
    pBufReq->event         = NvMMEvent_NewBufferRequirements;

    pBufReq->memorySpace   = NvMMMemoryType_SYSTEM;
    pBufReq->byteAlignment = 4;

    NVMM_CHK_ERR(BaseWriterContext->CoreWriterOps.GetBufferRequirements(
        BaseWriterContext->WriterContext,
        StreamIndex,
        &pBufReq->minBuffers,
        &pBufReq->maxBuffers,
        &pBufReq->minBufferSize,
        &pBufReq->maxBufferSize));

cleanup:
    NV_LOGGER_PRINT((NVLOG_NVMM_BASEWRITER, NVLOG_DEBUG, "--NvMMBaseWriterBlockGetBufferRequirements"));
    NV_LOGGER_PRINT((NVLOG_NVMM_BASEWRITER, NVLOG_ERROR, "--NvMMBaseWriterBlockGetBufferRequirements ->%x",Status));
    return ((Status != NvSuccess) ? NV_FALSE : NV_TRUE);
}

// This function tries to do process with the data.

static NvError
NvMMBaseWriterBlockDoWork(
    NvMMBlockHandle BlockHandle,
    NvMMDoWorkCondition DoWorkCondition)
{
    NvMMBaseWriterContext*  BaseWriterContext = NULL;
    NvError Status = NvSuccess;
    NvU32 i = 0;
    NvMMStream *pInStream = NULL;
    NvMMBuffer *pNextInBuf = NULL;
    NvU32 EntriesInput = 0;

    NVMM_CHK_ARG(BlockHandle && BlockHandle->pContext);
    BaseWriterContext = BlockHandle->pContext;

    if(BaseWriterContext->Block.State != NvMMState_Running)
        return NvSuccess;
    NVMM_CHK_ARG(BaseWriterContext->CoreWriterOps.ProcessRawBuffer);
    // Try to produce at most one buffer for each stream
    for (i=0; i < BaseWriterContext->TotalStreams; i++)
    {
        pInStream = BaseWriterContext->Block.pStreams[i];
        // Get number of pending input and output buffers
        EntriesInput = NvMMQueueGetNumEntries(pInStream->BufQ);
        // All input buffers are consumed or returned. End of Stream flag is set means,
        // no new buffers to come, so send stream end
        if (pInStream->bEndOfStream &&
            (!EntriesInput ||
            (pInStream->EndOfStreamBufferCount >= pInStream->NumBuffers)))
        {
            if (!pInStream->bEndOfStreamEventSent)
            {
                NVMM_CHK_ERR(NvMMBaseWriterBlockSendEvent(
                    BaseWriterContext->Block,
                    NvMMEvent_BlockStreamEnd,
                    0,
                    pInStream->StreamIndex));
                pInStream->bEndOfStreamEventSent = NV_TRUE;
            }
        }
        else
        {
            // Process while we have at least one full input  else go to next stream,
            if (EntriesInput)
            {
                // Pop next input buffer
                NVMM_CHK_ERR(NvMMQueueDeQ(pInStream->BufQ, &pNextInBuf));
                if (pNextInBuf->PayloadInfo.BufferFlags & NvMMBufferFlag_EndOfStream)
               {
                   // just put it back into the queue
                   NVMM_CHK_ERR(NvMMQueueEnQ(pInStream->BufQ, &pNextInBuf, 0));
                    pInStream->EndOfStreamBufferCount++;
               }
               else
               {
                   if(pNextInBuf->Payload.Ref.sizeOfValidDataInBytes > 0)
                   {
                       // Send stream bufffer to writer
                       // Here 2nd arg is streamIndex. Writer code should correctly identify the stream type
                       NVMM_CHK_ERR(BaseWriterContext->CoreWriterOps.ProcessRawBuffer(
                           BaseWriterContext->WriterContext,
                           /*streamIndex*/ i,
                           pNextInBuf));
                   }
                   // Return processed and now empty input buffer
                   NvMMBaseWriterBlockSendEmptyBuffer(pInStream,pNextInBuf);
                   EntriesInput = NvMMQueueGetNumEntries(pInStream->BufQ);
                   if(EntriesInput)
                       Status = NvError_Busy;
               }
            }
        }
    }
cleanup:
    if((Status != NvSuccess) && (Status != NvError_Busy))
    {
        NV_LOGGER_PRINT((NVLOG_NVMM_BASEWRITER, NVLOG_ERROR, "--NvMMBaseWriterBlockDoWork ->%x",Status));
        NVMM_CHK_ERR(NvMMBaseWriterBlockSendEvent(
            BaseWriterContext->Block,
            NvMMEvent_BlockError,
            Status,
            0));
        NvMMBaseWriterBlockSendEmptyBuffer(pInStream,pNextInBuf);
    }
    return Status;
}

NvError NvMMBaseWriterBlockOpen(
    NvMMBlockHandle *pBlockHandle,
    NvMMInternalCreationParameters *CreationParams,
    NvOsSemaphoreHandle BlockSemaHandle,
    NvMMDoWorkFunction *pDoWorkFunction)
{
   NvError Status = NvSuccess;
    NvMMBlock* pBaseBlockhandle = NULL;

    NV_LOGGER_PRINT((NVLOG_NVMM_BASEWRITER, NVLOG_DEBUG, "++NvMMBaseWriterBlockOpen"));

    NVMM_CHK_ARG(CreationParams);
    //No need to check other ARGS as they are yet to be populated

    // Call base class open function.
    // Allocates memory and populates NvMMBlock and our private context struct
    // with default values and function pointers for default implementations.
    NVMM_CHK_ERR( NvMMBlockOpen(&pBaseBlockhandle,
        sizeof(NvMMBaseWriterContext),
        CreationParams,
        BlockSemaHandle,
        NvMMBaseWriterBlockDoWork,
        NvMMBaseWriterBlockPrivateClose,
        (NvMMGetBufferRequirementsFunction )NvMMBaseWriterBlockGetBufferRequirements));

    // Override default methods
    pBaseBlockhandle->SetAttribute = NvMMBaseWriterBlockSetAttribute;
    pBaseBlockhandle->GetAttribute = NvMMBaseWriterBlockGetAttribute;

    // Populate block handle and work function
    //  move callback into NvMMBlock
    if (pDoWorkFunction)
        *pDoWorkFunction = NvMMBlockDoWork;

    *pBlockHandle = pBaseBlockhandle;

cleanup:
    if(Status != NvSuccess)
        NvMMBaseWriterBlockPrivateClose(pBaseBlockhandle);

    NV_LOGGER_PRINT((NVLOG_NVMM_BASEWRITER, NVLOG_DEBUG, "--NvMMBaseWriterBlockOpen"));
    return Status;
}

void
NvMMBaseWriterBlockClose(
    NvMMBlockHandle BlockHandle,
    NvMMInternalDestructionParameters *DestructionParams)
{
    NvMMBlockTryClose(BlockHandle);
}
