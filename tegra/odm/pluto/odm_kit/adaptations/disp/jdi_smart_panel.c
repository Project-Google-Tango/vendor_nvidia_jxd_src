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
#include "jdi_smart_panel.h"
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
#define DSI_INSTANCE 1     //E1605 is hardcoded to DSI B

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

static NvBool jdipanel_Setup( NvOdmDispDeviceHandle hDevice );
static void jdipanel_Release( NvOdmDispDeviceHandle hDevice );
static void jdipanel_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
                NvOdmDispDeviceMode *modes );
static NvBool jdipanel_SetMode( NvOdmDispDeviceHandle hDevice,
                NvOdmDispDeviceMode *mode, NvU32 flags );
static void jdipanel_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity );
static void jdipanel_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
                NvOdmDispDevicePowerLevel level );
static NvBool jdipanel_GetParameter( NvOdmDispDeviceHandle hDevice,
                NvOdmDispParameter param, NvU32 *val );
static void * jdipanel_GetConfiguration( NvOdmDispDeviceHandle hDevice );
static void jdipanel_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
                NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values );
static NvBool jdipanel_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
                NvOdmDispSpecialFunction function, void *config );
static void jdipanel_NullMode( NvOdmDispDeviceHandle hDevice );
static void jdipanel_Resume( NvOdmDispDeviceHandle hDevice );
static void jdipanel_Suspend( NvOdmDispDeviceHandle hDevice );
static NvBool jdipanel_PanelInit( NvOdmDispDeviceHandle hDevice, NvOdmDispDeviceMode *mode);

// Color calibration
static NvU32 s_ColorCalibration_Red     = NVODM_DISP_FP_TO_FX(1.00);
static NvU32 s_ColorCalibration_Green   = NVODM_DISP_FP_TO_FX(1.00);
static NvU32 s_ColorCalibration_Blue    = NVODM_DISP_FP_TO_FX(1.00);

// Other global variables.
static NvOdmDispDsiTransport   s_trans;
static NvBool s_bBacklight = NV_FALSE;
static NvBool NvOdmDispI2cWrite8(NvOdmServicesI2cHandle hOdmDispI2c,
    NvU32 DevAddr, NvU32 SpeedKHz, NvU8 RegAddr, NvU8 Data);
/********************************************
 * Please use byte clock for the following table
 * Byte Clock to Pixel Clock conversion:
 * The DSI D-PHY calculation:
 * h_total = (h_sync_width + h_back_porch + h_disp_active + h_front_porch)
 * v_total = (v_sync_width + v_back_porch + v_disp_active + v_front_porch)
 * dphy_clk = h_total * v_total * fps * bpp / data_lanes / 2
 ********************************************/
static NvOdmDispDeviceMode jdipanel_Modes[] =
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
            16,   // h_sync_width,
            2,   // v_sync_width,
            32,  // h_back_porch,
            3,   // v_back_porch
            720,   // h_disp_active
            1280,   // v_disp_active
            48,  // h_front_porch,
            3,  // v_front_porch
        },
        NV_FALSE // deprecated.
    }
};

static NvBool jdipanel_discover (NvOdmDispDeviceHandle hDevice)
{
    NvU64 guid;
    NvOdmPeripheralConnectivity const *conn;
    /* get the main panel */
    guid = JDI_PANEL_GUID;
    /* get the connectivity info */
    conn = NvOdmPeripheralGetGuid(guid);
    if( !conn )
    {
        return NV_FALSE;
    }
    hDevice->PeripheralConnectivity = conn;
    return NV_TRUE;
}

static NvBool NvOdmDispI2cWrite8(
    NvOdmServicesI2cHandle hOdmDispI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 Data)
{
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;
    NvU8 WriteBuffer[2];
    NvOdmI2cTransactionInfo TransactionInfo = {0 , 0, 0, 0};
    WriteBuffer[0] = RegAddr & 0xFF;    // PMU offset
    WriteBuffer[1] = Data & 0xFF;    // written data
    TransactionInfo.Address = DevAddr;
    TransactionInfo.Buf = &WriteBuffer[0];
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 2;
    status = NvOdmI2cTransaction(hOdmDispI2c, &TransactionInfo, 1,
        SpeedKHz, 1000);

    if (status == NvOdmI2cStatus_Success)
        return NV_TRUE;
    switch (status)
    {
        case NvOdmI2cStatus_Timeout:
            NvOdmOsPrintf("%s(): Failed: Timeout\n", __func__);
            break;
        case NvOdmI2cStatus_SlaveNotFound:
            NvOdmOsPrintf("%s(): Failed: SlaveNotFound slave Add 0x%x\n",
                __func__, DevAddr);
            break;
        default:
            NvOdmOsPrintf("%s() Failed: status 0x%x\n", __func__, status);
            break;
    }
    return NV_FALSE;
}
static NvBool
jdipanel_Setup( NvOdmDispDeviceHandle hDevice )
{
    NvOdmServicesPmuVddRailCapabilities RailCaps;
    NvU32 SettlingTime = 0;
    NvU32 i;
    NvOdmServicesPmuHandle hPmu;

    if (!hDevice)
        return NV_FALSE;
    /* get the peripheral config */
    if( !jdipanel_discover(hDevice))
        return NV_FALSE;

    if( !hDevice->bInitialized )
    {
        hDevice->modes = jdipanel_Modes;
        hDevice->nModes = NV_ARRAY_SIZE(jdipanel_Modes);

        hDevice->Type = NvOdmDispDeviceType_DSI;
        hDevice->Usage = NvOdmDispDeviceUsage_Primary;
        hDevice->MaxHorizontalResolution = (NvU32)jdipanel_Modes[0].width;
        hDevice->MaxVerticalResolution = (NvU32)jdipanel_Modes[0].height;
        hDevice->BaseColorSize = NvOdmDispBaseColorSize_888;
        hDevice->DataAlignment = NvOdmDispDataAlignment_MSB;
        hDevice->PinMap = NvOdmDispPinMap_Single_Rgb24_Spi5;

        hDevice->power = NvOdmDispDevicePowerLevel_Off;
        jdipanel_NullMode( hDevice );
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
jdipanel_NullMode( NvOdmDispDeviceHandle hDevice )
{
    if (!hDevice)
        return ;

    hDevice->CurrentMode.width = 0;
    hDevice->CurrentMode.height = 0;
    hDevice->CurrentMode.bpp = 0;
    hDevice->CurrentMode.flags = (NvU32)-1;
}

static void
jdipanel_Release( NvOdmDispDeviceHandle hDevice )
{
    if (!hDevice)
        return ;

    hDevice->bInitialized = NV_FALSE;
    jdipanel_NullMode( hDevice );
}

static void
jdipanel_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
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
jdipanel_SetMode( NvOdmDispDeviceHandle hDevice,
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
        jdipanel_NullMode( hDevice );
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
        jdipanel_PanelInit(hDevice, mode);
    }
    else
    {
        jdipanel_NullMode(hDevice);
    }
    return NV_TRUE;
}


static void
jdipanel_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    if( !s_bBacklight )
    {
        NvOdmBacklightInit( hDevice, NV_FALSE );
        s_bBacklight = NV_TRUE;
    }

    NvOdmBacklightIntensity( hDevice, intensity );

}


static void
jdipanel_SetBacklightIntensity( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    //To Do: Add PWM support

}


static void
jdipanel_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level )
{
    NV_ASSERT( hDevice );
    if (!hDevice)
        return ;

    hDevice->power = level;

    switch( level ) {
    case NvOdmDispDevicePowerLevel_Off:
    case NvOdmDispDevicePowerLevel_Suspend:
        jdipanel_Suspend( hDevice );
        break;
    case NvOdmDispDevicePowerLevel_On:
        if( hDevice->CurrentMode.width && hDevice->CurrentMode.height &&
            hDevice->CurrentMode.bpp )
        {
            jdipanel_Resume( hDevice );
        }
        break;
    default:
        break;
    }
}

static NvBool
jdipanel_GetParameter( NvOdmDispDeviceHandle hDevice,
    NvOdmDispParameter param, NvU32 *val )
{
    NV_ASSERT( hDevice );
    NV_ASSERT( val );
    if (!hDevice ||!val)
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
jdipanel_GetConfiguration( NvOdmDispDeviceHandle hDevice )
{
    NvOdmDispDsiConfig *dsi = NULL;
    if (!hDevice)
        return NULL;

    dsi = &hDevice->Config.dsi;
    dsi->DataFormat = NvOdmDsiDataFormat_24BitP;
    dsi->VideoMode = NvOdmDsiVideoModeType_DcDrivenCommandMode;
    dsi->VirChannel = NvOdmDsiVirtualChannel_0;
    dsi->NumOfDataLines = 3;
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
    dsi->HsClockControl = NvOdmDsiHsClock_TxOnly;
    dsi->EnableHsClockInLpMode = NV_FALSE;
    dsi->bIsPhytimingsValid = NV_FALSE;
    dsi->DsiDisplayMode = NvOdmDispDsiMode_Normal;
    return (void *)dsi;
}

static void
jdipanel_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values )
{
    *nPins = 0;
}

static NvBool
jdipanel_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
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
jdipanel_Resume( NvOdmDispDeviceHandle hDevice )
{
    jdipanel_PanelInit( hDevice, &hDevice->CurrentMode );
}

static void
jdipanel_Suspend( NvOdmDispDeviceHandle hDevice )
{
    //Not a valid case in BL
}

/* GPIO Handles */
static NvOdmServicesGpioHandle s_hGpio;
static NvOdmGpioPinHandle s_Lcd_bl_pwm;
static NvOdmGpioPinHandle s_Lcd_bl_en;
static NvOdmGpioPinHandle s_Lcd_rst;
static NvOdmGpioPinHandle s_Lcd_te;

/* DSI parameters for initialisation */
static NvU8 s_CmdPageSelect[] = {0xEE};
static NvU8 s_NovatekFactoryReset[] = {0x08};
static NvU8 s_SetMipiLane[] = {0x02};
static NvU8 s_DsiCmdMode[] = {0x08};
static NvU8 s_CmdPageSelect1[] = {0x04};
static NvU8 s_ReloadCmd2Page3[] = {0x01};
static NvU8 s_IncreaseVddLevel[] = {0x53};
static NvU8 s_IncreaseVddLevel1[] = {0x05};
static NvU8 s_EnlargeInternalTiming[] = {0x60};
static NvU8 s_ReloadNFactoryCmg[] = {0x01};
static NvU8 s_UnknownyCmg[] = {0x77};
static NvU8 s_ZeroCmd[] = { 0x00 };

static NvBool
jdipanel_PanelInit( NvOdmDispDeviceHandle hDevice, NvOdmDispDeviceMode *mode)
{
    NvU32 seqSize;
    NvU32 i;
    NvBool b;
    NvOdmServicesI2cHandle hOdmI2c = NULL;

    static DsiPanelData s_PanelInitSeqa[] = {
        { 0x15,      0xFF,   1,  s_CmdPageSelect, NV_FALSE},
        { 0x15,      0x26,   1,  s_NovatekFactoryReset, NV_FALSE},
        { 0x00, -1,  10, NULL, NV_FALSE},
        { 0x15,      0x26,   1, s_ZeroCmd, NV_FALSE},
        { 0x15,      0xFF,   1, s_ZeroCmd, NV_FALSE},
        { 0x00, -1,  15, NULL, NV_FALSE},
    };
    static DsiPanelData s_PanelInitSeq[] = {
          //DsiCmd, PanelReg, DataSize, PData, IsLongPacket
        { 0x15,      0xBA,   1,  s_SetMipiLane, NV_FALSE},
        { 0x00, -1,  5, NULL, NV_FALSE},
        { 0x15,      0xc2,   1, s_DsiCmdMode, NV_FALSE},
        { 0x00, -1,  5, NULL, NV_FALSE},
        { 0x15,      0xFF,   1, s_CmdPageSelect1, NV_FALSE},
        { 0x00, -1,  5, NULL, NV_FALSE},
        { 0x15,      0x09,   1, s_ZeroCmd, NV_FALSE},
        { 0x00, -1,  5, NULL, NV_FALSE},
        { 0x15,      0x0a,   1, s_ZeroCmd, NV_FALSE},
        { 0x00, -1,  5, NULL, NV_FALSE},
        { 0x15,      0xfb,   1, s_ReloadCmd2Page3, NV_FALSE},
        { 0x00, -1,  5, NULL, NV_FALSE},
        { 0x15,      0xff,   1, s_CmdPageSelect, NV_FALSE},
        { 0x00, -1,  5, NULL, NV_FALSE},
        { 0x15,      0x12,   1, s_IncreaseVddLevel, NV_FALSE},
        { 0x00, -1,  5, NULL, NV_FALSE},
        { 0x15,      0x13,   1, s_IncreaseVddLevel1, NV_FALSE},
        { 0x00, -1,  5, NULL, NV_FALSE},
        { 0x15,      0x6a,   1, s_EnlargeInternalTiming, NV_FALSE},
        { 0x00, -1,  5, NULL, NV_FALSE},
        { 0x15,      0xfb,   1,  s_ReloadNFactoryCmg, NV_FALSE},
        { 0x00, -1,  5, NULL, NV_FALSE},
        { 0x15,      0xFF,   1, s_ZeroCmd, NV_FALSE},
        { 0x00, -1,  5, NULL, NV_FALSE},
        { 0x15,      0x3a,   1, s_UnknownyCmg, NV_FALSE},
        { 0x00, -1,  5, NULL, NV_FALSE},
        { 0x05,      0x11,   1, s_ZeroCmd, NV_FALSE},
        { 0x00, -1,  2000, NULL, NV_FALSE},
#if (DC_CTRL_MODE == TEGRA_DC_OUT_ONESHOT_MODE)
        { 0x15,      0x35,   1, s_ZeroCmd, NV_FALSE},
#endif
        { 0x00, -1,  5, NULL, NV_FALSE},
        { 0x05,      0x29,   1, s_ZeroCmd, NV_FALSE},
        { 0x00, -1,  150, NULL, NV_FALSE},
    };


    NV_ASSERT( mode->width && mode->height && mode->bpp );
    if (!hDevice || !mode)
        return NV_FALSE;


    if( !hDevice->bInitialized )
    {
        NV_ASSERT( !"device not initialized" );
        return NV_FALSE;
    }

    s_hGpio = NvOdmGpioOpen();
    if( !s_hGpio )
    {
        NV_ASSERT(!"Gpio open failed");
        return NV_FALSE;
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

    seqSize = NV_ARRAY_SIZE(s_PanelInitSeqa);
    for ( i =0; i<seqSize; i++)
    {
        if (s_PanelInitSeqa[i].pData == NULL)
        {
            NvOdmOsSleepMS(s_PanelInitSeqa[i].DataSize);
            continue;
        }
        //Delay might be required between commands
        b = s_trans.NvOdmDispDsiWrite( hDevice,
                  s_PanelInitSeqa[i].DsiCmd,
                  s_PanelInitSeqa[i].PanelReg,
                  s_PanelInitSeqa[i].DataSize,
                  s_PanelInitSeqa[i].pData,
                  DSI_INSTANCE,
                  s_PanelInitSeqa[i].IsLongPacket );
        if (!b)
        {
            DSI_PANEL_DEBUG(("Error> DSI Panel Initialization Failed\r\n"));
            return NV_FALSE;
        }
    }

    s_Lcd_bl_en = NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('h' - 'a'), 2);
    if (!s_Lcd_bl_en)
    {
        NV_ASSERT( !"Pin handle could not be acquired");
        return NV_FALSE;
    }
    /* On DSIa backlight */
    NvOdmGpioSetState(s_hGpio, s_Lcd_bl_en, 0x1);
    NvOdmGpioConfig( s_hGpio, s_Lcd_bl_en, NvOdmGpioPinMode_Output);

    s_Lcd_bl_pwm= NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('h' - 'a') , 1);
    if (!s_Lcd_bl_pwm)
    {
        NV_ASSERT(!"GPIO pin acquisition failed \n");
        return NV_FALSE;
    }
    NvOdmGpioSetState(s_hGpio, s_Lcd_bl_pwm, 1);
    NvOdmGpioConfig(s_hGpio, s_Lcd_bl_pwm, NvOdmGpioPinMode_Output);

    // Setup PWM pin in "function mode" (used for backlight intensity)
    s_Lcd_te = NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('r' - 'a'), 6);
    if (!s_Lcd_te)
    {
        NV_ASSERT(!"GPIO pin acquisition failed \n");
        return NV_FALSE;
    }
    NvOdmGpioConfig(s_hGpio, s_Lcd_te, NvOdmGpioPinMode_Function);

    hOdmI2c = NvOdmI2cOpen(NvOdmIoModule_I2c, 1);

    NvOdmDispI2cWrite8(hOdmI2c, 0x9a, 100, 0x00, 0x03);
    NvOdmDispI2cWrite8(hOdmI2c, 0x9a, 100, 0x0b, 0x7f);
    NvOdmDispI2cWrite8(hOdmI2c, 0x9a, 100, 0x0c, 0x7f);

    s_Lcd_rst= NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('h' - 'a') , 5);
    if (!s_Lcd_rst)
    {
        NV_ASSERT(!"GPIO pin acquisition failed \n");
        return NV_FALSE;
    }
    NvOdmGpioSetState(s_hGpio, s_Lcd_rst, 1);
    NvOdmGpioConfig(s_hGpio, s_Lcd_rst, NvOdmGpioPinMode_Output);
    NvOdmOsWaitUS(10);
    NvOdmGpioSetState(s_hGpio, s_Lcd_rst, 0x0);
    NvOdmOsWaitUS(20);
    NvOdmGpioSetState(s_hGpio, s_Lcd_rst, 0x1);
    NvOdmOsWaitUS(200);

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
        if (!b)
        {
            DSI_PANEL_DEBUG(("Error> DSI Panel Initialization Failed\r\n"));
            return NV_FALSE;
        }
    }
    return NV_TRUE;
}

void
jdipanel_GetHal(NvOdmDispDeviceHandle hDevice)
{
    NV_ASSERT(hDevice);
    if (!hDevice)
        return;

    hDevice->Setup = jdipanel_Setup;
    hDevice->Release = jdipanel_Release;
    hDevice->ListModes = jdipanel_ListModes;
    hDevice->SetMode = jdipanel_SetMode;
    hDevice->SetPowerLevel = jdipanel_SetPowerLevel;
    hDevice->GetParameter = jdipanel_GetParameter;
    hDevice->GetPinPolarities = jdipanel_GetPinPolarities;
    hDevice->GetConfiguration = jdipanel_GetConfiguration;
    hDevice->SetSpecialFunction = jdipanel_SetSpecialFunction;
    hDevice->SetBacklight = jdipanel_SetBacklight;
    hDevice->BacklightIntensity = jdipanel_SetBacklightIntensity;
}

