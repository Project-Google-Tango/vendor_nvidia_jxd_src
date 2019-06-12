/* Copyright (c) 2006 - 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property 
 * and proprietary rights in and to this software, related documentation 
 * and any modifications thereto.  Any use, reproduction, disclosure or 
 * distribution of this software and related documentation without an 
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

#include "common/NvxComponent.h"
#include "common/NvxIndexExtensions.h"
#include "common/NvxTrace.h"
#include "nvos.h"
#include "nvassert.h"

#include "nvmm.h"
#include "../nvmm/include/nvmm_bufferprofile.h"

#include "nvmm/components/common/nvmmtransformbase.h"
#include "nvmm_writer.h"

#if NVMM_BUFFER_PROFILING_ENABLED
#include "nvmm_util.h"
#endif

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */

#define NVX_3GP_VIDEO_TRACK 0
#define NVX_3GP_AUDIO_TRACK 1
#define INVALID_TRACK 0xFFFFFFFF
#define MAX_TRACKS 2
#define NVX_3GP_WORK_BUFFER_SIZE (400*1024)

typedef struct {
    OMX_STRING pFilename;
    OMX_STRING pTempFilePath;
    OMX_BOOL   bSentEOS;

    NvMMBlockType oBlockType;
    SNvxNvMMTransformData o3gpWriter;

    NvU8 nAudioSampleRateIndex;
    OMX_U32 nAudioBitRate;
    OMX_U32 nAudioChannels;
    OMX_U32 nMinBitRate;
    OMX_U32 nMaxBitRate;
    OMX_AUDIO_CDMARATETYPE eCDMARate;
    NvU64 nFileSize;
    NvU64 nDuration;

    OMX_AUDIO_AMRBANDMODETYPE eAMRBandMode;
    OMX_AUDIO_AACPROFILETYPE eAACProfile;

    // per track info
    OMX_TICKS   oMaxDuration;
    OMX_TICKS   oNewPosPeriod;
    OMX_TICKS   oLastPos;
    OMX_TICKS   oLastPosSent;
    OMX_TICKS   oRunningDuration[2];
    OMX_BOOL    bEOS[2];

    OMX_BOOL   b3gpInitialized;
    int nAudioFrames;
    int nVideoFrames;

    NvU32 nDesiredFramesAudio;
    NvU32 nDesiredFramesVideo;

    OMX_STRING pOutPutFormat;

    // Profiling information: Can be removed once 3GP writer becomes a NvMM block
#if NVMM_BUFFER_PROFILING_ENABLED
    NvMMBufferProfilingData Audio3GPMux[1];
    NvU32   AudProfNumBufferSent;
    NvU32   AudProfNumBufferReceived;
    NvU32   AudProfNumBufferProcessed;

    NvMMBufferProfilingData Video3GPMux[1];
    NvU32   VidProfNumBufferSent;
    NvU32   VidProfNumBufferReceived;
    NvU32   VidProfNumBufferProcessed;
#endif

} NvxMp4WriterState;

#define FILE_WRITER_BUFFER_COUNT 5
#define FILE_WRITER_BUFFER_SIZE  1024

static int s_nvx_port_audio            = -1;
static int s_nvx_port_video            = -1;

#define AAC_MAX_BITRATE  320000
#define AMRNB_MAX_BITRATE 12200
#define AMRWB_MAX_BITRATE 23850

/* AAC Object types look up for 3gp writer*/
enum { OBJECT_TYPE_AAC_LC = 2 };
enum { OBJECT_TYPE_AAC_PLUS = 5 };
/* Init functions */
OMX_ERRORTYPE NvxMp4FileWriterInit(OMX_IN OMX_HANDLETYPE hComponent);

/* =========================================================================
 *                     F U N C T I O N S
 * ========================================================================= */

static OMX_ERRORTYPE NvxMp4FileWriterDeInit(NvxComponent* pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxMp4WriterState *pState = pNvComp->pComponentData;
   
    NVXTRACE((NVXT_CALLGRAPH,pNvComp->eObjectType,"%s\n","--NvxMp4FileWriterDeInit"));

    if (pState) 
    {
        NvOsFree(pState->pFilename);
        NvOsFree(pState->pTempFilePath);
        NvOsFree(pState->pOutPutFormat);
        NvOsFree(pState);
    }
    return eError;
}

static NvU8 GetAACCodecSampleRateIndex(NvU32 SampleRate)
{
    // Supported sampling rates for AAC encoding
    NvU32 AacSamplingRates[] =
    {
        96000, 88200, 64000, 48000, 44100, 32000,
        24000, 22050, 16000, 12000, 11025, 8000
    };

    NvU16 NumElements = sizeof(AacSamplingRates) / sizeof(AacSamplingRates[0]);
    NvU8 i;

    for (i = 0; i < NumElements; ++i)
    {
        if (AacSamplingRates[i] == SampleRate)
        {
            break;
        }
    }

    if (i == NumElements)
        i = 11;   //default

    return i;
}

#if 0   // keep code here for information
static NvU16 GetAMRCodecParams(NvU32 BitRate)
{
    // Supported bit rates for AMRNB encoding
    NvU32 BitRates_AMRNB[] =
        {4750, 5150, 5900, 6700, 7400, 7950, 10200, 12200};

    NvU32 NumElements = sizeof(BitRates_AMRNB) / sizeof(BitRates_AMRNB[0]);
    NvU8 i;

    for (i = 0; i < NumElements; ++i)
    {
        if (BitRates_AMRNB[i] == BitRate)
        {
            break;
        }
    }

    if (i == NumElements)
        i = 7;   //default

    return i;
}
#endif
static NvU32 GetAmrBitRate(OMX_AUDIO_AMRBANDMODETYPE eAMRBandMode)
{
    // Supported bit rates for AMRNB encoding
    NvU32 BitRates_AMRWB[] =
        {0, 4750, 5150, 5900, 6700, 7400, 7950, 10200, 12200,
        6600, 8850, 12650, 14250, 15850, 18250, 19850, 23050, 23850};

    return BitRates_AMRWB[eAMRBandMode];
}

static OMX_ERRORTYPE NvxMp4FileWriterGetParameter(
    OMX_IN NvxComponent *pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_INOUT OMX_PTR pParam)
{
    //NvxMp4WriterState *pState = pNvComp->pComponentData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH,pNvComp->eObjectType,"%s\n","--NvxMp4FileWriterGetParameter"));

    switch (nIndex)
    {
    case OMX_IndexParamAudioAac:
        break;

    default:
        eError = NvxComponentBaseGetParameter(pNvComp, nIndex, pParam);
    }

    return eError;
}

static OMX_ERRORTYPE NvxMp4FileWriterSetParameter(
    OMX_IN NvxComponent *pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pParam)
{
    NvxMp4WriterState *pState = pNvComp->pComponentData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH,pNvComp->eObjectType,"%s\n","--NvxMp4FileWriterSetParameter"));

    switch (nIndex)
    {
        case NVX_IndexParamTempFilePath: 
        {
            NVX_PARAM_TEMPFILEPATH *pTempFilePathParam = pParam;

            NvOsFree(pState->pTempFilePath);
            pState->pTempFilePath = NvOsAlloc(NvOsStrlen(pTempFilePathParam->pTempPath) + 1);
            if(!pState->pTempFilePath)
            {
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxMp4FileWriterSetParameter:"
                    ":pTempFilePath = %s:[%s(%d)]\n",pState->pTempFilePath
                    , __FILE__, __LINE__));
                return OMX_ErrorInsufficientResources; 
            }
            NvOsStrncpy(pState->pTempFilePath, pTempFilePathParam->pTempPath, NvOsStrlen(pTempFilePathParam->pTempPath) + 1);
            break;
        }
        case NVX_IndexParamFilename: 
        {
            NVX_PARAM_FILENAME *pFilenameParam = pParam;

            NvOsFree(pState->pFilename);
            pState->pFilename = NvOsAlloc(NvOsStrlen(pFilenameParam->pFilename) + 1);
            if(!pState->pFilename)
            {
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxMp4FileWriterSetParameter:"
                    ":pFilename = %s:[%s(%d)]\n",pState->pFilename
                    , __FILE__, __LINE__));
                return OMX_ErrorInsufficientResources; 
            }
            NvOsStrncpy(pState->pFilename, pFilenameParam->pFilename, NvOsStrlen(pFilenameParam->pFilename) + 1);
            break;
        }

        case OMX_IndexParamAudioAac:
        {
            OMX_AUDIO_PARAM_AACPROFILETYPE *pAacProfile = (OMX_AUDIO_PARAM_AACPROFILETYPE *)pParam;
            pState->nAudioBitRate = pAacProfile->nBitRate;
            pState->nAudioChannels = pAacProfile->nChannels;
            pState->eAACProfile = pAacProfile->eAACProfile;
            if((pState->eAACProfile == OMX_AUDIO_AACObjectHE) || (pState->eAACProfile == OMX_AUDIO_AACObjectHE_PS))
            {
                pState->nAudioSampleRateIndex = GetAACCodecSampleRateIndex(pAacProfile->nSampleRate >> 1);
                if(pState->eAACProfile == OMX_AUDIO_AACObjectHE_PS)
                {
                    pState->nAudioChannels = 1;
                }
            }
            else
            {
                pState->nAudioSampleRateIndex = GetAACCodecSampleRateIndex(pAacProfile->nSampleRate);
            }
            break;
        }

        case OMX_IndexParamAudioAmr:
        {
            OMX_AUDIO_PARAM_AMRTYPE *pAmrType = (OMX_AUDIO_PARAM_AMRTYPE *)pParam;
            pState->eAMRBandMode = pAmrType->eAMRBandMode;
            pState->nAudioBitRate = GetAmrBitRate(pState->eAMRBandMode);
            pState->nAudioChannels = pAmrType->nChannels;
            break;
        }
        case OMX_IndexParamAudioQcelp8:
        {
            OMX_AUDIO_PARAM_QCELP8TYPE *pQcelp8Type = (OMX_AUDIO_PARAM_QCELP8TYPE *)pParam;
            pState->nAudioChannels = pQcelp8Type->nChannels;
            pState->nAudioBitRate = pQcelp8Type->nBitRate;
            pState->nMinBitRate = pQcelp8Type->nMinBitRate;
            pState->nMaxBitRate = pQcelp8Type->nMaxBitRate;
            pState->eCDMARate = pQcelp8Type->eCDMARate;
            break;
        }
        case OMX_IndexParamAudioQcelp13:
        {
            OMX_AUDIO_PARAM_QCELP13TYPE *pQcelp13Type = (OMX_AUDIO_PARAM_QCELP13TYPE *)pParam;
            pState->nAudioChannels = pQcelp13Type->nChannels;
            pState->nMinBitRate = pQcelp13Type->nMinBitRate;
            pState->nMaxBitRate = pQcelp13Type->nMaxBitRate;
            pState->eCDMARate = pQcelp13Type->eCDMARate;
            break;
        }

        case NVX_IndexParamMaxFrames:
        {
            OMX_PARAM_U32TYPE *pMaxFrames = (OMX_PARAM_U32TYPE *)pParam;
            if (NVX_3GP_AUDIO_TRACK == pMaxFrames->nPortIndex)
                pState->nDesiredFramesAudio = (NvU32)(pMaxFrames->nU32);
            else if (NVX_3GP_VIDEO_TRACK == pMaxFrames->nPortIndex)
                pState->nDesiredFramesVideo = (NvU32)(pMaxFrames->nU32);
            break;
        }

        case NVX_IndexParamOutputFormat:
        {
            NVX_PARAM_OUTPUTFORMAT *pFormat = (NVX_PARAM_OUTPUTFORMAT *)pParam;

            NvOsFree(pState->pOutPutFormat);
            pState->pOutPutFormat = NvOsAlloc(NvOsStrlen(pFormat->OutputFormat) + 1);
            if(!pState->pOutPutFormat)
            {
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxMp4FileWriterSetParameter:"
                    ":pOutPutFormat = %s:[%s(%d)]\n",pState->pOutPutFormat
                    , __FILE__, __LINE__));
                return OMX_ErrorInsufficientResources;
            }
            NvOsStrncpy(pState->pOutPutFormat, pFormat->OutputFormat, NvOsStrlen(pFormat->OutputFormat) + 1);
            break;
        }

        case NVX_IndexParamWriterFileSize:
        {
            NVX_PARAM_WRITERFILESIZE *pFileSize = (NVX_PARAM_WRITERFILESIZE *)pParam;
            pState->nFileSize = pFileSize->nFileSize;
            break;
        }
        case NVX_IndexParamDuration:
        {
            NVX_PARAM_DURATION *pDur = (NVX_PARAM_DURATION *)pParam;
            pState->nDuration = pDur->nDuration;
            break;
        }
        default:
            eError = NvxComponentBaseSetParameter(pNvComp, nIndex, pParam);
            break;
    }

    return eError;
}

static OMX_ERRORTYPE NvxMp4FileWriterGetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    NV_ASSERT(pNvComp && pComponentConfigStructure);
       
    NVXTRACE((NVXT_CALLGRAPH,pNvComp->eObjectType,"%s\n","--NvxMp4FileWriterGetConfig"));

    switch ( nIndex )
    {
        case NVX_IndexConfigH264NALLen:
        {
            NVX_VIDEO_CONFIG_H264NALLEN *pNalLen = 
                (NVX_VIDEO_CONFIG_H264NALLEN *)pComponentConfigStructure;
            pNalLen->nNalLen = 4;

            break;
        }

        case NVX_IndexConfig3GPMuxGetAVRecFrames:
        {
            NVX_CONFIG_3GPMUXGETAVRECFRAMES* pAVRecordedFrames = (NVX_CONFIG_3GPMUXGETAVRECFRAMES *) pComponentConfigStructure;
            NvxMp4WriterState *pState = pNvComp->pComponentData;
            {
                NvMMWriterAttrib_FrameCount oFrameCount;

                pState->o3gpWriter.hBlock->GetAttribute(pState->o3gpWriter.hBlock,
                                                        NvMMWriterAttribute_FrameCount,
                                                        sizeof(NvMMWriterAttrib_FrameCount),
                                                        &oFrameCount);
                pState->nAudioFrames = (int)oFrameCount.audioFrameCount;
                pState->nVideoFrames = (int)oFrameCount.videoFrameCount;
            }

            pAVRecordedFrames->nAudioFrames = pState->nAudioFrames;
            pAVRecordedFrames->nVideoFrames = pState->nVideoFrames;

            break;
        }

        case NVX_IndexConfigGetNVMMBlock:
        {
            NvxMp4WriterState *pState = pNvComp->pComponentData;
            NVX_CONFIG_GETNVMMBLOCK *pGetBlock = (NVX_CONFIG_GETNVMMBLOCK *)pComponentConfigStructure;

            if (!pState->b3gpInitialized)
                return OMX_ErrorNotReady;

            if (pGetBlock->nPortIndex != (NvU32)NVX_3GP_AUDIO_TRACK && pGetBlock->nPortIndex != (NvU32)NVX_3GP_VIDEO_TRACK)
                return OMX_ErrorBadParameter;

            pGetBlock->pNvMMTransform = &pState->o3gpWriter;

            if (pGetBlock->nPortIndex == (NvU32)NVX_3GP_AUDIO_TRACK)
                pGetBlock->nNvMMPort = s_nvx_port_audio;
            else if (pGetBlock->nPortIndex == (NvU32)NVX_3GP_VIDEO_TRACK)
                pGetBlock->nNvMMPort = s_nvx_port_video;

            break;
        }

        default: 
            return NvxComponentBaseGetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxMp4FileWriterSetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    NvMMWriterAttrib_SplitTrack SplitTrackFile;    
    NvError status = NvSuccess;
    NvxMp4WriterState *pState = pNvComp->pComponentData;

    NV_ASSERT(pNvComp && pComponentConfigStructure);
    NVXTRACE((NVXT_CALLGRAPH,pNvComp->eObjectType,"%s\n","--NvxMp4FileWriterSetConfig"));
    NvOsMemset(&SplitTrackFile, 0, sizeof(NvMMWriterAttrib_SplitTrack));
    SplitTrackFile.reInit = NV_TRUE;

    switch ( nIndex )
    {
        case NVX_IndexConfigSplitFilename:
        {
            NVX_PARAM_FILENAME *pFilenameParam = pComponentConfigStructure;
            NvOsFree(pState->pFilename);
            pState->pFilename = NvOsAlloc(NvOsStrlen(pFilenameParam->pFilename) + 1);
            NvOsStrncpy(pState->pFilename, pFilenameParam->pFilename, NvOsStrlen(pFilenameParam->pFilename) + 1);
            SplitTrackFile.fileName = pState->pFilename; 
            status = pState->o3gpWriter.hBlock->SetAttribute(pState->o3gpWriter.hBlock,
                                                             NvMMWriterAttribute_SplitTrack,
                                                             NvMMSetAttrFlag_Notification,
                                                             sizeof(NvMMWriterAttribute_SplitTrack)+NvOsStrlen(SplitTrackFile.fileName),
                                                             &SplitTrackFile);
            if (status != NvSuccess)
            {
                NvOsDebugPrintf("NvxMp4FileWriterOpen: Error setting output filename\n");
                return OMX_ErrorBadParameter;
            }
            NvOsSemaphoreWait(pState->o3gpWriter.SetAttrDoneSema);
        }
        default:
            break;
    }
    return OMX_ErrorNone;
}

static void NvxMp4FileWriterBlockEventHandler(NvxComponent *pNvComp, NvU32 eventType, NvU32 eventSize, void *eventInfo)
{
    NvxMp4WriterState *pState = 0;

    NV_ASSERT(pNvComp);
    NV_ASSERT(pNvComp->pComponentData);

    pState = (NvxMp4WriterState *)pNvComp->pComponentData;

    switch (eventType)
    {
        case NvMMEvent_BlockStreamEnd:
        {
            NvMMEventBlockStreamEndInfo *pBlockStreamEndInfo = (NvMMEventBlockStreamEndInfo *)eventInfo;
            int nOMXPortId = NVX_3GP_VIDEO_TRACK;

            if (s_nvx_port_video == (int)pBlockStreamEndInfo->streamIndex)
                nOMXPortId = NVX_3GP_VIDEO_TRACK;
            else if (s_nvx_port_audio == (int)pBlockStreamEndInfo->streamIndex)
                nOMXPortId = NVX_3GP_AUDIO_TRACK;
            else
                break;

            NvxSendEvent(pNvComp, OMX_EventBufferFlag,
                         pNvComp->pPorts[nOMXPortId].oPortDef.nPortIndex,
                         OMX_BUFFERFLAG_EOS, 0);
            break;
        }

        default:
            break;
    }
}

static OMX_ERRORTYPE NvxMp4FileWriterWorkerFunction (
    OMX_IN NvxComponent *pNvComp,
    OMX_BOOL bAllPortsReady,   
    OMX_OUT OMX_BOOL *pbMoreWork, 
    OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxMp4WriterState *pState = 0;
    NvxPort *pPortAudio = &(pNvComp->pPorts[NVX_3GP_AUDIO_TRACK]);
    NvxPort *pPortVideo = &(pNvComp->pPorts[NVX_3GP_VIDEO_TRACK]);
    OMX_BOOL bWouldFailAudio = OMX_TRUE;
    OMX_BOOL bWouldFailVideo = OMX_TRUE;

    NVXTRACE((NVXT_WORKER, pNvComp->eObjectType, "+NvxMp4FileWriterWorkerFunction\n"));

    pState = (NvxMp4WriterState *)pNvComp->pComponentData;

    *pbMoreWork = (pPortAudio->pCurrentBufferHdr != NULL) || (pPortVideo->pCurrentBufferHdr != NULL);

    if (!*pbMoreWork)
        return OMX_ErrorNone;

    eError = NvxNvMMTransformWorkerBase(&pState->o3gpWriter, s_nvx_port_audio,
                                        &bWouldFailAudio);

    if (!pPortAudio->pCurrentBufferHdr)
        NvxPortGetNextHdr(pPortAudio);

    eError = NvxNvMMTransformWorkerBase(&pState->o3gpWriter, s_nvx_port_video,
                                        &bWouldFailVideo);

    if (!pPortVideo->pCurrentBufferHdr)
        NvxPortGetNextHdr(pPortVideo);

    *pbMoreWork = (pPortAudio->pCurrentBufferHdr != NULL || pPortVideo->pCurrentBufferHdr != NULL)
                  && !bWouldFailAudio && !bWouldFailVideo;

    return eError;
}

static OMX_ERRORTYPE NvxMp4FileWriterOpen(OMX_IN NvxComponent *pNvComp )
{
    OMX_ERRORTYPE eError;
    NvxMp4WriterState *pState = pNvComp->pComponentData;
    NvxPort *pPortAudio = &(pNvComp->pPorts[NVX_3GP_AUDIO_TRACK]);
    NvxPort *pPortVideo = &(pNvComp->pPorts[NVX_3GP_VIDEO_TRACK]);
    NvU32 nStreamCount = 0;
    NvError status = NvSuccess;
    NvMMWriterAttrib_FileName fileParams;
    OMX_PARAM_PORTDEFINITIONTYPE oPortDefinition;
    NvMMWriterStreamConfig StreamConfig;
    NvMMWriterAttrib_TempFilePath AttribTempFilePath;

    NVXTRACE((NVXT_CALLGRAPH,pNvComp->eObjectType,"%s\n","--NvxMp4FileWriterOpen"));

    if (!pState->pFilename)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxMp4FileWriterOpen:"
               ":pFilename = %s:[%s(%d)]\n",pState->pFilename , __FILE__, __LINE__));
        return OMX_ErrorInvalidState;
    }

    eError = NvxComponentBaseAcquireResources(pNvComp);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxMp4FileWriterOpen:"
               ":eError = %x:[%s(%d)]\n",eError , __FILE__, __LINE__));
        return eError;
    }

    pState->b3gpInitialized = OMX_FALSE;
    pState->bEOS[NVX_3GP_VIDEO_TRACK] = OMX_FALSE;
    pState->bEOS[NVX_3GP_AUDIO_TRACK] = OMX_FALSE;
    pState->bSentEOS = OMX_FALSE;

    s_nvx_port_audio = -1;
    s_nvx_port_video = -1;
    if (pPortAudio->oPortDef.bEnabled)
    {
        nStreamCount++;
        s_nvx_port_audio = (int)nStreamCount - 1;
    }
    if (pPortVideo->oPortDef.bEnabled)
    {
        nStreamCount++;
        s_nvx_port_video = (int)nStreamCount - 1;
    }

    pState->o3gpWriter.nForceLocale = 1;
    eError = NvxNvMMTransformOpen(&pState->o3gpWriter, NvMMBlockType_3gppWriter, "3gpWriter", nStreamCount, pNvComp);
    if (eError != OMX_ErrorNone)
        return eError;
    //pState->o3gpWriter.bWantTunneledAlloc = OMX_TRUE;

    fileParams.szURI = pState->pFilename;
    status = pState->o3gpWriter.hBlock->SetAttribute(pState->o3gpWriter.hBlock,
                                                     NvMMWriterAttribute_FileName,
                                                     NvMMSetAttrFlag_Notification,
                                                     NvOsStrlen(fileParams.szURI),
                                                     &fileParams);
    if (status != NvSuccess)
    {
        NvOsDebugPrintf("NvxMp4FileWriterOpen: Error setting output filename\n");
        return OMX_ErrorBadParameter;
    }
    NvOsSemaphoreWait(pState->o3gpWriter.SetAttrDoneSema);


    status = pState->o3gpWriter.hBlock->SetAttribute(pState->o3gpWriter.hBlock, 
                                                     NvMMWriterAttribute_StreamCount,
                                                     NvMMSetAttrFlag_Notification,
                                                     sizeof(NvMMWriterAttrib_StreamCount),
                                                     &nStreamCount);
    if (status != NvSuccess)
    {
        NvOsDebugPrintf("NvxMp4FileWriterOpen: Error setting output stream count\n");
        return OMX_ErrorBadParameter;
    }
    NvOsSemaphoreWait(pState->o3gpWriter.SetAttrDoneSema);


    if(pState->pTempFilePath)
    {
        AttribTempFilePath.szURITempPath = pState->pTempFilePath;

        status = pState->o3gpWriter.hBlock->SetAttribute(pState->o3gpWriter.hBlock,
            NvMMWriterAttribute_TempFilePath,
            NvMMSetAttrFlag_Notification,
            NvOsStrlen(AttribTempFilePath.szURITempPath),
            &AttribTempFilePath);

        if (status != NvSuccess)
        {
            NvOsDebugPrintf("NvxMp4FileWriterOpen: Error setting output filename\n");
            return OMX_ErrorBadParameter;
        }
        NvOsSemaphoreWait(pState->o3gpWriter.SetAttrDoneSema);
    }

    if(pState->nFileSize > 0)
    {
        NvMMWriterAttrib_FileSize WriterFileSize;

        WriterFileSize.maxFileSize = pState->nFileSize;
        status = pState->o3gpWriter.hBlock->SetAttribute(pState->o3gpWriter.hBlock,
                                                     NvMMWriterAttribute_FileSize,
                                                     NvMMSetAttrFlag_Notification,
                                                     sizeof(WriterFileSize),
                                                     &WriterFileSize);
        if (status != NvSuccess)
        {
            return OMX_ErrorBadParameter;
        }
        NvOsSemaphoreWait(pState->o3gpWriter.SetAttrDoneSema);
    }

    if(pState->nDuration > 0)
    {
        NvMMWriterAttrib_TrakDurationLimit WriterMaxDuration;

        WriterMaxDuration.maxTrakDuration = pState->nDuration;
        status = pState->o3gpWriter.hBlock->SetAttribute(pState->o3gpWriter.hBlock,
                                                     NvMMWriterAttribute_TrakDurationLimit,
                                                     NvMMSetAttrFlag_Notification,
                                                     sizeof(WriterMaxDuration),
                                                     &WriterMaxDuration);
        if (status != NvSuccess)
        {
            return OMX_ErrorBadParameter;
        }
        NvOsSemaphoreWait(pState->o3gpWriter.SetAttrDoneSema);
    }

    oPortDefinition.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    oPortDefinition.nVersion.nVersion = NvxComponentSpecVersion.nVersion;

    // Fill up audio encoder settings
    if (pPortAudio->oPortDef.bEnabled)
    {
        oPortDefinition.nPortIndex = pPortAudio->nTunnelPort;
        eError = OMX_GetParameter(pPortAudio->hTunnelComponent,
                                  OMX_IndexParamPortDefinition,
                                  &oPortDefinition);
        if (eError != OMX_ErrorNone)
        {
            return eError;
        }
        NvOsMemcpy(&(pPortAudio->oPortDef.format.audio),
                 &(oPortDefinition.format.audio),
                 sizeof(OMX_AUDIO_PORTDEFINITIONTYPE));

        StreamConfig.BlockType = NvMMBlockType_3gppWriter;

        // SpeechAudioFlag values: 0=NBAMR 1=WBAMR 2=AAC 3=QCELP, 4=QCELP with mp4a support
        switch (oPortDefinition.format.audio.eEncoding)
        {
        case OMX_AUDIO_CodingAAC:
            if(pState->eAACProfile == OMX_AUDIO_AACObjectHE)
            {
                StreamConfig.StreamType = NvMMStreamType_AACSBR;
                StreamConfig.NvMMWriter_Config.AudioConfig.NvMMWriter_AudConfig.AACAudioConfig.aacObjectType = OBJECT_TYPE_AAC_PLUS;
            }
            else if(pState->eAACProfile == OMX_AUDIO_AACObjectHE_PS)
            {
                StreamConfig.StreamType = NvMMStreamType_AACSBR;
                StreamConfig.NvMMWriter_Config.AudioConfig.NvMMWriter_AudConfig.AACAudioConfig.aacObjectType = OBJECT_TYPE_AAC_PLUS;
            }
            else  //pState->eAACProfile == OMX_AUDIO_AACObjectLC
            {
            StreamConfig.StreamType = NvMMStreamType_AAC;
                StreamConfig.NvMMWriter_Config.AudioConfig.NvMMWriter_AudConfig.AACAudioConfig.aacObjectType = OBJECT_TYPE_AAC_LC;
            }
            StreamConfig.avg_bitrate = pState->nAudioBitRate;
            StreamConfig.max_bitrate = AAC_MAX_BITRATE;
            StreamConfig.NvMMWriter_Config.AudioConfig.NvMMWriter_AudConfig.AACAudioConfig.aacSamplingFreqIndex = pState->nAudioSampleRateIndex;
            StreamConfig.NvMMWriter_Config.AudioConfig.NvMMWriter_AudConfig.AACAudioConfig.aacChannelNumber = (NvU8)pState->nAudioChannels;
            break;

        case OMX_AUDIO_CodingAMR:
            if(pState->eAMRBandMode < OMX_AUDIO_AMRBandModeWB0)
            {
            StreamConfig.StreamType = NvMMStreamType_NAMR;
                StreamConfig.avg_bitrate = pState->nAudioBitRate;
                StreamConfig.max_bitrate = AMRNB_MAX_BITRATE;
                StreamConfig.NvMMWriter_Config.AudioConfig.NvMMWriter_AudConfig.AMRConfig.mode_set = (NvU32)(pState->eAMRBandMode - OMX_AUDIO_AMRBandModeNB0);
                StreamConfig.NvMMWriter_Config.AudioConfig.NvMMWriter_AudConfig.AMRConfig.mode_change_period = 0;
                StreamConfig.NvMMWriter_Config.AudioConfig.NvMMWriter_AudConfig.AMRConfig.frames_per_sample = 5;
            }
            else
            {
                StreamConfig.StreamType = NvMMStreamType_WAMR;
                StreamConfig.avg_bitrate = pState->nAudioBitRate;
                StreamConfig.max_bitrate = AMRWB_MAX_BITRATE;
            StreamConfig.NvMMWriter_Config.AudioConfig.NvMMWriter_AudConfig.AMRConfig.mode_set = (NvU32)(pState->eAMRBandMode - OMX_AUDIO_AMRBandModeNB0);
            StreamConfig.NvMMWriter_Config.AudioConfig.NvMMWriter_AudConfig.AMRConfig.mode_change_period = 0;
            StreamConfig.NvMMWriter_Config.AudioConfig.NvMMWriter_AudConfig.AMRConfig.frames_per_sample = 5;
            }
            break;

        case OMX_AUDIO_CodingQCELP13:
            StreamConfig.StreamType = NvMMStreamType_QCELP;
            break;

        case OMX_AUDIO_CodingQCELP8:
            StreamConfig.StreamType = NvMMStreamType_QCELP;
            StreamConfig.avg_bitrate = pState->nAudioBitRate;
            break;

        default:
            // Unsupported
            NV_DEBUG_PRINTF(("3GP Writer: Unsupported audio format\n"));
            return OMX_ErrorBadParameter;
        }

        if (pState->nDesiredFramesAudio)
        {
            StreamConfig.setFrames = NV_TRUE;
            StreamConfig.numberOfFrames = pState->nDesiredFramesAudio;
        }
        else
        {
            StreamConfig.setFrames = NV_FALSE;
        }

        pState->o3gpWriter.hBlock->SetAttribute(pState->o3gpWriter.hBlock,
                                                NvMMWriterAttribute_StreamConfig,
                                                NvMMSetAttrFlag_Notification,
                                                sizeof(NvMMWriterStreamConfig),
                                                &StreamConfig);
        NvOsSemaphoreWait(pState->o3gpWriter.SetAttrDoneSema);

        eError = NvxNvMMTransformSetupInput(&pState->o3gpWriter, s_nvx_port_audio, &pNvComp->pPorts[NVX_3GP_AUDIO_TRACK],
                                            FILE_WRITER_BUFFER_COUNT, FILE_WRITER_BUFFER_SIZE, OMX_FALSE);
        if (eError != OMX_ErrorNone)
            return eError;
    }

    if (pPortVideo->oPortDef.bEnabled)
    {
        oPortDefinition.nPortIndex = pPortVideo->nTunnelPort;
        eError = OMX_GetParameter(pPortVideo->hTunnelComponent,
                                  OMX_IndexParamPortDefinition,
                                  &oPortDefinition);
        if (eError != OMX_ErrorNone)
        {
            return eError;
        }
        NvOsMemcpy(&(pPortVideo->oPortDef.format.video),
                 &(oPortDefinition.format.video),
                 sizeof(OMX_IMAGE_PORTDEFINITIONTYPE));

        switch ( oPortDefinition.format.video.eCompressionFormat)
        {
        case OMX_VIDEO_CodingMPEG4:
            StreamConfig.StreamType = NvMMStreamType_MPEG4;
            break;
        case OMX_VIDEO_CodingH263:
            StreamConfig.StreamType = NvMMStreamType_H263;
            break;
        case OMX_VIDEO_CodingAVC:
            StreamConfig.StreamType = NvMMStreamType_H264;
            break;
        default:
            // Unsupported
            NV_DEBUG_PRINTF(("3GP Writer: Unsupported video format\n"));
            return OMX_ErrorBadParameter;
        }
        
        StreamConfig.NvMMWriter_Config.VideoConfig.width = (NvU32)oPortDefinition.format.video.nFrameWidth;
        StreamConfig.NvMMWriter_Config.VideoConfig.height = (NvU32)oPortDefinition.format.video.nFrameHeight;
        StreamConfig.NvMMWriter_Config.VideoConfig.frameRate = (oPortDefinition.format.video.xFramerate) ? 
                                                               (NvU32)((oPortDefinition.format.video.xFramerate)>>16) : 30;
        StreamConfig.NvMMWriter_Config.VideoConfig.level = (NvU8)1;
        StreamConfig.NvMMWriter_Config.VideoConfig.profile = (NvU8)1;
        StreamConfig.avg_bitrate = oPortDefinition.format.video.nBitrate/1000;
        StreamConfig.max_bitrate = 3*StreamConfig.avg_bitrate;  // a value that all decoders prefer

        if (pState->nDesiredFramesVideo)
        {
            StreamConfig.setFrames = NV_TRUE;
            StreamConfig.numberOfFrames = pState->nDesiredFramesVideo;
        }
        else
        {
            StreamConfig.setFrames = NV_FALSE;
        }

        pState->o3gpWriter.hBlock->SetAttribute(pState->o3gpWriter.hBlock,
                                                NvMMWriterAttribute_StreamConfig,
                                                NvMMSetAttrFlag_Notification,
                                                sizeof(NvMMWriterStreamConfig),
                                                &StreamConfig);

        eError = NvxNvMMTransformSetupInput(&pState->o3gpWriter, s_nvx_port_video, &pNvComp->pPorts[NVX_3GP_VIDEO_TRACK],
                                            FILE_WRITER_BUFFER_COUNT, FILE_WRITER_BUFFER_SIZE, OMX_FALSE);
        if (eError != OMX_ErrorNone)
            return eError;
    }

    NvxNvMMTransformSetEventCallback(&pState->o3gpWriter, NvxMp4FileWriterBlockEventHandler, pNvComp);

    pState->b3gpInitialized = OMX_TRUE;

    return eError;
}

static OMX_ERRORTYPE NvxMp4FileWriterClose(OMX_IN NvxComponent *pNvComp )
{
    NvxMp4WriterState *pState = pNvComp->pComponentData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH,pNvComp->eObjectType,"%s\n","--NvxMp4FileWriterClose"));

    if (!pState->b3gpInitialized)
        return OMX_ErrorNone;

    {
        NvMMWriterAttrib_FrameCount oFrameCount;

        pState->o3gpWriter.hBlock->GetAttribute(pState->o3gpWriter.hBlock,
                                                NvMMWriterAttribute_FrameCount,
                                                sizeof(NvMMWriterAttrib_FrameCount),
                                                &oFrameCount);
        pState->nAudioFrames = (int)oFrameCount.audioFrameCount;
        pState->nVideoFrames = (int)oFrameCount.videoFrameCount;
    }

    eError = NvxNvMMTransformClose(&pState->o3gpWriter);
    if (eError != OMX_ErrorNone)
        return eError;

    while (!pState->o3gpWriter.blockCloseDone);

    pState->b3gpInitialized = OMX_FALSE;

    return NvxComponentBaseReleaseResources(pNvComp);
}

static OMX_ERRORTYPE NvxMp4FileWriterPreChangeState(NvxComponent *pNvComp,
                                                    OMX_STATETYPE oNewState)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    NvxMp4WriterState *pState;

    //NVXTRACE((NVXT_CALLGRAPH, NVXT_FILE_WRITER, "+NvxMp4FileWriterPreChangeState\n"));

    pState = (NvxMp4WriterState *)pNvComp->pComponentData;
    NV_ASSERT(pState);

    if (!pState->b3gpInitialized)
        return OMX_ErrorNone;

    err = NvxNvMMTransformPreChangeState(&pState->o3gpWriter, oNewState,
                                         pNvComp->eState);
    
    return err;
}

static OMX_ERRORTYPE CommonInitThis(NvxComponent* pThis)
{
    // Need some default output file in order to pass conformance tests.
    NvxMp4WriterState *pState;  
    #define DEFAULT_OUTPUT_FILENAME "data/test.out"
    static const char szDefaultFilename[] = DEFAULT_OUTPUT_FILENAME;
    #define DEFAULT_OUTPUT_WRITER_NAME "3gpp"
    static const char szDefaultOutputWriterName[] = DEFAULT_OUTPUT_WRITER_NAME;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    pThis->eObjectType = NVXT_FILE_WRITER; 
  
    pThis->DeInit = NvxMp4FileWriterDeInit;
    pThis->GetParameter = NvxMp4FileWriterGetParameter;
    pThis->SetParameter = NvxMp4FileWriterSetParameter;
    pThis->GetConfig    = NvxMp4FileWriterGetConfig;
    pThis->SetConfig    = NvxMp4FileWriterSetConfig;
    pThis->WorkerFunction = NvxMp4FileWriterWorkerFunction;
    pThis->AcquireResources = NvxMp4FileWriterOpen;
    pThis->ReleaseResources = NvxMp4FileWriterClose;
    pThis->PreChangeState   = NvxMp4FileWriterPreChangeState;

    pThis->pComponentData = NvOsAlloc(sizeof(NvxMp4WriterState));
    if(!pThis->pComponentData)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType, "ERROR:CommonInitThis:"
               ":pComponentData = %p:[%s(%d)]\n",pThis->pComponentData , __FILE__, __LINE__));
        return OMX_ErrorInsufficientResources; 
    }
    NvOsMemset(pThis->pComponentData, 0, sizeof(NvxMp4WriterState));

    // Use some default file for conformance tests.
    pState = pThis->pComponentData; 
    pState->pFilename = NvOsAlloc(sizeof(DEFAULT_OUTPUT_FILENAME));
    if(!pState->pFilename)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType, "ERROR:CommonInitThis:"
               ":pFilename = %s [%s(%d)]\n",pState->pFilename , __FILE__, __LINE__));
        return OMX_ErrorInsufficientResources; 
    }
    NvOsStrncpy(pState->pFilename, szDefaultFilename, NvOsStrlen(szDefaultFilename) + 1);

    // Default output writer
    pState->pOutPutFormat = NvOsAlloc(sizeof(szDefaultOutputWriterName));
    if(!pState->pOutPutFormat)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType, "ERROR:CommonInitThis:"
               ":pOutPutFormat = %s [%s(%d)]\n",pState->pOutPutFormat , __FILE__, __LINE__));
        return OMX_ErrorInsufficientResources;
    }
    NvOsStrncpy(pState->pOutPutFormat, szDefaultOutputWriterName, NvOsStrlen(szDefaultOutputWriterName) + 1);

    pState->nAudioBitRate = 128000;
    pState->nAudioSampleRateIndex = 4;      // 44100
    pState->eAMRBandMode = OMX_AUDIO_AMRBandModeNB7;

    pState->nDesiredFramesAudio = 0;
    pState->nDesiredFramesVideo = 0;
    //in case of qcelp
    pState->nMinBitRate = 1;
    pState->nMaxBitRate = 4;
    pState->eCDMARate = OMX_AUDIO_CDMARateFull;
    return eError; 
}

OMX_ERRORTYPE NvxMp4FileWriterInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pNvComp;
    eError = NvxComponentCreate(hComponent,2,&pNvComp);   
     
    if (NvxIsError(eError))
        return eError;

    eError = CommonInitThis(pNvComp);
    if(NvxIsError(eError))
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxMp4FileWriterInit:"
           ":eError = %x [%s(%d)]\n",eError, __FILE__, __LINE__));
        return eError; 
    }
    pNvComp->pComponentName = "OMX.Nvidia.mp4.write";
    pNvComp->nComponentRoles = 1;
    pNvComp->sComponentRoles[0] = "container_muxer.3gp";

    NvxPortInitVideo(&pNvComp->pPorts[NVX_3GP_VIDEO_TRACK], OMX_DirInput,
                     FILE_WRITER_BUFFER_COUNT, FILE_WRITER_BUFFER_SIZE,
                     OMX_VIDEO_CodingAutoDetect);
    pNvComp->pPorts[NVX_3GP_VIDEO_TRACK].eNvidiaTunnelTransaction = ENVX_TRANSACTION_NVMM_TUNNEL;

    NvxPortInitAudio(&pNvComp->pPorts[NVX_3GP_AUDIO_TRACK], OMX_DirInput,
                     FILE_WRITER_BUFFER_COUNT, FILE_WRITER_BUFFER_SIZE,
                     OMX_AUDIO_CodingAutoDetect);
    pNvComp->pPorts[NVX_3GP_AUDIO_TRACK].eNvidiaTunnelTransaction = ENVX_TRANSACTION_NVMM_TUNNEL;

    return eError;
}

