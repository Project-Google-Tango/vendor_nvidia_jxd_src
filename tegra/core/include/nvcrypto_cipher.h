/*
 * Copyright (c) 2008 - 2013 NVIDIA Corporation.  All rights reserved.
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
 * @b  Description: This file declares the interface for NvCrypto cipher
 *     algorithms.
 */

#ifndef INCLUDED_NVCRYPTO_CIPHER_H
#define INCLUDED_NVCRYPTO_CIPHER_H

#include "nvcrypto_cipher_defs.h"
#include "nverror.h"
#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup nvcrypto_cipher_grp Cipher Algorithms
 *
 * This is the interface for the NvCrypto cipher algorithms.
 *
 * The usage model for cipher operations is as follows:
 *
 * -# Call NvCryptoCipherAlgo::SetAlgoParams() to set algorithm-specific
 *        parameters.
 * -# (Optional) Determine the optimal buffer alignment by calling
 *        NvCryptoCipherAlgo::QueryOptimalBufferAlignment(), and then aligning
 *        source and destination buffers accordingly.
 * -# Determine the algorithm's native block size via
 *        a call to NvCryptoCipherAlgo::QueryBlockSize().
 * -# For all message blocks except the last one, process the data by invoking
 *        NvCryptoCipherAlgo::ProcessBlocks().
 * -# For the final message block, determine if any explicit padding must be
 *        added to the source buffer such that algorithm constraints on the
 *        final block size are met. Use
 *        NvCryptoCipherAlgo::QueryIsExplicitPadding() to see if any padding is
 *        needed, or use NvCryptoCipherAlgo::QueryPadding() to determine the
 *        required padding size and to append the padding data to the end of
 *        the source buffer.
 * -# Finally, call \c ProcessBlocks() for the final block.
 * -# After the final block has been processed, call
 *        NvCryptoCipherAlgo::ReleaseAlgorithm() to release all state and
 *        resources associated with the cipher calculations. The
 *        ::NvCryptoCipherAlgoHandle is no longer valid after this call.
 *
 * @ingroup nvcrypto_modules
 * @{
 */

/// Forward declaration.
typedef struct NvCryptoCipherAlgoRec *NvCryptoCipherAlgoHandle;

/**
 * Allocate states (and function pointers) for the selected cipher algorithm.
 *
 * The returned handle (\a *pCipherAlgoHandle) must be passed in to all
 *     subsequentcalls to the \c NvCryptoCipher*() functions.
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
NvCryptoCipherSelectAlgorithm(
    NvCryptoCipherAlgoType CipherAlgoType,
    NvCryptoCipherAlgoHandle *pCipherAlgoHandle);

/**
 * Holds the cipher algorithm interface pointers and state information.
 */
typedef struct NvCryptoCipherAlgoRec
{
    /**
     * Sets algorithm-specific parameters.
     *
     * @note The parameters structure is a union of per-algorithm settings.
     *     Only the union member for the selected algorithm must be set by the
     *     caller.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param pAlgoParams A pointer to parameter structure.
     *
     * @retval NvSuccess Indicates algorithm parameters were set as specified.
     * @retval NvError_BadParameter Indicates algorithm parameter settings
     *     are illegal.
     * @retval NvError_InvalidAddress Indicates \a pAlgoParams is illegal
     *     (NULL).
     */
    NvError
    (*SetAlgoParams)(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvCryptoCipherAlgoParams *pAlgoParams);

    /**
     * Queries the type of algorithm assigned to specified algorithm state
     *     handle.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param pAlgoType A pointer to algorithm type.
     *
     * @retval NvSuccess Indicates algorithm type query was successful.
     * @retval NvError_InvalidAddress Indicates \a pAlgoType is illegal
     *      (NULL).
     */
    NvError
    (*QueryAlgoType)(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvCryptoCipherAlgoType *pAlgoType);
    
    /**
     * Queries whether cipher direction is encrypt.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param pIsEncrypt A pointer to NV_TRUE if encrypt algorithm,
     *     NV_FALSE if decrypt algorithm.
     *
     * @retval NvSuccess Indicates algorithm direction query was successful.
     * @retval NvError_InvalidAddress Indicates \a pIsEncrypt
     *      is illegal (NULL).
     */
    NvError
    (*QueryIsEncrypt)(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvBool *pIsEncrypt);
    
    /**
     * Queries whether padding is explicit.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param pIsExplicitPadding A pointer to NV_TRUE if passing is explicit,
     *     NV_FALSE if padding is implicit.
     *
     * @retval NvSuccess Indicates padding query was successful.
     * @retval NvError_InvalidAddress Indicates \a pIsExplicitPadding is
     *     illegal (NULL).
     */
    NvError
    (*QueryIsExplicitPadding)(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvBool *pIsExplicitPadding);
    
    /**
     * Queries native block size of cipher algorithm.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param pBlockSize A pointer to cipher's native block size, in bytes.
     *
     * @retval NvSuccess Indicates block size query wsa successful.
     * @retval NvError_InvalidAddress Indicates \a pBlockSize is illegal (NULL).
     */
    NvError
    (*QueryBlockSize)(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvU32 *pBlockSize);
    
    /**
     * Queries padding size and padding bytes based on the number of payload
     *     bytes processed to date.
     *
     * - If (upon entry) \a *pPaddingSize is less than the required padding
     *       size, \a *pPaddingSize is set to the required padding size and the 
     *       \c NvError_InsufficientMemory error is returned. Thus, to query the
     *       padding size, simply set \a *pPaddingSize to zero before invoking
     *       this function. The value of \a pBuffer is ignored.
     * - If (upon entry) \a *pPaddingSize is equal to or greater than the
     *      padding size, then \a *pPaddingSize is set to the required padding
     *      size and the actual padding bytes are copied to \a pBuffer. If
     *      \a pBuffer is not ignored; if it is illegal (NULL), then
     *      \c NvError_InvalidAddress is returned.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param pPaddingSize A pointer to the integer where the required padding
     *     size (in bytes) is to be stored.
     * @param pBuffer A pointer to buffer where padding bytes are to be stored.
     *
     * @retval NvSuccess Indicates padding size and bytes have been successfully
     *     returned.
     * @retval NvError_InsufficientMemory Indicates the specified padding buffer
     *     size is too small to hold the padding data.
     * @retval NvError_InvalidAddress Indicates the pointer address is illegal
     *     (NULL).
     */
    NvError
    (*QueryPadding)(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvU32 *pPaddingSize,
        NvU8 *pBuffer);
    
    /**
     * Queries padding size and padding bytes for a given payload size.
     *
     * The functionality is the same as for NvCryptoCipherAlgo::QueryPadding()
     *     except that the payload size is specified explicitly (rather than
     *     using the current payload size).
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param PayloadSize The payload size (in bytes) for which to compute
     *     padding.
     * @param pPaddingSize A pointer to integer where the required padding size
     *     (in bytes) is to be stored.
     * @param pBuffer A pointer to the buffer where padding bytes are to be
     *     stored.
     *
     * @retval NvSuccess Indicates oadding size and bytes have been successfully
     *     returned.
     * @retval NvError_InsufficientMemory Indicates the specified padding buffer
     *     size is too small to hold the padding data.
     * @retval NvError_InvalidAddress Indicates the pointer address is illegal
     *     NULL).
     */
    NvError
    (*QueryPaddingByPayloadSize)(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvU32 PayloadSize,
        NvU32 *PaddingSize,
        NvU8 *pBuffer);

    /**
     * Queries optimal alignment for source and destination buffers.
     *
     * Some algorithms may be able to provide enhanced performance when the data
     *     buffers meet certain alignment constraints. To receive the enhanced
     *     performance, allocate data buffers on N-byte boundaries where:
     *     <pre>
     *         N = *pAlignment
     *     </pre>.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param pAlignment A pointer to the alignment boundary, in bytes.
     *
     * @retval NvSuccess Indicates alignment boundary was returned successfully.
     * @retval NvError_InvalidAddress Indicates the pointer address is illegal
     *      (NULL).
     */
    NvError
    (*QueryOptimalBufferAlignment)(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvU32 *pAlignment);

    /**
     * Processes data blocks using cipher algorithm.
     *
     * The size of the input data and output data is always the same.  If
     *     padding is needed, the padding data can be obtained by calling
     *     NvCryptoCipherAlgo::QueryPadding().
     *
     * \a NumBytes must always be a multiple of the algorithm's block size
     *     except (possibly) when the final block of the message is being
     *     processed.  If the algorithm doesn't support a partial block for
     *     the final block of the message, then
     *     NvCryptoCipherAlgo::QueryPadding() will return the number of
     *     padding bytes required to complete the final block.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     * @param NumBytes Specifies the number of bytes to process; must normally
     *     be a multiple of the block size as described in detail above.
     * @param pSrcBuffer A pointer to input data buffer; must contain
     *     \a NumBytes of good data.
     * @param pDstBuffer A pointer to output data buffer; must be able to hold
     *     \a NumBytes of data.
     * @param IsFirstBlock Specifies NV_TRUE if source buffer contains first
     *     block in message, else NV_FALSE.
     * @param IsLastBlock Specifies NV_TRUE if source buffer contains final
     *     block in message, else NV_FALSE.
     *
     * @retval NvSuccess Indicates data bytes have been processed successfully.
     * @retval NvError_InvalidAddress Indicates an illegal data buffer address
     *     (NULL).
     * @retval NvError_InvalidSize Indicates \a NumBytes is not a multiple of
     *     the block size and the specified algorithm doesn't allow
     *     partial blocks.
     */
    NvError
    (*ProcessBlocks)(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvU32 NumBytes,
        const void *pSrcBuffer,
        void *pDstBuffer,
        NvBool IsFirstBlock,
        NvBool IsLastBlock);

    /**
     * Releases algorithm state and resources.
     *
     * @param AlgoHandle A handle to the algorithm state information.
     */
    void
    (*ReleaseAlgorithm)(
        NvCryptoCipherAlgoHandle AlgoHandle);

    /**
     * Computes the Cipher-based MAC (CMAC) hash for the input data.
     *
     * @param Key A crypto key for computing the hash.
     * @param KeyLen The length of the Key used.
     * @param pSrcBuf A pointer to the input data buffer to be signed.
     * @param pDstBuf A pointer to the output hash.
     * @param SrcBufSize The length of the input buffer.
     * @param IsFirstChunk A flag that specifies whether the given input
     *     buffer is the first chunk.
     * @param IsLaskChunk A flag that specifies whether the input buffer
     *     is the last chunk.
     * @param IsSbkKey A flag that specifies whether the key to be used is
     *     SecureBootKey.
     */
    NvError
    (*SePerformCmacOperation)(
        NvU8* Key, NvU32 KeyLen, NvU8 *pSrcBuf, NvU8 *pDstBuf, NvU32 SrcBufSize,
        NvBool IsFirstChunk, NvBool IsLastChunk, NvBool IsSbkKey);

} NvCryptoCipherAlgo;

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

/** @} */
#endif // INCLUDED_NVCRYPTO_CIPHER_H

