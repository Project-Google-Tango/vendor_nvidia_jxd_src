/*
 * Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVCMM_H
#define INCLUDED_NVCMM_H

#include <nvcolor.h>

/*
 * Core data types
 */
typedef enum
{
    NvCmsAbsColorSpace_None           = 0,

    NvCmsAbsColorSpace_DeviceRGB,

    NvCmsAbsColorSpace_sRGB,
    NvCmsAbsColorSpace_AdobeRGB,
    NvCmsAbsColorSpace_WideGamutRGB,

    NvCmsAbsColorSpace_Rec709,

    NvCmsAbsColorSpace_CIEXYZ,
    NvCmsAbsColorSpace_CIELAB,

    NvCmsAbsColorSpace_Force32        = 0x7FFFFFFF
} NvCmsAbsColorSpace;

typedef union
{
    NvF32 data[16];
    NvF32 d2d[4][4];
} NvCmsMatrix;

typedef union
{
    NvF32 data[4];
} NvCmsVector;

typedef struct NvCmsContextRec NvCmsContext;

typedef struct NvCmsProfileRec NvCmsProfile;
typedef struct NvCmsDisplayProfileRec NvCmsDisplayProfile;
typedef struct NvCmsDeviceLinkProfileRec NvCmsDeviceLinkProfile;

#endif // INCLUDED_NVCMM_H
