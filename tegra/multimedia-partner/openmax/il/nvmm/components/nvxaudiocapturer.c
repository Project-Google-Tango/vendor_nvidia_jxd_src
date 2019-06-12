/* Copyright (c) 2006 NVIDIA Corporation.  All rights reserved.
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
#include "nvrm_init.h"
#include "nvrm_memmgr.h"
#include "nvmm.h"
#include "nvmm_mixer.h"

#include "nvos.h"
#include "nvassert.h"

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */
enum { s_AGC_CLPPINGTH = 15000 };
enum { s_AGC_LOWERCOMPTH = 6000 };
enum { s_AGC_UPPERCOMPTH = 12000 };
enum { s_AGC_NOISEGATETH = 500 };

#define TIMEOUT_BEFORE_CAPTURE_STARTED        431   // about 10 seconds for 44100Hz
#define TIMEOUT_AFTER_CAPTURE_STARTED           8   // mixer has 8 frames of buffer

typedef struct {
    OMX_BOOL   bInitialized;
    OMX_BOOL   bErrorReportingEnabled;
    OMX_BOOL   bCapturing;
    OMX_BOOL   bFatalError;
    OMX_BOOL   bCapturingStarted;

    OMX_BOOL   bNeedSendEOS;
    OMX_BOOL   bHasBufferToDeliver;
    OMX_BOOL   bReadSizeFromStream;
    OMX_U32    nAMBufferSize;
    OMX_TICKS  nTimeCount;

    OMX_AUDIO_PARAM_PCMMODETYPE  oNvxPcmParameters;
    OMX_U32    nFrameDuration;
    OMX_U64    nCurrentTS;

    NvRmDeviceHandle             hRmDevice;
    NvRmMemHandle                hRmMem;
    NvU32                        StreamIndex;
    OMX_BOOL                     bForceSpdif;
    NVX_CONFIG_AUDIO_DRCPROPERTY oAgcProperty;
    NVX_CONFIG_AUDIO_PARAEQFILTER oParaEqFilter;
} SNvxAudioCapturerData;

#define AUDIO_CAPTURER_BUFFER_COUNT   20
#define AUDIO_CAPTURER_BUFFER_SIZE    2048

static const int s_nvx_num_ports               = 2;
static const int s_nvx_port_output             = 0;
static const int s_nvx_port_clock              = 1;

#define INIT_STRUCT(_X_, pNvComp)     (((_X_).nSize = sizeof(_X_)), (_X_).nVersion = (pNvComp)->oSpecVersion)

OMX_ERRORTYPE NvxAudioCapturerInit(OMX_IN OMX_HANDLETYPE hComponent);


static OMX_ERRORTYPE NvxAudioCapturerDeInit(NvxComponent* pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    SNvxAudioCapturerData *pNvxAudioCapturer = (SNvxAudioCapturerData *)pNvComp->pComponentData;

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "%s\n", "--NvxAudioCapturerDeInit"));

    if (pNvxAudioCapturer)
    {
        NvOsFree(pNvxAudioCapturer);
        pNvComp->pComponentData = 0;
    }

    return eError;
}

static OMX_ERRORTYPE NvxAudioCapturerGetParameter(OMX_IN NvxComponent *pNvComp,
                                                  OMX_IN OMX_INDEXTYPE nIndex,
                                                  OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    SNvxAudioCapturerData *pNvxAudioCapturer;

    NV_ASSERT(pNvComp && pComponentParameterStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioCapturerGetParameter\n"));

    pNvxAudioCapturer = (SNvxAudioCapturerData *)pNvComp->pComponentData;
    NV_ASSERT(pNvxAudioCapturer);

    switch (nIndex)
    {
        case OMX_IndexParamAudioPcm:
        {
            NvOsMemcpy(pComponentParameterStructure, &pNvxAudioCapturer->oNvxPcmParameters, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
            break;
        }

        default:
            return NvxComponentBaseGetParameter( pNvComp, nIndex, pComponentParameterStructure);
    }

    return OMX_ErrorNone;
}


static OMX_ERRORTYPE NvxAudioCapturerSetParameter(OMX_IN NvxComponent *pNvComp,
                                                  OMX_IN OMX_INDEXTYPE nIndex,
                                                  OMX_IN OMX_PTR pComponentParameterStructure)
{
    SNvxAudioCapturerData *pNvxAudioCapturer;

    NV_ASSERT(pNvComp && pComponentParameterStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioCapturerSetParameter\n"));

    pNvxAudioCapturer = (SNvxAudioCapturerData *)pNvComp->pComponentData;
    NV_ASSERT(pNvxAudioCapturer);

    switch (nIndex)
    {
        case OMX_IndexParamAudioPcm:
        {
            NvOsMemcpy(&pNvxAudioCapturer->oNvxPcmParameters, pComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
            break;
        }

        default:
            return NvxComponentBaseSetParameter(pNvComp, nIndex, pComponentParameterStructure);
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxAudioCapturerGetConfig(OMX_IN NvxComponent* pNvComp,
                                               OMX_IN OMX_INDEXTYPE nIndex,
                                               OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    SNvxAudioCapturerData *pNvxAudioCapturer;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioCapturerGetConfig\n"));

    pNvxAudioCapturer = (SNvxAudioCapturerData *)pNvComp->pComponentData;
    NV_ASSERT(pNvxAudioCapturer);

    if (!pNvxAudioCapturer->bInitialized)
        return OMX_ErrorNotReady;

    switch (nIndex)
    {
        case OMX_IndexConfigCapturing:
        {
            OMX_CONFIG_BOOLEANTYPE *pcc = (OMX_CONFIG_BOOLEANTYPE *)pComponentConfigStructure;
            pcc->bEnabled = pNvxAudioCapturer->bCapturing;
            break;
        }

        default:
            eError = NvxComponentBaseGetConfig(pNvComp, nIndex, pComponentConfigStructure);
        break;
    }

    return eError;
}

static OMX_ERRORTYPE s_AudioCapturerStart(SNvxAudioCapturerData *pNvxAudioCapturer)
{
    NV_ASSERT(pNvxAudioCapturer);

    if (!pNvxAudioCapturer->bInitialized)
        return OMX_ErrorNone;

    pNvxAudioCapturer->nCurrentTS = (OMX_U64)NvOsGetTimeUS();

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE s_AudioCapturerStop(SNvxAudioCapturerData *pNvxAudioCapturer)
{
    NV_ASSERT(pNvxAudioCapturer);

    if (!pNvxAudioCapturer->bInitialized)
        return OMX_ErrorNone;

    return OMX_ErrorNone;
}

static void s_SetAgcProperty(SNvxAudioCapturerData *pNvxAudioCapturer)
{
    if (!pNvxAudioCapturer->bInitialized)
        return;

    return;
}

static void s_SetParaEqFilter(SNvxAudioCapturerData *pNvxAudioCapturer)
{
    NvMMMixerStreamParaEQFilterProperty oParaEqFilterProperty;

    if (!pNvxAudioCapturer->bInitialized)
        return;

    oParaEqFilterProperty.structSize = sizeof(NvMMMixerStreamParaEQFilterProperty);
    oParaEqFilterProperty.bEnable = pNvxAudioCapturer->oParaEqFilter.bEnable;
    oParaEqFilterProperty.Direction = NvMMMixerStream_Output;

    if (oParaEqFilterProperty.bEnable)
    {
        int k;
        oParaEqFilterProperty.bUpdate = NV_TRUE;
        for (k = 0; k < MIXER_NUM_PARA_EQ_FILTERS; k++)
        {
            oParaEqFilterProperty.FilterType[k] = pNvxAudioCapturer->oParaEqFilter.FilterType[k];
            oParaEqFilterProperty.F0[k]         = pNvxAudioCapturer->oParaEqFilter.F0[k];
            oParaEqFilterProperty.BndWdth[k]    = pNvxAudioCapturer->oParaEqFilter.BndWdth[k];

            if(pNvxAudioCapturer->oParaEqFilter.dBGain[k] > 1200)
                oParaEqFilterProperty.dBGain[k] = 1200;
            else if (pNvxAudioCapturer->oParaEqFilter.dBGain[k] < -1800)
                oParaEqFilterProperty.dBGain[k] = -1800;
            else
                oParaEqFilterProperty.dBGain[k] = pNvxAudioCapturer->oParaEqFilter.dBGain[k];
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

    return;
}


static OMX_ERRORTYPE NvxAudioCapturerSetConfig(OMX_IN NvxComponent* pNvComp,
                                               OMX_IN OMX_INDEXTYPE nIndex,
                                               OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    SNvxAudioCapturerData *pNvxAudioCapturer;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioCapturerSetConfig\n"));

    pNvxAudioCapturer = (SNvxAudioCapturerData *)pNvComp->pComponentData;
    NV_ASSERT(pNvxAudioCapturer);

    switch (nIndex)
    {
        case OMX_IndexConfigCapturing:
        {
            OMX_CONFIG_BOOLEANTYPE *pcc = (OMX_CONFIG_BOOLEANTYPE *)pComponentConfigStructure;

            if (pcc->bEnabled != pNvxAudioCapturer->bCapturing)
            {
                // enable/disable capturing here;
                if (pcc->bEnabled)
                {
                    s_AudioCapturerStart(pNvxAudioCapturer);
                }
                else
                {
                    s_AudioCapturerStop(pNvxAudioCapturer);
                    pNvxAudioCapturer->bNeedSendEOS = OMX_TRUE;
                    pNvxAudioCapturer->bFatalError = OMX_FALSE;
                }

                pNvxAudioCapturer->bCapturing = pcc->bEnabled;
            }
            break;
        }

        case NVX_IndexConfigAudioDrcProperty:
        {
            NVX_CONFIG_AUDIO_DRCPROPERTY *pAgcProp = (NVX_CONFIG_AUDIO_DRCPROPERTY *)pComponentConfigStructure;

            NvOsMemcpy(&pNvxAudioCapturer->oAgcProperty, pAgcProp, sizeof(NVX_CONFIG_AUDIO_DRCPROPERTY));
            s_SetAgcProperty(pNvxAudioCapturer);
            break;
        }

        case NVX_IndexConfigAudioParaEqFilter:
        {
            NVX_CONFIG_AUDIO_PARAEQFILTER *pParaEqFilter = (NVX_CONFIG_AUDIO_PARAEQFILTER *)pComponentConfigStructure;

            NvOsMemcpy(&pNvxAudioCapturer->oParaEqFilter, pParaEqFilter, sizeof(NVX_CONFIG_AUDIO_PARAEQFILTER));
            s_SetParaEqFilter(pNvxAudioCapturer);
            break;
        }

        default:
            eError =  NvxComponentBaseSetConfig(pNvComp, nIndex, pComponentConfigStructure);
        break;
    }

    return eError;
}

static OMX_ERRORTYPE NvxAudioCapturerOpen(SNvxAudioCapturerData *pNvxAudioCapturer, NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    pNvxAudioCapturer->nTimeCount = 0;
    pNvxAudioCapturer->nFrameDuration = (pNvxAudioCapturer->nAMBufferSize * 1000000) /
                                        (pNvxAudioCapturer->oNvxPcmParameters.nSamplingRate *
                                         (pNvxAudioCapturer->oNvxPcmParameters.nBitPerSample >> 3) *
                                          pNvxAudioCapturer->oNvxPcmParameters.nChannels);
    pNvxAudioCapturer->bInitialized = OMX_TRUE;

    s_SetAgcProperty(pNvxAudioCapturer);
    s_SetParaEqFilter(pNvxAudioCapturer);

    return eError;

}

static OMX_ERRORTYPE NvxAudioCapturerAcquireResources(OMX_IN NvxComponent *pNvComp)
{
    SNvxAudioCapturerData *pNvxAudioCapturer;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioCapturerAcquireResources\n"));

    pNvxAudioCapturer = (SNvxAudioCapturerData *)pNvComp->pComponentData;
    NV_ASSERT(pNvxAudioCapturer);

    eError = NvxComponentBaseAcquireResources(pNvComp);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR, pNvComp->eObjectType, "ERROR:NvxAudioCapturerAcquireResources:"
           ":eError = %x:[%s(%d)]\n", eError, __FILE__, __LINE__));
        return eError;
    }

    eError = NvxAudioCapturerOpen(pNvxAudioCapturer, pNvComp);

    return eError;
}

static OMX_ERRORTYPE NvxAudioCapturerWorkerFunction(
    OMX_IN NvxComponent *pNvComp,
    OMX_IN OMX_BOOL     bAllPortsReady,            /* OMX_TRUE if all ports have a buffer ready */
    OMX_OUT OMX_BOOL    *pbMoreWork,      /* function should set to OMX_TRUE if there is more work */
    OMX_INOUT NvxTimeMs *puMaxMsecToNextCall)
{
    SNvxAudioCapturerData *pNvxAudioCapturer;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortOutCapture = &pNvComp->pPorts[s_nvx_port_output];
    NvxPort *pPortClock = &pNvComp->pPorts[s_nvx_port_clock];
    OMX_BUFFERHEADERTYPE *pBuffer;

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioCapturerWorkerFunction\n"));

    pNvxAudioCapturer = (SNvxAudioCapturerData *)pNvComp->pComponentData;

    if (pPortClock && pPortClock->pCurrentBufferHdr)
    {
        NvxPortReleaseBuffer(pPortClock, pPortClock->pCurrentBufferHdr);
    }

    //if (!bAllPortsReady || !pPortOutCapture->pCurrentBufferHdr)
    *pbMoreWork = pPortOutCapture->pCurrentBufferHdr && (pNvxAudioCapturer->bCapturing || pNvxAudioCapturer->bNeedSendEOS);
    if (!(*pbMoreWork))
        return OMX_ErrorNone;

    pBuffer = pPortOutCapture->pCurrentBufferHdr;

    if (pNvxAudioCapturer->bFatalError)
    {
        *pbMoreWork = OMX_FALSE;
        return OMX_ErrorNone;
    }

    if (!pNvxAudioCapturer->bHasBufferToDeliver)
    {
        if (pNvxAudioCapturer->bCapturing)
        {
            OMX_S32 askBytes = (OMX_S32)(pBuffer->nAllocLen - pBuffer->nFilledLen);

            while (askBytes > 0)
            {
                NvError Error = NvSuccess;
                int nTries = 0;
                int threshold;

                do {
                    threshold = pNvxAudioCapturer->bCapturingStarted ?
                                TIMEOUT_AFTER_CAPTURE_STARTED : TIMEOUT_BEFORE_CAPTURE_STARTED;

                    if (nTries > threshold)
                    {
                        pNvxAudioCapturer->bFatalError = OMX_TRUE;
                        NvOsDebugPrintf("NvxAudioCapturerWorkerFunction: hit fatal error\n");

                        if (pNvComp->pCallbacks && pNvComp->pCallbacks->EventHandler)
                            pNvComp->pCallbacks->EventHandler(pNvComp->hBaseComponent,
                                                              pNvComp->pCallbackAppData,
                                                              OMX_EventError,
                                                              OMX_ErrorUndefined, 0, 0);

                        break;
                    }

                    if (Error != NvSuccess)
                    {
                        nTries++;
                    }
                    else
                    {
                        pNvxAudioCapturer->bCapturingStarted = OMX_TRUE;
                    }

                } while (Error != NvSuccess);

                if (pNvxAudioCapturer->bFatalError)
                    break;

                pPortOutCapture->pCurrentBufferHdr->nTimeStamp = (OMX_TICKS)pNvxAudioCapturer->nCurrentTS;
                pNvxAudioCapturer->nCurrentTS += (OMX_U64)pNvxAudioCapturer->nFrameDuration;
            }

            pBuffer->nFilledLen = pBuffer->nAllocLen;
            *pbMoreWork = OMX_TRUE;
        }
        else if (pNvxAudioCapturer->bNeedSendEOS)
        {
            pBuffer->nFilledLen = 0;
            pBuffer->nFlags = OMX_BUFFERFLAG_EOS;
            *pbMoreWork = OMX_FALSE;
            pNvxAudioCapturer->bNeedSendEOS = OMX_FALSE;
        }
    }

    eError = NvxPortDeliverFullBuffer(pPortOutCapture, pPortOutCapture->pCurrentBufferHdr);
    if (eError == OMX_ErrorInsufficientResources)
    {
        pNvxAudioCapturer->bHasBufferToDeliver = OMX_TRUE;
        NVXTRACE((NVXT_ERROR, pNvComp->eObjectType, "ERROR:NvxAudioCapturerWorkerFunction:"
            ":eError = %x :[%s(%d)]\n",eError, __FILE__, __LINE__));
        return eError;
    }
    pNvxAudioCapturer->bHasBufferToDeliver = OMX_FALSE;

    if (!pPortOutCapture->pCurrentBufferHdr)
        NvxPortGetNextHdr(pPortOutCapture);

    *pbMoreWork |= !!(pPortOutCapture->pCurrentBufferHdr);

    return eError;
}

static OMX_ERRORTYPE NvxAudioCapturerClose(SNvxAudioCapturerData *pNvxAudioCapturer)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_AUDIO_RECORDER, "+NvxAudioCapturerClose\n"));

    if (!pNvxAudioCapturer->bInitialized)
        return OMX_ErrorNone;

    if(pNvxAudioCapturer->hRmDevice)
    {
        NvRmClose(pNvxAudioCapturer->hRmDevice);
        pNvxAudioCapturer->hRmDevice = 0;
    }

    pNvxAudioCapturer->bInitialized = OMX_FALSE;

    return eError;
}

static OMX_ERRORTYPE NvxAudioCapturerReleaseResources(OMX_IN NvxComponent *pNvComp)
{
    SNvxAudioCapturerData *pNvxAudioCapturer;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioCapturerReleaseResources\n"));

    pNvxAudioCapturer = (SNvxAudioCapturerData *)pNvComp->pComponentData;
    NV_ASSERT(pNvxAudioCapturer);

    eError = NvxAudioCapturerClose(pNvxAudioCapturer);
    NvxCheckError(eError, NvxComponentBaseReleaseResources(pNvComp));
    return eError;
}

OMX_ERRORTYPE NvxAudioCapturerInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent *pNvComp = 0;
    SNvxAudioCapturerData *pNvxAudioCapturer = NULL;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxAudioCapturerInit\n"));

    eError = NvxComponentCreate(hComponent, s_nvx_num_ports, &pNvComp);
    if (!NvxIsSuccess((NvS32)eError))
    {
        NVXTRACE((NVXT_ERROR, NVXT_AUDIO_RECORDER, "ERROR:NvxAudioCapturerInit:"
            ":eError = %d:[%s(%d)]\n", eError, __FILE__, __LINE__));
        return eError;
    }

    pNvComp->pComponentData = NvOsAlloc(sizeof(SNvxAudioCapturerData));
    if (!pNvComp->pComponentData)
    {
        NVXTRACE((NVXT_ERROR, NVXT_AUDIO_RECORDER, "ERROR:NvxAudioCapturerInit:"
          ":pComponentData = %p:[%s(%d)]\n", pNvComp->pComponentData, __FILE__, __LINE__));
        return OMX_ErrorInsufficientResources;
    }

    pNvxAudioCapturer = (SNvxAudioCapturerData *)pNvComp->pComponentData;
    NvOsMemset(pNvxAudioCapturer, 0, sizeof(SNvxAudioCapturerData));

    pNvComp->eObjectType = NVXT_AUDIO_RECORDER;

    // Register function pointers
    pNvComp->DeInit             = NvxAudioCapturerDeInit;
    pNvComp->GetParameter       = NvxAudioCapturerGetParameter;
    pNvComp->SetParameter       = NvxAudioCapturerSetParameter;
    pNvComp->GetConfig          = NvxAudioCapturerGetConfig;
    pNvComp->SetConfig          = NvxAudioCapturerSetConfig;
    pNvComp->WorkerFunction     = NvxAudioCapturerWorkerFunction;
    pNvComp->AcquireResources   = NvxAudioCapturerAcquireResources;
    pNvComp->ReleaseResources   = NvxAudioCapturerReleaseResources;

    pNvComp->pComponentName = "OMX.Nvidia.audio.capturer";
    pNvComp->nComponentRoles = 1;
    pNvComp->sComponentRoles[0] = "audio_capturer.pcm";

    // setup clock port
    NvxPortInitOther(&pNvComp->pPorts[s_nvx_port_clock], OMX_DirInput, 4,
                     sizeof(OMX_TIME_MEDIATIMETYPE), OMX_OTHER_FormatTime);
    pNvComp->pPorts[s_nvx_port_clock].eNvidiaTunnelTransaction = ENVX_TRANSACTION_CLOCK;

    // setup capture port
    NvxPortInitAudio(&pNvComp->pPorts[s_nvx_port_output], OMX_DirOutput,
                     AUDIO_CAPTURER_BUFFER_COUNT, AUDIO_CAPTURER_BUFFER_SIZE,
                     OMX_AUDIO_CodingPCM);
    pNvComp->pPorts[s_nvx_port_output].eNvidiaTunnelTransaction = ENVX_TRANSACTION_AUDIO;

    pNvxAudioCapturer->nAMBufferSize = AUDIO_CAPTURER_BUFFER_SIZE;

    INIT_STRUCT(pNvxAudioCapturer->oNvxPcmParameters, pNvComp);
    pNvxAudioCapturer->oNvxPcmParameters.nPortIndex = s_nvx_port_output;
    pNvxAudioCapturer->oNvxPcmParameters.nChannels = 2;
    pNvxAudioCapturer->oNvxPcmParameters.eNumData = OMX_NumericalDataSigned;
    pNvxAudioCapturer->oNvxPcmParameters.eEndian = OMX_EndianLittle;
    pNvxAudioCapturer->oNvxPcmParameters.bInterleaved = OMX_TRUE;
    pNvxAudioCapturer->oNvxPcmParameters.nBitPerSample = 16;
    pNvxAudioCapturer->oNvxPcmParameters.nSamplingRate = 44100;
    pNvxAudioCapturer->oNvxPcmParameters.ePCMMode = OMX_AUDIO_PCMModeLinear;
    pNvxAudioCapturer->oNvxPcmParameters.eChannelMapping[0]= OMX_AUDIO_ChannelLF;
    pNvxAudioCapturer->oNvxPcmParameters.eChannelMapping[1]= OMX_AUDIO_ChannelRF;
    pNvxAudioCapturer->nCurrentTS = (OMX_U64)1;

    pNvxAudioCapturer->oAgcProperty.EnableDrc = 0;
    pNvxAudioCapturer->oAgcProperty.ClippingTh = s_AGC_CLPPINGTH;
    pNvxAudioCapturer->oAgcProperty.LowerCompTh = s_AGC_LOWERCOMPTH;
    pNvxAudioCapturer->oAgcProperty.UpperCompTh = s_AGC_UPPERCOMPTH;
    pNvxAudioCapturer->oAgcProperty.NoiseGateTh = s_AGC_NOISEGATETH;

    return eError;
}

