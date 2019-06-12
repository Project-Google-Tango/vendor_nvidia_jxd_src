/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file nvml_aes.h
 *
 * NvMlAes interface for NvMl
 *
 * NvMlAes is NVIDIA's interface for AES encryption, decryption, and key
 * management.
 *
 */

#ifndef INCLUDED_NVML_AES_H
#define INCLUDED_NVML_AES_H

#include "nvcommon.h"
#include "t30/nvboot_aes.h"
#include "t30/nvboot_error.h"
#include "t30/nvboot_config.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Initializes the AES engineA and engineB
 *
 * @param None
 *
 * @return None
 */
void NvMlAesInitializeEngine(void);

/**
 * Over-writes the key schedule and Initial Vector in the in the specified
 * key slot with zeroes.
 * Convenient for preventing subsequent callers from gaining access
 * to a previously-used key.
 *
 * @param engine AES engine
 * @param slot key slot to clear
 *
 * @return None
 */
void NvMlAesClearKeyAndIv(NvBootAesEngine engine, NvBootAesKeySlot slot);


/**
 * Over-writes the initial vector in the specified AES engine with zeroes.
 * Convenient to prevent subsequent callers from gaining access to a
 * previously-used initial vector.
 *
 * @param engine AES engine
 *
 * @return None
 */
void NvMlAesClearIv(NvBootAesEngine engine,NvBootAesKeySlot slot);

/**
 * Computes key schedule for the given key, then loads key schedule and
 * initial vector into the specified key slot.
 *
 * @param engine AES engine
 * @param slot key slot to load
 * @param pointer to the key
 * @param pointer to the iv
 * @param IsEncryption If set to NV_TRUE indicates key schedule computation
 *        id for encryption else for decryption.
 *
 * @return None
 */
void
NvMlAesSetKeyAndIv(
    NvBootAesEngine engine,
    NvBootAesKeySlot slot,
    NvBootAesKey *pKey,
    NvBootAesIv *pIv,
    NvBool IsEncryption);

/**
 * Loads an initial vector into the specified AES engine for using it
 * during encryption or decryption.
 *
 * @param engine AES engine
 * @param pointer to the initial vector
 *
 * @return None
 */
void NvMlAesSetIv(NvBootAesEngine engine, NvBootAesKeySlot  slot, NvBootAesIv *pIv);

/**
 * Retrieves the initial vector for the specified AES engine. This will get the
 * IV that is stored in the specified engine registers. To get the original Iv
 * stored in the key table slot, NvBootAesSelectKeyIvSlot() API must be called
 * before calling NvBootAesGetIv().
 *
 * @param engine AES engine
 * @param pointer to the initial vector
 *
 * @return None
 */
void NvMlAesGetIv(NvBootAesEngine engine, NvBootAesKeySlot  slot, NvBootAesIv *pIv);

/**
 * Selects the key and iv from the internal key table for a specified key slot.
 *
 * @param engine AES engine
 * @param slot key slot for which key and IV needs to be selected
 *
 * @return None
 */
void NvMlAesSelectKeyIvSlot(NvBootAesEngine engine, NvBootAesKeySlot slot);


/**
 * Encrypt/Decrypt a specified number of blocks of cyphertext using
 * Cipher Block Chaining (CBC) mode.  A block is 16 bytes.
 * This is non-blocking API and need to call NvBootAesEngineIdle()
 * to check the engine status to confirm the AES engine operation is
 * done and comes out of the BUSY state.
 * Also make sure before calling this API engine must be IDLE.
 *
 * @param engine AES engine
 * @param nblocks number of blocks of ciphertext to process.
 *        One block is 16 bytes. Max number of blocks possible = 0xFFFFF.
 * @param src pointer to nblock blocks of ciphertext/plaintext depending on the
          IsEncryption status; ciphertext/plaintext is not modified (input)
 * @param dst pointer to nblock blocks of cleartext/ciphertext (output)
 *        depending on the IsEncryption;
 * @param IsEncryption If set to NV_TRUE indicates AES engine to start
 *        encryption on the source data to give cipher text else starts
 *        decryption on the source cipher data to give plain text.
 *
 * @return None
 */
void
NvMlAesStartEngine(
    NvBootAesEngine engine,
    NvU32 nblocks,
    NvU8 *src,
    NvU8 *dst,
    NvBool IsEncryption);

/**
 * Reports whether or not the specified AES engine is busy.
 *
 * @param engine AES engine
 *
 * @return NV_TRUE, if AES engine is Busy doing operation.
 * @return NV_FALSE, if AES engine is Idle.
 */
NvBool NvMlAesIsEngineBusy(NvBootAesEngine engine);

/**
 * Enables Hashing in the engine.
 * @param engine AES engine
 * @param enbHashing indicates whether hashing is to be enabled or disabled in
 *          the engine.
 * When hashing is enabled within the engine, output of AES is directed to
 * HASH_RESULT registers and not the destination address provided.
 *
 * It's the responsibility of the client to restore the state of hash enabling
 * after usage.
 *
 * @return None
 */
void NvMlAesEnableHashingInEngine(NvBootAesEngine engine, NvBool enbHashing);

/**
 * Reports whether hash is enabled in the specified AES engine or not.
 *
 * @param engine AES engine
 *
 * @return NV_TRUE, if AES engine has hash enabled.
 * @return NV_FALSE, if AES engine doesn't have hash enabled.
 */
NvBool NvMlAesIsHashingEnabled(NvBootAesEngine engine);

/**
 * API to read the output from Hash Results registers
 * This should be used only when hashing is enabled. If hashing is not enabled,
 * this API shall return all 0's as the output.
 * @param engine AES engine
 * @param pHashResult buffer in which hash output is to be read.
 *
 * @return None
 */
void NvMlAesReadHashOutput(NvBootAesEngine engine, NvU8 *pHashResult);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVML_AES_H

