/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b> NVIDIA Driver Development Kit: SPI flash Block Driver Interface</b>
 *
 * @b Description: This file declares the interfaces for spi flash block driver.
 */

#ifndef INCLUDED_NVDDK_SPIF_H
#define INCLUDED_NVDDK_SPIF_H

#include "nvrm_init.h"
#include "nvddk_blockdev.h"

/**
 * Initializes the SPI flash block driver by
 * allocating the required resources.
 * This function must be called before calling the
 * other spi flash block driver interfaces.
 *
 * @param hDevice The RM device handle.
 *
 * @retval NvSuccess. Block driver sucessfully initialized.
 * @retval NvError_InsufficientMemory. Failed to allocate memory.
 */
NvError
NvDdkSpifBlockDevInit(NvRmDeviceHandle hDevice);

/**
 * Uninitializes the SPI flash block driver by
 * freeing the resources used by the block driver.
 */
void
NvDdkSpifBlockDevDeinit(void);

/**
 * Opens the SPI flash block driver.
 * This function opens the low level Rm SPI driver,
 * allocates the SPI flash block driver handle and
 * populates it by assigning the corresponding SPI flash
 * driver functions.
 *
 * Clients get a valid block device handle only if the device
 * is successfully opened. Only one client can open the driver
 * at a time, simultaneous opening of the driver by multiple
 * clients is not allowed.
 *
 * @param Instance Instance of specific device.
 * @param MinorInstance Minor instance of specific device.
 * @param phBlockDev Returns pointer to device handle.
 *
 * @retval NvSuccess. SPI flash block driver opened successfully.
 * @retval NvError_InsufficientMemory Cannot allocate memory.
 */
NvError
NvDdkSpifBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev);

#endif

