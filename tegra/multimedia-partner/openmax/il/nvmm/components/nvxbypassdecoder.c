/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/** nvxbypassdecoder.c */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

#include <OMX_Core.h>
#include "nvassert.h"
#include "common/NvxTrace.h"
#include "components/common/NvxComponent.h"
#include "components/common/NvxPort.h"
#include "nvmm/components/common/NvxCompReg.h"
#include "nvmm/components/common/NvxCompMisc.h"
#include "nvfxmath.h"
#include "NvxTrace.h"
#include "string.h"
#include "inttypes.h"

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */
#define MAX_INPUT_BUFFERS   5
#define MAX_OUTPUT_BUFFERS  5

// For AC3 max frame size is 2^11 words = (2048*2)
// For DTS max frame size is 2^14. But sometimes frames come with extra padding
#define MIN_INPUT_BUFFERSIZE (16384*2)


// For AC3 2-Ch stream (1536*2*2)
// For DTS 2-Ch stream (128*32*2*2)

#define MIN_OUTPUT_BUFFERSIZE (128*32*2*2)

/* Number of internal frame buffers (AUDIO memory) */
static const int s_nvx_num_ports            = 2;
static const int s_nvx_port_input           = 0;
static const int s_nvx_port_output          = 1;

static OMX_ERRORTYPE NvxBypassDecoderAcquireResources(OMX_IN NvxComponent *pComponent);
static OMX_ERRORTYPE NvxBypassDecoderReleaseResources(OMX_IN NvxComponent *pComponent);

/* Bypass Decoder State information */
typedef struct SNvxBypassDecoderData
{
    OMX_BOOL bInitialized;
    OMX_BOOL bOverrideTimestamp;

    OMX_BOOL bFirstBuffer;
    OMX_BOOL bSilenceOutput;

    OMX_AUDIO_CODINGTYPE inputCodingType;
    OMX_AUDIO_CODINGTYPE outputCodingType;
    OMX_AUDIO_CODINGTYPE newOutputCodingType;

    NVX_AUDIO_DECODE_DATA parseData;

    NvU64   timeStamp;

} SNvxBypassDecoderData;

/* =========================================================================
 *                     P R O T O T Y P E S
 * ========================================================================= */

static OMX_ERRORTYPE NvxBypassDecoderOpen( SNvxBypassDecoderData * pNvxBypassDecoder, NvxComponent *pComponent );
static OMX_ERRORTYPE NvxBypassDecoderClose( SNvxBypassDecoderData * pNvxBypassDecoder );

/* =========================================================================
 *                     F U N C T I O N S
 * ========================================================================= */

/* =========================================================================
 * Bypass Decoder functions
 * ========================================================================= */

static OMX_ERRORTYPE NvxBypassDecoderDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxBypassDecoderDeInit\n"));

    NvOsFree(pNvComp->pComponentData);
    pNvComp->pComponentData = 0;
    return eError;
}

static OMX_ERRORTYPE NvxBypassDecoderGetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    SNvxBypassDecoderData *pNvxBypassDecoder;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxBypassDecoderGetParameter 0x%x \n", nParamIndex));

    pNvxBypassDecoder = (SNvxBypassDecoderData *)pComponent->pComponentData;

    switch (nParamIndex)
    {
    case OMX_IndexParamPortDefinition:
    {
        OMX_PARAM_PORTDEFINITIONTYPE *pPortDef =
            (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;

        NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
        if (s_nvx_port_output == (NvS32)pPortDef->nPortIndex)
        {
            NvOsMemcpy(&pPortDef->format.audio,
                        &pComponent->pPorts[s_nvx_port_output].oPortDef.format.audio,
                        sizeof(OMX_AUDIO_PORTDEFINITIONTYPE));

            pPortDef->format.audio.eEncoding = pNvxBypassDecoder->outputCodingType;
        }
        else
        {
            eError = OMX_ErrorNone;
        }
        break;
    }
    case OMX_IndexParamAudioPcm:
    {
        OMX_AUDIO_PARAM_PCMMODETYPE *pcmParams =
            (OMX_AUDIO_PARAM_PCMMODETYPE *)pComponentParameterStructure;

        if ((NvU32)s_nvx_port_output == pcmParams->nPortIndex)
        {
            pcmParams->eNumData = OMX_NumericalDataSigned;
            pcmParams->eEndian = OMX_EndianLittle;
            pcmParams->bInterleaved = OMX_TRUE;
            pcmParams->nBitPerSample = 16;
            pcmParams->ePCMMode = OMX_AUDIO_PCMModeLinear;
            pcmParams->eChannelMapping[0] = OMX_AUDIO_ChannelLF;
            pcmParams->eChannelMapping[1] = OMX_AUDIO_ChannelRF;
            pcmParams->nChannels = 2;
            pcmParams->nSamplingRate = pNvxBypassDecoder->parseData.nSampleRate;
        }
        else
        {
            eError = OMX_ErrorBadParameter;
        }
        break;
    }
    case NVX_IndexParamAudioAc3:
    {
        NVX_AUDIO_PARAM_AC3TYPE *pAc3Param =
            (NVX_AUDIO_PARAM_AC3TYPE *)pComponentParameterStructure;

        if ((NvU32)s_nvx_port_output == pAc3Param->nPortIndex)
        {
            // as per HDMI spec 1.3a, section 7.6, IEC61937 stream should use
            // 2 channel format
            pAc3Param->nChannels = 2;
            pAc3Param->nSampleRate = pNvxBypassDecoder->parseData.nSampleRate;
        }
        else
        {
            eError = OMX_ErrorBadParameter;
        }
        break;
    }
    case NVX_IndexParamAudioDts:
    {
        NVX_AUDIO_PARAM_DTSTYPE *pDtsaram =
            (NVX_AUDIO_PARAM_DTSTYPE *)pComponentParameterStructure;

        if ((NvU32)s_nvx_port_output == pDtsaram->nPortIndex)
        {
            // as per HDMI spec 1.3a, section 7.6, IEC61937 stream should use
            // 2 channel format
            pDtsaram->nChannels = 2;
            pDtsaram->nSampleRate = pNvxBypassDecoder->parseData.nSampleRate;
        }
        else
        {
            eError = OMX_ErrorBadParameter;
        }
        break;
    }
    default:
        eError = NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
    };

    return eError;
}

static OMX_ERRORTYPE NvxBypassDecoderSetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentParameterStructure)
{
    SNvxBypassDecoderData *pNvxBypassDecoder;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent && pComponentParameterStructure);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxBypassDecoderSetParameter 0x%x \n", nIndex));

    pNvxBypassDecoder = (SNvxBypassDecoderData *)pComponent->pComponentData;
    switch(nIndex)
    {
    case OMX_IndexParamStandardComponentRole:
    {
        const OMX_PARAM_COMPONENTROLETYPE *roleParams =
            (const OMX_PARAM_COMPONENTROLETYPE *)pComponentParameterStructure;

        NvOsDebugPrintf("BypassDecoder: setting component role: %s", roleParams->cRole);

        if (!strcmp((const char*)roleParams->cRole, "audio_decoder.ac3"))
            pNvxBypassDecoder->inputCodingType = (OMX_AUDIO_CODINGTYPE)NVX_AUDIO_CodingAC3;
        else if (!strcmp((const char*)roleParams->cRole, "audio_decoder.dts"))
            pNvxBypassDecoder->inputCodingType = (OMX_AUDIO_CODINGTYPE)NVX_AUDIO_CodingDTS;
        else
            NvOsDebugPrintf("BypassDecoder: Unsupported component role: %s", roleParams->cRole);
        break;
    }
    default:
        eError = NvxComponentBaseSetParameter(pComponent, nIndex, pComponentParameterStructure);
    }
    return eError;
}

static OMX_ERRORTYPE NvxBypassDecoderGetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    SNvxBypassDecoderData *pNvxBypassDecoder;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxBypassDecoderGetConfig\n"));

    pNvxBypassDecoder = (SNvxBypassDecoderData *)pNvComp->pComponentData;
    switch (nIndex)
    {
    case NVX_IndexConfigAudioSilenceOutput:
    {
        OMX_CONFIG_BOOLEANTYPE *pSilenceOutput;
        pSilenceOutput = (OMX_CONFIG_BOOLEANTYPE*)pComponentConfigStructure;
        if (pSilenceOutput->nSize == sizeof(OMX_CONFIG_BOOLEANTYPE))
        {
            pSilenceOutput->bEnabled = pNvxBypassDecoder->bSilenceOutput;
        }
        else
        {
            eError = OMX_ErrorBadParameter;
        }
        break;
    }
    default:
        eError = NvxComponentBaseGetConfig( pNvComp, nIndex, pComponentConfigStructure);
        break;
    }
    return eError;
}

static OMX_ERRORTYPE NvxBypassDecoderSetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentConfigStructure)
{
    SNvxBypassDecoderData *pNvxBypassDecoder;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxBypassDecoderSetConfig 0x%x \n", nIndex));

    pNvxBypassDecoder = (SNvxBypassDecoderData *)pNvComp->pComponentData;
    switch (nIndex)
    {
    case NVX_IndexConfigAudioSilenceOutput:
    {
        OMX_CONFIG_BOOLEANTYPE *pSilenceOutput =
            (OMX_CONFIG_BOOLEANTYPE *)pComponentConfigStructure;
        if (pSilenceOutput->nSize == sizeof(OMX_CONFIG_BOOLEANTYPE))
        {
            pNvxBypassDecoder->bSilenceOutput = pSilenceOutput->bEnabled;
            if (pSilenceOutput->bEnabled)
            {
                pNvxBypassDecoder->newOutputCodingType = OMX_AUDIO_CodingPCM;
            }
            else
            {
                pNvxBypassDecoder->newOutputCodingType = pNvxBypassDecoder->inputCodingType;
            }
            NvOsDebugPrintf("BypassDecoder: Setting output coding type to 0x%x", pNvxBypassDecoder->newOutputCodingType);
        }
        else
        {
            eError = OMX_ErrorBadParameter;
        }
        break;
    }
    default:
            eError = NvxComponentBaseSetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return eError;
}

static OMX_ERRORTYPE NvxBypassDecoderFlush(OMX_IN NvxComponent *pComponent, OMX_U32 nPort)
{
    SNvxBypassDecoderData *pNvxBypassDecoder;

    NV_ASSERT(pComponent && pComponent->pComponentData);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAudioDecoderFlush\n"));

    pNvxBypassDecoder = (SNvxBypassDecoderData *)pComponent->pComponentData;
    pNvxBypassDecoder->bOverrideTimestamp = OMX_TRUE;
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxBypassDecoderWorkerFunction(OMX_IN NvxComponent *pComponent, OMX_IN OMX_BOOL bAllPortsReady, OMX_OUT OMX_BOOL *pbMoreWork, OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    SNvxBypassDecoderData *pNvxBypassDecoder;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];
    NvxPort *pPortOut = &pComponent->pPorts[s_nvx_port_output];
    OMX_BUFFERHEADERTYPE *pInBuffer, *pOutBuffer;

    NVXTRACE((NVXT_WORKER, pComponent->eObjectType, "+NvxBypassDecoderWorkerFunction\n"));

    *pbMoreWork = bAllPortsReady;

    pNvxBypassDecoder = (SNvxBypassDecoderData *)pComponent->pComponentData;

    if (!bAllPortsReady)
    {
        //NvOsDebugPrintf("BypassDecoder: all ports not ready\n");
        return OMX_ErrorNone;
    }

    pInBuffer = pPortIn->pCurrentBufferHdr;
    pOutBuffer = pPortOut->pCurrentBufferHdr;

    if (NVX_AUDIO_CodingAC3 == pNvxBypassDecoder->inputCodingType)
    {
        NvxBypassProcessAC3(&pNvxBypassDecoder->parseData,
                            pNvxBypassDecoder->outputCodingType,
                            pInBuffer->pBuffer + pInBuffer->nOffset,
                            pInBuffer->nFilledLen,
                            pOutBuffer->pBuffer + pOutBuffer->nOffset,
                            pOutBuffer->nAllocLen);
    }
    else if (NVX_AUDIO_CodingDTS == pNvxBypassDecoder->inputCodingType)
    {
        NvxBypassProcessDTS(&pNvxBypassDecoder->parseData,
                            pNvxBypassDecoder->outputCodingType,
                            pInBuffer->pBuffer + pInBuffer->nOffset,
                            pInBuffer->nFilledLen,
                            pOutBuffer->pBuffer + pOutBuffer->nOffset,
                            pOutBuffer->nAllocLen);
    }
    else
        NvOsDebugPrintf("Invalid coding type %x", pNvxBypassDecoder->inputCodingType);

    if ((pNvxBypassDecoder->parseData.bFrameOK && pNvxBypassDecoder->parseData.outputBytes > 0) ||
        (pInBuffer->nFlags & OMX_BUFFERFLAG_EOS))
    {
        pOutBuffer->nFilledLen = pNvxBypassDecoder->parseData.outputBytes;
        if (NVX_AUDIO_CodingAC3 == pNvxBypassDecoder->inputCodingType) {
            if (pNvxBypassDecoder->bOverrideTimestamp == OMX_TRUE) {
                pNvxBypassDecoder->timeStamp = pInBuffer->nTimeStamp;
                pOutBuffer->nTimeStamp = pInBuffer->nTimeStamp;
                pNvxBypassDecoder->bOverrideTimestamp = OMX_FALSE;
            } else {
                NvU64 timeStamp = ((NvU64)pNvxBypassDecoder->parseData.outputBytes * 1000000)
                    / (2 * pNvxBypassDecoder->parseData.nChannels * pNvxBypassDecoder->parseData.nSampleRate);
                pNvxBypassDecoder->timeStamp += (timeStamp);
                pOutBuffer->nTimeStamp = pNvxBypassDecoder->timeStamp;
            }
        } else {
            pOutBuffer->nTimeStamp = pInBuffer->nTimeStamp;
        }

        pOutBuffer->nFlags = pInBuffer->nFlags;
        if (OMX_AUDIO_CodingPCM != pNvxBypassDecoder->outputCodingType)
            pOutBuffer->nFlags |= OMX_BUFFERFLAG_COMPRESSED;

        //NvOsDebugPrintf("BypassDecoder: release output buffer size: %d\n", pOutBuffer->nFilledLen);

        /* release output buffer */
        NvxPortDeliverFullBuffer(pPortOut, pOutBuffer);
    }
    else
    {
        NvOsDebugPrintf("BypassDecoder: Error in frame, skipping\n");
    }

    if (pInBuffer->nFlags & OMX_BUFFERFLAG_EOS)
    {
        pInBuffer->nFilledLen = 0;
        pInBuffer->nOffset = 0;
    }
    else
    {
        pInBuffer->nFilledLen -= pNvxBypassDecoder->parseData.bytesConsumed;
        pInBuffer->nOffset += pNvxBypassDecoder->parseData.bytesConsumed;
    }

    if (pInBuffer->nFilledLen == 0)
    {
        //NvOsDebugPrintf("BypassDecoder: release input buffer\n");
        /* Done with the input buffer, so now release it */
        NvxPortReleaseBuffer(pPortIn, pInBuffer);
    }

    if (pNvxBypassDecoder->newOutputCodingType != pNvxBypassDecoder->outputCodingType)
    {
        pNvxBypassDecoder->outputCodingType = pNvxBypassDecoder->newOutputCodingType;
        NvxSendEvent(pComponent,
                    OMX_EventPortSettingsChanged,
                    s_nvx_port_output,
                    OMX_IndexParamAudioPcm,
                    NULL);
        NvOsDebugPrintf("BypassDecoder: Format changed to 0x%x\n", pNvxBypassDecoder->outputCodingType);
    }

    return eError;
}

static OMX_ERRORTYPE NvxBypassDecoderAcquireResources(OMX_IN NvxComponent *pComponent)
{
    SNvxBypassDecoderData *pNvxBypassDecoder;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxBypassDecoderAcquireResources\n"));

    /* Get Aac Decoder instance */
    pNvxBypassDecoder = (SNvxBypassDecoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxBypassDecoder);

    eError = NvxComponentBaseAcquireResources(pComponent);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pComponent->eObjectType, "ERROR:NvxBypassDecoderAcquireResources:"
           ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    /* Open handles and setup frames */
    eError = NvxBypassDecoderOpen( pNvxBypassDecoder, pComponent );

    return eError;
}

static OMX_ERRORTYPE NvxBypassDecoderReleaseResources(OMX_IN NvxComponent *pComponent)
{
    SNvxBypassDecoderData *pNvxBypassDecoder;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxBypassDecoderReleaseResources\n"));

    /* Get Aac Decoder instance */
    pNvxBypassDecoder = (SNvxBypassDecoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxBypassDecoder);

    eError = NvxBypassDecoderClose(pNvxBypassDecoder);
    NvxCheckError(eError, NvxComponentBaseReleaseResources(pComponent));
    return eError;
}


/* =========================================================================
 *                     I N T E R N A L
 * ========================================================================= */

OMX_ERRORTYPE NvxBypassDecoderOpen( SNvxBypassDecoderData * pNvxBypassDecoder, NvxComponent *pComponent )
{
    NVXTRACE(( NVXT_CALLGRAPH, NVXT_AUDIO_DECODER, "+NvxBypassDecoderOpen\n"));

    pNvxBypassDecoder->bFirstBuffer = OMX_TRUE;
    pNvxBypassDecoder->bInitialized = OMX_TRUE;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxBypassDecoderClose(SNvxBypassDecoderData * pNvxBypassDecoder)
{
    NVXTRACE((NVXT_CALLGRAPH, NVXT_AUDIO_DECODER, "+NvxBypassDecoderClose\n"));

    if (!pNvxBypassDecoder->bInitialized)
        return OMX_ErrorNone;

    pNvxBypassDecoder->bInitialized = OMX_FALSE;
    return OMX_ErrorNone;
}

/******************
 * Init functions *
 ******************/
OMX_ERRORTYPE NvxBypassDecoderInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pThis;
    SNvxBypassDecoderData *pNvxBypassDecoder = NULL;

    OMX_AUDIO_PARAM_PCMMODETYPE  *pNvxPcmParameters;

    eError = NvxComponentCreate(hComponent, s_nvx_num_ports, &pThis);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType,"ERROR:NvxCommonBypassDecoderInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    NvxAddResource(pThis, NVX_AUDIO_MEDIAPROCESSOR_RES);

    pThis->eObjectType = NVXT_AUDIO_DECODER;

    pNvxBypassDecoder = NvOsAlloc(sizeof(SNvxBypassDecoderData));
    if (!pNvxBypassDecoder)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType,"ERROR:NvxCommonBypassDecoderInit:"
            ":SNvxAudioData = %d:[%s(%d)]\n",pNvxBypassDecoder,__FILE__, __LINE__));
        return OMX_ErrorInsufficientResources;
    }

    NvOsMemset(pNvxBypassDecoder, 0, sizeof(SNvxBypassDecoderData));

    pNvxBypassDecoder->bFirstBuffer = OMX_TRUE;
    pNvxBypassDecoder->inputCodingType = NVX_AUDIO_CodingAC3;
    pNvxBypassDecoder->outputCodingType = NVX_AUDIO_CodingAC3;
    pNvxBypassDecoder->newOutputCodingType = NVX_AUDIO_CodingAC3;
    pNvxBypassDecoder->parseData.nChannels = 2;
    pNvxBypassDecoder->parseData.nSampleRate = 48000;
    pNvxBypassDecoder->bOverrideTimestamp = OMX_TRUE;

    pThis->pComponentData = pNvxBypassDecoder;

    pThis->pComponentName = "OMX.Nvidia.bypass.decoder";
    pThis->nComponentRoles = 1;
    pThis->sComponentRoles[0] = "audio_decoder.ac3";

    pThis->DeInit             = NvxBypassDecoderDeInit;
    pThis->GetParameter       = NvxBypassDecoderGetParameter;
    pThis->SetParameter       = NvxBypassDecoderSetParameter;
    pThis->GetConfig          = NvxBypassDecoderGetConfig;
    pThis->SetConfig          = NvxBypassDecoderSetConfig;
    pThis->WorkerFunction     = NvxBypassDecoderWorkerFunction;
    pThis->AcquireResources   = NvxBypassDecoderAcquireResources;
    pThis->ReleaseResources   = NvxBypassDecoderReleaseResources;
    pThis->Flush              = NvxBypassDecoderFlush;
    pThis->pPorts[s_nvx_port_output].oPortDef.nPortIndex = s_nvx_port_output;

    NvxPortInitAudio(&pThis->pPorts[s_nvx_port_input], OMX_DirInput, MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE, pNvxBypassDecoder->inputCodingType);
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

    return eError;
}

