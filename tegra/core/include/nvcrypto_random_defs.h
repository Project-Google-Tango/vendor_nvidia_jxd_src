/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * @brief <b>NVIDIA Cryptography Framework</b>
 *
 * @b  Description: This file declares the data structures used by NvCrypto
 *     pseudo-random number generation (PRNG) algorithms.
 */

#ifndef INCLUDED_NVCRYPTO_RANDOM_DEFS_H
#define INCLUDED_NVCRYPTO_RANDOM_DEFS_H

#include "nvcrypto_common_defs.h"
#include "nverror.h"
#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif
/**
 * @defgroup nvcrypto_random_defs Pseudo-Random Number Generation Definitions
 *
 * Declares the data structures used by NvCrypto pseudo-random number
 * generation (PRNG) algorithms.
 *
 * @ingroup nvcrypto_random_grp
 * @{
 */

/**
 * Defines supported PRNG algorithms.
 */
typedef enum
{
    NvCryptoRandomAlgoType_Invalid,
    /// X9.31 algorithm using AES encryption, as documented in "NIST-Recommended
    ///     Random Number Generator Based on ANSI X9.31 Appendix A.2.4 Using the
    ///     3-Key Triple DES and AES Algorithms," Sharon S. Keller,
    ///     January 11, 2005.
    NvCryptoRandomAlgoType_AnsiX931Aes,

    NvCryptoRandomAlgoType_Num,
    NvCryptoRandomAlgoType_Max = 0x7fffffff
} NvCryptoRandomAlgoType;
    
/**
 * Holds ANSI X9.31 with AES (AnsiX931Aes) algorithm parameters.
 */
typedef struct NvCryptoRandomAlgoParamsAnsiX931AesRec
{
    /// Holds the select key type--SBK, SSK, user-specified, etc.
    NvCryptoCipherAlgoAesKeyType KeyType;
    /// Holds the AES key length, which must be 128-bit for SBK or SSK.
    NvCryptoCipherAlgoAesKeySize KeySize;
    /// Holds the specified AES key value, which is \c KeyType if
    ///     user-specified else ignored.
    NvU8 KeyBytes[NVCRYPTO_CIPHER_AES_MAX_KEY_BYTES];
    /// Holds the initial seed value.
    NvU8 InitialSeedBytes[NVCRYPTO_CIPHER_AES_IV_BYTES];
} NvCryptoRandomAlgoParamsAnsiX931Aes;

/**
 * Defines random algorithm parameters--a union of all per-algorithm parameter
 *     structures.
 */
typedef union 
{
    NvCryptoRandomAlgoParamsAnsiX931Aes AnsiX931Aes;
} NvCryptoRandomAlgoParams;
    

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

/** @} */
#endif // INCLUDED_NVCRYPTO_RANDOM_DEFS_H

