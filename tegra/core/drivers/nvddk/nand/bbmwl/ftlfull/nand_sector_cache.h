/**
 * @file NandSectorCache.h
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

#ifndef INCLUDED_NAND_SECTOR_CACHE_H
#define INCLUDED_NAND_SECTOR_CACHE_H

#include "nvos.h"
#include "nvrm_memmgr.h"
#include "nanddefinitions.h"


// The state of a sector in the cache.
typedef enum State
{
    // This entry is free.
    State_UNUSED,
    // This entry is in use, and is coherent with its sector on the media.
    State_CLEAN,
    // This entry is in use, and is not coherent with its sector on the media.
    State_DIRTY,

    State_Force32 = 0x7FFFFFFF
}State;

// An anonymous enumeration for constants.
enum
{
    // The number of lines to cache.
    CACHE_LINES = 32,
    // Maximum sectors to be cached
    MAX_SECTORS_TO_CACHE = 1
};


typedef struct CacheLine CacheLine;

/**
 * A management structure for sectors
 */
typedef struct SectorRec
{
    // The sector data.
    NvS32 *data;

    NvRmPhysAddr dataPhysAddr;

    // The logical block address of the sector.
    NvU32 address;

    // The cache line this sector belongs to.
    CacheLine *line;
}Sector;

/**
 * A management structure for cache lines.
 */
struct CacheLine
{
    // The line data.
    NvS32 *data;

    // The sectors in this line.
    Sector sector;

    // The logical block address of the starting sector.
    NvU32 address;

    // The logical unit number of the device that holds these sectors.
    NvS32 lun;

    // The last time this line was accessed.
    // Note that rollover is not handled well.
    NvU64 LastAccessTick;

    // Maintain state of sectors making a Nand Page
    State *SubState;
    // The state of this line.
    State state;
};

/**
 * A Sector cache.  This cache implements an LRU (Least-recently used)
 * fill policy and uses a copyback strategy for maintaining cache coherency.
 */
typedef struct NandSectorCacheRec
{
    // The sector and sector metadata storage.
    CacheLine lines[CACHE_LINES];

    // Indicates whether the Media is write protected.
    // if this is TRUE, indicates that the media is write protected.
    NvBool writeProtected;

    // This falg indicates whether media is tested for the writeprotected state.
    // If this is TRUE, indicates that the media is tested for
    // writeProtectedness and the flag writeProtected state is valid.
    NvBool testedForWriteProtection;

    // The address of the allocated memory for sector data.
    NvS32* sectorData;

    //nand device properties
    NandDeviceInfo  DeviceInfo;

    // Number of sectors making a Nand Page - log2 value
    NvU32 Log2SectorsPerPage;
    // Represents the application viewed sector size. Default value 512 bytes - log2 value
    NvU32 Log2AppBytesPerSector;
    // log2 of pages per block
    NvU32 Log2PagesPerBlock;
    // Buffer used to read Page sized data before updating 
    // with data from sectors dirty in cache.
    NvU8 *ReadData;
}NandSectorCache;

/**
 * Flushes the cache.
 * @param invalidate  If this is true, the cache entries are also invalidated.
 * If this is false, the cache entries remain valid.
 *
 * @return Status of the operation.
 * for a list of error codes.
 *
 */
NvError NandSectorCacheFlush(NvNandHandle hNand, NvBool invalidate);

/**
 * Reads data from the media.
 *
 * @param lun The logical unit number of the media being accessed.
 * @param sectorAddr The logical block address (LBA)
 * of the sector being accessed.
 * @param data The buffer for reading the data.
 * It will have a length of SECTOR_SIZE bytes.
 * @param sectorCount The number of sectors being written.
 *
 * @return Status of the operation.
 * for a list of error codes.
 *
 */
NvError NandSectorCacheRead(
    NvNandHandle hNand,
    NvS32 lun,
    NvU32 sectorAddr,
    NvU8 *data,
    NvS32 sectorCount);

/**
 * Writes data to the media.
 *
 * @param lun The logical unit number of the media being accessed.
 * @param sectorAddr The logical block address (LBA)
 * of the sector being accessed.
 * @param data The buffer for writing the data.
 * It will have a length of SECTOR_SIZE bytes.
 * @param sectorCount The number of sectors being written.
 *
 * @return Status of the operation.
 * for a list of error codes.
 *
 */
NvError NandSectorCacheWrite(
    NvNandHandle hNand,
    NvS32 lun,
    NvU32 sectorAddr,
    NvU8 *data,
    NvS32 sectorCount);

/**
 * Returns whether the sector is in the cache.
 *
 * @param lun The logical unit number of the media being accessed.
 * @param sectorAddr The logical block address (LBA)
 * of the sector being accessed.
 * @param sectorCount The number of sectors being accessed.
 *
 * @return a pointer to the sector, if it exists in the cache.
 */
Sector* NandSectorCacheGetSector(
    NvNandHandle hNand,
    NvS32 lun,
    NvU32 sectorAddr,
    NvS32 sectorCount);

/**
 * Writes a sector to the cache.
 *
 * @param sector A pointer to the sector.
 * @param data The buffer for writing the data. SECTOR_SIZE bytes will be copied.
 * @param StartOffset is sector offset in Nand Page where data is written
 * @param SectorCount is sector count within page
 *
 * @return Status of the operation.
 * for a list of error codes.
 *
 */
NvError NandSectorCacheSetSectorData(
    NvNandHandle hNand,
    Sector* sector,
    NvU8 *data, NvU32 StartOffset, NvU32 SectorCount);

// API to set CacheLine structure variables to its defaults
void CacheLineInit(NvNandHandle hNand, CacheLine* cacheLine);

#if defined(__cplusplus)
extern "C"
{
#endif  /* __cplusplus */
    /**
     * @param writer The WriteStrategy instance to handle writes
     * back to the media.
     */
    void NandSectorCacheInit(NvNandHandle hNand);
#if defined(__cplusplus)
}
#endif  /* __cplusplus */

#endif// INCLUDED_NAND_SECTOR_CACHE_H

