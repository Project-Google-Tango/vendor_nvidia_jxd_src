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
 * <b> NVIDIA Driver Development Kit: AES Block Device Interface</b>
 *
 * @b Description: This file defines the interface for AES.
 */

#ifndef INCLUDED_NVDDK_AES_BLOCKDEV_H
#define INCLUDED_NVDDK_AES_BLOCKDEV_H

#include "nvcommon.h"
#include "nverror.h"
#include "nvddk_aes_blockdev_defs.h"
#include "nvddk_blockdev.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup nvbdk_ddk_aes AES Block Device Interface
 *
 * This is the interface for AES block device.
 * @ingroup nvddk_modules
 * @{
 */

/**
 * Allocates and initializes the required global resources.
 *
 * @warning This method must be called once before using any NvDdkAesBlockDev
 *          APIs.
 *
 * @param hDevice RM device handle.
 *
 * @retval NvSuccess Global resources have been allocated and initialized.
 * @retval NvError_DeviceNotFound Device is not present or not supported.
 * @retval NvError_InsufficientMemory Cannot allocate memory.
 */
NvError NvDdkAesBlockDevInit(NvRmDeviceHandle hDevice);

/**
 * Allocates resources, powers on the device, and prepares the device for
 * I/O operations. A valid \c NvDdkBlockDevHandle is returned only if the
 * device is found. The same handle must be used for further operations,
 * except NvDdkAesBlockDevInit(). Multiple invocations from multiple
 * concurrent clients must be supported.
 *
 * @warning This method must be called once before using other NvDdkBlockDev
 *          APIs.
 *
 * @param Instance Instance of specific device.
 * @param MinorInstance Minor instance of the specific device.
 * @param phBlockDevInstance Returns a pointer to device handle.
 *
 * @retval NvSuccess Device is present and ready for I/O operations.
 * @retval NvError_DeviceNotFound Device is neither present nor responding 
 *                                   nor supported.
 * @retval NvError_InsufficientMemory Cannot allocate memory.
 */
NvError 
NvDdkAesBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDevInstance);

// Percent sign (%) in the below suppresses the link in doxygen output
// and is not part of the prototype.
/**
 * Releases all global resources allocated.
 *
 * The required function prototype is as follows:
 * <pre>    void %NvDdkAesBlockDevDeinit(void);    </pre>
 */
void NvDdkAesBlockDevDeinit(void);

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

/** @} */
#endif // INCLUDED_NVDDK_AES_BLOCKDEV_H

