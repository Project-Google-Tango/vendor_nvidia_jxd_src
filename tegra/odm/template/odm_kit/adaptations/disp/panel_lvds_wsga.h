/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_PANEL_LVDS_WSGA_H
#define INCLUDED_PANEL_LVDS_WSGA_H

#include "display_hal.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define LVDS_WSVGA_GUID NV_ODM_GUID('L','V','D','S','W','S','V','G')

/* Fills in the function pointer table for the LVDS WSVGA LCD.
 *  The backlight functions are not modified, to allow a variety of
 *  backlight selections to work with the same code */

void lvds_wsga_GetHal( NvOdmDispDeviceHandle hDevice );

NvBool lvds_wsga_Setup( NvOdmDispDeviceHandle hDevice );

void lvds_wsga_Release( NvOdmDispDeviceHandle hDevice );

void lvds_wsga_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
    NvOdmDispDeviceMode *modes );

NvBool lvds_wsga_SetMode( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDeviceMode *mode, NvU32 flags );

void lvds_wsga_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity );

void lvds_wsga_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level );

NvBool lvds_wsga_GetParameter( NvOdmDispDeviceHandle hDevice,
    NvOdmDispParameter param, NvU32 *val );

void * lvds_wsga_GetConfiguration( NvOdmDispDeviceHandle hDevice );

void lvds_wsga_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values );

NvBool lvds_wsga_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config );

NvBool lvds_wsga_BacklightSetup( NvOdmDispDeviceHandle hDevice, NvBool bReopen );

void lvds_wsga_BacklightRelease( NvOdmDispDeviceHandle hDevice );

void lvds_wsga_SetBacklightIntensity( NvOdmDispDeviceHandle hDevice, NvU8 intensity );

#if defined(__cplusplus)
}
#endif

#endif
