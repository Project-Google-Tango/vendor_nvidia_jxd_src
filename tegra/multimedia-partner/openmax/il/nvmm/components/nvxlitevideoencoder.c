/*
 * Copyright (c) 2006-2014 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** NvxVideoEncoder.c */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */
#include <OMX_Core.h>
#include "NvxTrace.h"
#include "NvxComponent.h"
#include "NvxPort.h"
#include "NvxCompReg.h"
#include "nvmmlitetransformbase.h"
#include "nvassert.h"
#include "nvmmlite_video.h"
#include "nvmm_videoenc.h"
#include "nvmm_util.h"

#if USE_ANDROID_NATIVE_WINDOW
#include "NvxHelpers.h"
#include "nvxliteandroidbuffer.h"
#include "nvgr.h"
#endif

#define USE_ANDROID_METADATA_BUFFERS (1)
#define ENABLE_ANDROID_INSERT_SPSPPS (1)

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */
#define MIN_INPUT_BUFFERS    2
#define MAX_INPUT_BUFFERS    6
#define MAX_OUTPUT_BUFFERS   6
#define MIN_INPUT_BUFFERSIZE 384 //50*1024
//#define MIN_OUTPUT_BUFFERSIZE (720*480*3/2)

#ifndef MAX
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#endif

#define s_GET_BUFFER_SIZE(width, height)    ((width) * (height) * 3 / 2)

/* Number of internal frame buffers (video memory) */
static const int s_nvx_num_ports                = 2;
static const unsigned int s_nvx_port_input      = 0;
static const unsigned int s_nvx_port_output     = 1;

/* nEncTuneFlags values */
typedef enum
{
    EncTune_USE_APPDefined_RATE_CONTROL     = 0x00000001,
    EncTune_USE_APPDefined_Quality_LEVEL    = 0x00000002,
    EncTune_USE_APPDefined_EncoderProperty  = 0x00000004,
    EncTune_USE_APPDefined_FRAME_SKIPPING   = 0x00000008
}eEncTuneFlags;

/* encoder state information */
typedef struct SNvxLiteVideoEncoderData
{
    OMX_BOOL bInitialized;
    OMX_BOOL bOverrideTimestamp;
    OMX_BOOL bErrorReportingEnabled;
    OMX_BOOL bRTPMode;

    SNvxNvMMLiteTransformData oBase;
    OMX_BOOL bFirstFrame;

#define TYPE_MPEG4 0
#define TYPE_H263  1
#define TYPE_H264  2
#define TYPE_VP8   3
    int oType;

    OMX_U32 nSurfWidth;
    OMX_U32 nSurfHeight;

    OMX_U32 nMinOutputBufferSize;

    OMX_U32 nBitRate;
    OMX_U32 nPeakBitrate;
    OMX_U32 nFrameRate;
    OMX_U32 nVirtualBufferSize;
    OMX_BOOL bStringentBitrate;
    OMX_BOOL bSvcEncodeEnable;
    OMX_BOOL bSetMaxEncClock;
    OMX_BOOL bFrameSkip;
    OMX_BOOL bAllIFrames;
    OMX_BOOL bBitBasedPacketization;
    OMX_BOOL bInsertSPSPPSAtIDR;
    OMX_BOOL bUseConstrainedBP;
    OMX_BOOL bInsertVUI;
    OMX_BOOL bInsertAUD;
    OMX_BOOL bEnableStuffing;
    OMX_BOOL bLowLatency;
    OMX_BOOL bSliceLevelEncode;
    OMX_BOOL bSuperFineQuality;
    OMX_BOOL bActiveSliceLevelEncode;
    OMX_BOOL bSkipFrame;

    NvMMVideoEncAppType eAppConfig;
    NvMMVideoEncQualityLevel eQualityLevel;
    NvMMVideoEncErrorResiliencyLevel eResilLevel;

    OMX_U32 TemporalTradeOffLevel;
    OMX_S32 RotateAngle;
    OMX_MIRRORTYPE Mirror;

    NvMMLiteBlockType oBlockType;
    const char *sBlockName;

    OMX_U32 nIFrameInterval;
    OMX_U32 nAVCIDRFrameInterval;
    OMX_CONFIG_RECTTYPE OutputCropRec;
    OMX_BOOL FlagCropRect;
    NvMMVideoEncRateCtrlMode eRateCtrlMode;
    OMX_U32 nSliceHeaderSpacing;
    OMX_U32 nInitQPI;
    OMX_U32 nInitQPP;
    OMX_U32 nInitQPB;   // NVENC onwards only
    OMX_U32 nMinQpI;
    OMX_U32 nMaxQpI;
    OMX_U32 nMinQpP;
    OMX_U32 nMaxQpP;
    OMX_U32 nMinQpB;    // NVENC onwards only
    OMX_U32 nMaxQpB;    // NVENC onwards only
    OMX_U32 eLevel;
    OMX_BOOL bTimeLapse;
    NvMMH264EncQualityParams H264EncQualityParams;
    OMX_STRING pTempFilePath;
    OMX_U32     nEncTuneFlags;
    OMX_BOOL    bSliceIntraRefreshEnable;
    OMX_U32     SliceIntraRefreshInterval;
    OMX_BOOL bCyclicIntraRefreshEnable;
    OMX_U32 Cirmbs;
} SNvxLiteVideoEncoderData;

extern void NvxNvMMLiteConvertProfilLevelToOmx(NvMMLiteVideoCodingType eType,
                                               OMX_U32 nNvmmLitePfl,
                                               OMX_U32 nNvmmLiteLvl,
                                               OMX_U32 *pOmxPfl,
                                               OMX_U32 *pOmxLvl);

static OMX_ERRORTYPE NvxVideoEncoderCommonstreamProp(SNvxNvMMLiteTransformData *pBase, SNvxLiteVideoEncoderData *pNvxVideoEncoder, NvU32 UpdateParams);

static void NvxParseEncoderCfg(FILE *fp, SNvxLiteVideoEncoderData *pNvxVideoEncoder);

#if !(USE_ANDROID_NATIVE_WINDOW)
static OMX_ERRORTYPE HandleStoreMetaDataInBuffersParamLite(void *pParam,
                                                SNvxNvMMLitePort *pPort,
                                                OMX_U32 PortAllowed);
#endif


static void s_SetActiveSliceLevelEncode(SNvxLiteVideoEncoderData *pNvxVideoEncoder)
{
    OMX_BOOL SliceLevelEncode = pNvxVideoEncoder->bActiveSliceLevelEncode;

    if (!pNvxVideoEncoder->bInitialized)
         return;

    if (pNvxVideoEncoder->bSliceLevelEncode)
    {
        pNvxVideoEncoder->oBase.hBlock->SetAttribute(pNvxVideoEncoder->oBase.hBlock,
                                                     NvMMAttributeVideoEnc_ActiveSliceLevelEncode,
                                                     0, sizeof(OMX_BOOL), &SliceLevelEncode);
    }
}

static void s_SetTemporalTradeOff(SNvxLiteVideoEncoderData *pNvxVideoEncoder)
{
    NvMMVideoEncTemporalTradeOff oVideoTemporalTradeOff;

    if (!pNvxVideoEncoder->bInitialized)
        return;

    //if (pNvxVideoEncoder->TemporalTradeOffLevel)
    {
        NvOsMemset(&oVideoTemporalTradeOff, 0, sizeof(NvMMVideoEncTemporalTradeOff));
        oVideoTemporalTradeOff.StructSize = sizeof(NvMMVideoEncTemporalTradeOff);
        oVideoTemporalTradeOff.VideoTemporalTradeOff = (NvMMVideoEncTemporalTradeoffLevel)(pNvxVideoEncoder->TemporalTradeOffLevel);
        pNvxVideoEncoder->oBase.hBlock->SetAttribute(pNvxVideoEncoder->oBase.hBlock,
                                                     NvMMAttributeVideoEnc_VideoTemporalTradeOff,
                                                     0,
                                                     sizeof(NvMMVideoEncTemporalTradeOff),
                                                     &oVideoTemporalTradeOff);
    }
}

static void s_SetInputRotation(SNvxLiteVideoEncoderData *pNvxVideoEncoder)
{
    NvMMVideoEncRotation oVideoRotateAngle;

    if (!pNvxVideoEncoder->bInitialized)
        return;

    {
        NvOsMemset(&oVideoRotateAngle, 0, sizeof(NvMMVideoEncRotation));
        oVideoRotateAngle.StructSize = sizeof(NvMMVideoEncRotation);
        switch(pNvxVideoEncoder->RotateAngle)
        {
        case 0:
            oVideoRotateAngle.Rotation = NvMMVideoEncRotation_None;
            break;
        case 90:
            oVideoRotateAngle.Rotation = NvMMVideoEncRotation_90;
            break;
        case 180:
            oVideoRotateAngle.Rotation = NvMMVideoEncRotation_180;
            break;
        case 270:
            oVideoRotateAngle.Rotation = NvMMVideoEncRotation_270;
            break;
        default:
            oVideoRotateAngle.Rotation = NvMMVideoEncRotation_None;
            break;
        }
        pNvxVideoEncoder->oBase.hBlock->SetAttribute(pNvxVideoEncoder->oBase.hBlock,
                                                     NvMMAttributeVideoEnc_Rotation,
                                                     0,
                                                     sizeof(NvMMVideoEncRotation),
                                                     &oVideoRotateAngle);
    }
}

static void s_SetInputMirroring(SNvxLiteVideoEncoderData *pNvxVideoEncoder)
{
    NvMMVideoEncInputMirroring oVideoMirror;

    if (!pNvxVideoEncoder->bInitialized)
        return;

    {
        NvOsMemset(&oVideoMirror, 0, sizeof(NvMMVideoEncInputMirroring));
        oVideoMirror.StructSize = sizeof(NvMMVideoEncInputMirroring);
        switch(pNvxVideoEncoder->Mirror)
        {
        case OMX_MirrorNone:
            oVideoMirror.Mirror = NvMMVideoEncMirror_None;
            break;
        case OMX_MirrorVertical:
            oVideoMirror.Mirror = NvMMVideoEncMirror_Vertical;
            break;
        case OMX_MirrorHorizontal:
            oVideoMirror.Mirror = NvMMVideoEncMirror_Horizontal;
            break;
        case OMX_MirrorBoth:
            oVideoMirror.Mirror = NvMMVideoEncMirror_Both;
            break;
        default:
            oVideoMirror.Mirror = NvMMVideoEncMirror_None;
            break;
        }
        pNvxVideoEncoder->oBase.hBlock->SetAttribute(pNvxVideoEncoder->oBase.hBlock,
                                                     NvMMAttributeVideoEnc_Mirroring,
                                                     0,
                                                     sizeof(NvMMVideoEncInputMirroring),
                                                     &oVideoMirror);
    }
}

static void s_SetH264OutputCrop(SNvxLiteVideoEncoderData* pNvxVideoEncoder)
{
    NvMMVideoEncH264CropRect H264CropRect;

    NvOsMemset(&H264CropRect, 0, sizeof(NvMMVideoEncH264CropRect));
    H264CropRect.StructSize = sizeof(NvMMVideoEncH264CropRect);
    H264CropRect.FlagCropRect = NV_TRUE;
    H264CropRect.Rect.top = pNvxVideoEncoder->OutputCropRec.nTop;
    H264CropRect.Rect.left = pNvxVideoEncoder->OutputCropRec.nLeft;
    H264CropRect.Rect.right = pNvxVideoEncoder->OutputCropRec.nWidth +
                                pNvxVideoEncoder->OutputCropRec.nLeft;
    H264CropRect.Rect.bottom = pNvxVideoEncoder->OutputCropRec.nHeight +
                                pNvxVideoEncoder->OutputCropRec.nTop;
    pNvxVideoEncoder->oBase.hBlock->SetAttribute(pNvxVideoEncoder->oBase.hBlock,
                                                 NvMMAttributeVideoEnc_H264CropRect,
                                                 0,
                                                 sizeof(NvMMVideoEncH264CropRect),
                                                 &H264CropRect);
}

static void s_SetNvMmVideoEncPacketMode(SNvxLiteVideoEncoderData* pNvxVideoEncoder)
{
    NvMMVideoEncPacket VideoPacketMode;

    NvOsMemset(&VideoPacketMode, 0, sizeof(NvMMVideoEncPacket));
    VideoPacketMode.StructSize = sizeof(NvMMVideoEncPacket);
    if (pNvxVideoEncoder->bBitBasedPacketization == NV_TRUE)
        VideoPacketMode.PacketType = NvMMVideoEncPacketType_BITS;
    else
        VideoPacketMode.PacketType = NvMMVideoEncPacketType_MBLK;
    VideoPacketMode.PacketLength = pNvxVideoEncoder->nSliceHeaderSpacing;

    pNvxVideoEncoder->oBase.hBlock->SetAttribute(pNvxVideoEncoder->oBase.hBlock,
                                         NvMMAttributeVideoEnc_Packet,
                                         0,
                                         sizeof(NvMMVideoEncPacket),
                                         &VideoPacketMode);
}

static void s_SetNvMmVideoEncInitialQuant(SNvxLiteVideoEncoderData* pNvxVideoEncoder)
{
    NvMMVideoEncInitialQP InitialQuant;

    NvOsMemset(&InitialQuant, 0, sizeof(NvMMVideoEncInitialQP));
    InitialQuant.StructSize = sizeof(NvMMVideoEncInitialQP);
    InitialQuant.InitialQPI = pNvxVideoEncoder->nInitQPI;
    InitialQuant.InitialQPP = pNvxVideoEncoder->nInitQPP;
    InitialQuant.InitialQPB = pNvxVideoEncoder->nInitQPB;   // NVENC onwards only

    pNvxVideoEncoder->oBase.hBlock->SetAttribute(pNvxVideoEncoder->oBase.hBlock,
                                         NvMMAttributeVideoEnc_InitialQP,
                                         0,
                                         sizeof(NvMMVideoEncInitialQP),
                                         &InitialQuant);
}

static void s_SetNvMmVideoEncQuantRange(SNvxLiteVideoEncoderData* pNvxVideoEncoder)
{
    NvMMVideoEncQPRange QuantRange;

    NvOsMemset(&QuantRange, 0, sizeof(NvMMVideoEncQPRange));
    QuantRange.StructSize = sizeof(NvMMVideoEncQPRange);
    QuantRange.IMinQP = pNvxVideoEncoder->nMinQpI;
    QuantRange.IMaxQP = pNvxVideoEncoder->nMaxQpI;
    QuantRange.PMinQP = pNvxVideoEncoder->nMinQpP;
    QuantRange.PMaxQP = pNvxVideoEncoder->nMaxQpP;
    QuantRange.BMinQP = pNvxVideoEncoder->nMinQpB;  // NVENC onwards only
    QuantRange.BMaxQP = pNvxVideoEncoder->nMaxQpB;  // NVENC onwards only

    pNvxVideoEncoder->oBase.hBlock->SetAttribute(pNvxVideoEncoder->oBase.hBlock,
                                         NvMMAttributeVideoEnc_QPRange,
                                         0,
                                         sizeof(NvMMVideoEncQPRange),
                                         &QuantRange);
}

static OMX_ERRORTYPE NvxVideoEncoderDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    SNvxLiteVideoEncoderData *pNvxVideoEncoder;

    pNvxVideoEncoder = (SNvxLiteVideoEncoderData *)pNvComp->pComponentData;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxLiteVideoEncoderInit\n"));

    if (pNvxVideoEncoder->oBase.hBlock)
    {
        eError = NvxNvMMLiteTransformClose(&pNvxVideoEncoder->oBase);

        if (eError != OMX_ErrorNone)
            return eError;
    }

    NvOsFree(pNvxVideoEncoder->pTempFilePath);
    pNvxVideoEncoder->pTempFilePath = NULL;
    NvOsFree(pNvComp->pComponentData);
    pNvComp->pComponentData = NULL;
    return eError;
}

static OMX_ERRORTYPE NvxVideoEncoderGetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    SNvxLiteVideoEncoderData *pNvxVideoEncoder;

    pNvxVideoEncoder = (SNvxLiteVideoEncoderData *)pComponent->pComponentData;

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxLiteVideoEncoderGetParameter\n"));

    NV_ASSERT(pNvxVideoEncoder);

    switch (nParamIndex)
    {
    case OMX_IndexParamVideoMpeg4:
        if (pNvxVideoEncoder->oType != TYPE_MPEG4)
            return OMX_ErrorBadPortIndex;

        NvOsMemcpy(pComponentParameterStructure,
                   pComponent->pPorts[s_nvx_port_input].pPortPrivate,
                   sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
        break;
    case OMX_IndexParamVideoAvc:
    {
        OMX_U32 eMaxProfile;
        OMX_U32 eMaxLevel;
        NvMMLiteVideoMaxCapabilities info;
        NvError err = OMX_ErrorNone;
        OMX_VIDEO_PARAM_AVCTYPE *pAvcParams;

        if (pNvxVideoEncoder->oType != TYPE_H264)
            return OMX_ErrorBadPortIndex;

        info.CapIndex = 0;
        info.eType = NvMMLiteVideo_CodingTypeH264;

        if (!pNvxVideoEncoder->oBase.hBlock)
        {
           err = NvxNvMMLiteTransformOpen(&pNvxVideoEncoder->oBase,
                                          pNvxVideoEncoder->oBlockType,
                                          pNvxVideoEncoder->sBlockName,
                                          s_nvx_num_ports,
                                          pComponent);
           if (err != OMX_ErrorNone)
               return err;
        }

        err = pNvxVideoEncoder->oBase.hBlock->GetAttribute (pNvxVideoEncoder->oBase.hBlock,
                                                            NvMMAttributeVideoEnc_MaxCapabilities,
                                                            sizeof(NvMMLiteVideoMaxCapabilities),
                                                            &info);

        if (NvSuccess != err)
            return OMX_ErrorNoMore;

        NvxNvMMLiteConvertProfilLevelToOmx(info.eType,
                                           info.MaxProfile,
                                           info.MaxLevel,
                                           &eMaxProfile,
                                           &eMaxLevel);

        pAvcParams =(OMX_VIDEO_PARAM_AVCTYPE *)pComponent->pPorts[s_nvx_port_input].pPortPrivate;
        pAvcParams->eProfile = eMaxProfile;
        pAvcParams->eLevel = eMaxLevel;

        if (pAvcParams->eProfile != OMX_VIDEO_AVCProfileBaseline)
            pAvcParams->bEntropyCodingCABAC = OMX_TRUE;

        NvOsMemcpy(pComponentParameterStructure,
                   pComponent->pPorts[s_nvx_port_input].pPortPrivate,
                   sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        break;
    }
    case OMX_IndexParamVideoH263:
        if (pNvxVideoEncoder->oType != TYPE_H263)
            return OMX_ErrorBadPortIndex;

        NvOsMemcpy(pComponentParameterStructure,
                   pComponent->pPorts[s_nvx_port_input].pPortPrivate,
                   sizeof(OMX_VIDEO_PARAM_H263TYPE));
        break;
    case NVX_IndexParamVideoVP8:
        if (pNvxVideoEncoder->oType != TYPE_VP8)
            return OMX_ErrorBadPortIndex;

        NvOsMemcpy(pComponentParameterStructure,
                   pComponent->pPorts[s_nvx_port_input].pPortPrivate,
                   sizeof(NVX_VIDEO_PARAM_VP8TYPE));
        break;
    case OMX_IndexParamVideoProfileLevelQuerySupported:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE * pProfLvl =
            (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
        NvMMLiteVideoProfileLevelInfo info;
        NvError err = OMX_ErrorNone;

        info.ProfileIndex = pProfLvl->nProfileIndex;

        if (pNvxVideoEncoder->oType == TYPE_H264)
        {
            info.eType = NvMMLiteVideo_CodingTypeH264;
        }
        else if (pNvxVideoEncoder->oType == TYPE_MPEG4)
        {
            info.eType = NvMMLiteVideo_CodingTypeMPEG4;
        }
        else if (pNvxVideoEncoder->oType == TYPE_H263)
        {
            info.eType = NvMMLiteVideo_CodingTypeH263;
        }

        if (!pNvxVideoEncoder->oBase.hBlock)
        {
           err = NvxNvMMLiteTransformOpen(&pNvxVideoEncoder->oBase,
                                          pNvxVideoEncoder->oBlockType,
                                          pNvxVideoEncoder->sBlockName,
                                          s_nvx_num_ports,
                                          pComponent);
           if (err != OMX_ErrorNone)
               return err;
        }

        err = pNvxVideoEncoder->oBase.hBlock->GetAttribute(pNvxVideoEncoder->oBase.hBlock,
                                                           NvMMAttributeVideoEnc_ProfileLevelQuery,
                                                           sizeof(NvMMLiteVideoProfileLevelInfo),
                                                           &info);

        if (NvSuccess != err)
            return OMX_ErrorNoMore;

        NvxNvMMLiteConvertProfilLevelToOmx(info.eType,
                                           info.Profile,
                                           info.Level,
                                           &pProfLvl->eProfile,
                                           &pProfLvl->eLevel);
        break;
    }
    case NVX_IndexParamCodecCapability:
    {
        NVX_PARAM_CODECCAPABILITY * pCap = (NVX_PARAM_CODECCAPABILITY *)pComponentParameterStructure;
        NvMMLiteVideoMaxCapabilities info;
        NvError err = OMX_ErrorNone;

        info.CapIndex = pCap->nCapIndex;

        if (!pNvxVideoEncoder->oBase.hBlock)
        {
           err = NvxNvMMLiteTransformOpen(&pNvxVideoEncoder->oBase,
                                          pNvxVideoEncoder->oBlockType,
                                          pNvxVideoEncoder->sBlockName,
                                          s_nvx_num_ports,
                                          pComponent);
           if (err != OMX_ErrorNone)
               return err;
        }

        if (pNvxVideoEncoder->oType == TYPE_H264)
        {
            info.eType = NvMMLiteVideo_CodingTypeH264;
        }
        else if (pNvxVideoEncoder->oType == TYPE_MPEG4)
        {
            info.eType = NvMMLiteVideo_CodingTypeMPEG4;
        }
        else if (pNvxVideoEncoder->oType == TYPE_H263)
        {
            info.eType = NvMMLiteVideo_CodingTypeH263;
        }
        else if (pNvxVideoEncoder->oType == TYPE_VP8)
        {
            info.eType = NvMMLiteVideo_CodingTypeVP8;
        }

        err = pNvxVideoEncoder->oBase.hBlock->GetAttribute (pNvxVideoEncoder->oBase.hBlock,
                                                            NvMMAttributeVideoEnc_MaxCapabilities,
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
    case OMX_IndexParamVideoPortFormat:
    {
        OMX_VIDEO_PARAM_PORTFORMATTYPE * portFormat =
            (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;

        if((NvU32)s_nvx_port_input == portFormat->nPortIndex)
        {
            if(portFormat->nIndex > 1)
            return OMX_ErrorNoMore;

            if (portFormat->nIndex == 0) {
                portFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                portFormat->eColorFormat = OMX_COLOR_FormatYUV420Planar;
            } else {
                portFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                portFormat->eColorFormat = OMX_COLOR_FormatAndroidOpaque;
            }
        }
        else if((NvU32)s_nvx_port_output == portFormat->nPortIndex)
        {
            if(portFormat->nIndex > 0)
                return OMX_ErrorNoMore;

            portFormat->eColorFormat = OMX_COLOR_FormatUnused;
            portFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
            if (pNvxVideoEncoder->oType == TYPE_H264)
                portFormat->eCompressionFormat = OMX_VIDEO_CodingAVC;
            else if (pNvxVideoEncoder->oType == TYPE_MPEG4)
                portFormat->eCompressionFormat = OMX_VIDEO_CodingMPEG4;
            else if (pNvxVideoEncoder->oType == TYPE_H263)
                portFormat->eCompressionFormat = OMX_VIDEO_CodingH263;
            else if (pNvxVideoEncoder->oType == TYPE_VP8)
                portFormat->eCompressionFormat = NVX_VIDEO_CodingVP8;
        }
    }
    break;
    case OMX_IndexParamVideoMacroblocksPerFrame:
    {
        OMX_PARAM_MACROBLOCKSTYPE* pMacroblocksType = (OMX_PARAM_MACROBLOCKSTYPE *)pComponentParameterStructure;
        if (pMacroblocksType->nPortIndex != (NvU32)s_nvx_port_input)
            return OMX_ErrorBadPortIndex;
        break;
    }
    case OMX_IndexParamQFactor:
        {
            OMX_IMAGE_PARAM_QFACTORTYPE *pQFactor =
                (OMX_IMAGE_PARAM_QFACTORTYPE *)pComponentParameterStructure;

            // round up to q-level. [1, 100]
            if ( pNvxVideoEncoder->eQualityLevel == NvMMVideoEncQualityLevel_High )
                pQFactor->nQFactor = 100;
            if ( pNvxVideoEncoder->eQualityLevel == NvMMVideoEncQualityLevel_Medium )
                pQFactor->nQFactor = 66;
            else pQFactor->nQFactor = 33;
        }
        break;

    case OMX_IndexParamPortDefinition:
        pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
        {
            OMX_ERRORTYPE eError = NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
            if((NvU32)s_nvx_port_output == pPortDef->nPortIndex)
            {
                if (pNvxVideoEncoder->oBase.oPorts[s_nvx_port_output].bHasSize)
                {
                    pPortDef->format.video.nFrameWidth = pNvxVideoEncoder->oBase.oPorts[s_nvx_port_output].nWidth;
                    pPortDef->format.video.nFrameHeight = pNvxVideoEncoder->oBase.oPorts[s_nvx_port_output].nHeight;
                    pPortDef->format.video.nStride = pNvxVideoEncoder->oBase.oPorts[s_nvx_port_output].nWidth;
                    if(pNvxVideoEncoder->RotateAngle == 90 || pNvxVideoEncoder->RotateAngle == 270)
                    {
                        pPortDef->format.video.nFrameWidth = pNvxVideoEncoder->oBase.oPorts[s_nvx_port_output].nHeight;
                        pPortDef->format.video.nFrameHeight = pNvxVideoEncoder->oBase.oPorts[s_nvx_port_output].nWidth;
                        pPortDef->format.video.nStride = pNvxVideoEncoder->oBase.oPorts[s_nvx_port_output].nHeight;
                    }
                    pPortDef->format.video.nBitrate = pNvxVideoEncoder->nBitRate;
                    pPortDef->format.video.xFramerate = pNvxVideoEncoder->nFrameRate<<16;
                }
            }
            else
            {
                return eError;
            }
        }
        break;

    case NVX_IndexParamVideoEncodeProperty:
        {
            NVX_PARAM_VIDENCPROPERTY * pEncProp =
                (NVX_PARAM_VIDENCPROPERTY *)pComponentParameterStructure;

            switch ( pNvxVideoEncoder->eAppConfig )
            {
                case NvMMVideoEncAppType_Camcorder:
                    pEncProp->eApplicationType = NVX_VIDEO_Application_Camcorder;
                    break;
                case NvMMVideoEncAppType_VideoTelephony:
                    pEncProp->eApplicationType = NVX_VIDEO_Application_VideoTelephony;
                    break;
                default:
                    pEncProp->eApplicationType = NVX_VIDEO_Application_Camcorder;
                    break;
            }

            switch ( pNvxVideoEncoder->eResilLevel )
            {
                case NvMMVideoEncErrorResiliencyLevel_None:
                    pEncProp->eErrorResiliencyLevel = NVX_VIDEO_ErrorResiliency_None;
                    break;
                case NvMMVideoEncErrorResiliencyLevel_Low:
                    pEncProp->eErrorResiliencyLevel = NVX_VIDEO_ErrorResiliency_Low;
                    break;
                case NvMMVideoEncErrorResiliencyLevel_High:
                    pEncProp->eErrorResiliencyLevel = NVX_VIDEO_ErrorResiliency_High;
                    break;
                default:
                    pEncProp->eErrorResiliencyLevel = NVX_VIDEO_ErrorResiliency_None;
                    break;
            }
            pEncProp->bSvcEncodeEnable = pNvxVideoEncoder->bSvcEncodeEnable;
            pEncProp->bSetMaxEncClock = pNvxVideoEncoder->bSetMaxEncClock;
            pEncProp->bFrameSkip = pNvxVideoEncoder->bFrameSkip;
            pEncProp->bAllIFrames = pNvxVideoEncoder->bAllIFrames;
            pEncProp->bBitBasedPacketization = pNvxVideoEncoder->bBitBasedPacketization;
            pEncProp->bInsertSPSPPSAtIDR = pNvxVideoEncoder->bInsertSPSPPSAtIDR;
            pEncProp->bUseConstrainedBP = pNvxVideoEncoder->bUseConstrainedBP;
            pEncProp->bInsertVUI = pNvxVideoEncoder->bInsertVUI;
            pEncProp->bInsertAUD = pNvxVideoEncoder->bInsertAUD;
            pEncProp->bEnableStuffing = pNvxVideoEncoder->bEnableStuffing;
            pEncProp->nPeakBitrate = pNvxVideoEncoder->nPeakBitrate;
            pEncProp->bLowLatency = pNvxVideoEncoder->bLowLatency;
            pEncProp->bSliceLevelEncode = pNvxVideoEncoder->bSliceLevelEncode;
            pEncProp->bSliceIntraRefreshEnable = pNvxVideoEncoder->bSliceIntraRefreshEnable;
            pEncProp->SliceIntraRefreshInterval = pNvxVideoEncoder->SliceIntraRefreshInterval;
            pEncProp->nVirtualBufferSize = pNvxVideoEncoder->nVirtualBufferSize;
        }
        break;

    case OMX_IndexParamVideoBitrate:
        {
            OMX_VIDEO_PARAM_BITRATETYPE *pBitrate =
                (OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
            pBitrate->nTargetBitrate = pNvxVideoEncoder->nBitRate;
            switch(pNvxVideoEncoder->eRateCtrlMode)
            {
                case NvMMVideoEncRateCtrlMode_VBR:
                case NvMMVideoEncRateCtrlMode_VBR2:
                    if (pNvxVideoEncoder->bFrameSkip)
                        pBitrate->eControlRate = OMX_Video_ControlRateVariableSkipFrames;
                    else
                        pBitrate->eControlRate = OMX_Video_ControlRateVariable;
                    break;
                default:
                    if (pNvxVideoEncoder->bFrameSkip)
                        pBitrate->eControlRate = OMX_Video_ControlRateConstantSkipFrames;
                    else
                        pBitrate->eControlRate = OMX_Video_ControlRateConstant;
            }
        }
        break;

    case NVX_IndexParamVideoEncodeStringentBitrate:
        {
            OMX_CONFIG_BOOLEANTYPE *pStringentBitrate =
                (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;
            pStringentBitrate->bEnabled = pNvxVideoEncoder->bStringentBitrate;
        }
        break;
    case NVX_IndexParamRateControlMode:
        {
            NVX_PARAM_RATECONTROLMODE *pRateControlMode =
                (NVX_PARAM_RATECONTROLMODE *)pComponentParameterStructure;;
            switch(pNvxVideoEncoder->eRateCtrlMode)
            {
                case NvMMVideoEncRateCtrlMode_VBR:
                    pRateControlMode->eRateCtrlMode = NVX_VIDEO_RateControlMode_VBR;
                    break;
                case NvMMVideoEncRateCtrlMode_VBR2:
                    pRateControlMode->eRateCtrlMode = NVX_VIDEO_RateControlMode_VBR2;
                    break;

                default:
                    pRateControlMode->eRateCtrlMode = NVX_VIDEO_RateControlMode_CBR;
            }
            break;
        }
    case OMX_IndexParamVideoQuantization:
        {
            OMX_VIDEO_PARAM_QUANTIZATIONTYPE* pQuantType =
                (OMX_VIDEO_PARAM_QUANTIZATIONTYPE *)pComponentParameterStructure;

            pQuantType->nQpI = pNvxVideoEncoder->nInitQPI;
            pQuantType->nQpP = pNvxVideoEncoder->nInitQPP;
            pQuantType->nQpB = pNvxVideoEncoder->nInitQPB;  // NVENC onwards only
        }
        break;

    case OMX_IndexParamVideoErrorCorrection:
        {
            return OMX_ErrorNotImplemented;
        }
        break;

    case NVX_IndexParamVideoEncodeH264QualityParams:
        {
            NVX_PARAM_VIDENC_H264_QUALITY_PARAMS* pH264QualityParams =
                (NVX_PARAM_VIDENC_H264_QUALITY_PARAMS *)pComponentParameterStructure;
            if(pH264QualityParams->nSize != sizeof(NVX_PARAM_VIDENC_H264_QUALITY_PARAMS))
                return OMX_ErrorBadPortIndex;
            pH264QualityParams->nFavorIntraBias = pNvxVideoEncoder->H264EncQualityParams.FavorIntraBias;
            pH264QualityParams->nFavorInterBias = pNvxVideoEncoder->H264EncQualityParams.FavorInterBias;
            pH264QualityParams->nFavorIntraBias_16X16 = pNvxVideoEncoder->H264EncQualityParams.FavorIntraBias_16X16;
        }
        break;
    case OMX_IndexParamVideoIntraRefresh:
    {
       OMX_VIDEO_PARAM_INTRAREFRESHTYPE *pInraRefresh = (OMX_VIDEO_PARAM_INTRAREFRESHTYPE *)pComponentParameterStructure;
       if(pInraRefresh->eRefreshMode == OMX_VIDEO_IntraRefreshCyclic)
       {
           pInraRefresh->nCirMBs = pNvxVideoEncoder->Cirmbs;
       }
       else
           return OMX_ErrorNotImplemented;
       break;
    }

    default:
        return NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
    };

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxVideoEncoderSetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentParameterStructure)
{
    SNvxLiteVideoEncoderData *pNvxVideoEncoder;
    pNvxVideoEncoder = (SNvxLiteVideoEncoderData *)pComponent->pComponentData;

    NV_ASSERT(pNvxVideoEncoder);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxLiteVideoEncoderSetParameter\n"));

    switch(nIndex)
    {
    case OMX_IndexParamVideoMpeg4:
        if (pNvxVideoEncoder->oType != TYPE_MPEG4)
            return OMX_ErrorBadPortIndex;

        NvOsMemcpy(pComponent->pPorts[s_nvx_port_input].pPortPrivate, pComponentParameterStructure,
               sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
        pNvxVideoEncoder->nSliceHeaderSpacing =
            ((OMX_VIDEO_PARAM_MPEG4TYPE *)pComponentParameterStructure)->nSliceHeaderSpacing;

        if (((OMX_VIDEO_PARAM_MPEG4TYPE *)pComponentParameterStructure)->nPFrames)
            pNvxVideoEncoder->nIFrameInterval =
                ((OMX_VIDEO_PARAM_MPEG4TYPE *)pComponentParameterStructure)->nPFrames + 1;

        break;

    case OMX_IndexParamVideoAvc:
        if (pNvxVideoEncoder->oType != TYPE_H264)
            return OMX_ErrorBadPortIndex;

        NvOsMemcpy(pComponent->pPorts[s_nvx_port_input].pPortPrivate, pComponentParameterStructure,
               sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        pNvxVideoEncoder->nSliceHeaderSpacing =
            ((OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure)->nSliceHeaderSpacing;

        if (((OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure)->nPFrames)
            pNvxVideoEncoder->nIFrameInterval = pNvxVideoEncoder->nAVCIDRFrameInterval =
                ((OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure)->nPFrames + 1;

        break;

    case OMX_IndexParamVideoH263:
        if (pNvxVideoEncoder->oType != TYPE_H263)
            return OMX_ErrorBadPortIndex;

        NvOsMemcpy(pComponent->pPorts[s_nvx_port_input].pPortPrivate, pComponentParameterStructure,
               sizeof(OMX_VIDEO_PARAM_H263TYPE));
        if (((OMX_VIDEO_PARAM_H263TYPE *)pComponentParameterStructure)->nPFrames)
            pNvxVideoEncoder->nIFrameInterval =
                ((OMX_VIDEO_PARAM_H263TYPE *)pComponentParameterStructure)->nPFrames + 1;
        break;

    case NVX_IndexParamVideoVP8:
        if (pNvxVideoEncoder->oType != TYPE_VP8)
            return OMX_ErrorBadPortIndex;

        NvOsMemcpy(pComponent->pPorts[s_nvx_port_input].pPortPrivate, pComponentParameterStructure,
               sizeof(NVX_VIDEO_PARAM_VP8TYPE));
        if (((NVX_VIDEO_PARAM_VP8TYPE *)pComponentParameterStructure)->nPFrames)
            pNvxVideoEncoder->nIFrameInterval =
                ((NVX_VIDEO_PARAM_VP8TYPE *)pComponentParameterStructure)->nPFrames + 1;
        break;

    case OMX_IndexParamVideoBitrate:
        {
            OMX_VIDEO_PARAM_BITRATETYPE *pBitrate =
                (OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
            pNvxVideoEncoder->nBitRate = pBitrate->nTargetBitrate;
            switch (pBitrate->eControlRate)
            {
                case OMX_Video_ControlRateVariable:
                    pNvxVideoEncoder->nEncTuneFlags |= EncTune_USE_APPDefined_RATE_CONTROL;
                    pNvxVideoEncoder->nEncTuneFlags |= EncTune_USE_APPDefined_FRAME_SKIPPING;
                    pNvxVideoEncoder->eRateCtrlMode = NvMMVideoEncRateCtrlMode_VBR2;
                    pNvxVideoEncoder->bFrameSkip = NV_FALSE;
                    break;
                case OMX_Video_ControlRateConstant:
                    pNvxVideoEncoder->nEncTuneFlags |= EncTune_USE_APPDefined_RATE_CONTROL;
                    pNvxVideoEncoder->nEncTuneFlags |= EncTune_USE_APPDefined_FRAME_SKIPPING;
                    pNvxVideoEncoder->eRateCtrlMode = NvMMVideoEncRateCtrlMode_CBR;
                    pNvxVideoEncoder->bFrameSkip = NV_FALSE;
                    break;
                case OMX_Video_ControlRateVariableSkipFrames:
                    pNvxVideoEncoder->nEncTuneFlags |= EncTune_USE_APPDefined_RATE_CONTROL;
                    pNvxVideoEncoder->nEncTuneFlags |= EncTune_USE_APPDefined_FRAME_SKIPPING;
                    pNvxVideoEncoder->eRateCtrlMode = NvMMVideoEncRateCtrlMode_VBR2;
                    pNvxVideoEncoder->bFrameSkip = NV_TRUE;
                    break;
                case OMX_Video_ControlRateConstantSkipFrames:
                    pNvxVideoEncoder->nEncTuneFlags |= EncTune_USE_APPDefined_RATE_CONTROL;
                    pNvxVideoEncoder->nEncTuneFlags |= EncTune_USE_APPDefined_FRAME_SKIPPING;
                    pNvxVideoEncoder->eRateCtrlMode = NvMMVideoEncRateCtrlMode_CBR;
                    pNvxVideoEncoder->bFrameSkip = NV_TRUE;
                    break;
                default:
                    // we can use enctune config of valid rate control mode is not set.
                    pNvxVideoEncoder->eRateCtrlMode = NvMMVideoEncRateCtrlMode_VBR2;
                    pNvxVideoEncoder->bFrameSkip = NV_FALSE;
            }
        }
        break;

    case OMX_IndexParamQFactor:
        {
            OMX_IMAGE_PARAM_QFACTORTYPE *pQFactor =
                (OMX_IMAGE_PARAM_QFACTORTYPE *)pComponentParameterStructure;

            pNvxVideoEncoder->nEncTuneFlags |= EncTune_USE_APPDefined_Quality_LEVEL;
            // OMX specifies nQFactor [1, 100], 100 being the best quality
            if ( pQFactor->nQFactor > 66 )
                pNvxVideoEncoder->eQualityLevel = NvMMVideoEncQualityLevel_High;
            else if ( pQFactor->nQFactor > 33 )
                pNvxVideoEncoder->eQualityLevel = NvMMVideoEncQualityLevel_Medium;
            else
                pNvxVideoEncoder->eQualityLevel = NvMMVideoEncQualityLevel_Low;
        }
        break;

    case NVX_IndexParamVideoEncodeProperty:
        {
            NVX_PARAM_VIDENCPROPERTY * pEncProp =
                (NVX_PARAM_VIDENCPROPERTY *)pComponentParameterStructure;

            switch ( pEncProp->eApplicationType )
            {
                case NVX_VIDEO_Application_Camcorder:
                    pNvxVideoEncoder->eAppConfig = NvMMVideoEncAppType_Camcorder;
                    break;
                case NVX_VIDEO_Application_VideoTelephony:
                    pNvxVideoEncoder->eAppConfig = NvMMVideoEncAppType_VideoTelephony;
                    break;
                default:
                    pNvxVideoEncoder->eAppConfig = NvMMVideoEncAppType_Camcorder;
                    break;
            }

            switch ( pEncProp->eErrorResiliencyLevel )
            {
                case NVX_VIDEO_ErrorResiliency_None:
                    pNvxVideoEncoder->eResilLevel = NvMMVideoEncErrorResiliencyLevel_None;
                    break;
                case NVX_VIDEO_ErrorResiliency_Low:
                    pNvxVideoEncoder->eResilLevel = NvMMVideoEncErrorResiliencyLevel_Low;
                    break;
                case NVX_VIDEO_ErrorResiliency_High:
                    pNvxVideoEncoder->eResilLevel = NvMMVideoEncErrorResiliencyLevel_High;
                    break;
                default:
                    pNvxVideoEncoder->eResilLevel = NvMMVideoEncErrorResiliencyLevel_None;
                    break;
            }
            pNvxVideoEncoder->bSvcEncodeEnable = pEncProp->bSvcEncodeEnable;
            pNvxVideoEncoder->bSetMaxEncClock = pEncProp->bSetMaxEncClock;
            pNvxVideoEncoder->bFrameSkip = pEncProp->bFrameSkip;
            pNvxVideoEncoder->bAllIFrames = pEncProp->bAllIFrames;
            pNvxVideoEncoder->bBitBasedPacketization = pEncProp->bBitBasedPacketization;
            pNvxVideoEncoder->bInsertSPSPPSAtIDR = pEncProp->bInsertSPSPPSAtIDR;
            pNvxVideoEncoder->bUseConstrainedBP = pEncProp->bUseConstrainedBP;
            pNvxVideoEncoder->bInsertVUI = pEncProp->bInsertVUI;
            pNvxVideoEncoder->bInsertAUD = pEncProp->bInsertAUD;
            pNvxVideoEncoder->bEnableStuffing = pEncProp->bEnableStuffing;
            pNvxVideoEncoder->nPeakBitrate = pEncProp->nPeakBitrate;
            pNvxVideoEncoder->bLowLatency = pEncProp->bLowLatency;
            pNvxVideoEncoder->bSliceLevelEncode = pEncProp->bSliceLevelEncode;
            pNvxVideoEncoder->bSliceIntraRefreshEnable = pEncProp->bSliceIntraRefreshEnable;
            pNvxVideoEncoder->SliceIntraRefreshInterval = pEncProp->SliceIntraRefreshInterval;
            pNvxVideoEncoder->nVirtualBufferSize = pEncProp->nVirtualBufferSize;
            pNvxVideoEncoder->nEncTuneFlags |= EncTune_USE_APPDefined_EncoderProperty;
        }
        break;

    case OMX_IndexParamPortDefinition:
        {
            OMX_PARAM_PORTDEFINITIONTYPE *pPortDef =
               (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
            NvU32 InputBufferSize = 0;
            OMX_ERRORTYPE err;

            if(pNvxVideoEncoder->oType == TYPE_H264)
            {
                OMX_U32 nActualWidth = 0, nActualHeight = 0;
                nActualWidth = pPortDef->format.video.nFrameWidth;
                nActualHeight = pPortDef->format.video.nFrameHeight;
                pNvxVideoEncoder->nSurfWidth = nActualWidth;
                pNvxVideoEncoder->nSurfHeight = nActualHeight;

                if ((nActualWidth != pNvxVideoEncoder->nSurfWidth) ||
                    (nActualHeight != pNvxVideoEncoder->nSurfHeight))
                {
                    pNvxVideoEncoder->OutputCropRec.nLeft = 0;
                    pNvxVideoEncoder->OutputCropRec.nTop = 0;
                    pNvxVideoEncoder->OutputCropRec.nWidth = nActualWidth;
                    pNvxVideoEncoder->OutputCropRec.nHeight = nActualHeight;
                    pNvxVideoEncoder->FlagCropRect = NV_TRUE;
                }
            }
            else
            {
                pNvxVideoEncoder->nSurfWidth = pPortDef->format.video.nFrameWidth;
                pNvxVideoEncoder->nSurfHeight = pPortDef->format.video.nFrameHeight;
            }

            pNvxVideoEncoder->oBase.oPorts[s_nvx_port_output].nWidth = pPortDef->format.video.nFrameWidth;
            pNvxVideoEncoder->oBase.oPorts[s_nvx_port_output].nHeight = pPortDef->format.video.nFrameHeight;
            pNvxVideoEncoder->oBase.oPorts[s_nvx_port_output].bHasSize = OMX_TRUE;
            pNvxVideoEncoder->oBase.oPorts[s_nvx_port_input].nWidth = pNvxVideoEncoder->nSurfWidth;
            pNvxVideoEncoder->oBase.oPorts[s_nvx_port_input].nHeight = pNvxVideoEncoder->nSurfHeight;
            pNvxVideoEncoder->oBase.oPorts[s_nvx_port_input].bHasSize = OMX_TRUE;
            pNvxVideoEncoder->nMinOutputBufferSize = s_GET_BUFFER_SIZE(pNvxVideoEncoder->nSurfWidth, pNvxVideoEncoder->nSurfHeight);
            if (pPortDef->nBufferSize < pComponent->pPorts[s_nvx_port_output].nMinBufferSize)
            {
                pComponent->pPorts[s_nvx_port_output].nReqBufferSize =
                    pComponent->pPorts[s_nvx_port_output].nMinBufferSize = pNvxVideoEncoder->nMinOutputBufferSize;
            }
            pComponent->pPorts[s_nvx_port_output].oPortDef.nBufferSize = pNvxVideoEncoder->nMinOutputBufferSize;

            err = NvxComponentBaseSetParameter(pComponent, nIndex, pComponentParameterStructure);

            InputBufferSize = s_GET_BUFFER_SIZE(pNvxVideoEncoder->oBase.oPorts[s_nvx_port_input].nWidth,
                pNvxVideoEncoder->oBase.oPorts[s_nvx_port_input].nHeight);

            if (InputBufferSize < pComponent->pPorts[s_nvx_port_input].nMinBufferSize)
            {
                pComponent->pPorts[s_nvx_port_input].nMinBufferSize = InputBufferSize;
                pComponent->pPorts[s_nvx_port_input].nReqBufferSize = InputBufferSize;
            }
            if (InputBufferSize > pComponent->pPorts[s_nvx_port_input].oPortDef.nBufferSize)
            {
                pComponent->pPorts[s_nvx_port_input].oPortDef.nBufferSize = InputBufferSize;
            }
            // Update the framerate passed by stagefright.
            if((NvU32)s_nvx_port_input == pPortDef->nPortIndex){
                pNvxVideoEncoder->nFrameRate =
                    (OMX_U32)((pPortDef->format.video.xFramerate + ((1<<16) - 1)) >> 16);
		NvOsDebugPrintf("Framerate set to : %d at %s", pNvxVideoEncoder->nFrameRate,
			__FUNCTION__);
            }

            return err;
        }

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

    case NVX_IndexParamVideoEncodeStringentBitrate:
        {
            OMX_CONFIG_BOOLEANTYPE *pStringentBitrate =
                (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;
            pNvxVideoEncoder->bStringentBitrate = pStringentBitrate->bEnabled;
        }
        break;
    case NVX_IndexConfigUseNvBuffer:
        {
            // used by camera DZ, in order embed nvmmbuffers in OMX buffer.
            NVX_PARAM_USENVBUFFER *pParam = (NVX_PARAM_USENVBUFFER *)pComponentParameterStructure;
            pNvxVideoEncoder->oBase.oPorts[s_nvx_port_input].bEmbedNvMMBuffer = pParam->bUseNvBuffer;
        }
        break;
    case NVX_IndexParamEmbedRmSurface:
        {
            OMX_CONFIG_BOOLEANTYPE *param = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;
            if (param)
                pNvxVideoEncoder->oBase.bEmbedRmSurface = param->bEnabled;
        }
        break;

#if USE_ANDROID_METADATA_BUFFERS
    case NVX_IndexParamStoreMetaDataBuffer:
        {
#if USE_ANDROID_NATIVE_WINDOW
            // XXX - see bug 1404155
            //
            // This size is a union of all MetaDataBuffer formats
            // including KMetadataVideoEditorBufferTypeGrallocSource
            // which copies an opaque native handle into the buffer.
            // Unfortunately it copies a fixed number of bytes which
            // may exceed the size of the data structure, leading to
            // unpredictable results including program termination.
            // In addition, the NvRmMemHandle is for a different
            // process and may be invalid. Support for this format
            // needs to go away ASAP.
            pComponent->pPorts[s_nvx_port_input].oPortDef.nBufferSize =
                MAX(sizeof(OMX_BUFFERHEADERTYPE), nvgr_get_handle_size()) + 4 + 8; // (buffertype + proprietarydata)
#else
            pComponent->pPorts[s_nvx_port_input].oPortDef.nBufferSize = sizeof(OMX_BUFFERHEADERTYPE) + 4;
#endif
            return HandleStoreMetaDataInBuffersParamLite(
                       pComponentParameterStructure,
                       pNvxVideoEncoder->oBase.oPorts,
                       s_nvx_port_input);
        }
        break;
#endif

    case NVX_IndexParamRateControlMode:
        {
            NVX_PARAM_RATECONTROLMODE *pRateControlMode =
                (NVX_PARAM_RATECONTROLMODE *)pComponentParameterStructure;

            pNvxVideoEncoder->nEncTuneFlags |= EncTune_USE_APPDefined_RATE_CONTROL;
            switch(pRateControlMode->eRateCtrlMode)
            {
                case NVX_VIDEO_RateControlMode_VBR:
                    pNvxVideoEncoder->eRateCtrlMode = NvMMVideoEncRateCtrlMode_VBR;
                    break;
                case NVX_VIDEO_RateControlMode_VBR2:
                    pNvxVideoEncoder->eRateCtrlMode = NvMMVideoEncRateCtrlMode_VBR2;
                    break;

                default:
                    pNvxVideoEncoder->eRateCtrlMode = NvMMVideoEncRateCtrlMode_CBR;
            }
        }
        break;
    case OMX_IndexParamVideoQuantization:
        {
            OMX_VIDEO_PARAM_QUANTIZATIONTYPE* pQuantType =
                (OMX_VIDEO_PARAM_QUANTIZATIONTYPE *)pComponentParameterStructure;
            if(pQuantType->nSize != sizeof(OMX_VIDEO_PARAM_QUANTIZATIONTYPE))
                return OMX_ErrorBadPortIndex;
            pNvxVideoEncoder->nInitQPI = pQuantType->nQpI;
            pNvxVideoEncoder->nInitQPP = pQuantType->nQpP;
            pNvxVideoEncoder->nInitQPB = pQuantType->nQpB;  // NVENC onwards only
        }
        break;
    case OMX_IndexParamVideoPortFormat:
    {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        NvxPort *pPort = NULL;

        // Error checking
        if (((NvU32)s_nvx_port_input == pPortFormat->nPortIndex)
            && (pPortFormat->eColorFormat != OMX_COLOR_FormatYUV420Planar)
            && (pPortFormat->eColorFormat != OMX_COLOR_FormatAndroidOpaque))
            return OMX_ErrorBadParameter;

        pPort = &pComponent->pPorts[pPortFormat->nPortIndex];

        if (pPortFormat->nVersion.s.nVersionMajor != pPort->oPortDef.nVersion.s.nVersionMajor)
            return OMX_ErrorVersionMismatch;

        if (pPortFormat->nSize != sizeof (*pPortFormat))
            return OMX_ErrorUnsupportedIndex;

        if (pPort->oPortDef.eDomain != OMX_PortDomainVideo)
            return OMX_ErrorUnsupportedIndex;

        if (pPortFormat->eCompressionFormat != pPort->oPortDef.format.video.eCompressionFormat)
            return OMX_ErrorUnsupportedSetting;

        pComponent->pPorts[pPortFormat->nPortIndex].oPortDef.format.video.eColorFormat = pPortFormat->eColorFormat;
        pComponent->pPorts[pPortFormat->nPortIndex].oPortDef.format.video.eCompressionFormat = pPortFormat->eCompressionFormat;
        break;
    }
    case OMX_IndexParamVideoIntraRefresh:
    {
       OMX_VIDEO_PARAM_INTRAREFRESHTYPE *pInraRefresh = (OMX_VIDEO_PARAM_INTRAREFRESHTYPE *)pComponentParameterStructure;
       if(pInraRefresh->eRefreshMode == OMX_VIDEO_IntraRefreshCyclic)
       {
           pNvxVideoEncoder->Cirmbs = pInraRefresh->nCirMBs;
           pNvxVideoEncoder->bCyclicIntraRefreshEnable = OMX_TRUE;
       }
       else
           return OMX_ErrorNotImplemented;
       break;
    }
    case OMX_IndexParamVideoErrorCorrection:
    {
        return OMX_ErrorNotImplemented;
    }
    break;

#if USE_ANDROID_NATIVE_WINDOW
    case NVX_IndexParamEnableAndroidBuffers:
        return HandleEnableANBLite(pComponent, s_nvx_port_input,
                               pComponentParameterStructure);
    case NVX_IndexParamUseAndroidNativeBuffer:
        return HandleUseANBLite(pComponent, pComponentParameterStructure);
#endif

    case NVX_IndexParamVideoEncodeH264QualityParams:
        {
            NVX_PARAM_VIDENC_H264_QUALITY_PARAMS* pH264QualityParams =
                (NVX_PARAM_VIDENC_H264_QUALITY_PARAMS *)pComponentParameterStructure;
            if(pH264QualityParams->nSize != sizeof(NVX_PARAM_VIDENC_H264_QUALITY_PARAMS))
                return OMX_ErrorBadPortIndex;
            pNvxVideoEncoder->H264EncQualityParams.FavorIntraBias = pH264QualityParams->nFavorIntraBias;
            pNvxVideoEncoder->H264EncQualityParams.FavorInterBias = pH264QualityParams->nFavorInterBias;
            pNvxVideoEncoder->H264EncQualityParams.FavorIntraBias_16X16 = pH264QualityParams->nFavorIntraBias_16X16;
        }
        break;

    case NVX_IndexParamTempFilePath:
        {
            NVX_PARAM_TEMPFILEPATH *pTempFilePathParam = (NVX_PARAM_TEMPFILEPATH *)pComponentParameterStructure;

            NvOsFree(pNvxVideoEncoder->pTempFilePath);
            pNvxVideoEncoder->pTempFilePath = NvOsAlloc(NvOsStrlen(pTempFilePathParam->pTempPath) + 1);
            if (!pNvxVideoEncoder->pTempFilePath)
            {
                NVXTRACE((NVXT_ERROR,pComponent->eObjectType, "ERROR:NvxVideoEncoderSetParameter:"
                    ":pTempFilePath = %s:[%s(%d)]\n",pNvxVideoEncoder->pTempFilePath
                    , __FILE__, __LINE__));
                return OMX_ErrorInsufficientResources;
            }
            NvOsStrncpy(pNvxVideoEncoder->pTempFilePath, pTempFilePathParam->pTempPath, NvOsStrlen(pTempFilePathParam->pTempPath) + 1);
            break;
        }
    case NVX_IndexParamEmbedMMBuffer:
        {
            OMX_CONFIG_BOOLEANTYPE *param = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;
            if (param)
                pNvxVideoEncoder->oBase.oPorts[s_nvx_port_input].bEmbedNvMMBuffer = param->bEnabled;
        }
        break;
    case NVX_IndexParamEncMaxClock:
        {
            OMX_CONFIG_BOOLEANTYPE *param = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;
            if (param)
                pNvxVideoEncoder->bSetMaxEncClock = param->bEnabled;
        }
        break;
    case NVX_IndexParamEnc2DCopy:
        {
            OMX_CONFIG_BOOLEANTYPE *param = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;
            if (param)
                pNvxVideoEncoder->oBase.oPorts[s_nvx_port_input].bEnc2DCopy = param->bEnabled;
        }
        break;

#if ENABLE_ANDROID_INSERT_SPSPPS
    case NVX_IndexParamInsertSPSPPSAtIDR:
        {
            // defined in frameworks/native/include/media/hardware/HardwareAPI.h
            struct PrependSPSPPSToIDRFramesParams {
                OMX_U32 nSize;
                OMX_VERSIONTYPE nVersion;
                OMX_BOOL bEnable;
            };
            struct PrependSPSPPSToIDRFramesParams *param = (struct PrependSPSPPSToIDRFramesParams *)pComponentParameterStructure;
            if (param)
                pNvxVideoEncoder->bInsertSPSPPSAtIDR = param->bEnable;
        }
        break;
#endif
    case NVX_IndexParamSkipFrame:
        {
            OMX_CONFIG_BOOLEANTYPE *param = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;
            if (param)
                pNvxVideoEncoder->bSkipFrame = param->bEnabled;
        }
        break;
    default:
        return NvxComponentBaseSetParameter(pComponent, nIndex, pComponentParameterStructure);
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxVideoEncoderGetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    NVX_CONFIG_TIMESTAMPTYPE *pTimestamp;
    SNvxLiteVideoEncoderData *pNvxVideoEncoder;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxLiteVideoEncoderGetConfig\n"));

    pNvxVideoEncoder = (SNvxLiteVideoEncoderData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
        case NVX_IndexConfigTimestampOverride:
        {
            pTimestamp = (NVX_CONFIG_TIMESTAMPTYPE *)pComponentConfigStructure;
            pTimestamp->bOverrideTimestamp = pNvxVideoEncoder->bOverrideTimestamp;
        }
        break;
        case NVX_IndexConfigGetNVMMBlock:
        {
            NVX_CONFIG_GETNVMMBLOCK *pBlockReq = (NVX_CONFIG_GETNVMMBLOCK *)pComponentConfigStructure;

            if (!pNvxVideoEncoder || !pNvxVideoEncoder->bInitialized)
                return OMX_ErrorNotReady;

            pBlockReq->pNvMMTransform = &pNvxVideoEncoder->oBase;
            pBlockReq->nNvMMPort = pBlockReq->nPortIndex; // nvmm and OMX port indexes match
        }
        break;
        case OMX_IndexConfigVideoFramerate:
        {
            OMX_CONFIG_FRAMERATETYPE *pFrameRate = (OMX_CONFIG_FRAMERATETYPE *)pComponentConfigStructure;
            pFrameRate->xEncodeFramerate = (pNvxVideoEncoder->nFrameRate<<16);
        }
        break;
        case NVX_IndexConfigH264NALLen:
        {
            NVX_VIDEO_CONFIG_H264NALLEN *pNalLen =
                (NVX_VIDEO_CONFIG_H264NALLEN *)pComponentConfigStructure;
            if (pNvxVideoEncoder->bRTPMode)
            {
                pNalLen->nNalLen = 4;
            }
            else
            {
                pNalLen->nNalLen = 0;
            }
        }
        break;
        case OMX_IndexConfigVideoNalSize:
        {
            OMX_VIDEO_CONFIG_NALSIZE *oOMXNalSize =
                (OMX_VIDEO_CONFIG_NALSIZE *)pComponentConfigStructure;
            if (pNvxVideoEncoder->bRTPMode)
            {
                oOMXNalSize->nNaluBytes = 4;
            }
            else
            {
                oOMXNalSize->nNaluBytes = 0;
            }
        }
        break;
        case NVX_IndexConfigDecoderConfigInfo:
        {
            NVX_CONFIG_DecoderConfigInfo *pDCI = (NVX_CONFIG_DecoderConfigInfo *)pComponentConfigStructure;
            NvMMVideoEncDecodeConfigInfo oNvMMDCI;

            if (!pNvxVideoEncoder || !pNvxVideoEncoder->bInitialized)
                return OMX_ErrorNotReady;

            pNvxVideoEncoder->oBase.hBlock->GetAttribute(pNvxVideoEncoder->oBase.hBlock,
                                                         NvMMAttributeVideoEnc_DciInfo,
                                                         sizeof(NvMMVideoEncDecodeConfigInfo),
                                                         &oNvMMDCI);

            pDCI->SizeOfDecoderConfigInfo = (OMX_U8)oNvMMDCI.SizeOfDecodeConfigInfo;
            NV_ASSERT(pDCI->SizeOfDecoderConfigInfo <= NVX_VIDEOENC_DCI_SIZE);
            NvOsMemcpy(pDCI->DecoderConfigInfo, oNvMMDCI.DecodeConfigInfo,
                       pDCI->SizeOfDecoderConfigInfo);
        }
        break;
        case OMX_IndexConfigVideoAVCIntraPeriod:
        {
            OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pIntraPeriod =
                (OMX_VIDEO_CONFIG_AVCINTRAPERIOD *) pComponentConfigStructure;
            pIntraPeriod->nIDRPeriod = pNvxVideoEncoder->nAVCIDRFrameInterval;
            pIntraPeriod->nPFrames = pNvxVideoEncoder->nIFrameInterval - 1;
        }
        break;
        case OMX_IndexConfigCommonOutputCrop:
        {
            OMX_CONFIG_RECTTYPE *pVideoCropRect =
                  (OMX_CONFIG_RECTTYPE *) pComponentConfigStructure;

            if (!pNvxVideoEncoder || !pNvxVideoEncoder->bInitialized)
                return OMX_ErrorNotReady;

            NvOsMemcpy(pVideoCropRect,&pNvxVideoEncoder->OutputCropRec,
                          sizeof(OMX_CONFIG_RECTTYPE));
        }
        break;
    case NVX_IndexConfigVideoEncodeQuantizationRange:
        {
            NVX_CONFIG_VIDENC_QUANTIZATION_RANGE* pQuantRange =
                (NVX_CONFIG_VIDENC_QUANTIZATION_RANGE *)pComponentConfigStructure;
            NvMMVideoEncQPRange QPRange;
            NvError err = NvSuccess;

            if (!pNvxVideoEncoder || !pNvxVideoEncoder->bInitialized)
                return OMX_ErrorNotReady;

            NvOsMemset(&QPRange, 0, sizeof(NvMMVideoEncQPRange));
            QPRange.StructSize = sizeof(NvMMVideoEncQPRange);

            err = pNvxVideoEncoder->oBase.hBlock->GetAttribute(pNvxVideoEncoder->oBase.hBlock,
                                                               NvMMAttributeVideoEnc_QPRange,
                                                               sizeof(NvMMVideoEncQPRange),
                                                               &QPRange);
            if (err != NvSuccess)
            {
                NV_DEBUG_PRINTF(("In NvxVideoEncoderGetConfig(), GetAttribute() FAILED \n"));
                return OMX_ErrorBadParameter;
            }

            pNvxVideoEncoder->nMinQpI = pQuantRange->nMinQpI = QPRange.IMinQP;
            pNvxVideoEncoder->nMaxQpI = pQuantRange->nMaxQpI = QPRange.IMaxQP;
            pNvxVideoEncoder->nMinQpP = pQuantRange->nMinQpP = QPRange.PMinQP;
            pNvxVideoEncoder->nMaxQpP = pQuantRange->nMaxQpP = QPRange.PMaxQP;
            pNvxVideoEncoder->nMinQpB = pQuantRange->nMinQpB = QPRange.BMinQP;
            pNvxVideoEncoder->nMaxQpB = pQuantRange->nMaxQpB = QPRange.BMaxQP;
        }
        break;

    case NVX_IndexConfigVideoStereoInfo:
        {
            OMX_U32 stereoFPType = 0, stereoContentType = 0, isStereoEnabled = 0;

            NVX_CONFIG_VIDEO_STEREOINFO* pStereoInfo =
                (NVX_CONFIG_VIDEO_STEREOINFO *)pComponentConfigStructure;

            if (pNvxVideoEncoder->bInitialized)
            {
                NvMMVideoEncH264StereoInfo stereoInfo;
                NvError err = NvSuccess;

                NV_DEBUG_PRINTF(("Check Stereo Status by querying the NvMM Encoder module\n"));

                err = pNvxVideoEncoder->oBase.hBlock->GetAttribute(pNvxVideoEncoder->oBase.hBlock,
                                                           NvMMAttributeVideoEnc_H264StereoInfo,
                                                           sizeof(NvMMVideoEncH264StereoInfo),
                                                           &stereoInfo);
                if (err != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("In NvxVideoEncoderGetConfig(), GetAttribute() FAILED \n"));
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

        case NVX_IndexConfigVideoProtect:
        {
            OMX_CONFIG_BOOLEANTYPE *param = (OMX_CONFIG_BOOLEANTYPE *)pComponentConfigStructure;
            param->bEnabled = pNvxVideoEncoder->oBase.bVideoProtect;
        }
        break;

        case NVX_IndexConfigVideoEncodeLastFrameQP:
        {
            NVX_PARAM_LASTFRAMEQP *param = (NVX_PARAM_LASTFRAMEQP *)pComponentConfigStructure;
            param->LastFrameQP = pNvxVideoEncoder->oBase.LastFrameQP;
            NV_DEBUG_PRINTF(("%s: NVX_IndexConfigVideoEncodeLastFrameQP: Last Frame QP  %d\n", __FUNCTION__,
                             pNvxVideoEncoder->oBase.LastFrameQP));
        }
        break;

        case NVX_IndexConfigUseNvBuffer2:
        {
            OMX_CONFIG_BOOLEANTYPE *param = (OMX_CONFIG_BOOLEANTYPE *)pComponentConfigStructure;
            param->bEnabled = pNvxVideoEncoder->oBase.bUseNvBuffer2;
        }
        break;

        default:
            return NvxComponentBaseGetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxVideoEncoderSetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentConfigStructure)
{
    NVX_CONFIG_TIMESTAMPTYPE *pTimestamp;
    SNvxLiteVideoEncoderData *pNvxVideoEncoder;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxLiteVideoEncoderSetConfig\n"));

    pNvxVideoEncoder = (SNvxLiteVideoEncoderData *)pNvComp->pComponentData;
    switch ( nIndex )
    {
        case NVX_IndexConfigTimestampOverride:
        {
            pTimestamp = (NVX_CONFIG_TIMESTAMPTYPE *)pComponentConfigStructure;
            pNvxVideoEncoder->bOverrideTimestamp = pTimestamp->bOverrideTimestamp;
            break;
        }
        case NVX_IndexConfigProfile:
        {
            NVX_CONFIG_PROFILE *pProf = (NVX_CONFIG_PROFILE *)pComponentConfigStructure;
            NvxNvMMLiteTransformSetProfile(&pNvxVideoEncoder->oBase, pProf);
            break;
        }
        case OMX_IndexConfigVideoFramerate:
        {
            OMX_CONFIG_FRAMERATETYPE *pFrameRate = (OMX_CONFIG_FRAMERATETYPE *)pComponentConfigStructure;
            pNvxVideoEncoder->nFrameRate = (OMX_U32)((pFrameRate->xEncodeFramerate + ((1<<16) - 1)) >> 16);

            if (pNvxVideoEncoder->bInitialized)
                NvxVideoEncoderCommonstreamProp(&pNvxVideoEncoder->oBase, pNvxVideoEncoder, NvMMBlockVEnc_FrameRate);
        }
        break;
        case NVX_IndexConfigH264NALLen:
        {
            NVX_VIDEO_CONFIG_H264NALLEN *pNalLen =
                (NVX_VIDEO_CONFIG_H264NALLEN *)pComponentConfigStructure;
            if (pNalLen->nNalLen > 0)
            {
                pNvxVideoEncoder->bRTPMode = OMX_TRUE;
            }
            else
            {
                pNvxVideoEncoder->bRTPMode = OMX_FALSE;
            }
        }
        break;
        case OMX_IndexConfigVideoNalSize:
        {
            OMX_VIDEO_CONFIG_NALSIZE *oOMXNalSize =
                (OMX_VIDEO_CONFIG_NALSIZE *)pComponentConfigStructure;
            if (oOMXNalSize->nNaluBytes > 0)
            {
                pNvxVideoEncoder->bRTPMode = OMX_TRUE;
            }
            else
            {
                pNvxVideoEncoder->bRTPMode = OMX_FALSE;
            }
        }
        break;
        case OMX_IndexConfigVideoBitrate:
        {
            OMX_VIDEO_CONFIG_BITRATETYPE *pBitrate =
                (OMX_VIDEO_CONFIG_BITRATETYPE *)pComponentConfigStructure;
            pNvxVideoEncoder->nBitRate = pBitrate->nEncodeBitrate;

            if (pNvxVideoEncoder->bInitialized)
                NvxVideoEncoderCommonstreamProp(&pNvxVideoEncoder->oBase, pNvxVideoEncoder, NvMMBlockVEnc_BitRate);
        }
        break;
        case OMX_IndexConfigVideoIntraVOPRefresh:
        {
            OMX_CONFIG_INTRAREFRESHVOPTYPE *pIntra =
                (OMX_CONFIG_INTRAREFRESHVOPTYPE *)pComponentConfigStructure;

            if (pNvxVideoEncoder->bInitialized && pIntra->IntraRefreshVOP)
            {
                NvMMVideoEncForceIntraFrame oFIF = { sizeof(NvMMVideoEncForceIntraFrame) };

                pNvxVideoEncoder->oBase.hBlock->SetAttribute(pNvxVideoEncoder->oBase.hBlock,
                                                             NvMMAttributeVideoEnc_ForceIntraFrame,
                                                             0, oFIF.StructSize, &oFIF);
            }
        }
        break;
        case NVX_IndexConfigSliceLevelEncode:
        {
            NVX_CONFIG_VIDEO_SLICELEVELENCODE *pSlice =
                (NVX_CONFIG_VIDEO_SLICELEVELENCODE *)pComponentConfigStructure;
            pNvxVideoEncoder->bActiveSliceLevelEncode = pSlice->SliceLevelEncode;
            s_SetActiveSliceLevelEncode(pNvxVideoEncoder);
        }
        break;
        case NVX_IndexConfigTemporalTradeOff:
        {
            NVX_CONFIG_TEMPORALTRADEOFF *pTemporalTradeOff = (NVX_CONFIG_TEMPORALTRADEOFF *)pComponentConfigStructure;
            pNvxVideoEncoder->TemporalTradeOffLevel = pTemporalTradeOff->TemporalTradeOffLevel;
            s_SetTemporalTradeOff(pNvxVideoEncoder);
            break;
        }
        case OMX_IndexConfigCommonRotate:
        {
            OMX_CONFIG_ROTATIONTYPE *pRotate = (OMX_CONFIG_ROTATIONTYPE *)pComponentConfigStructure;
            pNvxVideoEncoder->RotateAngle = pRotate->nRotation;
            s_SetInputRotation(pNvxVideoEncoder);
            break;
        }
        case OMX_IndexConfigCommonMirror:
        {
            OMX_CONFIG_MIRRORTYPE *pMirror = (OMX_CONFIG_MIRRORTYPE *)pComponentConfigStructure;
            pNvxVideoEncoder->Mirror = pMirror->eMirror;
            s_SetInputMirroring(pNvxVideoEncoder);
            break;
        }
        case OMX_IndexConfigVideoAVCIntraPeriod:
        {
            OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pIntraPeriod =
                  (OMX_VIDEO_CONFIG_AVCINTRAPERIOD *)pComponentConfigStructure;
            if (pNvxVideoEncoder->oType == TYPE_H264)
            {
                pNvxVideoEncoder->nAVCIDRFrameInterval = pIntraPeriod->nIDRPeriod;
                if (pIntraPeriod->nPFrames)
                    pNvxVideoEncoder->nIFrameInterval = pIntraPeriod->nPFrames + 1;
                else
                    pNvxVideoEncoder->nIFrameInterval = pIntraPeriod->nIDRPeriod;

                if (pNvxVideoEncoder->bInitialized)
                    NvxVideoEncoderCommonstreamProp(&pNvxVideoEncoder->oBase,
                            pNvxVideoEncoder, NvMMBlockVEnc_IntraFrameInterval);
            }
        }
        break;
        case OMX_IndexConfigCommonOutputCrop:
        {
            OMX_CONFIG_RECTTYPE *pH264VideoCropRect =
                  (OMX_CONFIG_RECTTYPE *) pComponentConfigStructure;

            if(pH264VideoCropRect->nSize != sizeof(OMX_CONFIG_RECTTYPE))
                  return OMX_ErrorBadParameter;
            NvOsMemcpy(&pNvxVideoEncoder->OutputCropRec, pH264VideoCropRect,
                           sizeof(OMX_CONFIG_RECTTYPE));
            pNvxVideoEncoder->FlagCropRect = NV_TRUE;
        }
        break;
    case NVX_IndexConfigVideoEncodeQuantizationRange:
        {
            NVX_CONFIG_VIDENC_QUANTIZATION_RANGE *pQuantRange =
                (NVX_CONFIG_VIDENC_QUANTIZATION_RANGE *)pComponentConfigStructure;
            if(pQuantRange->nSize != sizeof(NVX_CONFIG_VIDENC_QUANTIZATION_RANGE))
                return OMX_ErrorBadPortIndex;
            pNvxVideoEncoder->nMinQpI = pQuantRange->nMinQpI;
            pNvxVideoEncoder->nMaxQpI = pQuantRange->nMaxQpI;
            pNvxVideoEncoder->nMinQpP = pQuantRange->nMinQpP;
            pNvxVideoEncoder->nMaxQpP = pQuantRange->nMaxQpP;
            pNvxVideoEncoder->nMinQpB = pQuantRange->nMinQpB;
            pNvxVideoEncoder->nMaxQpB = pQuantRange->nMaxQpB;
            if (pNvxVideoEncoder->bInitialized)
                s_SetNvMmVideoEncQuantRange(pNvxVideoEncoder);
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

            NV_DEBUG_PRINTF(("EncoderSetConfig(): StereoEnabled, FPType, ContentType = %x, %x, %x\n",
                               isStereoEnabled, stereoFPType, stereoContentType));

            if (pNvxVideoEncoder->bInitialized)
            {
                NvMMVideoEncH264StereoInfo stereoInfo;
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
                stereoInfo.StructSize = sizeof(NvMMVideoEncH264StereoInfo);
                stereoInfo.StereoFlags = stereoflags;

                err = pNvxVideoEncoder->oBase.hBlock->SetAttribute(pNvxVideoEncoder->oBase.hBlock,
                                                           NvMMAttributeVideoEnc_H264StereoInfo,
                                                           0, stereoInfo.StructSize, &stereoInfo);
                if (err != NvSuccess)
                {
                    NV_DEBUG_PRINTF(("In NvxVideoEncoderSetConfig(), SetAttribute() FAILED.\n"));
                    return OMX_ErrorBadParameter;
                }
            }

        }
        break;
        case NVX_IndexParamTimeLapseView:
        {
            OMX_CONFIG_BOOLEANTYPE *timeLapse = (OMX_CONFIG_BOOLEANTYPE *)pComponentConfigStructure;
            pNvxVideoEncoder->bTimeLapse = timeLapse->bEnabled;
        }
        break;

        case NVX_IndexParamEncMaxClock:
        {
            OMX_CONFIG_BOOLEANTYPE *param = (OMX_CONFIG_BOOLEANTYPE *)pComponentConfigStructure;
            pNvxVideoEncoder->bSetMaxEncClock = param->bEnabled;
        }
        break;

        case NVX_IndexConfigVideo2DProcessing:
        {
            NVX_CONFIG_VIDEO2DPROCESSING *v2d = (NVX_CONFIG_VIDEO2DPROCESSING *)pComponentConfigStructure;
            NV_DEBUG_PRINTF(("%s got NVX_IndexConfigVideo2DProcessing port:%d flag:%x!!!!!",
                        __FUNCTION__,
                        v2d->nPortIndex,
                        v2d->nSetupFlags));

            NvxMutexLock(pNvxVideoEncoder->oBase.oPorts[s_nvx_port_input].hVPPMutex);
            pNvxVideoEncoder->oBase.oPorts[s_nvx_port_input].bEnc2DCopy = OMX_TRUE;
            pNvxVideoEncoder->oBase.oPorts[s_nvx_port_input].Video2DProc = *v2d;
            pNvxVideoEncoder->oBase.oPorts[s_nvx_port_input].nBackgroundDoneMap = 0;
            NvxMutexUnlock(pNvxVideoEncoder->oBase.oPorts[s_nvx_port_input].hVPPMutex);
        }
        break;

        case NVX_IndexConfigUseNvBuffer:
        {
            NVX_PARAM_USENVBUFFER* pParam = (NVX_PARAM_USENVBUFFER*)pComponentConfigStructure;
            pNvxVideoEncoder->oBase.oPorts[s_nvx_port_output].bEmbedNvMMBuffer = pParam->bUseNvBuffer;
        }
        break;

        case NVX_IndexConfigVideoProtect:
        {
            OMX_CONFIG_BOOLEANTYPE *param = (OMX_CONFIG_BOOLEANTYPE *)pComponentConfigStructure;
            pNvxVideoEncoder->oBase.bVideoProtect = param->bEnabled;
            NvOsDebugPrintf("%s: NVX_IndexConfigVideoProtect: VPR %s\n", __FUNCTION__,
                             pNvxVideoEncoder->oBase.bVideoProtect ? "ENABLED" : "DISABLED");
        }
        break;

        case NVX_IndexConfigVideoSuperFineQuality:
        {
            OMX_CONFIG_BOOLEANTYPE *superFineQuality = (OMX_CONFIG_BOOLEANTYPE *)pComponentConfigStructure;
            pNvxVideoEncoder->bSuperFineQuality = superFineQuality->bEnabled;
        }
        break;

        case NVX_IndexConfigUseNvBuffer2:
        {
            OMX_CONFIG_BOOLEANTYPE *param = (OMX_CONFIG_BOOLEANTYPE *)pComponentConfigStructure;
            pNvxVideoEncoder->oBase.bUseNvBuffer2 = param->bEnabled;
        }
        break;

        default:
            return NvxComponentBaseSetConfig( pNvComp, nIndex, pComponentConfigStructure);
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxVideoEncoderWorkerFunction(OMX_IN NvxComponent *pComponent, OMX_IN OMX_BOOL bAllPortsReady, OMX_OUT OMX_BOOL *pbMoreWork, OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    SNvxLiteVideoEncoderData *pNvxVideoEncoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortIn = &pComponent->pPorts[s_nvx_port_input];
    NvxPort *pPortOut = &pComponent->pPorts[s_nvx_port_output];

    NVXTRACE((NVXT_WORKER, pComponent->eObjectType, "+NvxLiteVideoEncoderWorkerFunction\n"));

    *pbMoreWork = bAllPortsReady;
    //if (!bAllPortsReady)
    //    return OMX_ErrorNone;

    pNvxVideoEncoder = (SNvxLiteVideoEncoderData *)pComponent->pComponentData;

    if (pNvxVideoEncoder->bFirstFrame)
    {
        // TO DO: Are there any codecs that need to query for initial setup?
        pNvxVideoEncoder->bFirstFrame = OMX_FALSE;
    }

    // Note: For each worker function trigger, ask NvMM component to
    // encode. Callbacks should return the actual data
    eError = NvxNvMMLiteTransformWorkerBase(&pNvxVideoEncoder->oBase,
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

static MPEG4EncLevelIdc s_GetNvMMMPEG4Level(OMX_VIDEO_MPEG4LEVELTYPE eLevel)
{
    switch (eLevel)
    {
    case OMX_VIDEO_MPEG4Level0:
    case OMX_VIDEO_MPEG4Level0b:    return MPEG4_Level_0;
    case OMX_VIDEO_MPEG4Level1:     return MPEG4_Level_1;
    case OMX_VIDEO_MPEG4Level2:     return MPEG4_Level_2;
    case OMX_VIDEO_MPEG4Level3:     return MPEG4_Level_3;
    case OMX_VIDEO_MPEG4Level4:
    case OMX_VIDEO_MPEG4Level4a:    return MPEG4_Level_4a;
    case OMX_VIDEO_MPEG4Level5:     return MPEG4_Level_5;
    default:                        return MPEG4_Level_Undefined;
    }
}

static H264EncLevelIdc s_GetNvMMH264Level(OMX_VIDEO_AVCLEVELTYPE eLevel)
{
    H264EncLevelIdc NvMMH264Level = H264_LevelIdc_Undefined;
    switch (eLevel)
    {
    case OMX_VIDEO_AVCLevel1:   NvMMH264Level = H264_LevelIdc_10;        break;
    case OMX_VIDEO_AVCLevel1b:  NvMMH264Level = H264_LevelIdc_1B;        break;
    case OMX_VIDEO_AVCLevel11:  NvMMH264Level = H264_LevelIdc_11;        break;
    case OMX_VIDEO_AVCLevel12:  NvMMH264Level = H264_LevelIdc_12;        break;
    case OMX_VIDEO_AVCLevel13:  NvMMH264Level = H264_LevelIdc_13;        break;
    case OMX_VIDEO_AVCLevel2:   NvMMH264Level = H264_LevelIdc_20;        break;
    case OMX_VIDEO_AVCLevel21:  NvMMH264Level = H264_LevelIdc_21;        break;
    case OMX_VIDEO_AVCLevel22:  NvMMH264Level = H264_LevelIdc_22;        break;
    case OMX_VIDEO_AVCLevel3:   NvMMH264Level = H264_LevelIdc_30;        break;
    case OMX_VIDEO_AVCLevel31:  NvMMH264Level = H264_LevelIdc_31;        break;
    case OMX_VIDEO_AVCLevel32:  NvMMH264Level = H264_LevelIdc_32;        break;
    case OMX_VIDEO_AVCLevel4:   NvMMH264Level = H264_LevelIdc_40;        break;
    case OMX_VIDEO_AVCLevel41:  NvMMH264Level = H264_LevelIdc_41;        break;
    case OMX_VIDEO_AVCLevel42:  NvMMH264Level = H264_LevelIdc_42;        break;
    case OMX_VIDEO_AVCLevel5:   NvMMH264Level = H264_LevelIdc_50;        break;
    case OMX_VIDEO_AVCLevel51:  NvMMH264Level = H264_LevelIdc_51;        break;
    default:        break;
    }
    return NvMMH264Level;
}

static NvMMH264EncProfile s_GetNvMMH264Profile(OMX_VIDEO_AVCPROFILETYPE eProfile)
{
    switch (eProfile)
    {
    case OMX_VIDEO_AVCProfileBaseline:   return H264_Profile_Baseline;
    case OMX_VIDEO_AVCProfileMain:       return H264_Profile_Main;
    case OMX_VIDEO_AVCProfileHigh:       return H264_Profile_High;
    default:                             return H264_Profile_Undefined;
    }
}

static OMX_ERRORTYPE NvxVideoEncoderCommonstreamProp(SNvxNvMMLiteTransformData *pBase, SNvxLiteVideoEncoderData *pNvxVideoEncoder, NvU32 UpdateParams)
{
    NvMMVideoEncBitStreamProperty oVideoParams;
    OMX_U32 Width = pNvxVideoEncoder->nSurfWidth;
    OMX_U32 Height = pNvxVideoEncoder->nSurfHeight;

    NvOsMemset(&oVideoParams, 0, sizeof(NvMMVideoEncBitStreamProperty));
    oVideoParams.StructSize  = sizeof(NvMMVideoEncBitStreamProperty);
    oVideoParams.VideoEncResolution.width = Width;
    oVideoParams.VideoEncResolution.height = Height;
    oVideoParams.UpdateParams = UpdateParams;
    oVideoParams.StreamIndex = 0;
    oVideoParams.BitRate     = pNvxVideoEncoder->nBitRate;
    oVideoParams.PeakBitrate = pNvxVideoEncoder->nPeakBitrate = pNvxVideoEncoder->nBitRate;
    oVideoParams.FrameRate   = pNvxVideoEncoder->nFrameRate;
    oVideoParams.bFrameSkip = pNvxVideoEncoder->bFrameSkip;
    oVideoParams.bStringentBitrate = pNvxVideoEncoder->bStringentBitrate;
    oVideoParams.bAllIFrames = pNvxVideoEncoder->bAllIFrames;
    oVideoParams.bEnableStuffing = pNvxVideoEncoder->bEnableStuffing;
    oVideoParams.bSliceLevelEncode = pNvxVideoEncoder->bSliceLevelEncode;
    oVideoParams.bMona = NV_FALSE;      // to be set by command line

    if (pNvxVideoEncoder->oType == TYPE_MPEG4) // to be set by command line
    {
        OMX_VIDEO_PARAM_MPEG4TYPE *pVideoParams =
            (OMX_VIDEO_PARAM_MPEG4TYPE *)pBase->pParentComp->pPorts[s_nvx_port_input].pPortPrivate;
        if (pNvxVideoEncoder->eLevel != OMX_VIDEO_MPEG4LevelMax)
            oVideoParams.Level.MPEG4Level = (MPEG4EncLevelIdc)pNvxVideoEncoder->eLevel;   //No conversion required.
        else
            oVideoParams.Level.MPEG4Level = s_GetNvMMMPEG4Level(pVideoParams->eLevel);
        if(oVideoParams.Level.MPEG4Level == MPEG4_Level_Unsupported)
        {
            NVXTRACE((NVXT_ERROR,NVXT_VIDEO_ENCODER,"ERROR:NvxVideoEncoderCommonstreamProp:"
            ":eError = OMX_ErrorUnsupportedIndex:[%s(%d)]\n",__FILE__, __LINE__));
            return OMX_ErrorUnsupportedIndex;
        }
        oVideoParams.IntraFrameInterval = pNvxVideoEncoder->nIFrameInterval;
    }
    else if (pNvxVideoEncoder->oType == TYPE_H264)
    {
        OMX_VIDEO_PARAM_AVCTYPE *pAvcParams =
            (OMX_VIDEO_PARAM_AVCTYPE *)pBase->pParentComp->pPorts[s_nvx_port_input].pPortPrivate;
        if (pNvxVideoEncoder->eLevel != OMX_VIDEO_AVCLevelMax)
            oVideoParams.Level.H264Level = (H264EncLevelIdc)pNvxVideoEncoder->eLevel;   //No conversion required.
        else
            oVideoParams.Level.H264Level = s_GetNvMMH264Level(pAvcParams->eLevel);
        oVideoParams.IntraFrameInterval = pNvxVideoEncoder->nAVCIDRFrameInterval;
    }
    else
    {
        oVideoParams.IntraFrameInterval = pNvxVideoEncoder->nIFrameInterval;
    }

    pBase->hBlock->SetAttribute( pBase->hBlock,
        NvMMAttributeVideoEnc_CommonStreamProperty,
        NvMMLiteSetAttrFlag_Notification,
        oVideoParams.StructSize,
        &oVideoParams);

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxVideoEncoderSetNvMMAttributes(SNvxNvMMLiteTransformData *pBase, SNvxLiteVideoEncoderData *pNvxVideoEncoder)
{
    NvMMVideoEncConfiguration oEncConfig;
    NvMMVideoEncLookahead oLookAhead;
    NvError err = NvSuccess;
    NVXTRACE((NVXT_CALLGRAPH, NVXT_VIDEO_ENCODER,"+NvxVideoEncoderSetNvMMAttributes"));
    NvOsMemset(&oEncConfig, 0, sizeof(NvMMVideoEncConfiguration));
    oEncConfig.structSize = sizeof(NvMMVideoEncConfiguration);
    oEncConfig.AppType = pNvxVideoEncoder->eAppConfig;
    oEncConfig.QualityLevel = pNvxVideoEncoder->eQualityLevel;
    oEncConfig.ErrResLevel = pNvxVideoEncoder->eResilLevel;
    oEncConfig.bSetMaxEncClock = pNvxVideoEncoder->bSetMaxEncClock;
    pBase->hBlock->SetAttribute( pBase->hBlock,
        NvMMAttributeVideoEnc_Configuration,
        0,
        oEncConfig.structSize,
        &oEncConfig);

    if(pNvxVideoEncoder->bTimeLapse)
    {
        oLookAhead.StructSize=sizeof(NvMMVideoEncLookahead);
        oLookAhead.EnableLookahead = 0;
        pBase->hBlock->SetAttribute( pBase->hBlock,
            NvMMAttributeVideoEnc_Lookahead,
            0,
            oLookAhead.StructSize,
            &oLookAhead);
    }

    NvxVideoEncoderCommonstreamProp(pBase, pNvxVideoEncoder, NvMMBlockVEnc_Force32);

    switch ( pNvxVideoEncoder->oType )
    {
        case TYPE_H263:
        case TYPE_MPEG4:
        {
            NvMMVideoEncMPEG4Params oMpeg4Params;
            NvOsMemset(&oMpeg4Params, 0, sizeof(NvMMVideoEncMPEG4Params));
            oMpeg4Params.StructSize = sizeof(NvMMVideoEncMPEG4Params);
            oMpeg4Params.VirtualBufferSize = pNvxVideoEncoder->nVirtualBufferSize = pNvxVideoEncoder->nBitRate;
            oMpeg4Params.InitBufferFullPerc = 0;
            oMpeg4Params.bH263Encode = (pNvxVideoEncoder->oType == TYPE_H263);

            err = pBase->hBlock->SetAttribute( pBase->hBlock,
                NvMMAttributeVideoEnc_Mpeg4StreamProperty,
                0,
                oMpeg4Params.StructSize,
                &oMpeg4Params);
            if (err != NvSuccess)
                return OMX_ErrorBadParameter;
        }
        break;
        case TYPE_H264:
        {
            NvMMVideoEncH264Params oAvcParams;
            OMX_VIDEO_PARAM_AVCTYPE *pAvcParams =
                (OMX_VIDEO_PARAM_AVCTYPE *)pBase->pParentComp->pPorts[s_nvx_port_input].pPortPrivate;
            NvOsMemset(&oAvcParams, 0, sizeof(NvMMVideoEncH264Params));
            oAvcParams.StructSize = sizeof(NvMMVideoEncH264Params);
            if (!pNvxVideoEncoder->nVirtualBufferSize)
            {
                oAvcParams.VirtualBufferSize = (pNvxVideoEncoder->nBitRate/100) << 8;
                pNvxVideoEncoder->nVirtualBufferSize = oAvcParams.VirtualBufferSize;
            }
            else
                oAvcParams.VirtualBufferSize = pNvxVideoEncoder->nVirtualBufferSize;
            oAvcParams.StreamMode = 0; // Byte stream mode = 0, NAL stream = 1
            oAvcParams.bRTPMode = (NvBool)pNvxVideoEncoder->bRTPMode;
            oAvcParams.RateCtrlMode = pNvxVideoEncoder->eRateCtrlMode;
            oAvcParams.bSvcEncode = (NvBool)pNvxVideoEncoder->bSvcEncodeEnable;   // It takes NvBool values
            oAvcParams.bInsertSPSPPSAtIDR = (NvBool)pNvxVideoEncoder->bInsertSPSPPSAtIDR;
            oAvcParams.bUseConstrainedBP = (NvBool)pNvxVideoEncoder->bUseConstrainedBP;
            oAvcParams.bInsertVUI = (NvBool)pNvxVideoEncoder->bInsertVUI;
            oAvcParams.bInsertAUD = (NvBool)pNvxVideoEncoder->bInsertAUD;
            oAvcParams.bLowLatency = (NvBool)pNvxVideoEncoder->bLowLatency;

            // Will apply NVENC onwards only
            oAvcParams.Profile = s_GetNvMMH264Profile(pAvcParams->eProfile);
            oAvcParams.IFrameInterval = pNvxVideoEncoder->nIFrameInterval;
            oAvcParams.NumBFrames = pAvcParams->nBFrames;
            oAvcParams.NumReferenceFrames = pAvcParams->nRefFrames;
            oAvcParams.bWeightedPrediction = (NvBool)(!!pAvcParams->nWeightedBipredicitonMode); // Only implicit supported for only B-frames
            oAvcParams.bCabacDisable = (NvBool)(!pAvcParams->bEntropyCodingCABAC);
            oAvcParams.DisableDeblockIdc = pAvcParams->eLoopFilterMode; // OMX and NvMM header values match
            oAvcParams.bSliceIntraRefreshEnable = (NvBool)pNvxVideoEncoder->bSliceIntraRefreshEnable;
            oAvcParams.SliceIntraRefreshInterval = pNvxVideoEncoder->SliceIntraRefreshInterval;
            oAvcParams.bCyclicIntraRefreshEnable = (NvBool)pNvxVideoEncoder->bCyclicIntraRefreshEnable;
            oAvcParams.Cirmbs = pNvxVideoEncoder->Cirmbs;
            oAvcParams.bChpBitsDisable = NV_TRUE;

            err = pBase->hBlock->SetAttribute( pBase->hBlock,
                NvMMAttributeVideoEnc_H264StreamProperty,
                0,
                oAvcParams.StructSize,
                &oAvcParams);
            if (err != NvSuccess)
                return OMX_ErrorBadParameter;
        }
        break;
        case TYPE_VP8:
        {
            NvMMVideoEncVP8Params oVP8Params;
            NVX_VIDEO_PARAM_VP8TYPE *pVP8Params =
                (NVX_VIDEO_PARAM_VP8TYPE *)pBase->pParentComp->pPorts[s_nvx_port_input].pPortPrivate;
            NvOsMemset(&oVP8Params, 0, sizeof(NvMMVideoEncVP8Params));
            oVP8Params.StructSize = sizeof(NvMMVideoEncVP8Params);
            oVP8Params.VirtualBufferSize = pNvxVideoEncoder->nVirtualBufferSize = pNvxVideoEncoder->nBitRate;
            oVP8Params.RateCtrlMode = pNvxVideoEncoder->eRateCtrlMode;
            oVP8Params.NumReferenceFrames = 1;
            oVP8Params.version = pVP8Params->eLevel;
            oVP8Params.filter_type = 0;
            oVP8Params.filter_level = pVP8Params->filter_level;
            oVP8Params.sharpness_level = pVP8Params->sharpness_level;
            err = pBase->hBlock->SetAttribute( pBase->hBlock,
                NvMMAttributeVideoEnc_VP8StreamProperty,
                0,
                oVP8Params.StructSize,
                &oVP8Params);
            if (err != NvSuccess)
                return OMX_ErrorBadParameter;
        }
        break;
        default: break;
    }
    NVXTRACE((NVXT_CALLGRAPH, NVXT_VIDEO_ENCODER,"-NvxVideoEncoderSetNvMMAttributes"));
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxVideoEncoderAcquireResources(OMX_IN NvxComponent *pComponent)
{
    SNvxLiteVideoEncoderData *pNvxVideoEncoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pVideoPort = NULL;
    FILE* fp;
    NvU32 maxInputBuffers;
    NvBool bEnable;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxLiteVideoEncoderAcquireResources\n"));
    /* Get encoder instance */
    pNvxVideoEncoder = (SNvxLiteVideoEncoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxVideoEncoder);

    eError = NvxComponentBaseAcquireResources(pComponent);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pComponent->eObjectType, "ERROR:NvxVideoEncoderAcquireResources:"
           ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }
    if (!pNvxVideoEncoder->oBase.hBlock)
    {
        eError = NvxNvMMLiteTransformOpen(&pNvxVideoEncoder->oBase, pNvxVideoEncoder->oBlockType, pNvxVideoEncoder->sBlockName, s_nvx_num_ports, pComponent);
        if (eError != OMX_ErrorNone)
        {
            NvOsDebugPrintf("%s : NvxNvMMLiteTransformOpen failed \n", __func__);
            return eError;
        }
    }
    if (!pNvxVideoEncoder->oBase.oPorts[s_nvx_port_input].bHasSize)
    {
        // If we don't nvmm tunnel, then allocate surface inputs
        pNvxVideoEncoder->oBase.oPorts[s_nvx_port_input].bHasSize = OMX_TRUE;
        pNvxVideoEncoder->oBase.oPorts[s_nvx_port_input].nWidth = pNvxVideoEncoder->nSurfWidth;
        pNvxVideoEncoder->oBase.oPorts[s_nvx_port_input].nHeight = pNvxVideoEncoder->nSurfHeight;
        pNvxVideoEncoder->oBase.oPorts[s_nvx_port_input].bRequestSurfaceBuffers = OMX_TRUE;
    }

    if (!pNvxVideoEncoder->oBase.oPorts[s_nvx_port_output].bHasSize)
    {
        pNvxVideoEncoder->oBase.oPorts[s_nvx_port_output].bHasSize = OMX_TRUE;
        pNvxVideoEncoder->oBase.oPorts[s_nvx_port_output].nWidth = pNvxVideoEncoder->nSurfWidth;
        pNvxVideoEncoder->oBase.oPorts[s_nvx_port_output].nHeight = pNvxVideoEncoder->nSurfHeight;
    }

    // Normally, use NvOs memory for output buffers
    // but NvRmMemHandle in VPR if Video Protection is enabled
    pNvxVideoEncoder->oBase.bInSharedMemory = pNvxVideoEncoder->oBase.bVideoProtect;

    pVideoPort = &pComponent->pPorts[s_nvx_port_output];
    if (pVideoPort->bNvidiaTunneling)
    {
       NVX_VIDEO_CONFIG_H264NALLEN oNalLen;
       eError = OMX_GetConfig( pVideoPort->hTunnelComponent,
           NVX_IndexConfigH264NALLen,
           &oNalLen);
       if (eError == OMX_ErrorNone)
       {
           // Only take action if downstream component supports this mode.
           // Otherwise, continue as normal
           if (oNalLen.nNalLen > 0)
              pNvxVideoEncoder->bRTPMode = OMX_TRUE;
       }
    }

    if (pNvxVideoEncoder->pTempFilePath)
    {
        fp = fopen((const char *)pNvxVideoEncoder->pTempFilePath,"r");
        if (fp != NULL)
        {
            NvDebugOmx(("opened enctune.conf successfully\n"));
            NvxParseEncoderCfg(fp,pNvxVideoEncoder);
            fclose(fp);
        }
    }

    eError = NvxVideoEncoderSetNvMMAttributes(&pNvxVideoEncoder->oBase,
        pNvxVideoEncoder);
    if (eError != OMX_ErrorNone)
        {
            NvOsDebugPrintf("NvxVideoEncoderSetNvMMAttributes failed");
            return eError;
        }

    eError = NvxNvMMLiteTransformSetupInput(&pNvxVideoEncoder->oBase,
        s_nvx_port_input,
        &pComponent->pPorts[s_nvx_port_input],
        MAX_INPUT_BUFFERS,
        MIN_INPUT_BUFFERSIZE,
        OMX_FALSE);
    if (eError != OMX_ErrorNone)
        {
            NvOsDebugPrintf("NvxNvMMLiteTransformSetupInput failed");
            return eError;
        }

    eError = NvxNvMMLiteTransformSetupOutput(&pNvxVideoEncoder->oBase,
        s_nvx_port_output,
        pVideoPort,
        s_nvx_port_input,
        (pNvxVideoEncoder->bSliceLevelEncode ? (MAX_OUTPUT_BUFFERS * 5) : MAX_OUTPUT_BUFFERS),
        pNvxVideoEncoder->nMinOutputBufferSize);
    if (eError != OMX_ErrorNone)
        {
            NvOsDebugPrintf("NvxNvMMLiteTransformSetupOutput failed");
            return eError;
        }

    pNvxVideoEncoder->bFirstFrame = OMX_TRUE;
    pNvxVideoEncoder->bInitialized = OMX_TRUE;

    s_SetTemporalTradeOff(pNvxVideoEncoder);
    s_SetInputRotation(pNvxVideoEncoder);
    s_SetInputMirroring(pNvxVideoEncoder);

    if(pNvxVideoEncoder->FlagCropRect)
        s_SetH264OutputCrop(pNvxVideoEncoder);

    if(!pNvxVideoEncoder->nSliceHeaderSpacing)
    {
        //spacing = 0x7fffffff will let encoder packet = total number of MBs
        //This will do single NAL unit encoding as required by SF writer.
        if (pNvxVideoEncoder->bSliceLevelEncode)
            if (pNvxVideoEncoder->bBitBasedPacketization)
                pNvxVideoEncoder->nSliceHeaderSpacing = \
                    (pNvxVideoEncoder->nBitRate / ((pNvxVideoEncoder->nFrameRate * 3)>>1));
            else
                pNvxVideoEncoder->nSliceHeaderSpacing = \
                    ((pNvxVideoEncoder->nSurfWidth * pNvxVideoEncoder->nSurfHeight) >> 10);
        else
            pNvxVideoEncoder->nSliceHeaderSpacing = 0x7fffffff;
        s_SetNvMmVideoEncPacketMode(pNvxVideoEncoder);
    }
    else
        s_SetNvMmVideoEncPacketMode(pNvxVideoEncoder);

    s_SetNvMmVideoEncInitialQuant(pNvxVideoEncoder);

    s_SetNvMmVideoEncQuantRange(pNvxVideoEncoder);

    if (pNvxVideoEncoder->oType == TYPE_H264)
    {
        pNvxVideoEncoder->oBase.hBlock->SetAttribute(pNvxVideoEncoder->oBase.hBlock,
                NvMMAttributeVideoEnc_H264QualityParams,
                0,
                sizeof(NvMMH264EncQualityParams),
                &pNvxVideoEncoder->H264EncQualityParams);

        // check slice level encoding is enabled at the driver
        if (pNvxVideoEncoder->bSliceLevelEncode)
        {
            NvMMVideoEncBitStreamProperty oVideoParams;

            NvOsMemset(&oVideoParams, 0, sizeof(NvMMVideoEncBitStreamProperty));
            oVideoParams.StructSize  = sizeof(NvMMVideoEncBitStreamProperty);

            pNvxVideoEncoder->oBase.hBlock->GetAttribute(pNvxVideoEncoder->oBase.hBlock,
                    NvMMAttributeVideoEnc_CommonStreamProperty,
                    sizeof(NvMMVideoEncBitStreamProperty),
                    &oVideoParams);
            pNvxVideoEncoder->bSliceLevelEncode = oVideoParams.bSliceLevelEncode;
        }

        s_SetActiveSliceLevelEncode(pNvxVideoEncoder);
    }
    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "-NvxLiteVideoEncoderAcquireResources\n"));

    maxInputBuffers = MAX_INPUT_BUFFERS;
    pNvxVideoEncoder->oBase.hBlock->SetAttribute(pNvxVideoEncoder->oBase.hBlock,
                                                NvMMAttributeVideoEnc_MaxInputBuffers,
                                                0, sizeof(NvU32), &maxInputBuffers);

    if (pNvxVideoEncoder->bSuperFineQuality)
    {
        NvU32 Enable = NV_TRUE;
        pNvxVideoEncoder->oBase.hBlock->SetAttribute(pNvxVideoEncoder->oBase.hBlock,
                NvMMAttributeVideoEnc_EnableNoiseReduction,
                0, sizeof(NvU32), &Enable);
    }

    bEnable = (pNvxVideoEncoder->bSkipFrame == OMX_TRUE) ? NV_TRUE : NV_FALSE;
    pNvxVideoEncoder->oBase.hBlock->SetAttribute(pNvxVideoEncoder->oBase.hBlock,
                                                 NvMMAttributeVideoEnc_SkipFrame,
                                                 0, sizeof(NvBool),
                                                 &bEnable);
    return eError;
}

static OMX_ERRORTYPE NvxVideoEncoderReleaseResources(OMX_IN NvxComponent *pComponent)
{
    SNvxLiteVideoEncoderData *pNvxVideoEncoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxLiteVideoEncoderReleaseResources\n"));

    /* Get encoder instance */
    pNvxVideoEncoder = (SNvxLiteVideoEncoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxVideoEncoder);

    if (!pNvxVideoEncoder->bInitialized)
        return OMX_ErrorNone;

    eError = NvxNvMMLiteTransformClose(&pNvxVideoEncoder->oBase);

    if (eError != OMX_ErrorNone)
        return eError;

    pNvxVideoEncoder->bInitialized = OMX_FALSE;

    NvxCheckError(eError, NvxComponentBaseReleaseResources(pComponent));
    return eError;
}

static OMX_ERRORTYPE NvxVideoEncoderFlush(OMX_IN NvxComponent *pComponent, OMX_U32 nPort)
{
    SNvxLiteVideoEncoderData *pNvxVideoEncoder = 0;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxLiteVideoEncoderFlush\n"));

    /* Get video encoder instance */
    pNvxVideoEncoder = (SNvxLiteVideoEncoderData *)pComponent->pComponentData;
    NV_ASSERT(pNvxVideoEncoder);

    return NvxNvMMLiteTransformFlush(&pNvxVideoEncoder->oBase, nPort);
}

static OMX_ERRORTYPE NvxVideoEncoderPreCheckChangeState(OMX_IN NvxComponent *pNvComp,OMX_IN OMX_STATETYPE oNewState)
{
    NV_ASSERT(pNvComp);
    if (oNewState == OMX_StateIdle && pNvComp->eState == OMX_StateExecuting)
    {
        SNvxLiteVideoEncoderData *pNvxVideoEncoder;
        pNvxVideoEncoder = (SNvxLiteVideoEncoderData *)pNvComp->pComponentData;
        NV_ASSERT(pNvxVideoEncoder);
        pNvxVideoEncoder->oBase.bStopping= OMX_TRUE;
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxVideoEncoderFillThisBuffer(NvxComponent *pNvComp, OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    SNvxLiteVideoEncoderData *pNvxVideoEncoder = 0;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxLiteVideoEncoderFillThisBuffer\n"));

    pNvxVideoEncoder = (SNvxLiteVideoEncoderData *)pNvComp->pComponentData;
    if (!pNvxVideoEncoder->bInitialized)
        return OMX_ErrorNone;

    return NvxNvMMLiteTransformFillThisBuffer(&pNvxVideoEncoder->oBase, pBufferHdr, s_nvx_port_output);
}

static OMX_ERRORTYPE NvxVideoEncoderPreChangeState(NvxComponent *pNvComp,
                                                   OMX_STATETYPE oNewState)
{
    SNvxLiteVideoEncoderData *pNvxVideoEncoder = 0;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxLiteVideoEncoderPreChangeState\n"));

    pNvxVideoEncoder = (SNvxLiteVideoEncoderData *)pNvComp->pComponentData;
    if (!pNvxVideoEncoder->bInitialized)
        return OMX_ErrorNone;

    return NvxNvMMLiteTransformPreChangeState(&pNvxVideoEncoder->oBase, oNewState,
                                          pNvComp->eState);
}

static OMX_ERRORTYPE NvxVideoEncoderEmptyThisBuffer(NvxComponent *pNvComp,
                                             OMX_BUFFERHEADERTYPE* pBufferHdr,
                                             OMX_BOOL *bHandled)
{
    SNvxLiteVideoEncoderData *pNvxVideoEncoder = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxLiteVideoEncoderEmptyThisBuffer\n"));

    pNvxVideoEncoder = (SNvxLiteVideoEncoderData *)pNvComp->pComponentData;
    if (!pNvxVideoEncoder->bInitialized)
        return OMX_ErrorNone;

    if (NvxNvMMLiteTransformIsInputSkippingWorker(&pNvxVideoEncoder->oBase,
                                              s_nvx_port_input))
    {
        eError = NvxPortEmptyThisBuffer(&pNvComp->pPorts[pBufferHdr->nInputPortIndex], pBufferHdr);
        *bHandled = (eError == OMX_ErrorNone);
    }
    else
        *bHandled = OMX_FALSE;

    return eError;
}

/*****************************************************************************/

static OMX_ERRORTYPE NvxCommonVideoEncoderInit(OMX_HANDLETYPE hComponent, NvMMLiteBlockType oBlockType, const char *sBlockName)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent* pNvComp;
    SNvxLiteVideoEncoderData *data = NULL;

    eError = NvxComponentCreate(hComponent, s_nvx_num_ports, &pNvComp);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,NVXT_VIDEO_ENCODER,"ERROR:NvxCommonVideoEncoderInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp->eObjectType = NVXT_VIDEO_ENCODER;

    pNvComp->pComponentData = NvOsAlloc(sizeof(SNvxLiteVideoEncoderData));
    if (!pNvComp->pComponentData)
        return OMX_ErrorInsufficientResources;

    data = (SNvxLiteVideoEncoderData *)pNvComp->pComponentData;
    NvOsMemset(data, 0, sizeof(SNvxLiteVideoEncoderData));

    switch (oBlockType)
    {
        case NvMMLiteBlockType_EncMPEG4:
            data->oType = TYPE_MPEG4;
            break;
        case NvMMLiteBlockType_EncH264:
            data->oType = TYPE_H264;
            break;
        case NvMMLiteBlockType_EncH263:
            oBlockType = NvMMLiteBlockType_EncMPEG4;
            data->oType = TYPE_H263;
            break;
        case NvMMLiteBlockType_EncVP8:
            data->oType = TYPE_VP8;
            break;
        default: return OMX_ErrorBadParameter;
    }

    data->oBlockType = oBlockType;
    data->sBlockName = sBlockName;
    data->nSurfWidth  = 176; // default width/height
    data->nSurfHeight = 144;
    data->nMinOutputBufferSize = s_GET_BUFFER_SIZE(data->nSurfWidth, data->nSurfHeight);
    data->nBitRate = 1000000;
    data->nPeakBitrate = 0;     // if set to 0, then encoder driver derive PeakBitrate from level idc
    data->nFrameRate = 30;
    data->bStringentBitrate = OMX_FALSE;
    data->bFrameSkip = OMX_FALSE;
    data->bAllIFrames = OMX_FALSE;
    data->bBitBasedPacketization = OMX_FALSE;
    data->bSvcEncodeEnable = OMX_FALSE;
    data->bInsertSPSPPSAtIDR = OMX_FALSE;
    data->bUseConstrainedBP = OMX_FALSE;
    data->bInsertVUI = OMX_FALSE;
    data->bInsertAUD = OMX_FALSE;
    data->bEnableStuffing = OMX_FALSE;
    data->bLowLatency = OMX_FALSE;
    data->bSliceLevelEncode = OMX_FALSE;
    data->bRTPMode = OMX_FALSE; // default to bytestream mode (elem format recognized by conformance tests)
    data->eAppConfig = NvMMVideoEncAppType_Camcorder; // camcorder or telephony
    data->eQualityLevel = NvMMVideoEncQualityLevel_High;
    data->eResilLevel = NvMMVideoEncErrorResiliencyLevel_None;
    data->bSetMaxEncClock = OMX_FALSE;
    data->TemporalTradeOffLevel = 0;
    data->RotateAngle = 0;
    data->Mirror = OMX_MirrorNone;
    data->nIFrameInterval =
    data->nAVCIDRFrameInterval = 60;
    data->eRateCtrlMode = NvMMVideoEncRateCtrlMode_CBR;
    data->nSliceHeaderSpacing = 0;
    data->nInitQPI = 24;
    data->nInitQPP = 24;
    data->nInitQPB = 28;
    data->bTimeLapse = OMX_FALSE;
    data->bSliceIntraRefreshEnable = OMX_FALSE;
    data->SliceIntraRefreshInterval = 0;
    data->Cirmbs = 0;
    data->bCyclicIntraRefreshEnable = OMX_FALSE;
    data->bSuperFineQuality = OMX_FALSE;
    data->bSkipFrame = OMX_FALSE;
    data->nVirtualBufferSize = 0;

    pNvComp->DeInit             = NvxVideoEncoderDeInit;
    pNvComp->GetParameter       = NvxVideoEncoderGetParameter;
    pNvComp->SetParameter       = NvxVideoEncoderSetParameter;
    pNvComp->GetConfig          = NvxVideoEncoderGetConfig;
    pNvComp->SetConfig          = NvxVideoEncoderSetConfig;
    pNvComp->WorkerFunction     = NvxVideoEncoderWorkerFunction;
    pNvComp->AcquireResources   = NvxVideoEncoderAcquireResources;
    pNvComp->ReleaseResources   = NvxVideoEncoderReleaseResources;
    pNvComp->FillThisBufferCB   = NvxVideoEncoderFillThisBuffer;
    pNvComp->PreChangeState     = NvxVideoEncoderPreChangeState;
    pNvComp->EmptyThisBuffer    = NvxVideoEncoderEmptyThisBuffer;
    pNvComp->Flush              = NvxVideoEncoderFlush;
    pNvComp->PreCheckChangeState = NvxVideoEncoderPreCheckChangeState;

    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_input],
        OMX_DirInput,
        MAX_INPUT_BUFFERS,
        MIN_INPUT_BUFFERSIZE,
        OMX_VIDEO_CodingUnused);

    pNvComp->pPorts[s_nvx_port_input].oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;

    pNvComp->pPorts[s_nvx_port_input].eNvidiaTunnelTransaction = ENVX_TRANSACTION_GFSURFACES;

    if (!data->pTempFilePath)
    {
        data->pTempFilePath = NvOsAlloc(NvOsStrlen("/system/etc/enctune.conf") + 1);
        if (data->pTempFilePath)
        {
            NvOsStrncpy(data->pTempFilePath,"/system/etc/enctune.conf",(NvOsStrlen("/system/etc/enctune.conf") + 1));
        }
    }
    return OMX_ErrorNone;
}

/***************************************************************************/

OMX_ERRORTYPE NvxLiteMpeg4EncoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = NULL;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    OMX_VIDEO_PARAM_MPEG4TYPE *pVideoParams;
    SNvxLiteVideoEncoderData *data;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxLiteMpeg4EncoderInit\n"));

    eError = NvxCommonVideoEncoderInit(hComponent, NvMMLiteBlockType_EncMPEG4, "BlockM4vEnc");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxJpegEncoderInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ALLOC_STRUCT(pNvComp, pVideoParams, OMX_VIDEO_PARAM_MPEG4TYPE);
    if (!pVideoParams)
        return OMX_ErrorInsufficientResources;

    pNvComp->pComponentName = "OMX.Nvidia.mp4.encoder";
    pNvComp->sComponentRoles[0] = "video_encoder.mpeg4";
    pNvComp->nComponentRoles    = 1;

    data = (SNvxLiteVideoEncoderData *)pNvComp->pComponentData;
    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_output], OMX_DirOutput,
                     MAX_OUTPUT_BUFFERS, data->nMinOutputBufferSize, OMX_VIDEO_CodingMPEG4);

    // Set MPEG4 specific parameters:
    pVideoParams->eProfile = OMX_VIDEO_MPEG4ProfileSimple;
    pVideoParams->eLevel = OMX_VIDEO_MPEG4Level5;
    data->eLevel = OMX_VIDEO_MPEG4LevelMax;
    pVideoParams->bSVH = OMX_FALSE;
    pVideoParams->nIDCVLCThreshold = 0;
    pVideoParams->bACPred = OMX_TRUE;
    pVideoParams->bReversibleVLC = OMX_FALSE;
    pVideoParams->nHeaderExtension = 0;
    pVideoParams->bGov = OMX_FALSE;
    pVideoParams->nAllowedPictureTypes = 2; // check
    pVideoParams->nBFrames = 2;
    pVideoParams->nHeaderExtension = 0;
    pVideoParams->nIDCVLCThreshold = 0;
    pVideoParams->nMaxPacketSize = 2048;
    pVideoParams->nPFrames = 29;
    pVideoParams->nSliceHeaderSpacing = 0;
    pVideoParams->nTimeIncRes = 50;
    //Assigned invalid value for MinQP/MaxQP to not affect internal RC params
    data->nMinQpI = 0xff;
    data->nMaxQpI = 0xff;
    data->nMinQpP = 0xff;
    data->nMaxQpP = 0xff;
    data->nMinQpB = 0xff;   // NVENC onwards only
    data->nMaxQpB = 0xff;   // NVENC onwards only

    pNvComp->pPorts[s_nvx_port_input].pPortPrivate = pVideoParams;

    return eError;
}

OMX_ERRORTYPE NvxLiteH264EncoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = NULL;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    OMX_VIDEO_PARAM_AVCTYPE *pAvcParams;
    SNvxLiteVideoEncoderData *data;

    // This is purely for debugging purposes: does not require hardware to test
    // encoder framework

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxLiteH264EncoderInit\n"));
    eError = NvxCommonVideoEncoderInit(hComponent, NvMMLiteBlockType_EncH264, "BlockAvcEnc");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxLiteH264EncoderInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ALLOC_STRUCT(pNvComp, pAvcParams, OMX_VIDEO_PARAM_AVCTYPE);
    if (!pAvcParams)
        return OMX_ErrorInsufficientResources;

    pNvComp->pComponentName = "OMX.Nvidia.h264.encoder";
    pNvComp->sComponentRoles[0] = "video_encoder.avc";
    pNvComp->nComponentRoles    = 1;

    data = (SNvxLiteVideoEncoderData *)pNvComp->pComponentData;
    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_output], OMX_DirOutput,
                     MAX_OUTPUT_BUFFERS, data->nMinOutputBufferSize, OMX_VIDEO_CodingAVC);

    pAvcParams->eProfile = OMX_VIDEO_AVCProfileBaseline;
    pAvcParams->eLevel = OMX_VIDEO_AVCLevel4; // OMX_VIDEO_AVCLevel1;
    data->eLevel = OMX_VIDEO_AVCLevelMax;
    pAvcParams->bEntropyCodingCABAC = OMX_FALSE;
    pAvcParams->bUseHadamard = OMX_TRUE;
    pAvcParams->nPFrames = 29;
    pAvcParams->nBFrames = 0;
    pAvcParams->nRefFrames = 1;
    pAvcParams->bEnableFMO = OMX_FALSE;
    pAvcParams->bEnableASO = OMX_FALSE;
    pAvcParams->bWeightedPPrediction = OMX_FALSE;
    pAvcParams->bconstIpred = OMX_FALSE;

    //Assigned invalid value for MinQP/MaxQP to not affect internal RC params
    data->nMinQpI = 0xff;
    data->nMaxQpI = 0xff;
    data->nMinQpP = 0xff;
    data->nMaxQpP = 0xff;
    data->nMinQpB = 0xff;   // NVENC onwards only
    data->nMaxQpB = 0xff;   // NVENC onwards only

    data->H264EncQualityParams.StructSize = sizeof(NvMMH264EncQualityParams);
    data->H264EncQualityParams.FavorIntraBias = 0;
    data->H264EncQualityParams.FavorInterBias = 640;
    data->H264EncQualityParams.FavorIntraBias_16X16 = 256;

    pNvComp->pPorts[s_nvx_port_input].pPortPrivate = pAvcParams;

    return eError;
}

OMX_ERRORTYPE NvxLiteH263EncoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = NULL;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    OMX_VIDEO_PARAM_H263TYPE *pH263Params;
    SNvxLiteVideoEncoderData *data;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxLiteH263EncoderInit\n"));

    eError = NvxCommonVideoEncoderInit(hComponent, NvMMLiteBlockType_EncH263, "Block263Enc");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxLiteH263EncoderInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ALLOC_STRUCT(pNvComp, pH263Params, OMX_VIDEO_PARAM_H263TYPE);
    if (!pH263Params)
        return OMX_ErrorInsufficientResources;

    pNvComp->pComponentName = "OMX.Nvidia.h263.encoder";
    pNvComp->sComponentRoles[0] = "video_encoder.h263";
    pNvComp->nComponentRoles    = 1;

    data = (SNvxLiteVideoEncoderData *)pNvComp->pComponentData;
    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_output], OMX_DirOutput,
                     MAX_OUTPUT_BUFFERS, data->nMinOutputBufferSize, OMX_VIDEO_CodingH263);

    // Set H263 specific parameters:
    pH263Params->eProfile = OMX_VIDEO_H263ProfileBaseline;
    pH263Params->eLevel = OMX_VIDEO_H263Level10;
    data->eLevel = OMX_VIDEO_H263Level10;
    pH263Params->bPLUSPTYPEAllowed = OMX_FALSE;
    pH263Params->nAllowedPictureTypes = 2; // check
    pH263Params->bForceRoundingTypeToZero = OMX_TRUE;
    pH263Params->nPictureHeaderRepetition = 1;
    pH263Params->nGOBHeaderInterval = 0;
    //Assigned invalid value for MinQP/MaxQP to not affect internal RC params
    data->nMinQpI = 0xff;
    data->nMaxQpI = 0xff;
    data->nMinQpP = 0xff;
    data->nMaxQpP = 0xff;

    pNvComp->pPorts[s_nvx_port_input].pPortPrivate = pH263Params;

    return eError;
}

OMX_ERRORTYPE NvxLiteVP8EncoderInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = NULL;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    NVX_VIDEO_PARAM_VP8TYPE *pVP8Params;
    SNvxLiteVideoEncoderData *data;

    // This is purely for debugging purposes: does not require hardware to test
    // encoder framework

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxLiteVP8EncoderInit\n"));

    if (NvMMVP8Support() != NvSuccess)
    {
        eError = OMX_ErrorComponentNotFound;
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxLiteVP8EncoderInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    eError = NvxCommonVideoEncoderInit(hComponent, NvMMLiteBlockType_EncVP8, "BlockVP8Enc");
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxLiteVP8EncoderInit:"
          ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp = (NvxComponent *)(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate);
    ALLOC_STRUCT(pNvComp, pVP8Params, NVX_VIDEO_PARAM_VP8TYPE);
    if (!pVP8Params)
        return OMX_ErrorInsufficientResources;

    pNvComp->pComponentName = "OMX.Nvidia.vp8.encoder";
    pNvComp->sComponentRoles[0] = "video_encoder.vp8";
    pNvComp->sComponentRoles[1] = "video_encoder.vpx";
    pNvComp->nComponentRoles    = 2;

    data = (SNvxLiteVideoEncoderData *)pNvComp->pComponentData;
    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_output], OMX_DirOutput,
                     MAX_OUTPUT_BUFFERS, data->nMinOutputBufferSize, NVX_VIDEO_CodingVP8);

    pVP8Params->eProfile = NVX_VIDEO_VP8ProfileMain;
    pVP8Params->eLevel = NVX_VIDEO_VP8Level_Version0;
    data->eLevel = NVX_VIDEO_VP8Level_Version0;
    pVP8Params->nDCTPartitions = 1;
    pVP8Params->bErrorResilientMode = OMX_FALSE;
    pVP8Params->nPFrames = 29;
    pVP8Params->filter_level = 47;
    pVP8Params->sharpness_level = 0;

    //Assigned invalid value for MinQP/MaxQP to not affect internal RC params
    data->nMinQpI = 0xff;
    data->nMaxQpI = 0xff;
    data->nMinQpP = 0xff;
    data->nMaxQpP = 0xff;

    pNvComp->pPorts[s_nvx_port_input].pPortPrivate = pVP8Params;

    return eError;
}

#if !(USE_ANDROID_NATIVE_WINDOW)
struct StoreMetaDataInBuffersParams {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bStoreMetaData;
};

static OMX_ERRORTYPE HandleStoreMetaDataInBuffersParamLite(void *pParam,
                                                SNvxNvMMLitePort *pPort,
                                                OMX_U32 PortAllowed)
{
    struct StoreMetaDataInBuffersParams *pStoreMetaData =
        (struct StoreMetaDataInBuffersParams*)pParam;

    if (!pStoreMetaData || pStoreMetaData->nPortIndex != PortAllowed)
        return OMX_ErrorBadParameter;

    pPort[PortAllowed].bUsesAndroidMetaDataBuffers =
        pStoreMetaData->bStoreMetaData;
    pPort[PortAllowed].bEmbedNvMMBuffer = pStoreMetaData->bStoreMetaData;

    return OMX_ErrorNone;
}

#endif

static void NvxParseEncoderCfg(FILE *fp, SNvxLiteVideoEncoderData *pNvxVideoEncoder)
{
    char lineTrace[100];
    char Actresolution[32];
    char encFormat[32];
    NvU32 val;
    NvU32 len;
    char Tag[64];

    if ( pNvxVideoEncoder->oType == TYPE_H264 )
    {
        NvOsStrncpy(encFormat,"H264:",6);
    }
    else if ( pNvxVideoEncoder->oType == TYPE_VP8)
    {
        NvOsStrncpy(encFormat,"VP8:",5);
    }
    else
    {
        NvOsStrncpy(encFormat,"MPEG4:",7);
    }

    len = NvOsStrlen(encFormat);

    fgets(lineTrace,100,fp);
    while ((NvOsStrncmp(lineTrace,encFormat,len) != 0))
    {
        char *temp;
        temp = fgets(lineTrace,32,fp);

        if (temp == NULL)
            return;
    }

    if (pNvxVideoEncoder->eAppConfig == NvMMVideoEncAppType_VideoTelephony)
    {
            NvOsStrncpy(Actresolution,"Settings_Video_Telephony",25);
    }
    else if (pNvxVideoEncoder->nSurfHeight > 720)
    {
        NvOsStrncpy(Actresolution,"Settings_1080P",15);
    }
    else if (pNvxVideoEncoder->nSurfHeight == 720)
    {
        NvOsStrncpy(Actresolution,"Settings_720P",14);
    }
    else
    {
        NvOsStrncpy(Actresolution,"Settings_Default",17);
    }

    len = NvOsStrlen(Actresolution);

    fgets(lineTrace,100,fp);
    while ((NvOsStrncmp(lineTrace,Actresolution,len) != 0))
    {
        char *temp;
        temp = fgets(lineTrace,32,fp);

        if (temp == NULL)
            return;
    }

    while(!feof(fp))
    {
        NvU32 Value = 0;
        fflush(fp);
        fscanf(fp,"%s",Tag);

        if (!NvOsStrcmp(Tag,"IMinQP"))
        {
            fscanf(fp,"%ud",(unsigned int *)&pNvxVideoEncoder->nMinQpI);
            NvDebugOmx(("%s = %d\n",Tag,pNvxVideoEncoder->nMinQpI));
        }
        else if (!NvOsStrcmp(Tag,"IMaxQP"))
        {
            fscanf(fp,"%ud",(unsigned int *)&pNvxVideoEncoder->nMaxQpI);
            NvDebugOmx(("%s = %d\n",Tag,pNvxVideoEncoder->nMaxQpI));
        }
        else if (!NvOsStrcmp(Tag,"PMinQP"))
        {
            fscanf(fp,"%ud",(unsigned int *)&pNvxVideoEncoder->nMinQpP);
            NvDebugOmx(("%s = %d\n",Tag,pNvxVideoEncoder->nMinQpP));
        }
        else if (!NvOsStrcmp(Tag,"PMaxQP"))
        {
            fscanf(fp,"%ud",(unsigned int *)&pNvxVideoEncoder->nMaxQpP);
            NvDebugOmx(("%s = %d\n",Tag,pNvxVideoEncoder->nMaxQpP));
        }
        else if (!NvOsStrcmp(Tag,"BMinQP")) // NVENC onwards only
        {
            fscanf(fp,"%ud",(unsigned int *)&pNvxVideoEncoder->nMinQpB);
            NvDebugOmx(("%s = %d\n",Tag,pNvxVideoEncoder->nMinQpB));
        }
        else if (!NvOsStrcmp(Tag,"BMaxQP")) // NVENC onwards only
        {
            fscanf(fp,"%ud",(unsigned int *)&pNvxVideoEncoder->nMaxQpB);
            NvDebugOmx(("%s = %d\n",Tag,pNvxVideoEncoder->nMaxQpB));
        }
        else if (!NvOsStrcmp(Tag,"InitQP"))
        {
            fscanf(fp,"%ud",(unsigned int *)&pNvxVideoEncoder->nInitQPI);
            NvDebugOmx(("%s = %d\n",Tag,pNvxVideoEncoder->nInitQPI));
        }
        else if (!NvOsStrcmp(Tag,"PInitQP"))
        {
            fscanf(fp,"%ud",(unsigned int *)&pNvxVideoEncoder->nInitQPP);
            NvDebugOmx(("%s = %d\n",Tag,pNvxVideoEncoder->nInitQPP));
        }
        else if (!NvOsStrcmp(Tag,"BInitQP"))
        {
            fscanf(fp,"%ud",(unsigned int *)&pNvxVideoEncoder->nInitQPB);
            NvDebugOmx(("%s = %d\n",Tag,pNvxVideoEncoder->nInitQPB));
        }
        else if (!NvOsStrcmp(Tag,"VBR"))
        {
            fscanf(fp,"%d",&val);
            // Use this settings only if app has not asked any specific mode
            if(!(pNvxVideoEncoder->nEncTuneFlags & EncTune_USE_APPDefined_RATE_CONTROL))
            {
                if (val == 1)
                    pNvxVideoEncoder->eRateCtrlMode = NvMMVideoEncRateCtrlMode_VBR;
                else if (val == 2)
                    pNvxVideoEncoder->eRateCtrlMode = NvMMVideoEncRateCtrlMode_VBR2;
                else
                    pNvxVideoEncoder->eRateCtrlMode = NvMMVideoEncRateCtrlMode_CBR;
            }
            NvDebugOmx(("%s = %d\n",Tag,pNvxVideoEncoder->eRateCtrlMode));
        }
        else if (!NvOsStrcmp(Tag,"QualityLevel"))
        {
            fscanf(fp,"%s",Tag);
             // Use this settings only if app has not asked any specific mode
            if(!(pNvxVideoEncoder->nEncTuneFlags & EncTune_USE_APPDefined_Quality_LEVEL))
            {
                if (!NvOsStrcmp(Tag,"low"))
                    pNvxVideoEncoder->eQualityLevel = NvMMVideoEncQualityLevel_Low;
                else if (!NvOsStrcmp(Tag,"high"))
                    pNvxVideoEncoder->eQualityLevel = NvMMVideoEncQualityLevel_High;
                else if (!NvOsStrcmp(Tag,"medium"))
                    pNvxVideoEncoder->eQualityLevel = NvMMVideoEncQualityLevel_Medium;
            }
        }
        else if (!NvOsStrcmp(Tag,"level"))
        {
            fscanf(fp,"%ud",(unsigned int *)&pNvxVideoEncoder->eLevel);
            NvDebugOmx(("%s = %d\n",Tag,pNvxVideoEncoder->eLevel));
        }
        else if (!NvOsStrcmp(Tag,"packettype"))
        {
            fscanf(fp,"%ud",(unsigned int *)&Value);
            if(!(pNvxVideoEncoder->nEncTuneFlags & EncTune_USE_APPDefined_EncoderProperty))
            {
                NvDebugOmx(("%s = %d\n",Tag,Value));
                if (Value == 0)
                    pNvxVideoEncoder->bBitBasedPacketization = NV_FALSE;
                else
                    pNvxVideoEncoder->bBitBasedPacketization = NV_TRUE;
            }
        }
        else if (!NvOsStrcmp(Tag,"FrameSkip"))
        {
            fscanf(fp,"%ud",(unsigned int *)&Value);
            if((!(pNvxVideoEncoder->nEncTuneFlags & EncTune_USE_APPDefined_EncoderProperty)) &&
                (!(pNvxVideoEncoder->nEncTuneFlags & EncTune_USE_APPDefined_FRAME_SKIPPING)))
            {
                NvDebugOmx(("%s = %d\n",Tag,Value));
                if (Value == 0)
                    pNvxVideoEncoder->bFrameSkip = NV_FALSE;
                else
                    pNvxVideoEncoder->bFrameSkip = NV_TRUE;
            }
        }
        else if (!NvOsStrcmp(Tag,"IntraOnlyMode"))
        {
            fscanf(fp,"%ud",(unsigned int *)&Value);
            if(!(pNvxVideoEncoder->nEncTuneFlags & EncTune_USE_APPDefined_EncoderProperty))
            {
                NvDebugOmx(("%s = %d\n",Tag,Value));
                if (Value == 0)
                    pNvxVideoEncoder->bAllIFrames = NV_FALSE;
                else
                    pNvxVideoEncoder->bAllIFrames = NV_TRUE;
            }
        }
        else if (!NvOsStrcmp(Tag,"InsertSPSPPSAtIDR"))
        {
            fscanf(fp,"%ud",(unsigned int *)&Value);
            if(!(pNvxVideoEncoder->nEncTuneFlags & EncTune_USE_APPDefined_EncoderProperty))
            {
                NvDebugOmx(("%s = %d\n",Tag,Value));
                if (Value == 0)
                    pNvxVideoEncoder->bInsertSPSPPSAtIDR = NV_FALSE;
                else
                    pNvxVideoEncoder->bInsertSPSPPSAtIDR = NV_TRUE;
            }
        }
        else if (!NvOsStrcmp(Tag,"UseConstrainedBP"))
        {
            fscanf(fp,"%ud",(unsigned int *)&Value);
            if(!(pNvxVideoEncoder->nEncTuneFlags & EncTune_USE_APPDefined_EncoderProperty))
            {
                NvDebugOmx(("%s = %d\n",Tag,Value));
                if (Value == 0)
                    pNvxVideoEncoder->bUseConstrainedBP = NV_FALSE;
                else
                    pNvxVideoEncoder->bUseConstrainedBP = NV_TRUE;
            }
        }
        else if (!NvOsStrcmp(Tag,"SliceLevelEncode"))
        {
            fscanf(fp,"%ud",(unsigned int *)&Value);
            if(!(pNvxVideoEncoder->nEncTuneFlags & EncTune_USE_APPDefined_EncoderProperty))
            {
                NvDebugOmx(("%s = %d\n",Tag,Value));
                if (Value == 0)
                    pNvxVideoEncoder->bSliceLevelEncode = NV_FALSE;
                else
                    pNvxVideoEncoder->bSliceLevelEncode = NV_TRUE;
            }
        }
        else if (!NvOsStrcmp(Tag,"END"))
        {
            return;
        }
    }

    return;
}
