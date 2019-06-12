/*
 * Copyright (c) 2011-2012, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef INCLUDED_NVMM_UTIL_H
#define INCLUDED_NVMM_UTIL_H

#include "nvcommon.h"
#include "nvos.h"
#include "nvmm.h"
#include "nvrm_init.h"
#include "nvmm_event.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/** TEGRA ARCH. */
#define HW_ARCH_TEGRA_30 3
#define HW_ARCH_TEGRA_T114 4
#define HW_ARCH_TEGRA_T148 5
#define HW_ARCH_TEGRA_T124 6

/** HW chip arch struct. */
typedef struct
{
    NvU32 ChipId;
}HwChipArch;

NvError
NvMMUtilAllocateBuffer(
    NvRmDeviceHandle hRmDevice,
    NvU32 size,
    NvU32 align,
    NvMMMemoryType memoryType,
    NvBool bInSharedMemory,
    NvMMBuffer **pBuffer);

NvError
NvMMUtilDeallocateBuffer(
    NvMMBuffer *pBuffer);

NvError NvMMUtilAllocateVideoBuffer(
    NvRmDeviceHandle hRmDevice,
    NvMMVideoFormat VideoFormat,
    NvMMBuffer **pBuf);

NvError NvMMUtilDeallocateVideoBuffer(NvMMBuffer *pBuffer);

NvError
NvMMUtilDeallocateBlockSideBuffer(
    NvMMBuffer *pBuffer,
    void *hService,
    NvBool bAbnormalTermination);

enum { NVMM_SURFACE_RESTRICTION_PITCH_16BYTE_ALIGNED  = (1UL << 0),
       NVMM_SURFACE_RESTRICTION_YPITCH_EQ_2X_UVPITCH  = (1UL << 1),
       NVMM_SURFACE_RESTRICTION_HEIGHT_16BYTE_ALIGNED = (1UL << 2),
       NVMM_SURFACE_RESTRICTION_WIDTH_16BYTE_ALIGNED  = (1UL << 3),
       NVMM_SURFACE_RESTRICTION_NEED_TO_CROP          = (1UL << 4)};

/**
 * NvMMUtilAllocateSurfaces
 *
 * @param hRm Rm device handle
 * @param pSurfDesc a pointer to an NvMMSurfaceDescriptor which needs surfaces
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

NvError NvMemAlloc(NvRmDeviceHandle hRmDev,
                          NvRmMemHandle* phMem,
                          NvU32 Size,
                          NvU32 Alignment,
                          NvU32 *PhysicalAddress);

NvError
NvMMUtilAllocateSurfaces(
    NvRmDeviceHandle hRmDev,
    NvMMSurfaceDescriptor *pSurfaceDesc);

void NvMemFree(NvRmMemHandle hMem);

void NvMMUtilDestroySurfaces(NvMMSurfaceDescriptor *pSurfaceDesc);

int NvMMIsUsingNewAVP(void);

NvError NvMMVP8Support(void);

NvU32 NvMMGetHwChipId(void);

#if defined(__cplusplus)
}
#endif

#endif
