/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvcrypto_random_ansix931.h"
#include "nvddk_aes_blockdev.h"
#include "nvos.h"
#include "nvassert.h"
#include "nvrm_init.h"
#include "nvrm_module.h"
#include "nvddk_blockdevmgr.h"

/// NVCRYPTO_COMPUTE_RANDOM_DT_UPFRONT set to 1 enables computing all the blocks
/// of DT vector up-front, the resulting random data will be "less random" since
/// sampling the system time over a shorter interval and hence are more likely to
/// end up re-using the same(nearest) DT value multiple times. 
/// This method computes the random number faster as it processes all blocks in 
/// single go.
/// NVCRYPTO_COMPUTE_RANDOM_DT_UPFRONT set to 0 enables each block of DT to be 
/// updated before the corresponding block of R, I, and V are computed. Thus, the 
/// calculation would be -- compute a block of DT, compute corresponding blocks 
/// of R/I/V, then compute the next block of DT, followed by the next block of 
/// R/I/V, etc., etc. In this case resulting random data is "more random" and 
/// takes comparatively more time as single block is computed at a time.
#define NVCRYPTO_COMPUTE_RANDOM_DT_UPFRONT 0

/// forward declaration
typedef struct NvCryptoRandomAlgoAnsiX931AesRec *NvCryptoRandomAlgoAnsiX931AesHandle;

typedef struct NvCryptoRandomAlgoAnsiX931AesRec
{
    /**
     * state information required for all cipher algorithms
     */

    /// standard entry points
    NvCryptoRandomAlgo RandomAlgo;

    /**
     * state information specific to the Ansi X9.31 AES algorithm
     */

    /// cipher algorithm type
    NvCryptoRandomAlgoType AlgoType;
    
    /// handle to Aes Block driver
    NvDdkBlockDevHandle AesHandle;

    /// counter, used to ensure DT vector changes even if timer doesn't advance
    NvU8 Counter;
    
    /// chip's unique id, used to compute next DT vector
    NvU64 UniqueId;
} NvCryptoRandomAlgoAnsiX931Aes;


// locally defined static functions

/**
 * The following routines are defined according to the prototypes given in
 * nvcrypto_random.h.  Full documentation can be found there.
 */
static NvError
SetAlgoParams(
    NvCryptoRandomAlgoHandle AlgoHandle,
    NvCryptoRandomAlgoParams *pAlgoParams);

static NvError
QueryAlgoType(
    NvCryptoRandomAlgoHandle AlgoHandle,
    NvCryptoRandomAlgoType *pAlgoType);

static NvError
QuerySeedSize(
    NvCryptoRandomAlgoHandle AlgoHandle,
    NvU32 *pSeedSize);

static NvError
QuerySeed(
    NvCryptoRandomAlgoHandle AlgoHandle,
    void *pSeed);

static NvError
SetSeed(
    NvCryptoRandomAlgoHandle AlgoHandle,
    void *pSeed);

static NvError
GenerateSamples(
    NvCryptoRandomAlgoHandle AlgoHandle,
    NvU32 NumBytes,
    void *pBuffer);

static void
ReleaseAlgorithm(
    NvCryptoRandomAlgoHandle AlgoHandle);

// end of static function definitions


static NvError
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

static NvError
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

static NvError
SetAlgoParams(
    NvCryptoRandomAlgoHandle AlgoHandle,
    NvCryptoRandomAlgoParams *pAlgoParams)
{
    NvError e = NvSuccess;
    
    NvCryptoRandomAlgoAnsiX931AesHandle pAlgo = 
        (NvCryptoRandomAlgoAnsiX931AesHandle)AlgoHandle;
    NvCryptoRandomAlgoParamsAnsiX931Aes *pRandomParams = &(pAlgoParams->AnsiX931Aes);
    NvDdkAesBlockDevIoctl_SelectKeyInputArgs SelectKey;
    NvDdkAesBlockDevIoctl_SetInitialVectorInputArgs SeedVector;
    NvDdkAesBlockDevIoctl_SelectOperationInputArgs SelectOperation;
    NvDdkAesKeyType AesKeyType = NvDdkAesKeyType_Invalid;
    NvDdkAesKeySize AesKeySize = NvDdkAesKeySize_Invalid;


    // check for valid inputs
    if (!pAlgo)
        return NvError_InvalidAddress;

    if (!pAlgoParams)
        return NvError_InvalidAddress;


    // select operation type
    SelectOperation.OpMode = NvDdkAesOperationalMode_AnsiX931;
    SelectOperation.IsEncrypt = NV_TRUE;  // not used in ANSIX931 mode
    NV_CHECK_ERROR(pAlgo->AesHandle->NvDdkBlockDevIoctl(
                       pAlgo->AesHandle,
                       NvDdkAesBlockDevIoctlType_SelectOperation,
                       sizeof(SelectOperation),
                       0,
                       &SelectOperation,
                       NULL));

    // convert from NvCrypto types to NvDdkAes types
    NV_CHECK_ERROR(ConvertToAesKeyType(pRandomParams->KeyType,
                                       &AesKeyType));

    NV_CHECK_ERROR(ConvertToAesKeySize(pRandomParams->KeySize,
                                       &AesKeySize));

    // select key
    SelectKey.KeyType = AesKeyType;
    SelectKey.KeyLength = AesKeySize;
    NvOsMemcpy(SelectKey.Key, pRandomParams->KeyBytes, sizeof(pRandomParams->KeyBytes));
    SelectKey.IsDedicatedKeySlot = NV_FALSE;
    
    NV_CHECK_ERROR(pAlgo->AesHandle->NvDdkBlockDevIoctl(
                       pAlgo->AesHandle,
                       NvDdkAesBlockDevIoctlType_SelectKey,
                       sizeof(SelectKey),
                       0,
                       &SelectKey,
                       NULL));
    // overwrite key data
    NvOsMemset(SelectKey.Key, 0x0, NvDdkAesConst_MaxKeyLengthBytes);

    // set initial seed
    NvOsMemcpy(SeedVector.InitialVector, pRandomParams->InitialSeedBytes,
               sizeof(pRandomParams->InitialSeedBytes));

    NV_CHECK_ERROR(pAlgo->AesHandle->NvDdkBlockDevIoctl(
                       pAlgo->AesHandle,
                       NvDdkAesBlockDevIoctlType_SetInitialVector,
                       sizeof(SeedVector),
                       0,
                       &SeedVector,
                       NULL));

    // initialize counter
    pAlgo->Counter = 0;

    return NvSuccess;
}

static NvError
QueryAlgoType(
    NvCryptoRandomAlgoHandle AlgoHandle,
    NvCryptoRandomAlgoType *pAlgoType)
{
    NvCryptoRandomAlgoAnsiX931AesHandle pAlgo = 
        (NvCryptoRandomAlgoAnsiX931AesHandle)AlgoHandle;

    if (!pAlgo)
        return NvError_InvalidAddress;

    if (!pAlgoType)
        return NvError_InvalidAddress;
    
    *pAlgoType = pAlgo->AlgoType;

    return NvSuccess;
}

static NvError
QuerySeedSize(
    NvCryptoRandomAlgoHandle AlgoHandle,
    NvU32 *pSeedSize)
{
    NvCryptoRandomAlgoAnsiX931AesHandle pAlgo = 
        (NvCryptoRandomAlgoAnsiX931AesHandle)AlgoHandle;
    
    if (!pAlgo)
        return NvError_InvalidAddress;
    
    if (!pSeedSize)
        return NvError_InvalidAddress;
    
    *pSeedSize = NvDdkAesConst_BlockLengthBytes;

    return NvSuccess;
}

static NvError
QuerySeed(
    NvCryptoRandomAlgoHandle AlgoHandle,
    void *pSeed)
{
    NvError e = NvSuccess;
    NvCryptoRandomAlgoAnsiX931AesHandle pAlgo = 
        (NvCryptoRandomAlgoAnsiX931AesHandle)AlgoHandle;
    NvDdkAesBlockDevIoctl_GetInitialVectorOutputArgs SeedVector;

    if (!pAlgo)
        return NvError_InvalidAddress;
    
    if (!pSeed)
        return NvError_InvalidAddress;

    NV_CHECK_ERROR(pAlgo->AesHandle->NvDdkBlockDevIoctl(
                       pAlgo->AesHandle,
                       NvDdkAesBlockDevIoctlType_GetInitialVector,
                       0,
                       sizeof(SeedVector),
                       NULL,
                       &SeedVector));

    NvOsMemcpy((NvU8 *)pSeed, SeedVector.InitialVector, NvDdkAesConst_BlockLengthBytes);

    return e;
}

static NvError
SetSeed(
    NvCryptoRandomAlgoHandle AlgoHandle,
    void *pSeed)
{
    NvError e = NvSuccess;
    NvCryptoRandomAlgoAnsiX931AesHandle pAlgo = 
        (NvCryptoRandomAlgoAnsiX931AesHandle)AlgoHandle;
    NvDdkAesBlockDevIoctl_SetInitialVectorInputArgs SeedVector;

    if (!pAlgo)
        return NvError_InvalidAddress;
    
    if (!pSeed)
        return NvError_InvalidAddress;

    NvOsMemcpy(SeedVector.InitialVector, pSeed, NvDdkAesConst_BlockLengthBytes);

    NV_CHECK_ERROR(pAlgo->AesHandle->NvDdkBlockDevIoctl(
                       pAlgo->AesHandle,
                       NvDdkAesBlockDevIoctlType_SetInitialVector,
                       sizeof(SeedVector),
                       0,
                       &SeedVector,
                       NULL));

    return e;
}

static void
ReleaseAlgorithm(
    NvCryptoRandomAlgoHandle AlgoHandle)
{
    NvCryptoRandomAlgoAnsiX931AesHandle pAlgo = 
        (NvCryptoRandomAlgoAnsiX931AesHandle)AlgoHandle;
    
    if (!pAlgo)
        return;

    // close AES engine
    pAlgo->AesHandle->NvDdkBlockDevClose(pAlgo->AesHandle);
    pAlgo->AesHandle = NULL;

    // free context
    NvOsFree(pAlgo);
}

static NvError 
ProcessRandomSample(
    NvCryptoRandomAlgoAnsiX931AesHandle pAlgo,
    NvU32 TotalNoOfBlocks,
    void *pBuffer)
{
    NvDdkAesBlockDevIoctl_ProcessBufferInputArgs RandomValBuffer;
    NvU8 *pDateTimeVector = pBuffer;
    NvError e = NvSuccess;
    NvU64 TimeUS;

    // Fill the process buffer input args for random number generation.
    RandomValBuffer.BufferSize = (TotalNoOfBlocks * NvDdkAesConst_BlockLengthBytes);
    RandomValBuffer.DestBuffer = pBuffer;
    RandomValBuffer.SrcBuffer  = pDateTimeVector;
    RandomValBuffer.SkipOffset = 0;

    // prepare date/time (DT) input vector
    while (TotalNoOfBlocks)
    {
        // For X9.31, DT is a 128-bit quantity.  It is constructed as follows --
        // * 64 bits encode the time in microseconds.  Since 2^64 usec ~= 585000
        //   years, upper 8 bits are unlikely to change (depending on epoch
        //   value); so XOR a wrap-around counter in the upper 8 bits.
        //   Note: 2^56 usec ~= 2280 years.
        // * 64 bits encode the chip's Unique Id.  This helps ensure that different
        //   chips will generate different pseudo-random sequences
        TimeUS = NvOsGetTimeUS();
        TimeUS ^= ((NvU64)pAlgo->Counter) << 56;
        pAlgo->Counter++;
        NvOsMemcpy(pDateTimeVector, &TimeUS, sizeof(TimeUS));
        NvOsMemcpy(pDateTimeVector + sizeof(TimeUS), &pAlgo->UniqueId, sizeof(pAlgo->UniqueId));

        TotalNoOfBlocks--; 
        pDateTimeVector += NvDdkAesConst_BlockLengthBytes;
    }

    // Process the buffer for generating the random number.
    NV_CHECK_ERROR(pAlgo->AesHandle->NvDdkBlockDevIoctl(
                   pAlgo->AesHandle,
                   NvDdkAesBlockDevIoctlType_ProcessBuffer,
                   sizeof(RandomValBuffer),
                   0,
                   &RandomValBuffer,
                   NULL));

    return e;
}


static NvError
GenerateSamples(
    NvCryptoRandomAlgoHandle AlgoHandle,
    NvU32 NumBytes,
    void *pBuffer)
{
    NvCryptoRandomAlgoAnsiX931AesHandle pAlgo = 
        (NvCryptoRandomAlgoAnsiX931AesHandle)AlgoHandle;
    NvError e = NvSuccess;
    NvU8 *pRandomBuf = pBuffer;
    NvU32 TotalNoOfBlocks = 0;

    if (!pAlgo)
        return NvError_InvalidAddress;

    if (!NumBytes)
        return NvSuccess;

    if (!pBuffer)
        return NvError_InvalidAddress;


    // Calaculate total number of blocks 
    TotalNoOfBlocks = (NumBytes / NvDdkAesConst_BlockLengthBytes);

#if NVCRYPTO_COMPUTE_RANDOM_DT_UPFRONT
    // Generate the Random samples for total blocks.
    NV_CHECK_ERROR(ProcessRandomSample(pAlgo, TotalNoOfBlocks, pRandomBuf));
#else
    while (TotalNoOfBlocks)
    {
        // Generate the Random samples for each blocks.
        NV_CHECK_ERROR(ProcessRandomSample(pAlgo, 1, pRandomBuf));
        // Decrement then Block count
        TotalNoOfBlocks--; 
        // Advance the buffer pointer
        pRandomBuf += NvDdkAesConst_BlockLengthBytes;
    }
#endif

    // Check total number of blocks is multiples of AES block size?
    if (NumBytes % NvDdkAesConst_BlockLengthBytes)
    {
        NvU8 DT[NvDdkAesConst_BlockLengthBytes];
        // We are here here means we have few bytes left for RNG,
        // process these bytes as one block AES random sample, in the local buffer.
        NV_CHECK_ERROR(ProcessRandomSample(pAlgo, 1, DT));
        // Copy the left over random vector generated to the client buffer 
        NvOsMemcpy(pRandomBuf + (TotalNoOfBlocks * NvDdkAesConst_BlockLengthBytes), 
                   DT, (NumBytes % NvDdkAesConst_BlockLengthBytes));
    }

    return e;
}

NvError
NvCryptoRandomSelectAlgorithmAnsiX931Aes(
    NvCryptoRandomAlgoType RandomAlgoType,
    NvCryptoRandomAlgoHandle *pRandomAlgoHandle)
{
    NvError e = NvSuccess;
    NvCryptoRandomAlgoAnsiX931AesHandle pAlgo = NULL;
    NvRmDeviceHandle RmDeviceHandle = NULL;

    if (!pRandomAlgoHandle)
        return NvError_InvalidAddress;
    
    switch(RandomAlgoType)
    {
        case NvCryptoRandomAlgoType_AnsiX931Aes:
            break;
        default:
            return NvError_BadParameter;
    }

    pAlgo = (NvCryptoRandomAlgoAnsiX931AesHandle)
        NvOsAlloc(sizeof(NvCryptoRandomAlgoAnsiX931Aes));
    
    if (!pAlgo)
        return NvError_InsufficientMemory;

    // Clear context memory
    NvOsMemset(pAlgo, 0x0, sizeof(NvCryptoRandomAlgoAnsiX931Aes));

    // initialize entry points
    pAlgo->RandomAlgo.SetAlgoParams = SetAlgoParams;
    pAlgo->RandomAlgo.QueryAlgoType = QueryAlgoType;
    pAlgo->RandomAlgo.QuerySeedSize = QuerySeedSize;
    pAlgo->RandomAlgo.QuerySeed = QuerySeed;
    pAlgo->RandomAlgo.SetSeed = SetSeed;
    pAlgo->RandomAlgo.GenerateSamples = GenerateSamples;
    pAlgo->RandomAlgo.ReleaseAlgorithm = ReleaseAlgorithm;

    // initialize context data
    pAlgo->AlgoType = RandomAlgoType;
    pAlgo->AesHandle = NULL;

    // get chip Unique Id
    NV_CHECK_ERROR_CLEANUP(NvRmOpen(&RmDeviceHandle, 0));
    NV_CHECK_ERROR_CLEANUP(
        NvRmQueryChipUniqueId(RmDeviceHandle, 
                              sizeof(pAlgo->UniqueId), 
                              &pAlgo->UniqueId)
    );
    NvRmClose(RmDeviceHandle);
    RmDeviceHandle = NULL;

    // Request Blockdev manager to Open AES driver
    NV_CHECK_ERROR_CLEANUP(NvDdkBlockDevMgrDeviceOpen(
                               NvDdkBlockDevMgrDeviceId_Aes,
                               0, /* Instance */
                               0, /* MinorInstance */
                               &pAlgo->AesHandle));

    *pRandomAlgoHandle = (NvCryptoRandomAlgoHandle)pAlgo;

    return e;

fail:
    
    NvRmClose(RmDeviceHandle);
    
    // free context
    NvOsFree(pAlgo);

    return e;
}

