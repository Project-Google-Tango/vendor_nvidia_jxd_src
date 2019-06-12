/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
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

/* PMU Board ID */
#define PMU_E1731           0x06c3 //TI PMU
#define PMU_E1733           0x06c5 //AMS3722/29 BGA
#define PMU_E1734           0x06c6 //AMS3722 CSP
#define PMU_E1735           0x06c7 //TI 913/4
#define PMU_E1736           0x06c8 //TI 913/4
#define PMU_E1936           0x0790 //TI 913/4
#define PMU_E1769           0x06e9 //TI 913/4
#define PMU_E1761           0x06e1 //TI 913/4

NvU32 GetBctKeyValue(void);

#define BOARD_PM358    0x0166
#define BOARD_PM359    0x0167
#define BOARD_PM363    0x016B
#define BOARD_PM374    0x0176
#define BOARD_PM370    0x0172

#define PROC_BOARD_P1761 0x6E1

#if NV_DEBUG
static NvOdmDebugConsole
s_NvOdmQueryDebugConsoleSetting = NvOdmDebugConsole_Dcc;
#else
static NvOdmDebugConsole
s_NvOdmQueryDebugConsoleSetting = NvOdmDebugConsole_None;
#endif

/* SPI flash ODM configuration */
static  NvOdmQuerySpifConfig s_NvOdmQuerySpifConfig =
{
    0x6,     /* Write enable command */
    0x4,     /* Write disable command */
    0x5,     /* Read status command */
    0x1,     /* Write status command */
    0x3,     /* Read data command */
    0x2,     /* Write data command */
    0xD8,    /* Block erase command */
    0x20,    /* Sector erase command */
    0xC7,    /* Chip erase command */
    0x90,    /* Read device ID command */
    0x1,     /* Value to specify the busy status of the device. */
    256,     /* Bytes per page */
    4096,    /* Bytes per sector */
    16,      /* Sectors per block */
    64,      /* Total blocks on the device */
    0,       /* SPI chip select */
    15000,   /* SPI clock speed in KHz */
    0        /* SPI pin map */
};

static const NvU8
s_NvOdmQueryDeviceNamePrefixValue[] = {'T','e','g','r','a',0};

static const NvU8
s_NvOdmQueryManufacturerSetting[] = {'N','V','I','D','I','A',0};

static const NvU8
s_NvOdmQueryModelSetting[] = {'T','1','2','4',0};

static const NvU8
s_NvOdmQueryPlatformSetting[] = {'A','r','d','b','e','g',0};

static const NvU8
s_NvOdmQueryPlatformSetting_laguna[] = {'L','a','g','u','n','a',0};

static const NvU8
s_NvOdmQueryPlatformSetting_TN8FFD[] = {'T','N','8','-','F','F','D',0};

static const NvU8
s_NvOdmQueryProjectNameSetting[] = {'O','D','M',' ','K','i','t',0};

static const NvOdmDownloadTransport
s_NvOdmQueryDownloadTransportSetting = NvOdmDownloadTransport_SpiEthernet;

static const NvOdmQuerySdioInterfaceProperty s_NvOdmQuerySdioInterfaceProperty[] =
{
    { NV_FALSE, 10, NV_FALSE, 2, 2, NvOdmQuerySdioSlotUsage_wlan,   2, 2, 2, 2},
    { NV_FALSE, 10, NV_TRUE,  2, 3, NvOdmQuerySdioSlotUsage_unused, 2, 3, 2 ,3},
    { NV_TRUE,  10, NV_FALSE, 0, 3, NvOdmQuerySdioSlotUsage_Media,  0, 3, 0, 3},
    { NV_FALSE, 10, NV_TRUE,  4, 0, NvOdmQuerySdioSlotUsage_Boot,   4, 0, 4, 4},
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


/* --- Function Implementations ---*/

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
                ((NvRmModuleNandInterfaceCaps*)pCaps)->NandInterfaceWidth = 16;
            }
            else
                return NvError_NotSupported;
            return NvSuccess;

        case NvOdmIoModule_Uart:
            if (Instance == 0)
                ((NvRmModuleUartInterfaceCaps *)pCaps)->NumberOfInterfaceLines = 8;
            else if ((Instance == 1) || (Instance == 3))
                ((NvRmModuleUartInterfaceCaps *)pCaps)->NumberOfInterfaceLines = 4;
            else if (Instance == 2 || (Instance == 4))
                ((NvRmModuleUartInterfaceCaps *)pCaps)->NumberOfInterfaceLines = 0;
            else
                return NvError_NotSupported;
            return NvSuccess;

        case NvOdmIoModule_Pwm:
            if (Instance == 1)
                ((NvRmModulePwmInterfaceCaps *)pCaps)->PwmOutputIdSupported = 15;
            else
            {
                ((NvRmModulePwmInterfaceCaps *)pCaps)->PwmOutputIdSupported = 0;
                return NvError_NotSupported;
            }
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
        return (NvOdmDebugConsole_Automation | (NvOdmDebugConsole_UartA +
                      NV_DRF_VAL(TEGRA_DEVKIT, BCT_CUSTOPT, CONSOLE_OPTION, CustOpt)));
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

    static const NvOdmQuerySpiDeviceInfo s_OdmWinBondSpiFInfo =
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

        if ((ControllerId == 3) && (ChipSelect == 0))
             return &s_OdmDefaultSpiInfo;

        if ((ControllerId == 3) && (ChipSelect == 1))
             return &s_OdmWinBondSpiFInfo;

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
        0x38,
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
    NvOdmBoardInfo BoardInfo;

    NvOdmPeripheralGetBoardInfo(0x0, &BoardInfo);
    if ((BoardInfo.BoardID == BOARD_PM358)||(BoardInfo.BoardID == BOARD_PM359)||
        (BoardInfo.BoardID == BOARD_PM374)||(BoardInfo.BoardID == BOARD_PM370)||
                                           (BoardInfo.BoardID == BOARD_PM363))
    {
        return s_NvOdmQueryPlatformSetting_laguna;;
    }
    else if (BoardInfo.BoardID == PROC_BOARD_P1761)
    {
        return s_NvOdmQueryPlatformSetting_TN8FFD;
    }
    else
    {
        return s_NvOdmQueryPlatformSetting;
    }
}

const NvU8* NvOdmQueryProjectName(void)
{
    return s_NvOdmQueryProjectNameSetting;
}

NvU32
NvOdmQueryPinAttributes(const NvOdmPinAttrib** pPinAttributes)
{
    return NvError_NotSupported;
}

NvBool NvOdmQueryGetPmuProperty(NvOdmPmuProperty* pPmuProperty)
{
    NvOdmPmuBoardInfo PmuBoardInfo;
    NvOdmBoardInfo ProcBoard;

    NvOdmPeripheralGetBoardInfo(0, &ProcBoard);

    NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_PmuBoard,
                                 &PmuBoardInfo, sizeof(PmuBoardInfo));

    // Interrupt polarity low for TI913
    if(PmuBoardInfo.BoardInfo.BoardID == PMU_E1736  ||
        PmuBoardInfo.BoardInfo.BoardID == PMU_E1769 ||
        PmuBoardInfo.BoardInfo.BoardID == PMU_E1936 ||
        PmuBoardInfo.BoardInfo.BoardID == PMU_E1735 ||
        PmuBoardInfo.BoardInfo.BoardID == PMU_E1761)
    {
        pPmuProperty->IrqConnected = NV_TRUE;
        pPmuProperty->IrqPolarity  = NvOdmInterruptPolarity_Low;
    }
    else
    {
        // Default values
        pPmuProperty->IrqConnected = NV_FALSE;
        pPmuProperty->IrqPolarity  = NvOdmInterruptPolarity_High;
    }
    return NV_TRUE;
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
        NvOdmUsbModeType_Host,
        NvOdmUsbIdPinType_None,
        NvOdmUsbConnectorsMuxType_None,
        NV_FALSE,
        { 0, 0, 0, 0 }
    };

     static const NvOdmUsbProperty Usb2Property =
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
    if (DeviceId == 0xEF15EF15) // winbond Device ID
    {
        s_NvOdmQuerySpifConfig.TotalBlocks = 64; // 4MB
    }
    return &s_NvOdmQuerySpifConfig;
}

NvU32
NvOdmQuerySpifWPStatus(
    NvU32 DeviceId,
    NvU32 StartBlock,
    NvU32 NumberOfBlocks)
{
    return 0;
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

NvBool NvOdmQueryGetModemId(NvU8 *ModemId)
{
    NvU32 CustOpt = GetBctKeyValue();

    *ModemId = NV_DRF_VAL(TEGRA_DEVKIT, BCT_CUSTOPT, MODEM, CustOpt);
    return NV_TRUE;
}

NvBool NvOdmQueryGetPmicWdtDisableSetting(NvBool *PmicWdtDisable)
{
    return NV_FALSE;
}

NvBool NvOdmQueryIsClkReqPinUsed(NvU32 Instance)
{
    // For all instances on the cardhu does not use this pin.
    return NV_FALSE;
}

#ifdef LPM_BATTERY_CHARGING
NvBool NvOdmQueryChargingConfigData(NvOdmChargingConfigData *pData)
{
    NvOdmPmuBoardInfo PmuBoardInfo;

    if (!pData)
        return NV_FALSE;

    NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_PmuBoard,
                                 &PmuBoardInfo, sizeof(PmuBoardInfo));

    /* To be on the safer side value has to >= 100000 */
    pData->LpmCpuKHz = 100000;
    /* milli seconds */
    pData->NotifyTimeOut = 2000;

#ifdef READ_BATTERY_SOC
    if(PmuBoardInfo.BoardInfo.BoardID == PMU_E1736 ||
            PmuBoardInfo.BoardInfo.BoardID == PMU_E1761 ||
            PmuBoardInfo.BoardInfo.BoardID == PMU_E1936 ||
            PmuBoardInfo.BoardInfo.BoardID == PMU_E1735 ||
            PmuBoardInfo.BoardInfo.BoardID == PMU_E1769)
    {
        pData->BootloaderChargeLevel = 1;
        pData->OsChargeLevel = 5;
        pData->ExitChargeLevel = 5;
    }
    else
       return NV_FALSE;
#else
    if(PmuBoardInfo.BoardInfo.BoardID == PMU_E1735)
    {
        pData->BootloaderChargeLevel = 6800;
        pData->OsChargeLevel = 7200;
        pData->ExitChargeLevel = 7200;
    }
    else if(PmuBoardInfo.BoardInfo.BoardID == PMU_E1736 ||
            PmuBoardInfo.BoardInfo.BoardID == PMU_E1761 ||
            PmuBoardInfo.BoardInfo.BoardID == PMU_E1936 ||
            PmuBoardInfo.BoardInfo.BoardID == PMU_E1769)
    {
        pData->BootloaderChargeLevel = 3100;
        pData->OsChargeLevel = 3500;
        pData->ExitChargeLevel = 3500;
    }
    else
        return NV_FALSE;

#endif

    return NV_TRUE;
}
#endif

NvU8
NvOdmQueryGetSdCardInstance(void)
{
    return 0;
}

NvU32
NvOdmQueryGetWifiOdmData(void)
{
    NvU32 OdmData = 0;       // Customer configuration option
    OdmData = GetBctKeyValue();
    return (OdmData & 0x1f00) >> 8; // Bits 8-12 reserved for Wifi on T114
}

NvU8
NvOdmQueryBlUnlockBit(void)
{
    return 13;
}

NvU8
NvOdmQueryBlWaitBit(void)
{
    return 14;
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
            FieldValue = (NvOdmDebugConsole_Automation | (NvOdmDebugConsole_UartA +
                          NV_DRF_VAL(TEGRA_DEVKIT, BCT_CUSTOPT, CONSOLE_OPTION, CustOpt)));
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

NvBool
NvOdmQueryGetDebugCarveoutPresence(void)
{
    NvU32 OdmData = 0;       // Customer configuration option
    OdmData = GetBctKeyValue();
    return (OdmData & 0x100000) >> 20; // Bit 20
}

NvU32
NvOdmQueryGetUsbPortOwner(void)
{
    // FIXME:- Implement this function
    NvU32 OdmData = 0;       // Customer configuration option
    OdmData = GetBctKeyValue();
    return (OdmData & 0x0F000000) >> 24; // Bits 27-24
}

NvU32 NvOdmQueryGetUsbData(void)
{
    return (( GetBctKeyValue() & 0x0f000000 ) >> 24); // Bits 27-24 reserved for USB
}

NvS32 NvOdmQueryGetLaneData(void)
{
// Bits 30-28 reserved for PCIe, SATA & USB Lane on Laguna platforms
    return (( GetBctKeyValue() & 0x70000000 ) >> 28);
}

NvU32 NvOdmQueryGetPLLMFreqData(void)
{
    return (( GetBctKeyValue() & 0x0200000 ) >> 21); // Bit 21 reserved for PLLM
}

NvU32 NvOdmQueryRotateDisplay (void)
{
    NvOdmBoardInfo ProcBoard;
    NvU32 Rotation = 0;

    NvOdmPeripheralGetBoardInfo(0, &ProcBoard);

    if (ProcBoard.BoardID == PROC_BOARD_P1761)
        Rotation = 180;

    return Rotation;
}

NvU32 NvOdmQueryRotateBMP(void)
{
    return 0;
}

void NvOdmQueryGetSnorTimings(SnorTimings *timings, enum NorTiming NorTimings)
{
    /* TODO: need to fill the right timing here */
    timings->timing0 = 0;
    timings->timing1 = 0;
}

NvBool NvOdmQueryLp0Supported(void)
{
    return NV_FALSE;
}


NvBool NvOdmEnableCpuPowerRail(void)
{
    return NV_TRUE;
}

void NvOdmQueryLP0WakeConfig(NvU32 *wake_mask, NvU32 *wake_level)
{
    // Not implemented
}
NvBool NvOdmQueryIsHdmiPrimaryDisplay(void)
{
    return NV_FALSE;
}
