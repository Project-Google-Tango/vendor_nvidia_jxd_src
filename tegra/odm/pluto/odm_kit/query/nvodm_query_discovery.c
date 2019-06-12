/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
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

#include "nvcommon.h"
#include "nvodm_query_gpio.h"
#include "nvodm_modules.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query_kbc_gpio_def.h"
#include "subboards/nvodm_query_discovery_e1580_addresses.h"
#include "tca6408.h"
#include "nvodm_imager_guid.h"
#include "tegra_devkit_custopt.h"
#include "nvodm_keylist_reserved.h"
#include "nvbct.h"
#include "nvrm_drf.h"
#include "nvddk_fuse.h"

#define NVODM_QUERY_I2C_INSTANCE_ID      0
#define NVODM_QUERY_MAX_EEPROMS          8      // Maximum number of EEPROMs per bus segment
#define NVODM_QUERY_I2C_EEPROM_ADDRESS   0xA0   // I2C device base address for EEPROM (7'h50)
#define NVODM_QUERY_BOARD_HEADER_START   0x00   // Offset to Part Number in EERPOM
#define NVODM_QUERY_I2C_CLOCK_SPEED      100    // kHz
#define I2C_AUDIOCODEC_ID                4
#define IO_EXPANDER_I2C_INSTANCE         1

// TODO: Update this with correct value
#define BOARD_E1580            0x062c
#define BOARD_E1575            0x0627
#define BOARD_E1577            0x0629

#define DISPLAY_BOARD_E1582    0x062E
// display board ids
#define BOARD_E1582   0x062e
#define BOARD_E1605   0x0645

#define TPS65914_PMUGUID NV_ODM_GUID('t','p','s','6','5','9','1','4')
#define TPS65914_SW_REV 0xb7
// The following are used to store entries read from EEPROMs at runtime.
static NvOdmBoardInfoRev1 s_ExtendedBoardModuleTable[NVODM_QUERY_MAX_EEPROMS];
static const NvOdmIoAddress s_SdioAddresses[] =
{
    { NvOdmIoModule_Sdio, 0x0, 0x0, 0 },
    { NvOdmIoModule_Sdio, 0x2, 0x0, 0 },
    { NvOdmIoModule_Sdio, 0x3, 0x0, 0 },
};

static const NvOdmIoAddress s_Lg500PanelAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_AVDD_LCD, 0x00},
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_VDD_LCD, 0x00},
};

static const NvOdmIoAddress s_NovatekSmartPanelAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_AVDD_LCD, 0x00},
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_VDD_LCD, 0x00},
};

static const NvOdmIoAddress s_Sharp1080pPanelAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_AVDD_LCD, 0x00},
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_VDD_LCD, 0x00},
};
static NvOdmPeripheralConnectivity s_Peripherals[] =
{
#include "subboards/nvodm_query_discovery_e1580_peripherals.h"
#include "subboards/nvodm_query_discovery_e1625_peripherals.h"
    //  LVDS LCD Display
    {
        NV_ODM_GUID('L','V','D','S','W','S','V','G'),   // LVDS WSVGA panel
        s_LvdsDisplayAddresses,
        NV_ARRAY_SIZE(s_LvdsDisplayAddresses),
        NvOdmPeripheralClass_Display
    },

    //  Sdio module
    {
        NV_ODM_GUID('s','d','i','o','_','m','e','m'),
        s_SdioAddresses,
        NV_ARRAY_SIZE(s_SdioAddresses),
        NvOdmPeripheralClass_Other,
    },

    {
        NV_ODM_GUID('l','g','5','0','0','d','s','i'),
        s_Lg500PanelAddresses,
        NV_ARRAY_SIZE(s_Lg500PanelAddresses),
        NvOdmPeripheralClass_Display,
    },
    {
        NV_ODM_GUID('j','d','i','_','_','d','s','i'),
        s_NovatekSmartPanelAddresses,
        NV_ARRAY_SIZE(s_NovatekSmartPanelAddresses),
        NvOdmPeripheralClass_Display,
    },
    {
        NV_ODM_GUID('s','p','L','S','-','X','0','1'),
        s_Sharp1080pPanelAddresses,
        NV_ARRAY_SIZE(s_Sharp1080pPanelAddresses),
        NvOdmPeripheralClass_Display,
    },
};

static NvBool s_ReadBoardInfoDone = NV_FALSE;
static NvU8 NumBoards = 0;

/* Display Board Table */
static NvOdmDisplayBoardInfo displayBoardTable[] = {
    {
        NvOdmBoardDisplayType_DSI,
        NV_TRUE,
        {
            BOARD_E1605,
        }
    },
    {
        NvOdmBoardDisplayType_DSI,
        NV_TRUE,
        {
            BOARD_E1582,
        }
    },
    {
        NvOdmBoardDisplayType_DSI,
        NV_TRUE,
        {
            BOARD_E1577,
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

    for (i=0; i<sizeof(NvOdmBoardInfoRev1); i++)
    {
        Error = NvOdmPeripheralI2cRead8(
            hOdmI2c, I2cAddr, Offset+i, (NvU8 *)&ReadBuffer[i]);
        if (Error != NvOdmI2cStatus_Success)
            return NV_FALSE;
    }
    NvOdmOsMemcpy(pExtendedBoardInfo, &ReadBuffer[0], sizeof(NvOdmBoardInfoRev1));
    return NV_TRUE;
}

NvBool
NvOdmPeripheralGetBoardInfo(
    NvU16 BoardId,
    NvOdmBoardInfo *pBoardInfo)
{
    NvBool RetVal = NV_FALSE;
    NvOdmServicesI2cHandle hOdmI2c = NULL;
    NvU8 EepromInst, CurrentBoard;
    NvU16 MainProcessorBoard[] = {BOARD_E1580, BOARD_E1575, BOARD_E1577};
    NvU8 ProcBoardCount = 0;

    if (!s_ReadBoardInfoDone)
    {
        hOdmI2c = NvOdmI2cOpen(NvOdmIoModule_I2c, NVODM_QUERY_I2C_INSTANCE_ID);
        if (!hOdmI2c)
        {
            // Exit
            pBoardInfo = NULL;
            return NV_FALSE;
        }
        s_ReadBoardInfoDone = NV_TRUE;

        for (EepromInst=0; EepromInst < NVODM_QUERY_MAX_EEPROMS; EepromInst++)
        {
            RetVal = NvOdmPeripheralReadPartNumber(
                  hOdmI2c, EepromInst, &s_ExtendedBoardModuleTable[NumBoards]);

            if (RetVal == NV_TRUE) {
                NvOdmOsPrintf("0x%02x BoardInfo: 0x%04x:0x%04x:0x%04x:0x%04x:0x%02x:0x%02x:0x%02x:0x%02x:0x%02x:0x%02x:0x%02x:0x%02x:0x%02x\n ",
                          EepromInst, s_ExtendedBoardModuleTable[NumBoards].Version, s_ExtendedBoardModuleTable[NumBoards].Size,
                          s_ExtendedBoardModuleTable[NumBoards].BoardID, s_ExtendedBoardModuleTable[NumBoards].SKU,
                          s_ExtendedBoardModuleTable[NumBoards].Fab, s_ExtendedBoardModuleTable[NumBoards].Revision,
                          s_ExtendedBoardModuleTable[NumBoards].MinorRevision, s_ExtendedBoardModuleTable[NumBoards].MemType,
                          s_ExtendedBoardModuleTable[NumBoards].PowerConfig, s_ExtendedBoardModuleTable[NumBoards].MiscConfig,
                          s_ExtendedBoardModuleTable[NumBoards].ModemBands, s_ExtendedBoardModuleTable[NumBoards].TouchScreenConfigs,
                          s_ExtendedBoardModuleTable[NumBoards].DisplayConfigs);
                NumBoards++;
            }
        }

        NvOdmI2cClose(hOdmI2c);
    }

    if (NumBoards)
    {
       if (BoardId)
       {
          // Linear search for given BoardId; if found, return entry
          for (CurrentBoard=0; CurrentBoard < NumBoards; CurrentBoard++)
          {
             if (s_ExtendedBoardModuleTable[CurrentBoard].BoardID == BoardId)
             {
                // Match found
                pBoardInfo->BoardID  = s_ExtendedBoardModuleTable[CurrentBoard].BoardID;
                pBoardInfo->SKU      = s_ExtendedBoardModuleTable[CurrentBoard].SKU;
                pBoardInfo->Fab      = s_ExtendedBoardModuleTable[CurrentBoard].Fab;
                pBoardInfo->Revision = s_ExtendedBoardModuleTable[CurrentBoard].Revision;
                pBoardInfo->MinorRevision = s_ExtendedBoardModuleTable[CurrentBoard].MinorRevision;
                return NV_TRUE;
               }
            }
         }
         else
         {
            for (CurrentBoard=0; CurrentBoard < NumBoards; CurrentBoard++)
            {
                for (ProcBoardCount = 0; ProcBoardCount < NV_ARRAY_SIZE(MainProcessorBoard); ++ProcBoardCount)
                {
                    if (s_ExtendedBoardModuleTable[CurrentBoard].BoardID == MainProcessorBoard[ProcBoardCount])
                    {
                        // Match found
                        pBoardInfo->BoardID  = s_ExtendedBoardModuleTable[CurrentBoard].BoardID;
                        pBoardInfo->SKU      = s_ExtendedBoardModuleTable[CurrentBoard].SKU;
                        pBoardInfo->Fab      = s_ExtendedBoardModuleTable[CurrentBoard].Fab;
                        pBoardInfo->Revision = s_ExtendedBoardModuleTable[CurrentBoard].Revision;
                        pBoardInfo->MinorRevision = s_ExtendedBoardModuleTable[CurrentBoard].MinorRevision;
                        return NV_TRUE;
                    }
               }
            }
        }
    }
    return NV_FALSE;
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

NvBool
NvOdmQueryGetBoardModuleInfo(
    NvOdmBoardModuleType mType,
    void *BoardModuleData,
    int DataLen)
{
    NvOdmBoardInfo *procBoardInfo;
    NvOdmBoardInfo BoardInfo;
    NvU32 CustOpt;
    NvOdmPowerSupplyInfo *pSupplyInfo;

    NvOdmPmuBoardInfo *pPmuInfo;
    NvOdmMaxCpuCurrentInfo *pCpuCurrInfo;

    NvOdmDisplayBoardInfo *pDisplayInfo;
    NvOdmDisplayPanelInfo *pDisplayPanelInfo;
    NvOdmAudioCodecBoardInfo *pAudioCodecBoard = NULL;

    NvOdmI2cStatus Error;
    NvOdmServicesI2cHandle hOdmI2c = NULL;
    static NvOdmServicesPmuHandle hPmu = NULL;

    unsigned int i;
    NvBool status;
    NvU32 value;
    NvU8 sw_rev_val = 0;
    NvU32 Instance = 0;
    NvU32 Addr = 0;
    NvU32 SKU;
    NvU32 speedo0;
    NvU32 SettlingTime = 0;
    NvU32 size = sizeof(NvU32);
    NvError e = NvSuccess;

    const NvOdmPeripheralConnectivity *pConnectivity =
                            NvOdmPeripheralGetGuid(TPS65914_PMUGUID);
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
                     NvOdmOsPrintf("reached default case \n");
                     pSupplyInfo->SupplyType = NvOdmBoardPowerSupply_Adapter;
                     return NV_TRUE;
            }
            break;

        case NvOdmBoardModuleType_PmuBoard:
            if (DataLen != sizeof(NvOdmPmuBoardInfo))
                return NV_FALSE;

            pPmuInfo = BoardModuleData;

            /* Default is 0 */
            pPmuInfo->core_edp_mv = 0;
            pPmuInfo->core_edp_ma = 0;
            pPmuInfo->isPassBoardInfoToKernel = NV_TRUE;

            // Get the board Id
            status = NvOdmPeripheralGetBoardInfo(0, &BoardInfo);

            if (pConnectivity)
            {
                if (pConnectivity->AddressList[0].Interface == NvOdmIoModule_I2c)
                {
                    Instance = pConnectivity->AddressList[0].Instance;
                    Addr = pConnectivity->AddressList[0].Address;
                }
            }
            // Get Otp Revision
            hOdmI2c = NvOdmI2cOpen(NvOdmIoModule_I2c, Instance);
            if (!hOdmI2c) {
                NvOdmOsPrintf("%s(): I2c service can not be opened\n");
                return NV_FALSE;
            }
            if (BoardInfo.BoardID == BOARD_E1580 ) {
                Error = NvOdmPeripheralI2cRead8(hOdmI2c, Addr, TPS65914_SW_REV, &sw_rev_val);
                if (Error != NvOdmI2cStatus_Success) {
                    NvOdmOsPrintf("%s(): Error in I2c READ\n");
                    return NV_FALSE;
                }
                NvOdmOsPrintf("Otp value is %x\n", sw_rev_val);
            }
            NvOdmI2cClose(hOdmI2c);
            // Get engineering design point current
            if (sw_rev_val == 0xb2)
            {
                pPmuInfo->core_edp_ma = 4000;
                pPmuInfo->core_edp_mv = 1120;
            }
            else
            {
                if ((sw_rev_val == 0xb6) || (sw_rev_val == 0xb9))
                     pPmuInfo->core_edp_ma = 6000;

                // Get engineering design point voltage
                e = NvDdkFuseGet(NvDdkFuseDataType_Sku, &SKU, &size);
                if (e == NvSuccess)
                {
                   if (SKU == 0x6 || SKU == 0x5 || SKU == 0x7)
                   {
                       e = NvDdkFuseGet(NvDdkFuseDataType_SocSpeedo0,
                                        &speedo0, &size);
                       if (e == NvSuccess)
                       {
                          if (speedo0 < 1123)
                              pPmuInfo->core_edp_mv = 1250;
                          else
                              pPmuInfo->core_edp_mv = 1170;
                       }
                   }
                   else if (SKU == 0x4 || SKU == 0x3)
                        pPmuInfo->core_edp_mv = 1250;
                   else
                        pPmuInfo->core_edp_mv = 1120;
                }
                if(status == NV_TRUE)
                   return NV_TRUE;
                //if status is false then set these dafault values
                pPmuInfo->core_edp_ma = 4000;
                pPmuInfo->core_edp_mv = 1120;

            }

            return NV_TRUE;

        case NvOdmBoardModuleType_DisplayBoard:
            if (DataLen != sizeof(NvOdmDisplayBoardInfo))
                return NV_FALSE;
            pDisplayInfo = BoardModuleData;
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
            pDisplayPanelInfo->DisplayPanelId = NvOdmBoardDisplayPanelId_Default;

            // Enable VDD_LCD_1V8 through PMU GPIO4
            // This is required to give DISP_ID_LG_1_BOE_0 signal 1.8v which sets GPIO_P3
            // high for LG panel. in the LP0 exit.
            if (!hPmu)
                hPmu = NvOdmServicesPmuOpen();
            if (!hPmu)
            {
                NvOdmOsPrintf("%s(): PMU can not open\n", __func__);
                return NV_FALSE;
            }

            /* set the rail volatage to the recommended */
            NvOdmServicesPmuSetVoltage(hPmu,
                PlutoPowerRails_EN_VDD_1V8_LCD, 1800,
                &SettlingTime );
            /* wait for rail to settle */
            NvOdmOsWaitUS(SettlingTime);

            // Check Panel id for E1852 board, otherwise default
            if (NvOdmPeripheralGetBoardInfo(DISPLAY_BOARD_E1582, &BoardInfo))
            {
                if (Tca6408Open(IO_EXPANDER_I2C_INSTANCE))
                {
                    Tca6408SetDirectionInput(TCA6408_PIN_INDEX(3));
                    value = Tca6408GetInput(TCA6408_PIN_INDEX(3));
                    Tca6408Close();
                    if (value == TCA6408_GPIO_PIN_LOW)
                        pDisplayPanelInfo->DisplayPanelId = NvOdmBoardDisplayPanelId_Sharp;
                }
            }
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
