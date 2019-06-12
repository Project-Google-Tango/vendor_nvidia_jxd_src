/*
 * Copyright (c) 2008-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b> NVIDIA Driver Development Kit: Block Device Manager Definitions</b>
 *
 * @b Description: This file declares defines for the
 *                 block device manager.
 */

#ifndef INCLUDED_NVDDK_BLOCKDEVMGR_DEFS_H
#define INCLUDED_NVDDK_BLOCKDEVMGR_DEFS_H

#include "nvcommon.h"
#include "nvrm_module.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup nvbdk_ddk_blkdevmgrdefs Block Device Manager Definitions
 *
 * These are the the block device manager definitions.
 *
 * @ingroup nvddk_modules
 * @{
 */

/**
 * Defines block device driver types.
 *
 * There are two categories of block device drivers:
 * - Physical block device drivers--drivers that control RM modules directly.
 * - Logical block device drivers--drivers that may not manage RM modules
 *     directly. For example, they may simply control other drivers.
 *
 * For physical drivers, the corresponding \c NvDdkBlockDevMgrDeviceId and
 *     \c NvRmModuleId enums have the same value. The
 *     c\ NvDdkBlockDevMgrDeviceId enum values for logical devices lie
 *     outside the range of valid \c NvRmModuleId enum values, such that the
 *     two can never collide.
 *
 * In this way, clients needing to access only physical drivers can pass in
 *     \c NvRmModuleId values instead of \c NvDdkBlockDevMgrDeviceId values.
 *     This eases the task of migrating legacy code to use the NvDDK Block
 *     Device Manager.
 *
 * @note For physical block device drivers enums match corresponding
 *     \c NvRmModuleId, and for logical block device drivers enums are
 *     guaranteed not to conflict with \c NvRmModuleId values.
 */
typedef enum
{

    // Physical block device drivers enums match corresponding NvRmModuleId.

    /// Specifies an invalid module ID. (Physical block device driver.)
    NvDdkBlockDevMgrDeviceId_Invalid = NvRmModuleID_Invalid,

    /// Specifies the IDE controller. (Physical block device driver.)
    NvDdkBlockDevMgrDeviceId_Ide = NvRmModuleID_Ide,

    /// Specifies the SDMMC controller. (Physical block device driver.)
    NvDdkBlockDevMgrDeviceId_SDMMC = NvRmModuleID_Sdio,

    /// Specifies the NAND controller. (Physical block device driver.)
    NvDdkBlockDevMgrDeviceId_Nand = NvRmModuleID_Nand,

    /// Specifies the NOR Controller. (Physical block device driver.)
    NvDdkBlockDevMgrDeviceId_Nor = NvRmModuleID_SyncNor,

    /// Specifies the AES controller. (Physical block device driver.)
    NvDdkBlockDevMgrDeviceId_Aes,

    /// Specifies the SPI flash physical block device driver.
    NvDdkBlockDevMgrDeviceId_Spi = NvRmModuleID_Spi,

    /// Specifies the SE block device driver.
    NvDdkBlockDevMgrDeviceId_Se = NvRmModuleID_Se,

    /// Specifies the USB3 Host block device driver.
    NvDdkBlockDevMgrDeviceId_Usb3 = NvRmModuleID_XUSB_HOST,

    /// Specifies the USB Host block device driver.
    NvDdkBlockDevMgrDeviceId_Usb2Otg = NvRmModuleID_Usb2Otg,

    /// Specifies the SATA controller. (Physical block device driver.)
    NvDdkBlockDevMgrDeviceId_Sata = NvRmModuleID_Sata,

    // Logical device driver enums are guaranteed not to conflict with
    // NvRmModuleId's.

    /// Specifies a logical block device driver.
    NvDdkBlockDevMgrDeviceId_LogicalPartition = NvRmModuleID_Num,

    /// Must appear after the last legal item.
    NvDdkBlockDevMgrDeviceId_Num,

    /// Ignore -- Forces compilers to make 32-bit enums.
    NvDdkBlockDevMgrDeviceId_Force32 = 0x7fffffff
} NvDdkBlockDevMgrDeviceId;

#if defined(__cplusplus)
}
#endif

/** @} */
#endif /* #ifndef INCLUDED_NVDDK_BLOCKDEVMGR_DEFS_H */
