/*
 * Copyright (c) 2008 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcrypto_hash_cmac.h"
#include "nvcrypto_padding.h"
#include "nvcrypto_cipher.h"
#include "nvos.h"
#include "nvassert.h"
#include "nvddk_fuse.h"

enum
{
    /// size of hash result, also size of underlying cipher operations
    NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES = 16,

    /// size of scratch buffer used to store intermediate encryption results
    NVCRYPTO_HASH_AES_SCRATCH_SIZE_BYTES = 4096
};

#if NV_IS_SE_USED
    NvU8 *CmacHash;
#endif

/// forward declaration
typedef struct NvCryptoHashAlgoCmacRec *NvCryptoHashAlgoCmacHandle;

typedef struct NvCryptoHashAlgoCmacRec
{
    /**
     * state information required for all hash algorithms
     */

    /// standard entry points
    NvCryptoHashAlgo HashAlgo;

    /**
     * state information specific to the AES-CBC algorithm
     */

    /// hash algorithm type
    NvCryptoHashAlgoType AlgoType;
    
    /// NV_TRUE is operation is calculate; NV_FALSE if verify
    NvBool IsCalculate;

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
     * handle to block cipher algorithm
     */
    NvCryptoCipherAlgoHandle CipherHandle;

    /// padding type
    NvCryptoPaddingType PaddingType;
    
    /// NV_TRUE if explicit padding, else NV_FALSE if implicit padding
    NvBool IsExplicitPadding;
    
    /// Block size for the CMAC hash algorithm
    NvU32 BlockSize;
    
    /// K1 subkey
    NvU8 K1[NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES]; // FIXME
    
    /// K2 subkey
    NvU8 K2[NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES]; // FIXME

    /// NV_TRUE if hash calculation is complete, else NV_FALSE
    NvBool IsHashReady;

    /// calculated hash value
    NvU8 CalculatedHash[NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES]; // FIXME

    /**
     * temporary buffer used when encrypting data.
     *
     * The CMAC algorithm only explicitly references the final block of the
     * encrypted message.  All the other intermediate encryption results can be
     * discarded.  To reduce memory footprint, multiple smaller encryptions are
     * performed and the destination buffer is repeatedly overwritten.  The
     * smaller the destination buffer, the greater the number of iterations
     * required, and hence the lower the performance.
     */
    NvU8 *pScratch;    

} NvCryptoHashAlgoCmac;

// locally defined static functions

/**
 * Shift a byte vector left by one bit
 *
 * The vector is treated as one big bit string, where the Most Significant
 * Bit (MSB) is in first byte of the vector
 *
 * @param Buffer buffer where byte vector is stored
 * @param Size number of elements in vector
 * @param pOriginalMsb pointer to buffer where MSB of the original vector
 *        is to be stored
 *
 * @retval none
 */

static void
LeftShiftOneBit(
    NvU8 *Buffer,
    NvU32 Size,
    NvU8 *pOriginalMsb);

/**
 * Compute CMAC subkeys K1 and K2 from the AES-CMAC spec (RFC 4493)
 *
 * @param CipherHandle handle to the cipher algorithm state information
 * @param K1 pointer to buffer where CMAC subkey K1 is to be stored
 * @param K2 pointer to buffer where CMAC subkey K2 is to be stored
 *
 * @retval NvError_Success Subkeys computed successfully
 * Note: other errors reported by block cipher operations are passed through
 */

static NvError
GenerateSubkeys(
    NvCryptoCipherAlgoHandle CipherHandle,
    NvU8 *pK1,
    NvU8 *pK2);

/**
 * Process data blocks using cipher algorithm; throw away intermediate results
 * and only return the final encrypted block.
 *
 * If the source buffer is larger then the scratch (destination) buffer, then
 * multiple scratch-buffer-size operations are performed, overwriting the
 * scratch buffer contents each time.
 *
 * NumBytes must always be a multiple of the algorithm's block size.
 *
 * Final result is always stored at the beginning of the scratch buffer.
 *
 * @param CipherHandle handle to the cipher algorithm state information
 * @param NumBytes number of bytes to process; must be a multiple of the block
 *        size as described above
 * @param pSrcBuffer pointer to input data buffer; must contain NumBytes of
 *        good data
 * @param pScratch pointer to scratch data buffer; contents will be repeatedly
 *        overwritten; but ultimately the final output block will be stored at
 *        the beginning of the buffer
 * @param ScratchSize size of pScratch buffer, in bytes
 * @param BlockSize cipher algorithm's native block size, in bytes
 * @param IsFirstBlock NV_TRUE if source buffer contains first block in
 *        message, else NV_FALSE
 * @param IsLastBlock NV_TRUE if source buffer contains final block in
 *        message, else NV_FALSE
 *
 * @retval NvSuccess Data bytes have been processed successfully
 * @retval NvError_InvalidAddress Illegal data buffer address (NULL)
 * @retval NvError_InvalidSize NumBytes is not a multiple of the block size
 */
#if !NV_IS_SE_USED
static NvError
ProcessBlocksKeepLast(
    NvCryptoCipherAlgoHandle CipherHandle,
    NvU32 NumBytes,
    const NvU8 *pSrcBuffer,
    NvU8 *pScratch,
    NvU32 ScratchSize,
    NvU32 BlockSize,
    NvBool IsFirstBlock,
    NvBool IsLastBlock);
#endif
/**
 * The following routines are defined according to the prototypes given in
 * nvcrypto_hash.h.  Full documentation can be found there.
 */

static NvError
SetAlgoParams(
    NvCryptoHashAlgoHandle AlgoHandle,
    NvCryptoHashAlgoParams *pAlgoParams);

static NvError
QueryAlgoType(
    NvCryptoHashAlgoHandle AlgoHandle,
    NvCryptoHashAlgoType *pAlgoType);

static NvError
QueryIsCalculate(
    NvCryptoHashAlgoHandle AlgoHandle,
    NvBool *pIsCalculate);


static NvError
QueryIsExplicitPadding(
    NvCryptoHashAlgoHandle AlgoHandle,
    NvBool *pIsExplicitPadding);


static NvError
QueryBlockSize(
    NvCryptoHashAlgoHandle AlgoHandle,
    NvU32 *pBlockSize);

static NvError
QueryPadding(
    NvCryptoHashAlgoHandle AlgoHandle,
    NvU32 *pPaddingSize,
    NvU8 *pBuffer);

static NvError
QueryPaddingByPayloadSize(
    NvCryptoHashAlgoHandle AlgoHandle,
    NvU32 PayloadSize,
    NvU32 *pPaddingSize,
    NvU8 *pBuffer);

static NvError
QueryOptimalBufferAlignment(
    NvCryptoHashAlgoHandle AlgoHandle,
    NvU32 *pAlignment);

static NvError
ProcessBlocks(
    NvCryptoHashAlgoHandle AlgoHandle,
    NvU32 NumBytes,
    const void *pSrcBuffer,
    NvBool IsFirstBlock,
    NvBool IsLastBlock);

static NvError
QueryHash(
    NvCryptoHashAlgoHandle AlgoHandle,
    NvU32 *pHashSize,
    NvU8 *pBuffer);

static NvError
VerifyHash(
    NvCryptoHashAlgoHandle AlgoHandle,
    NvU8 *pExpectedHashValue,
    NvBool *pIsValid);

static void
ReleaseAlgorithm(
    NvCryptoHashAlgoHandle AlgoHandle);

// end of static function definitions
#if !NV_IS_SE_USED || defined(BOOT_MINIMAL_BL)
NvError
ProcessBlocksKeepLast(
    NvCryptoCipherAlgoHandle CipherHandle,
    NvU32 NumBytes,
    const NvU8 *pSrcBuffer,
    NvU8 *pScratch,
    NvU32 ScratchSize,
    NvU32 BlockSize,
    NvBool IsFirstBlock,
    NvBool IsLastBlock)
{
    NvError e;
    
    NV_ASSERT(NumBytes % BlockSize == 0);
    NV_ASSERT(CipherHandle);
    NV_ASSERT(pSrcBuffer);
    NV_ASSERT(ScratchSize);
    NV_ASSERT(ScratchSize>=BlockSize);
    
    if (!NumBytes)
        return NvSuccess;
    
    
    while (NumBytes)
    {
        NvU32 BytesToProcess = NumBytes;
        NvBool IsLastIteration = NV_FALSE;
        
        if (BytesToProcess > ScratchSize)
            BytesToProcess = ScratchSize;

        if (BytesToProcess == NumBytes)
            IsLastIteration = NV_TRUE;
        
        NV_CHECK_ERROR(CipherHandle->ProcessBlocks(
                           CipherHandle,
                           BytesToProcess,
                           pSrcBuffer,
                           pScratch,
                           IsFirstBlock,
                           IsLastIteration ? IsLastBlock : NV_FALSE));
        
        IsFirstBlock = NV_FALSE;

        if (IsLastIteration && NumBytes != BlockSize)
            NvOsMemcpy(pScratch, pScratch + NumBytes - BlockSize, BlockSize);
        
        NumBytes -= BytesToProcess;
        pSrcBuffer += BytesToProcess;
    }
    
    return NvSuccess;
}
#endif
#define GET_MSB(x)  ( (x) >> (8*sizeof(x)-1) )

void
LeftShiftOneBit(
    NvU8 *Buffer,
    NvU32 Size,
    NvU8 *pOriginalMsb)
{
    NvU8 Carry;
    NvU32 i;
    
    *pOriginalMsb = GET_MSB(Buffer[0]);
    
    // left shift one bit
    Buffer[0] <<= 1;
    for (Carry=0, i=1; i<Size; i++) {
        Carry = GET_MSB(Buffer[i]);
        Buffer[i-1] |= Carry;
        Buffer[i] <<= 1;
    }
}

NvError
GenerateSubkeys(
    NvCryptoCipherAlgoHandle CipherHandle,
    NvU8 *pK1,
    NvU8 *pK2)
{
    NvError e;
    NvCryptoCipherAlgoHandle pCipherAlgo = (NvCryptoCipherAlgoHandle)CipherHandle;

    NvU8 L[NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES];
    NvU8 Zeroes[NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES];
    NvU8 const Rb = 0x87;
    NvU8 Msb;
    
    NV_ASSERT(CipherHandle);
    NV_ASSERT(pK1);
    NV_ASSERT(pK2);
    
    NvOsMemset(Zeroes, 0x0, sizeof(Zeroes));
    
    NV_CHECK_ERROR(pCipherAlgo->ProcessBlocks(pCipherAlgo,
                                              NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES,
                                              Zeroes,
                                              L,
                                              NV_TRUE,
                                              NV_TRUE));
    
    // compute K1 subkey
    
    NvOsMemcpy(pK1, L, NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES);
    LeftShiftOneBit(pK1, NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES, &Msb);
    
    if (Msb)
        pK1[NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES - 1] ^= Rb;
    
    // compute K2 subkey
    
    NvOsMemcpy(pK2, pK1, NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES);
    LeftShiftOneBit(pK2, NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES, &Msb);
    
    if (Msb)
        pK2[NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES - 1] ^= Rb;
    
    return NvSuccess;
}


NvError
SetAlgoParams(
    NvCryptoHashAlgoHandle AlgoHandle,
    NvCryptoHashAlgoParams *pAlgoParams)
{
    NvError e;
    
    NvCryptoHashAlgoCmacHandle pAlgo = (NvCryptoHashAlgoCmacHandle)AlgoHandle;
    NvCryptoHashAlgoParamsAesCmac *pHashParams = &(pAlgoParams->AesCmac);

    NvCryptoCipherAlgoParams CipherParams;
    NvCryptoCipherAlgoParamsAesCbc *pCipherParams = &(CipherParams.AesCbc);

    NvU32 CipherBlockSize;

    // check for valid inputs
    if (!pAlgo)
        return NvError_InvalidAddress;

    if (!pAlgoParams)
        return NvError_InvalidAddress;

    // verify that padding type is supported
    switch (pHashParams->PaddingType)
    {
        case NvCryptoPaddingType_ExplicitBitPaddingAlways:
        case NvCryptoPaddingType_ExplicitBitPaddingOptional:
        case NvCryptoPaddingType_ImplicitBitPaddingAlways:
        case NvCryptoPaddingType_ImplicitBitPaddingOptional:
            break;
        default:
            return NvError_BadParameter;
    }

    // populate context structure
    pAlgo->IsCalculate = pHashParams->IsCalculate;
    pAlgo->PaddingType = pHashParams->PaddingType;

    NV_CHECK_ERROR_CLEANUP(NvCryptoPaddingQueryIsExplicit(pAlgo->PaddingType, 
                                                          &pAlgo->IsExplicitPadding));
    
    // obtain cipher algorithm
    NV_CHECK_ERROR_CLEANUP(NvCryptoCipherSelectAlgorithm(
                               NvCryptoCipherAlgoType_AesCbc,
                               &pAlgo->CipherHandle));

    // configure cipher algorithm
    // Note: padding type doesn't matter since hash algorithm will ensure that
    // padding constraints are met
    pCipherParams->IsEncrypt = NV_TRUE;
    pCipherParams->KeyType = pHashParams->KeyType;
    pCipherParams->KeySize = pHashParams->KeySize;
    NvOsMemcpy(pCipherParams->KeyBytes, pHashParams->KeyBytes, 
               sizeof(pCipherParams->KeyBytes));
    NvOsMemset(pCipherParams->InitialVectorBytes, 0x0,
               sizeof(pCipherParams->InitialVectorBytes));
    pCipherParams->PaddingType = NvCryptoPaddingType_ExplicitBitPaddingOptional;

    NV_CHECK_ERROR_CLEANUP(pAlgo->CipherHandle->SetAlgoParams(
                               pAlgo->CipherHandle,
                               &CipherParams));
    
    // overwrite key data
    NvOsMemset(pCipherParams->KeyBytes, 0x0, sizeof(pCipherParams->KeyBytes));

    // confirm block size -- RFC 4493 is only defined for the AES-128 block cipher,
    // which has a known block size
    NV_CHECK_ERROR_CLEANUP(pAlgo->CipherHandle->QueryBlockSize(
                               pAlgo->CipherHandle,
                               &CipherBlockSize));
    
    if (CipherBlockSize != NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES)
    {
        e = NvError_InvalidSize;
        goto fail;
    }

    pAlgo->BlockSize = CipherBlockSize;

    // compute subkeys
    NV_CHECK_ERROR_CLEANUP(GenerateSubkeys(pAlgo->CipherHandle,
                                           pAlgo->K1,
                                           pAlgo->K2));

    return NvSuccess;

fail:

    if (pAlgo->CipherHandle)
        pAlgo->CipherHandle->ReleaseAlgorithm(
            pAlgo->CipherHandle);

    pAlgo->CipherHandle = (NvCryptoCipherAlgoHandle)NULL;
    
    return e;
}

NvError
QueryAlgoType(
    NvCryptoHashAlgoHandle AlgoHandle,
    NvCryptoHashAlgoType *pAlgoType)
{
    NvCryptoHashAlgoCmacHandle pAlgo = (NvCryptoHashAlgoCmacHandle)AlgoHandle;

    if (!pAlgo)
        return NvError_InvalidAddress;

    if (!pAlgoType)
        return NvError_InvalidAddress;
    
    *pAlgoType = pAlgo->AlgoType;

    return NvSuccess;
}

NvError
QueryIsCalculate(
    NvCryptoHashAlgoHandle AlgoHandle,
    NvBool *pIsCalculate)
{
    NvCryptoHashAlgoCmacHandle pAlgo = (NvCryptoHashAlgoCmacHandle)AlgoHandle;
   
    if (!pAlgo)
        return NvError_InvalidAddress;
        
    if (!pIsCalculate)
        return NvError_InvalidAddress;
    
    *pIsCalculate = pAlgo->IsCalculate;

    return NvSuccess;
}

    
NvError
QueryIsExplicitPadding(
    NvCryptoHashAlgoHandle AlgoHandle,
    NvBool *pIsExplicitPadding)
{
    NvCryptoHashAlgoCmacHandle pAlgo = (NvCryptoHashAlgoCmacHandle)AlgoHandle;
    
    if (!pAlgo)
        return NvError_InvalidAddress;
    
    if (!pIsExplicitPadding)
        return NvError_InvalidAddress;
    
    *pIsExplicitPadding = pAlgo->IsExplicitPadding;

    return NvSuccess;
}

NvError
QueryBlockSize(
    NvCryptoHashAlgoHandle AlgoHandle,
    NvU32 *pBlockSize)
{
    NvCryptoHashAlgoCmacHandle pAlgo = (NvCryptoHashAlgoCmacHandle)AlgoHandle;
    
    if (!pAlgo)
        return NvError_InvalidAddress;
    
    if (!pBlockSize)
        return NvError_InvalidAddress;
    
    *pBlockSize = pAlgo->BlockSize;

    return NvSuccess;
}

NvError
QueryPadding(
    NvCryptoHashAlgoHandle AlgoHandle,
    NvU32 *pPaddingSize,
    NvU8 *pBuffer)
{
    NvError e;

    NvBool IsExplicit;
    NvU32 BufferSize;
    NvCryptoHashAlgoCmacHandle pAlgo = (NvCryptoHashAlgoCmacHandle)AlgoHandle;
    
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
                       NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES,
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
    NvCryptoHashAlgoHandle AlgoHandle,
    NvU32 PayloadSize,
    NvU32 *pPaddingSize,
    NvU8 *pBuffer)
{
    NvError e;

//    NvBool IsExplicit; // FIXME -- remove?
    NvU32 BufferSize;
    NvCryptoHashAlgoCmacHandle pAlgo = (NvCryptoHashAlgoCmacHandle)AlgoHandle;

    NvBool IsEmptyPayload;
    NvU32 PayloadSizeModuloBlockSize;
    
    if (!pAlgo)
        return NvError_InvalidAddress;
    
    if (!pPaddingSize)
        return NvError_InvalidAddress;
#if 0     // FIXME -- remove?
    // client's padding size is zero if implicit padding is selected
    NV_CHECK_ERROR(NvCryptoPaddingQueryIsExplicit(pAlgo->PaddingType, &IsExplicit));
    if (!IsExplicit)
    {
        *pPaddingSize = 0;
        return NvSuccess;
    }
#endif
    // save original buffer size; it gets clobbered by NvCryptoPaddingQuerySize()
    BufferSize = *pPaddingSize;
    
    // determine explicit padding size
    IsEmptyPayload = PayloadSize ? NV_FALSE : NV_TRUE;
    PayloadSizeModuloBlockSize = PayloadSize % NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES;

    NV_CHECK_ERROR(NvCryptoPaddingQuerySize(
                       pAlgo->PaddingType,
                       NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES,
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
    NvCryptoHashAlgoHandle AlgoHandle,
    NvU32 *pAlignment)
{
    NvError e;

    NvCryptoHashAlgoCmacHandle pAlgo = (NvCryptoHashAlgoCmacHandle)AlgoHandle;
    
    if (!pAlgo)
        return NvError_InvalidAddress;
    
    if (!pAlignment)
        return NvError_InvalidAddress;
    
    NV_CHECK_ERROR(pAlgo->CipherHandle->QueryOptimalBufferAlignment(
                       pAlgo->CipherHandle,
                       pAlignment));

    return NvSuccess;
}

NvError
ProcessBlocks(
    NvCryptoHashAlgoHandle AlgoHandle,
    NvU32 NumBytes,
    const void *pSrcBuffer,
    NvBool IsFirstBlock,
    NvBool IsLastBlock)
{
#if NV_IS_SE_USED && !defined(BOOT_MINIMAL_BL)
    NvError e = NvSuccess;
    NvCryptoHashAlgoCmacHandle pAlgo = (NvCryptoHashAlgoCmacHandle)AlgoHandle;
    NvCryptoCipherAlgoHandle CipherHandle = pAlgo->CipherHandle;
    static NvU8 key[16] = {0};
    static NvBool IsSbkKey = NV_FALSE;
    NvU32 size;
    NvDdkFuseOperatingMode OpMode;

    if(IsFirstBlock)
    {
        size =  sizeof(NvU32);
        NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_OpMode,
                (void *)&OpMode, (NvU32 *)&size));
        if (OpMode == NvDdkFuseOperatingMode_OdmProductionSecure)
            IsSbkKey = NV_TRUE;
    }

    CmacHash = NvOsAlloc(16);
    NvOsMemset(CmacHash, 16, 0);
    NV_CHECK_ERROR_CLEANUP(CipherHandle->SePerformCmacOperation(key,
            16, (void *)(pSrcBuffer), CmacHash, NumBytes,
            IsFirstBlock, IsLastBlock, IsSbkKey));

    if (IsLastBlock)
        pAlgo->IsHashReady = NV_TRUE;

fail:
    return e;
#else
    NvError e;
    NvCryptoHashAlgoCmacHandle pAlgo = (NvCryptoHashAlgoCmacHandle)AlgoHandle;

    NvU32 NumBlocks;

    if (!pAlgo)
        return NvError_InvalidAddress;
    
    if (IsLastBlock && pAlgo->IsExplicitPadding && 
        NumBytes % NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES != 0)
        return NvError_InvalidSize;
    
    if (IsLastBlock && pAlgo->IsExplicitPadding && NumBytes == 0)
        return NvError_InvalidSize;
    
    if (!IsLastBlock && NumBytes % NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES != 0)
        return NvError_InvalidSize;
    
    if (!pSrcBuffer)
          return NvError_InvalidAddress;

    if (IsFirstBlock)
    {
        pAlgo->IsHashReady = NV_FALSE;
        pAlgo->IsEmptyPayload = NV_TRUE;
        pAlgo->PayloadSizeModuloBlockSize = 0;
    }
    
    // process all blocks, except the final one
    NV_ASSERT(pAlgo->BlockSize);
    
    NumBlocks = NumBytes / pAlgo->BlockSize;
    if (NumBlocks && IsLastBlock && NumBytes % pAlgo->BlockSize == 0)
        NumBlocks--;

    if (NumBlocks)
        NV_CHECK_ERROR(ProcessBlocksKeepLast(
                           pAlgo->CipherHandle,
                           NumBlocks*pAlgo->BlockSize,
                           (const NvU8 *)pSrcBuffer,
                           pAlgo->pScratch,
                           NVCRYPTO_HASH_AES_SCRATCH_SIZE_BYTES,
                           pAlgo->BlockSize,
                           IsFirstBlock,
                           NV_FALSE));

    // update payload size
    if (NumBytes != 0)
        pAlgo->IsEmptyPayload = NV_FALSE;
    
    pAlgo->PayloadSizeModuloBlockSize += NumBytes;
    pAlgo->PayloadSizeModuloBlockSize %= pAlgo->BlockSize;

    // prepare final block
    if (IsLastBlock)
    {
        NvU8 FinalPayloadBlocks[2*NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES];
        NvU8 FinalEncryptBlocks[2*NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES];
        NvU32 FinalBlockOffset;
        NvU32 FinalPayloadSize;
        NvU32 PaddingSize = 0;
        NvU32 i;
        NvU8 *SubKey;
        
        // copy last block or portion thereof to local buffer

        if (!pAlgo->IsEmptyPayload && pAlgo->PayloadSizeModuloBlockSize == 0)
            FinalPayloadSize = pAlgo->BlockSize;
        else
            FinalPayloadSize = (NvU32)pAlgo->PayloadSizeModuloBlockSize;

        NvOsMemcpy(FinalPayloadBlocks, 
                   (const NvU8 *)pSrcBuffer+NumBytes-FinalPayloadSize,
                   FinalPayloadSize);
        
        // add padding to data in local buffer, if needed

        if (!pAlgo->IsExplicitPadding)
        {
            // determine size of implicit padding, if any
            NV_CHECK_ERROR(NvCryptoPaddingQuerySize(
                               pAlgo->PaddingType,
                               pAlgo->BlockSize,
                               pAlgo->IsEmptyPayload,
                               FinalPayloadSize,
                               &PaddingSize));

            // assert that padding will fit inside fixed-size buffer for final
            // blocks
            //
            // this should always be true because padding can at most fill out
            // the last block in the payload plus (optionally) add one more
            // block
            NV_ASSERT(FinalPayloadSize + PaddingSize <= 
                      sizeof(FinalPayloadBlocks));

            // assert that padding must result in a payload that is an exact
            // multiple of the block size; otherwise, the padding algorithm is
            // broken
            NV_ASSERT((FinalPayloadSize + PaddingSize) % pAlgo->BlockSize == 0);

            // add the padding
            NV_CHECK_ERROR(NvCryptoPaddingQueryData(
                               pAlgo->PaddingType,
                               PaddingSize,
                               sizeof(FinalPayloadBlocks) - FinalPayloadSize,
                               FinalPayloadBlocks + FinalPayloadSize));

            FinalPayloadSize += PaddingSize;
        }
        
        if (!pAlgo->IsEmptyPayload && pAlgo->PayloadSizeModuloBlockSize == 0)
            SubKey = pAlgo->K1;
        else
            SubKey = pAlgo->K2;
        
        // XOR final block with appropriate subkey
        FinalBlockOffset = FinalPayloadSize - pAlgo->BlockSize;

        for (i=0; i<pAlgo->BlockSize; i++)
            FinalPayloadBlocks[FinalBlockOffset + i] ^= SubKey[i];

        // encrypt last block(s)
        NV_CHECK_ERROR(pAlgo->CipherHandle->ProcessBlocks(
                           pAlgo->CipherHandle, FinalPayloadSize, 
                           FinalPayloadBlocks, FinalEncryptBlocks,
                           NumBlocks ? NV_FALSE : IsFirstBlock, IsLastBlock));

        // copy last block of encryption result into hash result
        NvOsMemcpy(pAlgo->CalculatedHash, FinalEncryptBlocks + FinalBlockOffset, 
                   pAlgo->BlockSize);

        // clear the temporary buffers
        NvOsMemset(FinalPayloadBlocks, 0x0, sizeof(FinalPayloadBlocks));
        NvOsMemset(FinalEncryptBlocks, 0x0, sizeof(FinalEncryptBlocks));

        pAlgo->IsHashReady = NV_TRUE;
    }

    return NvSuccess;
#endif
}

NvError
QueryHash(
    NvCryptoHashAlgoHandle AlgoHandle,
    NvU32 *pHashSize,
    NvU8 *pBuffer)
{
    NvU32 BufferSize;
    NvCryptoHashAlgoCmacHandle pAlgo = (NvCryptoHashAlgoCmacHandle)AlgoHandle;
    
    if (!pAlgo)
        return NvError_InvalidAddress;
    
    if (!pHashSize)
        return NvError_InvalidAddress;

    if (!pAlgo->IsHashReady)
        return NvError_InvalidState;
    
    // save original buffer size
    BufferSize = *pHashSize;

    *pHashSize = NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES;

    if (BufferSize < *pHashSize)
        return NvError_InsufficientMemory;
    
    if (!pBuffer)
        return NvError_InvalidAddress;
#if NV_IS_SE_USED && !defined(BOOT_MINIMAL_BL)
    NvOsMemcpy(pBuffer, CmacHash, NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES);
#else
    NvOsMemcpy(pBuffer, pAlgo->CalculatedHash, NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES);
#endif
    return NvSuccess;
}


NvError
VerifyHash(
    NvCryptoHashAlgoHandle AlgoHandle,
    NvU8 *pExpectedHashValue,
    NvBool *pIsValid)
{
    NvCryptoHashAlgoCmacHandle pAlgo = (NvCryptoHashAlgoCmacHandle)AlgoHandle;
    
    if (!pAlgo)
        return NvError_InvalidAddress;
    
    if (!pExpectedHashValue)
        return NvError_InvalidAddress;

    if (!pIsValid)
        return NvError_InvalidAddress;

    if (!pAlgo->IsHashReady)
        return NvError_InvalidState;

#if NV_IS_SE_USED && !defined(BOOT_MINIMAL_BL)
    *pIsValid = ! NvOsMemcmp(CmacHash, pExpectedHashValue,
                            NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES);
#else
    *pIsValid = ! NvOsMemcmp(pAlgo->CalculatedHash, pExpectedHashValue,
                             NVCRYPTO_HASH_AES_BLOCK_SIZE_BYTES);
#endif
    return NvSuccess;
}

void
ReleaseAlgorithm(
    NvCryptoHashAlgoHandle AlgoHandle)
{
    NvCryptoHashAlgoCmacHandle pAlgo = (NvCryptoHashAlgoCmacHandle)AlgoHandle;
    
    if (!pAlgo)
        return;

    // over-write the hash result
    NvOsMemset(pAlgo->CalculatedHash, 0x0, sizeof(pAlgo->CalculatedHash));
    
    // close AES engine, for non-null handle 
    if (pAlgo->CipherHandle)
        pAlgo->CipherHandle->ReleaseAlgorithm(pAlgo->CipherHandle);

    // over-write the scratch buffer, then free it
    NvOsMemset(pAlgo->pScratch, 0x0, NVCRYPTO_HASH_AES_SCRATCH_SIZE_BYTES);
    NvOsFree(pAlgo->pScratch);
    
    // free context
    NvOsFree(pAlgo);
}

NvError
NvCryptoHashSelectAlgorithmCmac(
    NvCryptoHashAlgoType HashAlgoType,
    NvCryptoHashAlgoHandle *pHashAlgoHandle)
{
    NvCryptoHashAlgoCmacHandle pAlgo = NULL;
    
    if (!pHashAlgoHandle)
        return NvError_InvalidAddress;
    
    switch(HashAlgoType)
    {
        case NvCryptoHashAlgoType_AesCmac:
            break;
        default:
            return NvError_BadParameter;
    }

    pAlgo = (NvCryptoHashAlgoCmacHandle)NvOsAlloc(sizeof(NvCryptoHashAlgoCmac));
    if (!pAlgo)
        return NvError_InsufficientMemory;
   
    // initialize entry points

    pAlgo->HashAlgo.SetAlgoParams = SetAlgoParams;
    pAlgo->HashAlgo.QueryAlgoType = QueryAlgoType;
    pAlgo->HashAlgo.QueryIsCalculate = QueryIsCalculate;
    pAlgo->HashAlgo.QueryIsExplicitPadding = QueryIsExplicitPadding;
    pAlgo->HashAlgo.QueryBlockSize = QueryBlockSize;
    pAlgo->HashAlgo.QueryPadding = QueryPadding;
    pAlgo->HashAlgo.QueryPaddingByPayloadSize = QueryPaddingByPayloadSize;
    pAlgo->HashAlgo.QueryOptimalBufferAlignment = QueryOptimalBufferAlignment;
    pAlgo->HashAlgo.ProcessBlocks = ProcessBlocks;
    pAlgo->HashAlgo.QueryHash = QueryHash;
    pAlgo->HashAlgo.VerifyHash = VerifyHash;
    pAlgo->HashAlgo.ReleaseAlgorithm = ReleaseAlgorithm;

    // initialize context data
    
    pAlgo->AlgoType = HashAlgoType;
    pAlgo->IsCalculate = NV_FALSE;
    pAlgo->IsEmptyPayload = NV_TRUE;
    pAlgo->PayloadSizeModuloBlockSize = 0;
    pAlgo->CipherHandle = NULL;
    pAlgo->PaddingType = NvCryptoPaddingType_Invalid;
    NvOsMemset(pAlgo->K1, 0x0, sizeof(pAlgo->K1));
    NvOsMemset(pAlgo->K2, 0x0, sizeof(pAlgo->K2));
    pAlgo->IsHashReady = NV_FALSE;

    pAlgo->pScratch = (NvU8 *)NvOsAlloc(NVCRYPTO_HASH_AES_SCRATCH_SIZE_BYTES);
    if (!pAlgo->pScratch)
    {
        NvOsFree(pAlgo);
        return NvError_InsufficientMemory;
    }
 
    *pHashAlgoHandle = (NvCryptoHashAlgoHandle)pAlgo;
   
    return NvSuccess;
}

    
