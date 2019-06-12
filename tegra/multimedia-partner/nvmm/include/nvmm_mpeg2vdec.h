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
 *           NvMM Video Decoder APIs</b>
 *
 * @b Description: Declares Interface for NvMM Video Decoder APIs.
 */

#ifndef INCLUDED_NVMM_MPEG2VDEC_H
#define INCLUDED_NVMM_MPEG2VDEC_H

/** @defgroup nvmm_videodec Video Decode API
 *
 * Describe the APIs here
 *
 * @ingroup nvmm_modules
 * @{
 */

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define READ_MBERROR_FROM_FILE 0

/**
 * @brief MPEG2 Video Decoder Attribute enumerations.
 * Following enum are used by mpeg2 video decoder for setting the attributes.
 * They are used as 'eAttributeType' variable in SetAttribute() and GetAttribute() APIs.
 * @see SetAttribute
 */

typedef enum
{
    // Enable Error Concealment
    NvMMMpeg2VDecAttribute_ErrorConcealmentOptions,

    // Deinterlacing Options
    NvMMMpeg2VDecAttribute_DeinterlacingOptions,

    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMMpeg2VDecAttribute_Force32 = 0x7FFFFFFF
} NvMMMpeg2VDecAttribute;


typedef struct
{
    // StructSize
    NvU32   StructSize;

    // Enable Error Concealment
    NvBool  ErrorConceal;

    // Enable Zero MV Concealment
    NvBool  ZeroMVConceal;
#if READ_MBERROR_FROM_FILE
    // Read MBError file in driver and put errors
    // This is TEMPERORY and will be removed.
    NvBool  PutError;
#endif

} NvMMMpeg2VDec_ErrorConcealmentOptions;

// Deinterlacing methods enum
typedef enum
{
    /** NO deinterlacing */
    DeintMethod_NoDeinterlacing,

    /** Bob on full frame. Two field output one frame. */
    DeintMethod_BobAtFrameRate,

    /** Bob on full frame. Two field output two frames. */
    DeintMethod_BobAtFieldRate,

    /** Weave on full frame. Two field output one frame. (This is same as NO deinterlacing) */
    DeintMethod_WeaveAtFrameRate,

    /** Weave on full frame. Two field output two frames. */
    DeintMethod_WeaveAtFieldRate,

    /** Advanced1. DeintMethod decided at MB level. Two field output one frame. */
    DeintMethod_Advanced1AtFrameRate,

    /** Advanced1. DeintMethod decided at MB level. Two field output two frames. */
    DeintMethod_Advanced1AtFieldRate,

    DeintMethod_Force32 = 0x7FFFFFFF
} DeinterlaceMethod;

typedef struct
{
    // StructSize
    NvU32   StructSize;

    // Deinterlacing Method
    DeinterlaceMethod DeintMethod;
} NvMMMpeg2VDec_DeinterlacingOptions;

#ifdef __cplusplus
}
#endif

#endif // INCLUDED_NVMM_MPEG2VDEC_H
