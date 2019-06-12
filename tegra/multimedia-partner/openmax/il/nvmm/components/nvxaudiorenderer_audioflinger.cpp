/*
 * Copyright (c) 2010-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** NvxAudioRenderer.c */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

#include <media/AudioTrack.h>
#include <hardware/audio.h>

extern "C" {
#include <OMX_Core.h>
#include <NVOMX_TrackList.h>

#include "nvassert.h"
#include "common/NvxTrace.h"
#include "components/common/NvxComponent.h"
#include "components/common/NvxPort.h"
#include "nvmm/components/common/nvmmtransformbase.h"
#include "nvmm/components/common/NvxCompReg.h"
#include "nvmm/common/NvxHelpers.h"

#include "nvmm.h"
#include "nvmm_audio.h"
#include "nvmm_mixer.h"
#include "nvmm_wave.h"
#include "NVOMX_TrackList.h"
#include "NvxTrace.h"

#ifdef BUILD_GOOGLETV
#include "OMX_IndexExt.h"
#include "OMX_Audio.h"
#include "OMX_AudioExt.h"
#include "cutils/properties.h"
#endif

using namespace android;

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */

// DEBUG defines. TO DO: fix this
#define MAX_INPUT_BUFFERS    10
#define MIN_INPUT_BUFFERSIZE 100*1024

/* Number of internal frame buffers (AUDIO memory) */
static const OMX_U32 s_nvx_num_ports            = 2;
static const OMX_U32 s_nvx_port_input           = 0;
static const OMX_U32 s_nvx_port_clock           = 1;

/* Audio Renderer State information */
typedef struct SNvxAudioRendererData
{
    AudioTrack *mTrack;
    OMX_BOOL bAudioStarted;
    OMX_AUDIO_PARAM_PCMMODETYPE oLocalMode;

    OMX_TICKS lastTimestamp;

    uint32_t mLatency;

    OMX_BOOL bFirstRun;

    int volume;
    int mute;

    audio_io_handle_t output;

    OMX_BOOL bRateIsPaused;
} SNvxAudioRendererData;

static OMX_ERRORTYPE NvxAudioRendererPreChangeState(NvxComponent *pNvComp,
                                                    OMX_STATETYPE oNewState);

static void SetVolume(SNvxAudioRendererData *pNvxAudio)
{
    float vol = (float)(pNvxAudio->volume) / 256.0;
    if (pNvxAudio->mute)
        vol = 0.0;

    if (pNvxAudio->mTrack)
        pNvxAudio->mTrack->setVolume(vol, vol);
}

static status_t SinkOpen(SNvxAudioRendererData *pNvxAudio, uint32_t sampleRate, int channelCount, int format)
{
    if (pNvxAudio->mTrack)
    {
        delete pNvxAudio->mTrack;
        pNvxAudio->mTrack = 0;
    }

    int afSampleRate, afFrameCount, frameCount;
    int bufferCount = 4;
    int mStreamType = AUDIO_STREAM_MUSIC;

    if (AudioSystem::getOutputFrameCount(&afFrameCount, (audio_stream_type_t)mStreamType) != NO_ERROR) {
        return NO_INIT;
    }
    if (AudioSystem::getOutputSamplingRate(&afSampleRate, (audio_stream_type_t)mStreamType) != NO_ERROR) {
        return NO_INIT;
    }

    frameCount = (sampleRate*afFrameCount*bufferCount)/afSampleRate;

    pNvxAudio->mTrack = new AudioTrack((audio_stream_type_t)mStreamType, sampleRate,
        (audio_format_t)format, (channelCount == 2) ? AUDIO_CHANNEL_OUT_STEREO :
                                      AUDIO_CHANNEL_OUT_MONO,
        frameCount, 0, NULL, NULL, 0, 0);

    if (!pNvxAudio->mTrack || pNvxAudio->mTrack->initCheck() != NO_ERROR)
    {
        delete pNvxAudio->mTrack;
        pNvxAudio->mTrack = NULL;
        return NO_INIT;
    }

    SetVolume(pNvxAudio);

    pNvxAudio->mLatency = pNvxAudio->mTrack->latency();

    return NO_ERROR;
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

static OMX_ERRORTYPE NvxAudioRendererDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioRendererDeInit\n"));

    NvOsFree(pNvComp->pComponentData);
    pNvComp->pComponentData = 0;
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
    SNvxAudioRendererData *data;
    OMX_AUDIO_CONFIG_VOLUMETYPE *vt;
    OMX_AUDIO_CONFIG_MUTETYPE *mt;

    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioRenderGetConfig\n"));

    data = (SNvxAudioRendererData *)pNvComp->pComponentData;

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

            vt->bLinear = OMX_TRUE;
            if (vt->bLinear)
            {
                vt->sVolume.nMin = 0;
                vt->sVolume.nMax = 256;
                vt->sVolume.nValue = data->volume;
            }
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
            mt->bMute = (OMX_BOOL)data->mute;
            break;
        }
        case NVX_IndexConfigAudioSessionId:
        {
            OMX_PARAM_U32TYPE *sessionid = (OMX_PARAM_U32TYPE *)pComponentConfigStructure;
            if (sessionid->nSize != sizeof(OMX_PARAM_U32TYPE))
            {
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxAudioRenderGetConfig:"
                   "[%s(%d)]\n",__FILE__, __LINE__));
                return OMX_ErrorBadParameter;
            }
            if (data->mTrack)
            {
                sessionid->nU32 = data->mTrack->getSessionId();
            }
            else
            {
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxAudioRenderGetConfig:"
                   "[%s(%d)]\n",__FILE__, __LINE__));
                return OMX_ErrorNotReady;
            }
            break;
        }
#ifdef BUILD_GOOGLETV
        case NVX_IndexConfigAudioCaps:
        {
            char prop[PROPERTY_VALUE_MAX];
            memset(prop, 0, sizeof(prop));
            NVX_AUDIO_CONFIG_CAPS *caps = (NVX_AUDIO_CONFIG_CAPS*)pComponentConfigStructure;

            // AC-3 Capability
            property_get("media.tegra.hw.ac3dec", prop, "0");
            caps->supportAc3 = (OMX_BOOL)atoi(prop);

            // E-AC-3 Capability
            property_get("media.tegra.hw.eac3dec", prop, "0");
            caps->supportEac3 = (OMX_BOOL)atoi(prop);

            // DTS Capability
            property_get("media.tegra.hw.dtsdec", prop, "0");
            caps->supportDts = (OMX_BOOL)atoi(prop);
        }
#endif
        default:
            eError = NvxComponentBaseGetConfig( pNvComp, nIndex, pComponentConfigStructure);
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
            if (!vt->bLinear)
            {
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:"
                    "NvxAudioRendererSetConfig:bLinear= %d:[%s(%d)]\n",vt->bLinear,
                    __FILE__, __LINE__));
                return OMX_ErrorBadParameter;
            }
            pNvxAudio->volume = vt->sVolume.nValue;
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
            pNvxAudio->mute = mt->bMute;
            SetVolume(pNvxAudio);
            break;
        }
        default:
            eError = NvxComponentBaseSetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return eError;
}

static OMX_ERRORTYPE NvxAudioRendererWorkerFunction(OMX_IN NvxComponent *pComponent, OMX_IN OMX_BOOL bAllPortsReady, OMX_OUT OMX_BOOL *pbMoreWork, OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    SNvxAudioRendererData   *pNvxAudio = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];
    NvxPort *pPortClock = &pComponent->pPorts[s_nvx_port_clock];
    OMX_BUFFERHEADERTYPE *pInBuffer;

    NVXTRACE((NVXT_WORKER, pComponent->eObjectType, "+NvxAudioRendererWorkerFunction\n"));

    pNvxAudio = (SNvxAudioRendererData *)pComponent->pComponentData;

    *pbMoreWork = (OMX_BOOL)((pPortIn->pCurrentBufferHdr != NULL));

    if (pPortClock && pPortClock->pCurrentBufferHdr)
    {
        NvxPortReleaseBuffer(pPortClock, pPortClock->pCurrentBufferHdr);
    }

    if (!pPortIn->pCurrentBufferHdr)
        return OMX_ErrorNone;

    if (pPortClock && pPortClock->bNvidiaTunneling)
    {
        OMX_HANDLETYPE hClock = pPortClock->hTunnelComponent;
        if (hClock)
        {
            if (NvxCCGetClockRate(hClock) == 0)
            {
                if (!pNvxAudio->bRateIsPaused)
                {
                    pNvxAudio->bRateIsPaused = OMX_TRUE;
                    if (pNvxAudio->mTrack)
                        pNvxAudio->mTrack->pause();
                }
                *pbMoreWork = OMX_FALSE;
                *puMaxMsecToNextCall = 100;
                return OMX_ErrorNone;
            }
            else if (pNvxAudio->bRateIsPaused)
            {
                SetVolume(pNvxAudio);
                if (pNvxAudio->mTrack)
                    pNvxAudio->mTrack->start();
                pNvxAudio->bRateIsPaused = OMX_FALSE;
            }
        }
    }

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

    audio_io_handle_t output = android::AudioSystem::getOutput((audio_stream_type_t)AUDIO_STREAM_MUSIC);
    if (memcmp(pLocalMode, &(pNvxAudio->oLocalMode),
               sizeof(OMX_AUDIO_PARAM_PCMMODETYPE)) ||
            output != pNvxAudio->output)
    {
        int format = AUDIO_FORMAT_PCM_16_BIT;

        pNvxAudio->output = output;

        memcpy(&(pNvxAudio->oLocalMode), pLocalMode,
               sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
        if (pNvxAudio->mTrack)
        {
            pNvxAudio->mTrack->stop();
            pNvxAudio->mTrack->flush();
            delete pNvxAudio->mTrack;
            pNvxAudio->mTrack = 0;
        }

        if (pNvxAudio->oLocalMode.nBitPerSample == 8)
            format = AUDIO_FORMAT_PCM_8_BIT;

        SinkOpen(pNvxAudio, pNvxAudio->oLocalMode.nSamplingRate,
                 pNvxAudio->oLocalMode.nChannels,
                 format);

        if (pNvxAudio->mTrack)
            pNvxAudio->mTrack->start();
        pNvxAudio->bAudioStarted = OMX_TRUE;
    }

    /* If there is data,  play it */
    if (pInBuffer->nFilledLen > 0)
    {
        ssize_t ret = NO_INIT;

        if (pNvxAudio->mTrack)
            ret = pNvxAudio->mTrack->write(pInBuffer->pBuffer +
                                           pInBuffer->nOffset,
                                           pInBuffer->nFilledLen);

        if (pInBuffer->nFilledLen < (OMX_U32)ret)
        {
            NvOsDebugPrintf("ERROR1: %s[%d] %lld nFilledLen %d nOffset %d ret %d\n",
                __FUNCTION__, __LINE__, pInBuffer->nTimeStamp,
                pInBuffer->nFilledLen, pInBuffer->nOffset, ret);
            pInBuffer->nFilledLen = 0;
        }
        else
        {
            pInBuffer->nFilledLen -= ret;
            pInBuffer->nOffset += ret;
        }

        if (!pNvxAudio->bAudioStarted)
        {
            if (pNvxAudio->mTrack)
                pNvxAudio->mTrack->start();
            pNvxAudio->bAudioStarted = OMX_TRUE;
        }
    }

    if (pInBuffer->nFilledLen == 0 && pNvxAudio->mTrack)
    {
        pInBuffer->nOffset = 0;

        if (pInBuffer->nTimeStamp / 100000 != pNvxAudio->lastTimestamp)
        {
            OMX_TICKS newTS = pInBuffer->nTimeStamp;
            newTS -= pNvxAudio->mLatency * 1000;

            if (newTS > 0)
            {
                SendClockUpdate(pComponent, pPortClock, newTS);

                pNvxAudio->lastTimestamp = pInBuffer->nTimeStamp / 100000;
            }
        }

        /* Done with the input buffer, so now release it */
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

    if (pNvxAudio->mTrack)
    {
        pNvxAudio->mTrack->stop();
        delete pNvxAudio->mTrack;
        pNvxAudio->mTrack = 0;
    }

    NvxCheckError(eError, NvxComponentBaseReleaseResources(pComponent));
    return eError;
}

static OMX_ERRORTYPE NvxAudioRendererFlush(OMX_IN NvxComponent *pComponent, OMX_U32 nPort)
{
    SNvxAudioRendererData *pNvxAudio;

    NV_ASSERT(pComponent && pComponent->pComponentData);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAudioRendererFlush\n"));
    pNvxAudio = (SNvxAudioRendererData *)pComponent->pComponentData;

    if (pNvxAudio->mTrack && (nPort == s_nvx_port_input || nPort == OMX_ALL))
    {
        pNvxAudio->mTrack->stop();
        pNvxAudio->mTrack->flush();
        pNvxAudio->mTrack->start();
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxAudioRendererPreChangeState(NvxComponent *pNvComp,
                                                    OMX_STATETYPE oNewState)
{
    SNvxAudioRendererData *pNvxAudio = 0;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioRendererPreChangeState\n"));

    pNvxAudio = (SNvxAudioRendererData *)pNvComp->pComponentData;
    if (!pNvxAudio->mTrack)
        return OMX_ErrorNone;

    if (oNewState == OMX_StateIdle)
    {
        pNvxAudio->mTrack->stop();
    }
    else if (pNvComp->eState == OMX_StateExecuting && oNewState == OMX_StatePause)
    {
        pNvxAudio->mTrack->pause();
    }
    else if (pNvComp->eState == OMX_StatePause && oNewState == OMX_StateExecuting)
    {
        SetVolume(pNvxAudio);
        pNvxAudio->mTrack->start();
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxAudioRendererInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    NvxComponent *pThis = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    SNvxAudioRendererData *data = NULL;
    OMX_AUDIO_PARAM_PCMMODETYPE  *pNvxPcmParameters;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxAudioRendererInit\n"));

    eError = NvxComponentCreate(hComponent, s_nvx_num_ports, &pThis);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType,"ERROR:NvxAudioRendererInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    //NvxAddResource(pThis, NVX_AUDIO_MEDIAPROCESSOR_RES);

    pThis->eObjectType = NVXT_AUDIO_RENDERER;

    data = (SNvxAudioRendererData *)NvOsAlloc(sizeof(SNvxAudioRendererData));
    if (!data)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType,"ERROR:NvxAudioRendererInit:"
            ":SNvxAudioData = %d:[%s(%d)]\n",data,__FILE__, __LINE__));
        return OMX_ErrorInsufficientResources;
    }

    NvOsMemset(data, 0, sizeof(SNvxAudioRendererData));
    data->volume = 256;
    data->mute = 0;
    data->lastTimestamp = (OMX_TICKS)-1;

    pThis->pComponentName = "OMX.Nvidia.audio.render";
    pThis->nComponentRoles = 1;
    pThis->sComponentRoles[0] = "audio_renderer.pcm";

    pThis->pComponentData = data;
    pThis->GetParameter       = NvxAudioRendererGetParameter;
    pThis->SetParameter       = NvxAudioRendererSetParameter;
    pThis->GetConfig          = NvxAudioRendererGetConfig;
    pThis->SetConfig          = NvxAudioRendererSetConfig;
    pThis->WorkerFunction     = NvxAudioRendererWorkerFunction;
    pThis->AcquireResources   = NvxAudioRendererAcquireResources;
    pThis->ReleaseResources   = NvxAudioRendererReleaseResources;
    pThis->PreChangeState     = NvxAudioRendererPreChangeState;
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

}
