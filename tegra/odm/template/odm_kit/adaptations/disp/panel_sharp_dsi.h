/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_PANEL_DSI_H
#define INCLUDED_PANEL_DSI_H

#include "display_hal.h"

#if defined(__cplusplus)
extern "C"
{
#endif
#define SHARP_DSI_GUID NV_ODM_GUID('s','h','a','r','p','d','s','i')

NvBool sharpdsi_Setup( NvOdmDispDeviceHandle hDevice );

void sharpdsi_Release( NvOdmDispDeviceHandle hDevice );

void sharpdsi_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
    NvOdmDispDeviceMode *modes );

NvBool sharpdsi_SetMode( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDeviceMode *mode, NvU32 flags );

void sharpdsi_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity );

void sharpdsi_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level );

NvBool sharpdsi_GetParameter( NvOdmDispDeviceHandle hDevice,
    NvOdmDispParameter param, NvU32 *val );

void * sharpdsi_GetConfiguration( NvOdmDispDeviceHandle hDevice );

void sharpdsi_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values );

NvBool sharpdsi_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config );

void SharpDSI_GetHal(NvOdmDispDeviceHandle hDevice);

#if defined(__cplusplus)
}
#endif

#endif
