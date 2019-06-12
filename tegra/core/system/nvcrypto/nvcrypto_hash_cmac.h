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
 * @brief <b>NVIDIA Cryptography Framework</b>
 *
 * @b  Description: This file declares the interface for NvCrypto CMAC
 *     hash algorithms.
 */

#ifndef INCLUDED_NVCRYPTO_HASH_CMAC_H
#define INCLUDED_NVCRYPTO_HASH_CMAC_H

#include "nvcrypto_hash.h"
#include "nverror.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup nvcrypto_hash_cmac_grp CMAC Hash Interface
 *
 * This is the interface for NvCrypto CMAC hash algorithms.
 *
 * @ingroup nvcrypto_modules
 * @{
 */

/**
 * Allocate states (and function pointers) for the selected hash algorithm
 *
 * The returned handle (\a *pHashAlgoHandle) must be passed in to all
 * subsequent calls to the \c NvCryptoHash*() functions.
 *
 * @param HashAlgoType Specifies the type of hash algorithm to be computed.
 * @param pHashAlgoHandle A pointer to handle containing hash algorithm
 *     functions and state information.
 *
 * @retval NvSuccess Indicates the hash state allocated successfully.
 * @retval NvError_BadParameter Indicates an unknown algorithm type.
 * @retval NvError_InvalidAddress Indicates a pointer is illegal (NULL).
 */
NvError
NvCryptoHashSelectAlgorithmCmac(
    NvCryptoHashAlgoType HashAlgoType,
    NvCryptoHashAlgoHandle *pHashAlgoHandle);

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

/** @} */
#endif // INCLUDED_NVCRYPTO_HASH_CMAC_H

