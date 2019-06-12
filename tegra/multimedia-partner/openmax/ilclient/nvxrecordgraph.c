/*
 * Copyright (c) 2009-2014 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#include "nvutil.h"
#include "nvxrecordgraph.h"
#include "nvmm_logger.h"

static char *g_3gpWriter    = "OMX.Nvidia.mp4.write";
static char *g_AudioCapturer = "OMX.Nvidia.audio.capturer";
static char *g_AacEncoder   = "OMX.Nvidia.aac.encoder";
static char *g_AmrEncoder   = "OMX.Nvidia.amr.encoder";
static char *g_AmrWBEncoder = "OMX.Nvidia.amrwb.encoder";
static char *g_H264Encoder  = "OMX.Nvidia.h264.encoder";
static char *g_Mpeg4Encoder = "OMX.Nvidia.mp4.encoder";
static char *g_H263Encoder  = "OMX.Nvidia.h263.encoder";
static char *g_WavEncoder   = "OMX.Nvidia.wav.encoder";
static char *g_WavWriter    = "OMX.Nvidia.wav.write";
static char *g_AmrWriter    = "OMX.Nvidia.amr.write";

static char *g_strAVWriter   = "AVWriter";
static char *g_strVideoEncoder = "VideoEncoder";
static char *g_strAudioCapture = "AudioCapture";
static char *g_strAudioEncoder = "AudioEncoder";

static OMX_VIDEO_MPEG4LEVELTYPE g_eLevelMpeg4[] =
{
    OMX_VIDEO_MPEG4Level0,
    OMX_VIDEO_MPEG4Level1,
    OMX_VIDEO_MPEG4Level2,
    OMX_VIDEO_MPEG4Level3,
    OMX_VIDEO_MPEG4LevelMax,
};

#define g_nLevelsMpeg4      (sizeof(g_eLevelMpeg4) / sizeof(g_eLevelMpeg4[0]))

static OMX_VIDEO_AVCLEVELTYPE g_eLevelH264[] =
{
    OMX_VIDEO_AVCLevel1,
    OMX_VIDEO_AVCLevel1b,
    OMX_VIDEO_AVCLevel11,
    OMX_VIDEO_AVCLevel12,
    OMX_VIDEO_AVCLevel13,
    OMX_VIDEO_AVCLevel2,
    OMX_VIDEO_AVCLevel21,
    OMX_VIDEO_AVCLevel22,
    OMX_VIDEO_AVCLevel3,
    OMX_VIDEO_AVCLevel31,
    OMX_VIDEO_AVCLevel32,
    OMX_VIDEO_AVCLevel4,
    OMX_VIDEO_AVCLevel41,
    OMX_VIDEO_AVCLevel42,
    OMX_VIDEO_AVCLevel5,
    OMX_VIDEO_AVCLevel51,
    OMX_VIDEO_AVCLevelMax,
};
#define g_nLevelsH264        (sizeof(g_eLevelH264) / sizeof(g_eLevelH264[0]))

const NvxRecorderParam  g_DefaultRecorderParams = {1280, 720, INVALID_PARAM, 0, 30, 0,
                                                0, 0, VIDEO_ENCODER_TYPE_H264,
                                                255, 0, 0, OMX_MirrorNone, 0, 0, NVX_VIDEO_RateControlMode_VBR,
                                                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 25, OMX_FALSE, OMX_FALSE,
                                                640, 0, 256, NV_FALSE, NV_FALSE, NV_FALSE,
                                                NV_FALSE, NV_FALSE, NV_FALSE, NV_FALSE, NV_FALSE, IMAGE_ENCODER_TYPE_JPEG };

static OMX_VERSIONTYPE vOMX;
#define INIT_PARAM(_X_)  (NvOsMemset(&(_X_), 0xDE, sizeof(_X_)), ((_X_).nSize = sizeof (_X_)), (_X_).nVersion = vOMX)

#define NVX_CHK_ERR(expr) {               \
            eError = (expr);            \
            if ((eError != OMX_ErrorNone))   \
            {                       \
                NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "%s[%d] : %s Failed! NvError = 0x%08x",  __FUNCTION__, __LINE__, #expr, eError)); \
                goto cleanup;     \
            }                       \
        }

#define NVX_CHK_ARG(expr) {               \
            if (((expr) == 0))   \
            {                       \
                NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "%s[%d] : %s Assert Failed!", __FUNCTION__, __LINE__, #expr)); \
                eError = NvError_BadParameter;\
                goto cleanup;\
            }                       \
        }

// forward declarations of internal functions.
void
NvxRecorderDeInitNonTunneledComponents(NvxRecorderGraph *pRecorder);

void
NvxRecorderPostExecuteNonTunneledComponents(NvxRecorderGraph *pRecorder);

OMX_ERRORTYPE
NvxRecorderInitNonTunneledComponents ( NvxRecorderGraph *pRecorder );

OMX_ERRORTYPE NvxInitRecorderStruct(NvxRecorderGraph *pRecorder,
                                    NvxFramework pOmxFramework)
{
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));
    if(!pRecorder || !pOmxFramework)
        return OMX_ErrorBadParameter;

    pRecorder->m_pOmxFramework = pOmxFramework;
    vOMX = NvxFrameworkGetOMXILVersion(pRecorder->m_pOmxFramework);
    pRecorder->eAudioCodingType = OMX_AUDIO_CodingAAC;
    pRecorder->eAacProfileType = OMX_AUDIO_AACObjectLC;
    pRecorder->nAudioSampleRate = 44100;
    pRecorder->nAudioBitRate = 128000;
    pRecorder->nChannels = 2;
    pRecorder->RecorderParam = g_DefaultRecorderParams;
    pRecorder->PreviewParam.nWidth = 720;
    pRecorder->PreviewParam.nHeight = 480;
    pRecorder->AudioEncEnable = OMX_FALSE;
    pRecorder->VideoEncEnable = OMX_FALSE;
    pRecorder->ImageEncEnable = OMX_FALSE;
    pRecorder->eRecState = NvxRecorderGraphState_Initialized;
    pRecorder->useExternalAudioSource = OMX_FALSE;
    pRecorder->nFileSize = 0;
    pRecorder->nDuration = 0;
    pRecorder->eAudioSource = 1;
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,"--%s[%d]", __FUNCTION__, __LINE__));
    return OMX_ErrorNone;
}

void NvxRecorderGraphUseExternalAudioSource(NvxRecorderGraph *pRecorder,
                                                    char *audSourceName)
{
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));
    if(!pRecorder || !audSourceName)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
           "--%s[%d] Invalid input!!!", __FUNCTION__, __LINE__));
        return;
    }

    pRecorder->useExternalAudioSource = OMX_TRUE;
    pRecorder->externalAudioSource = audSourceName;
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
                  "--%s[%d]:: Audio Source : %s", __FUNCTION__, __LINE__, audSourceName));
    return ;
}


OMX_ERRORTYPE s_SetupVideoEncoder(NvxRecorderGraph *pRecorder,
                                         OMX_HANDLETYPE hVideoEncoder)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE oPortDef;
    OMX_VIDEO_PARAM_BITRATETYPE oBitRate;
    OMX_CONFIG_FRAMERATETYPE oFrameRate;
    OMX_IMAGE_PARAM_QFACTORTYPE oQFactor;
    NVX_PARAM_VIDENCPROPERTY oEncodeProp;
    OMX_INDEXTYPE eIndex;
    NvxRecorderParam Param = pRecorder->RecorderParam;
    NvS32 AskingWidth = 0, AskingHeight = 0;


    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));

    NVX_CHK_ARG (pRecorder && hVideoEncoder);

    AskingWidth = Param.nWidthCapture;
    AskingHeight = Param.nHeightCapture;

    INIT_PARAM(oPortDef);
    oPortDef.nPortIndex = 0;
    OMX_GetParameter(hVideoEncoder, OMX_IndexParamPortDefinition, &oPortDef);
    oPortDef.format.video.nFrameWidth = Param.nWidthCapture;
    oPortDef.format.video.nFrameHeight = Param.nHeightCapture;
    NVX_CHK_ERR(OMX_SetParameter(hVideoEncoder, OMX_IndexParamPortDefinition, &oPortDef));

    if (VIDEO_ENCODER_TYPE_MPEG4 == Param.videoEncoderType)
    {
        OMX_VIDEO_PARAM_MPEG4TYPE oMpeg4Type;
        INIT_PARAM(oMpeg4Type);
        oMpeg4Type.nPortIndex = 0;
        OMX_GetParameter(hVideoEncoder, OMX_IndexParamVideoMpeg4, &oMpeg4Type);
        if (Param.videoEncodeLevel >= g_nLevelsMpeg4)
            Param.videoEncodeLevel = g_nLevelsMpeg4 - 1;
        oMpeg4Type.eLevel = g_eLevelMpeg4[Param.videoEncodeLevel];
        oMpeg4Type.nSliceHeaderSpacing = Param.nSliceHeaderSpacing;
        oMpeg4Type.nPFrames =  Param.nIFrameInterval-1;
        NVX_CHK_ERR(OMX_SetParameter(hVideoEncoder, OMX_IndexParamVideoMpeg4, &oMpeg4Type));
    }
    else  if (VIDEO_ENCODER_TYPE_H264 == Param.videoEncoderType)
    {
        OMX_VIDEO_PARAM_AVCTYPE oH264Type;
        OMX_CONFIG_RECTTYPE oH264OutputCropRect;

        INIT_PARAM(oH264Type);
        oH264Type.nPortIndex = 0;
        OMX_GetParameter(hVideoEncoder, OMX_IndexParamVideoAvc, &oH264Type);
        if (Param.videoEncodeLevel >= g_nLevelsH264)
            Param.videoEncodeLevel = g_nLevelsH264 - 1;
        oH264Type.eLevel = g_eLevelH264[Param.videoEncodeLevel];
        oH264Type.nSliceHeaderSpacing = Param.nSliceHeaderSpacing;
        oH264Type.nPFrames =  Param.nIFrameInterval-1;
        NVX_CHK_ERR(OMX_SetParameter(hVideoEncoder, OMX_IndexParamVideoAvc, &oH264Type));

        if (Param.videoEncoderType == VIDEO_ENCODER_TYPE_H264)
        {
            INIT_PARAM(oH264OutputCropRect);
            oH264OutputCropRect.nPortIndex = 0;
            oH264OutputCropRect.nLeft = 0;
            oH264OutputCropRect.nTop = 0;
            oH264OutputCropRect.nWidth = AskingWidth;
            oH264OutputCropRect.nHeight = AskingHeight;

            // set the crop rectangle to the asking resolution
            eError = OMX_SetConfig(hVideoEncoder,
                        OMX_IndexConfigCommonOutputCrop,
                        &oH264OutputCropRect);

            if (eError != OMX_ErrorNone)
                return eError;
        }
    }

    {
        NVX_CONFIG_TEMPORALTRADEOFF oTemporalTradeOff;
        INIT_PARAM(oTemporalTradeOff);
        oTemporalTradeOff.nPortIndex = 0;
        oTemporalTradeOff.TemporalTradeOffLevel = Param.TemporalTradeOffLevel;
        NVX_CHK_ERR(OMX_GetExtensionIndex(
                                hVideoEncoder,
                                NVX_INDEX_CONFIG_VIDEO_ENCODE_TEMPORALTRADEOFF,
                                &eIndex));
        NVX_CHK_ERR(OMX_SetConfig(hVideoEncoder, eIndex, &oTemporalTradeOff));
    }

    {
        NVX_CONFIG_VIDENC_QUANTIZATION_RANGE oQPRange;
        INIT_PARAM(oQPRange);
        oQPRange.nPortIndex = 0;
        oQPRange.nMinQpI = Param.nMinQpI;
        oQPRange.nMaxQpI = Param.nMaxQpI;
        oQPRange.nMinQpP = Param.nMinQpP;
        oQPRange.nMaxQpP = Param.nMaxQpP;
        oQPRange.nMinQpB = Param.nMinQpB;
        oQPRange.nMaxQpB = Param.nMaxQpB;
        NVX_CHK_ERR(OMX_GetExtensionIndex(
                                hVideoEncoder,
                                NVX_INDEX_CONFIG_VIDEO_ENCODE_QUANTIZATION_RANGE,
                                &eIndex));
        NVX_CHK_ERR(OMX_SetConfig(hVideoEncoder, eIndex, &oQPRange));
    }

    {
        NVX_PARAM_VIDENC_H264_QUALITY_PARAMS oH264QualityParams;
        INIT_PARAM(oH264QualityParams);
        oH264QualityParams.nPortIndex = 0;
        oH264QualityParams.nFavorInterBias = Param.nFavorInterBias;
        oH264QualityParams.nFavorIntraBias = Param.nFavorIntraBias;
        oH264QualityParams.nFavorIntraBias_16X16 = Param.nFavorIntraBias_16X16;
        NVX_CHK_ERR(OMX_GetExtensionIndex(
                                hVideoEncoder,
                                NVX_INDEX_PARAM_VIDENC_H264_QUALITY_PARAMS,
                                &eIndex));
        OMX_SetParameter(hVideoEncoder, eIndex, &oH264QualityParams);
    }

    {
        OMX_CONFIG_BOOLEANTYPE oStringentBitrate;
        INIT_PARAM(oStringentBitrate);
        oStringentBitrate.bEnabled = Param.bStringentBitrate;
        NVX_CHK_ERR(OMX_GetExtensionIndex(
                                hVideoEncoder,
                                NVX_INDEX_PARAM_VIDEO_ENCODE_STRINGENTBITRATE,
                                &eIndex));
        NVX_CHK_ERR(OMX_SetParameter(hVideoEncoder, eIndex, &oStringentBitrate));
    }

    {
        OMX_CONFIG_ROTATIONTYPE oRotation;
        INIT_PARAM(oRotation);
        oRotation.nPortIndex = 0;
        oRotation.nRotation = Param.nRotation;
        NVX_CHK_ERR(OMX_SetConfig(hVideoEncoder, OMX_IndexConfigCommonRotate,
                               &oRotation));
    }

    {
        OMX_CONFIG_MIRRORTYPE oMirror;
        INIT_PARAM(oMirror);
        oMirror.nPortIndex = 0;
        oMirror.eMirror = Param.eMirror;
        NVX_CHK_ERR(OMX_SetConfig(hVideoEncoder, OMX_IndexConfigCommonMirror,
                               &oMirror));
    }

    INIT_PARAM(oBitRate);
    oBitRate.nTargetBitrate = Param.nBitRate;
    oBitRate.eControlRate = OMX_Video_ControlRateConstantSkipFrames;
    NVX_CHK_ERR(OMX_SetParameter(hVideoEncoder, OMX_IndexParamVideoBitrate, &oBitRate));

    INIT_PARAM(oFrameRate);
    oFrameRate.xEncodeFramerate = ((Param.nFrameRate)<<16);
    NVX_CHK_ERR(OMX_SetConfig(hVideoEncoder, OMX_IndexConfigVideoFramerate, &oFrameRate));

    INIT_PARAM(oEncodeProp);
    oEncodeProp.nPortIndex = 1;
    oEncodeProp.eApplicationType = (Param.nAppType == 0) ?
        NVX_VIDEO_Application_Camcorder : NVX_VIDEO_Application_VideoTelephony;
    switch (Param.nErrorResilLevel)
    {
        case 0: oEncodeProp.eErrorResiliencyLevel = NVX_VIDEO_ErrorResiliency_None;
                break;
        case 1: oEncodeProp.eErrorResiliencyLevel = NVX_VIDEO_ErrorResiliency_Low;
                break;
        case 2: oEncodeProp.eErrorResiliencyLevel = NVX_VIDEO_ErrorResiliency_High;
                break;
        default:oEncodeProp.eErrorResiliencyLevel = NVX_VIDEO_ErrorResiliency_None;
                break;
    }
    oEncodeProp.bSvcEncodeEnable = OMX_FALSE;
    oEncodeProp.bSetMaxEncClock = Param.bSetMaxEncClock;
    oEncodeProp.bFrameSkip = Param.bFrameSkip;
    oEncodeProp.bAllIFrames = Param.bAllIFrames;
    oEncodeProp.bBitBasedPacketization = Param.bBitBasedPacketization;
    oEncodeProp.bInsertSPSPPSAtIDR = Param.bInsertSPSPPSAtIDR;
    oEncodeProp.bUseConstrainedBP = Param.bUseConstrainedBP;
    oEncodeProp.bInsertVUI = Param.bInsertVUI;
    oEncodeProp.bInsertAUD = Param.bInsertAUD;
    oEncodeProp.nPeakBitrate = Param.nPeakBitrate;
    oEncodeProp.bEnableStuffing = Param.bEnableStuffing;
    oEncodeProp.bLowLatency = OMX_FALSE;
    oEncodeProp.bSliceLevelEncode = OMX_FALSE;
    oEncodeProp.nVirtualBufferSize = 0; // Set it to zero as it is not specified
    NVX_CHK_ERR(OMX_GetExtensionIndex(
                                hVideoEncoder,
                                NVX_INDEX_PARAM_VIDEO_ENCODE_PROPERTY,
                                &eIndex));
    NVX_CHK_ERR(OMX_SetParameter(hVideoEncoder, eIndex, &oEncodeProp));

    INIT_PARAM(oQFactor);
    switch (Param.nQualityLevel)
    {
        case 0: oQFactor.nQFactor = 33; break;
        case 1: oQFactor.nQFactor = 66; break;
        case 2: oQFactor.nQFactor = 100; break;
        default: oQFactor.nQFactor = 66; break;
    }
    NVX_CHK_ERR(OMX_SetParameter(hVideoEncoder, OMX_IndexParamQFactor, &oQFactor));

    {
        OMX_VIDEO_PARAM_QUANTIZATIONTYPE oQuantType;
        INIT_PARAM(oQuantType);
        oQuantType.nPortIndex = 1;
        oQuantType.nQpI = Param.nInitialQPI;
        oQuantType.nQpP = Param.nInitialQPP;
        oQuantType.nQpB = 0;            //not used.
        NVX_CHK_ERR(OMX_SetParameter(hVideoEncoder, OMX_IndexParamVideoQuantization, &oQuantType));
    }

    {
        NVX_PARAM_RATECONTROLMODE oRCMode;
        INIT_PARAM(oRCMode);
        oRCMode.nPortIndex = 1;
        oRCMode.eRateCtrlMode = Param.eRCMode;
        NVX_CHK_ERR(OMX_GetExtensionIndex(hVideoEncoder, NVX_INDEX_PARAM_RATECONTROLMODE,
                                   &eIndex));
        NVX_CHK_ERR(OMX_SetParameter(hVideoEncoder, eIndex, &oRCMode));
    }

cleanup:

    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,"--%s[%d] eError = 0x%x",
                   __FUNCTION__, __LINE__, eError));
    return eError;
}
OMX_ERRORTYPE NvxRecorderEnablePort(NvxPort *port, OMX_BOOL enable)
{
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_COMMANDTYPE portCmd;

    if(!port)
        return OMX_ErrorBadParameter;

    portCmd = enable ? OMX_CommandPortEnable : OMX_CommandPortDisable;

    omxErr = OMX_SendCommand(port->component->handle,
                             portCmd,port->portNum,0);
    return omxErr;
}

static NvxComponent * NvxRecorderGraphInitializeWriter(NvxRecorderGraph *pRecorder)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_HANDLETYPE hAVWriter;
    NvxGraph *pGraph;
    char *AVWriter = g_3gpWriter;
    NvxComponent *pAVWriter = NULL;
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,"++%s[%d]", __FUNCTION__, __LINE__));

    NVX_CHK_ARG (pRecorder && pRecorder->m_pGraph);

    pGraph = pRecorder->m_pGraph;

    if (OMX_AUDIO_CodingPCM == pRecorder->eAudioCodingType)
    {
        AVWriter = g_WavWriter;
    }
    else if(NvxOUTPUT_FORMAT_RAW_AMR == pRecorder->eOutputFormat)
    {
        AVWriter = g_AmrWriter;
    }

    NVX_CHK_ERR (NvxGraphCreateComponentByName( pGraph,AVWriter, g_strAVWriter,
                                            &pAVWriter));
    {
        NvxRecorderAppContext *pAppCtx =  NULL;
        pAppCtx = &pRecorder->RecordAppContext;

        NVX_CHK_ERR (NvxGraphSetCompEventHandler(pAVWriter,
                                    pAppCtx->pClientContext,
                                    pAppCtx->pEventHandler));
    }

    hAVWriter = pAVWriter->handle;

    {
        OMX_INDEXTYPE eIndexParamFilename, eIndexParamTempPath;
        NVX_PARAM_FILENAME oFilenameParam;
        NVX_PARAM_TEMPFILEPATH oTempFilePath;
        OMX_STRING strTempFilePath = "/data/";
        NVX_CHK_ERR (OMX_GetExtensionIndex(hAVWriter, NVX_INDEX_PARAM_FILENAME,
                                       &eIndexParamFilename));

        if (pRecorder->outfile)
        {
            INIT_PARAM(oFilenameParam);
            oFilenameParam.pFilename = pRecorder->outfile;
            eError = OMX_SetParameter(hAVWriter, eIndexParamFilename, &oFilenameParam);
            NVX_CHK_ERR(eError);
        }

        if (!(OMX_AUDIO_CodingPCM == pRecorder->eAudioCodingType || NvxOUTPUT_FORMAT_RAW_AMR == pRecorder->eOutputFormat))
        {
            NVX_CHK_ERR (OMX_GetExtensionIndex(hAVWriter, NVX_INDEX_PARAM_TEMPFILEPATH,
                                           &eIndexParamTempPath));
            {
                INIT_PARAM(oTempFilePath);
                oTempFilePath.pTempPath = strTempFilePath;
                eError = OMX_SetParameter(hAVWriter, eIndexParamTempPath, &oTempFilePath);
                NVX_CHK_ERR(eError);
            }
        }

        if(NvxOUTPUT_FORMAT_MP4 == pRecorder->eOutputFormat)
        {
            OMX_INDEXTYPE eIndexParamOutputFormat;
            NVX_CHK_ERR (OMX_GetExtensionIndex(hAVWriter, NVX_INDEX_PARAM_OUTPUTFORMAT,
                                           &eIndexParamOutputFormat));
            {
                NVX_PARAM_OUTPUTFORMAT oFormat;

                INIT_PARAM(oFormat);
                oFormat.OutputFormat = "mp4";
                eError = OMX_SetParameter(hAVWriter, eIndexParamOutputFormat, &oFormat);
                NVX_CHK_ERR(eError);
            }
        }
        if(pRecorder->nFileSize)
        {
            OMX_INDEXTYPE eIndexParamWriterFileSize;
            NVX_CHK_ERR (OMX_GetExtensionIndex(hAVWriter, NVX_INDEX_PARAM_WRITERFILESIZE,
                                           &eIndexParamWriterFileSize));
            {
                NVX_PARAM_WRITERFILESIZE oParam;

                INIT_PARAM(oParam);
                oParam.nFileSize = pRecorder->nFileSize;
                eError = OMX_SetParameter(hAVWriter, eIndexParamWriterFileSize, &oParam);
                if(eError != OMX_ErrorNone)
                {
                    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "--%s[%d]- OMX_SetParameter Failed", __FUNCTION__, __LINE__));
                }
            }
        }
        if(pRecorder->nDuration)
        {
            OMX_INDEXTYPE eIndexParamWriterDuration;
            NVX_CHK_ERR (OMX_GetExtensionIndex(hAVWriter, NVX_INDEX_PARAM_DURATION,
                                           &eIndexParamWriterDuration));
            {
                NVX_PARAM_DURATION oParam;

                INIT_PARAM(oParam);
                oParam.nDuration = pRecorder->nDuration;
                eError = OMX_SetParameter(hAVWriter, eIndexParamWriterDuration, &oParam);
                if(eError != OMX_ErrorNone);
                if(eError != OMX_ErrorNone)
                {
                    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "--%s[%d]- OMX_SetParameter Failed 0x%x \n",
                         __FUNCTION__, __LINE__, eError));
                }
            }
        }
        if(pRecorder->nFileSize)
        {
            OMX_INDEXTYPE eIndexParamWriterFileSize;
            NVX_CHK_ERR (OMX_GetExtensionIndex(hAVWriter, NVX_INDEX_PARAM_WRITERFILESIZE,
                                           &eIndexParamWriterFileSize));
            {
                NVX_PARAM_WRITERFILESIZE oParam;

                INIT_PARAM(oParam);
                oParam.nFileSize = pRecorder->nFileSize;
                eError = OMX_SetParameter(hAVWriter, eIndexParamWriterFileSize, &oParam);
                if(eError != OMX_ErrorNone)
                {
                    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "--%s[%d]- OMX_SetParameter Failed", __FUNCTION__, __LINE__));
                }
            }
        }
        if(pRecorder->nDuration)
        {
            OMX_INDEXTYPE eIndexParamWriterDuration;
            NVX_CHK_ERR (OMX_GetExtensionIndex(hAVWriter, NVX_INDEX_PARAM_DURATION,
                                           &eIndexParamWriterDuration));
            {
                NVX_PARAM_DURATION oParam;

                INIT_PARAM(oParam);
                oParam.nDuration = pRecorder->nDuration;
                eError = OMX_SetParameter(hAVWriter, eIndexParamWriterDuration, &oParam);
                if(eError != OMX_ErrorNone)
                {
                    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "--%s[%d]- OMX_SetParameter Failed 0x%x \n",
                         __FUNCTION__, __LINE__, eError));
                }
            }
        }
    }
    OMX_SendCommand(hAVWriter, OMX_CommandPortDisable, OMX_ALL, 0);

cleanup:
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
    return (eError == OMX_ErrorNone)?pAVWriter:NULL;
}

 #define ASSIGN_VIDEO_ENCODER(venc, param)  \
    do { \
            venc = VIDEO_ENCODER_TYPE_MPEG4 == param.videoEncoderType ? \
                         g_Mpeg4Encoder : \
                         (VIDEO_ENCODER_TYPE_H263 == param.videoEncoderType ? \
                         g_H263Encoder : g_H264Encoder);\
    }while(0)

static NvxComponent *NvxRecorderGraphInitializeVideoEncoder(NvxRecorderGraph *pRecorder)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent *pVideoEncoder;
    char *videoEncoder = NULL;
    NvxRecorderAppContext *pAppCtx =  NULL;
    NvxRecorderParam Param = pRecorder->RecorderParam;

    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,"++%s[%d]", __FUNCTION__, __LINE__));

    NVX_CHK_ARG (pRecorder &&  (pRecorder->VideoEncEnable != OMX_FALSE));

    ASSIGN_VIDEO_ENCODER(videoEncoder, Param);

    CheckVideoParams(pRecorder);
    pAppCtx = &pRecorder->RecordAppContext;

    NVX_CHK_ERR (NvxGraphCreateComponentByName(pRecorder->m_pGraph,videoEncoder, g_strVideoEncoder, &pVideoEncoder));

    NVX_CHK_ARG (pAppCtx->pClientContext);
    NVX_CHK_ERR (NvxGraphSetCompEventHandler(pVideoEncoder,
                                pAppCtx->pClientContext,
                                pAppCtx->pEventHandler));

    eError = NvxGraphSetCompBufferCallbacks(pVideoEncoder,
                                pAppCtx->pClientContext,
                                pAppCtx->pFillBufferDone,
                                pAppCtx->pClientContext,
                                pAppCtx->pEmptyBufferDone);

    NvxRecorderEnablePort(&pVideoEncoder->ports[0], OMX_TRUE);
    NvxRecorderEnablePort(&pVideoEncoder->ports[1], OMX_TRUE);
    NVX_CHK_ERR (s_SetupVideoEncoder(pRecorder, pVideoEncoder->handle));

    NVX_CHK_ERR (NvxConnectTunneled(pVideoEncoder, 1, pRecorder->m_pAVWriter, 0));

cleanup:
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,"--%s[%d]", __FUNCTION__, __LINE__));
    return (eError == OMX_ErrorNone)?pVideoEncoder:NULL;
}

#define NO_AMR_BITRATES 18
static const OMX_U32 BitRates_AMR[NO_AMR_BITRATES] = { 0, 4750, 5150, 5900, 6700,
                                               7400, 7950, 10200, 12200,
                                               6600, 8850, 12650, 14250, 15850,
                                               18250, 19850, 23050, 23850};


static OMX_ERRORTYPE NvxRecorderGraphInitAudioCaptureEncode(NvxRecorderGraph *pRecorder)
{
    char  *AudioEncoder = NULL;
    char *AudioCapture = NULL;
    OMX_ERRORTYPE eError;
    NvxComponent *pAudioCapturer, *pAudioEncoder;
    NvU32 WriterAudioPort = 1;

    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,"++%s[%d]", __FUNCTION__, __LINE__));

    NVX_CHK_ARG (pRecorder &&  (pRecorder->AudioEncEnable != OMX_FALSE));

    if(OMX_AUDIO_CodingAMR == pRecorder->eAudioCodingType)
    {
        if(pRecorder->nAudioSampleRate == 16000)
            AudioEncoder = g_AmrWBEncoder;
        else
            AudioEncoder = g_AmrEncoder;
        if(pRecorder->eOutputFormat == NvxOUTPUT_FORMAT_RAW_AMR)
            WriterAudioPort = 0;
    }
    else if (OMX_AUDIO_CodingPCM == pRecorder->eAudioCodingType)
    {
        AudioEncoder = g_WavEncoder;
        WriterAudioPort = 0;
    }
    else
    {
        AudioEncoder = g_AacEncoder;
    }

    CheckAudioParams(pRecorder);
    NvxRecorderEnablePort(&pRecorder->m_pAVWriter->ports[WriterAudioPort], OMX_TRUE);

    AudioCapture = (pRecorder->useExternalAudioSource == OMX_TRUE)? \
                        pRecorder->externalAudioSource : g_AudioCapturer;

    if(pRecorder->useExternalAudioSource)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,"%s: Using External Audio Source : %s\n",  \
                                        __FUNCTION__, AudioCapture));
    }

    NVX_CHK_ERR (NvxGraphCreateComponentByName(pRecorder->m_pGraph,
                        AudioCapture, g_strAudioCapture, &pAudioCapturer));

    NvxRecorderEnablePort(&pAudioCapturer->ports[0], OMX_TRUE);/* audio out*/
    {
        OMX_AUDIO_PARAM_PCMMODETYPE oCapturerPcm;
        INIT_PARAM(oCapturerPcm);
        oCapturerPcm.nPortIndex = 0;
        OMX_GetParameter( pAudioCapturer->handle, OMX_IndexParamAudioPcm, &oCapturerPcm);
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,"%s[%d] : Capture sampling freq = %d\n",
                        __FUNCTION__, __LINE__, pRecorder->nAudioSampleRate));
        oCapturerPcm.nSamplingRate = pRecorder->nAudioSampleRate;
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,"%s[%d] : Capture channels = %d\n",
                        __FUNCTION__, __LINE__, pRecorder->nChannels));
        oCapturerPcm.nChannels = pRecorder->nChannels;
        OMX_SetParameter( pAudioCapturer->handle, OMX_IndexParamAudioPcm, &oCapturerPcm);

    }

    {
        OMX_PARAM_SETAUDIOSOURCE oAudioSource;
        INIT_PARAM(oAudioSource);
        oAudioSource.nPortIndex = 0;
        oAudioSource.AudioSourceParam = (int) pRecorder->eAudioSource;

        eError = OMX_SetParameter(pAudioCapturer->handle, (OMX_INDEXTYPE)SetAudioSourceParamExt,
                &oAudioSource);
        NVX_CHK_ERR(eError);
    }

    NVX_CHK_ERR (NvxConnectComponentToClock(pAudioCapturer));

    pRecorder->m_pAudioCapturer = pAudioCapturer;
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,"%s[%d] : pAudioCapturer = 0x%08X is added in graph\n",
                    __FUNCTION__, __LINE__, pRecorder->m_pAudioCapturer));

    // audio encoder component
    NVX_CHK_ERR (NvxGraphCreateComponentByName(pRecorder->m_pGraph, AudioEncoder, g_strAudioEncoder, &pAudioEncoder));

    NvxRecorderEnablePort(&pAudioEncoder->ports[0], OMX_TRUE);/* audio in */
    NvxRecorderEnablePort(&pAudioEncoder->ports[1], OMX_TRUE);/* audio in */

    if (OMX_AUDIO_CodingAAC == pRecorder->eAudioCodingType)
    {
        OMX_AUDIO_PARAM_AACPROFILETYPE oAacProfile;
        INIT_PARAM(oAacProfile);
        oAacProfile.nPortIndex = 1;
        OMX_GetParameter( pAudioEncoder->handle, OMX_IndexParamAudioAac, &oAacProfile);
        oAacProfile.eAACProfile = pRecorder->eAacProfileType;
        oAacProfile.nChannels = pRecorder->nChannels;
        oAacProfile.nSampleRate = pRecorder->nAudioSampleRate;
        oAacProfile.nBitRate = pRecorder->nAudioBitRate;
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,"%s[%d]: Encode bitrate for AAC = %d\n",
                    __FUNCTION__, __LINE__, oAacProfile.nBitRate));
        OMX_SetParameter( pAudioEncoder->handle, OMX_IndexParamAudioAac, &oAacProfile);
        OMX_SetParameter( pRecorder->m_pAVWriter->handle, OMX_IndexParamAudioAac, &oAacProfile);
    }

    if (OMX_AUDIO_CodingAMR == pRecorder->eAudioCodingType)
    {
        OMX_U32 i;

        OMX_AUDIO_PARAM_AMRTYPE oAmrProfile;
        INIT_PARAM(oAmrProfile);
        oAmrProfile.nPortIndex = 1;
        OMX_GetParameter( pAudioEncoder->handle, OMX_IndexParamAudioAmr, &oAmrProfile);

        for (i = 0; i < NO_AMR_BITRATES; i++)
        {
            if (pRecorder->nAudioBitRate == BitRates_AMR[i])
            {
                oAmrProfile.eAMRBandMode = i;
                oAmrProfile.nBitRate = pRecorder->nAudioBitRate;
                break;
            }
        }
        pRecorder->nAudioBitRate = oAmrProfile.nBitRate;
        pRecorder->nChannels = oAmrProfile.nChannels;
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,"%s[%d]: Encode bitrate for AMR = %d\n",
                        __FUNCTION__, __LINE__, oAmrProfile.eAMRBandMode));
        oAmrProfile.eAMRDTXMode = OMX_AUDIO_AMRDTXModeOff;
        OMX_SetParameter( pAudioEncoder->handle, OMX_IndexParamAudioAmr, &oAmrProfile);
        OMX_SetParameter( pRecorder->m_pAVWriter->handle, OMX_IndexParamAudioAmr, &oAmrProfile);
    }
    if (OMX_AUDIO_CodingPCM == pRecorder->eAudioCodingType)
    {
        OMX_AUDIO_PARAM_PCMMODETYPE oPcmProfile;
        INIT_PARAM(oPcmProfile);
        oPcmProfile.nPortIndex = 1;
        OMX_GetParameter( pAudioEncoder->handle, OMX_IndexParamAudioPcm, &oPcmProfile);
        oPcmProfile.nChannels = pRecorder->nChannels;
        oPcmProfile.nBitPerSample = 16;
        oPcmProfile.nSamplingRate = pRecorder->nAudioSampleRate;
        oPcmProfile.ePCMMode = OMX_AUDIO_PCMModeLinear;
        OMX_SetParameter( pAudioEncoder->handle, OMX_IndexParamAudioPcm, &oPcmProfile);
    }

    pRecorder->m_pAudioEncoder = pAudioEncoder;
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,"%s[%d]:  m_pAudioEncoder = 0x%08X is added in graph\n",
                    __FUNCTION__, __LINE__, pRecorder->m_pAudioEncoder));

    NVX_CHK_ERR (NvxConnectTunneled(pRecorder->m_pAudioCapturer, 0, pRecorder->m_pAudioEncoder, 0));
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,"%s[%d]: NvxConnectTunneled Success between m_pAudioCapturer and m_pAudioEncoder\n",
                    __FUNCTION__, __LINE__));

    NVX_CHK_ERR (NvxConnectTunneled(pRecorder->m_pAudioEncoder, 1, pRecorder->m_pAVWriter, WriterAudioPort));
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,"%s[%d]: NvxConnectTunneled Success between m_pAudioEncoder and m_pAVWriter\n",
                    __FUNCTION__, __LINE__));

cleanup:
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,"--%s[%d]", __FUNCTION__, __LINE__));
    return eError;
}

OMX_ERRORTYPE NvxCreateRecorderGraph(NvxRecorderGraph *pRecorder)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));

    NVX_CHK_ARG (pRecorder);

    if (pRecorder->bAllTunneled)
    {
        eError = NvxCreateTunneledRecorderGraph(pRecorder);
        return eError;
    }

    NVX_CHK_ERR (NvxGraphInit(pRecorder->m_pOmxFramework, &pRecorder->m_pGraph, OMX_TRUE));

    if (pRecorder->eRecState != NvxRecorderGraphState_Initialized)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "%s[%d] : Create Graph not ready", __FUNCTION__, __LINE__));
        eError = OMX_ErrorInvalidState;
        goto cleanup;
    }

    if ((pRecorder->AudioEncEnable == OMX_FALSE) &&
            (pRecorder->VideoEncEnable == OMX_FALSE))
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "%s[%d] : Audio/Video encoder is not set",
                        __FUNCTION__, __LINE__));
        eError = OMX_ErrorInvalidState;
        goto cleanup;

    }

    pRecorder->m_pAVWriter = NvxRecorderGraphInitializeWriter(pRecorder);
    if (NULL == pRecorder->m_pAVWriter)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "%s[%d]: NvxRecorderCreateAndInitializeWrite Failed!",
                        __FUNCTION__, __LINE__));
        eError = OMX_ErrorInvalidState;
        goto cleanup;

    }
    NvxGraphSetComponentAsEndpoint(pRecorder->m_pGraph, pRecorder->m_pAVWriter);

    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "NvxCreateRecorderGraph : Writer Initialized. pRecorder->m_pAVWriter = 0x%08X is added in graph\n",
        pRecorder->m_pAVWriter));

    if(pRecorder->VideoEncEnable == OMX_TRUE)
    {

        NvxRecorderEnablePort(&pRecorder->m_pAVWriter->ports[0], OMX_TRUE); /* video in */
        pRecorder->m_pVideoEncoder = NvxRecorderGraphInitializeVideoEncoder(pRecorder);
        if (NULL == pRecorder->m_pVideoEncoder)
        {
            NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR,"%s[%d] : NvxRecorderGraphInitializeVideoEncoder Failed!",
                            __FUNCTION__, __LINE__));
            eError = OMX_ErrorInvalidState;
            goto cleanup;
        }
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "NvxCreateRecorderGraph : VideoEncoder Initialized. pRecorder->m_pVideoEncoder = 0x%08X is added in graph\n",
            pRecorder->m_pVideoEncoder));
    }
    if(pRecorder->AudioEncEnable == OMX_TRUE)
    {
        NVX_CHK_ERR (NvxRecorderGraphInitAudioCaptureEncode(pRecorder));
    }

    if(pRecorder->VideoEncEnable == OMX_TRUE)
    {
        NVX_CHK_ERR (NvxRecorderInitNonTunneledComponents(pRecorder));
    }

    eError = NvxGraphTransitionAllToState(pRecorder->m_pGraph, OMX_StateIdle,
                                          10000);
    if (OMX_ErrorNone != eError)   {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR,"NvxCreateRecorderGraph: NvxTransitionAllComponentsToState to OMX_StateIdle Failed! NvError = 0x%08x",
            eError));
        NvxGraphTransitionAllToState( pRecorder->m_pGraph, OMX_StateLoaded, \
                                      10000);
        goto cleanup;
    }

    pRecorder->eRecState = NvxRecorderGraphState_Prepared;

    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_INFO, "NvxRecorderGraph Created"));

cleanup:
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
    return eError;
}

void NvxRecorderRegisterClientCallbacks( NvxRecorderGraph *pRecorder,
                                         void *pThis,
                                         NvxFillBufferDone pFillBufferDone,
                                         NvxEmptyBufferDone pEmptyBufferDone,
                                         NvxEventHandler pEventHandler)
{
    // Save into our cilent context.
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));
    pRecorder->RecordAppContext.pClientContext = pThis;
    pRecorder->RecordAppContext.pFillBufferDone = pFillBufferDone;
    pRecorder->RecordAppContext.pEmptyBufferDone = pEmptyBufferDone;
    pRecorder->RecordAppContext.pEventHandler = pEventHandler;
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
    return;
}

OMX_ERRORTYPE
NvxRecorderInitNonTunneledComponents ( NvxRecorderGraph *pRecorder )
{
    OMX_PARAM_PORTDEFINITIONTYPE oPortDef;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NVX_PARAM_USENVBUFFER oParam;
    OMX_INDEXTYPE eIndex;
    NvU32 width, height, size, i;

    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));

    NVX_CHK_ARG (pRecorder &&  pRecorder->m_pVideoEncoder);

    if( OMX_TRUE == pRecorder->bUseNvBuffers)
    {
        NVX_CHK_ERR (OMX_GetExtensionIndex(pRecorder->m_pVideoEncoder->handle, NVX_INDEX_CONFIG_USENVBUFFER, &eIndex));
        INIT_PARAM(oParam);
        oParam.nPortIndex = 0;
        oParam.bUseNvBuffer = OMX_TRUE;
        NVX_CHK_ERR (OMX_SetParameter(pRecorder->m_pVideoEncoder->handle, eIndex, &oParam));
    }

    INIT_PARAM(oPortDef);
    oPortDef.nPortIndex = 0;
    OMX_GetParameter(pRecorder->m_pVideoEncoder->handle, OMX_IndexParamPortDefinition, &oPortDef);
    width = oPortDef.format.image.nFrameWidth;
    height = oPortDef.format.image.nFrameHeight;
    if(OMX_TRUE != pRecorder->bUseNvBuffers)
    {
        oPortDef.nBufferSize = (width*height)<<1;
        OMX_SetParameter(pRecorder->m_pVideoEncoder->handle, OMX_IndexParamPortDefinition, &oPortDef);
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,"%s[%d] :OMX_SetParameter: size=%d\n",
                        __FUNCTION__, __LINE__, oPortDef.nBufferSize));
    }
    size = oPortDef.nBufferSize;
    oPortDef.nBufferCountActual = oPortDef.nBufferCountMin;
    {
        NvxComponent *comp = pRecorder->m_pVideoEncoder;
        OMX_U32 nBuffers, port = oPortDef.nPortIndex;
        NVX_CHK_ERR (NvxAllocateBuffersForPort(comp, port, &nBuffers));

        NVX_CHK_ARG (pRecorder->nVideoInputBuffers <= MAX_OUTPUT_BUFFERS);

        NVX_CHK_ERR (NvxGetAllocatedBuffers(comp,
                                        port,
                                        &pRecorder->VideoEncoderInputBuffer[0],
                                        nBuffers));
        for (i = 0; i < nBuffers; i++)
        {
            pRecorder->VideoEncoderInputBuffer[i]->pAppPrivate =
                               (OMX_PTR)&pRecorder->VideoEncoderInputBuffer[i];
        }
        pRecorder->nVideoInputBuffers = nBuffers;
    }
    pRecorder->oVideoEncInputPortDef = oPortDef;

cleanup:
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
    return eError;
}

void
NvxRecorderDeInitNonTunneledComponents(NvxRecorderGraph *pRecorder)
{
    NvU32 i;
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));
    {
        NvxComponent *comp = pRecorder->m_pVideoEncoder;
        OMX_U32 nBuffers = pRecorder->nVideoInputBuffers;
        OMX_U32 port = pRecorder->oVideoEncInputPortDef.nPortIndex;

        NvxFreeBuffersForPort(comp, port);
        for (i = 0; i < nBuffers; i++)
        {
            pRecorder->VideoEncoderInputBuffer[i] = NULL;
        }
        pRecorder->nVideoInputBuffers = 0;
    }
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
}

OMX_ERRORTYPE NvxChangeStateRecorderGraph(NvxRecorderGraph *pRecorder, NvxRecorderGraphState eRecGraphState)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxGraph *pRecorderGraph;
    NvxComponent *pAudioCapturer = NULL;
    OMX_CONFIG_BOOLEANTYPE     cc;
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));

    NVX_CHK_ARG (pRecorder);
    pRecorderGraph = pRecorder->m_pGraph;
    NVX_CHK_ARG (pRecorderGraph);

    if((pRecorder->RecInputType != NvxRecorderInputType_File) && (pRecorder->AudioEncEnable == OMX_TRUE))
    {
        pAudioCapturer = pRecorder->m_pAudioCapturer;
        NVX_CHK_ARG (pAudioCapturer);
    }

    if ((pRecorder->eRecState == NvxRecorderGraphState_Prepared) && (eRecGraphState == NvxRecorderGraphState_Start))
    {
        NVX_CHK_ERR (NvxGraphTransitionAllToState(pRecorderGraph, OMX_StateExecuting, \
                                              10000));

        if((pRecorder->RecInputType != NvxRecorderInputType_File) && (pRecorder->AudioEncEnable == OMX_TRUE))
        {
              cc.bEnabled = OMX_TRUE;
              OMX_SetConfig(pAudioCapturer->handle, OMX_IndexConfigCapturing, &cc);
        }
        pRecorder->eRecState = NvxRecorderGraphState_Start;
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_INFO, "NvxRecorderGraph Started"));
    }
    if ((pRecorder->eRecState == NvxRecorderGraphState_Start) && (eRecGraphState == NvxRecorderGraphState_stop))
    {
        if((pRecorder->RecInputType != NvxRecorderInputType_File) && (pRecorder->AudioEncEnable == OMX_TRUE))
        {
            cc.bEnabled = OMX_FALSE;
            OMX_SetConfig(pAudioCapturer->handle, OMX_IndexConfigCapturing, &cc);
            NvxGraphStopClock(pRecorderGraph);
        }
        if (pRecorder->VideoEncEnable ||
            pRecorder->ImageEncEnable ||
            pRecorder->PreviewEnable)
        {
            NvOsDebugPrintf(" Closing camera");
            INIT_PARAM(cc);
            OMX_INDEXTYPE eIndexPreviewEnable;
            NVX_CHK_ERR(OMX_GetExtensionIndex(
                pRecorder->m_pCamera->handle,
                NVX_INDEX_CONFIG_PREVIEWENABLE,
                &eIndexPreviewEnable));
            cc.bEnabled = OMX_FALSE;
            OMX_SetConfig(
                pRecorder->m_pCamera->handle,
                OMX_IndexConfigCapturing, &cc);
            OMX_SetConfig(
                pRecorder->m_pCamera->handle,
                eIndexPreviewEnable, &cc);
            NvOsDebugPrintf(" Shutdown the camera ports");
        }
        if (!(pRecorder->bAllTunneled))
        {
            NvxReleaseRecorderGraph(pRecorder);
            pRecorder->eRecState = NvxRecorderGraphState_stop;
        }
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_INFO, "NvxRecorderGraph Stopped"));
    }

cleanup:
    if (OMX_ErrorNone != eError && pRecorder)
    {
        if (!(pRecorder->bAllTunneled))
            NvxReleaseRecorderGraph(pRecorder);
    }
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
    return eError;
}
OMX_ERRORTYPE NvxReleaseRecorderGraph(NvxRecorderGraph *pRecorder)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));

    if (!pRecorder->m_pGraph)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "pRecorder->m_pGraph is NULL %s[%d]", __FUNCTION__, __LINE__));
        return eError;
    }

    if ((pRecorder->eRecState == NvxRecorderGraphState_Start) || \
        (pRecorder->eRecState == NvxRecorderGraphState_Prepared))
    {
       eError = NvxGraphTransitionAllToState(pRecorder->m_pGraph, OMX_StateIdle, \
                                              10000);
        if (OMX_ErrorNone != eError)
        {
            NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "State Transition to idle fails with error 0x%08X %s[%d]", eError, __FUNCTION__, __LINE__));
            // Continue to clean-up even after error
        }
        if((!(pRecorder->bAllTunneled)) && (pRecorder->VideoEncEnable == OMX_TRUE))
            NvxRecorderDeInitNonTunneledComponents(pRecorder);
    }
    NvxGraphTeardown(pRecorder->m_pGraph);
    if (pRecorder->outfile)
    {
        NvOsFree(pRecorder->outfile);
        pRecorder->outfile = NULL;
    }

    NvxGraphDeinit(pRecorder->m_pGraph);
    pRecorder->m_pGraph = NULL;

    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
    return OMX_ErrorNone;
}

void CheckAudioParams(NvxRecorderGraph *pRecorder)
{
    OMX_U32 nAudioSampleRate = 0;
    OMX_U32 nAudioBitRate = 0;
    OMX_U32 nChannels = 0;

    switch(pRecorder->eAudioCodingType)
    {
    case OMX_AUDIO_CodingAAC:
        if(pRecorder->eAacProfileType == OMX_AUDIO_AACObjectHE_PS)
        {
            nAudioSampleRate = 44100;
            nAudioBitRate = 64000;
            nChannels = 2;
        }
        else
        {
            nAudioSampleRate = 44100;
            nAudioBitRate = 128000;
            nChannels = 2;
        }
        break;
    case OMX_AUDIO_CodingAMR:
        if(pRecorder->nAudioSampleRate == 16000)
        {
            nAudioSampleRate = 16000;
            nAudioBitRate = 23850;
            nChannels = 1;
        }
        else
        {
            nAudioSampleRate = 8000;
            nAudioBitRate = 12200;
            nChannels = 1;
        }
        break;
    default:
        break;
    }
    if (pRecorder->nAudioSampleRate == INVALID_PARAM)
        pRecorder->nAudioSampleRate = nAudioSampleRate;
    if (pRecorder->nAudioBitRate == INVALID_PARAM)
        pRecorder->nAudioBitRate = nAudioBitRate;
    if (pRecorder->nChannels == INVALID_PARAM)
        pRecorder->nChannels = nChannels;

}
void CheckVideoParams(NvxRecorderGraph *pRecorder)
{
    NvxRecorderParam *pParam = &pRecorder->RecorderParam;
    if (pParam->nWidthCapture == INVALID_PARAM)
        pParam->nWidthCapture = g_DefaultRecorderParams.nWidthCapture;
    if (pParam->nHeightCapture == INVALID_PARAM)
        pParam->nHeightCapture = g_DefaultRecorderParams.nHeightCapture;
    if (pParam->nBitRate == INVALID_PARAM)
    {
        pParam->nBitRate = 192000;
        if (pParam->nWidthCapture >= 1280) {
            pParam->nBitRate = 10000000;
        } else if (pParam->nWidthCapture >= 720) {
            pParam->nBitRate = 6000000;
        } else if (pParam->nWidthCapture >= 480) {
            pParam->nBitRate = 4000000;
        } else if (pParam->nWidthCapture >= 352) {
            pParam->nBitRate = 1000000;
        } else if (pParam->nWidthCapture >= 320) {
            pParam->nBitRate = 512000;
        }
    }
}

#define UPDATE_RECORDER_PARAMS(x,y) \
 do{ \
    if(x != INVALID_PARAM) \
        y=x; \
 }while(0)

OMX_ERRORTYPE NvxRecorderSetVideoParams(NvxRecorderGraph *pRecorder,
   NvxRecorderVideoParams *pParams)
{
    NvxRecorderParam *pRecorderParam;

    if(!pRecorder || !pParams)
        return OMX_ErrorBadParameter;

    pRecorderParam = &pRecorder->RecorderParam;
    UPDATE_RECORDER_PARAMS(pParams->nVideoCaptureWidth, pRecorderParam->nWidthCapture);
    UPDATE_RECORDER_PARAMS(pParams->nVideoCaptureHeight, pRecorderParam->nHeightCapture);
    UPDATE_RECORDER_PARAMS(pParams->videoEncoderType, pRecorderParam->videoEncoderType);
    UPDATE_RECORDER_PARAMS(pParams->bUseNvBuffers, pRecorder->bUseNvBuffers);
    UPDATE_RECORDER_PARAMS(pParams->nBitRate, pRecorderParam->nBitRate);
    UPDATE_RECORDER_PARAMS(pParams->nVideoFrameRate, pRecorderParam->nFrameRate);

    pRecorder->VideoEncEnable = OMX_TRUE;

    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,"%s[%d]: SetVideoRecorderParams - CW = %d, CH = %d,  usnvbuf = %d ",
                        __FUNCTION__, __LINE__, pRecorderParam->nWidthCapture,
                        pRecorderParam->nHeightCapture, pRecorder->bUseNvBuffers));

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxRecorderSetAudioParams(NvxRecorderGraph *pRecorder,
   NvxRecorderAudioParams *pParams)
{
    if(!pRecorder || !pParams)
        return OMX_ErrorBadParameter;

    UPDATE_RECORDER_PARAMS(pParams->nChannels, pRecorder->nChannels);
    UPDATE_RECORDER_PARAMS(pParams->nAudioSampleRate, pRecorder->nAudioSampleRate);
    UPDATE_RECORDER_PARAMS(pParams->nAudioBitRate, pRecorder->nAudioBitRate);
    UPDATE_RECORDER_PARAMS(pParams->eAudioCodingType, pRecorder->eAudioCodingType);
    UPDATE_RECORDER_PARAMS(pParams->eAacProfileType, pRecorder->eAacProfileType);
    UPDATE_RECORDER_PARAMS(pParams->eAudioSource, pRecorder->eAudioSource);

    pRecorder->AudioEncEnable = OMX_TRUE;

    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,"%s[%d]: Ch = %d, SR = %d,  BR = %d, Codec=%d AS = %d\n",
                     __FUNCTION__, __LINE__, pRecorder->nChannels,
                     pRecorder->nAudioSampleRate, pRecorder->nAudioBitRate,
                     pRecorder->eAudioCodingType, pRecorder->eAudioSource));

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxRecorderSetImageParams(NvxRecorderGraph *pRecorder,
   NvxRecorderImageParams *pParams)
{
    NvxRecorderParam *pRecorderParam;
    if(!pRecorder || !pParams)
        return OMX_ErrorBadParameter;

    pRecorderParam = &pRecorder->RecorderParam;
    UPDATE_RECORDER_PARAMS(pParams->nImageCaptureWidth, pRecorderParam->nWidthCapture);
    UPDATE_RECORDER_PARAMS(pParams->nImageCaptureHeight, pRecorderParam->nHeightCapture);
    UPDATE_RECORDER_PARAMS(pParams->imageEncoderType, pRecorderParam->imageEncoderType);

    pRecorder->ImageEncEnable = OMX_TRUE;

    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,"%s[%d]: SetImageRecorderParams - CW = %d, CH = %d ",
                        __FUNCTION__, __LINE__, pRecorderParam->nWidthCapture,
                        pRecorderParam->nHeightCapture));
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxRecorderSetPreviewParams(NvxRecorderGraph *pRecorder,
   NvxPreviewParam *pParams)
{
    NvxPreviewParam *pPreviewParam;

    if(!pRecorder || !pParams)
        return OMX_ErrorBadParameter;

    pPreviewParam = &pRecorder->PreviewParam;
    UPDATE_RECORDER_PARAMS(pParams->nWidth, pPreviewParam->nWidth);
    UPDATE_RECORDER_PARAMS(pParams->nHeight, pPreviewParam->nHeight);
    pRecorder->PreviewEnable = OMX_TRUE;

    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,"%s[%d]: SetImageRecorderParams - CW = %d, CH = %d ",
                        __FUNCTION__, __LINE__, pPreviewParam->nWidth,
                        pPreviewParam->nHeight));
    return OMX_ErrorNone;
}


OMX_ERRORTYPE NvxRecorderSetOutputFile(NvxRecorderGraph *pRecorder, char *sPath)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    int len;

    NVX_CHK_ARG (pRecorder);

    len = NvOsStrlen(sPath) + 1;

    if(pRecorder->outfile)
    {
        NvOsFree(pRecorder->outfile);
        pRecorder->outfile = NULL;
    }
    pRecorder->outfile = (char*)NvOsAlloc(len);
    NVX_CHK_ARG(pRecorder->outfile);

    NvOsMemset(pRecorder->outfile, 0, len);
    NvOsStrncpy(pRecorder->outfile, sPath, len);

cleanup:
    return eError;
}


OMX_BUFFERHEADERTYPE *NvxRecorderGetVideoEncoderBuffer(NvxRecorderGraph *pRecorder,
                                                    OMX_U32 index)
{
    if(!pRecorder)
        return NULL;

    return pRecorder->VideoEncoderInputBuffer[index];
}

OMX_U32 NvxRecorderVideoEncoderGetBufferCount(NvxRecorderGraph *pRecorder)
{
    if(!pRecorder)
        return 0;

    return pRecorder->nVideoInputBuffers;
}

NvxComponent* NvxRecorderGetVideoEncoder(NvxRecorderGraph *pRecorder)
{
    if(!pRecorder || !pRecorder->m_pVideoEncoder)
        return NULL;

    return pRecorder->m_pVideoEncoder;
}

OMX_ERRORTYPE NvxRecorderSetOutputFormat(NvxRecorderGraph *pRecorder,
                                            NvxOutputFormat Params)
{
    if(!pRecorder)
        return OMX_ErrorBadParameter;

    pRecorder->eOutputFormat = Params;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxRecorderSetOutputFileSize(NvxRecorderGraph *pRecorder,
                                            OMX_U64 nSizeInBytes)
{
    if(!pRecorder)
        return OMX_ErrorBadParameter;
    NvOsDebugPrintf("in NvxRecorderSetOutputFileSize nSizeInBytes = %lld\n",nSizeInBytes);
    pRecorder->nFileSize = nSizeInBytes;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxRecorderSetMaxDuration(NvxRecorderGraph *pRecorder,
                                            OMX_U64 nDuration)
{
    if(!pRecorder)
        return OMX_ErrorBadParameter;
    NvOsDebugPrintf("in NvxRecorderSetMaxDuration nDuration = %lld\n",nDuration);
    pRecorder->nDuration = nDuration;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxRecorderSetVideoInputFileName(NvxRecorderGraph *pRecorder,
                                            char *pFileName)
{
    if(!pRecorder)
        return OMX_ErrorBadParameter;
    NvOsDebugPrintf("in NvxRecorderInputFileName InputFileName = %s\n",pFileName);
    pRecorder->pVideoInputFileName = pFileName;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxRecorderSetAudioInputFileName(NvxRecorderGraph *pRecorder,
                                            char *pFileName)
{
    if(!pRecorder)
        return OMX_ErrorBadParameter;
    NvOsDebugPrintf("in NvxRecorderInputFileName InputFileName = %s\n",pFileName);
    pRecorder->pAudioInputFileName = pFileName;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxRecorderSetImageInputFileName(NvxRecorderGraph *pRecorder,
                                            char *pFileName)
{
    if(!pRecorder)
        return OMX_ErrorBadParameter;
    NvOsDebugPrintf("in NvxRecorderInputFileName InputFileName = %s\n",pFileName);
    pRecorder->pImageInputFileName = pFileName;
    return OMX_ErrorNone;
}


