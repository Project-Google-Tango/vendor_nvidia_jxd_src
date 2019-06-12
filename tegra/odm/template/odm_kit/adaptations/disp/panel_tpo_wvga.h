/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_PANEL_TPO_H
#define INCLUDED_PANEL_TPO_H

#include "display_hal.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define TPO_WVGA_GUID NV_ODM_GUID('T','P','O','_','W','V','G','A')

/* Fills in the function pointer table for the TPO WVGA LCD.
 *  The backlight functions are not modified, to allow a variety of
 *  backlight selections to work with the same code */

void tpowvga_GetHal( NvOdmDispDeviceHandle hDevice );


NvBool tpowvga_Setup( NvOdmDispDeviceHandle hDevice );

void tpowvga_Release( NvOdmDispDeviceHandle hDevice );

void tpowvga_ListModes( NvOdmDispDeviceHandle hDevice,
                            NvU32 *nModes,
                            NvOdmDispDeviceMode *modes );

NvBool tpowvga_SetMode( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDeviceMode *mode, NvU32 flags );

void tpowvga_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity );

void tpowvga_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
                                NvOdmDispDevicePowerLevel level );

NvBool tpowvga_GetParameter( NvOdmDispDeviceHandle hDevice,
                                 NvOdmDispParameter param,
                                 NvU32 *val );

void * tpowvga_GetConfiguration( NvOdmDispDeviceHandle hDevice );

void tpowvga_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
                                   NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values );

NvBool tpowvga_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
                                       NvOdmDispSpecialFunction function, void *config );

#if defined(__cplusplus)
}
#endif

#endif
