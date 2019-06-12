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
 * @file nvEnhancedfilesystem.h
 *
 * NvEnhancedFileSystem is NVIDIA's Enhanced File System public interface.
 *
 */

#ifndef NVENHANCEDFS_H
#define NVENHANCEDFS_H

#include "nvfsmgr.h"
#include "nvddk_blockdev.h"
#include "nverror.h"
#include "nvfs.h"


#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Initializes Enhanced File system driver
 *
 * @retval NvSuccess Global resources have been allocated and initialized.
 * @retval NvError_DeviceNotFound File system is not present or not supported.
 * @retval NvError_InsufficientMemory Can't allocate memory.
 */
NvError
NvEnhancedFileSystemInit(void);

/**
 * Mounts Enhanced file system
 *
 * @param hPart handle for partition where file system is mounted
 * @param hDevice handle for device where partition is located
 * @param FileSystemAttr attribute value interpreted by driver
 *      It is ignored in Enhanced file system
 * @param phFileSystem address of Enhanced file system driver instance handle
 *
 * @retval NvError_Success No Error
 * @retval NvError_InvalidParameter Illegal parameter value
 * @NvError_InsufficientMemory Memory allocation failed
 */

NvError
NvEnhancedFileSystemMount(
    NvPartitionHandle hPart,
    NvDdkBlockDevHandle hDevice,
    NvFsMountInfo *pMountInfo,
    NvU32 FileSystemAttr,
    NvFileSystemHandle *phFileSystem);

/**
 * Shutdown Enhanced File System driver
 */
void
NvEnhancedFileSystemDeinit(void);

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef ENHANCEDFS_H */
