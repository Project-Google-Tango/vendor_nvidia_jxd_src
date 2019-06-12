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
 * @file nvfs.h
 * @brief <b> Nv File System Driver Interface.</b>
 *
 * @b Description: This file declares the interface for File System Drivers.
 */

#ifndef INCLUDED_NVFS_H
#define INCLUDED_NVFS_H

#include "nvcommon.h"
#include "nverror.h"
#include "nvfs_defs.h"
#include "nvddk_blockdev.h"
#include "nvfsmgr_defs.h"
#include "nvpartmgr_defs.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * NvFs is NVIDIA's interface for file system drivers.
 *
 * <b>Theory of operations</b>
 *
 * File system drivers are required to implement all of the API's defined
 * below.  Implementation must support multiple mounted instances of a given
 * file system, and multiple concurrent operations per instance.
 *
 * <b>Usage Model</b>
 *
 * The file system driver has two types of handles --
 * 1. one or more file system instance handles -- for local state/resources 
 *    per mounted instance of the file system
 * 2. one or more file handles (per instance handle) -- for state/resources
 *    per open file
 *
 * The global state must be initialized exactly once, via the
 * NvXxxFileSystemInit() routine.  One or more instances of the file system can
 * be mounted by calling NvXxxFileSystemMount() one or more times.
 *
 * Each mount call returns an instance handle, which is used to carry out all
 * further operations on the file system, as defined in the NvFileSystemRec
 * structure.  One such operation is to open a file, which is performed by
 * calling NvFileSystemFileOpen().  This routine returns a file handle.  The
 * same file handle must be used when invoking further operations on the file,
 * as defined in the NvFileSystemFileRec structure.
 *
 * After all operations have been completed for a given file, call
 * NvFileSystemFileClose() to close the file.  After all files have been closed
 * for a given instance of the file system, call NvFileSystemUnmount() to
 * unmount the instance and release its resources.  When all instances have been
 * closed, call NvXxxFileSystemDeinit() to release the global resources.
 *
 * <b>Implementation Details</b>
 *
 * File system drivers must implement the operations defined by the
 * NvFileSystemRec and NvFileSystemFileRec structures below. 
 *
 * The file system driver must also implement the following 
 * API's, where "Xxx" is the name of the specific file system type --
 *
 * * NvXxxFileSystemInit()
 * * NvXxxFileSystemMount()
 * * NvXxxFileSystemDeinit()
 *
 * The required prototypes for these functions are specified below.  File system
 * drivers must implement the actual functions, replacing "Xxx" with the name of
 * the specific file system type.
 */

/*
 * NvFileSystemHandle
 *
 * handle to a given instance of a mounted file system driver.
 */
typedef struct NvFileSystemRec *NvFileSystemHandle;

/*
 * NvFileSystemFileHandle
 *
 * handle to an open file on a mounted file system.
 */
typedef struct NvFileSystemFileRec *NvFileSystemFileHandle;
typedef struct NvPartInfoRec *NvPartitionHandle;

/**
 * NvXxxFileSystemInit()
 *
 * Initialize file system driver
 *
 * The required function prototype is as follows --
 *
 *    NvError NvXxxFileSystemInit(void);
 *
 * @retval NvSuccess Global resources have been allocated and initialized.
 * @retval NvError_DeviceNotFound File system is not present or not supported.
 * @retval NvError_InsufficientMemory Can't allocate memory.
 */
typedef NvError
(*pfNvXxxFileSystemInit)(void);

/**
 * NvXxxFileSystemMount()
 *
 * Mount a file system
 *
 * phFileSystem is the address of a handle to the file system driver.  The
 * driver must allocate its own context structure and return a handle
 * to it.  This handle will saved and passed in as an argument when
 * other file system functions are invoked.
 *
 * FileSystemAttr is an attribute value provided to the driver.  Interpretation
 * of this value is left up to the driver.
 *
 * The required function prototype is as follows --
 *
 *    NvError
 *    NvXxxFileSystemMount(
 *        NvPartitionHandle hPart,
 *        NvDdkBlockDevHandle hDevice,
 *        NvFsMountInfo *pMountInfo,
 *        NvU32 FileSystemAttr,
 *        NvFileSystemHandle *phFileSystem);
 *
 * @param hPart handle for partition where file system is mounted
 * @param hDevice handle for device where partition is located.
 * @param pMountInfo mount info required for the file system driver to mount
 * the file system.
 * @param FileSystemAttr attribute value interpreted by driver
 * @param phFileSystem address of driver instance handle
 *
 * @retval NvError_Success No Error
 * @retval NvError_InvalidParameter Illegal parameter value
 *
 * TODO -- document other error codes
 */
typedef NvError
(*pfNvXxxFileSystemMount)(
    NvPartitionHandle hPart,
    NvDdkBlockDevHandle hDevice,
    NvFsMountInfo *pMountInfo,
    NvU32 FileSystemAttr,
    NvFileSystemHandle *phFileSystem);

/**
 * NvXxxFileSystemDeinit()
 *
 * Shutdown a file system driver
 *
 * The required function prototype is as follows --
 *
 *    NvError NvXxxFileSystemDeinit(void);
 */
typedef void
(*pfNvXxxFileSystemDeinit)(void);

/**
 * Structure for holding File System Interface pointers.
 */
typedef struct NvFileSystemRec
{
    /**
     * NvFileSystemUnmount()
     *
     * Unmout a file system
     *
     * @param hFileSystem file system handle
     *
     * @retval NvError_Success No Error
     * @retval NvError_InvalidParameter Illegal parameter value
     * @retval NvError_AccessDenied One or more files open and FS called for
     *      unmount
     * TODO -- document other error codes
     */
    NvError
    (*NvFileSystemUnmount)(
        NvFileSystemHandle hFileSystem);
    
    /**
     * NvFileSystemFileOpen()
     *
     * Open a file
     *
     * @param hFileSystem file system handle
     * @param FilePath path to file, relative to file system mount point.
     * @param Flags specify access mode for the file
     * @param phFile pointer to a file handle
     *
     * @retval NvError_Success No Error
     * @retval NvError_InvalidParameter Illegal parameter value
     * @retval NvError_DeviceNotFound Device is neither present nor responding
     * @retval NvError_InsufficientMemory Can't allocate memory
     *
     * TODO -- document other error codes
     */
    NvError
    (*NvFileSystemFileOpen)(
        NvFileSystemHandle hFileSystem,
        const char *FilePath,
        NvU32 Flags,
        NvFileSystemFileHandle *phFile);
    
    /**
     * NvFileSystemFileQueryStat()
     *
     * Query information about a file given its name.
     *
     * @param hFileSystem file system handle
     * @param FilePath path to file, relative to file system mount point.
     * @param pStat buffer where file information will be placed
     *
     * @retval NvError_Success No Error
     * @retval NvError_InvalidParameter Illegal parameter value
     * @retval NvError_NotSupported Not supported
     */
    NvError
    (*NvFileSystemFileQueryStat)(
        NvFileSystemHandle hFileSystem,
        const char *FilePath,
        NvFileStat *pStat);
    
    /**
     * NvFileSystemFileSystemQueryStat()
     *
     * Query information about a filesystem given the name of any file in
     * the filesystem.
     *
     * @param hFileSystem file system handle
     * @param FilePath path to any file on file system, relative to file system
     *        mount point.
     * @param pStat buffer where filesystem information will be placed
     *
     * @retval NvError_Success No Error
     * @retval NvError_InvalidParameter Illegal parameter value
     * @retval NvError_NotSupported Not supported
     */
    NvError
    (*NvFileSystemFileSystemQueryStat)(
        NvFileSystemHandle hFileSystem,
        const char *FilePath,
        NvFileSystemStat *pStat);

    /**
     * NvFileSystemFormat()
     *
     * Perform file system format of the partition.
     *
     * @param hFileSystem file system handle
     * @param StartLogicalSector start logicalSector of partition to format
     * @param NumLogicalSectors number of logical Sectors in partition
     *
     * @retval NvError_Success No Error
     * @retval NvError_InvalidParameter Illegal parameter value
     * @retval NvError_NotSupported Not supported
     */
    NvError
    (*NvFileSystemFormat)(
        NvFileSystemHandle hFileSystem,
        NvU64 StartLogicalSector, NvU64 NumLogicalSectors,
        NvU32 PTendSector, NvBool IsPTpartition);

} NvFileSystem;

/**
 * Structure for holding File Interface pointers.
 */

typedef struct NvFileSystemFileRec
{
    /**
     * NvFileSystemFileClose()
     *
     * Close a file
     *
     * @param hFile file handle
     *
     * @retval NvError_Success No Error
     * @retval NvError_InvalidParameter Illegal parameter value
     *
     * TODO -- document other error codes
     */
    NvError
    (*NvFileSystemFileClose)(
        NvFileSystemFileHandle hFile);
    
    /**
     * NvFileSystemFileRead()
     *
     * Read data from a file into specified buffer
     *
     * @param hFile file handle
     * @param pBuffer pointer to buffer where read data is to be placed
     * @param BytesToRead number of bytes of data to read
     * @param BytesRead pointer to number of bytes actually read
     *
     * @retval NvError_Success No Error
     * @retval NvError_InvalidParameter Illegal parameter value
     * @retval NvError_InvalidSize Read size request is more than file size
     * @        or it is more than allocated size in partition
     * @retval NvError_FileReadFailed encountered device read error
     *
     * TODO -- document other error codes
     */

    NvError
    (*NvFileSystemFileRead)(
        NvFileSystemFileHandle hFile,
        void *pBuffer,
        NvU32 BytesToRead,
        NvU32 *BytesRead);
    
    /**
     * NvFileSystemFileWrite()
     *
     * Write data to a file from specified buffer
     *
     * @param hFile file handle
     * @param pBuffer pointer to buffer containing write data
     * @param BytesToWrite number of bytes of data to write
     * @param BytesWrite pointer to number of bytes actually written
     *
     * @retval NvError_Success No Error
     * @retval NvError_InvalidParameter Illegal parameter value
     * @retval NvError_InvalidSize Write size request is more than allocated
     * @        size in partition
     * @retval NvError_FileWriteFailed encountered device write error
     *
     * TODO -- document other error codes
     */
    NvError
    (*NvFileSystemFileWrite)(
        NvFileSystemFileHandle hFile,
        const void *pBuffer,
        NvU32 BytesToWrite,
        NvU32 *BytesWritten);

    /**
     * NvFileSystemFileSeek()
     *
     * Set File Position
     * (NOTE: Only supported for files opened in read access mode) 
     *
     * @param hFile file handle
     * @param ByteOffset offset from the file position specified by Whence
     * @param Whence specifices the origin position for seek operations (see
     * nvos.h)
     * 
     * @retval NvError_Success No Error
     * @retval NvError_InvalidParameter Illegal parameter value
     * 
     * TODO -- document other error codes
     */
    NvError
    (*NvFileSystemFileSeek)(
        NvFileSystemFileHandle hFile,
        NvS64 ByteOffset,
        NvU32 Whence);

    /**
     * NvFileSystemFtell()
     *
     * Return the current file offset from the beginning of the file
     *
     * @param hFile file handle
     * @param ByteOffset location for returning the file offset in bytes
     *
     * @retval NvError_Success No Error
     * @retval NvError_InvalidParameter Illegal parameter value
     *
     * TODO -- document other error codes
     */
    NvError
    (*NvFileSystemFtell)(
        NvFileSystemFileHandle hFile,
        NvU64 *ByteOffset);
   
    /**
     * NvFileSystemFileIoctl()
     *
     * Perform an I/O Control operation on the file.
     *
     * @param hFile file handle
     * @param IoctlType specifies the type of control operation to perform
     * @param InputSize size of input arguments buffer, in bytes
     * @param OutputSize size of output arguments buffer, in bytes
     * @param InputArgs pointer to input arguments buffer
     * @param OutputArgs pointer to output arguments buffer
     *
     * @retval NvError_Success No Error
     * @retval NvError_InvalidParameter Illegal parameter value
     * @retval NvError_NotSupported Not supported
     */
    NvError
    (*NvFileSystemFileIoctl)(
         NvFileSystemFileHandle hFile,
         NvFileSystemIoctlType IoctlType,
         NvU32 InputSize,
         NvU32 OutputSize,
         const void * InputArgs,
         void *OutputArgs);
    
    /**
     * NvFileSystemFileQueryFstat()
     *
     * Query information about a file given its handle.
     *
     * @param hFile file handle
     * @param pStat buffer where file information will be placed
     *
     * @retval NvError_Success No Error
     * @retval NvError_InvalidParameter Illegal parameter value
     * @retval NvError_NotSupported Not supported
     */
    NvError
    (*NvFileSystemFileQueryFstat)(
        NvFileSystemFileHandle hFile,
        NvFileStat *pStat);

} NvFileSystemFile;


#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVFS_H */
