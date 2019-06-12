/*
 * Copyright (c) 2011 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef INCLUDED_NVDDK_VIC_H
#define INCLUDED_NVDDK_VIC_H

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "nvrm_channel.h"
#include "nvrm_chip.h"
#include "nvrm_surface.h"

#if defined(__cplusplus)
extern "C" {
#endif

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################

/* This is the space reserved in the config & exec structures. Check
 * NvDdkVicSession.MaxSlots for actual number of slots supported by hardware
 */
#define NVDDK_VIC_MAX_SLOTS_RESERVED 5

/* This is the space reserved in the config & exec structures. Check
 * NvDdkVicSession.MaxClearRects for actual number of clear rects supported
 * by hardware
 */
#define NVDDK_VIC_MAX_CLEAR_RECTS_RESERVED 8

#define NVDDK_VIC_MAX_SURFACES_PER_SLOT 8


//###########################################################################
//################################## TYPES ##################################
//###########################################################################

typedef enum
{
    NvDdkVicPixelFormat_A8,
    NvDdkVicPixelFormat_L8,
    NvDdkVicPixelFormat_A4L4,
    NvDdkVicPixelFormat_L4A4,
    NvDdkVicPixelFormat_R8,
    NvDdkVicPixelFormat_A8L8,
    NvDdkVicPixelFormat_L8A8,
    NvDdkVicPixelFormat_R8G8,
    NvDdkVicPixelFormat_G8R8,
    NvDdkVicPixelFormat_B5G6R5,
    NvDdkVicPixelFormat_R5G6B5,
    NvDdkVicPixelFormat_B6G5R5,
    NvDdkVicPixelFormat_R5G5B6,
    NvDdkVicPixelFormat_A1B5G5R5,
    NvDdkVicPixelFormat_A1R5G5B5,
    NvDdkVicPixelFormat_B5G5R5A1,
    NvDdkVicPixelFormat_R5G5B5A1,
    NvDdkVicPixelFormat_A5B5G5R1,
    NvDdkVicPixelFormat_A5R1G5B5,
    NvDdkVicPixelFormat_B5G5R1A5,
    NvDdkVicPixelFormat_R1G5B5A5,
    NvDdkVicPixelFormat_X1B5G5R5,
    NvDdkVicPixelFormat_X1R5G5B5,
    NvDdkVicPixelFormat_B5G5R5X1,
    NvDdkVicPixelFormat_R5G5B5X1,
    NvDdkVicPixelFormat_A4B4G4R4,
    NvDdkVicPixelFormat_A4R4G4B4,
    NvDdkVicPixelFormat_B4G4R4A4,
    NvDdkVicPixelFormat_R4G4B4A4,
    NvDdkVicPixelFormat_B8_G8_R8,
    NvDdkVicPixelFormat_R8_G8_B8,
    NvDdkVicPixelFormat_A8B8G8R8,
    NvDdkVicPixelFormat_A8R8G8B8,
    NvDdkVicPixelFormat_B8G8R8A8,
    NvDdkVicPixelFormat_R8G8B8A8,
    NvDdkVicPixelFormat_X8B8G8R8,
    NvDdkVicPixelFormat_X8R8G8B8,
    NvDdkVicPixelFormat_B8G8R8X8,
    NvDdkVicPixelFormat_R8G8B8X8,
    NvDdkVicPixelFormat_A2B10G10R10,
    NvDdkVicPixelFormat_A2R10G10B10,
    NvDdkVicPixelFormat_B10G10R10A2,
    NvDdkVicPixelFormat_R10G10B10A2,
    NvDdkVicPixelFormat_A4P4,
    NvDdkVicPixelFormat_P4A4,
    NvDdkVicPixelFormat_P8A8,
    NvDdkVicPixelFormat_A8P8,
    NvDdkVicPixelFormat_P8,
    NvDdkVicPixelFormat_P1,
    NvDdkVicPixelFormat_U8V8,
    NvDdkVicPixelFormat_V8U8,
    NvDdkVicPixelFormat_A8Y8U8V8,
    NvDdkVicPixelFormat_V8U8Y8A8,
    NvDdkVicPixelFormat_Y8_U8_V8,
    NvDdkVicPixelFormat_Y8_V8_U8,
    NvDdkVicPixelFormat_U8_V8_Y8,
    NvDdkVicPixelFormat_V8_U8_Y8,
    NvDdkVicPixelFormat_Y8_U8__Y8_V8,
    NvDdkVicPixelFormat_Y8_V8__Y8_U8,
    NvDdkVicPixelFormat_U8_Y8__V8_Y8,
    NvDdkVicPixelFormat_V8_Y8__U8_Y8,
    NvDdkVicPixelFormat_Y8___U8V8_N444,
    NvDdkVicPixelFormat_Y8___V8U8_N444,
    NvDdkVicPixelFormat_Y8___U8V8_N422,
    NvDdkVicPixelFormat_Y8___V8U8_N422,
    NvDdkVicPixelFormat_Y8___U8V8_N422R,
    NvDdkVicPixelFormat_Y8___V8U8_N422R,
    NvDdkVicPixelFormat_Y8___U8V8_N420,
    NvDdkVicPixelFormat_Y8___V8U8_N420,
    NvDdkVicPixelFormat_Y8___U8___V8_N444,
    NvDdkVicPixelFormat_Y8___U8___V8_N422,
    NvDdkVicPixelFormat_Y8___U8___V8_N422R,
    NvDdkVicPixelFormat_Y8___U8___V8_N420,
    NvDdkVicPixelFormat_U8,
    NvDdkVicPixelFormat_V8
} NvDdkVicPixelFormat;

typedef enum
{
    NvDdkVicSurfaceLayout_Pitch,
    NvDdkVicSurfaceLayout_BlockLinear
} NvDdkVicSurfaceLayout;

typedef enum
{
    NvDdkVicFrameFormat_Progressive,
    NvDdkVicFrameFormat_InterlacedTopFieldFirst,
    NvDdkVicFrameFormat_InterlacedBottomFieldFirst,
    NvDdkVicFrameFormat_TopField,
    NvDdkVicFrameFormat_BottomField,
    NvDdkVicFrameFormat_SubpicProgressive,
    NvDdkVicFrameFormat_SubpicInterlacedTopFieldFirst,
    NvDdkVicFrameFormat_SubpicInterlacedBottomFieldFirst,
    NvDdkVicFrameFormat_SubpicTopField,
    NvDdkVicFrameFormat_SubpicBottomField,
    NvDdkVicFrameFormat_TopFieldChromaBottom,
    NvDdkVicFrameFormat_BottomFieldChromaTop,
    NvDdkVicFrameFormat_SubpicTopFieldChromaBottom,
    NvDdkVicFrameFormat_SubpicBottomFieldChromaTop
} NvDdkVicFrameFormat;

typedef enum
{
    NvDdkVicBlendSrcFactC_K1,
    NvDdkVicBlendSrcFactC_K1_Times_DstA,
    NvDdkVicBlendSrcFactC_One_Minus_K1_Times_DstA,
    NvDdkVicBlendSrcFactC_K1_Times_SrcA,
    NvDdkVicBlendSrcFactC_Zero
} NvDdkVicBlendSrcFactC;

typedef enum
{
    NvDdkVicBlendDstFactC_K1,
    NvDdkVicBlendDstFactC_K2,
    NvDdkVicBlendDstFactC_K1_Times_DstA,
    NvDdkVicBlendDstFactC_One_Minus_K1_Times_DstA,
    NvDdkVicBlendDstFactC_One_Minus_K1_Times_SrcA,
    NvDdkVicBlendDstFactC_Zero,
    NvDdkVicBlendDstFactC_One
} NvDdkVicBlendDstFactC;

typedef enum
{
    NvDdkVicBlendSrcFactA_K1,
    NvDdkVicBlendSrcFactA_K2,
    NvDdkVicBlendSrcFactA_One_Minus_K1_Times_DstA,
    NvDdkVicBlendSrcFactA_Zero
} NvDdkVicBlendSrcFactA;

typedef enum
{
    NvDdkVicBlendDstFactA_K2,
    NvDdkVicBlendDstFactA_One_Minus_K1_Times_SrcA,
    NvDdkVicBlendDstFactA_Zero,
    NvDdkVicBlendDstFactA_One
} NvDdkVicBlendDstFactA;

typedef enum
{
    // Opaque (all alpha inside target rect will be set to 1.0)
    NvDdkVicAlphaFillMode_Opaque,
    // Background (all alpha inside target rect will be set to background alpha)
    NvDdkVicAlphaFillMode_Background,
    // Destination (alpha will remain unchanged)
    NvDdkVicAlphaFillMode_Destination,
    // Source stream (alpha from source stream without planar alpha)
    NvDdkVicAlphaFillMode_SourceStream,
    // Composited (composited alpha, starting with background)
    NvDdkVicAlphaFillMode_Composited,
    // Source alpha (alpha from source stream with planar alpha)
    NvDdkVicAlphaFillMode_SourceAlpha
} NvDdkVicAlphaFillMode;

typedef enum
{
    NvDdkVicVideoFormat_Progressive,
    NvDdkVicVideoFormat_FieldBased,
    NvDdkVicVideoFormat_Interleaved
} NvDdkVicVideoFormat;

typedef enum
{
    NvDdkVicSubsamplingType_420,
    NvDdkVicSubsamplingType_422,
    NvDdkVicSubsamplingType_422R,
    NvDdkVicSubsamplingType_444
} NvDdkVicSubsamplingType;

typedef enum
{
    NvDdkVicColorSpace_RGB_0_255,
    NvDdkVicColorSpace_RGB_16_235,
    NvDdkVicColorSpace_YCbCr601,
    NvDdkVicColorSpace_YCbCr601_ER,
    NvDdkVicColorSpace_YCbCr601_RR,
    NvDdkVicColorSpace_YCbCr709
} NvDdkVicColorSpace;

typedef enum
{
    NvDdkVicFilter_Nearest,
    NvDdkVicFilter_Bilinear,
    NvDdkVicFilter_5_Tap,
    NvDdkVicFilter_10_Tap,
    NvDdkVicFilter_Smart,
    NvDdkVicFilter_Nicest
} NvDdkVicFilter;

typedef struct NvDdkVicBlendRec
{
    // Blending is controlled by setting the factors in the following blending
    // formulas:
    //    outputR = (SrcFactC * srcR) + (DstFactC * dstR)
    //    outputG = (SrcFactC * srcG) + (DstFactC * dstG)
    //    outputB = (SrcFactC * srcB) + (DstFactC * dstB)
    //    outputA = (SrcFactA * srcA) + (DstFactA * dstA)
    // Supported blending expressions for each factor:
    // +-------------------------+----------+----------+----------+----------+
    // |                         | SrcFactC | DstFactC | SrcFactA | DstFactA |
    // +-------------------------+----------+----------+----------+----------+
    // | K1                      | x        | x        | x        |          |
    // | K2                      |          | x        | x        | x        |
    // | K1_Times_DstA           | x        | x        |          |          |
    // | K1_Times_SrcA           | x        |          |          |          |
    // | One_Minus_K1_Times_DstA | x        | x        | x        |          |
    // | One_Minus_K1_Times_SrcA | x        |          |          | x        |
    // | Zero                    | x        | x        | x        | x        |
    // | One                     |          | x        |          | x        |
    // +-------------------------+----------+----------+----------+----------+
    float K1;
    float K2;
    NvDdkVicBlendSrcFactC SrcFactC;
    NvDdkVicBlendDstFactC DstFactC;
    NvDdkVicBlendSrcFactA SrcFactA;
    NvDdkVicBlendDstFactA DstFactA;
    // Override src component with a constant.
    float OverrideR;
    float OverrideG;
    float OverrideB;
    float OverrideA;
    NvBool UseOverrideR;
    NvBool UseOverrideG;
    NvBool UseOverrideB;
    NvBool UseOverrideA;
    // Force output to equal dst (i.e. set to NV_TRUE to ignore the component).
    NvBool MaskR;
    NvBool MaskG;
    NvBool MaskB;
    NvBool MaskA;
} NvDdkVicBlend;

typedef struct NvDdkVicSurfaceInfoRec
{
    NvDdkVicPixelFormat PixelFormat;
    NvDdkVicSurfaceLayout SurfaceLayout;
    NvDdkVicFrameFormat FrameFormat; // input only
    NvDdkVicSubsamplingType SubSamplingType;
    NvDdkVicColorSpace ColorSpace;
    NvU32 LumaSurfaceWidth;
    NvU32 LumaSurfaceHeight;
    NvU32 LumaPaddedWidth;
    NvU32 LumaPaddedHeight;
    NvU32 ChromaSurfaceWidth;
    NvU32 ChromaSurfaceHeight;
    NvU32 ChromaPaddedWidth;
    NvU32 ChromaPaddedHeight;
    NvU8 ChromaPlanes;
    NvU8 ChromaLocHoriz;
    NvU8 ChromaLocVert;
    NvU8 CacheWidth; // input only
    NvU8 BlkHeight;
    NvBool ForOutput;
    NvBool HasAlpha;
} NvDdkVicSurfaceInfo;

typedef struct NvDdkVicSurfaceVideoInfoRec
{
    NvDdkVicVideoFormat Format;
    NvBool EvenField;
    NvBool SubStream;
} NvDdkVicSurfaceVideoInfo;

/*
// TODO: Palettes
// TODO: CanHighQualityDeInterlace
*/

typedef struct NvDdkVicColorMatrixRec
{
    float m[3][4];
} NvDdkVicColorMatrix;

typedef struct NvDdkVicSlotRec
{
    NvBool Enable;
    NvBool DeNoise;
    NvBool CadenceDetect;
    NvBool MotionMap;
    NvBool IsEven;
    NvBool MMapCombine;
    NvBool DoInitialMotionCalc;
    NvDdkVicSurfaceInfo Info;
    NvDdkVicSurfaceVideoInfo VideoInfo;
    NvDdkVicBlend Blend;
    NvRectF32 SourceRect;
    NvRect DestRect;
    NvDdkVicColorMatrix ColorMatrix;
    NvU32 ClearRectMask;
    //DXVAHD_DEINTERLACE_MODE_PRIVATE DeInterlaceMode;
    float PanoramicScaling;
    NvDdkVicFilter FilterX;
    NvDdkVicFilter FilterY;
} NvDdkVicSlot;

typedef struct NvDdkVicFloatColorRec
{
    float R;
    float G;
    float B;
    float A;
} NvDdkVicFloatColor;

typedef struct NvDdkVicParametersRec
{
    NvDdkVicSlot Slot[NVDDK_VIC_MAX_SLOTS_RESERVED];
    NvDdkVicSurfaceInfo OutputInfo;
    NvRect TargetRect;
    NvDdkVicFloatColor BackgroundColor;
    NvDdkVicColorSpace BackgroundColorSpace;
    NvDdkVicAlphaFillMode AlphaFillMode;
    NvRect ClearRects[NVDDK_VIC_MAX_CLEAR_RECTS_RESERVED];
    NvBool OutputFlipX;
    NvBool OutputFlipY;
    NvBool OutputTranspose;
    NvBool EnableCrcs;
} NvDdkVicConfigParameters;

typedef NvU32 NvDdkVicCrc;

typedef void (*NvDdkVicPostFlushCallback)(
    struct NvRmStreamRec *Stream,
    NvU32 SyncPointBase,
    NvU32 SyncPointsUsed,
    void *UserData);

typedef struct NvDdkVicCapsRec
{
    NvRmChipVICVersion VICVersion;
    NvU32 MaxSlots;
    NvU32 MaxClearRects;
    NvU32 MaxSurfaceWidth;
    NvU32 MaxSurfaceHeight;
    float MinScaleFactor;
    float MaxScaleFactor;
} NvDdkVicCaps;

typedef struct VicHALRec VicHAL;
typedef struct VicHALContextRec VicHALContext;

typedef struct NvDdkVicSessionRec
{
    NvDdkVicConfigParameters Parms;
    NvRmMemHandle ConfigStructHandle;
    NvRmDeviceHandle RmDevice;
    NvRmChannelHandle Channel;
    NvRmStream Stream;
    NvBool StreamInitialized;
    NvDdkVicPostFlushCallback PostFlushCallback;
    void *PostFlushCallbackUserData;
    NvRmFence LastFlushFence;

    NvDdkVicCaps Caps;
    VicHAL *HAL;
    VicHALContext *HALCtx;
} NvDdkVicSession;

typedef struct NvDdkVicExecuteParametersRec
{
    NvRmSurface *OutputSurface;
    NvRmSurface *InputSurfaces[NVDDK_VIC_MAX_SLOTS_RESERVED][NVDDK_VIC_MAX_SURFACES_PER_SLOT];
} NvDdkVicExecParameters;

typedef enum
{
   NvDdkVicPerPixelAlpha_Ignore,
   NvDdkVicPerPixelAlpha_Premultiplied,
   NvDdkVicPerPixelAlpha_NonPremultiplied,
} NvDdkVicPerPixelAlpha;

typedef enum
{
    NvDdkVicTransform_None         = 0,
    NvDdkVicTransform_FlipX        = 1 << 0,
    NvDdkVicTransform_FlipY        = 1 << 1,
    NvDdkVicTransform_Transpose    = 1 << 2,
    NvDdkVicTransform_InvTranspose = NvDdkVicTransform_Transpose | NvDdkVicTransform_FlipX | NvDdkVicTransform_FlipY,
    NvDdkVicTransform_Rotate90     = NvDdkVicTransform_Transpose | NvDdkVicTransform_FlipX,
    NvDdkVicTransform_Rotate180    = NvDdkVicTransform_FlipX     | NvDdkVicTransform_FlipY,
    NvDdkVicTransform_Rotate270    = NvDdkVicTransform_Transpose | NvDdkVicTransform_FlipY,
} NvDdkVicTransform;


//###########################################################################
//############################### PROTOTYPES ################################
//###########################################################################

/**
 *
 */
NvError NvDdkVicCreateSession(
    NvRmDeviceHandle RmDevice,
    NvDdkVicSession **Out);

/**
 *
 */
void NvDdkVicFreeSession(
    NvDdkVicSession *Session);

/**
 *
 */
void NvDdkVicSetPostFlushCallback(
    NvDdkVicSession *Session,
    NvDdkVicPostFlushCallback Callback,
    void *UserData);

/**
 *
 */
NvError NvDdkVicConfigure(
    NvDdkVicSession *Session,
    NvDdkVicConfigParameters *ConfigParmeters);

/**
 *
 */
NvError NvDdkVicExecute(
    NvDdkVicSession *Session,
    NvDdkVicExecParameters *ExecParameters,
    NvRmFence *FencesIn,
    NvU32 NumFencesIn,
    NvRmFence *FenceOut);

/**
 * Reads the VIC CRCs from the last execute (waits for it to finish first).
 * The active configuration must have EnableCrcs field set, otherwise
 * returns NvError_InvalidOperation.
 */
NvError NvDdkVicReadCrc(
    NvDdkVicSession *Session,
    NvDdkVicCrc *Crc);

/**
 *
 */
NvError NvDdkVicGetSurfaceInfo(
    NvDdkVicSession *Session,
    NvDdkVicSurfaceInfo *Info,
    NvRmSurface *Surfaces,
    NvU32 SurfaceCount,
    NvBool ForOutput);

/**
 *
 */
NvError NvDdkVicGetColorSpace(
    NvDdkVicColorSpace *OutColorSpace,
    NvBool *OutIsYUV,
    NvColorSpace ColorSpace);

/**
 *
 */
void NvDdkVicGetColorConversionMatrix(
    NvDdkVicColorMatrix *Out,
    NvDdkVicColorSpace FromSpace,
    NvDdkVicColorSpace ToSpace);

/**
 *
 */
void NvDdkVicConvertColor(
    NvDdkVicFloatColor *Out,
    const NvDdkVicFloatColor *In,
    const NvDdkVicColorMatrix *Matrix);

/**
 *
 */
NvError NvDdkVicConfigureSourceSurface(
    NvDdkVicSession *Session,
    NvDdkVicConfigParameters *Parms,
    NvU32 SlotIdx,
    NvRmSurface *Buffers,
    NvU32 NumBuffers,
    const NvRectF32 *SourceRect,
    const NvRect *DestRect);

/**
 *
 */
NvError NvDdkVicConfigureTargetSurface(
    NvDdkVicSession *Session,
    NvDdkVicConfigParameters *Parms,
    NvRmSurface *Buffers,
    NvU32 NumBuffers,
    const NvRect *TargetRect);

/**
 * Configure clear rects to clear slots below opaque surfaces (optimization)
 * This should be called after setting up slot dest rects and blending.
 * A good time to call this is just before NvDdkVicConfigure.
 */
int NvDdkVicConfigureClearRects(
    NvDdkVicSession *Session,
    NvDdkVicConfigParameters *Parms);

/**
 *
 */
void NvDdkVicConfigureBlending(
    NvDdkVicSession *Session,
    NvDdkVicConfigParameters *Parms,
    NvU32 SlotIdx,
    NvDdkVicPerPixelAlpha PerPixelAlpha,
    float ConstantAlpha);

/**
 *
 */
void NvDdkVicConfigureTransform(
    NvDdkVicSession *Session,
    NvDdkVicConfigParameters *Parms,
    NvDdkVicTransform Transform);

/**
 *
 */
void NvDdkVicApplyClipAndTransform(
    NvDdkVicSession *Session,
    NvDdkVicTransform Transform,
    const NvRect *TargetRect,
    NvRectF32 *SourceRectInOut,
    NvRect *DestRectInOut);


#if defined(__cplusplus)
} // extern "C"
#endif

#endif //INCLUDED_NVDDK_VIC_H
