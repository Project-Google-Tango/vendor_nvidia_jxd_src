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
 * <b>NVIDIA Partition Manager</b>
 *
 * Definitions for the NVIDIA partition manager.
 *
 */

#ifndef INCLUDED_NVPARTMGR_DEFS_H
#define INCLUDED_NVPARTMGR_DEFS_H

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup nvpartmgr_group_defs Partition Manager Definitions
 *
 * Declares definitions for the NVIDIA partition manager.
 *
 * @ingroup nvpartmgr_group
 * @{
 */

/**
 * File system attributes.
 */
#define NvPartitionAttr_WriteProtect 0x1

/**
 * String attributes.
 *
 * All strings are in UTF-8 format and have a maximum length, as
 * defined below.
 */
#ifdef NV_EMBEDDED_BUILD
#define NVPARTMGR_PARTITION_NAME_LENGTH 21
#else
#define NVPARTMGR_PARTITION_NAME_LENGTH 4
#endif
#define NVPARTMGR_MOUNTPATH_NAME_LENGTH (NVPARTMGR_PARTITION_NAME_LENGTH)

/**
 * Specifies Fstab-like information for a partition.
 *
 * This structure identifies the location of the partition, the file system type
 * for the partition, and the location (in the file system address space) where
 * the partition is to be mounted.
 *
 * The typical process for mounting a partition is as follows:
 *
 *    @note Information about the size and location of the partition itself must
 *    be queried from the partition manager.
 *    For an example, see NvPartMgrGetPartInfo().
 *
 * -# Open the device driver for the storage device containing the partition
 *    based on \a DeviceId, \a DeviceInstance, and \a DeviceAttr.
 * -# Open the file system driver for the partition based on \a FileSystemType
 *    and \a FileSystemAttr.
 * -# Register the \a MountPath in the global file system address space such
 *    that operations within the \a MountPath branch of the address space are
 *    directed to this instance of the file system driver.
 *
 * \a DeviceAttr and \a FileSystemAttr are both attributes associated with the
 * partition and are used during the mounting process above. They are used to
 * tailor the behavior of the device driver and file system driver on a
 * per-partition basis.
 */
typedef struct NvFsMountInfoRec
{
    /**
     * Holds the type of device on which partition is located.
     */
    NvU32 DeviceId;

    /**
     * Holds the device instance on which partition is located.
     */
    NvU32 DeviceInstance;

    /**
     * Holds the attribute passed to device driver when it is opened for
     * accessing data in the partition. The device driver
     * determines the meaning of this value.
     *
     * @warning This is a placeholder; it is not currently implemented or
     *          supported.
     */
    NvU32 DeviceAttr;

    /**
     * Holds the path where partition will be mounted in the file system.
     *
     * @note The string is in UTF-8 format.
     *
     * @warning The current implementation requires \a PartitionName and
     *          \a MountPath to be identical.
     */
    char MountPath[NVPARTMGR_MOUNTPATH_NAME_LENGTH];

    /**
     * Holds the type of file system to mount on this partition.
     */
    NvU32 FileSystemType;

    /**
     * Holds the attribute passed to file system driver when it is mounted
     * on the partition. The file system driver determines the meaning of this
     * value.
     */
    NvU32 FileSystemAttr;
} NvFsMountInfo;

// Temporary enum to describe the kind of Partition being created.
/*
 * @deprecated Do not use.
 */
typedef enum
{
    NvPartMgrPartitionType_Bct = 0x1,
    NvPartMgrPartitionType_Bootloader,
    NvPartMgrPartitionType_PartitionTable,
    NvPartMgrPartitionType_Os,
    NvPartMgrPartitionType_NvData,
    NvPartMgrPartitionType_Data,
    NvPartMgrPartitionType_Mbr,
    NvPartMgrPartitionType_Ebr,
    NvPartMgrPartitionType_GP1,
    NvPartMgrPartitionType_GPT,
    NvPartMgrPartitionType_BootloaderStage2,
    NvPartMgrPartitionType_FuseBypass,
    NvPartMgrPartitionType_ConfigTable,
    NvPartMgrPartitionType_WB0,
    NvPartMgrPartitionType_Force32 = 0x7FFFFFFF,
} NvPartMgrPartitionType;

/**
 * Provides MBR-like information for a partition.
 */
typedef struct NvPartInfoRec
{
    /**
     * Holds the partition attributes.
     *
     * @warning Not currently implemented or supported.
     */
    NvU32 PartitionAttr;
    /**
     * Dummy bytes added to avoid implicit padding done on the device side
     * for \a PartitionAttr based on max data type of structure. (Here NvU64).
     */
    NvU32 Padding1;
    /**
     * Holds the logical start address for the partition, in sectors (of the
     * device where the partition is located).
     */
    NvU64 StartLogicalSectorAddress;
    /**
     * Holds the logical size of the partition, in sectors (of the device
     * where the partition is located)
     */
    NvU64 NumLogicalSectors;
    /**
     * Holds the partition start physical sector on the storage media.
     */
    NvU64 StartPhysicalSectorAddress;
    /**
     * Holds the partition end physical sector on the storage media (inclusive).
     */
    NvU64 EndPhysicalSectorAddress;

    //This member is DEPRECATED. Will be removed soon.
    /**
     * @deprecated Do not use.
     */
    NvPartMgrPartitionType PartitionType;
#ifndef NV_EMBEDDED_BUILD
    /**
     * Dummy bytes added to avoid implicit padding done on the device side
     * for \a PartitionType based on max data type of structure. (Here NvU64)
     */
    NvU32 Padding2;
#else
    /**
     * Specifies whether the partition is write-protected on boot.
     */
    NvU32 IsWriteProtected;
#endif
} NvPartInfo;


/**
 * Specifies public partition information.
 */
typedef struct NvPartitionStatRec
{
   /** Holds the partition size in bytes. */
   NvU64 PartitionSize;
} NvPartitionStat;


/**
 * @} <!-- ends previous definition used to organize this material at top -->
 * @addtogroup nvpartmgr_group
 *
 * <h3>Partition Allocation</h3>
 *
 * Generally, a partition is specified in terms of a set of desired logical
 * properties. The underlying physical properties may differ from the logical
 * properties, for example to hide the presence of bad blocks on the storage
 * media.
 *
 * The allocation policy governs how the logical partition properties are mapped
 * to physical partition properties when the partition is first created on the
 * storage device.
 *
 * The supported allocation properties are --
 *
 * - Relative--The current partition is placed immediately after the
 * previously-defined partition on the storage device. If no previous partition
 * has been defined on the storage device, the partition is placed at physical
 * block zero.
 *
 * - Absolute--The current partition is placed at the specified physical
 * address on the storage device. The address must be aligned to block (erase)
 * boundaries on the storage device.
 *
 * For both policies, the client specifies the logical size of the partition in
 * bytes. If the storage device contains any bad blocks when the partition is
 * created, additional blocks wil be allocated to the partition such that there
 * will be enough good blocks to store the number of data bytes specified by the
 * client. These good blocks are referred to as logical blocks.
 *
 * To deal with the possibility that some good blocks may subsequently go bad in
 * the operation, the client can also specify that additional good blocks be
 * allocated to the partition. Depending on the device driver, it may be
 * possible to use these addition good blocks to replace blocks that go bad.
 * Thus, the additional blocks are referred to as <em>replacement blocks</em>.
 *
 * The client specifies the number of replacement blocks as a percentage of the
 * logical blocks. The calculation of replacement blocks is always rounded up
 * to ensure that the client never gets fewer replacement blocks
 * (percentage-wise) than requested.
 *
 * The calculations for logical blocks and replacement blocks are as follows:
 * <pre>
 *
 *   LB = ceiling(LogicalPartitionSize/BlockSize)
 *   RB = ceiling(LB * PartitionPercentReserved / 100)
 *
 * </pre>
 * Where RB is the number of reserved blocks and LB is the number of logical
 * blocks. The block driver for the storage device knows the device's
 * \a BlockSize and hence can carry out the calculations.
 *
 * Finally, there's a partition attribute that is passed to the device driver
 * when the partition is created. Interpretation of this attribute is
 * determined by the device driver. This provides a mechanism for associating
 * driver-specific properties with the partition on a per-partition basis.
 */

/**
 * Defines the alignment policy.
 */
typedef enum
{
    /// Partition starts at a specified physical address; address must be
    /// block-aligned.
    NvPartMgrAllocPolicyType_Absolute,
    /// Partition starts at the physical block address immediately following the
    /// end of the previous partition.
    NvPartMgrAllocPolicyType_Relative,

    NvPartMgrAllocPolicyType_Force32 = 0x7FFFFFFF
} NvPartMgrAllocPolicyType;

/**
 * Defines the allocation policy.
 */
typedef struct NvPartAllocInfoRec
{
    /**
     * The allocation policy governing the lay out of the partition on
     * the storage device.
     */
    NvPartMgrAllocPolicyType AllocPolicy;

    /**
     * Physical start address of partition on storage device, in bytes; must be
     * block aligned.
     *
     * @warning This parameter is ignored except when the \a AllocPolicy is
     *          \c NvPartMgrAllocPolicyType_Absolute.
     */
    NvU64 StartAddress;

    /**
     * Logical size of partition, in bytes.
     *
     * Partition is guaranteed to hold the specified amount of data when it is
     * created. Storage capacity of the partition may degrade over time if any
     * of its data blocks fail afterward.
     */
    NvU64 Size;

    /**
     * Number of reserved blocks to allocate in the partition, specified
     * as a percentage of the number of logical blocks in the partition.
     *
     * \a PartitionPercentReserved must be an integer in the range [0, 100].
     *
     * Calculations are rounded up to guarantee that the actual percentage of
     * reserved blocks allocated meets or exceeds the caller's requirements.
     */
    NvU32 PercentReserved;

    /**
     * Driver-dependent partition attribute.
     *
     * This value is passed to the relevant device driver when the partition is
     * created. Its meaning is determined by the device driver.
     */
    NvU32 AllocAttribute;

#ifdef NV_EMBEDDED_BUILD
    /**
     * A flag indicating whether the partition is write-protected.
     *
     * On boot, this attribute is checked and if it is TRUE then the primary
     * boot loader write protects the sectors corresponding to this partition.
     */
    NvU32 IsWriteProtected;
#endif
} NvPartAllocInfo;

/**
  * Defines the partition attributes.
  */
typedef enum
{
    /// Data that must be backed up and then restored.
    NvPartMgrPartitionAttribute_Preserve,

    /// Ignore -- Forces compilers to make 32-bit enums.
     NvPartMgrPartitionAttribute_Force32 = 0x7FFFFFFF
}NvPartMgrPartitionAttribute;

/** @} */

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVPARTMGR_DEFS_H */
