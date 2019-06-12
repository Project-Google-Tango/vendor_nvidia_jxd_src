/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_nvddk_aes_common_H
#define INCLUDED_nvddk_aes_common_H


#if defined(__cplusplus)
extern "C"
{
#endif


/** @file
 * @brief <b>NVIDIA Driver Development Kit:
 *           NvDdk AES Driver Definitions</b>
 *
 * @b Description: This file defines the common definitions for the DDK AES .
 */

/**
 * @defgroup nvddk_aes_common AES Common Driver Definitions
 *
 * These are the common definitions for the NvDDK AES driver.
 *
 * @ingroup nvddk_modules
 * @{
 */

/**
 * Defines AES-related constants.
 */

typedef enum
{

    /**
     * Maximum supported AES key length, in bytes.
     */
    NvDdkAesConst_MaxKeyLengthBytes = 32,

    /**
     * AES initial vector length, in bytes.
     */
    NvDdkAesConst_IVLengthBytes = 16,

    /**
     * AES block size, in bytes.
     */
    NvDdkAesConst_BlockLengthBytes = 16,
    NvDdkAesConst_Num,
    NvDdkAesConst_Force32 = 0x7FFFFFFF
} NvDdkAesConst;

/**
 * Defines AES key lengths.
 */

typedef enum
{
    NvDdkAesKeySize_Invalid,

    /**
     * Defines 128-bit key length size, in bytes.
     */
    NvDdkAesKeySize_128Bit = 16,

    /**
     * Defines 192-bit key length size, in bytes.
     */
    NvDdkAesKeySize_192Bit = 24,

    /**
     * Defines 256-bit key length size, in bytes.
     */
    NvDdkAesKeySize_256Bit = 32,
    NvDdkAesKeySize_Num,
    NvDdkAesKeySize_Force32 = 0x7FFFFFFF
} NvDdkAesKeySize;

/**
 * AES key types.
 */

typedef enum
{
    NvDdkAesKeyType_Invalid,

    /**
     * Secure Boot Key (SBK) is used to protect boot-related data when the chip
     * is in ODM production mode.
     */
    NvDdkAesKeyType_SecureBootKey,

    /**
     * Secure Storage Key (SSK) is unique per chip, unless explicitly
     * over-ridden; it is intended for data that must be tied to a specific
     * instance of the chip.
     */
    NvDdkAesKeyType_SecureStorageKey,

    /**
     * User-specified key is an arbitrary key supplied explicitly by the caller.
     */
    NvDdkAesKeyType_UserSpecified,

    /**
     * Encypted key is an encrypted key supplied explicitly by the caller.
     */
    NvDdkAesKeyType_EncryptedKey,
    NvDdkAesKeyType_Num,
    NvDdkAesKeyType_Force32 = 0x7FFFFFFF
} NvDdkAesKeyType;

/**
 * Defines supported cipher algorithms.
 */

typedef enum
{
    NvDdkAesOperationalMode_Invalid,

    /**
     * AES with Cipher Block Chaining (CBC)--
     * For AES spec, see FIPS Publication 197;
     * For CBC spec, see NIST Special Publication 800-38A.
     */
    NvDdkAesOperationalMode_Cbc,

    /**
     * AES ANSIX931 RNG--
     * Algorithm is specified in the document "NIST-Recommended Random Number
     * Generator Based on ANSI X9.31 Appendix A.2.4 Using the 3-Key Triple DES
     * and AES Algorithms," Sharon S. Keller, January 11, 2005;
     * Input vector is specified by the user.
     */
    NvDdkAesOperationalMode_AnsiX931,

    /**
     * AES with Electronic Codebook (ECB)--
     * For AES spec, see FIPS Publication 197;
     * For ECB spec, see NIST Special Publication 800-38A.
     */
    NvDdkAesOperationalMode_Ecb,
    NvDdkAesOperationalMode_Num,
    NvDdkAesOperationalMode_Force32 = 0x7FFFFFFF
} NvDdkAesOperationalMode;

/**
 * Holds the key argument.
 */

typedef struct NvDdkAesKeyInfoRec
{

    /**
     * Select key type -- SBK, SSK, user-specified.
     */
    NvDdkAesKeyType KeyType;

    /**
     * Length of key; ignored unless key type is user-specified.
     */
    NvDdkAesKeySize KeyLength;

    /**
     * Key value; ignored unless key type is user-specified.
     */
    NvU8 Key[32];

    /**
     * NV_TRUE if key must be assigned a dedicated key slot, else NV_FALSE.
     * Assigned key slot may be time-multiplexed if \a IsDedicateKeySlot is not
     * set to NV_TRUE.
     */
    NvBool IsDedicatedKeySlot;
} NvDdkAesKeyInfo;

/**
 * Holds NvDdk AES operation argument.
 */

typedef struct NvDdkAesOperationRec
{

    /**
     * Block cipher mode of operation.
     */
    NvDdkAesOperationalMode OpMode;

    /**
     * Select NV_TRUE to perform an encryption operation, NV_FALSE to perform a
     * decryption operation.
     *
     * @note IsEncrypt will be ignored when AES operation mode is
     * NvDdkAesOperationalMode_AnsiX931.
     */
    NvBool IsEncrypt;
} NvDdkAesOperation;

/**
 * Holds NvDdk AES capabilities argument.
 */

typedef struct NvDdkAesCapabilitiesRec
{

    /**
     * Buffer alignment. For optimal performance, ensure that source and
     * destination buffers for cipher operations are aligned such that
     * <pre>
     * (pBuffer % OptimalBufferAlignment) == 0
     * </pre>
     * where \a pBuffer is the start address of the buffer.
     */
    NvU32 OptimalBufferAlignment;
} NvDdkAesCapabilities;

/** @} */

#if defined(__cplusplus)
}
#endif

#endif
