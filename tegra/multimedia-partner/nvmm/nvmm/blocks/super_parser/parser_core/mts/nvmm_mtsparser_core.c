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
#include "nvmm_mtsparser_core.h"
#include "nvlocalfilecontentpipe.h"
#include "nvmm_mtsparser.h"


/* Context for Mts parser core */

typedef struct
{
    NvString szFilename;
    CPhandle cphandle;
    CP_PIPETYPE *pPipe;
    NvMtsParser *pMtsReaderHandle;
    NvBool   bSentEOS;    
    NvU64 ParserTimeStamp;
    NvMMAttrib_ParseRate rate;
    NvMMAttrib_ParsePosition position;
    NvU32 numberOfTracks;
} NvMMMtsParserCoreContext;


// Prototypes for Ogg implementation of parser functions
NvError MtsCoreParserGetNumberOfStreams(NvMMParserCoreHandle hParserCore, NvU32 *streams);
NvError MtsCoreParserGetTotalTrackTime(NvMMParserCoreHandle hParserCore, NvS64* TotalTime);
NvError MtsCoreParserGetStreamInfo(NvMMParserCoreHandle hParserCore, NvMMStreamInfo **pInfo);
NvError MtsCoreParserSetRate(NvMMParserCoreHandle hParserCore, NvS32 rate); 
NvS32   MtsCoreParserGetRate(NvMMParserCoreHandle hParserCore); 
NvError MtsCoreParserSetPosition(NvMMParserCoreHandle hParserCore, NvU64 *timestamp); 
NvError MtsCoreParserGetPosition( NvMMParserCoreHandle hParserCore, NvU64 *timestamp); 
NvError MtsCoreParserOpen(NvMMParserCoreHandle hParserCore, NvString pURI);
NvError MtsCoreParserClose(NvMMParserCoreHandle hParserCore);
void    MtsCoreParserGetMaxOffsets(NvMMParserCoreHandle hParserCore, NvU32 *pMaxOffsets);

NvError 
MtsCoreParserGetNextWorkUnit(
                             NvMMParserCoreHandle hParserCore, 
                             NvU32 streamIndex, 
                             NvMMBuffer *pBuffer, 
                             NvU32 *size, 
                             NvBool *pMoreWorkPending);

NvBool  
MtsCoreParserGetBufferRequirements(
                                   NvMMParserCoreHandle hParserCore,
                                   NvU32 StreamIndex,
                                   NvU32 Retry,
                                   NvMMNewBufferRequirementsInfo *pBufReq);

NvError 
MtsCoreParserGetAttribute(
                          NvMMParserCoreHandle hParserCore, 
                          NvU32 AttributeType, 
                          NvU32 AttributeSize, 
                          void *pAttribute);



NvError 
NvMMCreateMtsCoreParser(
                        NvMMParserCoreHandle *hParserCore, 
                        NvMMParserCoreCreationParameters *pParams)
{
    NvError status                     = NvSuccess;
    NvMMParserCore *pParserCore        = NULL; 

    if ((pParserCore = NvOsAlloc(sizeof(NvMMParserCore))) == NULL)
    {
        status = NvError_InsufficientMemory;
        goto NvMMMtsParserCoreOpen_err;
    }
    NvOsMemset(pParserCore, 0, sizeof(NvMMParserCore));

    pParserCore->Open               = MtsCoreParserOpen;
    pParserCore->GetNumberOfStreams = MtsCoreParserGetNumberOfStreams;
    pParserCore->GetStreamInfo      = MtsCoreParserGetStreamInfo;
    pParserCore->SetRate            = MtsCoreParserSetRate;
    pParserCore->GetRate            = MtsCoreParserGetRate;
    pParserCore->SetPosition        = MtsCoreParserSetPosition;
    pParserCore->GetPosition        = MtsCoreParserGetPosition;
    pParserCore->GetNextWorkUnit    = MtsCoreParserGetNextWorkUnit;
    pParserCore->Close              = MtsCoreParserClose;
    pParserCore->GetMaxOffsets      = MtsCoreParserGetMaxOffsets;
    pParserCore->GetAttribute       = MtsCoreParserGetAttribute;
    pParserCore->GetBufferRequirements = MtsCoreParserGetBufferRequirements;
    pParserCore->eCoreType          = NvMMParserCoreType_MtsParser;
    pParserCore->UlpKpiMode         = pParams->UlpKpiMode;
    pParserCore->bEnableUlpMode     = pParams->bEnableUlpMode;
    pParserCore->bReduceVideoBuffers= pParams->bReduceVideoBuffers;

    *hParserCore = pParserCore;

    return NvSuccess;

NvMMMtsParserCoreOpen_err:

    if (pParserCore)
    {
        NvOsFree(pParserCore);
        *hParserCore = NULL;
    }

    return status;
}

void NvMMDestroyMtsCoreParser(NvMMParserCoreHandle hParserCore)
{
    if (hParserCore)
    {
        NvOsFree(hParserCore);
    }
}

NvError MtsCoreParserOpen(NvMMParserCoreHandle hParserCore, NvString pFilename)
{
    NvError status = NvSuccess;
    NvU32 StringLength = 0;
    NvU32 streams;
    NvMMMtsParserCoreContext *pContext;
    
    if ((pContext = NvOsAlloc(sizeof(NvMMMtsParserCoreContext))) == NULL)
    {
        return NvError_InsufficientMemory;
    }
    NvOsMemset(pContext, 0, sizeof(NvMMMtsParserCoreContext));
    hParserCore->pContext = (void*)pContext;

    StringLength = NvOsStrlen(pFilename) + 1;
    pContext->szFilename = NvOsAlloc(StringLength);
    if (pContext->szFilename == NULL)
    {
        NvOsFree(pContext);
        return NvError_InsufficientMemory;
    }
    NvOsStrncpy(pContext->szFilename, pFilename, StringLength);

    NvMMGetContentPipe((CP_PIPETYPE_EXTENDED**)&pContext->cphandle);

    pContext->pPipe = (CP_PIPETYPE *)pContext->cphandle;
    pContext->cphandle = 0;

    if (pContext->pPipe->Open(&pContext->cphandle, pContext->szFilename,CP_AccessRead) != NvSuccess)
    {
        NvOsFree(pContext->szFilename);
        NvOsFree(pContext);
        return NvError_ParserOpenFailure;
    }

    pContext->pMtsReaderHandle = NvGetMtsReaderHandle(pContext->cphandle, pContext->pPipe);
    if (pContext->pMtsReaderHandle == NULL)
    {
        NvOsFree(pContext->szFilename);
        NvOsFree(pContext);
        status = NvError_InsufficientMemory;
        goto NvMMMtsParserOpen_err;
    }

    status = NvParseMtsTrackInfo(pContext->pMtsReaderHandle);
    if(status != NvSuccess)
    {
        NvOsFree(pContext->szFilename);
        NvOsFree(pContext);
        status = NvError_ParserFailure;
        goto NvMMMtsParserOpen_err;
    }

    pContext->rate.Rate  = 1000;

    MtsCoreParserGetNumberOfStreams(hParserCore, &streams);

    return status;     

NvMMMtsParserOpen_err:

    if(pContext)
    {
        if(pContext->szFilename)
        {
            NvOsFree(pContext->szFilename);
            pContext->szFilename = NULL;
        }
        if (pContext->pMtsReaderHandle)
        {
            NvReleaseMtsReaderHandle(pContext->pMtsReaderHandle);
            NvOsFree(pContext->pMtsReaderHandle);
            pContext->pMtsReaderHandle = NULL;
        }

        if(pContext->cphandle)
        {
            if( pContext->pPipe->Close(pContext->cphandle) != NvSuccess)
            {
                status = NvError_ParserCloseFailure;
            }
            pContext->cphandle = 0;
        }    
        NvOsFree(pContext);
    }

    return status;   
}

NvError MtsCoreParserGetNumberOfStreams(NvMMParserCoreHandle hParserCore, NvU32 *streams)
{    
    NvError status = NvSuccess;
    NvMMMtsParserCoreContext *pContext = (NvMMMtsParserCoreContext *)hParserCore->pContext;

    *streams = pContext->pMtsReaderHandle->numberOfTracks;
    return status;     
}

NvError MtsCoreParserGetStreamInfo(NvMMParserCoreHandle hParserCore, NvMMStreamInfo **pInfo)
{        
    NvError status = NvSuccess;
    NvS32 i,TotalTracks;
    NvS64 TotalTrackTime = 0;
    NvS64 TotalTrackDuration = 0;

    NvMMMtsParserCoreContext* pContext = (NvMMMtsParserCoreContext *)hParserCore->pContext;
    if (pContext == NULL)
    {
        return NvError_ParserFailure;
    }

    if(pContext->pMtsReaderHandle != NULL)
    {
        TotalTracks         = pContext->pMtsReaderHandle->pInfo->pTrackInfo->numberOfTracks;
        MtsCoreParserGetTotalTrackTime(hParserCore, &TotalTrackTime);
        TotalTrackDuration  = ((NvS64)TotalTrackTime * 10000000);    

        for (i = 0; i < TotalTracks; i++)
        {
            pInfo[i]->TotalTime   = TotalTrackDuration;
            pInfo[i]->BlockType   = NvMMBlockType_SuperParser;
            pInfo[i]->StreamType = pContext->pMtsReaderHandle->pInfo->pTrackInfo->contentType;

            switch(pInfo[i]->StreamType)
            {
                case NvMMStreamType_AC3:
                case NvMMStreamType_WAV:
                    // Fill in Audio Properties info.
                    pInfo[i]->NvMMStream_Props.AudioProps.SampleRate    = pContext->pMtsReaderHandle->pInfo->pTrackInfo->samplingFreq;
                    pInfo[i]->NvMMStream_Props.AudioProps.BitRate       = pContext->pMtsReaderHandle->pInfo->pTrackInfo->bitRate;
                    pInfo[i]->NvMMStream_Props.AudioProps.NChannels     = pContext->pMtsReaderHandle->pInfo->pTrackInfo->noOfChannels;
                    pInfo[i]->NvMMStream_Props.AudioProps.BitsPerSample = pContext->pMtsReaderHandle->pInfo->pTrackInfo->sampleSize;
                    break;

                case NvMMStreamType_H264:
                case NvMMStreamType_H263:   
                    // Fill in Video Properties info.
                    pInfo[i]->NvMMStream_Props.VideoProps.Width         = pContext->pMtsReaderHandle->pInfo->pTrackInfo->Width;
                    pInfo[i]->NvMMStream_Props.VideoProps.Height        = pContext->pMtsReaderHandle->pInfo->pTrackInfo->Height;
                    pInfo[i]->NvMMStream_Props.VideoProps.Fps           = pContext->pMtsReaderHandle->pInfo->pTrackInfo->Fps;
                    pInfo[i]->NvMMStream_Props.VideoProps.VideoBitRate  = pContext->pMtsReaderHandle->pInfo->pTrackInfo->VideoBitRate; 
                    break;
                 default:
                    break;
            }
            
        }
    }
    else 
    {
        return NvError_ParserFailure;
    }    

    return status;
} 

NvError MtsCoreParserGetTotalTrackTime(NvMMParserCoreHandle hParserCore, NvS64* TotalTime)
{    
    NvError status = NvSuccess;
    NvMMMtsParserCoreContext *pContext = (NvMMMtsParserCoreContext *)hParserCore->pContext;

    *TotalTime = pContext->pMtsReaderHandle->pInfo->pTrackInfo->total_time;

    return status;    
}

NvError MtsCoreParserSetRate(NvMMParserCoreHandle hParserCore, NvS32 rate)
{ 
    NvError status = NvSuccess;
    NvMMMtsParserCoreContext *pContext = (NvMMMtsParserCoreContext *)hParserCore->pContext;

    pContext->rate.Rate = rate;   
    return status;
}

NvS32 MtsCoreParserGetRate(NvMMParserCoreHandle hParserCore)
{ 
    NvMMMtsParserCoreContext *pContext = (NvMMMtsParserCoreContext *)hParserCore->pContext;
    return (pContext->rate.Rate);
}

NvError MtsCoreParserSetPosition(NvMMParserCoreHandle hParserCore, NvU64 *timestamp)
{ 
    NvError status = NvSuccess;
    NvS64 mSec;   
    NvU32 divFactor = 10000;
    NvU64 TotalTime = 0;
    NvS64 TotalTrackTime = 0;
    NvMMMtsParserCoreContext *pContext = (NvMMMtsParserCoreContext *)hParserCore->pContext;

    mSec = ((*timestamp)/divFactor); 

    MtsCoreParserGetTotalTrackTime(hParserCore, &TotalTrackTime);
    TotalTime = ((NvU64)TotalTrackTime * 10000000);

    if(*timestamp > TotalTime)
    {
        return NvError_ParserEndOfStream;
    }

    status = NvMtsSeekToTime(pContext->pMtsReaderHandle,mSec);

    pContext->ParserTimeStamp = *timestamp;
    pContext->position.Position = *timestamp;

    return status;
}

NvError MtsCoreParserGetPosition( NvMMParserCoreHandle hParserCore, NvU64 *timestamp)
{
    NvError status = NvSuccess;
    NvMMMtsParserCoreContext *pContext = (NvMMMtsParserCoreContext *)hParserCore->pContext;

    *(timestamp) = pContext->position.Position;
    return status;
}

NvError 
MtsCoreParserGetNextWorkUnit(
                             NvMMParserCoreHandle hParserCore, 
                             NvU32 streamIndex, 
                             NvMMBuffer *pBuffer, 
                             NvU32 *size, 
                             NvBool *pMoreWorkPending)
{ 
    NvError status = NvSuccess;










    


    return status;
}

NvError MtsCoreParserClose(NvMMParserCoreHandle hParserCore)
{
    NvError status = NvSuccess;
    NvMMMtsParserCoreContext *pContext = (NvMMMtsParserCoreContext *)hParserCore->pContext;

    if (pContext)
    {        
        if (pContext->pMtsReaderHandle)
        {
            NvReleaseMtsReaderHandle(pContext->pMtsReaderHandle);
            NvOsFree(pContext->pMtsReaderHandle);
            pContext->pMtsReaderHandle = NULL;
        }

        if (pContext->cphandle)
        {
            if (pContext->pPipe->Close(pContext->cphandle) != NvSuccess)
            {
                status = NvError_ParserCloseFailure;
            }
            pContext->cphandle = 0;
        }

        if (pContext->szFilename)
        {
            NvOsFree(pContext->szFilename); 
        }
        NvOsFree(pContext);
    }
    return status;
}

NvBool  
MtsCoreParserGetBufferRequirements(
                                   NvMMParserCoreHandle hParserCore,
                                   NvU32 StreamIndex,
                                   NvU32 Retry,
                                   NvMMNewBufferRequirementsInfo *pBufReq)
{  
   NvMMMtsParserCoreContext* pContext = (NvMMMtsParserCoreContext *)hParserCore->pContext;
   NvMMStreamType type = pContext->pMtsReaderHandle->pInfo->pTrackInfo->contentType;
   
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

    switch(type)
    {
        case NvMMStreamType_AC3:
        case NvMMStreamType_WAV:
            if(pContext->pMtsReaderHandle->pInfo->pTrackInfo->AudioSuggestedBufferSize)
            {
                if (pContext->pMtsReaderHandle->pInfo->pTrackInfo->AudioSuggestedBufferSize < 2048)
                    pContext->pMtsReaderHandle->pInfo->pTrackInfo->AudioSuggestedBufferSize = 2048;
                pBufReq->maxBufferSize = pContext->pMtsReaderHandle->pInfo->pTrackInfo->AudioSuggestedBufferSize;
                pBufReq->minBufferSize = pContext->pMtsReaderHandle->pInfo->pTrackInfo->AudioSuggestedBufferSize;
            }
            else
            {
                pBufReq->maxBufferSize = 65536;
                pBufReq->minBufferSize = 512;
            }   
            break;

        case NvMMStreamType_H264:
        case NvMMStreamType_H263:   
            if(((pContext->pMtsReaderHandle->pInfo->pTrackInfo->Width) == 0)||((pContext->pMtsReaderHandle->pInfo->pTrackInfo->Height) ==0))
            {
                pBufReq->minBufferSize = 32768;
                pBufReq->maxBufferSize = 512000*2 - 32768;
            }
            else
            {                
                if(((pContext->pMtsReaderHandle->pInfo->pTrackInfo->Width) > 320) && ((pContext->pMtsReaderHandle->pInfo->pTrackInfo->Height) > 240))
                {
                    pBufReq->minBufferSize = ((pContext->pMtsReaderHandle->pInfo->pTrackInfo->Width)*(pContext->pMtsReaderHandle->pInfo->pTrackInfo->Height)*3)>>2;
                    pBufReq->maxBufferSize = ((pContext->pMtsReaderHandle->pInfo->pTrackInfo->Width)*(pContext->pMtsReaderHandle->pInfo->pTrackInfo->Height)*3)>>2;
                }
                else
                {
                    pBufReq->minBufferSize = ((pContext->pMtsReaderHandle->pInfo->pTrackInfo->Width)*(pContext->pMtsReaderHandle->pInfo->pTrackInfo->Height)*3)>>1;
                    pBufReq->maxBufferSize = ((pContext->pMtsReaderHandle->pInfo->pTrackInfo->Width)*(pContext->pMtsReaderHandle->pInfo->pTrackInfo->Height)*3)>>1;
                }    
            }
            
            break;
         default:
            break;

    }   

    return NV_TRUE;
}

void MtsCoreParserGetMaxOffsets(NvMMParserCoreHandle hParserCore, NvU32 *pMaxOffsets)
{
    *pMaxOffsets = 0;
}

NvError 
MtsCoreParserGetAttribute(
                          NvMMParserCoreHandle hParserCore,
                          NvU32 AttributeType, 
                          NvU32 AttributeSize, 
                          void *pAttribute)
{
    NvError status = NvSuccess;
    return status;
}
















































