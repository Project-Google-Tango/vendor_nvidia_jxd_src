/*
 * Copyright (c) 2008 - 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_disp.h"
#include "panel_tpo_wvga.h"
#include "nvodm_services.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query_gpio.h"
#include "nvodm_backlight.h"

#define PANEL_CONTROL_INTERFACE_SPEED_KHZ 1000

static NvOdmDispDeviceMode tpowvga_modes[] =
{
    // WVGA, 24 bit
    { 800, // width
      480, // height
      24, // bpp   // need to check? rui
      0, // refresh
      0, // frequency
      0, // flags
      // timing
      { 4, // h_ref_to_sync
        2, // v_ref_to_sync
        1, // h_sync_width
        1, // v_sync_width
        216, // h_back_porch
        35, // v_back_porch
        800, // h_disp_active
        480, // v_disp_active
        40, // h_front_porch
        10, // v_front_porch
      },
      NV_FALSE // partial
    },
};

static NvOdmServicesGpioHandle s_hGpio;
static NvOdmGpioPinHandle s_hResetGpioPin;
static NvOdmGpioPinHandle s_hLCDPWR0GpioPin;
static NvOdmGpioPinHandle s_hLCDPWR1GpioPin;
static NvU32 s_spi_instance = 0;
static NvU32 s_spi_chipselect = 0;

static NvBool s_bReopen;
static NvBool s_bBacklight;
static NvU8 s_backlight_intensity;

NvBool tpowvga_panel_init( NvOdmDispDeviceHandle hDevice, NvU32 bpp );
NvBool tpowvga_discover( NvOdmDispDeviceHandle hDevice );
void   tpowvga_resume( NvOdmDispDeviceHandle hDevice );
void   tpowvga_suspend( NvOdmDispDeviceHandle hDevice );

static void
tpowvga_nullmode( NvOdmDispDeviceHandle hDevice )
{
    hDevice->CurrentMode.width = 0;
    hDevice->CurrentMode.height = 0;
    hDevice->CurrentMode.bpp = 0;
}

NvBool tpowvga_Setup( NvOdmDispDeviceHandle hDevice )
{
    if( !hDevice->bInitialized )
    {
        //NvU32 gpioPinCount;

        // 24 bit panel, 4-wire SPI
        hDevice->Type = NvOdmDispDeviceType_TFT;
        hDevice->Usage = NvOdmDispDeviceUsage_Unassigned;
        hDevice->MaxHorizontalResolution = 800;
        hDevice->MaxVerticalResolution = 480;
        hDevice->BaseColorSize = NvOdmDispBaseColorSize_888;
        hDevice->DataAlignment = NvOdmDispDataAlignment_MSB;
        hDevice->PinMap = NvOdmDispPinMap_Single_Rgb24_Spi5;  // need to check? rui
        hDevice->PwmOutputPin = NvOdmDispPwmOutputPin_Unspecified;
        hDevice->modes = tpowvga_modes;
        hDevice->nModes = NV_ARRAY_SIZE(tpowvga_modes);
        hDevice->CurrentPower = NvOdmDispDevicePowerLevel_Off;

        tpowvga_nullmode( hDevice );
/*
        if( !NvOdmBacklightInit(hDevice, s_bReopen) )
        {
            return NV_FALSE;
        }
*/
        s_hGpio = NvOdmGpioOpen();
        if( !s_hGpio )
        {
            return NV_FALSE;
        }

        hDevice->bInitialized = NV_TRUE;
    }

    return NV_TRUE;
}

void tpowvga_Release( NvOdmDispDeviceHandle hDevice )
{
    hDevice->bInitialized = NV_FALSE;

    // FIXME: send display off command
    NvOdmBacklightIntensity( hDevice, 0 );
    NvOdmBacklightDeinit(hDevice);
    s_bBacklight = NV_FALSE;

    NvOdmGpioReleasePinHandle(s_hGpio, s_hResetGpioPin);
    NvOdmGpioReleasePinHandle(s_hGpio, s_hLCDPWR0GpioPin);
    NvOdmGpioReleasePinHandle(s_hGpio, s_hLCDPWR1GpioPin);

    NvOdmGpioClose(s_hGpio);
    s_hResetGpioPin = 0;
    s_hLCDPWR0GpioPin = 0;
    s_hLCDPWR1GpioPin = 0;
    s_hGpio = 0;

    tpowvga_nullmode( hDevice );
}

void tpowvga_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
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

NvBool tpowvga_SetMode( NvOdmDispDeviceHandle hDevice,
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
            tpowvga_panel_init( hDevice, mode->bpp );
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
        tpowvga_nullmode( hDevice );
    }

    return NV_TRUE;
}

void tpowvga_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    if( !s_bBacklight )
    {
        NvOdmBacklightInit( hDevice, NV_FALSE );
        s_bBacklight = NV_TRUE;
    }

    NvOdmBacklightIntensity( hDevice, intensity );
    s_backlight_intensity = intensity;
}

void tpowvga_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
                            NvOdmDispDevicePowerLevel level )
{
    NV_ASSERT( hDevice );
    hDevice->CurrentPower = level;

    switch( level )
    {
        case NvOdmDispDevicePowerLevel_Off:
        case NvOdmDispDevicePowerLevel_Suspend:
            NvOdmBacklightIntensity( hDevice, 0 );
            tpowvga_suspend( hDevice );
            break;
        case NvOdmDispDevicePowerLevel_On:
            tpowvga_resume( hDevice );
            NvOdmBacklightIntensity( hDevice, s_backlight_intensity );
            break;
        default:
            break;
    }
}

NvBool tpowvga_GetParameter( NvOdmDispDeviceHandle hDevice,
                             NvOdmDispParameter param, NvU32 *val )
{
    NV_ASSERT( hDevice );
    NV_ASSERT( val );

    switch( param )
    {
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
        default:
            return NV_FALSE;
    }

    return NV_TRUE;
}

void * tpowvga_GetConfiguration( NvOdmDispDeviceHandle hDevice )
{
    NvOdmDispTftConfig *tft = &hDevice->Config.tft;
    return (void *)tft;
}

void tpowvga_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
                               NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values )
{
    *nPins = 4;
    Pins[0] = NvOdmDispPin_DataEnable;
    Values[0] = NvOdmDispPinPolarity_High;
    Pins[1] = NvOdmDispPin_HorizontalSync;
    Values[1] = NvOdmDispPinPolarity_Low;
    Pins[2] = NvOdmDispPin_VerticalSync;
    Values[2] = NvOdmDispPinPolarity_Low;
    Pins[3] = NvOdmDispPin_PixelClock;
    Values[3] = NvOdmDispPinPolarity_High;
}

NvBool tpowvga_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
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

#define SPI( p, c1) \
    CmdData[0] = (p); \
    CmdData[1] = (c1); \
    NvOdmSpiTransaction( hSpi, s_spi_chipselect, PANEL_CONTROL_INTERFACE_SPEED_KHZ, 0, CmdData, 2, 16 );

NvBool
tpowvga_discover( NvOdmDispDeviceHandle hDevice )
{
    NvU64 guid;
    NvOdmPeripheralConnectivity const *conn;

    /* get the main panel */
    guid = TPO_WVGA_GUID;

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
tpowvga_suspend( NvOdmDispDeviceHandle hDevice )
{
    NvOdmPeripheralConnectivity const *conn;
    NvOdmServicesPmuHandle hPmu;
    NvU32 i;

    /* Put panel into reset */
    NvOdmGpioSetState( s_hGpio, s_hResetGpioPin, 0x1 );

    /* Disable the power rail */
    hPmu = NvOdmServicesPmuOpen();
    conn = hDevice->PeripheralConnectivity;

    NV_ASSERT( conn );

    for ( i = 0; i < conn->NumAddress; i++ )
    {
        if ( conn->AddressList[i].Interface == NvOdmIoModule_Vdd )
        {
            /* set the rail volatage to the recommended */
            NvOdmServicesPmuSetVoltage( hPmu,
                                        conn->AddressList[i].Address,
                                        NVODM_VOLTAGE_OFF,
                                        0 );
        }
    }

    NvOdmServicesPmuClose( hPmu );
}

void
tpowvga_resume( NvOdmDispDeviceHandle hDevice )
{
    NvOdmPeripheralConnectivity const *conn;
    NvOdmServicesPmuHandle hPmu;
    NvOdmServicesSpiHandle hSpi;
    NvU32 settle_us;
    NvU32 i;
    NvU8 CmdData[2];

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
                                             conn->AddressList[i].Address,
                                             &cap );

            /* set the rail volatage to the recommended */
            NvOdmServicesPmuSetVoltage( hPmu,
                                        conn->AddressList[i].Address,
                                        cap.requestMilliVolts,
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
    NvOdmOsWaitUS( 10000 );
    NvOdmGpioSetState( s_hGpio, s_hResetGpioPin, 0x0 );
    NvOdmOsWaitUS( 10000 );
    NvOdmGpioSetState( s_hGpio, s_hResetGpioPin, 0x1 );
    NvOdmOsWaitUS( 10000 );

    // Correct GAMMA
    SPI(0x00, 0x00);  // R00
    SPI(0x04, 0xc1);
    SPI(0x08, 0x07);  // R02
    SPI(0x0C, 0x5F);
    SPI(0x10, 0x17);  // R04
    SPI(0x14, 0x20);
    SPI(0x18, 0x08);  // R06
    SPI(0x1C, 0x20);
    SPI(0x20, 0x20);  // R08
    SPI(0x24, 0x20);
    SPI(0x28, 0x20);  // R0A
    SPI(0x2C, 0x20);
    SPI(0x30, 0x20);  // R0C
    SPI(0x34, 0x22);
    SPI(0x38, 0x10);  // R0E
    SPI(0x3C, 0x10);
    SPI(0x40, 0x10);  // R10
    SPI(0x44, 0x15);
    SPI(0x48, 0x6A);  // R12
    SPI(0x4C, 0xFF);
    SPI(0x50, 0x86);  // R14
    SPI(0x54, 0x7C);
    SPI(0x58, 0xC2);  // R16
    SPI(0x5C, 0xD1);
    SPI(0x60, 0xF5);  // R18
    SPI(0x64, 0x25);
    SPI(0x68, 0x4A);  // R1A
    SPI(0x6C, 0xBF);
    SPI(0x70, 0x15);  // R1C
    SPI(0x74, 0x6A);
    SPI(0x78, 0xA4);  // R1E
    SPI(0x7C, 0xFF);
    SPI(0x80, 0xF0);  // R20
    SPI(0x84, 0xF0);
    SPI(0x88, 0x08);  // R22

    NvOdmOsWaitUS( 1000 );

    NvOdmSpiClose( hSpi );
}

NvBool
tpowvga_panel_init( NvOdmDispDeviceHandle hDevice, NvU32 bpp )
{
    NvOdmPeripheralConnectivity const *conn;
    NvBool spi_found = NV_FALSE;
    NvU32 i;

    if( !hDevice->bInitialized )
    {
        return NV_FALSE;
    }

    /* get the peripheral config */
    if( !tpowvga_discover( hDevice ) )
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
        }
        else if (conn->AddressList[i].Interface == NvOdmIoModule_Gpio)
        {
            //  There should only be one GPIO in the discovery
            //  database for this panel, corresponding to the reset pin.
            //  'b'-'a', 2

            s_hResetGpioPin = NvOdmGpioAcquirePinHandle(s_hGpio,
                                                        conn->AddressList[i].Instance,
                                                        conn->AddressList[i].Address);
        }
    }

    if (!spi_found)
    {
        return NV_FALSE;
    }

    if (!s_hResetGpioPin)
    {
        return NV_FALSE;
    }

    // Set LCD_PWR0 to be high
    // 'b' - 'a', 2
    s_hLCDPWR0GpioPin = NvOdmGpioAcquirePinHandle(s_hGpio,
                                                  'b' - 'a',
                                                  2);
    NvOdmGpioSetState( s_hGpio, s_hLCDPWR0GpioPin, 0x1 );
    NvOdmGpioConfig( s_hGpio, s_hLCDPWR0GpioPin, NvOdmGpioPinMode_Output);

    NvOdmOsWaitUS( 11000 );

    // Set LCD_PWR1 to be high
    // 'c' - 'a', 1
    s_hLCDPWR1GpioPin = NvOdmGpioAcquirePinHandle(s_hGpio,
                                                  'c' - 'a',
                                                  1);
    NvOdmGpioSetState( s_hGpio, s_hLCDPWR1GpioPin, 0x1 );
    NvOdmGpioConfig( s_hGpio, s_hLCDPWR1GpioPin, NvOdmGpioPinMode_Output);

    /* pull reset low to prevent chip select issues */
    if( !s_bReopen )
    {
        NvOdmGpioSetState( s_hGpio, s_hResetGpioPin, 0x1 );
        NvOdmGpioConfig( s_hGpio, s_hResetGpioPin, NvOdmGpioPinMode_Output);
    }

    return NV_TRUE;
}

void tpowvga_GetHal( NvOdmDispDeviceHandle hDevice )
{
    NV_ASSERT(hDevice);


    hDevice->pfnSetup = tpowvga_Setup;
    hDevice->pfnRelease = tpowvga_Release;
    hDevice->pfnListModes = tpowvga_ListModes;
    hDevice->pfnSetMode = tpowvga_SetMode;
    hDevice->pfnSetPowerLevel = tpowvga_SetPowerLevel;
    hDevice->pfnGetParameter = tpowvga_GetParameter;
    hDevice->pfnGetPinPolarities = tpowvga_GetPinPolarities;
    hDevice->pfnGetConfiguration = tpowvga_GetConfiguration;
    hDevice->pfnSetSpecialFunction = tpowvga_SetSpecialFunction;

    // Backlight functions
    hDevice->pfnSetBacklight = tpowvga_SetBacklight;

}
