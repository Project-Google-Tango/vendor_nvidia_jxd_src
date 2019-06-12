/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_PANEL_PANASONIC_H
#define INCLUDED_PANEL_PANASONIC_H

#include "panels.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define PANASONIC_PANEL_GUID NV_ODM_GUID('N','V','D','A','p','a','n','a')

void panasonic_GetHal( NvOdmDispDeviceHandle hDevice );

NvBool panasonic_Setup( NvOdmDispDeviceHandle hDevice );

void panasonic_Release( NvOdmDispDeviceHandle hDevice );

void panasonic_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
    NvOdmDispDeviceMode *modes );

NvBool panasonic_SetMode( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDeviceMode *mode, NvU32 flags );

void panasonic_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity );

void panasonic_SetBacklightIntensity( NvOdmDispDeviceHandle hDevice, NvU8 intensity );

void panasonic_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level );

NvBool panasonic_GetParameter( NvOdmDispDeviceHandle hDevice,
    NvOdmDispParameter param, NvU32 *val );

void * panasonic_GetConfiguration( NvOdmDispDeviceHandle hDevice );

void panasonic_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values );

NvBool panasonic_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config );

#if defined(__cplusplus)
}
#endif

#endif
