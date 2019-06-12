/*
 * Copyright (c) 2010-2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvxplayergraph.h"
#include "NVOMX_DecoderExtensions.h"
#include "NVOMX_RendererExtensions.h"
#include "NVOMX_IndexExtensions.h"
#include "nvutil.h"
#include "nvmm_logger.h"
#include "nvfxmath.h"

static char *g_Reader        = "OMX.Nvidia.reader";
static char *g_AudioRender   = "OMX.Nvidia.audio.render";
static char *g_OvRenderYUV   = "OMX.Nvidia.std.iv_renderer.overlay.yuv420";
static char *g_FrameExtract  = "OMX.Nvidia.video.extractor";
static char *g_Mp4Decoder    = "OMX.Nvidia.mp4.decode";
static char *g_Mpeg2Decoder  = "OMX.Nvidia.mpeg2v.decode";
static char *g_VP6Decoder    = "OMX.Nvidia.vp6.decode";
static char *g_H264Decoder   = "OMX.Nvidia.h264.decode";
static char *g_H264SecureDecoder = "OMX.Nvidia.h264.decode.secure";
static char *g_Vc1Decoder    = "OMX.Nvidia.vc1.decode";
static char *g_Vc1SecureDecoder = "OMX.Nvidia.vc1.decode.secure";
static char *g_AacDecoder    = "OMX.Nvidia.aac.decoder";
static char *g_eAacPDecoder  = "OMX.Nvidia.eaacp.decoder";
static char *g_BsacDecoder   = "OMX.Nvidia.bsac.decoder";
static char *g_Mp3Decoder    = "OMX.Nvidia.mp3.decoder";
static char *g_WavDecoder    = "OMX.Nvidia.wav.decoder";
static char *g_WmaDecoder    = "OMX.Nvidia.wma.decoder";
static char *g_WmaProDecoder = "OMX.Nvidia.wmapro.decoder";
static char *g_WmaLSLDecoder = "OMX.Nvidia.wmalossless.decoder";
static char *g_VorbisDecoder = "OMX.Nvidia.vorbis.decoder";
static char *g_AmrDecoder    = "OMX.Nvidia.amr.decoder";
static char *g_AmrWBDecoder  = "OMX.Nvidia.amrwb.decoder";
static char *g_MJpegDecoder  = "OMX.Nvidia.mjpeg.decoder";
static char *g_TheoraDecoder = "OMX.Nvidia.theora.decoder";
static char *g_Mp2Decoder    = "OMX.Nvidia.mp2.decoder";
static char *g_JpegDecoder   = "OMX.Nvidia.jpeg.decoder";
static char *g_ImageReader   = "OMX.Nvidia.image.read";
static char *g_H263Decoder    = "OMX.Nvidia.h263.decode";

typedef struct NvxPlayerGraphPrivRec
{
    OMX_BOOL isFrameExtract;
    OMX_BOOL noAudio;
    OMX_BOOL noVideo;

    OMX_U32  fileCacheSize;

    char *vidDecoderName;
    char *audDecoderName;

    OMX_INDEXTYPE metaIndex;

    OMX_TICKS startTime;

    NvOsSemaphoreHandle firstFrameEvent;
    NvOsMutexHandle hStateMutex;

    OMX_BOOL hasBufferEvent;
    OMX_BOOL bufferingPauseGraph;
    OMX_U32  bufferingPercent;

    OMX_BOOL useExternalAudRend;
    char *externalAudRend;
} NvxPlayerGraphPriv;

static OMX_VERSIONTYPE vOMX;
#define NOTSET_U8 ((OMX_U8)0xDE)
#define INIT_PARAM1(_X_)  (NvOsMemset(&(_X_), NOTSET_U8, sizeof(_X_)), ((_X_).nSize = sizeof (_X_)), (_X_).nVersion = vOMX)
#define INIT_PARAM(a, b) do { NvOsMemset(&(a), 0, sizeof((a))); (a).nSize = sizeof((a)); (a).nVersion = NvxFrameworkGetOMXILVersion((b)->framework); } while (0)

static void PlayerGraphSendStatus(NvxPlayerGraph2 *player, NvxMediaPlayerCallbackType status)
{
    NvxMediaPlayerCallback *cb = (NvxMediaPlayerCallback *)player->appCallBack.pCallBack;
    NvOsDebugPrintf("\n$$%s[%d] %d", __FUNCTION__, __LINE__,(int)cb);
    if (cb)
    {
        cb->eventHandler(status, 0, player->appCallBack.pAppData);
    }
}

static OMX_ERRORTYPE PlayerEventHandler(NvxComponent *hComponent,
                                        OMX_PTR pAppData,
                                        OMX_EVENTTYPE eEvent,
                                        OMX_U32 nData1, OMX_U32 nData2,
                                        OMX_PTR pEventData)
{
    NvxGraph *graph = hComponent->graph;
    NvxPlayerGraph2 *player = (NvxPlayerGraph2 *)pAppData;

    switch (eEvent)
    {
        case OMX_EventError:
            NvOsSemaphoreSignal(player->priv->firstFrameEvent);
            break;

        case OMX_EventBufferFlag:
            if (nData2 & OMX_BUFFERFLAG_EOS)
            {
                if (NvxGraphIsAtEndOfStream(graph))
                {
                    NvOsDebugPrintf("\n\nNvxGraphAtEndOfStream: %s[%d]\n", __FUNCTION__, __LINE__);
                    NvOsSemaphoreSignal(player->priv->firstFrameEvent);
                    PlayerGraphSendStatus(player,NvxMediaPlayerCallbackType_EOS);
                }
            }
            break;

        case NVX_EventFirstFrameDisplayed:
            NvOsSemaphoreSignal(player->priv->firstFrameEvent);
            break;

        case NVX_EventForBuffering:
            player->priv->bufferingPauseGraph = (OMX_BOOL)nData1;
            player->priv->bufferingPercent = nData2;
            player->priv->hasBufferEvent = OMX_TRUE;
            NvxGraphSignalEndOfStreamEvent(graph);
            break;
        case NVX_StreamChangeEvent:
            PlayerGraphSendStatus(player,NvxMediaPlayerCallbackType_StreamChange);
            break;

        default:
            break;
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE PlayerGraphCreateReader(NvxPlayerGraph2 *player,
                                             char *reader,
                                             NvxComponent **comp)
{
    OMX_ERRORTYPE err;
    NvxComponent *component;
    OMX_INDEXTYPE indexParamFilename, indexCacheSize, indexParamUserAgent;
    NVX_PARAM_FILENAME filenameParam;
    NVX_CONFIG_FILECACHESIZE cacheParam;
    NVX_PARAM_USERAGENT useragent;

    *comp = NULL;

    err = NvxGraphCreateComponentByName(player->graph, reader, "READER",
                                        &component);
    if (OMX_ErrorNone != err)
    {
        return err;
    }

    if (player->userAgent)
    {
        err = OMX_GetExtensionIndex(component->handle, NVX_INDEX_PARAM_USERAGENT,
                                &indexParamUserAgent);
        if (OMX_ErrorNone != err)
        {
            return err;
        }

        INIT_PARAM(useragent, player);
        useragent.pUserAgentStr = (char *)player->userAgent;
        err = OMX_SetParameter(component->handle, indexParamUserAgent,
                               &useragent);
    }

    // for now, only care about disabling this
    if (player->priv->fileCacheSize == 0)
    {
        err = OMX_GetExtensionIndex(component->handle, 
                                    NVX_INDEX_CONFIG_FILECACHESIZE,
                                    &indexCacheSize);
        if (OMX_ErrorNone == err)
        {
            INIT_PARAM(cacheParam, player);
            cacheParam.nFileCacheSize = player->priv->fileCacheSize;

            OMX_SetConfig(component->handle, indexCacheSize, &cacheParam);
        }
    }

    err = OMX_GetExtensionIndex(component->handle, NVX_INDEX_PARAM_FILENAME,
                                &indexParamFilename);
    if (OMX_ErrorNone != err)
    {
        return err;
    }

    INIT_PARAM(filenameParam, player);
    filenameParam.pFilename = (char *)player->uri;
    err = OMX_SetParameter(component->handle, indexParamFilename, 
                           &filenameParam);

    OMX_GetExtensionIndex(component->handle,
                          NVX_INDEX_CONFIG_QUERYMETADATA,
                          &(player->priv->metaIndex));

    *comp = component;
    return err;
}

static void PlayerGraphProbeInput(NvxPlayerGraph2 *player, NvxComponent *comp)
{
    NVX_PARAM_DURATION duration;
    NVX_PARAM_STREAMTYPE streamType;
    OMX_PARAM_PORTDEFINITIONTYPE portDef;
    NVX_PARAM_AUDIOPARAMS audParams;
    NVX_PARAM_STREAMCOUNT NvxStreamCount;

    OMX_ERRORTYPE err;
    OMX_INDEXTYPE param, audioIndex;

    player->videoType = NvxStreamType_NONE;
    player->audioType = NvxStreamType_NONE;

    err = OMX_GetExtensionIndex(comp->handle, NVX_INDEX_PARAM_STREAMCOUNT, &param);
    if (OMX_ErrorNone != err)
        return;
    INIT_PARAM(NvxStreamCount, player);

    err = OMX_GetParameter(comp->handle, param, &NvxStreamCount);
    if (OMX_ErrorNone != err)
        return;
    player->StreamCount = NvxStreamCount.StreamCount;

    err = OMX_GetExtensionIndex(comp->handle, NVX_INDEX_PARAM_DURATION, &param);
    if (OMX_ErrorNone != err)
        return;

    INIT_PARAM(duration, player);
    INIT_PARAM(streamType, player);
    INIT_PARAM(portDef, player);
    INIT_PARAM(audParams, player);

    err = OMX_GetParameter(comp->handle, param, &duration);
    if (OMX_ErrorNone == err)
        player->durationInMS = duration.nDuration / 1000;

    err = OMX_GetExtensionIndex(comp->handle, NVX_INDEX_PARAM_STREAMTYPE, 
                                &param);
    if (OMX_ErrorNone != err)
        return;

    err = OMX_GetExtensionIndex(comp->handle, NVX_INDEX_PARAM_AUDIOPARAMS, 
                                &audioIndex);
    if (OMX_ErrorNone != err)
        return;

    streamType.nPort = 0;
    streamType.eStreamType = NvxStreamType_NONE;
    err = OMX_GetParameter(comp->handle, param, &streamType);

    if (OMX_ErrorNone == err && streamType.eStreamType != NvxStreamType_NONE)
    {
        if (!player->priv->vidDecoderName && !player->priv->noVideo)
        {
            switch (streamType.eStreamType)
            {
                case NvxStreamType_MPEG4:
                    player->priv->vidDecoderName = g_Mp4Decoder; break;
                case NvxStreamType_H263:
                    player->priv->vidDecoderName = g_H263Decoder; break;
                case NvxStreamType_WMV:
                    player->priv->vidDecoderName = g_Vc1Decoder; break;
                case NvxStreamType_WMV_Secure:
                    player->priv->vidDecoderName = g_Vc1SecureDecoder; break;
                case NvxStreamType_H264:
                    player->priv->vidDecoderName = g_H264Decoder; break;
                case NvxStreamType_H264_Secure:
                    player->priv->vidDecoderName = g_H264SecureDecoder; break;
                case NvxStreamType_MPEG2V:
                    player->priv->vidDecoderName = g_Mpeg2Decoder; break;
                case NvxStreamType_VP6:
                    player->priv->vidDecoderName = g_VP6Decoder; break;
                case NvxStreamType_MJPEG:
                    player->priv->vidDecoderName = g_MJpegDecoder; break;
                case NvxStreamType_Theora:
                    player->priv->vidDecoderName = g_TheoraDecoder; break;
                default:
                    break;
            }
        }

        portDef.nPortIndex = 0;
        err = OMX_GetParameter(comp->handle, OMX_IndexParamPortDefinition, 
                               &portDef);
        if (OMX_ErrorNone == err)
        {
            player->videoType = streamType.eStreamType;
            player->vidWidth = portDef.format.video.nFrameWidth;
            player->vidHeight = portDef.format.video.nFrameHeight;
            player->vidBitrate = portDef.format.video.nBitrate;
            player->vidFramerate = portDef.format.video.xFramerate;
        }
    }

    streamType.nPort = 1;
    streamType.eStreamType = NvxStreamType_NONE;
    err = OMX_GetParameter(comp->handle, param, &streamType);
    if (OMX_ErrorNone == err && streamType.eStreamType != NvxStreamType_NONE)
    {
        if (!player->priv->audDecoderName && !player->priv->noAudio)
        {
            switch (streamType.eStreamType)
            {
                case NvxStreamType_MP2:
                    player->priv->audDecoderName = g_Mp2Decoder; break;
                case NvxStreamType_MP3:
                    player->priv->audDecoderName = g_Mp3Decoder; break;
                case NvxStreamType_WAV:
                    player->priv->audDecoderName = g_WavDecoder; break;
                case NvxStreamType_AAC:
                    player->priv->audDecoderName = g_AacDecoder; break;
                case NvxStreamType_AACSBR:
                    player->priv->audDecoderName = g_eAacPDecoder; break;
                case NvxStreamType_BSAC:
                    player->priv->audDecoderName = g_BsacDecoder; break;
                case NvxStreamType_WMA:
                    player->priv->audDecoderName = g_WmaDecoder; break;
                case NvxStreamType_WMAPro:
                    player->priv->audDecoderName = g_WmaProDecoder; break;
                case NvxStreamType_WMALossless:
                    player->priv->audDecoderName = g_WmaLSLDecoder; break;
                case NvxStreamType_AMRWB:
                    player->priv->audDecoderName = g_AmrWBDecoder; break;
                case NvxStreamType_AMRNB:
                    player->priv->audDecoderName = g_AmrDecoder; break;
                case NvxStreamType_VORBIS:
                    player->priv->audDecoderName = g_VorbisDecoder; break;
                default: break;
            }
        }

        audParams.nPort = 1;
        err = OMX_GetParameter(comp->handle, audioIndex, &audParams);
        if (OMX_ErrorNone == err)
        {
            player->audioType = streamType.eStreamType;
            player->audSample = audParams.nSampleRate;
            player->audChannels = audParams.nChannels;
            player->audBitrate = audParams.nBitRate;
            player->audBitsPerSample = audParams.nBitsPerSample;
        }
    }
}

static void PlayerGraphSetAudioOnly(NvxPlayerGraph2 *player, 
                                    NvxComponent *audDec, 
                                    NvxComponent *audRend)
{
    NVX_CONFIG_AUDIOONLYHINT audioOnly;
    NVX_CONFIG_DISABLETSUPDATES disableTS;
    OMX_INDEXTYPE index;
    OMX_ERRORTYPE err;

    INIT_PARAM(audioOnly, player);
    err = OMX_GetExtensionIndex(audDec->handle, NVX_INDEX_CONFIG_AUDIOONLYHINT,
                                &index);
    if (OMX_ErrorNone == err)
    {
        audioOnly.bAudioOnlyHint = OMX_TRUE;
        OMX_SetConfig(audDec->handle, index, &audioOnly);
    }

    INIT_PARAM(disableTS, player);
    err = OMX_GetExtensionIndex(audRend->handle, 
                                NVX_INDEX_CONFIG_DISABLETIMESTAMPUPDATES, 
                                &index);
    if (OMX_ErrorNone == err)
    {
        disableTS.bDisableTSUpdates = OMX_TRUE;
        OMX_SetConfig(audRend->handle, index, &disableTS);
    }
}

static OMX_ERRORTYPE PlayerGraphCreatePlayback(NvxPlayerGraph2 *player)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    NvxComponent *reader = NULL;
    char *audio = NULL, *video = NULL;
    char *audrendname = g_AudioRender;

    if (player->type == NvxPlayerGraphType_JpegDecode)
    {
        video = g_JpegDecoder;
        err = PlayerGraphCreateReader(player, g_ImageReader, &reader);
    }
    else
    {
        err = PlayerGraphCreateReader(player, g_Reader, &reader);
    }

    if (OMX_ErrorNone != err || !reader)
        return err;

    if (player->priv->useExternalAudRend)
        audrendname = player->priv->externalAudRend;
        
    if (player->type != NvxPlayerGraphType_JpegDecode)
    {
        PlayerGraphProbeInput(player, reader);

        video = player->priv->vidDecoderName;
        audio = player->priv->audDecoderName;
    }

    if (!video)
        OMX_SendCommand(reader->handle, OMX_CommandPortDisable, 0, 0);
    if (!audio)
        OMX_SendCommand(reader->handle, OMX_CommandPortDisable, 1, 0);

    if ((!video && !audio) || (player->priv->noAudio && player->priv->noVideo) ||
        (!video && player->priv->noAudio) || (!audio && player->priv->noVideo))
    {
        return OMX_ErrorUndefined;
    }

    if (video)
    {
        NvxComponent *vidDec, *vidRend;
        char *renderName = (player->priv->isFrameExtract) ? g_FrameExtract : 
                                                           g_OvRenderYUV;
        err = NvxGraphCreateComponentByName(player->graph, video, "VIDDEC",
                                            &vidDec);
        if (OMX_ErrorNone != err)
            return err;
        OMX_CONFIG_ROTATIONTYPE oRotation;
        INIT_PARAM1(oRotation);
        oRotation.nRotation = 90;
        err = NvxGraphCreateComponentByName(player->graph, renderName, 
            "VIDREND", &vidRend);
        NvOsDebugPrintf("\n$$%s[%d] ", __FUNCTION__, __LINE__);
        err = OMX_SetConfig( vidRend->handle, OMX_IndexConfigCommonRotate, &oRotation);
        NvOsDebugPrintf("\n$$%s[%d] status:0x%x ", __FUNCTION__, __LINE__,err);
        OMX_CONFIG_MIRRORTYPE oMirror;
        INIT_PARAM1(oMirror);
        oMirror.nPortIndex = 0;
        oMirror.eMirror = 1;
        OMX_SetConfig(vidRend->handle, OMX_IndexConfigCommonMirror,
           &oMirror);
        if (OMX_ErrorNone != err)
            return err;

        if (!player->priv->isFrameExtract)
            NvxConnectComponentToClock(vidRend);

        err = NvxConnectTunneled(reader, 0, vidDec, 0);
        if (OMX_ErrorNone != err)
            return err;

        err = NvxConnectTunneled(vidDec, 1, vidRend, 0);
        if (OMX_ErrorNone != err)
            return err;

        NvxGraphSetComponentAsEndpoint(player->graph, vidRend);
    }

    if (audio)
    {
        NvxComponent *audDec, *audRend;

        err = NvxGraphCreateComponentByName(player->graph, audio, "AUDDEC",
                                            &audDec);
        if (OMX_ErrorNone != err)
            return err;

        err = NvxGraphCreateComponentByName(player->graph, audrendname,
                                            "AUDREND", &audRend);
        if (OMX_ErrorNone != err)
            return err;

        if (!video)
        {
            PlayerGraphSetAudioOnly(player, audDec, audRend);
        }

        NvxConnectComponentToClock(audRend);

        err = NvxConnectTunneled(reader, 1, audDec, 0);
        if (OMX_ErrorNone != err)
            return err;

        err = NvxConnectTunneled(audDec, 1, audRend, 0);
        if (OMX_ErrorNone != err)
            return err;

        NvxGraphSetComponentAsEndpoint(player->graph, audRend);
    } 

    return err;
}

static OMX_ERRORTYPE PlayerGraphCreateMetadata(NvxPlayerGraph2 *player)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    NvxComponent *comp;

    player->priv->fileCacheSize = 0;

    err = PlayerGraphCreateReader(player, g_Reader, &comp);
    if (OMX_ErrorNone != err || !comp)
        return err;

    PlayerGraphProbeInput(player, comp);

    return err;
}

static OMX_ERRORTYPE PlayerGraphCreateFrameExtractor(NvxPlayerGraph2 *player)
{
    player->priv->isFrameExtract = OMX_TRUE;
    player->priv->fileCacheSize = 0;
    player->priv->noAudio = OMX_TRUE;
    player->priv->noVideo = OMX_FALSE;
    return PlayerGraphCreatePlayback(player);
}

/**************** start public interface *************************/

OMX_ERRORTYPE NvxPlayerGraphInit(NvxFramework framework,
                                 NvxPlayerGraph2 **player,
                                 NvxPlayerGraphType type,
                                 const OMX_U8 *uri,
                                 const OMX_U8 *userAgent)
{
    NvxPlayerGraph2 *gr = NULL;
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_U32 len = 0;

    if (!player || type > NvxPlayerGraphType_Force32 || !type || !uri)
    {
        err = OMX_ErrorBadParameter;
        goto fail;
    }

    *player = NULL;
 
    gr = NvOsAlloc(sizeof(NvxPlayerGraph2));
    if (!gr)
    {
        err = OMX_ErrorInsufficientResources;
        goto fail;
    }
    
    NvOsMemset(gr, 0, sizeof(NvxPlayerGraph2));

    len = NvOsStrlen((char *)uri);
    gr->uri = NvOsAlloc(len + 1);
    if (!gr->uri)
    {
        err = OMX_ErrorInsufficientResources;
        goto fail;
    }
    NvOsStrncpy((char *)(gr->uri), (char *)uri,  len + 1);

    if (userAgent)
    {
        len = NvOsStrlen((char *)userAgent);
        gr->userAgent = NvOsAlloc(len + 1);
        if (!gr->userAgent)
        {
            err = OMX_ErrorInsufficientResources;
            goto fail;
        }
        NvOsStrncpy((char *)(gr->userAgent), (char *)userAgent,  len + 1);
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_INFO,
            "NvxPlayerGraph userAgent passed is %s \n", gr->userAgent));
    }

    gr->priv = NvOsAlloc(sizeof(NvxPlayerGraphPriv));
    if (!gr->priv)
    {
        err = OMX_ErrorInsufficientResources;
        goto fail;
    }

    NvOsMemset(gr->priv, 0, sizeof(NvxPlayerGraphPriv));

    gr->framework = framework;
    gr->type = type;

    gr->priv->noAudio = OMX_FALSE;
    gr->priv->noVideo = OMX_FALSE;
    gr->priv->fileCacheSize = 1;

    if (NvSuccess != NvOsSemaphoreCreate(&gr->priv->firstFrameEvent, 0))
    {
        err = OMX_ErrorInsufficientResources;
        goto fail;
    }

    if (NvSuccess != NvOsMutexCreate(&gr->priv->hStateMutex))
    {
        err = OMX_ErrorInsufficientResources;
        goto fail;
    }

    err = NvxGraphInit(framework, &gr->graph,
                       ((type != NvxPlayerGraphType_Metadata) &&
                        (type != NvxPlayerGraphType_FrameExtraction) &&
                        (type != NvxPlayerGraphType_JpegDecode)));
    if (OMX_ErrorNone != err)
    {
        goto fail;
    }

    NvxGraphSetGraphEventHandler(gr->graph, gr, PlayerEventHandler);

    *player = gr;

    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_INFO, "NvxPlayerGraph Initialized"));
    return OMX_ErrorNone;

fail:
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "NvxPlayerGraph Init Failed! NvError = 0x%08x", err));
    if (gr)
    {
        NvxPlayerGraphDeinit(gr);
    }
    return err;
}

OMX_ERRORTYPE NvxPlayerGraphCreate(NvxPlayerGraph2 *player)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    
    if (!player)
    {
        err = OMX_ErrorBadParameter;
        goto fail;
    }

    switch (player->type)
    {
        case NvxPlayerGraphType_Normal:
        case NvxPlayerGraphType_JpegDecode:
            err = PlayerGraphCreatePlayback(player);
            break;
        case NvxPlayerGraphType_Metadata:
            err =  PlayerGraphCreateMetadata(player);
            break;
        case NvxPlayerGraphType_FrameExtraction:
            err =  PlayerGraphCreateFrameExtractor(player);
            break;
        default:
            err = OMX_ErrorBadParameter;
            goto fail;
    }

    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_INFO, "NvxPlayerGraph Created. Type = %s", 
        (player->type==NvxPlayerGraphType_Normal)?"NORMAL":(player->type==NvxPlayerGraphType_Metadata)?"METADATA":
        (player->type==NvxPlayerGraphType_FrameExtraction)?"FRAMEEXTRACTION":"UNKNOWN"));
    return err;
    
fail:
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "NvxPlayerGraph Create Failed! NvError = 0x%08x", err));   
    return err;
}

OMX_ERRORTYPE NvxPlayerGraphDeinit(NvxPlayerGraph2 *player)
{
    if (player->graph)
    {
        NvxGraphTeardown(player->graph);
        NvxGraphDeinit(player->graph);
    }

    if (player->priv && player->priv->firstFrameEvent)
        NvOsSemaphoreDestroy(player->priv->firstFrameEvent);
    if (player->priv && player->priv->hStateMutex)
        NvOsMutexDestroy(player->priv->hStateMutex);

    NvOsFree(player->priv);
    NvOsFree(player->uri);
    NvOsFree(player->userAgent);
    NvOsFree(player);

    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_INFO, "NvxPlayerGraph DeInitialized"));
    return OMX_ErrorNone;
}

OMX_BOOL NvxPlayerGraphHasBufferingEvent(NvxPlayerGraph2 *player,
                                         OMX_BOOL *needPause,
                                         OMX_U32 *bufferPercent)
{
    OMX_BOOL retval = OMX_FALSE;

    if (!player || !needPause || !bufferPercent)
        return OMX_ErrorBadParameter;

    // FIXME: lock this
    if (player->priv->hasBufferEvent)
    {
        retval = OMX_TRUE;
        player->priv->hasBufferEvent = OMX_FALSE;
        *needPause = player->priv->bufferingPauseGraph;
        *bufferPercent = player->priv->bufferingPercent;
    }
    else
    {
        *needPause = OMX_FALSE;
        *bufferPercent = 0;
    }

    return retval; 
}

void NvxPlayerGraphSetRendererOverlay(NvxPlayerGraph2 *player,
                                      NvUPtr oOverlay)
{
    OMX_INDEXTYPE index;
    NVX_CONFIG_EXTERNALOVERLAY externalOverlay;
    OMX_ERRORTYPE err;
    NvxComponent *comp;

    if (!player)
        return;

    comp = NvxGraphLookupComponent(player->graph, "VIDREND");
    if (!comp)
        return;

    err = OMX_GetExtensionIndex(comp->handle, NVX_INDEX_CONFIG_EXTERNALOVERLAY,
                                &index);
    if (OMX_ErrorNone != err)
        return;

    INIT_PARAM(externalOverlay, player);
    externalOverlay.oOverlayPtr = (NvU64)oOverlay;

    OMX_SetConfig(comp->handle, index, &externalOverlay);
}

void NvxPlayerGraphUseExternalAudioRenderer(NvxPlayerGraph2 *player,
                                            char *audRenderName)
{
    if (!player)
        return;

    player->priv->useExternalAudRend = OMX_TRUE;
    player->priv->externalAudRend = audRenderName;
}

OMX_ERRORTYPE NvxPlayerGraphStop(NvxPlayerGraph2 *player)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

    if (!player)
    {
        return OMX_ErrorBadParameter;
    }
    NvOsMutexLock(player->priv->hStateMutex);
    NvxGraphSetEndOfStream(player->graph, OMX_TRUE);
    NvxGraphClearError(player->graph);

    if (player->graph->state == OMX_StateExecuting ||
        player->graph->state == OMX_StatePause)
    {
        NvxGraphStopClock(player->graph);
        err = NvxGraphTransitionAllToState(player->graph, OMX_StateIdle, 5000);
        NvxGraphClearError(player->graph);
    }
    if (err != OMX_ErrorNone)
    {
        goto fail;
    }
    NvOsMutexUnlock(player->priv->hStateMutex);
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_INFO, "NvxPlayerGraph Stopped"));

    return OMX_ErrorNone;

fail:
    NvOsMutexUnlock(player->priv->hStateMutex);
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "NvxPlayerGraph Stop Failed! NvError = 0x%08x", err));       
    return err;
}

OMX_ERRORTYPE NvxPlayerGraphPause(NvxPlayerGraph2 *player, OMX_BOOL pause)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

    if (!player)
    {
        return OMX_ErrorBadParameter;
    }
    NvOsMutexLock(player->priv->hStateMutex);
    if (pause)
    {
        if (player->graph->state == OMX_StateExecuting ||
            player->graph->state == OMX_StateIdle)
        {
            NvxGraphPauseClock(player->graph, pause);
            err = NvxGraphTransitionAllToState(player->graph, OMX_StatePause, 
                                               5000);
            if (err == OMX_ErrorNone)
            {
                NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_INFO, "NvxPlayerGraph Paused"));
            }      
        }
    }
    else
    {
        if (player->graph->state == OMX_StatePause)
        {
            err = NvxGraphTransitionAllToState(player->graph, OMX_StateExecuting, 
                                           5000);
            NvxGraphPauseClock(player->graph, pause);

            if (err == OMX_ErrorNone)
            {
                NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_INFO, "NvxPlayerGraph Resumed"));
            }
        }
    }
    NvOsMutexUnlock(player->priv->hStateMutex);
    if (err != OMX_ErrorNone)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "NvxPlayerGraph %s Failed! NvError = 0x%08x", pause?"Pause":"Resume", err));   
    }
    return err;
}

OMX_ERRORTYPE NvxPlayerGraphToIdle(NvxPlayerGraph2 *player)
{
    OMX_ERRORTYPE err;

    if (!player)
        return OMX_ErrorBadParameter;

    NvOsMutexLock(player->priv->hStateMutex);
    err = NvxGraphTransitionAllToState(player->graph, OMX_StateIdle, 5000);
    if (OMX_ErrorNone != err)
    {
        NvxGraphTransitionAllToState(player->graph, OMX_StateLoaded, 5000);
        NvOsMutexUnlock(player->priv->hStateMutex);
        return err;
    }
    {
        OMX_TIME_CONFIG_TIMESTAMPTYPE timestamp;
        NvxComponent *reader = NULL;
        reader = NvxGraphLookupComponent(player->graph, "READER");
        if (!reader)
        {
            err = OMX_ErrorNone;
        }
        else
        {
            INIT_PARAM(timestamp, player);
            timestamp.nPortIndex = 0;
            timestamp.nTimestamp = 0;
            err = OMX_SetConfig(reader->handle, OMX_IndexConfigTimePosition,
                        &timestamp);
            if (err != OMX_ErrorNone)
            {
                // Ignore error for now
                err = OMX_ErrorNone;
            }
        }
    }

    player->priv->startTime = 0;
    NvOsMutexUnlock(player->priv->hStateMutex);
    return err;
}

OMX_ERRORTYPE NvxPlayerGraphTeardown(NvxPlayerGraph2 *player)
{
    if (!player)
        return OMX_ErrorBadParameter;

    return NvxGraphTeardown(player->graph);
}

OMX_ERRORTYPE NvxPlayerGraphStartPlayback(NvxPlayerGraph2 *player)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

    if (!player)
    {
        err = OMX_ErrorBadParameter;
        goto fail;
    }
    NvOsMutexLock(player->priv->hStateMutex);
    NvxGraphStartClock(player->graph, player->priv->startTime);
    err = NvxGraphTransitionAllToState(player->graph, OMX_StateExecuting,
                                       5000);
    if (OMX_ErrorNone != err)
    {
        goto fail;
    }
    NvOsMutexUnlock(player->priv->hStateMutex);
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_INFO, "NvxPlayerGraph Started"));
    return OMX_ErrorNone;
    
fail:
    NvOsMutexUnlock(player->priv->hStateMutex);
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "NvxPlayerGraph Start Failed! NvError = 0x%08x", err));     
    return err;
}

OMX_ERRORTYPE NvxPlayerGraphSeek(NvxPlayerGraph2 *player, int *timeInMS)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_BOOL atStart = OMX_FALSE;
    OMX_TIME_CONFIG_TIMESTAMPTYPE timestamp;
    NvxComponent *reader;
    OMX_STATETYPE curState;
    NvOsDebugPrintf("++%s[%d]", __FUNCTION__, __LINE__);

    if (!player || !timeInMS)
    {
        return OMX_ErrorBadParameter;
    }

    NvOsMutexLock(player->priv->hStateMutex);
    reader = NvxGraphLookupComponent(player->graph, "READER");
    if (!reader)
    {
        err = OMX_ErrorBadParameter;
        goto fail;
    }     

    if (player->graph->state != OMX_StateExecuting &&
        player->graph->state != OMX_StatePause)
    {
        player->priv->startTime = *timeInMS;
        atStart = OMX_TRUE;
    }

    curState = player->graph->state;

    NvxGraphSetEndOfStream(player->graph, OMX_FALSE);

    INIT_PARAM(timestamp, player);
    timestamp.nPortIndex = 0;
    timestamp.nTimestamp = (OMX_TICKS)( (OMX_TICKS)(*timeInMS) * 1000);
    NvOsDebugPrintf ("\n\nSeekTime = %d\n", *timeInMS);

    if (!atStart)
    {
        NvxGraphTransitionAllToState(player->graph, OMX_StatePause, 5000);
        NvxGraphStopClock(player->graph);
    }

    err = OMX_SetConfig(reader->handle, OMX_IndexConfigTimePosition, 
                        &timestamp);
    if (err != OMX_ErrorNone)
    {
        // FIXME continue?
        NvxGraphSetEndOfStream(player->graph, OMX_TRUE);
        goto fail;
    }

    *timeInMS = (int)(timestamp.nTimestamp / 1000);

    if (atStart)
    {
        NvxGraphStartClock(player->graph, *timeInMS);
        NvxGraphStopClock(player->graph);
        goto fail;
    }

    NvxGraphFlushAllComponents(player->graph);
    NvxGraphStartClock(player->graph, *timeInMS);

    if (OMX_StateExecuting == curState)
    {
        NvxGraphTransitionAllToState(player->graph, OMX_StateExecuting,
                                     5000);
    }
    NvOsMutexUnlock(player->priv->hStateMutex);
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_INFO, "NvxPlayerGraph Seeked to Time = %d ms", *timeInMS));
    NvOsDebugPrintf("--%s[%d]", __FUNCTION__, __LINE__);

    return OMX_ErrorNone;
    
fail:
    NvOsMutexUnlock(player->priv->hStateMutex);
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "NvxPlayerGraph Seek Failed! NvError = 0x%08x, SeekTime = %d ms", err, *timeInMS));
    NvOsDebugPrintf("\n\n\nNvxPlayerGraph Seek Failed! NvError = 0x%08x, SeekTime = %d ms", err, *timeInMS);

    return err;
}

static OMX_BOOL IsValidUTFString(OMX_U8* data)
{
    if (data == NULL)
    {
        return OMX_FALSE;
    }

    while (*data != '\0')
    {
        char utf8 = *(data++);
        // consider high four bits.
        switch (utf8 >> 4)
        {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            break;
        case 0x08:
        case 0x09:
        case 0x0a:
        case 0x0b:
        case 0x0f:
            return OMX_FALSE;
        case 0x0e:
            {
                // 1110 bit pattern means we have 2 more bytes.
                utf8 = *(data++);
                if ((utf8 & 0xc0) != 0x80)
                {
                    return OMX_FALSE;
                }
                // Go Ahead for last byte.
            }
        case 0x0c:
        case 0x0d:
            {
                // 110x bit pattern meanswe have 1 more additional byte.
                utf8 = *(data++);
                if ((utf8 & 0xc0) != 0x80)
                {
                  return OMX_FALSE;
                }
                break;
            }
        default:
            return OMX_FALSE;
        }
    }

    return OMX_TRUE;
}

static OMX_BOOL ConvertAscciToUtf8(OMX_U8* srcData, OMX_U8* utf8Data, OMX_U32 *utf8Len)
{
    OMX_U8 data = 0;
    OMX_U32 outLen = 0, inLen = 0;
    OMX_U8 *src =srcData;

    while ((data = *(src++)) != '\0')
    {
        inLen++;
        if (data < 0x80)
        {
            *(utf8Data++) = data; outLen++;
        }
        else if (data <= 0xFF)
        {
            *(utf8Data++) = 0xC0 | (data >> 6); outLen++;
            *(utf8Data++) = 0x80 | (data & 0x3F); outLen++;
        }
        else
        {
            NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "Conversion from ascii to utf-8 is not supported for ascii value \
                                                                greater than 0xFF"));
            return OMX_FALSE;
        }
    }
    *(utf8Data++) = '\0';
    outLen++;
    *utf8Len = outLen;

    return OMX_TRUE;
}

OMX_ERRORTYPE NvxPlayerGraphExtractMetadata(NvxPlayerGraph2 *player,
                                            ENvxMetadataType type,
                                            OMX_U8 **retData, int *retLen)
{
    OMX_ERRORTYPE err;
    NVX_CONFIG_QUERYMETADATA md;
    OMX_U8 *buffer = NULL;
    OMX_U32 len = 0;
    OMX_BOOL HandleUnknownString = OMX_FALSE;
    NvxComponent *comp;

    if (!player || !retData || !retLen || (int)(player->priv->metaIndex) == 0)
    {
        err = OMX_ErrorBadParameter;
        goto fail;
    }    

    INIT_PARAM(md, player);
    md.sValueStr = NULL;
    md.nValueLen = 0;
    md.eType = type;

    *retData = NULL;
    *retLen = 0;
    comp = NvxGraphLookupComponent(player->graph, "READER");
    if (!comp)
    {
        err = OMX_ErrorUndefined;
        goto fail;
    }            
    err = OMX_GetConfig(comp->handle, player->priv->metaIndex, &md);
    if (OMX_ErrorInsufficientResources == err)
    {
        len = md.nValueLen;
        if (0 == len || (NvxMetadata_CoverArt != type && len > 16384))
        {
            err = OMX_ErrorInsufficientResources;
            goto fail;
        }            

        len += 4;
        buffer = (OMX_U8 *)NvOsAlloc(len);
        if (!buffer)
        {
            err = OMX_ErrorInsufficientResources;
            goto fail;
        }            

        NvOsMemset(buffer, 0, len);

        md.sValueStr = (OMX_STRING)buffer;
        md.nValueLen = len;
        err = OMX_GetConfig(comp->handle, player->priv->metaIndex, &md);
    }

    if (OMX_ErrorNone != err || 0 == md.nValueLen)
    {
        if (buffer)
            NvOsFree(buffer);
        buffer = NULL;
    }
    else
    {
        if (OMX_MetadataCharsetASCII == md.eCharSet ||
            OMX_MetadataCharsetUTF8 == md.eCharSet ||
            NvxMetadata_CoverArt == type)
        {
            // check for only UTF and Ascii, for coverart copy data as it is
            if (OMX_MetadataCharsetUTF8 == md.eCharSet ||
                OMX_MetadataCharsetASCII == md.eCharSet)
            {
                // check for valid utf
                if (!IsValidUTFString(buffer))
                {
                    OMX_U8 *utf8Buf = NULL;
                    OMX_U32 utf8len = (2 * md.nValueLen) +1;

                    utf8Buf = (OMX_U8 *)NvOsAlloc(utf8len);
                    if (!utf8Buf)
                    {
                        err = OMX_ErrorInsufficientResources;
                        goto fail;
                    }

                    NvOsMemset(utf8Buf, 0, utf8len);
                    // Convert to utf
                    if (ConvertAscciToUtf8(buffer, utf8Buf, &utf8len))
                    {
                        // cross check after conversion, this just to make sure that there crash due to invalid conversion
                        if (!IsValidUTFString(utf8Buf))
                        {
                            HandleUnknownString = OMX_TRUE;
                        }
                        else
                        {
                            // valid conversion to utf
                            *retData = utf8Buf;
                            *retLen = utf8len;
                        }
                    }
                    else
                    {
                        HandleUnknownString = OMX_TRUE;
                    }
                    if (HandleUnknownString == OMX_TRUE)
                    {
                        OMX_U8 *tmpBuffer = NULL;
                        tmpBuffer = NvOsAlloc(8);
                        if (!tmpBuffer)
                            return OMX_ErrorNone;
                        NvOsSnprintf((char *)tmpBuffer, 8, "Unknown");
                        *retData = tmpBuffer;
                        *retLen = 8;
                        NvOsFree(utf8Buf);
                        utf8Buf = NULL;
                    }
                    NvOsFree(buffer);
                    return OMX_ErrorNone;
                }
            }
            *retData = buffer;
            *retLen = (int)md.nValueLen;
        }
        else if (NVOMX_MetadataCharsetU32 == md.eCharSet)
        {
            NvU32 nNum;
            NvOsMemcpy(&nNum, buffer, sizeof(NvU32));
            *retData = NvOsAlloc(64);
            NvOsSnprintf((char *)*retData, 64, "%d", nNum);
            *retLen = (int)NvOsStrlen((char *)*retData) + 1;
            NvOsFree(buffer);
        }
        else if (NVOMX_MetadataCharsetU64 == md.eCharSet)
        {
            NvU64 nNum;
            NvOsMemcpy(&nNum, buffer, sizeof(NvU64));
            *retData = NvOsAlloc(sizeof(NvU64));
            NvOsMemcpy(((char *)*retData), &nNum, sizeof(NvU64));
            *retLen = sizeof(NvU64);
            NvOsFree(buffer);
        }
        else if (OMX_MetadataCharsetUTF16LE == md.eCharSet)
        {
            OMX_U32 sizeConv = 0;
            OMX_U8 *tmpBuffer = NULL;
            sizeConv = NvUStrlConvertCodePage(NULL, 0, NvOsCodePage_Utf8,
                                              md.sValueStr, md.nValueLen,
                                              NvOsCodePage_Utf16);

            if (sizeConv <= 0)
            {
                NvOsFree(buffer);
                return OMX_ErrorNone;
            }

            tmpBuffer = NvOsAlloc(sizeConv);
            if (!tmpBuffer)
            {
                NvOsFree(buffer);
                return OMX_ErrorNone;
            }

            NvUStrlConvertCodePage(tmpBuffer, sizeConv, NvOsCodePage_Utf8,
                                   md.sValueStr, md.nValueLen,
                                   NvOsCodePage_Utf16);
            *retData = tmpBuffer;
            *retLen = (int)sizeConv;
            NvOsFree(buffer);
        }
        else
        {
            //NvOsDebugPrintf("Unsupported encoding for %d", eType);
            NvOsFree(buffer);
        }
    }
    return OMX_ErrorNone;

fail:    
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "NvxPlayerGraph ExtractMetadata Failed! NvError = 0x%08x", err));
    return err;
}

OMX_ERRORTYPE NvxPlayerGraphMetaSwitchToTrack(NvxPlayerGraph2 *player,
                                              const OMX_U8 *uri)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_INDEXTYPE indexParamFilename;
    NVX_PARAM_FILENAME filenameParam;
    NvxComponent *comp;

    if (!player || NvxPlayerGraphType_Metadata != player->type)
    {
        err = OMX_ErrorBadParameter;
        goto fail;
    }     

    comp = NvxGraphLookupComponent(player->graph, "READER");
    if (!comp)
    {
        err = OMX_ErrorUndefined;
        goto fail;
    }     
   

    err = OMX_GetExtensionIndex(comp->handle, NVX_INDEX_PARAM_FILENAME,
                                &indexParamFilename);
    if (OMX_ErrorNone != err)
    {
        goto fail;
    }   

    INIT_PARAM(filenameParam, player);
    filenameParam.pFilename = (char*)uri;
    err = OMX_SetParameter(comp->handle, indexParamFilename, &filenameParam);

    PlayerGraphProbeInput(player, comp);

    {
        OMX_U8 *newuri;
        OMX_U32 len;

        len = NvOsStrlen((char *)uri);
        newuri = NvOsAlloc(len + 1);
        if (!newuri)
        {
            err = OMX_ErrorInsufficientResources;
            goto fail;
        }   

        NvOsStrncpy((char *)newuri, (char *)uri,  len + 1);
        NvOsFree(player->uri);
        player->uri = newuri;
    }

    return OMX_ErrorNone;

fail:        
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "NvxPlayerGraph MetaSwitchToTrack Failed! NvError = 0x%08x", err));    
    return err;   
}
static OMX_ERRORTYPE sNvxPlayerGraphGetThumbnailSeekTime(NvxPlayerGraph2 *player,
                                                OMX_U64 *time)
{
    ENvxMetadataType eType = NvxMetadata_MAX;
    int len = 0;
    char *pValue = NULL;
    OMX_ERRORTYPE err = OMX_ErrorNone;
    NvU64 ThmbSeekTime = 0;
    if (!player || !time || (int)(player->priv->metaIndex) == 0)
    {
        return OMX_ErrorBadParameter;
    }
    eType = NvxMetadata_ThumbnailSeektime;

    err = NvxPlayerGraphExtractMetadata(player, eType, (OMX_U8**)&pValue, &len);

    if((err != OMX_ErrorNone) || !pValue)
    {
        return OMX_ErrorBadParameter;
    }

    ThmbSeekTime = *((NvU64*)(pValue));
    NvOsFree(pValue);
    *time = ThmbSeekTime;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxPlayerGraphExtractFrame(NvxPlayerGraph2 *player, int timeInMS,
                                         int *dataLen, int *width, int *height)
{
    OMX_INDEXTYPE index, lowresindex;
    NVX_CONFIG_CAPTURERAWFRAME raw;
    OMX_PARAM_PORTDEFINITIONTYPE portDef;
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_U64 ThumbnailSeekTime = 0;
    NvxComponent *comp, *viddeccomp;
    NVX_PARAM_LOWRESOURCEMODE lowresourceParam;

    if (!player)
    {
        err = OMX_ErrorBadParameter;
        goto fail;
    }     
    
    comp = NvxGraphLookupComponent(player->graph, "VIDREND");
    
    if (!comp || !dataLen || !width || !height)
    {
        err = OMX_ErrorBadParameter;
        goto fail;
    }

    err = sNvxPlayerGraphGetThumbnailSeekTime(player, &ThumbnailSeekTime);
    if(err == OMX_ErrorNone)
    {
        // convert to ms from 100ns units.
        timeInMS = (int)(ThumbnailSeekTime / 10000);
    }

    if (timeInMS < 0)
        timeInMS = 7000;

    *dataLen = 0;
    *width = 0;
    *height = 0;
    err = OMX_GetExtensionIndex(comp->handle,
                                NVX_INDEX_CONFIG_CAPTURERAWFRAME,
                                &index);
    if (OMX_ErrorNone != err)
    {
        goto fail;
    }
    viddeccomp = NvxGraphLookupComponent(player->graph, "VIDDEC");
    if(!viddeccomp)
    {
        err = OMX_ErrorBadParameter;
        goto fail;
    }
    err = OMX_GetExtensionIndex(viddeccomp->handle,
                                NVX_INDEX_PARAM_LOWRESOURCEMODE, &lowresindex);
    if (OMX_ErrorNone != err)
    {
        return err;
    }

    INIT_PARAM(lowresourceParam, player);
    lowresourceParam.bLowMemMode = OMX_TRUE;
    err = OMX_SetParameter(viddeccomp->handle, lowresindex,
                           &lowresourceParam);

    NvxPlayerGraphToIdle(player);

    if (player->durationInMS > timeInMS)
    {
        NvxPlayerGraphSeek(player, &timeInMS);

        if (timeInMS >= player->durationInMS)
        {
            timeInMS = 0;
            NvxPlayerGraphSeek(player, &timeInMS);
        }

        if (OMX_ErrorNone != NvxGraphGetError(player->graph))
        {
            NvxGraphClearError(player->graph);
            NvxGraphSetEndOfStream(player->graph, OMX_FALSE);

            NvxPlayerGraphMetaSwitchToTrack(player, player->uri);
        }
    }

    err = NvxPlayerGraphStartPlayback(player);
    if (OMX_ErrorNone != err)
    {
        goto fail;
    }     

    if (OMX_ErrorNone != NvxGraphGetError(player->graph) ||
        (NvSuccess != NvOsSemaphoreWaitTimeout(player->priv->firstFrameEvent,
                                               2000)))
    {
        NvxPlayerGraphStop(player);
        goto fail;
    }

    INIT_PARAM(raw, player);
    raw.pBuffer = NULL;
    raw.nBufferSize = 0;

    err = OMX_GetConfig(comp->handle, index, &raw);
    if (OMX_ErrorBadParameter != err || raw.nBufferSize <= 0)
    {
        goto fail;
    }    

    *dataLen = (int)raw.nBufferSize;

    INIT_PARAM(portDef, player);
    portDef.nPortIndex = 0;
    OMX_GetParameter(comp->handle, OMX_IndexParamPortDefinition,
                     &portDef);

    *width = (int)portDef.format.video.nFrameWidth;
    *height = (int)portDef.format.video.nFrameHeight;

    return OMX_ErrorNone;

fail:    
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "NvxPlayerGraph ExtractFrame Failed! NvError = 0x%08x", err));        
    return err;    
}

OMX_ERRORTYPE NvxPlayerGraphGetFrame(NvxPlayerGraph2 *player,
                                     char **data, int dataLen)
{
    OMX_INDEXTYPE index;
    OMX_ERRORTYPE err;
    NVX_CONFIG_CAPTURERAWFRAME oRaw;
    NvxComponent *comp;

    if (!player)
    {
        err = OMX_ErrorBadParameter;
        goto fail;
    }        

    comp = NvxGraphLookupComponent(player->graph, "VIDREND");

    if (!comp || !data || !*data || !dataLen)
    {
        err = OMX_ErrorBadParameter;
        goto fail;
    }    
    
    err = OMX_GetExtensionIndex(comp->handle, 
                                   NVX_INDEX_CONFIG_CAPTURERAWFRAME, &index);
    if (OMX_ErrorNone != err)
    {
        goto fail;
    }        

    INIT_PARAM(oRaw, player);
    oRaw.pBuffer = (void *)*data;
    oRaw.nBufferSize = dataLen;
    
    err = OMX_GetConfig(comp->handle, index, &oRaw);
    if (OMX_ErrorNone != err)
    {
        goto fail;
    }   
        
    return OMX_ErrorNone;
    
fail:    
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "NvxPlayerGraph GetFrame Failed! NvError = 0x%08x", err));        
    return err;
}

OMX_ERRORTYPE NvxPlayerGraphSetRate(
    NvxPlayerGraph2 *player,
    float playSpeed)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

   if (player == NULL)
       return OMX_ErrorBadParameter;

    err = NvxPlayerGraphPause(player, OMX_TRUE);
    if (OMX_ErrorNone != err)
    {
        NvOsDebugPrintf("Error Occured at NvxPlayerGraphPause:0x%x",err);
        goto exit;
    }

    NvxGraphStopClock(player->graph);

    err = NvxGraphSetRate(player->graph, playSpeed);
    if (OMX_ErrorNone != err)
    {
        NvOsDebugPrintf("Error Occured at SetRate:0x%x",err);
        goto exit;
    }
    err = NvxPlayerGraphPause(player, OMX_TRUE);
    if (OMX_ErrorNone != err)
        NvOsDebugPrintf("Error Occured at NvxPlayerGraphPause:0x%x",err);

exit:
    return err;
}

void NvxPlayerGraphSetCallBack(NvxPlayerGraph2 *player,
                void *Callback, void *pAppData)
{
   if (player == NULL)
       return;

    player->appCallBack.pCallBack = Callback;
    player->appCallBack.pAppData = pAppData;
}

OMX_ERRORTYPE NvxPlayerGraphGetDuration (
    NvxPlayerGraph2 *player, NvS64 *time)
{
    if (player == NULL)
        return OMX_ErrorBadParameter;
    *time= player->durationInMS;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxPlayerGetStreamInfo (
    NvxPlayerGraph2 *player,
    NvxComponent *comp,
    void *pStreamInfo)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    if (player == NULL || pStreamInfo == NULL)
        return OMX_ErrorBadParameter;
    NVX_PARAM_STREAMTYPE *pStreamType = (NVX_PARAM_STREAMTYPE *)pStreamInfo;
    OMX_INDEXTYPE Param;
    err = OMX_GetExtensionIndex(comp->handle, NVX_INDEX_PARAM_STREAMTYPE, &Param);
    if (OMX_ErrorNone != err)
        return err;
    err = OMX_GetParameter(comp->handle, Param, pStreamType);
    return err;
}

OMX_ERRORTYPE
NvxPlayerGraphSetVolume (
    NvxPlayerGraph2 *player,
    OMX_S32 volume)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    if (player == NULL)
    {
        return OMX_ErrorBadParameter;
    }
    NvxComponent *AudioRenderer;
    AudioRenderer = NvxGraphLookupComponent(player->graph, "AUDREND");
    if (!AudioRenderer)
    {
        NvOsDebugPrintf("AudioRenderer NULL %s[%d]", __FUNCTION__, __LINE__);
        return OMX_ErrorBadParameter;
    }
    OMX_AUDIO_CONFIG_VOLUMETYPE vt;
    INIT_PARAM(vt, player);
    vt.nPortIndex = 0;
    vt.bLinear = OMX_FALSE;
    vt.sVolume.nValue = volume;
    err = OMX_SetConfig(
        AudioRenderer->handle,
        OMX_IndexConfigAudioVolume,
        &vt);
    return err;
}


OMX_ERRORTYPE
NvxPlayerGraphGetVolume (
    NvxPlayerGraph2 *player,
    void *volume)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    if (player == NULL || volume == NULL)
    {
        return OMX_ErrorBadParameter;
    }
    NvxComponent *AudioRenderer;
    OMX_AUDIO_CONFIG_VOLUMETYPE *pParamVolumeType =
        (OMX_AUDIO_CONFIG_VOLUMETYPE *)volume;
    AudioRenderer = NvxGraphLookupComponent(player->graph, "AUDREND");
    if (!AudioRenderer)
    {
        NvOsDebugPrintf("AudioRenderer NULL %s[%d]", __FUNCTION__, __LINE__);
        return OMX_ErrorBadParameter;
    }
    OMX_AUDIO_CONFIG_VOLUMETYPE vt;
    INIT_PARAM(vt, player);
    vt.nPortIndex = 0;
    err = OMX_GetConfig(
        AudioRenderer->handle,
        OMX_IndexConfigAudioVolume,
        &vt);
    pParamVolumeType->sVolume.nValue = vt.sVolume.nValue;
    pParamVolumeType->sVolume.nMax = vt.sVolume.nMax;
    pParamVolumeType->sVolume.nMin = vt.sVolume.nMin;
    return err;
}

OMX_ERRORTYPE
NvxPlayerGraphSetMute (
    NvxPlayerGraph2 *player,
    OMX_BOOL mute)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    if (player == NULL)
    {
        return OMX_ErrorBadParameter;
    }
    NvxComponent *AudioRenderer;
    AudioRenderer = NvxGraphLookupComponent(player->graph, "AUDREND");
    if (!AudioRenderer)
    {
        NvOsDebugPrintf("AudioRenderer NULL %s[%d]", __FUNCTION__, __LINE__);
        return OMX_ErrorBadParameter;
    }
    OMX_AUDIO_CONFIG_MUTETYPE mt;
    INIT_PARAM(mt, player);
    mt.nPortIndex = 0;
    mt.bMute = mute;
    err = OMX_SetConfig (
        AudioRenderer->handle,
        OMX_IndexConfigAudioMute,
        &mt);
    return err;
}

OMX_ERRORTYPE
NvxPlayerGraphGetMute (
    NvxPlayerGraph2 *player,
    void *mute)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    NvxComponent *AudioRenderer;
    if (player == NULL || mute == NULL)
    {
        return OMX_ErrorBadParameter;
    }
    OMX_AUDIO_CONFIG_MUTETYPE *pParamVolumeMute = (OMX_AUDIO_CONFIG_MUTETYPE *)mute;
    AudioRenderer = NvxGraphLookupComponent(player->graph, "AUDREND");
    if (!AudioRenderer)
    {
        NvOsDebugPrintf("AudioRenderer NULL %s[%d]", __FUNCTION__, __LINE__);
        return OMX_ErrorBadParameter;
    }
    OMX_AUDIO_CONFIG_MUTETYPE mt;
    INIT_PARAM(mt, player);
    mt.nPortIndex = 0;
    err = OMX_GetConfig (
        AudioRenderer->handle,
        OMX_IndexConfigAudioMute,
        &mt);
    pParamVolumeMute->bMute = mt.bMute;
    return err;
}

OMX_ERRORTYPE NvxPlayerGraphGetPosition(
    NvxPlayerGraph2 *player, NvS64 *time)
{

    if (player == NULL || time == NULL)
        return OMX_ErrorBadParameter;
    *time = (OMX_TICKS) NvxGraphGetClockTime(player->graph);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxPlayerGraphSetRotateAngle(NvxPlayerGraph2 *player, NvS32 Angle)
{

    NvxComponent *vidRend =NULL;
    OMX_CONFIG_ROTATIONTYPE oRotation;
    OMX_ERRORTYPE err = OMX_ErrorNone;
    if (!player)
        return OMX_ErrorBadParameter;

    vidRend = NvxGraphLookupComponent(player->graph, "VIDREND");
    if (!vidRend)
        return OMX_ErrorBadParameter;
    INIT_PARAM1(oRotation);
    oRotation.nRotation = Angle;
    err = OMX_SetConfig( vidRend->handle, OMX_IndexConfigCommonRotate, &oRotation);
    if (OMX_ErrorNone != err)
            NvOsDebugPrintf("Error Occured at SetRate:0x%x",err);
    return err;
}

OMX_ERRORTYPE NvxPlayerGraphSetMirror(NvxPlayerGraph2 *player, NvS32 mirror)
{

    NvxComponent *vidRend =NULL;
    OMX_CONFIG_MIRRORTYPE oMirror;
    OMX_ERRORTYPE err = OMX_ErrorNone;

    if (!player)
        return OMX_ErrorBadParameter;

    vidRend = NvxGraphLookupComponent(player->graph, "VIDREND");
    if (!vidRend)
        return OMX_ErrorBadParameter;

    INIT_PARAM1(oMirror);
    oMirror.nPortIndex = 0;
    oMirror.eMirror = mirror;
    err = OMX_SetConfig(vidRend->handle, OMX_IndexConfigCommonMirror, &oMirror);
    return err;
}

OMX_ERRORTYPE NvxPlayerGraphSetVideoScale(NvxPlayerGraph2 *player, NvU64 videoScale)
{

    NvxComponent *vidRend =NULL;
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_TIME_CONFIG_SCALETYPE oTimeScale;

    INIT_PARAM1(oTimeScale);
    oTimeScale.xScale = NvSFxFloat2Fixed(videoScale);
    if (!player)
        return OMX_ErrorBadParameter;

    vidRend = NvxGraphLookupComponent(player->graph, "VIDREND");
    if (!vidRend)
        return OMX_ErrorBadParameter;

    err= OMX_SetConfig(vidRend->handle, OMX_IndexConfigTimeScale, &oTimeScale);
    if (err != OMX_ErrorNone)
        NvOsDebugPrintf("\nError at setconfig in setrate: 0x%x compname:%s",err,vidRend->name);

    return err;
}

OMX_ERRORTYPE
NvxPlayerGraphSetBackgroundColor(NvxPlayerGraph2 *player, NvU32 Color)
{
    NvxComponent *vidRend =NULL;
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_CONFIG_COLORKEYTYPE oColorKey;
    INIT_PARAM1(oColorKey);

    if (!player)
        return OMX_ErrorBadParameter;
    vidRend = NvxGraphLookupComponent(player->graph, "VIDREND");
    if (!vidRend)
        return OMX_ErrorBadParameter;

    oColorKey.nARGBColor = Color;
    oColorKey.nARGBMask  = 0xFFFFFFFF;
    err = OMX_SetConfig( vidRend->handle, OMX_IndexConfigCommonColorKey, &oColorKey);
    if (err != OMX_ErrorNone)
        NvOsDebugPrintf("\nError at setconfig in setrate: 0x%x compname:%s",err,vidRend->name);
    return err;
}

