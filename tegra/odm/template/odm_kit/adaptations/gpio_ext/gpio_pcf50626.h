/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_GPIO_PCF50626_H
#define INCLUDED_GPIO_PCF50626_H

#include "gpio_ext_hal.h"

#if defined(__cplusplus)
extern "C"
{
#endif

void
GPIO_PCF50626_NvOdmExternalGpioWritePins(
    NvU32 Port,
    NvU32 Pin,
    NvU32 PinValue);

NvU32
GPIO_PCF50626_NvOdmExternalGpioReadPins(
    NvU32 Port,
    NvU32 Pin);

#if defined(__cplusplus)
}
#endif

#endif
