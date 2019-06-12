/*
* Copyright (c) 2006 NVIDIA Corporation.  All Rights Reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property and
* proprietary rights in and to this software and related documentation.  Any
* use, reproduction, disclosure or distribution of this software and related
* documentation without an express license agreement from NVIDIA Corporation
* is strictly prohibited.
*/

/** NvxAudioEncoder.c */
/* ====================================================================*/
/* Location        \sw\embedded\drivers\OpenMax\il\nvmm\components  */
/* =========================================================================
*                     I N C L U D E S
* ========================================================================= */

#include <OMX_Core.h>
#include "common/NvxTrace.h"
#include "components/common/NvxComponent.h"
#include "components/common/NvxPort.h"
#include "nvmm/components/common/nvmmtransformbase.h"
#include "nvmm/components/common/NvxCompReg.h"
#include "nvmm_amr.h"
#include "nvmm_amrwb.h"
#include "nvmm_ilbc.h"
#include "nvmm_wave.h"


#include "nvmm_aac.h"
#include "nvmm_mixer.h"

#include "nvassert.h"

/* =========================================================================
*                     D E F I N E S
* ========================================================================= */

// DEBUG defines. TO DO: fix this
#define MAX_INPUT_BUFFERS    5
#define MAX_OUTPUT_BUFFERS   20
#define MIN_INPUT_BUFFERSIZE 8192 // 4096
#define MIN_INPUT_BUFFERSIZE_AACPLUS 8192
#define MIN_INPUT_BUFFERSIZE_AMRNB 3200
#define MIN_INPUT_BUFFERSIZE_AMRWB 6400
#define MIN_INPUT_BUFFERSIZE_ILBC 2880
#define MIN_OUTPUT_BUFFERSIZE 5000 //1536
#define MIN_OUTPUT_BUFFERSIZE_ILBC 480
#define MAX_AAC_CHANNELS      2

/* Number of internal frame buffers (AUDIO memory) */
static const int s_nvx_num_ports            = 2;
static const int s_nvx_port_input           = 0;
static const int s_nvx_port_output          = 1;

/* Aac Encoder State information */
typedef struct SNvxAudioEncoderData
{
    OMX_BOOL bInitialized;
    OMX_BOOL bOverrideTimestamp;
    
    NvMMBlockType oBlockType;
    const char *sBlockName;
    
    SNvxNvMMTransformData oBase;
    
} SNvxAudioEncoderData;

/* =========================================================================
*                     P R O T O T Y P E S
* ========================================================================= */

static OMX_ERRORTYPE NvxAudioEncoderOpen( SNvxAudioEncoderData * pNvxAudioEncoder, NvxComponent *pComponent );
static OMX_ERRORTYPE NvxAudioEncoderClose( SNvxAudioEncoderData * pNvxAudioEncoder );

/* =========================================================================
*                     F U N C T I O N S
* ========================================================================= */

static OMX_ERRORTYPE NvxAudioEncoderDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioEncoderInit\n"));

    NvOsFree(pNvComp->pComponentData);
    pNvComp->pComponentData = 0;
    return eError;
}

static OMX_ERRORTYPE NvxAudioEncoderGetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    SNvxAudioEncoderData *pNvxAudioEncoder;

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAudioEncoderGetParameter\n"));
    NV_ASSERT(pComponent && pComponentParameterStructure);

    pNvxAudioEncoder = (SNvxAudioEncoderData *)pComponent->pComponentData;
    switch (nParamIndex)
    {
    case OMX_IndexParamPortDefinition:
        pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
        eError = NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
        break;

    case OMX_IndexParamAudioAac:
        if (NvMMBlockType_EncAAC != pNvxAudioEncoder->oBlockType)
        {
            return OMX_ErrorBadParameter;
        }
        NV_ASSERT(pComponent->pPorts[s_nvx_port_output].pPortPrivate);
        NvOsMemcpy(pComponentParameterStructure, pComponent->pPorts[s_nvx_port_output].pPortPrivate, sizeof(OMX_AUDIO_PARAM_AACPROFILETYPE));
        break;

    case OMX_IndexParamAudioAmr:
        if (NvMMBlockType_EncAMRNB != pNvxAudioEncoder->oBlockType && NvMMBlockType_EncAMRWB != pNvxAudioEncoder->oBlockType )
        {
            return OMX_ErrorBadParameter;
        }
        NV_ASSERT(pComponent->pPorts[s_nvx_port_output].pPortPrivate);
        NvOsMemcpy(pComponentParameterStructure, pComponent->pPorts[s_nvx_port_output].pPortPrivate, sizeof(OMX_AUDIO_PARAM_AMRTYPE));
        break;
    case OMX_IndexParamAudioPcm:
        if(((OMX_AUDIO_PARAM_PCMMODETYPE *)pComponentParameterStructure)->nPortIndex ==
                                    (NvU32)s_nvx_port_input)
        {
            NV_ASSERT(pComponent->pPorts[s_nvx_port_input].pPortPrivate);
            NvOsMemcpy(pComponentParameterStructure,
            pComponent->pPorts[s_nvx_port_input].pPortPrivate,
            sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
        }
        else
        {
            if (NvMMBlockType_EncWAV != pNvxAudioEncoder->oBlockType )
                return OMX_ErrorBadParameter;
            else
            {
                NV_ASSERT(pComponent->pPorts[s_nvx_port_output].pPortPrivate);
                NvOsMemcpy(pComponentParameterStructure, 
                pComponent->pPorts[s_nvx_port_output].pPortPrivate,
                sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
            }
        }
        break;

    //case OMX_IndexParamAudioilbc:
    //    if (NvMMBlockType_EnciLBC != pNvxAudioEncoder->oBlockType)
    //    {
    //        return OMX_ErrorBadParameter;
    //    }
    //    NV_ASSERT(pComponent->pPorts[s_nvx_port_output].pPortPrivate);
    //    NvOsMemcpy(pComponentParameterStructure, pComponent->pPorts[s_nvx_port_output].pPortPrivate, sizeof(OMX_AUDIO_PARAM_ILBCTYPE));
    //    break;

    default:
        eError = NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
    };
    
    return eError;
}

static OMX_ERRORTYPE NvxAudioEncoderSetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    SNvxAudioEncoderData *pNvxAudioEncoder;
    
    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAudioEncoderSetParameter\n"));
    NV_ASSERT(pComponent && pComponentParameterStructure);

    pNvxAudioEncoder = (SNvxAudioEncoderData *)pComponent->pComponentData;
    switch(nIndex)
    {
    case OMX_IndexParamAudioAmr:
        if (NvMMBlockType_EncAMRNB != pNvxAudioEncoder->oBlockType && NvMMBlockType_EncAMRWB != pNvxAudioEncoder->oBlockType)
        {
            return OMX_ErrorBadParameter;
        }
        NV_ASSERT(pComponent->pPorts[s_nvx_port_output].pPortPrivate);
        NvOsMemcpy(pComponent->pPorts[s_nvx_port_output].pPortPrivate, pComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_AMRTYPE));
        break;
        
    case OMX_IndexParamAudioAac:
        {
            OMX_AUDIO_PARAM_AACPROFILETYPE *pNvxAacEncoderParameters = 
                (OMX_AUDIO_PARAM_AACPROFILETYPE *)pComponent->pPorts[s_nvx_port_output].pPortPrivate;
            NvU32 bufferSize = MIN_INPUT_BUFFERSIZE;
            if (NvMMBlockType_EncAAC != pNvxAudioEncoder->oBlockType)
            {
                return OMX_ErrorBadParameter;
            }
            NV_ASSERT(pComponent->pPorts[s_nvx_port_output].pPortPrivate);
            NvOsMemcpy(pComponent->pPorts[s_nvx_port_output].pPortPrivate, pComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_AACPROFILETYPE));

            if((pNvxAacEncoderParameters->eAACProfile == OMX_AUDIO_AACObjectHE) || (pNvxAacEncoderParameters->eAACProfile ==    OMX_AUDIO_AACObjectHE_PS))
            {
                bufferSize = ((MIN_INPUT_BUFFERSIZE_AACPLUS / MAX_AAC_CHANNELS) * pNvxAacEncoderParameters->nChannels);
            }
            else
            {
                bufferSize = ((MIN_INPUT_BUFFERSIZE / MAX_AAC_CHANNELS) * pNvxAacEncoderParameters->nChannels);
            } 
            NvxPortInitAudio(&pComponent->pPorts[s_nvx_port_input], OMX_DirInput, MAX_INPUT_BUFFERS, bufferSize, OMX_AUDIO_CodingPCM);
        }
        break;
    case OMX_IndexParamAudioPcm:
        {
            if(((OMX_AUDIO_PARAM_PCMMODETYPE *)pComponentParameterStructure)->nPortIndex ==
                                        (NvU32)s_nvx_port_input)
            {
                NV_ASSERT(pComponent->pPorts[s_nvx_port_input].pPortPrivate);
                NvOsMemcpy(pComponent->pPorts[s_nvx_port_input].pPortPrivate,
                pComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
            }
            else
            {
                if (NvMMBlockType_EncWAV != pNvxAudioEncoder->oBlockType)
                    return OMX_ErrorBadParameter;
                else
                {
                    NV_ASSERT(pComponent->pPorts[s_nvx_port_output].pPortPrivate);
                    NvOsMemcpy(pComponent->pPorts[s_nvx_port_output].pPortPrivate,
                    pComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
                }
            }
        }
        break;

    //case OMX_IndexParamAudioilbc:
    //    if (NvMMBlockType_EnciLBC != pNvxAudioEncoder->oBlockType)
    //    {
    //        return OMX_ErrorBadParameter;
    //    }
    //    NV_ASSERT(pComponent->pPorts[s_nvx_port_output].pPortPrivate);
    //    NvOsMemcpy(pComponent->pPorts[s_nvx_port_output].pPortPrivate, pComponentParameterStructure, sizeof(OMX_AUDIO_PARAM_ILBCTYPE));
    //    break;


    default:
        eError = NvxComponentBaseSetParameter(pComponent, nIndex, pComponentParameterStructure);
    }
    return eError;
}

static OMX_ERRORTYPE NvxAudioEncoderGetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    SNvxAudioEncoderData *pNvxAudioEncoder;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    
    NV_ASSERT(pNvComp && pComponentConfigStructure);
    
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioEncoderGetConfig\n"));
    
    pNvxAudioEncoder = (SNvxAudioEncoderData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
    case NVX_IndexConfigGetNVMMBlock:
        {
            NVX_CONFIG_GETNVMMBLOCK *pGetBlock = (NVX_CONFIG_GETNVMMBLOCK *)pComponentConfigStructure;
            
            if (!pNvxAudioEncoder || !pNvxAudioEncoder->bInitialized)
                return OMX_ErrorNotReady;
            
            if ((NvS32)pGetBlock->nPortIndex != s_nvx_port_output)
                return OMX_ErrorBadParameter;
            
            pGetBlock->pNvMMTransform = &pNvxAudioEncoder->oBase;
            pGetBlock->nNvMMPort = s_nvx_port_output;
            eError = OMX_ErrorNone;
            break;
        }
    default:
        eError = NvxComponentBaseGetConfig( pNvComp, nIndex, pComponentConfigStructure);
        break;
    }
    return eError;
}

static OMX_ERRORTYPE NvxAudioEncoderSetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentConfigStructure)
{
    SNvxAudioEncoderData *pNvxAudioEncoder;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    
    NV_ASSERT(pNvComp && pComponentConfigStructure);
    
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioEncoderSetConfig\n"));
    
    pNvxAudioEncoder = (SNvxAudioEncoderData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
    case NVX_IndexConfigProfile:
        {
            NVX_CONFIG_PROFILE *pProf = (NVX_CONFIG_PROFILE *)pComponentConfigStructure;
            NvxNvMMTransformSetProfile(&pNvxAudioEncoder->oBase, pProf);
            break;
        }
    default:
        eError = NvxComponentBaseSetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return eError;
}

static OMX_ERRORTYPE NvxAudioEncoderWorkerFunction(OMX_IN NvxComponent *pComponent, OMX_IN OMX_BOOL bAllPortsReady, OMX_OUT OMX_BOOL *pbMoreWork, OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    SNvxAudioEncoderData   *pNvxAudioEncoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];
    NvxPort *pPortOut = &pComponent->pPorts[s_nvx_port_output];
    OMX_BOOL bWouldFail = OMX_FALSE;
    
    NVXTRACE((NVXT_WORKER, pComponent->eObjectType, "+NvxAudioEncoderWorkerFunction\n"));
    
    *pbMoreWork = bAllPortsReady;
    
    pNvxAudioEncoder = (SNvxAudioEncoderData *)pComponent->pComponentData;
    
    if (!pPortIn->pCurrentBufferHdr)
        NvxPortGetNextHdr(pPortIn);
    
    if (pPortIn->pCurrentBufferHdr)
    {
        eError = NvxNvMMTransformWorkerBase(&pNvxAudioEncoder->oBase,
            s_nvx_port_input, &bWouldFail);
    }
    
    if (!pPortIn->pCurrentBufferHdr)
        NvxPortGetNextHdr(pPortIn);
    
    if (!pPortOut->pCurrentBufferHdr)
        NvxPortGetNextHdr(pPortOut);
    
    *pbMoreWork = (pPortIn->pCurrentBufferHdr &&
        !bWouldFail);
    return eError;
}

static OMX_ERRORTYPE NvxAudioEncoderAcquireResources(OMX_IN NvxComponent *pComponent)
{
    SNvxAudioEncoderData  *pNvxAudioEncoder = 0;
    OMX_ERRORTYPE       eError          = OMX_ErrorNone;
    
    NV_ASSERT(pComponent);
    
    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAudioEncoderAcquireResources\n"));
    
    /* Get Aac Encoder instance */
    pNvxAudioEncoder = (SNvxAudioEncoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxAudioEncoder);
    
    eError = NvxComponentBaseAcquireResources(pComponent);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pComponent->eObjectType, "ERROR:NvxAudioEncoderAcquireResources:"
            ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError; 
    }
    
    /* Open Aac handles and setup frames */
    eError = NvxAudioEncoderOpen( pNvxAudioEncoder, pComponent );
    
    return eError;
}

static OMX_ERRORTYPE NvxAudioEncoderReleaseResources(OMX_IN NvxComponent *pComponent)
{
    SNvxAudioEncoderData *    pNvxAudioEncoder  = 0;
    OMX_ERRORTYPE           eError          = OMX_ErrorNone;
    
    NV_ASSERT(pComponent);
    
    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAudioEncoderReleaseResources\n"));
    
    /* Get Aac Encoder instance */
    pNvxAudioEncoder = (SNvxAudioEncoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxAudioEncoder);
    
    eError = NvxAudioEncoderClose( pNvxAudioEncoder );
    NvxCheckError(eError, NvxComponentBaseReleaseResources(pComponent));
    return eError;
}

static OMX_ERRORTYPE NvxAudioEncoderFlush(OMX_IN NvxComponent *pComponent, OMX_U32 nPort)
{
    SNvxAudioEncoderData *    pNvxAudioEncoder  = 0;
    
    NV_ASSERT(pComponent);
    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxAudioEncoderFlush\n"));
    
    /* Get Aac Encoder instance */
    pNvxAudioEncoder = (SNvxAudioEncoderData *)pComponent->pComponentData;
    NV_ASSERT(pComponent);
    
    return NvxNvMMTransformFlush(&pNvxAudioEncoder->oBase, nPort);
}

/* =========================================================================
*                     I N T E R N A L
* ========================================================================= */

OMX_ERRORTYPE NvxAudioEncoderOpen( SNvxAudioEncoderData * pNvxAudioEncoder, NvxComponent *pComponent )
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvS32 min_inpbuf_size = MIN_INPUT_BUFFERSIZE;
    
    NVXTRACE(( NVXT_CALLGRAPH, NVXT_AUDIO_DECODER, "+NvxAudioEncoderOpen\n"));
    
    eError = NvxNvMMTransformOpen(&pNvxAudioEncoder->oBase, pNvxAudioEncoder->oBlockType, pNvxAudioEncoder->sBlockName, s_nvx_num_ports, pComponent);
    if (eError != OMX_ErrorNone)
        return eError;

    if (NvMMBlockType_EncAMRNB == pNvxAudioEncoder->oBlockType)
    {
        min_inpbuf_size = MIN_INPUT_BUFFERSIZE_AMRNB;
    }
    else if (NvMMBlockType_EncAMRWB == pNvxAudioEncoder->oBlockType)
    {
        min_inpbuf_size = MIN_INPUT_BUFFERSIZE_AMRWB;
    }
    else if (NvMMBlockType_EnciLBC == pNvxAudioEncoder->oBlockType)
    {
        min_inpbuf_size = MIN_INPUT_BUFFERSIZE_ILBC;
    }
    else if (NvMMBlockType_EncAAC == pNvxAudioEncoder->oBlockType)
    {
        OMX_AUDIO_PARAM_AACPROFILETYPE *pNvxAacEncoderParameters = 
            (OMX_AUDIO_PARAM_AACPROFILETYPE *)pComponent->pPorts[s_nvx_port_output].pPortPrivate;

        if((pNvxAacEncoderParameters->eAACProfile == OMX_AUDIO_AACObjectHE) || (pNvxAacEncoderParameters->eAACProfile == OMX_AUDIO_AACObjectHE_PS))
        {
            min_inpbuf_size = MIN_INPUT_BUFFERSIZE_AACPLUS;
        }
    }

    eError = NvxNvMMTransformSetupInput(&pNvxAudioEncoder->oBase, s_nvx_port_input, &pComponent->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERS, min_inpbuf_size, OMX_FALSE);
    if (eError != OMX_ErrorNone)
        return eError;
    
    eError = NvxNvMMTransformSetupOutput(&pNvxAudioEncoder->oBase, s_nvx_port_output, &pComponent->pPorts[s_nvx_port_output], s_nvx_port_input, MAX_OUTPUT_BUFFERS, MIN_OUTPUT_BUFFERSIZE);
    if (eError != OMX_ErrorNone && eError != OMX_ErrorNotReady)
        return eError;
    
    if (NvMMBlockType_EncAAC == pNvxAudioEncoder->oBlockType)
    {
        NvMMAudioEncPropertyAAC  oEncPropertyAAC;
        OMX_AUDIO_PARAM_AACPROFILETYPE *pNvxAacEncoderParameters = 
            (OMX_AUDIO_PARAM_AACPROFILETYPE *)pComponent->pPorts[s_nvx_port_output].pPortPrivate;
        
        NvOsMemset(&oEncPropertyAAC, 0, sizeof(NvMMAudioEncPropertyAAC));
        oEncPropertyAAC.structSize = sizeof(NvMMAudioEncPropertyAAC);
        oEncPropertyAAC.InputChannels = pNvxAacEncoderParameters->nChannels;
        oEncPropertyAAC.SampleRate = pNvxAacEncoderParameters->nSampleRate;
        oEncPropertyAAC.BitRate = pNvxAacEncoderParameters->nBitRate; // 128000;
        if(pNvxAacEncoderParameters->eAACProfile == OMX_AUDIO_AACObjectLC)
        {
        oEncPropertyAAC.Mode = NvMMAudioAacMode_LC;
        }
        else if(pNvxAacEncoderParameters->eAACProfile == OMX_AUDIO_AACObjectHE)
        {
            oEncPropertyAAC.Mode = NvMMAudioAacMode_PLUS;
        }
        else if(pNvxAacEncoderParameters->eAACProfile == OMX_AUDIO_AACObjectHE_PS)
        {
            oEncPropertyAAC.Mode = NvMMAudioAacMode_PLUS_ENHANCED;
        }
        else 
        {
            eError = OMX_ErrorUnsupportedSetting;
            return eError;
        }
        oEncPropertyAAC.CRCEnabled = 0;                 /* Default CRC is OFF */
        oEncPropertyAAC.Tns_Mode = 1;                   /* Default TNS if ON */
        oEncPropertyAAC.quantizerQuality = 2;           /* {0|1|2} (default is highest quality: 2) */
        oEncPropertyAAC.TsOverHeadBits = 56;
        
        pNvxAudioEncoder->oBase.hBlock->SetAttribute(pNvxAudioEncoder->oBase.hBlock,
            NvMMAudioAacAttribute_TrackInfo, NvMMSetAttrFlag_Notification,
            sizeof(oEncPropertyAAC), &oEncPropertyAAC);
        NvOsSemaphoreWait(pNvxAudioEncoder->oBase.SetAttrDoneSema);
    }
    else if (NvMMBlockType_EncAMRNB == pNvxAudioEncoder->oBlockType)
    {
        NvMMAudioEncPropertyAMRNB oEncPropertyAMRNB;
        OMX_AUDIO_PARAM_AMRTYPE *pNvxAmrnbEncoderParameters = 
            (OMX_AUDIO_PARAM_AMRTYPE *)pComponent->pPorts[s_nvx_port_output].pPortPrivate;
        
        NvOsMemset(&oEncPropertyAMRNB, 0, sizeof(NvMMAudioEncPropertyAMRNB));
        oEncPropertyAMRNB.structSize = sizeof(NvMMAudioEncPropertyAMRNB);
        oEncPropertyAMRNB.Mode = (NvU32)(pNvxAmrnbEncoderParameters->eAMRBandMode - OMX_AUDIO_AMRBandModeNB0);   // NvMMAudioAmrMode_1220
        switch(pNvxAmrnbEncoderParameters->eAMRDTXMode)
        {
            case OMX_AUDIO_AMRDTXModeOff:
                oEncPropertyAMRNB.DTxState = 0;
                break;
            case OMX_AUDIO_AMRDTXModeOnVAD1:
            case OMX_AUDIO_AMRDTXModeOnVAD2:
                oEncPropertyAMRNB.DTxState = 1;
                break;
            default:
                oEncPropertyAMRNB.DTxState = 0;
                break;
        }
        switch(pNvxAmrnbEncoderParameters->eAMRFrameFormat)
        {
        case OMX_AUDIO_AMRFrameFormatFSF:
            oEncPropertyAMRNB.FileFormat = 0;
            break;
        case OMX_AUDIO_AMRFrameFormatIF1:
            oEncPropertyAMRNB.FileFormat = 1;
            break;
        case OMX_AUDIO_AMRFrameFormatIF2:
            oEncPropertyAMRNB.FileFormat = 2;
            break;
        default:
            oEncPropertyAMRNB.FileFormat = 0;               // 0 = MMS,  1 = IF1,  2 = IF2
            break;
        }
        pNvxAudioEncoder->oBase.hBlock->SetAttribute(pNvxAudioEncoder->oBase.hBlock,
            NvMMAudioAmrAttribute_Mode, NvMMSetAttrFlag_Notification,
            sizeof(NvU32),
            &oEncPropertyAMRNB.Mode);
        NvOsSemaphoreWait(pNvxAudioEncoder->oBase.SetAttrDoneSema);
        pNvxAudioEncoder->oBase.hBlock->SetAttribute(pNvxAudioEncoder->oBase.hBlock,
            NvMMAudioAmrAttribute_MMS_Format, NvMMSetAttrFlag_Notification,
            sizeof(NvU32),
            &oEncPropertyAMRNB.FileFormat);
        NvOsSemaphoreWait(pNvxAudioEncoder->oBase.SetAttrDoneSema);
        pNvxAudioEncoder->oBase.hBlock->SetAttribute(pNvxAudioEncoder->oBase.hBlock,
            NvMMAudioAmrAttribute_DTxState, NvMMSetAttrFlag_Notification,
            sizeof(NvU32),
            &oEncPropertyAMRNB.DTxState);
        NvOsSemaphoreWait(pNvxAudioEncoder->oBase.SetAttrDoneSema);
    }
    else if (NvMMBlockType_EncAMRWB == pNvxAudioEncoder->oBlockType)
    {
        NvMMAudioEncPropertyAMRWB oEncPropertyAMRWB;
        OMX_AUDIO_PARAM_AMRTYPE *pNvxAmrwbEncoderParameters = 
            (OMX_AUDIO_PARAM_AMRTYPE *)pComponent->pPorts[s_nvx_port_output].pPortPrivate;
        
        NvOsMemset(&oEncPropertyAMRWB, 0, sizeof(NvMMAudioEncPropertyAMRWB));
        oEncPropertyAMRWB.structSize = sizeof(NvMMAudioEncPropertyAMRWB);
        oEncPropertyAMRWB.Mode = (NvU32)(pNvxAmrwbEncoderParameters->eAMRBandMode - OMX_AUDIO_AMRBandModeWB0);   
        oEncPropertyAMRWB.Format = NvMMAudioAmrWBFormat_MIME;    //Default , MIME. ITU

        switch(pNvxAmrwbEncoderParameters->eAMRDTXMode)
        {
            case OMX_AUDIO_AMRDTXModeOff:
                oEncPropertyAMRWB.DTxState = 0;
                break;
            case OMX_AUDIO_AMRDTXModeOnVAD1:
            case OMX_AUDIO_AMRDTXModeOnVAD2:
                oEncPropertyAMRWB.DTxState = 1;
                break;
            default:
                oEncPropertyAMRWB.DTxState = 0;
                break;
        }
        switch(pNvxAmrwbEncoderParameters->eAMRFrameFormat)
        {
        case OMX_AUDIO_AMRFrameFormatFSF:
            oEncPropertyAMRWB.FileFormat = 0;
            break;
        case OMX_AUDIO_AMRFrameFormatIF1:
            oEncPropertyAMRWB.FileFormat = 1;
            break;
        case OMX_AUDIO_AMRFrameFormatIF2:
            oEncPropertyAMRWB.FileFormat = 2;
            break;
        default:
            oEncPropertyAMRWB.FileFormat = 0;               // 0 = MMS,  1 = IF1,  2 = IF2
            break;
        }

        pNvxAudioEncoder->oBase.hBlock->SetAttribute(pNvxAudioEncoder->oBase.hBlock,
            NvMMAudioAmrWBAttribute_Mode, NvMMSetAttrFlag_Notification,
            sizeof(NvU32),
            &oEncPropertyAMRWB.Mode);
        NvOsSemaphoreWait(pNvxAudioEncoder->oBase.SetAttrDoneSema);
        pNvxAudioEncoder->oBase.hBlock->SetAttribute(pNvxAudioEncoder->oBase.hBlock,
            NvMMAudioAmrWBAttribute_MMS_Format, NvMMSetAttrFlag_Notification,
            sizeof(NvU32),
            &oEncPropertyAMRWB.FileFormat);
        NvOsSemaphoreWait(pNvxAudioEncoder->oBase.SetAttrDoneSema);
        pNvxAudioEncoder->oBase.hBlock->SetAttribute(pNvxAudioEncoder->oBase.hBlock,
            NvMMAudioAmrWBAttribute_DTxState, NvMMSetAttrFlag_Notification,
            sizeof(NvU32),
            &oEncPropertyAMRWB.DTxState);
        NvOsSemaphoreWait(pNvxAudioEncoder->oBase.SetAttrDoneSema);        
        pNvxAudioEncoder->oBase.hBlock->SetAttribute(pNvxAudioEncoder->oBase.hBlock,
            NvMMAudioAmrWBAttribute_Format, NvMMSetAttrFlag_Notification,
            sizeof(NvU32),
            &oEncPropertyAMRWB.Format);
        NvOsSemaphoreWait(pNvxAudioEncoder->oBase.SetAttrDoneSema);
    }
    else if (NvMMBlockType_EnciLBC == pNvxAudioEncoder->oBlockType)
    {
        //NvU32 Mode;
        //OMX_AUDIO_PARAM_ILBCTYPE *pNvxilbcEncoderParameters = 
        //    (OMX_AUDIO_PARAM_ILBCTYPE *)pComponent->pPorts[s_nvx_port_output].pPortPrivate;

        //if (pNvxilbcEncoderParameters->eILBCMode)
        //    Mode = OMX_AUDIO_ILBC_MODE1;
        //else
        //    Mode = OMX_AUDIO_ILBC_MODE0;

        //pNvxAudioEncoder->oBase.hBlock->SetAttribute(pNvxAudioEncoder->oBase.hBlock,
        //    NvMMAudioILBCAttribute_Mode, NvMMSetAttrFlag_Notification,
        //    sizeof(NvU32),
        //    &Mode);
        //NvOsSemaphoreWait(pNvxAudioEncoder->oBase.SetAttrDoneSema);
    }
    else if (NvMMBlockType_EncWAV == pNvxAudioEncoder->oBlockType)
    {

        NvMMAudioEncPropertyWAV oEncPropertyWav;
        OMX_AUDIO_PARAM_PCMMODETYPE *pNvxWavEncoderParameters = 
            (OMX_AUDIO_PARAM_PCMMODETYPE *)pComponent->pPorts[s_nvx_port_output].pPortPrivate;
        
        NvOsMemset(&oEncPropertyWav, 0, sizeof(NvMMAudioEncPropertyWAV));
        oEncPropertyWav.structSize = sizeof(NvMMAudioEncPropertyWAV);
        
        if(pNvxWavEncoderParameters->ePCMMode == OMX_AUDIO_PCMModeLinear)
        {
            oEncPropertyWav.Mode = NvMMAudioWavMode_PCM;
        }
        else if(pNvxWavEncoderParameters->ePCMMode == OMX_AUDIO_PCMModeALaw)
        {
            oEncPropertyWav.Mode = NvMMAudioWavMode_ALAW;
        }
        else if(pNvxWavEncoderParameters->ePCMMode == OMX_AUDIO_PCMModeMULaw)
        {
            oEncPropertyWav.Mode = NvMMAudioWavMode_MULAW;
        }
        else 
        {
            eError = OMX_ErrorUnsupportedSetting;
            return eError;
        }
        
        
        oEncPropertyWav.BitRate = 0;
        oEncPropertyWav.SampleRate = pNvxWavEncoderParameters->nSamplingRate;
        oEncPropertyWav.InputChannels = pNvxWavEncoderParameters->nChannels;
        pNvxAudioEncoder->oBase.hBlock->SetAttribute(pNvxAudioEncoder->oBase.hBlock,
            NvMMWaveAttribute_TrackInfo, NvMMSetAttrFlag_Notification,
            sizeof(NvMMAudioEncPropertyWAV),
            &oEncPropertyWav);
        NvOsSemaphoreWait(pNvxAudioEncoder->oBase.SetAttrDoneSema);
    }
    
    pNvxAudioEncoder->bInitialized = OMX_TRUE;
    
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxAudioEncoderClose(SNvxAudioEncoderData * pNvxAudioEncoder)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    
    NVXTRACE((NVXT_CALLGRAPH, NVXT_AUDIO_DECODER, "+NvxAudioEncoderClose\n"));
    
    if (!pNvxAudioEncoder->bInitialized)
        return OMX_ErrorNone;
    
    eError = NvxNvMMTransformClose(&pNvxAudioEncoder->oBase);
    
    if (eError != OMX_ErrorNone)
        return eError;
    
    pNvxAudioEncoder->bInitialized = OMX_FALSE;
    
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxAudioEncoderFillThisBuffer(NvxComponent *pNvComp, OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    SNvxAudioEncoderData *pNvxAudioEncoder = 0;
    
    NV_ASSERT(pNvComp);
    
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioEncoderFillThisBuffer\n"));
    
    pNvxAudioEncoder = (SNvxAudioEncoderData *)pNvComp->pComponentData;
    if (!pNvxAudioEncoder->bInitialized)
        return OMX_ErrorNone;
    
    return NvxNvMMTransformFillThisBuffer(&pNvxAudioEncoder->oBase, pBufferHdr, s_nvx_port_output);
}


static OMX_ERRORTYPE NvxAudioEncoderPreChangeState(NvxComponent *pNvComp,
                                                   OMX_STATETYPE oNewState)
{
    SNvxAudioEncoderData *pNvxAudioEncoder = 0;
    
    NV_ASSERT(pNvComp);
    
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioEncoderPreChangeState\n"));
    
    pNvxAudioEncoder = (SNvxAudioEncoderData *)pNvComp->pComponentData;
    if (!pNvxAudioEncoder->bInitialized)
        return OMX_ErrorNone;
    
    return NvxNvMMTransformPreChangeState(&pNvxAudioEncoder->oBase, oNewState,
        pNvComp->eState);
}

static OMX_ERRORTYPE NvxAudioEncoderEmptyThisBuffer(NvxComponent *pNvComp,
                                                    OMX_BUFFERHEADERTYPE* pBufferHdr,
                                                    OMX_BOOL *bHandled)
{
    SNvxAudioEncoderData *pNvxAudioEncoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    
    NV_ASSERT(pNvComp);
    
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAudioEncoderEmptyThisBuffer\n"));
    
    pNvxAudioEncoder = (SNvxAudioEncoderData *)pNvComp->pComponentData;
    if (!pNvxAudioEncoder->bInitialized)
        return OMX_ErrorNone;
    
    if (NvxNvMMTransformIsInputSkippingWorker(&pNvxAudioEncoder->oBase,
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
static OMX_ERRORTYPE NvxCommonAudioEncoderInit(OMX_HANDLETYPE hComponent, NvMMBlockType oBlockType, const char *sBlockName)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pThis;
    SNvxAudioEncoderData *data = NULL;
    
    OMX_AUDIO_PARAM_PCMMODETYPE  *pNvxPcmParameters;
    
    eError = NvxComponentCreate(hComponent, s_nvx_num_ports, &pThis);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType,"ERROR:NvxCommonAudioEncoderInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;  
    }
    
    NvxAddResource(pThis, NVX_AUDIO_MEDIAPROCESSOR_RES);
    
    pThis->eObjectType = NVXT_AUDIO_DECODER;
    
    data = NvOsAlloc(sizeof(SNvxAudioEncoderData));
    if (!data)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType,"ERROR:NvxCommonAudioEncoderInit:"
            ":SNvxAudioData = %d:[%s(%d)]\n",data,__FILE__, __LINE__));
        return OMX_ErrorInsufficientResources;  
    }
    
    NvOsMemset(data, 0, sizeof(SNvxAudioEncoderData));
    
    data->oBlockType = oBlockType;
    data->sBlockName = sBlockName;
    
    pThis->pComponentData = data;
    
    pThis->DeInit             = NvxAudioEncoderDeInit;
    pThis->GetParameter       = NvxAudioEncoderGetParameter;
    pThis->SetParameter       = NvxAudioEncoderSetParameter;
    pThis->GetConfig          = NvxAudioEncoderGetConfig;
    pThis->SetConfig          = NvxAudioEncoderSetConfig;
    pThis->WorkerFunction     = NvxAudioEncoderWorkerFunction;
    pThis->AcquireResources   = NvxAudioEncoderAcquireResources;
    pThis->ReleaseResources   = NvxAudioEncoderReleaseResources;
    pThis->FillThisBufferCB   = NvxAudioEncoderFillThisBuffer;
    pThis->PreChangeState     = NvxAudioEncoderPreChangeState;
    pThis->EmptyThisBuffer    = NvxAudioEncoderEmptyThisBuffer;
    pThis->Flush              = NvxAudioEncoderFlush;
    
    NvxPortInitAudio(&pThis->pPorts[s_nvx_port_input], OMX_DirInput, MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE, OMX_AUDIO_CodingPCM);
    pThis->pPorts[s_nvx_port_output].eNvidiaTunnelTransaction = ENVX_TRANSACTION_NVMM_TUNNEL;
    
    /* Initialize default parameters - required of a standard component */
    ALLOC_STRUCT(pThis, pNvxPcmParameters, OMX_AUDIO_PARAM_PCMMODETYPE);
    if (!pNvxPcmParameters)
        return OMX_ErrorInsufficientResources;
    
    //Initialize the default values for the standard component requirement.
    pNvxPcmParameters->nPortIndex = s_nvx_port_input;
    pNvxPcmParameters->nChannels = 2;
    pNvxPcmParameters->eNumData = OMX_NumericalDataSigned;
    pNvxPcmParameters->nSamplingRate = 48000;
    pNvxPcmParameters->ePCMMode = OMX_AUDIO_PCMModeLinear;
    pNvxPcmParameters->bInterleaved = OMX_TRUE;
    pNvxPcmParameters->nBitPerSample = 16;
    pNvxPcmParameters->eChannelMapping[0] = OMX_AUDIO_ChannelLF;
    pNvxPcmParameters->eChannelMapping[1] = OMX_AUDIO_ChannelRF;
    
    pThis->pPorts[s_nvx_port_input].pPortPrivate = pNvxPcmParameters;
    
    return eError;
}

/**** AAC ****/

OMX_ERRORTYPE NvxAacEncoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pThis = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_AUDIO_PARAM_AACPROFILETYPE  *pNvxAacEncoderParameters;
    
    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxAudioEncoderInit\n"));
    
    eError = NvxCommonAudioEncoderInit(hComponent, NvMMBlockType_EncAAC, "BlockAACEnc");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxAudioEncoderInit:"
            ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;  
    }
    
    pThis = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    
    /* Register function pointers */
    pThis->pComponentName = "OMX.Nvidia.aac.encoder";
    pThis->nComponentRoles = 1;
    pThis->sComponentRoles[0] = "audio_encoder.aac";
    
    ALLOC_STRUCT(pThis, pNvxAacEncoderParameters, OMX_AUDIO_PARAM_AACPROFILETYPE);
    if (!pNvxAacEncoderParameters)
        return OMX_ErrorInsufficientResources;
    
    //Initialize the default values for the standard component requirement.
    pNvxAacEncoderParameters->nPortIndex = s_nvx_port_output;
    pNvxAacEncoderParameters->nChannels = 2;
    pNvxAacEncoderParameters->nSampleRate = 44100;
    pNvxAacEncoderParameters->nBitRate = 128000; //288000;  // This is the minimum.
    pNvxAacEncoderParameters->nAudioBandWidth = 0;
    pNvxAacEncoderParameters->nFrameLength = 0;
    pNvxAacEncoderParameters->eAACProfile = OMX_AUDIO_AACObjectLC;
    pNvxAacEncoderParameters->eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP2ADTS;
    pNvxAacEncoderParameters->eChannelMode = OMX_AUDIO_ChannelModeStereo;
    
    pThis->pPorts[s_nvx_port_output].pPortPrivate = pNvxAacEncoderParameters;
    
    NvxPortInitAudio(&pThis->pPorts[s_nvx_port_output], OMX_DirOutput, MAX_OUTPUT_BUFFERS + 5, MIN_OUTPUT_BUFFERSIZE, OMX_AUDIO_CodingAAC);
    
    return eError;
}

/**** AMR nb encoder init ****/

OMX_ERRORTYPE NvxAmrnbEncoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent *pThis;
    
    OMX_AUDIO_PARAM_AMRTYPE  *pNvxAmrnbEncoderParameters;
    
    eError = NvxCommonAudioEncoderInit(hComponent, NvMMBlockType_EncAMRNB, "BlockAmrnbEnc");
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED,"ERROR:NvxAmrnbAudioEncoderInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;  
    }
    
    pThis = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    
    pThis->pComponentName = "OMX.Nvidia.amr.encoder";
    pThis->nComponentRoles = 1;
    pThis->sComponentRoles[0] = "audio_encoder.amrnb";

    NvxPortInitAudio(&pThis->pPorts[s_nvx_port_input], OMX_DirInput, MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE_AMRNB, OMX_AUDIO_CodingPCM);
    NvxPortInitAudio(&pThis->pPorts[s_nvx_port_output], OMX_DirOutput, MAX_OUTPUT_BUFFERS + 5, MIN_OUTPUT_BUFFERSIZE, OMX_AUDIO_CodingAMR);
    
    // Std Component must have default parameter values set.
    ALLOC_STRUCT(pThis, pNvxAmrnbEncoderParameters, OMX_AUDIO_PARAM_AMRTYPE);
    if (!pNvxAmrnbEncoderParameters)
        return OMX_ErrorInsufficientResources;
    
    //Initialize the default values for the standard component requirement.
    pNvxAmrnbEncoderParameters->nPortIndex = s_nvx_port_output;
    pNvxAmrnbEncoderParameters->nChannels = 1;
    pNvxAmrnbEncoderParameters->nBitRate = 12200;
    pNvxAmrnbEncoderParameters->eAMRBandMode = OMX_AUDIO_AMRBandModeNB7;  // 0 through 7
    
    pNvxAmrnbEncoderParameters->eAMRDTXMode = OMX_AUDIO_AMRDTXModeOff;
    pNvxAmrnbEncoderParameters->eAMRFrameFormat = OMX_AUDIO_AMRFrameFormatConformance;
    pThis->pPorts[s_nvx_port_output].pPortPrivate = pNvxAmrnbEncoderParameters;
    
    return eError;
}

/**** AMR wb encoder init ****/

OMX_ERRORTYPE NvxAmrwbEncoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent *pThis;
    
    OMX_AUDIO_PARAM_AMRTYPE  *pNvxAmrwbEncoderParameters;
    
    eError = NvxCommonAudioEncoderInit(hComponent, NvMMBlockType_EncAMRWB, "BlockAmrwbEnc");
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED,"ERROR:NvxAmrwbAudioEncoderInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;  
    }
    
    pThis = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    
    pThis->pComponentName = "OMX.Nvidia.amrwb.encoder";
    pThis->nComponentRoles = 1;
    pThis->sComponentRoles[0] = "audio_encoder.amrwb";

    NvxPortInitAudio(&pThis->pPorts[s_nvx_port_input], OMX_DirInput, MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE_AMRWB, OMX_AUDIO_CodingPCM);
    NvxPortInitAudio(&pThis->pPorts[s_nvx_port_output], OMX_DirOutput, MAX_OUTPUT_BUFFERS + 5, MIN_OUTPUT_BUFFERSIZE, OMX_AUDIO_CodingAMR);
    
    // Std Component must have default parameter values set.
    ALLOC_STRUCT(pThis, pNvxAmrwbEncoderParameters, OMX_AUDIO_PARAM_AMRTYPE);
    if (!pNvxAmrwbEncoderParameters)
        return OMX_ErrorInsufficientResources;
    
    //Initialize the default values for the standard component requirement.
    pNvxAmrwbEncoderParameters->nPortIndex = s_nvx_port_output;
    pNvxAmrwbEncoderParameters->nChannels = 1;
    pNvxAmrwbEncoderParameters->nBitRate = 23850;
    pNvxAmrwbEncoderParameters->eAMRBandMode = OMX_AUDIO_AMRBandModeWB8;  // 0 through 8
    
    pNvxAmrwbEncoderParameters->eAMRDTXMode = OMX_AUDIO_AMRDTXModeOff;
    pNvxAmrwbEncoderParameters->eAMRFrameFormat = OMX_AUDIO_AMRFrameFormatConformance;
    pThis->pPorts[s_nvx_port_output].pPortPrivate = pNvxAmrwbEncoderParameters;
    
    return eError;
}

/**** ilbc encoder init ****/

OMX_ERRORTYPE NvxilbcEncoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent *pThis;
    
//    OMX_AUDIO_PARAM_ILBCTYPE  *pNvxilbcEncoderParameters;
    
    eError = NvxCommonAudioEncoderInit(hComponent, NvMMBlockType_EnciLBC, "BlockilbcEnc");
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED,"ERROR:NvxAmrwbAudioEncoderInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;  
    }
    pThis = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    // There is no NvMM layer here.
    pThis->pPorts[s_nvx_port_output].eNvidiaTunnelTransaction = ENVX_TRANSACTION_DEFAULT;

    pThis->pComponentName = "OMX.Nvidia.ilbc.encoder";
    pThis->nComponentRoles = 1;
    pThis->sComponentRoles[0] = "audio_encoder.ilbc";

    NvxPortInitAudio(&pThis->pPorts[s_nvx_port_input], OMX_DirInput, MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE_ILBC, OMX_AUDIO_CodingPCM);
    //NvxPortInitAudio(&pThis->pPorts[s_nvx_port_output], OMX_DirOutput, MAX_OUTPUT_BUFFERS + 5, MIN_OUTPUT_BUFFERSIZE_ILBC, OMX_AUDIO_CodingILBC);
    
    // Std Component must have default parameter values set.
    //ALLOC_STRUCT(pThis, pNvxilbcEncoderParameters, OMX_AUDIO_PARAM_ILBCTYPE);
    //if (!pNvxilbcEncoderParameters)
    //    return OMX_ErrorInsufficientResources;
    //
    ////Initialize the default values for the standard component requirement.
    //pNvxilbcEncoderParameters->nPortIndex = s_nvx_port_output;
    //pNvxilbcEncoderParameters->nChannels = 1;
    //pNvxilbcEncoderParameters->eILBCMode = 0;

    //pThis->pPorts[s_nvx_port_output].pPortPrivate = pNvxilbcEncoderParameters;
    
    return eError;
}

/**** Wave encoder init ****/

OMX_ERRORTYPE NvxWavEncoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent *pThis;
    OMX_AUDIO_PARAM_PCMMODETYPE *pNvxwavEncoderParameters;
    

    eError = NvxCommonAudioEncoderInit(hComponent, NvMMBlockType_EncWAV, "BlockAmrwbEnc");
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED,"ERROR:NvxAmrwbAudioEncoderInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;  
    }
    pThis = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    
    pThis->pComponentName = "OMX.Nvidia.wav.encoder";
    pThis->nComponentRoles = 1;
    pThis->sComponentRoles[0] = "audio_encoder.wav";

    NvxPortInitAudio(&pThis->pPorts[s_nvx_port_input], OMX_DirInput, MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE, OMX_AUDIO_CodingPCM);
    NvxPortInitAudio(&pThis->pPorts[s_nvx_port_output], OMX_DirOutput, MAX_OUTPUT_BUFFERS + 5, MIN_OUTPUT_BUFFERSIZE, OMX_AUDIO_CodingPCM);
    
    // Std Component must have default parameter values set.
    ALLOC_STRUCT(pThis, pNvxwavEncoderParameters, OMX_AUDIO_PARAM_PCMMODETYPE);
    if (!pNvxwavEncoderParameters)
        return OMX_ErrorInsufficientResources;

    pNvxwavEncoderParameters->nPortIndex = s_nvx_port_output;
    pNvxwavEncoderParameters->nChannels = 1;
    pNvxwavEncoderParameters->nBitPerSample = 16;
    pNvxwavEncoderParameters->nSamplingRate = 44100;
    pNvxwavEncoderParameters->ePCMMode = OMX_AUDIO_PCMModeLinear;


    pThis->pPorts[s_nvx_port_output].pPortPrivate = pNvxwavEncoderParameters;
    
    return eError;
}
