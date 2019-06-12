/**
 * Copyright (c) 2008-2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
/**
 * @file nvext2filesystem.h
 *
 * NvBasicFileSystem is NVIDIA's Basic File System public interface.
 *
 */

#ifndef EXT2FS_H
#define EXT2FS_H

#include "nvfsmgr.h"
#include "nvddk_blockdev.h"
#include "nverror.h"
#include "nvfs.h"


#if defined(__cplusplus)
extern "C"
{
#endif

#define NV_EXT2_NDIR_BLOCKS 12
#define NV_EXT2_IND_BLOCK   (NV_EXT2_NDIR_BLOCKS)
#define NV_EXT2_DIND_BLOCK  (NV_EXT2_IND_BLOCK + 1)
#define NV_EXT2_TIND_BLOCK  (NV_EXT2_DIND_BLOCK + 1)
#define NV_EXT2_N_BLOCKS    (NV_EXT2_TIND_BLOCK + 1)

typedef struct NvExt2SuperBlockRec
{
    NvU32 InodeCount;
    NvU32 BlockCount;
    NvU32 ReservedBlockCount;
    NvU32 FreeBlockCount;
    NvU32 FreeInodeCount;
    NvU32 FirstDataBlock;
    NvU32 BlockSizeLog2;
    NvS32 FragSizeLog2;
    NvU32 BlocksPerGroup;
    NvU32 FragsPerGroup;
    NvU32 InodesPerGroup;
    NvU32 MountTime;
    NvU32 WriteTime;
    NvU16 MountCount;
    NvS16 MaxMountCount;
    NvU16 Magic;
    NvU16 State;
    NvU16 Errors;
    NvU16 MinorRevLevel;
    NvU32 LastCheck;
    NvU32 CheckInterval;
    NvU32 CreatorOs;
    NvU32 RevLevel;
    NvU16 DefaultUid;
    NvU16 DefaultGid;
    NvU32 FirstInodeNumber;
    NvU16 InodeSize;
    NvU16 BlockGroupNumber;
    NvU32 CompatibilityFeatureSet;
    NvU32 InCompatibilityFeatureSet;
    NvU32 RoCompatibilityFeatureSet;
    NvU32 Pad2[30];
    NvU32 JournalInodeNum;
    NvU32 Pad3[199];
} NvExt2SuperBlock;

typedef struct NvExt2GroupDescRec
{
    NvU32 BlockBitmapBlock;
    NvU32 InodeBitmapBlock;
    NvU32 InodeTableBlock;
    NvU16 FreeBlockCount;
    NvU16 FreeInodeCount;
    NvU16 UsedDirectoryCount;
    NvU16 Pad[7];
} NvExt2GroupDesc;

typedef struct NvExt2InodeRec
{
    NvU16 Mode;
    NvU16 Uid;
    NvU32 Size;
    NvU32 AccessTime;
    NvU32 CreateTime;
    NvU32 ModifyTime;
    NvU32 DeleteTime;
    NvU16 Gid;
    NvU16 LinkCount;
    NvU32 NumBlocks;
    NvU32 Flags;
    NvU32 Reserved;
    NvU32 BlockTable[NV_EXT2_N_BLOCKS];
    NvU32 Version;
    NvU32 FileAcl;
    NvU32 DirectoryAcl;
    NvU32 FragAddress;
    NvU8  FragCount;
    NvU8  FragSize;
    NvU16 Pad[5];
} NvExt2Inode;

typedef struct NvExt2DirectoryRec
{
    NvU32 Inode;
    NvU16 Size;
    NvU16 NameLength;
    NvU8  Name[1];
} NvExt2Directory;

typedef struct JournalHeaderRec
{
    NvU32 Magic;
    NvU32 BlockType;
    NvU32 Sequence;
} JournalHeader;

typedef struct NvExt2JournalSuperBlockRec
{
    JournalHeader Header;
    NvU32 BlockSize;                    /* journal device blocksize */
    NvU32 TotalBlocksInJournFile;       /* total blocks in journal file */
    NvU32 FirstBlock;                   /* first block of log information */
    NvU32 FirstCommitId;                /* first commit ID expected in log */
    NvU32 LogStartBlock;                /* blocknr of start of log */
    NvU32 ErrNo;                        /* Error value, as set by journal_abort(). */
    NvU32 CompatibleFeatureSet;         /* compatible feature set */
    NvU32 InCompatibleFeatureSet;       /* incompatible feature set */
    NvU32 RoCompatibleFeatureSet;       /* readonly-compatible feature set */
    NvU8  Uuid[16];                     /* 128-bit uuid for journal */
    NvU32 NumOfUsers;                   /* Nr of filesystems sharing log */
    NvU32 SuperBlockCopyBlockNum;       /* Blocknr of dynamic superblock copy*/
    NvU32 MaxJournalBlocksPerTrans;     /* Limit of journal blocks per trans.*/
    NvU32 MaxDataBlocksPerTrans;        /* Limit of data blocks per trans. */
    NvU32 Pad[44];
    NvU8  Users[16*48];                 /* ids of all fs'es sharing the log */
} NvExt2JournalSuperBlock;

#define NV_EXT2_SUPERBLOCK_OFFSET 1024
#define NV_EXT2_MAGIC 0xEF53
#define NV_EXT2_ROOT_INODE_INDEX 2
#define NV_EXT2_FIRST_INODE_INDEX 11
#define NV_EXT2_INODE_BLOCKSIZE 512
#define NV_EXT2_SUPERBLOCK_UUID_OFFSET 0x68
#define NV_EXT2_SUPERBLOCK_UUID_LENGTH 16
#define NV_EXT2_JOURNAL_INODE_INDEX  8
#define NV_EXT2_FEATURE_COMPAT_HAS_JOURNAL  0x0004
#define NV_EXT2_FEATURE_INCOMPAT_FILETYPE   0x0002
#define NV_EXT2_FEATURE_INCOMPAT_EXTENTS   0x0040
#define NV_EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER 0x0001
#define NV_EXT2_FEATURE_RO_COMPAT_LARGE_FILE 0x0002
#define NV_EXT2_JSB_HEADER_MAGIC_NUMBER 0xc03b3998U
#define NV_EXT2_JSB_SUPERBLOCK_V1   3
#define NV_EXT2_JSB_SUPERBLOCK_V2   4


/**
 * Initializes Ext2 File system driver
 *
 * @retval NvSuccess Global resources have been allocated and initialized.
 * @retval NvError_DeviceNotFound File system is not present or not supported.
 * @retval NvError_InsufficientMemory Can't allocate memory.
 */
NvError
NvExt2FileSystemInit(void);

/**
 * Mounts Ext2 file system
 *
 * @param hPart handle for partition where file system is mounted
 * @param hDevice handle for device where partition is located
 * @param FileSystemAttr attribute value interpreted by driver
 *      It is ignored in basic file system
 * @param phFileSystem address of Basic file system driver instance handle
 *
 * @retval NvError_Success No Error
 * @retval NvError_InvalidParameter Illegal parameter value
 * @NvError_InsufficientMemory Memory allocation failed
 */

NvError
NvExt2FileSystemMount(
    NvPartitionHandle hPart,
    NvDdkBlockDevHandle hDevice,
    NvFsMountInfo *pMountInfo,
    NvU32 FileSystemAttr,
    NvFileSystemHandle *phFileSystem);

/**
 * Shutdown Ext2 File System driver
 */
void
NvExt2FileSystemDeinit(void);

/**
 * Formats a partition with the Ext2 file system. Partitions do
 * not need to be erased prior to calling this function. 
 *
 * @note Bad blocks are not handled by this function, so it is strongly
 * recommended that this only be used for mass storage devices that
 * internally manage bad blocks, such as MMC.
 *
 * @param hDev Block handle used for format operations
 * @param NvPartitionName Name of the partition to format.
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError NvExt2PrivFormatPartition(
    NvDdkBlockDevHandle hDev,
    const char    *NvPartitionName);

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef EXT2FS_H */
