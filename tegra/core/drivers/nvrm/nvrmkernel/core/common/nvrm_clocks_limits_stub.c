/*
 * Copyright (c) 2009 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvrm_clocks.h"

#if !defined(NV_SHMOO_DATA_INIT)
#define NV_SHMOO_DATA_INIT (0)
#endif

#if !NV_SHMOO_DATA_INIT
NvError
NvRmPrivChipShmooDataInit(
    NvRmDeviceHandle hRmDevice,
    NvRmChipFlavor* pChipFlavor)
{
    return NvError_NotSupported;
}
#endif

