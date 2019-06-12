/**
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef BACKLIGHT_ADAPTATION_AP20_H
#define BACKLIGHT_ADAPTATION_AP20_H

#include "display_hal.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/*  Fills in the function pointer table for backlight controls
 *  which use AP20 (rather than an external PMU).
 */

void BL_AP20_GetHal(NvOdmDispDeviceHandle hDevice);

#if defined(__cplusplus)
}
#endif

#endif
