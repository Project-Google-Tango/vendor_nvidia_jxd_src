/*
 * Copyright (c) 2011 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** NvxAlsaSink.c */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

#include <OMX_Core.h>
#include <NVOMX_TrackList.h>

#include "nvassert.h"
#include "common/NvxTrace.h"
#include "components/common/NvxComponent.h"
#include "components/common/NvxPort.h"
#include "nvmm/components/common/NvxCompReg.h"
#include "nvmm/common/NvxHelpers.h"
#include "NvxTrace.h"
#include <alsa/asoundlib.h>

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */

// DEBUG defines. TO DO: fix this
#define MAX_INPUT_BUFFERS    10
#define MIN_INPUT_BUFFERSIZE 100*1024

#define MIXER_HEADPHONE_ID_NAME "Headphone"
#define MIXER_SPEAKER_ID_NAME "Speaker"

/* Added for OMXplayer audio bugId 857114*/
typedef enum
{
    NvAlsaDevice_None = 0,
    NvAlsaDevice_Headphone,
    NvAlsaDevice_Speaker = 4,
    NvAlsaDevice_Last
} NvAlsaDevice;

/* Number of internal frame buffers (AUDIO memory) */
static const int s_nvx_num_ports            = 2;
static const int s_nvx_port_input           = 0;
static const int s_nvx_port_clock           = 1;

/* Audio Renderer State information */
typedef struct
{
    snd_pcm_t *playback_handle;
    snd_pcm_hw_params_t* hw_params;

    OMX_BOOL bInitialized;
    OMX_BOOL bTunneling;
    OMX_BOOL bHaveEndBuffer;
    NvOsSemaphoreHandle eosWaitSem;

    OMX_TICKS lastposition;

    OMX_BOOL bFirstRun;
    OMX_BOOL bConfigured;
    NVX_AUDIO_OUTPUTTYPE eOutputType;
    OMX_STATETYPE eState;
    OMX_U32 nUpdateCount;
    OMX_U32 nSamplingRate;
    OMX_S32 bFirstAudioFrameUpdated;
    snd_pcm_t   *pNativeRender;
    OMX_S32 volume;
    OMX_BOOL mute;

}SNvxAlsaSinkData;

/* Alsa Simple Mixer Interface for Volume Control */
typedef struct{
    snd_mixer_t * hMixer;
    snd_mixer_elem_t * mixelem;
    OMX_S32 minVol;
    OMX_S32 maxVol;
    OMX_S32 minVoldB;
    OMX_S32 maxVoldB;
}NvAlsaMixer;
static NvAlsaMixer NvAlsaMixerData;

static OMX_ERRORTYPE NvxAlsaSinkPreChangeState(NvxComponent *pNvComp,OMX_STATETYPE oNewState);
static OMX_ERRORTYPE NvxAlsaSinkOpen(SNvxAlsaSinkData * pNvxAudio, NvxComponent *pComponent);
static OMX_ERRORTYPE NvxAlsaSinkClose(SNvxAlsaSinkData *pNvxAudio);
static OMX_ERRORTYPE NvxAlsaSinkDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAlsaSinkInit\n"));

    NvOsFree(pNvComp->pComponentData);
    pNvComp->pComponentData = 0;
    return eError;
}

static OMX_ERRORTYPE NvxAlsaSinkGetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    SNvxAlsaSinkData *pNvxAudio;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAlsaSinkGetParameter\n"));

    pNvxAudio = (SNvxAlsaSinkData *)pComponent->pComponentData;

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


static OMX_ERRORTYPE NvxAlsaSetAudioParams(SNvxAlsaSinkData *pNvxAudio, OMX_AUDIO_PARAM_PCMMODETYPE *pLocalMode )
{
    unsigned int rate;
    snd_pcm_uframes_t period_size,buffer_size;
    snd_pcm_format_t snd_pcm_format = SND_PCM_FORMAT_UNKNOWN;
    snd_pcm_sw_params_t *sw_params;

    if(pNvxAudio->bConfigured)
       return OMX_ErrorNone;

    if(snd_pcm_hw_params_set_channels(pNvxAudio->playback_handle,pNvxAudio->hw_params,pLocalMode->nChannels))
    {
        NvOsDebugPrintf("NvxAlsaSinkSetParameter: Error setting nChannels %d\n",pLocalMode->nChannels);
        return OMX_ErrorBadParameter;
    }
    if(pLocalMode->bInterleaved == OMX_TRUE)
    {
        if(snd_pcm_hw_params_set_access(pNvxAudio->playback_handle,pNvxAudio->hw_params,SND_PCM_ACCESS_RW_INTERLEAVED)<0)
        {
            NvOsDebugPrintf("NvxAlsaSinkSetParameter: Error Setting Interleaved RW \n");
            return OMX_ErrorHardware;
        }
    }
    else
    {
        if(snd_pcm_hw_params_set_access(pNvxAudio->playback_handle,pNvxAudio->hw_params,SND_PCM_ACCESS_RW_NONINTERLEAVED)<0)
        {
            NvOsDebugPrintf("NvxAlsaSinkSetParameter: Error Setting Non-Interleaved RW \n");
            return OMX_ErrorHardware;
        }
    }
    rate = pLocalMode->nSamplingRate;
    if(snd_pcm_hw_params_set_rate_near(pNvxAudio->playback_handle,pNvxAudio->hw_params,&rate,0)<0)
    {
        NvOsDebugPrintf("NvxAlsaSinkSetParameter: Error Setting rate \n");
        return OMX_ErrorHardware;
    }
    else
    {
        //NvOsDebugPrintf("Alsa Sampling rate set near %lu \n",rate);
    }
    pNvxAudio->nSamplingRate = rate;
    switch(pLocalMode->nBitPerSample)
    {
        case 8 :
            snd_pcm_format = SND_PCM_FORMAT_S8;
            break;
        case 16 :
            snd_pcm_format = SND_PCM_FORMAT_S16_LE;
            break;
        case 24 :
            snd_pcm_format = SND_PCM_FORMAT_S24_LE;
            break;
        case 32 :
            snd_pcm_format = SND_PCM_FORMAT_S32_LE;
            break;
        default :
            return OMX_ErrorBadParameter;
    }
    if(snd_pcm_format != SND_PCM_FORMAT_UNKNOWN)
    {
        if(snd_pcm_hw_params_set_format(pNvxAudio->playback_handle,pNvxAudio->hw_params,snd_pcm_format)<0)
        {
            NvOsDebugPrintf("NvxAlsaSinkSetParameter: Error Setting NumBitsPerSample\n");
            return OMX_ErrorHardware;
        }
    }
    else
    {
        return OMX_ErrorBadParameter;
    }
    //Set the Buffer-Size and Period-Size
    period_size = (rate / 8);
    buffer_size = (period_size * 4);
    if(snd_pcm_hw_params_set_period_size_near(pNvxAudio->playback_handle,pNvxAudio->hw_params,\
        &period_size,0)<0)
    {
        NvOsDebugPrintf("Warning NvxAlsaSinkSetParameter: Period Size is not set to %d\n",period_size);
    }
    if(snd_pcm_hw_params_set_buffer_size_near(pNvxAudio->playback_handle,pNvxAudio->hw_params,\
        &buffer_size)<0)
    {

        NvOsDebugPrintf("Warning NvxAlsaSinkSetParameter: Buffer Size is not set to %d\n",buffer_size);
    }

    //Configure and Prepare ALSA
    if(snd_pcm_hw_params(pNvxAudio->playback_handle,pNvxAudio->hw_params)<0)
    {
        NvOsDebugPrintf("NvxAlsaSinkSetParameter: Cannont Configure Alsa \n");
        return OMX_ErrorHardware;
    }

    //Configure the software params to avoid polling
    if (snd_pcm_sw_params_malloc (&sw_params) < 0) {
       NvOsDebugPrintf("cannot allocate software parameters structure \n");
       return OMX_ErrorHardware;
    }

    if ((snd_pcm_sw_params_current (pNvxAudio->playback_handle, sw_params)) < 0) {
       NvOsDebugPrintf("cannot initialize software parameters structure \n");
       return OMX_ErrorHardware;
    }

    if ((snd_pcm_sw_params_set_avail_min (pNvxAudio->playback_handle, sw_params,2 * period_size ))<0){
       NvOsDebugPrintf("cannot set minimum available count \n");
       return OMX_ErrorHardware;
    }

    if ((snd_pcm_sw_params_set_start_threshold (pNvxAudio->playback_handle, sw_params, 0U)) < 0) {
        NvOsDebugPrintf("cannot set start mode \n");
       return OMX_ErrorHardware;
    }

    if ((snd_pcm_sw_params (pNvxAudio->playback_handle, sw_params)) < 0) {
        NvOsDebugPrintf("cannot set software parameters \n");
        return OMX_ErrorHardware;
    }

    snd_pcm_sw_params_free (sw_params);

    if(snd_pcm_prepare(pNvxAudio->playback_handle)<0)
    {
        NvOsDebugPrintf("NvxAlsaSinkSetParameter: Cannont Prepare Alsa \n");
        return OMX_ErrorHardware;
    }
    pNvxAudio->bConfigured = OMX_TRUE;
    return OMX_ErrorNone;

}

static OMX_ERRORTYPE NvxAlsaSinkSetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentParameterStructure)
{
    SNvxAlsaSinkData *pNvxAudio;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAlsaSinkSetParameter\n"));

    pNvxAudio = (SNvxAlsaSinkData *)pComponent->pComponentData;

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
                   if(pNvxAudio->bInitialized)
                        return(NvxAlsaSetAudioParams(pNvxAudio,pLocalMode));
            }
            return OMX_ErrorBadParameter;
        }
        break;
        case OMX_IndexParamPortDefinition:
        {
            OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;

            if (pPortDef->format.audio.pNativeRender)
            {
                pNvxAudio->pNativeRender = pPortDef->format.audio.pNativeRender;
            }

            eError = NvxComponentBaseSetParameter(pComponent, nIndex, pComponentParameterStructure);
        }
        break;
        default:
            eError = NvxComponentBaseSetParameter(pComponent, nIndex, pComponentParameterStructure);
            break;
    }
    return eError;
}

static OMX_ERRORTYPE GetMute(OMX_BOOL *mute)
{
    OMX_S32 leftSwitch, rightSwitch;

    if (!NvAlsaMixerData.hMixer)
    {
        NvOsDebugPrintf("%s: Mixer Handle NULL\n",__FUNCTION__);
        return OMX_ErrorNotReady;
    }

    if((snd_mixer_selem_get_playback_switch(NvAlsaMixerData.mixelem,
            SND_MIXER_SCHN_FRONT_LEFT,(int *)&leftSwitch) !=0) ||
            (snd_mixer_selem_get_playback_switch(NvAlsaMixerData.mixelem,
            SND_MIXER_SCHN_FRONT_RIGHT,(int *)&rightSwitch) !=0))
    {
        NvOsDebugPrintf("%s: snd_mixer_selem_get_playback_switch Failed\n",__FUNCTION__);
        return OMX_ErrorNotReady;
    }

    *mute = !(leftSwitch || rightSwitch);
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE SetMute(OMX_BOOL mute)
{
    if (!NvAlsaMixerData.hMixer)
    {
        NvOsDebugPrintf("%s: Mixer Handle NULL\n",__FUNCTION__);
        return OMX_ErrorNotReady;
    }

    if(snd_mixer_selem_set_playback_switch_all(NvAlsaMixerData.mixelem, !mute) !=0)
    {
        NvOsDebugPrintf("%s: snd_mixer_selem_set_playback_switch_all Failed\n",__FUNCTION__);
        return OMX_ErrorNotReady;
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE
GetVolumedB(OMX_AUDIO_CONFIG_VOLUMETYPE * vt)
{
    OMX_S32 left, right;

    if (!NvAlsaMixerData.hMixer)
    {
        NvOsDebugPrintf("%s: Mixer Handle NULL\n",__FUNCTION__);
        return OMX_ErrorNotReady;
    }
    /* Obtain current volume in dB*/
    if (snd_mixer_selem_get_playback_dB(NvAlsaMixerData.mixelem,
            SND_MIXER_SCHN_FRONT_LEFT, &left) != 0)
        return OMX_ErrorNotReady;

    if (snd_mixer_selem_get_playback_dB(NvAlsaMixerData.mixelem,
            SND_MIXER_SCHN_FRONT_RIGHT, &right) != 0)
        return OMX_ErrorNotReady;

    vt->sVolume.nValue = (left + right) / 2;
    vt->sVolume.nMin = NvAlsaMixerData.minVoldB;
    vt->sVolume.nMax = NvAlsaMixerData.maxVoldB;
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE
GetVolume(OMX_AUDIO_CONFIG_VOLUMETYPE * vt)
{
    OMX_S32 left, right;

    if (!NvAlsaMixerData.hMixer)
    {
        NvOsDebugPrintf("%s: Mixer Handle NULL\n",__FUNCTION__);
        return OMX_ErrorNotReady;
    }
    /* Obtain current volume */
    if (snd_mixer_selem_get_playback_volume(NvAlsaMixerData.mixelem,
            SND_MIXER_SCHN_FRONT_LEFT, &left) != 0)
        return OMX_ErrorNotReady;

    if (snd_mixer_selem_get_playback_volume(NvAlsaMixerData.mixelem,
            SND_MIXER_SCHN_FRONT_RIGHT, &right) != 0)
        return OMX_ErrorNotReady;

    /* Using Volume Range of 0 to 100*/
    vt->sVolume.nValue = (((left + right) / 2 - NvAlsaMixerData.minVol) * 100) / (NvAlsaMixerData.maxVol - NvAlsaMixerData.minVol);
    vt->sVolume.nMin = 0;
    vt->sVolume.nMax = 100;
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE SetVolumedB(OMX_AUDIO_CONFIG_VOLUMETYPE * vt)
{
    OMX_S32 newVol = vt->sVolume.nValue;
    OMX_S32 direction = 1;
    if (!NvAlsaMixerData.hMixer)
    {
        NvOsDebugPrintf("%s: Mixer Handle NULL\n",__FUNCTION__);
        return OMX_ErrorNotReady;
    }

    direction = newVol > 0 ? -1 : 1;

    /* Set new dB volume */
    if (snd_mixer_selem_set_playback_dB(NvAlsaMixerData.mixelem,
                SND_MIXER_SCHN_FRONT_LEFT, newVol, direction) != 0)
    {
        NvOsDebugPrintf("%s: snd_mixer_selem_set_playback_volume Failed\n",__FUNCTION__);
        return OMX_ErrorNotReady;
    }
    if (snd_mixer_selem_set_playback_dB(NvAlsaMixerData.mixelem,
                SND_MIXER_SCHN_FRONT_RIGHT, newVol, direction) != 0)
    {
        NvOsDebugPrintf("%s: snd_mixer_selem_set_playback_volume Failed\n",__FUNCTION__);
        return OMX_ErrorNotReady;
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE SetVolume(OMX_AUDIO_CONFIG_VOLUMETYPE * vt)
{
    OMX_S32 newVol = vt->sVolume.nValue;
    OMX_S32 actualVolume;
    if (!NvAlsaMixerData.hMixer)
    {
        NvOsDebugPrintf("%s: Mixer Handle NULL\n",__FUNCTION__);
        return OMX_ErrorNotReady;
    }
    /* Convert Percent to Absolute Value */
    actualVolume =
        ((newVol * (NvAlsaMixerData.maxVol - NvAlsaMixerData.minVol)) / 100) + NvAlsaMixerData.minVol;

    /* Set new volume */
    if (snd_mixer_selem_set_playback_volume(NvAlsaMixerData.mixelem,
                SND_MIXER_SCHN_FRONT_LEFT, actualVolume) != 0)
    {
        NvOsDebugPrintf("%s: snd_mixer_selem_set_playback_volume Failed\n",__FUNCTION__);
        return OMX_ErrorNotReady;
    }
    if (snd_mixer_selem_set_playback_volume(NvAlsaMixerData.mixelem,
                SND_MIXER_SCHN_FRONT_RIGHT, actualVolume) != 0)
    {
        NvOsDebugPrintf("%s: snd_mixer_selem_set_playback_volume Failed\n",__FUNCTION__);
        return OMX_ErrorNotReady;
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxAlsaSinkGetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    SNvxAlsaSinkData *data;

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_AUDIO_CONFIG_VOLUMETYPE *vt;
    OMX_AUDIO_CONFIG_MUTETYPE *mt;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioRenderGetConfig\n"));

    data = (SNvxAlsaSinkData *)pNvComp->pComponentData;

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
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxAlsaRenderGetConfig:"
                   ":nPortIndex = %d:[%s(%d)]\n",vt->nPortIndex ,__FILE__, __LINE__));
                return OMX_ErrorBadParameter;
            }
            if (!vt->bLinear)
            {
                eError = GetVolumedB(vt);
            }
            else
            {
                eError = GetVolume(vt);
            }
            if(eError)
                return eError;
            break;
        }
        case OMX_IndexConfigAudioMute:
        {
            mt = (OMX_AUDIO_CONFIG_MUTETYPE *)pComponentConfigStructure;
            if (mt->nPortIndex != 0)
            {
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxAlsaRenderGetConfig:"
                   ":nPortIndex = %d:[%s(%d)]\n",mt->nPortIndex  ,__FILE__, __LINE__));
                return OMX_ErrorBadParameter;
            }
            eError = GetMute(&mt->bMute);
            if(eError)
                return eError;
            break;
        }

        default:
            eError = NvxComponentBaseGetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return eError;
}

static OMX_ERRORTYPE NvxAlsaSinkSetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentConfigStructure)
{
    //NVX_CONFIG_TIMESTAMPTYPE *pTimestamp;
    SNvxAlsaSinkData *pNvxAudio;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_AUDIO_CONFIG_VOLUMETYPE *vt;
    OMX_AUDIO_CONFIG_MUTETYPE *mt;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAlsaSinkSetConfig\n"));

    pNvxAudio = (SNvxAlsaSinkData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
        case  NVX_IndexConfigAudioOutputType:
        {
            NVX_CONFIG_AUDIOOUTPUT *ot = (NVX_CONFIG_AUDIOOUTPUT *)pComponentConfigStructure;
            pNvxAudio->eOutputType = ot->eOutputType;
            break;
        }
        case OMX_IndexConfigAudioVolume:
        {
            vt = (OMX_AUDIO_CONFIG_VOLUMETYPE *)pComponentConfigStructure;
            if (vt->nPortIndex != 0)
            {
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxAlsaRendererSetConfig:"
                  ":nPortIndex = %d:[%s(%d)]\n",vt->nPortIndex,__FILE__, __LINE__));
                return OMX_ErrorBadParameter;
            }

            if (!vt->bLinear)
            {
                eError = SetVolumedB(vt);
            }
            else
            {
                eError = SetVolume(vt);
            }
            if(eError)
                return eError;
            pNvxAudio->volume = vt->sVolume.nValue;
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
            eError = SetMute(mt->bMute);
            if(eError)
                return eError;
            pNvxAudio->mute = mt->bMute;
            break;
        }

        default:
            eError = NvxComponentBaseSetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return eError;
}

static int
xrun_recovery (SNvxAlsaSinkData *pNvxAudio, int err)
{
    if (err == -EPIPE)
    {          /* under-run */
        err = snd_pcm_prepare (pNvxAudio->playback_handle);
        if (err < 0)
            return 0;
    }
    else if (err == -ESTRPIPE)
    {
        while ((err = snd_pcm_resume (pNvxAudio->playback_handle)) == -EAGAIN)
            NvOsSleepMS(100);           /* wait until the suspend flag is released */
        if (err < 0)
        {
            err = snd_pcm_prepare (pNvxAudio->playback_handle);
            if (err < 0)
            {
            }
        }
        return 0;
    }
    return err;
}

static OMX_ERRORTYPE NvxAlsaPlayBackBuffer(SNvxAlsaSinkData *pNvxAudio,OMX_BUFFERHEADERTYPE *pOMXBuf,OMX_AUDIO_PARAM_PCMMODETYPE *pLocalMode,OMX_BOOL *pbWouldFail)
{
    OMX_U32 frameSize;
    OMX_S32 written;
    OMX_S32 totalBuffer,toSend;
    OMX_S32 offsetBuffer;
    OMX_BOOL allDataSent;
    snd_pcm_sframes_t spaceAvailable;

    *pbWouldFail = OMX_FALSE;
    frameSize = pLocalMode->nChannels * (pLocalMode->nBitPerSample >> 3);
    allDataSent = OMX_FALSE;
    totalBuffer = pOMXBuf->nFilledLen / frameSize;
    offsetBuffer = 0;
    while(!allDataSent)
    {
        written = snd_pcm_wait(pNvxAudio->playback_handle,1000);
        if ((spaceAvailable = snd_pcm_avail_update (pNvxAudio->playback_handle)) < 0) {
            if (spaceAvailable == -EPIPE) {
                if (xrun_recovery (pNvxAudio, written) < 0) {
                    NvOsDebugPrintf("ALSA Sink Worker Function : xrun cant recover\n");
                    *pbWouldFail = OMX_TRUE;
                    return OMX_ErrorHardware;
                }
                break;
            } else {
                NvOsDebugPrintf("ALSA Sink Worker Function :ALSA avail update (%ld)\n",
                spaceAvailable);
                break;
            }
        }

        toSend = totalBuffer>spaceAvailable?spaceAvailable:totalBuffer;

        if(written >= 0)
            written = snd_pcm_writei(pNvxAudio->playback_handle,pOMXBuf->pBuffer + (offsetBuffer * frameSize),toSend);
        if (written < 0) {
            if (written == -EAGAIN) {
                continue;
            }else if (xrun_recovery (pNvxAudio, written) < 0) {
                NvOsDebugPrintf("ALSA Sink Worker Function : Cannot send data to Audio Driver \n");
                *pbWouldFail = OMX_TRUE;
                return OMX_ErrorHardware;
            }
            continue;
        }
        if(written != totalBuffer) {
            totalBuffer = totalBuffer - written;
            offsetBuffer += written;
        }
        else {
            allDataSent = OMX_TRUE;
        }
    }
    return OMX_ErrorNone;
}


static OMX_ERRORTYPE NvxAlsaSinkWorkerFunction(OMX_IN NvxComponent *pComponent, OMX_IN OMX_BOOL bAllPortsReady, OMX_OUT OMX_BOOL *pbMoreWork, OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    SNvxAlsaSinkData   *pNvxAudio = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];
    NvxPort *pPortClock = &pComponent->pPorts[s_nvx_port_clock];
    OMX_TICKS position = 0;
    OMX_BOOL bWouldFail = OMX_TRUE;
    OMX_BOOL bSetPosition = OMX_FALSE;
    OMX_BUFFERHEADERTYPE *pOMXBuf = NULL;
    NVXTRACE((NVXT_WORKER, pComponent->eObjectType, "+NvxAlsaSinkWorkerFunction\n"));

    pNvxAudio = (SNvxAlsaSinkData *)pComponent->pComponentData;
    *pbMoreWork = (pPortIn->pCurrentBufferHdr != NULL);
    if (pPortClock && pPortClock->pCurrentBufferHdr)
    {
        NvxPortReleaseBuffer(pPortClock, pPortClock->pCurrentBufferHdr);
    }

    if (!pPortIn->pCurrentBufferHdr)
    {
        return OMX_ErrorNone;
    }

    pOMXBuf = pPortIn->pCurrentBufferHdr;

    {
        OMX_AUDIO_PARAM_PCMMODETYPE oPCMMode;
        OMX_AUDIO_PARAM_PCMMODETYPE *pLocalMode = (OMX_AUDIO_PARAM_PCMMODETYPE *)pPortIn->pPortPrivate;
        OMX_BOOL bQueryPosition = OMX_TRUE;

        if (pPortIn->pCurrentBufferHdr->nFlags & NVX_BUFFERFLAG_CONFIGCHANGED)
        {
            pNvxAudio->bFirstRun = OMX_TRUE;
        }

        if (pNvxAudio->bFirstRun || pLocalMode->nSamplingRate == 0 ||
            pLocalMode->nChannels == 0 || pLocalMode->nBitPerSample == 0)
        {
            if (pPortIn->hTunnelComponent)
            {
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
                    NvxAlsaSetAudioParams(pNvxAudio,pLocalMode);
                    pNvxAudio->bFirstRun = OMX_FALSE;
                }
            }
               else {
                NvxAlsaSetAudioParams(pNvxAudio,pLocalMode);
                pNvxAudio->bFirstRun = OMX_FALSE;
            }
        }

        if(pNvxAudio->eState != OMX_StateExecuting)
        {
            return OMX_ErrorNotReady;
        }

        /* Lets Do AudioPlayback in WorkerThread itself for now */
        if(pOMXBuf->nFilledLen)
        {
            NvxAlsaPlayBackBuffer(pNvxAudio,pOMXBuf,pLocalMode,&bWouldFail);
            pOMXBuf->nFilledLen = 0;
        }

        if (pPortIn->pCurrentBufferHdr->nFlags & OMX_BUFFERFLAG_EOS)
        {
            pNvxAudio->bHaveEndBuffer = OMX_TRUE;
            bQueryPosition = OMX_FALSE;
            NvxSendEvent(pComponent, OMX_EventBufferFlag,
                         pComponent->pPorts[s_nvx_port_input].oPortDef.nPortIndex,
                         OMX_BUFFERFLAG_EOS, 0);
        }

        if(bQueryPosition && pPortIn->pCurrentBufferHdr)
        {
            position = pPortIn->pCurrentBufferHdr->nTimeStamp * 10;
            bSetPosition = OMX_TRUE;
        }

        NvxPortReleaseBuffer(pPortIn, pPortIn->pCurrentBufferHdr);

        if (!pPortIn->pCurrentBufferHdr)
            NvxPortGetNextHdr(pPortIn);

        *pbMoreWork = (pPortIn->pCurrentBufferHdr && !bWouldFail);


    }

    if (pPortClock && position / 100000 != pNvxAudio->lastposition && bSetPosition &&
        position != (OMX_TICKS)-1)
    {
        OMX_HANDLETYPE hClock = pPortClock->hTunnelComponent;
         snd_pcm_sframes_t  delayx,delayy;
        OMX_TICKS delayPTS;
        float val;
        /* AV-Sync */
        /* Maximum delay is usally the total buffersize available
         * in the ring bufffer. Now calculate the delayPTS that
         * will be corresponding to this buffer size
         * So the mediatime = currentPts in buffer - delayPTS
         * */
         position = position / 10;
         snd_pcm_avail_delay(pNvxAudio->playback_handle,&delayx,&delayy);
         val = (float)(delayy+delayx)/(float)pNvxAudio->nSamplingRate;
         delayPTS =  (OMX_TICKS)((val)*(float) 90000.0) ;
         if(position > delayPTS*10)
            position -= delayPTS*10;
         else
            position = 0;
         if (hClock)
         {
            if (pPortClock->bNvidiaTunneling)
            {
                NvxCCSetCurrentAudioRefClock(hClock, position);
                if (!pNvxAudio->bFirstAudioFrameUpdated)
                {
                    NvxSendEvent(pComponent,NVX_EventFirstAudioFramePlayed, 0, 0, 0);
                    pNvxAudio->bFirstAudioFrameUpdated = 1;
                }
            }
            else
            {
                OMX_TIME_CONFIG_TIMESTAMPTYPE timestamp;
                timestamp.nSize = sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE);
                timestamp.nVersion = pComponent->oSpecVersion;
                timestamp.nPortIndex = pPortClock->nTunnelPort;
                timestamp.nTimestamp = position;
                if (OMX_ErrorNone == OMX_SetConfig(hClock, OMX_IndexConfigTimeCurrentAudioReference, &timestamp))
                    pNvxAudio->lastposition = position / 100000;
            }
         }
    }

    return eError;
}

static OMX_ERRORTYPE NvxAlsaSinkAcquireResources(OMX_IN NvxComponent *pComponent)
{
    SNvxAlsaSinkData *pNvxAudio = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAlsaSinkAcquireResources\n"));

    pNvxAudio = (SNvxAlsaSinkData *)pComponent->pComponentData;
    NV_ASSERT(pNvxAudio);
    eError = NvxComponentBaseAcquireResources(pComponent);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pComponent->eObjectType, "ERROR:NvxAlsaSinkAcquireResources:"
           ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }
    eError = NvxAlsaSinkOpen(pNvxAudio, pComponent);

    return eError;
}

static OMX_ERRORTYPE NvxAlsaSinkReleaseResources(OMX_IN NvxComponent *pComponent)
{
    SNvxAlsaSinkData *pNvxAudio = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAlsaSinkReleaseResources\n"));
    pNvxAudio = (SNvxAlsaSinkData *)pComponent->pComponentData;
    NV_ASSERT(pNvxAudio);

    eError = NvxAlsaSinkClose(pNvxAudio);
    NvxCheckError(eError, NvxComponentBaseReleaseResources(pComponent));
    return eError;
}

static OMX_ERRORTYPE NvxAlsaSinkFlush(OMX_IN NvxComponent *pComponent, OMX_U32 nPort)
{
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE AvSetState(SNvxAlsaSinkData *pData,
                                      OMX_STATETYPE oNewState,
                                      OMX_STATETYPE oOldState)
{
    int start = 0, stop = 0, pause = 0;
    OMX_STATETYPE eState;


    eState = pData->eState;

    if (oNewState == OMX_StateExecuting &&
        (oOldState == OMX_StateIdle || oOldState == OMX_StatePause))
        start = 1;
    else if ((oNewState == OMX_StateIdle || oNewState == OMX_StateLoaded ||
              oNewState == OMX_StateInvalid) &&
             (oOldState == OMX_StateExecuting || oOldState == OMX_StatePause))
        stop = 1;
    else if (oNewState == OMX_StatePause && oOldState == OMX_StateExecuting)
        pause = 1;

    if (start && eState != OMX_StateExecuting)
    {
        pData->eState = OMX_StateExecuting;
        return OMX_ErrorNone;
    }

    if (stop && eState != OMX_StateIdle )
    {
           pData->eState = OMX_StateIdle;
        return OMX_ErrorNone;
    }

    if (pause && eState != OMX_StatePause)
    {
        pData->eState = OMX_StatePause;
        return OMX_ErrorNone;
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxAlsaSinkPreChangeState(NvxComponent *pNvComp,
                                                    OMX_STATETYPE oNewState)
{
    SNvxAlsaSinkData *pNvxAudio = 0;
    OMX_ERRORTYPE err;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioRendererPreChangeState\n"));
    pNvxAudio = (SNvxAlsaSinkData *)pNvComp->pComponentData;
    if (!pNvxAudio->bInitialized)
        return OMX_ErrorNone;

    err = AvSetState(pNvxAudio, oNewState, pNvComp->eState);
    if (OMX_ErrorNone != err)
        return err;
    return OMX_ErrorNone;
}


#define MIXER_SAMPLERATE    NVMM_AUDIO_IO_DEFAULT_SAMPLE_RATE
#define MIXER_BITSPERSAMPLE 16
#define MIXER_CHANNELS      2
#define MIXER_BLOCKALIGN    4
#define MIXER_CONTAINERSIZE 2
#define MAX_QUEUE_SIZE      10

static OMX_ERRORTYPE NvxAlsaSinkSetPlaybackRoute(char* pCard, NvAlsaDevice device)   /* For omxplayer audio bugID 857114*/
{
    snd_ctl_t *pHandle;
    snd_ctl_elem_info_t *pInfo;
    snd_ctl_elem_id_t *pId;
    snd_ctl_elem_value_t *pControl;
    snd_ctl_elem_type_t type;

    snd_ctl_elem_info_alloca (&pInfo);
    snd_ctl_elem_id_alloca (&pId);
    snd_ctl_elem_value_alloca (&pControl);

    snd_ctl_elem_id_set_interface (pId, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_id_set_name (pId, "Pcm Playback Route");

    if (snd_ctl_open (&pHandle, pCard, 0) < 0) {
        NvOsDebugPrintf("Control %s open error %\n", pCard);
        return OMX_ErrorComponentNotFound;
    }

  snd_ctl_elem_info_set_id (pInfo, pId);

  if (snd_ctl_elem_info (pHandle, pInfo) < 0) {
        NvOsDebugPrintf("Control %s cinfo error\n", pCard);
        return OMX_ErrorNotReady;
    }

    snd_ctl_elem_info_get_id (pInfo, pId);

  type = snd_ctl_elem_info_get_type (pInfo);
    snd_ctl_elem_value_set_id (pControl, pId);

    if (type == SND_CTL_ELEM_TYPE_INTEGER) {
    snd_ctl_elem_value_set_integer (pControl, 0, device);

        if (snd_ctl_elem_write (pHandle, pControl) < 0) {
            NvOsDebugPrintf("Control %s element write error\n", pCard);
            return OMX_ErrorUnsupportedSetting;
        }
    }

  snd_ctl_close (pHandle);

  return OMX_ErrorNone;
}

static OMX_ERRORTYPE AudioOutputVolumeInterfaceOpen(OMX_STRING pCardName)
{
    snd_mixer_selem_id_t *mixerId;
    OMX_S32 leftSwitch, rightSwitch;
    snd_mixer_elem_t * tempElem;

    /* Open mixer device */
    if (snd_mixer_open(&NvAlsaMixerData.hMixer, 0) != 0){
        NvOsDebugPrintf("%s: Opening Mixer Device Failed\n",__FUNCTION__);
        return OMX_ErrorComponentNotFound;
    }
    if (snd_mixer_attach(NvAlsaMixerData.hMixer,
                pCardName) != 0){
        NvOsDebugPrintf("%s: Mixer Attach Failed\n",__FUNCTION__);
        goto FAIL;
    }
    if (snd_mixer_selem_register(NvAlsaMixerData.hMixer, NULL, NULL) < 0){
        NvOsDebugPrintf("%s: Register Mixer Failed\n",__FUNCTION__);
        goto FAIL;
    }
    if (snd_mixer_load(NvAlsaMixerData.hMixer) < 0){
        NvOsDebugPrintf("%s: Load Mixer Failed\n",__FUNCTION__);
        goto FAIL;
    }
    /* Obtain mixer element */
    snd_mixer_selem_id_alloca(&mixerId);

    /* Check if Headphone is ON */
    snd_mixer_selem_id_set_name(mixerId, MIXER_HEADPHONE_ID_NAME);
    tempElem = snd_mixer_find_selem(NvAlsaMixerData.hMixer, mixerId);

    if(!tempElem)
        goto FAIL;

    if((snd_mixer_selem_get_playback_switch(tempElem,
        SND_MIXER_SCHN_FRONT_LEFT,(int *)&leftSwitch) !=0) ||
        (snd_mixer_selem_get_playback_switch(tempElem,
        SND_MIXER_SCHN_FRONT_RIGHT,(int *)&rightSwitch) !=0))
    {
        NvOsDebugPrintf("%s: snd_mixer_selem_get_playback_switch : Headphone Failed\n",__FUNCTION__);
    }
    else
    {
        if(leftSwitch && rightSwitch)
        {
            NvAlsaMixerData.mixelem = tempElem;
        }
        else
        {
            snd_mixer_selem_id_set_name(mixerId, MIXER_SPEAKER_ID_NAME);
            tempElem = snd_mixer_find_selem(NvAlsaMixerData.hMixer, mixerId);
            if((snd_mixer_selem_get_playback_switch(tempElem,
                SND_MIXER_SCHN_FRONT_LEFT,(int *)&leftSwitch) !=0) ||
                (snd_mixer_selem_get_playback_switch(tempElem,
                SND_MIXER_SCHN_FRONT_RIGHT,(int *)&rightSwitch) !=0))
            {
                NvOsDebugPrintf("%s: snd_mixer_selem_get_playback_switch : Speaker Failed\n",__FUNCTION__);
            }
            else
            {
                if(leftSwitch && rightSwitch)
                {
                    NvAlsaMixerData.mixelem = tempElem;
                    NvxAlsaSinkSetPlaybackRoute(pCardName, NvAlsaDevice_Speaker);
                }
                else
                {
                    NvOsDebugPrintf("%s: Both Headphone and Speaker Switch OFF\n",__FUNCTION__);
                    goto FAIL;
                }
            }

        }
    }

    if (NvAlsaMixerData.mixelem)
    {
        /* Obtain linear integer volume range boundaries */
        if (snd_mixer_selem_get_playback_volume_range(
            NvAlsaMixerData.mixelem,
            &NvAlsaMixerData.minVol,
            &NvAlsaMixerData.maxVol) != 0)
        {
            NvOsDebugPrintf("%s: snd_mixer_selem_get_playback_volume_range Failed\n",
                __FUNCTION__);
            return OMX_ErrorNotReady;
        }
        /* Obtain non-linear (millibel) volume range boundaries */
        if (snd_mixer_selem_get_playback_dB_range(
            NvAlsaMixerData.mixelem,
            &NvAlsaMixerData.minVoldB,
            &NvAlsaMixerData.maxVoldB) != 0)
        {
            NvOsDebugPrintf("%s: snd_mixer_selem_get_playback_dB_range Failed\n",
                __FUNCTION__);
            return OMX_ErrorNotReady;
        }
        if ((NvAlsaMixerData.minVol >= NvAlsaMixerData.maxVol) ||
            (NvAlsaMixerData.minVoldB >= NvAlsaMixerData.maxVoldB))
        {
            NvOsDebugPrintf("%s: minVol >= maxVol\n",__FUNCTION__);
            return OMX_ErrorUndefined;
        }
        /* Allow other applications to change mixer */
        snd_mixer_handle_events(NvAlsaMixerData.hMixer);
        return OMX_ErrorNone;
    }
    NvOsDebugPrintf("%s: Acquire Mixer Element Failed\n",__FUNCTION__);
FAIL:
    snd_mixer_close(NvAlsaMixerData.hMixer);
    NvAlsaMixerData.hMixer = NULL;
    return OMX_ErrorUndefined;
}

static OMX_ERRORTYPE NvxAlsaSinkOpenDevice(SNvxAlsaSinkData * pNvxAudio)
{
    int deviceId = 0;
    OMX_STRING pCardName = "default";
    OMX_ERRORTYPE mixerErr;

    pNvxAudio->playback_handle = NULL;
    switch(pNvxAudio->eOutputType)
    {
        case NVX_AUDIO_OutputHdmi : pCardName = "tegra_hdmi"; deviceId = 0; break;
        case NVX_AUDIO_OutputSpdif: pCardName = "tegra_spdif"; deviceId = 1; break;
        case NVX_AUDIO_OutputIEC:   pCardName = "tegra_iec"; deviceId = 2; break;
        default :
            break;
    }

    // Following change is to support HDMI Video and alsa on non HDMI port
    // till alsa support is added for HDMI audio output.
    // Refer to Bug 692128
    if (pNvxAudio->eOutputType == NVX_AUDIO_OutputHdmi){
        pCardName = "default";
    }


    if (!pNvxAudio->pNativeRender)
    {
        if(snd_pcm_open(&pNvxAudio->playback_handle, \
                    pCardName, SND_PCM_STREAM_PLAYBACK,SND_PCM_NONBLOCK)<0)
        {
            NvOsDebugPrintf("%s: open alsa audio device failed\n",__FUNCTION__);
            return OMX_ErrorHardware;
        }
        NvxAlsaSinkSetPlaybackRoute(pCardName, NvAlsaDevice_Headphone); /* Added for omxplayer audio bugID 857114 */
        mixerErr = AudioOutputVolumeInterfaceOpen(pCardName);
        if (mixerErr != OMX_ErrorNone)
        {
            NvOsDebugPrintf("%s: Open Volume Interface Failed\n",__FUNCTION__);
        }
    }
    else
    {
        printf("Using Native Audio Renderer \n");
        pNvxAudio->playback_handle = pNvxAudio->pNativeRender;
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxAlsaSinkOpen(SNvxAlsaSinkData * pNvxAudio, NvxComponent *pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortClock = &(pComponent->pPorts[s_nvx_port_clock]);
    NVXTRACE(( NVXT_CALLGRAPH, NVXT_AUDIO_RENDERER, "+NvxAlsaSinkOpen\n"));

    if((eError=NvxAlsaSinkOpenDevice(pNvxAudio)) != OMX_ErrorNone)
    {
        return eError;
    }

    if(snd_pcm_hw_params_malloc(&pNvxAudio->hw_params)<0)
    {
        NvOsDebugPrintf("%s: Failed allocating hw params\n",__FUNCTION__);
        return OMX_ErrorHardware;
    }

    if(snd_pcm_hw_params_any(pNvxAudio->playback_handle,pNvxAudio->hw_params) < 0)
    {
        NvOsDebugPrintf("%s: Failed initialize hardware params\n",__FUNCTION__);
        return OMX_ErrorHardware;
    }

    if (pPortClock->hTunnelComponent)
    {
        pNvxAudio->nUpdateCount = 0;
        NvxCCSetWaitForAudio(pPortClock->hTunnelComponent);
    }
    pNvxAudio->bInitialized = OMX_TRUE;
    pNvxAudio->bFirstRun = OMX_TRUE;
    pNvxAudio->bHaveEndBuffer = OMX_FALSE;
    pNvxAudio->bFirstAudioFrameUpdated = 0;
    pNvxAudio->bConfigured = OMX_FALSE;
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxAlsaSinkClose(SNvxAlsaSinkData *pNvxAudio)
{
    int deviceId = 0;
    OMX_STRING pCardName = "default";
    NVXTRACE((NVXT_CALLGRAPH, NVXT_AUDIO_RENDERER, "+NvxAlsaSinkClose\n"));

    if (!pNvxAudio->bInitialized)
      return OMX_ErrorNone;

    switch(pNvxAudio->eOutputType)
    {
        case NVX_AUDIO_OutputHdmi : pCardName = "tegra_hdmi"; deviceId = 0; break;
        case NVX_AUDIO_OutputSpdif: pCardName = "tegra_spdif"; deviceId = 1; break;
        case NVX_AUDIO_OutputIEC:   pCardName = "tegra_iec"; deviceId = 2; break;
        default :
          break;
    }

    // Following change is to support HDMI Video and alsa on non HDMI port
    // till alsa support is added for HDMI audio output.
    // Refer to Bug 692128
    if (pNvxAudio->eOutputType == NVX_AUDIO_OutputHdmi) {
        pCardName = "default";
    }

    if(pNvxAudio->hw_params)
    {
        snd_pcm_hw_params_free(pNvxAudio->hw_params);
    }

    if (pNvxAudio->pNativeRender)
    {
        pNvxAudio->playback_handle = NULL;
    }
    else
    {
        if(pNvxAudio->playback_handle)
        {
            snd_pcm_close(pNvxAudio->playback_handle);
        }

            NvxAlsaSinkSetPlaybackRoute (pCardName, NvAlsaDevice_None); /* Added for omxplayer audio bug 857114*/
    }
    if (NvAlsaMixerData.hMixer != NULL)
        snd_mixer_close(NvAlsaMixerData.hMixer);
    pNvxAudio->bInitialized = OMX_FALSE;
    pNvxAudio->bConfigured = OMX_FALSE;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxAudioRendererInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    NvxComponent *pThis = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_AUDIO_PARAM_PCMMODETYPE  *pNvxPcmParameters;
    SNvxAlsaSinkData *data = NULL;
    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxAlsaSinkInit\n"));

    eError = NvxComponentCreate(hComponent, s_nvx_num_ports, &pThis);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType,"ERROR:NvxAlsaSinkInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pThis->eObjectType = NVXT_AUDIO_RENDERER;
    data = NvOsAlloc(sizeof(SNvxAlsaSinkData));
    if (!data)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType,"ERROR:NvxAlsaSinkInit:"
            ":SNvxAudioData = %d:[%s(%d)]\n",data,__FILE__, __LINE__));
        return OMX_ErrorInsufficientResources;
    }
    NvOsMemset(data, 0, sizeof(SNvxAlsaSinkData));
    pThis->pComponentData = data;
    pThis->pComponentName = "OMX.Nvidia.audio.renderer";
    pThis->nComponentRoles = 1;
    pThis->sComponentRoles[0] = "audio_renderer.pcm";

    pThis->GetParameter       = NvxAlsaSinkGetParameter;
    pThis->SetParameter       = NvxAlsaSinkSetParameter;
    pThis->GetConfig          = NvxAlsaSinkGetConfig;
    pThis->SetConfig          = NvxAlsaSinkSetConfig;
    pThis->WorkerFunction     = NvxAlsaSinkWorkerFunction;
    pThis->AcquireResources   = NvxAlsaSinkAcquireResources;
    pThis->ReleaseResources   = NvxAlsaSinkReleaseResources;
    pThis->PreChangeState     = NvxAlsaSinkPreChangeState;
    pThis->DeInit             = NvxAlsaSinkDeInit;
    pThis->Flush              = NvxAlsaSinkFlush;

    pThis->pPorts[s_nvx_port_input].oPortDef.nPortIndex = s_nvx_port_input;

    NvxPortInitAudio(&pThis->pPorts[s_nvx_port_input], OMX_DirInput, MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE, OMX_AUDIO_CodingPCM);

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

    NvxPortInitOther(&pThis->pPorts[s_nvx_port_clock], OMX_DirInput, 4, sizeof(OMX_TIME_MEDIATIMETYPE), OMX_OTHER_FormatTime);
    pThis->pPorts[s_nvx_port_clock].eNvidiaTunnelTransaction = ENVX_TRANSACTION_CLOCK;

    return eError;
}
