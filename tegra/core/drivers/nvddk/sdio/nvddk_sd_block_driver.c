/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/** @file
 * @brief <b>Sd Protocol Driver</b>
 *
 * @b Description: Contains the implementation of Sd Protocol Driver API's .
 */

#include "nvddk_sdio.h"
#include "nvddk_sdio_private.h"
#include "nvassert.h"
#include <nvodm_query.h>
#include "nvddk_fuse.h"
#include "nvboot_bct.h"
#include "nvboot_bit.h"
#include "nvpartmgr_defs.h"
#include "nvddk_sdio_utils.h"

// Workaround for SD partial erase support
#ifndef WAR_SD_FORMAT_PART
// Allow for write 0xFF to erase partial erase group sized sectors
#define WAR_SD_FORMAT_PART 1
#endif

// disable hs200 mode temporary
#ifndef BOOT_MINIMAL_BL
#define ENABLE_HS200_MODE_SUPPORT    1
#else
#define ENABLE_HS200_MODE_SUPPORT    0
#endif

#define ENABLE_SANITIZE_OPERATION 0

// macro to make string from constant
#define MACRO_GET_STR(x, y) \
    NvOsMemcpy(x, #y, (NvOsStrlen(#y) + 1))

// Macro to get expression for modulo value that is power of 2
// Expression: DIVIDEND % (pow(2, Log2X))
#define MACRO_MOD_LOG2NUM(DIVIDEND, Log2X) \
    ((DIVIDEND) & ((1 << (Log2X)) - 1))

// Macro to get expression for multiply by number which is power of 2
// Expression: VAL * (1 << Log2Num)
#define MACRO_POW2_LOG2NUM(Log2Num) \
    (1 << (Log2Num))

// Macro to get expression for multiply by number which is power of 2
// Expression: VAL * (1 << Log2Num)
#define MACRO_MULT_POW2_LOG2NUM(VAL, Log2Num) \
    ((VAL) << (Log2Num))

// Macro to get expression for div by number that is power of 2
// Expression: VAL / (1 << Log2Num)
#define MACRO_DIV_POW2_LOG2NUM(VAL, Log2Num) \
    ((VAL) >> (Log2Num))

//WAR for Write Timeout and Data CRC issues observed with write. Refer bug 918157
#define MAX_WRITE_RETRY_COUNT 5

// Sd Specific Defines
#define SD_SECTOR_SIZE   512
#define SD_SECTOR_SZ_LOG2 9
#define MAX_SECTORS_PER_BLOCK 512
#define MAX_BLOCK_SIZE 2097152 //2MB
#define SD_IDENT_CLOCK_KHZ 200
#define SD_HOST_VOLTAGE_RANGE   0x100
#define SD_HOST_CHECK_PATTERN   0xAA
#define SD_CARD_OCR_VALUE   0x00300000
#define SD_CARD_POWERUP_STATUS_MASK 0x80000000
#define SD_CARD_CAPACITY_MASK   0x40000000
#define SD_TX_CLOCK_KHZ 25000
#define SD_SDHC_TX_CLOCK_KHZ 50000
#define MMC_HSM_EN_FOR_HS200_KHZ   100000
#define SD_SDHC_HIGHSPEED_QUERY  0x00FFFF01
#define SD_SDHC_HIGHSPEED_SET  0x80FFFF01
#define SD_SDHC_SWITCH_BLOCK_SIZE  64
#define SD_MAX_RESPONSE_WORDS   5
#define SD_TRANSFER_STATE_MASK  0x1E00
#define SD_TRANSFER_STATE_SHIFT  9
#define SD_SDHC_CSIZE_MASK  0x3FFFFF00
#define SD_SDHC_CSIZE_WORD  1
#define SD_SDHC_CSIZE_SHIFT  8
#define SD_SDHC_CSIZE_MULTIPLIER  1024
#define SD_SDHC_GROUP1_MASK 0x2
#define SD_SDHC_GROUP1_SHIFT 8
#define SD_CSD_BLOCK_LEN_WORD   2
#define SD_CSD_BLOCK_LEN_SHIFT  8
#define SD_CSD_BLOCK_LEN_MASK  0xF
#define SD_CSD_CSIZE_HIGH_WORD   2
#define SD_CSD_CSIZE_HIGH_WORD_SHIFT 10
#define SD_CSD_CSIZE_HIGH_WORD_MASK 0x3
#define SD_CSD_CSIZE_LOW_WORD   1
#define SD_CSD_CSIZE_LOW_WORD_SHIFT   22
#define SD_CSD_CSIZE_MULT_WORD   1
#define SD_CSD_CSIZE_MULT_SHIFT  7
#define SD_CSD_CSIZE_MULT_MASK  0x7
#define SD_CSD_SECTOR_SIZE_HIGH_WORD      1
#define SD_CSD_SECTOR_SIZE_HIGH_MASK      0x3F
#define SD_CSD_SECTOR_SIZE_HIGH_SHIFT     1
#define SD_CSD_SECTOR_SIZE_LOW_WORD       0
#define SD_CSD_SECTOR_SIZE_LOW_MASK       0x80000000
#define SD_CSD_SECTOR_SIZE_LOW_SHIFT      31
#define SD_CSD_WRITE_BL_LEN_WORD      0
#define SD_CSD_WRITE_BL_LEN_MASK      0x3C000
#define SD_CSD_WRITE_BL_LEN_SHIFT     14
#define SD_BUS_WIDTH_1BIT   0
#define SD_BUS_WIDTH_4BIT   2

#define MMC_POWER_ON_TIMEOUT_USEC 1000
#define MMC_EXPECTED_OCR 0x40FF8080
#define MMC_IDENT_CLOCK_KHZ 100
#define MMC_SECTOR_ADDRES_BIT_MASK 0x40000000
#define MMC_LEGACY_SPEED_ARGUMENT 0x03B90000
#define MMC_HIGH_SPEED_ARGUMENT 0x03B90100
#define MMC_HS200_SPEED_ARGUMENT 0x03B90200
#define MMC_BUS_WIDTH_ARG 0x03b70000
#define MMC_ENABLE_HIGH_CAPACITY_ERASE_ARG 0x03AF0100
#define MMC_CARD_POWERUP_STATUS_MASK 0x80000000
#define MMC_CARD_CAPACITY_MASK   0x40000000
#define MMC_READ_WRITE_ERROR_MASK 0x80000
#define ERASE_CMD_ERROR 0x10002000
#define SD_HC_ERASE_UNIT_SIZE 512 * 1024
#define SD_HC_ERASE_UNIT_SIZE_LOG2 (9 + 10)
#define EXTCSD_ERASE_GROUP_DEF_BYTE_OFFSET 175
#define EXTCSD_HC_ERASE_GRP_SIZE_BYTE_OFFSET 224
#define EMMC_SWITCH_SELECT_PARTITION_ARG 0x03b30000
#define EMMC_SWITCH_SELECT_PARTITION_OFFSET 0x8
// This defines the maximum number of sectors that can be erased at a time
#define EMMC_MAX_ERASABLE_SECTORS 0x200000
#define EMMC_MAX_ERASABLE_SECTORS_LOG2 21
#define MMC_BOOT_PARTITION_WP_ARG 0x03AD0000
#define MMC_USER_PARTITION_WP_ARG 0x03AB0000
#define MMC_DEVICE_TYPE_HIGHSPEED_26MHZ 0x1
#define MMC_DEVICE_TYPE_HIGHSPEED_52MHZ 0x2
#define MMC_DEVICE_TYPE_HIGHSPEED_DDR_52MHZ_1_8_V 0x4
#define MMC_DEVICE_TYPE_HIGHSPEED_DDR_52MHZ_1_2_V 0x8
#define MMC_DEVICE_TYPE_HS200_SDR_200MHZ_1_8_V 0x10
#define MMC_DEVICE_TYPE_HS200_SDR_200MHZ_1_2_V 0x20
#define MMC_SDR_MODE_8_BIT_BUSWIDTH 0x2
#define MMC_SDR_MODE_4_BIT_BUSWIDTH 0x1
#define MMC_DDR_MODE_8_BIT_BUSWIDTH 0x6
#define MMC_DDR_MODE_4_BIT_BUSWIDTH 0x5

// The maximum transfer size in a single read/write call is 32 MB.
// Defining this with respect to the logical sector size.
// MMC_SD_MAX_READ_WRITE_SECTORS(0x1FFF) * SECTOR_SIZE(4096)
#define MMC_SD_MAX_READ_WRITE_SECTORS 0x1FFF

/// Defines various Application specific Sd Commands as per spec
typedef enum
{
    SdAppCmd_SetBusWidth = 6,
    SdAppCmd_SdStatus = 13,
    SdAppCmd_SendNumWriteBlocks = 22,
    SdAppCmd_SetWriteBlockEraseCount = 23,
    SdAppCmd_SendOcr = 41,
    SdAppCmd_SetClearCardDetect = 42,
    SdAppCmd_SendScr = 51,
    SdAppCmd_Force32 = 0x7FFFFFFF
} SdApplicationCommands;

/// Defines Emmc card partitions.
typedef enum
{
    SdmmcAccessRegion_UserArea = 0,
    SdmmcAccessRegion_BootPartition1,
    SdmmcAccessRegion_BootPartition2,
    SdmmcAccessRegion_Num,
    SdmmcAccessRegion_Unknown,
    SdmmcAccessRegion_Force32 = 0x7FFFFFFF
} SdmmcAccessRegion;

/// Defines various Sd Card states.
typedef enum
{
    SdState_Idle = 0,
    SdState_Ready,
    SdState_Ident,
    SdState_Stby,
    SdState_Tran,
    SdState_Data,
    SdState_Rcv,
    SdState_Prg,
    SdState_Dis,
    SdState_Force32 = 0x7FFFFFFF
} SdState;

typedef struct SdPartTabRec
{
    // Data for each partition
    // Partition start sector address
    NvU32 StartLSA;
    // number of sectors in partition
    NvU32 NumOfSectors;
    // Remember the partition id for the partition/region
    NvU32 MinorInstance;
    // next entry pointer
    struct SdPartTabRec * Next;
} SdPartTab, *SdPartTabHandle;

typedef struct SdRec {
    NvDdkBlockDev BlockDev;
    NvDdkBlockDevInfo BlockDevInfo;
    NvDdkSdioCommand pCmd;
    volatile NvDdkSdioStatus pCurrentStatus;
    NvRmDeviceHandle hRmDevice;
    NvDdkSdioDeviceHandle hSdDdk;
    NvU32 Response[SD_MAX_RESPONSE_WORDS];
    NvU16 Rca;
    NvU32 NumOfBlocks;
    NvDdkSdioBlockSize BlockLengthInUse;
    NvBool IsAutoIssueCMD12Supported;
    NvBool IsSdhc;
    NvBool IsMMC;
    NvBool IsHighSpeed;
    NvU32 Instance;
    // mutex for exclusive operations
    NvOsMutexHandle LockDev;
    NvU32 PowerUpCounter;
    NvU32 RefCount;
    SdPartTabHandle hHeadRegionList;
    // Erase block size in number of sectors
    NvU32 EraseGrpSize;
    // LCM of SD erase group size and SD block driver Block size
    NvU32 PartUnitSize;
    NvU32 LogicalAddressStart;
    NvBool IsReadVerifyWriteEnabled;
    NvBool IsEnabledEraseGrpDef;
    NvU32 EraseBlockCount;
    NvU32 BootPartitionSize;
    NvU8 BootConfig;
    NvU32 DataBusWidth;
    NvU32 CardState;
    NvU32 SectorsPerPageLog2;
    NvU32 CardIdReg[4];
    NvU32 CurrentRegion;
    NvU32 WPGrpSize;
    NvU32 CardType;
    NvU32 ErasedMemContent;
    NvBool IsSecureEraseSupported;
    NvBool IsTrimEraseSupported;
    NvBool IsSanitizeSupported;
    NvBool DoSecureErase;
    NvBool DoTrimErase;
    NvBool DoSanitize;
    // Speed mode supported by the card
    NvDdkSdioUhsMode Uhsmode;
    // Host capabilities
    NvDdkSdioHostCapabilities SdHostCaps;
}Sd, *SdHandle;


// Local type defined to store data common
// to multiple Sd Controller instances
typedef struct SdCommonInfoRec
{
    // For each sdio controller we are maintaining device state in
    // SdDevHandle. This data for a particular controller is also
    // visible when using the partition specific sdio Block Device Driver
    // Handle - SdBlockDevHandle
    SdHandle *SdDevStateList;
    // List of locks for each device
    NvOsMutexHandle *SdDevLockList;
    // Count of Sd controllers on SOC, queried from NvRM
    NvU32 MaxInstances;
    // Global RM Handle passed down from Device Manager
    NvRmDeviceHandle hRm;
    // Global boolean flag to indicate if init is done,
    // NV_FALSE indicates not initialized
    // NV_TRUE indicates initialized
    NvBool IsBlockDevInitialized;
    // Init sdio block dev driver ref count
    NvU32 InitRefCount;
} SdCommonInfo ;

// Global info shared by multiple sdio controllers,
// and make sure we initialize the instance
SdCommonInfo s_SdCommonInfo = { NULL, NULL, 0, NULL, 0, 0 };

// Composite type for Sd Block device driver. It contains
// the block device driver handle returned to device manager open call.
// Additionally, it contains the information specific to a device.
typedef struct SdBlockDevRec {
    // This is partition specific information
    //
    // IMPORTANT: For code to work must ensure that  NvDdkBlockDev
    // is the first element of this structure
    // Block dev driver handle specific to a partition on a device
    NvDdkBlockDev BlockDev;
    // Partition Id for partition corresponding to the block dev
    NvU32 MinorInstance;
    // Flag to indicate the block driver is powered ON
    NvBool IsPowered;
    // One SdDevHandle is shared between multiple partitions on
    // same device. This is a pointer and is created once only
    SdHandle hDev;
} SdBlockDev, *SdBlockDevHandle;

NvBool s_IsUserPartitionAllocStarted= NV_FALSE;

// function to check power of 2
static NvBool
UtilCheckPowerOf2(NvU32 Num)
{
    // A power of 2 satisfies condition (N & (N - 1)) == (2 * N - 1)
    if ((Num & (Num - 1)) == 0)
        return NV_TRUE;
    else
        return NV_FALSE;
}

static NvError
SetBusWidth(
    SdHandle hSd,
    NvDdkSdioDataWidth Buswidth)
{
    NvError e = NvSuccess;
    NvU32 SdioStatus = 0;
    NvDdkSdioCommand *pCmd = &hSd->pCmd;

    // Set the card bus width using ACMD6
    SD_SET_COMMAND_PARAMETERS(pCmd,
        SdCommand_ApplicationCommand,
        NvDdkSdioCommandType_Normal,
        NV_FALSE,
        (NvU32)(hSd->Rca << 16),
        NvDdkSdioRespType_R1,
        SD_SECTOR_SIZE);
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd, &SdioStatus));
    if (SdioStatus != NvDdkSdioError_None)
    {
        e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
        goto fail;
    }
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
        hSd->hSdDdk,
        pCmd->CommandCode,
        pCmd->ResponseType,
        hSd->Response));

    SD_SET_COMMAND_PARAMETERS(pCmd,
        SdAppCmd_SetBusWidth,
        NvDdkSdioCommandType_Normal,
        NV_FALSE,
        (Buswidth == 0)?
        SD_BUS_WIDTH_1BIT:
        SD_BUS_WIDTH_4BIT,
        NvDdkSdioRespType_R1,
        SD_SECTOR_SIZE);
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd, &SdioStatus));
    if (SdioStatus != NvDdkSdioError_None)
    {
        e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
        goto fail;
    }
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
        hSd->hSdDdk,
        pCmd->CommandCode,
        pCmd->ResponseType,
        hSd->Response));

    // Now change the Host bus width as well
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSetHostBusWidth(hSd->hSdDdk, Buswidth));

    fail:
    if (e)
    {
        SD_ERROR_PRINT("SetBusWidth :Error 0x%x\n", e);
    }
    return e;
}

static NvError
SetBlockSize(
    SdHandle hSd,
    NvU32 blockSize)
{
    NvError e = NvSuccess;
    NvU32 SdioStatus = 0;
    NvDdkSdioCommand *pCmd = &hSd->pCmd;

    // Setting block size using CMD16 is illegal in DDR mode
    if (hSd->Uhsmode == NvDdkSdioUhsMode_DDR50)
        return NvSuccess;

    SD_SET_COMMAND_PARAMETERS(pCmd,
        SdCommand_SetBlockLength,
        NvDdkSdioCommandType_Normal,
        NV_FALSE,
        blockSize,
        NvDdkSdioRespType_R1,
        SD_SECTOR_SIZE);
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd, &SdioStatus));
    if (SdioStatus != NvDdkSdioError_None)
    {
        e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
        goto fail;
    }
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
        hSd->hSdDdk,
        pCmd->CommandCode,
        pCmd->ResponseType,
        hSd->Response));

    fail:
    if (e)
    {
        SD_ERROR_PRINT("SetBlockSize :Error 0x%x\n", e);
    }
    return e;
}

static NvError
SwitchToHighSpeed(SdHandle hSd)
{
    NvError e = NvSuccess;
    NvDdkSdioCommand* pCmd = &hSd->pCmd;
    NvU32 SdioStatus = 0;
    NvU32 *pSwitchResponse;
    NvU32 Group1Response=0;

    pSwitchResponse = NvOsAlloc(SD_SDHC_SWITCH_BLOCK_SIZE);
    if (pSwitchResponse == NULL)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    // Set the block size
    NV_CHECK_ERROR_CLEANUP(SetBlockSize(hSd,SD_SDHC_SWITCH_BLOCK_SIZE));

    // switch command
    SD_SET_COMMAND_PARAMETERS(pCmd,
        SdCommand_Switch,
        NvDdkSdioCommandType_Normal,
        NV_TRUE,
        SD_SDHC_HIGHSPEED_QUERY,
        NvDdkSdioRespType_R1,
        SD_SDHC_SWITCH_BLOCK_SIZE);

    NV_CHECK_ERROR_CLEANUP(NvDdkSdioRead(hSd->hSdDdk,
        SD_SDHC_SWITCH_BLOCK_SIZE,
        (NvU32 *)pSwitchResponse,
        pCmd,
        hSd->IsAutoIssueCMD12Supported,
        &SdioStatus));
    if (SdioStatus != NvDdkSdioError_None)
    {
        e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
        goto fail;
    }
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
        hSd->hSdDdk,
        pCmd->CommandCode,
        pCmd->ResponseType,
        hSd->Response));
    Group1Response = (pSwitchResponse[3] >> SD_SDHC_GROUP1_SHIFT)
        & SD_SDHC_GROUP1_MASK;
    if (!Group1Response)
    {
        e = SET_ERROR(NvError_NotSupported, pCmd->CommandCode);
        goto fail;
    }

    // switch command to change the speed
    SD_SET_COMMAND_PARAMETERS(pCmd,
        SdCommand_Switch,
        NvDdkSdioCommandType_Normal,
        NV_TRUE,
        SD_SDHC_HIGHSPEED_SET,
        NvDdkSdioRespType_R1,
        SD_SDHC_SWITCH_BLOCK_SIZE);

    NV_CHECK_ERROR_CLEANUP(NvDdkSdioRead(hSd->hSdDdk,
        SD_SDHC_SWITCH_BLOCK_SIZE,
        (NvU32 *)pSwitchResponse,
        pCmd,
        hSd->IsAutoIssueCMD12Supported,
        &SdioStatus));
    if (SdioStatus != NvDdkSdioError_None)
    {
        e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
        goto fail;
    }
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
        hSd->hSdDdk,
        pCmd->CommandCode,
        pCmd->ResponseType,
        hSd->Response));

    Group1Response = (pSwitchResponse[3] >> SD_SDHC_GROUP1_SHIFT) &
        SD_SDHC_GROUP1_MASK;
    if (!Group1Response)
    {
        e = SET_ERROR(NvError_InvalidState, pCmd->CommandCode);
        goto fail;
    }

fail:
    NvOsFree(pSwitchResponse);
    pSwitchResponse = NULL;
    if (e)
    {
        SD_ERROR_PRINT("SwitchToHighSpeed :Error 0x%x\n", e);
    }
    return e;
}

static NvError
ReadCsd(SdHandle hSd)
{
    NvError e = NvSuccess;
    NvU32 SdioStatus = 0;
    NvU32 CSize = 0;
    NvU32 CSizeMult = 0;
    NvU32 ReadBlLength = 0;
    NvU32 EraseSectorSize = 0;
    NvU32 MaxWriteBlockLength = 0;
    NvU32 *pCsd = NULL;
    NvDdkSdioCommand* pCmd = &hSd->pCmd;

    SD_SET_COMMAND_PARAMETERS(pCmd,
        SdCommand_SendCsd,
        NvDdkSdioCommandType_Normal,
        NV_FALSE,
        (NvU32)(hSd->Rca << 16),
        NvDdkSdioRespType_R2,
        SD_SECTOR_SIZE);
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd, &SdioStatus));
    if (SdioStatus != NvDdkSdioError_None)
    {
        e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
        goto fail;
    }
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
        hSd->hSdDdk,
        pCmd->CommandCode,
        pCmd->ResponseType,
        hSd->Response));

    pCsd = hSd->Response;
    if (hSd->IsSdhc == NV_FALSE)
    {
        /*
         * Number of Blocks = (C_SIZE + 1) * CSizeMult
         * CSizeMult = 2^(C_SIZE_MULT + 2)
         * Capacity of the device = (Number of Blocks * BLOCK_LEN) bytes
         * BLOCK_LEN = 2^(READ_BL_LEN)
         */
        ReadBlLength = ((pCsd[SD_CSD_BLOCK_LEN_WORD]
            >> SD_CSD_BLOCK_LEN_SHIFT)
                         & SD_CSD_BLOCK_LEN_MASK);
        ReadBlLength = (NvU32)(1 << ReadBlLength);

        CSize = (pCsd[SD_CSD_CSIZE_HIGH_WORD]
                  & SD_CSD_CSIZE_HIGH_WORD_MASK)
                 << SD_CSD_CSIZE_HIGH_WORD_SHIFT;
        CSize |= (pCsd[SD_CSD_CSIZE_LOW_WORD]
             >> SD_CSD_CSIZE_LOW_WORD_SHIFT);
        CSize++;

        CSizeMult = ((pCsd[SD_CSD_CSIZE_MULT_WORD]
                     >> SD_CSD_CSIZE_MULT_SHIFT)
                     & SD_CSD_CSIZE_MULT_MASK);
        CSizeMult = (NvU32)(1 << (CSizeMult + 2));

        hSd->NumOfBlocks = (CSize * CSizeMult *
            (ReadBlLength/SD_SECTOR_SIZE));
    }
    else
    {
        /*
             * Capacity of the device = (C_SIZE + 1) * 512 * 1024 bytes
             */
        CSize = (pCsd[SD_SDHC_CSIZE_WORD] & SD_SDHC_CSIZE_MASK)
            >> SD_SDHC_CSIZE_SHIFT;
        CSize++;
        hSd->NumOfBlocks = CSize * SD_SDHC_CSIZE_MULTIPLIER;
    }

    // Calculate Erase Group Size from Erase Sector Size CSD[45:39] and
    // Max Write data block length CSD[25:22]
    EraseSectorSize = ((pCsd[SD_CSD_SECTOR_SIZE_HIGH_WORD] &
                        SD_CSD_SECTOR_SIZE_HIGH_MASK) <<
                        SD_CSD_SECTOR_SIZE_HIGH_SHIFT);
    EraseSectorSize |= ((pCsd[SD_CSD_SECTOR_SIZE_LOW_WORD] &
                        SD_CSD_SECTOR_SIZE_LOW_MASK) >>
                        SD_CSD_SECTOR_SIZE_LOW_SHIFT);
    EraseSectorSize += 1;

    MaxWriteBlockLength = ((pCsd[SD_CSD_WRITE_BL_LEN_WORD] &
                           SD_CSD_WRITE_BL_LEN_MASK) >>
                           SD_CSD_WRITE_BL_LEN_SHIFT);

    // Erase Group Size in terms of sectors
    hSd->EraseGrpSize = (EraseSectorSize << MaxWriteBlockLength) / SD_SECTOR_SIZE;

    fail:
    if (e)
    {
        SD_ERROR_PRINT("ReadCsd :Error 0x%x\n", e);
    }
    return e;
}

/*
 * This functions get the status of card
 */
static NvError
GetCardStatus(SdHandle hSd)
{
    NvError e = NvSuccess;
    NvDdkSdioCommand *pCmd = &hSd->pCmd;
    NvU32 SdioStatus;

    // Send CMD13
    SD_SET_COMMAND_PARAMETERS(pCmd,
        SdCommand_SendStatus,
        NvDdkSdioCommandType_Normal,
        NV_FALSE,
        (NvU32)(hSd->Rca << 16),
        NvDdkSdioRespType_R1,
        SD_SECTOR_SIZE);

    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(hSd->hSdDdk,
        pCmd,
        &SdioStatus));

    if (SdioStatus != NvDdkSdioError_None)
    {
        e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(hSd->hSdDdk,
        pCmd->CommandCode,
        pCmd->ResponseType,
        hSd->Response));

fail:
    if (e)
    {
        SD_ERROR_PRINT("GetCardStatus :Error 0x%x\n", e);
    }
    return e;
}

/*
 * This functions checks whether the card is in transfer state.
 * The card has to be in transfer state after it is identified.
 */
static NvBool
IsCardInTransferState(SdHandle hSd)
{
    NvU32 SdioStatus;
    NvError e = NvSuccess;

    //Get Card Status
    NV_CHECK_ERROR_CLEANUP(GetCardStatus(hSd));

    // Extract the Card State from the Response.
    SdioStatus = ((hSd->Response[0] & SD_TRANSFER_STATE_MASK) >>
        SD_TRANSFER_STATE_SHIFT);
    hSd->CardState = SdioStatus;

    // Card should be in Tansfer state now.
    if (SdioStatus == SdState_Tran)
        return NV_TRUE;
fail:
    if (e)
    {
        SD_ERROR_PRINT("IsCardInTransferState :Error 0x%x\n", e);
    }
    return NV_FALSE;
}

/*
 * This fuction waits for the card to not be busy.
 */
static NvError
WaitCardNoTransferState(SdHandle hSd)
{
    NvError e = NvSuccess;

    do
    {
        // Get Card Status
        NV_CHECK_ERROR_CLEANUP(GetCardStatus(hSd));

        // Check whether the card is in transfer state
    } while (((hSd->Response[0] & SD_TRANSFER_STATE_MASK) >>
        SD_TRANSFER_STATE_SHIFT) == SdState_Prg);

fail:
    if (e)
    {
        SD_ERROR_PRINT("WaitCardNoTransferState :Error 0x%x\n", e);
    }
    return e;
}

static NvError
MmcCardInit(
    SdHandle hSd,
    NvU32 HostCapacitySupport,
    NvU32 *CardCapacitySupport)
{
    NvU32 SdioStatus = 0;
    NvError e = NvSuccess;
    NvU32 OCRRegister = 0;
    NvDdkSdioCommand *pCmd = &hSd->pCmd;
    NvU32 retries = 0;

    do
    {
        retries++;
        OCRRegister |= MMC_EXPECTED_OCR;
        SD_SET_COMMAND_PARAMETERS(pCmd,
            MmcCommand_MmcInit,
            NvDdkSdioCommandType_Normal,
            NV_FALSE,
            OCRRegister,
            NvDdkSdioRespType_R3,
            SD_SECTOR_SIZE);
        NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd,
            &SdioStatus));
        if (SdioStatus == NvDdkSdioError_CommandTimeout)
        {
            // Check if the inserted card is an sd card
            e = NvError_SdioCardNotPresent;
            goto fail;
        }
        if(SdioStatus != NvDdkSdioError_None)
        {
            e =NvError_SdioCommandFailed;
            goto fail;
        }

        NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
            hSd->hSdDdk,
            pCmd->CommandCode,
            pCmd->ResponseType,
            hSd->Response));
        OCRRegister = hSd->Response[0];
        if (OCRRegister & (NvU32)(MMC_CARD_POWERUP_STATUS_MASK))
            break;

        // 1000ms is the max time for the card to be ready
        if (retries > 100)
        {
            e = NvError_Timeout;
            goto fail;
        }
        NvOsSleepMS(10);
    }while(1);

    // Card powered up, check if the card supports high Capacity
    // If OCRRegister is non-zero, this indicates that the
    // card supports SDHC
    *CardCapacitySupport = (OCRRegister & MMC_CARD_CAPACITY_MASK);
    hSd->IsSdhc = (OCRRegister & MMC_CARD_CAPACITY_MASK) ? NV_TRUE : NV_FALSE;
    hSd->IsMMC = NV_TRUE;

fail:
    if (e)
    {
        SD_ERROR_PRINT("MmcCardInit :Error 0x%x\n", e);
    }
    return e;
}

static NvError
MmcDoSanitize(SdHandle hSd)
{
    NvError e = NvSuccess;
    NvU32 SdStatus = 0;
    NvU32 i;
    NvU32 MaxRetries = 3;
    NvDdkSdioCommand *pCmd = &hSd->pCmd;

    for (i = 1;i < MaxRetries ;i++)
    {
        SD_SET_COMMAND_PARAMETERS(pCmd,
            MmcCommand_Switch,
            NvDdkSdioCommandType_Normal,
            NV_FALSE,
            MMC_SANITIZE_ARG,
            NvDdkSdioRespType_R1b,
            SD_SECTOR_SIZE);

        NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd, &SdStatus));

        if (SdStatus != NvDdkSdioError_None)
        {
            if (i == MaxRetries)
            {
                e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
                goto fail;
            }
            else
                continue;
        }

        // Get the command response
        NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
            hSd->hSdDdk,
            pCmd->CommandCode,
            pCmd->ResponseType,
            hSd->Response));

        // Wait for the card to not be busy
        NV_CHECK_ERROR_CLEANUP(WaitCardNoTransferState(hSd));

        if (!e)
            break;
    }

fail:
    if (e)
    {
        SD_ERROR_PRINT("MmcDoSanitize :Error 0x%x\n", e);
    }
    return e;
}

static NvError
MmcSetBusWidth(SdHandle hSd, NvU32 BusWidth)
{
    NvError e = NvSuccess;
    NvU32 SdStatus = 0;
    NvDdkSdioCommand *pCmd = &hSd->pCmd;

    SD_SET_COMMAND_PARAMETERS(pCmd,
        MmcCommand_Switch,
        NvDdkSdioCommandType_Normal,
        NV_FALSE,
        (MMC_BUS_WIDTH_ARG | (BusWidth << 8)),
        NvDdkSdioRespType_R1b,
        SD_SECTOR_SIZE);
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd, &SdStatus));
    if(SdStatus != NvDdkSdioError_None)
    {
        e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
        goto fail;
    }

    // Get the command response
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
        hSd->hSdDdk,
        pCmd->CommandCode,
        pCmd->ResponseType,
        hSd->Response));

    // Wait for the card to not be busy
    NV_CHECK_ERROR_CLEANUP(WaitCardNoTransferState(hSd));

    // Now set the Host bus width
    if ((BusWidth == MMC_DDR_MODE_8_BIT_BUSWIDTH) ||
        (BusWidth == MMC_SDR_MODE_8_BIT_BUSWIDTH))
    {
        NV_CHECK_ERROR_CLEANUP(NvDdkSdioSetHostBusWidth(hSd->hSdDdk,
            NvDdkSdioDataWidth_8Bit));
    }
    else if(BusWidth == MMC_SDR_MODE_4_BIT_BUSWIDTH)
    {
        NV_CHECK_ERROR_CLEANUP(NvDdkSdioSetHostBusWidth(hSd->hSdDdk,
            NvDdkSdioDataWidth_4Bit));
    }
    else
        NV_CHECK_ERROR_CLEANUP(NvDdkSdioSetHostBusWidth(hSd->hSdDdk,
            NvDdkSdioDataWidth_1Bit));

fail:
    if (e)
    {
        SD_ERROR_PRINT("MmcSetBusWidth :Error 0x%x\n", e);
    }
    return e;
}

static NvError
MmcConfigureWriteProtection(SdHandle hSd, NvBool PwrWPEnable,
    NvBool PermWPEnable, SdmmcAccessRegion region)
{
    NvError e = NvSuccess;
    NvU32 SdStatus = 0;
    NvDdkSdioCommand *pCmd = &hSd->pCmd;

    SD_SET_COMMAND_PARAMETERS(pCmd,
        MmcCommand_Switch,
        NvDdkSdioCommandType_Normal,
        NV_FALSE,
        0,
        NvDdkSdioRespType_R1b,
        SD_SECTOR_SIZE);

    if (region == SdmmcAccessRegion_BootPartition1)
    {
        pCmd->CmdArgument = ((PwrWPEnable | (PermWPEnable << 2) |
            (!PermWPEnable << 4) | (!PwrWPEnable << 6))) << 8;
        pCmd->CmdArgument |= MMC_BOOT_PARTITION_WP_ARG;
    }
    else if (region == SdmmcAccessRegion_UserArea)
    {
        pCmd->CmdArgument = (PwrWPEnable | (PermWPEnable << 2) |
            (!PwrWPEnable << 3) | (!PermWPEnable << 4)) << 8;
        pCmd->CmdArgument |= MMC_USER_PARTITION_WP_ARG;
    }
    else
    {
        e = SET_ERROR(NvError_BadParameter, pCmd->CommandCode);
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd, &SdStatus));
    if(SdStatus != NvDdkSdioError_None)
    {
        e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
        goto fail;
    }

    // Get the command response
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
        hSd->hSdDdk,
        pCmd->CommandCode,
        pCmd->ResponseType,
        hSd->Response));

    // Wait for the card to not be busy
    NV_CHECK_ERROR_CLEANUP(WaitCardNoTransferState(hSd));

fail:
    if (e)
    {
        SD_ERROR_PRINT("MmcConfigureWriteProtection :Error 0x%x\n", e);
    }
    return e;
}

static NvError MmcSwitchToHighSpeed( SdHandle hSd, NvU32 SdSpeedArgument )
{
    NvError e = NvError_BadParameter;
    NvDdkSdioCommand *pCmd = &hSd->pCmd;
    NvU32 SdStatus = 0;

    SD_SET_COMMAND_PARAMETERS(pCmd,
        MmcCommand_Switch,
        NvDdkSdioCommandType_Normal,
        NV_FALSE,
        SdSpeedArgument,
        NvDdkSdioRespType_R1b,
        SD_SECTOR_SIZE);
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd, &SdStatus));
    if (NvDdkSdioError_CommandTimeout == SdStatus)
    {
        e = SET_ERROR(NvError_SdioCommandFailed , pCmd->CommandCode);
        goto fail;
    }

    // check card version
    // Get the command response
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(hSd->hSdDdk,
        pCmd->CommandCode,
        pCmd->ResponseType,
        hSd->Response));

    // Wait for the card to not be busy
    NV_CHECK_ERROR_CLEANUP(WaitCardNoTransferState(hSd));

fail:
    if (e)
    {
        SD_ERROR_PRINT("MmcSwitchtoHighSpeed :Error 0x%x\n", e);
    }
    return e;
}

#ifdef NV_EMBEDDED_BUILD
// This function returns true if the eMMC card vendor
// is Micron and the part is N2M400JC0345K10E
static NvBool
IsMICRONN2M400JC0345K10E(SdHandle hSd)
{
    if (((hSd->CardIdReg[1] & ~0xFF) == 0x32476800) &&
        (hSd->CardIdReg[2] == 0X4D4D4333) &&
        ((hSd->CardIdReg[3] & 0X00FF03FF) == 0X00FE014E))
        return NV_TRUE;

    return NV_FALSE;
}
#endif

static NvError ReadExtCSD(SdHandle hSd, NvU32 Instance)
{

    NvError e = NvSuccess;
    NvU32 TransferSize = 512; // EXT_CSD is 512 bytes
    NvU8* pDstVirtBuffer = NULL;
    NvU32 SdStatus = 0;
    NvDdkSdioCommand *pCmd = &hSd->pCmd;
    const NvOdmQuerySdioInterfaceProperty* pOdmSdio;

    pOdmSdio = NvOdmQueryGetSdioInterfaceProperty(Instance);
    if(!pOdmSdio)
    {
        e = NvError_BadValue;
        goto fail;
    }

    pDstVirtBuffer = NvOsAlloc(TransferSize);
    if (!pDstVirtBuffer)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    SD_SET_COMMAND_PARAMETERS(pCmd,
        MmcCommand_ReadExtCSD,
        NvDdkSdioCommandType_Normal,
        NV_FALSE,
        0,
        NvDdkSdioRespType_R1,
        SD_SECTOR_SIZE);

    // Read EXT CSD using CMD8
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioRead(hSd->hSdDdk,
        TransferSize,
        (void *)pDstVirtBuffer,
        pCmd,
        NV_TRUE,
        &SdStatus));
    if (SdStatus != NvDdkSdioError_None)
    {
        e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
        hSd->hSdDdk,
        pCmd->CommandCode,
        pCmd->ResponseType,
        hSd->Response));

    // Gets Number of blocks
    // Ext CSD -
    // Sector Count SEC_COUNT 4 R [215:212]
    if (hSd->IsSdhc)
    {
        hSd->NumOfBlocks = (NvU32)(pDstVirtBuffer[212] | (pDstVirtBuffer[213] << 8) |
                                  (pDstVirtBuffer[214] << 16) | (pDstVirtBuffer[215] << 24));
    }

    // get high capacity erase and write-protect size
    hSd->IsEnabledEraseGrpDef =
            (0x1 & pDstVirtBuffer[EXTCSD_ERASE_GROUP_DEF_BYTE_OFFSET]);
    if (hSd->IsEnabledEraseGrpDef)
    {
        hSd->EraseGrpSize = pDstVirtBuffer[224] << SD_HC_ERASE_UNIT_SIZE_LOG2;
        hSd->EraseGrpSize /= SD_SECTOR_SIZE;
        hSd->WPGrpSize = pDstVirtBuffer[221] * hSd->EraseGrpSize;
    }

    hSd->CardType = pDstVirtBuffer[196];
#if ENABLE_HS200_MODE_SUPPORT
    // Check if the card support HS200i mode, For emmc it set as SDR104
    if ((hSd->CardType & (MMC_DEVICE_TYPE_HS200_SDR_200MHZ_1_8_V |
          MMC_DEVICE_TYPE_HS200_SDR_200MHZ_1_2_V)) &&
          hSd->SdHostCaps.IsSDR104modeSupported)
    {
        hSd->Uhsmode = NvDdkSdioUhsMode_SDR104;
    }

    // Check if the card support DDR50 mode
    else
#endif
    if ((hSd->CardType & (MMC_DEVICE_TYPE_HIGHSPEED_DDR_52MHZ_1_8_V |
               MMC_DEVICE_TYPE_HIGHSPEED_DDR_52MHZ_1_2_V)) &&
               hSd->SdHostCaps.IsDDR50modeSupported)
    {
        hSd->Uhsmode = NvDdkSdioUhsMode_DDR50;
    }

    // Check if the card support SDR50 mode
    else if ((hSd->CardType & (MMC_DEVICE_TYPE_HIGHSPEED_52MHZ)) &&
          hSd->SdHostCaps.IsSDR50modeSupported)
    {
        hSd->Uhsmode = NvDdkSdioUhsMode_SDR50;
    }
    else
    {
        hSd->Uhsmode = NvDdkSdioUhsMode_SDR25;
    }
    SD_INFO_PRINT("Uhs Mode = %d\n", hSd->Uhsmode);

    if (pDstVirtBuffer[173] & 0xFF)
    {
        NV_CHECK_ERROR_CLEANUP(MmcConfigureWriteProtection(
            hSd, NV_FALSE, NV_FALSE, SdmmcAccessRegion_BootPartition1));
    }

    if (pDstVirtBuffer[171] & 0xFF)
    {
        NV_CHECK_ERROR_CLEANUP(MmcConfigureWriteProtection(
            hSd, NV_FALSE, NV_FALSE, SdmmcAccessRegion_UserArea));
    }

    // Check if the card supports secure erase operation.
    if (pDstVirtBuffer[231] & 0x1)
    {
        hSd->IsSecureEraseSupported = NV_TRUE;
    }

    // Check if the card supports trim erase operation.
    if (pDstVirtBuffer[231] & (1 << 4))
    {
        hSd->IsTrimEraseSupported = NV_TRUE;
    }

    // Check if the card supports sanitize operation.
    if (pDstVirtBuffer[231] & (1 << 6))
    {
        hSd->IsSanitizeSupported = NV_TRUE;
    }

    hSd->ErasedMemContent = pDstVirtBuffer[181];
    if (pOdmSdio->usage == NvOdmQuerySdioSlotUsage_Boot)
    {
        hSd->BootPartitionSize = (NvU32)(pDstVirtBuffer[226] << (7 + 10));/* (128 * 1024) */
    }

    hSd->BootConfig = pDstVirtBuffer[179];

fail:
    if (pDstVirtBuffer)
    {
        NvOsFree(pDstVirtBuffer);
        pDstVirtBuffer = NULL;
    }
    if (e)
    {
        SD_ERROR_PRINT("ReadExtCSD :Error 0x%x\n", e);
    }
    return e;

}

// Function to erase given number of erase group size units
// given erase start address
// Assumption the erase boundaries are perfectly aligned to
// units of SD erasable group size
static NvError
SdUtilEraseGroupSize(SdBlockDevHandle hSdBlockDev,
    NvU32 StartSector,
    NvU32 NumOfSectors, SdmmcAccessRegion Region)
{
    NvU32 EndSector = 0;
    NvU32 SdStatus;
    SdHandle hSd = hSdBlockDev->hDev;
    NvDdkSdioCommand *pCmd = &hSd->pCmd;
    NvError e = NvSuccess;

    if (NumOfSectors)
    {
        NvOsDebugPrintf("\r\nRegion=%d SD Erase start 512B-sector=%d,"
            "512B-sector-num=%d ", Region, StartSector, NumOfSectors);
    }
    // This workaround is needed to format cards that are slow to respond
    while (NumOfSectors)
    {
        // Make the start aligned to EMMC_MAX_ERASABLE_SECTORS for big erase
        if (NumOfSectors >= EMMC_MAX_ERASABLE_SECTORS)
        {
            NvU32 UnalignedSectors;
            UnalignedSectors =
                MACRO_MOD_LOG2NUM(StartSector, EMMC_MAX_ERASABLE_SECTORS_LOG2);
                //(StartSector % EMMC_MAX_ERASABLE_SECTORS);
            if (UnalignedSectors == 0)
            {
                EndSector = EMMC_MAX_ERASABLE_SECTORS;
            }
            else
            {
                // Trying erase less than EMMC_MAX_ERASABLE_SECTORS blocks
                EndSector = (EMMC_MAX_ERASABLE_SECTORS - UnalignedSectors);
            }
        }
        else
        {
            // Expecting multiple of erase group size sectors
            if (NumOfSectors % hSd->EraseGrpSize)
            {
                SD_ERROR_PRINT("\r\nErase size %d sectors for erase grp=%d ",
                    NumOfSectors, hSd->EraseGrpSize);
            }
            // Trying erase less than EMMC_MAX_ERASABLE_SECTORS blocks
            EndSector = NumOfSectors;
        }

        SD_SET_COMMAND_PARAMETERS(pCmd,
             (hSd->IsMMC ? MmcCommand_EraseWriteBlockStart :
             SdCommand_EraseWriteBlockStart),
             NvDdkSdioCommandType_Normal,
             NV_FALSE,
             (hSd->IsSdhc ? StartSector :
             StartSector * SD_SECTOR_SIZE),
             NvDdkSdioRespType_R1,
             SD_SECTOR_SIZE);

        NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(
            hSd->hSdDdk, pCmd,
            &SdStatus));
        if (SdStatus)
        {
            e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
            goto fail;
        }

        SD_SET_COMMAND_PARAMETERS(pCmd,
             (hSd->IsMMC)?(MmcCommand_EraseWriteBlockEnd):
             (SdCommand_EraseWriteBlockEnd),
             NvDdkSdioCommandType_Normal,
             NV_FALSE,
             hSd->IsSdhc ? (StartSector + EndSector - 1):
                ((StartSector + EndSector - 1) * SD_SECTOR_SIZE),
             NvDdkSdioRespType_R1,
             SD_SECTOR_SIZE);

        NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(
            hSd->hSdDdk, pCmd,
            &SdStatus));
        if (SdStatus)
        {
            e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
            goto fail;
        }

        SD_SET_COMMAND_PARAMETERS(pCmd,
             (hSd->IsMMC)?(MmcCommand_Erase):
             (SdCommand_Erase),
             NvDdkSdioCommandType_Normal,
             NV_FALSE,
             (hSd->DoSecureErase ? 0x80000000 : 0x0),
             NvDdkSdioRespType_R1b,
             SD_SECTOR_SIZE);

        NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(
            hSd->hSdDdk, pCmd,
            &SdStatus));
        if (SdStatus)
        {
            e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
            goto fail;
        }

        NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
            hSd->hSdDdk,
            pCmd->CommandCode,
            pCmd->ResponseType,
            hSd->Response));

        if (hSd->Response[0]&ERASE_CMD_ERROR)
        {
            e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
            goto fail;
        }

        // Wait for the card to not be busy
        NV_CHECK_ERROR_CLEANUP(WaitCardNoTransferState(hSd));

#ifdef NV_EMBEDDED_BUILD
        /* This is the hack to temporary support format all devices.
           The new Numonyx mNand is nearly 100 times slower in
           erasing then old mNands. The hack should be remove once
           the slow erase issue gets resolved */

        if (IsMICRONN2M400JC0345K10E(hSdBlockDev->hDev) && StartSector >= 106496)
        {
            NvOsDebugPrintf("Breaking out...\r\n");
            break;
        }
#endif
        NumOfSectors -= EndSector;
        StartSector += EndSector;
    }

fail:
    if (e != NvSuccess)
    {
        SD_ERROR_PRINT("\r\nErase from start sector=%d for %d sectors "
            "failed  with error 0x%x\n",
            (StartSector >> hSdBlockDev->hDev->SectorsPerPageLog2),
            (NumOfSectors >> hSdBlockDev->hDev->SectorsPerPageLog2), e);
    }
    return e;
}

static NvError
SdUtilTrimErase(
    SdBlockDevHandle hSdBlockDev,
    NvU32 StartSector,
    NvU32 NumOfSectors,
    SdmmcAccessRegion Region)
{
    NvU32 SdStatus;
    NvU32 SectorsToErase = 0;
    SdHandle hSd = hSdBlockDev->hDev;
    NvDdkSdioCommand *pCmd = &hSd->pCmd;
    NvError e = NvSuccess;

    NV_ASSERT(hSd->DoTrimErase);

    /* This workaround is needed to format cards that are slow to respond */
    while (NumOfSectors)
    {
        /* Make the start aligned to EMMC_MAX_ERASABLE_SECTORS for big erase */
        if (NumOfSectors >= EMMC_MAX_ERASABLE_SECTORS)
        {
            NvU32 UnalignedSectors;
            UnalignedSectors =
                MACRO_MOD_LOG2NUM(StartSector, EMMC_MAX_ERASABLE_SECTORS_LOG2);
            if (UnalignedSectors == 0)
            {
                SectorsToErase = EMMC_MAX_ERASABLE_SECTORS;
            }
            else
            {
                /* Trying erase less than EMMC_MAX_ERASABLE_SECTORS blocks */
                SectorsToErase = (EMMC_MAX_ERASABLE_SECTORS - UnalignedSectors);
            }
        }
        else
        {
            /* Trying erase less than EMMC_MAX_ERASABLE_SECTORS blocks */
            SectorsToErase = NumOfSectors;
        }

        SD_SET_COMMAND_PARAMETERS(pCmd,
             MmcCommand_EraseWriteBlockStart,
             NvDdkSdioCommandType_Normal,
             NV_FALSE,
             (hSd->IsSdhc ? StartSector :
             StartSector * SD_SECTOR_SIZE),
             NvDdkSdioRespType_R1,
             SD_SECTOR_SIZE);

        NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(
            hSd->hSdDdk, pCmd,
            &SdStatus));
        if (SdStatus)
        {
            e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
            goto fail;
        }

        SD_SET_COMMAND_PARAMETERS(pCmd,
             MmcCommand_EraseWriteBlockEnd,
             NvDdkSdioCommandType_Normal,
             NV_FALSE,
             hSd->IsSdhc ? (StartSector + SectorsToErase - 1):
                ((StartSector + SectorsToErase - 1) * SD_SECTOR_SIZE),
             NvDdkSdioRespType_R1,
             SD_SECTOR_SIZE);

        NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(
            hSd->hSdDdk, pCmd,
            &SdStatus));
        if (SdStatus)
        {
            e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
            goto fail;
        }

        SD_SET_COMMAND_PARAMETERS(pCmd,
             MmcCommand_Erase,
             NvDdkSdioCommandType_Normal,
             NV_FALSE,
             (hSd->DoSecureErase ? 0x80000001 : 0x1),
             NvDdkSdioRespType_R1b,
             SD_SECTOR_SIZE);

        NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(
            hSd->hSdDdk, pCmd,
            &SdStatus));
        if (SdStatus)
        {
            e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
            goto fail;
        }

        NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
            hSd->hSdDdk,
            pCmd->CommandCode,
            pCmd->ResponseType,
            hSd->Response));

        if (hSd->Response[0] & ERASE_CMD_ERROR)
        {
            e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
            goto fail;
        }

        // Wait for the card to not be busy
        NV_CHECK_ERROR_CLEANUP(WaitCardNoTransferState(hSd));

        if (hSd->DoSecureErase)
        {
            /* Secure Trim erase step2 */
            SD_SET_COMMAND_PARAMETERS(pCmd,
                MmcCommand_EraseWriteBlockStart,
                NvDdkSdioCommandType_Normal,
                NV_FALSE,
                (hSd->IsSdhc ? StartSector :
                StartSector * SD_SECTOR_SIZE),
                NvDdkSdioRespType_R1,
                SD_SECTOR_SIZE);

            NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(
                hSd->hSdDdk, pCmd, &SdStatus));
            if (SdStatus)
            {
                e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
                goto fail;
            }

            SD_SET_COMMAND_PARAMETERS(pCmd,
                MmcCommand_EraseWriteBlockEnd,
                NvDdkSdioCommandType_Normal,
                NV_FALSE,
                hSd->IsSdhc ? (StartSector + SectorsToErase - 1):
                    ((StartSector + SectorsToErase - 1) * SD_SECTOR_SIZE),
                NvDdkSdioRespType_R1,
                SD_SECTOR_SIZE);

            NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(
                hSd->hSdDdk, pCmd, &SdStatus));
            if (SdStatus)
            {
                e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
                goto fail;
            }

            SD_SET_COMMAND_PARAMETERS(pCmd,
                MmcCommand_Erase,
                NvDdkSdioCommandType_Normal,
                NV_FALSE,
                0x80008000,
                NvDdkSdioRespType_R1b,
                SD_SECTOR_SIZE);

            NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(
                hSd->hSdDdk, pCmd, &SdStatus));
            if (SdStatus)
            {
                e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
                goto fail;
            }

            NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
                hSd->hSdDdk,
                pCmd->CommandCode,
                pCmd->ResponseType,
                hSd->Response));

            if (hSd->Response[0] & ERASE_CMD_ERROR)
            {
                e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
                goto fail;
            }

            // Wait for the card to not be busy
            NV_CHECK_ERROR_CLEANUP(WaitCardNoTransferState(hSd));
        }
        NumOfSectors -= SectorsToErase;
        StartSector += SectorsToErase;
    }

fail:
    if (e != NvSuccess)
    {
        SD_ERROR_PRINT("\r\nErase from start sector=%d for %d sectors "
            "failed  with error 0x%x\n",
            (StartSector >> hSdBlockDev->hDev->SectorsPerPageLog2),
            (NumOfSectors >> hSdBlockDev->hDev->SectorsPerPageLog2), e);
    }
    return e;
}

// This function returns the region relative address given StartSector
// and the number of sectors. Region type in which the start lies
// is also returned. This function uses address as 512B sectors
static void
UtilGetRegionRelativeAddress(SdHandle hSd,
    NvU32 *pRelStart, NvU32 *pNumSectors, SdmmcAccessRegion *pRegion)
{
    NvU32 EndSector;
    NvU32 BlocksPerPartition = hSd->BootPartitionSize >> SD_SECTOR_SZ_LOG2;
    NvU32 BlockStart = *pRelStart;
    NvU32 BlockEnd;
    EndSector = BlockStart;
    BlockEnd = (!pNumSectors)? (*pRelStart + 1) : (*pRelStart + *pNumSectors);
    if (BlockStart < BlocksPerPartition)
    {
        *pRegion = SdmmcAccessRegion_BootPartition1;
        if (pNumSectors)
        {
            EndSector = (BlockEnd >= BlocksPerPartition)?
                BlocksPerPartition : BlockEnd;
        }
        // Start Sector within region obtained
    }
    else if (BlockStart < (BlocksPerPartition << 1))
    {
        *pRegion = SdmmcAccessRegion_BootPartition2;
        if (pNumSectors)
        {
            EndSector = (BlockEnd >= (BlocksPerPartition << 1))?
                (BlocksPerPartition << 1) : BlockEnd;
        }
        // Start Sector within region - BootPart2 starts after BootPart1
        *pRelStart -= BlocksPerPartition;
    }
    else
    {
        *pRegion = SdmmcAccessRegion_UserArea;
        if (pNumSectors)
            EndSector = BlockEnd;
        // Start Sector within region - UsrPart starts after BootPart2
        *pRelStart -= (BlocksPerPartition << 1);
    }
    if (pNumSectors)
    {
        *pNumSectors = (EndSector - BlockStart);
    }
}

static NvError
MmcSelectPartition(SdHandle hSd, NvU32 region)
{
    NvError e = NvSuccess;
    NvU32 SdStatus;
    NvDdkSdioCommand *pCmd;

    if (hSd->CurrentRegion == region)
    {
        goto fail;
    }

    SdStatus = 0;
    pCmd = &hSd->pCmd;
    SD_SET_COMMAND_PARAMETERS(pCmd,
        MmcCommand_Switch,
        NvDdkSdioCommandType_Normal,
        NV_FALSE,
        0,
        NvDdkSdioRespType_R1b,
        SD_SECTOR_SIZE);

        pCmd->CmdArgument = (NvU32)(hSd->BootConfig & (~0x7));
        pCmd->CmdArgument |= region;
        pCmd->CmdArgument <<= EMMC_SWITCH_SELECT_PARTITION_OFFSET;
        pCmd->CmdArgument |= EMMC_SWITCH_SELECT_PARTITION_ARG;
    NV_CHECK_ERROR(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd, &SdStatus));
    if(SdStatus != NvDdkSdioError_None)
    {
        e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
        goto fail;
    }

    // Get the command response
    NV_CHECK_ERROR(NvDdkSdioGetCommandResponse(hSd->hSdDdk,
                                                                                          pCmd->CommandCode,
                                                                                          pCmd->ResponseType,
                                                                                          hSd->Response));
    // Wait for the card to not be busy
    NV_CHECK_ERROR_CLEANUP(WaitCardNoTransferState(hSd));

    hSd->CurrentRegion = region;

fail:
    return e;
}

// Function to return the bounds of first region
// Input: Sector Range - pStartSector and pNumSectors
// Output: first region bounds in pStartSector and pNumSectors,
//             count of sectors beyond returned region(which is subset
//             of original input pNumSectors).
// FIXME: anytime MmcSelectRegion changes, we need to revisit
//          implementation of UtilGetMmcFirstRegionBounds
//          used to erase given sector range
static NvError UtilGetMmcFirstRegionBounds(SdHandle hSd,
    NvU32 *pNumSectors, NvU32 *pRelStart, NvU32 *pUnusedSectors,
    SdmmcAccessRegion *pRegion)
{
    NvError e = NvSuccess;
    if (!hSd->BootPartitionSize)
    {
        *pUnusedSectors = 0; // we don't have support for regions
        return e; // for v4.2 cards, there is no boot partition
    }
    if ((!pNumSectors) || (!(*pNumSectors)))
    {
        return e; // for v4.2 cards, there is no boot partition
    }
    else
    {
        if (pUnusedSectors && pNumSectors)
            *pUnusedSectors = *pNumSectors;
        // Get region relative start, the region in which start address lies
        UtilGetRegionRelativeAddress(hSd, pRelStart,
            pNumSectors, pRegion);
        // Set remaining sectors beyond first region
        if (pUnusedSectors && pNumSectors)
            *pUnusedSectors -= *pNumSectors;
        // Select the region only for non-zero sector count
        NV_CHECK_ERROR(MmcSelectPartition(hSd, *pRegion));
    }

    return NvSuccess;
}

static NvError MmcSelectRegion(SdHandle hSd, NvU32* Block, NvU32 *pNumOfBlocks, NvBool AllowPartitionSwitch)
{

    SdmmcAccessRegion region = SdmmcAccessRegion_UserArea;
    NvError e = NvSuccess;

    if (!hSd->BootPartitionSize)
    {
        return e; // for v4.2 cards, there is no boot partition
    }
    else
    {
        NvU32 UnusedSectors;
        // Get the region corresponding to address *Block, get region relative
        // address RelStart. This function uses 512B sectors now.
        *Block <<= hSd->SectorsPerPageLog2;
        *pNumOfBlocks <<= hSd->SectorsPerPageLog2;
        // Get the region relative 512B-sector address and count
        e = UtilGetMmcFirstRegionBounds(hSd,
            pNumOfBlocks, Block, &UnusedSectors, &region);
        if (e != NvSuccess)
        {
            SD_ERROR_PRINT("\r\nError UtilGetMmcFirstRegionBounds e=0x%x ", e);
            return e;
        }
        // Get the region relative address and count as
        // block driver reported sector size
        *Block >>=  hSd->SectorsPerPageLog2;
        *pNumOfBlocks >>=  hSd->SectorsPerPageLog2;
    }

    if (AllowPartitionSwitch == NV_TRUE)
    {
        NV_CHECK_ERROR(MmcSelectPartition(hSd, region));
    }

    return e;
}

static NvError
SendInterfaceCondition( SdHandle hSd, NvU32 *HostCapacitySupport)
{
    NvU32 SdioStatus = 0;
    NvError e = NvSuccess;
    NvDdkSdioCommand *pCmd = &hSd->pCmd;

    SD_SET_COMMAND_PARAMETERS(pCmd,
        SdCommand_SendIfCond,
        NvDdkSdioCommandType_Normal,
        NV_FALSE,
        (SD_HOST_VOLTAGE_RANGE |
         SD_HOST_CHECK_PATTERN),
        NvDdkSdioRespType_R7,
        SD_SECTOR_SIZE);
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd, &SdioStatus));

    // SdioStatus = NvDdkSdioError_None indicates that we have a
    // SDHC card, else, the card is a normal SD card and
    // SendInterfaceCondition command is not applicable
    if (SdioStatus == NvDdkSdioError_None)
    {
        NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
            hSd->hSdDdk,
            pCmd->CommandCode,
            pCmd->ResponseType,
            hSd->Response));

        // The card is SDHC, check the response
        *HostCapacitySupport = 1;

        // Check the voltage range
        if (((hSd->Response[0] & 0xF00) >> 8) != (SD_HOST_VOLTAGE_RANGE >> 8))
        {
            // card does not support the voltage range we requested,
            // fallback to 1.x mode (Non-SDHC)
            *HostCapacitySupport = 0;
        }

        // Check the pattern integrity
        if ((hSd->Response[0] & 0xFF) != SD_HOST_CHECK_PATTERN)
        {
            // integrity check should not fail. If failed, this may mean
            // that there is noise on the CMD line.
            e = SET_ERROR(NvError_SdioSdhcPatternIntegrityFailed, pCmd->CommandCode);
            goto fail;
        }
    }
    else if (SdioStatus != NvDdkSdioError_CommandTimeout)
    {
        e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
        goto fail;
    }
fail:
    if (e)
    {
        SD_ERROR_PRINT("SendInterfaceConditions :Error 0x%x\n", e);
    }
    return e;
}

static NvError
ReadOperatingControlRegister(
    SdHandle hSd,
    NvU32 HostCapacitySupport,
    NvU32 *CardCapacitySupport)
{
    NvU32 SdioStatus = 0;
    NvError e = NvSuccess;
    NvU32 OCRRegister = 0;
    NvDdkSdioCommand *pCmd = &hSd->pCmd;

    do
    {
        SD_SET_COMMAND_PARAMETERS(pCmd,
            SdCommand_ApplicationCommand,
            NvDdkSdioCommandType_Normal,
            NV_FALSE,
            0,
            NvDdkSdioRespType_R1,
            SD_SECTOR_SIZE);
        NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(
            hSd->hSdDdk,
            pCmd,
            &SdioStatus));
        if (SdioStatus == NvDdkSdioError_CommandTimeout)
        {
            // If command timed out, this indicates that the
            // card is not present or gone bad
            e = SET_ERROR(NvError_SdioCardNotPresent, pCmd->CommandCode);
            goto fail;
        }

        if (SdioStatus != NvDdkSdioError_None)
        {
            e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
            goto fail;
        }

        NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
            hSd->hSdDdk,
            pCmd->CommandCode,
            pCmd->ResponseType,
            hSd->Response));

        OCRRegister = HostCapacitySupport << 30;
        OCRRegister |= SD_CARD_OCR_VALUE;
        SD_SET_COMMAND_PARAMETERS(pCmd,
            SdAppCmd_SendOcr,
            NvDdkSdioCommandType_Normal,
            NV_FALSE,
            OCRRegister,
            NvDdkSdioRespType_R3,
            SD_SECTOR_SIZE);
        NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(
            hSd->hSdDdk,
            pCmd,
            &SdioStatus));
        if (SdioStatus != NvDdkSdioError_None)
        {
            e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
            goto fail;
        }

        NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
            hSd->hSdDdk,
            pCmd->CommandCode,
            pCmd->ResponseType,
            hSd->Response));

        OCRRegister = hSd->Response[0];
        if (OCRRegister == 0)
        {
            // Indicates no card is present in the slot
            e = SET_ERROR(NvError_SdioCardNotPresent, SdAppCmd_SendOcr);
            goto fail;
        }
    } while (!(OCRRegister & (NvU32)(SD_CARD_POWERUP_STATUS_MASK)));

    // Card powered up, check if the card supports SDHC
    // If OCRRegister is non-zero, this indicates that the
    // card supports SDHC
    *CardCapacitySupport = (OCRRegister & SD_CARD_CAPACITY_MASK);
fail:
    if (e)
    {
        SD_ERROR_PRINT("ReadOperatingControlRegister :Error 0x%x\n", e);
    }

    return e;
}

static NvError
ReadCardIdentification(SdHandle hSd)
{
    NvU32 SdioStatus = 0;
    NvError e = NvSuccess;
    NvDdkSdioCommand *pCmd = &hSd->pCmd;

    SD_SET_COMMAND_PARAMETERS(pCmd,
        (hSd->IsMMC)? SdCommand_AllSendCid : MmcCommand_AllSendCid,
        NvDdkSdioCommandType_Normal,
        NV_FALSE,
        0,
        NvDdkSdioRespType_R2,
        SD_SECTOR_SIZE);
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd, &SdioStatus));
    if (SdioStatus != NvDdkSdioError_None)
    {
        e = SET_ERROR (NvError_SdioCommandFailed, pCmd->CommandCode);
        goto fail;
    }
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
        hSd->hSdDdk,
        pCmd->CommandCode,
        pCmd->ResponseType,
        hSd->Response));

    hSd->CardIdReg[0] = hSd->Response[0];
    hSd->CardIdReg[1] = hSd->Response[1];
    hSd->CardIdReg[2] = hSd->Response[2];
    hSd->CardIdReg[3] = hSd->Response[3];

fail:
    if (e)
    {
        SD_ERROR_PRINT("ReadCardIdentificationregister :Error 0x%x\n", e);
    }
    return e;
}

static NvError
ReadRelativeCardAddress(SdHandle hSd)
{
    NvU32 SdioStatus = 0;
    NvError e = NvSuccess;
    NvDdkSdioCommand *pCmd = &hSd->pCmd;

    SD_SET_COMMAND_PARAMETERS(pCmd,
        SdCommand_SetRelativeAddress,
        NvDdkSdioCommandType_Normal,
        NV_FALSE,
        0,
        NvDdkSdioRespType_R6,
        SD_SECTOR_SIZE);
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd, &SdioStatus));
    if (SdioStatus != NvDdkSdioError_None)
    {
        e = SET_ERROR (NvError_SdioCommandFailed, pCmd->CommandCode);
        goto fail;
    }
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
        hSd->hSdDdk,
        pCmd->CommandCode,
        pCmd->ResponseType,
        hSd->Response));
    hSd->Rca = (hSd->Response[0] >> 16);
fail:
    if (e)
    {
        SD_ERROR_PRINT("ReadRelativeCardAddress :Error 0x%x\n", e);
    }
    return e;
}

static NvError
SelectDeselectCard(SdHandle hSd, NvBool isSelect)
{
    NvU32 SdioStatus = 0;
    NvError e = NvSuccess;
    NvDdkSdioCommand *pCmd = &hSd->pCmd;

    SD_SET_COMMAND_PARAMETERS(pCmd,
        SdCommand_SelectDeselectCard,
        NvDdkSdioCommandType_Normal,
        NV_FALSE,
        (NvU32)(hSd->Rca << 16),
        (isSelect)? NvDdkSdioRespType_R1b :
                          NvDdkSdioRespType_NoResp,
        SD_SECTOR_SIZE);
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd, &SdioStatus));
    if (SdioStatus != NvDdkSdioError_None)
    {
        e = SET_ERROR (NvError_SdioCommandFailed, pCmd->CommandCode);
        goto fail;
    }
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
        hSd->hSdDdk,
        pCmd->CommandCode,
        pCmd->ResponseType,
        hSd->Response));
fail:
    if (e)
    {
        SD_ERROR_PRINT("SelectDeselectCard :Error 0x%x\n", e);
    }
    return e;
}

static void MmcDecodeCSD(SdHandle hSd)
{
    NvU32 CSize;
    NvU32 CSizeMult;
    NvU32 ReadBlLength;
    NvU32 EraseGrpSize_CSD;
    NvU32 EraseGrpSizeMult;
    NvU32 WriteBlLength;

    /*
     * Calculating No of Blocks. This is valid for 2GB cards only.
     * BLOCKNR = (C_SIZE+1) * MULT
     * MULT = 2^(C_SIZE_MULT+2), where (C_SIZE_MULT < 8)
     * BLOCK_LEN = 2^READ_BL_LEN, where (READ_BL_LEN < 12)
     * Therefore, the maximal capacity which can be coded is 4096*512*2048 =
     * 4 GBytes. Example: A 4 MByte mmc with BLOCK_LEN = 512 can be
     * coded by C_SIZE_MULT = 0 and C_SIZE = 2047.
     */
    // [73:62]
    CSize = ((hSd->Response[1] & 0xFFC00000) >> 22) |
            ((hSd->Response[2] & 0x3) << 10);

    // [49:47]
    CSizeMult = ((hSd->Response[1] & 0x380) >> 7);

    // [83:80]
    ReadBlLength = ((hSd->Response[2] & 0xF00) >> 8);

    hSd->NumOfBlocks = (CSize + 1) << (CSizeMult + 2);
    if (ReadBlLength >= SD_SECTOR_SZ_LOG2)
    {
        hSd->NumOfBlocks = hSd->NumOfBlocks <<
          (ReadBlLength - SD_SECTOR_SZ_LOG2);
    }
    else
    {
        hSd->NumOfBlocks = hSd->NumOfBlocks >>
          (SD_SECTOR_SZ_LOG2 - ReadBlLength);
    }

    EraseGrpSizeMult = ((hSd->Response[0] & 0xE0000000) >> 29) | ((hSd->Response[1]&0x3) << 3);
    EraseGrpSize_CSD = ((hSd->Response[1] & 0x7C) >> 2);
    hSd->EraseGrpSize = ((EraseGrpSize_CSD + 1) * (EraseGrpSizeMult + 1));
    WriteBlLength = (1 << ((hSd->Response[0] & 0x3C000) >> 14));
    hSd->EraseGrpSize *= WriteBlLength;
    hSd->EraseGrpSize /= SD_SECTOR_SIZE;
    hSd->WPGrpSize = ((hSd->Response[0]&0x1F000000) >> 24);
    hSd->WPGrpSize *= hSd->EraseGrpSize;

}

#if 0
static NvError MmcEnableHighCapacityErase( SdHandle hSd )
{
    NvError e = NvError_BadParameter;
    NvDdkSdioCommand *pCmd = &hSd->pCmd;
    NvU32 SdStatus = 0;

    SD_SET_COMMAND_PARAMETERS(pCmd,
                                                          MmcCommand_Switch,
                                                          NvDdkSdioCommandType_Normal,
                                                          NV_FALSE,
                                                          MMC_ENABLE_HIGH_CAPACITY_ERASE_ARG,
                                                          NvDdkSdioRespType_R1b,
                                                          SD_SECTOR_SIZE);
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd, &SdStatus));
    if (NvDdkSdioError_CommandTimeout == SdStatus)
    {
        e = SET_ERROR(NvError_SdioCommandFailed , pCmd->CommandCode);
        goto fail;
    }

    // check card version
    // Get the command response
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(hSd->hSdDdk,
                                                                                          pCmd->CommandCode,
                                                                                          pCmd->ResponseType,
                                                                                          hSd->Response));

fail:
    if (e)
    {
        SD_ERROR_PRINT("MmcHighCapacity erase :Error 0x%x\n", e);
    }
    return e;
}
#endif

static NvError
SdIdentifyCard(
    SdHandle hSd,
    NvU32 Instance)
{
    NvError e = NvSuccess;
    NvU32 ClockRateInKHz;
    NvU32 ConfiguredClockRate = 0;
    NvU32 HostCapacitySupport = 0, CardCapacitySupport = 0;

    // Check if the card is SDHC,
    // issue CMD8 to get the interface condition register
    NV_CHECK_ERROR_CLEANUP(SendInterfaceCondition(hSd, &HostCapacitySupport));

    // Send ACMD41 to read the operating control register
    // and wake up the card
    NV_CHECK_ERROR_CLEANUP(ReadOperatingControlRegister(hSd,
        HostCapacitySupport,
        &CardCapacitySupport));

    // Finally, check if we have SDHC or not
    hSd->IsSdhc = (HostCapacitySupport && CardCapacitySupport)? NV_TRUE :
        NV_FALSE;

    // Send CMD2 to read the card identification data
    NV_CHECK_ERROR_CLEANUP(ReadCardIdentification(hSd));

    // Send CMD3 to read the relative card address
    NV_CHECK_ERROR_CLEANUP(ReadRelativeCardAddress(hSd));

    // Send CMD 9 to read card specific data
    NV_CHECK_ERROR_CLEANUP(ReadCsd(hSd));

    // Send CMD 7 to select the card.
    NV_CHECK_ERROR_CLEANUP(SelectDeselectCard(hSd,NV_TRUE));

    if (!IsCardInTransferState(hSd))
    {
        e = SET_ERROR(NvError_InvalidState, SdCommand_SendStatus);
        goto fail;
    }

    // Set clock to normal speed
    ClockRateInKHz = SD_TX_CLOCK_KHZ;
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSetClockFrequency(hSd->hSdDdk,
                    ClockRateInKHz, &ConfiguredClockRate));
    // Switch to high speed (if possible)
    if (hSd->IsSdhc)
    {
        if (SwitchToHighSpeed(hSd) == NvSuccess)
        {
            ClockRateInKHz = SD_SDHC_TX_CLOCK_KHZ;
            // if the DDk is unable to set the clock to high speed,
            // set the clock back to normal speed.
            if (NvDdkSdioSetClockFrequency(hSd->hSdDdk,
                                        ClockRateInKHz,
                                        &ConfiguredClockRate) != NvSuccess)
            {
                // Set clock to normal speed
                ClockRateInKHz = SD_TX_CLOCK_KHZ;
                // If we are unable to set the clock back, return error
                NV_CHECK_ERROR_CLEANUP(NvDdkSdioSetClockFrequency(hSd->hSdDdk,
                        ClockRateInKHz, &ConfiguredClockRate));
            }
        }
    }

    // Send ACMD6 to Set bus width to Four bit wide.
    NV_CHECK_ERROR_CLEANUP(SetBusWidth(hSd, NvDdkSdioDataWidth_4Bit));

    // Send CMD 16 to set block length
    NV_CHECK_ERROR_CLEANUP(SetBlockSize(hSd, SD_SECTOR_SIZE));

fail:
    if (e)
    {
        SD_ERROR_PRINT("SdIdentifycard :Error 0x%x\n", e);
    }
    return e;
}

static NvError
MmcIdentifyCard(
    SdHandle hSd,
    NvU32 Instance)
{
    NvError e = NvSuccess;
    NvU32 ClockRateInKHz;
    NvU32 ConfiguredClockRate = 0;
    NvU32 SdStatus = 0;
    NvDdkSdioCommand *pCmd = &hSd->pCmd;
    NvU32 BusWidth4Bit;
    NvU32 BusWidth8Bit;
    NvU32 SpeedArgument;
#if NVDDK_SDMMC_T124
    NvU32 TapValue, TrimValue;
    const NvOdmQuerySdioInterfaceProperty* pSdioInterfaceCaps;
#endif
#if !NVDDK_SDMMC_T30
    NvU32 Reg;
#endif

    // Send CMD2 to read the card identification data
    NV_CHECK_ERROR_CLEANUP(ReadCardIdentification(hSd));

    // Send CMD3 to assign the relative card address
    hSd->Rca = 1;
    SD_SET_COMMAND_PARAMETERS(pCmd,
        MmcCommand_AssignRCA,
        NvDdkSdioCommandType_Normal,
        NV_FALSE,
        (NvU32)(hSd->Rca << 16),
        NvDdkSdioRespType_R1,
        SD_SECTOR_SIZE);
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd, &SdStatus));
    if (SdStatus != NvDdkSdioError_None)
    {
        e= SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
        hSd->hSdDdk,
        pCmd->CommandCode,
        pCmd->ResponseType,
        hSd->Response));

    // Send CMD 9 to read card specific data
    SD_SET_COMMAND_PARAMETERS(pCmd,
        SdCommand_SendCsd,
        NvDdkSdioCommandType_Normal,
        NV_FALSE,
        (NvU32)(hSd->Rca << 16),
        NvDdkSdioRespType_R2,
        SD_SECTOR_SIZE);

    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd, &SdStatus));
    if(SdStatus != NvDdkSdioError_None)
    {
        e= SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
        hSd->hSdDdk,
        pCmd->CommandCode,
        pCmd->ResponseType,
        hSd->Response));

    MmcDecodeCSD(hSd);

    // Send CMD 7 to select the card.
    SD_SET_COMMAND_PARAMETERS(pCmd,
        MmcCommand_SelectDeselectCard,
        NvDdkSdioCommandType_Normal,
        NV_FALSE,
        (NvU32)(hSd->Rca << 16),
        NvDdkSdioRespType_R1b,
        SD_SECTOR_SIZE);
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd, &SdStatus));
    if(SdStatus != NvDdkSdioError_None)
    {
        e= SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
        hSd->hSdDdk,
        pCmd->CommandCode,
        pCmd->ResponseType,
        hSd->Response));

    // Check if the card is in transfer state
    if (!IsCardInTransferState(hSd))
    {
        e= SET_ERROR(NvError_InvalidState, SdCommand_SendStatus);
        goto fail;
    }

    //Get the ext CSD register
    NV_CHECK_ERROR_CLEANUP(ReadExtCSD(hSd, Instance));

#if NVDDK_SDMMC_T124
    // get the interface property from odm query
    pSdioInterfaceCaps = NvOdmQueryGetSdioInterfaceProperty(Instance);
#endif

    //Switch to HS200 mode if supported
    if (hSd->Uhsmode == NvDdkSdioUhsMode_SDR104)
    {
        SD_INFO_PRINT("Setting to SDR104\n");
        // set buswidth
        if (hSd->DataBusWidth == 4)
        {
            NV_CHECK_ERROR_CLEANUP(MmcSetBusWidth(hSd, MMC_SDR_MODE_4_BIT_BUSWIDTH));
        }
        else if (MmcSetBusWidth(hSd, MMC_SDR_MODE_8_BIT_BUSWIDTH))
        {
            // If 8 bit bus width switch fails, try 4 bit bus width switch.
            NV_CHECK_ERROR_CLEANUP(MmcSetBusWidth(hSd, MMC_SDR_MODE_4_BIT_BUSWIDTH));
            SD_ERROR_PRINT("8-bit buswidth switch failed. Using 4-bit buswidth\n");
        }

        MmcSwitchToHighSpeed(hSd, MMC_HS200_SPEED_ARGUMENT);

        // send Uhs mode
         NV_CHECK_ERROR_CLEANUP(NvDdkSdioSetUhsmode(hSd->hSdDdk, hSd->Uhsmode));

        // If we are unable to set the clock back, return error
        NV_CHECK_ERROR_CLEANUP(NvDdkSdioSetClockFrequency(
            hSd->hSdDdk,
            MMC_HS200_TX_CLOCK_KHZ,
            &ConfiguredClockRate));
        SD_INFO_PRINT("ConfiguredClockRate: %d\n", ConfiguredClockRate);

#if !NVDDK_SDMMC_T30
        // If mode is hs200 and frequency >100 MHz, we need to enable
        // CFG2TMC_GMACFG_HSM_EN bit in APB_MISC_GP_GMACFGPADCTRL_0 register
        if (ConfiguredClockRate > MMC_HSM_EN_FOR_HS200_KHZ)
        {
            NV_MISC_READ(GP_GMACFGPADCTRL, Reg);
            Reg = NV_FLD_SET_DRF_DEF(APB_MISC, GP_GMACFGPADCTRL,
                      CFG2TMC_GMACFG_HSM_EN, ENABLE, Reg);
            NV_MISC_WRITE(GP_GMACFGPADCTRL, Reg);
        }
#endif

        // execute tuning if it is uhs mode
        SD_INFO_PRINT("NvDdkSdioSetUshmode: ExecuteTuning\n");
        NvDdkSdioSetBlocksize(hSd->hSdDdk, 128);
        hSd->hSdDdk->NvDdkSdioExecuteTuning(hSd->hSdDdk);
        NvDdkSdioSetBlocksize(hSd->hSdDdk, 512);
#if NVDDK_SDMMC_T124
        TapValue = pSdioInterfaceCaps->TapValueHs200;
        TrimValue = pSdioInterfaceCaps->TrimValueHs200;
#endif
    }
    else
    {
        if (hSd->Uhsmode == NvDdkSdioUhsMode_DDR50)
        {
            SD_INFO_PRINT("Setting to DDR50\n");
            SpeedArgument = MMC_HIGH_SPEED_ARGUMENT;
            ClockRateInKHz = SD_SDHC_TX_CLOCK_KHZ;
            BusWidth4Bit = MMC_DDR_MODE_4_BIT_BUSWIDTH;
            BusWidth8Bit = MMC_DDR_MODE_8_BIT_BUSWIDTH;
#if NVDDK_SDMMC_T124
            TapValue = pSdioInterfaceCaps->TapValueDdr52;
            TrimValue = pSdioInterfaceCaps->TrimValueDdr52;
#endif
        }
        else if (hSd->Uhsmode == NvDdkSdioUhsMode_SDR50)
        {
            SD_INFO_PRINT("Setting to SDR50\n");
            SpeedArgument = MMC_HIGH_SPEED_ARGUMENT;
            ClockRateInKHz = SD_SDHC_TX_CLOCK_KHZ;
            BusWidth4Bit = MMC_SDR_MODE_4_BIT_BUSWIDTH;
            BusWidth8Bit = MMC_SDR_MODE_8_BIT_BUSWIDTH;
#if NVDDK_SDMMC_T124
            TapValue = pSdioInterfaceCaps->TapValue;
            TrimValue = pSdioInterfaceCaps->TrimValue;
#endif
        }
        else
        {
            SD_INFO_PRINT("Setting to SDR25\n");
            SpeedArgument = MMC_LEGACY_SPEED_ARGUMENT;
            ClockRateInKHz = SD_TX_CLOCK_KHZ;
            BusWidth4Bit = MMC_SDR_MODE_4_BIT_BUSWIDTH;
            BusWidth8Bit = MMC_SDR_MODE_8_BIT_BUSWIDTH;
#if NVDDK_SDMMC_T124
            TapValue = pSdioInterfaceCaps->TapValue;
            TrimValue = pSdioInterfaceCaps->TrimValue;
#endif
        }

        MmcSwitchToHighSpeed(hSd, SpeedArgument);

        // If we are unable to set the clock back, return error
        NV_CHECK_ERROR_CLEANUP(NvDdkSdioSetClockFrequency(
            hSd->hSdDdk,
            ClockRateInKHz,
            &ConfiguredClockRate));
        SD_INFO_PRINT("ConfiguredClockRate: %d\n", ConfiguredClockRate);

        // set buswidth
        if (hSd->DataBusWidth == 1)
        {
            // do nothing
        }
        else if (hSd->DataBusWidth == 4)
        {
            NV_CHECK_ERROR_CLEANUP(MmcSetBusWidth(hSd, BusWidth4Bit));
        }
        else if (MmcSetBusWidth(hSd, BusWidth8Bit))
        {
            // If 8 bit bus width switch fails, try 4 bit bus width switch.
            NV_CHECK_ERROR_CLEANUP(MmcSetBusWidth(hSd, BusWidth4Bit));
            SD_ERROR_PRINT("8-bit buswidth switch failed. Using 4-bit buswidth\n");
        }

        // send Uhs mode
        if (hSd->Uhsmode == NvDdkSdioUhsMode_DDR50)
            NV_CHECK_ERROR_CLEANUP(NvDdkSdioSetUhsmode(hSd->hSdDdk, hSd->Uhsmode));
    }

#if NVDDK_SDMMC_T124
    NvDdkSdioConfigureTapAndTrimValues(hSd->hSdDdk, TapValue, TrimValue);
#endif

    SetBlockSize(hSd, SD_SECTOR_SIZE);
fail:
    if (e)
    {
        SD_ERROR_PRINT("MmcIdentifyCard :Error 0x%x\n", e);
    }
    return e;
}

static NvError
IdentifyCard(
    SdHandle hSd,
    NvU32 Instance)
{
    NvError e = NvSuccess;
    NvU32 HostCapacitySupport = 0;
    NvU32 CardCapacitySupport = 0;
    NvU32 SdStatus = 0;
    NvDdkSdioInterfaceCapabilities SdInterfaceCaps;
    NvDdkSdioCommand *pCmd = &hSd->pCmd;

    // Set the block length
    hSd->BlockLengthInUse = SD_SECTOR_SIZE;
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSetBlocksize(hSd->hSdDdk, SD_SECTOR_SIZE));

    // Get the sdio DDK Capabilities
    NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCapabilities(
        hSd->hSdDdk,
        &hSd->SdHostCaps,
        &SdInterfaceCaps,
        Instance));

    hSd->IsAutoIssueCMD12Supported = hSd->SdHostCaps.IsAutoCMD12Supported;
    hSd->DataBusWidth = SdInterfaceCaps.MmcInterfaceWidth;

    // Send CMD0 to set the card in idle state
    SD_SET_COMMAND_PARAMETERS(pCmd,
        SdCommand_GoIdleState,
        NvDdkSdioCommandType_Normal,
        NV_FALSE,
        0,
        NvDdkSdioRespType_NoResp,
        SD_SECTOR_SIZE);
    NV_CHECK_ERROR(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd, &SdStatus));
    if(SdStatus != NvDdkSdioError_None)
    {
        e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
        hSd->hSdDdk,
        pCmd->CommandCode,
        pCmd->ResponseType,
        hSd->Response));

    hSd->CurrentRegion = SdmmcAccessRegion_Unknown;

    e = MmcCardInit(hSd, HostCapacitySupport, &CardCapacitySupport);
    if ((e != NvSuccess) && (e != NvError_SdioCardNotPresent))
    {
        e = SET_ERROR(e, MmcCommand_MmcInit);
        goto fail;
    }

    if (hSd->IsMMC)
    {
        NV_CHECK_ERROR_CLEANUP(MmcIdentifyCard(hSd, Instance));
    }
    else
    {
        NV_CHECK_ERROR_CLEANUP(SdIdentifyCard(hSd, Instance));
    }

fail:
    if (e)
    {
        SD_ERROR_PRINT("IdentifyCard :Error 0x%x\n", e);
    }
    return e;
}

static NvError
SdUtilLockBlock(
    SdBlockDevHandle hSdBlockDev,
    NvU32 StartSector,
    NvU32 NumberOfSectors,
    NvBool EnableLocks)
{

    NvError e = NvSuccess;
    NvU32 i;
    SdHandle hSd = hSdBlockDev->hDev;
    NvDdkSdioCommand *pCmd = &hSd->pCmd;
    NvU32 SdStatus = 0;
    NvU32 StarteMMCSector;
    NvU32 LogToPhyFactor;
    NvU32 NumberOfeMMCSectors;
    NvU32 TotalBlocks;
    NvU32 TempBlocks;

    LogToPhyFactor = hSd->BlockDevInfo.BytesPerSector /
                                SD_SECTOR_SIZE;

    TotalBlocks = NumberOfSectors;

    while (TotalBlocks)
    {
        StarteMMCSector = StartSector;
        NumberOfeMMCSectors = TotalBlocks;
        NV_CHECK_ERROR_CLEANUP(MmcSelectRegion(
                hSd,&StarteMMCSector,&NumberOfeMMCSectors, NV_TRUE));

        StarteMMCSector *= LogToPhyFactor;

        TempBlocks = NumberOfeMMCSectors * LogToPhyFactor;

        // Returning unaligned error if write protect group size aligned
        // requests are not issued.
        if ((StarteMMCSector % hSd->WPGrpSize) ||
                (TempBlocks % hSd->WPGrpSize))
        {
            e = NvError_EmmcBlockDriverLockUnaligned;
            goto fail;
        }

        TempBlocks /= hSd->WPGrpSize;

        for ( i=0; i < TempBlocks; i++)
        {
            //Send the Write protect command
            pCmd->CommandCode = MmcCommand_SetWriteProt;
            pCmd->CmdArgument = StarteMMCSector;
            pCmd->ResponseType = NvDdkSdioRespType_R1b;
            NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(
                hSd->hSdDdk,
                pCmd,
                &SdStatus));
             if (SdStatus)
            {
                e = NvError_EmmcBlockDriverLockFailure;
                goto fail;
            }

            NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
                hSd->hSdDdk,
                MmcCommand_SetWriteProt,
                NvDdkSdioRespType_R1b,
                hSd->Response));

            StarteMMCSector += hSd->WPGrpSize;
        }

    TotalBlocks -= NumberOfeMMCSectors;
    StartSector += NumberOfeMMCSectors;
    }
fail:
    if (e)
    {
        SD_ERROR_PRINT("eMMC locking failed with error %d\n", e);
    }
    return e;
}

static void
SdPowerUp(NvDdkBlockDevHandle hBlockDev)
{
    SdBlockDevHandle hSdBlkDev = (SdBlockDevHandle)hBlockDev;

    NvOsMutexLock(hSdBlkDev->hDev->LockDev);

    if ((hSdBlkDev->hDev->PowerUpCounter <
        hSdBlkDev->hDev->RefCount) && (!hSdBlkDev->IsPowered))
    {
        // Ensure that PowerUpCounter is never above RefCount
        hSdBlkDev->hDev->PowerUpCounter++;
        // Power up only when the first block driver on device does power up
        if (hSdBlkDev->hDev->PowerUpCounter == 1)
        {
            // call Ddk Sd API to power up the controller
            (void)NvDdkSdioResume(hSdBlkDev->hDev->hSdDdk, NV_TRUE);
            hSdBlkDev->IsPowered = NV_TRUE;
        }
    }
    NvOsMutexUnlock(hSdBlkDev->hDev->LockDev);

}

static void
SdPowerDown(NvDdkBlockDevHandle hBlockDev)
{
    SdBlockDevHandle hSdBlkDev = (SdBlockDevHandle)hBlockDev;

    NvOsMutexLock(hSdBlkDev->hDev->LockDev);

    if (hSdBlkDev->IsPowered)
    {
        // Ensure that PowerUpCounter is never decremented below 0
        if (hSdBlkDev->hDev->PowerUpCounter > 0)
        {
            hSdBlkDev->hDev->PowerUpCounter--;
            // Power down only when the last block driver on device
            // does power down
            if (!hSdBlkDev->hDev->PowerUpCounter)
            {
                // call Ddk Sd API to power down the controller
                (void)NvDdkSdioSuspend(hSdBlkDev->hDev->hSdDdk, NV_FALSE);
                hSdBlkDev->IsPowered = NV_FALSE;
            }
        }
    }
    NvOsMutexUnlock(hSdBlkDev->hDev->LockDev);
}

static void
SdGetDeviceInfo(
    NvDdkBlockDevHandle hBlockDev,
    NvDdkBlockDevInfo* pBlockDevInfo)
{
    SdBlockDevHandle hSd;
    volatile NvU32 temp;
    NvU32 NumLog2;
    NvU32 NumOfBPblocks, TotalBlocks;

    if ((hBlockDev == NULL) || (pBlockDevInfo == NULL))
        return;

    hSd = (SdBlockDevHandle)hBlockDev;
    pBlockDevInfo->BytesPerSector = (SD_SECTOR_SIZE << 3);
    // The min partition size will be equal to erase group size if the
    // erase group size is less than 2 MB.
    // The max unit of  partition size allowed is 2MB.
    if ((hSd->hDev->EraseGrpSize * SD_SECTOR_SIZE) > MAX_BLOCK_SIZE)
        pBlockDevInfo->SectorsPerBlock = MAX_SECTORS_PER_BLOCK;
    else
        pBlockDevInfo->SectorsPerBlock =
            (hSd->hDev->EraseGrpSize * SD_SECTOR_SIZE ) /
                                        pBlockDevInfo->BytesPerSector;
    temp = pBlockDevInfo->BytesPerSector >> SD_SECTOR_SZ_LOG2;
    NumLog2 = SdUtilGetLog2(temp);
    NumOfBPblocks = ((hSd->hDev->BootPartitionSize) >> SD_SECTOR_SZ_LOG2) << 1;
    TotalBlocks = hSd->hDev->NumOfBlocks + NumOfBPblocks;
    pBlockDevInfo->TotalSectors = TotalBlocks >> 3;
    pBlockDevInfo->TotalBlocks = TotalBlocks >> NumLog2;
    NumLog2 = SdUtilGetLog2(pBlockDevInfo->SectorsPerBlock);
    pBlockDevInfo->TotalBlocks >>= NumLog2;
    pBlockDevInfo->DeviceType = NvDdkBlockDevDeviceType_Fixed;
    pBlockDevInfo->Private = hSd->hDev->CardIdReg;
}

static NvError
SdRead(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 BlockNum,
    void* const pBuffer,
    NvU32 NumberOfBlocks)
{
    NvError e = NvSuccess;
    NvError ResetError = NvSuccess;
    SdBlockDevHandle hSdBlkDev = (SdBlockDevHandle)hBlockDev;
    SdHandle hSd = hSdBlkDev->hDev;
    NvU32 size;
    NvDdkSdioCommand* pCmd = NULL;
    NvU32 SdioStatus = 0;
    NvU32 CurrentBlocks;
    NvU32 BlocksToTransfer = 0;
    NvU32 StartBlock = BlockNum;
    NvU8 *CurrentBuffer = (NvU8 *)pBuffer;
    NvDdkBlockDevInfo *pDeviceInfo;
    NvU32 ResidueBlks;
    NvU32 RegNumBlks;
    NvU32 BytesPerSectLog2;
    NvU32 ControllerResetRetry = 0;

    SD_DEBUG_PRINT("SdRead: Block Number = %d, Number of Blocks = %d\n", BlockNum, NumberOfBlocks);
    if ((hBlockDev == NULL) || (pBuffer == NULL) || (NumberOfBlocks == 0))
    {
        e = NvError_BadParameter;
        goto fail;
    }

    if (BlockNum > hSd->NumOfBlocks)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    pDeviceInfo = &(hSd->BlockDevInfo);
    pCmd = &hSd->pCmd;
    ResidueBlks = NumberOfBlocks;
    BytesPerSectLog2 = SdUtilGetLog2(pDeviceInfo->BytesPerSector);
    while (ResidueBlks)
    {
        RegNumBlks = ResidueBlks;
        StartBlock = BlockNum;

        // Handle read across regions
        NV_CHECK_ERROR_CLEANUP(MmcSelectRegion(hSd, &StartBlock, &RegNumBlks, NV_TRUE));

        CurrentBlocks = RegNumBlks;
        while (CurrentBlocks)
        {
            if(CurrentBlocks > MMC_SD_MAX_READ_WRITE_SECTORS)
                BlocksToTransfer = MMC_SD_MAX_READ_WRITE_SECTORS;
            else
                BlocksToTransfer = CurrentBlocks;
            size = BlocksToTransfer << BytesPerSectLog2;

            if (!IsCardInTransferState(hSd))
            {
                e = SET_ERROR(NvError_InvalidState, SdCommand_ReadSingle);
                goto fail;
            }
            SD_SET_COMMAND_PARAMETERS(pCmd,
                ((BlocksToTransfer > 0)? SdCommand_ReadMultiple :
                    SdCommand_ReadSingle),
                NvDdkSdioCommandType_Normal,
                NV_TRUE,
                0,
                NvDdkSdioRespType_R1,
                SD_SECTOR_SIZE);

            // Set read cmd argument.
            if (hSd->IsSdhc)
            {
                pCmd->CmdArgument = (StartBlock << hSd->SectorsPerPageLog2);
            }
            else
            {
                pCmd->CmdArgument = StartBlock << BytesPerSectLog2;
            }
            NV_CHECK_ERROR_CLEANUP(NvDdkSdioRead(
                hSd->hSdDdk,
                size,
                (void *)CurrentBuffer,
                pCmd,
                hSd->IsAutoIssueCMD12Supported,
                &SdioStatus));

            if (SdioStatus != NvDdkSdioError_None &&
                   !(SdioStatus &  NvDdkSdioError_DataTimeout))
            {
                //Data timeout error handled separately
                e = SET_ERROR(NvError_SdioCommandFailed , pCmd->CommandCode);
                goto fail;
            }

            if ((SdioStatus == NvDdkSdioError_None) &&
                !(hSd->IsAutoIssueCMD12Supported) &&
                (RegNumBlks > 1))
            {
                //Issue CMD12 for stop transmission
                SD_SET_COMMAND_PARAMETERS(pCmd,
                    SdCommand_StopTransmission,
                    NvDdkSdioCommandType_Abort,
                    NV_FALSE,
                    0,
                    NvDdkSdioRespType_R1,
                    SD_SECTOR_SIZE);
                NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(
                    hSd->hSdDdk,
                    pCmd,
                    &SdioStatus));
                if (SdioStatus != NvDdkSdioError_None)
                {
                    e = SET_ERROR(NvError_SdioCommandFailed , pCmd->CommandCode);
                    goto fail;
                }
            }
            // Check response
            NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
                hSd->hSdDdk,
                pCmd->CommandCode,
                pCmd->ResponseType,
                hSd->Response));

            if (SdioStatus & NvDdkSdioError_DataTimeout)
            {

                //Get status of card
                NV_CHECK_ERROR_CLEANUP(GetCardStatus(hSd));

                if (hSd->Response[0] & MMC_READ_WRITE_ERROR_MASK)
                {
                    //generic data read or write error detectected. Reseting controller.
                    if (ControllerResetRetry > 2)
                        goto fail;

                    ControllerResetRetry++;

                    NvOsMutexLock(hSd->LockDev);

                    //Issue soft reset to controller
                    PrivSdioReset(hSd->hSdDdk, SDMMC_SW_RESET_FOR_ALL);

                    //Card Identification should be done at frequency < 400 Khz
                    //and host date buswidth should be 1Bit
                    NvDdkSdioSetClockFrequency(hSd->hSdDdk,
                        MMC_IDENT_CLOCK_KHZ, NULL);

                    NvDdkSdioSetHostBusWidth(hSd->hSdDdk, NvDdkSdioDataWidth_1Bit);

                    //Reidentify the card
                    ResetError = IdentifyCard(hSd, hSd->Instance);

                    NvOsMutexUnlock(hSd->LockDev);

                    if (ResetError != NvSuccess)
                        goto fail;

                    break;
                }
                else
                {
                    e = SET_ERROR(NvError_SdioCommandFailed , pCmd->CommandCode);
                    goto fail;
                }
            }
            else
            {
                ControllerResetRetry = 0;
                CurrentBlocks -= BlocksToTransfer;
                StartBlock += BlocksToTransfer;
                CurrentBuffer += size;
            }
        }
        // blocks from region read from deducted to get remaining blocks to read
        // block region should not change in case of read/write failure
        if (!ControllerResetRetry)
        {
            ResidueBlks -= RegNumBlks;
            BlockNum += RegNumBlks;
        }
    }

fail:
    if (!IsCardInTransferState(hSd) && ((hSd->CardState == SdState_Rcv) |
           (hSd->CardState == SdState_Data)))

    {
        // Send CMD12 to read the card identification data
        SD_SET_COMMAND_PARAMETERS(pCmd,
            MmcCommand_StopTransmissionCommand,
            NvDdkSdioCommandType_Normal,
            NV_FALSE,
            0,
            NvDdkSdioRespType_R1,
            SD_SECTOR_SIZE);
        NV_CHECK_ERROR(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd, &SdioStatus));

        if(SdioStatus != NvDdkSdioError_None)
        {
            e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
        }

        NV_CHECK_ERROR(NvDdkSdioGetCommandResponse(hSd->hSdDdk,
            pCmd->CommandCode,
            pCmd->ResponseType,
            hSd->Response));
    }

    return e;
}

static NvError
SendCommandTwelve(SdHandle hSd)
{
    NvDdkSdioCommand *pCmd = &hSd->pCmd;
    NvU32 SdioStatus = 0;
    NvError e = NvSuccess;
    if (!IsCardInTransferState(hSd) && ((hSd->CardState == SdState_Rcv) |
        (hSd->CardState == SdState_Data)))
    {
        // Send CMD12 to read the card identification data
        SD_SET_COMMAND_PARAMETERS(pCmd,
            MmcCommand_StopTransmissionCommand,
            NvDdkSdioCommandType_Normal,
            NV_FALSE,
            0,
            NvDdkSdioRespType_R1b,
            SD_SECTOR_SIZE);
        NV_CHECK_ERROR(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd, &SdioStatus));
        if(SdioStatus != NvDdkSdioError_None)
        {
            e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
        }

        NV_CHECK_ERROR(NvDdkSdioGetCommandResponse(hSd->hSdDdk,
            pCmd->CommandCode,
            pCmd->ResponseType,
            hSd->Response));
    }
    return e;
}

static NvError
SdWrite(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 BlockNum,
    const void* pBuffer,
    NvU32 NumberOfBlocks)
{
    NvError e = NvSuccess;
    NvError ResetError = NvSuccess;
    NvU32 RetryCount = MAX_WRITE_RETRY_COUNT;
    SdBlockDevHandle hSdBlkDev = (SdBlockDevHandle)hBlockDev;
    SdHandle hSd = hSdBlkDev->hDev;
    NvU32 size = NumberOfBlocks * SD_SECTOR_SIZE;
    NvDdkSdioCommand* pCmd = NULL;
    NvU32 SdioStatus = 0;
    NvU32 CurrentBlocks;
    NvU32 BlocksToTransfer = 0;
    NvU32 StartBlock = BlockNum;
    NvU8 *CurrentBuffer = (NvU8 *)pBuffer;
    NvDdkBlockDevInfo *pDeviceInfo;
    NvU32 ResidueBlks;
    NvU32 RegNumBlks;
    NvU32 BytesPerSectLog2;
    NvU32 ControllerResetRetry = 0;

//    NvOsDebugPrintf("Write:1 %d\n", (NvU32)NvOsGetTimeUS());
    SD_DEBUG_PRINT("SdWrite: Block Number = %d, Number of Blocks = %d\n", BlockNum, NumberOfBlocks);

    if ((hBlockDev == NULL) || (pBuffer == NULL) || (NumberOfBlocks == 0))
    {
        e = NvError_BadParameter;
        goto fail;
    }

    if (BlockNum > hSd->NumOfBlocks)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    pDeviceInfo = &(hSd->BlockDevInfo);
    pCmd = &hSd->pCmd;

    ResidueBlks = NumberOfBlocks;
    BytesPerSectLog2 = SdUtilGetLog2(pDeviceInfo->BytesPerSector);
    while (ResidueBlks)
    {
        RegNumBlks = ResidueBlks;
        StartBlock = BlockNum;

        // Handle write across region boundaries
        NV_CHECK_ERROR_CLEANUP(MmcSelectRegion(hSd, &StartBlock, &RegNumBlks, NV_TRUE));

        CurrentBlocks = RegNumBlks;
        while (CurrentBlocks)
        {
            if(CurrentBlocks > MMC_SD_MAX_READ_WRITE_SECTORS)
                BlocksToTransfer = MMC_SD_MAX_READ_WRITE_SECTORS;
            else
                BlocksToTransfer = CurrentBlocks;

            size = BlocksToTransfer << BytesPerSectLog2;
            if (!IsCardInTransferState(hSd))
            {
                e = SET_ERROR(NvError_InvalidState, SdCommand_WriteMultiple);
                goto fail;
            }

            SD_SET_COMMAND_PARAMETERS(pCmd,
                ((BlocksToTransfer > 0) ? SdCommand_WriteMultiple :
                    SdCommand_WriteSingle),
                NvDdkSdioCommandType_Normal,
                NV_TRUE,
                0,
                NvDdkSdioRespType_R1,
                SD_SECTOR_SIZE);

            // Set read cmd argument.
            if (hSd->IsSdhc)
            {
                pCmd->CmdArgument = (StartBlock << hSd->SectorsPerPageLog2);
            }
            else
            {
                pCmd->CmdArgument = StartBlock << BytesPerSectLog2;
            }

            e = NvDdkSdioWrite(
                hSd->hSdDdk,
                size,
                (void *)CurrentBuffer,
                pCmd,
                hSd->IsAutoIssueCMD12Supported,
                &SdioStatus);

            //WAR for write timeout and data CRC issues observed. Refer bug 918157
            if(e != NvSuccess || SdioStatus == NvDdkSdioError_DataCRC)
            {
                while(RetryCount > 0)
                {
                    SendCommandTwelve(hSd);
                    SD_SET_COMMAND_PARAMETERS(pCmd,
                        ((BlocksToTransfer > 0) ? SdCommand_WriteMultiple :
                            SdCommand_WriteSingle),
                        NvDdkSdioCommandType_Normal,
                        NV_TRUE,
                        0,
                        NvDdkSdioRespType_R1,
                        SD_SECTOR_SIZE);
                    if (hSd->IsSdhc)
                    {
                        pCmd->CmdArgument = (StartBlock << hSd->SectorsPerPageLog2);
                    }
                    else
                    {
                        pCmd->CmdArgument = StartBlock << BytesPerSectLog2;
                    }
                    e = NvDdkSdioWrite(
                        hSd->hSdDdk,
                        size,
                        (void *)CurrentBuffer,
                        pCmd,
                        hSd->IsAutoIssueCMD12Supported,
                        &SdioStatus);
                    if(e == NvSuccess && SdioStatus != NvDdkSdioError_DataCRC)
                        break;
                    RetryCount--;
                }
            }

            if (SdioStatus != NvDdkSdioError_None && !(SdioStatus & NvDdkSdioError_DataTimeout))
            {
                //Data timeout error handled separately
                e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
                goto fail;
            }

            if ((SdioStatus == NvDdkSdioError_None) &&
                !(hSd->IsAutoIssueCMD12Supported) &&
                (RegNumBlks > 1))
            {
                //Issue CMD12 for stop transmission
                SD_SET_COMMAND_PARAMETERS(pCmd,
                    SdCommand_StopTransmission,
                    NvDdkSdioCommandType_Abort,
                    NV_FALSE,
                    0,
                    NvDdkSdioRespType_R1b,
                    SD_SECTOR_SIZE);
                NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(
                    hSd->hSdDdk,
                    pCmd,
                    &SdioStatus));
                if (SdioStatus != NvDdkSdioError_None)
                {
                    e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
                    goto fail;
                }
            }

            // Check response
            NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(
                hSd->hSdDdk,
                pCmd->CommandCode,
                pCmd->ResponseType,
                hSd->Response));

            // Data timeout error
            if (SdioStatus & NvDdkSdioError_DataTimeout)
            {

                //Get card status
                NV_CHECK_ERROR_CLEANUP(GetCardStatus(hSd));

                //generic data read or write error detectected. Reseting controller.
                if (hSd->Response[0] & MMC_READ_WRITE_ERROR_MASK)
                {
                    if (ControllerResetRetry > 2)
                        goto fail;

                    ControllerResetRetry++;

                    NvOsMutexLock(hSd->LockDev);

                    //Issue soft reset to controller
                    PrivSdioReset(hSd->hSdDdk, SDMMC_SW_RESET_FOR_ALL);

                    //Card Identification should be done at frequency < 400 Khz
                    //and host date buswidth should be 1Bit
                    NvDdkSdioSetClockFrequency(hSd->hSdDdk,
                            MMC_IDENT_CLOCK_KHZ, NULL);
                    NvDdkSdioSetHostBusWidth(hSd->hSdDdk, NvDdkSdioDataWidth_1Bit);

                    //Reidentify the card
                    ResetError = IdentifyCard(hSd, hSd->Instance);

                    NvOsMutexUnlock(hSd->LockDev);

                    if (ResetError != NvSuccess)
                        goto fail;

                    break;
                }
                else
                {
                    e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
                    goto fail;
                }
            }
            else
            {
                ControllerResetRetry = 0;
                CurrentBlocks -= BlocksToTransfer;
                StartBlock += BlocksToTransfer;
                CurrentBuffer += size;
            }
        }
        // blocks from region written into deducted to get remaining blocks to write
        // block region should not change in case of read/write failure
        if (!ControllerResetRetry)
        {
            ResidueBlks -= RegNumBlks;
            BlockNum += RegNumBlks;
        }
    } // end of while

fail:
    if (!IsCardInTransferState(hSd) && ((hSd->CardState == SdState_Rcv) |
        (hSd->CardState == SdState_Data)))
    {
        // Send CMD12 to read the card identification data
        SD_SET_COMMAND_PARAMETERS(pCmd,
            MmcCommand_StopTransmissionCommand,
            NvDdkSdioCommandType_Normal,
            NV_FALSE,
            0,
            NvDdkSdioRespType_R1b,
            SD_SECTOR_SIZE);
        NV_CHECK_ERROR(NvDdkSdioSendCommand(hSd->hSdDdk, pCmd, &SdioStatus));
        if(SdioStatus != NvDdkSdioError_None)
        {
            e = SET_ERROR(NvError_SdioCommandFailed, pCmd->CommandCode);
        }

        NV_CHECK_ERROR(NvDdkSdioGetCommandResponse(hSd->hSdDdk,
            pCmd->CommandCode,
            pCmd->ResponseType,
            hSd->Response));
    }

//    NvOsDebugPrintf("Write:2 %d", (NvU32)NvOsGetTimeUS());
    return e;
}

static void
SdRegisterHotplugSemaphore(
    NvDdkBlockDevHandle hBlockDev,
    NvOsSemaphoreHandle hHotPlugSema)
{
    // Hot plugging is not supported yet.
}


// Local function to close a Sd block device driver handle
static void SdClose(NvDdkBlockDevHandle hBlockDev)
{
    SdBlockDevHandle hSdBlkDev = (SdBlockDevHandle)hBlockDev;
    NvU32 BootDevice;
    NvU32 Instance;
    NvU32 Size;

    if (hSdBlkDev->IsPowered)
    {
        hSdBlkDev->IsPowered = NV_FALSE;
    }

    if (hSdBlkDev->hDev->RefCount > 1)
    {
        // decrement reference count for Sd major instance
        NvOsMutexLock(hSdBlkDev->hDev->LockDev);
        hSdBlkDev->hDev->RefCount--;
        hSdBlkDev->hDev->PowerUpCounter--;
        NvOsMutexUnlock(hSdBlkDev->hDev->LockDev);
    }
    else
    {
        Size = sizeof(NvU32);
        NvDdkFuseGet(NvDdkFuseDataType_SecBootDeviceSelect,
                (void *)&BootDevice, (NvU32 *)&Size);
        NvDdkFuseGet(NvDdkFuseDataType_SecBootDevInst,
                (void *)&Instance, (NvU32 *)&Size);
        // If the boot device is emmc and the instance is corresponding to
        //emmc device, do not close. It will be closed in Deinit function
        if (!((BootDevice == NvBootDevType_Sdmmc) &&
            (hSdBlkDev->hDev->Instance == Instance)))
        {
            // Close sdio Ddk Handle
            NvDdkSdioClose(hSdBlkDev->hDev->hSdDdk);
            (*(s_SdCommonInfo.SdDevStateList +
                hSdBlkDev->hDev->Instance)) = NULL;
            NvOsFree(hSdBlkDev->hDev);
        }
        NvOsFree(hSdBlkDev);
        hBlockDev = NULL;
   }
}

// Local function to open a Sd block device driver handle
static NvError
SdOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    SdBlockDevHandle* hSdBlkDev)
{
    NvError e = NvSuccess;
    SdBlockDevHandle hTempSdBlockDev;
    SdHandle hTempDev = NULL;

    NV_ASSERT(hSdBlkDev);

    SD_DEBUG_PRINT("SdOpen: Instance = %d\n", Instance);

    NvOsMutexLock((*(s_SdCommonInfo.SdDevLockList + Instance)));
    hTempSdBlockDev = NvOsAlloc(sizeof(SdBlockDev));
    if (!hTempSdBlockDev)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    NvOsMemset(hTempSdBlockDev, 0, sizeof(SdBlockDev));

    // Controller Initialization for already opened instance is not needed
    if ((*(s_SdCommonInfo.SdDevStateList + Instance)) == NULL)
    {
        // Allocate once for device
         if ((hTempDev =
            (SdHandle)NvOsAlloc(sizeof(struct SdRec))) == NULL)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }
        // Ensures that SdDevHandle->hHeadRegionList
        // gets initialized to NULL
        NvOsMemset(hTempDev, 0, sizeof(struct SdRec));
        // Use the lock create earlier
        hTempDev->LockDev = *(s_SdCommonInfo.SdDevLockList + Instance);

        // init sdio Ddk handle
        NV_CHECK_ERROR_CLEANUP(NvDdkSdioOpen(
            s_SdCommonInfo.hRm,
            &hTempDev->hSdDdk,
            Instance));

        NV_CHECK_ERROR_CLEANUP(IdentifyCard(hTempDev, Instance));

        // Save the device major instance
        hTempDev->Instance = Instance;
        // There are 2 cases:
        //      a) During System initialization Open is called
        //         with MinorInstance==0
        //      b) During Normal Boot Open is called with non-zero
        //         MinorInstance.
        // We need to load the region table from device in case (b)

        // set the device instance state entry
        (*(s_SdCommonInfo.SdDevStateList + Instance)) =
            hTempDev;
    }

    hTempSdBlockDev->hDev =
         (*(s_SdCommonInfo.SdDevStateList + Instance));

     // Finished initializing device dependent data
     // save device info, after initialization of device specific data
     SdGetDeviceInfo(
        &hTempSdBlockDev->BlockDev, &hTempSdBlockDev->hDev->BlockDevInfo);
    // Once device info is obtained initialize log2 Sectors per Page
    hTempSdBlockDev->hDev->SectorsPerPageLog2 =
        SdUtilGetLog2(hTempSdBlockDev->hDev->BlockDevInfo.BytesPerSector
        >> SD_SECTOR_SZ_LOG2);

    // Initialize the partition id
    hTempSdBlockDev->MinorInstance = MinorInstance;
    hTempSdBlockDev->IsPowered = NV_TRUE;

    // increment Dev ref count
    hTempSdBlockDev->hDev->RefCount++;
    hTempSdBlockDev->hDev->PowerUpCounter++;
    *hSdBlkDev = hTempSdBlockDev;

    NvOsMutexUnlock((*(s_SdCommonInfo.SdDevLockList + Instance)));

    return e;
fail:
    (*(s_SdCommonInfo.SdDevStateList + Instance)) = NULL;
    // Release resoures here.
    if (hTempDev)
    {
        // Close sdio Ddk Handle else get Sd already open
        NvDdkSdioClose(hTempDev->hSdDdk);
        NvOsFree(hTempDev);
    }
    NvOsFree(hTempSdBlockDev);
    *hSdBlkDev = NULL;
    NvOsMutexUnlock((*(s_SdCommonInfo.SdDevLockList + Instance)));
    return e;
}

// It is called when erase of partial erasable group size is needed
// This function input argument start and number of sectors such that
// has either of following is true -
// 1. StartSector is aligned to erasable group size
// 2. NumOfSectors+StartSector is aligned to erasable group size
static NvError
UtilClearSkipBlocks(
    SdBlockDevHandle hSdBlockDev,
    NvU32 StartSector,
    NvU32 NumOfSectors)
{
    NvError e = NvSuccess;
    // FIXME: Write to EMMC blocks to initialize blocks fails at block 0x65C800
#if WAR_SD_FORMAT_PART
    NvU8 *Buf;
    NvU32 i;
    NvDdkBlockDevInfo *pDeviceInfo = &(hSdBlockDev->hDev->BlockDevInfo);
    NvU32 TmpStartSector;

    Buf = NvOsAlloc(sizeof(NvU8) * pDeviceInfo->BytesPerSector);
    if (!Buf)
        return NvError_InsufficientMemory;

    TmpStartSector = StartSector/(pDeviceInfo->BytesPerSector/SD_SECTOR_SIZE);
    NumOfSectors /= (pDeviceInfo->BytesPerSector/SD_SECTOR_SIZE);

    if (hSdBlockDev->hDev->ErasedMemContent)
        NvOsMemset(Buf, 0xFF, pDeviceInfo->BytesPerSector);
    else
        NvOsMemset(Buf, 0x0, pDeviceInfo->BytesPerSector);
    for (i = 0; i < NumOfSectors; i++)
    {
        e = SdWrite((NvDdkBlockDevHandle)hSdBlockDev, (TmpStartSector + i),
            (const void *)Buf, 1);
        if (e != NvSuccess)
        {
            SD_ERROR_PRINT("\r\nError code=0x%x SD clear skip blocks - "
                "sector=%d ", e, (StartSector + i));
            NV_ASSERT(NV_FALSE);
            break;
        }
    }
    NvOsFree(Buf);
#endif
    return e;
}

static NvError
SdUtilEraseLogicalSectors(
    SdBlockDevHandle hSdBlockDev,
    NvU32 StartSector,
    NvU32 NumOfSectors)
{
    NvError e;
    NvU32 TmpNumSectors;
    NvU32 TmpStartSector;
    NvU32 UnusedSectors = 0;
    NvU32 SkipSectors = 0;
    NvU32 RelStart;
    NvU32 SuffixSectors = 0;
    NvU32 SkipSuffixStart = 0;
    SdmmcAccessRegion Region = SdmmcAccessRegion_Unknown;
    NvU32 i;

    TmpNumSectors = NumOfSectors;
    TmpStartSector = StartSector;
    for (i = 0; i < SdmmcAccessRegion_Num; i++)
    {
        RelStart = TmpStartSector;
        e = UtilGetMmcFirstRegionBounds(hSdBlockDev->hDev,
            &TmpNumSectors, &RelStart, &UnusedSectors, &Region);
        if (e != NvSuccess)
            return e;

        // Erase from the next block boundary. This check needs to be present until the
        // Sd block size issue is fixed.

        NumOfSectors = TmpNumSectors;
        if ((TmpNumSectors % hSdBlockDev->hDev->EraseGrpSize) != 0)
        {
            // Erase for partition sectors residing in boundary Erase groups
            NvU32 OldStartSector = RelStart;
            NvU32 TempVal;
            RelStart = (OldStartSector + hSdBlockDev->hDev->EraseGrpSize - 1);
            // EraseGrpSize aligned address beyond start address
            TempVal = (RelStart % hSdBlockDev->hDev->EraseGrpSize);
            // RelStart is aligned to EraseGrpSize
            RelStart -= TempVal;
            // We skip SkipSectors
            SkipSectors = ((RelStart - OldStartSector) < NumOfSectors)?
                (RelStart - OldStartSector) : NumOfSectors;
            if (SkipSectors)
            {
                // Send unaligned start OldStartSector
                // Erase unaligned blocks before start of partition
                // if not part of PT
                e = UtilClearSkipBlocks(hSdBlockDev, OldStartSector,
                    SkipSectors);
                if (e != NvSuccess)
                    return e;
            }
            // We cannot erase beyond requested block
            // FIXME: We cannot assume sector number is 2K multiple
            NumOfSectors -= SkipSectors;
            SuffixSectors = (NumOfSectors % hSdBlockDev->hDev->EraseGrpSize);
            if (SuffixSectors != 0)
            {
                // Send aligned start derived from aligned StartSector
                SkipSuffixStart = (RelStart + NumOfSectors - SuffixSectors);
                // We have some suffix blocks less than erase group size
                NumOfSectors -= SuffixSectors;
                SkipSectors += SuffixSectors;
            }
        }

        // Erase groups residing within partition
        SdUtilEraseGroupSize(hSdBlockDev, RelStart, NumOfSectors, Region);

        if (SuffixSectors)
        {
            // Erase unaligned blocks at start of next partition for PT
            e = UtilClearSkipBlocks(hSdBlockDev, SkipSuffixStart, SuffixSectors);
            if (e != NvSuccess)
                return e;
        }

        // Finished processing a region try next region
        TmpStartSector += (NumOfSectors + SkipSectors);
        TmpNumSectors = UnusedSectors;
        UnusedSectors = 0;
        if (TmpNumSectors == 0)
        {
            // Finished processing all regions inside sector range
            break;
        }
    }
    if (TmpNumSectors != 0)
    {
        SD_ERROR_PRINT("\nError: failed to erase %d sectors ",
            TmpNumSectors);
        return NvError_EmmcBlockDriverEraseFailure;
    }

    if (hSdBlockDev->hDev->DoSanitize == NV_TRUE)
        MmcDoSanitize(hSdBlockDev->hDev);

    return NvSuccess;
}

static NvError
SdUtilTrimEraseLogicalSectors(
    SdBlockDevHandle hSdBlockDev,
    NvU32 StartSector,
    NvU32 NumOfSectors)
{
    NvError e;
    NvU32 i;
    NvU32 RemainingSectors = 0;
    NvU32 RelStart = StartSector;
    NvU32 ErStartSector = StartSector;
    NvU32 SectorsToErase = NumOfSectors;
    SdmmcAccessRegion Region = SdmmcAccessRegion_Unknown;

    for (i = 0; i < SdmmcAccessRegion_Num; i++)
    {
        e = UtilGetMmcFirstRegionBounds(hSdBlockDev->hDev,
            &SectorsToErase, &RelStart, &RemainingSectors, &Region);
        if (e != NvSuccess)
        {
            return e;
        }

        /* Erase write blocks residing within partition */
        SdUtilTrimErase(hSdBlockDev, RelStart, SectorsToErase, Region);

        ErStartSector += SectorsToErase;
        RelStart = ErStartSector;
        SectorsToErase = RemainingSectors;
        RemainingSectors = 0;
        if (SectorsToErase == 0)
        {
            /* Finished processing all regions inside sector range */
            break;
        }
    }
    if (SectorsToErase != 0)
    {
        SD_ERROR_PRINT("\nError: failed to erase %d sectors ",
            SectorsToErase);
        return NvError_EmmcBlockDriverEraseFailure;
    }

    if (hSdBlockDev->hDev->DoSanitize == NV_TRUE)
        MmcDoSanitize(hSdBlockDev->hDev);

    return NvSuccess;
}

// Function to format Sd
static NvError
SdFormatDevice(SdBlockDevHandle hSdBlkDev)
{
    NvError e = NvSuccess;
    NvU32 NumOfBPblocks = ((hSdBlkDev->hDev->BootPartitionSize) >> SD_SECTOR_SZ_LOG2) << 1;
    NvU32 TotalBlocks = hSdBlkDev->hDev->NumOfBlocks + NumOfBPblocks;

#if ENABLE_SANITIZE_OPERATION
    if (hSdBlkDev->hDev->IsSanitizeSupported)
        hSdBlkDev->hDev->DoSanitize = NV_TRUE;
    else if (hSdBlkDev->hDev->IsSecureEraseSupported)
        hSdBlkDev->hDev->DoSecureErase = NV_TRUE;
#endif
    // Use Erase command for entire device
    NV_CHECK_ERROR(SdUtilEraseLogicalSectors(
        hSdBlkDev,
        0,
        TotalBlocks));

    return e;
}

// Read and Write Physical sector function
static NvError
SdUtilRdWrSector(SdBlockDevHandle hSdBlockDev,
    NvU32 Opcode, NvU32 InputSize, const void * InputArgs)
{
    NvError e = NvSuccess;
    NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs *pRdPhysicalIn =
        (NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs *)InputArgs;
    NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs *pWrPhysicalIn =
        (NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs *)InputArgs;

    NV_ASSERT(InputArgs);
    // Data is read from/written on chip block size at max each time
    if (Opcode == NvDdkBlockDevIoctlType_ReadPhysicalSector)
    {
        NV_ASSERT(InputSize == sizeof(NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs));
        // Read call
        e = SdRead(
            &hSdBlockDev->BlockDev,
            pRdPhysicalIn->SectorNum,
            pRdPhysicalIn->pBuffer,
            pRdPhysicalIn->NumberOfSectors);
    }
    else if (Opcode == NvDdkBlockDevIoctlType_WritePhysicalSector)
    {
        NV_ASSERT(InputSize == sizeof(NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs));
        // Write call
        e = SdWrite(
            &hSdBlockDev->BlockDev,
            pWrPhysicalIn->SectorNum,
            pWrPhysicalIn->pBuffer,
            pWrPhysicalIn->NumberOfSectors);
    }
    return e;
}

// Function to validate new partition arguments
static NvBool
SdUtilIsPartitionArgsValid(
    SdBlockDevHandle hSdBlockDev,
    NvU32 StartLogicalSector, NvU32 EndLogicalSector)
{
    SdPartTabHandle hCurrRegion;

    hCurrRegion = hSdBlockDev->hDev->hHeadRegionList;
    while (hCurrRegion)
    {
        // Check that there is no overlap in existing partition
        // with new partition
        if (((EndLogicalSector <= hCurrRegion->StartLSA)
            /* logical address of new partition finishes before start of
            compared partition */ ||
            (StartLogicalSector >=
            (hCurrRegion->StartLSA + (hCurrRegion->NumOfSectors)))
            /* logical address new partition starts next to or
            after compared partition */))
        {
            hCurrRegion = hCurrRegion->Next;
            continue;
        }
        else
            return NV_FALSE;
    }

    return NV_TRUE;
}

// Simple function to get LCM of 2 unsigned numbers
static NvU32 SdUtilGetLCM(NvU32 Num1, NvU32 Num2)
{
    NvU32 Max;
    NvU32 Min;
    NvU32 Prod = 1;
    NvU32 NumLCM;
    NvU32 i;
    if (Num1 == Num2) return Num1;
    Max = (Num1 > Num2) ? Num1 : Num2;
    Min = (Num1 == Max) ? Num2 : Num1;
    Prod = Num1 * Num2;
    NumLCM = Prod;
    for (i = Max; i < Prod; i += Max)
    {
        if (!(i % Min))
        {
            NumLCM = i;
            break;
        }
    }
    NvOsDebugPrintf("\r\nLCM of %d and %d =%d ", Num1, Num2, NumLCM);
    return NumLCM;
}

// Local function used to allocate a partition
// Percentage of replacement sectors sent as part of input argument
// is ignored while creating Sd partition - as there are no bad blocks
// in Sd. Hence, logical address equals physical address and size
// for Sd.
static NvError
SdUtilAllocPart(
    SdBlockDevHandle hSdBlockDev,
    NvDdkBlockDevIoctl_AllocatePartitionInputArgs *pAllocPartIn,
    NvDdkBlockDevIoctl_AllocatePartitionOutputArgs *pAllocPartOut)
{
    SdPartTabHandle hReg;
    SdPartTabHandle pEntry;
    NvU32 StartLogicalSector;
    NvError e = NvSuccess;
    NvU32 RemainingSectorCount;
    NvDdkBlockDevInfo *pDeviceInfo;
    NvU32 NumOfBlocks;
    NvU32 SizeUnit;
    NvBool FlagEraseAndBlockAlign = NV_TRUE;
    NvU32 SdNumSectors;
    NvU32 SectorsPerBlkLog2 = 0;
    NvBool FlagUseLog2 = NV_FALSE;
    NvU32 PartSectors;
    NvU32 NoOfSectors;
    NvU32 NumOfBPblocks;

    // Get device info for block dev handle
    pDeviceInfo = &(hSdBlockDev->hDev->BlockDevInfo);
    // Minimum allocate unit should be Erase group size for EMMC
    // but sometimes it is wasteful so we have a compromise

    // Since erase group size is sometimes no aligned to 4 sectors
    // We find the smallest multiple of EGS that is also multiple
    // of the SD block driver Block size(2K * 64)
    if (hSdBlockDev->hDev->PartUnitSize)
        SizeUnit = hSdBlockDev->hDev->PartUnitSize;
    else
    {
        SizeUnit = SdUtilGetLCM(hSdBlockDev->hDev->EraseGrpSize,
            (pDeviceInfo->SectorsPerBlock <<
            hSdBlockDev->hDev->SectorsPerPageLog2));
        hSdBlockDev->hDev->PartUnitSize = SizeUnit;
        // Enforce  SizeUnit alignment if it is not greater than Erase group
    }
    // disable aligning to both erase grp size and blk size if
    // we think that we would waste much of the space
    if (SizeUnit > hSdBlockDev->hDev->EraseGrpSize)
        FlagEraseAndBlockAlign = NV_FALSE;

    // Check partition allocation in user partition of emmc is started
    if (s_IsUserPartitionAllocStarted != NV_TRUE)
    {
        // get the number of logical sectors in boot partitions
        NumOfBPblocks = ((hSdBlockDev->hDev->BootPartitionSize) >>
                             SD_SECTOR_SZ_LOG2) << 1;
        NumOfBPblocks >>= hSdBlockDev->hDev->SectorsPerPageLog2;
        // If the partition is GP1, make sure that it will be allocated in
        // user partition of the emmc
        if ((pAllocPartIn->PartitionType == NvPartMgrPartitionType_GP1) &&
            (hSdBlockDev->hDev->LogicalAddressStart < NumOfBPblocks))
        {
            hSdBlockDev->hDev->LogicalAddressStart = NumOfBPblocks;
            s_IsUserPartitionAllocStarted = NV_TRUE;
        }
    }

    // Set logical start block before bad block check
    StartLogicalSector = hSdBlockDev->hDev->LogicalAddressStart;

    // Get start physical block number
    // Partition starts from superblock aligned addresses hence chip 0
    if (pAllocPartIn->AllocationType ==
        NvDdkBlockDevPartitionAllocationType_Absolute)
    {
        StartLogicalSector = pAllocPartIn->StartPhysicalSectorAddress;
        // change the next address as absolute address seen
        hSdBlockDev->hDev->LogicalAddressStart =
            pAllocPartIn->StartPhysicalSectorAddress;

        // Check if new partition start is located within existing partitions
        if (SdUtilIsPartitionArgsValid(hSdBlockDev,
            StartLogicalSector, StartLogicalSector) == NV_FALSE)
        {
            // Partition start is within existing partitions
            return NvError_BlockDriverOverlappedPartition;
        }
    }

    if (UtilCheckPowerOf2(hSdBlockDev->hDev->BlockDevInfo.SectorsPerBlock))
    {
        SectorsPerBlkLog2 =
            SdUtilGetLog2(hSdBlockDev->hDev->BlockDevInfo.SectorsPerBlock);
        FlagUseLog2 = NV_TRUE;
    }
    //check for partition attribute 0x800 to substract the give size from available size
    if(pAllocPartIn->PartitionAttribute & NVDDK_BLOCKDEV_PART_MGMT_POLICY_REMAIN_SIZE)
    {
        // This means allocate till end of device
        // Find number of blocks available
        if (FlagUseLog2)
        {
            RemainingSectorCount =
                (hSdBlockDev->hDev->BlockDevInfo.TotalBlocks << SectorsPerBlkLog2) -
                hSdBlockDev->hDev->LogicalAddressStart;
        }
        else
        {
            RemainingSectorCount =
                (hSdBlockDev->hDev->BlockDevInfo.TotalBlocks *
                hSdBlockDev->hDev->BlockDevInfo.SectorsPerBlock) -
                hSdBlockDev->hDev->LogicalAddressStart;
        }
        NoOfSectors = SizeUnit >> hSdBlockDev->hDev->SectorsPerPageLog2;
        pAllocPartIn->NumLogicalSectors = RemainingSectorCount -
                    ((pAllocPartIn->NumLogicalSectors + NoOfSectors-1)/NoOfSectors) * (NoOfSectors);
    }
    // Check for NumLogicalSectors == -1 and its handling
    if (pAllocPartIn->NumLogicalSectors == 0xFFFFFFFF)
    {
        // This means allocate till end of device
        // Find number of blocks available
        if (FlagUseLog2)
        {
            RemainingSectorCount =
                (hSdBlockDev->hDev->BlockDevInfo.TotalBlocks << SectorsPerBlkLog2) -
                hSdBlockDev->hDev->LogicalAddressStart;
        }
        else
        {
            RemainingSectorCount =
                (hSdBlockDev->hDev->BlockDevInfo.TotalBlocks *
                hSdBlockDev->hDev->BlockDevInfo.SectorsPerBlock) -
                hSdBlockDev->hDev->LogicalAddressStart;
        }
        pAllocPartIn->NumLogicalSectors = RemainingSectorCount;
    }
    SdNumSectors = pAllocPartIn->NumLogicalSectors <<
        hSdBlockDev->hDev->SectorsPerPageLog2;
    PartSectors = (SdNumSectors % SizeUnit);
    // Partition size to be a multiple of erase group size and Block size.
    if ((FlagEraseAndBlockAlign) && (SdNumSectors % SizeUnit))
    {
        NvOsDebugPrintf("\r\nPart-id=%d size from %d sectors by %d sectors ",
            pAllocPartIn->PartitionId, SdNumSectors,
            (SizeUnit - PartSectors));
        SdNumSectors += (SizeUnit - PartSectors);
        pAllocPartIn->NumLogicalSectors = SdNumSectors >>
            hSdBlockDev->hDev->SectorsPerPageLog2;
    }
    // Else, only block size alignment is ensured

    // Get number of blocks to transfer, must be after
    // logic to take care of user partition size of -1
    if (FlagUseLog2)
    {
        NumOfBlocks = (pAllocPartIn->NumLogicalSectors >> SectorsPerBlkLog2);
        // Ceil the size to next block
        NumOfBlocks += (((NumOfBlocks << SectorsPerBlkLog2)
            < pAllocPartIn->NumLogicalSectors)? 1 : 0);
    }
    else
    {
        NumOfBlocks = (pAllocPartIn->NumLogicalSectors / pDeviceInfo->SectorsPerBlock);
        // Ceil the size to next block
        NumOfBlocks += (((NumOfBlocks * pDeviceInfo->SectorsPerBlock)
            < pAllocPartIn->NumLogicalSectors)? 1 : 0);
    }
    // validate the new partition args against existing partitions
    if (SdUtilIsPartitionArgsValid(hSdBlockDev,
        StartLogicalSector,
        (StartLogicalSector + pAllocPartIn->NumLogicalSectors)) == NV_FALSE)
    {
        e = NvError_InvalidAddress;
        goto LblEnd;
    }
    // Update the logical start address for next partition
    if (FlagUseLog2)
    {
        hSdBlockDev->hDev->LogicalAddressStart +=
            (NumOfBlocks << SectorsPerBlkLog2);
    }
    else
    {
        hSdBlockDev->hDev->LogicalAddressStart +=
            (NumOfBlocks * pDeviceInfo->SectorsPerBlock);
    }

    // allocate a new region table entry and initialize
    // with the partition details
    pEntry = NvOsAlloc(sizeof(struct SdPartTabRec));
    if (!pEntry)
        return NvError_InsufficientMemory;
    pEntry->Next = NULL;
    pEntry->MinorInstance = pAllocPartIn->PartitionId;
    pEntry->StartLSA = StartLogicalSector;
    if (FlagUseLog2)
    {
        pEntry->NumOfSectors = (NumOfBlocks << SectorsPerBlkLog2);
    }
    else
    {
        pEntry->NumOfSectors = (NumOfBlocks * pDeviceInfo->SectorsPerBlock);
    }

    // Return start logical sector address to upper layers
    pAllocPartOut->StartLogicalSectorAddress = StartLogicalSector;
    pAllocPartOut->NumLogicalSectors = pEntry->NumOfSectors;
    // Getting the Physical Start and Num Sector
    {
        NvU32 PhysicalSector = StartLogicalSector;
        NvU32 NumberOfBlocks = 1;
        e = MmcSelectRegion(hSdBlockDev->hDev, &PhysicalSector, &NumberOfBlocks, NV_FALSE);
        if (e == NvSuccess)
            pAllocPartOut->StartPhysicalSectorAddress = PhysicalSector;
        else
            goto LblEnd;
        // Don't assume that NumSector will be same for logical and physical mapping.
        // It can be different if logical sector start in other partition eg:BootPartition
        PhysicalSector = StartLogicalSector + pEntry->NumOfSectors;
        e = MmcSelectRegion(hSdBlockDev->hDev, &PhysicalSector, &NumberOfBlocks, NV_FALSE);
        if (e == NvSuccess)
            pAllocPartOut->NumPhysicalSectors =
                PhysicalSector - pAllocPartOut->StartPhysicalSectorAddress;
        else
            goto LblEnd;
    }
    NvOsDebugPrintf("\r\nSD Alloc Partid=%d, start sector=%d,num=%d ",
        pAllocPartIn->PartitionId, StartLogicalSector, pEntry->NumOfSectors);

    hReg = hSdBlockDev->hDev->hHeadRegionList;

    // goto region table link-list end
    if (hReg)
    {
        while (hReg->Next)
        {
            hReg = hReg->Next;
        }
    }
    // Add new region-table-entry to end-of-list
    if (hReg)
    {
        hReg->Next = pEntry;
    }
    else
    {
        hSdBlockDev->hDev->hHeadRegionList = pEntry;
    }
LblEnd:
    return e;
}

// Function used to free the region table created during
// partition creation. Region table is not to be used for normal boot.
static void
SdFreePartTbl(SdBlockDevHandle hSdBlockDev)
{
    // Free each element in partition table
    SdPartTabHandle hPartEntry;
    SdPartTabHandle hPartEntryNext;
    hPartEntry = hSdBlockDev->hDev->hHeadRegionList;
    if (hPartEntry)
    {
        while (hPartEntry->Next)
        {
            hPartEntryNext = hPartEntry->Next;
            NvOsFree(hPartEntry);
            // move to next entry
            hPartEntry = hPartEntryNext;
        }
        // Free the last entry
        NvOsFree(hPartEntry);
        hSdBlockDev->hDev->hHeadRegionList = NULL;
    }
}

// Utility to return name of Ioctl given ioctl opcode
static void
UtilGetIoctlName(NvU32 Opcode, NvU8 *IoctlStr)
{
    switch(Opcode)
    {
        case NvDdkBlockDevIoctlType_ReadPhysicalSector:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_ReadPhysicalSector);
            break;
        case NvDdkBlockDevIoctlType_WritePhysicalSector:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_WritePhysicalSector);
            break;
        case NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector);
            break;
        case NvDdkBlockDevIoctlType_GetPartitionPhysicalSectors:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_GetPartitionPhysicalSectors);
            break;
        case NvDdkBlockDevIoctlType_FormatDevice:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_FormatDevice);
            break;
        case NvDdkBlockDevIoctlType_PartitionOperation:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_PartitionOperation);
            break;
        case NvDdkBlockDevIoctlType_AllocatePartition:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_AllocatePartition);
            break;
        case NvDdkBlockDevIoctlType_EraseLogicalSectors:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_EraseLogicalSectors);
            break;
        case NvDdkBlockDevIoctlType_ErasePartition:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_ErasePartition);
            break;
        case NvDdkBlockDevIoctlType_WriteVerifyModeSelect:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_WriteVerifyModeSelect);
            break;
        case NvDdkBlockDevIoctlType_LockRegion:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_LockRegion);
            break;
        case NvDdkBlockDevIoctlType_ErasePhysicalBlock:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_ErasePhysicalBlock);
            break;
        case NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus);
            break;
        case NvDdkBlockDevIoctlType_VerifyCriticalPartitions:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_VerifyCriticalPartitions);
            break;
        case NvDdkBlockDevIoctlType_UnprotectAllSectors:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_UnprotectAllSectors);
            break;
        case NvDdkBlockDevIoctlType_ProtectSectors:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_ProtectSectors);
            break;
        default:
            // Illegal Ioctl string
            MACRO_GET_STR(IoctlStr, UnknownIoctl);
            break;
    }
}

// Local Sd function to perform the requested ioctl operation
static  NvError
SdBlockDevIoctl(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 Opcode,
    NvU32 InputSize,
    NvU32 OutputSize,
    const void *InputArgs,
    void *OutputArgs)
{
    SdBlockDevHandle hSdBlockDev = (SdBlockDevHandle)hBlockDev;
    NvError e = NvSuccess;
    SdHandle hSd = hSdBlockDev->hDev;

    // Lock before Ioctl
    NvOsMutexLock(hSdBlockDev->hDev->LockDev);

    // Decode the IOCTL opcode
    switch (Opcode)
    {
        case NvDdkBlockDevIoctlType_ReadPhysicalSector:
        case NvDdkBlockDevIoctlType_WritePhysicalSector:
        {
            // Check if partition create is done with part id == 0
            if (hSdBlockDev->MinorInstance)
            {
                e = NvError_NotSupported;
                break;
            }
            e = SdUtilRdWrSector(hSdBlockDev,
                Opcode, InputSize, InputArgs);
        }
        break;

        case NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector:
        {
            // Return the same logical address
            NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs *pMapPhysicalSectorIn =
                (NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs *)InputArgs;
            NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs *pMapPhysicalSectorOut =
                (NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs *)OutputArgs;

            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs));
            NV_ASSERT(OutputSize ==
                sizeof(NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs));
            NV_ASSERT(InputArgs);
            NV_ASSERT(OutputArgs);

             // Check if partition create is done with part id == 0
            if (hSdBlockDev->MinorInstance)
            {
                e = NvError_NotSupported;
                break;
            }
           pMapPhysicalSectorOut->PhysicalSectorNum =
                            pMapPhysicalSectorIn->LogicalSectorNum;
        }
        break;

        case NvDdkBlockDevIoctlType_GetPartitionPhysicalSectors:
        {
            NvU32 PhysicalSectorNum;
            NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsInputArgs
                *pLogicalBlockIn =
                (NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsInputArgs *)
                InputArgs;
            NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsOutputArgs
                *pPhysicalBlockOut =
                (NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsOutputArgs *)
                OutputArgs;
            NvS32 delta; //delta must be signed
            NvU32 NumberOfBlocks = 1;

            PhysicalSectorNum =
                pLogicalBlockIn->PartitionLogicalSectorStart;
            // Given the logical page start the physical is returned
            e = MmcSelectRegion(hSd, &PhysicalSectorNum, &NumberOfBlocks, NV_FALSE);
            if (e == NvSuccess)
            {
                delta = (NvS32)(PhysicalSectorNum -
                    pLogicalBlockIn->PartitionLogicalSectorStart);

                pPhysicalBlockOut->PartitionPhysicalSectorStart =
                    PhysicalSectorNum;
                // FIXME:Caller is not initializing the PartitionLogicalSectorStop
                pPhysicalBlockOut->PartitionPhysicalSectorStop = (NvU32)
                    ((NvS32)pLogicalBlockIn->PartitionLogicalSectorStop + delta);
            }
        }
        break;

        case NvDdkBlockDevIoctlType_FormatDevice:
        e = SdFormatDevice(hSdBlockDev);
        break;

        case NvDdkBlockDevIoctlType_PartitionOperation:
        {
            NvDdkBlockDevIoctl_PartitionOperationInputArgs *pPartOpIn;
            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_PartitionOperationInputArgs));
            NV_ASSERT(InputArgs);
            pPartOpIn = (NvDdkBlockDevIoctl_PartitionOperationInputArgs *)
                InputArgs;
            // Check if partition create is done with part id == 0
            if (hSdBlockDev->MinorInstance)
            {
                // Error as indicates block dev driver handle is
                // not part id == 0
                e = NvError_NotSupported;
                break;
            }
            if (pPartOpIn->Operation == NvDdkBlockDevPartitionOperation_Finish)
            {
                // Seems region table in memory should be updated using lock
                SdFreePartTbl(hSdBlockDev);
            }
        }
        break;

        case NvDdkBlockDevIoctlType_AllocatePartition:
        {
            NvDdkBlockDevIoctl_AllocatePartitionInputArgs *pAllocPartIn =
                (NvDdkBlockDevIoctl_AllocatePartitionInputArgs *)InputArgs;
            NvDdkBlockDevIoctl_AllocatePartitionOutputArgs *pAllocPartOut =
                (NvDdkBlockDevIoctl_AllocatePartitionOutputArgs *)OutputArgs;

            // Variable contains the start logical sector number for allocated partition
            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_AllocatePartitionInputArgs));
            NV_ASSERT(InputArgs);
            NV_ASSERT(OutputSize ==
                sizeof(NvDdkBlockDevIoctl_AllocatePartitionOutputArgs));
            NV_ASSERT(OutputArgs);

            // Check if partition create is done with part id == 0
            if (hSdBlockDev->MinorInstance)
            {
                // Error as indicates block dev driver handle
                // is not part id == 0
                e = NvError_NotSupported;
                break;
            }
            // Upper layer like partition manager keeps the partition table
            // Check for valid partitions - during creation
            // Check if partition create is done with part id == 0
            // Partition creation happens before NvDdkSdioOpen, so no locking needed
            e = SdUtilAllocPart(hSdBlockDev,
                pAllocPartIn, pAllocPartOut);
        }
        break;

        case NvDdkBlockDevIoctlType_EraseLogicalSectors:
        {
            NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs *ip =
                (NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs *)InputArgs;
            NvU32 StartSector;
            NvU32 NumSectors;

            NV_ASSERT(InputArgs);

            hSdBlockDev->hDev->DoSecureErase = NV_FALSE;
            hSdBlockDev->hDev->DoSanitize = NV_FALSE;
            if (ip->IsSecureErase == NV_TRUE)
            {
                if (hSdBlockDev->hDev->IsSanitizeSupported)
                    hSdBlockDev->hDev->DoSanitize = NV_TRUE;
                else if (hSdBlockDev->hDev->IsSecureEraseSupported)
                    hSdBlockDev->hDev->DoSecureErase = NV_TRUE;
            }

            StartSector = ip->StartLogicalSector <<
                hSdBlockDev->hDev->SectorsPerPageLog2;
            NumSectors = ip->NumberOfLogicalSectors <<
                hSdBlockDev->hDev->SectorsPerPageLog2;

            if ((hSdBlockDev->hDev->IsTrimEraseSupported) &&
                (ip->IsTrimErase))
            {
                hSdBlockDev->hDev->DoTrimErase = NV_TRUE;
                e = SdUtilTrimEraseLogicalSectors(hSdBlockDev, StartSector, NumSectors);
            }
            else
            {
                hSdBlockDev->hDev->DoTrimErase = NV_FALSE;
                e = SdUtilEraseLogicalSectors(hSdBlockDev, StartSector, NumSectors);
            }
        }
        break;

        case NvDdkBlockDevIoctlType_ErasePartition:
        {
            NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs *ip =
                (NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs *)InputArgs;
            NvU32 StartSector;
            NvU32 NumSectors;

            NV_ASSERT(InputArgs);

            hSdBlockDev->hDev->DoSecureErase = NV_FALSE;
            hSdBlockDev->hDev->DoSanitize = NV_FALSE;
            if (ip->IsSecureErase == NV_TRUE)
            {
                if (hSdBlockDev->hDev->IsSanitizeSupported)
                    hSdBlockDev->hDev->DoSanitize = NV_TRUE;
                else if (hSdBlockDev->hDev->IsSecureEraseSupported)
                    hSdBlockDev->hDev->DoSecureErase = NV_TRUE;
            }

            StartSector = ip->StartLogicalSector <<
                hSdBlockDev->hDev->SectorsPerPageLog2;
            NumSectors = ip->NumberOfLogicalSectors <<
                    hSdBlockDev->hDev->SectorsPerPageLog2;

            if ((hSdBlockDev->hDev->IsTrimEraseSupported) &&
                (ip->IsTrimErase))
            {
                hSdBlockDev->hDev->DoTrimErase = NV_TRUE;
                e = SdUtilTrimEraseLogicalSectors(hSdBlockDev, StartSector, NumSectors);
            }
            else
            {
                hSdBlockDev->hDev->DoTrimErase = NV_FALSE;
                e = SdUtilEraseLogicalSectors(hSdBlockDev, StartSector, NumSectors);
            }
        }
            break;

        case NvDdkBlockDevIoctlType_WriteVerifyModeSelect:
        {
            NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgs *pWriteVerifyModeIn =
                (NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgs *)InputArgs;

            NV_ASSERT(InputArgs);

            hSdBlockDev->hDev->IsReadVerifyWriteEnabled =
                pWriteVerifyModeIn->IsReadVerifyWriteEnabled;
        }
        break;

        case NvDdkBlockDevIoctlType_LockRegion:
        {
            NvDdkBlockDevIoctl_LockRegionInputArgs*pLockRegionIn =
                (NvDdkBlockDevIoctl_LockRegionInputArgs *)InputArgs;

            NV_ASSERT(pLockRegionIn);

            e = SdUtilLockBlock(hSdBlockDev, pLockRegionIn->LogicalSectorStart,
                pLockRegionIn->NumberOfSectors, pLockRegionIn->EnableLocks);
        }
        break;

        case NvDdkBlockDevIoctlType_ErasePhysicalBlock:
        {
            // Erases sector
            NvDdkBlockDevIoctl_ErasePhysicalBlockInputArgs *pErasePhysicalBlock =
                (NvDdkBlockDevIoctl_ErasePhysicalBlockInputArgs *)InputArgs;
            SdHandle hSd = hSdBlockDev->hDev;
            NvU32 StartSector;
            NvU32 NumOfSectors;

            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_ErasePhysicalBlockInputArgs));
            NV_ASSERT(InputArgs);

            hSdBlockDev->hDev->DoSecureErase = NV_FALSE;
            hSdBlockDev->hDev->DoSanitize = NV_FALSE;
            if (pErasePhysicalBlock->IsSecureErase == NV_TRUE)
            {
                if (hSdBlockDev->hDev->IsSanitizeSupported)
                    hSdBlockDev->hDev->DoSanitize = NV_TRUE;
                else if (hSdBlockDev->hDev->IsSecureEraseSupported)
                    hSdBlockDev->hDev->DoSecureErase = NV_TRUE;
            }

            StartSector = pErasePhysicalBlock->BlockNum *
                (hSd->BlockDevInfo.SectorsPerBlock);
            StartSector >>= hSd->SectorsPerPageLog2;

            NumOfSectors = pErasePhysicalBlock->NumberOfBlocks *
                (hSd->BlockDevInfo.SectorsPerBlock);
            NumOfSectors >>= hSd->SectorsPerPageLog2;

            if ((hSdBlockDev->hDev->IsTrimEraseSupported) &&
                (pErasePhysicalBlock->IsTrimErase))
            {
                hSdBlockDev->hDev->DoTrimErase = NV_TRUE;
                e = SdUtilTrimErase(hSdBlockDev, StartSector, NumOfSectors, 0);
                if (hSdBlockDev->hDev->DoSanitize == NV_TRUE)
                    MmcDoSanitize(hSdBlockDev->hDev);
            }
            else
            {
                hSdBlockDev->hDev->DoTrimErase = NV_FALSE;
                e = UtilClearSkipBlocks(hSdBlockDev, StartSector, NumOfSectors);
            }
        }
        break;

        case NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus:
        {
            NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs *pPhysicalQueryOut =
                (NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs *)OutputArgs;
            NvU32 LockStatus = 0;

            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_QueryPhysicalBlockStatusInputArgs));
            NV_ASSERT(OutputSize ==
                sizeof(NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs));
            NV_ASSERT(InputArgs);
            NV_ASSERT(OutputArgs);

            // EMMC does not have concept of bad blocks
            pPhysicalQueryOut->IsGoodBlock= NV_TRUE;
            // Check block for lock mode
            // Lock status is 32 bit value - each bit indicating if
            // the WP_GROUP is locked.
            // The bit value for LSB indicates the lock status for the
            // sector number passed to IsBlock Locked function
            pPhysicalQueryOut->IsLockedBlock = LockStatus & 0x1;
        }
        break;

        case NvDdkBlockDevIoctlType_VerifyCriticalPartitions:
        {
            e = NvSuccess;
        }
        break;

        case NvDdkBlockDevIoctlType_ConfigureWriteProtection:
        {
            NvDdkBlockDevIoctl_ConfigureWriteProtectionInputArgs *pInput =
                (NvDdkBlockDevIoctl_ConfigureWriteProtectionInputArgs *)InputArgs;
            NvBool PowerONWP = NV_FALSE;
            NvBool PermWP = NV_FALSE;

            NV_ASSERT(pInput);

            switch (pInput->WriteProtectionType)
            {
                case NvDdkBlockDevWriteProtectionType_Permanent:
                    PermWP = pInput->Enable;
                    break;
                case NvDdkBlockDevWriteProtectionType_PowerON:
                    PowerONWP = pInput->Enable;
                    break;
                default:
                    // Disable all types of write protection
                    break;
            }
            e = MmcConfigureWriteProtection(hSdBlockDev->hDev, PowerONWP, PermWP,
                pInput->Region);
        }
        break;
        case NvDdkBlockDevIoctlType_UnprotectAllSectors:
        case NvDdkBlockDevIoctlType_ProtectSectors:
            e = NvError_NotImplemented;
            break;
        case NvDdkBlockDevIoctlType_UpgradeDeviceFirmware:
            NvOsDebugPrintf("Update Device Firmware ioctl not Supported\n");
            break;
        default:
        e = NvError_BlockDriverIllegalIoctl;
        break;
    }

    // unlock at end of ioctl
    NvOsMutexUnlock(hSdBlockDev->hDev->LockDev);

    if (e != NvSuccess)
    {
        NvU8 IoctlName[80];
        // function to return name corresponding to ioctl opcode
        UtilGetIoctlName(Opcode, IoctlName);
        NvOsDebugPrintf("\r\nInst=%d, SD ioctl %s failed: error code=0x%x ",
            hSdBlockDev->hDev->Instance, IoctlName, e);
    }
    return e;
}

static void SdmmcFlushCache(NvDdkBlockDevHandle hBlockDev)
{
    // Unsupported function for SDMMC
}

NvError
NvDdkSdBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev)
{
    NvError e = NvSuccess;
    SdBlockDevHandle hSdBlkDev;

    *phBlockDev = NULL;

        NV_CHECK_ERROR_CLEANUP(SdOpen(Instance, MinorInstance, &hSdBlkDev));
        hSdBlkDev->BlockDev.NvDdkBlockDevClose = &SdClose;
        hSdBlkDev->BlockDev.NvDdkBlockDevGetDeviceInfo = &SdGetDeviceInfo;
        hSdBlkDev->BlockDev.NvDdkBlockDevReadSector = &SdRead;
        hSdBlkDev->BlockDev.NvDdkBlockDevWriteSector = &SdWrite;
        hSdBlkDev->BlockDev.NvDdkBlockDevPowerUp = &SdPowerUp;
        hSdBlkDev->BlockDev.NvDdkBlockDevPowerDown = &SdPowerDown;
        hSdBlkDev->BlockDev.NvDdkBlockDevRegisterHotplugSemaphore =
                                                    &SdRegisterHotplugSemaphore;
        hSdBlkDev->BlockDev.NvDdkBlockDevIoctl = &SdBlockDevIoctl;
        hSdBlkDev->BlockDev.NvDdkBlockDevFlushCache = &SdmmcFlushCache;
        *phBlockDev = &hSdBlkDev->BlockDev;

fail:
    return e;
}

NvError
NvDdkSdBlockDevInit(NvRmDeviceHandle hDevice)
{
    NvError e = NvSuccess;
    NvU32 i;

    if (!(s_SdCommonInfo.IsBlockDevInitialized))
    {
        // Initialize the global state including creating mutex
        // The mutex to lock access

        // Get Sd device instances available
        s_SdCommonInfo.MaxInstances =
            NvRmModuleGetNumInstances(hDevice, NvRmModuleID_Sdio);
        // Allocate table of SdBlockDevHandle, each entry in this table
        // corresponds to a device
        s_SdCommonInfo.SdDevStateList = NvOsAlloc(
            sizeof(SdHandle) * s_SdCommonInfo.MaxInstances);
        if (!s_SdCommonInfo.SdDevStateList)
            return NvError_InsufficientMemory;
        NvOsMemset((void *)s_SdCommonInfo.SdDevStateList, 0,
            (sizeof(SdHandle) * s_SdCommonInfo.MaxInstances));

        // Create locks per device
        s_SdCommonInfo.SdDevLockList = NvOsAlloc(
            sizeof(NvOsMutexHandle) * s_SdCommonInfo.MaxInstances);
        if (!s_SdCommonInfo.SdDevLockList)
            return NvError_InsufficientMemory;
        NvOsMemset((void *)s_SdCommonInfo.SdDevLockList, 0,
            (sizeof(SdHandle) * s_SdCommonInfo.MaxInstances));
        // Allocate the mutex for locking each device
        for (i = 0; i < s_SdCommonInfo.MaxInstances; i++)
        {
            e = NvOsMutexCreate(&(*(s_SdCommonInfo.SdDevLockList + i)));
            if (e != NvSuccess)
                return NvError_NotInitialized;
        }

        // Save the RM Handle
        s_SdCommonInfo.hRm = hDevice;
        s_SdCommonInfo.IsBlockDevInitialized = NV_TRUE;
    }
    // increment Sd block dev driver init ref count
    s_SdCommonInfo.InitRefCount++;
    return e;
}

void
NvDdkSdBlockDevDeinit(void)
{
    NvU32 i;
    SdHandle hTempDev = NULL;
    NvU32 BootDevice;
    NvU32 Instance;
    NvU32 Size;

    if (s_SdCommonInfo.IsBlockDevInitialized)
    {
        Size = sizeof(NvU32);
        NvDdkFuseGet(NvDdkFuseDataType_SecBootDeviceSelect,
                (void *)&BootDevice, (NvU32 *)&Size);
        NvDdkFuseGet(NvDdkFuseDataType_SecBootDevInst,
                (void *)&Instance, (NvU32 *)&Size);
        // If the boot device is emmc and ddk handle of the emmc device instance
        // is opened, do close it here
        if ((BootDevice == NvBootDevType_Sdmmc) &&
            (*(s_SdCommonInfo.SdDevStateList + Instance) != NULL))
        {
            hTempDev = (*(s_SdCommonInfo.SdDevStateList + Instance));
            NvDdkSdioClose(hTempDev->hSdDdk);
            NvOsFree(hTempDev);
            (*(s_SdCommonInfo.SdDevStateList + Instance)) = NULL;
        }

        if (s_SdCommonInfo.InitRefCount == 1)
        {
            // Dissociate from Rm Handle
            s_SdCommonInfo.hRm = NULL;
            // Free allocated list of handle pointers
            // free mutex list
            for (i = 0; i < s_SdCommonInfo.MaxInstances; i++)
            {
                NvOsFree((*(s_SdCommonInfo.SdDevStateList + i)));
                (*(s_SdCommonInfo.SdDevStateList + i)) = NULL;
                NvOsMutexDestroy((*(s_SdCommonInfo.SdDevLockList + i)));
            }
            NvOsFree(s_SdCommonInfo.SdDevLockList);
            s_SdCommonInfo.SdDevLockList = NULL;
            // free device instances
            NvOsFree(s_SdCommonInfo.SdDevStateList);
            s_SdCommonInfo.SdDevStateList = NULL;
            // reset the maximum Sd controller count
            s_SdCommonInfo.MaxInstances = 0;
            s_SdCommonInfo.IsBlockDevInitialized = NV_FALSE;
        }
        // decrement Sd dev driver init ref count
        s_SdCommonInfo.InitRefCount--;
    }
}


