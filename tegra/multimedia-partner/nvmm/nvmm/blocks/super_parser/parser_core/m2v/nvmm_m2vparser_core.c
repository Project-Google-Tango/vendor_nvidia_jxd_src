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
#include "nvmm_m2vparser_core.h"
#include "nvlocalfilecontentpipe.h"
#include "nvmm_m2vparser.h"


#if ENABLE_PRE_PARSING 
#define MPEG2V_FRAME_SIZE 310*1024   //720*576*3/4
#else
#define MPEG2V_FRAME_SIZE 48*1024   
#endif

#if ENABLE_PROFILING
NvU32 StartTime, FinishTime, TotalTimePreParsing= 0;
NvU32 TotalTimeParsing= 0;
#endif

NvU32 FrameNum;

/* Context for M2v parser core */
typedef struct
{
    NvString szFilename;
    CPhandle cphandle;
    CP_PIPETYPE *pPipe;
    NvM2vParser *pM2vReaderHandle;
    NvBool   bSentEOS;    
    NvU64 ParserTimeStamp;
    NvMMAttrib_ParseRate rate;
    NvMMAttrib_ParsePosition position;
    NvU32 numberOfTracks;
    NvU32 BytesAvailable;
} NvMMM2vParserCoreContext;

// Prototypes for Ogg implementation of parser functions
NvError M2vCoreParserGetNumberOfStreams(NvMMParserCoreHandle hParserCore, NvU32 *streams);
NvError M2vCoreParserGetTotalTrackTime(NvMMParserCoreHandle hParserCore, NvS64* TotalTime);
NvError M2vCoreParserGetStreamInfo(NvMMParserCoreHandle hParserCore, NvMMStreamInfo **pInfo);
NvError M2vCoreParserSetRate(NvMMParserCoreHandle hParserCore, NvS32 rate); 
NvS32   M2vCoreParserGetRate(NvMMParserCoreHandle hParserCore); 
NvError M2vCoreParserSetPosition(NvMMParserCoreHandle hParserCore, NvU64 *timestamp); 
NvError M2vCoreParserGetPosition( NvMMParserCoreHandle hParserCore, NvU64 *timestamp); 
NvError M2vCoreParserOpen(NvMMParserCoreHandle hParserCore, NvString pURI);
NvError M2vCoreParserClose(NvMMParserCoreHandle hParserCore);
void    M2vCoreParserGetMaxOffsets(NvMMParserCoreHandle hParserCore, NvU32 *pMaxOffsets);

NvError 
M2vCoreParserGetNextWorkUnit(
                             NvMMParserCoreHandle hParserCore, 
                             NvU32 streamIndex, 
                             NvMMBuffer *pBuffer, 
                             NvU32 *size, 
                             NvBool *pMoreWorkPending);

NvBool  
M2vCoreParserGetBufferRequirements(
                                   NvMMParserCoreHandle hParserCore,
                                   NvU32 StreamIndex,
                                   NvU32 Retry,
                                   NvMMNewBufferRequirementsInfo *pBufReq);

NvError 
M2vCoreParserGetAttribute(
                          NvMMParserCoreHandle hParserCore, 
                          NvU32 AttributeType, 
                          NvU32 AttributeSize, 
                          void *pAttribute);

NvError 
NvMMCreateM2vCoreParser(
                        NvMMParserCoreHandle *hParserCore, 
                        NvMMParserCoreCreationParameters *pParams)
{
    NvMMParserCore *pParserCore        = NULL; 

    pParserCore = NvOsAlloc(sizeof(NvMMParserCore));
    if (pParserCore == NULL)
    {
        *hParserCore = NULL;
        return NvError_InsufficientMemory;
    }
    *hParserCore = pParserCore;
    NvOsMemset(pParserCore, 0, sizeof(NvMMParserCore));

    pParserCore->Open               = M2vCoreParserOpen;
    pParserCore->GetNumberOfStreams = M2vCoreParserGetNumberOfStreams;
    pParserCore->GetStreamInfo      = M2vCoreParserGetStreamInfo;
    pParserCore->SetRate            = M2vCoreParserSetRate;
    pParserCore->GetRate            = M2vCoreParserGetRate;
    pParserCore->SetPosition        = M2vCoreParserSetPosition;
    pParserCore->GetPosition        = M2vCoreParserGetPosition;
    pParserCore->GetNextWorkUnit    = M2vCoreParserGetNextWorkUnit;
    pParserCore->Close              = M2vCoreParserClose;
    pParserCore->GetMaxOffsets      = M2vCoreParserGetMaxOffsets;
    pParserCore->GetAttribute       = M2vCoreParserGetAttribute;
    pParserCore->GetBufferRequirements = M2vCoreParserGetBufferRequirements;
    pParserCore->eCoreType          = NvMMParserCoreType_M2vParser;
    pParserCore->UlpKpiMode         = pParams->UlpKpiMode;
    pParserCore->bEnableUlpMode     = pParams->bEnableUlpMode;
    pParserCore->bReduceVideoBuffers= pParams->bReduceVideoBuffers;

    return NvSuccess;
}

void NvMMDestroyM2vCoreParser(NvMMParserCoreHandle hParserCore)
{
    if (hParserCore)
    {
        NvOsFree(hParserCore);
    }
}


NvError M2vCoreParserOpen(NvMMParserCoreHandle hParserCore, NvString pFilename)
{
    NvError status = NvSuccess;
    NvU32 StringLength = 0;
    NvU32 streams;
    NvMMM2vParserCoreContext *pContext=NULL;

    if ((pContext = NvOsAlloc(sizeof(NvMMM2vParserCoreContext))) == NULL)
    {
        return NvError_InsufficientMemory;
    }
    NvOsMemset(pContext, 0, sizeof(NvMMM2vParserCoreContext));

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
        pContext->szFilename=NULL;
        NvOsFree(pContext);
        pContext=NULL;
        return NvError_ParserOpenFailure;
    }

    pContext->pM2vReaderHandle = NvGetM2vReaderHandle(pContext->cphandle, pContext->pPipe);
    if (pContext->pM2vReaderHandle == NULL)
    {
        status = NvError_InsufficientMemory;
        goto NvMMM2vParserOpen_err;
    }

    status = NvParseM2vTrackInfo(pContext->pM2vReaderHandle);
    if(status != NvSuccess)
    {
        status = NvError_ParserFailure;
        goto NvMMM2vParserOpen_err;
    }

    pContext->rate.Rate  = 1000;
    hParserCore->pContext = (void*)pContext;
    M2vCoreParserGetNumberOfStreams(hParserCore, &streams);

    return status;     

NvMMM2vParserOpen_err:

    NvOsFree(pContext->szFilename);
    pContext->szFilename=NULL;
    NvOsFree(pContext);
    pContext=NULL;

    return status;       
}

NvError M2vCoreParserGetNumberOfStreams(NvMMParserCoreHandle hParserCore, NvU32 *streams)
{    
    NvError status = NvSuccess;
    NvMMM2vParserCoreContext *pContext = (NvMMM2vParserCoreContext *)hParserCore->pContext;

    *streams = pContext->pM2vReaderHandle->numberOfTracks;
    return status;     
}

NvError M2vCoreParserGetStreamInfo(NvMMParserCoreHandle hParserCore, NvMMStreamInfo **pInfo)
{        
    NvError status = NvSuccess;
    NvS32 i,TotalTracks;
    NvS64 TotalTrackTime = 0;
    NvU64 TotalTrackDuration = 0;
    NvMMM2vParserCoreContext *pContext = (NvMMM2vParserCoreContext *)hParserCore->pContext;

    TotalTracks = pContext->pM2vReaderHandle->numberOfTracks;
    M2vCoreParserGetTotalTrackTime(hParserCore, &TotalTrackTime);
    TotalTrackDuration = ((NvU64)TotalTrackTime * 10000000);

    for (i = 0; i < TotalTracks; i++)
    {
        pInfo[i]->StreamType                                = NvMMStreamType_MPEG2V;
        pInfo[i]->BlockType                                 = NvMMBlockType_SuperParser;
        pInfo[i]->TotalTime                                 = TotalTrackDuration;
        pInfo[i]->NvMMStream_Props.VideoProps.Width         = pContext->pM2vReaderHandle->pInfo->pTrackInfo->Width;
        pInfo[i]->NvMMStream_Props.VideoProps.Height        = pContext->pM2vReaderHandle->pInfo->pTrackInfo->Height;
        pInfo[i]->NvMMStream_Props.VideoProps.Fps           = pContext->pM2vReaderHandle->pInfo->pTrackInfo->Fps;
        pInfo[i]->NvMMStream_Props.VideoProps.VideoBitRate  = pContext->pM2vReaderHandle->pInfo->pTrackInfo->VideoBitRate; 
    } 

    //pContext->pM2vReaderHandle->bM2vHeader = NV_TRUE;
    pContext->pM2vReaderHandle->InitFlag = NV_TRUE;

    return status;
} 

NvError M2vCoreParserGetTotalTrackTime(NvMMParserCoreHandle hParserCore, NvS64* TotalTime)
{    
    NvError status = NvSuccess;
    NvMMM2vParserCoreContext *pContext = (NvMMM2vParserCoreContext *)hParserCore->pContext;

    *TotalTime = pContext->pM2vReaderHandle->pInfo->pTrackInfo->total_time;

    return status;    
}

NvError M2vCoreParserSetRate(NvMMParserCoreHandle hParserCore, NvS32 rate)
{ 
    NvError status = NvSuccess;
    NvMMM2vParserCoreContext *pContext = (NvMMM2vParserCoreContext *)hParserCore->pContext;

    pContext->rate.Rate = rate;   
    return status;
}

NvS32 M2vCoreParserGetRate(NvMMParserCoreHandle hParserCore)
{ 
    NvMMM2vParserCoreContext *pContext = (NvMMM2vParserCoreContext *)hParserCore->pContext;
    return (pContext->rate.Rate);
}

NvError M2vCoreParserSetPosition(NvMMParserCoreHandle hParserCore, NvU64 *timestamp)
{ 
    NvError status = NvSuccess;
    NvS64 mSec;   
    NvU32 divFactor = 10000;
    NvU64 TotalTime = 0;
    NvS64 TotalTrackTime = 0;
    NvMMM2vParserCoreContext *pContext = (NvMMM2vParserCoreContext *)hParserCore->pContext;

    mSec = ((*timestamp)/divFactor); 

    M2vCoreParserGetTotalTrackTime(hParserCore, &TotalTrackTime);
    TotalTime = ((NvU64)TotalTrackTime * 10000000);

    if(*timestamp > TotalTime)
    {
        return NvError_ParserEndOfStream;
    }
    else if(*timestamp == 0)
    {
        pContext->bSentEOS = NV_FALSE;
    }
    status = NvM2vSeekToTime(pContext->pM2vReaderHandle,mSec);

    pContext->ParserTimeStamp = *timestamp;
    pContext->position.Position = *timestamp;

    return status;
}

NvError M2vCoreParserGetPosition( NvMMParserCoreHandle hParserCore, NvU64 *timestamp)
{
    NvError status = NvSuccess;
    NvMMM2vParserCoreContext *pContext = (NvMMM2vParserCoreContext *)hParserCore->pContext;

    *(timestamp) = pContext->position.Position;
    return status;
}
#if ENABLE_PRE_PARSING
NvError 
M2vCoreParserGetNextWorkUnit(
                             NvMMParserCoreHandle hParserCore, 
                             NvU32 streamIndex, 
                             NvMMBuffer *pBuffer, 
                             NvU32 *size, 
                             NvBool *pMoreWorkPending)
{ 
    NvError status = NvSuccess;
    NvU8 *pData = NULL;
    NvMMM2vParserCoreContext *pContext = (NvMMM2vParserCoreContext *)hParserCore->pContext;
    NvM2vParser *pState=pContext->pM2vReaderHandle;
    PictureList* pPicList=pState->pPicListRunning;
#if ENABLE_PROFILING
    StartTime = NvOsGetTimeMS();
#endif

    // Initially pMoreWorkPending is set to FALSE
    *pMoreWorkPending = NV_FALSE;
    if(pContext->bSentEOS == NV_TRUE)
    {
        pBuffer->Payload.Ref.startOfValidData= 0;
        pBuffer->Payload.Ref.sizeOfValidDataInBytes = 0;
        *size = 0;
        return NvError_ParserEndOfStream;
    }

    if (pBuffer)
    {        
        pBuffer->Payload.Ref.startOfValidData = 0;
        pBuffer->Payload.Ref.sizeOfValidDataInBytes = *size;
        pData = (NvU8 *)pBuffer->Payload.Ref.pMem;


        if( (pPicList!= NULL)  && (FrameNum < pPicList->PictureNum))
        {  // printf("Framenum  %d size %d  ",FrameNum,pPicList->PictureSize[FrameNum]);
            status = pState->pPipe->Read(pState->cphandle,
                (CPbyte*)pData, (CPuint)pPicList->PictureSize[FrameNum]);
            // printf("Framenum  %d read ");
            pBuffer->Payload.Ref.startOfValidData= 0;
            pBuffer->Payload.Ref.sizeOfValidDataInBytes= pPicList->PictureSize[FrameNum];
            *size = pPicList->PictureSize[FrameNum];
            FrameNum++;

            if(FrameNum>=pPicList->PictureNum)
            {
                pState->pPicListRunning=pPicList->pNext;
                FrameNum=0;
                if(pState->pPicListRunning==NULL){pContext->bSentEOS = NV_TRUE;}
            }
        }
#if ENABLE_PROFILING
        FinishTime = NvOsGetTimeMS();
        TotalTimePreParsing += FinishTime - StartTime;
#endif
        return status;
    }
    return NvError_InSufficientBufferSize;
}
#else
NvError 
M2vCoreParserGetNextWorkUnit(
                             NvMMParserCoreHandle hParserCore, 
                             NvU32 streamIndex, 
                             NvMMBuffer *pBuffer, 
                             NvU32 *size, 
                             NvBool *pMoreWorkPending)
{ 
    NvError status = NvSuccess;
    NvU32 askBytes = 0;
    NvU32 pPosition = 0,endposition = 0;
    CP_CHECKBYTESRESULTTYPE eResult = CP_CheckBytesOk;  
    NvU8 *pData = NULL;
    NvU32 ulBytesRead = 0;
    NvMMM2vParserCoreContext *pContext = (NvMMM2vParserCoreContext *)hParserCore->pContext;
    NvM2vParser *pState=pContext->pM2vReaderHandle;
    // Initially pMoreWorkPending is set to FALSE
    *pMoreWorkPending = NV_FALSE;
    if(pContext->bSentEOS == NV_TRUE)
    {
        pBuffer->Payload.Ref.startOfValidData= 0;
        pBuffer->Payload.Ref.sizeOfValidDataInBytes = 0;
        *size = 0;
        return NvError_ParserEndOfStream;
    }
#if ENABLE_PROFILING
    StartTime = NvOsGetTimeMS();
#endif
    if (pBuffer)
    {        
        pBuffer->Payload.Ref.startOfValidData = 0;
        pBuffer->Payload.Ref.sizeOfValidDataInBytes = *size;
        pData = (NvU8 *)pBuffer->Payload.Ref.pMem;
        // Set ask bytes to the preset buffer size
        askBytes = MPEG2V_FRAME_SIZE;

        if( pBuffer->Payload.Ref.sizeOfBufferInBytes < askBytes)
        {
            // returning required minimum size
            *size = askBytes;
            return NvError_InSufficientBufferSize;
        }

        status = pState->pPipe->CheckAvailableBytes(pState->cphandle, MPEG2V_FRAME_SIZE,&eResult);
        if(eResult==CP_CheckBytesInsufficientBytes)
        { 
            status =pState->pPipe->GetPosition(pState->cphandle,(CPuint*) &pPosition);
            if(status != NvSuccess) return NvError_ParserFailure;
            status =pState->pPipe->SetPosition(pState->cphandle, 0, CP_OriginEnd) ;
            if(status != NvSuccess) return NvError_ParserFailure;
            status = pState->pPipe->GetPosition(pState->cphandle, (CPuint*)&endposition);
            if(status != NvSuccess) return NvError_ParserFailure;
            status =pState->pPipe->SetPosition(pState->cphandle, pPosition, CP_OriginBegin);
            if(status != NvSuccess) return NvError_ParserFailure;                
            status = pState->pPipe->Read(pState->cphandle,(CPbyte*)(pData),(CPuint)(endposition - pPosition));
            ulBytesRead = endposition - pPosition;
            pContext->bSentEOS = NV_TRUE;
        }

        if( eResult == CP_CheckBytesOk)
        {  
            status = pState->pPipe->Read(pState->cphandle,
                (CPbyte*)(pData), (CPuint)(MPEG2V_FRAME_SIZE));
            ulBytesRead=MPEG2V_FRAME_SIZE;

        }
        pBuffer->Payload.Ref.startOfValidData= 0;
        pBuffer->Payload.Ref.sizeOfValidDataInBytes= ulBytesRead;
        *size = ulBytesRead;

#if ENABLE_PROFILING
        FinishTime = NvOsGetTimeMS();
        TotalTimeParsing += FinishTime - StartTime;
#endif
        return status;
    }
    return NvError_InSufficientBufferSize;
}

#endif



NvError M2vCoreParserClose(NvMMParserCoreHandle hParserCore)
{
    NvError status = NvSuccess;
    NvMMM2vParserCoreContext *pContext = (NvMMM2vParserCoreContext *)hParserCore->pContext;

    if (pContext)
    {        
        if (pContext->pM2vReaderHandle)
        {
            NvReleaseM2vReaderHandle(pContext->pM2vReaderHandle);
            NvOsFree(pContext->pM2vReaderHandle);
            pContext->pM2vReaderHandle = NULL;
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
#if ENABLE_PROFILING
    NvOsDebugPrintf("PARSING TIME preparsing %d noparsing %d",TotalTimePreParsing,TotalTimeParsing);
#endif
    return status;
}

NvBool  
M2vCoreParserGetBufferRequirements(
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

    pBufReq->minBufferSize = MPEG2V_FRAME_SIZE;
    pBufReq->maxBufferSize = MPEG2V_FRAME_SIZE;     

    return NV_TRUE;
}



void M2vCoreParserGetMaxOffsets(NvMMParserCoreHandle hParserCore, NvU32 *pMaxOffsets)
{
    *pMaxOffsets = 0;
}

NvError 
M2vCoreParserGetAttribute(
                          NvMMParserCoreHandle hParserCore,
                          NvU32 AttributeType, 
                          NvU32 AttributeSize, 
                          void *pAttribute)
{
    NvError status = NvSuccess;
    return status;
}

