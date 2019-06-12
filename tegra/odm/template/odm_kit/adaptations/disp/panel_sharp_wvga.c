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
#include "panel_sharp_wvga.h"
#include "nvodm_services.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query_gpio.h"
#include "nvodm_backlight.h"

#define PANEL_CONTROL_INTERFACE_SPEED_KHZ 1000

static NvOdmDispDeviceMode sharpwvga_modes[] =
{
    // WVGA, 16 bit
    { 800, // width
      480, // height
      16, // bpp
      0, // refresh
      0, // frequency
      0, // flags
      // timing
      { 4, // h_ref_to_sync
        2, // v_ref_to_sync
        10, // h_sync_width
        3, // v_sync_width
        20, // h_back_porch
        3, // v_back_porch
        800, // h_disp_active
        480, // v_disp_active
        70, // h_front_porch
        3, // v_front_porch
      },
      NV_FALSE // partial
    },

    // WVGA, 24 bit
    { 800, // width
      480, // height
      24, // bpp
      0, // refresh
      0, // frequency
      0, // flags
      // timing
      { 4, // h_ref_to_sync
        2, // v_ref_to_sync
        10, // h_sync_width
        3, // v_sync_width
        20, // h_back_porch
        3, // v_back_porch
        800, // h_disp_active
        480, // v_disp_active
        70, // h_front_porch
        3, // v_front_porch
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

NvBool sharpwvga_panel_init( NvOdmDispDeviceHandle hDevice, NvU32 bpp );
NvBool sharpwvga_discover( NvOdmDispDeviceHandle hDevice );
void sharpwvga_suspend( NvOdmDispDeviceHandle hDevice );
void sharpwvga_resume( NvOdmDispDeviceHandle hDevice );

static void
sharpwvga_nullmode( NvOdmDispDeviceHandle hDevice )
{
    hDevice->CurrentMode.width = 0;
    hDevice->CurrentMode.height = 0;
    hDevice->CurrentMode.bpp = 0;
}

NvBool sharpwvga_Setup( NvOdmDispDeviceHandle hDevice )
{
    if( !hDevice->bInitialized )
    {
        NvU32 gpioPinCount;

        // FIXME: report a 16 bit panel for the 16 bit mode
        hDevice->Type = NvOdmDispDeviceType_TFT;
        hDevice->Usage = NvOdmDispDeviceUsage_Unassigned;
        hDevice->MaxHorizontalResolution = 800;
        hDevice->MaxVerticalResolution = 480;
        hDevice->BaseColorSize = NvOdmDispBaseColorSize_888;
        hDevice->DataAlignment = NvOdmDispDataAlignment_MSB;
        hDevice->PinMap = NvOdmDispPinMap_Single_Rgb24_Spi5;
        hDevice->PwmOutputPin = NVODM_DISP_GPIO_MAKE_PIN('w'-'a', 1);
        hDevice->modes = sharpwvga_modes;
        hDevice->nModes = NV_ARRAY_SIZE(sharpwvga_modes);
        hDevice->CurrentPower = NvOdmDispDevicePowerLevel_Off;

        sharpwvga_nullmode( hDevice );

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

void sharpwvga_Release( NvOdmDispDeviceHandle hDevice )
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

    sharpwvga_nullmode( hDevice );
}

void sharpwvga_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
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

NvBool sharpwvga_SetMode( NvOdmDispDeviceHandle hDevice,
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
            sharpwvga_panel_init( hDevice, mode->bpp );
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
        sharpwvga_nullmode( hDevice );
    }

    return NV_TRUE;
}

void sharpwvga_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    if( !s_bBacklight )
    {
        NvOdmBacklightInit( hDevice, NV_FALSE );
        s_bBacklight = NV_TRUE;
    }

    NvOdmBacklightIntensity( hDevice, intensity );
    s_backlight_intensity = intensity;
}

void sharpwvga_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level )
{
    NV_ASSERT( hDevice );
    hDevice->CurrentPower = level;

    switch( level ) {
    case NvOdmDispDevicePowerLevel_Off:
    case NvOdmDispDevicePowerLevel_Suspend:
        NvOdmBacklightIntensity( hDevice, 0 );
        sharpwvga_suspend( hDevice );
        break;
    case NvOdmDispDevicePowerLevel_On:
        sharpwvga_resume( hDevice );
        NvOdmBacklightIntensity( hDevice, s_backlight_intensity );
        break;
    default:
        break;
    }
}

NvBool sharpwvga_GetParameter( NvOdmDispDeviceHandle hDevice,
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

void * sharpwvga_GetConfiguration( NvOdmDispDeviceHandle hDevice )
{
    NvOdmDispTftConfig *tft = &hDevice->Config.tft;
    return (void *)tft;
}

void sharpwvga_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values )
{
    *nPins = 0;
}

NvBool sharpwvga_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
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

#define SPI( p, c ) \
    CmdData[0] = (p); \
    CmdData[1] = (c); \
    NvOdmSpiTransaction( hSpi, s_spi_chipselect, \
        PANEL_CONTROL_INTERFACE_SPEED_KHZ, 0, CmdData, 2, 9 );

NvBool
sharpwvga_discover( NvOdmDispDeviceHandle hDevice )
{
    NvU64 guid;
    NvOdmPeripheralConnectivity const *conn;

    /* get the main panel */
    NvOdmDispGetDefaultGuid(&guid);

    /* get the connectivity info */
    conn = NvOdmPeripheralGetGuid( guid );
    if( !conn )
    {
        NV_ASSERT(!"ERROR: Panel not found.");
        return NV_FALSE;
    }

    hDevice->PeripheralConnectivity = conn;

    return NV_TRUE;
}

void
sharpwvga_resume( NvOdmDispDeviceHandle hDevice )
{
    NvOdmPeripheralConnectivity const *conn;
    NvOdmServicesPmuHandle hPmu;
    NvOdmServicesSpiHandle hSpi;
    NvU32 settle_us;
    NvU8 CmdData[2];
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

    /* take panel out of reset */
    NvOdmGpioSetState( s_hGpio, s_hResetGpioPin, 0x1 );

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

    SPI( 0, 1 ); // swreset
    SPI( 1, 0 ); // no op
    SPI( 1, 0 ); // no op
    NvOdmOsWaitUS( 40000 );
    SPI( 0, 0x11 ); // slpout
    SPI( 1, 0 ); // no op
    SPI( 1, 0 ); // no op
    NvOdmOsWaitUS( 10000 );
    SPI( 0, 0x3a ); // colmod
    SPI( 1, 0x70 ); // 24bpp
    SPI( 1, 0 ); // no op
    SPI( 0, 0xb0 ); // ???
    SPI( 1, 1 );
    SPI( 1, 0xfe );
    SPI( 0, 0xb1 ); // ???
    SPI( 1, 0xde );
    SPI( 1, 0x21 );
    SPI( 0, 0xfc ); // ???
    SPI( 1, 0 );
    SPI( 1, 0x33 );
    SPI( 0, 0xb4 ); // ???
    SPI( 1, 0 );
    SPI( 1, 0 );
    SPI( 0, 0x29 ); // dspon
    SPI( 1, 0 ); // no op
    SPI( 1, 0 ); // no op
    NvOdmOsWaitUS( 5000 );

    NvOdmSpiClose( hSpi );
}

void
sharpwvga_suspend( NvOdmDispDeviceHandle hDevice )
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

    SPI( 0, 0x28 ); // dspoff
    NvOdmOsWaitUS( 120000 );
    SPI( 0, 0x10 ); // slpin

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
sharpwvga_panel_init( NvOdmDispDeviceHandle hDevice, NvU32 bpp )
{
    NvOdmPeripheralConnectivity const *conn;
    NvBool spi_found = NV_FALSE;
    NvU32 i;

    if( !hDevice->bInitialized )
    {
        return NV_FALSE;
    }

    /* get the peripheral config */
    if( !sharpwvga_discover( hDevice ) )
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

void sharpwvga_GetHal( NvOdmDispDeviceHandle hDevice )
{
    NV_ASSERT(hDevice);

    hDevice->pfnSetup = sharpwvga_Setup;
    hDevice->pfnRelease = sharpwvga_Release;
    hDevice->pfnListModes = sharpwvga_ListModes;
    hDevice->pfnSetMode = sharpwvga_SetMode;
    hDevice->pfnSetPowerLevel = sharpwvga_SetPowerLevel;
    hDevice->pfnGetParameter = sharpwvga_GetParameter;
    hDevice->pfnGetPinPolarities = sharpwvga_GetPinPolarities;
    hDevice->pfnGetConfiguration = sharpwvga_GetConfiguration;
    hDevice->pfnSetSpecialFunction = sharpwvga_SetSpecialFunction;

    // Backlight functions
    hDevice->pfnSetBacklight = sharpwvga_SetBacklight;
}
