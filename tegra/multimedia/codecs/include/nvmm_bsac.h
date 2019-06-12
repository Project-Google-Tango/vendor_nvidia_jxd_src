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
 *           NvMMBlock bsac audio decoder specific structure</b>
 *
 * @b Description: Declares Interface for bsac Audio decoder.
 */

#ifndef INCLUDED_NVMM_BSAC_H
#define INCLUDED_NVMM_BSAC_H

/**
 * @defgroup nvmm_bsac bsac Codec API
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

    /** aacdec block's stream indices.
    Stream indices must start at 0.
*/
typedef enum
{
    NvMMAudioBsacStream_In = 0, //!< Input
    NvMMAudioBsacStream_Out,    //!< Output
    NvMMAudioBsacStreamCount

} NvMMBsacStream;


/** 
* @brief BSAC codec Parameters
*/
typedef struct 
{
    NvU32 structSize;
    /* Control for enable/disable of CRC checking for error resiliency */
    NvU32   CRCEnabled;

} NvMMAudioDecPropertyBSAC;


/** 
* @brief bsac codec Parameters
*/
typedef struct 
{
    NvU32 structSize;
    /* Encoding mode */
    //NvMMAudioBsacMode   Mode;
    /* Control for enable/disable of CRC checking for error resiliency */
    NvU32   CRCEnabled;
    /* Input sampler rate of stream in Hz */
    NvU32 SampleRate;
    /* Bitrate of stream in bits/second */
    NvU32 BitRate;
    /* Number of input audio channels to encode */
    NvU32 InputChannels;
    /* tns Enable */
    NvU32 Tns_Mode;
    /* Quantizer Quality */ 
    NvU32 quantizerQuality;

} NvMMAudioEncPropertyBSAC;

typedef struct
{
        NvU32 objectType;
        NvU32 profile;
        NvU32 samplingFreqIndex;
        NvU32 samplingFreq;
        NvU32 noOfChannels;
        NvU32 sampleSize;
        NvU32 channelConfiguration;
        NvU32 bitRate;
        NvU32 targetLayer;

} NvMMAudioBsacTrackInfo;

typedef enum
{
    NvMMBsacAttribute_CommonStreamProperty = (NvMMAttributeAudioDec_Unknown + 100),
    NvMMAudioBsacAttribute_TrackInfo,
    NvMMAudioBsacAttribute_MultiInputframe,
    NvMMAudioBsacAttribute_EnableAdtsParsing,    /* *(NvBool*)pAttribute should be NV_TRUE or NV_FALSE */
    NvMMAudioBsacAttribute_Force32 = 0x7FFFFFFF
}NvMMAudioBsacAttribute;

#define HW_VERSION_AP10             0x10
#define HW_VERSION_AP15             0x15
#define TIMER_MULTIPLIER_REG        0x60005014
#define TIMER_MULTIPLIER_FACTOR     0x218

/*typedef struct
{
    NvU32 vdeDeviceId;
} NvMMAudioCapabilities;*/

//NvU8 NvAacGetHwVersion(NvRmDeviceHandle hRmDevice);

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_AAC_H
