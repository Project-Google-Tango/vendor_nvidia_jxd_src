/*
 * Copyright (c) 2008-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_BACKLIGHT_H
#define INCLUDED_BACKLIGHT_H

#include "nvodm_services.h"
#include "nvodm_disp.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Wrapper for backlight ODM implementations.
 */
NvBool
NvOdmBacklightInit( NvOdmDispDeviceHandle hDevice, NvBool bReopen );

void
NvOdmBacklightDeinit( NvOdmDispDeviceHandle hDevice );

void
NvOdmBacklightIntensity( NvOdmDispDeviceHandle hDevice, NvU8 intensity );

#if defined(__cplusplus)
}
#endif

#endif
