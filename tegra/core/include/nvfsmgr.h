/**
 * Copyright (c) 2008-2009 NVIDIA Corporation.  All rights reserved.
 *`
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file nvfsmgr.h
 *
 * NvFsMgr is NVIDIA's interface for accessing file system drivers.
 *
 * To initialize the File System Manager, the client calls NvFsMgrInit() exactly
 * once before any other operations can be performed.
 *
 * Once the File System Manager is initialized, the client can obtain a handle
 * to a file system driver by calling NvFsMgrFileSystemMount() and specifying
 * the desired file system type.
 *
 * At this point, the client can perform operations on the mounted file system
 * via the NvFs interface.  When finished performing operations on a given
 * mounted file system, close it by calling NvFileSystemUnmount() through the
 * NvFs interface.
 *
 * After all file system drivers have been unmounted, call NvFsMgrDeinit() to
 * shut down the File System Manager.
 *
 */

#ifndef INCLUDED_NVFSMGR_H
#define INCLUDED_NVFSMGR_H

#include "nvcommon.h"
#include "nverror.h"
#include "nvfs.h"
#include "nvfsmgr_defs.h"
#include "nvpartmgr_defs.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * NvFsMgrInit()
 *
 * Initialize the filesystem manager.
 *
 * @retval NvError_Success No Error
 * @retval NvError_NotInitialized Failed to initialize
 */
NvError
NvFsMgrInit(void);

/**
 * NvFsMgrDeinit()
 *
 * Shutdown the filesystem manager.
 */
void
NvFsMgrDeinit(void);

/**
 * NvFsMgrFileSystemMount()
 *
 * Mount a file system
 *
 * phFs is the address of a handle to the file system driver.  Subsequent file
 * system operations can be invoked through the handle; see documentation on the
 * File System interface for full details.
 *
 * FileSystemAttr is an attribute value provided to the file system.
 * Interpretation of this value is left up to the file system.
 *
 * @param FsType file system type
 * @param hPart handle for partition where file system is mounted
 * @param hDevice handle for device where partition is located
 * @param FsMountInfo the filesystem mount info
 * @param FileSystemAttr attribute value interpreted by driver
 * @param phFs address of driver instance handle
 *
 * @retval NvError_Success No Error
 * @retval NvError_InvalidParameter Illegal parameter value
 *
 * TODO -- document other error codes
 */
NvError
NvFsMgrFileSystemMount(
    NvFsMgrFileSystemType FsType,
    NvPartitionHandle hPart,
    NvDdkBlockDevHandle hDevice,
    NvFsMountInfo *FsMountInfo,
    NvU32 FileSystemAttr,
    NvFileSystemHandle *phFs);

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVFSMGR_H */
