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
 *           NvMM Audio Encoder APIs</b>
 *
 * @b Description: Declares Interface for NvMM Audio Encoder APIs.
 */

#ifndef INCLUDED_NVMM_AUDIOENC_H
#define INCLUDED_NVMM_AUDIOENC_H

/**
 * @defgroup nvmm_audioenc Audio Encoder Multimedia API
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
 * @brief Audio Encoder Attribute enumerations.
 * Following enum are used by audio encoders for setting the attributes.
 * They are used as 'eAttributeType' variable in SetAttribute() API.
 * @see SetAttribute
 */
enum
{
    /* uses NvMMAudioDecBitStreamProperty */
    NvMMAttributeAudioEnc_CommonStreamProperty =
                    (NvMMAttributeAudioEnc_Unknown + 1),
    NvMMAttributeAudioEnc_AacStreamProperty,
    NvMMAttributeAudioEnc_AmrStreamProperty,    
    NvMMAttributeAudioEnc_AmrwbStreamProperty,
    NvMMAttributeAudioEnc_Mode,
    NvMMAttributeAudioEnc_FileFormat,

    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMAttributeAudioEnc_Force32 = 0x7FFFFFFF
};

/**
 * @brief Defines the structure for holding the stream dependent properties
 * like resolution, fps etc. Client should request for this structure filled by
 * encoder via GetAttribute() call, and the request should be made only after
 * encoder is done with bitstream header decoding.
 * @see GetAttribute
 */
typedef struct
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
    /// Size in bytes of frames of data used by encoder
    NvU32 FrameSize;
    /// Number of input audio channels to encode
    NvU32 InputChannels;
    /// Number of output audio channels in encoded stream
    NvU32 OutputChannels;
    /// void pointer to a encoder specific bitstream property structure,
    /// which is defined in respective encoder header file

} NvMMAudioEncBitStreamProperty;

/**
 * @brief Defines the structure for holding the configuartion for the encoder
 * capabilities. These are stream independent properties. Encoder fills this
 * structure on NvMMGetMMBlockCapabilities() function being called by Client.
 * @see NvMMGetMMBlockCapabilities
 */
typedef struct
{
    NvU32 structSize;
    /// Holds minimum bitrate supported by encoder in bps
    NvU32 MinBitRate;
     /// Holds maximum bitrate supported by encoder in bps
    NvU32 MaxBitRate;
    /// Holds maximum ample rate supported by encoder
    NvU32 MinSampleRate;
    /// Holds maximum ample rate supported by encoder
    NvU32 MaxSampleRate;
    /// Holds maximum number of input channels supported by encoder
    NvU32 InputChannels;
    /// Holds maximum number of output channels supported by encoder
    NvU32 OutputChannels;

} NvMMAudioEncCapabilities;

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_AUDIOENC_H
