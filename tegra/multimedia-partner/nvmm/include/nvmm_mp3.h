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
 *           NvMMBlock MP3 audio decoder specific structure</b>
 *
 * @b Description: Declares Interface for MP3 Audio decoder.
 */

#ifndef INCLUDED_NVMM_MP3_H
#define INCLUDED_NVMM_MP3_H

/**
 * @defgroup nvmm_mp3 MP3 Codec API
 * 
 * 
 * @ingroup nvmm_modules
 * @{
 */

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/** MP3dec block's stream indices.
    Stream indices must start at 0.
*/
typedef enum
{
    NvMMAudioMP3Stream_In = 0, //!< Input
    NvMMAudioMP3Stream_Out,    //!< Output
    NvMMAudioMP3StreamCount

} NvMMAudioMP3Stream;


/** 
* @brief MP3 encode mode enumeration
*/
typedef enum
{
    NvMMAudioMP3Mode_32=0,
    NvMMAudioMP3Mode_40,
    NvMMAudioMP3Mode_56,
    NvMMAudioMP3Mode_64,
    NvMMAudioMP3Mode_80,
    NvMMAudioMP3Mode_96,
    NvMMAudioMP3Mode_112,
    NvMMAudioMP3Mode_128,
    NvMMAudioMP3Mode_160,
    NvMMAudioMP3Mode_192,
    NvMMAudioMP3Mode_224,
    NvMMAudioMP3Mode_256,
    NvMMAudioMP3Mode_320,
    NvMMAudioMP3Mode_Force32 = 0x7FFFFFFF
} NvMMAudioMP3Mode;

/** 
* @brief MP3 channel mode enumeration
*/
typedef enum
{
    NvMMAudioMP3ChanMode_Mono=0,
    NvMMAudioMP3ChanMode_Stereo,
    NvMMAudioMP3ChanMode_JointStereo,
    NvMMAudioMP3ChanMode_Dual,
    NvMMAudioMP3ChanMode_Force32 = 0x7FFFFFFF
} NvMMAudioMP3ChanMode;


/** 
* @brief MP3 decoder Parameters
*/
typedef struct 
{
    NvU32 structSize;
    /// Channel mode of MP3 stream
    NvMMAudioMP3ChanMode   ChannelMode;

} NvMMAudioDecPropertyMP3;


/** 
* @brief MP3 encoder Parameters
*/
typedef struct 
{
    //MPEG Type  MPEG 1, 2, 2.5
    NvU32 Type;
    //MPEG Layer 
    NvU32 Layer;
    /// Channel mode of MP3 stream
    NvMMAudioMP3Mode   Mode;
    /// Channel mode of MP3 stream
    NvMMAudioMP3ChanMode   ChannelMode;

} NvMMAudioEncPropertyMP3;

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

}NvMMAudioMP3TrackInfo;

typedef enum
{
    NvMMAudioMP3Attribute_CommonStreamProperty = (NvMMAttributeAudioDec_Unknown + 300),
    NvMMAudioMP3Attribute_TrackInfo,

    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMAudioMP3Attribute_Force32 = 0x7FFFFFFF

}NvMMAudioMP3Attribute;

#define HW_VERSION_AP10             0x10
#define HW_VERSION_AP15             0x15
#define TIMER_MULTIPLIER_REG        0x60005014
#define TIMER_MULTIPLIER_FACTOR     0x218

typedef struct
{
    NvU32 vdeDeviceId;
} NvMMMP3AudioCapabilities;

NvU8 NvMP3GetHwVersion(NvRmDeviceHandle hRmDevice);


typedef enum Nv_AUDIO_MP3STREAMFORMATTYPE {
    Nv_AUDIO_MP3StreamFormatUnused = 0,
    Nv_AUDIO_MP3StreamFormatMP1Layer3,/**< MP3 Audio MPEG 1 Layer 3 Stream format */
    Nv_AUDIO_MP3StreamFormatMP2Layer3,/**< MP3 Audio MPEG 2 Layer 3 Stream format */
    Nv_AUDIO_MP3StreamFormatMP2_5Layer3,/**< MP3 Audio MPEG2.5 Layer 3 Stream format */
    Nv_AUDIO_MP3StreamFormatMax = 0x7FFFFFFF
} Nv_AUDIO_MP3STREAMFORMATTYPE;


typedef enum Nv_AUDIO_CHANNELMODETYPE {
    Nv_AUDIO_ChannelModeUnused = 0,
    Nv_AUDIO_ChannelModeStereo,
    Nv_AUDIO_ChannelModeJointStereo,
    Nv_AUDIO_ChannelModeDual,
    Nv_AUDIO_ChannelModeMono,
    Nv_AUDIO_ChannelModeMax = 0x7FFFFFFF
} Nv_AUDIO_CHANNELMODETYPE;

/**mp3 params */
typedef struct NvMMAudioMp3Param {
    NvU32 nChannels;
    NvU32 nBitRate;
    NvU32 nSampleRate;
    NvU32 nAudioBandWidth;
    Nv_AUDIO_CHANNELMODETYPE eChannelMode;
    Nv_AUDIO_MP3STREAMFORMATTYPE eFormat;
} NvMMAudioMp3Param;


#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_MP3_H


