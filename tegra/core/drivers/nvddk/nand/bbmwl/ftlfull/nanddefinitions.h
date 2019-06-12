/**
 * @file NandDefinitions.h
 *
 * @brief A sector cache.
 */

/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NAND_DEFINITIONS_H
#define INCLUDED_NAND_DEFINITIONS_H

#include "nvrm_memmgr.h"
#include "nvassert.h"
#include "nvodm_query_nand.h"
#include "../nvnandregion.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define USE_OLD_FLUSH_LOGIC 0

// Check the .cfg percent reserved against this limit
#define FTL_DATA_PERCENT_RSVD_MAX 25


typedef struct NvNandRec *NvNandHandle;

void *
AllocateVirtualMemory(NvU32 SizeInBytes, NvU32 keyword);

void
ReleaseVirtualMemory(void* pVirtualAddr, NvU32 keyword);

//maximum subregions FTL full can support
enum {MAX_NAND_SUBREGIONS = 3};

// Memory alloc keywords.
typedef enum MemAllocKeyword
{
    TAT_ALLOC = 0,
    TT_ALLOC,
    SECTOR_CACHE_ALLOC,
    STRATEGY_ALLOC,
    FTL_ALLOC,
    TL_ALLOC
}MemAllocKeyword;

// This is enumeration for the type of NAND Flash.
typedef enum NandFlashType
{
    // Nand flash type is not known
    UNKNOWN_NAND_TYPE = 0,
    // Nand flash is of type SLC
    SLC,
    // Nand flash is of type MLC
    MLC
}NandFlashType;

typedef enum TTTableValues
{
    // Total number of TT pages that can be cached.
    TOTAL_NUMBER_OF_TT_CACHE_PAGES = 16,
    MAX_NAND_REGIONS = 3
}TTTableValues;

// This describes the status of the complete block.
// Important Note: This size of the structure should be divisible to page size.
typedef struct BlockStatusEntryRec
{
    // this is the Physical block associated for the Logical block
    NvU32 PhysBlkNum:24;
    // this block is reserved for Translation Table (TT)
    NvU32 TtReserved : 1;
    // this block is reserved for Translation table Allocation Table (TAT)
    NvU32 TatReserved : 1;
    // this block is part of system reserved blocks
    NvU32 SystemReserved : 1;
    // this block is part of bad data block management reserved blocks
    NvU32 DataReserved : 1;
    // this is to flag if the block is good/bad, 1: Good and 0: Bad
    NvU32 BlockGood : 1;
    // this is to flag if the block is used/unused, 1: Unused and 0: Used
    NvU32 BlockNotUsed : 1;
    // This two bits are for storing region info
    NvU32 Region : 2;
}BlockStatusEntry;

// Device information form nand flash
typedef struct NandDeviceInfoRec
{
    // Device Maker code
    NvU8 MakerCode;
    // Device code
    NvU8 DeviceCode;
    // Device type
    NandFlashType DeviceType;
    // Total Device capacity in KiloBytes, includes only data area,
    // no redundant area
    NvS32 DeviceCapacity;
    // Holds the total number of blocks that are present in the Nand flash device
    NvS32 NoOfPhysBlks;
    // Holds the total number of zones that are present in the Nand flash device
    NvS32 NoOfZones;
    // Page size in bytes, includes only data area, no redundant area
    short PageSize;
    // Block Size in bytes, includes only data area, no redundant area
    NvS32 BlkSize;
    // Number of Pages/Block
    short PgsPerBlk;
    // Sector Size in bytes
    short SectorSize;
    // Number of Sectors/Page
    short SctsPerPg;
    // Number of Sectors/Block
    short SectorsPerBlock;
    // Redundant area size/Sector
    NvS8 RendAreaSizePerSector;
    // Interface bus width
    NvS8 BusWidth;
    // Minimum serial access time
    NvS8 SerialAccessMin;
    // Number of Nand flash devices present on the board.
    NvU8 NumberOfDevices;

    NvOdmNandInterleaveCapability InterleaveCapabililty;
}NandDeviceInfo;

enum
{
    // free flag
    FREE = 0,
    // busy flag
    BUSY
};

typedef enum NvIoctlCommands
{
    READ_PHYSICAL_SECTOR,
    WRITE_PHYSICAL_SECTOR,
}NvIoctlCommands;

// private information defination is in TT
// Structure to track write operations on a set of eight sectors of a block

typedef struct PageStateRec
{
    NvS8 state;
    NvS32 PageNumber;
}PageInfo;

/**
* @class   TrackLba
* @brief   This class is used to keep track of free and busy pages
* in a logical block, which is recently accessed.
*
*/
typedef struct TrackLba TrackLba;
struct TrackLba
{
    // logical block address
    NvS32 lba;
    // pointer to physical blocks of lba.
    NvS32* pBlocks;
    // pointer to physical blocks of lba.
    NvS32* pPrevBlocks;
    // Flag that indicates whether it is in use.
    volatile NvBool inUse;
    // lba accessed time
    NvU64 LastAccessTick;
    // this is hold the state of a page during write operation
    // and to avoid copybacks in case, we are writing into free pages.
    PageInfo * pPageState;
    // Holds the last free page number.
    NvS32 LastFreePage;
    #if !USE_OLD_FLUSH_LOGIC
    // Holds the last free page number.
    NvS32 FirstFreePage;
    // This will be true if the write request is random.
    NvBool IsWriteRequestRandom;
    #endif
    // number of interleaved banks.
    NvS32 NoOfInterleavedBanks;
    // number of physical pages in logical block
    NvS32 PgsPerLBlk;
    // Nand device information
    NandDeviceInfo *NandDevInfo;
    // This holds the number of bits to be shifted left/right to get the
    // multiplication/division with NoOfInterleavedBanks value.
    NvS32 bsc4NoOfInterleavedBanks;
    // This holds the number of bits to be shifted left/right to get the
    // multiplication/division with NoOfInterleavedBanks value.
    NvS32 bsc4SctsPerPg;
};

typedef struct NandStrategyInterleave
{
    // Nand flash device info
    NandDeviceInfo NandDevInfo;
    // number of interleaved banks
    NvS32 NoOfILBanks;
    // number of banks on the board
    NvS32 NumberOfBanksOnboard;
    // sectors per logical block
    NvS32 SecsPerLBlk;
    // pages per logical block
    NvS32 PgsPerLBlk;
    // sectors per logical page
    NvS32 SctsPerLPg;
    // pointer to physical page array
    NvS32* pTmpPhysPgs;
    // pointer to physical page array
    NvS32* pCopybackSrcPages;
    // pointer to physical page array
    NvS32* pCopybackDstnPages;
    // pointer to physical block array
    NvS32* pTempBlocks;
    // pointer to physical block array
    NvS32* pPhysBlks;
    // pointer to physical block array
    NvS32* pPrevPhysBlks;
    // pointer to track lba objects
    TrackLba* pTrackLba;
    // This says whether ordering is applicable or not.
    // For MLC it is applicable and for SLC it is not applicable.
    NvBool IsOrderingValid;
    // tag info
    TagInformation TagInfo;
    // tag info page number
    NvS32 TagInfoPage;
    // number of lba's to track
    NvS32 NoOfLbas2Track;
    // This holds the number of bits to be shifted left/right to get the
    // multiplication/division with PgsPerBlk value.
    NvS32 bsc4PgsPerBlk;
    // This holds the number of bits to be shifted left/right to get the
    // multiplication/division with PgsPerLBlk value.
    NvS32 bsc4PgsPerLBlk;
    // This holds the number of bits to be shifted left/right to get the
    // multiplication/division with SecsPerLBlk value.
    NvS32 bsc4SecsPerLBlk;
    // This holds the number of bits to be shifted left/right to get the
    // multiplication/division with SctsPerLPg value.
    NvS32 bsc4SctsPerLPg;
    // This holds the number of bits to be shifted left/right to get the
    // multiplication/division with SctsPerPg value.
    NvS32 bsc4SctsPerPg;
    // This holds the number of bits to be shifted left/right to get the
    // multiplication/division with NoOfILBanks value.
    NvS32 bsc4NoOfILBanks;
    // physical blocks per zone
    NvS32 PhysBlksPerZone;
    // Holds the status of current tt opertion performed.
    NvS32 ttStatus;
    // Holds the status of Nand Strategy.
    NvError selfStatus;
}NandStrategyInterleave;

// structure to hold the block related information
typedef struct BlockInfoRec
{
    // Holds the bank number.
    NvS32 BnkNum;
    // Holds the block number in the specified bank number.
    NvS32 BlkNum;
}BlockInfo;

typedef struct MiscellaneousInfoRec
{
    // This is total number of banks on board
    NvU32 NumOfBanksOnBoard;
    // Number of banks to use for interleaving
    NvU32 NoOfILBanks;
    // This are number of physical block per bank
    NvU32 PhysBlksPerBank;
    // Number of zones per bank
    NvU32 ZonesPerBank;
    // This is physical blocks per zone
    NvU32 PhysBlksPerZone;
    // Number of physical blocks per logical block.
    NvU32 PhysBlksPerLogicalBlock;
    // blocks reserved for TAT management
    NvS32 tatLogicalBlocks;
    // blocks reserved for TT management
    NvS32 ttLogicalBlocks;
    // blocks rserved for ware leveling/ bad block management
    NvS32 bbmwlDataRservedBlocks;
    // blocks rserved for TAT/TT/FBB
    NvS32 bbmwlSystemRservedBlocks;
    // total blocks reserved by BBMWL driver, this includes TAT, TT,
    // FBB, data reserved
    NvS32 ftlReservedLblocks;

    NvU32 RemainingDataRsvdBlocks;
    NvU32 TotalPhysicalBlocks;

    // Total number of logical block from all the banks
    NvU32 TotalLogicalBlocks;
    // TotEraseBlks is erase blocks available in all the device.
    NvU32 TotEraseBlks;
    // Total blocks reserved for TT
    NvS32 NumOfBlksForTT;
    // Number of pages required to store the translation table
    NvS32 PgsRegForTT;
    // Number of blocks required for TT
    NvS32 BlksRequiredForTT;
    // Number of pages allocated for the translation table management.
    NvS32 PgsAlloctdForTT;
    // Number of translation pages required to store a zone
    NvU32 TtPagesRequiredPerZone;
    // Total blocks reserved for TAT
    NvS32 NumOfBlksForTAT;
    // This gives the total extra pages we have in the block, or accross
    // the block, used for TT page replacements
    NvS32 ExtraPagesForTTMgmt;
    // last page used by TT (i.e.relative physical address)
    NvS32 LastTTPageUsed;
    // This gives the set of the TAT blocks that are used currently to
    // store TAT among the total blocks available
    NvS32 CurrentTatLBInUse;
    // This holds the number of bits to be shifted left/right to get the
    // multiplication/division with PgsPerBlk value.
    NvS32 bsc4PgsPerBlk;
    // This holds the number of bits to be shifted left/right to get the
    // multiplication/division with NoOfILBanks value.
    NvS32 bsc4NoOfILBanks;
    // This holds the number of bits to be shifted left/right to get the
    // multiplication/division with PgsPerBlk value.
    NvS32 bsc4SctsPerPg;
    // This holds the number of bits to be shifted left/right to get the
    // multiplication/division with sectorSize value.
    NvS32 bsc4SectorSize;
    // Holds the tag info page number. for SLC it will be the starting
    // page of block and for MLC it will be the ending page of block.
    // Tag info page contains the info about the block.
    NvS32 TagInfoPage;
    // this is the page number from which we can use the
    // following pages in a sys block.
    NvS32 SysRsvdBlkStartPage;
    // this is the page number above which thepages in a sys block can be used.
    NvS32 SysRsvdBlkEndPage;
    // Holds the number of blocks that can be used in the system rsvd blocks
    NvS32 PagesPerSysRsvdBlock;
}MiscellaneousInfo;

// This is Translation table Allocation Block (TAT).
typedef struct TranslationAllocationTableRec
{
    // this is the header signature to identify the start of Translation Table
    // Signature is "_PPI_IL_TAT_START_"
    // This will be at first sector of the TT page.
    NvS8* headerSignature;
    // TAT Revision Number used to know the latest TAT page.
    NvU32* RevNum;
    // TAT start block
    BlockInfo* TatStBlk;
    // TAT end block
    BlockInfo* tatEndBlock;
    // TT start block
    BlockInfo* ttStartBlock;
    // TT end block
    BlockInfo* ttEndBlock;
    // the maximum page required is 384, as per the Translation table size.
    NvU16* TtAllocPg;
    // this is to identify the block numbers contains the translation table
    BlockInfo*  TtAllocBlk;
    // the last block used in NAND FLASH, this is for wear level
    NvS32* lastPhysicalBlockUsed;
    // Last used TT block.
    BlockInfo* lastUsedTTBlock;
    // this is the footer signature. The signature is "TEND"
    NvS8* footerSignature;
    // total number of blocks needed for TT
    NvS32 TtBlkCnt;
    // total number of pages needed for TT
    NvS32 TtPageCnt;
    // page number of the current TAT
    NvS32 TatPgNum;
    // physical block number of the current TAT block
    BlockInfo TatBlkNum;
    // page number of the next free TAT location
    NvS32 NextTatPgNum;
    // physical block number of the next free TAT location
    BlockInfo NextTatBlkNum;
    // used to know wether the cached/working TAT is modified/dirty
    NvBool IsDirty;
    // size of a sector in a nand block
    NvS32 SectorSize;
}TranslationAllocationTable;

typedef struct TatHandlerRec
{
    BlockInfo* tatBlocks;
    BlockInfo* ttBlocks;
    TranslationAllocationTable WorkingCopy;
    NvS8* wBuffer;
}TatHandler;

typedef struct NandInterleaveTATRec
{
    // Miscellaneous info
    MiscellaneousInfo Misc;
    NvBool tatHandletInitialized;
    TatHandler tatHandler;
    // pointer to bad blocks per zone array.
    NvS8* pBadBlkPerZone;
    // this will point to a array of size 'numberOfBanksOnBoard'
    NvS32* pPhysPgs;
    // this will point to a array of size 'numberOfBanksOnBoard'
    NvS32* pWritePhysPgs;
    // NandEntree operation status.
    NvError NEStatus;
    // Flag indicating that the TAT revision Number is about to roll over.
    NvBool RevNumRollOverAboutToHappen;
    // Max number of bad pages
    NvU16 MaxNoOfBadPages;
}NandInterleaveTAT;

// structure holding the translation table cached page info
typedef struct BucketRec
{
    // Tells whether the cache bucket is dirty.
    NvBool IsDirty;
    // Holds the last accessed time.
    NvU64 LastAccessTick;
    // logical page number of TT, whose data it is holding.
    NvS32 LogicalPageNumber;
    // pointer the TT page buffer.
    NvS8 *ttPage;
    // Physical pointer of TT page buffer.
    NvS8* ttPagePhysAdd;
}Bucket;

// Translation table cache
typedef struct TranslationTableCacheRec
{
    // Array of buckets
    Bucket bucket[TOTAL_NUMBER_OF_TT_CACHE_PAGES];
    // Holds the number of unused cache buckets availble.
    NvS32 FreeCachedBkts;
}TranslationTableCache;

#if defined(__cplusplus)
}
#endif

#endif

