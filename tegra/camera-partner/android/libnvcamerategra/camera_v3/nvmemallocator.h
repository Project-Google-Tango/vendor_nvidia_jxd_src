/*
 * Copyright (c) 2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef NV_MEM_ALLOCATOR_H
#define NV_MEM_ALLOCATOR_H

#include "nvmm.h"
#include "nvrm_surface.h"

namespace android {

typedef enum {
    NV_CAMERA_HAL3_COLOR_YV12,
    NV_CAMERA_HAL3_COLOR_NV12,
    NV_CAMERA_HAL3_COLOR_NV21
} NvCameraHAL3ColorFormat;

class NvMemAllocator
{
public:
    static NvError AllocateNvMMSurface(
        NvMMSurfaceDescriptor *pSurface,
        NvU32 Width,
        NvU32 Height,
        NvCameraHAL3ColorFormat format);

    static void FreeSurface(NvMMSurfaceDescriptor *pSurface);

private:
    NvMemAllocator() {};
    ~NvMemAllocator() {};
};

}
#endif // NV_MEM_ALLOCATOR_H
