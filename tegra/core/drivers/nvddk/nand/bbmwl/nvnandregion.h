/**
 * @file
 * @brief   Hardware NAND Flash Driver Interface.
 *
 * @b Description:  This file outlines the interface for a generic hardware
 *                  block device Interface class that can be used to transfer
 *                  data from block device driver to lower level hardware NAND device
 *                  driver.
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

#ifndef NVNANDREGION_H_
#define NVNANDREGION_H_

#include "nvddk_nand.h"
#include "nvddk_blockdev_defs.h"
#include "nverror.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/// Forward declartion.
typedef struct NvnandRegionRec* NvNandRegionHandle;
typedef  NvDdkBlockDevInfo NvNandRegionInfo;

/**
 * Structure for holding region properties
 */
typedef struct NandRegionProperties
{
    NvU32 StartLogicalBlock;
    NvU32 TotalLogicalBlocks;
    NvU32 StartPhysicalBlock;
    NvU32 TotalPhysicalBlocks;
    NvU32 InterleaveBankCount;
    NvU32 MgmtPolicy;
    // Flag to indicate last unbounded partition
    NvBool IsUnbounded;
    // Flag to indicate if sequential read required for partition
    NvBool IsSequencedReadNeeded;
}NandRegionProperties;

// Defines actual spare area lengths
enum
{
    // Actual length of nand spare area
    NAND_SPARE_AREA_LENGTH = 64,
    // Spare area used for storing user info
    NAND_TAG_AREA_LENGTH = 8
};

typedef struct InformationRec
{
    // 0:DataReserved; 1:!DataReserved
    NvU8 DataReserved :1;
    // 0:SystemReserved; 1:!SystemReserved
    NvU8 SystemReserved :1;
    // Page status 1:good/0:bad
    NvU8 PageStatus :1;
    // 0:BbReserved; 1:!BbReserved
    NvU8 BbReserved :1;
    // 0:FwReserved; 1:!FwReserved
    NvU8 FwReserved :1;
    // 0:TatReserved; 1:!TatReserved
    NvU8 TatReserved :1;
    // 0:TtReserved; 1:!TtReserved
    NvU8 TtReserved :1;
    // 0:NvpReserved; 1:!NvpReserved
    NvU8 NvpReserved :1;
}Information;

// user tag info that can be stores in the redundant area
//tag area definition for nand.
typedef struct TagInformationRec
{
    // specifies the condition of the block, 1:good/0:bad
    NvU8 BlockGood;
    // reserved status
    NvU8 BlkType;
    // private information defination is in TT
    Information Info;
    // reserved for future ussage
    NvU8 ReservedByte;
    // lba of the physical block
    NvS32 LogicalBlkNum;
    // Padding bytes
    NvU8 Padding[NAND_SPARE_AREA_LENGTH - NAND_TAG_AREA_LENGTH];
}TagInformation;


typedef struct NvDdkNandFunctionsPtrsRec
{
    /**
     * Reads the data from the selected Nand device(s) synchronously.
     *
     * @param hNand Handle to the Nand, which is returned by NvDdkNandOpen().
     * @param StartDeviceNum The Device number, which Read Operation has to be 
     *      started from. It starts from value '0'.
     * @param pPageNumbers Pointer to an array containing page numbers for
     *      each Nand Device. If there are (n + 1) Nand Devices, then 
     *      array size should be (n + 1).
     *      pPageNumbers[0] gives page number to access in Nand Device 0.
     *      pPageNumbers[1] gives page number to access in Nand Device 1.
     *      ....................................
     *      pPageNumbers[n] gives page number to access in Nand Device n.
     *      
     *      If Nand Device 'n' should not be accessed, fill pPageNumbers[n] as 
     *      0xFFFFFFFF.
     *      If the Read starts from Nand Device 'n', all the page numbers 
     *      in the array should correspond to the same row, even though we don't 
     *      access the same row pages for '0' to 'n-1' Devices.
     * @param pDataBuffer Pointer to read the page data into. The size of buffer 
     *      should be (*pNoOfPages * PageSize).
     * @param pTagBuffer Pointer to read the tag data into. The size of buffer 
     *      should be (*pNoOfPages * TagSize).
     * @param pNoOfPages The number of pages to read. This count should include 
     *      only valid page count. Consder that Total Nand Devices present is 4,
     *      Need to read 1 page from Device1 and 1 page from Device3. In this case,
     *      StartDeviceNum should be 1 and Number of pages should be 2.pPageNumbers[0]
     *      and pPageNumbers[2] should have 0xFFFFFFFF.pPageNumbers[1] and 
     *      pPageNumbers[3] should have valid page numbers.
     *      The same Pointer returns the number of pages Read successfully.
     * @param IgnoreEccError If set to NV_TRUE, It ignores the Ecc error and 
     *      continues to read the subsequent pages with out aborting read operation.
     *      This is required during bad block replacements.
     *
     * @retval NvSuccess Nand read operation completed successfully.
     * @retval NvError_NandReadEccFailed Indicates nand read encountered ECC 
     *      errors that can not be corrected.
     * @retval NvError_NandErrorThresholdReached Indicates nand read encountered 
     *      correctable ECC errors and they are equal to the threshold value set.
     * @retval NvError_NandOperationFailed Nand read operation failed.
     */
    NvError 
    (*NvDdkDeviceRead)(
        NvDdkNandHandle hNand,
        NvU8 StartDeviceNum,
        NvU32* pPageNumbers,
        NvU8* const pDataBuffer,
        NvU8* const pTagBuffer,
        NvU32 *pNoOfPages,
        NvBool IgnoreEccError);
    
    /**
     * Writes the data to the selected Nand device(s) synchronously.
     *
     * @param hNand Handle to the Nand, which is returned by NvDdkNandOpen().
     * @param StartDeviceNum The Device number, which Write Operation has to be 
     *      started from. It starts from value '0'.
     * @param pPageNumbers Pointer to an array containing page numbers for
     *      each Nand Device. If there are (n + 1) Nand Devices, then 
     *      array size should be (n + 1).
     *      pPageNumbers[0] gives page number to access in Nand Device 0.
     *      pPageNumbers[1] gives page number to access in Nand Device 1.
     *      ....................................
     *      pPageNumbers[n] gives page number to access in Nand Device n.
     *      
     *      If Nand Device 'n' should not be accessed, fill pPageNumbers[n] as 
     *      0xFFFFFFFF.
     *      If the Read starts from Nand Device 'n', all the page numbers 
     *      in the array should correspond to the same row, even though we don't 
     *      access the same row pages for '0' to 'n-1' Devices.
     * @param pDataBuffer Pointer to read the page data into. The size of buffer 
     *      should be (*pNoOfPages * PageSize).
     * @param pTagBuffer Pointer to read the tag data into. The size of buffer 
     *      should be (*pNoOfPages * TagSize).
     * @param pNoOfPages The number of pages to write. This count should include 
     *      only valid page count. Consder that Total Nand Devices present is 4,
     *      Need to write 1 page to Device1 and 1 page to Device3. In this case,
     *      StartDeviceNum should be 1 and Number of pages should be 2.pPageNumbers[0]
     *      and pPageNumbers[2] should have 0xFFFFFFFF.pPageNumbers[1] and 
     *      pPageNumbers[3] should have valid page numbers.
     *      The same Pointer returns the number of pages Written successfully.
     * 
     * @retval NvSuccess Operation completed successfully.
     * @retval NvError_NandOperationFailed Operation failed.
     */
    NvError
    (*NvDdkDeviceWrite)(
        NvDdkNandHandle hNand,
        NvU8 StartDeviceNum,
        NvU32* pPageNumbers,
        const NvU8* pDataBuffer,
        const NvU8* pTagBuffer,
        NvU32 *pNoOfPages);
    
    /**
     * Erases the selected blocks from the Nand device(s) synchronously.
     *
     * @param hNand Handle to the Nand, which is returned by NvDdkNandOpen().
     * @param StartDeviceNum The Device number, which Erase Operation has to be 
     *      started from. It starts from value '0'.
     * @param pPageNumbers Pointer to an array containing page numbers for
     *      each Nand Device. If there are (n + 1) Nand Devices, then 
     *      array size should be (n + 1).
     *      pPageNumbers[0] gives page number to access in Nand Device 0.
     *      pPageNumbers[1] gives page number to access in Nand Device 1.
     *      ....................................
     *      pPageNumbers[n] gives page number to access in Nand Device n.
     *      
     *      If Nand Device 'n' should not be accessed, fill pPageNumbers[n] as 
     *      0xFFFFFFFF.
     *      If the Read starts from Nand Device 'n', all the page numbers 
     *      in the array should correspond to the same row, even though we don't 
     *      access the same row pages for '0' to 'n-1' Devices.
     * @param pNumberOfBlocks The number of blocks to erase. This count should include 
     *      only valid block count. Consder that Total Nand Devices present is 4,
     *      Need to erase 1 block from Device1 and 1 block from Device3. In this case,
     *      StartDeviceNum should be 1 and Number of blocks should be 2.pPageNumbers[0]
     *      and pPageNumbers[2] should have 0xFFFFFFFF.pPageNumbers[1] and 
     *      pPageNumbers[3] should have valid page numbers corresponding to blocks.
     *      The same Pointer returns the number of blocks erased successfully.
     *
     * @retval NvSuccess Operation completed successfully.
     * @retval NvError_NandOperationFailed Operation failed.
     */
    NvError 
    (*NvDdkDeviceErase)(
        NvDdkNandHandle hNand,
        NvU8 StartDeviceNum,
        NvU32* pPageNumbers,
        NvU32* pNumberOfBlocks);
    
    /**
     * Gets the Nand flash device information.
     *
     * @param hNand Handle to the Nand, which is returned by NvDdkNandOpen().
     * @param DeviceNumber Nand Flash device number.
     * @param pDeviceInfo Returns the device information.
     *
     * @retval NvSuccess Operation completed successfully.
     * @retval NvError_NandOperationFailed Nand copy back operation failed.
     */
    NvError 
    (*NvDdkGetDeviceInfo)(
        NvDdkNandHandle hNand,
        NvU8 DeviceNumber,
        NvDdkNandDeviceInfo* pDeviceInfo);
    
    /**
     * Returns the details of the Locked apertures.like Device number, Starting 
     * page number, Ending page number of the region locked
     *
     * @param hNand Handle to the Nand, which is returned by NvDdkNandOpen().
     * @param pFlashLockParams Pointer to first array element of LockParams type 
     * with eight elements in the array. 
     * check if pFlashLockParams[i].DeviceNumber == 0xFF, then that aperture is free to 
     * use for locking.
     */
    void 
    (*NvDdkGetLockedRegions)(
        NvDdkNandHandle hNand,
        LockParams* pFlashLockParams);

    /**
     * Gives the block specific information such as tag information, lock status, block good/bad
     * @param hNand Handle to the Nand, which is returned by NvDdkNandOpen().
     * @param DeviceNumber Device number in which the requested block exists
     * @param BlockNumber requested physical block number
     * @param pBlockInfo return the block information
     * @param SkippedBytesReadEnable if NV_TRUE causes skipped 
     *              bytes along with tag info
     *
     * @retval NvSuccess Operation completed successfully.
     * @retval NvError_NandOperationFailed Operation failed.
     */
    NvError
    (*NvDdkGetBlockInfo)(
        NvDdkNandHandle hNand,
        NvU32 DeviceNumber,
        NvU32 BlockNumber,
        NandBlockInfo* pBlockInfo,
        NvBool SkippedBytesReadEnable);

    NvError
    (*NvDdkReadSpare)(
        NvDdkNandHandle hNand,
        NvU8 StartDeviceNum,
        NvU32* pPageNumbers,
        NvU8* const pSpareBuffer,
        NvU8 OffsetInSpareAreaInBytes,
        NvU8 NumSpareAreaBytes);

    NvError
    (*NvDdkWriteSpare)(
        NvDdkNandHandle hNand,
        NvU8 StartDeviceNum,
        NvU32* pPageNumbers,
        NvU8* const pSpareBuffer,
        NvU8 OffsetInSpareAreaInBytes,
        NvU8 NumSpareAreaBytes);

}NvDdkNandFunctionsPtrs;

NvError 
NvNandOpenRegion(
    NvNandRegionHandle* phNandRegion,
    NvDdkNandHandle hNvDdkNand,
    NvDdkNandFunctionsPtrs *pFuncPtrs,
    NandRegionProperties* nandregion);

/**
 * Structure for holding nand region Interface pointers.
 */
typedef struct NvnandRegionRec
{
    /**
     * It closes the region safely and releases all the resources allocated.
     *
     * @param hNandRegion Handle acquired during the NvNandOpenRegion call.
     *
     */
    void (*NvNandCloseRegion)(NvNandRegionHandle hNandRegion);

    /**
     * It gets the region's information.
     * If the region is present and initialized successfully, it will return 
     * valid number of TotalBlocks.
     * If the region is neither present nor initialization failed, then it will
     * return TotalBlocks as zero.
     *
     * @param hNandRegion Handle acquired during the NvNandOpenRegion call.
     * @param pNandRegionInfo Returns region's information.
     *
     */
    void
    (*NvNandRegionGetInfo)(
        NvNandRegionHandle hNandRegion,
        NvNandRegionInfo* pNandRegionInfo);
    /**
     * Reads data synchronously from the nand region.
     *
     * @warning The region needs to be initialized using NvNandOpenRegion before
     * using this method.
     *
     * @param hNandRegion Handle acquired during the NvNandOpenRegion call.
     * @param SectorNum Sector number to read the data from.
     * @param pBuffer Pointer to Buffer, which data will be read into.
     * @param NumberOfSectors Number of sectors to read.
     *
     * @retval NvSuccess Read is successful.
     * @retval NvError_ReadFailed Read operation failed.
     * @retval NvError_InvalidBlock Sector(s) address doesn't exist in the region.
     */
    NvError 
    (*NvNandRegionReadSector)(
        NvNandRegionHandle hNandRegion,
        NvU32 SectorNum, 
        void* const pBuffer, 
        NvU32 NumberOfSectors);

    /**
     * Writes data synchronously to the nand region.
     *
     * @warning The region needs to be initialized using NvNandOpenRegion before
     * using this method.
     *
     * @param hNandRegion Handle acquired during the NvNandOpenRegion call.
     * @param SectorNum Sector number to write the data to.
     * @param pBuffer Pointer to Buffer, which data will be written from.
     * @param NumberOfSectors Number of sectors to write.
     *
     * @retval NvSuccess Write is successful.
     * @retval NvError_WriteFailed Write operation failed.
     * @retval NvError_InvalidBlock Sector(s) address doesn't exist in the region.
     */
    NvError 
    (*NvNandRegionWriteSector)(
        NvNandRegionHandle hNandRegion,
        NvU32 SectorNum, 
        const void* pBuffer, 
        NvU32 NumberOfSectors);

    /**
     * Flush buffered data in the reigion out to the device.
     *
     * @warning The region needs to be initialized using NvNandOpenRegion before
     * using this method.
     *
     * @param hNandRegion Handle acquired during the NvNandOpenRegion call.
     */
    void (*NvNandRegionFlushCache)(NvNandRegionHandle hNandRegion);

    /**
     * Perform an I/O Control operation on the region.
     *
     * @param hNandRegion Handle acquired during the NvNandOpenRegion call.
     * @param Opcode Type of control operation to perform
     * @param InputSize Size of input arguments buffer, in bytes
     * @param OutputSize Size of output arguments buffer, in bytes
     * @param InputArgs Pointer to input arguments buffer
     * @param OutputArgs Pointer to output arguments buffer
     *
     * @retval NvError_Success Ioctl is successful
     * @retval NvError_NotSupported Opcode is not recognized
     * @retval NvError_InvalidSize InputSize or OutputSize is incorrect.
     * @retval NvError_InvalidParameter InputArgs or OutputArgs is incorrect.
     * @retval NvError_IoctlFailed Ioctl failed for other reason.
     */
    NvError
    (*NvNandRegionIoctl)(
        NvNandRegionHandle hNandRegion,
        NvU32 Opcode,
        NvU32 InputSize,
        NvU32 OutputSize,
        const void *InputArgs,
        void *OutputArgs);

}NvnandRegion;


/*
  * This API initializes the block translation layer
  * @param hNvDdkNand Handle to the low level nand ddk driver
  * @param pFuncPtrs Pointer to the structure containings the pointers to Ddk APIs
  * @param GlobalInterleaveCount  interleave count for the nand device
  */
    void NvBtlInit(NvDdkNandHandle hNvDdkNand, NvDdkNandFunctionsPtrs *pFuncPtrs, NvU32 
    GlobalInterleaveCount);
/*
  * This API translates given device number and physical block number to actual device and PBA.
  * For example there are two nand devices on board but interleave count is one. Client to FTL
  * will treat there is only one device and gives block address sequentially from one device to
  * other. This api will break the given block address to absoulte address with respect to the chip.
  *
  * @param hNvDdkNand Handle to the low level nand ddk driver
  * @param GlobalInterleaveCount  interleave count for the nand device
  */
NvBool NvBtlGetPba(NvU32 *DevNum, NvU32 *BlockNum);

#if defined(__cplusplus)
}
#endif  /* __cplusplus */


#endif /* NVNANDREGION_H_ */
