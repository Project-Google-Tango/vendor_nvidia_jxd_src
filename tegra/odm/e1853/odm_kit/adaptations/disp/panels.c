/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "panels.h"
#include "nvodm_services.h"
#include "nvodm_backlight.h"
#include "panel_lcd_wvga.h"

static NvOdmDispDevice g_PrimaryDisp;
static NvBool s_init = NV_FALSE;

void
NvOdmDispListDevices(NvU32 *nDevices, NvOdmDispDeviceHandle *phDevices)
{
    NV_ASSERT(nDevices);
    if (!s_init)
    {
        lcd_wvga_GetHal(&g_PrimaryDisp);
        if (!g_PrimaryDisp.Setup(&g_PrimaryDisp))
        {
            *nDevices = 0;
            return;
        }
        s_init = NV_TRUE;
    }

    if (!(*nDevices))
    {
        *nDevices = 1;
    }
    else
    {
        NV_ASSERT(phDevices);
        *phDevices = &g_PrimaryDisp;
    }
}

void
NvOdmDispReleaseDevices(NvU32 nDevices, NvOdmDispDeviceHandle *hDevices)
{
}

void
NvOdmDispGetDefaultGuid(NvU64 *guid)
{
    NV_ASSERT(guid);
    *guid = LCD_WVGA_GUID;
}

NvBool
NvOdmDispGetDeviceByGuid(NvU64 guid, NvOdmDispDeviceHandle *phDevice)
{
    NV_ASSERT(phDevice);
    switch (guid)
    {
        case LCD_WVGA_GUID:
            *phDevice = &g_PrimaryDisp;
            break;
        default:
            *phDevice = 0;
            return NV_FALSE;
    }
    return NV_TRUE;
}

void
NvOdmDispListModes(NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
    NvOdmDispDeviceMode *modes)
{
    NV_ASSERT(hDevice);
    hDevice->ListModes(hDevice, nModes, modes);
}

NvBool
NvOdmDispSetMode(NvOdmDispDeviceHandle hDevice, NvOdmDispDeviceMode *mode,
    NvU32 flags)
{
    return NV_TRUE;
}

void
NvOdmDispSetPowerLevel(NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level)
{
}

NvBool
NvOdmDispGetParameter(NvOdmDispDeviceHandle hDevice,
    NvOdmDispParameter param, NvU32 *val)
{
    NV_ASSERT(hDevice);
    return hDevice->GetParameter(hDevice, param, val);
}

void *
NvOdmDispGetConfiguration(NvOdmDispDeviceHandle hDevice)
{
    NV_ASSERT(hDevice);
    return hDevice->GetConfiguration(hDevice);
}

void
NvOdmDispGetPinPolarities(NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values)
{
    NV_ASSERT(nPins);
    if (hDevice->GetPinPolarities)
        hDevice->GetPinPolarities( hDevice, nPins, Pins, Values );
    else
        *nPins = 0;
}

NvBool
NvOdmDispSetSpecialFunction(NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config)
{
    return NV_FALSE;
}

NvBool
NvOdmBacklightInit(NvOdmDispDeviceHandle hDevice, NvBool bReopen)
{
    return NV_FALSE;
}

void
NvOdmBacklightDeinit(NvOdmDispDeviceHandle hDevice)
{
}

void
NvOdmBacklightIntensity(NvOdmDispDeviceHandle hDevice, NvU8 intensity)
{
}

void
NvOdmDispSetBacklight(NvOdmDispDeviceHandle hDevice, NvU8 intensity)
{
}
