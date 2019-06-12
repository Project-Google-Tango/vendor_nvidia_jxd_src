/**
 * @file SectorCache.cpp
 *
 * @brief A sector cache.
 *
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

#include "nand_sector_cache.h"
#include "nvnandftlfull.h"
#include "nvassert.h"
#include "nvos.h"
#include <string.h>
#include "nvnandtlfull.h"
#include "nand_debug_utility.h"
#include "nvodm_query.h"
#include "nand_ttable.h"

//#define CACHE_DEBUG
#ifdef CACHE_DEBUG
    #define SC_DEBUG_INFO(X) NvOsDebugPrintf X
#else
    #define SC_DEBUG_INFO(X)
#endif

// Data type used to return calculated section boundaries
typedef struct _TagNandBounds
{
    // Members here demarcate the data to write into 3 sections:
    // 1. Prefix: partial Page at start of Nand data section. 
    //      End of data is Page aligned but not start of data.
    // 2. Arbitrary number of complete Nand pages that are Nand Page aligned 
    // 3. Suffix: partial Page at end of Nand data section. 
    //      Start of data is Nand Page aligned but not end of data.
    NvBool IsPrefixPartial;
    NvBool IsComplete;
    NvBool IsSuffixPartial;
    NvU32 SectorPrefixStart;
    NvU32 SectorPrefixCount;
    NvU32 PageCompleteStart;
    NvU32 PageCompleteCount;
    NvU32 PageSuffix;
    NvU32 SectorSuffixCount;
} NandDataBounds;

// Macro used to check condition taht 512B sector size in use
// If this condition is true 512B sector is in use.
// This condition if false means Sector size equals Nand Page size. 
#define MACRO_CONDITION_NAND_512B_SECTOR \
    (nsCache->Log2SectorsPerPage > 0)

#ifdef CACHE_DEBUG
// Partial Print Read/Write data Enable
#define PRINT_BYTES_RD_WR 0
#if PRINT_BYTES_RD_WR
#ifndef NAND_BYTES_TO_PRINT
#define NAND_BYTES_TO_PRINT 0x20
#define NAND_BYTES_PER_ROW 0x10
#endif
#define MACRO_NAND_PRINT_ARRAY(BytePtr, SectorStart, \
    TotalBytes, PrintBytes, RowBytes) \
    { \
        NvU32 i; \
        NvU32 MaxBytes; \
        SC_DEBUG_INFO(("\nSector start 0x%x: 0x%x Bytes ", SectorStart, TotalBytes)); \
        MaxBytes = (PrintBytes > TotalBytes) ? TotalBytes : PrintBytes; \
        for (i = 0; (i < PrintBytes); i++) \
        { \
            if (!MACRO_MOD_LOG2NUM(i, 4)) \
                SC_DEBUG_INFO(("\n")); \
            SC_DEBUG_INFO(("0x%x ", *((NvU8 *)(BytePtr) + i))); \
        } \
        SC_DEBUG_INFO(("\n")); \
    }
#else
#define MACRO_NAND_PRINT_ARRAY(BytePtr, SectorStart, \
    TotalBytes, PrintBytes, RowBytes) 
#endif

// Function Print Enable
#define PRINT_FN_NAMES 0
#else
#define PRINT_FN_NAMES 0
#define PRINT_BYTES_RD_WR 0
#define MACRO_NAND_PRINT_ARRAY(BytePtr, SectorStart, \
    TotalBytes, PrintBytes, RowBytes) 
#endif

// Enables print code to debug 512B sector support
#ifndef NAND_512B_DBG_PRINT
#define NAND_512B_DBG_PRINT 0
#endif

#ifndef NAND_SECTOR_CACHE_ASSERT_RETURN
#define NAND_SECTOR_CACHE_ASSERT_RETURN 0
#endif
#if NAND_SECTOR_CACHE_ASSERT_RETURN
// Macro used to help debug when error occurs 
#define MACRO_RETURN_ERR(X) \
    { \
        NV_ASSERT(NV_FALSE); \
        return (X); \
    }
#else
#define MACRO_RETURN_ERR(X) \
        return (X);
#endif

// constant to indicate sector cache reset page address
static const NvU32 NAND_RESET_SECTOR_CACHE_PAGE_ADDRESS = 0xFFFFFFFF;

#if NAND_512B_DBG_PRINT
static volatile NvU32 s_Enable512BPrint = 0;
static NvU32 s_512Bcount = 0;
// print sector cache state after following number of calls to read/write
#define NAND_SECTOR_CACHE_LOG_CALL_COUNT 8
#define LOG2_NAND_SECTOR_CACHE_LOG_CALL_COUNT 3
// Wait after every 1000 prints
#define NAND_WAIT_512B_LOG 1000
#endif
#ifndef NAND_WAR_FLUSH_CHECK
#define NAND_WAR_FLUSH_CHECK 0
#endif
#ifndef NAND_DELAYED_READ
#define NAND_DELAYED_READ 0 
/* 0 - Immediately when data written to cache read unused sub-buckets from Nand */
/* 1 - Delay data read for unused sub-buckets from Nand */
#endif

#ifndef MAINTAIN_PAGE_ORDERING
// Macro to enable ascending page search in sector cache
#define MAINTAIN_PAGE_ORDERING 1
#endif

#ifndef RD_FLUSH_IMPROVEMENT
// Macro to add optimization to avoid dirty page flush during read
#define RD_FLUSH_IMPROVEMENT 1
#endif

#ifndef NAND_SCACHE_RD_VERIFY
// write read verification for writes during cache flush call
#define NAND_SCACHE_RD_VERIFY 0
#endif     

// Function to calculate the Page start and count given 
// the sector start and count
static void
UtilConvSectorToPageArgs(NvU32 SavedSectorAddr, NvU32 SavedSectorCount, 
            NvU32 Log2SectorsPerPage, NvU32 *pPageStart, NvU32 *pPageCount)
{
    NvU32 SectorStartOffset;
    NvU32 SectorEndResidue;
#if PRINT_FN_NAMES
    SC_DEBUG_INFO(("\nUtilConvSectorToPageArgs "));
#endif
    // Sector size 512B
    // convert the sector address to Nand Page address
    *pPageStart = MACRO_DIV_POW2_LOG2NUM(SavedSectorAddr, Log2SectorsPerPage);
    // calculate sector offset in start Page
    SectorStartOffset = MACRO_MOD_LOG2NUM(SavedSectorAddr, Log2SectorsPerPage);
    // calculate Nand Page count
    *pPageCount = MACRO_DIV_POW2_LOG2NUM((SectorStartOffset + SavedSectorCount),  
        Log2SectorsPerPage);
    // End Residue 
    SectorEndResidue = MACRO_MOD_LOG2NUM((SectorStartOffset + SavedSectorCount),  
        Log2SectorsPerPage);
    // Ensure sector count is non-zero
    if (SectorEndResidue)
        (*pPageCount)++;
}

// Function to calculate prefix, complete and suffic sections for read/write
static void
UtilGetPageBounds(NvNandHandle hNand, 
    NvU32 sectorAddr, NvU32 sectorCount, 
    NandDataBounds *pDataBounds
    )
{
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    NvU32 StartSectorOffset;
    NvU32 RemainingSectors = sectorCount;
#if PRINT_FN_NAMES
    SC_DEBUG_INFO(("\nUtilGetPageBounds "));
#endif
    // Reset all members 
    NvOsMemset(pDataBounds, 0x0, sizeof(NandDataBounds));

    // set complete page read/write start address, increment if start unaligned
    pDataBounds->PageCompleteStart = MACRO_DIV_POW2_LOG2NUM(sectorAddr, 
        nsCache->Log2SectorsPerPage);
    if (MACRO_CONDITION_NAND_512B_SECTOR)
    {
        // Start sector offset in Page
        StartSectorOffset = MACRO_MOD_LOG2NUM(sectorAddr, 
            nsCache->Log2SectorsPerPage);
        // Calculate all the 3 section boundaries
        if (StartSectorOffset)
        {
            // Start address is not page aligned and we have partial prefix section
            pDataBounds->IsPrefixPartial = NV_TRUE;
            // sector start in page
            pDataBounds->SectorPrefixStart = sectorAddr;
            // sectors in start partial page
            pDataBounds->SectorPrefixCount = 
                ((StartSectorOffset + RemainingSectors) >= 
                (NvU32)(MACRO_POW2_LOG2NUM(nsCache->Log2SectorsPerPage))) ?
                ((MACRO_POW2_LOG2NUM(nsCache->Log2SectorsPerPage)) - 
                StartSectorOffset) : RemainingSectors;
            // Remaining sectors
            RemainingSectors -= pDataBounds->SectorPrefixCount;
            // complete page start can be the next page only
            pDataBounds->PageCompleteStart++;
        }
    }
    // Get remaining pages
    pDataBounds->PageCompleteCount = MACRO_DIV_POW2_LOG2NUM(RemainingSectors,  
        nsCache->Log2SectorsPerPage);
    if (pDataBounds->PageCompleteCount)
    {
        // We have some more sectors remaining which are accesssed 
        // as complete pages.
        pDataBounds->IsComplete = NV_TRUE;
        // update remaining sectors
        RemainingSectors -= (MACRO_MULT_POW2_LOG2NUM(pDataBounds->PageCompleteCount,
            nsCache->Log2SectorsPerPage));
    }
    if (MACRO_CONDITION_NAND_512B_SECTOR)
    {
        if (RemainingSectors)
        {
            // End address is not page aligned and we have partial suffix page
            pDataBounds->IsSuffixPartial = NV_TRUE;
            // sectors in end partial page
            pDataBounds->SectorSuffixCount = RemainingSectors;
            // set end page address
            pDataBounds->PageSuffix = (pDataBounds->PageCompleteStart + 
                pDataBounds->PageCompleteCount);
        }
    }
}

// Reset state of cache line
static void
UtilSetCacheLineState(NvNandHandle hNand, CacheLine *Bucket, 
    State NewState)
{
    NvU32 i;
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    // Reset all sub-bucket states
    for (i = 0; i < (NvU32)(MACRO_POW2_LOG2NUM(nsCache->Log2SectorsPerPage)); i++)
    {
        Bucket->SubState[i] = NewState;
    }
    // Set the cache line state
    Bucket->state = NewState;
}

// Utility to check if sectors in page are partially cached.
static void
UtilIsPartialPageCached(NvNandHandle hNand, CacheLine *Line,
    NvBool *pIsPartialCached)
{
    NvU32 i;
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    // assume full caching
    *pIsPartialCached = NV_FALSE;

    for (i = 0; i < (NvU32)(MACRO_POW2_LOG2NUM(nsCache->Log2SectorsPerPage)); i++)
    {
        if (Line->SubState[i] == State_UNUSED)
        {
            *pIsPartialCached = NV_TRUE;
        }
    }
}

#if NAND_512B_DBG_PRINT
// utility to print sector cache data structure
static void
UtilPrintSectorCache(NvNandHandle hNand)
{
    NvU32 i;
    NvU32 j;
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;

    NvOsDebugPrintf("\nSCache[%u]: ", s_512Bcount);
    for (i = 0; i < CACHE_LINES; i++)
    {
        if (nsCache->lines[i].state != State_UNUSED)
        {
            NvOsDebugPrintf("{L%u-%u,pg=0x%x[",
                i, nsCache->lines[i].state, nsCache->lines[i].address);
            for (j = 0; j < nsCache->SectorsPerPage; j++)
            {
                NvOsDebugPrintf("%u ", nsCache->lines[i].SubState[j]);
                if (j == (nsCache->SectorsPerPage - 1))
                    NvOsDebugPrintf("]} ");
            }
        }
    }
    NvOsDebugPrintf("\n");
}
#endif

// Utility to find a cache bucket to use. If free cache bucket 
// available it is returned. Else, LRU cache bucket is found.
// Caller sends *pCacheUpdate == NV_FALSE. When no free cache bucket 
// found the flag *pCacheUpdate is set NV_TRUE. 
static void
NandFindCacheLine(NvNandHandle hNand,
    NvBool *pCacheUpdated,
    NvU32 *pFreeLineIndex,
    NvU32 *pOldestAccessedLine)
{
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    NvS32 lineCount;
    NvU64 oldestAccessed = 0;
#if PRINT_FN_NAMES
    SC_DEBUG_INFO(("\nNandFindCacheLine "));
#endif
    *pCacheUpdated = NV_FALSE;
    for (lineCount = 0; lineCount < CACHE_LINES; lineCount++)
    {
        // Check to see if any cache window is free
        if (nsCache->lines[lineCount].state == State_UNUSED)
        {
            *pFreeLineIndex = lineCount;
            *pCacheUpdated = NV_TRUE;
            break;
        }
        if ((lineCount == 0) ||
        (nsCache->lines[lineCount].LastAccessTick < oldestAccessed))
        {
            oldestAccessed = nsCache->lines[lineCount].LastAccessTick;
            *pOldestAccessedLine = lineCount;
        }
    }
}

// Utility to flush cache line using sector pointer
static NvError
NandFlushSector(NvNandHandle hNand, Sector *sector)
{
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    NvError returnValue = NvSuccess;
#if PRINT_FN_NAMES
    SC_DEBUG_INFO(("\nNandFlushSector Page=0x%x ", 
        sector->line->address));
#endif
#if NAND_512B_DBG_PRINT
    if (s_Enable512BPrint)
    {
        NvOsDebugPrintf("{Flush Pg=0x%x} ", 
            sector->line->address);
    }
#endif
    if (MACRO_CONDITION_NAND_512B_SECTOR)
    {
        NvBool IsPartialCached;
        // Utility to check if any unused sub-buckets exist in this cache line 
        // or if any sub-buckets are dirty
        UtilIsPartialPageCached(hNand, sector->line, 
            &IsPartialCached);
        if (sector->line->state == State_DIRTY)
        {
#if NAND_DELAYED_READ
            if (IsPartialCached == NV_TRUE) /* unused pages exist */
            {
                NvU32 i;
                // We deferred read of page for sector write within page 
                // but need it before flush of dirty sectors 
                returnValue = NandTLRead(hNand, 
                    sector->line->lun, 
                    sector->line->address, 
                    nsCache->ReadData, 1);
                if (returnValue != NvSuccess)
                    MACRO_RETURN_ERR(returnValue);
                // Update read buffer with dirty sectors in page
                for (i = 0; i < (NvU32)(MACRO_POW2_LOG2NUM(nsCache->Log2SectorsPerPage)); i++)
                {
                    // Copy the data read from disk into cache
                    if (sector->line->SubState[i] == 
                        State_UNUSED)
                    {
                        // update sub bucket in cache line from read buffer
                        NvOsMemcpy(((NvU8*)(sector->line->data) + 
                            (NvU32)(MACRO_MULT_POW2_LOG2NUM(i, nsCache->Log2AppBytesPerSector))), 
                            ((NvU8 *)nsCache->ReadData + 
                            (NvU32)(MACRO_MULT_POW2_LOG2NUM(i, nsCache->Log2AppBytesPerSector))), 
                            (NvU32)(MACRO_POW2_LOG2NUM(nsCache->Log2AppBytesPerSector)));
                    }
                }
            }
#endif
            // Flush the cached page
            returnValue = NandTLWrite(
                hNand, sector->line->address, (NvU8*)(sector->line->data), 1);
#if NAND_SCACHE_RD_VERIFY
            if (returnValue != NvSuccess)
                MACRO_RETURN_ERR(returnValue);
            // Read the page back
            returnValue = NandTLRead(hNand, 
                sector->line->lun,
                sector->line->address,                
                (NvU8*)(nsCache->ReadData), 1);
            if (returnValue != NvSuccess)
                MACRO_RETURN_ERR(returnValue);
            {
                NvU32 Val;
                // Compare the read back data with data flushed
                Val = NvOsMemcmp(nsCache->ReadData, sector->line->data, 
                    (NvU32)(MACRO_POW2_LOG2NUM(nsCache->Log2SectorsPerPage + nsCache->Log2AppBytesPerSector)));
                if (Val != 0)
                {
                    NvOsDebugPrintf("\nError: Rd verify failed pg=0x%x ",
                        sector->line->address);
                }
            }
#endif
        }
    }
    else
    {
        // Flush the updated page
        returnValue = NandTLWrite(
            hNand, sector->line->address, (NvU8*)(sector->line->data), 1);
    }
    return returnValue;
}

// This function searches the sector cache and 
// returns the pages in increasing order if any is cached
// from range PageStart till next PageCount pages
static Sector* NandSectorCacheGetSectorAscending(
    NvNandHandle hNand,
    NvS32 lun,
    NvU32 PageStart,
    NvS32 PageCount)
{
    NvU32 lineCount;
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    // Initialize sector cache index for minimum page with illegal index
    NvU32 MinIndex = CACHE_LINES;
    Sector *pSector = NULL;

#if PRINT_FN_NAMES
    SC_DEBUG_INFO(("\nNandSectorCacheGetSector pg Start=0x%x, pg Count=0x%x,UNused=%d ",
        PageStart, PageCount, State_UNUSED));
#endif
    // Check each sector cache line for the particular page
    for (lineCount = 0; lineCount < CACHE_LINES; lineCount++)
    {
        if ((nsCache->lines[lineCount].state != State_UNUSED) &&
            (nsCache->lines[lineCount].lun == lun))
        {
            NvU32 lineAddress = nsCache->lines[lineCount].address;
            NvU32 lastSectorAddress = PageStart + (PageCount - 1);

            // Next time CacheGetSectorAscending is called page returned 
            // in previous call would be in state UNUSED
            if ((lineAddress <= lastSectorAddress) &&
                (lineAddress >= PageStart))
            {
                // Keep track of the smallest page address which is used and in range
                if (MinIndex >= CACHE_LINES)
                {
                    // This is the first entry found in range
                    MinIndex = lineCount;
                    pSector = &nsCache->lines[lineCount].sector;
                }
                else
                {
                    // update the minimum page number
                    if (nsCache->lines[MinIndex].address > lineAddress)
                    {
                        MinIndex = lineCount;
                        pSector = &nsCache->lines[lineCount].sector;
                    }
                }
            }
        }
    }
    return pSector;
}

// Function to invalidate cached page in a range
static void
NandInvalidateCachedPages(NvNandHandle hNand,
    NvS32 lun,
    NvU32 PageStart,
    NvS32 PageCount)
{
    Sector* sector;
#if PRINT_FN_NAMES
    SC_DEBUG_INFO(("\nNandInvalidateCachedPages Start=%d, count=%d ", 
        PageStart, PageCount));
#endif
    while (1)
    {
        // page search order not important since do not write but invalidate
        sector = NandSectorCacheGetSector(hNand, 
            lun, PageStart, PageCount);

        // If no page cache return
        if (!sector)
            break;
        
        // Mark cache line UNUSED, since we could do write where
        // data is cached
        if (sector->line->state != State_UNUSED)
            UtilSetCacheLineState(hNand, sector->line, State_UNUSED);
    }
}

#if RD_FLUSH_IMPROVEMENT
// function to update client data buffer with updated cached data 
static void
NandUpdateClientRdData(NvNandHandle hNand,
    NvS32 lun,
    NvU32 PageStart,
    NvS32 PageCount, NvU8 *data)
{
    Sector* sector;
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    NvU32 ByteOffset;
    // Save the start sector and sector count
    NvU32 StartPage = PageStart;
    NvU32 CountPage = PageCount;
    NvU32 i;
    NvU32 DiffLastStart;
#if PRINT_FN_NAMES
    SC_DEBUG_INFO(("\nNandUpdateClientRdData Start=%d, count=%d ", 
        StartPage, PageCount));
#endif
    while (1)
    {
        // page search order used as we want to move forward the start page
        sector = NandSectorCacheGetSectorAscending(hNand, 
            lun, StartPage, CountPage);

        // If no page cache return
        if (!sector)
            break;
        // Dirty pages need to be copied
        if (sector->line->state == State_DIRTY)
        {
            NvU32 PageOffset;
            PageOffset = (sector->address - PageStart);
            // Get byte offset within client buffer for dirty page
            ByteOffset = (PageOffset << 
                (nsCache->Log2SectorsPerPage + nsCache->Log2AppBytesPerSector));
            for (i = 0; i < (NvU32)MACRO_POW2_LOG2NUM(nsCache->Log2SectorsPerPage); i++)
            {
                // Copy data from cache into client Read-Buffer
                NvOsMemcpy(((NvU8 *)data + ByteOffset + 
                    (i * MACRO_POW2_LOG2NUM(nsCache->Log2AppBytesPerSector))), 
                    ((NvU8 *)sector->data + 
                    (i * MACRO_POW2_LOG2NUM(nsCache->Log2AppBytesPerSector))), 
                    MACRO_POW2_LOG2NUM(nsCache->Log2AppBytesPerSector));
            }
            // If processed last page, break
            if ((PageOffset + 1) == (NvU32)PageCount)
                break;
        }
        // Page offset compared to last start page
        DiffLastStart = (sector->address - StartPage) + 1;
        // Move the start page to page after last cached page in range
        StartPage += DiffLastStart;
        // decrease page count
        CountPage -= DiffLastStart;
        // If finished all pages then break
        if (!CountPage)
            break;
    }
}
#endif

#if MAINTAIN_PAGE_ORDERING || !RD_FLUSH_IMPROVEMENT
// Function should not be called for single page write/read
static NvError
NandFlushCachedPages(NvNandHandle hNand,
    NvS32 lun,
    NvU32 PageStart,
    NvS32 PageCount)
{
    Sector* sector;
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    NvError returnValue = NvSuccess;
#if PRINT_FN_NAMES
    SC_DEBUG_INFO(("\nNandFlushCachedPages Start=%d, count=%d ", 
        PageStart, PageCount));
#endif
    if (nsCache->DeviceInfo.DeviceType == MLC)
    {
        // sectors found in range returned are for page ascending search.
        sector = NandSectorCacheGetSectorAscending(hNand, 
            lun, PageStart, PageCount);
    }
    else
    {
        // page search order not important for non-MLC
        sector = NandSectorCacheGetSector(hNand, 
            lun, PageStart, PageCount);
    }
    if ((sector) && (PageCount > 1))
    {
        SC_DEBUG_INFO(("\r\n\t\t\t\t*** sector %d is under cache", 
            sector->address));
        // otherwise invalidate any lines this read affects.
        do
        {
            if (sector->line->state == State_DIRTY)
            {
                SC_DEBUG_INFO(("\r\n\t\t\t\t*** Flushing cache line "
                    "at addr %d",sector->line->address));
                // Flush dirty pages before invalidating
                returnValue = NandFlushSector(hNand, sector);
                if (returnValue != NvSuccess)
                    MACRO_RETURN_ERR(returnValue);
            }
            // Mark cache line UNUSED, since we could do write where
            // data is not cached
            if (sector->line->state != State_UNUSED)
                UtilSetCacheLineState(hNand, sector->line, State_UNUSED);
            // Try to find next page that is cached from 
            // sectorAddr till the next sectorCount sectors
            if (nsCache->DeviceInfo.DeviceType == MLC)
            {
                // sectors returned are for page ascending search.
                sector = NandSectorCacheGetSectorAscending(hNand, 
                    lun, PageStart, PageCount);
            }
            else
            {
                // page search order not important for non-MLC
                sector = NandSectorCacheGetSector(hNand, 
                    lun, PageStart, PageCount);
            }
        }while (sector != 0);
    }
    return returnValue;
}
#endif

// Complete page writes are done by calling this utility
static NvError
NandPageFullWrite(NvNandHandle hNand,
    NvS32 lun,
    NvU32 PageStart,
    NvU8 *data,
    NvS32 PageCount) 
{
#if MAINTAIN_PAGE_ORDERING || PRINT_BYTES_RD_WR
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
#endif
    NvError returnValue;
#if MAINTAIN_PAGE_ORDERING
    NvU32 BlockAlignedPage;
#endif

#if PRINT_FN_NAMES
    SC_DEBUG_INFO(("\nNandPageFullWrite "));
#endif
#if MAINTAIN_PAGE_ORDERING
    // Some MLC do not allow writing pages out of order in same block 
    // Flush cached pages in same block if start page is not block aligned
    BlockAlignedPage = ((PageStart >> nsCache->Log2PagesPerBlock) << 
        nsCache->Log2PagesPerBlock);
    if ((nsCache->DeviceInfo.DeviceType == MLC) && 
        (BlockAlignedPage < PageStart))
    {
        returnValue = NandFlushCachedPages(hNand, lun, BlockAlignedPage, 
            (PageStart - BlockAlignedPage));
        if (returnValue != NvSuccess)
            MACRO_RETURN_ERR(returnValue);
    }
#endif    

    // Invalidate cached pages 
    NandInvalidateCachedPages(hNand, lun, PageStart, PageCount);
    
    // Call uncached write pages function
    returnValue = NandTLWrite(hNand, PageStart, data, PageCount);
    MACRO_NAND_PRINT_ARRAY(data, (NvU32)(MACRO_MULT_POW2_LOG2NUM(PageStart, nsCache->Log2SectorsPerPage)),
        (NvU32)(MACRO_MULT_POW2_LOG2NUM(PageCount, (nsCache->Log2SectorsPerPage + nsCache->Log2AppBytesPerSector))), 
        NAND_BYTES_TO_PRINT, NAND_BYTES_PER_ROW);

    return returnValue;
}

#if !DISABLE_READ_SECTOR_CACHE
// Read a particular cache bucket
static NvError
NandReadCacheLine(NvNandHandle hNand,
    NvS32 lun,
    NvU32 SectorOffset,
    NvU8 *data, NvU32 lineCount, NvU32 sectorCount, NvU32 PageStart)
{
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    NvU32 lineAddress;
    NvError returnValue = NvSuccess;

#if PRINT_FN_NAMES
    SC_DEBUG_INFO(("\nNandReadCacheLine fill Index=%d, Page=0x%x ",
        lineCount, PageStart));
#endif
#if NAND_512B_DBG_PRINT
    if (s_Enable512BPrint)
    {
        NvOsDebugPrintf("{RdFill Indx=%d,Pg=0x%x,"
            "offset=%u,count=%u} ",
            lineCount, PageStart, SectorOffset, sectorCount);
    }
#endif
    // Use the free cache bucket for the sector to read
    lineAddress = PageStart;
    nsCache->lines[lineCount].address = lineAddress;
    // dbgPrintf("\r\n Cached lineAddress %d", lineAddress);
    // set the sector address
    nsCache->lines[lineCount].sector.address = lineAddress;
    nsCache->lines[lineCount].lun = lun;
    // Read data into new cache line
    returnValue = NandTLRead(hNand, lun,
               lineAddress,
               (NvU8*)nsCache->lines[lineCount].data,
               1);
    if (returnValue != NvSuccess)
        MACRO_RETURN_ERR(returnValue);
    SC_DEBUG_INFO(("\r\n\t\t\t\t*** CACHED the single sector"));
    // Return the data to application
    NvOsMemcpy(data, ((NvU8 *)nsCache->lines[lineCount].data + 
        (NvU32)(MACRO_MULT_POW2_LOG2NUM(SectorOffset, nsCache->Log2AppBytesPerSector))), 
        (NvU32)(MACRO_MULT_POW2_LOG2NUM(sectorCount, nsCache->Log2AppBytesPerSector)));
    MACRO_NAND_PRINT_ARRAY(((NvU8 *)nsCache->lines[lineCount].data + 
        (NvU32)(MACRO_MULT_POW2_LOG2NUM(SectorOffset, nsCache->Log2AppBytesPerSector))), 
        ((NvU32)(MACRO_MULT_POW2_LOG2NUM(PageStart, 
        nsCache->Log2SectorsPerPage)) + SectorOffset),
        (NvU32)(MACRO_MULT_POW2_LOG2NUM(sectorCount, nsCache->Log2AppBytesPerSector)),
        NAND_BYTES_TO_PRINT, NAND_BYTES_PER_ROW);
    // Mark state of all sectors as cached and not dirty i.e. clean
    UtilSetCacheLineState(hNand, &(nsCache->lines[lineCount]), State_CLEAN);
    // Update access time of cache line
    nsCache->lines[lineCount].LastAccessTick = NvOsGetTimeUS();
    return returnValue;
}
#endif

// Write a particular cache bucket
static NvError
NandWriteCacheLine(NvNandHandle hNand,
    NvS32 lun,
    NvU32 SectorOffset,
    NvU8 *data, NvU32 lineCount, NvU32 sectorCount, NvU32 PageStart)
{
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    NvU32 lineAddress;
    NvError returnValue;

#if PRINT_FN_NAMES
    SC_DEBUG_INFO(("\nNandWriteCacheLine fill Index=%d, Page=0x%x ", 
        lineCount, PageStart));
#endif
#if NAND_512B_DBG_PRINT
    if (s_Enable512BPrint)
    {
        NvOsDebugPrintf("{WrFill Indx=%d,Pg=0x%x,"
            "offset=%u,count=%u} ",
            lineCount, PageStart, SectorOffset, sectorCount);
    }
#endif
    // Use the free cache bucket for the sector to write
    lineAddress = PageStart;
    nsCache->lines[lineCount].address = lineAddress;
    // dbgPrintf("\r\n Cached lineAddress %d", lineAddress);
    // set the sector address
    nsCache->lines[lineCount].sector.address = lineAddress;
    nsCache->lines[lineCount].lun = lun;
    SC_DEBUG_INFO(("\r\n\t\t\t\t*** CACHED the single sector"));
#if !NAND_DELAYED_READ
    if (MACRO_CONDITION_NAND_512B_SECTOR)
    {
        // If sector == 0 and sectorCount == 4 we skip read
        if ((SectorOffset) || (sectorCount < ((NvU32)(MACRO_POW2_LOG2NUM(nsCache->Log2SectorsPerPage)))))
        {
            // We deferred read of page for sector write within page 
            // but need it before flush of dirty sectors 
            returnValue = NandTLRead(hNand, 
                nsCache->lines[lineCount].lun, 
                nsCache->lines[lineCount].address, 
                (NvU8 *)nsCache->lines[lineCount].data, 1);
            if (returnValue != NvSuccess)
                MACRO_RETURN_ERR(returnValue);
            // Mark all sub-buckets as CLEAN
            UtilSetCacheLineState(hNand, &(nsCache->lines[lineCount]), State_CLEAN);
        }
    }
#endif
    // write into cache line
    returnValue = NandSectorCacheSetSectorData(
        hNand, 
        &nsCache->lines[lineCount].sector,
        data, SectorOffset, sectorCount);
    // update the timestamp for write
    nsCache->lines[lineCount].LastAccessTick = NvOsGetTimeUS();
    return returnValue;
}

// static NvBool s_PrintTT = NV_TRUE;

// Function to read from cache line after cache hit - single page read case
static void
NandCacheHitRead(NvNandHandle hNand,
    NvS32 lun,
    NvU32 SectorOffset, NvU8 *data, NvS32 sectorCount, 
    NvU32 PageStart, NvU32 PageCount,
    NvBool *pIsCached)
{
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    Sector* sector;
    NvU32 BytesToCopy;
#if PRINT_FN_NAMES
    SC_DEBUG_INFO(("\nNandCacheHitRead "));
#endif
    // Uncached page by default
    *pIsCached = NV_FALSE;
    // see if there is a cache hit
    sector = NandSectorCacheGetSector(hNand, lun, PageStart, PageCount);
    if ((sector) && (PageCount == 1))
    {
        SC_DEBUG_INFO(("\r\n\t\t\t\t*** sector to Read=%d is under cache ", 
            sector->address));
#if NAND_512B_DBG_PRINT
        if (s_Enable512BPrint)
        {
            NvOsDebugPrintf("{Rd hit:pg=0x%x} ", sector->address);
        }
#endif
        // set flag for cache page
        *pIsCached = NV_TRUE;
        if (MACRO_CONDITION_NAND_512B_SECTOR)
        {
            BytesToCopy = (NvU32)(MACRO_MULT_POW2_LOG2NUM(sectorCount, nsCache->Log2AppBytesPerSector));
        }
        else
        {
            // Copy Nand Page size data when sector size equals Nand Page size
            SectorOffset = 0;
            // sector count 1 only expected for 2K sector size as
            // we cache requests for 1 page only
            sectorCount = 1;
            BytesToCopy = NandTLGetSectorSize(hNand);
        }
        // Cache line state does not change with read
        // if so, and if it is a single-sector read,
        // satisfy the read from the cache.
        NvOsMemcpy(data, ((NvU8 *)sector->data + 
            ((NvU32)MACRO_MULT_POW2_LOG2NUM(SectorOffset, nsCache->Log2AppBytesPerSector))), BytesToCopy);
        MACRO_NAND_PRINT_ARRAY(((NvU8 *)sector->data + 
            (NvU32)(MACRO_MULT_POW2_LOG2NUM(SectorOffset, nsCache->Log2AppBytesPerSector))), 
            ((NvU32)(MACRO_MULT_POW2_LOG2NUM(PageStart, nsCache->Log2SectorsPerPage)) + SectorOffset), 
            BytesToCopy,
            NAND_BYTES_TO_PRINT, NAND_BYTES_PER_ROW);

        // save access time to decide on LRU replacement
        sector->line->LastAccessTick = NvOsGetTimeUS();
    }            
}

#if DISABLE_READ_SECTOR_CACHE
// function used to read 512B sized sector(s) in a page
static NvError
NandPartialPageRead(NvNandHandle hNand,
    NvS32 lun,
    NvU32 sectorAddr,
    NvU8 *data,
    NvU32 sectorCount, NvU32 PageStart)
{
    NvU32 SectorOffset;
    NvError returnValue;
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    SectorOffset = MACRO_MOD_LOG2NUM(sectorAddr, nsCache->Log2SectorsPerPage);
    // Cached read only possible for read within a page
    if (!(((SectorOffset + sectorCount) <= 
        ((NvU32)MACRO_POW2_LOG2NUM(nsCache->Log2SectorsPerPage))) && 
        (sectorCount)))
    {
        NvOsDebugPrintf("\nError: trying cached read past page limits ");
        return NvError_NandWriteFailed;
    }
    NvOsDebugPrintf("\n 512B Read: Page=%d, within page sector in page=%d, "
        "sector count=%d ", PageStart, SectorOffset, sectorCount);
    if (!hNand->BufSector512B)
    {
        NvOsDebugPrintf("\nError: 512B buffer allocate failed earlier ");
        return NvError_NandReadFailed;
    }
    // Read the single page
    returnValue = NandTLRead(hNand, lun, PageStart, hNand->BufSector512B, 1);
    if (returnValue != NvSuccess)
        MACRO_RETURN_ERR(returnValue);
    // update page dat with 512B sector write data
    NvOsMemcpy(data, ((NvU8 *)hNand->BufSector512B + (MACRO_MULT_POW2_LOG2NUM(
        SectorOffset, nsCache->Log2AppBytesPerSector))),
        (MACRO_MULT_POW2_LOG2NUM(sectorCount, nsCache->Log2AppBytesPerSector)));

    return returnValue;
}
#endif

// Function to write cache line after cache hit
static NvError
NandCacheHitWrite(NvNandHandle hNand,
    NvS32 lun,
    NvU32 SectorOffset, NvU8 *data, NvS32 sectorCount, 
    NvU32 PageStart, NvU32 PageCount,
    NvBool *pIsCached)
{
    Sector* sector;
    NvError returnValue = NvSuccess;
#if PRINT_FN_NAMES
    SC_DEBUG_INFO(("\nNandUpdateCache "));
#endif
    // Uncached page by default
    *pIsCached = NV_FALSE;
    // Check for cached page if single page write
    sector = NandSectorCacheGetSector(hNand, 
        lun, PageStart, PageCount);
    if ((sector) && (PageCount == 1))
    {
        SC_DEBUG_INFO(("\r\n\t\t\t\t*** sector to Write=%d is under cache ", 
            sector->address));
#if NAND_512B_DBG_PRINT
    if (s_Enable512BPrint)
    {
        NvOsDebugPrintf("{Wr hit:pg=0x%x} ", sector->address);
    }
#endif
        // ALready hit so no need to update cache line information 

        // set flag for cache page
        *pIsCached = NV_TRUE;
        // data for single sector write already cached, write to cache.
        returnValue = NandSectorCacheSetSectorData(hNand, sector, data,
            SectorOffset, sectorCount);
        // save access time to decide on LRU replacement
        sector->line->LastAccessTick = NvOsGetTimeUS();
    }
    return returnValue;
}

// New Cache line is initialized with the data to read by this utility
static NvError
NandCachedRead(NvNandHandle hNand,
    NvS32 lun,
    NvU32 sectorAddr,
    NvU8 *data,
    NvU32 sectorCount, NvU32 PageStart)
{
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    NvError returnValue = NvSuccess;
#if !DISABLE_READ_SECTOR_CACHE
     NvBool cacheUpdated;
    NvU32 oldestAccessedLine = 0;
    NvU32 lineCount = CACHE_LINES;
#endif
    NvBool IsCachedPage;
    NvU32 SectorOffset;

#if PRINT_FN_NAMES
    SC_DEBUG_INFO(("\nNandCachedRead "));
#endif
    SectorOffset = 
        MACRO_MOD_LOG2NUM(sectorAddr, nsCache->Log2SectorsPerPage);
    // Cached read only possible for write within a page
    if (!(((SectorOffset + sectorCount) <= 
        ((NvU32)MACRO_POW2_LOG2NUM(nsCache->Log2SectorsPerPage))) && 
        (sectorCount)))
    {
        NvOsDebugPrintf("\nError: trying cached read past page limits ");
        return NvError_NandReadFailed;
    }
    // see if there is a cache hit
    NandCacheHitRead(hNand, lun, SectorOffset, data, sectorCount, 
        PageStart, 1, &IsCachedPage);
    // cache hit
    if (IsCachedPage == NV_TRUE)
    {
        //NvOsDebugPrintf("\n Sector read: Cache hit Page start=%d, "
            //"sectorInPage=%d, sector count=%d ", PageStart, 
            //SectorOffset, sectorCount);
        return (returnValue);
    }
#if DISABLE_READ_SECTOR_CACHE
    // 512B sized sector(s) read
    returnValue = NandPartialPageRead(hNand, 
        lun, sectorAddr, 
        data, sectorCount, PageStart);

    return returnValue;
#else
    // Find free cache bucket or return LRU cache line
    NandFindCacheLine(hNand, &cacheUpdated, &lineCount, &oldestAccessedLine);
    // If cacheUpdated is NV_TRUE we found free cache bucket
    if (cacheUpdated == NV_TRUE)
    {
        returnValue = NandReadCacheLine(hNand, lun,
            SectorOffset, data, lineCount, sectorCount, PageStart);  
    }
    else
    {
        // replace a cache window. If the entry is dirty, flush to disk.
        if (nsCache->lines[oldestAccessedLine].state == State_DIRTY)
        {
            SC_DEBUG_INFO(("\r\n\t\t\t\t*** NO EMPTY bucket, flushing "
                "oldest accessed cache %d",
                nsCache->lines[oldestAccessedLine].address));
#if NAND_512B_DBG_PRINT
            if (s_Enable512BPrint)
            {
                NvOsDebugPrintf("{Flush CLine%u,pg=0x%x} ",
                    oldestAccessedLine, 
                    nsCache->lines[oldestAccessedLine].address);
            }
#endif
            returnValue = NandFlushSector(hNand, 
                &(nsCache->lines[oldestAccessedLine].sector));
            if (returnValue != NvSuccess)
                MACRO_RETURN_ERR(returnValue);
            // No need to change cache line state as all sub-buckets 
            // get changed to CLEAN after NandReadCacheLine
        }
        returnValue = NandReadCacheLine(hNand, lun,
            SectorOffset, data, oldestAccessedLine, sectorCount, PageStart);  
    }

    return returnValue;
#endif
}

// New Cache line is written by this utility
// Cached write utility NandCachedWrite is called for single sector/page
// write calls
static NvError
NandCachedWrite(NvNandHandle hNand,
    NvS32 lun,
    NvU32 sectorAddr,
    NvU8 *data,
    NvU32 sectorCount, NvU32 PageStart)
{
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    NvError returnValue;
    NvBool cacheUpdated;
    NvU32 oldestAccessedLine = 0;
    NvU32 lineCount = CACHE_LINES;
    NvBool IsCachedPage = NV_FALSE;
    NvU32 SectorOffset;

#if PRINT_FN_NAMES
    SC_DEBUG_INFO(("\nNandCachedWrite "));
#endif
    SectorOffset = MACRO_MOD_LOG2NUM(sectorAddr, nsCache->Log2SectorsPerPage);
    // Cached write only possible for write within a page
    if (!(((SectorOffset + sectorCount) <= 
        ((NvU32)MACRO_POW2_LOG2NUM(nsCache->Log2SectorsPerPage))) && 
        (sectorCount)))
    {
        NvOsDebugPrintf("\nError: trying cached write past page limits ");
        return NvError_NandWriteFailed;
    }
    // see if there is a cache hit
    returnValue = NandCacheHitWrite(hNand, 
        lun, SectorOffset, data, sectorCount, 
        PageStart, 1, &IsCachedPage);
    // got error due to illegal arguments to function
    if (returnValue != NvSuccess)
        MACRO_RETURN_ERR(returnValue);
    // cache hit
    if (IsCachedPage == NV_TRUE)
        return (returnValue);
    // Find free cache bucket or return LRU cache line
    NandFindCacheLine(hNand, &cacheUpdated, &lineCount, &oldestAccessedLine);
    // If cacheUpdated is NV_TRUE we found free cache bucket
    if (cacheUpdated == NV_TRUE)
    {
        // write cache line 
        returnValue = NandWriteCacheLine(hNand, lun,
            SectorOffset, data, lineCount, sectorCount, PageStart);  
    }
    else
    {
        // replace a cache window. If the entry is dirty, flush to disk.
        if (nsCache->lines[oldestAccessedLine].state == State_DIRTY)
        {
            SC_DEBUG_INFO(("\r\n\t\t\t\t*** NO EMPTY bucket, flushing "
                "oldest accessed cache %d",
                nsCache->lines[oldestAccessedLine].address));
#if NAND_512B_DBG_PRINT
            if (s_Enable512BPrint)
            {
                NvOsDebugPrintf("{Flush CLine%u,pg=0x%x} ",
                    oldestAccessedLine, 
                    nsCache->lines[oldestAccessedLine].address);
            }
#endif
            returnValue = NandFlushSector(hNand, 
                &(nsCache->lines[oldestAccessedLine].sector));
            if (returnValue != NvSuccess)
                MACRO_RETURN_ERR(returnValue);
        }
        // Reset state of all sub buckets in cache line as not all 
        // sub-buckets could be written
        UtilSetCacheLineState(hNand, &(nsCache->lines[oldestAccessedLine]), 
            State_UNUSED);
        // call write cache line function
        returnValue = NandWriteCacheLine(hNand, lun,
            SectorOffset, data, oldestAccessedLine, sectorCount, PageStart);  
    }

    return returnValue;
}

// Utility NandMultiPageRead called for multiple sector/page read 
static NvError
NandMultiPageRead(NvNandHandle hNand,
    NvS32 lun,
    NvU32 sectorAddr,
    NvU8 *data,
    NvS32 sectorCount,
    NvU32 PageStart, NvU32 PageCount)
{
    NvError returnValue = NvSuccess;
    NandDataBounds DataBounds;
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    NvU32 CompleteStartOffset;
    NvU32 SuffixByteOffset;

#if PRINT_FN_NAMES
    SC_DEBUG_INFO(("\nNandMultiPageRead "));
#endif
    // Call function to reurn boundaries of the 3 types of sections
    //      - prefix, complete and suffix
    UtilGetPageBounds(hNand, sectorAddr, sectorCount, &DataBounds);

    // Break the sector count into 
    // 1. Prefix section - 
    if (DataBounds.IsPrefixPartial == NV_TRUE)
    {
        // cached read for partial page
        returnValue = NandCachedRead(hNand, 
            lun, DataBounds.SectorPrefixStart, 
            data, DataBounds.SectorPrefixCount, PageStart);
        if (returnValue != NvSuccess)
            MACRO_RETURN_ERR(returnValue);
    }
    // 2. Full page sequence
    if (DataBounds.IsComplete == NV_TRUE)
    {
        // Calculate byte offset at which data read into buffer
        CompleteStartOffset = (MACRO_MULT_POW2_LOG2NUM(
                DataBounds.SectorPrefixCount, nsCache->Log2AppBytesPerSector));
#if !DISABLE_READ_SECTOR_CACHE
        if (DataBounds.PageCompleteCount == 1)
        {
            // cache single page reads
            returnValue = NandCachedRead(hNand, lun, 
                0 /* page aligned sector */,
                ((NvU8 *)data + CompleteStartOffset), 
                (NvU32)(MACRO_POW2_LOG2NUM(nsCache->Log2SectorsPerPage)), 
                DataBounds.PageCompleteStart);
            if (returnValue != NvSuccess)
                MACRO_RETURN_ERR(returnValue);
        }
        else
        {
#endif
#if !RD_FLUSH_IMPROVEMENT
            // Flush cached pages before read since pages could be dirty
            returnValue = NandFlushCachedPages(hNand, lun, 
                DataBounds.PageCompleteStart, DataBounds.PageCompleteCount);
            if (returnValue != NvSuccess)
                MACRO_RETURN_ERR(returnValue);
#endif
            //      - calls function uncached read
            returnValue = NandTLRead(hNand, lun, DataBounds.PageCompleteStart, 
                ((NvU8 *)data + CompleteStartOffset), 
                DataBounds.PageCompleteCount);
            if (returnValue != NvSuccess)
                MACRO_RETURN_ERR(returnValue);
#if RD_FLUSH_IMPROVEMENT
            // Update client data with dirty bucket data
            NandUpdateClientRdData(hNand, lun, 
                DataBounds.PageCompleteStart, DataBounds.PageCompleteCount,
                ((NvU8 *)data + CompleteStartOffset));
#endif
            MACRO_NAND_PRINT_ARRAY(((NvU8 *)data + CompleteStartOffset), 
                (NvU32)(MACRO_MULT_POW2_LOG2NUM(DataBounds.PageCompleteStart, (nsCache->Log2SectorsPerPage + nsCache->Log2AppBytesPerSector))),
                (NvU32)(MACRO_MULT_POW2_LOG2NUM(DataBounds.PageCompleteCount, 
                (nsCache->Log2AppBytesPerSector + nsCache->Log2SectorsPerPage))),
                NAND_BYTES_TO_PRINT, NAND_BYTES_PER_ROW);
#if !DISABLE_READ_SECTOR_CACHE
        }
#endif
    }
    // 3. Suffix section - 
    if (DataBounds.IsSuffixPartial == NV_TRUE)
    {
        // Calculate byte offset at which data read into buffer
        SuffixByteOffset = (
            MACRO_MULT_POW2_LOG2NUM(DataBounds.SectorPrefixCount, 
            nsCache->Log2AppBytesPerSector) +
            MACRO_MULT_POW2_LOG2NUM(DataBounds.PageCompleteCount, 
            (nsCache->Log2SectorsPerPage + nsCache->Log2AppBytesPerSector)));
        // cached read for partial page read
        returnValue = NandCachedRead(hNand, lun, 0, 
            ((NvU8 *)data + SuffixByteOffset),
            DataBounds.SectorSuffixCount, 
            (DataBounds.PageCompleteStart + DataBounds.PageCompleteCount));
    }
    return returnValue;
}

// Utility NandMultiPageWrite called for multiple sector/page write 
static NvError
NandMultiPageWrite(NvNandHandle hNand,
    NvS32 lun,
    NvU32 sectorAddr,
    NvU8 *data,
    NvS32 sectorCount,
    NvU32 PageStart, NvU32 PageCount)
{
    NvError returnValue = NvSuccess;
    NandDataBounds DataBounds;
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    NvU32 CompleteStartOffset;
    NvU32 SuffixByteOffset;

#if PRINT_FN_NAMES
    SC_DEBUG_INFO(("\nNandMultiPageWrite "));
#endif
    // Call function to reurn boundaries of the 3 types of sections
    //      - prefix, complete and suffix
    UtilGetPageBounds(hNand, sectorAddr, sectorCount, &DataBounds);

    // Break the sector count into 
    // 1. Prefix section - 
    if (DataBounds.IsPrefixPartial == NV_TRUE)
    {
        // cached write for partial page
        returnValue = NandCachedWrite(hNand, 
            lun, DataBounds.SectorPrefixStart, 
            data, DataBounds.SectorPrefixCount, PageStart);
        if (returnValue != NvSuccess)
            MACRO_RETURN_ERR(returnValue);
    }
    // 2. Full page sequence
    if (DataBounds.IsComplete == NV_TRUE)
    {
        CompleteStartOffset = (MACRO_MULT_POW2_LOG2NUM(
                DataBounds.SectorPrefixCount, nsCache->Log2AppBytesPerSector));
        if (DataBounds.PageCompleteCount == 1)
        {
            // Cache if writing one page
            // Both for 512B sector and Nand Page sized sectors we 
            // can be here
            returnValue = NandCachedWrite(hNand, lun, 
                0/* page aligned hence passing 0 */, 
                ((NvU8 *)data + CompleteStartOffset), 
                (NvU32)(MACRO_POW2_LOG2NUM(nsCache->Log2SectorsPerPage)), DataBounds.PageCompleteStart);
            if (returnValue != NvSuccess)
                MACRO_RETURN_ERR(returnValue);
        }
        else
        {
            //      - calls function NandPageFullWrite
            returnValue = NandPageFullWrite(hNand, lun, 
                DataBounds.PageCompleteStart, 
                ((NvU8 *)data + CompleteStartOffset), 
                DataBounds.PageCompleteCount);
            if (returnValue != NvSuccess)
                MACRO_RETURN_ERR(returnValue);
        }
    }
    // 3. Suffix section - 
    if (DataBounds.IsSuffixPartial == NV_TRUE)
    {
        SuffixByteOffset = (
            MACRO_MULT_POW2_LOG2NUM(DataBounds.SectorPrefixCount, 
            nsCache->Log2AppBytesPerSector) +
            MACRO_MULT_POW2_LOG2NUM(DataBounds.PageCompleteCount, 
            (nsCache->Log2SectorsPerPage + nsCache->Log2AppBytesPerSector)));
        // cached write for partial page write
        returnValue = NandCachedWrite(hNand, lun, 0, 
            ((NvU8 *)data + SuffixByteOffset), DataBounds.SectorSuffixCount, 
            (DataBounds.PageCompleteStart + DataBounds.PageCompleteCount));
    }
    return returnValue;
}


/*
 ******************************************************************************
 */
void CacheLineInit(NvNandHandle hNand, CacheLine* cacheLine)
{
    NvU32 i;
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    cacheLine->data = 0;
    cacheLine->address = NAND_RESET_SECTOR_CACHE_PAGE_ADDRESS;
    cacheLine->lun = 0;
    cacheLine->LastAccessTick = 0ll;
    // Allocate the array to store state of sectors in page cache 
    cacheLine->SubState = (State *)AllocateVirtualMemory(
        (NvU32)(MACRO_MULT_POW2_LOG2NUM(sizeof(State), 
        nsCache->Log2SectorsPerPage)), SECTOR_CACHE_ALLOC);
    // Reset cache line bucket and sub-bucket states
    // Reset all sub-bucket states
    for (i = 0; i < (NvU32)(MACRO_POW2_LOG2NUM(nsCache->Log2SectorsPerPage)); i++)
    {
        cacheLine->SubState[i] = State_UNUSED;
    }
    // Reset the cache line state
    cacheLine->state = State_UNUSED;
}

/*
 ******************************************************************************
 */
void NandSectorCacheInit(NvNandHandle hNand)
{
    NvS8* sectorAddress = 0;
    NvS32 i;
    NvU32 AppBytesPerSector;
    NvU32 SectorsPerPage;
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    NvU32 CacheSize;

    nsCache->writeProtected = NV_TRUE;
    nsCache->testedForWriteProtection = NV_FALSE;
    NandTLGetDeviceInfo(hNand,0,&nsCache->DeviceInfo);
    CacheSize = NandTLGetSectorSize(hNand) * CACHE_LINES;
    nsCache->sectorData =
        (NvS32*)AllocateVirtualMemory(CacheSize, SECTOR_CACHE_ALLOC);
    // set data pointer to be used to initialize cache line data buffer
    sectorAddress = (NvS8*)nsCache->sectorData;

    // Initialize the application viewed sector size
    AppBytesPerSector = NvOdmQueryGetBlockDeviceSectorSize(NvOdmIoModule_Nand);
    AppBytesPerSector = (!AppBytesPerSector) ? 
        nsCache->DeviceInfo.PageSize : AppBytesPerSector;
    // init bytes in a sector
    nsCache->Log2AppBytesPerSector = (NvU32)NandUtilGetLog2(AppBytesPerSector);
    SectorsPerPage = MACRO_DIV_POW2_LOG2NUM(nsCache->DeviceInfo.PageSize, 
        nsCache->Log2AppBytesPerSector);
    // Ensure non-zero sectors per page
    if (SectorsPerPage == 0)
        SectorsPerPage = 1;
    // initialize sectors in a page
    nsCache->Log2SectorsPerPage = (NvU32)NandUtilGetLog2(SectorsPerPage);
    // set pages per block log2
    nsCache->Log2PagesPerBlock = NandUtilGetLog2((NvU32)
        nsCache->DeviceInfo.PgsPerBlk);
    // Allocate temporary buffer needed to merge page read data and 
    // cache buffer
    nsCache->ReadData = (NvU8 *)AllocateVirtualMemory(
        (NvU32)(MACRO_MULT_POW2_LOG2NUM(sizeof(NvU8), 
        (nsCache->Log2SectorsPerPage + nsCache->Log2AppBytesPerSector))), 
        SECTOR_CACHE_ALLOC);
    // initialize temporary read buffer used for deferred read of 
    // sectors in page that are unused
    NvOsMemset(nsCache->ReadData, 0, 
        (NvU32)(MACRO_MULT_POW2_LOG2NUM(sizeof(NvU8), 
        (nsCache->Log2SectorsPerPage + nsCache->Log2AppBytesPerSector))));
    // Initialize each cache line data structure
    for (i = 0; i < CACHE_LINES; i++)
    {
        // Initialize cache line info
        CacheLineInit(hNand, &nsCache->lines[i]);
        // set the sector address
        nsCache->lines[i].sector.data = (NvS32*)sectorAddress;
        // back pointer from sector to cache line structure
        nsCache->lines[i].sector.line = &nsCache->lines[i];
        // reset address field
        nsCache->lines[i].sector.address = 
            NAND_RESET_SECTOR_CACHE_PAGE_ADDRESS;
        // set the line address
        nsCache->lines[i].data = (NvS32*)sectorAddress;

        // move data pointer to be used to initialize buffer for 
        // next cache line
        sectorAddress += NandTLGetSectorSize(hNand);
    }
#if DISABLE_READ_SECTOR_CACHE
    // allocate page sized buffer to enable 512B sector read/write
    hNand->BufSector512B = NvOsAlloc(sizeof(NvU8) * nsCache->DeviceInfo.PageSize);
    if (!hNand->BufSector512B)
    {
        NvOsDebugPrintf("\nError: failed to allocate buffer for 512B sector support ");
    }
    else
    {
        NvOsMemset(hNand->BufSector512B, 0xFF, 
            (sizeof(NvU8) * nsCache->DeviceInfo.PageSize));
    }
#endif
}

/*
 ******************************************************************************
 */

Sector* NandSectorCacheGetSector(
    NvNandHandle hNand,
    NvS32 lun,
    NvU32 PageStart,
    NvS32 PageCount)
{
    NvU32 lineCount;
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;

#if PRINT_FN_NAMES
    SC_DEBUG_INFO(("\nNandSectorCacheGetSector pg Start=0x%x, pg Count=0x%x,UNused=%d ",
        PageStart, PageCount, State_UNUSED));
#endif
    for (lineCount = 0; lineCount < CACHE_LINES; lineCount++)
    {
#if PRINT_FN_NAMES
        SC_DEBUG_INFO(("[index=%d, state=%d, lun=%d, expLun=%d, addr=0x%x] ", 
            lineCount, nsCache->lines[lineCount].state, 
            nsCache->lines[lineCount].lun, lun, 
            nsCache->lines[lineCount].address));
#endif
        if ((nsCache->lines[lineCount].state != State_UNUSED) &&
            (nsCache->lines[lineCount].lun == lun))
        {
            NvU32 lineAddress = nsCache->lines[lineCount].address;
            NvU32 lastSectorAddress = PageStart + (PageCount - 1);

            if ((lineAddress <= lastSectorAddress) &&
                (lineAddress >= PageStart))
            {

                // Next time CacheGetSector is called the page returned 
                // in last call would be in state UNUSED
                return &nsCache->lines[lineCount].sector;
            }
        }
    }
    return 0;
}

/*
 ******************************************************************************
 */
// Function to update data to write into cache line
// SectorOffset - start sector number to be written. Value < sectors/page
// SectorCount - number of sectors to be written. Less than sectors per page(4)
// When sector size equals Nand Page size
//      function is passed SectorOffset == 0 and SectorCount == 0
NvError NandSectorCacheSetSectorData(
    NvNandHandle hNand,
    Sector* sector,
    NvU8 *data, NvU32 SectorOffset, NvU32 SectorCount)
{
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    NvU32 i;
    NvU32 SizeInBytes;
    NvError returnValue = NvSuccess;
    // Cached write only possible for write within a page
    if (!(((SectorOffset + SectorCount) <= 
        ((NvU32)MACRO_POW2_LOG2NUM(nsCache->Log2SectorsPerPage))) && 
        (SectorCount)))
     {
        NvOsDebugPrintf("\nError: trying cached write past page limits ");
        return NvError_NandReadFailed;
    }
    SC_DEBUG_INFO(("\nNandSectorCacheSetSectorData "));
    // Sanity check for parameters - sector pointer and data array
    if (sector == 0 || data == 0)
    {
        MACRO_RETURN_ERR(NvError_NandTLFailed);
    }

    if (MACRO_CONDITION_NAND_512B_SECTOR)
    {
        // Copy 512B or sector sized data when sector size is 
        // smaller than Nand Page size
        SizeInBytes = (NvU32)(MACRO_MULT_POW2_LOG2NUM(SectorCount, nsCache->Log2AppBytesPerSector));
    }
    else
    {
        // Copy Nand Page size data when sector size equals Nand Page size
        SectorOffset = 0;
        // SectorCount must be 1 as multi page writes are not cached
        SectorCount = 1;
        SizeInBytes = NandTLGetSectorSize(hNand);
    }
    // Copy sectors within page for partial page write
    NvOsMemcpy(((NvU8 *)sector->data + 
        (NvU32)(MACRO_MULT_POW2_LOG2NUM(SectorOffset, nsCache->Log2AppBytesPerSector))), data, SizeInBytes);
    
    MACRO_NAND_PRINT_ARRAY((NvU8 *)sector->data + 
        (NvU32)(MACRO_MULT_POW2_LOG2NUM(SectorOffset, nsCache->Log2AppBytesPerSector)), 
        ((NvU32)(MACRO_MULT_POW2_LOG2NUM(sector->line->address, nsCache->Log2SectorsPerPage)) + SectorOffset),
        SizeInBytes, 
        NAND_BYTES_TO_PRINT, NAND_BYTES_PER_ROW);
    // Update cache state of sectors written within page
    for (i = SectorOffset; i < (SectorOffset + SectorCount); i++)
    {
        sector->line->SubState[i] = State_DIRTY;
    }
    // Set page cache state after write as dirty
    if (sector->line->state != State_DIRTY)
        sector->line->state = State_DIRTY;
#if NAND_WAR_FLUSH_CHECK
    returnValue = NandFlushSector(hNand, sector);
    UtilSetCacheLineState(hNand, sector->line, State_UNUSED);
#endif
    return returnValue;
}

/*
 ******************************************************************************
 // If invalidate is NV_FALSE, the cache line is flushed but still valid
 */
NvError NandSectorCacheFlush(NvNandHandle hNand, NvBool invalidate)
{
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    NvError returnValue = NvSuccess;
    State newState = (invalidate)?
                     State_UNUSED : State_CLEAN;
    NvU32 lineCount;
    NvU32 MinIndex;
    Sector *pSector = NULL;
    NvU32 NextStart = 0;

#if PRINT_FN_NAMES
    SC_DEBUG_INFO(("\nNandSectorCacheFlush "));
#endif
#if NAND_512B_DBG_PRINT
    if (s_Enable512BPrint)
    {
        NvOsDebugPrintf("{Flush ALL} ");
    }
#endif
    do
    {
        // Reset required variables before each interation
        MinIndex = CACHE_LINES;
        pSector = NULL;
        // Loop to find minimum address page for MLC
        // For SLC Loop goes by increasing sector cache line index 
        for (lineCount = NextStart; lineCount < CACHE_LINES; lineCount++)
        {
            // Pages that are flushed earlier would have state as newState
            // hence excluding them
            if ((nsCache->lines[lineCount].state != State_UNUSED) &&
                (nsCache->lines[lineCount].state != newState))
            {
                if (nsCache->DeviceInfo.DeviceType == MLC)
                {
                    // MLC need ordered flush
                    NvU32 lineAddress = nsCache->lines[lineCount].address;
                    // Next start initialized to be from 0
                    NextStart = 0;
                    // Keep track of the smallest page address 
                    if (MinIndex >= CACHE_LINES)
                    {
                        // This is the first entry found in range
                        MinIndex = lineCount;
                    }
                    else
                    {
                        // update the minimum page index
                        if (nsCache->lines[MinIndex].address > lineAddress)
                        {
                            MinIndex = lineCount;
                        }
                    }
                }
                else
                {
                    // Non-MLC do not need ordered flush 
                    NextStart = lineCount + 1;
                    MinIndex = lineCount;
                    // Found one used entry to flush 
                    break;
                }
            }
        }
        // Flush the minimum address page
        if (MinIndex < CACHE_LINES)
            pSector = &nsCache->lines[MinIndex].sector;
        // If we found sector that is used pSector will be non-NULL
        if (pSector)
        {
            switch(pSector->line->state)
            {
                case State_DIRTY:
                    // Flush a Nand Cache line
                    returnValue = NandFlushSector(hNand, pSector);
                    if (returnValue != NvSuccess)
                        MACRO_RETURN_ERR(returnValue);
                    // fall through
                case State_CLEAN:
                    // update sub bucket
                    // Cache line state is changed to newState
                    UtilSetCacheLineState(hNand, pSector->line, newState);
                    break;
                default:
                    // Illegal state
                    NV_ASSERT(NV_FALSE);
                    break;
            }
            // 
        }
        // Check for more cache lines when we last time we found used lines
    } while (pSector);

    return returnValue;
}

/*
 ******************************************************************************
 */
NvError NandSectorCacheRead(
    NvNandHandle hNand,
    NvS32 lun,
    NvU32 sectorAddr,
    NvU8 *data,
    NvS32 sectorCount)
{
    NvS32 returnStatus = NvSuccess;
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    // Save sector address and size
    NvU32 SavedSectorCount = sectorCount;
    NvU32 SavedSectorAddr = sectorAddr;
#if PRINT_FN_NAMES
    SC_DEBUG_INFO(("\nNandSectorCacheRead Start Sector0x%x, SectorCount=0x%x ", 
        SavedSectorAddr, SavedSectorCount));
#endif
    // Calculate the Nand page count 
    if (MACRO_CONDITION_NAND_512B_SECTOR)
    {
        // Sector size 512B
        // convert the sector parameters to page parameters
        UtilConvSectorToPageArgs(SavedSectorAddr, SavedSectorCount, 
            (NvU32)(nsCache->Log2SectorsPerPage), &sectorAddr, (NvU32 *)&sectorCount);
    }
#if NAND_512B_DBG_PRINT
    if (s_Enable512BPrint)
    {
        NvU32 Val;
        NvOsDebugPrintf("\n{Rd Start=0x%x,Cnt=0x%x,Pg=0x%x,pgCnt=%u} ", 
            SavedSectorAddr, SavedSectorCount, sectorAddr, sectorCount);
        // Print sector cache state after 10 calls to read or write
        Val = s_512Bcount & ((1 << LOG2_NAND_SECTOR_CACHE_LOG_CALL_COUNT) - 1);
        // Print sector cache state after 10 calls to read or write
        if (!(Val))
        {
            UtilPrintSectorCache(hNand);
        }
       if ((s_512Bcount) && (!(s_512Bcount % NAND_WAIT_512B_LOG)))
        {
            // Used to stop and allow user to collect the partial dump from PB
            volatile NvU32 Killer9 = 0;
            while(Killer9);
        }
        s_512Bcount++;
    }
#endif
    // sector count here is in terms of Nand pages

    // Multiple page writes are not cached and handled by TL write API
    returnStatus = NandMultiPageRead(hNand, lun, SavedSectorAddr,
        data, SavedSectorCount, sectorAddr, sectorCount);

    return returnStatus;
}

/*
 ******************************************************************************
 */
NvError NandSectorCacheWrite(
    NvNandHandle hNand,
    NvS32 lun,
    NvU32 sectorAddr,
    NvU8 *data,
    NvS32 sectorCount)
{
    NvError returnValue = NvSuccess;
    NandSectorCache* nsCache = hNand->NandTl.SectorCache;
    // Save sector address and size
    NvU32 SavedSectorCount = sectorCount;
    NvU32 SavedSectorAddr = sectorAddr;
#if PRINT_FN_NAMES
    SC_DEBUG_INFO(("\nNandSCWrite Start=0x%x, Count=0x%x ", 
        SavedSectorAddr, SavedSectorCount));
#endif
    // Calculate the Nand page count 
    if (MACRO_CONDITION_NAND_512B_SECTOR)
    {
        // convert the sector parameters to page parameters
        UtilConvSectorToPageArgs(SavedSectorAddr, SavedSectorCount, 
            nsCache->Log2SectorsPerPage, &sectorAddr, (NvU32 *)&sectorCount);
    }
#if NAND_512B_DBG_PRINT
    if (s_Enable512BPrint)
    {
        NvU32 Val;
        NvOsDebugPrintf("\n{Wr Start=0x%x,Cnt=0x%x,Pg=0x%x,pgCnt=%u} ", 
            SavedSectorAddr, SavedSectorCount, sectorAddr, sectorCount);
        Val = s_512Bcount & ((1 << LOG2_NAND_SECTOR_CACHE_LOG_CALL_COUNT) - 1);
        // Print sector cache state after 10 calls to read or write
        if (!(Val))
        {
            UtilPrintSectorCache(hNand);
        }
        if ((s_512Bcount) && (!(s_512Bcount % NAND_WAIT_512B_LOG)))
        {
            // Used to stop and allow user to collect the partial dump from PB
            volatile NvU32 Killer9 = 0;
            while(Killer9);
        }
        s_512Bcount++;
    }
#endif
    // sector count here is in terms of Nand pages

    SC_DEBUG_INFO(("\r\n\t\t\t\t*** SC_WRITE %d sectors from %d addr", 
        sectorCount, sectorAddr));
    // Multiple page writes are not cached and handled by TL write API
    returnValue = NandMultiPageWrite(hNand, lun, SavedSectorAddr,
        data, SavedSectorCount, sectorAddr, sectorCount);

    return returnValue;
}

