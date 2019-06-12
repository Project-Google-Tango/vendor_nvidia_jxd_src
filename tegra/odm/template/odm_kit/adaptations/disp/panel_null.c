/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_disp.h"
#include "panel_null.h"
#include "nvodm_services.h"

static NvOdmDispDeviceMode null_modes[] =
{
    // WVGA
    { 800, // width
      480, // height
      16, // bpp
      0, // refresh
      0, // frequency
      0, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        1, // h_sync_width
        1, // v_sync_width
        10, // h_back_porch
        1, // v_back_porch
        800, // h_disp_active
        480, // v_disp_active
        2, // h_front_porch
        2, // v_front_porch
      },
      NV_FALSE // partial
    },

    // portrait
    { 272, // width
      480, // height
      16, // bpp
      0, // refresh
      0, // frequency
      0, // flags
      // timing
      { 1, // h_ref_to_sync
        1, // v_ref_to_sync
        1, // h_sync_width
        1, // v_sync_width
        10, // h_back_porch
        1, // v_back_porch
        272, // h_disp_active
        480, // v_disp_active
        2, // h_front_porch
        2, // v_front_porch
      },
      NV_FALSE // partial
    },
};

static void
null_nullmode( NvOdmDispDeviceHandle hDevice )
{
    hDevice->CurrentMode.width = 0;
    hDevice->CurrentMode.height = 0;
    hDevice->CurrentMode.bpp = 0;
}

NvBool null_Setup( NvOdmDispDeviceHandle hDevice )
{
    if( !hDevice->bInitialized )
    {
        hDevice->Type = NvOdmDispDeviceType_TFT;
        hDevice->Usage = NvOdmDispDeviceUsage_Unassigned;
        hDevice->MaxHorizontalResolution = 800;
        hDevice->MaxVerticalResolution = 480;
        hDevice->BaseColorSize = NvOdmDispBaseColorSize_888;
        hDevice->DataAlignment = NvOdmDispDataAlignment_MSB;
        hDevice->PinMap = NvOdmDispPinMap_Single_Rgb24_Spi5;
        hDevice->PwmOutputPin = NvOdmDispPwmOutputPin_Unspecified;
        hDevice->modes = null_modes;
        hDevice->nModes = NV_ARRAY_SIZE(null_modes);
        hDevice->CurrentPower = NvOdmDispDevicePowerLevel_Off;

        null_nullmode( hDevice );

        hDevice->bInitialized = NV_TRUE;
    }

    return NV_TRUE;
}

void null_Release( NvOdmDispDeviceHandle hDevice )
{
    hDevice->bInitialized = NV_FALSE;
    null_nullmode( hDevice );
}

void null_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
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

NvBool null_SetMode( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDeviceMode *mode, NvU32 flags )
{
    NV_ASSERT( hDevice );

    if( mode )
    {
        hDevice->CurrentMode = *mode;
    }
    else
    {
        null_nullmode( hDevice );
    }

    return NV_TRUE;
}

void null_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
}

void null_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level )
{
    NV_ASSERT( hDevice );
    hDevice->CurrentPower = level;
}

NvBool null_GetParameter( NvOdmDispDeviceHandle hDevice,
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

void * null_GetConfiguration( NvOdmDispDeviceHandle hDevice )
{
    NvOdmDispTftConfig *tft = &hDevice->Config.tft;
    return (void *)tft;
}

void null_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values )
{
    *nPins = 0;
}

NvBool null_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config )
{
    return NV_FALSE;
}

void null_GetHal( NvOdmDispDeviceHandle hDevice )
{
    NV_ASSERT(hDevice);

    hDevice->pfnSetup = null_Setup;
    hDevice->pfnRelease = null_Release;
    hDevice->pfnListModes = null_ListModes;
    hDevice->pfnSetMode = null_SetMode;
    hDevice->pfnSetPowerLevel = null_SetPowerLevel;
    hDevice->pfnGetParameter = null_GetParameter;
    hDevice->pfnGetPinPolarities = null_GetPinPolarities;
    hDevice->pfnGetConfiguration = null_GetConfiguration;
    hDevice->pfnSetSpecialFunction = null_SetSpecialFunction;

    // Backlight functions
    hDevice->pfnSetBacklight = null_SetBacklight;
}
