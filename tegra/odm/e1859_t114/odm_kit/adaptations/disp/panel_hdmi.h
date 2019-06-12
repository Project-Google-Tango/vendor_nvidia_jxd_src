/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_PANEL_HDMI_H
#define INCLUDED_PANEL_HDMI_H

#include "panels.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define HDMI_GUID NV_ODM_GUID('f','f','a','2','h','d','m','i')

void hdmi_GetHal( NvOdmDispDeviceHandle hDevice );

NvBool hdmi_Setup( NvOdmDispDeviceHandle hDevice );

void hdmi_Release( NvOdmDispDeviceHandle hDevice );

void hdmi_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
    NvOdmDispDeviceMode *modes );

NvBool hdmi_SetMode( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDeviceMode *mode, NvU32 flags );

void hdmi_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity );

void hdmi_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level );

NvBool hdmi_GetParameter( NvOdmDispDeviceHandle hDevice,
    NvOdmDispParameter param, NvU32 *val );

void * hdmi_GetConfiguration( NvOdmDispDeviceHandle hDevice );

void hdmi_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values );

NvBool hdmi_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config );

#if defined(__cplusplus)
}
#endif

#endif
