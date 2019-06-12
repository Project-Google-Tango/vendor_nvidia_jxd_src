/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvos.h"
#include "nvassert.h"
#include "nvrm_hardware_access.h"

NvError NvRmPhysicalMemMap(
    NvRmPhysAddr     phys,
    size_t           size,
    NvU32            flags,
    NvOsMemAttribute memType,
    void           **ptr )
{
    return NvOsPhysicalMemMap(phys, size, memType, flags, ptr);
}

void NvRmPhysicalMemUnmap(void *ptr, size_t size)
{
    NvOsPhysicalMemUnmap(ptr, size);
}
