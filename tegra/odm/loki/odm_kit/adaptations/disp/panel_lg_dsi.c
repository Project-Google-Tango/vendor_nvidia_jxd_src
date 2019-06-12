/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_disp.h"
#include "panel_lg_dsi.h"
#include "nvodm_services.h"
#include "nvodm_query_pinmux.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query_gpio.h"
#include "nvos.h"
#include "nvodm_sdio.h"
#include "nvodm_services.h"
#include "nvodm_pmu.h"
#include "nvodm_backlight.h"

#if defined(DISP_OAL)
#include "nvbl_debug.h"
#else
#define DSI_PANEL_DEBUG(x) NvOsDebugPrintf x
#endif
#define DSI_PANEL_DEBUG(x) NvOsDebugPrintf x

//set to 0 for 1st DSI instance and 1 for 2nd DSI instance
#define DSI_INSTANCE 0

// Backlight intensity related macros
#define DEFAULT_BACKLIGHT_INTENSITY 128
#define DEFAULT_PWM_SIGNAL_FREQUENCY 25000

#define TEGRA_DC_OUT_CONTINUOUS_MODE 0
#define TEGRA_DC_OUT_ONESHOT_MODE 1

#define DC_CTRL_MODE    TEGRA_DC_OUT_CONTINUOUS_MODE

static NvOdmGpioPinHandle s_Lcd_bl_pwm;
static NvOdmGpioPinHandle s_Lcd_rst;
static NvOdmGpioPinHandle s_En_Vdd_bl = NULL;
static NvOdmGpioPinHandle s_En_Vdd_Lcd = NULL;

typedef struct DsiPanelDataRec
{
    NvU32 DsiCmd;
    NvU32 PanelReg;
    NvU32 DataSize;
    NvU8 *pData;
    NvBool IsLongPacket;
} DsiPanelData;

static NvBool lgpanel_Setup( NvOdmDispDeviceHandle hDevice );
static void lgpanel_Release( NvOdmDispDeviceHandle hDevice );
static void lgpanel_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
                NvOdmDispDeviceMode *modes );
static NvBool lgpanel_SetMode( NvOdmDispDeviceHandle hDevice,
                NvOdmDispDeviceMode *mode, NvU32 flags );
static void lgpanel_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity );
static void lgpanel_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
                NvOdmDispDevicePowerLevel level );
static NvBool lgpanel_GetParameter( NvOdmDispDeviceHandle hDevice,
                NvOdmDispParameter param, NvU32 *val );
static void * lgpanel_GetConfiguration( NvOdmDispDeviceHandle hDevice );
static void lgpanel_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
                NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values );
static NvBool lgpanel_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
                NvOdmDispSpecialFunction function, void *config );
static void lgpanel_NullMode( NvOdmDispDeviceHandle hDevice );
static void lgpanel_Resume( NvOdmDispDeviceHandle hDevice );
static void lgpanel_Suspend( NvOdmDispDeviceHandle hDevice );
static NvBool lgpanel_PanelInit( NvOdmDispDeviceHandle hDevice, NvOdmDispDeviceMode *mode);

// Color calibration
static NvU32 s_ColorCalibration_Red     = NVODM_DISP_FP_TO_FX(1.00);
static NvU32 s_ColorCalibration_Green   = NVODM_DISP_FP_TO_FX(1.00);
static NvU32 s_ColorCalibration_Blue    = NVODM_DISP_FP_TO_FX(1.00);

// Other global variables.
static NvOdmDispDsiTransport   s_trans;
static NvBool s_bBacklight = NV_FALSE;
static NvOdmServicesPwmHandle s_hOdmPwm = NULL;
static NvOdmGpioPinHandle s_Lcd_bl_en;
static NvOdmServicesGpioHandle s_hGpio;

/********************************************
 * Please use byte clock for the following table
 * Byte Clock to Pixel Clock conversion:
 * The DSI D-PHY calculation:
 * h_total = (h_sync_width + h_back_porch + h_disp_active + h_front_porch)
 * v_total = (v_sync_width + v_back_porch + v_disp_active + v_front_porch)
 * dphy_clk = h_total * v_total * fps * bpp / data_lanes / 2
 ********************************************/
static NvOdmDispDeviceMode lgpanel_Modes[] =
{
    {
        720,                          // width
        1280,                          // height
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
            4,   // h_sync_width,
            4,   // v_sync_width,
            112,  // h_back_porch,
            12,   // v_back_porch
            720,   // h_disp_active
            1280,   // v_disp_active
            12,  // h_front_porch,
            8,  // v_front_porch
        },
        NV_FALSE // deprecated.
    }
};

static NvBool lgpanel_discover (NvOdmDispDeviceHandle hDevice)
{
    NvU64 guid;
    NvOdmPeripheralConnectivity const *conn;
    /* get the main panel */
    guid = LG_PANEL_GUID;
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
lgpanel_Setup( NvOdmDispDeviceHandle hDevice )
{
    NvOdmPeripheralConnectivity const *pCon;
    NvOdmServicesPmuVddRailCapabilities RailCaps;
    NvU32 SettlingTime = 0;
    NvU32 i;
    NvOdmServicesPmuHandle hPmu;

    if (!hDevice)
        return NV_FALSE;
    /* get the peripheral config */
    if( !lgpanel_discover(hDevice))
        return NV_FALSE;

    if( !hDevice->bInitialized )
    {
        hDevice->modes = lgpanel_Modes;
        hDevice->nModes = NV_ARRAY_SIZE(lgpanel_Modes);

        hDevice->Type = NvOdmDispDeviceType_DSI;
        hDevice->Usage = NvOdmDispDeviceUsage_Primary;
        hDevice->MaxHorizontalResolution = (NvU32)lgpanel_Modes[0].width;
        hDevice->MaxVerticalResolution = (NvU32)lgpanel_Modes[0].height;
        hDevice->BaseColorSize = NvOdmDispBaseColorSize_888;
        hDevice->DataAlignment = NvOdmDispDataAlignment_MSB;
        hDevice->PinMap = NvOdmDispPinMap_Single_Rgb24_Spi5;

        hDevice->power = NvOdmDispDevicePowerLevel_Off;
        lgpanel_NullMode( hDevice );
        hPmu = NvOdmServicesPmuOpen();

        if (!s_hGpio)
        {
            s_hGpio = NvOdmGpioOpen();
            if (!s_hGpio)
            {
                NV_ASSERT(!"GPIO open failed \n");
                return NV_FALSE;
            }
        }

        if (!s_Lcd_rst)
        {
            s_Lcd_rst= NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('h' - 'a') , 3);
            if (!s_Lcd_rst)
            {
                NV_ASSERT(!"GPIO pin acquisition failed \n");
                return NV_FALSE;
            }
            NvOdmGpioSetState(s_hGpio, s_Lcd_rst, 0x0);
            NvOdmGpioConfig(s_hGpio, s_Lcd_rst, NvOdmGpioPinMode_Output);
            NvOdmOsSleepMS(20);
        }

        s_En_Vdd_Lcd = NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('u' - 'a') , 4);
        if (!s_En_Vdd_Lcd)
        {
            NV_ASSERT(!"GPIO pin acquisition failed \n");
            return NV_FALSE;
        }
        NvOdmGpioSetState(s_hGpio, s_En_Vdd_Lcd, 1);
        NvOdmGpioConfig(s_hGpio, s_En_Vdd_Lcd, NvOdmGpioPinMode_Output);

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

        NvOdmServicesPmuClose(hPmu);

        // Get PWM handle for backlight intensity
        s_hOdmPwm = NvOdmPwmOpen();
        NV_ASSERT(s_hOdmPwm);

        hDevice->bInitialized = NV_TRUE;
    }

    return NV_TRUE;
}

static void
lgpanel_NullMode( NvOdmDispDeviceHandle hDevice )
{
    if (!hDevice)
        return ;

    hDevice->CurrentMode.width = 0;
    hDevice->CurrentMode.height = 0;
    hDevice->CurrentMode.bpp = 0;
    hDevice->CurrentMode.flags = (NvU32)-1;
}

static void
lgpanel_Release( NvOdmDispDeviceHandle hDevice )
{
    if (!hDevice)
        return ;
    hDevice->bInitialized = NV_FALSE;
    lgpanel_NullMode( hDevice );
}

static void
lgpanel_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
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
lgpanel_SetMode( NvOdmDispDeviceHandle hDevice,
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
        lgpanel_NullMode( hDevice );
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
    lgpanel_PanelInit(hDevice, mode);

    }
    else
    {
        lgpanel_NullMode(hDevice);
    }
    return NV_TRUE;
}


static void
lgpanel_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    if( !s_bBacklight )
    {
        NvOdmBacklightInit( hDevice, NV_FALSE );
        s_bBacklight = NV_TRUE;
    }

    // Default brightness value (same as used in kernel display for BL menu framebuffer)
    intensity = DEFAULT_BACKLIGHT_INTENSITY;
    NvOdmBacklightIntensity( hDevice, intensity );
}

static void
lgpanel_SetBacklightIntensity( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    NvU32 RequestedFreq, ReturnedFreq;
    NvU32 DutyCycle, PwmPin = 0;
    unsigned int i;
    const NvOdmPeripheralConnectivity *conn;

    NV_ASSERT( hDevice );
    NV_ASSERT(s_hOdmPwm);

    conn = hDevice->PeripheralConnectivity;
    if (!conn)
        return;

    for (i=0; i<conn->NumAddress; i++)
    {
        if (conn->AddressList[i].Interface == NvOdmIoModule_Pwm)
        {
            PwmPin = conn->AddressList[i].Instance;
            break;
        }
    }

    // PWM signal frequency in Hz
    RequestedFreq = DEFAULT_PWM_SIGNAL_FREQUENCY;

    // Upper 16 bits of DutyCycle is intensity percentage (whole number portion)
    // Intensity must be scaled from 0 - 255 to a percentage.
    DutyCycle = ((intensity * 100)/255) << 16;

    if (intensity != 0)
    {
        NvOdmGpioSetState(s_hGpio, s_Lcd_bl_en, 0x1);
        NvOdmPwmConfig(s_hOdmPwm, NvOdmPwmOutputId_PWM1,
                       NvOdmPwmMode_Enable, DutyCycle, &RequestedFreq,
                       &ReturnedFreq);
    }
    else
    {
        NvOdmGpioSetState(s_hGpio, s_Lcd_bl_en, 0x0);
        NvOdmPwmConfig(s_hOdmPwm, NvOdmPwmOutputId_PWM1,
                       NvOdmPwmMode_Enable, 0, &RequestedFreq,
                       &ReturnedFreq);
    }
}


static void
lgpanel_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level )
{
    NV_ASSERT( hDevice );
    if (!hDevice)
        return ;

    hDevice->power = level;

    switch( level ) {
    case NvOdmDispDevicePowerLevel_Off:
    case NvOdmDispDevicePowerLevel_Suspend:
        lgpanel_Suspend( hDevice );
        break;
    case NvOdmDispDevicePowerLevel_On:
        if( hDevice->CurrentMode.width && hDevice->CurrentMode.height &&
            hDevice->CurrentMode.bpp )
        {
            lgpanel_Resume( hDevice );
        }
        break;
    default:
        break;
    }
}

static NvBool
lgpanel_GetParameter( NvOdmDispDeviceHandle hDevice,
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
lgpanel_GetConfiguration( NvOdmDispDeviceHandle hDevice )
{
    NvOdmDispDsiConfig *dsi = NULL;
    if (!hDevice)
        return NULL;
    dsi = &hDevice->Config.dsi;
    dsi->DataFormat = NvOdmDsiDataFormat_24BitP;
    dsi->VideoMode = NvOdmDsiVideoModeType_NonBurst;
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
lgpanel_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values )
{
    *nPins = 0;
}

static NvBool
lgpanel_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config )
{
    switch( function )
    {
        case NvOdmDispSpecialFunction_DSI_Transport:
            s_trans = *(NvOdmDispDsiTransport *)config;
            break;
        default:
            // Don't bother asserting.
            return NV_FALSE;
    }

    return NV_TRUE;
}

static void
lgpanel_Resume( NvOdmDispDeviceHandle hDevice )
{
    lgpanel_PanelInit( hDevice, &hDevice->CurrentMode );
}

static void
lgpanel_Suspend( NvOdmDispDeviceHandle hDevice )
{
   //Not a valid case in bootloader
}

static NvU8 s_MipiDsiConfig[] = {0x43, 0x00, 0x80, 0x00, 0x00};
static NvU8 s_DispCtrl1[] = {0x34, 0x20, 0x40, 0x00, 0x20};
static NvU8 s_DispCtrl2[] = {0x04, 0x74, 0x0f, 0x16, 0x13};
static NvU8 s_IntOsc[] = {0x01, 0x08};
static NvU8 s_PwrCtrl1[] = {0x00};
static NvU8 s_PwrCtrl3[] = {0x00, 0x09, 0x10, 0x02, 0x00, 0x66, 0x00, 0x13, 0x00};
static NvU8 s_PwrCtrl4[] = {0x23, 0x24, 0x12, 0x12, 0x60};
static NvU8 s_PGammaCurveRed[] = {0x21, 0x25, 0x67, 0x36, 0x0a, 0x06, 0x61, 0x23, 0x03};
static NvU8 s_NGammaCurveRed[] = {0x31, 0x25, 0x66, 0x36, 0x05, 0x06, 0x61, 0x23, 0x03};
static NvU8 s_PGammaCurveGreen[] = {0x41, 0x26, 0x56, 0x36, 0x0a, 0x06, 0x61, 0x23, 0x03};
static NvU8 s_NGammaCurveGreen[] = {0x51, 0x26, 0x55, 0x36, 0x05, 0x06, 0x61, 0x23, 0x03};
static NvU8 s_PGammaCurveBlue[] = {0x41, 0x26, 0x56, 0x36, 0x0a, 0x06, 0x61, 0x23, 0x03};
static NvU8 s_NGammaCurveBlue[] = {0x51, 0x26, 0x55, 0x36, 0x05, 0x06, 0x61, 0x23, 0x03};
static NvU8 s_ScanDirection[] = {0x0B};
static NvU8 s_Otp20[] = {0x00};

/* Sleep out and display on commands */
static NvU8 s_PwrCtrl20[] = {0x02};
static NvU8 s_PwrCtrl21[] = {0x06};
static NvU8 s_PwrCtrl22[] = {0x4e};
/* Send Slpout --- No data here ----*/
static NvU8 s_Otp21[] = {0x80};
/* Send Disp On -- No data here-----*/
static NvU8 s_NUll[] = {0x00};


static NvBool
lgpanel_PanelInit( NvOdmDispDeviceHandle hDevice, NvOdmDispDeviceMode *mode)
{
    NvU32 seqSize;
    NvU32 i;
    NvBool b;
    static DsiPanelData s_PanelInitSeq[] = {
        //DsiCmd, PanelReg, DataSize, PData, IsLongPacket
        { 0x00, -1,  20, NULL, NV_FALSE},           /* wait 20ms */
        {0x39, 0xE0,  5, s_MipiDsiConfig, NV_TRUE},
        {0x39, 0xB5,  5, s_DispCtrl1, NV_TRUE},
        {0x39, 0xB6,  5, s_DispCtrl2, NV_TRUE},
        {0x39, 0xC0,  2, s_IntOsc, NV_TRUE},
        {0x23, 0xC1,  1, s_PwrCtrl1, NV_FALSE},
        {0x39, 0xC3,  9, s_PwrCtrl3, NV_TRUE},
        {0x39, 0xC4,  5, s_PwrCtrl4, NV_TRUE},
        {0x39, 0xD0,  9, s_PGammaCurveRed, NV_TRUE},
        {0x39, 0xD1,  9, s_NGammaCurveRed, NV_TRUE},
        {0x39, 0xD2,  9, s_PGammaCurveGreen, NV_TRUE},
        {0x39, 0xD3,  9, s_NGammaCurveGreen, NV_TRUE},
        {0x39, 0xD4,  9, s_PGammaCurveBlue, NV_TRUE},
        {0x39, 0xD5,  9, s_NGammaCurveBlue, NV_TRUE},
        {0x23, 0x36,  1, s_ScanDirection, NV_FALSE},
        {0x23, 0xF9,  1, s_Otp20, NV_FALSE},

        /* Sleep out and display on commands */
        {0x23, 0xC2,  1, s_PwrCtrl20, NV_FALSE},
        { 0x00, -1,  20, NULL, NV_FALSE},           /* wait 20ms */
        {0x23, 0xC2,  1, s_PwrCtrl21, NV_FALSE},
        { 0x00, -1,  20, NULL, NV_FALSE},           /* wait 20ms */
        {0x23, 0xC2,  1, s_PwrCtrl22, NV_FALSE},
        { 0x00, -1,  100, NULL, NV_FALSE},           /* wait > 90ms */
        {0x05, 0x11,  1, s_NUll, NV_FALSE},
        { 0x00, -1,  140, NULL, NV_FALSE},           /* wait > 130ms */
        {0x23, 0xf9,  1, s_Otp21, NV_FALSE},
        { 0x00, -1,  20, NULL, NV_FALSE},           /* wait > 20ms */
        {0x05, 0x29,  1, s_NUll, NV_FALSE},
    };


    NV_ASSERT( mode->width && mode->height && mode->bpp );
    if (!hDevice || !mode)
        return NV_FALSE;

    if( !hDevice->bInitialized )
    {
        NV_ASSERT( !"device not initialized" );
        return NV_FALSE;
    }

    if (!s_hGpio)
    {
        s_hGpio = NvOdmGpioOpen();
        if (!s_hGpio)
        {
            NV_ASSERT(!"GPIO open failed \n");
            return NV_FALSE;
        }
    }

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

    if (!s_Lcd_rst)
    {
        s_Lcd_rst= NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('h' - 'a') , 3);
        if (!s_Lcd_rst)
        {
            NV_ASSERT(!"GPIO pin acquisition failed \n");
            return NV_FALSE;
        }
    }

    NvOdmOsSleepMS(1);
    NvOdmGpioSetState(s_hGpio, s_Lcd_rst, 0x1);
    NvOdmOsSleepMS(20);

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
            DSI_PANEL_DEBUG(("Error> DSI Panel Initialization Failed\r\n"));
            return NV_FALSE;
        }
    }

    s_Lcd_bl_en = NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('h' - 'a') , 2);
    if (!s_Lcd_bl_en)
    {
        NV_ASSERT(!"GPIO pin acquisition failed \n");
        return NV_FALSE;
    }
    NvOdmGpioSetState(s_hGpio, s_Lcd_bl_en, 1);
    NvOdmGpioConfig(s_hGpio, s_Lcd_bl_en, NvOdmGpioPinMode_Output);

    return NV_TRUE;
}

void
lgpanel_GetHal(NvOdmDispDeviceHandle hDevice)
{
    NV_ASSERT(hDevice);
    if (!hDevice)
        return ;

    hDevice->Setup = lgpanel_Setup;
    hDevice->Release = lgpanel_Release;
    hDevice->ListModes = lgpanel_ListModes;
    hDevice->SetMode = lgpanel_SetMode;
    hDevice->SetPowerLevel = lgpanel_SetPowerLevel;
    hDevice->GetParameter = lgpanel_GetParameter;
    hDevice->GetPinPolarities = lgpanel_GetPinPolarities;
    hDevice->GetConfiguration = lgpanel_GetConfiguration;
    hDevice->SetSpecialFunction = lgpanel_SetSpecialFunction;
    hDevice->SetBacklight = lgpanel_SetBacklight;
    hDevice->BacklightIntensity = lgpanel_SetBacklightIntensity;
}
