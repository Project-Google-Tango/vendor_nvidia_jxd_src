/*
 * Copyright (c) 2013, NVIDIA Corporation.  All rights reserved.
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
#include "nvodm_backlight.h"
#include "nvos.h"

#if defined(DISP_OAL)
#include "nvbl_debug.h"
#else
#define DSI_PANEL_DEBUG(x) NvOsDebugPrintf x
#endif
#define DSI_PANEL_DEBUG(x) NvOsDebugPrintf x

//set to 0 for 1st DSI instance and 1 for 2nd DSI instance
#define DSI_INSTANCE 0
#define TEGRA_DC_OUT_CONTINUOUS_MODE 0
#define TEGRA_DC_OUT_ONESHOT_MODE 1

#define DC_CTRL_MODE    TEGRA_DC_OUT_CONTINUOUS_MODE
#define DELAY_MS 20

typedef struct DsiPanelDataRec
{
    NvU32 DsiCmd;
    NvU32 PanelReg;
    NvU32 DataSize;
    NvU8 *pData;
    NvBool IsLongPacket;
} DsiPanelData;

static NvBool sharpDsi_PanelInit( NvOdmDispDeviceHandle hDevice, NvOdmDispDeviceMode *mode);

// Color calibration
static NvU32 s_ColorCalibration_Red     = NVODM_DISP_FP_TO_FX(1.00);
static NvU32 s_ColorCalibration_Green   = NVODM_DISP_FP_TO_FX(1.00);
static NvU32 s_ColorCalibration_Blue    = NVODM_DISP_FP_TO_FX(1.00);

static NvU8 s_ZeroCmd[] = { 0x00 };
static NvOdmServicesGpioHandle s_hGpio =  NULL;
static NvOdmGpioPinHandle s_Lcd_bl_en = NULL;
static NvOdmGpioPinHandle s_Lcd_bl_pwm = NULL;
static NvOdmGpioPinHandle s_Lcd_rst = NULL;

static NvBool s_bBacklight = NV_FALSE;


// Other global variables.
static NvOdmDispDsiTransport   s_trans;

static NvOdmDispDeviceMode sharpDsi_modes[] =
{
    { 2560, // width
      1600, // height
      24, // bpp(color depth)
      0, // refresh
      0, // frequency
      0, // flags
      // timing
      { 4, // h_ref_to_sync
        1, // v_ref_to_sync
        32, // h_sync_width
        6, // v_sync_width
        80, // h_back_porch
        37, // v_back_porch
        2560, // h_disp_active
        1600, // v_disp_active
        48, // h_front_porch
        3, // v_front_porch
      },
      NV_FALSE // partial
    },
};

static void
sharpDsi_nullmode( NvOdmDispDeviceHandle hDevice )
{
    if (!hDevice)
        return ;
    hDevice->CurrentMode.width = 0;
    hDevice->CurrentMode.height = 0;
    hDevice->CurrentMode.bpp = 0;
}

static NvBool sharpDsi_discover (NvOdmDispDeviceHandle hDevice)
{
    NvU64 guid;
    NvOdmPeripheralConnectivity const *conn;
    NvOdmBoardInfo PmuBoard;

    if (!NvOdmGetPmuBoardId(&PmuBoard))
    {
        NvOdmOsPrintf("This Pmu Module is not present.\n");
    }

    /* get the main panel */
    if (PmuBoard.BoardID == PMU_E1735)
        guid = SHARP_TPS65914_2516_PANEL_GUID;
    else
        guid = SHARP_2516_PANEL_GUID;

    /* get the connectivity info */
    conn = NvOdmPeripheralGetGuid(guid);
    if( !conn )
    {
        return NV_FALSE;
    }

    hDevice->PeripheralConnectivity = conn;
    return NV_TRUE;
}

NvBool sharpDsi_Setup( NvOdmDispDeviceHandle hDevice )
{
    NvOdmServicesPmuVddRailCapabilities RailCaps;
    NvU32 SettlingTime = 0;
    NvU32 i;
    NvOdmServicesPmuHandle hPmu;
    /* get the peripheral config */
    if( !sharpDsi_discover(hDevice))
    {
        return NV_FALSE;
    }

    if (!hDevice)
        return NV_FALSE;

    if( !hDevice->bInitialized )
    {
        hDevice->Type = NvOdmDispDeviceType_DSI;
        hDevice->Usage = NvOdmDispDeviceUsage_Primary;
        hDevice->MaxHorizontalResolution = sharpDsi_modes[0].width;
        hDevice->MaxVerticalResolution = sharpDsi_modes[0].height;
        hDevice->BaseColorSize = NvOdmDispBaseColorSize_888;
        hDevice->DataAlignment = NvOdmDispDataAlignment_MSB;
        hDevice->PinMap = NvOdmDispPinMap_Single_Rgb24_Spi5;
        hDevice->PwmOutputPin = NvOdmDispPwmOutputPin_Unspecified;
        hDevice->modes = sharpDsi_modes;
        hDevice->nModes = NV_ARRAY_SIZE(sharpDsi_modes);
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
                    hDevice->PeripheralConnectivity->AddressList[i].Address,
                    RailCaps.requestMilliVolts, &SettlingTime );

                /* wait for rail to settle */
                NvOdmOsWaitUS(SettlingTime);
            }
        }
        if (SettlingTime)
        {
            NvOdmOsWaitUS(SettlingTime);
        }

        sharpDsi_nullmode( hDevice );

        hDevice->bInitialized = NV_TRUE;
    }

    return NV_TRUE;
}

void sharpDsi_Release( NvOdmDispDeviceHandle hDevice )
{
    if (!hDevice)
        return ;
    hDevice->bInitialized = NV_FALSE;
    sharpDsi_nullmode( hDevice );
}

void sharpDsi_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
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

NvBool sharpDsi_SetMode( NvOdmDispDeviceHandle hDevice,
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
        sharpDsi_nullmode(hDevice);
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
        sharpDsi_PanelInit(hDevice, mode);
    }
    else
    {
        sharpDsi_nullmode(hDevice);
    }
    return NV_TRUE;
}

void sharpDsi_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    if( !s_bBacklight )
    {
        NvOdmBacklightInit( hDevice, NV_FALSE );
        s_bBacklight = NV_TRUE;
    }
    /* Add delay before initializing backlight to avoid any
       corruption and  give time for panel init to stabilize */
    NvOdmOsSleepMS(DELAY_MS);
    NvOdmBacklightIntensity( hDevice, intensity );
}

void sharpDsi_SetBacklightIntensity( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    NV_ASSERT( hDevice );

    s_Lcd_bl_pwm= NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('h' - 'a') , 1);
    if (!s_Lcd_bl_pwm)
    {
        NV_ASSERT(!"GPIO pin acquisition failed \n");
        return ;
    }
    NvOdmGpioSetState(s_hGpio, s_Lcd_bl_pwm, 1);
    NvOdmGpioConfig(s_hGpio, s_Lcd_bl_pwm, NvOdmGpioPinMode_Output);
}

static void
sharpDsi_Resume( NvOdmDispDeviceHandle hDevice )
{
    sharpDsi_PanelInit( hDevice, &hDevice->CurrentMode );
}

void sharpDsi_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
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

        break;
    case NvOdmDispDevicePowerLevel_On:
        if( hDevice->CurrentMode.width && hDevice->CurrentMode.height &&
            hDevice->CurrentMode.bpp )
        {
            sharpDsi_Resume( hDevice );
        }
        break;
    default:
        break;
    }
}

NvBool sharpDsi_GetParameter( NvOdmDispDeviceHandle hDevice,
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

void* sharpDsi_GetConfiguration( NvOdmDispDeviceHandle hDevice )
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
    dsi->DsiDisplayMode = NvOdmDispDsiMode_Ganged;
    dsi->GangedType     = NvOdmDisp_GangedType_OddEven;
    return (void *)dsi;
}

void sharpDsi_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values )
{
    *nPins = 0;
}

NvBool sharpDsi_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
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

void sharpDsi_GetHal( NvOdmDispDeviceHandle hDevice )
{
    NV_ASSERT(hDevice);
    if (!hDevice)
        return ;

    hDevice->Setup = sharpDsi_Setup;
    hDevice->Release = sharpDsi_Release;
    hDevice->ListModes = sharpDsi_ListModes;
    hDevice->SetMode = sharpDsi_SetMode;
    hDevice->SetPowerLevel = sharpDsi_SetPowerLevel;
    hDevice->GetParameter = sharpDsi_GetParameter;
    hDevice->GetPinPolarities = sharpDsi_GetPinPolarities;
    hDevice->GetConfiguration = sharpDsi_GetConfiguration;
    hDevice->SetSpecialFunction = sharpDsi_SetSpecialFunction;

    // Backlight functions
    hDevice->SetBacklight = sharpDsi_SetBacklight;
    hDevice->BacklightIntensity = sharpDsi_SetBacklightIntensity;
}

static NvU8 fbuf_mode_sel[] = {0x00, 0x17}; //odd-even, DRAM through
static NvU8 smode[] = {0x07, 0x07}; //vsync mode
static NvU8 mode_set[] = {0x00, 0x70};
static NvU8 mipi_if_sel[] = {0x01, 0x00};

static NvBool sharpDsi_PanelInit( NvOdmDispDeviceHandle hDevice,
                          NvOdmDispDeviceMode *mode)
{
    NvBool b;
    NvU32 i;
    static DsiPanelData s_PanelInitSeq[] = {
        // Panel Reg Addr, Data Size, Register Data
        { 0x29,      0x10,   2, fbuf_mode_sel, NV_TRUE},
        { 0x00, (NvU32)-1,  20,      NULL, NV_FALSE},

        { 0x29,      0x10,   2, mipi_if_sel, NV_TRUE},
        { 0x00, (NvU32)-1,  20,      NULL, NV_FALSE},

        { 0x29,      0x10,   2,     smode, NV_TRUE},
        { 0x00, (NvU32)-1,  20,      NULL, NV_FALSE},

        { 0x29,      0x70,   2,  mode_set, NV_TRUE},
        { 0x00, (NvU32)-1,  20,      NULL, NV_FALSE},

        { 0x05,      0x11,   1, s_ZeroCmd, NV_FALSE},
        { 0x00, (NvU32)-1, 120,      NULL, NV_FALSE},
        { 0x05,      0x29,   1, s_ZeroCmd, NV_FALSE},
        { 0x00, (NvU32)-1, 20,      NULL, NV_FALSE},
    };
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

    s_Lcd_rst= NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('h' - 'a') , 3);
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

    /* Send panel Init sequence */
    for (i = 0; i < NV_ARRAY_SIZE(s_PanelInitSeq); i++)
    {
        if (s_PanelInitSeq[i].pData == NULL)
        {
            NvOdmOsSleepMS(s_PanelInitSeq[i].DataSize);
            continue;
        }

        NvOdmOsSleepMS(DELAY_MS);

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
    NvOdmOsSleepMS(DELAY_MS);

     return NV_TRUE; //returning false till the code is added
}
