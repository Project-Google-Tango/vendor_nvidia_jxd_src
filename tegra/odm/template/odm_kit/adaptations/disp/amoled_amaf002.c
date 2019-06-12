/**
 * Copyright (c) 2008 - 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*  NVIDIA Tegra ODM Kit Sample Display Adaptation for the Samsung
 *  AMOLED-AMAF002 WQVGA LCD
 */

#include "nvodm_disp.h"
#include "display_hal.h"
#include "nvodm_services.h"
#include "nvodm_query_discovery.h"
#include "amoled_amaf002.h"

#define PANEL_CONTROL_INTERFACE_SPEED_KHZ 8000

//  This define should be 0 or 1, to indicate whether to use dynamic
//  allocation of the instance data, or static allocation
#define ADAPTATION_USES_DYNAMIC_ALLOC 1

//  This adaptation should be either 0 or a valid PinMap for the
//  SPI instance used by the display, if the display's SPI controller
//  is being multiplexed across multiple pin mux configurations
#define ADAPTATION_SUPPORTS_MULTIPLEXED_SPI 0

static const NvOdmDispDeviceMode
AmoLedAmaF002Modes[] =
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

typedef struct AmoLedStateRec
{
    NvOdmDispTftConfig Config;
    const NvOdmPeripheralConnectivity *pConn;
    NvOdmServicesGpioHandle hGpio;
    NvOdmGpioPinHandle hResetPin;
    NvU32 SpiReference;
} AmoLedState;


/*  This helper function walks the connectivity list,
 *  searching for voltage rails, and enables (or
 *  disables) each */

static void
SetVoltageRails(const NvOdmPeripheralConnectivity *pConn,
                NvBool Enable)
{
    NvOdmServicesPmuHandle hPmu;
    NvU32 i;

    if (!pConn)
        return;

    hPmu = NvOdmServicesPmuOpen();
    if (!hPmu)
        return;

    for(i = 0; i<pConn->NumAddress; i++)
    {
        if (pConn->AddressList[i].Interface == NvOdmIoModule_Vdd)
        {
            if (Enable)
            {
                NvOdmServicesPmuVddRailCapabilities Cap;
                NvU32 SettleTime;

                /* address is the vdd rail id */
                NvOdmServicesPmuGetCapabilities(hPmu,
                    pConn->AddressList[i].Address, &Cap );

                /* set the rail volatage to the recommended */
                NvOdmServicesPmuSetVoltage(hPmu,
                    pConn->AddressList[i].Address,
                    Cap.requestMilliVolts,
                    &SettleTime);

                /* wait for rail to settle */
                NvOdmOsWaitUS(SettleTime);
            }
            else
            {
                NvOdmServicesPmuSetVoltage(hPmu,
                    pConn->AddressList[i].Address,
                    NVODM_VOLTAGE_OFF, 0);
            }
        }
    }

    NvOdmServicesPmuClose(hPmu);
}

//  The display adaptation HAL will only call Setup when the
//  panel isn't already initialized, so no state checking is
//  required on input to this function
static NvBool
AmoLedAmaF002_Setup(NvOdmDispDeviceHandle hDevice)
{
    //  The AmoLedInstance structure is dynamically allocated,
    //  in case there is a design that happens to use multiple
    //  panels on the same device (e.g., identical inner and
    //  outer display on a clamshell.  Alternatively, it could
    //  be declared as a static variable on designs where only
    //  a single panel is used.
    AmoLedState *AmoLedInstance = NULL;
    const NvOdmPeripheralConnectivity *pConn = NULL;
    NvU32 i;

#if !ADAPTATION_USES_DYNAMIC_ALLOC
    static AmoLedState InstanceState;
#endif

    //  Setup for this panel involves allocation of the state
    //  object, detection of the SPI controller instance and
    //  chip select
    pConn = NvOdmPeripheralGetGuid(AMOLED_AMAF002_GUID);
    NV_ASSERT(pConn!=NULL);
    if (!pConn )
        goto cleanup;

#if ADAPTATION_USES_DYNAMIC_ALLOC
    AmoLedInstance = (AmoLedState *)NvOdmOsAlloc(sizeof(AmoLedState));
#else
    AmoLedInstance = &InstanceState;
#endif

    NV_ASSERT(AmoLedInstance!=NULL);
    if (!AmoLedInstance)
        goto cleanup;

    NvOdmOsMemset(AmoLedInstance, 0, sizeof(AmoLedInstance));

    AmoLedInstance->pConn = pConn;

    AmoLedInstance->SpiReference = ~0UL;

    hDevice->Type = NvOdmDispDeviceType_TFT;
    hDevice->Usage = NvOdmDispDeviceUsage_Unassigned;
    hDevice->MaxHorizontalResolution = AmoLedAmaF002Modes[0].width;
    hDevice->MaxVerticalResolution = AmoLedAmaF002Modes[0].height;
    hDevice->BaseColorSize = NvOdmDispBaseColorSize_888;
    hDevice->DataAlignment = NvOdmDispDataAlignment_MSB;
    hDevice->PinMap = NvOdmDispPinMap_Single_Rgb24_Spi5;
    hDevice->PwmOutputPin = NvOdmDispPwmOutputPin_Unspecified;
    hDevice->CurrentPower = NvOdmDispDevicePowerLevel_Off;

    AmoLedInstance->hGpio = NvOdmGpioOpen();
    if (!AmoLedInstance->hGpio)
        goto cleanup;

    for (i=0; i<pConn->NumAddress; i++)
    {
        if (pConn->AddressList[i].Interface == NvOdmIoModule_Spi)
        {
            //  There should only be one SPI controller in the
            //  discovery database for this panel.  Assert if more
            //  than one is found.
            NV_ASSERT(AmoLedInstance->SpiReference==~0UL);
            AmoLedInstance->SpiReference = i;
        }
        else if (pConn->AddressList[i].Interface == NvOdmIoModule_Gpio)
        {
            //  There should only be one GPIO in the discovery
            //  database for this panel, corresponding to the reset pin.
            //  Assert if more than one is found
            NV_ASSERT(!AmoLedInstance->hResetPin);
            AmoLedInstance->hResetPin =
                NvOdmGpioAcquirePinHandle(AmoLedInstance->hGpio,
                    pConn->AddressList[i].Instance,
                    pConn->AddressList[i].Address);
            if (!AmoLedInstance->hResetPin)
                goto cleanup;
        }
    }

    //  Make sure that a SPI controller was found
    if (AmoLedInstance->SpiReference==~0UL)
        goto cleanup;

    //  All resources were successfully acquired, so
    hDevice->pPrivateDisplay = (void *)AmoLedInstance;
    return NV_TRUE;

cleanup:
    if (AmoLedInstance)
    {
        if (AmoLedInstance->hResetPin)
            NvOdmGpioReleasePinHandle(AmoLedInstance->hGpio,
                                      AmoLedInstance->hResetPin);

        if (AmoLedInstance->hGpio)
            NvOdmGpioClose(AmoLedInstance->hGpio);

#if ADAPTATION_USES_DYNAMIC_ALLOC
        NvOdmOsFree(AmoLedInstance);
#endif
    }
    hDevice->pPrivateDisplay = NULL;
    NV_ASSERT(!"Unable to acquire resources for AMOLED AMA-F002");
    return NV_FALSE;
}

static void
AmoLedAmaF002_Release(NvOdmDispDeviceHandle hDevice)
{
    AmoLedState *AmoLedInstance = NULL;

    AmoLedInstance = (AmoLedState *)hDevice->pPrivateDisplay;

    //  If setup did not succeed, don't try to release.
    if (!AmoLedInstance)
        return;

    NvOdmGpioReleasePinHandle(AmoLedInstance->hGpio,
                              AmoLedInstance->hResetPin);

    NvOdmGpioClose(AmoLedInstance->hGpio);

    NvOdmOsMemset(AmoLedInstance, 0, sizeof(AmoLedState));

#if ADAPTATION_USES_DYNAMIC_ALLOC
    NvOdmOsFree(AmoLedInstance);
#endif

    hDevice->pPrivateDisplay = NULL;
}

static void
AmoLedAmaF002_ListModes(NvOdmDispDeviceHandle hDevice,
                        NvU32 *pNumModes,
                        NvOdmDispDeviceMode *pModes)
{
    NV_ASSERT(pNumModes);

    if (!pModes)
    {
        *pNumModes = NV_ARRAY_SIZE(AmoLedAmaF002Modes);
    }
    else
    {
        NvU32 i, Len;
        Len = NV_MIN(*pNumModes, NV_ARRAY_SIZE(AmoLedAmaF002Modes));
        for (i=0; i<Len; i++)
            pModes[i] = AmoLedAmaF002Modes[i];
    }
}

static NvBool
AmoLedAmaF002_SetMode(NvOdmDispDeviceHandle hDevice,
                      NvOdmDispDeviceMode *pMode, NvU32 flags)
{
    AmoLedState *AmoLedInstance = NULL;
    NV_ASSERT(hDevice);

    AmoLedInstance = (AmoLedState *)hDevice->pPrivateDisplay;
    NV_ASSERT(AmoLedInstance);

    if (pMode)
    {
        //  Panel transitions from Off->Suspend->Active.
        //  Prior to activating the reset pin needs to be pulled
        //  low.  However, this should only be done when the panel
        //  is not active.
        if (hDevice->CurrentPower != NvOdmDispDevicePowerLevel_Suspend)
        {
            NvOdmGpioSetState(AmoLedInstance->hGpio,
                AmoLedInstance->hResetPin, 0x0);
            NvOdmGpioConfig(AmoLedInstance->hGpio,
                AmoLedInstance->hResetPin, NvOdmGpioPinMode_Output);
        }
    }

    return NV_TRUE;
}

static NvOdmServicesSpiHandle
OpenSpiHandle(const NvOdmIoAddress *pAddr)
{
    NvOdmServicesSpiHandle hSpi = NULL;
    hSpi = NvOdmSpiOpen(pAddr->Interface, pAddr->Instance);
    return hSpi;
}

#define SPI( SPI, CS, REG, VAL ) \
    do { \
        NvU8 CmdData[2]; \
        CmdData[0] = (REG); \
        CmdData[1] = (VAL); \
        NvOdmSpiTransaction( (SPI), (CS), PANEL_CONTROL_INTERFACE_SPEED_KHZ, \
                             0, CmdData, NV_ARRAY_SIZE(CmdData), 16 ); \
    } while (0);


static void
PanelSuspend(const AmoLedState *AmoLedInstance)
{
    NvOdmServicesSpiHandle hSpi = NULL;
    const NvOdmIoAddress *pAddr = NULL;

    pAddr = &AmoLedInstance->pConn->AddressList[AmoLedInstance->SpiReference];

    hSpi = OpenSpiHandle(pAddr);
    NV_ASSERT(hSpi != NULL);

    /* Display Off Squence */
    SPI(hSpi, pAddr->Address, 0x04, 0x03);  // EL_ON = 0
    NvOdmOsWaitUS(400000);    // <= 100mS

    /* Display Off Squence */
    SPI(hSpi, pAddr->Address, 0x04, 0x01);  // EL_ON = 0
    NvOdmOsWaitUS(120000); // <= 60mS

    SPI(hSpi, pAddr->Address, 0x04, 0x00);  // ON/OFF = 0
    SPI(hSpi, pAddr->Address, 0x20, 0x00);  // Booster(1)
    SPI(hSpi, pAddr->Address, 0x03, 0x02);  // Main Power(1)
    NvOdmOsWaitUS(20000); // <= 10mS

    NvOdmSpiClose(hSpi);
}

static void
PanelResume(const AmoLedState *AmoLedInstance)
{
    NvOdmServicesSpiHandle hSpi = NULL;
    const NvOdmIoAddress *pAddr = NULL;

    pAddr = &AmoLedInstance->pConn->AddressList[AmoLedInstance->SpiReference];

    hSpi = OpenSpiHandle(pAddr);

    NV_ASSERT(hSpi != NULL);

    SetVoltageRails(AmoLedInstance->pConn, NV_TRUE);

    NvOdmOsWaitUS(20000);
    NvOdmGpioSetState(AmoLedInstance->hGpio, AmoLedInstance->hResetPin, 0x1);
    NvOdmOsWaitUS(2000);

    SPI(hSpi, pAddr->Address, 0x01, 0x00);  // PANEL TYPE
    SPI(hSpi, pAddr->Address, 0x21, 0x22);  // DCDC
    SPI(hSpi, pAddr->Address, 0x22, 0x03);  // VCL1

    SPI(hSpi, pAddr->Address, 0x23, 0x18);  // VREGOUT
    SPI(hSpi, pAddr->Address, 0x24, 0x33);  // VGH/VGL
    SPI(hSpi, pAddr->Address, 0x25, 0x33);  // VSUS/VINT
    SPI(hSpi, pAddr->Address, 0x26, 0x00);  // BT
    SPI(hSpi, pAddr->Address, 0x27, 0x42);  // LA/CA
    SPI(hSpi, pAddr->Address, 0x2F, 0x02);  // Compensation

    SPI(hSpi, pAddr->Address, 0x20, 0x01);  // Booster(1)
    NvOdmOsWaitUS(20000); // <= 10mS

    SPI(hSpi, pAddr->Address, 0x20, 0x11);  // Booster(2)
    NvOdmOsWaitUS(40000); // <= 20mS

    SPI(hSpi, pAddr->Address, 0x20, 0x31);  // Booster(3)
    NvOdmOsWaitUS(120000);    // <= 60mS

    SPI(hSpi, pAddr->Address, 0x20, 0x71);  // Booster(4)
    NvOdmOsWaitUS(120000);    // <= 60mS

    SPI(hSpi, pAddr->Address, 0x20, 0x73);  // Booster(5)
    NvOdmOsWaitUS(40000); // <= 20mS

    SPI(hSpi, pAddr->Address, 0x20, 0x77);  // Booster(6)
    NvOdmOsWaitUS(20000); // <= 10mS

    SPI(hSpi, pAddr->Address, 0x04, 0x01);  // AMP_ON
    NvOdmOsWaitUS(20000); // <= 10mS

    SPI(hSpi, pAddr->Address, 0x06, 0x55);  // Panel control1
    SPI(hSpi, pAddr->Address, 0x07, 0x05);  // Panel control2
    SPI(hSpi, pAddr->Address, 0x08, 0x00);  // Panel control3
    SPI(hSpi, pAddr->Address, 0x09, 0x58);  // Panel control4
    SPI(hSpi, pAddr->Address, 0x0A, 0x21);  // Panel control5
    SPI(hSpi, pAddr->Address, 0x0C, 0x00);  // Display control4
    SPI(hSpi, pAddr->Address, 0x0D, 0x11);  // Display control5
    SPI(hSpi, pAddr->Address, 0x0E, 0x00);  // Display control6
    SPI(hSpi, pAddr->Address, 0x0F, 0x1E);  // Display control7
    SPI(hSpi, pAddr->Address, 0x10, 0x00);  // Data Mode
    SPI(hSpi, pAddr->Address, 0x1C, 0x08);  // VFP
    SPI(hSpi, pAddr->Address, 0x1D, 0x05);  // VBP
    // sample on rising edge to prevent flickering
    SPI(hSpi, pAddr->Address, 0x1F, 0x10);
    SPI(hSpi, pAddr->Address, 0x30, 0x1A);
    SPI(hSpi, pAddr->Address, 0x31, 0x22);
    SPI(hSpi, pAddr->Address, 0x32, 0x22);
    SPI(hSpi, pAddr->Address, 0x33, 0x5A);
    SPI(hSpi, pAddr->Address, 0x34, 0x67);
    SPI(hSpi, pAddr->Address, 0x35, 0x7F);
    SPI(hSpi, pAddr->Address, 0x36, 0x1E);
    SPI(hSpi, pAddr->Address, 0x37, 0x1C);
    SPI(hSpi, pAddr->Address, 0x38, 0x19);
    SPI(hSpi, pAddr->Address, 0x39, 0x27);
    SPI(hSpi, pAddr->Address, 0x3A, 0x23);
    SPI(hSpi, pAddr->Address, 0x3B, 0x21);
    SPI(hSpi, pAddr->Address, 0x3C, 0x26);
    SPI(hSpi, pAddr->Address, 0x3D, 0x1E);
    SPI(hSpi, pAddr->Address, 0x3E, 0x1B);
    SPI(hSpi, pAddr->Address, 0x3F, 0x04);
    SPI(hSpi, pAddr->Address, 0x40, 0x1E);
    SPI(hSpi, pAddr->Address, 0x41, 0x26);
    SPI(hSpi, pAddr->Address, 0x04, 0x05);  // EL_ON
    // Display Data Start

    // Wait several frames
    NvOdmOsWaitUS(400000);    // <= 100mS

    // DISPLAY ON
    SPI(hSpi, pAddr->Address, 0x04, 0x07);  // Display ON
    NvOdmSpiClose(hSpi);
}

static void
AmoLedAmaF002_SetPowerLevel(NvOdmDispDeviceHandle hDevice,
                            NvOdmDispDevicePowerLevel Level)
{
    AmoLedState *AmoLedInstance = NULL;
    NV_ASSERT( hDevice );

    AmoLedInstance = (AmoLedState *)hDevice->pPrivateDisplay;
    NV_ASSERT(AmoLedInstance);

    switch(Level)
    {
    case NvOdmDispDevicePowerLevel_Off:
        PanelSuspend(AmoLedInstance);
        NvOdmGpioSetState(AmoLedInstance->hGpio,
                          AmoLedInstance->hResetPin, 0x0);
        break;

    case NvOdmDispDevicePowerLevel_On:
        PanelResume(AmoLedInstance);
        break;

    case NvOdmDispDevicePowerLevel_Suspend:
        PanelSuspend(AmoLedInstance);
        break;

    case NvOdmDispDevicePowerLevel_SelfRefresh:
    case NvOdmDispDevicePowerLevel_Num:
    case NvOdmDispDevicePowerLevel_Force32:
    default:
        break;
    }
}

static NvBool
AmoLedAmaF002_GetParameter(NvOdmDispDeviceHandle hDevice,
    NvOdmDispParameter param, NvU32 *val)
{
    NV_ASSERT(val);

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
        *val = (NvU32)NvOdmDispPwmOutputPin_Unspecified;
        break;
    default:
        return NV_FALSE;
    }

    return NV_TRUE;
}

static void*
AmoLedAmaF002_GetConfiguration(NvOdmDispDeviceHandle hDevice)
{
    AmoLedState *AmoLedInstance = NULL;

    NV_ASSERT(hDevice && hDevice->pPrivateDisplay);

    AmoLedInstance = (AmoLedState *)hDevice->pPrivateDisplay;
    return (void *)&AmoLedInstance->Config;
}

static void
AmoLedAmaF002_GetPinPolarities(NvOdmDispDeviceHandle hDevice,
                               NvU32 *pNumPins,
                               NvOdmDispPin *pPins,
                               NvOdmDispPinPolarity *pValues)
{
    *pNumPins = 4;
    NV_ASSERT(*pNumPins <= NvOdmDispPin_Num);

    pPins[0] = NvOdmDispPin_DataEnable;
    pValues[0] = NvOdmDispPinPolarity_Low;
    pPins[1] = NvOdmDispPin_HorizontalSync;
    pValues[1] = NvOdmDispPinPolarity_Low;
    pPins[2] = NvOdmDispPin_VerticalSync;
    pValues[2] = NvOdmDispPinPolarity_Low;
    pPins[3] = NvOdmDispPin_PixelClock;
    pValues[3] = NvOdmDispPinPolarity_Low;
}



void AmoLedAmaF002_GetHal(NvOdmDispDeviceHandle hDevice)
{
    NV_ASSERT(hDevice);

    hDevice->pfnSetup = AmoLedAmaF002_Setup;
    hDevice->pfnRelease = AmoLedAmaF002_Release;
    hDevice->pfnListModes = AmoLedAmaF002_ListModes;
    hDevice->pfnSetMode = AmoLedAmaF002_SetMode;
    hDevice->pfnSetPowerLevel = AmoLedAmaF002_SetPowerLevel;
    hDevice->pfnGetParameter = AmoLedAmaF002_GetParameter;
    hDevice->pfnGetPinPolarities = AmoLedAmaF002_GetPinPolarities;
    hDevice->pfnGetConfiguration = AmoLedAmaF002_GetConfiguration;
    hDevice->pfnSetSpecialFunction = NULL;
}
