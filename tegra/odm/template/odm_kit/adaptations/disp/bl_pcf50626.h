/**
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef BACKLIGHT_ADAPTATION_PCF50626_H
#define BACKLIGHT_ADAPTATION_PCF50626_H

#include "display_hal.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/* Fills in the function pointer table for the PCF50626 backlight controls.
 *  The backlight functions are not modified, to allow a variety of
 *  backlight selections to work with the same code */

void BL_PCF50626_GetHal(NvOdmDispDeviceHandle hDevice);

#if defined(__cplusplus)
}
#endif

#endif
