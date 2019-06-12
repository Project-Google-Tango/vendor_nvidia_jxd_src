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
 * @brief <b> NVIDIA Cryptography Framework</b>
 *
 * @b  Description: This file declares the interface for NvCrypto
 *     ANSI X9.31 AES pseudo-random number generation (PRNG) algorithms.
 */

#ifndef INCLUDED_NVCRYPTO_RANDOM_AES_H
#define INCLUDED_NVCRYPTO_RANDOM_AES_H

#include "nvcrypto_random.h"
#include "nverror.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup nvcrypto_random_aes_grp AES PRNG Interface
 *
 * This is the interface for NvCrypto ANSI X9.31 AES pseudo-random number
 *     generation (PRNG) algorithms.
 *
 * @ingroup nvcrypto_modules
 * @{
 */

/**
 * Allocates state (and function pointers) for the selected PRNG algorithm.
 *
 * The returned handle (\a *pRandomAlgoHandle) must be passed in to all
 * subsequent calls to the \c NvCryptoRandom*() functions.
 *
 * @param RandomAlgoType Specifies the type of PRNG algorithm to be computed.
 * @param pRandomAlgoHandle A pointer to a handle containing PRNG algorithm
 *     functions and state information.
 *
 * @retval NvSuccess Indicates PRNG state was allocated successfully.
 * @retval NvError_BadParameter Indicates an unknown algorithm type.
 * @retval NvError_InvalidAddress Indicates the pointer is illegal (NULL).
 */

NvError
NvCryptoRandomSelectAlgorithmAnsiX931Aes(
    NvCryptoRandomAlgoType RandomAlgoType,
    NvCryptoRandomAlgoHandle *pRandomAlgoHandle);

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

/** @} */
#endif // INCLUDED_NVCRYPTO_RANDOM_AES_H

