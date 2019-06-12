/**
 * Copyright (c) 2008-2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
/**
 * @file nvbasicfilesystem.h
 *
 * NvBasicFileSystem is NVIDIA's Basic File System public interface.
 *
 */

#ifndef NVBASICFS_H
#define NVBASICFS_H

#include "nvfsmgr.h"
#include "nvddk_blockdev.h"
#include "nverror.h"
#include "nvfs.h"


#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Initializes Basic File system driver
 *
 * @retval NvSuccess Global resources have been allocated and initialized.
 * @retval NvError_DeviceNotFound File system is not present or not supported.
 * @retval NvError_InsufficientMemory Can't allocate memory.
 */
NvError
NvBasicFileSystemInit(void);

/**
 * Mounts basic file system
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
NvBasicFileSystemMount(
    NvPartitionHandle hPart,
    NvDdkBlockDevHandle hDevice,
    NvFsMountInfo *pMountInfo,
    NvU32 FileSystemAttr,
    NvFileSystemHandle *phFileSystem);

/**
 * Shutdown Basic File System driver
 */
void
NvBasicFileSystemDeinit(void);

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef BASICFS_H */
