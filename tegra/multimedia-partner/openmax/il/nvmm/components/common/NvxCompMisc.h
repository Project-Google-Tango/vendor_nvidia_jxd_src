/* Copyright (c) 2006-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** NvxCompReg.h
 *
 *  NVIDIA's lists of components for OMX Component Registration.
 */

#ifndef _NVX_COMP_MISC_H
#define _NVX_COMP_MISC_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <OMX_Core.h>
#include <stdlib.h>

typedef struct {
    const OMX_U8    *data;
    int             size;
    OMX_U8          *dataEnd;
    OMX_U8          *currentPtr;
    OMX_U32         currentBuffer;
    int             bitCount;
    int             zeroCount;
} NVX_OMX_NALU_BIT_STREAM;

typedef struct {
    OMX_U32 profile_idc;
    OMX_U32 constraint_set0123_flag;
    OMX_U32 level_idc;

    OMX_U32 nWidth;
    OMX_U32 nHeight;
} NVX_OMX_SPS;

typedef struct {
    OMX_U32 entropy_coding_mode_flag;
} NVX_OMX_PPS;

OMX_BOOL NVOMX_ParseSPS(OMX_U8* data, int len, NVX_OMX_SPS *pSPS);

// Bitstream state information for bypass parsing
typedef struct
{
    unsigned int   *wordPtr;   //< pointer to buffer where to write next byte
    unsigned int   word;       //< next byte to write in buffer
    unsigned int   bit_count;  //< counts encoded bits
    unsigned int   word2;
    unsigned int   validbits;  //< number of valid bits in byte
} BYPASS_MOD_BUF_PTR;

// Bypass decoder structures and functions
typedef struct NVX_AUDIO_DECODE_DATA
{
    BYPASS_MOD_BUF_PTR mbp;  //< Bitstream state information

    OMX_U32  bytesConsumed;  //< Number of bytes parsed
    OMX_U32  outputBytes;    //< Number of bytes after parsing
    OMX_BOOL bFrameOK;       //< Set to true if frame was processed successfully

    OMX_U32 nChannels;       //< Number of output channels
    OMX_U32 nSampleRate;     //< Output sample rate
} NVX_AUDIO_DECODE_DATA;

void NvxBypassProcessAC3(NVX_AUDIO_DECODE_DATA *pContext,
                         OMX_AUDIO_CODINGTYPE outputCodingType,
                         OMX_U8 *inptr8,
                         OMX_U32 inputSize,
                         OMX_U8 *outptr8,
                         OMX_U32 outputSize);
void NvxBypassProcessDTS(NVX_AUDIO_DECODE_DATA *pContext,
                         OMX_AUDIO_CODINGTYPE outputCodingType,
                         OMX_U8 *inptr8,
                         OMX_U32 inputSize,
                         OMX_U8 *outptr8,
                         OMX_U32 outputSize);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

