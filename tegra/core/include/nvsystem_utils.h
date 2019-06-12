/*
 * Copyright (c) 2008 - 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVSYSTEM_UTILS_H
#define INCLUDED_NVSYSTEM_UTILS_H

#include "nvcommon.h"
#include "nverror.h"
#include "nvddk_fuse.h"
#include "nvstormgr.h"

#if defined(__cplusplus)
extern "C"
{
#endif

// FIXME: placeholder
NvError
NvSysUtilBootloaderWrite( NvU8 *bootloader, NvU32 length, NvU32 part_id,
    NvDdkFuseOperatingMode, NvU8 *hash, NvU32 hash_size, NvU32 *padded_length);

NvError
NvSysUtilEncyptedBootloaderWrite( NvU8 *bootloader, NvU32 length, NvU32 part_id,
    NvDdkFuseOperatingMode, NvU8 *hash, NvU32 hash_size, NvU32 *padded_length, NvBool IsEncrypted );

NvError
NvSysUtilBootloaderReadVerify( NvU8 *bootloader, NvU32 part_id,
    NvDdkFuseOperatingMode OperatingMode,NvU64 size, NvU8 *ExpectedHash, NvU64 StagingBufSize);

/**
 * NvSysUtilImageInfo
 * Gets the entrypoint, version and load address of a partition for a given PartitionId
 */
NvError
NvSysUtilImageInfo(NvU32 PartitionId, NvU32 *LoadAddress,
    NvU32 *EntryPoint, NvU32 *version, NvU32 length );

NvBool
NvSysUtilGetDeviceInfo( NvU32 id, NvU32 logicalSector, NvU32 *sectorsPerBlock,
                        NvU32 *bytesPerSector, NvU32 *physicalSector);

/**
 * @param Buffer Buffer containing the sparse data
 * @param size size of buffer
 * @retval NV_TRUE The given buffer is first buffer of Sparse file and contains magic number
 * @retval NV_FALSE The given buffer is not the first buffer of sparse file
 */
NvBool NvSysUtilCheckSparseImage(const NvU8 *Buffer, NvU32 size);

/**
 * @param hFile Handle to the storage manager
 * @param Buffer Buffer containing the sparse data
 * @param size size of buffer
 * @param IsStartSparse Boolean to tell first buffer to unsparse
 * @retval NvSuccess Successfully unsparsed the given buffer
 * @retval NvError_NotSupported This version is not supported
 * @retval NvError_CountMismatch CRC mismatch occured
 * @retval NvError_InvalidSize The buffer size is invalid
 * @retval NvError_BadParameter Indicates that the parameters in the given sparse file
 *                                   are not recognised
 */
NvError
NvSysUtilUnSparse(
    NvStorMgrFileHandle hFile,
    const NvU8 *Buffer,
    NvU32 Size,
    NvBool IsStartSparse,
    NvBool IsLastBuffer,
    NvBool IsSignRequired,
    NvBool IsCRC32Required,
    NvU8 *Hash,
    NvDdkFuseOperatingMode *OpMode);

/**
 * Signs the incoming data stream
 *
 * @param pBuffer pointer to data buffer
 * @param bytesToSign number of bytes in this part of the data stream.
 * @param StartMsg marks beginning of the data stream.
 * @param EndMsg Marks the end of the msg.
 * @param pHash Hash computes so far.
 * @warning Caller of this function is responsible for setting
 *            StartMsg and EndMsg flags correctly.
 * @warning There wont be any valid hash stored in pHash
 *            this API is called with EndMsg flag set
 *
 * @retval NvSuccess if signing the data was successful
 * @retval NvError_* There were errors signing the data
 */
NvError
NvSysUtilSignData(
    const NvU8 *pBuffer,
    NvU32 bytesToSign,
    NvBool StartMsg,
    NvBool EndMsg,
    NvU8 *pHash,
    NvDdkFuseOperatingMode *pOpMode);

/**
 * Encrypts the incoming data stream
 *
 * @param pBuffer pointer to data buffer
 * @param bytesToSign number of bytes in this part of the data stream.
 * @param StartMsg marks beginning of the data stream.
 * @param EndMsg Marks the end of the msg.
 * @param IsDecrypt whether to encrypt or decypt the msg
 * @retval NvSuccess if signing the data was successful
 * @retval NvError_* There were errors Encrypting  the data
 */
NvError
NvSysUtilEncryptData(NvU8 *pBuffer,
    NvU32 bytesToSign,
    NvBool StartMsg,
    NvBool EndMsg,
    NvBool IsDecrypt,
    NvDdkFuseOperatingMode OpMode);

/**
 * Returns the ChipId of device
 *
 * @param none
 * @retval Nonzero ChipId value on a successful reading
 * @retval zero on any error
 */
NvU32 NvSysUtilGetChipId(void);

/**
 * Converts character to lower case character.
 * @param character character to be converted to lower case
 */
char NvSysUtilsToLower(const char character);

/**
 * Case insensitive string comparision.
 * @params strings to be compared
 */
NvS32 NvSysUtilsIStrcmp(const char *string1, const char *string2);


#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVSYSTEM_UTILS_H
