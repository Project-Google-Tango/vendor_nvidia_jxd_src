/*
 * Copyright (c) 2007 - 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all NvS32ellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file nvmm_wavparser.h
 * @brief <b>NVIDIA Media Parser Package:wav parser/b>
 *
 * @b Description:   This file outlines the API's provided by wav parser
 */

#ifndef INCLUDED_NVMM_WAVPARSER_H
#define INCLUDED_NVMM_WAVPARSER_H

#include "nvmm_wavparser_defines.h"
#include "nvlocalfilecontentpipe.h"
#include "nvmm.h"

#define INPUT_SIZE 4096
#define INITIAL_HEADER_SIZE 320
#define WAVE_FORMAT_PCM  (0x0001)
#define WAVE_FORMAT_ADPCM  (0x0002)
#define WAVE_FORMAT_ALAW  (0x0006)
#define WAVE_FORMAT_MULAW  (0x0007)

#define SIGN_BIT        (0x80)          /* Sign bit for a A-law byte. */
#define QUANT_MASK      (0xf)           /* Quantization field mask. */
#define NSEGS           (8)             /* Number of A-law segments. */
#define SEG_SHIFT       (4)             /* Left shift for segment number. */
#define SEG_MASK        (0x70)          /* Segment field mask. */
#define BIAS            (0x84)          /* Bias for linear code. */
#define CLIP            8159
#define WAVE_FORMAT_EXTENSIBLE 65534
#define RIFF_TAG_SEARCH_RANGE    16 * 1024     /*Search RIFF header for initial 16 K*/
#define RIFF_TAG    0x46464952
#define WAVE_TAG  0x45564157
#define FMT_TAG     0x20746d66
#define DATA_TAG  0x61746164

typedef struct
{
     NvS32 numberOfTracks;
     NvWavStreamInfo *pInfo;
     CPhandle cphandle;
     CP_PIPETYPE_EXTENDED *pPipe;
     NvBool bWavHeader;
     NvU32 dataOffset;
     NvU64 total_time;
     NvU32 bitRate;
     NvU32 bytesRead;
}NvWavParser;

NvWavParser * NvGetWavReaderHandle(CPhandle hContent, CP_PIPETYPE_EXTENDED *pPipe );
void NvReleaseWavReaderHandle(NvWavParser * pState);
NvError NvWavSeekToTime(NvWavParser *pState, NvS64 mSec);
NvU64 GetWavTimeStamp(NvWavParser *pState,NvU32 askBytes);
NvError NvParseWav(NvWavParser *pState);
NvU32 AdpcmBytesPerBlock(NvU16 chans, NvU16 samplesPerBlock );

#endif //INCLUDED_NVMM_WAVPARSER_H

