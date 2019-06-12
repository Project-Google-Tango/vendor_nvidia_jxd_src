/*
 * Copyright (c) 2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief <b>Usb Host Block Driver</b>
 *
 * @b Description: Contains Usb Host block driver implementation.
 */

#ifndef INCLUDED_NVDDK_USBH_BLOCK_DRIVER_H
#define INCLUDED_NVDDK_USBH_BLOCK_DRIVER_H


#include "nvddk_blockdev.h"

NvError NvDdkUsbhBlockDevInit(NvRmDeviceHandle hDevice);

void NvDdkUsbhBlockDevDeinit(void);

NvError
NvDdkUsbhBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev);

#endif
