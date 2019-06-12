/*
 * Copyright (c) 2010 - 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** NvxAudioRenderer_opensles.c */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

/* Android OpenSL ES headers */
#include <../al/include/SLES/OpenSLES.h>
#include <../al/include/SLES/OpenSLES_Android.h>

#include <OMX_Core.h>
#include <NVOMX_TrackList.h>

#include "nvassert.h"
#include "common/NvxTrace.h"
#include "components/common/NvxComponent.h"
#include "components/common/NvxPort.h"
#include "nvmm/components/common/NvxCompReg.h"
#include "nvmm/common/NvxHelpers.h"

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */

#define PROCESS_OSL_RESULT(r)   \
    if(r != SL_RESULT_SUCCESS)  \
    {                           \
        NvOsDebugPrintf("NvxAudioRenderer failed with error %x\n", r);    \
        return r;               \
    }                           \
                                \

#define PROCESS_OSL_RESULT_VOID(r)\
    if(r != SL_RESULT_SUCCESS)  \
    {                           \
        NvOsDebugPrintf("NvxAudioRenderer failed with error %x\n", r);    \
        return;                 \
    }                           \
                                \

#define MAX_INPUT_BUFFERS    10
#define MIN_INPUT_BUFFERSIZE 24*1024

/* Number of internal frame buffers (AUDIO memory) */
static const OMX_U32 s_nvx_num_ports            = 2;
static const OMX_U32 s_nvx_port_input           = 0;
static const OMX_U32 s_nvx_port_clock           = 1;

/* Audio Renderer State information */
typedef struct SNvxAudioRendererData
{
    OMX_BOOL bAudioStarted;
    OMX_AUDIO_PARAM_PCMMODETYPE oLocalMode;

    OMX_TICKS lastTimestamp;
    OMX_BOOL bFirstRun;

    SLmillibel slVolume;
    OMX_BOOL bMute;
    NvMMQueueHandle usedBufQueue;

    SLObjectItf engineObj;
    SLObjectItf playerObj;
    SLObjectItf outputMixObj;
    SLPlayItf   playItf;
    SLAndroidSimpleBufferQueueItf bqBufferQueue;
    SLVolumeItf volumeItf;

} SNvxAudioRendererData;

static OMX_ERRORTYPE NvxAudioRendererPreChangeState(NvxComponent *pNvComp,
                                                    OMX_STATETYPE oNewState);
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context);

static void SetVolume(SNvxAudioRendererData *pNvxAudio)
{
    SLresult r;
    r = (*(pNvxAudio->volumeItf))->SetVolumeLevel(pNvxAudio->volumeItf, pNvxAudio->slVolume);
    PROCESS_OSL_RESULT_VOID(r);
}

static void SetMute(SNvxAudioRendererData *pNvxAudio)
{
    SLresult r;
    r = (*(pNvxAudio->volumeItf))->SetMute(pNvxAudio->volumeItf, pNvxAudio->bMute);
    PROCESS_OSL_RESULT_VOID(r);
}

// this callback handler is called every time a buffer finishes playing
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    NvxComponent *pComponent;
    OMX_BUFFERHEADERTYPE *pOMXBuffer;
    NvxPort *pPortIn;
    SNvxAudioRendererData *pNvxAudio;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvError nvError;

    pComponent = (NvxComponent *)context;
    pPortIn = &pComponent->pPorts[s_nvx_port_input];
    pNvxAudio = (SNvxAudioRendererData *)pComponent->pComponentData;

    nvError = NvMMQueueDeQ(pNvxAudio->usedBufQueue, &pOMXBuffer);
    if (nvError == NvSuccess)
    {
        pOMXBuffer->nFilledLen = 0;
        //NvOsDebugPrintf("%s: releasing buffer %p length %d\n", __FUNCTION__,
        //                        pOMXBuffer, pOMXBuffer->nFilledLen);
        NvxPortReleaseBuffer(pPortIn, pOMXBuffer);
    }
}

static SLuint32 getChannelMask(OMX_AUDIO_PARAM_PCMMODETYPE *pcmParams)
{
    int i;
    SLuint32 channelMask = 0;

    for (i=0; i<OMX_AUDIO_MAXCHANNELS; i++)
    {
        if (!pcmParams->eChannelMapping[i])
            break;

        switch(pcmParams->eChannelMapping[i])
        {
        case OMX_AUDIO_ChannelLF:
            channelMask |= SL_SPEAKER_FRONT_LEFT;
            break;
        case OMX_AUDIO_ChannelRF:
            channelMask |= SL_SPEAKER_FRONT_RIGHT;
            break;
        case OMX_AUDIO_ChannelCF:
            channelMask |= SL_SPEAKER_FRONT_CENTER;
            break;
        case OMX_AUDIO_ChannelLS:
            channelMask |= SL_SPEAKER_SIDE_LEFT;
            break;
        case OMX_AUDIO_ChannelRS:
            channelMask |= SL_SPEAKER_SIDE_RIGHT;
            break;
        case OMX_AUDIO_ChannelLFE:
            channelMask |= SL_SPEAKER_LOW_FREQUENCY;
            break;
        case OMX_AUDIO_ChannelCS:
            channelMask |= SL_SPEAKER_BACK_CENTER;
            break;
        case OMX_AUDIO_ChannelLR:
            channelMask |= SL_SPEAKER_BACK_LEFT;
            break;
        case OMX_AUDIO_ChannelRR:
            channelMask |= SL_SPEAKER_BACK_RIGHT;
            break;
        default:
            NvOsDebugPrintf("Invalid channel mapping\n");
            break;
        }
    }

    return channelMask;
}

static SLresult SinkOpen(NvxComponent *pComponent)
{
    SLresult r;
    SLEngineItf engineItf;

    SLDataSource srcData;
    SLDataLocator_AndroidSimpleBufferQueue srcDl;
    SLDataFormat_PCM srcFmt;

    SLDataSink snkData;
    SLDataLocator_OutputMix locator_outputmix;

    SLInterfaceID iid[10];
    SLboolean requiredIid[10];

    SNvxAudioRendererData *pNvxAudio;
    NvxPort *pPortIn;
    OMX_AUDIO_PARAM_PCMMODETYPE *pcmParams;
    OMX_S32 i;

    SLMuteSoloItf muteItf;

    pNvxAudio = (SNvxAudioRendererData *)pComponent->pComponentData;
    pPortIn = &pComponent->pPorts[s_nvx_port_input];
    pcmParams = (OMX_AUDIO_PARAM_PCMMODETYPE *)pPortIn->pPortPrivate;

    //NvOsDebugPrintf("Creating engine\n");
    r = slCreateEngine(&pNvxAudio->engineObj, 0, NULL, 0, NULL, NULL);
    PROCESS_OSL_RESULT(r);

    //NvOsDebugPrintf("Realizing engine\n");
    r = (*(pNvxAudio->engineObj))->Realize(pNvxAudio->engineObj, SL_BOOLEAN_FALSE);
    PROCESS_OSL_RESULT(r);

    //NvOsDebugPrintf("Querying for engine interface\n");
    r = (*(pNvxAudio->engineObj))->GetInterface(pNvxAudio->engineObj, SL_IID_ENGINE, (void*)&engineItf);
    PROCESS_OSL_RESULT(r);

    // create output mix, with environmental reverb specified as a non-required interface
    iid[0] = SL_IID_VOLUME;
    requiredIid[0] = SL_BOOLEAN_TRUE;

    //NvOsDebugPrintf("Crating OutputMix object\n");
    r = (*engineItf)->CreateOutputMix(engineItf, &pNvxAudio->outputMixObj, 0, iid, requiredIid);
    PROCESS_OSL_RESULT(r);

    // realize the output mix
    //NvOsDebugPrintf("Realizing OutputMix\n");
    r = (*(pNvxAudio->outputMixObj))->Realize(pNvxAudio->outputMixObj, SL_BOOLEAN_FALSE);
    PROCESS_OSL_RESULT(r);

    // Setup the source.
    srcFmt.formatType = SL_DATAFORMAT_PCM;
    srcFmt.numChannels = pcmParams->nChannels;
    srcFmt.samplesPerSec = pcmParams->nSamplingRate * 1000;
    srcFmt.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
    srcFmt.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
    srcFmt.channelMask = getChannelMask(pcmParams);
    srcFmt.endianness = SL_BYTEORDER_LITTLEENDIAN;

    srcDl.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
    srcDl.numBuffers = MAX_INPUT_BUFFERS;

    srcData.pLocator = &srcDl;
    srcData.pFormat = &srcFmt;

    // Setup the sink.
    locator_outputmix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
    locator_outputmix.outputMix = pNvxAudio->outputMixObj;

    snkData.pLocator = &locator_outputmix;
    snkData.pFormat = NULL;

    iid[0] = SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
    iid[1] = SL_IID_VOLUME;
    iid[2] = SL_IID_MUTESOLO;

    requiredIid[0] = SL_BOOLEAN_TRUE;
    requiredIid[1] = SL_BOOLEAN_TRUE;
    requiredIid[2] = SL_BOOLEAN_TRUE;

    NvOsDebugPrintf("Creating the player channels %d rate %d mask 0x%x\n",
        pcmParams->nChannels, pcmParams->nSamplingRate, srcFmt.channelMask);
    r = (*engineItf)->CreateAudioPlayer(engineItf, &pNvxAudio->playerObj, &srcData, &snkData, 3, iid, requiredIid);
    PROCESS_OSL_RESULT(r);

    //NvOsDebugPrintf("Realizing player\n");
    r = (*(pNvxAudio->playerObj))->Realize(pNvxAudio->playerObj, SL_BOOLEAN_FALSE);
    PROCESS_OSL_RESULT(r);

    //NvOsDebugPrintf("Querying for play interface\n");
    r = (*(pNvxAudio->playerObj))->GetInterface(pNvxAudio->playerObj, SL_IID_PLAY, (void*)&pNvxAudio->playItf);
    PROCESS_OSL_RESULT(r);

    // get the buffer queue interface
    //NvOsDebugPrintf("Querying for buffer queue interface\n");
    r = (*(pNvxAudio->playerObj))->GetInterface(pNvxAudio->playerObj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &pNvxAudio->bqBufferQueue);
    PROCESS_OSL_RESULT(r);

    //NvOsDebugPrintf("Registering callback\n");
    r = (*(pNvxAudio->bqBufferQueue))->RegisterCallback(pNvxAudio->bqBufferQueue, bqPlayerCallback, pComponent);
    PROCESS_OSL_RESULT(r);

    // get the volume interface
    r = (*(pNvxAudio->playerObj))->GetInterface(pNvxAudio->playerObj, SL_IID_VOLUME, &pNvxAudio->volumeItf);
    if (SL_RESULT_SUCCESS == r) {
        r = (*(pNvxAudio->volumeItf))->GetVolumeLevel(pNvxAudio->volumeItf, &pNvxAudio->slVolume);
        PROCESS_OSL_RESULT(r);
    } else {
        NvOsDebugPrintf("could not get SL_IID_VOLUME interface\n");
    }

    return 0;
}

static int SinkClose(SNvxAudioRendererData *pNvxAudio)
{
    if (pNvxAudio->playerObj)
    {
        NvOsDebugPrintf("destroying player object\n");
        (*(pNvxAudio->playerObj))->Destroy(pNvxAudio->playerObj);
    }

    if (pNvxAudio->outputMixObj)
    {
        NvOsDebugPrintf("destroying outputMix object\n");
        (*(pNvxAudio->outputMixObj))->Destroy(pNvxAudio->outputMixObj);
    }

    if (pNvxAudio->engineObj)
    {
        NvOsDebugPrintf("destroying engine object\n");
        (*(pNvxAudio->engineObj))->Destroy(pNvxAudio->engineObj);
    }

    pNvxAudio->playItf = NULL;
    pNvxAudio->playerObj = NULL;
    pNvxAudio->engineObj = NULL;
    pNvxAudio->outputMixObj = NULL;
    return 0;
}

static void FlushUsedBuffers(NvxComponent *pComponent)
{
    NvError nvError = NvSuccess;
    SNvxAudioRendererData *pNvxAudio;
    NvxPort *pPortIn;
    OMX_BUFFERHEADERTYPE *pOMXBuffer;

    pNvxAudio = (SNvxAudioRendererData *)pComponent->pComponentData;
    pPortIn = &pComponent->pPorts[s_nvx_port_input];

    NvU32 numEntries = NvMMQueueGetNumEntries(pNvxAudio->usedBufQueue);
    while(numEntries > 0 && nvError == NvSuccess)
    {
        nvError = NvMMQueueDeQ(pNvxAudio->usedBufQueue, &pOMXBuffer);
        if (nvError == NvSuccess)
        {
            pOMXBuffer->nFilledLen = 0;
            //NvOsDebugPrintf("%s: releasing buffer %p length %d\n", __FUNCTION__,
            //                        pOMXBuffer, pOMXBuffer->nFilledLen);
            NvxPortReleaseBuffer(pPortIn, pOMXBuffer);
            numEntries--;
        }
    }
    if (numEntries)
        NvOsDebugPrintf("Unable to free %d buffers\n", numEntries);
}

static void SendClockUpdate(NvxComponent *pComponent, NvxPort *pPortClock, OMX_TICKS newTS)
{
    OMX_HANDLETYPE hClock;

    hClock = pPortClock->hTunnelComponent;
    if (!hClock)
        return;

    if (pPortClock->bNvidiaTunneling)
    {
        NvxCCSetCurrentAudioRefClock(hClock, newTS);
    }
    else
    {
        OMX_TIME_CONFIG_TIMESTAMPTYPE timestamp;
        timestamp.nSize = sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE);
        timestamp.nVersion = pComponent->oSpecVersion;
        timestamp.nPortIndex = pPortClock->nTunnelPort;
        timestamp.nTimestamp = newTS;

        OMX_SetConfig(hClock, OMX_IndexConfigTimeCurrentAudioReference,
                      &timestamp);
    }
}

static OMX_ERRORTYPE NvxAudioRendererDeInit(NvxComponent *pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    SNvxAudioRendererData *pNvxAudio;
    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAudioRendererDeInit\n"));

    pNvxAudio = (SNvxAudioRendererData *)pComponent->pComponentData;
    SinkClose(pNvxAudio);

    NvMMQueueDestroy(&pNvxAudio->usedBufQueue);

    NvOsFree(pComponent->pComponentData);
    pComponent->pComponentData = 0;
    return eError;
}

static OMX_ERRORTYPE NvxAudioRendererGetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    SNvxAudioRendererData *pNvxAudio;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAudioRendererGetParameter\n"));

    pNvxAudio = (SNvxAudioRendererData *)pComponent->pComponentData;

    switch (nParamIndex)
    {
        case OMX_IndexParamAudioPcm:
        {
            NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];
            OMX_AUDIO_PARAM_PCMMODETYPE *pLocalMode = (OMX_AUDIO_PARAM_PCMMODETYPE *)pPortIn->pPortPrivate;
            OMX_AUDIO_PARAM_PCMMODETYPE *pMode = (OMX_AUDIO_PARAM_PCMMODETYPE *)pComponentParameterStructure;
            if (pLocalMode->nPortIndex == pMode->nPortIndex)
            {
                NvOsMemcpy(pMode, pLocalMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
                return OMX_ErrorNone;
            }
            return OMX_ErrorBadParameter;
        }
    default:
        eError = NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
    };

    return eError;
}

static OMX_ERRORTYPE NvxAudioRendererSetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentParameterStructure)
{
    SNvxAudioRendererData *pNvxAudio;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAudioRendererSetParameter\n"));

    pNvxAudio = (SNvxAudioRendererData *)pComponent->pComponentData;

    switch(nIndex)
    {
        case OMX_IndexParamAudioPcm:
        {
            NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];
            OMX_AUDIO_PARAM_PCMMODETYPE *pLocalMode = (OMX_AUDIO_PARAM_PCMMODETYPE *)pPortIn->pPortPrivate;
            OMX_AUDIO_PARAM_PCMMODETYPE *pMode = (OMX_AUDIO_PARAM_PCMMODETYPE *)pComponentParameterStructure;
            if (pLocalMode->nPortIndex == pMode->nPortIndex)
            {
                NvOsMemcpy(pLocalMode, pMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
                return OMX_ErrorNone;
            }
            return OMX_ErrorBadParameter;
        }
        default:
            eError = NvxComponentBaseSetParameter(pComponent, nIndex, pComponentParameterStructure);
            break;
    }
    return eError;
}

static OMX_ERRORTYPE NvxAudioRendererGetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    SNvxAudioRendererData *pNvxAudio;
    OMX_AUDIO_CONFIG_VOLUMETYPE *vt;
    OMX_AUDIO_CONFIG_MUTETYPE *mt;

    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioRenderGetConfig\n"));

    pNvxAudio = (SNvxAudioRendererData *)pNvComp->pComponentData;

    switch ( nIndex )
    {
        case NVX_IndexConfigGetClock:
        {
            NvxPort *pPortClock = &pNvComp->pPorts[s_nvx_port_clock];
            NVX_CONFIG_GETCLOCK *pGetClock = (NVX_CONFIG_GETCLOCK *)pComponentConfigStructure;
            if (pPortClock)
                pGetClock->hClock = pPortClock->hTunnelComponent;
            else
                pGetClock->hClock = NULL;
            break;
        }
        case OMX_IndexConfigAudioVolume:
        {
            vt = (OMX_AUDIO_CONFIG_VOLUMETYPE *)pComponentConfigStructure;
            if (vt->nPortIndex != 0)
            {
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxAudioRenderGetConfig:"
                   ":nPortIndex = %d:[%s(%d)]\n",vt->nPortIndex ,__FILE__, __LINE__));
                return OMX_ErrorBadParameter;
            }

            vt->bLinear = OMX_FALSE;
            vt->sVolume.nMin = SL_MILLIBEL_MIN;
            vt->sVolume.nMax = 0; // PLATFORM_MILLIBEL_MAX_VOLUME is 0 for android
            vt->sVolume.nValue = pNvxAudio->slVolume;

            break;
        }
        case OMX_IndexConfigAudioMute:
        {
            mt = (OMX_AUDIO_CONFIG_MUTETYPE *)pComponentConfigStructure;
            if (mt->nPortIndex != 0)
            {
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxAudioRenderGetConfig:"
                   ":nPortIndex = %d:[%s(%d)]\n",mt->nPortIndex  ,__FILE__, __LINE__));
                return OMX_ErrorBadParameter;
            }
            mt->bMute = pNvxAudio->bMute;
            break;
        }
        default:
            eError = NvxComponentBaseGetConfig(pNvComp, nIndex, pComponentConfigStructure);
    }
    return eError;
}

static OMX_ERRORTYPE NvxAudioRendererSetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentConfigStructure)
{
    //NVX_CONFIG_TIMESTAMPTYPE *pTimestamp;
    SNvxAudioRendererData *pNvxAudio;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_AUDIO_CONFIG_VOLUMETYPE *vt;
    OMX_AUDIO_CONFIG_MUTETYPE *mt;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioRendererSetConfig\n"));

    pNvxAudio = (SNvxAudioRendererData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
        case OMX_IndexConfigAudioVolume:
        {
            vt = (OMX_AUDIO_CONFIG_VOLUMETYPE *)pComponentConfigStructure;
            if (vt->nPortIndex != 0)
            {
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxAudioRendererSetConfig:"
                  ":nPortIndex = %d:[%s(%d)]\n",vt->nPortIndex,__FILE__, __LINE__));
                return OMX_ErrorBadParameter;
            }
            if (vt->bLinear)
            {
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:"
                    "NvxAudioRendererSetConfig:bLinear= %d:[%s(%d)]\n",vt->bLinear,
                    __FILE__, __LINE__));
                return OMX_ErrorBadParameter;
            }
            pNvxAudio->slVolume = vt->sVolume.nValue;
            SetVolume(pNvxAudio);
            break;
        }
        case OMX_IndexConfigAudioMute:
        {
            mt = (OMX_AUDIO_CONFIG_MUTETYPE *)pComponentConfigStructure;
            if (mt->nPortIndex != 0)
            {
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxAudioRendererSetConfig:"
                   ":nPortIndex = %d:[%s(%d)]\n",mt->nPortIndex ,__FILE__, __LINE__));
                return OMX_ErrorBadParameter;
            }
            pNvxAudio->bMute = mt->bMute;
            SetMute(pNvxAudio);
            break;
        }
        default:
            eError = NvxComponentBaseSetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return eError;
}

static OMX_ERRORTYPE NvxAudioRendererWorkerFunction(OMX_IN NvxComponent *pComponent, OMX_IN OMX_BOOL bAllPortsReady, OMX_OUT OMX_BOOL *pbMoreWork, OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    SLresult r;
    SNvxAudioRendererData   *pNvxAudio = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];
    NvxPort *pPortClock = &pComponent->pPorts[s_nvx_port_clock];
    OMX_BUFFERHEADERTYPE *pInBuffer;
    SLuint32 state;

    NVXTRACE((NVXT_WORKER, pComponent->eObjectType, "+NvxAudioRendererWorkerFunction\n"));

    pNvxAudio = (SNvxAudioRendererData *)pComponent->pComponentData;

    *pbMoreWork = (OMX_BOOL)((pPortIn->pCurrentBufferHdr != NULL));

    if (pPortClock && pPortClock->pCurrentBufferHdr)
    {
        NvxPortReleaseBuffer(pPortClock, pPortClock->pCurrentBufferHdr);
    }

    if (!pPortIn->pCurrentBufferHdr)
        return OMX_ErrorNone;

    pInBuffer = pPortIn->pCurrentBufferHdr;

    if (pInBuffer->nFlags & NVX_BUFFERFLAG_CONFIGCHANGED)
        pNvxAudio->bFirstRun = OMX_TRUE;

    OMX_AUDIO_PARAM_PCMMODETYPE *pLocalMode = (OMX_AUDIO_PARAM_PCMMODETYPE *)pPortIn->pPortPrivate;

    if (pNvxAudio->bFirstRun || pLocalMode->nSamplingRate == 0 ||
        pLocalMode->nChannels == 0 || pLocalMode->nBitPerSample == 0)
    {
        if (pPortIn->hTunnelComponent)
        {
            OMX_AUDIO_PARAM_PCMMODETYPE oPCMMode;

            oPCMMode.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
            oPCMMode.nVersion.nVersion = NvxComponentSpecVersion.nVersion;
            oPCMMode.nPortIndex = pPortIn->nTunnelPort;
            eError = OMX_GetParameter(pPortIn->hTunnelComponent,
                                      OMX_IndexParamAudioPcm,
                                      &oPCMMode);

            if (OMX_ErrorNone == eError)
            {
                NvOsMemcpy(pLocalMode, &oPCMMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
                pLocalMode->nPortIndex = s_nvx_port_input;
                pNvxAudio->bFirstRun = OMX_FALSE;
            }
        }
        else
            pNvxAudio->bFirstRun = OMX_FALSE;
    }

    if (memcmp(pLocalMode, &(pNvxAudio->oLocalMode), sizeof(OMX_AUDIO_PARAM_PCMMODETYPE)))
    {
        // close previous stream in case of format change
        SinkClose(pNvxAudio);

        memcpy(&(pNvxAudio->oLocalMode), pLocalMode,
               sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));

        r = SinkOpen(pComponent);
        if(r != SL_RESULT_SUCCESS)
        {
            NvOsDebugPrintf("SinkOpen failed with error 0x%x\n", r);
            return OMX_ErrorUndefined;
        }

        NvOsDebugPrintf("setting player state to SL_PLAYSTATE_PLAYING\n");
        r = (*(pNvxAudio->playItf))->SetPlayState(pNvxAudio->playItf, SL_PLAYSTATE_PLAYING);
        PROCESS_OSL_RESULT(r);

        pNvxAudio->bAudioStarted = OMX_TRUE;
    }

    //NvOsDebugPrintf("Got buffer %p of size %d flags 0x%x bufcount %d\n",
    //                                pInBuffer, pInBuffer->nFilledLen, pInBuffer->nFlags, bufcount);

    r = (*(pNvxAudio->playItf))->GetPlayState(pNvxAudio->playItf, &state);
    if(r != SL_RESULT_SUCCESS)
    {
        NvOsDebugPrintf("Error querying player state 0x%x\n", r);
        return OMX_ErrorUndefined;
    }

    if (pInBuffer->nFilledLen > 0 && state == SL_PLAYSTATE_PLAYING)
    {
        r = (*(pNvxAudio->bqBufferQueue))->Enqueue(pNvxAudio->bqBufferQueue,
                                            pInBuffer->pBuffer + pInBuffer->nOffset,
                                            pInBuffer->nFilledLen);
        PROCESS_OSL_RESULT(r);

        // push in queue for returning to IL client in bqPlayerCallback
        NvMMQueueEnQ(pNvxAudio->usedBufQueue, &pInBuffer, 0);

        // get next input buffer
        pPortIn->pCurrentBufferHdr = NULL;
        NvxPortGetNextHdr(pPortIn);
    }
    else
    {
        if (state == SL_PLAYSTATE_STOPPED)
        {
            NvOsDebugPrintf("Invalid state... discarding buffer\n");
            pInBuffer->nFilledLen = 0;
        }
    }

    if (pInBuffer->nTimeStamp / 100000 != pNvxAudio->lastTimestamp)
    {
        OMX_TICKS newTS = pInBuffer->nTimeStamp;
        if (newTS > 0)
        {
            SendClockUpdate(pComponent, pPortClock, newTS);
            pNvxAudio->lastTimestamp = pInBuffer->nTimeStamp / 100000;
        }
    }

    if (pInBuffer->nFilledLen <= 0)
    {
        /* nothing to do with this buffer, release it */
        NvxPortReleaseBuffer(pPortIn, pInBuffer);
    }

    *pbMoreWork = OMX_TRUE;
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxAudioRendererAcquireResources(OMX_IN NvxComponent *pComponent)
{
    SNvxAudioRendererData *pNvxAudio = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAudioRendererAcquireResources\n"));

    pNvxAudio = (SNvxAudioRendererData *)pComponent->pComponentData;
    NV_ASSERT(pNvxAudio);

    eError = NvxComponentBaseAcquireResources(pComponent);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pComponent->eObjectType, "ERROR:NvxAudioRendererAcquireResources:"
           ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    return eError;
}

static OMX_ERRORTYPE NvxAudioRendererReleaseResources(OMX_IN NvxComponent *pComponent)
{
    SNvxAudioRendererData *pNvxAudio = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAudioRendererReleaseResources\n"));

    pNvxAudio = (SNvxAudioRendererData *)pComponent->pComponentData;
    NV_ASSERT(pNvxAudio);

    NvxCheckError(eError, NvxComponentBaseReleaseResources(pComponent));
    return eError;
}

static OMX_ERRORTYPE NvxAudioRendererFlush(OMX_IN NvxComponent *pComponent, OMX_U32 nPort)
{
    SNvxAudioRendererData *pNvxAudio;

    NV_ASSERT(pComponent && pComponent->pComponentData);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAudioRendererFlush\n"));
    pNvxAudio = (SNvxAudioRendererData *)pComponent->pComponentData;

    if (pNvxAudio->bAudioStarted /*pNvxAudio->mTrack*/ && (nPort == s_nvx_port_input || nPort == OMX_ALL))
    {
        SLresult r;
        NvOsDebugPrintf("Flushing\n");
         r = (*(pNvxAudio->playItf))->SetPlayState(pNvxAudio->playItf, SL_PLAYSTATE_STOPPED);
        if(r != SL_RESULT_SUCCESS)
        {
            NvOsDebugPrintf("Error setting player state to SL_PLAYSTATE_STOPPED\n");
            return OMX_ErrorUndefined;
        }

        r = (*(pNvxAudio->bqBufferQueue))->Clear(pNvxAudio->bqBufferQueue);
        if(r != SL_RESULT_SUCCESS)
        {
            NvOsDebugPrintf("Error clearing buffer queue\n");
            return OMX_ErrorUndefined;
        }

        FlushUsedBuffers(pComponent);
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxAudioRendererPreChangeState(NvxComponent *pNvComp,
                                                    OMX_STATETYPE oNewState)
{
    SLresult r;
    SNvxAudioRendererData *pNvxAudio;
    SLuint32 currState;

    NV_ASSERT(pNvComp);
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioRendererPreChangeState\n"));

    //NvOsDebugPrintf("Pre Transitioning state from 0x%x to 0x%x\n", pNvComp->eState, oNewState);

    pNvxAudio = (SNvxAudioRendererData *)pNvComp->pComponentData;
    if (!pNvxAudio->playItf)
        return OMX_ErrorNone;

    if (pNvComp->eState != OMX_StateLoaded && oNewState == OMX_StateIdle)
    {
        r = (*(pNvxAudio->playItf))->GetPlayState(pNvxAudio->playItf, &currState);
        if(r != SL_RESULT_SUCCESS)
        {
            NvOsDebugPrintf("Error getting player state\n");
            return OMX_ErrorUndefined;
        }

        if (SL_PLAYSTATE_PAUSED == currState || SL_PLAYSTATE_PLAYING == currState)
        {
            NvOsDebugPrintf("setting player state to SL_PLAYSTATE_STOPPED\n");
            r = (*(pNvxAudio->playItf))->SetPlayState(pNvxAudio->playItf, SL_PLAYSTATE_STOPPED);
            if(r != SL_RESULT_SUCCESS)
            {
                NvOsDebugPrintf("Error setting player state to SL_PLAYSTATE_STOPPED\n");
                return OMX_ErrorUndefined;
            }

            r = (*(pNvxAudio->bqBufferQueue))->Clear(pNvxAudio->bqBufferQueue);
            if(r != SL_RESULT_SUCCESS)
            {
                NvOsDebugPrintf("Error clearing buffer queue\n");
                return OMX_ErrorUndefined;
            }
        }

        // this needs to be done for successful state change
        FlushUsedBuffers(pNvComp);
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxAudioRendererChangeState(NvxComponent *pNvComp,
                                                    OMX_STATETYPE oNewState)
{
    SLresult r;
    SNvxAudioRendererData *pNvxAudio;

    NV_ASSERT(pNvComp);
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioRendererPreChangeState\n"));

    //NvOsDebugPrintf("Transitioning state from 0x%x to 0x%x\n", pNvComp->eState, oNewState);

    pNvxAudio = (SNvxAudioRendererData *)pNvComp->pComponentData;
    if (!pNvxAudio->playItf)
        return OMX_ErrorNone;

    if (pNvComp->eState == OMX_StateExecuting && oNewState == OMX_StatePause)
    {
        NvOsDebugPrintf("setting player state to SL_PLAYSTATE_PAUSED\n");
        r = (*(pNvxAudio->playItf))->SetPlayState(pNvxAudio->playItf, SL_PLAYSTATE_PAUSED);
        if(r != SL_RESULT_SUCCESS)
        {
            NvOsDebugPrintf("Error setting player state to SL_PLAYSTATE_PAUSED\n");
            return OMX_ErrorUndefined;
        }
    }
    else if (pNvComp->eState == OMX_StatePause && oNewState == OMX_StateExecuting)
    {
        //SetVolume(pNvxAudio);
        NvOsDebugPrintf("setting player state to SL_PLAYSTATE_PLAYING\n");
        r = (*(pNvxAudio->playItf))->SetPlayState(pNvxAudio->playItf, SL_PLAYSTATE_PLAYING);
        if(r != SL_RESULT_SUCCESS)
        {
            NvOsDebugPrintf("Error setting player state to SL_PLAYSTATE_PLAYING\n");
            return OMX_ErrorUndefined;
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxAudioRendererInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    NvxComponent *pThis = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvError nvError;
    SNvxAudioRendererData *pNvxAudio = NULL;
    OMX_AUDIO_PARAM_PCMMODETYPE  *pNvxPcmParameters;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxAudioRendererInit\n"));

    eError = NvxComponentCreate(hComponent, s_nvx_num_ports, &pThis);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType,"ERROR:NvxAudioRendererInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pThis->eObjectType = NVXT_AUDIO_RENDERER;

    pNvxAudio = (SNvxAudioRendererData *)NvOsAlloc(sizeof(SNvxAudioRendererData));
    if (!pNvxAudio)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType,"ERROR:NvxAudioRendererInit:"
            ":SNvxAudioData = %d:[%s(%d)]\n",pNvxAudio,__FILE__, __LINE__));
        return OMX_ErrorInsufficientResources;
    }

    NvOsMemset(pNvxAudio, 0, sizeof(SNvxAudioRendererData));
    pNvxAudio->slVolume = 0;
    pNvxAudio->bMute = SL_BOOLEAN_FALSE;
    pNvxAudio->lastTimestamp = (OMX_TICKS)-1;

    nvError = NvMMQueueCreate(&pNvxAudio->usedBufQueue, MAX_INPUT_BUFFERS, sizeof(OMX_BUFFERHEADERTYPE *), 1);
    if (nvError != NvSuccess)
    {
        NvOsDebugPrintf("NvMMQueueCreate failed error %x\n", nvError);
        return OMX_ErrorInsufficientResources;
    }

    pThis->pComponentName = "OMX.Nvidia.audio.render";
    pThis->nComponentRoles = 1;
    pThis->sComponentRoles[0] = "audio_renderer.pcm";

    pThis->pComponentData     = pNvxAudio;
    pThis->GetParameter       = NvxAudioRendererGetParameter;
    pThis->SetParameter       = NvxAudioRendererSetParameter;
    pThis->GetConfig          = NvxAudioRendererGetConfig;
    pThis->SetConfig          = NvxAudioRendererSetConfig;
    pThis->WorkerFunction     = NvxAudioRendererWorkerFunction;
    pThis->AcquireResources   = NvxAudioRendererAcquireResources;
    pThis->ReleaseResources   = NvxAudioRendererReleaseResources;
    pThis->PreChangeState     = NvxAudioRendererPreChangeState;
    pThis->ChangeState        = NvxAudioRendererChangeState;
    pThis->DeInit             = NvxAudioRendererDeInit;
    pThis->Flush              = NvxAudioRendererFlush;

    pThis->pPorts[s_nvx_port_input].oPortDef.nPortIndex = s_nvx_port_input;

    NvxPortInitAudio(&pThis->pPorts[s_nvx_port_input], OMX_DirInput,
                     MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE,
                     OMX_AUDIO_CodingPCM);

    /* Initialize default parameters - required of a standard component */
    ALLOC_STRUCT(pThis, pNvxPcmParameters, OMX_AUDIO_PARAM_PCMMODETYPE);
    if (!pNvxPcmParameters)
        return OMX_ErrorInsufficientResources;

    //Initialize the default values for the standard component requirement.
    pNvxPcmParameters->nPortIndex = pThis->pPorts[s_nvx_port_input].oPortDef.nPortIndex;
    pNvxPcmParameters->nChannels = 2;
    pNvxPcmParameters->eNumData = OMX_NumericalDataSigned;
    pNvxPcmParameters->nSamplingRate = 48000;
    pNvxPcmParameters->ePCMMode = OMX_AUDIO_PCMModeLinear;
    pNvxPcmParameters->bInterleaved = OMX_TRUE;
    pNvxPcmParameters->nBitPerSample = 16;
    pNvxPcmParameters->eChannelMapping[0] = OMX_AUDIO_ChannelLF;
    pNvxPcmParameters->eChannelMapping[1] = OMX_AUDIO_ChannelRF;

    pThis->pPorts[s_nvx_port_input].pPortPrivate = pNvxPcmParameters;

    NvxPortInitOther(&pThis->pPorts[s_nvx_port_clock], OMX_DirInput, 4,
                     sizeof(OMX_TIME_MEDIATIMETYPE), OMX_OTHER_FormatTime);
    pThis->pPorts[s_nvx_port_clock].eNvidiaTunnelTransaction = ENVX_TRANSACTION_CLOCK;

    return eError;
}


