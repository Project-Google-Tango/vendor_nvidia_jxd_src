/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_nvstormgr_H
#define INCLUDED_nvstormgr_H


#if defined(__cplusplus)
extern "C"
{
#endif


/** @file
 * @brief <b>NVIDIA File System Manager</b>
 *
 * @b Description: Handles data storage outside the normal
 *    operating system's file system.
 */

/**
 * @defgroup nv_stormgr_group File System Manager
 *
 * The NVIDIA simple file system manager, which handles data storage outside
 * the normal OS's file system.
 *
 * <h4>To use the file system manager</h4>
 *
 * -# First invoke NvStorMgrInit() to initialized the File System Manager.
 * -# Then perform one of several possible queries, perform a partition
 * format operation to prepare the storage media for file system operations,
 * or open a file.
 *
 * The query operations are:
 * - NvStorMgrPartitionQueryStat
 * - NvStorMgrFileSystemQueryStat
 * - NvStorMgrFileQueryStat
 *
 * <h4>To perform a low-level format</h4>
 * - Call NvStorMgrFormat().
 *
 * <h4>To open a file</h4>
 *
 * -# Call the NvStorMgrFileOpen() function.
 * -# Store the resulting file handle, \a hFile.
 *
 * It will be required to perform other operations on the file.
 *
 * Now a variety of file operations can be performed:
 * - NvStorMgrFileRead
 * - NvStorMgrFileWrite
 * - NvStorMgrFileSeek
 * - NvStorMgrFtell
 * - NvStorMgrFileIoctl
 * - NvStorMgrFileQueryFstat
 *
 * <h4>To close a file</h4>
 * - When all operations on the file have been completed, close the file via
 * NvStorMgrFileClose().
 *
 * <h4>To shut down the filesystem manager</h4>
 * - When no further file system manager operations are needed, close the
 * manager by invoking NvStorMgrDeinit().
 *
 * @{
 */
#include "nvcommon.h"
#include "nverror.h"
#include "nvpartmgr.h"
#include "nvfsmgr.h"
#include "nvfs.h"

/// Forward declaration.

typedef struct NvStorMgrFileRec *NvStorMgrFileHandle;

/*
 * Functions
 */

/**
 * Initializes the filesystem manager.
 */

 NvError NvStorMgrInit(
    void  );

/**
 * Shuts down the filesystem manager.
 */

 void NvStorMgrDeinit(
    void  );

/**
 * Opens a file.
 *
 * @param FileName Specify the name of the file to open.
 * @param Flags Specify the access mode for the file.
 * @param phFile A pointer to a file handle.
 *
 * @retval NvError_Success No error.
 * @retval NvError_InvalidParameter Illegal parameter value.
 */

 NvError NvStorMgrFileOpen(
    const char * FileName,
    NvU32 Flags,
    NvStorMgrFileHandle * phFile );

/**
 * Closes a file.
 *
 * @param hFile Handle to the file.
 *
 * @retval NvError_Success No error.
 * @retval NvError_InvalidParameter Illegal parameter value.
 */

 NvError NvStorMgrFileClose(
    NvStorMgrFileHandle hFile );

/**
 * Reads data from a file into the specified buffer.
 *
 * @param hFile Handle to the file.
 * @param pBuffer A pointer to buffer where read data is to be placed.
 * @param BytesToRead The number of bytes of data to read.
 * @param BytesRead A pointer to number of bytes actually read.
 *
 * @retval NvError_Success No error.
 * @retval NvError_InvalidParameter Illegal parameter value.
 */

 NvError NvStorMgrFileRead(
    NvStorMgrFileHandle hFile,
    void* pBuffer,
    NvU32 BytesToRead,
    NvU32 * BytesRead );

/**
 * Writes data to a file from specified buffer.
 *
 * @param hFile Handle to the file.
 * @param pBuffer A pointer to buffer containing write data.
 * @param BytesToWrite The number of bytes of data to write.
 * @param BytesWritten A pointer to number of bytes actually written
 *
 * @retval NvError_Success No error.
 * @retval NvError_InvalidParameter Illegal parameter value.
 */

 NvError NvStorMgrFileWrite(
    NvStorMgrFileHandle hFile,
    void* pBuffer,
    NvU32 BytesToWrite,
    NvU32 * BytesWritten );

/**
 * Sets file position.
 * @note Only supported for file streams opened in read access mode.
 *
 * @param hFile Handle to the file.
 * @param ByteOffset The offset from the file position specified by \a Whence.
 * @param Whence Specifices the file seek origin (see nvfs_defs.h).
 *
 * @retval NvError_Success No error.
 * @retval NvError_InvalidParameter Illegal parameter value.
 */

 NvError NvStorMgrFileSeek(
    NvStorMgrFileHandle hFile,
    NvS64 ByteOffset,
    NvU32 Whence );

/**
 * Returns the current file offset from the beginning of the file.
 *
 * @param hFile Handle to the file.
 * @param ByteOffset Location for returning the file offset in bytes.
 *
 * @retval NvError_Success No error.
 * @retval NvError_InvalidParameter Illegal parameter value.
 *
 */

 NvError NvStorMgrFtell(
    NvStorMgrFileHandle hFile,
    NvU64 * ByteOffset );

/**
 * Perform an I/O control operation on the file.
 *
 * @param hFile Handle to the file.
 * @param Opcode Specifies the type of control operation to perform.
 * @param InputSize Size of input arguments buffer, in bytes.
 * @param OutputSize Size of output arguments buffer, in bytes.
 * @param InputArgs A pointer to input arguments buffer.
 * @param OutputArgs A pointer to output arguments buffer.
 *
 * @retval NvError_Success No error.
 * @retval NvError_InvalidParameter Illegal parameter value.
 */

 NvError NvStorMgrFileIoctl(
    NvStorMgrFileHandle hFile,
    NvU32 Opcode,
    NvU32 InputSize,
    NvU32 OutputSize,
    const void* InputArgs,
    void* OutputArgs );

/**
 * Queries information about a file given its handle.
 *
 * @param hFile Handle to the file.
 * @param pStat buffer A pointer to where file information will be placed.
 *
 * @retval NvError_Success No error.
 * @retval NvError_InvalidParameter Illegal parameter value.
 */

 NvError NvStorMgrFileQueryFstat(
    NvStorMgrFileHandle hFile,
    NvFileStat * pStat );

/**
 * Queries information about a file given its name.
 *
 * @param FileName Name of file.
 * @param pStat A pointer to a buffer where file information will be placed.
 *
 * @retval NvError_Success No error.
 * @retval NvError_InvalidParameter Illegal parameter value.
 */

 NvError NvStorMgrFileQueryStat(
    const char * FileName,
    NvFileStat * pStat );

/**
 * Queries information about a filesystem given the name of any file in
 * the filesystem.
 *
 * @param FileName Name of a file on the filesystem.
 * @param pStat A pointer to a buffer where filesystem information will be
 *    placed.
 *
 * @retval NvError_Success No Error.
 * @retval NvError_InvalidParameter Illegal parameter value.
 */

 NvError NvStorMgrFileSystemQueryStat(
    const char * FileName,
    NvFileSystemStat * pStat );

/**
 * Queries parent partition information given the name of any file in
 * the filesystem.
 *
 * @param FileName Name of a file on the partition.
 * @param pStat A pointer to a buffer where partition information will be placed.
 *
 * @retval NvError_Success No error.
 * @retval NvError_InvalidParameter Illegal parameter value.
 */

 NvError NvStorMgrPartitionQueryStat(
    const char * FileName,
    NvPartitionStat * pStat );

/**
 * Perform low-level partition format suitable for the file
 * system that will be mounted on the partition.
 *
 * @param PartitionName Name of the partition to be formatted.
 *
 * @retval NvError_Success No error.
 * @retval NvError_InvalidParameter Illegal parameter value.
 *
 */

 NvError NvStorMgrFormat(
    const char * PartitionName );

/** @} */

#if defined(__cplusplus)
}
#endif

#endif
