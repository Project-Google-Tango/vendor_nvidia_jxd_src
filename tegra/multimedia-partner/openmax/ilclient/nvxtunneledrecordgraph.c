/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
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

static char *g_Camera       = "OMX.Nvidia.camera";
static char *g_OvRenderYUV   = "OMX.Nvidia.std.iv_renderer.overlay.yuv420";
static char *g_VReader       = "OMX.Nvidia.video.read.large";
static char *g_AReader       = "OMX.Nvidia.audio.read";
static char *g_3gpWriter     = "OMX.Nvidia.mp4.write";
static char *g_AudioCapturer = "OMX.Nvidia.audio.capturer";
static char *g_AacEncoder    = "OMX.Nvidia.aac.encoder";
static char *g_AmrEncoder    = "OMX.Nvidia.amr.encoder";
static char *g_AmrWBEncoder  = "OMX.Nvidia.amrwb.encoder";
static char *g_H264Encoder   = "OMX.Nvidia.h264.encoder";
static char *g_Mpeg4Encoder  = "OMX.Nvidia.mp4.encoder";
static char *g_H263Encoder   = "OMX.Nvidia.h263.encoder";
static char *g_AmrWriter     = "OMX.Nvidia.amr.write";
static char *g_ImageWriter   = "OMX.Nvidia.image.write";
static char *g_JpegEncoder   = "OMX.Nvidia.jpeg.encoder";

static char *g_strAVWriter      = "AVWriter";
static char *g_strVideoEncoder  = "VideoEncoder";
static char *g_strAudioCapture  = "AudioCapture";
static char *g_strAudioEncoder  = "AudioEncoder";
static char *g_strFileReader    = "Filereader";
static char *g_strJpegEncoder   = "JpegEncoder";
static char *g_strPreviewRenderer   = "PreviewRenderer";

#define NO_AMR_BITRATES 18
static const OMX_U32 BitRates_AMR[NO_AMR_BITRATES] = { 0, 4750, 5150, 5900, 6700,
                                               7400, 7950, 10200, 12200,
                                               6600, 8850, 12650, 14250, 15850,
                                               18250, 19850, 23050, 23850};

static OMX_VERSIONTYPE vOMX;
#define INIT_PARAM(_X_)  (NvOsMemset(&(_X_), 0xDE, sizeof(_X_)),      \
    ((_X_).nSize = sizeof (_X_)), (_X_).nVersion = vOMX)

#define NVX_CHK_ERR(expr) {               \
            eError = (expr);            \
            if ((eError != OMX_ErrorNone))   \
            {                       \
                NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, \
                 "%s[%d] : %s Failed! NvError = 0x%08x",               \
                 __FUNCTION__, __LINE__, #expr, eError)); \
                goto cleanup;     \
            }                       \
        }

#define NVX_CHK_ARG(expr) {               \
            if (((expr) == 0))   \
            {                       \
                NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, \
                 "%s[%d] : %s Assert Failed!", __FUNCTION__, __LINE__, #expr)); \
                eError = NvError_BadParameter;\
                goto cleanup;\
            }                       \
        }

OMX_ERRORTYPE NvxTunneledInitializeCamera(NvxRecorderGraph *pRecorder);
OMX_ERRORTYPE NvxTunneledPreview(NvxRecorderGraph *pRecorder);
OMX_ERRORTYPE NvxTunneledInitializeImageChain(NvxRecorderGraph *pRecorder);
OMX_ERRORTYPE NvxRemoveRecComponnets(NvxRecorderGraph *pRecorder);

OMX_ERRORTYPE NvxTunneledInitializeCamera(NvxRecorderGraph *pRecorder)
{
    OMX_ERRORTYPE eError= OMX_ErrorNone;
    NvxComponent *pCamera = NULL;

    OMX_INDEXTYPE eIndex;
    OMX_PARAM_SENSORMODETYPE     oSensorMode;
    NVX_CONFIG_STEREOCAPABLE     oStCapable;
    NVX_PARAM_STEREOCAMERAMODE   oStMode;
    OMX_PARAM_PORTDEFINITIONTYPE oPortDef;
    NvxRecorderParam recParam = pRecorder->RecorderParam;
    NvxPreviewParam previewParam = pRecorder->PreviewParam;

    if (pRecorder->m_pCamera)
        return eError;

    NVX_CHK_ERR (NvxGraphCreateComponentByName (
        pRecorder->m_pGraph,
        g_Camera,
        g_strFileReader,
        &pCamera));

    NVX_CHK_ERR (OMX_SendCommand (
        pCamera->handle,
        OMX_CommandPortDisable,
        OMX_ALL, 0));

    if (pRecorder->PreviewEnable)
    {
        INIT_PARAM(oPortDef);
        oPortDef.nPortIndex = 0;
        NVX_CHK_ERR (OMX_GetParameter (
            pCamera->handle,
            OMX_IndexParamPortDefinition,
            &oPortDef));
        oPortDef.format.video.nFrameWidth = previewParam.nWidth;
        oPortDef.format.video.nFrameHeight = previewParam.nHeight;

        NVX_CHK_ERR (OMX_SetParameter(
            pCamera->handle,
            OMX_IndexParamPortDefinition,
            &oPortDef));
       {
            OMX_CONFIG_ROTATIONTYPE oRotation;
            INIT_PARAM(oRotation);
            oRotation.nPortIndex = 0;
            oRotation.nRotation = 0;
            NVX_CHK_ERR (OMX_SetConfig (
                pCamera->handle,
                OMX_IndexConfigCommonRotate,
                &oRotation));
        }
        {
            OMX_CONFIG_BOOLEANTYPE    cc;
            OMX_INDEXTYPE eIndexPreviewEnable;
            INIT_PARAM(cc);
            NVX_CHK_ERR (OMX_GetExtensionIndex (
                pCamera->handle,
                NVX_INDEX_CONFIG_PREVIEWENABLE,
                &eIndexPreviewEnable));

            cc.bEnabled = OMX_TRUE;
            OMX_SetConfig(pCamera->handle, eIndexPreviewEnable, &cc);
        }
        {
            NVX_CHK_ERR(NvxRecorderEnablePort(&pCamera->ports[0], OMX_TRUE));
        }
    }

    NVX_CHK_ERR(OMX_GetExtensionIndex (
        pCamera->handle,
        NVX_INDEX_CONFIG_STEREOCAPABLE,
        &eIndex));

    INIT_PARAM(oStCapable);
    NVX_CHK_ERR(OMX_GetConfig(pCamera->handle, eIndex, &oStCapable));

    if (oStCapable.StereoCapable)
    {
        INIT_PARAM(oStMode);
        //Left only
        oStMode.StereoCameraMode = 0x01;
        NVX_CHK_ERR(OMX_GetExtensionIndex (
            pCamera->handle,
            NVX_INDEX_PARAM_STEREOCAMERAMODE,
            &eIndex));

        NVX_CHK_ERR(OMX_SetParameter(pCamera->handle, eIndex, &oStMode));
    }

    {
        OMX_INDEXTYPE  eIndexSensorId;
        OMX_PARAM_U32TYPE  oSensorIdParam;
        INIT_PARAM(oSensorIdParam);
        oSensorMode.nPortIndex = 1;
        oSensorIdParam.nU32 = 0;
        NVX_CHK_ERR (OMX_GetExtensionIndex (
            pCamera->handle,
            NVX_INDEX_PARAM_SENSORID,
            &eIndexSensorId));
        NVX_CHK_ERR (OMX_SetParameter (
            pCamera->handle,
            eIndexSensorId,
            &oSensorIdParam));
    }

    {
        OMX_CONFIG_ROTATIONTYPE oRotation;
        INIT_PARAM(oRotation);
        oRotation.nPortIndex = 1;
        oRotation.nRotation = 0;
        NVX_CHK_ERR (OMX_SetConfig (
            pCamera->handle,
            OMX_IndexConfigCommonRotate,
            &oRotation));
    }

    INIT_PARAM(oSensorMode);
    oSensorMode.nPortIndex = 1;
    NVX_CHK_ERR (OMX_GetParameter (
        pCamera->handle,
        OMX_IndexParamCommonSensorMode,
        &oSensorMode));

    if(pRecorder->ImageEncEnable)
        oSensorMode.bOneShot = OMX_TRUE;
    else
        oSensorMode.bOneShot = OMX_FALSE;

    NVX_CHK_ERR (OMX_SetParameter (
        pCamera->handle,
        OMX_IndexParamCommonSensorMode,
        &oSensorMode));

    NVX_CHK_ERR (NvxConnectComponentToClock(pCamera));

    if(pRecorder->VideoEncEnable || pRecorder->ImageEncEnable)
    {
        INIT_PARAM(oPortDef);
        oPortDef.nPortIndex = 1;
        NVX_CHK_ERR (OMX_GetParameter (
            pCamera->handle,
            OMX_IndexParamPortDefinition,
            &oPortDef));
        oPortDef.format.video.nFrameWidth = recParam.nWidthCapture;
        oPortDef.format.video.nFrameHeight = recParam.nHeightCapture;
        NVX_CHK_ERR (OMX_SetParameter (
            pCamera->handle,
            OMX_IndexParamPortDefinition,
            &oPortDef));
        OMX_CONFIG_BOOLEANTYPE    cc;
        INIT_PARAM(cc);
        cc.bEnabled = OMX_TRUE;
        NVX_CHK_ERR (OMX_SetConfig (
            pCamera->handle,
            OMX_IndexConfigCapturing,
            &cc));
        //Enable camera capture port
        NVX_CHK_ERR (NvxRecorderEnablePort(&pCamera->ports[1], OMX_TRUE));
    }
    pRecorder->m_pCamera = pCamera;
cleanup:
    return eError;
}

OMX_ERRORTYPE NvxTunneledAudioCapture(NvxRecorderGraph *pRecorder)
{
    char *AudioCapture = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent *pAudioCapturer = NULL;
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
        "++%s[%d]", __FUNCTION__, __LINE__));
    NVX_CHK_ARG (pRecorder &&  (pRecorder->AudioEncEnable != OMX_FALSE));
    if (pRecorder->m_pAudioCapturer)
        return eError;
    CheckAudioParams(pRecorder);
    AudioCapture = g_AudioCapturer;
    NVX_CHK_ERR (NvxGraphCreateComponentByName(pRecorder->m_pGraph,
                        AudioCapture, g_strAudioCapture, &pAudioCapturer));
    NVX_CHK_ERR(NvxRecorderEnablePort(&pAudioCapturer->ports[0], OMX_TRUE));
    {
        OMX_AUDIO_PARAM_PCMMODETYPE oCapturerPcm;
        INIT_PARAM(oCapturerPcm);
        oCapturerPcm.nPortIndex = 0;

        NVX_CHK_ERR(OMX_GetParameter (
            pAudioCapturer->handle,
            OMX_IndexParamAudioPcm,
            &oCapturerPcm));

        oCapturerPcm.nSamplingRate = pRecorder->nAudioSampleRate;

        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
            "%s[%d] : Capture sampling freq = %d\n",
            __FUNCTION__,
            __LINE__,
            pRecorder->nAudioSampleRate));

        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
            "%s[%d] : Capture channels = %d\n",
            __FUNCTION__,
            __LINE__,
            pRecorder->nChannels));

        oCapturerPcm.nChannels = pRecorder->nChannels;
        eError = OMX_SetParameter (
            pAudioCapturer->handle,
            OMX_IndexParamAudioPcm,
            &oCapturerPcm);

        if(eError != OMX_ErrorNone)
        {
            NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR,
                "ALSA capture module gives it, but can be excused"));
        }
    }
    {
        OMX_PARAM_SETAUDIOSOURCE oAudioSource;
        INIT_PARAM(oAudioSource);
        oAudioSource.nPortIndex = 0;
        oAudioSource.AudioSourceParam = (int) pRecorder->eAudioSource;
        eError = OMX_SetParameter (
            pAudioCapturer->handle,
            (OMX_INDEXTYPE)SetAudioSourceParamExt,
            &oAudioSource);

        if(eError != OMX_ErrorNone)
        {
            NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR,
                "ALSA capture module gives it, but can be excused"));
        }
    }

    NVX_CHK_ERR (NvxConnectComponentToClock(pAudioCapturer));
    pRecorder->m_pAudioCapturer = pAudioCapturer;
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
        "%s[%d] : pAudioCapturer = 0x%08X is added in graph\n",
        __FUNCTION__, __LINE__, pRecorder->m_pAudioCapturer));

cleanup:
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
        "--%s[%d]", __FUNCTION__, __LINE__));
    return eError;
}

OMX_ERRORTYPE NvxTunneledPreview(NvxRecorderGraph *pRecorder)
{
    OMX_ERRORTYPE eError= OMX_ErrorNone;
    NvxComponent *pYuvReader = NULL;
    NvxComponent *pVideoRenderer = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE oPortDef;
    OMX_HANDLETYPE hPreviewRender = NULL;
    NvxPreviewParam PreviewParam = pRecorder->PreviewParam;

    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
        "++%s[%d]", __FUNCTION__, __LINE__));
    if (pRecorder->RecInputType == NvxRecorderInputType_File)
    {
        {
            OMX_INDEXTYPE eIndexParamFilename;
            NVX_PARAM_FILENAME oFilenameParam;
            OMX_STRING filename =(OMX_STRING) NvOsAlloc(128);
            NVX_CHK_ERR (NvxGraphCreateComponentByName (
                pRecorder->m_pGraph,
                g_VReader,
                g_strFileReader,
                &pYuvReader));

            NVX_CHK_ERR(OMX_SendCommand (
                pYuvReader->handle,
                OMX_CommandPortDisable,
                OMX_ALL,
                0));

            NvOsStrncpy(filename,"/home/ubuntu/input.yuv", 23);
            NVX_CHK_ERR(OMX_GetExtensionIndex (
                pYuvReader->handle,
                NVX_INDEX_PARAM_FILENAME,
                &eIndexParamFilename));
            INIT_PARAM(oFilenameParam);
            oFilenameParam.pFilename = filename;
            NVX_CHK_ERR(OMX_SetParameter (
                pYuvReader->handle,
                eIndexParamFilename,
                &oFilenameParam));
        }
        {
            //Set Size of input  Unit
            OMX_VIDEO_CONFIG_NALSIZE oNalSize;
            OMX_U32 nDataSize = (PreviewParam.nWidth * PreviewParam.nHeight * 3)/2;
            INIT_PARAM(oNalSize);
            oNalSize.nNaluBytes = nDataSize ;
            eError = OMX_SetConfig( pYuvReader->handle,
                                                OMX_IndexConfigVideoNalSize,
                                                &oNalSize);
            if ( eError != OMX_ErrorNone )
            {
                // Non-fatal error. Just continue reading
                // maximum amount of data available
                NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR,
                    "Unable to specify specific read size for input\n"));
            }
        }
        {
            INIT_PARAM(oPortDef);
            oPortDef.nPortIndex = 0;
            NVX_CHK_ERR(OMX_GetParameter (
                pYuvReader->handle,
                OMX_IndexParamPortDefinition,
                &oPortDef));
            oPortDef.format.video.nFrameWidth = PreviewParam.nWidth;
            oPortDef.format.video.nFrameHeight =PreviewParam.nHeight;
            NVX_CHK_ERR(OMX_SetParameter (
                pYuvReader->handle,
                OMX_IndexParamPortDefinition,
                &oPortDef));
        }
        NVX_CHK_ERR(NvxRecorderEnablePort(&pYuvReader->ports[0], OMX_TRUE));
    }
    else if (pRecorder->m_pCamera)
    {
        pYuvReader = pRecorder->m_pCamera;
    }
    else
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR,
            " ERROR: Camera Component not found\n"));
        goto cleanup;
    }

    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
        " Creating Preview Component\n"));
    NVX_CHK_ERR (NvxGraphCreateComponentByName (
        pRecorder->m_pGraph,
        g_OvRenderYUV,
        g_strPreviewRenderer,
        &pVideoRenderer));

    hPreviewRender = pVideoRenderer->handle;
    NVX_CHK_ERR(OMX_SendCommand (
        hPreviewRender,
        OMX_CommandPortDisable,
        OMX_ALL,
        0));

    INIT_PARAM(oPortDef);
    oPortDef.nPortIndex = 0;
    NVX_CHK_ERR(OMX_GetParameter (
        hPreviewRender,
        OMX_IndexParamPortDefinition,
        &oPortDef));
    oPortDef.format.video.nFrameWidth = PreviewParam.nWidth;
    oPortDef.format.video.nFrameHeight =PreviewParam.nHeight;
    NVX_CHK_ERR(OMX_SetParameter (
        hPreviewRender,
        OMX_IndexParamPortDefinition,
        &oPortDef));

    {   // Set Output position
        OMX_CONFIG_POINTTYPE oPoint;
        OMX_FRAMESIZETYPE oSize;

        INIT_PARAM(oPoint);
        oPoint.nPortIndex = 0;
        oPoint.nX = PreviewParam.nWidth;
        oPoint.nY = PreviewParam.nHeight;

        NVX_CHK_ERR(OMX_SetConfig (
            hPreviewRender,
            OMX_IndexConfigCommonOutputPosition,
            &oPoint));
        INIT_PARAM(oSize);
        oSize.nPortIndex = 0;
        oSize.nWidth = PreviewParam.nWidth;
        oSize.nHeight = PreviewParam.nHeight;

        NVX_CHK_ERR(OMX_SetConfig (
            hPreviewRender,
            OMX_IndexConfigCommonOutputSize,
            &oSize));
    }


    {   // Set output rectangle
        OMX_CONFIG_RECTTYPE oCrop;
        INIT_PARAM(oCrop);
        oCrop.nPortIndex = 0;
        oCrop.nLeft = 0;
        oCrop.nTop = 0;
        oCrop.nWidth = PreviewParam.nWidth;
        oCrop.nHeight = PreviewParam.nHeight;
        NVX_CHK_ERR(OMX_SetConfig (
            hPreviewRender,
            OMX_IndexConfigCommonInputCrop,
            &oCrop));
    }
    {  //Set output Aspect
        OMX_INDEXTYPE eIndex;
        NVX_CONFIG_KEEPASPECT aspect;
        INIT_PARAM(aspect);
        NVX_CHK_ERR (OMX_GetExtensionIndex (
            hPreviewRender,
            NVX_INDEX_CONFIG_KEEPASPECT,
            &eIndex));
        aspect.bKeepAspect = OMX_TRUE;
        OMX_SetConfig(hPreviewRender, eIndex, &aspect);
    }
    NvxRecorderEnablePort(&pVideoRenderer->ports[0], OMX_TRUE); //video in
    NVX_CHK_ERR(NvxConnectTunneled(pYuvReader, 0, pVideoRenderer, 0));
    NVX_CHK_ERR(NvxRecorderEnablePort(&pVideoRenderer->ports[1], OMX_TRUE));
    NVX_CHK_ERR(NvxConnectComponentToClock(pVideoRenderer));
cleanup:
    return eError;

}

OMX_ERRORTYPE NvxTunneledInitializeWriter(NvxRecorderGraph *pRecorder)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_HANDLETYPE hWriterHandle;
    NvxComponent *pWriter = NULL;
    NvxGraph *pGraph;
    char *AVWriterName = NULL;
    if (pRecorder->m_pAVWriter)
        return eError;

    if(pRecorder->eOutputFormat == NvxOUTPUT_FORMAT_RAW_AMR)
    {
        AVWriterName = g_AmrWriter;
        pRecorder->audiowriterport = 0;
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
            " Enabling AMR Audio Writer \n"));
    }
    else if ((pRecorder->eOutputFormat == NvxOUTPUT_FORMAT_MP4) ||
        (pRecorder->eOutputFormat == NvxOUTPUT_FORMAT_THREE_GPP))
    {
        AVWriterName = g_3gpWriter;
        pRecorder->audiowriterport = 1;
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
            " Enabling 3GP Audio Writer \n"));
    }
    else if(pRecorder->eOutputFormat == NvxOUTPUT_FORMAT_JPG)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
            " Init Image Writer \n"));
        AVWriterName = g_ImageWriter;
    }
    else
    {
        NvOsDebugPrintf(" Error No Good Format %d\n", pRecorder->eOutputFormat);
        eError = OMX_ErrorBadParameter;
        goto cleanup;
    }

    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
        "++%s[%d]", __FUNCTION__, __LINE__));

    NVX_CHK_ARG (pRecorder && pRecorder->m_pGraph);
    pGraph = pRecorder->m_pGraph;
    NVX_CHK_ERR (NvxGraphCreateComponentByName(pGraph,
        AVWriterName,
        g_strAVWriter,
        &pWriter));

    hWriterHandle = pWriter->handle;
    OMX_SendCommand(hWriterHandle, OMX_CommandPortDisable, OMX_ALL, 0);

    NvxRecorderAppContext *pAppCtx =  NULL;
    pAppCtx = &pRecorder->RecordAppContext;

    // Set Recorder Event handler
    NVX_CHK_ERR(NvxGraphSetCompEventHandler(
        pWriter,
        pAppCtx->pClientContext,
        pAppCtx->pEventHandler));

    if (pRecorder->outfile)
    {
        OMX_INDEXTYPE eIndexParamFilename;
        NVX_PARAM_FILENAME oFilenameParam;

        NVX_CHK_ERR (OMX_GetExtensionIndex (
            hWriterHandle,
            NVX_INDEX_PARAM_FILENAME,
            &eIndexParamFilename));
        INIT_PARAM(oFilenameParam);
        oFilenameParam.pFilename = pRecorder->outfile;
        NVX_CHK_ERR(OMX_SetParameter (
            hWriterHandle,
            eIndexParamFilename,
            &oFilenameParam));
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
            " Setting output file name is done\n"));
    }

    if ((NvxOUTPUT_FORMAT_MP4 == pRecorder->eOutputFormat) ||
        (NvxOUTPUT_FORMAT_THREE_GPP == pRecorder->eOutputFormat))
    {
        OMX_INDEXTYPE eIndexParamOutputFormat;
        NVX_PARAM_OUTPUTFORMAT oFormat;

        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
            " Setting output format as mp4\n"));

        NVX_CHK_ERR (OMX_GetExtensionIndex(hWriterHandle,
            NVX_INDEX_PARAM_OUTPUTFORMAT, &eIndexParamOutputFormat));
        INIT_PARAM(oFormat);
        oFormat.OutputFormat = "mp4";
        eError = OMX_SetParameter(hWriterHandle, eIndexParamOutputFormat, &oFormat);
        NVX_CHK_ERR(eError);
    }

    if(NvxOUTPUT_FORMAT_RAW_AMR == pRecorder->eOutputFormat)
    {
        OMX_INDEXTYPE eIndexParamOutputFormat;
        NVX_PARAM_OUTPUTFORMAT oFormat;

        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
            " Setting output format as amr\n"));

        NVX_CHK_ERR (OMX_GetExtensionIndex(hWriterHandle,
            NVX_INDEX_PARAM_OUTPUTFORMAT, &eIndexParamOutputFormat));

        INIT_PARAM(oFormat);
        oFormat.OutputFormat = "amr";
        OMX_SetParameter(hWriterHandle, eIndexParamOutputFormat, &oFormat);
    }

    pRecorder->m_pAVWriter = pWriter;
    NvxGraphSetComponentAsEndpoint (
        pRecorder->m_pGraph,
        pRecorder->m_pAVWriter);
cleanup:
    return eError;
}
OMX_ERRORTYPE NvxTunneledInitializeAudioChain(NvxRecorderGraph *pRecorder)
{
    OMX_ERRORTYPE eError= OMX_ErrorNone;
    char *aEncoder =NULL;
    if (pRecorder->m_pAudioEncoder)
        return eError;

    if(OMX_AUDIO_CodingAMR == pRecorder->eAudioCodingType)
    {
        if(pRecorder->nAudioSampleRate == 16000)
            aEncoder = g_AmrWBEncoder;
        else
            aEncoder = g_AmrEncoder;
    }
    else
    {
        aEncoder = g_AacEncoder;
    }

    CheckAudioParams(pRecorder);

    if (pRecorder->RecInputType == NvxRecorderInputType_File)
    {
        NvxComponent *pAudioReader = NULL;
        OMX_HANDLETYPE hAudioReader = NULL;
        OMX_INDEXTYPE eIndexParamFilename;
        NVX_PARAM_FILENAME oFilenameParam;
        NVX_CHK_ERR (NvxGraphCreateComponentByName (
            pRecorder->m_pGraph,
            g_AReader,
            g_strFileReader,
            &pAudioReader));
        hAudioReader = pAudioReader->handle;
        OMX_SendCommand(hAudioReader, OMX_CommandPortDisable, OMX_ALL, 0);
        eError = OMX_GetExtensionIndex (
            hAudioReader,
            NVX_INDEX_PARAM_FILENAME,
            &eIndexParamFilename);
        INIT_PARAM(oFilenameParam);
        oFilenameParam.pFilename = pRecorder->pAudioInputFileName;
        NVX_CHK_ERR (OMX_SetParameter (
            hAudioReader,
            eIndexParamFilename,
            &oFilenameParam));
        NvxRecorderEnablePort(&pAudioReader->ports[0], OMX_TRUE);
        pRecorder->m_pAudioReader = pAudioReader;
    }
    else if (pRecorder->m_pAudioCapturer)
    {
        pRecorder->m_pAudioReader = pRecorder->m_pAudioCapturer;
    }
    else
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR,
            " Audio Capture Module Not Found \n"));
        return OMX_ErrorBadParameter;
    }
    {
        NvxComponent *pAudioEncoder = NULL;
        OMX_HANDLETYPE hAudioEncoder = NULL;
        NVX_CHK_ERR (NvxGraphCreateComponentByName (
            pRecorder->m_pGraph,
            aEncoder,
            g_strAudioEncoder,
            &pAudioEncoder));
        hAudioEncoder = pAudioEncoder->handle;
        OMX_SendCommand(hAudioEncoder, OMX_CommandPortDisable, OMX_ALL, 0);
        NvxRecorderEnablePort(&pAudioEncoder->ports[0], OMX_TRUE);// audio raw in
        NvxRecorderEnablePort(&pAudioEncoder->ports[1], OMX_TRUE);//audio encoded out
        pRecorder->m_pAudioEncoder = pAudioEncoder;

        //SET AUDIO PCM PARAMETERS FOR INPUT PORT OF ENCODER
        {
            OMX_AUDIO_PARAM_PCMMODETYPE oInputPcm;
            INIT_PARAM(oInputPcm);
            oInputPcm.nPortIndex = 0;
            NVX_CHK_ERR(OMX_GetParameter (
                hAudioEncoder,
                OMX_IndexParamAudioPcm,
                &oInputPcm));
            oInputPcm.nSamplingRate = pRecorder->nAudioSampleRate;
            oInputPcm.nChannels = pRecorder->nChannels;
            NVX_CHK_ERR(OMX_SetParameter (
                hAudioEncoder,
                OMX_IndexParamAudioPcm,
                &oInputPcm));
        }

        if (OMX_AUDIO_CodingAAC == pRecorder->eAudioCodingType)
        {
            OMX_AUDIO_PARAM_AACPROFILETYPE oAacProfile;

            // Set AAC Profile for Audio Encoder
            INIT_PARAM(oAacProfile);
            oAacProfile.nPortIndex = 1;
            OMX_GetParameter( hAudioEncoder, OMX_IndexParamAudioAac, &oAacProfile);
            oAacProfile.eAACProfile   = pRecorder->eAacProfileType;
            oAacProfile.nChannels     = pRecorder->nChannels;
            oAacProfile.nSampleRate = pRecorder->nAudioSampleRate;
            oAacProfile.nBitRate        = pRecorder->nAudioBitRate;
            NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
                "%s[%d]: Encode bitrate for AAC = %d\n",
                        __FUNCTION__, __LINE__, oAacProfile.nBitRate));
            OMX_SetParameter( hAudioEncoder, OMX_IndexParamAudioAac, &oAacProfile);
            OMX_SetParameter (
                pRecorder->m_pAVWriter->handle,
                OMX_IndexParamAudioAac,
                &oAacProfile);
        }

        if (OMX_AUDIO_CodingAMR == pRecorder->eAudioCodingType)
        {
            OMX_U32 i;

            OMX_AUDIO_PARAM_AMRTYPE oAmrProfile;
            INIT_PARAM(oAmrProfile);
            oAmrProfile.nPortIndex = 1;
            OMX_GetParameter( hAudioEncoder, OMX_IndexParamAudioAmr, &oAmrProfile);

            for (i = 0; i < NO_AMR_BITRATES; i++)
            {
                if (pRecorder->nAudioBitRate == BitRates_AMR[i])
                {
                    oAmrProfile.eAMRBandMode = i;
                    oAmrProfile.nBitRate = pRecorder->nAudioBitRate;
                    break;
                }
            }
            oAmrProfile.nBitRate = pRecorder->nAudioBitRate;
            oAmrProfile.nChannels = pRecorder->nChannels;
            NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
                "%s[%d]: Encode bitrate for AMR = %d\n",
                __FUNCTION__, __LINE__, oAmrProfile.eAMRBandMode));
            oAmrProfile.eAMRDTXMode = OMX_AUDIO_AMRDTXModeOff;
            OMX_SetParameter( hAudioEncoder, OMX_IndexParamAudioAmr, &oAmrProfile);

            oAmrProfile.nPortIndex = pRecorder->audiowriterport;
            OMX_SetParameter(
                pRecorder->m_pAVWriter->handle,
                OMX_IndexParamAudioAmr,
                &oAmrProfile);
        }

    }

    NvxRecorderEnablePort (
        &pRecorder->m_pAVWriter->ports[pRecorder->audiowriterport],
        OMX_TRUE);  //Audio in

    NVX_CHK_ERR (NvxConnectTunneled (
        pRecorder->m_pAudioReader,
        0,
        pRecorder->m_pAudioEncoder, 0));

    NVX_CHK_ERR (NvxConnectTunneled (
        pRecorder->m_pAudioEncoder,
        1,
        pRecorder->m_pAVWriter,
        pRecorder->audiowriterport));

cleanup:
    return eError;
}

OMX_ERRORTYPE NvxTunneledInitializeVideoChain(NvxRecorderGraph *pRecorder)
{
    OMX_ERRORTYPE eError= NvSuccess;
    NvxComponent *pVideoReader;
    int videoport = 0;
    if (pRecorder->m_pVideoEncoder)
        return eError;

    if (pRecorder->RecInputType == NvxRecorderInputType_File)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
            " Using FileReader for video encoding\n"));
        {
            OMX_INDEXTYPE eIndexParamFilename;
            NVX_PARAM_FILENAME oFilenameParam;
            OMX_STRING filename =(OMX_STRING) NvOsAlloc(128);
            NVX_CHK_ERR (NvxGraphCreateComponentByName (
                pRecorder->m_pGraph,
                g_VReader,
                g_strFileReader,
                &pVideoReader));
            NVX_CHK_ERR(OMX_SendCommand (
                pVideoReader->handle,
                OMX_CommandPortDisable,
                OMX_ALL,
                0));
            NvOsStrncpy(filename,"/home/ubuntu/input.yuv",23);
            NVX_CHK_ERR(OMX_GetExtensionIndex (
                pVideoReader->handle,
                NVX_INDEX_PARAM_FILENAME,
                &eIndexParamFilename));
            INIT_PARAM(oFilenameParam);
            oFilenameParam.pFilename = filename;
            NVX_CHK_ERR(OMX_SetParameter (
                pVideoReader->handle,
                eIndexParamFilename,
                &oFilenameParam));
        }
        {
            OMX_VIDEO_CONFIG_NALSIZE oNalSize;
            OMX_U32 nDataSize = (176 * 144 * 3)/2;
            INIT_PARAM(oNalSize);
            oNalSize.nNaluBytes = nDataSize ;
            eError = OMX_SetConfig( pVideoReader->handle,
                                                OMX_IndexConfigVideoNalSize,
                                                &oNalSize);
            if ( eError != OMX_ErrorNone )
            {
                NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR,
                    "Unable to specify specific read size for input\n"));
            }
        }
        NvxRecorderEnablePort(&pVideoReader->ports[0], OMX_TRUE);
        pRecorder->m_pyuvReader = pVideoReader;
        videoport = 0;
    }
    else if (pRecorder->m_pCamera)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_INFO,
            " Using camera Source\n"));
        pRecorder->m_pyuvReader = pRecorder->m_pCamera;
        videoport = 1;
    }
    else
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR,
            " No Video Source Found\n"));
    }

    {
        NvxComponent *pVideoEncoder;
        char *vEncoder =NULL;
        if (pRecorder->RecorderParam.videoEncoderType == VIDEO_ENCODER_TYPE_MPEG4)
        {
            NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
                " Using Mpeg4 Encoder\n"));
            vEncoder = g_Mpeg4Encoder;
        }
        else if(pRecorder->RecorderParam.videoEncoderType ==VIDEO_ENCODER_TYPE_H263)
        {
            vEncoder = g_H263Encoder;
            NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
                " Using H263 Encoder\n"));
        }
        else //Deafault is assumed as H264 Encoder
        {
            NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
                " Using H264 Encoder\n"));
            vEncoder = g_H264Encoder;
           pRecorder->RecorderParam.videoEncoderType = VIDEO_ENCODER_TYPE_H264;
        }
        NVX_CHK_ERR (NvxGraphCreateComponentByName (
            pRecorder->m_pGraph,
            vEncoder,
            g_strVideoEncoder,
            &pVideoEncoder));
        //Validate params
        CheckVideoParams(pRecorder);
        //Set Enc Params
        NVX_CHK_ERR (s_SetupVideoEncoder(pRecorder, pVideoEncoder->handle));
        OMX_SendCommand(pVideoEncoder->handle, OMX_CommandPortDisable, OMX_ALL, 0);
        NvxRecorderEnablePort(&pVideoEncoder->ports[0], OMX_TRUE);  //YUV in
        NvxRecorderEnablePort(&pVideoEncoder->ports[1], OMX_TRUE);  //Encoded video out
        pRecorder->m_pVideoEncoder = pVideoEncoder;
    }
    NVX_CHK_ERR(NvxRecorderEnablePort (
        &pRecorder->m_pAVWriter->ports[0],
        OMX_TRUE));  //Video in
    NVX_CHK_ERR (NvxConnectTunneled (
        pRecorder->m_pyuvReader,
        1,
        pRecorder->m_pVideoEncoder,
        0));
    NVX_CHK_ERR (NvxConnectTunneled (
        pRecorder->m_pVideoEncoder,
        1,
        pRecorder->m_pAVWriter,
        0));
cleanup:
    return eError;
}

OMX_ERRORTYPE NvxTunneledInitializeImageChain(NvxRecorderGraph *pRecorder)
{
    OMX_ERRORTYPE eError= NvSuccess;
    NvxRecorderParam *pParam = &pRecorder->RecorderParam;
    NvxComponent *pImageReader;
    OMX_U32 imageport =1;
    if (pRecorder->RecInputType == NvxRecorderInputType_File)
    {
        {
            OMX_INDEXTYPE eIndexParamFilename;
            NVX_PARAM_FILENAME oFilenameParam;
            NVX_CHK_ERR (NvxGraphCreateComponentByName (
                pRecorder->m_pGraph,
                g_VReader,
                g_strFileReader,
                &pImageReader));
            OMX_SendCommand (
                pImageReader->handle,
                OMX_CommandPortDisable,
                OMX_ALL,
                0);
            NVX_CHK_ERR(OMX_GetExtensionIndex (
                pImageReader->handle,
                NVX_INDEX_PARAM_FILENAME,
                &eIndexParamFilename));
            INIT_PARAM(oFilenameParam);
            oFilenameParam.pFilename = pRecorder->pImageInputFileName;
            NVX_CHK_ERR(OMX_SetParameter (
                pImageReader->handle,
                eIndexParamFilename,
                &oFilenameParam));
        }
        {
            //Set Size of input  Unit
            OMX_VIDEO_CONFIG_NALSIZE oNalSize;
            OMX_U32 nLumaSize = pParam->nWidthCapture * pParam->nHeightCapture;
            INIT_PARAM(oNalSize);
            oNalSize.nNaluBytes = nLumaSize + nLumaSize/2;
            eError = OMX_SetConfig( pImageReader->handle,
                                    OMX_IndexConfigVideoNalSize,
                                   &oNalSize);
            if ( eError != OMX_ErrorNone )
            {
                NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
                    "Unable to specify specific read size for input\n"));
            }
        }
        NvxRecorderEnablePort(&pImageReader->ports[0], OMX_TRUE);
        pRecorder->m_pyuvReader = pImageReader;
        imageport = 0;
    }
    else if (pRecorder->m_pCamera)
    {
        pRecorder->m_pyuvReader = pRecorder->m_pCamera;
    }
    else
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
            " Camera Component Not found"));
        goto cleanup;
    }

    {
        NvxComponent *pImageEncoder = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE oEncPortDef;

        char *iEncoder = g_JpegEncoder; // Make Jpeg By Default
        NVX_CHK_ERR (NvxGraphCreateComponentByName (
            pRecorder->m_pGraph,
            iEncoder,
            g_strJpegEncoder,
            &pImageEncoder));
        INIT_PARAM(oEncPortDef);
        oEncPortDef.nPortIndex = 0;
        eError = OMX_GetParameter (
            pImageEncoder->handle,
            OMX_IndexParamPortDefinition,
            &oEncPortDef);
        oEncPortDef.format.image.nFrameWidth = pParam->nWidthCapture;
        oEncPortDef.format.image.nFrameHeight = pParam->nHeightCapture;
        NVX_CHK_ERR(OMX_SetParameter (
            pImageEncoder->handle,
            OMX_IndexParamPortDefinition,
            &oEncPortDef));
        OMX_SendCommand (
            pImageEncoder->handle, OMX_CommandPortDisable, OMX_ALL, 0);
        NvxRecorderEnablePort(&pImageEncoder->ports[0], OMX_TRUE);
        NvxRecorderEnablePort(&pImageEncoder->ports[1], OMX_TRUE);
        pRecorder->m_pImageEncoder = pImageEncoder;
    }
    //Video in
    NvxRecorderEnablePort(&pRecorder->m_pAVWriter->ports[0], OMX_TRUE);
    NVX_CHK_ERR (NvxConnectTunneled (
        pRecorder->m_pyuvReader,
        imageport,
        pRecorder->m_pImageEncoder,
        0));
    NVX_CHK_ERR (NvxConnectTunneled (
        pRecorder->m_pImageEncoder,
        1,
        pRecorder->m_pAVWriter,
        0));
cleanup:
    return eError;
}

/*
 * The below function will use all components in tunneled mode of operation
 */
OMX_ERRORTYPE
NvxCreateTunneledRecorderGraph (
    NvxRecorderGraph *pRecorder)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
        "NvxCreateTunneledRecorderGraph++\n"));
    NVX_CHK_ERR (NvxGraphInit(pRecorder->m_pOmxFramework,
        &pRecorder->m_pGraph,
        OMX_TRUE));
    if (pRecorder->eRecState != NvxRecorderGraphState_Initialized)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR,
            "%s[%d] : Create Graph not ready", __FUNCTION__, __LINE__));
        eError = OMX_ErrorInvalidState;
        goto cleanup;
    }

    if(pRecorder->VideoEncEnable ||
        pRecorder->AudioEncEnable ||
        pRecorder->ImageEncEnable)
    {
        NVX_CHK_ERR (NvxTunneledInitializeWriter(pRecorder));
    }

    if (pRecorder->VideoEncEnable ||
        pRecorder->ImageEncEnable ||
        pRecorder->PreviewEnable)
    {
        NVX_CHK_ERR (NvxTunneledInitializeCamera(pRecorder));
        if (pRecorder->PreviewEnable)
        {
            NVX_CHK_ERR (NvxTunneledPreview(pRecorder));
        }
    }

    if (pRecorder->AudioEncEnable)
    {
        if (pRecorder->RecInputType != NvxRecorderInputType_File)
            NVX_CHK_ERR (NvxTunneledAudioCapture(pRecorder));
        NVX_CHK_ERR (NvxTunneledInitializeAudioChain(pRecorder));
    }

    if (pRecorder->VideoEncEnable)   //Video Encoder
    {
        NVX_CHK_ERR (NvxTunneledInitializeVideoChain(pRecorder));
    }
    if(pRecorder->ImageEncEnable)
    {
        NVX_CHK_ERR (NvxTunneledInitializeImageChain(pRecorder));
    }

    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_INFO,
        " Change state of all to idle\n"));
    eError = NvxGraphTransitionAllToState (
        pRecorder->m_pGraph,
        OMX_StateIdle,
        10000);
    if (OMX_ErrorNone != eError)   {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR,
            "NvxTransitionAllComponentsToState to OMX_StateIdle Failed! NvError = 0x%08x",
            eError));
        NvxGraphTransitionAllToState( pRecorder->m_pGraph, OMX_StateLoaded, \
                                      10000);
        goto cleanup;
    }
    pRecorder->eRecState = NvxRecorderGraphState_Prepared;
cleanup:
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG,
        "--%s[%d]", __FUNCTION__, __LINE__));
    return eError;
}

OMX_ERRORTYPE NvxReleaseRecorderComponents(NvxRecorderGraph *pRecorder)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NV_LOGGER_PRINT((
        NVLOG_OMX_IL_CLIENT,
        NVLOG_DEBUG,
        "++%s[%d]", __FUNCTION__, __LINE__));

    if (!pRecorder->m_pGraph)
    {
        NV_LOGGER_PRINT((
            NVLOG_OMX_IL_CLIENT,
            NVLOG_DEBUG,
            "pRecorder->m_pGraph is NULL %s[%d]", __FUNCTION__, __LINE__));
        return eError;
    }

    if ((pRecorder->eRecState == NvxRecorderGraphState_Start) ||
        (pRecorder->eRecState == NvxRecorderGraphState_Prepared))
    {
        eError = NvxGraphTransitionAllToState(pRecorder->m_pGraph, OMX_StateIdle,
            10000);
        if (OMX_ErrorNone != eError)
        {
            NV_LOGGER_PRINT((
                NVLOG_OMX_IL_CLIENT,
                NVLOG_DEBUG,
                "State Transition to idle fails with error 0x%08X %s[%d]",
                eError, __FUNCTION__, __LINE__));
        }
    }
    NVX_CHK_ERR(NvxRemoveRecComponnets(pRecorder));
    if (pRecorder->outfile)
    {
        NvOsFree(pRecorder->outfile);
        pRecorder->outfile = NULL;
    }
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
cleanup:
    return OMX_ErrorNone;
}
OMX_ERRORTYPE NvxRemoveRecComponnets(NvxRecorderGraph *pRecorder)
{
    NvxComponent *comp, *next, *prev;
    NvxGraph *graph = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    prev = NULL;
    graph = pRecorder->m_pGraph;
    if (!graph)
        return OMX_ErrorBadParameter;
    if (graph->state == OMX_StateExecuting ||
        graph->state == OMX_StatePause)
    {
        NvxGraphStopClock(graph);
        NVX_CHK_ERR(NvxGraphTransitionAllToState(graph, OMX_StateIdle, 10000));
    }

    if (graph->state == OMX_StateIdle)
    {
        NVX_CHK_ERR(NvxGraphTransitionAllToState(graph, OMX_StateLoaded, 10000));
    }

    comp = graph->components;
    prev = comp;
//First component is always clock
    comp = comp->next;
    while (comp)
    {
        next = comp->next;
        if (comp != pRecorder->m_pCamera)
        {
            GraphFreeComp(comp);
        }
        else
        {
            prev->next = comp;
            prev = comp;
        }
        comp = next;
    }
    prev->next = NULL;
cleanup:
    return OMX_ErrorNone;
}
