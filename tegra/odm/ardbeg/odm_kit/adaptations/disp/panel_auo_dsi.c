/*
 * Copyright (c) 2013 - 2014, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_disp.h"
#include "panel_auo_dsi.h"
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

#define INITIAL_BACKLIGHT_INTENSITY 153

typedef struct DsiPanelDataRec
{
    NvU32 DsiCmd;
    NvU32 PanelReg;
    NvU32 DataSize;
    NvU8 *pData;
    NvBool IsLongPacket;
} DsiPanelData;

static NvBool auoDsi_PanelInit(NvOdmDispDeviceHandle hDevice, NvOdmDispDeviceMode *mode);

// Color calibration
static NvU32 s_ColorCalibration_Red     = NVODM_DISP_FP_TO_FX(1.00);
static NvU32 s_ColorCalibration_Green   = NVODM_DISP_FP_TO_FX(1.00);
static NvU32 s_ColorCalibration_Blue    = NVODM_DISP_FP_TO_FX(1.00);

static NvU8 s_ZeroCmd[] = { 0x00 };
static NvOdmServicesGpioHandle s_hGpio =  NULL;
static NvOdmGpioPinHandle s_Lcd_bl_en = NULL;
static NvOdmGpioPinHandle s_Lcd_rst = NULL;
static NvOdmGpioPinHandle s_Lcd_vgh_en = NULL;

#define S818_LCD_DSI_40PIN 1
//#define S818_LCD_DSI_31PIN 1

#ifdef S818_LCD_DSI_40PIN
static NvOdmServicesPwmHandle s_hOdmPwm = NULL;

static NvBool s_bBacklight = NV_FALSE;


// Other global variables.
static NvOdmDispDsiTransport   s_trans;

static NvOdmDispDeviceMode auoDsi_E1807_modes[] =
{
    { 1200, // width
      1920, // height
      24, // bpp(color depth)
      0, // refresh
      0, // frequency
      0, // flags
      // timing
      { 4, // h_ref_to_sync
        1, // v_ref_to_sync
        16, // h_sync_width
        2, // v_sync_width
        32, // h_back_porch
        16, // v_back_porch
        1200, // h_disp_active
        1920, // v_disp_active
        120, // h_front_porch
        17, // v_front_porch
      },
      NV_FALSE // partial
    },
};

static NvOdmDispDeviceMode auoDsi_E1937_modes[] =
{
    { 1200, // width
      1920, // height
      24, // bpp(color depth)
      0, // refresh
      0, // frequency
      0, // flags
      // timing
      { 4, // h_ref_to_sync
        1, // v_ref_to_sync
        10, // h_sync_width
        2, // v_sync_width
        54, // h_back_porch
        3, // v_back_porch
        1200, // h_disp_active
        1920, // v_disp_active
        64, // h_front_porch
        15, // v_front_porch
      },
      NV_FALSE // partial
    },
};

static void
auoDsi_nullmode( NvOdmDispDeviceHandle hDevice )
{
    if (!hDevice)
        return ;
    hDevice->CurrentMode.width = 0;
    hDevice->CurrentMode.height = 0;
    hDevice->CurrentMode.bpp = 0;
}

static NvBool auoDsi_discover (NvOdmDispDeviceHandle hDevice)
{
    NvU64 guid = 0;
    NvOdmPeripheralConnectivity const *conn;
    NvOdmDisplayBoardInfo DisplayBoardInfo;
    NvOdmDisplayPanelInfo DisplayPanelInfo;

    NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayBoard,
                      &DisplayBoardInfo, sizeof(DisplayBoardInfo));

    /* get the main panel */
    if (DisplayBoardInfo.BoardInfo.BoardID == BOARD_P1761)
    {
        if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayPanel,
                              &DisplayPanelInfo, sizeof(DisplayPanelInfo)))
        {
            if (DisplayPanelInfo.DisplayPanelId)  //E1937
                guid = AUO_E1937TPS65914_PANEL_GUID;
            else
                guid = AUO_E1807TPS65914_PANEL_GUID;
        }
    }
    else if (DisplayBoardInfo.BoardInfo.BoardID == BOARD_E1807)
        guid = AUO_E1807TPS65914_PANEL_GUID;
    else if (DisplayBoardInfo.BoardInfo.BoardID == BOARD_E1937)
        guid = AUO_E1937TPS65914_PANEL_GUID;
    else
    {
        NV_ASSERT(!"auoDsi_discover : Not Found BoardID \n");
        return NV_FALSE;
    }

    /* get the connectivity info */
    conn = NvOdmPeripheralGetGuid(guid);
    if( !conn )
    {
        NV_ASSERT(!"AUO guid acquisition failed \n");
        return NV_FALSE;
    }

    hDevice->PeripheralConnectivity = conn;
    return NV_TRUE;
}

NvBool auoDsi_Setup( NvOdmDispDeviceHandle hDevice )
{
    NvOdmServicesPmuVddRailCapabilities RailCaps;
    NvU32 SettlingTime = 0;
    NvU32 i;
    NvOdmServicesPmuHandle hPmu;
    NvOdmDisplayBoardInfo DisplayBoardInfo;
    NvOdmDisplayPanelInfo DisplayPanelInfo;

    /* get the peripheral config */
    if( !auoDsi_discover(hDevice))
    {
        return NV_FALSE;
    }

    if (!hDevice)
        return NV_FALSE;

    NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayBoard,
                      &DisplayBoardInfo, sizeof(DisplayBoardInfo));

    if( !hDevice->bInitialized )
    {
        hDevice->Type = NvOdmDispDeviceType_DSI;
        hDevice->Usage = NvOdmDispDeviceUsage_Primary;
        hDevice->BaseColorSize = NvOdmDispBaseColorSize_888;
        hDevice->DataAlignment = NvOdmDispDataAlignment_MSB;
        hDevice->PinMap = NvOdmDispPinMap_Single_Rgb24_Spi5;
        hDevice->PwmOutputPin = NvOdmDispPwmOutputPin_Unspecified;
        hDevice->power = NvOdmDispDevicePowerLevel_Off;
        if (DisplayBoardInfo.BoardInfo.BoardID == BOARD_P1761)
        {
            if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayPanel,
                                  &DisplayPanelInfo, sizeof(DisplayPanelInfo)))
            {
                if (DisplayPanelInfo.DisplayPanelId)  //E1937
                {
                    hDevice->MaxHorizontalResolution = auoDsi_E1937_modes[0].width;
                    hDevice->MaxVerticalResolution = auoDsi_E1937_modes[0].height;
                    hDevice->modes = auoDsi_E1937_modes;
                    hDevice->nModes = NV_ARRAY_SIZE(auoDsi_E1937_modes);
                }
                else
                {
                    hDevice->MaxHorizontalResolution = auoDsi_E1807_modes[0].width;
                    hDevice->MaxVerticalResolution = auoDsi_E1807_modes[0].height;
                    hDevice->modes = auoDsi_E1807_modes;
                    hDevice->nModes = NV_ARRAY_SIZE(auoDsi_E1807_modes);
                }
            }
        }
        else if (DisplayBoardInfo.BoardInfo.BoardID == BOARD_E1807)
        {
            hDevice->MaxHorizontalResolution = auoDsi_E1807_modes[0].width;
            hDevice->MaxVerticalResolution = auoDsi_E1807_modes[0].height;
            hDevice->modes = auoDsi_E1807_modes;
            hDevice->nModes = NV_ARRAY_SIZE(auoDsi_E1807_modes);
        }
        else  //E1937
        {
            hDevice->MaxHorizontalResolution = auoDsi_E1937_modes[0].width;
            hDevice->MaxVerticalResolution = auoDsi_E1937_modes[0].height;
            hDevice->modes = auoDsi_E1937_modes;
            hDevice->nModes = NV_ARRAY_SIZE(auoDsi_E1937_modes);
        }

        hPmu = NvOdmServicesPmuOpen();

        for( i = 0; i < hDevice->PeripheralConnectivity->NumAddress; i++ )
        {
            if(hDevice->PeripheralConnectivity->AddressList[i].Interface
                == NvOdmIoModule_Vdd )
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

        NvOdmOsSleepMS(4);
        auoDsi_nullmode( hDevice );

        NvOdmServicesPmuClose(hPmu);

        // Get PWM handle for backlight intensity
        s_hOdmPwm = NvOdmPwmOpen();
        NV_ASSERT(s_hOdmPwm);

        hDevice->bInitialized = NV_TRUE;
    }

    return NV_TRUE;
}

void auoDsi_Release( NvOdmDispDeviceHandle hDevice )
{

    NvBool b;
    NvU32 i;
    NvOdmServicesPmuHandle hPmu;
    NvOdmDisplayBoardInfo DisplayBoardInfo;
    NvOdmDisplayPanelInfo DisplayPanelInfo;
    NvBool NeedcmdInit = NV_FALSE;

    static DsiPanelData s_PanelDeInitSeq[] = {
        // Panel Reg Addr, Data Size, Register Data
        { 0x05,      0x28,   1, s_ZeroCmd, NV_FALSE},
        { 0x00, (NvU32)-1,  20,      NULL, NV_FALSE},
        { 0x05,      0x10,   1, s_ZeroCmd, NV_FALSE},
        { 0x00, (NvU32)-1,  80,      NULL, NV_FALSE},
    };

    if (!hDevice)
        return ;

    NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayBoard,
                      &DisplayBoardInfo, sizeof(DisplayBoardInfo));
    if (DisplayBoardInfo.BoardInfo.BoardID == BOARD_P1761)
    {
        if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayPanel,
            &DisplayPanelInfo, sizeof(DisplayPanelInfo)))
        {
            if (DisplayPanelInfo.DisplayPanelId)  //E1937
                NeedcmdInit = NV_FALSE;
            else
                NeedcmdInit = NV_TRUE;
        }
    }
    else if (DisplayBoardInfo.BoardInfo.BoardID == BOARD_E1807)
        NeedcmdInit = NV_TRUE;
    else
        NeedcmdInit = NV_FALSE;

    NvOdmBacklightIntensity( hDevice, 0 );
    NvOdmBacklightDeinit(hDevice);
    hDevice->bInitialized = NV_FALSE;

    s_bBacklight = NV_FALSE;
    NvOdmPwmClose(s_hOdmPwm);
    s_hOdmPwm = NULL;

    /* Send panel Init sequence */
    if (NeedcmdInit)
    {
        for (i = 0; i < NV_ARRAY_SIZE(s_PanelDeInitSeq); i++)
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
            }
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

    if (s_Lcd_bl_en)
    {
        NvOdmGpioSetState(s_hGpio, s_Lcd_bl_en, 0);
        NvOdmGpioReleasePinHandle( s_hGpio, s_Lcd_bl_en);
        s_Lcd_bl_en = NULL;
    }

    if (s_Lcd_rst)
    {
        NvOdmGpioReleasePinHandle( s_hGpio, s_Lcd_rst);
        s_Lcd_rst = NULL;
    }
    //Remove Initialised variables etc
    auoDsi_nullmode( hDevice );
}

void auoDsi_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
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

NvBool auoDsi_SetMode( NvOdmDispDeviceHandle hDevice,
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
        auoDsi_nullmode(hDevice);
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
        auoDsi_PanelInit(hDevice, mode);
    }
    else
    {
        auoDsi_nullmode(hDevice);
    }
    return NV_TRUE;
}

void auoDsi_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
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

void auoDsi_SetBacklightIntensity( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
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
auoDsi_Resume( NvOdmDispDeviceHandle hDevice )
{
    auoDsi_PanelInit( hDevice, &hDevice->CurrentMode );
}

void auoDsi_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
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
            auoDsi_Resume( hDevice );
        }
        break;
    default:
        break;
    }
}

NvBool auoDsi_GetParameter( NvOdmDispDeviceHandle hDevice,
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
    case NvOdmDispParameter_BacklightInitialIntensity:
        *val = INITIAL_BACKLIGHT_INTENSITY;
        break;
    default:
        return NV_FALSE;
    }
    return NV_TRUE;
}

void* auoDsi_GetConfiguration( NvOdmDispDeviceHandle hDevice )
{
    NvOdmDispDsiConfig *dsi;
    if (!hDevice)
        return NULL;
    dsi = &hDevice->Config.dsi;
    dsi->DataFormat = NvOdmDsiDataFormat_24BitP;
    dsi->VideoMode = NvOdmDsiVideoModeType_NonBurstSyncEnd;
    dsi->VirChannel = NvOdmDsiVirtualChannel_0;
    dsi->NumOfDataLines = 4;
    dsi->RefreshRate = 60;
    dsi->Freq = 162000;
    dsi->PanelResetTimeoutMSec = 202;
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

void auoDsi_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values )
{
    *nPins = 0;
}

NvBool auoDsi_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
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

void auoDsi_GetHal( NvOdmDispDeviceHandle hDevice )
{
    NV_ASSERT(hDevice);
    if (!hDevice)
        return ;

    hDevice->Setup = auoDsi_Setup;
    hDevice->Release = auoDsi_Release;
    hDevice->ListModes = auoDsi_ListModes;
    hDevice->SetMode = auoDsi_SetMode;
    hDevice->SetPowerLevel = auoDsi_SetPowerLevel;
    hDevice->GetParameter = auoDsi_GetParameter;
    hDevice->GetPinPolarities = auoDsi_GetPinPolarities;
    hDevice->GetConfiguration = auoDsi_GetConfiguration;
    hDevice->SetSpecialFunction = auoDsi_SetSpecialFunction;

    // Backlight functions
    hDevice->SetBacklight = auoDsi_SetBacklight;
    hDevice->BacklightIntensity = auoDsi_SetBacklightIntensity;
}

static NvBool auoDsi_PanelInit( NvOdmDispDeviceHandle hDevice,
                          NvOdmDispDeviceMode *mode)
{
    NvBool b;
    NvU32 i;
    NvOdmDisplayBoardInfo DisplayBoardInfo;
    NvOdmDisplayPanelInfo DisplayPanelInfo;
    NvBool NeedcmdInit = NV_FALSE;

	static NvU8 init_cmd1[] = {0xB3,0x14,0x08,0x00,0x22,0x00};
	static NvU8 init_cmd2[] = {0xB4,0x0C};
	static NvU8 init_cmd3[] = {0xB6,0x3A,0xC3};
	static NvU8 init_cmd4[] = {0x2A,0x00,0x00,0x04,0xAF};
	static NvU8 init_cmd5[] = {0x2B,0x00,0x00,0x07,0x7F};
	static NvU8 init_cmd6[] = {0xE6};
	static NvU8 init_cmd7[] = {0x2C};
	
    static DsiPanelData s_PanelInitSeq[] = {
        // Panel Reg Addr, Data Size, Register Data
        {0x05,      0x01,   1, s_ZeroCmd, NV_FALSE},
        {0x00, (NvU32)-1,   5,      NULL, NV_FALSE},
        {0x23,      0xB0,   1, s_ZeroCmd, NV_FALSE},
		{0x00, (NvU32)-1,   3,      NULL, NV_FALSE},
		{0x29,      0x00,   6, init_cmd1, NV_TRUE},
		{0x00, (NvU32)-1,   3,      NULL, NV_FALSE},
		{0x29,      0x00,   2, init_cmd2, NV_TRUE},
		{0x29,      0x00,   3, init_cmd3, NV_TRUE},
		{0x15,      0x51,   1, init_cmd6, NV_FALSE},
		{0x15,      0x53,   1, init_cmd7, NV_FALSE},
        {0x05,      0x29,   1, s_ZeroCmd, NV_FALSE},
        {0x00, (NvU32)-1,  15,      NULL, NV_FALSE},
        {0x05,      0x11,   1, s_ZeroCmd, NV_FALSE},
        {0x00, (NvU32)-1, 120,      NULL, NV_FALSE},
    };
    if (!hDevice)
        return NV_FALSE;
    NV_ASSERT( mode->width && mode->height && mode->bpp );

    if (!hDevice->bInitialized)
    {
        NV_ASSERT(!"device not initialized");
        return NV_FALSE;
    }

    NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayBoard,
                      &DisplayBoardInfo, sizeof(DisplayBoardInfo));
    if (DisplayBoardInfo.BoardInfo.BoardID == BOARD_P1761)
    {
        if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayPanel,
            &DisplayPanelInfo, sizeof(DisplayPanelInfo)))
        {
            if (DisplayPanelInfo.DisplayPanelId)  //E1937
                NeedcmdInit = NV_FALSE;
            else
                NeedcmdInit = NV_TRUE;            //E1807
        }
    }
    else if (DisplayBoardInfo.BoardInfo.BoardID == BOARD_E1807)
        NeedcmdInit = NV_TRUE;
    else
        NeedcmdInit = NV_FALSE;

    s_hGpio = NvOdmGpioOpen();
    if (!s_hGpio)
    {
        NV_ASSERT(!"GPIO open failed \n");
        return NV_FALSE;
    }

    s_Lcd_rst= NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('h' - 'a') , 3);
    if (!s_Lcd_rst)
    {
        NV_ASSERT(!"GPIO pin acquisition failed \n");
        return NV_FALSE;
    }

    s_Lcd_bl_en = NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('h' - 'a') , 2);
    if (!s_Lcd_bl_en)
    {
        NV_ASSERT(!"GPIO pin acquisition failed \n");
        return NV_FALSE;
    }
	s_Lcd_vgh_en= NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('r' - 'a') , 4);
	if (!s_Lcd_vgh_en){
		NvOsDebugPrintf("[pengge]:GPIO pin acquisition failed\n");
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

    NvOdmGpioSetState(s_hGpio, s_Lcd_rst, 1);
    NvOdmGpioConfig(s_hGpio, s_Lcd_rst, NvOdmGpioPinMode_Output);
	NvOdmGpioSetState(s_hGpio, s_Lcd_vgh_en, 1);
    NvOdmGpioConfig(s_hGpio, s_Lcd_vgh_en, NvOdmGpioPinMode_Output);
	
    NvOdmOsSleepMS(20);
	NvOdmGpioSetState(s_hGpio,s_Lcd_vgh_en,0x01);
	NvOdmOsSleepMS(20);
    NvOdmGpioSetState(s_hGpio, s_Lcd_rst, 0x01);
	NvOdmOsSleepMS(20);
	NvOdmGpioSetState(s_hGpio,s_Lcd_vgh_en,0x00);
	NvOdmOsSleepMS(20);
	NvOdmGpioSetState(s_hGpio,s_Lcd_vgh_en,0x01);
 
	 NvOdmOsSleepMS(5);
	NvOdmGpioSetState(s_hGpio, s_Lcd_bl_en, 0x01);
    NvOdmGpioConfig(s_hGpio, s_Lcd_bl_en, NvOdmGpioPinMode_Output);
	NvOdmGpioSetState(s_hGpio, s_Lcd_bl_en, 0x01);
	NvOdmOsSleepMS(20);

    /* Send panel Init sequence */
    if (NeedcmdInit)
    {
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
    }



    return NV_TRUE; //returning false till the code is added
}
#endif
#ifdef	S818_LCD_DSI_31PIN
static NvOdmGpioPinHandle s_vdd_3v3 = NULL;
static NvOdmGpioPinHandle vdd_rst_3v3 = NULL;
static NvOdmServicesPwmHandle s_hOdmPwm = NULL;

static NvBool s_bBacklight = NV_FALSE;


// Other global variables.
static NvOdmDispDsiTransport   s_trans;

static NvOdmDispDeviceMode auoDsi_E1807_modes[] =
{
    { 1200, // width
      1920, // height
      24, // bpp(color depth)
      0, // refresh
      0, // frequency
      0, // flags
      // timing
      { 2, // h_ref_to_sync
        2, // v_ref_to_sync
        12, // h_sync_width
        2, // v_sync_width
        60, // h_back_porch
        8, // v_back_porch
        1200, // h_disp_active
        1920, // v_disp_active
        120, // h_front_porch
        8, // v_front_porch
      },
      NV_FALSE // partial
    },
};

static NvOdmDispDeviceMode auoDsi_E1937_modes[] =
{
    { 1200, // width
      1920, // height
      24, // bpp(color depth)
      0, // refresh
      0, // frequency
      0, // flags
      // timing
      { 2, // h_ref_to_sync
        2, // v_ref_to_sync
        12, // h_sync_width
        2, // v_sync_width
        60, // h_back_porch
        8, // v_back_porch
        1200, // h_disp_active
        1920, // v_disp_active
        120, // h_front_porch
        8, // v_front_porch
      },
      NV_FALSE // partial
    },
};

static void
auoDsi_nullmode( NvOdmDispDeviceHandle hDevice )
{
    if (!hDevice)
        return ;
    hDevice->CurrentMode.width = 0;
    hDevice->CurrentMode.height = 0;
    hDevice->CurrentMode.bpp = 0;
}

static NvBool auoDsi_discover (NvOdmDispDeviceHandle hDevice)
{
    NvU64 guid = 0;
    NvOdmPeripheralConnectivity const *conn;
    NvOdmDisplayBoardInfo DisplayBoardInfo;
    NvOdmDisplayPanelInfo DisplayPanelInfo;

    NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayBoard,
                      &DisplayBoardInfo, sizeof(DisplayBoardInfo));

    /* get the main panel */
    if (DisplayBoardInfo.BoardInfo.BoardID == BOARD_P1761)
    {
        if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayPanel,
                              &DisplayPanelInfo, sizeof(DisplayPanelInfo)))
        {
            if (DisplayPanelInfo.DisplayPanelId)  //E1937
                guid = AUO_E1937TPS65914_PANEL_GUID;
            else
                guid = AUO_E1807TPS65914_PANEL_GUID;
        }
    }
    else if (DisplayBoardInfo.BoardInfo.BoardID == BOARD_E1807)
        guid = AUO_E1807TPS65914_PANEL_GUID;
    else if (DisplayBoardInfo.BoardInfo.BoardID == BOARD_E1937)
        guid = AUO_E1937TPS65914_PANEL_GUID;
    else
    {
        NV_ASSERT(!"auoDsi_discover : Not Found BoardID \n");
        return NV_FALSE;
    }

    /* get the connectivity info */
    conn = NvOdmPeripheralGetGuid(guid);
    if( !conn )
    {
        NV_ASSERT(!"AUO guid acquisition failed \n");
        return NV_FALSE;
    }

    hDevice->PeripheralConnectivity = conn;
    return NV_TRUE;
}

NvBool auoDsi_Setup( NvOdmDispDeviceHandle hDevice )
{
    NvOdmServicesPmuVddRailCapabilities RailCaps;
    NvU32 SettlingTime = 0;
    NvU32 i;
    NvOdmServicesPmuHandle hPmu;
    NvOdmDisplayBoardInfo DisplayBoardInfo;
    NvOdmDisplayPanelInfo DisplayPanelInfo;

    /* get the peripheral config */
    if( !auoDsi_discover(hDevice))
    {
        return NV_FALSE;
    }

    if (!hDevice)
        return NV_FALSE;

    NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayBoard,
                      &DisplayBoardInfo, sizeof(DisplayBoardInfo));

    if( !hDevice->bInitialized )
    {
        hDevice->Type = NvOdmDispDeviceType_DSI;
        hDevice->Usage = NvOdmDispDeviceUsage_Primary;
        hDevice->BaseColorSize = NvOdmDispBaseColorSize_888;
        hDevice->DataAlignment = NvOdmDispDataAlignment_MSB;
        hDevice->PinMap = NvOdmDispPinMap_Single_Rgb24_Spi5;
        hDevice->PwmOutputPin = NvOdmDispPwmOutputPin_Unspecified;
        hDevice->power = NvOdmDispDevicePowerLevel_Off;
        if (DisplayBoardInfo.BoardInfo.BoardID == BOARD_P1761)
        {
            if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayPanel,
                                  &DisplayPanelInfo, sizeof(DisplayPanelInfo)))
            {
                if (DisplayPanelInfo.DisplayPanelId)  //E1937
                {
                    hDevice->MaxHorizontalResolution = auoDsi_E1937_modes[0].width;
                    hDevice->MaxVerticalResolution = auoDsi_E1937_modes[0].height;
                    hDevice->modes = auoDsi_E1937_modes;
                    hDevice->nModes = NV_ARRAY_SIZE(auoDsi_E1937_modes);
                }
                else
                {
                    hDevice->MaxHorizontalResolution = auoDsi_E1807_modes[0].width;
                    hDevice->MaxVerticalResolution = auoDsi_E1807_modes[0].height;
                    hDevice->modes = auoDsi_E1807_modes;
                    hDevice->nModes = NV_ARRAY_SIZE(auoDsi_E1807_modes);
                }
            }
        }
        else if (DisplayBoardInfo.BoardInfo.BoardID == BOARD_E1807)
        {
            hDevice->MaxHorizontalResolution = auoDsi_E1807_modes[0].width;
            hDevice->MaxVerticalResolution = auoDsi_E1807_modes[0].height;
            hDevice->modes = auoDsi_E1807_modes;
            hDevice->nModes = NV_ARRAY_SIZE(auoDsi_E1807_modes);
        }
        else  //E1937
        {
            hDevice->MaxHorizontalResolution = auoDsi_E1937_modes[0].width;
            hDevice->MaxVerticalResolution = auoDsi_E1937_modes[0].height;
            hDevice->modes = auoDsi_E1937_modes;
            hDevice->nModes = NV_ARRAY_SIZE(auoDsi_E1937_modes);
        }

        hPmu = NvOdmServicesPmuOpen();

        for( i = 0; i < hDevice->PeripheralConnectivity->NumAddress; i++ )
        {
            if(hDevice->PeripheralConnectivity->AddressList[i].Interface
                == NvOdmIoModule_Vdd )
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

        NvOdmOsSleepMS(4);
        auoDsi_nullmode( hDevice );

        NvOdmServicesPmuClose(hPmu);

        // Get PWM handle for backlight intensity
        s_hOdmPwm = NvOdmPwmOpen();
        NV_ASSERT(s_hOdmPwm);

        hDevice->bInitialized = NV_TRUE;
    }

    return NV_TRUE;
}

void auoDsi_Release( NvOdmDispDeviceHandle hDevice )
{

    NvBool b;
    NvU32 i;
    NvOdmServicesPmuHandle hPmu;
    NvOdmDisplayBoardInfo DisplayBoardInfo;
    NvOdmDisplayPanelInfo DisplayPanelInfo;
    NvBool NeedcmdInit = NV_FALSE;

    static DsiPanelData s_PanelDeInitSeq[] = {
        // Panel Reg Addr, Data Size, Register Data
        { 0x05,      0x28,   1, s_ZeroCmd, NV_FALSE},
        { 0x00, (NvU32)-1,  20,      NULL, NV_FALSE},
        { 0x05,      0x10,   1, s_ZeroCmd, NV_FALSE},
        { 0x00, (NvU32)-1,  80,      NULL, NV_FALSE},
    };

    if (!hDevice)
        return ;

    NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayBoard,
                      &DisplayBoardInfo, sizeof(DisplayBoardInfo));
    if (DisplayBoardInfo.BoardInfo.BoardID == BOARD_P1761)
    {
        if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayPanel,
            &DisplayPanelInfo, sizeof(DisplayPanelInfo)))
        {
            if (DisplayPanelInfo.DisplayPanelId)  //E1937
                NeedcmdInit = NV_FALSE;
            else
                NeedcmdInit = NV_TRUE;
        }
    }
    else if (DisplayBoardInfo.BoardInfo.BoardID == BOARD_E1807)
        NeedcmdInit = NV_TRUE;
    else
        NeedcmdInit = NV_FALSE;

    NvOdmBacklightIntensity( hDevice, 0 );
    NvOdmBacklightDeinit(hDevice);
    hDevice->bInitialized = NV_FALSE;

    s_bBacklight = NV_FALSE;
    NvOdmPwmClose(s_hOdmPwm);
    s_hOdmPwm = NULL;

    /* Send panel Init sequence */
    if (NeedcmdInit)
    {
        for (i = 0; i < NV_ARRAY_SIZE(s_PanelDeInitSeq); i++)
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
            }
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

    if (s_Lcd_bl_en)
    {
        NvOdmGpioSetState(s_hGpio, s_Lcd_bl_en, 0);
        NvOdmGpioReleasePinHandle( s_hGpio, s_Lcd_bl_en);
        s_Lcd_bl_en = NULL;
    }

    if (s_Lcd_rst)
    {
        NvOdmGpioReleasePinHandle( s_hGpio, s_Lcd_rst);
        s_Lcd_rst = NULL;
    }
    //Remove Initialised variables etc
    auoDsi_nullmode( hDevice );
}

void auoDsi_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
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

NvBool auoDsi_SetMode( NvOdmDispDeviceHandle hDevice,
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
        auoDsi_nullmode(hDevice);
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
        auoDsi_PanelInit(hDevice, mode);
    }
    else
    {
        auoDsi_nullmode(hDevice);
    }
    return NV_TRUE;
}

void auoDsi_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
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

void auoDsi_SetBacklightIntensity( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
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
auoDsi_Resume( NvOdmDispDeviceHandle hDevice )
{
    auoDsi_PanelInit( hDevice, &hDevice->CurrentMode );
}

void auoDsi_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
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
            auoDsi_Resume( hDevice );
        }
        break;
    default:
        break;
    }
}

NvBool auoDsi_GetParameter( NvOdmDispDeviceHandle hDevice,
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
    case NvOdmDispParameter_BacklightInitialIntensity:
        *val = INITIAL_BACKLIGHT_INTENSITY;
        break;
    default:
        return NV_FALSE;
    }
    return NV_TRUE;
}

void* auoDsi_GetConfiguration( NvOdmDispDeviceHandle hDevice )
{
    NvOdmDispDsiConfig *dsi;
    if (!hDevice)
        return NULL;
    dsi = &hDevice->Config.dsi;
    dsi->DataFormat = 	NvOdmDsiDataFormat_24BitP;
    dsi->VideoMode = 	NvOdmDsiVideoModeType_DcDrivenCommandMode;
    dsi->VirChannel = 	NvOdmDsiVirtualChannel_0;
    dsi->NumOfDataLines = 4;
    dsi->RefreshRate = 60;
    dsi->Freq = 162000;
    dsi->PanelResetTimeoutMSec = 202;
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

void auoDsi_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values )
{
    *nPins = 0;
}

NvBool auoDsi_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
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

void auoDsi_GetHal( NvOdmDispDeviceHandle hDevice )
{
    NV_ASSERT(hDevice);
    if (!hDevice)
        return ;

    hDevice->Setup = auoDsi_Setup;
    hDevice->Release = auoDsi_Release;
    hDevice->ListModes = auoDsi_ListModes;
    hDevice->SetMode = auoDsi_SetMode;
    hDevice->SetPowerLevel = auoDsi_SetPowerLevel;
    hDevice->GetParameter = auoDsi_GetParameter;
    hDevice->GetPinPolarities = auoDsi_GetPinPolarities;
    hDevice->GetConfiguration = auoDsi_GetConfiguration;
    hDevice->SetSpecialFunction = auoDsi_SetSpecialFunction;

    // Backlight functions
    hDevice->SetBacklight = auoDsi_SetBacklight;
    hDevice->BacklightIntensity = auoDsi_SetBacklightIntensity;
}

static NvBool auoDsi_PanelInit( NvOdmDispDeviceHandle hDevice,
                          NvOdmDispDeviceMode *mode)
{
    NvBool b;
    NvU32 i;
    NvOdmDisplayBoardInfo DisplayBoardInfo;
    NvOdmDisplayPanelInfo DisplayPanelInfo;
    NvBool NeedcmdInit = NV_FALSE;

	static NvU8 init_cmd1[] = {0xB3,0x04,0x08,0x00,0x22,0x00};
	static NvU8 init_cmd2[] = {0xB4,0x0C};
	static NvU8 init_cmd3[] = {0xB6,0x3A,0xC3};
	static NvU8 init_cmd4[] = {0x2A,0x00,0x00,0x04,0xAF};
	static NvU8 init_cmd5[] = {0x2B,0x00,0x00,0x07,0x7F};
	static NvU8 init_cmd6[] = {0xE6};
	static NvU8 init_cmd7[] = {0x2C};
	
    static DsiPanelData s_PanelInitSeq[] = {
        // Panel Reg Addr, Data Size, Register Data
        {0x05,      0x01,   1, s_ZeroCmd, NV_FALSE},
        {0x00, (NvU32)-1,   5,      NULL, NV_FALSE},
        
        {0x05,      0x11,   1, s_ZeroCmd, NV_FALSE},
        {0x00, (NvU32)-1,   100,    NULL, NV_FALSE},

		{0x23,      0xB0,   1, s_ZeroCmd, NV_FALSE},
		{0x00, (NvU32)-1,   3,      NULL, NV_FALSE},
		{0x29,      0x00,   6, init_cmd1, NV_TRUE},
		{0x00, (NvU32)-1,   3,      NULL, NV_FALSE},
		{0x29,      0x00,   2, init_cmd2, NV_TRUE},
		{0x00, (NvU32)-1,   3,      NULL, NV_FALSE},
		{0x29,      0x00,   3, init_cmd3, NV_TRUE},
		{0x00, (NvU32)-1,	3,		NULL, NV_FALSE},
		
		{0x00, (NvU32)-1,   3,      NULL, NV_FALSE},
		{0x15,      0x53,   1, init_cmd7, NV_FALSE},
		{0x00, (NvU32)-1,   3,      NULL, NV_FALSE},
		
		{0x15,      0x3A,   1, 		0x77, NV_FALSE},
		{0x00, (NvU32)-1,   3,      NULL, NV_FALSE},
		{0x39,      0x00,   5, init_cmd4, NV_TRUE},
		{0x00, (NvU32)-1,   3,      NULL, NV_FALSE},
		{0x39,      0x00,   5, init_cmd5, NV_TRUE},
		{0x00, (NvU32)-1,   3,      NULL, NV_FALSE},
		
		{0x05,      0x29,   1, s_ZeroCmd, NV_FALSE},
        {0x00, (NvU32)-1,  150,      NULL, NV_FALSE},
        {0x05,      0x11,   1, s_ZeroCmd, NV_FALSE},
        {0x00, (NvU32)-1, 120,      NULL, NV_FALSE},
        {0x15,      0x51,   1, init_cmd6, NV_FALSE},
    };
    if (!hDevice)
        return NV_FALSE;
    NV_ASSERT( mode->width && mode->height && mode->bpp );

    if (!hDevice->bInitialized)
    {
        NV_ASSERT(!"device not initialized");
        return NV_FALSE;
    }

    NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayBoard,
                      &DisplayBoardInfo, sizeof(DisplayBoardInfo));
    if (DisplayBoardInfo.BoardInfo.BoardID == BOARD_P1761)
    {
        if (NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayPanel,
            &DisplayPanelInfo, sizeof(DisplayPanelInfo)))
        {
            if (DisplayPanelInfo.DisplayPanelId)  //E1937
                NeedcmdInit = NV_FALSE;
            else
                NeedcmdInit = NV_TRUE;            //E1807
        }
    }
    else if (DisplayBoardInfo.BoardInfo.BoardID == BOARD_E1807)
        NeedcmdInit = NV_TRUE;
    else
        NeedcmdInit = NV_FALSE;

    s_hGpio = NvOdmGpioOpen();
    if (!s_hGpio)
    {
        NV_ASSERT(!"GPIO open failed \n");
        return NV_FALSE;
    }
#if 0
    s_Lcd_rst= NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('h' - 'a') , 3);
    if (!s_Lcd_rst)
    {
        NV_ASSERT(!"GPIO pin acquisition failed \n");
        return NV_FALSE;
    }
#endif
	s_vdd_3v3 = NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('i' - 'a') , 3);
    if (!s_vdd_3v3)
    {
        NV_ASSERT(!"GPIO pin acquisition failed \n");
        return NV_FALSE;
    }

    s_Lcd_bl_en = NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('h' - 'a') , 2);
    if (!s_Lcd_bl_en)
    {
        NV_ASSERT(!"GPIO pin acquisition failed \n");
        return NV_FALSE;
    }
	
	NvOdmOsSleepMS(20);
	s_Lcd_vgh_en= NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('r' - 'a') , 4);
	if (!s_Lcd_vgh_en){
		NvOsDebugPrintf("[pengge]:GPIO pin ra4 acquisition failed\n");
		return NV_FALSE;
	}
#if 1	
	NvOdmOsSleepMS(20);
	vdd_rst_3v3= NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('v' - 'a') , 1);
	if (!vdd_rst_3v3){
		NvOsDebugPrintf("[pengge]:GPIO pin pv1 acquisition failed\n");
		return NV_FALSE;
	}
#endif	
	NvOdmOsSleepMS(20);
    b = s_trans.NvOdmDispDsiInit(hDevice, DSI_INSTANCE, 0);
    if (!b)
    {
        NV_ASSERT("dsi init failed \n");
        return NV_FALSE;
    }

    b = s_trans.NvOdmDispDsiEnableCommandMode(hDevice, DSI_INSTANCE);
    if (!b)
    {
        NvOsDebugPrintf("[pengge]:dsi command mode failed\n");
        return NV_FALSE;
    }else{
		NvOsDebugPrintf("[pengge]:dsi command mode success\n");
	}
	NvOdmOsSleepMS(20);
	NvOdmGpioSetState(s_hGpio, s_vdd_3v3, 0x01);
	NvOdmGpioConfig(s_hGpio, s_vdd_3v3, NvOdmGpioPinMode_Output);
	//NvOdmGpioSetState(s_hGpio, s_vdd_3v3, 0x01);
	NvOdmOsSleepMS(20);

	NvOdmGpioSetState(s_hGpio, s_Lcd_vgh_en, 0x01);
    NvOdmGpioConfig(s_hGpio, s_Lcd_vgh_en, NvOdmGpioPinMode_Output);
	//NvOdmGpioSetState(s_hGpio,s_Lcd_vgh_en,0x01);
	NvOdmOsSleepMS(20);
#if 1
	NvOdmGpioSetState(s_hGpio, vdd_rst_3v3, 0x01);
	NvOdmGpioConfig(s_hGpio, vdd_rst_3v3, NvOdmGpioPinMode_Output);
	//NvOdmGpioSetState(s_hGpio, vdd_rst_3v3, 0x00);
	NvOdmOsSleepMS(20);
	//NvOdmGpioSetState(s_hGpio, vdd_rst_3v3, 0x01);
	//NvOdmOsSleepMS(100);
#endif
	NvOdmGpioSetState(s_hGpio, s_Lcd_bl_en, 0x01);
    NvOdmGpioConfig(s_hGpio, s_Lcd_bl_en, NvOdmGpioPinMode_Output);
	//NvOdmGpioSetState(s_hGpio, s_Lcd_bl_en, 0x01);
	NvOdmOsSleepMS(20);
	/* Send panel Init sequence */
    if (NeedcmdInit)
    {
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
    }



    return NV_TRUE; //returning false till the code is added
}

#endif
