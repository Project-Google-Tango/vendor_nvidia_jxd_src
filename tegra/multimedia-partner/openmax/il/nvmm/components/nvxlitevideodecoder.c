/*
 * Copyright (c) 2006-2014 NVIDIA Corporation.  All Rights Reserved.
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
#include "nvmm/components/common/nvmmlitetransformbase.h"

#if USE_ANDROID_NATIVE_WINDOW
#include "NvxHelpers.h"
#include "nvxliteandroidbuffer.h"
#include "nvgr.h"
#endif


#include "nvmmlite_video.h"
#include "nvmmlite_videodec.h"
#include "nvmm_util.h"

#include "nvfxmath.h"

#if USE_ANDROID_NATIVE_WINDOW
#define USE_ANDROID_METADATA_BUFFERS (1)
#else
#define USE_ANDROID_METADATA_BUFFERS (0)
#endif

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */

// DEBUG defines. TO DO: fix this
#define MIN_INPUT_BUFFERS    2
#define MAX_INPUT_BUFFERS    10
#define MAX_OUTPUT_BUFFERS   20
#define MIN_INPUT_BUFFERSIZE 1024
#define MAX_INPUT_BUFFERSIZE 2048*2048
#define MIN_OUTPUT_BUFFERSIZE 384
#define MAX_OUTPUT_BUFFERSIZE (640*480*3)/2

/* Number of internal frame buffers (video memory) */
static const int s_nvx_num_ports            = 2;
static const unsigned int s_nvx_port_input           = 0;
static const unsigned int s_nvx_port_output          = 1;

/* Decoder State information */
typedef struct SNvxLiteVideoDecoderData
{
    OMX_BOOL bInitialized;
    OMX_BOOL bOverrideTimestamp;
    OMX_BOOL bErrorReportingEnabled;

    SNvxNvMMLiteTransformData oBase;
    OMX_BOOL bFirstFrame;

#define TYPE_MP4   0
#define TYPE_H264  1
#define TYPE_VC1   2
#define TYPE_MPEG2 3
#define TYPE_MJPEG 6
#define TYPE_VP8   7
#define TYPE_H265  8

    int oType;

    NvMMLiteBlockType oBlockType;
    const char *sBlockName;
    OMX_BOOL bLowMem;

    OMX_BOOL bSkipFrames;
    OMX_BOOL bUseLowResource;
    OMX_BOOL bH264DisableDPB;
    OMX_BOOL bFullFrameData;
    OMX_U32  nTunneledBufferCount; /* Requested number of buffers when in Tunneled mode*/
    NvMMLiteVDec_DeinterlacingOptions AttribDeint;     /* deinterlacing method */
    NvU32    nSurfaceLayout;
    OMX_BOOL bUseLowOutBuff;
    OMX_BOOL bFilterTimestamps; /*Enables filtering of timestamps to be in increasing order always*/
    OMX_BOOL bDisableDvfs;
    OMX_BOOL bMjolnirStreaming;
    OMX_BOOL bDRCOptimization;
    NvMMLiteVDec_SliceDecode sliceDecodeEnable;
    OMX_BOOL bClearAsEncrypted;
    OMX_BOOL bDecodeState;
} SNvxLiteVideoDecoderData;

static OMX_ERRORTYPE NvxVideoDecoderReleaseResources(OMX_IN NvxComponent *pComponent);
static OMX_ERRORTYPE NvxVideoDecoderAcquireResources(OMX_IN NvxComponent *pComponent);

extern void NvxNvMMLiteConvertProfilLevelToOmx(NvMMLiteVideoCodingType eType,
                                               OMX_U32 nNvmmLitePfl,
                                               OMX_U32 nNvmmLiteLvl,
                                               OMX_U32 *pOmxPfl,
                                               OMX_U32 *pOmxLvl);

static OMX_ERRORTYPE NvxVideoDecoderDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    SNvxLiteVideoDecoderData *pNvxVideoDecoder;
    pNvxVideoDecoder = (SNvxLiteVideoDecoderData *)pNvComp->pComponentData;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoDecoderDeInit\n"));

    if (pNvxVideoDecoder->oBase.hBlock)
        NvxNvMMLiteTransformClose(&pNvxVideoDecoder->oBase);

#ifdef BUILD_GOOGLETV
    if (pNvxVideoDecoder->oBase.bSecure == OMX_TRUE)
        NvMMCryptoDeinit();
#endif

    NvOsFree(pNvComp->pComponentData);
    pNvComp->pComponentData = 0;
    return eError;
}

static OMX_ERRORTYPE NvxVideoDecoderGetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    SNvxLiteVideoDecoderData *pNvxVideoDecoder;

    pNvxVideoDecoder = (SNvxLiteVideoDecoderData *)pComponent->pComponentData;

    NV_ASSERT(pNvxVideoDecoder);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoDecoderGetParameter\n"));

    switch (nParamIndex)
    {
    case OMX_IndexParamVideoH263:
        {
            NvMMLiteVideoDec_H263ParamsInfo H263params;
            OMX_VIDEO_PARAM_H263TYPE *pH263Params;
            NvError err = OMX_ErrorNone;
            if (((OMX_VIDEO_PARAM_H263TYPE *)pComponentParameterStructure)->nPortIndex != (NvU32)s_nvx_port_input || pNvxVideoDecoder->oType != TYPE_MP4)
                return OMX_ErrorBadPortIndex;

            if(!pNvxVideoDecoder->oBase.hBlock)
                return OMX_ErrorNotReady;

            err = pNvxVideoDecoder->oBase.hBlock->GetAttribute(
                pNvxVideoDecoder->oBase.hBlock, NvMMLiteVideoDecAttribute_H263TYPE,
                            sizeof(NvMMLiteVideoDec_H263ParamsInfo), &H263params);

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
            NvMMLiteVideoDec_MPEG4ParamsInfo mpeg4params;
            OMX_VIDEO_PARAM_MPEG4TYPE *pMpeg4Params;
            NvError err = OMX_ErrorNone;
            if (((OMX_VIDEO_PARAM_MPEG4TYPE *)pComponentParameterStructure)->nPortIndex != (NvU32)s_nvx_port_input || pNvxVideoDecoder->oType != TYPE_MP4)
                return OMX_ErrorBadPortIndex;

            if(!pNvxVideoDecoder->oBase.hBlock)
                return OMX_ErrorNotReady;

            err = pNvxVideoDecoder->oBase.hBlock->GetAttribute(
                pNvxVideoDecoder->oBase.hBlock, NvMMLiteVideoDecAttribute_MPEG4TYPE,
                            sizeof(NvMMLiteVideoDec_MPEG4ParamsInfo), &mpeg4params);
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
            NvMMLiteVideoDec_Mpeg2ParamsInfo mpeg2params;
            OMX_VIDEO_PARAM_MPEG2TYPE *pMpeg2Params;
            NvError err = OMX_ErrorNone;

            if (((OMX_VIDEO_PARAM_MPEG2TYPE *)pComponentParameterStructure)->nPortIndex != (NvU32)s_nvx_port_input || pNvxVideoDecoder->oType != TYPE_MPEG2)
                return OMX_ErrorBadPortIndex;

            if(!pNvxVideoDecoder->oBase.hBlock)
                return OMX_ErrorNotReady;

            err = pNvxVideoDecoder->oBase.hBlock->GetAttribute(
                pNvxVideoDecoder->oBase.hBlock, NvMMLiteVideoDecAttribute_MPEG2TYPE,
                            sizeof(NvMMLiteVideoDec_Mpeg2ParamsInfo), &mpeg2params);

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
        NvMMLiteVideoDec_AVCParamsInfo avcparams;
        OMX_VIDEO_PARAM_AVCTYPE *pAvcParams;
        NvError err = OMX_ErrorNone;

        if (((OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure)->nPortIndex != (NvU32)s_nvx_port_input || pNvxVideoDecoder->oType != TYPE_H264)
            return OMX_ErrorBadPortIndex;

        if(!pNvxVideoDecoder->oBase.hBlock)
            return OMX_ErrorNotReady;

        err = pNvxVideoDecoder->oBase.hBlock->GetAttribute(
                pNvxVideoDecoder->oBase.hBlock, NvMMLiteVideoDecAttribute_AVCTYPE,
                            sizeof(NvMMLiteVideoDec_AVCParamsInfo), &avcparams);
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
        OMX_VIDEO_PARAM_PROFILELEVELTYPE * pProfLvl = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
        NvMMLiteVideoProfileLevelInfo info;
        NvError err = OMX_ErrorNone;

        info.ProfileIndex = pProfLvl->nProfileIndex;

        if (!pNvxVideoDecoder->oBase.hBlock)
        {
            err = NvxNvMMLiteTransformOpen(&pNvxVideoDecoder->oBase,
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
        else if (pNvxVideoDecoder->oType == TYPE_H265)
        {
            info.eType = NvMMLiteVideo_CodingTypeH265;
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

        err = pNvxVideoDecoder->oBase.hBlock->GetAttribute(pNvxVideoDecoder->oBase.hBlock,
                                            NvMMLiteVideoDecAttribute_ProfileLevelQuery,
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
        NVX_PARAM_CODECCAPABILITY * pCap = (NVX_PARAM_CODECCAPABILITY *)pComponentParameterStructure;
        NvMMLiteVideoMaxCapabilities info;
        NvError err = OMX_ErrorNone;

        info.CapIndex = pCap->nCapIndex;

        if(!pNvxVideoDecoder->oBase.hBlock)
        {
            err = NvxNvMMLiteTransformOpen(&pNvxVideoDecoder->oBase,
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
        else if (pNvxVideoDecoder->oType == TYPE_H265)
        {
            info.eType = NvMMLiteVideo_CodingTypeH265;
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

        err = pNvxVideoDecoder->oBase.hBlock->GetAttribute(pNvxVideoDecoder->oBase.hBlock,
                                            NvMMLiteVideoDecAttribute_MaxCapabilities,
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

                if (pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output].bHasSize)
                {
                    /* XXX: As of now the StageFright does not seem to use the
                    * nStride and nSliceHeight params to allocate surfaces,
                    * probably coming in L
                    * pPortDef->format.video.nFrameWidth = pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output].outRect.nWidth;
                    * pPortDef->format.video.nFrameHeight = pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output].outRect.nHeight;*/

                    pPortDef->format.video.nFrameWidth = pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output].nWidth;
                    pPortDef->format.video.nFrameHeight = pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output].nHeight;
                    pPortDef->format.video.nStride = pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output].nWidth;
                    pPortDef->format.video.nSliceHeight = pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output].nHeight;
                }
                else
                {
                    pPortDef->format.video.nStride = pPortDef->format.video.nFrameWidth;
                    pPortDef->format.video.nSliceHeight = pPortDef->format.video.nFrameHeight;
                }

                pPortDef->format.video.nFrameWidth = (pPortDef->format.video.nFrameWidth + 1) & ~0x1;
                pPortDef->format.video.nFrameHeight = (pPortDef->format.video.nFrameHeight + 1) & ~0x1;
                pPortDef->format.video.nStride = (pPortDef->format.video.nStride + 1) & ~0x1;
                pPortDef->format.video.nSliceHeight = (pPortDef->format.video.nSliceHeight + 1) & ~0x1;

                switch (pPortDef->format.video.eColorFormat) {
                    case OMX_COLOR_FormatYUV420Planar:
                    case OMX_COLOR_FormatYUV420SemiPlanar:
                            pPortDef->nBufferSize = (pPortDef->format.video.nStride*pPortDef->format.video.nSliceHeight*3)>>1;
                    break;

                    case OMX_COLOR_FormatYUV422Planar:
                    case OMX_COLOR_FormatYUV422SemiPlanar:
                            pPortDef->nBufferSize = (pPortDef->format.video.nStride*pPortDef->format.video.nSliceHeight*2);
                    break;

#if USE_ANDROID_NATIVE_WINDOW
                    case NVGR_PIXEL_FORMAT_YUV420:
                    case HAL_PIXEL_FORMAT_YV12:
                    case NVGR_PIXEL_FORMAT_NV12:
                            pPortDef->nBufferSize = (pPortDef->format.video.nStride*pPortDef->format.video.nSliceHeight*3)>>1;
                    break;

                    case NVGR_PIXEL_FORMAT_YUV422:
                    case NVGR_PIXEL_FORMAT_YUV422R:
                    case NVGR_PIXEL_FORMAT_NV16:
                            pPortDef->nBufferSize = (pPortDef->format.video.nStride*pPortDef->format.video.nSliceHeight*2);
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
        return HandleGetANBUsageLite(pComponent, pComponentParameterStructure, 0);
#endif

    case NVX_IndexParamSurfaceLayout:
        {
            NVX_PARAM_SURFACELAYOUT *param = (NVX_PARAM_SURFACELAYOUT*)pComponentParameterStructure;
            if (param) {
                OMX_ERRORTYPE eError = pNvxVideoDecoder->oBase.hBlock->GetAttribute(pNvxVideoDecoder->oBase.hBlock,
                                                                     NvMMLiteVideoDecAttribute_SurfaceLayout,
                                                                     sizeof(NvU32),
                                                                     &pNvxVideoDecoder->nSurfaceLayout);
                if (eError != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("In GetAttribute() FAILED on SurfaceLayout\n"));
                    return OMX_ErrorBadParameter;
                }

                param->bTiledMode = (OMX_BOOL)(pNvxVideoDecoder->nSurfaceLayout == NvRmSurfaceLayout_Tiled);
            }
        }
        break;
    case NVX_IndexParamFilterTimestamps:
        {
            NvError err = NvSuccess;
            NVX_PARAM_FILTER_TIMESTAMPS *pParamFilterTimestamps = (NVX_PARAM_FILTER_TIMESTAMPS *)pComponentParameterStructure;

            if (pNvxVideoDecoder->oBase.hBlock)
            {
                err = pNvxVideoDecoder->oBase.hBlock->GetAttribute(pNvxVideoDecoder->oBase.hBlock,
                                                        NvMMLiteVideoDecAttribute_FilterTimestamp,
                                                        sizeof(OMX_BOOL), &pNvxVideoDecoder->bFilterTimestamps);
                if (err != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("In NvxVideoDecoderGetParameter(), GetAttribute() FAILED.\n"));
                    return OMX_ErrorBadParameter;
                }
            }
            pParamFilterTimestamps->bFilterTimestamps = pNvxVideoDecoder->bFilterTimestamps;
        }
        break;
    case NVX_IndexParamVideoDecFullSliceData:
        {
            NvError err = NvSuccess;
            NVX_VIDEO_PARAM_SLICE_DECODE *pSliceStruct = (NVX_VIDEO_PARAM_SLICE_DECODE *)pComponentParameterStructure;
            pSliceStruct->nSize = sizeof(NVX_VIDEO_PARAM_SLICE_DECODE);

            if (pNvxVideoDecoder->oBase.hBlock)
            {
                err = pNvxVideoDecoder->oBase.hBlock->GetAttribute(pNvxVideoDecoder->oBase.hBlock,
                                                        NvMMLiteVideoDecAttribute_CompleteSliceInputBuffer,
                                                        sizeof(NvMMLiteVDec_SliceDecode), &pNvxVideoDecoder->sliceDecodeEnable);
                if (err != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("In NvxVideoDecoderGetParameter(), GetSliceDecodeParameter() FAILED.\n"));
                    return OMX_ErrorBadParameter;
                }
            }
            pSliceStruct->nAuthentication = 0;
            pSliceStruct->bEnabled = pNvxVideoDecoder->sliceDecodeEnable.bEnabled;
        }
        break;

    default:
        return NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
    };

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxVideoDecoderSetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentParameterStructure)
{
    SNvxLiteVideoDecoderData *pNvxVideoDecoder;
    pNvxVideoDecoder = (SNvxLiteVideoDecoderData *)pComponent->pComponentData;

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
            NVX_PARAM_DEINTERLACE *pDeinterlace =
                (NVX_PARAM_DEINTERLACE *)pComponentParameterStructure;
            pNvxVideoDecoder->AttribDeint.StructSize = sizeof(NvMMLiteVDec_DeinterlacingOptions);
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
        OMX_ERRORTYPE err = HandleEnableANBLite(pComponent, s_nvx_port_output,
                                            pComponentParameterStructure);

        pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output].bUsesANBs = (pComponent->pPorts[s_nvx_port_output].oPortDef.format.video.eColorFormat != OMX_COLOR_FormatYUV420Planar);
        return err;
    }
    case NVX_IndexParamUseAndroidNativeBuffer:
        return HandleUseANBLite(pComponent, pComponentParameterStructure);
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
    case NVX_IndexParamVideoDecFullSliceData:
        {
            NVX_VIDEO_PARAM_SLICE_DECODE *pSliceStruct = (NVX_VIDEO_PARAM_SLICE_DECODE *)pComponentParameterStructure;
            pNvxVideoDecoder->sliceDecodeEnable.StructSize = sizeof(NvMMLiteVDec_SliceDecode);
            pNvxVideoDecoder->sliceDecodeEnable.bEnabled = pSliceStruct->bEnabled;
            pNvxVideoDecoder->sliceDecodeEnable.nAuthentication = pSliceStruct->nAuthentication;
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
            else if (s_nvx_port_output == pParam->nPortIndex)
                pNvxVideoDecoder->bUseLowOutBuff = pParam->bUseLowBuffer;
        }
        break;
    case NVX_IndexParamEmbedMMBuffer:
        {
            OMX_CONFIG_BOOLEANTYPE *param = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;
            if (param)
                pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output].bEmbedNvMMBuffer = param->bEnabled;
        }
        break;

    case NVX_IndexParamSurfaceLayout:
        {
            NVX_PARAM_SURFACELAYOUT *param = (NVX_PARAM_SURFACELAYOUT*)pComponentParameterStructure;
            if (param) {
                pNvxVideoDecoder->nSurfaceLayout = param->bTiledMode ?
                                        NvRmSurfaceLayout_Tiled : NvRmSurfaceLayout_Pitch;
                NvOsDebugPrintf("%s@%d: SurfaceLayout = %d\n", __FUNCTION__, __LINE__, pNvxVideoDecoder->nSurfaceLayout);
            }
        }
        break;
    case NVX_IndexParamFilterTimestamps:
        {
            NVX_PARAM_FILTER_TIMESTAMPS *pParamFilterTimestamps = (NVX_PARAM_FILTER_TIMESTAMPS *)pComponentParameterStructure;

            pNvxVideoDecoder->bFilterTimestamps = pParamFilterTimestamps->bFilterTimestamps;
        }
        break;
    case NVX_IndexParamVideoDisableDvfs:
        {
            OMX_CONFIG_BOOLEANTYPE *DisableDvfs = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;
            if(DisableDvfs)
                pNvxVideoDecoder->bDisableDvfs = DisableDvfs->bEnabled;
        }
        break;
    case NVX_IndexParamVideoMjolnirStreaming:
        {
            OMX_CONFIG_BOOLEANTYPE *bMjolnirStreaming = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;
            if(bMjolnirStreaming)
                pNvxVideoDecoder->bMjolnirStreaming = bMjolnirStreaming->bEnabled;
        }
        break;
#if USE_ANDROID_METADATA_BUFFERS
    case NVX_IndexParamStoreMetaDataBuffer:
        {
            return HandleStoreMetaDataInBuffersParamLite(
                       pComponentParameterStructure,
                       pNvxVideoDecoder->oBase.oPorts,
                       s_nvx_port_output);
        }
        break;
#endif
    case NVX_IndexParamVideoDRCOptimization:
        {
            OMX_CONFIG_BOOLEANTYPE *bDRCOptimization = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;
            if(bDRCOptimization)
                pNvxVideoDecoder->bDRCOptimization = bDRCOptimization->bEnabled;
        }
        break;
    case NVX_IndexParamVideoPostProcessType:
        {
            NVX_PARAM_VPP *param = (NVX_PARAM_VPP *)pComponentParameterStructure;
            pNvxVideoDecoder->oBase.sVpp.nVppType = param->nVppType;
            pNvxVideoDecoder->oBase.sVpp.bVppEnable = param->bVppEnable;
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
    SNvxLiteVideoDecoderData *pNvxVideoDecoder;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoDecoderGetConfig\n"));

    pNvxVideoDecoder = (SNvxLiteVideoDecoderData *)pNvComp->pComponentData;
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
            SNvxNvMMLitePort *pPort;
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
            SNvxNvMMLitePort *pPort;
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
                NvMMLiteVideoDecH264StereoInfo stereoInfo;
                NvError err = NvSuccess;

                NV_DEBUG_PRINTF(("Check Stereo Status by querying the NvMM Decoder module\n"));

                NvOsMemset(&stereoInfo, 0,
                           sizeof(NvMMLiteVideoDecH264StereoInfo));
                err = pNvxVideoDecoder->oBase.hBlock->GetAttribute(pNvxVideoDecoder->oBase.hBlock,
                                                    NvMMLiteVideoDecAttribute_H264StereoInfo,
                                                    sizeof(NvMMLiteVideoDecH264StereoInfo),
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

        case NVX_IndexConfigMVCStitchInfo:
        {

            NVX_CONFIG_VIDEO_MVC_INFO* pMVCInfo =
            (NVX_CONFIG_VIDEO_MVC_INFO*)pComponentConfigStructure;

            if (pNvxVideoDecoder->bInitialized)
            {
                NvBool bstitchInfo;
                NvError err = NvSuccess;

                err = pNvxVideoDecoder->oBase.hBlock->GetAttribute(pNvxVideoDecoder->oBase.hBlock,
                                                                    NvMMLiteVideoDecAttribute_H264StitchMVCViews,
                                                                    sizeof(NvBool),
                                                                    &(bstitchInfo));
                if (err != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("In NvxVideoDecoderGetConfig(), GetAttribute() FAILED.\n"));
                    return OMX_ErrorBadParameter;
                }

                pMVCInfo->stitch_MVCViews_Flag = (OMX_BOOL)bstitchInfo;
            }

        }
        break;

        case OMX_IndexConfigTimeSeekMode:
        {
            OMX_TIME_CONFIG_SEEKMODETYPE* pSeekMode = (OMX_TIME_CONFIG_SEEKMODETYPE*)pComponentConfigStructure;
            SNvxNvMMLitePort *pPort;

            pPort = &(pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output]);
            pSeekMode->eType = pPort->nSeekType;
            NV_DEBUG_PRINTF(("GetConfig OMX_IndexConfigTimeSeekMode eType - %d",pSeekMode->eType));
        }
        break;
        case OMX_IndexConfigTimePosition:
        {
            OMX_TIME_CONFIG_TIMESTAMPTYPE* pTimePosition = (OMX_TIME_CONFIG_TIMESTAMPTYPE*)pComponentConfigStructure;
            SNvxNvMMLitePort *pPort;

            pPort = &(pNvxVideoDecoder->oBase.oPorts[s_nvx_port_input]);
            pTimePosition->nTimestamp = pPort->position;
            NV_DEBUG_PRINTF(("GetConfig OMX_IndexConfigTimePosition nTimestamp - %lld",pTimePosition->nTimestamp));
        }
        break;

        case NVX_IndexConfigAribConstraints:
        {
            OMX_CONFIG_ARIBCONSTRAINTS *con = (OMX_CONFIG_ARIBCONSTRAINTS*)pComponentConfigStructure;

            if (pNvxVideoDecoder->bInitialized)
            {
                NvMMLiteVideoDec_AribConstraintsInfo constraints;
                NvError err = NvSuccess;
                err = pNvxVideoDecoder->oBase.hBlock->GetAttribute(pNvxVideoDecoder->oBase.hBlock,
                                                    NvMMLiteVideoDecAttribute_AribConstraints,
                                                    sizeof(NvMMLiteVideoDec_AribConstraintsInfo),
                                                    &constraints);
                if (err != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("In NvxVideoDecoderGetConfig(), GetAttribute() FAILED \n"));
                    return OMX_ErrorBadParameter;
                }
                con->nWidth                  = constraints.Width;
                con->nHeight                 = constraints.Height;
            }
        }
        break;
        case OMX_IndexConfigCommonMirror:
        {
            OMX_CONFIG_MIRRORTYPE *pMirror = (OMX_CONFIG_MIRRORTYPE *)pComponentConfigStructure;
            pMirror->eMirror = pNvxVideoDecoder->oBase.mirror_type;
        }
        break;
        case NVX_IndexConfigVideoDecodeState:
        {
            NvError err = NvSuccess;
            OMX_CONFIG_VIDEODECODESTATE *pDecodeState = (OMX_CONFIG_VIDEODECODESTATE*)pComponentConfigStructure;
            err = pNvxVideoDecoder->oBase.hBlock->GetAttribute(pNvxVideoDecoder->oBase.hBlock,
                                                NvMMLiteVideoDecAttribute_DecodeState,
                                                sizeof(NvBool), &pNvxVideoDecoder->bDecodeState);
            if (err != NvSuccess)
            {
                NV_DEBUG_PRINTF(("In NvxVideoDecoderSetConfig(), GetAttribute() FAILED.\n"));
                return OMX_ErrorBadParameter;
            }
            pDecodeState->bDecodeState = pNvxVideoDecoder->bDecodeState;
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
    SNvxLiteVideoDecoderData *pNvxVideoDecoder;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoDecoderSetConfig\n"));

    pNvxVideoDecoder = (SNvxLiteVideoDecoderData *)pNvComp->pComponentData;
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
            NvxNvMMLiteTransformSetProfile(&pNvxVideoDecoder->oBase, pProf);
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
            return OMX_ErrorNone;
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
                NvMMLiteVideoDecH264StereoInfo stereoInfo;
                OMX_U32 stereoflags = 0;
                NvError err = NvSuccess;
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
                stereoInfo.StructSize = sizeof(NvMMLiteVideoDecH264StereoInfo);
                stereoInfo.StereoFlags = stereoflags;

                err = pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock,
                                                    NvMMLiteVideoDecAttribute_H264StereoInfo,
                                                    0, stereoInfo.StructSize, &stereoInfo);
                if (err != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("In NvxVideoDecoderSetConfig(), SetAttribute() FAILED.\n"));
                    return OMX_ErrorBadParameter;
                }
            }

        }
        break;

    case NVX_IndexConfigMVCStitchInfo:
        {

            NVX_CONFIG_VIDEO_MVC_INFO* pMVCInfo =
                (NVX_CONFIG_VIDEO_MVC_INFO*)pComponentConfigStructure;

            if (pNvxVideoDecoder->bInitialized)
            {
                NvBool bstitchInfo;
                NvError err = NvSuccess;

                bstitchInfo = (NvBool)pMVCInfo->stitch_MVCViews_Flag;

                err = pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock,
                                                    NvMMLiteVideoDecAttribute_H264StitchMVCViews,
                                                    0, sizeof(NvBool), &(bstitchInfo));
                if (err != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("In NvxVideoDecoderSetConfig(), SetAttribute() FAILED.\n"));
                    return OMX_ErrorBadParameter;
                }
            }

        }
        break;

    case OMX_IndexConfigTimeSeekMode:
        {
            OMX_TIME_CONFIG_SEEKMODETYPE* pSeekMode = (OMX_TIME_CONFIG_SEEKMODETYPE*)pComponentConfigStructure;
            SNvxNvMMLitePort *pPort;

            NV_DEBUG_PRINTF(("SetConfig OMX_IndexConfigTimeSeekMode eType - %d",pSeekMode->eType));

            pPort = &(pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output]);
            pPort->nSeekType = pSeekMode->eType;
        }
        break;
    case OMX_IndexConfigTimePosition:
        {
            OMX_TIME_CONFIG_TIMESTAMPTYPE* pTimePosition = (OMX_TIME_CONFIG_TIMESTAMPTYPE*)pComponentConfigStructure;
            SNvxNvMMLitePort *pPort;

            NV_DEBUG_PRINTF(("SetConfig OMX_IndexConfigTimePosition nTimestamp - %lld",pTimePosition->nTimestamp));

            pPort = &(pNvxVideoDecoder->oBase.oPorts[s_nvx_port_output]);
            pPort->nTimestamp = pTimePosition->nTimestamp * 10;
        }
        break;
    case NVX_IndexConfigVideoDecodeIFrames:
        {
            OMX_CONFIG_BOOLEANTYPE *pDecodeIFrames = (OMX_CONFIG_BOOLEANTYPE*)pComponentConfigStructure;
            if (pDecodeIFrames->nSize == sizeof(OMX_CONFIG_BOOLEANTYPE))
            {
                NvBool bDecodeIFrames = pDecodeIFrames->bEnabled;
                NvError err = NvSuccess;
                err = pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock,
                                                    NvMMLiteVideoDecAttribute_DecodeIDRFramesOnly,
                                                    0, sizeof(NvBool), &bDecodeIFrames);
                if (err != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("In NvxVideoDecoderSetConfig(), SetAttribute() FAILED.\n"));
                    return OMX_ErrorBadParameter;
                }
            }
        }
        break;
    case NVX_IndexConfigWaitOnFence:
        {
            OMX_CONFIG_BOOLEANTYPE *param = (OMX_CONFIG_BOOLEANTYPE *)pComponentConfigStructure;
            pNvxVideoDecoder->oBase.bWaitOnFence = param->bEnabled;
        }
        break;
    case OMX_IndexConfigCommonMirror:
        {
            OMX_CONFIG_MIRRORTYPE *pMirror = (OMX_CONFIG_MIRRORTYPE *)pComponentConfigStructure;
            pNvxVideoDecoder->oBase.mirror_type = pMirror->eMirror;
        }
        break;
    case NVX_IndexConfigThumbnailMode:
        {
            OMX_CONFIG_BOOLEANTYPE *param = (OMX_CONFIG_BOOLEANTYPE *)pComponentConfigStructure;
            pNvxVideoDecoder->oBase.bThumbnailMode = param->bEnabled;
        }
        break;
    case NVX_IndexConfigVideoDecodeState:
        {
            NvError err = NvSuccess;
            OMX_CONFIG_VIDEODECODESTATE *pDecodeState = (OMX_CONFIG_VIDEODECODESTATE*)pComponentConfigStructure;
            pNvxVideoDecoder->bDecodeState = pDecodeState->bDecodeState;
            err = pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock,
                                                NvMMLiteVideoDecAttribute_DecodeState,
                                                0, sizeof(NvBool), &pNvxVideoDecoder->bDecodeState);
            if (err != NvSuccess)
            {
                NV_DEBUG_PRINTF(("In NvxVideoDecoderSetConfig(), SetAttribute() FAILED.\n"));
                return OMX_ErrorBadParameter;
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
    SNvxLiteVideoDecoderData *pNvxVideoDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];
    NvxPort *pPortOut = &pComponent->pPorts[s_nvx_port_output];

    NVXTRACE((NVXT_WORKER, pComponent->eObjectType, "+NvxVideoDecoderWorkerFunction\n"));

    pNvxVideoDecoder = (SNvxLiteVideoDecoderData *)pComponent->pComponentData;

    *pbMoreWork = bAllPortsReady;

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

    eError = NvxNvMMLiteTransformWorkerBase(&pNvxVideoDecoder->oBase,
                                        s_nvx_port_input, pbMoreWork);

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

    return eError;
}

static OMX_ERRORTYPE NvxVideoDecoderAcquireResources(OMX_IN NvxComponent *pComponent)
{
    SNvxLiteVideoDecoderData *pNvxVideoDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    // By default disabling the SkipWorker thread for optimization of untunneled ports
    OMX_BOOL bCanSkipWorker = OMX_FALSE;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoDecoderAcquireResources\n"));

    /* Get MP4 Decoder instance */
    pNvxVideoDecoder = (SNvxLiteVideoDecoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxVideoDecoder);

    eError = NvxComponentBaseAcquireResources(pComponent);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pComponent->eObjectType, "ERROR:NvxVideoDecoderAcquireResources:"
           ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    if (!pNvxVideoDecoder->oBase.hBlock)
    {
        eError = NvxNvMMLiteTransformOpen(&pNvxVideoDecoder->oBase,
                                          pNvxVideoDecoder->oBlockType,
                                          pNvxVideoDecoder->sBlockName,
                                          s_nvx_num_ports,
                                          pComponent);
        if (eError != OMX_ErrorNone)
        {
            NvOsDebugPrintf("%s : NvxNvMMLiteTransformOpen failed \n", __func__);
            return eError;
        }
    }

    eError = NvxNvMMLiteTransformSetupInput(&pNvxVideoDecoder->oBase, s_nvx_port_input, &pComponent->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERS, MAX_INPUT_BUFFERSIZE, bCanSkipWorker);
    if (eError != OMX_ErrorNone)
        return eError;

    eError = NvxNvMMLiteTransformSetupOutput(&pNvxVideoDecoder->oBase, s_nvx_port_output, &pComponent->pPorts[s_nvx_port_output], s_nvx_port_input, MAX_OUTPUT_BUFFERS, MIN_OUTPUT_BUFFERSIZE);
    if (eError != OMX_ErrorNone)
        return eError;

    {
        OMX_BOOL bNvidiaTunneled =
        (pComponent->pPorts[s_nvx_port_output].bNvidiaTunneling) &&
        (pComponent->pPorts[s_nvx_port_output].eNvidiaTunnelTransaction == ENVX_TRANSACTION_GFSURFACES ||
         pComponent->pPorts[s_nvx_port_output].eNvidiaTunnelTransaction == ENVX_TRANSACTION_NVMM_TUNNEL);

        NvxNvMMLiteTransformSetStreamUsesSurfaces(&pNvxVideoDecoder->oBase, s_nvx_port_output, bNvidiaTunneled);
    }

    if (pNvxVideoDecoder->bH264DisableDPB &&
        (!NvOsStrcmp(pComponent->pComponentName, "OMX.Nvidia.h264.decode") ||
         !NvOsStrcmp(pComponent->pComponentName, "OMX.Nvidia.h264.decode.secure") ||
         !NvOsStrcmp(pComponent->pComponentName, "OMX.Nvidia.h264.decode.low.latency")))
    {
        pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock,
            NvMMLiteVideoDecAttribute_H264_DisableDPB,
            0, sizeof(OMX_BOOL), &pNvxVideoDecoder->bH264DisableDPB);
    }

    if (pNvxVideoDecoder->oBase.bThumbnailMode)
    {
                    pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock,NvMMLiteVideoDecAttribute_ThumbnailDecode,0, sizeof(OMX_BOOL), &pNvxVideoDecoder->oBase.bThumbnailMode);
    }
    if (OMX_TRUE == pNvxVideoDecoder->bUseLowOutBuff)
    {
        NvBool bLowOutBuff = NV_TRUE;

        pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock,
            NvMMLiteVideoDecAttribute_UseLowOutputBuffer,
            0, sizeof(NvBool), &bLowOutBuff);

        pNvxVideoDecoder->oBase.bUseLowOutBuff = OMX_TRUE;
    }

    if (pNvxVideoDecoder->bFullFrameData)
    {
        pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock,
            NvMMLiteVideoDecAttribute_CompleteFrameInputBuffer,
            0, sizeof(OMX_BOOL), &pNvxVideoDecoder->bFullFrameData);
    }
    if (pNvxVideoDecoder->sliceDecodeEnable.bEnabled)
    {
        NvError err = NvSuccess;
        err = pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock,
                    NvMMLiteVideoDecAttribute_CompleteSliceInputBuffer,
                    0, sizeof(NvMMLiteVDec_SliceDecode), &pNvxVideoDecoder->sliceDecodeEnable);
        if (err != NvSuccess)
            return OMX_ErrorBadParameter;
    }
    if(pNvxVideoDecoder->bDisableDvfs)
    {
        NvError err = NvSuccess;
        err = pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock,
                    NvMMLiteVideoDecAttribute_DisableDFS,
                    0, sizeof(OMX_BOOL),&pNvxVideoDecoder->bDisableDvfs);
        if (err != NvSuccess)
            return OMX_ErrorBadParameter;
    }
    if(pNvxVideoDecoder->bMjolnirStreaming)
    {
        NvError err = NvSuccess;
        err = pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock,
                    NvMMLiteVideoDecAttribute_Mjolnir_Streaming_playback,
                    0, sizeof(OMX_BOOL),&pNvxVideoDecoder->bMjolnirStreaming);
        if (err != NvSuccess)
            return OMX_ErrorBadParameter;
    }

    //Set attribute for surface layout
    pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock,
        NvMMLiteVideoDecAttribute_SurfaceLayout,
        0, sizeof(pNvxVideoDecoder->nSurfaceLayout),
        &pNvxVideoDecoder->nSurfaceLayout);

    //Set attribute for Deinterlacing
    pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock,
        NvMMLiteVideoDecAttribute_Deinterlacing,
        0, sizeof(NvMMLiteVDec_DeinterlacingOptions),
        &pNvxVideoDecoder->AttribDeint);

    if (pNvxVideoDecoder->bFilterTimestamps)
    {
        NvError err = NvSuccess;
        err = pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock,
                           NvMMLiteVideoDecAttribute_FilterTimestamp,
                           0, sizeof(OMX_BOOL), &pNvxVideoDecoder->bFilterTimestamps);
        if (err != NvSuccess)
            return OMX_ErrorNotReady;
    }

    if (pNvxVideoDecoder->bClearAsEncrypted)
    {
        pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock,
            NvMMLiteVideoDecAttribute_ClearAsEncrypted,
            0, sizeof(OMX_BOOL), &pNvxVideoDecoder->bClearAsEncrypted);
    }
    if (pNvxVideoDecoder->bDRCOptimization)
    {
        pNvxVideoDecoder->oBase.hBlock->SetAttribute(pNvxVideoDecoder->oBase.hBlock,
            NvMMLiteVideoDecAttribute_EnableDRCOptimization,
            0, sizeof(OMX_BOOL), &pNvxVideoDecoder->bDRCOptimization);
    }

    pNvxVideoDecoder->bFirstFrame = OMX_TRUE;
    pNvxVideoDecoder->bInitialized = OMX_TRUE;
    return eError;
}

static OMX_ERRORTYPE NvxVideoDecoderReleaseResources(OMX_IN NvxComponent *pComponent)
{
    SNvxLiteVideoDecoderData *pNvxVideoDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoDecoderReleaseResources\n"));

    /* Get MP4 Decoder instance */
    pNvxVideoDecoder = (SNvxLiteVideoDecoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxVideoDecoder);

    eError = NvxNvMMLiteTransformClose(&pNvxVideoDecoder->oBase);

    if (eError != OMX_ErrorNone)
        return eError;

    pNvxVideoDecoder->bInitialized = OMX_FALSE;

    NvxCheckError(eError, NvxComponentBaseReleaseResources(pComponent));
    return eError;
}

static void SetSkipFrames(OMX_BOOL bSkip,
                          SNvxLiteVideoDecoderData *pNvxVideoDecoder,
                          SNvxNvMMLiteTransformData *pBase)
{
    NvU32 val = (NvU32)bSkip;

    if (bSkip == pNvxVideoDecoder->bSkipFrames)
        return;

    pNvxVideoDecoder->bSkipFrames = bSkip;
    pBase->hBlock->SetAttribute(pBase->hBlock, NvMMLiteVideoDecAttribute_SkipFrames,
                                0, sizeof(NvU32), &val);
}

static OMX_ERRORTYPE NvxVideoDecoderFlush(OMX_IN NvxComponent *pComponent, OMX_U32 nPort)
{
    SNvxLiteVideoDecoderData *pNvxVideoDecoder = 0;
    OMX_ERRORTYPE status = OMX_ErrorNone;

    NV_ASSERT(pComponent && pComponent->pComponentData);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxVideoDecoderFlush\n"));
    pNvxVideoDecoder = (SNvxLiteVideoDecoderData *)pComponent->pComponentData;

    SetSkipFrames(OMX_FALSE, pNvxVideoDecoder, &pNvxVideoDecoder->oBase);
    status =  NvxNvMMLiteTransformFlush(&pNvxVideoDecoder->oBase, nPort);

    return status;
}

static OMX_ERRORTYPE NvxVideoDecoderFillThisBuffer(NvxComponent *pNvComp, OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    OMX_ERRORTYPE status;
    SNvxLiteVideoDecoderData *pNvxVideoDecoder = 0;
    SNvxNvMMLiteTransformData *pBase;
    NvxBufferPlatformPrivate *pPrivateData =
                (NvxBufferPlatformPrivate *)pBufferHdr->pPlatformPrivate;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoDecoderFillThisBuffer\n"));

    pNvxVideoDecoder = (SNvxLiteVideoDecoderData *)pNvComp->pComponentData;
    if (!pNvxVideoDecoder->bInitialized)
        return OMX_ErrorNone;

    pBase = &pNvxVideoDecoder->oBase;

    if (pBufferHdr->nFlags & NVX_BUFFERFLAG_DECODERSLOW)
        SetSkipFrames(OMX_TRUE, pNvxVideoDecoder, pBase);
    if (pBufferHdr->nFlags & NVX_BUFFERFLAG_DECODERNOTSLOW)
        SetSkipFrames(OMX_FALSE, pNvxVideoDecoder, pBase);

    if (pPrivateData->pData)
    {
        pBase = (SNvxNvMMLiteTransformData *)pPrivateData->pData;
        pPrivateData->pData = 0;
    }

    status = NvxNvMMLiteTransformFillThisBuffer(pBase, pBufferHdr, s_nvx_port_output);
    if (status != OMX_ErrorNone)
        NV_DEBUG_PRINTF(("%s failed NvxNvMMTransformFillThisBuffer %x", __FUNCTION__, status));

    return status;
}

static OMX_ERRORTYPE NvxVideoDecoderFreeBuffer(NvxComponent *pNvComp, OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    SNvxLiteVideoDecoderData *pNvxVideoDecoder = 0;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoDecoderFillThisBuffer\n"));

    pNvxVideoDecoder = (SNvxLiteVideoDecoderData *)pNvComp->pComponentData;
    if (!pNvxVideoDecoder->bInitialized)
        return OMX_ErrorNone;

    return NvxNvMMLiteTransformFreeBuffer(&pNvxVideoDecoder->oBase, pBufferHdr, s_nvx_port_output);
}


static OMX_ERRORTYPE NvxVideoDecoderPreChangeState(NvxComponent *pNvComp,
                                                   OMX_STATETYPE oNewState)
{
    SNvxLiteVideoDecoderData *pNvxVideoDecoder = 0;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoDecoderPreChangeState\n"));

    pNvxVideoDecoder = (SNvxLiteVideoDecoderData *)pNvComp->pComponentData;
    if (!pNvxVideoDecoder->bInitialized)
        return OMX_ErrorNone;
    return NvxNvMMLiteTransformPreChangeState(&pNvxVideoDecoder->oBase, oNewState,
                                          pNvComp->eState);
}

static OMX_ERRORTYPE NvxVideoDecoderEmptyThisBuffer(NvxComponent *pNvComp,
                                             OMX_BUFFERHEADERTYPE* pBufferHdr,
                                             OMX_BOOL *bHandled)
{
    SNvxLiteVideoDecoderData *pNvxVideoDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoDecoderEmptyThisBuffer\n"));

    pNvxVideoDecoder = (SNvxLiteVideoDecoderData *)pNvComp->pComponentData;
    if (!pNvxVideoDecoder->bInitialized)
        return OMX_ErrorNone;

    if (NvxNvMMLiteTransformIsInputSkippingWorker(&pNvxVideoDecoder->oBase,
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
    SNvxLiteVideoDecoderData *pNvxVideoDecoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxVideoDecoderPortEvent\n"));

    pNvxVideoDecoder = (SNvxLiteVideoDecoderData *)pNvComp->pComponentData;
    if (!pNvxVideoDecoder->bInitialized)
        return eError;

    NvxNvMMLiteTransformPortEventHandler(&pNvxVideoDecoder->oBase, nPort, uEventType);

    return eError;
}

/*****************************************************************************/

static OMX_ERRORTYPE NvxCommonVideoDecoderInit(OMX_HANDLETYPE hComponent, NvMMLiteBlockType oBlockType, const char *sBlockName)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pNvComp;
    SNvxLiteVideoDecoderData *data = NULL;

    eError = NvxComponentCreate(hComponent, s_nvx_num_ports, &pNvComp);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,NVXT_VIDEO_DECODER,"ERROR:NvxCommonVideoDecoderInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    // FIXME: resources?

    pNvComp->eObjectType = NVXT_VIDEO_DECODER;

    pNvComp->pComponentData = NvOsAlloc(sizeof(SNvxLiteVideoDecoderData));
    if (!pNvComp->pComponentData)
        return OMX_ErrorInsufficientResources;

    data = (SNvxLiteVideoDecoderData *)pNvComp->pComponentData;
    NvOsMemset(data, 0, sizeof(SNvxLiteVideoDecoderData));

    switch (oBlockType)
    {
        case NvMMLiteBlockType_DecMPEG4: data->oType = TYPE_MP4; break;
        case NvMMLiteBlockType_DecH264: data->oType = TYPE_H264; break;
        case NvMMLiteBlockType_DecH265: data->oType = TYPE_H265; break;
        case NvMMLiteBlockType_DecVC1: data->oType = TYPE_VC1; break;
        case NvMMLiteBlockType_DecMPEG2: data->oType = TYPE_MPEG2; break;
        case NvMMLiteBlockType_DecMJPEG: data->oType = TYPE_MJPEG; break;
        case NvMMLiteBlockType_DecVP8: data->oType = TYPE_VP8; break;
        default: return OMX_ErrorBadParameter;
    }

    data->bLowMem = OMX_TRUE;
    data->oBlockType = oBlockType;
    data->sBlockName = sBlockName;
    data->bUseLowResource = OMX_FALSE;
    data->bH264DisableDPB = OMX_FALSE;
    data->bFilterTimestamps = OMX_FALSE;
#if USE_FULL_FRAME_MODE
    data->bFullFrameData = NV_TRUE;
#else
    data->bFullFrameData = NV_FALSE;
#endif
    data->bDisableDvfs = OMX_FALSE;
    data->bMjolnirStreaming = OMX_FALSE;
    data->bDRCOptimization = OMX_FALSE;
    data->AttribDeint.StructSize  = sizeof(NvMMLiteVDec_DeinterlacingOptions);
    data->AttribDeint.DeintMethod = NvMMLite_DeintMethod_Advanced1AtFrameRate;
    data->nSurfaceLayout = NvRmSurfaceLayout_Tiled;
    data->oBase.bWaitOnFence = NV_FALSE;
    data->oBase.bThumbnailMode = NV_FALSE;
    data->oBase.mirror_type = OMX_MirrorNone;
    data->sliceDecodeEnable.StructSize = sizeof(NvMMLiteVDec_SliceDecode);
    data->sliceDecodeEnable.bEnabled = OMX_FALSE;
    data->sliceDecodeEnable.nAuthentication = 0;
    data->bClearAsEncrypted = OMX_FALSE;

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
                     2, MIN_OUTPUT_BUFFERSIZE,
                     OMX_VIDEO_CodingUnused);

#if USE_ANDROID_NATIVE_WINDOW
    NvRmSurfaceLayout layout = NvRmSurfaceGetDefaultLayout();
    if (layout == NvRmSurfaceLayout_Blocklinear)
        pNvComp->pPorts[s_nvx_port_output].oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
#else
    pNvComp->pPorts[s_nvx_port_output].oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
#endif //USE_ANDROID_NATIVE_WINDOW

    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_output], MAX_OUTPUT_BUFFERSIZE);
    data->nTunneledBufferCount = MAX_OUTPUT_BUFFERS + 5;
    pNvComp->pPorts[s_nvx_port_output].eNvidiaTunnelTransaction =
            ENVX_TRANSACTION_NVMM_TUNNEL;

    return OMX_ErrorNone;
}

/***************************************************************************/

OMX_ERRORTYPE NvxLiteMp4DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    OMX_VIDEO_PARAM_MPEG4TYPE *pVideoParams;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxMp4DecoderInit\n"));

    eError = NvxCommonVideoDecoderInit(hComponent, NvMMLiteBlockType_DecMPEG4, "BlockMP4Dec");
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
                     MAX_INPUT_BUFFERS, MAX_INPUT_BUFFERSIZE, OMX_VIDEO_CodingMPEG4);
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERSIZE);


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

OMX_ERRORTYPE NvxLiteMp4ExtDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    OMX_VIDEO_PARAM_MPEG4TYPE *pVideoParams;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxMp4ExtDecoderInit\n"));

    eError = NvxCommonVideoDecoderInit(hComponent, NvMMLiteBlockType_DecMPEG4, "BlockMP4Dec");
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
                     MAX_INPUT_BUFFERS, MAX_INPUT_BUFFERSIZE, OMX_VIDEO_CodingMPEG4);
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERSIZE);


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

OMX_ERRORTYPE NvxLiteH263DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    OMX_VIDEO_PARAM_H263TYPE *pVideoParams;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxH263DecoderInit\n"));

    eError = NvxCommonVideoDecoderInit(hComponent, NvMMLiteBlockType_DecMPEG4, "BlockMP4Dec");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxH263DecoderInit:"
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
                     MAX_INPUT_BUFFERS, MAX_INPUT_BUFFERSIZE,
                     OMX_VIDEO_CodingH263);
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_input],
                              MAX_INPUT_BUFFERSIZE);

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

OMX_ERRORTYPE NvxLiteH265DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    OMX_VIDEO_PARAM_AVCTYPE *pAvcParams;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxH265DecoderInit\n"));

    eError = NvxCommonVideoDecoderInit(hComponent, NvMMLiteBlockType_DecH265, "BlockH265Dec");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxH265DecoderInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ALLOC_STRUCT(pNvComp, pAvcParams, OMX_VIDEO_PARAM_AVCTYPE);
    if (!pAvcParams)
        return OMX_ErrorInsufficientResources;

    /* Register function pointers */
    pNvComp->pComponentName = "OMX.Nvidia.h265.decode";
    pNvComp->sComponentRoles[0] = "video_decoder.hevc";    // check the correct type
    pNvComp->nComponentRoles    = 1;

    // TO DO: set the buffer size appropriately
    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_input], OMX_DirInput,
                     MAX_INPUT_BUFFERS, MAX_INPUT_BUFFERSIZE, NVX_VIDEO_CodingHEVC);
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERSIZE);

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

OMX_ERRORTYPE NvxLiteH264DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    OMX_VIDEO_PARAM_AVCTYPE *pAvcParams;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxH264DecoderInit\n"));

    eError = NvxCommonVideoDecoderInit(hComponent, NvMMLiteBlockType_DecH264, "BlockH264Dec");
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
                     MAX_INPUT_BUFFERS, MAX_INPUT_BUFFERSIZE, OMX_VIDEO_CodingAVC);
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERSIZE);

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

OMX_ERRORTYPE NvxLiteH264DecoderSecureInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    SNvxLiteVideoDecoderData *pNvxVideoDecoder = NULL;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxH264DecoderSecureInit\n"));

    eError = NvxLiteH264DecoderInit(hComponent);
    if (OMX_ErrorNone == eError)
    {
        pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
        pNvxVideoDecoder = (SNvxLiteVideoDecoderData *)pNvComp->pComponentData;

        pNvComp->pComponentName = "OMX.Nvidia.h264.decode.secure";
        pNvComp->sComponentRoles[0] = "video_decoder.avc.secure";
        pNvComp->sComponentRoles[1] = "video_decoder.avc";
        pNvComp->nComponentRoles = 2;
        pNvxVideoDecoder->oBase.bSecure = OMX_TRUE;
#ifdef BUILD_GOOGLETV
        NvMMCryptoInit();
#endif
        pNvxVideoDecoder->bClearAsEncrypted = OMX_TRUE;
    }

    return eError;
}


OMX_ERRORTYPE NvxLiteH264DecoderLowLatencyInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    SNvxLiteVideoDecoderData *pNvxVideoDecoder = NULL;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxLiteH264DecoderLowLatencyInit\n"));

    eError = NvxLiteH264DecoderInit(hComponent);
    if (OMX_ErrorNone == eError)
    {
        pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
        pNvxVideoDecoder = (SNvxLiteVideoDecoderData *)pNvComp->pComponentData;

        pNvComp->pComponentName = "OMX.Nvidia.h264.decode.low.latency";
        NvOsDebugPrintf("NvxLiteH264DecoderLowLatencyInit set DPB and Mjstreaming");
        pNvxVideoDecoder->bH264DisableDPB = OMX_TRUE;
        pNvxVideoDecoder->bMjolnirStreaming = OMX_TRUE;
    }

    return eError;
}

OMX_ERRORTYPE NvxLiteH264ExtDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    OMX_VIDEO_PARAM_AVCTYPE *pAvcParams;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxH264DecoderInit\n"));

    eError = NvxCommonVideoDecoderInit(hComponent, NvMMLiteBlockType_DecH264, "BlockH264Dec");
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
                     MAX_INPUT_BUFFERS, MAX_INPUT_BUFFERSIZE, OMX_VIDEO_CodingAVC);
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERSIZE);

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

OMX_ERRORTYPE NvxLiteVc1DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    OMX_VIDEO_PARAM_WMVTYPE *pWmvParams;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxVc1DecoderInit\n"));

    eError = NvxCommonVideoDecoderInit(hComponent, NvMMLiteBlockType_DecVC1, "BlockVc1Dec");
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
                     MAX_INPUT_BUFFERS, MAX_INPUT_BUFFERSIZE, OMX_VIDEO_CodingWMV);
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERSIZE);

    pWmvParams->eFormat = OMX_VIDEO_WMVFormat9;
    pNvComp->pPorts[s_nvx_port_input].pPortPrivate = pWmvParams;
    return eError;
}

OMX_ERRORTYPE NvxLiteVC1DecoderSecureInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    SNvxLiteVideoDecoderData *pNvxVideoDecoder = NULL;
    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxLiteVC1DecoderSecureInit\n"));

    eError = NvxLiteVc1DecoderInit(hComponent);
    if (OMX_ErrorNone == eError)
    {
        pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
        pNvxVideoDecoder = (SNvxLiteVideoDecoderData *)pNvComp->pComponentData;

        pNvComp->pComponentName = "OMX.Nvidia.vc1.decode.secure";
        pNvComp->sComponentRoles[0] = "video_decoder.vc1.secure";
        pNvComp->sComponentRoles[1] = "video_decoder.vc1";
        pNvComp->nComponentRoles = 2;
        pNvxVideoDecoder->oBase.bSecure = OMX_TRUE;
#ifdef BUILD_GOOGLETV
        NvMMCryptoInit();
#endif
        pNvxVideoDecoder->bClearAsEncrypted = OMX_TRUE;
    }

    return eError;
}

OMX_ERRORTYPE NvxLiteVP8DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    OMX_VIDEO_PARAM_PORTFORMATTYPE *pVP8Params;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxLiteVP8DecoderInit\n"));

    if (NvMMVP8Support() != NvSuccess)
    {
        eError = OMX_ErrorComponentNotFound;
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxLiteVP8DecoderInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    eError = NvxCommonVideoDecoderInit(hComponent, NvMMLiteBlockType_DecVP8, "BlockVP8Dec");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxLiteVP8DecoderInit:"
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

OMX_ERRORTYPE NvxLiteMpeg2DecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    OMX_VIDEO_PARAM_MPEG2TYPE  *pMpeg2Params;
    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxMpeg2DecoderInit\n"));

    eError = NvxCommonVideoDecoderInit(hComponent, NvMMLiteBlockType_DecMPEG2, "BlockMpeg2Dec");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxMpeg2DecoderInit:"
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

    pMpeg2Params->nPortIndex=s_nvx_port_input;
    pMpeg2Params-> nPFrames=3;  //check
    pMpeg2Params-> nBFrames=2;  //check
    pMpeg2Params-> eProfile=OMX_VIDEO_MPEG2ProfileMain;
    pMpeg2Params-> eLevel=OMX_VIDEO_MPEG2LevelML;
    pNvComp->pPorts[s_nvx_port_input].pPortPrivate = pMpeg2Params;
    return eError;
}

OMX_ERRORTYPE NvxLiteMJpegDecoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxLiteMJpegDecoderInit\n"));

    eError = NvxCommonVideoDecoderInit(hComponent, NvMMLiteBlockType_DecMJPEG, "BlockMJpgDec");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxLiteMJpegDecoderInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    pNvComp->pComponentName = "OMX.Nvidia.mjpeg.decoder";
    pNvComp->sComponentRoles[0] = "video_decoder.mjpeg";    // check the correct type
    pNvComp->nComponentRoles    = 1;

    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_input], OMX_DirInput,
                     MAX_INPUT_BUFFERS, MAX_INPUT_BUFFERSIZE, OMX_VIDEO_CodingMJPEG);
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_input], MAX_INPUT_BUFFERSIZE);

    return eError;
}

