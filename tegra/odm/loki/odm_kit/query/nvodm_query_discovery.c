/*
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
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
#include "nvodm_query_discovery_imager.h"
#include "nvodm_query_kbc_gpio_def.h"
#include "subboards/nvodm_query_discovery_e2548_addresses.h"
#include "nvodm_imager_guid.h"
#ifdef BOOT_MINIMAL_BL
#include "aos_avp_board_info.h"
#endif
#include "nvbct.h"
#include "nvbct_customer_platform_data.h"
#include "nvboot_bit.h"
#include "aos_avp_board_info.h"
#include "nvddk_sdio.h"
#include "nvddk_blockdevmgr.h"
#include "nvddk_blockdev.h"

#define NVODM_QUERY_I2C_INSTANCE_ID      0
#define NVODM_QUERY_MAX_EEPROMS          8      // Maximum number of EEPROMs per bus segment
#define NVODM_QUERY_I2C_EEPROM_ADDRESS   0xA0   // I2C device base address for EEPROM (7'h50)
#define NVODM_QUERY_BOARD_HEADER_START   0x00   // Offset to Part Number in EERPOM
#define NVODM_QUERY_I2C_CLOCK_SPEED      100    // kHz
#define I2C_AUDIOCODEC_ID                4
#define PMU_BOARDID_INDEX                0x4
#define NV_BIT_ADDRESS 0x40000000
#define NV_BDK_SD_SECTOR_SIZE       4096

static NvBoardInfo s_BoardInfo;
static NvBoardInfo s_PmuBoardInfo;
static NvBoardInfo s_DisplayBoardInfo;

/* Loki id */
#define BOARD_E2548            0x9F4
/* Thor 1.9 id */
#define BOARD_E2549            0x9F5
#define BOARD_E2530            0x9E2

/* Foster Sku id */
#define FOSTER_SKU_ID          0x384

//Add any new processor boards info here
#define  PROCESSOR_BOARDS_BOARD_INFO BOARD_E2548,BOARD_E2549, BOARD_E2530

//display boards
#define BOARD_P2534            0x9E6

//display boards fab and rev
#define BOARD_P2534_THOR_FAB    0x0
#define BOARD_P2534_THOR_REV    0x0

// Loki PMU board ids
#define PMU_BOARD_E2545        0x9F1

#define PMU_BOARDS_BOARD_INFO PMU_BOARD_E2545

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

static const NvOdmIoAddress s_VddSataAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, LokiPowerRails_Invalid, 0x00},
};

static const NvOdmIoAddress s_Lg500PanelAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, LokiPowerRails_AVDD_DSI_CSI, 0x00},
    { NvOdmIoModule_Vdd, 0x00, LokiPowerRails_AVDD_LCD, 0x00},
    { NvOdmIoModule_Vdd, 0x00, LokiPowerRails_VDD_3V3_SYS, 0x00},
};


//BQ2491X
static const NvOdmIoAddress s_Bq2419xAddresses[] =
{
    { NvOdmIoModule_I2c, 0x0,  0xD6, 0 },
};

//BQ27441
static const NvOdmIoAddress s_Bq27441Addresses[] =
{
    { NvOdmIoModule_I2c, 0x0,  0xAA, 0 },
};

//LC709203F
static const NvOdmIoAddress s_Lc709203fAddresses[] =
{
    { NvOdmIoModule_I2c, 0x0,  0x16, 0 },
};

static const NvOdmIoAddress s_JdiPanelAddresses[] =
{
    {NvOdmIoModule_Vdd, 0x00, LokiPowerRails_AVDD_DSI_CSI, 0x00},
    {NvOdmIoModule_Vdd, 0x00, LokiPowerRails_AVDD_LCD, 0x00},
    {NvOdmIoModule_Vdd, 0x00, LokiPowerRails_VDD_3V3_SYS, 0x00},
    {NvOdmIoModule_Pwm, 0x1,  0x00, 0x00},
};

static NvOdmPeripheralConnectivity s_Peripherals[] =
{

#include "subboards/nvodm_query_discovery_e2548_peripherals.h"

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
        s_JdiPanelAddresses,
        NV_ARRAY_SIZE(s_JdiPanelAddresses),
        NvOdmPeripheralClass_Display,
    },
    {
        NV_VDD_SATA_ODM_ID,
        s_VddSataAddresses,
        NV_ARRAY_SIZE(s_VddSataAddresses),
        NvOdmPeripheralClass_Other,
    },
    // Bq27441
    {
        NV_ODM_GUID('b','q','2','7','4','4','1','*'),
        s_Bq27441Addresses,
        NV_ARRAY_SIZE(s_Bq27441Addresses),
        NvOdmPeripheralClass_Other
    },
    // Lc709203f
    {
        NV_ODM_GUID('l','c','7','0','9','2','0','*'),
        s_Lc709203fAddresses,
        NV_ARRAY_SIZE(s_Lc709203fAddresses),
        NvOdmPeripheralClass_Other
    },
    //  BQ2419x
    {
        NV_ODM_GUID('b','q','2','4','1','9','x','*'),
        s_Bq2419xAddresses,
        NV_ARRAY_SIZE(s_Bq2419xAddresses),
        NvOdmPeripheralClass_Other
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
            BOARD_P2534,
        }
    }
 };

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

#if NOT_SHARED_QUERY_LIBRARY==1
NvError NvCpuGetBoardInfoFromNCT(void)
{

    NvDdkBlockDevHandle hSdbdk = NULL;
    NvDdkBlockDevInfo DeviceInfo;
    NvError ErrStatus = NvSuccess;
    NvU32 Instance = 3, i, seek = 0, InitialWidth = 140;
    NvU8 *pBuf = NULL;
    NvU32 BoardInfoStartSectorOffset = 2;
    NvU32 NCTStartSector = 3968;
    NvU32 NCTDataStartSector = NCTStartSector + (NCT_ENTRY_OFFSET/NV_BDK_SD_SECTOR_SIZE);


    pBuf = NvOsAlloc(NV_BDK_SD_SECTOR_SIZE);
    NvOsMemset(pBuf, 0, NV_BDK_SD_SECTOR_SIZE);

    ErrStatus = NvDdkBlockDevMgrInit();
    if (ErrStatus != NvError_Success)
    {
        goto fail;
    }
    ErrStatus = NvDdkBlockDevMgrDeviceOpen(NvDdkBlockDevMgrDeviceId_SDMMC,
            Instance,0,&hSdbdk);
    if (ErrStatus != NvError_Success)
    {
        goto fail;
    }

    ErrStatus = hSdbdk->NvDdkBlockDevReadSector(hSdbdk,
                    NCTDataStartSector +BoardInfoStartSectorOffset,
                    pBuf, 1);
    if (ErrStatus != NvError_Success)
    {
        goto fail;
    }

    seek = InitialWidth;
    s_BoardInfo.BoardId = ((pBuf[seek + 1] )<<8)| pBuf[seek];seek+=4;
    s_BoardInfo.Sku = ((pBuf[seek + 1] )<<8)| pBuf[seek];seek+=4;
    s_BoardInfo.Fab =((pBuf[seek + 1] )<<8)| pBuf[seek];
    s_BoardInfo.Revision = 0;
    s_BoardInfo.MinorRevision = 0;

    seek = InitialWidth + NCT_ENTRY_DATA_OFFSET;
    s_PmuBoardInfo.BoardId = ((pBuf[seek + 1] )<<8)| pBuf[seek]; seek+=4;
    s_PmuBoardInfo.Sku = ((pBuf[seek + 1] )<<8)| pBuf[seek];seek+=4;
    s_PmuBoardInfo.Fab =((pBuf[seek + 1] )<<8)| pBuf[seek];
    s_PmuBoardInfo.Revision = 0;
    s_PmuBoardInfo.MinorRevision = 0;

    seek = InitialWidth + (NCT_ENTRY_DATA_OFFSET * 2);
    s_DisplayBoardInfo.BoardId = ((pBuf[seek + 1] )<<8)| pBuf[seek];seek+=4;
    s_DisplayBoardInfo.Sku = ((pBuf[seek + 1] )<<8)| pBuf[seek];seek+=4;
    s_DisplayBoardInfo.Fab =((pBuf[seek + 1] )<<8)| pBuf[seek];
    s_DisplayBoardInfo.Revision = 0;
    s_DisplayBoardInfo.MinorRevision = 0;

    if (hSdbdk)
        hSdbdk->NvDdkBlockDevClose(hSdbdk);
    else
        NvOsDebugPrintf("Sd Not Initialised\n");
    NvDdkBlockDevMgrDeinit();

fail:
    if (pBuf)
        NvOsFree(pBuf);

    return ErrStatus;
}
#endif

static NvBool
NvOdmPeripheralReadPartNumber(
    NvOdmServicesI2cHandle hOdmI2c,
    NvU8 EepromInst,
    NvOdmBoardInfoRev1 *pExtendedBoardInfo)
{
    NvU32 BctSize = 0;
    NvBctHandle hBct;
    NvBctAuxInfo *pAuxInfo;
    NvU32 Size = 0;
    NvU32 Instance = 0;
    NvU8 *pCustomerData = NULL;
    NvError err = NvError_Success;
    NvBool b = NV_FALSE;

    err = NvBctInit(&BctSize, NULL, NULL);
    if (err)
        goto fail;
    err = NvBctInit(&BctSize, NULL, &hBct);
    if (err)
        goto fail;

    err = NvBctGetData(
            hBct,
            NvBctDataType_AuxDataAligned,
            &Size,
            &Instance,
            NULL);
    if (err)
        goto fail;
    pCustomerData = NvOsAlloc(Size);

    if (!pCustomerData)
        goto fail;

    NvOsMemset(pCustomerData, 0, Size);

    // get the pCustomer data
    err = NvBctGetData(
            hBct,
            NvBctDataType_AuxDataAligned,
            &Size,
            &Instance,
            pCustomerData);

    if (err)
        goto fail;
    //Fill out partinfo stuff
#if NOT_SHARED_QUERY_LIBRARY==1

    pAuxInfo = (NvBctAuxInfo*)pCustomerData;
    if (EepromInst == 0x6)
    {
        pExtendedBoardInfo->BoardID = pAuxInfo->NCTBoardInfo.proc_board_id;
        pExtendedBoardInfo->Fab = pAuxInfo->NCTBoardInfo.proc_fab;
        pExtendedBoardInfo->SKU = pAuxInfo->NCTBoardInfo.proc_sku;
        pExtendedBoardInfo->Revision = 0;
        pExtendedBoardInfo->MinorRevision = 0;
    }
    else if (EepromInst == 0x4)
    {
        pExtendedBoardInfo->BoardID = pAuxInfo->NCTBoardInfo.pmu_board_id;
        pExtendedBoardInfo->Fab = pAuxInfo->NCTBoardInfo.pmu_fab;
        pExtendedBoardInfo->SKU = pAuxInfo->NCTBoardInfo.pmu_sku;
        pExtendedBoardInfo->Revision = 0;
        pExtendedBoardInfo->MinorRevision = 0;
    }
    else if (EepromInst == 0x1)
    {
        pExtendedBoardInfo->BoardID = pAuxInfo->NCTBoardInfo.display_board_id;
        pExtendedBoardInfo->Fab = pAuxInfo->NCTBoardInfo.display_fab;
        pExtendedBoardInfo->SKU = pAuxInfo->NCTBoardInfo.display_sku;
        pExtendedBoardInfo->Revision = 0;
        pExtendedBoardInfo->MinorRevision = 0;
    }
    else
    {
        NvOsDebugPrintf("Invalid eeprom instance\n");
        goto fail;
    }
#endif
    if (!(pExtendedBoardInfo->BoardID))
    {
        NvOsDebugPrintf("Invalid boardId read from bct\n");
        goto fail;
    }
    b = NV_TRUE;
fail:
    if (pCustomerData)
        NvOsFree(pCustomerData);
    if (hBct)
        NvBctDeinit(hBct);
    return b;
}

NvBool
NvOdmPeripheralGetBoardInfo(
    NvU16 BoardId,
    NvOdmBoardInfo *pBoardInfo)
{
    NvBool RetVal = NV_FALSE;
    NvOdmServicesI2cHandle hOdmI2c = NULL;
    NvU8 EepromInst, CurrentBoard;
    NvU16 MainProcessorBoard[] = {PROCESSOR_BOARDS_BOARD_INFO};
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
    NvOdmBoardInfo *procBoardInfo = NULL;
    NvOdmBoardInfo BoardInfo;

    NvOdmPmuBoardInfo *pPmuInfo;
    NvOdmMaxCpuCurrentInfo *pCpuCurrInfo;
    NvOdmDisplayPanelInfo *pDisplayPanelInfo;
    NvOdmTouchInfo *pTouchInfo;
    NvOdmPowerSupplyInfo *pSupplyInfo;

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

        case NvOdmBoardModuleType_PmuBoard:
            if (DataLen != sizeof(NvOdmPmuBoardInfo))
                return NV_FALSE;

            // Look for E2545
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

        case NvOdmBoardModuleType_PowerSupply:
            pSupplyInfo = (NvOdmPowerSupplyInfo *)BoardModuleData;

            if(!NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_ProcessorBoard,
                        &BoardInfo, sizeof(BoardInfo)))
                return NV_FALSE;

            if ((BoardInfo.BoardID == BOARD_E2530) &&
                    (BoardInfo.SKU == FOSTER_SKU_ID))
            {
                //foster doesn't have battery/charger
                pSupplyInfo->SupplyType = NvOdmBoardPowerSupply_Adapter;
            }
            else
            {
                // Hardcoding to battery for NFF
                pSupplyInfo->SupplyType = NvOdmBoardPowerSupply_Battery;
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
            return NV_TRUE;
        case NvOdmBoardModuleType_Touch:
            pTouchInfo = (NvOdmTouchInfo *)BoardModuleData;
            pTouchInfo->TouchPanelId = NvOdmBoardTouchPanelId_Loki_Wintek_5_66_Unlamin;

            if (NvOdmPeripheralGetBoardInfo(BOARD_P2534, &BoardInfo))
            {
                if (BoardInfo.Fab == BOARD_P2534_THOR_FAB && BoardInfo.Revision == BOARD_P2534_THOR_REV)
                    pTouchInfo->TouchPanelId = NvOdmBoardTouchPanelId_Thor_Wintek;
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
