/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
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
 *     hash algorithms.
 */

#ifndef INCLUDED_NVCRYPTO_HASH_DEFS_H
#define INCLUDED_NVCRYPTO_HASH_DEFS_H

#include "nvcrypto_common_defs.h"
#include "nverror.h"
#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup nvcrypto_hash_defs Hash Algorithm Definitions
 *
 * Declares the data structures used by NvCrypto hash algorithms.
 *
 * @ingroup nvcrypto_hash_grp
 * @{
 */

/**
 * Defines supported hash algorithms.
 */
typedef enum
{
    NvCryptoHashAlgoType_Invalid,
    /// Specifies AES Cipher-based MAC (CMAC), see IEFT RFC 4493 spec.
    NvCryptoHashAlgoType_AesCmac,

    NvCryptoHashAlgoType_Num,
    NvCryptoHashAlgoType_Max = 0x7fffffff
} NvCryptoHashAlgoType;

/**
 * AES Cipher-based Message Authentication Code (CMAC) algorithm parameters.
 *
 * This hash algorithm is built on top of the AES-CBC block cipher.
 */
typedef struct NvCryptoHashAlgoParamsAesCmacRec
{
    /// Holds NV_TRUE to calculate hash; NV_FALSE to verify hash.
    NvBool IsCalculate;
    /// Holds the select key type--SBK, SSK, user-specified, etc.
    NvCryptoCipherAlgoAesKeyType KeyType;
    /// Holds the AES key length, which must be 128-bit for SBK or SSK.
    NvCryptoCipherAlgoAesKeySize KeySize;
    /// Holds the AES key value is \a KeyType if user-specified; else ignored.
    NvU8 KeyBytes[NVCRYPTO_CIPHER_AES_MAX_KEY_BYTES];
    /// Holds the type of padding to use.
    NvCryptoPaddingType PaddingType;
} NvCryptoHashAlgoParamsAesCmac;

/**
 * Defines hash algorithm parameters--a union of all per-algorithm parameter
 *     structures.
 */
typedef union 
{
    NvCryptoHashAlgoParamsAesCmac AesCmac;
} NvCryptoHashAlgoParams;


#if defined(__cplusplus)
}
#endif  /* __cplusplus */

/** @} */
#endif // INCLUDED_NVCRYPTO_HASH_DEFS_H

