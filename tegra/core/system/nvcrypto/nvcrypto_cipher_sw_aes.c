/*
 * Copyright (c) 2008-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcrypto_cipher_aes.h"
#include "nvcrypto_padding.h"
#include "nvos.h"
#include "nvassert.h"
#include "nvaes_ref.h"

enum
{
    NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES = 16
};

/// forward declaration
typedef struct NvCryptoCipherAlgoAesRec *NvCryptoCipherAlgoAesHandle;

typedef struct NvCryptoCipherAlgoAesRec
{
    /**
     * state information required for all cipher algorithms
     */

    /// standard entry points
    NvCryptoCipherAlgo CipherAlgo;

    /**
     * state information specific to the AES-CBC algorithm
     */

    /// cipher algorithm type
    NvCryptoCipherAlgoType AlgoType;

    /// NV_TRUE is operation is encryption; NV_FALSE if decryption
    NvBool IsEncrypt;

    /// NV_TRUE if no data has been processed yet; else NV_FALSE
    NvBool IsEmptyPayload;

    /**
     * number of bytes of data processed modulo block size (in bytes)
     *
     * Note that there's no need to keep explicit track of the amount of data
     * processed so far; the padding al
     */
    NvU32 PayloadSizeModuloBlockSize;

    /**
     * handle to AES hardware accelerator engine
     */
    void *AesHandle;

    /// padding type
    NvCryptoPaddingType PaddingType;

    /// Initial vector to use when encrypting/decrypting the first block in the
    /// chain
    NvU8 InitialVector[NVCRYPTO_CIPHER_AES_IV_BYTES];
} NvCryptoCipherAlgoAes;

/**
 * The following global variable is defined to hold
 * the algo key bytes aes_ref SW AES
 */
static NvU8 AlgoKeyBytes[NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES];

/**
 * The following routines are defined according to the prototypes given in
 * nvcrypto_cipher.h.  Full documentation can be found there.
 */

static NvError
SetAlgoParams(
    NvCryptoCipherAlgoHandle AlgoHandle,
    NvCryptoCipherAlgoParams *pAlgoParams);

static NvError
QueryAlgoType(
    NvCryptoCipherAlgoHandle AlgoHandle,
    NvCryptoCipherAlgoType *pAlgoType);

static NvError
QueryIsEncrypt(
    NvCryptoCipherAlgoHandle AlgoHandle,
    NvBool *pIsEncrypt);


static NvError
QueryIsExplicitPadding(
    NvCryptoCipherAlgoHandle AlgoHandle,
    NvBool *pIsExplicitPadding);


static NvError
QueryBlockSize(
    NvCryptoCipherAlgoHandle AlgoHandle,
    NvU32 *pBlockSize);

static NvError
QueryPadding(
    NvCryptoCipherAlgoHandle AlgoHandle,
    NvU32 *pPaddingSize,
    NvU8 *pBuffer);

static NvError
QueryPaddingByPayloadSize(
    NvCryptoCipherAlgoHandle AlgoHandle,
    NvU32 PayloadSize,
    NvU32 *pPaddingSize,
    NvU8 *pBuffer);

static NvError
QueryOptimalBufferAlignment(
    NvCryptoCipherAlgoHandle AlgoHandle,
    NvU32 *pAlignment);

static NvError
ProcessBlocks(
    NvCryptoCipherAlgoHandle AlgoHandle,
    NvU32 NumBytes,
    const void *pSrcBuffer,
    void *pDstBuffer,
    NvBool IsFirstBlock,
    NvBool IsLastBlock);

static void
ReleaseAlgorithm(
    NvCryptoCipherAlgoHandle AlgoHandle);

// end of static function definitions

NvError
SetAlgoParams(
    NvCryptoCipherAlgoHandle AlgoHandle,
    NvCryptoCipherAlgoParams *pAlgoParams)
{
    NvCryptoCipherAlgoAesHandle pAlgo = (NvCryptoCipherAlgoAesHandle)AlgoHandle;
    NvCryptoCipherAlgoParamsAesCbc *pParams = &(pAlgoParams->AesCbc);

    // check for valid inputs
    if (!pAlgo)
        return NvError_InvalidAddress;

    if (!pAlgoParams)
        return NvError_InvalidAddress;

    // verify that padding type is supported
    switch (pParams->PaddingType)
    {
        case NvCryptoPaddingType_ExplicitBitPaddingAlways:
        case NvCryptoPaddingType_ExplicitBitPaddingOptional:
            break;
        default:
            return NvError_BadParameter;
    }

    // populate context structure
    pAlgo->IsEncrypt = pParams->IsEncrypt;
    pAlgo->PaddingType = pParams->PaddingType;
    NvOsMemcpy(pAlgo->InitialVector, pParams->InitialVectorBytes,
               sizeof(pParams->InitialVectorBytes));

    // populate the AlgoKeyBytes array with the Key Bytes
    NvOsMemcpy(AlgoKeyBytes, pParams->KeyBytes, NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES);

    return NvSuccess;
}

NvError
QueryAlgoType(
    NvCryptoCipherAlgoHandle AlgoHandle,
    NvCryptoCipherAlgoType *pAlgoType)
{
    NvCryptoCipherAlgoAesHandle pAlgo = (NvCryptoCipherAlgoAesHandle)AlgoHandle;

    if (!pAlgo)
        return NvError_InvalidAddress;

    if (!pAlgoType)
        return NvError_InvalidAddress;

    *pAlgoType = pAlgo->AlgoType;

    return NvSuccess;
}

NvError
QueryIsEncrypt(
    NvCryptoCipherAlgoHandle AlgoHandle,
    NvBool *pIsEncrypt)
{
    NvCryptoCipherAlgoAesHandle pAlgo = (NvCryptoCipherAlgoAesHandle)AlgoHandle;

    if (!pAlgo)
        return NvError_InvalidAddress;

    if (!pIsEncrypt)
        return NvError_InvalidAddress;

    *pIsEncrypt = pAlgo->IsEncrypt;

    return NvSuccess;
}


NvError
QueryIsExplicitPadding(
    NvCryptoCipherAlgoHandle AlgoHandle,
    NvBool *pIsExplicitPadding)
{
    NvError e;
    NvCryptoCipherAlgoAesHandle pAlgo = (NvCryptoCipherAlgoAesHandle)AlgoHandle;

    if (!pAlgo)
        return NvError_InvalidAddress;

    if (!pIsExplicitPadding)
        return NvError_InvalidAddress;

    NV_CHECK_ERROR(NvCryptoPaddingQueryIsExplicit(pAlgo->PaddingType,
                                                  pIsExplicitPadding));

    return NvSuccess;
}

NvError
QueryBlockSize(
    NvCryptoCipherAlgoHandle AlgoHandle,
    NvU32 *pBlockSize)
{
    NvCryptoCipherAlgoAesHandle pAlgo = (NvCryptoCipherAlgoAesHandle)AlgoHandle;

    if (!pAlgo)
        return NvError_InvalidAddress;

    if (!pBlockSize)
        return NvError_InvalidAddress;

    *pBlockSize = NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES;

    return NvSuccess;
}

NvError
QueryPadding(
    NvCryptoCipherAlgoHandle AlgoHandle,
    NvU32 *pPaddingSize,
    NvU8 *pBuffer)
{
    NvError e;

    NvBool IsExplicit;
    NvU32 BufferSize;
    NvCryptoCipherAlgoAesHandle pAlgo = (NvCryptoCipherAlgoAesHandle)AlgoHandle;

    if (!pAlgo)
        return NvError_InvalidAddress;

    if (!pPaddingSize)
        return NvError_InvalidAddress;

    // client's padding size is zero if implicit padding is selected
    NV_CHECK_ERROR(NvCryptoPaddingQueryIsExplicit(pAlgo->PaddingType,
                                                  &IsExplicit));
    if (!IsExplicit)
    {
        *pPaddingSize = 0;
        return NvSuccess;
    }

    // save original buffer size; it gets clobbered by NvCryptoPaddingQuerySize()
    BufferSize = *pPaddingSize;

    // determine explicit padding size
    NV_CHECK_ERROR(NvCryptoPaddingQuerySize(
                       pAlgo->PaddingType,
                       NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES,
                       pAlgo->IsEmptyPayload,
                       pAlgo->PayloadSizeModuloBlockSize,
                       pPaddingSize));

    // store padding bytes in client buffer
    NV_CHECK_ERROR(NvCryptoPaddingQueryData(
                       pAlgo->PaddingType,
                       *pPaddingSize,
                       BufferSize,
                       pBuffer));

    return NvSuccess;
}

NvError
QueryPaddingByPayloadSize(
    NvCryptoCipherAlgoHandle AlgoHandle,
    NvU32 PayloadSize,
    NvU32 *pPaddingSize,
    NvU8 *pBuffer)
{
    NvError e;

    NvBool IsExplicit;
    NvU32 BufferSize;
    NvCryptoCipherAlgoAesHandle pAlgo = (NvCryptoCipherAlgoAesHandle)AlgoHandle;

    NvBool IsEmptyPayload;
    NvU32 PayloadSizeModuloBlockSize;

    if (!pAlgo)
        return NvError_InvalidAddress;

    if (!pPaddingSize)
        return NvError_InvalidAddress;

    // client's padding size is zero if implicit padding is selected
    NV_CHECK_ERROR(NvCryptoPaddingQueryIsExplicit(pAlgo->PaddingType, &IsExplicit));
    if (!IsExplicit)
    {
        *pPaddingSize = 0;
        return NvSuccess;
    }

    // save original buffer size; it gets clobbered by NvCryptoPaddingQuerySize()
    BufferSize = *pPaddingSize;

    // determine explicit padding size
    IsEmptyPayload = PayloadSize ? NV_FALSE : NV_TRUE;
    PayloadSizeModuloBlockSize = PayloadSize % NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES;

    NV_CHECK_ERROR(NvCryptoPaddingQuerySize(
                       pAlgo->PaddingType,
                       NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES,
                       IsEmptyPayload,
                       PayloadSizeModuloBlockSize,
                       pPaddingSize));

    // store padding bytes in client buffer
    NV_CHECK_ERROR(NvCryptoPaddingQueryData(
                       pAlgo->PaddingType,
                       *pPaddingSize,
                       BufferSize,
                       pBuffer));

    return NvSuccess;
}


NvError
QueryOptimalBufferAlignment(
    NvCryptoCipherAlgoHandle AlgoHandle,
    NvU32 *pAlignment)
{
    NvCryptoCipherAlgoAesHandle pAlgo = (NvCryptoCipherAlgoAesHandle)AlgoHandle;

    if (!pAlgo)
        return NvError_InvalidAddress;

    if (!pAlignment)
        return NvError_InvalidAddress;

    /* FIXME : Currently hard-coding the OptimalBufferAlignment to NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES */
    *pAlignment = NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES;

    return NvSuccess;
}

NvError
ProcessBlocks(
    NvCryptoCipherAlgoHandle AlgoHandle,
    NvU32 NumBytes,
    const void *pSrcBuffer,
    void *pDstBuffer,
    NvBool IsFirstBlock,
    NvBool IsLastBlock)
{
    NvU8 pAesRefExpKey[NVAES_KEYCOLS * NVAES_STATECOLS * (NVAES_ROUNDS + 1)];
    NvU8 pAesRefIn[NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES];
    // IV (Initial Vector) is all zero for Cmac/Cbc -- Need to confirm
    NvU8 pAesRefOut[NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES];
    static NvU8 pAesIv[NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES] = {0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00};
    NvU8 i;
    NvU8 *pSrcPtr;
    NvU8 *pDstPtr;


    NvCryptoCipherAlgoAesHandle pAlgo = (NvCryptoCipherAlgoAesHandle)AlgoHandle;
    if (!pAlgo)
        return NvError_InvalidAddress;

    if (!NumBytes)
        return NvSuccess;

    if (NumBytes % NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES != 0)
        return NvError_InvalidSize;

    if (!pSrcBuffer)
        return NvError_InvalidAddress;

    if (!pDstBuffer)
        return NvError_InvalidAddress;

    // load IV when first block is processed
    if (IsFirstBlock)
    {
        NvOsMemcpy(pAesIv, pAlgo->InitialVector, NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES);
    }

    // perform crypto operation
    NvAesExpandKey((NvU8 *)AlgoKeyBytes, (NvU8 *)pAesRefExpKey);

    pSrcPtr = (NvU8 *)pSrcBuffer;
    pDstPtr = (NvU8 *)pDstBuffer;

    while (NumBytes) {
        NvOsMemcpy(pAesRefIn, pSrcPtr, NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES);

        if (pAlgo->IsEncrypt == NV_TRUE)
        {
            for (i=0; i<NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES; i++)
            {
                    pAesRefIn[i] ^= pAesIv[i];
            }
            NvAesEncrypt(pAesRefIn, pAesRefExpKey, pAesRefOut);

            NvOsMemcpy(pAesIv, pAesRefOut, NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES);
        }
        else
        {
            NvAesDecrypt(pAesRefIn, pAesRefExpKey, pAesRefOut);
            for (i=0; i<NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES; i++)
            {
                    pAesRefOut[i] ^= pAesIv[i];
            }

            NvOsMemcpy(pAesIv, pAesRefIn, NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES);
        }

        NvOsMemcpy(pDstPtr, pAesRefOut, NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES);

        NumBytes -= NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES;
        pSrcPtr += NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES;
        pDstPtr += NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES;
    }

    return NvSuccess;
}

void
ReleaseAlgorithm(
    NvCryptoCipherAlgoHandle AlgoHandle)
{
    NvCryptoCipherAlgoAesHandle pAlgo = (NvCryptoCipherAlgoAesHandle)AlgoHandle;

    if (!pAlgo)
        return;

    // close AES engine
    pAlgo->AesHandle = NULL;

    // free context
    NvOsFree(pAlgo);
}

NvError
NvCryptoCipherSelectAlgorithmAes(
    NvCryptoCipherAlgoType CipherAlgoType,
    NvCryptoCipherAlgoHandle *pCipherAlgoHandle)
{
    NvCryptoCipherAlgoAesHandle pAlgo;

    switch(CipherAlgoType)
    {
        case NvCryptoCipherAlgoType_AesCbc:
            break;
        default:
            return NvError_BadParameter;
    }

    pAlgo = (NvCryptoCipherAlgoAesHandle)NvOsAlloc(sizeof(NvCryptoCipherAlgoAes));

    if (!pAlgo)
        return NvError_InsufficientMemory;

    // initialize entry points

    pAlgo->CipherAlgo.SetAlgoParams = SetAlgoParams;
    pAlgo->CipherAlgo.QueryAlgoType = QueryAlgoType;
    pAlgo->CipherAlgo.QueryIsEncrypt = QueryIsEncrypt;
    pAlgo->CipherAlgo.QueryIsExplicitPadding = QueryIsExplicitPadding;
    pAlgo->CipherAlgo.QueryBlockSize = QueryBlockSize;
    pAlgo->CipherAlgo.QueryPadding = QueryPadding;
    pAlgo->CipherAlgo.QueryPaddingByPayloadSize = QueryPaddingByPayloadSize;
    pAlgo->CipherAlgo.QueryOptimalBufferAlignment = QueryOptimalBufferAlignment;
    pAlgo->CipherAlgo.ProcessBlocks = ProcessBlocks;
    pAlgo->CipherAlgo.ReleaseAlgorithm = ReleaseAlgorithm;

    // initialize context data

    pAlgo->AlgoType = CipherAlgoType;
    pAlgo->IsEncrypt = NV_FALSE;
    pAlgo->IsEmptyPayload = NV_TRUE;
    pAlgo->PayloadSizeModuloBlockSize = 0;
    pAlgo->AesHandle = NULL;
    pAlgo->PaddingType = NvCryptoPaddingType_Invalid;

    *pCipherAlgoHandle = (NvCryptoCipherAlgoHandle)pAlgo;

    return NvSuccess;
}

NvError
NvCryptoCipherSelectAlgorithmSeAes(
        NvCryptoCipherAlgoType CipherAlgoType,
        NvCryptoCipherAlgoHandle *pCipherAlgoHandle)
{
    return NvCryptoCipherSelectAlgorithmAes(CipherAlgoType, pCipherAlgoHandle);
}
