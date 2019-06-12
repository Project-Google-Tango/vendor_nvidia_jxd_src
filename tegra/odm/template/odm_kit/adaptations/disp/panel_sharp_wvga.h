/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_PANEL_SHARPWVGA_H
#define INCLUDED_PANEL_SHARPWVGA_H

#include "display_hal.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define SHARP_WVGA_GUID NV_ODM_GUID('S','H','A','R','P','_','W','V')
#define SHARP_WVGA_AP20_GUID NV_ODM_GUID('S','H','P','_','A','P','2','0')

/* Fills in the function pointer table for the SHARP WVGA LCD.
 *  The backlight functions are not modified, to allow a variety of
 *  backlight selections to work with the same code */

void sharpwvga_GetHal( NvOdmDispDeviceHandle hDevice );

NvBool sharpwvga_Setup( NvOdmDispDeviceHandle hDevice );

void sharpwvga_Release( NvOdmDispDeviceHandle hDevice );

void sharpwvga_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
    NvOdmDispDeviceMode *modes );

NvBool sharpwvga_SetMode( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDeviceMode *mode, NvU32 flags );

void sharpwvga_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity );

void sharpwvga_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level );

NvBool sharpwvga_GetParameter( NvOdmDispDeviceHandle hDevice,
    NvOdmDispParameter param, NvU32 *val );

void * sharpwvga_GetConfiguration( NvOdmDispDeviceHandle hDevice );

void sharpwvga_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values );

NvBool sharpwvga_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config );

#if defined(__cplusplus)
}
#endif

#endif
