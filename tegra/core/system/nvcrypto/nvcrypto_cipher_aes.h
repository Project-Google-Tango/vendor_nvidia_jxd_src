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
 * @b  Description: This file declares the interface for NvCrypto AES
 *     cipher algorithms.
 */

#ifndef INCLUDED_NVCRYPTO_CIPHER_AES_H
#define INCLUDED_NVCRYPTO_CIPHER_AES_H

#include "nvcrypto_cipher.h"
#include "nverror.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup nvcrypto_cipher_aes_grp AES Cipher Interface
 *
 * This is the interface for NvCrypto AES cipher algorithms.
 *
 * @ingroup nvcrypto_modules
 * @{
 */

/**
 * Allocate states (and function pointers) for the selected cipher algorithm.
 *
 * The returned handle (\a *pCipherAlgoHandle) must be passed in to all
 * subsequentcalls to the \c NvCryptoCipher*() functions.
 *
 * @param CipherAlgoType Specifies the type of cipher algorithm to be computed.
 * @param pCipherAlgoHandle A pointer to a handle containing cipher algorithm
 *     functions and state information.
 *
 * @retval NvSuccess Indicates the cipher state allocated successfully.
 * @retval NvError_BadParameter Indicates an unknown algorithm type.
 * @retval NvError_InvalidAddress Indicates the pointer is illegal (NULL).
 */

NvError
NvCryptoCipherSelectAlgorithmAes(
    NvCryptoCipherAlgoType CipherAlgoType,
    NvCryptoCipherAlgoHandle *pCipherAlgoHandle);

/**
 * Allocate states (and function pointers) for the selected cipher algorithm.
 * The returned handle (\a *pCipherAlgoHandle) must be passed in to all
 * subsequentcalls to the \c NvCryptoCipher*() functions.
 *
 * @param CipherAlgoType Specifies the type of cipher algorithm to be computed.
 * @param pCipherAlgoHandle A pointer to a handle containing cipher algorithm
 * functions and state information.
 *
 * @retval NvSuccess Indicates the cipher state allocated successfully.
 * @retval NvError_BadParameter Indicates an unknown algorithm type.
 * @retval NvError_InvalidAddress Indicates the pointer is illegal (NULL).
 */
NvError NvCryptoCipherSelectAlgorithmSeAes(
        NvCryptoCipherAlgoType CipherAlgoType,
        NvCryptoCipherAlgoHandle *pCipherAlgoHandle);

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

/** @} */
#endif // INCLUDED_NVCRYPTO_CIPHER_AES_H
