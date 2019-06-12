/*
 * Copyright (c) 2011 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVXCAMERA_H
#define INCLUDED_NVXCAMERA_H

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Defines the pixel formats for buffer negotiation
**/
typedef enum
{
    NvxFOURCCFormat_YV16 = 1, //YUV422 Planar
    NvxFOURCCFormat_NV16,     //YUV422 Semi-planar U followed by V
    NvxFOURCCFormat_NV61,     //YUV422 Semi-planar V followed by U
    NvxFOURCCFormat_YV12,     //YUV420 Planar
    NvxFOURCCFormat_NV12,     //YUV420 Semi-planar U followed by V
    NvxFOURCCFormat_NV21,     //YUV420 Semi-planar V followed by U
    NvxFOURCCFormat_Force32 = 0x7FFFFFFF
} NvxFOURCCFormat;

NvError NvxCameraUpdateExposureRegions(void *pNvxCamera);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVXCAMERA_H
