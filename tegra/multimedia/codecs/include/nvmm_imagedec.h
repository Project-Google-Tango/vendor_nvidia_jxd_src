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
 *           NvMM Image Decoder APIs</b>
 *
 * @b Description: Declares Interface for NvMM Image Decoder APIs.
 */

#ifndef INCLUDED_NVMM_IMAGEDEC_H
#define INCLUDED_NVMM_IMAGEDEC_H

/**
 * @defgroup nvmm_imagedec Image Decode API
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
 * @brief Image Decode Attribute enumerations.
 * Following enum are used by image encoders for setting the attributes.
 * They are used as 'eAttributeType' variable in SetAttribute() API.
 * @see SetAttribute
 */
enum
{
    /* uses NvMMVideoEncBitStreamProperty */
    NvMMAttributeImageDec_CommonStreamProperty =
                        (NvMMAttributeImageDec_Unknown + 1),
    NvMMAttribtueImageDec_JPEGStreamProperty,

    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMAttributeImageDec_Force32 = 0x7FFFFFFF
};

/**
 * @brief Defines the structure for holding the decoding attributes
 * that can be set by SetAttribute() call. 
 * @see GetAttribute
 */
typedef struct
{
    NvU32 StructSize;
    /// Index of stream with given properties
    NvU32 StreamIndex;
    /// Bitrate of stream in bits/second
    NvU32 BitRate;

} NvMMVideoEncBitStreamProperty;

/**
 * @brief Defines the structure for holding the configuartion for the encoder
 * capabilities. These are stream independent properties. Encoder fills this
 * structure on NvMMGetMMBlockCapabilities() function being called by Client.
 * @see NvMMGetMMBlockCapabilities
 */
typedef struct
{
    NvU32 StructSize;
     /// Holds maximum bitrate supported by encoder in bps
    NvU32 MaxBitRate;
} NvMMVideoEncCapabilities;

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_IMAGEDEC_H
