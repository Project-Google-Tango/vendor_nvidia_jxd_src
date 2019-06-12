/*
 * Copyright (c) 2011-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** NvxAlsaCapture.c */

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
#define MAX_OUTPUT_BUFFERS    10
#define MIN_OUTPUT_BUFFERSIZE 2048


/* Number of internal frame buffers (AUDIO memory) */
static const int s_nvx_num_ports            = 2;
static const int s_nvx_port_output           = 0;
static const int s_nvx_port_clock           = 1;

/* Audio Renderer State information */
typedef struct
{
    snd_pcm_t *capture_handle;
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
    OMX_U32    nFrameDuration;
    OMX_TICKS  nCurrentTS;
    OMX_AUDIO_PARAM_PCMMODETYPE  oNvxPcmParameters;

    snd_pcm_t   *pNativeRender;

}SNvxAlsaCaptureData;

static OMX_ERRORTYPE NvxAlsaCapturePreChangeState(NvxComponent *pNvComp,OMX_STATETYPE oNewState);
static OMX_ERRORTYPE NvxAlsaCaptureOpen(SNvxAlsaCaptureData * pNvxAudio, NvxComponent *pComponent);
static OMX_ERRORTYPE NvxAlsaCaptureClose(SNvxAlsaCaptureData *pNvxAudio);
static OMX_ERRORTYPE NvxAlsaCaptureDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAlsaCaptureInit\n"));

    NvOsFree(pNvComp->pComponentData);
    pNvComp->pComponentData = 0;
    return eError;
}

static OMX_ERRORTYPE NvxAlsaCaptureGetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    SNvxAlsaCaptureData *pNvxAudio;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAlsaCaptureGetParameter\n"));

    pNvxAudio = (SNvxAlsaCaptureData *)pComponent->pComponentData;

    switch (nParamIndex)
    {
        case OMX_IndexParamAudioPcm:
        {
            NvxPort *pPortOut = &pComponent->pPorts[s_nvx_port_output];
            OMX_AUDIO_PARAM_PCMMODETYPE *pLocalMode = (OMX_AUDIO_PARAM_PCMMODETYPE *)pPortOut->pPortPrivate;
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


static OMX_ERRORTYPE NvxAlsaSetAudioParams(SNvxAlsaCaptureData *pNvxAudio, OMX_AUDIO_PARAM_PCMMODETYPE *pLocalMode )
{
    unsigned int rate;
    snd_pcm_uframes_t period_size,buffer_size;
    snd_pcm_format_t snd_pcm_format = SND_PCM_FORMAT_UNKNOWN;

    if(pNvxAudio->bConfigured)
       return OMX_ErrorNone;

    if(snd_pcm_hw_params_set_channels(pNvxAudio->capture_handle,pNvxAudio->hw_params,pLocalMode->nChannels))
    {
        NvOsDebugPrintf("NvxAlsaCaptureSetParameter: Error setting nChannels %d\n",pLocalMode->nChannels);
        return OMX_ErrorBadParameter;
    }
    pNvxAudio->oNvxPcmParameters.nChannels = pLocalMode->nChannels;
    if(pLocalMode->bInterleaved == OMX_TRUE)
    {
        if(snd_pcm_hw_params_set_access(pNvxAudio->capture_handle,pNvxAudio->hw_params,SND_PCM_ACCESS_RW_INTERLEAVED)<0)
        {
            NvOsDebugPrintf("NvxAlsaCaptureSetParameter: Error Setting Interleaved RW \n");
            return OMX_ErrorHardware;
        }
    }
    else
    {
        if(snd_pcm_hw_params_set_access(pNvxAudio->capture_handle,pNvxAudio->hw_params,SND_PCM_ACCESS_RW_NONINTERLEAVED)<0)
        {
            NvOsDebugPrintf("NvxAlsaCaptureSetParameter: Error Setting Non-Interleaved RW \n");
            return OMX_ErrorHardware;
        }
    }
    rate = pLocalMode->nSamplingRate;
    if(snd_pcm_hw_params_set_rate_near(pNvxAudio->capture_handle,pNvxAudio->hw_params,&rate,0)<0)
    {
        NvOsDebugPrintf("NvxAlsaCaptureSetParameter: Error Setting rate \n");
        return OMX_ErrorHardware;
    }
    else
    {
        //NvOsDebugPrintf("Alsa Sampling rate set near %lu \n",rate);
    }
    pNvxAudio->nSamplingRate = rate;
    pNvxAudio->oNvxPcmParameters.nSamplingRate = rate;
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
    pNvxAudio->oNvxPcmParameters.nBitPerSample = pLocalMode->nBitPerSample;
    if(snd_pcm_format != SND_PCM_FORMAT_UNKNOWN)
    {
        if(snd_pcm_hw_params_set_format(pNvxAudio->capture_handle,pNvxAudio->hw_params,snd_pcm_format)<0)
        {
            NvOsDebugPrintf("NvxAlsaCaptureSetParameter: Error Setting NumBitsPerSample\n");
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
    if(snd_pcm_hw_params_set_period_size_near(pNvxAudio->capture_handle,pNvxAudio->hw_params,\
        &period_size,0)<0)
    {
        NvOsDebugPrintf("Warning NvxAlsaCaptureSetParameter: Period Size is not set to %d\n",period_size);
    }
    if(snd_pcm_hw_params_set_buffer_size_near(pNvxAudio->capture_handle,pNvxAudio->hw_params,\
        &buffer_size)<0)
    {

        NvOsDebugPrintf("Warning NvxAlsaCaptureSetParameter: Buffer Size is not set to %d\n",buffer_size);
    }

    //Configure and Prepare ALSA
    if(snd_pcm_hw_params(pNvxAudio->capture_handle,pNvxAudio->hw_params)<0)
    {
        NvOsDebugPrintf("NvxAlsaCaptureSetParameter: Cannont Configure Alsa \n");
        return OMX_ErrorHardware;
    }
    if(snd_pcm_prepare(pNvxAudio->capture_handle)<0)
    {
        NvOsDebugPrintf("NvxAlsaCaptureSetParameter: Cannont Prepare Alsa \n");
        return OMX_ErrorHardware;
    }
    pNvxAudio->bConfigured = OMX_TRUE;
    return OMX_ErrorNone;

}

static OMX_ERRORTYPE NvxAlsaCaptureSetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentParameterStructure)
{
    SNvxAlsaCaptureData *pNvxAudio;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAlsaCaptureSetParameter\n"));

    pNvxAudio = (SNvxAlsaCaptureData *)pComponent->pComponentData;

    switch(nIndex)
    {
        case OMX_IndexParamAudioPcm:
        {
            NvxPort *pPortOut = &pComponent->pPorts[s_nvx_port_output];
            OMX_AUDIO_PARAM_PCMMODETYPE *pLocalMode = (OMX_AUDIO_PARAM_PCMMODETYPE *)pPortOut->pPortPrivate;
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

static OMX_ERRORTYPE NvxAlsaCaptureGetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    SNvxAlsaCaptureData *data;

    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioRenderGetConfig\n"));

    data = (SNvxAlsaCaptureData *)pNvComp->pComponentData;

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
        default:
            eError = NvxComponentBaseGetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return eError;
}

static OMX_ERRORTYPE NvxAlsaCaptureSetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentConfigStructure)
{
    //NVX_CONFIG_TIMESTAMPTYPE *pTimestamp;
    SNvxAlsaCaptureData *pNvxAudio;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAlsaCaptureSetConfig\n"));

    pNvxAudio = (SNvxAlsaCaptureData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
        case  NVX_IndexConfigAudioOutputType:
        {
            NVX_CONFIG_AUDIOOUTPUT *ot = (NVX_CONFIG_AUDIOOUTPUT *)pComponentConfigStructure;
            pNvxAudio->eOutputType = ot->eOutputType;
            break;
        }
        default:
            eError = NvxComponentBaseSetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return eError;
}

static OMX_ERRORTYPE NvxAlsaCaptureToBuffer(SNvxAlsaCaptureData *pNvxAudio,OMX_BUFFERHEADERTYPE *pOMXBuf,OMX_AUDIO_PARAM_PCMMODETYPE *pLocalMode,OMX_BOOL *pbWouldFail)
{
    OMX_U32 frameSize;
    OMX_S32 written;
    OMX_S32 totalBuffer;
    OMX_S32 offsetBuffer;
    OMX_BOOL allDataSent;
    *pbWouldFail = OMX_FALSE;
    frameSize = pLocalMode->nChannels * (pLocalMode->nBitPerSample >> 3);
    allDataSent = OMX_FALSE;
    totalBuffer = pOMXBuf->nAllocLen / frameSize;
    offsetBuffer = 0;
    pOMXBuf->nFilledLen = 0;
    while(!allDataSent)
    {
        written = snd_pcm_readi(pNvxAudio->capture_handle,pOMXBuf->pBuffer + (offsetBuffer * frameSize),totalBuffer);
        if(written < 0)
        {
            if(written == -EPIPE)
            {
                NvOsDebugPrintf("ALSA Capture Worker Function : Alsa Overrun \n");
                snd_pcm_prepare(pNvxAudio->capture_handle);
                written = 0;
            }
            else
            {
                NvOsDebugPrintf("ALSA Capture Worker Function : Cannot send data to Audio Driver \n");
                 *pbWouldFail = OMX_TRUE;
                return OMX_ErrorHardware;
            }
        }
        pOMXBuf->nFilledLen += (written * frameSize);
        if(written != totalBuffer) {
            totalBuffer = totalBuffer - written;
            offsetBuffer = written;
        }
        else {
            allDataSent = OMX_TRUE;
        }
    }
    return OMX_ErrorNone;
}


static OMX_ERRORTYPE NvxAlsaCaptureWorkerFunction(OMX_IN NvxComponent *pComponent, OMX_IN OMX_BOOL bAllPortsReady, OMX_OUT OMX_BOOL *pbMoreWork, OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    SNvxAlsaCaptureData   *pNvxAudio = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortOut = &pComponent->pPorts[s_nvx_port_output];
    NvxPort *pPortClock = &pComponent->pPorts[s_nvx_port_clock];
    OMX_TICKS position = 0;
    OMX_BOOL bWouldFail = OMX_TRUE;
    OMX_BOOL bSetPosition = OMX_FALSE;
    OMX_BUFFERHEADERTYPE *pOMXBuf = NULL;
    NVXTRACE((NVXT_WORKER, pComponent->eObjectType, "+NvxAlsaCaptureWorkerFunction\n"));

    pNvxAudio = (SNvxAlsaCaptureData *)pComponent->pComponentData;
    *pbMoreWork = (pPortOut->pCurrentBufferHdr != NULL);
    if (pPortClock && pPortClock->pCurrentBufferHdr)
    {
        NvxPortReleaseBuffer(pPortClock, pPortClock->pCurrentBufferHdr);
    }

    if (!pPortOut->pCurrentBufferHdr)
    {
        return OMX_ErrorNone;
    }

    pOMXBuf = pPortOut->pCurrentBufferHdr;

    {
        OMX_AUDIO_PARAM_PCMMODETYPE oPCMMode;
        OMX_AUDIO_PARAM_PCMMODETYPE *pLocalMode = (OMX_AUDIO_PARAM_PCMMODETYPE *)pPortOut->pPortPrivate;
        OMX_BOOL bQueryPosition = OMX_TRUE;

        if (pPortOut->pCurrentBufferHdr->nFlags & NVX_BUFFERFLAG_CONFIGCHANGED)
        {
            pNvxAudio->bFirstRun = OMX_TRUE;
        }

        if (pNvxAudio->bFirstRun || pLocalMode->nSamplingRate == 0 ||
            pLocalMode->nChannels == 0 || pLocalMode->nBitPerSample == 0)
        {
            if (pPortOut->hTunnelComponent)
            {
                oPCMMode.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
                oPCMMode.nVersion.nVersion = NvxComponentSpecVersion.nVersion;
                oPCMMode.nPortIndex = pPortOut->nTunnelPort;
                eError = OMX_GetParameter(pPortOut->hTunnelComponent,
                                          OMX_IndexParamAudioPcm,
                                          &oPCMMode);
                if (OMX_ErrorNone == eError)
                {
                    NvOsMemcpy(pLocalMode, &oPCMMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
                    pLocalMode->nPortIndex = s_nvx_port_output;
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

        NvxAlsaCaptureToBuffer(pNvxAudio,pOMXBuf,pLocalMode,&bWouldFail);

        if (pNvxAudio->nFrameDuration == 0)
        {
            OMX_U64 y = 0;
            y =
                (MIN_OUTPUT_BUFFERSIZE * 1000000) /
                (pNvxAudio->oNvxPcmParameters.nSamplingRate *
                (pNvxAudio->oNvxPcmParameters.nBitPerSample >> 3) *
                pNvxAudio->oNvxPcmParameters.nChannels);
            pNvxAudio->nFrameDuration = (OMX_U32)(y);
        }
        pPortOut->pCurrentBufferHdr->nTimeStamp = (OMX_TICKS)pNvxAudio->nCurrentTS;
        pNvxAudio->nCurrentTS += (OMX_U64)pNvxAudio->nFrameDuration;

        if (pPortOut->pCurrentBufferHdr->nFlags & OMX_BUFFERFLAG_EOS)
        {
            pNvxAudio->bHaveEndBuffer = OMX_TRUE;
            bQueryPosition = OMX_FALSE;
            NvxSendEvent(pComponent, OMX_EventBufferFlag,
                         pComponent->pPorts[s_nvx_port_output].oPortDef.nPortIndex,
                         OMX_BUFFERFLAG_EOS, 0);
        }

        if(bQueryPosition && pPortOut->pCurrentBufferHdr)
        {
            position = pPortOut->pCurrentBufferHdr->nTimeStamp * 10;
            bSetPosition = OMX_TRUE;
        }

        if (pPortOut->pCurrentBufferHdr->nFilledLen || (pPortOut->pCurrentBufferHdr->nFlags & OMX_BUFFERFLAG_EOS))
        {
            NvxPortDeliverFullBuffer(pPortOut, pPortOut->pCurrentBufferHdr);
        }

        if (!pPortOut->pCurrentBufferHdr)
            NvxPortGetNextHdr(pPortOut);

        *pbMoreWork = (pPortOut->pCurrentBufferHdr && !bWouldFail);


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
         snd_pcm_avail_delay(pNvxAudio->capture_handle,&delayx,&delayy);
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

static OMX_ERRORTYPE NvxAlsaCaptureAcquireResources(OMX_IN NvxComponent *pComponent)
{
    SNvxAlsaCaptureData *pNvxAudio = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAlsaCaptureAcquireResources\n"));

    pNvxAudio = (SNvxAlsaCaptureData *)pComponent->pComponentData;
    NV_ASSERT(pNvxAudio);
    eError = NvxComponentBaseAcquireResources(pComponent);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pComponent->eObjectType, "ERROR:NvxAlsaCaptureAcquireResources:"
           ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }
    eError = NvxAlsaCaptureOpen(pNvxAudio, pComponent);

    return eError;
}

static OMX_ERRORTYPE NvxAlsaCaptureReleaseResources(OMX_IN NvxComponent *pComponent)
{
    SNvxAlsaCaptureData *pNvxAudio = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAlsaCaptureReleaseResources\n"));
    pNvxAudio = (SNvxAlsaCaptureData *)pComponent->pComponentData;
    NV_ASSERT(pNvxAudio);

    eError = NvxAlsaCaptureClose(pNvxAudio);
    NvxCheckError(eError, NvxComponentBaseReleaseResources(pComponent));
    return eError;
}

static OMX_ERRORTYPE NvxAlsaCaptureFlush(OMX_IN NvxComponent *pComponent, OMX_U32 nPort)
{
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE AvSetState(SNvxAlsaCaptureData *pData,
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

static OMX_ERRORTYPE NvxAlsaCapturePreChangeState(NvxComponent *pNvComp,
                                                    OMX_STATETYPE oNewState)
{
    SNvxAlsaCaptureData *pNvxAudio = 0;
    OMX_ERRORTYPE err;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioRendererPreChangeState\n"));
    pNvxAudio = (SNvxAlsaCaptureData *)pNvComp->pComponentData;
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

static OMX_ERRORTYPE NvxAlsaCaptureOpenDevice(SNvxAlsaCaptureData * pNvxAudio)
{

      int deviceId = 0;
       OMX_STRING pCardName = "default";

    pNvxAudio->capture_handle = NULL;
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
        if(snd_pcm_open(&pNvxAudio->capture_handle, \
        pCardName, SND_PCM_STREAM_CAPTURE,0)<0)
        {
                NvOsDebugPrintf("%s: open alsa audio device failed\n",__FUNCTION__);
                return OMX_ErrorHardware;
        }
    }
    else
    {
        printf("Using Native Audio Renderer \n");
        pNvxAudio->capture_handle = pNvxAudio->pNativeRender;
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxAlsaCaptureOpen(SNvxAlsaCaptureData * pNvxAudio, NvxComponent *pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortClock = &(pComponent->pPorts[s_nvx_port_clock]);
    NVXTRACE(( NVXT_CALLGRAPH, NVXT_AUDIO_RENDERER, "+NvxAlsaCaptureOpen\n"));

    if((eError=NvxAlsaCaptureOpenDevice(pNvxAudio)) != OMX_ErrorNone)
    {
        return eError;
    }

    if(snd_pcm_hw_params_malloc(&pNvxAudio->hw_params)<0)
    {
        NvOsDebugPrintf("%s: Failed allocating hw params\n",__FUNCTION__);
        return OMX_ErrorHardware;
    }

    if(snd_pcm_hw_params_any(pNvxAudio->capture_handle,pNvxAudio->hw_params) < 0)
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

static OMX_ERRORTYPE NvxAlsaCaptureClose(SNvxAlsaCaptureData *pNvxAudio)
{

    NVXTRACE((NVXT_CALLGRAPH, NVXT_AUDIO_RENDERER, "+NvxAlsaCaptureClose\n"));

    if (!pNvxAudio->bInitialized)
        return OMX_ErrorNone;

    if(pNvxAudio->hw_params)
    {
        snd_pcm_hw_params_free(pNvxAudio->hw_params);
    }

    if (pNvxAudio->pNativeRender)
    {
        pNvxAudio->capture_handle = NULL;
    }
    else
    {
        if(pNvxAudio->capture_handle)
        {
            snd_pcm_close(pNvxAudio->capture_handle);
        }
    }

    pNvxAudio->bInitialized = OMX_FALSE;
    pNvxAudio->bConfigured = OMX_FALSE;

    return OMX_ErrorNone;
}


OMX_ERRORTYPE NvxAudioCapturerInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    NvxComponent *pThis = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_AUDIO_PARAM_PCMMODETYPE  *pNvxPcmParameters;
    SNvxAlsaCaptureData *data = NULL;
    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxAlsaCaptureInit\n"));

    eError = NvxComponentCreate(hComponent, s_nvx_num_ports, &pThis);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType,"ERROR:NvxAlsaCaptureInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pThis->eObjectType = NVXT_AUDIO_CAPTURE;
    data = NvOsAlloc(sizeof(SNvxAlsaCaptureData));
    if (!data)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType,"ERROR:NvxAlsaCaptureInit:"
            ":SNvxAudioData = %d:[%s(%d)]\n",data,__FILE__, __LINE__));
        return OMX_ErrorInsufficientResources;
    }
    NvOsMemset(data, 0, sizeof(SNvxAlsaCaptureData));
    pThis->pComponentData = data;
    pThis->pComponentName = "OMX.Nvidia.audio.capturer";
    pThis->nComponentRoles = 1;
    pThis->sComponentRoles[0] = "audio_capturer.pcm";

    pThis->GetParameter       = NvxAlsaCaptureGetParameter;
    pThis->SetParameter       = NvxAlsaCaptureSetParameter;
    pThis->GetConfig          = NvxAlsaCaptureGetConfig;
    pThis->SetConfig          = NvxAlsaCaptureSetConfig;
    pThis->WorkerFunction     = NvxAlsaCaptureWorkerFunction;
    pThis->AcquireResources   = NvxAlsaCaptureAcquireResources;
    pThis->ReleaseResources   = NvxAlsaCaptureReleaseResources;
    pThis->PreChangeState     = NvxAlsaCapturePreChangeState;
    pThis->DeInit             = NvxAlsaCaptureDeInit;
    pThis->Flush              = NvxAlsaCaptureFlush;

    pThis->pPorts[s_nvx_port_output].oPortDef.nPortIndex = s_nvx_port_output;

    NvxPortInitAudio(&pThis->pPorts[s_nvx_port_output], OMX_DirOutput, MAX_OUTPUT_BUFFERS, MIN_OUTPUT_BUFFERSIZE, OMX_AUDIO_CodingPCM);

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

    NvxPortInitOther(&pThis->pPorts[s_nvx_port_clock], OMX_DirInput, 4, sizeof(OMX_TIME_MEDIATIMETYPE), OMX_OTHER_FormatTime);
    pThis->pPorts[s_nvx_port_clock].eNvidiaTunnelTransaction = ENVX_TRANSACTION_CLOCK;

    return eError;
}
