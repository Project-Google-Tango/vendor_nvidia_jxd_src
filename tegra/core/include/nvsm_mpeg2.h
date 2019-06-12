/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef SHADERMASTER_MPEG2_H
#define SHADERMASTER_MPEG2_H

#include "nvrm_surface.h"
#include "nvcommon.h"
#include "nvsm.h"

typedef struct NvSmMpeg2Rec *NvSmMpeg2Handle;

typedef struct 
{
    // src surface memory handle
    NvRmMemHandle hSrcSurface;
    // dst surface memory handle
    NvRmMemHandle hDstSurface;
    // src surface offset to allow top->bottom or bottom->top compensation
    NvU16 SrcOffset;
    // dst surface offset to allow top->bottom or bottom->top compensation
    NvU16 DstOffset;
    // stride: surface Pitch if 16x16 compensation or 2*Pitch if field
    // compensation
    NvU16 Stride;
    // horizontal position of macroblock (currently a part of NvPoint)
    NvU16 x;
    // vertical position of macroblock (currently a part of NvPoint)
    NvU16 y;
    // number of columns to compensate from x
    NvU16 w;
    // number of lines to compensate from y
    NvU16 h;
    // motion vector x (currently a part of NvPoint)
    NvS16 dx;
    // motion vector y (currently a part of NvPoint)
    NvS16 dy;
    // enable second_half_flag to select odd field when fields are separated
    // valid only if FieldPicture is set in NvSmMpeg2SetInput
    NvBool second_half_flag;
} NvSmMpeg2McInfo;

typedef struct NvSmMpeg2McBlockRec
{
    // Tells which group does this block belong to
    NvU8 group;
    // Block co-ordinates
    NvRect block;
    // Motion vector for Reference Frame 1 (used for Copy)
    NvPoint mv1;
    // Motion vector for Reference Frame 2 (used for Averaging)
    NvPoint mv2;
    // Pointer to next block belonging in the same group
    struct NvSmMpeg2McBlockRec *pNext;
} NvSmMpeg2McBlock;

typedef enum {
    // dst = src(mv) 
    NvSmMpeg2Stage3Mode_Copy,
    // dst = (dst+src(mv)+1)/2 (B-frames)
    NvSmMpeg2Stage3Mode_Average,
    // dst = idct output+128
    NvSmMpeg2Stage3Mode_Intra,
    // addition with idct output
    NvSmMpeg2Stage3Mode_Add,

    NvSmMpeg2Stage3Mode_Last,
} NvSmMpeg2Stage3Mode;

// Memory handles are NV_GR3D_SURF_BYTE_QUANTUM*2 (32) bytes aligned.
#define NVSM_MPEG2_ALIGN 32
// Width is a multiple of 32 (in fact we only really need pitch of internal
// surface to be multiple of NVRM_SURFACE_TILE_WIDTH (64), but for simplicity
// simply restrict the width);
#define NVSM_MPEG2_WIDTH 32
// Height is multiple of 16.
#define NVSM_MPEG2_HEIGHT 16
// Reference frame pitch is a multiple of
// NV_GR3D_TEX_NPOT_STRIDE_ALIGNMENT_BYTES (64), pad it if the width is not
// divisible by 64.
#define NVSM_MPEG2_REFERENCE_FRAME_PITCH 64

// Maximum supported size if 2048x2048.  Due to precision issues, applying
// motion compensation over large regions needs to be done in 1024x1024 tiles.
#define NVSM_MPEG2_MAX_TILE_SIZE 1024

// Width and Height are multiples of NVSM_MPEG2_WIDTH and NVSM_MPEG2_HEIGHT
// respectively.  This function allocates a temporary buffer shared by stages
// 1 and 2, so give it maximum width and height you expect for frames you will
// render.
NvSmMpeg2Handle NvSmMpeg2Init(NvRmDeviceHandle hRmDev,
        NvU32 Width, NvU32 Height);

// Changes width and height.  Invalidates input and output surfaces:
// NvSmMpeg2SetInput and NvSmMpeg2SetOutput must be called after calling
// NvSmMpeg2ChangeSize.  Product of Width and Height specified in this
// function must be not greater than product of width and height given to
// NvSmMpeg2Init.
NvError NvSmMpeg2ChangeSize(NvSmMpeg2Handle hNvSmMpeg2,
        NvU32 Width, NvU32 Height);

// hMemInput: size is Width*Height*2, alignment is NVSM_MPEG2_ALIGN, pitch is
// Width*2.
NvError NvSmMpeg2SetInput(NvSmMpeg2Handle hNvSmMpeg2, NvRmMemHandle hMemInput,
                          NvU32 Offset, NvBool FieldPicture);
// hMemOutput: size is Width*Height, alignment is NVSM_MPEG2_ALIGN, pitch is
// Width.
NvError NvSmMpeg2SetOutput(NvSmMpeg2Handle hNvSmMpeg2,
        NvRmMemHandle hMemOutput, NvU32 Offset, NvU32 Stride);
// hMemReferenceFrame: pitch is a multiple of
// NVSM_MPEG2_REFERENCE_FRAME_PITCH, pad if needed.
NvError NvSmMpeg2SetReferenceFrame(NvSmMpeg2Handle hNvSmMpeg2,
        NvRmMemHandle hMemReferenceFrame);

NvError NvSmMpeg2SetReferenceFrame1(NvSmMpeg2Handle hNvSmMpeg2,
        NvRmMemHandle hMemReferenceFrame2, NvU32 Offset1);
NvError NvSmMpeg2SetReferenceFrame2(NvSmMpeg2Handle hNvSmMpeg2,
        NvRmMemHandle hMemReferenceFrame2, NvU32 Offset2);

typedef enum {
    NvSmMpeg2SkipFirstStage = 1,
    NvSmMpeg2SkipSecondStage = 2,
    NvSmMpeg2SkipFirstAndSecondStage = 3,
} NvSmMpeg2Passthrough;

NvSmMpeg2Handle NvSmMpeg2DebugInitPassthrough(NvRmDeviceHandle hRmDev,
        NvU32 Width, NvU32 Height, NvSmMpeg2Passthrough PassthroughType);

NvRmMemHandle NvSmMpeg2GetStage2Handle(NvSmMpeg2Handle hNvSmMpeg2);

void NvSmMpeg2Destroy(NvSmMpeg2Handle hNvSmMpeg2);

// Performs columns IDCT.
//
// If IsInterlaced is NV_FALSE, IDCT is performed on groups of elements
//   { (i,0), (i,1), ..., (i,7) } and
//   { (i,8), (i,9), ..., (i,15) } for 0 <= i <= 15.
//
// If IsInterlaced is NV_TRUE, IDCT is performed on groups of elements
//   { (i,0), (i,2), ..., (i,14) } and
//   { (i,1), (i,3), ..., (i,15) } for 0 <= i <= 15.
//
// Coordinates above refer to individual macroblocks.  Macroblocks are
// specified by their upper-left corner using blocks array.
void NvSmMpeg2Stage1(NvSmMpeg2Handle hNvSmMpeg2, NvBool IsInterlaced,
        const NvPoint *blocks, NvU32 count);

// Performs rows IDCT.
//
// IDCT is performed on groups of elements
//   { (0,j), (1,j), ..., (7,j) } and
//   { (8,j), (9,j), ..., (15,i) } for 0 <= j <= 15.
//
// Coordinates above refer to individual macroblocks.  Macroblocks are
// specified by their upper-left corner using blocks array.
void NvSmMpeg2Stage2(NvSmMpeg2Handle hNvSmMpeg2,
        const NvPoint *blocks, NvU32 count);

// MotionVectors are multiplied by two -- one pixel is 2, half-pixel is 1.
// Blocks' coordinates are *not* multiplied by two, they are the same as in
// NvSmMpeg2Stage1 and 2.
void NvSmMpeg2Stage3(NvSmMpeg2Handle hNvSmMpeg2,
        const NvSmMpeg2McBlock *Blocks, NvSmMpeg2Stage3Mode flag, NvBool IsChroma);

// Returns a value that can be waited on using NvSmMpeg2Wait.
NvU32 NvSmMpeg2SyncPointIncrement(NvSmMpeg2Handle hNvSmMpeg2);
void NvSmMpeg2Wait(NvSmMpeg2Handle hNvSmMpeg2, NvU32 value);

#endif
