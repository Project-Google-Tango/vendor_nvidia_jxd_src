/*
 * Copyright (c) 2013 - 2014, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA APX ODM Kit::
 *         Implementation of the ODM Peripheral Discovery API</b>
 *
 * @b Description: The peripheral connectivity database implementation.
 */
#include "nvbct.h"
#include "nvcommon.h"
#include "nvodm_query_gpio.h"
#include "nvodm_modules.h"
#include "nvcommon.h"
#include "nvodm_query_gpio.h"
#include "nvodm_modules.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query_discovery_imager.h"
#include "nvodm_query_kbc_gpio_def.h"
#include "subboards/nvodm_query_discovery_e1780_addresses.h"
#include "subboards/nvodm_query_discovery_e1806_addresses.h"
#include "nvodm_imager_guid.h"
#ifdef BOOT_MINIMAL_BL
#include "aos_avp_board_info.h"
#endif
#include "tegra_devkit_custopt.h"
#include "nvodm_keylist_reserved.h"
#include "nvbct.h"
#include "nvrm_drf.h"
#include "tca6408.h"

#define NVODM_QUERY_I2C_INSTANCE_ID      0
#define NVODM_QUERY_MAX_EEPROMS          8      // Maximum number of EEPROMs per bus segment
#define NVODM_QUERY_I2C_EEPROM_ADDRESS   0xA0   // I2C device base address for EEPROM (7'h50)
#define NVODM_QUERY_BOARD_HEADER_START   0x00   // Offset to Part Number in EERPOM
#define NVODM_QUERY_I2C_CLOCK_SPEED      100    // kHz
#define I2C_AUDIOCODEC_ID                4
#define PMU_I2C_INSTANCE_ID              4
#define PMU_EEPROM_INSTANCE_ID           5
#define TPS65913_I2C_SLAVE_ADDR          0xB0
#define TPS65913_I2C_SLAVE_ADDR2         0xB2

/* Ardbeg id */
#define BOARD_E1780            0x6F4
#define BOARD_E1781            0x6F5
#define BOARD_E1792            0x700
#define BOARD_E1791            0x6ff
#define BOARD_E1797            0x705
#define BOARD_E1782            0x6F6
#define BOARD_E1783            0x6F7
#define BOARD_P1761            0x6E1
#define BOARD_E1762            0x6E2
#define BOARD_E1784            0x6F8
#define BOARD_E1922            0x782
#define BOARD_E1923            0x783

/* Laguna board ids */
#define BOARD_PM358            0x0166
#define BOARD_PM359            0x0167
#define BOARD_PM363            0x016B
#define BOARD_PM374            0x0176
#define BOARD_PM370            0x0172

//Add any new processor boards info here
#define  PROCESSOR_BOARDS_BOARD_INFO BOARD_E1780,BOARD_PM358,BOARD_PM359,\
                                     BOARD_PM370,BOARD_PM374,BOARD_PM363,\
                                     BOARD_E1781,BOARD_E1792,BOARD_E1782,\
                                     BOARD_E1783,BOARD_E1784,BOARD_E1791,\
                                     BOARD_P1761,BOARD_E1922,BOARD_E1923

//display boards
#define BOARD_E1813            0x715
#define BOARD_E1639            0x0667
#define BOARD_E1627            0x065B
#define BOARD_PM354            0x0162
#define BOARD_E1824            0x0720
#define BOARD_E1549            0x060D
#define BOARD_E1807            0x070F
#define BOARD_E1937            0x0791

// Ardbeg PMU board ids
#define PMU_BOARD_E1731        0x6C3
#define PMU_BOARD_E1733        0x6C5
#define PMU_BOARD_E1734        0x6C6
#define PMU_BOARD_E1735        0x6C7
#define PMU_BOARD_E1736        0x6C8
#define PMU_BOARD_E1769        0x6E9
#define PMU_BOARD_E1936        0x790
#define PMU_BOARD_E1761        0x6E1

#define PMU_BOARDS_BOARD_INFO PMU_BOARD_E1731, PMU_BOARD_E1733, PMU_BOARD_E1734,\
                              PMU_BOARD_E1735, PMU_BOARD_E1736, PMU_BOARD_E1769,\
                              PMU_BOARD_E1761, PMU_BOARD_E1936

#define IS_PMU_BOARD(x) \
    (x) == PMU_BOARD_E1731 ? NV_TRUE : \
    (x) == PMU_BOARD_E1733 ? NV_TRUE : \
    (x) == PMU_BOARD_E1734 ? NV_TRUE : \
    (x) == PMU_BOARD_E1735 ? NV_TRUE : \
    (x) == PMU_BOARD_E1736 ? NV_TRUE : \
    (x) == PMU_BOARD_E1769 ? NV_TRUE : \
    (x) == PMU_BOARD_E1936 ? NV_TRUE : \
    (x) == PMU_BOARD_E1761 ? NV_TRUE : \
    NV_FALSE

// The following are used to store entries read from EEPROMs at runtime.
static NvOdmBoardInfoRev1 s_ExtendedBoardModuleTable[NVODM_QUERY_MAX_EEPROMS];
#ifdef BOOT_MINIMAL_BL
extern NvBoardInfo g_BoardInfo;
extern NvBoardInfo g_PmuBoardId;
extern NvBoardInfo g_DisplayBoardId;
#endif
static const NvOdmIoAddress s_SdioAddresses[] =
{
    { NvOdmIoModule_Sdio, 0x0, 0x0, 0 },
    { NvOdmIoModule_Sdio, 0x2, 0x0, 0 },
    { NvOdmIoModule_Sdio, 0x3, 0x0, 0 },
};

//LC709203F
static const NvOdmIoAddress s_Lc709203fAddresses[] =
{
    { NvOdmIoModule_I2c, 0x1,  0x16, 0 },
};

static const NvOdmIoAddress s_VddSataAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_VDDIO_SATA, 0x00},
};

static const NvOdmIoAddress s_Sharp2516PanelAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_AVDD_DSI_CSI, 0x00},
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_VDD_LCD_DIS, 0x00},
    { NvOdmIoModule_Pwm, 0x1,  0x00, 0x00},
};

static const NvOdmIoAddress s_PanasonicPanelAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_AVDD_DSI_CSI, 0x00},
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_VDD_LCD_DIS, 0x00},
    { NvOdmIoModule_Pwm, 0x1,  0x00, 0x00},
};

static const NvOdmIoAddress s_LgdPanelAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_AVDD_DSI_CSI, 0x00},
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_VDD_LCD_DIS, 0x00},
    { NvOdmIoModule_Pwm, 0x1,  0x00, 0x00},
};

static const NvOdmIoAddress s_Sharp2516PanelAddressesTps65914[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_AVDD_DSI_CSI, 0x00},
    { NvOdmIoModule_Pwm, 0x1,  0x00, 0x00},
};

static const NvOdmIoAddress s_PanasonicPanelAddressesTps65914[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_AVDD_DSI_CSI, 0x00},
    { NvOdmIoModule_Pwm, 0x1,  0x00, 0x00},
};

static const NvOdmIoAddress s_PanasonicPanelAddressesE1736Tps65914[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_AVDD_DSI_CSI, 0x00},
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_VD_LCD_HV, 0x00},
    { NvOdmIoModule_Pwm, 0x1,  0x00, 0x00},
};

static const NvOdmIoAddress s_LgdPanelAddressesTps65914[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_AVDD_DSI_CSI, 0x00},
    { NvOdmIoModule_Pwm, 0x1,  0x00, 0x00},
};

static const NvOdmIoAddress s_LgdPanelAddressesE1736Tps65914[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_AVDD_DSI_CSI, 0x00},
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_VD_LCD_HV, 0x00},
    { NvOdmIoModule_Pwm, 0x1,  0x00, 0x00},
};

static const NvOdmIoAddress s_LgdPanelAddressesE1769Tps65914[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_AVDD_DSI_CSI, 0x00},
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_VD_LCD_HV, 0x00},
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_LCD_1V8, 0x00},
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_EN_LCD_1V8, 0x00},
    { NvOdmIoModule_Pwm, 0x1,  0x00, 0x00},
};

static const NvOdmIoAddress s_AuoPanelAddressesE1807Tps65914[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_AVDD_DSI_CSI, 0x00},
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_EN_LCD_1V8, 0x00},
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_VDD_LCD_DIS, 0x00},
    { NvOdmIoModule_Pwm, 0x1,  0x00, 0x00},
};

static const NvOdmIoAddress s_AuoPanelAddressesE1937Tps65914[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_AVDD_DSI_CSI, 0x00},
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_EN_LCD_1V8, 0x00},
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_VDD_LCD_DIS, 0x00},
    { NvOdmIoModule_Pwm, 0x1,  0x00, 0x00},
};

static const NvOdmIoAddress s_HdmiAddresses[] =
{
    { NvOdmIoModule_Hdmi, 0, 0, 0 },
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_AVDD_HDMI , 0x00},
};

static NvOdmPeripheralConnectivity s_Peripherals[] =
{

#include "subboards/nvodm_query_discovery_e1780_peripherals.h"
#include "subboards/nvodm_query_discovery_e1823_peripherals.h"
#include "subboards/nvodm_query_discovery_e1806_peripherals.h"
#include "subboards/nvodm_query_discovery_e1793_peripherals.h"

    //  Sdio module
    {
        NV_ODM_GUID('s','d','i','o','_','m','e','m'),
        s_SdioAddresses,
        NV_ARRAY_SIZE(s_SdioAddresses),
        NvOdmPeripheralClass_Other,
    },
    {
        NV_ODM_GUID('N','V','2','5','1','6','s','p'),
        s_Sharp2516PanelAddresses,
        NV_ARRAY_SIZE(s_Sharp2516PanelAddresses),
        NvOdmPeripheralClass_Display,
    },
    {
        NV_ODM_GUID('N','V','D','A','p','a','n','a'),
        s_PanasonicPanelAddresses,
        NV_ARRAY_SIZE(s_PanasonicPanelAddresses),
        NvOdmPeripheralClass_Display,
    },
    {
        NV_ODM_GUID('N','V','D','I','A','l','g','d'),
        s_LgdPanelAddresses,
        NV_ARRAY_SIZE(s_LgdPanelAddresses),
        NvOdmPeripheralClass_Display,
    },
    {
        NV_ODM_GUID('N','V','6','5','9','1','s','p'),
        s_Sharp2516PanelAddressesTps65914,
        NV_ARRAY_SIZE(s_Sharp2516PanelAddressesTps65914),
        NvOdmPeripheralClass_Display,
    },
    {
        NV_ODM_GUID('N','V','9','1','4','p','a','n'),
        s_PanasonicPanelAddressesTps65914,
        NV_ARRAY_SIZE(s_PanasonicPanelAddressesTps65914),
        NvOdmPeripheralClass_Display,
    },
    {
        NV_ODM_GUID('N','V','9','1','4','l','g','d'),
        s_LgdPanelAddressesTps65914,
        NV_ARRAY_SIZE(s_LgdPanelAddressesTps65914),
        NvOdmPeripheralClass_Display,
    },
    {
        NV_VDD_SATA_ODM_ID,
        s_VddSataAddresses,
        NV_ARRAY_SIZE(s_VddSataAddresses),
        NvOdmPeripheralClass_Other,
    },
    // Lc709203f
    {
        NV_ODM_GUID('l','c','7','0','9','2','0','*'),
        s_Lc709203fAddresses,
        NV_ARRAY_SIZE(s_Lc709203fAddresses),
        NvOdmPeripheralClass_Other
    },
    {
        NV_ODM_GUID('N','V','9','1','4','_','p','a'),
        s_PanasonicPanelAddressesE1736Tps65914,
        NV_ARRAY_SIZE(s_PanasonicPanelAddressesE1736Tps65914),
        NvOdmPeripheralClass_Display,
    },
    {
        NV_ODM_GUID('N','V','9','1','4','_','l','g'),
        s_LgdPanelAddressesE1736Tps65914,
        NV_ARRAY_SIZE(s_LgdPanelAddressesE1736Tps65914),
        NvOdmPeripheralClass_Display,
    },
    {
        NV_ODM_GUID('N','V','1','7','6','9','l','g'),
        s_LgdPanelAddressesE1769Tps65914,
        NV_ARRAY_SIZE(s_LgdPanelAddressesE1769Tps65914),
    },
    {
        NV_ODM_GUID('f','f','a','2','h','d','m','i'),
        s_HdmiAddresses,
        NV_ARRAY_SIZE(s_HdmiAddresses),
        NvOdmPeripheralClass_Display,
    },
    {
        NV_ODM_GUID('N','V','1','8','0','7','a','u'),
        s_AuoPanelAddressesE1807Tps65914,
        NV_ARRAY_SIZE(s_AuoPanelAddressesE1807Tps65914),
    },
    {
        NV_ODM_GUID('N','V','1','9','3','7','a','u'),
        s_AuoPanelAddressesE1937Tps65914,
        NV_ARRAY_SIZE(s_AuoPanelAddressesE1937Tps65914),
    },
};

static NvBool s_ReadBoardInfoDone = NV_FALSE;
static NvU8 NumBoards = 0;

/* DISPLAY table */
static NvOdmDisplayBoardInfo displayBoardTable[] = {
    {
        NvOdmBoardDisplayType_DSI,
        NV_TRUE,
        {
            BOARD_E1639,
        }
    },
    {
        NvOdmBoardDisplayType_DSI,
        NV_TRUE,
        {
            BOARD_E1797,
        }
    },
    {
        NvOdmBoardDisplayType_DSI,
        NV_TRUE,
        {
            BOARD_E1549,
        }
    },
    {
        NvOdmBoardDisplayType_DSI,
        NV_TRUE,
        {
            BOARD_E1813,
        }
    },
    {
        NvOdmBoardDisplayType_DSI,
        NV_TRUE,
        {
            BOARD_E1627,
        }
    },
    {
        NvOdmBoardDisplayType_DSI,
        NV_TRUE,
        {
            BOARD_PM354,
        }
    },
    {
        NvOdmBoardDisplayType_DSI,
        NV_TRUE,
        {
            BOARD_P1761,
        }
    },
    {
        NvOdmBoardDisplayType_EDP,
        NV_TRUE,
        {
            BOARD_E1824,
        }
    },
    {
        NvOdmBoardDisplayType_EDP,
        NV_TRUE,
        {
            BOARD_PM363,
        }
    },
 };

extern NvU32 GetBctKeyValue(void);

/*****************************************************************************/

static const NvOdmPeripheralConnectivity*
NvApGetAllPeripherals (NvU32 *pNum)
{
    if (!pNum) {
        return NULL;
    }

    *pNum = NV_ARRAY_SIZE(s_Peripherals);
    return (const NvOdmPeripheralConnectivity *)s_Peripherals;
}

// This implements a simple linear search across the entire set of currently-
// connected peripherals to find the set of GUIDs that Match the search
// criteria.  More clever implementations are possible, but given the
// relatively small search space (max dozens of peripherals) and the relative
// infrequency of enumerating peripherals, this is the easiest implementation.
const NvOdmPeripheralConnectivity *
NvOdmPeripheralGetGuid(NvU64 SearchGuid)
{
    const NvOdmPeripheralConnectivity *pAllPeripherals;
    NvU32 NumPeripherals;
    NvU32 i;

    pAllPeripherals = NvApGetAllPeripherals(&NumPeripherals);

    if (!pAllPeripherals || !NumPeripherals) {
        return NULL;
    }

    for (i=0; i<NumPeripherals; i++) {
        if (SearchGuid == pAllPeripherals[i].Guid) {
            return &pAllPeripherals[i];
        }
    }

    return NULL;
}

static NvBool
IsBusMatch(
    const NvOdmPeripheralConnectivity *pPeriph,
    const NvOdmPeripheralSearch *pSearchAttrs,
    const NvU32 *pSearchVals,
    NvU32 offset,
    NvU32 NumAttrs)
{
    NvU32 i, j;
    NvBool IsMatch = NV_FALSE;

    for (i=0; i<pPeriph->NumAddress; i++)
    {
        j = offset;
        do
        {
            switch (pSearchAttrs[j])
            {
            case NvOdmPeripheralSearch_IoModule:
                IsMatch = (pSearchVals[j] == (NvU32)(pPeriph->AddressList[i].Interface));
                break;
            case NvOdmPeripheralSearch_Address:
                IsMatch = (pSearchVals[j] == pPeriph->AddressList[i].Address);
                break;
            case NvOdmPeripheralSearch_Instance:
                IsMatch = (pSearchVals[j] == pPeriph->AddressList[i].Instance);
                break;
            case NvOdmPeripheralSearch_PeripheralClass:
            default:
                NV_ASSERT(!"Bad Query!");
                break;
            }
            j++;
        } while (IsMatch && j<NumAttrs && pSearchAttrs[j]!=NvOdmPeripheralSearch_IoModule);

        if (IsMatch)
        {
            return NV_TRUE;
        }
    }
    return NV_FALSE;
}

static NvBool
IsPeripheralMatch(
    const NvOdmPeripheralConnectivity *pPeriph,
    const NvOdmPeripheralSearch *pSearchAttrs,
    const NvU32 *pSearchVals,
    NvU32 NumAttrs)
{
    NvU32 i;
    NvBool IsMatch = NV_TRUE;

    for (i=0; i<NumAttrs && IsMatch; i++)
    {
        switch (pSearchAttrs[i])
        {
        case NvOdmPeripheralSearch_PeripheralClass:
            IsMatch = (pSearchVals[i] == (NvU32)(pPeriph->Class));
            break;
        case NvOdmPeripheralSearch_IoModule:
            IsMatch = IsBusMatch(pPeriph, pSearchAttrs, pSearchVals, i, NumAttrs);
            break;
        case NvOdmPeripheralSearch_Address:
        case NvOdmPeripheralSearch_Instance:
            // In correctly-formed searches, these parameters will be parsed by
            // IsBusMatch, so we ignore them here.
            break;
        default:
            NV_ASSERT(!"Bad search attribute!");
            break;
        }
    }
    return IsMatch;
}

NvU32
NvOdmPeripheralEnumerate(
    const NvOdmPeripheralSearch *pSearchAttrs,
    const NvU32 *pSearchVals,
    NvU32 NumAttrs,
    NvU64 *pGuidList,
    NvU32 NumGuids)
{
    const NvOdmPeripheralConnectivity *pAllPeripherals;
    NvU32 NumPeripherals;
    NvU32 Matches;
    NvU32 i;

    pAllPeripherals = NvApGetAllPeripherals(&NumPeripherals);

    if (!pAllPeripherals || !NumPeripherals)
    {
        return 0;
    }

    if (!pSearchAttrs || !pSearchVals)
    {
        NumAttrs = 0;
    }

    for (i=0, Matches=0; i<NumPeripherals && (Matches < NumGuids || !pGuidList); i++)
    {
        if ( !NumAttrs || IsPeripheralMatch(&pAllPeripherals[i], pSearchAttrs, pSearchVals, NumAttrs) )
        {
            if (pGuidList)
                pGuidList[Matches] = pAllPeripherals[i].Guid;

            Matches++;
        }
    }

    return Matches;
}


static NvOdmI2cStatus
NvOdmPeripheralI2cRead8(
    NvOdmServicesI2cHandle hOdmI2c,
    NvU8 I2cAddr,
    NvU8 Offset,
    NvU8 *pData)
{
    NvU8 ReadBuffer[1];
    NvOdmI2cStatus Error;
    NvOdmI2cTransactionInfo TransactionInfo;

    ReadBuffer[0] = Offset;

    TransactionInfo.Address = I2cAddr;
    TransactionInfo.Buf = ReadBuffer;
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 1;

    Error = NvOdmI2cTransaction(
        hOdmI2c, &TransactionInfo, 1, NVODM_QUERY_I2C_CLOCK_SPEED, NV_WAIT_INFINITE);
    if (Error != NvOdmI2cStatus_Success)
    {
        return Error;
    }

    NvOdmOsMemset(ReadBuffer, 0, sizeof(ReadBuffer));

    TransactionInfo.Address = (I2cAddr | 0x1);
    TransactionInfo.Buf = ReadBuffer;
    TransactionInfo.Flags = 0;
    TransactionInfo.NumBytes = 1;

    // Read data from ROM at the specified offset
    Error = NvOdmI2cTransaction(
        hOdmI2c, &TransactionInfo, 1, NVODM_QUERY_I2C_CLOCK_SPEED, NV_WAIT_INFINITE);
    if (Error != NvOdmI2cStatus_Success)
        return Error;

    *pData = ReadBuffer[0];
    return Error;
}

static NvOdmI2cStatus
NvOdmPeripheralI2cWrite8(
    NvOdmServicesI2cHandle hOdmI2c,
    NvU32 I2cAddr,
    NvU8 Offset,
    NvU8 Data)
{
    NvU8 WriteBuffer[2];
    NvOdmI2cStatus Error;
    NvOdmI2cTransactionInfo TransactionInfo;

    WriteBuffer[0] = Offset & 0xFF;
    WriteBuffer[1] = Data & 0xFF;

    TransactionInfo.Address = I2cAddr;
    TransactionInfo.Buf = &WriteBuffer[0];
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 2;

    Error = NvOdmI2cTransaction(
        hOdmI2c, &TransactionInfo, 1, NVODM_QUERY_I2C_CLOCK_SPEED, NV_WAIT_INFINITE);

    return Error;
}

static NvBool
NvOdmPeripheralReadPartNumber(
    NvOdmServicesI2cHandle hOdmI2c,
    NvU8 EepromInst,
    NvOdmBoardInfoRev1 *pExtendedBoardInfo)
{
    NvOdmI2cStatus Error;
    NvU32 i;
    NvU8 I2cAddr, Offset;
    NvU8 ReadBuffer[sizeof(NvOdmBoardInfoRev1)];

    NvOdmOsMemset(ReadBuffer, 0, sizeof(ReadBuffer));

    // EepromInst*2, since 7-bit addressing
    I2cAddr = NVODM_QUERY_I2C_EEPROM_ADDRESS + (EepromInst << 1);

    /**
     * Offset to the board number entry in EEPROM.
     */
    Offset = NVODM_QUERY_BOARD_HEADER_START;
#ifndef BOOT_MINIMAL_BL
    for (i=0; i<sizeof(NvOdmBoardInfoRev1); i++)
    {
        Error = NvOdmPeripheralI2cRead8(
            hOdmI2c, I2cAddr, Offset+i, (NvU8 *)&ReadBuffer[i]);
        if (Error != NvOdmI2cStatus_Success)
            return NV_FALSE;
        NvOdmOsMemcpy(pExtendedBoardInfo, &ReadBuffer[0], sizeof(NvOdmBoardInfoRev1));
    }
#else
        NvOdmOsMemcpy(pExtendedBoardInfo, &ReadBuffer[0], sizeof(NvOdmBoardInfoRev1));
    if (EepromInst == 0x6)
        NvOdmOsMemcpy(pExtendedBoardInfo, (NvU8 *)(&g_BoardInfo), sizeof(NvOdmBoardInfoRev1));
    if (EepromInst == 0x5)
        NvOdmOsMemcpy(pExtendedBoardInfo, (NvU8 *)(&g_PmuBoardId), sizeof(NvOdmBoardInfoRev1));
    if (EepromInst == 0x1)
        NvOdmOsMemcpy(pExtendedBoardInfo, (NvU8 *)(&g_DisplayBoardId), sizeof(NvOdmBoardInfoRev1));
#endif
    return NV_TRUE;
}

static NvBool
NvOdmEnableEepromRail(void)
{
    NvOdmI2cStatus Error;
    NvOdmServicesI2cHandle hOdmI2c = NULL;
    NvOdmBoardInfoRev1 PmuBoardInfo;

    hOdmI2c = NvOdmI2cOpen(NvOdmIoModule_I2c, NVODM_QUERY_I2C_INSTANCE_ID);
    if (!hOdmI2c)
        return NV_FALSE;
    NvOdmPeripheralReadPartNumber(hOdmI2c, PMU_EEPROM_INSTANCE_ID, &PmuBoardInfo);
    NvOdmI2cClose(hOdmI2c);

    // Enable display board eeprom rail
    switch (PmuBoardInfo.BoardID)
    {
        case PMU_BOARD_E1733:
        case PMU_BOARD_E1734:
            // Configure TCA6408 GPIO6 to enable VDD_LCD_1V8
            if (!Tca6408Open(NVODM_QUERY_I2C_INSTANCE_ID))
                return NV_FALSE;
            Tca6408SetDirectionOutput(TCA6408_PIN_INDEX(6), 1);
            Tca6408Close();
            break;

        case PMU_BOARD_E1735:
            // Configure TPS65913 GPIO7 to enable VDD_LCD_1V8
            hOdmI2c = NvOdmI2cOpen(NvOdmIoModule_I2c, PMU_I2C_INSTANCE_ID);
            if (!hOdmI2c)
                return NV_FALSE;
            Error = NvOdmPeripheralI2cWrite8(
                            hOdmI2c, TPS65913_I2C_SLAVE_ADDR,  0xFB, 0x00);
            if (Error != NvOdmI2cStatus_Success)
                goto Failed;
            Error = NvOdmPeripheralI2cWrite8(
                            hOdmI2c, TPS65913_I2C_SLAVE_ADDR2, 0x81, 0x80);
            if (Error != NvOdmI2cStatus_Success)
                goto Failed;
            Error = NvOdmPeripheralI2cWrite8(
                            hOdmI2c, TPS65913_I2C_SLAVE_ADDR2, 0x82, 0x80);
            if (Error != NvOdmI2cStatus_Success)
                goto Failed;
            NvOdmI2cClose(hOdmI2c);
            break;

        default:
            break;
    }

    return NV_TRUE;

Failed:
    NvOdmI2cClose(hOdmI2c);
    return NV_FALSE;
}

NvError GetBctAuxInfo(NvBctAuxInfo *pAuxInfo)
{
    NvBctHandle hBct = NULL;
    NvU32 Size, Instance;
    NvError e = NvSuccess;
    NvU8 *buffer = NULL;

    if (pAuxInfo == NULL)
    {
        e = NvError_InvalidAddress;
        return e;
    }
    Size = 0;
    Instance = 0;

    NV_CHECK_ERROR_CLEANUP(NvBctInit(&Size, NULL, &hBct));

    Size = 0;
    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(hBct, NvBctDataType_AuxDataAligned,
            &Size, &Instance, NULL)
    );

    buffer = NvOsAlloc(Size);
    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(hBct, NvBctDataType_AuxDataAligned,
            &Size, &Instance, buffer)
    );
    NvOsMemcpy((NvU8 *)pAuxInfo, buffer, sizeof(NvBctAuxInfo));

fail:

    if (buffer)
        NvOsFree(buffer);

    NvBctDeinit(hBct);
    return e;
}

NvBool
NvOdmPeripheralGetBoardInfo(
    NvU16 BoardId,
    NvOdmBoardInfo *pBoardInfo)
{
    static NvBctAuxInfo auxInfo;

    if (auxInfo.NCTBoardInfo.proc_board_id == 0) {
        if (GetBctAuxInfo(&auxInfo) == NvSuccess) {
           NvOdmOsPrintf("GetBctAuxInfo OK!\n");
           NvOdmOsPrintf("procid:%d:,pmu id:%d\n",
                   auxInfo.NCTBoardInfo.proc_board_id,
                   auxInfo.NCTBoardInfo.pmu_board_id);
       }
        else {
           NvOdmOsPrintf("GetBctAuxInfo Failed!\n");
           return NV_FALSE;

       }

   }
    if (BoardId == 0) {
        pBoardInfo->BoardID  = auxInfo.NCTBoardInfo.proc_board_id;
        pBoardInfo->SKU      = auxInfo.NCTBoardInfo.proc_sku;
        pBoardInfo->Fab      = auxInfo.NCTBoardInfo.proc_fab;
        pBoardInfo->Revision = 0;
        pBoardInfo->MinorRevision = 0;
    } else if (IS_PMU_BOARD(BoardId)) {
        if (BoardId == auxInfo.NCTBoardInfo.pmu_board_id) {
            pBoardInfo->BoardID  = auxInfo.NCTBoardInfo.pmu_board_id;
            pBoardInfo->SKU      = auxInfo.NCTBoardInfo.pmu_sku;
            pBoardInfo->Fab      = auxInfo.NCTBoardInfo.pmu_fab;
            pBoardInfo->Revision = 0;
            pBoardInfo->MinorRevision = 0;
            NvOdmOsPrintf("%s: got correct PMU board:%d!\n", __func__, BoardId);
        } else {
            NvOdmOsPrintf("%s: un-match PMU board: BoardId:%d,pmu_board_id:%d!\n",
                   __func__, BoardId, auxInfo.NCTBoardInfo.pmu_board_id);
	    return NV_FALSE;

       }
		    } else {
            NvOdmOsPrintf("%s: wrong PMU board:%d!\n", __func__, BoardId);
            return NV_FALSE;
    }
    return NV_TRUE;
}

static NvBool GetAudioCodecName(char *pCodecName, int MaxNameLen)
{
    NvOdmServicesI2cHandle hOdmI2c = NULL;

    hOdmI2c = NvOdmI2cOpen(NvOdmIoModule_I2c, I2C_AUDIOCODEC_ID);
    if (!hOdmI2c) {
         NvOdmOsPrintf("%s(): The I2c service can not be open\n");
         return NV_FALSE;
    }

    NvOdmOsMemset(pCodecName, 0, MaxNameLen);

    // Read codec and fill the codec name
    NvOdmOsMemcpy(pCodecName, "rt5640", 6);

    NvOdmI2cClose(hOdmI2c);
    return NV_TRUE;
}

#ifdef NV_USE_NCT
static NvBool readDisplayBoardIDFromNCT(NvOdmDisplayBoardInfo *pDisplayInfo)
{
    nct_item_type buf;

    if (NvNctReadItem(NCT_ID_BOARD_INFO, &buf) == NvSuccess) {
        pDisplayInfo->BoardInfo.BoardID = buf.board_info.display_board_id;
        pDisplayInfo->BoardInfo.SKU = buf.board_info.display_sku;
        pDisplayInfo->BoardInfo.Fab = buf.board_info.display_fab;
    }
    else if (NvNctReadItem(NCT_ID_LCD_ID, &buf) == NvSuccess) {
        pDisplayInfo->BoardInfo.BoardID = buf.lcd_id.id;
        pDisplayInfo->BoardInfo.SKU = 0;
        pDisplayInfo->BoardInfo.Fab = 0;
    }
    else
        return NV_FALSE;

    pDisplayInfo->DisplayType = NvOdmBoardDisplayType_DSI;
    pDisplayInfo->IsPassBoardInfoToKernel = NV_TRUE;
    pDisplayInfo->BoardInfo.Revision = 0;
    pDisplayInfo->BoardInfo.MinorRevision = 0;
    return NV_TRUE;
}

static NvBool readTouchBoardIDFromNCT(NvOdmTouchInfo *pTouchInfo)
{
    NvOdmDisplayBoardInfo DisplayInfo;

    // Neither NCT_ID_TOUCH_ID or NCT_ID_LCD_ID gives enough information about
    // touch panels. Use full display information.

    if (readDisplayBoardIDFromNCT(&DisplayInfo)) {
        if (DisplayInfo.BoardInfo.BoardID == BOARD_E1937) {
            if (DisplayInfo.BoardInfo.SKU == 1100) {
                pTouchInfo->TouchVendorId = NvOdmBoardTouchVendorId_None;
            } else {
                pTouchInfo->TouchVendorId = NvOdmBoardTouchVendorId_Maxim;
                pTouchInfo->TouchPanelId = NvOdmBoardTouchPanelId_TN8;
            }
        }
        else if (DisplayInfo.BoardInfo.BoardID == BOARD_E1807) {
            pTouchInfo->TouchVendorId = NvOdmBoardTouchVendorId_None;
        }
        else if (DisplayInfo.BoardInfo.BoardID == BOARD_E1549) {
            pTouchInfo->TouchVendorId = NvOdmBoardTouchVendorId_Maxim;
            pTouchInfo->TouchPanelId = NvOdmBoardTouchPanelId_TN7;
        }
        return NV_TRUE;
    }
    return NV_FALSE;
}
#endif

NvBool
NvOdmQueryGetBoardModuleInfo(
    NvOdmBoardModuleType mType,
    void *BoardModuleData,
    int DataLen)
{
    NvOdmBoardInfo *procBoardInfo;
    NvOdmBoardInfo BoardInfo;

    NvOdmPmuBoardInfo *pPmuInfo;
    NvOdmMaxCpuCurrentInfo *pCpuCurrInfo;
    NvOdmDisplayPanelInfo *pDisplayPanelInfo;
    NvOdmTouchInfo *pTouchInfo;
    NvOdmPowerSupplyInfo *pSupplyInfo;
    NvU32 CustOpt;

    NvOdmDisplayBoardInfo *pDisplayInfo;
    NvOdmAudioCodecBoardInfo *pAudioCodecBoard = NULL;

    NvU16 PmuBoardIdList[] = {PMU_BOARDS_BOARD_INFO};
    unsigned int i;
    NvBool status;

    switch (mType)
    {
        case NvOdmBoardModuleType_ProcessorBoard:
            if (DataLen != sizeof(NvOdmBoardInfo))
                return NV_FALSE;

            status = NvOdmPeripheralGetBoardInfo(0, BoardModuleData);
            if (status)
            {
                procBoardInfo = (NvOdmBoardInfo *)BoardModuleData;
                NvOdmOsPrintf("The proc BoardInfo: 0x%04x:0x%04x:0x%02x:0x%02x:0x%02x\n",
                             procBoardInfo->BoardID, procBoardInfo->SKU,
                             procBoardInfo->Fab, procBoardInfo->Revision, procBoardInfo->MinorRevision);
            }
            return status;

        case NvOdmBoardModuleType_PowerSupply:
            pSupplyInfo = (NvOdmPowerSupplyInfo *)BoardModuleData;

            CustOpt = GetBctKeyValue();
            switch (NV_DRF_VAL(TEGRA_DEVKIT, BCT_CUSTOPT, POWERSUPPLY, CustOpt))
            {
                case TEGRA_DEVKIT_BCT_CUSTOPT_0_POWERSUPPLY_BATTERY:
                    pSupplyInfo->SupplyType = NvOdmBoardPowerSupply_Battery;
                    return NV_TRUE;

                default:
                    pSupplyInfo->SupplyType = NvOdmBoardPowerSupply_Adapter;
                    return NV_TRUE;
            }
            break;
        case NvOdmBoardModuleType_Touch:
            pTouchInfo = (NvOdmTouchInfo *)BoardModuleData;
            pTouchInfo->TouchVendorId = NvOdmBoardTouchVendorId_Raydium;
            pTouchInfo->TouchFeature = NvOdmTouchEnableNone;
#ifdef NV_USE_NCT
            status = readTouchBoardIDFromNCT(pTouchInfo);
            if (status)
                return NV_TRUE;
#endif
            if (NvOdmPeripheralGetBoardInfo(BOARD_E1549, &BoardInfo) ||
                NvOdmPeripheralGetBoardInfo(BOARD_E1762, &BoardInfo)) {
                pTouchInfo->TouchVendorId = NvOdmBoardTouchVendorId_Maxim;
                pTouchInfo->TouchPanelId = NvOdmBoardTouchPanelId_TN7;
            } else if (NvOdmPeripheralGetBoardInfo(BOARD_P1761, &BoardInfo)) {
                /* No touch for P1761 with 12x8 display */
                if (s_ExtendedBoardModuleTable[0].DisplayConfigs == 0)
                    pTouchInfo->TouchVendorId = NvOdmBoardTouchVendorId_None;
                else {
                    pTouchInfo->TouchVendorId = NvOdmBoardTouchVendorId_Maxim;
                    pTouchInfo->TouchPanelId = NvOdmBoardTouchPanelId_TN8;
                }
            }
            return NV_TRUE;

        case NvOdmBoardModuleType_PmuBoard:
            if (DataLen != sizeof(NvOdmPmuBoardInfo))
                return NV_FALSE;

            // Look for E1731
            for(i = 0; i < NV_ARRAY_SIZE(PmuBoardIdList); i++)
                if(NvOdmPeripheralGetBoardInfo(PmuBoardIdList[i], &BoardInfo))
                    break;

            ((NvOdmPmuBoardInfo *) BoardModuleData)->BoardInfo = BoardInfo;

            pPmuInfo = (NvOdmPmuBoardInfo *)BoardModuleData;

            /* Default is 0 */
            pPmuInfo->core_edp_mv = 1150;
            pPmuInfo->core_edp_ma = 4000;
            pPmuInfo->isPassBoardInfoToKernel = NV_TRUE;

            return NV_TRUE;

        case NvOdmBoardModuleType_DisplayBoard:
            if (DataLen != sizeof(NvOdmDisplayBoardInfo))
                return NV_FALSE;
            pDisplayInfo = BoardModuleData;
#ifdef NV_USE_NCT
            status = readDisplayBoardIDFromNCT(pDisplayInfo);
            if (status)
                return NV_TRUE;
#endif
            if (NvOdmQueryIsHdmiPrimaryDisplay())
            {
                pDisplayInfo->DisplayType = NvOdmBoardDisplayType_HDMI;
                pDisplayInfo->IsPassBoardInfoToKernel = NV_FALSE;
            } else
            {
                for (i = 0; i < NV_ARRAY_SIZE(displayBoardTable); ++i)
                {
                    status = NvOdmPeripheralGetBoardInfo(displayBoardTable[i].BoardInfo.BoardID, &BoardInfo);
                    if (status)
                    {
                        pDisplayInfo->DisplayType = displayBoardTable[i].DisplayType;
                        pDisplayInfo->IsPassBoardInfoToKernel = displayBoardTable[i].IsPassBoardInfoToKernel;

                        pDisplayInfo->BoardInfo.BoardID = BoardInfo.BoardID;
                        pDisplayInfo->BoardInfo.SKU = BoardInfo.SKU;
                        pDisplayInfo->BoardInfo.Fab = BoardInfo.Fab;
                        pDisplayInfo->BoardInfo.Revision = BoardInfo.Revision;
                        pDisplayInfo->BoardInfo.MinorRevision = BoardInfo.MinorRevision;
                        return NV_TRUE;
                    }
                }
                /* Default is LVDS */
                pDisplayInfo->DisplayType = NvOdmBoardDisplayType_LVDS;
            }
            return NV_TRUE;

       case NvOdmBoardModuleType_AudioCodec:
           pAudioCodecBoard = (NvOdmAudioCodecBoardInfo *)BoardModuleData;
           return GetAudioCodecName(pAudioCodecBoard->AudioCodecName,
                       sizeof(pAudioCodecBoard->AudioCodecName));

       case NvOdmBoardModuleType_CpuCurrent:
            pCpuCurrInfo = BoardModuleData;
            /* Default is 0 */
            pCpuCurrInfo->MaxCpuCurrentmA = 0;

            // Get the board Id
            status = NvOdmPeripheralGetBoardInfo(0, &BoardInfo);

            return NV_FALSE;
        case NvOdmBoardModuleType_DisplayPanel:
            pDisplayPanelInfo = (NvOdmDisplayPanelInfo *)BoardModuleData;
            pDisplayPanelInfo->DisplayPanelId = s_ExtendedBoardModuleTable[0].DisplayConfigs;
            return NV_TRUE;

        default:
            break;
    }
    return NV_FALSE;
}

NvError
NvOdmQueryGetUpatedBoardModuleInfo(
    NvOdmBoardModuleType mType,
    NvU16 *BoardModuleData,
    NvOdmUpdatedBoardModuleInfoType datatype)
{
    NvU16 BoardId = 0;
    NvOdmBoardInfo BoardInfo;
    NvU8 i;
    NvBool status = NV_FALSE;

    if (!s_ReadBoardInfoDone)
    {
        NvOdmPeripheralGetBoardInfo(0, &BoardInfo); //Fill the data
        s_ReadBoardInfoDone = NV_TRUE;
    }

    switch(mType)
    {
        case NvOdmBoardModuleType_ProcessorBoard:
        case NvOdmBoardModuleType_PmuBoard:
        case NvOdmBoardModuleType_CpuCurrent:
            NvOdmPeripheralGetBoardInfo(0, &BoardInfo); //Get the correct processor data
            BoardId = BoardInfo.BoardID;
            break;
        case NvOdmBoardModuleType_AudioCodec:
            return NvError_NotSupported; //No BoardInfo exists for audio codec
        case NvOdmBoardModuleType_DisplayBoard:
            for (i = 0; i < NV_ARRAY_SIZE(displayBoardTable); ++i)
            {
                status = NvOdmPeripheralGetBoardInfo(displayBoardTable[i].BoardInfo.BoardID, &BoardInfo);
                if (status)
                    BoardId = displayBoardTable[i].BoardInfo.BoardID;
            }
            if (!status)
                return NvError_NotSupported;
            break;
        default:
            return NV_FALSE;
    }

    for (i = 0;  i < NumBoards; i++)
    {
        if (s_ExtendedBoardModuleTable[i].BoardID == BoardId)
            break;
    }

    if (i == NumBoards)
        return NvError_NotSupported;

    switch (datatype)
    {
    case NvOdmUpdatedBoardModuleInfoType_Version:
        *BoardModuleData  = s_ExtendedBoardModuleTable[i].Version;
        return NvError_Success;

    case NvOdmUpdatedBoardModuleInfoType_Size:
        *BoardModuleData  = s_ExtendedBoardModuleTable[i].Size;
        return NvError_Success;

    case NvOdmUpdatedBoardModuleInfoType_MemType:
        *BoardModuleData  = s_ExtendedBoardModuleTable[i].MemType;
        return NvError_Success;

    case NvOdmUpdatedBoardModuleInfoType_PowerConfig:
        *BoardModuleData  = s_ExtendedBoardModuleTable[i].PowerConfig;
        return NvError_Success;

    case NvOdmUpdatedBoardModuleInfoType_MiscConfig:
        *BoardModuleData  = s_ExtendedBoardModuleTable[i].MiscConfig;
        return NvError_Success;

    case NvOdmUpdatedBoardModuleInfoType_ModemBands:
        *BoardModuleData  = s_ExtendedBoardModuleTable[i].ModemBands;
        return NvError_Success;

    case NvOdmUpdatedBoardModuleInfoType_TouchScreenConfigs:
        *BoardModuleData  = s_ExtendedBoardModuleTable[i].TouchScreenConfigs;
        return NvError_Success;

    case NvOdmUpdatedBoardModuleInfoType_DisplayConfigs:
        *BoardModuleData  = s_ExtendedBoardModuleTable[i].DisplayConfigs;
        return NvError_Success;

    default:
        break;
    }
    return NvError_NotSupported;
}
