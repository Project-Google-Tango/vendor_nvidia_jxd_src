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
 *           NvMMLite Audio Decoder APIs</b>
 *
 * @b Description: Declares Interface for NvMMLite Audio Decoder APIs.
 */

#ifndef INCLUDED_NVMMLITE_AUDIODEC_H
#define INCLUDED_NVMMLITE_AUDIODEC_H

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

/**
 * @brief Audio Decoder Attribute enumerations.
 * Following enum are used by audio decoders for setting the attributes.
 * They are used as 'eAttributeType' variable in SetAttribute() API.
 * @see SetAttribute
 */
enum
{
    /* uses NvMMLiteAudioDecBitStreamProperty */
    NvMMLiteAttributeAudioDec_CommonStreamProperty =
                    (NvMMLiteAttributeAudioDec_Unknown + 1),
    NvMMLiteAttributeAudioDec_AacStreamProperty,
    NvMMLiteAttributeAudioDec_AmrStreamProperty,
    NvMMLiteAttributeAudioDec_AmrwbStreamProperty,
    NvMMLiteAttributeAudioDec_Mp3StreamProperty,
    NvMMLiteAttributeAudioDec_RateInfo,
    NvMMLiteAttributeAudioDec_SilenceDiscard,
    NvMMLiteAttributeAudioDec_SyncDecode,
    NvMMLiteAttributeAudioDec_WmaStreamProperty,
    NvMMLiteAttributeAudioDec_PcmStreamProperty,
    NvMMLiteAttributeAudioDec_MaxOutputChannels,
    NvMMLiteAttributeAudioDec_DualMonoOutputMode,
    NvMMLiteAttributeAudioDec_AacCapabilities,
    NvMMLiteAttributeAudioDec_WmaCapabilities,
    NvMMLiteAttributeAudioDec_Mp3Capabilities,
    NvMMLiteAttributeAudioDec_GaplessPlayback = 0x7FFFFFFE,
    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMLiteAttributeAudioDec_Force32 = 0x7FFFFFFF
};


typedef struct
{
    NvU32 Rate;

}NvMMLiteAudioRateInfo;


/**
 * @brief Defines the structure for holding the stream dependent properties
 * like resolution, fps etc. Client should request for this structure filled by
 * decoder via GetAttribute() call, and the request should be made only after
 * decoder is done with bitstream header decoding.
 * @see GetAttribute
 */
typedef struct NvMMLiteAudioDecBitStreamProperty_
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

} NvMMLiteAudioDecBitStreamProperty;

/** The NvMMLiteNumericalDataType enumeration is used to indicate if data
    is signed or unsigned
 */
typedef enum
{
    NvMMLite_NumericalDataSigned, /**< signed data */
    NvMMLite_NumericalDataUnsigned, /**< unsigned data */
    NvMMLite_NumercialDataUnknown = 0x7FFFFFFF
} NvMMLiteNumericalDataType;

/** The NvMMLiteEndianType enumeration is used to indicate the bit ordering
    for numerical data (i.e. big endian, or little endian).
 */
typedef enum
{
    NvMMLite_EndianBig, /**< big endian */
    NvMMLite_EndianLittle, /**< little endian */
    NvMMLite_EndianUnknown = 0x7FFFFFFF
}NvMMLiteEndianType;

/** PCM mode type  */
typedef enum
{
    NvMMLite_PCMModeLinear = 0,  /**< Linear PCM encoded data */
    NvMMLite_PCMModeALaw,        /**< A law PCM encoded data (G.711) */
    NvMMLite_PCMModeMULaw,       /**< Mu law PCM encoded data (G.711)  */
    NvMMLite_PCMModeUnknown = 0x7FFFFFFF
}NvMMLitePcmModeType;

typedef enum
{
    NvMMLite_ChannelNone = 0x0,    /**< Unused or empty */
    NvMMLite_ChannelLF   = 0x1,    /**< Left front */
    NvMMLite_ChannelRF   = 0x2,    /**< Right front */
    NvMMLite_ChannelCF   = 0x3,    /**< Center front */
    NvMMLite_ChannelLS   = 0x4,    /**< Left surround */
    NvMMLite_ChannelRS   = 0x5,    /**< Right surround */
    NvMMLite_ChannelLFE  = 0x6,    /**< Low frequency effects */
    NvMMLite_ChannelCS   = 0x7,    /**< Back surround */
    NvMMLite_ChannelLR   = 0x8,    /**< Left rear. */
    NvMMLite_ChannelRR   = 0x9,    /**< Right rear. */
    NvMMLite_ChannelUnknown  = 0x7FFFFFFF
}NvMMLiteChannelType;


typedef enum
{
    NvMMLite_ChannelModeStereo = 0,/**< 2 channels, the bitrate allocation between those two channels changes accordingly to each channel information */
    NvMMLite_ChannelModeJointStereo,/**< mode that takes advantage of what is common between 2 channels for higher compression gain */
    NvMMLite_ChannelModeDual,/**< 2 mono-channels, each channel is encoded with half the bitrate of the overall bitrate */
    NvMMLite_ChannelModeMono,/**< Mono channel mode */
    NvMMLite_ChannelModeUnknown = 0x7FFFFFFF
} NvMMLiteChannelModeType;

/** PCM format description */
typedef struct  {
    NvU32 nChannels;                /**< Number of channels (e.g. 2 for stereo) */
    NvMMLiteNumericalDataType eNumData;   /**< indicates PCM data as signed or unsigned */
    NvMMLiteEndianType eEndian;           /**< indicates PCM data as little or big endian */
    NvBool bInterleaved;            /**< True for normal interleaved data; false for
                                           non-interleaved data (e.g. block data) */
    NvU32 nBitPerSample;            /**< Bit per sample */
    NvU32 nSamplingRate;            /**< Sampling rate of the source data.  Use 0 for
                                           variable or unknown sampling rate. */
    NvMMLitePcmModeType ePCMMode;   /**< PCM mode enumeration */
    NvMMLiteChannelType eChannelMapping[16]; /**< Slot i contains channel defined by eChannelMap[i] */
}NvMMLiteAudioPcmParam;

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
    NvU32 * pSampleRatesSupported;  /**< Indexed array containing the supported sampling rates. Ignored if
                                              isFreqRangeContinuous is true */
    NvU32 nSampleRatesSupported;  /**< Size of the pSamplingRatesSupported array */

    NvU32 nMinBitRate;              /**< Holds minimum bitrate supported by decoder in bps */
    NvU32 nMaxBitRate;              /**< Holds maximum bitrate supported by decoder in bps */
    NvBool isBitrateRangeContinuous;/**< Returns true if the device supports a continuous range of
                                              bitrates between minBitRate and maxBitRate */
    NvU32 * pBitratesSupported;     /**< Indexed array containing the supported bitrates. Ignored if
                                              isBitrateRangeContinuous is true */
    NvU32 nBitratesSupported;     /**< Size of the pBitratesSupported array. Ignored if
                                            isBitrateRangeContinuous is true */
    NvU32 nProfileType;             /**< Holds profile type  */
    NvU32 nModeType;                /**< Hold Mode type */
    NvU32 nStreamFormatType;        /**< Hold StreamFormat type */
} NvMMLiteAudioDecoderCapability;



#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMMLITE_AUDIODEC_H
