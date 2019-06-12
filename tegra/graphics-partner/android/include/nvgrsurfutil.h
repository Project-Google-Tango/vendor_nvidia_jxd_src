/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVGRSURFUTIL_H
#define INCLUDED_NVGRSURFUTIL_H

#include <hardware/gralloc.h>
#include <nvassert.h>

#ifdef PLATFORM_IS_KITKAT
#define MAP_HAL_PIXEL_FORMAT_5551
#define MAP_HAL_PIXEL_FORMAT_4444
#else
#define MAP_HAL_PIXEL_FORMAT_5551 \
    MAP(HAL_PIXEL_FORMAT_RGBA_5551,  NvColorFormat_R5G5B5A1)
#define MAP_HAL_PIXEL_FORMAT_4444 \
    MAP(HAL_PIXEL_FORMAT_RGBA_4444,  NvColorFormat_R4G4B4A4)
#endif


/*
 * Mapping of HAL formats to NvColor formats.
 * This has to be a 1-to-1 mapping.
 */
#define NVGR_HAL_NV_FORMAT_MAP()                             \
    MAP(HAL_PIXEL_FORMAT_RGBA_8888,  NvColorFormat_A8B8G8R8) \
    MAP(HAL_PIXEL_FORMAT_RGBX_8888,  NvColorFormat_X8B8G8R8) \
    MAP(HAL_PIXEL_FORMAT_RGB_888,    NvColorFormat_R8_G8_B8) \
    MAP(HAL_PIXEL_FORMAT_RGB_565,    NvColorFormat_R5G6B5)   \
    MAP(HAL_PIXEL_FORMAT_BGRA_8888,  NvColorFormat_A8R8G8B8) \
    MAP_HAL_PIXEL_FORMAT_5551                                \
    MAP_HAL_PIXEL_FORMAT_4444                                \
    MAP(NVGR_PIXEL_FORMAT_YUV420,    NvColorFormat_Y8)       \
    MAP(NVGR_PIXEL_FORMAT_UYVY,      NvColorFormat_UYVY)     \
    MAP(NVGR_PIXEL_FORMAT_ABGR_8888, NvColorFormat_R8G8B8A8) \
    MAP(HAL_PIXEL_FORMAT_RAW_SENSOR, NvColorFormat_L16)

static NV_INLINE NvColorFormat
NvGrGetNvFormat (int f)
{
#define MAP(HalFormat, NvFormat) \
    case HalFormat:              \
        return NvFormat;

    switch (f) {
        NVGR_HAL_NV_FORMAT_MAP();

    case NVGR_PIXEL_FORMAT_YUV422:
    case NVGR_PIXEL_FORMAT_YUV422R:
    case NVGR_PIXEL_FORMAT_YUV420_NV12:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
    case NVGR_PIXEL_FORMAT_NV12:
        return NvColorFormat_Y8;
    case HAL_PIXEL_FORMAT_Y8:
        return NvColorFormat_L8;
    case HAL_PIXEL_FORMAT_Y16:
        return NvColorFormat_L16;
    case HAL_PIXEL_FORMAT_BLOB:
        return NvColorFormat_A8B8G8R8;
    default:
        return NvColorFormat_Unspecified;
    }

#undef MAP
}

static NV_INLINE int
NvGrGetHalFormat (NvColorFormat f)
{
#define MAP(HalFormat, NvFormat) \
    case NvFormat:              \
        return HalFormat;

    switch (f)
    {
        NVGR_HAL_NV_FORMAT_MAP();

        default:
            // There's no invalid HAL format, but 0 is not attributed
            // and works fine for EGL's NATIVE_VISUAL_ID.
            return 0;
    }

#undef MAP
}

// Rounds up to the nearest multiple of align, assuming align is a power of 2.
static NV_INLINE int NvGrAlignUp(const NvU32 x, const NvU32 align) {
    NV_ASSERT(NV_IS_POWER_OF_2(align));
    return ((x)+align-1) & (~(align-1));
}

#endif
