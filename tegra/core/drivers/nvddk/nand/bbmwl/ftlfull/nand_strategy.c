
/**
 * @file
 * @brief Implements the sequential read writes to nand flash
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

#include "nand_strategy.h"
#include "nvos.h"
#include "nvnandtlfull.h"
//#include "nandconfiguration.h"

// Set Folowing Macro to 1 for using Reserved PBAs only for replacement
// default it should be 0.
#define USE_RSVD_PBA_ONLY 0
#define STRESS_TEST_GETALIGNEDPBA 0

typedef struct CopyBackHandlerRec
{
    NvU32* pSrcPageNumbers;
    NvU32* pDestPageNumbers;
    NvU32* ppNewDestPageNumbers;
    PageInfo* pTrackingBuf;
}CopyBackHandler;

typedef struct CopyBackIntHandlerRec
{
    NvU32* pSrcPageNumbers;
    NvU32* pDestPageNumbers;
    NvU32* pNewDestPageNums;
    PageInfo* pTrackingBuf;
    NvU8 State;
    NvBool IgnoreECCError;
}CopyBackIntHandler;

typedef enum
{
    CopyBackMode_READ_FAIL,
    CopyBackMode_READ_SRC_FAIL,
    CopyBackMode_READ_DEST_FAIL,
    CopyBackMode_WRITE_FAIL,
    CopyBackMode_FLUSH_SRC_DEST,
    CopyBackMode_FLUSH_DEST_MERGE,
    CopyBackMode_FLUSH_SRC_DEST_MERGE,
    CopyBackMode_Force32 = 0x7FFFFFFF
}CopyBackMode;

// --------------------- Function prototypes -------------------------
/**
 * @brief This method tells whether the lba is in tracking list or not.
 * if it is in the tracking list, index will give the position
 * in the lbas tracking array.
 *
 * @param lba lba to check for.
 * @param index returns index of the lba, if it exists in tracking list.
 *
 * @return returns NV_TRUE, if the lba is in tracking list.
 */
NvBool NandStrategyIsLbaInTrackingList(
    NvNandHandle hNand,
    NvS32 lba,
    NvS32* index);

/**
 * @brief This method adds the specified lba to tracking list.
 *
 * @param lba logical block address
 * @param pBlocks pointer to newly allocated blocks.
 * @param pPrevBlocks pointer to previously allocated blocks.
 *
 * @return returns the index after adding the lba to tracking list.
 */
NvS32 NandStrategyAddLba2TrackingList(
    NvNandHandle hNand,
    NvS32 lba,
    NvS32* pBlocks,
    NvS32* pPrevBlocks);

/**
 * @brief This method removes the lba specified by the index.
 *
 * @param index index of lba in the tracking list.
 */
void NandStrategyRemoveLbaFromTrackingList(NvNandHandle hNand, NvS32 index);

/**
 * @brief This method flushes the lba specified by the index to the nand flash.
 *
 * @param index lba's index in the track list.
 *
 * @return returns the status.
 */
NvError NandStrategyFlushLbaTrackingList(NvNandHandle hNand, NvS32 index);

/**
 * @brief This method tells whether the lba has free pages.
 *
 * @param index lba's index in the track list.
 *
 * @return returns NV_TRUE, if the lba has free pages.
 */
NvBool NandStrategyHasLbaFreePages(NvNandHandle hNand, NvS32 index);

/**
 * @brief clears the physical page array.
 *
 * @param pPages pointer to the array of physical pages.
 */
void NandStrategyclearPhysicalPageArray(NvNandHandle hNand, NvS32* pPages);

/**
 * @brief This method converts the block number to physical number.
 *
 * @param pBlocks array containing block numbers.
 * @param pPages array to hold the page numbers after conversion
 * from block to page no.
 * @param pageOffset page offset in a block.
 *
 */
void NandStrategyConvertBlockNoToPageNo(
    NvNandHandle hNand,
    NvS32* pBlocks,
    NvS32* pPages,
    NvS32 pageOffset);

/**
 * @brief This method replaces a bad block with a good block.
 * It copies the data from bad block to a good block and marks bad
 * block as bad in TT and returns good block number.
 *
 * @param bank bad block's bank number and returns the
 * block number it is replaced with.
 * @param badBlock bad block number.
 * @param startPageOffset startpage offset for copyback during block replacement.
 * @param noOfPages no of pages to copyback during block replacement.
 */
NvBool NandStrategyReplaceBadBlock(
    NvNandHandle hNand,
    NvS32 bank,
    NvS32* badBlock,
    NvS32 startPageOffset,
    NvS32 noOfPages);

/**
 * @brief This method checks whether only read error occurred during copyback.
 *
 * @param pErrorInfo Pointer to error info structure,
 * which has error info from nand driver.
 *
 * @return returns NV_TRUE, if only read operation fails.
 */
NvBool NandStrategyIsOnlyReadFailed(void);

/**
 * @brief This method Verifies whether the block is bad or not.
 * verification is done by writing a known data pattern to the
 * block and reading it back.
 *
 * @param bank bank number of the block to be verified.
 * @param block block number to verify.
 *
 * @return returns NV_TRUE, if the block is really bad.
 */
NvBool NandStrategyIsBlockBad(NvNandHandle hNand, NvS32 bank, NvS32 block);

/**
 * @brief This method verifies the block and marks it as the bad one, if needed.
 * If the verification says that the block is not bad, then it will
 * mark the block as unused and reserved.
 *
 * @param bank bank number of the block to be verified and marked.
 * @param block block number.
 *
 * @return returns the status.
 */
NvError NandStrategyMarkBlockAsBad(
    NvNandHandle hNand,
    NvS32 bank,
    NvS32 block);

/*
 *StrategyPrivDoCopyback handles all error cases that happens while doing read 
 * write operations. This function also handles all copy back operations that 
 * happens in FTL layer.
 * This function should handle all error cases of copy back.
 * @param pSrcBlock: pointer to source block.
 * @param pDestBlock: pointer to destination block.
 * @param pNewDestBlock: pointer to new destination block.
 * @param pTrackingBuffer: pointer to tracking buffer
 * @param NumberOfPages: Number of pages to be copied back. if it is set to 
 * "-1" then get the required data from tracking buffer.
 * @param SrcBlkPageOffset: PageOffset in Source block.
 * @param DstBlkPageOffset: PageOffset in Destination block.
 * @param SrcSectorpgOffset: Sector Offset in page of Source Block. (bank)
 * @param DstSectorpgOffset: Sector Offset in page of Dest Block. (bank)
 */
NvError 
    StrategyPrivDoCopyback(
        NvNandHandle hNand,
        CopyBackHandler * pCpyBakHandler,
        NvBool IsWriteRequestRandom,
        CopyBackMode CpyBakMode);

void TrackLbaInitialize(
    TrackLba* parent,
    NvS32 NoOfInterleavedBanks,
    NandDeviceInfo *pDevInfo);

void TrackLbaSetLbaInfo(
    TrackLba* parent,
    NvS32 lba,
    NvS32* pPrevBlocks,
    NvS32* pBlocks);

NvBool TrackLbaIsPages2WriteFree(
    TrackLba* parent,
    NvS32 lPageOffset,
    NvS32 lPageSectorOffset,
    NvS32 noOfSectors);

NvError TrackLbaMarkPages2WriteBusy(
    NvNandHandle hNand,
    TrackLba* parent,
    NvS32 lPageOffset,
    NvS32 lPageSectorOffset,
    NvS32 noOfSectors);

void TrackLbaMarkPages2WriteFree(
    TrackLba* parent,
    NvS32 lPageOffset,
    NvS32 lPageSectorOffset,
    NvS32 noOfSectors);

NvBool TrackLbaGetConsecutivePageStatus(
    TrackLba* parent,
    NvS32 startPageOffset,
    NvS32 endPageOffset,
    NvS32* logicalBlockPageOffset,
    NvS32* logicalPageSectorOffset,
    NvS32* pageCount,
    NvS32* sectorCount,
    NvS32 state);

void NandStrategyConvertPageNoToBlockNo(
    NvNandHandle hNand,
    NvS32* pBlocks,
    NvS32* pPages);

static NvU32 GetNoOfContinuousPages(
    PageInfo * pPageState,
    NvS32 NoOfPages,
    NvS32 StartPageOffset);

NvError MergeSrcNDest(
    NvNandHandle hNand, 
    CopyBackIntHandler * pCpyBakIntHandler, 
    NvBool IsWriteRequestRandom);

NvError CopySrcToDest(
    NvNandHandle hNand, 
    CopyBackIntHandler * pCpyBakIntHandler, 
    NvBool IsWriteRequestRandom);
#if STRESS_TEST_GETALIGNEDPBA
static void StressTestGetAlignedPBA(NvNandHandle hNand);
#endif
void MarkPBAsetAsBad(NvNandHandle hNand, NvU32* pPages);
// ---------------------------------------------------------------------------

// initialize method must be called before using this STRUCTURE.
NvError NandStrategyInitialize(NvNandHandle hNand)
{
    NvError status = NvSuccess;
    NvS32 i;

    // set required variables to their defaults
    hNand->NandStrat.NoOfILBanks = hNand->InterleaveCount;
    hNand->NandStrat.NumberOfBanksOnboard = hNand->NandCfg.BlkCfg.NoOfBanks;
    hNand->NandStrat.SecsPerLBlk = 0;
    hNand->NandStrat.PgsPerLBlk = 0;
    hNand->NandStrat.SctsPerLPg = 0;
    hNand->NandStrat.pTmpPhysPgs = 0;
    hNand->NandStrat.pCopybackSrcPages = 0;
    hNand->NandStrat.pCopybackDstnPages = 0;
    hNand->NandStrat.pTempBlocks = 0;
    hNand->NandStrat.pPhysBlks = 0;
    hNand->NandStrat.pPrevPhysBlks = 0;
    hNand->NandStrat.pTrackLba = 0;
    hNand->NandStrat.TagInfoPage = 0;
    hNand->NandStrat.NoOfLbas2Track = 0;
    hNand->NandStrat.bsc4PgsPerLBlk = 0;
    hNand->NandStrat.bsc4PgsPerBlk = 0;
    hNand->NandStrat.bsc4SecsPerLBlk = 0;
    hNand->NandStrat.bsc4SctsPerLPg = 0;
    hNand->NandStrat.bsc4SctsPerPg = 0;
    hNand->NandStrat.bsc4NoOfILBanks = 0;
    hNand->NandStrat.PhysBlksPerZone = 0;

    NvOsMemset(&hNand->NandStrat.TagInfo, 0xFF ,sizeof(TagInformation));
    hNand->NandStrat.pTmpPhysPgs =
        AllocateVirtualMemory(
            sizeof(NvS32) * hNand->NandStrat.NoOfILBanks,
            STRATEGY_ALLOC);
    hNand->NandStrat.pCopybackSrcPages =
        AllocateVirtualMemory(
            sizeof(NvS32) * hNand->NandStrat.NoOfILBanks,
            STRATEGY_ALLOC);
    hNand->NandStrat.pCopybackDstnPages =
        AllocateVirtualMemory(
            sizeof(NvS32) * hNand->NandStrat.NoOfILBanks,
            STRATEGY_ALLOC);
    hNand->NandStrat.pPhysBlks =
        AllocateVirtualMemory(
            sizeof(NvS32) * hNand->NandStrat.NoOfILBanks,
            STRATEGY_ALLOC);
    hNand->NandStrat.pPrevPhysBlks =
        AllocateVirtualMemory(
            sizeof(NvS32) * hNand->NandStrat.NoOfILBanks,
            STRATEGY_ALLOC);
    hNand->NandStrat.pTempBlocks =
        AllocateVirtualMemory(
            sizeof(NvS32) * hNand->NandStrat.NoOfILBanks,
            STRATEGY_ALLOC);
    hNand->NandStrat.NoOfLbas2Track = NUMBER_OF_LBAS_TO_TRACK;

    if (!hNand->NandStrat.pTmpPhysPgs ||
    !hNand->NandStrat.pCopybackSrcPages ||
    !hNand->NandStrat.pCopybackDstnPages ||
    !hNand->NandStrat.pPhysBlks ||
    !hNand->NandStrat.pPrevPhysBlks ||
    !hNand->NandStrat.pTempBlocks)
    {
        ERR_DEBUG_INFO(("\r\n NandStrategyInitialize: Mem alloc failed"));
    }
    else
    {
        status = NandTLGetDeviceInfo(hNand, 0, &hNand->NandStrat.NandDevInfo);
        if (status != NvSuccess)
        {
            ERR_DEBUG_INFO(("\r\n NandStrategyInitialize: get device info failed"));
            RETURN(status);
        }

        hNand->NandStrat.SecsPerLBlk =
            (hNand->NandStrat.NandDevInfo.SectorsPerBlock *
            hNand->NandStrat.NoOfILBanks);
        hNand->NandStrat.PgsPerLBlk =
            (hNand->NandStrat.NandDevInfo.PgsPerBlk *
            hNand->NandStrat.NoOfILBanks);
        hNand->NandStrat.SctsPerLPg =
            (hNand->NandStrat.NandDevInfo.SctsPerPg *
            hNand->NandStrat.NoOfILBanks);

        if (hNand->NandStrat.NandDevInfo.DeviceType == SLC)
            hNand->NandStrat.IsOrderingValid = NV_TRUE;
        else if(hNand->NandStrat.NandDevInfo.DeviceType == MLC)
            hNand->NandStrat.IsOrderingValid = NV_TRUE;

        hNand->NandStrat.bsc4PgsPerBlk =
            (NvS32)NandUtilGetLog2(hNand->NandStrat.NandDevInfo.PgsPerBlk);
        hNand->NandStrat.bsc4PgsPerLBlk =
            (NvS32)NandUtilGetLog2(hNand->NandStrat.PgsPerLBlk);
        hNand->NandStrat.bsc4SecsPerLBlk =
            (NvS32)NandUtilGetLog2(hNand->NandStrat.SecsPerLBlk);
        hNand->NandStrat.bsc4SctsPerLPg =
            (NvS32)NandUtilGetLog2(hNand->NandStrat.SctsPerLPg);
        hNand->NandStrat.bsc4SctsPerPg =
            (NvS32)NandUtilGetLog2(hNand->NandStrat.NandDevInfo.SctsPerPg);
        hNand->NandStrat.bsc4NoOfILBanks =
            (NvS32)NandUtilGetLog2(hNand->NandStrat.NoOfILBanks);
        hNand->NandStrat.PhysBlksPerZone = hNand->NandCfg.BlkCfg.PhysBlksPerZone;

        if (!hNand->NandStrat.bsc4PgsPerBlk ||
            !hNand->NandStrat.bsc4SecsPerLBlk ||
            !hNand->NandStrat.bsc4SctsPerLPg ||
            !hNand->NandStrat.bsc4SctsPerPg)
        {
            RETURN(NvError_NandTLFailed);
        }

        if (hNand->NandStrat.NoOfLbas2Track)
        {
            hNand->NandStrat.pTrackLba =
                AllocateVirtualMemory(
                    sizeof(TrackLba) * hNand->NandStrat.NoOfLbas2Track,
                    STRATEGY_ALLOC);

            for (i = 0; i < hNand->NandStrat.NoOfLbas2Track; i++)
            {
                TrackLbaInitialize(
                    &hNand->NandStrat.pTrackLba[i],
                    hNand->NandStrat.NoOfILBanks,
                    &hNand->NandStrat.NandDevInfo);
            }
        }
        hNand->pNandTt =
            AllocateVirtualMemory(sizeof(NvNandTt), STRATEGY_ALLOC);

        status = InitNandInterleaveTTable(hNand);
        if (status != NvSuccess)
            status = NvError_NandTLFailed;
    }
    return status;
}

/*
 * This function counts the no of continuous pages.
 */
static NvU32 GetNoOfContinuousPages(
    PageInfo * pPageState,
    NvS32 NoOfPages,
    NvS32 StartPageOffset)
{
    NvS32 i = 0;
    NvU32 Count = 1;

    NV_ASSERT(NoOfPages);
    for (i = StartPageOffset; i < (StartPageOffset + NoOfPages - 1); i++)
    {
        if ((pPageState[i+1].PageNumber - pPageState[i].PageNumber) == 1)
            Count++;
        else
            break;
    }
    return Count;
}

static void
NandSanityCheckPageNum(
    NvNandHandle hNand,
    NvS32* pBlocks, NvBool *FlagNoPage)
{
    NvS32 bank;
    *FlagNoPage = NV_TRUE;
    for (bank = 0; bank < hNand->NandStrat.NoOfILBanks; bank++)
    {
        if (pBlocks[bank] >= 0)
        {
            if (*FlagNoPage == NV_TRUE)
                *FlagNoPage = NV_FALSE;
        }
    }
}

NvError NandStrategyGetSectorPageToWrite(
    NvNandHandle hNand,
    NvU32 reqSectorNumber,
    NvU32 reqSectors,
    NvS32* pPhysPgs,
    NvS32* logicalPageSectorOffset,
    NvS32* sectors2Write,
    NvU8** pTagBuffer)
{
    NvS32 logicalBlock2Write =
        reqSectorNumber >> hNand->NandStrat.bsc4SecsPerLBlk;
    NvU32 logicalBlockPageOffset =
        (reqSectorNumber % hNand->NandStrat.SecsPerLBlk) >>
        hNand->NandStrat.bsc4SctsPerLPg;
    NvS32 TmplogicalPgeStrOffset = 0;
    NvBool physBlksAssociated2lba = NV_FALSE;
    NvS32 availableSectors;
    NvS32 i, index;
    NvBool FlagNoPage;

    // Page Offset varies for MLC and SLC
    // It is the page offset in physical block.
    NvU32 PageOffset = 0;
    NvS32 PageBoundary;
    // Get mapped Lba
    NvS32 MappedLba;
    // Local Region number array
    NvU32 TmpRegNum[MAX_ALLOWED_INTERLEAVE_BANKS];
    NvBool IsNonZeroRegion = NV_FALSE;

    if (!(hNand->NandStrat.IsOrderingValid))
        PageOffset = logicalBlockPageOffset;

    *logicalPageSectorOffset =
        (reqSectorNumber % hNand->NandStrat.SctsPerLPg);
    // Fill all positions of all arrays with -1.
    // '-1' is not a valid physical block or page.
    NandStrategyclearPhysicalPageArray(hNand, pPhysPgs);
    NandStrategyclearPhysicalPageArray(hNand, hNand->NandStrat.pPhysBlks);
    NandStrategyclearPhysicalPageArray(hNand, hNand->NandStrat.pPrevPhysBlks);

    // Tag area lba needs to be added with ftlStart
    MappedLba = logicalBlock2Write;
    MapLBA(hNand, &MappedLba);
    // check whether we are writing the tag info page, if so we need to
    // write the tag info We don't need to write tag info, unless the
    // block is bad. so, it is commented.
    if (logicalBlockPageOffset == 0)
    { // logicalBlockPageOffset == hNand->NandStrat.TagInfoPage) {
        // set the tag info of the pba with the lba in redundant area
        NvOsMemset(&hNand->NandStrat.TagInfo, 0xFF ,sizeof(TagInformation));
        hNand->NandStrat.TagInfo.LogicalBlkNum = MappedLba;
        *pTagBuffer = (NvU8*)&hNand->NandStrat.TagInfo;
    }
    else
    {
        *pTagBuffer = 0;
    }

    // find out the max sectors that can be written to the logical block
    // in a single stretch.
    availableSectors = hNand->NandStrat.SecsPerLBlk -
        ((logicalBlockPageOffset << hNand->NandStrat.bsc4SctsPerLPg) +
        *logicalPageSectorOffset);
    *sectors2Write = (reqSectors >= (NvU32)availableSectors) ?
        availableSectors : reqSectors;

    if(*pTagBuffer)
        *sectors2Write = hNand->NandStrat.NandDevInfo.SctsPerPg;
    
    LOG_NAND_DEBUG_INFO(("\r\n\r\n GetWriteSec:: reqSectorNumber:%d; "
        "reqSectors:%d; LBA:%d; "
        "Page:%d; SectorOff:%d; availableSectors:%d; LSAOffset = %d",
         reqSectorNumber, reqSectors, logicalBlock2Write,
         logicalBlockPageOffset, *logicalPageSectorOffset,
         availableSectors,
         reqSectorNumber%hNand->NandStrat.SecsPerLBlk),
         IL_STRAT_WRITE_DEBUG_INFO);
     RW_TRACE(("\n ***Wr: LBA = %d, SectrOffset = %d, PhysPgOffset = %d"
        "Num pages = %d\n", logicalBlock2Write, *logicalPageSectorOffset,
        logicalBlockPageOffset, *sectors2Write / hNand->NandDevInfo.SctsPerPg));

    {
        // Is LBA present in the Tracking list?
        // If yes, check whether the area we are trying to write is free.

        if (NandStrategyIsLbaInTrackingList(hNand, logicalBlock2Write, &index))
        {
            LOG_NAND_DEBUG_INFO(("\r\n GetWriteSec: LBA is in tracking list"),
                IL_STRAT_WRITE_DEBUG_INFO);
            RW_TRACE(("\r\n Write: LBA %d in Tracking List", logicalBlock2Write));
            /*if (TrackLbaIsPages2WriteFree(
                &hNand->NandStrat.pTrackLba[index],
                logicalBlockPageOffset,
                *logicalPageSectorOffset,
                *sectors2Write))*/
                PageBoundary = hNand->NandStrat.pTrackLba[index].FirstFreePage +
                    (*sectors2Write >> hNand->NandStrat.bsc4SctsPerPg);
                
            if (PageBoundary <= hNand->NandStrat.PgsPerLBlk)
            {
                // the pages we are trying to write are free.
                // so mark them as busy and write them.
                if (hNand->NandStrat.IsOrderingValid)
                {
                    // PageOffset would be the first Free page.
                    PageOffset = 
                        (hNand->NandStrat.pTrackLba[index].FirstFreePage >>
                        hNand->NandStrat.bsc4NoOfILBanks);
                    // TmplogicalPgeStrOffset would be different from logicalPageSectorOffset
                    // This is becasue the offset in which the request is 
                    // made may not be the offset in which it actually writes
                    TmplogicalPgeStrOffset = 
                        (hNand->NandStrat.pTrackLba[index].FirstFreePage % 
                        hNand->NandStrat.NoOfILBanks) << 
                        hNand->NandStrat.bsc4SctsPerPg;
                }

                NandStrategyConvertBlockNoToPageNo(
                    hNand, hNand->NandStrat.pTrackLba[index].pBlocks,
                    pPhysPgs, PageOffset);
                NandSanityCheckPageNum(hNand, pPhysPgs, &FlagNoPage);
                if (FlagNoPage)
                {
                    NV_ASSERT(NV_FALSE);
                    NvOsDebugPrintf("\nError: NandStrategyGetSectorPageToWrite InTracking case, No Page ");
                }

                hNand->NandStrat.selfStatus =
                    TrackLbaMarkPages2WriteBusy(
                        hNand, &hNand->NandStrat.pTrackLba[index],
                        logicalBlockPageOffset, *logicalPageSectorOffset,
                        *sectors2Write);
                CheckNvError(hNand->NandStrat.selfStatus, NvError_NandTLFailed);

                if (hNand->NandStrat.IsOrderingValid)
                {
                    *logicalPageSectorOffset = TmplogicalPgeStrOffset;
                }

                RW_TRACE(("\r\n pages to write r free"));
                RW_TRACE(("\r\n Dest PBAs 2 write: "));
                for (i = 0; i < hNand->NandStrat.NoOfILBanks; i++)
                    RW_TRACE(("%d ", hNand->NandStrat.pTrackLba[index].pBlocks[i]));
                RW_TRACE(("\r\n write end"));
                return NvSuccess;
            }
            else
            {
                RW_TRACE(("\r\n pages to write r not free"));
                // flush and remove this lba from tracking.
                // After flushing, new PBA's will be assaigned to LBA in
                // the NandStrategyFlushLbaTrackingList method.
                hNand->NandStrat.selfStatus =
                    NandStrategyFlushLbaTrackingList(hNand, index);
                CheckNvError(hNand->NandStrat.selfStatus, 
                    hNand->NandStrat.selfStatus);
            }
        }
    }

    // LBA is not present in the tracking list.
    // get the actual physical block(s) to which our lba is pointing to
    hNand->NandStrat.ttStatus = GetPBA(
        hNand, logicalBlock2Write, hNand->NandStrat.pPrevPhysBlks);
    ChkILTTErr(hNand->NandStrat.ttStatus, hNand->NandStrat.ttStatus);
    if (hNand->IsNonZeroRegion)
    {
        IsNonZeroRegion = NV_TRUE;
        // Save Region numbers
        NvOsMemcpy(TmpRegNum, hNand->RegNum, (MAX_ALLOWED_INTERLEAVE_BANKS *
            sizeof(NvU32)));
    }

    ERR_DEBUG_INFO(("\r\n GetWriteSec:LBA  assosiated PBAs:"));
    for (i = 0; i < hNand->NandStrat.NoOfILBanks; i++)
        RW_TRACE(("%d ",hNand->NandStrat.pPrevPhysBlks[i]));

    // check whether we have a pba(s) associated to lba. checking the 0th
    // position is enough. we associate pba's from all the interleave banks.
    // if we fail to associate from any of the bank, then we will not
    // have pba(s) associated to lba at all.
    if (hNand->NandStrat.pPrevPhysBlks[0] != -1)
        physBlksAssociated2lba = NV_TRUE;

    // PBAs aren't associated earlier
    if (!physBlksAssociated2lba)
    {
        RW_TRACE(("\r\n GetWriteSec: PBAs aren't associated earlier"));
        
        // clear the physical block array
        ClearPhysicalPageArray(hNand, hNand->NandStrat.pPhysBlks);
        
        // we do not have PBA's associated with our LBA;
        // Get free physical blocks and assign it to our LBA and mark
        // the pba as used in TT
        hNand->NandStrat.ttStatus =
            GetAlignedPBA(
                hNand,
                logicalBlock2Write,
                NULL,
                hNand->NandStrat.pPhysBlks,
#if USE_RSVD_PBA_ONLY
                NV_TRUE);
#else
                NV_FALSE);
#endif
        ChkILTTErr(hNand->NandStrat.ttStatus, NvError_NandNoFreeBlock);

        // return error if we do not have a free block
        if (hNand->NandStrat.pPhysBlks[0] == -1)
        {
            NvOsDebugPrintf("\r\nNvError_NandNoFreeBlock1 ");
            //goto HandleNoFreeBlock;
            RETURN(NvError_NandNoFreeBlock);
        }
        if (hNand->NandStrat.ttStatus != NvSuccess)
        {
            NvOsDebugPrintf("\r\n GetNewPBA failed Sts: 0x%x in GetSectorPage2Write", hNand->NandStrat.ttStatus);
            return hNand->NandStrat.ttStatus;
        }
        ChkILTTErr(hNand->NandStrat.ttStatus, hNand->NandStrat.ttStatus);


        // start tracking the block, if tracking is enabled
        if (hNand->NandStrat.NoOfLbas2Track)
        {
            // start tracking this lba.
            index = NandStrategyAddLba2TrackingList(
                hNand, logicalBlock2Write, hNand->NandStrat.pPrevPhysBlks,
                hNand->NandStrat.pPhysBlks);
            if (index < 0)
                RETURN(NvError_NandTLFailed);

            if (hNand->NandStrat.IsOrderingValid)
            {
                // PageOffset would be the first Free page.
                PageOffset = (hNand->NandStrat.pTrackLba[index].FirstFreePage
                    >> hNand->NandStrat.bsc4NoOfILBanks);

                // TmplogicalPgeStrOffset would be different 
                // from logicalPageSectorOffset
                // This is becasue the offset in which the request is 
                // made may not be the offset in which it actually writes
                TmplogicalPgeStrOffset = 
                    (hNand->NandStrat.pTrackLba[index].FirstFreePage % 
                    hNand->NandStrat.NoOfILBanks) << 
                    hNand->NandStrat.bsc4SctsPerPg;
            }
            // This operation never fails,
            // as we don't have valid previous blocks.
            hNand->NandStrat.selfStatus = TrackLbaMarkPages2WriteBusy(
                    hNand, &hNand->NandStrat.pTrackLba[index],
                    logicalBlockPageOffset, *logicalPageSectorOffset,
                    *sectors2Write);
            CheckNvError(hNand->NandStrat.selfStatus, 
                hNand->NandStrat.selfStatus);
            if (hNand->NandStrat.IsOrderingValid)
            {
                *logicalPageSectorOffset = TmplogicalPgeStrOffset;
            }
        }
        else
        {
            // Allocate the new PBA's to our LBA for future reference
            hNand->NandStrat.ttStatus = AssignPba2Lba(
                hNand, hNand->NandStrat.pPhysBlks, logicalBlock2Write, NV_TRUE);
            ChkILTTErr(hNand->NandStrat.ttStatus, hNand->NandStrat.ttStatus);
        }

        // now we have a pba for our lba, so return
        // the page number to which we need to write data  in our new pba
        NandStrategyConvertBlockNoToPageNo(
            hNand, hNand->NandStrat.pPhysBlks, pPhysPgs, PageOffset);

        NandSanityCheckPageNum(hNand, pPhysPgs, &FlagNoPage);
        if (FlagNoPage)
        {
            NV_ASSERT(NV_FALSE);
            NvOsDebugPrintf("\nError: NandStrategyGetSectorPageToWrite GetPBA case, No Page ");
        }
        RW_TRACE(("\r\n write end"));
        return NvSuccess;
    }

    {
        // lba have associated pbas already. we come here in the following cases.
        // 1. Lba is not under tracking.
        // 2. Lba is under tracking, but the requested sectors are not free. So,
        // it was removed from the tracking.
        RW_TRACE(("\r\n GetWriteSec: Entered case 2"));
            
        // clear the physical block array
        ClearPhysicalPageArray(hNand, hNand->NandStrat.pPhysBlks);
        
        // Get a free physical block(s) and mark the pba as used in TT.
        // Don't assign it to our LBA here. We need to assign only when
        // this LBA is removed from tracking.
        hNand->NandStrat.ttStatus =
            GetAlignedPBA(
                hNand,
                logicalBlock2Write,
                hNand->NandStrat.pPrevPhysBlks,
                hNand->NandStrat.pPhysBlks,
#if USE_RSVD_PBA_ONLY
                NV_TRUE);
#else
                NV_FALSE);
#endif
        // ChkILTTErr(hNand->NandStrat.ttStatus, NvError_NandNoFreeBlock);
        
        // return error if we don't have a free block.
        if ((physBlksAssociated2lba) &&
        (hNand->NandStrat.pPhysBlks[0] == -1) &&
        (NvError_NandNoFreeBlock == hNand->NandStrat.ttStatus))
        {
            // Initialize physical page array
            NvOsMemset(pPhysPgs, 0xFF, 
                (sizeof(NvU32) * MAX_ALLOWED_INTERLEAVE_BANKS));
            for (i = 0; i < hNand->NandStrat.NoOfILBanks; i++)
                    pPhysPgs[i] = (hNand->NandStrat.pPrevPhysBlks[i] *
                        hNand->NandDevInfo.PgsPerBlk);
            NvOsDebugPrintf("\r\nNvError_NandNoFreeBlock2 ");
            //goto HandleNoFreeBlock;
            RETURN(NvError_NandNoFreeBlock);
        }
        RW_TRACE(("\r\n GetWriteSec: New PBA for this LBA"));
        for (i = 0; i < hNand->NandStrat.NoOfILBanks; i++)
            RW_TRACE(("%d ", hNand->NandStrat.pPhysBlks[i]));
        RW_TRACE(("\n"));
        if (hNand->NandStrat.ttStatus != NvSuccess)
        {
            NvOsDebugPrintf("\r\n GetNewPBA failed Sts: 0x%x in GetSectorPage2Write #2", hNand->NandStrat.ttStatus);
            return hNand->NandStrat.ttStatus;
        }
        if (hNand->NandStrat.NoOfLbas2Track)
        {
            // start tracking this lba.
            index = NandStrategyAddLba2TrackingList(
                hNand,
                logicalBlock2Write,
                hNand->NandStrat.pPrevPhysBlks,
                hNand->NandStrat.pPhysBlks);
            if (index < 0)
                RETURN(NvError_NandTLFailed);

            if (hNand->NandStrat.IsOrderingValid)
            {
                // PageOffset would be the first Free page.
                PageOffset = (hNand->NandStrat.pTrackLba[index].FirstFreePage >>
                    hNand->NandStrat.bsc4NoOfILBanks);

                // TmplogicalPgeStrOffset would be different from logicalPageSectorOffset
                // This is becasue the offset in which the request is 
                // made may not be the offset in which it actually writes.
                TmplogicalPgeStrOffset = 
                    (hNand->NandStrat.pTrackLba[index].FirstFreePage % 
                    hNand->NandStrat.NoOfILBanks) << 
                    hNand->NandStrat.bsc4SctsPerPg;
            }
            // now we have a new pba for our lba, return
            // the page number to which we need to write data  in our new pba
            NandStrategyConvertBlockNoToPageNo(
                hNand, hNand->NandStrat.pTrackLba[index].pBlocks, pPhysPgs,
                PageOffset);
            NandSanityCheckPageNum(hNand, pPhysPgs, &FlagNoPage);
            if (FlagNoPage)
            {
                NV_ASSERT(NV_FALSE);
                NvOsDebugPrintf("\nError: NandStrategyGetSectorPageToWrite "
                    "PBA assigned already case, No Page ");
            }

            hNand->NandStrat.selfStatus = TrackLbaMarkPages2WriteBusy(
                hNand, &hNand->NandStrat.pTrackLba[index], 
                logicalBlockPageOffset, *logicalPageSectorOffset, *sectors2Write);

            CheckNvError(hNand->NandStrat.selfStatus, hNand->NandStrat.selfStatus);
            if (hNand->NandStrat.IsOrderingValid)
            {
                *logicalPageSectorOffset = TmplogicalPgeStrOffset;
            }
        }
        else
        {
            NV_ASSERT(!"\n No of Tracking LBAs are zero. NOT IMPLEMENTED\n");
            return NvError_NandTLFailed;
        }
    }
    if (IsNonZeroRegion)
    {
        hNand->IsNonZeroRegion = NV_TRUE;
        // Restore Region Numbers into hNand handle
        NvOsMemcpy(hNand->RegNum, TmpRegNum, (MAX_ALLOWED_INTERLEAVE_BANKS *
            sizeof(NvU32)));
    }
    return NvSuccess;
}

NvError NandStrategyGetSectorPageToRead(
    NvNandHandle hNand,
    NvU32 reqSectorNumber,
    NvU32 reqSectors,
    NvS32* pPhysPgs,
    NvS32* logicalPageSectorOffset,
    NvS32* sectors2Read,
    NvU8** pTagBuffer)
{
    NvS32 availableSectors, logicalBlock2Read, logicalBlockPageOffset;
    NvS32 PageOffset = 0;
    NvS32 index, i;
    NvS32 startPageOffset, endPageOffset;
    NvS32 dummy1, dummy2, dummy3;

    logicalBlockPageOffset =
        (reqSectorNumber%hNand->NandStrat.SecsPerLBlk) >>
        hNand->NandStrat.bsc4SctsPerLPg;
    logicalBlock2Read = reqSectorNumber >> hNand->NandStrat.bsc4SecsPerLBlk;
    *logicalPageSectorOffset = reqSectorNumber%hNand->NandStrat.SctsPerLPg;
    availableSectors = hNand->NandStrat.SecsPerLBlk -
        ((logicalBlockPageOffset << hNand->NandStrat.bsc4SctsPerLPg) +
        *logicalPageSectorOffset);
    *sectors2Read = (reqSectors >= (NvU32)availableSectors) ?
        availableSectors : reqSectors;
    *pTagBuffer = 0;

    // clear required arrays
    NandStrategyclearPhysicalPageArray(hNand, pPhysPgs);
    NandStrategyclearPhysicalPageArray(hNand, hNand->NandStrat.pPhysBlks);

    LOG_NAND_DEBUG_INFO(("\r\n\r\n Read Req with LSA:%d; NumSec:%d; LBA:%d; \
                         Page:%d; SectorOff:%d; LSAOffset = %d",
                         reqSectorNumber, reqSectors, logicalBlock2Read,
                         logicalBlockPageOffset, *logicalPageSectorOffset,
                         reqSectorNumber%hNand->NandStrat.SecsPerLBlk),
                         IL_STRAT_READ_DEBUG_INFO);

    if (NandStrategyIsLbaInTrackingList(hNand, logicalBlock2Read, &index))
    {
        // the lba we are trying to read is under tracking.
        // some data might be in old blocks and some might be in new blocks.
        // Need to give correct data.
        startPageOffset =
            (logicalBlockPageOffset <<
            hNand->NandStrat.bsc4NoOfILBanks) +
            (*logicalPageSectorOffset >> hNand->NandStrat.bsc4SctsPerPg);
        endPageOffset = hNand->NandStrat.PgsPerLBlk - 1;
        RW_TRACE(("\r\n read_strat LBA %d in tracking",
                    hNand->NandStrat.pTrackLba[index].lba));
#if TRACE_READ_WRITE
    {   
        NvS32 bank = 0;
        for(bank = 0; bank<hNand->InterleaveCount; bank++)
        RW_TRACE(("\r\n PBA1(%d)PBA2(%d)",
        hNand->NandStrat.pTrackLba[index].pPrevBlocks[bank],
        hNand->NandStrat.pTrackLba[index].pBlocks[bank]));
    }
#endif

        if (TrackLbaGetConsecutivePageStatus(
            &hNand->NandStrat.pTrackLba[index],
            startPageOffset,
            endPageOffset,
            &dummy1,
            &dummy2,
            &dummy3,
            &availableSectors,
            FREE))
        {
            // sectors are free meaning that the data is in old blocks.
            // read from the old blocks.
            // Check if our lba is associated with a pba
            if (hNand->NandStrat.pTrackLba[index].pPrevBlocks[0] == -1)
                return NvError_NandTLNoBlockAssigned;

            // now we have a pba's for our lba, so return the page numbers
            // from which we need to read data
            NandStrategyConvertBlockNoToPageNo(
                hNand,
                hNand->NandStrat.pTrackLba[index].pPrevBlocks,
                pPhysPgs,
                logicalBlockPageOffset);
            RW_TRACE(("\r\n Request falled in old block"));
            *sectors2Read = (reqSectors >= (NvU32)availableSectors) ?
                availableSectors : reqSectors;
        }
        else if (TrackLbaGetConsecutivePageStatus(
            &hNand->NandStrat.pTrackLba[index],
            startPageOffset,
            endPageOffset,
            &dummy1,
            &dummy2,
            &dummy3,
            &availableSectors, BUSY))
        {
            // sectors are busy meaning that the data is in new blocks.
            // read from the new blocks.
            // Check if our lba is associated with a pba
            if (hNand->NandStrat.pTrackLba[index].pBlocks[0] == -1)
                return NvError_NandTLNoBlockAssigned;

            // now we have a pba's for our lba, so return the page numbers
            // from which we need to read data
            if (!(hNand->NandStrat.pTrackLba[index].IsWriteRequestRandom))
                PageOffset = logicalBlockPageOffset;
            else
            {
                PageOffset = 
                    hNand->NandStrat.pTrackLba[index].pPageState[startPageOffset].PageNumber / 
                        hNand->NandStrat.NoOfILBanks;
                *logicalPageSectorOffset = 
                    (hNand->NandStrat.pTrackLba[index].pPageState[startPageOffset].PageNumber % 
                        hNand->NandStrat.NoOfILBanks)  << 
                        hNand->NandStrat.bsc4SctsPerPg;
            }
            RW_TRACE(("\n in Read req no = %d Given no = %d", 
            logicalBlockPageOffset, PageOffset));
            NandStrategyConvertBlockNoToPageNo(
                hNand,
                hNand->NandStrat.pTrackLba[index].pBlocks,
                pPhysPgs,
                PageOffset);
            RW_TRACE(("\r\n Request falled in new block"));

            *sectors2Read = (reqSectors >= (NvU32)availableSectors) ?
                availableSectors : reqSectors;

            if (hNand->NandStrat.pTrackLba[index].IsWriteRequestRandom)
            {
                *sectors2Read = GetNoOfContinuousPages(
                (hNand->NandStrat.pTrackLba[index].pPageState),
                *sectors2Read >> hNand->NandStrat.bsc4SctsPerPg,
                startPageOffset) << hNand->NandStrat.bsc4SctsPerPg;
            }
        }
        else
        {
            // the requested sector is neither free nor written.
            // Check the page busy/free marking logic. There must be a bug.
            RETURN(NvError_NandTLFailed);
        }
    }
    else
    {
        // get the actual physical blocks to which our lba is pointing to
        hNand->NandStrat.ttStatus =
            GetPBA(hNand, logicalBlock2Read, hNand->NandStrat.pPhysBlks);
        ChkILTTErr(hNand->NandStrat.ttStatus, hNand->NandStrat.ttStatus);

        RW_TRACE(("\n PBAs:"));
        for (i = 0; i < hNand->NandStrat.NoOfILBanks; i++)
            RW_TRACE(("%d ", hNand->NandStrat.pPhysBlks[i]));

        // Check if our lba is associated with a pba
        if (hNand->NandStrat.pPhysBlks[0] == -1)
            return NvError_NandTLNoBlockAssigned;

        // now we have a pba's for our lba, so return the page numbers from
        // which we need to read data
        NandStrategyConvertBlockNoToPageNo(
            hNand,
            hNand->NandStrat.pPhysBlks,
            pPhysPgs,
            logicalBlockPageOffset);
    }
    RW_TRACE(("\r\n Read end"));
    RW_TRACE(("\r\n availableSectors = %d, *sectors2Read = %d",
        availableSectors, *sectors2Read));

    return NvSuccess;
}
NvError NandStrategyReplaceErrorSector(
    NvNandHandle hNand,
    NvS32 errorSecNum,
    NvBool writeError)
{
    // NvBool halt = NV_TRUE;
    // while (halt);
    NvS32 logicalBlock2Replace =
        errorSecNum >> hNand->NandStrat.bsc4SecsPerLBlk;
    NvS32 logicalBlockPageOffset =
        (errorSecNum % hNand->NandStrat.SecsPerLBlk) >>
        hNand->NandStrat.bsc4SctsPerLPg;
    NvS32 logicalPageSectorOffset = errorSecNum % hNand->NandStrat.SctsPerLPg;
    // find the bank number on which the error sector falls on.
    NvS32 errorBank =
        logicalPageSectorOffset >> hNand->NandStrat.bsc4SctsPerPg;
    NvS32 index;
    NvS32 pageCount;
    NvS32 sectorCount, zone, startPageOffset, endPageOffset;
    NvBool oldBlockIsBad;
    LOG_NAND_DEBUG_INFO(("\r\n replaceErrorSector, writeError = %d", writeError),
        BAD_BLOCK_DEBUG_INFO);

    if (NandStrategyIsLbaInTrackingList(hNand, logicalBlock2Replace, &index))
    {
        LOG_NAND_DEBUG_INFO(("\r\n BadBlock is under tracking"),
            BAD_BLOCK_DEBUG_INFO);
        LOG_NAND_DEBUG_INFO(("\r\n errorBank = %d, lba = %d, \
            lBlockPageOffset = %d, lPageSectorOffset = %d",
            errorBank, logicalBlock2Replace, logicalBlockPageOffset,
            logicalPageSectorOffset), BAD_BLOCK_DEBUG_INFO);
        // see which block is bad, the new block or old block.
        // replace the appropriate one.
        oldBlockIsBad =
            TrackLbaIsPages2WriteFree(
                &hNand->NandStrat.pTrackLba[index],
                logicalBlockPageOffset,
                logicalPageSectorOffset, 1/* noOfSectors*/);

        if (oldBlockIsBad)
        {
            LOG_NAND_DEBUG_INFO(("\r\n old block is bad, errorpba = %d",
                hNand->NandStrat.pTrackLba[index].pPrevBlocks[errorBank]),
                BAD_BLOCK_DEBUG_INFO);
            if (writeError)
            {
                // write operation should never fall in the old block.
                // Find what's not correct.
                RETURN(NvError_NandTLFailed);
            }

            zone = hNand->NandStrat.pTrackLba[index].pPrevBlocks[errorBank] /
                hNand->NandStrat.PhysBlksPerZone;
            // release a data reserved block from the same zone for
            // compensating the bad block.
            if (!IsPbaReserved(
                hNand,
                errorBank,
                hNand->NandStrat.pTrackLba[index].pPrevBlocks[errorBank]))
            {
                hNand->NandStrat.ttStatus =
                    ReleasePbaFromReserved(hNand, errorBank, zone);
                ChkILTTErr(hNand->NandStrat.ttStatus, 
                    hNand->NandStrat.ttStatus);
            }

            // during copyback, the low level driver ignores read errors.
            // It will report only write errors.
            // old block is bad. First copy the data from old block to new block.
            hNand->NandStrat.selfStatus =
                NandStrategyFlushLbaTrackingList(hNand, index);
            CheckNvError(hNand->NandStrat.selfStatus, 
                hNand->NandStrat.selfStatus);

            // mark the old block as bad, if necessary. if it is not bad,
            // then mark as unused and rsvd.
            hNand->NandStrat.selfStatus =
                NandStrategyMarkBlockAsBad(
                    hNand, errorBank,
                    hNand->NandStrat.pTrackLba[index].pPrevBlocks[errorBank]);
            CheckNvError(hNand->NandStrat.selfStatus, 
                hNand->NandStrat.selfStatus);
        }
        else
        {
            // new block is bad. Take another new block
            // and copy all the data to it.
            LOG_NAND_DEBUG_INFO(("\r\n new block is bad, errorpba = %d",
                hNand->NandStrat.pTrackLba[index].pBlocks[errorBank]),
                BAD_BLOCK_DEBUG_INFO);
            startPageOffset = 0;
            endPageOffset = hNand->NandStrat.PgsPerLBlk - 1;
            // Read error occurred in new block.
            // Just copy the busy pages from error block to new block
            TrackLbaGetConsecutivePageStatus(
                &hNand->NandStrat.pTrackLba[index],
                startPageOffset,
                endPageOffset,
                &logicalBlockPageOffset,
                &logicalPageSectorOffset,
                &pageCount,
                &sectorCount, BUSY);

            if (sectorCount)
            {
                LOG_NAND_DEBUG_INFO(("\r\n busy sec found, pageOffset = %d, \
                    secOffset = %d, secCount = %d",
                    logicalBlockPageOffset, logicalPageSectorOffset, sectorCount),
                    BAD_BLOCK_DEBUG_INFO);

                if (!NandStrategyReplaceBadBlock(
                    hNand,
                    errorBank,
                    &hNand->NandStrat.pTrackLba[index].pBlocks[errorBank],
                    0, pageCount))
                {
                    RETURN(NvError_NandTLFailed);
                }
            }
            else
            {
                // Something is not correct. find out.
                RETURN(NvError_NandTLFailed);
            }
        }
    }
    else
    {
        // clear the required arrays.
        NandStrategyclearPhysicalPageArray(hNand, hNand->NandStrat.pTempBlocks);
        // get the actual physical block(s) to which our lba is pointing to
        hNand->NandStrat.ttStatus =
            GetPBA(hNand, logicalBlock2Replace, hNand->NandStrat.pTempBlocks);
        ChkILTTErr(hNand->NandStrat.ttStatus, hNand->NandStrat.ttStatus);
        // Check if our lba is associated with a pba
        if (hNand->NandStrat.pTempBlocks[0] == -1)
        {
            // we should never come here. something is wrong. find out.
            RETURN(NvError_NandTLNoBlockAssigned);
        }

        LOG_NAND_DEBUG_INFO(("\r\n errorBank = %d, lba = %d, errorPba = %d, \
            lBlockPageOffset = %d, lPageSectorOffset = %d",
            errorBank, logicalBlock2Replace,
            hNand->NandStrat.pTempBlocks[errorBank],
            logicalBlockPageOffset, logicalPageSectorOffset),
            BAD_BLOCK_DEBUG_INFO);

        if (NandStrategyReplaceBadBlock(
            hNand,
            errorBank,
            &hNand->NandStrat.pTempBlocks[errorBank],
            0, hNand->NandStrat.PgsPerLBlk))
        {
            // Allocate the new PBA to our LBA for future reference
            hNand->NandStrat.ttStatus =
                AssignPba2Lba(
                    hNand,
                    hNand->NandStrat.pTempBlocks,
                    logicalBlock2Replace,
                    NV_TRUE);
            ChkILTTErr(hNand->NandStrat.ttStatus, hNand->NandStrat.ttStatus);
        }
        else
        {
            // Replace failed, indicating that there are no good blocks
            // to replace the bad one with.
            LOG_NAND_DEBUG_INFO(("\r\n Replace bad block failed"),
                BAD_BLOCK_DEBUG_INFO);
            RETURN(NvError_NandTLFailed);
        }
    }

    return NvSuccess;
}

NvError NandStrategyFlushTranslationTable(NvNandHandle hNand)
{
    LOG_NAND_DEBUG_INFO(("\r\n flushing TTable"), IL_STRAT_DEBUG_INFO);
    hNand->NandStrat.selfStatus = NandStrategyFlushLbaTrackingList(hNand, -1);
    CheckNvError(hNand->NandStrat.selfStatus, hNand->NandStrat.selfStatus);
    hNand->NandStrat.ttStatus = FlushTranslationTable(hNand, -1, 0, NV_TRUE);
    ChkILTTErr(hNand->NandStrat.ttStatus, hNand->NandStrat.ttStatus);
    LOG_NAND_DEBUG_INFO(("\r\n flushed TTable"), IL_STRAT_DEBUG_INFO);
    return NvSuccess;
}

void NandStrategyclearPhysicalPageArray(NvNandHandle hNand, NvS32* pPages)
{
    NvS32 i;
    for (i = 0; i < hNand->NandStrat.NoOfILBanks; i++)
        pPages[i] = -1;
}

NvBool NandStrategyReplaceBadBlock(
    NvNandHandle hNand,
    NvS32 bank,
    NvS32* badBlock,
    NvS32 startPageOffset,
    NvS32 noOfPages)
{
    return NV_TRUE;
}

NvBool NandStrategyIsOnlyReadFailed(void)
{
    NvBool returnVal = NV_TRUE;
    /*
        // See what failed. if there is no write error, that indicates
        // that only read error occurred.
        for (int i = 0; i < hNand->NandStrat.NoOfILBanks; i++)
        {
            if (pErrorInfo->pCopybackDstnStatus[i] != NandDriver::OK)
            {
                returnVal = NV_FALSE;
                break;
            }
        }
    */
    return returnVal;
}

void NandStrategyConvertBlockNoToPageNo(
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
            RW_TRACE(("\npBlocks[%d] = %d\n", bank, pBlocks[bank]));
        }
    }
}

void NandStrategyConvertPageNoToBlockNo(
    NvNandHandle hNand,
    NvS32* pBlocks,
    NvS32* pPages)
{
    NvS32 bank;
    for (bank = 0; bank < hNand->NandStrat.NoOfILBanks; bank++)
    {
        if (pPages[bank] != -1)
        {
            pBlocks[bank] =
                (pPages[bank] >> hNand->NandStrat.bsc4PgsPerBlk);
            RW_TRACE(("\npBlocks[%d] = %d\n", bank, pBlocks[bank]));
        }
    }
}

NvBool NandStrategyIsLbaInTrackingList(
    NvNandHandle hNand,
    NvS32 lba,
    NvS32* index)
{
    NvS32 i;
    for (i = 0; i < hNand->NandStrat.NoOfLbas2Track; i++)
    {
        if ((hNand->NandStrat.pTrackLba[i].lba == lba) &&
        hNand->NandStrat.pTrackLba[i].inUse)
        {
            *index = i;
            LOG_NAND_DEBUG_INFO(("\r\n lba = %d present in track list", lba),
                IL_STRAT_DEBUG_INFO);
            // update last accessed time
            hNand->NandStrat.pTrackLba[*index].LastAccessTick =
                NvOsGetTimeUS();
            return NV_TRUE;
        }
    }

    return NV_FALSE;
}

NvU32 NandStrategyGetTrackingListCount(NvNandHandle hNand)
{
    NvS32 i;
    NvU32 ListCount = 0;
    for (i = 0; i < hNand->NandStrat.NoOfLbas2Track; i++)
    {
        if (hNand->NandStrat.pTrackLba[i].inUse)
        {
            RW_TRACE(("\r\n LBA %d is in tracking",hNand->NandStrat.pTrackLba[i].lba));
            ListCount++;
        }
    }
    RW_TRACE(("\r\n Total LBA in tracking %d",ListCount));
    return ListCount;
}

NvS32 NandStrategyAddLba2TrackingList(
    NvNandHandle hNand,
    NvS32 lba,
    NvS32* pPrevBlocks,
    NvS32* pBlocks)
{
    NvS32 index = 0;
    NvS32 i;
    NvU64 leastAccessedTime = hNand->NandStrat.pTrackLba[index].LastAccessTick;

    LOG_NAND_DEBUG_INFO(("\r\n adding lba to tracking list = %d", lba),
        IL_STRAT_DEBUG_INFO);
    // look for a free track lba entry
    for (i = 0; i < hNand->NandStrat.NoOfLbas2Track; i++)
    {
        if (!hNand->NandStrat.pTrackLba[i].inUse)
        {
            index = i;
            LOG_NAND_DEBUG_INFO(("\n index = %d", index), IL_STRAT_DEBUG_INFO);
            hNand->NandStrat.pTrackLba[i].inUse = NV_TRUE;
            TrackLbaSetLbaInfo(
                &hNand->NandStrat.pTrackLba[i],
                lba, pPrevBlocks, pBlocks);
            TrackLbaMarkPages2WriteFree(
                &hNand->NandStrat.pTrackLba[index],
                0,
                0,
                hNand->NandStrat.SecsPerLBlk);
            // update last accessed time
            hNand->NandStrat.pTrackLba[index].LastAccessTick = NvOsGetTimeUS();
            return index;
        }
        else
        {
            // check the free sector count. If there is no free sectors, flush it.
            if (!NandStrategyHasLbaFreePages(hNand, i))
            {
                LOG_NAND_DEBUG_INFO(("\r\n lba = %d, has no free pages",
                    hNand->NandStrat.pTrackLba[i].lba),
                    IL_STRAT_DEBUG_INFO);
                index = i;
                break;
            }

            // There are some free sectors in the LBA, so flush it based
            // on the access time.
            if (leastAccessedTime > hNand->NandStrat.pTrackLba[i].LastAccessTick)
            {
                leastAccessedTime = hNand->NandStrat.pTrackLba[i].LastAccessTick;
                index = i;
            }
        }
    }

    // we are here means, track lba list is full.
    // Flush the oldest one to nand flash.
    hNand->NandStrat.selfStatus = NandStrategyFlushLbaTrackingList(hNand, index);
    CheckNvError(hNand->NandStrat.selfStatus, -1);
    // add now
    LOG_NAND_DEBUG_INFO(("\n index = %d", index), IL_STRAT_DEBUG_INFO);
    hNand->NandStrat.pTrackLba[index].inUse = NV_TRUE;
    TrackLbaSetLbaInfo(
        &hNand->NandStrat.pTrackLba[index],
        lba,
        pPrevBlocks,
        pBlocks);

    TrackLbaMarkPages2WriteFree(
        &hNand->NandStrat.pTrackLba[index],
        0,
        0,
        hNand->NandStrat.SecsPerLBlk);
    // update last accessed time
    hNand->NandStrat.pTrackLba[index].LastAccessTick = NvOsGetTimeUS();
    return index;
}

void NandStrategyRemoveLbaFromTrackingList(NvNandHandle hNand, NvS32 index)
{
    RW_TRACE(("\r\n removing lba = %d from track list, index = %d",
                        hNand->NandStrat.pTrackLba[index].lba, index));
    hNand->NandStrat.pTrackLba[index].inUse = NV_FALSE;
    hNand->NandStrat.pTrackLba[index].FirstFreePage = 0;
    hNand->NandStrat.pTrackLba[index].IsWriteRequestRandom = NV_FALSE;
}

/**
 * This function does the ordering. The block contains continuous blocks written.
 * The exact map is stored in the tracking Buffer. The pagesa read from old block
 * and are written in new block.
 */
NvError NandStrategyFlushLbaTrackingList(
    NvNandHandle hNand,
    NvS32 position)
{
    NvS32 startIndex;
    NvS32 endIndex;
    NvS32 i, index;
    NvBool prevBlocksArevalid;
    NvS32 * pDestBlocks = NULL;
    NvU32 PhysBlks[4] = {-1, -1, -1, -1};
    NvU32 SrcPages[4] = {-1, -1, -1, -1};
    NvU32 DestPages[4] = {-1, -1, -1, -1};
    NvU32 NewDestPages[4] = {-1, -1, -1, -1};
    CopyBackHandler CpyBakHandler;

    if (position == -1)
    {
        RW_TRACE(("\r\n flushing all lba from tracking list"));
        // we need to flush all the lba's back to Nand Flash.
        startIndex = 0;
        endIndex = hNand->NandStrat.NoOfLbas2Track;
    }
    else
    {
        startIndex = position;
        endIndex = position + 1;
    }

    for (index = startIndex; index < endIndex; index++)
    {
        if (!hNand->NandStrat.pTrackLba[index].inUse)
            continue;
#if STRESS_TEST_GETALIGNEDPBA
        StressTestGetAlignedPBA(hNand);
#endif
        if (hNand->NandStrat.pTrackLba[index].IsWriteRequestRandom)
        {
            // Initialize the buffer to -1.
            ClearPhysicalPageArray(hNand, (NvS32*)PhysBlks);
            hNand->NandStrat.ttStatus = GetAlignedPBA(hNand,
                hNand->NandStrat.pTrackLba[index].lba,
                hNand->NandStrat.pTrackLba[index].pBlocks, (NvS32*)PhysBlks,
                (USE_RSVD_PBA_ONLY) ? NV_TRUE : NV_FALSE);

            ChkILTTErr(hNand->NandStrat.ttStatus, NvError_NandNoFreeBlock);

            if (PhysBlks[0] == -1)
            {
                NvOsDebugPrintf("\r\nNvError_NandNoFreeBlock1 ");
                RETURN(NvError_NandNoFreeBlock);
            }
            if (hNand->NandStrat.ttStatus != NvSuccess)
            {
                NvOsDebugPrintf("\r\n GetNewPBA failed Sts: 0x%x in Flush",
                    hNand->NandStrat.ttStatus);
                return hNand->NandStrat.ttStatus;
            }

            RW_TRACE(("\nNew Dest PBAs: "));
            // Convert Block Numbers to Page numbers
            NandStrategyConvertBlockNoToPageNo(hNand,
                (NvS32*)PhysBlks, (NvS32*)NewDestPages, 0);

            RW_TRACE(("\r\n New PBAs in flush: "));
            for (i = 0; i < hNand->NandStrat.NoOfILBanks; i++)
                RW_TRACE(("%d ", PhysBlks[i]));
            pDestBlocks = (NvS32*)PhysBlks;
        }
        else
            pDestBlocks = hNand->NandStrat.pTrackLba[index].pBlocks;

        prevBlocksArevalid = NV_FALSE;
        RW_TRACE(("\r\n flushing lba from tracking list = %d, index = %d",
                             hNand->NandStrat.pTrackLba[index].lba,
                             index));

        for (i = 0; i < hNand->NandStrat.NoOfILBanks; i++)
        {
            if (hNand->NandStrat.pTrackLba[index].pPrevBlocks[i] != -1)
            {
                prevBlocksArevalid = NV_TRUE;
                break;
            }
        }

        RW_TRACE(("\nDest PBAs: "));
        // Convert Dest Blocks to Pages.
        NandStrategyConvertBlockNoToPageNo(hNand,
            hNand->NandStrat.pTrackLba[index].pBlocks, (NvS32 *)DestPages, 0);

        if (prevBlocksArevalid)
        {
            RW_TRACE(("\r\n InFlush: Prev Blks Valid"));
            RW_TRACE(("\nSrc PBAs: "));
            NandStrategyConvertBlockNoToPageNo(hNand,
                hNand->NandStrat.pTrackLba[index].pPrevBlocks, 
                (NvS32 *)SrcPages, 0);

            if (!hNand->NandStrat.pTrackLba[index].IsWriteRequestRandom)
            {
                RW_TRACE(("\r\n InFlush: Write Request is not Random"));

                CpyBakHandler.pSrcPageNumbers = SrcPages;
                CpyBakHandler.pDestPageNumbers = DestPages;
                CpyBakHandler.ppNewDestPageNumbers = NULL;
                CpyBakHandler.pTrackingBuf = 
                    &(hNand->NandStrat.pTrackLba[index].pPageState[0]);
                
                hNand->NandStrat.selfStatus =
                    StrategyPrivDoCopyback(hNand, &CpyBakHandler,
                        NV_FALSE, CopyBackMode_FLUSH_SRC_DEST);
                CheckNvError(hNand->NandStrat.selfStatus, 
                    hNand->NandStrat.selfStatus);

                // New Pages might have got updated in case of any failure.
                // Updating the dest blocks with new pages.
                NandStrategyConvertPageNoToBlockNo(hNand, (NvS32 *)pDestBlocks,
                    (NvS32*)CpyBakHandler.pDestPageNumbers);

            }
            else
            {
                RW_TRACE(("\r\n InFlush: Write Request is Random"));

                CpyBakHandler.pSrcPageNumbers = SrcPages;
                CpyBakHandler.pDestPageNumbers = DestPages;
                CpyBakHandler.ppNewDestPageNumbers = NewDestPages;
                CpyBakHandler.pTrackingBuf = 
                    &(hNand->NandStrat.pTrackLba[index].pPageState[0]);

                hNand->NandStrat.selfStatus = 
                    StrategyPrivDoCopyback(hNand, &CpyBakHandler, NV_TRUE, 
                    CopyBackMode_FLUSH_SRC_DEST_MERGE);
                CheckNvError(hNand->NandStrat.selfStatus, 
                    hNand->NandStrat.selfStatus);

                NandStrategyConvertPageNoToBlockNo(hNand,
                    (NvS32 *)pDestBlocks, (NvS32 *)NewDestPages);
            }

            // Allocate the new PBA to our LBA for future refference.
            hNand->NandStrat.ttStatus = AssignPba2Lba(hNand, 
                pDestBlocks, hNand->NandStrat.pTrackLba[index].lba, NV_TRUE);
            ChkILTTErr(hNand->NandStrat.ttStatus, 
                hNand->NandStrat.ttStatus);

            // release old pbas
            for (i = 0; i < hNand->NandStrat.NoOfILBanks; i++)
            {
                if (hNand->NandStrat.pTrackLba[index].pPrevBlocks[i] != -1)
                {
                    hNand->NandStrat.ttStatus = SetPbaUnused(hNand, i,
                        hNand->NandStrat.pTrackLba[index].pPrevBlocks[i]);
                    ChkILTTErr(hNand->NandStrat.ttStatus, 
                        hNand->NandStrat.ttStatus);
                    if (hNand->NandStrat.pTrackLba[index].IsWriteRequestRandom)
                    {
                        // Release New pba for MLC random request
                        hNand->NandStrat.ttStatus = SetPbaUnused(hNand, i,
                            hNand->NandStrat.pTrackLba[index].pBlocks[i]);
                        ChkILTTErr(hNand->NandStrat.ttStatus, 
                            hNand->NandStrat.ttStatus);
                    }
                }
            }
        }
        else
        {
            RW_TRACE(("\r\n InFlush: Prev Blks Not Valid"));
            if (hNand->NandStrat.pTrackLba[index].IsWriteRequestRandom)
            {
                RW_TRACE(("\r\n InFlush: Write Request is Random"));

                CpyBakHandler.pSrcPageNumbers = NULL;
                CpyBakHandler.pDestPageNumbers = DestPages;
                CpyBakHandler.ppNewDestPageNumbers = NewDestPages;
                CpyBakHandler.pTrackingBuf = 
                    &(hNand->NandStrat.pTrackLba[index].pPageState[0]);

                hNand->NandStrat.selfStatus = 
                    StrategyPrivDoCopyback(
                    hNand, &CpyBakHandler, NV_TRUE, 
                    CopyBackMode_FLUSH_DEST_MERGE);
                CheckNvError(hNand->NandStrat.selfStatus, 
                    hNand->NandStrat.selfStatus);

                NandStrategyConvertPageNoToBlockNo(hNand,
                    pDestBlocks, (NvS32 *)NewDestPages);
            }
            hNand->NandStrat.ttStatus = AssignPba2Lba(hNand, 
                pDestBlocks, hNand->NandStrat.pTrackLba[index].lba, NV_TRUE);
            ChkILTTErr(hNand->NandStrat.ttStatus, hNand->NandStrat.ttStatus);
            // don't need to copyback anything.
            RW_TRACE(("\r\n prevBlocks Are not valid"));
            if (hNand->NandStrat.pTrackLba[index].IsWriteRequestRandom)
            {
                for (i = 0; i < hNand->NandStrat.NoOfILBanks; i++)
                {
                    if (hNand->NandStrat.pTrackLba[index].pBlocks[i] != -1)
                    {
                        hNand->NandStrat.ttStatus = SetPbaUnused(
                            hNand, i,
                            hNand->NandStrat.pTrackLba[index].pBlocks[i]);
                        ChkILTTErr(hNand->NandStrat.ttStatus, 
                            hNand->NandStrat.ttStatus);
                    }
                }
            }
        }
        NandStrategyRemoveLbaFromTrackingList(hNand, index);
    }
    return NvSuccess;
}
NvBool NandStrategyHasLbaFreePages(NvNandHandle hNand, NvS32 index)
{
    NvS32 logicalBlockPageOffset;
    NvS32 logicalPageSectorOffset;
    NvS32 endPageOffset = hNand->NandStrat.PgsPerLBlk - 1;
    NvS32 pageCount, sectorCount, startPageOffset;

    for (startPageOffset = 0; startPageOffset < hNand->NandStrat.PgsPerLBlk;)
    {
        TrackLbaGetConsecutivePageStatus(
            &hNand->NandStrat.pTrackLba[index],
            startPageOffset, endPageOffset,
            &logicalBlockPageOffset,
            &logicalPageSectorOffset,
            &pageCount,
            &sectorCount, BUSY);
        if (sectorCount)
        {
            startPageOffset += pageCount;
        }
        else
        {
            TrackLbaGetConsecutivePageStatus(
                &hNand->NandStrat.pTrackLba[index],
                startPageOffset, endPageOffset,
                &logicalBlockPageOffset,
                &logicalPageSectorOffset,
                &pageCount,
                &sectorCount, FREE);
            if (sectorCount)
            {
                return NV_TRUE;
            }
        }
    }

    return NV_FALSE;
}

NvBool NandStrategyIsBlockBad(NvNandHandle hNand, NvS32 bank, NvS32 physBlk)
{
    // write a known data pattern.
    NvU8* pData =
        AllocateVirtualMemory(
            sizeof(NvU8) * hNand->NandStrat.NandDevInfo.PageSize,
            STRATEGY_ALLOC);// uncached
    NvS32 k, i, j;
    NvU8 testPattern;
    if (!pData)
    {
        return NV_FALSE;
    }

    // clear the physical page array
    NandStrategyclearPhysicalPageArray(hNand, hNand->NandStrat.pTmpPhysPgs);
    hNand->NandStrat.pTmpPhysPgs[bank] =
        physBlk << hNand->NandStrat.bsc4PgsPerBlk;

    for (k = 0; k < 2; k++)
    {
        hNand->NandStrat.pTmpPhysPgs[bank] =
            physBlk << hNand->NandStrat.bsc4PgsPerBlk;
        if (NandTLErase(hNand, hNand->NandStrat.pTmpPhysPgs) != NvSuccess)
        {
            // delete []pData;
            ReleaseVirtualMemory( (void*)pData, STRATEGY_ALLOC);
            return NV_TRUE;
        }
        else
        {
            // Check whether the block has all 0xFF's.
            // Read the data and compare.
            for (i = 0; i < hNand->NandStrat.NandDevInfo.PgsPerBlk; i++)
            {
                hNand->NandStrat.pTmpPhysPgs[bank] =
                    (physBlk << hNand->NandStrat.bsc4PgsPerBlk) + i;
                if (NandTLReadPhysicalPage(hNand, hNand->NandStrat.pTmpPhysPgs,
                                           bank <<
                                            hNand->NandStrat.bsc4SctsPerPg,
                                           pData,
                                           hNand->NandStrat.NandDevInfo.SctsPerPg,
                                           NULL, NV_FALSE) != NvSuccess)
                {
                    // delete []pData;
                    ReleaseVirtualMemory( (void*)pData, STRATEGY_ALLOC);
                    return NV_TRUE;
                }

                for (j = 0; j < hNand->NandStrat.NandDevInfo.PageSize; j++)
                {
                    if (pData[j] != 0xFF)
                    {
                        // delete []pData;
                        ReleaseVirtualMemory( (void*)pData, STRATEGY_ALLOC);
                        return NV_TRUE;
                    }
                }
            }
        }

        if (k)
        {
            testPattern = 0xA5;
        }
        else
        {
            testPattern = 0x5A;
        }

        NvOsMemset(pData, testPattern, hNand->NandStrat.NandDevInfo.PageSize);
        for (i = 0; i < hNand->NandStrat.NandDevInfo.PgsPerBlk; i++)
        {
            hNand->NandStrat.pTmpPhysPgs[bank] =
                (physBlk << hNand->NandStrat.bsc4PgsPerBlk) + i;
            if (NandTLWritePhysicalPage(
                    hNand, hNand->NandStrat.pTmpPhysPgs,
                    bank << hNand->NandStrat.bsc4SctsPerPg, pData,
                    hNand->NandStrat.NandDevInfo.SctsPerPg, NULL) != NvSuccess)
            {
                // delete []pData;
                ReleaseVirtualMemory( (void*)pData, STRATEGY_ALLOC);
                return NV_TRUE;
            }
        }

        // Read the data back and compare.
        for (i = 0; i < hNand->NandStrat.NandDevInfo.PgsPerBlk; i++)
        {
            hNand->NandStrat.pTmpPhysPgs[bank] =
                (physBlk << hNand->NandStrat.bsc4PgsPerBlk) + i;

            if (NandTLReadPhysicalPage(hNand, hNand->NandStrat.pTmpPhysPgs,
                    bank << hNand->NandStrat.bsc4SctsPerPg, pData,
                    hNand->NandStrat.NandDevInfo.SctsPerPg,
                    NULL, NV_FALSE) != NvSuccess)
            {
                // delete []pData;
                ReleaseVirtualMemory( (void*)pData, STRATEGY_ALLOC);
                return NV_TRUE;
            }

            for (j = 0; j < hNand->NandStrat.NandDevInfo.PageSize; j++)
            {
                if (pData[j] != testPattern)
                {
                    // delete []pData;
                    ReleaseVirtualMemory( (void*)pData, STRATEGY_ALLOC);
                    return NV_TRUE;
                }
            }
        }
    }
    ReleaseVirtualMemory((void*)pData, STRATEGY_ALLOC);
    // delete []pData;
    return NV_FALSE;
}

NvError NandStrategyMarkBlockAsBad(
    NvNandHandle hNand,
    NvS32 bank,
    NvS32 block)
{
#if STRESS_TEST_GETALIGNEDPBA
    if (1)
#else
    if (NandStrategyIsBlockBad(hNand, bank, block))
#endif
    {
        ERR_DEBUG_INFO(("\r\n Marking block as bad bank = %d, blk = %d",
            bank, block));
        // mark the pba as bad
        hNand->NandStrat.ttStatus = SetPbaBad(hNand, bank, block);
        ChkILTTErr(hNand->NandStrat.ttStatus, hNand->NandStrat.ttStatus);
    }
    else
    {
        ERR_DEBUG_INFO(("\r\n FALSE bad block indication bank = %d, blk = %d",
            bank, block));
        // It's a false indication resulted because of internal
        // copyback from bad block to a good block. set the pba as unused.
        hNand->NandStrat.ttStatus = SetPbaUnused(hNand, bank, block);
        ChkILTTErr(hNand->NandStrat.ttStatus, hNand->NandStrat.ttStatus);

        // Mark the pba as reserved as we replaced this
        // one with a block from rsvd pool.
        hNand->NandStrat.ttStatus = SetPbaReserved(hNand, bank, block, NV_TRUE);
        ChkILTTErr(hNand->NandStrat.ttStatus, hNand->NandStrat.ttStatus);
    }

    return NvSuccess;
}

void TrackLbaInitialize(
    TrackLba* parent,
    NvS32 NoOfInterleavedBanks,
    NandDeviceInfo *pDevInfo)
{
    NvS32 count = 0;
    NvU64 InterleavedBankSize;

    parent->lba = 0;
    parent->pBlocks = 0;
    parent->pPrevBlocks = 0;
    parent->LastAccessTick = 0ll;
    parent->pPageState = 0;
    parent->LastFreePage = 0;
    parent->FirstFreePage = 0;
    parent->inUse = NV_FALSE;
    parent->NoOfInterleavedBanks = NoOfInterleavedBanks;
    parent->PgsPerLBlk = pDevInfo->PgsPerBlk * NoOfInterleavedBanks;
    parent->NandDevInfo = pDevInfo;

    // Initially Write request is assumed as non-random
    parent->IsWriteRequestRandom = NV_FALSE;

    parent->pPageState =
        AllocateVirtualMemory(
            sizeof(PageInfo) * parent->PgsPerLBlk,
            STRATEGY_ALLOC);
    for (count = 0; count< parent->PgsPerLBlk; count++)
    {
        parent->pPageState[count].state = FREE;
        parent->pPageState[count].PageNumber = count;
    }

    parent->LastAccessTick = 0;
    InterleavedBankSize = sizeof(NvS32) * parent->NoOfInterleavedBanks;
    parent->pBlocks =
        AllocateVirtualMemory(
            (NvU32)InterleavedBankSize,
            STRATEGY_ALLOC);
    parent->pPrevBlocks =
        AllocateVirtualMemory(
        (NvU32)InterleavedBankSize,
        STRATEGY_ALLOC);
    parent->lba = -1;
    parent->bsc4NoOfInterleavedBanks = 
        (NvS32)NandUtilGetLog2(parent->NoOfInterleavedBanks);
    parent->bsc4SctsPerPg = (NvS32)NandUtilGetLog2(pDevInfo->SctsPerPg);
    if (!parent->bsc4SctsPerPg)
        RETURN_VOID;
}

void TrackLbaSetLbaInfo(
    TrackLba* parent,
    NvS32 lba,
    NvS32* pPrevBlocks,
    NvS32* pBlocks)
{
    NvS32 i;
    parent->lba = lba;
    // reset the last free page info
    parent->LastFreePage = 0;
    parent->FirstFreePage = 0;
    parent->IsWriteRequestRandom = NV_FALSE;
    for (i = 0; i <  parent->NoOfInterleavedBanks; i++)
    {
        parent->pBlocks[i] = pBlocks[i];
        parent->pPrevBlocks[i] = pPrevBlocks[i];
    }
}

NvBool TrackLbaIsPages2WriteFree(
    TrackLba* parent,
    NvS32 lPageOffset,
    NvS32 lPageSectorOffset,
    NvS32 noOfSectors)
{
    // find the start page
    NvS32 startPage = lPageSectorOffset >>  parent->bsc4SctsPerPg;
    // convert logical page to relative physical page number
    NvS32 pageNo =
        (lPageOffset <<  parent->bsc4NoOfInterleavedBanks) + startPage;
    NvS32 noOfPages = 1;
    NvBool retVal = NV_TRUE;
    // find the end page
    NvS32 secOffset = lPageSectorOffset + noOfSectors;
    NvS32 endPage =
        (secOffset +  parent->NandDevInfo->SctsPerPg - 1) >>  parent->bsc4SctsPerPg;
    NvS32 i;
    noOfPages = endPage - startPage;
    if((pageNo+noOfPages) > parent->PgsPerLBlk)
            return NV_FALSE;

    for (i = 0; i < noOfPages; i++)
    {
        if (parent->pPageState[pageNo + i].state != FREE)
        {
            retVal = NV_FALSE;
            break;
        }
    }

    return retVal;
}

NvError
TrackLbaMarkPages2WriteBusy(
                           NvNandHandle hNand,
                           TrackLba* parent,
                           NvS32 lPageOffset,
                           NvS32 lPageSectorOffset,
                           NvS32 noOfSectors)
{
    // find the start page
    NvS32 startPage = lPageSectorOffset >>  parent->bsc4SctsPerPg;
    // convert logical page to relative physical page number
    NvS32 pageNo =
        (NvS32) (((NvU32)lPageOffset <<  (NvU32)parent->bsc4NoOfInterleavedBanks) + ((NvU32) startPage));
    NvS32 noOfPages;
    // find the end page
    NvS32 secOffset = lPageSectorOffset + noOfSectors;
    NvS32 endPage =
        (secOffset +  parent->NandDevInfo->SctsPerPg - 1) >>  parent->bsc4SctsPerPg;
    NvS32 i;

    noOfPages = endPage - startPage;

    LOG_NAND_DEBUG_INFO(("\r\n markPages2WriteBusy, lba = %d, pn = %d, nopages = %d",
                        parent->lba, pageNo, noOfPages), IL_STRAT_DEBUG_INFO);

    // In case of MLC, we have to write the pages in sequential order.
    // if we get any request to write the page(s), which fall in the middle
    // of block, first copyback those pages and then execute the write request.

    if (hNand->NandStrat.IsOrderingValid)
        if (pageNo != parent->FirstFreePage)
        {
            RW_TRACE(("\nRandom TRUE\n"));
            parent->IsWriteRequestRandom = NV_TRUE;
        }

    RW_TRACE(("\nGiven page number = %d, Req page no = %d\n", 
        parent->FirstFreePage, pageNo));

    for (i = 0; i < noOfPages; i++)
    {
        parent->pPageState[pageNo + i].state = BUSY;
        // This Value is used only in MLC case.
        parent->pPageState[pageNo + i].PageNumber = parent->FirstFreePage++;
    }

    LOG_NAND_DEBUG_INFO(("\r\n writing the data above the write request, \
                         First free page = %d",
                         parent->FirstFreePage), IL_STRAT_DEBUG_INFO);
    return NvSuccess;
}

void TrackLbaMarkPages2WriteFree(
    TrackLba* parent,
    NvS32 lPageOffset,
    NvS32 lPageSectorOffset,
    NvS32 noOfSectors)
{
    // find the start page
    NvS32 startPage = lPageSectorOffset >>  parent->bsc4SctsPerPg;
    // convert logical page to relative physical page number
    NvS32 pageNo =
        (lPageOffset <<  parent->bsc4NoOfInterleavedBanks) + startPage;
    NvS32 noOfPages;
    // find the end page
    NvS32 secOffset = lPageSectorOffset + noOfSectors;
    NvS32 endPage =
        (secOffset +  parent->NandDevInfo->SctsPerPg - 1) >> parent->bsc4SctsPerPg;
    NvS32 i;

    noOfPages = endPage - startPage;

    LOG_NAND_DEBUG_INFO(("\r\n markPages2WriteFree, lba = %d, pn = %d, nopages = %d",
                        parent->lba, pageNo, noOfPages),
                        IL_STRAT_DEBUG_INFO);

    for (i = 0; i < noOfPages; i++)
    {
        parent->pPageState[pageNo + i].state = FREE;
    }
}

NvBool TrackLbaGetConsecutivePageStatus(
    TrackLba* parent,
    NvS32 startPageOffset,
    NvS32 endPageOffset,
    NvS32* logicalBlockPageOffset,
    NvS32* logicalPageSectorOffset,
    NvS32* pageCount,
    NvS32* sectorCount, NvS32 state)
{
    NvBool retVal = NV_FALSE;
    NvS32 i;
    *pageCount = 0;

    for (i = startPageOffset; i <= endPageOffset; i++)
    {
        if (parent->pPageState[i].state == state)
        {
            *pageCount = *pageCount + 1;
            retVal = NV_TRUE;
        }
        else
            break;
    }

    *logicalBlockPageOffset = startPageOffset >>
        parent->bsc4NoOfInterleavedBanks;
    *logicalPageSectorOffset =
        (startPageOffset % parent->NoOfInterleavedBanks) <<
        parent->bsc4SctsPerPg;
    *sectorCount = *pageCount <<  parent->bsc4SctsPerPg;
    return retVal;
}

// Mark the given set of PBAs pointed by pBlks as BAD
void MarkPBAsetAsBad(NvNandHandle hNand, NvU32* pPages)
{
    NvU8 i =0;
    NvS32 pBlocks[4] = {-1, -1, -1, -1};

    NandStrategyConvertPageNoToBlockNo(hNand, pBlocks, (NvS32 *)pPages);
    for (i = 0; i < hNand->InterleaveCount; i++)
    {
        if (pBlocks[i] != -1)
        {
            hNand->NandStrat.selfStatus =
                NandStrategyMarkBlockAsBad(hNand, i, pBlocks[i]);
            if (hNand->NandStrat.selfStatus != NvSuccess)
            {
                NvOsDebugPrintf("\n [Nand_Strategy] Failed to mark PBAs BAD");
                NV_ASSERT(NV_FALSE);
            }
        }
     }
}

/*
 * StrategyHandleError - handles the error that happens during read/write 
 * operations in FTL layer. This traverses through the tracking list to find 
 * out matching LogicalBlock and then calls StrategyPrivDoCopyback.
 * @param LogicalBlockNumber: logical block number for which we need to find a
 * replacement physical block.
 * @param StartingLBAPageOffset: Physical PageOffset from the starting of the 
 LBA.e.g. If we have two way interleave column and our starting page request 
 is from second Page number of block at column # 2, then 
 StartingLBAPageOffset = 3, ( 2 pages from column #1 + 1 page from column #2)
 * @param PhysicalPagesAccessedBeforeFail: Number Of physical Pages Written/
 Read before the failure happened
 * @pram NumberOfPages: Number of pages for which the failed request arrived.
 * @pram IsWriteError: A variable that is used in case of write errors to 
clear 
     the tracking buffer.based on the failed page & sector offsets.
*/
NvError
    StrategyHandleError (
    NvNandHandle hNand,
    NvU32 LogicalBlockNumber,
    NvU32 StartingLBAPageOffset,
    NvU32 PhysicalPagesAccessedBeforeFail,
    NvU32 NumberOfPages,
    NvBool IsWriteError
    )
{
    NvS32 logicalBlock2Replace = LogicalBlockNumber;
    NvS32 index;
    NvBool oldBlockIsBad;
    NvBool IsLBAInTracking;
    NvU32 i;
    NvU32 OldPages[4] = {-1, -1, -1, -1};
    NvU32 NewPages[4] = {-1, -1, -1, -1};
    CopyBackHandler CpyBakHandler;

    NvU32 LogicalFailePageOffset = (StartingLBAPageOffset + 
        PhysicalPagesAccessedBeforeFail) / hNand->InterleaveCount;
    NvU32 logicalFailedSectorOffset = (StartingLBAPageOffset + 
        PhysicalPagesAccessedBeforeFail) % hNand->InterleaveCount;
    #ifdef NEW_STRAT_DEBUG
    static NvU8 ErrorCount = 0;
    ErrorCount++;
    if(ErrorCount == 5)
    {
        DumpTT(hNand, -1);
        ErrorCount = 0;
    }
    #endif
    ERR_DEBUG_INFO(("\nTime: 0x%x",NvOsGetTimeUS()));
    logicalFailedSectorOffset <<= hNand->NandStrat.bsc4SctsPerPg;
    ERR_DEBUG_INFO(("\n +++++++++++++++++ Strategy Handle error "));
    // Check if the LBA is in tracking
    IsLBAInTracking = NandStrategyIsLbaInTrackingList(hNand,
        logicalBlock2Replace, &index);
    if (IsLBAInTracking == NV_TRUE)
    {
        ERR_DEBUG_INFO(("\r\n BadBlock is under tracking"));
        // see which block is bad, the new block or old block.
        // replace the appropriate one.
        oldBlockIsBad = TrackLbaIsPages2WriteFree(
            &hNand->NandStrat.pTrackLba[index], LogicalFailePageOffset,
            logicalFailedSectorOffset, 1);

        // If old block is Bad, replace the old PBAs
        if (oldBlockIsBad)
        {
            ERR_DEBUG_INFO(("\r\n old PBA to replace"));
            // get a new PBA to replace Bad block
            hNand->NandStrat.ttStatus = 
                GetAlignedPBA(hNand,
                    hNand->NandStrat.pTrackLba[index].lba,
                    hNand->NandStrat.pTrackLba[index].pPrevBlocks,
                    (NvS32 *)NewPages, NV_TRUE);
            ChkILTTErr(hNand->NandStrat.ttStatus, NvError_NandNoFreeBlock);

            for (i = 0; i < (NvU32)hNand->InterleaveCount; i++)
            {
                OldPages[i] = hNand->NandStrat.pTrackLba[index].pPrevBlocks[i] 
                    << hNand->NandStrat.bsc4PgsPerBlk;
                NewPages[i] <<= hNand->NandStrat.bsc4PgsPerBlk;
            }
             ERR_DEBUG_INFO(("\r\n bad PBA: %d in IntrlvColumn %d ",
                hNand->NandStrat.pTrackLba[index].pPrevBlocks[logicalFailedSectorOffset >> hNand->NandStrat.bsc4SctsPerPg],
                (logicalFailedSectorOffset >> hNand->NandStrat.bsc4SctsPerPg)));
            // copy back all previously FREE marked pages from the failed block 
            // to new block

            CpyBakHandler.pSrcPageNumbers = OldPages;
            CpyBakHandler.pDestPageNumbers = NULL;
            CpyBakHandler.ppNewDestPageNumbers = NewPages;
            CpyBakHandler.pTrackingBuf = 
                &(hNand->NandStrat.pTrackLba[index].pPageState[0]);

            hNand->NandStrat.selfStatus =
                StrategyPrivDoCopyback(hNand, &CpyBakHandler, NV_FALSE,
                CopyBackMode_READ_SRC_FAIL);

            CheckNvError(hNand->NandStrat.selfStatus, 
                hNand->NandStrat.selfStatus);

            NandStrategyConvertPageNoToBlockNo(
                hNand, hNand->NandStrat.pTrackLba[index].pPrevBlocks, 
                (NvS32 *)CpyBakHandler.ppNewDestPageNumbers);
        }
        // If new block is Bad, replace the new PBAs
        else
        {
            ERR_DEBUG_INFO(("\r\n new PBA to replace"));
            // clear the tracking buffer entries for the pages that were not
            // written.
            if (IsWriteError == NV_TRUE)
            {
                i = (StartingLBAPageOffset + PhysicalPagesAccessedBeforeFail);
                for (; i < (StartingLBAPageOffset + NumberOfPages); i++)
                {
                    hNand->NandStrat.pTrackLba[index].pPageState[i].state = FREE;
                    hNand->NandStrat.pTrackLba[index].FirstFreePage--;
                }
            }

            ERR_DEBUG_INFO(("\r\n bad PBA: %d in IntrlvColumn %d ",
            hNand->NandStrat.pTrackLba[index].pBlocks[logicalFailedSectorOffset >> hNand->NandStrat.bsc4SctsPerPg],
                (logicalFailedSectorOffset >> hNand->NandStrat.bsc4SctsPerPg)));
            // get a new PBA to replace Bad block
            hNand->NandStrat.ttStatus =
                GetAlignedPBA(hNand, hNand->NandStrat.pTrackLba[index].lba,
                    hNand->NandStrat.pTrackLba[index].pBlocks,
                    (NvS32*)NewPages, NV_TRUE);
            ChkILTTErr(hNand->NandStrat.ttStatus, NvError_NandNoFreeBlock);

            for (i = 0; i < (NvU32)hNand->InterleaveCount; i++)
            {
                OldPages[i] = hNand->NandStrat.pTrackLba[index].pBlocks[i] <<
                    hNand->NandStrat.bsc4PgsPerBlk;
                NewPages[i] <<= hNand->NandStrat.bsc4PgsPerBlk;
            }
            // copy back all previously BUSY marked pages from the failed block 
            // to new block

            CpyBakHandler.pSrcPageNumbers = NULL;
            CpyBakHandler.pDestPageNumbers = OldPages;
            CpyBakHandler.ppNewDestPageNumbers = NewPages;
            CpyBakHandler.pTrackingBuf = 
                &(hNand->NandStrat.pTrackLba[index].pPageState[0]);

            // We come here when fail occured in reading or writing to DEST blocks.
            // CopyBackMode_READ_DEST_FAIL is same case as CopyBackMode_WRITE_FAIL
            // The Destination blocks is marked as bad in CopySrcToDest which is
            // called in StrategyPrivDoCopyback.
            hNand->NandStrat.selfStatus =
                StrategyPrivDoCopyback(hNand, &CpyBakHandler, NV_FALSE,
                CopyBackMode_READ_DEST_FAIL);
            CheckNvError(hNand->NandStrat.selfStatus,
                hNand->NandStrat.selfStatus);
/*            
            for (i = 0; i < (NvU32)hNand->InterleaveCount; i++)
            {
                // mark the old block as bad
                hNand->NandStrat.selfStatus =
                    NandStrategyMarkBlockAsBad(
                        hNand, i,
                        hNand->NandStrat.pTrackLba[index].pBlocks[i]);
                CheckNvError(hNand->NandStrat.selfStatus, 
                    hNand->NandStrat.selfStatus);
                // Update page numbers
                hNand->NandStrat.pTrackLba[index].pBlocks[i] = 
                    (NewPages[i] >> hNand->NandStrat.bsc4PgsPerBlk);
            }
*/
            NandStrategyConvertPageNoToBlockNo(hNand,
                hNand->NandStrat.pTrackLba[index].pBlocks, (NvS32 *)NewPages);
        }
    }
    else
    {
        ERR_DEBUG_INFO(("\r\n PBA not in tracking"));
        // clear the required arrays.
        NandStrategyclearPhysicalPageArray(hNand, hNand->NandStrat.pTempBlocks);
        // get the actual physical block(s) to which our lba is pointing to
        hNand->NandStrat.ttStatus =
            GetPBA(hNand, logicalBlock2Replace, hNand->NandStrat.pTempBlocks);
        ChkILTTErr(hNand->NandStrat.ttStatus, hNand->NandStrat.ttStatus);
        ERR_DEBUG_INFO(("\r\n bad PBA: %d in IntrlvColumn %d ",
            hNand->NandStrat.pTempBlocks[logicalFailedSectorOffset >> hNand->NandStrat.bsc4SctsPerPg],
            (logicalFailedSectorOffset >> hNand->NandStrat.bsc4SctsPerPg)));
        // get a new PBA to replace Bad block
        hNand->NandStrat.ttStatus = GetAlignedPBA(hNand,
                -1,hNand->NandStrat.pTempBlocks, (NvS32 *)NewPages, NV_TRUE);
        ChkILTTErr(hNand->NandStrat.ttStatus, NvError_NandNoFreeBlock);

        for (i = 0; i < (NvU32)hNand->InterleaveCount; i++)
        {
            OldPages[i] = hNand->NandStrat.pTempBlocks[i] << 
                hNand->NandStrat.bsc4PgsPerBlk;
            NewPages[i] <<= hNand->NandStrat.bsc4PgsPerBlk;
        }

        CpyBakHandler.pSrcPageNumbers = OldPages;
        CpyBakHandler.pDestPageNumbers = NULL;
        CpyBakHandler.ppNewDestPageNumbers = NewPages;
        CpyBakHandler.pTrackingBuf = NULL;

        hNand->NandStrat.ttStatus =
            StrategyPrivDoCopyback(hNand, &CpyBakHandler, NV_FALSE, 
            CopyBackMode_READ_FAIL);
        CheckNvError(hNand->NandStrat.selfStatus, hNand->NandStrat.selfStatus);

        for (i = 0; i < (NvU32)hNand->InterleaveCount; i++)
        {
            NewPages[i] >>= hNand->NandStrat.bsc4PgsPerBlk;
        }

        if (hNand->NandStrat.ttStatus == NvSuccess)
        {
            // Assign the new PBA to our LBA for future reference
            hNand->NandStrat.ttStatus =
                AssignPba2Lba(
                    hNand,
                    (NvS32 *)NewPages,
                    logicalBlock2Replace,
                    NV_TRUE);
            ChkILTTErr(hNand->NandStrat.ttStatus, hNand->NandStrat.ttStatus);
        }
        else
        {
            // Replace failed, indicating that there are no good blocks
            // to replace the bad one with.
            ERR_DEBUG_INFO(("\r\n Replace bad block failed"));
            RETURN(NvError_NandTLFailed);
        }

        // The Source blocks are marked as bad in CopySrcToDest which is
        // called from StrategyPrivDoCopyback.
/*
        for (i = 0; i < (NvU32)hNand->InterleaveCount; i++)
        {
            // mark the old block as bad
            hNand->NandStrat.selfStatus =
                NandStrategyMarkBlockAsBad(
                    hNand, i,
                     hNand->NandStrat.pTempBlocks[i]);
            CheckNvError(hNand->NandStrat.selfStatus, hNand->NandStrat.selfStatus);
        }
*/
    }
    ERR_DEBUG_INFO(("\nExiting Handle Error "));
    return NvSuccess;
}

// Returns the number of pages that the same state is present in the tracking list
static NvU32 
NumberOfPagesInSameState(PageInfo* pTrackingBuf, NvU8 State, NvU32 MaxVal)
{
    NvU32 i = 0;
    while((i < MaxVal) && (pTrackingBuf[i].state == State))
    {
        i++;
    }
    return i;
}

/*
 check for all copy back errors 
 If Write error, replace the block with a new block and adjust the pagenumbers
 If all read errors, set Ignore Ecc to TRUE
 For all other errors, ASSERT.
*/
static NvError 
HandleErrorsInCopy(
    NvNandHandle hNand,
    CopyBackIntHandler * pCpyBakIntHandler,
    NvError e,
    NvBool IsWriteRequestRandom,
    NvU32 * pTrackSectorOffset,
    NvU32 * pNumberOfPages2BeTracked)
{
    NvS32 DstnBlocks[4] = {-1, -1, -1, -1};
    NvS32 NewDstnBlocks[4] = {-1, -1, -1, -1};
    NvS32 DstnPageNumbers[4] = {-1, -1, -1, -1};
    NvU32 i = 0;

    NandStrategyConvertPageNoToBlockNo(hNand, DstnBlocks,
        (NvS32 *)pCpyBakIntHandler->pDestPageNumbers);

    hNand->NandStrat.ttStatus =
        GetAlignedPBA(hNand, -1, DstnBlocks, NewDstnBlocks, NV_TRUE);
    ChkILTTErr(hNand->NandStrat.ttStatus, NvError_NandNoFreeBlock);

    NandStrategyConvertBlockNoToPageNo(
        hNand, NewDstnBlocks, DstnPageNumbers, 0);

   if (e == NvError_NandWriteFailed)
   {
        if (!pCpyBakIntHandler->IgnoreECCError)
        {
            // If SLC flush fails due to write, New block is taken and merged.
            pCpyBakIntHandler->pNewDestPageNums = (NvU32 *)DstnPageNumbers;
            MergeSrcNDest(hNand, pCpyBakIntHandler, IsWriteRequestRandom);

            MarkPBAsetAsBad(hNand, pCpyBakIntHandler->pDestPageNumbers);
            for (i = 0; i < (NvU32)hNand->InterleaveCount; i++)
            {
                pCpyBakIntHandler->pDestPageNumbers[i] = 
                    pCpyBakIntHandler->pNewDestPageNums[i];
            }
            *pNumberOfPages2BeTracked = 0;
        }
        else
        {
            // If destination to new destination fails, New destination 
            // is replaced.
            *pNumberOfPages2BeTracked = hNand->NandStrat.PgsPerLBlk;
            *pTrackSectorOffset = 0;
            MarkPBAsetAsBad(hNand, pCpyBakIntHandler->pDestPageNumbers);
            NandStrategyConvertBlockNoToPageNo(hNand,
                NewDstnBlocks, (NvS32 *)pCpyBakIntHandler->pDestPageNumbers, 0);
        }
    }
    else if ((e == NvError_NandReadFailed) ||
        (e == NvError_NandReadEccFailed) ||
        (e == NvError_NandErrorThresholdReached))
    {
        if (!pCpyBakIntHandler->IgnoreECCError)
        {
            // If SLC flush fails due to read, New block is taken and merged.
            pCpyBakIntHandler->IgnoreECCError = NV_TRUE;
            pCpyBakIntHandler->pNewDestPageNums = (NvU32 *)DstnPageNumbers;
            MergeSrcNDest(hNand, pCpyBakIntHandler, IsWriteRequestRandom);
            // mark the old block as unused
            for (i = 0; i < (NvU32)hNand->InterleaveCount; i++)
            {
                hNand->NandStrat.selfStatus =
                    SetPbaUnused(hNand, i, 
                        pCpyBakIntHandler->pDestPageNumbers[i]);
                CheckNvError(hNand->NandStrat.selfStatus,
                    hNand->NandStrat.selfStatus);
                pCpyBakIntHandler->pDestPageNumbers[i] = 
                    pCpyBakIntHandler->pNewDestPageNums[i];
            }
            *pNumberOfPages2BeTracked = 0;
        }
    }
    else
    {
        // We do not handle any other errors.
        NvOsDebugPrintf("\n\n **** Fail: Invalid Case **** \n\n");
    }
    return NvSuccess;
}

// Following test keeps getting a new block from the reserved pool and mark it
// as BAD until the GetAlignedPba returns NO_FREE_BLOCK.
#if STRESS_TEST_GETALIGNEDPBA
void StressTestGetAlignedPBA(NvNandHandle hNand)
{

    NvS32 OldBlks[4] = {-1, -1, -1, -1};
    NvS32 Temp[4] = {-1, -1, -1, -1};
    NvU32 Pages[4] = {-1, -1, -1, -1};
    NvU8 i = 0;
    NvS32* pOldBlks = NULL;
    NvS32 Status = 0;
    do
    {
        Status = GetAlignedPBA(
                    hNand,
                    0,
                    pOldBlks,
                    Temp,
                    NV_TRUE);

        NandStrategyConvertBlockNoToPageNo(hNand, OldBlks, Pages, 0);
        MarkPBAsetAsBad(hNand, Pages);
        for (i = 0; i < hNand->InterleaveCount; i++)
        {
            OldBlks[i] = Temp[i];
            Temp[i] = -1;
        }
        pOldBlks = OldBlks;

     } while(Status != TT_NO_FREE_BLOCK);
    NV_ASSERT(NV_FALSE);
    NvOsDebugPrintf(" \n\n\n\nNO FREE BLOCKS\n\n\n\n");
}
#endif

/*
 check for all copy back errors 
 If Write error, replace the block with a new block and adjust the pagenumbers
 If all read errors, set Ignore Ecc to TRUE
 For all other errors, ASSERT.
*/
static NvError 
HandleErrorsInMerge(
    NvNandHandle hNand,
    NvU32* pPageNums,
    NvError e, 
    NvBool *IgnoreEccError)
{
    NvU8 i;
    NvS32 OldBlks[4] = {-1, -1, -1, -1};
    NvS32 Temp[4] = {-1, -1, -1, -1};
    ERR_DEBUG_INFO(("\nEntering Cpybk error chk"));
    ERR_DEBUG_INFO(("\n Old PBAs"));
    for (i = 0; i < hNand->InterleaveCount; i++)
    {
        OldBlks[i] = ((pPageNums)[i]) >> hNand->NandStrat.bsc4PgsPerBlk;
        ERR_DEBUG_INFO(("\n %d", OldBlks[i]));
    }
    if (e == NvError_NandWriteFailed)
    {
        // get a new PBA to replace pNewDestPageNumbers
        hNand->NandStrat.ttStatus =
            GetAlignedPBA(
                hNand,
                -1,
                (int *)(&OldBlks[0]),
                Temp,
                NV_TRUE);
        ChkILTTErr(hNand->NandStrat.ttStatus, NvError_NandNoFreeBlock);

        ERR_DEBUG_INFO(("\n New PBAs"));
        for (i = 0; i < hNand->InterleaveCount; i++)
        {
            ERR_DEBUG_INFO(("\t %d ", Temp[i]));
        }
        // mark the old block as bad
        for (i = 0; i < hNand->InterleaveCount; i++)
        {
            hNand->NandStrat.selfStatus =
                NandStrategyMarkBlockAsBad(
                    hNand, i,
                    OldBlks[i]);
            CheckNvError(hNand->NandStrat.selfStatus, hNand->NandStrat.selfStatus);
        // Update the pNewDestPageNumbers with the new PBA returned
            (pPageNums)[i] = Temp[i] << hNand->NandStrat.bsc4PgsPerBlk;
        }
    }
    else if ((e == NvError_NandReadFailed) ||
        (e == NvError_NandReadEccFailed) ||
        (e == NvError_NandErrorThresholdReached))
    {
        if (!(*IgnoreEccError))
        {
            // Repalce Dest Blocks
            // Set old des blocks as unused

            // get a new PBA to replace pNewDestPageNumbers
            hNand->NandStrat.ttStatus =
            GetAlignedPBA(
                hNand,
                -1,
                (int *)(&OldBlks[0]),
                Temp,
                NV_TRUE);
            ChkILTTErr(hNand->NandStrat.ttStatus, NvError_NandNoFreeBlock);

            ERR_DEBUG_INFO(("\n New PBAs"));
            for (i = 0; i < hNand->InterleaveCount; i++)
            {
                ERR_DEBUG_INFO(("\t %d ", Temp[i]));
            }

            // mark the old block as bad
            for (i = 0; i < hNand->InterleaveCount; i++)
            {
                hNand->NandStrat.selfStatus =
                    SetPbaUnused(hNand, i, OldBlks[i]);
                CheckNvError(hNand->NandStrat.selfStatus, hNand->NandStrat.selfStatus);
                // Update the pNewDestPageNumbers with the new PBA returned
                (pPageNums)[i] = Temp[i] << hNand->NandStrat.bsc4PgsPerBlk;
            }
            *IgnoreEccError = NV_TRUE;
        }
    }
    else
    {
        // We do not handle any other errors.
        NV_ASSERT(NV_FALSE);
    }
    ERR_DEBUG_INFO(("\nExiting Cpybk error chk"));
    return NvSuccess;
}

NvError CopySrcToDest(
    NvNandHandle hNand, 
    CopyBackIntHandler * pCpyBakIntHandler, 
    NvBool IsWriteRequestRandom)
{
    NvU32 i = 0;
    NvError e = NvSuccess;
    NvU32 TrackSectorOffset = 0;
    NvU32 NumberOfPages2BeTracked = 0;
    NvU32 NumberOfPagesToSkip = 0;
    NvU32 NumberOfPagesToCopy = 0;

    NvS32 SourcePageNumbers[4] = {-1, -1, -1, -1};
    NvS32 DstnPageNumbers[4] = {-1, -1, -1, -1};
    NvS32 SectorCount;
    NvU32 NoOfPagesInOneTrans = 0;
    NvU32 SrcSectorOffset = 0;
    NvU32 DestSectorOffset = 0;
    NvU8 State2CpyBak = pCpyBakIntHandler->State;
    NvU8 State2Ignore = State2CpyBak ? FREE : BUSY;
    ERR_DEBUG_INFO(("\nEntering CopySrcToDest"));

    RW_TRACE(("\r\n State2CpyBak = %d", State2CpyBak));
    NumberOfPages2BeTracked = hNand->NandStrat.PgsPerLBlk;
    while (NumberOfPages2BeTracked > 0)
    {
        RW_TRACE(("\r\n NumberOfPages2BeTracked = %d", NumberOfPages2BeTracked));
        NumberOfPagesToSkip = NumberOfPagesInSameState(
            &(pCpyBakIntHandler->pTrackingBuf[TrackSectorOffset]), 
            State2Ignore, NumberOfPages2BeTracked);
        NumberOfPages2BeTracked -= NumberOfPagesToSkip;
        if (NumberOfPages2BeTracked == 0)
            break;
        TrackSectorOffset += NumberOfPagesToSkip;
        DestSectorOffset = (TrackSectorOffset % hNand->NandStrat.NoOfILBanks)
            << hNand->NandStrat.bsc4SctsPerPg;
        NumberOfPagesToCopy = NumberOfPagesInSameState(
            &(pCpyBakIntHandler->pTrackingBuf[TrackSectorOffset]),
            State2CpyBak, NumberOfPages2BeTracked);
        RW_TRACE(("\r\n NumberOfPagesToCopy = %d", NumberOfPagesToCopy));
        for (i = 0; i < (NvU32)hNand->InterleaveCount; i++)
        {
            if (pCpyBakIntHandler->pDestPageNumbers[i] == -1)
            {
                SourcePageNumbers[i] = -1;
            }
            else
            {
                if (IsWriteRequestRandom && (State2CpyBak == BUSY))
                {
                    SourcePageNumbers[i] = pCpyBakIntHandler->pSrcPageNumbers[i] + 
                        (pCpyBakIntHandler->pTrackingBuf[TrackSectorOffset].PageNumber 
                            / hNand->NandStrat.NoOfILBanks);
                    SrcSectorOffset = 
                        (pCpyBakIntHandler->pTrackingBuf[TrackSectorOffset].PageNumber %
                        hNand->NandStrat.NoOfILBanks) <<
                        hNand->NandStrat.bsc4SctsPerPg;
                }
                else
                {
                    SourcePageNumbers[i] = pCpyBakIntHandler->pSrcPageNumbers[i] + 
                    (TrackSectorOffset / hNand->NandStrat.NoOfILBanks);
                    SrcSectorOffset = DestSectorOffset;
                }
                DstnPageNumbers[i] = pCpyBakIntHandler->pDestPageNumbers[i] + 
                    (TrackSectorOffset / hNand->NandStrat.NoOfILBanks);
            }
        }
        if (!NumberOfPagesToCopy)
            continue;

        if (IsWriteRequestRandom && (State2CpyBak == BUSY))
        {
            NoOfPagesInOneTrans = GetNoOfContinuousPages(
                pCpyBakIntHandler->pTrackingBuf, NumberOfPagesToCopy, TrackSectorOffset);
        }
        else
            NoOfPagesInOneTrans = NumberOfPagesToCopy;

        RW_TRACE(("\r\n NoOfPagesInOneTrans = %d", NoOfPagesInOneTrans));
        NumberOfPages2BeTracked -= NoOfPagesInOneTrans;
        SectorCount = NoOfPagesInOneTrans << hNand->NandStrat.bsc4SctsPerPg;

        // NumberOfPages2BeTracked -= (NumberOfPagesToCopy + NumberOfPagesToSkip);
        TrackSectorOffset += NoOfPagesInOneTrans;
        if (SectorCount > 0)
        {
            for (i = 0; i < (NvU32)hNand->InterleaveCount; i++)
            {
                RW_TRACE(("\nCpy bak: case 1: Src Blk = %d, Dest Blk = %d SrcPageOffset = %d, DestPageOffset = %d No of pages = %d\n", 
                  SourcePageNumbers[i] / hNand->NandDevInfo.PgsPerBlk,
                    DstnPageNumbers[i] / hNand->NandDevInfo.PgsPerBlk,
                    SourcePageNumbers[i] % hNand->NandDevInfo.PgsPerBlk, 
                    DstnPageNumbers[i] % hNand->NandDevInfo.PgsPerBlk, 
                    NoOfPagesInOneTrans));
            }
            e = NandTLCopyBack(
                hNand, SourcePageNumbers, DstnPageNumbers,
                SrcSectorOffset, DestSectorOffset, &SectorCount, 
                pCpyBakIntHandler->IgnoreECCError);
            if (e != NvSuccess)
            {
                RW_TRACE(("\r\n Error b4 HandleErrorsInCopy 0x%08X", e));
                e = HandleErrorsInCopy(hNand, pCpyBakIntHandler, e, 
                    IsWriteRequestRandom, &TrackSectorOffset,
                    &NumberOfPages2BeTracked);
                CheckNvError(e, e);
            }
        }
    }

    if (pCpyBakIntHandler->IgnoreECCError)
    {
        //Mark Src block as bad
        MarkPBAsetAsBad(hNand, pCpyBakIntHandler->pSrcPageNumbers);
    }
    return e;
}

NvError MergeSrcNDest(
    NvNandHandle hNand, 
    CopyBackIntHandler * pCpyBakIntHandler, 
    NvBool IsWriteRequestRandom)
{
    NvU32 i = 0;
    NvError e = NvSuccess;
    NvU32 TrackSectorOffset = 0;
    NvU32 NumberOfPages2BeTracked = 0;
    NvU32 NumberOfPagesToCopy = 0;
    NvU32 * pSrc = NULL;
    NvS32 SourcePageNumbers[4] = {-1, -1, -1, -1};
    NvS32 DstnPageNumbers[4] = {-1, -1, -1, -1};
    NvS32 SectorCount;
    NvU32 NoOfPagesInOneTrans = 0;
    NvU32 SrcSectorOffset = 0;
    NvU32 DestSectorOffset = 0;
    NvU8 State2CpyBak = BUSY;
    NvBool IgnoreSrcEccError = pCpyBakIntHandler->IgnoreECCError;
    NvBool IgnoreDestEcccError = NV_FALSE;

    ERR_DEBUG_INFO(("\nEntering MergeSrcNDest"));
    NumberOfPages2BeTracked = hNand->NandStrat.PgsPerLBlk;
    TrackSectorOffset = 0;
    while (NumberOfPages2BeTracked > 0)
    {
        RW_TRACE(("\r\n NumberOfPages2BeTracked = %d", NumberOfPages2BeTracked));
        State2CpyBak = BUSY;
        pSrc = pCpyBakIntHandler->pDestPageNumbers;
        DestSectorOffset = (TrackSectorOffset % hNand->NandStrat.NoOfILBanks) 
            << hNand->NandStrat.bsc4SctsPerPg;
        NumberOfPagesToCopy = NumberOfPagesInSameState(
            &pCpyBakIntHandler->pTrackingBuf[TrackSectorOffset], State2CpyBak, NumberOfPages2BeTracked);
        if (!NumberOfPagesToCopy)
        {
            State2CpyBak = FREE;
            pSrc = pCpyBakIntHandler->pSrcPageNumbers;
            NumberOfPagesToCopy = NumberOfPagesInSameState(
                &pCpyBakIntHandler->pTrackingBuf[TrackSectorOffset], State2CpyBak, NumberOfPages2BeTracked);
        }
        RW_TRACE(("\r\n State2CpyBak in merge = %d", State2CpyBak));
        if (!NumberOfPagesToCopy)
            NV_ASSERT(0);
        RW_TRACE(("\r\n NumberOfPagesToCopy = %d", NumberOfPagesToCopy));
        for (i = 0; i < (NvU32)hNand->InterleaveCount; i++)
        {
            if (pSrc[i] == -1)
            {
                SourcePageNumbers[i] = -1;
                NV_ASSERT(0);
            }
            else
            {
                if (IsWriteRequestRandom && (State2CpyBak == BUSY))
                {
                    SourcePageNumbers[i] = pSrc[i] + 
                        (pCpyBakIntHandler->pTrackingBuf[TrackSectorOffset].PageNumber 
                            / hNand->NandStrat.NoOfILBanks);
                    SrcSectorOffset = 
                        (pCpyBakIntHandler->pTrackingBuf[TrackSectorOffset].PageNumber %
                        hNand->NandStrat.NoOfILBanks) <<
                    hNand->NandStrat.bsc4SctsPerPg;
                }
                else
                {
                    SourcePageNumbers[i] = pSrc[i] + 
                    (TrackSectorOffset / hNand->NandStrat.NoOfILBanks);
                    SrcSectorOffset = DestSectorOffset;
                }
                DstnPageNumbers[i] = pCpyBakIntHandler->pNewDestPageNums[i] + 
                    (TrackSectorOffset / hNand->NandStrat.NoOfILBanks);

            }
        }

        if (IsWriteRequestRandom && (State2CpyBak == BUSY))
        {
            NoOfPagesInOneTrans = GetNoOfContinuousPages(
                pCpyBakIntHandler->pTrackingBuf, NumberOfPagesToCopy, TrackSectorOffset);
        }
        else
            NoOfPagesInOneTrans = NumberOfPagesToCopy;
        RW_TRACE(("\r\n NoOfPagesInOneTrans = %d", NoOfPagesInOneTrans));
        SectorCount = NoOfPagesInOneTrans << hNand->NandStrat.bsc4SctsPerPg;
        if (SectorCount > 0)
        {
            NvBool * IgnoreEccError = NULL;
            if (State2CpyBak == FREE)
                IgnoreEccError = &IgnoreSrcEccError;
            else
                IgnoreEccError = &IgnoreDestEcccError;
            for (i = 0; i < (NvU32)hNand->InterleaveCount; i++)
            {
                RW_TRACE(("\nCpy bak: case 2: Src Blk = %d, New Dest Blk = %d SrcOffset = %d, DestOffset = %d No of pages = %d\n", 
                  SourcePageNumbers[i] / hNand->NandDevInfo.PgsPerBlk,
                    DstnPageNumbers[i] / hNand->NandDevInfo.PgsPerBlk,
                    SourcePageNumbers[i] % hNand->NandDevInfo.PgsPerBlk, 
                    DstnPageNumbers[i] % hNand->NandDevInfo.PgsPerBlk, 
                    NoOfPagesInOneTrans));
            }
            e = NandTLCopyBack(
                hNand, SourcePageNumbers, DstnPageNumbers,
                SrcSectorOffset, DestSectorOffset, &SectorCount, 
                *IgnoreEccError);
            if (e != NvSuccess)
            {
                RW_TRACE(("\r\n Error b4 HandleErrorsInMerge 0x%08X", e));
                HandleErrorsInMerge(hNand, 
                (pCpyBakIntHandler->pNewDestPageNums), e, IgnoreEccError);
                NumberOfPages2BeTracked = hNand->NandStrat.PgsPerLBlk;
                TrackSectorOffset = 0;
            }
            else
            {
                NumberOfPages2BeTracked -= NoOfPagesInOneTrans;
                TrackSectorOffset += NoOfPagesInOneTrans;
            }
        }
    }

    if (IgnoreSrcEccError)
        MarkPBAsetAsBad(hNand, pCpyBakIntHandler->pSrcPageNumbers);
    if (IgnoreDestEcccError)
        MarkPBAsetAsBad(hNand, pCpyBakIntHandler->pDestPageNumbers);
    return e;
}

/*
 *StrategyPrivDoCopyback handles all error cases that happens while doing read 
 * write operations. This function also handles all copy back operations that 
 * happens in FTL layer.
 * This function should handle all error cases of copy back.
 * @param pSrcBlock: pointer to source block.
 * @param pDestBlock: pointer to destination block.
 * @param pNewDestBlock: pointer to new destination block.
 * @param pCpyBakHandler->pTrackingBuffer: pointer to tracking buffer
 * @param pCpyBakHandler->NumberOfPages: Number of pages to be copied back. if it is set to 
 "-1" then get the required data from tracking buffer.
 */
NvError 
    StrategyPrivDoCopyback(
        NvNandHandle hNand,
        CopyBackHandler * pCpyBakHandler,
        NvBool IsWriteRequestRandom,
        CopyBackMode CpyBakmode)
{
    NvError e = NvSuccess;

    CopyBackIntHandler CpyBakIntHandler = {0};
    PageInfo * pPageState = NULL;
    ERR_DEBUG_INFO(("\nEntering Priv Cpybk "));

    switch (CpyBakmode)
    {
        // Ignore ECC
        case CopyBackMode_READ_FAIL:
            // In this case whole block is copied back irrespective of 
            // tracking buffer. So fill the temp tracking buffer with all FREE 
            // values and copy call FREE pages.
            RW_TRACE(("\r\n CopyBackMode_READ_FAIL"));
            pPageState = AllocateVirtualMemory(
                sizeof(PageInfo) << hNand->NandStrat.bsc4PgsPerLBlk,
                STRATEGY_ALLOC);
            NvOsMemset(pPageState, FREE, 
                sizeof(PageInfo) << hNand->NandStrat.bsc4PgsPerLBlk);
            CpyBakIntHandler.pSrcPageNumbers = pCpyBakHandler->pSrcPageNumbers;
            CpyBakIntHandler.pDestPageNumbers = pCpyBakHandler->ppNewDestPageNumbers;
            CpyBakIntHandler.pTrackingBuf = pPageState;
            CpyBakIntHandler.pNewDestPageNums = NULL;
            CpyBakIntHandler.IgnoreECCError = NV_TRUE;
            CpyBakIntHandler.State = FREE;
            break;
        case CopyBackMode_READ_SRC_FAIL:
            RW_TRACE(("\r\n CopyBackMode_READ_SRC_FAIL"));
            CpyBakIntHandler.pSrcPageNumbers = pCpyBakHandler->pSrcPageNumbers;
            CpyBakIntHandler.pDestPageNumbers = pCpyBakHandler->ppNewDestPageNumbers;
            CpyBakIntHandler.pTrackingBuf = pCpyBakHandler->pTrackingBuf;
            CpyBakIntHandler.pNewDestPageNums = NULL;
            CpyBakIntHandler.IgnoreECCError = NV_TRUE;
            CpyBakIntHandler.State = FREE;
            break;
        case CopyBackMode_READ_DEST_FAIL:
        case CopyBackMode_WRITE_FAIL:
            RW_TRACE(("\r\n CopyBackMode_READ_DEST_FAIL"));
            CpyBakIntHandler.pSrcPageNumbers = pCpyBakHandler->pDestPageNumbers;
            CpyBakIntHandler.pDestPageNumbers = pCpyBakHandler->ppNewDestPageNumbers;
            CpyBakIntHandler.pTrackingBuf = pCpyBakHandler->pTrackingBuf;
            CpyBakIntHandler.pNewDestPageNums = NULL;
            CpyBakIntHandler.IgnoreECCError = NV_TRUE;
            CpyBakIntHandler.State = BUSY;
            break;
        case CopyBackMode_FLUSH_DEST_MERGE:
            RW_TRACE(("\r\n In CopyBackMode_FLUSH_DEST_MERGE"));
            CpyBakIntHandler.pSrcPageNumbers = pCpyBakHandler->pDestPageNumbers;
            CpyBakIntHandler.pDestPageNumbers = pCpyBakHandler->ppNewDestPageNumbers;
            CpyBakIntHandler.pTrackingBuf = pCpyBakHandler->pTrackingBuf;
            CpyBakIntHandler.pNewDestPageNums = NULL;
            CpyBakIntHandler.IgnoreECCError = NV_FALSE;
            CpyBakIntHandler.State = BUSY;
            break;
        case CopyBackMode_FLUSH_SRC_DEST:
            RW_TRACE(("\r\n In CopyBackMode_FLUSH_SRC_DEST"));
            CpyBakIntHandler.pSrcPageNumbers = pCpyBakHandler->pSrcPageNumbers;
            CpyBakIntHandler.pDestPageNumbers = pCpyBakHandler->pDestPageNumbers;
            CpyBakIntHandler.pTrackingBuf = pCpyBakHandler->pTrackingBuf;
            CpyBakIntHandler.pNewDestPageNums = NULL;
            CpyBakIntHandler.IgnoreECCError = NV_FALSE;
            CpyBakIntHandler.State = FREE;
            break;
        case CopyBackMode_FLUSH_SRC_DEST_MERGE:
            RW_TRACE(("\r\n In CopyBackMode_FLUSH_SRC_DEST_MERGE"));
            CpyBakIntHandler.pSrcPageNumbers = pCpyBakHandler->pSrcPageNumbers;
            CpyBakIntHandler.pDestPageNumbers = pCpyBakHandler->pDestPageNumbers;
            CpyBakIntHandler.pTrackingBuf = pCpyBakHandler->pTrackingBuf;
            CpyBakIntHandler.pNewDestPageNums = pCpyBakHandler->ppNewDestPageNumbers;
            CpyBakIntHandler.IgnoreECCError = NV_FALSE;
            break;
        default:
            NvOsDebugPrintf("\n Not Expected to come here\n");
            break;
    }
    if (CpyBakmode == CopyBackMode_FLUSH_SRC_DEST_MERGE)
        MergeSrcNDest(hNand, &CpyBakIntHandler, IsWriteRequestRandom);
    else
        CopySrcToDest(hNand, &CpyBakIntHandler, IsWriteRequestRandom);
    if (pPageState)
        ReleaseVirtualMemory(pPageState, STRATEGY_ALLOC);
    return e;
}

