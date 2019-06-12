

/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvmm.h"
#include "nvmm_event.h"
#include "nvrm_transport.h"
#include "nvmm_block.h"
#include "nvmm_queue.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvmm_debug.h"
#include "nvmm_amrparser_core.h"
#include "nvmm_contentpipe.h"
#include "nvmm_amrparser.h"


#define READ_AMR_INPUT_BUFFER_SIZE         960
#define AMR_DEC_MAX_INPUT_BUFFER_SIZE       960
#define AMR_DEC_MIN_INPUT_BUFFER_SIZE       960

#define AWB_MAGIC_NUMBER        "#!AMR-WB\n"

/* Context for amr parser core */
typedef struct
{
    CPhandle cphandle;
    CP_PIPETYPE_EXTENDED *pPipe;
    NvAmrParser *pAmrReaderHandle;
    NvBool   bSentEOS;    
    NvU64 ParserTimeStamp;
    NvU64 RunningTimeStamp;
    NvMMAttrib_ParseRate rate;
    NvMMAttrib_ParsePosition position;
    NvU32 numberOfTracks;
    // Indicates file reached to EOF while seeking
    NvBool bFileReachedEOF;
} NvMMAmrParserCoreContext;


// Prototypes for Ogg implementation of parser functions
NvError AmrCoreParserGetNumberOfStreams(NvMMParserCoreHandle hParserCore, NvU32 *streams);
NvError AmrCoreParserGetTotalTrackTime(NvMMParserCoreHandle hParserCore, NvU64* TotalTime);
NvError AmrCoreParserGetStreamInfo(NvMMParserCoreHandle hParserCore, NvMMStreamInfo **pInfo);
NvError AmrCoreParserSetRate(NvMMParserCoreHandle hParserCore, NvS32 rate); 
NvS32   AmrCoreParserGetRate(NvMMParserCoreHandle hParserCore); 
NvError AmrCoreParserSetPosition(NvMMParserCoreHandle hParserCore, NvU64 *timestamp); 
NvError AmrCoreParserGetPosition( NvMMParserCoreHandle hParserCore, NvU64 *timestamp); 
NvError AmrCoreParserOpen(NvMMParserCoreHandle hParserCore, NvString pURI);
NvError AmrCoreParserClose(NvMMParserCoreHandle hParserCore);
void AmrCoreParserGetMaxOffsets(NvMMParserCoreHandle hParserCore, NvU32 *pMaxOffsets);

NvError 
AmrCoreParserGetAttribute(
    NvMMParserCoreHandle hParserCore,
    NvU32 AttributeType, 
    NvU32 AttributeSize, 
    void *pAttribute);

NvError 
AmrCoreParserGetNextWorkUnit(
    NvMMParserCoreHandle hParserCore, 
    NvU32 streamIndex, 
    NvMMBuffer *pBuffer, 
    NvU32 *size,
    NvBool *pMoreWorkPending);

NvBool  
AmrCoreParserGetBufferRequirements(
    NvMMParserCoreHandle hParserCore,
    NvU32 StreamIndex,
    NvU32 Retry,
    NvMMNewBufferRequirementsInfo *pBufReq);

NvError 
NvMMCreateAmrCoreParser(
    NvMMParserCoreHandle *hParserCore, 
    NvMMParserCoreCreationParameters *pParams)
{
    NvMMParserCore *pParserCore        = NULL; 

    if ((pParserCore = NvOsAlloc(sizeof(NvMMParserCore))) == NULL)
    {
        return NvError_InsufficientMemory;
    }
    NvOsMemset(pParserCore, 0, sizeof(NvMMParserCore));

    pParserCore->Open               = AmrCoreParserOpen;
    pParserCore->GetNumberOfStreams = AmrCoreParserGetNumberOfStreams;
    pParserCore->GetStreamInfo      = AmrCoreParserGetStreamInfo;
    pParserCore->SetRate            = AmrCoreParserSetRate;
    pParserCore->GetRate            = AmrCoreParserGetRate;
    pParserCore->SetPosition        = AmrCoreParserSetPosition;
    pParserCore->GetPosition        = AmrCoreParserGetPosition;
    pParserCore->GetNextWorkUnit    = AmrCoreParserGetNextWorkUnit;
    pParserCore->Close              = AmrCoreParserClose;
    pParserCore->GetMaxOffsets      = AmrCoreParserGetMaxOffsets;
    pParserCore->GetAttribute       = AmrCoreParserGetAttribute;
    pParserCore->GetBufferRequirements = AmrCoreParserGetBufferRequirements;
    pParserCore->eCoreType          = NvMMParserCoreType_AmrParser;
    pParserCore->UlpKpiMode         = pParams->UlpKpiMode;
    pParserCore->bEnableUlpMode     = pParams->bEnableUlpMode;
    pParserCore->bReduceVideoBuffers= pParams->bReduceVideoBuffers;

    *hParserCore = pParserCore;

    return NvSuccess;
    
}

void NvMMDestroyAmrCoreParser(NvMMParserCoreHandle hParserCore)
{
    if (hParserCore)
    {
        NvOsFree(hParserCore);
    }
}


NvError AmrCoreParserOpen(NvMMParserCoreHandle hParserCore, NvString pFilename)
{
    NvError Status = NvSuccess;
    NvMMAmrParserCoreContext *pContext = NULL;
    NvU32 startSize,endSize = 0;

    NV_LOGGER_PRINT((NVLOG_AMR_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));

    NVMM_CHK_ARG (hParserCore && pFilename);

    pContext = NvOsAlloc(sizeof(NvMMAmrParserCoreContext));
    NVMM_CHK_MEM(pContext);
    NvOsMemset(pContext, 0, sizeof(NvMMAmrParserCoreContext));

    if ( (hParserCore->MinCacheSize == 0) &&
        (hParserCore->MaxCacheSize == 0) &&
        (hParserCore->SpareCacheSize == 0))
    {
        hParserCore->bUsingCachedCP = NV_FALSE;
        hParserCore->UlpKpiMode     = NV_FALSE;
        hParserCore->bEnableUlpMode = NV_FALSE;
    }
    else
    {
        hParserCore->bUsingCachedCP = NV_TRUE;
    }

    // The below function need to use to initialize with CCP 
    NvmmGetFileContentPipe(&pContext->pPipe);

    NVMM_CHK_ERR (pContext->pPipe->cpipe.Open(&pContext->cphandle, pFilename,CP_AccessRead));

    pContext->pAmrReaderHandle = NvGetAmrReaderHandle(pContext->cphandle, pContext->pPipe);
    NVMM_CHK_MEM(pContext->pAmrReaderHandle);

    if (hParserCore->bUsingCachedCP)
    {
        NVMM_CHK_ERR (pContext->pPipe->InitializeCP (pContext->cphandle,
            hParserCore->MinCacheSize,
            hParserCore->MaxCacheSize,
            hParserCore->SpareCacheSize,
            (CPuint64 *) &hParserCore->pActualCacheSize));
    }
    if (hParserCore->bUsingCachedCP)
    {
        NVMM_CHK_ERR(pContext->pAmrReaderHandle->pPipe->GetSize(
                                      pContext->cphandle, &pContext->pAmrReaderHandle->FileSize));
    }
    else
    {
        NVMM_CHK_ERR( pContext->pAmrReaderHandle->pPipe->cpipe.SetPosition
                                                  (pContext->cphandle, 0, CP_OriginBegin));

        NVMM_CHK_ERR(pContext->pAmrReaderHandle->pPipe->cpipe.GetPosition
                                                     (pContext->cphandle, (CPuint*)&startSize));

        NVMM_CHK_ERR(pContext->pAmrReaderHandle->pPipe->cpipe.SetPosition
                                                          (pContext->cphandle, 0, CP_OriginEnd));

        NVMM_CHK_ERR(pContext->pAmrReaderHandle->pPipe->cpipe.GetPosition
                                                         (pContext->cphandle, (CPuint*)&endSize));

        pContext->pAmrReaderHandle->FileSize = endSize - startSize;

        NVMM_CHK_ERR( pContext->pAmrReaderHandle->pPipe->cpipe.SetPosition
                                                  (pContext->cphandle, 0, CP_OriginBegin));
    }

    NVMM_CHK_ERR (NvParseAmrAwbTrackInfo(pContext->pAmrReaderHandle));
    pContext->RunningTimeStamp = 200000; //20 msec frame * 100 nSec

    hParserCore->pContext = (void*)pContext;    
    pContext->rate.Rate  = 1000;
    hParserCore->pPipe    = pContext->pPipe;
    hParserCore->cphandle = pContext->cphandle;

cleanup:
    if (Status != NvSuccess)
    {
        (void)AmrCoreParserClose(hParserCore);
    }
    NV_LOGGER_PRINT((NVLOG_AMR_PARSER, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));    
    return Status;
}

NvError AmrCoreParserGetNumberOfStreams(NvMMParserCoreHandle hParserCore, NvU32 *streams)
{    
    NvError status = NvSuccess;
    if(hParserCore && hParserCore->pContext && streams)
    {
        NvMMAmrParserCoreContext *pContext = (NvMMAmrParserCoreContext *)hParserCore->pContext;
        if(pContext->pAmrReaderHandle)
        {
            *streams =   pContext->pAmrReaderHandle->numberOfTracks;
        }
        else
        {
            status = NvError_InvalidAddress;
        }
    }
    else
    {
        status = NvError_InvalidAddress;
    }
    return status;
}

NvError AmrCoreParserGetStreamInfo(NvMMParserCoreHandle hParserCore, NvMMStreamInfo **pInfo)
{        
    NvError status = NvSuccess;
    NvS32 i,TotalTracks;
    NvU64 TotalTrackDuration = 0;
    NvMMAmrParserCoreContext *pContext = (NvMMAmrParserCoreContext *)hParserCore->pContext;

    TotalTracks = pContext->pAmrReaderHandle->numberOfTracks;
    AmrCoreParserGetTotalTrackTime(hParserCore, &TotalTrackDuration);

    for (i = 0; i < TotalTracks; i++)
    {
        if (pContext->pAmrReaderHandle->pInfo->pTrackInfo->streamType == NvStreamTypeAMRNB) 
        {
            pInfo[i]->StreamType = NvMMStreamType_NAMR;
        }
        else 
        {
            pInfo[i]->StreamType = NvMMStreamType_WAMR;
        }          
        pInfo[i]->BlockType                                 = NvMMBlockType_SuperParser;
        pInfo[i]->TotalTime                                 = TotalTrackDuration;
        pInfo[i]->NvMMStream_Props.AudioProps.SampleRate    = pContext->pAmrReaderHandle->pInfo->pTrackInfo->samplingFreq;
        pInfo[i]->NvMMStream_Props.AudioProps.BitRate       = pContext->pAmrReaderHandle->pInfo->pTrackInfo->bitRate;
        pInfo[i]->NvMMStream_Props.AudioProps.NChannels     = pContext->pAmrReaderHandle->pInfo->pTrackInfo->noOfChannels;
        pInfo[i]->NvMMStream_Props.AudioProps.BitsPerSample = pContext->pAmrReaderHandle->pInfo->pTrackInfo->sampleSize; 
    } 

    return status;
} 

NvError AmrCoreParserGetTotalTrackTime(NvMMParserCoreHandle hParserCore, NvU64* TotalTime)
{    
    NvError status = NvSuccess;
    NvMMAmrParserCoreContext *pContext = (NvMMAmrParserCoreContext *)hParserCore->pContext;

    *TotalTime = pContext->pAmrReaderHandle->pInfo->pTrackInfo->total_time;
    
    return status;    
}

NvError AmrCoreParserSetRate(NvMMParserCoreHandle hParserCore, NvS32 rate)
{ 
    NvError status = NvSuccess;
    NvMMAmrParserCoreContext *pContext = (NvMMAmrParserCoreContext *)hParserCore->pContext;

    pContext->rate.Rate = rate;   
    
    return status;
}

NvS32 AmrCoreParserGetRate(NvMMParserCoreHandle hParserCore)
{ 
    NvMMAmrParserCoreContext *pContext = (NvMMAmrParserCoreContext *)hParserCore->pContext;
    return (pContext->rate.Rate);
}

NvError AmrCoreParserSetPosition(NvMMParserCoreHandle hParserCore, NvU64 *timestamp)
{ 
    NvError status = NvSuccess;    
    NvS64 mSec;   
    NvU64 TotalTime = 0;
    NvMMAmrParserCoreContext *pContext = (NvMMAmrParserCoreContext *)hParserCore->pContext;

    mSec = *timestamp;

    AmrCoreParserGetTotalTrackTime(hParserCore, &TotalTime);
    
    if(*timestamp > TotalTime)
    {
        pContext->bFileReachedEOF = NV_TRUE;
        // Update the parse position with TotalTime of the track
        pContext->position.Position = TotalTime;
    }
    else if (*timestamp == 0)
    {
        // CEPlayer will call seek(0) for repeat mode hence reset the flag
        pContext->bFileReachedEOF = NV_FALSE;
    }

    if (pContext->pAmrReaderHandle->pInfo->pTrackInfo->streamType == NvStreamTypeAMRNB)
        status = NvAmrSeek(pContext->pAmrReaderHandle,mSec);
    else if(pContext->pAmrReaderHandle->pInfo->pTrackInfo->streamType == NvStreamTypeAMRWB)
        status = NvAwbSeek(pContext->pAmrReaderHandle,mSec);
    
    pContext->ParserTimeStamp = *timestamp;
    pContext->position.Position = *timestamp;
   
    return status;
}

NvError AmrCoreParserGetPosition( NvMMParserCoreHandle hParserCore, NvU64 *timestamp)
{
    NvError status = NvSuccess;
    NvMMAmrParserCoreContext *pContext = (NvMMAmrParserCoreContext *)hParserCore->pContext;
   
    *(timestamp) = pContext->position.Position;
    return status;
}

NvError 
AmrCoreParserGetNextWorkUnit(
    NvMMParserCoreHandle hParserCore, 
    NvU32 streamIndex, 
    NvMMBuffer *pBuffer, 
    NvU32 *size, 
    NvBool *pMoreWorkPending)
{ 
    NvError status = NvSuccess;
    NvU32 askBytes = 0;    
    NvU8 *pData = NULL;
    CP_CHECKBYTESRESULTTYPE eResult = CP_CheckBytesOk;   
    NvU32 bytesRead=0;
    CPuint64 askBytes64 = 0;
    NvMMAmrParserCoreContext *pContext = (NvMMAmrParserCoreContext *)hParserCore->pContext;

    // Initially pMoreWorkPending is set to FALSE
    *pMoreWorkPending = NV_FALSE;

    if (pContext->bFileReachedEOF)
    {
        return NvError_ParserEndOfStream;
    }

    if (pBuffer)
    {        
        pBuffer->Payload.Ref.startOfValidData = 0;
        pBuffer->Payload.Ref.sizeOfValidDataInBytes = *size;
      
        // Set ask bytes to the preset buffer size
        askBytes = READ_AMR_INPUT_BUFFER_SIZE;

         // If Total Readbytes is matched with FileSize it means EOS is set
        if (pContext->pAmrReaderHandle->TotalReadBytes >=  pContext->pAmrReaderHandle->FileSize)
            return NvError_ParserEndOfStream;
         // Recalcualte the askBytes based upon avalaible bytes left in the file
        if ((pContext->pAmrReaderHandle->TotalReadBytes + askBytes)> pContext->pAmrReaderHandle->FileSize)
        {
            askBytes = (NvU32)(pContext->pAmrReaderHandle->FileSize - pContext->pAmrReaderHandle->TotalReadBytes);
        }
         if( pBuffer->Payload.Ref.sizeOfBufferInBytes < askBytes)
        {
            // returning required minimum size
            *size = askBytes;
            return NvError_InSufficientBufferSize;
        }    
                
        status = pContext->pPipe->cpipe.CheckAvailableBytes(pContext->cphandle, askBytes, &eResult);   
        // Check if we reached end of stream.
        if (eResult == CP_CheckBytesAtEndOfStream)
        {
            return NvError_ParserEndOfStream;
        }
        else if (eResult == CP_CheckBytesNotReady)
        {
            return NvError_ContentPipeNotReady;
        }
        if (eResult == CP_CheckBytesOk)
        {      
                pData = (NvU8 *)pBuffer->Payload.Ref.pMem;
                
                if (pContext->pAmrReaderHandle->pInfo->pTrackInfo->streamType == NvStreamTypeAMRNB)
                    status = NvAmrGetNextFrame(pContext->pAmrReaderHandle,pData,askBytes,&bytesRead);   
                else if(pContext->pAmrReaderHandle->pInfo->pTrackInfo->streamType == NvStreamTypeAMRWB)
                    status = NvAwbGetNextFrame(pContext->pAmrReaderHandle,pData,askBytes,&bytesRead);   
                
                if(status != NvSuccess)
                {
                    return status;
                }

                if(bytesRead != askBytes)
                    askBytes = bytesRead;
                

                pBuffer->Payload.Ref.startOfValidData= 0;
                pBuffer->Payload.Ref.sizeOfValidDataInBytes= askBytes;

                pContext->ParserTimeStamp +=  pContext->RunningTimeStamp;
                pBuffer->PayloadInfo.TimeStamp = pContext->ParserTimeStamp;   
           
            *size = askBytes;
        }

        if (eResult == CP_CheckBytesInsufficientBytes)
        {   
            status = pContext->pPipe->GetAvailableBytes(pContext->cphandle, &askBytes64);
            if (status != NvSuccess) return NvError_ParserFailure;
            NV_ASSERT(askBytes64 <= (CPuint64)0xffffffff);
            askBytes = (NvU32)askBytes64;
            
            pData = (NvU8 *)pBuffer->Payload.Ref.pMem;
            
            if (pContext->pAmrReaderHandle->pInfo->pTrackInfo->streamType == NvStreamTypeAMRNB)
                status = NvAmrGetNextFrame(pContext->pAmrReaderHandle,pData,askBytes,&bytesRead);   
            else if(pContext->pAmrReaderHandle->pInfo->pTrackInfo->streamType == NvStreamTypeAMRWB)
                status = NvAwbGetNextFrame(pContext->pAmrReaderHandle,pData,askBytes,&bytesRead);   
            
            if(status != NvSuccess)
            {
                return status;
            }

            if(bytesRead != askBytes)
            {
                askBytes = bytesRead;
            }

            pBuffer->Payload.Ref.startOfValidData = 0;
            pBuffer->Payload.Ref.sizeOfValidDataInBytes = askBytes;

            pContext->ParserTimeStamp +=  pContext->RunningTimeStamp;
            pBuffer->PayloadInfo.TimeStamp = pContext->ParserTimeStamp;   
            
            pContext->bSentEOS = NV_TRUE;
            *size = askBytes;                     
        }
    }      
    return status;
}

NvError AmrCoreParserClose(NvMMParserCoreHandle hParserCore)
{
    NvError Status = NvSuccess;
    NvMMAmrParserCoreContext *pContext = NULL;

    NV_LOGGER_PRINT((NVLOG_AMR_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));

    NVMM_CHK_ARG(hParserCore && hParserCore->pContext);

    pContext = (NvMMAmrParserCoreContext *)hParserCore->pContext;

    if (pContext->pAmrReaderHandle)
    {
        NvReleaseAmrReaderHandle(pContext->pAmrReaderHandle);
    }

    if (pContext->cphandle)
    {
        Status = pContext->pPipe->cpipe.Close(pContext->cphandle);
    }


cleanup:
    NvOsFree(pContext);

    hParserCore->pContext = NULL;

    return Status;
}

NvBool  
AmrCoreParserGetBufferRequirements(
    NvMMParserCoreHandle hParserCore,
    NvU32 StreamIndex,
    NvU32 Retry,
    NvMMNewBufferRequirementsInfo *pBufReq)
{  
    if (Retry > 0)
    {
        return NV_FALSE;
    }

    NvOsMemset (pBufReq, 0, sizeof(NvMMNewBufferRequirementsInfo));

    pBufReq->structSize    = sizeof(NvMMNewBufferRequirementsInfo);
    pBufReq->event         = NvMMEvent_NewBufferRequirements;
    pBufReq->minBuffers    = 4;
    pBufReq->maxBuffers    = NVMMSTREAM_MAXBUFFERS;
    pBufReq->memorySpace   = NvMMMemoryType_SYSTEM;
    pBufReq->byteAlignment = 4;     
    
    pBufReq->minBufferSize = AMR_DEC_MIN_INPUT_BUFFER_SIZE;
    pBufReq->maxBufferSize = AMR_DEC_MAX_INPUT_BUFFER_SIZE;     
    
    return NV_TRUE;
}



void AmrCoreParserGetMaxOffsets(NvMMParserCoreHandle hParserCore, NvU32 *pMaxOffsets)
{
    *pMaxOffsets = 0;
}

NvError 
AmrCoreParserGetAttribute(
    NvMMParserCoreHandle hParserCore,
    NvU32 AttributeType, 
    NvU32 AttributeSize, 
    void *pAttribute)
{
    NvError status = NvSuccess;
    return status;
}




























