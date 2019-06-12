/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_PANEL_SAMSUNGSDI_H
#define INCLUDED_PANEL_SAMSUNGSDI_H

#include "display_hal.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define SAMSUNG_DSI_GUID NV_ODM_GUID('S','A','M','S','_','D','S','I')

/* Fills in the function pointer table for the SAMSUNG DSI LCD. */

void samsungdsi_GetHal( NvOdmDispDeviceHandle hDevice );

NvBool samsungsdi_Setup( NvOdmDispDeviceHandle hDevice );

void samsungsdi_Release( NvOdmDispDeviceHandle hDevice );

void samsungsdi_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
    NvOdmDispDeviceMode *modes );

NvBool samsungsdi_SetMode( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDeviceMode *mode, NvU32 flags );

void samsungsdi_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity );

void samsungsdi_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level );

NvBool samsungsdi_GetParameter( NvOdmDispDeviceHandle hDevice,
    NvOdmDispParameter param, NvU32 *val );

void * samsungsdi_GetConfiguration( NvOdmDispDeviceHandle hDevice );

void samsungsdi_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values );

NvBool samsungsdi_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config );

#if defined(__cplusplus)
}
#endif

#endif
