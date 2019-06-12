/*
 * Copyright (c) 2008-2012 NVIDIA Corporation.  All rights reserved.
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
 * @b Description: Accesses information about data
 *                 partitions on storage devices.
 */

/**
 * @defgroup nvpartmgr_group Partition Manager Interface
 *
 * The NVIDIA<sup>&reg;</sup> interface for accessing information
 * about data partitions on storage devices.
 *
 * A single physical storage device may be treated as multiple smaller logical
 * volumes. Each such logical volume is referred to as a partition. The
 * Partition Manager maintains information on the size, location, and other
 * attributes of the various partitions that exist on the system. Collectively,
 * this information is referred to as the Partition Table (PT).
 * @note There is exactly one PT on the system and it contains information on
 * all the partitions on all the non-removable storage devices on the system.
 *
 * <h3>Supported Operations</h3>
 *
 * The Partition Manager provides support for two major operations.
 * - Creating a partition table and writing it out to a storage device. This
 * happens only once, during system initialization. Thereafer the PT is never
 * modified for the remainder of the lifetime of the device.
 * - Reading a pre-existing partition table from a storage device and querying
 * it for information about the various partitions. This is the normal use case
 * and it happens at least once during each boot cycle.
 *
 * <h4>To create a partition table</h4>
 *
 * -# A client calls NvPartMgrInit() to initialize the Partition Manager.
 * -# Then NvPartMgrCreateTableStart() is invoked to signal that definition of
 *    the various partitions is ready to begin.
 * -# Next, the partitions are defined one-by-one using the
 *    NvPartMgrAddTableEntry() routine.
 * -# Once all the partitions have been defined, NvPartMgrCreateTableFinish() is
 *    called to signal the end of the partition table.
 * -# Finally, the PT is written to a storage device via NvPartMgrSaveTable()
 *    and the Partition Manager can be shut down by calling NvPartMgrDeinit().
 *
 * <h4>To read and query a partition table</h4>
 *
 * -# The Partition Manager is initialized via NvPartMgrInit().
 * -# The pre-existing PT is loaded from a storage device using
 *    NvPartMgrLoadTable().
 * -# Thereafter, information from the PT can be accessed via the
 *    NvPartMgrGet*() and NvPartMgrQuery*() APIs.
 * -# When the PT information is no longer needed, call NvPartMgrDeinit() to
 *    shut down the Partition Manager.
 *
 * @note The partition ID value of zero (0) is reserved. Each partition
 * must have a unique ID and a unique name. All strings are in UTF-8 format and
 * have a maximum length of \c NVPARTMGR_PARTITION_NAME_LENGTH.
 * @{
 */

#ifndef INCLUDED_NVPARTMGR_H
#define INCLUDED_NVPARTMGR_H

#include "nvcommon.h"
#include "nverror.h"
#include "nvpartmgr_defs.h"
#include "nvddk_blockdevmgr_defs.h"


#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Initializes the partition manager.
 *
 * @retval NvError_Success No error.
 */
NvError
NvPartMgrInit(void);

/**
 * Shuts down the partition manager.
 */
void
NvPartMgrDeinit(void);

/**
 * Reads Partition Table (PT) from specified storage device. Up to
 * \a NumLogicalSectors are searched in an effort to find a valid PT.
 *
 * @param DeviceId Storage device identifier.
 * @param DeviceInstance The instance of the storage device.
 * @param StartLogicalSector First sector to read when searching for PT.
 * @param NumLogicalSectors Maximum logical sectors to read when searching for
 *        PT.
 * @param IsSigned If NV_TRUE, check signature on partition table image loaded
 *        from storage device; if NV_FALSE, don't check signature.
 * @param IsEncrypted If NV_TRUE, decrypt partition table image loaded from
 *        storage device; if NV_FALSE, don't perform decryption.
 *
 * @retval NvSuccess Partition Table load is successful.
 * @retval NvError_ReadFailed Partition Table load operation failed.
 */
NvError
NvPartMgrLoadTable(
    NvDdkBlockDevMgrDeviceId DeviceId,
    NvU32 DeviceInstance,
    NvU32 StartLogicalSector,
    NvU32 NumLogicalSectors,
    NvBool IsSigned,
    NvBool IsEncrypted);

/**
 * Unloads the partition table like deinit of a partition table.
 * This is required for \c --create in resume mode.
 */
void
NvPartMgrUnLoadTable(void);
/**
 * Indicates that creation of a new Partition Table is ready to begin.
 *
 * @param NumPartitions The number of partitions to be added.
 *
 * @retval NvError_Success No error.
 * @retval NvError_BadParameter Invalid value for \a NumPartitions.
 * @retval NvError_InsufficientMemory No free memory available.
 */
NvError
NvPartMgrCreateTableStart(NvU32 NumPartitions);

/**
 * Indicates that creation of a new Partition Table has been completed.
 *
 * @retval NvError_Success No error.
 */
NvError
NvPartMgrCreateTableFinish(void);

/**
 * Creates a new partition and adds an entry to the Partition Table.
 *
 * See ::NvPartAllocInfo for details about the partition
 * allocation process. Details related to mounting a file system on the
 * partition can be found in ::NvFsMountInfo.
 *
 * @warning Current implementation requires \a PartitionName and
 *          \a MountPath (from NvFsMountInfo) to be identical. To ensure this
 *          constraint is met, the value of \a MountPath will be overridden and
 *          forced to the value of \a PartitionName.
 *
 * @param PartitionId ID of partition to add.
 * @param PartitionName Name of the partition.
 * @param PartitionType Type of the partition.
 * @param PartitionAttr The partition attribute.
 * @param pAllocInfo A pointer to information specifying how to allocate the
 *        partition.
 * @param pMountInfo A pointer to information specifying how to mount a file
 *        system on the partition.
 *
 * @retval NvSuccess Partition creation is successful.
 */
NvError
NvPartMgrAddTableEntry(
    NvU32 PartitionId,
    char PartitionName[NVPARTMGR_PARTITION_NAME_LENGTH],
    NvPartMgrPartitionType PartitionType,
    NvU32 PartitionAttr,
    NvPartAllocInfo *pAllocInfo,
    NvFsMountInfo *pMountInfo);

/**
 * Writes the Partition Table to the storage device. The caller specifies the
 * partition in which the Partition Table is to be stored. Multiple copies of
 * the Partition Table will be written, as many as will fit in the partition.
 *
 * The Partition Table can optionally be signed and/or encrypted with the Secure
 * Storage Key (SSK) before being written to the storage device.
 *
 * @param PartitionId ID of partition where Partition Table is to be stored.
 * @param SignTable NV_TRUE if Partition Table is to be signed, else NV_FALSE.
 * @param EncryptTable NV_TRUE if Partition Table is to be encrypted, else
 *        NV_FALSE.
 *
 * @retval NvSuccess Partition Table save is successful.
 * @retval NvError_BadParameter The function cannot find the partition ID.
 * @retval NvError_InsufficientMemory There is inadequate memory for the
 * operation.
 */
NvError
NvPartMgrSaveTable(
    NvU32 PartitionId,
    NvBool SignTable,
    NvBool EncryptTable);

/**
 * Gets the partition ID for the specified partition.
 *
 * @param[in] PartitionName Name of the partition.
 * @param[out] PartitionId Pointer to the partition's ID.
 *
 * @retval NvSuccess Partition name lookup is successful.
 * @retval NvError_BadParameter Partition name not found.
 */
NvError
NvPartMgrGetIdByName(
    const char *PartitionName,
    NvU32 *PartitionId);

/**
 * Gets the partition name for the specified partition ID.
 *
 * @param[in] PartitionId Partition ID to look up.
 * @param[out] PartitionName Name of the corresponding partition.
 *
 * @retval NvSuccess Partition ID lookup is successful.
 * @retval NvError_BadParameter Partition ID not found.
 */
NvError
NvPartMgrGetNameById(
    NvU32 PartitionId,
    char *PartitionName);

/**
 * Gets information about the file system to be mounted on the partition.
 *
 * @param[in] PartitionId Partition ID to look up.
 * @param[out] pFsMountInfo A pointer to File System information for specified
 *        partition.
 *
 * @retval NvSuccess File system information retrieved successfully.
 * @retval NvError_BadParameter Partition ID not found.
 */
NvError
NvPartMgrGetFsInfo(
    NvU32 PartitionId,
    NvFsMountInfo *pFsMountInfo);

/**
 * Get information about the location, size, and other attributes for the
 * specified partition.
 *
 * @param[in] PartitionId Partition ID to look up.
 * @param[out] pPartInfo A pointer to location/size information for specified
 *        partition.
 *
 * @retval NvSuccess Partition information retrieved successfully.
 * @retval NvError_BadParameter Partition ID not found.
 */
NvError
NvPartMgrGetPartInfo(
    NvU32 PartitionId,
    NvPartInfo *pPartInfo);

/**
 * Iterates through the partition IDs in the Partition Table.
 * Returns the ID of the partition that
 * follows the partition with the specified ID.
 *
 * Pass in a partition ID of zero to obtain the ID of the first
 * partition in the Partition Table. To walk through all partition ID's, call
 * \c NvPartMgrGetNextId multiple times, each time passing in the output from
 * the previous invocation.
 *
 * A value of zero will be returned with either:
 * - The end of the Partition Table is encountered, or
 * - The specified partition ID wasn't found.
 *
 * @param[in] PartitionId Partition ID for current partition. Specify zero to look
 * up ID of first partition in Partition Table.
 *
 * @returns The partition ID for the following partition; zero means current partition ID
 *         isn't found.
 */
NvU32
NvPartMgrGetNextId(
    NvU32 PartitionId);

/**
 * Gets the device identifier and device instance for the next device
 * in the global device list.
 * If both the @c DeviceId and @c DeviceInstance parameters are zero,
 * then the NvPartMgrGetNextId() function returns the device identifier
 * and instance for the first device in the list.
 * If the current device is not found in the list, then this function
 * returns the device identifier and instance for the first device in the list.
 *
 * @param[in] DeviceId The storage device identifier for the the current device.
 * @param[in] DeviceInstance The device instance for the current device.
 * @param[out] NextDeviceId  The next device identifier.
 * @param[out] NextDeviceInstance The next device instance.
 *
 * @retval NvSuccess Partition information retrieved successfully.
 * @retval NvError_BadValue Partition ID not found.
 *
 */
NvU32
NvPartMgrGetNextDeviceInstance(
        NvU32 DeviceId,
        NvU32 DeviceInstance,
        NvU32 *NextDeviceId,
        NvU32 *NextDeviceInstance);

/**
 * Gets the partition ID of the next partition on the specified device.
 *
 * @param[in] DeviceId The device ID.
 * @param[in] DeviceInstance The device instance.
 * @param[in] PartitionId The current partition ID on the specified device.
 *
 * @return The next partition ID after the @a PartitionId on the device;
 * or, if @a PartitionId is 0, the first partition on the device;
 * or 0 otherwise.
 */
NvU32
NvPartMgrGetNextIdForDeviceInstance(
    NvU32 DeviceId,
    NvU32 DeviceInstance,
    NvU32 PartitionId);

/**
 * Gets the number of partitions in the table.
 * @return The number of partitions in the table.
 */
NvU32 NvPartMgrGetNumPartition(void);

#if defined(__cplusplus)
}
#endif

/** @} */

#endif /* #ifndef INCLUDED_NVPARTMGR_H */
