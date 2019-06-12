/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * @brief <b>nVIDIA Driver Development Kit:
 *           MSENC ME control</b>
 *
 * @b Description: Defines the API implementation for MPEG2/MPEG4/H264 MSENC ME
 */

#ifndef INCLUDED_NVME_H
#define INCLUDED_NVME_H

#include "nvrm_surface.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Define the notion of mmblock handle that represents one instance
 * of a particular type of mmblock. The handle is in fact a pointer to
 * a structure encapsulating the instance's context. This handle is
 * passed to every mmblock entrypoint to establish context.
 *
 * @ingroup mmblock
 */
typedef struct NvMSEncMEBlockRec * NvMSEncMEBlockHandle;

/** Maximum number of surfaces per input. */
#define MAX_NUM_OF_SURFACES 3

/* Video Encoder ME type */
typedef enum
{
    NvMSEncME_UnKnown = 0,
    NvMSEncME_MPEG2,
    NvMSEncME_MPEG4,
    NvMSEncME_H264,
    NvMSEncME_Force32 = 0x7FFFFFFF
} NvMSEncMEType;

typedef enum
{
    P_ME_TYPE = 0,
    B_ME_TYPE = 1,
    I_ME_TYPE = 2
} NvMSEncME_FrameType;

/* Output Mode of ME */
typedef enum
{
    NvMSEncME_UnKnownMode = 0,
    NvMSEncME_Mode,
    NvMSEncMVOnly_Mode,
    NvMSEncMEOutput_Force32 = 0x7FFFFFFF
} NvMSEncMEOutputMode;

/** Defines parameters to set through SetParameters
 */
typedef struct
{
    NvU32 StructSize;
    NvU32 Width;
    NvU32 Height;
    NvU32 Profile;
    NvU32 Level;
    NvU32 NumBFrames;
    NvMSEncMEType EncoderType;
    NvMSEncMEOutputMode OutputMode;
} NvMSEncMEParams;

/**
 * @brief A mmblocks interface is a set of function pointers that provide
 * entrypoints for a given mmblock instance. Note that each instance may have
 * its own interface implementation (i.e. a different set of function
 * pointers).
 *
 * @ingroup mmblock
 */
typedef struct NvMSEncMEBlockRec
{
    NvU32 StructSize;
    void *pcontext;
/* Set parameters of the structure NvMSEncMEParams */
    NvError (*SetParameters)(NvMSEncMEBlockHandle hBlock,NvU32 ParameterSize,void *pparameter);
/* Allocate Nvrm Surfaces for input and reference YUVs */
    NvError (*AllocateSurfaces)(NvMSEncMEBlockHandle hBlock);
/* Get the size of OutputBuffer */
    NvError (*GetOutputBufferSize)(NvMSEncMEBlockHandle hBlock,NvU32 *OutBufSize);
} NvMSEncMEBlock;

/** Defines FrameHeader of OutputBuffer
 * Currently it is not sending in outputBuffer
 */
typedef struct packetME_FRAMEH {
    NvU32 FrameBitBudget;
    NvU32 ImageSizeX           : 16;
    NvU32 ImageSizeY           : 16;
    NvU32 FrameNum             : 32;
    NvU32 FrameType            : 2;          //0 (I), 1 (P), 2 (SKIP), 3 (B)
    NvU32 CurrFrameRefType     : 1;
    NvU32 NumSliceGroupsMinus1 : 3;          //H.264 only, always 0 for MSENC2.0
    NvU32 Context_ID           : 2;
    NvU32 Reserved             : 5;
    NvU32 BDirectType          : 1;          //0 (spatial), 1 (temporal)
    NvU32 PicOrderCountType    : 2;
    NvU32 PicOrderCount        : 16;
} ME_FRAMEH;

typedef struct blk_4x4_TYPE
{
    NvU8 pixels[16]; //16 pixels of a 4x4 block
} blk_4x4_s;

typedef struct ME_SUB_MB_DATA_ {
    blk_4x4_s curr;
    blk_4x4_s mocomp;
} ME_SUB_MB_DATA_S;

/** Defines format of MB Header0 */
typedef struct packetME_MBH0 {
    NvU64 QP                : 8;
    NvU64 L_MB_NB_Map       : 1;
    NvU64 TL_MB_NB_Map      : 1;
    NvU64 Top_MB_NB_Map     : 1;
    NvU64 TR_MB_NB_Map      : 1;
    NvU64 sliceGroupID      : 3;
    NvU64 firstMBinSlice    : 1;
    NvU64 part0ListStatus   : 2;
    NvU64 part1ListStatus   : 2;
    NvU64 part2ListStatus   : 2;
    NvU64 part3ListStatus   : 2;
    NvU64 RefID_0_L0        : 5;
    NvU64 RefID_1_L0        : 5;
    NvU64 RefID_2_L0        : 5;
    NvU64 RefID_3_L0        : 5;
    NvU64 RefID_0_L1        : 5;
    NvU64 RefID_1_L1        : 5;
    NvU64 RefID_2_L1        : 5;
    NvU64 RefID_3_L1        : 5;
    NvU64 sliceType         : 2;
    NvU64 lastMbInSlice     : 1;
    NvU64 reserved2         : 3;
    NvU64 MVMode            : 2;
    NvU64 MBy               : 8;
    NvU64 MBx               : 8;
    NvU64 MB_Type           : 2;
    NvU64 reserved1         : 6;
    NvU64 MB_Cost           : 17;
    NvU64 MVModeSubTyp0     : 2;
    NvU64 MVModeSubTyp1     : 2;
    NvU64 MVModeSubTyp2     : 2;
    NvU64 MVModeSubTyp3     : 2;
    NvU64 part0BDir         : 1;
    NvU64 part1BDir         : 1;
    NvU64 part2BDir         : 1;
    NvU64 part3BDir         : 1;
    NvU64 reserved0         : 3;
} ME_MBH0;

/** Defines format of MB Header1 */
typedef struct packetME_MBH1 {
    NvU64 L0_mvy_0     : 16;
    NvU64 L0_mvy_1     : 16;
    NvU64 L0_mvy_2     : 16;
    NvU64 L0_mvy_3     : 16;
    NvU64 L0_mvx_0     : 16;
    NvU64 L0_mvx_1     : 16;
    NvU64 L0_mvx_2     : 16;
    NvU64 L0_mvx_3     : 16;
} ME_MBH1;

/** Defines format of MB Header2 */
typedef struct packetME_MBH2 {
    NvU64 L1_mvy_0     : 16;
    NvU64 L1_mvy_1     : 16;
    NvU64 L1_mvy_2     : 16;
    NvU64 L1_mvy_3     : 16;
    NvU64 L1_mvx_0     : 16;
    NvU64 L1_mvx_1     : 16;
    NvU64 L1_mvx_2     : 16;
    NvU64 L1_mvx_3     : 16;
} ME_MBH2;

/** Defines format of MB Header3 */
typedef struct packetME_MBH3 {
    NvU64 Interscore    : 21;
    NvU64 Reserved0     : 11;
    NvU64 Intrascore    : 19;
    NvU64 Reserved1     : 13;
    NvU64 Reserved2;
} ME_MBH3;

/** Defines format of OutputBuffer for one MB */
typedef struct ME_MB_DATA{
    ME_MBH0 head0;
    ME_MBH1 head1;
    ME_MBH2 head2;
    ME_MBH3 head3;
    ME_SUB_MB_DATA_S mb_data[24]; // Discard if output mode is MV only
} ME_MB_DATA_S;

/*
 * @brief Open the Block for MSENC ME mode
 */
NvError
NvMSEncMEOpen(
    NvMSEncMEBlockHandle *hBlock);

/*
 * @brief Execute MSENC ME mode
 * input pRefSurf is reference/reconstructed yuv
 * input pInpSurf is current yuv to be processed
 */
NvError
NvMSEncMEExecute(
    NvMSEncMEBlockHandle hBlock,
    NvRmSurface *pRefSurf,
    NvRmSurface *pRefSurfBwd,
    NvRmSurface *pInpSurf,
    NvU32 FrameNum,
    NvU32 FrameType);

/*
 * @brief Execute MSENC ME mode if inputs are in heap mem
 * input pRefBuf(in heap mem) is reference/reconstructed yuv
 * input pInpBuf(in heap mem) is current yuv to be processed
 */
NvError
NvMSEncMERun(
    NvMSEncMEBlockHandle hBlock,
    NvU8 *pRefBuf,
    NvU8 *pRefBufBwd,
    NvU8 *pInpBuf,
    NvU32 FrameNum,
    NvU32 FrameType);

/* @brief Get the output buffer with the format
 * of struct ME_MB_DATA
 */
NvError
NvMSEncMEGetOutput(
    NvMSEncMEBlockHandle hBlock,
    NvU8 *pOutBuf);

/* @brief Close the block of MSENC ME*/
void
NvMSEncMEClose(
    NvMSEncMEBlockHandle hBlock);

/* Typical code flow from app side */
/*
 *
Example 1:
 *
    NvMSEncMEOpen()
    SetParameters()
    GetOutputBufferSize()
    NvMSEncMEExecute()
    NvMSEncMEGetOutput()
    NvMSEncMEExecute()
    NvMSEncMEGetOutput()
    .....
    .....
    NvMSEncMEClose()

Example 2: App can enalbe pipeline of 2 frames
 *
 *  NvMSEncMEOpen()
    SetParameters()
    GetOutputBufferSize()
    NvMSEncMEExecute()  -1
    NvMSEncMEExecute()  -2
    NvMSEncMEGetOutput() - Get output of 1 // App can use advantage of pipeline if needed
    NvMSEncMEGetOutput() - Get output of 2
    .....
    .....
    NvMSEncMEClose()
**/
#if defined(__cplusplus)
}
#endif

#endif
