/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * @brief <b> Hardware NAND Flash Driver Interface.</b>
 *
 * @b Description:  This file outlines the interface for a generic hardware
 *                  file system Interface class that can be used to transfer
 *                  data from file system to lower level hardware NAND device
 *                  driver.
 */

#ifndef INCLUDED_HW_NAND_H
#define INCLUDED_HW_NAND_H

#include "nanddefinitions.h"
#include "nvddk_nand_blockdev_defs.h"

#if defined(__cplusplus)
extern "C"
{
#endif

    enum
    {
        // Sector size
        SECTOR_SIZE = 512,
        // Eight word or 32 byte alignment
        EIGHT_WORD_ALIGNMENT = 32,
        // Four word alignment
        FOUR_WORD_ALIGNMENT = 16,
        // One word alignment
        ONE_WORD_ALIGNMENT = 4,
        // run-time good marker
        RUN_TIME_GOOD_MARK = 0xFF,
        // time out for shim operations
        NV_SHIM_NAND_TIME_OUT = 15000
    };

    NvBool NandTLInit(NvNandHandle hNand);

    // gets the device info for the requested deviceNumber
    NvError
    NandTLGetDeviceInfo(
                       NvNandHandle hNand,
                       NvS32 DeviceNo,
                       NandDeviceInfo* pDevInfo);

    // prints the debug string to an output terminal
    void DbgOutput(NvNandHandle hNand, NvS8* str);

    // Callback for every device/bank present on the board
    void OnBankIdentify(NvNandHandle hNand, NvS32 BankNum, NvS32 Location);

    void
    NandGetInfo(
               NvNandHandle hNand,
               NvU32 DeviceNo,
               NandDeviceInfo* pDevInfo);

    void NandDbgOutput(NvS8* str);

    void NandOnBankIdentify(NvS32 BankNum, NvS32 Location);

    // NandEntree  Methods - See NandEntree  Class for the functions description.
    NvError
    NandTLReadPhysicalPage(
                          NvNandHandle hNand,
                          NvS32* pPageNumbers,
                          NvU32 SectorOffset,
                          NvU8 *pData,
                          NvU32 SectorCount,
                          NvU8* pTagBuffer,
                          NvBool IgnoreEccError);

    NvError
    NandTLWritePhysicalPage(
        NvNandHandle hNand,
        NvS32* pPageNumbers,
        NvU32 SectorOffset,
        NvU8 *pData,
        NvU32 SectorCount,
        NvU8* pTagBuffer);

    NvError NandTLErase(NvNandHandle hNand, NvS32* pPageNumbers);

    NvError
        NandTLCopyBack(
        NvNandHandle hNand,
        NvS32* pSourcePageNumbers,
        NvS32* pDstnPageNumbers,
        NvU32 SrcSectorOffset,
        NvU32 DstSectorOffset,
        NvS32* SectorCount,
        NvBool IgnoreEccError);

    NvError NandTLGetBlockStatus(NvNandHandle hNand, NvS32* pPageNumbers);

    NvError NandTLFlushTranslationTable(NvNandHandle hNand);

    // NandSectorCache::ReadWriteStrategy Methods - See MultiSectorCache::ReadWriteStrategy Class for the functions description.
    NvError
    NandTLWrite(
        NvNandHandle hNand,
        NvU32 SectorAddr,
        NvU8 *data,
        NvS32 SectorCount);

    NvError
    NandTLRead(
              NvNandHandle hNand,
              NvS32 lun,
              NvU32 SectorAddr,
              NvU8 *data,
              NvS32 sectorCount);

    NvS32 
     NandTLGetSectorSize(NvNandHandle hNand);
    NvBool
    NandTLIsLbaLocked(NvNandHandle hNand, NvS32 reqPageNum);
    
    NvBool
    NandTLValidateParams(
        NvNandHandle hNand,
        NvS32* pPageNumbers,
        NvU32 sectorOffset,
        NvS32 sectorCount);

    NvError 
    NandTLSetRegionInfo(
        NvNandHandle hNand,
        NvDdkBlockDevIoctl_DefineSubRegionInputArgs* pSubRegionInfo);

    NvBool 
    NandTLEraseLogicalBlocks(
        NvNandHandle hNand,
        NvU32 StarBlock,
        NvU32 EndBlock);

    NvError NandTLGetBlockInfo(
        NvNandHandle hNand, BlockInfo* block, NandBlockInfo * pDdkBlockInfo,
        NvBool SkippedBytesReadEnable);

    NvU32 NandStrategyGetTrackingListCount(NvNandHandle hNand);

    NvError NandTLEraseAllBlocks(NvNandHandle hNand);

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

#endif // INCLUDED_HW_NAND_H
