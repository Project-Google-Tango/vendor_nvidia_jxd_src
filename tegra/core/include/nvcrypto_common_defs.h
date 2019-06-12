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
 * @b  Description: This file declares common data structures used by multiple
 *     crypto algorithms.
 */

#ifndef INCLUDED_NVCRYPTO_COMMON_DEFS_H
#define INCLUDED_NVCRYPTO_COMMON_DEFS_H

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup nvcrypto_common_grp Common Definitions
 *
 * These are the common data structures used by the NvCrypto algorithms.
 *
 * @ingroup nvcrypto_modules
 * @{
 */

/**
 * Defines AES key lengths.
 */
typedef enum
{
    NvCryptoCipherAlgoAesKeySize_Invalid,
    NvCryptoCipherAlgoAesKeySize_128Bit = 128/8,
    NvCryptoCipherAlgoAesKeySize_192Bit = 192/8,
    NvCryptoCipherAlgoAesKeySize_256Bit = 256/8,
    NvCryptoCipherAlgoAesKeySize_Max = 0x7fffffff
} NvCryptoCipherAlgoAesKeySize;

/**
 * Anonymous enum for AES key, vector, and block length values.
 */
enum {
    /// Maximum supported AES key length, in bytes.
    NVCRYPTO_CIPHER_AES_MAX_KEY_BYTES = 256/8,
    /// AES initial vector length, in bytes.
    NVCRYPTO_CIPHER_AES_IV_BYTES = 128/8,
    /// AES block length, in bytes.
    NVCRYPTO_CIPHER_AES_BLOCK_BYTES = 128/8
};
    
/**
 * Defines AES key types.
 */
typedef enum
{
    NvCryptoCipherAlgoAesKeyType_Invalid,
    /// Secure Boot Key (SBK) is used to protect boot-related data when the chip
    /// is in ODM non-secure mode (ODM production mode).
    NvCryptoCipherAlgoAesKeyType_SecureBootKey,
    /// Secure Storage Key (SSK) is unique per chip, unless explicitly over-
    /// ridden; it is intended for data that must be tied to a specific
    /// instance of the chip.
    NvCryptoCipherAlgoAesKeyType_SecureStorageKey,
    /// User-specified key is an arbitrary key supplied explicitly by the
    ///      caller.
    NvCryptoCipherAlgoAesKeyType_UserSpecified,

    NvCryptoCipherAlgoAesKeyType_Num,
    NvCryptoCipherAlgoAesKeyType_Max = 0x7fffffff
} NvCryptoCipherAlgoAesKeyType;

/**
 * Defines padding types.
 *
 * Explicit padding must be added to the payload data by the client.
 *
 * Implicit padding is added inside a crypto algorithm such that the crypto
 * calculations can be carried out as though the padding had been added to the
 * payload. The padding is subsequently discarded and never appears in the
 * payload data apparent to the client.
 *
 * Take the example of a block-cipher based MAC algorithm. The size of the MAC
 * result does not depend on the size of the payload data; however, the payload
 * data must be a multiple of the cipher block size.
 *
 * In this case, if explicit padding is used then the client must add padding to
 * the payload so as to ensure that the payload is a multiple of the cipher
 * block size.
 *
 * If implicit padding is used, padding can be added inside the MAC calculation
 * and then discarded, because:
 * - the client only cares about the MAC result, and
 * - the MAC can be recalculated in the future without access to the original
 *       padding data (so long as the padding algorithm is known).
 *
 */
typedef enum
{
    NvCryptoPaddingType_Invalid,
    /// Use no padding; caller must ensure that input data meets any block size
    ///     constraints.
    NvCryptoPaddingType_None,
    /// The byte pattern "0x80 0x00 ... 0x00" is appended to the
    ///     input data. If the input data is already an exact multiple of the
    ///     block size, then an additional block will be added to accomodate
    ///     the padding.
    NvCryptoPaddingType_ExplicitBitPaddingAlways,
    /// Same as \c ExplicitBitPaddingAlways, except that no padding is added if
    ///     the input data is already an exact multiple of the block size.
    NvCryptoPaddingType_ExplicitBitPaddingOptional,
    /// No padding is added to the input data, but calculations proceed as if
    ///     \c  ExplicitBitPaddingAlways padding were applied.
    NvCryptoPaddingType_ImplicitBitPaddingAlways,
    /// No padding is added to the input data, but calculations proceed as if
    ///     \c ExplicitBitPaddingOptional padding were applied.
    NvCryptoPaddingType_ImplicitBitPaddingOptional,

    NvCryptoPaddingType_Num,
    NvCryptoPaddingType_Max = 0x7fffffff
} NvCryptoPaddingType;


#if defined(__cplusplus)
}
#endif  /* __cplusplus */

/** @} */
#endif // INCLUDED_NVCRYPTO_COMMON_DEFS_H

