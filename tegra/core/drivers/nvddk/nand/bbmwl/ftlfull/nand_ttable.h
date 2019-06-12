/**
 * @file
 * @brief   Hardware NAND device driver strategy implementation for Sequential
 *          pattern Translation Table.
 *
 *
 * @b Description:  This file implements of strategy required for Translation
 *                  Table for sequential pattern NAND FLASH.
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

#ifndef INCLUDED_NAND_INTERLEAVE_TTABLE_H
#define INCLUDED_NAND_INTERLEAVE_TTABLE_H

#include "nanddefinitions.h"
#include "nand_debug_utility.h"
#include "nvrm_memmgr.h"
#include "nvnandftlfull.h"

/**
 * @brief Constructor for NandInterleaveTTable.
 *
 */
NvError InitNandInterleaveTTable(NvNandHandle hNand);

void DeInitNandInterleaveTTable(NvNandHandle hNand);

/**
 * @brief   This API Gets a free physical block(s) from
 * the available nand devices
 *
 * @param   lba logical block address for which we need a
 * new free physical block
 * @param   pPrevPhysBlks is a pointer to the current
 * physical block address for the requested lba
 * @param   pPhysBlks is pointer to the new PBA's allocated.
 *
 * @return  Status is the error status
 */
NvError
GetAlignedPBA(
                      NvNandHandle hNand,
                      NvS32 lba,
                      NvS32* pPrevPhysBlks,
    int* pPhysBlks,
    NvBool ForBadBlkMagement);
NvError
GetFreePBA(
    NvNandHandle hNand,
    int lba,
    int* pPrevPhysBlks,
    int* pPhysBlks,
    NvBool ForBadBlkMagement);
/**
 * @brief   This API Gets a physical block(s) associated with the lba
 *
 * @param   lba logical block address for which we need the
 * associated physical blocks.
 * @param   pPhysBlks is pointer to the new PBA's allocated.
 *
 * @return  Status is the error status
 */
NvError GetPBA(NvNandHandle hNand, NvS32 lba, NvS32* pPhysBlks);

/**
 * @brief   This API updates the region info for the physical block.
 *
 * @param   hNand nand tfl full handle
 * @param   pPhysBlks pointer tp  physical block addresses
 * @param   IsDataReserved specifies wether the pba should be marked as reservesd or not
 *
 * @return  Status is the error status
 */
 
NvError
SetPbaRegionInfo(
             NvNandHandle hNand,
             NvS32* pPhysBlks,
             NvS32 RegionNumber,
             NvBool IsDataReserved);
             
/**
 * @brief   This API allocates a physical block(s) to a logical block.
 *
 * @param   pPhysBlks pointer tp  physical block addresses
 * that need to be assigned
 * @param   lba logical block address for which we need assign physical block.
 * @param   updateTagInfo specifies wether we should update
 * taginfo of spare area or not
 *
 * @return  Status is the error status
 */

NvError
AssignPba2Lba(
             NvNandHandle hNand,
             NvS32* pPhysBlks,
             NvS32 lba,
             NvBool UpdateTagInfo);
/**
 * @brief   This API marks a physical block as bad.
 *
 * @param   bank pba's bank number.
 * @param   pba pPhysicalBlock address that need to be marked as bad.
 *
 * @return  Status is the error status
 */
NvError SetPbaBad (NvNandHandle hNand, NvS32 bank, NvS32 pba);

/**
 * @brief   This API marks a physical block as unused.
 *
 * @param   bank pba's bank number.
 * @param   pba physical block address that need to be marked as unused.
 *
 * @return  Status is the error status
 */
NvError SetPbaUnused(NvNandHandle hNand, NvS32 bank, NvS32 pba);

/**
 * @brief   This API marks a physical block as data reserved/non reserved.
 *
 * @param   bank pba's bank number.
 * @param   pba physical block address that need to be marked as unused.
 * @param   reserved indicates whether to set the block as data
 * reserved/non reserved.
 *
 * @return  Status is the error status
 */
NvError SetPbaReserved(
    NvNandHandle hNand,
    NvS32 bank,
    NvS32 pba,
    NvBool Reserved);

/**
 * @brief   This API tells whether the physical block is a data reserved block.
 *
 * @param   bank pba's bank number.
 * @param   pba physical block address that needs the data reserved information.
 *
 * @return  Status is the error status
 */
NvBool IsPbaReserved(NvNandHandle hNand, NvS32 bank, NvS32 pba);

/**
 * @brief   This API releases a block from the reserved blocks of a zone.
 *
 * @param   bank pba's bank number.
 * @param   zone zone number from which the block need to be released.
 *
 * @return
 */
NvError ReleasePbaFromReserved(NvNandHandle hNand, NvS32 bank, NvS32 zone);

/**
 * @brief   This API is to flush the translation table to NAND FLASH
 *
 * @param   logicalPage if it is -1, all dirty pages will be flushed,
 * else specified page will be flushed.
 * @param   data pointer to the data buffer of a specified logical page.
 * if logicalPage is -1, this param is not applicable.
 * @param   flushTAT if it is NV_TRUE, TAT will be flushed either.
 *
 * @return  OK successful
 */

NvError
FlushTranslationTable(
                     NvNandHandle hNand,
                     NvS32 LogicalPage,
                     NvS8 *pData,
                     NvBool FlushTAT);

void MapLBA(NvNandHandle hNand, NvS32* pLba);

void InitBucket(Bucket *bucket);

void InitBlockInfo(BlockInfo *blockInfo);

void SetBlockInfo(BlockInfo *blockInfo, NvS32 BankNo, NvS32 BlockNo);

void GetBlockInfo(BlockInfo* blockInfo, NvS32* pBankNo, NvS32* pBlockNo);

void InitTranslationTableCache(TranslationTableCache *pTTCache);

 void DumpTT(NvNandHandle hNand, NvS32 page);

extern const NvS8 NAND_IL_TAT_HEADER[];
extern const NvS8 NAND_IL_TAT_FOOTER[];
extern const NvS8 NAND_FACTORY_BB_TABLE_SIGNATURE[];

void
InitTranslationAllocationTable(
                              TranslationAllocationTable*pTransAllocTable,
                              NvS32 BlockCount,
                              NvS32 PageCount,
                              NvS32 SectorLength);

void CopyTranslationAllocationTable(
    NvNandHandle hNand,
    TranslationAllocationTable *pOrig,
    NvS8 *pAddr);

void
InitTatHandler(
              TatHandler *pTatHandler,
              NvS32 BlockCount,
              NvS32 PageCount,
              NvS32 SectorLength);

NvError InitNandInterleaveTAT(NvNandHandle hNand);

void DeInitNandInterleaveTAT(NvNandHandle hNand);

// Converts a TT entry number to block status entry number
NvS32 GetTTableBlockStatusEntryNumber(
    NvNandHandle hNand,
    NvS32 bank,
    NvS32 ttEntry);

// clears the physical page array.
void ClearPhysicalPageArray(NvNandHandle hNand, NvS32* pPages);

NvError PageModified(NvNandHandle hNand, NvS32 LogicalPageNumber);

NvError GetPage(NvNandHandle hNand, NvS32 logicalPage, NvS8* ttPage);

NvError FlushCachedTT(
    NvNandHandle hNand,
    NvS32 LogicalPageNumber,
    NvS8* data);

void MarkBlockAsBad(
    NvNandHandle hNand,
    NvS32 bank,
    NvS32 physBlk);

NvError FlushTAT(NvNandHandle hNand);

NvBool ReplaceTTBadBlock(
    NvNandHandle hNand,
    NvS32 ttLogicalPageNumber,
    NvS32 ttLogicalBlockNumber,
    NvS32* currentBlock,
    NvS32* currentPage,
    NvS32* badPageCount);

#endif // INCLUDED_NAND_INTERLEAVE_TTABLE_H

