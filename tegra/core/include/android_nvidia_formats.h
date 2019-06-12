/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef ANDROID_NVIDIA_FORMATS_H
#define ANDROID_NVIDIA_FORMATS_H

#include <hardware/hardware.h>

#ifdef __cplusplus
extern "C"
{
#endif

enum {
    HAL_NV_PIXEL_FORMAT_YCbCr_420_SP = 0x100,
    HAL_NV_PIXEL_FORMAT_YCbCr_422_SP = 0x101,
};

enum {
    OVERLAY_NV_FORMAT_YCbCr_420_SP = HAL_NV_PIXEL_FORMAT_YCbCr_420_SP,
    OVERLAY_NV_FORMAT_YCbCr_422_SP = HAL_NV_PIXEL_FORMAT_YCbCr_422_SP,
};

#ifdef __cplusplus
}
#endif

#endif
