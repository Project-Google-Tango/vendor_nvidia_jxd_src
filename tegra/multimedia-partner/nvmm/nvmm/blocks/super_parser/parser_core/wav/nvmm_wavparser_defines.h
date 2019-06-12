/* Copyright (c) 2006 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an
* express license agreement from NVIDIA Corporation is strictly prohibited.
*/

#ifndef INCLUDED_NVMM_WAVPARSER_DEFINES_H
#define INCLUDED_NVMM_WAVPARSER_DEFINES_H

#include "nvos.h"
#include "nvmm.h"
#include "nvmm_parser.h"

typedef struct 
{    
    NvU32 structSize;
    NvU32 streamIndex;
    NvU32 samplingFreq;
    NvU32 sampleSize;
    NvU32 containerSize;
    NvU32 blockAlign;
    NvU32 frameSize;
    NvU32 noOfChannels;
    NvU16 audioFormat;
    //ADPCM parameters
    NvU16 nCoefs;
    NvU16 samplesPerBlock;
    NvU32 uDataLength;
 }NvWavTrackInfo;

typedef  struct 
{
    NvU32 ChunkId;
    NvU32 ChunkSize;
    NvU32 Format;
    NvU32 SubChunk1Id;  
    NvU32 SubChunk1Size;
    NvU16 AudioFormat;
    NvU16 NumChannels;
    NvU32 SampleRate;
    NvU32 Byterate;
    NvU16 BlockAlign;
    NvU16 BitsPerSample;
    NvU32 SubChunk2Id;
    NvU32 SubChunk2Size;
    NvU32 tempChunkId;  
    NvU32 tempChunkSize;
    
}NvWavHdr;

typedef struct
{
    NvWavTrackInfo *pTrackInfo;
    NvWavHdr *pHeader;
}NvWavStreamInfo;



#endif //INCLUDED_NVMM_WAVPARSER_DEFINES_H





