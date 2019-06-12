/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_disp.h"
#include "panel_lgd.h"
#include "nvodm_services.h"
#include "nvodm_backlight.h"

//set to 0 for 1st DSI instance and 1 for 2nd DSI instance
#define DSI_INSTANCE 0
#define TEGRA_DC_OUT_CONTINUOUS_MODE 0
#define TEGRA_DC_OUT_ONESHOT_MODE 1

#define DC_CTRL_MODE    TEGRA_DC_OUT_CONTINUOUS_MODE

// Number of bits per byte
#define DSI_NO_OF_BITS_PER_BYTE 8
// Period of one bit time in nano seconds
#define DSI_TBIT(Freq)    (((1000) * (1000))/((Freq) * (2)))
// Period of one byte time in nano seconds
#define DSI_TBYTE(Freq)    ((DSI_TBIT(Freq)) * (DSI_NO_OF_BITS_PER_BYTE))
#define DSI_PHY_TIMING_DIV(X, Freq) ((X) / (DSI_TBYTE(Freq)))
#define DSI_THSTRAIL_VAL(Freq) \
     (NV_MAX(((8) * (DSI_TBIT(Freq))), ((60) + ((4) * (DSI_TBIT(Freq))))))

#define LGD_PANEL_CALC_PHY_TIMINGS \
    dsi->PhyTiming.ThsExit = DSI_PHY_TIMING_DIV(120, (DsiPhyFreqKHz));                          \
    dsi->PhyTiming.TclkZero = DSI_PHY_TIMING_DIV(330, (DsiPhyFreqKHz));                         \
    dsi->PhyTiming.Ttlpx = (DSI_PHY_TIMING_DIV(60, DsiPhyFreqKHz) == 0) ? 1 :                   \
            (DSI_PHY_TIMING_DIV(60, DsiPhyFreqKHz));                                            \
    dsi->PhyTiming.TclkTrail = DSI_PHY_TIMING_DIV(80, (DsiPhyFreqKHz));                         \
    dsi->PhyTiming.ThsTrail = ((3) + (DSI_PHY_TIMING_DIV((DSI_THSTRAIL_VAL(DsiPhyFreqKHz)),     \
            DsiPhyFreqKHz)));                                                                   \
    dsi->PhyTiming.TdatZero = DSI_PHY_TIMING_DIV((((220) + ((5) * (DSI_TBIT(DsiPhyFreqKHz))))), \
            DsiPhyFreqKHz);                                                                     \
    dsi->PhyTiming.ThsPrepr =                                                                   \
        (DSI_PHY_TIMING_DIV((((20) + ((5) * (DSI_TBIT(DsiPhyFreqKHz))))), DsiPhyFreqKHz) == 0)? \
            1: DSI_PHY_TIMING_DIV((((20) + ((5) * (DSI_TBIT(DsiPhyFreqKHz))))),                 \
            DsiPhyFreqKHz);                                                                     \
    dsi->PhyTiming.TclkPost =                                                                   \
        DSI_PHY_TIMING_DIV(                                                                     \
            ((70) + ((52) * (DSI_TBIT(DsiPhyFreqKHz)))),                                        \
            (DsiPhyFreqKHz));                                                                   \
    dsi->PhyTiming.TclkPrepr = DSI_PHY_TIMING_DIV(27, (DsiPhyFreqKHz))                          \

typedef struct DsiPanelDataRec
{
    NvU32 DsiCmd;
    NvU32 PanelReg;
    NvU32 DataSize;
    NvU8 *pData;
    NvBool IsLongPacket;
} DsiPanelData;

static NvBool lgdDsi_PanelInit( NvOdmDispDeviceHandle hDevice, NvOdmDispDeviceMode *mode);

// Color calibration
static NvU32 s_ColorCalibration_Red     = NVODM_DISP_FP_TO_FX(1.00);
static NvU32 s_ColorCalibration_Green   = NVODM_DISP_FP_TO_FX(1.00);
static NvU32 s_ColorCalibration_Blue    = NVODM_DISP_FP_TO_FX(1.00);

// Other global variables.
static NvOdmDispDsiTransport   s_trans;

static NvOdmServicesGpioHandle s_hGpio =  NULL;
static NvOdmGpioPinHandle s_Lcd_bl_en = NULL;
static NvOdmGpioPinHandle s_En_Vdd_bl = NULL;

static NvOdmServicesPwmHandle s_hOdmPwm = NULL;

static NvBool s_bBacklight = NV_FALSE;


static NvOdmDispDeviceMode lgd_modes[] =
{
    { 800, // width
      1280, // height
      24, // bpp(color depth)
      60, // refresh
      71000, // frequency
      0, // flags
      // timing
      { 10, // h_ref_to_sync
        1, // v_ref_to_sync
        1, // h_sync_width
        1, // v_sync_width
        57, // h_back_porch
        14, // v_back_porch
        800, // h_disp_active
        1280, // v_disp_active
        32, // h_front_porch
        28, // v_front_porch
      },
      NV_FALSE // partial
    },
};

static void
lgd_nullmode( NvOdmDispDeviceHandle hDevice )
{
    if (!hDevice)
        return ;
    hDevice->CurrentMode.width = 0;
    hDevice->CurrentMode.height = 0;
    hDevice->CurrentMode.bpp = 0;
}

static NvBool lgd_discover (NvOdmDispDeviceHandle hDevice)
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
        guid = LGD_TPS65914_PANEL_GUID;
    else if (PmuBoard.BoardID == PMU_E1736)
        guid = LGD_E1736TPS65914_PANEL_GUID;
    else if (PmuBoard.BoardID == PMU_E1936)
        guid = LGD_E1736TPS65914_PANEL_GUID;
    else if (PmuBoard.BoardID == PMU_E1769)
        guid = LGD_E1769TPS65914_PANEL_GUID;
    else
        guid = LGD_PANEL_GUID;

    /* get the connectivity info */
    conn = NvOdmPeripheralGetGuid(guid);
    if( !conn )
    {
        return NV_FALSE;
    }

    hDevice->PeripheralConnectivity = conn;
    return NV_TRUE;
}

NvBool lgd_Setup( NvOdmDispDeviceHandle hDevice )
{
    NvOdmServicesPmuVddRailCapabilities RailCaps;
    NvU32 SettlingTime = 0;
    NvU32 i;
    NvOdmServicesPmuHandle hPmu;
    /* get the peripheral config */
    if( !lgd_discover(hDevice))
    {
        return NV_FALSE;
    }

    if (!hDevice)
        return NV_FALSE;

    if( !hDevice->bInitialized )
    {
        hDevice->Type = NvOdmDispDeviceType_DSI;
        hDevice->Usage = NvOdmDispDeviceUsage_Primary;
        hDevice->MaxHorizontalResolution = lgd_modes[0].width;
        hDevice->MaxVerticalResolution = lgd_modes[0].height;
        hDevice->BaseColorSize = NvOdmDispBaseColorSize_888;
        hDevice->DataAlignment = NvOdmDispDataAlignment_MSB;
        hDevice->PinMap = NvOdmDispPinMap_Single_Rgb24_Spi5;
        hDevice->PwmOutputPin = NvOdmDispPwmOutputPin_Unspecified;
        hDevice->modes = lgd_modes;
        hDevice->nModes = NV_ARRAY_SIZE(lgd_modes);
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
                &SettlingTime ); //VDDIO_MIPI_AND_AVDD_LCD_ENABLED.

                /* wait for rail to settle */
                NvOdmOsWaitUS(SettlingTime);
            }
        }
        if (SettlingTime)
        {
            NvOdmOsWaitUS(SettlingTime);
        }

        NvOdmOsSleepMS(150);
        lgd_nullmode( hDevice );

        NvOdmServicesPmuClose(hPmu);

        // Get PWM handle for backlight intensity
        s_hOdmPwm = NvOdmPwmOpen();
        NV_ASSERT(s_hOdmPwm);

        hDevice->bInitialized = NV_TRUE;
    }

    return NV_TRUE;
}

void lgd_Release( NvOdmDispDeviceHandle hDevice )
{
    NvU32 seqSize;
    NvBool b;
    NvU32 i;
    NvOdmServicesPmuHandle hPmu;
    static NvU8 s_ZeroCmd[] = { 0x00 };

    static DsiPanelData s_PanelDeInitSeq[] = {
        // Panel Reg Addr, Data Size, Register Data
        { 0x15,      0x11,   1, s_ZeroCmd, NV_FALSE},
        { 0x00, (NvU32)-1, 160,      NULL, NV_FALSE},
    };

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

    if (s_hGpio)
    {
        NvOdmGpioClose( s_hGpio );
        s_hGpio = NULL;
    }

    s_bBacklight = NV_FALSE;
    NvOdmPwmClose(s_hOdmPwm);
    s_hOdmPwm = NULL;

    b = s_trans.NvOdmDispDsiInit(hDevice, DSI_INSTANCE, 0);
    if (!b)
    {
        NV_ASSERT("dsi init failed \n");
        return;
    }

    b = s_trans.NvOdmDispDsiEnableCommandMode(hDevice, DSI_INSTANCE);
    if (!b)
    {
        NV_ASSERT("dsi command mode failed\n");
        return;
    }

    seqSize = NV_ARRAY_SIZE(s_PanelDeInitSeq);

    for ( i =0; i<seqSize; i++)
    {
        if (s_PanelDeInitSeq[i].pData == NULL)
        {
            NvOdmOsSleepMS(s_PanelDeInitSeq[i].DataSize);
            continue;
        }

        //Delay might be required between commands
        b = s_trans.NvOdmDispDsiWrite( hDevice,
        s_PanelDeInitSeq[i].DsiCmd,
        s_PanelDeInitSeq[i].PanelReg,
        s_PanelDeInitSeq[i].DataSize,
        s_PanelDeInitSeq[i].pData,
        DSI_INSTANCE,
        s_PanelDeInitSeq[i].IsLongPacket );

        if ( !b )
        {
            NvOdmOsPrintf("Error> DSI Panel DeInitialization Failed\n");
            return;
        }
        else
        {
            NvOdmOsPrintf("DSI Panel DeInitialization Success\n");
        }
    }

    hPmu = NvOdmServicesPmuOpen();
    for( i = hDevice->PeripheralConnectivity->NumAddress; i > 0; i-- )
    {
        if (hDevice->PeripheralConnectivity->AddressList[i-1].Interface ==
            NvOdmIoModule_Vdd)
        {
            /* set the rail volatage to the recommended */
            NvOdmServicesPmuSetVoltage(hPmu,
                hDevice->PeripheralConnectivity->AddressList[i-1].Address,
                NVODM_VOLTAGE_OFF,
                0);
        }
    }

    NvOdmServicesPmuClose(hPmu);

    //Remove Initialised variables etc
    lgd_nullmode( hDevice );
}

void lgd_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
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

NvBool lgd_SetMode( NvOdmDispDeviceHandle hDevice,
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
        lgd_nullmode(hDevice);
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
        lgdDsi_PanelInit(hDevice, mode);
    }
    else
    {
        lgd_nullmode(hDevice);
    }
    return NV_TRUE;
}

void lgd_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    if( !s_bBacklight )
    {
        NvOdmBacklightInit( hDevice, NV_FALSE );
        s_bBacklight = NV_TRUE;
    }

    NvOdmBacklightIntensity( hDevice, intensity );
}

void lgd_SetBacklightIntensity( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
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

    RequestedFreq = 1000; // Hz

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
lgdDsi_Resume( NvOdmDispDeviceHandle hDevice )
{
    lgdDsi_PanelInit( hDevice, &hDevice->CurrentMode );
}

void lgd_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
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
            lgdDsi_Resume( hDevice );
        }
            break;
        default:
            break;
    }
}

NvBool lgd_GetParameter( NvOdmDispDeviceHandle hDevice,
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

void* lgd_GetConfiguration( NvOdmDispDeviceHandle hDevice )
{
    NvOdmDispDsiConfig *dsi;
    NvU32 DsiPhyFreqKHz = 0;
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
    dsi->DsiDisplayMode = NvOdmDispDsiMode_Normal;

    dsi->bIsPhytimingsValid = NV_FALSE;
    if (s_trans.NvOdmDispDsiGetPhyFreq)
    {
        s_trans.NvOdmDispDsiGetPhyFreq( hDevice, DSI_INSTANCE, &DsiPhyFreqKHz);
        if( !DsiPhyFreqKHz )
        {
            NvOdmOsMemset( &dsi->PhyTiming, 0, sizeof(dsi->PhyTiming) );
        }
        else
        {
            LGD_PANEL_CALC_PHY_TIMINGS;

            dsi->bIsPhytimingsValid = NV_TRUE;
        }
    }
    return (void *)dsi;
}

void lgd_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values )
{
    *nPins = 0;
}

NvBool lgd_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
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

void lgd_GetHal( NvOdmDispDeviceHandle hDevice )
{
    NV_ASSERT(hDevice);
    if (!hDevice)
        return ;

    hDevice->Setup = lgd_Setup;
    hDevice->Release = lgd_Release;
    hDevice->ListModes = lgd_ListModes;
    hDevice->SetMode = lgd_SetMode;
    hDevice->SetPowerLevel = lgd_SetPowerLevel;
    hDevice->GetParameter = lgd_GetParameter;
    hDevice->GetPinPolarities = lgd_GetPinPolarities;
    hDevice->GetConfiguration = lgd_GetConfiguration;
    hDevice->SetSpecialFunction = lgd_SetSpecialFunction;
    // Backlight functions
    hDevice->SetBacklight = lgd_SetBacklight;
    hDevice->BacklightIntensity = lgd_SetBacklightIntensity;
}

static NvBool lgdDsi_PanelInit( NvOdmDispDeviceHandle hDevice,
                          NvOdmDispDeviceMode *mode)
{
    NvU32 seqSize;
    NvU32 i;
    NvBool b;

    static NvU8 s_ZeroCmd[] = { 0x00 };
    static NvU8 s_IncreaseSwingCmd[] = { 0x0B };
    static NvU8 s_CtrlSetting1Cmd[] = { 0xEA };
    static NvU8 s_CtrlSetting2Cmd[] = { 0x5F };
    static NvU8 s_RxDrivingCmd[] = { 0x68 };

    static DsiPanelData s_PanelInitSeq[] = {
        // Panel Reg Addr, Data Size, Register Data
        { 0x15,      0x01,   1, s_ZeroCmd, NV_FALSE},
        { 0x00, (NvU32)-1, 20,      NULL, NV_FALSE},
        { 0x15,      0xAE,   1, s_IncreaseSwingCmd, NV_FALSE},
        { 0x15,      0xEE,   1, s_CtrlSetting1Cmd, NV_FALSE},
        { 0x15,      0xEF,   1, s_CtrlSetting2Cmd, NV_FALSE},
        { 0x15,      0xF2,   1, s_RxDrivingCmd, NV_FALSE},
        { 0x15,      0xEE,   1, s_ZeroCmd, NV_FALSE},
        { 0x15,      0xEF,   1, s_ZeroCmd, NV_FALSE},
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
            NvOdmOsPrintf("Error> DSI Panel Initialization Failed\n");
            return NV_FALSE;
        }
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

    return NV_TRUE;
}
