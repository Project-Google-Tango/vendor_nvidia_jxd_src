/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_PANEL_SHARPWVGAP_H
#define INCLUDED_PANEL_SHARPWVGAP_H

#include "display_hal.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define SHARP_WVGAP_GUID NV_ODM_GUID('S','H','A','R','P','_','W','P')
#define SHARP_WVGAP_AP20_GUID NV_ODM_GUID('S','H','P','P','A','P','2','0')

/* Fills in the function pointer table for the SHARP WVGA LCD.
 *  The backlight functions are not modified, to allow a variety of
 *  backlight selections to work with the same code */

void sharpwvgap_GetHal( NvOdmDispDeviceHandle hDevice );

NvBool sharpwvgap_Setup( NvOdmDispDeviceHandle hDevice );

void sharpwvgap_Release( NvOdmDispDeviceHandle hDevice );

void sharpwvgap_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
    NvOdmDispDeviceMode *modes );

NvBool sharpwvgap_SetMode( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDeviceMode *mode, NvU32 flags );

void sharpwvgap_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity );

void sharpwvgap_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level );

NvBool sharpwvgap_GetParameter( NvOdmDispDeviceHandle hDevice,
    NvOdmDispParameter param, NvU32 *val );

void * sharpwvgap_GetConfiguration( NvOdmDispDeviceHandle hDevice );

void sharpwvgap_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values );

NvBool sharpwvgap_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config );

#if defined(__cplusplus)
}
#endif

#endif
