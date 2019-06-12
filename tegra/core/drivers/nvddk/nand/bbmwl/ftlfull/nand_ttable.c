/**
 * @file
 * @brief Implements the translation table
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

#include "nand_ttable.h"
#include "nvnandtlfull.h"
#include <string.h>
#include <stdio.h>

#define DISABLE_ALIGNMENT 0
// Value of TT entry physical block that is uninitialized 
#define BLOCK_NUM_CLEAR 0x00FFFFFF

static NvS32 GetCacheBucket(TranslationTableCache *pTTCache);

static NvBool IsCached(
    TranslationTableCache *pTTCache,
    NvS32 PageNum,
    NvS32* pBucketNum);

static NvS8* CachePage(NvNandHandle hNand, NvS32 LogicalPage, NvS32* pBucket);

static void MarkPageDirty(NvNandHandle hNand, NvS32 LogicalPageNumber);

static NvBool ValidateLBA(NvNandHandle hNand, NvS32 lba);

static NvBool ValidatePBA(NvNandHandle hNand, NvS32 pba);
NvError PrintData(NvNandHandle hNand);

// static NvBool eraseBlock(NvNandHandle hNand, NvS32 bank, NvS32 PhysicalBlock);

static NvS32 GetLastPbaUsed(NvNandHandle hNand);

void ForceTTFlush(NvNandHandle hNand);

NvS32 GetRegionIdFromTT(NvNandHandle hNand, NvS32 bank, NvS32* pPhysBlks);

static void ConvertBlockNoToPageNo(
    NvNandHandle hNand,
    NvS32* pBlocks,
    NvS32* pPages,
    NvS32 pageOffset);

void NandStrategyWriteTest(NvNandHandle hNand);
/**
 * @brief This method erases the specified physical block.
 *
 * @param bank bank number.
 * @param physBlk physical block number to erase.
 *
 * @return returns true, if the erase is successful.
 */
static NvBool
EraseBlock(
    NvNandHandle hNand,
    NvS32 bank,
    NvS32* physBlk);
/**
 * @brief This method erases the blocks specified in the physical page array.
 * if erase fails, then it marks the failed block as bad and
 * gets new block from the block's zone.
 *
 * @param pBlocks ponter to physical block array for erase.
 *
 * @return returns true, if the erase is successful.
 */
static NvBool 
EraseInterleaveBlock(NvNandHandle hNand, NvS32* pBlocks);

NvError
ReadTagInfo(
           NvNandHandle hNand,
           NvS32* pPages,
           NvS32 SectorOffset,
           NvU8 *pSpareBuffer);

NvError
WriteTagInfo(
            NvNandHandle hNand,
            NvS32 *pPages,
            NvS32 SectorOffset,
           NvU8 *pSpareBuffer);
static NvError RecoverLostBlocks(NvNandHandle hNand);

void NandStrategyReadTest(NvNandHandle hNand);

#define ENABLE_ERASE_IN_GET_PBA 0
// static void dumpSpareArea(NvS32 page);
/*****************************************************************************/

NvError InitNandInterleaveTTable(NvNandHandle hNand)
{
    NvS32 CachedPage;
    NvU64 PageSize;
    NvU64 NumBanks;
    NvError RetStatus = NvSuccess;

    hNand->pNandTt->lastUsedPba = 0;
    hNand->pNandTt->pPhysPgs = 0;
    hNand->pNandTt->ttEntriesPerCachePage = 0;
    hNand->pNandTt->NoOfILBanks = hNand->InterleaveCount;
    hNand->pNandTt->bFlushTTable = NV_FALSE;
    hNand->pNandTt->pZones = 0;
    hNand->pNandTt->pTTPages = 0;
    hNand->pNandTt->pEntries = 0;

    // all the init functions for the corresponding member structures
    InitTranslationTableCache(&hNand->pNandTt->ttCache);

    // call the initialize function over here

    hNand->pNandTt->ttEntriesPerCachePage = hNand->NandDevInfo.PageSize /
                                            (sizeof(BlockStatusEntry) *
                                            hNand->InterleaveCount);
    SHOW(("\r\n hNand->pNandTt->ttEntriesPerCachePage = %d", hNand->pNandTt->ttEntriesPerCachePage));
    PageSize = hNand->NandDevInfo.PageSize * TOTAL_NUMBER_OF_TT_CACHE_PAGES;
    // downcast the PageSize to avoid compiler warning
    hNand->pNandTt->pTTCacheMemory = AllocateVirtualMemory(((NvU32) PageSize), TT_ALLOC);

    // get the memory for the TT cache buckets
    for (CachedPage = 0; CachedPage < TOTAL_NUMBER_OF_TT_CACHE_PAGES; CachedPage++)
    {
        hNand->pNandTt->ttCache.bucket[CachedPage].ttPage =
            (NvS8*)hNand->pNandTt->pTTCacheMemory +
            (hNand->NandDevInfo.PageSize *
            CachedPage);
        hNand->pNandTt->ttCache.bucket[CachedPage].ttPagePhysAdd =
        (NvS8 *)(hNand->pNandTt->pTTCacheMemory +
         (hNand->NandDevInfo.PageSize * CachedPage));
        if (!hNand->pNandTt->ttCache.bucket[CachedPage].ttPage)
            RETURN(NvError_NandTTFailed);
    }

    // since we are just starting all buckets are free
    hNand->pNandTt->ttCache.FreeCachedBkts = TOTAL_NUMBER_OF_TT_CACHE_PAGES;

    NvOsMemset(&hNand->ITat.Misc, 0, sizeof(MiscellaneousInfo));
    RetStatus = InitNandInterleaveTAT(hNand);
    ChkILTTErr(RetStatus, NvError_NandTLFailed);

    NumBanks = sizeof(NvS32) * hNand->NandCfg.BlkCfg.NoOfBanks;
    hNand->pNandTt->pPhysPgs = AllocateVirtualMemory((NvU32) NumBanks, TT_ALLOC);
    hNand->pNandTt->pIsNewBlockReserved = AllocateVirtualMemory(sizeof(NvBool) *
        hNand->pNandTt->NoOfILBanks, TT_ALLOC);
    hNand->pNandTt->pZones = AllocateVirtualMemory(sizeof(NvS32) *
        hNand->ITat.Misc.NumOfBanksOnBoard *
        hNand->ITat.Misc.ZonesPerBank, TT_ALLOC);
    hNand->pNandTt->pTTPages = AllocateVirtualMemory(sizeof(NvS32) *
        hNand->pNandTt->NoOfILBanks, TT_ALLOC);
    hNand->pNandTt->pEntries = AllocateVirtualMemory(sizeof(NvS32) *
        hNand->pNandTt->NoOfILBanks, TT_ALLOC);

    if (!hNand->pNandTt->pPhysPgs || !hNand->pNandTt->pIsNewBlockReserved ||
        !hNand->pNandTt->pZones || !hNand->pNandTt->pTTPages ||
        !hNand->pNandTt->pEntries)
    {
        DumpTT(hNand, -1);
        LOG_NAND_DEBUG_INFO(("\r\n No memory available"), ERROR_DEBUG_INFO);
        RETURN(NvError_NandTTFailed);
    }

    hNand->pNandTt->bsc4ttEntriesPerCachePage =
        (NvS32)NandUtilGetLog2(hNand->pNandTt->ttEntriesPerCachePage);
    hNand->pNandTt->bsc4PhysBlksPerZone =
        (NvS32)NandUtilGetLog2(hNand->ITat.Misc.PhysBlksPerZone);
    if (!hNand->pNandTt->bsc4ttEntriesPerCachePage ||
        !hNand->pNandTt->bsc4PhysBlksPerZone)
    {
        RETURN(NvError_NandTTFailed);
    }
    // get the last pba used info from TAT
    hNand->pNandTt->lastUsedPba =
        hNand->ITat.tatHandler.WorkingCopy.lastPhysicalBlockUsed;
    // NandUtility::OBJ().startTest();

    // ScanDevice();
    if (hNand->NandFallBackMode)
    {
        // Now recover any lost blocks because of power failure.
        RecoverLostBlocks(hNand);
    }
     //DumpTT(hNand, -1);
     PrintData(hNand);
    return NvSuccess;
}

void DeInitNandInterleaveTTable(NvNandHandle hNand)
{
    // Relase all the allocated memory.
    // Release the memory for the TT cache buckets
    if (hNand->pNandTt->pTTCacheMemory)
    {
        ReleaseVirtualMemory(hNand->pNandTt->pTTCacheMemory, TL_ALLOC);
    }

    NvOsFree(hNand->pNandTt->pPhysPgs);
    NvOsFree(hNand->pNandTt->pIsNewBlockReserved);
    NvOsFree(hNand->pNandTt->pZones);
    NvOsFree(hNand->pNandTt->pTTPages);
    NvOsFree(hNand->pNandTt->pEntries);
}

NvS32 GetRegionIdFromTT(NvNandHandle hNand, NvS32 bank, NvS32* pPhysBlks)
{
    NvS8* pTtPage = NULL;
    NvS32 Page2bCached;
    NvS32 ttEntry;
    NvS32 entry;
    BlockStatusEntry *pBlockSts;
    NvS32 pba;
    if (pPhysBlks == NULL)
        return 0;
    // physical block accessed for particular interleave column
    pba = pPhysBlks[bank];
    if (!ValidatePBA(hNand, pba))
        RETURN(NvError_NandTTFailed);

    // get the page number of the TT table in which we have the current lba entry
    Page2bCached = pba >> hNand->pNandTt->bsc4ttEntriesPerCachePage;
    // cache the page and get the cached data
    pTtPage = CachePage(hNand, Page2bCached, 0);
    if (!pTtPage)
        RETURN(NvError_NandTTFailed);

    pBlockSts = (BlockStatusEntry*)pTtPage;
    // get the offset of the logical block entry from the cached page
    ttEntry = pba % hNand->pNandTt->ttEntriesPerCachePage;
    // get the entry number w.r.t bank
    entry = GetTTableBlockStatusEntryNumber(hNand, bank, ttEntry);
    return pBlockSts[entry].Region;
}

NvBool EraseBlock(
    NvNandHandle hNand,
    NvS32 bank,
    NvS32* physBlk)
{
    NvS32 i;
    NvError Sts = NvSuccess;
    // clear the physical page array
    for (i = 0; i < hNand->NandStrat.NoOfILBanks; i++)
        hNand->NandStrat.pTmpPhysPgs[i] = -1;

    hNand->NandStrat.pTmpPhysPgs[bank] =
        *physBlk << hNand->NandStrat.bsc4PgsPerBlk;

    Sts = NandTLErase(hNand, hNand->NandStrat.pTmpPhysPgs);
    if (Sts != NvSuccess)
        return NV_FALSE;
    else
        return NV_TRUE;
}

static void ConvertBlockNoToPageNo(
    NvNandHandle hNand,
    NvS32* pBlocks,
    NvS32* pPages,
    NvS32 pageOffset)
{
    NvS32 bank;
    for (bank = 0; bank < hNand->NandStrat.NoOfILBanks; bank++)
    {
        if (pBlocks[bank] != -1)
        {
            pPages[bank] =
                (pBlocks[bank] << hNand->NandStrat.bsc4PgsPerBlk) +
                    pageOffset;
        }
    }
}

NvBool EraseInterleaveBlock(NvNandHandle hNand, NvS32* pBlocks)
{
    NvError eraseStatus;
    NvS32 PageNumbers[MAX_ALLOWED_INTERLEAVE_BANKS];
    NvS32 i = 0;
    NvBool Status = NV_TRUE;
    NvBool RetSts = NV_TRUE;
    
    ConvertBlockNoToPageNo(
        hNand,
        pBlocks,
        hNand->NandStrat.pTmpPhysPgs,
        0);

    for (i = 0; i < hNand->InterleaveCount; i++)
        PageNumbers[i] = hNand->NandStrat.pTmpPhysPgs[i];

    eraseStatus = NandTLErase(hNand, hNand->NandStrat.pTmpPhysPgs);
    if (eraseStatus != NvSuccess)
    {
        NvOsDebugPrintf("\n [Strategy] Erase Failed ");
        
        for (i = 0; i < hNand->InterleaveCount; i++)
        {
            if (PageNumbers[i] != -1)
            {
                Status = EraseBlock(hNand, i, &pBlocks[i]);
                if (Status == NV_FALSE)
                {
                    // set pba as bad
                    SetPbaBad(hNand, i, pBlocks[i]);
                    RetSts = NV_FALSE;
                }
                else
                {
                    SetPbaUnused(hNand, i, pBlocks[i]);
                }
            }
        }
    }
    return RetSts;
}


NvError
GetAlignedPBA(
    NvNandHandle hNand,
    int lba,
    int* pPrevPhysBlks,
    int* pPhysBlks,
    NvBool ForBadBlkMagement)
{
    NvError RetSts = NvSuccess;
    NvS32 Bank = 0;
    NvU32 RequestedBlocks[MAX_NAND_SUPPORTED];
#if DISABLE_ALIGNMENT
    NvU32 RefferenceIntCount = -1;
    NvU32 Loop = 5;
    NvU32 BlocksPerChip = hNand->NandDevInfo.NoOfPhysBlks;
    NvBool IsAlignmentReqd = NV_FALSE;
    NvBool GetNewBlocks = NV_FALSE;
#endif
    NvBool CheckForFreeBlocks = NV_FALSE;
    for (Bank = 0; Bank < hNand->pNandTt->NoOfILBanks; Bank++)
        RequestedBlocks[Bank] = pPhysBlks[Bank];

#if DISABLE_ALIGNMENT
    {
        if((hNand->pNandTt->NoOfILBanks == 4) &&
            (hNand->NandDevInfo.NumberOfDevices == 8))
            IsAlignmentReqd = NV_TRUE;
    }
#endif
    do
    {
#if DISABLE_ALIGNMENT
        do
        {
            GetNewBlocks = NV_FALSE;
            RefferenceIntCount = -1;
#endif

            for (Bank = 0; Bank < hNand->pNandTt->NoOfILBanks; Bank++)
            {
                DEBUG_TT_INFO(("\n Start LUP Bank %d: [chip ** PBA]: [%d**%d]\n",
                Bank, (hNand->pNandTt->lastUsedPba[Bank] / hNand->NandDevInfo.NoOfPhysBlks),
                hNand->pNandTt->lastUsedPba[Bank]));
            }

            RetSts = GetFreePBA(
                hNand,
                lba,
                pPrevPhysBlks,
                pPhysBlks,
                ForBadBlkMagement);
            if(RetSts != NvSuccess)
                goto Fail;
#if DISABLE_ALIGNMENT
            if(!IsAlignmentReqd)
                  goto FoundFreeBlocks;
            for (Bank = 0; Bank < hNand->pNandTt->NoOfILBanks; Bank++)
            {
                DEBUG_TT_INFO(("\n AlignedPBA: Bank %d: [chip ** PBA]: [%d**%d]\n",
                    (pPhysBlks[Bank] / hNand->NandDevInfo.NoOfPhysBlks),
                    pPhysBlks[Bank]));
            }
            for (Bank = 0; Bank < hNand->pNandTt->NoOfILBanks; Bank++)
            {
                DEBUG_TT_INFO(("\n END LUP: Bank %d [chip ** PBA]: [%d**%d]]\n\n\n",
                    (hNand->pNandTt->lastUsedPba[Bank] /  hNand->NandDevInfo.NoOfPhysBlks), 
                    hNand->pNandTt->lastUsedPba[Bank]));
            }
            for (Bank = 0; Bank < hNand->InterleaveCount; Bank++)
            {
                if(RequestedBlocks[Bank] == -1)
                {
                    if(RefferenceIntCount == -1)
                        RefferenceIntCount = pPhysBlks[Bank] / BlocksPerChip;
                    else if(RefferenceIntCount != (pPhysBlks[Bank] / BlocksPerChip))
                    {
                        GetNewBlocks = NV_TRUE;
                        break;
                    }
                }
            }

            if(!GetNewBlocks)
            {
                DEBUG_TT_INFO(("\n Found aligned  Bloks"));
                goto FoundFreeBlocks;
            }
            DEBUG_TT_INFO(("\n !!!!!!!!!!!!!!!!!!!!!!PBA realigned ##############"));
            for (Bank = 0; Bank < hNand->InterleaveCount; Bank++)
            {
                if(RequestedBlocks[Bank] == -1)
                {
                    RetSts = SetPbaUnused(hNand, Bank, pPhysBlks[Bank]);
                    pPhysBlks[Bank] = -1;
                    if(RetSts != NvSuccess)
                        goto Fail;
                }
            }
        }while(Loop--);
        if(GetNewBlocks)
        {
            NvOsDebugPrintf("\n\n\n\n\n Align block Failed \n\n\n");
            RetSts = NvError_NandNoFreeBlock;
            goto Fail;
        }
FoundFreeBlocks:
#endif
        CheckForFreeBlocks = EraseInterleaveBlock(hNand, pPhysBlks);
        // If erase fails, then we need to get new PBAs. Reset pPhysBlks to
        // originally requested value
        if (!CheckForFreeBlocks)
        {
            for (Bank = 0; Bank < hNand->pNandTt->NoOfILBanks; Bank++)
                pPhysBlks[Bank] = RequestedBlocks[Bank];
        }
    }while(!CheckForFreeBlocks);

Fail:
    return RetSts;
}

NvError
GetFreePBA(
    NvNandHandle hNand,
    int lba,
    int* pPrevPhysBlks,
    int* pPhysBlks,
    NvBool ForBadBlkMagement)
{
    // This is an important API as it covers the wear leveling logic
    // of the nand blocks.
    /** There are few things to understand in this implementation.
     * 1) whenever we get a request with pPrevPhysBlks as NULL,
     *      this means there is no PBA associated with the lba and
     *      we should get a free Physical block. In this case we don't consider
     *      data reserved blocks for the free block search.
     * 2) if pPrevPhysBlks is not NULL, this means the request for
     *      unused PBA is for copyback operations. So in this case we should
     *      try to use data reserved blocks, if possible.
     * 3) As a part of wear leveling logic the assignment of a new PBA
     *      need to start always from the previously allocated block.
     * 4) The physBlkAddress returned by this API is always relative
     *      to the interleaved coloumn.
     */

    NvBool checkForFreeBlocks = NV_FALSE;
    // Need to take care of this.
    NvBool skipResBlocks = (pPrevPhysBlks == NULL) ? NV_TRUE: NV_FALSE;
    NvBool breakFromLoop;
    NvBool freeBlocksAvailable = NV_FALSE;
    int bucketNum;
    int ttPage, lastPba, startPage, entryOffset, bank, pba, ttEntry, entry;
    NvU32 i;
    BlockStatusEntry *blockSts;
    int EndTtPage;
    
    for (i = 0; i < hNand->ITat.Misc.NoOfILBanks; i++)
    {
        hNand->pNandTt->pIsNewBlockReserved[i] = NV_FALSE;
        if (pPrevPhysBlks != NULL)
            hNand->pNandTt->pZones[i] = pPrevPhysBlks[i] >>
                hNand->pNandTt->bsc4PhysBlksPerZone;
    }

    EndTtPage = hNand->ITat.Misc.PgsRegForTT;
    do
    {
        checkForFreeBlocks = NV_FALSE;
        lastPba = GetLastPbaUsed(hNand);
        // convert last used pba to TT page/entry format
        startPage = lastPba >> hNand->pNandTt->bsc4ttEntriesPerCachePage;
        entryOffset = lastPba % hNand->pNandTt->ttEntriesPerCachePage;

        for (
            ttPage = startPage;
            ttPage < EndTtPage;
            ttPage++)
        {
            // get a bucket from the cache for our search operations
            if (!CachePage(hNand, ttPage, &bucketNum))
                RETURN(NvError_NandTTFailed);
            blockSts =
            (BlockStatusEntry*) hNand->pNandTt->ttCache.bucket[bucketNum].ttPage;

            for (bank = 0; bank < hNand->pNandTt->NoOfILBanks; bank++)
            {
                // check whether we got any free block for the interleave
                // coloumn, if so skip searching for that.
                if (pPhysBlks[bank] != -1)
                    continue;

                for (
                    ttEntry = entryOffset;
                    ttEntry < hNand->pNandTt->ttEntriesPerCachePage;
                    ttEntry++)
                {
                    pba = (ttPage <<
                        hNand->pNandTt->bsc4ttEntriesPerCachePage) + ttEntry;

                    //if (hNand->pNandTt->lastUsedPba[bank] > pba)
                        //continue;

                    entry = GetTTableBlockStatusEntryNumber(hNand, bank, ttEntry);
                    // If the pba is a system reserved entry; skip it
                    if (blockSts[entry].SystemReserved)
                        continue;

                    // If the block is bad; skip it
                    if (!blockSts[entry].BlockGood)
                        continue;

                    // if we are requested to ignore reserved block; do it
                    if (blockSts[entry].DataReserved)
                    {
                        if (skipResBlocks)
                        {
                            continue;
                        }
                    }
                    else
                    {
                        if (!skipResBlocks && ForBadBlkMagement)
                        {
                            continue;
                        }
                    }

                    // check wether the pba is used or not
                    if (blockSts[entry].BlockNotUsed)
                    {
                        NvS32 regionId = 0;
                        regionId =
                            GetRegionIdFromTT(hNand, bank, pPrevPhysBlks);
                        if (regionId != (NvS32)blockSts[entry].Region)
                            continue;

                        if (ENABLE_ERASE_IN_GET_PBA)
                        {
                            NvU32 PageNumbers[MAX_NAND_SUPPORTED] = {-1, -1, -1, -1, -1, -1, -1, -1};
                            PageNumbers[bank] = pba * hNand->NandDevInfo.PgsPerBlk;

                            if (NandTLErase(hNand, (NvS32 *)PageNumbers))
                            {
                                // if it is bad, mark in TT and continue to next block
                                blockSts[entry].BlockGood = 0;
                                MarkPageDirty(hNand, ttPage);
                                continue;
                            }
                        }

                        if (blockSts[entry].Region == 2)
                        {
                            // decrement used count
                            hNand->UsedRegion2Count++;
                            LOG_NAND_DEBUG_INFO(("\r\nRegion2 used: bank=%d,pba=%d ", bank, pba), TT_DEBUG_INFO);
                        }
                        // mark in TT that this pba is used
                        blockSts[entry].BlockNotUsed = 0;

                        // we need the following info to free the blocks,
                        // incase we don't get
                        // free blocks in all the banks.
                        hNand->pNandTt->pTTPages[bank] = ttPage;
                        hNand->pNandTt->pEntries[bank] = entry;

                        // we got a free pba. store it for future refferences
                        hNand->pNandTt->lastUsedPba[bank] = pba;
                        pPhysBlks[bank] = pba;
                        // Mark the bucket as dirty
                        MarkPageDirty(hNand, ttPage);

                        // if the newly allocated block is a reserved block,
                        // make a note of it
                        if (blockSts[entry].DataReserved)
                        {
                            hNand->pNandTt->pIsNewBlockReserved[bank] = NV_TRUE;
                            // don't do this here. do this at the time of
                            // assaigning to lba.
                            blockSts[entry].DataReserved = 0;
                        }
                        break;
                    }
                }
            }

            breakFromLoop = NV_TRUE;
            // check whether we found free blocks in all the banks.
            // if so, break from the loop
            for (bank = 0; bank < hNand->pNandTt->NoOfILBanks; bank++)
            {
                if (pPhysBlks[bank] == -1)
                {
                    breakFromLoop = NV_FALSE;
                    break;
                }
            }

            if (breakFromLoop)
            {
                freeBlocksAvailable = NV_TRUE;
                break;
            }
            entryOffset = 0;
        }

        // Check whether we reached the end. if so, roll over to the beginning
        // to check if we have any free blocks from the beginnig.
        if (ttPage >= hNand->ITat.Misc.PgsRegForTT)
        {
            checkForFreeBlocks = NV_TRUE;
            // we are here means, we couldn't find a a free block either
            // in all interleaved coloumns or in some coloumns.
            // start from the begining to find a free block for the coloumns
            // we didn't find the free block.
            for (bank = 0; bank < hNand->pNandTt->NoOfILBanks; bank++)
            {
                if (pPhysBlks[bank] == -1)
                {
                    if (hNand->pNandTt->lastUsedPba[bank] == 0)
                    {
                        // we are here means, either we already rolled over or
                        // we started from 0 and couldn't
                        // find the free blocks. i.e we don't have any free
                        // blocks. break from here.
                        checkForFreeBlocks = NV_FALSE;
                        NvOsDebugPrintf("\r\n No free blocks Available- \
                            find out the reason, bank = %d",
                            bank);
                        break;
                    }
                    else
                    {
                        hNand->pNandTt->lastUsedPba[bank] = 0;
                        
                        RW_TRACE(("\r\n ***** roll over to the \
                            starting to check if we have any free blocks, bank = %d\r\n",
                            bank));
                        PrintData(hNand);
                        FlushTranslationTable(hNand, -1, 0, NV_TRUE);
                        RW_TRACE(("\r\n LBA requested = %d",lba));
                        // when rolling back set loop limits to avoid repeat
                        EndTtPage = startPage;
                    }
                }
            }
        }
    }while (checkForFreeBlocks);

    // by this time we should get a free physical block.
    // if not that means we do not have any
    if (!freeBlocksAvailable)
    {
    #if 0
        // We are here mean that we don't have free blocks in any one of the
        // interleave banks or in all of the interleaved banks.
        // release the free blocks that are marked as used, if any.
        for (bank = 0; bank < hNand->pNandTt->NoOfILBanks; bank++)
        {
            if (pPhysBlks[bank] != -1)
            {
                // Load the required page into cache
                if (!CachePage(hNand, hNand->pNandTt->pTTPages[bank], &bucketNum))
                    RETURN(NvError_NandTTFailed);

                blockSts =
                (BlockStatusEntry*) hNand->pNandTt->ttCache.bucket[bucketNum].ttPage;
                blockSts[hNand->pNandTt->pEntries[bank]].BlockNotUsed = 1;
                if (hNand->pNandTt->pIsNewBlockReserved[bank])
                    blockSts[hNand->pNandTt->pEntries[bank]].DataReserved = 1;
                MarkPageDirty(hNand, hNand->pNandTt->pTTPages[bank]);
            }
        }
        ScanDevice(hNand);
    #endif
        ClearPhysicalPageArray(hNand, pPhysBlks);
        RETURN(NvError_NandNoFreeBlock);
    }

    // if the newFree Physical block(s) is a reserved block then make the old
    // physical block(s) as reserved.
    if (!skipResBlocks)
    {
        // ENABLE_DEBUG;
        for (i = 0; i < hNand->ITat.Misc.NoOfILBanks; i++)
        {
            if (hNand->pNandTt->pIsNewBlockReserved[i])
            {
                SetPbaReserved(hNand, i, pPrevPhysBlks[i], NV_TRUE);
                ERR_DEBUG_INFO(("\r\n released a rsvd block for usage blk = %d, bank = %d",
                    pPhysBlks[i], i));
                ERR_DEBUG_INFO(("\r\n Marking the blk = %d of bank = %d as rsvd", 
                    pPrevPhysBlks[i], i));
            }
        }
    }

    ERR_DEBUG_INFO(("\r\n Got unused Pbas "));
    for (bank = 0; bank < hNand->pNandTt->NoOfILBanks; bank++)
    {
        ERR_DEBUG_INFO(("%d ", pPhysBlks[bank]));
    }

    return NvSuccess;
}

NvError PrintData(NvNandHandle hNand)
{
    int bucketNum;
    int ttPage, bank, ttEntry, entry;
    BlockStatusEntry *blockSts;
    NvU32 UsedLba[4] = {0,0,0,0};
    NvU32 BadBlock[4] = {0,0,0,0};
    NvU32 UsedDataRsvd[4] = {0,0,0,0};
    NvU32 UnusedDataRsvd[4] = {0,0,0,0};
    NvU32 UnUsedPba[4] = {0,0,0,0};
    NvU32 SysRsvd[4] = {0,0,0,0};
    for (
        ttPage = 0;
        ttPage < hNand->ITat.Misc.PgsRegForTT;
        ttPage++)
    {
        // get a bucket from the cache for our search operations
        if (!CachePage(hNand, ttPage, &bucketNum))
            RETURN(NvError_NandTTFailed);
        blockSts =
        (BlockStatusEntry*) hNand->pNandTt->ttCache.bucket[bucketNum].ttPage;

        for (bank = 0; bank < hNand->pNandTt->NoOfILBanks; bank++)
        {

            for (
                ttEntry = 0;
                ttEntry < hNand->pNandTt->ttEntriesPerCachePage;
                ttEntry++)
            {
                entry = GetTTableBlockStatusEntryNumber(hNand, bank, ttEntry);
                
                if ((blockSts[entry].PhysBlkNum & 0x00FFFFFF) != 0x00FFFFFF)
                    UsedLba[bank]++;
                    
                if (blockSts[entry].SystemReserved)
                {
                    SysRsvd[bank]++;
                    continue;
                }
                // If the block is bad; skip it
                if (!blockSts[entry].BlockGood)
                {
                    BadBlock[bank]++;
                    continue;
                }

                // if we are requested to ignore reserved block; do it
                if (blockSts[entry].DataReserved)
                {
                    if(!blockSts[entry].BlockNotUsed)
                    {
                        UsedDataRsvd[bank]++;
                    }
                    else
                        UnusedDataRsvd[bank]++;
                    continue;
                }

                // check wether the pba is used or not
                if (blockSts[entry].BlockNotUsed)
                {
                    UnUsedPba[bank]++;
                }
            }
        }
    }

    for (bank = 0; bank < hNand->pNandTt->NoOfILBanks; bank++)
    {
        DEBUG_TT_INFO(("\r\n Bank %d, TotalLogicalBlocks %d, UsedPba %d, UnUsedPba %d",
            bank, hNand->ITat.Misc.TotalLogicalBlocks, UsedLba[bank], UnUsedPba[bank]));
        DEBUG_TT_INFO(("UnUsedDataRsvd %d, BadBlock %d,UsedDataRsvd %d, SysRsvd %d",
            UnusedDataRsvd[bank], BadBlock[bank], UsedDataRsvd[bank], SysRsvd[bank]));
        if(hNand->ITat.Misc.TotalLogicalBlocks >= (UsedLba[bank] + UnUsedPba[bank]))
        {
            ERR_DEBUG_INFO(("\r\n error Not enough free blocks"));
            //while(1);
        }
        NandStrategyGetTrackingListCount(hNand);
    }
    return NvSuccess;
}

NvError GetPBA(
    NvNandHandle hNand,
    NvS32 lba,
    NvS32* pPhysBlks)
{
    NvS8* pTtPage = NULL;
    NvS32 Page2bCached;
    NvS32 bank;
    NvS32 ttEntry;
    NvS32 ActualPba;
    NvS32 entry;
    BlockStatusEntry *pBlockSts = NULL;

    if (!ValidateLBA(hNand, lba))
        RETURN(NvError_NandTTFailed);
    // get the entry number in the translation table from the logical block address(lba).
    MapLBA(hNand, &lba);

    // get the page number of the TT table in which we have the current lba entry
    Page2bCached = lba >> hNand->pNandTt->bsc4ttEntriesPerCachePage;

    // cache the page and get the cached data
    pTtPage = CachePage(hNand, Page2bCached, 0);
    if (!pTtPage)
        RETURN(NvError_NandTTFailed);

    pBlockSts = (BlockStatusEntry*)pTtPage;
    // get the offset of the logical block entry from the cached page
    ttEntry = lba % hNand->pNandTt->ttEntriesPerCachePage;

    // Clear the flag indicating non-zero Region block
    hNand->IsNonZeroRegion = NV_FALSE;
    for (bank = 0; bank < hNand->pNandTt->NoOfILBanks; bank++)
    {
        // get the entry number w.r.t bank
        entry = GetTTableBlockStatusEntryNumber(hNand, bank, ttEntry);
        pPhysBlks[bank] = pBlockSts[entry].PhysBlkNum;
        if (pPhysBlks[bank] != BLOCK_NUM_CLEAR)
        {
            // Save the region number in the Ftl handle
            hNand->RegNum[bank] = pBlockSts[pPhysBlks[bank]].Region;
            // Set flag used to bypass code specific to WM
            if ((hNand->RegNum[bank]) && (!hNand->IsNonZeroRegion))
                hNand->IsNonZeroRegion = NV_TRUE;
        }
    }

    // check the pba entries. checking one interleave coloumn entry should be enough.
    ActualPba = pPhysBlks[0];
    ActualPba &= 0x00FFFFFF;

    if (ActualPba == 0x00FFFFFF)
    {
        // there is no PBA assocoaited to this lba. send the same info to the requester
        ClearPhysicalPageArray(hNand, pPhysBlks);
    }
    return NvSuccess;
}

NvError
AssignPba2Lba(
             NvNandHandle hNand,
             NvS32 *pPhysBlks,
             NvS32 lba,
             NvBool UpdateTagInfo)
{
    NvS32 Page2bCached;
    NvS32 ttEntry;
    NvS32 bank;
    NvS32 entry;
    NvS32 i;
    BlockStatusEntry *pBlockSts;
    NvS8* pTtPage = NULL;

#ifdef IL_STRAT_DEBUG
    NvS32 orgLBA = lba;
#endif
    RW_TRACE(("\r\n Assign LBA %d to",lba));

    if (!ValidateLBA(hNand, lba))
        RETURN(NvError_NandTTFailed);
    // get the entry number in the translation table from the logical block address(lba).
    MapLBA(hNand, &lba);

    // get the page number of the TT table in which we have the current lba entry
    Page2bCached = lba >> hNand->pNandTt->bsc4ttEntriesPerCachePage;

    // cache the page and get the cached data
    pTtPage = CachePage(hNand, Page2bCached, 0);
    if (!pTtPage)
        RETURN(NvError_NandTTFailed);

    pBlockSts = (BlockStatusEntry*)pTtPage;
    // get the offset of the logical block entry from the cached page
    ttEntry = lba % hNand->pNandTt->ttEntriesPerCachePage;

    for (bank = 0; bank < hNand->pNandTt->NoOfILBanks; bank++)
    {
        RW_TRACE(("\r\n PBA %d to",pPhysBlks[bank]));
        // get the entry number w.r.t bank
        entry = GetTTableBlockStatusEntryNumber(hNand, bank, ttEntry);
        // fill the pba info in the lba entry of the TT table
        pBlockSts[entry].PhysBlkNum = pPhysBlks[bank];
        // We are modofying the data in the page so make it as dirty
        MarkPageDirty(hNand, Page2bCached);
    }

    // if (orgLBA);
    LOG_NAND_DEBUG_INFO(("\r\n lba %d-->assinged PBAs:", orgLBA), TT_DEBUG_INFO);
    for (i = 0; i < hNand->pNandTt->NoOfILBanks; i++)
        LOG_NAND_DEBUG_INFO(("%d ", pPhysBlks[i]), TT_DEBUG_INFO);

    for (i = 0; i < hNand->pNandTt->NoOfILBanks; i++)
    {
        SetPbaReserved(hNand, i, pPhysBlks[i], NV_FALSE);
    }

    return NvSuccess;
}

NvError
SetPbaRegionInfo(
             NvNandHandle hNand,
             NvS32* pPhysBlks,
             NvS32 RegionNumber,
             NvBool IsDataReserved)
{
    NvS8* pTtPage = NULL;
    NvS32 Page2bCached;
    NvS32 ttEntry;
    NvS32 bank;
    NvS32 pba;
    NvS32 entry;
    BlockStatusEntry *pBlockSts;
    for (bank = 0; bank < hNand->pNandTt->NoOfILBanks; bank++)
    {
        pba = pPhysBlks[bank];
        
        if(RegionNumber < 0 || RegionNumber > MAX_NAND_SUBREGIONS)
            RETURN(NvError_NandTTFailed);
        
        if (!ValidatePBA(hNand, pba))
            RETURN(NvError_NandTTFailed);

        // get the page number of the TT table in which we have the current lba entry
        Page2bCached = pba >> hNand->pNandTt->bsc4ttEntriesPerCachePage;
        // cache the page and get the cached data
        pTtPage = CachePage(hNand, Page2bCached, 0);
        if (!pTtPage)
            RETURN(NvError_NandTTFailed);

        pBlockSts = (BlockStatusEntry*)pTtPage;
        // get the offset of the logical block entry from the cached page
        ttEntry = pba % hNand->pNandTt->ttEntriesPerCachePage;
        // get the entry number w.r.t bank
        entry = GetTTableBlockStatusEntryNumber(hNand, bank, ttEntry);

        // set the regiion info and make it as unsused
        pBlockSts[entry].Region = RegionNumber;
        if(IsDataReserved)
        {
            // Compaction blocks marked unused
            pBlockSts[entry].BlockNotUsed = 1;
            pBlockSts[entry].DataReserved = 1;
        }
        
        // We are modofying the data in the page so make it as dirty
        MarkPageDirty(hNand, Page2bCached);
    }
    LOG_NAND_DEBUG_INFO(("\r\n setting pba = %d of bank = %d as bad, entry = %d",
                         pba, bank, entry), BAD_BLOCK_DEBUG_INFO);
    return NvSuccess;
}

NvError SetPbaBad(NvNandHandle hNand, NvS32 bank, NvS32 pba)
{
    NvS8* pTtPage = NULL;
    NvS32 Page2bCached;
    NvS32 ttEntry;
    NvS32 entry;
    BlockStatusEntry *pBlockSts;

    if (!ValidatePBA(hNand, pba))
        RETURN(NvError_NandTTFailed);

    // get the page number of the TT table in which we have the current lba entry
    Page2bCached = pba >> hNand->pNandTt->bsc4ttEntriesPerCachePage;
    // cache the page and get the cached data
    pTtPage = CachePage(hNand, Page2bCached, 0);
    if (!pTtPage)
        RETURN(NvError_NandTTFailed);

    pBlockSts = (BlockStatusEntry*)pTtPage;
    // get the offset of the logical block entry from the cached page
    ttEntry = pba % hNand->pNandTt->ttEntriesPerCachePage;
    // get the entry number w.r.t bank
    entry = GetTTableBlockStatusEntryNumber(hNand, bank, ttEntry);

    // set the pba as bad in the tt entry
    pBlockSts[entry].BlockGood = 0;
    // We are modofying the data in the page so make it as dirty
    MarkPageDirty(hNand, Page2bCached);

    // set in the tag info of the pba that it is bad in redundant area.
    MarkBlockAsBad(hNand, bank, pba);
    LOG_NAND_DEBUG_INFO(("\r\n setting pba = %d of bank = %d as bad, entry = %d",
                         pba, bank, entry), BAD_BLOCK_DEBUG_INFO);
    return NvSuccess;
}

NvError SetPbaUnused(NvNandHandle hNand, NvS32 bank, NvS32 pba)
{
    NvS8 *pTtPage = NULL;
    NvS32 Page2bCached;
    NvS32 ttEntry;
    NvS32 entry;
    BlockStatusEntry *pBlockSts = NULL;

    if (!ValidatePBA(hNand, pba))
        RETURN(NvError_NandTTFailed);

    // get the page number of the TT table in which we have the current lba entry
    Page2bCached = pba >> hNand->pNandTt->bsc4ttEntriesPerCachePage;
    // cache the page and get the cached data
    pTtPage = CachePage(hNand, Page2bCached, 0);
    if (!pTtPage)
        RETURN(NvError_NandTTFailed);

    pBlockSts = (BlockStatusEntry*)pTtPage;
    // get the offset of the logical block entry from the cached page
    ttEntry = pba % hNand->pNandTt->ttEntriesPerCachePage;
    // get the entry number w.r.t bank
    entry = GetTTableBlockStatusEntryNumber(hNand, bank, ttEntry);

    // set the pba as unused in the tt entry
    pBlockSts[entry].BlockNotUsed = 1;
    // We are modifying the data in the page so make it as dirty
    MarkPageDirty(hNand, Page2bCached);
    if (pBlockSts[entry].Region == 2)
    {
        // decrement used count
        hNand->UsedRegion2Count--;
        LOG_NAND_DEBUG_INFO(("\r\nRegion2 freed: bank=%d,pba=%d ", bank, pba), TT_DEBUG_INFO);
    }
    LOG_NAND_DEBUG_INFO(("\r\n setting pba = %d of bank = %d as unused, entry = %d",
                         pba, bank, entry), TT_DEBUG_INFO);
    return NvSuccess;
}

NvError
SetPbaReserved(
              NvNandHandle hNand,
              NvS32 bank,
              NvS32 pba,
              NvBool Reserved)
{
    NvS8* pTtPage = NULL;
    NvS32 Page2bCached;
    NvS32 ttEntry;
    NvS32 entry;
    BlockStatusEntry *pBlockSts;

    if (!ValidatePBA(hNand, pba))
        RETURN(NvError_NandTTFailed);

    // get the page number of the TT table in which we have the current lba entry
    Page2bCached = pba >> hNand->pNandTt->bsc4ttEntriesPerCachePage;
    // cache the page and get the cached data
    pTtPage = CachePage(hNand, Page2bCached, 0);
    if (!pTtPage)
        RETURN(NvError_NandTTFailed);

    pBlockSts = (BlockStatusEntry*)pTtPage;
    // get the offset of the logical block entry from the cached page
    ttEntry = pba % hNand->pNandTt->ttEntriesPerCachePage;
    // get the entry number w.r.t bank
    entry = GetTTableBlockStatusEntryNumber(hNand, bank, ttEntry);

    // set the pba as data reserved in the tt entry
    if (pBlockSts[entry].DataReserved)
    {
        if (Reserved == NV_FALSE)
        {
            pBlockSts[entry].DataReserved = 0;
            // We are modifying the data in the page so make it as dirty
            MarkPageDirty(hNand, Page2bCached);
        }
    }
    else
    {
        if (Reserved == NV_TRUE)
        {
            pBlockSts[entry].DataReserved = 1;
            // We are modifying the data in the page so make it as dirty
            MarkPageDirty(hNand, Page2bCached);
        }
    }
    return NvSuccess;
}

NvBool IsPbaReserved(NvNandHandle hNand, NvS32 bank, NvS32 pba)
{
    NvS8* pTtPage = NULL;
    NvS32 Page2bCached;
    NvS32 ttEntry;
    NvS32 entry;
    BlockStatusEntry *pBlockSts = NULL;

    if (!ValidatePBA(hNand, pba))
        return NV_FALSE;

    // get the page number of the TT table in which we have the current lba entry
    Page2bCached = pba >> hNand->pNandTt->bsc4ttEntriesPerCachePage;
    // cache the page and get the cached data
    pTtPage = CachePage(hNand, Page2bCached, 0);
    if (!pTtPage)
        return NV_FALSE;

    pBlockSts = (BlockStatusEntry*)pTtPage;
    // get the offset of the logical block entry from the cached page
    ttEntry = pba % hNand->pNandTt->ttEntriesPerCachePage;
    // get the entry number w.r.t bank
    entry = GetTTableBlockStatusEntryNumber(hNand, bank, ttEntry);

    // see whether the pba is data reserved in the tt entry.
    if (pBlockSts[entry].DataReserved)
        return NV_TRUE;

    return NV_FALSE;
}

NvError
ReleasePbaFromReserved(
                      NvNandHandle hNand,
                      NvS32 bank,
                      NvS32 zone)
{
    NvS32 StartPage = zone * hNand->ITat.Misc.TtPagesRequiredPerZone;
    NvS32 EndPage = StartPage + hNand->ITat.Misc.TtPagesRequiredPerZone;
    NvS32 BucketNum;
    NvBool ReleasingDone = NV_FALSE;
    NvS32 entry;
    NvS32 ttEntry;
    NvS32 ttPage;
    BlockStatusEntry *pBlockSts = NULL;

    do
    {
        for (ttPage = StartPage; ttPage < EndPage; ttPage++)
        {
            // Load the required page into cache
            if (!CachePage(hNand, ttPage, &BucketNum))
                RETURN(NvError_NandTTFailed);
            pBlockSts =
            (BlockStatusEntry*)hNand->pNandTt->ttCache.bucket[BucketNum].ttPage;

            for (
                ttEntry = 0;
                ttEntry < hNand->pNandTt->ttEntriesPerCachePage;
                ttEntry++)
            {
                entry = GetTTableBlockStatusEntryNumber(hNand, bank, ttEntry);
                // If the pba is a system reserved entry; skip it
                if (pBlockSts[entry].SystemReserved)
                    continue;
                // If the block is bad; skip it
                if (!pBlockSts[entry].BlockGood)
                    continue;
                // we are interested only in reserved blocks as we are
                // returning the PBA for bad block replacement
                if (!pBlockSts[entry].DataReserved)
                    continue;

                // check wether the pba is used or not
                if (pBlockSts[entry].BlockNotUsed)
                {
                    // mark in TT that this pba is not reserved any more.
                    pBlockSts[entry].DataReserved = 0;

                    // Make the bucket as dirty
                    MarkPageDirty(hNand, ttPage);
                    ReleasingDone = NV_TRUE;
                    break;
                }
            }

            if (ReleasingDone == NV_TRUE)
            {
                break;
            }
        }

        if (ReleasingDone == NV_TRUE)
        {
            break;
        }

        // Check whether we have already traversed the entire TT.
        if ((StartPage == 0) && (EndPage == hNand->ITat.Misc.PgsRegForTT))
        {
            break;
        }

        // Free blocks are not available in it's zone.
        // Then look for the free blocks in other zone.
        StartPage = 0;
        EndPage = hNand->ITat.Misc.PgsRegForTT;
    }while (1);

    if (ReleasingDone == NV_TRUE)
    {
        LOG_NAND_DEBUG_INFO(("\r\n released a Pba From resered blocks of a zone = %d",
                             zone), TT_DEBUG_INFO);
        return NvSuccess;
    }
    else
    {
        LOG_NAND_DEBUG_INFO(("\r\n can't release a Pba From resered blocks of \
            a zone = %d",
            zone), ERROR_DEBUG_INFO);
        RETURN(NvError_NandTTFailed);
    }
}

NvError
FlushTranslationTable(
                     NvNandHandle hNand,
                     NvS32 LogicalPage,
                     NvS8* pData,
                     NvBool doTATFlush)
{
    NvS32 CachedPage;
    do
    {
        if (pData && (LogicalPage != -1))
        {
            NvS32 BucketNum;
            if (!IsCached(&hNand->pNandTt->ttCache, LogicalPage, &BucketNum))
                RETURN(NvError_NandTTFailed);
            hNand->pNandTt->tatStatus = FlushCachedTT(hNand, LogicalPage, pData);
            CheckNvError(hNand->pNandTt->tatStatus, NvError_NandTTFailed);
            hNand->pNandTt->ttCache.bucket[BucketNum].IsDirty = NV_FALSE;
        }
        else
        {
            for (
                CachedPage = 0;
                CachedPage < TOTAL_NUMBER_OF_TT_CACHE_PAGES;
                CachedPage++)
            {
                if ((hNand->pNandTt->ttCache.bucket[CachedPage].LogicalPageNumber >= 0) &&
                    hNand->pNandTt->ttCache.bucket[CachedPage].IsDirty)
                {
                    hNand->pNandTt->tatStatus =
                       FlushCachedTT(hNand,
                       hNand->pNandTt->ttCache.bucket[CachedPage].LogicalPageNumber,
                       hNand->pNandTt->ttCache.bucket[CachedPage].ttPagePhysAdd);
                    CheckNvError(hNand->pNandTt->tatStatus, NvError_NandTTFailed);
                }
                hNand->pNandTt->ttCache.bucket[CachedPage].IsDirty = NV_FALSE;
            }

            if (doTATFlush == NV_TRUE)
            {
                hNand->pNandTt->tatStatus = FlushTAT(hNand);
                CheckNvError(hNand->pNandTt->tatStatus, NvError_NandTTFailed);
            }
        }

        // this flag can be set as a result of calling flushCachedTT.
        // so check it and flush entire TTable if needed.
        if (hNand->pNandTt->bFlushTTable)
        {
            LogicalPage = -1;
            hNand->pNandTt->bFlushTTable = NV_FALSE;
            doTATFlush = NV_TRUE;
        }
        else
        {
            break;
        }
    }while (1);

    return NvSuccess;
}

void InitTranslationTableCache(TranslationTableCache *ttCache)
{
    NvS32 i;
    ttCache->FreeCachedBkts = 0;
    for (i = 0; i < TOTAL_NUMBER_OF_TT_CACHE_PAGES; i++)
    {
        InitBucket(&ttCache->bucket[i]);
    }
}

/**
 * @brief This method gets the cache bucket number.
 *
 */
NvS32 GetCacheBucket(TranslationTableCache *pTtCache)
{
    NvS32 RetBucket = 0;
    NvS32 CachedPage;
    NvU64 OldestAccessed;

    if (pTtCache->FreeCachedBkts)
    {
        // we still have free buckets, check where it is and return it
        for (
            CachedPage = 0;
            CachedPage < TOTAL_NUMBER_OF_TT_CACHE_PAGES;
            CachedPage++)
        {
            if (pTtCache->bucket[CachedPage].LogicalPageNumber < 0)
            {
                pTtCache->FreeCachedBkts--;
                return CachedPage;
            }
        }
    }
    else
    {
        // we dont have any free buckets. search for last allocated
        // bucket and return it
        OldestAccessed = pTtCache->bucket[RetBucket].LastAccessTick;
        for (
            CachedPage = 0;
            CachedPage < TOTAL_NUMBER_OF_TT_CACHE_PAGES;
            CachedPage++)
        {
            if (pTtCache->bucket[CachedPage].LastAccessTick < OldestAccessed)
            {
                RetBucket = CachedPage;
                OldestAccessed = pTtCache->bucket[CachedPage].LastAccessTick;
            }
        }
    }

    return RetBucket;
}

/**
 * @brief This method tells whether the specified page is already present in cache.
 *
 * @param pageNum page number.
 * @param bucketNum returns the bucket number, if the page is present in cache.
 *
 * @return returns NV_TRUE, if the requested page is present in the cache.
 */
NvBool IsCached(TranslationTableCache *pTtCache, NvS32 PageNum, NvS32 *pBucketNum)
{
    NvS32 CachedPage;

    *pBucketNum = -1;
    for (CachedPage = 0; CachedPage < TOTAL_NUMBER_OF_TT_CACHE_PAGES; CachedPage++)
    {
        if (pTtCache->bucket[CachedPage].LogicalPageNumber == PageNum)
        {
            *pBucketNum = CachedPage;
            pTtCache->bucket[*pBucketNum].LastAccessTick = NvOsGetTimeUS();
            break;
        }
    }
    return(*pBucketNum >= 0) ? (NV_TRUE) : (NV_FALSE);
}

// these are private functions of the class ....

/**
 * @brief   This API caches the requested logical TT page.
 *
 * @param   LogicalPageNumber is the logical page number for the translation
 *          table page that needs to be cached
 * @param   bucket is the pointer to the translation table table that is
 *          cached
 *
 * @return  pointer to the loaded page buffer.
 */
NvS8* CachePage(NvNandHandle hNand, NvS32 LogicalPage, NvS32 *pBucket)
{
    NvS32 BucketNum;
    NvS32 sts;
    if ((LogicalPage < 0) || (LogicalPage >= hNand->ITat.Misc.PgsRegForTT))
        RETURN(NULL);

    // check if the requested page is already cached
    if (!IsCached(&hNand->pNandTt->ttCache, LogicalPage, &BucketNum))
    {
        // requested page number is not cached. go cache the page
        BucketNum = GetCacheBucket(&hNand->pNandTt->ttCache);

        // check if the bucket is already holding a cached page
        if (hNand->pNandTt->ttCache.bucket[BucketNum].LogicalPageNumber >= 0)
        {
            // YES, our cached bucket has some data in it.
            // Flush the data if it is dirty
            if (hNand->pNandTt->ttCache.bucket[BucketNum].IsDirty)
            {
                sts = FlushTranslationTable(hNand,
                  hNand->pNandTt->ttCache.bucket[BucketNum].LogicalPageNumber,
                  hNand->pNandTt->ttCache.bucket[BucketNum].ttPagePhysAdd,
                  NV_TRUE);
                if (sts != NvSuccess)
                {
                    RETURN(NULL);
                }
            }
        }

        sts = GetPage(
            hNand,
            LogicalPage,
            hNand->pNandTt->ttCache.bucket[BucketNum].ttPagePhysAdd);
        if (sts != NvSuccess)
        {
            RETURN(NULL);
        }

        // update last accessed time
        hNand->pNandTt->ttCache.bucket[BucketNum].LastAccessTick =
            NvOsGetTimeUS();
        hNand->pNandTt->ttCache.bucket[BucketNum].LogicalPageNumber =
            LogicalPage;
    }

    if (pBucket) *pBucket = BucketNum;
    return hNand->pNandTt->ttCache.bucket[BucketNum].ttPage;
}

/**
 * @brief This method maps the lba to physical entry number
 * in the translation table.
 *
 * @param lba gives the lba for mapping and returns the physical
 * entry number in the TT.
 */
void MapLBA(NvNandHandle hNand, NvS32 *pLba)
{
    *pLba += hNand->FtlStartLba;
}

/**
 * @brief This method mark the specified logical page as the dirty indicating
 *  that it is modified.
 *
 * @param LogicalPageNumber logical TT page number.
 */
void MarkPageDirty(NvNandHandle hNand, NvS32 LogicalPageNumber)
{
    NvS32 bucketNum;

    if (IsCached(&hNand->pNandTt->ttCache, LogicalPageNumber, &bucketNum))
    {
        if (hNand->pNandTt->ttCache.bucket[bucketNum].IsDirty)
            return;
        PageModified(hNand, LogicalPageNumber);
        hNand->pNandTt->ttCache.bucket[bucketNum].IsDirty = NV_TRUE;

    }
    else
    {
        // We can not mark a page as dirty if it is not in cache.
        // something went wrong, go fix it.
        RETURN_VOID;
    }
}

// Utility to check for number being power of 2
static NvBool IsPowerOf2(NvU32 Num)
{
    if (Num & (Num - 1))
        return NV_FALSE;
    else
        return NV_TRUE;
}

// Local function to print some of NvNand Handle information 
static void DumpNandHandlerInfo(NvNandHandle hNand)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    NvS32 i;
    NvS32 j;
    char StrTemp[80];
    char Str[10];

    // Print some of the members of NvNandHandle
    NvOsDebugPrintf("\n NvNandHandle: FtlStartLba=%d, FtlEndLba=%d "
         "FtlStartPba=%d, FtlEndPba=%d ", hNand->FtlStartLba, hNand->FtlEndLba
        , hNand->FtlStartPba, hNand->FtlEndPba);

    // Track Lba details if entry is in use
    for (i = 0; i < hNand->NandStrat.NoOfLbas2Track; i++)
    {
        if (hNand->NandStrat.pTrackLba[i].inUse)
        {
            NvOsSnprintf(StrTemp, 80, " pBlocks[");
            for (j = 0; j < 
                hNand->NandStrat.pTrackLba[i].NoOfInterleavedBanks; j++)
            {
                NvOsSnprintf(Str, 10, "%d ", 
                    hNand->NandStrat.pTrackLba[i].pBlocks[j]);
                strcat(StrTemp, Str);
            }
            strcat(StrTemp, "]   prevBlocks[");
            for (j = 0; j < 
                hNand->NandStrat.pTrackLba[i].NoOfInterleavedBanks; j++)
            {
                NvOsSnprintf(Str, 10, "%d ",
                    hNand->NandStrat.pTrackLba[i].pPrevBlocks[j]);
                strcat(StrTemp, Str);
            }
            strcat(StrTemp, "] ");
            NvOsDebugPrintf("\nTrackLba[%d]: lba=%d, %s ", i, 
                hNand->NandStrat.pTrackLba[i].lba, StrTemp);
        }
    }

    NvOsDebugPrintf("\r\n Misc start");

    NvOsDebugPrintf("\r\n NumOfBanksOnBoard = %d", 
        nandILTat->Misc.NumOfBanksOnBoard);
    NvOsDebugPrintf("\r\n NoOfILBanks = %d", 
        nandILTat->Misc.NoOfILBanks);
    NvOsDebugPrintf("\r\n PhysBlksPerBank = %d",
        nandILTat->Misc.PhysBlksPerBank);
    NvOsDebugPrintf("\r\n ZonesPerBank = %d",
        nandILTat->Misc.ZonesPerBank);
    NvOsDebugPrintf("\r\n PhysBlksPerZone = %d", 
        nandILTat->Misc.PhysBlksPerZone);
    NvOsDebugPrintf("\r\n PhysBlksPerLogicalBlock = %d",
        nandILTat->Misc.PhysBlksPerLogicalBlock);
    NvOsDebugPrintf("\r\n TotalLogicalBlocks = %d",
        nandILTat->Misc.TotalLogicalBlocks);
    NvOsDebugPrintf("\r\n TotEraseBlks = %d", 
        nandILTat->Misc.TotEraseBlks);
    NvOsDebugPrintf("\r\n NumOfBlksForTT = %d",
        nandILTat->Misc.NumOfBlksForTT);
    NvOsDebugPrintf("\r\n PgsRegForTT = %d", 
        nandILTat->Misc.PgsRegForTT);
    NvOsDebugPrintf("\r\n TtPagesRequiredPerZone = %d", 
        nandILTat->Misc.TtPagesRequiredPerZone);
    NvOsDebugPrintf("\r\n NumOfBlksForTAT = %d", 
        nandILTat->Misc.NumOfBlksForTAT);
    NvOsDebugPrintf("\r\n BlksRequiredForTT = %d", 
        nandILTat->Misc.BlksRequiredForTT);
    NvOsDebugPrintf("\r\n PgsAlloctdForTT = %d", 
        nandILTat->Misc.PgsAlloctdForTT);
    NvOsDebugPrintf("\r\n ExtraPagesForTTMgmt = %d",
        nandILTat->Misc.ExtraPagesForTTMgmt);
    NvOsDebugPrintf("\r\n LastTTPageUsed = %d", 
        nandILTat->Misc.LastTTPageUsed);
    NvOsDebugPrintf("\r\n CurrentTatLBInUse = %d",
        nandILTat->Misc.CurrentTatLBInUse);
    NvOsDebugPrintf("\r\n bsc4PgsPerBlk = %d", 
        nandILTat->Misc.bsc4PgsPerBlk);

    NvOsDebugPrintf("\r\n Misc end");

    NvOsDebugPrintf("\r\n TAT Handler start");

    for (i = 0; i < nandILTat->Misc.NumOfBlksForTAT; i++)
    {

        NvOsDebugPrintf("\r\n tatBlocks[%d] bank = %d, block = %d",
            i, 
            nandILTat->tatHandler.tatBlocks[i].BnkNum,
            nandILTat->tatHandler.tatBlocks[i].BlkNum);
    }

    for (i = 0; i < nandILTat->Misc.NumOfBlksForTT; i++)
    {

        NvOsDebugPrintf("\r\n ttBlocks[%d] bank = %d, block = %d", 
            i, 
            nandILTat->tatHandler.ttBlocks[i].BnkNum,
            nandILTat->tatHandler.ttBlocks[i].BlkNum);
    }

    NvOsDebugPrintf("\r\n tat Block bank = %d, block = %d", 
        nandILTat->tatHandler.WorkingCopy.TatBlkNum.BnkNum,
        nandILTat->tatHandler.WorkingCopy.TatBlkNum.BlkNum);
    for (i = 0; i < nandILTat->tatHandler.WorkingCopy.TtBlkCnt; i++)
    {

        NvOsDebugPrintf("\r\n TtAllocBlk[%d] bank = %d, block = %d",
            i, nandILTat->tatHandler.WorkingCopy.TtAllocBlk[i].BnkNum,
            nandILTat->tatHandler.WorkingCopy.TtAllocBlk[i].BlkNum);
    }

    NvOsDebugPrintf("\r\n lastUsedTTBlock bank = %d, block = %d", 
        nandILTat->tatHandler.WorkingCopy.lastUsedTTBlock->BnkNum,
        nandILTat->tatHandler.WorkingCopy.lastUsedTTBlock->BlkNum);

    NvOsDebugPrintf("\r\n TAT Handler end");
}


/**
 * @brief This method dumps the translation table info on to the console.
 *
 * @param page translation table page number.
 */

void DumpTT(NvNandHandle hNand, NvS32 page)
{
    NvS8 *pTtPage = NULL;
    NvS32 StartPage = page;
    NvS32 EndPage = page + 1;
    NvS32 ttPageNum;
    NvS32 sBlock;
    BlockStatusEntry* blockSts;
    NvU32 bank;
    NvS32 prevSuperBlock[MAX_NAND_SUPPORTED];
    NvS32 currSuperBlock[MAX_NAND_SUPPORTED];
    NvU32 prevSpbExits = 0;
    NvBool spBlocksSame = NV_FALSE;
    NvS32 superBlkCnt = -1;
    NvU32 FreeBlocks = 0;
    NvU32 BadBlocks = 0;
    NvU32 RegionCount[4] = { 0, 0, 0, 0};
    NvU32 DataReserveBlocks = 0;
    NvU32 SysReserveBlocks = 0;
    NvU32 TatReserveBlocks = 0;
    NvU32 TtReserveBlocks = 0;
    NvU32 IllegalReserveBlocks = 0;
    NvU32 TotalBlocks = 0;
    BlockStatusEntry* ptrBlkStatusEntry;
     #define CACHE_SP_BLOCK(SuperBlock,ILBanks) { \
            for (bank = 0;bank < ILBanks;bank++) \
            { \
                SuperBlock[bank] = \
                    *((NvS32*)&blockSts[(sBlock*ILBanks) + bank]); \
                ptrBlkStatusEntry = (BlockStatusEntry *)(&SuperBlock[bank]); \
                TotalBlocks++; \
                RegionCount[ptrBlkStatusEntry->Region]++; \
                /* Ignore the flags for system reserved blocks */ \
                if (ptrBlkStatusEntry->SystemReserved) \
                { \
                    /* count system reserved blocks */ \
                    SysReserveBlocks++; \
                } \
                else if (ptrBlkStatusEntry->DataReserved) \
                { \
                    /* count data reserved blocks */ \
                    DataReserveBlocks++; \
                    /* We can have bad blocks in data reserved */ \
                    if (!ptrBlkStatusEntry->BlockGood) \
                    { \
                        /* count bad blocks */ \
                        BadBlocks++; \
                    } \
                } \
                else if (ptrBlkStatusEntry->TatReserved) \
                { \
                    /* count Tat reserved blocks */ \
                    TatReserveBlocks++; \
                } \
                else if (ptrBlkStatusEntry->TtReserved) \
                { \
                    /* count Tt reserved blocks */ \
                    TtReserveBlocks++; \
                } \
                else if ((!IsPowerOf2((SuperBlock[bank] & 0x0F000000)))) \
                { \
                    /* we have multiple reserved bits set */ \
                    IllegalReserveBlocks++; \
                } \
                else \
                { \
                    /* Count the Free blocks, Bad blocks, 
                    Data system Tat and Tt rsrv */ \
                    if (!ptrBlkStatusEntry->BlockGood) \
                    { \
                        /* count bad blocks */ \
                        BadBlocks++; \
                    } \
                    else if ((ptrBlkStatusEntry->BlockGood) && \
                        (ptrBlkStatusEntry->BlockNotUsed)) \
                    { \
                        /* count free block */ \
                        FreeBlocks++; \
                    } \
                } \
            } \
        }
     #define COPY_SP_BLOCK(destSuperBlock,srcSuperBlock) { \
            for (bank = 0;bank < hNand->ITat.Misc.NoOfILBanks;bank++) \
            { \
                destSuperBlock[bank] = srcSuperBlock[bank]; \
            } \
        } 
     #define COMPARE_SP_BLOCK(destSuperBlock,srcSuperBlock) { \
            for (bank = 0;bank < hNand->ITat.Misc.NoOfILBanks;bank++) \
            { \
                spBlocksSame = NV_TRUE; \
                if(destSuperBlock[bank] != srcSuperBlock[bank]) \
                    spBlocksSame = NV_FALSE; \
            } \
        } 
     #define FTL_GOOD_BLK_MASK 0x10000000
     #define FTL_FREE_BLK_MASK 0x20000000
     #define FTL_SYS_RSVD_MASK 0x04000000
     #define FTL_REGION_MASK 0xC0000000
     #define FTL_REGION_LSB 30
     #define PRINT_SP_BLOCK(SuperBlock,SuperBlockNum) { \
            NvOsDebugPrintf(" **SuperBlock %d \r\n",SuperBlockNum); \
            for (bank = 0;bank < hNand->ITat.Misc.NoOfILBanks;bank++) \
            { \
                NvS32 pba = SuperBlock[bank] & 0x00FFFFFF; \
                if (SuperBlock[bank] & FTL_SYS_RSVD_MASK) \
                { \
                    NvOsDebugPrintf(" *0x%08X [%d] [SYS-RSVD] \r\n", SuperBlock[bank], \
                        ((pba == 0x00FFFFFF) ? -1 : pba)); \
                } \
                else \
                { \
                    if (SuperBlock[bank] & FTL_GOOD_BLK_MASK) \
                    { \
                        if (SuperBlock[bank] & FTL_FREE_BLK_MASK) \
                        { \
                            NvOsDebugPrintf(" *0x%08X [%d] [ ^^^ FREE BLK ] Region%d\r\n",\
                                SuperBlock[bank], ((pba == 0x00FFFFFF) ? -1 : pba), \
                                ((SuperBlock[bank] & FTL_REGION_MASK) >> FTL_REGION_LSB)); \
                        } \
                        else \
                        { \
                            NvOsDebugPrintf(" *0x%08X [%d] [ USED BLK ] Region%d\r\n",\
                                SuperBlock[bank], ((pba == 0x00FFFFFF) ? -1 : pba), \
                                ((SuperBlock[bank] & FTL_REGION_MASK) >> FTL_REGION_LSB)); \
                        } \
                    } \
                    else \
                    { \
                        NvOsDebugPrintf(" *0x%08X [%d] [*** BAD BLK ***]\r\n", \
                            SuperBlock[bank], ((pba == 0x00FFFFFF) ? -1 : pba)); \
                    } \
                } \
            } \
            NvOsDebugPrintf("\r\n"); \
        } 

    DumpNandHandlerInfo(hNand);
    // Print the format of TT entry
    NvOsDebugPrintf("\r\n++++++++++++++++++\r\n"
        "TT 32-bit entry format in dump : \r\n"
        "=============\r\n"
        "Region: b31-b30\r\n"
        "BlockNotUsed: b29\r\n"
        "BlockGood: b28\r\n"
        "DataReserved: b27\r\n"
        "SystemReserved: b26\r\n"
        "TatReserved: b25\r\n"
        "TtReserved: b24\r\n"
        "PhysBlkNum: b23-b0\r\n"
        "============\r\n");

    if (page == -1)
    {
        StartPage = 0;
        EndPage = StartPage + hNand->ITat.Misc.PgsRegForTT;
    }

    for (ttPageNum = StartPage; ttPageNum < EndPage; ttPageNum++)
    {
        NvOsDebugPrintf("\r\n Dumping page %d\r\n", ttPageNum);
        // cache the page and get the cached data
        pTtPage = CachePage(hNand, ttPageNum, 0);
        if (!pTtPage)
            return;

        blockSts = (BlockStatusEntry*) pTtPage;

         for (sBlock = 0; sBlock < hNand->pNandTt->ttEntriesPerCachePage; sBlock++)
            {
                ++superBlkCnt;
                if(!prevSpbExits)
                {
                    CACHE_SP_BLOCK(prevSuperBlock,hNand->ITat.Misc.NoOfILBanks);
                    PRINT_SP_BLOCK(prevSuperBlock,superBlkCnt);
                    prevSpbExits = 1;
                }
                else
                {
                    CACHE_SP_BLOCK(currSuperBlock,hNand->ITat.Misc.NoOfILBanks);
                    COMPARE_SP_BLOCK(prevSuperBlock,currSuperBlock);
                    if(spBlocksSame == NV_FALSE)
                    {
                        COPY_SP_BLOCK(prevSuperBlock,currSuperBlock);
                        PRINT_SP_BLOCK(currSuperBlock,superBlkCnt);
                    }
                }
            }
    }
    COMPARE_SP_BLOCK(prevSuperBlock,currSuperBlock);
    if(spBlocksSame == NV_TRUE)
    {
        PRINT_SP_BLOCK(currSuperBlock,superBlkCnt);
    }
    // Print the count for bad, free and different types of reserved blocks
    NvOsDebugPrintf("\r\nTotal=%u,Free=%u,Bad=%u,Reserve Data=%u,"
        "System=%u,Tat=%u,Tt=%u,Illegal=%u,Region0=%u,"
        "Region1=%u,Region2=%u,Region3=%u \r\n", 
        TotalBlocks, FreeBlocks, BadBlocks,
        DataReserveBlocks, SysReserveBlocks, 
        TatReserveBlocks, TtReserveBlocks, IllegalReserveBlocks,
        RegionCount[0], RegionCount[1], RegionCount[2], RegionCount[3]);

    PrintData(hNand);
}

/**
 * @brief This method checks whether the LBA is in valid range.
 *
 * @param LBA logical block number to check.
 */
NvBool ValidateLBA(NvNandHandle hNand, NvS32 lba)
{
    if ((lba < 0)|| (lba >= (NvS32)hNand->ITat.Misc.TotalLogicalBlocks))
    {
        return NV_FALSE;
    }
    return NV_TRUE;
}

/**
 * @brief This method checks whether the PBA is in valid range.
 *
 * @param LBA physical block number to check.
 */
NvBool ValidatePBA(NvNandHandle hNand, NvS32 pba)
{
    if ((pba < 0) || (pba >= (NvS32)(hNand->ITat.Misc.TotEraseBlks >>
                               hNand->ITat.Misc.bsc4NoOfILBanks)))
    {
        return NV_FALSE;
    }
    return NV_TRUE;
}

/**
 * @brief This method returns the lasd PBA Used.
 *
 * @return returns the last pba used.
 */
NvS32 GetLastPbaUsed(NvNandHandle hNand)
{
    NvS32 LastPba = hNand->pNandTt->lastUsedPba[0];
    NvU32 bank;

    for (bank = 1; bank < hNand->ITat.Misc.NoOfILBanks; bank++)
    {
        if (hNand->pNandTt->lastUsedPba[bank] < LastPba)
        {
            LastPba = hNand->pNandTt->lastUsedPba[bank];
        }
    }
    LOG_NAND_DEBUG_INFO(("\r\n hNand->pNandTt->lastUsedPba %d", LastPba),
        TT_DEBUG_INFO);
    return LastPba;
}

/**
 * @brief This method is used to force the all TT page flush.
 *      This will be used by the NandInterleaveTAT class to let this class
 *      know that all the TT pages should be flush once as it moving the
 *      TT pages to new TT block set.
 *
 */
void ForceTTFlush(NvNandHandle hNand)
{
    hNand->pNandTt->bFlushTTable = NV_TRUE;
}

/**
 * @brief This method reads the tag info.
 *
 * @param pPages pointer to page array holding the page number to
 *  read tag info from.
 * @param sectorOffset sector offset which tells the bank to read the tag
 *  info from.
 * @param pSpareBuffer - spare buffer pointer
 *
 * @return returns the status of operation.
 */
NvError
ReadTagInfo(
           NvNandHandle hNand,
           NvS32* pPages,
           NvS32 SectorOffset,
           NvU8 *pSpareBuffer)
{
    return NandTLReadPhysicalPage(hNand, pPages, SectorOffset, NULL, 0,
                                  pSpareBuffer, NV_FALSE);
}

/**
 * @brief This method write the tag info.
 *
 * @param pPages pointer to page array holding the page number to write tag info to.
 * @param sectorOffset sector offset which tells the bank to write the tag info to.
 * @param pSpareBuffer - spare buffer pointer
 *
 * @return returns the status of operation.
 */
NvError
WriteTagInfo(
            NvNandHandle hNand,
            NvS32* pPages,
            NvS32 SectorOffset,
            NvU8 *pSpareBuffer)
{
    return NandTLWritePhysicalPage(hNand, pPages, SectorOffset, NULL, 0,
                                  pSpareBuffer);
}
#if 0
/**
 * @brief This method is for the debug purpose. it traverses through the
 *          whole Translation table and displays info on the comms channel viewer.
 *
 */
void ScanDevice(NvNandHandle hNand)
{
    NvS32 BucketNum;
    NvS32 i;
    NvS32 ttPage;
    NvS32 ttEntry;
    NvS32 bank;
    NvS32 entry;
    NvS32 *pSysRsvdBlockCount = (NvS32 *)AllocateVirtualMemory(sizeof(NvS32) *
        hNand->pNandTt->NoOfILBanks, TT_ALLOC);
    NvS32 *pBadBlockCount = (NvS32 *)AllocateVirtualMemory(sizeof(NvS32) *
        hNand->pNandTt->NoOfILBanks, TT_ALLOC);
    NvS32 *pFreeBlockCount = (NvS32 *)AllocateVirtualMemory(sizeof(NvS32) *
        hNand->pNandTt->NoOfILBanks, TT_ALLOC);
    NvS32 *pUsedBlockCount = (NvS32 *)AllocateVirtualMemory(sizeof(NvS32) *
        hNand->pNandTt->NoOfILBanks, TT_ALLOC);
    NvS32 *pDataRsvdBlockCount = (NvS32 *)AllocateVirtualMemory(sizeof(NvS32) *
        hNand->pNandTt->NoOfILBanks, TT_ALLOC);
    BlockStatusEntry *blockSts;

    for (i = 0; i < hNand->pNandTt->NoOfILBanks; i++)
    {
        pSysRsvdBlockCount[i] = 0;
        pBadBlockCount[i] = 0;
        pFreeBlockCount[i] = 0;
        pUsedBlockCount[i] = 0;
        pDataRsvdBlockCount[i] = 0;
    }

    LOG_NAND_DEBUG_INFO(("\r\nScan device start\r\n"), ERROR_DEBUG_INFO);
    for (ttPage = 0; ttPage < hNand->ITat.Misc.PgsRegForTT; ttPage++)
    {
        if (!CachePage(hNand, ttPage, &BucketNum))
        {
            NvOsFree(pSysRsvdBlockCount);
            NvOsFree(pBadBlockCount);
            NvOsFree(pFreeBlockCount);
            NvOsFree(pUsedBlockCount);
            NvOsFree(pDataRsvdBlockCount);
            return;
        }

        blockSts =
            (BlockStatusEntry*) hNand->pNandTt->ttCache.bucket[BucketNum].ttPage;
        for (
            ttEntry = 0;
            ttEntry < hNand->pNandTt->ttEntriesPerCachePage;
            ttEntry++)
        {
            for (bank = 0; bank < hNand->pNandTt->NoOfILBanks; bank++)
            {
                entry = GetTTableBlockStatusEntryNumber(hNand, bank, ttEntry);
                // If the pba is a system reserved entry; skip it
                if (blockSts[entry].SystemReserved)
                {
                    pSysRsvdBlockCount[bank]++;
                    // LOG_NAND_DEBUG_INFO(("%3d. %s, bank = %d \r\n",
                    // (ttPage << hNand->pNandTt->bsc4ttEntriesPerCachePage) + ttEntry, "SYS", bank), TT_DEBUG_INFO);
                    continue;
                }
                // If the block is bad; skip it
                if (!blockSts[entry].BlockGood)
                {
                    pBadBlockCount[bank]++;
                    // LOG_NAND_DEBUG_INFO(("%3d. %s, bank = %d \r\n",
                    // (ttPage << hNand->pNandTt->bsc4ttEntriesPerCachePage) + ttEntry, "BAD", bank), TT_DEBUG_INFO);
                    continue;
                }
                // if we are requested to ignore reserved block; do it
                if (blockSts[entry].DataReserved && (blockSts[entry].BlockNotUsed))
                {
                    pDataRsvdBlockCount[bank]++;
                    // LOG_NAND_DEBUG_INFO(("%3d. %s, bank = %d \r\n",
                    // (ttPage << hNand->pNandTt->bsc4ttEntriesPerCachePage) + ttEntry, "DAT", bank), TT_DEBUG_INFO);
                    continue;
                }
                // check wether the pba is used or not
                if (blockSts[entry].BlockNotUsed)
                {
                    pFreeBlockCount[bank]++;
                    // LOG_NAND_DEBUG_INFO(("%3d. %s, bank = %d \r\n",
                    // (ttPage << hNand->pNandTt->bsc4ttEntriesPerCachePage) + ttEntry, "FREE", bank), TT_DEBUG_INFO);
                    continue;
                }
                pUsedBlockCount[bank]++;
                // LOG_NAND_DEBUG_INFO(("%3d. %s %d, bank = %d\r\n",
                // (ttPage << hNand->pNandTt->bsc4ttEntriesPerCachePage) + ttEntry, "USED",
                // blockSts[entry].PhysBlkNum, bank), TT_DEBUG_INFO);
            }
        }
    }

    for (i = 0; i < hNand->pNandTt->NoOfILBanks; i++)
    {
        LOG_NAND_DEBUG_INFO(("\r\nNo System blocks in bank %d = %d", i,
                             pSysRsvdBlockCount[i]), ERROR_DEBUG_INFO);
        LOG_NAND_DEBUG_INFO(("\r\nNo Bad blocks in bank %d = %d", i,
            pBadBlockCount[i]),
                            ERROR_DEBUG_INFO);
        LOG_NAND_DEBUG_INFO(("\r\nNo Free blocks in bank %d = %d", i,
            pFreeBlockCount[i]),
                            ERROR_DEBUG_INFO);
        LOG_NAND_DEBUG_INFO(("\r\nNo Data rsvd blocks in bank %d = %d", i,
                             pDataRsvdBlockCount[i]), ERROR_DEBUG_INFO);
        LOG_NAND_DEBUG_INFO(("\r\nNo Used blocks in bank %d = %d", i,
            pUsedBlockCount[i]),
                            ERROR_DEBUG_INFO);
        LOG_NAND_DEBUG_INFO(("\r\n"), ERROR_DEBUG_INFO);
    }

    LOG_NAND_DEBUG_INFO(("\r\nScan device end"), ERROR_DEBUG_INFO);

    NvOsFree(pSysRsvdBlockCount);
    NvOsFree(pBadBlockCount);
    NvOsFree(pFreeBlockCount);
    NvOsFree(pUsedBlockCount);
    NvOsFree(pDataRsvdBlockCount);
}
#endif
/**
 * @brief This method is for recovering the lost blocks becuase of power failure.
 *          it traverses through the whole Translation table and recovers the
 *          blocks that are indicated as used and not assigned to any LBA's.
 *
 * @return returns the status of operation.
 */
/**
 * The Power failure can happen in the following cases.
 * case 1: We got the new PBA's for an LBA. At the time of acquiring,
 *          we mark the new PBA's as used. now for some reason(either rollover
 *          during the allocation of block or TT block rollover.) the
 *          TT and TAT got flushed. Now the power is removed for the Device.
 *          on next boot up, we see the new PBA's as used PBA's,
 *          which are not really used. LBA is still pointing to old PBA's only.
 *          So, we need to look for orphan PBA's and make sure that
 *          they are in unused state. otherwise they will never be used.
 * case 2: We got the new PBA's for an LBA and assagined them to the LBA.
 *          during the assaignment, the TT and TAT got flushed.
 *          before releasing the old PBA's, the power to the device is removed.
 *          In this case the old PBA's will be used state even they are not.
 *          So, we need to look for orphan PBA's and make sure that
 *          they are in unused state. otherwise they will never be used.
 * case 3: We got the new PBA's for an LBA. At the time of acquiring,
 *          we might have marked the old blocks as data reserved.
 *          Before the new PBA's are assaigned to LBA, the TT and TAT flush
 *          happened and power to the device is removed. after power on,
 *          we need to ensure that any of the used PBA's are not
 *          marked as data rsvd.
 */
NvError RecoverLostBlocks(NvNandHandle hNand)
{
    // NandInterleaveTTable::Status status = FAILURE;
    NvBool* pAllBlocksStatus = (NvBool *)AllocateVirtualMemory(sizeof(NvBool) *
        hNand->ITat.Misc.TotEraseBlks, TT_ALLOC);
    NvS32 NoOfBlocksPerColumn;
    NvS32 lba;
    NvS32 Page2bCached;
    NvU32 tempLba;
    NvS32 ttEntry;
    NvS32 bank;
    NvS32 entry;
    NvS32 ActualPba;
    NvS32 PhysicalEntry;
    NvS32 PhysicalBlock;
    NvS32 ArrayEntry;
    NvS8 *pTtPage;
    BlockStatusEntry *pBlockSts;
    NvU32 i;

    LOG_NAND_DEBUG_INFO(("\r\nTrying to recover any lost blocks "),
        ERROR_DEBUG_INFO);

    if (!pAllBlocksStatus)
        goto errorHandler;
    for (i = 0; i < hNand->ITat.Misc.TotEraseBlks; i++)
    {
        pAllBlocksStatus[i] = NV_FALSE;
    }

    for (tempLba = 0; tempLba < hNand->ITat.Misc.TotalLogicalBlocks; tempLba++)
    {
        lba = tempLba;
        // get the entry number in the translation table from the
        // logical block address(lba).
        MapLBA(hNand, &lba);

        pTtPage = NULL;
        // get the page number of the TT table in which we have the current lba entry
        Page2bCached = lba >> hNand->pNandTt->bsc4ttEntriesPerCachePage;

        // cache the page and get the cached data
        pTtPage = CachePage(hNand, Page2bCached, 0);
        if (!pTtPage)
            goto errorHandler;

        pBlockSts = (BlockStatusEntry*)pTtPage;
        // get the offset of the logical block entry from the cached page
        ttEntry = lba % hNand->pNandTt->ttEntriesPerCachePage;

        for (bank = 0; bank < hNand->pNandTt->NoOfILBanks; bank++)
        {
            // get the entry number w.r.t bank
            entry = GetTTableBlockStatusEntryNumber(hNand, bank, ttEntry);
            ActualPba = (pBlockSts[entry].PhysBlkNum & 0x00FFFFFF);

            if (ActualPba != 0x00FFFFFF)
            {
                // get the pba number of lba and mark it as used.
                PhysicalEntry =
                    GetTTableBlockStatusEntryNumber(hNand, bank, ActualPba);
                pAllBlocksStatus[PhysicalEntry] = NV_TRUE;
            }
            else
            {
                // LOG_NAND_DEBUG_INFO(("\r\n lba %d has no pba's", tempLba), ERROR_DEBUG_INFO);
            }
        }
    }

    NoOfBlocksPerColumn = (hNand->ITat.Misc.TotEraseBlks /
        hNand->ITat.Misc.NoOfILBanks);
    // Now release the blocks, which are left in the used state becuase
    // of power failure but they are not really used blocks.
    for (PhysicalBlock = 0; PhysicalBlock < NoOfBlocksPerColumn; PhysicalBlock++)
    {
        pTtPage = NULL;
        // get the page number of the TT table in which
        // we have the current block entry
        Page2bCached = PhysicalBlock >> hNand->pNandTt->bsc4ttEntriesPerCachePage;

        // cache the page and get the cached data
        pTtPage = CachePage(hNand, Page2bCached, 0);
        if (!pTtPage)
        {
            NvOsFree(pAllBlocksStatus);
            RETURN(NvError_NandTTFailed);
        }

        pBlockSts = (BlockStatusEntry*)pTtPage;
        // get the offset of the physical block entry from the cached page
        ttEntry = PhysicalBlock % hNand->pNandTt->ttEntriesPerCachePage;

        for (bank = 0; bank < hNand->pNandTt->NoOfILBanks; bank++)
        {
            // get the entry number w.r.t bank
            entry = GetTTableBlockStatusEntryNumber(hNand, bank, ttEntry);
            ArrayEntry = GetTTableBlockStatusEntryNumber(hNand, bank, PhysicalBlock);

            // if the block is system reserved, skip it.
            if (pBlockSts[entry].SystemReserved || pBlockSts[entry].TatReserved ||
                pBlockSts[entry].TtReserved)
            {
                continue;
            }

            if (!pAllBlocksStatus[ArrayEntry])
            {
                if (pBlockSts[entry].BlockNotUsed)
                {
                    /*
                    // Nothing to do here
                    if (blockSts[entry].DataReserved)
                    {
                        LOG_NAND_DEBUG_INFO(("\r\n found a free block = %d, bank = %d, %s",
                                                 physBlk, bank, "DAT"), ERROR_DEBUG_INFO);
                    }
                    else
                    {
                        LOG_NAND_DEBUG_INFO(("\r\n found a free block = %d, bank = %d, %s",
                                                 physBlk, bank, "FREE"), ERROR_DEBUG_INFO);
                    }
                    */
                }
                else
                {
                    // No LBA is pointing to this PBA. But it is marked as used.
                    // So, release it.
                    if ((!pBlockSts[entry].SystemReserved) && pBlockSts[entry].BlockGood)
                    {
                        LOG_NAND_DEBUG_INFO(("\r\n *****found a lost block = %d, bank = %d and it is released for usage",
                                             PhysicalBlock, bank), ERROR_DEBUG_INFO);
                        pBlockSts[entry].BlockNotUsed = 1;
                        // We are modofying the data in the page
                        // so make it as dirty
                        MarkPageDirty(hNand, Page2bCached);
                    }
                }
            }
            else
            {
                if (pBlockSts[entry].DataReserved)
                {
                    LOG_NAND_DEBUG_INFO(("\r\n *****found a used block = %d, bank = %d, \
                        which is marked as rsvd. it is marked as non rsvd.",
                        PhysicalBlock, bank), ERROR_DEBUG_INFO);
                    // it can't be a data reserved block when an LBA is
                    // pointing to it. change it. It must be becaue
                    // of power failure.
                    pBlockSts[entry].DataReserved = 0;
                    // We are modofying the data in the page so make it as dirty
                    MarkPageDirty(hNand, Page2bCached);
                }

                if (pBlockSts[entry].BlockNotUsed)
                {
                    LOG_NAND_DEBUG_INFO(("\r\n *****This block = %d, bank = %d \
                        is used by some lba, but it is marked as unused in TT.",
                        PhysicalBlock, bank), ERROR_DEBUG_INFO);
                }
            }
        }
    }
errorHandler:
    LOG_NAND_DEBUG_INFO(("\r\nRecovering lost blocks is done"), ERROR_DEBUG_INFO);
    NvOsFree(pAllBlocksStatus);
    return NvSuccess;
}

void InitBucket(Bucket *pBucket)
{
    pBucket->IsDirty = NV_FALSE;
    pBucket->LastAccessTick = 0ll;
    pBucket->LogicalPageNumber = -1;
    pBucket->ttPage = 0;
}

void InitBlockInfo(BlockInfo *pBlockInfo)
{
    pBlockInfo->BnkNum = -1;
    pBlockInfo->BlkNum = -1;
}

void SetBlockInfo(BlockInfo *pBlockInfo, NvS32 BankNo, NvS32 BlockNo)
{
    pBlockInfo->BnkNum = BankNo;
    pBlockInfo->BlkNum = BlockNo;
}

void GetBlockInfo(BlockInfo *pBlockInfo, NvS32 *pBankNo, NvS32 *pBlockNo)
{
    *pBankNo = pBlockInfo->BnkNum;
    *pBlockNo = pBlockInfo->BlkNum;
}

