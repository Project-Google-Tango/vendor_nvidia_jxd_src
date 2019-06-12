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
 * <b> NVIDIA Driver Development Kit: Block Device Manager</b>
 *
 * @b Description: This file declares the interface for accessing block drivers.
 */


#ifndef INCLUDED_NVDDK_BLOCKDEVMGR_H
#define INCLUDED_NVDDK_BLOCKDEVMGR_H

#include "nvcommon.h"
#include "nverror.h"

#include "nvddk_blockdev.h"
#include "nvddk_blockdevmgr_defs.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup nvbdk_ddk_blkdevmgr Block Device Manager
 *
 * This is the interface for accessing block drivers.
 *
 * To initialize the device manager, the client calls
 *     NvDdkBlockDevMgrInit() exactly once before any other operations can
 *     be performed.
 *
 * Once the device manager is initialized, the client can obtain a
 *     handle to a block driver by calling NvDdkBlockDevMgrDeviceOpen()
 *     and specifying the desired device type.
 *
 * At this point, the client can perform operations on the device via
 *     the \c NvDdkBlockDev interface. When finished performing operations
 *     on a given device, close it by calling \c NvDdkBlockDevClose through
 *     the \c NvDdkBlockDev interface.
 *
 * After all device drivers are closed, call NvDdkBlockDevMgrDeinit()
 *     to shut down the device manager.
 *
 * @ingroup nvddk_modules
 * @{
 */

/**
 * Initializes the device manager.
 */
NvError
NvDdkBlockDevMgrInit(void);

/**
 * Shuts down the device manager.
 */
void
NvDdkBlockDevMgrDeinit(void);

/**
 * Opens a device driver.
 *
 * \a phBlockDev is the address of a handle to the device driver.
 *     Subsequent device operations can be invoked through the handle.
 *
 * \a DriverAttr is an attribute value provided to the driver. Interpretation
 *     of this value is left up to the driver.
 *
 * @param DeviceId Storage device type.
 * @param Instance Storage device instance number.
 * @param MinorInstance Storage device minor instance number.
 * @param phBlockDev A pointer to the handle to the block driver for storage
 *     device.
 *
 * @retval NvError_Success No Error.
 * @retval NvError_InvalidParameter Illegal parameter value.
 */
NvError
NvDdkBlockDevMgrDeviceOpen(
    NvDdkBlockDevMgrDeviceId DeviceId,
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev);

#if defined(__cplusplus)
}
#endif

/** @} */
#endif /* #ifndef INCLUDED_NVDDK_BLOCKDEVMGR_H */
