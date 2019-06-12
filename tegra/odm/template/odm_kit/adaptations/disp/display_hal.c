/*
 * Copyright (c) 2008 - 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvodm_disp.h"
#include "display_hal.h"
#include "nvodm_backlight.h"
#include "nvodm_query_discovery.h"
#include "nvodm_keylist_reserved.h"
#include "nvrm_drf.h"
#include "amoled_amaf002.h"
#include "panel_samsungsdi.h"
#include "panel_sharp_wvga.h"
#include "panel_sharp_wvgap.h"
#include "panel_tpo_wvga.h"
#include "panel_sharp_dsi.h"
#include "panel_lvds_wsga.h"
#include "panel_null.h"
#include "panel_hdmi.h"
#include "bl_pcf50626.h"
#include "bl_ap20.h"

#define NUM_DEVICES 1

#define INVALID_HANDLE ((NvOdmDispDeviceHandle)0)
#define HANDLE(x) (NvOdmDispDeviceHandle)(((x)&0x7fffffffUL) | 0x80000000UL)
#define IS_VALID_HANDLE(x) ( ((NvU32)(x)) & 0x80000000UL )
#define HANDLE_INDEX(x) (((NvU32)(x)) & 0x7fffffffUL)

#define NULL_PANEL_GUID NV_ODM_GUID('f','f','a','_','l','c','d','0')
#define FAKE_HDMI_GUID NV_ODM_GUID('f','a','k','e','h','d','m','i')
#define FAKE_CRT_GUID NV_ODM_GUID('f','a','k','e','_','c','r','t')

NvOdmDispDevice g_AmoLedAmaF002;
NvOdmDispDevice g_SharpWvga;
NvOdmDispDevice g_SharpWvgap;
NvOdmDispDevice g_SamsungDsi;
NvOdmDispDevice g_NullPanel;
NvOdmDispDevice g_TpoWvga;
NvOdmDispDevice g_SharpDsi;
NvOdmDispDevice g_LvdsWsga;
NvOdmDispDevice g_HdmiDrvTerm;
NvOdmDispDevice g_FakeRemovable;

static NvOdmDispDeviceHandle
GetDeviceHandleByGuid(NvU64 Guid)
{
    switch (Guid) {
    case AMOLED_AMAF002_GUID:
        AmoLedAmaF002_GetHal(&g_AmoLedAmaF002);
        //  Call the backlight's "GetHAL" in order to get the
        //  backlight associated with this GUID
        g_AmoLedAmaF002.pfnSetup(&g_AmoLedAmaF002);
        return &g_AmoLedAmaF002;

    case SHARP_WVGA_GUID:
        sharpwvga_GetHal(&g_SharpWvga);
        BL_PCF50626_GetHal(&g_SharpWvga);
        g_SharpWvga.pfnSetup(&g_SharpWvga);
        return &g_SharpWvga;

    case SHARP_WVGAP_GUID:
        sharpwvgap_GetHal(&g_SharpWvgap);
        BL_PCF50626_GetHal(&g_SharpWvgap);
        g_SharpWvgap.pfnSetup(&g_SharpWvgap);
        return &g_SharpWvgap;

    case SHARP_WVGA_AP20_GUID:
        sharpwvga_GetHal(&g_SharpWvga);
        BL_AP20_GetHal(&g_SharpWvga);
        g_SharpWvga.pfnSetup(&g_SharpWvga);
        return &g_SharpWvga;

    case SAMSUNG_DSI_GUID:
        samsungdsi_GetHal(&g_SamsungDsi);

        // This panel does not require a backlight.
        g_SamsungDsi.pfnBacklightSetup = NULL;
        g_SamsungDsi.pfnBacklightRelease = NULL;
        g_SamsungDsi.pfnBacklightIntensity = NULL;

        g_SamsungDsi.pfnSetup(&g_SamsungDsi);
        return &g_SamsungDsi;

    case TPO_WVGA_GUID:
        tpowvga_GetHal(&g_TpoWvga);        
        BL_PCF50626_GetHal(&g_TpoWvga);
        g_TpoWvga.pfnSetup(&g_TpoWvga);
        return &g_TpoWvga;

    case SHARP_DSI_GUID:
        SharpDSI_GetHal(&g_SharpDsi);
        BL_PCF50626_GetHal(&g_SharpDsi);
        g_SharpDsi.pfnSetup(&g_SharpDsi);
        return &g_SharpDsi;

    case LVDS_WSVGA_GUID:
        lvds_wsga_GetHal(&g_LvdsWsga);
        g_LvdsWsga.pfnSetup(&g_LvdsWsga);
        return &g_LvdsWsga;

    case FAKE_HDMI_GUID:
        g_FakeRemovable.Type = NvOdmDispDeviceType_HDMI;
        return &g_FakeRemovable;

    case FAKE_CRT_GUID:
        g_FakeRemovable.Type = NvOdmDispDeviceType_CRT;
        return &g_FakeRemovable;

    case NULL_PANEL_GUID:
    default:
        null_GetHal(&g_NullPanel);

        // This panel does not require a backlight.
        g_NullPanel.pfnBacklightSetup = NULL;
        g_NullPanel.pfnBacklightRelease = NULL;
        g_NullPanel.pfnBacklightIntensity = NULL;

        g_NullPanel.pfnSetup(&g_NullPanel);
        return &g_NullPanel;
    }
}

void
NvOdmDispListDevices(NvU32 *nDevices, NvOdmDispDeviceHandle *phDevices)
{
    NvOdmPeripheralSearch SearchAttr = NvOdmPeripheralSearch_PeripheralClass;
    NvU32 SearchVal = NvOdmPeripheralClass_Display;
    NvU64 GuidList[16];
    NvU32 Cnt = NV_ARRAY_SIZE(GuidList);
    NvU32 count;
    NvOdmDispDeviceHandle hDisplay = NULL;
    NvOdmServicesKeyListHandle hKeyList = NULL;
    NvU32 opt = 0;

#define TEGRA_DEVKIT_BCT_CUSTOPT_0_DISPLAY_OPTION_RANGE      22:20
#define TEGRA_DEVKIT_BCT_CUSTOPT_0_DISPLAY_OPTION_DEFAULT    0x0UL
// embedded panel (lvds, dsi, etc)
#define TEGRA_DEVKIT_BCT_CUSTOPT_0_DISPLAY_OPTION_EMBEDDED   0x0UL
// no panels (external or embedded)
#define TEGRA_DEVKIT_BCT_CUSTOPT_0_DISPLAY_OPTION_NULL       0x1UL
// use hdmi as the primary
#define TEGRA_DEVKIT_BCT_CUSTOPT_0_DISPLAY_OPTION_HDMI       0x2UL
// use crt as the primary 
#define TEGRA_DEVKIT_BCT_CUSTOPT_0_DISPLAY_OPTION_CRT        0x3UL

    // TODO: Add support for more multiple simultaneous panels

    hKeyList = NvOdmServicesKeyListOpen();
    if( hKeyList )
    {
        opt = NvOdmServicesGetKeyValue( hKeyList,
           NvOdmKeyListId_ReservedBctCustomerOption );
        NvOdmServicesKeyListClose( hKeyList );
    }

    // test code for hdmi as primary
    //opt = NV_DRF_DEF( TEGRA_DEVKIT, BCT_CUSTOPT, DISPLAY_OPTION, HDMI );

    switch( NV_DRF_VAL( TEGRA_DEVKIT, BCT_CUSTOPT, DISPLAY_OPTION, opt ) ) {
    case TEGRA_DEVKIT_BCT_CUSTOPT_0_DISPLAY_OPTION_NULL:
        hDisplay = GetDeviceHandleByGuid( NULL_PANEL_GUID );
        break;
    case TEGRA_DEVKIT_BCT_CUSTOPT_0_DISPLAY_OPTION_HDMI:
        hDisplay = GetDeviceHandleByGuid( FAKE_HDMI_GUID );
        break;
    case TEGRA_DEVKIT_BCT_CUSTOPT_0_DISPLAY_OPTION_CRT:
        hDisplay = GetDeviceHandleByGuid( FAKE_CRT_GUID );
        break;
    case TEGRA_DEVKIT_BCT_CUSTOPT_0_DISPLAY_OPTION_EMBEDDED:
    default:
        Cnt = NvOdmPeripheralEnumerate( &SearchAttr, &SearchVal, 1,
            GuidList, Cnt );
        if( !Cnt )
        {
            GuidList[0] = NULL_PANEL_GUID;
        }

        hDisplay = GetDeviceHandleByGuid(GuidList[0]);
        break;
    }        

    hDisplay->Usage = NvOdmDispDeviceUsage_Primary;

    count = *nDevices;
    *nDevices = 1;
    if (count)
    {
        NV_ASSERT(phDevices);
        *phDevices = hDisplay;
    }
}

void
NvOdmDispListModes(NvOdmDispDeviceHandle hDevice,
                   NvU32 *nModes,
                   NvOdmDispDeviceMode *modes )
{
    if (hDevice && hDevice->pfnListModes)
    {
        hDevice->pfnListModes(hDevice, nModes, modes);
    }
    else
    {
        *nModes = 0;
    }
}

void
NvOdmDispReleaseDevices(NvU32 nDevices, NvOdmDispDeviceHandle *hDevices)
{
    while( nDevices-- )
    {
        (*hDevices)->pfnRelease(*hDevices);
        hDevices++;
    }
}

void
NvOdmDispGetDefaultGuid( NvU64 *guid )
{
    NvOdmPeripheralSearch SearchAttr = NvOdmPeripheralSearch_PeripheralClass;
    NvU32 SearchVal = NvOdmPeripheralClass_Display;

    // Get first display device found
    // TO DO: Add support for more multiple simultaneous panels
    if ( !NvOdmPeripheralEnumerate(&SearchAttr, &SearchVal, 1, guid, 1) )
    {
        *guid = NULL_PANEL_GUID;
    }
}

NvBool
NvOdmDispGetDeviceByGuid( NvU64 guid, NvOdmDispDeviceHandle *phDevice )
{
    switch( guid ) {
    case AMOLED_AMAF002_GUID:
        *phDevice = &g_AmoLedAmaF002;
        break;
    case SHARP_WVGA_GUID:
    case SHARP_WVGA_AP20_GUID:
        *phDevice = &g_SharpWvga;
        break;
    case SHARP_WVGAP_GUID:
    case SHARP_WVGAP_AP20_GUID:
        *phDevice = &g_SharpWvgap;
        break;
    case SAMSUNG_DSI_GUID:
        *phDevice = &g_SamsungDsi;
        break;
    case NULL_PANEL_GUID:
        *phDevice = &g_NullPanel;
        break;
    case TPO_WVGA_GUID:
        *phDevice = &g_TpoWvga;
        break;
    case SHARP_DSI_GUID:
        *phDevice = &g_SharpDsi;
        break;
    case LVDS_WSVGA_GUID:
        *phDevice = &g_LvdsWsga;
        break;
    case NV_HDMI_PARAM_ODM_ID:
        if (0 == g_HdmiDrvTerm.Type)
        {
            hdmi_GetHal(&g_HdmiDrvTerm);
            g_HdmiDrvTerm.pfnSetup(&g_HdmiDrvTerm);
        }
        *phDevice = &g_HdmiDrvTerm;
        break;
    default:
        *phDevice = 0;
        return NV_FALSE;
    }

    return NV_TRUE;
}

NvBool
NvOdmDispSetMode(NvOdmDispDeviceHandle hDevice, NvOdmDispDeviceMode *mode,
    NvU32 flags )
{
    NV_ASSERT(hDevice);
    return hDevice->pfnSetMode(hDevice, mode, flags);
}

void
NvOdmDispSetBacklight(NvOdmDispDeviceHandle hDevice,
                      NvU8 intensity)
{
    if (hDevice->pfnSetBacklight != NULL)
        hDevice->pfnSetBacklight(hDevice, intensity);
}

NvBool
NvOdmBacklightInit(NvOdmDispDeviceHandle hDevice, NvBool bReopen)
{
    if (hDevice->pfnBacklightSetup != NULL)
        return hDevice->pfnBacklightSetup(hDevice, bReopen);
    else
        return NV_FALSE;
}

void
NvOdmBacklightDeinit(NvOdmDispDeviceHandle hDevice)
{
    if (hDevice->pfnBacklightRelease != NULL)
        hDevice->pfnBacklightRelease(hDevice);
}

void
NvOdmBacklightIntensity(NvOdmDispDeviceHandle hDevice,
                      NvU8 intensity)
{
    if (hDevice->pfnBacklightIntensity != NULL)
        hDevice->pfnBacklightIntensity(hDevice, intensity);
}

void
NvOdmDispSetPowerLevel(NvOdmDispDeviceHandle hDevice,
                       NvOdmDispDevicePowerLevel level)
{
    NV_ASSERT(hDevice);
    if( hDevice->pfnSetPowerLevel )
    {
        hDevice->pfnSetPowerLevel(hDevice, level);
    }
}

NvBool
NvOdmDispGetParameter(NvOdmDispDeviceHandle hDevice,
                      NvOdmDispParameter param,
                      NvU32 *val)
{
    NV_ASSERT(hDevice);
    if( hDevice->pfnGetParameter )
    {
        return hDevice->pfnGetParameter(hDevice, param, val);
    }

    // default implementation
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
    case NvOdmDispParameter_ColorCalibrationRed:
        *val = NVODM_DISP_FP_TO_FX(1.0633);
        break;
    case NvOdmDispParameter_ColorCalibrationGreen:
        *val = NVODM_DISP_FP_TO_FX(0.9959);
        break;
    case NvOdmDispParameter_ColorCalibrationBlue:
        *val = NVODM_DISP_FP_TO_FX(0.8724);
        break;
    default:
        return NV_FALSE;
    }

    return NV_TRUE;
}

void *
NvOdmDispGetConfiguration(NvOdmDispDeviceHandle hDevice)
{
    NV_ASSERT(hDevice);
    if( hDevice->pfnGetConfiguration )
    {
        return hDevice->pfnGetConfiguration(hDevice);
    }
    return 0;
}

void
NvOdmDispGetPinPolarities(NvOdmDispDeviceHandle hDevice,
    NvU32 *nPins, 
    NvOdmDispPin *pins,
    NvOdmDispPinPolarity *vals)
{
    NV_ASSERT(hDevice);

    if( hDevice->pfnGetPinPolarities )
    {
        hDevice->pfnGetPinPolarities(hDevice, nPins, pins, vals);
    }
}

NvBool
NvOdmDispSetSpecialFunction(NvOdmDispDeviceHandle hDevice,
                            NvOdmDispSpecialFunction function,
                            void *config)
{
    if( hDevice && hDevice->pfnSetSpecialFunction )
    {
        return hDevice->pfnSetSpecialFunction(hDevice, function, config);
    }
    return NV_FALSE;
}
