/*
 * Copyright (c) 2009-2012 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** User-friendly functions for using the very verbose NvDdkBlockDev
 *  interfaces */


#include "nvcommon.h"
#include "nvddk_blockdevmgr.h"
#include "nvaboot_blockdev_nice.h"
#include "nvddk_blockdev.h"
#include "nvos.h"

void NvDdkBlockDevGetDeviceInfo(
    NvDdkBlockDevHandle hBlockDev,
    NvDdkBlockDevInfo *pBlockDevInfo)
{
    if (hBlockDev && hBlockDev->NvDdkBlockDevGetDeviceInfo)
        hBlockDev->NvDdkBlockDevGetDeviceInfo(hBlockDev, pBlockDevInfo);
}

void NvDdkBlockDevFlushCache(NvDdkBlockDevHandle hBlockDev)
{
    if (hBlockDev && hBlockDev->NvDdkBlockDevFlushCache)
        hBlockDev->NvDdkBlockDevFlushCache(hBlockDev);
}

void NvDdkBlockDevClose(NvDdkBlockDevHandle hBlockDev)
{
    if (hBlockDev)
    {
        if (hBlockDev->NvDdkBlockDevFlushCache)
            hBlockDev->NvDdkBlockDevFlushCache(hBlockDev);
        if (hBlockDev->NvDdkBlockDevClose)
            hBlockDev->NvDdkBlockDevClose(hBlockDev);
    }
}

#define IP(x) NvDdkBlockDevIoctl_##x##Args
#define I(x) NvDdkBlockDevIoctlType_##x

NvError NvDdkBlockDevIoctl(
    NvDdkBlockDevHandle hBlockDev,
    NvU32               Opcode,
    NvU32               InputSize,
    NvU32               OutputSize,
    const void         *pInput,
    void               *pOutput)
{
    if (hBlockDev)
        return hBlockDev->NvDdkBlockDevIoctl(hBlockDev, Opcode, InputSize,
                    OutputSize, pInput, pOutput);
    return NvError_BadParameter;
}

NvError NvDdkBlockDevMapLogicalToPhysical(
    NvDdkBlockDevHandle hBlockDev,
    NvU32               Logical,
    NvU32              *pPhysical)
{
    IP(MapLogicalToPhysicalSectorInput) in;
    IP(MapLogicalToPhysicalSectorOutput) out;
    NvError e;

    if (!hBlockDev || !pPhysical)
        return NvError_BadParameter;

    in.LogicalSectorNum = Logical;
    e = NvDdkBlockDevIoctl(hBlockDev, I(MapLogicalToPhysicalSector),
            sizeof(in), sizeof(out), &in, &out);

    if (e==NvSuccess)
        *pPhysical = out.PhysicalSectorNum;

    return NvSuccess;
}
    
NvError NvDdkBlockDevReadSector(
    NvDdkBlockDevHandle hBlockDev,
    NvU32               SectorNum,
    void               *pBuffer,
    NvU32               NumSectors,
    NvBool              LogicalAddress)
{
    IP(ReadPhysicalSectorInput) in;

    if (!hBlockDev)
        return NvError_BadParameter;

    if (LogicalAddress)
        return hBlockDev->NvDdkBlockDevReadSector(hBlockDev, 
                    SectorNum, pBuffer, NumSectors);

    in.SectorNum = SectorNum;
    in.NumberOfSectors = NumSectors;
    in.pBuffer = (void *)pBuffer;
    return NvDdkBlockDevIoctl(hBlockDev, I(ReadPhysicalSector),
               sizeof(in), 0, &in, NULL);
}

NvError NvDdkBlockDevWriteSector(
    NvDdkBlockDevHandle hBlockDev,
    NvU32               SectorNum,
    const void*         pBuffer,
    NvU32               NumSectors,
    NvBool              LogicalAddress)

{
    IP(WritePhysicalSectorInput) in;

    if (!hBlockDev)
        return NvError_BadParameter;

    if (LogicalAddress)
        return hBlockDev->NvDdkBlockDevWriteSector(hBlockDev,
                    SectorNum, pBuffer, NumSectors);

    in.SectorNum = SectorNum;
    in.NumberOfSectors = NumSectors;
    in.pBuffer = pBuffer;
    return NvDdkBlockDevIoctl(hBlockDev, I(WritePhysicalSector),
               sizeof(in), 0, &in, NULL);
}

