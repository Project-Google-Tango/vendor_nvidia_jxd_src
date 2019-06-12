/*
 * Copyright (c) 2007-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit:
 *           NvMM Audio Decoder APIs</b>
 *
 * @b Description: Declares Interface for NvMM Audio Decoder APIs.
 */

#ifndef INCLUDED_NVMM_AUDIODEC_H
#define INCLUDED_NVMM_AUDIODEC_H

/**
 * @defgroup nvmm_audiodec Audio Decoder Multimedia API
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

#define MAX_SAMPLERATE_TABLE_SIZE  16  //max sample rate array can be of 16
#define MAX_BITRATE_TABLE_SIZE     24  //max bit rate array can be of 24

/**
 * @brief Audio Decoder Attribute enumerations.
 * Following enum are used by audio decoders for setting the attributes.
 * They are used as 'eAttributeType' variable in SetAttribute() API.
 * @see SetAttribute
 */
enum
{
    /* uses NvMMAudioDecBitStreamProperty */
    NvMMAttributeAudioDec_CommonStreamProperty =
                    (NvMMAttributeAudioDec_Unknown + 1),
    NvMMAttributeAudioDec_AacStreamProperty,
    NvMMAttributeAudioDec_AmrStreamProperty,
    NvMMAttributeAudioDec_AmrwbStreamProperty,
    NvMMAttributeAudioDec_Mp3StreamProperty,
    NvMMAttributeAudioDec_RateInfo,
    NvMMAttributeAudioDec_SilenceDiscard,
    NvMMAttributeAudioDec_SyncDecode,
    NvMMAttributeAudioDec_WmaStreamProperty,
    NvMMAttributeAudioDec_PcmStreamProperty,
    NvMMAttributeAudioDec_AacCapabilities,
    NvMMAttributeAudioDec_Mp3Capabilities,
    NvMMAttributeAudioDec_WmaCapabilities,
    NvMMAttributeAudioDec_AmrCapabilities,
    NvMMLiteAttributeAudioDec_GaplessPlayback = 0x7FFFFFFE,
    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMAttributeAudioDec_Force32 = 0x7FFFFFFF
};


typedef struct
{
    NvU32 Rate;

}NvMMAudioRateInfo;


/**
 * @brief Defines the structure for holding the stream dependent properties
 * like resolution, fps etc. Client should request for this structure filled by
 * decoder via GetAttribute() call, and the request should be made only after
 * decoder is done with bitstream header decoding.
 * @see GetAttribute
 */
typedef struct NvMMAudioDecBitStreamProperty_
{
    NvU32 structSize;
    /// Index of stream with given properties
    NvU32 StreamIndex;
    /// Output sampler rate of stream in Hz
    NvU32 SampleRate;
    /// Bitrate of stream in bits/second
    NvU32 BitRate;
    /// Number of valid bits in a sample
    NvU32 BitsPerSample;
    /** size of the sample container in byte
     * For example 24bit data in 32bit container would be a container size of 4
     */
    NvU32 ContainerSize;
    /// Block alignment in bytes of a sample block
    NvU32 BlockAlign;
    /// Size in bytes of frames of data used by decoder
    NvU32 FrameSize;
    /// Number of output audio channels in stream
    NvU32 Channels;
    // Duration in milliseconds of stream
    NvU32 Duration;

} NvMMAudioDecBitStreamProperty;

/**
 * @brief Defines the structure for holding the configuartion for the audio decoder
 * capabilities. These are stream independent properties. Decoder fills this
 * structure and pass to the IL-Client.
 */
typedef struct
{
    NvU32 nMaxChannels;             /**< Holds maximum number of channels supported by decoder */
    NvU32 nMinBitsPerSample;        /**< Holds minimum number of bits required for each sample supported by decoder */
    NvU32 nMaxBitsPerSample;        /**< Holds maximum number of bits required for each sample supported by decoder */
    NvU32 nMinSampleRate;           /**< Holds maximum ample rate supported by decoder */
    NvU32 nMaxSampleRate;           /**< Holds maximum ample rate supported by decoder */
    NvBool isFreqRangeContinuous;   /**< Returns true if the device supports a continuous range of
                                                                sampling rates between minSampleRate and maxSampleRate */
    NvU32 pSampleRatesSupported[MAX_SAMPLERATE_TABLE_SIZE]; /**< Indexed array containing the supported sampling rates.
                                                                  Ignored if isFreqRangeContinuous is true */
    NvU32 nSampleRatesSupported;  /**< Size of the pSamplingRatesSupported array */

    NvU32 nMinBitRate;              /**< Holds minimum bitrate supported by decoder in bps */
    NvU32 nMaxBitRate;              /**< Holds maximum bitrate supported by decoder in bps */
    NvBool isBitrateRangeContinuous;/**< Returns true if the device supports a continuous range of
                                                              bitrates between minBitRate and maxBitRate */
    NvU32 pBitratesSupported[MAX_BITRATE_TABLE_SIZE]; /**< Indexed array containing the supported bitrates. Ignored if
                                                                                 isBitrateRangeContinuous is true */
    NvU32 nBitratesSupported;     /**< Size of the pBitratesSupported array. Ignored if
                                                          isBitrateRangeContinuous is true */
    NvU32 nProfileType;             /**< Holds profile type  */
    NvU32 nModeType;                /**< Hold Mode type */
    NvU32 nStreamFormatType;        /**< Hold StreamFormat type */
}NvMMAudioDecoderCapability;

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_AUDIODEC_H
