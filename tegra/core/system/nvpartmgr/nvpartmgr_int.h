/**
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file nvpartmgr.h
 *
 * Definitions for NVIDIA's partition manager.
 *
 */

#ifndef INCLUDED_NVPARTMGR_INT_H
#define INCLUDED_NVPARTMGR_INT_H

#include "nvcommon.h"
#include "nvassert.h"
#include "nvpartmgr_defs.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Magic number to identify partition table on storage media
 */
#define NV_PART_MAGIC_WO_COMPLEMENT 0x70617274UL // 'Part'
#define NV_PART_MAGIC ( ((NvU64)(NV_PART_MAGIC_WO_COMPLEMENT)<<32) | \
        (~((NvU64)(NV_PART_MAGIC_WO_COMPLEMENT))) )
#define NV_PART_VERSION 0x0100

#define NV_PART_AES_HASH_BLOCK_LEN    16
/*
 * Partition table image on storage device
 *
 * The partition table image that exists on the storage device consists of four
 * elements --
 * 
 * 1. the insecure header, NvPartTableHeaderInsecure, which is never signed or
 *    encrypted 
 * 2. the secure header, NvPartTableHeaderSecure, which is (optionally) signed 
 *    and/or encrypted
 * 3. an array of partition NvPartTableEntry entries, one entry per partition
 * 4. any trailing padding require by the signature and/or encryption algorithms
 *    used to secure the secure header and partition entry array
 *
 * The insecure header is used to identify the partition table image and read it
 * from the storage device.  The remainder of the image (i.e., the secure header
 * plus the array of partition entries) may optionally be signed and/or
 * encrypted.  The amount of data stored in signed and/or encrypted form is
 * equal to the InsecureLength minus sizeof(NvPartTableHeaderInsecure).
 *
 * SecureMagic, SecureVersion, and SecureLength are an important part of the
 * validation process. For valid images, each secure value (after possible
 * decryption) must exactly match the corresponding insecure value.  If any
 * mismatch occurs, the image is invalid.
 *
 * To help avoid memory alignment issues when addressing the
 * NvPartTableHeaderSecure and NvPartTableEntry structures, the insecure and
 * secure headers are both padded to a multiple of sizeof(NvU64) bytes.
 * Compile-time assertions verifies that these size constraint is always met.
 * When modifying the header structures, be sure to add explicit padding as
 * required to ensure continued conformance to the size constraints.
 */

/**
 * insecure header
 *
 * The following data is neither signed nor encrypted
 */
typedef struct NvPartTableHeaderInsecureRec
{
    /**
     * Magic number used to identify partition table on storage media
     */
    NvU64 Magic;

    /**
     * Partition table format version number
     */
    NvU32 InsecureVersion;

    /**
     * Length of partition data (in bytes) as stored on the storage device,
     * including the entire header, all partition table entries, and any
     * padding.  This value is never encrypted.
     */
    NvU32 InsecureLength;

    /**
     * Signature for secure header and all partition entries
     */
    NvU8 Signature[NV_PART_AES_HASH_BLOCK_LEN];

} NvPartTableHeaderInsecure;
    
/*
 * ensure insecure header is 8-byte aligned so that there will be no alignment
 * issues with any data structures that follow
 */
NV_CT_ASSERT( sizeof(NvPartTableHeaderInsecure) % sizeof(NvU64) == 0 );

/**
 * secure header
 *
 * The following data may be signed and/or encrypted, according to settings in
 * the insecure header
 */
typedef struct NvPartTableHeaderSecureRec
{
    /**
     * ************************************************************************
     * *** The following data may be encrypted and/or signed according to the
     * *** IsEncrypted and IsSigned settings.
     * ***
     * *** Encryption and/or signing covers all data from this point forward to
     * *** the end of the Partition Table image as it appears on the storage 
     * *** device.
     * ************************************************************************
     */
    
    /**
     * Random data used to randomize the first block of ciphertext that results
     * from encrypting this structure.  Serves a similar purpose as an
     * initialization vector.
     */
    NvU8 RandomData[16];

    /**
     * Magic number used to identify partition table on storage media
     */
    NvU64 SecureMagic;

    /**
     * Partition table format version number
     */
    NvU32 SecureVersion;
     
    /**
     * Length of partition data (in bytes) as stored on the storage device, including
     * the entire header, all partition table entries, and any padding.  This value
     * may be encrypted.
     */
    NvU32 SecureLength;
     
    /**
     * Number of partition table entries to follow
     */
    NvU32 NumPartitions;

    /**
     * Dummy bytes to ensure that structure size is a multiple of sizeof(NvU64)
     */
    NvU8 Padding1[4];

} NvPartTableHeaderSecure;

/*
 * ensure secure header is 8-byte aligned so that there will be no alignment
 * issues with any data structures that follow
 */
NV_CT_ASSERT( sizeof(NvPartTableHeaderSecure) % sizeof(NvU64) == 0 );

/*
 * Partition table entry
 */

typedef struct NvPartitionTableEntryRec
{
    /// partition id
    NvU32 PartitionId;
    /// partition name
    char PartitionName[NVPARTMGR_PARTITION_NAME_LENGTH];
    /// fstab-like info
    NvFsMountInfo MountInfo;
    /// MBR-like info
    NvPartInfo PartInfo;
} NvPartitionTableEntry;

/**
 * Partition table
 *
 * Note that any padding required by the crypto algoritms cannot be included
 * here since the number of elements in the TableEntry[] array is unknown at
 * compile time.  Padding will have to be added at runtime, after the array
 * size is known.
 *
 * The total size of the partition table image (as it exists on the storage
 * device) is as follows --
 *
 * total partition table image size = 
 *   sizeof(NvPartitionTable) 
 *   + sizeof(NvPartitionTableEntry[NvPartitionTable.SecureHeader.NumPartitions]) 
 *   + sizeof(crypto padding)
 */

typedef struct NvPartitionTableRec
{
    /// Insecure header
    NvPartTableHeaderInsecure InsecureHeader;
    /// Secure header
    NvPartTableHeaderSecure SecureHeader;
    /// Variable-length array of table entries
    NvPartitionTableEntry *TableEntry;
} NvPartitionTable;

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVPARTMGR_INT_H */
