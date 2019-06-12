/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_Usb_BDK_TESTS_H
#define INCLUDED_Usb_BDK_TESTS_H

#include "nverror.h"
#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif


#define NVBDKTEST_USB_SECTOR_SIZE       512
#define NVBDKTEST_USB_UNIT_SIZE       1024
#define NV_MICROSEC_IN_SEC                 1000000
#define NV_BYTES_IN_KB                     1024
#define NV_BYTES_IN_MB                     (NV_BYTES_IN_KB * NV_BYTES_IN_KB)
#define NV_WRITE_READ_VERIFY_NEEDED_FOR_SPEED_TEST      1


/**
 * Initializes the Usb Block driver, and opens the sd controller for the given
 * instance.
 *
 * @param Instance The Instance of the USb Controller.
 *
 * @retval NvSuccess. Block driver sucessfully initialized.
 * @retval NvError_InsufficientMemory. Failed to allocate memory.
 * @retval NvError_DeviceNotFound Indicates device is either not present, not
 *            responding, or not supported.
 */
NvError UsbTestInit(NvU32 Instance);

/**
 * Verifies the Usb Memory partially, by checking Write and Read data.
 *
 * @retval NvSuccess. Block driver sucessfully initialized.
 * @retval NvError_InsufficientMemory. Failed to allocate memory.
 * @retval NvError_NotImplemented If it is a stub function.
 */

/**
 * Closes the Usb Controller Instance and the block driver.
 *
 */
void UsbTestClose(void);


#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_USB_BDK_TESTS_H
