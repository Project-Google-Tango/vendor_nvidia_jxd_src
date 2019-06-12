/*
 * Copyright (c) 2008 - 2012 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_disp.h"
#include "nvodm_backlight.h"
#include "panel_sharp_dsi.h"
#include "nvodm_services.h"
#include "nvodm_query_pinmux.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query_gpio.h"
#include "nvos.h"

#if defined(DISP_OAL)
#include "nvbl_debug.h"
#else
#define DSI_PANEL_DEBUG(x) NvOsDebugPrintf x
#endif
#define DSI_PANEL_DEBUG(x) NvOsDebugPrintf x

//set to 0 for 1st DSI instance and 1 for 2nd DSI instance
#define DSI_INSTANCE 0

// FIXME: Enabling the DC driven command mode by default.
// Need to set mode based on ODM query
#define NVODM_DC_DRIVEN_COMMAND_MODE    0

// Turtle and Pure color calibration
static NvU32 s_ColorCalibration_Red[2] = 
{
    NVODM_DISP_FP_TO_FX(1.0f), // hvga
    NVODM_DISP_FP_TO_FX(1.0f) // qvga
};

static NvU32 s_ColorCalibration_Green[2] =
{
    NVODM_DISP_FP_TO_FX(1.00f), // hvga
    NVODM_DISP_FP_TO_FX(0.8000f) // qvga
};

static NvU32 s_ColorCalibration_Blue[2] =
{
    NVODM_DISP_FP_TO_FX(1.0f), // hvga
    NVODM_DISP_FP_TO_FX(0.5166f) // qvga
};

static NvBool s_UseHvga = NV_TRUE;
#define CALIBRATION_INDEX (( s_UseHvga ) ? 0 : 1)

/********************************************
 * Please use byte clock for the following table
 * Byte Clock to Pixel Clock conversion:
 * The DSI D-PHY calculation:
 * h_total = (h_sync_width + h_back_porch + h_disp_active + h_front_porch)
 * v_total = (v_sync_width + v_back_porch + v_disp_active + v_front_porch)
 * dphy_clk = h_total * v_total * fps * bpp / data_lanes / 2
 ********************************************/
static NvOdmDispDeviceMode sharpdsi_modes_hvga[] = 
{
    // DSI, HVGA, 24 bit
    {
        320,                          // width
        480,                          // height    
        24,                           // bpp
        0,                            // refresh
        0,                            // frequency
        NVODM_DISP_MODE_FLAG_NONE,    // flags
        // timing
        {
            4,   // h_ref_to_sync,
            1,   // v_ref_to_sync,
            4,   // h_sync_width,
            1,   // v_sync_width,
            64,  // h_back_porch,
            1,   // v_back_porch
            320,   // h_disp_active
            480,   // v_disp_active
            64,  // h_front_porch,
            10,  // v_front_porch
        },
        NV_FALSE // deprecated.
    },

    // DSI Command Mode, HVGA, 24 bit
    {
        320,                          // width
        480,                          // height    
        24,                           // bpp
        0,                            // refresh
        0,                            // frequency
        NVODM_DISP_MODE_FLAG_PARTIAL, // flags
        // timing
        {
            4,   // h_ref_to_sync,
            1,   // v_ref_to_sync,
            4,   // h_sync_width,
            1,   // v_sync_width,
            64,  // h_back_porch,
            1,   // v_back_porch
            320,   // h_disp_active
            480,   // v_disp_active
            64,  // h_front_porch,
            10,  // v_front_porch
        },
        NV_TRUE  // deprecated
    },
};

static NvOdmDispDeviceMode sharpdsi_modes_qvga[] = 
{
    // DSI, QVGA, 24 bit
    {
        320,                          // width
        240,                          // height
        24,                           // bpp
        0,                            // refresh
        0,                            // frequency
        NVODM_DISP_MODE_FLAG_NONE,    // flags
        // timing
        {
            4,   // h_ref_to_sync,
            1,   // v_ref_to_sync,
            4,   // h_sync_width,
            1,   // v_sync_width,
            64,  // h_back_porch,
            1,   // v_back_porch
            320, // h_disp_active
            240, // v_disp_active
            64,  // h_front_porch,
            10,  // v_front_porch
        },
        NV_FALSE // partial
    },

    // DSI Command Mode, QVGA, 24 bit
    {
        320,                          // width
        240,                          // height
        24,                           // bpp
        0,                            // refresh
        0,                            // frequency
        NVODM_DISP_MODE_FLAG_PARTIAL, // flags
        // timing
        {
            4,   // h_ref_to_sync,
            1,   // v_ref_to_sync,
            4,   // h_sync_width,
            1,   // v_sync_width,
            64,  // h_back_porch,
            1,   // v_back_porch
            320, // h_disp_active
            240, // v_disp_active
            64,  // h_front_porch,
            10,  // v_front_porch
        },
        NV_TRUE, // deprecated
    },

};

NvOdmDispDsiTransport s_trans;
NvOdmServicesGpioHandle s_hGpio;
NvOdmGpioPinHandle s_hLCDResPin;
static NvBool s_bBacklight = NV_FALSE;

typedef struct DsiPanelDataRec
{
    NvU32 DsiCmd;
    NvU32 PanelReg;
    NvU32 DataSize;
    NvU8 *pData;
    NvBool IsLongPacket;
} DsiPanelData;

static NvU8 s_EnterSleepMode[] = {0x00};
static NvU8 s_ExitSleepMode[] = {0x00};
static NvU8 s_PanelDisplayOn[] = {0x00};
static NvU8 s_PanelDisplayOff[] = {0x00};
static NvU8 s_EnterCommandMode[] = {0x00};
static NvU8 s_EnterVideoMode[] = {0x21};
static NvU8 s_TearOn[] = {0x00};

static NvU8 s_Manufacturer_Enable[] = {0x00};
static NvU8 s_Manufacturer_Disable[] = {0x02};
static NvU8 s_Manufacturer_E2h[] = {0x1F};
static NvU8 s_Manufacturer_B3h[] = {0x00,0x80};
static NvU8 s_Manufacturer_B6h[] = {0x01};
static NvU8 s_Manufacturer_B8h[] = {0x00,0x0F,0x07,0xFF,0xFF,0xC8,0xE4,0x0F,
                                    0x18,0x90,0x90,0x37,0x5A,0x87,0xBE};
static NvU8 s_Manufacturer_B9h[] = {0x00,0xFF,0x00,0x00};
static NvU8 s_Manufacturer_BEh[] = {0x10};
static NvU8 s_Manufacturer_C0h[] = {0x01,0x5D,0x02,0x0C,0x30};
static NvU8 s_Manufacturer_C1h[] = {0x8A,0x00,0x00,0x00};
static NvU8 s_Manufacturer_C2h[] = {0x8A,0x00,0x00,0x00};
static NvU8 s_Manufacturer_C3h[] = {0x8A,0x00,0x00,0x00};
static NvU8 s_Manufacturer_C5h[] = {0x9D,0x07,0x7D,0x08,0x87,0x0D,0x0F,0x04,
                                    0x04,0x00,0x04,0x90};
static NvU8 s_Manufacturer_C6h[] = {0x03,0x61};
static NvU8 s_Manufacturer_C8h[] = {0x29,0x46,0xEE,0xFA,0xD3,0x14,0x61,0x90,
                                    0x10,0x84,0x77,0x64,0x81,0xCA,0x05,0x90};
static NvU8 s_Manufacturer_C9h[] = {0x29,0x46,0xEE,0xFA,0xD3,0x14,0x61,0x90,
                                    0x10,0x84,0x77,0x64,0x81,0xCA,0x05,0x90};
static NvU8 s_Manufacturer_CAh[] = {0x29,0x46,0xEE,0xFA,0xD3,0x14,0x61,0x90,
                                    0x10,0x84,0x77,0x64,0x81,0xCA,0x05,0x90};
static NvU8 s_Manufacturer_D0h[] = {0x33,0x02,0x41,0x00};
static NvU8 s_Manufacturer_D2h[] = {0xB3,0x00,0xA8};
static NvU8 s_Manufacturer_D3h[] = {0x33,0x03};
static NvU8 s_Manufacturer_D4h[] = {0x33,0x43};
static NvU8 s_Manufacturer_D5h[] = {0x03,0x00,0x08,0x6F,0x1A,0x00};
static NvU8 s_Manufacturer_D6h[] = {0x01,0x20,0x5A};
static NvU8 s_Manufacturer_D7h[] =
    {0x00,0x60,0x70,0xA8,0xCA,0xAC,0xAA,0x8A,0xC6};
static NvU8 s_Manufacturer_D9h[] = {0x7D,0x0B,0xFF};

static NvU8 s_Manufacturer_B3h_qvga[] = {0x10,0x80};
static NvU8 s_Manufacturer_B8h_qvga[] =
    { 0x01,0x0F,0x07,0xFF,0xFF,0xC8,0xE4,0x0F, 0x18,0x90,0x90,0x37,0x5A,0x87,
      0xBE 
    };
static NvU8 s_Manufacturer_B9h_qvga[] = {0x01,0xFF,0x00,0x18};
static NvU8 s_Manufacturer_C0h_qvga[] = {0x01,0x6E,0xE2,0x1C,0x30};
static NvU8 s_Manufacturer_C1h_qvga[] = {0xCA,0x00,0x00,0x00};
static NvU8 s_Manufacturer_C2h_qvga[] = {0xCA,0x00,0x00,0x0F};
static NvU8 s_Manufacturer_C3h_qvga[] = {0xCA,0x00,0x00,0x0F};
static NvU8 s_Manufacturer_C5h_qvga[] = // not sure what this is
    { 0x99,0x07,0x7D,0x08,0x82,0x0C,0x10,0x84, 0x04,0x00,0x04,0x90,0x00,0x00 };
static NvU8 s_Manufacturer_C8h_qvga[] =
    { 0x11,0xCF,0x6A,0xF7,0x51,0x4A,0x76,0x90,0x11,0xCD,0x6A,0xF7,0x59,0xCA,
      0x36,0x90
    };
static NvU8 s_Manufacturer_C9h_qvga[] =
    { 0x11,0xCF,0x6A,0xF7,0x51,0x4A,0x76,0x90,0x11,0xCD,0x6A,0xF7,0x59,0xCA,
      0x36,0x90
    };
static NvU8 s_Manufacturer_CAh_qvga[] =
    { 0x11,0xCF,0x6A,0xF7,0x51,0x4A,0x76,0x90,0x11,0xCD,0x6A,0xF7,0x59,0xCA,
      0x36,0x90
    };
static NvU8 s_Manufacturer_D0h_qvga[] = {0x33,0x02,0x51,0x00};
static NvU8 s_Manufacturer_D5h_qvga[] = {0x03,0x00,0x08,0x60,0x21,0x00};
static NvU8 s_Manufacturer_D6h_qvga[] = {0x01,0x20,0x1A};

#if NVODM_DC_DRIVEN_COMMAND_MODE
static NvU8 s_PixelDataFormat[] = {0x07};
static NvU8 s_AddressMode[] = {0x00};
static NvU8 s_ColumnAddress[] = {0x00, 0x00, 0x01, 0x3F};
static NvU8 s_PageAddress[] = {0x00, 0x00, 0x01, 0xDF};
#endif

static NvBool sharpdsi_panel_init( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDeviceMode *mode);
static NvBool sharpdsi_discover( NvOdmDispDeviceHandle hDevice );
static void sharpdsi_suspend( NvOdmDispDeviceHandle hDevice );
static void sharpdsi_resume( NvOdmDispDeviceHandle hDevice );

static void
sharpdsi_nullmode( NvOdmDispDeviceHandle hDevice )
{
    hDevice->CurrentMode.width = 0;
    hDevice->CurrentMode.height = 0;
    hDevice->CurrentMode.bpp = 0;
    hDevice->CurrentMode.flags = -1;
}

NvBool sharpdsi_Setup( NvOdmDispDeviceHandle hDevice )
{
    if( !hDevice->Init )
    {
        hDevice->Type = NvOdmDispDeviceType_DSI;
        hDevice->Usage = NvOdmDispDeviceUsage_Unassigned;
        hDevice->MaxHorizontalResolution = (s_UseHvga) ? 
            sharpdsi_modes_hvga[0].width : sharpdsi_modes_qvga[0].width;
        hDevice->MaxVerticalResolution = (s_UseHvga) ? 
            sharpdsi_modes_hvga[0].height : sharpdsi_modes_qvga[0].height;
        hDevice->BaseColorSize = NvOdmDispBaseColorSize_888;
        hDevice->DataAlignment = NvOdmDispDataAlignment_MSB;
        hDevice->PinMap = NvOdmDispPinMap_Single_Rgb24_Spi5;
        hDevice->PwmOutputPin = NvOdmDispPwmOutputPin_Unspecified;

        if( s_UseHvga )
        {
            hDevice->modes = sharpdsi_modes_hvga;
            hDevice->nModes = NV_ARRAY_SIZE(sharpdsi_modes_hvga);
        }
        else
        {
            hDevice->modes = sharpdsi_modes_qvga;
            hDevice->nModes = NV_ARRAY_SIZE(sharpdsi_modes_qvga);
        }

        hDevice->CurrentPower = NvOdmDispDevicePowerLevel_Off;

        sharpdsi_nullmode(hDevice);

        // Get GPIO controller handle
        s_hGpio = NvOdmGpioOpen();
        if( !s_hGpio )
        {
            goto cleanup;
        }

        hDevice->Init = NV_TRUE;
    }

    return NV_TRUE;

cleanup:
    return NV_FALSE;
}

void sharpdsi_Release( NvOdmDispDeviceHandle hDevice )
{
    NvOdmBacklightIntensity( hDevice, 0 );
    NvOdmBacklightDeinit(hDevice);

    s_bBacklight = NV_FALSE;
    hDevice->Init = NV_FALSE;

    if( s_hLCDResPin )
    {
        NvOdmGpioReleasePinHandle( s_hGpio, s_hLCDResPin);
        s_hLCDResPin = NULL;
    }

    if( s_hGpio )
    {
        NvOdmGpioClose( s_hGpio );
        s_hGpio = NULL;
    }

    sharpdsi_nullmode( hDevice );
}

void sharpdsi_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
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

NvBool sharpdsi_SetMode( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDeviceMode *mode, NvU32 flag)
{
    NV_ASSERT( hDevice );

    if( mode )
    {
        hDevice->CurrentMode = *mode;
    }
    else
    {
        sharpdsi_nullmode( hDevice );        
    }

    if( hDevice->CurrentPower != NvOdmDispDevicePowerLevel_On )
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

        sharpdsi_panel_init( hDevice, mode );
        
        if( !s_bBacklight && !NvOdmBacklightInit( hDevice, 0 ) )
        {
            return NV_FALSE;
        }

        NvOdmBacklightIntensity(hDevice, 0xFF);
        s_bBacklight = NV_TRUE;
    }
    else
    {
        // FIXME: send power off sequence
        if( !s_bBacklight )
        {
            if( !NvOdmBacklightInit( hDevice, NV_FALSE ) )
            {
                return NV_FALSE;
            }
            s_bBacklight = NV_TRUE;
        }
        NvOdmBacklightIntensity( hDevice, 0 );
    }
    
    return NV_TRUE;
}

void sharpdsi_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    if( !s_bBacklight )
    {
        NvOdmBacklightInit( hDevice, NV_FALSE );
        s_bBacklight = NV_TRUE;
    }

    NvOdmBacklightIntensity( hDevice, intensity );
}

void sharpdsi_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level )
{
    NV_ASSERT( hDevice );

    hDevice->CurrentPower = level;

    switch( level ) {
    case NvOdmDispDevicePowerLevel_Off:
    case NvOdmDispDevicePowerLevel_Suspend:
        sharpdsi_suspend( hDevice );
        break;
    case NvOdmDispDevicePowerLevel_On:
        if( hDevice->CurrentMode.width && hDevice->CurrentMode.height &&
            hDevice->CurrentMode.bpp )
        {
            sharpdsi_resume( hDevice );
        }
        break;
    default:
        break;
    }
}

NvBool sharpdsi_GetParameter( NvOdmDispDeviceHandle hDevice,
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
    case NvOdmDispParameter_ColorCalibrationRed:
        *val = s_ColorCalibration_Red[ CALIBRATION_INDEX ];
        break;
    case NvOdmDispParameter_ColorCalibrationGreen:
        *val = s_ColorCalibration_Green[ CALIBRATION_INDEX ];
        break;
    case NvOdmDispParameter_ColorCalibrationBlue:
        *val = s_ColorCalibration_Blue[ CALIBRATION_INDEX ];
        break;
    case NvOdmDispParameter_PwmOutputPin:
        *val = NvOdmDispPwmOutputPin_Unspecified;
        break;
    default:
        return NV_FALSE;
    }

    return NV_TRUE;
}

void *
sharpdsi_GetConfiguration( NvOdmDispDeviceHandle hDevice )
{
    NvOdmDispDsiConfig *dsi = &hDevice->Config.dsi;
    dsi->DataFormat = NvOdmDsiDataFormat_24BitP;
    dsi->VideoMode = NvOdmDsiVideoModeType_NonBurst;
    dsi->VirChannel = NvOdmDsiVirtualChannel_0;
    dsi->NumOfDataLines = 2;
    dsi->RefreshRate = 60;
    dsi->Freq = 80220;
    dsi->PanelResetTimeoutMSec = 202;
    dsi->LpCommandModeFreqKHz = 20000;
    dsi->HsCommandModeFreqKHz = 50000;
    dsi->HsSupportForFrameBuffer = NV_TRUE;
    dsi->flags = 0;
    dsi->HsClockControl = NvOdmDsiHsClock_TxOnly;
    dsi->EnableHsClockInLpMode = NV_FALSE;
    dsi->bIsPhytimingsValid = NV_FALSE;
    dsi->DsiDisplayMode = NvOdmDispDsiMode_Normal;
    return (void *)dsi;
}

void
sharpdsi_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values )
{
    *nPins = 0;
}

NvBool
sharpdsi_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config )
{
    switch( function ) {
    case NvOdmDispSpecialFunction_PartialMode:
        // FIXME: this panel supports partial mode, implement
        return NV_FALSE;
    case NvOdmDispSpecialFunction_DSI_Transport:
        s_trans = *(NvOdmDispDsiTransport *)config;
        break;
    default:
        NV_ASSERT( !"unknown special function" );
        return NV_FALSE;
    }

    return NV_TRUE;
}

static NvBool
sharpdsi_discover( NvOdmDispDeviceHandle hDevice )
{
    NvOdmPeripheralConnectivity const *conn;

    /* get the connectivity info */
    conn = NvOdmPeripheralGetGuid( SHARP_DSI_GUID );
    if( !conn )
    {
        return NV_FALSE;
    }

    hDevice->PeripheralConnectivity = conn;

    /* FIXME: add GPIO check here */

    return NV_TRUE;
}

static void
sharpdsi_resume( NvOdmDispDeviceHandle hDevice )
{
    sharpdsi_panel_init( hDevice, &hDevice->CurrentMode );
}

static void
sharpdsi_suspend( NvOdmDispDeviceHandle hDevice )
{
    static DsiPanelData s_PanelOffSeq[] =
    {
        { 0x05, 0x28,  1, s_PanelDisplayOff, NV_FALSE},
        { 0x23, 0xB4,  1, s_EnterCommandMode, NV_FALSE },
        { 0x00, -1,  30, NULL, NV_FALSE}, /* wait 30ms (> 1 frame) */
        { 0x05, 0x10,  1, s_EnterSleepMode, NV_FALSE},
        { 0x00, -1,  120, NULL, NV_FALSE}, /* wait 120ms */
    };

    NvOdmServicesPmuHandle hPmu;
    NvOdmPeripheralConnectivity const *conn;
    int SeqList;
    int i = 0;
    NvBool b;

    // configure the dsi for command mode
    s_trans.NvOdmDispDsiEnableCommandMode( hDevice, DSI_INSTANCE );

    SeqList = NV_ARRAY_SIZE(s_PanelOffSeq);
    for (i = 0; i < SeqList; i++)
    {
        if (s_PanelOffSeq[i].pData == NULL) 
        {
            NvOdmOsSleepMS(s_PanelOffSeq[i].DataSize);
            continue;
        } 

        b = s_trans.NvOdmDispDsiWrite( hDevice,
                s_PanelOffSeq[i].DsiCmd,
                s_PanelOffSeq[i].PanelReg,
                s_PanelOffSeq[i].DataSize,
                s_PanelOffSeq[i].pData,
                DSI_INSTANCE,
                s_PanelOffSeq[i].IsLongPacket );
        if( !b )
        {
            DSI_PANEL_DEBUG(("Error> DSI Panel Suspend Failed\r\n"));
        }
    }

    NvOdmGpioSetState( s_hGpio, s_hLCDResPin, 0x0 );
    NvOdmGpioConfig( s_hGpio,s_hLCDResPin, NvOdmGpioPinMode_Output);

    conn = hDevice->PeripheralConnectivity;

    hPmu = NvOdmServicesPmuOpen();
    for( i = 0; i < (int) conn->NumAddress; i++ )
    {
        if( conn->AddressList[i].Interface == NvOdmIoModule_Vdd )
        {
            NvOdmServicesPmuSetVoltage( hPmu,
                conn->AddressList[i].Address, NVODM_VOLTAGE_OFF, 0 );
        }
    }

    NvOdmServicesPmuClose( hPmu );
}

static NvBool
sharpdsi_panel_init( NvOdmDispDeviceHandle hDevice, NvOdmDispDeviceMode *mode)
{
    static DsiPanelData s_PanelInitSeq_HVGA_CmdMode[] = {
        // Panel Reg Addr, Data Size, Register Data
        { 0x23, 0xB0,  1, s_Manufacturer_Enable, NV_FALSE },
        { 0x23, 0xE2,  1, s_Manufacturer_E2h, NV_FALSE },
        { 0x29, 0xB3,  2, s_Manufacturer_B3h, NV_TRUE },
        { 0x23, 0xB6,  1, s_Manufacturer_B6h, NV_FALSE },
        { 0x29, 0xB8, 15, s_Manufacturer_B8h, NV_TRUE },
        { 0x29, 0xB9,  4, s_Manufacturer_B9h, NV_TRUE },
        { 0x23, 0xBE,  1, s_Manufacturer_BEh, NV_FALSE },
        { 0x29, 0xC0,  5, s_Manufacturer_C0h, NV_TRUE },
        { 0x29, 0xC1,  4, s_Manufacturer_C1h, NV_TRUE },
        { 0x29, 0xC2,  4, s_Manufacturer_C2h, NV_TRUE },
        { 0x29, 0xC3,  4, s_Manufacturer_C3h, NV_TRUE },
        { 0x29, 0xC5, 12, s_Manufacturer_C5h, NV_TRUE },
        { 0x29, 0xC6,  2, s_Manufacturer_C6h, NV_TRUE },
        { 0x29, 0xC8, 16, s_Manufacturer_C8h, NV_TRUE },
        { 0x29, 0xC9, 16, s_Manufacturer_C9h, NV_TRUE },
        { 0x29, 0xCA, 16, s_Manufacturer_CAh, NV_TRUE },
        { 0x29, 0xD0,  4, s_Manufacturer_D0h, NV_TRUE },
        { 0x29, 0xD2,  3, s_Manufacturer_D2h, NV_TRUE },
        { 0x29, 0xD3,  2, s_Manufacturer_D3h, NV_TRUE },
        { 0x29, 0xD4,  2, s_Manufacturer_D4h, NV_TRUE },
        { 0x29, 0xD5,  6, s_Manufacturer_D5h, NV_TRUE },
        { 0x29, 0xD6,  3, s_Manufacturer_D6h, NV_TRUE },
        { 0x29, 0xD7,  9, s_Manufacturer_D7h, NV_TRUE },
        { 0x29, 0xD9,  3, s_Manufacturer_D9h, NV_TRUE },
        { 0x23, 0xB0,  1, s_Manufacturer_Disable, NV_FALSE },
        { 0x05, 0x11,  1, s_ExitSleepMode, NV_FALSE},
        { 0x00, -1,  120, NULL, NV_FALSE},              /* wait 120ms */
        { 0x23, 0xB4,  1, s_EnterCommandMode, NV_FALSE },
        { 0x05, 0x29,  1, s_PanelDisplayOn, NV_FALSE},
        { 0x39, 0x35,  1, s_TearOn, NV_TRUE},
    };

    static DsiPanelData s_PanelInitSeq_HVGA[] = {
        // Panel Reg Addr, Data Size, Register Data
        { 0x23, 0xB0,  1, s_Manufacturer_Enable, NV_FALSE },
        { 0x23, 0xE2,  1, s_Manufacturer_E2h, NV_FALSE },
        { 0x29, 0xB3,  2, s_Manufacturer_B3h, NV_TRUE },
        { 0x23, 0xB6,  1, s_Manufacturer_B6h, NV_FALSE },
        { 0x29, 0xB8, 15, s_Manufacturer_B8h, NV_TRUE },
        { 0x29, 0xB9,  4, s_Manufacturer_B9h, NV_TRUE },
        { 0x23, 0xBE,  1, s_Manufacturer_BEh, NV_FALSE },
        { 0x29, 0xC0,  5, s_Manufacturer_C0h, NV_TRUE },
        { 0x29, 0xC1,  4, s_Manufacturer_C1h, NV_TRUE },
        { 0x29, 0xC2,  4, s_Manufacturer_C2h, NV_TRUE },
        { 0x29, 0xC3,  4, s_Manufacturer_C3h, NV_TRUE },
        { 0x29, 0xC5, 12, s_Manufacturer_C5h, NV_TRUE },
        { 0x29, 0xC6,  2, s_Manufacturer_C6h, NV_TRUE },
        { 0x29, 0xC8, 16, s_Manufacturer_C8h, NV_TRUE },
        { 0x29, 0xC9, 16, s_Manufacturer_C9h, NV_TRUE },
        { 0x29, 0xCA, 16, s_Manufacturer_CAh, NV_TRUE },
        { 0x29, 0xD0,  4, s_Manufacturer_D0h, NV_TRUE },
        { 0x29, 0xD2,  3, s_Manufacturer_D2h, NV_TRUE },
        { 0x29, 0xD3,  2, s_Manufacturer_D3h, NV_TRUE },
        { 0x29, 0xD4,  2, s_Manufacturer_D4h, NV_TRUE },
        { 0x29, 0xD5,  6, s_Manufacturer_D5h, NV_TRUE },
        { 0x29, 0xD6,  3, s_Manufacturer_D6h, NV_TRUE },
        { 0x29, 0xD7,  9, s_Manufacturer_D7h, NV_TRUE },
        { 0x29, 0xD9,  3, s_Manufacturer_D9h, NV_TRUE },
        { 0x23, 0xB0,  1, s_Manufacturer_Disable, NV_FALSE },
        { 0x05, 0x11,  1, s_ExitSleepMode, NV_FALSE},
        { 0x00, -1,  120, NULL, NV_FALSE},              /* wait 120ms */
#if NVODM_DC_DRIVEN_COMMAND_MODE
        { 0x23, 0xB4,  1, s_EnterCommandMode, NV_FALSE },
        { 0x05, 0x29,  1, s_PanelDisplayOn, NV_FALSE},

        { 0x39, 0x3A,  1, s_PixelDataFormat, NV_TRUE},
        { 0x39, 0x36,  1, s_AddressMode, NV_TRUE},
        { 0x39, 0x2A,  1, s_ColumnAddress, NV_TRUE},
        { 0x39, 0x2B,  1, s_PageAddress, NV_TRUE},
#else
        { 0x23, 0xB4,  1, s_EnterVideoMode, NV_FALSE },
        { 0x05, 0x29,  1, s_PanelDisplayOn, NV_FALSE},
#endif
        { 0x39, 0x35,  1, s_TearOn, NV_TRUE},
    };

    static DsiPanelData s_PanelInitSeq_QVGA[] = {
        // Panel Reg Addr, Data Size, Register Data
        { 0x23, 0xB0,  1, s_Manufacturer_Enable, NV_FALSE },
        { 0x23, 0xE2,  1, s_Manufacturer_E2h, NV_FALSE },
        { 0x29, 0xB3,  2, s_Manufacturer_B3h_qvga, NV_TRUE },
        { 0x23, 0xB6,  1, s_Manufacturer_B6h, NV_FALSE },
        { 0x29, 0xB8, 15, s_Manufacturer_B8h_qvga, NV_TRUE },
        { 0x29, 0xB9,  4, s_Manufacturer_B9h_qvga, NV_TRUE },
        { 0x23, 0xBE,  1, s_Manufacturer_BEh, NV_FALSE },
        { 0x29, 0xC0,  5, s_Manufacturer_C0h_qvga, NV_TRUE },
        { 0x29, 0xC1,  4, s_Manufacturer_C1h_qvga, NV_TRUE },
        { 0x29, 0xC2,  4, s_Manufacturer_C2h_qvga, NV_TRUE },
        { 0x29, 0xC3,  4, s_Manufacturer_C3h_qvga, NV_TRUE },
        { 0x29, 0xC5, 12, s_Manufacturer_C5h_qvga, NV_TRUE },
        { 0x29, 0xC6,  2, s_Manufacturer_C6h, NV_TRUE },
        { 0x29, 0xC8, 16, s_Manufacturer_C8h_qvga, NV_TRUE },
        { 0x29, 0xC9, 16, s_Manufacturer_C9h_qvga, NV_TRUE },
        { 0x29, 0xCA, 16, s_Manufacturer_CAh_qvga, NV_TRUE },
        { 0x29, 0xD0,  4, s_Manufacturer_D0h_qvga, NV_TRUE },
        { 0x29, 0xD2,  3, s_Manufacturer_D2h, NV_TRUE },
        { 0x29, 0xD3,  2, s_Manufacturer_D3h, NV_TRUE },
        { 0x29, 0xD4,  2, s_Manufacturer_D4h, NV_TRUE },
        { 0x29, 0xD5,  6, s_Manufacturer_D5h_qvga, NV_TRUE },
        { 0x29, 0xD6,  3, s_Manufacturer_D6h_qvga, NV_TRUE },
        { 0x29, 0xD7,  9, s_Manufacturer_D7h, NV_TRUE },
        { 0x29, 0xD9,  3, s_Manufacturer_D9h, NV_TRUE },
        { 0x23, 0xB0,  1, s_Manufacturer_Disable, NV_FALSE },
        { 0x05, 0x11,  1, s_ExitSleepMode, NV_FALSE},
        { 0x00, -1,  120, NULL, NV_FALSE},              /* wait 120ms */
        { 0x23, 0xB4,  1, s_EnterVideoMode, NV_FALSE },
        { 0x05, 0x29,  1, s_PanelDisplayOn, NV_FALSE},        
        { 0x39, 0x35,  1, s_TearOn, NV_TRUE},     
    };

    static DsiPanelData s_PanelInitSeq_QVGA_CmdMode[] = {
        // Panel Reg Addr, Data Size, Register Data
        { 0x23, 0xB0,  1, s_Manufacturer_Enable, NV_FALSE },
        { 0x23, 0xE2,  1, s_Manufacturer_E2h, NV_FALSE },
        { 0x29, 0xB3,  2, s_Manufacturer_B3h_qvga, NV_TRUE },
        { 0x23, 0xB6,  1, s_Manufacturer_B6h, NV_FALSE },
        { 0x29, 0xB8, 15, s_Manufacturer_B8h_qvga, NV_TRUE },
        { 0x29, 0xB9,  4, s_Manufacturer_B9h_qvga, NV_TRUE },
        { 0x23, 0xBE,  1, s_Manufacturer_BEh, NV_FALSE },
        { 0x29, 0xC0,  5, s_Manufacturer_C0h_qvga, NV_TRUE },
        { 0x29, 0xC1,  4, s_Manufacturer_C1h_qvga, NV_TRUE },
        { 0x29, 0xC2,  4, s_Manufacturer_C2h_qvga, NV_TRUE },
        { 0x29, 0xC3,  4, s_Manufacturer_C3h_qvga, NV_TRUE },
        { 0x29, 0xC5, 12, s_Manufacturer_C5h_qvga, NV_TRUE },
        { 0x29, 0xC6,  2, s_Manufacturer_C6h, NV_TRUE },
        { 0x29, 0xC8, 16, s_Manufacturer_C8h_qvga, NV_TRUE },
        { 0x29, 0xC9, 16, s_Manufacturer_C9h_qvga, NV_TRUE },
        { 0x29, 0xCA, 16, s_Manufacturer_CAh_qvga, NV_TRUE },
        { 0x29, 0xD0,  4, s_Manufacturer_D0h_qvga, NV_TRUE },
        { 0x29, 0xD2,  3, s_Manufacturer_D2h, NV_TRUE },
        { 0x29, 0xD3,  2, s_Manufacturer_D3h, NV_TRUE },
        { 0x29, 0xD4,  2, s_Manufacturer_D4h, NV_TRUE },
        { 0x29, 0xD5,  6, s_Manufacturer_D5h_qvga, NV_TRUE },
        { 0x29, 0xD6,  3, s_Manufacturer_D6h_qvga, NV_TRUE },
        { 0x29, 0xD7,  9, s_Manufacturer_D7h, NV_TRUE },
        { 0x29, 0xD9,  3, s_Manufacturer_D9h, NV_TRUE },
        { 0x23, 0xB0,  1, s_Manufacturer_Disable, NV_FALSE },
        { 0x05, 0x11,  1, s_ExitSleepMode, NV_FALSE},
        { 0x00, -1,  120, NULL, NV_FALSE},              /* wait 120ms */
        { 0x23, 0xB4,  1, s_EnterCommandMode, NV_FALSE },
        { 0x05, 0x29,  1, s_PanelDisplayOn, NV_FALSE},        
        { 0x39, 0x35,  1, s_TearOn, NV_TRUE},     
    };

    DsiPanelData *s_PanelInitSeq;
    NvU32 SeqList;
    NvU32 i;
    NvOdmServicesPmuHandle hPmu;
    NvOdmPeripheralConnectivity const *conn;
    NvU32 settle_us;
    NvBool b;

    NV_ASSERT( mode->width && mode->height && mode->bpp );

    if( !hDevice->Init )
    {
        NV_ASSERT( !"device not initialized" );
        return NV_FALSE;
    }

    /* get the peripheral config */
    if( !sharpdsi_discover( hDevice ) )
    {
        NV_ASSERT( !"failed to find guid in discovery database" );
        return NV_FALSE;
    }

    conn = hDevice->PeripheralConnectivity;

    hPmu = NvOdmServicesPmuOpen();
    for( i = 0; i < conn->NumAddress; i++ )
    {
        if( conn->AddressList[i].Interface == NvOdmIoModule_Vdd )
        {
            NvOdmServicesPmuVddRailCapabilities cap;

            /* address is the vdd rail id */
            NvOdmServicesPmuGetCapabilities( hPmu,
                conn->AddressList[i].Address, &cap );

            /* set the rail volatage to the recommended */
            NvOdmServicesPmuSetVoltage( hPmu,
                conn->AddressList[i].Address, cap.requestMilliVolts,
                &settle_us );
    
            /* wait for rail to settle */
            NvOdmOsWaitUS( settle_us );
        }
    }
    NvOdmServicesPmuClose( hPmu );

    for( i = 0; i < conn->NumAddress; i++ )
    {
        if( conn->AddressList[i].Interface == NvOdmIoModule_Gpio )
        {
            NvU32 const Instance = conn->AddressList[i].Instance;
            NvU32 const Address  = conn->AddressList[i].Address;

            if( !s_hLCDResPin )
            {
                s_hLCDResPin = NvOdmGpioAcquirePinHandle( s_hGpio,
                    Instance, Address );
                break;
            }
        }
    }

    NV_ASSERT( s_hLCDResPin );

    if( s_UseHvga )
    {
        if (mode->flags & NVODM_DISP_MODE_FLAG_PARTIAL) 
        {
            SeqList = NV_ARRAY_SIZE(s_PanelInitSeq_HVGA_CmdMode);
            s_PanelInitSeq = &s_PanelInitSeq_HVGA_CmdMode[0];
        } 
        else 
        {
            SeqList = NV_ARRAY_SIZE(s_PanelInitSeq_HVGA);
            s_PanelInitSeq = &s_PanelInitSeq_HVGA[0];
        }
    }
    else
    {
        if (mode->flags & NVODM_DISP_MODE_FLAG_PARTIAL) 
        {
            SeqList = NV_ARRAY_SIZE(s_PanelInitSeq_QVGA_CmdMode);
            s_PanelInitSeq = &s_PanelInitSeq_QVGA_CmdMode[0];
        } 
        else 
        {
            SeqList = NV_ARRAY_SIZE(s_PanelInitSeq_QVGA);
            s_PanelInitSeq = &s_PanelInitSeq_QVGA[0];
        }
    }

    NvOdmGpioSetState( s_hGpio, s_hLCDResPin, 0x0 );
    NvOdmGpioConfig( s_hGpio, s_hLCDResPin, NvOdmGpioPinMode_Output);
    NvOdmOsWaitUS( 5000 );
    NvOdmGpioSetState( s_hGpio, s_hLCDResPin, 0x1 );

    b = s_trans.NvOdmDispDsiInit( hDevice, DSI_INSTANCE, 0 );
    if( !b )
    {
        NV_ASSERT( !"dsi init failed" );
        return NV_FALSE;
    }

    // configure the dsi for command mode - command table will put the panel
    // in video mode eventually
    b = s_trans.NvOdmDispDsiEnableCommandMode( hDevice, DSI_INSTANCE );
    if( !b )
    {
        NV_ASSERT( !"dsi command mode failed" );
        return NV_FALSE;
    }

    for (i = 0; i < SeqList; i++)
    {
        if (s_PanelInitSeq[i].pData == NULL) 
        {
            NvOdmOsSleepMS(s_PanelInitSeq[i].DataSize);
            continue;
        }

        // TODO: Check the delay required between commands.
        NvOdmOsSleepMS(20);

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

    return NV_TRUE;
}

void SharpDSI_GetHal(NvOdmDispDeviceHandle hDevice)
{
    NV_ASSERT(hDevice);

    hDevice->pfnSetup = sharpdsi_Setup;
    hDevice->pfnRelease = sharpdsi_Release;
    hDevice->pfnListModes = sharpdsi_ListModes;
    hDevice->pfnSetMode = sharpdsi_SetMode;
    hDevice->pfnSetPowerLevel = sharpdsi_SetPowerLevel;
    hDevice->pfnGetParameter = sharpdsi_GetParameter;
    hDevice->pfnGetPinPolarities = sharpdsi_GetPinPolarities;
    hDevice->pfnGetConfiguration = sharpdsi_GetConfiguration;
    hDevice->pfnSetSpecialFunction = sharpdsi_SetSpecialFunction;
    hDevice->pfnSetBacklight = sharpdsi_SetBacklight;
}
