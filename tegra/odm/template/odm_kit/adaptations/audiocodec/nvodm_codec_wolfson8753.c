/**
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * NVIDIA APX ODM Kit:: Wolfson Codec Implementation
 *
 * This file implements the Wolfson audio codec driver.
 */

#include "nvodm_services.h"
#include "nvodm_audiocodec.h"
#include "nvodm_query.h"
#include "audiocodec_hal.h"
#include "nvodm_query_discovery.h"
#include "wolfson8753_registers.h"
#include "nvodm_pmu.h"
#include "nvfxmath.h"

// Module debug: 0=disable, 1=enable
#define NVODM_WM8753_ENABLE_LOG         (0)
#define NVODM_WM8753_LOG_HW_ACCESS      (0)
#define WM8753_PREFIX                   "WM8753: "

#if NVODM_WM8753_ENABLE_LOG
    #define NVODM_WM8753_LOG(x)         NvOdmOsDebugPrintf x
    #define NVODM_CHECK_CONDITION(c, tag, str) \
        do { if(c) { NVODM_WM8753_LOG(str); goto tag; } } while (0)
#else
    #define NVODM_WM8753_LOG(x)         do {} while(0)
    #define NVODM_CHECK_CONDITION(c, tag, str) \
        do { if(c) { goto tag; } } while (0)
#endif

#define CODEC_INF_FREQ 1000 // 1MHz

#define WAIT_TIME_ON_VREF_TOSETTLE 700000 // 700 msec
//#define WAIT_TIME_ON_DAC_MUTE      25000 // 10 to 30 msec

// To enable the input recording, set this to 1.
#define ENABLE_RXN_SETTINGS 0
#define W8753_I2C_TIMEOUT 3000 // 1 seconds
#define W8753_I2C_SCLK  400

#define NVODM_I2C_IS_READ 0

#if ENABLE_RXN_SETTINGS
NvOdmAudioSignalType gs_InputSignalType = NvOdmAudioSignalType_MicIn;
NvOdmAudioSignalType gs_InputSignalId = 1;
#endif

//#define MIC_DETECT_ENABLE
//#define EAR_SPEAKER_ENABLE

#define W8753_SET_REG_VAL(r,f,n, v) \
    (((v)& (~(W8753_##r##_0_##f##_SEL << W8753_##r##_0_##f##_LOC))) | \
        (((n) & W8753_##r##_0_##f##_SEL) << W8753_##r##_0_##f##_LOC))

#define W8753_SET_REG_DEF(r,f,c,v) \
    (((v)& (~(W8753_##r##_0_##f##_SEL << W8753_##r##_0_##f##_LOC))) | \
        (((W8753_##r##_0_##f##_##c) & W8753_##r##_0_##f##_SEL) << W8753_##r##_0_##f##_LOC))

#define W8753_GET_REG_VAL(r,f,v) \
    ((((v) >> W8753_##r##_0_##f##_LOC)& W8753_##r##_0_##f##_SEL))

#define CONCORDEII_BOARDID  0x0B0F
#define WHISTLERID_E1107    0x0B07
#define WHISTLERID_E1108    0x0B08
#define WHISTLERID_E1109    0x0B09
#define WHISTLERID_E1117    0x0B11
#define WHISTLERID_E1120    0x0B14

// A list of the Whistler boards
static NvU32 gs_WhistlerBoards[] = {
    WHISTLERID_E1107,
    WHISTLERID_E1108,
    WHISTLERID_E1109,
    WHISTLERID_E1117,
    WHISTLERID_E1120
};

/*
 * Wolfson codec register sequences for 8753
 */
typedef enum
{
    WCodecRegIndex_DacControl               = 1,
    WCodecRegIndex_AdcControl,
    WCodecRegIndex_PcmAudioInterface,
    WCodecRegIndex_HiFiAudioInterface,
    WCodecRegIndex_InterfaceControl,
    WCodecRegIndex_SampleRateControl1,
    WCodecRegIndex_SampleRateControl2,
    WCodecRegIndex_LeftDacVolume,
    WCodecRegIndex_RightDacVolume,
    WCodecRegIndex_BaseControl,
    WCodecRegIndex_TrebleControl,
    WCodecRegIndex_Alc1,
    WCodecRegIndex_Alc2,
    WCodecRegIndex_Alc3,
    WCodecRegIndex_NoiseGate,
    WCodecRegIndex_LeftAdcVolume,
    WCodecRegIndex_RightAdcVolume,
    WCodecRegIndex_AdditionalControl_R18,
    WCodecRegIndex_3DControl,
    WCodecRegIndex_PowerManagement1,
    WCodecRegIndex_PowerManagement2,
    WCodecRegIndex_PowerManagement3,
    WCodecRegIndex_PowerManagement4,
    WCodecRegIndex_IdRegister,
    WCodecRegIndex_InterruptPolarity,
    WCodecRegIndex_InterruptEnable,
    WCodecRegIndex_GpioControl1,
    WCodecRegIndex_GpioControl2,
    WCodecRegIndex_R29,
    WCodecRegIndex_R30,
    WCodecRegIndex_Reset,
    WCodecRegIndex_RecordMix1,
    WCodecRegIndex_RecordMix2,
    WCodecRegIndex_LeftOutMix1,
    WCodecRegIndex_LeftOutMix2,
    WCodecRegIndex_RightOutMix1,
    WCodecRegIndex_RightOutMix2,
    WCodecRegIndex_MonoOutMix1,
    WCodecRegIndex_MonoOutMix2,
    WCodecRegIndex_LOut1Volume,
    WCodecRegIndex_ROut1Volume,
    WCodecRegIndex_LOut2Volume,
    WCodecRegIndex_ROut2Volume,
    WCodecRegIndex_MonoOutVolume,
    WCodecRegIndex_OutputControl,
    WCodecRegIndex_AdcInputMode,
    WCodecRegIndex_InputControl1,
    WCodecRegIndex_InputControl2,
    WCodecRegIndex_LeftInputVolume,
    WCodecRegIndex_RightInputVolume,
    WCodecRegIndex_MicBiasControl,
    WCodecRegIndex_ClockControl,
    WCodecRegIndex_Pll1Control1,
    WCodecRegIndex_Pll1Control2,
    WCodecRegIndex_Pll1Control3,
    WCodecRegIndex_Pll1Control4,
    WCodecRegIndex_Pll2Control1,
    WCodecRegIndex_Pll2Control2,
    WCodecRegIndex_Pll2Control3,
    WCodecRegIndex_Pll2Control4,
    WCodecRegIndex_BiasControl,
    WCodecRegIndex_R62,
    WCodecRegIndex_AdditionalControl_R63,
    WCodecRegIndex_Max,
    WCodecRegIndex_Force32             = 0x7FFFFFFF
} WCodecRegIndex;

typedef enum
{
    WCODECINTSTATUS_UNKNOWN           = 0,
    WCODECINTSTATUS_MIC_SHORTBIAS     = 0x1,
    WCODECINTSTATUS_MIC_DETECT        = 0x2,
    WCODECINTSTATUS_GPIO3             = 0x8,
    WCODECINTSTATUS_GPIO4             = 0x10,
    WCODECINTSTATUS_GPIO5             = 0x20,
    WCODECINTSTATUS_HEADPHONE_DETECT  = 0x40,
    WCODECINTSTATUS_THERMAL_SHUTDOWN  = 0x80
} WCodecIntStatus;
/*
 * The codec control variables.
 */
typedef struct
{
    NvU32 HPOut1VolLeft;
    NvU32 HPOut1VolRight;

    NvU32 HPOut2VolLeft;
    NvU32 HPOut2VolRight;

    NvU32 AdcSamplingRate;
    NvU32 DacSamplingRate;

    NvU32 LineInVolLeft;
    NvU32 LineInVolRight;

    NvU32 MicInVolLeft;
    NvU32 MicInVolRight;

} WolfsonCodecControls;

/*
* Struct to keep the volume ranges
* Keep the Db range and linear limits
*/
#define W8753_VOLUME_REGISTER_ENTRIES 3
typedef struct W8753AudioCodecVolumeRegisterRec
{
    NvS32 MinDbRange;
    NvS32 MaxDbRange;
    NvU32 MaxLinearRange;
    NvU32 MinLinearRange;
    NvU32 BaseRegValue;     // Equivalent to 0db
}W8753AudioCodecVolumeRegister;

static W8753AudioCodecVolumeRegister s_W8753VolRegisterInfo[W8753_VOLUME_REGISTER_ENTRIES] =
{
    // LineOut1
    // +6dB , -80dB
    { 600, -8000, 200, 1, 0x79 },

    // LineIn
    // +30dB, -97dB, 3100 for full range +30
    { 3000, -9700, 3100, 1, 0xC3 },

    // MicIn
    // +30dB, -18dB
    { 3000, -1800, 3100, 14, 0x17}
};

// Possible Output Mode in the codec
typedef enum
{
    OutputMode_HeadPhone = 0,
    OutputMode_EarSpkr,
    OutputMode_StereoSpkr,
    OutputMode_Force32 = 0x7FFFFFFF

} OutputMode;

/*
 * Codec structure.
 */
typedef struct W8753AudioCodecRec
{
    NvOdmIoModule IoModule;
    NvU32 Address;
    NvU32 InstanceId;
    NvOdmServicesSpiHandle hOdmSpiService;
    NvOdmServicesPmuHandle hOdmServicePmuDevice;
    NvOdmServicesI2cHandle hOdmServiceI2cDevice;
    NvOdmServicesGpioHandle hGpio;
    NvOdmGpioPinHandle hGpioPin;
    NvOdmServicesGpioIntrHandle hGpioIntr;
    NvOdmOsSemaphoreHandle hIntSema;
    NvOdmOsThreadHandle hEventThread;
    NvOdmOsSemaphoreHandle hEventSema;
    NvOdmOsSemaphoreHandle hSettlingSema;
    NvOdmOsSemaphoreHandle hDebounceSema;
    NvOdmOsThreadHandle hGpioThread;
    NvOdmOsThreadHandle hDebounceThread;
    WolfsonCodecControls WCodecControl;
    NvOdmQueryI2sACodecInterfaceProp WCodecInterface;
    NvU32  WCodecRegVals[WCodecRegIndex_Max];
    NvU32  MCLKFreq;
    NvS32  StereoSpkrUseCount;
    NvU32  VddId;
    volatile NvU32 CodecShutdown;
    volatile NvU32  MicPortIndex;
    NvU64  CodecGuid;
    NvBool IsMCLKEnabled;
    NvBool IsMicInUse;
    NvBool IsHiFiCodecInUse;
    NvBool IsVoiceCodecInUse;
    NvBool LastHeadPhoneMode;
    NvBool LastHeadSetMicMode;
    NvBool IsHeadSetMicEnabled;
    NvBool IsEarSpkrEnabled;
    NvBool IsBoardConcorde2;
    NvBool IsBoardWhistler;
    NvU32  IntStatus;
    NvU32  SignalPower;
    volatile NvBool IsHeadPhoneEnabled;
    volatile NvBool IsSpkrAmpEnabled;
    volatile NvU32  CodecInUseCount;
    OutputMode CodecOutputMode;

} W8753AudioCodec, *W8753AudioCodecHandle;

static W8753AudioCodec s_W8753;       /* Unique audio codec for the whole system*/

static const NvU32 s_W8753_ResetVal[WCodecRegIndex_Max] =
    {
            0x0,    // 0 Index
            0x008,  // R01 WCodecRegIndex_DacControl
            0x00,   // R02 WCodecRegIndex_AdcControl
            0x002,  // R03 WCodecRegIndex_PcmAudioInterface
            0x00A,  // R04 WCodecRegIndex_HiFiAudioInterface
            0x33,   // R05 WCodecRegIndex_InterfaceControl
            0x20,   // R06 WCodecRegIndex_SampleRateControl1
            0x7,    // R07 WCodecRegIndex_SampleRateControl2
            0x1FF,  // R08 WCodecRegIndex_LeftDacVolume
            0x1FF,  // R09 WCodecRegIndex_RightDacVolume
            0xF,    // R10 WCodecRegIndex_BaseControl
            0xF,    // R11 WCodecRegIndex_TrebleControl
            0x7B,   // R12 WCodecRegIndex_Alc1
            0x0,    // R13 WCodecRegIndex_Alc2
            0x32,   // R14 WCodecRegIndex_Alc3
            0x0,    // R15 WCodecRegIndex_NoiseGate
            0xC3,   // R16 WCodecRegIndex_LeftAdcVolume
            0xC3,   // R17 WCodecRegIndex_RightAdcVolume
            0xC0,   // R18 WCodecRegIndex_AdditionalControl_R18
            0x0,    // R19 WCodecRegIndex_3DControl
            0x0,    // R20 WCodecRegIndex_PowerManagement1
            0x0,    // R21 WCodecRegIndex_PowerManagement2
            0x0,    // R22 WCodecRegIndex_PowerManagement3
            0x0,    // R23 WCodecRegIndex_PowerManagement4
            0x0,    // R24 WCodecRegIndex_IdRegister
            0x0,    // R25 WCodecRegIndex_InterruptPolarity
            0x0,    // R26 WCodecRegIndex_InterruptEnable
            0x0,    // R27 WCodecRegIndex_GpioControl1
            0x0,    // R28 WCodecRegIndex_GpioControl2
            0x0,    // R29 WCodecRegIndex_R29
            0x0,    // R30 WCodecRegIndex_R30
            0x0,    // R31 WCodecRegIndex_Reset
            0x55,   // R32 WCodecRegIndex_RecordMix1
            0x5,    // R33 WCodecRegIndex_RecordMix2
            0x50,   // R34 WCodecRegIndex_LeftOutMix1
            0x55,   // R35 WCodecRegIndex_LeftOutMix2
            0x50,   // R36 WCodecRegIndex_RightOutMix1
            0x55,   // R37 WCodecRegIndex_RightOutMix2
            0x50,   // R38 WCodecRegIndex_MonoOutMix1
            0x55,   // R39 WCodecRegIndex_MonoOutMix2
            0x1F9,  // R40 WCodecRegIndex_LOut1Volume
            0x1F9,  // R41 WCodecRegIndex_ROut1Volume
            0x1F9,  // R42 WCodecRegIndex_LOut2Volume
            0x1F9,  // R43 WCodecRegIndex_ROut2Volume
            0x79,   // R44 WCodecRegIndex_MonoOutVolume
            0x0,    // R45 WCodecRegIndex_OutputControl
            0x0,    // R46 WCodecRegIndex_AdcInputMode
            0x0,    // R47 WCodecRegIndex_InputControl1
            0x0,    // R48 WCodecRegIndex_InputControl2
            0x97,   // R49 WCodecRegIndex_LeftInputVolume
            0x97,   // R50 WCodecRegIndex_RightInputVolume
            0x0,    // R51 WCodecRegIndex_MicBiasControl
            0x4,    // R52 WCodecRegIndex_ClockControl
            0x0,    // R53 WCodecRegIndex_Pll1Control1
            0x83,   // R54 WCodecRegIndex_Pll1Control2
            0x24,   // R55 WCodecRegIndex_Pll1Control3
            0x1BA,  // R56 WCodecRegIndex_Pll1Control4
            0x0,    // R57 WCodecRegIndex_Pll2Control1
            0x83,   // R58 WCodecRegIndex_Pll2Control2
            0x24,   // R59 WCodecRegIndex_Pll2Control3
            0x1BA,  // R60 WCodecRegIndex_Pll2Control4
            0x0,    // R61 WCodecRegIndex_BiasControl
            0x0,    // R62 WCodecRegIndex_R62
            0x0     // R63 WCodecRegIndex_AdditionalControl
    };

typedef enum
{
    PowerMode_Off = 0,
    PowerMode_On,
    PowerMode_Standby,
    PowerMode_Fast,
    PowerMode_Force32 = 0x7FFFFFFF
}PowerMode;

typedef struct PLL1FreqParamsRec
{
    NvU32 MCLK;      // in KHz
    NvU32 OutFreq;   // in KHZ
    NvU32 nValue;    // in Hex
    NvU32 kValue;    // in Hex
}PLL1FreqParams;

#define PLLFREQTABLE_ENTRYCOUNT 4
// Currentily we are only keeping 4 entries
PLL1FreqParams PLLFreqTable[PLLFREQTABLE_ENTRYCOUNT] =
{
    { 12000 , 11289 , 0x7, 0x21B08A },
    { 12000 , 12288 , 0x8, 0x0C49BA },
    { 13000 , 11289 , 0x6, 0x3CA2F5 },
    { 13000 , 12288 , 0x7, 0x23F54A },
};

// Status Register register Index
typedef enum
{
    STATUS_REG_DEVICE_HI = 1,
    STATUS_REG_DEVICE_LO,
    STATUS_REG_DEVICE_REV,
    STATUS_REG_DEVICE_CAP,
    STATUS_REG_DEVICE_STATUS,
    STATUS_REG_INT_STATUS,
    STATUS_REG_Force32 = 0x7FFFFFFF
}StatusRegAddress;

/*------------------------------------*/

#define NVODM_CODEC_W8753_MAX_CLOCKS 3


// Prototype definition
static void
ACW8753SetVolume(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortName PortName,
    NvOdmAudioVolume *pVolume,
    NvU32 ListCount);

static NvBool ConfigExternalClock(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsEnable)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 ClockInstances[NVODM_CODEC_W8753_MAX_CLOCKS];
    NvU32 ClockFrequencies[NVODM_CODEC_W8753_MAX_CLOCKS];
    NvU32 NumClocks = 0;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    IsSuccess = NvOdmExternalClockConfig(
                hAudioCodec->CodecGuid, !IsEnable,
                ClockInstances, ClockFrequencies, &NumClocks);

    if (IsSuccess)
    {
        // CDEV1 -> MCLK for codec
        if (NumClocks >= 1)
        {
            hAudioCodec->MCLKFreq = ClockFrequencies[0];
        }
    }

    return IsSuccess;
}

static void SetPowerOffCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    NvOdmServicesPmuVddRailCapabilities RailCaps;
    NvU32 SettlingTime;

    if (!hAudioCodec) return;

    if (hAudioCodec->hGpio)
    {
        if (hAudioCodec->hGpioPin)
        {
            if (hAudioCodec->hGpioIntr)
            {
                NvOdmGpioInterruptUnregister(hAudioCodec->hGpio, hAudioCodec->hGpioPin, hAudioCodec->hGpioIntr);
                hAudioCodec->hGpioIntr = 0;
            }
            NvOdmGpioReleasePinHandle(hAudioCodec->hGpio, hAudioCodec->hGpioPin);
        }

        NvOdmGpioClose(hAudioCodec->hGpio);
        hAudioCodec->hGpio = 0;
        hAudioCodec->hGpioPin = 0;
    }

    if (hAudioCodec->hOdmServicePmuDevice != NULL)
    {
        // Search for the Vdd rail and power Off the module
        if (hAudioCodec->VddId)
        {
            NvOdmServicesPmuGetCapabilities(hAudioCodec->hOdmServicePmuDevice,hAudioCodec->VddId, &RailCaps);
            NvOdmServicesPmuSetVoltage(hAudioCodec->hOdmServicePmuDevice,
                        hAudioCodec->VddId, NVODM_VOLTAGE_OFF, &SettlingTime);
            if (SettlingTime)
                NvOdmOsWaitUS(SettlingTime);
            hAudioCodec->VddId  = 0;
        }
        NvOdmServicesPmuClose(hAudioCodec->hOdmServicePmuDevice);
        hAudioCodec->hOdmServicePmuDevice = NULL;

        ConfigExternalClock(hOdmAudioCodec, NV_FALSE);
        hAudioCodec->IsMCLKEnabled = NV_FALSE;
    }
}

static NvBool SetPowerOnCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvU32 InstanceID)
{
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    const NvOdmPeripheralConnectivity *pPerConnectivity = NULL;
    NvU32 Index;
    NvOdmServicesPmuVddRailCapabilities RailCaps;
    NvU32 SettlingTime;
    NvU32 GpioPort   = 0;
    NvU32 GpioPin    = 0;
    NvU32 NumOfGuids = 1;

    NvU32 searchVals[2];
    const NvOdmPeripheralSearch searchAttrs[] =
    {
        NvOdmPeripheralSearch_IoModule,
        NvOdmPeripheralSearch_Instance,
    };

    searchVals[0] =  NvOdmIoModule_Dap;
    searchVals[1] =  InstanceID;

    NumOfGuids = NvOdmPeripheralEnumerate(  searchAttrs,
                                            searchVals,
                                            2,
                                            &hAudioCodec->CodecGuid,
                                            NumOfGuids);

    // Get the peripheral connectivity information
    pPerConnectivity = NvOdmPeripheralGetGuid(hAudioCodec->CodecGuid);
    if (pPerConnectivity == NULL)
        return NV_FALSE;

    if (hAudioCodec->hOdmServicePmuDevice == NULL)
    {
        hAudioCodec->hOdmServicePmuDevice = NvOdmServicesPmuOpen();
        if (!hAudioCodec->hOdmServicePmuDevice)
            return NV_FALSE;
    }

    // Search for the Vdd rail and set the proper volage to the rail.
    for (Index = 0; Index < pPerConnectivity->NumAddress; ++Index)
    {
        if (pPerConnectivity->AddressList[Index].Interface == NvOdmIoModule_Vdd)
        {
            hAudioCodec->VddId = pPerConnectivity->AddressList[Index].Address;
            NvOdmServicesPmuGetCapabilities(hAudioCodec->hOdmServicePmuDevice,hAudioCodec->VddId, &RailCaps);
            NvOdmServicesPmuSetVoltage(hAudioCodec->hOdmServicePmuDevice,
                        hAudioCodec->VddId,RailCaps.requestMilliVolts, &SettlingTime);
            if (SettlingTime)
                NvOdmOsWaitUS(SettlingTime);
        }
        else if (pPerConnectivity->AddressList[Index].Interface == NvOdmIoModule_Gpio)
        {
            GpioPort = pPerConnectivity->AddressList[Index].Instance;
            GpioPin  = pPerConnectivity->AddressList[Index].Address;
        }
    }

    if (!ConfigExternalClock(hOdmAudioCodec, NV_TRUE))
        return NV_FALSE;

    hAudioCodec->IsMCLKEnabled = NV_TRUE;

    // for Gpioport and pin
    if (GpioPort)
    {
        hAudioCodec->hGpio = NvOdmGpioOpen();

        if (!hAudioCodec->hGpio)
        {
            goto failcodecPower;
        }

        hAudioCodec->hGpioPin = NvOdmGpioAcquirePinHandle(hAudioCodec->hGpio, GpioPort, GpioPin);
        if (!hAudioCodec->hGpioPin)
        {
            goto failcodecPower;
        }

        NvOdmGpioConfig(hAudioCodec->hGpio,
                        hAudioCodec->hGpioPin,
                        NvOdmGpioPinMode_InputData);
    }

    return NV_TRUE;

failcodecPower:
    SetPowerOffCodec(hOdmAudioCodec);
    return NV_FALSE;
}

static NvBool OpenOdmServiceHandle(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    const NvOdmPeripheralConnectivity *pPerConnectivity = NULL;
    NvU32 Index;

    // Get the peripheral connectivity information
    pPerConnectivity = NvOdmPeripheralGetGuid(hAudioCodec->CodecGuid);
    if (pPerConnectivity == NULL)
        return NV_FALSE;

    // Search for the Io interfacing module
    for (Index = 0; Index < pPerConnectivity->NumAddress; ++Index)
    {
        if ((pPerConnectivity->AddressList[Index].Interface == NvOdmIoModule_Spi) ||
            (pPerConnectivity->AddressList[Index].Interface == NvOdmIoModule_I2c) ||
            (pPerConnectivity->AddressList[Index].Interface == NvOdmIoModule_I2c_Pmu))
        {
            break;
        }
    }

    // If IO module is not found then return fail.
    if (Index == pPerConnectivity->NumAddress)
        return NV_FALSE;

    hAudioCodec->IoModule   = pPerConnectivity->AddressList[Index].Interface;
    hAudioCodec->Address    = pPerConnectivity->AddressList[Index].Address;
    hAudioCodec->InstanceId = pPerConnectivity->AddressList[Index].Instance;

    // Supporting only spi & i2c interfaces
    if ((hAudioCodec->IoModule == NvOdmIoModule_I2c)||
        (hAudioCodec->IoModule == NvOdmIoModule_I2c_Pmu))
    {
        hAudioCodec->hOdmServiceI2cDevice =
            NvOdmI2cOpen(hAudioCodec->IoModule,
                           hAudioCodec->InstanceId);

        if (!hAudioCodec->hOdmServiceI2cDevice)
            return NV_FALSE;
    }
    else if (hAudioCodec->IoModule == NvOdmIoModule_Spi)
    {

        NVODM_WM8753_LOG((WM8753_PREFIX "[OpenOdmServiceHandle] IsWhistler %d \n", hAudioCodec->IsBoardWhistler));

        hAudioCodec->hOdmSpiService =
                NvOdmSpiOpen(hAudioCodec->IoModule,
                             hAudioCodec->InstanceId);
        if (!hAudioCodec->hOdmSpiService)
            return NV_FALSE;
    }
    return NV_TRUE;
}

static void CloseOdmServiceHandle(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    if (!hAudioCodec)
        return;

    // Closing the SPI ODM Service
    if (hAudioCodec->hOdmSpiService)
        NvOdmSpiClose(hAudioCodec->hOdmSpiService);
     hAudioCodec->hOdmSpiService = NULL;

     if (hAudioCodec->hOdmServiceI2cDevice)
        NvOdmI2cClose(hAudioCodec->hOdmServiceI2cDevice);
     hAudioCodec->hOdmServiceI2cDevice = NULL;
}

static NvBool
W8753_WriteRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    WCodecRegIndex RegIndex,
    NvU32 Data)
{
    NvU32 DataToSend;
    NvU8 pTxBuffer[5] = {0};
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    NvOdmI2cTransactionInfo TransactionInfo;
    NvOdmI2cStatus I2cTransStatus;

    if ((hAudioCodec->IoModule == NvOdmIoModule_I2c)||
        (hAudioCodec->IoModule == NvOdmIoModule_I2c_Pmu))
    {
        DataToSend = (RegIndex << 9) | (Data & 0x1FF);
        pTxBuffer[0] = (NvU8)((DataToSend >> 8) & 0xFF);
        pTxBuffer[1] = (NvU8)((DataToSend) & 0xFF);

        TransactionInfo.Address  = hAudioCodec->Address;
        TransactionInfo.Buf      = pTxBuffer;
        TransactionInfo.Flags    = NVODM_I2C_IS_WRITE;
        TransactionInfo.NumBytes = 2;

        // write the Accelator offset (from where data is to be read)
        I2cTransStatus = NvOdmI2cTransaction(hAudioCodec->hOdmServiceI2cDevice, &TransactionInfo, 1, W8753_I2C_SCLK,
                        W8753_I2C_TIMEOUT);

        // HW- BUG!! If timeout, again retransmit the data.
        if (I2cTransStatus == NvOdmI2cStatus_Timeout)
            I2cTransStatus = NvOdmI2cTransaction(hAudioCodec->hOdmServiceI2cDevice, &TransactionInfo, 1, W8753_I2C_SCLK,
                            W8753_I2C_TIMEOUT);

        if (I2cTransStatus != NvOdmI2cStatus_Success)
        {
            return NV_FALSE;
        }
    }
    else if (hAudioCodec->IoModule == NvOdmIoModule_Spi)
    {
        DataToSend = (RegIndex << 9) | (Data & 0x1FF);
        pTxBuffer[0] = (NvU8)((DataToSend >> 8) & 0xFF);
        pTxBuffer[1] = (NvU8)((DataToSend) & 0xFF);

        NvOdmSpiTransaction(hAudioCodec->hOdmSpiService, hAudioCodec->Address,
            CODEC_INF_FREQ, NULL,  pTxBuffer, 2, 16);
    }

#if NVODM_WM8753_LOG_HW_ACCESS
    NVODM_WM8753_LOG((WM8753_PREFIX "[WriteRegister] RegIndex:0x%x Data:%08x and combined data 0x%08x\n",
                      RegIndex, Data, DataToSend));
#endif

    hAudioCodec->WCodecRegVals[RegIndex] = Data;
    return NV_TRUE;
}


static void
W8753_ReadRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU8 *pReadBuffer)
{
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    NvOdmI2cTransactionInfo TransactionInfo;
    NvOdmI2cStatus I2cTransStatus;

    if ((hAudioCodec->IoModule == NvOdmIoModule_I2c)||
        (hAudioCodec->IoModule == NvOdmIoModule_I2c_Pmu))
    {
        TransactionInfo.Address  = hAudioCodec->WCodecInterface.DeviceAddress;
        TransactionInfo.Buf      = pReadBuffer;
        TransactionInfo.Flags    = NVODM_I2C_IS_READ;
        TransactionInfo.NumBytes = 2;

        // write the Accelator offset (from where data is to be read)
        I2cTransStatus = NvOdmI2cTransaction(hAudioCodec->hOdmServiceI2cDevice, &TransactionInfo, 1, W8753_I2C_SCLK,
                        W8753_I2C_TIMEOUT);

        // HW- BUG!! If timeout, again retransmit the data.
        if (I2cTransStatus == NvOdmI2cStatus_Timeout)
            I2cTransStatus = NvOdmI2cTransaction(hAudioCodec->hOdmServiceI2cDevice, &TransactionInfo, 1, W8753_I2C_SCLK,
                            W8753_I2C_TIMEOUT);

        if (I2cTransStatus != NvOdmI2cStatus_Success)
        {
            *pReadBuffer = 0;
        }
    }
    else if (hAudioCodec->IoModule == NvOdmIoModule_Spi)
    {
        NvOdmSpiTransaction(hAudioCodec->hOdmSpiService, hAudioCodec->Address, CODEC_INF_FREQ,
                    pReadBuffer, NULL, 2, 16);
    }
}

static void SetReadEnable(NvOdmPrivAudioCodecHandle hOdmAudioCodec,
                          StatusRegAddress StatusIndex,
                          NvU8 *pRxBuffer)
{
    NvU32 GpioCont2Reg;
    NvU32 ReadContReg;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    GpioCont2Reg = hAudioCodec->WCodecRegVals[WCodecRegIndex_GpioControl2];
    ReadContReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_IdRegister];

    // Device Id is 8753 High = 0x87 and Low is 0x53. We are trying to read the DevId
    // Low.
    GpioCont2Reg = W8753_SET_REG_DEF(GPIO_CONTROL2, GPIO1M, SDOUT, GpioCont2Reg);
    ReadContReg = W8753_SET_REG_DEF(READ_CONTROL, READEN, ENABLE, ReadContReg);

    switch (StatusIndex)
    {
        case STATUS_REG_DEVICE_HI:
            break;
        case STATUS_REG_DEVICE_LO:
            ReadContReg = W8753_SET_REG_DEF(READ_CONTROL, READSEL, DEVID_LO, ReadContReg);
            break;
        case STATUS_REG_DEVICE_REV:
        case STATUS_REG_DEVICE_CAP:
            break;
        case STATUS_REG_DEVICE_STATUS:
            ReadContReg = W8753_SET_REG_DEF(READ_CONTROL, READSEL, DEV_STATUS, ReadContReg);
            break;
        case STATUS_REG_INT_STATUS:
            ReadContReg = W8753_SET_REG_DEF(READ_CONTROL, READSEL, INT_STATUS, ReadContReg);
            break;
        default:
            break;
    }

    W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_GpioControl2, GpioCont2Reg);
    W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_IdRegister, ReadContReg);

    W8753_ReadRegister(hOdmAudioCodec, pRxBuffer);  // pRxBuffer[0] should be 0x53.
    //NvOdmOsDebugPrintf("Device Read Id %02x %02x \n", pRxBuffer[0], pRxBuffer[1]);

}

static NvU32 RegisterReadStatus(NvOdmPrivAudioCodecHandle hNewOdmACodec, StatusRegAddress StatusIndex)
{
    NvU8 pRxBuffer[5] ={0};

    SetReadEnable(hNewOdmACodec, StatusIndex, pRxBuffer);

    //NvOdmOsDebugPrintf("Register Value 0x%02x \n", pRxBuffer[0]);

    return pRxBuffer[0];
}

static NvS32 ObtainVolumeIndB(NvS32 Volume, NvBool IsLinear, NvU32 TableIndex)
{
    NvS32 dB;
    NvS32 MaxDbLimit     = s_W8753VolRegisterInfo[TableIndex].MaxDbRange;
    NvS32 MinDbLimit     = s_W8753VolRegisterInfo[TableIndex].MinDbRange;
    NvS32 MaxLinearLimit = (NvS32)s_W8753VolRegisterInfo[TableIndex].MaxLinearRange;
    NvS32 MinLinearLimit = (NvS32)s_W8753VolRegisterInfo[TableIndex].MinLinearRange;

    #define limit(x,max,min)  ((x > max) ? max : ((x < min) ? min : x))

    dB = IsLinear ? NV_SFX_TO_WHOLE(NV_SFX_MUL_BY_WHOLE(
        NvSFxDiv(NV_SFX_LOG(NvSFxDiv(NV_SFX_WHOLE_TO_FX(limit(Volume, MaxLinearLimit, MinLinearLimit)), NV_SFX_WHOLE_TO_FX(100))),
            NV_SFX_LOG(NV_SFX_WHOLE_TO_FX(10))),
        20)) : limit(Volume, MaxDbLimit, MinDbLimit) / 100;

    return dB;
}

static NvBool
SetMicrophoneInVolume(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 MicId,
    NvU32 MicGain)
{
    NvU32 InputControl1;
    NvU32 Gain;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    InputControl1 = hAudioCodec->WCodecRegVals[WCodecRegIndex_InputControl1];
    // Mic Gain ares 00, 01,10,11 -> 12,18,24,30
    // 6 dB fro 25% gain
    Gain = MicGain/25;
    if (MicId == 0)
        InputControl1 = W8753_SET_REG_VAL(INPUT_CONTROL1, MIC1BOOST, Gain, InputControl1);
    else if (MicId == 1)
        InputControl1 = W8753_SET_REG_VAL(INPUT_CONTROL1, MIC2BOOST, Gain, InputControl1);
    else
        return NV_FALSE;
    return W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_InputControl1, InputControl1);
}

static NvBool SetMicrophoneInMute(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsMute)
{
    // Not found the information on data sheet.
    // As there is no mute bit for MicIn volume - shutting Preamp value
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    NvU32 PowerManagement2  = hAudioCodec->WCodecRegVals[WCodecRegIndex_PowerManagement2];
    if (!IsMute)
    {
        if (hAudioCodec->MicPortIndex == 0)
        {
            PowerManagement2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, MICAMP1EN, ON, PowerManagement2);
        }
        else if (hAudioCodec->MicPortIndex == 1)
        {
            PowerManagement2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, MICAMP2EN, ON, PowerManagement2);
        }
    }
    else
    {
        if (hAudioCodec->MicPortIndex == 0)
        {
            PowerManagement2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, MICAMP1EN, OFF, PowerManagement2);
        }
        else if (hAudioCodec->MicPortIndex == 1)
        {
            PowerManagement2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, MICAMP2EN, OFF, PowerManagement2);
        }
    }

    W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement2, PowerManagement2);
    return NV_TRUE;
}

static NvBool
SetHeadphoneOutVolume(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 ChannelMask,
    NvU32 ChannelId,
    NvU32 IsLinear,
    NvS32 Volume)
{
    NvU32 Vol;
    NvU32 VolReg;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    Vol = Volume;
    if (Vol)
    {
        Vol = s_W8753VolRegisterInfo[0].BaseRegValue + ObtainVolumeIndB(Volume, IsLinear, 0);
    }

    if (ChannelId == 0)
    {
        if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
        {
            VolReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_LOut1Volume];
            VolReg = W8753_SET_REG_VAL(LOUT1_VOL, LOUT1VOL, Vol, VolReg);
            W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LOut1Volume, VolReg);
            hAudioCodec->WCodecControl.HPOut1VolLeft = VolReg;
        }

        if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
        {
            VolReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_ROut1Volume];
            VolReg = W8753_SET_REG_VAL(ROUT1_VOL, ROUT1VOL, Vol, VolReg);
            W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ROut1Volume, VolReg);
            hAudioCodec->WCodecControl.HPOut1VolRight = VolReg;
        }
    }

    if ((ChannelId == 1) || hAudioCodec->IsEarSpkrEnabled)
    {
        if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
        {
            VolReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_LOut2Volume];
            VolReg = W8753_SET_REG_VAL(LOUT2_VOL, LOUT2VOL, Vol, VolReg);
            W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LOut2Volume, VolReg);
            hAudioCodec->WCodecControl.HPOut2VolLeft = VolReg;
        }

        if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
        {
            VolReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_ROut2Volume];
            VolReg = W8753_SET_REG_VAL(ROUT2_VOL, ROUT2VOL, Vol, VolReg);
            W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ROut2Volume, VolReg);
            hAudioCodec->WCodecControl.HPOut2VolRight = VolReg;
        }
    }

    return NV_TRUE;
}

static NvBool
SetPGAInputVolume(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 ChannelMask,
    NvU32 ChannelId,
    NvU32 IsLinear,
    NvS32 Volume)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32  InputVol;
    NvS32  VolReg = 0x1F, dB = 0;

    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    // Until we have a better logic to use the ObtainVolumeIndB - use the following
    // Maximum is set only upto 12db
    // Volume is getting in the range 0 - 0xFFFF
    if (!IsLinear)
    {
        VolReg = ObtainVolumeIndB(Volume, IsLinear, 2);
    }
    else
    {
        Volume = Volume >> 8;
        if (Volume >= 128)
        {
            // 6dB is set as 1/2 of linear.
            // 8 Steps from 6dB to 12dB
            VolReg = 0x1F;
            dB = ((Volume * 10) - 1280) / 158;
            VolReg +=dB;
            // Set Mic Boost as +24db
            SetMicrophoneInVolume(hOdmAudioCodec, hAudioCodec->MicPortIndex, 70);
        }
        else if ( Volume >= 64)
        {
            // 0db is set a 1/4 of linear
            // 8 Steps from 0dB to 6dB
            VolReg = 0x1F;
            dB = (1280 - (Volume * 10)) / 80;
            VolReg -=dB;
            // Set Mic Boost as +18db
            SetMicrophoneInVolume(hOdmAudioCodec, hAudioCodec->MicPortIndex, 40);
        }
        else
        {
            // 23 Steps from -17.5dB to 0dB
            VolReg = 0x17;
            dB = (640 - (Volume * 10)) / 27;
            VolReg -=dB;
            // Set Mic Boost as +12db
            SetMicrophoneInVolume(hOdmAudioCodec, hAudioCodec->MicPortIndex, 0);
        }
    }

    if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
    {
        InputVol    = hAudioCodec->WCodecRegVals[WCodecRegIndex_LeftInputVolume];
        // Enable LIVU in LInVol
        InputVol = W8753_SET_REG_DEF(LEFT_CHANNEL_PGA_VOL, LIVU, BOTH, InputVol);
        InputVol = W8753_SET_REG_VAL(LEFT_CHANNEL_PGA_VOL, LINVOL, VolReg , InputVol);
        if (Volume)
        {
            InputVol = W8753_SET_REG_DEF(LEFT_CHANNEL_PGA_VOL, LINMUTE, DISABLE, InputVol);
        }
        else
        {
            InputVol = W8753_SET_REG_DEF(LEFT_CHANNEL_PGA_VOL, LINMUTE, ENABLE, InputVol);
        }

        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LeftInputVolume, InputVol);
        hAudioCodec->WCodecControl.MicInVolLeft = InputVol;
    }

    if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
    {
        InputVol   = hAudioCodec->WCodecRegVals[WCodecRegIndex_RightInputVolume];
        // Enable RIVU/RZCEN in RInVol
        InputVol = W8753_SET_REG_DEF(RIGHT_CHANNEL_PGA_VOL, RIVU, BOTH, InputVol);
        InputVol = W8753_SET_REG_VAL(RIGHT_CHANNEL_PGA_VOL, RINVOL, VolReg, InputVol);
        if (Volume)
        {
            InputVol = W8753_SET_REG_DEF(RIGHT_CHANNEL_PGA_VOL, RINMUTE, DISABLE, InputVol);
        }
        else
        {
            InputVol = W8753_SET_REG_DEF(RIGHT_CHANNEL_PGA_VOL, RINMUTE, ENABLE, InputVol);
        }

        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RightInputVolume, InputVol);
        hAudioCodec->WCodecControl.MicInVolRight = InputVol;
    }

    NVODM_WM8753_LOG((WM8753_PREFIX "[SetPGAInputVolume] PGA VOLUME %d RegValue 0x%x \n", Volume, VolReg));
    return IsSuccess;
}

static NvBool
SetLineInVolume(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 ChannelMask,
    NvU32 ChannelId,
    NvU32 IsLinear,
    NvS32 Volume)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 Vol;
    NvS32 VolReg = 0xCF, dB = 0;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    // In W8753, the Line In volume gain is not avialble. It will be possible
    // only through the adc gain.
    Vol = Volume;
    if (Vol)
    {
        if (!IsLinear)
        {
            VolReg = ObtainVolumeIndB(Volume, IsLinear, 1);
        }
        else
        {

            // Until we have a better logic to use the ObtainVolumeIndB - use the following
            // Maximum is set only upto 12db
            // Volume is getting in the range 0 - 0xFFFF
            Volume = Volume >> 8;
            if (Volume >= 128)
            {
                // 6dB is set as 1/2 of linear.
                // 8 Steps from 6dB to 12dB
                VolReg = 0xCF;
                dB = ((Volume * 10) - 1280) / 158;
                VolReg +=dB;
            }
            else if ( Volume >= 64)
            {
                // 0db is set a 1/4 of linear
                // 8 Steps from 0dB to 6dB
                VolReg = 0xCF;
                dB = (1280 - (Volume * 10)) / 80;
                VolReg -=dB;
            }
            else
            {
                // 97 Steps from -97dB to 0dB
                VolReg = 0xC3;
                dB = ((640 - (Volume * 10)) * 6) / 20;
                VolReg -=dB;
            }
        }
    }

    if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
    {
        VolReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_LeftAdcVolume];
        VolReg = W8753_SET_REG_VAL(LEFT_ADC_DIG_VOL, LADCVOL, Vol, VolReg);
        VolReg = W8753_SET_REG_DEF(LEFT_ADC_DIG_VOL, LAVU, BOTH, VolReg);
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LeftAdcVolume, VolReg);
        if (IsSuccess)
            hAudioCodec->WCodecControl.LineInVolLeft = VolReg;
    }
    if (!IsSuccess)
        return IsSuccess;

    if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
    {
        VolReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_RightAdcVolume];
        VolReg = W8753_SET_REG_VAL(RIGHT_ADC_DIG_VOL, RADCVOL, Vol, VolReg);
        VolReg = W8753_SET_REG_DEF(RIGHT_ADC_DIG_VOL, RAVU, BOTH, VolReg);
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RightAdcVolume, VolReg);
        if (IsSuccess)
            hAudioCodec->WCodecControl.LineInVolRight = VolReg;
    }

    NVODM_WM8753_LOG((WM8753_PREFIX "[SetLineInVolume] LineIn VOLUME %d RegValue 0x%x \n", Volume, VolReg));
    return IsSuccess;
}

static NvBool
SetHeadphoneOutMute(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 ChannelMask,
    NvU32 ChannelId,
    NvBool IsMute)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 LeftVolReg;
    NvU32 RightVolReg;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    if (IsMute)
    {
        LeftVolReg =  0x180;
        RightVolReg = 0x180;
    }
    else
    {
        if (ChannelId == 0)
        {
            LeftVolReg =  hAudioCodec->WCodecControl.HPOut1VolLeft;
            RightVolReg =  hAudioCodec->WCodecControl.HPOut1VolRight;
        }
        else
        {
            LeftVolReg =  hAudioCodec->WCodecControl.HPOut2VolLeft;
            RightVolReg =  hAudioCodec->WCodecControl.HPOut2VolRight;
        }
    }

    if (ChannelId == 0)
    {
        if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
        {
            W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LOut1Volume, LeftVolReg);
        }
        if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
        {
            W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ROut1Volume, RightVolReg);
        }
    }

    if ((ChannelId == 1) || hAudioCodec->IsEarSpkrEnabled)
    {
        if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
        {
            W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LOut2Volume, LeftVolReg);
        }
        if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
        {
            W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ROut2Volume, RightVolReg);
        }
    }
    return IsSuccess;
}


static NvBool
SetLineInMute(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 ChannelMask,
    NvBool IsMute)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 LeftVolReg;
    NvU32 RightVolReg;
    NvU32 LeftVol;
    NvU32 RightVol;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    LeftVol =  (IsMute)? 0: ((hAudioCodec->WCodecControl.LineInVolLeft*256)/100);
    RightVol = (IsMute)? 0: ((hAudioCodec->WCodecControl.LineInVolRight*256)/100);

    if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
    {
        LeftVolReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_LeftAdcVolume];
        LeftVolReg = W8753_SET_REG_VAL(LEFT_ADC_DIG_VOL, LADCVOL, LeftVol, LeftVolReg);
        LeftVolReg = W8753_SET_REG_DEF(LEFT_ADC_DIG_VOL, LAVU, BOTH, LeftVolReg);
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LeftAdcVolume, LeftVolReg);
    }
    if (!IsSuccess)
        return IsSuccess;

    if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
    {
        RightVolReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_RightAdcVolume];
        RightVolReg = W8753_SET_REG_VAL(RIGHT_ADC_DIG_VOL, RADCVOL, RightVol, RightVolReg);
        RightVolReg = W8753_SET_REG_DEF(RIGHT_ADC_DIG_VOL, RAVU, BOTH, RightVolReg);
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RightAdcVolume, RightVolReg);
    }
    return IsSuccess;
}

static NvBool ResetCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    return W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_Reset, W8753_RESET_0_RESET_VAL);
}

/*
 * Shuts down the audio codec driver.
 */
static NvBool ShutDownCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;

    // Shutoff all the modules
    IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement1, 0);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement2, 0);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement3, 0);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement4, 0);

    return IsSuccess;
}


static NvBool SetCodecPower(NvOdmPrivAudioCodecHandle hOdmAudioCodec, PowerMode PowerType)
{
    NvBool IsSuccess ;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    NvU32 PowerMngmnt1 = hAudioCodec->WCodecRegVals[WCodecRegIndex_PowerManagement1];
    switch (PowerType)
    {
        case PowerMode_On:
            PowerMngmnt1 = W8753_SET_REG_DEF(POWER_MANAGEMENT1, VMIDSEL, DIV_50K, PowerMngmnt1);
            IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement1, PowerMngmnt1);
            PowerMngmnt1 = W8753_SET_REG_DEF(POWER_MANAGEMENT1, VREF, ON, PowerMngmnt1);
            break;
        case PowerMode_Fast:
            PowerMngmnt1 = W8753_SET_REG_DEF(POWER_MANAGEMENT1, VMIDSEL, DIV_5K, PowerMngmnt1);
            IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement1, PowerMngmnt1);
            PowerMngmnt1 = W8753_SET_REG_DEF(POWER_MANAGEMENT1, VREF, ON, PowerMngmnt1);
            break;
        case PowerMode_Standby:
            PowerMngmnt1 = W8753_SET_REG_DEF(POWER_MANAGEMENT1, VMIDSEL, DIV_500K, PowerMngmnt1);
            IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement1, PowerMngmnt1);
            PowerMngmnt1 = W8753_SET_REG_DEF(POWER_MANAGEMENT1, VREF, ON, PowerMngmnt1);
            break;
        case PowerMode_Off:
        default:
            PowerMngmnt1 = W8753_SET_REG_DEF(POWER_MANAGEMENT1, VREF, OFF, PowerMngmnt1);
            IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement1, PowerMngmnt1);
            PowerMngmnt1 = W8753_SET_REG_DEF(POWER_MANAGEMENT1, VMIDSEL, DISABLE, PowerMngmnt1);
    }

    if (IsSuccess)
    {
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement1, PowerMngmnt1);
        //  Allow VREF to settle
        NvOdmOsWaitUS(WAIT_TIME_ON_VREF_TOSETTLE);
    }
    return IsSuccess;
}

static NvBool InitializeDigitalInterface(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    NvU32 DigHiFiIntReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_HiFiAudioInterface];
    NvU32 SampleContReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_SampleRateControl1];
    NvU32 SampleCont2Reg = hAudioCodec->WCodecRegVals[WCodecRegIndex_SampleRateControl2];

    // Initializing the configuration parameters.
    if (hAudioCodec->WCodecInterface.IsUsbMode)
        SampleContReg = W8753_SET_REG_DEF(SAMPLING_CONTROL1, USB_NORM_MODE, USB, SampleContReg);
    else
        SampleContReg = W8753_SET_REG_DEF(SAMPLING_CONTROL1, USB_NORM_MODE, NORM, SampleContReg);

    if (hAudioCodec->WCodecInterface.IsCodecMasterMode)
    {
        DigHiFiIntReg = W8753_SET_REG_DEF(DIGITAL_HIFIINTERFACE, MS, MASTER_MODE, DigHiFiIntReg);
        SampleCont2Reg = W8753_SET_REG_DEF(SAMPLING_CONTROL2, BMODE, DIV4, SampleCont2Reg);
        SampleCont2Reg = W8753_SET_REG_DEF(SAMPLING_CONTROL2, PBMODE, DIV4, SampleCont2Reg);
    }
    else
        DigHiFiIntReg = W8753_SET_REG_DEF(DIGITAL_HIFIINTERFACE, MS, SLAVE_MODE, DigHiFiIntReg);

    NVODM_WM8753_LOG((WM8753_PREFIX "[InitializeDigitalInterface] Codec Mode IsMaster %d \n",
                      hAudioCodec->WCodecInterface.IsCodecMasterMode));

    switch (hAudioCodec->WCodecInterface.I2sCodecDataCommFormat)
    {
        case NvOdmQueryI2sDataCommFormat_Dsp:
            DigHiFiIntReg = W8753_SET_REG_DEF(DIGITAL_HIFIINTERFACE, FORMAT, DSP_MODE, DigHiFiIntReg);
            break;

        case NvOdmQueryI2sDataCommFormat_I2S:
            DigHiFiIntReg = W8753_SET_REG_DEF(DIGITAL_HIFIINTERFACE, FORMAT, I2S_MODE, DigHiFiIntReg);
            break;

        case NvOdmQueryI2sDataCommFormat_LeftJustified:
            DigHiFiIntReg = W8753_SET_REG_DEF(DIGITAL_HIFIINTERFACE, FORMAT, LEFT_JUSTIFIED, DigHiFiIntReg);
            break;

        case NvOdmQueryI2sDataCommFormat_RightJustified:
            DigHiFiIntReg = W8753_SET_REG_DEF(DIGITAL_HIFIINTERFACE, FORMAT, RIGHT_JUSTIFIED, DigHiFiIntReg);
            break;

        default:
            // Default will be the i2s mode
            DigHiFiIntReg = W8753_SET_REG_DEF(DIGITAL_HIFIINTERFACE, FORMAT, I2S_MODE, DigHiFiIntReg);
            break;
    }

    if (hAudioCodec->WCodecInterface.I2sCodecLRLineControl == NvOdmQueryI2sLRLineControl_LeftOnLow)
        DigHiFiIntReg = W8753_SET_REG_DEF(DIGITAL_HIFIINTERFACE, LRP, LEFT_DACLRC_LOW, DigHiFiIntReg);
    else
        DigHiFiIntReg = W8753_SET_REG_DEF(DIGITAL_HIFIINTERFACE, LRP, LEFT_DACLRC_HIGH, DigHiFiIntReg);


    IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_HiFiAudioInterface, DigHiFiIntReg);
    if (IsSuccess)
    {
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_SampleRateControl1, SampleContReg);
    }
    if (IsSuccess)
    {
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_SampleRateControl2, SampleCont2Reg);
    }
    return IsSuccess;
}


static NvBool InitializeHeadphoneOut(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvU32 HeadphoneId, NvU32 Volume)
{
    NvU32 OutReg;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    OutReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_LOut1Volume];
    OutReg = W8753_SET_REG_DEF(LOUT1_VOL, LO1ZC, ENABLE, OutReg);
    hAudioCodec->WCodecRegVals[WCodecRegIndex_LOut1Volume] = OutReg;

    OutReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_ROut1Volume];
    OutReg = W8753_SET_REG_DEF(ROUT1_VOL, RO1ZC, ENABLE, OutReg);
    hAudioCodec->WCodecRegVals[WCodecRegIndex_ROut1Volume] = OutReg;

    OutReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_LOut2Volume];
    OutReg = W8753_SET_REG_DEF(LOUT2_VOL, LO2ZC, DISABLE, OutReg);
    hAudioCodec->WCodecRegVals[WCodecRegIndex_LOut2Volume] = OutReg;

    OutReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_ROut2Volume];
    OutReg = W8753_SET_REG_DEF(ROUT2_VOL, RO2ZC, DISABLE, OutReg);
    hAudioCodec->WCodecRegVals[WCodecRegIndex_ROut2Volume] = OutReg;

    // Set TOEN bit
    //OutReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_AdditionalControl_R18];
    //OutReg = W8753_SET_REG_DEF(ADDCTRL, TOEN, ENABLE, OutReg);
    //W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_AdditionalControl_R18, OutReg);

    return SetHeadphoneOutVolume(hOdmAudioCodec,
                (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
                HeadphoneId,
                NV_TRUE, Volume);
}

static NvBool InitializeAnalogAudioPath(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 ChannelId)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 MixerReg;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    // Set OnboardMic
    hAudioCodec->MicPortIndex = 1;

    // Enable LDAC to left mixer/ disable the LM to the leftmixer and reduce
    // the sound to 0.
    MixerReg  = hAudioCodec->WCodecRegVals[WCodecRegIndex_LeftOutMix1];
    MixerReg  = W8753_SET_REG_DEF(LEFTMIXER_CONTROL1, LD2LO, ENABLE, MixerReg);
    MixerReg  = W8753_SET_REG_DEF(LEFTMIXER_CONTROL1, LM2LO, DISABLE, MixerReg);
    MixerReg  = W8753_SET_REG_DEF(LEFTMIXER_CONTROL1, LM2LOVOL, DB0, MixerReg);
    IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LeftOutMix1, MixerReg);

    // Enable RDAC to right mixer, Disable RM to right mixer
    MixerReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_RightOutMix1];
    MixerReg = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL1, RD2RO, ENABLE, MixerReg);
    MixerReg = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL1, RM2RO, DISABLE, MixerReg);
    MixerReg = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL1, RM2ROVOL, DB0, MixerReg);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RightOutMix1, MixerReg);

    // Disable VDAC/sidetone to left mixer
    MixerReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_LeftOutMix2];
    MixerReg = W8753_SET_REG_DEF(LEFTMIXER_CONTROL2, VXD2LO, DISABLE, MixerReg);
    MixerReg = W8753_SET_REG_DEF(LEFTMIXER_CONTROL2, VXD2LOVOL, DB0, MixerReg);
    MixerReg = W8753_SET_REG_DEF(LEFTMIXER_CONTROL2, ST2LO, DISABLE, MixerReg);
    MixerReg = W8753_SET_REG_DEF(LEFTMIXER_CONTROL2, ST2LOVOL, DB0, MixerReg);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LeftOutMix2, MixerReg);

    // Disable VDac/sidetone to right mixer
    MixerReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_RightOutMix2];
    MixerReg = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL2, VXD2RO, DISABLE, MixerReg);
    MixerReg = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL2, VXD2ROVOL, DB0, MixerReg);
    MixerReg = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL2, ST2RO, DISABLE, MixerReg);
    MixerReg = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL2, ST2ROVOL, DB0, MixerReg);

    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RightOutMix2, MixerReg);

    //if (IsSuccess)
    //    IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_MonoOutMix1, MixerReg);
    //if (IsSuccess)
    //    IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_MonoOutMix2, MixerReg);

    return IsSuccess;
}

static NvBool SetDacVolume(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsMute)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 LDacVol, RDacVol;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    LDacVol = hAudioCodec->WCodecRegVals[WCodecRegIndex_LeftDacVolume];
    RDacVol = hAudioCodec->WCodecRegVals[WCodecRegIndex_RightDacVolume];

    if (IsMute)
    {
        LDacVol = W8753_SET_REG_DEF(LEFT_DAC_DIG_VOL, LDACVOL, DIGMUTE, LDacVol);
        LDacVol = W8753_SET_REG_DEF(LEFT_DAC_DIG_VOL, LDVU, BOTH, LDacVol);
        RDacVol = W8753_SET_REG_DEF(RIGHT_DAC_DIG_VOL, RDACVOL, DIGMUTE, RDacVol);
        RDacVol = W8753_SET_REG_DEF(RIGHT_DAC_DIG_VOL, RDVU, BOTH, RDacVol);
    }
    else
    {
        LDacVol = W8753_SET_REG_DEF(LEFT_DAC_DIG_VOL, LDACVOL, NORMAL, LDacVol);
        LDacVol = W8753_SET_REG_DEF(LEFT_DAC_DIG_VOL, LDVU, BOTH, LDacVol);
        RDacVol = W8753_SET_REG_DEF(RIGHT_DAC_DIG_VOL, RDACVOL, NORMAL, RDacVol);
        RDacVol = W8753_SET_REG_DEF(RIGHT_DAC_DIG_VOL, RDVU, BOTH, RDacVol);
    }

    IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LeftDacVolume, LDacVol);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RightDacVolume, RDacVol);

    return IsSuccess;
}

static NvBool SetDacMute(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsMute)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 DacControl;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    DacControl = hAudioCodec->WCodecRegVals[WCodecRegIndex_DacControl];

    IsSuccess = SetDacVolume(hOdmAudioCodec, IsMute);

    if (IsMute)
    {
        DacControl = W8753_SET_REG_DEF(DAC_CONTROL, DACMU, MUTE, DacControl);
    }
    else
    {
        DacControl = W8753_SET_REG_DEF(DAC_CONTROL, DACMU, UNMUTE, DacControl);
    }

    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_DacControl, DacControl);

    return IsSuccess;
}

/*
 * Sets the ADC format
 */
static NvBool
SetAdcDataFormat(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 PortId,
    NvOdmQueryI2sDataCommFormat DataFormat,
    NvU32 Channels)
{
    NvU32 AdcFormatReg = 0;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    AdcFormatReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_PcmAudioInterface];

    // If vdac is in Use - expecting the record to happen from vdac side
    // ignoring the hifi dac Format here.
    if ( (PortId == 0) && hAudioCodec->IsVoiceCodecInUse)
        return NV_TRUE;

    switch (DataFormat)
    {
        case NvOdmQueryI2sDataCommFormat_Dsp:
            AdcFormatReg = W8753_SET_REG_DEF(PCM_AUDIO_INTERFACE, PFORMAT, DSP_MODE, AdcFormatReg);
            break;

        case NvOdmQueryI2sDataCommFormat_I2S:
            AdcFormatReg = W8753_SET_REG_DEF(PCM_AUDIO_INTERFACE, PFORMAT, I2S_MODE, AdcFormatReg);
            break;

        case NvOdmQueryI2sDataCommFormat_LeftJustified:
            AdcFormatReg = W8753_SET_REG_DEF(PCM_AUDIO_INTERFACE, PFORMAT, LEFT_JUSTIFIED, AdcFormatReg);
            break;

        case NvOdmQueryI2sDataCommFormat_RightJustified:
            AdcFormatReg = W8753_SET_REG_DEF(PCM_AUDIO_INTERFACE, PFORMAT, RIGHT_JUSTIFIED, AdcFormatReg);
            break;

        default:
            // Default will be the i2s mode
            AdcFormatReg = W8753_SET_REG_DEF(PCM_AUDIO_INTERFACE, PFORMAT, I2S_MODE, AdcFormatReg);
            break;
    }

    // Set the channel
    if (Channels == 1)
        AdcFormatReg = W8753_SET_REG_DEF(PCM_AUDIO_INTERFACE, MONO, LEFTONLYDATA, AdcFormatReg);
    else
        AdcFormatReg = W8753_SET_REG_DEF(PCM_AUDIO_INTERFACE, MONO, LEFTRIGHTDATA, AdcFormatReg);

    return W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PcmAudioInterface, AdcFormatReg);
}

/*
 * Sets the PCM size for the audio data.
 */
static NvBool
SetDataFormat(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 PortId,
    NvOdmQueryI2sDataCommFormat DataFormat)
{
    NvU32 DataFormatReg = 0;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    DataFormatReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_HiFiAudioInterface];

    switch (DataFormat)
    {
        case NvOdmQueryI2sDataCommFormat_Dsp:
            DataFormatReg = W8753_SET_REG_DEF(DIGITAL_HIFIINTERFACE, FORMAT, DSP_MODE, DataFormatReg);
            break;

        case NvOdmQueryI2sDataCommFormat_I2S:
            DataFormatReg = W8753_SET_REG_DEF(DIGITAL_HIFIINTERFACE, FORMAT, I2S_MODE, DataFormatReg);
            break;

        case NvOdmQueryI2sDataCommFormat_LeftJustified:
            DataFormatReg = W8753_SET_REG_DEF(DIGITAL_HIFIINTERFACE, FORMAT, LEFT_JUSTIFIED, DataFormatReg);
            break;

        case NvOdmQueryI2sDataCommFormat_RightJustified:
            DataFormatReg = W8753_SET_REG_DEF(DIGITAL_HIFIINTERFACE, FORMAT, RIGHT_JUSTIFIED, DataFormatReg);
            break;

        default:
            // Default will be the i2s mode
            DataFormatReg = W8753_SET_REG_DEF(DIGITAL_HIFIINTERFACE, FORMAT, I2S_MODE, DataFormatReg);
            break;
    }
    return W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_HiFiAudioInterface, DataFormatReg);
}

/*
 * Sets the PCM size for the audio data.
 */
static NvBool
SetPcmSize(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 PortId,
    NvU32 PcmSize)
{
    NvU32 DataInfoReg;
    NvBool IsSuccess = NV_TRUE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    DataInfoReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_HiFiAudioInterface];
    if (PortId == 1)
        DataInfoReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_PcmAudioInterface];

    switch (PcmSize)
    {
        case 16:
            if (PortId == 1)
                DataInfoReg = W8753_SET_REG_DEF(PCM_AUDIO_INTERFACE, PWL, 16BITS, DataInfoReg);
            else
                DataInfoReg = W8753_SET_REG_DEF(DIGITAL_HIFIINTERFACE, WL, 16BITS, DataInfoReg);
            break;

        case 20:
            if (PortId == 1)
                DataInfoReg = W8753_SET_REG_DEF(PCM_AUDIO_INTERFACE, PWL, 16BITS, DataInfoReg);
            else
                DataInfoReg = W8753_SET_REG_DEF(DIGITAL_HIFIINTERFACE, WL, 20BITS, DataInfoReg);
            break;

        case 24:
            if (PortId == 1)
                DataInfoReg = W8753_SET_REG_DEF(PCM_AUDIO_INTERFACE, PWL, 16BITS, DataInfoReg);
            else
                DataInfoReg = W8753_SET_REG_DEF(DIGITAL_HIFIINTERFACE, WL, 24BITS, DataInfoReg);
            break;

        case 32:
            if (PortId == 1)
                DataInfoReg = W8753_SET_REG_DEF(PCM_AUDIO_INTERFACE, PWL, 16BITS, DataInfoReg);
            else
                DataInfoReg = W8753_SET_REG_DEF(DIGITAL_HIFIINTERFACE, WL, 32BITS, DataInfoReg);
            break;

        default:
            return NV_FALSE;
    }

    if (PortId == 1)
        IsSuccess =  W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PcmAudioInterface, DataInfoReg);
    else
        IsSuccess =  W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_HiFiAudioInterface, DataInfoReg);

    return IsSuccess;
}

static NvBool CodecPowerUpSeq(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;

     // Mute the Dac
    if (IsSuccess)
        IsSuccess = SetDacMute(hOdmAudioCodec, NV_TRUE);

    // Set VMID to low power standby mode
    IsSuccess = SetCodecPower(hOdmAudioCodec, PowerMode_Standby);

    return IsSuccess;
}

static NvBool SetPllFreq(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool Is11_2896MHz)
{
    NvBool IsSuccess = NV_TRUE, IsMCLKDiv2 = NV_FALSE;
    NvU32 Pll1Control1, Pll1Control2;
    NvU32 i;
    NvU32 Pll1Control3, Pll1Control4;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    NvU32 Pll1K_21_18, Pll1K_17_9;
    NvU32 Pll1K_8_0;
    NvU32 PllKConstant = 0, PllNConstant = 0;

    // check whether to set MCLK DIV2 bit
    // Obtain the desired PLL K and N values.
    if (hAudioCodec->MCLKFreq)
    {
        for ( i = 0 ; i < PLLFREQTABLE_ENTRYCOUNT; i+=2)
        {
            if ((hAudioCodec->MCLKFreq >> 1) == PLLFreqTable[i].MCLK)
                IsMCLKDiv2 = NV_TRUE;

            if ((hAudioCodec->MCLKFreq == PLLFreqTable[i].MCLK) ||
                ((hAudioCodec->MCLKFreq >> 1) == PLLFreqTable[i].MCLK))
            {
                PllKConstant = (Is11_2896MHz)? PLLFreqTable[i].kValue:
                                    PLLFreqTable[i+1].kValue;

                PllNConstant = (Is11_2896MHz)? PLLFreqTable[i].nValue:
                                    PLLFreqTable[i+1].nValue;
                break;
            }
        }
    }

    Pll1K_21_18 = (PllKConstant >> 18) & 0xF;
    Pll1K_17_9 = (PllKConstant >> 9) & 0x1FF;
    Pll1K_8_0 = (PllKConstant ) & 0x1FF;

    Pll1Control1 = hAudioCodec->WCodecRegVals[WCodecRegIndex_Pll1Control1];
    Pll1Control2 = hAudioCodec->WCodecRegVals[WCodecRegIndex_Pll1Control2];
    Pll1Control3 = hAudioCodec->WCodecRegVals[WCodecRegIndex_Pll1Control3];
    Pll1Control4 = hAudioCodec->WCodecRegVals[WCodecRegIndex_Pll1Control4];

    Pll1Control2 = W8753_SET_REG_VAL(PLL1_CONTROL2, PLL1N, PllNConstant, Pll1Control2);
    Pll1Control2 = W8753_SET_REG_VAL(PLL1_CONTROL2, PLL1K, Pll1K_21_18, Pll1Control2);
    Pll1Control3 = W8753_SET_REG_VAL(PLL1_CONTROL3, PLL1K, Pll1K_17_9, Pll1Control3);
    Pll1Control4 = W8753_SET_REG_VAL(PLL1_CONTROL4, PLL1K, Pll1K_8_0, Pll1Control4);

    if (IsMCLKDiv2)
    {
        Pll1Control1 = W8753_SET_REG_DEF(PLL1_CONTROL1, MCLK1DIV2, ENABLE, Pll1Control1);
    }

    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_Pll1Control1, Pll1Control1);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_Pll1Control2, Pll1Control2);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_Pll1Control3, Pll1Control3);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_Pll1Control4, Pll1Control4);

    return IsSuccess;
}

/*
 * Sets the sampling rate for the audio playback and record path.
 */
static NvBool
SetSamplingRate(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortType PortType,
    NvU32 SamplingRate)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 SRateContReg1;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    if (!hAudioCodec->WCodecInterface.IsCodecMasterMode)
    {
        hAudioCodec->WCodecControl.AdcSamplingRate = SamplingRate;
        hAudioCodec->WCodecControl.DacSamplingRate = SamplingRate;
        return NV_TRUE;
    }

    if ((hAudioCodec->WCodecControl.AdcSamplingRate == SamplingRate) &&
         (hAudioCodec->WCodecControl.DacSamplingRate == SamplingRate))
         return NV_TRUE;

    SRateContReg1 = hAudioCodec->WCodecRegVals[WCodecRegIndex_SampleRateControl1];
    SRateContReg1 = W8753_SET_REG_DEF(SAMPLING_CONTROL1, SRMODE, ADC_SAMP_SR_USB, SRateContReg1);

    // Codec as Master and not usb mode - set the PLL accordingly
    if (!hAudioCodec->WCodecInterface.IsUsbMode)
    {
        NvBool Is11_2896MHz = NV_FALSE, IsSetDesiredMhz = NV_FALSE;
        switch (SamplingRate)
        {
            case 8000:
            case 12000:
            case 16000:
            case 24000:
            case 32000:
            case 48000:
            case 96000:
                Is11_2896MHz    = NV_FALSE;
                IsSetDesiredMhz = NV_TRUE;
                break;

            case 8018:
            case 8021:
            case 11025:
            case 11259:
            case 22050:
            case 22058:
            case 44118:
            case 44100:
            case 88200:
            case 88235:
                Is11_2896MHz    = NV_TRUE;
                IsSetDesiredMhz = NV_TRUE;
                break;

            default:
                break;
        }

        if (IsSetDesiredMhz)
        {
            IsSuccess = SetPllFreq(hOdmAudioCodec, Is11_2896MHz);
        }
        if (!IsSetDesiredMhz || !IsSuccess)
        {
            return NV_FALSE;
        }
    }

    switch (SamplingRate)
    {
        case 8000:
            SRateContReg1 = W8753_SET_REG_DEF(SAMPLING_CONTROL1, SR, ADC_8_DAC_8, SRateContReg1);
            break;

        case 8018:
        case 8021:
            if (hAudioCodec->WCodecInterface.IsUsbMode)
            {
                SRateContReg1 = W8753_SET_REG_DEF(SAMPLING_CONTROL1, SR, USB_ADC_8_01_DAC_8_01, SRateContReg1);
            }
            else
            {
                SRateContReg1 = W8753_SET_REG_DEF(SAMPLING_CONTROL1, SR, NORMAL_ADC_8_01_DAC_8_01, SRateContReg1);
            }
            break;


        case 11025:
            if (hAudioCodec->WCodecInterface.IsUsbMode)
            {
                SRateContReg1 = W8753_SET_REG_DEF(SAMPLING_CONTROL1, SR, USB_ADC_11_025_DAC_11_025, SRateContReg1);
            }
            else
            {
                SRateContReg1 = W8753_SET_REG_DEF(SAMPLING_CONTROL1, SR, NORMAL_ADC_11_025_DAC_11_025, SRateContReg1);
            }
            break;

        case 12000:
            SRateContReg1 = W8753_SET_REG_DEF(SAMPLING_CONTROL1, SR, ADC_12_DAC_12, SRateContReg1);
            break;

        case 16000:
            SRateContReg1 = W8753_SET_REG_DEF(SAMPLING_CONTROL1, SR, ADC_16_DAC_16, SRateContReg1);
            break;

        case 22050:
        case 22058:
            if (hAudioCodec->WCodecInterface.IsUsbMode)
            {
                SRateContReg1 = W8753_SET_REG_DEF(SAMPLING_CONTROL1, SR, USB_ADC_22_05_DAC_22_05, SRateContReg1);
            }
            else
            {
                SRateContReg1 = W8753_SET_REG_DEF(SAMPLING_CONTROL1, SR, NORMAL_ADC_22_05_DAC_22_05, SRateContReg1);
            }
            break;

        case 24000:
            SRateContReg1 = W8753_SET_REG_DEF(SAMPLING_CONTROL1, SR, ADC_24_DAC_24, SRateContReg1);
            break;

        case 32000:
            SRateContReg1 = W8753_SET_REG_DEF(SAMPLING_CONTROL1, SR, ADC_32_DAC_32, SRateContReg1);
            break;

        case 44118:
        case 44100:
            if (hAudioCodec->WCodecInterface.IsUsbMode)
            {
                SRateContReg1 = W8753_SET_REG_DEF(SAMPLING_CONTROL1, SR, USB_ADC_44_1_DAC_44_1, SRateContReg1);
            }
            else
            {
                SRateContReg1 = W8753_SET_REG_DEF(SAMPLING_CONTROL1, SR, NORMAL_ADC_44_1_DAC_44_1, SRateContReg1);
            }
            break;

        case 48000:
            SRateContReg1 = W8753_SET_REG_DEF(SAMPLING_CONTROL1, SR, ADC_48_DAC_48, SRateContReg1);
            break;

        case 88200:
        case 88235:
            if (hAudioCodec->WCodecInterface.IsUsbMode)
            {
                SRateContReg1 = W8753_SET_REG_DEF(SAMPLING_CONTROL1, SR, USB_ADC_88_2_DAC_88_2, SRateContReg1);
            }
            else
            {
                SRateContReg1 = W8753_SET_REG_DEF(SAMPLING_CONTROL1, SR, NORMAL_ADC_88_2_DAC_88_2, SRateContReg1);
            }
            break;

        case 96000:
            SRateContReg1 = W8753_SET_REG_DEF(SAMPLING_CONTROL1, SR, ADC_96_DAC_96, SRateContReg1);
            break;

        default:
            return NV_FALSE;
    }

    NVODM_WM8753_LOG((WM8753_PREFIX "[SetSamplingRate] SampleRate %d RegValue %d \n", SamplingRate, SRateContReg1));
    IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_SampleRateControl1, SRateContReg1);

    if (IsSuccess)
    {
        hAudioCodec->WCodecControl.AdcSamplingRate = SamplingRate;
        hAudioCodec->WCodecControl.DacSamplingRate = SamplingRate;
    }
    return IsSuccess;
}

static NvBool InitializePLLControl(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    NvU32 ClockControl = hAudioCodec->WCodecRegVals[WCodecRegIndex_ClockControl];

    ClockControl = W8753_SET_REG_DEF(CLOCK_CONTROL, MCLKSEL, MCLK_PIN, ClockControl);
    ClockControl = W8753_SET_REG_DEF(CLOCK_CONTROL, PCMDIV, DISABLE, ClockControl);
    ClockControl = W8753_SET_REG_DEF(CLOCK_CONTROL, CLKEQ, MCLK_PLL1, ClockControl);

    if (hAudioCodec->WCodecInterface.IsCodecMasterMode)
    {
        NvU32 Pll1Control1 = hAudioCodec->WCodecRegVals[WCodecRegIndex_Pll1Control1];

        // Set default samplerate and set PLL1 for generating the clock
        //IsSuccess    = SetSamplingRate(hOdmAudioCodec, 0, 44100);
        ClockControl = W8753_SET_REG_DEF(CLOCK_CONTROL, MCLKSEL, PLL1, ClockControl);
        Pll1Control1 = W8753_SET_REG_DEF(PLL1_CONTROL1, PLL1EN, ENABLE, Pll1Control1);
        Pll1Control1 = W8753_SET_REG_DEF(PLL1_CONTROL1, PLL1RB, ACTIVE, Pll1Control1);
        Pll1Control1 = W8753_SET_REG_DEF(PLL1_CONTROL1, PLL1DIV2, ENABLE, Pll1Control1);
        if (IsSuccess)
            IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_Pll1Control1, Pll1Control1);
    }
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ClockControl, ClockControl);
    return IsSuccess;
}

/*
* Set the HeadPhone Mic set - MIC1
* Set the OnBoard Mic  - MIC2
*
*/
static NvBool SetMicInput(NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 PortId,
    NvOdmAudioConfigInOutSignal *pConfigIO)
{
    NvBool IsSuccess = NV_TRUE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    NvU32 AdcInputMode  = hAudioCodec->WCodecRegVals[WCodecRegIndex_AdcInputMode];
    NvU32 InputControl2 = hAudioCodec->WCodecRegVals[WCodecRegIndex_InputControl2];
    NvU32 AdcControl    = hAudioCodec->WCodecRegVals[WCodecRegIndex_AdcControl];

    AdcInputMode  = W8753_SET_REG_DEF(ADC_INPUT, RADCSEL, PGA, AdcInputMode);
    AdcInputMode  = W8753_SET_REG_DEF(ADC_INPUT, LADCSEL, PGA, AdcInputMode);
    InputControl2 = W8753_SET_REG_DEF(INPUT_CONTROL2, RXALC, NO_RX, InputControl2);
    InputControl2 = W8753_SET_REG_DEF(INPUT_CONTROL2, LINEALC, NO_LINE, InputControl2);
    InputControl2 = W8753_SET_REG_DEF(INPUT_CONTROL2, MIC1ALC, NO_MIC1, InputControl2);
    InputControl2 = W8753_SET_REG_DEF(INPUT_CONTROL2, MIC2ALC, NO_MIC2, InputControl2);

#if !defined(MIC_DETECT_ENABLE)
    hAudioCodec->MicPortIndex  = pConfigIO->InSignalId;
#endif

    //Set the MIC BOOST
    IsSuccess = SetMicrophoneInVolume(hOdmAudioCodec,pConfigIO->InSignalId,70);

    // Determine the Mic availability based on MicBias Bit setting - TO DO
    if (hAudioCodec->MicPortIndex == 0)
    {
        NvU32 LeftMixerReg2  = hAudioCodec->WCodecRegVals[WCodecRegIndex_LeftOutMix2];
        NvU32 RightMixerReg2 = hAudioCodec->WCodecRegVals[WCodecRegIndex_RightOutMix2];

        AdcControl = W8753_SET_REG_DEF(ADC_CONTROL, DATSEL, LEFT_LEFT, AdcControl);

        // Set the MicMux to Mic1 for SideTone
        InputControl2 = W8753_SET_REG_DEF(INPUT_CONTROL2, MICMUX, MIC1PREAMP, InputControl2);

        // Setting the Sidetone Vol to -15db as default.
        LeftMixerReg2  = W8753_SET_REG_VAL(LEFTMIXER_CONTROL2, ST2LOVOL, 0x7, LeftMixerReg2);
        RightMixerReg2 = W8753_SET_REG_VAL(RIGHTMIXER_CONTROL2, ST2ROVOL, 0x7, RightMixerReg2);

        if(pConfigIO->IsEnable)
        {
            // Enable sidetone to left mixer
            LeftMixerReg2 = W8753_SET_REG_DEF(LEFTMIXER_CONTROL2, ST2LO, ENABLE, LeftMixerReg2);
            // Enable sidetone to right mixer
            RightMixerReg2 = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL2, ST2RO, ENABLE, RightMixerReg2);
            // Need Alc mix for Left Adc data
            InputControl2 = W8753_SET_REG_DEF(INPUT_CONTROL2, MIC1ALC, MIC1, InputControl2);
        }
        else
        {
            // Disable sidetone to left mixer
            LeftMixerReg2 = W8753_SET_REG_DEF(LEFTMIXER_CONTROL2, ST2LO, DISABLE, LeftMixerReg2);
            // Disable sidetone to right mixer
            RightMixerReg2 = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL2, ST2RO, DISABLE, RightMixerReg2);
            // No Need Alc mix for Left Adc data
            InputControl2 = W8753_SET_REG_DEF(INPUT_CONTROL2, MIC1ALC, NO_MIC1, InputControl2);
        }
        if (IsSuccess)
            IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LeftOutMix2, LeftMixerReg2);
        if (IsSuccess)
            IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RightOutMix2, RightMixerReg2);
    }
    else
    {
        AdcControl = W8753_SET_REG_DEF(ADC_CONTROL, DATSEL, RIGHT_RIGHT, AdcControl);
        // Set the MicMux to Mic2 for SideTone
        InputControl2 = W8753_SET_REG_DEF(INPUT_CONTROL2, MICMUX, MIC2PREAMP, InputControl2);
    }

    IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_AdcInputMode, AdcInputMode);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_InputControl2, InputControl2);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_AdcControl, AdcControl);

    // Setting the shadows register MicVolume
    if(pConfigIO->IsEnable)
    {
            if (IsSuccess)
                IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LeftInputVolume,
                                                    hAudioCodec->WCodecControl.MicInVolLeft);
            if (IsSuccess)
                IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RightInputVolume,
                                                    hAudioCodec->WCodecControl.MicInVolRight);
    }
    return IsSuccess;
}

static NvBool SetInterfaceControl(NvOdmPrivAudioCodecHandle hOdmAudioCodec,
                                  NvU32  PortId,
                                  NvBool IsPowerOn)
{
    // If Dap2 is enabled - set the Mode for both VOICE and HIFI
    // and expecting the recording to go through VxADC.
    // Otherwise recording is done through HIFI ADC.
    NvBool IsSuccess = NV_TRUE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    NvU32 DigInterface = hAudioCodec->WCodecRegVals[WCodecRegIndex_InterfaceControl];
    NvU32 AdcControl   = hAudioCodec->WCodecRegVals[WCodecRegIndex_AdcControl];

    if ((PortId == 1) && IsPowerOn) // VoiceCodec
    {
        DigInterface = W8753_SET_REG_DEF(INTERFACE_CONTROL, VXDTRI, NORMAL, DigInterface);
        DigInterface = W8753_SET_REG_DEF(INTERFACE_CONTROL, IFMODE, VOICE_HIFI, DigInterface);
        AdcControl   = W8753_SET_REG_DEF(ADC_CONTROL, VXFILT, VOICE_FILTER, AdcControl);
    }
    else if (((PortId == 1) && !IsPowerOn) ||
            ((PortId == 0) && !hAudioCodec->IsVoiceCodecInUse))    // switch to HIFI
    {
        DigInterface = W8753_SET_REG_DEF(INTERFACE_CONTROL, ADCDTRI, NORMAL, DigInterface);
        DigInterface = W8753_SET_REG_DEF(INTERFACE_CONTROL, IFMODE,  HIFI, DigInterface);
        AdcControl   = W8753_SET_REG_DEF(ADC_CONTROL, VXFILT, HIFI_FILTER, AdcControl);
    }

    IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_InterfaceControl, DigInterface);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_AdcControl, AdcControl);

    return IsSuccess;
}
/*
 * Sets the input analog line to the input to ADC.
 */
static NvBool
SetInputPortIO(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 PortId,
    NvOdmAudioConfigInOutSignal *pConfigIO)
{
    NvU32 InputControl1;
    NvU32 AdcInputMode;
    NvU32 PowerManagement2;
    NvU32 PcmAudioInterface;
    NvBool IsSuccess = NV_FALSE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

//    NVODM_CHECK_CONDITION((PortId != 0), EXIT,
//                          (WM8753_PREFIX "[SetInputPortIO] Invalid PortId:%x!", PortId));
    NVODM_CHECK_CONDITION((pConfigIO->InSignalType != NvOdmAudioSignalType_LineIn &&
                           pConfigIO->InSignalType != NvOdmAudioSignalType_MicIn), EXIT,
                          (WM8753_PREFIX "[SetInputPortIO] Invalid InSignalType:%x!", pConfigIO->InSignalType));
    NVODM_CHECK_CONDITION(((pConfigIO->InSignalType == NvOdmAudioSignalType_LineIn && pConfigIO->InSignalId != 0) ||
                           (pConfigIO->InSignalType == NvOdmAudioSignalType_MicIn && pConfigIO->InSignalId > 1)), EXIT,
                          (WM8753_PREFIX "[SetInputPortIO] Invalid InSignalId:%x!", pConfigIO->InSignalId));

    AdcInputMode      = hAudioCodec->WCodecRegVals[WCodecRegIndex_AdcInputMode];
    InputControl1     = hAudioCodec->WCodecRegVals[WCodecRegIndex_InputControl1];
    PowerManagement2  = hAudioCodec->WCodecRegVals[WCodecRegIndex_PowerManagement2];
    PcmAudioInterface = hAudioCodec->WCodecRegVals[WCodecRegIndex_PcmAudioInterface];

    PcmAudioInterface = W8753_SET_REG_DEF(PCM_AUDIO_INTERFACE, ADCDOP, ADCDAT, PcmAudioInterface);

    IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PcmAudioInterface, PcmAudioInterface);

    if (pConfigIO->InSignalType == NvOdmAudioSignalType_LineIn)
    {
        NvU32 AdcControl = hAudioCodec->WCodecRegVals[WCodecRegIndex_AdcControl];
        AdcInputMode = W8753_SET_REG_DEF(ADC_INPUT, RADCSEL, LINE2, AdcInputMode);
        AdcInputMode = W8753_SET_REG_DEF(ADC_INPUT, LADCSEL, LINE1, AdcInputMode);
        InputControl1 = W8753_SET_REG_DEF(INPUT_CONTROL1, LM, LINE1, InputControl1);
        InputControl1 = W8753_SET_REG_DEF(INPUT_CONTROL1, RM, LINE2, InputControl1);
        PowerManagement2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, MICAMP1EN, OFF, PowerManagement2);
        PowerManagement2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, MICAMP2EN, OFF, PowerManagement2);
        AdcControl = W8753_SET_REG_DEF(ADC_CONTROL, DATSEL, LEFT_RIGHT, AdcControl);

        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_AdcInputMode, AdcInputMode);
        if (IsSuccess)
            IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_InputControl1, InputControl1);
        if (IsSuccess)
            IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement2, PowerManagement2);
        if (IsSuccess)
            IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_AdcControl, AdcControl);
    }

    else
    {
        if (IsSuccess)
            IsSuccess = SetMicInput(hOdmAudioCodec, PortId, pConfigIO);
    }

EXIT:

    return IsSuccess;
}


/*
 * Sets the output analog line from the DAC.
 */
static NvBool
SetOutputPortIO(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 PortId,
    NvOdmAudioConfigInOutSignal *pConfigIO)
{
    NvU32 LeftMixerCont1;
    NvU32 LeftMixerCont2;
    NvU32 RightMixerCont1;
    NvU32 RightMixerCont2;
    NvU32 PowerManagement3;
    NvBool IsSuccess = NV_FALSE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    NVODM_CHECK_CONDITION((pConfigIO->OutSignalType & ~(NvOdmAudioSignalType_LineOut |
                                                        NvOdmAudioSignalType_Speakers |
                                                        NvOdmAudioSignalType_HeadphoneOut)), EXIT,
                          (WM8753_PREFIX "[SetOutputPortIO] Invalid OutSignalType:%x!", pConfigIO->OutSignalType));
    NVODM_CHECK_CONDITION((pConfigIO->OutSignalId > 1), EXIT,
                          (WM8753_PREFIX "[SetOutputPortIO] Invalid OutSignalId:%x!", pConfigIO->OutSignalId));

    LeftMixerCont1 = hAudioCodec->WCodecRegVals[WCodecRegIndex_LeftOutMix1];
    LeftMixerCont2 = hAudioCodec->WCodecRegVals[WCodecRegIndex_LeftOutMix2];
    RightMixerCont1 = hAudioCodec->WCodecRegVals[WCodecRegIndex_RightOutMix1];
    RightMixerCont2 = hAudioCodec->WCodecRegVals[WCodecRegIndex_RightOutMix2];
    PowerManagement3 = hAudioCodec->WCodecRegVals[WCodecRegIndex_PowerManagement3];

    LeftMixerCont1 = W8753_SET_REG_DEF(LEFTMIXER_CONTROL1, LD2LO, ENABLE, LeftMixerCont1);
    LeftMixerCont1 = W8753_SET_REG_DEF(LEFTMIXER_CONTROL1, LM2LO, DISABLE, LeftMixerCont1);

    RightMixerCont1 = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL1, RD2RO, ENABLE, RightMixerCont1);
    RightMixerCont1 = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL1, RM2RO, DISABLE, RightMixerCont1);

    if (pConfigIO->OutSignalId == 0)
    {
        PowerManagement3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, LOUT1, ON, PowerManagement3);
        PowerManagement3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, ROUT1, ON, PowerManagement3);
        PowerManagement3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, LOUT2, OFF, PowerManagement3);
        PowerManagement3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, ROUT2, OFF, PowerManagement3);
    }
    else
    {
        PowerManagement3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, LOUT1, OFF, PowerManagement3);
        PowerManagement3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, ROUT1, OFF, PowerManagement3);
        PowerManagement3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, LOUT2, ON, PowerManagement3);
        PowerManagement3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, ROUT2, ON, PowerManagement3);
    }

    IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement3, PowerManagement3);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LeftOutMix1, LeftMixerCont1);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LeftOutMix2, LeftMixerCont2);

    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RightOutMix1, RightMixerCont1);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RightOutMix2, RightMixerCont2);

EXIT:

    return IsSuccess;
}

/*
 * Sets the codec bypass.
 */
static NvBool
SetBypass(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioSignalType InputAnalogLineType,
    NvU32 InputLineId,
    NvOdmAudioSignalType OutputAnalogLineType,
    NvU32 OutputLineId,
    NvU32 IsEnable)
{
    NvBool IsSuccess = NV_FALSE;
    NvU32 LeftMixerCont1;
    NvU32 LeftMixerCont2;
    NvU32 RightMixerCont1;
    NvU32 RightMixerCont2;
    NvU32 PowerManagement3;
    NvU32 InputControl1;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    NVODM_CHECK_CONDITION(((InputLineId > 0) || (OutputLineId > 1)), EXIT,
                          (WM8753_PREFIX "[SetBypass] Invalid LineId!"));

    LeftMixerCont1 = hAudioCodec->WCodecRegVals[WCodecRegIndex_LeftOutMix1];
    LeftMixerCont2 = hAudioCodec->WCodecRegVals[WCodecRegIndex_LeftOutMix2];
    RightMixerCont1 = hAudioCodec->WCodecRegVals[WCodecRegIndex_RightOutMix1];
    RightMixerCont2 = hAudioCodec->WCodecRegVals[WCodecRegIndex_RightOutMix2];
    PowerManagement3 = hAudioCodec->WCodecRegVals[WCodecRegIndex_PowerManagement3];
    InputControl1 = hAudioCodec->WCodecRegVals[WCodecRegIndex_InputControl1];

    LeftMixerCont1 = W8753_SET_REG_DEF(LEFTMIXER_CONTROL1, LD2LO, DISABLE, LeftMixerCont1);
    LeftMixerCont1 = W8753_SET_REG_DEF(LEFTMIXER_CONTROL1, LM2LO, DISABLE, LeftMixerCont1);
    LeftMixerCont2 = W8753_SET_REG_DEF(LEFTMIXER_CONTROL2, VXD2LO, DISABLE, LeftMixerCont2);
    LeftMixerCont2 = W8753_SET_REG_DEF(LEFTMIXER_CONTROL2, ST2LO, DISABLE, LeftMixerCont2);
    RightMixerCont1 = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL1, RD2RO, DISABLE, RightMixerCont1);
    RightMixerCont1 = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL1, RM2RO, DISABLE, RightMixerCont1);
    RightMixerCont2 = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL2, VXD2RO, DISABLE, RightMixerCont2);
    RightMixerCont2 = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL2, ST2RO, DISABLE, RightMixerCont2);

    if (InputAnalogLineType & NvOdmAudioSignalType_LineIn)
    {
        if (IsEnable)
        {
            LeftMixerCont1 = W8753_SET_REG_DEF(LEFTMIXER_CONTROL1, LM2LO, ENABLE, LeftMixerCont1);
            RightMixerCont1 = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL1, RM2RO, ENABLE, RightMixerCont1);
            InputControl1 = W8753_SET_REG_DEF(INPUT_CONTROL1, LM, LINE1, InputControl1 );
            InputControl1 = W8753_SET_REG_DEF(INPUT_CONTROL1, RM, LINE2, InputControl1 );
        }
    }

    if (InputAnalogLineType & NvOdmAudioSignalType_MicIn)
    {
        if (IsEnable)
        {
            LeftMixerCont2 = W8753_SET_REG_DEF(LEFTMIXER_CONTROL2, ST2LO, ENABLE, LeftMixerCont2);
            RightMixerCont2 = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL2, ST2RO, ENABLE, RightMixerCont2);
        }
    }
    if (OutputLineId == 0)
    {
        PowerManagement3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, LOUT1, ON, PowerManagement3);
        PowerManagement3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, ROUT1, ON, PowerManagement3);
        PowerManagement3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, LOUT2, OFF, PowerManagement3);
        PowerManagement3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, ROUT2, OFF, PowerManagement3);
    }
    else
    {
        PowerManagement3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, LOUT1, OFF, PowerManagement3);
        PowerManagement3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, ROUT1, OFF, PowerManagement3);
        PowerManagement3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, LOUT2, ON, PowerManagement3);
        PowerManagement3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, ROUT2, ON, PowerManagement3);
    }

    IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement3, PowerManagement3);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_InputControl1, InputControl1);

    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LeftOutMix1, LeftMixerCont1);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LeftOutMix2, LeftMixerCont2);

    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RightOutMix1, RightMixerCont1);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RightOutMix2, RightMixerCont2);

EXIT:

    return IsSuccess;
}


/*
 * Sets the side attenuation.
 */
static NvBool
SetSideToneAttenuation(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 SideToneAtenuation,
    NvBool IsEnabled)
{
    return NV_FALSE;
}

static NvBool
SetOperationMode(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 PortId,
    NvBool IsMaster)
{
    NvBool IsSuccess = NV_TRUE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    NvU32 RegisterValue = 0;
    NvU32 DigInterface = hAudioCodec->WCodecRegVals[WCodecRegIndex_InterfaceControl];

    if (PortId == 1) // Voice Codec
    {
        // Set PMS if codec is Master mode
        RegisterValue = hAudioCodec->WCodecRegVals[WCodecRegIndex_PcmAudioInterface];
        if (IsMaster)
        {
            RegisterValue = W8753_SET_REG_DEF(PCM_AUDIO_INTERFACE, PMS, MASTERMODE, RegisterValue);
            // Set VXFS pin accordingly - as Output
            DigInterface = W8753_SET_REG_DEF(INTERFACE_CONTROL, VXFSOE, OUTPUT, DigInterface);
            // Set VXCLKTRI state
            DigInterface = W8753_SET_REG_DEF(INTERFACE_CONTROL, VXCLKTRI, NORMAL, DigInterface);
        }
        else
        {
            RegisterValue = W8753_SET_REG_DEF(PCM_AUDIO_INTERFACE, PMS, SLAVEMODE, RegisterValue);
            // Set VXFS pin accordingly - as Input
            DigInterface = W8753_SET_REG_DEF(INTERFACE_CONTROL, VXFSOE, INPUT, DigInterface);
            // Set VXCLKTRI state
            DigInterface = W8753_SET_REG_DEF(INTERFACE_CONTROL, VXCLKTRI, TRISTATE, DigInterface);

        }
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PcmAudioInterface, RegisterValue);
    }
    else
    {
        RegisterValue = hAudioCodec->WCodecRegVals[WCodecRegIndex_HiFiAudioInterface];

        if (IsMaster)
        {
            RegisterValue = W8753_SET_REG_DEF(DIGITAL_HIFIINTERFACE, MS, MASTER_MODE, RegisterValue);
            // Set LRC pin accordingly - as Output
            DigInterface = W8753_SET_REG_DEF(INTERFACE_CONTROL, LRCOE, OUTPUT, DigInterface);
        }

        else
        {
            RegisterValue = W8753_SET_REG_DEF(DIGITAL_HIFIINTERFACE, MS, SLAVE_MODE, RegisterValue);
            // Set LRC pin accordingly - as Input
            DigInterface = W8753_SET_REG_DEF(INTERFACE_CONTROL, LRCOE, INPUT, DigInterface);
        }

        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_HiFiAudioInterface, RegisterValue);
    }

    if(IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_InterfaceControl, DigInterface);

    return IsSuccess;
}

//
//Set power registers to Enable/Disable Analog Rxn
//
static NvBool SetAnalogRxnPower(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioSignalType SignalType,
    NvU32 SignalId,
    NvBool IsPowerOn)
{
    NvBool IsSuccess = NV_TRUE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    NvU32 PowerControl1 = hAudioCodec->WCodecRegVals[WCodecRegIndex_PowerManagement1];
    NvU32 PowerControl2 = hAudioCodec->WCodecRegVals[WCodecRegIndex_PowerManagement2];
    NvU32 PowerControl3 = hAudioCodec->WCodecRegVals[WCodecRegIndex_PowerManagement3];
    NvU32 PowerControl4 = hAudioCodec->WCodecRegVals[WCodecRegIndex_PowerManagement4];

    if (IsPowerOn)
    {
        // Enable MIC-BIAS
        PowerControl1 = W8753_SET_REG_DEF(POWER_MANAGEMENT1, MICB, ON, PowerControl1);
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, MICAMP1EN, ON, PowerControl2);
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, RXMIX, ON, PowerControl2);
        PowerControl3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, OUT4, ON, PowerControl3);
        PowerControl3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, MONO1, ON, PowerControl3);
        PowerControl3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, MONO2, ON, PowerControl3);
        PowerControl4 = W8753_SET_REG_DEF(POWER_MANAGEMENT4, MONOMIX, ON, PowerControl4);
    }
    else
    {
        PowerControl1 = W8753_SET_REG_DEF(POWER_MANAGEMENT1, MICB, OFF, PowerControl1);
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, MICAMP1EN, OFF, PowerControl2);
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, RXMIX, OFF, PowerControl2);
        PowerControl3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, OUT4, OFF, PowerControl3);
        PowerControl3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, MONO1, OFF, PowerControl3);
        PowerControl3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, MONO2, OFF, PowerControl3);
        PowerControl4 = W8753_SET_REG_DEF(POWER_MANAGEMENT4, MONOMIX, OFF, PowerControl4);
    }

    IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement1, PowerControl1);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement2, PowerControl2);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement3, PowerControl3);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement4, PowerControl4);

    return IsSuccess;
}

//
//Set power registers for LineOut or HeadPhone Out device
//
static NvBool SetOutPortRegisters(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 SignalId,
    NvBool IsPowerOn)
{
    NvBool IsSuccess = NV_TRUE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    NvU32 PowerControl3 = hAudioCodec->WCodecRegVals[WCodecRegIndex_PowerManagement3];
    NvU32 PowerControl4 = hAudioCodec->WCodecRegVals[WCodecRegIndex_PowerManagement4];

    if (IsPowerOn)
    {
        PowerControl4 = W8753_SET_REG_DEF(POWER_MANAGEMENT4, RIGHTMIX, ON, PowerControl4);
        PowerControl4 = W8753_SET_REG_DEF(POWER_MANAGEMENT4, LEFTMIX, ON, PowerControl4);
        if (SignalId == 0)
        {
            PowerControl3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, LOUT1, ON, PowerControl3);
            PowerControl3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, ROUT1, ON, PowerControl3);
        }
        else
        {
            PowerControl3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, LOUT2, ON, PowerControl3);
            PowerControl3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, ROUT2, ON, PowerControl3);
        }
        PowerControl3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, OUT4, ON, PowerControl3);
    }
    else
    {
        PowerControl4 = W8753_SET_REG_DEF(POWER_MANAGEMENT4, RIGHTMIX, OFF, PowerControl4);
        PowerControl4 = W8753_SET_REG_DEF(POWER_MANAGEMENT4, LEFTMIX, OFF, PowerControl4);
        if (SignalId == 0)
        {
            PowerControl3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, LOUT1, OFF, PowerControl3);
            PowerControl3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, ROUT1, OFF, PowerControl3);
        }
        else
        {
            PowerControl3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, LOUT2, OFF, PowerControl3);
            PowerControl3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, ROUT2, OFF, PowerControl3);
        }
        //PowerControl3 = W8753_SET_REG_DEF(POWER_MANAGEMENT3, OUT4, OFF, PowerControl3);
    }

    IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement4, PowerControl4);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement3, PowerControl3);

    return IsSuccess;
}

static NvBool SetOutPortPower(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioSignalType SignalType,
    NvU32 SignalId,
    NvBool IsPowerOn)
{
    NvBool IsSuccess = NV_TRUE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    NvU32 PowerControl1 = hAudioCodec->WCodecRegVals[WCodecRegIndex_PowerManagement1];
    NvU32 LMixer1Reg    = hAudioCodec->WCodecRegVals[WCodecRegIndex_LeftOutMix1];
    NvU32 RMixer1Reg    = hAudioCodec->WCodecRegVals[WCodecRegIndex_RightOutMix1];

    if (IsPowerOn)
    {
//#if defined(MIC_DETECT_ENABLE)
        PowerControl1 = W8753_SET_REG_DEF(POWER_MANAGEMENT1, MICB, ON, PowerControl1);
//#endif
        PowerControl1 = W8753_SET_REG_DEF(POWER_MANAGEMENT1, DACL, ON, PowerControl1);
        PowerControl1 = W8753_SET_REG_DEF(POWER_MANAGEMENT1, DACR, ON, PowerControl1);
        LMixer1Reg    = W8753_SET_REG_DEF(LEFTMIXER_CONTROL1, LD2LO, ENABLE, LMixer1Reg);
        RMixer1Reg    = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL1, RD2RO, ENABLE, RMixer1Reg);
    }
    else
    {
//#if defined(MIC_DETECT_ENABLE)
        PowerControl1 = W8753_SET_REG_DEF(POWER_MANAGEMENT1, MICB, OFF, PowerControl1);
//#endif
        PowerControl1 = W8753_SET_REG_DEF(POWER_MANAGEMENT1, DACL, OFF, PowerControl1);
        PowerControl1 = W8753_SET_REG_DEF(POWER_MANAGEMENT1, DACR, OFF, PowerControl1);
        LMixer1Reg    = W8753_SET_REG_DEF(LEFTMIXER_CONTROL1, LD2LO, DISABLE, LMixer1Reg);
        RMixer1Reg    = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL1, RD2RO, DISABLE, RMixer1Reg);
    }

    IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LeftOutMix1, LMixer1Reg);

    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RightOutMix1, RMixer1Reg);

    if (IsSuccess)
    IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement1, PowerControl1);

    if (IsSuccess & !hAudioCodec->IsVoiceCodecInUse)
        SetOutPortRegisters(hOdmAudioCodec, SignalId, IsPowerOn);

    hAudioCodec->IsHiFiCodecInUse = IsPowerOn;
    return IsSuccess;
}

static NvBool SetGPIO3Register(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsEnable)
{
    NvBool IsSuccess = NV_TRUE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    NvU32 GpioControl2 = hAudioCodec->WCodecRegVals[WCodecRegIndex_GpioControl2];
    // Setting the Gpio3 bits to 100 -low(disable) and 101 high
    if (IsEnable)
    {
        NVODM_WM8753_LOG((WM8753_PREFIX "[SetGPIO3Register] Spkr on\n"));
        GpioControl2 = W8753_SET_REG_DEF(GPIO_CONTROL2, GPIO3M, DRIVE_HIGH, GpioControl2);
        IsSuccess    = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_GpioControl2, GpioControl2);
        if (!hAudioCodec->IsSpkrAmpEnabled)
        {
            NvOdmOsWaitUS(100000); // Adding 100ms wait to settle
        }
        hAudioCodec->IsSpkrAmpEnabled = NV_TRUE;
    }
    else
    {
        NVODM_WM8753_LOG((WM8753_PREFIX "[SetGPIO3Register] Spkr off\n"));
        GpioControl2 = W8753_SET_REG_DEF(GPIO_CONTROL2, GPIO3M, DRIVE_LOW, GpioControl2);
        IsSuccess    = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_GpioControl2, GpioControl2);
        hAudioCodec->IsSpkrAmpEnabled = NV_FALSE;
    }

    return IsSuccess;
}

static NvBool SetSpeakerMode(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsEnable)
{
    NvBool IsSuccess = NV_TRUE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    // Setting the Gpio3 bits to 100 -low(disable) and 101 high
    if (IsEnable)
    {
        hAudioCodec->StereoSpkrUseCount++;
        IsSuccess = SetGPIO3Register(hOdmAudioCodec, IsEnable);
        NVODM_WM8753_LOG((WM8753_PREFIX "[SetSpeakerMode] Spkr+ %d \n", hAudioCodec->StereoSpkrUseCount));
    }
    else
    {
        // Disable Speaker mode for now.
        if (hAudioCodec->StereoSpkrUseCount > 0)
            hAudioCodec->StereoSpkrUseCount--;

        NVODM_WM8753_LOG((WM8753_PREFIX "[SetSpeakerMode] Spkr- %d hpe:%d ese:%d\n", hAudioCodec->StereoSpkrUseCount,
                          hAudioCodec->IsHeadPhoneEnabled ? 1 : 0, hAudioCodec->IsEarSpkrEnabled ? 1 : 0));

        if ((!hAudioCodec->StereoSpkrUseCount)
            && (hAudioCodec->IsHeadPhoneEnabled || hAudioCodec->IsEarSpkrEnabled))
        {
            IsSuccess = SetGPIO3Register(hOdmAudioCodec, IsEnable);
        }
    }

    return IsSuccess;
}

// Set/Reset the ear speaker power and volume
static NvBool SetEarSpeaker(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsEnable)
{
    NvBool IsSuccess = NV_TRUE;
#if defined(EAR_SPEAKER_ENABLE)
    NvU32  LVolReg   = 0, RVolReg = 0;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    if (IsEnable)
    {
        // Already enabled - return no update
        if(hAudioCodec->IsEarSpkrEnabled)
           return NV_FALSE;
    }
    else
    {
        // Already disabled - return no update
        if (!hAudioCodec->IsEarSpkrEnabled)
            return NV_FALSE;
    }

    // Enable power
    // Enable Volume
    if (IsEnable)
    {
        IsSuccess = SetOutPortRegisters(hOdmAudioCodec, 0 , NV_FALSE);
        IsSuccess = SetOutPortRegisters(hOdmAudioCodec, 1 , NV_TRUE);
        LVolReg = hAudioCodec->WCodecControl.HPOut1VolLeft;
        RVolReg = hAudioCodec->WCodecControl.HPOut1VolRight;
    }
    else
    {
        IsSuccess = SetOutPortRegisters(hOdmAudioCodec, 1 , NV_FALSE);
        IsSuccess = SetOutPortRegisters(hOdmAudioCodec, 0 , NV_TRUE);
    }

    hAudioCodec->IsEarSpkrEnabled = IsEnable;

    W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LOut2Volume, LVolReg);
    hAudioCodec->WCodecControl.HPOut2VolLeft = LVolReg;
    W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ROut2Volume, RVolReg);
    hAudioCodec->WCodecControl.HPOut2VolRight = RVolReg;

#endif
    return IsSuccess;
}

// Select the right mode of operation and set the
// output speaker modes properly
static NvBool SelectOutputMode(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsEnable, NvBool IsForceMode)
{
    NvBool IsSuccess = NV_TRUE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    if (IsEnable)
    {
#if defined(EAR_SPEAKER_ENABLE)
        if ((hAudioCodec->IsVoiceCodecInUse) && !IsForceMode)
        {
            if ((hAudioCodec->StereoSpkrUseCount <= 1)
                && !hAudioCodec->IsHeadPhoneEnabled)
            {
                // Set Mode as EarSpeaker Type
                hAudioCodec->CodecOutputMode = OutputMode_EarSpkr;
            }
        }
        else
#endif
        {
             if (hAudioCodec->IsHeadPhoneEnabled)
                 // Set as HeadPhone Mode
                 hAudioCodec->CodecOutputMode = OutputMode_HeadPhone;
             else
                // Set as StereoSpeaker mode
                hAudioCodec->CodecOutputMode = OutputMode_StereoSpkr;
        }

    }

    NVODM_WM8753_LOG((WM8753_PREFIX "[SelectOutputMode] HP %d\n", hAudioCodec->IsHeadPhoneEnabled));
    switch (hAudioCodec->CodecOutputMode)
    {
        case OutputMode_EarSpkr:
            IsSuccess = SetEarSpeaker(hOdmAudioCodec, IsEnable);
            if (IsSuccess)
            {
                if (hAudioCodec->IsHeadPhoneEnabled)
                    IsSuccess = SetSpeakerMode(hOdmAudioCodec, IsEnable);
                else
                    IsSuccess = SetSpeakerMode(hOdmAudioCodec, !IsEnable);
            }
            break;
        case OutputMode_HeadPhone:
        case OutputMode_StereoSpkr:
        default:
            if (hAudioCodec->IsVoiceCodecInUse && !hAudioCodec->IsHeadPhoneEnabled)
                IsSuccess = SetEarSpeaker(hOdmAudioCodec, !IsEnable);
            if (IsSuccess)
                IsSuccess = SetSpeakerMode(hOdmAudioCodec, IsEnable);
            break;
    }
    return NV_TRUE;
}


//
// Set the power registers for VoiceDac device
//
static NvBool SetVoiceDacPower(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioSignalType SignalType,
    NvU32 SignalId,
    NvBool IsPowerOn)
{
    NvBool IsSuccess = NV_TRUE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    NvU32 PowerControl1 = hAudioCodec->WCodecRegVals[WCodecRegIndex_PowerManagement1];

    if (!hAudioCodec->IsHiFiCodecInUse)
        SetOutPortRegisters(hOdmAudioCodec, SignalId, IsPowerOn);

    if (IsPowerOn)
    {
        PowerControl1 = W8753_SET_REG_DEF(POWER_MANAGEMENT1, VDAC, ON, PowerControl1);
        IsSuccess = SetDacVolume(hOdmAudioCodec, NV_TRUE);
    }
    else
    {
        PowerControl1 = W8753_SET_REG_DEF(POWER_MANAGEMENT1, VDAC, OFF, PowerControl1);
        IsSuccess = SetDacVolume(hOdmAudioCodec, NV_FALSE);
    }

    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement1, PowerControl1);

    hAudioCodec->IsVoiceCodecInUse = IsPowerOn;
#if defined(EAR_SPEAKER_ENABLE)
    SelectOutputMode(hOdmAudioCodec, IsPowerOn, NV_FALSE);
#endif
    return IsSuccess;
}

//
// Set the power registers for LineIn device
//
static NvBool SetLineInPower(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioSignalType SignalType,
    NvU32 SignalId,
    NvBool IsPowerOn)
{
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    NvU32 PowerControl2 = hAudioCodec->WCodecRegVals[WCodecRegIndex_PowerManagement2];

    if (IsPowerOn)
    {
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, ADCL, ON, PowerControl2);
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, ADCR, ON, PowerControl2);
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, LINEMIX, ON, PowerControl2);
    }
    else
    {
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, ADCL, OFF, PowerControl2);
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, ADCR, OFF, PowerControl2);
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, LINEMIX, OFF, PowerControl2);
    }

    return W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement2, PowerControl2);
}

//
// Set the power registers for MicIn device
//
static NvBool SetMicInPower(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioSignalType SignalType,
    NvU32 SignalId,
    NvBool IsPowerOn)
{
    NvBool IsSuccess = NV_TRUE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    NvU32 PowerControl1 = hAudioCodec->WCodecRegVals[WCodecRegIndex_PowerManagement1];
    NvU32 PowerControl2 = hAudioCodec->WCodecRegVals[WCodecRegIndex_PowerManagement2];

#if !defined(MIC_DETECT_ENABLE)
    hAudioCodec->MicPortIndex  = SignalId;
#endif

    if (IsPowerOn)
    {
        PowerControl1 = W8753_SET_REG_DEF(POWER_MANAGEMENT1, MICB, ON, PowerControl1);
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, ADCL, ON, PowerControl2);
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, ADCR, ON, PowerControl2);
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, MICAMP1EN, OFF, PowerControl2);
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, MICAMP2EN, OFF, PowerControl2);
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, ALCMIX, OFF, PowerControl2);
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, PGAL, ON, PowerControl2);
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, PGAR, ON, PowerControl2);

        if (hAudioCodec->MicPortIndex == 0)
        {
            PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, MICAMP1EN, ON, PowerControl2);
            PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, ALCMIX, ON, PowerControl2);

        }else
        {
            PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, MICAMP2EN, ON, PowerControl2);
        }

        hAudioCodec->IsMicInUse = NV_TRUE;
    }
    else
    {
#if !defined(MIC_DETECT_ENABLE)
        //PowerControl1 = W8753_SET_REG_DEF(POWER_MANAGEMENT1, MICB, OFF, PowerControl1);
#endif
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, ADCL, OFF, PowerControl2);
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, ADCR, OFF, PowerControl2);
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, ALCMIX, OFF, PowerControl2);
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, PGAL, OFF, PowerControl2);
        PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, PGAR, OFF, PowerControl2);
        if (hAudioCodec->MicPortIndex == 0)
        {
            PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, MICAMP1EN, OFF, PowerControl2);
        }
        else
        {
            PowerControl2 = W8753_SET_REG_DEF(POWER_MANAGEMENT2, MICAMP2EN, OFF, PowerControl2);
        }

        hAudioCodec->IsMicInUse = NV_FALSE;
    }

    IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement1, PowerControl1);

    IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerManagement2, PowerControl2);

    return IsSuccess;
}

/*
 * Sets the power status of the specified devices of the audio codec.
 */
static NvBool
SetPowerStatus(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 PortId,
    NvOdmAudioSignalType SignalType,
    NvU32 SignalId,
    NvU32 ChannelMask,
    NvBool IsPowerOn)
{
    NvBool IsSuccess = NV_TRUE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    NVODM_WM8753_LOG((WM8753_PREFIX "++[SetPowerStatus] PortId:%d SignalType:%x:%s SignalId:%d %s\n",
                      PortId, SignalType,
                      (SignalType == NvOdmAudioSignalType_LineIn ? "LineIn" :
                       SignalType == NvOdmAudioSignalType_MicIn ? "MicIn" : "OTHER"),
                       SignalId, (IsPowerOn ? "ON" : "OFF")));

    if (SignalType & NvOdmAudioSignalType_LineIn)
    {
        IsSuccess = SetLineInPower(hOdmAudioCodec, SignalType, SignalId, IsPowerOn);
        NVODM_CHECK_CONDITION(!IsSuccess, EXIT, (WM8753_PREFIX "[SetPowerStatus] SetLineInPower failed!"));
    }

    if (SignalType & NvOdmAudioSignalType_MicIn)
    {
        IsSuccess  = SetMicInPower(hOdmAudioCodec, SignalType, SignalId, IsPowerOn);
        NVODM_CHECK_CONDITION(!IsSuccess, EXIT, (WM8753_PREFIX "[SetPowerStatus] SetMicInPower failed!"));
    }

    if (SignalType & (NvOdmAudioSignalType_LineOut | NvOdmAudioSignalType_HeadphoneOut | NvOdmAudioSignalType_Speakers))
    {
        NvBool signalBool = 0;

        if (IsPowerOn)
        {
            hAudioCodec->SignalPower |= SignalType;
        }

        else
        {
            hAudioCodec->SignalPower &= ~SignalType;
        }

        if (SignalType & NvOdmAudioSignalType_HeadphoneOut)
        {
            hAudioCodec->IsHeadPhoneEnabled = IsPowerOn;
        }

        signalBool = hAudioCodec->SignalPower ? NV_TRUE : NV_FALSE;

        NVODM_WM8753_LOG((WM8753_PREFIX "[SetPowerStatus] SignalType:0x%08x IsPowerOn:%d signalPower:0x%08x signalBool:%d\n",
                          SignalType, IsPowerOn ? NV_TRUE : NV_FALSE, hAudioCodec->SignalPower, signalBool));
        if (PortId == 0)
        {
            IsSuccess = SetOutPortPower(hOdmAudioCodec, SignalType, SignalId, signalBool);
            NVODM_CHECK_CONDITION(!IsSuccess, EXIT, (WM8753_PREFIX "[SetPowerStatus] SetOutPortPower failed!"));
        }

        else
        {
            // Control the power for Vdac
            IsSuccess = SetVoiceDacPower(hOdmAudioCodec, SignalType, SignalId, IsPowerOn);
            NVODM_CHECK_CONDITION(!IsSuccess, EXIT, (WM8753_PREFIX "[SetPowerStatus] SetVoiceDacPower failed!"));
        }

        if (SignalType & NvOdmAudioSignalType_Speakers)
        {
            IsSuccess = SetSpeakerMode(hOdmAudioCodec, IsPowerOn);
            NVODM_CHECK_CONDITION(!IsSuccess, EXIT, (WM8753_PREFIX "[SetPowerStatus] SetSpeakerMode failed!"));
            // Signal the settling semaphore so the event thread does not wait.
            if (!IsPowerOn) {
                if (hAudioCodec->hSettlingSema)
                    NvOdmOsSemaphoreSignal(hAudioCodec->hSettlingSema);
            }
        }

        IsSuccess = SetInterfaceControl(hOdmAudioCodec, PortId, IsPowerOn);
        NVODM_CHECK_CONDITION(!IsSuccess, EXIT, (WM8753_PREFIX "[SetPowerStatus] SetInterfaceControl failed!"));
    }


    if (SignalType & NvOdmAudioSignalType_Analog_Rxn)
    {
        IsSuccess = SetAnalogRxnPower(hOdmAudioCodec, SignalType, SignalId, IsPowerOn);
        NVODM_CHECK_CONDITION(!IsSuccess, EXIT, (WM8753_PREFIX "[SetPowerStatus] SetAnalogRxnPower failed!"));
    }

EXIT:

    NVODM_WM8753_LOG((WM8753_PREFIX "--[SetPowerStatus]"));
    return IsSuccess;
}

#if ENABLE_RXN_SETTINGS

static NvBool ConfigureInputLines(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvOdmAudioConfigInOutSignal ConfigIO;
    NvOdmAudioVolume Volume[2];
    NvBool IsSuccess;

    // Set input line
    ConfigIO.InSignalType = gs_InputSignalType;
    ConfigIO.InSignalId = gs_InputSignalId;
    ConfigIO.IsEnable = NV_TRUE;
    ConfigIO.OutSignalType = NvOdmAudioSignalType_Digital_Pcm;
    ConfigIO.OutSignalId = 0;
    IsSuccess = SetInputPortIO(hOdmAudioCodec, 0, &ConfigIO);

    // Set input volume
    Volume[0].IsLinear= NV_TRUE;
    Volume[0].SignalType = gs_InputSignalType;
    Volume[0].SignalId = gs_InputSignalId;
    Volume[0].ChannelType = NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight;
    Volume[0].VolumeLevel = 50;

    if (IsSuccess)
        ACW8753SetVolume(hOdmAudioCodec,
                        NVODMAUDIO_PORTNAME(NvOdmAudioPortType_Input, 0),
                        (void *)Volume, 1);

    // Power on the input path
    if (IsSuccess)
        SetPowerStatus(hOdmAudioCodec, 0, gs_InputSignalType, gs_InputSignalId, 0, NV_TRUE);

    return IsSuccess;

}
static NvBool EnableRxn(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    NvU32 LeftMix1  = hAudioCodec->WCodecRegVals[WCodecRegIndex_LeftOutMix1];
    NvU32 RightMix1 = hAudioCodec->WCodecRegVals[WCodecRegIndex_RightOutMix1];
    NvU32 MonoMix2  = hAudioCodec->WCodecRegVals[WCodecRegIndex_MonoOutMix2];
    NvU32 LVol1     = hAudioCodec->WCodecRegVals[WCodecRegIndex_LOut1Volume];
    NvU32 RVol1     = hAudioCodec->WCodecRegVals[WCodecRegIndex_ROut1Volume];
    NvU32 LVol2    = hAudioCodec->WCodecRegVals[WCodecRegIndex_LOut2Volume];
    NvU32 RVol2     = hAudioCodec->WCodecRegVals[WCodecRegIndex_ROut2Volume];
    NvU32 InCtrl1   = hAudioCodec->WCodecRegVals[WCodecRegIndex_InputControl1];
    NvU32 InCtrl2   = hAudioCodec->WCodecRegVals[WCodecRegIndex_InputControl2];
    NvU32 LInVol    = hAudioCodec->WCodecRegVals[WCodecRegIndex_LeftInputVolume];
    NvU32 RInVol    = hAudioCodec->WCodecRegVals[WCodecRegIndex_RightInputVolume];
    NvU32 MicBias   = hAudioCodec->WCodecRegVals[WCodecRegIndex_MicBiasControl];

    IsSuccess = SetPowerStatus(hOdmAudioCodec, 0, NvOdmAudioSignalType_Analog_Rxn, 0,
                    (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
                     NV_TRUE);

    // Enable LM signal to LeftMixer and Enable max volume.
    LeftMix1 = W8753_SET_REG_DEF(LEFTMIXER_CONTROL1, LM2LO, ENABLE, LeftMix1);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LeftOutMix1, LeftMix1);

    // Enable RM signal to RightMixer and Enable max volume.
    RightMix1 = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL1, RM2RO, ENABLE, RightMix1);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RightOutMix1, RightMix1);

    // Enable ST signal in Monomix2 and enable max volume
    MonoMix2 = W8753_SET_REG_DEF(MONOMIXER_CONTROL2, ST2MO, ENABLE, MonoMix2);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_MonoOutMix2, MonoMix2);

    // Enable LO1VU in LeftVol1
    LVol1 = W8753_SET_REG_DEF(LOUT1_VOL, LO1VU, BOTH, LVol1);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LOut1Volume, LVol1);

    // Enable R01VU in RightVol1
    RVol1 = W8753_SET_REG_DEF(ROUT1_VOL, RO1VU, BOTH, RVol1);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ROut1Volume, RVol1);

    // Enable LO2VU in LeftVol2
    LVol2 = W8753_SET_REG_DEF(LOUT2_VOL, LO2VU, BOTH, LVol2);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LOut2Volume, LVol2);

    // Enable R02VU in RightVol2
    RVol2 = W8753_SET_REG_DEF(ROUT2_VOL, RO2VU, BOTH, RVol2);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ROut2Volume, RVol2);

    // Enable RM/LM/MIC1BooST in InputCtrl1
    InCtrl1 = W8753_SET_REG_DEF(INPUT_CONTROL1, LM, RXMIX_OUT, InCtrl1);
    InCtrl1 = W8753_SET_REG_DEF(INPUT_CONTROL1, RM, RXMIX_OUT, InCtrl1);
    InCtrl1 = W8753_SET_REG_DEF(INPUT_CONTROL1, MIC1BOOST, DB18, InCtrl1);

    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_InputControl1, InCtrl1);

    // Enable micmux to st in inputctrl2
    InCtrl2 = W8753_SET_REG_DEF(INPUT_CONTROL2, MICMUX, MIC1PREAMP, InCtrl2);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_InputControl2, InCtrl2);

    // Enable LIVU/LZCEN in LInVol
    LInVol = W8753_SET_REG_DEF(LEFT_CHANNEL_PGA_VOL, LIVU, BOTH, LInVol);
    LInVol = W8753_SET_REG_DEF(LEFT_CHANNEL_PGA_VOL, LINMUTE, DISABLE, LInVol);
    LInVol = W8753_SET_REG_DEF(LEFT_CHANNEL_PGA_VOL, LZCEN, ENABLE, LInVol);

    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LeftInputVolume, LInVol);

    // Enable RIVU/RZCEN in RInVol
    RInVol = W8753_SET_REG_DEF(RIGHT_CHANNEL_PGA_VOL, RIVU, BOTH, RInVol);
    RInVol = W8753_SET_REG_DEF(RIGHT_CHANNEL_PGA_VOL, RINMUTE, DISABLE, RInVol);
    RInVol = W8753_SET_REG_DEF(RIGHT_CHANNEL_PGA_VOL, RZCEN, ENABLE, RInVol);

    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RightInputVolume, RInVol);

    // Enable .75v/RZCEN in MicBias
    MicBias = W8753_SET_REG_DEF(MICBIAS_CONTROL, MBVSEL, 75V, MicBias);
    MicBias = W8753_SET_REG_DEF(MICBIAS_CONTROL, MICSEL, MIC1, MicBias);
    MicBias = W8753_SET_REG_DEF(MICBIAS_CONTROL, MBSCTHRESH, MAX, MicBias);
    MicBias = W8753_SET_REG_DEF(MICBIAS_CONTROL, MBTHRESH, MAX, MicBias);

    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_MicBiasControl, MicBias);

    if (IsSuccess)
        IsSuccess = ConfigureInputLines(hOdmAudioCodec);

    return IsSuccess;
}
#endif

/*
*  Use PLL2 to generate the Vdac clock
*  Trying to Generate 12.288 out of Pll2 and divide by 6 to generate 2.048Mhz
*/
static NvBool UsePLL2ForVdacClock(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvOdmAudioConfigInOutSignal *pConfigIO)
{
    NvBool IsSuccess = NV_TRUE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    NvU32 Pll2Control  = hAudioCodec->WCodecRegVals[WCodecRegIndex_Pll2Control1];
    NvU32 ClockControl = hAudioCodec->WCodecRegVals[WCodecRegIndex_ClockControl];
    NvU32 K_Value = 0, PllKConstant = 0 , PllNConstant = 0;

    // if MCLKFreq == 26 or 24 ( div ) MCLK by 2
    if ((hAudioCodec->MCLKFreq == 26000) ||(hAudioCodec->MCLKFreq == 24000))
    {
        Pll2Control = W8753_SET_REG_DEF(PLL2_CONTROL1, MCLK2DIV2, ENABLE, Pll2Control);
    }

    if ((hAudioCodec->MCLKFreq == 26000) ||(hAudioCodec->MCLKFreq == 13000))
    {
        PllKConstant = 0x23F54A;
        PllNConstant = 0x7;
    }
    else if ((hAudioCodec->MCLKFreq == 12000) ||(hAudioCodec->MCLKFreq == 24000))
    {
        PllKConstant = 0xC49BA;
        PllNConstant = 0x8;
    }

    if (pConfigIO->IsEnable)
    {
        Pll2Control = W8753_SET_REG_DEF(PLL2_CONTROL1, PLL2EN, ENABLE, Pll2Control);
        Pll2Control = W8753_SET_REG_DEF(PLL2_CONTROL1, PLL2RB, ACTIVE, Pll2Control);
        Pll2Control = W8753_SET_REG_DEF(PLL2_CONTROL1, PLL2DIV2, ENABLE, Pll2Control);
    }
    else
    {
        Pll2Control = W8753_SET_REG_DEF(PLL2_CONTROL1, PLL2EN, DISABLE, Pll2Control);
        Pll2Control = W8753_SET_REG_DEF(PLL2_CONTROL1, PLL2RB, RESET, Pll2Control);
    }

    // For testing purpose - enable clk2sel also
    //Pll2Control = W8753_SET_REG_DEF(PLL2_CONTROL1, CLK2SEL, PLL2BLE, Pll2Control);

    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_Pll2Control1, Pll2Control);

    if (pConfigIO->IsEnable)
    {
        Pll2Control = hAudioCodec->WCodecRegVals[WCodecRegIndex_Pll2Control2];
        K_Value = (PllKConstant >> 18) & W8753_PLL2_CONTROL2_0_PLL2K_SEL;
        Pll2Control = W8753_SET_REG_VAL(PLL2_CONTROL2, PLL2N, PllNConstant, Pll2Control);
        Pll2Control = W8753_SET_REG_VAL(PLL2_CONTROL2, PLL2K, K_Value, Pll2Control);

        if (IsSuccess)
            IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_Pll2Control2, Pll2Control);

        Pll2Control = hAudioCodec->WCodecRegVals[WCodecRegIndex_Pll2Control3];
        K_Value = (PllKConstant >> 9) & W8753_PLL2_CONTROL3_0_PLL2K_SEL;
        Pll2Control = W8753_SET_REG_VAL(PLL2_CONTROL3, PLL2K, K_Value, Pll2Control);

        if (IsSuccess)
            IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_Pll2Control3, Pll2Control);

        Pll2Control = hAudioCodec->WCodecRegVals[WCodecRegIndex_Pll2Control4];
        K_Value = (PllKConstant) & W8753_PLL2_CONTROL4_0_PLL2K_SEL;
        Pll2Control = W8753_SET_REG_VAL(PLL2_CONTROL4, PLL2K, K_Value, Pll2Control);

        if (IsSuccess)
            IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_Pll2Control4, Pll2Control);

        // As we are getting 12.288 Clock - div by 6
        ClockControl = W8753_SET_REG_DEF(CLOCK_CONTROL, PCMDIV, DIV6, ClockControl);
        ClockControl = W8753_SET_REG_DEF(CLOCK_CONTROL, PCMCLKSEL, PLL2, ClockControl);
        ClockControl = W8753_SET_REG_DEF(CLOCK_CONTROL, CLKEQ, PCMCLK_PLL2, ClockControl);
    }
    else
    {
        ClockControl = W8753_SET_REG_DEF(CLOCK_CONTROL, PCMDIV, DISABLE, ClockControl);
        ClockControl = W8753_SET_REG_DEF(CLOCK_CONTROL, PCMCLKSEL, PCMCLK_PIN, ClockControl);
        ClockControl = W8753_SET_REG_DEF(CLOCK_CONTROL, CLKEQ, MCLK_PLL1, ClockControl);
    }
    // For testing purpose - Enable the GP2CLKSEL
    //ClockControl = W8753_SET_REG_DEF(CLOCK_CONTROL, GP2CLK2SEL, CLK2_OUT, ClockControl);

    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ClockControl, ClockControl);

    return IsSuccess;
}

static NvBool SetVdacClock(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvOdmAudioConfigInOutSignal *pConfigIO)
{
    NvU32 ClockControl;
    NvBool IsSuccess = NV_TRUE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    ClockControl = hAudioCodec->WCodecRegVals[WCodecRegIndex_ClockControl];

    //Setting the clock for Voice DAC
    //Using MCLK as the source for VDAC -  MCLK is driven from PLLA.
    //PLLA is 11.289
    if (!hAudioCodec->WCodecInterface.IsCodecMasterMode)
    {
        ClockControl = W8753_SET_REG_DEF(CLOCK_CONTROL, MCLKSEL, MCLK_PIN, ClockControl);
        ClockControl = W8753_SET_REG_DEF(CLOCK_CONTROL, CLKEQ, MCLK_PLL1, ClockControl);
        ClockControl = W8753_SET_REG_DEF(CLOCK_CONTROL, PCMDIV, DIV5_5, ClockControl);
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ClockControl, ClockControl);
    }
    else
    {
        // use pll2
        IsSuccess = UsePLL2ForVdacClock(hOdmAudioCodec, pConfigIO);
    }

    return IsSuccess;
}

static NvBool SetVdacRegisters(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvOdmAudioConfigInOutSignal *pConfigIO)
{
    NvBool IsSuccess     = NV_TRUE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    NvU32 RegisterValue               = 0;

    // Set the vdac clock
    if (IsSuccess)
        SetVdacClock(hOdmAudioCodec, pConfigIO);

    // Need both Vdac + Hifi
    RegisterValue = hAudioCodec->WCodecRegVals[WCodecRegIndex_InterfaceControl];
    if (pConfigIO->IsEnable)
    {
        RegisterValue = W8753_SET_REG_DEF(INTERFACE_CONTROL, IFMODE, VOICE_HIFI, RegisterValue);
        if (IsSuccess)
            IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_InterfaceControl, RegisterValue);
    }

    // Setting SRMODE bit to use bit for VDAC
    RegisterValue = hAudioCodec->WCodecRegVals[WCodecRegIndex_SampleRateControl1];
    if (pConfigIO->IsEnable)
        RegisterValue = W8753_SET_REG_DEF(SAMPLING_CONTROL1, SRMODE, ADC_SAMP_PSR, RegisterValue);
    else
        RegisterValue = W8753_SET_REG_DEF(SAMPLING_CONTROL1, SRMODE, ADC_SAMP_SR_USB, RegisterValue);

    if (IsSuccess)
      IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_SampleRateControl1, RegisterValue);

    // Enable VXD to left mixer & right mixer
    RegisterValue  = hAudioCodec->WCodecRegVals[WCodecRegIndex_LeftOutMix2];
    if (pConfigIO->IsEnable)
    {
        // Setting the VDAC volume to Max
        NVODM_WM8753_LOG((WM8753_PREFIX "[SetVdacRegisters] Enabled \n"));
        RegisterValue = W8753_SET_REG_DEF(LEFTMIXER_CONTROL2, VXD2LO, ENABLE, RegisterValue);
        RegisterValue = W8753_SET_REG_DEF(LEFTMIXER_CONTROL2, VXD2LOVOL, MAX, RegisterValue);
    }
    else
    {
        NVODM_WM8753_LOG((WM8753_PREFIX "[SetVdacRegisters] Disabled \n"));
        RegisterValue = W8753_SET_REG_DEF(LEFTMIXER_CONTROL2, VXD2LOVOL, DB0, RegisterValue);
        RegisterValue = W8753_SET_REG_DEF(LEFTMIXER_CONTROL2, VXD2LO, DISABLE, RegisterValue);
    }

    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LeftOutMix2, RegisterValue);

    RegisterValue = hAudioCodec->WCodecRegVals[WCodecRegIndex_RightOutMix2];
    if (pConfigIO->IsEnable)
    {
        RegisterValue = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL2, VXD2RO, ENABLE, RegisterValue);
        RegisterValue = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL2, VXD2ROVOL, MAX, RegisterValue);
    }
    else
    {
        RegisterValue = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL2, VXD2ROVOL, DB0, RegisterValue);
        RegisterValue = W8753_SET_REG_DEF(RIGHTMIXER_CONTROL2, VXD2RO, DISABLE, RegisterValue);
    }
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RightOutMix2, RegisterValue);

    RegisterValue = hAudioCodec->WCodecRegVals[WCodecRegIndex_SampleRateControl2];
    if (pConfigIO->IsEnable)
        RegisterValue = W8753_SET_REG_DEF(SAMPLING_CONTROL2, VXDOSR, 128X, RegisterValue);
    else
        RegisterValue = W8753_SET_REG_DEF(SAMPLING_CONTROL2, VXDOSR, 64X, RegisterValue);

    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_SampleRateControl2, RegisterValue);

    return IsSuccess;
}

static NvBool OpenWolfsonCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;

    IsSuccess = SetCodecPower(hOdmAudioCodec, PowerMode_Standby);

    // Reset the codec.
    if (IsSuccess)
        IsSuccess = ResetCodec(hOdmAudioCodec);

    // To avoid the pop up sound.
    if (IsSuccess)
        IsSuccess = CodecPowerUpSeq(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = InitializePLLControl(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = InitializeDigitalInterface(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = InitializeAnalogAudioPath(hOdmAudioCodec, 0);

    // can be moved later once we set this along with the volume change
    if (IsSuccess)
        IsSuccess = SetMicrophoneInVolume(hOdmAudioCodec,1,50);

    if (IsSuccess)
        IsSuccess = InitializeHeadphoneOut(hOdmAudioCodec, 0, 100);

    if (IsSuccess)
        IsSuccess = SetPowerStatus(hOdmAudioCodec, 0,  NvOdmAudioSignalType_HeadphoneOut, 0,
            (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
                NV_TRUE);


    // VMID - 50k and enable VREF and wait
    if (IsSuccess)
    {
        IsSuccess = SetCodecPower(hOdmAudioCodec, PowerMode_On);
    }


#if ENABLE_RXN_SETTINGS
    if (IsSuccess)
        IsSuccess = EnableRxn(hOdmAudioCodec);
#endif

    if (IsSuccess)
        IsSuccess = SetDacMute(hOdmAudioCodec, NV_FALSE);

    if (!IsSuccess)
        ShutDownCodec(hOdmAudioCodec);
    return IsSuccess;
}

static const NvOdmAudioPortCaps * ACW8753GetPortCaps(void)
{
    static const NvOdmAudioPortCaps s_PortCaps=
    {
        1,   // MaxNumberOfInputPort
        2    // MaxNumberOfOutputPort
    };

    return &s_PortCaps;
}


static const NvOdmAudioOutputPortCaps *
ACW8753GetOutputPortCaps(
    NvU32 OutputPortId)
{
    static const NvOdmAudioOutputPortCaps s_OutputPortCaps =
            {
                1,          // MaxNumberOfDigitalInput;
                2,          // MaxNumberOfLineOut;
                2,          // MaxNumberOfHeadphoneOut;
                2,          // MaxNumberOfSpeakers;
                NV_TRUE,    // IsShortCircuitCurrLimitSupported;
                NV_FALSE,   // IsNonLinerVolumeSupported;
                0,          // MaxVolumeInMilliBel;
                0,          // MinVolumeInMilliBel;
            };

    if (OutputPortId == 0)
        return &s_OutputPortCaps;
    return NULL;
}


static const NvOdmAudioInputPortCaps *
ACW8753GetInputPortCaps(
    NvU32 InputPortId)
{
    static const NvOdmAudioInputPortCaps s_InputPortCaps =
            {
                1,          // MaxNumberOfLineIn;
                2,          // MaxNumberOfMicIn;
                0,          // MaxNumberOfCdIn;
                0,          // MaxNumberOfVideoIn;
                0,          // MaxNumberOfMonoInput;
                1,          // MaxNumberOfOutput;
                NV_FALSE,   // IsSideToneAttSupported;
                NV_FALSE,   // IsNonLinerGainSupported;
                0,          // MaxVolumeInMilliBel;
                0           // MinVolumeInMilliBel;
            };

    if (InputPortId == 0)
        return &s_InputPortCaps;
    return NULL;
}

#if defined(MIC_DETECT_ENABLE)
// This function is used to set the Mic port which is enabled currently based
// on Mic detection routine
static void SetMicPort(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsHeadSetMicEnabled)
{
    NvBool IsSuccess = NV_TRUE;
    NvOdmAudioConfigInOutSignal AudioConfig = {0};
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    AudioConfig.InSignalType   = NvOdmAudioSignalType_MicIn;
    AudioConfig.InSignalId     = 1; //Mic2 - OnBoard
    AudioConfig.OutSignalType  = NvOdmAudioSignalType_Digital_I2s;
    AudioConfig.IsEnable       = hAudioCodec->IsMicInUse;

    if (hAudioCodec->IsHeadSetMicEnabled)
        AudioConfig.InSignalId = 0; //Mic1 - HeadsetMic

    hAudioCodec->MicPortIndex  = AudioConfig.InSignalId;

    if (hAudioCodec->IsMicInUse)
    {
        // SetPowerState
        IsSuccess = SetMicInPower(hOdmAudioCodec, AudioConfig.InSignalType,
                                            AudioConfig.InSignalId, NV_TRUE);
         // SetInputPortIO
        if (IsSuccess)
        IsSuccess = SetMicInput(hOdmAudioCodec,  AudioConfig.InSignalId, (void*)&AudioConfig);
    }
    return;
}
#endif


static void HandleMicDetectInterrupt(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvU32 DevStatus)
{
#if defined(MIC_DETECT_ENABLE)
    NvU32 RegValue     = 0;
    NvBool IsSuccess   = NV_TRUE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    // Invert the MICDETPOL
    RegValue = hAudioCodec->WCodecRegVals[WCodecRegIndex_InterruptPolarity];
    if (DevStatus & WCODECINTSTATUS_MIC_DETECT)
    {
        NVODM_WM8753_LOG((WM8753_PREFIX "[HandleMicDetectInterrupt] HEADSET MIC Detected\n"));
        RegValue = W8753_SET_REG_DEF(INT_POLARITY, MICDETPOL, BELOWTHRESHOLD, RegValue);
        hAudioCodec->IsHeadSetMicEnabled = NV_TRUE;
    }
    else
    {
        NVODM_WM8753_LOG((WM8753_PREFIX "[HandleMicDetectInterrupt] NO HEADSET MIC\n"));
        RegValue = W8753_SET_REG_DEF(INT_POLARITY, MICDETPOL, ABOVETHRESHOLD, RegValue);
        hAudioCodec->IsHeadSetMicEnabled = NV_FALSE;
    }

    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_InterruptPolarity, RegValue);

    //Set the Mic stuff accordingly
    if (hAudioCodec->LastHeadSetMicMode != hAudioCodec->IsHeadSetMicEnabled)
        SetMicPort(hOdmAudioCodec, hAudioCodec->IsHeadSetMicEnabled);

    hAudioCodec->LastHeadSetMicMode = hAudioCodec->IsHeadSetMicEnabled;

    RegValue = hAudioCodec->WCodecRegVals[WCodecRegIndex_InterruptEnable];
    RegValue = W8753_SET_REG_DEF(INT_MASK, MICDETEN, ENABLE, RegValue);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_InterruptEnable, RegValue);

#endif //MIC_DETECT_ENABLE
}

static void HandleHeadPhoneInterrupt(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvU32 DevStatus)
{
    NvU32 RegValue     = 0;
    NvBool IsSuccess   = NV_TRUE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    // Invert the GPIO4IPOL
    RegValue = hAudioCodec->WCodecRegVals[WCodecRegIndex_InterruptPolarity];

    if (DevStatus & WCODECINTSTATUS_GPIO4)
    {
        // Gpio4 pin high - no headphone is connected
        // Enable the internal speaker
        RegValue = W8753_SET_REG_DEF(INT_POLARITY, GPIO4IPOL, INT_LOW, RegValue);
        if (hAudioCodec->IsBoardConcorde2)
        {
            // Gpio4 pin high - headphone is connected
            hAudioCodec->IsHeadPhoneEnabled = NV_TRUE;
            NVODM_WM8753_LOG((WM8753_PREFIX "[HandleHeadPhoneInterrupt] HP is enabled\n"));
        }
        else
        {
            hAudioCodec->IsHeadPhoneEnabled = NV_FALSE;
            NVODM_WM8753_LOG((WM8753_PREFIX "[HandleHeadPhoneInterrupt] Spkr is enabled\n"));
        }
    }
    else
    {
        // Gpio4 pin low - headphone is connected
        // Disable the internal speaker
        RegValue = W8753_SET_REG_DEF(INT_POLARITY, GPIO4IPOL, INT_HIGH, RegValue);
        if (hAudioCodec->IsBoardConcorde2)
        {
            // Gpio4 pin low - no headphone connected
            hAudioCodec->IsHeadPhoneEnabled = NV_FALSE;
            NVODM_WM8753_LOG((WM8753_PREFIX "[HandleHeadPhoneInterrupt] Spkr is enabled \n"));
        }
        else
        {
            hAudioCodec->IsHeadPhoneEnabled = NV_TRUE;
            NVODM_WM8753_LOG((WM8753_PREFIX "[HandleHeadPhoneInterrupt] HP is enabled \n"));
        }
    }

    if (hAudioCodec->LastHeadPhoneMode != hAudioCodec->IsHeadPhoneEnabled)
            IsSuccess = SelectOutputMode(hOdmAudioCodec, hAudioCodec->LastHeadPhoneMode, NV_FALSE);

    hAudioCodec->LastHeadPhoneMode = hAudioCodec->IsHeadPhoneEnabled;

    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_InterruptPolarity, RegValue);

    // Enable the GPIO4IEN - headphone interrupt
    RegValue = hAudioCodec->WCodecRegVals[WCodecRegIndex_InterruptEnable];
    RegValue = W8753_SET_REG_DEF(INT_MASK, GPIO4IEN, ENABLE, RegValue);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_InterruptEnable, RegValue);
}

static void AudioCodecDebounceThread(void *arg)
{
    NvOdmPrivAudioCodecHandle hOdmAudioCodec = (NvOdmPrivAudioCodecHandle)arg;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)arg;
    NvU32  DeviceStatus = 0;
    NvU32 AudioISRdebounceTimeMS = 5;

    if (!hAudioCodec) return;

    while (!hAudioCodec->CodecShutdown)
    {
        NVODM_WM8753_LOG((WM8753_PREFIX "[AudioCodecDebounceThread] Waiting for Debounce notification\n"));

        // Wait for the level to get settled after interrupt
        if (hAudioCodec->hDebounceSema)
            NvOdmOsSemaphoreWait(hAudioCodec->hDebounceSema);

        if (hAudioCodec->CodecShutdown) break;
        // Sleep
        NvOdmOsSleepMS(AudioISRdebounceTimeMS);

        if (hAudioCodec->IntStatus)
        {
            // Read the device Status [SR4] -
            DeviceStatus = RegisterReadStatus(hOdmAudioCodec,STATUS_REG_DEVICE_STATUS);
            NVODM_WM8753_LOG((WM8753_PREFIX "[AudioCodecDebounceThread] Device Status 0x%x\n", DeviceStatus));

            if (hAudioCodec->IntStatus & WCODECINTSTATUS_GPIO4)
            {
                NVODM_WM8753_LOG((WM8753_PREFIX "[AudioCodecDebounceThread] GPIO4 Interrupt: 0x%08x\n", DeviceStatus));
                HandleHeadPhoneInterrupt(hOdmAudioCodec, DeviceStatus);
            }

            if (hAudioCodec->IntStatus & WCODECINTSTATUS_MIC_DETECT)
            {
                NVODM_WM8753_LOG((WM8753_PREFIX "[AudioCodecDebounceThread] MIC_DETECT Interrupt: 0x%08x\n", DeviceStatus));
                HandleMicDetectInterrupt(hOdmAudioCodec, DeviceStatus);
            }
        }
    }
}


static void AudioCodec_GpioIsr(void *arg)
{
    NvOdmPrivAudioCodecHandle hOdmAudioCodec = (NvOdmPrivAudioCodecHandle)arg;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)arg;
    NvU32 RegValue = 0;

    if (!hAudioCodec) return;

    hAudioCodec->IntStatus = 0;

    hAudioCodec->IntStatus = RegisterReadStatus(hOdmAudioCodec, STATUS_REG_INT_STATUS);
    NVODM_WM8753_LOG((WM8753_PREFIX "[AudioCodec_GpioIsr] Int Status from ISR 0x%x\n", hAudioCodec->IntStatus));

    // Clear the GPIO4IEN - Disable Interrupt will be enable by Debounce Thread
    RegValue  = hAudioCodec->WCodecRegVals[WCodecRegIndex_InterruptEnable];
    if (hAudioCodec->IntStatus & WCODECINTSTATUS_GPIO4)
    {
        NVODM_WM8753_LOG((WM8753_PREFIX "[AudioCodec_GpioIsr] Clear HP Interrupt\n"));
        RegValue  = W8753_SET_REG_DEF(INT_MASK, GPIO4IEN, DISABLE, RegValue);

    }
#if defined(MIC_DETECT_ENABLE)
    // Clear MICDETEN
    if (hAudioCodec->IntStatus & WCODECINTSTATUS_MIC_DETECT)
    {
        NVODM_WM8753_LOG((WM8753_PREFIX "[AudioCodec_GpioIsr] Clear MIC Interrupt\n"));
        RegValue  = W8753_SET_REG_DEF(INT_MASK, MICDETEN, DISABLE, RegValue);
    }
#endif

    W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_InterruptEnable, RegValue);

    // Signal the debounce thread
    if (hAudioCodec->hDebounceSema)
        NvOdmOsSemaphoreSignal(hAudioCodec->hDebounceSema);

    if (hAudioCodec->hGpioIntr)
        NvOdmGpioInterruptDone(hAudioCodec->hGpioIntr);
}


static NvBool InitGpioIntRegisters(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    NvU32 RegValue     = 0;
    NvBool IsSuccess   = NV_TRUE;

    // Write to  R27 bit 8&7
    RegValue = hAudioCodec->WCodecRegVals[WCodecRegIndex_GpioControl1];
    RegValue = W8753_SET_REG_DEF(GPIO_CONTROL1, INTCON, ACTIVE_LOW, RegValue);
    IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_GpioControl1, RegValue);

    // Before setting the R28 make sure GP2CLK2SEL = 0
    // Write the R28 GP2m[2:0] as 0x11
    RegValue = hAudioCodec->WCodecRegVals[WCodecRegIndex_GpioControl2];
    RegValue = W8753_SET_REG_DEF(GPIO_CONTROL2, GPIO2M, INTERRUPT, RegValue);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_GpioControl2, RegValue);

    // Enable GPIO4IPOL
    // Set R25.4
    RegValue = hAudioCodec->WCodecRegVals[WCodecRegIndex_InterruptPolarity];
    if (hAudioCodec->IsBoardConcorde2)
        RegValue = W8753_SET_REG_DEF(INT_POLARITY, GPIO4IPOL, INT_HIGH, RegValue);
    else
        RegValue = W8753_SET_REG_DEF(INT_POLARITY, GPIO4IPOL, INT_LOW, RegValue);
#if defined(MIC_DETECT_ENABLE)
    // Set R25.1 MICDETPOL
    RegValue = W8753_SET_REG_DEF(INT_POLARITY, MICDETPOL, ABOVETHRESHOLD, RegValue);
#endif

    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_InterruptPolarity, RegValue);

#if defined(MIC_DETECT_ENABLE)
    // Enable MBCEN for Mic detection comparator circuit and set  THRESHOLD value
    RegValue   = hAudioCodec->WCodecRegVals[WCodecRegIndex_MicBiasControl];
    RegValue   = W8753_SET_REG_DEF(MICBIAS_CONTROL, MBCEN, ENABLE, RegValue);
    RegValue   = W8753_SET_REG_DEF(MICBIAS_CONTROL, MBTHRESH, MIN, RegValue);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_MicBiasControl, RegValue);
#endif

#if defined(EAR_SPEAKER_ENABLE)
    // Setting the HPSWEN / HPSWPOL and ROUT2INV
    RegValue = hAudioCodec->WCodecRegVals[WCodecRegIndex_OutputControl];

    //RegValue = W8753_SET_REG_DEF(OUTPUT_CONTROL, HPSWEN, ENABLE, RegValue);
    // GPIO4 high means Speaker
    //RegValue = W8753_SET_REG_DEF(OUTPUT_CONTROL, HPSWPOL, HIGHSPKR, RegValue);
    // Set ROUT2INV
    RegValue = W8753_SET_REG_DEF(OUTPUT_CONTROL, ROUT2INV, ENABLE, RegValue);
    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_OutputControl, RegValue);
#endif

    return IsSuccess;
}


static NvBool EnableAudioInterrupt(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    NvU32 RegValue     = 0;
    NvBool IsSuccess   = NV_TRUE;
    /* can only be initialized once */
    if (hAudioCodec->hGpioIntr)
        return NV_FALSE;

    if (!hAudioCodec->hGpio) return NV_FALSE;

    // Setup the Interrupt Routine
    IsSuccess = NvOdmGpioInterruptRegister(hAudioCodec->hGpio, &hAudioCodec->hGpioIntr,
        hAudioCodec->hGpioPin, NvOdmGpioPinMode_InputInterruptLow, AudioCodec_GpioIsr,
        (void*)hOdmAudioCodec, 0);

    if (!IsSuccess || !hAudioCodec->hGpioIntr)
            IsSuccess = NV_FALSE;

    // Enabling the interrupt
    // Set 26.4  ( GPIO4IEN )
    RegValue = hAudioCodec->WCodecRegVals[WCodecRegIndex_InterruptEnable];
    RegValue = W8753_SET_REG_DEF(INT_MASK, GPIO4IEN, ENABLE, RegValue);
#if defined(MIC_DETECT_ENABLE)
     RegValue = W8753_SET_REG_DEF(INT_MASK, MICDETEN, ENABLE, RegValue);
#endif

    if (IsSuccess)
        IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_InterruptEnable, RegValue);

    return IsSuccess;
}


static NvBool SetRegisterOnPowerMode(
                 NvOdmPrivAudioCodecHandle hOdmAudioCodec,
                 NvOdmAudioCodecPowerMode  PowerMode)
{
    NvBool IsSuccess = NV_TRUE;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    if (NvOdmAudioCodecPowerMode_Normal == PowerMode)
    {
        if (hAudioCodec->IsVoiceCodecInUse)
        {
            IsSuccess = SetDacVolume(hOdmAudioCodec, NV_FALSE);
        }
    }
    else
    {
        if (hAudioCodec->IsVoiceCodecInUse)
        {
            IsSuccess = SetDacVolume(hOdmAudioCodec, NV_TRUE);
        }
    }
    return IsSuccess;
}

// Thread that can handle various event that need some timing
// Speaker is shutoff while no audio is running for 30sec , if Speaker mode is enabled
static void
WolfsonCodec8753EventThread(void *arg)
{
    NvOdmPrivAudioCodecHandle hOdmAudioCodec = (NvOdmPrivAudioCodecHandle)arg;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)arg;

    if (!hAudioCodec) return;

    while (!hAudioCodec->CodecShutdown)
    {
        NvOdmOsSemaphoreWait(hAudioCodec->hEventSema);

        if (hAudioCodec->CodecShutdown) break;

        if (!hAudioCodec->IsSpkrAmpEnabled)
            continue;

        if (!hAudioCodec->CodecInUseCount)
            // Sleep for 30 secs
            NvOdmOsSemaphoreWaitTimeout(hAudioCodec->hSettlingSema, 30000);

        if (!hAudioCodec->CodecInUseCount)
        {
            // shutdown speaker mode.
            SetGPIO3Register(hOdmAudioCodec, NV_FALSE);
        }
    } // While loop
}

static void AudioCodecGpioStartThread(void *arg)
{
    NvOdmPrivAudioCodecHandle hOdmAudioCodec = (NvOdmPrivAudioCodecHandle)arg;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)arg;

    if (!hAudioCodec) return;
    // Wait for 1sec before proceeding to get the Interrupt Levels to  get
    // settled after codec init.
    NvOdmOsSleepMS(1000);
    NVODM_WM8753_LOG((WM8753_PREFIX "[AudioCodecGpioStartThread] Enabling Gpio for codec \n"));
    if (!hAudioCodec->CodecShutdown)
        EnableAudioInterrupt(hOdmAudioCodec);
}


static void ACW8753Close(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    NVODM_WM8753_LOG((WM8753_PREFIX "[++Close]\n"));
    if (hOdmAudioCodec != NULL)
    {
        hAudioCodec->CodecShutdown = 1;
        ShutDownCodec(hOdmAudioCodec);
        if (hAudioCodec->hGpioIntr)
        {
            NvU32 RegValue  = hAudioCodec->WCodecRegVals[WCodecRegIndex_InterruptEnable];
            RegValue  = W8753_SET_REG_DEF(INT_MASK, GPIO4IEN, DISABLE, RegValue);
            W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_InterruptEnable, RegValue);
        }
        (void)SetPowerOffCodec(hOdmAudioCodec);

        if (hAudioCodec->hGpioThread)
        {
            NvOdmOsThreadJoin(hAudioCodec->hGpioThread);
            hAudioCodec->hGpioThread = 0;
        }

        if (hAudioCodec->hEventThread)
        {
            if (hAudioCodec->hSettlingSema)
                NvOdmOsSemaphoreSignal(hAudioCodec->hSettlingSema);
            if (hAudioCodec->hEventSema)
                NvOdmOsSemaphoreSignal(hAudioCodec->hEventSema);
            NvOdmOsThreadJoin(hAudioCodec->hEventThread);
            hAudioCodec->hEventThread = 0;
        }

        if (hAudioCodec->hEventSema)
        {
            NvOdmOsSemaphoreDestroy(hAudioCodec->hEventSema);
            hAudioCodec->hEventSema = 0;
        }

        if (hAudioCodec->hSettlingSema)
        {
            NvOdmOsSemaphoreDestroy(hAudioCodec->hSettlingSema);
            hAudioCodec->hSettlingSema = 0;
        }

        if (hAudioCodec->hDebounceThread)
        {
             if (hAudioCodec->hDebounceSema)
                NvOdmOsSemaphoreSignal(hAudioCodec->hDebounceSema);
            NvOdmOsThreadJoin(hAudioCodec->hDebounceThread);
            hAudioCodec->hDebounceThread = 0;
        }
        if (hAudioCodec->hDebounceSema)
        {
            NvOdmOsSemaphoreDestroy(hAudioCodec->hDebounceSema);
            hAudioCodec->hDebounceSema = 0;
        }

        CloseOdmServiceHandle(hOdmAudioCodec);
    }

    NVODM_WM8753_LOG((WM8753_PREFIX "[--Close]\n"));
}

static NvBool ACW8753Open(NvOdmPrivAudioCodecHandle hOdmAudioCodec,
                        const NvOdmQueryI2sACodecInterfaceProp *pI2sCodecInt,
                        void* hCodecNotifySem )
{
    NvBool IsSuccess  = NV_TRUE;
    NvU8 pRxBuffer[5] = {0};
    NvU32 Index;
    NvOdmBoardInfo BoardInfo;
    W8753AudioCodecHandle hNewACodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    hNewACodec->hOdmSpiService = NULL;
    hNewACodec->hOdmServicePmuDevice = NULL;

    // Codec interface paramater
    hNewACodec->WCodecInterface.IsCodecMasterMode = pI2sCodecInt->IsCodecMasterMode;
    hNewACodec->WCodecInterface.IsUsbMode = pI2sCodecInt->IsUsbMode;
    hNewACodec->WCodecInterface.I2sCodecDataCommFormat = pI2sCodecInt->I2sCodecDataCommFormat;
    hNewACodec->WCodecInterface.I2sCodecLRLineControl = pI2sCodecInt->I2sCodecLRLineControl;

    // Codec control parameter
    hNewACodec->WCodecControl.LineInVolLeft  = 0;
    hNewACodec->WCodecControl.LineInVolRight = 0;
    hNewACodec->WCodecControl.HPOut1VolLeft  = 0;
    hNewACodec->WCodecControl.HPOut2VolRight = 0;
    hNewACodec->WCodecControl.HPOut2VolLeft  = 0;
    hNewACodec->WCodecControl.HPOut1VolRight = 0;
    hNewACodec->WCodecControl.AdcSamplingRate = 0;
    hNewACodec->WCodecControl.DacSamplingRate = 0;
    hNewACodec->MCLKFreq = 0;
    hNewACodec->WCodecControl.MicInVolLeft  = 0x11F; //0db
    hNewACodec->WCodecControl.MicInVolRight = 0x11F; //0db
    hNewACodec->hIntSema = (NvOdmOsSemaphoreHandle)hCodecNotifySem;

    // Set all register values to reset and initailize the addresses
    for (Index = 0; Index < WCodecRegIndex_Max; ++Index)
        hNewACodec->WCodecRegVals[Index] = s_W8753_ResetVal[Index];

    IsSuccess = SetPowerOnCodec(hOdmAudioCodec, pI2sCodecInt->DapPortIndex);
    if (!IsSuccess)
        goto ErrorExit;

    // Scan for whistler boards
    for (Index=0; Index<NV_ARRAY_SIZE(gs_WhistlerBoards); Index++)
    {
        if (NvOdmPeripheralGetBoardInfo(gs_WhistlerBoards[Index], &BoardInfo) )
        {
            hNewACodec->IsBoardWhistler = NV_TRUE;
            break;
        }
    }

    IsSuccess = OpenOdmServiceHandle(hOdmAudioCodec);
    if (!IsSuccess)
        goto ErrorExit;

    // Create the semaphore for notifying events
    hNewACodec->hEventSema   = NvOdmOsSemaphoreCreate(0);
    if (!hNewACodec->hEventSema)
        goto ErrorExit;

    hNewACodec->hSettlingSema   = NvOdmOsSemaphoreCreate(0);
    if (!hNewACodec->hSettlingSema)
        goto ErrorExit;

    // Create the thread to handle the events
    hNewACodec->hEventThread =
        NvOdmOsThreadCreate((NvOdmOsThreadFunction)WolfsonCodec8753EventThread,
                            (void*)hOdmAudioCodec);
    if (!hNewACodec->hEventThread)
        goto ErrorExit;

    if (hNewACodec->hGpio)
    {
        hNewACodec->hDebounceSema = NvOdmOsSemaphoreCreate(0);
        if (!hNewACodec->hDebounceSema)
            goto ErrorExit;

        hNewACodec->hDebounceThread =
            NvOdmOsThreadCreate((NvOdmOsThreadFunction)AudioCodecDebounceThread,
                                (void*)hOdmAudioCodec);
        if (!hNewACodec->hDebounceThread)
        {
            goto ErrorExit;
        }
    }

    IsSuccess = OpenWolfsonCodec(hOdmAudioCodec);

    if (IsSuccess)
        SetReadEnable(hOdmAudioCodec,STATUS_REG_DEVICE_LO, pRxBuffer);

    // Check whether the Board is Concorde2
    // Gpio Interrupt state are toggled for Concorde2

    if (!hNewACodec->IsBoardWhistler)
        hNewACodec->IsBoardConcorde2 = NvOdmPeripheralGetBoardInfo(CONCORDEII_BOARDID, &BoardInfo);

    NVODM_WM8753_LOG((WM8753_PREFIX "[Open] Detected Board E%d%d SKU %d Fab %d Rev %d as Concorde2 %d Whistler %d \n",
                      (BoardInfo.BoardID >> 8) & 0xFF, BoardInfo.BoardID & 0x00FF,
                      BoardInfo.SKU, BoardInfo.Fab, BoardInfo.Revision,
                      hNewACodec->IsBoardConcorde2, hNewACodec->IsBoardWhistler));



    // Register Gpio if we have pin& port set for gpio
    if (hNewACodec->hGpio)
    {
        // Thread to enable the ISR
        hNewACodec->hGpioThread =
            NvOdmOsThreadCreate((NvOdmOsThreadFunction)AudioCodecGpioStartThread,
                                (void*)hOdmAudioCodec);
        if (!hNewACodec->hGpioThread)
            goto ErrorExit;

        // Init Gpio Interrupt Registers
        IsSuccess = InitGpioIntRegisters(hOdmAudioCodec);
        if (!IsSuccess)
            goto ErrorExit;
    }

    // Enable the stereo spkr
    if (!hNewACodec->IsHeadPhoneEnabled)
    {
        hNewACodec->CodecInUseCount++;
        IsSuccess = SelectOutputMode(hOdmAudioCodec, NV_TRUE, NV_FALSE);
        hNewACodec->LastHeadPhoneMode  = NV_FALSE;
        hNewACodec->LastHeadSetMicMode = NV_FALSE;
    }

    return NV_TRUE;

ErrorExit:
    ACW8753Close(hOdmAudioCodec);
    return NV_FALSE;
}

static const NvOdmAudioWave *
ACW8753GetPortPcmCaps(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortName PortName,
    NvU32 *pValidListCount)
{
    static const NvOdmAudioWave s_AudioPcmProps[] =
    {
        // NumberOfChannels
        // IsSignedData
        // IsLittleEndian
        // IsInterleaved
        // NumberOfBitsPerSample
        // SamplingRateInHz
        // DatabitsPerLRCLK
        // DataFormat
        // NvOdmAudioWaveFormat FormatType;
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 16, 8000,  32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 16, 32000, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 16, 44100, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 16, 48000, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 16, 88200, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 16, 96000, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
    };

    *pValidListCount = sizeof(s_AudioPcmProps)/sizeof(s_AudioPcmProps[0]);
    return &s_AudioPcmProps[0];
}
static NvBool
ACW8753SetPortPcmProps(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortName PortName,
    NvOdmAudioWave *pWaveParams)
{
    NvBool IsSuccess;
    NvOdmAudioPortType PortType;
    NvU32 PortId;

    PortType = NVODMAUDIO_GET_PORT_TYPE(PortName);
    PortId = NVODMAUDIO_GET_PORT_ID(PortName);

    if ((PortType == NvOdmAudioPortType_Input) ||
        (PortType == NvOdmAudioPortType_Output))
    {
        IsSuccess = SetPcmSize(hOdmAudioCodec, PortId, pWaveParams->NumberOfBitsPerSample);
        if (IsSuccess && (PortId == 0))
            IsSuccess = SetSamplingRate(hOdmAudioCodec, PortType, pWaveParams->SamplingRateInHz);
        if (IsSuccess && (PortId == 0))
            IsSuccess = SetDataFormat(hOdmAudioCodec, PortId, pWaveParams->DataFormat);
        if (IsSuccess)
            IsSuccess = SetAdcDataFormat(hOdmAudioCodec, PortId, pWaveParams->DataFormat, pWaveParams->NumberOfChannels);
        return IsSuccess;
    }

    return NV_FALSE;
}


static void
ACW8753SetVolume(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortName PortName,
    NvOdmAudioVolume *pVolume,
    NvU32 ListCount)
{
    //NvU32 PortId;
    NvU32 Index;

    //PortId = NVODMAUDIO_GET_PORT_ID(PortName);

    for (Index = 0; Index < ListCount; ++Index)
    {
        if (pVolume[Index].SignalType ==  NvOdmAudioSignalType_HeadphoneOut)
        {
            SetHeadphoneOutVolume(hOdmAudioCodec, pVolume[Index].ChannelMask, 0, pVolume[Index].IsLinear, pVolume[Index].VolumeLevel);
        }
        else if (pVolume[Index].SignalType ==  NvOdmAudioSignalType_LineIn)
        {
            SetLineInVolume(hOdmAudioCodec, pVolume[Index].ChannelMask, 0, pVolume[Index].IsLinear, pVolume[Index].VolumeLevel);
        }
        else if (pVolume[Index].SignalType ==  NvOdmAudioSignalType_MicIn)
        {
            SetPGAInputVolume(hOdmAudioCodec, pVolume[Index].ChannelMask, 0, pVolume[Index].IsLinear, pVolume[Index].VolumeLevel);
        }
    }
}


static void
ACW8753SetMuteControl(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortName PortName,
    NvOdmAudioMuteData *pMuteParam,
    NvU32 ListCount)
{
    NvU32 PortId;
    NvU32 Index;

    PortId = NVODMAUDIO_GET_PORT_ID(PortName);
    if (PortId != 0)
        return;

    NVODM_WM8753_LOG((WM8753_PREFIX "[SetMuteControl] IsMute:%d \n", pMuteParam->IsMute));

    for (Index = 0; Index < ListCount; ++Index)
    {
        if (pMuteParam->SignalType ==  NvOdmAudioSignalType_HeadphoneOut)
        {
            SetHeadphoneOutMute(hOdmAudioCodec, pMuteParam->ChannelMask, 0,
                        pMuteParam->IsMute);
        }
        else if (pMuteParam->SignalType ==  NvOdmAudioSignalType_LineIn)
        {
            SetLineInMute(hOdmAudioCodec, pMuteParam->ChannelMask,
                        pMuteParam->IsMute);
        }
        else if (pMuteParam->SignalType ==  NvOdmAudioSignalType_MicIn)
        {
            SetMicrophoneInMute(hOdmAudioCodec, pMuteParam->IsMute);
        }
        else if (pMuteParam->SignalType ==  NvOdmAudioSignalType_Speakers)
        {
            SelectOutputMode(hOdmAudioCodec, pMuteParam->IsMute, NV_TRUE);
        }
    }
}

static NvBool
ACW8753SetConfiguration(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortName PortName,
    NvOdmAudioConfigure ConfigType,
    void *pConfigData)
{
    NvBool IsSuccess = NV_TRUE;
    NvOdmAudioPortType PortType;
    NvU32 PortId;
    NvOdmAudioConfigBypass *pBypassConfig = pConfigData;
    NvOdmAudioConfigSideToneAttn *pSideToneAttn = pConfigData;

    PortType = NVODMAUDIO_GET_PORT_TYPE(PortName);
    PortId = NVODMAUDIO_GET_PORT_ID(PortName);

    NVODM_WM8753_LOG((WM8753_PREFIX "++[SetConfiguration] ConfigType:%x: %s\n", ConfigType,
                      ConfigType == NvOdmAudioConfigure_Pcm ? "Pcm" :
                      ConfigType == NvOdmAudioConfigure_InOutSignal ? "InOutSignal" :
                      ConfigType == NvOdmAudioConfigure_ByPass ? "ByPass" :
                      ConfigType == NvOdmAudioConfigure_SideToneAtt ? "SideToneAtt" :
                      ConfigType == NvOdmAudioConfigure_None ? "None" :
                      ConfigType == NvOdmAudioConfigure_ShortCircuit ? "ShortCircuit" :
                      ConfigType == NvOdmAudioConfigure_ODM ? "ODM" :
                      ConfigType == NvOdmAudioConfigure_Vdac ? "Vdac" :
                      ConfigType == NvOdmAudioConfigure_InOutSignalCallback ? "InOutSignalCallback" : "OTHER"));

    if (ConfigType == NvOdmAudioConfigure_Pcm)
    {
        IsSuccess = ACW8753SetPortPcmProps(hOdmAudioCodec, PortName, pConfigData);
        goto EXIT;
    }

    if (ConfigType == NvOdmAudioConfigure_InOutSignal)
    {
        NVODM_WM8753_LOG((WM8753_PREFIX "[SetConfiguration] PortType:%s State:%s ist:%x isi:%x ost:%x osi:%x\n",
                          ((PortType & NvOdmAudioPortType_Input) ? "INPUT" :
                           (PortType & NvOdmAudioPortType_Output) ? "OUTPUT" : "OTHER"),
                          (((NvOdmAudioConfigInOutSignal*)pConfigData)->IsEnable ? "ENABLE" : "DISABLE"),
                          ((NvOdmAudioConfigInOutSignal*)pConfigData)->InSignalType,
                          ((NvOdmAudioConfigInOutSignal*)pConfigData)->InSignalId,
                          ((NvOdmAudioConfigInOutSignal*)pConfigData)->OutSignalType,
                          ((NvOdmAudioConfigInOutSignal*)pConfigData)->OutSignalId));

        if (PortType == NvOdmAudioPortType_Input)
        {
            IsSuccess = SetInputPortIO(hOdmAudioCodec, PortId, pConfigData);
            goto EXIT;
        }

        if (PortType == NvOdmAudioPortType_Output)
        {
            IsSuccess = SetOutputPortIO(hOdmAudioCodec, PortId, pConfigData);
            goto EXIT;
        }

        // Return default value
        goto EXIT;
    }

    if (ConfigType == NvOdmAudioConfigure_ByPass)
    {
        IsSuccess = SetBypass(hOdmAudioCodec, pBypassConfig->InSignalType,
            pBypassConfig->InSignalId, pBypassConfig->OutSignalType,
            pBypassConfig->OutSignalId, pBypassConfig->IsEnable);
        goto EXIT;
    }

    if (ConfigType == NvOdmAudioConfigure_SideToneAtt)
    {
        IsSuccess = SetSideToneAttenuation(hOdmAudioCodec, pSideToneAttn->SideToneAttnValue,
                            pSideToneAttn->IsEnable);
        goto EXIT;
    }

    // Expecting voice codec settings - this can be moved to SetOutputPortIO
    // with a different PortID
    if (ConfigType == NvOdmAudioConfigure_Vdac)
    {
        IsSuccess = SetVdacRegisters(hOdmAudioCodec, pConfigData);
        goto EXIT;
    }

EXIT:

    NVODM_WM8753_LOG((WM8753_PREFIX "--[SetConfiguration]"));
    return IsSuccess;
}

static void
ACW8753GetConfiguration(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortName PortName,
    NvOdmAudioConfigure ConfigType,
    void* pConfigData)
{
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;
    NvOdmAudioPortType PortType = NVODMAUDIO_GET_PORT_TYPE(PortName);
    NvU32 PortId = NVODMAUDIO_GET_PORT_ID(PortName);

    if (ConfigType == NvOdmAudioConfigure_InOutSignal)
    {
        NvOdmAudioConfigInOutSignal* pConfigIO = (NvOdmAudioConfigInOutSignal*)pConfigData;
        pConfigIO->IsEnable = NV_FALSE;
        if (PortType == NvOdmAudioPortType_Input)
        {
            if (pConfigIO->InSignalType == NvOdmAudioSignalType_LineIn)
            {
                pConfigIO->IsEnable = NV_TRUE;
            }

            else if ((pConfigIO->InSignalType == NvOdmAudioSignalType_MicIn) && (PortId == 1))
            {
                //
                // Set the default to the Headset Mic until auto detection at the start is enabled.
                //
                pConfigIO->IsEnable = NV_TRUE;
            }
        }

        if (PortType == NvOdmAudioPortType_Output)
        {
            if (pConfigIO->OutSignalType == NvOdmAudioSignalType_HeadphoneOut)
            {
                //
                // Set Headphone to the default until auto detection at the start is enabled.
                //
                hAudioCodec->IsHeadPhoneEnabled = NV_TRUE;
                pConfigIO->IsEnable = NV_TRUE;
            }
        }
    }
}

static void
ACW8753SetPowerState(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortName PortName,
    NvOdmAudioSignalType SignalType,
    NvU32 SignalId,
    NvBool IsPowerOn)
{
    NvU32 PortId;
    PortId = NVODMAUDIO_GET_PORT_ID(PortName);

    SetPowerStatus(hOdmAudioCodec, PortId, SignalType, SignalId, 0, IsPowerOn);
}

static void
ACW8753SetPowerMode(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioCodecPowerMode PowerMode)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 RegValue   = 0;
    W8753AudioCodecHandle hAudioCodec = (W8753AudioCodecHandle)hOdmAudioCodec;

    if (NvOdmAudioCodecPowerMode_Normal == PowerMode)
    {
        hAudioCodec->CodecInUseCount++;
        if (hAudioCodec->CodecInUseCount == 1)
        {
            if (!hAudioCodec->IsMCLKEnabled)
            {
                NVODM_WM8753_LOG((WM8753_PREFIX "[SetPowerMode] Codec Power On done \n"));
                IsSuccess = ConfigExternalClock(hOdmAudioCodec, NV_TRUE);

                if (IsSuccess)
                    hAudioCodec->IsMCLKEnabled = NV_TRUE;
            }

            NvOdmOsSemaphoreSignal(hAudioCodec->hEventSema);
            if (!hAudioCodec->IsHeadPhoneEnabled
    #if defined(EAR_SPEAKER_ENABLE)
                || (!hAudioCodec->IsEarSpkrEnabled && hAudioCodec->IsVoiceCodecInUse)
    #endif
                )
                SetGPIO3Register(hOdmAudioCodec, NV_TRUE);

            // Enable Gpio4 Interrupt bit
            if (hAudioCodec->hGpioIntr)
            {
                RegValue  = hAudioCodec->WCodecRegVals[WCodecRegIndex_InterruptEnable];
                RegValue  = W8753_SET_REG_DEF(INT_MASK, GPIO4IEN, ENABLE, RegValue);
                IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_InterruptEnable, RegValue);
            }
        }
        SetRegisterOnPowerMode(hOdmAudioCodec, PowerMode);
    }
    else
    {
        if (hAudioCodec->CodecInUseCount > 0)
            hAudioCodec->CodecInUseCount--;

        if (!hAudioCodec->CodecInUseCount)
        {
            // Disable Gpio4 Interrupt bit
            if (hAudioCodec->hGpioIntr)
            {
                RegValue  = hAudioCodec->WCodecRegVals[WCodecRegIndex_InterruptEnable];
                RegValue  = W8753_SET_REG_DEF(INT_MASK, GPIO4IEN, DISABLE, RegValue);
                IsSuccess = W8753_WriteRegister(hOdmAudioCodec, WCodecRegIndex_InterruptEnable, RegValue);
            }

            NvOdmOsSemaphoreSignal(hAudioCodec->hEventSema);
            SetRegisterOnPowerMode(hOdmAudioCodec, PowerMode);

            // Power off the register and save the reg values
            if (hAudioCodec->IsMCLKEnabled && (!hAudioCodec->IsVoiceCodecInUse))
            {
                IsSuccess = ConfigExternalClock(hOdmAudioCodec, NV_FALSE);
                if (IsSuccess)
                    hAudioCodec->IsMCLKEnabled = NV_FALSE;
                NVODM_WM8753_LOG((WM8753_PREFIX "[SetPowerMode] Codec Power Off done \n"));
            }
        }
    }
}

static void
ACWM8753SetOperationMode(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortName PortName,
    NvBool IsMaster)
{
    NvU32 PortId;

    PortId = NVODMAUDIO_GET_PORT_ID(PortName);
    //SetDacMute(hOdmAudioCodec, NV_TRUE);
    SetOperationMode(hOdmAudioCodec, PortId, IsMaster);
    //SetDacMute(hOdmAudioCodec, NV_FALSE);
}

void W8753InitCodecInterface(NvOdmAudioCodec *pCodecInterface)
{
    NvOdmOsMemset(&s_W8753, 0, sizeof(W8753AudioCodec));

    pCodecInterface->pfnGetPortCaps = ACW8753GetPortCaps;
    pCodecInterface->pfnGetOutputPortCaps = ACW8753GetOutputPortCaps;
    pCodecInterface->pfnGetInputPortCaps = ACW8753GetInputPortCaps;
    pCodecInterface->pfnGetPortPcmCaps = ACW8753GetPortPcmCaps;
    pCodecInterface->pfnOpen = ACW8753Open;
    pCodecInterface->pfnClose = ACW8753Close;
    pCodecInterface->pfnSetVolume = ACW8753SetVolume;
    pCodecInterface->pfnSetMuteControl = ACW8753SetMuteControl;
    pCodecInterface->pfnSetConfiguration = ACW8753SetConfiguration;
    pCodecInterface->pfnGetConfiguration = ACW8753GetConfiguration;
    pCodecInterface->pfnSetPowerState = ACW8753SetPowerState;
    pCodecInterface->pfnSetPowerMode = ACW8753SetPowerMode;
    pCodecInterface->pfnSetOperationMode = ACWM8753SetOperationMode;

    pCodecInterface->hCodecPrivate = &s_W8753;
    pCodecInterface->IsInited = NV_TRUE;
}

