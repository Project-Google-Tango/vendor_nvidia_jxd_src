/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcrypto_cipher_aes.h"
#include "nvcrypto_padding.h"
#include "nvddk_blockdevmgr.h"
#include "nvddk_aes_blockdev_defs.h"
#include "nvassert.h"

enum
{
    NVCRYPTO_CIPHER_AES_BLOCK_SIZE_BYTES = 16
};

NV_CT_ASSERT(NVCRYPTO_CIPHER_AES_MAX_KEY_BYTES == (int)NvDdkAesConst_MaxKeyLengthBytes);
NV_CT_ASSERT(NVCRYPTO_CIPHER_AES_IV_BYTES == (int)NvDdkAesConst_IVLengthBytes);

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
    NvDdkBlockDevHandle AesHandle;

    /// padding type
    NvCryptoPaddingType PaddingType;
    
    /// Initial vector to use when encrypting/decrypting the first block in the
    /// chain
    NvU8 InitialVector[NVCRYPTO_CIPHER_AES_IV_BYTES];
} NvCryptoCipherAlgoAes;

/**
 * convert AES key type from NvCrypto enum to NvDdkAes enum
 *
 * @param CryptoKeyType AES key type using NvCryptoCipherAlgoAesKeyType enum
 * @param pAesKeyType pointer to AES key type using NvDdkAesKeyType enum
 *
 * @retval NvError_Success Successful conversion
 * @retval NvError_BadParameter Unknown or unconvertable CryptoKeyType
 */
static NvError
ConvertToAesKeyType(
    NvCryptoCipherAlgoAesKeyType CryptoKeyType,
    NvDdkAesKeyType *pAesKeyType);

/**
 * convert AES key size from NvCrypto enum to NvDdkAes enum
 *
 * @param CryptoKeySize AES key size using NvCryptoCipherAlgoAesKeySize enum
 * @param pAesKeySize pointer to AES key size using NvDdkAesKeySize enum
 *
 * @retval NvError_Success Successful conversion
 * @retval NvError_BadParameter Unknown or unconvertable CryptoKeySize
 */
static NvError
ConvertToAesKeySize(
    NvCryptoCipherAlgoAesKeySize CryptoKeySize,
    NvDdkAesKeySize *pAesKeySize);

/**
 * convert AES operation type from NvCrypto enum to NvDdkAes enum
 *
 * @param CryptoAlgoType AES operation type using NvCryptoCipherAlgoType enum
 * @param pAesOperationalMode pointer to AES operation type using 
 * NvDdkAesOperationalMode enum
 *
 * @retval NvError_Success Successful conversion
 * @retval NvError_BadParameter Unknown or unconvertable CryptoAlgoType
 */
static NvError
ConvertToAesOperationalMode(
    NvCryptoCipherAlgoType CryptoAlgoType,
    NvDdkAesOperationalMode *pAesOperationalMode);

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
ConvertToAesKeyType(
    NvCryptoCipherAlgoAesKeyType CryptoKeyType,
    NvDdkAesKeyType *pAesKeyType)
{
    switch(CryptoKeyType)
    {
        case NvCryptoCipherAlgoAesKeyType_SecureBootKey:
            *pAesKeyType = NvDdkAesKeyType_SecureBootKey;
            break;
            
        case NvCryptoCipherAlgoAesKeyType_SecureStorageKey:
            *pAesKeyType = NvDdkAesKeyType_SecureStorageKey;
            break;
            
        case NvCryptoCipherAlgoAesKeyType_UserSpecified:
            *pAesKeyType = NvDdkAesKeyType_UserSpecified;
            break;
            
        default:
            return NvError_BadParameter;
    }
    
    return NvSuccess;
}

NvError
ConvertToAesKeySize(
    NvCryptoCipherAlgoAesKeySize CryptoKeySize,
    NvDdkAesKeySize *pAesKeySize)
{
    switch(CryptoKeySize)
    {
        case NvCryptoCipherAlgoAesKeySize_128Bit:
            *pAesKeySize = NvDdkAesKeySize_128Bit;
            break;
            
        case NvCryptoCipherAlgoAesKeySize_192Bit:
            *pAesKeySize = NvDdkAesKeySize_192Bit;
            break;
        
        case NvCryptoCipherAlgoAesKeySize_256Bit:
            *pAesKeySize = NvDdkAesKeySize_256Bit;
            break;
        
        default:
            return NvError_BadParameter;
    }
    
    return NvSuccess;
}

NvError
ConvertToAesOperationalMode(
    NvCryptoCipherAlgoType CryptoAlgoType,
    NvDdkAesOperationalMode *pAesOperationalMode)
{
    switch(CryptoAlgoType)
    {
        case NvCryptoCipherAlgoType_AesCbc:
            *pAesOperationalMode = NvDdkAesOperationalMode_Cbc;
            break;
            
        default:
            return NvError_BadParameter;
    }
    
    return NvSuccess;
}

NvError
SetAlgoParams(
    NvCryptoCipherAlgoHandle AlgoHandle,
    NvCryptoCipherAlgoParams *pAlgoParams)
{
    NvError e;
    
    NvCryptoCipherAlgoAesHandle pAlgo = (NvCryptoCipherAlgoAesHandle)AlgoHandle;
    NvCryptoCipherAlgoParamsAesCbc *pParams = &(pAlgoParams->AesCbc);

    NvDdkAesBlockDevIoctl_SelectKeyInputArgs SelectKey;
    NvDdkAesKeyType AesKeyType = NvDdkAesKeyType_Invalid;
    NvDdkAesKeySize AesKeySize = NvDdkAesKeySize_Invalid;

    NvDdkAesBlockDevIoctl_SelectOperationInputArgs SelectOperation;
    NvDdkAesOperationalMode AesOperationalMode = (NvDdkAesOperationalMode)0; // illegal
    
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

    // convert from NvCrypto types to NvDdkAes types
    NV_CHECK_ERROR(ConvertToAesKeyType(pParams->KeyType,
                                       &AesKeyType));

    NV_CHECK_ERROR(ConvertToAesKeySize(pParams->KeySize,
                                       &AesKeySize));

    NV_CHECK_ERROR(ConvertToAesOperationalMode(
                       pAlgo->AlgoType,
                       &AesOperationalMode));
    
    // populate context structure
    pAlgo->IsEncrypt = pParams->IsEncrypt;
    pAlgo->PaddingType = pParams->PaddingType;
    NvOsMemcpy(pAlgo->InitialVector, pParams->InitialVectorBytes, 
               sizeof(pParams->InitialVectorBytes));
    
    // select operation type
    SelectOperation.OpMode = AesOperationalMode;
    SelectOperation.IsEncrypt = pAlgo->IsEncrypt;
    
    NV_CHECK_ERROR(pAlgo->AesHandle->NvDdkBlockDevIoctl(
                       pAlgo->AesHandle,
                       NvDdkAesBlockDevIoctlType_SelectOperation,
                       sizeof(SelectOperation),
                       0,
                       &SelectOperation,
                       NULL));

    // select key
    SelectKey.KeyType = AesKeyType;
    SelectKey.KeyLength = AesKeySize;
    NvOsMemcpy(SelectKey.Key, pParams->KeyBytes, sizeof(pParams->KeyBytes));
    SelectKey.IsDedicatedKeySlot = NV_FALSE;
    
    NV_CHECK_ERROR(pAlgo->AesHandle->NvDdkBlockDevIoctl(
                       pAlgo->AesHandle,
                       NvDdkAesBlockDevIoctlType_SelectKey,
                       sizeof(SelectKey),
                       0,
                       &SelectKey,
                       NULL));
    
    // clobber key value
    NvOsMemset(SelectKey.Key, 0x0, NvDdkAesConst_MaxKeyLengthBytes);

    
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
    NvError e;

    NvCryptoCipherAlgoAesHandle pAlgo = (NvCryptoCipherAlgoAesHandle)AlgoHandle;
    NvDdkAesBlockDevIoctl_GetCapabilitiesOutputArgs GetCaps;
    
    if (!pAlgo)
        return NvError_InvalidAddress;
    
    if (!pAlignment)
        return NvError_InvalidAddress;
    
    NV_CHECK_ERROR(pAlgo->AesHandle->NvDdkBlockDevIoctl(
                       pAlgo->AesHandle,
                       NvDdkAesBlockDevIoctlType_GetCapabilities,
                       0,
                       sizeof(GetCaps),
                       NULL,
                       &GetCaps));
    
    *pAlignment = GetCaps.OptimalBufferAlignment;

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
    NvError e;

    NvDdkAesBlockDevIoctl_SetInitialVectorInputArgs SetInitialVector;
    
    NvDdkAesBlockDevIoctl_ProcessBufferInputArgs ProcessBuffer;

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
        NvOsMemcpy(SetInitialVector.InitialVector, pAlgo->InitialVector,
                   NvDdkAesConst_IVLengthBytes);
        NV_CHECK_ERROR(pAlgo->AesHandle->NvDdkBlockDevIoctl(
                           pAlgo->AesHandle,
                           NvDdkAesBlockDevIoctlType_SetInitialVector,
                           sizeof(SetInitialVector),
                           0,
                           &SetInitialVector,
                           NULL));
    }
    
    // perform crypto operation
    ProcessBuffer.BufferSize = NumBytes;
    ProcessBuffer.SrcBuffer = pSrcBuffer;
    ProcessBuffer.DestBuffer = pDstBuffer;
    ProcessBuffer.SkipOffset = 0;
    
    NV_CHECK_ERROR(pAlgo->AesHandle->NvDdkBlockDevIoctl(
                       pAlgo->AesHandle,
                       NvDdkAesBlockDevIoctlType_ProcessBuffer,
                       sizeof(ProcessBuffer),
                       0,
                       &ProcessBuffer,
                       NULL));
    
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
    pAlgo->AesHandle->NvDdkBlockDevClose(pAlgo->AesHandle);
    pAlgo->AesHandle = NULL;

    // free context
    NvOsFree(pAlgo);
}

NvError
NvCryptoCipherSelectAlgorithmAes(
    NvCryptoCipherAlgoType CipherAlgoType,
    NvCryptoCipherAlgoHandle *pCipherAlgoHandle)
{
    NvError e;
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

    NV_CHECK_ERROR_CLEANUP(NvDdkBlockDevMgrDeviceOpen(
                               NvDdkBlockDevMgrDeviceId_Aes,
                               0, /* Instance */
                               0, /* MinorInstance */
                               &pAlgo->AesHandle));
    
    *pCipherAlgoHandle = (NvCryptoCipherAlgoHandle)pAlgo;

    return NvSuccess;
    
fail:

    NvOsFree(pAlgo);
    return e;
}

    
