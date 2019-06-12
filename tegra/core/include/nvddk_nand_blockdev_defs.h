/*
 * Copyright (c) 2008-2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b> NVIDIA Driver Development Kit: NAND Block Driver Definitions</b>
 *
 * @b Description: This file defines the public data structures for the
 *    NAND Block driver.
 */

#ifndef INCLUDED_NVDDK_NAND_BLOCKDEV_DEFS_H
#define INCLUDED_NVDDK_NAND_BLOCKDEV_DEFS_H

#include "nvcommon.h"
#include "nvddk_blockdev_defs.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup nvddk_nand_defs NAND Block Driver Definitions
 *
 *  This file defines the public data structures for the
 *    NAND Block driver.
 *
 * @ingroup nvddk_modules
 * @{
 */


/**
 * Defines NAND-specific IOCTL types.
 *
 * @note Device-specific IOCTLs start after standard IOCTLs.
 */
typedef enum
{

    /**
     * Define partition subregion.
     *
     * @par Inputs: 
     * ::DefineSubRegionInputArgsRec
     *
     * @par Outputs:
     * None.
     */
    NvDdkBlockDevIoctlType_DefineSubRegion = NvDdkBlockDevIoctlType_Num,
    /**
     * Query first time boot.
     *
     * @par Inputs: 
     * None.
     *
     * @par Outputs:
     * ::NvDdkBlockDevIoctl_QueryFirstBootOutputArgs
     */
    NvDdkBlockDevIoctlType_QueryFirstBoot,
    /**
     * Force block remap on next boot.
     *
     * @par Inputs: 
     * None.
     *
     * @par Outputs:
     * None.
     */
    NvDdkBlockDevIoctlType_ForceBlkRemap,

    /**
     * Gets bad block info for particular PBA.
     *
     * @par Inputs:
     * ::NvDdkBlockDevIoctl_IsGoodBlockInputArgsRec
     *
     * @par Outputs:
     * ::NvDdkBlockDevIoctl_IsGoodBlockOutputArgsRec
     * 
     * @retval NvError_Success Operation successful.
     * @retval NvError_ReadError Read operation failed.
     * @retval NvError_InvalidBlock PBA specified is not found or invalid.
     */
    NvDdkBlockDevIoctlType_IsGoodBlock,

    NvDdkNandBlockDevIoctlType_Num,
    /**
     * Ignore -- Forces compilers to make 32-bit enums.
     */
    NvDdkNandBlockDevIoctlType_Force32 = 0x7FFFFFFF
} NvDdkNandBlockDevIoctlType;

/**
 * Define subregion IOCTL input arguments.
 *
 * Define NAND bad block management and wear leveling subregions for existing
 * NAND partition.
 * 
 * Once defined, subregion bad block management and wear leveling will be restricted
 * to within the subregion using free subregion blocks.
 *  
 * @retval NvError_Success Partition subregions successfully defined and enabled.
 * @retval NvError_BadValue Invalid input argument(s).
 */
typedef struct NvDdkBlockDevIoctl_DefineSubRegionInputArgsRec
{
    /// Subregion start logical block address (inclusive).
    NvU32 StartLogicalBlock;
    /// Subregion stop logical block address (inclusive).
    NvU32 StopLogicalBlock;
    /// Start of subregion free logical block address (inclusive).
    NvU32 StartFreeLogicalBlock;
    /// Number of subregion free blocks available for bad block management
    /// and wear leveling.
    NvU32 FreeBlockCount;
} NvDdkBlockDevIoctl_DefineSubRegionInputArgs;


/**
 * Query first time boot IOCTL output arguments.
 *
 * NAND block driver query for first time boot.
 * 
 * @retval NvError_Success First boot query successful.
 * @retval NvError_BadValue Invalid input argument(s).
 */
typedef struct NvDdkBlockDevIoctl_QueryFirstBootOutputArgsRec
{
    /// NV_TRUE if first boot, NV_FALSE if not.
    NvBool IsFirstBoot;
} NvDdkBlockDevIoctl_QueryFirstBootOutputArgs;

/**
 * Gets NAND block good/bad information IOCTL input arguments.
 * 
 * Returns the range of physical sectors encompassing the currently
 * opened partition.
 *
 * @retval NvError_Success Query partition physical sectors IOCTL successful.
 * @retval NvError_BadValue Invalid input argument(s).
 */
typedef struct NvDdkBlockDevIoctl_IsGoodBlockInputArgsRec
{
    NvU32 ChipNumber;
    NvU32 PhysicalBlockAddress;
}NvDdkBlockDevIoctl_IsGoodBlockInputArgs;

typedef struct NvDdkBlockDevIoctl_IsGoodBlockOutputArgsRec
{
    NvBool IsFactoryGoodBlock;
    NvBool IsRuntimeGoodBlock;
    // IsFactoryGoodBlock && IsRuntimeGoodBlock
    NvBool IsGoodBlock;
}NvDdkBlockDevIoctl_IsGoodBlockOutputArgs;


 /**
 * NAND-specific definitions for the ::NvDdkBlockDevIoctl_AllocatePartitionInputArgs
 * IOCTL, specifically the \c PartitionAttribute value (see nvddk_blockdev_defs.h).
 */

/// Interleave factor.
#define NVDDK_NAND_BLOCKDEV_INTERLEAVE_FACTOR_FIELD  0x3

#define NVDDK_NAND_BLOCKDEV_INTERLEAVE_FACTOR_1      0x1
#define NVDDK_NAND_BLOCKDEV_INTERLEAVE_FACTOR_2      0x2
#define NVDDK_NAND_BLOCKDEV_INTERLEAVE_FACTOR_4      0x3

/// Interleave enable.
#define NVDDK_NAND_BLOCKDEV_INTERLEAVE_ENABLE_FIELD  0x4

#define NVDDK_NAND_BLOCKDEV_INTERLEAVE_ENABLED       0x4
#define NVDDK_NAND_BLOCKDEV_INTERLEAVE_DISABLED      0x0

/// Management policy.
#define NVDDK_NAND_BLOCKDEV_MGMT_POLICY_FIELD        0x18

#define NVDDK_NAND_BLOCKDEV_MGMT_POLICY_FTL_LITE     0x8
#define NVDDK_NAND_BLOCKDEV_MGMT_POLICY_FTL          0x0
#define NVDDK_NAND_BLOCKDEV_MGMT_POLICY_EXT_FTL     0x10


#if defined(__cplusplus)
}
#endif  /* __cplusplus */
/** @} */

#endif // INCLUDED_NVDDK_NAND_BLOCKDEV_DEFS_H

