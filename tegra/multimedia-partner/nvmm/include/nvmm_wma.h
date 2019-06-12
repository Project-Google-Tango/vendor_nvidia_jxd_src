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
 *           NvMMBlock WMA audio decoder specific structure</b>
 *
 * @b Description: Declares Interface for WMA Audio decoder.
 */

#ifndef INCLUDED_NVMM_WMA_H
#define INCLUDED_NVMM_WMA_H

/**
 * @defgroup nvmm_wma WMA Codec API
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

/** WMA Version */
typedef enum Nv_AUDIO_WMAFORMATTYPE {
  Nv_AUDIO_WMAFormatUnused = 0, /**< format unused or unknown */
  Nv_AUDIO_WMAFormat7,          /**< Windows Media Audio format 7 */
  Nv_AUDIO_WMAFormat8,          /**< Windows Media Audio format 8 */
  Nv_AUDIO_WMAFormat9,          /**< Windows Media Audio format 9 */
  Nv_AUDIO_WMAFormat10,         /**< Windows Media Audio format 10 */
  Nv_AUDIO_WMAFormatMax = 0x7FFFFFFF
} Nv_AUDIO_WMAFORMATTYPE;


/** WMA Profile */
typedef enum Nv_AUDIO_WMAPROFILETYPE {
  Nv_AUDIO_WMAProfileUnused = 0,  /**< profile unused or unknown */
  Nv_AUDIO_WMAProfileL1,
  Nv_AUDIO_WMAProfileL2,
  Nv_AUDIO_WMAProfileL3,
  Nv_AUDIO_WMAProfileL,/*Wma-Std Profiles*/
  Nv_AUDIO_WMAProfileM0a = 0x10,
  Nv_AUDIO_WMAProfileM0b,
  Nv_AUDIO_WMAProfileM1,
  Nv_AUDIO_WMAProfileM2,
  Nv_AUDIO_WMAProfileM3,
  Nv_AUDIO_WMAProfileM,/*Wma-Pro Profiles*/
  Nv_AUDIO_WMAProfileN1 = 0x20,
  Nv_AUDIO_WMAProfileN2,
  Nv_AUDIO_WMAProfileN3,
  Nv_AUDIO_WMAProfileN,/*Wma LSL Profile*/
  Nv_AUDIO_WMAProfileMax = 0x7FFFFFFF
} Nv_AUDIO_WMAPROFILETYPE;


/** Wma params */
typedef struct
{
    NvU32 nChannels;
    NvU32 nBitRate;
    Nv_AUDIO_WMAFORMATTYPE  eFormat;
    Nv_AUDIO_WMAPROFILETYPE eProfile;
    NvU32 nSamplingRate;
    NvU16 nBlockAlign;
    NvU16 nEncodeOptions;
    NvU32 nSuperBlockAlign;
} NvMMAudioWmaParam;


#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_WMA_H
