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
 *           NvMMLiteBlock AAC audio decoder/encoder specific structure</b>
 *
 * @b Description: Declares Interface for AAC Audio decoder.
 */

#ifndef INCLUDED_NVMMLITE_AAC_H
#define INCLUDED_NVMMLITE_AAC_H

/**
 * @defgroup nvmm_aac AAC Codec API
 * 
 * 
 * @ingroup nvmm_modules
 * @{
 */

#include "nvmmlite.h"
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
    NvMMLiteAudioAacStream_In = 0, //!< Input
    NvMMLiteAudioAacStream_Out,    //!< Output
    NvMMLiteAudioAacStreamCount

} NvMMLiteAacStream;

typedef enum
{
    NvMMLiteAudioAacMode_LC = 0,
    NvMMLiteAudioAacMode_LTP,
    NvMMLiteAudioAacMode_PLUS,
    NvMMLiteAudioAacMode_PLUS_ENHANCED,
    NvMMLiteAudioAacMode_Force32 = 0x7FFFFFFF
}NvMMLiteAudioAacMode;


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

} NvMMLiteAudioDecPropertyAAC;


/**
* @brief AAC codec Parameters
*/
typedef struct
{
    NvU32 structSize;
    /* Encoding mode */
    NvMMLiteAudioAacMode   Mode;
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
    /* Transport OverHead */
    NvU32 TsOverHeadBits;

} NvMMLiteAudioEncPropertyAAC;

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
    NvBool HasDRM;
    NvU32 PolicyType;
    NvU32 MeteringTime;
    NvU32 LicenseConsumeTime;
    NvU32 DrmContext;
    NvU32 CoreContext;
} NvMMLiteAudioAacTrackInfo;

typedef enum
{
    NvMMLiteAacAttribute_CommonStreamProperty = (NvMMLiteAttributeAudioDec_Unknown + 100),
    NvMMLiteAudioAacAttribute_TrackInfo,
    NvMMLiteAudioAacAttribute_MultiInputframe,
    NvMMLiteAudioAacAttribute_EnableAdtsParsing,    /* *(NvBool*)pAttribute should be NV_TRUE or NV_FALSE */
    //Channel Selection attribute for dual ch file.
    //  NvMMLite_AAC_TRACK_LEFT --- left ch will be selected
    //  NvMMLite_AAC_TRACK_RIGHT --- right ch will be selected
    //  NvMMLite_AAC_TRACK_ALL --- both the channels will be mixed
    NvMMLiteAudioAacAttribute_DmTrack,
    NvMMLiteAudioAacAttribute_ComplianceTest,
    NvMMLiteAudioAacAttribute_Force32 = 0x7FFFFFFF
}NvMMLiteAudioAacAttribute;

#define HW_VERSION_AP10             0x10
#define HW_VERSION_AP15             0x15
#define TIMER_MULTIPLIER_REG        0x60005014
#define TIMER_MULTIPLIER_FACTOR     0x218

#define NvMMLite_AAC_TRACK_ALL                       0
#define NvMMLite_AAC_TRACK_LEFT                     -1
#define NvMMLite_AAC_TRACK_RIGHT                     1

typedef struct
{
    NvU32 vdeDeviceId;
} NvMMLiteAudioCapabilities;


/*AAC mode type.  Note that the term profile is used with the MPEG-2
    standard and the term object type and profile is used with MPEG-4 */

/** AAC tool usage (for nAACtools in OMX_AUDIO_PARAM_AACPROFILETYPE).
 * Required for encoder configuration and optional as decoder info output.
 * For MP3, OMX_AUDIO_CHANNELMODETYPE is sufficient. */

typedef enum
{
    NvMMLite_AUDIO_AACToolNone = 0x00000000, /**< no AAC tools allowed (encoder config) or active (decoder info output) */
    NvMMLite_AUDIO_AACToolMS   = 0x00000001, /**< MS: Mid/side joint coding tool allowed or active */
    NvMMLite_AUDIO_AACToolIS   = 0x00000002, /**< IS: Intensity stereo tool allowed or active */
    NvMMLite_AUDIO_AACToolTNS  = 0x00000004, /**< TNS: Temporal Noise Shaping tool allowed or active */
    NvMMLite_AUDIO_AACToolPNS  = 0x00000008, /**< PNS: MPEG-4 Perceptual Noise substitution tool allowed or active */
    NvMMLite_AUDIO_AACToolLTP  = 0x00000010, /**< LTP: MPEG-4 Long Term Prediction tool allowed or active */
    NvMMLite_AUDIO_AACToolAll  = 0x7FFFFFFF /**< all AAC tools allowed or active (*/
}NvMMLiteAudioAacTools;

/** MPEG-4 AAC error resilience (ER) tool usage (for nAACERtools in OMX_AUDIO_PARAM_AACPROFILETYPE).
 * Required for ER encoder configuration and optional as decoder info output */
typedef enum
{
    NvMMLite_AUDIO_AACERNone  = 0x00000000, /**< no AAC ER tools allowed/used */
    NvMMLite_AUDIO_AACERVCB11 = 0x00000001, /**< VCB11: Virtual Code Books for AAC section data */
    NvMMLite_AUDIO_AACERRVLC  = 0x00000002, /**< RVLC: Reversible Variable Length Coding */
    NvMMLite_AUDIO_AACERHCR   = 0x00000004, /**< HCR: Huffman Codeword Reordering */
    NvMMLite_AUDIO_AACERAll   = 0x7FFFFFFF  /**< all AAC ER tools allowed/used */
}NvMMLiteAudioAacERTools;



typedef enum
{
    NvMMLiteAudio_AACObjectNull = 0,/**< Null, not used */
    NvMMLiteAudio_AACObjectMain,/**< AAC Main object */
    NvMMLiteAudio_AACObjectLC,/**< AAC Low Complexity object (AAC profile) */
    NvMMLiteAudio_AACObjectSSR,/**< AAC Scalable Sample Rate object */
    NvMMLiteAudio_AACObjectLTP,/**< AAC Long Term Prediction object */
    NvMMLiteAudio_AACObjectHE,/**< AAC High Efficiency (object type SBR, HE-AAC profile) */
    NvMMLiteAudio_AACObjectScalable,/**< AAC Scalable object */
    NvMMLiteAudio_AACObjectTwinVQ,
    NvMMLiteAudio_AACObjectCELP,
    NvMMLiteAudio_AACObjectHVXC,
    NvMMLiteAudio_AACObjectTTSI = 12,
    NvMMLiteAudio_AACObjectMainSynthetic,
    NvMMLiteAudio_AACObjectWavetableSynthesis,
    NvMMLiteAudio_AACObjectGeneralMIDI,
    NvMMLiteAudio_AACObjectAlgorithmSynthesisAndAudioFX,
    NvMMLiteAudio_AACObjectERLC,/**< ER AAC Low Complexity object (Error Resilient AAC-LC) */
    NvMMLiteAudio_AACObjectERLTP = 19,
    NvMMLiteAudio_AACObjectERScalable,
    NvMMLiteAudio_AACObjectERTwinVQ,
    NvMMLiteAudio_AACObjectERBSAC,
    NvMMLiteAudio_AACObjectLD,/**< AAC Low Delay object (Error Resilient) */
    NvMMLiteAudio_AACObjectERCELP,
    NvMMLiteAudio_AACObjectERHVXC,
    NvMMLiteAudio_AACObjectERHILN,
    NvMMLiteAudio_AACObjectParametric,
    NvMMLiteAudio_AACObjectHE_PS = 29,/**< AAC High Efficiency with Parametric Stereo coding (HE-AAC v2, object type PS) */
    NvMMLiteAudio_AACObjectHE_MPS,    /**< AAC High Efficiency with MPEG Surround Coding */
    NvMMLiteAudio_AACObjectUnknown = 0x7FFFFFFF
} NvMMLiteAudioAacProfileType;

typedef enum
{
    NvMMLiteAudio_AACStreamFormatMP2ADTS = 0,/**< AAC Audio Data Transport Stream 2 format */
    NvMMLiteAudio_AACStreamFormatMP4ADTS,/**< AAC Audio Data Transport Stream 4 format */
    NvMMLiteAudio_AACStreamFormatMP4LOAS,/**< AAC Low Overhead Audio Stream format */
    NvMMLiteAudio_AACStreamFormatMP4LATM,/**< AAC Low overhead Audio Transport Multiplex */
    NvMMLiteAudio_AACStreamFormatADIF,/**< AAC Audio Data Interchange Format */
    NvMMLiteAudio_AACStreamFormatMP4FF,/**< AAC inside MPEG-4/ISO File Format */
    NvMMLiteAudio_AACStreamFormatRAW,/**< AAC Raw Format */
    NvMMLiteAudio_AACStreamFormatUnknown = 0x7FFFFFFF
} NvMMLiteAudioAacStreamFormatType;

typedef enum
{
    NvMMLiteAudio_ChannelModeStereo = 0,/**< 2 channels, the bitrate allocation between those two channels changes accordingly to each channel information */
    NvMMLiteAudio_ChannelModeJointStereo,/**< mode that takes advantage of what is common between 2 channels for higher compression gain */
    NvMMLiteAudio_ChannelModeDual,/**< 2 mono-channels, each channel is encoded with half the bitrate of the overall bitrate */
    NvMMLiteAudio_ChannelModeMono,/**< Mono channel mode */
    NvMMLiteAudio_ChannelModeUnknown = 0x7FFFFFFF
} NvMMLiteAudioChannelModeType;

/** AAC params */
typedef struct
{
    NvU32 nChannels;/**< Number of channels */
    NvU32 nSampleRate;/**< Sampling rate of the source data.  Use 0 for variable or unknown sampling rate. */
    NvU32 nBitRate;/**< Bit rate of the input data.  Use 0 for variable  rate or unknown bit rates */
    NvU32 nAudioBandWidth;/**< Audio band width (in Hz) to which an encoder should limit the audio signal. Use 0 to let encoder decide */
    NvU32 nFrameLength;/**< Frame length (in audio samples per channel) of the codec. Can be 1024 or 960 (AAC-LC), 2048 (HE-AAC), 480 or 512 (AAC-LD)*/
    NvU32 nAACtools;/**< AAC tool usage */
    NvU32 nAACERtools;/**< MPEG-4 AAC error resilience tool usage */
    NvMMLiteAudioAacProfileType eAACProfile;/**< AAC profile enumeration */
    NvMMLiteAudioAacStreamFormatType eAACStreamFormat;/**< AAC stream format enumeration */
    NvMMLiteAudioChannelModeType eChannelMode;/**< Channel mode enumeration */
} NvMMLiteAudioAacParam;

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NvMMLITE_AAC_H
