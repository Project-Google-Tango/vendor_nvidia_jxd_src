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
 *           NvMM Wave APIs</b>
 *
 * @b Description: Declares Interface for NvMM Audio Wave APIs.
 */

#ifndef INCLUDED_NVMM_WAVE_H
#define INCLUDED_NVMM_WAVE_H

/**
 * @defgroup nvmm_wavedec Audio Wave Multimedia API
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
 * @brief Audio Wave Attribute enumerations.
 * Following enum are used by audio wave for setting the attributes.
 * They are used as 'eAttributeType' variable in SetAttribute() API.
 * @see SetAttribute
 */
typedef enum
{
    NvMMWaveAttribute_CommonStreamProperty = (NvMMAttributeAudioDec_Unknown + 1), /* uses NvMMWaveStreamProperty */
    NvMMWaveAttribute_Position,
    NvMMWaveAttribute_TrackInfo,
    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMWaveAttribute_Force32 = 0x7FFFFFFF

}NvMMAudioWaveAttribute;

typedef enum
{
    NvMMAudioWavStream_In = 0, //!< Input
    NvMMAudioWavStream_Out,    //!< Output
    NvMMAudioWavStreamCount

} NvMMWavStream;

typedef struct
{
    // TODO: the 2 should not be hardcoded
    // Holds the current position of all streams
    NvU32 position[2];

} NvMMWavePosition;

typedef enum
{
    NvMMAudioWavMode_PCM = 0,
    NvMMAudioWavMode_ALAW,
    NvMMAudioWavMode_MULAW,    
    NvMMAudioWavMode_Force32 = 0x7FFFFFFF
}NvMMAudioWavMode;


/**
 * @brief Defines the structure for holding the stream dependent properties like
 * resolution, fps etc. Client should request for this structure filled by decoder
 * via GetAttribute() call, and the request should be made only after decoder is
 * done with bitstream header decoding.
 * @see GetAttribute
 */
typedef struct NvMMWaveStreamProperty_
{
    NvU32 structSize;
    /// Index of stream with given properties
    NvU32 StreamIndex;
    /// Output sampler rate of stream in Hz
    NvU32 SampleRate;
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
    //WAV format(alaw, ulaw and wav)
    NvU16 audioFormat;
    //ADPCM parameters
    NvU16 nCoefs;
    NvU16 samplesPerBlock;
    NvU32 uDataLength;
} NvMMWaveStreamProperty;


/** 
* @brief WAVE codec Parameters
*/
typedef struct 
{
    NvU32 structSize;
    /* Encoding mode */
    NvMMAudioWavMode   Mode;
    /* Control for enable/disable of CRC checking for error resiliency */    
    NvU32 SampleRate;
    /* Bitrate of stream in bits/second */
    NvU32 BitRate;
    /* Number of input audio channels to encode */
    NvU32 InputChannels;   

} NvMMAudioEncPropertyWAV;


/**
 * @brief Defines the structure for holding the configuartion for the decoder
 * capabilities. These are stream independent properties. Decoder fills this
 * structure on NvMMGetMMBlockCapabilities() function being called by Client.
 * @see NvMMGetMMBlockCapabilities
 */
typedef struct
{
    NvU32 structSize;
    /// Holds minimum sample rate supported by Wave decoder
    NvU32 MinSampleRate;
    /// Holds maximum sample rate supported by Wave decoder 
    NvU32 MaxSampleRate;
     /// Holds minimum bits per sample supported by Wave decoder
    NvU32 MinBitsPerSample;
    /// Holds maximum bits per sample supported by Wave decoder 
    NvU32 MaxBitsPerSample;
    /// Holds maximum number of channels supported by Wave decoder
    NvU32 Channels;

} NvMMWaveCapabilities;

#define min(a,b) (((a) < (b)) ? (a) : (b))

/** 
* @brief Wave codec Parameters
*/
#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM     1
#endif
#ifndef WAVE_FORMAT_EXTENSIBLE
#define WAVE_FORMAT_EXTENSIBLE (0xfffe)
#endif
#ifndef WAVE_FORMAT_ADPCM
#define WAVE_FORMAT_ADPCM  (0x0002)
#endif
#ifndef WAVE_FORMAT_ALAW
#define WAVE_FORMAT_ALAW  (0x0006)
#endif
#ifndef WAVE_FORMAT_MULAW
#define WAVE_FORMAT_MULAW  (0x0007)
#endif

typedef struct
{
    NvU8    Id[4];
    NvU32   Size;
    NvU8    Type[4];

} WAVE_Header;

typedef struct
{
    NvU16   Format;
    NvU16   Channels;
    NvU32   SamplesPerSec;
    NvU32   BytesPerSec;
    NvU16   BlockAlign;
    NvU16   BitsPerSample;

} WAVE_FMT_Header;

typedef struct
{
    NvU16   Size;
    NvU16   SamplesPerBlock;

} WAVE_FMT_EX_Header;

typedef struct
{
    NvU8    Id[4];
    NvU32   Size;

} WAVE_CHUNK_Header;

typedef struct
{
    WAVE_Header         hdr;
    WAVE_FMT_Header     hdrFormat;
    WAVE_FMT_EX_Header  hdrFormatex;
    WAVE_CHUNK_Header   hdrChunk;
    NvU32               amountRemaining;
    NvU32               amountToRead;       // Calculated

}WAVE_STREAM, *PWAVE_STREAM;

/** The NvMMNumericalDataType enumeration is used to indicate if data
    is signed or unsigned
 */
typedef enum
{
    NvMM_NumericalDataSigned, /**< signed data */
    NvMM_NumericalDataUnsigned, /**< unsigned data */
    NvMM_NumercialDataUnknown = 0x7FFFFFFF
} NvMMNumericalDataType;

/** The NvMMEndianType enumeration is used to indicate the bit ordering
    for numerical data (i.e. big endian, or little endian).
 */
typedef enum
{
    NvMM_EndianBig, /**< big endian */
    NvMM_EndianLittle, /**< little endian */
    NvMM_EndianUnknown = 0x7FFFFFFF
}NvMMEndianType;

/** PCM mode type  */
typedef enum
{
    NvMM_PCMModeLinear = 0,  /**< Linear PCM encoded data */
    NvMM_PCMModeALaw,        /**< A law PCM encoded data (G.711) */
    NvMM_PCMModeMULaw,       /**< Mu law PCM encoded data (G.711)  */
    NvMM_PCMModeUnknown = 0x7FFFFFFF
}NvMMPcmModeType;

typedef enum
{
    NvMM_ChannelNone = 0x0,    /**< Unused or empty */
    NvMM_ChannelLF   = 0x1,    /**< Left front */
    NvMM_ChannelRF   = 0x2,    /**< Right front */
    NvMM_ChannelCF   = 0x3,    /**< Center front */
    NvMM_ChannelLS   = 0x4,    /**< Left surround */
    NvMM_ChannelRS   = 0x5,    /**< Right surround */
    NvMM_ChannelLFE  = 0x6,    /**< Low frequency effects */
    NvMM_ChannelCS   = 0x7,    /**< Back surround */
    NvMM_ChannelLR   = 0x8,    /**< Left rear. */
    NvMM_ChannelRR   = 0x9,    /**< Right rear. */
    NvMM_ChannelUnknown  = 0x7FFFFFFF
}NvMMChannelType;


/** PCM format description */
typedef struct  {
    NvU32 nChannels;                /**< Number of channels (e.g. 2 for stereo) */
    NvMMNumericalDataType eNumData;   /**< indicates PCM data as signed or unsigned */
    NvMMEndianType eEndian;           /**< indicates PCM data as little or big endian */
    NvBool bInterleaved;            /**< True for normal interleaved data; false for
                                           non-interleaved data (e.g. block data) */
    NvU32 nBitPerSample;            /**< Bit per sample */
    NvU32 nSamplingRate;            /**< Sampling rate of the source data.  Use 0 for
                                           variable or unknown sampling rate. */
    NvMMPcmModeType ePCMMode;   /**< PCM mode enumeration */
    NvMMChannelType eChannelMapping[16]; /**< Slot i contains channel defined by eChannelMap[i] */
}NvMMAudioPcmParam;

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_WAVE_H
