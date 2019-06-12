/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvml_hash.h"
#include "nvml_aes.h"
#include "t30/nvboot_config.h"
#include "nvboot_util_int.h"

#define NVBOOT_HASH_ENGINE    NVBOOT_SBK_ENGINEB
#define NVBOOT_HASH_SLOT      NVBOOT_SBK_ENCRYPT_SLOT
/**
 * @file Nvml_hash.c
 *
 * NvBootHash interface for NvBoot
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
 * employs an underlying AES-128 encryption computation.
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
 * 1. call NvBootHashInitialize() to prepare for the AES-CMAC calculation
 * 2. call NvBootHashBlock() to start the hash calculation for a single message
 *    block; this call doesn't block.  Poll on NvBootHashDone() until the block
 *    operation has completed.  Be sure to flag the first and last blocks.
 * 3. repeat step (2) for each of the N blocks.  After the last iteration, the
 *    hash result can be found in the *pHash buffer
 *
 */

#define AES_BLOCK_LENGTH_BYTES (NVBOOT_AES_BLOCK_LENGTH*sizeof(NvU32))

/**
 * Rb parameter value, from the AES-CMAC spec
 */

#define HASH_CMAC_RB 0x87

/**
 * Extract most significant bit (MSB).  Return 0 if the MSB is zero,
 * 1 if the MSB is one.
 */

#define HASH_IS_MSB_SET(x)  ( (x) >> (8*sizeof(x)-1) )

void
NvMlHashGenerateSubkeys (
    NvBootAesEngine engine,
    NvBootAesIv *pK1)
{
    NvU8 src[AES_BLOCK_LENGTH_BYTES];
    NvU32 i;
    NvU32 carry;
    NvU32 msbL;
    NvU8 *p;

    // wait for engine to become idle
    while ( NvMlAesIsEngineBusy(engine) )
        ;

    // set initial vector to all zeroes

    NvMlAesClearIv(engine, NVBOOT_HASH_SLOT);

    // compute L = AES(key, zeroes); store result in *pK1

    NvBootUtilMemset(src, 0x0, sizeof(src));

    NvMlAesStartEngine(engine, 1 /* one block */, src, (NvU8*)pK1->iv,
                         NV_TRUE);

    // wait for engine to become idle
    while ( NvMlAesIsEngineBusy(engine) )
        ;
    /* This is important because if hashing is enabled in engine
     * all the while, then subkey generated will have to be read
     * into memory explicitly.
     */
    if (NvMlAesIsHashingEnabled(engine))
    {
        NvMlAesReadHashOutput(engine, (NvU8 *)pK1->iv);
    }

    // compute K1 as follows --
    //
    // if MSB(L) is equal to 0
    // then K1 = L << 1
    // else K1 = (L << 1) XOR Rb;
    //
    // where Rb = 0x87

    // AES-CMAC computes an IV from some computed ciphertext.  Use NvU8's to
    // perform the computation since that's the form of the ciphertext (and
    // the plaintext that the IV will eventually be XOR'ed with).

    p = (NvU8 *)pK1->iv;
    msbL = HASH_IS_MSB_SET(*p);

    p[0] <<= 1;
    for (carry=0, i=1; i<AES_BLOCK_LENGTH_BYTES; i++)
    {
        carry = HASH_IS_MSB_SET(p[i]);
        p[i-1] |= carry;
        p[i] <<= 1;
    }

    if (msbL)
    {
        p = (NvU8 *)pK1->iv;
        p[AES_BLOCK_LENGTH_BYTES-1] ^= HASH_CMAC_RB;
    }

}

void
NvMlHashBlocks (
    NvBootAesEngine engine,
    NvBootAesIv *pK1,
    NvU8 *pData,
    NvBootHash *pHash,
    NvU32 numBlocks,
    NvBool firstChunk,
    NvBool lastChunk)
{
    // pHash becomes redundant parameter since hash output is not going to
    // be read until the last chunk l is also hashed.This is just passed to the
    // AES_startEngine so that it doesn't complain on the destination being
    // nullDestination address has no meaning here since output of AES shall
    // not be routed to the destination address.

    NvU32 *Src = (NvU32 *)pData;
    NvU32  Offset;
    NvU32 i;
    NvU32 lastBuffer[NVBOOT_AES_BLOCK_LENGTH];

    // Handling of first and last blocks gets reduced because we would like all
    // the blocks of the chunk to be sent as a stream of data to AES. Here
    // breaking the block and K1 XOR IV and writing it back, as was the case
    // earlier, may not be the best way.
    if (numBlocks)
    {
        if( firstChunk)
        {
                 //When first chunk, clear the IV
            NvMlAesClearIv(engine, NVBOOT_HASH_SLOT);
        }

        /* When the last chunk arrives, break the chunk of m AES blocks into
         * m-1 and 1. Compute hash over all the m-1 blocks in one go. Then
         * compute hash only over last block for which lastBuffer<-IV+m'th
         * block, where  lastBuffer is a local buffer of size 1 AES block
         * allocated for the purpose. Since the code for NvBootHashBlocks will
         * wait till hash is computed over the m-1 blocks when lastChunk is
         * true, it is going to create issues when we want to parallelize any
         * two operations using the two AES engines. Therefore, the caller for
         * NvBootHashBlocks should take care of breaking the last chunk into
         * m-1 and 1 blocks. If it doesn't take care of this, the implementation
         * of NvBootHashBlocks will default take care of it but this might block
         * the flow for most part of the operation.
         */
        if(lastChunk)
        {
            if(numBlocks > 1)
            {
                 // wait for engine to become idle
                // This wait is a must since the same engine is going to be
                // used again. The next request tothe same engine needs
                // to be  blocked on this.
                while ( NvMlAesIsEngineBusy(engine) )
                    ;
                // encrypt the input data; the resulting
                // ciphertext is the hash value
           NvMlAesStartEngine(engine, (numBlocks-1), pData,
                                     (NvU8 *)pHash->hash, NV_TRUE);
            }
            Offset = (numBlocks - 1) * NVBOOT_AES_BLOCK_LENGTH;

            //XOR the key K1 with the last block of message (recovery code)
            for (i=0; i<NVBOOT_AES_IV_LENGTH; i++)
            {
                lastBuffer[i] = pK1->iv[i] ^ Src[Offset + i];
            }

            // wait for engine to become idle

            // This wait is a must since the same engine is going to be used
            // again. The next request tothe same engine needs to be blocked
            // on this.
            while ( NvMlAesIsEngineBusy(engine) )
                ;
            // encrypt the input data; the resulting
            // ciphertext is the hash value
            NvMlAesStartEngine(engine, 1 /* last block */, (NvU8 *)lastBuffer,
                                 (NvU8 *)pHash->hash, NV_TRUE);
        }
        else
        {
            // wait for engine to become idle

            // This wait is a must since the same engine is going to be used
            // again. The next request to the same engine needs to be blocked
            // on this.
            while ( NvMlAesIsEngineBusy(engine) )
                ;

            // encrypt the input data; the resulting
            // ciphertext is the hash value
            NvMlAesStartEngine(engine, numBlocks, pData, (NvU8 *)pHash->hash,
                                 NV_TRUE);

        }

    }
}


