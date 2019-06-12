/*
 * Copyright (c) 2012-2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVRM_CHIP_PRIVATE_H
#define INCLUDED_NVRM_CHIP_PRIVATE_H

#include "nvrm_init.h"

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

/**
 * Holds the GPU information.
 */
typedef struct NvRmPrivGpuInfoRec
{
    NvU32   NumPixelPipes;
    NvU32   NumALUsPerPixelPipe;
} NvRmPrivGpuInfo;

/**
 * @brief Returns GPU informations.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param pGpuInfo A pointer to a GPU information structure
 *
 * @retval ::NvError_Success
 * @retval ::NvError_BadParameter
 * @retval ::NvError_BadValue
 */
NvError NvRmPrivGpuGetInfo(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmPrivGpuInfo *pGpuInfo );

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // INCLUDED_NVRM_CHIP_PRIVATE_H
