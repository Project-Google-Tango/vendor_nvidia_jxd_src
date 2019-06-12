/**
* @file
* @brief   Hardware NAND device driver strategy implementation for Interleave
*          pattern.
*
*
* @b Description:  This file outlines the implementation of strategy required
*                  for interleave pattern on multi-bank NAND FLASH.
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

#ifndef INCLUDED_NAND_STRATEGY_INTERLEAVE_H
#define INCLUDED_NAND_STRATEGY_INTERLEAVE_H

#include "nand_ttable.h"
#include "nvrm_memmgr.h"
#include "nvassert.h"
#include "nvnandftlfull.h"

#if defined(__cplusplus)
extern "C"
{
#endif

    /**
    * @class   NandStrategyInterleave
    * @brief   The NandStrategyInterleave class is an implementation to support
    *          the Bad block management, wear leveling and data recovery for the
    *          NAND FLASH using interleave pattern.
    */

    /**
    * @brief   This is the initialization routine. Checks the integrity of the translation
    *          table. Fix the translation table if any as per data recovery!
    *
    * @return OK initialized
    * @return FAILURE initialization failed
    */
    NvError NandStrategyInitialize(NvNandHandle hNand);
    /**
    * @brief   This API is a request to get Physical Block and page offset for a sector
    *          to write.This API can get in to three condition and they are:
    *              1) No physical block assigned for sector
    *              2) Physical Block is assigned, and the page in request is empty
    *              3) Physical block assigned, but the page to write is not empty
    *          The conditions above taken care as follow:
    *              1) No physical block assigned for sector -
    *                  Check if copyBack is true!!
    *                  If yes,
    *                      Copy the free pages in lastPhysicalBlock from the same
    *                      pages in the Physical Block assigned for Logical block (get
    *                      the logical block by calculating it from lastCluster).
    *                      Update translation table by freeing the Physical block assigned
    *                      for the logical block and assign the lastPhysicalBlock to the
    *                      logical block. This is as per Enhance Write throughput.
    *                      Set the copyBack flag to false!!
    *                  Assign new block if no physical block assigned for the logical
    *                  block and erase the block if not empty.
    *                  Save the new physical block, cluster and page to lastPhysicalBlock,
    *                  lastCluster and LastPage respectively.
    *                  Return the Device number, block number and page number!
    *              2) Physical Block is assigned and the page in request is empty -
    *                  Condition 1: Check if the current cluster write request is followed
    *                                  by lastCluster!!
    *                  Condition 2: Check if this is the last page to write in the block!!
    *                  If, (Condition 1 is no or Condition 2 is yes) and (copyBack is true),
    *                      Copy the free pages in lastPhysicalBlock from the same
    *                      pages in the Physical Block assigned for Logical block (get
    *                      the logical block by calculating it from lastCluster).
    *                      Update translation table by freeing the Physical block assigned
    *                      for the logical block and assign the lastPhysicalBlock to the
    *                      logical block. This is as per Enhance Write throughput.
    *                      Set the copyBack flag to false!!
    *                  Save the cluster and page to lastCluster and LastPage respectively.
    *                  Return the Device number, block number and page number!
    *              3) Physical block assigned and the page to write is not empty so
    *                  assign new block, erase if required and write the data to new
    *                  block.
    *                  Save the new physical block, cluster and page to lastPhysicalBlock,
    *                  lastCluster and LastPage respectively.
    *                  Set the copyBack flag to true!!
    *
    * @param   reqSectorNumber is the logical sector number from which write operartion starts
    * @param   reqSectors gives the requsted number of sectors we need to write
    * @param   pPhysPgs is pointer to the physical page array, which returns the page numbers to write to.
    * @param   logicalPageSectorOffset is the reference to sector offset of the requested sector in the logical page.
    * @param   sectors2Write is the reference to atual number of sectors that can be written from reqSectorNumber
    * @param   pTagBuffer reference to the tag buffer pointer.
    *
    * @return  OK Free block available
    * @return  NO_FREE_BLOCK No free block available
    */
    NvError NandStrategyGetSectorPageToWrite(
        NvNandHandle hNand,
        NvU32 reqSectorNumber,
        NvU32 reqSectors,
        NvS32* pPhysPgs,
        NvS32* logicalPageSectorOffset,
        NvS32* sectors2Write,
        NvU8** pTagBuffer);
    /**
    * @brief   This API is to get the physical address for the logical address for the read
    *          operation.
    *
    * @param   reqSectorNumber is the logical sector number to from which read operartion starts
    * @param   reqSectors gives the requsted number of sectors we need to read
    * @param   pPhysPgs is pointer to the physical pages array, which returns the page numbr to read from.
    * @param   logicalPageSectorOffset is the reference to sector offset of the requested sector in the logical page.
    * @param   sectors2Read is the reference to atual number of sectors that can be read from reqSectorNumber
    * @param   pTagBuffer reference to the tag buffer pointer.
    *
    * @return  OK operation successful
    * @return  NO_BLOCK_ASSIGNED no physical block assigned for the cluster or in other word
    *                          the read operation in invalid
    */
    NvError NandStrategyGetSectorPageToRead(
        NvNandHandle hNand,
        NvU32 reqSectorNumber,
        NvU32 reqSectors,
        NvS32* pPhysPgs,
        NvS32* logicalPageSectorOffset,
        NvS32* sectors2Read,
        NvU8** pTagInfoBuffer);
    /**
    * @brief   This API is to inform that the block alloted for the write operation
    *          in API getClusterPageToWrite has return with write error. So, this
    *          API will mark the previous block bad and return new block.The block
    *          replacement will be done from same device.
    *
    * @param   errorLogicalSector is the logical sector number on which the write operation failed
    * @param       writeError tells whehther the error is during the write or read operation.
    *
    * @return  OK operation successful
    * @return  NO_FREE_BLOCK No free block available
    */
    NvError NandStrategyReplaceErrorSector(NvNandHandle hNand, NvS32 errorLogicalSector, NvBool writeError);
    /**
    * @brief   This API is to flush the translation table to NAND FLASH
    *
    * @return  OK successful
    */
    NvError NandStrategyFlushTranslationTable(NvNandHandle hNand);

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
        NvBool IsWriteError);

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_NAND_STRATEGY_INTERLEAVE_H */
