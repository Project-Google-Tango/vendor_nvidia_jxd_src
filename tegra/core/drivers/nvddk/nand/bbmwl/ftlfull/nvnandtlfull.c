/*
 * Copyright (c) 2007-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvnandtlfull.h"
#include "nand_strategy.h"
#include "nand_debug_utility.h"
#include "nand_sector_cache.h"


NvU32 g_NoSectorsTransferred = 0;

#define NO_FREE_BLOCK_ERROR 0
// Region number for kernle sunregion in WM partition
#define REGION_WM_KERNEL 1

// Use in-place write only for region 1(kernel subregion of WM partition)
static NvError
    NandTLWritePhysicalBlock(
    NvNandHandle hNand,
    NvS32* BlkNum,
    NvU8*pBuffer,
    NvS32 pageOffset,
    NvS32 NoOfSectorsToWrite);

NvBool NandTLInit(NvNandHandle hNand)
{
    hNand->MultiPageReadWriteOPeration = NV_TRUE;
    hNand->ValidateWriteOperation = NV_FALSE;
    hNand->NandTl.IsInitialized = NV_FALSE;
    hNand->NandTl.TotalPhysicalBlocks = 0;
    hNand->NandTl.SectorCache = 0;
    hNand->NandTl.InterleaveBanks = hNand->InterleaveCount;
    hNand->NandTl.NumberOfDevices = hNand->NandCfg.BlkCfg.NoOfBanks;
    // Read the status to determine if the device is available
    // and it's configuration
    if (NandTLGetDeviceInfo(hNand, 0, &(hNand->NandDevInfo)) == NvSuccess)
    {
        hNand->NandTl.TotalPhysicalBlocks =
            (hNand->NandDevInfo.NoOfPhysBlks) *
            hNand->NandTl.NumberOfDevices;
    }
    else
    {
        goto errorExit;
    }
    
    NV_ASSERT(hNand->gs_NandDeviceInfo.NumSpareAreaBytes);
    hNand->pSpareBuf = 
        NvOsAlloc(hNand->gs_NandDeviceInfo.NumSpareAreaBytes);
    if (!hNand->pSpareBuf)
    {
        hNand->NandTl.IsInitialized = NV_FALSE;
        goto errorExit;
    }

    if (!hNand->NandTl.SectorCache)
    {
        hNand->NandTl.SectorCache =
            AllocateVirtualMemory(sizeof(NandSectorCache), TL_ALLOC);
        NandSectorCacheInit(hNand);
    }
    hNand->TmpPgBuf =
        AllocateVirtualMemory(hNand->NandDevInfo.PageSize, TL_ALLOC);
    if (NandStrategyInitialize(hNand) != NvSuccess)
    {
        goto errorExit;
    }

    NandStrategyFlushTranslationTable(hNand);

    // For 1024 physical blocks, there will be 1000 logical blocks
    hNand->NandTl.TotalLogicalBlocks = hNand->ITat.Misc.TotalLogicalBlocks;
    LOG_NAND_DEBUG_INFO(("totalLogicalBlocks: %d\n",
        hNand->NandTl.TotalLogicalBlocks),
        HW_NAND_DEBUG_INFO);

    NvOsMemset(hNand->NandLockParams, 0xFF, sizeof(LockParams) * MAX_NAND_SUPPORTED);
    hNand->DdkFuncs.NvDdkGetLockedRegions(hNand->hNvDdkNand, hNand->NandLockParams);
    hNand->NandTl.IsInitialized = NV_TRUE;

    errorExit:
    return(hNand->NandTl.IsInitialized);
}

NvS32 NandTLGetSectorSize(NvNandHandle hNand)
{
    return(hNand->NandDevInfo.PageSize);
}

NvError
    NandTLRead(
    NvNandHandle hNand,
    NvS32 lun,
    NvU32 lba,
    NvU8 *pBuffer,
    NvS32 NumberOfSectors)
{
    NvS32 SectorOffset;
    NvS32 NumSectors2Rd;
    NvU8* pTagInfoBuffer;
    NvError Status = NvSuccess;
    NvError Status1 = NvSuccess;

    NvU32 LogicalBlockNumber;
    NvU32 StartingLBAPageOffset;
    NvU32 NumberOfPages;
    NvU32 NumOfSectrsTransferred = 0;

    lba <<= hNand->NandStrat.bsc4SctsPerPg;
    NumberOfSectors <<= hNand->NandStrat.bsc4SctsPerPg;
    // TODO:: check if this is correct

    if (!hNand->NandTl.IsInitialized)
    {
        NvOsDebugPrintf("\r\nNandTL_INVALID_ARGUMENT1 ");
        NV_ASSERT(!"\r\nNandTL_INVALID_ARGUMENT1 ");
        goto errorExit;
    }

    while (NumberOfSectors)
    {
        if (NumberOfSectors < 0)
        {
            NvOsDebugPrintf("\r\nNandTL_INVALID_ARGUMENT2 ");
            NV_ASSERT(!"\r\nNandTL_INVALID_ARGUMENT2 ");
            goto errorExit;
        }

        RW_TRACE(("\r\n Read: LSA: [%d 0x%08X] LPA: [%d 0x%08X] Scts: %d, Pgs: %d", lba, lba,
            lba >> hNand->NandStrat.bsc4SctsPerPg,
            lba >> hNand->NandStrat.bsc4SctsPerPg,
            NumberOfSectors, NumberOfSectors >> hNand->NandStrat.bsc4SctsPerPg));

        // Get the physical address
        Status1 = NandStrategyGetSectorPageToRead(
            hNand, lba, NumberOfSectors,
            hNand->NandTl.PageNumbersResLock, &SectorOffset,
            &NumSectors2Rd, &pTagInfoBuffer);
        if (Status1 != NvSuccess)
        {
            NvOsMemset(
                pBuffer, 0,
                (NumSectors2Rd *
                hNand->NandDevInfo.SectorSize));
            pBuffer += (NumSectors2Rd *
                hNand->NandDevInfo.SectorSize);
            NumberOfSectors -= NumSectors2Rd;
            lba += NumSectors2Rd;
            continue;
        }

        NumberOfPages = NumSectors2Rd >> 
                    hNand->NandStrat.bsc4SctsPerPg;
        RW_TRACE(("\r\n read_ddk StartBank(%d)NumPages(%d)",
                    (SectorOffset >> hNand->NandStrat.bsc4SctsPerPg),
                    NumberOfPages));
#if TRACE_READ_WRITE
    {   
        NvS32 bank = 0;
        for(bank = 0; bank<hNand->InterleaveCount; bank++)
        RW_TRACE(("\r\n In Read Block(%d)Offset(%d)",
        hNand->NandTl.PageNumbersResLock[bank]>> hNand->NandStrat.bsc4PgsPerBlk,
        hNand->NandTl.PageNumbersResLock[bank] % hNand->NandDevInfo.PgsPerBlk));
    }
#endif
        Status = NandTLReadPhysicalPage(
            hNand, hNand->NandTl.PageNumbersResLock, SectorOffset,
            (NvU8*)pBuffer, NumSectors2Rd,
            (NvU8*)pTagInfoBuffer, NV_FALSE);
        NumOfSectrsTransferred = g_NoSectorsTransferred;
        if (Status != NvSuccess)
        {
            NvOsDebugPrintf("\n TlRead failed Status:%d, ", Status);
            if ((Status == NvError_NandErrorThresholdReached) ||
                (Status == NvError_NandReadEccFailed))
            {
                LogicalBlockNumber = lba >> hNand->NandStrat.bsc4SecsPerLBlk;

                // StartingLBAPageOffset is the page offset of the requested 
                // sector. The value returned by NandStrategyGetSectorPageToRead
                // might not be the actual page offset where the request is given.
                // The status in tracking buffer must be checked from page where 
                // request is given.
                StartingLBAPageOffset = 
                    (lba % hNand->NandStrat.SecsPerLBlk) >>
                    (hNand->NandStrat.bsc4SctsPerPg);

                Status1 = StrategyHandleError (hNand, LogicalBlockNumber, 
                    StartingLBAPageOffset, 0, NumberOfPages, NV_FALSE);
                if (Status1 != NvSuccess)
                {
                    ERR_DEBUG_INFO(("\n\r Strategy Fail Status = 0x%08X", Status1));
                    NV_ASSERT(NV_FALSE);
                    return NvError_NandReadFailed;
                }

                pBuffer += (NumOfSectrsTransferred *
                    hNand->NandDevInfo.SectorSize);
                NumberOfSectors -= NumOfSectrsTransferred;
                lba += NumOfSectrsTransferred;
                Status = NvSuccess;

            }
            else
            {
                // on TIME_OUT_ERROR or NAND_ACCESS_FAILED error or
                // any unknown error, we return the error to top level.
                Status = NvError_NandTLFailed;
                // Assert here as it is not expected.
                NV_ASSERT(NV_FALSE);
                goto errorExit;
            }

        }
        else
        {
            pBuffer += (NumSectors2Rd * hNand->NandDevInfo.SectorSize);
            NumberOfSectors -= NumSectors2Rd;
            lba += NumSectors2Rd;
        }
    }

    errorExit:
    if (Status != NvSuccess)
    {
        NvOsDebugPrintf("\r\nTL read error=%u,sector start=0x%x,count=0x%x ",
            Status, lba, NumberOfSectors);
        DumpTT(hNand, -1);
    }
    return Status;
}

NvError
    NandTLWrite(
    NvNandHandle hNand,
    NvU32 lba,
    NvU8 *pBuffer,
    NvS32 NumberOfSectors)
{
    NvS32 SectorOffset;
    NvS32 NoOfSectorsToWrite;
    NvU8* pTagInfoBuffer;
    NvError Status = NvSuccess;
    NvU32 LogicalBlockNumber;
    NvU32 StartingLBAPageOffset;
    NvU32 NumberOfPages;
    NvError Status1 = NvSuccess;
    NvBool IsRegion1Block;
    NvU32 i;

    lba <<= hNand->NandStrat.bsc4SctsPerPg;
    NumberOfSectors <<= hNand->NandStrat.bsc4SctsPerPg;

    if (!hNand->NandTl.IsInitialized)
    {
        NvOsDebugPrintf("\r\nNandTL_INVALID_ARGUMENT3 ");
        NV_ASSERT(!"\r\nNandTL_INVALID_ARGUMENT3 ");
        goto errorExit;
    }

    while (NumberOfSectors)
    {
        if (NumberOfSectors < 0)
        {
            NvOsDebugPrintf("\r\nNandTL_INVALID_ARGUMENT4 ");
            NV_ASSERT(!"\r\nNandTL_INVALID_ARGUMENT4 ");
            goto errorExit;
        }

        RW_TRACE(("\r\n Write: LSA: [%d 0x%08X] LPA: [%d 0x%08X] Scts: %d, Pgs: %d", lba, lba,
            lba >> hNand->NandStrat.bsc4SctsPerPg,
            lba >> hNand->NandStrat.bsc4SctsPerPg,
            NumberOfSectors, NumberOfSectors >> hNand->NandStrat.bsc4SctsPerPg));

        // Get the physical address
        Status = NandStrategyGetSectorPageToWrite(
            hNand, lba, NumberOfSectors, hNand->NandTl.PageNumbersResLock,
            &SectorOffset, &NoOfSectorsToWrite, &pTagInfoBuffer);
        if ((Status != NvSuccess) && (Status != NvError_NandNoFreeBlock))
        {
            NV_ASSERT(NV_FALSE);
            Status = NvError_NandWriteFailed;
            goto errorExit;
        }

        IsRegion1Block = NV_FALSE;
        for (i = 0; i < (NvU32)hNand->InterleaveCount; i++)
            if (hNand->RegNum[i] != REGION_WM_KERNEL)
            {
                IsRegion1Block = NV_FALSE;
                break;
            }
            else
                IsRegion1Block = NV_TRUE;
        if ((Status == NvError_NandNoFreeBlock) && IsRegion1Block)
        {
            // If physical block is within Region 1 limits allow
            // write to same physical block
            Status = NandTLWritePhysicalBlock(
                hNand,
                hNand->NandTl.PageNumbersResLock,
                (NvU8*)pBuffer,
                lba & (hNand->NandDevInfo.SectorsPerBlock - 1),
                NoOfSectorsToWrite);
            if (Status != NvSuccess)
                goto errorExit;
        }
        else
        {
            if (Status == NvError_NandNoFreeBlock)
            {
                NvOsDebugPrintf("\r\nError: No free Blk, ");
                for (i = 0; i < (NvU32)hNand->InterleaveCount; i++)
                    NvOsDebugPrintf(" Region[%d]=%d ", i, hNand->RegNum[i]);
                DumpTT(hNand, -1);
            }
            NumberOfPages = NoOfSectorsToWrite >> hNand->NandStrat.bsc4SctsPerPg;
#if TRACE_READ_WRITE

            RW_TRACE(("\r\n write_ddk StartBank(%d)NumPages(%d)",
                    (SectorOffset >> hNand->NandStrat.bsc4SctsPerPg),
                    NumberOfPages));
            {
                NvS32 bank = 0;
                for(bank = 0; bank<hNand->InterleaveCount; bank++)
                RW_TRACE(("\r\n In Write Block(%d)Offset(%d)",
                hNand->NandTl.PageNumbersResLock[bank]>> hNand->NandStrat.bsc4PgsPerBlk,
                hNand->NandTl.PageNumbersResLock[bank] % hNand->NandDevInfo.PgsPerBlk));
            }
#endif
            Status = NandTLWritePhysicalPage(
                hNand,
                hNand->NandTl.PageNumbersResLock,
                SectorOffset,
                (NvU8*)pBuffer,
                NoOfSectorsToWrite,
                (NvU8*)pTagInfoBuffer);
        }
        if (Status != NvSuccess)
        {
            if (Status == NvError_NandWriteFailed)
            {
                // Logical Block number of the requested sector.
                LogicalBlockNumber = lba >> hNand->NandStrat.bsc4SecsPerLBlk;

                // StartingLBAPageOffset is the page offset of the requested 
                // sector. The value returned by NandStrategyGetSectorPageToWrite
                // might not be the actual page offset where the request is given.
                // The status in tracking buffer must be checked from page where 
                // request is given.
                StartingLBAPageOffset = 
                    (lba % hNand->NandStrat.SecsPerLBlk) >>
                    (hNand->NandStrat.bsc4SctsPerPg);

                Status1 = StrategyHandleError (hNand, LogicalBlockNumber, 
                    StartingLBAPageOffset, 0, NumberOfPages, NV_TRUE);

                if (Status1 != NvSuccess)
                {
                    NvOsDebugPrintf("\n Strategy Handle Error failed in Wr Status:%d, ", Status1);
                    NV_ASSERT(NV_FALSE);
                    return NvError_NandWriteFailed;
                }
                else
                {
                    Status = NvSuccess;
                }
            }
            else
            {
                // on TIME_OUT_ERROR or NAND_ACCESS_FAILED error or
                // any unknown error, we return the error to top level.
                Status = NvError_NandTLFailed;
                // Assert here as it is not expected.
                NV_ASSERT(NV_FALSE);
                goto errorExit;
            }
        }
        else
        {
            pBuffer += (NoOfSectorsToWrite * hNand->NandDevInfo.SectorSize);
            NumberOfSectors -= NoOfSectorsToWrite;
            lba += NoOfSectorsToWrite;
        }
    }

errorExit:
    if (Status != NvSuccess)
    {
        NvOsDebugPrintf("\r\nTL write error=%u,sector start=0x%x,count=0x%x ",
            Status, lba, NumberOfSectors);
        DumpTT(hNand, -1);
    }
    return(Status);
}

NvBool NandTLIsLbaLocked(NvNandHandle hNand, NvS32 reqPageNum)
{
    NvU32 logicalBlock2Write = reqPageNum >> hNand->NandStrat.bsc4PgsPerLBlk;
    NvU32 deviceNumber = reqPageNum & ((1 << hNand->NandStrat.bsc4NoOfILBanks) -1);
    NvU32 pageNum = 0;
    NvU32 nandDevice = 0;
    pageNum = reqPageNum & ((1 << hNand->NandStrat.bsc4PgsPerLBlk) -1);
    pageNum = pageNum >> hNand->NandStrat.bsc4NoOfILBanks;

    hNand->NandStrat.ttStatus = GetPBA(
        hNand,
        logicalBlock2Write,
        hNand->NandStrat.pPrevPhysBlks);
    pageNum = (hNand->NandStrat.pPrevPhysBlks[deviceNumber] << hNand->NandStrat.bsc4PgsPerLBlk) + pageNum;
    deviceNumber = hNand->NandCfg.DeviceLocations[deviceNumber];

    for (nandDevice = 0;nandDevice < MAX_NAND_SUPPORTED;nandDevice ++)
        if (hNand->NandLockParams[nandDevice].DeviceNumber == deviceNumber)
            if ((pageNum >= hNand->NandLockParams[nandDevice].StartPageNumber) &&
                (pageNum <= hNand->NandLockParams[nandDevice].EndPageNumber))
                return NV_TRUE;

    return NV_FALSE;
}

NvBool
    NandTLValidateParams(
    NvNandHandle hNand,
    NvS32* pPageNumbers,
    NvU32 SectorOffset,
    NvS32 SectorCount)
{
    NvS32 ValidColumnCount = 0;
    NvS32 NoOfPagesTowrite = 0;
    NvS32 InterleaveColumn = 0;
    NvS32 RequiredRows = 0;
    NvS32 s_SectorsPerRow = hNand->NandTl.InterleaveBanks <<
                            hNand->NandStrat.bsc4SctsPerPg;

    // Check basics !!!!!!!!!!!!
    // Check if the sector offset is at page boundary, if not return error.
    // The code considers all the calls are on page boundary.
    // Partial page write is anyhow not supported by NAND.
    if ((SectorOffset & (hNand->NandDevInfo.SctsPerPg - 1)) ||
        (SectorCount <= 0) || // check if the sector count is not <= 0
        ((NvS32)SectorOffset > (s_SectorsPerRow - 1)))
    {
        // Check if sector offset is greater than interleave column
        NvOsDebugPrintf("\r\nTLvalidate FAIL1 sector offset=0x%x,count=0x%x,"
            "sectorsPerRow=%u ", SectorOffset, SectorCount, s_SectorsPerRow);
        RETURN(NV_FALSE);
    }

    NoOfPagesTowrite =
        (SectorCount + (hNand->NandDevInfo.SctsPerPg - 1)) >> 
        hNand->NandStrat.bsc4SctsPerPg;

    // Determine the available valid columns
    for (InterleaveColumn = 0;
        InterleaveColumn < hNand->NandTl.InterleaveBanks;
        InterleaveColumn++)
    {
        // Check if the value in the column is within the limit of
        // number of pages in a block
        if (pPageNumbers[InterleaveColumn] >= 0)
        {
            ValidColumnCount++;
        }
    }

    if (!ValidColumnCount)
    {
        /* Check if there is no valid column(s) */
        NvOsDebugPrintf("\r\nTLvalidate FAIL2, Interleave bank Pgs[ ");
        for (InterleaveColumn = 0;
            InterleaveColumn < hNand->NandTl.InterleaveBanks;
            InterleaveColumn++)
        {
            NvOsDebugPrintf(" %d ", pPageNumbers[InterleaveColumn]);
        }
        NvOsDebugPrintf("]\n");
        RETURN(NV_FALSE);
    }

    // Check if the number pages to write is greater than available
    // interleave columns
    if (NoOfPagesTowrite >= hNand->NandTl.InterleaveBanks)
    {
        // We are here that means the pages to write requires all the
        // interleave columns. Therefore, valid column count should be
        // equal to interleave columns, if not return false
        if (ValidColumnCount != hNand->NandTl.InterleaveBanks)
        {
            // We are here that means the valid column count for the
            // write operation is  incorrect or not sufficient
            NvOsDebugPrintf("\r\nTLvalidate FAIL3 ");
            RETURN(NV_FALSE);
        }
    }
    else
    {
        // We are here that means the pages required is less that a
        // interleave columns. Check if the valid column is sufficient
        // to the number of sector count, if no return error
        if (ValidColumnCount < NoOfPagesTowrite)
        {
            NvOsDebugPrintf("\r\nTLvalidate FAIL4 ");
            RETURN(NV_FALSE);
        }
    }

    // We are here that means the valid columns available are okay
    // Therefore, lets check now if we are crossing the block boundary
    // requiredRows excluding the current row =
    // ((noOfPagesTowrite - pages in current row) +
    // (hNand->NandTl.InterleaveBanks - 1)) / hNand->NandTl.InterleaveBanks
    RequiredRows = ((NoOfPagesTowrite - (hNand->NandTl.InterleaveBanks -
        (SectorOffset >> hNand->NandStrat.bsc4SctsPerPg))) +
        (hNand->NandTl.InterleaveBanks - 1)) /
        hNand->NandTl.InterleaveBanks;
    if (((pPageNumbers[0] & (hNand->NandDevInfo.PgsPerBlk - 1)) + RequiredRows) >=
        hNand->NandDevInfo.PgsPerBlk)
    {
        NvOsDebugPrintf("\r\nTLvalidate FAIL5 page[0]=0x%x,Reqd rows=0x%x  ",
            pPageNumbers[0], RequiredRows);
        RETURN(NV_FALSE);
    }
    return(NV_TRUE);
}

// This function converts pagenumbers array from interleave column index to 
// physical device array
static NvError
    InterleaveToPhysicalPages(
    NvNandHandle hNand,
    NvU32 SectorOffset, NvU32 SectorCount,
    NvS32* pPageNumbers,
    NvU8* pStartingDeviceNo, NvU32** ppPageNums)
{
    static NvU32 s_PageNumbers[MAX_NAND_SUPPORTED];
    NvU8 PhysicalDeviceNumber = 0;
    NvU8 RowNumber = 0;
    NvU32 PagesPerDevice = (hNand->NandDevInfo.NoOfPhysBlks <<
        hNand->NandStrat.bsc4PgsPerBlk);
    NvU8 ColumnNumber = SectorOffset >> hNand->NandStrat.bsc4SctsPerPg;
    NvU8 i = 0;
    NvU8 DevicesPerColumn = 0;
    NvError TlStatus = NvSuccess;
    NvU32 Log2BlksPerChip;
    NvU32 PgCount;
    NvU32 TotalPage;
    TotalPage = SectorCount >> hNand->NandStrat.bsc4SctsPerPg;
    DevicesPerColumn = hNand->gs_NandDeviceInfo.NumberOfDevices >>
        hNand->NandStrat.bsc4NoOfILBanks;
    for (i = 0; i < MAX_NAND_SUPPORTED; i++)
        s_PageNumbers[i] = 0xFFFFFFFF;
    // This assert will hit when either of the pPageNumbers or SectorOffset 
    // contains wrong values.    
    if (pPageNumbers[ColumnNumber] == 0xFFFFFFFF)
    {
        NvOsDebugPrintf("\n Interleave2PhysicalPg fail1: illegal page ");
        NV_ASSERT(!"\n Interleave2PhysicalPg fail1: illegal page ");
        goto LblFail;
    }
    Log2BlksPerChip = NandUtilGetLog2(hNand->NandDevInfo.NoOfPhysBlks);
    PgCount = 0;
    for (i = ColumnNumber; i < (hNand->NandTl.InterleaveBanks +
        ColumnNumber); i++)
    {
        NvU32 Index = MACRO_MOD_LOG2NUM(i, hNand->NandStrat.bsc4NoOfILBanks);
        if (pPageNumbers[Index] == 0xFFFFFFFF)
            continue;
        // To calculate the physical Device number
        RowNumber = pPageNumbers[Index] >> (hNand->NandStrat.bsc4PgsPerBlk +
            Log2BlksPerChip);

        PhysicalDeviceNumber = (Index * DevicesPerColumn) + RowNumber;
        if (!(PhysicalDeviceNumber < hNand->NandDevInfo.NumberOfDevices))
        {
            NvOsDebugPrintf("\nInterleave2PhysPg fail2: illegal device ");
            NV_ASSERT(!"\nInterleave2PhysPg fail2: illegal device ");
            goto LblFail;
        }
        if (Index == ColumnNumber)
            *pStartingDeviceNo = PhysicalDeviceNumber;
        // To fill the physical pagenumbers as per physical device numbers which is 
        // required by ddk
        s_PageNumbers[PhysicalDeviceNumber] = pPageNumbers[Index] &
            (PagesPerDevice - 1);
        PgCount++;
        if (PgCount == TotalPage)
            break;
    }
    // return physical pages found
    *ppPageNums = s_PageNumbers;
    return NvSuccess;

LblFail:
    *ppPageNums = NULL;
    return TlStatus;
}


NvU32 FTL_Time =0;
#define DIFF(T1, T2) \
    (((T1) > (T2)) ? ((T1) - (T2)) : ((T2) - (T1)))

NvError
    NandTLReadPhysicalPage(
    NvNandHandle hNand,
    NvS32* pPageNumbers,
    NvU32 SectorOffset,
    NvU8 *pData,
    NvU32 SectorCount,
    NvU8* pTagBuffer,
    NvBool IgnoreEccError)
{
    NvError ErrStatus;
    NvU32 NoOfIterations;
    NvU32 NoOfPages;
    NvU32 NoOfFullPgRead;
    NvError status = NvSuccess;
    NvU32* pPageNums = NULL;
    NvU8 DeviceNum = 0;
    NvU32 i = 0;

    if ((SectorCount == 0) && (pTagBuffer))
    {
        SectorCount = hNand->NandDevInfo.SctsPerPg;
    }
    NoOfFullPgRead = SectorCount >> hNand->NandStrat.bsc4SctsPerPg;

    if (hNand->MultiPageReadWriteOPeration)
    {
        NoOfIterations = 1;
        NoOfPages = NoOfFullPgRead;
    }
    else
    {
        NoOfIterations = NoOfFullPgRead;
        NoOfPages = 1;
    }

    if (!NandTLValidateParams(hNand, pPageNumbers, SectorOffset, SectorCount))
    {
        status = NvError_NandTLFailed;
        goto errorExit;
    }

    for (i = 0; i < NoOfIterations; i++)
    {
        status = InterleaveToPhysicalPages(hNand, SectorOffset, SectorCount, pPageNumbers, &DeviceNum, &pPageNums);
        if (status != NvSuccess)
            goto errorExit;
        if (pData)
        {
            ErrStatus = hNand->DdkFuncs.NvDdkDeviceRead(hNand->hNvDdkNand, DeviceNum, pPageNums, pData,
                pTagBuffer, &NoOfPages, IgnoreEccError);
        }
        else
        {
            ErrStatus = hNand->DdkFuncs.NvDdkReadSpare(hNand->hNvDdkNand,
                DeviceNum, pPageNums, pTagBuffer, 0,
                hNand->gs_NandDeviceInfo.NumSpareAreaBytes);
        }
        if (ErrStatus != NvSuccess)
        {
            ERR_DEBUG_INFO(("\nDdk Read error code=0x%x", ErrStatus));
            LOG_NAND_DEBUG_INFO(("Rd, Num pages = %d", NoOfPages), HW_NAND_DEBUG_INFO);
            if (ErrStatus == NvError_NandErrorThresholdReached)
            {
                status = NvError_NandErrorThresholdReached;
            }
            else if ((ErrStatus == NvError_NandCommandQueueError) ||
                (ErrStatus == NvError_NandReadEccFailed))
            {
                status = NvError_NandReadEccFailed;
            }
            else
            {
                NvOsDebugPrintf("\nDdk Read error code=0x%x ", ErrStatus);
                status = NvError_NandReadFailed;
            }
        }
        g_NoSectorsTransferred = NoOfPages << hNand->NandStrat.bsc4SctsPerPg;
    }
    errorExit:
    return(status);
}

NvError
    NandTLWritePhysicalPage(
    NvNandHandle hNand,
    NvS32* pPageNumbers,
    NvU32 SectorOffset,
    NvU8* pData,
    NvU32 SectorCount,
    NvU8* pTagBuffer)
{
    NvU32 NoOfFullPgWrite;
    NvError ErrStatus;
    NvU32 NoOfIterations;
    NvU32 NoOfPages;
    NvU32 i;
    static NvU8* s_pTestBuffer = 0;
    static NvU8* s_pTagTestBuffer;
    NvU32 BufferSize;
    NvU64 TotalPageSize = hNand->gs_NandDeviceInfo.PageSize *
                                        hNand->gs_NandDeviceInfo.PagesPerBlock;
    NvU64 BuffSize;
    NvU32* pPageNums = NULL;
    NvU8 DeviceNum = 0;
    NvError TlStatus;

//    ERR_DEBUG_INFO(("\r\n In TL Phys Wr pg"));

    if (hNand->ValidateWriteOperation)
    {
        static NvU8 s_TagTestBuffer[64];
        if (pTagBuffer)
            s_pTagTestBuffer = &s_TagTestBuffer[0];
        else
            s_pTagTestBuffer = NULL;

        // this is a workaround to eliminate compiler warning for overflow, underflow
        BuffSize = ((NvU32)TotalPageSize) * hNand->NandTl.InterleaveBanks;
        BufferSize = (NvU32) BuffSize;

        if (!s_pTestBuffer)
            s_pTestBuffer = AllocateVirtualMemory(BufferSize, TL_ALLOC);

        // read the area first and check whether it has all 0xFF's.
        if (pData)
            NandTLReadPhysicalPage(hNand, pPageNumbers, SectorOffset,
                s_pTestBuffer, SectorCount,
                s_pTagTestBuffer, NV_FALSE);
        else
            NandTLReadPhysicalPage(
                hNand,
                pPageNumbers,
                SectorOffset,
                0,
                SectorCount,
                s_pTagTestBuffer,
                NV_FALSE);

        // check the area.
        if (pData)
        {
            for (i = 0; i < (SectorCount*512); i++)
            {
                if (s_pTestBuffer[i] != 0xFF)
                {
                    LOG_NAND_DEBUG_INFO(("***data is not 0xFF, pn = %d, byte = %d",
                        pPageNumbers[0], i),
                        HW_NAND_DEBUG_INFO);
                    TlStatus = NvError_NandEraseFailed;
                    return TlStatus;
                }
            }
        }

        if (pTagBuffer)
        {
            for (i = 0; i < (12); i++)
            {
                if (s_pTagTestBuffer[i] != 0xFF)
                {
                    LOG_NAND_DEBUG_INFO(("***tag is not 0xFF, pn = %d, byte = %d",
                        pPageNumbers[0], i),
                        HW_NAND_DEBUG_INFO);
                    TlStatus = NvError_NandEraseFailed;
                    return TlStatus;
                }
            }
        }
    }

    if ((SectorCount == 0) && (pTagBuffer))
    {
        SectorCount = hNand->NandDevInfo.SctsPerPg;
    }

    if (!NandTLValidateParams(hNand, pPageNumbers, SectorOffset, SectorCount))
    {
        RETURN(NvError_NandTLFailed);
    }

    NoOfFullPgWrite = SectorCount >> hNand->NandStrat.bsc4SctsPerPg;
    if (hNand->MultiPageReadWriteOPeration)
    {
        NoOfIterations = 1;
        NoOfPages = NoOfFullPgWrite;
    }
    else
    {
        NoOfIterations = NoOfFullPgWrite;
        NoOfPages = 1;
    }

    for (i = 0; i < NoOfIterations; i++)
    {
        TlStatus = InterleaveToPhysicalPages(hNand, SectorOffset, SectorCount, pPageNumbers, &DeviceNum, &pPageNums);
        if (TlStatus != NvSuccess)
            return TlStatus;
        if (pTagBuffer)
        {
            if (pData)
            {
                // Data + Tag update
                ErrStatus = hNand->DdkFuncs.NvDdkDeviceWrite(hNand->hNvDdkNand,
                    DeviceNum, pPageNums, pData, pTagBuffer, &NoOfPages);
            }
            else
            {
                // write all zero next to factory bad byte
                NvOsMemset(hNand->pSpareBuf, 0x0, 
                    hNand->gs_NandDeviceInfo.NumSpareAreaBytes);
                // Run-time bad mark - entire tag area from factory bad made 0
                ErrStatus = hNand->DdkFuncs.NvDdkWriteSpare(hNand->hNvDdkNand,
                    DeviceNum, pPageNums, hNand->pSpareBuf, 1,
                    (hNand->gs_NandDeviceInfo.NumSpareAreaBytes-1));
            }
        }
        else
        {
            // Only data write
            ErrStatus = hNand->DdkFuncs.NvDdkDeviceWrite(hNand->hNvDdkNand,
                DeviceNum, pPageNums, pData, NULL, &NoOfPages);
        }
        g_NoSectorsTransferred = NoOfPages << hNand->NandStrat.bsc4SctsPerPg;
        if (ErrStatus != NvSuccess)
        {
            LOG_NAND_DEBUG_INFO(("Wr, Num pages = %d", NoOfPages), HW_NAND_DEBUG_INFO);
            return(NvError_NandWriteFailed);
        }
    }

    if (hNand->ValidateWriteOperation)
    {
        // read the area first and check whether it has all 0xFF's.
        if (pData)
            NandTLReadPhysicalPage(hNand, pPageNumbers, SectorOffset,
                s_pTestBuffer, SectorCount,
                s_pTagTestBuffer, NV_FALSE);
        else
            NandTLReadPhysicalPage(
                hNand, pPageNumbers, SectorOffset, 0, SectorCount,
                s_pTagTestBuffer, NV_FALSE);

        // check the area.
        if (pData)
        {
            // read(pPageNumbers, sectorOffset, (NvU8*)TestBufferPhysAddr,
            // sectorCount, pTagTestBuffer);
            for (i = 0; i < (SectorCount*512); i++)
            {
                if (s_pTestBuffer[i] != pData[i])
                {
                    LOG_NAND_DEBUG_INFO(("***data mismatch, pn = %d, byte = %d",
                        pPageNumbers[0], i), HW_NAND_DEBUG_INFO);
                    TlStatus = NvError_NandWriteFailed;
                    return TlStatus;
                }
            }
        }

        if (pTagBuffer)
        {
            // read(pPageNumbers, sectorOffset, 0, sectorCount, pTagTestBuffer);
            for (i = 0; i < (12); i++)
            {
                if (s_pTagTestBuffer[i] != pTagBuffer[i])
                {
                    LOG_NAND_DEBUG_INFO(("***tag mismatch, pn = %d, byte = %d",
                        pPageNumbers[0], i), HW_NAND_DEBUG_INFO);
                    TlStatus = NvError_NandWriteFailed;
                    return TlStatus;
                }
            }
        }
    }
//    ERR_DEBUG_INFO(("\r\n Out TL Phys Wr pg success"));

    return(NvSuccess);
}

NvError NandTLGetBlockInfo(NvNandHandle hNand, BlockInfo* block,
    NandBlockInfo * pDdkBlockInfo, NvBool SkippedBytesReadEnable)
{
    NvU8 ChipNum = 0;
    NvU32 BlockNum;
    NvS32 pPageNumbers[4] = {-1, -1, -1, -1};
    NvU32 *ppPageNums = NULL;
    NvError ErrVal = NvSuccess;
    NvError TlStatus;

    pPageNumbers[block->BnkNum] = block->BlkNum << hNand->NandStrat.bsc4PgsPerBlk;
    TlStatus = InterleaveToPhysicalPages(
        hNand,
        block->BnkNum << hNand->NandStrat.bsc4SctsPerPg,
        1 << hNand->NandStrat.bsc4SctsPerPg,
        pPageNumbers,
        &ChipNum,
        &ppPageNums);
    if (TlStatus != NvSuccess)
        return TlStatus;

    BlockNum = block->BlkNum % hNand->NandDevInfo.NoOfPhysBlks;
//    NvOsDebugPrintf("\r\n Block = %d Device = %d", BlockNum, ChipNum);
    ErrVal = NvDdkNandGetBlockInfo(
        hNand->hNvDdkNand, ChipNum, BlockNum, pDdkBlockInfo,
        SkippedBytesReadEnable);

    if (ErrVal != NvSuccess)
    {
        NvOsDebugPrintf("\r\n In NandTLGetBlockInfo Error = 0x%x", ErrVal);
        return ErrVal;
    }
    else
        return NvSuccess;
}

NvError NandTLErase(NvNandHandle hNand, NvS32* pPageNumbers)
{
    NvError ErrStatus;
    NvU32* pPageNums = NULL;
    NvU8 DeviceNum = 0;
    NvU32 NumOfBlocks = 0;
    NvU32 NumOfBlocksErased = 0;
    NvS32 i = 0;
    NvU32 ColumnNumber = 0xFFFFFFFF;
    NvError TlStatus;
    NvU32 SectorCount;
    for (i = 0; i < hNand->NandTl.InterleaveBanks; i++)
    {
        if (pPageNumbers[i] != 0xFFFFFFFF)
        {
            RW_TRACE(("In TL ERASE blk = %d\n", pPageNumbers[i] / hNand->NandDevInfo.PgsPerBlk));
            NumOfBlocks++;
            if (ColumnNumber == 0xFFFFFFFF)
                ColumnNumber = i;
        }
    }

    NumOfBlocksErased = NumOfBlocks;
    SectorCount = NumOfBlocks << (hNand->NandStrat.bsc4PgsPerBlk +
        hNand->NandStrat.bsc4SctsPerPg);
    TlStatus = InterleaveToPhysicalPages(hNand,
        ColumnNumber << hNand->NandStrat.bsc4SctsPerPg, SectorCount,
        pPageNumbers, &DeviceNum, &pPageNums);
    if (TlStatus != NvSuccess)
        return TlStatus;
    ErrStatus = hNand->DdkFuncs.NvDdkDeviceErase(hNand->hNvDdkNand, DeviceNum, pPageNums,
                                                &NumOfBlocksErased);

    if (NumOfBlocksErased != NumOfBlocks)
    {
        for (i = NumOfBlocksErased; i < hNand->NandTl.InterleaveBanks; i++)
            pPageNumbers[i] = 0xFFFFFFFF;
        return(NvError_NandEraseFailed);
    }
    
    if (NvSuccess != ErrStatus)
    {
        LOG_NAND_DEBUG_INFO(("NvDdkNandErase() returned %d\n", ErrStatus),
            HW_NAND_DEBUG_INFO);
        return(NvError_NandEraseFailed);
    }
    return(NvSuccess);
}

NvError
    NandTLCopyBack(
    NvNandHandle hNand,
    NvS32* pSourcePageNumbers,
    NvS32* pDstnPageNumbers,
    NvU32 SrcSectorOffset,
    NvU32 DstSectorOffset,
    NvS32* SectorCount,
    NvBool IgnoreEccError)
{
    NvError ErrStatus = NvSuccess;
    NvU32 NoOfPages = (*SectorCount) >> hNand->NandStrat.bsc4SctsPerPg; // / hNand->NandDevInfo.SctsPerPg;
    NvU8 DeviceNum1 = 0, DeviceNum2 = 0;
    NvU32 SrcPgNums[MAX_NAND_SUPPORTED];
    NvU32 DstPgNums[MAX_NAND_SUPPORTED];
    NvU32* pPageNums = NULL;
    NvError TlStatus = NvSuccess;

    ERR_DEBUG_INFO(("\nEntering Tl Cpybk "));

    TlStatus = InterleaveToPhysicalPages(hNand, SrcSectorOffset,
        (*SectorCount), pSourcePageNumbers, &DeviceNum1, &pPageNums);
    if (TlStatus != NvSuccess)
    {
        goto fail;
    }
    NvOsMemcpy(SrcPgNums, pPageNums, MAX_NAND_SUPPORTED * sizeof(NvS32));
    TlStatus = InterleaveToPhysicalPages(hNand, DstSectorOffset,
        (*SectorCount), pDstnPageNumbers, &DeviceNum2, &pPageNums);
    if (TlStatus != NvSuccess)
    {
        goto fail;
    }
    NvOsMemcpy(DstPgNums, pPageNums, MAX_NAND_SUPPORTED * sizeof(NvS32));
    ErrStatus = NvDdkNandCopybackPages(hNand->hNvDdkNand, DeviceNum1,
                    DeviceNum2, SrcPgNums, DstPgNums, &NoOfPages,
                    IgnoreEccError);
    *SectorCount = NoOfPages * hNand->NandDevInfo.SctsPerPg;
    if (ErrStatus != NvSuccess)
    {
//        NV_ASSERT(NV_FALSE);
        ERR_DEBUG_INFO(("\nCpybk failed after %d page transfers, "
            "Ddk copyback error code=0x%x ", NoOfPages, ErrStatus));
        if (ErrStatus == NvError_NandReadEccFailed)
        {
                TlStatus = NvError_NandReadFailed;
        }
        else if (ErrStatus == NvError_NandErrorThresholdReached)
        {
                TlStatus = NvError_NandErrorThresholdReached;
        }
        else if (ErrStatus == NvError_NandWriteFailed)
            TlStatus = NvError_NandWriteFailed;
        else
            TlStatus = NvError_NandCopyBackFailed;
    }
fail:
    ERR_DEBUG_INFO(("\nExiting Tl Cpybk "));
    return TlStatus;
}

NvError
    NandTLGetDeviceInfo(
    NvNandHandle hNand,
    NvS32 DeviceNumber,
    NandDeviceInfo* pDeviceInfo)
{
    NvError Error;

    Error = hNand->DdkFuncs.NvDdkGetDeviceInfo(hNand->hNvDdkNand, DeviceNumber, &hNand->gs_NandDeviceInfo);
    CheckNvError(Error, Error);
    pDeviceInfo->MakerCode = hNand->gs_NandDeviceInfo.VendorId;
    pDeviceInfo->DeviceCode = hNand->gs_NandDeviceInfo.DeviceId;
    pDeviceInfo->DeviceType = (NandFlashType)hNand->gs_NandDeviceInfo.NandType;
    pDeviceInfo->DeviceCapacity = hNand->gs_NandDeviceInfo.DeviceCapacityInKBytes;
    pDeviceInfo->NoOfPhysBlks = hNand->gs_NandDeviceInfo.NoOfBlocks;
    pDeviceInfo->NoOfZones = hNand->gs_NandDeviceInfo.ZonesPerDevice;
    pDeviceInfo->PageSize = (short)hNand->gs_NandDeviceInfo.PageSize;
    pDeviceInfo->BlkSize =
        hNand->gs_NandDeviceInfo.PageSize * hNand->gs_NandDeviceInfo.PagesPerBlock;
    pDeviceInfo->PgsPerBlk = hNand->gs_NandDeviceInfo.PagesPerBlock;
    pDeviceInfo->SectorSize = 512;
    pDeviceInfo->SctsPerPg = pDeviceInfo->PageSize/pDeviceInfo->SectorSize;
    pDeviceInfo->SectorsPerBlock = pDeviceInfo->BlkSize/pDeviceInfo->SectorSize;
    pDeviceInfo->RendAreaSizePerSector = 16;
    pDeviceInfo->BusWidth = 8;
    pDeviceInfo->SerialAccessMin = 50;
    pDeviceInfo->NumberOfDevices = hNand->gs_NandDeviceInfo.NumberOfDevices;
    pDeviceInfo->InterleaveCapabililty = hNand->gs_NandDeviceInfo.InterleaveCapability;

    return NvSuccess;

}

NvError NandTLGetBlockStatus(NvNandHandle hNand, NvS32* pPageNumbers)
{
    NvError sts;
    NvU32 i;

    NvOsMemset(hNand->pSpareBuf, 0x0,
        hNand->gs_NandDeviceInfo.NumSpareAreaBytes);
    sts = NandTLReadPhysicalPage(
        hNand, pPageNumbers,
        0,
        NULL,
        hNand->NandDevInfo.SctsPerPg,
        hNand->pSpareBuf,
        NV_FALSE);

    CheckNvError(sts, NvSuccess);
    for (i = 0; i < 16; i++)
    {
        if (hNand->pSpareBuf[1] != RUN_TIME_GOOD_MARK/* run-time bad check */)
            return(NvError_NandTLFailed);
    }
    return(sts);
}


NvError NandTLFlushTranslationTable(NvNandHandle hNand)
{
    return(NandStrategyFlushTranslationTable(hNand));
}

NvError NandTLWritePhysicalBlock(
    NvNandHandle hNand,
    NvS32* pPageNum,
    NvU8*pBuffer,
    NvS32 sectorOffset,
    NvS32 NoOfSectorsToWrite)
{
    NvError status = NvSuccess;
    NvU8* blockData = NULL;
    TagInformation taginfo;
    NvU32 i;
    
    if (pPageNum[0] == -1)
    {
        NvOsDebugPrintf("\r\nNandTL_INVALID_ARGUMENT5 ");
        NV_ASSERT(!"\r\nNandTL_INVALID_ARGUMENT5 ");
        return NvSuccess;
    }
    if ((sectorOffset + NoOfSectorsToWrite) >
        hNand->NandDevInfo.SectorsPerBlock)
    {
        NvOsDebugPrintf("\r\nNandTL_INVALID_ARGUMENT6 ");
        NV_ASSERT(!"\r\nNandTL_INVALID_ARGUMENT6 ");
        return NvSuccess;
    }
    
    blockData =
        AllocateVirtualMemory(hNand->NandDevInfo.BlkSize, TL_ALLOC);

    //read taginfo
    status = NandTLReadPhysicalPage(
        hNand,
        pPageNum,
        0,
        NULL,
        hNand->NandDevInfo.SctsPerPg,
        (NvU8 *)&taginfo,
        NV_FALSE);

    // read the complete block into memory
    status = NandTLReadPhysicalPage(
        hNand,
        pPageNum,
        0,
        blockData,
        hNand->NandDevInfo.SectorsPerBlock * hNand->InterleaveCount,
        NULL,
        NV_FALSE);
    ExitOnTLErr(status);
    // erase the block
    status = NandTLErase(hNand, pPageNum);
    ExitOnTLErr(status);
    
    // replace the old data in the memory with new data
    NvOsMemcpy(
        &blockData[sectorOffset * hNand->NandDevInfo.SectorSize],
        pBuffer,
        NoOfSectorsToWrite * hNand->NandDevInfo.SectorSize);

    // write modified data into the erased block
    status = NandTLWritePhysicalPage(
        hNand,
        pPageNum,
        0,
        blockData,
        hNand->NandDevInfo.SctsPerPg,
        (NvU8 *)&taginfo);
    ExitOnTLErr(status);
    // Write from Page 1 onwards
    for (i = 0; i < MAX_ALLOWED_INTERLEAVE_BANKS; i++)
        if (pPageNum[i] != -1)
            pPageNum[i]++;
    status = NandTLWritePhysicalPage(
        hNand,
        pPageNum,
        ((hNand->InterleaveCount > 1)? hNand->NandDevInfo.SctsPerPg : 0),
        &blockData[hNand->NandDevInfo.PageSize],
        ((hNand->NandDevInfo.SectorsPerBlock * hNand->InterleaveCount) -
        hNand->NandDevInfo.SctsPerPg),
        NULL);
    ExitOnTLErr(status);


    errorExit:
    ReleaseVirtualMemory(blockData,TL_ALLOC);
    return(status);
}

NvError 
NandTLSetRegionInfo(
    NvNandHandle hNand,
    NvDdkBlockDevIoctl_DefineSubRegionInputArgs* pSubRegionInfo)
{
    NvError e = NvError_Success;
    NvU32 BlockNum = pSubRegionInfo->StartLogicalBlock;
    NvS32 PhysicalBlocks[MAX_NAND_SUPPORTED];
    hNand->RegionCount++;
    if(hNand->RegionCount > MAX_NAND_SUBREGIONS)
        return NvError_BadParameter;
    
    for(;BlockNum <= pSubRegionInfo->StopLogicalBlock;BlockNum++)
    {
        NvOsMemset(PhysicalBlocks,0xFF,sizeof(NvU32) * MAX_NAND_SUPPORTED);
        if(GetPBA(hNand,BlockNum,PhysicalBlocks) != NvSuccess)
        {
            e = NvError_NandOperationFailed;
            goto exit;
        }
        if(SetPbaRegionInfo(hNand,PhysicalBlocks,hNand->RegionCount,NV_FALSE) != NvSuccess)
        {
            e = NvError_NandOperationFailed;
            goto exit;
        }
    }
    // go to exit if there is no compaction for this region
    if(!pSubRegionInfo->FreeBlockCount)
        goto exit;

    for(BlockNum = 0; BlockNum < pSubRegionInfo->FreeBlockCount;BlockNum++)
    {
        NvOsMemset(PhysicalBlocks,0xFF,sizeof(NvU32) * MAX_NAND_SUPPORTED);
        if(GetPBA(hNand,BlockNum + pSubRegionInfo->StartFreeLogicalBlock,PhysicalBlocks) 
            != NvSuccess)
        {
            e = NvError_NandOperationFailed;
            goto exit;
        }
        if(SetPbaRegionInfo(hNand,PhysicalBlocks,hNand->RegionCount,NV_TRUE) != NvSuccess)
        {
            e = NvError_NandOperationFailed;
            goto exit;
        }
    }
    exit:
        //DumpTT(hNand, -1);
    return e;
}

NvBool 
NandTLEraseLogicalBlocks(
    NvNandHandle hNand,
    NvU32 StarBlock,
    NvU32 EndBlock)
{
    NvU32 LbaCount;
    NvS32 DevCount;
    NvS32 PhysicalBlocks[MAX_NAND_SUPPORTED];
    if(StarBlock > hNand->ITat.Misc.TotalLogicalBlocks)
        return NV_FALSE;
    if(EndBlock == 0xFFFFFFFF)
        EndBlock = hNand->FtlEndLba;
    if(EndBlock > hNand->ITat.Misc.TotalLogicalBlocks)
        return NV_FALSE;
    
    for(LbaCount = StarBlock; LbaCount < EndBlock; LbaCount++)
    {
        NvOsMemset(PhysicalBlocks,0xFF,sizeof(NvU32) * MAX_NAND_SUPPORTED);
        if(GetPBA(hNand,LbaCount,PhysicalBlocks) != NvSuccess)
            return NV_FALSE;

        for(DevCount = 0; DevCount < hNand->pNandTt->NoOfILBanks; DevCount++)
        {
            if(PhysicalBlocks[DevCount] != 0xFFFFFFFF)
                if(SetPbaUnused(hNand,DevCount,PhysicalBlocks[DevCount]) != NvSuccess)
                    return NV_FALSE;
        }
        NvOsMemset(PhysicalBlocks,0xFF,sizeof(NvU32) * MAX_NAND_SUPPORTED);
        if(AssignPba2Lba(hNand,PhysicalBlocks,LbaCount,NV_TRUE) != NvSuccess)
            return NV_FALSE;
    }
    return NV_TRUE;
}

NvError NandTLEraseAllBlocks(NvNandHandle hNand)
{
    NvS32 BlockNum = 0;
    NvS32 DevCount = 0;
    NvU32 PageNumber[MAX_NAND_SUPPORTED];
    NvError e;
    for (DevCount = 0;DevCount < hNand->NandTl.InterleaveBanks; DevCount++)
    {
        for (BlockNum = hNand->FtlStartPba;BlockNum < hNand->FtlEndPba; BlockNum++)
        {
            NvU32 DeviceNum = DevCount;
            NvU32 BlkNum;
            NandBlockInfo BlockInfo;
            // Get chip number and block number within chip
            BlkNum = BlockNum;
            DeviceNum = DevCount;
            if (NvBtlGetPba(&DeviceNum, &BlkNum) == NV_FALSE)
            {
                NvOsDebugPrintf("\nTLEraseAll fail BtlGetPba: Chip=%d,Block=%d ", 
                    DevCount, BlockNum);
                continue;
            }
            NvOsMemset(PageNumber, 0xFF, (sizeof(NvU32) * MAX_NAND_SUPPORTED));
            PageNumber[DeviceNum] = BlkNum << hNand->NandStrat.bsc4PgsPerBlk;
            BlockInfo.pTagBuffer = (NvU8*)hNand->pSpareBuf;
            // Reset sparebuf contents before call to GetBlockInfo
            NvOsMemset(BlockInfo.pTagBuffer, 0x0,
                hNand->gs_NandDeviceInfo.NumSpareAreaBytes);
            //get block info
            if (hNand->DdkFuncs.NvDdkGetBlockInfo(hNand->hNvDdkNand, DeviceNum,
                BlkNum, &BlockInfo, NV_TRUE) != NvSuccess)
            {
                NvOsDebugPrintf("\nGetBlock info failed: Chip=%d, Blk=%d ",
                    DeviceNum, BlkNum);
                continue;
                //return NvError_NandReadFailed;
            }
            
            //erase the block if its good
            if((BlockInfo.IsFactoryGoodBlock) && // factory good
                (hNand->pSpareBuf[1] == RUN_TIME_GOOD_MARK)) // run-time bad
            {
                NvU32 NumberOfBlocks = 1;

                NvOsMemset(PageNumber, 0xFF, sizeof(PageNumber));
                PageNumber[DeviceNum] = BlkNum <<
                    hNand->NandStrat.bsc4PgsPerBlk;
                if (NvDdkNandErase(
                    hNand->hNvDdkNand,
                    DeviceNum,
                    PageNumber,
                    &NumberOfBlocks) != NvError_Success)
                {
                    NvOsMemset(hNand->pSpareBuf, 0x0,
                        hNand->gs_NandDeviceInfo.NumSpareAreaBytes);
                    //mark the block as bad, if erase fails
                    e = NvDdkNandWriteSpare(hNand->hNvDdkNand, DeviceNum, 
                        PageNumber, hNand->pSpareBuf, 1,
                        (hNand->gs_NandDeviceInfo.NumSpareAreaBytes - 1));
                    if (e != NvSuccess)
                        NvOsDebugPrintf("\nMarking Bad block failed for" 
                        "Chip=%d Block=%d ", DeviceNum, BlkNum);
                }
            }
            else
            {
               NvOsDebugPrintf("\nFound Bad block Chip=%d Block=%d ", 
                                DeviceNum, BlkNum);
               NvOsDebugPrintf("\nFactory Bad: 0x%x, Run-time bad marker: 0x%x ",
                   hNand->pSpareBuf[0],hNand->pSpareBuf[1]);
            }
        }
    }
    
    return NvSuccess;
}

void *
    AllocateVirtualMemory(NvU32 SizeInBytes, NvU32 keyword)
{
    NvU8 *BufferAddr;
    BufferAddr = (NvU8 *)NvOsAlloc(SizeInBytes);
    if (!(BufferAddr))
    {
        NvOsDebugPrintf("\nAlloc memory failed ");
    }
    return(BufferAddr);
}

void
    ReleaseVirtualMemory(void* pVirtualAddr, NvU32 keyword)
{
    if (pVirtualAddr)
        NvOsFree(pVirtualAddr);
}

// prints the debug string to an output terminal
void DbgOutput(NvNandHandle hNand, NvS8* str)
{
    LOG_NAND_DEBUG_INFO((str), HW_NAND_DEBUG_INFO);
}
void NandGetInfo(NvNandHandle hNand, NvU32 DeviceNo, NandDeviceInfo* pDevInfo)
{
    NvError Error;
    Error = NandTLGetDeviceInfo(hNand, DeviceNo, pDevInfo);
    if (Error != NvSuccess)
        RETURN_VOID;
}

// prints the debug string to an output terminal
void NandDbgOutput(NvS8* str)
{
    LOG_NAND_DEBUG_INFO((str), HW_NAND_DEBUG_INFO);
}

// Callback for every device/bank present on the board
void NandOnBankIdentify(NvS32 bankNum, NvS32 location)
{
}



