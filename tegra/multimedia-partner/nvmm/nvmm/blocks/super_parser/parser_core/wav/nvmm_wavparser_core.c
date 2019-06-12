/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvassert.h"
#include "nvos.h"
#include "nvrm_transport.h"
#include "nvrm_hardware_access.h"
#include "nvmm.h"
#include "nvmm_event.h"
#include "nvmm_block.h"
#include "nvmm_queue.h"
#include "nvmm_debug.h"
#include "nvmm_util.h"
#include "nvmm_parser_core.h"
#include "nvmm_wavparser.h"
#include "nvmm_wavparser_core.h"
#include "nvmm_ulp_util.h"
#include "nvmm_ulp_kpi_logger.h"
#include "nvmm_contentpipe.h"



#define WAV_DEC_MAX_INPUT_BUFFER_SIZE      8192
#define WAV_DEC_MIN_INPUT_BUFFER_SIZE       8192
#define READ_BUFFER_SIZE 8192


/* Context for Wav parser core */
typedef struct NvMMWavParserCoreContextRec
{
    /* Members for wav core parser */
    NvString szFilename;
    CPhandle cphandle;
    CP_PIPETYPE_EXTENDED*pPipe;
    NvWavParser *pWavReaderHandle;
    NvBool   bSentEOS;    
    NvU64 ParserTimeStamp;
    NvU64 timeStamp;
    NvMMAttrib_ParseRate rate;
    NvMMAttrib_ParsePosition position;
    NvU32 numberOfTracks;
    NvBool bFileReachedEOF;
} NvMMWavParserCoreContext;



/* prototypes for Wav implementation of parser functions */
NvError WavCoreParserGetNumberOfStreams(NvMMParserCoreHandle hParserCore, NvU32 *streams);
NvError WavCoreParserGetTotalTrackTime(NvMMParserCoreHandle hParserCore, NvU64* TotalTime);
NvError WavCoreParserGetStreamInfo(NvMMParserCoreHandle hParserCore, NvMMStreamInfo **pInfo);
NvError WavCoreParserSetRate(NvMMParserCoreHandle hParserCore, NvS32 rate);
NvS32   WavCoreParserGetRate(NvMMParserCoreHandle hParserCore);
NvError WavCoreParserSetPosition(NvMMParserCoreHandle hParserCore, NvU64 *timestamp);
NvError WavCoreParserGetPosition(NvMMParserCoreHandle hParserCore, NvU64 *timestamp);
void    WavCoreParserGetMaxOffsets(NvMMParserCoreHandle hParserCore, NvU32 *pMaxOffsets);
NvError WavCoreParserOpen(NvMMParserCoreHandle hParserCore, NvString pURI);
NvError WavCoreParserClose(NvMMParserCoreHandle hParserCore);

NvError 
WavCoreParserGetNextWorkUnit(
    NvMMParserCoreHandle hParserCore, 
    NvU32 streamIndex, 
    NvMMBuffer *pBuffer,
    NvU32 *size, 
    NvBool *pMoreWorkPending);


NvBool 
WavCoreParserGetBufferRequirements(
    NvMMParserCoreHandle hParserCore,
    NvU32 StreamIndex,
    NvU32 Retry,
    NvMMNewBufferRequirementsInfo *pBufReq);

NvError 
WavCoreParserGetAttribute(
    NvMMParserCoreHandle hParserCore, 
    NvU32 AttributeType, 
    NvU32 AttributeSize, 
    void *pAttribute);

NvError 
NvMMCreateWavCoreParser(
    NvMMParserCoreHandle *hParserCore, 
    NvMMParserCoreCreationParameters *pParams)
{

    NvError status = NvSuccess;
    NvMMParserCore *pParserCore = NULL; 

    if ((pParserCore = NvOsAlloc(sizeof(NvMMParserCore))) == NULL)
    {
        status = NvError_InsufficientMemory;
        goto NvMMWavParserCoreOpen_err;
    }
    NvOsMemset(pParserCore, 0, sizeof(NvMMParserCore));

    pParserCore->Open               = WavCoreParserOpen;
    pParserCore->GetNumberOfStreams = WavCoreParserGetNumberOfStreams;
    pParserCore->GetStreamInfo      = WavCoreParserGetStreamInfo;
    pParserCore->SetRate            = WavCoreParserSetRate;
    pParserCore->GetRate            = WavCoreParserGetRate;
    pParserCore->SetPosition        = WavCoreParserSetPosition;
    pParserCore->GetPosition        = WavCoreParserGetPosition;
    pParserCore->GetNextWorkUnit    = WavCoreParserGetNextWorkUnit;
    pParserCore->Close              = WavCoreParserClose;
    pParserCore->GetMaxOffsets      = WavCoreParserGetMaxOffsets;
    pParserCore->GetAttribute       = WavCoreParserGetAttribute;
    pParserCore->GetBufferRequirements = WavCoreParserGetBufferRequirements;
    pParserCore->eCoreType          = NvMMParserCoreType_WavParser;
    pParserCore->UlpKpiMode         = pParams->UlpKpiMode;
    pParserCore->bEnableUlpMode     = pParams->bEnableUlpMode;
    pParserCore->bReduceVideoBuffers= pParams->bReduceVideoBuffers;

NvMMWavParserCoreOpen_err:
    *hParserCore = pParserCore;
    return status;
}

void NvMMDestroyWavCoreParser(NvMMParserCoreHandle hParserCore)
{
     if (hParserCore)
    {
        NvOsFree(hParserCore);
    }
}


NvError WavCoreParserOpen(NvMMParserCoreHandle hParserCore, NvString pFilename)
{
  
    NvError status = NvSuccess;
    NvU32 StringLength = 0;
    NvU32 streams;
    NvMMWavParserCoreContext *pContext;  
    
    if ((pContext = NvOsAlloc(sizeof(NvMMWavParserCoreContext))) == NULL)
    {
        status = NvError_InsufficientMemory;
        goto NvMMWavParserOpen_err;
    }
    NvOsMemset(pContext, 0, sizeof(NvMMWavParserCoreContext));
    hParserCore->pContext = (void*)pContext;

    StringLength = NvOsStrlen(pFilename) + 1;
    pContext->szFilename = NvOsAlloc(StringLength);
    if (pContext->szFilename == NULL)
    {
        status = NvError_InsufficientMemory;
        goto NvMMWavParserOpen_err;
    }
    NvOsStrncpy(pContext->szFilename, pFilename, StringLength);
    if((hParserCore->MinCacheSize == 0) && (hParserCore->MaxCacheSize == 0) && (hParserCore->SpareCacheSize == 0))
    {
        hParserCore->bUsingCachedCP = NV_FALSE;   
    }
    else
    {
        hParserCore->bUsingCachedCP = NV_TRUE;
    }   

    NvmmGetFileContentPipe(&pContext->pPipe);
    
    if (pContext->pPipe->cpipe.Open(&pContext->cphandle, pContext->szFilename,CP_AccessRead) != NvSuccess)
    {
        status = NvError_ParserOpenFailure;
        goto NvMMWavParserOpen_err;
    }

    if (hParserCore->bUsingCachedCP)
    {
        status =  pContext->pPipe->InitializeCP(pContext->cphandle, 
                                               hParserCore->MinCacheSize,
                                               hParserCore->MaxCacheSize,
                                               hParserCore->SpareCacheSize,
                                               (CPuint64 *)&hParserCore->pActualCacheSize);
        if (status != NvSuccess)
        {
            goto NvMMWavParserOpen_err;
        }
    }

    pContext->pWavReaderHandle = NvGetWavReaderHandle(pContext->cphandle, pContext->pPipe);
    if (pContext->pWavReaderHandle == NULL)
    {
        status = NvError_InsufficientMemory;
        goto NvMMWavParserOpen_err;
    }

    status = NvParseWav(pContext->pWavReaderHandle);
    if(status)
    {
        goto  NvMMWavParserOpen_err;
    }
    
    pContext->rate.Rate  = 1000;

    WavCoreParserGetNumberOfStreams(hParserCore, &streams);
    pContext->timeStamp = 0;
    pContext->ParserTimeStamp =0;

    hParserCore->pPipe    = pContext->pPipe;
    hParserCore->cphandle = pContext->cphandle;
    hParserCore->pContext = (void*)pContext;

    return status;     

NvMMWavParserOpen_err:
    if(pContext)
    {
        if(pContext->szFilename)
        {
            NvOsFree(pContext->szFilename);
            pContext->szFilename = NULL;
        }
        if (pContext->pWavReaderHandle)
        {
            NvReleaseWavReaderHandle(pContext->pWavReaderHandle);
            NvOsFree(pContext->pWavReaderHandle);
            pContext->pWavReaderHandle = NULL;
        }

        if(pContext->cphandle)
        {
            if( pContext->pPipe->cpipe.Close(pContext->cphandle) != NvSuccess)
            {
                status = NvError_ParserCloseFailure;
            }
            pContext->cphandle = 0;
        }    
        NvOsFree(pContext);
        hParserCore->pContext = NULL;
    }
    
    return status;   
}

NvError WavCoreParserGetNumberOfStreams(NvMMParserCoreHandle hParserCore, NvU32 *streams)
{
    NvError status = NvSuccess;
    NvMMWavParserCoreContext *pContext = (NvMMWavParserCoreContext *)hParserCore->pContext;
    if (pContext == NULL)
     {
         return NvError_ParserFailure;
     }

    *streams = pContext->pWavReaderHandle->numberOfTracks;
    return status;     
    
}

NvError WavCoreParserGetTotalTrackTime(NvMMParserCoreHandle hParserCore, NvU64* TotalTime)
{

    NvError status = NvSuccess;
    NvMMWavParserCoreContext *pContext = (NvMMWavParserCoreContext *)hParserCore->pContext;
    
    if (pContext == NULL)
     {
         return NvError_ParserFailure;
     }
    
    if(pContext->pWavReaderHandle != NULL)
      *TotalTime = pContext->pWavReaderHandle->total_time;
     else
        return NvError_ParserFailure;

    return status;    
    
}

NvError WavCoreParserGetStreamInfo(NvMMParserCoreHandle hParserCore, NvMMStreamInfo **pInfo)
{

    NvError status = NvSuccess;
    NvS32 i,TotalTracks;
    NvU64 TotalTrackTime = 0;
    NvU64 TotalTrackDuration = 0;
    NvMMWavParserCoreContext *pContext = (NvMMWavParserCoreContext *)hParserCore->pContext;

    if (pContext == NULL)
    {
        return NvError_ParserFailure;
    }

    if(pContext->pWavReaderHandle != NULL)
    {
        TotalTracks = pContext->pWavReaderHandle->numberOfTracks;
        WavCoreParserGetTotalTrackTime(hParserCore, &TotalTrackTime);
        TotalTrackDuration =((NvU64)TotalTrackTime * 10000000);
        
        for (i = 0; i < TotalTracks; i++)
        {
            pInfo[i]->StreamType                                = NvMMStreamType_WAV;
            pInfo[i]->BlockType                                 = NvMMBlockType_SuperParser;
            pInfo[i]->TotalTime                                 = TotalTrackDuration;            
            pInfo[i]->NvMMStream_Props.AudioProps.SampleRate    = pContext->pWavReaderHandle->pInfo->pTrackInfo->samplingFreq;
            pInfo[i]->NvMMStream_Props.AudioProps.BitRate       = pContext->pWavReaderHandle->bitRate;
            pInfo[i]->NvMMStream_Props.AudioProps.NChannels     = pContext->pWavReaderHandle->pInfo->pTrackInfo->noOfChannels;
            pInfo[i]->NvMMStream_Props.AudioProps.BitsPerSample = pContext->pWavReaderHandle->pInfo->pTrackInfo->sampleSize;
        }
    }
    else
    {
        return NvError_ParserFailure;
    }
    return status;

}

NvError WavCoreParserSetRate(NvMMParserCoreHandle hParserCore, NvS32 rate)
{
    NvError status = NvSuccess;
    NvMMWavParserCoreContext *pContext = (NvMMWavParserCoreContext *)hParserCore->pContext;
    if (pContext == NULL)
     {
         return NvError_ParserFailure;
     }

    pContext->rate.Rate = rate;   
    return status;
}

NvS32 WavCoreParserGetRate(NvMMParserCoreHandle hParserCore)
{
    NvMMWavParserCoreContext *pContext = (NvMMWavParserCoreContext *)hParserCore->pContext;
    if (pContext == NULL)
     {
         return NvError_ParserFailure;
     }
    return (pContext->rate.Rate);
}

NvError WavCoreParserSetPosition(NvMMParserCoreHandle hParserCore, NvU64 *timestamp)
{
    NvError status = NvSuccess;
    NvS64 mSec;   
    NvU32 divFactor = 10000;
    NvU64 TotalTime = 0;
    NvU64 TotalTrackTime = 0;
    NvMMWavParserCoreContext *pContext = (NvMMWavParserCoreContext *)hParserCore->pContext;
    if (pContext == NULL)
     {
         return NvError_ParserFailure;
     }

    mSec = ((*timestamp)/divFactor); 

    WavCoreParserGetTotalTrackTime(hParserCore, &TotalTrackTime);
    TotalTime = ((NvU64)TotalTrackTime * 10000000);

    if(*timestamp > TotalTime)
    {
        return NvError_ParserEndOfStream;
    }
    else if (*timestamp == 0)
    {
        // CEPlayer will call seek(0) for repeat mode hence reset the flag
        pContext->bFileReachedEOF = NV_FALSE;
        pContext->pWavReaderHandle->bWavHeader = NV_TRUE;
    }

    status = NvWavSeekToTime(pContext->pWavReaderHandle,mSec);

    pContext->ParserTimeStamp = *timestamp;
    pContext->position.Position = *timestamp;

    return status;
}

NvError WavCoreParserGetPosition(NvMMParserCoreHandle hParserCore, NvU64 *timestamp)
{
    NvError status = NvSuccess;
    NvMMWavParserCoreContext *pContext = (NvMMWavParserCoreContext *)hParserCore->pContext;
    if (pContext == NULL)
        return NvError_ParserFailure;

    *(timestamp) = pContext->position.Position;
    return status;
}

NvError 
WavCoreParserGetNextWorkUnit(
    NvMMParserCoreHandle hParserCore, 
    NvU32 streamIndex, 
    NvMMBuffer *pBuffer, 
    NvU32 *size, 
    NvBool *pMoreWorkPending)
{
    
    NvError status = NvSuccess;
    NvU32 askBytes = 0;
    CP_CHECKBYTESRESULTTYPE eResult = CP_CheckBytesOk;
    NvMMWavParserCoreContext *pContext = (NvMMWavParserCoreContext *)hParserCore->pContext;
    CPuint64 askBytes64 = 0;
    NvWavTrackInfo *wTrackInfo = NULL;    
    
    if (pContext == NULL)
        return NvError_ParserFailure;

    if (pBuffer == NULL)
        return NvError_ParserFailure;

    wTrackInfo =(NvWavTrackInfo *) (pBuffer->Payload.Ref.pMem);

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
        askBytes = READ_BUFFER_SIZE;

        if( pBuffer->Payload.Ref.sizeOfBufferInBytes < askBytes)
        {
            // returning required minimum size
            *size = askBytes;
            return NvError_InSufficientBufferSize;
        }

        if (pContext->pWavReaderHandle->bWavHeader == NV_TRUE)
        {
            //This code is for Parsers providing the expected Wav structure info  for the decoder.
            NvOsMemset(wTrackInfo,0,sizeof(NvWavTrackInfo));
            NvOsMemcpy(wTrackInfo, pContext->pWavReaderHandle->pInfo->pTrackInfo, sizeof(NvWavTrackInfo));
            pBuffer->Payload.Ref.sizeOfValidDataInBytes = sizeof(NvWavTrackInfo);
            pBuffer->Payload.Ref.startOfValidData = 0;
            // Update the size with actual max frame bytes read for this work unit..
            *size = sizeof(NvWavTrackInfo);
            pContext->pWavReaderHandle->bWavHeader = NV_FALSE;

            pBuffer->PayloadInfo.BufferFlags |= NvMMBufferFlag_HeaderInfoFlag;
            
            return NvSuccess;
        }
        
        if((pContext->pWavReaderHandle->pInfo->pHeader->SubChunk2Size - pContext->pWavReaderHandle->bytesRead) < askBytes)
        {
            askBytes = pContext->pWavReaderHandle->pInfo->pHeader->SubChunk2Size - pContext->pWavReaderHandle->bytesRead;
            
            if(pContext->pWavReaderHandle->bytesRead >= pContext->pWavReaderHandle->pInfo->pHeader->SubChunk2Size)
            {
                pContext->pWavReaderHandle->bytesRead = 0;
                return NvError_ParserEndOfStream;
            }   
        }      

        status = pContext->pPipe->cpipe.CheckAvailableBytes(pContext->cphandle, askBytes, &eResult);
        // Check if we reached end of stream.
        if (eResult == CP_CheckBytesAtEndOfStream)
        {
            return NvError_ParserEndOfStream;
        }

        if(eResult == CP_CheckBytesInsufficientBytes)
        {
            status = pContext->pPipe->GetAvailableBytes(pContext->cphandle, &askBytes64);
             if (status != NvSuccess) return NvError_ParserFailure;
             askBytes = (NvU32)askBytes64;
        }

        if((eResult == CP_CheckBytesInsufficientBytes) || (eResult == CP_CheckBytesOk)) 
        {
            pBuffer->Payload.Ref.startOfValidData = 0;
            pBuffer->Payload.Ref.sizeOfValidDataInBytes = 0;
          
            status = pContext->pPipe->cpipe.Read(pContext->cphandle, (CPbyte*)pBuffer->Payload.Ref.pMem, askBytes);
            if(status != NvSuccess)
            {
                 if(status == NvError_EndOfFile)
                 {
                     return NvError_ParserEndOfStream;
                 }
                 else
                 {
                     return NvError_ParserReadFailure;
                 }
            }
            pContext->pWavReaderHandle->bytesRead += askBytes;                       
            
            pBuffer->Payload.Ref.sizeOfValidDataInBytes= askBytes;
            pBuffer->PayloadInfo.TimeStamp = pContext->ParserTimeStamp;
            pContext->timeStamp = GetWavTimeStamp(pContext->pWavReaderHandle, askBytes);
            pContext->ParserTimeStamp +=  pContext->timeStamp;
            //NvOsDebugPrintf("Sending buffer to mixer ts:%d \n", (NvU32)pContext->ParserTimeStamp);
            //pBuffer->PayloadInfo.TimeStamp = pContext->ParserTimeStamp;
            *size = askBytes;
        }   
    }    

    return status;

}


NvError WavCoreParserClose(NvMMParserCoreHandle hParserCore)
{
    NvError status = NvSuccess;
    NvMMWavParserCoreContext *pContext = (NvMMWavParserCoreContext *)hParserCore->pContext;

    if (pContext)
    {        
        if (pContext->pWavReaderHandle)
        {
            NvReleaseWavReaderHandle(pContext->pWavReaderHandle);
            NvOsFree(pContext->pWavReaderHandle);
            pContext->pWavReaderHandle = NULL;
        }
        
        if (pContext->cphandle)
        {
            if (pContext->pPipe->cpipe.Close(pContext->cphandle) != NvSuccess)
            {
                status = NvError_ParserCloseFailure;
            }
            pContext->cphandle = 0;
        }

        if (pContext->szFilename)
        {
            NvOsFree(pContext->szFilename); 
            pContext->szFilename = NULL;
        }
        NvOsFree(pContext);
        hParserCore->pContext = NULL;
    }
    return status;

}

void WavCoreParserGetMaxOffsets(NvMMParserCoreHandle hParserCore, NvU32 *pMaxOffsets)
{
   *pMaxOffsets = 0;
}

NvBool  
WavCoreParserGetBufferRequirements(
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

        pBufReq->minBufferSize = WAV_DEC_MIN_INPUT_BUFFER_SIZE;
        pBufReq->maxBufferSize = WAV_DEC_MAX_INPUT_BUFFER_SIZE;

        return NV_TRUE;

    

}

NvError 
WavCoreParserGetAttribute(
    NvMMParserCoreHandle hParserCore, 
    NvU32 AttributeType, 
    NvU32 AttributeSize, 
    void *pAttribute)
{
   NvError status = NvSuccess;
    return status;           
}

