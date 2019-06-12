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
 *     pseudo-random number generation (PRNG) algorithms.
 */

#ifndef INCLUDED_NVCRYPTO_RANDOM_H
#define INCLUDED_NVCRYPTO_RANDOM_H

#include "nvcrypto_random_defs.h"
#include "nverror.h"
#include "nvcommon.h"

#if defined(__cplusplus)

extern "C"
{
#endif

/**
 * @defgroup nvcrypto_random_grp Pseudo-Random Number Generation Interface
 *
 * This is the interface for the NvCrypto pseudo-random number generation (PRNG)
 * algorithms.
 *
 * The usage model for PRNG operations is as follows:
 *
 * -# Call NvCryptoRandomAlgo::SetAlgoParams() to set algorithm-specific
 *        parameters.
 * -# (Optional) Determine the algorithm's seed size by calling
 *        NvCryptoRandomAlgo::QuerySeedSize().
 * -# (Optional) Get current seed value via NvCryptoRandomAlgo::QuerySeed().
 * -# (Optional) Set the new seed value via NvCryptoRandomAlgo::SetSeed().
 * -# Generate pseudo-random numbers by invoking
 *        NvCryptoRandomAlgo::GenerateSamples().
 * -# (Optional) Repeat from Step 3 "Get current seed value".
 * -# When finished, call NvCryptoRandomAlgo::ReleaseAlgorithm() to release
 *        all state and resources associated with the PRNG calculations. The
 *        ::NvCryptoRandomAlgoHandle is no longer valid after this call.
 *
 * @ingroup nvcrypto_modules
 * @{
 */

/// Forward declaration.
typedef struct NvCryptoRandomAlgoRec *NvCryptoRandomAlgoHandle;

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
NvCryptoRandomSelectAlgorithm(
    NvCryptoRandomAlgoType RandomAlgoType,
    NvCryptoRandomAlgoHandle *pRandomAlgoHandle);

/**
 * Holds the PRNG algorithm interface pointers and state information.
 */
typedef struct NvCryptoRandomAlgoRec
{
    /**
     * Sets algorithm-specific parameters.
     *
     * @note The parameters structure is a union of per-algorithm settings.
     *     Only the union member for the selected algorithm must be set by the
     *     caller.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param pAlgoParams A pointer to parameter structure
     *
     * @retval NvSuccess Indicates algorithm parameter was set as specified.
     * @retval NvError_BadParameter Indicates algorithm parameter settings
     *    are illegal.
     * @retval NvError_InvalidAddress Indicates \a pAlgoParams is illegal
     *    (NULL).
     */
    NvError
    (*SetAlgoParams)(
        NvCryptoRandomAlgoHandle AlgoHandle,
        NvCryptoRandomAlgoParams *pAlgoParams);

    /**
     * Queries type of algorithm assigned to specified algorithm state handle.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param pAlgoType A pointer to algorithm type.
     *
     * @retval NvSuccess Indicates algorithm type query was successful.
     * @retval NvError_InvalidAddress Indicates \a pAlgoType is illegal (NULL).
     */
    NvError
    (*QueryAlgoType)(
        NvCryptoRandomAlgoHandle AlgoHandle,
        NvCryptoRandomAlgoType *pAlgoType);
    
    /**
     * Queries the seed size of PRNG algorithm.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param pSeedSize A pointer to the PRNG's seed size, in bytes.
     *
     * @retval NvSuccess Indicates seed size query was successful.
     * @retval NvError_InvalidAddress Indicates \a pSeedSize is illegal (NULL).
     */
    NvError
    (*QuerySeedSize)(
        NvCryptoRandomAlgoHandle AlgoHandle,
        NvU32 *pSeedSize);
    
    /**
     * Gets the current seed value for the PRNG algorithm.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param pSeed A pointer to buffer where seed value is to be placed; buffer
     *     must be at least as large as reported by \c QuerySeedSize().
     *
     * @retval NvSuccess Seed value query was successful.
     * @retval NvError_InvalidAddress \a pSeed is illegal (NULL).
     */
    NvError
    (*QuerySeed)(
        NvCryptoRandomAlgoHandle AlgoHandle,
        void *pSeed);
    
    /**
     * Sets seed value for PRNG algorithm.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param pSeed A pointer to the buffer containing the seed value; buffer
     *     must be at least as large as reported by
     *     NvCryptoRandomAlgo::QuerySeedSize().
     *
     * @retval NvSuccess Seed size query was successful.
     * @retval NvError_InvalidAddress \a pSeedSize is illegal (NULL).
     */
    NvError
    (*SetSeed)(
        NvCryptoRandomAlgoHandle AlgoHandle,
        void *pSeed);
    
    /**
     * Generates pseudo-random data using PRNG algorithm.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param NumBytes  The number of bytes of pseudo-random data to generate.
     * @param pBuffer A pointer to output buffer where pseudo-random data is to
     *     be stored.
     *
     * @retval NvSuccess Pseudo-random data has been generated successfully.
     * @retval NvError_InvalidAddress Illegal data buffer address (NULL).
     */
    NvError
    (*GenerateSamples)(
        NvCryptoRandomAlgoHandle AlgoHandle,
        NvU32 NumBytes,
        void *pBuffer);

    /**
     * Releases algorithm state and resources.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     */
    void
    (*ReleaseAlgorithm)(
        NvCryptoRandomAlgoHandle AlgoHandle);
    
} NvCryptoRandomAlgo;

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

/** @} */
#endif // INCLUDED_NVCRYPTO_RANDOM_H

