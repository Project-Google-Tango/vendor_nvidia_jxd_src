/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_PANEL_LGD_H
#define INCLUDED_PANEL_LGD_H

#include "panels.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define LGD_PANEL_GUID NV_ODM_GUID('N','V','D','I','A','l','g','d')
#define LGD_TPS65914_PANEL_GUID NV_ODM_GUID('N','V','9','1','4','l','g','d')
#define LGD_E1736TPS65914_PANEL_GUID NV_ODM_GUID('N','V','9','1','4','_','l','g')
#define LGD_E1769TPS65914_PANEL_GUID NV_ODM_GUID('N','V','1','7','6','9','l','g')

void lgd_GetHal( NvOdmDispDeviceHandle hDevice );

NvBool lgd_Setup( NvOdmDispDeviceHandle hDevice );

void lgd_Release( NvOdmDispDeviceHandle hDevice );

void lgd_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
    NvOdmDispDeviceMode *modes );

NvBool lgd_SetMode( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDeviceMode *mode, NvU32 flags );

void lgd_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity );

void lgd_SetBacklightIntensity( NvOdmDispDeviceHandle hDevice, NvU8 intensity );

void lgd_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level );

NvBool lgd_GetParameter( NvOdmDispDeviceHandle hDevice,
    NvOdmDispParameter param, NvU32 *val );

void * lgd_GetConfiguration( NvOdmDispDeviceHandle hDevice );

void lgd_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values );

NvBool lgd_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config );

#if defined(__cplusplus)
}
#endif

#endif
