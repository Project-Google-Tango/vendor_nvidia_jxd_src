/*
 * Copyright (c) 2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_PANEL_SHARP_2516_H
#define INCLUDED_PANEL_SHARP_2516_H

#include "panels.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define SHARP_2516_PANEL_GUID NV_ODM_GUID('N','V','2','5','1','6','s','p')
#define SHARP_TPS65914_2516_PANEL_GUID NV_ODM_GUID('N','V','6','5','9','1','s','p')

void sharpDsi_GetHal( NvOdmDispDeviceHandle hDevice );

NvBool sharpDsi_Setup( NvOdmDispDeviceHandle hDevice );

void sharpDsi_Release( NvOdmDispDeviceHandle hDevice );

void sharpDsi_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
    NvOdmDispDeviceMode *modes );

NvBool sharpDsi_SetMode( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDeviceMode *mode, NvU32 flags );

void sharpDsi_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity );

void sharpDsi_SetBacklightIntensity( NvOdmDispDeviceHandle hDevice, NvU8 intensity );

void sharpDsi_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level );

NvBool sharpDsi_GetParameter( NvOdmDispDeviceHandle hDevice,
    NvOdmDispParameter param, NvU32 *val );

void * sharpDsi_GetConfiguration( NvOdmDispDeviceHandle hDevice );

void sharpDsi_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values );

NvBool sharpDsi_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config );

#if defined(__cplusplus)
}
#endif

#endif
