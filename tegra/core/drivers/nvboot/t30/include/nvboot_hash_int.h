/*
 * Copyright (c) 2008 - 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file nvboot_hash_int.h
 *
 * Internal NvBootHash interface for NvBoot
 *
 * NvBootHash is NVIDIA's interface for computing the AES-CMAC keyed hash
 * function.
 *
 * The AES-CMAC algorithm is defined in RFC 4493 published by the Internet
 * Engineering Task Force (IETF).  The RFC is available at the following web
 * site, among others --
 *
 *    http://tools.ietf.org/html/rfc4493
 *
 * The 128-bit variant is implemented here, i.e., AES-CMAC-128.  This variant
 * employs an underlying AES-128 encryption computataion.
 *
 * This implementation only handles messages that are a multiple of 16 bytes in
 * length.  The intent is to compute the hash value for AES-encrypted messages
 * which (by definition) are always a multiple of 16 bytes in length.  Hence,
 * the K2 constant will never be used and therefore is not even computed.
 * Similarly, there's no provision for padding packets since they're already
 * required to be a multiple of 16 bytes in length.
 *
 * The input message is computed one 16-byte block at a time.  The caller must
 * provide the AES key as well as buffers for the K1 parameter and the hash
 * result (also used to store intermediate values).
 *
 * To compute the hash for an N-block message --
 * 1. call NvBootAesInitializeEngine() to initialize AES engines
 * 2. call NvBootAesSetKeyAndIv() to set AES key and clear IV to zeroes
 * 3. call NvBootAesSelectKeyIvSlot() to select the key slot
 * 4. call NvBootHashGenerateSubkeys() to prepare for the AES-CMAC calculation
 * 5. call NvBootHashBlock() to start the hash calculation for a single message
 *    block; this call doesn't block.  Poll on NvBootAesIsEngineBusy() until
 *    the block operation has completed.  Be sure to flag the first and last
 *    blocks.
 * 6. repeat step (6) for each of the N blocks.  After the last iteration, the
 *    hash result can be found in the *pHash buffer
 *
 */

#ifndef INCLUDED_NVBOOT_HASH_INT_H
#define INCLUDED_NVBOOT_HASH_INT_H

#include "nvcommon.h"
#include "t30/nvboot_aes.h"
#include "t30/nvboot_error.h"
#include "t30/nvboot_hash.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define NVBOOT_BLOCKWISE_HASHING 0


/**
 * Compute the constants needed for AES-CMAC operation.  Assumes AES key slot
 * is already selected and that it contains the desired key and an initial
 * vector of zero.
 *
 * @return void
 *
 * @param engine AES engine (input)
 * @param pK1 pointer to buffer for K1 parameter (output)
 *
 * Note: the K2 parameter isn't needed since this implementation restricts
 *       the message length to be a multiple of 16 bytes in length
 */
void
NvBootHashGenerateSubkeys (
    NvBootAesEngine engine,
    NvBootAesIv *pK1);

/**
 * Hash a block (16 bytes) of data.
 *
 * @param engine AES engine (input)
 * @param slot key slot (input)
 * @param first set to NvTrue for first block in message, else NvFalse (input)
 * @param last set to NvTrue for last block in message, else NvFalse (input)
 * @param pK1 pointer to K1 parameter, needed only for last block (input)
 * @param pData pointer to block of data to be hashed (input)
 * @param pHash pointer to hash result (output)
 *
 * @return void
 *
 * Note: this implementation restricts the message length to be a multiple
 *       of 16 bytes; otherwise, the amount of data in the final "block"
 *       would be needed (to compute the padding), along with a 16-byte
 *       scratch buffer to hold the padded block, and the K2 parameter
 */

#if NVBOOT_BLOCKWISE_HASHING
void
NvBootHashBlock (
    NvBootAesEngine engine,
    NvBool first,
    NvBool last,
    NvBootAesIv *pK1,
    NvU8 *pData,
    NvBootHash *pHash);

#endif
/**
 * Hash 16 byte-blocks of data.
 *
 * @param engine AES engine (input)
 * @param slot key slot (input)
 * @param pK1 pointer to K1 parameter, needed only for last block (input)
 * @param pData pointer to block of data to be hashed (input)
 * @param pHash pointer to hash result (output)
 *              Though parameter pHash is redundant right now, this may be utilized when hashing is not
 *              used in hardware NvBootAesStartEngine aes code asserts when either source or destination
 *              are NULL
 * @param numBlocks number of blocks over which the has is to be computed
 * @param firstChunk first chunk of the data stream to be hashed. A chunk may contain one or
 *              several blocks.
 * @param lastChunk last block(s) of the data stream to be hashed.
 * @return void
 *
 * Note: this implementation restricts the message length to be a multiple
 *       of 16 bytes; otherwise, the amount of data in the final "block"
 *       would be needed (to compute the padding), along with a 16-byte
 *       scratch buffer to hold the padded block, and the K2 parameter
 */
void
NvBootHashBlocks (
    NvBootAesEngine engine,
    NvBootAesIv *pK1,
    NvU8 *pData,
    NvBootHash *pHash,
    NvU32 numBlocks,
    NvBool firstChunk,
    NvBool lastChunk);


#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVBOOT_HASH_INT_H
