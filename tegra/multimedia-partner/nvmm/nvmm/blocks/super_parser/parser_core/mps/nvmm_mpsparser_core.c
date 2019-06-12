/*
* Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
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
#include "nvmm_mpsparser_core.h"
#include "nvlocalfilecontentpipe.h"
#include "nvmm_mps_parser.h"

#define NV_MPSCORE_VIDEO_BUF_SIZE (310*1024) //720*576*3/4
#define NV_MPSCORE_AUDIO_BUF_SIZE (4*1024)

#define NV_MPSCORE_AUDIO_PES_COUNT (1)
#define NV_MPSCORE_VIDEO_PES_COUNT (20)


/* Context for MPEG-PS parser core */
typedef struct
{
    NvString szFilename;
    CPhandle cphandle;
    CP_PIPETYPE *pPipe;
    NvMpsParser *pMpsParser;
    NvBool   bSentEOS;
    NvMMAttrib_ParseRate rate;
    NvMMAttrib_ParsePosition position;
    NvBool   bIsAudioFirstStream;
} NvMMMpsCoreParserContext;

// Prototypes for Ogg implementation of parser functions
NvError MpsCoreParserGetNumberOfStreams(NvMMParserCoreHandle hParserCore, NvU32 *streams);
NvError MpsCoreParserGetStreamInfo(NvMMParserCoreHandle hParserCore, NvMMStreamInfo **pInfo);
NvError MpsCoreParserSetRate(NvMMParserCoreHandle hParserCore, NvS32 rate);
NvS32   MpsCoreParserGetRate(NvMMParserCoreHandle hParserCore);
NvError MpsCoreParserSetPosition(NvMMParserCoreHandle hParserCore, NvU64 *timestamp);
NvError MpsCoreParserGetPosition( NvMMParserCoreHandle hParserCore, NvU64 *timestamp);
NvError MpsCoreParserOpen(NvMMParserCoreHandle hParserCore, NvString pURI);
NvError MpsCoreParserClose(NvMMParserCoreHandle hParserCore);
void    MpsCoreParserGetMaxOffsets(NvMMParserCoreHandle hParserCore, NvU32 *pMaxOffsets);

NvError
MpsCoreParserGetNextWorkUnit(
                             NvMMParserCoreHandle hParserCore,
                             NvU32 streamIndex,
                             NvMMBuffer *pBuffer,
                             NvU32 *size,
                             NvBool *pMoreWorkPending);

NvBool
MpsCoreParserGetBufferRequirements(
                                   NvMMParserCoreHandle hParserCore,
                                   NvU32 StreamIndex,
                                   NvU32 Retry,
                                   NvMMNewBufferRequirementsInfo *pBufReq);

NvError
MpsCoreParserGetAttribute(
                          NvMMParserCoreHandle hParserCore,
                          NvU32 AttributeType,
                          NvU32 AttributeSize,
                          void *pAttribute);

NvError
NvMMCreateMpsCoreParser(
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

    pParserCore->Open               = MpsCoreParserOpen;
    pParserCore->GetNumberOfStreams = MpsCoreParserGetNumberOfStreams;
    pParserCore->GetStreamInfo      = MpsCoreParserGetStreamInfo;
    pParserCore->SetRate            = MpsCoreParserSetRate;
    pParserCore->GetRate            = MpsCoreParserGetRate;
    pParserCore->SetPosition        = MpsCoreParserSetPosition;
    pParserCore->GetPosition        = MpsCoreParserGetPosition;
    pParserCore->GetNextWorkUnit    = MpsCoreParserGetNextWorkUnit;
    pParserCore->Close              = MpsCoreParserClose;
    pParserCore->GetMaxOffsets      = MpsCoreParserGetMaxOffsets;
    pParserCore->GetAttribute       = MpsCoreParserGetAttribute;
    pParserCore->GetBufferRequirements = MpsCoreParserGetBufferRequirements;

    pParserCore->eCoreType          = NvMMParserCoreType_MpsParser;
    pParserCore->UlpKpiMode         = pParams->UlpKpiMode;
    pParserCore->bEnableUlpMode     = pParams->bEnableUlpMode;
    pParserCore->bReduceVideoBuffers= pParams->bReduceVideoBuffers;

    return NvSuccess;
}

void NvMMDestroyMpsCoreParser(NvMMParserCoreHandle hParserCore)
{
    if (hParserCore)
    {
        NvOsFree(hParserCore);
    }
}

NvError MpsCoreParserOpen(NvMMParserCoreHandle hParserCore, NvString pFilename)
{
    NvError status = NvSuccess;
    NvU32 StringLength = 0;
    //NvU32 streams;
    NvMMMpsCoreParserContext *pContext=NULL;
    NvMpsParser *pMpsParser;

    if ((pContext = NvOsAlloc(sizeof(NvMMMpsCoreParserContext))) == NULL)
    {
        status = NvError_InsufficientMemory;
        goto MpsCoreParserOpen_MemFailure;
    }
    NvOsMemset(pContext, 0, sizeof(NvMMMpsCoreParserContext));

    StringLength = NvOsStrlen(pFilename) + 1;
    pContext->szFilename = NvOsAlloc(StringLength);
    if (pContext->szFilename == NULL)
    {
        status = NvError_InsufficientMemory;
        goto MpsCoreParserOpen_MemFailure;
    }
    NvOsStrncpy(pContext->szFilename, pFilename, StringLength);

    pContext->pMpsParser = NvMpsParserCreate(NV_MPS_MAX_SCRATCH_SIZE);
    if (pContext->pMpsParser == NULL)
    {
        NvOsDebugPrintf("MpegPs Parser: pContext->pMpsParser Creation Failed. Out of memory?\n");
        status = NvError_InsufficientMemory;
        goto MpsCoreParserOpen_MemFailure;
    }
    pMpsParser = pContext->pMpsParser;

    NvMMGetContentPipe((CP_PIPETYPE_EXTENDED**)&pContext->cphandle);
    pContext->pPipe = (CP_PIPETYPE *)pContext->cphandle;
    pContext->cphandle = 0;

    if (pContext->pPipe == NULL)
    {
        status =  NvError_ParserOpenFailure;
        goto MpsCoreParserOpen_MemFailure;
    }
    status = pContext->pPipe->Open(&pContext->cphandle, pFilename,CP_AccessRead);
    if (status!= NvSuccess)
    {
        status =  NvError_ParserOpenFailure;
        goto MpsCoreParserOpen_MemFailure;
    }

    status = NvMpsParserInit(pMpsParser, pContext->pPipe, pContext->cphandle);
    if ((status != NvSuccess) || (pMpsParser->uNumStreams == 0))
    {
        NvOsDebugPrintf("MpegPs Parser: pMpsParser Init Failed\n");
        status =  NvError_ParserOpenFailure;
        goto MpsCoreParserOpen_InitFailure;
    }

    /* Fail if video resolution is more than SD resolution.
     * See bug 634827
     */
    if (pMpsParser->pAvInfo->bHasVideo)
    {
        if ((pMpsParser->pAvInfo->uWidth > 720) ||
            (pMpsParser->pAvInfo->uHeight > 576))
        {
            NvOsDebugPrintf("MpegPs Parser: Video resolution more than 720x576 is not supported\n");
            status =  NvError_ParserOpenFailure;
            goto MpsCoreParserOpen_InitFailure;
        }
    }

    if (pMpsParser->pAvInfo->bHasAudio)
    {
        pContext->bIsAudioFirstStream = NV_TRUE;
    }
    else
    {
        pContext->bIsAudioFirstStream = NV_FALSE;
    }

    pContext->rate.Rate  = 1000;
    pContext->bSentEOS = NV_FALSE;
    hParserCore->pContext = (void*)pContext;

    return status;

MpsCoreParserOpen_InitFailure:
   if (pContext && pContext->pPipe && pContext->cphandle)
    {
        pContext->pPipe->Close(pContext->cphandle);
        pContext->cphandle = 0;
    }
MpsCoreParserOpen_MemFailure:
   if (pContext)
   {
       if (pContext->pMpsParser)
       {
            NvMpsParserDestroy(pContext->pMpsParser);
            pContext->pMpsParser = NULL;
       }
       if (pContext->szFilename)
       {
           NvOsFree(pContext->szFilename);
           pContext->szFilename = NULL;
       }
       NvOsFree(pContext);
       pContext = NULL;
   }
   return status;
}

NvError MpsCoreParserClose(NvMMParserCoreHandle hParserCore)
{
    NvError status = NvSuccess;
    NvMMMpsCoreParserContext *pContext = (NvMMMpsCoreParserContext *)hParserCore->pContext;

    if (pContext)
    {
       if (pContext->pPipe && pContext->cphandle)
        {
            pContext->pPipe->Close(pContext->cphandle);
            pContext->cphandle = 0;
        }
        if (pContext->pMpsParser)
        {
            NvMpsParserDestroy(pContext->pMpsParser);
            pContext->pMpsParser = NULL;
        }
        if (pContext->szFilename)
        {
            NvOsFree(pContext->szFilename);
            pContext->szFilename = NULL;
        }
        NvOsFree(pContext);
    }

    return status;
}

NvError MpsCoreParserGetNumberOfStreams(NvMMParserCoreHandle hParserCore, NvU32 *streams)
{
    NvError status = NvSuccess;
    NvMMMpsCoreParserContext *pContext = (NvMMMpsCoreParserContext *)hParserCore->pContext;

    *streams = pContext->pMpsParser->uNumStreams;
    return status;
}

NvError MpsCoreParserGetStreamInfo(NvMMParserCoreHandle hParserCore, NvMMStreamInfo **pStreamInfo)
{
    NvError status = NvSuccess;
    NvMMMpsCoreParserContext *pContext = (NvMMMpsCoreParserContext *)hParserCore->pContext;
    NvMMStreamInfo *pInfo = *pStreamInfo;
    NvU32 uVideoIndx = 0;
    NvMpsParserAvInfo *pAvInfo = pContext->pMpsParser->pAvInfo;
    NvU64 lTotalTime = ((pContext->pMpsParser->lDuration) * 10000)/9;

    if (pContext->bIsAudioFirstStream)
    {
        if (pAvInfo->eAudioType == NV_MPS_AUDIO_MP2)
        {
            pInfo[0].StreamType                                = NvMMStreamType_MP2;
        }
        else if (pAvInfo->eAudioType == NV_MPS_AUDIO_MP3)
        {
            pInfo[0].StreamType                                = NvMMStreamType_MP3;
        }
        else
        {
            pInfo[0].StreamType                                = NvMMStreamType_UnsupportedAudio;
        }
        pInfo[0].BlockType                                 = NvMMBlockType_SuperParser;
        pInfo[0].TotalTime                                 = lTotalTime;
        pInfo[0].NvMMStream_Props.AudioProps.SampleRate    = pAvInfo->uSamplingFreq;
        pInfo[0].NvMMStream_Props.AudioProps.BitRate       = pAvInfo->uAudioBitRate;
        pInfo[0].NvMMStream_Props.AudioProps.NChannels     = pAvInfo->uNumChannels;
        pInfo[0].NvMMStream_Props.AudioProps.BitsPerSample = pAvInfo->uBitsPerSample;
        uVideoIndx = 1;
    }

    if (pAvInfo->bHasVideo)
    {
        pInfo[uVideoIndx].StreamType                                = NvMMStreamType_MPEG2V;
        pInfo[uVideoIndx].BlockType                                 = NvMMBlockType_SuperParser;
        pInfo[uVideoIndx].TotalTime                                 = lTotalTime;
        pInfo[uVideoIndx].NvMMStream_Props.VideoProps.Width         = pAvInfo->uWidth;
        pInfo[uVideoIndx].NvMMStream_Props.VideoProps.Height        = pAvInfo->uHeight;
        pInfo[uVideoIndx].NvMMStream_Props.VideoProps.Fps           = pAvInfo->uFps;
        pInfo[uVideoIndx].NvMMStream_Props.VideoProps.VideoBitRate  = 256*1024; //TODO: Calculate exact value
    }

    return status;
}

NvBool
MpsCoreParserGetBufferRequirements(
                                   NvMMParserCoreHandle hParserCore,
                                   NvU32 StreamIndex,
                                   NvU32 Retry,
                                   NvMMNewBufferRequirementsInfo *pBufReq)
{
    NvMMMpsCoreParserContext *pContext = (NvMMMpsCoreParserContext *)hParserCore->pContext;

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


    if (pContext->bIsAudioFirstStream && StreamIndex == 0)
    {
        pBufReq->minBufferSize = NV_MPSCORE_AUDIO_BUF_SIZE;
        pBufReq->maxBufferSize = NV_MPSCORE_AUDIO_BUF_SIZE;
    }
    else
    {
        pBufReq->minBufferSize = NV_MPSCORE_VIDEO_BUF_SIZE;
        pBufReq->maxBufferSize = NV_MPSCORE_VIDEO_BUF_SIZE;
    }

    return NV_TRUE;
}

NvError MpsCoreParserSetRate(NvMMParserCoreHandle hParserCore, NvS32 rate)
{
    NvError status = NvSuccess;
    NvMMMpsCoreParserContext *pContext = (NvMMMpsCoreParserContext *)hParserCore->pContext;

    pContext->rate.Rate = rate;
    return status;
}

NvS32 MpsCoreParserGetRate(NvMMParserCoreHandle hParserCore)
{
    NvMMMpsCoreParserContext *pContext = (NvMMMpsCoreParserContext *)hParserCore->pContext;
    return (pContext->rate.Rate);
}

NvError MpsCoreParserSetPosition(NvMMParserCoreHandle hParserCore, NvU64 *timestamp)
{
    NvError status = NvSuccess;
    return status;
}

NvError MpsCoreParserGetPosition( NvMMParserCoreHandle hParserCore, NvU64 *timestamp)
{
    NvError status = NvSuccess;
    return status;
}

void MpsCoreParserGetMaxOffsets(NvMMParserCoreHandle hParserCore, NvU32 *pMaxOffsets)
{
    *pMaxOffsets = 0;
}

NvError
MpsCoreParserGetAttribute(
                          NvMMParserCoreHandle hParserCore,
                          NvU32 AttributeType,
                          NvU32 AttributeSize,
                          void *pAttribute)
{
    NvError status = NvSuccess;
    return status;
}

NvError
MpsCoreParserGetNextWorkUnit(
                             NvMMParserCoreHandle hParserCore,
                             NvU32 streamIndex,
                             NvMMBuffer *pBuffer,
                             NvU32 *size,
                             NvBool *pMoreWorkPending)
{
    NvError status = NvSuccess;
    NvU32 askBytes = 0;
    eNvMpsMediaType eMediaType;
    NvU8 *pData = NULL;
    //NvU32 ulBytesRead = 0;
    NvMMMpsCoreParserContext *pContext = (NvMMMpsCoreParserContext *)hParserCore->pContext;
    NvMpsParser *pMpsParser = pContext->pMpsParser;
    NvU32 uCount = 0;
    NvU32 uTotalBytes;
    NvU32 uCountLimit;

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

        uCount = 0;
        uTotalBytes = 0;

        if ((streamIndex == 0) && (pContext->bIsAudioFirstStream))
        {
            uCountLimit = NV_MPSCORE_AUDIO_PES_COUNT;
            eMediaType = NV_MPS_STREAM_AUDIO;
        }
        else
        {
            uCountLimit = NV_MPSCORE_VIDEO_PES_COUNT;
            eMediaType = NV_MPS_STREAM_VIDEO;
        }
        while(uCount < uCountLimit)
        {
            uCount++;

            status = NvMpsParserGetNextPESInfo(pMpsParser, &eMediaType, &askBytes);
            if (status != NvSuccess)
            {
                if (pMpsParser->bAnyEOS)
                {
                    pContext->bSentEOS = NV_TRUE;
                    status = NvError_ParserEndOfStream;
                }
                pBuffer->Payload.Ref.startOfValidData= 0;
                pBuffer->Payload.Ref.sizeOfValidDataInBytes = 0;
                *size = 0;
                return status;
            }

            if( pBuffer->Payload.Ref.sizeOfBufferInBytes <= (uTotalBytes+askBytes))
            {
                // returning required minimum size
                *size = askBytes;
                NvOsDebugPrintf("MPEG-PS parser error: InSufficientBufferSize\n");
                return NvError_InSufficientBufferSize;
            }

            status = NvMpsParserGetNextPESData(pMpsParser, &eMediaType, pData+uTotalBytes, &askBytes);
            if (status != NvSuccess)
            {
                if (pMpsParser->bAnyEOS)
                {
                    pContext->bSentEOS = NV_TRUE;
                    status = NvError_ParserEndOfStream;
                }
                pBuffer->Payload.Ref.startOfValidData= 0;
                pBuffer->Payload.Ref.sizeOfValidDataInBytes = 0;
                *size = 0;
                return status;
            }
            uTotalBytes += askBytes;
        }

        pBuffer->Payload.Ref.startOfValidData= 0;
        pBuffer->Payload.Ref.sizeOfValidDataInBytes= uTotalBytes;
        *size = uTotalBytes;

        return status;
    }

    return NvError_InSufficientBufferSize;
}
