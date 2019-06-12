/*
 * Copyright (c) 2006 - 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** NvxAudioDecoder.c */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

#include <OMX_Core.h>
#include "nvassert.h"
#include "common/NvxTrace.h"
#include "components/common/NvxComponent.h"
#include "components/common/NvxPort.h"
#include "nvmm/components/common/nvmmlitetransformbase.h"
#include "nvmm/components/common/NvxCompReg.h"
#include "nvfxmath.h"
#include "nvmmlite_audiodec.h"
#include "nvmmlite_mp3.h"
#include "nvmmlite_aac.h"
#include "nvmm_wma.h"
#include "NvxTrace.h"

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */

#define MAX_INPUT_BUFFERS   5
#define MAX_OUTPUT_BUFFERS  10
#define MIN_INPUT_BUFFERSIZE 24000 /* MAX INPUT SIZE defined in WMA decoder*/
#define MIN_OUTPUT_BUFFERSIZE 18432 /* MAX OUTPUT SIZE is for AC3 6-Ch stream*/


/* Number of internal frame buffers (AUDIO memory) */
static const int s_nvx_num_ports            = 2;
static const int s_nvx_port_input           = 0;
static const int s_nvx_port_output          = 1;

static OMX_ERRORTYPE NvxAudioDecoderAcquireResources(OMX_IN NvxComponent *pComponent);
static OMX_ERRORTYPE NvxAudioDecoderReleaseResources(OMX_IN NvxComponent *pComponent);

extern void NvxConvertProfileModeFormatToOmx(NvMMLiteBlockType oBlockType, NvU32 eProfile,
                                                                    OMX_U32 *eOmxProfile, NvU32 eMode, OMX_U32 *eOmxMode,
                                                                    NvU32 eFormat, OMX_U32 *eOmxFormat);


/* Aac Decoder State information */
typedef struct SNvxAudioDecoderData
{
    OMX_BOOL bInitialized;
    OMX_BOOL bOverrideTimestamp;

    NvMMLiteBlockType oBlockType;
    const char *sBlockName;

    SNvxNvMMLiteTransformData oBase;
    NvU32 samplingFreqIndex;

    OMX_BOOL bSentEOS;
    OMX_BOOL bCanSkipWorker;

    OMX_BOOL bFirstBuffer;
    OMX_BOOL (*HandleFirstPacket)(NvxComponent *pThis, NvxPort *pPort,
                                  SNvxNvMMLiteTransformData *pBase);

} SNvxAudioDecoderData;

/* =========================================================================
 *                     P R O T O T Y P E S
 * ========================================================================= */

static OMX_ERRORTYPE NvxAudioDecoderOpen( SNvxAudioDecoderData * pNvxAudioDecoder, NvxComponent *pComponent );
static OMX_ERRORTYPE NvxAudioDecoderClose( SNvxAudioDecoderData * pNvxAudioDecoder );

/* =========================================================================
 *                     F U N C T I O N S
 * ========================================================================= */

static OMX_ERRORTYPE NvxAudioDecoderDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioDecoderInit\n"));

    NvOsFree(pNvComp->pComponentData);
    pNvComp->pComponentData = 0;
    return eError;
}

static OMX_ERRORTYPE NvxAudioDecoderGetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    SNvxAudioDecoderData *pNvxAudioDecoder;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAudioDecoderGetParameter 0x%x \n", nParamIndex));

    pNvxAudioDecoder = (SNvxAudioDecoderData *)pComponent->pComponentData;

    switch (nParamIndex)
    {
    case OMX_IndexParamPortDefinition:
        pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
        {
            eError = NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
            if(s_nvx_port_output == (NvS32)pPortDef->nPortIndex)
            {
                NvOsMemcpy(&pPortDef->format.audio, &pComponent->pPorts[s_nvx_port_output].oPortDef.format.audio,
                            sizeof(OMX_AUDIO_PORTDEFINITIONTYPE));
            }
            else
            {
                return eError;
            }
        }
        break;
    case OMX_IndexParamAudioPcm:
    {
        NvxPort *pPortOut = &pComponent->pPorts[s_nvx_port_output];
        OMX_AUDIO_PARAM_PCMMODETYPE *pLocalMode = (OMX_AUDIO_PARAM_PCMMODETYPE *)pPortOut->pPortPrivate;
        OMX_AUDIO_PARAM_PCMMODETYPE *pMode = (OMX_AUDIO_PARAM_PCMMODETYPE *)pComponentParameterStructure;
        NvError err = OMX_ErrorNone;
        NvMMLiteAudioPcmParam Pcmparam;

        if (pNvxAudioDecoder->oBase.hBlock)
        {
            err = pNvxAudioDecoder->oBase.hBlock->GetAttribute(
                pNvxAudioDecoder->oBase.hBlock, NvMMLiteAttributeAudioDec_PcmStreamProperty,
                sizeof(NvMMLiteAudioPcmParam), &Pcmparam);
        }

        if ((NvU32)s_nvx_port_output == pMode->nPortIndex)
        {
            SNvxNvMMLitePort *pPort = &(pNvxAudioDecoder->oBase.oPorts[s_nvx_port_output]);
            NvOsMemcpy(pMode, pLocalMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
            if (pPort->bAudioMDValid)
            {
                pLocalMode->nSamplingRate = pMode->nSamplingRate = pPort->audioMetaData.nSampleRate;
                pLocalMode->nChannels = pMode->nChannels = pPort->audioMetaData.nChannels;
                pLocalMode->nBitPerSample = pMode->nBitPerSample = pPort->audioMetaData.nBitsPerSample;
            }

            if ((err == NvSuccess) && pPort->bAudioMDValid && (pMode->nChannels > 2))
            {
                int i = 0;
                for (;(i < 16) && Pcmparam.eChannelMapping[i] ;i++)
                {
                    switch (Pcmparam.eChannelMapping[i])
                    {
                        case NvMMLite_ChannelNone:
                        {
                            pMode->eChannelMapping[i] = OMX_AUDIO_ChannelNone;
                            break;
                        }
                        case NvMMLite_ChannelLF:
                        {
                            pMode->eChannelMapping[i] = OMX_AUDIO_ChannelLF;
                            break;
                        }
                        case NvMMLite_ChannelRF:
                        {
                            pMode->eChannelMapping[i] = OMX_AUDIO_ChannelRF;
                            break;
                        }
                        case NvMMLite_ChannelCF:
                        {
                            pMode->eChannelMapping[i] = OMX_AUDIO_ChannelCF;
                            break;
                        }
                        case NvMMLite_ChannelLS:
                        {
                            pMode->eChannelMapping[i] = OMX_AUDIO_ChannelLS;
                            break;
                        }
                        case NvMMLite_ChannelRS:
                        {
                            pMode->eChannelMapping[i] = OMX_AUDIO_ChannelRS;
                            break;
                        }
                        case NvMMLite_ChannelLFE:
                        {
                            pMode->eChannelMapping[i] = OMX_AUDIO_ChannelLFE;
                            break;
                        }
                        case NvMMLite_ChannelCS:
                        {
                            pMode->eChannelMapping[i] = OMX_AUDIO_ChannelCS;
                            break;
                        }
                        case NvMMLite_ChannelLR:
                        {
                            pMode->eChannelMapping[i] = OMX_AUDIO_ChannelLR;
                            break;
                        }
                        case NvMMLite_ChannelRR:
                        {
                            pMode->eChannelMapping[i] = OMX_AUDIO_ChannelRR;
                            break;
                        }
                        default:
                        {
                            pMode->eChannelMapping[i] = OMX_AUDIO_ChannelNone;
                            break;
                        }
                    }
                }
            }
            return OMX_ErrorNone;

        }
        return OMX_ErrorBadParameter;
    }
    case OMX_IndexParamAudioMp3:
    {
        OMX_AUDIO_PARAM_MP3TYPE *pMp3Param, *pLocalMode;
        NvMMLiteAudioMp3Param Mp3param;
        NvError err = OMX_ErrorNone;

        NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];

        pLocalMode = (OMX_AUDIO_PARAM_MP3TYPE *)pPortIn->pPortPrivate;
        pMp3Param  = (OMX_AUDIO_PARAM_MP3TYPE *)pComponentParameterStructure;

        if (pNvxAudioDecoder->oBlockType == NvMMLiteBlockType_DecMP3 &&
            (NvU32)s_nvx_port_input == pMp3Param->nPortIndex)
        {
            if (!pNvxAudioDecoder->oBase.hBlock)
            {
                // Return Default values if getParam called in loaded state
                NvOsMemcpy(pMp3Param, pLocalMode, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
                return OMX_ErrorNone;
            }

            err = pNvxAudioDecoder->oBase.hBlock->GetAttribute(
                      pNvxAudioDecoder->oBase.hBlock,
                      NvMMLiteAttributeAudioDec_Mp3StreamProperty,
                      sizeof(NvMMLiteAudioMp3Param), &Mp3param);

            if (NvSuccess != err)
                return OMX_ErrorNotReady;

            pLocalMode->nChannels       = Mp3param.nChannels;
            pLocalMode->nBitRate        = Mp3param.nBitRate;
            pLocalMode->nSampleRate     = Mp3param.nSampleRate;
            pLocalMode->nAudioBandWidth = Mp3param.nAudioBandWidth;

            NvxConvertProfileModeFormatToOmx(pNvxAudioDecoder->oBlockType, 0, NULL,
                                                    Mp3param.eChannelMode, (OMX_U32 *)&pLocalMode->eChannelMode,
                                                    Mp3param.eFormat, (OMX_U32 *)&pLocalMode->eFormat);

            NvOsMemcpy(pMp3Param, pLocalMode, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
            return OMX_ErrorNone;
        }
        return OMX_ErrorBadParameter;
    }
    case OMX_IndexParamAudioAac:
    {
        OMX_AUDIO_PARAM_AACPROFILETYPE *pLocalMode, *pAAC;
        NvMMLiteAudioAacParam Aacparam;
        NvError err = OMX_ErrorNone;
        NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];

        pLocalMode = (OMX_AUDIO_PARAM_AACPROFILETYPE *)pPortIn->pPortPrivate;
        pAAC       = (OMX_AUDIO_PARAM_AACPROFILETYPE *)pComponentParameterStructure;

        if ((pNvxAudioDecoder->oBlockType == NvMMLiteBlockType_DecAAC       ||
             pNvxAudioDecoder->oBlockType == NvMMLiteBlockType_DecEAACPLUS) &&
            (NvU32)s_nvx_port_input == pAAC->nPortIndex)
        {
            if (!pNvxAudioDecoder->oBase.hBlock)
            {
                // Return Default values if getParam called in loaded state
                NvOsMemcpy(pAAC, pLocalMode, sizeof(OMX_AUDIO_PARAM_AACPROFILETYPE));
                return OMX_ErrorNone;
            }

            err = pNvxAudioDecoder->oBase.hBlock->GetAttribute(
                    pNvxAudioDecoder->oBase.hBlock, NvMMLiteAttributeAudioDec_AacStreamProperty,
                    sizeof(NvMMLiteAudioAacParam), &Aacparam);

            if (NvSuccess != err)
            {
                NvOsDebugPrintf("%s[%d] err 0x%x --  \n",
                __FUNCTION__, __LINE__, err);
                return OMX_ErrorNotReady;
            }

            pLocalMode->nChannels       = Aacparam.nChannels;
            pLocalMode->nSampleRate     = Aacparam.nSampleRate;
            pLocalMode->nBitRate        = Aacparam.nBitRate;
            pLocalMode->nAudioBandWidth = Aacparam.nAudioBandWidth;
            pLocalMode->nFrameLength    = Aacparam.nFrameLength;

            pLocalMode->nAACtools = 0;
            if (NvMMLite_AUDIO_AACToolNone == Aacparam.nAACtools)
            {
                pLocalMode->nAACtools = OMX_AUDIO_AACToolNone;
            }
            else if (NvMMLite_AUDIO_AACToolAll == Aacparam.nAACtools)
            {
                pLocalMode->nAACtools = OMX_AUDIO_AACToolAll;
            }
            else
            {
                if (Aacparam.nAACtools & NvMMLite_AUDIO_AACToolMS)
                    pLocalMode->nAACtools |= OMX_AUDIO_AACToolMS;
                if (Aacparam.nAACtools & NvMMLite_AUDIO_AACToolIS)
                    pLocalMode->nAACtools |= OMX_AUDIO_AACToolIS;
                if (Aacparam.nAACtools & NvMMLite_AUDIO_AACToolTNS)
                    pLocalMode->nAACtools |= OMX_AUDIO_AACToolTNS;
                if (Aacparam.nAACtools & NvMMLite_AUDIO_AACToolPNS)
                    pLocalMode->nAACtools |= OMX_AUDIO_AACToolPNS;
                if (Aacparam.nAACtools & NvMMLite_AUDIO_AACToolLTP)
                    pLocalMode->nAACtools |= OMX_AUDIO_AACToolLTP;
            }

            pLocalMode->nAACERtools = 0;
            if (NvMMLite_AUDIO_AACERNone == Aacparam.nAACERtools)
            {
                pLocalMode->nAACERtools = OMX_AUDIO_AACERNone;
            }
            else if (NvMMLite_AUDIO_AACERAll == Aacparam.nAACERtools)
            {
                pLocalMode->nAACERtools = OMX_AUDIO_AACERAll;
            }
            else
            {
                if (Aacparam.nAACERtools& NvMMLite_AUDIO_AACERVCB11)
                    pLocalMode->nAACERtools |= OMX_AUDIO_AACERVCB11;
                if (Aacparam.nAACERtools& NvMMLite_AUDIO_AACERRVLC)
                    pLocalMode->nAACERtools |= OMX_AUDIO_AACERRVLC;
                if (Aacparam.nAACERtools& NvMMLite_AUDIO_AACERHCR)
                    pLocalMode->nAACERtools |= OMX_AUDIO_AACERHCR;
            }

            NvxConvertProfileModeFormatToOmx(pNvxAudioDecoder->oBlockType, Aacparam.eAACProfile, (OMX_U32 *)&pLocalMode->eAACProfile,
                                                    Aacparam.eChannelMode, (OMX_U32 *)&pLocalMode->eChannelMode,
                                                    Aacparam.eAACStreamFormat, (OMX_U32 *)&pLocalMode->eAACStreamFormat);

            NvOsMemcpy(pAAC, pLocalMode, sizeof(OMX_AUDIO_PARAM_AACPROFILETYPE));
            return OMX_ErrorNone;
        }

        return OMX_ErrorBadParameter;
    }
    case OMX_IndexParamAudioWma:
    {
        OMX_AUDIO_PARAM_WMATYPE *pWmaParam, *pLocalMode;
        NvMMAudioWmaParam Wmaparam;
        NvError err = OMX_ErrorNone;

        NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];

        pLocalMode = (OMX_AUDIO_PARAM_WMATYPE *)pPortIn->pPortPrivate;
        pWmaParam  = (OMX_AUDIO_PARAM_WMATYPE *)pComponentParameterStructure;

        if ((pNvxAudioDecoder->oBlockType == NvMMLiteBlockType_DecWMA     ||
             pNvxAudioDecoder->oBlockType == NvMMLiteBlockType_DecWMAPRO  ||
             pNvxAudioDecoder->oBlockType == NvMMLiteBlockType_DecWMALSL ||
             pNvxAudioDecoder->oBlockType == NvMMLiteBlockType_DecWMASUPER) &&
            ((NvU32)s_nvx_port_input == pWmaParam->nPortIndex))
        {
            if (!pNvxAudioDecoder->oBase.hBlock)
            {
                // Return Default values if getParam called in loaded state
                NvOsMemcpy(pWmaParam, pLocalMode, sizeof(OMX_AUDIO_PARAM_WMATYPE));
                return OMX_ErrorNone;
            }

            err = pNvxAudioDecoder->oBase.hBlock->GetAttribute(
                    pNvxAudioDecoder->oBase.hBlock, NvMMLiteAttributeAudioDec_WmaStreamProperty,
                    sizeof(NvMMAudioWmaParam), &Wmaparam);

            if (NvSuccess != err)
                return OMX_ErrorNotReady;

            pLocalMode->nChannels        = Wmaparam.nChannels;
            pLocalMode->nBitRate         = Wmaparam.nBitRate;
            pLocalMode->nSamplingRate    = Wmaparam.nSamplingRate;
            pLocalMode->nBlockAlign      = Wmaparam.nBlockAlign;
            pLocalMode->nEncodeOptions   = Wmaparam.nEncodeOptions;
            pLocalMode->nSuperBlockAlign = Wmaparam.nSuperBlockAlign;

            NvxConvertProfileModeFormatToOmx(pNvxAudioDecoder->oBlockType, Wmaparam.eProfile, (OMX_U32 *)&pLocalMode->eProfile,
                                                    0, NULL,
                                                    Wmaparam.eFormat, (OMX_U32 *)&pLocalMode->eFormat);

            NvOsMemcpy(pWmaParam, pLocalMode, sizeof(OMX_AUDIO_PARAM_WMATYPE));
            return OMX_ErrorNone;
        }
        return OMX_ErrorBadParameter;
    }

    case NVX_IndexParamSyncDecodeMode:
        ((NVX_PARAM_SYNCDECODE *)pComponentParameterStructure)->bSyncDecodeMode = pNvxAudioDecoder->bCanSkipWorker;
        break;

    case NVX_IndexParamAudioCodecCapability:
    {
        NVX_PARAM_AUDIOCODECCAPABILITY * pCapInfo = (NVX_PARAM_AUDIOCODECCAPABILITY *)pComponentParameterStructure;
        NvMMLiteAudioDecoderCapability pLiteCapInfo;

        NvError err = OMX_ErrorNone;

        pLiteCapInfo.pSampleRatesSupported = NvOsAlloc(sizeof(NvU32)* 16);//max sample rate array can be of 16
        pLiteCapInfo.pBitratesSupported = NvOsAlloc(sizeof(NvU32)* 24); //max bit rate array can be of 24

        if (pNvxAudioDecoder->oBlockType == NvMMLiteBlockType_DecAAC || pNvxAudioDecoder->oBlockType == NvMMLiteBlockType_DecEAACPLUS)
        {
            OMX_AUDIO_PARAM_AACPROFILETYPE *eOmxProfile;
            NvMMLiteAudioAacParam pAACparams;

            eOmxProfile = (OMX_AUDIO_PARAM_AACPROFILETYPE *)pComponent->pPorts[s_nvx_port_input].pPortPrivate;

            if (!pNvxAudioDecoder->oBase.hBlock)
            {
                NvOsDebugPrintf("Return default values if getParam called in loaded state \n");
                pCapInfo->nProfileType = (OMX_U32)eOmxProfile->eAACProfile;
                pCapInfo->nModeType = (OMX_U32)eOmxProfile->eChannelMode;
                pCapInfo->nStreamFormatType = (OMX_U32)eOmxProfile->eAACStreamFormat;
                pCapInfo->nMaxBitRate = 320000;
                pCapInfo->nMaxChannels = 6;
                pCapInfo->nMaxBitsPerSample = 16;
                pCapInfo->nMaxSampleRate = 48000;
                pCapInfo->nMinBitRate = 8000;
                pCapInfo->nMinBitsPerSample = 16;
                pCapInfo->nMinSampleRate = 8000;
                return OMX_ErrorNone;
            }

            err = pNvxAudioDecoder->oBase.hBlock->GetAttribute(pNvxAudioDecoder->oBase.hBlock,
                                                    NvMMLiteAttributeAudioDec_AacStreamProperty,
                                                    sizeof(NvMMLiteAudioAacParam),
                                                    &pAACparams);

            if (NvSuccess != err)
            {
                NvOsDebugPrintf("%s[%d] err 0x%x --  \n", __FUNCTION__, __LINE__, err);
                return OMX_ErrorNotReady;
            }

            NvxConvertProfileModeFormatToOmx(pNvxAudioDecoder->oBlockType, pAACparams.eAACProfile, (OMX_U32 *)&eOmxProfile->eAACProfile,
                                                    pAACparams.eChannelMode, (OMX_U32 *)&eOmxProfile->eChannelMode,
                                                    pAACparams.eAACStreamFormat, (OMX_U32 *)&eOmxProfile->eAACStreamFormat);

            pCapInfo->nProfileType = (OMX_U32)eOmxProfile->eAACProfile;
            pCapInfo->nModeType = (OMX_U32)eOmxProfile->eChannelMode;
            pCapInfo->nStreamFormatType = (OMX_U32)eOmxProfile->eAACStreamFormat;

            err = pNvxAudioDecoder->oBase.hBlock->GetAttribute(
                    pNvxAudioDecoder->oBase.hBlock, NvMMLiteAttributeAudioDec_AacCapabilities,
                    sizeof(NvMMLiteAudioDecoderCapability), &pLiteCapInfo);

            if (NvSuccess != err)
            {
                NvOsDebugPrintf("%s[%d] err 0x%x --  \n", __FUNCTION__, __LINE__, err);
                return OMX_ErrorNotReady;
            }

        }

        else if (pNvxAudioDecoder->oBlockType == NvMMLiteBlockType_DecMP3)
        {
            OMX_AUDIO_PARAM_MP3TYPE *eOmxProfile;
            NvMMLiteAudioMp3Param Mp3param;

            NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];

            eOmxProfile = (OMX_AUDIO_PARAM_MP3TYPE *)pPortIn->pPortPrivate;

            if (!pNvxAudioDecoder->oBase.hBlock)
            {
                NvOsDebugPrintf("Return default values if getParam called in loaded state \n");
                pCapInfo->nProfileType = 0;
                pCapInfo->nModeType = (OMX_U32)eOmxProfile->eChannelMode;
                pCapInfo->nStreamFormatType = (OMX_U32)eOmxProfile->eFormat;
                pCapInfo->nMaxBitRate = 320000;
                pCapInfo->nMaxChannels = 2;
                pCapInfo->nMaxBitsPerSample = 16;
                pCapInfo->nMaxSampleRate = 48000;
                pCapInfo->nMinBitRate = 32000;
                pCapInfo->nMinBitsPerSample = 16;
                pCapInfo->nMinSampleRate = 8000;
                return OMX_ErrorNone;
            }

            err = pNvxAudioDecoder->oBase.hBlock->GetAttribute(
                      pNvxAudioDecoder->oBase.hBlock,
                      NvMMLiteAttributeAudioDec_Mp3StreamProperty,
                      sizeof(NvMMLiteAudioMp3Param), &Mp3param);

            if (NvSuccess != err)
            {
                NvOsDebugPrintf("%s[%d] err 0x%x --  \n", __FUNCTION__, __LINE__, err);
                return OMX_ErrorNotReady;
            }

            NvxConvertProfileModeFormatToOmx(pNvxAudioDecoder->oBlockType, 0, NULL,
                                                    Mp3param.eChannelMode, (OMX_U32 *)&eOmxProfile->eChannelMode,
                                                    Mp3param.eFormat, (OMX_U32 *)&eOmxProfile->eFormat);

            pCapInfo->nProfileType = 0;
            pCapInfo->nModeType = (OMX_U32)eOmxProfile->eChannelMode;
            pCapInfo->nStreamFormatType = (OMX_U32)eOmxProfile->eFormat;

            pLiteCapInfo.nStreamFormatType = (OMX_U32)eOmxProfile->eFormat;

            err = pNvxAudioDecoder->oBase.hBlock->GetAttribute(
                    pNvxAudioDecoder->oBase.hBlock, NvMMLiteAttributeAudioDec_Mp3Capabilities,
                    sizeof(NvMMLiteAudioDecoderCapability), &pLiteCapInfo);

            if (NvSuccess != err)
            {
                NvOsDebugPrintf("%s[%d] err 0x%x --  \n", __FUNCTION__, __LINE__, err);
                return OMX_ErrorNotReady;
            }

        }

        else if ((pNvxAudioDecoder->oBlockType == NvMMLiteBlockType_DecWMA     ||
                 pNvxAudioDecoder->oBlockType == NvMMLiteBlockType_DecWMAPRO  ||
                 pNvxAudioDecoder->oBlockType == NvMMLiteBlockType_DecWMALSL ||
                 pNvxAudioDecoder->oBlockType == NvMMLiteBlockType_DecWMASUPER))
        {
            OMX_AUDIO_PARAM_WMATYPE *pLocalMode;
            NvMMAudioWmaParam Wmaparam;


            NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];

            pLocalMode = (OMX_AUDIO_PARAM_WMATYPE *)pPortIn->pPortPrivate;

            if (!pNvxAudioDecoder->oBase.hBlock)
            {
                NvOsDebugPrintf("Return default values if getParam called in loaded state \n");
                pCapInfo->nProfileType = (OMX_U32)pLocalMode->eProfile;
                pCapInfo->nModeType = 0;
                pCapInfo->nStreamFormatType = (OMX_U32)pLocalMode->eFormat;
                pCapInfo->nMaxBitRate = 384000;
                pCapInfo->nMaxChannels = 6;
                pCapInfo->nMaxBitsPerSample = 16;
                pCapInfo->nMaxSampleRate = 48000;
                pCapInfo->nMinBitRate = 8000;
                pCapInfo->nMinBitsPerSample = 16;
                pCapInfo->nMinSampleRate = 8000;
                return OMX_ErrorNone;
            }

            err = pNvxAudioDecoder->oBase.hBlock->GetAttribute(
                    pNvxAudioDecoder->oBase.hBlock, NvMMLiteAttributeAudioDec_WmaStreamProperty,
                    sizeof(NvMMAudioWmaParam), &Wmaparam);

            if (NvSuccess != err)
            {
                NvOsDebugPrintf("%s[%d] err 0x%x --  \n", __FUNCTION__, __LINE__, err);
                return OMX_ErrorNotReady;
            }

            NvxConvertProfileModeFormatToOmx(pNvxAudioDecoder->oBlockType, Wmaparam.eProfile, (OMX_U32 *)&pLocalMode->eProfile,
                                                    0, NULL,
                                                    Wmaparam.eFormat, (OMX_U32 *)&pLocalMode->eFormat);

            pCapInfo->nProfileType = (OMX_U32)pLocalMode->eProfile;
            pCapInfo->nModeType = 0;
            pCapInfo->nStreamFormatType = (OMX_U32)pLocalMode->eFormat;

            err = pNvxAudioDecoder->oBase.hBlock->GetAttribute(
                    pNvxAudioDecoder->oBase.hBlock, NvMMLiteAttributeAudioDec_WmaCapabilities,
                    sizeof(NvMMLiteAudioDecoderCapability), &pLiteCapInfo);

            if (NvSuccess != err)
            {
                NvOsDebugPrintf("%s[%d] err 0x%x --  \n", __FUNCTION__, __LINE__, err);
                return OMX_ErrorNotReady;
            }

        }
        else
        {
            return OMX_ErrorNotReady;
        }

        pCapInfo->nMaxBitRate = pLiteCapInfo.nMaxBitRate;
        pCapInfo->nMaxChannels = pLiteCapInfo.nMaxChannels;
        pCapInfo->nMaxBitsPerSample = pLiteCapInfo.nMaxBitsPerSample;
        pCapInfo->nMaxSampleRate = pLiteCapInfo.nMaxSampleRate;
        pCapInfo->nMinBitRate = pLiteCapInfo.nMinBitRate;
        pCapInfo->nMinBitsPerSample = pLiteCapInfo.nMinBitsPerSample;
        pCapInfo->nMinSampleRate = pLiteCapInfo.nMinSampleRate;
        pCapInfo->isBitrateRangeContinuous = pLiteCapInfo.isBitrateRangeContinuous;
        pCapInfo->isFreqRangeContinuous = pLiteCapInfo.isFreqRangeContinuous;

        if (pCapInfo->isFreqRangeContinuous == OMX_FALSE)
        {
            pCapInfo->nSampleRatesSupported = pLiteCapInfo.nSampleRatesSupported;
            pCapInfo->pSampleRatesSupported = (OMX_U32 *)pLiteCapInfo.pSampleRatesSupported;
        }
        else
        {
            NvOsFree(pLiteCapInfo.pSampleRatesSupported);
            pCapInfo->pSampleRatesSupported = NULL;
            pCapInfo->nSampleRatesSupported = 0;
        }

        if (pCapInfo->isBitrateRangeContinuous == OMX_FALSE)
        {
          pCapInfo->nBitratesSupported = pLiteCapInfo.nBitratesSupported;
          pCapInfo->pBitratesSupported = (OMX_U32 *)pLiteCapInfo.pBitratesSupported;
        }
        else
        {
          NvOsFree(pLiteCapInfo.pBitratesSupported);
          pCapInfo->pBitratesSupported = NULL;
          pCapInfo->nBitratesSupported = 0;
        }

    }
        break;

    default:
        eError = NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
    };

    return eError;
}

static OMX_ERRORTYPE NvxAudioDecoderSetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentParameterStructure)
{
    SNvxAudioDecoderData *pNvxAudioDecoder;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent && pComponentParameterStructure);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAudioDecoderSetParameter 0x%x \n", nIndex));

    pNvxAudioDecoder = (SNvxAudioDecoderData *)pComponent->pComponentData;
    switch(nIndex)
    {
    case NVX_IndexParamSyncDecodeMode:
        pNvxAudioDecoder->bCanSkipWorker = !((NVX_PARAM_SYNCDECODE *)pComponentParameterStructure)->bSyncDecodeMode;
        break;
    case OMX_IndexParamAudioMp3:
        {
            OMX_AUDIO_PARAM_MP3TYPE *pMp3Param, *pLocalMode;
            NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];

            pLocalMode = (OMX_AUDIO_PARAM_MP3TYPE *)pPortIn->pPortPrivate;
            pMp3Param = (OMX_AUDIO_PARAM_MP3TYPE *)pComponentParameterStructure;

            if (pNvxAudioDecoder->oBlockType == NvMMLiteBlockType_DecMP3 &&
                (NvU32)s_nvx_port_input == pMp3Param->nPortIndex)
            {
                NvOsMemcpy(pLocalMode, pMp3Param, sizeof(OMX_AUDIO_PARAM_AMRTYPE));
                return OMX_ErrorNone;
            }
            return OMX_ErrorBadParameter;
        }
    case OMX_IndexParamAudioAac:
    {
        OMX_AUDIO_PARAM_AACPROFILETYPE *pLocalMode, *pAAC;
        NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];

        pLocalMode = (OMX_AUDIO_PARAM_AACPROFILETYPE *)pPortIn->pPortPrivate;
        pAAC = (OMX_AUDIO_PARAM_AACPROFILETYPE *)pComponentParameterStructure;

        if ((pNvxAudioDecoder->oBlockType == NvMMLiteBlockType_DecAAC ||
            pNvxAudioDecoder->oBlockType == NvMMLiteBlockType_DecEAACPLUS) &&
            (NvU32)s_nvx_port_input == pAAC->nPortIndex)
        {
            // opencore misidentifies its mp4ff format stuff as adts
            if (pAAC->eAACProfile != OMX_AUDIO_AACObjectLC ||
                //pAAC->eAACStreamFormat != OMX_AUDIO_AACStreamFormatMP4FF ||
                pAAC->eChannelMode != OMX_AUDIO_ChannelModeStereo)
            {
                return OMX_ErrorUnsupportedSetting;
            }

            return OMX_ErrorNone;
        }

        return OMX_ErrorBadParameter;
    }
    case OMX_IndexParamAudioWma:
    {
        OMX_AUDIO_PARAM_WMATYPE *pLocalMode, *pWMA;
        NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];

        pLocalMode = (OMX_AUDIO_PARAM_WMATYPE *)pPortIn->pPortPrivate;
        pWMA = (OMX_AUDIO_PARAM_WMATYPE *)pComponentParameterStructure;

        if ((pNvxAudioDecoder->oBlockType == NvMMLiteBlockType_DecWMA ||
            pNvxAudioDecoder->oBlockType == NvMMLiteBlockType_DecWMAPRO ||
            pNvxAudioDecoder->oBlockType == NvMMLiteBlockType_DecWMALSL) &&
            (NvU32)s_nvx_port_input == pWMA->nPortIndex)
        {

            return OMX_ErrorNone;
        }

        return OMX_ErrorBadParameter;
    }
    case OMX_IndexParamAudioPcm:
    {
        OMX_AUDIO_PARAM_PCMMODETYPE *pMode = (OMX_AUDIO_PARAM_PCMMODETYPE *)pComponentParameterStructure;
        NvxPort *pPortOut = &pComponent->pPorts[s_nvx_port_output];
        OMX_AUDIO_PARAM_PCMMODETYPE *pLocalMode = (OMX_AUDIO_PARAM_PCMMODETYPE *)pPortOut->pPortPrivate;

        if(s_nvx_port_output == pMode->nPortIndex) {
            pLocalMode->nSamplingRate = pMode->nSamplingRate;
            pLocalMode->nChannels = pMode->nChannels;
            pLocalMode->nBitPerSample = pMode->nBitPerSample;
            return OMX_ErrorNone;
        }
        return OMX_ErrorBadParameter;
    }
    default:
        eError = NvxComponentBaseSetParameter(pComponent, nIndex, pComponentParameterStructure);
    }
    return eError;
}

static OMX_ERRORTYPE NvxAudioDecoderGetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    SNvxAudioDecoderData *pNvxAudioDecoder;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioDecoderGetConfig\n"));

    pNvxAudioDecoder = (SNvxAudioDecoderData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
        case NVX_IndexConfigAudioDualMonoOuputMode:
        {
            OMX_PARAM_U32TYPE *pDualMonoOutputMode;
            pDualMonoOutputMode = (OMX_PARAM_U32TYPE *)pComponentConfigStructure;
            if (pNvxAudioDecoder->oBase.hBlock &&
                (pDualMonoOutputMode->nSize == sizeof(OMX_PARAM_U32TYPE)))
            {
                eError = pNvxAudioDecoder->oBase.hBlock->GetAttribute(
                        pNvxAudioDecoder->oBase.hBlock, NvMMLiteAttributeAudioDec_DualMonoOutputMode,
                        sizeof(NvU32), &pDualMonoOutputMode->nU32);
            }
            else
            {
                eError = OMX_ErrorNotReady;
                NvOsDebugPrintf("NVX_IndexConfigAudioDualMonoOuputMode Error: block not open \n");
            }

            break;
        }

        default:
            eError = NvxComponentBaseGetConfig( pNvComp, nIndex, pComponentConfigStructure);
            break;
    }
    return eError;
}

static OMX_ERRORTYPE NvxAudioDecoderSetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentConfigStructure)
{
    SNvxAudioDecoderData *pNvxAudioDecoder;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioDecoderSetConfig 0x%x \n", nIndex));

    pNvxAudioDecoder = (SNvxAudioDecoderData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
        case NVX_IndexConfigProfile:
        {
            NVX_CONFIG_PROFILE *pProf = (NVX_CONFIG_PROFILE *)pComponentConfigStructure;
            NvxNvMMLiteTransformSetProfile(&pNvxAudioDecoder->oBase, pProf);
            break;
        }
        case OMX_IndexConfigTimeScale:
        {
            OMX_TIME_CONFIG_SCALETYPE *pScale;
            NvMMLiteAudioRateInfo oRate;
            float fRate;

            pScale= (OMX_TIME_CONFIG_SCALETYPE *)pComponentConfigStructure;
            fRate= NvSFxFixed2Float(pScale->xScale);
            /* TODO do a correct mapping between OMX and NVMM */

            oRate.Rate = (NvU32)(fRate * 100000.0);

            pNvxAudioDecoder->oBase.hBlock->SetAttribute(pNvxAudioDecoder->oBase.hBlock,
                    NvMMLiteAttributeAudioDec_RateInfo,
                    NvMMLiteSetAttrFlag_Notification,
                    sizeof(NvMMLiteAudioRateInfo), &oRate);

            break;
        }
        case NVX_IndexConfigMaxOutputChannels:
        {
            OMX_PARAM_U32TYPE *pChannel;
            pChannel = (OMX_PARAM_U32TYPE *)pComponentConfigStructure;
            if (pNvxAudioDecoder->oBase.hBlock)
            {
                pNvxAudioDecoder->oBase.hBlock->SetAttribute(pNvxAudioDecoder->oBase.hBlock,
                    NvMMLiteAttributeAudioDec_MaxOutputChannels, NvMMLiteSetAttrFlag_Notification,
                    sizeof(NvU32), &pChannel->nU32);
            }
            else
                NvOsDebugPrintf("NVX_IndexConfigMaxOutputChannels Error: block not open \n");

            break;
        }
        case NVX_IndexConfigAudioDualMonoOuputMode:
        {
            OMX_PARAM_U32TYPE *pDualMonoOutputMode;
            pDualMonoOutputMode = (OMX_PARAM_U32TYPE *)pComponentConfigStructure;
            if (pNvxAudioDecoder->oBase.hBlock &&
                (pDualMonoOutputMode->nSize == sizeof(OMX_PARAM_U32TYPE)))
            {
                pNvxAudioDecoder->oBase.hBlock->SetAttribute(pNvxAudioDecoder->oBase.hBlock,
                    NvMMLiteAttributeAudioDec_DualMonoOutputMode, NvMMSetAttrFlag_Notification,
                    sizeof(OMX_U32), &pDualMonoOutputMode->nU32);
            }
            else
            {
                eError = OMX_ErrorNotReady;
                NvOsDebugPrintf("NVX_IndexConfigAudioDualMonoOuputMode Error: block not open \n");
            }

            break;
        }

    default:
            eError = NvxComponentBaseSetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return eError;
}

static OMX_ERRORTYPE NvxAudioDecoderWorkerFunction(OMX_IN NvxComponent *pComponent, OMX_IN OMX_BOOL bAllPortsReady, OMX_OUT OMX_BOOL *pbMoreWork, OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    SNvxAudioDecoderData   *pNvxAudioDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];
    NvxPort *pPortOut = &pComponent->pPorts[s_nvx_port_output];

    NVXTRACE((NVXT_WORKER, pComponent->eObjectType, "+NvxAudioDecoderWorkerFunction\n"));

    *pbMoreWork = bAllPortsReady;

    pNvxAudioDecoder = (SNvxAudioDecoderData *)pComponent->pComponentData;

    if (pPortIn->pCurrentBufferHdr && pNvxAudioDecoder->bFirstBuffer)
    {
        pNvxAudioDecoder->bFirstBuffer = OMX_FALSE;
        if (pNvxAudioDecoder->HandleFirstPacket)
        {
            if (pNvxAudioDecoder->HandleFirstPacket(pComponent, pPortIn,
                                                    &pNvxAudioDecoder->oBase))
            {
                NvxPortReleaseBuffer(pPortIn, pPortIn->pCurrentBufferHdr);
                return eError;
            }
        }
    }

    eError = NvxNvMMLiteTransformWorkerBase(&pNvxAudioDecoder->oBase,
                                            s_nvx_port_input, pbMoreWork);

    if (!pPortIn->pCurrentBufferHdr)
        NvxPortGetNextHdr(pPortIn);

    if (!pPortOut->pCurrentBufferHdr)
        NvxPortGetNextHdr(pPortOut);

    return eError;
}

static OMX_ERRORTYPE NvxAudioDecoderAcquireResources(OMX_IN NvxComponent *pComponent)
{
    SNvxAudioDecoderData  *pNvxAudioDecoder = 0;
    OMX_ERRORTYPE       eError          = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAudioDecoderAcquireResources\n"));

    /* Get Aac Decoder instance */
    pNvxAudioDecoder = (SNvxAudioDecoderData *)pComponent->pComponentData;

    NV_ASSERT(pNvxAudioDecoder);

    eError = NvxComponentBaseAcquireResources(pComponent);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pComponent->eObjectType, "ERROR:NvxAudioDecoderAcquireResources:"
           ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    /* Open Aac handles and setup frames */
    eError = NvxAudioDecoderOpen( pNvxAudioDecoder, pComponent );

    return eError;
}

static OMX_ERRORTYPE NvxAudioDecoderReleaseResources(OMX_IN NvxComponent *pComponent)
{
    SNvxAudioDecoderData *    pNvxAudioDecoder  = 0;
    OMX_ERRORTYPE           eError          = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAudioDecoderReleaseResources\n"));

    /* Get Aac Decoder instance */
    pNvxAudioDecoder = (SNvxAudioDecoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxAudioDecoder);

    eError = NvxAudioDecoderClose( pNvxAudioDecoder );
    NvxCheckError(eError, NvxComponentBaseReleaseResources(pComponent));
    return eError;
}

static OMX_ERRORTYPE NvxAudioDecoderFlush(OMX_IN NvxComponent *pComponent, OMX_U32 nPort)
{
    SNvxAudioDecoderData *pNvxAudioDecoder;

    NV_ASSERT(pComponent && pComponent->pComponentData);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAudioDecoderFlush\n"));

    pNvxAudioDecoder = (SNvxAudioDecoderData *)pComponent->pComponentData;
    pNvxAudioDecoder->bSentEOS = OMX_FALSE;
    return NvxNvMMLiteTransformFlush(&pNvxAudioDecoder->oBase, nPort);
}

/* =========================================================================
 *                     I N T E R N A L
 * ========================================================================= */

OMX_ERRORTYPE NvxAudioDecoderOpen( SNvxAudioDecoderData * pNvxAudioDecoder, NvxComponent *pComponent )
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NVXTRACE(( NVXT_CALLGRAPH, NVXT_AUDIO_DECODER, "+NvxAudioDecoderOpen\n"));

    pNvxAudioDecoder->bCanSkipWorker = OMX_FALSE;
    pNvxAudioDecoder->bFirstBuffer = OMX_TRUE;

    eError = NvxNvMMLiteTransformOpen(&pNvxAudioDecoder->oBase, pNvxAudioDecoder->oBlockType, pNvxAudioDecoder->sBlockName, s_nvx_num_ports, pComponent);
    if (eError != OMX_ErrorNone)
        return eError;

    eError = NvxNvMMLiteTransformSetupInput(&pNvxAudioDecoder->oBase, s_nvx_port_input, &pComponent->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE, pNvxAudioDecoder->bCanSkipWorker);
    if (eError != OMX_ErrorNone && eError != OMX_ErrorNotReady)
        return eError;

    eError = NvxNvMMLiteTransformSetupOutput(&pNvxAudioDecoder->oBase, s_nvx_port_output, &pComponent->pPorts[s_nvx_port_output], s_nvx_port_input, MAX_OUTPUT_BUFFERS, MIN_OUTPUT_BUFFERSIZE);
    if (eError != OMX_ErrorNone && eError != OMX_ErrorNotReady)
        return eError;

    pNvxAudioDecoder->bInitialized = OMX_TRUE;
    if (pNvxAudioDecoder->bCanSkipWorker == OMX_FALSE)
    {
        NvBool SyncDecodeMode = NV_TRUE;
        pNvxAudioDecoder->oBase.hBlock->SetAttribute(pNvxAudioDecoder->oBase.hBlock,
                NvMMLiteAttributeAudioDec_SyncDecode,
                NvMMLiteSetAttrFlag_Notification,
                sizeof(NvMMLiteAudioRateInfo), &SyncDecodeMode);

    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxAudioDecoderClose(SNvxAudioDecoderData * pNvxAudioDecoder)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_AUDIO_DECODER, "+NvxAudioDecoderClose\n"));

    if (!pNvxAudioDecoder->bInitialized)
        return OMX_ErrorNone;

    eError = NvxNvMMLiteTransformClose(&pNvxAudioDecoder->oBase);

    if (eError != OMX_ErrorNone)
        return eError;

    pNvxAudioDecoder->bInitialized = OMX_FALSE;

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxAudioDecoderFillThisBuffer(NvxComponent *pNvComp, OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    SNvxAudioDecoderData *pNvxAudioDecoder = 0;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioDecoderFillThisBuffer\n"));

    pNvxAudioDecoder = (SNvxAudioDecoderData *)pNvComp->pComponentData;
    if (!pNvxAudioDecoder->bInitialized)
        return OMX_ErrorNone;

    return NvxNvMMLiteTransformFillThisBuffer(&pNvxAudioDecoder->oBase, pBufferHdr, s_nvx_port_output);
}


static OMX_ERRORTYPE NvxAudioDecoderPreChangeState(NvxComponent *pNvComp,
                                                   OMX_STATETYPE oNewState)
{
    SNvxAudioDecoderData *pNvxAudioDecoder = 0;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioDecoderPreChangeState\n"));

    pNvxAudioDecoder = (SNvxAudioDecoderData *)pNvComp->pComponentData;
    if (!pNvxAudioDecoder->bInitialized)
        return OMX_ErrorNone;

    if (pNvComp->eState == OMX_StateIdle && oNewState == OMX_StateExecuting)
    {
        pNvxAudioDecoder->bSentEOS = OMX_FALSE;
    }

    return NvxNvMMLiteTransformPreChangeState(&pNvxAudioDecoder->oBase, oNewState,
                                          pNvComp->eState);
}

static OMX_ERRORTYPE NvxAudioDecoderEmptyThisBuffer(NvxComponent *pNvComp,
                                             OMX_BUFFERHEADERTYPE* pBufferHdr,
                                             OMX_BOOL *bHandled)
{
    SNvxAudioDecoderData *pNvxAudioDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioDecoderEmptyThisBuffer\n"));

    pNvxAudioDecoder = (SNvxAudioDecoderData *)pNvComp->pComponentData;
    if (!pNvxAudioDecoder->bInitialized)
        return OMX_ErrorNone;

    if (NvxNvMMLiteTransformIsInputSkippingWorker(&pNvxAudioDecoder->oBase,
                                                  s_nvx_port_input))
    {
        eError = NvxPortEmptyThisBuffer(&pNvComp->pPorts[pBufferHdr->nInputPortIndex], pBufferHdr);
        *bHandled = (eError == OMX_ErrorNone);
    }
    else
        *bHandled = OMX_FALSE;

    return eError;
}

/******************
 * Init functions *
 ******************/
static OMX_ERRORTYPE NvxCommonAudioDecoderInit(OMX_HANDLETYPE hComponent, NvMMLiteBlockType oBlockType, const char *sBlockName)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pThis;
    SNvxAudioDecoderData *data = NULL;

    OMX_AUDIO_PARAM_PCMMODETYPE  *pNvxPcmParameters;

    eError = NvxComponentCreate(hComponent, s_nvx_num_ports, &pThis);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType,"ERROR:NvxCommonAudioDecoderInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    NvxAddResource(pThis, NVX_AUDIO_MEDIAPROCESSOR_RES);

    pThis->eObjectType = NVXT_AUDIO_DECODER;

    data = NvOsAlloc(sizeof(SNvxAudioDecoderData));
    if (!data)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType,"ERROR:NvxCommonAudioDecoderInit:"
            ":SNvxAudioData = %d:[%s(%d)]\n",data,__FILE__, __LINE__));
        return OMX_ErrorInsufficientResources;
    }

    NvOsMemset(data, 0, sizeof(SNvxAudioDecoderData));

    data->oBlockType = oBlockType;
    data->sBlockName = sBlockName;

    data->bCanSkipWorker = OMX_TRUE;
    data->bFirstBuffer = OMX_TRUE;

    pThis->pComponentData = data;

    pThis->DeInit             = NvxAudioDecoderDeInit;
    pThis->GetParameter       = NvxAudioDecoderGetParameter;
    pThis->SetParameter       = NvxAudioDecoderSetParameter;
    pThis->GetConfig          = NvxAudioDecoderGetConfig;
    pThis->SetConfig          = NvxAudioDecoderSetConfig;
    pThis->WorkerFunction     = NvxAudioDecoderWorkerFunction;
    pThis->AcquireResources   = NvxAudioDecoderAcquireResources;
    pThis->ReleaseResources   = NvxAudioDecoderReleaseResources;
    pThis->FillThisBufferCB   = NvxAudioDecoderFillThisBuffer;
    pThis->PreChangeState     = NvxAudioDecoderPreChangeState;
    pThis->EmptyThisBuffer    = NvxAudioDecoderEmptyThisBuffer;
    pThis->Flush              = NvxAudioDecoderFlush;

    pThis->pPorts[s_nvx_port_output].oPortDef.nPortIndex = s_nvx_port_output;

    NvxPortInitAudio(&pThis->pPorts[s_nvx_port_output], OMX_DirOutput, MAX_OUTPUT_BUFFERS + 5, MIN_OUTPUT_BUFFERSIZE, OMX_AUDIO_CodingPCM);

    /* Initialize default parameters - required of a standard component */
    ALLOC_STRUCT(pThis, pNvxPcmParameters, OMX_AUDIO_PARAM_PCMMODETYPE);
    if (!pNvxPcmParameters)
        return OMX_ErrorInsufficientResources;

    //Initialize the default values for the standard component requirement.
    pNvxPcmParameters->nPortIndex = pThis->pPorts[s_nvx_port_output].oPortDef.nPortIndex;
    pNvxPcmParameters->nChannels = 2;
    pNvxPcmParameters->eNumData = OMX_NumericalDataSigned;
    pNvxPcmParameters->nSamplingRate = 48000;
    pNvxPcmParameters->ePCMMode = OMX_AUDIO_PCMModeLinear;
    pNvxPcmParameters->bInterleaved = OMX_TRUE;
    pNvxPcmParameters->nBitPerSample = 16;
    pNvxPcmParameters->eChannelMapping[0] = OMX_AUDIO_ChannelLF;
    pNvxPcmParameters->eChannelMapping[1] = OMX_AUDIO_ChannelRF;

    pThis->pPorts[s_nvx_port_output].pPortPrivate = pNvxPcmParameters;

    return eError;
}

/**** MP3 ****/

OMX_ERRORTYPE NvxMp3LiteDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent *pThis;

    OMX_AUDIO_PARAM_MP3TYPE  *pNvxMp3DecoderParameters;

    eError = NvxCommonAudioDecoderInit(hComponent, NvMMLiteBlockType_DecMP3, "BlockMP3Dec");
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED,"ERROR:NvxMp3AudioDecoderInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pThis = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    pThis->pComponentName = "OMX.Nvidia.mp3.decoder";
    pThis->nComponentRoles = 1;
    pThis->sComponentRoles[0] = "audio_decoder.mp3";

    pThis->bCanUsePartialFrame = OMX_TRUE;

    NvxPortInitAudio(&pThis->pPorts[s_nvx_port_input], OMX_DirInput, MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE, OMX_AUDIO_CodingMP3);

    // Std Component must have default parameter values set.
    ALLOC_STRUCT(pThis, pNvxMp3DecoderParameters, OMX_AUDIO_PARAM_MP3TYPE);
    if (!pNvxMp3DecoderParameters)
        return OMX_ErrorInsufficientResources;

    //Initialize the default values for the standard component requirement.
    pNvxMp3DecoderParameters->nPortIndex = pThis->pPorts[s_nvx_port_input].oPortDef.nPortIndex; ; /* OMX_AUDIOPORTBASE+ 0 */
    pNvxMp3DecoderParameters->nChannels = 2;
    pNvxMp3DecoderParameters->nSampleRate = 48000;
    pNvxMp3DecoderParameters->nAudioBandWidth = 0;
    pNvxMp3DecoderParameters->eChannelMode = OMX_AUDIO_ChannelModeStereo;

    pThis->pPorts[s_nvx_port_input].pPortPrivate = pNvxMp3DecoderParameters;

    return eError;
}
/**** AAC ****/

typedef struct simpleBitstream
{
    const unsigned char *   buffer ;
    unsigned int            readBits ;
    unsigned int            dataWord ;
    unsigned int            validBits ;

} simpleBitstream, *simpleBitstreamPtr ;

static int __getBits (simpleBitstreamPtr self, const unsigned int numberOfBits)
{
    if (self->validBits <= 16)
    {
        self->validBits += 16 ;
        self->dataWord <<= 16 ;
        self->dataWord |= (unsigned int) *self->buffer++ << 8 ;
        self->dataWord |= (unsigned int) *self->buffer++ ;
    }

    self->readBits += numberOfBits ;
    self->validBits -= numberOfBits ;
    return ((self->dataWord >> self->validBits) & ((1L << numberOfBits) - 1)) ;
}

static const NvS32 SamplingRateTable[16] =
{ 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350, -1, -1, -1 };

static OMX_BOOL HandleFirstPacketAAC(NvxComponent *pThis, NvxPort *pPort,
                                     SNvxNvMMLiteTransformData *pBase)
{
    OMX_BUFFERHEADERTYPE *buffer = pPort->pCurrentBufferHdr;
    simpleBitstream decConfig;
    NvMMLiteAudioAacTrackInfo AACDecTrackInfo;
    OMX_AUDIO_PARAM_AACPROFILETYPE *pNvxAacDecoderParameters;
    SNvxAudioDecoderData *pNvxAudioDecoder = (SNvxAudioDecoderData *)pThis->pComponentData;
    NvOsMemset(&AACDecTrackInfo, 0, sizeof(NvMMLiteAudioAacTrackInfo));

    if ((sizeof(NvMMLiteAudioAacTrackInfo) == buffer->nFilledLen) ||
        (buffer->nFilledLen == 0))
        return OMX_FALSE;

    decConfig.validBits = decConfig.readBits = decConfig.dataWord = 0 ;
    decConfig.buffer = (const unsigned char*)(buffer->pBuffer);
    pNvxAacDecoderParameters = (OMX_AUDIO_PARAM_AACPROFILETYPE *)pPort->pPortPrivate;
    if (buffer->nFlags & OMX_BUFFERFLAG_CODECCONFIG)
    {
        NvU32 tmp;
        tmp = __getBits(&decConfig, 5);
        if (tmp == OMX_AUDIO_AACObjectHE || tmp == OMX_AUDIO_AACObjectHE_PS)
        {
            pNvxAacDecoderParameters->eAACProfile = tmp;
        }
        else
        {
            pNvxAacDecoderParameters->eAACProfile  = OMX_AUDIO_AACObjectLC;
        }

        AACDecTrackInfo.objectType = pNvxAacDecoderParameters->eAACProfile;
        AACDecTrackInfo.profile = 0x40;
        pNvxAudioDecoder->samplingFreqIndex = AACDecTrackInfo.samplingFreqIndex = tmp = __getBits(&decConfig, 4);

        if (tmp == 0x0F)
            AACDecTrackInfo.samplingFreq = __getBits(&decConfig, 24);
        else
            AACDecTrackInfo.samplingFreq = SamplingRateTable[tmp];
    }
    else
    {
        pNvxAacDecoderParameters->eAACProfile  = OMX_AUDIO_AACObjectLC;
        AACDecTrackInfo.objectType = pNvxAacDecoderParameters->eAACProfile;
        AACDecTrackInfo.samplingFreq = 44100;
        pNvxAudioDecoder->samplingFreqIndex =4;
        AACDecTrackInfo.samplingFreqIndex =4;
    }

    pNvxAacDecoderParameters->nSampleRate = AACDecTrackInfo.samplingFreq;
    pBase->hBlock->SetAttribute(pBase->hBlock,
                                NvMMLiteAudioAacAttribute_TrackInfo,
                                0,
                                sizeof(NvMMLiteAudioAacTrackInfo),
                                &AACDecTrackInfo);
    return OMX_TRUE;
}


/**** eAACPlus ****/

OMX_ERRORTYPE NvxLiteEAacPlusDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pThis = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_AUDIO_PARAM_AACPROFILETYPE  *pNvxAacDecoderParameters;
    SNvxAudioDecoderData *data = NULL;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxEAacPlusDecoderInit\n"));

    eError = NvxCommonAudioDecoderInit(hComponent, NvMMLiteBlockType_DecEAACPLUS, "BlockAACDec");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxEAacPlusDecoderInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pThis = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    data = (SNvxAudioDecoderData *)pThis->pComponentData;

    /* Register function pointers */
    pThis->pComponentName = "OMX.Nvidia.eaacp.decoder";
    pThis->nComponentRoles = 3;
    pThis->sComponentRoles[0] = "audio_decoder.eaacplus";
    pThis->sComponentRoles[1] = "audio_decoder.aac";
    pThis->sComponentRoles[2] = "audio_decoder.aac.secure";

    data->HandleFirstPacket = HandleFirstPacketAAC;

    pThis->pPorts[s_nvx_port_input].oPortDef.nPortIndex = 0;

    ALLOC_STRUCT(pThis, pNvxAacDecoderParameters, OMX_AUDIO_PARAM_AACPROFILETYPE);
    if (!pNvxAacDecoderParameters)
        return OMX_ErrorInsufficientResources;

    //Initialize the default values for the standard component requirement.
    pNvxAacDecoderParameters->nPortIndex = pThis->pPorts[s_nvx_port_input].oPortDef.nPortIndex; ; /* OMX_AUDIOPORTBASE+ 0 */
    pNvxAacDecoderParameters->nChannels = 2;
    pNvxAacDecoderParameters->nSampleRate = 44100;
    pNvxAacDecoderParameters->nBitRate = 288000;  // This is the minimum.
    pNvxAacDecoderParameters->nAudioBandWidth = 0;
    pNvxAacDecoderParameters->nFrameLength = 0;
    pNvxAacDecoderParameters->eAACProfile = OMX_AUDIO_AACObjectLC;
    pNvxAacDecoderParameters->eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP2ADTS;
    pNvxAacDecoderParameters->eChannelMode = OMX_AUDIO_ChannelModeStereo;

    pThis->pPorts[s_nvx_port_input].pPortPrivate = pNvxAacDecoderParameters;

    NvxPortInitAudio(&pThis->pPorts[s_nvx_port_input], OMX_DirInput, MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE, OMX_AUDIO_CodingAAC);

    return eError;
}

/*** WMA ***/

OMX_ERRORTYPE NvxLiteWmaDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pThis = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_AUDIO_PARAM_WMATYPE  *pNvxWmaDecoderParameters;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxAudioDecoderInit\n"));

    eError = NvxCommonAudioDecoderInit(hComponent, NvMMLiteBlockType_DecWMA, "BlockWMADec");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxAudioDecoderInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pThis = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    /* Register function pointers */
    pThis->pComponentName = "OMX.Nvidia.wma.decoder";
    pThis->nComponentRoles = 1;
    pThis->sComponentRoles[0] = "audio_decoder.wma";

    pThis->pPorts[s_nvx_port_input].oPortDef.nPortIndex = 0;

    ALLOC_STRUCT(pThis, pNvxWmaDecoderParameters, OMX_AUDIO_PARAM_WMATYPE);
    if (!pNvxWmaDecoderParameters)
        return OMX_ErrorInsufficientResources;

    //Initialize the default values for the standard component requirement.
    pNvxWmaDecoderParameters->nPortIndex = pThis->pPorts[s_nvx_port_input].oPortDef.nPortIndex; ; /* OMX_AUDIOPORTBASE+ 0 */
    pNvxWmaDecoderParameters->nChannels = 2;
    pNvxWmaDecoderParameters->nSamplingRate = 44100;
    pNvxWmaDecoderParameters->nBitRate = 196000;
    pNvxWmaDecoderParameters->eProfile = OMX_AUDIO_WMAFormatUnused;
    pThis->pPorts[s_nvx_port_input].pPortPrivate = pNvxWmaDecoderParameters;

    NvxPortInitAudio(&pThis->pPorts[s_nvx_port_input], OMX_DirInput, MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE, OMX_AUDIO_CodingWMA);

    return eError;
}

