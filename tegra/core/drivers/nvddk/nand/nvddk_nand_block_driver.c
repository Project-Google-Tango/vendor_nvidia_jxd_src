/*
 * Copyright (c) 2008 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file Nvddk_nand_block_driver.c
 * @brief <b>NVIDIA Driver Development Kit:
 *           NvDDK NAND Block Device Driver APIs</b>
 *
 * @b Description: Defines NvDDK NAND Block Device Driver Interface functions.
 *
 */

#include "nvddk_nand.h"
#include "nvassert.h"
#include "nvarguments.h"

/* When a device is formatted (or, optionally, at other times), the
 * block being operated on will randomly be marked bad with probability
 * (NAND_RANDOM_FAILURES / 2^(NAND_RANDOM_FAILURES_DENOMINATOR_LOG2)).
 */
#define NAND_RANDOM_FAILURES 0
// Macro to enable Extra block driver prints for firefly to debug
// bootup hang after Nand corruption
#define FIREFLY_CORRUPT_DEBUG 0
#define NAND_RANDOM_FAILURES_DENOMINATOR_LOG2 10

#include "nvos.h"
#include "bbmwl/nvnandregion.h"
#include "nvddk_nand_blockdev_defs.h"
#include "nvrm_module.h"
#include "bbmwl/ftlfull/nandconfiguration.h"
#include "bbmwl/ftlfull/nand_debug_utility.h"
#include "bbmwl/ftlfull/nanddefinitions.h"

// FIXME:
// We have enabled extra error codes to help identify cause
// for nvflash intermittent failures - Bug 589253
#ifndef NAND_MORE_ERR_CODES
#define NAND_MORE_ERR_CODES 1
#endif

#if NAND_MORE_ERR_CODES
// Current error codes use bits 0-23, hence using the bits 28-31 to
// distinguish between the error cases
#define NAND_ERR_MASK(x, err) \
            ((((x) & 0xF) << 28) | (err))
enum {
    LOCAL_ERROR_CODE1 = 1,
    LOCAL_ERROR_CODE2,
    LOCAL_ERROR_CODE3,
    LOCAL_ERROR_CODE4,
    LOCAL_ERROR_CODE5,
    LOCAL_ERROR_CODE6,
    LOCAL_ERROR_CODE7,
    LOCAL_ERROR_CODE8,
    LOCAL_ERROR_CODE9,
    LOCAL_ERROR_CODE10,
    LOCAL_ERROR_CODE11,
    LOCAL_ERROR_CODE12,
    LOCAL_ERROR_CODE13,
    LOCAL_ERROR_CODE14,
    LOCAL_ERROR_CODE15,
    LOCAL_ERROR_CODE_MAX
};
#endif

// Variable used to fake bad blocks used for debugging
#ifndef NAND_FAKE_BAD_ENABLED
#define NAND_FAKE_BAD_ENABLED 0
#endif

// Default Error message from block driver enabled
#ifndef NAND_BLK_DEV_ERROR_PRINT
#define NAND_BLK_DEV_ERROR_PRINT 1
#endif

// Debug print for read and write calls
#ifndef NAND_BLK_DEV_RD_WR_DEBUG
#define NAND_BLK_DEV_RD_WR_DEBUG 0
#endif

// Print Nand details
#ifndef ENABLE_NAND_INFO_PRINT
#define ENABLE_NAND_INFO_PRINT 0
#endif

// other block driver debug prints
#ifndef NAND_BLK_DEV_DEBUG
#define NAND_BLK_DEV_DEBUG 0
#endif

#if NAND_BLK_DEV_DEBUG
    #define NAND_BLK_DEV_DEBUG_INFO(X) NvOsDebugPrintf X
#else
    #define NAND_BLK_DEV_DEBUG_INFO(X)
#endif

#if NAND_BLK_DEV_ERROR_PRINT
#define NAND_BLK_DEV_ERROR_LOG(X) NvOsDebugPrintf X
#else
#define NAND_BLK_DEV_ERROR_LOG(X)
#endif

#if NAND_BLK_DEV_RD_WR_DEBUG
#define NAND_BLK_DEV_RD_WR_LOG(X) NvOsDebugPrintf X
#else
#define NAND_BLK_DEV_RD_WR_LOG(X)
#endif



// Constants used by the Nand block driver functions
enum {
    // Management policy constants to be passed to FTL
    FTL_LITE_POLICY = 1,
    FTL_FULL_POLICY = 2,
    // non-Nvidia FTL management policy
    EXT_FTL_POLICY = 3,
    // number of region table copies for redundancy
    NAND_REGIONTABLE_REDUNDANT_COPIES = 4,
    // Constant indicating start PBA, as Block 0 contains BCT
    FIRST_PHYSICAL_BLOCK = 1,
    // This must match with nvddk_nand.c definition which currently
    // uses 8 chips as maximum. So array with 8 elements is checked
    // for calls to NvDdk APIs.
    // Constant representing maximum number of Nand interleaved chips
    MAX_INTERLEAVED_NAND_CHIPS = 8,
    // const for log2 MAX_INTERLEAVED_NAND_CHIPS
    LOG2MAX_INTERLEAVED_NAND_CHIPS = 3, // log2(8) == 3,
    // Assumption of the default region table size
    DEFAULT_REGION_TBL_SIZE_TO_READ = 1,
    // Byte count of header for region table written to Nand Device
    REG_TBL_HEADER_BYTE_COUNT = 12,
    // default interleave setting is - no interleaving
    NAND_NOINTERLEAVE_COUNT = 1,
    // Tag info run-time bad block check uses this
    NAND_TAGINFO_GOOD_BLOCK_VALUE = 0xFF,
    // Interleave disable is 1 chip interleave setting
    NAND_INTERLEAVE_COUNT_1 = 1,
    NAND_INTERLEAVE_COUNT_2 = 2,
    NAND_INTERLEAVE_COUNT_4 = 4,
    MAX_NAND_INTERLEAVE_COUNT = NAND_INTERLEAVE_COUNT_4,
    // maximum number of lock apertures available
    NAND_LOCK_APERTURES = 8,
    // Assumed percent of bad blocks in user or last partition,
    // when the requested partition size is special value - 0xFFFFFFFF
    NAND_BAD_BLOCK_PERCENT_IN_USER_PART = 5,
    // constant to represent number of bytes in region table header
    // includes - start region table pattern, number of region table
    //            entries and device interleave count. Each field 4 bytes.
    NAND_REGIONTABLE_HEADER_SIZE = 3 * sizeof(NvU32),
    // constant to represent number of bytes in region table footer
    // includes - end region table pattern. Each field 4 bytes.
    NAND_REGIONTABLE_FOOTER_SIZE = 1 * sizeof(NvU32),
    // Keep a minimum of these many blocks as reserved in partitions
    NAND_MIN_PART_RESERVED_BLOCKS = 4,
    // Constant indicating unlocked lock aperture
    ILLEGAL_DEV_NUMBER = 0xFF
};

// These constants are not enums since they have MSB of NvU32 data set
//
// magic number before start of region table
static const NvU32 s_NandRegTblStartMagicNumber = 0xfeedbabe;
// magic number before end of region table
static const NvU32 s_NandRegTblEndMagicNumber = 0xbabeefed;
// illegal block address used to initialize table of block address
static const NvU32 s_NandIllegalAddress = 0xFFFFFFFF;
static NvS32 s_LastGoodBlock = (-1);

// used special case for remainder-of-device
#define NAND_PART_SIZE_ALLOC_REMAINING ((NvU32)-1)

// Macro to return the size of a region table entry
// FIXME: change this macro based on definition of structure
// NandRegionTabRec
// Deduct the size of next pointer from the sizeof NandRegionTabRec
#define NAND_MACRO_GET_SIZEOF_REGION_ENTRY \
    (sizeof(struct NandRegionTabRec) - (sizeof(NvU32 *)))

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

// Macro to calculate start sector number for last partition when
// block number, log2 of particular partition interleave count, and
// block driver handle is given
#define GET_PART_START_SECTOR_FROM_BLK_IL(Blk, hNandBlkDev, Log2IL) \
                MACRO_MULT_POW2_LOG2NUM(Blk, \
                    (hNandBlkDev->hDev->Log2PagesPerBlock + \
                    Log2IL));

// Structure to represent an entry in region table
typedef struct NandRegionTabRec
{
    // Data for each region
    NandRegionProperties RegProp;
    // Remember the partition id for the partition/region
    NvU32 MinorInstance;
    // next entry pointer
    struct NandRegionTabRec * Next;
} *NandRegionTabHandle;

// This represents entire Device specific information
typedef struct NandDevRec
{
    // major instance of device
    NvU32 Instance;
    // number of opened clients
    NvU32 RefCount;
    // Counter to keep track of power down clients
    NvU32 PowerUpCounter;
    // mutex for exclusive operations
    NvOsMutexHandle LockDev;
    // Ddk Nand handle
    NvDdkNandHandle hDdkNand;
    // Region Table for the device instance
    NandRegionTabHandle hHeadRegionList;
    // Interleave specific for device
    NvU32 InterleaveCount;
    // keep device info for quick access
    NvDdkBlockDevInfo BlkDevInfo;
    // store log2 of pages per block
    NvU32 Log2PagesPerBlock;
    // store log2 of pages per block
    NvU32 Log2BytesPerPage;
    // store log2 of interleave count
    NvU32 Log2InterleaveCount;
    NvDdkNandDeviceInfo NandDeviceInfo;
    // Ddk returned number of devices, chip count
    NvU32 DdkNumDev;
    // Flag to indicate if FTL is initialized
    NvU32 IsFtlInitialized;
    // Fields used only during partition creation -
    //       PhysicalAddressStart
    //       LogicalAddressStart
    // Variable to track physical block address of partitions created
    NvU32 PhysicalAddressStart;
    // Variable to track logical block address of partitions created
    NvU32 LogicalAddressStart;
    // Structure containing the locked apertures
    LockParams NandFlashLocks[NAND_LOCK_APERTURES];
    //Number of Apertures being used.
    NvU32 NumofLockAperturesUsed;
    //Flag to indicate if the flash has been locked before
    NvBool IsFlashLocked;
    // Structure holding the pointers to the Ddk Nand functions
    NvDdkNandFunctionsPtrs NandDdkFuncs;
    // Data for special partition part_id == 0 kept here
    // Block dev driver handle specific to a partition on a device
    // Buffer to hold the spare area data
    NvU8* pSpareBuf;
} *NandDevHandle;

typedef struct NandBlocDevRec {
    // This is partition specific information
    //
    // IMPORTANT: For code to work must ensure that  NvDdkBlockDev
    // is the first element of this structure
    // Block dev driver handle specific to a partition on a device
    NvDdkBlockDev BlockDev;
    // FTL handle for the partition represented by the block dev handle
    NvNandRegionHandle hFtl;
    // Keep pointer to Region Table entry for partition represented by
    // this block device driver
    NandRegionTabHandle hRegEntry;
    // Store partition interleave count log2
    NvU32 Log2InterleaveCountPart;
    // Flag to indicate the block driver is powered ON
    NvBool IsPowered;
    // Flag to indicate is write need to be read verified
    NvBool IsReadVerifyWriteEnabled;

    // This is device wide information
    //
    // One NandDevHandle is shared between multiple partitions on same device
    // This is a pointer and is created once only
    NandDevHandle hDev;
}NandBlocDev, *NandBlockDevHandle;

// Local type defined to store data common
// to multiple Nand Controller instances
typedef struct NandCommonInfoRec
{
    // For each Nand controller we are maintaining device state in
    // NandDevHandle. This data for a particular controller is also
    // visible when using the partition specific Nand Block Device Driver
    // Handle - NandBlockDevHandle
    NandDevHandle *NandDevStateList;
    // List of locks for each device
    NvOsMutexHandle *NandDevLockList;
    // Count of Nand controllers on SOC, queried from NvRM
    NvU32 MaxInstances;
    // Global RM Handle passed down from Device Manager
    NvRmDeviceHandle hRmNand;
    // Global boolean flag to indicate if init is done,
    // NV_FALSE indicates not initialized
    // NV_TRUE indicates initialized
    NvBool IsBlockDevInitialized;
    // Init nand block dev driver ref count
    NvU32 InitRefCount;
    // chip number within the last bank where the region table resides
    NvU32 LastBankChipNum;
    // Block number within the chip where the region table resides
    NvU32 ChipBlockNum;
    // Nand Erase type used during format
    NvDdkBlockDevEraseType NandEraseType;
} NandCommonInfo ;

typedef struct
{
    NvBool IsFactoryGoodBlock;
    NvBool IsRuntimeGoodBlock;
    // IsFactoryGoodBlock && IsRuntimeGoodBlock
    NvBool IsGoodBlock;
}NandSpareInfo;

// Static info shared by multiple Nand controllers,
// and make sure we initialize the instance
static NandCommonInfo s_NandCommonInfo = { NULL, NULL, 0, NULL, 0, 0, 0, 0, 0 };

// saves a copy of the region table for verification purpose
static NvU32* s_RegTableGoldenCopy = NULL;
// size of the region table
static NvU32 s_TotalRegTableByteCount = 0;


// Function to compute logical plus replacement block count
static NvU32
NandUtilGetReplacementBlocks(NvU32 x, NvU32 PerCent)
{
    NvU32 y;
    // Percent could be above 100
    // Ensure that ceiling of the replacement blocks is used
    y = PerCent * x / 100;
    if ((y * 100) < (PerCent * x))
        y++;
    // Add a const size of blocks as reserved. We are seeing issues
    // as probably runtime bad block info is no longer available after
    // nvflash erase
    y += NAND_MIN_PART_RESERVED_BLOCKS;
    return (y);
}

// Function to return a word(4byte data) from a byte array
static NvU32
NandUtilGetWord(NvU8 *pByte)
{
    NvU8 a, b, c, d;
    a = *(pByte + 0);
    b = *(pByte + 1);
    c = *(pByte + 2);
    d = *(pByte + 3);
    return (((d << 24) | (c << 16) | (b << 8) | (a)));
}

// Function used to convert device absolute logical sector
// address to partition relative logical sector address
static NvError
NandUtilGetPartitionRelativeAddr(
    NandBlockDevHandle hNandBlkDev, NvU32 *pFtlSector)
{
    NvError e = NvSuccess;
    // Partition Relative address should consider interleave mode
    NvU32 StartSector = GET_PART_START_SECTOR_FROM_BLK_IL(
        hNandBlkDev->hRegEntry->RegProp.StartLogicalBlock, hNandBlkDev,
        hNandBlkDev->Log2InterleaveCountPart);
    if (StartSector > *pFtlSector)
    {
        e = NvError_NandRegionIllegalAddress;
    }
    else
    {
        *pFtlSector = *pFtlSector - StartSector;
    }
    return e;
}

#if NAND_FAKE_BAD_ENABLED
// Locally defined user type to store fake bad information
typedef struct TagNandFakeBadBlkInfo
{
    NvU32 ChipNumber;
    NvU32 BadBlockNumber;
} NandFakeBadBlkInfo;

// Fake bad block table
static NandFakeBadBlkInfo s_NandFakeBadTable[] =
{
// SLC
        {0, 4},
        {1, 2047},
        {1, 2046},
        {1, 2045},
        {1, 2044},
        {1, 2043},
        {1, 2042},
        {1, 2040},
        {1, 8191},
        {1, 8190},
        {1, 8188},
        {1, 8187},
        {1, 8186},
// MLC
        {3, 8191},
        {3, 8190},
        {3, 8189},
        {3, 8188},
        {3, 8187},
        {3, 8186},
        // 2 bad block within locked region in WM
        {0, 405},
        {0, 458},
};
#endif

// This function fakes bad blocks and can be used to locally test
// different board behavior. Purpose is to test nvflash errors
// seen with different boards.
static NvBool
NandUtilIsFakeBad(NvU32 ChipNumber, NvU32 PBA)
{
#if NAND_FAKE_BAD_ENABLED
    NvU32 i;
    NvU32 NandFakeBadTableSize;
    NandFakeBadTableSize = (sizeof(s_NandFakeBadTable) /
        sizeof(NandFakeBadBlkInfo));
    for (i = 0; i < NandFakeBadTableSize; i++)
    {
        // If chip number and block number matches, block assumed bad
        if ((s_NandFakeBadTable[i].ChipNumber == ChipNumber) &&
            (s_NandFakeBadTable[i].BadBlockNumber == PBA))
        {
            NAND_BLK_DEV_ERROR_LOG(("\nFake Bad block: Chip%u Block=%u ",
                                                            ChipNumber, PBA));
            return NV_TRUE;
        }
    }
#endif
    return NV_FALSE;
}


// This function checks if the block with PBA passed as argument is good block
// If no interleaving mode is used, then caller may have called
// function NvBtlGetPba to get chipnumber and physical block before
// this function call. Else, make sure the chip number passed is correct.
static NvError
NandIsGoodBlock(NandBlockDevHandle hBlockDev, NvU32 ChipNumber,
                            NvU32 PBA, NandSpareInfo *pSpareInfo)
{
    NvError e = NvSuccess;
    NvDdkBlockDevIoctl_IsGoodBlockInputArgs BlockInfoInputArgs;
    NandBlockInfo NandBlockInfo;
    if (hBlockDev->hFtl)
    {
        NV_ASSERT(hBlockDev->hDev->pSpareBuf);
        NV_ASSERT(hBlockDev->hDev->NandDeviceInfo.NumSpareAreaBytes);
        NV_ASSERT(pSpareInfo);

        BlockInfoInputArgs.PhysicalBlockAddress = PBA;
        BlockInfoInputArgs.ChipNumber = ChipNumber;

        e = hBlockDev->hFtl->NvNandRegionIoctl(
                            hBlockDev->hFtl,
                            NvDdkBlockDevIoctlType_IsGoodBlock,
                            sizeof(NvDdkBlockDevIoctl_IsGoodBlockInputArgs),
                            sizeof(NvDdkBlockDevIoctl_IsGoodBlockOutputArgs),
                            &BlockInfoInputArgs, pSpareInfo);
    }
    else
    {
        // FIXME: the code below has to be removed once the DeleteAll command is
        // removed and Format partition is implemented
        NV_ASSERT(hBlockDev->hDev->pSpareBuf);
        NV_ASSERT(hBlockDev->hDev->NandDeviceInfo.NumSpareAreaBytes);
        NV_ASSERT(pSpareInfo);

        NandBlockInfo.pTagBuffer = hBlockDev->hDev->pSpareBuf;
        // Initialize the spare buffer before calling GetBlockInfo API
        NvOsMemset(NandBlockInfo.pTagBuffer, 0,
            hBlockDev->hDev->NandDeviceInfo.NumSpareAreaBytes);
        // Need skipped bytes along with tag information
        e = NvDdkNandGetBlockInfo(hBlockDev->hDev->hDdkNand, ChipNumber,
            PBA, &NandBlockInfo, NV_TRUE);
        if (e != NvSuccess)
        {
            pSpareInfo->IsFactoryGoodBlock = NV_FALSE;
            pSpareInfo->IsRuntimeGoodBlock = NV_FALSE;
            // We get failure sometimes and consider the blocks as bad
            e = NvSuccess;
        }
        else
        {
            pSpareInfo->IsFactoryGoodBlock = NV_TRUE;
            pSpareInfo->IsRuntimeGoodBlock = NV_TRUE;
        }

        // check factory bad block info
        if (NandBlockInfo.IsFactoryGoodBlock == NV_FALSE)
        {
            NAND_BLK_DEV_ERROR_LOG(("\nFactory Bad block: Chip%u Block=%u ",
                                                    ChipNumber, PBA));
            pSpareInfo->IsFactoryGoodBlock = NV_FALSE;
        }
        // run-time bad check
        if(hBlockDev->hDev->pSpareBuf[1] != 0xFF)
        {
            NAND_BLK_DEV_ERROR_LOG(("\nRuntime Bad block: Chip%u Block=%u,"
                "RTB=0x%x ", ChipNumber, PBA,
                hBlockDev->hDev->pSpareBuf[1]));
            pSpareInfo->IsRuntimeGoodBlock = NV_FALSE;
        }
        pSpareInfo->IsGoodBlock = pSpareInfo->IsFactoryGoodBlock &&
                                        pSpareInfo->IsRuntimeGoodBlock;
    }
    return e;
}

// This function checks if the block with PBA passed as argument is locked
// If no interleaving mode is used, then caller may have called
// function NvBtlGetPba to get chipnumber and physical block before
// this function call. Else, make sure the chip number passed is correct.
static NvError
NandIsBlockLocked(
    NvDdkNandHandle hDdkNand,
    NvU32 ChipNumber,
    NvU32 PBA,
    NvBool *pStatus)
{
    NvError ErrStatus = NvSuccess;
    NandBlockInfo NandBlockInfo;
    TagInformation TagInfo;
    NandBlockInfo.pTagBuffer = (NvU8 *)&TagInfo;
    ErrStatus = NvDdkNandGetBlockInfo(
        hDdkNand,
        ChipNumber,
        PBA,
        &NandBlockInfo, NV_FALSE);
    *pStatus = NandBlockInfo.IsBlockLocked;
    return ErrStatus;
}

// Calculate the maximum allowed interleave count for given CS count
static NvU32
NandUtilGetMaxInterleaveForCScount(NvU32 DevCount)
{
    NvU32 InterleaveCount;
    NvU32 NextInterleaveCount;
    // Start with leave interleave count allowed
    InterleaveCount = 1;
    NextInterleaveCount = InterleaveCount << 1;
    while ((!(DevCount % NextInterleaveCount)) &&
        (NextInterleaveCount <= MAX_NAND_INTERLEAVE_COUNT))
    {
        // Check for next higher InterleaveCount
        InterleaveCount <<= 1;
        NextInterleaveCount = InterleaveCount << 1;
    }
    return InterleaveCount;
}


// local function to mark blocks as bad
static NvError
UtilMarkBad(NandBlockDevHandle hNandBlockDev,
    NvU32 ChipNum, NvU32 BlockNum)
{
    NvError e = NvError_Success;
    NvU32 SpareAreaSize;
    NvU32 dev;
    NvU32 Log2PgsPerBlk;
    NvU32 PageNumber[MAX_INTERLEAVED_NAND_CHIPS];

    NV_ASSERT(ChipNum < MAX_NAND_SUPPORTED);

    NAND_BLK_DEV_ERROR_LOG(("\nMarking Runtime Bad block: Chip%u Block=%u ",
                                            ChipNum, BlockNum));

    for(dev = 0; dev < MAX_INTERLEAVED_NAND_CHIPS;dev++)
        PageNumber[dev] = s_NandIllegalAddress;

    SpareAreaSize = hNandBlockDev->hDev->NandDeviceInfo.NumSpareAreaBytes;
    NV_ASSERT(SpareAreaSize);
    NV_ASSERT(hNandBlockDev->hDev->pSpareBuf);

    Log2PgsPerBlk = NandUtilGetLog2(hNandBlockDev->hDev->NandDeviceInfo.PagesPerBlock);
    PageNumber[ChipNum] = (BlockNum << Log2PgsPerBlk);
    NvOsMemset(hNandBlockDev->hDev->pSpareBuf, 0x0, SpareAreaSize);
    //mark the block as runtime bad
    NV_CHECK_ERROR_CLEANUP(NvDdkNandWriteSpare(hNandBlockDev->hDev->hDdkNand,
        ChipNum, PageNumber,hNandBlockDev->hDev->pSpareBuf,
        1, (SpareAreaSize -1)));
fail:
    if (e != NvSuccess)
    {
        NvOsDebugPrintf("\nBlock driver mark bad failed at "
            "Chip=%d, Block=%d ",ChipNum, BlockNum);
    }

    return e;
}

// Function used to scan the last Nand chip and find good block
// for region table
static NvError
    NandUtilGetLastGoodBlock(NandBlockDevHandle hBlockDev, NvS32 * pLastGoodBlock)
{
    NvS32 k = 0;
    NvU32 CurrDevNum = 0;
    NvU32 Log2BlocksPerChip;
    NvBool IsGoodBlk;
    NvBool IsFakeBad;
    NvError e = NvSuccess;
    NandSpareInfo SpareInfo;

    Log2BlocksPerChip = NandUtilGetLog2(hBlockDev->hDev->NandDeviceInfo.NoOfBlocks);
    CurrDevNum = (hBlockDev->hDev->NandDeviceInfo.NumberOfDevices -1);
    for (k = (hBlockDev->hDev->NandDeviceInfo.NoOfBlocks -1);
                k >= 0;
                k--)
    {
        // Region table is written to the last chip always.
        {
            NV_CHECK_ERROR_CLEANUP(
                NandIsGoodBlock(hBlockDev,
                CurrDevNum,
                k,
                &SpareInfo)
                );
            IsFakeBad = NandUtilIsFakeBad(CurrDevNum, k);
            IsGoodBlk = (SpareInfo.IsGoodBlock && (!IsFakeBad));
        }
        if (!IsGoodBlk)
        {
            // Block to write region table is bad block
            NAND_BLK_DEV_ERROR_LOG(("\nScan for Region table blocks: Chip=%u, "
                "Block=%u Bad ", CurrDevNum, k));
            // try to check the other interleaved chips or
            // remaining blocks in current chip
            continue;
        }
        else
        {
            *pLastGoodBlock = (CurrDevNum << Log2BlocksPerChip) + k;
            s_NandCommonInfo.LastBankChipNum = CurrDevNum;
            s_NandCommonInfo.ChipBlockNum = k;
            break; // we found a good block
        }
    }
fail:
    return e;
}

// Given chip number and block number on chip, erases the block
// Function assumes block to be erased is good block
static NvError NandUtilEraseBlock(NandBlockDevHandle hNandBlockDev,
    NvU32 ChipNum, NvU32 BlockAddr)
{
    NvU32 TmpBlkCount;
    NvU32 PageNumber[MAX_INTERLEAVED_NAND_CHIPS];
    NvError e = NvSuccess;
    NvOsMemset(PageNumber, 0xFF,
        MACRO_MULT_POW2_LOG2NUM(sizeof(NvU32),
        LOG2MAX_INTERLEAVED_NAND_CHIPS));
    PageNumber[ChipNum] = MACRO_MULT_POW2_LOG2NUM(BlockAddr,
        hNandBlockDev->hDev->Log2PagesPerBlock);
    // Erase a block at a time
    TmpBlkCount = 1;
    if ((e = NvDdkNandErase(hNandBlockDev->hDev->hDdkNand,
        ChipNum, PageNumber, &TmpBlkCount)) != NvError_Success)
    {
        //mark the block as runtime bad, if erase fails
        TmpBlkCount = 1;
        NAND_BLK_DEV_ERROR_LOG(("\nMarking Runtime Bad block: Chip%u Block=%u ",
                                                ChipNum, BlockAddr));
        NV_CHECK_ERROR_CLEANUP(UtilMarkBad(hNandBlockDev, ChipNum, BlockAddr));
        if ((e != NvSuccess) || ((!TmpBlkCount) && (e == NvSuccess)))
        {
            NvOsDebugPrintf("\r\nErase Partition Error: failed to erase "
                "block chip=%d,blk=%d ", ChipNum, BlockAddr);
            e = NvError_NandBlockDriverEraseFailure;
        }
    }
fail:
    return e;
}

// Local function to erase given number of sector starting
// from a particular sector address on one chip. Depending on
// type of erase some times run-time bad blocks are erased or skipped
static NvError
NandUtilEraseChipBlocks(NandBlockDevHandle hNandBlockDev,
    NvU32 BlockAddr, NvU32 BlockCount, NvU32 ChipNum,
    NvDdkBlockDevEraseType EraseType)
{
    NvError e = NvSuccess;
    NvU32 i;
    NandSpareInfo SpareInfo;
    NvBool IsFakeBad;

    for (i = 0; i < BlockCount; i++)
    {
        e = NandIsGoodBlock(hNandBlockDev, ChipNum, (BlockAddr + i), &SpareInfo);
        if (e != NvSuccess)
        {
            // Try erasing next block
            continue;
        }

        // Check if need to format based on EraseType
        switch (EraseType)
        {
            case NvDdkBlockDevEraseType_NonFactoryBadBlocks:
                // check factory badblock info
                if (!SpareInfo.IsFactoryGoodBlock)
                {
                    // This is factory bad block. It cannot be erased
                    // try next block erase
                    continue;
                }
                break;
            case NvDdkBlockDevEraseType_GoodBlocks:
            default:
                if (!SpareInfo.IsRuntimeGoodBlock)
                {
                    // This is bad block hence do not erase
                    // try next block erase
                    continue;
                }
                break;
        }
        // Check for fake bad as well
        IsFakeBad = NandUtilIsFakeBad(ChipNum, (BlockAddr + i));
        if (!IsFakeBad)
        {
            e = NandUtilEraseBlock(hNandBlockDev, ChipNum, (BlockAddr + i));
            if (e != NvSuccess)
            {
                // Try erasing next block
                continue;
            }
        }
    }
    return e;
}

// Utility function that gives static estimate of maximum block count
// The super block count is returned, so for interleaving
// the block count is less. For non-interleaved mode the block
// count is more.
static NvU32
NandUtilGetMaxBlockOnChip(NandBlockDevHandle hNandBlockDev)
{
    NvU32 MaxBlock;
    NvU32 Log2NumDevs;
    // Multiplier calculated depending on interleaving mode
    Log2NumDevs = NandUtilGetLog2(hNandBlockDev->hDev->NandDeviceInfo.NumberOfDevices);
    // get super block count
    MaxBlock = MACRO_MULT_POW2_LOG2NUM(hNandBlockDev->hDev->NandDeviceInfo.NoOfBlocks,
        (Log2NumDevs - hNandBlockDev->hDev->Log2InterleaveCount));
    // Make sure that region table good block is found
    if (s_LastGoodBlock < 0)
    {
        // Initializes the region table location on last chip
        (void)NandUtilGetLastGoodBlock(hNandBlockDev, &s_LastGoodBlock);
        // check if we have a valid last good block
        NV_ASSERT(s_LastGoodBlock >= 0);
    }
    // Deduct space for region table
    // We only exclude the last partition blocks in sequential access
    MaxBlock -= (hNandBlockDev->hDev->NandDeviceInfo.NoOfBlocks -
        s_NandCommonInfo.ChipBlockNum);
    return MaxBlock;
}

#if 0
// Find good physical block on a chip starting at specified physical block
static NvError
NandUtilGetGoodPhysicalBlockOnChip(
    NandBlockDevHandle hBlockDev, NvU32 ChipNum,
    NvU32 MaxChipBlock, NvU32 *pPhysicalBlock)
{
    NvU32 BlkNum = *pPhysicalBlock;
    NvError e = NvSuccess;
    NandSpareInfo SpareInfo;
    // Call function to check if physical block passed is good block

    NV_CHECK_ERROR_CLEANUP(NandIsGoodBlock(hBlockDev, ChipNum, BlkNum, &SpareInfo));
    while (!SpareInfo.IsGoodBlock)
    {
        NAND_BLK_DEV_ERROR_LOG(("\n Chip%d Block=%d bad ", ChipNum, BlkNum));
        // Limit the bad block skip to within maximum allowed blocks
        if ((BlkNum + 1) < MaxChipBlock)
            BlkNum++;
        else
            // Return error when cross maximum block allowed
            return NvError_NandRegionIllegalAddress;

        NV_CHECK_ERROR_CLEANUP(NandIsGoodBlock(hBlockDev, ChipNum, BlkNum, &SpareInfo));
    }
    // Initialize the good block number found
    *pPhysicalBlock = BlkNum;
    return NvSuccess;
fail:
    return e;
}
#endif
// Local function used to dump bad block table
static void
NandUtilDumpBad(NandBlockDevHandle hBlockDev)
{
    NvU32 ChipNum;
    NvU32 BlockIndex;
    NvU32 BadBlockCount;

    BadBlockCount = 0;
    for (ChipNum = 0;
        ChipNum < hBlockDev->hDev->NandDeviceInfo.NumberOfDevices; ChipNum++)
    {
        for (BlockIndex = 0;
            BlockIndex < hBlockDev->hDev->NandDeviceInfo.NoOfBlocks;
            BlockIndex++)
        {
            // Check for bad block
            NandSpareInfo SpareInfo;
            (void)NandIsGoodBlock(hBlockDev, ChipNum, BlockIndex, &SpareInfo);
            if (!SpareInfo.IsGoodBlock)
            {
                if (BadBlockCount == 0)
                {
                    NAND_BLK_DEV_ERROR_LOG(("\nDevice Bad block table: "));
                }
                BadBlockCount++;
                // Block is bad
                NAND_BLK_DEV_ERROR_LOG(("\n        {%u, %d}, ", ChipNum,
                    BlockIndex));
            }
        }
    }
    NAND_BLK_DEV_ERROR_LOG(("\nDevice has %d bad blocks ", BadBlockCount));
}

// Function to perform read verification of Ddk write calls
static NvError
NandReadVerifyDdkWrite(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 SectorNum,
    NvU32 ChipNum,
    const void* pBuffer,
    NvU32 NumberOfSectors);

// Dummy function that marks blocks as bad after read/write physical failure
static NvError
NandUtilMarkBlockBad(NandBlockDevHandle hNandBlockDev,
    NvU32 ChipNum, NvU32 CurrBlockNum)
{
    NvError e = NvError_Success;
    NvOsDebugPrintf("\nBlock dev Physical Ioctl failed. Marking Chip=%d,Blk=%d ", ChipNum, CurrBlockNum);
    // FIXME: do compare
    return e;
}

// This should be called in no interleave mode, since
// data written differs depending on interleave or no interleave
// Assumed this function is called for part_id==0 block driver open
// Caller first calls MapLogicalToPhysical Ioctl and then uses
// ReadPhysical Ioctl
static NvError
NandUtilRdWrPhysicalSector(NandBlockDevHandle hNandBlockDev,
    NvU32 Opcode,
    NvU32 InputSize,
    const void * InputArgs)
{
    NvError e = NvSuccess;
    NvU32 PageNumber[MAX_INTERLEAVED_NAND_CHIPS];
    NvU32 RemainingSectors = 0;
    NvU32 CurrSectorNum = 0;
    NvDdkBlockDevInfo *pDeviceInfo;
    NvU32 SectorsInCurrBlock;
    NvU32 SectorsFromChip;
    NvU32 SectorFromChipCp;
    // Chip number if uninitialized may remain so after call to NvBtlGetPba
    NvU32 ChipNum = 0;
    NvU32 OrigChipNum;
    NvU32 i;
    NvU32 CurrBlockNum;
    NvU32 RetBlockNum;
    NvU32 SectorOffset;
    NandSpareInfo SpareInfo;

    NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs *pRdPhysicalIn =
        (NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs *)InputArgs;
    NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs *pWrPhysicalIn =
        (NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs *)InputArgs;

    NV_ASSERT(InputArgs);
    OrigChipNum = ChipNum;
    pDeviceInfo = &(hNandBlockDev->hDev->BlkDevInfo);
    // Data is read from/written on chip block size at max each time
    if (Opcode == NvDdkBlockDevIoctlType_ReadPhysicalSector)
    {
        NV_ASSERT(InputSize ==
            sizeof(NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs));
        RemainingSectors = pRdPhysicalIn->NumberOfSectors;
        pRdPhysicalIn->NumberOfSectors = 0;
        // Get physical sector number
        CurrSectorNum = pRdPhysicalIn->SectorNum;
    }
    else if (Opcode == NvDdkBlockDevIoctlType_WritePhysicalSector)
    {
        NV_ASSERT(InputSize ==
            sizeof(NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs));
        RemainingSectors = pWrPhysicalIn->NumberOfSectors;
        pWrPhysicalIn->NumberOfSectors = 0;
        // Get physical sector number
        CurrSectorNum = pWrPhysicalIn->SectorNum;
    }
    // Partitions that use physical ioctls are never interleaved
    CurrBlockNum = MACRO_DIV_POW2_LOG2NUM(CurrSectorNum,
            hNandBlockDev->hDev->Log2PagesPerBlock);
    SectorOffset = MACRO_MOD_LOG2NUM(CurrSectorNum,
            hNandBlockDev->hDev->Log2PagesPerBlock);
#if 0
    // BCT write is being done to sector 1. Since we erase the corresponding
    // block from the start, sector 0 will be erased as part of write physical
    if (Opcode == NvDdkBlockDevIoctlType_WritePhysicalSector)
    {
        // Write needs erase hence we should get CurrSectorNum aligned to
        // start of block
        if (SectorOffset > 0)
        {
            // sector to write is not aligned to block beginning
            return NvError_NandBlockDriverWriteFailure;
        }
    }
#endif
    // Write one block at a time on each chip
    while (RemainingSectors)
    {
        SectorsInCurrBlock = pDeviceInfo->SectorsPerBlock - SectorOffset;
        SectorsFromChip = (SectorsInCurrBlock <= RemainingSectors)?
            SectorsInCurrBlock : RemainingSectors;
        // Check if current block in this chip is good
        RetBlockNum = CurrBlockNum;
        // Get Chip number and physical block number - given current physical
        // address, as we are doing sequential access(no interleaving)
        // on multiple chips
        if ((NvBtlGetPba(&ChipNum, &RetBlockNum) == NV_FALSE) ||
            (ChipNum >= hNandBlockDev->hDev->NandDeviceInfo.NumberOfDevices))
        {
            return NvError_InvalidAddress;
        }
        NV_CHECK_ERROR_CLEANUP(NandIsGoodBlock(hNandBlockDev,
            ChipNum, RetBlockNum, &SpareInfo));
        if (SpareInfo.IsGoodBlock)
        {
            // Initialize the Page numbers first since Ddk needs
            // interleaved chip page numbers
            for (i = 0; i < MAX_INTERLEAVED_NAND_CHIPS; i++)
                PageNumber[i] = s_NandIllegalAddress;
            PageNumber[ChipNum] = (MACRO_MULT_POW2_LOG2NUM(RetBlockNum,
                hNandBlockDev->hDev->Log2PagesPerBlock) + SectorOffset);
            SectorFromChipCp = SectorsFromChip;
            if (Opcode == NvDdkBlockDevIoctlType_ReadPhysicalSector)
            {
                // Read starting Physical sector address
                NV_CHECK_ERROR_CLEANUP(NvDdkNandRead(
                    hNandBlockDev->hDev->hDdkNand, ChipNum, PageNumber,
                    pRdPhysicalIn->pBuffer, NULL,
                    &SectorFromChipCp, NV_FALSE));
            }
            else
            {
                // Must erase Nand sectors before writing them with DDk API
                NV_CHECK_ERROR_CLEANUP(NandUtilEraseChipBlocks(hNandBlockDev,
                    CurrBlockNum, 1, ChipNum,
                    NvDdkBlockDevEraseType_GoodBlocks));
                // If block turns bad after erase we return error
                // Write starting Physical sector address
                NV_CHECK_ERROR_CLEANUP(NvDdkNandWrite(
                    hNandBlockDev->hDev->hDdkNand, ChipNum,
                    PageNumber, (NvU8 *)(pWrPhysicalIn->pBuffer),
                    NULL, &SectorFromChipCp));
                if (hNandBlockDev->IsReadVerifyWriteEnabled)
                {
                    // Read Verify function to be passed
                    // arguments no modified by Btl call
                    NV_CHECK_ERROR_CLEANUP(NandReadVerifyDdkWrite(
                        &(hNandBlockDev->BlockDev), CurrSectorNum,
                        OrigChipNum,pWrPhysicalIn->pBuffer, SectorFromChipCp));
                    //NvOsDebugPrintf("\nRead verify failed, Chip=%d,Blk=%d ", ChipNum, CurrBlockNum);
                }
            }
            if (SectorFromChipCp == SectorsFromChip)
            {
                if (Opcode == NvDdkBlockDevIoctlType_ReadPhysicalSector)
                {
                    pRdPhysicalIn->NumberOfSectors += SectorsFromChip;
                }
                else
                {
                    pWrPhysicalIn->NumberOfSectors += SectorsFromChip;
                }
                // Successfully wrote SectorsFromChip sectors to this block
                RemainingSectors -= SectorsFromChip;
                // move to next physical block and update start sector
                CurrBlockNum++;
                CurrSectorNum += SectorsFromChip;
            }
            else
            {
                // We read/wrote less than requested amount
                // as SectorFromChipCp (actual sectors written)
                // differs from SectorsFromChip
                NvOsDebugPrintf("\nPhysical Rd/Wr on block error: req=%d,"
                    "actual=%d ", SectorsFromChip, SectorFromChipCp);
                // Flag as error
                if (Opcode == NvDdkBlockDevIoctlType_ReadPhysicalSector)
                {
                    e = NvError_NandBlockDriverReadFailure;
                }
                else
                {
                    e = NvError_NandBlockDriverWriteFailure;
                }
                // Return the last error for Rd or write
                break;
            }
        }
        else
        {
            // In physical read/write any block found bad is returned as error
            NvOsDebugPrintf("\nBad block during Rd/Wr physical found at: Chip=%d, Block=%d ", ChipNum, CurrBlockNum);
            e = NvError_NandBadBlock;
            break;
        }
        // Sector offset is reset since all blocks except first are
        // aligned to block boundary
        SectorOffset = 0;
    }
fail:
    if (e != NvError_Success)
    {
        NandUtilMarkBlockBad(hNandBlockDev, ChipNum, CurrBlockNum);
    }
    return e;
}

// Function to erase physical blocks specified
// Treat as sequential physical block erase  - one block erase at a time
static NvError
NandUtilErasePhysicalBlock(NandBlockDevHandle hNandBlockDev,
    NvU32 BlockNum, NvU32 NumberOfBlocks)
{
    NvError e = NvSuccess;
    NvU32 EraseCount = 0;
    NvU32 PageNumbers[MAX_INTERLEAVED_NAND_CHIPS];
    NvU32 BlockStart;
    // Chip number if uninitialized may remain so after call to NvBtlGetPba
    NvU32 ChipNum = 0;
    NvU32 ActualBlockNum;
    NvU32 j;
    BlockStart = BlockNum;
    // get chip number for physical block address
    if ((NvBtlGetPba(&ChipNum, &BlockStart) == NV_FALSE) ||
        (ChipNum >= hNandBlockDev->hDev->NandDeviceInfo.NumberOfDevices))
    {
        return NvError_InvalidAddress;
    }
    ActualBlockNum = 1;
    while (EraseCount < NumberOfBlocks)
    {
        // initialize Page number needed for Ddk call
        for (j = 0; j < MAX_INTERLEAVED_NAND_CHIPS; j++)
        {
            PageNumbers[j] = s_NandIllegalAddress;
        }
        PageNumbers[ChipNum] = (MACRO_MULT_POW2_LOG2NUM(BlockStart,
            hNandBlockDev->hDev->Log2PagesPerBlock));
        // Erase 1 block
        if ((e = NvDdkNandErase(hNandBlockDev->hDev->hDdkNand, ChipNum,
            PageNumbers, &ActualBlockNum)) != NvSuccess)
        {
            NAND_BLK_DEV_ERROR_LOG(("\nUnable to Erase Nand starting "
                "block 0x%x ", BlockStart));
            goto LblEnd;
        }
        // Change Block Start after processing all chips
        if ((EraseCount > 0) && (MACRO_MOD_LOG2NUM(EraseCount,
            hNandBlockDev->hDev->Log2InterleaveCount)) == 0)
            BlockStart++;
        EraseCount++;
        // move to next chip to erase same block
        ChipNum = MACRO_MOD_LOG2NUM((ChipNum + 1),
            hNandBlockDev->hDev->Log2InterleaveCount);
    }
LblEnd:
    return e;
}

// Utility to print partitions in region table
static void NandUtilPrintRegionTable(NandDevHandle hDev)
{
    NandRegionTabHandle hRegEntry = hDev->hHeadRegionList;
    NvOsDebugPrintf("\nPartitions in region table: ");
    while (hRegEntry)
    {
        NvOsDebugPrintf(" Id=%d ", hRegEntry->MinorInstance);
        hRegEntry = hRegEntry->Next;
    }
}

// Function to map part_id to region table entry
// This function looks up the region table using the
// partition-id argument MinorInstance
static NvError
NandUtilGetRegionEntry(NandDevHandle hDev, NvU32 MinorInstance,
    NandRegionTabHandle *pHndRegEntry)
{
    NandRegionTabHandle hRegEntry = hDev->hHeadRegionList;
    while (hRegEntry)
    {
        if (hRegEntry->MinorInstance == MinorInstance)
        {
            // found the partition-id region entry, copy region properties
            *pHndRegEntry = hRegEntry;
            return NvSuccess;
        }
        hRegEntry = hRegEntry->Next;
    }
    NvOsDebugPrintf("\nError: NandUtilGetRegionEntry failed for part Id=%d ", MinorInstance);
    // Print Region table
    NandUtilPrintRegionTable(hDev);
    // FIXME: need error type - Partition not found on device
#if NAND_MORE_ERR_CODES
    return NAND_ERR_MASK(LOCAL_ERROR_CODE1, NvError_NandRegionTableOpFailure);
#else
    return NvError_NandRegionTableOpFailure;
#endif
}

// Function to map logical sector address to
// physical sector address when part_id == 0(raw block device driver access).
// Given logical sector number, need to find the physical sector address that
// corresponds to the logical sector number
// Note: Assumed no interleaving
static NvError
NandUtilLogical2PhysicalSector(NandBlockDevHandle hNandBlockDev,
    NvU32 LogicalSectorNum, NvU32 *pPhysicalSectorNum)
{
    // Start from first Physical block
    NvU32 CurrPhysicalBlock = FIRST_PHYSICAL_BLOCK;
    NvU32 MaxPhysicalBlock;
    NvU32 TagLogicalBlock;
    TagInformation *pTagInfo;
    // Table to help remember the last physical blocks read on a chip
    NvU32 SectorOffset;
    NvU32 BlockToSearch;
    NvU8 *pByte;
    NvError e = NvSuccess;
    NvBool IsGoodBlock;
    NvU32 DeviceNum = 0;
    NvU32 BlockNum = CurrPhysicalBlock;
    NandSpareInfo SpareInfo;
    NvBool IsFakeBad;

    // Start addresses are based on global interleave setting
    // Calculate the block address for given logical sector to search
    BlockToSearch = MACRO_DIV_POW2_LOG2NUM(LogicalSectorNum,
        hNandBlockDev->hDev->Log2PagesPerBlock);
    // sector offset in last block
    SectorOffset = MACRO_MOD_LOG2NUM(LogicalSectorNum,
        hNandBlockDev->hDev->Log2PagesPerBlock);

    // Logical 2 Physical should be used for partitions written
    // in sequential mode
    MaxPhysicalBlock = MACRO_DIV_POW2_LOG2NUM(
        hNandBlockDev->hDev->BlkDevInfo.TotalBlocks,
        hNandBlockDev->hDev->Log2InterleaveCount);
    while (CurrPhysicalBlock < MaxPhysicalBlock)
    {
         // find a good block starting from the beginning
        do
        {
            {
                DeviceNum = 0;
                BlockNum = CurrPhysicalBlock;
                if ((NvBtlGetPba(&DeviceNum, &BlockNum) == NV_FALSE) ||
                    (DeviceNum >= hNandBlockDev->hDev->NandDeviceInfo.NumberOfDevices))
                {
                    // chip number or block address illegal
                    NAND_BLK_DEV_ERROR_LOG(("\nNand Block driver map "
                        "logical2physical failed "
                        "BlockNum=%d, DeviceNum=%d, CurrPhysBlk=%d ",
                        BlockNum, DeviceNum, CurrPhysicalBlock));
                    return NvError_InvalidAddress;
                }
                NV_CHECK_ERROR_CLEANUP(NandIsGoodBlock(hNandBlockDev,
                                                DeviceNum,
                                                BlockNum, &SpareInfo));
                IsFakeBad = NandUtilIsFakeBad(DeviceNum, BlockNum);
                IsGoodBlock = (SpareInfo.IsGoodBlock && (!IsFakeBad));
            }
            CurrPhysicalBlock++;
            // Break out when block number crosses end of device
            if (CurrPhysicalBlock >= MaxPhysicalBlock)
                break;
        } while (!IsGoodBlock);
        pTagInfo = (TagInformation*)(hNandBlockDev->hDev->pSpareBuf +
            hNandBlockDev->hDev->NandDeviceInfo.TagOffset);
        // get logical block number for good block  referring to tag info
        // AOS compilation gets packing issue for TagInformation structure
        // TagLogicalBlock = TagInfo.LogicalBlkNum;
        pByte = (NvU8 *)&(pTagInfo->LogicalBlkNum);
        TagLogicalBlock = NandUtilGetWord(pByte);
        if (TagLogicalBlock != s_NandIllegalAddress)
        {
            if (TagLogicalBlock == BlockToSearch)
            {
                // Got block containing given sector number, decrement
                // CurrPhysicalBlock as it indicates next block
                *pPhysicalSectorNum = (MACRO_MULT_POW2_LOG2NUM(
                    (CurrPhysicalBlock - 1),
                    hNandBlockDev->hDev->Log2PagesPerBlock)
                    + SectorOffset);
                return NvSuccess;
            }
            if ((TagLogicalBlock > BlockToSearch) &&
                (CurrPhysicalBlock > BlockToSearch))
            {
                // Flag error as already passed the block
                // containing sector number
                // Do not print this as at least in a case this is legal
                //NAND_BLK_DEV_ERROR_LOG(("\nNand block driver Map "
                    //"logical2physical failed - "
                    //"uninitialized block %u. Found next logical block=%u "
                    //, BlockToSearch, TagLogicalBlock));
                return NvError_NandTagAreaSearchFailure;
            }
        }
        // Continue with next physical block
    }
    // At this point entire Nand chip has already been searched
    NAND_BLK_DEV_ERROR_LOG(("\nError: Failed to map logical block=%d "
        "in entire Nand. ", BlockToSearch));
    return NvError_NandTagAreaSearchFailure;
fail:
    return e;
}

// Function to free region table
static void RegionTableFree(NandRegionTabHandle PrevLink)
{
    NandRegionTabHandle hRegEntry;
    while (PrevLink)
    {
        // Get next entry
        hRegEntry = PrevLink->Next;
        // Free last entry
        NvOsFree(PrevLink);
        PrevLink = hRegEntry;
    }
}

// Function to interpret 1 sector sized data region table data read from device
// Returns :
//      number of region table entries
//      size of a region table entry
//      bytes of data read
//      initializes Device interleave count read in hDev
static NvError
NandUtilInterpretRegTblData(NandDevHandle hDev,
    NvU8 *Buf, NvU32 BufSize, NvU32 *pRegCount, NvU32 * pRegEntrySizeInBytes,
    NvU32 *pBytesOffset)
{
    NvU32 i;
    NvError e = NvSuccess;
    NvU32 BytesOffset;
    NandRegionTabHandle hRegEntryNew;
    NandRegionTabHandle hRegEntry = NULL;
    NvU8 *ptr;
    NvU32 RegionEntryByteCount;

    // Interpret read data first
    ptr = Buf;
    BytesOffset = 0;
    //  a) start region table pattern/number
    if ((*((NvU32 *)(ptr))) != s_NandRegTblStartMagicNumber)
    {
#if NAND_MORE_ERR_CODES
        e = NAND_ERR_MASK(LOCAL_ERROR_CODE2, NvError_NandRegionTableOpFailure);
#else
        e = NvError_NandRegionTableOpFailure;
#endif
        goto fail;
    }
    BytesOffset += sizeof(NvU32);
    //  b) NvU32 region entry count(RegCount)
    *pRegCount = (*((NvU32 *)(ptr + BytesOffset)));
    BytesOffset += sizeof(NvU32);

    //  c) Read device interleave count
    hDev->InterleaveCount = (*((NvU32 *)(ptr + BytesOffset)));
    // Update interleave count log
    hDev->Log2InterleaveCount = NandUtilGetLog2(hDev->InterleaveCount);
    BytesOffset += sizeof(NvU32);

    // Get size of region table entry
    RegionEntryByteCount = NAND_MACRO_GET_SIZEOF_REGION_ENTRY;
    //  d) data representing each region table entry up to RegCount entries
    for (i = 0; i < *pRegCount; i++)
    {
        // Check if we have sufficient data left
        if ((BytesOffset + RegionEntryByteCount) > BufSize)
        {
            // FIXME: handle this case by reading more data and
            // interpreting it
#if NAND_MORE_ERR_CODES
            e = NAND_ERR_MASK(LOCAL_ERROR_CODE3,
                NvError_NandRegionTableOpFailure);
#else
            e = NvError_NandRegionTableOpFailure;
#endif
            goto fail;
        }
        if ((hRegEntryNew =
            NvOsAlloc(sizeof(struct NandRegionTabRec))) == NULL)
        {
            e =  NvError_InsufficientMemory;
            goto fail;
        }
        NvOsMemcpy((void *)hRegEntryNew, (void *)(ptr + BytesOffset),
            RegionEntryByteCount);
        hRegEntryNew->Next = NULL;
        if (i == 0)
        {
            if (hDev->hHeadRegionList)
            {
                // Forced region table load found
                NvOsDebugPrintf("\n Possible forced region table load ");
                RegionTableFree(hDev->hHeadRegionList);
            }
            hDev->hHeadRegionList = hRegEntryNew;
            hRegEntry = hRegEntryNew;
        }
        else
        {
            hRegEntry->Next = hRegEntryNew;
            // move the current reg entry pointer to next
            hRegEntry = hRegEntry->Next;
        }
        BytesOffset += RegionEntryByteCount;
    }
    if (i == *pRegCount)
    {
        // Check if we have sufficient data left
        if ((BytesOffset + sizeof(NvU32)) > BufSize)
        {
            // FIXME: handle this case by reading more data and
            // interpreting it
#if NAND_MORE_ERR_CODES
            e = NAND_ERR_MASK(LOCAL_ERROR_CODE4,
                NvError_NandRegionTableOpFailure);
#else
            e = NvError_NandRegionTableOpFailure;
#endif
            goto fail;
        }
        //  e) end region table patten/number
        if ((*((NvU32 *)(ptr + BytesOffset)))
            != s_NandRegTblEndMagicNumber)
        {
#if NAND_MORE_ERR_CODES
        e = NAND_ERR_MASK(LOCAL_ERROR_CODE5, NvError_NandRegionTableOpFailure);
#else
            e = NvError_NandRegionTableOpFailure;
#endif
            goto fail;
        }
        BytesOffset += sizeof(NvU32);
    }
    *pBytesOffset = BytesOffset;
fail:
    return e;
}

static NvError CreateRegionTable(NandBlockDevHandle hNandBlockDev)
{
    NvError e = NvSuccess;
    NandRegionTabHandle hRegEntry = hNandBlockDev->hDev->hHeadRegionList;
    NvU32 RegEntryCount = 0;
    NvU32 RegionEntryByteCount;
    NvU32 TotalRegTableByteCount;
    NvU32 TotalRegTblSectorCount;
    NvU32 ByteOffset;
    NvU8 *ptr;
    NvDdkBlockDevInfo *pDeviceInfo;
    NvU32 j;

    // save region table(region entry link list) for the device
    pDeviceInfo = &(hNandBlockDev->hDev->BlkDevInfo);

    // Get size of the region table data
    while (hRegEntry)
    {
        RegEntryCount++;
        hRegEntry = hRegEntry->Next;
    }
    // Get size of region table entry
    RegionEntryByteCount = NAND_MACRO_GET_SIZEOF_REGION_ENTRY;
    // Initialize a buffer with the region table data
    // Format of Region table:
    //  a) start magic Number s_NandRegTblStartMagicNumber
    //  b) NvU32 region entry count(RegCount)
    //  c) NvU32 Device Interleave setting
    //  d) data representing each region table entry up to RegCount entries
    //  e) end magic Number s_NandRegTblEndMagicNumber
    // Size in bytes of region table
    TotalRegTableByteCount = ((RegEntryCount * RegionEntryByteCount) +
        NAND_REGIONTABLE_HEADER_SIZE + NAND_REGIONTABLE_FOOTER_SIZE);
    // set sectors of region table data
    TotalRegTblSectorCount = MACRO_DIV_POW2_LOG2NUM(TotalRegTableByteCount,
        hNandBlockDev->hDev->Log2BytesPerPage);
    // Ceil the sector count
    if (MACRO_MULT_POW2_LOG2NUM(TotalRegTblSectorCount,
        hNandBlockDev->hDev->Log2BytesPerPage) <
        TotalRegTableByteCount)
        TotalRegTblSectorCount++;
    // Check to make sure region table size falls within 1 block
    if ((TotalRegTblSectorCount > 1) ||
                    (TotalRegTblSectorCount > pDeviceInfo->SectorsPerBlock))
    {
        // Current implementation assumes region table is within sector size
        NAND_BLK_DEV_ERROR_LOG(("\nError: As Region table is bigger than "
            "1 sector size. Need to change Load Region table logic "));
#if NAND_MORE_ERR_CODES
        e = NAND_ERR_MASK(LOCAL_ERROR_CODE6, NvError_NandRegionTableOpFailure);
#else
        e = NvError_NandRegionTableOpFailure;
#endif
        goto fail;
    }
    // Write size is multiple of sector size, buffer allocated in sectors
    s_RegTableGoldenCopy = NvOsAlloc(MACRO_MULT_POW2_LOG2NUM(TotalRegTblSectorCount,
        hNandBlockDev->hDev->Log2BytesPerPage));
    if (!(s_RegTableGoldenCopy))
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    // Initialize buffer created
    NvOsMemset((void *)((s_RegTableGoldenCopy)), 0x0,
        MACRO_MULT_POW2_LOG2NUM(TotalRegTblSectorCount, hNandBlockDev->hDev->Log2BytesPerPage));
    // Allocate buffer to be initialized with region table data
    ptr = (NvU8 *)&(s_RegTableGoldenCopy)[0];
    ByteOffset = 0;
    // initialize start magic number of region table
    *((NvU32 *)ptr) = s_NandRegTblStartMagicNumber;
    ByteOffset += sizeof(NvU32);
    // set the number of partition on device
    *((NvU32 *)(ptr + ByteOffset)) = RegEntryCount;
    ByteOffset += sizeof(NvU32);
    // Save device interleave count as part of region table
    *((NvU32 *)(ptr + ByteOffset)) = hNandBlockDev->hDev->InterleaveCount;
    ByteOffset += sizeof(NvU32);
    hRegEntry = hNandBlockDev->hDev->hHeadRegionList;
    for (j = 0; j < RegEntryCount; j++)
    {
        if (!hRegEntry)
        {
#if NAND_MORE_ERR_CODES
        e = NAND_ERR_MASK(LOCAL_ERROR_CODE7, NvError_NandRegionTableOpFailure);
#else
            e = NvError_NandRegionTableOpFailure;
#endif
            goto fail;
        }
        // Copy the RegProp from region entry
        NvOsMemcpy((void *)(ptr + ByteOffset),
            (void *)(&hRegEntry->RegProp),
            sizeof(NandRegionProperties));
        ByteOffset += sizeof(NandRegionProperties);
        // Copy MinorInstance from region entry
        NvOsMemcpy((void *)(ptr + ByteOffset),
            (void *)(&hRegEntry->MinorInstance),
            sizeof(NvU32));
        ByteOffset += sizeof(NvU32);

        // read the next region entry
        hRegEntry = hRegEntry->Next;
    }
    *((NvU32 *)(ptr + ByteOffset)) = s_NandRegTblEndMagicNumber;
    s_TotalRegTableByteCount = TotalRegTableByteCount;
fail:
    return e;
}
// Function to save the region table created to end of Nand device
// Assumption that region table is within 1 block size
static NvError
NandUtilSaveRegionTable(NandBlockDevHandle hNandBlockDev)
{
    NvError e = NvSuccess;

    NvU32 ActualWriteSectors;
    NvU32 ActualWriteSectorsCp;
    NvU32 PageNumbers[MAX_INTERLEAVED_NAND_CHIPS];
    NvU32 DummyPages[MAX_INTERLEAVED_NAND_CHIPS];

    NvU32 CurrDevNum;
    NvS32 CurrBlockNum;
    NvU32 i1;
    NvU32 k;
    NvU32 SectorAddr;

    NvU8 *pBufRdVerify;
    NvU32 BufSize;
    NvU32 RdSectorCount;
    NandSpareInfo SpareInfo;
    NvBool IsFakeBad;
    NvBool IsGoodBlk;
    NvU32 ActualBlockNum;

    // Allocate a read buffer
    BufSize = MACRO_MULT_POW2_LOG2NUM(sizeof(NvU8), hNandBlockDev->hDev->Log2BytesPerPage);
    pBufRdVerify = NvOsAlloc(BufSize);
    if (!pBufRdVerify)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    // Initialize created array
    NvOsMemset(pBufRdVerify, 0, BufSize);
    NV_CHECK_ERROR_CLEANUP(CreateRegionTable(hNandBlockDev));

    NV_ASSERT(s_TotalRegTableByteCount != 0);
    ActualWriteSectors = MACRO_DIV_POW2_LOG2NUM(s_TotalRegTableByteCount,
        hNandBlockDev->hDev->Log2BytesPerPage);
    // Ceil the sector count
    if (MACRO_MULT_POW2_LOG2NUM(ActualWriteSectors,
                                    hNandBlockDev->hDev->Log2BytesPerPage) <
                                    s_TotalRegTableByteCount)
    {
        ActualWriteSectors++;
    }

    CurrDevNum = s_NandCommonInfo.LastBankChipNum;
    CurrBlockNum = s_NandCommonInfo.ChipBlockNum;

    // we know the current block is good
    IsGoodBlk = NV_TRUE;
    // save multiple redudant copies of the region table for safety
    for (k = 0; k < NAND_REGIONTABLE_REDUNDANT_COPIES; k++)
    {
LabelStartRegionCopy:
        ActualWriteSectorsCp = ActualWriteSectors;
        do {
            NV_CHECK_ERROR_CLEANUP(
                            NandIsGoodBlock(hNandBlockDev,
                            CurrDevNum,
                            CurrBlockNum,
                            &SpareInfo)
                            );
            IsFakeBad = NandUtilIsFakeBad(CurrDevNum, CurrBlockNum);
            IsGoodBlk = (SpareInfo.IsGoodBlock && (!IsFakeBad));
            if (!IsGoodBlk)
            {
                // Try next block
                CurrBlockNum -= 1;
            }
            if (CurrBlockNum < 0)
            {
#if NAND_MORE_ERR_CODES
                e = NAND_ERR_MASK(LOCAL_ERROR_CODE8, NvError_NandRegionTableOpFailure);
#else
                e = NvError_NandRegionTableOpFailure;
#endif
                goto fail;
            }
        } while (!IsGoodBlk);
        SectorAddr = MACRO_MULT_POW2_LOG2NUM(CurrBlockNum,
                            hNandBlockDev->hDev->Log2PagesPerBlock);
        // Initialize the Page numbers first since Ddk needs
        // interleaved chip page numbers
        for (i1 = 0; i1 < MAX_INTERLEAVED_NAND_CHIPS; i1++)
            PageNumbers[i1] = s_NandIllegalAddress;
        PageNumbers[CurrDevNum] = SectorAddr;
        ActualBlockNum = 1;
        // Erase the good block before saving region table
        if ((e = NvDdkNandErase(hNandBlockDev->hDev->hDdkNand, CurrDevNum,
            PageNumbers, &ActualBlockNum)) != NvSuccess)
        {
            NAND_BLK_DEV_ERROR_LOG(("\nUnable to Erase Nand chip=%d,block=%d ",
                CurrDevNum, CurrBlockNum));
            // Block could be bad
            IsGoodBlk = NV_FALSE;
            goto LblMarkBad;
        }
        e = NvDdkNandWrite(hNandBlockDev->hDev->hDdkNand, CurrDevNum,
            PageNumbers, (NvU8 *)(s_RegTableGoldenCopy), NULL, &ActualWriteSectorsCp);
        // FIXME:
        // Temporarily enabling Region table write/read validation by default
        // as verify region table is seen to fail intermittently on AP16
        if ((e == NvSuccess) /*&& (hNandBlockDev->IsReadVerifyWriteEnabled)*/)
        {
            // To ensure that cached data is not returned, read a different
            // page after the write above
            for (i1 = 0; i1 < MAX_INTERLEAVED_NAND_CHIPS; i1++)
                DummyPages[i1] = s_NandIllegalAddress;
            DummyPages[CurrDevNum] = 0;
            RdSectorCount = 1;
            (void)NvDdkNandRead(hNandBlockDev->hDev->hDdkNand, CurrDevNum,
                DummyPages, pBufRdVerify, NULL, &RdSectorCount, NV_FALSE);
            // See DdkRead does not read data properly if this implementation
            // is changed to call function NandReadVerifyDdkWrite.
            for (i1 = 0; i1 < ActualWriteSectorsCp; i1++)
            {
                // Till this point FTL Write API has not been called, so
                // Tag search fails. Hence, Ddk Read API used to verify data
                RdSectorCount = 1;
                e = NvDdkNandRead(hNandBlockDev->hDev->hDdkNand, CurrDevNum,
                    PageNumbers, pBufRdVerify, NULL, &RdSectorCount, NV_FALSE);
                if (e != NvSuccess)
                {
                    // Block could be bad
                    IsGoodBlk = NV_FALSE;
                    goto LblMarkBad;
                }
                if (RdSectorCount == 1)
                {
                    // compare the read sector
                    if (NandUtilMemcmp((void *)((NvU8 *)s_RegTableGoldenCopy +
                        (MACRO_MULT_POW2_LOG2NUM(i1, hNandBlockDev->hDev->Log2BytesPerPage))),
                        (void *)pBufRdVerify, BufSize))
                    {
                        // Block could be bad
                        IsGoodBlk = NV_FALSE;
                        // process the blocks previous to this
                        goto LblMarkBad;
                    }
                }
            }
        }
        else if (e != NvSuccess)
        {
            // Block could be bad
            IsGoodBlk = NV_FALSE;
        }
LblMarkBad:
        if (!IsGoodBlk)
        {
            // Anytime bad blocks marked in region table reset last good block
            s_LastGoodBlock = -1;
            // Mark the region table block as bad
            NV_CHECK_ERROR_CLEANUP(UtilMarkBad(hNandBlockDev, CurrDevNum,
                CurrBlockNum));
            // get next block and try to write this region table copy
            goto LabelStartRegionCopy;
        }
        else
        {
            IsGoodBlk = NV_FALSE;
            NvOsDebugPrintf("\nSave Region Table copy %u at CurrBlockNum %u", k, CurrBlockNum);
            // Try next block
            CurrBlockNum -= 1;
        }
    }
fail:
    if (pBufRdVerify)
        NvOsFree(pBufRdVerify);
    return e;
}

// Function to compute the Minimum number of physical blocks required by FTL
// for bad block management. The value is dependent on FTL policy
static NvU32
GetMinPhysicalBlocksReq(
        NandDevHandle hDev,
        NvU32 MgtPolicy)
{
    NvU32 MinPhysicalBlocksReq =0;
    NvU32 NumOfTTBlocks =0;
    NvU32 NumOfTATBlocks =0;
    NvS32 ReqBlkCount =0;
    NvS32 ReqRowCount =0;

    switch (MgtPolicy)
    {
        case FTL_FULL_POLICY:
            ReqBlkCount = GeneralConfiguration_BLOCKS_FOR_TAT_STORAGE;
            ReqRowCount = (ReqBlkCount / hDev->InterleaveCount);
            ReqRowCount += (ReqBlkCount % hDev->InterleaveCount) ? 1 : 0;
            ReqRowCount = ReqRowCount * hDev->NandDeviceInfo.NumberOfDevices;
            NumOfTATBlocks =
                ReqRowCount * hDev->InterleaveCount;

            ReqBlkCount = GeneralConfiguration_BLOCKS_FOR_TT_STORAGE;
            ReqRowCount = (ReqBlkCount / hDev->InterleaveCount);
            ReqRowCount += (ReqBlkCount % hDev->InterleaveCount) ? 1 : 0;
            ReqRowCount = ReqRowCount * hDev->NandDeviceInfo.NumberOfDevices;
            NumOfTTBlocks =
                ReqRowCount * hDev->NandDeviceInfo.NumberOfDevices;
            MinPhysicalBlocksReq = NumOfTATBlocks + NumOfTTBlocks + (2 + NUMBER_OF_LBAS_TO_TRACK);
            break;
        default:
            NV_ASSERT(!"Unsupported FTL management policy");
    }
    return MinPhysicalBlocksReq;
}

// Utility function to calculate the number of physical blocks in the last partition
static NvError NandUtilGetNumPhysicalBlocks(NandBlockDevHandle hBlockDev,
                                        NvU32 StartPhysicalSuperBlock,
                                        NvU32 *pNumPhysicalBlocks)
{
    NvError e = NvSuccess;
    NvU32 StartPhysicalBlock;
    // block where the last partition ends
    NvU32 LastPartitionEndBlock;
    // account for bad blocks found while saving region table, across all the banks
    NvU32 UnusedRegionTableBlocks;
    NvU32 BadBlockCount = 0;
    NvBool IsGoodBlk;
    NvU32 k;
    NvS32 CurrBlockNum;
    NvU32 CurrDevNum;
    NandSpareInfo SpareInfo;
    NvBool IsFakeBad;

    if (s_LastGoodBlock < 0)
    {
        NV_CHECK_ERROR_CLEANUP(NandUtilGetLastGoodBlock(hBlockDev, &s_LastGoodBlock));
    }
    StartPhysicalBlock = (MACRO_MULT_POW2_LOG2NUM(StartPhysicalSuperBlock,
                                        hBlockDev->hDev->Log2InterleaveCount));
    CurrBlockNum = s_NandCommonInfo.ChipBlockNum;
    CurrDevNum = s_NandCommonInfo.LastBankChipNum;
    if ((s_LastGoodBlock < 0) ||
    ((CurrDevNum == 0) && (CurrBlockNum == 0)))
    {
#if NAND_MORE_ERR_CODES
        e = NAND_ERR_MASK(LOCAL_ERROR_CODE9, NvError_NandRegionTableOpFailure);
#else
        e = NvError_NandRegionTableOpFailure;
#endif
        goto fail;
    }
     for (k = 0; k < (NAND_REGIONTABLE_REDUNDANT_COPIES - 1); k++)
    {
        do
        {
            CurrBlockNum -= 1;
            if (CurrBlockNum < 0)
            {
#if NAND_MORE_ERR_CODES
                e = NAND_ERR_MASK(LOCAL_ERROR_CODE10, NvError_NandRegionTableOpFailure);
#else
                e = NvError_NandRegionTableOpFailure;
#endif
                goto fail;
            }
            NV_CHECK_ERROR_CLEANUP(
                            NandIsGoodBlock(hBlockDev,
                            CurrDevNum,
                            CurrBlockNum,
                            &SpareInfo)
                            );
            IsFakeBad = NandUtilIsFakeBad(CurrDevNum, CurrBlockNum);
            IsGoodBlk = (SpareInfo.IsGoodBlock && (!IsFakeBad));

            if (!IsGoodBlk)
            {
                BadBlockCount ++;
            }
        }while (!IsGoodBlk);
    }

    LastPartitionEndBlock = s_LastGoodBlock -
                                        NAND_REGIONTABLE_REDUNDANT_COPIES -
                                        BadBlockCount;
    UnusedRegionTableBlocks = (((hBlockDev->hDev->NandDeviceInfo.NoOfBlocks -
                                    s_NandCommonInfo.ChipBlockNum)
                                    + (NAND_REGIONTABLE_REDUNDANT_COPIES - 1)
                                    + BadBlockCount) *
                                    ((hBlockDev->hDev->InterleaveCount <= 1) ? 0 : (hBlockDev->hDev->InterleaveCount - 1)));
    // Deduct the blocks before this partition and bad blocks count before
    // region table block from all previous banks if any
    *pNumPhysicalBlocks =  (LastPartitionEndBlock + 1) -
                                    StartPhysicalBlock -
                                    UnusedRegionTableBlocks;
    // Get super block count - any partial super block in end is discarded
    *pNumPhysicalBlocks >>= hBlockDev->hDev->Log2InterleaveCount;

fail:
    return e;
}

// Utility to initialize the physical block count for the end partition
static NvError
NandUtilSetEndPartPhysicalBLockCount(NandBlockDevHandle hBlockDev)
{
    NvError e = NvSuccess;
    NandRegionTabHandle hRegEntry;

    hRegEntry = hBlockDev->hDev->hHeadRegionList;
    while (hRegEntry->Next)
        hRegEntry = hRegEntry->Next;
    // found the last partition
    if (hRegEntry->RegProp.TotalPhysicalBlocks ==
        NAND_PART_SIZE_ALLOC_REMAINING)
    {
        e = NandUtilGetNumPhysicalBlocks(hBlockDev,
                                    hRegEntry->RegProp.StartPhysicalBlock,
                                    &(hRegEntry->RegProp.TotalPhysicalBlocks));
    }
    return e;
}

// Function to read Nand device and populate the region table
// Assumption that region table is within 1 block size
static NvError
NandUtilLoadRegionTable(NandBlockDevHandle hBlockDev)
{
    NvError e = NvSuccess;
    NvU32 PageNumbers[MAX_INTERLEAVED_NAND_CHIPS];
    NvU32 ActualReadSectors;
    NvU8 *Buf = NULL;
    NvDdkBlockDevInfo *pDeviceInfo;
    NvU32 RegCount;
    NvU32 RegEntrySizeInBytes;
    NvU32 BytesOffset;
    NvU32 i1;
    NvS32 CurrBlockNum;
    NvU32 CurrDevNum;
    NvU32 k;
    NandSpareInfo SpareInfo;
    NvBool IsFakeBad;
    NvBool IsGoodBlk;

    pDeviceInfo = &(hBlockDev->hDev->BlkDevInfo);
    Buf = NvOsAlloc(pDeviceInfo->BytesPerSector);
    if (!Buf)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    // Initialize buffer created
    NvOsMemset(Buf, 0, pDeviceInfo->BytesPerSector);

    if (s_LastGoodBlock < 0)
    {
        NV_CHECK_ERROR_CLEANUP(NandUtilGetLastGoodBlock(hBlockDev, &s_LastGoodBlock));
    }
    CurrDevNum = s_NandCommonInfo.LastBankChipNum;
    CurrBlockNum = s_NandCommonInfo.ChipBlockNum;
    if ((s_LastGoodBlock < 0) ||
        ((CurrDevNum == 0) && (CurrBlockNum == 0)))
    {
#if NAND_MORE_ERR_CODES
        e = NAND_ERR_MASK(LOCAL_ERROR_CODE11, NvError_NandRegionTableOpFailure);
#else
        e = NvError_NandRegionTableOpFailure;
#endif
        goto fail;
    }
    // start with the assumption that the current block is good
    IsGoodBlk = NV_TRUE;
    for (k =0; k < NAND_REGIONTABLE_REDUNDANT_COPIES; k++)
    {
        // Try to read 1 sector only
        ActualReadSectors = DEFAULT_REGION_TBL_SIZE_TO_READ;
        while (!IsGoodBlk)
        {
            CurrBlockNum -= 1;
            if (CurrBlockNum < 0)
            {
#if NAND_MORE_ERR_CODES
                e = NAND_ERR_MASK(LOCAL_ERROR_CODE12, NvError_NandRegionTableOpFailure);
#else
                e = NvError_NandRegionTableOpFailure;
#endif
                goto fail;
            }
            NV_CHECK_ERROR_CLEANUP(
                            NandIsGoodBlock(hBlockDev,
                            CurrDevNum,
                            CurrBlockNum,
                            &SpareInfo)
                            );
            IsFakeBad = NandUtilIsFakeBad(CurrDevNum, CurrBlockNum);
            IsGoodBlk = (SpareInfo.IsGoodBlock && (!IsFakeBad));
        }
        // Initialize the Page numbers first since Ddk needs
        // interleaved chip page numbers
        for (i1 = 0; i1 < MAX_INTERLEAVED_NAND_CHIPS; i1++)
            PageNumbers[i1] = s_NandIllegalAddress;
        PageNumbers[CurrDevNum] =
            MACRO_MULT_POW2_LOG2NUM(CurrBlockNum,
            hBlockDev->hDev->Log2PagesPerBlock);
        e = NvDdkNandRead(hBlockDev->hDev->hDdkNand, CurrDevNum, PageNumbers,
            Buf, NULL, &ActualReadSectors, NV_FALSE);
        if (e == NvSuccess)
        {
            // Try to interpret region table data, if data is not
            // correct, do not proceed further
            e = NandUtilInterpretRegTblData(
                hBlockDev->hDev, Buf, pDeviceInfo->BytesPerSector, &RegCount,
                &RegEntrySizeInBytes, &BytesOffset);
            if (e == NvSuccess)
            {
                // Successfully interpreted region table data - finish
                // Overwrite the physical block count for last partition
                NV_CHECK_ERROR_CLEANUP(
                            NandUtilSetEndPartPhysicalBLockCount(hBlockDev));
                break;
            }
            else
            {
                NvOsDebugPrintf("\nRegion Table copy at CurrBlockNum %u is probably corrupt", CurrBlockNum);
            }
        }
        IsGoodBlk = NV_FALSE;
        // In case of read failure continue other block read
    }
fail:
    // Only for OS bootup dump bad block list
    // Print bad block table for error
    if (e != NvSuccess)
    {
        // Dump the bad block table during bootup failure
        NandUtilDumpBad(hBlockDev);
    }
    if (Buf)
        NvOsFree(Buf);
    return e;
}

// Local function that extracts interleave count field from
// partition attribute
static void
NandUtilMapPartitionAttrib2InterleaveArgForFTL(
    NvU32 PartitionAttrib, NvU32 *pInterleaveCount,
    NvBool *pIsInterleaveEnabled)
{
    if (PartitionAttrib & NVDDK_NAND_BLOCKDEV_INTERLEAVE_ENABLE_FIELD)
        *pIsInterleaveEnabled = NV_TRUE;
    else
        *pIsInterleaveEnabled = NV_FALSE;
    switch (PartitionAttrib & NVDDK_NAND_BLOCKDEV_INTERLEAVE_FACTOR_FIELD)
    {
        case NVDDK_NAND_BLOCKDEV_INTERLEAVE_FACTOR_2:
            *pInterleaveCount = NAND_INTERLEAVE_COUNT_2;
            break;
        case NVDDK_NAND_BLOCKDEV_INTERLEAVE_FACTOR_4:
            *pInterleaveCount = NAND_INTERLEAVE_COUNT_4;
            break;
        case NVDDK_NAND_BLOCKDEV_INTERLEAVE_FACTOR_1:
        default:
            *pInterleaveCount = NAND_INTERLEAVE_COUNT_1;
            break;
    }
}

// This function looks returns one physical block number for block that is
// good block. It also ensures that the block does not fall in the reserved
// area kept for region table at end of device
static NvError NandUtilGetNextPhysicalBlock(
    NandBlockDevHandle hNandBlockDev,
    NvU32 ChipNumber, NvU32 CurrBlock,
    NvU32 *pCurrGoodPhysicalBlocks, NvBool *pAbortScan)
{
    // Saved physical block number we try to access but may be bad and hence
    // end up accessing subsequent blocks
    NvU32 SavedChipPhysicalBlock;
    NvU32 ActualPhysicalBlock;
    //NvError Err;
    //NvU32 MaxBlock;
    NvS32 TempValue = 0;
    NvError e = NvSuccess;

    // Next block to check is one more than block number
    // stored in array CurrGoodPhysicalBlocks
    ActualPhysicalBlock = CurrBlock;
    // Get chip number for the physical address
    if ((NvBtlGetPba(&ChipNumber, &ActualPhysicalBlock) == NV_FALSE) ||
        (ChipNumber >= hNandBlockDev->hDev->NandDeviceInfo.NumberOfDevices))
    {
        return NvError_InvalidAddress;
    }
    SavedChipPhysicalBlock = ActualPhysicalBlock;
    if (ChipNumber == (NvU32)(hNandBlockDev->hDev->NandDeviceInfo.NumberOfDevices - 1))
    {
        if ((s_NandCommonInfo.ChipBlockNum == 0) &&
            (s_NandCommonInfo.LastBankChipNum == 0))
        {
            // Initialize the values by calling function
            NV_CHECK_ERROR_CLEANUP(NandUtilGetLastGoodBlock(hNandBlockDev, &TempValue));
            NV_ASSERT (TempValue !=0);
        }
    }
#if 0
    // call function to calculate maximum block number allowed
    MaxBlock = NandUtilGetMaxBlockOnChip(hNandBlockDev);
    // Allocate requested block rather than allocating requested
    // good block.
    // Ensure current block is good else find next good physical block
    Err = NandUtilGetGoodPhysicalBlockOnChip(hNandBlockDev,
        ChipNumber, MaxBlock, &ActualPhysicalBlock);
    if ((Err != NvSuccess) || ((s_NandCommonInfo.LastBankChipNum == ChipNumber) &&
        (s_NandCommonInfo.ChipBlockNum <= ActualPhysicalBlock)))
    {
        // Flag error in case of partition that are not last one
        return NvError_NandRegionIllegalAddress;
    }
#endif
    // The good block is off by the difference being added
    CurrBlock += (ActualPhysicalBlock - SavedChipPhysicalBlock);
    // Save the current good block
    *pCurrGoodPhysicalBlocks = CurrBlock;
    return NvSuccess;
fail:
    return e;
}

// Get maximum value in array among up to Count elements
static NvU32
NandUtilGetMaxInArray(NvU32 *ArrBlk, NvU32 Count)
{
    NvU32 TmpMax;
    NvU32 i;
    TmpMax = ArrBlk[0];
    // Start from 2nd chip i.e. skip chip0
    for (i = 1; i < Count; i++)
    {
        // Update Maximum of end physical block number among all the chips
        if (ArrBlk[i] > TmpMax)
            TmpMax = ArrBlk[i];
    }
    return TmpMax;
}

//  Utility to update the super block count and current good block
// for each interleave column. This is called after we run out
// of blocks for an interleave column. At this point all interleave
// columns less than the exhausted interleave column are updated
// with last good block number
// Assumes that CurrGoodBlock and LastGoodBlocks are passed as argument
static void
NandUtilUpdateLastGoodBlock(NvU32 AbortInterleaveColumn,
    NvU32 *LastGoodPhysicalBlocks,
    NvU32 *CurrGoodPhysicalBlocks, NvU32 *pTempBlkCount)
{
    NvU32 k;
    // Iterate over all interleave columns before the column
    // in which blocks are exhausted
    for (k = 0; k < AbortInterleaveColumn; k++)
    {
        // Last good block must not be illegal value 0xFFFFFFFF
        if ((LastGoodPhysicalBlocks[k] != 0xFFFFFFFF) &&
            (*pTempBlkCount > 0))
        {
            // Initialize the last good physical block number to
            // previous good block
            CurrGoodPhysicalBlocks[k] = LastGoodPhysicalBlocks[k];
            // Super block count in TempBlkCount updated
            if (k == 0)
            {
                // Reduce good block count, -- has higher precedence than *
                *pTempBlkCount -= 1;
            }
        }
    }
}

// Function to return the end physical address given the
// start physical address in array CurrGoodPhysicalBlocks.
// and number of logical blocks in the partition
// Multiple interleave columns supported.
static NvError
NandUtilGetPhysicalEnd(
    NandBlockDevHandle hNandBlockDev,
    NvU32 *CurrGoodPhysicalBlocks,
    NvU32 NumLogicalBlocks, NvU32 InterleaveCount,
    NvU32 *pBadBlocks,
    NvU32 *pLogicalActual)
{
    NvU32 TempBlkCount;
    NvU32 i;
    NvBool AbortScan = NV_FALSE;
    NvU32 ChipNumber = MAX_INTERLEAVED_NAND_CHIPS;
    NvU32 CurrBlock;
    NvU32 BadBlockCount = 0;
    NvError e = NvSuccess;
    NvU32 LastGoodPhysicalBlocks[MAX_INTERLEAVED_NAND_CHIPS];
    NvU32 MaxBlock;

    // Initialize LastGoodPhysicalBlocks to 0xFFFFFFFF
    NvOsMemset(LastGoodPhysicalBlocks, 0xFF,
        MACRO_MULT_POW2_LOG2NUM(sizeof(NvU32), LOG2MAX_INTERLEAVED_NAND_CHIPS));
     // TempBlkCount tracks the number of logical blocks found - initialize it
    TempBlkCount = *pLogicalActual;
    // call function to calculate maximum block number allowed
    MaxBlock = NandUtilGetMaxBlockOnChip(hNandBlockDev);
    while ((TempBlkCount < NumLogicalBlocks) && (AbortScan == NV_FALSE))
    {
        // Process all chips in case of interleave enabled
        // Find the good physical block address for each chip, return the
        // highest physical address of chips after counting the desired
        // number of good blocks
        for (i = 0; i < InterleaveCount; i++)
        {
            ChipNumber = i;
            // First Block on ChipNumber for a partition
            CurrBlock = CurrGoodPhysicalBlocks[i];
            // if not first blocks in partition
            //      block number to check is one after last good block
            if ((TempBlkCount > 1) ||
                ((TempBlkCount == 1) && (i == 0)))
            {
                // This block is not first in partition for the interleave col
                // Good block must not cross allowed block limit
                // Next block to process within maximum block number allowed
                if ((CurrBlock + 1) < MaxBlock)
                {
                    // Super Block  number incremented
                    // once for InterleaveCount blocks
                    CurrBlock++;
                }
                else
                {
                    // Reached end of Nand
                    AbortScan = NV_TRUE;
                    // We have run out of blocks in one interleave column so
                    // last good block of all previous interleave columns
                    // discarded and need to use the previous good block
                    //
                    // This is done once before function exit

                    // Scanned till end of Nand and stop
                    break;
                }
            }
            // Save the curr good physical block before change
            LastGoodPhysicalBlocks[i] = CurrGoodPhysicalBlocks[i];
            // Function to return good physical block address that
            // does not overlap reserved region table
            NV_CHECK_ERROR_CLEANUP(NandUtilGetNextPhysicalBlock(hNandBlockDev,
                ChipNumber, CurrBlock,
                &CurrGoodPhysicalBlocks[i], &AbortScan));
            // Extra sanity check for physical address - cannot exceed
            // maximum block number allowed
            if ((AbortScan == NV_FALSE) && (CurrGoodPhysicalBlocks[i] >
                MaxBlock))
                NV_ASSERT(NV_FALSE);
            // Accumulate the bad block count as difference between
            // logical super block and actual physical block on
            // each interleave column
            BadBlockCount += (CurrGoodPhysicalBlocks[i] - CurrBlock);
            // Cannot process this block as it overlaps with region table
            if (AbortScan == NV_TRUE)
            {
                // Need to remove good blocks in previous interleave column
                // found in this iteration of while ... (TempBlkCount < NumLogicalBlocks)
                //
                // This is done once before function exit

                // Scanned till end of Nand and stop
                break;
            }
            // Super block count in TempBlkCount updated
            if (i == 0)
            {
                // TempBlkCount tracks the number of good logical blocks found
                TempBlkCount++;
            }
            // In order to support interleaving we cannot stop here
            // even if required number of blocks have been found
            // We make sure that in all interleave columns we have good blocks
        }
    }
fail:
    // If reached here due to Aborted search for good block, we
    // may need to remove found good blocks in previous column
    if ((AbortScan == NV_TRUE) && (ChipNumber < MAX_INTERLEAVED_NAND_CHIPS))
    {
        // Need to use the previous good block
        NandUtilUpdateLastGoodBlock(ChipNumber, LastGoodPhysicalBlocks,
            CurrGoodPhysicalBlocks, &TempBlkCount);
        // Not an error when we have exhausted blocks on Chip
        e = NvSuccess;
    }
    // return the end physical address
    *pLogicalActual = TempBlkCount;
    // bad block count returned
    *pBadBlocks = BadBlockCount;
    return e;
}

// Function to calculate end physical address
// replacement blocks requested already considered in NumLogicalBlocks
// It starts from the StartPhysical block assumes that start physical block
// is on Chip==0, and checks for good block on all chips up to interleave count.
// This information is stored in an array that keeps track of the last good
// block for a particular Chip number. Once it has counted as many good blocks
// requested for partition creation, it stops. Gets the maximum block number
// found on all chips(as the end physical block) and
// returns number of physical block.
static NvError
NandUtilCalcNumberOfPhysicalBlocks(
    NandBlockDevHandle hNandBlockDev,
    NvU32 StartPhysicalBlock,
    NvU32 InterleaveCount,
    NvU32 *pLogicalBlocks,
    NvU32 PercentReserved,
    NvU32 *pNumPhysicalBlocks,
    NvU32 IsRelativeAlloc)
{
    NvU32 TempBlkCount;
    NvU32 NumLogicalBlocks;
    NvU32 TempPhysicalAddressEnd;
    // Last physical block represents block number across
    // multiple chip accessed sequentially
    NvU32 CurrGoodPhysicalBlocks[MAX_INTERLEAVED_NAND_CHIPS];
    NvU32 i;
    NvU32 SpareBlockCount;
    NvError e = NvSuccess;
    NvU32 PartBadBlockCount = 0;


    // Initialize array of last found good physical block
    NvOsMemset(CurrGoodPhysicalBlocks, 0,
        (sizeof(NvU32) * MAX_INTERLEAVED_NAND_CHIPS));
    // Interleave count used to calculate super block count in partition
    NumLogicalBlocks = (*pLogicalBlocks);
    // relative partition allocation only uses percent reserved parameter
    if (IsRelativeAlloc)
    {
        // spare block calculation should consider the interleave count
        SpareBlockCount = NandUtilGetReplacementBlocks(NumLogicalBlocks,
            PercentReserved);
        // Get spare block count given the PercentReserved blocks
        NumLogicalBlocks += SpareBlockCount;
    }
    // Initialize array containing last good physical block address for a chip
    // with start block
    for (i = 0; i < InterleaveCount; i++)
        CurrGoodPhysicalBlocks[i] = StartPhysicalBlock;
    // initialize block count - using super block count
    TempBlkCount = 0;
    // Call function to return end physical address
    NV_CHECK_ERROR_CLEANUP(NandUtilGetPhysicalEnd(
        hNandBlockDev,
        CurrGoodPhysicalBlocks,
        NumLogicalBlocks, InterleaveCount,
        &PartBadBlockCount,
        &TempBlkCount));
    if ((TempBlkCount == 0) || (TempBlkCount < NumLogicalBlocks))
    {
        NvOsDebugPrintf("\r\nError: Unable to find requested blocks on Nand: "
            "req=%d,found=%d ", NumLogicalBlocks, TempBlkCount);
        // No block allocated, so return error
        return NvError_NandRegionIllegalAddress;
    }

    // Get highest physical block index
    TempPhysicalAddressEnd = NandUtilGetMaxInArray(CurrGoodPhysicalBlocks,
        InterleaveCount);
    // Update the device info on current physical block as
    // the next block from returned end physical block
    if ((TempPhysicalAddressEnd + 1) >
        hNandBlockDev->hDev->PhysicalAddressStart)
    {
        hNandBlockDev->hDev->PhysicalAddressStart = (TempPhysicalAddressEnd + 1);
    }
    // Calculate number of physical blocks
    *pNumPhysicalBlocks = (TempPhysicalAddressEnd - StartPhysicalBlock) + 1;
fail:
    return e;
}

// Function to validate new partition arguments
// New partition should not overlap with existing partitions
static NvBool
NandUtilIsPartitionArgsValid(
    NandBlockDevHandle hNandBlockDev,
    NvU32 StartLogicalBlock, NvU32 EndLogicalReplacementBlock,
    NvU32 StartPhysicalBlock, NvU32 EndPhysicalReplacementBlock)
{
    NandRegionTabHandle hCurrRegion;

    hCurrRegion = hNandBlockDev->hDev->hHeadRegionList;
    while (hCurrRegion)
    {
        // Check that there is no overlap in existing partition
        // with new partition
        if (((EndLogicalReplacementBlock <=
            hCurrRegion->RegProp.StartLogicalBlock)
            /* logical address of new partition finishes before start of
            compared partition */ ||
            (StartLogicalBlock >= (hCurrRegion->RegProp.StartLogicalBlock +
            (hCurrRegion->RegProp.TotalLogicalBlocks))) /* logical address
            new partition starts next to or after compared partition */) &&
            ((EndPhysicalReplacementBlock <=
            hCurrRegion->RegProp.StartPhysicalBlock) /* physical address
            of new partition finishes before start of compared partition */ ||
            (StartPhysicalBlock >= (hCurrRegion->RegProp.StartPhysicalBlock +
            (hCurrRegion->RegProp.TotalPhysicalBlocks))) /* physical address
            new partition starts next to or after compared partition */))
        {
            hCurrRegion = hCurrRegion->Next;
            continue;
        }
        else
            return NV_FALSE;
    }

    return NV_TRUE;
}

// Function to update physical block count in previous region table entry
static void
NandUtilUpdatePrevAbsPartitionEnd(NandBlockDevHandle hNandBlockDev,
    NvU32 StartPhysicalBlock)
{
    NandRegionTabHandle hReg;
   // Find last region table link-list entry
    hReg = hNandBlockDev->hDev->hHeadRegionList;
    if (hReg)
    {
        while (hReg->Next)
        {
            hReg = hReg->Next;
        }
        // End partition details are filled for the previous partition
        // physical block count set
        hReg->RegProp.TotalPhysicalBlocks =
            (StartPhysicalBlock - hReg->RegProp.StartPhysicalBlock);
    }
}

// Nand local allocate partition function definition
static NvError
NandUtilAllocPart(
    NandBlockDevHandle hNandBlockDev,
    NvDdkBlockDevIoctl_AllocatePartitionInputArgs *pAllocPartIn,
    NvDdkBlockDevIoctl_AllocatePartitionOutputArgs *pAllocPartOut)
{
    NandRegionTabHandle hReg;
    NandRegionTabHandle pEntry;
    NvU32 StartLogicalBlock;
    NvU32 NumLogicalBlocks;
    NvU32 StartPhysicalBlock;
    NvU32 NumPhysicalBlocks;
    NvU32 InterleaveCount;
    NvError e = NvSuccess;
    NvU32 StartSector;
    NvU32 NumInterleavedLogicalBlocks;
    NvBool IsInterleaveEnabled;
    NvU32 Log2InterleaveCount;
    NvBool IsRelativeAlloc;

    NvU32 MinPhysicalBlocksReq;


    // Validate partition address and size alignments
    if (pAllocPartIn->AllocationType ==
        NvDdkBlockDevPartitionAllocationType_Relative)
    {
        // The block for StartSector number is on Chip0
        StartSector = MACRO_MULT_POW2_LOG2NUM(
            hNandBlockDev->hDev->LogicalAddressStart,
            (hNandBlockDev->hDev->Log2PagesPerBlock +
            hNandBlockDev->hDev->Log2InterleaveCount));
    }
    else
    {
        // The block for StartSector number is on Chip0
        // When partitions EXCEPT BCT and EBT are interleaved
        // calculation of StartPhysicalSectorAddress in .cfg file
        // must be correct.
        // Actual physical address will be less when interleave is enabled
        StartSector = pAllocPartIn->StartPhysicalSectorAddress;
        // Start sector must be super block aligned
        if (MACRO_MOD_LOG2NUM(StartSector,
            (hNandBlockDev->hDev->Log2PagesPerBlock +
            hNandBlockDev->hDev->Log2InterleaveCount)))
        {
            e = NvError_NandRegionIllegalAddress;
            goto fail;
        }
    }

    // Set logical start block before bad block check
    StartLogicalBlock = hNandBlockDev->hDev->LogicalAddressStart;
    // Get start physical block number
    // Partition starts from super-block aligned addresses hence chip 0
    if (pAllocPartIn->AllocationType ==
        NvDdkBlockDevPartitionAllocationType_Relative)
    {
        // Update tracked physical address
        if ((!hNandBlockDev->hDev->PhysicalAddressStart) ||
            (StartLogicalBlock >
            (MACRO_MULT_POW2_LOG2NUM(hNandBlockDev->hDev->PhysicalAddressStart,
            hNandBlockDev->hDev->Log2InterleaveCount))))
        {
            hNandBlockDev->hDev->PhysicalAddressStart =
                (MACRO_DIV_POW2_LOG2NUM(StartLogicalBlock,
                hNandBlockDev->hDev->Log2InterleaveCount));
            // Ceil to next physical block
            if (MACRO_MOD_LOG2NUM(StartLogicalBlock,
                hNandBlockDev->hDev->Log2InterleaveCount) != 0)
                hNandBlockDev->hDev->PhysicalAddressStart++;
        }
        StartPhysicalBlock = hNandBlockDev->hDev->PhysicalAddressStart;
    }
    else // absolute allocation
    {
        StartPhysicalBlock = MACRO_DIV_POW2_LOG2NUM(
            pAllocPartIn->StartPhysicalSectorAddress,
            (hNandBlockDev->hDev->Log2PagesPerBlock +
            hNandBlockDev->hDev->Log2InterleaveCount));
        // Update physical blocks in previous partition if any
        NandUtilUpdatePrevAbsPartitionEnd(hNandBlockDev, StartPhysicalBlock);
        // Check if new partition start is located within existing partitions
        if (NandUtilIsPartitionArgsValid(hNandBlockDev,
            StartLogicalBlock, StartLogicalBlock,
            StartPhysicalBlock, StartPhysicalBlock) == NV_FALSE)
        {
            // Partition start is within existing partitions
            return NvError_NandRegionIllegalAddress;
        }
        // Update tracked physical address
        if (StartPhysicalBlock > hNandBlockDev->hDev->PhysicalAddressStart)
            hNandBlockDev->hDev->PhysicalAddressStart = StartPhysicalBlock;
    }
    // initialize interleave count in region properties
    NandUtilMapPartitionAttrib2InterleaveArgForFTL(
        pAllocPartIn->PartitionAttribute,
        &InterleaveCount, &IsInterleaveEnabled);
    // Make sure interleave count requested is not more than what can
    // be supported by Ddk
    if ((InterleaveCount != hNandBlockDev->hDev->InterleaveCount) &&
        (hNandBlockDev->hDev->InterleaveCount > NAND_INTERLEAVE_COUNT_1) &&
        (InterleaveCount > NAND_INTERLEAVE_COUNT_1))
    {
        // 2nd interleaved partition on a device cannot have different
        // interleave count than interleave count already chosen
        e = NvError_NandBlockDriverMultiInterleave;
        goto fail;
    }
    // sanity check - Interleave count not more than number of devices
    if ((hNandBlockDev->hDev->NandDeviceInfo.NumberOfDevices >=
        NAND_INTERLEAVE_COUNT_1) &&
        (InterleaveCount > hNandBlockDev->hDev->NandDeviceInfo.NumberOfDevices))
    {
        InterleaveCount = NandUtilGetMaxInterleaveForCScount(
            hNandBlockDev->hDev->NandDeviceInfo.NumberOfDevices);
    }
    // If interleaved access is allowed for a partition, the region table is
    // is stored in interleaved mode
    if ((InterleaveCount <= hNandBlockDev->hDev->NandDeviceInfo.NumberOfDevices) &&
        (InterleaveCount > hNandBlockDev->hDev->InterleaveCount))
    {
        // Requested interleave count cannot be more than the Ddk recognized
        // number of devices
        hNandBlockDev->hDev->InterleaveCount = InterleaveCount;
        // update interleave count log figure
        hNandBlockDev->hDev->Log2InterleaveCount =
            NandUtilGetLog2(hNandBlockDev->hDev->InterleaveCount);
        // The function pointers are unchanged from previous values
        // Whenever global interleave count changes we call Btl Init API
        NvBtlInit(hNandBlockDev->hDev->hDdkNand,
            &(hNandBlockDev->hDev->NandDdkFuncs), InterleaveCount);
    }
    // Set the effective interleave count for this partition
    InterleaveCount = IsInterleaveEnabled ?
        InterleaveCount : NAND_INTERLEAVE_COUNT_1;
    Log2InterleaveCount = NandUtilGetLog2(InterleaveCount);

    IsRelativeAlloc = (pAllocPartIn->AllocationType ==
        NvDdkBlockDevPartitionAllocationType_Relative);
    // Check for NumLogicalSectors == -1 and its handling
    if (pAllocPartIn->NumLogicalSectors == NAND_PART_SIZE_ALLOC_REMAINING)
    {
        // Case to handle end partition that spans till end of device
        NumLogicalBlocks = pAllocPartIn->NumLogicalSectors;
        // logical block count kept unchanged
        NumInterleavedLogicalBlocks = NumLogicalBlocks;
        // Function to scan for last good block - Region table stored here.
        // block index is returned , chip number and block number on chip
        // is also initiailzed and stored in static variable s_NandCommonInfo
        NV_CHECK_ERROR_CLEANUP(NandUtilGetLastGoodBlock(hNandBlockDev,
            &s_LastGoodBlock));
        // number of blocks in this relative partition
        NV_CHECK_ERROR_CLEANUP(
                    NandUtilGetNumPhysicalBlocks(hNandBlockDev,
                                    hNandBlockDev->hDev->PhysicalAddressStart,
                                                        &NumPhysicalBlocks));
        NAND_BLK_DEV_ERROR_LOG(("\nPartition %d - number of physical blocks = %d ",
            pAllocPartIn->PartitionId, NumPhysicalBlocks));
        // assume best case - logical blocks count same as physical block count
        // for absolute allocation tell PT table this best case figure
        // actual block count less for last partition spanning till device end
        NumInterleavedLogicalBlocks = NumPhysicalBlocks;
    }
    else
    {
        // NumLogicalBlocks is block count without replacement blocks
        NumLogicalBlocks = MACRO_DIV_POW2_LOG2NUM(
            pAllocPartIn->NumLogicalSectors,
            hNandBlockDev->hDev->Log2PagesPerBlock);
        //Ceil logical blocks
        if (MACRO_MULT_POW2_LOG2NUM(NumLogicalBlocks,
            hNandBlockDev->hDev->Log2PagesPerBlock) <
            pAllocPartIn->NumLogicalSectors)
            NumLogicalBlocks += 1;
        // super block count
        NumInterleavedLogicalBlocks = MACRO_DIV_POW2_LOG2NUM(NumLogicalBlocks,
            Log2InterleaveCount);
        if (MACRO_MOD_LOG2NUM(NumLogicalBlocks, Log2InterleaveCount) != 0)
            NumInterleavedLogicalBlocks++;
        // This function handles end partition calculation based on
        // good blocks found till end of device
        if ((e = NandUtilCalcNumberOfPhysicalBlocks(hNandBlockDev,
            StartPhysicalBlock, InterleaveCount, &NumInterleavedLogicalBlocks,
            pAllocPartIn->PercentReservedBlocks,
            &NumPhysicalBlocks, IsRelativeAlloc)) != NvSuccess)
        {
            goto fail;
        }
        // validate the new partition args against existing partitions
        if (NandUtilIsPartitionArgsValid(hNandBlockDev,
            StartLogicalBlock, (StartLogicalBlock + NumInterleavedLogicalBlocks),
            StartPhysicalBlock, (StartPhysicalBlock + NumPhysicalBlocks))
            == NV_FALSE)
        {
            e = NvError_BlockDriverOverlappedPartition;
            goto fail;
        }
        // Update the logical start address for next partition
        hNandBlockDev->hDev->LogicalAddressStart += NumInterleavedLogicalBlocks;
    }

    // Partitions must be aligned to super block boundaries
    // Start of first block is aligned to DeviceNum 0 block 0
    // NumInterleavedLogicalBlocks value is such that number
    // of blocks is super block aligned

   // Find last region table link-list entry
    hReg = hNandBlockDev->hDev->hHeadRegionList;
    if (hReg)
    {
        while (hReg->Next)
        {
            hReg = hReg->Next;
        }
    }
    // allocate a new region table entry and initialize
    // with the partition details
    pEntry = NvOsAlloc(sizeof(struct NandRegionTabRec));
    if (!pEntry)
        return NvError_InsufficientMemory;
    // Initialize allocated region table entry
    NvOsMemset(pEntry, 0x0, (sizeof(struct NandRegionTabRec)));
    pEntry->Next = NULL;
    pEntry->MinorInstance = pAllocPartIn->PartitionId;
    // Initialize pEntry->RegProp
    // Initialize argument Region Properties to be passed to FTL
    // FIXME: Need to return this start logical sector address
    // available in pEntry->RegProp.StartLogicalBlock *
    // BlkDevInfo.SectorsPerBlock
    pEntry->RegProp.StartLogicalBlock = StartLogicalBlock;
    pEntry->RegProp.InterleaveBankCount = InterleaveCount;
    // initialize management policy in region properties
    pEntry->RegProp.MgmtPolicy = ((pAllocPartIn->PartitionAttribute &
        NVDDK_NAND_BLOCKDEV_MGMT_POLICY_FIELD) ==
        NVDDK_NAND_BLOCKDEV_MGMT_POLICY_FTL)? FTL_FULL_POLICY :
        (((pAllocPartIn->PartitionAttribute &
        NVDDK_NAND_BLOCKDEV_MGMT_POLICY_FIELD) ==
        NVDDK_NAND_BLOCKDEV_MGMT_POLICY_FTL_LITE)? FTL_LITE_POLICY :
        EXT_FTL_POLICY);

    pEntry->RegProp.StartPhysicalBlock = StartPhysicalBlock;
    if (pEntry->RegProp.MgmtPolicy == FTL_FULL_POLICY)
    {
        if (!pAllocPartIn->PercentReservedBlocks)
        {
            NV_ASSERT(pAllocPartIn->PercentReservedBlocks != 0);
            e = NvError_BadParameter;
            NvOsFree(pEntry);
            pEntry = NULL;
            goto fail;
        }

        if (pAllocPartIn->PercentReservedBlocks > FTL_DATA_PERCENT_RSVD_MAX)
        {
            NvOsDebugPrintf("\nInvalid value for PercentReserved = %d [should not exceed]"
                 "%d, setting PercentReserved = %d\n",pAllocPartIn->PercentReservedBlocks,
                FTL_DATA_PERCENT_RSVD_MAX, FTL_DATA_PERCENT_RSVD_MAX);
                // if percent reserved value exceeds max limit cap it to max limit
                pAllocPartIn->PercentReservedBlocks = FTL_DATA_PERCENT_RSVD_MAX;
        }
        if (pAllocPartIn->NumLogicalSectors == NAND_PART_SIZE_ALLOC_REMAINING)
        {
            pEntry->RegProp.TotalPhysicalBlocks = NAND_PART_SIZE_ALLOC_REMAINING;
        }
        else
        {
            // Takes care of fixed size FTL full partition
            // Calculate end physical address by checking for good blocks and
            // adding replacement blocks requested
            pEntry->RegProp.TotalPhysicalBlocks = NumPhysicalBlocks;
        }
        // The nandregion->TotalLogicalBlocks will be interpreted as
        // percent reserve data for the ftl full managed partition
        // This needs a better fix
        pEntry->RegProp.TotalLogicalBlocks =
                                    pAllocPartIn->PercentReservedBlocks;
    }
    else
    {
        // TotalLogicalBlocks is super block count
        pEntry->RegProp.TotalLogicalBlocks = NumInterleavedLogicalBlocks;
        // Start physical address is chosen based on allocation type

        // In case of relative partition allocation end physical is known
        if (IsRelativeAlloc)
        {
            // Calculate end physical address by checking for good blocks and
            // adding replacement blocks requested
            pEntry->RegProp.TotalPhysicalBlocks = NumPhysicalBlocks;
        }
    }
    // Set start logical sector address based on global interleave setting
    pAllocPartOut->StartLogicalSectorAddress =
        GET_PART_START_SECTOR_FROM_BLK_IL(StartLogicalBlock,
            hNandBlockDev, Log2InterleaveCount);

    // Set start physical sector address based on global interleave setting
    pAllocPartOut->StartPhysicalSectorAddress =
        GET_PART_START_SECTOR_FROM_BLK_IL(StartPhysicalBlock,
            hNandBlockDev, Log2InterleaveCount);


    // Special handling for end partition -
    // as it can be recreated from region table block
    if (pAllocPartIn->NumLogicalSectors == NAND_PART_SIZE_ALLOC_REMAINING)
    {
        pEntry->RegProp.IsUnbounded = NV_TRUE;
    }
    else
    {
        pEntry->RegProp.IsUnbounded = NV_FALSE;
    }
    // flag to ftllite for special handling of partitions read by bootrom
    pEntry->RegProp.IsSequencedReadNeeded = pAllocPartIn->IsSequencedReadNeeded;
    // Count blocks(not super blocks) based on partition interleave setting
    pAllocPartOut->NumLogicalSectors = MACRO_MULT_POW2_LOG2NUM(
        NumInterleavedLogicalBlocks,
        (Log2InterleaveCount +
         hNandBlockDev->hDev->Log2PagesPerBlock));

    pAllocPartOut->NumPhysicalSectors = MACRO_MULT_POW2_LOG2NUM(
        NumPhysicalBlocks,
        (Log2InterleaveCount +
         hNandBlockDev->hDev->Log2PagesPerBlock));

    if (pEntry->RegProp.MgmtPolicy == FTL_FULL_POLICY)
    {
        MinPhysicalBlocksReq = GetMinPhysicalBlocksReq(hNandBlockDev->hDev,
                                                pEntry->RegProp.MgmtPolicy);
        // For last partition the total physical blocks is not stored
        // but is now calculated by load region table function hence
        // using NumPhysicalBlocks
        if (NumPhysicalBlocks <= (MinPhysicalBlocksReq))
        {
            NvOsDebugPrintf("Insufficient space, cannot create partition");
            e = NvError_InsufficientMemory;
            if (pEntry)
            {
                NvOsFree(pEntry);
                pEntry = NULL;
            }
            goto fail;
        }
    }
    // Added this log during nvflash to help debugging, this is not error
    if (IsRelativeAlloc)
    {
        // Relative allocation partition table print
        NAND_BLK_DEV_ERROR_LOG(("\nPartId %u: LB[%u %u] PB[%u %u] IL%u "
            " LS[%u %u] ",
            pEntry->MinorInstance,
            pEntry->RegProp.StartLogicalBlock, pEntry->RegProp.TotalLogicalBlocks,
            pEntry->RegProp.StartPhysicalBlock, pEntry->RegProp.TotalPhysicalBlocks,
            pEntry->RegProp.InterleaveBankCount,
            pAllocPartOut->StartLogicalSectorAddress,
            pAllocPartOut->NumLogicalSectors));
    }
    else // absolute partition allocation
    {
        // Absolute allocation partition table print
        if (hReg)
        {
            // print the logical and physical super block start and count respectively
            NAND_BLK_DEV_ERROR_LOG(("\nAbs PartId %u: LB[%u %u] PB[%u %u] IL%u ",
                hReg->MinorInstance,
                hReg->RegProp.StartLogicalBlock, hReg->RegProp.TotalLogicalBlocks,
                hReg->RegProp.StartPhysicalBlock, hReg->RegProp.TotalPhysicalBlocks,
                hReg->RegProp.InterleaveBankCount));
        }
        // print last partition
        if (pAllocPartIn->NumLogicalSectors == NAND_PART_SIZE_ALLOC_REMAINING)
        {
            NAND_BLK_DEV_ERROR_LOG(("\nLast Abs PartId %u: LS[%u %u] "
                " PartId %u: LB[%u %u] PB[%u %u] IL%u ",
                pEntry->MinorInstance,
                pAllocPartOut->StartLogicalSectorAddress,
                pAllocPartOut->NumLogicalSectors,
                pEntry->MinorInstance,
                pEntry->RegProp.StartLogicalBlock, pEntry->RegProp.TotalLogicalBlocks,
                pEntry->RegProp.StartPhysicalBlock, pEntry->RegProp.TotalPhysicalBlocks,
                pEntry->RegProp.InterleaveBankCount));
        }
        // Prints the sector start and count sent to partmgr
        else
        {
            NAND_BLK_DEV_ERROR_LOG(("\nAbs ** PartId %u: LS[%u %u] ",
                    pEntry->MinorInstance,
                    pAllocPartOut->StartLogicalSectorAddress,
                    pAllocPartOut->NumLogicalSectors));
        }
    }

    // goto region table link-list end
    hReg = hNandBlockDev->hDev->hHeadRegionList;
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
        hNandBlockDev->hDev->hHeadRegionList = pEntry;
    }
fail:
    return e;
}

// Local function used to format Nand device
static void NandUtilFormatDev(
    NandBlockDevHandle hNandBlockDev,
    NvDdkBlockDevEraseType EraseType)
{
    NvU32 BlockCount;
    NvU32 i, j;
    NvU8 *Buffer = NULL;
    NvU8 *pPageBuffer = NULL;
    NvU8 *pTagBuffer = NULL;

    /*  Pad out both page & tag size to a multiple of 64B, and ensure that
     *  the two buffers are at least separated by 64B
     */
    if (EraseType == NvDdkBlockDevEraseType_VerifiedGoodBlocks)
    {
        Buffer =
            NvOsAlloc(((hNandBlockDev->hDev->NandDeviceInfo.TagSize+63) & ~63) +
                ((hNandBlockDev->hDev->NandDeviceInfo.PageSize+63) & ~63) + 64);
    }

    if (Buffer)
    {
        pPageBuffer = Buffer;
        pTagBuffer = Buffer +
            ((hNandBlockDev->hDev->NandDeviceInfo.PageSize+63)&~63) + 64;
    }

    BlockCount = (hNandBlockDev->hDev->BlkDevInfo.TotalBlocks /
                  hNandBlockDev->hDev->NandDeviceInfo.NumberOfDevices);

    for (i=0; i<hNandBlockDev->hDev->NandDeviceInfo.NumberOfDevices; i++)
    {
        if (Buffer)
        {
            for (j=0; j<BlockCount; j++)
            {
                if (NvDdkNandUtilEraseAndTestBlock(hNandBlockDev->hDev->hDdkNand,
                        &hNandBlockDev->hDev->NandDeviceInfo,
                        pPageBuffer, pTagBuffer, i, j)==NandBlock_RuntimeBad)
                {
                    NvU32 PageNumbers[8] = {~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0};
                    NvU32 NumPages = 1;
                    NvU8  BadBlockSentinel[4] = { ~NAND_SPARE_SENTINEL_GOOD,
                                                  ~NAND_SPARE_SENTINEL_GOOD,
                                                  ~NAND_SPARE_SENTINEL_GOOD,
                                                  ~NAND_SPARE_SENTINEL_GOOD };

                    PageNumbers[i] = j *
                        hNandBlockDev->hDev->NandDeviceInfo.PagesPerBlock;
                    /*  FIXME: To be compatible with FTL and non-FTL, both
                     *  the 1st byte of the spare area and the FTL tag area
                     *  are written for run-time bad blocks.  FTL should migrate
                     *  over to use the spare area, so that only 1 byte needs
                     *  to be written.  For simplicity, the entire tag area is
                     *  cleared to 0 for run-time bad blocks.  The page data is
                     *  filled with whatever happens to be left-over in the
                     *  page-buffer following the NandUtilEraseAndTestBlock
                     *  call.
                     */
                    NvOsMemset(pTagBuffer, 0,
                        hNandBlockDev->hDev->NandDeviceInfo.TagSize);
                    NvDdkNandWriteSpare(hNandBlockDev->hDev->hDdkNand,
                        i, PageNumbers, BadBlockSentinel,
                        NAND_SPARE_SENTINEL_LOC, NAND_SPARE_SENTINEL_LEN);
                    NvDdkNandWrite(hNandBlockDev->hDev->hDdkNand, i,
                        PageNumbers, pPageBuffer, pTagBuffer, &NumPages);
                }
            }
        }
        else  // Failed to allocate scratch space for testing.  Just erase NAND.
        {
            // Erase failure for a block does not abort format
            (void)NandUtilEraseChipBlocks(hNandBlockDev, 0,
                      BlockCount, i, EraseType);
        }
    }
    if (Buffer)
        NvOsFree(Buffer);
}

// Utility to actually lock the merged distinct regions
static void NandUtilLockApertures(NandBlockDevHandle hNandBlockDev)
{
    NvU32 i;
    hNandBlockDev->hDev->IsFlashLocked = NV_TRUE;
    for (i = 0; i < hNandBlockDev->hDev->NumofLockAperturesUsed; i++)
    {
        NvDdkNandSetFlashLock(hNandBlockDev->hDev->hDdkNand,
                                    &(hNandBlockDev->hDev->NandFlashLocks[i]));
    }
}

// utility to check for adjacent blocks before locking them
static NvBool
ArePagesInSameBlock(
    NandBlockDevHandle hNandBlockDev,
    NvU32 Page1,
    NvU32 Page2)
{
    // These pages are present in the same chip, so the interleave factor
    // should not be considered.
    NvU32 Blk1 = Page1/hNandBlockDev->hDev->NandDeviceInfo.PagesPerBlock;
    NvU32 Blk2 = Page2/hNandBlockDev->hDev->NandDeviceInfo.PagesPerBlock;
    if (Blk1 == Blk2)
        return NV_TRUE;
    else if (((Blk1>Blk2)?(Blk1 - Blk2):(Blk2 - Blk1)) == 1)
        return NV_TRUE;

    return NV_FALSE;
}

// Function to merge blocks to be locked
static NvError
NandUtilMergeLockedBlocks(
    NandBlockDevHandle hNandBlockDev,
    NvU32 DeviceNum,
    NvU32 PhyStartSector,
    NvU32 PhyEndSector)
{
    NvU32 i;
    NvU32 IsAdjusted = NV_FALSE;

    for (i = 0; i < hNandBlockDev->hDev->NumofLockAperturesUsed; i++)
    {
        if (hNandBlockDev->hDev->NandFlashLocks[i].DeviceNumber == DeviceNum)
        {
            if (ArePagesInSameBlock(hNandBlockDev,
                        hNandBlockDev->hDev->NandFlashLocks[i].EndPageNumber,
                        PhyStartSector))
            {
                hNandBlockDev->hDev->NandFlashLocks[i].EndPageNumber = PhyEndSector;
                IsAdjusted = NV_TRUE;
                break;
            }
            if (ArePagesInSameBlock(hNandBlockDev,
                        hNandBlockDev->hDev->NandFlashLocks[i].StartPageNumber,
                        PhyEndSector))
            {
                hNandBlockDev->hDev->NandFlashLocks[i].StartPageNumber = PhyStartSector;
                IsAdjusted = NV_TRUE;
                break;
            }
        }
    }

    if (!IsAdjusted)
    {
        i = hNandBlockDev->hDev->NumofLockAperturesUsed;
        if (hNandBlockDev->hDev->NumofLockAperturesUsed <= NAND_LOCK_APERTURES)
        {
            hNandBlockDev->hDev->NandFlashLocks[i].DeviceNumber = DeviceNum;
            hNandBlockDev->hDev->NandFlashLocks[i].StartPageNumber= PhyStartSector;
            hNandBlockDev->hDev->NandFlashLocks[i].EndPageNumber= PhyEndSector;
            hNandBlockDev->hDev->NumofLockAperturesUsed++;
            return NvSuccess;
        }
        else
        {
            return NvError_NandBlockDriverLockFailure;
        }
    }

    return NvSuccess;
}

// Function called to lock given sectors from a
// start sector(aligned to block boundary)
static NvError
NandUtilLockRegions(
    NandBlockDevHandle hNandBlockDev,
    NvU32 LogicalSectorStart,
    NvU32 NumberOfSectors,
    NvBool EnableLocks)
 {
    NvError e;
    NvU32 LBAStart;
    NvU32 LBAEnd;
    LockParams LockInfo;
    NvU32 InterleaveCount;
    TagInformation TagInfo;
    NandBlockInfo BlockInfo;
    NvU32 i;
    NvU32 PresentPhyBlk;
    NvU32 RegEndPhyBlk;
    NvU32 DeviceNum;
    NvU32 PhyBlk;
    NvU32 MaxPBA[MAX_INTERLEAVED_NAND_CHIPS];
    NvU32 MinPBA[MAX_INTERLEAVED_NAND_CHIPS];
    NvU32 BlkCnt;
    NandRegionTabHandle RegTab;
    NvU32 TempEndSector;
    NvU32 RegionStartSector;

    BlockInfo.pTagBuffer = (NvU8 *)&TagInfo;
    NvOsMemset(MaxPBA, 0, MACRO_MULT_POW2_LOG2NUM(sizeof(NvU32),
        LOG2MAX_INTERLEAVED_NAND_CHIPS));
    NvOsMemset(MinPBA, 0xFF, MACRO_MULT_POW2_LOG2NUM(sizeof(NvU32),
        LOG2MAX_INTERLEAVED_NAND_CHIPS));

    // Locate the region table where the Lock Region start resides
    // (NOTE: Interleave can be enabled/disabled per partition, pages/block
    // is constant across the NAND device. This is done by reporting
    // unused sectors as part of sector count for non-interleaved partition.)
    RegTab = hNandBlockDev->hDev->hHeadRegionList;
    while (RegTab != NULL)
    {
        // Calculate start sector of partition
        RegionStartSector = MACRO_MULT_POW2_LOG2NUM(RegTab->RegProp.StartLogicalBlock,
                        (hNandBlockDev->hDev->Log2PagesPerBlock +
                         hNandBlockDev->hDev->Log2InterleaveCount));
        // Calculate Start sector of block beyond partition end
        TempEndSector = MACRO_MULT_POW2_LOG2NUM(RegTab->RegProp.TotalLogicalBlocks,
                        (hNandBlockDev->hDev->Log2PagesPerBlock +
                         hNandBlockDev->hDev->Log2InterleaveCount)) +
                         RegionStartSector;
        if ((LogicalSectorStart < TempEndSector)
                &&(RegionStartSector <= LogicalSectorStart))
        {
            // Found partition entry in which lock start sector is located
            break;
        }

        RegTab = RegTab->Next;
    }
    if (RegTab == NULL)
    {
        return NvError_InvalidAddress;
    }

    // Calculate the lock region start/stop LBA's from the region table info
    // LogicalSectorStart is passed by Shim which interprets entire
    // Nand as either interleaved or non-interleaved
    LBAStart = MACRO_DIV_POW2_LOG2NUM(LogicalSectorStart,
         (hNandBlockDev->hDev->Log2PagesPerBlock +
          hNandBlockDev->hDev->Log2InterleaveCount));
    LBAEnd = MACRO_DIV_POW2_LOG2NUM((LogicalSectorStart + NumberOfSectors - 1),
                (hNandBlockDev->hDev->Log2PagesPerBlock +
                hNandBlockDev->hDev->Log2InterleaveCount));

    InterleaveCount = RegTab->RegProp.InterleaveBankCount;
    // Find the Physical block address range in each interleave column.
    for (i = 0; i < InterleaveCount; i++)
    {
        RegEndPhyBlk = RegTab->RegProp.StartPhysicalBlock +
                            RegTab->RegProp.TotalPhysicalBlocks - 1;
        PresentPhyBlk = RegTab->RegProp.StartPhysicalBlock;
        BlkCnt = 0;
        while (PresentPhyBlk <= RegEndPhyBlk)
            {
                NvBool IsFakeBad;
                PhyBlk = PresentPhyBlk;
                DeviceNum = i;
                if (!NvBtlGetPba(&DeviceNum, &PhyBlk))
                    break;
                IsFakeBad = NandUtilIsFakeBad(DeviceNum, PhyBlk);

                // Initialize the spare buffer before calling GetBlockInfo API
                NvOsMemset(BlockInfo.pTagBuffer, 0,
                    hNandBlockDev->hDev->NandDeviceInfo.NumSpareAreaBytes);
                // Need skipped bytes along with tag information
                NV_CHECK_ERROR_CLEANUP(NvDdkNandGetBlockInfo(
                    hNandBlockDev->hDev->hDdkNand, DeviceNum,
                    PhyBlk, &BlockInfo, NV_TRUE));

                // Skip uninitialized blocks
                if (((NvS32)NandUtilGetWord((NvU8 *)&TagInfo.LogicalBlkNum) == (-1))
                    /* Skip bad blocks when counting blocks to lock */
                    || ((IsFakeBad) ||
                    (BlockInfo.IsFactoryGoodBlock == NV_FALSE) ||
                    /* Run-time bad block check */
                    (hNandBlockDev->hDev->pSpareBuf[1] != 0xFF)))
                {
                    PresentPhyBlk++;
                    continue;
                }
                // Check if the physical block lies in the region to be locked.
                if ((NandUtilGetWord((NvU8 *)&TagInfo.LogicalBlkNum) <= LBAEnd) &&
                        (NandUtilGetWord((NvU8 *)&TagInfo.LogicalBlkNum) >= LBAStart))
                {
                     BlkCnt++;
                    if (PresentPhyBlk > MaxPBA[DeviceNum])
                    {
                        MaxPBA[DeviceNum] = PresentPhyBlk;
                    }

                    if (PresentPhyBlk < MinPBA[DeviceNum])
                    {
                        MinPBA[DeviceNum] = PresentPhyBlk;
                    }
                    // Stop scanning once we find all the logical blocks to be
                    // locked.
                    if (BlkCnt == (LBAEnd - LBAStart + 1))
                        break;
                }
                PresentPhyBlk++;
            }
        }

        // Check for regions to be locked in each of the chips.
        for (i=0 ; i < MAX_INTERLEAVED_NAND_CHIPS; i++)
        {
            // Skip chips with no region to be locked.
            if (((NvS32)MinPBA[i] == (-1)) && (MaxPBA[i] == 0))
            {
                continue;
            }
            // The Start sector is aligned to the start sector of the block and the
            // end to the last sector of the block.
            LockInfo.DeviceNumber = i;
            LockInfo.StartPageNumber =
                MACRO_MULT_POW2_LOG2NUM(MinPBA[i], hNandBlockDev->hDev->Log2PagesPerBlock);
            LockInfo.EndPageNumber = (MACRO_MULT_POW2_LOG2NUM(MaxPBA[i],
                hNandBlockDev->hDev->Log2PagesPerBlock) +
                (hNandBlockDev->hDev->NandDeviceInfo.PagesPerBlock - 1));

            // Add this region to the list of regions that need to be locked and also try to coalesce the lock regions
            NV_CHECK_ERROR_CLEANUP(NandUtilMergeLockedBlocks(hNandBlockDev,
                                    LockInfo.DeviceNumber,
                                    LockInfo.StartPageNumber,
                                    LockInfo.EndPageNumber));
      }
    // Check if this is the last Lock IOCTL and lock all regions if it is.
     if (EnableLocks)
     {
        if (hNandBlockDev->hDev->IsFlashLocked)
        {
            return NvError_AlreadyAllocated;
        }

       NandUtilLockApertures( hNandBlockDev);
     }
     return NvSuccess;
fail:
     return e;
}

// Function to get physical boundaries of partition given logical boundary
static NvError GetPartitionPhysicalSector(
        NandBlockDevHandle hNandBlockDev,
        NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsInputArgs *pInputArgs,
        NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsOutputArgs *pOutputArgs)
{
    NvError e = NvSuccess;
    NandRegionTabHandle hRegion = hNandBlockDev->hRegEntry;
    NvU32 StartLogicalSector = 0;

    if (hRegion)
    {
        while (hRegion )
        {
            NvU32 Log2InterleaveCount =
                NandUtilGetLog2(hRegion->RegProp.InterleaveBankCount);
            // Use partition specific interleave setting to compare against
            // start logical addresses sent from PT manager
            StartLogicalSector = (hRegion ->RegProp.StartLogicalBlock << (
                hNandBlockDev->hDev->Log2PagesPerBlock + Log2InterleaveCount));
            if (StartLogicalSector ==
                pInputArgs->PartitionLogicalSectorStart)
            {
                // match found
                if (hRegion->RegProp.TotalPhysicalBlocks ==
                    NAND_PART_SIZE_ALLOC_REMAINING)
                {
                    NV_CHECK_ERROR_CLEANUP(
                        NandUtilSetEndPartPhysicalBLockCount(
                        hNandBlockDev));
                }
                // Use device wide specific interleave setting to calculate
                // physical addresses returned back PT manager
                pOutputArgs->PartitionPhysicalSectorStart =
                                (hRegion->RegProp.StartPhysicalBlock
                                << (hNandBlockDev->hDev->Log2PagesPerBlock +
                                hNandBlockDev->hDev->Log2InterleaveCount));
                pOutputArgs->PartitionPhysicalSectorStop =
                                ((hRegion->RegProp.StartPhysicalBlock +
                                 hRegion->RegProp.TotalPhysicalBlocks)
                                << (hNandBlockDev->hDev->Log2PagesPerBlock +
                                hNandBlockDev->hDev->Log2InterleaveCount));

                e = NvSuccess;
                goto fail;
            }
            hRegion = hRegion->Next;
        }
        e = NvError_BadParameter;
    }
fail:
    return e;
}

// Function to erase partition
// Erases All good blocks in partition. Run-time bad interpretation
// based on the partition management policy
static NvError NandUtilErasePartition(NandBlockDevHandle hNandBlockDev,
    NvU64 StartSector, NvU64 NumSectors)
{
    NvU32 i;
    NvU32 StartBlk;
    NvU32 EndBlk;
    NvError e = NvSuccess;
    NandSpareInfo SpareInfo;
    NvU32 ActualStart;
    NvU32 ActualEnd;
    NvU32 Log2InterleaveCount = NandUtilGetLog2(
        hNandBlockDev->hRegEntry->RegProp.InterleaveBankCount);
    NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsInputArgs InArgs;
    NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsOutputArgs  OutArgs;
    // StartBlk indicates start physical block number counting only used blocks
    // Note: non-intereleaved partitions waste the banks other than bank 0
    StartBlk = hNandBlockDev->hRegEntry->RegProp.StartPhysicalBlock;
    EndBlk = StartBlk + hNandBlockDev->hRegEntry->RegProp.TotalPhysicalBlocks;
    InArgs.PartitionLogicalSectorStart = (NvU32)StartSector;
    InArgs.PartitionLogicalSectorStop = (NvU32)(StartSector + NumSectors);
    e = GetPartitionPhysicalSector(hNandBlockDev, &InArgs, &OutArgs);
    if (e != NvSuccess)
    {
        NvOsDebugPrintf("\r\nErase failed. Get Physical Sectors failed for "
            "logical start=%d,stop=%d ", InArgs.PartitionLogicalSectorStart,
            InArgs.PartitionLogicalSectorStop);
        goto fail;
    }
    // sanity for Partition boundary given in format partition
    // We may not logical block count for last partition, hence using physical
    NV_ASSERT((MACRO_MULT_POW2_LOG2NUM(StartBlk,
        (hNandBlockDev->hDev->Log2PagesPerBlock +
        hNandBlockDev->hDev->Log2InterleaveCount))) ==
        OutArgs.PartitionPhysicalSectorStart);
    // We may not logical block count for last partition, hence using physical
    NV_ASSERT((MACRO_MULT_POW2_LOG2NUM(
        hNandBlockDev->hRegEntry->RegProp.TotalPhysicalBlocks,
        (hNandBlockDev->hDev->Log2PagesPerBlock +
        hNandBlockDev->hDev->Log2InterleaveCount))) ==
        (OutArgs.PartitionPhysicalSectorStop -
        OutArgs.PartitionPhysicalSectorStart));
    NAND_BLK_DEV_ERROR_LOG(("\r\nErase Partition part-id=%d: Start=%d,End=%d ",
        hNandBlockDev->hRegEntry->MinorInstance, StartBlk, (EndBlk - 1)));
    // Actual physical block number on target system is obtained
    // by viewing using partition interleave mode
    ActualStart = MACRO_MULT_POW2_LOG2NUM(StartBlk, Log2InterleaveCount);
    ActualEnd = ActualStart +
        hNandBlockDev->hRegEntry->RegProp.TotalPhysicalBlocks;
    // Erase the number of physical blocks in the partition
    for (i = ActualStart; i < ActualEnd; i++)
    {
        NvU32 BnkNum;
        NvU32 BlkNum;
        NvBool IsFakeBad;
        NvBool IsGoodBlk;
        // Get chip and block number corresponding to physical block
        // We do not erase the other banks when partition is non-interleaved
        BnkNum = MACRO_MOD_LOG2NUM(i, Log2InterleaveCount);
        BlkNum = MACRO_DIV_POW2_LOG2NUM(i, Log2InterleaveCount);
        // get the chip number within the bank
        if (!NvBtlGetPba(&BnkNum, &BlkNum))
            break;
        // Check if block is good by calling FTL API corresponding to handle
        e = NandIsGoodBlock(hNandBlockDev,
            BnkNum, BlkNum, &SpareInfo);
        if (e != NvSuccess)
            continue;
        // Check for fake bad as well
        IsFakeBad = NandUtilIsFakeBad(BnkNum, BlkNum);
        if (s_NandCommonInfo.NandEraseType == NvDdkBlockDevEraseType_GoodBlocks)
        {
            // FIXME:
            // Once Android run-time bad starts getting used in all codebases
            // Enable factory and run-time good blocks only.
            IsGoodBlk = (SpareInfo.IsGoodBlock && (!IsFakeBad));
        }
        else
        {
            // Erase factory good blocks
            IsGoodBlk = (SpareInfo.IsFactoryGoodBlock && (!IsFakeBad));
        }
        if (IsGoodBlk)
        {
            // Erase the good block
            e = NandUtilEraseBlock(hNandBlockDev, BnkNum, BlkNum);
        }
    }
fail:
    return NvSuccess;
}

// Local function used to read back data written into Nand earlier and verify
// if write data correctly wrote to Nand.
static NvError
NandReadVerifyDdkWrite(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 SectorNum,
    NvU32 ChipNum,
    const void* pBuffer,
    NvU32 NumberOfSectors)
{
    NvError e = NvSuccess;
    NvU32 PageNumber[MAX_INTERLEAVED_NAND_CHIPS];
    NvU8 *pBufRdVerify;
    NvU32 i;
    NvU32 i2;
    NvU32 BufSize;
    NandBlockDevHandle hNandBlkDev = (NandBlockDevHandle)hBlockDev;
    NvU32 RetBlockNum;
    NvU32 SectorFromChipCp;
    NandSpareInfo SpareInfo;
    NvU32 SectorOffset;

    // Allocate a read buffer
    BufSize = MACRO_MULT_POW2_LOG2NUM(sizeof(NvU8),
        hNandBlkDev->hDev->Log2BytesPerPage);
    pBufRdVerify = (NvU8 *)NvOsAlloc(BufSize);
    RetBlockNum = MACRO_DIV_POW2_LOG2NUM(SectorNum,
        (hNandBlkDev->hDev->Log2PagesPerBlock +
        hNandBlkDev->hDev->Log2InterleaveCount));
    SectorOffset = MACRO_MOD_LOG2NUM(SectorNum,
        (hNandBlkDev->hDev->Log2InterleaveCount +
        hNandBlkDev->hDev->Log2PagesPerBlock));
    if (!pBufRdVerify)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    // clear read buffer
    NvOsMemset(pBufRdVerify, 0x0, BufSize);
    // Iterate over all requested sectors
    for (i = 0; i < NumberOfSectors; i++)
    {
        SectorFromChipCp = 1;
         if ((NvBtlGetPba(&ChipNum, &RetBlockNum) == NV_FALSE) ||
            (ChipNum > hNandBlkDev->hDev->NandDeviceInfo.NumberOfDevices))
        {
            e = NvError_InvalidAddress;
            goto fail;
        }
        NV_CHECK_ERROR_CLEANUP(NandIsGoodBlock(hNandBlkDev,
            ChipNum, RetBlockNum, &SpareInfo));
        if (SpareInfo.IsGoodBlock)
        {
            // Initialize the Page numbers first since Ddk needs
            // interleaved chip page numbers
            for (i2 = 0; i2 < MAX_INTERLEAVED_NAND_CHIPS; i2++)
                PageNumber[i2] = s_NandIllegalAddress;
            PageNumber[ChipNum] = (RetBlockNum << hNandBlkDev->hDev->Log2PagesPerBlock) +
                SectorOffset + i;
            // Read starting Physical sector address
            // One sector at a time is read and compared
            NV_CHECK_ERROR_CLEANUP(NvDdkNandRead(hNandBlkDev->hDev->hDdkNand,
                    ChipNum,
                    PageNumber,
                    (NvU8 *)pBufRdVerify,
                    NULL, &SectorFromChipCp, NV_FALSE));
            // compare the read sector)
            if (NandUtilMemcmp((void *)((NvU8 *)pBuffer +
                (MACRO_MULT_POW2_LOG2NUM(i, hNandBlkDev->hDev->Log2BytesPerPage))),
                (void *)pBufRdVerify, BufSize))
            {
                e = NvError_BlockDriverWriteVerifyFailed;
                goto fail;
            }
        }
    }
fail:
    NvOsFree(pBufRdVerify);
    return e;
}

// macro to make string from constant
#define MACRO_GET_STR(x, y) \
    NvOsMemcpy(x, #y, (NvOsStrlen(#y) + 1))

// Utility to return name of Ioctl given ioctl opcode
static void
UtilGetIoctlName(NvU32 Opcode, NvU8 *IoctlStr)
{
    switch(Opcode)
    {
        case NvDdkBlockDevIoctlType_DisableCache:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_DisableCache);
            break;
        case NvDdkBlockDevIoctlType_EraseLogicalSectors:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_EraseLogicalSectors);
            break;
        case NvDdkBlockDevIoctlType_QueryFirstBoot:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_QueryFirstBoot);
            break;
        case NvDdkBlockDevIoctlType_DefineSubRegion:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_DefineSubRegion);
            break;
        case NvDdkBlockDevIoctlType_WriteVerifyModeSelect:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_WriteVerifyModeSelect);
            break;
        case NvDdkBlockDevIoctlType_AllocatePartition:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_AllocatePartition);
            break;
        case NvDdkBlockDevIoctlType_PartitionOperation:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_PartitionOperation);
            break;
        case NvDdkBlockDevIoctlType_ReadPhysicalSector :
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_ReadPhysicalSector);
            break;
        case NvDdkBlockDevIoctlType_WritePhysicalSector:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_WritePhysicalSector);
            break;
        case NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus);
            break;
        case NvDdkBlockDevIoctlType_ErasePhysicalBlock:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_ErasePhysicalBlock);
            break;
        case NvDdkBlockDevIoctlType_LockRegion:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_LockRegion);
            break;
        case NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector);
            break;
        case NvDdkBlockDevIoctlType_FormatDevice:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_FormatDevice);
            break;
        case NvDdkBlockDevIoctlType_GetPartitionPhysicalSectors:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_GetPartitionPhysicalSectors);
            break;
        case NvDdkBlockDevIoctlType_IsGoodBlock:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_IsGoodBlock);
            break;
        case NvDdkBlockDevIoctlType_UnprotectAllSectors:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_UnprotectAllSectors);
            break;
        case NvDdkBlockDevIoctlType_ProtectSectors:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_ProtectSectors);
            break;
    }
}

/********************************************************/
/* Start NAND Block device driver Interface definitions */
/********************************************************/

// Nand local function to open block driver
static NvError
NandBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NandBlockDevHandle* hNandBlkDev)
{
    NvError e = NvSuccess;
    NandBlockDevHandle hTempNandBlockDev = NULL;
    NandDevHandle hTempDev = NULL;
    NandRegionTabHandle hRegEntry;
    static NvBool s_IsFirstCall = NV_TRUE;
    NvU32 Log2NumDevs;
    NvU32 SpareAreaSize;
    NV_ASSERT(hNandBlkDev);

#if FIREFLY_CORRUPT_DEBUG
    NAND_BLK_DEV_ERROR_LOG(("\nNand Open Entry "));
#endif
    NvOsMutexLock((*(s_NandCommonInfo.NandDevLockList + Instance)));
    hTempNandBlockDev = NvOsAlloc(sizeof(NandBlocDev));
    if (!hTempNandBlockDev)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    // initialize all members of nand block driver to 0
    NvOsMemset(hTempNandBlockDev, 0, sizeof(NandBlocDev));
    // Allocate the data shared by all partitions on a device
    // This has to be created once.
    if (*(s_NandCommonInfo.NandDevStateList + Instance))
    {
        hTempNandBlockDev->hDev =
            (*(s_NandCommonInfo.NandDevStateList + Instance));
        NV_CHECK_ERROR_CLEANUP(NvDdkNandResume(
            hTempNandBlockDev->hDev->hDdkNand));
        // without close if open is called we reach here
        // Make sure region table is loaded for non-zero part-id open
        if ((MinorInstance) && !(hTempNandBlockDev->hDev->hHeadRegionList))
        {
                NAND_BLK_DEV_DEBUG_INFO(("\n Explicit load region table "));
                // Need to read region table from Nand device
                e = NandUtilLoadRegionTable(hTempNandBlockDev);
                if (e != NvSuccess)
                {
                    NAND_BLK_DEV_ERROR_LOG(("\nError Nand block driver "
                        "Load Region table call failed for part-id=%d, "
                        "error code=%d ", MinorInstance, e));
                    goto fail;
                }
        }
    }
    else
    {
         // Allocate once for device
         hTempDev = (NandDevHandle)NvOsAlloc(sizeof(struct NandDevRec));
         if (!hTempDev)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }
         // Ensures that NandDevHandle->hHeadRegionList gets
         // LogicalAddressStart also set as 0
         // initialized to NULL
         NvOsMemset(hTempDev, 0, sizeof(struct NandDevRec));
        // Use the lock create earlier
         hTempDev->LockDev = *(s_NandCommonInfo.NandDevLockList + Instance);
        // init Ddk Nand handle
        NV_CHECK_ERROR_CLEANUP(NvDdkNandOpen(
            s_NandCommonInfo.hRmNand, &hTempDev->hDdkNand));

        hTempDev->NandDdkFuncs.NvDdkDeviceErase = NvDdkNandErase;
        hTempDev->NandDdkFuncs.NvDdkDeviceRead = NvDdkNandRead;
        hTempDev->NandDdkFuncs.NvDdkDeviceWrite = NvDdkNandWrite;
        hTempDev->NandDdkFuncs.NvDdkGetDeviceInfo = NvDdkNandGetDeviceInfo;
        hTempDev->NandDdkFuncs.NvDdkGetLockedRegions = NvDdkNandGetLockedRegions;
        hTempDev->NandDdkFuncs.NvDdkGetBlockInfo = NvDdkNandGetBlockInfo;
        hTempDev->NandDdkFuncs.NvDdkReadSpare = NvDdkNandReadSpare;
        hTempDev->NandDdkFuncs.NvDdkWriteSpare = NvDdkNandWriteSpare;

        // variable to track physical address of created partitions is 0
        // save device info
        NV_CHECK_ERROR_CLEANUP(NvDdkNandGetDeviceInfo(
            hTempDev->hDdkNand, 0, &hTempDev->NandDeviceInfo));
        Log2NumDevs = NandUtilGetLog2(hTempDev->NandDeviceInfo.NumberOfDevices);
        // Update the block driver DeviceInfo copy
        hTempDev->BlkDevInfo.BytesPerSector = hTempDev->NandDeviceInfo.PageSize;
        hTempDev->BlkDevInfo.SectorsPerBlock = hTempDev->NandDeviceInfo.PagesPerBlock;
        hTempDev->BlkDevInfo.TotalBlocks = MACRO_MULT_POW2_LOG2NUM(hTempDev->NandDeviceInfo.NoOfBlocks,
            Log2NumDevs);
        // Initialize log of pages per block
        hTempDev->Log2PagesPerBlock =
            NandUtilGetLog2(hTempDev->BlkDevInfo.SectorsPerBlock);
        // Initialize log of bytes per page
        hTempDev->Log2BytesPerPage =
            NandUtilGetLog2(hTempDev->BlkDevInfo.BytesPerSector);
        hTempDev->BlkDevInfo.DeviceType = NvDdkBlockDevDeviceType_Fixed;

        SpareAreaSize = hTempDev->NandDeviceInfo.NumSpareAreaBytes;
        hTempDev->pSpareBuf = NvOsAlloc(SpareAreaSize);
        if(!hTempDev->pSpareBuf)
        {
            e= NvError_InsufficientMemory;
            goto fail;
        }
        NV_CHECK_ERROR_CLEANUP(NvDdkNandResume(hTempDev->hDdkNand));
        // Start with no interleave and update later as other partitions
        // are opened using the maximum of the interleave count
        hTempDev->InterleaveCount = NAND_NOINTERLEAVE_COUNT;
        // Update log2 interleave count
        hTempDev->Log2InterleaveCount = NandUtilGetLog2(hTempDev->InterleaveCount);
        // Save the device major instance
        hTempDev->Instance = Instance;

        // set the device instance state entry
        hTempNandBlockDev->hDev = hTempDev;
        (*(s_NandCommonInfo.NandDevStateList + Instance)) =
                                                        hTempNandBlockDev->hDev;
        if (MinorInstance)
        {
                // Need to read region table from Nand device
                e = NandUtilLoadRegionTable(hTempNandBlockDev);
                if (e != NvSuccess)
                {
                    NAND_BLK_DEV_ERROR_LOG(("\nError Nand block driver Load Region "
                        "table call failed for part-id=%d, error code=%d ",
                        MinorInstance, e));
                    goto fail;
                }
                // Interleave count gets read from region table here
        }
        // Print the interleave count once in OS image
        if (s_IsFirstCall == NV_TRUE)
        {
            NAND_BLK_DEV_ERROR_LOG(("\nGlobal Nand Interleave count = %u \n",
                hTempDev->InterleaveCount));
            s_IsFirstCall = NV_FALSE;
        }
    }

    // initialize FTL unconditionally - NvBtlGetPba API also used for part id 0
    if (!((*(s_NandCommonInfo.NandDevStateList + Instance))->
        IsFtlInitialized))
    {
        // initialize FTL, once for the Nand device.
        // Else, write-read tests do not work
        NvBtlInit((*(s_NandCommonInfo.NandDevStateList + Instance))->
            hDdkNand,
            &((*(s_NandCommonInfo.NandDevStateList + Instance))->NandDdkFuncs),
            (*(s_NandCommonInfo.NandDevStateList + Instance))->
            InterleaveCount);
        (*(s_NandCommonInfo.NandDevStateList + Instance))->
            IsFtlInitialized = 1;
    }

    // Only for non-zero MinorInstance number we need to get FTL handle
    if (MinorInstance)
    {
        NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgs WrVerifyMode;
        // Find the partition-id region entry,
        NV_CHECK_ERROR_CLEANUP(NandUtilGetRegionEntry(
            (*(s_NandCommonInfo.NandDevStateList + Instance)),
            MinorInstance, &hRegEntry));
        // assign region entry handle
        hTempNandBlockDev->hRegEntry = hRegEntry;
        // Init partition local interleave count log2
        hTempNandBlockDev->Log2InterleaveCountPart =
            NandUtilGetLog2(hRegEntry->RegProp.InterleaveBankCount);
        // Call FTL function
        e = NvNandOpenRegion(&hTempNandBlockDev->hFtl,
            (*(s_NandCommonInfo.NandDevStateList + Instance))->hDdkNand,
            &((*(s_NandCommonInfo.NandDevStateList + Instance))->NandDdkFuncs),
            &hRegEntry->RegProp);
        if (e != NvSuccess)
        {
            NAND_BLK_DEV_ERROR_LOG(("\nFTL open for partition=%d failed,code=%d ",
                MinorInstance, e));
        }
        // set FTL write verify mode
        WrVerifyMode.IsReadVerifyWriteEnabled =
            hTempNandBlockDev->IsReadVerifyWriteEnabled;
        NV_CHECK_ERROR_CLEANUP(hTempNandBlockDev->hFtl->NvNandRegionIoctl(
                        hTempNandBlockDev->hFtl,
                        NvDdkBlockDevIoctlType_WriteVerifyModeSelect,
                        sizeof(NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgs),
                        0, &WrVerifyMode, NULL));
    }
    else
    {
        // FTL pointer is NULL when part_id == 0 partition is opened
    }
    // Partition specific members need to be initialized
    // mark powered on
    hTempNandBlockDev->IsPowered = NV_TRUE;
    // Disable read verify mode every time device is opened
    hTempNandBlockDev->IsReadVerifyWriteEnabled = NV_FALSE;
    // Reference and power up counters are reset to 0 by
    // memset operation after allocation of the NandBlocDev type
    // Increment Dev ref count
    hTempNandBlockDev->hDev->RefCount++;
    // power up counter is also incremented
    hTempNandBlockDev->hDev->PowerUpCounter++;
    *hNandBlkDev = hTempNandBlockDev;
    // FIXME : enable read verify mode for test purpose
    // hTempNandBlockDev->IsReadVerifyWriteEnabled = NV_TRUE;
    NvOsMutexUnlock((*(s_NandCommonInfo.NandDevLockList + Instance)));
    return e;
fail:
    (*(s_NandCommonInfo.NandDevStateList + Instance)) = NULL;
    // Release resources here.
    if (hTempDev)
    {
        // Close Nand Ddk Handle else get Nand already open
        NvDdkNandClose(hTempDev->hDdkNand);
        NvOsFree(hTempDev->pSpareBuf);
        NvOsFree(hTempDev);
    }
    if (hTempNandBlockDev)
        NvOsFree(hTempNandBlockDev);
    *hNandBlkDev = NULL;
    NvOsMutexUnlock((*(s_NandCommonInfo.NandDevLockList + Instance)));
    if (e != NvSuccess)
    {
        // Nand Block driver open call fails
        NAND_BLK_DEV_ERROR_LOG(("\nNand Block dev open failed error 0x%x ",
            e));
    }
#if FIREFLY_CORRUPT_DEBUG
    else
    {
        NAND_BLK_DEV_ERROR_LOG(("\nNand Open Success "));
    }
#endif
    return e;
}

// Nand local function to close block driver
static void NandBlockDevClose(NvDdkBlockDevHandle hBlockDev)
{
    NandBlockDevHandle hNandBlkDev = (NandBlockDevHandle)hBlockDev;

    // ensures Close does not result in complications if open not done
    if (hNandBlkDev->hDev->RefCount == 0)
    {
        return;
    }
    // Partition specific members need to be de-initialized
    // mark powered off
    hNandBlkDev->IsPowered = NV_FALSE;
    // Disable read verify mode every time device is closed
    hNandBlkDev->IsReadVerifyWriteEnabled = NV_FALSE;
    if (hNandBlkDev->hDev->RefCount == 1)
    {
        // We can free the partition specific data
        if (hNandBlkDev->hFtl)
            hNandBlkDev->hFtl->NvNandCloseRegion(hNandBlkDev->hFtl);

        // Close Nand Ddk Handle else get Nand already open
        NvDdkNandClose(hNandBlkDev->hDev->hDdkNand);

        // Globals also need to be cleaned
        (*(s_NandCommonInfo.NandDevStateList +
            hNandBlkDev->hDev->Instance)) = NULL;
        // Free device specific data
        // Free the region table
        RegionTableFree(hNandBlkDev->hDev->hHeadRegionList);
        // Region table head must be reset
        hNandBlkDev->hDev->hHeadRegionList = NULL;
        NvOsFree(hNandBlkDev->hDev->pSpareBuf);
        NvOsFree(hNandBlkDev->hDev);
    }
    else
    {
        // decrement reference count for Nand major instance
        NvOsMutexLock(hNandBlkDev->hDev->LockDev);
        hNandBlkDev->hDev->RefCount--;
        hNandBlkDev->hDev->PowerUpCounter--;
        // We can free the partition specific data
        if (hNandBlkDev->hFtl)
            hNandBlkDev->hFtl->NvNandCloseRegion(hNandBlkDev->hFtl);

        NvDdkNandSuspend(hNandBlkDev->hDev->hDdkNand);
        NvOsMutexUnlock(hNandBlkDev->hDev->LockDev);
    }
    NvOsFree(hNandBlkDev);
    hBlockDev = NULL;
}

// Nand local function for block driver read sector
static NvError
NandBlockDevReadSector(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 SectorNum,
    void* const pBuffer,
    NvU32 NumberOfSectors)
{
    NvError e;
    NandBlockDevHandle hNandBlkDev = (NandBlockDevHandle)hBlockDev;

#if FIREFLY_CORRUPT_DEBUG
    NAND_BLK_DEV_ERROR_LOG(("\nNand Read Entry "));
#endif
    if (!hNandBlkDev->hRegEntry)
    {
        NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs
            MapLogical2PhysicalInArgs;
        NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs
            MapLogical2PhysicalOutArgs;
        NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs RdPhysicalSectorArgs;
        // part-id == 0 has region entry pointer as NULL
        // Since FTL handle is uninitialized cannot call FTL function for read
        // calling physical read ioctl after mapping the logical sector
        // number to physical sector number
        MapLogical2PhysicalInArgs.LogicalSectorNum = SectorNum;
        MapLogical2PhysicalOutArgs.PhysicalSectorNum = 0;
        // ioctl calls implement locking
        NV_CHECK_ERROR_CLEANUP(hBlockDev->NvDdkBlockDevIoctl(hBlockDev,
            NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector,
            sizeof(NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs),
            sizeof(NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs),
            &MapLogical2PhysicalInArgs, &MapLogical2PhysicalOutArgs));
        RdPhysicalSectorArgs.SectorNum =
            MapLogical2PhysicalOutArgs.PhysicalSectorNum;
        RdPhysicalSectorArgs.NumberOfSectors = NumberOfSectors;
        RdPhysicalSectorArgs.pBuffer = pBuffer;
        NV_CHECK_ERROR_CLEANUP(hBlockDev->NvDdkBlockDevIoctl(hBlockDev,
            NvDdkBlockDevIoctlType_ReadPhysicalSector,
            sizeof(NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs),
            0, &RdPhysicalSectorArgs, NULL));
    }
    else
    {
        // non-zero id partitions have region entry pointer initialized to
        // entry from the region table
        NvOsMutexLock(hNandBlkDev->hDev->LockDev);
        NV_CHECK_ERROR_CLEANUP(NvDdkNandResume(hNandBlkDev->hDev->hDdkNand));
        // Debug build can enable log of read request
        NAND_BLK_DEV_RD_WR_LOG(("\nPartId=%u, , Read: start=0x%x, sector "
            "count=0x%x ", hNandBlkDev->hRegEntry->MinorInstance,
            SectorNum, NumberOfSectors));
        NV_CHECK_ERROR_CLEANUP(NandUtilGetPartitionRelativeAddr(
            hNandBlkDev, &SectorNum));
        NV_CHECK_ERROR_CLEANUP(hNandBlkDev->hFtl->NvNandRegionReadSector(hNandBlkDev->hFtl,
            SectorNum, pBuffer, NumberOfSectors));
fail:
        if (e != NvSuccess)
        {
            // Nand block driver read fails
            NAND_BLK_DEV_ERROR_LOG(("\n Nand block driver: Read "
                "Error = 0x%x, PartId=%u, Read: start=0x%x, sector count=0x%x ",
                e, hNandBlkDev->hRegEntry->MinorInstance,
                SectorNum, NumberOfSectors));
        }
#if FIREFLY_CORRUPT_DEBUG
        else
        {
            NAND_BLK_DEV_ERROR_LOG(("\nNand Read Success "));
        }
#endif
        NvOsMutexUnlock(hNandBlkDev->hDev->LockDev);
    }
    return e;
}

// Nand local function for block driver write sector
static NvError
NandBlockDevWriteSector(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 SectorNum,
    const void* pBuffer,
    NvU32 NumberOfSectors)
{
    NvError e;
    NandBlockDevHandle hNandBlkDev = (NandBlockDevHandle)hBlockDev;

#if FIREFLY_CORRUPT_DEBUG
    NAND_BLK_DEV_ERROR_LOG(("\nNand Write Entry "));
#endif
    if (!hNandBlkDev->hRegEntry)
    {
        NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs
            MapLogical2PhysicalInArgs;
        NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs
            MapLogical2PhysicalOutArgs;
        NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs WrPhysicalSectorArgs;
        // part-id == 0 has region entry pointer as NULL
        // Since FTL handle is uninitialized cannot call FTL function for write
        // calling physical write ioctl after mapping the logical sector
        // number to physical sector number
        MapLogical2PhysicalInArgs.LogicalSectorNum = SectorNum;
        MapLogical2PhysicalOutArgs.PhysicalSectorNum = 0;
        // ioctl calls implement locking
        NV_CHECK_ERROR_CLEANUP(hBlockDev->NvDdkBlockDevIoctl(hBlockDev,
            NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector,
            sizeof(NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs),
            sizeof(NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs),
            &MapLogical2PhysicalInArgs, &MapLogical2PhysicalOutArgs));
        WrPhysicalSectorArgs.SectorNum =
            MapLogical2PhysicalOutArgs.PhysicalSectorNum;
        WrPhysicalSectorArgs.NumberOfSectors = NumberOfSectors;
        WrPhysicalSectorArgs.pBuffer = pBuffer;
        NV_CHECK_ERROR_CLEANUP(hBlockDev->NvDdkBlockDevIoctl(hBlockDev,
            NvDdkBlockDevIoctlType_WritePhysicalSector,
            sizeof(NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs),
            0, &WrPhysicalSectorArgs, NULL));
    }
    else
    {
        // non-zero id partitions have region entry pointer initialized to
        // entry from the region table
        // Debug build can enable log of write request
        NAND_BLK_DEV_RD_WR_LOG(("\nPartId=%u, , Write: start=0x%x, sector "
            "count=0x%x ", hNandBlkDev->hRegEntry->MinorInstance,
            SectorNum, NumberOfSectors));
        NvOsMutexLock(hNandBlkDev->hDev->LockDev);

        NV_CHECK_ERROR_CLEANUP(NvDdkNandResume(hNandBlkDev->hDev->hDdkNand));
        NV_CHECK_ERROR_CLEANUP(NandUtilGetPartitionRelativeAddr(
            hNandBlkDev, &SectorNum));
        // Write may fail if block fails write and no replacement found
        // FTL Lite takes care of read verify now
        NV_CHECK_ERROR_CLEANUP(hNandBlkDev->hFtl->NvNandRegionWriteSector(hNandBlkDev->hFtl,
            SectorNum, pBuffer, NumberOfSectors));
fail:
        if (e != NvSuccess)
        {
            // Nand block driver write fails
            // LTK lock test becomes slower if these prints are enabled
            if (e != NvError_NandBlockIsLocked)
            {
                NAND_BLK_DEV_ERROR_LOG(("\nNand block driver: Write Error = "
                    "0x%x, PartId=%u, , Write: start=0x%x, sector count=0x%x ", e,
                    hNandBlkDev->hRegEntry->MinorInstance, SectorNum,
                    NumberOfSectors));
            }
        }
#if FIREFLY_CORRUPT_DEBUG
        else
        {
            NAND_BLK_DEV_ERROR_LOG(("\nNand Write Success "));
        }
#endif
        NvOsMutexUnlock(hNandBlkDev->hDev->LockDev);
    }
    return e;
}

// Nand local function to get device information
static void
NandBlockDevGetDeviceInfo(
    NvDdkBlockDevHandle hBlockDev,
    NvDdkBlockDevInfo* pBlockDevInfo)
{
    NandBlockDevHandle hNandBlkDev = (NandBlockDevHandle)hBlockDev;
    NvDdkNandDeviceInfo DeviceInfo;

    NvOsMutexLock(hNandBlkDev->hDev->LockDev);
    (void)NvDdkNandResume(hNandBlkDev->hDev->hDdkNand);
    if (!hNandBlkDev->hFtl)
    {
        NvU32 Log2NumDevs;
        // This case is for block device driver handles with part_id == 0
        // Since FTL handle does not exist for part_id == 0,
        // using Ddk API to get the parameters to be returned
        NvDdkNandGetDeviceInfo(hNandBlkDev->hDev->hDdkNand, 0, &DeviceInfo);
        Log2NumDevs = NandUtilGetLog2(DeviceInfo.NumberOfDevices);
        pBlockDevInfo->TotalBlocks =
            MACRO_MULT_POW2_LOG2NUM(DeviceInfo.NoOfBlocks, Log2NumDevs);
        // Correct the total block count by Region Table Reserved Area
        pBlockDevInfo->TotalBlocks -=
            (DeviceInfo.NoOfBlocks - s_NandCommonInfo.ChipBlockNum);
        pBlockDevInfo->BytesPerSector = DeviceInfo.PageSize;
        // When interleaved global setting is used sectors per block increases
        pBlockDevInfo->SectorsPerBlock = DeviceInfo.PagesPerBlock;
    }
    else
    {
        // FTL pointer should not be NULL when
        hNandBlkDev->hFtl->NvNandRegionGetInfo(
            hNandBlkDev->hFtl, pBlockDevInfo);
    }
    NvOsMutexUnlock(hNandBlkDev->hDev->LockDev);
}

// Nand local function to power up block driver
static void NandBlockDevPowerUp(NvDdkBlockDevHandle hBlockDev)
{
    NandBlockDevHandle hNandBlkDev = (NandBlockDevHandle)hBlockDev;

    NvOsMutexLock(hNandBlkDev->hDev->LockDev);
    // ignore error if encountered
    (void)NvDdkNandResume(hNandBlkDev->hDev->hDdkNand);
    NvOsMutexUnlock(hNandBlkDev->hDev->LockDev);
}

// Nand local function to power down block driver
static void NandBlockDevPowerDown(NvDdkBlockDevHandle hBlockDev)
{
    NandBlockDevHandle hNandBlkDev = (NandBlockDevHandle)hBlockDev;

    NvOsMutexLock(hNandBlkDev->hDev->LockDev);
    // Flush FTL cache
    hNandBlkDev->hFtl->NvNandRegionFlushCache(hNandBlkDev->hFtl);
    // Power down Nand
    (void)NvDdkNandSuspend(hNandBlkDev->hDev->hDdkNand);
    NvOsMutexUnlock(hNandBlkDev->hDev->LockDev);
}

// Nand local function for block driver flush cache operation
static void NandBlockDevFlushCache(NvDdkBlockDevHandle hBlockDev)
{
    NandBlockDevHandle hNandBlkDev = (NandBlockDevHandle)hBlockDev;

    NvOsMutexLock(hNandBlkDev->hDev->LockDev);

    (void)NvDdkNandResume(hNandBlkDev->hDev->hDdkNand);
    hNandBlkDev->hFtl->NvNandRegionFlushCache(hNandBlkDev->hFtl);
    // Error is never encountered for this step
    NvOsMutexUnlock(hNandBlkDev->hDev->LockDev);
}

static NvError VerifyRegionTable(NandBlockDevHandle hNandBlockDev)
{
    NvError e = NvSuccess;
    NvU8 *pRegionTableFromDisk;
    NvU32 ActualReadSectors;
    NvDdkBlockDevInfo *pDeviceInfo;
    NvS32 CurrBlockNum;
    NvU32 CurrDevNum;
    NandSpareInfo SpareInfo;
    NvBool IsFakeBad;
    NvBool IsGoodBlk;
    NvU32 PageNumbers[MAX_INTERLEAVED_NAND_CHIPS];
    NvU32 k;
    NvU32 i1;
    NvU32 NumErrors =0;

    ActualReadSectors = MACRO_DIV_POW2_LOG2NUM(s_TotalRegTableByteCount,
        hNandBlockDev->hDev->Log2BytesPerPage);
    // Ceil the sector count
    if (MACRO_MULT_POW2_LOG2NUM(ActualReadSectors,
                                    hNandBlockDev->hDev->Log2BytesPerPage) <
                                    s_TotalRegTableByteCount)
    {
        ActualReadSectors++;
    }
    // save region table(region entry link list) for the device
    pDeviceInfo = &(hNandBlockDev->hDev->BlkDevInfo);

    pRegionTableFromDisk = NvOsAlloc(pDeviceInfo->BytesPerSector);
    if (!pRegionTableFromDisk)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    // Initialize buffer created
    NvOsMemset(pRegionTableFromDisk, 0, pDeviceInfo->BytesPerSector);

    if (s_LastGoodBlock < 0)
    {
        NV_CHECK_ERROR_CLEANUP(NandUtilGetLastGoodBlock(hNandBlockDev, &s_LastGoodBlock));
    }
    CurrDevNum = s_NandCommonInfo.LastBankChipNum;
    CurrBlockNum = s_NandCommonInfo.ChipBlockNum;
    if ((s_LastGoodBlock < 0) ||
        ((CurrDevNum == 0) && (CurrBlockNum == 0)))
    {
#if NAND_MORE_ERR_CODES
        e = NAND_ERR_MASK(LOCAL_ERROR_CODE13, NvError_NandRegionTableOpFailure);
#else
        e = NvError_NandRegionTableOpFailure;
#endif
        goto fail;
    }
    // Try to read 1 sector only
    ActualReadSectors = DEFAULT_REGION_TBL_SIZE_TO_READ;
    // start with the assumption that the current block is good
    IsGoodBlk = NV_TRUE;
    for (k =0; k < NAND_REGIONTABLE_REDUNDANT_COPIES; k++)
    {
        do
        {
            NV_CHECK_ERROR_CLEANUP(
                            NandIsGoodBlock(hNandBlockDev,
                            CurrDevNum,
                            CurrBlockNum,
                            &SpareInfo)
                            );
            IsFakeBad = NandUtilIsFakeBad(CurrDevNum, CurrBlockNum);
            IsGoodBlk = (SpareInfo.IsGoodBlock && (!IsFakeBad));
            if (!IsGoodBlk)
            {
                // Try next block
                CurrBlockNum -= 1;
            }
            if (CurrBlockNum < 0)
            {
#if NAND_MORE_ERR_CODES
                e = NAND_ERR_MASK(LOCAL_ERROR_CODE14, NvError_NandRegionTableOpFailure);
#else
                e = NvError_NandRegionTableOpFailure;
#endif
                goto fail;
            }
        }while (!IsGoodBlk);
        // Initialize the Page numbers first since Ddk needs
        // interleaved chip page numbers
        for (i1 = 0; i1 < MAX_INTERLEAVED_NAND_CHIPS; i1++)
            PageNumbers[i1] = s_NandIllegalAddress;
        PageNumbers[CurrDevNum] =
            MACRO_MULT_POW2_LOG2NUM(CurrBlockNum,
            hNandBlockDev->hDev->Log2PagesPerBlock);
        NV_CHECK_ERROR_CLEANUP(NvDdkNandRead(hNandBlockDev->hDev->hDdkNand, CurrDevNum, PageNumbers,
            pRegionTableFromDisk, NULL, &ActualReadSectors, NV_FALSE));
        NumErrors = NvOsMemcmp(s_RegTableGoldenCopy,
                                pRegionTableFromDisk,
                                s_TotalRegTableByteCount);
        if (NumErrors)
        {
#if NAND_MORE_ERR_CODES
            e = NAND_ERR_MASK(LOCAL_ERROR_CODE15, NvError_NandRegionTableOpFailure);
#else
            e = NvError_NandRegionTableOpFailure;
#endif
            NvOsDebugPrintf("\nData mismatch in Copy of Region Table at BlockNum %d",
                            CurrBlockNum);
            break;
        }
        IsGoodBlk = NV_FALSE;
        // Try next block
        CurrBlockNum -= 1;
    }

fail:
    if (pRegionTableFromDisk)
    {
        NvOsFree(pRegionTableFromDisk);
        pRegionTableFromDisk = NULL;
    }
    return e;
}

// Nand local function block driver ioctl operation
static  NvError
NandBlockDevIoctl(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 Opcode,
    NvU32 InputSize,
    NvU32 OutputSize,
    const void *InputArgs,
    void *OutputArgs)
{
    NandBlockDevHandle hNandBlockDev = (NandBlockDevHandle)hBlockDev;
    NvError e = NvSuccess;

#if FIREFLY_CORRUPT_DEBUG
    {
        NvU8 IoctlStr[100];
        UtilGetIoctlName(Opcode, IoctlStr);
        NAND_BLK_DEV_ERROR_LOG(("\nNand Ioctl opcode=%s Entry ", IoctlStr));
    }
#endif
    // Lock before Ioctl
    NvOsMutexLock(hNandBlockDev->hDev->LockDev);

    NV_CHECK_ERROR_CLEANUP(NvDdkNandResume(hNandBlockDev->hDev->hDdkNand));
    // Decode the IOCTL opcode
    switch (Opcode)
    {
        case NvDdkBlockDevIoctlType_ReadPhysicalSector :
        case NvDdkBlockDevIoctlType_WritePhysicalSector:
            // Check if partition create is done with part id == 0
            if (hNandBlockDev->hRegEntry)
            {
                // Error as indicates block dev driver handle is
                // not part id == 0
                e = NvError_BlockDriverNoPartition;
                break;
            }
            e = NandUtilRdWrPhysicalSector(hNandBlockDev, Opcode,
                InputSize, InputArgs);
            break;

        case NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus:
            {
                // Implementation assumes this ioctl call is made
                // in no interleaving mode
                NvDdkBlockDevIoctl_QueryPhysicalBlockStatusInputArgs
                    *pPhysicalQueryIn =
                    (NvDdkBlockDevIoctl_QueryPhysicalBlockStatusInputArgs *)
                    InputArgs;
                NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs
                    *pPhysicalQueryOut =
                    (NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs *)
                    OutputArgs;
                NvU32 ChipNum = 0;
                NvU32 BlockNum;
                NandSpareInfo SpareInfo;
                NV_ASSERT(InputSize ==
                    sizeof(
                    NvDdkBlockDevIoctl_QueryPhysicalBlockStatusInputArgs));
                NV_ASSERT(OutputSize ==
                    sizeof(
                    NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs));
                NV_ASSERT(InputArgs);
                NV_ASSERT(OutputArgs);
                // Check block for good condition
                BlockNum = pPhysicalQueryIn->BlockNum;
                if ((NvBtlGetPba(&ChipNum, &BlockNum) == NV_FALSE) ||
                    (ChipNum > hNandBlockDev->hDev->NandDeviceInfo.NumberOfDevices))
                {
                    e = NvError_InvalidAddress;
                    break;
                }
                // Check block for Good / Bad
                NV_CHECK_ERROR_CLEANUP(NandIsGoodBlock(hNandBlockDev,
                    ChipNum, BlockNum, &SpareInfo));
                pPhysicalQueryOut->IsGoodBlock = SpareInfo.IsGoodBlock;

                // Check block for lock mode
                NV_CHECK_ERROR_CLEANUP(NandIsBlockLocked(hNandBlockDev->hDev->hDdkNand,
                    ChipNum, BlockNum, &(pPhysicalQueryOut->IsLockedBlock)));
            }
            break;

        case NvDdkBlockDevIoctlType_ErasePhysicalBlock:
            {
                // erases physical block
                NvDdkBlockDevIoctl_ErasePhysicalBlockInputArgs
                    *pErasePhysicalBlock =
                    (NvDdkBlockDevIoctl_ErasePhysicalBlockInputArgs *)
                    InputArgs;
                NV_ASSERT(InputSize ==
                    sizeof(NvDdkBlockDevIoctl_ErasePhysicalBlockInputArgs));
                NV_ASSERT(InputArgs);
                e = NandUtilErasePhysicalBlock(hNandBlockDev,
                    pErasePhysicalBlock->BlockNum,
                    pErasePhysicalBlock->NumberOfBlocks);
            }
            break;

        case NvDdkBlockDevIoctlType_LockRegion:
            {
                NvDdkBlockDevIoctl_LockRegionInputArgs *Ip =
                (NvDdkBlockDevIoctl_LockRegionInputArgs *)InputArgs;
                NV_ASSERT(InputSize ==
                    sizeof(NvDdkBlockDevIoctl_LockRegionInputArgs));
                NV_ASSERT(InputArgs);
                // Call API to lock address range
                e = NandUtilLockRegions(hNandBlockDev,
                    Ip->LogicalSectorStart,
                    Ip->NumberOfSectors,
                    Ip->EnableLocks);
            }
            break;

        case NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector:
            {
                NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs
                    *pMapPhysicalSectorIn =
                    (NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs *)
                    InputArgs;
                NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs
                    *pMapPhysicalSectorOut =
                    (NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs *)
                    OutputArgs;
                NV_ASSERT(InputSize ==
                    sizeof(
                    NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs));
                NV_ASSERT(OutputSize ==
                    sizeof(
                    NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs));
                NV_ASSERT(InputArgs);
                NV_ASSERT(OutputArgs);
                // Check if partition create is done with part id == 0
                if (hNandBlockDev->hRegEntry)
                {
                    // Error as indicates block dev driver handle is
                    // not part id == 0
                    e = NvError_BlockDriverNoPartition;
                    break;
                }
                // for partition id == 0, hFtl is NULL
                if (hNandBlockDev->hFtl)
                {
                   e = hNandBlockDev->hFtl->NvNandRegionIoctl(
                        hNandBlockDev->hFtl,
                        Opcode, InputSize, OutputSize, InputArgs, OutputArgs);
                }
                else
                {
                    e = NandUtilLogical2PhysicalSector(hNandBlockDev,
                        pMapPhysicalSectorIn->LogicalSectorNum,
                        &pMapPhysicalSectorOut->PhysicalSectorNum);
                }
            }
            break;

        case NvDdkBlockDevIoctlType_FormatDevice:
            {
                NvDdkBlockDevIoctl_FormatDeviceInputArgs *pFormatDevIn =
                    (NvDdkBlockDevIoctl_FormatDeviceInputArgs *)
                    InputArgs;
                NV_ASSERT(InputSize ==
                    sizeof(NvDdkBlockDevIoctl_FormatDeviceInputArgs));
                NV_ASSERT(InputArgs);
                // Format entire device (can not fail)
                NandUtilFormatDev(hNandBlockDev,
                    pFormatDevIn->EraseType);
            }
            break;

        case NvDdkBlockDevIoctlType_PartitionOperation:
            {
                NvDdkBlockDevIoctl_PartitionOperationInputArgs *pPartOp =
                    (NvDdkBlockDevIoctl_PartitionOperationInputArgs *)
                    InputArgs;
                NV_ASSERT(InputSize ==
                    sizeof(NvDdkBlockDevIoctl_PartitionOperationInputArgs));
                NV_ASSERT(InputArgs);
                // Check if partition create is done with part id == 0
                if (hNandBlockDev->hRegEntry)
                {
                    // Error as indicates block dev driver handle
                    // is not part id == 0
                    e = NvError_BlockDriverIllegalPartId;
                    break;
                }
                // Seems region table in memory should be updated using lock
                if (pPartOp->Operation ==
                    NvDdkBlockDevPartitionOperation_Finish)
                {
                    // Any errors during save are returned to caller
                    e = NandUtilSaveRegionTable(hNandBlockDev);
                }
                else if (pPartOp->Operation ==
                    NvDdkBlockDevPartitionOperation_Start)
                {
                    // clear old region table if existing
                    RegionTableFree(hNandBlockDev->hDev->hHeadRegionList);
                    // Region table head must be reset
                    hNandBlockDev->hDev->hHeadRegionList = NULL;
                }
            }
            break;

        case NvDdkBlockDevIoctlType_AllocatePartition:
            {
                NvDdkBlockDevIoctl_AllocatePartitionInputArgs *pAllocPartIn =
                    (NvDdkBlockDevIoctl_AllocatePartitionInputArgs *)InputArgs;
                NvDdkBlockDevIoctl_AllocatePartitionOutputArgs *pAllocPartOut =
                    (NvDdkBlockDevIoctl_AllocatePartitionOutputArgs *)
                    OutputArgs;
                NV_ASSERT(InputSize ==
                    sizeof(NvDdkBlockDevIoctl_AllocatePartitionInputArgs));
                NV_ASSERT(InputArgs);
                NV_ASSERT(OutputSize ==
                    sizeof(NvDdkBlockDevIoctl_AllocatePartitionOutputArgs));
                NV_ASSERT(OutputArgs);
                // Check if partition create is done with part id == 0
                if (hNandBlockDev->hRegEntry)
                {
                    // Error as indicates block dev driver handle
                    // is not part id == 0
                    e = NvError_BlockDriverIllegalPartId;
                    break;
                }
                // Partition creation happens before NvDdkNandOpen,
                // so no locking needed
                e = NandUtilAllocPart(hNandBlockDev, pAllocPartIn,
                    pAllocPartOut);
            }
            break;

        case NvDdkBlockDevIoctlType_WriteVerifyModeSelect:
            {
                NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgs *
                    pWriteVerifyModeIn =
                    (NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgs *)InputArgs;
                NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgs WrVerifyMode;
                NV_ASSERT(InputArgs);
                hNandBlockDev->IsReadVerifyWriteEnabled =
                    pWriteVerifyModeIn->IsReadVerifyWriteEnabled;
                // set FTL write verify mode
                WrVerifyMode.IsReadVerifyWriteEnabled =
                    hNandBlockDev->IsReadVerifyWriteEnabled;
                NV_CHECK_ERROR_CLEANUP(hNandBlockDev->hFtl->NvNandRegionIoctl(
                                hNandBlockDev->hFtl,
                                NvDdkBlockDevIoctlType_WriteVerifyModeSelect,
                                sizeof(NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgs),
                                0, &WrVerifyMode, NULL));
            }
            break;

        // Nand specific block driver Ioctls
        case NvDdkBlockDevIoctlType_DefineSubRegion:
            {
                NV_ASSERT(InputSize ==
                    sizeof(NvDdkBlockDevIoctl_DefineSubRegionInputArgs));
                NV_ASSERT(InputArgs);
                // Call FTL implementation of define sub region ioctl
                if (hNandBlockDev->hFtl)
                {
                    // Define sub-region caller assumes the entire Nand to
                    // be either completely interleaved or non-interleaved

                    // Need copy as Nand shim reuses InputArgs after return
                    NvDdkBlockDevIoctl_DefineSubRegionInputArgs SubRegionArgs;
                    NvU32 SectorNum;
                    NvOsMemcpy(&SubRegionArgs, InputArgs, sizeof(SubRegionArgs));
                    // FTL expects partition relative addresses,
                    // hence strip off the device relative partition start
                    SectorNum = MACRO_MULT_POW2_LOG2NUM(SubRegionArgs.StartLogicalBlock,
                        (hNandBlockDev->hDev->Log2PagesPerBlock +
                        hNandBlockDev->hDev->Log2InterleaveCount));
                    NV_CHECK_ERROR_CLEANUP(NandUtilGetPartitionRelativeAddr(
                        hNandBlockDev, &SectorNum));
                    // SectorNum returned is relative to partition
                    // hence using partition interleave mode InterleaveBankCount
                    SubRegionArgs.StartLogicalBlock = MACRO_DIV_POW2_LOG2NUM(SectorNum,
                        (hNandBlockDev->hDev->Log2PagesPerBlock +
                        hNandBlockDev->Log2InterleaveCountPart));
                    // block numbers received from Shim should be
                    // interpreted as super block sized
                    SectorNum = MACRO_MULT_POW2_LOG2NUM(SubRegionArgs.StopLogicalBlock,
                        (hNandBlockDev->hDev->Log2PagesPerBlock +
                        hNandBlockDev->hDev->Log2InterleaveCount));
                    NV_CHECK_ERROR_CLEANUP(NandUtilGetPartitionRelativeAddr(
                        hNandBlockDev, &SectorNum));
                    // SectorNum returned is relative to partition
                    // hence using partition interleave mode InterleaveBankCount
                    SubRegionArgs.StopLogicalBlock = MACRO_DIV_POW2_LOG2NUM(SectorNum,
                        (hNandBlockDev->hDev->Log2PagesPerBlock +
                        hNandBlockDev->Log2InterleaveCountPart));

                    if(SubRegionArgs.StartFreeLogicalBlock!=0xFFFFFFFF)
                    {
                        // block numbers received from Shim should be
                        // interpreted as super block sized
                        SectorNum = MACRO_MULT_POW2_LOG2NUM(SubRegionArgs.StartFreeLogicalBlock,
                            (hNandBlockDev->hDev->Log2PagesPerBlock +
                            hNandBlockDev->hDev->Log2InterleaveCount));
                        NV_CHECK_ERROR_CLEANUP(NandUtilGetPartitionRelativeAddr(
                            hNandBlockDev, &SectorNum));
                        // SectorNum returned is relative to partition
                        // hence using partition interleave mode InterleaveBankCount
                        SubRegionArgs.StartFreeLogicalBlock = MACRO_DIV_POW2_LOG2NUM(SectorNum,
                            (hNandBlockDev->hDev->Log2PagesPerBlock +
                            hNandBlockDev->Log2InterleaveCountPart));
                    }

                    // call FTL API to create sub region
                    e = hNandBlockDev->hFtl->NvNandRegionIoctl(
                        hNandBlockDev->hFtl, Opcode, InputSize, 0,
                        &SubRegionArgs, NULL);
                }
                else
                {
                    e = NvError_NotSupported;
                }
            }
            break;

        case NvDdkBlockDevIoctlType_QueryFirstBoot:
            {
                NV_ASSERT(OutputSize ==
                    sizeof(NvDdkBlockDevIoctl_QueryFirstBootOutputArgs));
                NV_ASSERT(OutputArgs);
                // Call FTL implementation of query first boot ioctl
                if (hNandBlockDev->hFtl)
                {
                    e = hNandBlockDev->hFtl->NvNandRegionIoctl(
                        hNandBlockDev->hFtl, Opcode, 0, OutputSize,
                        NULL, OutputArgs);
                }
                else
                {
                    e = NvError_NotSupported;
                }
            }
            break;

        case NvDdkBlockDevIoctlType_DisableCache:
            {
                // Call FTL implementation of disable cache
                if (hNandBlockDev->hFtl)
                {
                    e = hNandBlockDev->hFtl->NvNandRegionIoctl(
                        hNandBlockDev->hFtl, Opcode, InputSize, OutputSize,
                        InputArgs, OutputArgs);
                }
                else
                {
                    e = NvError_NotSupported;
                }
            }
            break;

        case NvDdkBlockDevIoctlType_EraseLogicalSectors:
            {
                NvU32 Log2InterleaveCountPart;
                NV_ASSERT(InputSize ==
                    sizeof(NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs));
                NV_ASSERT(InputArgs);
                Log2InterleaveCountPart = NandUtilGetLog2(
                    hNandBlockDev->hRegEntry->RegProp.InterleaveBankCount);
                // Call FTL implementation of erase partition ioctl
                if (hNandBlockDev->hFtl)
                {
                    NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs *EraseArgs
                        = (NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs*)
                        InputArgs;
                    // FTL expects partition relative addresses,
                    // hence strip off the device relative partition start
                    NV_CHECK_ERROR_CLEANUP(NandUtilGetPartitionRelativeAddr(
                        hNandBlockDev, &EraseArgs->StartLogicalSector));
                    // Check that  erase is within partition limits
                    // except when erasing till end of Nand
                    if ((EraseArgs->NumberOfLogicalSectors != (NvU32)-1) &&
                        (EraseArgs->NumberOfLogicalSectors >
                        (MACRO_MULT_POW2_LOG2NUM(hNandBlockDev->hRegEntry->RegProp.TotalLogicalBlocks,
                        (hNandBlockDev->hDev->Log2PagesPerBlock +
                        // Considering partition interleave setting to
                        // decide on erased limit
                        Log2InterleaveCountPart)))))
                    {
                        e = NvError_NandBlockDriverEraseFailure;
                    }
                    else
                    {
                        // If argument valid call FTL implementation
                        e = hNandBlockDev->hFtl->NvNandRegionIoctl(
                            hNandBlockDev->hFtl, Opcode, InputSize, 0,
                            (void *)EraseArgs, NULL);
                    }
                }
                else
                {
                    e = NvError_NotSupported;
                }
            }
            break;

        case NvDdkBlockDevIoctlType_ForceBlkRemap:
            {
                // Call FTL implementation of ForceBlkRemap ioctl
                if (hNandBlockDev->hFtl)
                {
                    e = hNandBlockDev->hFtl->NvNandRegionIoctl(
                        hNandBlockDev->hFtl, Opcode, 0, 0,
                        NULL, NULL);
                }
                else
                {
                    e = NvError_NotSupported;
                }
            }
            break;

        case NvDdkBlockDevIoctlType_VerifyCriticalPartitions:
            {
                e = VerifyRegionTable(hNandBlockDev);
            }
            break;
        case NvDdkBlockDevIoctlType_GetPartitionPhysicalSectors:
        {
            NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsInputArgs
                *pInputArgs =
                (NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsInputArgs*)
                InputArgs;
            NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsOutputArgs
                *pOutputArgs =
                (NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsOutputArgs*)
                OutputArgs;
            NV_ASSERT(InputArgs);
            NV_ASSERT(InputSize ==
                    sizeof(NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsInputArgs));
            e = GetPartitionPhysicalSector(hNandBlockDev, pInputArgs, pOutputArgs);
        }
            break;
        case NvDdkBlockDevIoctlType_IsGoodBlock:
            if (hNandBlockDev->hFtl)
            {
                // FTL defines the run-time bad interpretation
                e = hNandBlockDev->hFtl->NvNandRegionIoctl(
                    hNandBlockDev->hFtl, NvDdkBlockDevIoctlType_IsGoodBlock,
                    InputSize, OutputSize, InputArgs, OutputArgs);
            }
            else
            {
                // part-id == 0 case operation is not defined
                e = NvError_BlockDriverIllegalIoctl;
            }
            break;

        case NvDdkBlockDevIoctlType_ErasePartition:
            if (hNandBlockDev->hFtl)
            {
                NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs *EraseArgs
                    = (NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs*)
                    InputArgs;
                // Call partition erase
                e = NandUtilErasePartition(hNandBlockDev,
                    EraseArgs->StartLogicalSector,
                    EraseArgs->NumberOfLogicalSectors);
            }
            else
            {
                // part-id == 0 case operation is not defined
                e = NvError_BlockDriverIllegalIoctl;
            }
            break;

        case NvDdkBlockDevIoctlType_UnprotectAllSectors:
        case NvDdkBlockDevIoctlType_ProtectSectors:
            e = NvError_NotImplemented;
            break;

        default:
            e = NvError_BlockDriverIllegalIoctl;
            break;
    }


fail:
    if (e != NvSuccess)
    {
        NvU8 IoctlStr[100];
        UtilGetIoctlName(Opcode, IoctlStr);
        // Failed last ioctl call
        NAND_BLK_DEV_ERROR_LOG(("\nNand Block dev ioctl opcode=%s error 0x%x ",
            IoctlStr, e));
    }
#if FIREFLY_CORRUPT_DEBUG
    else
    {
        NvU8 IoctlStr[100];
        UtilGetIoctlName(Opcode, IoctlStr);
        NAND_BLK_DEV_ERROR_LOG(("\nNand Ioctl opcode=%s Success ", IoctlStr));
    }
#endif
    // unlock at end of ioctl
    NvOsMutexUnlock(hNandBlockDev->hDev->LockDev);
    return e;
}

/********************************************/
/* Start Device Manager visible NAND Block  */
/* device driver Interface definitions      */
/********************************************/
// Nand Global block driver open API. This is called by the
// Device Manager as a function pointer.
NvError
NvDdkNandBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev)
{
    NvError e = NvSuccess;
    NandBlockDevHandle hNandBlockDev;

    *phBlockDev = NULL;
    // open Nand to get handle for partition
    // NandBlockDevRec contains the NvDdkBlockDevHandle as first element
    // It has additional data which are used as needed using
    // NandBlockDevHandle type
    // When need to access block device driver functionality
    // use NvDdkBlockDevHandle type
    NV_CHECK_ERROR_CLEANUP(NandBlockDevOpen(Instance,
        MinorInstance, &hNandBlockDev));

    // block dev handle for first partition on a device is part of allocated
    // instance NandBlockDevHandle, one for each device
    hNandBlockDev->BlockDev.NvDdkBlockDevClose = &NandBlockDevClose;
    hNandBlockDev->BlockDev.NvDdkBlockDevGetDeviceInfo =
        &NandBlockDevGetDeviceInfo;
    hNandBlockDev->BlockDev.NvDdkBlockDevReadSector = &NandBlockDevReadSector;
    hNandBlockDev->BlockDev.NvDdkBlockDevWriteSector =
        &NandBlockDevWriteSector;
    hNandBlockDev->BlockDev.NvDdkBlockDevPowerUp = &NandBlockDevPowerUp;
    hNandBlockDev->BlockDev.NvDdkBlockDevPowerDown = &NandBlockDevPowerDown;
    hNandBlockDev->BlockDev.NvDdkBlockDevRegisterHotplugSemaphore = NULL;
    hNandBlockDev->BlockDev.NvDdkBlockDevIoctl = &NandBlockDevIoctl;
    hNandBlockDev->BlockDev.NvDdkBlockDevFlushCache = &NandBlockDevFlushCache;
    (*phBlockDev) = &hNandBlockDev->BlockDev;
fail:
    return e;
}

// Nand Global block driver init API. This is called by the
// Device Manager as a function pointer.
NvError
NvDdkNandBlockDevInit(NvRmDeviceHandle hDevice)
{
    NvError e = NvSuccess;
    NvU32 i;
    if (!s_NandCommonInfo.IsBlockDevInitialized)
    {
        // Initialize the global state including creating mutex
        // The mutex to lock access

        // Get Nand device instances available
        s_NandCommonInfo.MaxInstances =
            NvRmModuleGetNumInstances(hDevice, NvRmModuleID_Nand);
        // Allocate table of NvNandRegionHandle, each entry in this table
        // corresponds to a device
        s_NandCommonInfo.NandDevStateList = NvOsAlloc(
            sizeof(NandDevHandle) * s_NandCommonInfo.MaxInstances);
        if (!s_NandCommonInfo.NandDevStateList)
            return NvError_InsufficientMemory;
        NvOsMemset((void *)s_NandCommonInfo.NandDevStateList, 0,
            (sizeof(NandDevHandle) * s_NandCommonInfo.MaxInstances));

        s_NandCommonInfo.NandDevLockList = NvOsAlloc(
            sizeof(NvOsMutexHandle) * s_NandCommonInfo.MaxInstances);
        if (!s_NandCommonInfo.NandDevLockList)
            return NvError_InsufficientMemory;
        NvOsMemset((void *)s_NandCommonInfo.NandDevLockList, 0,
            (sizeof(NandDevHandle) * s_NandCommonInfo.MaxInstances));
        // Allocate the mutex for locking each device
        for (i = 0; i < s_NandCommonInfo.MaxInstances; i++)
        {
            e = NvOsMutexCreate(&(*(s_NandCommonInfo.NandDevLockList + i)));
            if (e != NvSuccess)
                return e;
        }

        // Save the RM Handle
        s_NandCommonInfo.hRmNand = hDevice;
        s_NandCommonInfo.IsBlockDevInitialized = NV_TRUE;
        // Set Nand default erase type
        s_NandCommonInfo.NandEraseType =
            NvDdkBlockDevEraseType_NonFactoryBadBlocks;
    }
    // increment nand block dev driver init ref count
    s_NandCommonInfo.InitRefCount++;
    return e;
}

// Nand Global block driver deinit API. This is called by the
// Device Manager as a function pointer.
void
NvDdkNandBlockDevDeinit(void)
{
    NvU32 i;
    if (s_NandCommonInfo.IsBlockDevInitialized)
    {
        if (s_NandCommonInfo.InitRefCount == 1)
        {
            // this is a saved copy of region table used for verification
            if (s_RegTableGoldenCopy)
                NvOsFree(s_RegTableGoldenCopy);
            // Dissociate from Rm Handle
            s_NandCommonInfo.hRmNand = NULL;
            // Free allocated list of handle pointers
            // free mutex list
            for (i = 0; i < s_NandCommonInfo.MaxInstances; i++)
            {
                NvOsFree((*(s_NandCommonInfo.NandDevStateList + i)));
                (*(s_NandCommonInfo.NandDevStateList + i)) = NULL;
                NvOsMutexDestroy((*(s_NandCommonInfo.NandDevLockList + i)));
            }
            NvOsFree(s_NandCommonInfo.NandDevLockList);
            s_NandCommonInfo.NandDevLockList = NULL;
            // free device instances
            NvOsFree(s_NandCommonInfo.NandDevStateList);
            s_NandCommonInfo.NandDevStateList = NULL;
            // reset the maximum Nand controller count
            s_NandCommonInfo.MaxInstances = 0;
            s_NandCommonInfo.IsBlockDevInitialized = NV_FALSE;
        }
        // decrement nand dev driver init ref count
        s_NandCommonInfo.InitRefCount--;
    }
}


