/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_disp.h"
#include "panel_panasonic.h"
#include "nvodm_services.h"
#include "nvodm_backlight.h"

//set to 0 for 1st DSI instance and 1 for 2nd DSI instance
#define DSI_INSTANCE 0
#define TEGRA_DC_OUT_CONTINUOUS_MODE 0
#define TEGRA_DC_OUT_ONESHOT_MODE 1

#define DC_CTRL_MODE    TEGRA_DC_OUT_CONTINUOUS_MODE

typedef struct DsiPanelDataRec
{
    NvU32 DsiCmd;
    NvU32 PanelReg;
    NvU32 DataSize;
    NvU8 *pData;
    NvBool IsLongPacket;
} DsiPanelData;

static NvBool panasonicDsi_PanelInit( NvOdmDispDeviceHandle hDevice, NvOdmDispDeviceMode *mode);

// Color calibration
static NvU32 s_ColorCalibration_Red     = NVODM_DISP_FP_TO_FX(1.00);
static NvU32 s_ColorCalibration_Green   = NVODM_DISP_FP_TO_FX(1.00);
static NvU32 s_ColorCalibration_Blue    = NVODM_DISP_FP_TO_FX(1.00);

// Other global variables.
static NvOdmDispDsiTransport   s_trans;

static NvOdmServicesGpioHandle s_hGpio =  NULL;
static NvOdmGpioPinHandle s_Lcd_bl_en = NULL;
static NvOdmGpioPinHandle s_Lcd_rst = NULL;
static NvOdmGpioPinHandle s_En_Vdd_bl = NULL;

static NvOdmServicesPwmHandle s_hOdmPwm = NULL;

static NvBool s_bBacklight = NV_FALSE;


static NvOdmDispDeviceMode panasonic_modes[] =
{
    { 1920, // width
      1200, // height
      24, // bpp(color depth)
      0, // refresh
      0, // frequency
      0, // flags
      // timing
      { 2, // h_ref_to_sync
        2, // v_ref_to_sync
        16, // h_sync_width
        2, // v_sync_width
        32, // h_back_porch
        16, // v_back_porch
        1920, // h_disp_active
        1200, // v_disp_active
        112, // h_front_porch
        17, // v_front_porch
      },
      NV_FALSE // partial
    },
};

static void
panasonic_nullmode( NvOdmDispDeviceHandle hDevice )
{
    if (!hDevice)
        return ;
    hDevice->CurrentMode.width = 0;
    hDevice->CurrentMode.height = 0;
    hDevice->CurrentMode.bpp = 0;
}

static NvBool panasonic_discover (NvOdmDispDeviceHandle hDevice)
{
    NvU64 guid;
    NvOdmPeripheralConnectivity const *conn;

    /* get the main panel */
    guid = PANASONIC_PANEL_GUID;

    /* get the connectivity info */
    conn = NvOdmPeripheralGetGuid(guid);
    if( !conn )
    {
        return NV_FALSE;
    }

    hDevice->PeripheralConnectivity = conn;
    return NV_TRUE;
}

NvBool panasonic_Setup( NvOdmDispDeviceHandle hDevice )
{
    NvOdmPeripheralConnectivity const *pCon;
    NvOdmServicesPmuVddRailCapabilities RailCaps;
    NvU32 SettlingTime = 0;
    NvU32 i;
    NvOdmServicesPmuHandle hPmu;
    /* get the peripheral config */
    if( !panasonic_discover(hDevice))
    {
        return NV_FALSE;
    }

    if (!hDevice)
        return NV_FALSE;

    if( !hDevice->bInitialized )
    {
        hDevice->Type = NvOdmDispDeviceType_DSI;
        hDevice->Usage = NvOdmDispDeviceUsage_Primary;
        hDevice->MaxHorizontalResolution = panasonic_modes[0].width;
        hDevice->MaxVerticalResolution = panasonic_modes[0].height;
        hDevice->BaseColorSize = NvOdmDispBaseColorSize_888;
        hDevice->DataAlignment = NvOdmDispDataAlignment_MSB;
        hDevice->PinMap = NvOdmDispPinMap_Single_Rgb24_Spi5;
        hDevice->PwmOutputPin = NvOdmDispPwmOutputPin_Unspecified;
        hDevice->modes = panasonic_modes;
        hDevice->nModes = NV_ARRAY_SIZE(panasonic_modes);
        hDevice->power = NvOdmDispDevicePowerLevel_Off;
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

        panasonic_nullmode( hDevice );

        NvOdmServicesPmuClose(hPmu);

        // Get PWM handle for backlight intensity
        s_hOdmPwm = NvOdmPwmOpen();
        NV_ASSERT(s_hOdmPwm);

        hDevice->bInitialized = NV_TRUE;
    }

    return NV_TRUE;
}

void panasonic_Release( NvOdmDispDeviceHandle hDevice )
{
    if (!hDevice)
        return ;

    NvOdmBacklightIntensity( hDevice, 0 );
    NvOdmBacklightDeinit(hDevice);
    hDevice->bInitialized = NV_FALSE;

    if (s_Lcd_bl_en)
    {
        NvOdmGpioReleasePinHandle( s_hGpio, s_Lcd_bl_en);
        s_Lcd_bl_en = NULL;
    }

    if (s_En_Vdd_bl)
    {
        NvOdmGpioReleasePinHandle( s_hGpio, s_En_Vdd_bl);
        s_En_Vdd_bl = NULL;
    }

    if (s_Lcd_rst)
    {
        NvOdmGpioReleasePinHandle( s_hGpio, s_Lcd_rst);
        s_Lcd_rst = NULL;
    }

    if (s_hGpio)
    {
        NvOdmGpioClose( s_hGpio );
        s_hGpio = NULL;
    }

    s_bBacklight = NV_FALSE;
    NvOdmPwmClose(s_hOdmPwm);
    s_hOdmPwm = NULL;

    //Remove Initialised variables etc
    panasonic_nullmode( hDevice );
}

void panasonic_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
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

NvBool panasonic_SetMode( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDeviceMode *mode, NvU32 flags )
{
    NV_ASSERT(hDevice);
    if (!hDevice)
        return NV_FALSE;

    if (mode)
    {
        hDevice->CurrentMode = *mode;
    }
    else
    {
        panasonic_nullmode(hDevice);
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
        panasonicDsi_PanelInit(hDevice, mode);
    }
    else
    {
        panasonic_nullmode(hDevice);
    }
    return NV_TRUE;
}

void panasonic_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    if( !s_bBacklight )
    {
        NvOdmBacklightInit( hDevice, NV_FALSE );
        s_bBacklight = NV_TRUE;
    }

    NvOdmBacklightIntensity( hDevice, intensity );
}

void panasonic_SetBacklightIntensity( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
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

    RequestedFreq = 200; // Hz

    // Upper 16 bits of DutyCycle is intensity percentage (whole number portion)
    // Intensity must be scaled from 0 - 255 to a percentage.
    DutyCycle = ((intensity * 100)/255) << 16;

    if (intensity != 0)
    {
        NvOdmGpioSetState(s_hGpio, s_Lcd_bl_en, 0x1);

        NvOdmPwmConfig(s_hOdmPwm, NvOdmPwmOutputId_PWM0 + PwmPin,
                       NvOdmPwmMode_Enable, DutyCycle, &RequestedFreq,
                       &ReturnedFreq);
    }
    else
    {
        NvOdmGpioSetState(s_hGpio, s_Lcd_bl_en, 0x0);
        NvOdmPwmConfig(s_hOdmPwm, NvOdmPwmOutputId_PWM0 + PwmPin,
                       NvOdmPwmMode_Enable, 0, &RequestedFreq,
                       &ReturnedFreq);
    }
}

static void
panasonicDsi_Resume( NvOdmDispDeviceHandle hDevice )
{
    panasonicDsi_PanelInit( hDevice, &hDevice->CurrentMode );
}

void panasonic_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level )
{
    NV_ASSERT(hDevice);
    if (!hDevice)
        return ;

    hDevice->power = level;

    switch(level)
    {
        case NvOdmDispDevicePowerLevel_Off:
        case NvOdmDispDevicePowerLevel_Suspend:
        //Not required in BL
            break;
        case NvOdmDispDevicePowerLevel_On:
            if( hDevice->CurrentMode.width && hDevice->CurrentMode.height &&
                hDevice->CurrentMode.bpp )
        {
            panasonicDsi_Resume( hDevice );
        }
            break;
        default:
            break;
    }
}

NvBool panasonic_GetParameter( NvOdmDispDeviceHandle hDevice,
    NvOdmDispParameter param, NvU32 *val )
{
    NV_ASSERT(hDevice);
    NV_ASSERT(val);
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
        case NvOdmDispParameter_DsiInstance:
            *val = DSI_INSTANCE;
            break;
        case NvOdmDispParameter_PwmOutputPin:
            *val = hDevice->PwmOutputPin;
            break;
        default:
            return NV_FALSE;
    }
    return NV_TRUE;
}

void* panasonic_GetConfiguration( NvOdmDispDeviceHandle hDevice )
{
    NvOdmDispDsiConfig *dsi;
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
    dsi->PanelResetTimeoutMSec = 202;
    dsi->LpCommandModeFreqKHz = 4165;
    dsi->HsCommandModeFreqKHz = 20250;
    dsi->HsSupportForFrameBuffer = NV_FALSE;
    dsi->flags = 0;
    dsi->HsClockControl = NvOdmDsiHsClock_TxOnly;
    dsi->EnableHsClockInLpMode = NV_FALSE;
    dsi->bIsPhytimingsValid = NV_FALSE;
    dsi->DsiDisplayMode = NvOdmDispDsiMode_Normal;
    return (void *)dsi;
}

void panasonic_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values )
{
    *nPins = 0;
}

NvBool panasonic_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config )
{
     switch( function )
    {
    case NvOdmDispSpecialFunction_PartialMode:
        // FIXME: Does this support partial mode
        return NV_FALSE;
    case NvOdmDispSpecialFunction_DSI_Transport:
        s_trans = *(NvOdmDispDsiTransport *)config;
        break;
    default:
        return NV_FALSE;
    }
    return NV_TRUE;
}

void panasonic_GetHal( NvOdmDispDeviceHandle hDevice )
{
    NV_ASSERT(hDevice);
    if (!hDevice)
        return ;

    hDevice->Setup = panasonic_Setup;
    hDevice->Release = panasonic_Release;
    hDevice->ListModes = panasonic_ListModes;
    hDevice->SetMode = panasonic_SetMode;
    hDevice->SetPowerLevel = panasonic_SetPowerLevel;
    hDevice->GetParameter = panasonic_GetParameter;
    hDevice->GetPinPolarities = panasonic_GetPinPolarities;
    hDevice->GetConfiguration = panasonic_GetConfiguration;
    hDevice->SetSpecialFunction = panasonic_SetSpecialFunction;
    // Backlight functions
    hDevice->SetBacklight = panasonic_SetBacklight;
    hDevice->BacklightIntensity = panasonic_SetBacklightIntensity;
}

static NvBool panasonicDsi_PanelInit( NvOdmDispDeviceHandle hDevice,
                          NvOdmDispDeviceMode *mode)
{
    NvBool b;
    if (!hDevice)
        return NV_FALSE;
    NV_ASSERT( mode->width && mode->height && mode->bpp );

    if (!hDevice->bInitialized)
    {
        NV_ASSERT(!"device not initialized");
        return NV_FALSE;
    }

    s_hGpio = NvOdmGpioOpen();
    if (!s_hGpio)
    {
        NV_ASSERT(!"GPIO open failed \n");
        return NV_FALSE;
    }

    s_En_Vdd_bl = NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('g' - 'a') , 0);
    if (!s_En_Vdd_bl)
    {
        NV_ASSERT(!"GPIO pin acquisition failed \n");
        return NV_FALSE;
    }
    NvOdmGpioSetState(s_hGpio, s_En_Vdd_bl, 1);
    NvOdmGpioConfig(s_hGpio, s_En_Vdd_bl, NvOdmGpioPinMode_Output);

    s_Lcd_bl_en = NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('h' - 'a') , 2);
    if (!s_Lcd_bl_en)
    {
        NV_ASSERT(!"GPIO pin acquisition failed \n");
        return NV_FALSE;
    }
    NvOdmGpioSetState(s_hGpio, s_Lcd_bl_en, 1);
    NvOdmGpioConfig(s_hGpio, s_Lcd_bl_en, NvOdmGpioPinMode_Output);

    s_Lcd_rst= NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('h' - 'a') , 3);
    if (!s_Lcd_rst)
    {
        NV_ASSERT(!"GPIO pin acquisition failed \n");
        return NV_FALSE;
    }
    NvOdmGpioSetState(s_hGpio, s_Lcd_rst, 1);
    NvOdmGpioConfig(s_hGpio, s_Lcd_rst, NvOdmGpioPinMode_Output);
    NvOdmOsWaitUS(100);
    NvOdmGpioSetState(s_hGpio, s_Lcd_rst, 0x0);
    NvOdmOsWaitUS(100);
    NvOdmGpioSetState(s_hGpio, s_Lcd_rst, 0x1);

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
    //No Panel Init sequence required for Panasonic panel
    return NV_TRUE;
}
