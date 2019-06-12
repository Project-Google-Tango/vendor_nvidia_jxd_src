/*
 * Copyright (c) 2006 NVIDIA Corporation.  All Rights Reserved.
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
#include "NvxTrace.h"

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */

// DEBUG defines. TO DO: fix this
#define MAX_INPUT_BUFFERS    10
#define MIN_INPUT_BUFFERSIZE 100*1024
#define OMX_INVALID_GRAPHICEQ_GAIN ((NvS32)0xffffffff)
/* Number of internal frame buffers (AUDIO memory) */
static const int s_nvx_num_ports            = 2;
static const int s_nvx_port_input           = 0;
static const int s_nvx_port_clock           = 1;

enum EqChannel
{
    EQ_CHANNEL_LEFT = 0,
    EQ_CHANNEL_RIGHT,
};

enum { s_DRC_CLPPINGTH = 15000 };
enum { s_DRC_LOWERCOMPTH = 6000 };
enum { s_DRC_UPPERCOMPTH = 12000 };
enum { s_DRC_NOISEGATETH = 500 };
enum { s_MIXER_UNITY_GAIN = 256 };


/* Audio Renderer State information */
typedef struct SNvxAudioRendererData
{
    OMX_BOOL bInitialized;
    
    NvMMBlockType oBlockType;
    
    SNvxNvMMTransformData oMixer;

    OMX_BOOL bTunneling;
    OMX_BOOL bHaveEndBuffer;
    NvOsSemaphoreHandle eosWaitSem;

    OMX_TICKS lastposition;

    NvU32 mixer_input;
    NvU32 mixer_output;

    OMX_BOOL bFirstRun;

    int volume;
    int pan;
    int mute;

    //NvS32 dbGain[EQBANDS]; //deprecated single channel Graphic EQ Gain settings
    OMX_BOOL bGotEQ;

    NVX_AUDIO_OUTPUTTYPE eOutputType;

    OMX_BOOL bDisableUpdates;
    OMX_U32 nUpdateCount;

    NVX_CONFIG_AUDIO_DRCPROPERTY oDrcProperty;
    NVX_CONFIG_AUDIO_PARAEQFILTER oParaEqFilter;
    NVX_CONFIG_AUDIO_GRAPHICEQ oGraphicEqProp;

    OMX_S32 bFirstAudioFrameUpdated;
} SNvxAudioRendererData;

static OMX_ERRORTYPE NvxAudioRendererOpen(SNvxAudioRendererData * pNvxAudio, NvxComponent *pComponent);
static OMX_ERRORTYPE NvxAudioRendererClose(SNvxAudioRendererData *pNvxAudio);
static OMX_ERRORTYPE NvxAudioRendererPreChangeState(NvxComponent *pNvComp, 
                                                    OMX_STATETYPE oNewState);

static int sEqBandFreqs[EQBANDS] = { 100, 230, 529, 1217, 2798 };

static void SetEQ(SNvxAudioRendererData *pNvxAudio)
{
    NvMMMixerStream5BandEQProperty eq;

    if (!pNvxAudio->bInitialized)
        return;

    eq.structSize = sizeof(NvMMMixerStream5BandEQProperty);
    NvOsMemcpy(&eq.dBGain[EQ_CHANNEL_LEFT][0], &pNvxAudio->oGraphicEqProp.dBGain[EQ_CHANNEL_LEFT][0], sizeof(NvS32) * EQBANDS);
    NvOsMemcpy(&eq.dBGain[EQ_CHANNEL_RIGHT][0], &pNvxAudio->oGraphicEqProp.dBGain[EQ_CHANNEL_RIGHT][0], sizeof(NvS32) * EQBANDS);

    pNvxAudio->oMixer.hBlock->SetAttribute(pNvxAudio->oMixer.hBlock,
                                           NvMMMixerAttribute_EQ, 0,
                                           eq.structSize, &eq);
}

static void GetEQ(SNvxAudioRendererData *pNvxAudio)
{
    NvMMMixerStream5BandEQProperty eq;

    if (!pNvxAudio->bInitialized)
        return;

    eq.structSize = sizeof(NvMMMixerStream5BandEQProperty);

    pNvxAudio->oMixer.hBlock->GetAttribute(pNvxAudio->oMixer.hBlock,
                                           NvMMMixerAttribute_EQ,
                                           eq.structSize, &eq);

    NvOsMemcpy(&pNvxAudio->oGraphicEqProp.dBGain[EQ_CHANNEL_LEFT][0], &eq.dBGain[EQ_CHANNEL_LEFT][0], sizeof(NvS32) * EQBANDS);
    NvOsMemcpy(&pNvxAudio->oGraphicEqProp.dBGain[EQ_CHANNEL_RIGHT][0], &eq.dBGain[EQ_CHANNEL_RIGHT][0], sizeof(NvS32) * EQBANDS);
}

static void SetVolume(SNvxAudioRendererData *pNvxAudio)
{
    NvMMMixerStreamSetVolumePanMuteProperty vpm;

    if (!pNvxAudio->bInitialized)
        return;

    vpm.structSize = sizeof(NvMMMixerStreamSetVolumePanMuteProperty);
    vpm.StreamIndex = pNvxAudio->mixer_input;
    vpm.volPanMute.Volume = pNvxAudio->volume;
    vpm.volPanMute.Pan = pNvxAudio->pan;
    vpm.volPanMute.Mute = pNvxAudio->mute;

    pNvxAudio->oMixer.hBlock->SetAttribute(pNvxAudio->oMixer.hBlock,
                                           NvMMMixerAttribute_Volume, 0,
                                           vpm.structSize, &vpm);
}

static void SetTSUpdates(SNvxNvMMTransformData *pData, OMX_BOOL bUpdate)
{
    NvMMMixerSendTimeStampEvents tse;

    tse.SendTimeStamps = bUpdate;
    tse.TimeStampFrequency = 10;

    pData->hBlock->SetAttribute(
            pData->hBlock, 
            NvMMMixerAttribute_SendTimeStampEvents, 0, 
            sizeof(NvMMMixerSendTimeStampEvents), &tse);
}

static void SetDrcProperty(SNvxAudioRendererData *pNvxAudio)
{
    NvMMMixerStreamDrcProperty oDrcProperty;

    if (!pNvxAudio->bInitialized)
        return;

    oDrcProperty.structSize = sizeof(NvMMMixerStreamDrcProperty);
    oDrcProperty.EnableDrc = pNvxAudio->oDrcProperty.EnableDrc;
    oDrcProperty.ClippingTh = pNvxAudio->oDrcProperty.ClippingTh;
    oDrcProperty.LowerCompTh = pNvxAudio->oDrcProperty.LowerCompTh;
    oDrcProperty.UpperCompTh = pNvxAudio->oDrcProperty.UpperCompTh;
    oDrcProperty.NoiseGateTh = pNvxAudio->oDrcProperty.NoiseGateTh;

    pNvxAudio->oMixer.hBlock->SetAttribute(pNvxAudio->oMixer.hBlock,
                                           NvMMMixerAttribute_DRC, 0,
                                           sizeof(NvMMMixerStreamDrcProperty), 
                                           &oDrcProperty);
    return;
}

static void SetParaEqFilter(SNvxAudioRendererData *pNvxAudio)
{
    NvMMMixerStreamParaEQFilterProperty oParaEqFilterProperty;

    if (!pNvxAudio->bInitialized)
        return;

    oParaEqFilterProperty.structSize = sizeof(NvMMMixerStreamParaEQFilterProperty);
    oParaEqFilterProperty.bEnable = pNvxAudio->oParaEqFilter.bEnable;
    oParaEqFilterProperty.Direction = NvMMMixerStream_Input;

    if (oParaEqFilterProperty.bEnable)
    {
        int k;
        oParaEqFilterProperty.bUpdate = NV_TRUE;
        for (k = 0; k < MIXER_NUM_PARA_EQ_FILTERS; k++)
        {
            oParaEqFilterProperty.FilterType[k] = pNvxAudio->oParaEqFilter.FilterType[k];
            oParaEqFilterProperty.F0[k]         = pNvxAudio->oParaEqFilter.F0[k];
            oParaEqFilterProperty.BndWdth[k]    = pNvxAudio->oParaEqFilter.BndWdth[k];

            if(pNvxAudio->oParaEqFilter.dBGain[k] > 1200) 
                oParaEqFilterProperty.dBGain[k] = 1200;
            else if (pNvxAudio->oParaEqFilter.dBGain[k] < -1800) 
                oParaEqFilterProperty.dBGain[k] = -1800;
            else
                oParaEqFilterProperty.dBGain[k] = pNvxAudio->oParaEqFilter.dBGain[k];
        }
    }
    else
    {
        int k;
        for (k = 0; k < MIXER_NUM_PARA_EQ_FILTERS; k++)
        {
            oParaEqFilterProperty.FilterType[k] = 1;    // bandpass
            oParaEqFilterProperty.F0[k] = 1000;
            oParaEqFilterProperty.BndWdth[k] = 500;
            oParaEqFilterProperty.dBGain[k] = 0;
        }
    }

    pNvxAudio->oMixer.hBlock->SetAttribute(pNvxAudio->oMixer.hBlock,
                                           NvMMMixerAttribute_ParaEQFilter, 0,
                                           sizeof(NvMMMixerStreamParaEQFilterProperty), 
                                           &oParaEqFilterProperty);
    return;
}

static OMX_ERRORTYPE NvxAudioRendererDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioRendererInit\n"));

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
                NvxNvMMTransformSetupAudioParams(&pNvxAudio->oMixer, pNvxAudio->mixer_input, pLocalMode->nSamplingRate, pLocalMode->nBitPerSample, pLocalMode->nChannels);
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
    OMX_AUDIO_CONFIG_BALANCETYPE *bt;
    OMX_AUDIO_CONFIG_STEREOWIDENINGTYPE *wt;

    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioRenderGetConfig\n"));

    data = (SNvxAudioRendererData *)pNvComp->pComponentData;

    switch ( nIndex )
    {
        case NVX_IndexConfigGetNVMMBlock:
        {
            NVX_CONFIG_GETNVMMBLOCK *pGetBlock = (NVX_CONFIG_GETNVMMBLOCK *)pComponentConfigStructure;

            if (!data || !data->bInitialized)
                return OMX_ErrorNotReady;

            if (pGetBlock->nPortIndex != (NvU32)s_nvx_port_input)
                return OMX_ErrorBadParameter;

            pGetBlock->pNvMMTransform = &data->oMixer;
            pGetBlock->nNvMMPort = data->mixer_input;
            eError = OMX_ErrorNone;
            break;
        }
        case  NVX_IndexConfigAudioOutputType:
        {
            NVX_CONFIG_AUDIOOUTPUT *ot = (NVX_CONFIG_AUDIOOUTPUT *)pComponentConfigStructure;
            ot->eOutputType = data->eOutputType;
            break;
        }
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

            vt->bLinear = 1;
            if (vt->bLinear) 
            {
                vt->sVolume.nMin = 0;
                vt->sVolume.nMax = s_MIXER_UNITY_GAIN;
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
            mt->bMute = data->mute;
            break;
        }
        case OMX_IndexConfigAudioBalance:
        {
            bt = (OMX_AUDIO_CONFIG_BALANCETYPE *)pComponentConfigStructure;
            if (bt->nPortIndex != 0)
            {
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxAudioRenderGetConfig:"
                   ":nPortIndex = %d:[%s(%d)]\n",bt->nPortIndex  ,__FILE__, __LINE__));
                return OMX_ErrorBadParameter;
            }
            bt->nBalance = data->pan;
            break;
        }
        case OMX_IndexConfigAudioStereoWidening:
        {
            wt = (OMX_AUDIO_CONFIG_STEREOWIDENINGTYPE *)pComponentConfigStructure;
            if (wt->nPortIndex != 0)
            {
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxAudioRenderGetConfig:"
                   ":nPortIndex = %d:[%s(%d)]\n",wt->nPortIndex  ,__FILE__, __LINE__));
                return OMX_ErrorBadParameter;
            }
            wt->eWideningType = OMX_AUDIO_StereoWideningHeadphones;
            //NvxAudioGetStereoWiden(&data->oBase, &wt->nStereoWidening);
            wt->bEnable = (wt->nStereoWidening > 0);
            break;
        }
        case OMX_IndexConfigAudioEqualizer:
        {
            OMX_AUDIO_CONFIG_EQUALIZERTYPE *eq = (OMX_AUDIO_CONFIG_EQUALIZERTYPE *)pComponentConfigStructure;
            OMX_U32 nBand;

            GetEQ(data);

            eq->sBandIndex.nMin = 0;
            eq->sBandIndex.nMax = EQBANDS - 1;

            nBand = eq->sBandIndex.nValue;
            if (nBand >= EQBANDS)
                return OMX_ErrorBadParameter;

            eq->sCenterFreq.nValue = sEqBandFreqs[nBand];
            eq->sBandLevel.nMin = -1200;
            eq->sBandLevel.nMax = 1200;
            eq->sBandLevel.nValue = (50-data->oGraphicEqProp.dBGain[0][nBand]) * 24;//Setting only left channel dBgain to keep 
  
            break;
        }
        case NVX_IndexConfigUlpMode:
        {
            return OMX_ErrorBadParameter;
        }
        case NVX_IndexConfigDisableTimestampUpdates:
        {
            NVX_CONFIG_DISABLETSUPDATES *pConfig = (NVX_CONFIG_DISABLETSUPDATES *)pComponentConfigStructure;
            //Enable ULP mode for the component
            pConfig->bDisableTSUpdates = data->bDisableUpdates;
            break;
        }
        case NvxIndexConfigTracklist:
        {
            eError = OMX_ErrorNotImplemented;
            break;
        }
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
    OMX_AUDIO_CONFIG_BALANCETYPE *bt;
    OMX_AUDIO_CONFIG_STEREOWIDENINGTYPE *wt;
    
    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioRendererSetConfig\n"));     

    pNvxAudio = (SNvxAudioRendererData *)pNvComp->pComponentData;
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
        case OMX_IndexConfigAudioBalance:
        {
            bt = (OMX_AUDIO_CONFIG_BALANCETYPE *)pComponentConfigStructure;
            if (bt->nPortIndex != 0)
            {
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxAudioRendererSetConfig:"
                   ":nPortIndex = %d:[%s(%d)]\n",bt->nPortIndex ,__FILE__, __LINE__));
                return OMX_ErrorBadParameter;
            }
            pNvxAudio->pan = bt->nBalance;
            SetVolume(pNvxAudio);
            break;
        }
        case OMX_IndexConfigAudioStereoWidening:
        {
            wt = (OMX_AUDIO_CONFIG_STEREOWIDENINGTYPE *)pComponentConfigStructure;
            if (wt->nPortIndex != 0)
            {
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxAudioRendererSetConfig:"
                   ":nPortIndex = %d:[%s(%d)]\n",wt->nPortIndex ,__FILE__, __LINE__));
                return OMX_ErrorBadParameter;
            }
            //if (wt->bEnable)
            //    NvxAudioSetStereoWiden(&pNvxAudio->oBase, wt->nStereoWidening);
            //else
            //    NvxAudioSetStereoWiden(&pNvxAudio->oBase, 0);
            break;
        }
        case OMX_IndexConfigAudioEqualizer:
        {
            OMX_AUDIO_CONFIG_EQUALIZERTYPE *eq = (OMX_AUDIO_CONFIG_EQUALIZERTYPE *)pComponentConfigStructure;
            OMX_U32 nBand;

            nBand = eq->sBandIndex.nValue;

            if (nBand >= EQBANDS)
                return OMX_ErrorBadParameter;

            if (eq->sBandLevel.nValue < -1200 || eq->sBandLevel.nValue > 1200)
                return OMX_ErrorBadParameter;

            //setting same value to left and right channel to keep OMX_IndexConfigAudioEqualizer case working
            pNvxAudio->oGraphicEqProp.dBGain[0][nBand] = 50-(int)(eq->sBandLevel.nValue / 24);
            pNvxAudio->oGraphicEqProp.dBGain[1][nBand] = 50-(int)(eq->sBandLevel.nValue / 24);
            SetEQ(pNvxAudio);
 
            break;
        }
        case NVX_IndexConfigUlpMode:
        {
            return OMX_ErrorBadParameter;
        }
        case NVX_IndexConfigDisableTimestampUpdates:
        {
            NVX_CONFIG_DISABLETSUPDATES *pConfig = (NVX_CONFIG_DISABLETSUPDATES *)pComponentConfigStructure;
            if (pNvxAudio->bDisableUpdates && !pConfig->bDisableTSUpdates)
            {
                SetTSUpdates(&pNvxAudio->oMixer, OMX_TRUE);
                pNvxAudio->nUpdateCount = 0;
            }
            pNvxAudio->bDisableUpdates = pConfig->bDisableTSUpdates;
            break;
        }
        case NvxIndexConfigTracklist:
        {
            eError = OMX_ErrorNotImplemented;
            break;
        }
        case NVX_IndexConfigAudioDrcProperty:
        {
            NVX_CONFIG_AUDIO_DRCPROPERTY *pDrcProp = (NVX_CONFIG_AUDIO_DRCPROPERTY *)pComponentConfigStructure;

            NvOsMemcpy(&pNvxAudio->oDrcProperty, pDrcProp, sizeof(NVX_CONFIG_AUDIO_DRCPROPERTY));
            SetDrcProperty(pNvxAudio);
            break;
        }
        case NVX_IndexConfigAudioParaEqFilter:
        {
            NVX_CONFIG_AUDIO_PARAEQFILTER *pParaEqFilter = (NVX_CONFIG_AUDIO_PARAEQFILTER *)pComponentConfigStructure;

            NvOsMemcpy(&pNvxAudio->oParaEqFilter, pParaEqFilter, sizeof(NVX_CONFIG_AUDIO_PARAEQFILTER));
            SetParaEqFilter(pNvxAudio);
            break;
        }
        case NVX_IndexConfigAudioGraphicEq:
        {
            NVX_CONFIG_AUDIO_GRAPHICEQ *pGraphicEqProp = (NVX_CONFIG_AUDIO_GRAPHICEQ *)pComponentConfigStructure;
            OMX_S32 i,ch; 

            NvOsMemcpy(&pNvxAudio->oGraphicEqProp, pGraphicEqProp, sizeof(NVX_CONFIG_AUDIO_GRAPHICEQ));
            for(ch = 0; ch < EQ_NO_CHANNEL; ch++)
            {
                for(i = 0; i < EQBANDS; i++)
                    pNvxAudio->oGraphicEqProp.dBGain[ch][i] = 50-(int)(pNvxAudio->oGraphicEqProp.dBGain[ch][i] / 24);
            }

            SetEQ(pNvxAudio);
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
    OMX_TICKS position = 0;
    OMX_BOOL bWouldFail = OMX_TRUE;
    OMX_BOOL bSetPosition = OMX_FALSE;

    NVXTRACE((NVXT_WORKER, pComponent->eObjectType, "+NvxAudioRendererWorkerFunction\n"));

    pNvxAudio = (SNvxAudioRendererData *)pComponent->pComponentData;

    *pbMoreWork = (pPortIn->pCurrentBufferHdr != NULL);

    if (pPortClock && pPortClock->pCurrentBufferHdr)
    {
        NvxPortReleaseBuffer(pPortClock, pPortClock->pCurrentBufferHdr);
    }

    if (!pPortIn->pCurrentBufferHdr)
        return OMX_ErrorNone;

    if (pNvxAudio->bTunneling)
    {
        if (pPortIn->pCurrentBufferHdr->nTimeStamp > 0)
        {
            position = pPortIn->pCurrentBufferHdr->nTimeStamp;
            bSetPosition = OMX_TRUE;
        }

        if (pPortIn->pCurrentBufferHdr->nFlags & OMX_BUFFERFLAG_EOS)
        {
            pNvxAudio->bHaveEndBuffer = OMX_FALSE;
        }

        NvxPortReleaseBuffer(pPortIn, pPortIn->pCurrentBufferHdr);
    }
    else
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

                    NvxNvMMTransformSetupAudioParams(&pNvxAudio->oMixer, pNvxAudio->mixer_input, pLocalMode->nSamplingRate, pLocalMode->nBitPerSample, pLocalMode->nChannels);
                    pNvxAudio->bFirstRun = OMX_FALSE;
                }
            }
            else
                pNvxAudio->bFirstRun = OMX_FALSE;
        }

        if (pPortIn->pCurrentBufferHdr->nFlags & OMX_BUFFERFLAG_EOS)
        {
            pNvxAudio->bHaveEndBuffer = OMX_TRUE;
            bQueryPosition = OMX_FALSE;
        }

        eError = NvxNvMMTransformWorkerBase(&pNvxAudio->oMixer, pNvxAudio->mixer_input, 
                                            &bWouldFail);

        if (bQueryPosition)
        {
            position = NvxNvMMTransformGetMediaTime(&pNvxAudio->oMixer, 
                                                    pNvxAudio->mixer_input);
            bSetPosition = OMX_TRUE;
        }

        if (!pPortIn->pCurrentBufferHdr) 
            NvxPortGetNextHdr(pPortIn);

        *pbMoreWork = (pPortIn->pCurrentBufferHdr && !bWouldFail);
    }

    if (pPortClock && position / 100000 != pNvxAudio->lastposition && bSetPosition &&
        position != (OMX_TICKS)-1 && pNvxAudio->bDisableUpdates)
    {
        OMX_HANDLETYPE hClock = pPortClock->hTunnelComponent;
        OMX_TICKS audio_ts = position;

        if (hClock) 
        {
            if (pPortClock->bNvidiaTunneling)
            {
                NvxCCSetCurrentAudioRefClock(hClock, audio_ts);
                pNvxAudio->lastposition = position / 100000;
            }
            else
            {
                OMX_TIME_CONFIG_TIMESTAMPTYPE timestamp;
                timestamp.nSize = sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE);
                timestamp.nVersion = pComponent->oSpecVersion;
                timestamp.nPortIndex = pPortClock->nTunnelPort;
                timestamp.nTimestamp = audio_ts;

                if (OMX_ErrorNone == OMX_SetConfig(hClock, OMX_IndexConfigTimeCurrentAudioReference, &timestamp))
                    pNvxAudio->lastposition = position / 100000;
            }
        }
    }

    return eError;
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

    eError = NvxAudioRendererOpen(pNvxAudio, pComponent); 

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

    eError = NvxAudioRendererClose(pNvxAudio); 
    NvxCheckError(eError, NvxComponentBaseReleaseResources(pComponent));
    return eError;
}

static OMX_ERRORTYPE NvxAudioRendererFlush(OMX_IN NvxComponent *pComponent, OMX_U32 nPort)
{
    SNvxAudioRendererData *pNvxAudio;
    NvMMMixerStreamState in, out;
    NvMMBlockHandle hNvMMBlock;
    NvMMState eState = NvMMState_Paused;

    NV_ASSERT(pComponent && pComponent->pComponentData);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAudioRendererFlush\n"));
    pNvxAudio = (SNvxAudioRendererData *)pComponent->pComponentData;

    hNvMMBlock = pNvxAudio->oMixer.hBlock;
    hNvMMBlock->GetState(hNvMMBlock, &eState);
    NvxMutexUnlock(pComponent->hWorkerMutex);
    if (eState == NvMMState_Paused)
    {
        in.pin = pNvxAudio->mixer_input;
        in.newState = NvMMState_Stopped;

        hNvMMBlock->Extension(hNvMMBlock, NvMMAudioExtension_SetState, 
                              sizeof(NvMMMixerStreamState), &in,
                              sizeof(NvMMMixerStreamState), &out);

        in.newState = NvMMState_Paused;

        hNvMMBlock->Extension(hNvMMBlock, NvMMAudioExtension_SetState, 
                              sizeof(NvMMMixerStreamState), &in,
                              sizeof(NvMMMixerStreamState), &out);

    }
    NvxMutexLock(pComponent->hWorkerMutex);
    SetTSUpdates(&pNvxAudio->oMixer, OMX_TRUE);
    pNvxAudio->nUpdateCount = 0;
    pNvxAudio->bFirstAudioFrameUpdated = 0;

    return NvxNvMMTransformFlush(&pNvxAudio->oMixer, nPort);
}

/* =========================================================================
 *                     I N T E R N A L
 * ========================================================================= */
#define MIXER_SAMPLERATE    NVMM_AUDIO_IO_DEFAULT_SAMPLE_RATE
#define MIXER_BITSPERSAMPLE 16
#define MIXER_CHANNELS      2
#define MIXER_BLOCKALIGN    4
#define MIXER_CONTAINERSIZE 2
#define MAX_QUEUE_SIZE      10

static NvError AvInitializeMixerPin(NvMMMixerStreamProperty * pMixerProperty,
                                    NvMMMixerOutputFormat *pFormat)
{
    pMixerProperty->structSize = sizeof(NvMMMixerStreamProperty);
    pMixerProperty->SampleRate = pFormat->SampleRate;
    pMixerProperty->BitsPerSample = pFormat->BitsPerSample;
    pMixerProperty->ContainerSize = MIXER_CONTAINERSIZE;
    pMixerProperty->BlockAlign = MIXER_BLOCKALIGN;
    pMixerProperty->FrameSize = 0;  // TODO: not sure
    pMixerProperty->Channels = pFormat->Channels;
    pMixerProperty->QueueSize = 10;
    pMixerProperty->BufferCount = 10;

    return NvSuccess;
}

static NvError AvGetMixerPin(NvMMBlockHandle hBlock, NvMMAudioPinType pinType,
                             NvMMAudioPinDirection pinDirection, NvU32 SampleRate,
                             NvU32 BitsPerSample, NvU32 Channels,
                             NVX_AUDIO_OUTPUTTYPE eOutput, NvU32 *pin)
{
    NvMMMixerGetPin gpi, gpo;
    NvMMMixerOutputFormat format;

    NvOsMemset(&gpi, 0, sizeof(NvMMMixerGetPin));
    NvOsMemset(&gpo, 0, sizeof(NvMMMixerGetPin));

    // Setup the pin format
    format.SampleRate = SampleRate;
    format.BitsPerSample = BitsPerSample;
    format.Channels = Channels;
    AvInitializeMixerPin(&gpi.mixerProperty, &format);

    // Get the pin
    gpi.pinType = pinType;
    gpi.pinDirection = pinDirection;
    switch (eOutput)
    {
        case NVX_AUDIO_OutputHdmi:
            gpi.pinPort = NvMMAudioMixerPort_Spdif;
            break;
        case NVX_AUDIO_OutputI2S:
            gpi.pinPort = NvMMAudioMixerPort_I2s1;
            break;
        case NVX_AUDIO_OutputRingtone:
            gpi.pinPort = NvMMAudioMixerPort_Ringtone;
            break;
        default:
            if (pinDirection == NVMM_AUDIO_DIRECTION_OUTPUT)
                gpi.pinPort = NvMMAudioMixerPort_Music;
            else
                gpi.pinPort = NvMMAudioMixerPort_Undefined;
            break;
    }

    hBlock->Extension(hBlock, NvMMAudioExtension_GetPin,
        sizeof(NvMMMixerGetPin), &gpi, sizeof(NvMMMixerGetPin), &gpo);

    *pin = gpo.pin;
/*
    if(gpo.status != NvSuccess)
    {
        // TODO: figure out how to display error string from error table.
        NvOsDebugPrintf("AvGetMixerPin failed, return code: %d\n", gpo.status);
    }
*/
    return gpo.status;
}

static void AvReleaseMixerPin(NvMMBlockHandle hBlock, NvU32 pin)
{
    NvMMMixerReleasePin in, out;

    NvOsMemset(&in, 0, sizeof(NvMMMixerReleasePin));
    NvOsMemset(&out, 0, sizeof(NvMMMixerReleasePin));

    in.pin = pin;

    hBlock->Extension(hBlock, NvMMAudioExtension_ReleasePin,
        sizeof(NvMMMixerReleasePin), &in, sizeof(NvMMMixerReleasePin), &out);
}

static OMX_ERRORTYPE AvSetStatePerPin(SNvxNvMMTransformData *pData, 
                                      NvU32 pin,
                                      OMX_STATETYPE oNewState,
                                      OMX_STATETYPE oOldState)
{
    int start = 0, stop = 0, pause = 0;
    NvMMBlockHandle hNvMMBlock = pData->hBlock;
    NvError status = NvSuccess;
    NvMMState eState = NvMMState_Running;
    NvMMMixerSetState state;
    state.pin = pin;
    state.newState = 0;

    if (!hNvMMBlock)
        return OMX_ErrorNone;

    hNvMMBlock->GetState(hNvMMBlock, &eState);

    if (oNewState == OMX_StateExecuting && 
        (oOldState == OMX_StateIdle || oOldState == OMX_StatePause))
        start = 1;
    else if ((oNewState == OMX_StateIdle || oNewState == OMX_StateLoaded ||
              oNewState == OMX_StateInvalid) &&
             (oOldState == OMX_StateExecuting || oOldState == OMX_StatePause))
        stop = 1;
    else if (oNewState == OMX_StatePause && oOldState == OMX_StateExecuting)
        pause = 1;

    if (start && eState != NvMMState_Running)
    {
        state.newState = NvMMState_Running;
        status = hNvMMBlock->SetAttribute(hNvMMBlock, NvMMMixerAttribute_SetState, 0, sizeof(NvMMMixerSetState), &state);
        if (status != NvSuccess)
            return OMX_ErrorInvalidState;

        SetTSUpdates(pData, OMX_TRUE);
    
        return OMX_ErrorNone;
    }

    if (stop && eState != NvMMState_Stopped)
    {
        state.newState = NvMMState_Stopped;
        status = pData->hBlock->SetAttribute(hNvMMBlock, NvMMMixerAttribute_SetState, 0, sizeof(NvMMMixerSetState), &state);
        return OMX_ErrorNone;
    }

    if (pause && eState != NvMMState_Paused)
    {
        state.newState = NvMMState_Paused;
        status = pData->hBlock->SetAttribute(hNvMMBlock, NvMMMixerAttribute_SetState, 0, sizeof(NvMMMixerSetState), &state);
        if (status != NvSuccess)
            return OMX_ErrorInvalidState;
    
        return OMX_ErrorNone;
    }

    return OMX_ErrorNone;
}

static void NvxAudioRendererClockUpdate(NvxComponent *pNvComp, NvU32 eventType, NvU32 eventSize, void *eventInfo)
{
    SNvxAudioRendererData *pNvxAudio = 0;
    NvxPort *pPortClock = NULL;

    NV_ASSERT(pNvComp);
    NV_ASSERT(pNvComp->pComponentData);

    pNvxAudio = (SNvxAudioRendererData *)pNvComp->pComponentData;

    switch (eventType)
    {
        case NvMMEvent_TimeStampUpdate:
        {
            NvMMEventTimeStampUpdate *ts = (NvMMEventTimeStampUpdate *)eventInfo;
            NvU64 timestamp = ts->timeStamp / 10;

            pPortClock = &(pNvComp->pPorts[s_nvx_port_clock]);
            if (pPortClock && pPortClock->hTunnelComponent)
            {
                NvxCCSetCurrentAudioRefClock(pPortClock->hTunnelComponent,
                                             timestamp);

                if (!pNvxAudio->bFirstAudioFrameUpdated)
                {
                    NvxSendEvent(pNvComp, NVX_EventFirstAudioFramePlayed, 0, 0, 0);
                    pNvxAudio->bFirstAudioFrameUpdated = 1;
                }
            }

            if (timestamp > 0)
                pNvxAudio->nUpdateCount++;

            if (pNvxAudio->bDisableUpdates && pNvxAudio->nUpdateCount > 10)
            {
                SetTSUpdates(&pNvxAudio->oMixer, OMX_FALSE);
            }

            break;
        }
        case NvMMEvent_BlockStreamEnd:
        {
            SNvxNvMMPort *pPort = &(pNvxAudio->oMixer.oPorts[pNvxAudio->mixer_input]);

            NvOsSemaphoreSignal(pNvxAudio->eosWaitSem);

            // fake a EOS if we're tunneled.
            if (!pNvxAudio->bHaveEndBuffer)
                NvxSendEvent(pNvComp, OMX_EventBufferFlag,
                             pNvComp->pPorts[s_nvx_port_input].oPortDef.nPortIndex,
                             OMX_BUFFERFLAG_EOS, 0);
            else
                pNvxAudio->bHaveEndBuffer = OMX_FALSE;

            if (pPort->pSavedEOSBuf)
            {
                NvxPortReleaseBuffer(pPort->pOMXPort, pPort->pSavedEOSBuf);
                pPort->pSavedEOSBuf = NULL;
            }

            NvDebugOmx(("NvxAudioRendererClockUpdate: Sending EOS event"));
            break;
        }
        default:
            break;
    }
}

static OMX_ERRORTYPE NvxAudioRendererOpen(SNvxAudioRendererData * pNvxAudio, NvxComponent *pComponent)
{    
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pInPort = &(pComponent->pPorts[s_nvx_port_input]);
    NvxPort *pPortClock = &(pComponent->pPorts[s_nvx_port_clock]);
    OMX_AUDIO_PARAM_PCMMODETYPE *pLocalMode = (OMX_AUDIO_PARAM_PCMMODETYPE *)pInPort->pPortPrivate;

    NVXTRACE(( NVXT_CALLGRAPH, NVXT_AUDIO_RENDERER, "+NvxAudioRendererOpen\n"));

    pNvxAudio->bTunneling = (pInPort->bNvidiaTunneling) &&
                 (pInPort->eNvidiaTunnelTransaction == ENVX_TRANSACTION_NVMM_TUNNEL);

    if (NvSuccess != NvOsSemaphoreCreate(&(pNvxAudio->eosWaitSem), 0))
        return OMX_ErrorInsufficientResources;

    // open for 4, since this won't be known until the mixer gets queried..
    eError = NvxNvMMTransformOpen(&pNvxAudio->oMixer, NvMMBlockType_AudioMixer, "BlockMixer", 4, pComponent);
    if (eError != OMX_ErrorNone)
        return eError;

    AvGetMixerPin(pNvxAudio->oMixer.hBlock, NVMM_AUDIO_PINTYPE_PLAYBACK, 
                  NVMM_AUDIO_DIRECTION_INPUT,
                  0, 0, 0, pNvxAudio->eOutputType, &pNvxAudio->mixer_input);

    eError = NvxNvMMTransformSetupInput(&pNvxAudio->oMixer, pNvxAudio->mixer_input, &pComponent->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE, OMX_FALSE);
    if (eError != OMX_ErrorNone && eError != OMX_ErrorNotReady)
        return eError;

    NvxNvMMTransformSetupAudioParams(&pNvxAudio->oMixer, pNvxAudio->mixer_input, pLocalMode->nSamplingRate, pLocalMode->nBitPerSample, pLocalMode->nChannels);

    NvxNvMMTransformSetEventCallback(&pNvxAudio->oMixer, NvxAudioRendererClockUpdate, pComponent);

    if (pPortClock->hTunnelComponent)
    {
        SetTSUpdates(&pNvxAudio->oMixer, OMX_TRUE);
        pNvxAudio->nUpdateCount = 0;
        NvxCCSetWaitForAudio(pPortClock->hTunnelComponent);
    }

    pNvxAudio->bInitialized = OMX_TRUE;
    pNvxAudio->bFirstRun = OMX_TRUE;
    pNvxAudio->bHaveEndBuffer = OMX_FALSE;
    pNvxAudio->bFirstAudioFrameUpdated = 0;

    SetVolume(pNvxAudio);

    {
        int i,ch;
        pNvxAudio->oGraphicEqProp.bEnable = OMX_FALSE;
        for( ch = 0; ch < EQ_NO_CHANNEL; ch++)
        {
            for (i = 0; i < EQBANDS; i++)
            {
                if (pNvxAudio->oGraphicEqProp.dBGain[ch][i] != OMX_INVALID_GRAPHICEQ_GAIN)
                    pNvxAudio->oGraphicEqProp.bEnable = OMX_TRUE;
            }
        }

        if (pNvxAudio->oGraphicEqProp.bEnable)
            SetEQ(pNvxAudio);
    }

    SetDrcProperty(pNvxAudio);
    SetParaEqFilter(pNvxAudio);

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxAudioRendererClose(SNvxAudioRendererData *pNvxAudio)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_AUDIO_RENDERER, "+NvxAudioRendererClose\n"));

    if (!pNvxAudio->bInitialized)
        return OMX_ErrorNone;

    AvReleaseMixerPin(pNvxAudio->oMixer.hBlock, pNvxAudio->mixer_input);

    eError = NvxNvMMTransformClose(&pNvxAudio->oMixer);
    if (eError != OMX_ErrorNone)
        return eError;

    NvOsSemaphoreDestroy(pNvxAudio->eosWaitSem);

    pNvxAudio->bInitialized = OMX_FALSE; 
    
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxAudioRendererPreChangeState(NvxComponent *pNvComp, 
                                                    OMX_STATETYPE oNewState)
{
    SNvxAudioRendererData *pNvxAudio = 0;
    OMX_ERRORTYPE err;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioRendererPreChangeState\n"));

    pNvxAudio = (SNvxAudioRendererData *)pNvComp->pComponentData;
    if (!pNvxAudio->bInitialized)
        return OMX_ErrorNone;

    err = AvSetStatePerPin(&pNvxAudio->oMixer, pNvxAudio->mixer_input, oNewState, pNvComp->eState);
    if (OMX_ErrorNone != err)
        return err;

    err = NvxNvMMTransformPreChangeState(&pNvxAudio->oMixer, oNewState, 
                                         pNvComp->eState);
    if (OMX_ErrorNone != err)
        return err;

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

    data = NvOsAlloc(sizeof(SNvxAudioRendererData));
    if (!data)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType,"ERROR:NvxAudioRendererInit:"
            ":SNvxAudioData = %d:[%s(%d)]\n",data,__FILE__, __LINE__));
        return OMX_ErrorInsufficientResources;
    }

    NvOsMemset(data, 0, sizeof(SNvxAudioRendererData));
    data->volume = s_MIXER_UNITY_GAIN;
    data->pan = 0;
    data->mute = 0;
    data->eOutputType = NVX_AUDIO_OutputMusic;

    {
        int i,ch;
        data->oGraphicEqProp.bEnable = OMX_FALSE;
        for(ch = 0; ch < EQ_NO_CHANNEL; ch++)
        {
            for (i = 0; i < EQBANDS; i++)
                data->oGraphicEqProp.dBGain[ch][i] = OMX_INVALID_GRAPHICEQ_GAIN;
        }
    }
    data->oMixer.bWantTunneledAlloc = OMX_TRUE;
    data->oMixer.enableUlpMode = OMX_TRUE;

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

    NvxPortInitAudio(&pThis->pPorts[s_nvx_port_input], OMX_DirInput, MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE, OMX_AUDIO_CodingPCM);
    pThis->pPorts[s_nvx_port_input].eNvidiaTunnelTransaction = ENVX_TRANSACTION_NVMM_TUNNEL;

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

    data->oDrcProperty.EnableDrc = 0;
    data->oDrcProperty.ClippingTh = s_DRC_CLPPINGTH;
    data->oDrcProperty.LowerCompTh = s_DRC_LOWERCOMPTH;
    data->oDrcProperty.UpperCompTh = s_DRC_UPPERCOMPTH;
    data->oDrcProperty.NoiseGateTh = s_DRC_NOISEGATETH;

    return eError;
}
