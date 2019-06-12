/*
 * Copyright (c) 2013 - 2014, NVIDIA CORPORATION.  All rights reserved.
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

#define AUO_E1807TPS65914_PANEL_GUID NV_ODM_GUID('N','V','1','8','0','7','a','u')
#define AUO_E1937TPS65914_PANEL_GUID NV_ODM_GUID('N','V','1','9','3','7','a','u')

void auoDsi_GetHal( NvOdmDispDeviceHandle hDevice );

NvBool auoDsi_Setup( NvOdmDispDeviceHandle hDevice );

void auoDsi_Release( NvOdmDispDeviceHandle hDevice );

void auoDsi_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
    NvOdmDispDeviceMode *modes );

NvBool auoDsi_SetMode( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDeviceMode *mode, NvU32 flags );

void auoDsi_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity );

void auoDsi_SetBacklightIntensity( NvOdmDispDeviceHandle hDevice, NvU8 intensity );

void auoDsi_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level );

NvBool auoDsi_GetParameter( NvOdmDispDeviceHandle hDevice,
    NvOdmDispParameter param, NvU32 *val );

void * auoDsi_GetConfiguration( NvOdmDispDeviceHandle hDevice );

void auoDsi_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values );

NvBool auoDsi_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config );

#if defined(__cplusplus)
}
#endif

#endif
