/**
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
/**
 * @file nvexternalfilesystem.h
 *
 * NvExternalFileSystem is NULL External File System public interface.
 * This filesystem is used to test the external NV FS manager.
 *
 */

#ifndef NVEXTFS_H
#define NVEXTFS_H

#include "nverror.h"
#include "nvfs.h"
#include "nvddk_blockdev.h"


#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Initializes external file system driver
 *
 * @retval NvSuccess Global resources have been allocated and initialized.
 * @retval NvError_DeviceNotFound File system is not present or not supported.
 * @retval NvError_InsufficientMemory Can't allocate memory.
 */
NvError
NvExternalFileSystemInit(void);

/**
 * Mounts external file system
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
NvExternalFileSystemMount(
    NvPartitionHandle hPart,
    NvDdkBlockDevHandle hDevice,
    NvFsMountInfo *pMountInfo,
    NvU32 FileSystemAttr,
    NvFileSystemHandle *phFileSystem);

/**
 * Shutdown external file system driver
 */
void
NvExternalFileSystemDeinit(void);

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef NVEXTFS_H */
