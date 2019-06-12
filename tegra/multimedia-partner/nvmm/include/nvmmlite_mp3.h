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
 *           NvMMLiteBlock MP3 audio decoder specific structure</b>
 *
 * @b Description: Declares Interface for MP3 Audio decoder.
 */

#ifndef INCLUDED_NVMMLITE_MP3_H
#define INCLUDED_NVMMLITE_MP3_H

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
    NvMMLiteAudioMP3Stream_In = 0, //!< Input
    NvMMLiteAudioMP3Stream_Out,    //!< Output
    NvMMLiteAudioMP3StreamCount

} NvMMLiteAudioMP3Stream;


/** 
* @brief MP3 encode mode enumeration
*/
typedef enum
{
    NvMMLiteAudioMP3Mode_32=0,
    NvMMLiteAudioMP3Mode_40,
    NvMMLiteAudioMP3Mode_56,
    NvMMLiteAudioMP3Mode_64,
    NvMMLiteAudioMP3Mode_80,
    NvMMLiteAudioMP3Mode_96,
    NvMMLiteAudioMP3Mode_112,
    NvMMLiteAudioMP3Mode_128,
    NvMMLiteAudioMP3Mode_160,
    NvMMLiteAudioMP3Mode_192,
    NvMMLiteAudioMP3Mode_224,
    NvMMLiteAudioMP3Mode_256,
    NvMMLiteAudioMP3Mode_320,
    NvMMLiteAudioMP3Mode_Force32 = 0x7FFFFFFF
} NvMMLiteAudioMP3Mode;

/** 
* @brief MP3 channel mode enumeration
*/
typedef enum
{
    NvMMLiteAudioMP3ChanMode_Mono=0,
    NvMMLiteAudioMP3ChanMode_Stereo,
    NvMMLiteAudioMP3ChanMode_JointStereo,
    NvMMLiteAudioMP3ChanMode_Dual,
    NvMMLiteAudioMP3ChanMode_Force32 = 0x7FFFFFFF
} NvMMLiteAudioMP3ChanMode;


/** 
* @brief MP3 decoder Parameters
*/
typedef struct 
{
    NvU32 structSize;
    /// Channel mode of MP3 stream
    NvMMLiteAudioMP3ChanMode   ChannelMode;

} NvMMLiteAudioDecPropertyMP3;


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
    NvMMLiteAudioMP3Mode   Mode;
    /// Channel mode of MP3 stream
    NvMMLiteAudioMP3ChanMode   ChannelMode;

} NvMMLiteAudioEncPropertyMP3;

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

}NvMMLiteAudioMP3TrackInfo;

typedef enum
{
    NvMMLiteAudioMP3Attribute_CommonStreamProperty = (NvMMLiteAttributeAudioDec_Unknown + 300),
    NvMMLiteAudioMP3Attribute_TrackInfo,

    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMLiteAudioMP3Attribute_Force32 = 0x7FFFFFFF

}NvMMLiteAudioMP3Attribute;

#define HW_VERSION_AP10             0x10
#define HW_VERSION_AP15             0x15
#define TIMER_MULTIPLIER_REG        0x60005014
#define TIMER_MULTIPLIER_FACTOR     0x218

typedef struct
{
    NvU32 vdeDeviceId;
} NvMMLiteMP3AudioCapabilities;

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
typedef struct NvMMLiteAudioMp3Param {
    NvU32 nChannels;
    NvU32 nBitRate;
    NvU32 nSampleRate;
    NvU32 nAudioBandWidth;
    Nv_AUDIO_CHANNELMODETYPE eChannelMode;
    Nv_AUDIO_MP3STREAMFORMATTYPE eFormat;
} NvMMLiteAudioMp3Param;


#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMMLITE_MP3_H


