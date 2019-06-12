/*
 * Copyright (c) 2009-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA APX ODM Kit:
 *         Implementation of the ODM Query API</b>
 *
 * @b Description: Implements the query functions for ODMs that may be
 *                 accessed at boot-time, runtime, or anywhere in between.
 */

#include "nvodm_query.h"
#include "nvodm_query_gpio.h"
#include "nvodm_query_memc.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query_pins.h"
#include "nvodm_query_pins_t30.h"
#include "nvrm_pinmux.h"
#include "nvrm_drf.h"
#include "tegra_devkit_custopt.h"
#include "nvodm_keylist_reserved.h"

NvU32 GetBctKeyValue(void);

#if NV_DEBUG
static NvOdmDebugConsole
s_NvOdmQueryDebugConsoleSetting = NvOdmDebugConsole_Dcc;
#else
static NvOdmDebugConsole
s_NvOdmQueryDebugConsoleSetting = NvOdmDebugConsole_None;
#endif

static const NvU8
s_NvOdmQueryDeviceNamePrefixValue[] = {'T','e','g','r','a',0};

static const NvU8
s_NvOdmQueryManufacturerSetting[] = {'N','V','I','D','I','A',0};

static const NvU8
s_NvOdmQueryModelSetting[] = {'T','3','0',0};

static const NvU8
s_NvOdmQueryPlatformSetting[] = {'P','1','8','5','2',0};

static const NvU8
s_NvOdmQueryProjectNameSetting[] = {'O','D','M',' ','K','i','t',0};

static const NvOdmDownloadTransport
s_NvOdmQueryDownloadTransportSetting = NvOdmDownloadTransport_SpiEthernet;

static const NvOdmQuerySdioInterfaceProperty s_NvOdmQuerySdioInterfaceProperty[] =
{
    // Trim Value is dummy here. Not setting this for t30.
    { NV_FALSE, 10, NV_FALSE,  6, 0, NvOdmQuerySdioSlotUsage_unused},
    { NV_FALSE,  10, NV_TRUE,  15, 0, NvOdmQuerySdioSlotUsage_wlan},
    { NV_TRUE, 10, NV_FALSE,  4, 0, NvOdmQuerySdioSlotUsage_Media},
    { NV_FALSE,  10, NV_TRUE,   6, 0, NvOdmQuerySdioSlotUsage_Boot},
};

static const NvOdmQueryOwrDeviceInfo s_NvOdmQueryOwrInfo = {
    NV_TRUE,
    0x1, /* Tsu */
    0xF, /* TRelease */
    0xF,  /* TRdv */
    0X3C, /* TLow0 */
    0x1, /* TLow1 */
    0x77, /* TSlot */

    0x78, /* TPdl */
    0x1E, /* TPdh */
    0x1DF, /* TRstl */
    0x1DF, /* TRsth */

    0x1E0, /* Tpp */
    0x5, /* Tfp */
    0x5, /* Trp */
    0x5, /* Tdv */
    0x5, /* Tpd */

    0x7, /* Read data sample clk */
    0x50, /* Presence sample clk */
    2,  /* Memory address size */
    0,  /* MemorySize */
};

#ifdef SET_KERNEL_PINMUX
static const NvU32 s_NvOdmClockLimit_Sdio[] = {
    26500,
    26500,
    26500,
    50000,
};

NvError
NvOdmQueryModuleInterfaceCaps(
    NvOdmIoModule Module,
    NvU32 Instance,
    void *pCaps)
{
    switch (Module)
    {
        case NvOdmIoModule_Sdio:
            if (Instance == 0)
                ((NvRmModuleSdmmcInterfaceCaps *)pCaps)->MmcInterfaceWidth = 4;
            else if ((Instance == 1) || (Instance == 2) || (Instance == 3))
                ((NvRmModuleSdmmcInterfaceCaps *)pCaps)->MmcInterfaceWidth = 8;
            else
                return NvError_NotSupported;
            return NvSuccess;

        case NvOdmIoModule_Nand:
            if (Instance == 0)
            {
                ((NvRmModuleNandInterfaceCaps*)pCaps)->IsCombRbsyMode = NV_TRUE;
                ((NvRmModuleNandInterfaceCaps*)pCaps)->NandInterfaceWidth = 8;
            }
            else
                return NvError_NotSupported;
            return NvSuccess;

        case NvOdmIoModule_Uart:
            if (Instance == 0)
                ((NvRmModuleUartInterfaceCaps *)pCaps)->NumberOfInterfaceLines = 8;
            else if ((Instance == 1) || (Instance == 2) || (Instance == 4))
                ((NvRmModuleUartInterfaceCaps *)pCaps)->NumberOfInterfaceLines = 0;
            else if (Instance == 3)
                ((NvRmModuleUartInterfaceCaps *)pCaps)->NumberOfInterfaceLines = 4;
            else
                return NvError_NotSupported;
            return NvSuccess;

            default:
                break;
    }
    return NvError_NotSupported;
}

void
NvOdmQueryClockLimits(
    NvOdmIoModule IoModule,
    const NvU32 **pClockSpeedLimits,
    NvU32 *pCount)
{

    switch(IoModule)
    {
        case NvOdmIoModule_Sdio:
            *pClockSpeedLimits = s_NvOdmClockLimit_Sdio;
            *pCount = NV_ARRAY_SIZE(s_NvOdmClockLimit_Sdio);
            break;

        default:
        *pClockSpeedLimits = NULL;
        *pCount = 0;
        break;
    }
}
#endif

/* --- Function Implementations ---*/

NvU32
GetBctKeyValue(void)
{
    NvOdmServicesKeyListHandle hKeyList = NULL;
    NvU32 BctCustOpt = 0;

    hKeyList = NvOdmServicesKeyListOpen();
    if (hKeyList)
    {
        BctCustOpt =
            NvOdmServicesGetKeyValue(hKeyList,
                                     NvOdmKeyListId_ReservedBctCustomerOption);
        NvOdmServicesKeyListClose(hKeyList);
    }

    return BctCustOpt;
}

NvOdmDebugConsole
NvOdmQueryDebugConsole(void)
{
    NvU32 CustOpt = GetBctKeyValue();
    switch (NV_DRF_VAL(TEGRA_DEVKIT, BCT_CUSTOPT, CONSOLE, CustOpt))
    {
    case TEGRA_DEVKIT_BCT_CUSTOPT_0_CONSOLE_NONE:
#ifdef DEFAULT_CONSOLE
        return s_NvOdmQueryDebugConsoleSetting;
#else
        return NvOdmDebugConsole_None;
#endif
    case TEGRA_DEVKIT_BCT_CUSTOPT_0_CONSOLE_DCC:
        return NvOdmDebugConsole_Dcc;
    case TEGRA_DEVKIT_BCT_CUSTOPT_0_CONSOLE_UART:
        return NvOdmDebugConsole_UartA +
            NV_DRF_VAL(TEGRA_DEVKIT, BCT_CUSTOPT, CONSOLE_OPTION, CustOpt);
    case TEGRA_DEVKIT_BCT_CUSTOPT_0_CONSOLE_AUTOMATION:
        return NvOdmDebugConsole_Automation;
    default:
        return s_NvOdmQueryDebugConsoleSetting;
    }
}

NvBool
NvOdmQueryUartOverSDEnabled(void)
{
    return NV_FALSE;
}

NvOdmDownloadTransport
NvOdmQueryDownloadTransport(void)
{
    return s_NvOdmQueryDownloadTransportSetting;
}

const NvU8*
NvOdmQueryDeviceNamePrefix(void)
{
    return s_NvOdmQueryDeviceNamePrefixValue;
}

static const NvOdmQuerySpdifInterfaceProperty s_NvOdmQuerySpdifInterfacePropertySetting =
{
    NvOdmQuerySpdifDataCaptureControl_FromLeft
};

const NvOdmQuerySpiDeviceInfo *
NvOdmQuerySpiGetDeviceInfo(
    NvOdmIoModule OdmIoModule,
    NvU32 ControllerId,
    NvU32 ChipSelect)
{
    static const NvOdmQuerySpiDeviceInfo s_OdmDefaultSpiInfo =
        {NvOdmQuerySpiSignalMode_0, NV_TRUE, NV_FALSE, 0, 0, 0};

    if (OdmIoModule == NvOdmIoModule_Spi)
    {
        if ((ControllerId == 0) && (ChipSelect == 0))
             return &s_OdmDefaultSpiInfo;

        if ((ControllerId == 1) && ((ChipSelect == 0) || (ChipSelect == 1) ||
                                    (ChipSelect == 2) || (ChipSelect == 3)))
             return &s_OdmDefaultSpiInfo;

        if ((ControllerId == 2) && ((ChipSelect == 0) || (ChipSelect == 1)))
             return &s_OdmDefaultSpiInfo;

        if ((ControllerId == 3) && ((ChipSelect == 0) || (ChipSelect == 1)))
             return &s_OdmDefaultSpiInfo;

        if ((ControllerId == 4) && ((ChipSelect == 2) || (ChipSelect == 3)))
             return &s_OdmDefaultSpiInfo;

        if ((ControllerId == 5) && (ChipSelect == 0))
             return &s_OdmDefaultSpiInfo;
    }

    return NULL;
}


/* Spi idle signal state */
static const NvOdmQuerySpiIdleSignalState s_NvOdmQuerySpiIdleSignalStateLevel[] =
{
    {NV_TRUE, NvOdmQuerySpiSignalMode_0, NV_FALSE},
    {NV_TRUE, NvOdmQuerySpiSignalMode_0, NV_FALSE},
    {NV_TRUE, NvOdmQuerySpiSignalMode_0, NV_FALSE},
    {NV_TRUE, NvOdmQuerySpiSignalMode_0, NV_FALSE},
    {NV_TRUE, NvOdmQuerySpiSignalMode_0, NV_FALSE},
    {NV_TRUE, NvOdmQuerySpiSignalMode_0, NV_FALSE}
};

const NvOdmQuerySpiIdleSignalState *
NvOdmQuerySpiGetIdleSignalState(
    NvOdmIoModule OdmIoModule,
    NvU32 ControllerId)
{
    if (OdmIoModule == NvOdmIoModule_Spi)
    {
        if (ControllerId <= 5)
            return &s_NvOdmQuerySpiIdleSignalStateLevel[ControllerId];
    }

    return NULL;
}

const NvOdmQueryI2sInterfaceProperty *
NvOdmQueryI2sGetInterfaceProperty(
    NvU32 I2sInstanceId)
{
    static const NvOdmQueryI2sInterfaceProperty s_Property =
    {
        NvOdmQueryI2sMode_Slave,
        NvOdmQueryI2sLRLineControl_LeftOnLow,
        NvOdmQueryI2sDataCommFormat_I2S,
        NV_FALSE,
        0
    };

    if ((!I2sInstanceId) || (I2sInstanceId == 1))
        return &s_Property;

    return NULL;
}

const NvOdmQueryDapPortProperty *
NvOdmQueryDapPortGetProperty(
    NvU32 DapPortId)
{
    static const NvOdmQueryDapPortProperty s_Property[] =
    {
        { NvOdmDapPort_None, NvOdmDapPort_None, { 0, 0, 0, 0 } },
        // I2S1 (DAC1) <-> DAP1 <-> HIFICODEC
        { NvOdmDapPort_I2s1, NvOdmDapPort_HifiCodecType,
          { 2, 16, 44100, NvOdmQueryI2sDataCommFormat_I2S } }, // Dap1
          // I2S2 (DAC2) <-> DAP2 <-> VOICECODEC
        {NvOdmDapPort_I2s2, NvOdmDapPort_VoiceCodecType,
          {2, 16, 8000, NvOdmQueryI2sDataCommFormat_I2S } },   // Dap2
    };
    if (DapPortId && DapPortId<NV_ARRAY_SIZE(s_Property))
        return &s_Property[DapPortId];
    return NULL;
}

const NvOdmQueryDapPortConnection*
NvOdmQueryDapPortGetConnectionTable(
    NvU32 ConnectionIndex)
{
    static const NvOdmQueryDapPortConnection s_Property[] =
    {
        { NvOdmDapConnectionIndex_Music_Path,
          2, { {NvOdmDapPort_I2s1, NvOdmDapPort_Dap1, NV_FALSE},
               {NvOdmDapPort_Dap1, NvOdmDapPort_I2s1, NV_TRUE} } },
    };
    NvU32 TableIndex   = 0;
    for( TableIndex = 0; TableIndex < NV_ARRAY_SIZE(s_Property); TableIndex++)
    {
        if (s_Property[TableIndex].UseIndex == ConnectionIndex)
            return &s_Property[TableIndex];
    }

    return NULL;
}

const NvOdmQuerySpdifInterfaceProperty *
NvOdmQuerySpdifGetInterfaceProperty(
    NvU32 SpdifInstanceId)
{
    if (SpdifInstanceId == 0)
        return &s_NvOdmQuerySpdifInterfacePropertySetting;

    return NULL;
}

const NvOdmQueryAc97InterfaceProperty *
NvOdmQueryAc97GetInterfaceProperty(
    NvU32 Ac97InstanceId)
{
    return NULL;
}

const NvOdmQueryI2sACodecInterfaceProp *
NvOdmQueryGetI2sACodecInterfaceProperty(
    NvU32 AudioCodecId)
{
    static const NvOdmQueryI2sACodecInterfaceProp s_Property =
    {
        NV_TRUE,
        0,
        0x34,
        NV_FALSE,
        NvOdmQueryI2sLRLineControl_LeftOnLow,
        NvOdmQueryI2sDataCommFormat_I2S
    };
    if (!AudioCodecId)
        return &s_Property;
    return NULL;
}

NvBool NvOdmQueryAsynchMemConfig(
    NvU32 ChipSelect,
    NvOdmAsynchMemConfig *pMemConfig)
{
    return NV_FALSE;
}

const void*
NvOdmQuerySdramControllerConfigGet(NvU32 *pEntries, NvU32 *pRevision)
{
    if (pEntries)
        *pEntries = 0;
    return NULL;
}

NvOdmQueryOscillator NvOdmQueryGetOscillatorSource(void)
{
    return NvOdmQueryOscillator_External;
}

NvU32 NvOdmQueryGetOscillatorDriveStrength(void)
{
    return 0x04;
}

const NvOdmWakeupPadInfo *NvOdmQueryGetWakeupPadTable(NvU32 *pSize)
{
    if (pSize)
        *pSize = 0;
    return NULL;
}

const NvU8* NvOdmQueryManufacturer(void)
{
    return s_NvOdmQueryManufacturerSetting;
}

const NvU8* NvOdmQueryModel(void)
{
    return s_NvOdmQueryModelSetting;
}

const NvU8* NvOdmQueryPlatform(void)
{
    return s_NvOdmQueryPlatformSetting;
}

const NvU8* NvOdmQueryProjectName(void)
{
    return s_NvOdmQueryProjectNameSetting;
}

#ifndef SET_KERNEL_PINMUX

#define PULLUPDOWN_NORMAL       0  // internal pull up-down to normal
#define PULLUPDOWN_PULLDOWN     1  // internal pull-down
#define PULLUPDOWN_PULLUP       2  // internal pull-up

#define BIT_VAL_POR             0   // keep bit untouch
#define BIT_VAL_0               2   // Set bit to 0
#define BIT_VAL_1               3   // Set bit to 1

#define HIGHSPEED_MODE_EN 1
#define SCHMITT_EN        1

// LP mode
#define LP_MODE_OHM_50    3
#define LP_MODE_OHM_100   2
#define LP_MODE_OHM_200   1
#define LP_MODE_OHM_400   0

// CAL_DRVDN(4 bits) - drive down (falling edge) - Driver Output Pull-Down drive
//             strength code. (4 bits)
// CAL_DRVUP(4 bits) - drive up (rising edge)  - Driver Output Pull-Up drive
//             strength code. (4 bit)
// works with combination of LMPD bits. for lower power modes, higher drive
//             strenght are masked.

//  DRVDN_SLWR (2 bit) - Driver Output Pull-Up slew control code  - 2bit code
//  code 11 is least slewing of signal. code 00 is highest slewing of the signal.
//
//  DRVUP_SLWF (2 bit) -Driver Output Pull-Down slew control code - 2bit code
//  code 11 is least slewing of signal. code 00 is highest slewing of the signal.
#define SET_PIN_PULLUPDOWN(n) ((n) << 2)
#define SET_BIT_6(n) ((n) << 4)
#define SET_BIT_7(n) ((n) << 6)
#define SET_BIT_8(n) ((n) << 8)

// Pin attributes
static const NvOdmPinAttrib pin_config[] = {
    {NvOdmPinRegister_T30_PullUpDown_ULPI_DATA0,
         SET_BIT_6(BIT_VAL_0) | SET_PIN_PULLUPDOWN(PULLUPDOWN_PULLUP)},

    /* Setting all i2c into normal/od_enable mode */
    {NvOdmPinRegister_T30_PullUpDown_GEN1_I2C_SCL,
         SET_BIT_7(BIT_VAL_0) | SET_BIT_6(BIT_VAL_0) |
                 SET_PIN_PULLUPDOWN(PULLUPDOWN_NORMAL)},
    {NvOdmPinRegister_T30_PullUpDown_GEN1_I2C_SDA,
         SET_BIT_7(BIT_VAL_0) | SET_BIT_6(BIT_VAL_0) |
                 SET_PIN_PULLUPDOWN(PULLUPDOWN_NORMAL)},

    {NvOdmPinRegister_T30_PullUpDown_GEN2_I2C_SCL,
         SET_BIT_7(BIT_VAL_0) | SET_BIT_6(BIT_VAL_0) |
                 SET_PIN_PULLUPDOWN(PULLUPDOWN_NORMAL)},
    {NvOdmPinRegister_T30_PullUpDown_GEN2_I2C_SDA,
         SET_BIT_7(BIT_VAL_0) | SET_BIT_6(BIT_VAL_0) |
                 SET_PIN_PULLUPDOWN(PULLUPDOWN_NORMAL)},

    {NvOdmPinRegister_T30_PullUpDown_DDC_SCL,
         SET_BIT_7(BIT_VAL_0) | SET_BIT_6(BIT_VAL_0) |
                 SET_PIN_PULLUPDOWN(PULLUPDOWN_NORMAL)},
    {NvOdmPinRegister_T30_PullUpDown_DDC_SDA,
         SET_BIT_7(BIT_VAL_0) | SET_BIT_6(BIT_VAL_0) |
                 SET_PIN_PULLUPDOWN(PULLUPDOWN_NORMAL)},

    {NvOdmPinRegister_T30_PullUpDown_PWR_I2C_SCL,
         SET_BIT_7(BIT_VAL_0) | SET_BIT_6(BIT_VAL_0) |
                 SET_PIN_PULLUPDOWN(PULLUPDOWN_NORMAL)},
    {NvOdmPinRegister_T30_PullUpDown_PWR_I2C_SDA,
         SET_BIT_7(BIT_VAL_0) | SET_BIT_6(BIT_VAL_0) |
                 SET_PIN_PULLUPDOWN(PULLUPDOWN_NORMAL)},

    {NvOdmPinRegister_T30_PullUpDown_VI_PCLK,
         SET_BIT_8(BIT_VAL_0) |
                 SET_PIN_PULLUPDOWN(PULLUPDOWN_PULLUP)},

    {NvOdmPinRegister_T30_PullUpDown_KB_ROW0,
         SET_BIT_7(BIT_VAL_0) | SET_PIN_PULLUPDOWN(PULLUPDOWN_PULLUP)},
    {NvOdmPinRegister_T30_PullUpDown_KB_ROW1,
         SET_BIT_7(BIT_VAL_0) | SET_PIN_PULLUPDOWN(PULLUPDOWN_PULLUP)},
    {NvOdmPinRegister_T30_PullUpDown_KB_ROW2,
         SET_BIT_7(BIT_VAL_0) | SET_PIN_PULLUPDOWN(PULLUPDOWN_PULLUP)},
    {NvOdmPinRegister_T30_PullUpDown_KB_COL0,
         SET_BIT_7(BIT_VAL_0) | SET_PIN_PULLUPDOWN(PULLUPDOWN_PULLUP)},
    {NvOdmPinRegister_T30_PullUpDown_KB_COL1,
         SET_BIT_7(BIT_VAL_0) | SET_PIN_PULLUPDOWN(PULLUPDOWN_PULLUP)},
    {NvOdmPinRegister_T30_PullUpDown_KB_COL2,
         SET_BIT_7(BIT_VAL_0) | SET_PIN_PULLUPDOWN(PULLUPDOWN_PULLUP)},
    {NvOdmPinRegister_T30_PullUpDown_KB_COL3,
         SET_BIT_7(BIT_VAL_0) | SET_PIN_PULLUPDOWN(PULLUPDOWN_PULLUP)},

    /*
     * Attributes for SDMMC4 pins.
     */
    {NvOdmPinRegister_T30_PullUpDown_CAM_MCLK,
        SET_BIT_7(BIT_VAL_0) | SET_BIT_6(BIT_VAL_0) |
            SET_PIN_PULLUPDOWN(PULLUPDOWN_NORMAL)},
    {NvOdmPinRegister_T30_PullUpDown_GPIO_PCC1,
        SET_BIT_7(BIT_VAL_0) | SET_BIT_6(BIT_VAL_0) |
            SET_PIN_PULLUPDOWN(PULLUPDOWN_NORMAL)},
    {NvOdmPinRegister_T30_PullUpDown_GPIO_PBB0,
        SET_BIT_7(BIT_VAL_0) | SET_BIT_6(BIT_VAL_0) |
            SET_PIN_PULLUPDOWN(PULLUPDOWN_PULLUP)},
    {NvOdmPinRegister_T30_PullUpDown_CAM_I2C_SCL,
        SET_BIT_7(BIT_VAL_0) | SET_BIT_6(BIT_VAL_0) |
            SET_PIN_PULLUPDOWN(PULLUPDOWN_PULLUP)},
    {NvOdmPinRegister_T30_PullUpDown_CAM_I2C_SDA,
        SET_BIT_7(BIT_VAL_0) | SET_BIT_6(BIT_VAL_0) |
            SET_PIN_PULLUPDOWN(PULLUPDOWN_PULLUP)},
    {NvOdmPinRegister_T30_PullUpDown_GPIO_PBB3,
        SET_BIT_7(BIT_VAL_0) | SET_BIT_6(BIT_VAL_0) |
            SET_PIN_PULLUPDOWN(PULLUPDOWN_PULLUP)},
    {NvOdmPinRegister_T30_PullUpDown_GPIO_PBB4,
        SET_BIT_7(BIT_VAL_0) | SET_BIT_6(BIT_VAL_0) |
            SET_PIN_PULLUPDOWN(PULLUPDOWN_PULLUP)},
    {NvOdmPinRegister_T30_PullUpDown_GPIO_PBB5,
        SET_BIT_7(BIT_VAL_0) | SET_BIT_6(BIT_VAL_0) |
            SET_PIN_PULLUPDOWN(PULLUPDOWN_PULLUP)},
    {NvOdmPinRegister_T30_PullUpDown_GPIO_PBB6,
        SET_BIT_7(BIT_VAL_0) | SET_BIT_6(BIT_VAL_0) |
            SET_PIN_PULLUPDOWN(PULLUPDOWN_PULLUP)},
    {NvOdmPinRegister_T30_PullUpDown_GPIO_PBB7,
        SET_BIT_7(BIT_VAL_0) | SET_BIT_6(BIT_VAL_0) |
            SET_PIN_PULLUPDOWN(PULLUPDOWN_PULLUP)},

    /* Setting drive strength of i2c pins to max*/
    // GEN1_I2C is though DBG
    {NvOdmPinRegister_T30_PadCtrl_DBGCFG,
         NVODM_QUERY_PIN_T30_PADCTRL_AOCFG1(0, 1, 3, 31, 31, 0, 0)},

    // GEN2_I2C is though ATCFG1
    {NvOdmPinRegister_T30_PadCtrl_ATCFG5,
         NVODM_QUERY_PIN_T30_PADCTRL_AOCFG1(0, 1, 3, 31, 31, 0, 0)},

    // CAM_I2C is though GMECFG
    {NvOdmPinRegister_T30_PadCtrl_GMECFG,
         NVODM_QUERY_PIN_T30_PADCTRL_AOCFG1(0, 1, 3, 31, 31, 0, 0)},

    // DDC_I2C is though DDC
    {NvOdmPinRegister_T30_PadCtrl_DDCCFG,
         NVODM_QUERY_PIN_T30_PADCTRL_AOCFG1(0, 1, 3, 31, 31, 0, 0)},

    // PWR_I2C is though AOCFG1
    {NvOdmPinRegister_T30_PadCtrl_AOCFG1,
         NVODM_QUERY_PIN_T30_PADCTRL_AOCFG1(0, 1, 3, 31, 31, 0, 0)},

    // GMI SNOR padctrl
    {NvOdmPinRegister_T30_PadCtrl_ATCFG1,
         NVODM_QUERY_PIN_T30_PADCTRL_ATCFG1(1, 1, 3, 0, 0, 3, 15)},

    {NvOdmPinRegister_T30_PadCtrl_ATCFG2,
         NVODM_QUERY_PIN_T30_PADCTRL_ATCFG1(1, 1, 3, 0, 0, 3, 15)},

    {NvOdmPinRegister_T30_PadCtrl_ATCFG3,
          NVODM_QUERY_PIN_T30_PADCTRL_ATCFG3(1, 1, 3, 0, 0, 3, 3)},

    {NvOdmPinRegister_T30_PadCtrl_DAP2CFG,
          NVODM_QUERY_PIN_T30_PADCTRL_AOCFG1(1, 1, 3, 0, 0, 3, 3)},

    {NvOdmPinRegister_T30_PadCtrl_DAP4CFG,
          NVODM_QUERY_PIN_T30_PADCTRL_AOCFG1(1, 1, 3, 0, 0, 3, 3)},

    {NvOdmPinRegister_T30_PadCtrl_DBGCFG,
          NVODM_QUERY_PIN_T30_PADCTRL_AOCFG1(1, 1, 3, 20, 0, 3, 3)},

    {NvOdmPinRegister_T30_PadCtrl_SPICFG,
          NVODM_QUERY_PIN_T30_PADCTRL_AOCFG1(1, 1, 3, 0, 0, 3, 3)},

    {NvOdmPinRegister_T30_PadCtrl_UART2CFG,
          NVODM_QUERY_PIN_T30_PADCTRL_AOCFG1(1, 1, 3, 0, 0, 3, 3)},

    {NvOdmPinRegister_T30_PadCtrl_UART3CFG,
          NVODM_QUERY_PIN_T30_PADCTRL_AOCFG1(1, 1, 3, 0, 0, 3, 3)},

    // SDMMC4 padctrl
    {NvOdmPinRegister_T30_PadCtrl_GMECFG,
          NVODM_QUERY_PIN_T30_PADCTRL_ATCFG3(0, 1, 3, 1, 4, 3, 3)},

    {NvOdmPinRegister_T30_PadCtrl_GMFCFG,
          NVODM_QUERY_PIN_T30_PADCTRL_ATCFG3(0, 1, 3, 0, 0, 3, 3)},

    {NvOdmPinRegister_T30_PadCtrl_GMGCFG,
          NVODM_QUERY_PIN_T30_PADCTRL_ATCFG3(0, 1, 3, 3, 0, 3, 3)},

    {NvOdmPinRegister_T30_PadCtrl_GMHCFG,
          NVODM_QUERY_PIN_T30_PADCTRL_ATCFG3(0, 1, 3, 0, 12, 3, 3)},
};
#endif

NvU32
NvOdmQueryPinAttributes(const NvOdmPinAttrib** pPinAttributes)
{
#ifndef SET_KERNEL_PINMUX
#if !NVOS_IS_QNX
    if (pPinAttributes)
    {
        *pPinAttributes = &pin_config[0];
        return NV_ARRAY_SIZE(pin_config);
    }
#endif
    return 0;
#endif
    return NvError_NotSupported;
}

NvBool NvOdmQueryGetPmuProperty(NvOdmPmuProperty* pPmuProperty)
{
    return NV_FALSE;
}

/**
 * Gets the lowest soc power state supported by the hardware
 *
 * @returns information about the SocPowerState
 */
const NvOdmSocPowerStateInfo* NvOdmQueryLowestSocPowerState(void)
{
    static NvOdmSocPowerStateInfo PowerStateInfo;
    static const NvOdmSocPowerStateInfo* pPowerStateInfo = NULL;

    if (pPowerStateInfo == NULL)
    {
        // Lowest power state.
        PowerStateInfo.LowestPowerState = NvOdmSocPowerState_Active;

        pPowerStateInfo = (const NvOdmSocPowerStateInfo*) &PowerStateInfo;
    }

    return (pPowerStateInfo);
}

const NvOdmUsbProperty*
NvOdmQueryGetUsbProperty(NvOdmIoModule OdmIoModule,
                         NvU32 Instance)
{
    static const NvOdmUsbProperty Usb1Property =
    {
        NvOdmUsbInterfaceType_Utmi,
        (NvOdmUsbChargerType_SE0 | NvOdmUsbChargerType_SE1 | NvOdmUsbChargerType_SK),
        20,
        NV_TRUE,
        NvOdmUsbModeType_Device,
        NvOdmUsbIdPinType_CableId,
        NvOdmUsbConnectorsMuxType_None,
        NV_FALSE,
        { 0, 0, 0, 0 }
    };

     static const NvOdmUsbProperty Usb2Property =
     {
        NvOdmUsbInterfaceType_UlpiExternalPhy,
        NvOdmUsbChargerType_UsbHost,
        20,
        NV_TRUE,
        NvOdmUsbModeType_None,
        NvOdmUsbIdPinType_None,
        NvOdmUsbConnectorsMuxType_None,
        NV_FALSE,
        { 0, 0, 0, 0 }
    };

    static const NvOdmUsbProperty Usb3Property =
    {
        NvOdmUsbInterfaceType_Utmi,
        NvOdmUsbChargerType_UsbHost,
        20,
        NV_TRUE,
        NvOdmUsbModeType_Host,
        NvOdmUsbIdPinType_CableId,
        NvOdmUsbConnectorsMuxType_None,
        NV_FALSE,
        { 0, 0, 0, 0 }
    };

    if (OdmIoModule == NvOdmIoModule_Usb && Instance == 0)
        return &(Usb1Property);

    if (OdmIoModule == NvOdmIoModule_Usb && Instance == 1)
        return &(Usb2Property);

    if (OdmIoModule == NvOdmIoModule_Usb && Instance == 2)
        return &(Usb3Property);

    return (const NvOdmUsbProperty *)NULL;
}

const NvOdmQuerySdioInterfaceProperty* NvOdmQueryGetSdioInterfaceProperty(NvU32 Instance)
{
    return &s_NvOdmQuerySdioInterfaceProperty[Instance];
}

const NvOdmQueryHsmmcInterfaceProperty* NvOdmQueryGetHsmmcInterfaceProperty(NvU32 Instance)
{
    return NULL;
}

NvU32
NvOdmQueryGetBlockDeviceSectorSize(NvOdmIoModule OdmIoModule)
{
    return 0;
}

const NvOdmQueryOwrDeviceInfo* NvOdmQueryGetOwrDeviceInfo(NvU32 Instance)
{
    return &s_NvOdmQueryOwrInfo;
}

const NvOdmGpioWakeupSource *NvOdmQueryGetWakeupSources(NvU32 *pCount)
{
    *pCount = 0;
    return NULL;
}

const NvOdmQuerySpifConfig*
NvOdmQueryGetSpifConfig(
    NvU32 Instance,
    NvU32 DeviceId)
{
    return NULL;
}

NvU32
NvOdmQuerySpifWPStatus(
    NvU32 DeviceId,
    NvU32 StartBlock,
    NvU32 NumberOfBlocks)
{
    return 0;
}

void NvOdmQueryGetSnorTimings(SnorTimings *timings, enum NorTiming NorTimings)
{
    timings->timing0 = 0x30300273;
    timings->timing1 = 0x30302;
}

NvBool NvOdmQueryIsClkReqPinUsed(NvU32 Instance)
{
    return NV_FALSE;
}

NvU32 NvOdmQueryDataPartnEncryptFooterSize(const char *name)
{
    /*
     * the size of the crypto footer for data partitions on android
     * is 16KB. we double it, to make it future-proof.
     */
    if (NvOdmOsStrcmp(name, "UDA") == 0)
        return (16384 * 2);
    else
        return 0;
}

#ifdef LPM_BATTERY_CHARGING
NvBool NvOdmQueryChargingConfigData(NvOdmChargingConfigData *pData)
{
    return NV_FALSE;
}
#endif

NvBool NvOdmQueryGetModemId(NvU8 *ModemId)
{
    /* Implement this function based on the need */
    return NV_FALSE;
}

NvBool NvOdmQueryGetPmicWdtDisableSetting(NvBool *PmicWdtDisable)
{
    return NV_FALSE;
}

NvU32
NvOdmQueryGetWifiOdmData(void)
{
    NvU32 OdmData ;

    // Reading bit 3, 4, 5 set for Wifi chip selection
    OdmData = GetBctKeyValue();
    return (OdmData >> 3) & 0x7;
}

NvU8
NvOdmQueryBlUnlockBit(void)
{
    return 8;
}

NvU8
NvOdmQueryBlWaitBit(void)
{
    return 12;
}

NvBool NvOdmQueryGetSkuOverride(NvU8 *SkuOverride)
{
    return NV_FALSE;
}

NvU8 NvOdmQueryGetDebugConsoleOptBits(void)
{
    return 15;
}

NvU32 NvOdmQueryGetOdmdataField(NvU32 CustOpt, NvU32 Field)
{
    NvU32 FieldValue = 0;

    switch (Field)
    {
    case NvOdmOdmdataField_DebugConsole:
        FieldValue = NV_DRF_VAL(TEGRA_DEVKIT, BCT_CUSTOPT, CONSOLE, CustOpt);
        break;
    case NvOdmOdmdataField_DebugConsoleOptions:
        switch (NV_DRF_VAL(TEGRA_DEVKIT, BCT_CUSTOPT, CONSOLE, CustOpt))
        {
        case TEGRA_DEVKIT_BCT_CUSTOPT_0_CONSOLE_NONE:
#ifdef DEFAULT_CONSOLE
            FieldValue = s_NvOdmQueryDebugConsoleSetting;
#else
            FieldValue = NvOdmDebugConsole_None;
#endif
            break;
        case TEGRA_DEVKIT_BCT_CUSTOPT_0_CONSOLE_DCC:
            FieldValue = NvOdmDebugConsole_Dcc;
            break;
        case TEGRA_DEVKIT_BCT_CUSTOPT_0_CONSOLE_UART:
            FieldValue = NvOdmDebugConsole_UartA +
                NV_DRF_VAL(TEGRA_DEVKIT, BCT_CUSTOPT, CONSOLE_OPTION, CustOpt);
            break;
        case TEGRA_DEVKIT_BCT_CUSTOPT_0_CONSOLE_AUTOMATION:
            FieldValue = NvOdmDebugConsole_Automation;
            break;
        default:
            FieldValue = s_NvOdmQueryDebugConsoleSetting;
            break;
        }
        break;
    default:
        FieldValue = 0;
        break;
    }
    return FieldValue;
}
NvU32 NvOdmQueryGetUsbData(void)
{
    return 0;
}

NvS32 NvOdmQueryGetLaneData(void)
{
    return -1;
}

NvU32 NvOdmQueryGetPLLMFreqData(void)
{
    return 0;
}

NvU32 NvOdmQueryRotateDisplay (void)
{
    return 0;
}

NvU32 NvOdmQueryRotateBMP(void)
{
    return 0;
}

NvBool NvOdmQueryLp0Supported(void)
{
    return NV_FALSE;
}

void NvOdmQueryLP0WakeConfig(NvU32 *wake_mask, NvU32 *wake_level)
{
    // Not implemented
}
