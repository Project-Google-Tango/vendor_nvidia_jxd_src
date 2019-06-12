/*
 * Copyright (c) 2007 - 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_disp.h"
#include "panel_sharp_wvgap.h"
#include "nvodm_services.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query_gpio.h"
#include "nvodm_backlight.h"

#define PANEL_CONTROL_INTERFACE_SPEED_KHZ 1000

static NvOdmDispDeviceMode sharpwvgap_modes[] =
{
    // WVGA Portrait, 24 bit
    { 480, // width
      800, // height
      24, // bpp
      0, // refresh
      0, // frequency
      0, // flags
      // timing
      { 11, // h_ref_to_sync
        1, // v_ref_to_sync
        16, // h_sync_width
        2, // v_sync_width
        24, // h_back_porch
        2, // v_back_porch
        480, // h_disp_active
        800, // v_disp_active
        16, // h_front_porch
        4, // v_front_porch
      },
      NV_FALSE // partial
    },

    // FIXME: this panel supports partial mode
};

#define ResetGpioPin                        (NvOdmGpioPin_DisplayPanel2Start)
static NvOdmServicesGpioHandle s_hGpio;
static NvOdmGpioPinHandle s_hResetGpioPin;
static NvU32 s_spi_instance = 0;
static NvU32 s_spi_chipselect = 0;

static const NvOdmGpioPinInfo *s_gpioPinTable;

static NvBool s_bReopen = NV_FALSE;
static NvBool s_bBacklight = NV_FALSE;
static NvU8 s_backlight_intensity;

NvBool sharpwvgap_panel_init( NvOdmDispDeviceHandle hDevice, NvU32 bpp );
NvBool sharpwvgap_discover( NvOdmDispDeviceHandle hDevice );
void sharpwvgap_suspend( NvOdmDispDeviceHandle hDevice );
void sharpwvgap_resume( NvOdmDispDeviceHandle hDevice );

static void
sharpwvgap_nullmode( NvOdmDispDeviceHandle hDevice )
{
    hDevice->CurrentMode.width = 0;
    hDevice->CurrentMode.height = 0;
    hDevice->CurrentMode.bpp = 0;
}

NvBool sharpwvgap_Setup( NvOdmDispDeviceHandle hDevice )
{
    if( !hDevice->bInitialized )
    {
        NvU32 gpioPinCount;

        // FIXME: report a 16 bit panel for the 16 bit mode
        hDevice->Type = NvOdmDispDeviceType_TFT;
        hDevice->Usage = NvOdmDispDeviceUsage_Unassigned;
        hDevice->MaxHorizontalResolution = 480;
        hDevice->MaxVerticalResolution = 800;
        hDevice->BaseColorSize = NvOdmDispBaseColorSize_888;
        hDevice->DataAlignment = NvOdmDispDataAlignment_MSB;
        hDevice->PinMap = NvOdmDispPinMap_Single_Rgb24_Spi5;
        hDevice->PwmOutputPin = NvOdmDispPwmOutputPin_Unspecified;
        hDevice->modes = sharpwvgap_modes;
        hDevice->nModes = NV_ARRAY_SIZE(sharpwvgap_modes);
        hDevice->CurrentPower = NvOdmDispDevicePowerLevel_Off;

        sharpwvgap_nullmode( hDevice );

        s_hGpio = NvOdmGpioOpen();
        if( !s_hGpio )
        {
            return NV_FALSE;
        }

        s_gpioPinTable = NvOdmQueryGpioPinMap(NvOdmGpioPinGroup_Display, 0,
            &gpioPinCount);
        if( gpioPinCount <= NvOdmGpioPin_DisplayPanel1End )
        {
            return NV_FALSE;
        }

        s_hResetGpioPin = NvOdmGpioAcquirePinHandle(s_hGpio,
            s_gpioPinTable[ResetGpioPin].Port,
            s_gpioPinTable[ResetGpioPin].Pin);

        hDevice->bInitialized = NV_TRUE;
    }

    return NV_TRUE;
}

void sharpwvgap_Release( NvOdmDispDeviceHandle hDevice )
{
    hDevice->bInitialized = NV_FALSE;

    // FIXME: send display off command

    NvOdmBacklightIntensity( hDevice, 0 );
    NvOdmBacklightDeinit(hDevice);
    s_bBacklight = NV_FALSE;

    NvOdmGpioReleasePinHandle(s_hGpio, s_hResetGpioPin);
    NvOdmGpioClose(s_hGpio);
    s_hResetGpioPin = 0;
    s_hGpio = 0;

    sharpwvgap_nullmode( hDevice );
}

void sharpwvgap_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
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

NvBool sharpwvgap_SetMode( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDeviceMode *mode, NvU32 flags )
{
    NV_ASSERT( hDevice );

    if( mode )
    {
        if( flags & NVODM_DISP_MODE_FLAGS_REOPEN )
        {
            s_bReopen = NV_TRUE;
        }

        /* only exporting 24bit. the panel supports 16 and 18 as well, but
         * that will be ignored.
         */

        // FIXME: handle real mode changes
        if( !hDevice->CurrentMode.width && !hDevice->CurrentMode.height &&
            !hDevice->CurrentMode.bpp )
        {
            sharpwvgap_panel_init( hDevice, mode->bpp );
        }

        if( !s_bBacklight && !NvOdmBacklightInit( hDevice, s_bReopen ) )
        {
            return NV_FALSE;
        }

        s_bBacklight = NV_TRUE;

        // FIXME: check for normal/partial mode
        hDevice->CurrentMode = *mode;
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
        sharpwvgap_nullmode( hDevice );
    }

    return NV_TRUE;
}

void sharpwvgap_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    if( !s_bBacklight )
    {
        NvOdmBacklightInit( hDevice, NV_FALSE );
        s_bBacklight = NV_TRUE;
    }

    NvOdmBacklightIntensity( hDevice, intensity );
    s_backlight_intensity = intensity;
}

void sharpwvgap_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level )
{
    NV_ASSERT( hDevice );
    hDevice->CurrentPower = level;

    switch( level ) {
    case NvOdmDispDevicePowerLevel_Off:
    case NvOdmDispDevicePowerLevel_Suspend:
        NvOdmBacklightIntensity( hDevice, 0 );
        sharpwvgap_suspend( hDevice );
        break;
    case NvOdmDispDevicePowerLevel_On:
        sharpwvgap_resume( hDevice );
        NvOdmBacklightIntensity( hDevice, s_backlight_intensity );
        break;
    default:
        break;
    }
}

NvBool sharpwvgap_GetParameter( NvOdmDispDeviceHandle hDevice,
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
    case NvOdmDispParameter_PwmOutputPin:
        *val = hDevice->PwmOutputPin;
        break;
    case NvOdmDispParameter_ColorCalibrationRed:
        *val = NVODM_DISP_FP_TO_FX(1.0633);
        break;
    case NvOdmDispParameter_ColorCalibrationGreen:
        *val = NVODM_DISP_FP_TO_FX(0.9959);
        break;
    case NvOdmDispParameter_ColorCalibrationBlue:
        *val = NVODM_DISP_FP_TO_FX(0.8724);
        break;

    default:
        return NV_FALSE;
    }

    return NV_TRUE;
}

void * sharpwvgap_GetConfiguration( NvOdmDispDeviceHandle hDevice )
{
    NvOdmDispTftConfig *tft = &hDevice->Config.tft;
    return (void *)tft;
}

void sharpwvgap_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values )
{
    *nPins = 0;
}

NvBool sharpwvgap_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config )
{
    // FIXME: this panel support partial mode, implement
    return NV_FALSE;
}

#define DO( statement ) \
    for( ;; ) \
    { \
        NvBool _e; \
        _e = statement; \
        if( _e ) break; \
        NvOdmOsWaitUS( 1 ); \
    }

#define SPI_1BYTE( a, b ) \
    CmdData[0] = (a); \
    CmdData[1] = (b); \
    NvOdmSpiTransaction( hSpi, s_spi_chipselect, \
        PANEL_CONTROL_INTERFACE_SPEED_KHZ, 0, CmdData, 2, 9 );


#define SPI_3BYTE( a, b, c, d ) \
    CmdData[2] = (d); \
    CmdData[1] = (c&0x01)|((b&0x7F)<<1); \
    CmdData[0] = (((b&0x80)>>7)|((a<<1)&0x02)); \
    NvOdmSpiTransaction( hSpi, s_spi_chipselect, \
        PANEL_CONTROL_INTERFACE_SPEED_KHZ, 0, CmdData, 3, 18 );


#define SPI_4BYTE( a, b, c, d, e, f ) \
    CmdData[3] = (f); \
    CmdData[2] = (e&0x01)|((d&0x7F)<<1); \
    CmdData[1] = (((d&0x80)>>7)|((c<<1)&0x02)|((b&0x3F)<<2)); \
    CmdData[0] = (((b&0xC0)>>6)|((a<<2)&0x04)); \
    NvOdmSpiTransaction( hSpi, s_spi_chipselect, \
        PANEL_CONTROL_INTERFACE_SPEED_KHZ, 0, CmdData, 4, 27 );



NvBool
sharpwvgap_discover( NvOdmDispDeviceHandle hDevice )
{
    NvU64 guid;
    NvOdmPeripheralConnectivity const *conn;

    NvOdmDispGetDefaultGuid(&guid);

    /* get the connectivity info */
    conn = NvOdmPeripheralGetGuid(guid);
    if( !conn )
    {
        NV_ASSERT(!"ERROR: Panel not found.");
        return NV_FALSE;
    }

    hDevice->PeripheralConnectivity = conn;

    return NV_TRUE;
}

void
sharpwvgap_resume( NvOdmDispDeviceHandle hDevice )
{
    NvOdmPeripheralConnectivity const *conn;
    NvOdmServicesPmuHandle hPmu;
    NvOdmServicesSpiHandle hSpi;
    NvU32 settle_us;
    NvU8 CmdData[4];
    NvU32 i;

    /** WARNING **/
    // This must call the power rail config regardless of the reopen
    // flag. Reference counts are not preserved across the bootloader
    // to operating system transition. All code must be self-balancing.

    /* enable the power rail */
    hPmu = NvOdmServicesPmuOpen();
    conn = hDevice->PeripheralConnectivity;

    NV_ASSERT( conn );

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

    if( s_bReopen )
    {
        // the bootloader has already initialized the panel, skip this resume.
        s_bReopen = NV_FALSE;
        return;
    }

    hSpi = NvOdmSpiOpen( NvOdmIoModule_Spi, s_spi_instance );
    NvOdmOsWaitUS( 1000 );
    /* take panel out of reset */
    NvOdmGpioSetState( s_hGpio, s_hResetGpioPin, 0x1 );
    NvOdmOsWaitUS( 5000 );

    /* send power up sequence:
     *
     * swreset (command 0x1)
     * slpout (command 0x11)
     * dspon (command 0x29)
     *
     * commands are 8 bits. the spi packets are 9 bits -- the top bit is
     * 0 for command, 1 for parameter data.
     *
     * NOTE: the actual sequence is undocumented, includes some no-ops
     * and mysterious commands.
     */

    SPI_3BYTE( 0, 0x29, 0, 0x11 );
    NvOdmOsWaitUS( 120000 );
    SPI_3BYTE( 0, 0x36, 1, 0x00 );
    SPI_3BYTE( 0, 0x3A, 1, 0x70 );
    SPI_3BYTE( 0, 0xB0, 1, 0x03 );
    SPI_3BYTE( 0, 0xB8, 1, 0x00 );
    SPI_4BYTE( 0, 0xB9, 1, 0x01, 1, 0xFF );
    SPI_3BYTE( 0, 0xB0, 1, 0x03 );

    NvOdmOsWaitUS( 5000 );

    NvOdmSpiClose( hSpi );
}

void
sharpwvgap_suspend( NvOdmDispDeviceHandle hDevice )
{
    NvOdmPeripheralConnectivity const *conn;
    NvOdmServicesPmuHandle hPmu;
    NvOdmServicesSpiHandle hSpi;
    NvU8 CmdData[2];
    NvU32 i;
    hSpi = NvOdmSpiOpen( NvOdmIoModule_Spi, s_spi_instance );

    if( !hSpi )
    {
        return;
    }

    SPI_1BYTE( 0, 0x28 );
    NvOdmOsWaitUS( 20000 );
    SPI_1BYTE( 0, 0x10 );
    NvOdmOsWaitUS( 120000 );

    /* put panel into reset */
    NvOdmGpioSetState( s_hGpio, s_hResetGpioPin, 0x1 );

    NvOdmSpiClose( hSpi );

    /* disable power */
    hPmu = NvOdmServicesPmuOpen();
    conn = hDevice->PeripheralConnectivity;

    NV_ASSERT( conn );

    for( i = 0; i < conn->NumAddress; i++ )
    {
        if( conn->AddressList[i].Interface == NvOdmIoModule_Vdd )
        {
            /* set the rail volatage to the recommended */
            NvOdmServicesPmuSetVoltage( hPmu,
                conn->AddressList[i].Address, NVODM_VOLTAGE_OFF, 0 );
        }
    }

    NvOdmServicesPmuClose( hPmu );
}

NvBool
sharpwvgap_panel_init( NvOdmDispDeviceHandle hDevice, NvU32 bpp )
{
    NvOdmPeripheralConnectivity const *conn;
    NvBool spi_found = NV_FALSE;
    NvU32 i;

    if( !hDevice->bInitialized )
    {
        return NV_FALSE;
    }

    /* get the peripheral config */
    if( !sharpwvgap_discover( hDevice ) )
    {
        return NV_FALSE;
    }

    /* get the spi instance and chip select */
    conn = hDevice->PeripheralConnectivity;
    for( i = 0; i < conn->NumAddress; i++ )
    {
        if( conn->AddressList[i].Interface == NvOdmIoModule_Spi )
        {
            s_spi_instance = conn->AddressList[i].Instance;
            s_spi_chipselect = conn->AddressList[i].Address;
            spi_found = NV_TRUE;
            break;
        }
    }

    if( !spi_found )
    {
        return NV_FALSE;
    }

    /* pull reset low to prevent chip select issues */
    if( !s_bReopen )
    {
        NvOdmGpioSetState( s_hGpio, s_hResetGpioPin, 0x0 );
        NvOdmGpioConfig( s_hGpio, s_hResetGpioPin, NvOdmGpioPinMode_Output);
    }

    return NV_TRUE;
}

void sharpwvgap_GetHal( NvOdmDispDeviceHandle hDevice )
{
    NV_ASSERT(hDevice);

    hDevice->pfnSetup = sharpwvgap_Setup;
    hDevice->pfnRelease = sharpwvgap_Release;
    hDevice->pfnListModes = sharpwvgap_ListModes;
    hDevice->pfnSetMode = sharpwvgap_SetMode;
    hDevice->pfnSetPowerLevel = sharpwvgap_SetPowerLevel;
    hDevice->pfnGetParameter = sharpwvgap_GetParameter;
    hDevice->pfnGetPinPolarities = sharpwvgap_GetPinPolarities;
    hDevice->pfnGetConfiguration = sharpwvgap_GetConfiguration;
    hDevice->pfnSetSpecialFunction = sharpwvgap_SetSpecialFunction;

    // Backlight functions
    hDevice->pfnSetBacklight = sharpwvgap_SetBacklight;
}
