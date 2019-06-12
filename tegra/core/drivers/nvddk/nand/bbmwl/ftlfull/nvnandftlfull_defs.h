/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
/**
 * @file nvnandftlfull_defs.h
 *
 * Public definitions for FTL full driver interface.
 *
 */
 #ifndef INCLUDED_NV__NAND_FTL_FULL_DEF_H_
#define INCLUDED_NV__NAND_FTL_FULL_DEF_H_

#include "nvddk_nand.h"

// Macro to disable read sector cache 
#ifndef DISABLE_READ_SECTOR_CACHE
#define DISABLE_READ_SECTOR_CACHE 1
#endif

#define MAX_REGIONS 3

typedef struct NvNandTtRec *NvNandTtHandle;

typedef struct NandSectorCacheRec *SectorCache;

/**
 * @class   HwNand
 * @brief   The HwNand class provides an interface between file system
 * and low level NAND device driver
 *
 */
typedef struct NvDdkNandTlRec
{
    // Gives the init status
    NvBool IsInitialized;
    // Pointer to Low Level NAND Driver
    // NandDriver* pNandDriver;
    // nand read write parameters control
    // NandReadWriteParamsControl rwParamControl;
    // nand standby control
    // NandStandbyControl standbyControl;
    // number of interleave banks
    NvS32 InterleaveBanks;
    // pointer to the Tag Info Buffer
    NvU32* pTagInfoBuffer;
    // Buffer to partial page
    NvU8* pPartialPageBuffer;

    NvS32 PageNumbersResLock[4];

    NvS32 PageOldNumbersResLock[4];
    // error info
    // NandErrorInfo::ErrorInfo errorInfo;
    // error info
    // NandErrorInfo::ErrorInfo errorInfo2;
    // Holds the number of devices that are present
    NvS32 NumberOfDevices;
    // Total Physical blocks available in all devices
    NvS32 TotalPhysicalBlocks;
    // Total logical blocks available in devices
    NvS32 TotalLogicalBlocks;
    // Total Device capacity
    NvS32 TotalDeviceCapacityInKB;
    // A pointer to a drive idle timer.  This timer is used
    // to shut the drive down after a period of inactivity.
    // DriveIdleTimer* driveIdleTimer;
    // Tells whether the clock is already enabled or not.
    NvBool IsclockEnabled;
    // Holds the Nand Strategy status
    NvError StratStatus;
    // Object to used to cache single/double sector read/write calls to nand
    SectorCache SectorCache;

    //taginfo structure to read/write user info
    TagInformation TagInfoBuffer;

}NvDdkNandTl;

typedef struct NandConfigurationRec
{
    // Block Configuration
    BlockConfiguration BlkCfg;
    // Nand Device Info
    NandDeviceInfo NandDevInfo;
    // Number of Physical Devices present
    NvS32 NoOfPhysDevs;
    NvBool AllowTranslationForSinglePlane;
    NvS32 PhysicalPagesPerZone;
    NvBool IsInitialized;
    NvS8 DeviceLocations[MAX_NAND_SUPPORTED];
}NandConfiguration;

typedef struct NvNandRec
{
    NvS32 FtlStartLba;
    NvS32 FtlEndLba;
    NvS32 FtlStartPba;
    NvS32 FtlEndPba;
    NvS32 InterleaveCount;
    NvBool IsFirstBoot;
    NvS32 RegionCount;
    NvDdkNandTl NandTl;

    //defines whether cache operations on FTL are enabled or not
    NvBool IsCacheEnabled;

    // pointer to Nand Strategey
    NandStrategyInterleave NandStrat;

    NvNandTtHandle pNandTt;
    // Translation allocation table interface
    NandInterleaveTAT ITat;

    // Factory babdBlock table holder
    NvU32* FbbPage;
    NvS32 CachedFbbPage;

    // temp buffer of page size
    NvU8* TmpPgBuf;

    // temp buffer of Tag size
    NvU8* pTagBuffer;

#if DISABLE_READ_SECTOR_CACHE
    // buffer of page size used to enable 512B sector support when cache is disabled
    NvU8* BufSector512B;
#endif

    // Poniter to nand device info
    NandDeviceInfo NandDevInfo;

    // Nand device handle
    NvDdkNandHandle hNvDdkNand;
    // Memory handle
    NvOsSemaphoreHandle gs_SemaId;
    NvDdkNandDeviceInfo gs_NandDeviceInfo;
    NvBool NandFallBackMode;
    NandConfiguration NandCfg;
    NvBool MultiPageReadWriteOPeration;
    NvBool ValidateWriteOperation;
    LockParams NandLockParams[MAX_NAND_SUPPORTED];
    // Read verify enable
    NvBool IsReadVerifyWriteEnabled;
    NvU32 PercentReserved;
    // Structure containing the pointers to the functions needed to perform any 
    // operations on the Nand Flash.
    NvDdkNandFunctionsPtrs DdkFuncs;
    // Storing region number for physical blocks in GetPBA call
    NvU32 RegNum[MAX_ALLOWED_INTERLEAVE_BANKS];
    // Track number of region2 used compaction blocks 
    NvU32 UsedRegion2Count;
    // Flag used to bypass WM specific code for CE6
    NvBool IsNonZeroRegion;
    // Buffer to hold the spare area
    NvU8* pSpareBuf;
}NvNand;

typedef struct NvNandTtRec
{
    // TT table variables
    TranslationTableCache ttCache;
    NvU8* pTTCacheMemory;
    // Physical block address which is Recently allocated to a LBA
    NvS32 *lastUsedPba;
    // this will point to a array of size "numberOfBanksOnBoard"
    NvS32* pPhysPgs;
    // tt entries per one cache page
    NvS32 ttEntriesPerCachePage;
    // This holds the number of bits to be shifted left/right to get
    // the multiplication/division with ttEntriesPerCachePage value.
    NvS32 bsc4ttEntriesPerCachePage;
    // This holds the number of bits to be shifted left/right to get
    // the multiplication/division with PhysBlksPerZone value.
    NvS32 bsc4PhysBlksPerZone;
    // Hold the interleave bank count
    NvS32 NoOfILBanks;
    // tells whether the TT need to be flushed.
    volatile NvBool bFlushTTable;
    // pointer to zones array
    NvS32* pZones;
    // pointer to array holding the block type
    NvBool* pIsNewBlockReserved;
    // variables to hold the tt page numbers of allocated
    // free blocks from each bank.
    NvS32* pTTPages;
    // variables to hold the tt entry numbers of allocated
    // free blocks from each bank.
    NvS32* pEntries;
    // Variable to hold TAT operation status.
    NvError tatStatus;
}NvNandTt;

#endif//INCLUDED_NV__NAND_FTL_FULL_DEF_H_
