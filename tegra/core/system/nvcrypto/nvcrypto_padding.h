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
 * @file nvcrypto_padding.h
 * @brief <b> Nv Cryptography Framework.</b>
 *
 * @b Description: This file declares the interface for NvCrypto padding
 *    algorithms.
 */

#ifndef INCLUDED_NVCRYPTO_PADDING_H
#define INCLUDED_NVCRYPTO_PADDING_H

#include "nvcrypto_common_defs.h"
#include "nverror.h"
#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
//{
#endif

/**
 * Query whether padding is explicit
 *
 * @param PaddingType Type of padding to apply
 * @param pIsExplicit pointer to NV_TRUE if padding is explicit, NV_FALSE if
 * padding is implicit
 *
 * @retval NvSuccess Padding query is successful
 * @retval NvError_InvalidAddress pIsExplicitPadding is illegal (NULL)
 * @retval NvError_BadParameter Unknown padding type
 */
NvError
NvCryptoPaddingQueryIsExplicit(
    NvCryptoPaddingType PaddingType,
    NvBool *pIsExplicit);

/**
 * Query padding size based on payload size and algorithm block size.  This
 * routine ignores whether the padding data will be used explicitly or
 * implicitly.
 *
 * In most cases, padding size calculations do not require explicit knowledge of
 * the payload size.  Crypto algorithms often also do not require explicit
 * knowledge of the cumulative size of the payload.  To avoid forcing crypto
 * algorithms to track the explicit payload size when it is otherwise not
 * needed, this routine relies instead on the following two pieces of
 * information --
 *
 * 1. whether the payload is empty (i.e., payload size is zero)
 * 2. payload size modulo block size
 *
 * Note that the values of these parameters remain within small fixed bounds no
 * matter how large the actual payload size becomes.
 *
 * Of course, if the explicit payload size is known for some reason, calculation
 * of the above two parameters is straightforward.
 *
 * @param PaddingType Type of padding to apply
 * @param BlockSize Crypto algorithm's block size (in bytes)
 * @param IsEmptyPayload NV_TRUE if payload size is zero, else NV_FALSE
 * @param PayloadSizeModuloBlockSize size of payload (in bytes) modulo the
 * crypto algorithm's block size (BlockSize) (in bytes)
 * @param pPaddingSize pointer to integer where the required padding size
 * (in bytes) is to be stored
 *
 * @retval NvSuccess Padding size successfully returned
 * @retval NvError_BadParameter Unknown padding type
 * @retval NvError_InvalidAddress Pointer address is illegal (NULL)
 */
NvError
NvCryptoPaddingQuerySize(
    NvCryptoPaddingType PaddingType,
    NvU32 BlockSize,
    NvU32 IsEmptyPayload,
    NvU32 PayloadSizeModuloBlockSize,
    NvU32 *pPaddingSize);

/**
 * Query padding data bytes
 *
 * If (upon entry) *pPaddingSize is less than the required padding size,
 * *pPaddingSize is set to the required padding size and the error
 * NvError_InsufficientMemory is returned.  Thus, to query the padding size,
 * simply set *pPaddingSize to zero before invoking this function.  The
 * value of pBuffer is ignored.
 *
 * If (upon entry) *pPaddingSize is equal to or greater than the padding
 * size, then *pPaddingSize is set to the required padding size and the
 * actual padding bytes are copied to pBuffer.  If pBuffer is not ignored;
 * if it is illegal (NULL), then NvError_InvalidAddress is returned.
 *
 * @param PaddingType Type of padding to apply
 * @param PaddingSize Padding size (in bytes)
 * @param BufferSize Size of buffer (in bytes) where padding data is to be
 * stored
 * @param pBuffer Pointer to buffer where padding data is to be stored
 *
 * @retval NvSuccess Padding size and bytes have been successfully returned
 * @retval NvError_BadParameter Unknown padding type
 * @retval NvError_InvalidAddress Pointer address is illegal (NULL)
 * @param AlgoHandle handle to the algorithm state information
 * @param pPaddingSize pointer to integer where the required padding size
 * (in bytes) is to be stored
 * @param pBuffer pointer to buffer where padding bytes are to be stored
 *
 * @retval NvSuccess Padding data successfully returned
 * @retval NvError_InsufficientMemory Specified padding buffer size is too
 * small to hold the padding data
 * @retval NvError_InvalidAddress Pointer address is illegal (NULL)
 */
NvError
NvCryptoPaddingQueryData(
    NvCryptoPaddingType PaddingType,
    NvU32 PaddingSize,
    NvU32 BufferSize,
    NvU8 *pBuffer);

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

#endif // INCLUDED_NVCRYPTO_PADDING_H

