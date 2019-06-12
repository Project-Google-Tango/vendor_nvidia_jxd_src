/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_PANEL_AUO_H
#define INCLUDED_PANEL_AUO_H

#include "panels.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define AUO_EDP_PANEL_GUID NV_ODM_GUID('N','V','D','A','_','a','u','o')

void auo_edp_GetHal( NvOdmDispDeviceHandle hDevice );

NvBool auo_edp_Setup( NvOdmDispDeviceHandle hDevice );

void auo_edp_Release( NvOdmDispDeviceHandle hDevice );

void auo_edp_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
    NvOdmDispDeviceMode *modes );

NvBool auo_edp_SetMode( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDeviceMode *mode, NvU32 flags );

void auo_edp_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity );

void auo_edp_SetBacklightIntensity( NvOdmDispDeviceHandle hDevice, NvU8 intensity );

void auo_edp_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level );

NvBool auo_edp_GetParameter( NvOdmDispDeviceHandle hDevice,
    NvOdmDispParameter param, NvU32 *val );

void * auo_edp_GetConfiguration( NvOdmDispDeviceHandle hDevice );

void auo_edp_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values );

NvBool auo_edp_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config );

#if defined(__cplusplus)
}
#endif

#endif
