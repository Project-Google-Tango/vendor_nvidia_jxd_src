/**
 * Copyright (c) 2008-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file nvfsmgr_pub.h
 *
 * Public definitions for NVIDIA's file system driver interface.
 *
 */

#ifndef INCLUDED_NVFSMGR_DEFS_H
#define INCLUDED_NVFSMGR_DEFS_H

#include "nvcommon.h"
#include "nvfs_defs.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define NV_PARTITION_NAME_LENGTH    4

/*
 * NvFsMgrFileSystemType
 *
 * Enumerated list of file system types.
 */
typedef enum
{
    /* NvFs Internal File System Range: 1 - 0x3FFFFFFF */
    NvFsMgrFileSystemType_None = 0, /* Illegal value  */
    NvFsMgrFileSystemType_Basic,    /* Basic filesystem */
#ifdef NV_EMBEDDED_BUILD
    NvFsMgrFileSystemType_Enhanced, /* Enhanced filesystem */
    NvFsMgrFileSystemType_Ext2,     /* Ext2 filesystem */
    NvFsMgrFileSystemType_Yaffs2,   /* Yaffs2 filesystem */
    NvFsMgrFileSystemType_Ext3,     /* Ext3 filesystem */
    NvFsMgrFileSystemType_Ext4,     /* Ext4 filesystem */
    NvFsMgrFileSystemType_Qnx,      /* QNX filesystem */
#endif
    NvFsMgrFileSystemType_Num,  /* Total number of internal filesystems */

    /* External File System Range: 0x40000000 - 0x7FFFFFFFF */
    NvFsMgrFileSystemType_External = 0x40000000,

    NvFsMgrFileSystemType_Force32 = 0x7FFFFFFF
} NvFsMgrFileSystemType;

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVFSMGR_DEFS_H */
