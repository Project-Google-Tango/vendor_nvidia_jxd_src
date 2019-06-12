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
 * @file nvfs_defs.h
 *
 * Definitions for NVIDIA's file system drivers.
 *
 */
#ifndef INCLUDED_NVSTORMGR_INT_H
#define INCLUDED_NVSTORMGR_INT_H

#include "nvpartmgr_defs.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct NvStorMgrFileRec
{
    /** partition handle */
    NvPartitionHandle hPart;
    /** virtual device driver handle */
    NvDdkBlockDevHandle hDevice;
    /** virtual file system driver handle */
    NvFileSystemHandle hFileSystem;
    /** file handle */
    NvFileSystemFileHandle hFile;
} NvStorMgrFile;

typedef struct NvStorMgrPartFSListRec
{
    NvStorMgrFile StorMgrFile;
    NvU32 PartitionID;
    NvU32 RefCount;
    struct NvStorMgrPartFSListRec* pNext;
}NvStorMgrPartFSList;


#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVFS_DEFS_H */

