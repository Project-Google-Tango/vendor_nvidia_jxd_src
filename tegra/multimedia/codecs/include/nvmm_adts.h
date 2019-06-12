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
 *           NvMMBlock AAC audio decoder specific structure</b>
 *
 * @b Description: Declares Interface for AAC Audio decoder.
 */

#ifndef INCLUDED_NVMM_ADTS_H
#define INCLUDED_NVMM_ADTS_H

/**
 * @defgroup nvmm_aac AAC Codec API
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
    NvMMAudioAdtsStream_In = 0, //!< Input
    NvMMAudioAdtsStream_Out,    //!< Output
    NvMMAudioAdtsStreamCount

} NvMMAdtsStream;




/** 
* @brief AAC codec Parameters
*/
typedef struct 
{
    NvU32 structSize;
    /* Control for Enable/disable of SBR portion of AAC+ decode */
    NvU32   SBREnabled;
    /* Control for enable/disable of CRC checking for error resiliency */
    NvU32   CRCEnabled;

} NvMMAudioDecPropertyADTS;




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

    } NvMMAudioAdtsTrackInfo;

typedef enum
{
    NvMMAdtsAttribute_CommonStreamProperty = (NvMMAttributeAudioDec_Unknown + 100),
    NvMMAudioAdtsAttribute_TrackInfo,
    NvMMAudioAdtsAttribute_MultiInputframe,
    NvMMAudioAdtsAttribute_EnableAdtsParsing,    /* *(NvBool*)pAttribute should be NV_TRUE or NV_FALSE */
    //Channel Selection attribute for dual ch file.
    //  NVMM_AAC_TRACK_LEFT --- left ch will be selected
    //  NVMM_AAC_TRACK_RIGHT --- right ch will be selected
    //  NVMM_AAC_TRACK_ALL --- both the channels will be mixed
    NvMMAudioAdtsAttribute_DmTrack,
    NvMMAudioAdtsAttribute_Force32 = 0x7FFFFFFF
}NvMMAudioAdtsAttribute;

#define HW_VERSION_AP10             0x10
#define HW_VERSION_AP15             0x15
#define TIMER_MULTIPLIER_REG        0x60005014
#define TIMER_MULTIPLIER_FACTOR     0x218

#define NVMM_AAC_TRACK_ALL                       0
#define NVMM_AAC_TRACK_LEFT                     -1
#define NVMM_AAC_TRACK_RIGHT                     1

//typedef struct
//{
   // NvU32 vdeDeviceId;
//} NvMMAudioCapabilities;

NvU8 NvAdtsGetHwVersion(NvRmDeviceHandle hRmDevice);

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_AAC_H
