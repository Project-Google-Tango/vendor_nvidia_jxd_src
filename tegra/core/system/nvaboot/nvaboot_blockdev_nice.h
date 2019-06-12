/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** User-friendly functions for using the very verbose NvDdkBlockDev
 *  interfaces */

#ifndef NVABOOT_BLOCKDEV_NICE_H
#define NVABOOT_BLOCKDEV_NICE_H


#include "nvcommon.h"
#include "nvddk_blockdevmgr.h"
#include "nvddk_blockdev.h"
#include "nvos.h"

#ifdef __cplusplus
extern "C"
{
#endif

void NvDdkBlockDevGetDeviceInfo(
    NvDdkBlockDevHandle hBlockDev,
    NvDdkBlockDevInfo *pBlockDevInfo);

void NvDdkBlockDevFlushCache(NvDdkBlockDevHandle hBlockDev);

void NvDdkBlockDevClose(NvDdkBlockDevHandle hBlockDev);

NvError NvDdkBlockDevIoctl(
    NvDdkBlockDevHandle hBlockDev,
    NvU32               Opcode,
    NvU32               InputSize,
    NvU32               OutputSize,
    const void         *pInput,
    void               *pOutput);

NvError NvDdkBlockDevMapLogicalToPhysical(
    NvDdkBlockDevHandle hBlockDev,
    NvU32               Logical,
    NvU32              *pPhysical);

NvError NvDdkBlockDevWriteSector(
    NvDdkBlockDevHandle hBlockDev,
    NvU32               SectorNum,
    const void*         pBuffer,
    NvU32               NumSectors,
    NvBool              LogicalAddress);

NvError NvDdkBlockDevReadSector(
    NvDdkBlockDevHandle hBlockDev,
    NvU32               SectorNum,
    void               *pBuffer,
    NvU32               NumSectors,
    NvBool              LogicalAddress);

#ifdef __cplusplus
}
#endif


#endif
