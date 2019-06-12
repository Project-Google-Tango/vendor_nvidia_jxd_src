/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_disp.h"
#include "panel_boe_dsi.h"
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

static NvBool boepanel_Setup( NvOdmDispDeviceHandle hDevice );
static void boepanel_Release( NvOdmDispDeviceHandle hDevice );
static void boepanel_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
                NvOdmDispDeviceMode *modes );
static NvBool boepanel_SetMode( NvOdmDispDeviceHandle hDevice,
                NvOdmDispDeviceMode *mode, NvU32 flags );
static void boepanel_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity );
static void boepanel_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
                NvOdmDispDevicePowerLevel level );
static NvBool boepanel_GetParameter( NvOdmDispDeviceHandle hDevice,
                NvOdmDispParameter param, NvU32 *val );
static void * boepanel_GetConfiguration( NvOdmDispDeviceHandle hDevice );
static void boepanel_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
                NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values );
static NvBool boepanel_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
                NvOdmDispSpecialFunction function, void *config );
static void boepanel_NullMode( NvOdmDispDeviceHandle hDevice );
static void boepanel_Resume( NvOdmDispDeviceHandle hDevice );
static void boepanel_Suspend( NvOdmDispDeviceHandle hDevice );
static NvBool boepanel_PanelInit( NvOdmDispDeviceHandle hDevice, NvOdmDispDeviceMode *mode);

// Color calibration
static NvU32 s_ColorCalibration_Red     = NVODM_DISP_FP_TO_FX(1.00);
static NvU32 s_ColorCalibration_Green   = NVODM_DISP_FP_TO_FX(1.00);
static NvU32 s_ColorCalibration_Blue    = NVODM_DISP_FP_TO_FX(1.00);

// Other global variables.
static NvOdmDispDsiTransport   s_trans;
static NvBool s_bBacklight = NV_FALSE;

/********************************************
 * Please use byte clock for the following table
 * Byte Clock to Pixel Clock conversion:
 * The DSI D-PHY calculation:
 * h_total = (h_sync_width + h_back_porch + h_disp_active + h_front_porch)
 * v_total = (v_sync_width + v_back_porch + v_disp_active + v_front_porch)
 * dphy_clk = h_total * v_total * fps * bpp / data_lanes / 2
 ********************************************/
static NvOdmDispDeviceMode boepanel_Modes[] =
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
            2,   // h_ref_to_sync,
            2,   // v_ref_to_sync,
            10,   // h_sync_width,
            3,   // v_sync_width,
            48,  // h_back_porch,
            11,   // v_back_porch
            720,   // h_disp_active
            1280,   // v_disp_active
            96,  // h_front_porch,
            8,  // v_front_porch
        },
        NV_FALSE // deprecated.
    }
};

static NvBool
boepanel_Setup( NvOdmDispDeviceHandle hDevice )
{
    if (!hDevice)
        return NV_FALSE;

    if( !hDevice->bInitialized )
    {
        hDevice->modes = boepanel_Modes;
        hDevice->nModes = NV_ARRAY_SIZE(boepanel_Modes);

        hDevice->Type = NvOdmDispDeviceType_DSI;
        hDevice->Usage = NvOdmDispDeviceUsage_Primary;
        hDevice->MaxHorizontalResolution = (NvU32)boepanel_Modes[0].width;
        hDevice->MaxVerticalResolution = (NvU32)boepanel_Modes[0].height;
        hDevice->BaseColorSize = NvOdmDispBaseColorSize_888;
        hDevice->DataAlignment = NvOdmDispDataAlignment_MSB;
        hDevice->PinMap = NvOdmDispPinMap_Single_Rgb24_Spi5;

        hDevice->power = NvOdmDispDevicePowerLevel_Off;
        boepanel_NullMode( hDevice );

        hDevice->bInitialized = NV_TRUE;
    }

    return NV_TRUE;
}

static void
boepanel_NullMode( NvOdmDispDeviceHandle hDevice )
{
    if (!hDevice)
        return ;

    hDevice->CurrentMode.width = 0;
    hDevice->CurrentMode.height = 0;
    hDevice->CurrentMode.bpp = 0;
    hDevice->CurrentMode.flags = (NvU32)-1;
}

static void
boepanel_Release( NvOdmDispDeviceHandle hDevice )
{
    if (!hDevice)
        return ;

    hDevice->bInitialized = NV_FALSE;
    boepanel_NullMode( hDevice );
}

static void
boepanel_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
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
boepanel_SetMode( NvOdmDispDeviceHandle hDevice,
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
        boepanel_NullMode( hDevice );
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
        boepanel_PanelInit(hDevice, mode);
    }
    else
    {
        boepanel_NullMode(hDevice);
    }
    return NV_TRUE;
}


static void
boepanel_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    if( !s_bBacklight )
    {
        NvOdmBacklightInit( hDevice, NV_FALSE );
        s_bBacklight = NV_TRUE;
    }

    NvOdmBacklightIntensity( hDevice, intensity );

}


static void
boepanel_SetBacklightIntensity( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    //To Do: Add PWM support

}


static void
boepanel_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level )
{
    NV_ASSERT( hDevice );
    if (!hDevice)
        return ;

    hDevice->power = level;

    switch( level ) {
    case NvOdmDispDevicePowerLevel_Off:
    case NvOdmDispDevicePowerLevel_Suspend:
        boepanel_Suspend( hDevice );
        break;
    case NvOdmDispDevicePowerLevel_On:
        if( hDevice->CurrentMode.width && hDevice->CurrentMode.height &&
            hDevice->CurrentMode.bpp )
        {
            boepanel_Resume( hDevice );
        }
        break;
    default:
        break;
    }
}

static NvBool
boepanel_GetParameter( NvOdmDispDeviceHandle hDevice,
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
boepanel_GetConfiguration( NvOdmDispDeviceHandle hDevice )
{
    NvOdmDispDsiConfig *dsi = NULL;
    if (!hDevice)
        return NULL;

    dsi = &hDevice->Config.dsi;
    dsi->DataFormat = NvOdmDsiDataFormat_24BitP;
    dsi->VideoMode = NvOdmDsiVideoModeType_Burst;
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
    dsi->HsClockControl = NvOdmDsiHsClock_TxOnly;
    dsi->EnableHsClockInLpMode = NV_FALSE;
    dsi->bIsPhytimingsValid = NV_FALSE;
    dsi->DsiDisplayMode = NvOdmDispDsiMode_Normal;
    return (void *)dsi;
}

static void
boepanel_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values )
{
    *nPins = 0;
}

static NvBool
boepanel_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
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
boepanel_Resume( NvOdmDispDeviceHandle hDevice )
{
    boepanel_PanelInit( hDevice, &hDevice->CurrentMode );
}

static void
boepanel_Suspend( NvOdmDispDeviceHandle hDevice )
{
    //Not a valid case in BL
}

static NvOdmServicesGpioHandle s_hGpio;
static NvOdmGpioPinHandle s_hbl_en;

static NvU8 s_SleepOut[] = {0x11};
static NvU8 s_SetPassword[] = {0xB9, 0xFF, 0x83, 0x94};
static NvU8 s_MipiCtrl[] = {0xBA, 0x13};   // 4 data lane and short (why short??)
static NvU8 s_SetPower[] = {0xB1, 0x7C, 0x00, 0x24, 0x09, 0x01, 0x11, 0x11, 0x36, 0x3E, 0x26, 0x26, 0x57, 0x02, 0x01, 0xE6};
static NvU8 s_SetCYC[] = {0xB4, 0x00, 0x00, 0x00, 0x05, 0x06, 0x41, 0x42, 0x02, 0x41, 0x42, 0x43, 0x47, 0x19, 0x58, 0x60, 0x08, 0x05, 0x10};
static NvU8 s_SetGip[] = {0xd5, 0x4c, 0x01, 0x00, 0x01, 0xcd, 0x23, 0xef, 0x45, 0x67, 0x89, 0xab, 0x11, 0x00, 0xdc,
                                0x10, 0xfe, 0x32, 0xba, 0x98, 0x76, 0x54, 0x00, 0x11, 0x40};
static NvU8 s_SetPanel[] = {0xcc, 0x09};
static NvU8 s_Commands[] = {0xc7, 0x00, 0x20};
static NvU8 s_Rgamma[] = {0xe0, 0x24, 0x33, 0x36, 0x3f, 0x3f, 0x3f, 0x3c, 0x56, 0x05, 0x0c, 0x0e,
                                0x11, 0x13, 0x12, 0x14, 0x12, 0x1e, 0x24, 0x33, 0x36, 0x3f, 0x3f,
                                0x3f, 0x3c, 0x56, 0x05, 0x0c, 0x0e, 0x11, 0x13, 0x12, 0x14, 0x12,
                                0x1e};
static NvU8 s_DispOn[] = {0x29};

static NvBool
boepanel_PanelInit( NvOdmDispDeviceHandle hDevice, NvOdmDispDeviceMode *mode)
{
    NvU32 seqSize;
    NvU32 i;
    NvBool b;

    static DsiPanelData s_PanelInitSeq[] = {
          //DsiCmd, PanelReg, DataSize, PData, IsLongPacket
          {0x23, 0x05,  1, s_SleepOut, NV_FALSE},
          {0x00, -1,  200, NULL, NV_FALSE},             /* wait 200ms */
          {0x29, 0x39,  4, s_SetPassword, NV_TRUE},
          {0x23, 0x15,  2, s_MipiCtrl, NV_TRUE},
          {0x29, 0x39,  16, s_SetPower, NV_TRUE},
          {0x29, 0x39,  19, s_SetCYC, NV_TRUE},
          {0x29, 0x39,  25, s_SetGip, NV_TRUE},
          {0x23, 0x15,  2, s_SetPanel, NV_FALSE},
          {0x29, 0x39,  3, s_Commands, NV_TRUE},
          {0x29, 0x39,  35, s_Rgamma, NV_TRUE},
          {0x00, -1,  5, NULL, NV_FALSE},                /* wait 5ms */
          {0x23, 0x05,  1, s_DispOn, NV_FALSE},
          {0x00, -1, 50, NULL, NV_FALSE},                /* wait 50ms */
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

    s_hbl_en = NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('h' - 'a'), 2);
    if (!s_hbl_en)
    {
        NV_ASSERT( !"Pin handle could not be acquired");
        return NV_FALSE;
    }
    /* On DSIa backlight */
    NvOdmGpioSetState(s_hGpio, s_hbl_en, 0x1);
    NvOdmGpioConfig( s_hGpio, s_hbl_en, NvOdmGpioPinMode_Output);

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
boepanel_GetHal(NvOdmDispDeviceHandle hDevice)
{
    NV_ASSERT(hDevice);
    if (!hDevice)
        return;

    hDevice->Setup = boepanel_Setup;
    hDevice->Release = boepanel_Release;
    hDevice->ListModes = boepanel_ListModes;
    hDevice->SetMode = boepanel_SetMode;
    hDevice->SetPowerLevel = boepanel_SetPowerLevel;
    hDevice->GetParameter = boepanel_GetParameter;
    hDevice->GetPinPolarities = boepanel_GetPinPolarities;
    hDevice->GetConfiguration = boepanel_GetConfiguration;
    hDevice->SetSpecialFunction = boepanel_SetSpecialFunction;
    hDevice->SetBacklight = boepanel_SetBacklight;
    hDevice->BacklightIntensity = boepanel_SetBacklightIntensity;
}

