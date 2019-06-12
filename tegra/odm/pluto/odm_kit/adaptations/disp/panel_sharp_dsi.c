/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_disp.h"
#include "panel_sharp_dsi.h"
#include "nvodm_services.h"
#include "nvodm_query_pinmux.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query_gpio.h"
#include "nvos.h"
#include "nvodm_sdio.h"
#include "nvodm_services.h"
#include "nvodm_pmu.h"
#include "nvodm_backlight.h"
#include "bl_led_max8831.h"

#define DSI_PANEL_DEBUG(x) NvOsDebugPrintf x

//set to 0 for 1st DSI instance and 1 for 2nd DSI instance
#define DSI_INSTANCE 1    //Sharp Connected to DSIB on E1582

#define TEGRA_DC_OUT_CONTINUOUS_MODE 0
#define TEGRA_DC_OUT_ONESHOT_MODE 1

#define DC_CTRL_MODE    TEGRA_DC_OUT_CONTINUOUS_MODE
#define DELAY_MS 20

static NvOdmServicesGpioHandle s_hGpio;
static NvOdmGpioPinHandle s_Lcd_bl_en;
static NvOdmGpioPinHandle s_Lcd_rst;

typedef struct DsiPanelDataRec
{
    NvU32 DsiCmd;
    NvU32 PanelReg;
    NvU32 DataSize;
    NvU8 *pData;
    NvBool IsLongPacket;
} DsiPanelData;

static NvBool sharp_Setup( NvOdmDispDeviceHandle hDevice );
static void sharp_Release( NvOdmDispDeviceHandle hDevice );
static void sharp_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
                NvOdmDispDeviceMode *modes );
static NvBool sharp_SetMode( NvOdmDispDeviceHandle hDevice,
                NvOdmDispDeviceMode *mode, NvU32 flags );
static void sharp_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity );
static void sharp_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
                NvOdmDispDevicePowerLevel level );
static NvBool sharp_GetParameter( NvOdmDispDeviceHandle hDevice,
                NvOdmDispParameter param, NvU32 *val );
static void* sharp_GetConfiguration( NvOdmDispDeviceHandle hDevice );
static void sharp_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
                NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values );
static NvBool sharp_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
                NvOdmDispSpecialFunction function, void *config );
static void sharp_NullMode( NvOdmDispDeviceHandle hDevice );
static void sharp_Resume( NvOdmDispDeviceHandle hDevice );
static void sharp_Suspend( NvOdmDispDeviceHandle hDevice );
static NvBool sharp_PanelInit( NvOdmDispDeviceHandle hDevice );

// Color calibration
static NvU32 s_ColorCalibration_Red     = NVODM_DISP_FP_TO_FX(1.00);
static NvU32 s_ColorCalibration_Green   = NVODM_DISP_FP_TO_FX(1.00);
static NvU32 s_ColorCalibration_Blue    = NVODM_DISP_FP_TO_FX(1.00);

// Other global variables.
static NvOdmDispDsiTransport   s_trans;
static NvBool s_bBacklight = NV_FALSE;

static NvOdmDispDeviceMode sharp_Modes[] =
{
    {
        1080,                          // width
        1920,                          // height
        24,                           // bpp
        0,                            // refresh
        0,                            // frequency
#if (DC_CTRL_MODE == TEGRA_DC_OUT_ONESHOT_MODE)
        NVODM_DISP_MODE_FLAG_USE_TEARING_EFFECT,    // flags for DC one-shot mode
#else
        NVODM_DISP_MODE_FLAG_NONE,    //flags for DC continuous mode
#endif
        // timing
        {
            4,   // h_ref_to_sync,
            1,   // v_ref_to_sync,
            10,   // h_sync_width,
            2,   // v_sync_width,
            50,  // h_back_porch,
            4,   // v_back_porch
            1080,   // h_disp_active
            1920,   // v_disp_active
            100,  // h_front_porch,
            4,  // v_front_porch
        },
        NV_FALSE // deprecated.
    }
};

static NvBool sharp_discover (NvOdmDispDeviceHandle hDevice)
{
    NvU64 guid;
    NvOdmPeripheralConnectivity const *conn;

    /* get the main panel */
    guid = SHARP_LS050T1SX01_PANEL_GUID;
    /* get the connectivity info */
    conn = NvOdmPeripheralGetGuid(guid);
    if( !conn )
    {
        return NV_FALSE;
    }
    hDevice->PeripheralConnectivity = conn;
    return NV_TRUE;
}

static NvBool
sharp_Setup( NvOdmDispDeviceHandle hDevice )
{
    NvOdmServicesPmuVddRailCapabilities RailCaps;
    NvU32 SettlingTime = 0;
    NvU32 i;
    NvOdmServicesPmuHandle hPmu;

    if (!hDevice)
        return NV_FALSE;

    /* get the peripheral config */
    if( !sharp_discover(hDevice))
        return NV_FALSE;

    s_hGpio = NvOdmGpioOpen();
    if( !s_hGpio )
    {
        NV_ASSERT(!"Gpio open failed");
        return NV_FALSE;
    }

    /* Toggle lcd_bl_en on */
    s_Lcd_bl_en = NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('h' - 'a'), 2);
    if (!s_Lcd_bl_en)
    {
        NV_ASSERT( !"Pin handle could not be acquired");
        return NV_FALSE;
    }
    NvOdmGpioSetState(s_hGpio, s_Lcd_bl_en, 0x1);
    NvOdmGpioConfig( s_hGpio, s_Lcd_bl_en, NvOdmGpioPinMode_Output);

    if( !hDevice->bInitialized )
    {
        hDevice->modes = sharp_Modes;
        hDevice->nModes = NV_ARRAY_SIZE(sharp_Modes);

        hDevice->Type = NvOdmDispDeviceType_DSI;
        hDevice->Usage = NvOdmDispDeviceUsage_Primary;
        hDevice->MaxHorizontalResolution = (NvU32)sharp_Modes[0].width;
        hDevice->MaxVerticalResolution = (NvU32)sharp_Modes[0].height;
        hDevice->BaseColorSize = NvOdmDispBaseColorSize_888;
        hDevice->DataAlignment = NvOdmDispDataAlignment_MSB;
        hDevice->PinMap = NvOdmDispPinMap_Single_Rgb24_Spi5;

        hDevice->power = NvOdmDispDevicePowerLevel_Off;
        sharp_NullMode( hDevice );

        hPmu = NvOdmServicesPmuOpen();

        for( i = 0; i < hDevice->PeripheralConnectivity->NumAddress; i++ )
        {
            if( hDevice->PeripheralConnectivity->AddressList[i].Interface == NvOdmIoModule_Vdd )
            {
                /* address is the vdd rail id */
                NvOdmServicesPmuGetCapabilities( hPmu,
                    hDevice->PeripheralConnectivity->AddressList[i].Address, &RailCaps);
                /* set the rail volatage to the recommended */
                NvOdmServicesPmuSetVoltage( hPmu,
                    hDevice->PeripheralConnectivity->AddressList[i].Address, RailCaps.requestMilliVolts,
                    &SettlingTime );
                /* wait for rail to settle */
                NvOdmOsWaitUS(SettlingTime);
            }
        }
        if (SettlingTime)
        {
            NvOdmOsWaitUS(SettlingTime);
        }

        hDevice->bInitialized = NV_TRUE;
    }

    return NV_TRUE;
}

static void
sharp_NullMode( NvOdmDispDeviceHandle hDevice )
{
    if (!hDevice)
        return ;

    hDevice->CurrentMode.width = 0;
    hDevice->CurrentMode.height = 0;
    hDevice->CurrentMode.bpp = 0;
    hDevice->CurrentMode.flags = (NvU32)-1;
}

static void
sharp_Release( NvOdmDispDeviceHandle hDevice )
{
    if (!hDevice)
        return ;
    hDevice->bInitialized = NV_FALSE;
    sharp_NullMode( hDevice );
    Max8831_BacklightDeinit();
}

static void
sharp_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
        NvOdmDispDeviceMode *modes )
{
    NV_ASSERT( hDevice );
    NV_ASSERT( nModes );

    if (!hDevice || !nModes)
        return ;

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

static NvBool
sharp_SetMode( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDeviceMode *mode, NvU32 flags )
{
    NV_ASSERT( hDevice );
    if (!hDevice)
        return NV_FALSE;

    if( mode )
    {
        hDevice->CurrentMode = *mode;
    }
    else
    {
        sharp_NullMode( hDevice );
    }

    if( hDevice->power != NvOdmDispDevicePowerLevel_On )
    {
        return NV_TRUE;
    }

    /* FIXME: Check more item if necessary */
    if( mode )
    {
        if( hDevice->CurrentMode.width == mode->width &&
            hDevice->CurrentMode.height == mode->height &&
            hDevice->CurrentMode.bpp == mode->bpp &&
            hDevice->CurrentMode.flags == mode->flags)
        {
            return NV_TRUE;
        }
        sharp_PanelInit(hDevice);
    }
    else
    {
        sharp_NullMode(hDevice);
    }
    return NV_TRUE;
}


static void
sharp_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    if( !s_bBacklight )
    {
        NvOdmBacklightInit( hDevice, NV_FALSE );
        s_bBacklight = NV_TRUE;
    }

    /* Add delay before setting backlight to avoid any
       corruption and  give time for panel init to stabilize */
    NvOdmOsSleepMS(DELAY_MS);
    NvOdmBacklightIntensity( hDevice, intensity );
}

static void
sharp_SetBacklightIntensity( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    Max8831_SetBacklightIntensity(intensity);
}


static void
sharp_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level )
{
    NV_ASSERT( hDevice );
    if (!hDevice)
        return ;

    hDevice->power = level;

    switch( level ) {
    case NvOdmDispDevicePowerLevel_Off:
    case NvOdmDispDevicePowerLevel_Suspend:
        sharp_Suspend( hDevice );
        break;
    case NvOdmDispDevicePowerLevel_On:
        if( hDevice->CurrentMode.width && hDevice->CurrentMode.height &&
            hDevice->CurrentMode.bpp )
        {
            sharp_Resume( hDevice );
        }
        break;
    default:
        break;
    }
}

static NvBool
sharp_GetParameter( NvOdmDispDeviceHandle hDevice,
    NvOdmDispParameter param, NvU32 *val )
{
    NV_ASSERT( hDevice );
    NV_ASSERT( val );

    if (!hDevice || !val)
        return NV_FALSE;

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
    case NvOdmDispParameter_ColorCalibrationRed:
        *val = s_ColorCalibration_Red;
        break;
    case NvOdmDispParameter_ColorCalibrationGreen:
        *val = s_ColorCalibration_Green;
        break;
    case NvOdmDispParameter_ColorCalibrationBlue:
        *val = s_ColorCalibration_Blue;
        break;
    case NvOdmDispParameter_PwmOutputPin:
        *val = NvOdmDispPwmOutputPin_Unspecified;
        break;
    case NvOdmDispParameter_DsiInstance:
        *val = DSI_INSTANCE;
        break;
    default:
        return NV_FALSE;
    }

    return NV_TRUE;
}

static void *
sharp_GetConfiguration( NvOdmDispDeviceHandle hDevice )
{
    NvOdmDispDsiConfig *dsi = NULL;
    if (!hDevice)
        return NULL;
    dsi = &hDevice->Config.dsi;
    dsi->DataFormat = NvOdmDsiDataFormat_24BitP;
    dsi->VideoMode = NvOdmDsiVideoModeType_NonBurstSyncEnd;
    dsi->VirChannel = NvOdmDsiVirtualChannel_0;
    dsi->NumOfDataLines = 4;
#if (DC_CTRL_MODE == TEGRA_DC_OUT_ONESHOT_MODE)
    dsi->RefreshRate = 62;
#else
    dsi->RefreshRate = 60;
#endif
    dsi->Freq = 162000;
    dsi->PanelResetTimeoutMSec = 0;
    dsi->LpCommandModeFreqKHz = 4165;
    dsi->HsCommandModeFreqKHz = 20250;
    dsi->HsSupportForFrameBuffer = NV_FALSE;
    dsi->flags = 0;
    dsi->HsClockControl = NvOdmDsiHsClock_Continuous;
    dsi->EnableHsClockInLpMode = NV_FALSE;
    dsi->bIsPhytimingsValid = NV_FALSE;
    dsi->DsiDisplayMode = NvOdmDispDsiMode_Normal;
    return (void *)dsi;
}

static void
sharp_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values )
{
    if (nPins == NULL)
        return;
    *nPins = 0;
}

static NvBool
sharp_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config )
{
    switch( function )
    {
        case NvOdmDispSpecialFunction_DSI_Transport:
            if (config == NULL)
                return NV_FALSE;
            s_trans = *(NvOdmDispDsiTransport *)config;
            break;
        default:
            // Don't bother asserting.
            return NV_FALSE;
    }

    return NV_TRUE;
}

static void
sharp_Resume( NvOdmDispDeviceHandle hDevice )
{
    sharp_PanelInit( hDevice );
}

static void
sharp_Suspend( NvOdmDispDeviceHandle hDevice )
{
   //Not a valid case in bootloader
}

static NvU8 panel_internal[] = {0x0f, 0xff};
static NvU8 panel_cmd1[] = {0x04};
static NvU8 panel_cmd2[] = {0x01};
static NvU8 s_Null[] = {0x00};

static NvBool
sharp_PanelInit( NvOdmDispDeviceHandle hDevice )
{
    NvU32 seqSize;
    NvU32 i;
    NvBool b;
    static DsiPanelData s_PanelInitSeq[] = {
        //DsiCmd, PanelReg, DataSize, PData, IsLongPacket
        { 0x23,      0xB0,   1,  panel_cmd1, NV_FALSE},
        { 0x05,      0x00,   1,  s_Null, NV_FALSE},
        { 0x05,      0x00,   1,  s_Null, NV_FALSE},
        { 0x23,      0xD6,   1,  panel_cmd2, NV_FALSE},
        { 0x29,      0x51,   2,  panel_internal, NV_TRUE},
        { 0x05,      0x29,   1,  s_Null, NV_FALSE},
        { 0x05,      0x11,   1,  s_Null, NV_FALSE},
    };

    if (!hDevice)
        return NV_FALSE;

    if( !hDevice->bInitialized )
    {
        NV_ASSERT( !"device not initialized" );
        return NV_FALSE;
    }

    s_Lcd_rst= NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('h' - 'a') , 5);
    if (!s_Lcd_rst)
    {
        NV_ASSERT(!"GPIO pin acquisition failed \n");
        return NV_FALSE;
    }
    NvOdmGpioSetState(s_hGpio, s_Lcd_rst, 1);
    NvOdmGpioConfig(s_hGpio, s_Lcd_rst, NvOdmGpioPinMode_Output);
    NvOdmOsWaitUS(200);
    NvOdmGpioSetState(s_hGpio, s_Lcd_rst, 0x0);
    NvOdmOsWaitUS(200);
    NvOdmGpioSetState(s_hGpio, s_Lcd_rst, 0x1);
    NvOdmOsWaitUS(200);

    b = s_trans.NvOdmDispDsiInit(hDevice, DSI_INSTANCE, 0);
    if (!b)
    {
        NV_ASSERT("dsi init failed \n");
        return NV_FALSE;
    }

    b = s_trans.NvOdmDispDsiEnableCommandMode(hDevice, DSI_INSTANCE);
    if (!b)
    {
        NV_ASSERT("dsi command mode failed\n");
        return NV_FALSE;
    }

    seqSize = NV_ARRAY_SIZE(s_PanelInitSeq);

    for ( i =0; i<seqSize; i++)
    {
        if (s_PanelInitSeq[i].pData == NULL)
        {
            NvOdmOsSleepMS(s_PanelInitSeq[i].DataSize);
            continue;
        }

        //Delay might be required between commands
        b = s_trans.NvOdmDispDsiWrite( hDevice,
        s_PanelInitSeq[i].DsiCmd,
        s_PanelInitSeq[i].PanelReg,
        s_PanelInitSeq[i].DataSize,
        s_PanelInitSeq[i].pData,
        DSI_INSTANCE,
        s_PanelInitSeq[i].IsLongPacket );

        if( !b )
        {
            DSI_PANEL_DEBUG((" Error> DSI Panel Initialization Failed\r\n"));
            return NV_FALSE;
        }
    }

    // for backlight control
    Max8831_BacklightInit();
    NvOdmOsSleepMS(DELAY_MS);

    return NV_TRUE;
}

void
sharp_GetHal(NvOdmDispDeviceHandle hDevice)
{
    NV_ASSERT(hDevice);
    if (!hDevice)
        return ;

    hDevice->Setup = sharp_Setup;
    hDevice->Release = sharp_Release;
    hDevice->ListModes = sharp_ListModes;
    hDevice->SetMode = sharp_SetMode;
    hDevice->SetPowerLevel = sharp_SetPowerLevel;
    hDevice->GetParameter = sharp_GetParameter;
    hDevice->GetPinPolarities = sharp_GetPinPolarities;
    hDevice->GetConfiguration = sharp_GetConfiguration;
    hDevice->SetSpecialFunction = sharp_SetSpecialFunction;
    hDevice->SetBacklight = sharp_SetBacklight;
    hDevice->BacklightIntensity = sharp_SetBacklightIntensity;
}
