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

#ifndef INCLUDED_NVFS_DEFS_H
#define INCLUDED_NVFS_DEFS_H

#include "nvcrypto_cipher_defs.h"
#include "nvcrypto_hash_defs.h"
#include "nvcommon.h"
#include "nverror.h"

#if defined(__cplusplus)
extern "C"
{
#endif


/*
 * File information
 */

typedef struct NvFileStatRec
{
    /** size of file in bytes */
    NvU64 Size;
} NvFileStat;
    
/*
 * Filesystem information
 */

typedef struct NvFileSystemStatRec
{
    /** filesystem type */
    NvU32 FsType;
    /** write-protect flag; NV_TRUE if enabled, else NV_FALSE */
    NvBool WriteProtect;
} NvFileSystemStat;
    
/**
 * Enum defining ioctl types.
 */
typedef enum
{
    NvFileSystemIoctlType_Invalid = 0,

    /**
     * Set crypto algorithm for encrypting/decrypting file data
     *
     * inputs: NvFileSystemIoctl_SetCryptoCipherAlgoInputArgs
     * outputs: none
     *
     * @retval NvError_Success Read is successful.
     * @retval NvError_AlreadyAllocated Cipher algorithm has already been specified
     * @retval NvError_BadParameter Algorithm type or parameter settings are illegal
     */
    NvFileSystemIoctlType_SetCryptoCipherAlgo,

    /**
     * Set crypto algorithm for hashing file data
     *
     * inputs: NvFileSystemIoctl_SetCryptoHashAlgoInputArgs
     * outputs: none
     *
     * @retval NvError_Success Read is successful.
     * @retval NvError_AlreadyAllocated Hash algorithm has already been specified
     * @retval NvError_BadParameter Algorithm type or parameter settings are illegal
     */
    NvFileSystemIoctlType_SetCryptoHashAlgo,

    /**
     * Query whether file data is encrypted
     *
     * inputs: none
     * outputs: NvFileSystemIoctl_QueryIsEncryptedOutputArgs
     *
     * @retval NvError_Success Query is successful
     */
    NvFileSystemIoctlType_QueryIsEncrypted,

    /**
     * Query whether a hash value was computed when the file data was written
     *
     * inputs: none
     * outputs: NvFileSystemIoctl_QueryIsHashedOutputArgs
     *
     * @retval NvError_Success Query is successful
     */
    NvFileSystemIoctlType_QueryIsHashed,

    /**
     * When a file is open for reading, returns the stored hash value that was
     * computed and stored when the file data was previously written
     *
     * If (upon entry) BufferSize is less than the size of the hash value, then
     * HashSize is set to actual hash size in bytes and the error
     * NvError_InsufficientMemory is returned.  Thus, to query the hash size,
     * simply set BufferSize to zero before invoking this function.  The value
     * of pBuffer is ignored.
     *
     * If (upon entry) BufferSize is equal to or greater than the size of the
     * value, then HashSize is set to the actual hash result size and the hash
     * value itself is copied into pBuffer.  pBuffer is not ignored in this
     * case; if it is illegal (NULL), then NvError_InvalidAddress is returned.
     *
     * inputs: NvFileSystemIoctl_GetStoredHashInputArgs
     * outputs: NvFileSystemIoctl_GetStoredHashOutputArgs
     *
     * @retval NvError_Success Stored hash retrieved successfully
     * @retval NvError_InvalidState File not open for read, or no hash algorithm
     * was specified data was written
     * @retval NvError_NotSupported Current file system does not support hashing
     * @retval NvError_InvalidAddress Illegal buffer pointer (NULL).
     * @retval NvError_InsufficientMemory Specified buffer size is too small to
     *         hold the hash result data
     */
    NvFileSystemIoctlType_GetStoredHash,

    /**
     * Return the hash value that was computed as part of the current file
     * read/write operation
     *
     * If (upon entry) BufferSize is less than the size of the hash value, then
     * HashSize is set to actual hash size in bytes and the error
     * NvError_InsufficientMemory is returned.  Thus, to query the hash size,
     * simply set BufferSize to zero before invoking this function.  The value
     * of pBuffer is ignored.
     *
     * If (upon entry) BufferSize is equal to or greater than the size of the
     * value, then HashSize is set to the actual hash result size and the hash
     * value itself is copied into pBuffer.  pBuffer is not ignored in this
     * case; if it is illegal (NULL), then NvError_InvalidAddress is returned.

     * inputs: NvFileSystemIoctl_GetCurrentHashInputArgs
     * outputs: NvFileSystemIoctl_GetCurrentHashOutputArgs
     *
     * @retval NvError_Success Current hash retrieved successfully
     * @retval NvError_InvalidState No hash algorithm was specified when data
     * was read/written
     * @retval NvError_NotSupported Current file system does not support hashing
     * @retval NvError_InvalidAddress Illegal buffer pointer (NULL).
     * @retval NvError_InsufficientMemory Specified buffer size is too small to
     *         hold the hash result data
     */
    NvFileSystemIoctlType_GetCurrentHash,

    /**
     * Report whether the (stored) hash value that was computed and stored when
     * the file was being written matches the (current) hash value that was
     * computed as the file was being read back
     *
     * inputs: none
     * outputs: NvFileSystemIoctl_IsValidHashOutputArgs
     *
     * @retval NvError_Success Validation operation completed successfully
     * @retval NvError_InvalidState No hash algorithm was specified when data
     * was written and/or no hash algorithm was specified when data was read
     * back
     * @retval NvError_NotSupported Current file system does not support hashing
     */
    NvFileSystemIoctlType_IsValidHash,

    /**
     * Enable or disable write and verify operations in block device driver
     *
     * inputs: NvFileSystemIoctl_WriteVerifyModeSelectInputArgs
     * outputs: none
     */
    NvFileSystemIoctlType_WriteVerifyModeSelect,

    /**
     * Enable or disable write tag information in block device driver
     *
     * inputs: NvFileSystemIoctl_WriteTagDataEnableInputArgs
     * outputs: none
     */
    NvFileSystemIoctlType_WriteTagDataDisable,

    NvFileSystemIoctlType_Num,
    NvFileSystemIoctlType_Force32 = 0x7FFFFFFF
} NvFileSystemIoctlType;

/**
 *
 * Input/output structures for ioctl types.
 *
 */

/**
 * SetCryptoCipherAlgo Ioctl 
 */ 

/// SetCryptoCipherAlgo Ioctl input arguments
typedef struct NvDdkBlockDevIoctl_SetCryptoCipherAlgoInputArgsRec
{
    /// cipher algorithm type
    NvCryptoCipherAlgoType CipherAlgoType;
    /// parameters for cipher algorithm
    NvCryptoCipherAlgoParams CipherAlgoParams;
} NvDdkBlockDevIoctl_SetCryptoCipherAlgoInputArgs;

/**
 * SetCryptoHashAlgo Ioctl 
 */ 

/// SetCryptoHashAlgo Ioctl input arguments
typedef struct NvDdkBlockDevIoctl_SetCryptoHashAlgoInputArgsRec
{
    /// hash algorithm type
    NvCryptoHashAlgoType HashAlgoType;
    /// parameters for cipher algorithm
    NvCryptoHashAlgoParams HashAlgoParams;
} NvDdkBlockDevIoctl_SetCryptoHashAlgoInputArgs;

/**
 * QueryIsEncrypted Ioctl 
 */ 

///  Ioctl output arguments
typedef struct NvFileSystemIoctl_QueryIsEncryptedOutputArgs
{
    /// NV_TRUE if file is stored in encrypted form, else NV_FALSE if file is
    /// stored as clear text
    NvBool IsEncrypted;
} NvFileSystemIoctl_QueryIsEncryptedOutputArgs;

/**
 * QueryIsHashed Ioctl 
 */ 

///  Ioctl output arguments
typedef struct NvFileSystemIoctl_QueryIsHashedOutputArgs
{
    /// NV_TRUE if hash was computed when file was written and hash was stored
    /// along with the file data; NV_FALSE if no hash was computed when file was
    /// written or resulting hash value was not stored along with the file data
    NvBool IsHashed;
} NvFileSystemIoctl_QueryIsHashedOutputArgs;

/**
 * GetStoredHash Ioctl 
 */ 

///  Ioctl input arguments
typedef struct NvFileSystemIoctl_GetStoredHashInputArgs
{
    /// size of buffer for hash value, in bytes
    NvU32 BufferSize;
    /// pointer to buffer where hash value is to be stored
    NvU8 *pBuffer;
} NvFileSystemIoctl_GetStoredHashInputArgs;

///  Ioctl output arguments
typedef struct NvFileSystemIoctl_GetStoredHashOutputArgs
{
    /// size of the hash value, in bytes
    NvU32 HashSize;
} NvFileSystemIoctl_GetStoredHashOutputArgs;

/**
 * GetCurrentHash Ioctl 
 */ 

///  Ioctl input arguments
typedef struct NvFileSystemIoctl_GetCurrentHashInputArgs
{
    /// size of buffer for hash value, in bytes
    NvU32 BufferSize;
    /// pointer to buffer where hash value is to be stored
    NvU8 *pBuffer;
} NvFileSystemIoctl_GetCurrentHashInputArgs;

///  Ioctl output arguments
typedef struct NvFileSystemIoctl_GetCurrentHashOutputArgs
{
    /// size of the hash value, in bytes
    NvU32 HashSize;
} NvFileSystemIoctl_GetCurrentHashOutputArgs;

/**
 * QueryIsValidHash Ioctl 
 */ 

///  Ioctl output arguments
typedef struct NvFileSystemIoctl_QueryIsValidHashOutputArgs
{
    /// NV_TRUE if stored hash value matches current hash value, else NV_FALSE
    NvBool IsValidHash;
} NvFileSystemIoctl_QueryIsValidHashOutputArgs;

/**
 * WriteVerifyModeSelect Ioctl
 */

///  Ioctl input arguments
typedef struct NvFileSystemIoctl_WriteVerifyModeSelectInputArgsRec
{
    /// Flag to indicate enable/disable read verify write operation
    /// in Block device driver.
    /// NV_TRUE to set write operations to be verified by read 
    /// operation in block device driver.
    /// NV_FALSE to set no verification of the write operation 
    /// in block device driver
    NvBool ReadVerifyWriteEnabled;
} NvFileSystemIoctl_WriteVerifyModeSelectInputArgs;


///  Ioctl input arguments
typedef struct NvFileSystemIoctl_WriteTagDataDisableInputArgsRec
{
    /// Flag to indicate enable/disable tag data write enable disable operation
    /// in Block device driver.
    /// NV_TRUE to disables tag data write 
    /// operation in block device driver.
    /// NV_FALSE to enables tag data write 
    /// in block device driver
    NvBool TagDataWriteDisable;
} NvFileSystemIoctl_WriteTagDataDisableInputArgs;

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVFS_DEFS_H */
