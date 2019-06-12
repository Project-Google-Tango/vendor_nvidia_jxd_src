/*
 * Copyright (c) 2006-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** NvxVideoDecoder.c */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

#include <OMX_Core.h>
#include "nvassert.h"
#include "nvos.h"
#include "common/NvxTrace.h"
#include "components/common/NvxComponent.h"
#include "components/common/NvxPort.h"
#include "components/common/NvxCompReg.h"
#include "nvmm/components/common/nvmmtransformbase.h"

#include "nvmmlite_video.h"
#include "nvmm_videodec.h"
#include "nvmm_mpeg2vdec.h"

#if USE_ANDROID_NATIVE_WINDOW
#include "NvxHelpers.h"
#include "nvxandroidbuffer.h"
#include "nvgr.h"
#endif

#include "nvfxmath.h"

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */

// DEBUG defines. TO DO: fix this
#define MIN_INPUT_BUFFERS    2
#define MAX_INPUT_BUFFERS    10
#define MAX_OUTPUT_BUFFERS   20
#define MIN_INPUT_BUFFERSIZE 1024
#define MAX_INPUT_BUFFERSIZE 1530*1024
#define MIN_OUTPUT_BUFFERSIZE 384
#define MAX_OUTPUT_BUFFERSIZE (640*480*3)/2

/* Number of internal frame buffers (video memory) */
static const int s_nvx_num_ports            = 2;
static const unsigned s_nvx_port_input           = 0;
static const unsigned s_nvx_port_output          = 1;

/* Decoder State information */
typedef struct SNvxVideoDecoderData
{
    OMX_BOOL bInitialized;
    OMX_BOOL bOverrideTimestamp;
    OMX_BOOL bErrorReportingEnabled;

    SNvxNvMMTransformData oBase;
    SNvxNvMMTransformData oBaseTwo;
    OMX_BOOL bFirstFrame;

#define TYPE_MP4   0
#define TYPE_H264  1
#define TYPE_VC1   2
#define TYPE_MPEG2 3
#define TYPE_MJPEG 6
#define TYPE_VP8   7

    int oType;

    NvMMBlockType oBlockType;
    const char *sBlockName;
    OMX_BOOL bLowMem;

    OMX_BOOL bSkipFrames;
    OMX_BOOL bUseLowResource;
    OMX_BOOL bH264DisableDPB;
    OMX_BOOL bFullFrameData;
    OMX_U32  nTunneledBufferCount; /* Requested number of buffers when in Tunneled mode*/
    NvMMMpeg2VDec_DeinterlacingOptions AttribDeint;     /* deinterlacing method */
} SNvxVideoDecoderData;

static OMX_ERRORTYPE NvxMpeg2Open(SNvxVideoDecoderData *pNvxVideoDecoder, NvxComponent *pComponent);
static OMX_ERRORTYPE NvxVideoDecoderReleaseResources(OMX_IN NvxComponent *pComponent);
static OMX_ERRORTYPE NvxVideoDecoderAcquireResources(OMX_IN NvxComponent *pComponent);

extern void NvxNvMMLiteConvertProfilLevelToOmx(NvMMLiteVideoCodingType eType,
                                               OMX_U32 nNvmmPfl,
                                               OMX_U32 nNvmmLvl,
                                               OMX_U32 *pOmxPfl,
                                               OMX_U32 *pOmxLvl);

static OMX_ERRORTYPE NvxVideoDecoderDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    SNvxVideoDecoderData *pNvxVideoDecoder;
    pNvxVideoDecoder = (SNvxVideoDecoderData *)pNvComp->pComponentData;

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoDecoderInit\n"));

    if (pNvxVideoDecoder->oBase.hBlock)
        NvxNvMMTransformClose(&pNvxVideoDecoder->oBase);

    NvOsFree(pNvComp->pComponentData);
    pNvComp->pComponentData = 0;
    return eError;
}

static OMX_ERRORTYPE NvxVideoDecoderGetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    SNvxVideoDecoderData *pNvxVideoDecoder;

    pNvxVideoDecoder = (SNvxVideoDecoderData *)pComponent->pComponentData;

    NV_ASSERT(pNvxVideoDecoder);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoDecoderGetParameter\n"));

    switch (nParamIndex)
    {
    case OMX_IndexParamVideoH263:
        {
            NvMMVideoDec_H263ParamsInfo H263params;
            OMX_VIDEO_PARAM_H263TYPE *pH263Params;
            NvError err = OMX_ErrorNone;
            if (((OMX_VIDEO_PARAM_H263TYPE *)pComponentParameterStructure)->nPortIndex != (NvU32)s_nvx_port_input || pNvxVideoDecoder->oType != TYPE_MP4)
                return OMX_ErrorBadPortIndex;

            if(!pNvxVideoDecoder->oBase.hBlock)
                return OMX_ErrorNotReady;

            err = pNvxVideoDecoder->oBase.hBlock->GetAttribute(
                            pNvxVideoDecoder->oBase.hBlock, NvMMVideoDecAttribute_H263TYPE,
                            sizeof(NvMMVideoDec_H263ParamsInfo), &H263params);

            if(NvError_BadParameter == err)
                return OMX_ErrorBadParameter;
            else if (NvSuccess != err)
                return OMX_ErrorNotReady;

            pH263Params = pComponent->pPorts[s_nvx_port_input].pPortPrivate;

            NvxNvMMLiteConvertProfilLevelToOmx(NvMMLiteVideo_CodingTypeH263,
                                               H263params.eProfile,
                                               H263params.eLevel,
                                               (OMX_U32 *)&pH263Params->eProfile,
                                               (OMX_U32 *)&pH263Params->eLevel);

            NvOsMemcpy(pComponentParameterStructure,
                       pComponent->pPorts[s_nvx_port_input].pPortPrivate,
                       sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
        }
        break;
    case OMX_IndexParamVideoMpeg4:
        {
            NvMMVideoDec_MPEG4ParamsInfo mpeg4params;
            OMX_VIDEO_PARAM_MPEG4TYPE *pMpeg4Params;
            NvError err = OMX_ErrorNone;
            if (((OMX_VIDEO_PARAM_MPEG4TYPE *)pComponentParameterStructure)->nPortIndex != (NvU32)s_nvx_port_input || pNvxVideoDecoder->oType != TYPE_MP4)
                return OMX_ErrorBadPortIndex;

            if(!pNvxVideoDecoder->oBase.hBlock)
                return OMX_ErrorNotReady;

            err = pNvxVideoDecoder->oBase.hBlock->GetAttribute(
                            pNvxVideoDecoder->oBase.hBlock, NvMMVideoDecAttribute_MPEG4TYPE,
                            sizeof(NvMMVideoDec_MPEG4ParamsInfo), &mpeg4params);
            if (NvSuccess != err)
                return OMX_ErrorNotReady;

            pMpeg4Params = pComponent->pPorts[s_nvx_port_input].pPortPrivate;

            //Copy the parameters
            pMpeg4Params->bSVH = mpeg4params.bSVH;
            pMpeg4Params->nIDCVLCThreshold = mpeg4params.nIDCVLCThreshold;
            pMpeg4Params->nTimeIncRes = mpeg4params.nTimeIncRes;
            pMpeg4Params->bReversibleVLC = mpeg4params.bReversibleVLC;

            NvxNvMMLiteConvertProfilLevelToOmx(NvMMLiteVideo_CodingTypeMPEG4,
                                               mpeg4params.eProfile,
                                               mpeg4params.eLevel,
                                               (OMX_U32 *)&pMpeg4Params->eProfile,
                                               (OMX_U32 *)&pMpeg4Params->eLevel);

            NvOsMemcpy(pComponentParameterStructure,
                       pComponent->pPorts[s_nvx_port_input].pPortPrivate,
                       sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
        }
        break;
    case OMX_IndexParamVideoMpeg2:
        {
            NvMMVideoDec_Mpeg2ParamsInfo mpeg2params;
            OMX_VIDEO_PARAM_MPEG2TYPE *pMpeg2Params;
            NvError err = OMX_ErrorNone;

            if (((OMX_VIDEO_PARAM_MPEG2TYPE *)pComponentParameterStructure)->nPortIndex != (NvU32)s_nvx_port_input || pNvxVideoDecoder->oType != TYPE_MPEG2)
                return OMX_ErrorBadPortIndex;

            if(!pNvxVideoDecoder->oBase.hBlock)
                return OMX_ErrorNotReady;

            err = pNvxVideoDecoder->oBase.hBlock->GetAttribute(
                            pNvxVideoDecoder->oBase.hBlock, NvMMVideoDecAttribute_MPEG2TYPE,
                            sizeof(NvMMVideoDec_Mpeg2ParamsInfo), &mpeg2params);

            if (NvSuccess != err)
                return OMX_ErrorNotReady;

            pMpeg2Params = pComponent->pPorts[s_nvx_port_input].pPortPrivate;
            // copy parameters
            NvxNvMMLiteConvertProfilLevelToOmx(NvMMLiteVideo_CodingTypeMPEG2,
                                               mpeg2params.eProfile,
                                               mpeg2params.eLevel,
                                               (OMX_U32 *)&pMpeg2Params->eProfile,
                                               (OMX_U32 *)&pMpeg2Params->eLevel);

            NvOsMemcpy(pComponentParameterStructure,
                pComponent->pPorts[s_nvx_port_input].pPortPrivate,
                sizeof(OMX_VIDEO_PARAM_MPEG2TYPE));
        }
        break;

    case OMX_IndexParamVideoAvc:
        {
        NvMMVideoDec_AVCParamsInfo avcparams;
        OMX_VIDEO_PARAM_AVCTYPE *pAvcParams;
        NvError err = OMX_ErrorNone;

        if (((OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure)->nPortIndex != (NvU32)s_nvx_port_input || pNvxVideoDecoder->oType != TYPE_H264)
            return OMX_ErrorBadPortIndex;

        if(!pNvxVideoDecoder->oBase.hBlock)
            return OMX_ErrorNotReady;

        err = pNvxVideoDecoder->oBase.hBlock->GetAttribute(
                pNvxVideoDecoder->oBase.hBlock, NvMMVideoDecAttribute_AVCTYPE,
                            sizeof(NvMMVideoDec_AVCParamsInfo), &avcparams);
        if (NvSuccess != err)
            return OMX_ErrorNotReady;

        pAvcParams = pComponent->pPorts[s_nvx_port_input].pPortPrivate;

        // copy avcparams into pPortPrivate
        pAvcParams->nRefFrames = avcparams.nRefFrames;
        pAvcParams->nRefIdx10ActiveMinus1 = avcparams.nRefIdx10ActiveMinus1;
        pAvcParams->nRefIdx11ActiveMinus1 = avcparams.nRefIdx11ActiveMinus1;
        pAvcParams->bFrameMBsOnly = avcparams.bFrameMBsOnly;
        pAvcParams->bMBAFF = avcparams.bMBAFF;
        pAvcParams->bEntropyCodingCABAC = avcparams.bEntropyCodingCABAC;
        pAvcParams->bWeightedPPrediction = avcparams.bWeightedPPrediction;
        pAvcParams->nWeightedBipredicitonMode = avcparams.nWeightedBipredicitonMode;
        pAvcParams->bconstIpred = avcparams.bconstIpred;
        pAvcParams->bDirect8x8Inference = avcparams.bDirect8x8Inference;
        pAvcParams->bDirectSpatialTemporal = avcparams.bDirectSpatialTemporal;
        pAvcParams->nCabacInitIdc = avcparams.nCabacInitIdc;
        pAvcParams->eLoopFilterMode = (OMX_VIDEO_AVCLOOPFILTERTYPE)avcparams.LoopFilterMode;

        NvxNvMMLiteConvertProfilLevelToOmx(NvMMLiteVideo_CodingTypeH264,
                                           avcparams.eProfile,
                                           avcparams.eLevel,
                                           (OMX_U32 *)&pAvcParams->eProfile,
                                           (OMX_U32 *)&pAvcParams->eLevel);

        NvOsMemcpy(pComponentParameterStructure,
                   pComponent->pPorts[s_nvx_port_input].pPortPrivate,
                   sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        break;
    }
    case OMX_IndexParamVideoWmv:
        if (((OMX_VIDEO_PARAM_WMVTYPE *)pComponentParameterStructure)->nPortIndex != (NvU32)s_nvx_port_input || pNvxVideoDecoder->oType != TYPE_VC1)
            return OMX_ErrorBadPortIndex;

        if (!pComponent->pPorts[s_nvx_port_input].pPortPrivate)
            return OMX_ErrorNotImplemented;

        NvOsMemcpy(pComponentParameterStructure,
                   pComponent->pPorts[s_nvx_port_input].pPortPrivate,
                   sizeof(OMX_VIDEO_PARAM_WMVTYPE));
        break;
    case OMX_IndexParamVideoProfileLevelQuerySupported:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE * pProfLvl;
        NvMMLiteVideoProfileLevelInfo info;
        NvError err = OMX_ErrorNone;
        pProfLvl = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;

        info.ProfileIndex = pProfLvl->nProfileIndex;

        if (!pNvxVideoDecoder->oBase.hBlock)
        {
            err = NvxNvMMTransformOpen(&pNvxVideoDecoder->oBase,
                                       pNvxVideoDecoder->oBlockType,
                                       pNvxVideoDecoder->sBlockName,
                                       s_nvx_num_ports,
                                       pComponent);
            if (err != OMX_ErrorNone)
                return err;
        }

        if (pNvxVideoDecoder->oType == TYPE_H264)
        {
            info.eType = NvMMLiteVideo_CodingTypeH264;
        }
        else if (pNvxVideoDecoder->oType == TYPE_MP4)
        {
            if (!NvOsStrcmp(pComponent->pComponentName, "OMX.Nvidia.h263.decode"))
                info.eType = NvMMLiteVideo_CodingTypeH263;
            else
                info.eType = NvMMLiteVideo_CodingTypeMPEG4;
        }
        else if (pNvxVideoDecoder->oType == TYPE_MPEG2)
        {
            info.eType = NvMMLiteVideo_CodingTypeMPEG2;
        }
        else
            return OMX_ErrorNoMore;

        err = pNvxVideoDecoder->oBase.hBlock->GetAttribute(pNvxVideoDecoder->oBase.hBlock,
                                            NvMMVideoDecAttribute_ProfileLevelQuery,
                                            sizeof(NvMMLiteVideoProfileLevelInfo),
                                            &info);

        if (NvSuccess != err)
            return OMX_ErrorNoMore;

        NvxNvMMLiteConvertProfilLevelToOmx(info.eType,
                                           info.Profile,
                                           info.Level,
                                           (OMX_U32 *)&pProfLvl->eProfile,
                                           (OMX_U32 *)&pProfLvl->eLevel);
        break;
    }
    case NVX_IndexParamCodecCapability:
    {
        NVX_PARAM_CODECCAPABILITY * pCap;
        NvMMLiteVideoMaxCapabilities info;
        NvError err = OMX_ErrorNone;
        pCap = (NVX_PARAM_CODECCAPABILITY *)pComponentParameterStructure;

        info.CapIndex = pCap->nCapIndex;

        if (!pNvxVideoDecoder->oBase.hBlock)
        {
            err = NvxNvMMTransformOpen(&pNvxVideoDecoder->oBase,
                                       pNvxVideoDecoder->oBlockType,
                                       pNvxVideoDecoder->sBlockName,
                                       s_nvx_num_ports,
                                       pComponent);
            if (err != OMX_ErrorNone)
                return err;
        }

        if (pNvxVideoDecoder->oType == TYPE_H264)
        {
            info.eType = NvMMLiteVideo_CodingTypeH264;
        }
        else if (pNvxVideoDecoder->oType == TYPE_MP4)
        {
            if (!NvOsStrcmp(pComponent->pComponentName, "OMX.Nvidia.h263.decode"))
                info.eType = NvMMLiteVideo_CodingTypeH263;
            else
                info.eType = NvMMLiteVideo_CodingTypeMPEG4;
        }
        else if (pNvxVideoDecoder->oType == TYPE_MPEG2)
        {
            info.eType = NvMMLiteVideo_CodingTypeMPEG2;
        }
        else
            return OMX_ErrorNoMore;

        err = pNvxVideoDecoder->oBase.hBlock->GetAttribute(pNvxVideoDecoder->oBase.hBlock,
                                            NvMMVideoDecAttribute_MaxCapabilities,
                                            sizeof(NvMMLiteVideoMaxCapabilities),
                                            &info);

        if (NvSuccess != err)
            return OMX_ErrorNoMore;

        NvxNvMMLiteConvertProfilLevelToOmx(info.eType,
                                           info.MaxProfile,
                                           info.MaxLevel,
                                           &pCap->nMaxProfile,
                                           &pCap->nMaxLevel);

        pCap->nMaxWidth   = info.MaxWidth;
        pCap->nMaxHeight  = info.MaxHeight;
        pCap->nFrameRate  = info.FrameRate;
        pCap->nMaxBitRate = info.Bitrate;
        break;
    }
    case OMX_IndexParamVideoMacroblocksPerFrame:
    {
        OMX_PARAM_MACROBLOCKSTYPE* pMacroblocksType = (OMX_PARAM_MACROBLOCKSTYPE *)pComponentParameterStructure;
        if (pMacroblocksType->nPortIndex != (NvU32)s_nvx_port_input)
            return OMX_ErrorBadPortIndex;
        break;
    }
    case OMX_IndexParamPortDefinition:
        pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
        {
            OMX_ERRORTYPE eError = NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
            if((NvU32)s_nvx_port_output == pPortDef->nPortIndex)
            {
                if(OMX_TRUE == pComponent->pPorts[s_nvx_port_output].bNvidiaTunneling)
                {
                    pPortDef->nBufferCountMin = pNvxVideoDecoder->nTunneledBufferCount;
                    pPortDef->nBufferCountActual = pNvxVideoDecoder->nTunneledBufferCount;

                    if(pComponent->pPorts[s_nvx_port_output].nReqBufferCount != pNvxVideoDecoder->nTunneledBufferCount)
                    {
                            pComponent->pPorts[s_nvx_port_output].nReqBufferCount  = pNvxVideoDecoder->nTunneledBufferCount;
                            pComponent->pPorts[s_nvx_port_output].nMinBufferCount  = pNvxVideoDecoder->nTunneledBufferCount;
                    }
                }

                if(pNvxVideoDecoder->oBaseTwo.hBlock)
                {
                    if (pNvxVideoDecoder->oBaseTwo.oPorts[s_nvx_port_output].bHasSize)
                    {
                        pPortDef->format.video.nFrameWidth = pNvxVideoDecoder->oBaseTwo.oPorts[s_nvx_port_output].nWidth;
                        pPortDef->format.video.nFrameHeight = pNvxVideoDecoder->oBaseTwo.oPorts[s_nvx_port_output].nHeight;
                        pPortDef->format.video.nStride = pNvxVideoDecoder->oBaseTwo.oPorts[s_nvx_port_output].nWidth;
                    }
                }
                else
                {
                    if (pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output].bHasSize)
                    {
                        pPortDef->format.video.nFrameWidth = pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output].nWidth;
                        pPortDef->format.video.nFrameHeight = pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output].nHeight;
                        pPortDef->format.video.nStride = pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output].nWidth;
                    }
                }

                pPortDef->format.video.nFrameWidth = (pPortDef->format.video.nFrameWidth + 1) & ~0x1;
                pPortDef->format.video.nFrameHeight = (pPortDef->format.video.nFrameHeight + 1) & ~0x1;

                switch (pPortDef->format.video.eColorFormat) {
                    case OMX_COLOR_FormatYUV420Planar:
                    case OMX_COLOR_FormatYUV420SemiPlanar:
                            pPortDef->nBufferSize = (pPortDef->format.video.nFrameWidth*pPortDef->format.video.nFrameHeight*3)>>1;
                    break;

                    case OMX_COLOR_FormatYUV422Planar:
                    case OMX_COLOR_FormatYUV422SemiPlanar:
                            pPortDef->nBufferSize = (pPortDef->format.video.nFrameWidth*pPortDef->format.video.nFrameHeight*2);
                    break;

#if USE_ANDROID_NATIVE_WINDOW
                    case NVGR_PIXEL_FORMAT_YUV420:
                    case HAL_PIXEL_FORMAT_YV12:
                            pPortDef->nBufferSize = (pPortDef->format.video.nFrameWidth*pPortDef->format.video.nFrameHeight*3)>>1;
                    break;

                    case NVGR_PIXEL_FORMAT_YUV422:
                    case NVGR_PIXEL_FORMAT_YUV422R:
                            pPortDef->nBufferSize = (pPortDef->format.video.nFrameWidth*pPortDef->format.video.nFrameHeight*2);
                    break;
#endif //USE_ANDROID_NATIVE_WINDOW

                    default:
                         NvOsDebugPrintf("Err:Should not be here");
                    break;
                }
            }
            else
            {
                return eError;
            }
        }
        break;
#if USE_ANDROID_NATIVE_WINDOW
    case NVX_IndexParamGetAndroidNativeBufferUsage:
    {
        NvBool SWAccess;

        SWAccess = NvMMVideoDec_OutSurfNoSWAccess;
        if(pNvxVideoDecoder->oBase.hBlock)
        {
            pNvxVideoDecoder->oBase.hBlock->GetAttribute(
                pNvxVideoDecoder->oBase.hBlock, NvMMVideoDecAttribute_SurfaceSWAccess,
                sizeof(NvBool), &SWAccess);
        }
        NV_DEBUG_PRINTF(("%s hBlock:%x. Output Surface SW Access :%d ",__FUNCTION__, pNvxVideoDecoder->oBase.hBlock, SWAccess));
        return HandleGetANBUsage(pComponent,
                                 pComponentParameterStructure,
                                 (OMX_U32)SWAccess);
    }
#endif
    default:
        return NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
    };

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxVideoDecoderSetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentParameterStructure)
{
    SNvxVideoDecoderData *pNvxVideoDecoder;
    pNvxVideoDecoder = (SNvxVideoDecoderData *)pComponent->pComponentData;

    NV_ASSERT(pNvxVideoDecoder);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoDecoderSetParameter\n"));

    switch(nIndex)
    {
    case NVX_IndexParamLowMemMode:
        pNvxVideoDecoder->bLowMem = ((NVX_PARAM_LOWMEMMODE *)pComponentParameterStructure)->bLowMemMode;
        break;

    case OMX_IndexParamVideoMpeg4:
        if (((OMX_VIDEO_PARAM_MPEG4TYPE *)pComponentParameterStructure)->nPortIndex != (NvU32)s_nvx_port_input || pNvxVideoDecoder->oType != TYPE_MP4)
            return OMX_ErrorBadPortIndex;

        NvOsMemcpy(pComponent->pPorts[s_nvx_port_input].pPortPrivate, pComponentParameterStructure,
               sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
        break;
    case OMX_IndexParamVideoMpeg2:
        if (((OMX_VIDEO_PARAM_MPEG2TYPE *)pComponentParameterStructure)->nPortIndex != (NvU32)s_nvx_port_input || pNvxVideoDecoder->oType != TYPE_MPEG2)
            return OMX_ErrorBadPortIndex;

        NvOsMemcpy(pComponent->pPorts[s_nvx_port_input].pPortPrivate, pComponentParameterStructure,
               sizeof(OMX_VIDEO_PARAM_MPEG2TYPE));
        break;

    case OMX_IndexParamVideoAvc:
        if (((OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure)->nPortIndex != (NvU32)s_nvx_port_input || pNvxVideoDecoder->oType != TYPE_H264)
            return OMX_ErrorBadPortIndex;

        NvOsMemcpy(pComponent->pPorts[s_nvx_port_input].pPortPrivate, pComponentParameterStructure,
               sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        break;
    case OMX_IndexParamVideoWmv:
        if (((OMX_VIDEO_PARAM_WMVTYPE *)pComponentParameterStructure)->nPortIndex != (NvU32)s_nvx_port_input || pNvxVideoDecoder->oType != TYPE_VC1)
            return OMX_ErrorBadPortIndex;

        if (!pComponent->pPorts[s_nvx_port_input].pPortPrivate)
            return OMX_ErrorNotImplemented; // allocate in init

        NvOsMemcpy(pComponent->pPorts[s_nvx_port_input].pPortPrivate, pComponentParameterStructure,
               sizeof(OMX_VIDEO_PARAM_WMVTYPE));
        break;

    case NVX_IndexParamDeinterlaceMethod:
        {
            // TODO : enable this when MPEG 2 NvMM block is ready
            NVX_PARAM_DEINTERLACE *pDeinterlace =
                (NVX_PARAM_DEINTERLACE *)pComponentParameterStructure;
            pNvxVideoDecoder->AttribDeint.StructSize = sizeof(NvMMMpeg2VDec_DeinterlacingOptions);
            pNvxVideoDecoder->AttribDeint.DeintMethod = pDeinterlace->DeinterlaceMethod;
        }
        break;
    case NVX_IndexConfigUseNvBuffer:
        {
            NVX_PARAM_USENVBUFFER* pParam = (NVX_PARAM_USENVBUFFER*)pComponentParameterStructure;
            pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output].bEmbedNvMMBuffer = pParam->bUseNvBuffer;
        }
        break;
    case OMX_IndexParamVideoProfileLevelQuerySupported:
        return OMX_ErrorNotImplemented;
    case NVX_IndexParamLowResourceMode:
        {
            pNvxVideoDecoder->bUseLowResource = ((NVX_PARAM_LOWRESOURCEMODE *)
            pComponentParameterStructure)->bLowMemMode;
        }
        break;
#if USE_ANDROID_NATIVE_WINDOW
    case NVX_IndexParamEnableAndroidBuffers:
    {
        OMX_ERRORTYPE err = HandleEnableANB(pComponent, s_nvx_port_output,
                                            pComponentParameterStructure);

        pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output].bUsesANBs = (pComponent->pPorts[s_nvx_port_output].oPortDef.format.video.eColorFormat != OMX_COLOR_FormatYUV420Planar);
        return err;
    }
    case NVX_IndexParamUseAndroidNativeBuffer:
        return HandleUseANB(pComponent, pComponentParameterStructure);
    case NVX_IndexParamUseSyncFdFromNativeBuffer:
    {
        OMX_CONFIG_BOOLEANTYPE *param = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;
        if (param)
            pNvxVideoDecoder->oBase.bUsesSyncFDFromBuffer = param->bEnabled;
        break;
    }
#endif
    case NVX_IndexParamEmbedRmSurface:
        {
            OMX_CONFIG_BOOLEANTYPE *param = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;
            if (param)
                pNvxVideoDecoder->oBase.bEmbedRmSurface = param->bEnabled;
        }
        break;
    case NVX_IndexParamH264DisableDPB:
        {
            pNvxVideoDecoder->bH264DisableDPB = ((NVX_PARAM_H264DISABLE_DPB *)
            pComponentParameterStructure)->bDisableDPB;
        }
        break;
    case NVX_IndexParamVideoDecFullFrameData:
        {
            OMX_CONFIG_BOOLEANTYPE *FullFrame = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;
            pNvxVideoDecoder->bFullFrameData = FullFrame->bEnabled;
        }
        break;
    case NVX_IndexParamUseLowBuffer:
        {
            NVX_PARAM_USELOWBUFFER *pParam = (NVX_PARAM_USELOWBUFFER *)pComponentParameterStructure;
            if (s_nvx_port_input == pParam->nPortIndex)
            {
                OMX_U32 nBufferCount;
                if (pParam->bUseLowBuffer)
                    nBufferCount = MIN_INPUT_BUFFERS;
                else
                    nBufferCount = MAX_INPUT_BUFFERS;
                pComponent->pPorts[s_nvx_port_input].nReqBufferCount = nBufferCount;
                pComponent->pPorts[s_nvx_port_input].nMinBufferCount = nBufferCount;
                pComponent->pPorts[s_nvx_port_input].oPortDef.nBufferCountMin = nBufferCount;
                pComponent->pPorts[s_nvx_port_input].oPortDef.nBufferCountActual = nBufferCount;
            }
        }
        break;
    case NVX_IndexParamEmbedMMBuffer:
        {
            OMX_CONFIG_BOOLEANTYPE *param = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;
            if (param)
                pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output].bEmbedNvMMBuffer = param->bEnabled;
        }
        break;

    default:
        return NvxComponentBaseSetParameter(pComponent, nIndex, pComponentParameterStructure);
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxVideoDecoderGetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    NVX_CONFIG_TIMESTAMPTYPE *pTimestamp;
    SNvxVideoDecoderData *pNvxVideoDecoder;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoDecoderGetConfig\n"));

    pNvxVideoDecoder = (SNvxVideoDecoderData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
        case NVX_IndexConfigTimestampOverride:
        {
            pTimestamp = (NVX_CONFIG_TIMESTAMPTYPE *)pComponentConfigStructure;
            pTimestamp->bOverrideTimestamp = pNvxVideoDecoder->bOverrideTimestamp;
            break;
        }
        case NVX_IndexConfigGetNVMMBlock:
        {
            NVX_CONFIG_GETNVMMBLOCK *pBlockReq = (NVX_CONFIG_GETNVMMBLOCK *)pComponentConfigStructure;

            if (!pNvxVideoDecoder || !pNvxVideoDecoder->bInitialized)
                return OMX_ErrorNotReady;

            pBlockReq->pNvMMTransform = &pNvxVideoDecoder->oBase;
            pBlockReq->nNvMMPort = pBlockReq->nPortIndex; // nvmm and OMX port indexes match
            break;
        }
        case OMX_IndexConfigVideoMacroBlockErrorMap:
        {
            break;
        }
        case OMX_IndexConfigCommonOutputCrop:
        {
            OMX_CONFIG_RECTTYPE* pRect = (OMX_CONFIG_RECTTYPE*)pComponentConfigStructure;
            SNvxNvMMPort *pPort;
            if (pRect->nPortIndex != (NvU32)s_nvx_port_output)
                return OMX_ErrorBadPortIndex;

            pPort = &(pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output]);
            pRect->nLeft = pPort->outRect.nLeft;
            pRect->nTop = pPort->outRect.nTop;
            pRect->nWidth = pPort->outRect.nWidth;
            pRect->nHeight = pPort->outRect.nHeight;

            if (pRect->nWidth == 0 || pRect->nHeight == 0)
            {
                pRect->nWidth = pNvComp->pPorts[s_nvx_port_output].oPortDef.format.video.nFrameWidth;
                pRect->nHeight = pNvComp->pPorts[s_nvx_port_output].oPortDef.format.video.nFrameHeight;
            }

            pRect->nWidth = (pRect->nWidth + 1) & ~0x1;
            pRect->nHeight = (pRect->nHeight + 1) & ~0x1;

            break;
        }
        case OMX_IndexConfigCommonScale:
        {
            OMX_CONFIG_SCALEFACTORTYPE *pScale = (OMX_CONFIG_SCALEFACTORTYPE*)pComponentConfigStructure;
            SNvxNvMMPort *pPort;
            if (pScale->nPortIndex != (NvU32)s_nvx_port_output)
                return OMX_ErrorBadPortIndex;

            pPort = &(pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output]);
            pScale->xWidth = pPort->xScaleWidth;
            pScale->xHeight = pPort->xScaleHeight;

            if (pScale->xWidth == 0 || pScale->xHeight == 0)
            {
                pScale->xWidth = pScale->xHeight = NV_SFX_ONE;
            }
            break;
        }
        break;

        case NVX_IndexConfigVideoStereoInfo:
        {
            OMX_U32 stereoFPType = 0, stereoContentType = 0, isStereoEnabled = 0;

            NVX_CONFIG_VIDEO_STEREOINFO* pStereoInfo =
                (NVX_CONFIG_VIDEO_STEREOINFO *)pComponentConfigStructure;

            if (pNvxVideoDecoder->bInitialized)
            {
                NvMMVideoDecH264StereoInfo stereoInfo;
                NvError err = NvSuccess;

                NV_DEBUG_PRINTF(("Check Stereo Status by querying the NvMM Decoder module\n"));

                err = pNvxVideoDecoder->oBase.hBlock->GetAttribute(pNvxVideoDecoder->oBase.hBlock,
                                                           NvMMVideoDecAttribute_H264StereoInfo,
                                                           sizeof(NvMMVideoDecH264StereoInfo),
                                                           &stereoInfo);
                if (err != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("In NvxVideoDecoderGetConfig(), GetAttribute() FAILED \n"));
                    return OMX_ErrorBadParameter;
                }

                stereoFPType =
                    (stereoInfo.StereoFlags & NvMMBufferFlag_Stereo_SEI_FPType_Mask) >>
                                         NvMMBufferFlag_Stereo_SEI_FPType_StartPos;
                stereoContentType =
                    (stereoInfo.StereoFlags & NvMMBufferFlag_Stereo_SEI_ContentType_Mask) >>
                                         NvMMBufferFlag_Stereo_SEI_ContentType_StartPos;
                if (stereoInfo.StereoFlags & NvMMBufferFlag_StereoEnable)
                {
                    NV_DEBUG_PRINTF(("Stereo is enabled, FPType = %d, Content type = %d\n",
                                       stereoFPType, stereoContentType));
                    isStereoEnabled = 1;
                }

                // Update the structure that we send back to the Application
                pStereoInfo->stereoEnable = isStereoEnabled;
                pStereoInfo->fpType = stereoFPType;
                pStereoInfo->contentType = stereoContentType;
            }

        }
        break;

        default:
            return NvxComponentBaseGetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxVideoDecoderSetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentConfigStructure)
{
    NVX_CONFIG_TIMESTAMPTYPE *pTimestamp;
    SNvxVideoDecoderData *pNvxVideoDecoder;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoDecoderSetConfig\n"));

    pNvxVideoDecoder = (SNvxVideoDecoderData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
        case OMX_IndexConfigVideoNalSize:
        {
            OMX_VIDEO_CONFIG_NALSIZE *pNalSize = (OMX_VIDEO_CONFIG_NALSIZE *)pComponentConfigStructure;
            pNvxVideoDecoder->oBase.nNalStreamFlag = 1;
            pNvxVideoDecoder->oBase.nNalSize = pNalSize->nNaluBytes;
            break;
        }
        case NVX_IndexConfigTimestampOverride:
        {
            pTimestamp = (NVX_CONFIG_TIMESTAMPTYPE *)pComponentConfigStructure;
            pNvxVideoDecoder->bOverrideTimestamp = pTimestamp->bOverrideTimestamp;
            break;
        }
        case NVX_IndexConfigUlpMode:
        {
            return OMX_ErrorBadParameter;
        }
        case NVX_IndexConfigProfile:
        {
            NVX_CONFIG_PROFILE *pProf = (NVX_CONFIG_PROFILE *)pComponentConfigStructure;
            NvxNvMMTransformSetProfile(&pNvxVideoDecoder->oBase, pProf);
            break;
        }
        case OMX_IndexConfigVideoMBErrorReporting:
        {
            OMX_CONFIG_MBERRORREPORTINGTYPE* pErrorReporting = (OMX_CONFIG_MBERRORREPORTINGTYPE *)pComponentConfigStructure;
            pNvxVideoDecoder->bErrorReportingEnabled = pErrorReporting->bEnabled;
        }
        break;
        case NVX_IndexConfigCheckResources:
        {
            NvU32 Profile = 0;
            NvU32 Capability;
            OMX_VIDEO_PARAM_PROFILELEVELTYPE * ProfileHeader = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentConfigStructure;

            //Set Profile on Decoder
            Capability = 1;
            Profile = (NvU32)ProfileHeader->eProfile;
            pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock, NvMMVideoDecAttribute_AllocateResource,
                                NvMMSetAttrFlag_Notification, sizeof(NvU32), &Profile);
            //Wait for the file attribute set acknowledgement
            NvOsSemaphoreWait (pNvxVideoDecoder->oBase.SetAttrDoneSema);

            //Check Profile Resource Available
            pNvxVideoDecoder->oBase.hBlock->GetAttribute(pNvxVideoDecoder->oBase.hBlock, NvMMVideoDecAttribute_ResourceAllocStatus,
                        sizeof(NvU32), &Capability);

            //Return
            if(Capability)
            {
                return OMX_ErrorNone;
            }
            else
            {
                return OMX_ErrorInsufficientResources;
            }
        }
        break;

    case NVX_IndexConfigVideoStereoInfo:
        {
            OMX_U32 stereoFPType, stereoContentType, isStereoEnabled;

            NVX_CONFIG_VIDEO_STEREOINFO* pStereoInfo =
                (NVX_CONFIG_VIDEO_STEREOINFO *)pComponentConfigStructure;

            // Read the stereo info that the App asks us to set
            isStereoEnabled = pStereoInfo->stereoEnable;
            stereoFPType = pStereoInfo->fpType;
            stereoContentType = pStereoInfo->contentType;

            NV_DEBUG_PRINTF(("DecoderSetConfig(): StereoEnabled, FPType, ContentType = %x, %x, %x\n",
                               isStereoEnabled, stereoFPType, stereoContentType));

            if (pNvxVideoDecoder->bInitialized)
            {
                NvMMVideoDecH264StereoInfo stereoInfo;
                OMX_U32 stereoflags = 0;
                NvError err = NvSuccess;
                NvOsMemset(&stereoInfo, 0, sizeof(NvMMVideoDecH264StereoInfo));
                if (isStereoEnabled)
                {
                    stereoflags |= NvMMBufferFlag_StereoEnable;
                }
                stereoflags |=
                     ((stereoFPType << NvMMBufferFlag_Stereo_SEI_FPType_StartPos) &
                      NvMMBufferFlag_Stereo_SEI_FPType_Mask);
                stereoflags |=
                     ((stereoContentType << NvMMBufferFlag_Stereo_SEI_ContentType_StartPos) &
                      NvMMBufferFlag_Stereo_SEI_ContentType_Mask);
                stereoInfo.StructSize = sizeof(NvMMVideoDecH264StereoInfo);
                stereoInfo.StereoFlags = stereoflags;

                err = pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock,
                                                           NvMMVideoDecAttribute_H264StereoInfo,
                                                           0, stereoInfo.StructSize, &stereoInfo);
                if (err != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("In NvxVideoDecoderSetConfig(), SetAttribute() FAILED.\n"));
                    return OMX_ErrorBadParameter;
                }
            }

        }
        break;

        default:
            return NvxComponentBaseSetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxVideoDecoderWorkerFunction(OMX_IN NvxComponent *pComponent, OMX_IN OMX_BOOL bAllPortsReady, OMX_OUT OMX_BOOL *pbMoreWork, OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    SNvxVideoDecoderData *pNvxVideoDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];
    NvxPort *pPortOut = &pComponent->pPorts[s_nvx_port_output];
    OMX_BOOL bWouldFail = OMX_TRUE;

    NVXTRACE((NVXT_WORKER, pComponent->eObjectType, "+NvxVideoDecoderWorkerFunction\n"));

    pNvxVideoDecoder = (SNvxVideoDecoderData *)pComponent->pComponentData;

    *pbMoreWork = bAllPortsReady;
    if (!bAllPortsReady)
        return OMX_ErrorNone;

    if (pNvxVideoDecoder->bFirstFrame)
    {
        /* Just to decode header parameters.
         * In this case Output Port can be in disabled state
         */

        if ((!pPortIn->pCurrentBufferHdr) ||
            ((pPortOut->oPortDef.bEnabled) && (!pPortOut->pCurrentBufferHdr)))
        {
            return OMX_ErrorNone;
        }

        if (pNvxVideoDecoder->oType == TYPE_H264 && pPortIn->hTunnelComponent)
        {
            NVX_VIDEO_CONFIG_H264NALLEN oNalLen;

            if (OMX_ErrorNone == OMX_GetConfig(pPortIn->hTunnelComponent,
                                               NVX_IndexConfigH264NALLen,
                                               &oNalLen))
            {
                pNvxVideoDecoder->oBase.nNalStreamFlag = 1;
                pNvxVideoDecoder->oBase.nNalSize = oNalLen.nNalLen;
            }
            else
            {
                OMX_VIDEO_CONFIG_NALSIZE oOMXNalSize;
                oOMXNalSize.nSize = sizeof(OMX_VIDEO_CONFIG_NALSIZE);
                oOMXNalSize.nVersion = pComponent->oSpecVersion;
                oOMXNalSize.nPortIndex = pPortIn->nTunnelPort;

                if (OMX_ErrorNone == OMX_GetConfig(pPortIn->hTunnelComponent,
                                               OMX_IndexConfigVideoNalSize,
                                               &oOMXNalSize))
                {
                    pNvxVideoDecoder->oBase.nNalStreamFlag = 1;
                    pNvxVideoDecoder->oBase.nNalSize = oOMXNalSize.nNaluBytes;
                }
            }
        }
        pNvxVideoDecoder->bFirstFrame = OMX_FALSE;
    }
    else if (!pPortIn->pCurrentBufferHdr || !pPortOut->pCurrentBufferHdr)
    {
        return OMX_ErrorNone;
    }

    eError = NvxNvMMTransformWorkerBase(&pNvxVideoDecoder->oBase,
                                        s_nvx_port_input, &bWouldFail);

    if (eError == OMX_ErrorMbErrorsInFrame)
    {
        if (pComponent->pCallbacks && pComponent->pCallbacks->EventHandler)
            pComponent->pCallbacks->EventHandler(pComponent->hBaseComponent,
                                             pComponent->pCallbackAppData,
                                             OMX_EventError,
                                             OMX_ErrorMbErrorsInFrame,
                                             0,
                                             0);

        eError = OMX_ErrorNone;
    }

    if (!pPortIn->pCurrentBufferHdr)
        NvxPortGetNextHdr(pPortIn);

    if (!pPortOut->pCurrentBufferHdr)
        NvxPortGetNextHdr(pPortOut);

    *pbMoreWork = (pPortIn->pCurrentBufferHdr && pPortOut->pCurrentBufferHdr &&
                   !bWouldFail);
    return eError;
}

static OMX_ERRORTYPE NvxVideoDecoderAcquireResources(OMX_IN NvxComponent *pComponent)
{
    SNvxVideoDecoderData *pNvxVideoDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    // By default disabling the SkipWorker thread for optimization of untunneled ports
    OMX_BOOL bCanSkipWorker = OMX_FALSE;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoDecoderAcquireResources\n"));

    /* Get MP4 Decoder instance */
    pNvxVideoDecoder = (SNvxVideoDecoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxVideoDecoder);

    eError = NvxComponentBaseAcquireResources(pComponent);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pComponent->eObjectType, "ERROR:NvxVideoDecoderAcquireResources:"
           ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    if (!NvOsStrcmp(pComponent->pComponentName, "OMX.Nvidia.mpeg2v.decode"))
    {
        eError = NvxMpeg2Open(pNvxVideoDecoder, pComponent);
        if (eError == NvxError_VideoDecNormalConfig)
            goto SkipTransformOpen;

        return eError;
    }

    if (!pNvxVideoDecoder->oBase.hBlock)
    {
        eError = NvxNvMMTransformOpen(&pNvxVideoDecoder->oBase,
                                      pNvxVideoDecoder->oBlockType,
                                      pNvxVideoDecoder->sBlockName,
                                      s_nvx_num_ports,
                                      pComponent);
        if (eError != OMX_ErrorNone)
        {
            NvOsDebugPrintf("%s : NvxNvMMTransformOpen failed \n", __func__);
            return eError;
        }
    }

SkipTransformOpen:
    eError = NvxNvMMTransformSetupInput(&pNvxVideoDecoder->oBase, s_nvx_port_input, &pComponent->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERS, MAX_INPUT_BUFFERSIZE, bCanSkipWorker);
    if (eError != OMX_ErrorNone)
        return eError;

    eError = NvxNvMMTransformSetupOutput(&pNvxVideoDecoder->oBase, s_nvx_port_output, &pComponent->pPorts[s_nvx_port_output], s_nvx_port_input, MAX_OUTPUT_BUFFERS, MIN_OUTPUT_BUFFERSIZE);
    if (eError != OMX_ErrorNone)
        return eError;

    {
        OMX_BOOL bNvidiaTunneled =
        (pComponent->pPorts[s_nvx_port_output].bNvidiaTunneling) &&
        (pComponent->pPorts[s_nvx_port_output].eNvidiaTunnelTransaction == ENVX_TRANSACTION_GFSURFACES ||
         pComponent->pPorts[s_nvx_port_output].eNvidiaTunnelTransaction == ENVX_TRANSACTION_NVMM_TUNNEL);

        NvxNvMMTransformSetStreamUsesSurfaces(&pNvxVideoDecoder->oBase, s_nvx_port_output, bNvidiaTunneled);
    }

    if(pNvxVideoDecoder->bUseLowResource)
    {
        pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock,
                                      NvMMVideoDecAttribute_UseLowResource,
                                      0, sizeof(OMX_BOOL),
                                      &pNvxVideoDecoder->bUseLowResource);
    }
    if ((pNvxVideoDecoder->bH264DisableDPB) && (!NvOsStrcmp(pComponent->pComponentName, "OMX.Nvidia.h264.decode")))
    {
        pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock,
                                      NvMMVideoDecAttribute_H264_DisableDPB,
                                      0, sizeof(OMX_BOOL),
                                      &pNvxVideoDecoder->bH264DisableDPB);
    }
    if (!NvOsStrcmp(pComponent->pComponentName, "OMX.Nvidia.h264.decode"))
    {
        pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock,
                                      NvMMVideoDecAttribute_NumInputBuffers,
                                      0, sizeof(NvU32),
                                      &pComponent->pPorts[s_nvx_port_input].oPortDef.nBufferCountActual);
    }
    if (pNvxVideoDecoder->bFullFrameData)
    {
        pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock,
            NvMMVideoDecAttribute_CompleteFrameInputBuffer,
            0, sizeof(OMX_BOOL), &pNvxVideoDecoder->bFullFrameData);
    }
    pNvxVideoDecoder->bFirstFrame = OMX_TRUE;
    pNvxVideoDecoder->bInitialized = OMX_TRUE;
    return eError;
}

static OMX_ERRORTYPE NvxVideoDecoderReleaseResources(OMX_IN NvxComponent *pComponent)
{
    SNvxVideoDecoderData *pNvxVideoDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoDecoderReleaseResources\n"));

    /* Get MP4 Decoder instance */
    pNvxVideoDecoder = (SNvxVideoDecoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxVideoDecoder);

    if(pNvxVideoDecoder->oBaseTwo.hBlock)
    {
        eError = NvxNvMMTransformClose(&pNvxVideoDecoder->oBaseTwo);
        if (eError != OMX_ErrorNone)
            return eError;
    }

    eError = NvxNvMMTransformClose(&pNvxVideoDecoder->oBase);

    if (eError != OMX_ErrorNone)
        return eError;

    pNvxVideoDecoder->bInitialized = OMX_FALSE;

    NvxCheckError(eError, NvxComponentBaseReleaseResources(pComponent));
    return eError;
}

static void SetSkipFrames(OMX_BOOL bSkip,
                          SNvxVideoDecoderData *pNvxVideoDecoder,
                          SNvxNvMMTransformData *pBase)
{
    NvU32 val = (NvU32)bSkip;

    if (bSkip == pNvxVideoDecoder->bSkipFrames)
        return;

    pNvxVideoDecoder->bSkipFrames = bSkip;
    pBase->hBlock->SetAttribute(pBase->hBlock, NvMMVideoDecAttribute_SkipFrames,
                                0, sizeof(NvU32), &val);
}

static OMX_ERRORTYPE NvxVideoDecoderFlush(OMX_IN NvxComponent *pComponent, OMX_U32 nPort)
{
    SNvxVideoDecoderData *pNvxVideoDecoder = 0;
    OMX_ERRORTYPE status = OMX_ErrorNone;

    NV_ASSERT(pComponent && pComponent->pComponentData);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoDecoderFlush\n"));
    pNvxVideoDecoder = (SNvxVideoDecoderData *)pComponent->pComponentData;

    SetSkipFrames(OMX_FALSE, pNvxVideoDecoder, &pNvxVideoDecoder->oBase);
    status =  NvxNvMMTransformFlush(&pNvxVideoDecoder->oBase, nPort);

    if (pNvxVideoDecoder->oBaseTwo.hBlock)
    {
        status = NvxNvMMTransformFlush(&pNvxVideoDecoder->oBaseTwo, nPort);
    }
    return status;
}

static OMX_ERRORTYPE NvxVideoDecoderFillThisBuffer(NvxComponent *pNvComp, OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    OMX_ERRORTYPE status;
    SNvxVideoDecoderData *pNvxVideoDecoder = 0;
    SNvxNvMMTransformData *pBase;
    NvxBufferPlatformPrivate *pPrivateData =
                (NvxBufferPlatformPrivate *)pBufferHdr->pPlatformPrivate;


    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoDecoderFillThisBuffer\n"));

    pNvxVideoDecoder = (SNvxVideoDecoderData *)pNvComp->pComponentData;
    if (!pNvxVideoDecoder->bInitialized)
        return OMX_ErrorNone;

    if (pNvxVideoDecoder->oBaseTwo.hBlock)
        pBase = &pNvxVideoDecoder->oBaseTwo;
    else
        pBase = &pNvxVideoDecoder->oBase;

    if (pBufferHdr->nFlags & NVX_BUFFERFLAG_DECODERSLOW)
        SetSkipFrames(OMX_TRUE, pNvxVideoDecoder, pBase);
    if (pBufferHdr->nFlags & NVX_BUFFERFLAG_DECODERNOTSLOW)
        SetSkipFrames(OMX_FALSE, pNvxVideoDecoder, pBase);

    if (pPrivateData->pData)
    {
        pBase = (SNvxNvMMTransformData *)pPrivateData->pData;
        pPrivateData->pData = 0;
    }

    status =  NvxNvMMTransformFillThisBuffer(pBase, pBufferHdr, s_nvx_port_output);
    if (status != OMX_ErrorNone)
        NV_DEBUG_PRINTF(("%s failed NvxNvMMTransformFillThisBuffer %x", __FUNCTION__, status));

    return status;
}

static OMX_ERRORTYPE NvxVideoDecoderFreeBuffer(NvxComponent *pNvComp, OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    SNvxVideoDecoderData *pNvxVideoDecoder = 0;
    SNvxNvMMTransformData *pBase;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoDecoderFillThisBuffer\n"));

    pNvxVideoDecoder = (SNvxVideoDecoderData *)pNvComp->pComponentData;
    if (!pNvxVideoDecoder->bInitialized)
        return OMX_ErrorNone;

    if (pNvxVideoDecoder->oType == TYPE_MPEG2)
        pBase = &pNvxVideoDecoder->oBaseTwo;
    else
        pBase = &pNvxVideoDecoder->oBase;

    return NvxNvMMTransformFreeBuffer(pBase, pBufferHdr, s_nvx_port_output);
}


static OMX_ERRORTYPE NvxVideoDecoderPreChangeState(NvxComponent *pNvComp,
                                                   OMX_STATETYPE oNewState)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    SNvxVideoDecoderData *pNvxVideoDecoder = 0;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoDecoderPreChangeState\n"));

    pNvxVideoDecoder = (SNvxVideoDecoderData *)pNvComp->pComponentData;
    if (!pNvxVideoDecoder->bInitialized)
        return OMX_ErrorNone;
    if(pNvxVideoDecoder->oBaseTwo.hBlock)
    {
        err= NvxNvMMTransformPreChangeState(&pNvxVideoDecoder->oBaseTwo, oNewState,
            pNvComp->eState);
        if (OMX_ErrorNone != err)
            return err;
    }
    return NvxNvMMTransformPreChangeState(&pNvxVideoDecoder->oBase, oNewState,
                                          pNvComp->eState);
}

static OMX_ERRORTYPE NvxVideoDecoderEmptyThisBuffer(NvxComponent *pNvComp,
                                             OMX_BUFFERHEADERTYPE* pBufferHdr,
                                             OMX_BOOL *bHandled)
{
    SNvxVideoDecoderData *pNvxVideoDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoDecoderEmptyThisBuffer\n"));

    pNvxVideoDecoder = (SNvxVideoDecoderData *)pNvComp->pComponentData;
    if (!pNvxVideoDecoder->bInitialized)
        return OMX_ErrorNone;

    if (NvxNvMMTransformIsInputSkippingWorker(&pNvxVideoDecoder->oBase,
                                              s_nvx_port_input))
    {
        eError = NvxPortEmptyThisBuffer(&pNvComp->pPorts[pBufferHdr->nInputPortIndex], pBufferHdr);
        *bHandled = (eError == OMX_ErrorNone);
    }
    else
        *bHandled = OMX_FALSE;

    return eError;
}

static OMX_ERRORTYPE NvxVideoDecoderPortEvent(NvxComponent *pNvComp,
                                              OMX_U32 nPort, OMX_U32 uEventType)
{
    SNvxVideoDecoderData *pNvxVideoDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    SNvxNvMMTransformData *pBase;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoDecoderPortEvent\n"));

    pNvxVideoDecoder = (SNvxVideoDecoderData *)pNvComp->pComponentData;
    if (!pNvxVideoDecoder->bInitialized)
        return eError;

    if (pNvxVideoDecoder->oBaseTwo.hBlock)
        pBase = &pNvxVideoDecoder->oBaseTwo;
    else
        pBase = &pNvxVideoDecoder->oBase;

    NvxNvMMTransformPortEventHandler(pBase, nPort, uEventType);

    return eError;
}

/*****************************************************************************/

static OMX_ERRORTYPE NvxCommonVideoDecoderInit(OMX_HANDLETYPE hComponent, NvMMBlockType oBlockType, const char *sBlockName)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pNvComp;
    SNvxVideoDecoderData *data = NULL;

    eError = NvxComponentCreate(hComponent, s_nvx_num_ports, &pNvComp);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,NVXT_VIDEO_DECODER,"ERROR:NvxCommonVideoDecoderInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    // FIXME: resources?

    pNvComp->eObjectType = NVXT_VIDEO_DECODER;

    pNvComp->pComponentData = NvOsAlloc(sizeof(SNvxVideoDecoderData));
    if (!pNvComp->pComponentData)
        return OMX_ErrorInsufficientResources;

    data = (SNvxVideoDecoderData *)pNvComp->pComponentData;
    NvOsMemset(data, 0, sizeof(SNvxVideoDecoderData));

    switch (oBlockType)
    {
        case NvMMBlockType_DecMPEG4: data->oType = TYPE_MP4; break;
        case NvMMBlockType_DecH264: data->oType = TYPE_H264; break;
        case NvMMBlockType_DecVC1: data->oType = TYPE_VC1; break;
        case NvMMBlockType_DecMPEG2: data->oType = TYPE_MPEG2; break;
        case NvMMBlockType_DecSuperJPEG: data->oType = TYPE_MJPEG; break;
        case NvMMBlockType_DecVP8: data->oType = TYPE_VP8; break;
        default: return OMX_ErrorBadParameter;
    }

    data->bLowMem = OMX_TRUE;
    data->oBlockType = oBlockType;
    data->sBlockName = sBlockName;
    data->oBase.enableUlpMode = OMX_TRUE;
    data->oBaseTwo.enableUlpMode = OMX_TRUE;
    data->bUseLowResource = OMX_FALSE;
    data->bH264DisableDPB = OMX_FALSE;
    data->bFullFrameData = NV_FALSE;

    pNvComp->DeInit             = NvxVideoDecoderDeInit;
    pNvComp->GetParameter       = NvxVideoDecoderGetParameter;
    pNvComp->SetParameter       = NvxVideoDecoderSetParameter;
    pNvComp->GetConfig          = NvxVideoDecoderGetConfig;
    pNvComp->SetConfig          = NvxVideoDecoderSetConfig;
    pNvComp->WorkerFunction     = NvxVideoDecoderWorkerFunction;
    pNvComp->AcquireResources   = NvxVideoDecoderAcquireResources;
    pNvComp->ReleaseResources   = NvxVideoDecoderReleaseResources;
    pNvComp->FillThisBufferCB   = NvxVideoDecoderFillThisBuffer;
    pNvComp->PreChangeState     = NvxVideoDecoderPreChangeState;
    pNvComp->EmptyThisBuffer    = NvxVideoDecoderEmptyThisBuffer;
    pNvComp->Flush              = NvxVideoDecoderFlush;
    pNvComp->PortEventHandler   = NvxVideoDecoderPortEvent;
    pNvComp->FreeBufferCB       = NvxVideoDecoderFreeBuffer;

    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_output], OMX_DirOutput,
                     4, MIN_OUTPUT_BUFFERSIZE,
                     OMX_VIDEO_CodingUnused);
    pNvComp->pPorts[s_nvx_port_output].oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;

    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_output], MAX_OUTPUT_BUFFERSIZE);
    data->nTunneledBufferCount = MAX_OUTPUT_BUFFERS + 5;
    pNvComp->pPorts[s_nvx_port_output].eNvidiaTunnelTransaction =
            ENVX_TRANSACTION_NVMM_TUNNEL;

    return OMX_ErrorNone;
}

/***************************************************************************/

OMX_ERRORTYPE NvxMp4DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    OMX_VIDEO_PARAM_MPEG4TYPE *pVideoParams;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxMp4DecoderInit\n"));

    eError = NvxCommonVideoDecoderInit(hComponent, NvMMBlockType_DecMPEG4, "BlockMP4Dec");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxMPEG4DecoderInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ALLOC_STRUCT(pNvComp, pVideoParams, OMX_VIDEO_PARAM_MPEG4TYPE);
    if (!pVideoParams)
        return OMX_ErrorInsufficientResources;

    /* Register function pointers */
    pNvComp->pComponentName = "OMX.Nvidia.mp4.decode";
    pNvComp->sComponentRoles[0] = "video_decoder.mpeg4";    // check the correct type
    pNvComp->nComponentRoles = 1;

    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_input], OMX_DirInput,
                     MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE, OMX_VIDEO_CodingMPEG4);
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERSIZE);
    pNvComp->pPorts[s_nvx_port_input].eNvidiaTunnelTransaction = ENVX_TRANSACTION_NVMM_TUNNEL;

    pVideoParams->eProfile = OMX_VIDEO_MPEG4ProfileSimple;
    pVideoParams->eLevel = OMX_VIDEO_MPEG4Level1;
    pVideoParams->bSVH = OMX_FALSE;
    pVideoParams->nIDCVLCThreshold = 0;
    pVideoParams->bACPred = OMX_TRUE;
    pVideoParams->bReversibleVLC = OMX_FALSE;
    pVideoParams->nHeaderExtension = 0;
    pVideoParams->bGov = OMX_FALSE;
    pVideoParams->nAllowedPictureTypes = 2; // check
    pVideoParams->nBFrames = 16; // check
    pVideoParams->nHeaderExtension = 0;
    pVideoParams->nIDCVLCThreshold = 0;
    pVideoParams->nMaxPacketSize = 2048;
    pVideoParams->nPFrames = 16;
    pVideoParams->nSliceHeaderSpacing = 0;
    pVideoParams->nTimeIncRes = 50;

    pNvComp->pPorts[s_nvx_port_input].pPortPrivate = pVideoParams;

    return eError;
}

OMX_ERRORTYPE NvxMp4ExtDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    OMX_VIDEO_PARAM_MPEG4TYPE *pVideoParams;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxMp4ExtDecoderInit\n"));

    eError = NvxCommonVideoDecoderInit(hComponent, NvMMBlockType_DecMPEG4, "BlockMP4Dec");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxMPEG4ExtDecoderInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ALLOC_STRUCT(pNvComp, pVideoParams, OMX_VIDEO_PARAM_MPEG4TYPE);
    if (!pVideoParams)
        return OMX_ErrorInsufficientResources;

    /* Register function pointers */
    pNvComp->pComponentName = "OMX.Nvidia.mp4ext.decode";
    pNvComp->sComponentRoles[0] = "video_decoder.mpeg4";    // check the correct type
    pNvComp->nComponentRoles = 1;

    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_input], OMX_DirInput,
                     MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE, OMX_VIDEO_CodingMPEG4);
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERSIZE);
    pNvComp->pPorts[s_nvx_port_input].eNvidiaTunnelTransaction = ENVX_TRANSACTION_NVMM_TUNNEL;

    pVideoParams->eProfile = OMX_VIDEO_MPEG4ProfileSimple;
    pVideoParams->eLevel = OMX_VIDEO_MPEG4Level1;
    pVideoParams->bSVH = OMX_FALSE;
    pVideoParams->nIDCVLCThreshold = 0;
    pVideoParams->bACPred = OMX_TRUE;
    pVideoParams->bReversibleVLC = OMX_FALSE;
    pVideoParams->nHeaderExtension = 0;
    pVideoParams->bGov = OMX_FALSE;
    pVideoParams->nAllowedPictureTypes = 2; // check
    pVideoParams->nBFrames = 16; // check
    pVideoParams->nHeaderExtension = 0;
    pVideoParams->nIDCVLCThreshold = 0;
    pVideoParams->nMaxPacketSize = 2048;
    pVideoParams->nPFrames = 16;
    pVideoParams->nSliceHeaderSpacing = 0;
    pVideoParams->nTimeIncRes = 50;

    pNvComp->pPorts[s_nvx_port_input].pPortPrivate = pVideoParams;

    return eError;
}

OMX_ERRORTYPE NvxH263DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    OMX_VIDEO_PARAM_H263TYPE *pVideoParams;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxH263DecoderInit\n"));

    eError = NvxCommonVideoDecoderInit(hComponent, NvMMBlockType_DecMPEG4, "BlockMP4Dec");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxMPEG4DecoderInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ALLOC_STRUCT(pNvComp, pVideoParams, OMX_VIDEO_PARAM_H263TYPE);
    if (!pVideoParams)
        return OMX_ErrorInsufficientResources;

    /* Register function pointers */
    pNvComp->pComponentName = "OMX.Nvidia.h263.decode";
    pNvComp->sComponentRoles[0] = "video_decoder.h263";
    pNvComp->nComponentRoles = 1;

    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_input], OMX_DirInput,
                     MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE,
                     OMX_VIDEO_CodingH263);
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_input],
                              MAX_INPUT_BUFFERSIZE);
    pNvComp->pPorts[s_nvx_port_input].eNvidiaTunnelTransaction = ENVX_TRANSACTION_NVMM_TUNNEL;

    pVideoParams->nPFrames = 0;
    pVideoParams->nBFrames = 0;
    pVideoParams->eProfile = OMX_VIDEO_H263ProfileBaseline;
    pVideoParams->eLevel = OMX_VIDEO_H263Level10;
    pVideoParams->bPLUSPTYPEAllowed = OMX_FALSE;
    pVideoParams->nAllowedPictureTypes = 0;
    pVideoParams->bForceRoundingTypeToZero = OMX_FALSE;
    pVideoParams->nPictureHeaderRepetition = 0;
    pVideoParams->nGOBHeaderInterval = 0;

    pNvComp->pPorts[s_nvx_port_input].pPortPrivate = pVideoParams;

    return eError;
}

OMX_ERRORTYPE NvxH264DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    OMX_VIDEO_PARAM_AVCTYPE *pAvcParams;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxH264DecoderInit\n"));

    eError = NvxCommonVideoDecoderInit(hComponent, NvMMBlockType_DecH264, "BlockH264Dec");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxH264DecoderInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ALLOC_STRUCT(pNvComp, pAvcParams, OMX_VIDEO_PARAM_AVCTYPE);
    if (!pAvcParams)
        return OMX_ErrorInsufficientResources;

    /* Register function pointers */
    pNvComp->pComponentName = "OMX.Nvidia.h264.decode";
    pNvComp->sComponentRoles[0] = "video_decoder.avc";    // check the correct type
    pNvComp->nComponentRoles    = 1;

    // TO DO: set the buffer size appropriately
    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_input], OMX_DirInput,
                     MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE, OMX_VIDEO_CodingAVC);
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERSIZE);
    pNvComp->pPorts[s_nvx_port_input].eNvidiaTunnelTransaction = ENVX_TRANSACTION_NVMM_TUNNEL;

    pAvcParams->eProfile = OMX_VIDEO_AVCProfileBaseline;
    pAvcParams->eLevel = OMX_VIDEO_AVCLevel1;
    pAvcParams->bUseHadamard = OMX_TRUE;
    pAvcParams->nRefFrames = 1;
    pAvcParams->bEnableFMO = OMX_FALSE;
    pAvcParams->bEnableASO = OMX_FALSE;
    pAvcParams->bWeightedPPrediction = OMX_FALSE;
    pAvcParams->bconstIpred = OMX_FALSE;

    pNvComp->pPorts[s_nvx_port_input].pPortPrivate = pAvcParams;

    return eError;
}

OMX_ERRORTYPE NvxH264ExtDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    OMX_VIDEO_PARAM_AVCTYPE *pAvcParams;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxH264DecoderInit\n"));

    eError = NvxCommonVideoDecoderInit(hComponent, NvMMBlockType_DecH264, "BlockH264Dec");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxH264ExtDecoderInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ALLOC_STRUCT(pNvComp, pAvcParams, OMX_VIDEO_PARAM_AVCTYPE);
    if (!pAvcParams)
        return OMX_ErrorInsufficientResources;

    /* Register function pointers */
    pNvComp->pComponentName = "OMX.Nvidia.h264ext.decode";
    pNvComp->sComponentRoles[0] = "video_decoder.avc";    // check the correct type
    pNvComp->nComponentRoles    = 1;

    // TO DO: set the buffer size appropriately
    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_input], OMX_DirInput,
                     MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE, OMX_VIDEO_CodingAVC);
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERSIZE);
    pNvComp->pPorts[s_nvx_port_input].eNvidiaTunnelTransaction = ENVX_TRANSACTION_NVMM_TUNNEL;

    pAvcParams->eProfile = OMX_VIDEO_AVCProfileBaseline;
    pAvcParams->eLevel = OMX_VIDEO_AVCLevel1;
    pAvcParams->bUseHadamard = OMX_TRUE;
    pAvcParams->nRefFrames = 1;
    pAvcParams->bEnableFMO = OMX_FALSE;
    pAvcParams->bEnableASO = OMX_FALSE;
    pAvcParams->bWeightedPPrediction = OMX_FALSE;
    pAvcParams->bconstIpred = OMX_FALSE;

    pNvComp->pPorts[s_nvx_port_input].pPortPrivate = pAvcParams;

    return eError;
}

OMX_ERRORTYPE NvxVc1DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    OMX_VIDEO_PARAM_WMVTYPE *pWmvParams;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxVc1DecoderInit\n"));

    eError = NvxCommonVideoDecoderInit(hComponent, NvMMBlockType_DecVC1, "BlockVc1Dec");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxVc1DecoderInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ALLOC_STRUCT(pNvComp, pWmvParams, OMX_VIDEO_PARAM_WMVTYPE);
    if (!pWmvParams)
        return OMX_ErrorInsufficientResources;

    pNvComp->pComponentName = "OMX.Nvidia.vc1.decode";
    pNvComp->sComponentRoles[0] = "video_decoder.vc1";
    pNvComp->sComponentRoles[1] = "video_decoder.wmv";
    pNvComp->nComponentRoles    = 2;

    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_input], OMX_DirInput,
                     MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE, OMX_VIDEO_CodingWMV);
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERSIZE);
    pNvComp->pPorts[s_nvx_port_input].eNvidiaTunnelTransaction = ENVX_TRANSACTION_NVMM_TUNNEL;

    pWmvParams->eFormat = OMX_VIDEO_WMVFormat9;
    pNvComp->pPorts[s_nvx_port_input].pPortPrivate = pWmvParams;
    return eError;
}

OMX_ERRORTYPE NvxVP8DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    OMX_VIDEO_PARAM_PORTFORMATTYPE *pVP8Params;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxVP8DecoderInit\n"));

    eError = NvxCommonVideoDecoderInit(hComponent, NvMMBlockType_DecVP8, "BlockVP8Dec");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxVP8DecoderInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ALLOC_STRUCT(pNvComp, pVP8Params, OMX_VIDEO_PARAM_PORTFORMATTYPE);
    if (!pVP8Params)
        return OMX_ErrorInsufficientResources;

    pNvComp->pComponentName = "OMX.Nvidia.vp8.decode";
    pNvComp->sComponentRoles[0] = "video_decoder.vp8";
    pNvComp->sComponentRoles[1] = "video_decoder.vpx";
    pNvComp->nComponentRoles    = 2;

    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_input], OMX_DirInput,
                     MAX_INPUT_BUFFERS, MAX_INPUT_BUFFERSIZE, NVX_VIDEO_CodingVP8);
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERSIZE);

    pVP8Params->nPortIndex = s_nvx_port_input;
    pNvComp->pPorts[s_nvx_port_input].pPortPrivate = pVP8Params;
    return eError;
}

OMX_ERRORTYPE NvxMpeg2DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    SNvxVideoDecoderData *data = NULL;
    OMX_VIDEO_PARAM_MPEG2TYPE  *pMpeg2Params;
    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxMpeg2DecoderInit\n"));

    eError = NvxCommonVideoDecoderInit(hComponent, NvMMBlockType_DecMPEG2, "BlockMpeg2Dec");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxAudioDecoderInit:"
            ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ALLOC_STRUCT(pNvComp, pMpeg2Params, OMX_VIDEO_PARAM_MPEG2TYPE);
    if (!pMpeg2Params)
        return OMX_ErrorInsufficientResources;
    pNvComp->pComponentName = "OMX.Nvidia.mpeg2v.decode";
    pNvComp->sComponentRoles[0] = "video_decoder.mpeg2";    // check the correct type
    pNvComp->nComponentRoles    = 1;

    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_input], OMX_DirInput,
        MAX_INPUT_BUFFERS, MAX_INPUT_BUFFERSIZE, OMX_VIDEO_CodingMPEG2);
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERSIZE);
    pNvComp->pPorts[s_nvx_port_input].eNvidiaTunnelTransaction = ENVX_TRANSACTION_NVMM_TUNNEL;

    //set default de-interlace mode in case of mpeg2 video decoder
    data = (SNvxVideoDecoderData *)pNvComp->pComponentData;
    data->AttribDeint.StructSize  = sizeof(NvMMMpeg2VDec_DeinterlacingOptions);
    data->AttribDeint.DeintMethod = DeintMethod_Advanced1AtFrameRate;

    pMpeg2Params->nPortIndex=s_nvx_port_input;
    pMpeg2Params-> nPFrames=3;  //check
    pMpeg2Params-> nBFrames=2;  //check
    pMpeg2Params-> eProfile=OMX_VIDEO_MPEG2ProfileMain;
    pMpeg2Params-> eLevel=OMX_VIDEO_MPEG2LevelML;
    pNvComp->pPorts[s_nvx_port_input].pPortPrivate = pMpeg2Params;
    return eError;
}

static OMX_ERRORTYPE NvxMpeg2Open(SNvxVideoDecoderData *pNvxVideoDecoder, NvxComponent *pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvU32 DualBlockConfig = 0;

    pNvxVideoDecoder->oBase.nForceLocale = 1;
    eError = NvxNvMMTransformOpen(&pNvxVideoDecoder->oBase, NvMMBlockType_DecMPEG2, "Mpeg2", s_nvx_num_ports, pComponent);
    if (eError != OMX_ErrorNone)
        return eError;

    pNvxVideoDecoder->oBase.hBlock->GetAttribute(pNvxVideoDecoder->oBase.hBlock,
                            NvMMVideoDecAttribute_MPEG2NeedsRecon,
                            sizeof(NvU32), &DualBlockConfig);

    if (DualBlockConfig)
    {
        // reset flags set in TransformOpen
        pNvxVideoDecoder->oBase.bVideoTransform = OMX_FALSE;
        pNvxVideoDecoder->oBase.bInSharedMemory = OMX_FALSE;
    }
    else
    {
        // opened block is sufficient
        return NvxError_VideoDecNormalConfig;
    }

    pNvxVideoDecoder->oBaseTwo.nForceLocale = 1;
    eError = NvxNvMMTransformOpen(&pNvxVideoDecoder->oBaseTwo, NvMMBlockType_DecMPEG2VideoRecon, "Mpeg2Recon", s_nvx_num_ports, pComponent);
    if (eError != OMX_ErrorNone)
        return eError;

    pNvxVideoDecoder->oBaseTwo.hBlock->SetAttribute(pNvxVideoDecoder->oBaseTwo.hBlock,
                            NvMMMpeg2VDecAttribute_DeinterlacingOptions,
                            0,
                            sizeof(NvMMMpeg2VDec_DeinterlacingOptions), &pNvxVideoDecoder->AttribDeint);

    eError = NvxNvMMTransformSetupInput(&pNvxVideoDecoder->oBase, s_nvx_port_input, &pComponent->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERS, MAX_INPUT_BUFFERSIZE, OMX_FALSE);
    if (eError != OMX_ErrorNone)
        return eError;

    eError = NvxNvMMTransformTunnelBlocks(&pNvxVideoDecoder->oBase, s_nvx_port_output,
        &pNvxVideoDecoder->oBaseTwo, s_nvx_port_input);
    if (eError != OMX_ErrorNone)
        return eError;

    pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output].nType = TF_TYPE_OUTPUT_TUNNELED;
    pNvxVideoDecoder->oBaseTwo.oPorts[s_nvx_port_input].nType = TF_TYPE_INPUT_TUNNELED;

    eError = NvxNvMMTransformSetupOutput(&pNvxVideoDecoder->oBaseTwo, s_nvx_port_output, &pComponent->pPorts[s_nvx_port_output], s_nvx_port_input, MAX_OUTPUT_BUFFERS, MIN_OUTPUT_BUFFERSIZE);
    if (eError != OMX_ErrorNone)
        return eError;

    {
        OMX_BOOL bNvidiaTunneled =
            (pComponent->pPorts[s_nvx_port_output].bNvidiaTunneling) &&
            (pComponent->pPorts[s_nvx_port_output].eNvidiaTunnelTransaction == ENVX_TRANSACTION_GFSURFACES ||
             pComponent->pPorts[s_nvx_port_output].eNvidiaTunnelTransaction == ENVX_TRANSACTION_NVMM_TUNNEL);

        NvxNvMMTransformSetStreamUsesSurfaces(&pNvxVideoDecoder->oBaseTwo, s_nvx_port_output, bNvidiaTunneled);
    }

    pNvxVideoDecoder->bFirstFrame = OMX_TRUE;
    pNvxVideoDecoder->bInitialized = OMX_TRUE;
    return eError;
}

OMX_ERRORTYPE NvxMJpegVideoDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxMJpegVideoDecoderInit\n"));

    eError = NvxCommonVideoDecoderInit(hComponent, NvMMBlockType_DecSuperJPEG, "BlockSuperJpgDec");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxMJpegVideoDecoderInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);

    pNvComp->pComponentName = "OMX.Nvidia.mjpeg.decoder";
    pNvComp->sComponentRoles[0] = "video_decoder.mjpeg";
    pNvComp->nComponentRoles    = 1;

    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_input], OMX_DirInput,
                     MAX_INPUT_BUFFERS, MIN_INPUT_BUFFERSIZE, OMX_VIDEO_CodingMJPEG);
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERSIZE);
    pNvComp->pPorts[s_nvx_port_input].eNvidiaTunnelTransaction = ENVX_TRANSACTION_NVMM_TUNNEL;

    return eError;
}
