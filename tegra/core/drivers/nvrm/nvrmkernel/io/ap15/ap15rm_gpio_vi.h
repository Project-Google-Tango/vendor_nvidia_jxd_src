/*
 * Copyright 2007 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef AP15RM_GPIO_VI_H
#define AP15RM_GPIO_VI_H

#include "nvcommon.h"
#include "nvrm_init.h"

NvError
NvRmPrivGpioViAcquirePinHandle(
        NvRmDeviceHandle hRm,
        NvU32 pinNumber);

void NvRmPrivGpioViReleasePinHandles(
        NvRmDeviceHandle hRm,
        NvU32 pin);

NvU32 NvRmPrivGpioViReadPins(
        NvRmDeviceHandle hRm,
        NvU32 pin );

void NvRmPrivGpioViWritePins(
        NvRmDeviceHandle hRm,
        NvU32 pin,
        NvU32 pinState );
NvError 
NvRmPrivGpioViPowerRailConfig(
        NvRmDeviceHandle hRm,
        NvBool Enable);

NvBool NvRmPrivGpioViDiscover(
        NvRmDeviceHandle hRm);

#endif /* AP15RM_GPIO_VI_H */
