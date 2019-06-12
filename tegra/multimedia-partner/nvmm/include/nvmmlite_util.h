/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef INCLUDED_NVMMLITE_UTIL_H
#define INCLUDED_NVMMLITE_UTIL_H

#include "nvcommon.h"
#include "nvos.h"
#include "nvmmlite.h"
#include "nvrm_init.h"
#include "nvmmlite_event.h"

#if defined(__cplusplus)
extern "C"
{
#endif

NvError
NvMMLiteUtilAllocateProtectedBuffer(
    NvRmDeviceHandle hRmDevice,
    NvU32 size,
    NvU32 align,
    NvMMMemoryType memoryType,
    NvBool bInSharedMemory,
    NvBool bProtected,
    NvMMBuffer **pBuffer);

#define              \
NvMMLiteUtilAllocateBuffer( \
    hRmDevice,       \
    size,            \
    align,           \
    memoryType,      \
    bInSharedMemory, \
    pBuffer)         \
NvMMLiteUtilAllocateProtectedBuffer( \
    hRmDevice,       \
    size,            \
    align,           \
    memoryType,      \
    bInSharedMemory, \
    NV_FALSE,        \
    pBuffer)

NvError
NvMMLiteUtilDeallocateBuffer(
    NvMMBuffer *pBuffer);

NvError NvMMLiteUtilAllocateVideoBuffer(
    NvRmDeviceHandle hRmDevice,
    NvMMLiteVideoFormat VideoFormat,
    NvMMBuffer **pBuf);

NvError NvMMLiteUtilDeallocateVideoBuffer(NvMMBuffer *pBuffer);

enum { NVMMLITE_SURFACE_RESTRICTION_PITCH_16BYTE_ALIGNED  = (1UL << 0),
       NVMMLITE_SURFACE_RESTRICTION_YPITCH_EQ_2X_UVPITCH  = (1UL << 1),
       NVMMLITE_SURFACE_RESTRICTION_HEIGHT_16BYTE_ALIGNED = (1UL << 2),
       NVMMLITE_SURFACE_RESTRICTION_WIDTH_16BYTE_ALIGNED  = (1UL << 3),
       NVMMLITE_SURFACE_RESTRICTION_NEED_TO_CROP          = (1UL << 4)};

/**
 * NvMMLiteUtilAllocateSurfaces
 *
 * @param hRm Rm device handle
 * @param pSurfDesc a pointer to an NvMMLiteSurfaceDescriptor which needs surfaces
 *                  allocation.
 * @param Width Surface's width
 * @param Height Surface's width
 * @param Layout Surface's layout
 * @param ColorFormat Surface's color format
 * @param NumSurfaces Number of surfaces.
 *               For YUV420 surface format:
 *                 - when 3 planes Y,U and V are in separate planes, the valid
 *                   NumSurfaces is 3.
 *                 - when Y is in one plane and UV in another plane, the valid
 *                   NumSurfaces is 2.
 *               For rest of surface formats, the valid NumSurfaces is 1.
 */

NvError
NvMMLiteUtilAllocateProtectedSurfaces(
    NvRmDeviceHandle hRmDev,
    NvMMSurfaceDescriptor *pSurfaceDesc,
    NvBool bProtected);

#define             \
NvMMLiteUtilAllocateSurfaces( \
    hRmDev,         \
    pSurfaceDesc)   \
NvMMLiteUtilAllocateProtectedSurfaces( \
    hRmDev,         \
    pSurfaceDesc,   \
    NV_FALSE)

void NvMMLiteUtilDestroySurfaces(NvMMSurfaceDescriptor *pSurfaceDesc);

#if defined(__cplusplus)
}
#endif

#endif
