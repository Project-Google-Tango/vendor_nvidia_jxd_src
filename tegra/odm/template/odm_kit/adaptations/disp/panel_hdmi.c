/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_disp.h"
#include "panel_hdmi.h"
#include "nvodm_services.h"

#define MAX_PANEL_FREQ      0xFFFFFFFF

/* video height is signed number (NvS32) => Max value is 0x7FFFFFFF */
#define MAX_HDMI_HEIGHT     0x7FFFFFFF

static NvOdmDispHdmiDriveTermEntry hdmi_drvterm_tbl [] =
{
    // Drive-strength and termination for HDMI 1080 mode
    {
        {
             MAX_HDMI_HEIGHT,   // MaxHeight - don't care. Use Max value
             1080,              // MinHeight - at least 1080
             MAX_PANEL_FREQ,    // maxPanelFreqKhz - don't care. Use Max value
             0,                 // maxPanelFreqKhz - 0
             NV_TRUE,           // checkPreEmp - check pre-empesis
             NV_TRUE,           // PreEmpesisOn - preEmpesis is on
        },
        {
             NV_TRUE,           // TmdsTermEn
             0,                 // TmdsTermAdj
             0,                 // TmdsLoadAdj
             0x0c,              // TmdsCurrentLane0
             0x0c,              // TmdsCurrentLane1
             0x0c,              // TmdsCurrentLane2
             0x0c,              // TmdsCurrentLane3
        },
    },

    // Drive-strength and termination for HDMI non-1080 mode
    {
        {
             1080,              // MaxHeight - smaller than 1080
             0,                 // MinHeight - 0
             MAX_PANEL_FREQ,    // maxPanelFreqKhz - don't care. Use Max value
             0,                 // maxPanelFreqKhz - 0
             NV_FALSE,          // checkPreEmp - don't check pre-empesis
             NV_FALSE,          // PreEmpesisOn - don't care
        },
        {
             NV_FALSE,          // TmdsTermEn
             0,                 // TmdsTermAdj
             0,                 // TmdsLoadAdj
             0x0c,              // TmdsCurrentLane0
             0x0c,              // TmdsCurrentLane1
             0x0c,              // TmdsCurrentLane2
             0x0c,              // TmdsCurrentLane3
        },
    },
};

static NvOdmDispHdmiDriveTermTbl hdmi_drvtrm_info =
{
    /**
     * Comment out the number of entry and replace it with with 0,
     * since this file is just a template. Customer should update
     * hdmi_drvterm_tbl to desire value and un-comment it.
     */
    0,    //NV_ARRAY_SIZE(hdmi_drvterm_tbl),
    hdmi_drvterm_tbl,
};

static void
hdmi_nullmode( NvOdmDispDeviceHandle hDevice )
{
    hDevice->CurrentMode.width = 0;
    hDevice->CurrentMode.height = 0;
    hDevice->CurrentMode.bpp = 0;
}

NvBool hdmi_Setup( NvOdmDispDeviceHandle hDevice )
{
    if( !hDevice->bInitialized )
    {
        hDevice->Type = NvOdmDispDeviceType_HDMI;
        hDevice->Usage = NvOdmDispDeviceUsage_Unassigned;
        hDevice->MaxHorizontalResolution = 0;
        hDevice->MaxVerticalResolution = 0;
        hDevice->BaseColorSize = NvOdmDispBaseColorSize_888;
        hDevice->DataAlignment = NvOdmDispDataAlignment_MSB;
        hDevice->PinMap = NvOdmDispPinMap_Single_Rgb24_Spi5;
        hDevice->PwmOutputPin = NvOdmDispPwmOutputPin_Unspecified;
        hDevice->modes = NULL;
        hDevice->nModes = 0;
        hDevice->odmDrvTrm = (void*) &hdmi_drvtrm_info;
        hDevice->CurrentPower = NvOdmDispDevicePowerLevel_Off;

        hdmi_nullmode( hDevice );

        hDevice->bInitialized = NV_TRUE;
    }

    return NV_TRUE;
}

void hdmi_Release( NvOdmDispDeviceHandle hDevice )
{
    hDevice->bInitialized = NV_FALSE;
    hdmi_nullmode( hDevice );
}

void hdmi_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
        NvOdmDispDeviceMode *modes )
{
    NV_ASSERT( hDevice );
    NV_ASSERT( nModes );

    if( !(*nModes ) )
    {
        *nModes = hDevice->nModes;
    }
    else
    {
        NvU32 i;
        NvU32 len;

        len = NV_MIN( *nModes, hDevice->nModes );

        for( i = 0; i < len; i++ )
        {
            modes[i] = hDevice->modes[i];
        }
    }
}

NvBool hdmi_SetMode( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDeviceMode *mode, NvU32 flags )
{
    return NV_FALSE;
}

void hdmi_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
}

void hdmi_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level )
{
    NV_ASSERT( hDevice );
    hDevice->CurrentPower = level;
}

NvBool hdmi_GetParameter( NvOdmDispDeviceHandle hDevice,
    NvOdmDispParameter param, NvU32 *val )
{
    NV_ASSERT( hDevice );
    NV_ASSERT( val );

    switch( param ) {
    case NvOdmDispParameter_Type:
        *val = hDevice->Type;
        break;
    case NvOdmDispParameter_Usage:
        *val = hDevice->Usage;
        break;
    case NvOdmDispParameter_MaxHorizontalResolution:
        *val = hDevice->MaxHorizontalResolution;
        break;
    case NvOdmDispParameter_MaxVerticalResolution:
        *val = hDevice->MaxVerticalResolution;
        break;
    case NvOdmDispParameter_BaseColorSize:
        *val = hDevice->BaseColorSize;
        break;
    case NvOdmDispParameter_DataAlignment:
        *val = hDevice->DataAlignment;
        break;
    case NvOdmDispParameter_PinMap:
        *val = hDevice->PinMap;
        break;
    case NvOdmDispParameter_PwmOutputPin:
        *val = hDevice->PwmOutputPin;
        break;
    default:
        return NV_FALSE;
    }

    return NV_TRUE;
}

void * hdmi_GetConfiguration( NvOdmDispDeviceHandle hDevice )
{
    void* config = hDevice->odmDrvTrm;

    return config;
}

void hdmi_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values )
{
    *nPins = 0;
}

NvBool hdmi_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config )
{
    return NV_FALSE;
}

void hdmi_GetHal( NvOdmDispDeviceHandle hDevice )
{
    NV_ASSERT(hDevice);

    hDevice->pfnSetup = hdmi_Setup;
    hDevice->pfnRelease = hdmi_Release;
    hDevice->pfnListModes = hdmi_ListModes;
    hDevice->pfnSetMode = hdmi_SetMode;
    hDevice->pfnSetPowerLevel = hdmi_SetPowerLevel;
    hDevice->pfnGetParameter = hdmi_GetParameter;
    hDevice->pfnGetPinPolarities = hdmi_GetPinPolarities;
    hDevice->pfnGetConfiguration = hdmi_GetConfiguration;
    hDevice->pfnSetSpecialFunction = hdmi_SetSpecialFunction;

    // Backlight functions
    hDevice->pfnSetBacklight = hdmi_SetBacklight;
}
