/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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
#include "panel_lg_dsi.h"
#include "panel_jdi.h"

NvOdmDispDevice g_PrimaryDisp;

static NvBool s_init = NV_FALSE;
extern void
NvOsDebugPrintf( const char *format, ... );


void
NvOdmDispListDevices( NvU32 *nDevices, NvOdmDispDeviceHandle *phDevices )
{
    NvBool b;
    NvU32 count;
    NvOdmDisplayBoardInfo DisplayBoardInfo;

    if( !s_init )
    {
        if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayBoard,
            &DisplayBoardInfo, sizeof(DisplayBoardInfo)))
        {
            switch(DisplayBoardInfo.BoardInfo.Fab) {
            case 0x0:
                lgpanel_GetHal(&g_PrimaryDisp);
                break;
            case 0x1:
            //fab 2 is for jdi 5" panel
            case 0x2:
                jdipanel_GetHal(&g_PrimaryDisp);
                break;
            default:
                lgpanel_GetHal(&g_PrimaryDisp);
                break;
            }
        }
        b = g_PrimaryDisp.Setup( &g_PrimaryDisp );
        NV_ASSERT( b );
        b = b; // Otherwise release builds get 'unused variable'.

        s_init = NV_TRUE;
    }

    if( !(*nDevices) )
    {
        *nDevices = 1;
    }
    else
    {
        NV_ASSERT( phDevices );
        count = *nDevices;

        *phDevices = &g_PrimaryDisp;
        phDevices++;
        count--;
    }
}

void
NvOdmDispReleaseDevices( NvU32 nDevices, NvOdmDispDeviceHandle *hDevices )
{
    while( nDevices-- )
    {
        (*hDevices)->Release( *hDevices );
        hDevices++;
    }

    s_init = NV_FALSE;
}

void
NvOdmDispGetDefaultGuid( NvU64 *guid )
{
    NvOdmDisplayBoardInfo DisplayBoardInfo;

    if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayBoard,
        &DisplayBoardInfo, sizeof(DisplayBoardInfo)))
    {
        switch(DisplayBoardInfo.BoardInfo.Fab) {
        case 0x0:
            *guid = LG_PANEL_GUID;
            break;
        case 0x1:
            *guid = JDI_PANEL_GUID;
            break;
        default:
            *guid = LG_PANEL_GUID;
            break;
        }
    }

    return;
}

NvBool
NvOdmDispGetDeviceByGuid( NvU64 guid, NvOdmDispDeviceHandle *phDevice )
{
    NvU64 pDispGuid;

    NvOdmDispGetDefaultGuid(&pDispGuid);

    if (guid == pDispGuid)
    {
        *phDevice = &g_PrimaryDisp;
        return NV_TRUE;
    }
    else
    {
        *phDevice = 0;
        return NV_FALSE;
    }
}

void
NvOdmDispListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
    NvOdmDispDeviceMode *modes )
{
    if (!hDevice)
        return ;

    hDevice->ListModes( hDevice, nModes, modes );
}

NvBool
NvOdmDispSetMode( NvOdmDispDeviceHandle hDevice, NvOdmDispDeviceMode *mode,
    NvU32 flags )
{
    if (!hDevice)
        return NV_FALSE;

    return hDevice->SetMode( hDevice, mode, flags );
}

void
NvOdmDispSetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    if (!hDevice)
        return ;

    hDevice->SetBacklight( hDevice, intensity );
}

void
NvOdmDispSetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level )
{
    if (!hDevice)
        return ;

    hDevice->SetPowerLevel( hDevice, level );
}

NvBool
NvOdmDispGetParameter( NvOdmDispDeviceHandle hDevice,
    NvOdmDispParameter param, NvU32 *val )
{
    if (!hDevice)
        return NV_FALSE;

    return hDevice->GetParameter( hDevice, param, val );
}

void *
NvOdmDispGetConfiguration( NvOdmDispDeviceHandle hDevice )
{
    if (!hDevice)
        return NULL;

    return hDevice->GetConfiguration( hDevice );
}

void
NvOdmDispGetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values )
{
    if (!hDevice)
        return ;

    hDevice->GetPinPolarities( hDevice, nPins, Pins, Values );
}

NvBool
NvOdmDispSetSpecialFunction( NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config )
{
    if (!hDevice)
        return NV_FALSE;

    return hDevice->SetSpecialFunction( hDevice, function, config );
}
NvBool
NvOdmBacklightInit(NvOdmDispDeviceHandle hDevice, NvBool bReopen)
{
    if (!hDevice)
        return NV_FALSE;

    if (hDevice->BacklightSetup != NULL)
        return hDevice->BacklightSetup(hDevice, bReopen);
    else
        return NV_FALSE;
}

void
NvOdmBacklightDeinit(NvOdmDispDeviceHandle hDevice)
{

    if (hDevice->BacklightRelease != NULL)
        hDevice->BacklightRelease(hDevice);
}

void
NvOdmBacklightIntensity(NvOdmDispDeviceHandle hDevice,
                      NvU8 intensity)
{
    if (hDevice->BacklightIntensity != NULL)
        hDevice->BacklightIntensity(hDevice, intensity);
}

