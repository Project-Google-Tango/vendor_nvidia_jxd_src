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
 * @b  Description: This file declares the interface for NvCrypto hash
 *     algorithms.
 */

#ifndef INCLUDED_NVCRYPTO_HASH_H
#define INCLUDED_NVCRYPTO_HASH_H

#include "nvcrypto_hash_defs.h"
#include "nverror.h"
#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup nvcrypto_hash_grp Hash Algorithm Interface
 *
 * This is the interface for the NvCrypto hash algorithms.
 *
 * The usage model for hash operations is as follows:
 *
 * -# Call NvCryptoHashAlgo::SetAlgoParams() to set algorithm-specific
 *        parameters.
 * -# (Optional) Determine optimal buffer alignment by calling
 *        NvCryptoHashAlgo::QueryOptimalBufferAlignment(), and then align the
 *        source buffer accordingly.
 * -# Determine algorithm's native block size via
 *        NvCryptoHashAlgo::QueryBlockSize().
 * -# For all message blocks except the last one, process the data by invoking
 *        NvCryptoHashAlgo::ProcessBlocks().
 * -# For the final message block, determine if any explicit padding must be
 *        added to the source buffer such that algorithm constraints on the
 *        final block size are met. Use
 *        NvCryptoHashAlgo::QueryIsExplicitPadding() to see if padding is
 *        needed, or use NvCryptoHashAlgo::QueryPadding() to determine the
 *        required padding size and to append the padding data to the end of
 *        the source buffer.
 * -# Finally, call \c ProcessBlocks() for the final block.
 * -# After the final block has been processed, call
 *        NvCryptoHashAlgo::QueryHash() to obtain the computed hash result.
 *        Or, if the operation mode for the hash algorithm is "verify" instead
 *        of "calculate" (call NvCryptoHashAlgo::QueryIsCalculate() to find out,
 *        if needed) then call NvCryptoHashAlgo::VerifyHash() to verify the
 *        computed hash value against the expected result.
 * -# After all hash processing has been completed, call
 *        NvCryptoHashAlgo::ReleaseAlgorithm() to release all state and
 *        resources associated with the hash calculations. The
 *        ::NvCryptoHashAlgoHandle is no longer valid after this call.
 *
 * @ingroup nvcrypto_modules
 * @{
 */

/// Forward declaration.
typedef struct NvCryptoHashAlgoRec *NvCryptoHashAlgoHandle;

/**
 * Allocate states (and function pointers) for the selected hash algorithm.
 *
 * The returned handle (\a *pHashAlgoHandle) must be passed in to all
 *     subsequent calls to the \c NvCryptoHash*() functions.
 *
 * @param HashAlgoType Specifies the type of hash algorithm to be computed.
 * @param pHashAlgoHandle A pointer to handle containing hash algorithm
 *     functions and state information.
 *
 * @retval NvSuccess Indicates the hash state allocated successfully.
 * @retval NvError_BadParameter Indicates an unknown algorithm type.
 * @retval NvError_InvalidAddress Indicates the pointer is illegal (NULL).
 */
NvError
NvCryptoHashSelectAlgorithm(
    NvCryptoHashAlgoType HashAlgoType,
    NvCryptoHashAlgoHandle *pHashAlgoHandle);
    
/**
 * Holds hash algorithm interface pointers and state information.
 */
typedef struct NvCryptoHashAlgoRec
{
    /**
     * Sets algorithm-specific parameters.
     *
     * @note The parameters structure is a union of per-algorithm settings.
     *     Only the union member for the selected algorithm must be set by the
     *     caller.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param pAlgoParams A pointer to the parameter structure.
     *
     * @retval NvSuccess Indicates algorithm parameters are set as specified.
     * @retval NvError_BadParameter Indicates algorithm parameter settings
     *     are illegal.
     * @retval NvError_InvalidAddress Indicates \a pAlgoParams is illegal
     *     (NULL).
     */
    NvError
    (*SetAlgoParams)(
        NvCryptoHashAlgoHandle AlgoHandle,
        NvCryptoHashAlgoParams *pAlgoParams);

    /**
     * Queries the type of algorithm assigned to the specified algorithm state
     *     handle.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param pAlgoType A pointer to algorithm type.
     *
     * @retval NvSuccess Indicates algorithm type query was successful.
     * @retval NvError_InvalidAddress Indicates \a pAlgoType is illegal (NULL).
     */
    NvError
    (*QueryAlgoType)(
        NvCryptoHashAlgoHandle AlgoHandle,
        NvCryptoHashAlgoType *pAlgoType);
    
    /**
     * Queries whether hash direction is calculate or verify.
     *
     * @note In many cases, the process for computing a hash value is the same
     *     as for verifying a hash value, except for the final comparison step. A
     *     distinction is made between these two processes in case they are
     *     different for some algorithms.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param pIsCalculate A pointer to NV_TRUE if algorithm computes a hash
     *     value, NV_FALSE if algorithm verifies a hash value.
     *
     * @retval NvSuccess Indicates algorithm direction query was successful.
     * @retval NvError_InvalidAddress Indicates \a pIsHashEncryptAlgo is an
     *     illegal (NULL).
     */
    NvError
    (*QueryIsCalculate)(
        NvCryptoHashAlgoHandle AlgoHandle,
        NvBool *pIsCalculate);
    
    /**
     * Queries whether padding is explicit.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param pIsExplicitPadding A pointer to NV_TRUE if passing is explicit,
     *     NV_FALSE if padding is implicit.
     *
     * @retval NvSuccess Indicates padding query was successful.
     * @retval NvError_InvalidAddress \a pIsExplicitPadding is illegal (NULL).
     */
    NvError
    (*QueryIsExplicitPadding)(
        NvCryptoHashAlgoHandle AlgoHandle,
        NvBool *pIsExplicitPadding);
    
    /**
     * Queries a native block size of hash algorithm.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param pBlockSize A pointer to hash's native block size, in bytes.
     *
     * @retval NvSuccess Indicates block size query was successful.
     * @retval NvError_InvalidAddress Indicates \a pBlockSize is illegal (NULL).
     */
    NvError
    (*QueryBlockSize)(
        NvCryptoHashAlgoHandle AlgoHandle,
        NvU32 *pBlockSize);
    
    /**
     * Queries padding size and padding bytes based on the number of payload
     * bytes processed to date.
     *
     * - If (upon entry) \a *pPaddingSize is less than the required padding
     *       size, \a *pPaddingSize is set to the required padding size and the
     *       \c NvError_InsufficientMemory error is returned. Thus, to query the
     *       padding size, simply set \a *pPaddingSize to zero before invoking
     *       this function. The value of \a pBuffer is ignored.
     * - If (upon entry) \a *pPaddingSize is equal to or greater than the
     *       padding size, then \a *pPaddingSize is set to the required padding
     *       size and the actual padding bytes are copied to \a pBuffer. If
     *       \a pBuffer is not ignored; if it is illegal (NULL), then
     *       \c NvError_InvalidAddress is returned.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param pPaddingSize A pointer to integer where the required padding size
     *     (in bytes) is to be stored.
     * @param pBuffer A pointer to buffer where padding bytes are to be stored.
     *
     * @retval NvSuccess Indicates padding size and bytes have been successfully
     *     returned.
     * @retval NvError_InsufficientMemory Indicates specified padding buffer
     *     size is too small to hold the padding data.
     * @retval NvError_InvalidAddress Indicates the pointer address is illegal
     *     (NULL).
     */
    NvError
    (*QueryPadding)(
        NvCryptoHashAlgoHandle AlgoHandle,
        NvU32 *pPaddingSize,
        NvU8 *pBuffer);
    
    /**
     * Queries padding size and padding bytes for a given payload size.
     *
     *     The functionality is the same as for NvCryptoHashAlgo::QueryPadding()
     *     except that the payload size is specified explicitly (rather than using
     *     the current payload size).
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param PayloadSize The payload size (in bytes) for which to compute
     *     padding.
     * @param pPaddingSize A pointer to the integer where the required padding
     *     size (in bytes) is to be stored.
     * @param pBuffer A pointer to the buffer where padding bytes are to be
     *     stored.
     *
     * @retval NvSuccess Indicates padding size and bytes have been successfully
     *     returned.
     * @retval NvError_InsufficientMemory Indicates specified padding buffer size
     *     is too small to hold the padding data.
     * @retval NvError_InvalidAddress Indicates a pointer address is illegal
     *     (NULL).
     */
    NvError
    (*QueryPaddingByPayloadSize)(
        NvCryptoHashAlgoHandle AlgoHandle,
        NvU32 PayloadSize,
        NvU32 *PaddingSize,
        NvU8 *pBuffer);

    /**
     * Queries optimal alignment for source buffer.
     *
     *     Some algorithms may be able to provide enhanced performance when the
     *     data buffers meet certain alignment constraints. To receive the
     *     enhanced performance, allocate data buffers on N-byte boundaries
     *     where:
     *     <pre>
     *         N = *pAlignment
     *     </pre>.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param pAlignment A pointer to alignment boundary, in bytes.
     *
     * @retval NvSuccess Indicates alignment boundary was returned successfully.
     * @retval NvError_InvalidAddress Indicates a pointer address is illegal
     *     (NULL).
     */
    NvError
    (*QueryOptimalBufferAlignment)(
        NvCryptoHashAlgoHandle AlgoHandle,
        NvU32 *pAlignment);

    /**
     * Processes data blocks using hash algorithm.
     *
     * \a NumBytes must always be a multiple of the algorithm's block size
     *     except (possibly) when the final block of the message is being
     *     processed. If the algorithm doesn't support a partial block for the
     *     final block of the message, then NvCryptoHashAlgo::QueryPadding()
     *     will return the number of padding bytes required to complete the
     *     final block.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param NumBytes Specifies the number of bytes to process; must normally
     *     be a multiple of the block size as described in detail above.
     * @param pSrcBuffer A pointer to input data buffer; must contain
     *     \a NumBytes of good data.
     * @param IsFirstBlock Specifies NV_TRUE if source buffer contains first
     *     block in message, else NV_FALSE.
     * @param IsLastBlock Specifies NV_TRUE if source buffer contains final
     *     block in message, else NV_FALSE.
     *
     * @retval NvSuccess Indicates data bytes have been processed successfully.
     * @retval NvError_InvalidAddress Indicates an illegal data buffer address
     *     (NULL).
     * @retval NvError_InvalidSize Indicates \a NumBytes is not a multiple of
     *     the block size and the specified algorithm doesn't allow partial
     *     blocks.
     */
    NvError
    (*ProcessBlocks)(
        NvCryptoHashAlgoHandle AlgoHandle,
        NvU32 NumBytes,
        const void *pSrcBuffer,
        NvBool IsFirstBlock,
        NvBool IsLastBlock);

    /**
     * Queries computed hash value.
     *
     * - If (upon entry) \a *pHashSize is less than the size of the hash result
     *       computed by this algorithm, then \a *pHashSize is set to actual
     *       hash result size in bytes and the NvError_InsufficientMemory error
     *       is returned. Thus, to query the hash result size, simply set
     *       \a *pHashSize to zero before invoking this function. The value of
     *       \a pBuffer is ignored.
     * - If (upon entry) \a *pHashSize is equal to or greater than the size of
     *       the hash result computed by this algorithm, then \a *pHashSize is
     *       set to the actual hash result size and the hash result itself is
     *       copied into \a pBuffer. \a pBuffer is not ignored in this case; if
     *       it is illegal (NULL), then \c NvError_InvalidAddress is returned.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param pHashSize A pointer to an integer where the hash result size (in
     *     bytes) is to be stored.
     * @param pBuffer A pointer to buffer where the hash result bytes are to
     *     be stored.
     *
     * @retval NvSuccess Indicates hash result size and bytes have been
     *     successfully returned.
     * @retval NvError_InsufficientMemory Indicates the specified buffer size
     *     is too small to hold the hash result data.
     * @retval NvError_InvalidAddress Indicates a pointer address is illegal
     *     (NULL).
     * @retval NvError_InvalidState Indicates the hash result hasn't been
     *     computed yet.
     */
    NvError
    (*QueryHash)(
        NvCryptoHashAlgoHandle AlgoHandle,
        NvU32 *pHashSize,
        NvU8 *pBuffer);
    
    /**
     * Verifies computed hash value against expected value.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param pExpectedHashValue A pointer to buffer containing expected hash
     *     value; must be the same size as computed hash value, as reported
     *     by NvCryptoHashAlgo::QueryHash().
     * @param pIsValid A pointer to NV_TRUE means computed hash value matches
     *     expected hash value, else NV_FALSE.
     *
     * @retval NvSuccess Indicates hash comparison has been performed; see
     *     \a pIsValid for result.
     * @retval NvError_InvalidAddress Indicates a pointer address is illegal
     *     (NULL).
     * @retval NvError_InvalidState Indicates the hash result hasn't been
     *     computed yet.
     */
    NvError
    (*VerifyHash)(
        NvCryptoHashAlgoHandle AlgoHandle,
        NvU8 *pExpectedHashValue,
        NvBool *pIsValid);

    /**
     * Releases algorithm state and resources.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     */
    void
    (*ReleaseAlgorithm)(
        NvCryptoHashAlgoHandle AlgoHandle);
    
} NvCryptoHashAlgo;

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

/** @} */
#endif // INCLUDED_NVCRYPTO_HASH_H

