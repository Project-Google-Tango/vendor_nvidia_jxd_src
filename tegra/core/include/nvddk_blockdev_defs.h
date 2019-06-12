/*
 * Copyright (c) 2008-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file
 * <b> NVIDIA Driver Development Kit: Block Device Interface Definitions</b>
 *
 * @b Description: This file defines the public data structures for block
 *                 devices.
 */

#ifndef INCLUDED_NVDDK_BLOCKDEV_DEFS_H
#define INCLUDED_NVDDK_BLOCKDEV_DEFS_H

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup nvbdk_ddk_blkdevintdefs Block Device Interface Definitions
 *
 * This defines the public data structures for block devices.
 *
 * @ingroup nvbdk_ddk_blkdevmulti
 * @{
 */

#define NVDDK_BLOCKDEV_PART_MGMT_POLICY_REMAIN_SIZE 0x800

/**
 * Defines device types.
 */
typedef enum
{
    /// Device is fixed and cannot be removed from system.
    NvDdkBlockDevDeviceType_Fixed = 1,
    /// Device is hot pluggable, can be removed or inserted at any time.
    NvDdkBlockDevDeviceType_Removable,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvDdkBlockDevDeviceType_Force32 = 0x7FFFFFFF
} NvDdkBlockDevDeviceType;

/**
 * Structure to hold block device information.
 */
typedef struct NvDdkBlockDevInfoRec
{
    /// Holds the size of the sector (read/write unit size) for the device.
    NvU32 BytesPerSector;
    /// Holds the size of the block (erase unit size) for the device.
    NvU32 SectorsPerBlock;
    /// Holds the total number of blocks on the device.
    /// A block is a group of sectors and is
    /// the smallest device area that the device driver can reference.
    NvU32 TotalBlocks;
    NvU32 TotalSectors;
    /// Holds the device type, fixed or removable.
    NvDdkBlockDevDeviceType DeviceType;
    /// Holds the private data
    void *Private;
} NvDdkBlockDevInfo;

/**
 * Defines I/O control types.
 */
typedef enum
{
    /**
     * Read the physical sector.
     *
     * @par Inputs:
     * ::NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs
     *
     * @par Outputs:
     * None.
     */
    NvDdkBlockDevIoctlType_ReadPhysicalSector = 1,

    /**
     * Write physical sector.
     *
     * @par Inputs:
     * ::NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs
     *
     * @par Outputs:
     * None.
     */
    NvDdkBlockDevIoctlType_WritePhysicalSector,

    /**
     * Query status for specified physical block.
     *
     * @par Inputs:
     * ::NvDdkBlockDevIoctl_QueryPhysicalBlockStatusInputArgs
     *
     * @par Outputs:
     * ::NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs
     */
    NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus,

    /**
     * Erase a sector, optionally verifying whether it is still good.
     *
     * @par Inputs:
     * ::NvDdkBlockDevIoctl_ErasePhysicalBlockInputArgs
     *
     * @par Outputs:
     * None.
     *
     * @retval NvError_Success Erase is successful.
     * @retval NvError_ResourceError Erase operation failed.
     */
    NvDdkBlockDevIoctlType_ErasePhysicalBlock,

    /**
     * Lock (write protect) block device region.
     *
     * @par Inputs:
     * ::NvDdkBlockDevIoctl_LockRegionInputArgs
     *
     * @par Outputs:
     * None.
     */
    NvDdkBlockDevIoctlType_LockRegion,

    /**
     * Configure the type of write protection.
     *
     * @par Inputs:
     * ::NvDdkBlockDevIoctl_ConfigureWriteProtectionInputArgs
     *
     * @par Outputs:
     * None.
     */
    NvDdkBlockDevIoctlType_ConfigureWriteProtection,

    /**
     * Map logical sector address to corresponding physical sector address.
     *
     * @par Inputs:
     * ::NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs
     *
     * @par Outputs:
     * ::NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs
     */
    NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector,

    /**
     * Low-level format device (exact functionality is device specific).
     *
     * @par Inputs:
     * ::NvDdkBlockDevIoctl_FormatDeviceInputArgs
     *
     * @par Outputs:
     * None.
     * @retval NvError_Success Format is successful.
     * @retval NvError_ResourceError Format operation failed.
     */
    NvDdkBlockDevIoctlType_FormatDevice,

    /**
     * Start/finish allocating partitions on the device.
     *
     * @par Inputs:
     * ::NvDdkBlockDevIoctl_PartitionOperationInputArgs
     *
     * @par Outputs:
     * None.
     */
    NvDdkBlockDevIoctlType_PartitionOperation,

    /**
     * Allocate a partition on the device.
     *
     * @par Inputs:
     * ::NvDdkBlockDevIoctl_AllocatePartitionInputArgs
     *
     * @par Outputs:
     * ::NvDdkBlockDevIoctl_AllocatePartitionOutputArgs
     */
    NvDdkBlockDevIoctlType_AllocatePartition,

    /**
     * Enable or disable read verify write operations.
     *
     * @par Inputs:
     * ::NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgs
     *
     * @par Outputs:
     * None.
     */
    NvDdkBlockDevIoctlType_WriteVerifyModeSelect,

    /**
     * Erase logical sectors.
     *
     * @par Inputs:
     * ::NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs
     *
     * @par Outputs:
     * None.
     */
    NvDdkBlockDevIoctlType_EraseLogicalSectors,

     /**
     * Disable cache.
     *
     * @par Inputs:
     * None
     *
     * @par Outputs:
     * None.
     */
    NvDdkBlockDevIoctlType_DisableCache,
    /**
     * Verifies critical NvFlash partitions viz. region table, etc. post NvFlash.
     *
     * @par Inputs:
     *
     *
     * @par Outputs:
     * None.
     */
    NvDdkBlockDevIoctlType_VerifyCriticalPartitions,

    /**
     * Get partition phyisical sectors.
     *
     * @par Inputs:
     * ::NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsInputArgsRec
     *
     * @par Outputs:
     * ::NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsOutputArgsRec
     */
    NvDdkBlockDevIoctlType_GetPartitionPhysicalSectors,

    //FIXME: The functionality of this IOCTL needs
    //to implemented in a more elegant manner.
    //Ideally, you'd have this functionality in the
    //externally managed partition APIs.
    NvDdkBlockDevIoctlType_GetPartitionPhysicalStartBlock,

    /**
     * Erases the entire partition.
     * In the case of NAND, depending on partition management policy
     * the factory and runtime bad blocks are not erased. Interpretation
     * of runtime bad block depends on partition management policy.
     *
     * @par Inputs:
     * ::NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs
     *
     * @par Outputs:
     * None.
     */
    NvDdkBlockDevIoctlType_ErasePartition,

    /**
     * Unprotects all the sectors on the device.
     *
     * @par Inputs:
     * None.
     *
     * @par Outputs:
     * None.
     */
    NvDdkBlockDevIoctlType_UnprotectAllSectors,

    /**
     * Protects the sectors against write/erases.
     *
     * @par Inputs:
     * ::NvDdkBlockDevIoctl_ProtectSectorsInputArgsRec
     *
     * @par Outputs:
     * None.
     */
    NvDdkBlockDevIoctlType_ProtectSectors,

    /**
     * Updates the device firmware
     *
     * @par Inputs:
     * ::NvDdkBlockDevIoctl_UpdateDeviceFirmwareInputArgsRec
     *
     * @par Outputs:
     * None.
     */
    NvDdkBlockDevIoctlType_UpgradeDeviceFirmware,

    NvDdkBlockDevIoctlType_Num,
    /**
     * Ignore -- Forces compilers to make 32-bit enums.
     */
    NvDdkBlockDevIoctlType_Force32 = 0x7FFFFFFF
} NvDdkBlockDevIoctlType;

/** @name Input/Output Structures for IOCTL Types
 *
*/
/*@{*/

/**
 * Read physical sector IOCTL input arguments.
 *
 * @retval NvError_Success Read is successful.
 * @retval NvError_ReadFailed Read operation failed.
 * @retval NvError_InvalidBlock Sector address does not exist in the device.
 */
typedef struct NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgsRec
{
    /// Sector number at which to begin reading.
    NvU32 SectorNum;
    /// Number of the sectors to read.
    NvU32 NumberOfSectors;
    /// Pointer to buffer that data will be read into.
    void *pBuffer;
} NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs;

/**
 * Write physical sector IOCTL input arguments.
 *
 * @retval NvError_Success Write is successful.
 * @retval NvError_WriteFailed Write operation failed.
 * @retval NvError_InvalidBlock Sector address does not exist in the device.
 */
typedef struct NvDdkBlockDevIoctl_WritePhysicalSectorInputArgsRec
{
    /// Sector number at which to begin writing.
    NvU32 SectorNum;
    /// Number of the sectors to write.
    NvU32 NumberOfSectors;
    /// Pointer to buffer containing data to be written.
    const void *pBuffer;
} NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs;

/**
 * Query physical block status IOCTL input arguments.
 *
 * Query good/bad status and write-protect status for specified physical block.
 *
 * @retval NvError_Success Query is successful.
 * @retval NvError_InvalidBlock Block address does not exist in the device.
 */
typedef struct NvDdkBlockDevIoctl_QueryPhysicalBlockStatusInputArgsRec
{
    /// Number of the sector to query.
    NvU32 BlockNum;
} NvDdkBlockDevIoctl_QueryPhysicalBlockStatusInputArgs;

/// Query physical block status IOCTL output arguments.
typedef struct NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgsRec
{
    /// NV_TRUE if block is good, NV_FALSE if block is bad.
    NvBool IsGoodBlock;
    /// NV_TRUE if block is write-protected, NV_FALSE if not write-protected.
    NvBool IsLockedBlock;
} NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs;

/**
 * Erase physical block IOCTL input arguments.
 *
 * @retval NvError_Success Erase is successful.
 * @retval NvError_WriteFailed Erase operation failed.
 * @retval NvError_InvalidBlock Block address does not exist in the device.
 */
typedef struct NvDdkBlockDevIoctl_ErasePhysicalBlockInputArgsRec
{
    /// Block number at which be begin erasing.
    NvU32 BlockNum;
    /// Number of the blocks to erase.
    NvU32 NumberOfBlocks;
    /**
     * Boolean flag to indicate if secure erase operation needs to be done.
     * This is valid for eMMC only. If this flag is enabled along with the
     * IsTrimErase flag then secure trim erase is performed.
     * Here is the example usage
     * Unsecure erase: IsSecureErase = NV_FALSE.
     * Secure erase: IsSecureErase = NV_TRUE and IsTrimErase = NV_FALSE.
     * Secure trim erase: IsSecureErase = NV_TRUE and IsTrimErase = NV_TRUE.
     */
    NvBool IsSecureErase;
    /**
     * Boolean flag to indicate if trim erase operation needs to be done.
     * This is valid for eMMC only. If this flag is enabled along with the
     * IsSecureErase flag then secure trim erase is performed.
     * Here is the example usage
     * Non trim erase: IsTrimErase = NV_FALSE.
     * Trim erase: IsTrimErase = NV_TRUE and IsSecureErase = NV_FALSE.
     * Secure trim erase: IsTrimErase = NV_TRUE and IsSecureErase = NV_TRUE.
     */
    NvBool IsTrimErase;
} NvDdkBlockDevIoctl_ErasePhysicalBlockInputArgs;

/**
 * Lock region IOCTL input arguments.
 *
 * This IOCTL is used to define block device write-protected regions.
 * The IOCTL may be called multiple times to setup several distinct lock regions.
 *
 * Set \a EnableLocks to NV_TRUE in the last IOCTL call to enable all defined lock
 * regions. Once enabled, lock regions cannot be disabled without resetting the
 * block device.
 *
 * Whenever possible hardware-based write protection will be used.
 *
 * @retval NvError_Success Lock operation succeeded.
 * @retval NvError_InvalidAddress Sector address does not exist in the device.
 * @retval NvError_AlreadyAllocated Hardware locking resources are all allocated.
 */
typedef struct NvDdkBlockDevIoctl_LockRegionInputArgsRec
{
    /// Logical sector address of the begining of the lock region.
    /// A LogicalSectorStart address of 0xFFFFFFFF is ignored.
    NvU32 LogicalSectorStart;
    /// Number of the logical sectors in the lock region.
    NvU32 NumberOfSectors;
    /// A value of NV_TRUE enables all defined lock regions.
    NvBool EnableLocks;
} NvDdkBlockDevIoctl_LockRegionInputArgs;

/**
 * Map logical to physical sector IOCTL input arguments.
 *
 * Map logical sector address to corresponding physical sector address.
 *
 * This routine can also be used to convert a logical block address to
 * a physical block address, using the NvDdkBlockDevInfoRec::SectorsPerBlock
 * information, which can be obtained by calling
 * NvDdkBlockDevGetDeviceInfo().
 *
 * The equations are as follows:
 * <pre>
 *     Logical sector address = Logical block address * SectorsPerBlock
 *
 *     Physical sector address = Physical block address * SectorsPerBlock    </pre>
 * @retval NvError_Success Address translation is successful.
 * @retval NvError_InvalidBlock Logical sector address does not exist in the
 *         device.
 */
typedef struct NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgsRec
{
    /// Logical sector address.
    NvU32 LogicalSectorNum;
} NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs;

/// Map logical to physical sector IOCTL output arguments.
typedef struct NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgsRec
{
    /// Physical sector address.
    NvU32 PhysicalSectorNum;
} NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs;

/**
 * Partition operation IOCTL.
 *
 * Notify driver that partition allocation is ready to commence, or has been
 * completed.
 *
 * The \a Start operation must be specified before the first partition is allocated
 * (via the ::NvDdkBlockDevPartitionAllocationType IOCTL). Then the \a Finish
 * operation must be specified after the last partition has been allocated.
 *
 * @retval NvError_Success Partition layout start/finish is successful.
 * @retval NvError_InvalidState Illegal sequencing of start/finish operations.
 * @retval NvError_AlreadyAllocated Partitions already allocated.
 * @retval NvError_BadValue Illegal Operation code.
 */
typedef enum
{
    /**
     * Prepare to allocate partitions.
     * Must be called once, before allocating first partition.
     */
    NvDdkBlockDevPartitionOperation_Start,
    /**
     * Finished allocating partitions.
     * Must be called once, after all partitions have been allocated.
     */
    NvDdkBlockDevPartitionOperation_Finish,

    /**
     * Ignore -- Forces compilers to make 32-bit enums.
     */
    NvDdkBlockDevPartitionOperation_Force32 = 0x7FFFFFFF
} NvDdkBlockDevPartitionOperation;

/// Partition operation IOCTL input arguments.
typedef struct NvDdkBlockDevIoctl_PartitionOperationInputArgsRec
{
    /// Partition allocation type.
    NvDdkBlockDevPartitionOperation Operation;
} NvDdkBlockDevIoctl_PartitionOperationInputArgs;

/**
 * Allocate a partition on the device.
 *
 * For devices that present logical addresses to their clients, this routine
 * provides the information they must define and initialize the
 * logical-to-physical mapping parameters for a given partition.
 *
 * This information is provided only once during the lifetime of the device,
 * i.e., at system initialization time. Thereafter, the driver must maintain
 * this mapping information internally.
 *
 * @note A \a Start operation must be performed via the ::NvDdkBlockDevPartitionOperation
 * IOCTL before the first partition is allocated. Similarly, the \a Finish
 * operation must be specified via the ::NvDdkBlockDevPartitionOperation IOCTL
 * after the last partition has been allocated.
 *
 * Partition placement can be relative or absolute. If relative, the partition
 * is placed immediately after the previously-defined partition (or starting at
 * the beginning of the device, if it's the first partition to be defined). If
 * absolute, then \a StartPhysicalSectorAddress specifies the address at which the
 * partition must begin. An error is reported if the absolute state address
 * overlaps an existing partition or if it fails to meet any required alignment
 * constraints.
 *
 * The caller specifies the number of good sectors that the partition must
 * contain.  The driver must allocate enough physical sectors to yield the
 * specified number of good sectors. An error is reported if there are not
 * enough good sectors available.
 *
 * As \a StartPhysicalSectorAddress specifies the address at which the partition
 * must begin, after the ::NvDdkBlockDevPartitionAllocationType IOCTL is successful,
 * this IOCTL returns the \a StartLogicalSectorAddress where this partition had been
 * created and the number of logical sectors that have been allocated to
 * this partition. Clients will be able to directly access the logical sectors in
 * the partition; thus, each logical sectors is assigned an address.
 *
 * The client also specifies the number of reserved blocks for the partition.
 * Clients cannot access reserved blocks directly; hence, they are not assigned
 * logical addresses. If a logical block is found to go bad during use, it is
 * marked as bad and retired. If there are any remaining reserved blocks, one
 * is promoted to a logical block and assigned the address of the failed block.
 * Thus, unless/until the supply of reserved blocks in the partition becomes
 * exhausted, the number of logical blocks (and, hence, the number of logical
 * sectors) in the partition remains constant.
 *
 * The client specifies the number of reserved blocks as a percentage of the
 * number of logical blocks. The driver must round up its internal calculation
 * of the number of reserved blocks to ensure that the specified percentage is
 * met or exceeded.
 *
 * The driver must ensure that the requested logical sectors are aligned to
 * erase unit (block) boundaries. The reserved blocks must, of course, also be
 * block aligned. The overriding requirement is that erase operations must be
 * entirely independent among partitions, i.e., an erase of any sector in a
 * given partition cannot have the side effect of erasing any sectors in any
 * another partition. Also, erase operations must be entirely independent
 * between logical sectors and reserved sectors within any given partition,
 * i.e., erasing any logical sector within a partition cannot have the side
 * effect of erasing any reserved sector within that partition, and vice-versa.
 *
 * Finally, the caller can pass in a generic partition attribute. Each driver
 * is free to specify the meaning of this attribute.
 *
 * @retval NvError_Success Partition layout is successful.
 * @retval NvError_InvalidAddress Partition start address is unaligned.
 * @retval NvError_AlreadyAllocated Partition overlaps a previously-defined
 *         partition.
 * @retval NvError_InvalidSize Partition does not fit on device.
 * @retval NvError_BadValue Partition attribute value is invalid.
 */
typedef enum
{
    /// Partition starts at a specified physical block address.
    NvDdkBlockDevPartitionAllocationType_Absolute,
    /// Partition starts at the physical block address immediately following the
    /// previous partition.
    NvDdkBlockDevPartitionAllocationType_Relative,

    /// Ignore -- Forces compilers to make 32-bit enums.
    NvDdkBlockDevPartitionAllocationType_Force32 = 0x7FFFFFFF
} NvDdkBlockDevPartitionAllocationType;

/// Allocate partition IOCTL input arguments.
typedef struct NvDdkBlockDevIoctl_AllocatePartitionInputArgsRec
{
    /// Partition ID.
    NvU32 PartitionId;
    /// Partition allocation type.
    NvDdkBlockDevPartitionAllocationType AllocationType;
    /// Physical start address of partition; ignored for relative partitions
    NvU32 StartPhysicalSectorAddress;
    /// Number of logical sectors in the partition.
    NvU32 NumLogicalSectors;
    /// Number of reserved blocks in the partition, expressed as a percentage
    /// of the number of logical blocks in the partition; must be in the range
    /// [0, 100].
    NvU32 PercentReservedBlocks;
    /// Partition attribute; meaning is interpreted by the device driver.
    NvU32 PartitionAttribute;
    /// Partition type to represent BCT, GPT, etc.
    NvU32 PartitionType;
    /// Read needs to be sequentially done for partitions having this flag as true
    /// e.g. bootloader and bct partitions are read sequentially by bootrom
    NvBool IsSequencedReadNeeded;
} NvDdkBlockDevIoctl_AllocatePartitionInputArgs;

/// Allocate partition IOCTL output arguments.
typedef struct NvDdkBlockDevIoctl_AllocatePartitionOutputArgsRec
{
    /// Logical start address of partition.
    NvU32 StartLogicalSectorAddress;
    /// Number of logical sectors that have been allocated in the partition.
    NvU32 NumLogicalSectors;
    /// Physical Start start address of the partition
    NvU32 StartPhysicalSectorAddress;
    /// Number of Physical sectors that have been allocated in the partition.
    NvU32 NumPhysicalSectors;
} NvDdkBlockDevIoctl_AllocatePartitionOutputArgs;


/// IOCTL to enable/disable read verification after write.
typedef struct
NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgsRec
{
    /// Flag to indicate enable/disable read verify write operation.
    /// NV_TRUE indicates write operations would be verified by read operation.
    /// NV_FALSE indicates there is no verification of the write operation.
    NvBool IsReadVerifyWriteEnabled;
} NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgs;

/**
 * Format device IOCTL. Performs low-level device format.
 *
 * @retval NvError_Success Format operation successful.
 * @retval NvError_NandOperationFailed format operation failed.
 */
typedef enum
{
    /**
     * Erase all good blocks on device/partition. Bad blocks detected
     * during device operation (run-time) and blocks marked bad at the
     * factory are retained.
     */
    NvDdkBlockDevEraseType_GoodBlocks,
    /**
     * Erase all good and run-time bad blocks. Factory bad blocks are
     * retained.
     */
    NvDdkBlockDevEraseType_NonFactoryBadBlocks,
    /**
     * Erase good, run-time, and factory bad blocks.
     */
    NvDdkBlockDevEraseType_AllBlocks,
    /**
     * Erase all good and run-time bad blocks, then exhaustively retest
     * the device to redetect run-time bad blocks.
     */
    NvDdkBlockDevEraseType_VerifiedGoodBlocks,

    /**
     * Ignore -- Forces compilers to make 32-bit enums.
     */
    NvDdkBlockDevEraseType_Force32 = 0x7FFFFFFF
} NvDdkBlockDevEraseType;

/// Format device IOCTL input arguments.
typedef struct NvDdkBlockDevIoctl_FormatDeviceInputArgsRec
{
    /// Device format type.
    NvDdkBlockDevEraseType EraseType;
} NvDdkBlockDevIoctl_FormatDeviceInputArgs;

/**
 * Erase logical sectors IOCTL input arguments.
 *
 * @retval NvError_Success erase sectors successful.
 * @retval NvError_BadValue Invalid input argument(s).
 */
typedef struct NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgsRec
{
    /// Start logical sector address (inclusive).
    NvU32 StartLogicalSector;
    /// Number of logical sectors to erase. If 0xFFFFFFFF erase all
    /// logical sectors from \a StartLogicalSector to the end of the device.
    NvU32 NumberOfLogicalSectors;
    /// PT partition end sector address used to check if PT would get erased
    /// if sectors before this partition are erased.
    /// All partitions erase the sectors before them.
    /// PT partition when formatted additionally erases unaligned sectors
    /// after it.
    NvU32 PTendSector;
    /// Boolean flag to indicate partition is PT partition
    // Format of PT partition on SD erases both previous and next partition
    // blocks. Note SD does not have partition specific management policy
    NvBool IsPTpartition;
    /**
     * Boolean flag to indicate if secure erase operation needs to be done.
     * This is valid for eMMC only. If this flag is enabled along with the
     * IsTrimErase flag then secure trim erase is performed.
     * Here is the example usage
     * Unsecure erase: IsSecureErase = NV_FALSE.
     * Secure erase: IsSecureErase = NV_TRUE and IsTrimErase = NV_FALSE.
     * Secure trim erase: IsSecureErase = NV_TRUE and IsTrimErase = NV_TRUE.
     */
    NvBool IsSecureErase;
    /**
     * Boolean flag to indicate if trim erase operation needs to be done.
     * This is valid for eMMC only. If this flag is enabled along with the
     * IsSecureErase flag then secure trim erase is performed.
     * Here is the example usage
     * Non trim erase: IsTrimErase = NV_FALSE.
     * Trim erase: IsTrimErase = NV_TRUE and IsSecureErase = NV_FALSE.
     * Secure trim erase: IsTrimErase = NV_TRUE and IsSecureErase = NV_TRUE.
     */
    NvBool IsTrimErase;
} NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs;

/**
 * Get partition physical sectors IOCTL input arguments.
 *
 * Return the range of physical sectors encompassing the currently
 * opened partition.
 *
 * @retval NvError_Success Query partition physical sectors IOCTL successful.
 * @retval NvError_BadValue Invalid input argument(s).
 */
typedef struct NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsInputArgsRec
{
    /// Partition start logical sector (inclusive).
    NvU32  PartitionLogicalSectorStart;
    /// Partition stop logical sector (inclusive).
    NvU32  PartitionLogicalSectorStop;
} NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsInputArgs;

/// Get partition physical sectors IOCTL output arguments.
typedef struct NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsOutputArgsRec
{
    /// Partition start physical sector (inclusive).
    NvU32   PartitionPhysicalSectorStart;
    /// Partition stop physical sector (inclusive).
    NvU32   PartitionPhysicalSectorStop;
} NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsOutputArgs;

/// Define the type of locking to be used.
typedef enum
{
    /// Permanent write protection locks a block until this feature is
    /// disabled. Neither clear command nor HW reset would disable this
    /// protection.
    NvDdkBlockDevWriteProtectionType_Permanent,
    /// Power ON write protection locks a block until a HW reset or a power
    /// failure occurs.
    NvDdkBlockDevWriteProtectionType_PowerON,
    /// Temporary write protection locks a block and can be disabled using the
    /// clear command.
    NvDdkBlockDevWriteProtectionType_Temporary,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvDdkBlockDevWriteProtectionType_Force32 = 0x7FFFFFFF
}NvDdkBlockDevWriteProtectionType;

/// Define access regions in eMMC.
typedef enum
{
    /// User region of the eMMC card.
    NvDdkBlockDevAccessRegion_UserArea = 0,
    /// First boot partition region of the eMMC card.
    NvDdkBlockDevAccessRegion_BootPartition1,
    /// Second boot partition region of the eMMC card.
    NvDdkBlockDevAccessRegion_BootPartition2,

    NvDdkBlockDevAccessRegion_Unknown,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvDdkBlockDevAccessRegion_Force32 = 0x7FFFFFFF
}NvDdkBlockDevAccessRegion;

/**
 * Configure write protection IOCTL input arguments.
 *
 * This IOCTL is used to define types of write-protection.
 *
 * @retval NvError_Success Configuration succeeded.
  * @retval NvError_BadParameter Incorrect parameter passed.
 */
typedef struct NvDdkBlockDevIoctl_ConfigureWriteProtectionInputArgsRec
{
    /// The type of write protection to be configured.
    NvDdkBlockDevWriteProtectionType WriteProtectionType;
    /// A value of NV_TRUE enables the requested write protection.
    NvBool Enable;
    /// For eMMC, specify the card region for which the requested write
    /// protection configuration is applied.
    NvDdkBlockDevAccessRegion Region;
} NvDdkBlockDevIoctl_ConfigureWriteProtectionInputArgs;

/**
 * Protect Sectors IOCTL input arguments.
 *
 * @retval NvError_Success erase sectors successful.
 * @retval NvError_BadValue Invalid input argument(s).
 */
typedef struct NvDdkBlockDevIoctl_ProtectSectorsInputArgsRec
{
    /// Physical Start start address of the partition
    NvU32 StartPhysicalSectorAddress;
    /// Number of Physical sectors that have been allocated in the partition.
    NvU32 NumPhysicalSectors;
    /// Indicates whether this is the last partition that is write protected
    NvBool IsLastPartitionToProtect;
} NvDdkBlockDevIoctl_ProtectSectorsInputArgs;

/*@}*/

/**
 * Device Fimrware upgrade IOCTL input arguments.
 */
typedef struct NvDdkBlockDevIoctl_DeviceFirmwareUpgradeInputArgsRec
{
    /// Pointer to the data, used in firmware upgrade
    void* pData;
} NvDdkBlockDevIoctl_DeviceFirmwareUpgradeInputArgs;

/*@}*/

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

/** @} */
#endif // INCLUDED_NVDDK_BLOCKDEV_DEFS_H

