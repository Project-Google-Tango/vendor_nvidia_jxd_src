/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit: 
 *           NvMMBlock MP2 audio decoder specific structure</b>
 *
 * @b Description: Declares Interface for MP2 Audio decoder.
 */

#ifndef INCLUDED_NVMM_MP2_H
#define INCLUDED_NVMM_MP2_H

/**
 * @defgroup nvmm_mp2 MP2 Codec API
 * 
 * 
 * @ingroup nvmm_modules
 * @{
 */

#include "nvmm.h"
#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

// Buffers' configuration
#define MP2_MIN_INPUT_BUFFER_SIZE 512
#define MP2_MAX_INPUT_BUFFER_SIZE 65536
#define MP2_MIN_INPUT_BUFFERS 1
#define MP2_MAX_INPUT_BUFFERS 32

#define MP2_MIN_OUTPUT_BUFFER_SIZE 8192 
#define MP2_MAX_OUTPUT_BUFFER_SIZE 8192 
#define MP2_MIN_OUTPUT_BUFFERS 10
#define MP2_MAX_OUTPUT_BUFFERS 10

/** 
 * MP2Dec block's stream indices.
 * Stream indices must start at 0.
 */
typedef enum
{
    //!< Input
    NvMMAudioMP2Stream_In = 0, 
    //!< Output
    NvMMAudioMP2Stream_Out,
    NvMMAudioMP2StreamCount
} NvMMMP2Stream;

typedef struct NvMMAudioMP2TrackInfoRec
{
    NvU32 objectType;
    NvU32 profile;
    NvU32 samplingFreqIndex;
    NvU32 samplingFreq;
    NvU32 noOfChannels;
    NvU32 sampleSize;
    NvU32 channelConfiguration;
    NvU32 bitRate;
} NvMMAudioMP2TrackInfo;

typedef enum
{
    NvMMAudioMP2Attribute_CommonStreamProperty = (NvMMAttributeAudioDec_Unknown + 300),
    NvMMAudioMP2Attribute_TrackInfo,
    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMAudioMP2Attribute_Force32 = 0x7FFFFFFF
} NvMMAudioMP2Attribute;

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_MP2_H
