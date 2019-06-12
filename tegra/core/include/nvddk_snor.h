/*
 * Copyright (c) 2008-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file
 * <b> NVIDIA Driver Development Kit: SNOR Driver APIs</b>
 *
 * @b Description: This file defines the interface for NvDDK SNOR module.
 */


#ifndef INCLUDED_NVDDK_SNOR_H
#define INCLUDED_NVDDK_SNOR_H


#include "nvcommon.h"
#include "nvos.h"
#include "nvddk_blockdev.h"

#if defined(__cplusplus)
 extern "C"
 {
#endif  /* __cplusplus */

#define DEBUG_PRINTS 0

/**
 * @defgroup nvbdk_ddk_Snor SNOR Driver APIs
 *
 *
 * @ingroup nvddk_modules
 * @{
 */

 /**
  * @brief Opaque handle for NvDdkSnor context
  */
typedef struct NvDdkSnorRec *NvDdkSnorHandle;

/**
 * NOR flash device information.
 */
typedef struct
{
    // Vendor ID.
    NvU8 VendorId;
    // Bus width of the chip: can be 8- or 16-bit.
    NvU8 BusWidth;
    // Page size in bytes, includes only data area, no redundant area.
    NvU32 PageSize;
    // Number of Pages per block.
    NvU32 PagesPerBlock;
    // Total number of blocks that are present in the NOR flash device.
    NvU32 NoOfBlocks;
    /**
     * Holds the zones per flash device--minimum value possible is 1.
     * Zone is a group of contiguous blocks among which internal copy back can
     * be performed, if the chip supports copy-back operation.
     * Zone is also referred as plane or district by some flashes.
     */
    NvU32 ZonesPerDevice;
    /**
     * Total device capacity in kilobytes.
     * Includes only data area, no redundant area.
     */
    NvU32 DeviceCapacityInKBytes;
    // Number of NOR flash devices present on the board.
    NvU8 NumberOfDevices;
    // Size of Spare area
    NvU32 NumSpareAreaBytes;
}NvDdkNorDeviceInfo;

/**
 * The structure for locking of required NOR flash pages.
 */
typedef struct
{
    // Device number of the flash being protected by lock feature.
    NvU8 DeviceNumber;
    // Starting page number, from where NOR lock feature should protect data.
    NvU32 StartPageNumber;
    // Ending page number, up to where NOR lock feature should protect data.
    NvU32 EndPageNumber;
}LockParams;

/**
 * Initializes the SNOR driver and returns a created handle to the client.
 * Only one instance of the handle can be created.
 *
 * @pre The client must call this API first before calling any further APIs.
 *
 * @param hDevice Handle to RM device.
 * @param Instance Instance ID for the nor controller.
 * @param phSnor A pointer to the handle where the created handle will be stored.
 *
 * @retval NvSuccess Initialization is successful.
 * @retval NvError_AlreadyAllocated The driver is already in use.
 */
NvError
NvDdkSnorOpen(
    NvRmDeviceHandle hDevice,
    NvU8 Instance,
    NvDdkSnorHandle *phSnor);

/**
 * Closes the SNOR driver and frees the handle.
 *
 * @param hSnor Handle to the SNOR driver.
 *
 */
void NvDdkSnorClose(NvDdkSnorHandle hSnor);

/**
 * Reads data from the SNOR device.
 *
 * @param hSnor Handle to the SNOR driver.
 *
 * @param ChipNum The device number, which read operation has to be
 *      started from. It starts from value '0'.
 *
 * @param StartByteOffset The byte offset from where to start read
 *	operation.
 *
 * @param SizeInBytes Number of byte to be read. The number of bytes
  *      should be (*pNoOfPages * PageSize).
 * @param pBuffer A pointer to read the page data into. The size of buffer
 *      should be (*pNoOfPages * PageSize).
 * @retval NvSuccess NOR read operation completed successfully.
 * @retval NvError_NorOperationFailed NOR read operation failed.
 */
NvError
NvDdkSnorRead(
    NvDdkSnorHandle hSnor,
    NvU8 ChipNum,
    NvU32 StartByteOffset,
    NvU32 SizeInBytes,
    NvU8* pBuffer,
    NvU32* SnorStatus);

/**
 * Writes data from the SNOR device.
 *
 * @param hSnor Handle to the SNOR driver.
 *
 * @param ChipNum The device number, which write operation has to be
 *      started from. It starts from value '0'.
 *
 * @param StartByteOffset The byte offset from where to start write
 *	operation.
 *
 * @param SizeInBytes Number of byte to be write. The number of bytes
  *      should be (*pNoOfPages * PageSize).
 * @param pBuffer A pointer to write the page data from. The size of buffer
 *      should be (*pNoOfPages * PageSize).
 *
 * @retval NvSuccess NOR read operation completed successfully.
 * @retval NvError_NorOperationFailed NOR write operation failed.
 */
NvError
NvDdkSnorWrite(
    NvDdkSnorHandle hSnor,
    NvU8 ChipNum,
    NvU32 StartByteOffset,
    NvU32 SizeInBytes,
    const NvU8* pBuffer,
    NvU32* SnorStatus);

/**
 * Erases the NOR flash.
 *
 * @param hSnor Handle to the NOR driver.
 *
 * @param hSnor Handle to the NOR, which is returned by NvDdkSnorOpen().
 * @param pPageNumbers A pointer to an array containing page numbers for
 *      each NOR device. If there are (n + 1) NOR devices, then
 *      array size should be (n + 1).
 *      - pPageNumbers[0] gives page number to access in NOR Device 0.
 *      - pPageNumbers[1] gives page number to access in NOR Device 1.
 *      - ....................................
 *      - pPageNumbers[n] gives page number to access in NOR Device n.
 *
 *      If NOR Device 'n' should not be accessed, fill \a pPageNumbers[n] as
 *      0xFFFFFFFF.
 *      If the read starts from NOR device 'n', all the page numbers
 *      in the array should correspond to the same row, even though we don't
 *      access the same row pages for '0' to 'n-1' Devices.
 * @param pNumberOfBlocks The number of blocks to erase. This count should include
 *      only valid block count. Consider that total NOR devices present is 4,
 *      Need to erase 1 block from Device1 and 1 block from Device3. In this case,
 *      \a StartDeviceNum should be 1 and number of blocks should be 2.
 *      \a pPageNumbers[0] and \a pPageNumbers[2] should have 0xFFFFFFFF.
 *      \a pPageNumbers[1] and \a pPageNumbers[3] should have valid page numbers
 *      corresponding to blocks.
 *      The same pointer returns the number of blocks erased successfully.
 *
 * @retval NvSuccess Operation completed successfully.
 * @retval NvError_NorOperationFailed Operation failed.
 */
NvError
NvDdkSnorErase(
    NvDdkSnorHandle hSnor,
    NvU8 StartDeviceNum,
    NvU32* pPageNumbers,
    NvU32* pNumberOfBlocks);

/**
 * Gets the NOR flash device information.
 *
 * @param hSnor Handle to the NOR driver.
 * @param DeviceNumber Nor flash device number.
 * @param pDeviceInfo Returns the device information.
 *
 * @retval NvSuccess Operation completed successfully.
 * @retval NvError_NorOperationFailed NOR operation failed.
 */
NvError
NvDdkSnorGetDeviceInfo(
    NvDdkSnorHandle hSnor,
    NvU8 DeviceNumber,
    NvDdkNorDeviceInfo* pDeviceInfo);

/**
 * Returns the details of the locked apertures, like device number, starting
 * page number, ending page number of the region locked.
 *
 * @param hSnor Handle to the NOR driver.
 * @param pFlashLockParams A pointer to first array element of \a LockParams type
 * with eight elements in the array.
 * Check if \a pFlashLockParams[i].DeviceNumber == 0xFF, then that aperture is free to
 * use for locking.
 */
void
NvDdkSnorGetLockedRegions(
    NvDdkSnorHandle hSnor,
    LockParams* pFlashLockParams);

/**
 * Locks the specified NOR flash pages.
 *
 * @param hSnor Handle to the NOR driver.
 * @param pFlashLockParams A pointer to the range of pages to be locked.
 */
void
NvDdkSnorSetFlashLock(
    NvDdkSnorHandle hSnor,
    LockParams* pFlashLockParams);

/**
 * Releases all regions that were locked using NvDdkSnorSetFlashLock().
 *
 * @param hSnor Handle to the SNOR driver.
 */
void NvDdkSnorReleaseFlashLock(NvDdkSnorHandle hSnor);


/**
 * Disable the SNOR controller. The clock is disabled and the RM is
 * notified that the driver is ready to be suspended.
 *
 * @param hSnor Handle to the SNOR driver.
 */
NvError NvDdkSnorSuspend(NvDdkSnorHandle hSnor);

/**
 * Resumes the SNOR controller from suspended state.
 *
 * @param hSnor Handle to the SNOR driver.
 *
 * @retval NvSuccess The driver is successfully resumed.
 */
NvError NvDdkSnorResume(NvDdkSnorHandle hSnor);

/**
 * It allocates the required resources, powers on the device,
 * and prepares the device for I/O operations.
 * Client gets a valid handle only if the device is found.
 * The same handle must be used for further operations.
 * The device can be opened by only one client at a time.
 *
 * @pre This method must be called once before using other
 *           NvDdkBlockDev APIs.
 *
 * @param Instance Instance of specific device.
 * @param MinorInstance Minor instance of specific device.
 * @param phBlockDev Returns a pointer to device handle.
 *
 * @retval NvSuccess Device is present and ready for I/O operations.
 * @retval NvError_DeviceNotFound Device is neither present nor responding
 *                                   nor supported.
 * @retval NvError_DeviceInUse The requested device is already in use.
 * @retval NvError_InsufficientMemory Cannot allocate memory.
 */
NvError
NvDdkSnorBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev);

/**
 * It write protects (includes erases) blocks.
 *
 * @param hSnor Handle to the specific device.
 * @param StartOffset Byteoffset from where protection is needed
 * @param Length Number of bytes to be protected
 * @param IsLastPartition Indicates if this is last parition. Used
 *        to do any post verification/locking after all block are locked.
 *
 * @retval NvSuccess Sectors are protected.
 * @retval NvError_InvalidState error while trying to protect
 */
NvError
NvDdkSnorProtectSectors(
    NvDdkSnorHandle hSnor,
    NvU32 StartOffset,
    NvU32 Length,
    NvBool IsLastPartition);

/**
 * Erases all protection bits & allows blocks to be erased/written
 *
 * @param hSnor Handle to the specific device.
 *
 * @retval NvSuccess Sectors are protected.
 * @retval NvError_InvalidState error while trying to unprotect
 */
NvError
NvDdkSnorUnprotectAllSectors(
    NvDdkSnorHandle hSnor);

NvError NvDdkSnorBlockDevInit(NvRmDeviceHandle hDevice);

void NvDdkSnorBlockDevDeinit(void);

NvError
NvDdkSnorGetNumBlocks(
    NvDdkSnorHandle hSnor,
    NvU32 *Size);

NvError
NvDdkSnorGetEraseSize(
    NvDdkSnorHandle hSnor,
    NvU32 *Size);

NvError
NvDdkSnorFormatDevice(
    NvDdkSnorHandle hSnor,
    NvU32 *SnorStatus);

NvError
NvDdkSnorEraseSectors(
    NvDdkSnorHandle hSnor,
    NvU32 StartOffset,
    NvU32 Length);

#if defined(__cplusplus)
 }
#endif  /* __cplusplus */

 /** @} */

#endif  /* INCLUDED_NVDDK_SNOR_H */

