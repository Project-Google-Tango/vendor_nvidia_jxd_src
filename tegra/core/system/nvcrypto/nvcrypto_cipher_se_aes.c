/*
 * Copyright (c) 2012 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcrypto_cipher_aes.h"
#include "nvcrypto_padding.h"
#include "nvddk_blockdevmgr.h"
#include "nvddk_se_blockdev.h"

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
     * number of bytes of data processed moduloblock size (in bytes)
     *
     * Note that there's no need to keep explicit track of the amount of data
     * processed so far; the padding al
     */
    NvU32 PayloadSizeModuloBlockSize;

    /**
     * handle to AES hardware accelerator engine
     */
    NvDdkBlockDevHandle AesHandle;

    /// padding type
    NvCryptoPaddingType PaddingType;

    /// Initial vector to use when encrypting/decrypting the first block in the chain
    NvU8 InitialVector[NVCRYPTO_CIPHER_AES_IV_BYTES];
} NvCryptoCipherAlgoAes;

/**
 * Sets the Algoritham related information in the context.
 *
 * @params AlgoHandle Algohandle.
 * @pAlgoParams parameters to be set
 *
 * @retval NvSuccess if operation is successful.
 * @retval NvError_Notinitliazed either if pAlgoParams or SeAlgoHandle are not
 * valid.
 */
static NvError
SetSeAlgoParams(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvCryptoCipherAlgoParams *pAlgoParams);

/**
 * Gets the Algoritham type into pAlgoType structure.
 *
 * @params AlgoHandle Algohandle.
 * @params pAlgoType pointer to the algotype
 *
 * @retval: NvSuccess if operation is successful.
 * @retval NvError_InvalidAddress either if pAlgoType or SeAlgoHandle are
 * not valid
 */
static NvError
SeQueryAlgoType(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvCryptoCipherAlgoType *pAlgoType);

/**
 * Gives the info if the operation is encryption or decryption.
 *
 * @params AlgoHandle Algohandle.
 * @params pIsEncrypt  is the pointer to the boolean result.
 *
 * @retval  NvSuccess if operation is successful.
 * @retval  NvError_InvalidAddress either if AlgoHandle or pIsEncrypt are
 * not valid.
 */
static NvError
SeQueryIsEncrypt(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvBool *pIsEncrypt);

/**
 * Gives the info if the padding is explicit or not.
 *
 * @params AlgoHandle Algohandle.
 * @params pIsExplicitPadding pointer to the booelan result.
 *
 * @retval  NvSuccess if operation is successful.
 * @retval  NvError_InvalidAddress either if AlgoHandle or pIsExplicitPadding are
 * not valid
 */
static NvError
SeQueryIsExplicitPadding(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvBool *pIsExplicitPadding);

/**
 * Gives the block size of the Aes operation.
 *
 * @params AlgoHandle Algohandle.
 * @params pBlockSize pointer to the output blocksize.
 *
 * @retval  NvSuccess if operation is successful.
 * @retval  NvError_InvalidAddress either if AlgoHandle or pBlockSize are not
 * valid.
 */
static NvError
SeQueryBlockSize(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvU32 *pBlockSize);
/**
 * pads the data in pBuufer.
 *
 * @params AlgoHandle Algohandle.
 * @Params pPaddingSize size of the data padded.
 * @params pBuffer buffer which is been padded.
 *
 * @retval  NvSuccess if operation is successful.
 * @retval  NvError_InvalidAddress if any of AlgoHandle, pPaddingSize or
 * pBuffer are not valid
 */
static NvError
SeQueryPadding(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvU32 *pPaddingSize,
        NvU8 *pBuffer);

/**
 * pads the data in pBuufer.
 *
 * @params AlgoHandle Algohandle.
 * @params PayloadSize size of the data buffer
 * @params pPaddingSize pointer to the padding size
 * @params pBuffer pointer to the buffer
 *
 * @retval  NvSuccess if operation is successful.
 * @retval  NvError_InvalidAddress if any of AlgoHandle, pPaddingSize or
 * pBuffer are not valid
 */
static NvError
SeQueryPaddingByPayloadSize(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvU32 PayloadSize,
        NvU32 *pPaddingSize,
        NvU8 *pBuffer);

/**
 * gets the optimul buffer alignment
 *
 * @params AlgoHandle Algohandle.
 * @params pAlignment pointer to output (optimal buffer alignment).
 *
 * @retval  NvSuccess if operation is successful.
 * @retval  NvError_InvalidAddress if either of AlgoHandle or pAlignment is not
 * valid.
 */
static NvError
SeQueryOptimalBufferAlignment(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvU32 *pAlignment);

/**
 * Process the blocks for operation (not hashing).
 *
 * @params AlgoHandle Algohandle.
 * @params NumBytes Number of input bytes.
 * @params pSrcBuffer pointer to the source data.
 * @params pDstBuffer pointer to the destination.
 * @params IsFirstBlock specifies if the block is first block.
 * @params IsLastBlock specifies if the block is last block.
 *
 * @retval: NvSuccess if operation is successful.
 * @retval: NvError_NotInitialized if any of SeAlgoHandle, pSrcBuffer,
 * pDstBuffer are not valid.
 */
static NvError SeProcessBlocks (
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvU32 NumBytes,
        const void *pSrcBuffer,
        void *pDstBuffer,
        NvBool IsFirstBlock,
        NvBool IsLastBlock);

/**
 * Release the algoritham
 *
 * @params AlgoHandle algo handle.
 */
static void
SeReleaseAlgorithm(
        NvCryptoCipherAlgoHandle AlgoHandle);

NvError SePerformCmacOperation(
        NvU8* Key, NvU32 KeyLen, NvU8 *pSrcBuf, NvU8 *pDstBuf, NvU32 SrcBufSize,
        NvBool IsFirstChunk, NvBool IsLastChunk, NvBool IsSbkKey);

NvError
SeQueryAlgoType(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvCryptoCipherAlgoType *pAlgoType)
{
    NvCryptoCipherAlgoAesHandle pAlgo = (NvCryptoCipherAlgoAesHandle)AlgoHandle;
    if (!pAlgo || !pAlgoType)
        return NvError_InvalidAddress;

    *pAlgoType = pAlgo->AlgoType;
    return NvSuccess;
}

NvError
SeQueryIsEncrypt(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvBool *pIsEncrypt)
{
    NvCryptoCipherAlgoAesHandle pAlgo = (NvCryptoCipherAlgoAesHandle)AlgoHandle;
    if (!pAlgo || !pIsEncrypt)
        return NvError_InvalidAddress;

    *pIsEncrypt = (pAlgo->IsEncrypt) ? 1 : 0;
    return NvSuccess;
}

NvError
SeQueryIsExplicitPadding(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvBool *pIsExplicitPadding)
{
    NvError e = NvSuccess;
    NvCryptoCipherAlgoAesHandle pAlgo = (NvCryptoCipherAlgoAesHandle)AlgoHandle;

    if (!pAlgo || !pIsExplicitPadding)
        return NvError_InvalidAddress;

    NV_CHECK_ERROR(NvCryptoPaddingQueryIsExplicit(pAlgo->PaddingType,
                pIsExplicitPadding));
    return e;
}

NvError
SeQueryBlockSize(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvU32 *pBlockSize)
{
    NvCryptoCipherAlgoAesHandle pAlgo = (NvCryptoCipherAlgoAesHandle)AlgoHandle;

    if (!pAlgo || !pBlockSize)
        return NvError_InvalidAddress;

    *pBlockSize = NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES;
    return NvSuccess;
}

NvError
SeQueryPadding(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvU32 *pPaddingSize,
        NvU8 *pBuffer)
{
    NvError e = NvSuccess;
    NvBool IsExplicit;
    NvU32 BufferSize;
    NvCryptoCipherAlgoAesHandle pAlgo = (NvCryptoCipherAlgoAesHandle)AlgoHandle;

    if (!pAlgo || !pPaddingSize || !pBuffer)
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
SeQueryPaddingByPayloadSize(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvU32 PayloadSize,
        NvU32 *pPaddingSize,
        NvU8 *pBuffer)
{
    NvError e = NvSuccess;
    NvBool IsExplicit;
    NvU32 BufferSize;
    NvBool IsEmptyPayload;
    NvU32 PayloadSizeModuloBlockSize;
    NvCryptoCipherAlgoAesHandle pAlgo = (NvCryptoCipherAlgoAesHandle)AlgoHandle;

    if (!pAlgo || !pPaddingSize || !pBuffer)
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
    IsEmptyPayload = PayloadSize ? NV_FALSE : NV_TRUE;
    PayloadSizeModuloBlockSize = PayloadSize %
        NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES;

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

static NvError
ConvertToSeAesKeyType(
        NvCryptoCipherAlgoAesKeyType CryptoKeyType,
        NvDdkSeAesKeyType *pAesKeyType)
{
    switch(CryptoKeyType)
    {
        case NvCryptoCipherAlgoAesKeyType_SecureBootKey:
            *pAesKeyType = NvDdkSeAesKeyType_SecureBootKey;
            break;
        case NvCryptoCipherAlgoAesKeyType_SecureStorageKey:
            *pAesKeyType = NvDdkSeAesKeyType_SecureStorageKey;
            break;
        case NvCryptoCipherAlgoAesKeyType_UserSpecified:
            *pAesKeyType = NvDdkSeAesKeyType_UserSpecified;
            break;
        default:
            return NvError_BadParameter;
    }
    return NvSuccess;
}

static NvError
ConvertToSeAesKeySize(
            NvCryptoCipherAlgoAesKeySize CryptoKeySize,
            NvDdkSeAesKeySize *pAesKeySize)
{
    switch(CryptoKeySize)
    {
        case NvCryptoCipherAlgoAesKeySize_128Bit:
            *pAesKeySize = NvDdkSeAesKeySize_128Bit;
            break;
        case NvCryptoCipherAlgoAesKeySize_192Bit:
            *pAesKeySize = NvDdkSeAesKeySize_192Bit;
            break;
        case NvCryptoCipherAlgoAesKeySize_256Bit:
            *pAesKeySize = NvDdkSeAesKeySize_256Bit;
            break;
        default:
            return NvError_BadParameter;
    }
    return NvSuccess;
}

static NvError
ConvertToSeAesOperationalMode(
        NvCryptoCipherAlgoType CryptoAlgoType,
        NvDdkSeAesOperationalMode *pAesOperationalMode)
{
    switch(CryptoAlgoType)
    {
        case NvCryptoCipherAlgoType_AesCbc:
            *pAesOperationalMode = NvDdkSeAesOperationalMode_Cbc;
            break;
        case NvCryptoCipherAlgoType_AesEcb:
            *pAesOperationalMode = NvDdkSeAesOperationalMode_Ecb;
            break;
        default:
            return NvError_BadParameter;
    }
    return NvSuccess;
}


static NvError
SetSeAlgoParams(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvCryptoCipherAlgoParams *pAlgoParams)
{
    NvError e = NvSuccess;
    NvDdkSeAesOperation OpInfo;
    NvDdkSeAesKeyInfo   KeyInfo;
    NvCryptoCipherAlgoParamsAesCbc *pParams = &(pAlgoParams->AesCbc);
    NvDdkSeAesKeyType SeAesKeyType = NvDdkSeAesKeyType_Invalid;
    NvDdkSeAesKeySize SeAesKeySize = NvDdkSeAesKeySize_Invalid;
    NvDdkSeAesOperationalMode SeAesOperationalMode = NvDdkSeAesOperationalMode_Invalid;
    NvCryptoCipherAlgoAesHandle pAlgo = (NvCryptoCipherAlgoAesHandle)AlgoHandle;

    if (!pAlgo || !pAlgoParams)
        return NvError_NotInitialized;

    switch (pParams->PaddingType)
    {
        case NvCryptoPaddingType_ExplicitBitPaddingAlways:
        case NvCryptoPaddingType_ExplicitBitPaddingOptional:
            break;
        default:
            return NvError_BadParameter;
    }

    // convert from NvCrypto types to NvDdkSeAes types
    NV_CHECK_ERROR(ConvertToSeAesKeyType(pParams->KeyType,
                &SeAesKeyType));
    NV_CHECK_ERROR(ConvertToSeAesKeySize(pParams->KeySize,
                &SeAesKeySize));
    NV_CHECK_ERROR(ConvertToSeAesOperationalMode(
                pAlgo->AlgoType,
                &SeAesOperationalMode));
    pAlgo->IsEncrypt = pParams->IsEncrypt;
    pAlgo->PaddingType = pParams->PaddingType;
    NvOsMemcpy(pAlgo->InitialVector, pParams->InitialVectorBytes,
                           sizeof(pParams->InitialVectorBytes));

    /// Select operation
    OpInfo.OpMode    = SeAesOperationalMode;
    OpInfo.IsEncrypt = pAlgo->IsEncrypt;
    NV_CHECK_ERROR_CLEANUP(pAlgo->AesHandle->NvDdkBlockDevIoctl(
                pAlgo->AesHandle,
                NvDdkSeAesBlockDevIoctlType_SelectOperation,
                sizeof (OpInfo),
                0,
                (const void *)&OpInfo,
                NULL));

    /// Set the key into the key slot
    KeyInfo.KeyType   = SeAesKeyType;
    KeyInfo.KeyLength = SeAesKeySize;
    NvOsMemcpy(KeyInfo.Key, pParams->KeyBytes, sizeof(pParams->KeyBytes));
    NV_CHECK_ERROR_CLEANUP(pAlgo->AesHandle->NvDdkBlockDevIoctl(
                pAlgo->AesHandle,
                NvDdkSeAesBlockDevIoctlType_SelectKey,
                sizeof (NvDdkSeAesKeyInfo),
                0,
                (const void *)&KeyInfo,
                NULL));

    /// clobber key value
    NvOsMemset(KeyInfo.Key, 0x0, NvDdkSeAesConst_MaxKeyLengthBytes);
fail:
    return e;
}

NvError
SeQueryOptimalBufferAlignment(
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvU32 *pAlignment)
{
     // TODO hard coding  the value
     *pAlignment = (64 / 8);
     return NvSuccess;
}

NvError SeProcessBlocks (
        NvCryptoCipherAlgoHandle AlgoHandle,
        NvU32 NumBytes,
        const void *pSrcBuffer,
        void *pDstBuffer,
        NvBool IsFirstBlock,
        NvBool IsLastBlock)
{
    NvError e = NvSuccess;
    NvDdkSeAesProcessBufferInfo PbInfo;
    NvDdkSeAesSetIvInfo IvInfo;
    NvCryptoCipherAlgoAesHandle pAlgo = (NvCryptoCipherAlgoAesHandle)AlgoHandle;

    if (!pAlgo || !pSrcBuffer || !pDstBuffer)
        return NvError_NotInitialized;

    if (!NumBytes)
        return NvSuccess;

    if (NumBytes % NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES != 0)
        return NvError_InvalidSize;

    /// set Iv for the first operation
    if  (IsFirstBlock)
    {
        /// Set IV
        IvInfo.pIV       = pAlgo->InitialVector;
        IvInfo.VectorSize = NvDdkSeAesConst_IVLengthBytes;
        NV_CHECK_ERROR_CLEANUP(pAlgo->AesHandle->NvDdkBlockDevIoctl(
                    pAlgo->AesHandle,
                    NvDdkSeAesBlockDevIoctlType_SetIV,
                    sizeof (IvInfo),
                    0,
                    (const void *)&IvInfo,
                    NULL));
    }

    /// Process Buffer
    PbInfo.pSrcBuffer = (NvU8 *)pSrcBuffer;
    PbInfo.pDstBuffer = (NvU8 *)pDstBuffer;
    PbInfo.SrcBufSize = NumBytes;
    NV_CHECK_ERROR_CLEANUP(pAlgo->AesHandle->NvDdkBlockDevIoctl(
                pAlgo->AesHandle,
                NvDdkSeAesBlockDevIoctlType_ProcessBuffer,
                sizeof (PbInfo),
                0,
                (const void *)&PbInfo,
                NULL));
fail:
    return e;
}

NvError NvCryptoCipherSelectAlgorithmSeAes(NvCryptoCipherAlgoType CipherAlgoType,
        NvCryptoCipherAlgoHandle *pCipherAlgoHandle)
{
    NvError e = NvSuccess;
    NvCryptoCipherAlgoAesHandle pAlgo;

    if (!pCipherAlgoHandle)
        return NvError_NotInitialized;

    pAlgo = (NvCryptoCipherAlgoAesHandle)NvOsAlloc(sizeof (struct NvCryptoCipherAlgoAesRec));
    if (!pAlgo)
        return NvError_InsufficientMemory;

    /// Zero Out the memory for clarity sake.
    NvOsMemset(pAlgo, 0x0, sizeof (struct NvCryptoCipherAlgoAesRec));

    switch (CipherAlgoType)
    {
        case NvCryptoCipherAlgoType_AesCbc:
        case NvCryptoCipherAlgoType_AesEcb:
            pAlgo->CipherAlgo.SetAlgoParams               = SetSeAlgoParams;
            pAlgo->CipherAlgo.QueryAlgoType               = SeQueryAlgoType;
            pAlgo->CipherAlgo.QueryIsEncrypt              = SeQueryIsEncrypt;
            pAlgo->CipherAlgo.QueryIsExplicitPadding      = SeQueryIsExplicitPadding;
            pAlgo->CipherAlgo.QueryBlockSize              = SeQueryBlockSize;
            pAlgo->CipherAlgo.QueryPadding                = SeQueryPadding;
            pAlgo->CipherAlgo.QueryPaddingByPayloadSize   = SeQueryPaddingByPayloadSize;
            pAlgo->CipherAlgo.QueryOptimalBufferAlignment = SeQueryOptimalBufferAlignment;
            pAlgo->CipherAlgo.ProcessBlocks               = SeProcessBlocks;
            pAlgo->CipherAlgo.ReleaseAlgorithm            = SeReleaseAlgorithm;
            pAlgo->CipherAlgo.SePerformCmacOperation      = SePerformCmacOperation;
            break;
        default:
            e = NvError_NotSupported;
            goto fail;
    }

     /// initialize context data
     pAlgo->AlgoType = CipherAlgoType;
     pAlgo->IsEncrypt = NV_FALSE;
     pAlgo->IsEmptyPayload = NV_TRUE;
     pAlgo->PayloadSizeModuloBlockSize = 0;
     pAlgo->AesHandle = NULL;
     pAlgo->PaddingType = NvCryptoPaddingType_Invalid;

    /// Open the Se block device
    NV_CHECK_ERROR_CLEANUP(NvDdkBlockDevMgrDeviceOpen(
                NvDdkBlockDevMgrDeviceId_Se,
                0,
                0,
                &pAlgo->AesHandle));
    *pCipherAlgoHandle = (NvCryptoCipherAlgoHandle)pAlgo;
    return NvSuccess;
fail:
    NvOsFree(pAlgo);
    return e;
}

void
SeReleaseAlgorithm(
        NvCryptoCipherAlgoHandle AlgoHandle)
{
    NvCryptoCipherAlgoAesHandle pAlgo = (NvCryptoCipherAlgoAesHandle)AlgoHandle;
    if (!pAlgo)
        return;

    /// Close the Se engine
    pAlgo->AesHandle->NvDdkBlockDevClose(pAlgo->AesHandle);
    pAlgo->AesHandle = NULL;

    /// Reclaim the memory
    NvOsFree(pAlgo);
}

NvError SePerformCmacOperation(
        NvU8* Key, NvU32 KeyLen, NvU8 *pSrcBuf, NvU8 *pDstBuf, NvU32 SrcBufSize,
        NvBool IsFirstChunk, NvBool IsLastChunk, NvBool IsSbkKey)
{
    NvError e = NvSuccess;
    NvDdkSeAesComputeCmacInInfo CmacInInfo;
    NvDdkSeAesComputeCmacOutInfo CmacOutInfo;
    NvDdkBlockDevHandle SeHandle;

    NV_CHECK_ERROR_CLEANUP(NvDdkSeBlockDevOpen(0, 0, &SeHandle));

    CmacInInfo.Key = Key;
    CmacInInfo.KeyLen = KeyLen;
    CmacInInfo.pBuffer = pSrcBuf;
    CmacInInfo.BufSize = SrcBufSize;
    CmacInInfo.IsFirstChunk = IsFirstChunk;
    CmacInInfo.IsLastChunk  = IsLastChunk;
    CmacInInfo.IsSbkKey  = IsSbkKey;

    if (IsLastChunk)
    {
        CmacOutInfo.pCMAC = pDstBuf;
        CmacOutInfo.CMACLen = 16;
    }
    NV_CHECK_ERROR_CLEANUP(SeHandle->NvDdkBlockDevIoctl(
                SeHandle,
                NvDdkSeAesBlockDevIoctlType_CalculateCMAC,
                sizeof(NvDdkSeAesComputeCmacInInfo),
                sizeof(NvDdkSeAesComputeCmacOutInfo),
                (const void *)&CmacInInfo,
                (void *)&CmacOutInfo));
fail:
    return e;
}
