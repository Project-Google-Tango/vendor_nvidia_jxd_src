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
#include "nvodm_backlight.h"
#include "panel_auo_edp.h"
#include "nvodm_services.h"
#include "toshiba_TC358770_reg.h"
#include "nvodm_disp_i2c.h"
#include "nvos.h"
#include "nvodm_backlight.h"

//set to 0 for 1st DSI instance and 1 for 2nd DSI instance
#define DSI_INSTANCE 0
#define TEGRA_DC_OUT_CONTINUOUS_MODE 0
#define TEGRA_DC_OUT_ONESHOT_MODE 1

#define DISP_I2C_INSTANCE 0 //Gen1_I2c
#define DISP_TC358770_ADDRESS 0x0f
#define DISP_TC358770_FREQUENCY 100

#define DC_CTRL_MODE    TEGRA_DC_OUT_CONTINUOUS_MODE

typedef struct DsiPanelDataRec
{
    NvU32 DsiCmd;
    NvU32 PanelReg;
    NvU32 DataSize;
    NvU8 *pData;
    NvBool IsLongPacket;
} DsiPanelData;

static NvBool auo_edpDsi_PanelInit( NvOdmDispDeviceHandle hDevice, NvOdmDispDeviceMode *mode);

// Color calibration
static NvU32 s_ColorCalibration_Red     = NVODM_DISP_FP_TO_FX(1.00);
static NvU32 s_ColorCalibration_Green   = NVODM_DISP_FP_TO_FX(1.00);
static NvU32 s_ColorCalibration_Blue    = NVODM_DISP_FP_TO_FX(1.00);

static NvOdmServicesGpioHandle s_hGpio =  NULL;
static NvOdmGpioPinHandle s_Lcd_bl_en = NULL;
static NvOdmGpioPinHandle s_Lcd_bl_pwm = NULL;
static NvOdmGpioPinHandle s_Lcd_rst = NULL;
static NvOdmGpioPinHandle s_En_Vdd_bl = NULL;
static NvOdmGpioPinHandle s_Lvds_en = NULL;


static NvBool s_bBacklight = NV_FALSE;


// Other global variables.
static NvOdmDispDsiTransport   s_trans;

static NvOdmDispDeviceMode auo_edp_modes[] =
{
    { 1920, // width
      1080, // height
      24, // bpp(color depth)
      0, // refresh
      0, // frequency
      0, // flags
      // timing
      { 4, // h_ref_to_sync
        1, // v_ref_to_sync
        28, // h_sync_width
        5, // v_sync_width
        148, // h_back_porch
        36, // v_back_porch
        1920, // h_disp_active
        1080, // v_disp_active
        66, // h_front_porch
        4, // v_front_porch
      },
      NV_FALSE // partial
    },
};

static NvBool auo_edp_discover (NvOdmDispDeviceHandle hDevice)
{
    NvU64 guid;
    NvOdmPeripheralConnectivity const *conn;

    /* get the main panel */
    guid = AUO_EDP_PANEL_GUID;

    /* get the connectivity info */
    conn = NvOdmPeripheralGetGuid(guid);
    if( !conn )
    {
        return NV_FALSE;
    }

    hDevice->PeripheralConnectivity = conn;
    return NV_TRUE;
}

static void
auo_edp_nullmode( NvOdmDispDeviceHandle hDevice )
{
    if (!hDevice)
        return ;
    hDevice->CurrentMode.width = 0;
    hDevice->CurrentMode.height = 0;
    hDevice->CurrentMode.bpp = 0;
}

NvBool auo_edp_Setup( NvOdmDispDeviceHandle hDevice )
{
    NvOdmPeripheralConnectivity const *pCon;
    NvOdmServicesPmuVddRailCapabilities RailCaps;
    NvU32 SettlingTime = 0;
    NvU32 i;
    NvOdmServicesPmuHandle hPmu;

    if (!hDevice)
        return NV_FALSE;

    /* get the peripheral config */
    if( !auo_edp_discover(hDevice))
    {
        return NV_FALSE;
    }

    if( !hDevice->bInitialized )
    {
        hDevice->Type = NvOdmDispDeviceType_DSI;
        hDevice->Usage = NvOdmDispDeviceUsage_Primary;
        hDevice->MaxHorizontalResolution = auo_edp_modes[0].width;
        hDevice->MaxVerticalResolution = auo_edp_modes[0].height;
        hDevice->BaseColorSize = NvOdmDispBaseColorSize_888;
        hDevice->DataAlignment = NvOdmDispDataAlignment_MSB;
        hDevice->PinMap = NvOdmDispPinMap_Single_Rgb24_Spi5;
        hDevice->PwmOutputPin = NvOdmDispPwmOutputPin_Unspecified;
        hDevice->modes = auo_edp_modes;
        hDevice->nModes = NV_ARRAY_SIZE(auo_edp_modes);
        hDevice->power = NvOdmDispDevicePowerLevel_Off;

        auo_edp_nullmode( hDevice );

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
        NvOdmServicesPmuClose(hPmu);

        hDevice->bInitialized = NV_TRUE;
    }

    return NV_TRUE;
}

void auo_edp_Release( NvOdmDispDeviceHandle hDevice )
{
    if (!hDevice)
        return ;
    hDevice->bInitialized = NV_FALSE;
    auo_edp_nullmode( hDevice );
}

void auo_edp_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
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

NvBool auo_edp_SetMode( NvOdmDispDeviceHandle hDevice,
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
        auo_edp_nullmode(hDevice);
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
        auo_edpDsi_PanelInit(hDevice, mode);
    }
    else
    {
        auo_edp_nullmode(hDevice);
    }
    return NV_TRUE;
}

void auo_edp_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    if( !s_bBacklight )
    {
        NvOdmBacklightInit( hDevice, NV_FALSE );
        s_bBacklight = NV_TRUE;
    }

    NvOdmBacklightIntensity( hDevice, intensity );
}

void auo_edp_SetBacklightIntensity( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    //To do: Add PWM support
}
static void
auo_edp_Resume( NvOdmDispDeviceHandle hDevice )
{
    auo_edpDsi_PanelInit( hDevice, &hDevice->CurrentMode );
}

void auo_edp_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
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
            auo_edp_Resume( hDevice );
        }
        break;
    default:
        break;
    }
}

NvBool auo_edp_GetParameter( NvOdmDispDeviceHandle hDevice,
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

void* auo_edp_GetConfiguration( NvOdmDispDeviceHandle hDevice )
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
    return (void *)dsi;
}

void auo_edp_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values )
{
    *nPins = 0;
}

NvBool auo_edp_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
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

static NvBool Init_TC358770_Bridge(NvOdmDispDeviceMode *mode)
{

    NvOdmServicesI2cHandle hOdmI2c = NULL;
    NvBool ret = NV_TRUE;
    NvU32 val;

    hOdmI2c =  NvOdmI2cOpen(NvOdmIoModule_I2c, DISP_I2C_INSTANCE);
    if (!hOdmI2c)
    {
     ret = NV_FALSE;
     goto fail;
    }

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_LINK_TRAINING_CTRL, 0xC0B1);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_SYSTEM_CLK_PARAM, 0x0105);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_PHY_CTRL, 0x0f);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_LINK_CLK_PLL_CTRL, 0x05);

    NvOsWaitUS(70);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_STREAM_CLK_PLL_PARAM, 0x0153822A);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_STREAM_CLK_PLL_CTRL, 0x05);

    NvOsWaitUS(70);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_PHY_CTRL, 0x1f);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_PHY_CTRL, 0x0f);

    NvOsWaitUS(70);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_AUX_CHANNEL_CONFIG1, 0x01063F);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_AUX_CHANNEL_DPCD_ADDR, 0x01);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_AUX_CHANNEL_CONFIG0, 0x09);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_AUX_CHANNEL_DPCD_ADDR, 0x02);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_AUX_CHANNEL_CONFIG0, 0x09);

    NvOsWaitUS(70);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_AUX_CHANNEL_DPCD_ADDR, 0x0100);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_AUX_CHANNEL_DPCD_WR_DATA0, 0x0406);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_AUX_CHANNEL_CONFIG0, 0x0108);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_AUX_CHANNEL_DPCD_ADDR, 0x0108);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_AUX_CHANNEL_DPCD_WR_DATA0, 0x01);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_AUX_CHANNEL_CONFIG0, 0x08);

    NvOsWaitUS(70);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_LINK_TRAINING_SINK_CONFIG, 0x21);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_LINK_TRAINING_LOOP_CTRL, 0x7600000D);

    NvOsWaitUS(70);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_LINK_TRAINING_CTRL, 0xC1B1);
    NvOsWaitUS(70);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_DP_CTRL, 0x41);

    NvOsWaitUS(70);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
           TC358770_LINK_TRAINING_SINK_CONFIG, 0x22);

    NvOsWaitUS(70);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_LINK_TRAINING_CTRL, 0xC2B1);

    NvOsWaitUS(70);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_AUX_CHANNEL_DPCD_ADDR, 0x0102);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_AUX_CHANNEL_DPCD_WR_DATA0, 0x00);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_AUX_CHANNEL_CONFIG0, 0x08);

    NvOsWaitUS(70);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_LINK_TRAINING_CTRL, 0x40b1);

    NvOsWaitUS(70);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_AUX_CHANNEL_DPCD_ADDR , 0x0200);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_AUX_CHANNEL_CONFIG0 , 0x0409);

    NvOsWaitUS(100);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
             TC358770_DSI0_PPI_TX_RX_TA , 0x040004);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_DSI0_PPI_LPTXTIMECNT , 0x04);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_DSI0_PPI_D0S_CLRSIPOCOUNT , 0x07);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_DSI0_PPI_D1S_CLRSIPOCOUNT , 0x07);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_DSI0_PPI_D2S_CLRSIPOCOUNT , 0x07);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_DSI0_PPI_D3S_CLRSIPOCOUNT , 0x07);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_DSI0_PPI_LANEENABLE , 0x1F);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_DSI0_DSI_LANEENABLE , 0x1F);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_DSI0_PPI_START , 0x01);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_DSI0_DSI_START , 0x01);

    NvOsWaitUS(70);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_CMD_CTRL , 0x00);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_LR_SIZE , 0x02800000);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
             TC358770_PG_SIZE , 0x00);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_RM_PXL , 0x00);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_HORI_SCLR_HCOEF , 0x00);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_HORI_SCLR_LCOEF , 0x00);

    NvOsWaitUS(70);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_VIDEO_FRAME_CTRL , 0x15400100);

    val = (mode->timing.Horiz_BackPorch << 16) | mode->timing.Horiz_SyncWidth;
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_HORIZONTAL_TIME0 , val);

    val = (mode->timing.Horiz_FrontPorch << 16) | mode->timing.Horiz_DispActive;
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
             TC358770_HORIZONTAL_TIME1 , val);

    val = (mode->timing.Vert_BackPorch << 16) | mode->timing.Vert_DispActive;
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_VERTICAL_TIME0 , val);

    val = (mode->timing.Vert_FrontPorch<< 16) | mode->timing.Vert_DispActive;
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_VERTICAL_TIME1 , val);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_VIDEO_FRAME_UPDATE_ENABLE , 0x01);

    NvOsWaitUS(70);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_VIDEO_FRAME_OUTPUT_DELAY , 0x1F030c);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_VIDEO_FRAME_SIZE , 0x020d0320);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_VIDEO_FRAME_START , 0x23008c);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_VIDEO_FRAME_ACTIVE_REGION_SIZE , 0x01e00280);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_VIDEO_FRAME_SYNC_WIDTH , 0x80028060);

    NvOsWaitUS(70);

    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_DP_CONFIG , 0x1EBF0020);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_NVALUE_VIDEO_CLK_REGEN , 0x02A3);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_DP_CTRL , 0x41);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_DP_CTRL , 0x43);
    NvOdmDispI2cWrite32(hOdmI2c, DISP_TC358770_ADDRESS, DISP_TC358770_FREQUENCY,
            TC358770_SYSTEM_CTRL , 0x01);
    return NV_TRUE;
fail:
    return NV_FALSE;
}

void auo_edp_GetHal( NvOdmDispDeviceHandle hDevice )
{
    NV_ASSERT(hDevice);
    if (!hDevice)
        return ;

    hDevice->Setup = auo_edp_Setup;
    hDevice->Release = auo_edp_Release;
    hDevice->ListModes = auo_edp_ListModes;
    hDevice->SetMode = auo_edp_SetMode;
    hDevice->SetPowerLevel = auo_edp_SetPowerLevel;
    hDevice->GetParameter = auo_edp_GetParameter;
    hDevice->GetPinPolarities = auo_edp_GetPinPolarities;
    hDevice->GetConfiguration = auo_edp_GetConfiguration;
    hDevice->SetSpecialFunction = auo_edp_SetSpecialFunction;

    // Backlight functions
    hDevice->SetBacklight = auo_edp_SetBacklight;
    hDevice->BacklightIntensity = auo_edp_SetBacklightIntensity;
}

static NvBool auo_edpDsi_PanelInit( NvOdmDispDeviceHandle hDevice,
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
    //No Panel Init sequence required for Auo-EDP panel,
    //Hence no initialisation of command mode and write functions
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

    NvOsWaitUS(100);

    s_Lcd_bl_en = NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('h' - 'a') , 2);
    if (!s_Lcd_bl_en)
    {
        NV_ASSERT(!"GPIO pin acquisition failed \n");
        return NV_FALSE;
    }
    NvOdmGpioSetState(s_hGpio, s_Lcd_bl_en, 1);
    NvOdmGpioConfig(s_hGpio, s_Lcd_bl_en, NvOdmGpioPinMode_Output);

    s_Lvds_en = NvOdmGpioAcquirePinHandle(s_hGpio, (NvU32)('g' - 'a') , 3);
    if (!s_Lcd_bl_en)
    {
        NV_ASSERT(!"GPIO pin acquisition failed \n");
        return NV_FALSE;
    }
    NvOdmGpioSetState(s_hGpio, s_Lcd_bl_en, 1);
    NvOdmGpioConfig(s_hGpio, s_Lcd_bl_en, NvOdmGpioPinMode_Output);

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

    if (!Init_TC358770_Bridge(mode))
    {
        NV_ASSERT("edp bridge initialisation failed \n");
        return NV_FALSE;
    }

     return NV_TRUE; //returning false till the code is added
}
