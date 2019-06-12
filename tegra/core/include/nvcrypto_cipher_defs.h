/*
 * Copyright (c) 2008-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * @brief <b> NVIDIA Cryptography Framework</b>
 *
 * @b  Description: This file declares the data structures used by NvCrypto
 *     cipher algorithms.
 */

#ifndef INCLUDED_NVCRYPTO_CIPHER_DEFS_H
#define INCLUDED_NVCRYPTO_CIPHER_DEFS_H

#include "nvcrypto_common_defs.h"
#include "nverror.h"
#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup nvcrypto_cipher_defs Cipher Algorithm Definitions
 *
 * Declares the data structures used by NvCrypto cipher algorithms.
 * @ingroup nvcrypto_cipher_grp
 * @{
 */

/**
 * Defines supported cipher algorithms.
 */
typedef enum
{
    NvCryptoCipherAlgoType_Invalid,
    /// AES with Cipher Block Chaining (CBC)
    ///     for AES spec. See FIPS Publication 197
    ///     for CBC spec, and see NIST Special Publication 800-38A.
    NvCryptoCipherAlgoType_AesCbc,
    NvCryptoCipherAlgoType_AesEcb,
    NvCryptoCipherAlgoType_Num,
    NvCryptoCipherAlgoType_Max = 0x7fffffff
} NvCryptoCipherAlgoType;

/**
 * Holds the AES Cipher Block Chaining (CBC) algorithm parameters.
 */
typedef struct NvCryptoCipherAlgoParamsAesCbcRec
{
    /// Holds NV_TRUE to perform encryption; NV_FALSE to perform decryption.
    NvBool IsEncrypt;
    /// Holds the select key type--SBK, SSK, user-specified, etc.
    NvCryptoCipherAlgoAesKeyType KeyType;
    /// Holds the AES key length, which must be 128-bit for SBK or SSK.
    NvCryptoCipherAlgoAesKeySize KeySize;
    /// Holds the specified AES key value, which is \c KeyType if
    ///     user-specified else ignored.
    NvU8 KeyBytes[NVCRYPTO_CIPHER_AES_MAX_KEY_BYTES];
    /// Holds the initial vector to use when encrypting/decrypting the
    ///     first block in the chain.
    NvU8 InitialVectorBytes[NVCRYPTO_CIPHER_AES_IV_BYTES];
    /// Holds the type of padding to use; only explicit padding is supported
    ///     for AES CBC.
    NvCryptoPaddingType PaddingType;
} NvCryptoCipherAlgoParamsAesCbc;

/**
 * Defines cipher algorithm parameters--a union of all per-algorithm
 *     parameter structures.
 */
typedef union
{
    NvCryptoCipherAlgoParamsAesCbc AesCbc;
} NvCryptoCipherAlgoParams;


#if defined(__cplusplus)
}
#endif  /* __cplusplus */

/** @} */
#endif // INCLUDED_NVCRYPTO_CIPHER_DEFS_H


