/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file 
 * @brief <b>%Input/Output 
 * 
 * @b Description: This header declares different arguments that are passed 
 * as ioctls btw ddk layer, shims and kernel modules.
 *
 */

#ifndef INCLUD_NV_ARGUMENTS_H
#define INCLUD_NV_ARGUMENTS_H

#include "nvcommon.h"

// Nand arguments required by shim
// populated thru the kernel ioctls.
// Read from the bsp args.

typedef struct _NandArgs {
    //Location where the image starts
    NvU32 ImageStart;
    //Location where the image ends
    NvU32 ImageEnd;
    //Location for the bad block table
    NvU32 BadBlockTable;
    //Interleave nand banks
    NvU32 InterleaveBankCount;
}NandArgs;

// Parition table info required by the OS drivers.
// Queried thru Kernel IOCTL call and read from the bsp args
typedef struct NvPartitionTableInfoRec
{
    // Logical sector address of the partition table on the storage medium
    NvU32 StartLogicalSector;
    // Number of logical sectors used for partition table
    NvU32 NumOfSectors;
} NvPartitionTableInfo;

// OS file system partition info required by the OS drivers
// Queried thru Kernel IOCTL call and read from the bsp args
typedef struct NvOsFsPartitionInfoRec
{
    // Device Id for the storage device on which OS filesystem partition exists
    NvU32 DeviceId;
    // Device instance of the storage device
    NvU32 DeviceInstance;
    // Partition Id for the OS filesystem partition
    NvU32 PartitionId;
    // Logical sector address of the start of OS filesystem partition
    NvU32 StartLogicalSector;
} NvOsFsPartitionInfo;

// Defining Mobile sku type 
#define SKU_MOBILE   1 

#endif
