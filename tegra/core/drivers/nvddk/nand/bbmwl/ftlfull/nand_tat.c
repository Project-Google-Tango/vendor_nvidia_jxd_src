/**
 * @file
 * @brief Implements the translation allocation table
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

#define NEW_TT_CODE 1
// Enable this to list all bad blocks
//#define LISTBADBLOCKS

const NvS8 NAND_IL_TAT_HEADER[] = {"_PPI_IL_TAT_START_"};
const NvS8 NAND_IL_TAT_FOOTER[] = {"_PPI_IL_TAT_END_"};

typedef enum NandInterleaveTatEnum
{
    NIT_HEADER_LENGTH = sizeof(NAND_IL_TAT_HEADER),
    NIT_FOOTER_LENGTH = sizeof(NAND_IL_TAT_FOOTER),
    NIT_MASK_DIRTY_BIT = 0x8000
}NandInterleaveTatEnum;

#ifndef TT_FIRST_BOOT_OPTIMIZATION
// When 1, adds code for shortcut create TT
#define TT_FIRST_BOOT_OPTIMIZATION 1
#if !TT_FIRST_BOOT_OPTIMIZATION
// Macro to use buffer updated with optimization
#ifndef USE_MIRROR
#define USE_MIRROR 1
#endif // USE_MIRROR
#endif // !TT_FIRST_BOOT_OPTIMIZATION
#endif // TT_FIRST_BOOT_OPTIMIZATION

#ifndef DEBUG_TT_OPTIMIZATION
// when TT_FIRST_BOOT_OPTIMIZATION 1, disable debug TT optimization
#define DEBUG_TT_OPTIMIZATION !TT_FIRST_BOOT_OPTIMIZATION
#if DEBUG_TT_OPTIMIZATION
// whenever mismatch in TT page data seen between enable optimization and
// the TT page data with no optimization we will get assert if
// ENABLE_TT_OPT_ASSERT is 1
#define ENABLE_TT_OPT_ASSERT 0
#endif
#endif

#ifndef DEBUG_TT_CREATE
// Enable this macro to get debug prints during TT create
#define DEBUG_TT_CREATE 0
#endif

// Constant to indicate maximum interleave banks possible
#define MAX_INTERLEAVE_COUNT 4

/**
 * In the Interleave mode, the zone organization is as shown below.
 * Here the bank 0 and bank 1 are used for interleaving and similarly bank2
 * and bank 3 are used for interleaving. One logical block is equivalent to
 * two physical blocks, one from bank 0 and other from bank 1.
 * even numbered pages will be present in bank 0 and odd numbered pages will be
 * present in bank 1.
 *
 *    Bank 0            Bank 1
    ____________      ____________
    |          |      |          |
    |  Zone 0  |      | Zone 0   |
    |          |      |          |
    |          |      |          |
------|----------|------|----------|--------
    |          |      |          |
    |  Zone 1  |      | Zone 1   |
    |          |      |          |
    |          |      |          |
    |__________|      |__________|

 *    Bank 2            Bank 3
    ____________      ____________
    |          |      |          |
    |  Zone 2  |      | Zone 2   |
    |          |      |          |
    |          |      |          |
------|----------|------|----------|--------
    |          |      |          |
    |  Zone 3  |      | Zone 3   |
    |          |      |          |
    |          |      |          |
    |__________|      |__________|

* We can use a double array to store the bad blocks present in a zone of
* specific bank badBlkPerZone[zonenumber][BnkNum offset]
* or use a single array and access the bad block count specific to a bank
* as follows.
* BadBlkPerZone[zonenumber * InterleaveBankCount + bank number offset]
* when a block in zone 0 of Bank 0 goes bad, we replace it with a
* block from the zone 0 of bank 0 only. so we need to maintain the bad block
* count separately for the banks depending on the zones.
* Here We are achieving this with the help of double array.
*/

#define NEW_CODE 1
#define TEST_CASE 0
#define LOG_NAND_DEBUG_INFO1(X,d) { \
        NvOsDebugPrintf X; \
}
static NvError Initialize(NvNandHandle hNand);

static void GetNextTTpage(
    NvNandHandle hNand,
    BlockInfo* nextTTBlock,
    NvS32* nextTTPage,
    NvBool getAbsolute);

static NvError WriteTATSignature(
    NvNandHandle hNand,
    NvBool writeOnlyHeader);

static void CountSystemRsvdBlks(NvNandHandle hNand);

static NvBool CheckForTAT(
    NvNandHandle hNand,
    BlockInfo* TATFoundBlock,
    NvS32* TATFoundPage,
    NvBool* falledBack);

static NvBool PrepareNandDevice(
    NvNandHandle hNand,
    NvBool enableErase);

static void GetNextTTBlockSet(NvNandHandle hNand, BlockInfo* nextTTBlockSet);

static void DoAdvancedWrite(
    NvNandHandle hNand,
    BlockInfo* blockInfo,
    NvS32 pageNumber,
    NvU8 *data);

static NvError DoAdvancedErase(
    NvNandHandle hNand,
    BlockInfo* blockInfo,
    NvBool isTAT);

static void GetNextTATBlock(
    NvNandHandle hNand,
    NvS32 currentLogicalBlock,
    NvS32* newLogicalBlock);

static NvS32 AlignBlockCount(
    NvNandHandle hNand,
    NvS32 totalBlocks);

static NvS32 GetBadPageCount(NvNandHandle hNand, BlockInfo* block);

NvS32 SetPhysicalPageArray(
    NvNandHandle hNand,
    NvS32* pPageArray,
    BlockInfo* blockInfo,
    NvS32 pageOffset);

static void FindBlockCount(
    NvNandHandle hNand,
    BlockInfo startBlock,
    BlockInfo endBlock,
    NvS32* count);

static void FillBlockInfo(
    NvNandHandle hNand,
    BlockInfo startBlock,
    BlockInfo endBlock,
    BlockInfo* blkInfo);

static void GetTATStartBlock(NvNandHandle hNand, BlockInfo* TatStBlk);

static NvBool GetBlockInfoFromTAT(NvNandHandle hNand);

static void CreateTATHandler(NvNandHandle hNand);

NvBool IsBlockBad(
    NvNandHandle hNand,
    BlockInfo* block,
    NvBool rsvdBlk);

static NvBool IsDefaultData(NvU8* pData, NvS32 length);

static void MarkPageAsBad(
    NvNandHandle hNand,
    NvS32* pPages,
    NvS32 sectorOffset,
    NvBool isTAT);

static NvError Write(
    NvNandHandle hNand,
    NvS32* pPageNumbers,
    NvU32 sectorOffset,
    NvU8 *pData,
    NvU32 sectorCount,
    NvU8* pTagBuffer);

static void DumpMiscAndTATHandlerInfoToLogger(NvNandHandle hNand);

extern NvError ReadTagInfo(
    NvNandHandle hNand,
    NvS32* pPages,
    NvS32 sectorOffset,
    NvU8* pSpareBuffer);

extern void ForceTTFlush(NvNandHandle hNand);

extern NvError WriteTagInfo(
    NvNandHandle hNand,
    NvS32* pPages,
    NvS32 sectorOffset,
    NvU8* pSpareBuffer);

extern void FormatFlash(
    NvNandHandle hNand,
    NandInterleaveTAT* pNandInterleaveTAT,
    NandDeviceInfo* pDevInfo);

NvError CreateTranslationTable(NvNandHandle hNand);

NvError WriteTTPage(
    NvNandHandle hNand,
    NvS32 ttPage,
    NvS32 *currentTTPage,
    NvS32 *currentTTBlock,
    NvS32 *ttLogicalBlkNum,
    NvS32 *ttLogicalPageNumber,
    NvS32 *badPageCount,
    BlockStatusEntry *transTab);

NvError WriteTATPage(
    NvNandHandle hNand,
    NvS32 CurTATBlk,
    NvBool IsFirstTime,
    NvS32 TatPage,
    NvBool WriteOnlyHeader);

NvError WriteTAT(NvNandHandle hNand);

void ReplaceBlockSet(NvNandHandle hNand);

#if defined(LISTBADBLOCKS)
void ListAllBadBlocks(NvNandHandle hNand);
#endif

/**
 * @brief This method Initializes the Nand interleave TAT class
 *
 */
NvError
Initialize(NvNandHandle hNand)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    NvBool IsKeyPressed;
    BlockInfo TATFoundBlock;
    NvS32 TATFoundPage = 0;
    NvBool falledBackToOldTAT;
    NvBool Status;
    NvS32 page;
    NvS32 LBNum;
    NvS32 pageOffset;
    NvS32 bank;
    // variable for log2 of pages-per-block
    NvU32 Log2PgsPerBlk;
    NvError RetStatus = NvSuccess;

#if defined(LISTBADBLOCKS)
    ListAllBadBlocks(hNand);
#endif

    IsKeyPressed = NV_FALSE;
    LOG_NAND_DEBUG_INFO(("\r\n IsKeyPressed = %d", IsKeyPressed),
        TAT_DEBUG_INFO);
    /*
     * Try to get the TAT block info from TAT.
     * if TAT is not found, that means there is no TAT at all and meaning
     * that the device is a new device not formatted.
     */
    Status = GetBlockInfoFromTAT(hNand);
    if ((!Status) || IsKeyPressed)
    {
        Status = PrepareNandDevice(hNand, IsKeyPressed);
        if (!Status)
            RETURN(NvError_NandTTFailed);
    }

    // Search for the working TAT page from the available TAT blocks

    TATFoundPage = 0;
    InitBlockInfo(&TATFoundBlock);

#if TEST_CASE
{
    NvU32 i = 0;
    NvS32 Blk = 0;
    NvS32 Bnk = 0;
    for (i = 1; i < 6; i++)
    {
        Blk = nandILTat->tatHandler.tatBlocks[i].BlkNum;
        Bnk = nandILTat->tatHandler.tatBlocks[i].BnkNum;
        MarkBlockAsBad(hNand, Bnk, Blk);
    }
    Blk = nandILTat->tatHandler.tatBlocks[7].BlkNum;
    Bnk = nandILTat->tatHandler.tatBlocks[7].BnkNum;
    MarkBlockAsBad(hNand, Bnk, Blk);
    for (i = 9; i < 16; i++)
    {
        Blk = nandILTat->tatHandler.tatBlocks[i].BlkNum;
        Bnk = nandILTat->tatHandler.tatBlocks[i].BnkNum;
        MarkBlockAsBad(hNand, Bnk, Blk);
    }
}
#endif

    // get the latest TAT page to use.
    Status = CheckForTAT(
        hNand,
        &TATFoundBlock,
        &TATFoundPage,
        &falledBackToOldTAT);

    if (Status == NV_FALSE)
    {
        /*
         * since the TAT is not found that means Translation table is
         * not there, so recreate the translation table.
         */
        LOG_NAND_DEBUG_INFO(("\r\n TAT not found; createTranslationTable"),
            TAT_DEBUG_INFO);
        hNand->IsFirstBoot = NV_TRUE;

        // Create Transition Table.
        RetStatus = CreateTranslationTable(hNand);
        CheckNvError(RetStatus, RetStatus);

        LOG_NAND_DEBUG_INFO(("\r\n Created TT..."), TAT_DEBUG_INFO);
    }
    else
    {
        // Initialize the handler working copy with TAT Block and page number
        nandILTat->tatHandler.WorkingCopy.TatBlkNum = TATFoundBlock;
        nandILTat->tatHandler.WorkingCopy.TatPgNum = TATFoundPage;
        // clear the physical block array
        ClearPhysicalPageArray(hNand,hNand->ITat.pPhysPgs);
        hNand->ITat.pPhysPgs[TATFoundBlock.BnkNum] =
            TATFoundBlock.BlkNum * hNand->NandDevInfo.PgsPerBlk +
            TATFoundPage;
        LOG_NAND_DEBUG_INFO(("\r\n TAT found; tatBank:%d,TatBlkNum:%d,\
            TatPgNum:%d",
            nandILTat->tatHandler.WorkingCopy.TatBlkNum.BnkNum,
            nandILTat->tatHandler.WorkingCopy.TatBlkNum.BnkNum,
            nandILTat->tatHandler.WorkingCopy.TatPgNum),
            TAT_DEBUG_INFO);
        NandTLReadPhysicalPage(
            hNand,
            hNand->ITat.pPhysPgs,
            TATFoundBlock.BnkNum <<
                nandILTat->Misc.bsc4SctsPerPg,
            (NvU8*)nandILTat->tatHandler.wBuffer,
            hNand->NandDevInfo.SctsPerPg, 0, 0);

        // check whether we fell back to old TT
        if (falledBackToOldTAT)
        {
            /*
             * we are here means we fell back. We don't know the state of
             * the following pages in TAT blocks and as well as in the
             * TT blocks. So, take new blocks and copy the data to them.
             */
            hNand->NandFallBackMode = NV_TRUE;
            LOG_NAND_DEBUG_INFO(("\r\n falled back to old TAT, page=%d,\
                blk = %d,bank=%d,revision=%d",
                nandILTat->tatHandler.WorkingCopy.TatPgNum,
                nandILTat->tatHandler.WorkingCopy.TatBlkNum.BlkNum,
                nandILTat->tatHandler.WorkingCopy.TatBlkNum.BnkNum,
                *nandILTat->tatHandler.WorkingCopy.RevNum),
                TAT_DEBUG_INFO);
            /*
             * change the tat page number to the end page number in a block
             * so that the tat will be written in a new block.
             * update the version number.
             */
            *nandILTat->tatHandler.WorkingCopy.RevNum =
                (*nandILTat->tatHandler.WorkingCopy.RevNum) + 1;
            nandILTat->tatHandler.WorkingCopy.TatPgNum =
                hNand->NandDevInfo.PgsPerBlk;
            WriteTATSignature(hNand, NV_FALSE);
            LOG_NAND_DEBUG_INFO(("\r\n written the TAT to new block, \
                page=%d,blk=%d,bank=%d,revision=%d",
                nandILTat->tatHandler.WorkingCopy.TatPgNum,
                nandILTat->tatHandler.WorkingCopy.TatBlkNum.BlkNum,
                nandILTat->tatHandler.WorkingCopy.TatBlkNum.BnkNum,
                *nandILTat->tatHandler.WorkingCopy.RevNum),
                TAT_DEBUG_INFO);
        }
    }

    /*
     * Scan the TAT list and check for the last page used for TT.
     * TAT should have been read by this time!!
     */
    nandILTat->Misc.LastTTPageUsed = 0;
    if (!hNand->NandFallBackMode)
    {
        for (page = 0; page < nandILTat->Misc.PgsRegForTT; page++)
        {
            if (nandILTat->tatHandler.WorkingCopy.TtAllocPg[page] >
                nandILTat->Misc.LastTTPageUsed)
                nandILTat->Misc.LastTTPageUsed =
                    nandILTat->tatHandler.WorkingCopy.TtAllocPg[page];
        }
    } else
    {

        nandILTat->Misc.LastTTPageUsed = 0xFFFF;
        // Calculate the log2 of Pages per block value
        Log2PgsPerBlk = NandUtilGetLog2(hNand->NandDevInfo.PgsPerBlk);
        for (
        page = 0;
        page < (nandILTat->tatHandler.WorkingCopy.TtBlkCnt <<
            nandILTat->Misc.bsc4PgsPerBlk);
        page++)
        {
            LBNum = MACRO_DIV_POW2_LOG2NUM(page, Log2PgsPerBlk);
            pageOffset = MACRO_MOD_LOG2NUM(page, Log2PgsPerBlk);

            ClearPhysicalPageArray(hNand,hNand->ITat.pPhysPgs);
            SetPhysicalPageArray(
                hNand,
                hNand->ITat.pPhysPgs,
                &nandILTat->tatHandler.WorkingCopy.TtAllocBlk[LBNum],
                pageOffset);
            bank = nandILTat->tatHandler.WorkingCopy.TtAllocBlk[LBNum].BnkNum;
            NandTLReadPhysicalPage(
                hNand,
                hNand->ITat.pPhysPgs,
                bank << nandILTat->Misc.bsc4SctsPerPg,
                hNand->TmpPgBuf,
                hNand->NandDevInfo.SctsPerPg, 0, 0);

            //ChkDriverErr(NandInterleaveTTable::FAILURE);
            Status =
                IsDefaultData(hNand->TmpPgBuf, hNand->NandDevInfo.PageSize);
            if (Status)
                continue;
            nandILTat->Misc.LastTTPageUsed = page;
            LOG_NAND_DEBUG_INFO(("\r\nLastTTPageUsed=%d",
                nandILTat->Misc.LastTTPageUsed),
                TAT_DEBUG_INFO);
        }
        if (nandILTat->Misc.LastTTPageUsed == 0xFFFF)
            nandILTat->Misc.LastTTPageUsed =
                (nandILTat->tatHandler.WorkingCopy.TtBlkCnt <<
                nandILTat->Misc.bsc4PgsPerBlk) - 1;
    }
    return(NvSuccess);
}


#if defined(LISTBADBLOCKS)
void ListAllBadBlocks(NvNandHandle hNand)
{
        NvS32 physBlk;
        NvU32 bank;
        NandInterleaveTAT *nandILTat = &hNand->ITat;
        BlockInfo Block;
        NvU32 BadCount = 0;
        NvOsDebugPrintf("\r\nListing all bad blocks in the device :");

        for (
            physBlk = 0;
            physBlk <  hNand->FtlEndPba;
            physBlk++)
        {
            for (bank = 0; bank < nandILTat->Misc.NoOfILBanks; bank++)
            {

                SetBlockInfo(&Block,bank,physBlk);
                // read the redundant area
                if(IsBlockBad(hNand,&Block,NV_FALSE))
                {
                    TagInformation *pTagInfo;
                    NvU8 size;
                    // Increment the bad block count
                    BadCount++;
                    pTagInfo = (TagInformation*)(hNand->pSpareBuf +
                        hNand->gs_NandDeviceInfo.TagOffset);
                    NvOsDebugPrintf("\r\n**Detected bad block: bank %d, "
                        "block %d", bank, physBlk);
                    for(size = 0; size < sizeof(TagInformation); size++)
                    {
                        NvOsDebugPrintf("TagInfo[%d] = %d ",
                            size, *(pTagInfo + size));
                    }
                }
            }
        }
        if (BadCount)
        {
            NvOsDebugPrintf("\r\nFound %d bad blocks ", BadCount);
        }
 }
#endif

#if NEW_TT_CODE
void ReplaceBlockSet(NvNandHandle hNand)
{
    NvS32 i = 0;
    NvS32 SrcPage = 0;
    NvU32 SrcBank = 0;
    NvU32 DestPage = 0;
    NvU32 DstnBank = 0;
    NvU32 PageOffset = 0;
    NvU32 LogicalBlock = 0;
    NvU32 Log2PgsPerBlk = 0;
    NvS32 ttBlock = 0;
    NvU8 * TmpBuff = NULL;
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    BlockInfo* nextTTBlockSet;
    NvError WriteStatus = NvSuccess;
    NvBool ContinueInLoop = NV_TRUE;
    TagInformation * pTagInfo = NULL;

    TmpBuff = AllocateVirtualMemory(hNand->NandDevInfo.PageSize,TAT_ALLOC);
    nextTTBlockSet = (BlockInfo *)AllocateVirtualMemory(
        sizeof(BlockInfo) * nandILTat->tatHandler.WorkingCopy.TtBlkCnt,
        TAT_ALLOC);//uncached
    do
    {
ReplaceBlockSetAgain:
        /*
         * now take the next TT block set and update them
         * with relevant information
         */
        for (i = 0; i < nandILTat->tatHandler.WorkingCopy.TtBlkCnt; i++)
        {
            InitBlockInfo(&nextTTBlockSet[i]);
        }

        // get the next set of TT blocks. We expect the erased blocks here.
        GetNextTTBlockSet(hNand, nextTTBlockSet);
        // update the last used block number
        *nandILTat->tatHandler.WorkingCopy.lastUsedTTBlock =
            nextTTBlockSet[nandILTat->tatHandler.WorkingCopy.TtBlkCnt - 1];

        // copy the old TT into next set of TT blocks
        DestPage = 0;
        for (
            SrcPage = 0;
            SrcPage < nandILTat->tatHandler.WorkingCopy.TtPageCnt;
            SrcPage++, DestPage++)
        {
            LogicalBlock = nandILTat->tatHandler.WorkingCopy.TtAllocPg[SrcPage]
                >> nandILTat->Misc.bsc4PgsPerBlk;
            PageOffset = MACRO_MOD_LOG2NUM(
                nandILTat->tatHandler.WorkingCopy.TtAllocPg[SrcPage],
                Log2PgsPerBlk);

            ClearPhysicalPageArray(hNand,hNand->ITat.pPhysPgs);
            SetPhysicalPageArray(hNand,
                hNand->ITat.pPhysPgs,
                &nandILTat->tatHandler.WorkingCopy.TtAllocBlk[LogicalBlock],
                PageOffset);

            for(;;)
            {
                ClearPhysicalPageArray(hNand,hNand->ITat.pWritePhysPgs);
                SetPhysicalPageArray(hNand,
                    hNand->ITat.pWritePhysPgs,
                    &nextTTBlockSet[
                    (DestPage >> nandILTat->Misc.bsc4PgsPerBlk)],
                    MACRO_MOD_LOG2NUM(DestPage, Log2PgsPerBlk));

                DstnBank =
                    nextTTBlockSet[
                    (DestPage >> nandILTat->Misc.bsc4PgsPerBlk)].BnkNum;

                ReadTagInfo(
                    hNand,
                    hNand->ITat.pWritePhysPgs,
                    (DstnBank << nandILTat->Misc.bsc4SctsPerPg),
                    hNand->pSpareBuf);

                pTagInfo = (TagInformation *)(hNand->pSpareBuf +
                hNand->gs_NandDeviceInfo.TagOffset);

                // if the page is not good for use, get next page
                if (pTagInfo->Info.PageStatus ==
                    Information_Status_PAGE_BAD)
                {
                    MarkBlockAsBad(hNand, DstnBank,
                        hNand->ITat.pWritePhysPgs[DstnBank] >>
                        hNand->ITat.Misc.bsc4PgsPerBlk);
                    goto ReplaceBlockSetAgain;
                }

                // read source/old page and Write to new page
                SrcBank = nandILTat->tatHandler.WorkingCopy.TtAllocBlk[
                    LogicalBlock].BnkNum;

                NandTLReadPhysicalPage(
                    hNand,
                    hNand->ITat.pPhysPgs,
                    SrcBank << nandILTat->Misc.bsc4SctsPerPg,
                    (NvU8 *)TmpBuff,
                    hNand->NandDevInfo.SctsPerPg, 0, NV_TRUE);

                WriteStatus = Write(
                    hNand,
                    hNand->ITat.pWritePhysPgs,
                    DstnBank << nandILTat->Misc.bsc4SctsPerPg,
                    (NvU8 *)TmpBuff,
                    hNand->NandDevInfo.SctsPerPg,
                    NULL);

                if (WriteStatus != NvSuccess)
                {
                    MarkBlockAsBad(hNand, DstnBank,
                        hNand->ITat.pWritePhysPgs[DstnBank] >>
                        hNand->ITat.Misc.bsc4PgsPerBlk);
                    goto ReplaceBlockSetAgain;
                }
                break;
            }
            // modify the TAT allocation page in sync with the new TT pages
            nandILTat->tatHandler.WorkingCopy.TtAllocPg[SrcPage] =
                DestPage;
            LOG_NAND_DEBUG_INFO(("\r\n copied the TT page from "
                "srcpage=%d to dstnpage=%d",
                SrcPage, DestPage), TAT_DEBUG_INFO);
        }

        /*
         * update the working copy allocation block
         * with the new set of TT blocks
         */
        for (
            ttBlock = 0;
            ttBlock < nandILTat->tatHandler.WorkingCopy.TtBlkCnt;
            ttBlock++)
        {
            nandILTat->tatHandler.WorkingCopy.TtAllocBlk[ttBlock] =
                nextTTBlockSet[ttBlock];
        }

        // make the last TT page used to the starting of the spare tt pages
        nandILTat->Misc.LastTTPageUsed = DestPage - 1;
        LOG_NAND_DEBUG_INFO(("\r\n last used page is updated to=%d",
            nandILTat->Misc.LastTTPageUsed), TAT_DEBUG_INFO);
        /*
         * HINT: need to check if there are any bad pages in the
         * default allocated TT pages
         * since TAT is modified put the TAT signature
         */
        WriteTATSignature(hNand, NV_TRUE);
        ContinueInLoop = NV_FALSE;
    }while(ContinueInLoop);
}

#endif
/**
 * @brief This method gets the next TT page for using.
 *
 * @param nextTTBlock returns the TT block that can be used.
 * @param nextTTPage page in the block that can be used.
 * @param getAbsolute if NV_TRUE returns the physical block number.
 * if false, returns the logical block number.
 */
void
GetNextTTpage(
    NvNandHandle hNand,
    BlockInfo* nextTTBlock,
    NvS32* nextTTPage,
    NvBool getAbsolute)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    NvBool flushTable = NV_FALSE;
    NvError writeStatus;
    NvS32 i,LBNum,pageOffset;
    NvS8* tmpBuff = 0;
    BlockInfo* nextTTBlockSet;
    NvS32 destPage ,srcPage,bank;
    NvS32 srcBank, dstnBank,ttBlock,lBlock;
    // variable for log2 of pages-per-block
    NvU32 Log2PgsPerBlk;
    TagInformation *pTagInfo;

    tmpBuff = AllocateVirtualMemory(hNand->NandDevInfo.PageSize,TAT_ALLOC);
    nextTTBlockSet = (BlockInfo *)AllocateVirtualMemory(
        sizeof(BlockInfo) * nandILTat->tatHandler.WorkingCopy.TtBlkCnt,
        TAT_ALLOC);//uncached
    // Calculate the log2 of Pages per block value
    Log2PgsPerBlk = NandUtilGetLog2(hNand->NandDevInfo.PgsPerBlk);

    for(;;)
    {
        // check if we are with in the pages given for TT management
        if ((nandILTat->Misc.LastTTPageUsed + 1) <
            nandILTat->Misc.PgsAlloctdForTT)
        {
            /*
             * YES. we still have some pages to work with, increase
             * the lastPage used and point to the next page
             */
            nandILTat->Misc.LastTTPageUsed++;
            // verify whether it is good to use
            LBNum = MACRO_DIV_POW2_LOG2NUM(nandILTat->Misc.LastTTPageUsed,
                Log2PgsPerBlk);
            pageOffset =
                MACRO_MOD_LOG2NUM(nandILTat->Misc.LastTTPageUsed,
                Log2PgsPerBlk);

            /*
             * check if we are trying to Write into tag info page. if so, skip
             * the page.
             */
/*
            if (pageOffset == nandILTat->Misc.TagInfoPage)
                continue;
*/

            bank = nandILTat->tatHandler.WorkingCopy.TtAllocBlk[LBNum].BnkNum;
            ClearPhysicalPageArray(hNand,hNand->ITat.pPhysPgs);
            SetPhysicalPageArray(
                hNand,
                hNand->ITat.pPhysPgs,
                &nandILTat->tatHandler.WorkingCopy.TtAllocBlk[LBNum],
                pageOffset);
            ReadTagInfo(
                hNand,
                hNand->ITat.pPhysPgs,
                (bank << nandILTat->Misc.bsc4SctsPerPg),
                hNand->pSpareBuf);
            //ChkDriverErr();
            pTagInfo = (TagInformation *)(hNand->pSpareBuf +
                hNand->gs_NandDeviceInfo.TagOffset);
            // if the page is not good for use, get next page
            if (pTagInfo->Info.PageStatus ==
                Information_Status_PAGE_BAD)
                continue;
            if (getAbsolute)
            {
                /*
                 * here we have to return the actual physical block in which
                 * the page exists and the page offset
                 */
                *nextTTBlock =
                    nandILTat->tatHandler.WorkingCopy.TtAllocBlk[LBNum];
                *nextTTPage = pageOffset;
            }
            else
            {
                /*
                 * here we have to return the start block number of the TT and
                 * the page offset from that block
                 */
                *nextTTBlock = nandILTat->tatHandler.WorkingCopy.TtAllocBlk[0];
                *nextTTPage = nandILTat->Misc.LastTTPageUsed;
            }
        }
        else
        {
            LOG_NAND_DEBUG_INFO(("\r\n Acquiring TT page from next available "
                "TT block \r\n"), TAT_DEBUG_INFO);

            /*
             * we do not have any pages left for TT management.
             * Flush all the cached pages in the current TT
             */
            flushTable = NV_TRUE;

            /*
             * now take the next TT block set and update them
             * with relevant information
             */
            for (i = 0; i < nandILTat->tatHandler.WorkingCopy.TtBlkCnt; i++)
            {
                InitBlockInfo(&nextTTBlockSet[i]);
            }
            // get the next set of TT blocks. We expect the erased blocks here.
            GetNextTTBlockSet(hNand, nextTTBlockSet);
            // update the last used block number
            *nandILTat->tatHandler.WorkingCopy.lastUsedTTBlock =
                nextTTBlockSet[nandILTat->tatHandler.WorkingCopy.TtBlkCnt - 1];

            // copy the old TT into next set of TT blocks
            destPage = 0;
            for (
                srcPage = 0;
                srcPage < nandILTat->tatHandler.WorkingCopy.TtPageCnt;
                srcPage++,destPage++)
            {

                NvBool dirtyBitSet =
                    (nandILTat->tatHandler.WorkingCopy.TtAllocPg[srcPage] &
                    NIT_MASK_DIRTY_BIT) ? NV_TRUE : NV_FALSE;

                if (dirtyBitSet)
                {
                    /*
                     * if the page is dirty, don't copy it.
                     * let the flush command do it.
                     */
                    LOG_NAND_DEBUG_INFO(("\r\n src page=%d is dirty, "
                        "skipping it", srcPage), TAT_DEBUG_INFO);
                    // adjust dest page.
                    destPage--;
                    continue;
                }

                lBlock = nandILTat->tatHandler.WorkingCopy.TtAllocPg[srcPage]
                    >> nandILTat->Misc.bsc4PgsPerBlk;
                pageOffset = MACRO_MOD_LOG2NUM(
                    nandILTat->tatHandler.WorkingCopy.TtAllocPg[srcPage],
                    Log2PgsPerBlk);

                ClearPhysicalPageArray(hNand,hNand->ITat.pPhysPgs);
                SetPhysicalPageArray(hNand,
                    hNand->ITat.pPhysPgs,
                    &nandILTat->tatHandler.WorkingCopy.TtAllocBlk[lBlock],
                    pageOffset);

                for(;;)
                {
                    ClearPhysicalPageArray(hNand,hNand->ITat.pWritePhysPgs);
                    SetPhysicalPageArray(hNand,
                        hNand->ITat.pWritePhysPgs,
                        &nextTTBlockSet[
                        (destPage >> nandILTat->Misc.bsc4PgsPerBlk)],
                        MACRO_MOD_LOG2NUM(destPage, Log2PgsPerBlk));

                    /*
                     * check if we are trying to Write into tag info page.
                     * if so, skip the page.
                     */
/*
                    if ((MACRO_MOD_LOG2NUM(destPage, Log2PgsPerBlk)) ==
                        nandILTat->Misc.TagInfoPage)
                    {
                        destPage++;
                        continue;
                    }
*/
                    // check if dest page is good to Write
                    dstnBank =
                        nextTTBlockSet[
                        (destPage >> nandILTat->Misc.bsc4PgsPerBlk)].BnkNum;
                    ReadTagInfo(
                        hNand,
                        hNand->ITat.pWritePhysPgs,
                        (dstnBank << nandILTat->Misc.bsc4SctsPerPg),
                        hNand->pSpareBuf);
                    pTagInfo = (TagInformation *)(hNand->pSpareBuf +
                        hNand->gs_NandDeviceInfo.TagOffset);
                    // if the page is not good for use, get next page
                    if (pTagInfo->Info.PageStatus ==
                        Information_Status_PAGE_BAD)
                    {
                        destPage++;
                        continue;
                    }

                    // read source/old page and Write to new page
                    srcBank = nandILTat->tatHandler.WorkingCopy.TtAllocBlk[
                        lBlock].BnkNum;
                    NandTLReadPhysicalPage(
                        hNand,
                        hNand->ITat.pPhysPgs,
                        srcBank << nandILTat->Misc.bsc4SctsPerPg,
                        (NvU8 *)tmpBuff,
                        hNand->NandDevInfo.SctsPerPg,0, 0);
                    //ChkDriverErr();
                    writeStatus = Write(
                        hNand,
                        hNand->ITat.pWritePhysPgs,
                        dstnBank << nandILTat->Misc.bsc4SctsPerPg,
                        (NvU8 *)tmpBuff,
                        hNand->NandDevInfo.SctsPerPg,
                        NULL);

                    if (writeStatus != NvSuccess)
                    {
                        MarkPageAsBad(
                            hNand,
                            hNand->ITat.pWritePhysPgs,
                            (dstnBank << nandILTat->Misc.bsc4SctsPerPg),
                            NV_FALSE);
                        destPage++;
                        continue;
                    }
                    break;
                }
                // modify the TAT allocation page in sync with the new TT pages
                nandILTat->tatHandler.WorkingCopy.TtAllocPg[srcPage] =
                    destPage;
                LOG_NAND_DEBUG_INFO(("\r\n copied the TT page from "
                    "srcpage=%d to dstnpage=%d",
                    srcPage,destPage), TAT_DEBUG_INFO);
            }

            /*
             * update the working copy allocation block
             * with the new set of TT blocks
             */
            for (
                ttBlock = 0;
                ttBlock < nandILTat->tatHandler.WorkingCopy.TtBlkCnt;
                ttBlock++)
            {
                nandILTat->tatHandler.WorkingCopy.TtAllocBlk[ttBlock] =
                    nextTTBlockSet[ttBlock];
            }

            // make the last TT page used to the starting of the spare tt pages
            nandILTat->Misc.LastTTPageUsed = destPage - 1;
            LOG_NAND_DEBUG_INFO(("\r\n last used page is updated to=%d",
                nandILTat->Misc.LastTTPageUsed), TAT_DEBUG_INFO);
            /*
             * HINT: need to check if there are any bad pages in the
             * default allocated TT pages
             * since TAT is modified put the TAT signature
             */
            WriteTATSignature(hNand, NV_TRUE);

            //flushTAT();
            // now try to get the next TT page from the new TT block set
            continue;
        }
        break;
    }
    if (flushTable) ForceTTFlush(hNand);
    ReleaseVirtualMemory(tmpBuff,TAT_ALLOC);
    ReleaseVirtualMemory(nextTTBlockSet,TAT_ALLOC);

}

NvError WriteTATPage(
    NvNandHandle hNand,
    NvS32 CurTATBlk,
    NvBool IsFirstTime,
    NvS32 TatPage,
    NvBool WriteOnlyHeader)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    NvS32 TatBlock, bank;
    NvS32 BadTATPhysBlk = -1;
    NvS32 BadTATBank = -1;
    NvU32 i;
    NvBool BlockBad = NV_TRUE;
    NvError Status = NvSuccess;
    NvS8* TmpBuff = NULL;
    static NvS32 BadTatLBlk = -1;
    TagInformation *pTagInfo;

    BlockBad = IsBlockBad(hNand,
        &nandILTat->tatHandler.tatBlocks[CurTATBlk], NV_FALSE);

    // don't check for the error. we will get ecc error as we
    // haven't written this area before.
    if (BlockBad)
    {
        LOG_NAND_DEBUG_INFO1(("\r\nBad Block found at LBA %d", CurTATBlk), 1);
        return NvError_NandTLFailed;
    }

    TatBlock = nandILTat->tatHandler.tatBlocks[CurTATBlk].BlkNum;
    bank = nandILTat->tatHandler.tatBlocks[CurTATBlk].BnkNum;
    ClearPhysicalPageArray(hNand, hNand->ITat.pPhysPgs);
    SetPhysicalPageArray(
        hNand,
        hNand->ITat.pPhysPgs,
        &nandILTat->tatHandler.tatBlocks[CurTATBlk],
        TatPage);

    ReadTagInfo(
        hNand,
        hNand->ITat.pPhysPgs,
        (bank << nandILTat->Misc.bsc4SctsPerPg),
        hNand->pSpareBuf);
    pTagInfo = (TagInformation *)(hNand->pSpareBuf +
        hNand->gs_NandDeviceInfo.TagOffset);

    if ((hNand->pSpareBuf[1] == 0xFF) && pTagInfo->Info.PageStatus)
    {
        if (IsFirstTime)
        {
            NvOsStrncpy(
                (char *)(nandILTat->tatHandler.WorkingCopy.headerSignature)
                , (const char *)(NAND_IL_TAT_HEADER),
                sizeof(NAND_IL_TAT_HEADER));
            NvOsStrncpy(
                (char *)(nandILTat->tatHandler.WorkingCopy.footerSignature)
                , (const char *)(NAND_IL_TAT_FOOTER),
                sizeof(NAND_IL_TAT_FOOTER));

            // update last used block number for all the interleaved banks
            for (i= 0; i < nandILTat->Misc.NoOfILBanks; i++)
            {
                nandILTat->tatHandler.WorkingCopy.lastPhysicalBlockUsed[i] =
                    hNand->FtlStartPba;
            }
        }

        if (WriteOnlyHeader)
        {
            // Increment the Rev Number by 1
            *nandILTat->tatHandler.WorkingCopy.RevNum += 1;

            TmpBuff = AllocateVirtualMemory(hNand->NandDevInfo.PageSize,
                TAT_ALLOC);
            NvOsMemset(TmpBuff, 0xFF,
                hNand->NandDevInfo.SctsPerPg <<
                nandILTat->Misc.bsc4SectorSize);
            NvOsMemcpy(TmpBuff,
                nandILTat->tatHandler.WorkingCopy.headerSignature,
                hNand->NandDevInfo.SectorSize);
            Status = Write(
                hNand,
                hNand->ITat.pPhysPgs,
                (bank << nandILTat->Misc.bsc4SctsPerPg),
                (NvU8 *)TmpBuff,
                hNand->NandDevInfo.SctsPerPg, NULL);
#if TEST_CASE
            if (CurTATBlk == 6)
                if ((TatPage == 3) || (TatPage == 4))
                    Status = NandTL_WRITE_ERROR;
#endif
        }
        else
        {
            // write TAT page
            Status = Write(
                hNand,
                hNand->ITat.pPhysPgs,
                bank << nandILTat->Misc.bsc4SctsPerPg,
                (NvU8*)nandILTat->tatHandler.wBuffer,
                hNand->NandDevInfo.SctsPerPg, NULL);
#if TEST_CASE
            if (CurTATBlk == 8)
                if ((TatPage == 3) || (TatPage == 2))
                    Status = NandTL_WRITE_ERROR;
#endif
        }

        if (BadTatLBlk != -1)
        {
            // If BadTatBlk is not -1 then previous write has failed so the
            // block has to be made bad.
            BadTATPhysBlk = nandILTat->tatHandler.tatBlocks[BadTatLBlk].BlkNum;
            BadTATBank = nandILTat->tatHandler.tatBlocks[BadTatLBlk].BnkNum;
            MarkBlockAsBad(hNand, BadTATBank, BadTATPhysBlk);
            LOG_NAND_DEBUG_INFO1(
                ("\r\nMarked blk bad bank = %d, block = %d Rev = %d lba = %d",
                BadTATBank, BadTATPhysBlk,
                *nandILTat->tatHandler.WorkingCopy.RevNum, BadTatLBlk), TAT_DEBUG_INFO);
            BadTatLBlk = -1;
        }

        if (Status == NvSuccess)
        {
            nandILTat->tatHandler.WorkingCopy.IsDirty =
                (WriteOnlyHeader ? NV_TRUE : NV_FALSE);

            LOG_NAND_DEBUG_INFO(
                ("\r\nwritten TAT page = %d, bank = %d, block = %d Rev = %d lba = %d WriteOnlyHeader = %d",
                TatPage, bank, TatBlock,
                *nandILTat->tatHandler.WorkingCopy.RevNum, CurTATBlk,
                WriteOnlyHeader), TAT_DEBUG_INFO);
            SetBlockInfo(&nandILTat->tatHandler.WorkingCopy.TatBlkNum,
                bank, TatBlock);

            nandILTat->tatHandler.WorkingCopy.TatPgNum = TatPage;
            // Write is success
            nandILTat->Misc.CurrentTatLBInUse = CurTATBlk;
        }
        else
        {
            // Store the Tat Block number of bad block.
            // If the value is not -1 then the blocka ismade block when the
            // write write happens.
            BadTatLBlk = CurTATBlk;
            if (WriteOnlyHeader)
            {
                // TAT Write Failed. So decrement the Rev Num.
                *nandILTat->tatHandler.WorkingCopy.RevNum -= 1;
            }
            LOG_NAND_DEBUG_INFO1(
                ("\r\nTAT write failed page = %d, bank = %d, block = %d Rev = %d lba = %d WriteOnlyHeader = %d",
                TatPage, bank, TatBlock,
                *nandILTat->tatHandler.WorkingCopy.RevNum, CurTATBlk,
                WriteOnlyHeader), TAT_DEBUG_INFO);
        }
    }

    if (TmpBuff)
        ReleaseVirtualMemory(TmpBuff, TAT_ALLOC);

    return Status;
}

#if NEW_CODE
/**
 * @brief This method writes the TAT signature into the TAT block.
 *
 * @param writeOnlyHeader if writeOnlyHeader is true, it write the only
 * header signature into the TAT block. if it is false, it write the
 * complete TAT info along with TAT header and footer signatures.
 * default value was true
 */
static NvError
WriteTATSignature(NvNandHandle hNand, NvBool writeOnlyHeader)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    NvS32 PrevTATBlockSetInUse = 0;
    NvError Status = NvSuccess;
    NvBool OnlyHeader = writeOnlyHeader;
    NvBool IsBlkBoundary = NV_FALSE;
    NvBool IsHeaderWriteFailed = NV_FALSE;

    if (writeOnlyHeader)
    {
        // if TAT is already dirty,no need to write any signature because
        // TAT signature exists on device. just return from here
        if (nandILTat->tatHandler.WorkingCopy.IsDirty)
            return NvSuccess;
    }

    // Assign CurrentTatPgNum to NextTatPgNum
    nandILTat->tatHandler.WorkingCopy.NextTatPgNum =
        nandILTat->tatHandler.WorkingCopy.TatPgNum;

    // Check for roll over of rev number & try 2 look ahead for the roll over.
    if (*nandILTat->tatHandler.WorkingCopy.RevNum >= 0xFFFFFF00)
    {
        // We need to write TAT to a new block and erase all the other TAT blks.
        hNand->ITat.RevNumRollOverAboutToHappen = NV_TRUE;
    }

    nandILTat->tatHandler.WorkingCopy.NextTatPgNum++;
    for(;;)
    {
        // check if our current TAT page is block boundary
        if (nandILTat->tatHandler.WorkingCopy.NextTatPgNum <
            hNand->NandDevInfo.PgsPerBlk)
        {
            Status = WriteTATPage(
                hNand,
                nandILTat->Misc.CurrentTatLBInUse,
                NV_FALSE,
                nandILTat->tatHandler.WorkingCopy.NextTatPgNum,
                OnlyHeader);
            if (Status != NvSuccess)
            {
                // store the current block in use
                PrevTATBlockSetInUse = nandILTat->Misc.CurrentTatLBInUse;

                // go to the next set of TAT blocks. we expect a erased
                // block(s) here.
                GetNextTATBlock(hNand, PrevTATBlockSetInUse,
                    &nandILTat->Misc.CurrentTatLBInUse);
                if (nandILTat->Misc.CurrentTatLBInUse == PrevTATBlockSetInUse)
                {
                    NvOsDebugPrintf("\r\n NO FREE TAT BLOCKS AVAILABLE\n");
                    return NvError_NandNoFreeBlock;
                }
                nandILTat->tatHandler.WorkingCopy.NextTatPgNum = 0;
                if (OnlyHeader)
                {
                    // If Only header fails then write previous valid TAT and
                    // then write the header.
                    OnlyHeader = NV_FALSE;
                    IsHeaderWriteFailed = NV_TRUE;
                }
                continue;
            }
            else if (IsBlkBoundary)
            {
                // Control comes here if the request to TAT write was outside
                // the block boundary.
                OnlyHeader = writeOnlyHeader;
                IsBlkBoundary = NV_FALSE;
                if (!writeOnlyHeader)
                    break;
                nandILTat->tatHandler.WorkingCopy.NextTatPgNum++;
                continue;
            }
            else if (IsHeaderWriteFailed)
            {
                // Contol comes here if Write to header fails and then previous
                // valid TAT is written.
                OnlyHeader = writeOnlyHeader;
                IsHeaderWriteFailed = NV_FALSE;
                nandILTat->tatHandler.WorkingCopy.NextTatPgNum++;
                continue;
            }
        }
        else
        {
            IsBlkBoundary = NV_TRUE;
            OnlyHeader = NV_FALSE;
            // store the current block in use
            PrevTATBlockSetInUse = nandILTat->Misc.CurrentTatLBInUse;
            // go to the next set of TAT blocks. we expect a erased
            // block(s) here.
            GetNextTATBlock(hNand, PrevTATBlockSetInUse,
                &nandILTat->Misc.CurrentTatLBInUse);
            if (nandILTat->Misc.CurrentTatLBInUse == PrevTATBlockSetInUse)
            {
                NvOsDebugPrintf("\r\n NO FREE TAT BLOCKS AVAILABLE\n");
                return NvError_NandNoFreeBlock;
            }
            nandILTat->tatHandler.WorkingCopy.NextTatPgNum = 0;
            continue;
        }
        break;
    }
    return NvSuccess;
}

#else
/**
 * @brief This method writes the TAT signature into the TAT block.
 *
 * @param writeOnlyHeader if writeOnlyHeader is NV_TRUE, it write the only
 * header signature into the TAT block. if it is false, it write the
 * complete TAT info along with TAT header and footer signatures.
 * default value was NV_TRUE
 */
void
WriteTATSignature(NvNandHandle hNand, NvBool writeOnlyHeader)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    NvError sts;
    NvS32 bank,prevTATBlocksSetInUSe;
    NvS8* tmpBuff = 0;
    TagInformation *pTagInfo;
    tmpBuff = AllocateVirtualMemory(hNand->NandDevInfo.PageSize,TAT_ALLOC);

    /*
     * if TAT is already dirty,no need to write any signature because
     * TAT signature exists on device. Just return from here
     */
    if (writeOnlyHeader && nandILTat->tatHandler.WorkingCopy.IsDirty)
    {
        ReleaseVirtualMemory ( tmpBuff, TAT_ALLOC);
        return;
    }

    nandILTat->tatHandler.WorkingCopy.NextTatPgNum =
        nandILTat->tatHandler.WorkingCopy.TatPgNum;
    nandILTat->tatHandler.WorkingCopy.NextTatBlkNum =
        nandILTat->tatHandler.WorkingCopy.TatBlkNum;

    // update revision number
    if (writeOnlyHeader)
    {
        *nandILTat->tatHandler.WorkingCopy.RevNum =
            (*nandILTat->tatHandler.WorkingCopy.RevNum) + 1;
    }

    {
        /*
         * Check for the roll over of revision number.
         * try to look ahead for the roll over.
         */
        if (*nandILTat->tatHandler.WorkingCopy.RevNum >= 0xFFFFFF00)
        {
            hNand->ITat.RevNumRollOverAboutToHappen = NV_TRUE;
            /*
             * We need to write TAT to a new block and erase
             * all the other TAT blocks.
             */
        }
    }

    for(;;)
    {
        nandILTat->tatHandler.WorkingCopy.NextTatPgNum++;

        // check if our current TAT page is block boundary
        if (nandILTat->tatHandler.WorkingCopy.NextTatPgNum <
            hNand->NandDevInfo.PgsPerBlk)
        {

            // skip tag info page
/*
             if (nandILTat->tatHandler.WorkingCopy.NextTatPgNum ==
                nandILTat->Misc.TagInfoPage)
                continue;
*/
            // verify whether it is good to use
            ClearPhysicalPageArray(hNand,hNand->ITat.pPhysPgs);
            SetPhysicalPageArray(
                hNand,
                hNand->ITat.pPhysPgs,
                &nandILTat->tatHandler.WorkingCopy.NextTatBlkNum,
                nandILTat->tatHandler.WorkingCopy.NextTatPgNum);

            bank = nandILTat->tatHandler.WorkingCopy.NextTatBlkNum.BnkNum;
            sts = ReadTagInfo(
                hNand,
                hNand->ITat.pPhysPgs,
                (bank << nandILTat->Misc.bsc4SctsPerPg),
                hNand->pSpareBuf);
            pTagInfo = (TagInformation *)(hNand->pSpareBuf +
                hNand->gs_NandDeviceInfo.TagOffset);
            // if the page is not good for use, get next page
            if ((pTagInfo->Info.PageStatus ==
                Information_Status_PAGE_BAD))// || (sts != NvSuccess) )
                continue;
        } else
        {

            // store the current block in use
            prevTATBlocksSetInUSe = nandILTat->Misc.CurrentTatLBInUse;
            /*
             * go to the next set of TAT blocks. we expect a erased
             * block(s) here.
             */
            GetNextTATBlock(
                hNand,
                prevTATBlocksSetInUSe,
                &nandILTat->Misc.CurrentTatLBInUse);

            nandILTat->tatHandler.WorkingCopy.NextTatBlkNum =
                nandILTat->tatHandler.tatBlocks[
                nandILTat->Misc.CurrentTatLBInUse];
            nandILTat->tatHandler.WorkingCopy.NextTatPgNum = -1;

            /*
             * get a known good page and write the working copy
             * into the new block.
             */
            for(;;)
            {
                nandILTat->tatHandler.WorkingCopy.NextTatPgNum++;
                // skip taginfo page
/*                if (nandILTat->tatHandler.WorkingCopy.NextTatPgNum ==
                    nandILTat->Misc.TagInfoPage)
                    continue;*/
                ClearPhysicalPageArray(hNand,hNand->ITat.pPhysPgs);
                SetPhysicalPageArray(
                    hNand,
                    hNand->ITat.pPhysPgs,
                    &nandILTat->tatHandler.WorkingCopy.NextTatBlkNum,
                    nandILTat->tatHandler.WorkingCopy.NextTatPgNum);

                bank = nandILTat->tatHandler.WorkingCopy.NextTatBlkNum.BnkNum;
                sts = ReadTagInfo(
                    hNand,
                    hNand->ITat.pPhysPgs,
                    (bank << nandILTat->Misc.bsc4SctsPerPg),
                    hNand->pSpareBuf);

                pTagInfo = (TagInformation *)(hNand->pSpareBuf +
                    hNand->gs_NandDeviceInfo.TagOffset);
                // if the page is not good for use, get next page
                // if ( (hNand->ITat.TagInfo.Info.PageStatus ==
                // Information_Status_PAGE_BAD) || (sts != NvSuccess) ) goto skipThis;
                if (pTagInfo->Info.PageStatus ==
                    Information_Status_PAGE_BAD)
                        continue;

                // modify the data in working copy to point to the new page
                nandILTat->tatHandler.WorkingCopy.TatBlkNum =
                    nandILTat->tatHandler.WorkingCopy.NextTatBlkNum;
                nandILTat->tatHandler.WorkingCopy.TatPgNum =
                    nandILTat->tatHandler.WorkingCopy.NextTatPgNum;
                sts = Write(
                    hNand,
                    hNand->ITat.pPhysPgs,
                    (bank << nandILTat->Misc.bsc4SctsPerPg),
                    (NvU8 *)nandILTat->tatHandler.wBuffer,
                    hNand->NandDevInfo.SctsPerPg,
                    NULL);

                if (sts != NvSuccess)
                {
                    // Mark page as bad
                    MarkPageAsBad(
                        hNand,
                        hNand->ITat.pPhysPgs,
                        (bank << nandILTat->Misc.bsc4SctsPerPg),
                        NV_TRUE);
                    continue;
                }
                else
                {

                    LOG_NAND_DEBUG_INFO(("\r\n TAT block changed and written "
                        "TAT page=%d, bank=%d,block=%d",
                        nandILTat->tatHandler.WorkingCopy.NextTatPgNum,
                        nandILTat->tatHandler.WorkingCopy.NextTatBlkNum.BnkNum,
                        nandILTat->tatHandler.WorkingCopy.NextTatBlkNum.\
                        BlkNum),
                        TAT_DEBUG_INFO);
                }
                break;
            }
            /*
             * we wrote our working copy into the new TAT block; continue our
             * search for next TAT page in this new block. Increment
             * version number.
             */
            if (writeOnlyHeader)
            {

                *nandILTat->tatHandler.WorkingCopy.RevNum =
                    (*nandILTat->tatHandler.WorkingCopy.RevNum) + 1;
                continue;
            }
            else
            {
                /*
                 * if we flushing our TAT back, then we don't need to write
                 * next page. Just return from here.
                 */
                nandILTat->tatHandler.WorkingCopy.IsDirty = NV_FALSE;
                if ( tmpBuff)
                    ReleaseVirtualMemory( tmpBuff, TAT_ALLOC);
                return;
            }
        }

        ClearPhysicalPageArray(hNand,hNand->ITat.pPhysPgs);
        SetPhysicalPageArray(
            hNand,
            hNand->ITat.pPhysPgs,
            &nandILTat->tatHandler.WorkingCopy.NextTatBlkNum,
            nandILTat->tatHandler.WorkingCopy.NextTatPgNum);
        bank = nandILTat->tatHandler.WorkingCopy.NextTatBlkNum.BnkNum;
        /*
         * if we are here, we suppose that we have gotten a free page
         * number for writing the TAT signature.
         */
        if (writeOnlyHeader)
        {

            NvOsMemset(tmpBuff, 0xFF,
                hNand->NandDevInfo.SctsPerPg <<
                nandILTat->Misc.bsc4SectorSize);
            NvOsMemcpy(
                tmpBuff,
                nandILTat->tatHandler.WorkingCopy.headerSignature,
                hNand->NandDevInfo.SectorSize);
            sts = Write(
                hNand,
                hNand->ITat.pPhysPgs,
                (bank << nandILTat->Misc.bsc4SctsPerPg),
                (NvU8 *)tmpBuff,
                hNand->NandDevInfo.SctsPerPg, NULL);
        }
        else
        {

            sts = Write(
                hNand,
                hNand->ITat.pPhysPgs,
                (bank << nandILTat->Misc.bsc4SctsPerPg),
                (NvU8 *)(nandILTat->tatHandler.wBuffer),
                //handler->WorkingCopy.headerSignature,
                hNand->NandDevInfo.SctsPerPg,
                NULL);
        }

        if (sts != NvSuccess)
        {
            // Mark page as bad
            MarkPageAsBad(
                hNand,
                hNand->ITat.pPhysPgs,
                (bank << nandILTat->Misc.bsc4SctsPerPg),
                NV_TRUE);
            continue;
        } else
        {

            LOG_NAND_DEBUG_INFO(("\r\nwritten TAT page=%d, bank=%d,block=%d",
                nandILTat->tatHandler.WorkingCopy.NextTatPgNum,
                nandILTat->tatHandler.WorkingCopy.NextTatBlkNum.BnkNum,
                nandILTat->tatHandler.WorkingCopy.NextTatBlkNum.BlkNum),
                TAT_DEBUG_INFO);
        }

        // indicate TAT as dirty, if we are writing only header
        if (writeOnlyHeader)
        {
            nandILTat->tatHandler.WorkingCopy.IsDirty = NV_TRUE;
        } else
        {

            // modify the data in working copy to point to the new page
            nandILTat->tatHandler.WorkingCopy.TatBlkNum =
                nandILTat->tatHandler.WorkingCopy.NextTatBlkNum;
            nandILTat->tatHandler.WorkingCopy.TatPgNum =
                nandILTat->tatHandler.WorkingCopy.NextTatPgNum;
            nandILTat->tatHandler.WorkingCopy.IsDirty = NV_FALSE;
        }
        break;
    }
    ReleaseVirtualMemory(tmpBuff,TAT_ALLOC);
}
#endif
/**
 * @brief This method counts the system reserved blocks and updates internal
 * variables.
 *
 */
void
CountSystemRsvdBlks(NvNandHandle hNand)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    NvS32 temp = 0;
    BlockInfo TatStBlk;
    BlockInfo tatEndBlock;
    BlockInfo ttStartBlock;
    BlockInfo ttEndBlock;

    InitBlockInfo(&TatStBlk);
    InitBlockInfo(&tatEndBlock);
    InitBlockInfo(&ttStartBlock);
    InitBlockInfo(&ttEndBlock);

    GetTATStartBlock(hNand, &TatStBlk);

    temp = (TatStBlk.BlkNum - 1) +
    AlignBlockCount(hNand, (hNand->NandCfg.BlkCfg.NoOfTatBlks /
    nandILTat->Misc.NoOfILBanks));

    SetBlockInfo(&tatEndBlock, 0,temp);
    temp++;
    SetBlockInfo(&ttStartBlock, 0,temp);

    temp = ttStartBlock.BlkNum - 1 +
        AlignBlockCount(hNand, (hNand->NandCfg.BlkCfg.NoOfTtBlks /
    nandILTat->Misc.NoOfILBanks));

    SetBlockInfo(&ttEndBlock, 0,temp);

    FindBlockCount(
        hNand, TatStBlk, tatEndBlock, &nandILTat->Misc.NumOfBlksForTAT);
    FindBlockCount(
        hNand, ttStartBlock, ttEndBlock, &nandILTat->Misc.NumOfBlksForTT);

    // update this to reflect the block count per bank
    CreateTATHandler(hNand);
    FillBlockInfo(hNand, TatStBlk, tatEndBlock,
        nandILTat->tatHandler.tatBlocks);
    FillBlockInfo(hNand, ttStartBlock, ttEndBlock,
        nandILTat->tatHandler.ttBlocks);

    *nandILTat->tatHandler.WorkingCopy.TatStBlk = TatStBlk;
    *nandILTat->tatHandler.WorkingCopy.tatEndBlock = tatEndBlock;
    *nandILTat->tatHandler.WorkingCopy.ttStartBlock = ttStartBlock;
    *nandILTat->tatHandler.WorkingCopy.ttEndBlock = ttEndBlock;

    nandILTat->Misc.TotalLogicalBlocks =
        (hNand->FtlEndPba - hNand->FtlStartPba) -
        nandILTat->Misc.ftlReservedLblocks -
        (2 + NUMBER_OF_LBAS_TO_TRACK);
}

/**
 * @brief This method checks for the presence of recent TAT.
 *
 * @param TATFoundBlock returns the block in which the latest TAT is present.
 * @param TATFoundPage returns the page number of block in which
 * the TAT is present.
 * @param falledBack tells whether we are falling back to previous
 * TAT as the previous power down was not proper.
 */
NvBool
CheckForTAT(
    NvNandHandle hNand,
    BlockInfo* TATFoundBlock,
    NvS32* TATFoundPage,
    NvBool* falledBack)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    NvS32 numberOfTATBlocks = nandILTat->Misc.NumOfBlksForTAT;
    NvS32 TATBlock = 0;
    NvBool TATFound = NV_FALSE;
    NvU32 RevNum = 0xFFFFFFFF;//-1;
    NvS8* pTempBuffer = 0;
    NvS32 bank,startPage,page;
    NvBool BlockBad = NV_TRUE;
    TagInformation *pTagInfo;
    TranslationAllocationTable* pTempTAT =
        (TranslationAllocationTable *)AllocateVirtualMemory(
            sizeof(TranslationAllocationTable),TAT_ALLOC);
    pTempBuffer =
        AllocateVirtualMemory(hNand->NandDevInfo.PageSize,TAT_ALLOC);

    InitTranslationAllocationTable(
        pTempTAT,
        nandILTat->Misc.BlksRequiredForTT,
        nandILTat->Misc.PgsRegForTT,
        hNand->NandDevInfo.SectorSize);

    CopyTranslationAllocationTable(
        hNand,
        pTempTAT,
        pTempBuffer);//*pTempTAT = pTempBuffer;
    *falledBack = NV_FALSE;

    while (numberOfTATBlocks > 0)
    {
        // clear the physical block array
        ClearPhysicalPageArray(hNand,hNand->ITat.pPhysPgs);
        bank = nandILTat->tatHandler.tatBlocks[TATBlock].BnkNum;
        startPage = SetPhysicalPageArray(
            hNand,
            hNand->ITat.pPhysPgs,
            &nandILTat->tatHandler.tatBlocks[TATBlock],
            nandILTat->Misc.TagInfoPage);

        BlockBad = IsBlockBad(hNand,
            &nandILTat->tatHandler.tatBlocks[TATBlock], NV_FALSE);
        if (BlockBad)
        {
            numberOfTATBlocks--;
            TATBlock++;
            continue;
        }

        for (page = 0; page < hNand->NandDevInfo.PgsPerBlk; page++)
        {

            // skip tag info page
/*
            if (page == nandILTat->Misc.TagInfoPage)
                continue;
*/
            bank = nandILTat->tatHandler.tatBlocks[TATBlock].BnkNum;
            hNand->ITat.pPhysPgs[bank] = startPage + page;
            /*
             * Don't check for the status. We will get ECC error here as
             * we haven't updated this area before.
             */
            ReadTagInfo(
                hNand,
                hNand->ITat.pPhysPgs,
                (bank << nandILTat->Misc.bsc4SctsPerPg),
                hNand->pSpareBuf);
            pTagInfo = (TagInformation *)(hNand->pSpareBuf +
                hNand->gs_NandDeviceInfo.TagOffset);

            // Check if the block and the page is good.
            if ((/* run-time bad block */hNand->pSpareBuf[1] ==
                RUN_TIME_GOOD_MARK) && (pTagInfo->Info.PageStatus ==
                Information_Status_PAGE_GOOD))
            {
                // read the header sector
                NandTLReadPhysicalPage(
                    hNand,
                    hNand->ITat.pPhysPgs,
                    nandILTat->tatHandler.tatBlocks[TATBlock].BnkNum <<
                        nandILTat->Misc.bsc4SctsPerPg,
                    (NvU8 *)pTempBuffer, hNand->NandDevInfo.SctsPerPg, 0,0);

                // Check if the TAT header and footer is available
                if ((NvOsStrncmp((const char *)(pTempTAT->headerSignature),
                    (const char *)(NAND_IL_TAT_HEADER),
                    sizeof(NAND_IL_TAT_HEADER)) == 0) &&
                    (NvOsStrncmp((const char *)(pTempTAT->footerSignature),
                    (const char *)(NAND_IL_TAT_FOOTER),
                    sizeof(NAND_IL_TAT_FOOTER)) == 0))
                {
                    /*
                     * We are here that means we have found the TAT with
                     * header and footer. But since we don't know if this
                     * is the last TAT in this block we need to proceed further
                     * with next page for the final and latest TAT.
                     */
                    if (TATFound)
                    {

                        if (RevNum <= *pTempTAT->RevNum)
                        {

                            RevNum = *pTempTAT->RevNum;
                            *TATFoundBlock =
                                nandILTat->tatHandler.tatBlocks[TATBlock];
                            *TATFoundPage = page;
                            nandILTat->Misc.CurrentTatLBInUse = TATBlock;
                            *falledBack = NV_FALSE;
                            LOG_NAND_DEBUG_INFO(("\r\nTATFound , revision=%d,"
                                "page=%d,bank=%d,block=%d", *pTempTAT->RevNum,
                                page, (*TATFoundBlock).BnkNum,
                                (*TATFoundBlock).BlkNum),
                                TAT_DEBUG_INFO);
                        }
                        else
                        {
                            LOG_NAND_DEBUG_INFO(("\r\nTATFound , revision=%d,"
                                "page=%d,bank=%d,block=%d", *pTempTAT->RevNum,
                                page,
                                nandILTat->tatHandler.tatBlocks[TATBlock].\
                                BnkNum,
                                nandILTat->tatHandler.tatBlocks[TATBlock].\
                                BlkNum),
                                TAT_DEBUG_INFO);
                        }
                    } else
                    {
                        RevNum = *pTempTAT->RevNum;
                        TATFound = NV_TRUE;
                        *TATFoundBlock =
                            nandILTat->tatHandler.tatBlocks[TATBlock];
                        *TATFoundPage = page;
                        nandILTat->Misc.CurrentTatLBInUse = TATBlock;
                        *falledBack = NV_FALSE;
                        LOG_NAND_DEBUG_INFO(("\r\nTATFound , revision=%d,\
                            page=%d,bank=%d,block=%d", *pTempTAT->RevNum,
                            page,
                            (*TATFoundBlock).BnkNum,
                            (*TATFoundBlock).BlkNum),
                            TAT_DEBUG_INFO);
                    }
                }
                // Check if the TAT header is available, but not the footer
                else if ((NvOsStrncmp(
                    (const char *)(pTempTAT->headerSignature),
                    (const char *)(NAND_IL_TAT_HEADER),
                    sizeof(NAND_IL_TAT_HEADER)) == 0) &&
                    (NvOsStrncmp((const char *)(pTempTAT->footerSignature),
                    (const char *)(NAND_IL_TAT_FOOTER),
                    sizeof(NAND_IL_TAT_FOOTER)) != 0))
                {
                    /*
                     * We are here that means TAT header is available but not
                     * the footer that means the TAT is corrupted. In that
                     * case declare TAT not found and set numberOfTATBlocks
                     * to zero so that it breaks from while loop and
                     * recreate the TAT and TT.
                     */
                    if ((TATFound == NV_TRUE))
                    {

                        if (*pTempTAT->RevNum > RevNum)
                            *falledBack = NV_TRUE;
                        LOG_NAND_DEBUG_INFO(("\r\nTAT Header Found , \
                             revision=%d,page=%d,bank=%d,block=%d",
                             *pTempTAT->RevNum,
                             page,
                             nandILTat->tatHandler.tatBlocks[TATBlock].BnkNum,
                             nandILTat->tatHandler.tatBlocks[TATBlock].BlkNum),
                             TAT_DEBUG_INFO);
                    }
                } else
                {
                    /*
                     * we are here that means for this page there is no valid
                     * signature available and since this is a good block and
                     * good page it is safe to assume that this block is not
                     * used and therefore continue to check from next block.
                     */
                    break;
                }
            }
        }

        numberOfTATBlocks--;
        TATBlock++;
    };

    LOG_NAND_DEBUG_INFO(("\r\n TATFound=%d, *falledBack=%d, RevNum=%d",
                         TATFound, *falledBack,RevNum), TAT_DEBUG_INFO);
    ReleaseVirtualMemory(pTempTAT,TAT_ALLOC);
    ReleaseVirtualMemory(pTempBuffer,TAT_ALLOC);
    return(TATFound);
}

/**
 * @brief This method prepares the Nand device for Format.
 *
 */
NvBool
PrepareNandDevice(NvNandHandle hNand, NvBool enableErase)
{
    /*
     * During the preparation of nand device, we should not erase BL, FW and
     * NVP blocks so skip them.
     * The BL and F/W image blocks are scattered across the interleaved banks.
     * Find out the TAT start block number.
     */
    BlockInfo TatStBlk;
    NvU32 dev = 0;
    NvS32 block;
    NvS32 PageNumber = 0;
    // variable for log2 of pages-per-block
    NvU32 Log2PgsPerBlk;
    // variable for log2 of number of physical blocks
    NvU32 Log2NoOfPhysBlks;
    LOG_NAND_DEBUG_INFO(("\r\n preparing NandDevice \r\n"), TAT_DEBUG_INFO);
    InitBlockInfo(&TatStBlk);
    GetTATStartBlock(hNand, &TatStBlk);
    // Calculate the log2 of Pages per block value
    Log2PgsPerBlk = NandUtilGetLog2(hNand->NandDevInfo.PgsPerBlk);
    // Calculate the log2 of number of physical block
    Log2NoOfPhysBlks = NandUtilGetLog2(hNand->NandDevInfo.NoOfPhysBlks);
    if (!TatStBlk.BlkNum) RETURN(NV_FALSE);
    //NandDebugUtility::OBJ().display((char *) "Formatting in progress");

    if(enableErase)
    {
        for (block = hNand->FtlStartPba; block < hNand->FtlEndPba; block++)
        {
            if(hNand->ITat.Misc.NoOfILBanks > 1)
            {
                ClearPhysicalPageArray( hNand,hNand->ITat.pPhysPgs);
                PageNumber = MACRO_MULT_POW2_LOG2NUM(
                    MACRO_MOD_LOG2NUM(block, Log2NoOfPhysBlks),
                    Log2PgsPerBlk);
                for(dev = 0; dev < hNand->ITat.Misc.NoOfILBanks;dev++)
                    hNand->ITat.pPhysPgs[dev] = PageNumber;
            }
            else
                hNand->ITat.pPhysPgs[0] =
                    block * hNand->NandDevInfo.PgsPerBlk;
            if (NandTLErase(hNand,hNand->ITat.pPhysPgs) != NvSuccess)
            {
                NV_DEBUG_PRINTF(("\r\n block marked as "
                    "badPrepareNandDeviceblk=%d", block));
            }
        }
    }

    /*
     * get the total count of system reserved blocks including
     * TAT and TT blocks
     */
    CountSystemRsvdBlks(hNand);

    LOG_NAND_DEBUG_INFO(("\r\n preparing NandDevice -end\r\n"),
        TAT_DEBUG_INFO);
    return(NV_TRUE);
}

/**
 * @brief This method gets the next TT block set to use.
 *
 * @param nextTTBlockSet pointer to next TT block set array.
 *
 */
void
GetNextTTBlockSet(NvNandHandle hNand, BlockInfo* nextTTBlockSet)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    //check we are in the total reserved blocks for TT
    NvS32 currentBlock,blockNum,ttBlock;
    BlockInfo* blkInfo;
    NvBool Status;

    for (
        currentBlock = 0;
        currentBlock < nandILTat->Misc.NumOfBlksForTT;
        currentBlock++)
    {
        blkInfo = nandILTat->tatHandler.WorkingCopy.lastUsedTTBlock;

        if ((blkInfo->BlkNum ==
            nandILTat->tatHandler.ttBlocks[currentBlock].BlkNum) &&
            (blkInfo->BnkNum ==
            nandILTat->tatHandler.ttBlocks[currentBlock].BnkNum))
        {
            /*
             * we found the last one of used TT blocks, the following
             * blocks should be free.
             */
            currentBlock++;
            break;
        }
    }

    //read redundant are to determine whether the block is good or bad
    blockNum = 0;
    for (
        ttBlock = 0;
        ttBlock < nandILTat->Misc.NumOfBlksForTT;
        ttBlock++, currentBlock++)
    {

        if (currentBlock >= nandILTat->Misc.NumOfBlksForTT)
            currentBlock = 0;

        // check whether the block is bad
        Status = IsBlockBad(hNand,
            &nandILTat->tatHandler.ttBlocks[currentBlock], NV_TRUE);
        if (Status)
        {
            continue;
        }

        // Erase the block
        if (DoAdvancedErase(
            hNand,
            &nandILTat->tatHandler.ttBlocks[currentBlock],
            NV_FALSE) != NvSuccess)
        {

            LOG_NAND_DEBUG_INFO(("\r\n Erasing acquired TT block,bank=%d"
                ",blk=%d", nextTTBlockSet[ttBlock].BnkNum,
                 nextTTBlockSet[ttBlock].BlkNum), TAT_DEBUG_INFO);
            // Erase failed, Mark it as bad and continue the operation.
            MarkBlockAsBad(hNand,
                nextTTBlockSet[ttBlock].BnkNum,
                nextTTBlockSet[ttBlock].BlkNum);
            continue;
        }

        nextTTBlockSet[blockNum++] = nandILTat->tatHandler.ttBlocks[currentBlock];
        if (blockNum >= nandILTat->tatHandler.WorkingCopy.TtBlkCnt)
            break;
    }
}

/**
 * @brief This method writes the requested page in the given block's good
 * page by checking the page status.
 *
 * @param blockInfo block info
 * @param pageNumber page number to start write operation from.
 * @param data pointer to the data.
 */
void
DoAdvancedWrite(
    NvNandHandle hNand,
    BlockInfo* blockInfo,
    NvS32 pageNumber,
    NvU8 *data)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    NvS32 bank,sts;
    for(;;)
    {
        ClearPhysicalPageArray(hNand,hNand->ITat.pPhysPgs);
        SetPhysicalPageArray(hNand, hNand->ITat.pPhysPgs,
            blockInfo, pageNumber);
        bank = blockInfo->BnkNum;
        sts = Write(
            hNand,
            hNand->ITat.pPhysPgs,
            bank << nandILTat->Misc.bsc4SctsPerPg,
            data,
            hNand->NandDevInfo.SctsPerPg,
            NULL);
        if (sts != NvSuccess)
        {
            ReplaceBlockSet(hNand);
            MarkBlockAsBad(hNand, bank,
                hNand->ITat.pPhysPgs[bank] >> hNand->ITat.Misc.bsc4PgsPerBlk);
            // Get next page and try writing into it.
            GetNextTTpage(hNand, blockInfo,&pageNumber,NV_TRUE);
            continue;
        }
        break;
    }
}

/**
 * @brief This method first reads the bad page info into a buffer and then
 * erases the block and writes
 * the bad page info back to the block
 *
 * @param blockInfo reference to block info.
 * @param isTAT tells whether the block that is to be erased a TAT block.
 *
 * @return returns the erase status.
 */
NvError
DoAdvancedErase(NvNandHandle hNand, BlockInfo* blockInfo, NvBool isTAT)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    NvError status = NvError_NandTTFailed;

    NvS8* pageStsStorage = 0;
    NvS32 startPage,pageNum;
    TagInformation *pTagInfo;
    /*
     * make a note of page status of all the pages in the block by
     * reading redundant area
     */
    LOG_NAND_DEBUG_INFO(("\r\nDoAdvancedErase for TAT/TT %d/%d \r\n",
        isTAT,!isTAT), TAT_DEBUG_INFO);

    pageStsStorage =
        AllocateVirtualMemory(hNand->NandDevInfo.PageSize,TAT_ALLOC);

        ClearPhysicalPageArray(hNand,hNand->ITat.pPhysPgs);
        startPage =
            SetPhysicalPageArray(hNand, hNand->ITat.pPhysPgs, blockInfo, 0);

        for (pageNum = 0; pageNum < hNand->NandDevInfo.PgsPerBlk; pageNum++)
        {

            hNand->ITat.pPhysPgs[blockInfo->BnkNum] = startPage+pageNum;
            hNand->ITat.NEStatus =
                ReadTagInfo(
                    hNand,
                    hNand->ITat.pPhysPgs,
                    (blockInfo->BnkNum << nandILTat->Misc.bsc4SctsPerPg),
                    hNand->pSpareBuf);
            pTagInfo = (TagInformation *)(hNand->pSpareBuf +
                hNand->gs_NandDeviceInfo.TagOffset);
            if (hNand->ITat.NEStatus == NvSuccess)
            {
                //note the page sts
                pageStsStorage[pageNum] =
                    pTagInfo->Info.PageStatus;
            } else
            {
                pageStsStorage[pageNum] = Information_Status_PAGE_BAD;
            }
        }

        ClearPhysicalPageArray(hNand,hNand->ITat.pPhysPgs);
        hNand->ITat.pPhysPgs[blockInfo->BnkNum] = startPage;
        // erase the block
        hNand->ITat.NEStatus = NandTLErase(hNand,hNand->ITat.pPhysPgs);
    if(hNand->ITat.NEStatus != NvSuccess)
    {
        ReleaseVirtualMemory(pageStsStorage,TAT_ALLOC);
        RETURN(hNand->ITat.NEStatus);
    }

        // write back the tag information along the stored page status
        for (pageNum = 0; pageNum < hNand->NandDevInfo.PgsPerBlk; pageNum++)
        {
            hNand->ITat.pPhysPgs[blockInfo->BnkNum] = startPage + pageNum;

            /*
             * write the info, only if the page in a block is bad or the page
             * is tag page
             */
            if (!pageStsStorage[pageNum])
            {
                // Initialize spare buffer to mark block bad
                NvOsMemset(hNand->pSpareBuf, 0x0,
                    hNand->gs_NandDeviceInfo.NumSpareAreaBytes);
                // Write tag info along with skipped bytes
                WriteTagInfo(
                    hNand,
                    hNand->ITat.pPhysPgs,
                    (blockInfo->BnkNum << nandILTat->Misc.bsc4SctsPerPg),
                    hNand->pSpareBuf);
                //ChkDriverErr();
            }
        }
        status = NvSuccess;
    ReleaseVirtualMemory(pageStsStorage,TAT_ALLOC);
    return(status);
}

/**
 * @brief This method gets the next logical TAT block to use.
 *
 * @param prevLogicalBlock previously used logical block.
 * @param newLogicalBlock returns the new logical block number to use.
 */
void
GetNextTATBlock(
    NvNandHandle hNand,
    NvS32 currentLogicalBlock,
    NvS32* newLogicalBlock)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    NvS32 count = nandILTat->Misc.NumOfBlksForTAT;
    NvBool BlockBad = NV_TRUE;
    *newLogicalBlock = currentLogicalBlock;
    LOG_NAND_DEBUG_INFO(("\r\n Getting Next TAT block"), TAT_DEBUG_INFO);
    while (count)
    {

        // go to the next set of TAT blocks
        currentLogicalBlock++;

        if (currentLogicalBlock >= nandILTat->Misc.NumOfBlksForTAT)
            currentLogicalBlock = 0;


        BlockBad = IsBlockBad(hNand,
            &nandILTat->tatHandler.tatBlocks[currentLogicalBlock], NV_FALSE);

        if (!BlockBad)
        {

            if (DoAdvancedErase(
                hNand,
                &nandILTat->tatHandler.tatBlocks[currentLogicalBlock],
                NV_TRUE) == NvSuccess)
            {

                *newLogicalBlock = currentLogicalBlock;
                break;
            }
            else
            {

                MarkBlockAsBad(hNand,
                    nandILTat->tatHandler.tatBlocks[currentLogicalBlock].BnkNum
                    ,
                    nandILTat->tatHandler.tatBlocks[currentLogicalBlock].BlkNum
                    );
            }
        }
        count--;
    }
    LOG_NAND_DEBUG_INFO((" curr l TAT blk=%d, New l TAT Block=%d",
        currentLogicalBlock, *newLogicalBlock), TAT_DEBUG_INFO);
}

/**
 * @brief Aligns the block count to the IL bank boundary
 *
 */
NvS32
AlignBlockCount(NvNandHandle hNand, NvS32 totalBlocks)
{
    return( ( (totalBlocks + hNand->ITat.Misc.NoOfILBanks - 1) >>
        hNand->ITat.Misc.bsc4NoOfILBanks ) <<
            hNand->ITat.Misc.bsc4NoOfILBanks );
}

/**
 * @brief This method gives the number of bad pages present in the block.
 *
 * @param block block info for which the bad page count is needed.
 *
 * @return returns the number of bad pages that are present in the block.
 */
NvS32
GetBadPageCount(NvNandHandle hNand, BlockInfo* block)
{

    NvS32 badPageCount = 0;
    NvS32 startPage,pageNum;
    /*
     * make a note of page status of all the pages in the block by
     * reading redundant area
     */
    LOG_NAND_DEBUG_INFO(("\r\n getBagPageCount for bank=%d,block=%d",
                         block->BnkNum,block->BlkNum), TAT_DEBUG_INFO);

    ClearPhysicalPageArray(hNand,hNand->ITat.pPhysPgs);
    startPage = SetPhysicalPageArray(hNand, hNand->ITat.pPhysPgs, block, 0);;

    for (pageNum = 0; pageNum < hNand->NandDevInfo.PgsPerBlk; pageNum++)
    {
        TagInformation *pTagInfo;

        hNand->ITat.pPhysPgs[block->BnkNum] = startPage + pageNum;
        ReadTagInfo(
            hNand,
            hNand->ITat.pPhysPgs,
            (block->BnkNum << hNand->ITat.Misc.bsc4SctsPerPg),
            hNand->pSpareBuf);
        pTagInfo = (TagInformation *)(hNand->pSpareBuf +
            hNand->gs_NandDeviceInfo.TagOffset);
        //ChkDriverErr();
        // check the page sts
        if (pTagInfo->Info.PageStatus ==
        Information_Status_PAGE_BAD)
            badPageCount++;
    }
    LOG_NAND_DEBUG_INFO(("bad Page Count=%d", badPageCount), TAT_DEBUG_INFO);
    return(badPageCount);
}

/**
 * @brief This method gives the entry size in a Translation table.
 *
 */
/**
 * @brief This method sets the physical page array with the page
 * number calculated from the block and page offset info.
 *
 * @param blockInfo
 * @param pageOffset
 *
 * @return returns the start page number of the block.
 */
NvS32
SetPhysicalPageArray(
    NvNandHandle hNand,
    NvS32* pPageArray,
    BlockInfo* blockInfo,
    NvS32 pageOffset)
{
    NvS32 startPage = blockInfo->BlkNum << hNand->ITat.Misc.bsc4PgsPerBlk;
    pPageArray[blockInfo->BnkNum] = startPage + pageOffset;
    return(startPage);
}

/**
 * @brief This method finds the number of blocks present between the
 * start and end blocks.
 *
 * @param startBlock start block info
 * @param endBlock end block info
 * @param count returns the block count.
 */
void
FindBlockCount(
    NvNandHandle hNand,
    BlockInfo startBlock,
    BlockInfo endBlock,
    NvS32* count)
{
    NvS32 i;
    *count = 0;
    // check whether both blocks are same row.
    if (startBlock.BlkNum == endBlock.BlkNum)
    {
        if (startBlock.BnkNum <= endBlock.BnkNum)
        {
            *count = endBlock.BnkNum - startBlock.BnkNum + 1;
        }
        return;
    }

    // count the number blocks in the start row
    for (i = 0; i < (NvS32)hNand->ITat.Misc.NoOfILBanks; i++)
    {

        if (i >= startBlock.BnkNum)
        {
            *count = *count + 1;
        }
    }
    startBlock.BnkNum = 0;
    startBlock.BlkNum++;

    // count the number blocks in the end row
    for (i = 0; i < (NvS32)hNand->ITat.Misc.NoOfILBanks; i++)
    {

        if (i <= endBlock.BnkNum)
        {
            *count = *count + 1;
        }
    }
    endBlock.BnkNum = 0;
    endBlock.BlkNum--;

    // count the number blocks in the middle rows
    if (startBlock.BlkNum <= endBlock.BlkNum)
    {

        *count += ((endBlock.BlkNum - startBlock.BlkNum + 1) <<
            hNand->ITat.Misc.bsc4NoOfILBanks);
    }
}

/**
 * @brief This method fills the info of the blocks present between the start
 * and end blocks into the blkinfo array.
 *
 * @param startBlock start block info
 * @param endBlock end block info
 * @param blkInfo returns the block info all the blocks between start
 * and end blocks.
 */
void
FillBlockInfo(
    NvNandHandle hNand,
    BlockInfo startBlock,
    BlockInfo endBlock,
    BlockInfo* blkInfo)
{
    NvS32 i;
    NvU32 j;
    NvS32 count = endBlock.BlkNum - startBlock.BlkNum + 1;
    for (i = 0; i < count; i++)
    {

        for (j = 0; j < hNand->ITat.Misc.NoOfILBanks; j++)
        {

            SetBlockInfo(
                &blkInfo[ (i << hNand->ITat.Misc.bsc4NoOfILBanks) + j],
                j,
                i + startBlock.BlkNum);
        }
    }
}

/**
 * @brief This method gets the tat start block info
 *
 * @param TatStBlk returns the tat start block
 */
void
GetTATStartBlock(NvNandHandle hNand, BlockInfo* TatStBlk)
{
    NvS32 blk = hNand->FtlEndPba -
        hNand->ITat.Misc.tatLogicalBlocks -
        hNand->ITat.Misc.ttLogicalBlocks;
    SetBlockInfo(TatStBlk, 0,blk);
}


/**
 * @brief This method check for the valid TAT page and get the blocks
 * allocated for TAT, TT and NVP.
 *
 * @return returns NV_TRUE, if it is successful in getting the info.
 */
NvBool
GetBlockInfoFromTAT(NvNandHandle hNand)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    BlockInfo TatStBlk;

    NvS32 numberOfTATBlocks;
    NvS32 rsvdBlocks;
    NvBool breakLoop = NV_FALSE;
    NvS8* pTempBuffer = 0;
    NvS32 startPage;
    NvS32 page;
    TranslationAllocationTable* pTempTAT = 0;
    TagInformation *pTagInfo;

    if (hNand->PercentReserved > FTL_DATA_PERCENT_RSVD_MAX)
    {
        NvOsDebugPrintf("Invalid percent reserved value = %d, should not exceed"
             "%d, setting it to %d",hNand->PercentReserved,
            FTL_DATA_PERCENT_RSVD_MAX, FTL_DATA_PERCENT_RSVD_MAX);
            // if percent reserved value exceeds max limit cap it to max limit
            hNand->PercentReserved = FTL_DATA_PERCENT_RSVD_MAX;
    }

    pTempTAT =
        (TranslationAllocationTable*)AllocateVirtualMemory(
            sizeof(TranslationAllocationTable),TAT_ALLOC);
    pTempBuffer =
        AllocateVirtualMemory(hNand->NandDevInfo.PageSize,TAT_ALLOC);

    hNand->ITat.Misc.tatLogicalBlocks =
        AlignBlockCount(hNand, hNand->NandCfg.BlkCfg.NoOfTatBlks) /
        nandILTat->Misc.NoOfILBanks;
    nandILTat->Misc.ttLogicalBlocks =
        AlignBlockCount(hNand, hNand->NandCfg.BlkCfg.NoOfTtBlks) /
        nandILTat->Misc.NoOfILBanks;
    /*
     * total data reserved blocks is 2.5% of the blocks that are being
     * managed by this driver
     */
    rsvdBlocks = hNand->FtlEndPba - hNand->FtlStartPba;
    rsvdBlocks = rsvdBlocks - (nandILTat->Misc.tatLogicalBlocks +
                                            nandILTat->Misc.ttLogicalBlocks);
    nandILTat->Misc.bbmwlDataRservedBlocks =
                                (rsvdBlocks * hNand->PercentReserved) / 100;
    nandILTat->Misc.bbmwlSystemRservedBlocks =
        nandILTat->Misc.tatLogicalBlocks +
        nandILTat->Misc.ttLogicalBlocks;
     nandILTat->Misc.ftlReservedLblocks =
     nandILTat->Misc.bbmwlSystemRservedBlocks +
     nandILTat->Misc.bbmwlDataRservedBlocks;

    ERR_DEBUG_INFO(("****************************************************"));
    ERR_DEBUG_INFO(("  FTL_start %d, End %d, FTL RSVD %d, dataRsvd %d, "
        "InterleaveCount %d \r\n", hNand->FtlStartPba, hNand->FtlEndPba,
                                nandILTat->Misc.ftlReservedLblocks,
                                nandILTat->Misc.bbmwlDataRservedBlocks,
                                nandILTat->Misc.NoOfILBanks));
    ERR_DEBUG_INFO((("CATEGORY 2 %d - %d; CATEGORY 3 %d - %d \r\n"),
        hNand->FtlStartPba,
        hNand->FtlEndPba - nandILTat->Misc.ftlReservedLblocks - 1,
        hNand->FtlEndPba -nandILTat->Misc.ftlReservedLblocks,
        hNand->FtlEndPba));
    ERR_DEBUG_INFO(("****************************************************"));


    numberOfTATBlocks = nandILTat->Misc.tatLogicalBlocks;
    GetTATStartBlock(hNand, &TatStBlk);
    InitTranslationAllocationTable(
        pTempTAT,
        nandILTat->Misc.BlksRequiredForTT,
        nandILTat->Misc.PgsRegForTT,
        hNand->NandDevInfo.SectorSize);
    CopyTranslationAllocationTable(hNand,pTempTAT, pTempBuffer);

    while (numberOfTATBlocks)
    {

        // clear the physical block array
        ClearPhysicalPageArray(hNand,hNand->ITat.pPhysPgs);
        startPage = SetPhysicalPageArray(
            hNand,
            hNand->ITat.pPhysPgs,
            &TatStBlk,
            nandILTat->Misc.TagInfoPage);

        /*
         * Check whether the block is good/bad. Don't check for the status.
         * we will get ECC error here as we haven't updated this area before.
         */
        ReadTagInfo(
            hNand,
            hNand->ITat.pPhysPgs,
            (TatStBlk.BnkNum << nandILTat->Misc.bsc4SctsPerPg),
            hNand->pSpareBuf);

        pTagInfo = (TagInformation *)(hNand->pSpareBuf +
            hNand->gs_NandDeviceInfo.TagOffset);

        // run-time bad block
        if (hNand->pSpareBuf[1] != RUN_TIME_GOOD_MARK)
        {

            TatStBlk.BnkNum++;
            if (TatStBlk.BnkNum >= (NvS32)nandILTat->Misc.NoOfILBanks)
            {
                TatStBlk.BnkNum = 0;
                TatStBlk.BlkNum++;
            }
            if (TatStBlk.BlkNum > (NvS32)nandILTat->Misc.TotEraseBlks)
            {
                ReleaseVirtualMemory( pTempTAT, TAT_ALLOC);
                RETURN(NV_FALSE);
            }
            continue;
        }

        for (page = 0; page < hNand->NandDevInfo.PgsPerBlk; page++)
        {

            // skip tag info page
/*
            if (page == nandILTat->Misc.TagInfoPage)
                continue;
*/
            hNand->ITat.pPhysPgs[TatStBlk.BnkNum] = startPage + page;
            /*
             * Don't check for the status. we will get ECC error here as
             * we haven't updated this area before.
             */
            ReadTagInfo(
                hNand,
                hNand->ITat.pPhysPgs,
                (TatStBlk.BnkNum << nandILTat->Misc.bsc4SctsPerPg),
                hNand->pSpareBuf);

            pTagInfo = (TagInformation *)(hNand->pSpareBuf +
                hNand->gs_NandDeviceInfo.TagOffset);
            // Check if the block and the page is good.
            if ((hNand->pSpareBuf[1] == RUN_TIME_GOOD_MARK) &&
            (pTagInfo->Info.PageStatus == Information_Status_PAGE_GOOD))
            {

                // read the header sector
                NandTLReadPhysicalPage(
                    hNand,
                    hNand->ITat.pPhysPgs,
                    TatStBlk.BnkNum << nandILTat->Misc.bsc4SctsPerPg,
                    (NvU8 *)pTempBuffer, hNand->NandDevInfo.SctsPerPg, 0,0);

                // Check if the TAT header and footer is available
                if ((NvOsStrncmp((const char *)(pTempTAT->headerSignature),
                (const char *)(NAND_IL_TAT_HEADER),
                sizeof(NAND_IL_TAT_HEADER)) == 0) &&
                (NvOsStrncmp((const char *)(pTempTAT->footerSignature),
                (const char *)(NAND_IL_TAT_FOOTER),
                sizeof(NAND_IL_TAT_FOOTER)) == 0))
                {
                    /*
                     * We are here that means we have found the TAT with
                     * header and footer. we can get the TAT, TT and NVP
                     * block info from this page.
                     */
                    breakLoop = NV_TRUE;
                    break;
                }
                // Check if the TAT header is available, but not the footer
                else if ((NvOsStrncmp(
                    (const char *)(pTempTAT->headerSignature),
                (const char *)(NAND_IL_TAT_HEADER),
                sizeof(NAND_IL_TAT_HEADER)) == 0) &&
                (NvOsStrncmp((const char *)(pTempTAT->footerSignature),
                (const char *)(NAND_IL_TAT_FOOTER),
                sizeof(NAND_IL_TAT_FOOTER)) != 0))
                {
                    /*
                     * We are here that means TAT header is available but
                     * not the footer.
                     * we can get the TAT, TT and NVP block info
                     * from this page.
                     */
                }
                else
                {
                    /*
                     * we are here that means for this page there is no valid
                     * signature available and since this is a good block and
                     * good page it is safe to assume that this block is not
                     * and therefore continue to check from next block.
                     */
                    break;
                }
            }
        }

        if (breakLoop)
            break;

        numberOfTATBlocks--;
        TatStBlk.BnkNum++;
        if (TatStBlk.BnkNum >= (NvS32)nandILTat->Misc.NoOfILBanks)
        {
            TatStBlk.BnkNum = 0;
            TatStBlk.BlkNum++;
        }
    };

    if (breakLoop)
    {

        FindBlockCount(
            hNand,
            *pTempTAT->TatStBlk,
            *pTempTAT->tatEndBlock,
            &nandILTat->Misc.NumOfBlksForTAT);
        FindBlockCount(
            hNand,
            *pTempTAT->ttStartBlock,
            *pTempTAT->ttEndBlock,
            &nandILTat->Misc.NumOfBlksForTT);

        // update this to reflect the block count per bank
        CreateTATHandler(hNand);
        FillBlockInfo(
            hNand,
            *pTempTAT->TatStBlk,
            *pTempTAT->tatEndBlock,
            nandILTat->tatHandler.tatBlocks);
        FillBlockInfo(
            hNand,
            *pTempTAT->ttStartBlock,
            *pTempTAT->ttEndBlock,
            nandILTat->tatHandler.ttBlocks);

        /*
         * sysRsvdBlocksInFirstZone should have calculated by this time.
         * Find out the logical blocks in first zone
         */
        nandILTat->Misc.TotalLogicalBlocks =
            (hNand->FtlEndPba - hNand->FtlStartPba) -
            nandILTat->Misc.ftlReservedLblocks -
            (2 + NUMBER_OF_LBAS_TO_TRACK);
    }

    ReleaseVirtualMemory(pTempTAT, TAT_ALLOC);
    ReleaseVirtualMemory(pTempBuffer, TAT_ALLOC);
    return(breakLoop);
}

/**
 * @brief this method creates the TAT handler
 *
 */
void
CreateTATHandler(NvNandHandle hNand)
{
    NvS32 i;
    NvU64 NumBlocks;
    NvU64 NumBlocksILBanks;

    NandInterleaveTAT *nandILTat = &hNand->ITat;
    if (nandILTat->tatHandletInitialized == NV_FALSE)
    {
        InitTatHandler(
            &nandILTat->tatHandler,
            nandILTat->Misc.BlksRequiredForTT,
            nandILTat->Misc.PgsRegForTT,
            hNand->NandDevInfo.SectorSize);//uncached
        // allocate memory to our TAT buffers
        nandILTat->tatHandler.wBuffer =
            AllocateVirtualMemory(hNand->NandDevInfo.PageSize,TAT_ALLOC);
        nandILTat->tatHandler.tatBlocks =
            (BlockInfo *)AllocateVirtualMemory(
                sizeof(BlockInfo) * nandILTat->Misc.NumOfBlksForTAT *
                nandILTat->Misc.NoOfILBanks,
                TAT_ALLOC);//uncached
                NumBlocks = sizeof(BlockInfo) * nandILTat->Misc.NumOfBlksForTT;
                NumBlocksILBanks= ((NvU32) NumBlocks)*nandILTat->Misc.NoOfILBanks;
        nandILTat->tatHandler.ttBlocks =
            (BlockInfo *)AllocateVirtualMemory((NvU32)NumBlocksILBanks,
                TAT_ALLOC);//uncached

        for (i = 0; i < nandILTat->Misc.NumOfBlksForTAT; i++)
        {
            InitBlockInfo(&nandILTat->tatHandler.tatBlocks[i]);
        }
        for (i = 0; i < nandILTat->Misc.NumOfBlksForTAT; i++)
        {
            InitBlockInfo(&nandILTat->tatHandler.ttBlocks[i]);
        }
        CopyTranslationAllocationTable(
            hNand,
            &nandILTat->tatHandler.WorkingCopy,
            nandILTat->tatHandler.wBuffer);

        nandILTat->tatHandletInitialized = NV_TRUE;
    }
}


/**
 * @brief This method tells whether the block is bad.
 *
 * @param block block for which the status is needed.
 * @param rsvdBlk tells whether the block is reserved block. if it false,
 * it checks only the tag info area. if it is NV_TRUE, then it also checks for
 * the bad page count and tells the block as bad, if the bad page count
 * is more than 50%. Default value is false
 *
 * @return returns NV_TRUE if the block is bad.
 */
NvBool
IsBlockBad(
    NvNandHandle hNand,
    BlockInfo* block,
    NvBool rsvdBlk)
{
    NvBool blkBad = NV_FALSE;
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    NvError RetStatus = NvSuccess;
    NandBlockInfo DdkBlockInfo;

    NV_ASSERT(hNand->pSpareBuf);

    // initialize tag buffer
    DdkBlockInfo.pTagBuffer = hNand->pSpareBuf;
    // Initialize the spare buffer before calling GetBlockInfo API
    NvOsMemset(DdkBlockInfo.pTagBuffer, 0x0,
        hNand->gs_NandDeviceInfo.NumSpareAreaBytes);
    // Need skipped bytes along with tag information
    RetStatus = NandTLGetBlockInfo(hNand, block, &DdkBlockInfo, NV_TRUE);
    if (RetStatus != NvSuccess)
    {
        blkBad = NV_TRUE;
        goto errorExit;
    }

    // hNand.ITAT.TagInfo needs to be initialized as it
    // seems to be used after calls to IsBlockBad
    if (!DdkBlockInfo.IsFactoryGoodBlock)
    {
    #if defined(LISTBADBLOCKS)
        NvOsDebugPrintf("\nFactory Bad Block in Bank=%d,Block=%d ",
            block->BnkNum, block->BlkNum);
    #endif
        blkBad= NV_TRUE;
        goto errorExit;
    }
    // run-time bad block check
    if (hNand->pSpareBuf[1] != RUN_TIME_GOOD_MARK)
    {
        #if defined(LISTBADBLOCKS)
        NvOsDebugPrintf("\nRun Time Bad Block in bank=%d,Block=%d ",
            block->BnkNum, block->BlkNum);
        #endif
        blkBad= NV_TRUE;
        goto errorExit;
    }

    if (rsvdBlk)
    {
        if (GetBadPageCount(hNand, block) >=
            (hNand->NandDevInfo.PgsPerBlk >> 1))
        {
            // Mark the block as bad
            NvOsMemset(hNand->pSpareBuf, 0x0,
                hNand->gs_NandDeviceInfo.NumSpareAreaBytes);
            WriteTagInfo(
                hNand,
                hNand->ITat.pPhysPgs,
                (block->BnkNum << nandILTat->Misc.bsc4SctsPerPg),
                hNand->pSpareBuf);
            blkBad= NV_TRUE;
            goto errorExit;
        }
    }
errorExit:
#if defined(LISTBADBLOCKS)
        NvOsDebugPrintf("\r\n badblock present in Chip = %d Block = %d",
            block->BnkNum, block->BlkNum);
#endif
    return(blkBad);
}


/**
 * @brief This method tells whether the data in the buffer is all oxFF or not.
 *
 * @param pData pointer to data buffer.
 * @param length length of data buffer.
 *
 * @return returns NV_TRUE, if the data buffer has all the data as 0xFF.
 */
NvBool
IsDefaultData(NvU8* pData, NvS32 length)
{

    NvBool retValue = NV_TRUE;
    NvS32 i;
    for (i = 0; i < length; i++)
    {
        if (pData[i] != 0xff)
        {
            retValue = NV_FALSE;
            break;
        }
    }

    return(retValue);
}

/**
 * @brief This method marks the specified page as bad.
 *
 * @param pPages pointer to the physical page array.
 * @param sectorOffset sector offset.
 * @param isTAT if NV_TRUE indicates that we are writing a TAT block,
 * else we are writing a TT block.
 */
void
MarkPageAsBad(
    NvNandHandle hNand,
    NvS32* pPages,
    NvS32 sectorOffset,
    NvBool isTAT)
{

    // Fill the tag information
    // Initialize spare buffer before tag write
    NvOsMemset(hNand->pSpareBuf, 0x0,
        hNand->gs_NandDeviceInfo.NumSpareAreaBytes);
    // don't check the status. For a bad page the write may fail.
    WriteTagInfo(hNand,pPages, sectorOffset, hNand->pSpareBuf);
}

/**
 * @brief This Method writes the data to Nand and reads back and
 * validates the data.
 *
 * @param pPageNumbers this will point to an array of page numbers to write.
 * @param pPageOldNumbers this will point to an array of page number
 * which holds the old page numbers.
 * @param sectorOffset The sector number w.r.t the page(s), from which
 * write operation should start.
 * @param pData The buffer for writing the data.
 * @param   sectorCount The number of sectors to write.
 * @param   pTagBuffer Holds the pointer to tag info buffer.
 *
 * @return returns the status of operation.
 */
NvError
Write(
    NvNandHandle hNand,
    NvS32* pPageNumbers,
    NvU32 sectorOffset,
    NvU8 *pData,
    NvU32 sectorCount,
    NvU8* pTagBuffer)
{
    NvError Status = NvSuccess;

    Status = NandTLWritePhysicalPage(
        hNand,
        pPageNumbers,
        sectorOffset,
        pData,
        sectorCount,
        pTagBuffer);
    if (Status == NvSuccess)
    {
        Status = NandTLReadPhysicalPage(
            hNand,
            pPageNumbers,
            sectorOffset,
            hNand->TmpPgBuf,
            sectorCount,
            pTagBuffer, 0);
        if (Status == NvSuccess)
        {
            return(NvSuccess);
        }
    }
    LOG_NAND_DEBUG_INFO(("\r\n TT page Write failed"), ERROR_DEBUG_INFO);
    return(Status);
}

/**
 * @brief This method dumps the Misc and Tat handler info to the logger
 * for debug info.
 *
 */
void
DumpMiscAndTATHandlerInfoToLogger(NvNandHandle hNand)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    NvS32 i;
    LOG_NAND_DEBUG_INFO(("\r\n Misc start"), TAT_DEBUG_INFO);

    LOG_NAND_DEBUG_INFO(("\r\n NumOfBanksOnBoard = %d",
        nandILTat->Misc.NumOfBanksOnBoard), TAT_DEBUG_INFO);
    LOG_NAND_DEBUG_INFO(("\r\n NoOfILBanks = %d",
        nandILTat->Misc.NoOfILBanks), TAT_DEBUG_INFO);
    LOG_NAND_DEBUG_INFO(("\r\n PhysBlksPerBank = %d",
        nandILTat->Misc.PhysBlksPerBank), TAT_DEBUG_INFO);
    LOG_NAND_DEBUG_INFO(("\r\n ZonesPerBank = %d",
        nandILTat->Misc.ZonesPerBank), TAT_DEBUG_INFO);
    LOG_NAND_DEBUG_INFO(("\r\n PhysBlksPerZone = %d",
        nandILTat->Misc.PhysBlksPerZone), TAT_DEBUG_INFO);
    LOG_NAND_DEBUG_INFO(("\r\n PhysBlksPerLogicalBlock = %d",
        nandILTat->Misc.PhysBlksPerLogicalBlock), TAT_DEBUG_INFO);
    LOG_NAND_DEBUG_INFO(("\r\n TotalLogicalBlocks = %d",
        nandILTat->Misc.TotalLogicalBlocks), TAT_DEBUG_INFO);
    LOG_NAND_DEBUG_INFO(("\r\n TotEraseBlks = %d",
        nandILTat->Misc.TotEraseBlks), TAT_DEBUG_INFO);
    LOG_NAND_DEBUG_INFO(("\r\n NumOfBlksForTT = %d",
        nandILTat->Misc.NumOfBlksForTT), TAT_DEBUG_INFO);
    LOG_NAND_DEBUG_INFO(("\r\n PgsRegForTT = %d",
        nandILTat->Misc.PgsRegForTT), TAT_DEBUG_INFO);
    LOG_NAND_DEBUG_INFO(("\r\n TtPagesRequiredPerZone = %d",
        nandILTat->Misc.TtPagesRequiredPerZone), TAT_DEBUG_INFO);
    LOG_NAND_DEBUG_INFO(("\r\n NumOfBlksForTAT = %d",
        nandILTat->Misc.NumOfBlksForTAT), TAT_DEBUG_INFO);
    LOG_NAND_DEBUG_INFO(("\r\n BlksRequiredForTT = %d",
        nandILTat->Misc.BlksRequiredForTT), TAT_DEBUG_INFO);
    LOG_NAND_DEBUG_INFO(("\r\n PgsAlloctdForTT = %d",
        nandILTat->Misc.PgsAlloctdForTT), TAT_DEBUG_INFO);
    LOG_NAND_DEBUG_INFO(("\r\n ExtraPagesForTTMgmt = %d",
        nandILTat->Misc.ExtraPagesForTTMgmt), TAT_DEBUG_INFO);
    LOG_NAND_DEBUG_INFO(("\r\n LastTTPageUsed = %d",
        nandILTat->Misc.LastTTPageUsed), TAT_DEBUG_INFO);
    LOG_NAND_DEBUG_INFO(("\r\n CurrentTatLBInUse = %d",
        nandILTat->Misc.CurrentTatLBInUse), TAT_DEBUG_INFO);
    LOG_NAND_DEBUG_INFO(("\r\n bsc4PgsPerBlk = %d",
        nandILTat->Misc.bsc4PgsPerBlk), TAT_DEBUG_INFO);

    LOG_NAND_DEBUG_INFO(("\r\n Misc end"), TAT_DEBUG_INFO);

    LOG_NAND_DEBUG_INFO(("\r\n TAT Handler start"), TAT_DEBUG_INFO);

    for (i = 0; i < nandILTat->Misc.NumOfBlksForTAT; i++)
    {

        LOG_NAND_DEBUG_INFO(("\r\n tatBlocks[%d] bank = %d, block = %d",
            i,
            nandILTat->tatHandler.tatBlocks[i].BnkNum,
            nandILTat->tatHandler.tatBlocks[i].BlkNum),
            TAT_DEBUG_INFO);
    }

    for (i = 0; i < nandILTat->Misc.NumOfBlksForTT; i++)
    {

        LOG_NAND_DEBUG_INFO(("\r\n ttBlocks[%d] bank = %d, block = %d",
            i,
            nandILTat->tatHandler.ttBlocks[i].BnkNum,
            nandILTat->tatHandler.ttBlocks[i].BlkNum), TAT_DEBUG_INFO);
    }

    LOG_NAND_DEBUG_INFO(("\r\n tat Block bank = %d, block = %d",
        nandILTat->tatHandler.WorkingCopy.TatBlkNum.BnkNum,
        nandILTat->tatHandler.WorkingCopy.TatBlkNum.BlkNum), TAT_DEBUG_INFO);
    for (i = 0; i < nandILTat->tatHandler.WorkingCopy.TtBlkCnt; i++)
    {
        LOG_NAND_DEBUG_INFO(("\r\n TtAllocBlk[%d] bank = %d, block = %d",
            i, nandILTat->tatHandler.WorkingCopy.TtAllocBlk[i].BnkNum,
            nandILTat->tatHandler.WorkingCopy.TtAllocBlk[i].BlkNum),
            TAT_DEBUG_INFO);
    }

    LOG_NAND_DEBUG_INFO(("\r\n lastUsedTTBlock bank = %d, block = %d",
        nandILTat->tatHandler.WorkingCopy.lastUsedTTBlock->BnkNum,
        nandILTat->tatHandler.WorkingCopy.lastUsedTTBlock->BlkNum),
        TAT_DEBUG_INFO);

    LOG_NAND_DEBUG_INFO(("\r\n TAT Handler end"), TAT_DEBUG_INFO);
}


void
InitTranslationAllocationTable(
    TranslationAllocationTable*transAllocTable,
    NvS32 bCount,
    NvS32 pCount,
    NvS32 sectorLength)
{
    NvOsMemset(transAllocTable,0,sizeof(TranslationAllocationTable));
    transAllocTable->TtBlkCnt = bCount;
    transAllocTable->TtPageCnt = pCount;
    transAllocTable->TatPgNum = -1;
    transAllocTable->NextTatPgNum = -1;
    transAllocTable->IsDirty = NV_FALSE;
    transAllocTable->SectorSize = sectorLength;
}

void
CopyTranslationAllocationTable(
    NvNandHandle hNand,
    TranslationAllocationTable *newTable,
    NvS8* addr)
{
    //This header signature will be residing in 0th sector of the TAT page
    newTable->headerSignature = addr;
    // make the revision number address as int aligned.
    newTable->RevNum =
        (NvU32*)( addr+ (NIT_HEADER_LENGTH + sizeof(NvS32) -
        (NIT_HEADER_LENGTH % sizeof(NvS32))) );
    newTable->TatStBlk = (BlockInfo*)(newTable->RevNum + 1);
    newTable->tatEndBlock = newTable->TatStBlk + 1;
    newTable->ttStartBlock = newTable->tatEndBlock + 1;
    newTable->ttEndBlock = newTable->ttStartBlock + 1;
    //This page entries will be residing in 1st and 2nd sectors of the TAT page
    newTable->TtAllocPg = (NvU16*) (addr + newTable->SectorSize);
    /*
     * Current TT block entires will reside in last/3rd sector of the TAT page
     * and it will be at the start of 3rd sector
     */
    newTable->TtAllocBlk = (BlockInfo*) (addr + newTable->SectorSize * 3);
    /*
     * Last physical block used information will reside in 3rd sector of TAT
     * page and will follow allocationBlock
     */
    newTable->lastPhysicalBlockUsed =
        (NvS32*) ((newTable->TtAllocBlk) + newTable->TtBlkCnt);
    // Last used logical TT block
    newTable->lastUsedTTBlock =
        (BlockInfo*) (newTable->lastPhysicalBlockUsed + hNand->InterleaveCount);
    /*
     * footerSignature information will reside in 3rd sector of TAT page
     * and will follow lastUsedTTBlock.
     */
    newTable->footerSignature = (NvS8*) (newTable->lastUsedTTBlock + 1);

    // check the ending address whether it is with in page limit.
    if ((newTable->footerSignature + NIT_FOOTER_LENGTH) >=
        (addr + 4*newTable->SectorSize))
        RETURN_VOID;
}

void
InitTatHandler(
    TatHandler *tatHandler,
    NvS32 bCount,
    NvS32 pCount,
    NvS32 sectorLength)
{
    tatHandler->tatBlocks = 0;
    tatHandler->ttBlocks = 0;
    InitTranslationAllocationTable(
        &tatHandler->WorkingCopy, bCount,pCount,sectorLength);
    tatHandler->wBuffer = 0;
}

// Function to create translation table
NvError CreateTranslationTable(NvNandHandle hNand)
{
    /*
     * the Translation table gives the information about the logical to
     * physical block mapping and as well as the physical block
     * health(good/bad). In the interleave mode, each logical block is
     * associated to 'x' number of physical blocks. Here 'x' is the number of
     * interleaved banks. Translation table contains as many
     * 'BlockStatusEntry' as the total number of physical blocks
     * that are present in all the banks.
     */

    /*
     * erase all the TAT and TT pages before erasing a block note
     * all the bad pages in the block and then erase
     * retain the bad pages by remarking them and also mark the blocks as
     * system reserved and TT or TAT depend upon the block. This is done by
     * DoAdvancedErase().
     */
    // The following are the TAT blocks
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    NvError rtnSts = NvSuccess;
    NvS32 tatBlock,ttBlock,ttPage;
    NvS32 blkStEntryPerTTPg; // Block Status Entry Per TT Page
    NvS32 physBlk;
    NvU32 bank;
    NvS32 ttLogicalBlkNum;
    NvS32  ttLogicalPageNumber;
    NvS32 currentTTBlock;
    NvS32 currentTTPage;
    NvS32 badPageCount;
    NvS32 startBlkNum,i;
    NvS32 startBlkNum_Mirror;
    NvS32 endBlkNum,entryNum,currentIndex;

    NvS32 ttPageStartEntryNumber;
    NvS32 ttPageEndEntryNumber;
    BlockStatusEntry *transTab = 0;
    BlockInfo* nextTTBlockSet = 0;
    BlockInfo blockInfo;
    NvBool blockIsBad = NV_FALSE;
    NvS32 totaltatblocks = 0;
    NvS32 totalttblocks = 0;
    NvS32 *pDataRsvdCount ;
    NvU64 RsvdDataCount;
#if NV_ENABLE_DEBUG_PRINTS
    NvU32 BadBlockCnt = 0;
#endif
    NvU32 IndexInPg;
    // variable to store log2 blkStEntryPerTTPg
    NvU32 Log2BlkStEntryPerTTPg;

    /*  Implementation of TT create optimization is based on
     * assumption that the TT is being created after nvflash and
     * the TAT is clean - indicating there is no sudden power down.
     *
     *  After nvflash when TT is being created the logical block number
     * for a block written with data is never more than the
     * physical block number. In case of WM where the USR and OS
     * flash.dio are in the same partitions, we are assuming the
     * flash.dio was written starting lower block addresses and is contiguous.
     *
     *  Getting first block with 0xFF in tag written indicates we
     * reached end of the DATA/USR area in the TT page.
     * FIXME: Do we need to check for more than 1 blocks with LBA as -1 ?
     */

    /*
     * Count of blocks for which Physical info of corresponding TT block entry
     * is already initialized in current TT page
     */
    NvU32 EntryCountInPage;
    // Flag to indicate if we found clean block
    NvBool FoundCleanChunk = NV_FALSE;
    // Flag to indicate completion of update logical information
    // of all blocks in the TT page
    NvBool FinishedLogicalInfoUpdate;
#if DEBUG_TT_CREATE
    // Count of blocks written into in current TT page
    NvU32 PhysicalBlkCount;
    // Count of number of blocks scanned in Nand scan for the TT page
    NvU32 ActualIterations;
    // Flags to print once when these category blocks encountered

    // Category 3 - contains data reserved blocks of FTL full partition
    NvBool IsSeenCategory3A = NV_FALSE;
    // Category 3 - includes TT and TAT reserved blocks in this type
    NvBool IsSeenCategory3B = NV_FALSE;
    // Category1 indicates reserved blocks before FTL full managed partition
    NvBool IsSeenCategory1 = NV_FALSE;
    // Category2 indicates Data/USR area for FTL full managed partition
    NvBool IsSeenCategory2 = NV_FALSE;
#endif
    // done TT entries population flag for each bank
    NvBool BankDoneArray[MAX_INTERLEAVE_COUNT];
#if DEBUG_TT_OPTIMIZATION
    NvBool IsMirrorIncomplete;
    BlockStatusEntry *transTab_Mirror = 0;
#endif
    nandILTat->Misc.RemainingDataRsvdBlocks = 0;
    ttLogicalBlkNum = 0;
    ttLogicalPageNumber = 0;
    currentTTBlock = 0;
    currentTTPage = 0;
    badPageCount = 0;
    RsvdDataCount = (sizeof(NvS32) * hNand->pNandTt->NoOfILBanks);
    pDataRsvdCount = (NvS32 *)AllocateVirtualMemory((NvU32) RsvdDataCount, TAT_ALLOC);
    InitBlockInfo(&blockInfo);
    NvOsMemset(pDataRsvdCount,0,sizeof(NvS32) * hNand->pNandTt->NoOfILBanks);
    // erase TAT blocks
    for (tatBlock = 0; tatBlock < nandILTat->Misc.NumOfBlksForTAT; tatBlock++)
    {
        if (!IsBlockBad(hNand,
            &nandILTat->tatHandler.tatBlocks[tatBlock], NV_FALSE))
        {
            if (DoAdvancedErase(
                hNand,
                &nandILTat->tatHandler.tatBlocks[tatBlock],
                NV_TRUE) != NvSuccess)
            {

                MarkBlockAsBad(
                    hNand,
                    nandILTat->tatHandler.tatBlocks[tatBlock].BnkNum,
                    nandILTat->tatHandler.tatBlocks[tatBlock].BlkNum);
            }
        }
    }
    // erase TT blocks
    for (ttBlock = 0; ttBlock < nandILTat->Misc.NumOfBlksForTT; ttBlock++)
    {
        if (!IsBlockBad(hNand, &nandILTat->tatHandler.ttBlocks[ttBlock],
            NV_FALSE))
        {
            if (DoAdvancedErase(
                hNand,
                &nandILTat->tatHandler.ttBlocks[ttBlock],
                NV_FALSE) != NvSuccess)
            {

                MarkBlockAsBad(
                    hNand,
                    nandILTat->tatHandler.ttBlocks[ttBlock].BnkNum,
                    nandILTat->tatHandler.ttBlocks[ttBlock].BlkNum);
            }
        }
    }

    /*
     * This is the number of block status entries that can fit in
     * one page of TT
     */
    blkStEntryPerTTPg =
        hNand->NandDevInfo.PageSize / sizeof(BlockStatusEntry);
    // Calculate the log2 of blocks in TT page
    Log2BlkStEntryPerTTPg = NandUtilGetLog2(blkStEntryPerTTPg);

    // Get the memory for one TT page

    if (!transTab)
        transTab = (BlockStatusEntry *)AllocateVirtualMemory(
            hNand->NandDevInfo.PageSize,
            TAT_ALLOC);
#if DEBUG_TT_OPTIMIZATION
    if (!transTab_Mirror)
        transTab_Mirror = (BlockStatusEntry *)AllocateVirtualMemory(
            hNand->NandDevInfo.PageSize,
            TAT_ALLOC);
#endif
    /*
     * Build TT
     * This is the for loop represent the number of pages that needs
     * to build the Translation Table
     */
    for (ttPage = 0; ttPage < nandILTat->Misc.PgsRegForTT; ttPage++)
    {
        // Reset entry count
        EntryCountInPage = 0;
        // Started logical info update so set finish flag as false
        FinishedLogicalInfoUpdate = NV_FALSE;
#if DEBUG_TT_CREATE
        PhysicalBlkCount = 0;
        ActualIterations = 0;
#endif
        // clear the done flag for all banks before starting
        for (i = 0; i < MAX_INTERLEAVE_COUNT; i++)
            BankDoneArray[i] = NV_FALSE;
        NV_DEBUG_PRINTF(("Creating TT for page %d \r\n",ttPage));
        ttPageStartEntryNumber = ttPage * blkStEntryPerTTPg;
        ttPageEndEntryNumber = (ttPage+1) * blkStEntryPerTTPg - 1;

        // Clean the page and set it to 0xFF before building the TT page
        NvOsMemset(transTab, 0xFF, hNand->NandDevInfo.PageSize);
#if DEBUG_TT_OPTIMIZATION
        IsMirrorIncomplete = NV_TRUE;
        NvOsMemset(transTab_Mirror, 0xFF, hNand->NandDevInfo.PageSize);
#endif

        if (hNand->IsFirstBoot == NV_TRUE /* We are not recreating
                TT table needed after sudden power down */)
        {
            /*
             * We would assume that the logical block number
             * is greater than the physical block number
             */
            startBlkNum_Mirror = ttPageStartEntryNumber >>
                nandILTat->Misc.bsc4NoOfILBanks;
        }
        else
        {
            // Recreating TT after sudden power down entire device scanned
            startBlkNum_Mirror = 0;
        }
#if TT_FIRST_BOOT_OPTIMIZATION
        startBlkNum = startBlkNum_Mirror;
#else
        // Start scanning from beginning when no optimization used
        startBlkNum = 0;
#endif
        endBlkNum = (nandILTat->Misc.TotEraseBlks /
            nandILTat->Misc.NoOfILBanks) - 1;
        totaltatblocks = hNand->ITat.Misc.tatLogicalBlocks <<
            nandILTat->Misc.bsc4NoOfILBanks;
        totalttblocks = hNand->ITat.Misc.ttLogicalBlocks <<
            nandILTat->Misc.bsc4NoOfILBanks;
        /*
         * scan the complete device to get the logical block, data reserved
         * block and system reserved block for the current translation
         * table page
         */
        for (
            physBlk = startBlkNum;
            physBlk <= endBlkNum;
            physBlk++)
        {
            if((physBlk % 500) == 0)
                NV_DEBUG_PRINTF(("Physical Block %d \r\n", physBlk));
#if DEBUG_TT_OPTIMIZATION
            // Disable mirror update till start crosses mirror start block
            if (physBlk < startBlkNum_Mirror)
            {
                if (IsMirrorIncomplete)
                    IsMirrorIncomplete = NV_FALSE;
            }
            else if (physBlk == startBlkNum_Mirror)
            {
                // Enable mirror update once physBlk crosses mirror start block
                if (!IsMirrorIncomplete)
                    IsMirrorIncomplete = NV_TRUE;
            }
#endif
            for (bank = 0; bank < nandILTat->Misc.NoOfILBanks; bank++)
            {
                TagInformation *pTagInfo;
#if DEBUG_TT_CREATE
                // Count actual iterations
                ActualIterations++;
#endif
                // Reset flag for clean chunk
                FoundCleanChunk = NV_FALSE;
                blockIsBad = NV_FALSE;
                ClearPhysicalPageArray(hNand,hNand->ITat.pPhysPgs);
                hNand->ITat.pPhysPgs[bank] =
                    (physBlk << nandILTat->Misc.bsc4PgsPerBlk) +
                    nandILTat->Misc.TagInfoPage;

                entryNum =
                    GetTTableBlockStatusEntryNumber(hNand,bank,physBlk);
                // Calculate the TT entry index in TT page
                IndexInPg = MACRO_MOD_LOG2NUM(entryNum, Log2BlkStEntryPerTTPg);
                //check whether the block is good/bad
                SetBlockInfo(&blockInfo, bank,physBlk);
                if (IsBlockBad(hNand, &blockInfo, NV_FALSE))
                {
                    blockIsBad = NV_TRUE;
                }

                // Get Tag information initialized in IsBlockBad call
                pTagInfo = (TagInformation *)(hNand->pSpareBuf +
                    hNand->gs_NandDeviceInfo.TagOffset);
                /*
                 * We will be creating translation table for all the
                 * blocks on the device. These blocks are divided into
                 * three categories.
                 * 1) Blocks before FtlStartPba and after FtlEndPba:
                 * these blocks are not managed by our bbmwl driver.
                 * Hence they are reserved for this driver.
                 * 2) Logical/user blocks: Blocks that can be used
                 * by the user. (FtlEndPba - FtlStartPba - bbmwlRsvdBlocks)
                 * 3) bbmwlRsvdBlocks: Blocks reserved by bbmwl
                 * driver (This includes data reserved blocks,
                 * TAT, TT, FBB blocks)
                 */

                // build TT for reserved area.CATEGORY ONE
                if ((physBlk < hNand->FtlStartPba) ||
                (physBlk >= hNand->FtlEndPba))
                {
                    if ((entryNum >= ttPageStartEntryNumber) &&
                    (entryNum <= ttPageEndEntryNumber))
                    {
#if DEBUG_TT_CREATE
                        /*
                         * print to indicate start of reserved area
                         * before the FTL full managed blocks
                         */
                        if (IsSeenCategory1 == NV_FALSE)
                        {
                            NV_DEBUG_PRINTF(("\r\n ^^^^^^^^^^^^^^ Category "
                                "1 - system reserved blocks at 0x%x ",
                                physBlk));
                            IsSeenCategory1 = NV_TRUE;
                        }
#endif
                        transTab[IndexInPg].BlockNotUsed = 0;
                        transTab[IndexInPg].BlockGood = 0;
                        transTab[IndexInPg].DataReserved = 0;
                        transTab[IndexInPg].SystemReserved = 1;
                        transTab[IndexInPg].TatReserved =0;
                        transTab[IndexInPg].TtReserved = 0;
                        transTab[IndexInPg].Region = 0;
#if DEBUG_TT_OPTIMIZATION
                        if (IsMirrorIncomplete)
                        {
                            transTab_Mirror[IndexInPg].BlockNotUsed = 0;
                            transTab_Mirror[IndexInPg].BlockGood = 0;
                            transTab_Mirror[IndexInPg].DataReserved = 0;
                            transTab_Mirror[IndexInPg].SystemReserved = 1;
                            transTab_Mirror[IndexInPg].TatReserved =0;
                            transTab_Mirror[IndexInPg].TtReserved = 0;
                            transTab_Mirror[IndexInPg].Region = 0;
                        }
#if ENABLE_TT_OPT_ASSERT
                        else
                            NV_ASSERT(0);
#endif
#endif
                        // Update entry count initialized with TT flags
                        EntryCountInPage++;
#if DEBUG_TT_CREATE
                        // If valid logical block, increment physical count
                        if (pTagInfo->LogicalBlkNum >= 0)
                        {
                            PhysicalBlkCount++;
                        }
#endif
                }
                }
                //Build TT for DATA/USER area CATEGORY TWO
                else if((physBlk >= hNand->FtlStartPba) && (physBlk <
                    (hNand->FtlEndPba - hNand->ITat.Misc.ftlReservedLblocks)))
                {
                    // Validate the entry if it's within the TT page limit
                    if ((entryNum >= ttPageStartEntryNumber) &&
                    (entryNum <= ttPageEndEntryNumber))
                    {
#if DEBUG_TT_CREATE
                        /*
                         * print to indicate start of user/data area
                         * FTL full managed blocks
                         */
                        if (IsSeenCategory2 == NV_FALSE)
                        {
                            NV_DEBUG_PRINTF(("\r\n ^^^^^^^^^^^^^^ Category "
                                "2 - DATA/USR blocks at 0x%x ",
                                physBlk));
                            IsSeenCategory2 = NV_TRUE;
                        }
#endif
                         if (blockIsBad)
                        {
                            NV_DEBUG_PRINTF(("\r\n block marked as bad in "
                                "2nd category bank=%d, blk=%d, Cnt = %d",
                                bank, physBlk, ++BadBlockCnt));
                            transTab[IndexInPg].BlockGood = 0;
#if DEBUG_TT_OPTIMIZATION
                            if (IsMirrorIncomplete)
                            {
                                transTab_Mirror[IndexInPg].BlockGood = 0;
                            }
#if ENABLE_TT_OPT_ASSERT
                            else
                                NV_ASSERT(0);
#endif
#endif
                        } else
                        {
                            transTab[IndexInPg].BlockGood = 1;
#if DEBUG_TT_OPTIMIZATION
                            if (IsMirrorIncomplete)
                            {
                                transTab_Mirror[IndexInPg].BlockGood = 1;
                            }
#if ENABLE_TT_OPT_ASSERT
                            else
                                NV_ASSERT(0);
#endif
#endif
                        }

                        transTab[IndexInPg].BlockNotUsed =
                            (pTagInfo->LogicalBlkNum < 0) ? 1 : 0;
                        transTab[IndexInPg].DataReserved = blockIsBad;
                        transTab[IndexInPg].SystemReserved = 0;
                        transTab[IndexInPg].TatReserved = 0;
                        transTab[IndexInPg].TtReserved = 0;
                        transTab[IndexInPg].Region = 0;
#if DEBUG_TT_OPTIMIZATION
                        if (IsMirrorIncomplete)
                        {
                            transTab_Mirror[IndexInPg].BlockNotUsed =
                                (pTagInfo->LogicalBlkNum < 0) ? 1 : 0;
                            transTab_Mirror[IndexInPg].DataReserved = blockIsBad;
                            transTab_Mirror[IndexInPg].SystemReserved = 0;
                            transTab_Mirror[IndexInPg].TatReserved = 0;
                            transTab_Mirror[IndexInPg].TtReserved = 0;
                            transTab_Mirror[IndexInPg].Region = 0;
                        }
#if ENABLE_TT_OPT_ASSERT
                        else
                            NV_ASSERT(0);
#endif
#endif

                        // Update entry count initialized with TT flags
                        EntryCountInPage++;
                        /*
                         * check whether we are exceeding our total
                         * reserved blocks count
                         */
                        if(transTab[IndexInPg].DataReserved)
                        {
                            if(pDataRsvdCount[bank] <
                                hNand->ITat.Misc.bbmwlDataRservedBlocks)
                            {
                                pDataRsvdCount[bank]++;
                            }
                            else
                            {
                                /*
                                 * If we reach max reserved count then treat
                                 * these blocks as normal blocks
                                 */
                                NV_DEBUG_PRINTF(("\r\n  Chip %d exceeded"
                                    " data reserved count",bank));
                                transTab[IndexInPg].DataReserved = 0;
#if DEBUG_TT_OPTIMIZATION
                                if (IsMirrorIncomplete)
                                {
                                    transTab_Mirror[IndexInPg].DataReserved = 0;
                                }
#if ENABLE_TT_OPT_ASSERT
                                else
                                    NV_ASSERT(0);
#endif
#endif
                            }
                        }
                    }
                    /*
                     * Validate the logical block number (if any) to check if
                     * it's within the TT page limit
                     */
                    if ((blockIsBad == NV_FALSE) &&
                    (pTagInfo->LogicalBlkNum >= 0))
                    {
                        /*
                         * we are here that means we are within the
                         * limit so enter this into TT determine the
                         * translation table entry number from logical
                         * block number
                         */
                        entryNum = pTagInfo->LogicalBlkNum;
                        /*
                         * this maps LBA to entry number in TTable.
                         * convert the entry number into bank specific
                         * entry number
                         */
                        entryNum = GetTTableBlockStatusEntryNumber(
                            hNand,
                            bank,
                            entryNum);
                            // Recalculate index of block in TT page
                            IndexInPg = entryNum % blkStEntryPerTTPg;

                        if ((entryNum >= ttPageStartEntryNumber) &&
                        (entryNum <= ttPageEndEntryNumber))
                        {
#if DEBUG_TT_CREATE
                            /*
                             * update count of physical addresses
                             * initialized
                             * Logical block number found in current range
                             */
                            PhysicalBlkCount++;
#endif
                            // Initialize physical address in TT entry
                            transTab[IndexInPg].PhysBlkNum = physBlk;
#if DEBUG_TT_OPTIMIZATION
                            if (IsMirrorIncomplete)
                                transTab_Mirror[IndexInPg].PhysBlkNum = physBlk;
#if ENABLE_TT_OPT_ASSERT
                            else
                                NV_ASSERT(0);
#endif
#endif
                        }
                        else
                        {
                            /*
                             * we don't expect to be here during the
                             * creation of TT.
                             * if we are here that indicates that the
                             * logical block entry and it's associated
                             * PBA(s) are not falling in the same TT
                             * page. If this happens,we need to enable
                             * scanning of all the blocks of nand flash
                             * for each TT page. Enabling can be done
                             * by making
                             * 'enableAllBlocksScannig4EachTTPage'
                             * as true, which is defined above.
                             */
                            /*
                             * Indicates we found LBA for a physical block
                             * outside current TT page. Since our
                             * assumption is LBA <= PBA we expect only
                             * (a) entryNum < ttPageStartEntryNumber -
                             *      Flag error
                             */
                            if (entryNum < ttPageStartEntryNumber)
                            {
                                NV_DEBUG_PRINTF(("\nError: TT create "
                                    "entryNum=0x%x outside range ",
                                    entryNum));
                            }
                            /*
                             * (b) entryNum > ttPageEndEntryNumber allowed
                             * which means LBA > PBA and not possible
                             */
                            else if (entryNum > ttPageEndEntryNumber)
                            {
                                if (hNand->IsFirstBoot == NV_TRUE)
                                {
                                    // We need to get entries in next page
                                    // for all banks before we give up  search
                                    // some bank may have more bad blocks
                                    NvU32 CountDone;
                                    NvU32 i1;
                                    if (!BankDoneArray[bank])
                                        BankDoneArray[bank] = NV_TRUE;
                                    CountDone = 0;
                                    for (i1 = 0; i1 < nandILTat->Misc.NoOfILBanks; i1++)
                                    {
                                        if (BankDoneArray[i1])
                                            CountDone++;
                                    }
                                    // We have populated the LBA information
                                    // for all blocks in TT page
                                    if ((!FinishedLogicalInfoUpdate) &&
                                        (CountDone == nandILTat->Misc.NoOfILBanks))
                                        FinishedLogicalInfoUpdate = NV_TRUE;
                                }
                                else
                                {
                                    NV_DEBUG_PRINTF(("\nError: TT create "
                                        "entryNum=0x%x past end physical 0x%x ",
                                        entryNum, ttPageEndEntryNumber));
                                }
                            }
                        }
                    }
                    else if (pTagInfo->LogicalBlkNum < 0)
                    {
                        // Logical block number = 0xFFFFFFFF indicates clean block
                        if ((blockIsBad == NV_FALSE) && (hNand->IsFirstBoot == NV_TRUE))
                           // case logical block 0xFF
                        {
                            /*
                             * When not in recreate TT mode, we abort further
                             * scan in a iteration once this flag is set.
                             * Found block which is not written into
                             */
                            FoundCleanChunk = NV_TRUE;
                        }
                    }
                }
                //Build TT for BBMWL reserved blocks CATEGORY THREE
                else
                {
                    /*
                     * Here we have data reserved blocks along
                     * with TAT,TT,FBB blocks
                     */

                    //Build TT for data reserved blocks
                    if(physBlk <
                        (hNand->FtlEndPba -
                        hNand->ITat.Misc.bbmwlSystemRservedBlocks))
                    {
                        if ((entryNum >= ttPageStartEntryNumber) &&
                        (entryNum <= ttPageEndEntryNumber))
                        {
#if DEBUG_TT_CREATE
                            /*
                             * Here we have data reserved blocks along
                             * with TAT,TT,FBB blocks
                             */
                            if (IsSeenCategory3A == NV_FALSE)
                            {
                                NV_DEBUG_PRINTF(("\r\n ^^^^^^^^^ Category "
                                    "3 - data reserved blocks at 0x%x ",
                                    physBlk));
                                IsSeenCategory3A = NV_TRUE;
                            }
#endif
                             if (blockIsBad )
                            {
                                NV_DEBUG_PRINTF(("\r\n block marked as bad "
                                    "in 3rd-A category bank=%d, blk=%d , Cnt = %d",
                                    bank, physBlk, ++BadBlockCnt));
                                transTab[IndexInPg].BlockGood = 0;
#if DEBUG_TT_OPTIMIZATION
                                if (IsMirrorIncomplete)
                                    transTab_Mirror[IndexInPg].BlockGood = 0;
#if ENABLE_TT_OPT_ASSERT
                                else
                                    NV_ASSERT(0);
#endif
#endif
                            }
                            else
                            {
                                transTab[IndexInPg].BlockGood = 1;
#if DEBUG_TT_OPTIMIZATION
                                if (IsMirrorIncomplete)
                                    transTab_Mirror[IndexInPg].BlockGood = 1;
#if ENABLE_TT_OPT_ASSERT
                                else
                                    NV_ASSERT(0);
#endif
#endif
                            }
                            transTab[IndexInPg].BlockNotUsed =
                                (pTagInfo->LogicalBlkNum < 0) ?
                                1 : 0;
                            transTab[IndexInPg].DataReserved = 0;
#if DEBUG_TT_OPTIMIZATION
                            if (IsMirrorIncomplete)
                            {
                                transTab_Mirror[IndexInPg].BlockNotUsed =
                                    (pTagInfo->LogicalBlkNum < 0) ?
                                    1 : 0;
                                transTab_Mirror[IndexInPg].DataReserved = 0;
                            }
#if ENABLE_TT_OPT_ASSERT
                            else
                                NV_ASSERT(0);
#endif
#endif
                            if ((pTagInfo->LogicalBlkNum < 0) &&
                                (blockIsBad == NV_FALSE))
                            {
                                if(pDataRsvdCount[bank])
                                    pDataRsvdCount[bank]--;
                                else
                                {
                                    transTab[IndexInPg].DataReserved = 1;
#if DEBUG_TT_OPTIMIZATION
                                    if (IsMirrorIncomplete)
                                        transTab_Mirror[IndexInPg].DataReserved = 1;
#if ENABLE_TT_OPT_ASSERT
                                    else
                                        NV_ASSERT(0);
#endif
#endif
                                    hNand->ITat.Misc.RemainingDataRsvdBlocks++;
                                }
                            }
                            transTab[IndexInPg].SystemReserved = 0;
                            transTab[IndexInPg].TatReserved = 0;
                            transTab[IndexInPg].TtReserved = 0;
                            transTab[IndexInPg].Region = 0;
#if DEBUG_TT_OPTIMIZATION
                            if (IsMirrorIncomplete)
                            {
                                transTab_Mirror[IndexInPg].SystemReserved = 0;
                                transTab_Mirror[IndexInPg].TatReserved = 0;
                                transTab_Mirror[IndexInPg].TtReserved = 0;
                                transTab_Mirror[IndexInPg].Region = 0;
                            }
#if ENABLE_TT_OPT_ASSERT
                            else
                                NV_ASSERT(0);
#endif
#endif
                            // Update entry count
                            EntryCountInPage++;
                        }
                        /*
                         * Validate the logical block number (if any) to check
                         * if it's within the TT page limit
                         */
                        if ((blockIsBad == NV_FALSE) &&
                        (pTagInfo->LogicalBlkNum >= 0))
                        {
                            /*
                             * we are here that means we are within the limit
                             * so enter this into TT. Determine the translation
                             * table entry number from logical block number
                             */
                            entryNum = pTagInfo->LogicalBlkNum;
                            /*
                             * convert the entry number into bank specific
                             * entry number
                             */
                            entryNum = GetTTableBlockStatusEntryNumber(
                                hNand,
                                bank,
                                entryNum);
                            // Recalculate index in TT page
                            IndexInPg = entryNum % blkStEntryPerTTPg;

                            if ((entryNum >= ttPageStartEntryNumber) &&
                            (entryNum <= ttPageEndEntryNumber))
                            {
#if DEBUG_TT_CREATE
                                /*
                                 * update count of physical addresses
                                 * initialized
                                 */
                                PhysicalBlkCount++;
#endif
                                transTab[IndexInPg].PhysBlkNum = physBlk;
#if DEBUG_TT_OPTIMIZATION
                                if (IsMirrorIncomplete)
                                    transTab_Mirror[IndexInPg].PhysBlkNum = physBlk;
#if ENABLE_TT_OPT_ASSERT
                                else
                                    NV_ASSERT(0);
#endif
#endif
                            }
                            else
                            {
                                /*
                                 * we don't expect to be here during the
                                 * creation of TT. If we are here that
                                 * indicates that the logical
                                 * block entry and it's associated PBA(s)
                                 * are not falling in the same TT page.
                                 * If this happens, we need to enable
                                 * scanning of all the blocks of
                                 * nand flash for each TT page. Enabling
                                 * can be done by making
                                 * 'enableAllBlocksScannig4EachTTPage'
                                 * as true, which is defined above.
                                 */
                                NV_DEBUG_PRINTF(("\nError: TT create2 entryNum"
                                    "=0x%x outside range ", entryNum));
                                /*
                                 * This condition is not expected as no data
                                 * reserved block should be uninitialized
                                 */
                            }
                        }
                    }
                    //Build TT for TAT,TT,FBB reserved blocks
                    else
                    {
                        if ((entryNum >= ttPageStartEntryNumber) &&
                            (entryNum <= ttPageEndEntryNumber))
                        {
#if DEBUG_TT_CREATE
                            /*
                             * Here we have data reserved blocks along
                             * with TAT,TT,FBB blocks
                             */
                            if (IsSeenCategory3B == NV_FALSE)
                            {
                                NV_DEBUG_PRINTF(("\r\n ######## Category 3B - "
                                    "TAT TT reserved blocks at 0x%x block ",
                                    physBlk));
                                IsSeenCategory3B = NV_TRUE;
                            }
#endif
                            transTab[IndexInPg].BlockNotUsed = 0;
                            transTab[IndexInPg].DataReserved = 0;;
                            transTab[IndexInPg].SystemReserved = 1;
                            transTab[IndexInPg].Region = 0;
#if DEBUG_TT_OPTIMIZATION
                            if (IsMirrorIncomplete)
                            {
                                transTab_Mirror[IndexInPg].BlockNotUsed = 0;
                                transTab_Mirror[IndexInPg].DataReserved = 0;;
                                transTab_Mirror[IndexInPg].SystemReserved = 1;
                                transTab_Mirror[IndexInPg].Region = 0;
                            }
#if ENABLE_TT_OPT_ASSERT
                            else
                                NV_ASSERT(0);
#endif
#endif // DEBUG_TT_OPTIMIZATION
                            if(totaltatblocks)
                            {
                                transTab[IndexInPg].TatReserved = 1;
                                transTab[IndexInPg].TtReserved = 0;
#if DEBUG_TT_OPTIMIZATION
                                if (IsMirrorIncomplete)
                                {
                                    transTab_Mirror[IndexInPg].TatReserved = 1;
                                    transTab_Mirror[IndexInPg].TtReserved = 0;
                                }
#if ENABLE_TT_OPT_ASSERT
                                else
                                    NV_ASSERT(0);
#endif
#endif
                                totaltatblocks--;
                            }
                            else if(totalttblocks)
                            {
                                transTab[IndexInPg].TatReserved = 0;
                                transTab[IndexInPg].TtReserved = 1;
#if DEBUG_TT_OPTIMIZATION
                                if (IsMirrorIncomplete)
                                {
                                    transTab_Mirror[IndexInPg].TatReserved = 0;
                                    transTab_Mirror[IndexInPg].TtReserved = 1;
                                }
#if ENABLE_TT_OPT_ASSERT
                                else
                                    NV_ASSERT(0);
#endif
#endif // DEBUG_TT_OPTIMIZATION
                                totalttblocks--;
                            }
                            else
                            {
                                transTab[IndexInPg].TatReserved = 0;
                                transTab[IndexInPg].TtReserved = 0;
#if DEBUG_TT_OPTIMIZATION
                                if (IsMirrorIncomplete)
                                {
                                    transTab_Mirror[IndexInPg].TatReserved = 0;
                                    transTab_Mirror[IndexInPg].TtReserved = 0;
                                }
#if ENABLE_TT_OPT_ASSERT
                                else
                                    NV_ASSERT(0);
#endif
#endif // DEBUG_TT_OPTIMIZATION
                            }
                             if (blockIsBad == NV_TRUE)
                            {
                                NV_DEBUG_PRINTF(("\r\n block marked as bad "
                                    "in 3rd-B category bank=%d, blk=%d, Cnt = %d",
                                    bank, physBlk, ++BadBlockCnt));
                                transTab[IndexInPg].BlockGood = 0;
#if DEBUG_TT_OPTIMIZATION
                                if (IsMirrorIncomplete)
                                    transTab_Mirror[IndexInPg].BlockGood = 0;
#if ENABLE_TT_OPT_ASSERT
                                else
                                    NV_ASSERT(0);
#endif
#endif // DEBUG_TT_OPTIMIZATION
                            }
                            else
                            {
                                transTab[IndexInPg].BlockGood = 1;
#if DEBUG_TT_OPTIMIZATION
                                if (IsMirrorIncomplete)
                                    transTab_Mirror[IndexInPg].BlockGood = 1;
#if ENABLE_TT_OPT_ASSERT
                                else
                                    NV_ASSERT(0);
#endif
#endif // DEBUG_TT_OPTIMIZATION
                            }
                            // Update entry count
                            EntryCountInPage++;
                        }
                    }

                }
            }
            if (hNand->IsFirstBoot == NV_TRUE)
            {
                /*
                 * In case of first boot after nvflash
                 * Check if the entries in this block have been found
                 */
                if ((EntryCountInPage == (((NvU32)ttPageEndEntryNumber -
                    (NvU32)ttPageStartEntryNumber) + 1)
                    /* Updated flags for all TT blocks in this range */) &&
                    ((FoundCleanChunk == NV_TRUE /* Found block 0xFF */) ||
                    (FinishedLogicalInfoUpdate == NV_TRUE)
                    /* Found all blocks with LBA in this TT page */ ))
                {
#if DEBUG_TT_OPTIMIZATION
                    // no abort but stop updating the mirror page
                    if (IsMirrorIncomplete)
                        IsMirrorIncomplete = NV_FALSE;
#elif TT_FIRST_BOOT_OPTIMIZATION
                    // abort processing
                    break;
#endif // DEBUG_TT_OPTIMIZATION, TT_FIRST_BOOT_OPTIMIZATION
                }
            }
        }
#if DEBUG_TT_OPTIMIZATION
        // Compare the TT page with the mirror TT page obtained with optimization
        if (NandUtilMemcmp(transTab, transTab_Mirror, hNand->NandDevInfo.PageSize))
        {
            NvOsDebugPrintf("\nError: TT create mismatch in "
                "transTab and mirror for TT page=%d, page size=%d ", ttPage,
                hNand->NandDevInfo.PageSize);
            // assert
            NV_ASSERT(NV_FALSE);
        }
#endif
        // Store TT
#if (DEBUG_TT_OPTIMIZATION && USE_MIRROR)
        rtnSts = WriteTTPage(
            hNand,
            ttPage,
            &currentTTPage,
            &currentTTBlock,
            &ttLogicalBlkNum,
            &ttLogicalPageNumber,
            &badPageCount,
            (BlockStatusEntry *)transTab_Mirror);
#else // (DEBUG_TT_OPTIMIZATION && USE_MIRROR)
    for(;;)
    {
        rtnSts = WriteTTPage(
            hNand,
            ttPage,
            &currentTTPage,
            &currentTTBlock,
            &ttLogicalBlkNum,
            &ttLogicalPageNumber,
            &badPageCount,
            (BlockStatusEntry *)transTab);
        if (rtnSts == NvError_NandBadBlock)
        {
            ReplaceTTBadBlock(
                hNand, ttLogicalPageNumber, ttLogicalBlkNum,
                &currentTTBlock, &currentTTPage, &badPageCount);
        }
        else if (rtnSts == NvSuccess)
            break;
    }
#endif // (DEBUG_TT_OPTIMIZATION && USE_MIRROR)
        ExitOnTLErr(rtnSts);
        SHOW(("\r\n updated TT page %d",ttPage));
#if DEBUG_TT_CREATE
        NV_DEBUG_PRINTF(("\r\nttLogicalPageNumber=%u, Actual iterations=0x%x, "
            "Blk count = 0x%x,physical count = 0x%x, FoundCleanChunk=%u  ",
            ttLogicalPageNumber, ActualIterations, EntryCountInPage,
            PhysicalBlkCount, ((FoundCleanChunk == NV_TRUE)? 1 : 0)));
#endif
    }

    *nandILTat->tatHandler.WorkingCopy.lastUsedTTBlock =
        nandILTat->tatHandler.WorkingCopy.TtAllocBlk[ttLogicalBlkNum];

    /*
     * check whether all the TT blocks are associated with a valid block info.
     * if not, associate the valid blocks to them.
     */
    nextTTBlockSet = (BlockInfo *)AllocateVirtualMemory(
        sizeof(BlockInfo) * nandILTat->tatHandler.WorkingCopy.TtBlkCnt,
        TAT_ALLOC);//uncached

    for (i = 0; i < nandILTat->tatHandler.WorkingCopy.TtBlkCnt; i++)
    {
        InitBlockInfo(&nextTTBlockSet[i]);
    }
    GetNextTTBlockSet(hNand, nextTTBlockSet);
    currentIndex = 0;

    for (i = 0; i < nandILTat->tatHandler.WorkingCopy.TtBlkCnt; i++)
    {
        if ((nandILTat->tatHandler.WorkingCopy.TtAllocBlk[i].BnkNum == -1) ||
            (nandILTat->tatHandler.WorkingCopy.TtAllocBlk[i].BlkNum == -1))
        {
            // associate a next good block.
            nandILTat->tatHandler.WorkingCopy.TtAllocBlk[i] =
                nextTTBlockSet[currentIndex];
            *nandILTat->tatHandler.WorkingCopy.lastUsedTTBlock =
                nextTTBlockSet[currentIndex];
            currentIndex++;
        }
    }

    // set the revision number to 0.
    *nandILTat->tatHandler.WorkingCopy.RevNum = 0;
    rtnSts = WriteTAT(hNand);
    ExitOnTLErr(rtnSts);
    errorExit:
    ReleaseVirtualMemory(transTab,TAT_ALLOC);
#if DEBUG_TT_OPTIMIZATION
    ReleaseVirtualMemory(transTab_Mirror,TAT_ALLOC);
#endif
    ReleaseVirtualMemory(nextTTBlockSet,TAT_ALLOC);
    ReleaseVirtualMemory(pDataRsvdCount,TAT_ALLOC);
    return(rtnSts);
}

#if NEW_TT_CODE
NvError
WriteTTPage(
    NvNandHandle hNand,
    NvS32 ttPage,
    NvS32 *currentTTPage,
    NvS32 *currentTTBlock,
    NvS32 *ttLogicalBlkNum,
    NvS32 *ttLogicalPageNumber,
    NvS32 *badPageCount,
    BlockStatusEntry *transTab)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    NvS32 startPage,bank, ttPageIndex;
    NvBool writeSuccess ;
    NvBool BlockBad = NV_TRUE ;


    /*
     * This API should only be used by createTranslationTable() API.
     * currentTTPage --> point to the free page where we can write the
     * ttinfo page. After we write it, we
     * need to make it point to next free page for the next writeTTPage().
     */
    if (ttPage == 0)
    {
        // reset the working copy info
        NvOsMemset(
            &nandILTat->tatHandler.WorkingCopy.TtAllocBlk[*ttLogicalBlkNum],
            0xFF,
            (nandILTat->tatHandler.WorkingCopy.TtBlkCnt * sizeof(BlockInfo)));
        NvOsMemset(&nandILTat->tatHandler.WorkingCopy.TtAllocPg[ttPage],
            0xFF,
               (nandILTat->tatHandler.WorkingCopy.TtPageCnt * sizeof(short)));
    }

    writeSuccess = NV_FALSE;

    while (*currentTTBlock < nandILTat->Misc.NumOfBlksForTT)
    {
        // check whether the TT page is beyond the block boundary.
        if (*currentTTPage >= hNand->NandDevInfo.PgsPerBlk)
        {
            *currentTTPage = 0;
            *currentTTBlock = *currentTTBlock + 1;
            *ttLogicalBlkNum  = *ttLogicalBlkNum + 1;
            *badPageCount = 0;
        }

        //int ttBlock = handler->ttBlocks[*currentTTBlock].BlkNum;
        bank = nandILTat->tatHandler.ttBlocks[*currentTTBlock].BnkNum;

        // clear the physical block array
        ClearPhysicalPageArray(hNand,hNand->ITat.pPhysPgs);
        startPage = SetPhysicalPageArray(
            hNand,
            hNand->ITat.pPhysPgs,
            &nandILTat->tatHandler.ttBlocks[*currentTTBlock],
            nandILTat->Misc.TagInfoPage);

        // check whether the block is bad.
        {
            // check only when we are starting write from start page
            if (*currentTTPage == 0)
            {

                BlockBad = IsBlockBad(hNand,
                    &nandILTat->tatHandler.ttBlocks[*currentTTBlock],
                    NV_FALSE);
                if (BlockBad)
                {

                    // block is not good. skip it.
                    (*ttLogicalBlkNum)--;
                    *currentTTPage = hNand->NandDevInfo.PgsPerBlk + 1;
                    continue;
                }
            }
        }

        for (ttPageIndex = *currentTTPage;
        ttPageIndex < hNand->NandDevInfo.PgsPerBlk;
        ttPageIndex++, *currentTTPage=*currentTTPage+1)
        {

            // skip the tag info page.
/*
            if (ttPageIndex == nandILTat->Misc.TagInfoPage)
                continue;
*/
            hNand->ITat.pPhysPgs[bank] = startPage + ttPageIndex;

            if (ReadTagInfo(
                hNand,
                hNand->ITat.pPhysPgs,
                (bank << nandILTat->Misc.bsc4SctsPerPg),
                hNand->pSpareBuf) == NvSuccess)
            {
                TagInformation *pTagInfo;
                pTagInfo = (TagInformation *)(hNand->pSpareBuf +
                    hNand->gs_NandDeviceInfo.TagOffset);
                // Check whether the page is bad, if so, skip it.
                if (pTagInfo->Info.PageStatus ==
                    Information_Status_PAGE_BAD)
                {

                    *badPageCount = *badPageCount + 1;
                    /*
                     * Check if the bad page count is greater than 50% then
                     * mark the block bad and replace the block
                     */
                    if (*badPageCount > hNand->ITat.MaxNoOfBadPages)
                        return NvError_NandBadBlock;
                    continue;
                }

                // page is good to write.
                if (Write(
                    hNand,
                    hNand->ITat.pPhysPgs,
                    bank << nandILTat->Misc.bsc4SctsPerPg,
                    (NvU8 *)&transTab[0],
                    hNand->NandDevInfo.SctsPerPg, NULL) == NvSuccess)
                {
                    writeSuccess = NV_TRUE;
                    break;
                }
                else
                {
                    // Mark page as bad
                    MarkPageAsBad(
                        hNand,
                        hNand->ITat.pPhysPgs,
                        (bank << nandILTat->Misc.bsc4SctsPerPg),
                        NV_FALSE);
                    // increment the bad page count
                    *badPageCount = *badPageCount + 1;
                    /*
                     * Check if the bad page count is greater than 50%
                     * then mark the block bad and replace the block
                     */
                    if (*badPageCount > hNand->ITat.MaxNoOfBadPages)
                        return NvError_NandBadBlock;
                }
            }
            else
            {
                // mark the page bad
                MarkPageAsBad(
                    hNand,
                    hNand->ITat.pPhysPgs,
                    (bank << nandILTat->Misc.bsc4SctsPerPg),
                    NV_FALSE);
                // increment the bad page count
                *badPageCount = *badPageCount + 1;
                /*
                 * Check if the bad page count is greater than 50% then
                 * mark the block bad and replace the block
                 */
                if (*badPageCount > hNand->ITat.MaxNoOfBadPages)
                    return NvError_NandBadBlock;
            }
        }

        if (writeSuccess)
        {
            LOG_NAND_DEBUG_INFO(("\r\nwritten TT page=%d, bank=%d,block=%d",
                *currentTTPage,
                nandILTat->tatHandler.ttBlocks[*currentTTBlock].BnkNum,
                nandILTat->tatHandler.ttBlocks[*currentTTBlock].BlkNum),
                TAT_DEBUG_INFO);
            nandILTat->tatHandler.WorkingCopy.TtAllocBlk[*ttLogicalBlkNum] =
                nandILTat->tatHandler.ttBlocks[*currentTTBlock];
            nandILTat->tatHandler.WorkingCopy.TtAllocPg[*ttLogicalPageNumber]
                = (*ttLogicalBlkNum << nandILTat->Misc.bsc4PgsPerBlk) +
                *currentTTPage;
            *ttLogicalPageNumber = *ttLogicalPageNumber + 1;
            /*
             * increase the *currentTTPage here because we break from for
             * loop therefore the count is not increased.
             */
            *currentTTPage = *currentTTPage + 1;
            break;
        }
    }

    if (!writeSuccess)
    {
        /*
         * HINT: If the write fails then replace the block with a good block
         * and mark the current block bad
         * after copy back the previous written data.
         */
        RETURN(NvError_NandTLFailed);
    }
    return(NvSuccess);
}

#else
NvError
WriteTTPage(
    NvNandHandle hNand,
    NvS32 ttPage,
    NvS32 *currentTTPage,
    NvS32 *currentTTBlock,
    NvS32 *ttLogicalBlkNum,
    NvS32 *ttLogicalPageNumber,
    NvS32 *badPageCount,
    BlockStatusEntry *transTab)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    NvS32 startPage,bank, ttPageIndex;
    NvBool writeSuccess ;
    NvBool BlockBad = NV_TRUE ;


    /*
     * This API should only be used by createTranslationTable() API.
     * currentTTPage --> point to the free page where we can write the
     * ttinfo page. After we write it, we
     * need to make it point to next free page for the next writeTTPage().
     */
    if (ttPage == 0)
    {
        // reset the working copy info
        NvOsMemset(
            &nandILTat->tatHandler.WorkingCopy.TtAllocBlk[*ttLogicalBlkNum],
            0xFF,
            (nandILTat->tatHandler.WorkingCopy.TtBlkCnt * sizeof(BlockInfo)));
        NvOsMemset(&nandILTat->tatHandler.WorkingCopy.TtAllocPg[ttPage],
            0xFF,
               (nandILTat->tatHandler.WorkingCopy.TtPageCnt * sizeof(short)));
    }

    writeSuccess = NV_FALSE;

    while (*currentTTBlock < nandILTat->Misc.NumOfBlksForTT)
    {
        // check whether the TT page is beyond the block boundary.
        if (*currentTTPage >= hNand->NandDevInfo.PgsPerBlk)
        {
            *currentTTPage = 0;
            *currentTTBlock = *currentTTBlock + 1;
            *ttLogicalBlkNum  = *ttLogicalBlkNum + 1;
            *badPageCount = 0;
        }

        //int ttBlock = handler->ttBlocks[*currentTTBlock].BlkNum;
        bank = nandILTat->tatHandler.ttBlocks[*currentTTBlock].BnkNum;

        // clear the physical block array
        ClearPhysicalPageArray(hNand,hNand->ITat.pPhysPgs);
        startPage = SetPhysicalPageArray(
            hNand,
            hNand->ITat.pPhysPgs,
            &nandILTat->tatHandler.ttBlocks[*currentTTBlock],
            nandILTat->Misc.TagInfoPage);

        // check whether the block is bad.
        {
            // check only when we are starting write from start page
            if (*currentTTPage == 0)
            {

                BlockBad = IsBlockBad(hNand,
                    &nandILTat->tatHandler.ttBlocks[*currentTTBlock],
                    NV_FALSE);
                if (BlockBad)
                {

                    // block is not good. skip it.
                    (*ttLogicalBlkNum)--;
                    *currentTTPage = hNand->NandDevInfo.PgsPerBlk + 1;
                    continue;
                }
            }
        }

        for (ttPageIndex = *currentTTPage;
        ttPageIndex < hNand->NandDevInfo.PgsPerBlk;
        ttPageIndex++, *currentTTPage=*currentTTPage+1)
        {

            // skip the tag info page.
/*
            if (ttPageIndex == nandILTat->Misc.TagInfoPage)
                continue;
*/
            hNand->ITat.pPhysPgs[bank] = startPage + ttPageIndex;

            if (ReadTagInfo(
                hNand,
                hNand->ITat.pPhysPgs,
                (bank << nandILTat->Misc.bsc4SctsPerPg),
                hNand->pSpareBuf) == NvSuccess)
            {
                TagInformation *pTagInfo;
                pTagInfo = (TagInformation *)(hNand->pSpareBuf +
                    hNand->gs_NandDeviceInfo.TagOffset);
                // Check whether the page is bad, if so, skip it.
                if (pTagInfo->Info.PageStatus ==
                    Information_Status_PAGE_BAD)
                {

                    *badPageCount = *badPageCount + 1;
                    /*
                     * Check if the bad page count is greater than 50% then
                     * mark the block bad and replace the block
                     */
                    if (*badPageCount > (hNand->NandDevInfo.PgsPerBlk >> 1))
                    {
                        if (!ReplaceTTBadBlock(
                            hNand,
                            *ttLogicalPageNumber,
                            *ttLogicalBlkNum,
                            currentTTBlock,
                            currentTTPage,
                            badPageCount))
                        {
                            RETURN(NvError_NandTLFailed);
                        }
                        ttPage = *currentTTPage;
                    }
                    continue;
                }

                // page is good to write.
                if (Write(
                    hNand,
                    hNand->ITat.pPhysPgs,
                    bank << nandILTat->Misc.bsc4SctsPerPg,
                    (NvU8 *)&transTab[0],
                    hNand->NandDevInfo.SctsPerPg, NULL) == NvSuccess)
                {
                    writeSuccess = NV_TRUE;
                    break;
                }
                else
                {
                    // Mark page as bad
                    MarkPageAsBad(
                        hNand,
                        hNand->ITat.pPhysPgs,
                        (bank << nandILTat->Misc.bsc4SctsPerPg),
                        NV_FALSE);
                    // increment the bad page count
                    *badPageCount = *badPageCount + 1;
                    /*
                     * Check if the bad page count is greater than 50%
                     * then mark the block bad and replace the block
                     */
                    if (*badPageCount > (hNand->NandDevInfo.PgsPerBlk >> 1))
                    {
                        if (!ReplaceTTBadBlock(
                            hNand,
                            *ttLogicalPageNumber,
                            *ttLogicalBlkNum,
                            currentTTBlock,
                            currentTTPage,
                            badPageCount))
                        {
                            RETURN(NvError_NandTLFailed);
                        }
                        ttPage = *currentTTPage;
                    }
                }
            }
            else
            {
                // mark the page bad
                MarkPageAsBad(
                    hNand,
                    hNand->ITat.pPhysPgs,
                    (bank << nandILTat->Misc.bsc4SctsPerPg),
                    NV_FALSE);
                // increment the bad page count
                *badPageCount = *badPageCount + 1;
                /*
                 * Check if the bad page count is greater than 50% then
                 * mark the block bad and replace the block
                 */
                if (*badPageCount > (hNand->NandDevInfo.PgsPerBlk >> 1))
                {
                    if (!ReplaceTTBadBlock(
                        hNand,
                        *ttLogicalPageNumber,
                        *ttLogicalBlkNum,
                        currentTTBlock,
                        currentTTPage,
                        badPageCount))
                    {
                        RETURN(NvError_NandTLFailed);
                    }
                    ttPage = *currentTTPage;
                }
            }
        }

        if (writeSuccess)
        {
            LOG_NAND_DEBUG_INFO(("\r\nwritten TT page=%d, bank=%d,block=%d",
                *currentTTPage,
                nandILTat->tatHandler.ttBlocks[*currentTTBlock].BnkNum,
                nandILTat->tatHandler.ttBlocks[*currentTTBlock].BlkNum),
                TAT_DEBUG_INFO);
            nandILTat->tatHandler.WorkingCopy.TtAllocBlk[*ttLogicalBlkNum] =
                nandILTat->tatHandler.ttBlocks[*currentTTBlock];
            nandILTat->tatHandler.WorkingCopy.TtAllocPg[*ttLogicalPageNumber]
                = (*ttLogicalBlkNum << nandILTat->Misc.bsc4PgsPerBlk) +
                *currentTTPage;
            *ttLogicalPageNumber = *ttLogicalPageNumber + 1;
            /*
             * increase the *currentTTPage here because we break from for
             * loop therefore the count is not increased.
             */
            *currentTTPage = *currentTTPage + 1;
            break;
        }
    }

    if (!writeSuccess)
    {
        /*
         * HINT: If the write fails then replace the block with a good block
         * and mark the current block bad
         * after copy back the previous written data.
         */
        RETURN(NvError_NandTLFailed);
    }
    return(NvSuccess);
}
#endif
NvError
WriteTAT(NvNandHandle hNand)
{
    NvS32 NumOfBlksForTAT = hNand->ITat.Misc.NumOfBlksForTAT;
    NvS32 CurTATBlk;
    NvError Status = NvSuccess;
    /*
     * this API will only be called by createTranslationTable API and FLush API.
     * This assumes that the TAT blocks are
     * erased and has no data in them.
     */

    // search for the page in system reserved - TAT to write the new TAT
    for (CurTATBlk = 0; CurTATBlk < NumOfBlksForTAT; CurTATBlk++)
    {
        // TatPage is 0 and Write Only header is NV_FALSE.
        Status = WriteTATPage(hNand, CurTATBlk, NV_TRUE, 0, NV_FALSE);
        if (Status == NvSuccess)
            return NvSuccess;
    }
    NvOsDebugPrintf("\r\n writing to TAT blocks failed");
    RETURN (NvError_NandTLFailed);
}
#if NEW_TT_CODE
NvBool
ReplaceTTBadBlock(
    NvNandHandle hNand,
    NvS32 ttLogicalPageNumber,
    NvS32 ttLogicalBlkNum,
    NvS32* CurrentTTBlock,
    NvS32* CurrentTTPage,
    NvS32* badPageCount)
{
    NvU32 i = 0;
    NvS32 NoOfSectors = 0;
    NvU32 Log2PgsPerBlk = 0;
    NvU32 NoOfPagesWritten = 0;
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    NvS32 SourcePageNumbers[4] = {-1, -1, -1, -1};
    NvS32 DstnPageNumbers[4] = {-1, -1, -1, -1};
    BlockInfo * pSrcBlockInfo = NULL;
    BlockInfo * pDestBlockInfo = NULL;
    NvError RetStatus = NvSuccess;

    // Get Log2 of Pages per Block
    Log2PgsPerBlk = NandUtilGetLog2(hNand->NandDevInfo.PgsPerBlk);

    // Current block
    pSrcBlockInfo = &nandILTat->tatHandler.WorkingCopy.TtAllocBlk[ttLogicalBlkNum];
    SourcePageNumbers[pSrcBlockInfo->BnkNum] =
        pSrcBlockInfo->BlkNum << Log2PgsPerBlk;

    NoOfPagesWritten = MACRO_MOD_LOG2NUM(ttLogicalPageNumber, Log2PgsPerBlk);
    NoOfSectors = NoOfPagesWritten << hNand->NandStrat.bsc4SctsPerPg;

    if(ttLogicalPageNumber == 0)
    {
        *CurrentTTBlock = *CurrentTTBlock + 1;
        return NV_TRUE;
    }

    for (i = 1;NV_TRUE;i++)
    {
        // Get the Destination Block
        pDestBlockInfo = &nandILTat->tatHandler.ttBlocks[*CurrentTTBlock + i];
        DstnPageNumbers[pDestBlockInfo->BnkNum] =
            pDestBlockInfo->BlkNum << Log2PgsPerBlk;

        // Copy Back the data from Src to Dest.
        RetStatus = NandTLCopyBack(
            hNand, SourcePageNumbers, DstnPageNumbers,
            pSrcBlockInfo->BnkNum << hNand->NandStrat.bsc4SctsPerPg,
            pDestBlockInfo->BnkNum << hNand->NandStrat.bsc4SctsPerPg,
            &NoOfSectors, NV_TRUE);
        if (RetStatus == NvSuccess)
            break;

        // Mark the Dest as bad if Write is failed.
        MarkBlockAsBad(hNand, pDestBlockInfo->BnkNum, pDestBlockInfo->BlkNum);
    }
    *CurrentTTBlock = *CurrentTTBlock + i;

    // Update the TAT.
    nandILTat->tatHandler.WorkingCopy.TtAllocBlk[ttLogicalBlkNum] =
        nandILTat->tatHandler.ttBlocks[*CurrentTTBlock];
    for (i = 0;i < NoOfPagesWritten;i++)
    {
        nandILTat->tatHandler.WorkingCopy.TtAllocPg[ttLogicalPageNumber + i]
            = (ttLogicalBlkNum << nandILTat->Misc.bsc4PgsPerBlk) + i;
    }
    // Mark the Src Block as bad
    MarkBlockAsBad(hNand, pSrcBlockInfo->BnkNum, pSrcBlockInfo->BlkNum);
    return NV_TRUE;
}
#else
NvBool
ReplaceTTBadBlock(
    NvNandHandle hNand,
    NvS32 ttLogicalPageNumber,
    NvS32 ttLogicalBlkNum,
    NvS32* currentBlock,
    NvS32* currentPage,
    NvS32* badPageCount)
{
    return(NV_TRUE);
}
#endif

NvError
GetPage(NvNandHandle hNand,NvS32 logicalPage, NvS8* ttPage)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    BlockInfo nextTTBlock;
    NvS32 nextTTPage,LBNum,pageOffset,blk,bank,physicalPage2Read,startPage;
    // variable to represent log2 pages-per-block
    NvU32 Log2PgsPerBlk;
    // We should have a working TAT copy if not return
    if (!nandILTat->tatHandler.WorkingCopy.headerSignature || !ttPage)
        RETURN(NvError_NandTTFailed);

    /*
     * if the requested logical page entry is free in TAT,
     * we can not read that page
     */
    if (nandILTat->tatHandler.WorkingCopy.TtAllocPg[logicalPage] == 0xFFFF)
        RETURN(NvError_NandTTFailed);

    /*
     * If the requested page entry is cached, return error. Because if
     * its cached TAT will not have updated data.
     */
    if (nandILTat->tatHandler.WorkingCopy.TtAllocPg[logicalPage] &
    NIT_MASK_DIRTY_BIT)
        RETURN(NvError_NandTTFailed);

    /*
     * we will be here if the requested logical page has an
     * uncached physical entry in the working TAT
     */
    physicalPage2Read =
        nandILTat->tatHandler.WorkingCopy.TtAllocPg[logicalPage];
    if (physicalPage2Read > nandILTat->Misc.LastTTPageUsed)
        RETURN(NvError_NandTTFailed);

    // Calculate the log2 of Pages per block value
    Log2PgsPerBlk = NandUtilGetLog2(hNand->NandDevInfo.PgsPerBlk);
    LBNum = MACRO_DIV_POW2_LOG2NUM(physicalPage2Read, Log2PgsPerBlk);
    pageOffset = MACRO_MOD_LOG2NUM(physicalPage2Read, Log2PgsPerBlk);
    blk = nandILTat->tatHandler.WorkingCopy.TtAllocBlk[LBNum].BlkNum;
    bank = nandILTat->tatHandler.WorkingCopy.TtAllocBlk[LBNum].BnkNum;
    LOG_NAND_DEBUG_INFO(("\r\n get TT logicalPage %d->physical %d, "
        "bank=%d,blk=%d \r\n",
        logicalPage,physicalPage2Read,bank,blk), TAT_DEBUG_INFO);

    ClearPhysicalPageArray(hNand,hNand->ITat.pPhysPgs);
    startPage = (blk << nandILTat->Misc.bsc4PgsPerBlk);
    hNand->ITat.pPhysPgs[bank] = startPage + pageOffset;

    if (NandTLReadPhysicalPage(
        hNand,
        hNand->ITat.pPhysPgs,
        bank << nandILTat->Misc.bsc4SctsPerPg,
        (NvU8 *)ttPage, hNand->NandDevInfo.SctsPerPg, 0,0) != NvSuccess)
    {

        // mark the page as bad in redundant area
        // Initialize spare buffer before tag write
        NvOsMemset(hNand->pSpareBuf, 0x0,
            hNand->gs_NandDeviceInfo.NumSpareAreaBytes);
        WriteTagInfo(
            hNand,
            hNand->ITat.pPhysPgs,
            (bank << nandILTat->Misc.bsc4SctsPerPg),
            hNand->pSpareBuf);
        //ChkDriverErr(NandInterleaveTTable::FAILURE);

        // get next free page entry of TT
        InitBlockInfo(&nextTTBlock);
        GetNextTTpage(hNand, &nextTTBlock,&nextTTPage,NV_TRUE);
        DoAdvancedWrite(hNand, &nextTTBlock, nextTTPage,(NvU8 *)ttPage);
    }
    return(NvSuccess);
}

NvError
FlushCachedTT(NvNandHandle hNand, NvS32 LogicalPageNumber, NvS8* data)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    BlockInfo nextTTBlock;
    NvS32 nextTTPage,LBNum,pageOffset,blk,bank,physicalPage2Write,startPage;
    NvBool flushDone = NV_FALSE;
    NvU32 Log2PgsPerBlk;
    NvError RetStatus = NvSuccess;

    // check the requested logical page is cached or not
    if ((nandILTat->tatHandler.WorkingCopy.TtAllocPg[LogicalPageNumber] &
        NIT_MASK_DIRTY_BIT) != NIT_MASK_DIRTY_BIT) RETURN(NvError_NandTTFailed);

    /*
     * during flush, we need to get a new page from the spare TT page
     * entries and then flush to new page and update the TAT.
     * we do not have any page associated with the requested logical
     * page entry
     */
    flushDone = NV_FALSE;
    InitBlockInfo(&nextTTBlock);
    // Calculate the log2 of Pages per block value
    Log2PgsPerBlk = NandUtilGetLog2(hNand->NandDevInfo.PgsPerBlk);
    do
    {
        // get next free page entry of TT
        GetNextTTpage(hNand, &nextTTBlock, &nextTTPage, NV_FALSE);
        // replace the logicalPage entry with the new TT page
        nandILTat->tatHandler.WorkingCopy.TtAllocPg[LogicalPageNumber] =
            nextTTPage;
        LOG_NAND_DEBUG_INFO(("\r\n pageModified ref. of current logical "
            "page %d is %d,bank=%d,blk=%d",LogicalPageNumber,
            nextTTPage, nextTTBlock.BnkNum,nextTTBlock.BlkNum),
            TAT_DEBUG_INFO);

        //Get the destination page entry to write the cached data
        physicalPage2Write = nextTTPage;
        LBNum = MACRO_DIV_POW2_LOG2NUM(physicalPage2Write, Log2PgsPerBlk);
        pageOffset = MACRO_MOD_LOG2NUM(physicalPage2Write, Log2PgsPerBlk);
        blk = nandILTat->tatHandler.WorkingCopy.TtAllocBlk[LBNum].BlkNum;
        bank = nandILTat->tatHandler.WorkingCopy.TtAllocBlk[LBNum].BnkNum;

        ClearPhysicalPageArray(hNand,hNand->ITat.pPhysPgs);
        startPage = (blk << nandILTat->Misc.bsc4PgsPerBlk);
        hNand->ITat.pPhysPgs[bank] = startPage + pageOffset;

        LOG_NAND_DEBUG_INFO(("\r\n Flushing logical page %d to page %d,"
            "bank=%d,blk=%d\r\n",
            LogicalPageNumber,
            pageOffset,bank,blk),
            TAT_DEBUG_INFO);
        RetStatus = Write(
            hNand,
            hNand->ITat.pPhysPgs,
            bank << nandILTat->Misc.bsc4SctsPerPg,
            (NvU8 *)data, hNand->NandDevInfo.SctsPerPg, NULL);
        if (RetStatus == NvSuccess)
        {
            flushDone = NV_TRUE;
        }
        else
        {
            ReplaceBlockSet(hNand);
            MarkBlockAsBad(hNand, bank,
                hNand->ITat.pPhysPgs[bank] >> hNand->ITat.Misc.bsc4PgsPerBlk);
        }
    } while (!flushDone);
    return(NvSuccess);
}

void
MarkBlockAsBad(NvNandHandle hNand,NvS32 bank, NvS32 physBlk)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    // set the tag info of the pba that it is bad in redundant area
    ClearPhysicalPageArray(hNand,hNand->ITat.pPhysPgs);
    hNand->ITat.pPhysPgs[bank] =
        physBlk << nandILTat->Misc.bsc4PgsPerBlk;
    // erase the block before marking bad
    hNand->ITat.NEStatus = NandTLErase(hNand,hNand->ITat.pPhysPgs);
    // Initialize spare buffer with tag info
    NvOsMemset(hNand->pSpareBuf, 0x0,
        hNand->gs_NandDeviceInfo.NumSpareAreaBytes);
    WriteTagInfo(
        hNand,
        hNand->ITat.pPhysPgs,
        (bank << nandILTat->Misc.bsc4SctsPerPg),
        hNand->pSpareBuf);
    nandILTat->Misc.RemainingDataRsvdBlocks--;
    if (nandILTat->Misc.RemainingDataRsvdBlocks <= 0)
    {
        ERR_DEBUG_INFO(("!!!!!!!!!!!!!!!Out of data rsvd blocks !!!!!!!!!!!!!!!"));
    }
    ERR_DEBUG_INFO(("\r\n Marking pba=%d of bank=%d as bad, Remaining data rsv %d",
                         physBlk, bank, nandILTat->Misc.RemainingDataRsvdBlocks));
}

NvError
FlushTAT(NvNandHandle hNand)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    NvS32 tatBlock;
    // if Current TAT is not modified, there is no need to update
    if (!nandILTat->tatHandler.WorkingCopy.IsDirty) return(NvSuccess);

#if !NEW_CODE
    if ((nandILTat->tatHandler.WorkingCopy.NextTatBlkNum.BnkNum == -1) ||
        (nandILTat->tatHandler.WorkingCopy.NextTatBlkNum.BlkNum == -1))
    {
        // Must not happen. find out.
        RETURN(NvError_NandTTFailed);
    }
#endif
    /*
     * make our page and block point to the address where only
     * header is written.
     */
    nandILTat->tatHandler.WorkingCopy.TatPgNum =
        nandILTat->tatHandler.WorkingCopy.NextTatPgNum;
#if !NEW_CODE
    nandILTat->tatHandler.WorkingCopy.TatBlkNum =
        nandILTat->tatHandler.WorkingCopy.NextTatBlkNum;
#endif
    /*
     * Write new TAT info to Nand Flash. this function will flush
     * into the page which is next to
     * header signature page.
     */
    WriteTATSignature(hNand, NV_FALSE);

    // Modify the next TATinfo
    nandILTat->tatHandler.WorkingCopy.NextTatPgNum = -1;
#if !NEW_CODE
    SetBlockInfo(&nandILTat->tatHandler.WorkingCopy.NextTatBlkNum, -1, -1);
#endif
    if (hNand->ITat.RevNumRollOverAboutToHappen)
    {

        LOG_NAND_DEBUG_INFO(("\r\n hNand->ITat.RevNumRollOverAboutToHappen"),
            ERROR_DEBUG_INFO);
        // Need to write TAT to a new block and erase all the other TAT blocks.
        hNand->ITat.RevNumRollOverAboutToHappen = NV_FALSE;
        //NandInterleaveTTable::BlockInfo workingTATBlockInfo;

        LOG_NAND_DEBUG_INFO(("\r\n erasing all TAT blocks"), ERROR_DEBUG_INFO);
        // erase all the TAT blocks except the working copy.
        for (
            tatBlock = 0;
            tatBlock < nandILTat->Misc.NumOfBlksForTAT;
            tatBlock++)
        {

            if (!IsBlockBad(hNand,
                &nandILTat->tatHandler.tatBlocks[tatBlock],
                NV_FALSE))
            {

                if (DoAdvancedErase(
                    hNand,
                    &nandILTat->tatHandler.tatBlocks[tatBlock],
                    NV_TRUE) != NvSuccess)
                {

                    MarkBlockAsBad(
                        hNand,
                        nandILTat->tatHandler.tatBlocks[tatBlock].BnkNum,
                        nandILTat->tatHandler.tatBlocks[tatBlock].BlkNum);
                }
            } else
            {
                LOG_NAND_DEBUG_INFO(("\r\n TAT block bad blk=%d, bank=%d,"
                    "skipping erase for this block",
                    nandILTat->tatHandler.tatBlocks[tatBlock].BlkNum,
                    nandILTat->tatHandler.tatBlocks[tatBlock].BnkNum),
                    ERROR_DEBUG_INFO);
            }
        }

        // set the revision number to 0.
        *nandILTat->tatHandler.WorkingCopy.RevNum = 0;
        LOG_NAND_DEBUG_INFO(("\r\n RevNum set to 0x%x and written the "
            "TAT page", *nandILTat->tatHandler.WorkingCopy.RevNum),
            ERROR_DEBUG_INFO);
        WriteTAT(hNand);
    }

    return(NvSuccess);
}

NvError
PageModified(NvNandHandle hNand,NvS32 LogicalPageNumber)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    /*
     * Just in case we already made it dirty and some body is still giving
     * this notification, just return that we are done
     */
    if (nandILTat->tatHandler.WorkingCopy.TtAllocPg[LogicalPageNumber] &
        NIT_MASK_DIRTY_BIT) return(NvSuccess);

    // make the logical page dirty
    nandILTat->tatHandler.WorkingCopy.TtAllocPg[LogicalPageNumber] |=
        NIT_MASK_DIRTY_BIT;

    /*
     * if TAT is not marked as dirty, write TAT signature to the next TAT page
     * to indicate that TAT became dirty
     */
    WriteTATSignature(hNand, NV_TRUE);
    return(NvSuccess);
}

void
ClearPhysicalPageArray(NvNandHandle hNand,NvS32* pPages)
{
    NvS32 i;
    for (i = 0; i < (NvS32)hNand->ITat.Misc.NoOfILBanks; i++)
        pPages[i] = -1;
}

NvS32
GetTTableBlockStatusEntryNumber(NvNandHandle hNand,
    NvS32 bank, NvS32 ttEntry)
{
    return((ttEntry << hNand->ITat.Misc.bsc4NoOfILBanks) + bank);
}

/**
 * @brief Initializes the nand interleave structure.
 *
 * @param nDriver pointer to Nand driver.
 * @param memProp memory properties to use for any memory allocation.
 * @param tt pointer to translation table.
 */
NvError
InitNandInterleaveTAT(NvNandHandle hNand)
{
    NandInterleaveTAT *nandILTat = &hNand->ITat;
    NvU32 i;
    NandDeviceInfo*pDevInfo = &(hNand->NandDevInfo);
    NvError RetStatus = NvSuccess;
    hNand->ITat.pBadBlkPerZone = 0;
    hNand->ITat.pPhysPgs = 0;
    hNand->ITat.pWritePhysPgs =0;
    hNand->ITat.RevNumRollOverAboutToHappen = NV_FALSE;
    hNand->ITat.MaxNoOfBadPages = 0;
    LOG_NAND_DEBUG_INFO(("\r\n constructing NandInterleaveTAT Instance"),
        TAT_DEBUG_INFO);

    hNand->FbbPage = AllocateVirtualMemory(pDevInfo->PageSize,TAT_ALLOC);
    hNand->CachedFbbPage = -1;
    nandILTat->tatHandletInitialized = NV_FALSE;
    nandILTat->Misc.TagInfoPage = 0;
    nandILTat->Misc.SysRsvdBlkStartPage = 1;
    nandILTat->Misc.SysRsvdBlkEndPage = pDevInfo->PgsPerBlk - 1;
    nandILTat->Misc.PagesPerSysRsvdBlock =
        nandILTat->Misc.SysRsvdBlkEndPage -
        nandILTat->Misc.SysRsvdBlkStartPage + 1;

    nandILTat->Misc.NumOfBanksOnBoard = hNand->NandCfg.BlkCfg.NoOfBanks;
    nandILTat->Misc.NoOfILBanks = hNand->InterleaveCount;
    nandILTat->Misc.PhysBlksPerZone = hNand->NandCfg.BlkCfg.PhysBlksPerZone;
    nandILTat->Misc.PhysBlksPerLogicalBlock = hNand->InterleaveCount;

    // We get the device capacity in KB. So multiply by 1024.
    if (pDevInfo->BlkSize / 1024)
    {
        nandILTat->Misc.TotEraseBlks =
            (pDevInfo->DeviceCapacity * nandILTat->Misc.NumOfBanksOnBoard) /
            (pDevInfo->BlkSize / 1024);
    } else
    {
        nandILTat->Misc.TotEraseBlks =
            ((pDevInfo->DeviceCapacity * nandILTat->Misc.NumOfBanksOnBoard) *
            (NvU32)1024) / pDevInfo->BlkSize;
    }

    nandILTat->Misc.PhysBlksPerBank =
        nandILTat->Misc.TotEraseBlks/nandILTat->Misc.NumOfBanksOnBoard;
    nandILTat->Misc.ZonesPerBank =
        (nandILTat->Misc.TotEraseBlks / nandILTat->Misc.NumOfBanksOnBoard) /
        nandILTat->Misc.PhysBlksPerZone;

    /*
     * each erase block will have an entry in the translation table.
     * so in total we need TotEraseBlks * BlockStatusEntry bits of memory
     */
    nandILTat->Misc.PgsRegForTT =
        ( (nandILTat->Misc.TotEraseBlks * sizeof(BlockStatusEntry)) +
         (((NvU16)pDevInfo->PageSize) - 1) ) / ((NvU16)pDevInfo->PageSize);
    nandILTat->Misc.TtPagesRequiredPerZone =
        (nandILTat->Misc.PgsRegForTT /(nandILTat->Misc.ZonesPerBank *
        nandILTat->Misc.NumOfBanksOnBoard)) *
        nandILTat->Misc.NoOfILBanks;

    /*
     * Calculate blocks required for working TT
     * TT pages required is always rounded to upper block boundary after
     * quadruple the actual pages required to write TT.
     */
    nandILTat->Misc.BlksRequiredForTT =
        ((nandILTat->Misc.PgsRegForTT * 8) +
        (nandILTat->Misc.PagesPerSysRsvdBlock-1)) /
        nandILTat->Misc.PagesPerSysRsvdBlock;
    nandILTat->Misc.PgsAlloctdForTT =
        nandILTat->Misc.BlksRequiredForTT * pDevInfo->PgsPerBlk;
    nandILTat->Misc.ExtraPagesForTTMgmt =
        (nandILTat->Misc.BlksRequiredForTT * pDevInfo->PgsPerBlk) -
        nandILTat->Misc.PgsRegForTT;

    // pDevInfo->PgsPerBlk value must always be in powers of 2.
    nandILTat->Misc.bsc4PgsPerBlk = (NvS32)NandUtilGetLog2(pDevInfo->PgsPerBlk);
    nandILTat->Misc.bsc4NoOfILBanks = (NvS32)NandUtilGetLog2(nandILTat->Misc.NoOfILBanks);
    nandILTat->Misc.bsc4SctsPerPg = (NvS32)NandUtilGetLog2(pDevInfo->SctsPerPg);
    nandILTat->Misc.bsc4SectorSize = (NvS32)NandUtilGetLog2(pDevInfo->SectorSize);
    if(     !nandILTat->Misc.bsc4PgsPerBlk
        ||  !nandILTat->Misc.bsc4SctsPerPg
        ||  !nandILTat->Misc.bsc4SectorSize)
        {
            RETURN(NvError_NandTTFailed);
        }
    hNand->ITat.pBadBlkPerZone =
        (NvS8 *)AllocateVirtualMemory(sizeof(NvS8) *
        nandILTat->Misc.NumOfBanksOnBoard *
        nandILTat->Misc.ZonesPerBank,TAT_ALLOC);
    hNand->ITat.pPhysPgs =
        (NvS32 *)AllocateVirtualMemory(
        sizeof(NvS32) * nandILTat->Misc.NoOfILBanks, TAT_ALLOC);
    hNand->ITat.pWritePhysPgs =
        (NvS32 *)AllocateVirtualMemory(sizeof(NvS32) *
        nandILTat->Misc.NoOfILBanks, TAT_ALLOC);

    // initialize the bad block per zone count array
    for (i=0;i<(nandILTat->Misc.NumOfBanksOnBoard *
        nandILTat->Misc.ZonesPerBank);i++)
    {
        hNand->ITat.pBadBlkPerZone[i] = 0;
    }
    if(hNand->FtlEndPba == -1)
    {
        if(nandILTat->Misc.NoOfILBanks == 1)
            hNand->FtlEndPba = nandILTat->Misc.TotEraseBlks;
        else
            hNand->FtlEndPba = nandILTat->Misc.PhysBlksPerBank;
    }
    nandILTat->Misc.TotalPhysicalBlocks =
        (hNand->FtlEndPba -  hNand->FtlStartPba) * nandILTat->Misc.NoOfILBanks;

    RetStatus = Initialize(hNand);
    if (RetStatus != NvSuccess)
    {
        LOG_NAND_DEBUG_INFO(("\r\n initialization failed"), TAT_DEBUG_INFO);
        RETURN(RetStatus);
    }

    /*
     * this function dumps the Misc and TAT handler info to logger,
     * if logger is enabled.
     */
    DumpMiscAndTATHandlerInfoToLogger(hNand);
    LOG_NAND_DEBUG_INFO(("\r\n constructing NandInterleaveTAT Instance - end"),
        TAT_DEBUG_INFO);
    return NvSuccess;
}

void DeInitNandInterleaveTAT(NvNandHandle hNand)
{
    ReleaseVirtualMemory(hNand->ITat.tatHandler.wBuffer, TAT_ALLOC);
    ReleaseVirtualMemory(hNand->ITat.tatHandler.tatBlocks, TAT_ALLOC);
    ReleaseVirtualMemory(hNand->ITat.tatHandler.ttBlocks, TAT_ALLOC);
    ReleaseVirtualMemory(hNand->FbbPage, TAT_ALLOC);
    ReleaseVirtualMemory(hNand->ITat.pBadBlkPerZone, TAT_ALLOC);
    ReleaseVirtualMemory(hNand->ITat.pPhysPgs, TAT_ALLOC);
    ReleaseVirtualMemory(hNand->ITat.pWritePhysPgs, TAT_ALLOC);
}

