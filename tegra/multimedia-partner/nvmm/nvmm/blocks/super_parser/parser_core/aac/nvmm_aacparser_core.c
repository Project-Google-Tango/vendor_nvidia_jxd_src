/*
* Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
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
#include "nvmm_aacparser_core.h"
#include "nvmm_contentpipe.h"
#include "nvmm_aacparser.h"
#include "nvmm_aac.h"
#include "nvmm_metadata.h"

#define AAC_NUM_CHANNELS_SUPPORTED       6

#define READ_AAC_INPUT_BUFFER_SIZE      (768 * AAC_NUM_CHANNELS_SUPPORTED*2)
#define AAC_MAX_INPUT_BUFFER_SIZE       (768 * AAC_NUM_CHANNELS_SUPPORTED*2)
#define AAC_MIN_INPUT_BUFFER_SIZE       (768 * AAC_NUM_CHANNELS_SUPPORTED*2)

/* Context for aac parser core */

typedef struct
{
    CPhandle cphandle;
    CP_PIPETYPE_EXTENDED *pPipe;
    NvAacParser *pAacReaderHandle;
    NvBool   bSentEOS;    
    NvU64 ParserTimeStamp;
    NvU64 RunningTimeStamp;
    NvMMAttrib_ParseRate rate;
    NvMMAttrib_ParsePosition position;
    NvU32 numberOfTracks;
    // Indicates file reached to EOF while seeking
    NvBool bFileReachedEOF;
} NvMMAacParserCoreContext;

// Prototypes for Ogg implementation of parser functions
NvError AacCoreParserGetNumberOfStreams(NvMMParserCoreHandle hParserCore, NvU32 *streams);
NvError AacCoreParserGetTotalTrackTime(NvMMParserCoreHandle hParserCore, NvS64* TotalTime);
NvError AacCoreParserGetStreamInfo(NvMMParserCoreHandle hParserCore, NvMMStreamInfo **pInfo);
NvError AacCoreParserSetRate(NvMMParserCoreHandle hParserCore, NvS32 rate); 
NvS32   AacCoreParserGetRate(NvMMParserCoreHandle hParserCore); 
NvError AacCoreParserSetPosition(NvMMParserCoreHandle hParserCore, NvU64 *timestamp); 
NvError AacCoreParserGetPosition( NvMMParserCoreHandle hParserCore, NvU64 *timestamp); 
NvError AacCoreParserOpen(NvMMParserCoreHandle hParserCore, NvString pURI);
NvError AacCoreParserClose(NvMMParserCoreHandle hParserCore);
void    AacCoreParserGetMaxOffsets(NvMMParserCoreHandle hParserCore, NvU32 *pMaxOffsets);

NvError 
AacCoreParserGetNextWorkUnit(
                             NvMMParserCoreHandle hParserCore, 
                             NvU32 streamIndex, 
                             NvMMBuffer *pBuffer, 
                             NvU32 *size, 
                             NvBool *pMoreWorkPending);

NvBool  
AacCoreParserGetBufferRequirements(
                                   NvMMParserCoreHandle hParserCore,
                                   NvU32 StreamIndex,
                                   NvU32 Retry,
                                   NvMMNewBufferRequirementsInfo *pBufReq);

NvError 
AacCoreParserGetAttribute(
                          NvMMParserCoreHandle hParserCore, 
                          NvU32 AttributeType, 
                          NvU32 AttributeSize, 
                          void *pAttribute);

NvError 
NvMMCreateAacCoreParser(
                        NvMMParserCoreHandle *hParserCore, 
                        NvMMParserCoreCreationParameters *pParams)
{
    NvMMParserCore *pParserCore        = NULL; 

    if ((pParserCore = NvOsAlloc(sizeof(NvMMParserCore))) == NULL)
    {
        return NvError_InsufficientMemory;
    }
    NvOsMemset(pParserCore, 0, sizeof(NvMMParserCore));

    pParserCore->Open               = AacCoreParserOpen;
    pParserCore->GetNumberOfStreams = AacCoreParserGetNumberOfStreams;
    pParserCore->GetStreamInfo      = AacCoreParserGetStreamInfo;
    pParserCore->SetRate            = AacCoreParserSetRate;
    pParserCore->GetRate            = AacCoreParserGetRate;
    pParserCore->SetPosition        = AacCoreParserSetPosition;
    pParserCore->GetPosition        = AacCoreParserGetPosition;
    pParserCore->GetNextWorkUnit    = AacCoreParserGetNextWorkUnit;
    pParserCore->Close              = AacCoreParserClose;
    pParserCore->GetMaxOffsets      = AacCoreParserGetMaxOffsets;
    pParserCore->GetAttribute       = AacCoreParserGetAttribute;
    pParserCore->GetBufferRequirements = AacCoreParserGetBufferRequirements;
    pParserCore->eCoreType          = NvMMParserCoreType_AacParser;
    pParserCore->UlpKpiMode         = pParams->UlpKpiMode;
    pParserCore->bEnableUlpMode     = pParams->bEnableUlpMode;
    pParserCore->bReduceVideoBuffers= pParams->bReduceVideoBuffers;

    *hParserCore = pParserCore;

    return NvSuccess;
}

void NvMMDestroyAacCoreParser(NvMMParserCoreHandle hParserCore)
{
    if (hParserCore)
    {
        NvOsFree(hParserCore);
    }
}

NvError AacCoreParserOpen(NvMMParserCoreHandle hParserCore, NvString pFilename)
{
    NvError Status = NvSuccess;
    NvMMAacParserCoreContext *pContext = NULL;

    NV_LOGGER_PRINT((NVLOG_AAC_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));
    
    NVMM_CHK_ARG (hParserCore && pFilename);
    
    pContext = NvOsAlloc(sizeof(NvMMAacParserCoreContext));
    NVMM_CHK_MEM(pContext);
    NvOsMemset(pContext, 0, sizeof(NvMMAacParserCoreContext));
    hParserCore->pContext = (void*)pContext;

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

    pContext->pAacReaderHandle = NvGetAacReaderHandle(pContext->cphandle, pContext->pPipe);
    NVMM_CHK_MEM(pContext->pAacReaderHandle);
    
    if (hParserCore->bUsingCachedCP)
    {
        NVMM_CHK_ERR (pContext->pPipe->InitializeCP (pContext->cphandle,
                hParserCore->MinCacheSize,
                hParserCore->MaxCacheSize,
                hParserCore->SpareCacheSize,
                (CPuint64 *) &hParserCore->pActualCacheSize));
    }
    
    NVMM_CHK_ERR (NvParseAacTrackInfo(pContext->pAacReaderHandle));

    pContext->RunningTimeStamp = GetAacTimeStamp(pContext->pAacReaderHandle->pInfo->pTrackInfo->samplingFreq);

    pContext->rate.Rate  = 1000;
    hParserCore->pPipe    = pContext->pPipe;
    hParserCore->cphandle = pContext->cphandle;

cleanup:
    if (Status != NvSuccess)
    {
        (void)AacCoreParserClose(hParserCore);
    }

    NV_LOGGER_PRINT((NVLOG_AAC_PARSER, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));    
    return Status;

}

NvError AacCoreParserGetNumberOfStreams(NvMMParserCoreHandle hParserCore, NvU32 *streams)
{    
    NvError status = NvSuccess;
    if(hParserCore && hParserCore->pContext && streams)
    {
        NvMMAacParserCoreContext *pContext = (NvMMAacParserCoreContext *)hParserCore->pContext;
        if(pContext->pAacReaderHandle)
        {
            *streams =   pContext->pAacReaderHandle->numberOfTracks;
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

NvError AacCoreParserGetStreamInfo(NvMMParserCoreHandle hParserCore, NvMMStreamInfo **pInfo)
{        
    NvError status = NvSuccess;
    NvS32 i,TotalTracks;
    NvS64 TotalTrackTime = 0;
    NvU64 TotalTrackDuration = 0;
    NvMMAacParserCoreContext *pContext = (NvMMAacParserCoreContext *)hParserCore->pContext;

    TotalTracks = pContext->pAacReaderHandle->numberOfTracks;
    AacCoreParserGetTotalTrackTime(hParserCore, &TotalTrackTime);
    TotalTrackDuration = ((NvU64)TotalTrackTime * 10000000);

    for (i = 0; i < TotalTracks; i++)
    {
        pInfo[i]->StreamType                                = NvMMStreamType_AAC;
        pInfo[i]->BlockType                                 = NvMMBlockType_SuperParser;
        pInfo[i]->TotalTime                                 = TotalTrackDuration;
        pInfo[i]->NvMMStream_Props.AudioProps.SampleRate    = pContext->pAacReaderHandle->pInfo->pTrackInfo->samplingFreq;
        pInfo[i]->NvMMStream_Props.AudioProps.BitRate       = pContext->pAacReaderHandle->pInfo->pTrackInfo->bitRate;
        pInfo[i]->NvMMStream_Props.AudioProps.NChannels     = pContext->pAacReaderHandle->pInfo->pTrackInfo->noOfChannels;
        pInfo[i]->NvMMStream_Props.AudioProps.BitsPerSample = pContext->pAacReaderHandle->pInfo->pTrackInfo->sampleSize; 
    } 

    pContext->pAacReaderHandle->bAacHeader = NV_TRUE;
    pContext->pAacReaderHandle->InitFlag = NV_TRUE;

    return status;
} 

NvError AacCoreParserGetTotalTrackTime(NvMMParserCoreHandle hParserCore, NvS64* TotalTime)
{    
    NvError status = NvSuccess;
    NvMMAacParserCoreContext *pContext = (NvMMAacParserCoreContext *)hParserCore->pContext;

    *TotalTime = pContext->pAacReaderHandle->pInfo->pTrackInfo->total_time;

    return status;    
}

NvError AacCoreParserSetRate(NvMMParserCoreHandle hParserCore, NvS32 rate)
{ 
    NvError status = NvSuccess;
    NvMMAacParserCoreContext *pContext = (NvMMAacParserCoreContext *)hParserCore->pContext;

    pContext->rate.Rate = rate;   

    return status;
}

NvS32 AacCoreParserGetRate(NvMMParserCoreHandle hParserCore)
{ 
    NvMMAacParserCoreContext *pContext = (NvMMAacParserCoreContext *)hParserCore->pContext;
    return (pContext->rate.Rate);
}

NvError AacCoreParserSetPosition(NvMMParserCoreHandle hParserCore, NvU64 *timestamp)
{ 
    NvError status = NvSuccess;
    NvS64 mSec;   
    NvU64 TotalTime = 0;
    NvS64 TotalTrackTime = 0;
    NvMMAacParserCoreContext *pContext = (NvMMAacParserCoreContext *)hParserCore->pContext;

    mSec = *timestamp;

    AacCoreParserGetTotalTrackTime(hParserCore, &TotalTrackTime);
    TotalTime = ((NvU64)TotalTrackTime * 10000000);

    if(*timestamp > TotalTime)
    {
        pContext->bFileReachedEOF = NV_TRUE;
        // Update the parse position with TotalTime of the track
        pContext->position.Position = TotalTime;
        return NvSuccess;
    }
    else if (*timestamp == 0)
    {
        // CEPlayer will call seek(0) for repeat mode hence reset the flag
        pContext->bFileReachedEOF = NV_FALSE;
        pContext->pAacReaderHandle->bAacHeader = NV_TRUE;
    }

    status = NvAacSeekToTime(pContext->pAacReaderHandle,mSec);

    pContext->ParserTimeStamp = *timestamp;
    pContext->position.Position = *timestamp;

    return status;
}

NvError AacCoreParserGetPosition( NvMMParserCoreHandle hParserCore, NvU64 *timestamp)
{
    NvError status = NvSuccess;
    NvMMAacParserCoreContext *pContext = (NvMMAacParserCoreContext *)hParserCore->pContext;

    *(timestamp) = pContext->position.Position;
    return status;
}

NvError 
AacCoreParserGetNextWorkUnit(
                             NvMMParserCoreHandle hParserCore, 
                             NvU32 streamIndex, 
                             NvMMBuffer *pBuffer, 
                             NvU32 *size, 
                             NvBool *pMoreWorkPending)
{ 
    NvError status = NvSuccess;
    NvU32 askBytes = 0;
    CP_CHECKBYTESRESULTTYPE eResult = CP_CheckBytesOk;  
    NvU8 *pData = NULL;
    NvU32 ulBytesRead;
    CPuint64 askBytes64 = 0;
    NvMMAudioAacTrackInfo aacTrackInfo; 

    NvMMAacParserCoreContext *pContext = (NvMMAacParserCoreContext *)hParserCore->pContext;
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
        askBytes = READ_AAC_INPUT_BUFFER_SIZE;

        if( pBuffer->Payload.Ref.sizeOfBufferInBytes < askBytes)
        {
            // returning required minimum size
            *size = askBytes;
            return NvError_InSufficientBufferSize;
        }

        if (pContext->pAacReaderHandle->bAacHeader == NV_TRUE) 
        {              
            aacTrackInfo.objectType = (NvU32)pContext->pAacReaderHandle->pInfo->pTrackInfo->objectType;
            aacTrackInfo.profile = (NvU32)pContext->pAacReaderHandle->pInfo->pTrackInfo->profile;
            aacTrackInfo.samplingFreqIndex = (NvU32)pContext->pAacReaderHandle->pInfo->pTrackInfo->samplingFreqIndex;
            aacTrackInfo.channelConfiguration = (NvU32)pContext->pAacReaderHandle->pInfo->pTrackInfo->channelConfiguration;
            aacTrackInfo.samplingFreq = (NvU32)pContext->pAacReaderHandle->pInfo->pTrackInfo->samplingFreq;
            aacTrackInfo.noOfChannels = (NvU32)pContext->pAacReaderHandle->pInfo->pTrackInfo->noOfChannels;
            aacTrackInfo.sampleSize = (NvU32)pContext->pAacReaderHandle->pInfo->pTrackInfo->sampleSize;            
            aacTrackInfo.bitRate = (NvU32)pContext->pAacReaderHandle->pInfo->pTrackInfo->bitRate;           

            NvOsMemcpy(pBuffer->Payload.Ref.pMem, (NvU8*)(&aacTrackInfo), sizeof( NvMMAudioAacTrackInfo));
            pBuffer->Payload.Ref.sizeOfValidDataInBytes = sizeof(NvMMAudioAacTrackInfo);
            pBuffer->Payload.Ref.startOfValidData = 0;
            // Update the size with actual max frame bytes read for this work unit..
            *size = sizeof(NvMMAudioAacTrackInfo); 
            pContext->pAacReaderHandle->bAacHeader = NV_FALSE;        

            pBuffer->PayloadInfo.BufferFlags |= NvMMBufferFlag_HeaderInfoFlag;

            return NvSuccess;
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

        if(eResult == CP_CheckBytesInsufficientBytes)
        {
            status = pContext->pPipe->GetAvailableBytes(pContext->cphandle, &askBytes64);
            if (status != NvSuccess) return NvError_ParserFailure;
            NV_ASSERT(askBytes64 <= (CPuint64)0xffffffff);
            askBytes = (NvU32)askBytes64;
        }        

        if((eResult == CP_CheckBytesInsufficientBytes) || (eResult == CP_CheckBytesOk)) 
        {
            pData = (NvU8 *)pBuffer->Payload.Ref.pMem;
            status = NvAdts_AacGetNextFrame(pContext->pAacReaderHandle, pData, askBytes, &ulBytesRead);
            if(status != NvSuccess)
            {
                return status;
            }

            if(ulBytesRead != askBytes)
            {
                askBytes = ulBytesRead;
            }

            pBuffer->Payload.Ref.startOfValidData= 0;
            pBuffer->Payload.Ref.sizeOfValidDataInBytes= askBytes;

            pContext->ParserTimeStamp +=  pContext->RunningTimeStamp;
            pBuffer->PayloadInfo.TimeStamp = pContext->ParserTimeStamp;                         

            *size = askBytes;
        }   
    }      
    return status;
}

NvError AacCoreParserClose(NvMMParserCoreHandle hParserCore)
{
    NvError Status = NvSuccess;
    NvMMAacParserCoreContext *pContext = NULL;

    NV_LOGGER_PRINT((NVLOG_AAC_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));
    
    NVMM_CHK_ARG(hParserCore && hParserCore->pContext);
    
    pContext = (NvMMAacParserCoreContext *)hParserCore->pContext;
 
    if (pContext->pAacReaderHandle)
    {
        NvReleaseAacReaderHandle(pContext->pAacReaderHandle);
    }

    if (pContext->cphandle)
    {
        Status = pContext->pPipe->cpipe.Close(pContext->cphandle);
    }

    NvOsFree(pContext);

    hParserCore->pContext = NULL;

cleanup:
    NV_LOGGER_PRINT((NVLOG_AAC_PARSER, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));    
    return Status;
}

NvBool  
AacCoreParserGetBufferRequirements(
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

    pBufReq->minBufferSize = AAC_MIN_INPUT_BUFFER_SIZE;
    pBufReq->maxBufferSize = AAC_MAX_INPUT_BUFFER_SIZE;     

    return NV_TRUE;
}



void AacCoreParserGetMaxOffsets(NvMMParserCoreHandle hParserCore, NvU32 *pMaxOffsets)
{
    *pMaxOffsets = 0;
}

NvError 
AacCoreParserGetAttribute(
                          NvMMParserCoreHandle hParserCore,
                          NvU32 AttributeType, 
                          NvU32 AttributeSize, 
                          void *pAttribute)
{
    NvError Status = NvSuccess;
    NvMMAacParserCoreContext  *pContext = NULL;
    NvMMCodecInfo *pCodecInfo;

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext && pAttribute);
    pContext = (NvMMAacParserCoreContext *) hParserCore->pContext;

    pCodecInfo = (NvMMCodecInfo*)pAttribute;

    if (AttributeType == NvMMAttribute_Header)
    {
        NvU16 data = 0;
        // AudioSpecificInfo follows

        // oooo offf fccc c000
        // o - audioObjectType
        // f - samplingFreqIndex
        // c - channelConfig

        data |= (pContext->pAacReaderHandle->pInfo->pTrackInfo->channelConfiguration<<3);
        data |= (pContext->pAacReaderHandle->pInfo->pTrackInfo->samplingFreqIndex<<7);
        data |= (pContext->pAacReaderHandle->pInfo->pTrackInfo->objectType<<11);
        data = (data>>8)|(data<<8);
        if(pCodecInfo->pBuffer)
        {
            NvOsMemcpy (pCodecInfo->pBuffer, (NvU8*) &data, 2);
        }
        pCodecInfo->BufferLen = 2;
    }

cleanup:
    return Status;
}





























































