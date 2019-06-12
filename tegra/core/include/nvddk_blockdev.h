/*
 * Copyright (c) 2008-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b> NVIDIA Driver Development Kit: Block Device Interface</b>
 *
 * @b Description: This file declares the interface for block devices.
 */

#ifndef INCLUDED_NVDDK_BLOCKDEV_H
#define INCLUDED_NVDDK_BLOCKDEV_H

#include "nverror.h"
#include "nvcommon.h"
#include "nvos.h"
#include "nvrm_init.h"
#include "nvddk_blockdev_defs.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/// Forward declarations
typedef struct NvDdkBlockDevRec *NvDdkBlockDevHandle;

/**
 * @defgroup nvbdk_ddk_blkdevmulti Block Device Interface (Multipartition)
 *
 * This is the interface for block devices.
 *
 * @par Theory of Operations
 *
 * This API presents a virtualized view of a block device. Implementation must
 * support multiple (virtual) instances of the block device, and multiple
 * concurrent operations per instance.
 *
 * If the underlying DDK does not support multiple instances or if the DDK does
 * not natively support concurrent operations, these capabilities must be
 * implemented within the block driver.
 *
 * @par Usage Model
 *
 * The global state must be initialized exactly once, via the
 * <a href="#NvDdkXxxBlockDevInit">NvDdkXxxBlockDevInit()</a> routine.
 * Thereafter, one or more (virtual) instances
 * of the block device can be opened by calling NvDdkBlockDevOpen() one or more
 * times. Each open returns an handle, which is used to carry out all further
 * operations on the block device, as defined in the ::NvDdkBlockDevRec
 * structure.
 *
 * After all operations have been completed for a given instance of the block
 * device, call NvDdkBlockDevClose() to close the instance and release its
 * resources. When all instances have been closed, call NvDdkXxxBlockDevDeinit()
 * to release the global resources.
 *
 * @par Implementation Details
 *
 * DDKs that support the virtual block device interface must implement the
 * operations defined by the ::NvDdkBlockDevRec structure. The DDK must
 * also implement the following APIs, where "Xxx" is the specific media type
 * such as Hsmmc, Sd, Nand, Ide, etc.
 * 
 * - <a href="#NvDdkXxxBlockDevInit">NvDdkXxxBlockDevInit()</a>
 * - <a href="#NvDdkXxxBlockDevOpen">NvDdkXxxBlockDevOpen()</a>
 * - <a href="#NvDdkXxxBlockDevDeinit">NvDdkXxxBlockDevDeinit()</a>
 *
 * The required prototypes for these functions are specified in this topic. DDKs
 * must implement the actual functions, replacing "Xxx" with the name of the
 * media type.
 * @ingroup nvddk_modules
 * @{
 */


/**
 * NvDdkXxxBlockDevInit()
 *
 * <a name="NvDdkXxxBlockDevInit">Allocates</a> and initializes
 * the required global resources
 *
 * The required function prototype is as follows:
 *
 * @code   NvError NvDdkXxxBlockDevInit(NvRmDeviceHandle hDevice);    @endcode
 *
 * @pre This method must be called once before using other NvDdkXxxBlockDev
 *          APIs.
 *
 * @param hDevice RM device handle.
 *
 * @retval NvSuccess Global resources have been allocated and initialized.
 * @retval NvError_DeviceNotFound Device is not present or not supported.
 * @retval NvError_InsufficientMemory Cannot allocate memory.
 */
typedef NvError
(*pfNvDdkXxxBlockDevInit)(
    NvRmDeviceHandle hDevice);

/**
 * NvDdkXxxBlockDevOpen()
 *
 * <a name="NvDdkXxxBlockDevOpen">Allocates</a> resources, powers on the device,
 *  and prepares the device for
 * I/O operations. A valid ::NvDdkBlockDevHandle is returned only if the
 * device is found. The same handle must be used for further operations
 * (except for <a href="#NvDdkXxxBlockDevDeinit">NvDdkXxxBlockDevDeinit()</a>).
 * Multiple invocations from multiple
 * concurrent clients must be supported.
 *
 * The required function prototype is as follows.
 * @code
 *    NvError 
 *    NvDdkXxxBlockDevOpen(
 *        NvU32 Instance,
 *        NvU32 MinorInstance,
 *        NvDdkBlockDevHandle *phBlockDevInstance);
 * @endcode
 *
 * @pre This method must be called once before using other NvDdkBlockDev
 *          APIs.
 *
 * @param Instance Instance of specific device.
 * @param MinorInstance Minor instance of the specific device.
 * @param phBlockDev A returned pointer to device handle.
 *
 * @retval NvSuccess Device is present and ready for I/O operations.
 * @retval NvError_DeviceNotFound Device is neither present nor responding 
 *                                   nor supported.
 * @retval NvError_InsufficientMemory Cannot allocate memory.
 */
typedef NvError 
(*pfNvDdkXxxBlockDevOpen)(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDevInstance);

/**
 * NvDdkXxxBlockDevDeinit()
 *
 * <a name="NvDdkXxxBlockDevDeinit">Releases</a> all global resources allocated.
 *
 * The required function prototype is as follows:
 *
 * @code   void NvDdkXxxBlockDevDeinit(void);   @endcode
 */
typedef void
(*pfNvDdkXxxBlockDevDeinit)(void);

/**
 * Structure for holding instance-specific block device interface pointers.
 */
typedef struct NvDdkBlockDevRec
{
    /**
     * Power down the device safely and release all the resources allocated.
     *
     * @param hBlockDev Handle acquired during the
     * <a href="#NvDdkXxxBlockDevOpen">NvDdkXxxBlockDevOpen()</a> call.
     *
     */
    void (*NvDdkBlockDevClose)(NvDdkBlockDevHandle hBlockDev);
    
    /**
     * Get the device information.
     * If device is hot pluggable, it must be called every time hot plug sema 
     * is signalled. 
     * If the device is present and initialized successfully, it returns
     * the valid number of total blocks.
     * If the device is not present or initialization failed, then it will
     * return total blocks as zero.
     *
     * @param hBlockDev Handle acquired during the
     * <a href="#NvDdkXxxBlockDevOpen">NvDdkXxxBlockDevOpen()</a> call.
     * @param pBlockDevInfo A pointer to the returned device information.
     *
     */
    void
    (*NvDdkBlockDevGetDeviceInfo)(
        NvDdkBlockDevHandle hBlockDev,
        NvDdkBlockDevInfo* pBlockDevInfo);
    
    /**
     * Registers a semaphore. 
     *
     * If the device is hot pluggable, this sema will be signaled on both removal
     * and insertion of device.
     *
     * @param hBlockDev Handle acquired during the
     * <a href="#NvDdkXxxBlockDevOpen">NvDdkXxxBlockDevOpen()</a> call.
     * @param hHotPlugSema Hanlde to semaphore.
     */
    void 
    (*NvDdkBlockDevRegisterHotplugSemaphore)(
        NvDdkBlockDevHandle hBlockDev, 
        NvOsSemaphoreHandle hHotPlugSema);
    
    /**
     * Reads data synchronously from the device.
     *
     * @pre The device must be initialized using
     * <a href="#NvDdkXxxBlockDevOpen">NvDdkXxxBlockDevOpen()</a> before
     * using this method.
     *
     * @param hBlockDev Handle acquired during the
     * <a href="#NvDdkXxxBlockDevOpen">NvDdkXxxBlockDevOpen()</a> call.
     * @param SectorNum Sector number to read the data from.
     * @param pBuffer A pointer to the buffer into which data will be read.
     * @param NumberOfSectors Number of sectors to read.
     *
     * @retval NvSuccess Read is successful.
     * @retval NvError_ReadFailed Read operation failed.
     * @retval NvError_InvalidBlock Sector's address does not exist in the
     *   device.
     */
    NvError 
    (*NvDdkBlockDevReadSector)(
        NvDdkBlockDevHandle hBlockDev, 
        NvU32 SectorNum, 
        void* const pBuffer, 
        NvU32 NumberOfSectors);
    
    /**
     * Writes data synchronously to the device.
     *
     * @pre The device must be initialized using
     * <a href="#NvDdkXxxBlockDevOpen">NvDdkXxxBlockDevOpen()</a> before
     * using this method.
     *
     * @param hBlockDev Handle acquired during the
     * <a href="#NvDdkXxxBlockDevOpen">NvDdkXxxBlockDevOpen()</a> call.
     * @param SectorNum Sector number to write the data to.
     * @param pBuffer A pointer to the buffer from which data will be written.
     * @param NumberOfSectors Number of sectors to write.
     *
     * @retval NvSuccess Write is successful.
     * @retval NvError_WriteFailed Write operation failed.
     * @retval NvError_InvalidBlock Sector's address doesn't exist in the
     *   device.
     */
    NvError 
    (*NvDdkBlockDevWriteSector)(
        NvDdkBlockDevHandle hBlockDev, 
        NvU32 SectorNum, 
        const void* pBuffer, 
        NvU32 NumberOfSectors);
    
    /**
     * Powers up the device. It could be taking out of low power mode or 
     * reinitializing.
     *
     * @pre The device must be initialized using
     *<a href="#NvDdkXxxBlockDevOpen">NvDdkXxxBlockDevOpen()</a> before
     * using this method.
     *
     * @param hBlockDev Handle acquired during the
     * <a href="#NvDdkXxxBlockDevOpen">NvDdkXxxBlockDevOpen()</a> call.
     */
    void (*NvDdkBlockDevPowerUp)(NvDdkBlockDevHandle hBlockDev);
    
    /**
     * Powers down the device. It could be low power mode or shutdown.
     *
     * @pre The device must be initialized using
     * <a href="#NvDdkXxxBlockDevOpen">NvDdkXxxBlockDevOpen()</a> before
     * using this method.
     *
     * @param hBlockDev Handle acquired during the
     * <a href="#NvDdkXxxBlockDevOpen">NvDdkXxxBlockDevOpen()</a> call.
     */
    void (*NvDdkBlockDevPowerDown)(NvDdkBlockDevHandle hBlockDev);

    /**
     * Flush buffered data out to the device.
     *
     * @pre The device needs to be initialized using
     * <a href="#NvDdkXxxBlockDevOpen">NvDdkXxxBlockDevOpen()</a> before
     * using this method.
     *
     * @param hBlockDev Handle acquired during the
     * <a href="#NvDdkXxxBlockDevOpen">NvDdkXxxBlockDevOpen()</a> call.
     */
    void (*NvDdkBlockDevFlushCache)(NvDdkBlockDevHandle hBlockDev);

    /**
     * NvDevIoctl()
     *
     * Perform an I/O control operation on the device.
     *
     * @param hBlockDev Handle acquired during the
     * <a href="#NvDdkXxxBlockDevOpen">NvDdkXxxBlockDevOpen()</a> call.
     * @param Opcode Type of control operation to perform.
     * @param InputSize Size of input arguments buffer, in bytes.
     * @param OutputSize Size of output arguments buffer, in bytes.
     * @param InputArgs A pointer to input arguments buffer.
     * @param OutputArgs A pointer to output arguments buffer.
     *
     * @retval NvError_Success IOCTL is successful.
     * @retval NvError_NotSupported \a Opcode is not recognized.
     * @retval NvError_InvalidSize \a InputSize or \a OutputSize is incorrect.
     * @retval NvError_InvalidParameter \a InputArgs or \a OutputArgs is
     *   incorrect.
     * @retval NvError_IoctlFailed IOCTL failed for other reason.
     */
    NvError
    (*NvDdkBlockDevIoctl)(
        NvDdkBlockDevHandle hBlockDev, 
        NvU32 Opcode,
        NvU32 InputSize,
        NvU32 OutputSize,
        const void *InputArgs,
        void *OutputArgs);

} NvDdkBlockDev;

#if defined(__cplusplus)
}
#endif  /* __cplusplus */
/** @} */

#endif // INCLUDED_NVDDK_BLOCKDEV_H


