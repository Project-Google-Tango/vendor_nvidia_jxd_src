/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_NVDDK_XUSB_BLOCK_DRIVER_H
#define INCLUDED_NVDDK_XUSB_BLOCK_DRIVER_H

#include "nvddk_blockdev.h"

NvError NvDdkXusbhBlockDevInit(NvRmDeviceHandle hDevice);

void NvDdkXusbhBlockDevDeinit(void);

NvError
NvDdkXusbhBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev);

#endif
