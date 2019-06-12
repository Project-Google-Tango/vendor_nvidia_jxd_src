/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_disp.h"
#include "panel_samsungsdi.h"
#include "nvodm_services.h"
#include "nvodm_query_gpio.h"
//#include "nvodm_query_gpio_oem.h"
#include "nvodm_query_discovery.h"

#define PANEL_CONTROL_INTERFACE_SPEED_KHZ 1000

static NvOdmDispDeviceMode samsungsdi_modes[] =
{
    // 272X480 display resolution
    { 272, // width
      480, // height
      16,
      (60 << 16), // refresh
      15237, // frequency
      0, // flags
      // timing
      { 11, // h_ref_to_sync
        1, // v_ref_to_sync
        2, // h_sync_width
        2, // v_sync_width
        118,//120, // h_back_porch
        6,//8, // v_back_porch
        272, // h_disp_active
        480, // v_disp_active
        120, // h_front_porch
        8, // v_front_porch
      },
      NV_FALSE // partial
    },
};

static NvOdmServicesGpioHandle s_hGpio;
static NvOdmGpioPinHandle s_hResetGpioPin;
static NvOdmGpioPinHandle s_hEnableGpioPin;
static NvU32 s_spi_instance = 0;
static NvU32 s_spi_chipselect = 0;
/*
static NvBool s_bReopen;
static NvBool s_bBacklight;
*/
NvBool samsungsdi_panel_init( NvOdmDispDeviceHandle hDevice);
NvBool samsungsdi_discover( NvOdmDispDeviceHandle hDevice );
void samsungsdi_suspend( NvOdmDispDeviceHandle hDevice );
void samsungsdi_resume( NvOdmDispDeviceHandle hDevice );
void samsungsdi_poweroff( NvOdmDispDeviceHandle hDevice );

static void
samsungsdi_nullmode( NvOdmDispDeviceHandle hDevice )
{
    hDevice->CurrentMode.width = 0;
    hDevice->CurrentMode.height = 0;
    hDevice->CurrentMode.bpp = 0;
}

NvBool samsungsdi_Setup( NvOdmDispDeviceHandle hDevice )
{
    if( !hDevice->bInitialized )
    {
        hDevice->Type = NvOdmDispDeviceType_TFT;
        hDevice->Usage = NvOdmDispDeviceUsage_Unassigned;
        hDevice->MaxHorizontalResolution = 272;
        hDevice->MaxVerticalResolution = 480;
        hDevice->BaseColorSize = NvOdmDispBaseColorSize_888;
        hDevice->DataAlignment = NvOdmDispDataAlignment_MSB;
        hDevice->PinMap = NvOdmDispPinMap_Single_Rgb24_Spi5;
        hDevice->PwmOutputPin = NvOdmDispPwmOutputPin_Unspecified;
        hDevice->modes = samsungsdi_modes;
        hDevice->nModes = NV_ARRAY_SIZE(samsungsdi_modes);
        hDevice->CurrentPower = NvOdmDispDevicePowerLevel_Off;

        samsungsdi_nullmode( hDevice );

        hDevice->bInitialized = NV_TRUE;
    }

    return NV_TRUE;
}

void samsungsdi_Release( NvOdmDispDeviceHandle hDevice )
{
    hDevice->bInitialized = NV_FALSE;
/*
    NvOdmBacklightIntensity( 0 );
    NvOdmBacklightDeinit();
    s_bBacklight = NV_FALSE;
*/
    NvOdmGpioReleasePinHandle(s_hGpio, s_hResetGpioPin);
    NvOdmGpioClose(s_hGpio);
    s_hResetGpioPin = 0;
    s_hGpio = 0;

    samsungsdi_nullmode( hDevice );
}

void samsungsdi_ListModes( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
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

NvBool samsungsdi_SetMode( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDeviceMode *mode, NvU32 flags )
{
    NV_ASSERT( hDevice );

    if( mode )
    {
/*
        if( flags & NVODM_DISP_MODE_FLAGS_REOPEN )
        {
            s_bReopen = NV_TRUE;
        }
*/
        // FIXME: handle real mode changes
        if( !hDevice->CurrentMode.width && !hDevice->CurrentMode.height &&
            !hDevice->CurrentMode.bpp )
        {
            samsungsdi_panel_init( hDevice);
        }
/*
        if( !s_bBacklight && !NvOdmBacklightInit( s_bReopen ) )
        {
            return NV_FALSE;
        }

        s_bBacklight = NV_TRUE;
*/
        // FIXME: check for normal/partial mode
        hDevice->CurrentMode = *mode;
    }
    else
    {
/*
        // FIXME: send power off sequence
        if( !s_bBacklight )
        {
            if( !NvOdmBacklightInit( 0 ) )
            {
                return NV_FALSE;
            }
            s_bBacklight = NV_TRUE;
        }
        NvOdmBacklightIntensity( 0 );
*/
    }

    return NV_TRUE;
}

void samsungsdi_SetBacklight( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
/*
    if( !s_bBacklight )
    {
        NvOdmBacklightInit( NV_FALSE );
        s_bBacklight = NV_TRUE;
    }

    NvOdmBacklightIntensity( intensity );
*/
}

void samsungsdi_SetPowerLevel( NvOdmDispDeviceHandle hDevice,
    NvOdmDispDevicePowerLevel level )
{
    NV_ASSERT( hDevice );
    hDevice->CurrentPower = level;

    switch( level ) {
    case NvOdmDispDevicePowerLevel_Off:
        samsungsdi_poweroff( hDevice );
        break;
    case NvOdmDispDevicePowerLevel_On:
        samsungsdi_resume( hDevice );
        break;
    case NvOdmDispDevicePowerLevel_Suspend:
        samsungsdi_suspend( hDevice );
        break;
    case NvOdmDispDevicePowerLevel_Num:
    case NvOdmDispDevicePowerLevel_Force32:
    default:
        break;
    }
}

NvBool samsungsdi_GetParameter( NvOdmDispDeviceHandle hDevice,
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
    default:
        return NV_FALSE;
    }

    return NV_TRUE;
}

void * samsungsdi_GetConfiguration( NvOdmDispDeviceHandle hDevice )
{
    NvOdmDispTftConfig *tft = &hDevice->Config.tft;
    return (void *)tft;
}

void samsungsdi_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values )
{
    *nPins = 4;
    Pins[0] = NvOdmDispPin_DataEnable;
    Values[0] = NvOdmDispPinPolarity_Low;
    Pins[1] = NvOdmDispPin_HorizontalSync;
    Values[1] = NvOdmDispPinPolarity_Low;
    Pins[2] = NvOdmDispPin_VerticalSync;
    Values[2] = NvOdmDispPinPolarity_Low;
    Pins[3] = NvOdmDispPin_PixelClock;
    Values[3] = NvOdmDispPinPolarity_Low;
}
NvBool samsungsdi_SetSpecialFunction( NvOdmDispDeviceHandle hDevice,
    NvOdmDispSpecialFunction function, void *config )
{
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
    NvOdmSpiTransaction( hSpi, s_spi_chipselect, PANEL_CONTROL_INTERFACE_SPEED_KHZ, 0, CmdData, 2, 16 );

NvBool
samsungsdi_discover( NvOdmDispDeviceHandle hDevice )
{
    NvU64 guid;
    NvOdmPeripheralConnectivity const *conn;

    /* get the main panel */
    guid = SAMSUNG_DSI_GUID;

    /* get the connectivity info */
    conn = NvOdmPeripheralGetGuid( guid );
    if( !conn )
    {
        return NV_FALSE;
    }

    hDevice->PeripheralConnectivity = conn;

    return NV_TRUE;
}

void
samsungsdi_resume( NvOdmDispDeviceHandle hDevice )
{
    NvOdmPeripheralConnectivity const *conn;
    NvOdmServicesPmuHandle hPmu;
    NvOdmServicesSpiHandle hSpi;
    NvU32 settle_us;
    NvU8 CmdData[2];
    NvU32 i;

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

    /* take panel out of reset */
    NvOdmOsWaitUS( 20000 ); // <= 10mS
    NvOdmGpioSetState(s_hGpio, s_hResetGpioPin, 0x0);
    NvOdmOsWaitUS( 2000 ); // <= 1mS
    NvOdmGpioSetState(s_hGpio, s_hResetGpioPin, 0x1);
    NvOdmOsWaitUS( 10000 ); // <= 1mS

    NvOdmServicesPmuClose( hPmu );
/*
    if( s_bReopen )
    {
        // the bootloader has already initialized the panel, skip this resume.
        s_bReopen = NV_FALSE;
        return;
    }
*/
   hSpi = NvOdmSpiOpen( NvOdmIoModule_Spi, s_spi_instance );
/*
    hSpi = NvOdmSpiOpen( NvOdmIoModule_Spi, s_spi_instance );
    if( !hSpi )
    {
        return;
    }
*/
#if 0 // this shouldn't be required unless we are multiplexed - see use of NvOdmSpiPinMuxOpen in panel_sharp_wvga.c
    DO(
        NvOdmSetPinMuxConfiguration( hSpi, NvOdmIoModule_Spi, s_spi_instance,
            NvOdmSpiPinMap_Config3 )
    );
#endif


    //Analog setting
    SPI( 0x01, 0x00 );  // PANEL TYPE
    SPI( 0x21, 0x22 );  // DCDC
    SPI( 0x22, 0x03 );  // VCL1
    SPI( 0x23, 0x18 );  // VREGOUT
    SPI( 0x24, 0x33 );  // VGH/VGL
    SPI( 0x25, 0x33 );  // VSUS/VINT
    SPI( 0x26, 0x00 );  // BT
    SPI( 0x27, 0x42 );  // LA/CA
    SPI( 0x2F, 0x02 );  // Compensation
    SPI( 0x20, 0x01 );  // Booster(1)
    NvOdmOsWaitUS( 200000 ); // <= 10mS
    SPI( 0x20, 0x11 );  // Booster(2)
    NvOdmOsWaitUS( 400000 ); // <= 20mS
    SPI( 0x20, 0x31 );  // Booster(3)
    NvOdmOsWaitUS( 120000 );    // <= 60mS
    SPI( 0x20, 0x71 );  // Booster(4)
    NvOdmOsWaitUS( 120000 );    // <= 60mS
    SPI( 0x20, 0x73 );  // Booster(5)
    NvOdmOsWaitUS( 400000 ); // <= 20mS
    SPI( 0x20, 0x77 );  // Booster(6)
    NvOdmOsWaitUS( 200000 ); // <= 10mS
    SPI( 0x04, 0x01 );  // AMP_ON
    NvOdmOsWaitUS( 200000 ); // <= 10mS
    SPI( 0x06, 0x55 );  // Panel control1
    SPI( 0x07, 0x05 );  // Panel control2
    SPI( 0x08, 0x00 );  // Panel control3
    SPI( 0x09, 0x58 );  // Panel control4
    SPI( 0x0A, 0x21 );  // Panel control5
    SPI( 0x0C, 0x00 );  // Display control4
    SPI( 0x0D, 0x11 );  // Display control5
    SPI( 0x0E, 0x00 );  // Display control6
    SPI( 0x0F, 0x1E );  // Display control7
    SPI( 0x10, 0x00 );  // Data Mode
    SPI( 0x1C, 0x08 );  // VFP
    SPI( 0x1D, 0x05 );  // VBP
    SPI( 0x1F, 0x10 );  // tcl: was 0 // low active test 0x01;
    SPI( 0x30, 0x1A );
    SPI( 0x31, 0x22 );
    SPI( 0x32, 0x22 );
    SPI( 0x33, 0x5A );
    SPI( 0x34, 0x67 );
    SPI( 0x35, 0x7F );
    SPI( 0x36, 0x1E );
    SPI( 0x37, 0x1C );
    SPI( 0x38, 0x19 );
    SPI( 0x39, 0x27 );
    SPI( 0x3A, 0x23 );
    SPI( 0x3B, 0x21 );
    SPI( 0x3C, 0x26 );
    SPI( 0x3D, 0x1E );
    SPI( 0x3E, 0x1B );
    SPI( 0x3F, 0x04 );
    SPI( 0x40, 0x1E );
    SPI( 0x41, 0x26 );
    SPI( 0x04, 0x05 );  // EL_ON
    // Display Data Start

    // Wait several frames
    // NvOdmOsWaitUS( 200000 );    // <= 100mS


    // DISPLAY ON
    SPI( 0x04, 0x07 );  // Display ON

    // Wait several frames
    NvOdmOsWaitUS( 200000 );    // <= 100mS

    // enable V_OLED_ELVDD and V_OLED_ELVSS
    NvOdmGpioSetState( s_hGpio, s_hEnableGpioPin, 0x1 );

    // Wait several frames
    NvOdmOsWaitUS( 200000 );    // <= 100mS


#if 0 // this shouldn't be required unless we are multiplexed - see use of NvOdmSpiPinMuxOpen in panel_sharp_wvga.c
    DO(
        NvOdmSetPinMuxConfiguration( hSpi, NvOdmIoModule_Spi, s_spi_instance,
            NvOdmSpiPinMap_Multiplexed )
    );
#endif

    NvOdmSpiClose( hSpi );
}

void
samsungsdi_poweroff( NvOdmDispDeviceHandle hDevice )
{
    NvOdmPeripheralConnectivity const *conn;
    NvOdmServicesPmuHandle hPmu;
    NvOdmServicesSpiHandle hSpi;
    NvU8 CmdData[2];
    NvU32 i;

    /* get the spi instance and chip select */
    conn = hDevice->PeripheralConnectivity;
    for( i = 0; i < conn->NumAddress; i++ )
    {
        if( conn->AddressList[i].Interface == NvOdmIoModule_Spi )
        {
            s_spi_instance = conn->AddressList[i].Instance;
            s_spi_chipselect = conn->AddressList[i].Address;
            break;
        }
    }

    hSpi = NvOdmSpiOpen( NvOdmIoModule_Spi, s_spi_instance );
    if( !hSpi )
    {
        return;
    }

    SPI( 0x04, 0x03 );  // EL_ON = 0
    NvOdmOsWaitUS( 400000 );    // <= 100mS
    /* Display Off Squence */
    SPI( 0x04, 0x01 );  // EL_ON = 0
    NvOdmOsWaitUS( 120000 );    // <= 60mS
    SPI( 0x04, 0x00 );  // ON/OFF = 0
    SPI( 0x20, 0x00 );  // Booster(1)
    SPI( 0x03, 0x02 );  // Main Power(1)
    NvOdmOsWaitUS( 20000 ); // <= 10mS

    NvOdmGpioSetState( s_hGpio, s_hResetGpioPin, 0x0 );

    NvOdmSpiClose( hSpi );

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

void
samsungsdi_suspend( NvOdmDispDeviceHandle hDevice )
{
    NvOdmPeripheralConnectivity const *conn;
    NvOdmServicesSpiHandle hSpi;
    NvU8 CmdData[2];
    NvU32 i;

    /* get the spi instance and chip select */
    conn = hDevice->PeripheralConnectivity;
    for( i = 0; i < conn->NumAddress; i++ )
    {
        if( conn->AddressList[i].Interface == NvOdmIoModule_Spi )
        {
            s_spi_instance = conn->AddressList[i].Instance;
            s_spi_chipselect = conn->AddressList[i].Address;
            break;
        }
    }

    hSpi = NvOdmSpiOpen( NvOdmIoModule_Spi, s_spi_instance );
    if( !hSpi )
    {
        return;
    }

    /* Display Off Squence */
    SPI( 0x04, 0x03 );  // EL_ON = 0
    NvOdmOsWaitUS( 400000 );    // <= 100mS
    /* Display Off Squence */
    SPI( 0x04, 0x01 );  // EL_ON = 0
    NvOdmOsWaitUS( 120000 ); // <= 60mS
    SPI( 0x04, 0x00 );  // ON/OFF = 0
    SPI( 0x20, 0x00 );  // Booster(1)
    SPI( 0x03, 0x02 );  // Main Power(1)
    NvOdmOsWaitUS( 20000 ); // <= 10mS

    NvOdmSpiClose( hSpi );
}

NvBool
samsungsdi_panel_init( NvOdmDispDeviceHandle hDevice)
{
    NvOdmPeripheralConnectivity const *conn;
    NvBool spi_found = NV_FALSE;
    NvU32 i;

    if( !hDevice->bInitialized )
    {
        return NV_FALSE;
    }

    /* get the peripheral config */
    if( !samsungsdi_discover( hDevice ) )
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

    /* acquire GPIO pins */
    s_hGpio = NvOdmGpioOpen();
    if( !s_hGpio )
    {
        return NV_FALSE;
    }

    s_hResetGpioPin = NvOdmGpioAcquirePinHandle(s_hGpio,
                                                'v'-'a',
                                                7);

    s_hEnableGpioPin = NvOdmGpioAcquirePinHandle(s_hGpio,
                                                'u'-'a',
                                                2);

    /* pull reset low to prevent chip select issues */
    NvOdmGpioSetState( s_hGpio, s_hResetGpioPin, 0x1 );
    NvOdmGpioConfig( s_hGpio, s_hResetGpioPin, NvOdmGpioPinMode_Output);

    /* set enable pin low (supplies off before reset) */
    NvOdmGpioSetState( s_hGpio, s_hEnableGpioPin, 0x0 );
    NvOdmGpioConfig( s_hGpio, s_hEnableGpioPin, NvOdmGpioPinMode_Output);

    return NV_TRUE;
}

void samsungdsi_GetHal( NvOdmDispDeviceHandle hDevice )
{
    NV_ASSERT(hDevice);

    hDevice->pfnSetup = samsungsdi_Setup;
    hDevice->pfnRelease = samsungsdi_Release;
    hDevice->pfnListModes = samsungsdi_ListModes;
    hDevice->pfnSetMode = samsungsdi_SetMode;
    hDevice->pfnSetPowerLevel = samsungsdi_SetPowerLevel;
    hDevice->pfnGetParameter = samsungsdi_GetParameter;
    hDevice->pfnGetPinPolarities = samsungsdi_GetPinPolarities;
    hDevice->pfnGetConfiguration = samsungsdi_GetConfiguration;
    hDevice->pfnSetSpecialFunction = samsungsdi_SetSpecialFunction;

    // Backlight functions
    hDevice->pfnSetBacklight = samsungsdi_SetBacklight;
}
