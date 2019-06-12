/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvos.h"
#include "nvassert.h"
#include "nvrm_power.h"
#include "nvrm_hardware_access.h"
#include "nvrm_module.h"
#include "nvrm_xpc.h"
#include "nvddk_aes.h"
#include "nvddk_aes_priv.h"
//fixmedlt to get rid of
#include "nvddk_aes_core.h"
#include "iram_address_allocation.h"
#include "t30/arapb_misc.h"

/// Enable when user key support is required
#define AES_ENABLE_USER_KEY 1

// RFC3394 key wrap block size is 64 bites which is equal to 8 bytes
#define AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES 8

// Number of RFC3394 key wrap blocks for 128 bit key
#define AES_RFC_3394_NUM_OF_BLOCKS_FOR_128BIT_KEY 2

// Number of RFC3394 key wrap blocks for 192 bit key
#define AES_RFC_3394_NUM_OF_BLOCKS_FOR_192BIT_KEY 3

// Number of RFC3394 key wrap blocks for 256 bit key
#define AES_RFC_3394_NUM_OF_BLOCKS_FOR_256BIT_KEY 4

static void AesCoreRequestHwAccess(void);
static void AesCoreReleaseHwAccess(void);
static void AesCoreDeAllocateRmMemory(AesHwContext *pAesHwCtxt);
static void AesCoreFreeUpEngine(void);
static void AesCoreDeInitializeEngineSpace(AesHwContext *pAesHwCtxt);

//fixmedlt static
//void AesCorePowerUp(AesCoreEngine *pAesCoreEngine);
//fixmedlt static
//void AesCorePowerDown(AesCoreEngine *pAesCoreEngine);

static NvError AesCoreAllocateRmMemory(AesHwContext *pAesHwCtxt);
static NvError AesCoreLoadSskToSecureScratchAndLock(NvRmDeviceHandle hRmDevice, const NvU8 *pKey);
static NvError AesCoreDfsBusyHint(NvRmDeviceHandle hRm, NvU32 PowerClientId, NvBool DfsOn);
static NvError AesCoreInitializeEngineSpace(NvRmDeviceHandle hRmDevice, AesHwContext *pAesHwCtxt);
static NvError AesCoreInitEngine(NvRmDeviceHandle hRmDevice);
static NvError AesCoreWrapKey(NvDdkAes *pAes, const NvU8 *pOrgKey, NvDdkAesKeySize KeySize,
                              NvU8 *pOrgIv, NvU8 *pWrapedKey, NvU8 *pWrapedIv);
static NvError AesCoreUnWrapKey(NvDdkAes *pAes, NvU8 *pWrapedKey, NvDdkAesKeySize KeySize,
                                NvU8 *pWrapedIv, NvU8 *pOrgKey, NvU8 *pOrgIv);
static NvError AesCoreGetFreeSlot(AesCoreEngine *pAesCoreEngine, AesHwEngine *pEngine,
                                  AesHwKeySlot *pKeySlot);
static NvError AesCoreGetCapabilities(NvRmDeviceHandle hRmDevice, AesHwCapabilities **caps);
static NvError AesCoreSetKey(NvDdkAes *pAes,
                             const NvDdkAesKeyType KeyType,
                             const NvBool IsDedicatedSlot,
                             const NvU8 *pKeyData,
                             const NvU32 KeySize);
static NvError AesCoreSetSbkKey(NvDdkAes *pAes, const NvDdkAesKeyType KeyType,
                                const NvBool IsDedicatedSlot, const NvU8 *pKeyData);
static NvError
AesCoreEcbProcessBuffer(
    NvDdkAes *pAes,
    NvU8 *pInputBuffer,
    NvU8 *pOutputBuffer,
    NvU32 BufSize,
    NvBool IsEncrypt);
static NvError
AesCoreProcessBuffer(
    NvDdkAes *pAes,
    NvU32 SkipOffset,
    NvU32 SrcBufferSize,
    NvU32 DestBufferSize,
    const NvU8 *pSrcBuffer,
    NvU8 *pDestBuffer);

static NvBool AesCoreClearUserKey(NvDdkAes *pAes);
static NvBool
AesCoreSbkClearCheck(
    AesHwContext *pAesHw,
    AesHwEngine Engine,
    AesHwKeySlot EncryptSlot,
    AesHwKeySlot DecryptSlot);
static NvBool
AesCoreSskLockCheck(
    AesHwContext *pAesHw,
    AesHwEngine Engine,
    AesHwKeySlot EncryptSlot,
    AesHwKeySlot DecryptSlot);
static NvBool
AesCoreUserKeyClearCheck(
    AesHwContext *pAesHw,
    AesHwEngine Engine,
    AesHwKeySlot KeySlot,
    AesKeySize KeySize);

static AesCoreEngine *s_pAesCoreEngine = NULL;
static AesHwCapabilities **gs_EngineCaps = NULL;
static NvOsMutexHandle s_AesCoreEngineMutex = {0};

// Original IV of size 8 byte which is used in RFC 3394 key wrap algorithm
static NvU8 s_OriginalIV[AES_RFC_IV_LENGTH_BYTES] =
{
    0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6
};

NvU32 g_AesHwKeySlotLengthBytes;

#define NVDDK_AES_CHECK_INPUT_PARAMS(PARM) \
    do \
    { \
        if ((!pAes) || (!PARM)) \
            return NvError_BadParameter; \
    } while (0)

NvError NvDdkAesOpen(NvU32 InstanceId, NvDdkAesHandle *phAes)
{
    NvError e = NvSuccess;
    NvDdkAes *pAes = NULL;
    NvOsMutexHandle Mutex = NULL;
    NvRmDeviceHandle hRmDevice = NULL;

    if (!phAes)
       return NvError_BadParameter;

    // Create mutex (if not already done)
    if (NULL == s_AesCoreEngineMutex)
    {
        e = NvOsMutexCreate(&Mutex);
        if (NvSuccess != e)
            return e;
        if (0 != NvOsAtomicCompareExchange32((NvS32*)&s_AesCoreEngineMutex, 0, (NvS32)Mutex))
            NvOsMutexDestroy(Mutex);
    }

    NvOsMutexLock(s_AesCoreEngineMutex);

    // Create client record
    pAes = NvOsAlloc(sizeof(NvDdkAes));
    if (!pAes)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    // Zero Out the Client memory initially
    NvOsMemset(pAes, 0, sizeof(NvDdkAes));

    NV_CHECK_ERROR_CLEANUP(NvRmOpen(&hRmDevice, 0));

    if (NULL == s_pAesCoreEngine)
    {
        // Init engine
        NV_CHECK_ERROR_CLEANUP(AesCoreInitEngine(hRmDevice));
        // Power up
        AesCorePowerUp(s_pAesCoreEngine);
    }

    // Add client
    s_pAesCoreEngine->OpenCount++;

    pAes->pAesCoreEngine = s_pAesCoreEngine;

    *phAes = pAes;
    NvOsMutexUnlock(s_AesCoreEngineMutex);
    return NvSuccess;

fail:
    NvDdkAesClose(pAes);
    NvOsMutexUnlock(s_AesCoreEngineMutex);
    return e;
}

void NvDdkAesClose(NvDdkAesHandle hAes)
{
    NvDdkAes *pClient = (NvDdkAes *)hAes;
    AesCoreEngine *pAesCoreEngine = NULL;

    if (!hAes)
       return;

    if (hAes->pAesCoreEngine)
    {
        pAesCoreEngine = hAes->pAesCoreEngine;

        NV_ASSERT(pAesCoreEngine);

        NvOsMutexLock(s_AesCoreEngineMutex);

        // Check if client is using USER key with dedicated slot then free the associated client slot
        if ((NV_TRUE == pClient->IsDedicatedSlot) &&
            (NvDdkAesKeyType_UserSpecified == pClient->KeyType))
        {
            // Client is using USER key with dedicated slot. So
            // clear the key in hardware slot and free the associated client slot
            if (!AesCoreClearUserKey(pClient))
            {
                NV_ASSERT(!"Failed to clear User Key");
            }
            pClient->pAesCoreEngine->IsKeySlotUsed[pClient->Engine][pClient->KeySlot] = NV_FALSE;
        }

        // Free up engine if no other clients
        if (pAesCoreEngine->OpenCount)
        {
            // Decrement the Open count
            pAesCoreEngine->OpenCount--;

            if (0 == pAesCoreEngine->OpenCount)
            {
                // Power down
                AesCorePowerDown(pAesCoreEngine);

                // Free up resources
                AesCoreFreeUpEngine();

                // Close Rm
                NvRmClose(pAesCoreEngine->AesHwCtxt.hRmDevice);

                NvOsFree(s_pAesCoreEngine);
                s_pAesCoreEngine = NULL;
            }
        }
        NvOsMutexUnlock(s_AesCoreEngineMutex);
    }

    NvOsMemset(pClient, 0, sizeof(NvDdkAes));
    NvOsFree(pClient);
}

NvError NvDdkAesSelectAndSetSbk(NvDdkAesHandle hAes, const NvDdkAesKeyInfo *pKeyInfo)
{
    NvDdkAes *pAes = (NvDdkAes*)hAes;

    NV_ASSERT(hAes);
    NVDDK_AES_CHECK_INPUT_PARAMS(pKeyInfo);

    switch (pKeyInfo->KeyLength)
    {
        case NvDdkAesKeySize_128Bit:
            return AesCoreSetSbkKey(pAes,
                pKeyInfo->KeyType,
                pKeyInfo->IsDedicatedKeySlot,
                pKeyInfo->Key);
            break;
        case NvDdkAesKeySize_192Bit:
        case NvDdkAesKeySize_256Bit:
        default:
            return NvError_NotSupported;
    }
}


NvError NvDdkAesSelectKey(NvDdkAesHandle hAes, const NvDdkAesKeyInfo *pKeyInfo)
{
    NvDdkAes *pAes = (NvDdkAes*)hAes;

    NV_ASSERT(hAes);
    NVDDK_AES_CHECK_INPUT_PARAMS(pKeyInfo);

    switch (pKeyInfo->KeyLength)
    {
        case NvDdkAesKeySize_128Bit:
        case NvDdkAesKeySize_192Bit:
        case NvDdkAesKeySize_256Bit:
            return AesCoreSetKey(pAes,
                pKeyInfo->KeyType,
                pKeyInfo->IsDedicatedKeySlot,
                pKeyInfo->Key,
                pKeyInfo->KeyLength);
            break;
        default:
            return NvError_NotSupported;
    }
}

NvError NvDdkAesSelectOperation(NvDdkAesHandle hAes, const NvDdkAesOperation *pOperation)
{
    NvDdkAes *pAes = (NvDdkAes*)hAes;

    NVDDK_AES_CHECK_INPUT_PARAMS(pOperation);

    switch (pOperation->OpMode)
    {
        case NvDdkAesOperationalMode_Cbc:
        case NvDdkAesOperationalMode_Ecb:
            pAes->IsEncryption = pOperation->IsEncrypt;
            break;
        case NvDdkAesOperationalMode_AnsiX931:
            pAes->IsEncryption = NV_TRUE;
            break;
        default:
            return NvError_NotSupported;
    }

    pAes->OpMode = pOperation->OpMode;
    return NvSuccess;
}

NvError NvDdkAesSetInitialVector(NvDdkAesHandle hAes, const NvU8 *pInitialVector, NvU32 VectorSize)
{
    NvDdkAes *pAes = (NvDdkAes*)hAes;

    NVDDK_AES_CHECK_INPUT_PARAMS(pInitialVector);

    // If the mode is ECB return error
    if (NvDdkAesOperationalMode_Ecb == pAes->OpMode)
        return NvError_NotSupported;

    if (VectorSize < NvDdkAesConst_IVLengthBytes)
        return NvError_BadParameter;

    // Set the IV and store it with client
    NvOsMemcpy(pAes->Iv, pInitialVector, NvDdkAesConst_IVLengthBytes);

    return NvSuccess;
}

NvError NvDdkAesGetInitialVector(NvDdkAesHandle hAes, NvU32 VectorSize, NvU8 *pInitialVector)
{
    NvDdkAes *pAes = (NvDdkAes*)hAes;

    NVDDK_AES_CHECK_INPUT_PARAMS(pInitialVector);

    if (VectorSize < NvDdkAesConst_IVLengthBytes)
        return NvError_BadParameter;

    NvOsMemcpy(pInitialVector, pAes->Iv, NvDdkAesConst_IVLengthBytes);

    return NvSuccess;
}

NvError
NvDdkAesProcessBuffer(
    NvDdkAesHandle hAes,
    NvU32 SrcBufferSize,
    NvU32 DestBufferSize,
    const NvU8 *pSrcBuffer,
    NvU8 *pDestBuffer)
{
    if ((!hAes) || (!pSrcBuffer) || (!pDestBuffer))
        return NvError_BadParameter;

    return AesCoreProcessBuffer((NvDdkAes*)hAes,
                                0,
                                SrcBufferSize,
                                DestBufferSize,
                                pSrcBuffer,
                                pDestBuffer);
}

NvError NvDdkAesClearSecureBootKey(NvDdkAesHandle hAes)
{
    NvError e = NvSuccess;
    NvDdkAes *pAes = (NvDdkAes*)hAes;
    AesHwContext *pAesHwCtxt = NULL;
    AesHwEngine Engine;

    if (!hAes)
        return NvError_BadParameter;

    NvOsMutexLock(s_AesCoreEngineMutex);

    for (Engine = AesHwEngine_A; Engine < AesHwEngine_Num; Engine++)
    {
        AesHwEngine SbkEngine = pAes->pAesCoreEngine->SbkEngine[Engine];
        AesHwKeySlot DecryptSlot = pAes->pAesCoreEngine->SbkDecryptSlot;
        AesHwKeySlot EncryptSlot = pAes->pAesCoreEngine->SbkEncryptSlot;

        if (SbkEngine < AesHwEngine_Num)
        {
            // Get the AES H/W context
            pAesHwCtxt = &pAes->pAesCoreEngine->AesHwCtxt;

            AesCoreRequestHwAccess();
            // Clear the SBK encrypt key
            gs_EngineCaps[SbkEngine]->pAesInterf->AesHwClearKeyAndIv(pAesHwCtxt,
                                                                     SbkEngine,
                                                                     EncryptSlot);
            // Clear the SBK decrypt key
            gs_EngineCaps[SbkEngine]->pAesInterf->AesHwClearKeyAndIv(pAesHwCtxt,
                                                                     SbkEngine,
                                                                     DecryptSlot);
            AesCoreReleaseHwAccess();

            if (!AesCoreSbkClearCheck(pAesHwCtxt, SbkEngine, EncryptSlot, DecryptSlot))
            {
                // Return error if SB clear check fails
                e = NvError_AesClearSbkFailed;
            }
        }
    }
    NvOsMutexUnlock(s_AesCoreEngineMutex);
    return e;
}

NvError NvDdkAesLockSecureStorageKey(NvDdkAesHandle hAes)
{
    NvError e = NvSuccess;
    NvDdkAes *pAes = (NvDdkAes*)hAes;
    AesHwContext *pAesHwCtxt = NULL;
    AesHwEngine Engine;

    if (!hAes)
        return NvError_BadParameter;

    NvOsMutexLock(s_AesCoreEngineMutex);

    NV_ASSERT(pAes->pAesCoreEngine);

    for (Engine = AesHwEngine_A; Engine < AesHwEngine_Num; Engine++)
    {
        AesHwEngine SskEngine = pAes->pAesCoreEngine->SskEngine[Engine];
        AesHwKeySlot DecryptSlot = pAes->pAesCoreEngine->SskDecryptSlot;
        AesHwKeySlot EncryptSlot = pAes->pAesCoreEngine->SskEncryptSlot;

        if (SskEngine < AesHwEngine_Num)
        {
            // Get the AES H/W context
            pAesHwCtxt = &pAes->pAesCoreEngine->AesHwCtxt;

            AesCoreRequestHwAccess();
            // Disable permissions to the SSK key slot in the AES engine
            gs_EngineCaps[SskEngine]->pAesInterf->AesHwLockSskReadWrites(pAesHwCtxt, SskEngine);
            AesCoreReleaseHwAccess();

            if (!AesCoreSskLockCheck(pAesHwCtxt, SskEngine, EncryptSlot, DecryptSlot))
            {
                e = NvError_AesLockSskFailed;
                goto fail;
            }

            // Also, lock the Secure scratch registers.
            NV_CHECK_ERROR_CLEANUP(AesCoreLoadSskToSecureScratchAndLock(pAesHwCtxt->hRmDevice, NULL));
        }
    }

fail:
    NvOsMutexUnlock(s_AesCoreEngineMutex);
    return e;
}

NvError
NvDdkAesSetAndLockSecureStorageKey(
    NvDdkAesHandle hAes,
    NvDdkAesKeySize KeyLength,
    const NvU8 *pSecureStorageKey)
{
    NvError e = NvSuccess;
    NvDdkAes *pAes = (NvDdkAes*)hAes;
    AesHwContext *pAesHwCtxt = NULL;
    AesHwIv Iv;
    AesHwEngine Engine;

    NvOsMutexLock(s_AesCoreEngineMutex);
    if (!s_pAesCoreEngine->SskUpdateAllowed)
    {
        NvOsMutexUnlock(s_AesCoreEngineMutex);
        return NvError_NotSupported;
    }
    NvOsMutexUnlock(s_AesCoreEngineMutex);

    NVDDK_AES_CHECK_INPUT_PARAMS(pSecureStorageKey);

    switch (KeyLength)
    {
        case NvDdkAesKeySize_128Bit:
            break;
        case NvDdkAesKeySize_192Bit:
        case NvDdkAesKeySize_256Bit:
        default:
            return NvError_NotSupported;
    }

    NvOsMutexLock(s_AesCoreEngineMutex);
    for (Engine = AesHwEngine_A; Engine < AesHwEngine_Num; Engine++)
    {
        AesHwEngine SskEngine = pAes->pAesCoreEngine->SskEngine[Engine];
        AesHwKeySlot DecryptSlot = pAes->pAesCoreEngine->SskDecryptSlot;
        AesHwKeySlot EncryptSlot = pAes->pAesCoreEngine->SskEncryptSlot;

        if (SskEngine < AesHwEngine_Num)
        {
            // Get the AES H/W context
            pAesHwCtxt = &pAes->pAesCoreEngine->AesHwCtxt;

            // Setup the SSK with Zero IV
            NvOsMemset(&Iv, 0, NvDdkAesConst_IVLengthBytes);

            AesCoreRequestHwAccess();

            // Setup SSK Key table for encryption
            gs_EngineCaps[SskEngine]->pAesInterf->AesHwSetKeyAndIv(
                pAesHwCtxt,
                SskEngine,
                EncryptSlot,
                (AesHwKey*)pSecureStorageKey,
                (AesKeySize)NvDdkAesKeySize_128Bit,
                &Iv,
                NV_TRUE);

            // Setup SSK Key table for decryption
            gs_EngineCaps[SskEngine]->pAesInterf->AesHwSetKeyAndIv(
                pAesHwCtxt,
                SskEngine,
                DecryptSlot,
                (AesHwKey*)pSecureStorageKey,
                (AesKeySize)NvDdkAesKeySize_128Bit,
                &Iv,
                NV_FALSE);

            // Disable the read / write access
            gs_EngineCaps[SskEngine]->pAesInterf->AesHwLockSskReadWrites(pAesHwCtxt, SskEngine);

            AesCoreReleaseHwAccess();

            if (!AesCoreSskLockCheck(pAesHwCtxt, SskEngine, EncryptSlot, DecryptSlot))
            {
                e = NvError_AesLockSskFailed;
                goto fail;
            }

            // Store the SSK in the Secure scratch and lock
            NV_CHECK_ERROR_CLEANUP(AesCoreLoadSskToSecureScratchAndLock(pAesHwCtxt->hRmDevice, pSecureStorageKey));
        }
    }

fail:
    NvOsMutexUnlock(s_AesCoreEngineMutex);
    return e;
}

NvError NvDdkAesDisableCrypto(NvDdkAesHandle hAes)
{
    NvError e = NvSuccess;
    NvDdkAes *pAes = (NvDdkAes*)hAes;
    AesHwContext *pAesHwCtxt = NULL;
    AesHwEngine Engine;

    if (!hAes)
        return NvError_BadParameter;

    // Get the AES H/W context
    pAesHwCtxt = &pAes->pAesCoreEngine->AesHwCtxt;

    NvOsMutexLock(s_AesCoreEngineMutex);

    AesCoreRequestHwAccess();
    for (Engine = AesHwEngine_A; Engine < AesHwEngine_Num; Engine++)
    {
        if (!gs_EngineCaps[Engine]->pAesInterf->AesHwDisableEngine(pAesHwCtxt, Engine))
            e = NvError_AesDisableCryptoFailed;
    }
    AesCoreReleaseHwAccess();

    if (NvSuccess == e)
    {
        // Mark engine as disabled
        s_pAesCoreEngine->IsEngineDisabled = NV_TRUE;
    }

    NvOsMutexUnlock(s_AesCoreEngineMutex);
    return e;
}

NvError NvDdkAesGetCapabilities(NvDdkAesHandle hAes, NvDdkAesCapabilities *pCapabilities)
{
    NvDdkAes *pAes = (NvDdkAes*)hAes;

    NVDDK_AES_CHECK_INPUT_PARAMS(pCapabilities);

    pCapabilities->OptimalBufferAlignment = 1;

    return NvSuccess;
}


////////////////////////////////////////////////////////////////////////
// AES engines are part of BSEA and BSEV (H/W components). These are
// used by the video/audio decoders. In order to prevent simultaneous access
// to these H/W components, H/W semaphores are used for mutual exclusion.
// The sequence of mutex acquisition in Video Decoders is, first VDE/BSEV
// and then BSEA. Same order is followed in order to avoid dead locks.

/**
 * Request access to HW.
 */
void AesCoreRequestHwAccess(void)
{
    NvRmXpcModuleAcquire(NvRmModuleID_Vde);
    NvRmXpcModuleAcquire(NvRmModuleID_BseA);
}

/**
 * Release access to HW.
 */
void AesCoreReleaseHwAccess(void)
{
    NvRmXpcModuleRelease(NvRmModuleID_BseA);
    NvRmXpcModuleRelease(NvRmModuleID_Vde);
}
/////////////////////////////////////////////////////////////////////////

static NvU32 AesCoreGetChipId(void)
{
    NvU32 ChipId = 0;
    NvRmPhysAddr MiscBase;
    NvRmDeviceHandle hRm = NULL;
    NvU32        MiscSize;
    NvU8        *pMisc;
    NvU32        reg;
    NvError      e = NvSuccess;

    NV_CHECK_ERROR_CLEANUP(NvRmOpen(&hRm, 0));
    NvRmModuleGetBaseAddress(hRm,
        NVRM_MODULE_ID(NvRmModuleID_Misc, 0), &MiscBase, &MiscSize);

    NV_CHECK_ERROR_CLEANUP(
        NvRmPhysicalMemMap(MiscBase, MiscSize, NVOS_MEM_READ_WRITE,
            NvOsMemAttribute_Uncached, (void**)&pMisc)
    );

    reg = NV_READ32(pMisc + APB_MISC_GP_HIDREV_0);
    ChipId = NV_DRF_VAL(APB_MISC_GP, HIDREV, CHIPID, reg);
    NvRmClose(hRm);

fail:
    if (e)
        NvOsDebugPrintf("failed to read the ChipId\n");

    return ChipId;
}

/**
 * Allocate the RM memory for key table and dma buffers
 *
 * @param pAesHwCtxt pointer to the Aes Hw engine context.
 *
 * @retval NvError_Success if memory is allocated successfully.
 * @retval NvError_InsufficientMemory if memory is not allocated successfully.
 */
NvError AesCoreAllocateRmMemory(AesHwContext *pAesHwCtxt)
{
    NvError e;
    static const NvRmHeap s_Heaps[] = {NvRmHeap_IRam};
    NvU8 * pKeyTabVirtAddr = NULL;
    NvRmPhysAddr KeyTabPhyAddr = 0;
    NvU8 * pDmaVirtAddr = NULL;
    NvRmPhysAddr DmaPhyAddr = 0;
    NvU32 ChipId = 0;
    NvU32 AesKeyTableOffset = 0;
    NvU32 size = 0;
    AesHwEngine Engine;

    ChipId = AesCoreGetChipId();
    if (ChipId == 0x30)
        AesKeyTableOffset = NV3P_AES_KEYTABLE_OFFSET_T30;
    else if (ChipId == 0x35)
        AesKeyTableOffset = NV3P_AES_KEYTABLE_OFFSET_T1XX;
    else
    {
        e = NvError_NotSupported;
        goto fail;
    }

    NV_ASSERT(pAesHwCtxt);

    size = (((g_AesHwKeySlotLengthBytes + NvDdkAesKeySize_256Bit) *
        AesHwEngine_Num) + AesKeyTableOffset);
    NvOsMutexLock(s_AesCoreEngineMutex);

    NV_CHECK_ERROR(NvRmMemHandleAlloc(pAesHwCtxt->hRmDevice, s_Heaps,
        NV_ARRAY_SIZE(s_Heaps), AES_HW_KEY_TABLE_ADDR_ALIGNMENT,
        NvOsMemAttribute_Uncached, size, 0, 1, &pAesHwCtxt->hKeyTableMemBuf));

    if (!pAesHwCtxt->hKeyTableMemBuf)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    KeyTabPhyAddr = NvRmMemPin(pAesHwCtxt->hKeyTableMemBuf) + AesKeyTableOffset;

    NV_CHECK_ERROR_CLEANUP(NvRmPhysicalMemMap(
        KeyTabPhyAddr,
        (size - AesKeyTableOffset),
        NVOS_MEM_READ_WRITE,
        NvOsMemAttribute_Uncached,
        (void **)&pKeyTabVirtAddr));

    for (Engine = AesHwEngine_A; Engine < AesHwEngine_Num; Engine++)
    {
        pAesHwCtxt->KeyTablePhyAddr[Engine] = (KeyTabPhyAddr +
            (Engine * (g_AesHwKeySlotLengthBytes + NvDdkAesKeySize_256Bit)));
        pAesHwCtxt->pKeyTableVirAddr[Engine] = (pKeyTabVirtAddr +
            (Engine * (g_AesHwKeySlotLengthBytes + NvDdkAesKeySize_256Bit)));
    }

    // Allocate DMA buffer for both the engines
    size = AES_HW_DMA_BUFFER_SIZE_BYTES * AesHwEngine_Num;

    NV_CHECK_ERROR_CLEANUP(NvRmMemHandleAlloc(pAesHwCtxt->hRmDevice, NULL, 0,
        AES_HW_DMA_ADDR_ALIGNMENT, NvOsMemAttribute_WriteCombined,
        size, 0, 1, &pAesHwCtxt->hDmaMemBuf));

    DmaPhyAddr = NvRmMemPin(pAesHwCtxt->hDmaMemBuf);

    NV_CHECK_ERROR_CLEANUP(NvRmMemMap(pAesHwCtxt->hDmaMemBuf, 0, size, NVOS_MEM_READ_WRITE, (void **)&pDmaVirtAddr));

    for (Engine = AesHwEngine_A; Engine < AesHwEngine_Num; Engine++)
    {
        pAesHwCtxt->DmaPhyAddr[Engine] = DmaPhyAddr + (Engine * AES_HW_DMA_BUFFER_SIZE_BYTES);
        pAesHwCtxt->pDmaVirAddr[Engine] = pDmaVirtAddr + (Engine * AES_HW_DMA_BUFFER_SIZE_BYTES);
    }

    NvOsMutexUnlock(s_AesCoreEngineMutex);
    return NvSuccess;

fail:
    AesCoreDeAllocateRmMemory(pAesHwCtxt);
    NvOsMutexUnlock(s_AesCoreEngineMutex);
    return e;
}

/**
 * De-allocate the RM memory allocated with .AesAllocateRmMemory()
 *
 * @param pAesHwCtxt pointer to the Aes Hw engine context.
 *
 * @retval None.
 */
void AesCoreDeAllocateRmMemory(AesHwContext *pAesHwCtxt)
{
    NvOsMutexLock(s_AesCoreEngineMutex);
    if (pAesHwCtxt->hKeyTableMemBuf)
    {
        NvOsPhysicalMemUnmap(
            pAesHwCtxt->pKeyTableVirAddr[0],
            ((g_AesHwKeySlotLengthBytes + NvDdkAesKeySize_256Bit) * AesHwEngine_Num));
        NvRmMemUnpin(pAesHwCtxt->hKeyTableMemBuf);
        NvRmMemHandleFree(pAesHwCtxt->hKeyTableMemBuf);
        pAesHwCtxt->hKeyTableMemBuf = NULL;
    }

    if (pAesHwCtxt->hDmaMemBuf)
    {
        NvRmMemUnmap(
            pAesHwCtxt->hDmaMemBuf,
            pAesHwCtxt->pDmaVirAddr[0],
            AES_HW_DMA_BUFFER_SIZE_BYTES * AesHwEngine_Num);
        NvRmMemUnpin(pAesHwCtxt->hDmaMemBuf);
        NvRmMemHandleFree(pAesHwCtxt->hDmaMemBuf);
        pAesHwCtxt->hDmaMemBuf = NULL;
    }
    NvOsMutexUnlock(s_AesCoreEngineMutex);
}

/**
 * Sets the key specified by the client in the AES engine
 *
 * @param pAes Aes client handle.
 * @param KeyType Key type
 * @param pKeyData pointer to the key data
 *
 * @retval NvSuccess if successfully completed.
 * @retval NvError_NotSupported if operation is not supported
 */
NvError
AesCoreSetSbkKey(
    NvDdkAes *pAes,
    const NvDdkAesKeyType KeyType,
    const NvBool IsDedicatedSlot,
    const NvU8 *pKeyData)
{
    NvError e = NvSuccess;
    AesCoreEngine *pAesCoreEngine = NULL;
    AesHwContext *pAesHwCtxt = NULL;

    NV_ASSERT(pAes);
    NV_ASSERT(pAes->pAesCoreEngine);

    pAesCoreEngine = pAes->pAesCoreEngine;
    pAesHwCtxt = &pAes->pAesCoreEngine->AesHwCtxt;

    NvOsMutexLock(s_AesCoreEngineMutex);

    pAes->Engine = pAesCoreEngine->SbkEngine[0];
    pAes->KeySlot = pAes->IsEncryption ? pAesCoreEngine->SbkEncryptSlot : pAesCoreEngine->SbkDecryptSlot;
    NvOsMemset(pAes->Iv, 0, NvDdkAesConst_IVLengthBytes);
    AesCoreRequestHwAccess();
    // Setup Key table
    gs_EngineCaps[pAes->Engine]->pAesInterf->AesHwSetKeyAndIv(
        pAesHwCtxt,
        pAes->Engine,
        pAes->KeySlot,
        (AesHwKey *)pKeyData,
        (AesKeySize)NvDdkAesKeySize_128Bit,
        (AesHwIv *)pAes->Iv,
        pAes->IsEncryption);

    // Select Key slot
    gs_EngineCaps[pAes->Engine]->pAesInterf->AesHwSelectKeyIvSlot(pAesHwCtxt,
                                                                  pAes->Engine,
                                                                  pAes->KeySlot,
                                                                  NvDdkAesKeySize_128Bit);

    // Get the IV and store it with client
    gs_EngineCaps[pAes->Engine]->pAesInterf->AesHwGetIv(pAesHwCtxt,
                                                        pAes->Engine,
                                                        (AesHwIv *)pAes->Iv,
                                                        pAes->KeySlot);

    AesCoreReleaseHwAccess();

    // Store the Key Type for this client
    pAes->KeyType = KeyType;
    NvOsMutexUnlock(s_AesCoreEngineMutex);
    return e;
}

/**
 * Select the key specified by the client in the AES engine
 *
 * @param pAes Aes client handle.
 * @param KeyType Key type
 * @param pKeyData pointer to the key data
 *
 * @retval NvSuccess if successfully completed.
 * @retval NvError_NotSupported if operation is not supported
 */
NvError
AesCoreSetKey(
    NvDdkAes *pAes,
    const NvDdkAesKeyType KeyType,
    const NvBool IsDedicatedSlot,
    const NvU8 *pKeyData,
    const NvDdkAesKeySize KeySize)
{
    NvError e = NvSuccess;
    AesCoreEngine *pAesCoreEngine = NULL;
    AesHwContext *pAesHwCtxt = NULL;
#if AES_ENABLE_USER_KEY
    AesHwEngine Engine;
    AesHwKeySlot KeySlot;
#endif

    NV_ASSERT(pAes);
    NV_ASSERT(pAes->pAesCoreEngine);

    pAesCoreEngine = pAes->pAesCoreEngine;
    pAesHwCtxt = &pAes->pAesCoreEngine->AesHwCtxt;

    pAes->IsDedicatedSlot = NV_FALSE;

    NvOsMutexLock(s_AesCoreEngineMutex);

    switch (KeyType)
    {
        case NvDdkAesKeyType_SecureBootKey:
            pAes->Engine = pAesCoreEngine->SbkEngine[0];
            pAes->KeySlot = pAes->IsEncryption ? pAesCoreEngine->SbkEncryptSlot : pAesCoreEngine->SbkDecryptSlot;
            pAes->KeySize = KeySize;
            break;
        case NvDdkAesKeyType_SecureStorageKey:
            pAes->Engine = pAesCoreEngine->SskEngine[0];
            pAes->KeySlot = pAes->IsEncryption ? pAesCoreEngine->SskEncryptSlot : pAesCoreEngine->SskDecryptSlot;
            pAes->KeySize = KeySize;
            break;
        case NvDdkAesKeyType_EncryptedKey:
            pAes->IsDedicatedSlot = IsDedicatedSlot;
            if (!IsDedicatedSlot)
            {
                NvOsMemcpy(pAes->Key, pKeyData, sizeof(AesHwKey));
                goto done;
            }
            // It is dedicated slot
            NV_CHECK_ERROR_CLEANUP(
                AesCoreGetFreeSlot(pAesCoreEngine, &Engine, &KeySlot));
            pAes->Engine = Engine;
            pAes->KeySlot = KeySlot;
            pAes->KeySize = KeySize;

            AesCoreRequestHwAccess();
            // Select Key slot
            gs_EngineCaps[pAes->Engine]->pAesInterf->AesHwSelectKeyIvSlot(
                pAesHwCtxt,
                pAesCoreEngine->SskEngine[0],
                pAesCoreEngine->SskDecryptSlot,
                pAes->KeySize);

            e = gs_EngineCaps[pAes->Engine]->pAesInterf->AesHwDecryptToSlot(
                pAesHwCtxt,
                pAesCoreEngine->SskEngine[0],
                NvDdkAesKeySize_128Bit,
                pKeyData,
                pAes->KeySlot);
            AesCoreReleaseHwAccess();
            if (e != NvSuccess)
            {
                goto fail;
            }
            pAes->pAesCoreEngine->IsKeySlotUsed[Engine][KeySlot] = NV_TRUE;
            goto done;
        case NvDdkAesKeyType_UserSpecified:
#if AES_ENABLE_USER_KEY
            pAes->IsDedicatedSlot = IsDedicatedSlot;
            if (!IsDedicatedSlot)
            {
                // It is not dedicated slot. Obfuscate the key.
                // Wrap the key using RFC3394 key wrapping algoritham.
                // The wrapped key and RFCwrapped IV will be stored in client handle.
                AesCoreRequestHwAccess();
                e = AesCoreWrapKey(pAes, pKeyData, KeySize, s_OriginalIV, pAes->Key,pAes->WrappedIv);
                AesCoreReleaseHwAccess();
                if (e != NvSuccess)
                    goto fail;
                pAes->KeySize = KeySize;
                goto done;
            }
            // It is dedicated slot
            NV_CHECK_ERROR_CLEANUP(AesCoreGetFreeSlot(pAesCoreEngine, &Engine, &KeySlot));
            pAes->pAesCoreEngine->IsKeySlotUsed[Engine][KeySlot] = NV_TRUE;
            pAes->Engine = Engine;
            pAes->KeySlot = KeySlot;
            // initialize IV to zeros
            NvOsMemset(pAes->Iv, 0, NvDdkAesConst_IVLengthBytes);
            AesCoreRequestHwAccess();
            // Setup Key table
            gs_EngineCaps[pAes->Engine]->pAesInterf->AesHwSetKeyAndIv(
                pAesHwCtxt,
                pAes->Engine,
                pAes->KeySlot,
                (AesHwKey *)pKeyData,
                (AesKeySize)pAes->KeySize,
                (AesHwIv *)pAes->Iv,
                pAes->IsEncryption);
            AesCoreReleaseHwAccess();
            break;
#endif
        default:
            goto fail;
    }

    AesCoreRequestHwAccess();

    // Select Key slot
    gs_EngineCaps[pAes->Engine]->pAesInterf->AesHwSelectKeyIvSlot(pAesHwCtxt,
                                                                  pAes->Engine,
                                                                  pAes->KeySlot,
                                                                  pAes->KeySize);

    // Get the IV and store it with client
    gs_EngineCaps[pAes->Engine]->pAesInterf->AesHwGetIv(pAesHwCtxt,
                                                        pAes->Engine,
                                                        (AesHwIv *)pAes->Iv,
                                                        pAes->KeySlot);

    AesCoreReleaseHwAccess();

done:
    // Store the Key Type for this client
    pAes->KeyType = KeyType;

fail:
    NvOsMutexUnlock(s_AesCoreEngineMutex);
    return e;
}

/**
 * Load the SSK into the secure scratch and disables the write permissions
 * Note: If Key is not specified then this API locks the Secure Scratch registers.
 *
 * @param hRmDevice RM device handle.
 * @param pKey pointer to the key. If pKey=NULL then key will not be set to the
 * secure scratch registers, but locks the Secure scratch register.
 *
 * @retval NvSuccess if successfully completed.
 */
static NvError AesCoreLoadSskToSecureScratchAndLock(NvRmDeviceHandle hRmDevice, const NvU8 *pKey)
{
    NvError e = NvSuccess;
    NvRmPhysAddr PhysAdr;
    NvU32 BankSize;
    NvU32 RmPwrClientId = 0;

    NV_ASSERT(hRmDevice);

    // Get the secure scratch base address
    NvRmModuleGetBaseAddress(hRmDevice, NVRM_MODULE_ID(NvRmModuleID_Pmif, 0), &PhysAdr, &BankSize);

    // Register with Power Manager
    RmPwrClientId = NVRM_POWER_CLIENT_TAG('A','E','S','2');

    NV_CHECK_ERROR_CLEANUP(NvRmPowerRegister(hRmDevice, NULL, &RmPwrClientId));

    // Enable the Voltage
    NV_CHECK_ERROR_CLEANUP(NvRmPowerVoltageControl(
        hRmDevice,
        NvRmModuleID_Pmif,
        RmPwrClientId,
        NvRmVoltsUnspecified,
        NvRmVoltsUnspecified,
        NULL,
        0,
        NULL));

    // Emable the clock to the PMIC
    NV_CHECK_ERROR_CLEANUP(NvRmPowerModuleClockControl(hRmDevice, NvRmModuleID_Pmif, RmPwrClientId, NV_TRUE));

#if STORE_SSK_IN_PMC
    // Store SSK and lock the secure scratch -- engine doesn't matter here
    // since the key is being providedand not really read from the key table of an engine here
    // If pKey == NULL this call will disable the write permissions to the scratch registers.
    AesCoreRequestHwAccess();
    gs_EngineCaps[0]->pAesInterf->AesHwLoadSskToSecureScratchAndLock(PhysAdr,
                                                                     (AesHwKey *)pKey,
                                                                     BankSize);
    AesCoreReleaseHwAccess();
#endif

fail:
    // Disable the clock to the PMIC
    NV_ASSERT_SUCCESS(NvRmPowerModuleClockControl(hRmDevice, NvRmModuleID_Pmif, RmPwrClientId, NV_FALSE));

    // Disable the Voltage
    NV_ASSERT_SUCCESS(NvRmPowerVoltageControl(
        hRmDevice,
        NvRmModuleID_Pmif,
        RmPwrClientId,
        NvRmVoltsOff,
        NvRmVoltsOff,
        NULL,
        0,
        NULL));

    // Unregister driver from Power Manager
    NvRmPowerUnRegister(hRmDevice, RmPwrClientId);
    return e;
}

/**
 * Check the SBK clear by encrypting / decrypting the known data.
 *
 * @param pAesHw Pointer to Aes H/W context.
 * @param Engine engine on which encryption and decryption need to be performed
 * @param EncryptSlot key slot where encrypt key is located.
 * @param DecryptSlot key slot where decrypt key is located.
 *
 * @retval NV_TRUE if successfully encryption and decryption is done else NV_FALSE
 */
NvBool
AesCoreSbkClearCheck(
    AesHwContext *pAesHw,
    AesHwEngine Engine,
    AesHwKeySlot EncryptSlot,
    AesHwKeySlot DecryptSlot)
{
    NvError e = NvSuccess;
    AesHwIv ZeroIv;
    NvU32 i;

    // Known Good data
    NvU8 GoldData[NvDdkAesConst_BlockLengthBytes] =
    {
        0x00, 0x01, 0x02, 0x03,
        0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F
    };

    // Encrypted data for above known data with Zero key and Zero IV
    NvU8 EncryptDataWithZeroKeyTable[NvDdkAesConst_BlockLengthBytes] =
    {
        0x7A, 0xCA, 0x0F, 0xD9,
        0xBC, 0xD6, 0xEC, 0x7C,
        0x9F, 0x97, 0x46, 0x66,
        0x16, 0xE6, 0xA2, 0x82
    };

    // Encrypted data for above known data with Zero key and Zero IV
    NvU8 EncryptDataWithZeroKeySchedule[NvDdkAesConst_BlockLengthBytes] =
    {
        0x18, 0x9D, 0x19, 0xEA,
        0xDB, 0xA7, 0xE3, 0x0E,
        0xD9, 0x72, 0x80, 0x8F,
        0x3F, 0x2B, 0xA0, 0x30
    };

    NvU8 *EncryptData;
    NvU8 EncryptBuffer[NvDdkAesConst_BlockLengthBytes];
    NvU8 DecryptBuffer[NvDdkAesConst_BlockLengthBytes];

    NvOsMutexLock(s_AesCoreEngineMutex);

    if (gs_EngineCaps[Engine]->IsHwKeySchedGenSupported)
    {
        EncryptData = EncryptDataWithZeroKeyTable;
    }
    else
    {
        EncryptData = EncryptDataWithZeroKeySchedule;
    }

    NvOsMemset(EncryptBuffer, 0, NvDdkAesConst_BlockLengthBytes);
    NvOsMemset(DecryptBuffer, 0, NvDdkAesConst_BlockLengthBytes);
    NvOsMemset(&ZeroIv, 0, sizeof(AesHwIv));

    AesCoreRequestHwAccess();

    // Select Encrypt Key slot
    gs_EngineCaps[Engine]->pAesInterf->AesHwSelectKeyIvSlot(pAesHw,
                                                            Engine,
                                                            EncryptSlot,
                                                            NvDdkAesKeySize_128Bit);

    // Set the Zero IV for test data
    gs_EngineCaps[Engine]->pAesInterf->AesHwSetIv(pAesHw, Engine, &ZeroIv, EncryptSlot);

    // Process the buffer for encryption
    NV_CHECK_ERROR_CLEANUP(gs_EngineCaps[Engine]->pAesInterf->AesHwStartEngine(
        pAesHw,
        Engine,
        NvDdkAesConst_BlockLengthBytes,
        GoldData,
        EncryptBuffer,
        NV_TRUE,
        NvDdkAesOperationalMode_Cbc));

    // Select Decrypt Key slot
    gs_EngineCaps[Engine]->pAesInterf->AesHwSelectKeyIvSlot(pAesHw,
                                                            Engine,
                                                            DecryptSlot,
                                                            NvDdkAesKeySize_128Bit);

    // Set the Zero IV for test data
    gs_EngineCaps[Engine]->pAesInterf->AesHwSetIv(pAesHw, Engine, &ZeroIv, DecryptSlot);

    // Process the buffer for encryption
    NV_CHECK_ERROR_CLEANUP(gs_EngineCaps[Engine]->pAesInterf->AesHwStartEngine(
        pAesHw,
        Engine,
        NvDdkAesConst_BlockLengthBytes,
        EncryptBuffer,
        DecryptBuffer,
        NV_FALSE,
        NvDdkAesOperationalMode_Cbc));

    // Clear the IV in the engine before we leave
    gs_EngineCaps[Engine]->pAesInterf->AesHwClearIv(pAesHw, Engine, EncryptSlot);
    gs_EngineCaps[Engine]->pAesInterf->AesHwClearIv(pAesHw, Engine, DecryptSlot);

    // Clear the DMA buffer before we leave from this operation.
    NV_ASSERT(pAesHw->pDmaVirAddr[Engine]);
    NvOsMemset(pAesHw->pDmaVirAddr[Engine], 0, AES_HW_DMA_BUFFER_SIZE_BYTES);

    for (i = 0; i < NvDdkAesConst_BlockLengthBytes; i++)
    {
        if ((EncryptData[i] != EncryptBuffer[i]) || (GoldData[i] != DecryptBuffer[i]) )
            goto fail;
    }

    AesCoreReleaseHwAccess();
    NvOsMutexUnlock(s_AesCoreEngineMutex);
    return NV_TRUE;

fail:
    // Clear the DMA buffer before we leave from this operation
    NV_ASSERT(pAesHw->pDmaVirAddr[Engine]);
    NvOsMemset(pAesHw->pDmaVirAddr[Engine], 0, AES_HW_DMA_BUFFER_SIZE_BYTES);
    AesCoreReleaseHwAccess();
    NvOsMutexUnlock(s_AesCoreEngineMutex);
    return NV_FALSE;
}

/**
 * Check the SSK lock is successfully done or not by writing zero key into SSK.
 *
 * @param pAesHw Pointer to Aes H/W context.
 * @param Engine engine where SSK is set
 * @param EncryptSlot key slot where encrypt key is located.
 * @param DecryptSlot key slot where decrypt key is located.
 *
 * @retval NV_TRUE if successfully SSK is locked else NV_FALSE
 */
NvBool
AesCoreSskLockCheck(
    AesHwContext *pAesHw,
    AesHwEngine Engine,
    AesHwKeySlot EncryptSlot,
    AesHwKeySlot DecryptSlot)
{
    NvError e = NvSuccess;
    AesHwIv ZeroIv;
    AesHwKey ZeroKey;
    NvU32 i;

    NvU8 GoldData[NvDdkAesConst_BlockLengthBytes] =
    {
        0x00, 0x11, 0x22, 0x33,
        0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB,
        0xCC, 0xDD, 0xEE, 0xFF
    };

    NvU8 EncryptBuffer1[NvDdkAesConst_BlockLengthBytes];
    NvU8 DecryptBuffer1[NvDdkAesConst_BlockLengthBytes];
    NvU8 EncryptBuffer2[NvDdkAesConst_BlockLengthBytes];
    NvU8 DecryptBuffer2[NvDdkAesConst_BlockLengthBytes];

    NvOsMemset(EncryptBuffer1, 0, NvDdkAesConst_BlockLengthBytes);
    NvOsMemset(DecryptBuffer1, 0, NvDdkAesConst_BlockLengthBytes);
    NvOsMemset(EncryptBuffer2, 0, NvDdkAesConst_BlockLengthBytes);
    NvOsMemset(DecryptBuffer2, 0, NvDdkAesConst_BlockLengthBytes);
    NvOsMemset(&ZeroIv, 0, sizeof(AesHwIv));
    NvOsMemset(&ZeroKey, 0, sizeof(AesHwKey));

    NvOsMutexLock(s_AesCoreEngineMutex);

    AesCoreRequestHwAccess();

    // Select Encrypt Key slot
    gs_EngineCaps[Engine]->pAesInterf->AesHwSelectKeyIvSlot(pAesHw,
                                                            Engine,
                                                            EncryptSlot,
                                                            NvDdkAesKeySize_128Bit);

    // Set the Zero IV for test data
    gs_EngineCaps[Engine]->pAesInterf->AesHwSetIv(pAesHw, Engine, &ZeroIv, EncryptSlot);

    // Process the buffer for encryption
    NV_CHECK_ERROR_CLEANUP(gs_EngineCaps[Engine]->pAesInterf->AesHwStartEngine(
        pAesHw,
        Engine,
        NvDdkAesConst_BlockLengthBytes,
        GoldData,
        EncryptBuffer1,
        NV_TRUE,
        NvDdkAesOperationalMode_Cbc));

    // Select Decrypt Key slot
    gs_EngineCaps[Engine]->pAesInterf->AesHwSelectKeyIvSlot(pAesHw,
                                                            Engine,
                                                            DecryptSlot,
                                                            NvDdkAesKeySize_128Bit);

    // Set the Zero IV for test data
    gs_EngineCaps[Engine]->pAesInterf->AesHwSetIv(pAesHw, Engine, &ZeroIv, DecryptSlot);

    // Process the buffer for encryption
    NV_CHECK_ERROR_CLEANUP(gs_EngineCaps[Engine]->pAesInterf->AesHwStartEngine(
        pAesHw,
        Engine,
        NvDdkAesConst_BlockLengthBytes,
        GoldData,
        DecryptBuffer1,
        NV_FALSE,
        NvDdkAesOperationalMode_Cbc));

    // Set Zero key to the SSK slot and try encryption / decryption with
    // Known data and check data after encryption and decryption are same with SSK

    // Setup SSK Key table for encryption
    gs_EngineCaps[Engine]->pAesInterf->AesHwSetKeyAndIv(
        pAesHw,
        Engine,
        EncryptSlot,
        &ZeroKey,
        (AesKeySize)NvDdkAesKeySize_128Bit,
        &ZeroIv,
        NV_TRUE);

    // Setup SSK Key table for encryption
    gs_EngineCaps[Engine]->pAesInterf->AesHwSetKeyAndIv(
        pAesHw,
        Engine,
        DecryptSlot,
        &ZeroKey,
        (AesKeySize)NvDdkAesKeySize_128Bit,
        &ZeroIv,
        NV_FALSE);

    // Select Encrypt Key slot
    gs_EngineCaps[Engine]->pAesInterf->AesHwSelectKeyIvSlot(pAesHw,
                                                            Engine,
                                                            EncryptSlot,
                                                            NvDdkAesKeySize_128Bit);

    // Set the Zero IV for test data
    gs_EngineCaps[Engine]->pAesInterf->AesHwSetIv(pAesHw, Engine, &ZeroIv, EncryptSlot);

    // Process the buffer for encryption
    NV_CHECK_ERROR_CLEANUP(gs_EngineCaps[Engine]->pAesInterf->AesHwStartEngine(
        pAesHw,
        Engine,
        NvDdkAesConst_BlockLengthBytes,
        GoldData,
        EncryptBuffer2,
        NV_TRUE,
        NvDdkAesOperationalMode_Cbc));

    // Select Decrypt Key slot
    gs_EngineCaps[Engine]->pAesInterf->AesHwSelectKeyIvSlot(pAesHw,
                                                            Engine,
                                                            DecryptSlot,
                                                            NvDdkAesKeySize_128Bit);

    // Set the Zero IV for test data
   gs_EngineCaps[Engine]->pAesInterf->AesHwSetIv(pAesHw, Engine, &ZeroIv, DecryptSlot);

    // Process the buffer for encryption
    NV_CHECK_ERROR_CLEANUP(gs_EngineCaps[Engine]->pAesInterf->AesHwStartEngine(
        pAesHw,
        Engine,
        NvDdkAesConst_BlockLengthBytes,
        GoldData,
        DecryptBuffer2,
        NV_FALSE,
        NvDdkAesOperationalMode_Cbc));

    // Clear the IV in the engine before we leave
    gs_EngineCaps[Engine]->pAesInterf->AesHwClearIv(pAesHw, Engine, EncryptSlot);
    gs_EngineCaps[Engine]->pAesInterf->AesHwClearIv(pAesHw, Engine, DecryptSlot);

    // Clear the DMA buffer before we leave from this operation.
    NV_ASSERT(pAesHw->pDmaVirAddr[Engine]);
    NvOsMemset(pAesHw->pDmaVirAddr[Engine], 0, AES_HW_DMA_BUFFER_SIZE_BYTES);

    // Check encrypt and decrypt output is same before and after zero key set to SSK
    // If both encrypt and decrypt data match then SSK lock is OK
    for (i = 0; i < NvDdkAesConst_BlockLengthBytes; i++)
    {
        if ((EncryptBuffer1[i] != EncryptBuffer2[i]) || (DecryptBuffer1[i] != DecryptBuffer2[i]) )
            goto fail;
    }
    AesCoreReleaseHwAccess();
    NvOsMutexUnlock(s_AesCoreEngineMutex);
    return NV_TRUE;

fail:
    // Clear the DMA buffer before we leave from this operation.
    NV_ASSERT(pAesHw->pDmaVirAddr[Engine]);
    NvOsMemset(pAesHw->pDmaVirAddr[Engine], 0, AES_HW_DMA_BUFFER_SIZE_BYTES);
    AesCoreReleaseHwAccess();
    NvOsMutexUnlock(s_AesCoreEngineMutex);
    return NV_FALSE;
}

/**
 * Add the Busy hints to boost or reduce the CPU, System and EMC frequencies.
 *
 * @param hRmDevice RM device handle.
 * @param PowerClientId The client ID obtained during Power registration.
 * @param DfsOn Indicator to boost the frequency, If set to NV_TRUE. Cancles the
 * DFS busy hint if set to NV_FALSE
 *
 * @retval NvSuccess if busy hint request completed successfully.
 * @retval NvError_NotSupported if DFS is disabled
 * @retval NvError_BadValue if specified client ID is not registered.
 * @retval NvError_InsufficientMemory if failed to allocate memory for
 *  busy hints.
 */
NvError
AesCoreDfsBusyHint(NvRmDeviceHandle hRm, NvU32 PowerClientId, NvBool DfsOn)
{
    #define AES_EMC_BOOST_FREQ_KHZ 100000
    #define AES_SYS_BOOST_FREQ_KHZ 100000

    NvRmDfsBusyHint AesBusyHintOn[] =
    {
        {NvRmDfsClockId_Emc, NV_WAIT_INFINITE, AES_EMC_BOOST_FREQ_KHZ, NV_TRUE},
        {NvRmDfsClockId_System, NV_WAIT_INFINITE, AES_SYS_BOOST_FREQ_KHZ, NV_TRUE}
    };

    NvRmDfsBusyHint AesBusyHintOff[] =
    {
        {NvRmDfsClockId_Emc, 0, 0, NV_TRUE},
        {NvRmDfsClockId_System, 0, 0, NV_TRUE}
    };

    NV_ASSERT(hRm);

    if (DfsOn)
    {
        return NvRmPowerBusyHintMulti(
            hRm,
            PowerClientId,
            AesBusyHintOn,
            NV_ARRAY_SIZE(AesBusyHintOn),
            NvRmDfsBusyHintSyncMode_Async);
    }
    else
    {
        return NvRmPowerBusyHintMulti(
            hRm,
            PowerClientId,
            AesBusyHintOff,
            NV_ARRAY_SIZE(AesBusyHintOff),
            NvRmDfsBusyHintSyncMode_Async);
    }
}

/**
 * Populate the structure for Aes context with the engine base address
 *
 * @param hRmDevice Rm device handle
 * @param pAesHwCtxt Pointer to Aes H/W context.
 *
 * @retval NvSuccess if successfully completed.
 *
 */
NvError AesCoreInitializeEngineSpace(NvRmDeviceHandle hRmDevice, AesHwContext *pAesHwCtxt)
{
    NvError e = NvSuccess;
    AesHwEngine Engine;

    NV_ASSERT(hRmDevice);
    NV_ASSERT(pAesHwCtxt);

    NvOsMutexLock(s_AesCoreEngineMutex);

    for (Engine = AesHwEngine_A; Engine < AesHwEngine_Num; Engine++)
    {
        switch(Engine)
        {
            case AesHwEngine_A:
                // Get the controller base address
                NvRmModuleGetBaseAddress(
                    hRmDevice,
                    NVRM_MODULE_ID(NvRmModuleID_Vde, 0),
                    &pAesHwCtxt->PhysAdr[Engine],
                    &pAesHwCtxt->BankSize[Engine]);
                break;
            case AesHwEngine_B:
                // Get the controller base address
                NvRmModuleGetBaseAddress(
                    hRmDevice,
                    NVRM_MODULE_ID(NvRmModuleID_BseA, 0),
                    &pAesHwCtxt->PhysAdr[Engine],
                    &pAesHwCtxt->BankSize[Engine]);
                break;
            default:
                break;
            }

            // Map the physycal memory to virtual memory
            NV_CHECK_ERROR_CLEANUP(NvRmPhysicalMemMap(
                pAesHwCtxt->PhysAdr[Engine],
                pAesHwCtxt->BankSize[Engine],
                NVOS_MEM_READ_WRITE,
                NvOsMemAttribute_Uncached,
                (void **)&pAesHwCtxt->pVirAdr[Engine]));
    }
    NvOsMutexUnlock(s_AesCoreEngineMutex);
    return NvSuccess;
fail:
    NvOsMutexUnlock(s_AesCoreEngineMutex);
    return e;
}

/**
 * Unmap all engine space
 *
 * @param pAesHwCtxt Pointer to Aes H/W context.
 *
 * @retval None.
 *
 */
void
AesCoreDeInitializeEngineSpace(
    AesHwContext *pAesHwCtxt)
{
    AesHwEngine Engine;

    NvOsMutexLock(s_AesCoreEngineMutex);
    // Clean up resources
    for (Engine = AesHwEngine_A; Engine < AesHwEngine_Num; Engine++)
    {
        // UnMap the virtual Address
        NvOsPhysicalMemUnmap(pAesHwCtxt->pVirAdr[Engine], pAesHwCtxt->BankSize[Engine]);
    }
    NvOsMutexUnlock(s_AesCoreEngineMutex);
}

/**
 * Find the Un-Used Key Slot
 *
 * @param pAesCoreEngine Pointer to Aes Block dev handle.
 * @param pEngine pointer to the engine
 * @param pKeySlot pointer to the key slot
 *
 * @retval NvSuccess if successfully completed.
 * @retval NvError_AlreadyAllocated if all slots are allocated
 */
NvError AesCoreGetFreeSlot(AesCoreEngine *pAesCoreEngine, AesHwEngine *pEngine, AesHwKeySlot *pKeySlot)
{
    NvU32 Engine;
    NvU32 KeySlot;

    NV_ASSERT(pAesCoreEngine);
    NV_ASSERT(pEngine);
    NV_ASSERT(pKeySlot);

    NvOsMutexLock(s_AesCoreEngineMutex);

    // Get the free slot
    for (Engine = AesHwEngine_A; Engine < AesHwEngine_Num; Engine++)
    {
        for (KeySlot = AesHwKeySlot_0; KeySlot < (NvU32)(gs_EngineCaps[Engine]->NumSlotsSupported); KeySlot++)
        {
            if (!pAesCoreEngine->IsKeySlotUsed[Engine][KeySlot])
            {
                *pEngine = Engine;
                *pKeySlot = KeySlot;
                NvOsMutexUnlock(s_AesCoreEngineMutex);
                return NvSuccess;
            }
        }
    }

    NvOsMutexUnlock(s_AesCoreEngineMutex);

    return NvError_AlreadyAllocated;
}

#if !ENABLE_SECURITY
NvU32 NvAesCoreGetEngineAddress(NvU32 Engine)
{
    NvU32 Address = 0;

    if (s_pAesCoreEngine)
    {
        NvOsMutexLock(s_AesCoreEngineMutex);
        Address = (NvU32)s_pAesCoreEngine->AesHwCtxt.pVirAdr[Engine];
        NvOsMutexUnlock(s_AesCoreEngineMutex);
    }
    return Address;
}
#endif

/**
 * Init AES engine.
 *
 * @param hRmDevice Resource Manager Handle.
 */
NvError AesCoreInitEngine(NvRmDeviceHandle hRmDevice)
{
    NvError e = NvSuccess;
    AesHwContext *pAesHwCtxt = NULL;
    AesHwEngine Engine;

    NV_ASSERT(hRmDevice);

    s_pAesCoreEngine = NvOsAlloc(sizeof(AesCoreEngine));
    if (NULL == s_pAesCoreEngine)
        return NvError_InsufficientMemory;

    // Clear the memory initially
    NvOsMemset(s_pAesCoreEngine, 0, sizeof(AesCoreEngine));

    // Get the AES H/W context
    pAesHwCtxt = &s_pAesCoreEngine->AesHwCtxt;

    // Store the RM handle for future use
    pAesHwCtxt->hRmDevice = hRmDevice;

    // Check it if it's correctly allocating memory
    gs_EngineCaps = (AesHwCapabilities **)NvOsAlloc(sizeof(AesHwCapabilities) * AesHwEngine_Num);
    if (NULL == gs_EngineCaps)
    {
        NvOsFree(s_pAesCoreEngine);
        s_pAesCoreEngine = NULL;
        return NvError_InsufficientMemory;
    }
    NvOsMemset(gs_EngineCaps, 0, AesHwEngine_Num * sizeof(*gs_EngineCaps));
    AesCoreGetCapabilities(hRmDevice, gs_EngineCaps);

    NV_CHECK_ERROR(AesCoreInitializeEngineSpace(hRmDevice, pAesHwCtxt));

    // Allocate memories required for H/W operation
    NV_CHECK_ERROR(AesCoreAllocateRmMemory(pAesHwCtxt));

    // Create mutex to gaurd the H/W engine
    for (Engine = AesHwEngine_A; Engine < AesHwEngine_Num; Engine++)
    {
        NV_CHECK_ERROR(NvOsMutexCreate(&pAesHwCtxt->mutex[Engine]));

        // Register with Power Manager
        s_pAesCoreEngine->RmPwrClientId[Engine] = NVRM_POWER_CLIENT_TAG('A','E','S','1');

        NV_CHECK_ERROR(NvRmPowerRegister(hRmDevice, NULL, &s_pAesCoreEngine->RmPwrClientId[Engine]));

        // Enable the voltage
        NV_CHECK_ERROR(NvRmPowerVoltageControl(
            hRmDevice,
            (Engine == AesHwEngine_A) ?
            NvRmModuleID_Vde : NvRmModuleID_BseA,
            s_pAesCoreEngine->RmPwrClientId[Engine],
            NvRmVoltsUnspecified,
            NvRmVoltsUnspecified,
            NULL,
            0,
            NULL));

        // Enable clock to VDE
        NV_CHECK_ERROR(NvRmPowerModuleClockControl(
            hRmDevice,
            (Engine == AesHwEngine_A) ?
            NvRmModuleID_Vde : NvRmModuleID_BseA,
            s_pAesCoreEngine->RmPwrClientId[Engine],
            NV_TRUE));
    }

    // Request the H/W semaphore before accesing the AES H/W
    AesCoreRequestHwAccess();

    // Reset the BSEV and BSEA engines
    NvRmModuleReset(hRmDevice, NvRmModuleID_Vde);
    NvRmModuleReset(hRmDevice, NvRmModuleID_BseA);

    for (Engine = AesHwEngine_A; Engine < AesHwEngine_Num; Engine++)
    {
        s_pAesCoreEngine->IsEngineDisabled =
            gs_EngineCaps[Engine]->pAesInterf->AesHwIsEngineDisabled(pAesHwCtxt, Engine);
    }

    // If engine is not disabled then set the SBK, SSK.
    if (!s_pAesCoreEngine->IsEngineDisabled)
    {
        //The slots dedicated already don't depend on which engine is being used but
        //on the capabilities the engines can provide. Basic assumption: both engines have
        //same capabilities
        gs_EngineCaps[0]->pAesInterf->AesGetUsedSlots(s_pAesCoreEngine);

        // Disable read access to all key slots
        for (Engine = AesHwEngine_A; Engine < AesHwEngine_Num; Engine++)
        {
            gs_EngineCaps[Engine]->pAesInterf->AesHwDisableAllKeyRead(
                pAesHwCtxt,
                Engine,
                (gs_EngineCaps[Engine]->NumSlotsSupported));
        }
    }

    // Release the H/W semaphore
    AesCoreReleaseHwAccess();

    // Disable clocks after AES init
    for (Engine = AesHwEngine_A; Engine < AesHwEngine_Num; Engine++)
    {
        // Disable clock to VDE
        NV_CHECK_ERROR(NvRmPowerModuleClockControl(
            pAesHwCtxt->hRmDevice,
            (Engine == AesHwEngine_A) ?
            NvRmModuleID_Vde : NvRmModuleID_BseA,
            s_pAesCoreEngine->RmPwrClientId[Engine],
            NV_FALSE));

        // Disable the voltage
        NV_CHECK_ERROR(NvRmPowerVoltageControl(
            pAesHwCtxt->hRmDevice,
            (Engine == AesHwEngine_A) ?
            NvRmModuleID_Vde : NvRmModuleID_BseA,
            s_pAesCoreEngine->RmPwrClientId[Engine],
            NvRmVoltsOff,
            NvRmVoltsOff,
            NULL,
            0,
            NULL));
    }

    s_pAesCoreEngine->SskUpdateAllowed =
            gs_EngineCaps[AesHwEngine_A]->pAesInterf->AesHwIsSskUpdateAllowed(hRmDevice);

    return e;
}

/**
  * Free up resources.
  */
void AesCoreFreeUpEngine(void)
{
    AesHwEngine Engine;

    // Get the AES H/W context
    AesHwContext *pAesHwCtxt = &s_pAesCoreEngine->AesHwCtxt;

    AesCoreDeInitializeEngineSpace(pAesHwCtxt);

    // Deallocate the memory
    AesCoreDeAllocateRmMemory(pAesHwCtxt);

    // Destroy mutex
    for (Engine = AesHwEngine_A; Engine < AesHwEngine_Num; Engine++)
    {
        NvOsMutexDestroy(pAesHwCtxt->mutex[Engine]);
        // Unregister driver from Power Manager
        NvRmPowerUnRegister(pAesHwCtxt->hRmDevice, s_pAesCoreEngine->RmPwrClientId[Engine]);
        if (gs_EngineCaps[Engine]->pAesInterf)
        {
            NvOsFree(gs_EngineCaps[Engine]->pAesInterf);
            gs_EngineCaps[Engine]->pAesInterf = NULL;
        }
    }
    if (gs_EngineCaps)
    {
        NvOsFree(gs_EngineCaps);
        gs_EngineCaps = NULL;
    }
}

/**
 * Power up the AES core engine.
 *
 * @param pAesCoreEngine pointer to the AesCoreEngine argument.
 */
void AesCorePowerUp(AesCoreEngine *pAesCoreEngine)
{
    AesHwEngine Engine;
    AesHwContext *pAesHwCtxt = NULL;

    NV_ASSERT(pAesCoreEngine);

    // Get the Aes Hw context
    pAesHwCtxt = &pAesCoreEngine->AesHwCtxt;

    for (Engine = AesHwEngine_A; Engine < AesHwEngine_Num; Engine++)
    {
        // DFS busy hints On
        NV_ASSERT_SUCCESS(AesCoreDfsBusyHint(pAesHwCtxt->hRmDevice, pAesCoreEngine->RmPwrClientId[Engine], NV_TRUE));

        // Enable the voltage
        NV_ASSERT_SUCCESS(NvRmPowerVoltageControl(
            pAesHwCtxt->hRmDevice,
            (Engine == AesHwEngine_A) ? NvRmModuleID_Vde : NvRmModuleID_BseA,
            pAesCoreEngine->RmPwrClientId[Engine],
            NvRmVoltsUnspecified,
            NvRmVoltsUnspecified,
            NULL,
            0,
            NULL));

        // Enable clock to VDE
        NV_ASSERT_SUCCESS(NvRmPowerModuleClockControl(
            pAesHwCtxt->hRmDevice,
            (Engine == AesHwEngine_A) ? NvRmModuleID_Vde : NvRmModuleID_BseA,
            pAesCoreEngine->RmPwrClientId[Engine],
            NV_TRUE));
    }
}

/**
 * Power down the AES core engine.
 *
 * @param pAesCoreEngine pointer to the AesCoreEngine argument.
 */
void AesCorePowerDown(AesCoreEngine *pAesCoreEngine)
{
    AesHwEngine Engine;
    AesHwContext *pAesHwCtxt = NULL;

    NV_ASSERT(pAesCoreEngine);

    // Get the AES H/W context
    pAesHwCtxt = &pAesCoreEngine->AesHwCtxt;

    for (Engine = AesHwEngine_A; Engine < AesHwEngine_Num; Engine++)
    {
        // Disable clock to VDE
        NV_ASSERT_SUCCESS(NvRmPowerModuleClockControl(
            pAesHwCtxt->hRmDevice,
            (Engine == AesHwEngine_A) ? NvRmModuleID_Vde : NvRmModuleID_BseA,
            pAesCoreEngine->RmPwrClientId[Engine],
            NV_FALSE));

        // Disable the voltage
        NV_ASSERT_SUCCESS(NvRmPowerVoltageControl(
            pAesHwCtxt->hRmDevice,
            (Engine == AesHwEngine_A) ? NvRmModuleID_Vde : NvRmModuleID_BseA,
            pAesCoreEngine->RmPwrClientId[Engine],
            NvRmVoltsOff,
            NvRmVoltsOff,
            NULL,
            0,
            NULL));

        // DFS busy hints Off
        NV_ASSERT_SUCCESS(AesCoreDfsBusyHint(pAesHwCtxt->hRmDevice, pAesCoreEngine->RmPwrClientId[Engine], NV_FALSE));
    }
}

/**
 * Process the buffers for encryption or decryption depending on the IOCTL args.
 *
 * @param pAes Aes client handle.
 * @param SkipOffset Skip initial SkipOffset bytes of SrcBuffer before beginning cipher.
 * @param DestBufferSize Size of dest buffer in bytes.
 * @param pSrcBuffer Pointer to src buffer.
 * @param pDestBuffer Pointer to dest buffer.
 *
 * @retval NvSuccess if successfully completed.
 */
NvError
AesCoreProcessBuffer(
    NvDdkAes *pAes,
    NvU32 SkipOffset,
    NvU32 SrcBufferSize,
    NvU32 DestBufferSize,
    const NvU8 *pSrcBuffer,
    NvU8 *pDestBuffer)
{
    NvError e = NvSuccess;
    AesHwContext *pAesHwCtxt = NULL;
    AesHwEngine Engine;
    AesHwKeySlot KeySlot;
    NvU32 TotalBytesToProcess = 0;
    NvU32 BytesToProcess = 0;

    NV_ASSERT(pAes);
    NV_ASSERT(pSrcBuffer);
    NV_ASSERT(pDestBuffer);

    // Check type of operation supported for the process buffer
    switch(pAes->OpMode)
    {
        case NvDdkAesOperationalMode_Cbc:
        case NvDdkAesOperationalMode_Ecb:
        case NvDdkAesOperationalMode_AnsiX931:
            break;
        default:
            return NvError_InvalidState;
    }

    if (DestBufferSize % NvDdkAesConst_BlockLengthBytes)
        return NvError_InvalidSize;

    // Check, client has already assigned key for this process if not return
    if ((NvDdkAesKeyType_Invalid == pAes->KeyType) || (pAes->KeyType >= NvDdkAesKeyType_Num))
        return NvError_InvalidState;

    // Get the AES H/W context
    pAesHwCtxt = &pAes->pAesCoreEngine->AesHwCtxt;

#if !AES_ENABLE_USER_KEY
    if (NvDdkAesKeyType_UserSpecified == pAes->KeyType)
    {
        return NvError_NotSupported;
    }
#endif

    TotalBytesToProcess = DestBufferSize;

    NvOsMutexLock(s_AesCoreEngineMutex);

    while (TotalBytesToProcess)
    {
        // Request the H/W semaphore before accesing the AES H/W
        AesCoreRequestHwAccess();

        // In the bootloader version entire buffer is processed for AES operation
        // At OS level only AES_HW_MAX_PROCESS_SIZE_BYTES (which takes 1ms)
        // will be processed and saves the AES state to realease the H/W engine.
        // Once the AES H/W lock is acquired again then remaining bytes or maximum of
        // AES_HW_MAX_PROCESS_SIZE_BYTES will be processed.
        if (TotalBytesToProcess > AES_HW_MAX_PROCESS_SIZE_BYTES)
            BytesToProcess = AES_HW_MAX_PROCESS_SIZE_BYTES;
        else
            BytesToProcess = TotalBytesToProcess;

        if ((!pAes->IsDedicatedSlot) && (NvDdkAesKeyType_UserSpecified == pAes->KeyType))
        {
            // If it is not dedicated slot, unwrap the key
            NvU8 UnWrappedRFCIv[AES_RFC_IV_LENGTH_BYTES];

            NV_CHECK_ERROR_CLEANUP(AesCoreGetFreeSlot(pAes->pAesCoreEngine, &Engine, &KeySlot));
            pAes->Engine = Engine;
            pAes->KeySlot = KeySlot;

            // Unwrap the key
            NV_CHECK_ERROR_CLEANUP(AesCoreUnWrapKey(
                pAes,
                pAes->Key,
                pAes->KeySize,
                pAes->WrappedIv,
                (pAesHwCtxt->pKeyTableVirAddr[Engine] + g_AesHwKeySlotLengthBytes),
                UnWrappedRFCIv));
            // Check whether the key unwrap is success or not by comparing the
            // unwrapped RFC IV with original RFC IV.
            if (NvOsMemcmp(UnWrappedRFCIv, s_OriginalIV, sizeof(s_OriginalIV)))
            {
                // Unwrap key failed.
                e = NvError_AesKeyUnWrapFailed;
                goto fail;
            }
            // Setup Key table
            gs_EngineCaps[pAes->Engine]->pAesInterf->AesHwSetKeyAndIv(
                pAesHwCtxt,
                pAes->Engine,
                pAes->KeySlot,
                (AesHwKey *)(pAesHwCtxt->pKeyTableVirAddr[Engine] + g_AesHwKeySlotLengthBytes),
                (AesKeySize)pAes->KeySize,
                (AesHwIv *)pAes->Iv,
                pAes->IsEncryption);

            // Memset the local variable to zeros where the key is stored.
            NvOsMemset(
                (pAesHwCtxt->pKeyTableVirAddr[Engine] + g_AesHwKeySlotLengthBytes),
                0,
                NvDdkAesKeySize_256Bit);
        }
        else if ((!pAes->IsDedicatedSlot) &&
            (NvDdkAesKeyType_EncryptedKey == pAes->KeyType))
        {
            NV_CHECK_ERROR_CLEANUP(
                AesCoreGetFreeSlot(pAes->pAesCoreEngine, &Engine, &KeySlot));
            pAes->Engine = Engine;
            pAes->KeySlot = KeySlot;

            // Select Key slot
            gs_EngineCaps[pAes->Engine]->pAesInterf->AesHwSelectKeyIvSlot(
                pAesHwCtxt,
                pAes->pAesCoreEngine->SskEngine[0],
                pAes->pAesCoreEngine->SskDecryptSlot,
                pAes->KeySize);

            NV_CHECK_ERROR_CLEANUP(
                gs_EngineCaps[pAes->Engine]->pAesInterf->AesHwDecryptToSlot(
                    pAesHwCtxt,
                    pAes->pAesCoreEngine->SskEngine[0],
                    NvDdkAesKeySize_128Bit,
                    pAes->Key,
                    pAes->KeySlot));

            gs_EngineCaps[pAes->Engine]->pAesInterf->AesHwSelectKeyIvSlot(
                pAesHwCtxt,
                pAes->Engine,
                pAes->KeySlot,
                pAes->KeySize);
            // Set the last IV operated with this client
            gs_EngineCaps[pAes->Engine]->pAesInterf->AesHwSetIv(
                pAesHwCtxt,
                pAes->Engine,
                (AesHwIv *)pAes->Iv,
                pAes->KeySlot);
        }
        else
        {
            // Select Key slot
            gs_EngineCaps[pAes->Engine]->pAesInterf->AesHwSelectKeyIvSlot(pAesHwCtxt,
                                                                          pAes->Engine,
                                                                          pAes->KeySlot,
                                                                          pAes->KeySize);

            // Set the last IV operated with this client
            gs_EngineCaps[pAes->Engine]->pAesInterf->AesHwSetIv(
                pAesHwCtxt,
                pAes->Engine,
                (AesHwIv *)pAes->Iv,
                pAes->KeySlot);
        }

        // Process the buffer for encryption/decryption
        e = gs_EngineCaps[pAes->Engine]->pAesInterf->AesHwStartEngine(
            pAesHwCtxt,
            pAes->Engine,
            BytesToProcess,
            pSrcBuffer,
            pDestBuffer,
            pAes->IsEncryption,
            pAes->OpMode);
        if (e != NvSuccess)
        {
            // If the key is user specified and not in dedicated slot,
            // then clear it.
            if ((!pAes->IsDedicatedSlot) &&
                ((NvDdkAesKeyType_UserSpecified == pAes->KeyType) ||
                (NvDdkAesKeyType_EncryptedKey == pAes->KeyType)))
            {
                // Clear key
                gs_EngineCaps[pAes->Engine]->pAesInterf->AesHwClearKeyAndIv(pAesHwCtxt,
                                                                            pAes->Engine,
                                                                            pAes->KeySlot);
            }
            goto fail;
        }
        // Store the last IV operated with this client
        gs_EngineCaps[pAes->Engine]->pAesInterf->AesHwGetIv(
            pAesHwCtxt,
            pAes->Engine,
            (AesHwIv *)pAes->Iv,
            pAes->KeySlot);

        // If the key is user specified or encrypted key and not in dedicated slot, clear it
        if ((!pAes->IsDedicatedSlot) &&
            ((NvDdkAesKeyType_UserSpecified == pAes->KeyType) ||
            (NvDdkAesKeyType_EncryptedKey == pAes->KeyType)))
        {
            // Clear key
            gs_EngineCaps[pAes->Engine]->pAesInterf->AesHwClearKeyAndIv(pAesHwCtxt,
                                                                        pAes->Engine,
                                                                        pAes->KeySlot);
        }
        else
        {
            // Clear the IV in the engine
            gs_EngineCaps[pAes->Engine]->pAesInterf->AesHwClearIv(pAesHwCtxt,
                                                                  pAes->Engine,
                                                                  pAes->KeySlot);
        }

        pSrcBuffer += BytesToProcess;
        pDestBuffer += BytesToProcess;
        TotalBytesToProcess -= BytesToProcess;

        // release the H/W semaphore
        AesCoreReleaseHwAccess();
    }

    // Clear the DMA buffer before we leave from this operation.
    NV_ASSERT(pAesHwCtxt->pDmaVirAddr[pAes->Engine]);
    NvOsMemset(pAesHwCtxt->pDmaVirAddr[pAes->Engine], 0, AES_HW_DMA_BUFFER_SIZE_BYTES);
    NvOsMutexUnlock(s_AesCoreEngineMutex);
    return NvSuccess;

fail:
    // Release the H/W semaphore
    AesCoreReleaseHwAccess();
    // Clear the DMA buffer before we leave from this operation
    NV_ASSERT(pAesHwCtxt->pDmaVirAddr[pAes->Engine]);
    NvOsMemset(pAesHwCtxt->pDmaVirAddr[pAes->Engine], 0, AES_HW_DMA_BUFFER_SIZE_BYTES);
    NvOsMutexUnlock(s_AesCoreEngineMutex);
    return e;
}

/**
 * Clear the User Key.
 *
 * @param pAes Aes client handle.
 *
 * @retval NV_TRUE if User key is cleared, NV_FALSE otherwise.
 */
NvBool AesCoreClearUserKey(NvDdkAes *pAes)
{
    AesHwContext *pAesHwCtxt = NULL;
    NvBool Status = NV_TRUE;

    NV_ASSERT(pAes);

    NvOsMutexLock(s_AesCoreEngineMutex);
    // Get the AES H/W context
    NV_ASSERT(pAes->pAesCoreEngine);
    pAesHwCtxt = &pAes->pAesCoreEngine->AesHwCtxt;

    AesCoreRequestHwAccess();
    /// Clear the key and IV
    gs_EngineCaps[pAes->Engine]->pAesInterf->AesHwClearKeyAndIv(pAesHwCtxt,
                                                                pAes->Engine,
                                                                pAes->KeySlot);
    AesCoreReleaseHwAccess();

    if (!AesCoreUserKeyClearCheck(pAesHwCtxt, pAes->Engine, pAes->KeySlot, pAes->KeySize))
    {
        // return false if User Key clear check fails
        Status = NV_FALSE;
    }
    NvOsMutexUnlock(s_AesCoreEngineMutex);
    return Status;
}

/**
 * Check the user key clear by encrypting the known data.
 *
 * @param pAesHw Pointer to Aes H/W context.
 * @param Engine engine on which encryption need to be performed
 * @param KeySlot key slot where encrypt key is located.
 * @param KeySize key length in bytes.
 *
 * @retval NV_TRUE if successfully encryption and decryption is done else NV_FALSE
 */
static NvBool AesCoreUserKeyClearCheck(AesHwContext *pAesHw,
                                       AesHwEngine Engine,
                                       AesHwKeySlot KeySlot,
                                       AesKeySize KeySize)
{
    NvError e = NvSuccess;
    AesHwIv ZeroIv;
    NvU32 i;

    // Known Good data
    NvU8 GoldData[NvDdkAesConst_BlockLengthBytes] =
    {
        0x00, 0x01, 0x02, 0x03,
        0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F
    };

    // Encrypted data for above known data with Zero key and Zero IV
    NvU8 EncryptDataWithZeroKeyTable[NvDdkAesConst_BlockLengthBytes] =
    {
        0x7A, 0xCA, 0x0F, 0xD9,
        0xBC, 0xD6, 0xEC, 0x7C,
        0x9F, 0x97, 0x46, 0x66,
        0x16, 0xE6, 0xA2, 0x82
    };

    // Encrypted data for above known data with Zero key and Zero IV
    NvU8 EncryptDataWithZeroKeySchedule[NvDdkAesConst_BlockLengthBytes] =
    {
        0x18, 0x9D, 0x19, 0xEA,
        0xDB, 0xA7, 0xE3, 0x0E,
        0xD9, 0x72, 0x80, 0x8F,
        0x3F, 0x2B, 0xA0, 0x30
    };

    NvU8 *EncryptData;
    NvU8 EncryptBuffer[NvDdkAesConst_BlockLengthBytes];

    NvOsMutexLock(s_AesCoreEngineMutex);

    if (gs_EngineCaps[Engine]->IsHwKeySchedGenSupported)
    {
        EncryptData = EncryptDataWithZeroKeyTable;
    }
    else
    {
        EncryptData = EncryptDataWithZeroKeySchedule;
    }

    NvOsMemset(EncryptBuffer, 0, NvDdkAesConst_BlockLengthBytes);
    NvOsMemset(&ZeroIv, 0, sizeof(AesHwIv));

    AesCoreRequestHwAccess();

    // Select Encrypt Key slot
    gs_EngineCaps[Engine]->pAesInterf->AesHwSelectKeyIvSlot(pAesHw, Engine, KeySlot, KeySize);

    // Set the Zero IV for test data
    gs_EngineCaps[Engine]->pAesInterf->AesHwSetIv(pAesHw, Engine, &ZeroIv, KeySlot);

    // Process the buffer for encryption
    NV_CHECK_ERROR_CLEANUP(gs_EngineCaps[Engine]->pAesInterf->AesHwStartEngine(
        pAesHw,
        Engine,
        NvDdkAesConst_BlockLengthBytes,
        GoldData,
        EncryptBuffer,
        NV_TRUE,
        NvDdkAesOperationalMode_Cbc));

    // Clear the IV in the engine before we leave
    gs_EngineCaps[Engine]->pAesInterf->AesHwClearIv(pAesHw, Engine, KeySlot);

    AesCoreReleaseHwAccess();

    // Clear the DMA buffer before we leave from this operation
    NV_ASSERT(pAesHw->pDmaVirAddr[Engine]);
    NvOsMemset(pAesHw->pDmaVirAddr[Engine], 0, AES_HW_DMA_BUFFER_SIZE_BYTES);

    for (i = 0; i < NvDdkAesConst_BlockLengthBytes; i++)
    {
        if (EncryptData[i] != EncryptBuffer[i])
           goto fail;
    }

    NvOsMutexUnlock(s_AesCoreEngineMutex);
    return NV_TRUE;

fail:
    // Clear the DMA buffer before we leave from this operation
    NV_ASSERT(pAesHw->pDmaVirAddr[Engine]);
    NvOsMemset(pAesHw->pDmaVirAddr[Engine], 0, AES_HW_DMA_BUFFER_SIZE_BYTES);
    NvOsMutexUnlock(s_AesCoreEngineMutex);
    return NV_FALSE;
}

/**
 * Return the AES module capabilities.
 *
 * @param hRmDevice Rm Device handle
 * @param caps pointer to the capcbilities structure
 *
 * @retval NvSuccess if capabilities could be returned successfully
 *
 */
NvError AesCoreGetCapabilities(NvRmDeviceHandle hRmDevice, AesHwCapabilities **caps)
{
    NvError e = NvSuccess;

    static AesHwCapabilities s_Caps;

    static NvRmModuleCapability s_EngineA_Caps[] =
    {
        {1, 3, 0, NvRmModulePlatform_Silicon, &s_Caps}
    };

    static NvRmModuleCapability s_engineB_caps[] = {
        {1, 2, 0, NvRmModulePlatform_Silicon, &s_Caps}
    };

    NV_ASSERT(hRmDevice);
    NV_ASSERT(caps);

    s_Caps.isHashSupported = NV_FALSE;
    s_Caps.IsHwKeySchedGenSupported = NV_TRUE;
    s_Caps.MinBufferAlignment = 16;
    s_Caps.MinKeyTableAlignment = 4;
    s_Caps.NumSlotsSupported = AesHwKeySlot_NumExt;
    s_Caps.pAesInterf = NULL;

    e = NvRmOpen(&hRmDevice, 0);
    if (e != NvSuccess)
    {
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(NvRmModuleGetCapabilities(
        hRmDevice,
        NvRmModuleID_Vde,
        s_EngineA_Caps,
        NV_ARRAY_SIZE(s_EngineA_Caps),
        (void **)&(caps[AesHwEngine_A])));

    if (caps[AesHwEngine_A] == &s_Caps)
    {
        if (s_Caps.pAesInterf == NULL)
        {
            s_Caps.pAesInterf = NvOsAlloc(sizeof(AesHwInterface));
            if (s_Caps.pAesInterf == NULL)
            {
                goto fail;
            }
            NvAesIntfT30GetHwInterface(s_Caps.pAesInterf);
        }
    }

    NV_CHECK_ERROR_CLEANUP(NvRmModuleGetCapabilities(
        hRmDevice,
        NvRmModuleID_BseA,
        s_engineB_caps,
        NV_ARRAY_SIZE(s_engineB_caps),
        (void **)&(caps[AesHwEngine_B])));

    if (caps[AesHwEngine_B] == &s_Caps)
    {
        if (s_Caps.pAesInterf == NULL)
        {
            s_Caps.pAesInterf = NvOsAlloc(sizeof(AesHwInterface));
            if (s_Caps.pAesInterf == NULL)
            {
                goto fail;
            }
            NvAesIntfT30GetHwInterface(s_Caps.pAesInterf);
        }
    }

fail:
    return e;
}

/**
 * Encrypt/Decrypt the given input buffer using Electronic CodeBook (ECB) mode.
 *
 * @param pAes Aes client handle.
 * @param pInputBuffer Pointer to input bufffer.
 * @param pOutputBuffer pointer to output buffer.
 * @param BufSize Buffer size.
 * @param IsEncrypt If set to NV_TRUE, encrypts the input buffer
 *          else decrypts it.
 *
 * @retval NvSuccess if successfully completed.
 */
NvError
AesCoreEcbProcessBuffer(
    NvDdkAes *pAes,
    NvU8 *pInputBuffer,
    NvU8 *pOutputBuffer,
    NvU32 BufSize,
    NvBool IsEncrypt)
{
    NvError e = NvSuccess;
    AesHwEngine SskEngine;
    AesHwKeySlot SskKeySlot;
    AesHwContext *pAesHwCtxt;

    NV_ASSERT(pAes);
    NV_ASSERT(pInputBuffer);
    NV_ASSERT(pOutputBuffer);
    NV_ASSERT(pAes->pAesCoreEngine);

    SskEngine = pAes->pAesCoreEngine->SskEngine[0];
    SskKeySlot = (IsEncrypt ? pAes->pAesCoreEngine->SskEncryptSlot : pAes->pAesCoreEngine->SskDecryptSlot);

    // Get the AES H/W context
    pAesHwCtxt = &pAes->pAesCoreEngine->AesHwCtxt;

    // Select SSK key for processing
    gs_EngineCaps[SskEngine]->pAesInterf->AesHwSelectKeyIvSlot(pAesHwCtxt,
                                                               SskEngine,
                                                               SskKeySlot,
                                                               NvDdkAesKeySize_128Bit);
    // Process the buffer
    e = gs_EngineCaps[SskEngine]->pAesInterf->AesHwStartEngine(
        pAesHwCtxt,
        SskEngine,
        BufSize,
        pInputBuffer,
        pOutputBuffer,
        IsEncrypt,
        NvDdkAesOperationalMode_Ecb);

    // Clear the DMA buffer
    NV_ASSERT(pAesHwCtxt->pDmaVirAddr[SskEngine]);
    NvOsMemset(pAesHwCtxt->pDmaVirAddr[SskEngine], 0, AES_HW_DMA_BUFFER_SIZE_BYTES);
    return e;
}

/**
 * Wrap the given key data using RFC 3394 algoritham.
 *
 * Follwing is RFC3394 key wrap algoritham.
 *     Inputs:  Plaintext, n 64-bit values {P1, P2, ..., Pn}, and
 *             Key, K (the KEK).
 *    Outputs: Ciphertext, (n+1) 64-bit values {C0, C1, ..., Cn}.

 *    1) Initialize variables.
 *        Set A = IV, an initial value (see 2.2.3)
 *        For i = 1 to n
 *            R[i] = P[i]
 *    2) Calculate intermediate values.
 *        For j = 0 to 5
 *            For i=1 to n
 *                B = AES(K, A | R[i])
 *                A = MSB(64, B) ^ t where t = (n*j)+i
 *                R[i] = LSB(64, B)
 *    3) Output the results.
 *        Set C[0] = A
 *        For i = 1 to n
 *            C[i] = R[i]
 *
 * @param pAes Aes client handle.
 * @param pOrgKey Pointer to Original Key.
 * @param KeySize key length specified in bytes
 * @param pOrgIv pointer to Original Iv which is used in RFC3394 algoritham.
 * @param pWrappedKey Pointer to wrapped key
 * @param pWrappedIv pointer to wrapped Iv
 *
 */
NvError AesCoreWrapKey(
    NvDdkAes *pAes,
    const NvU8 *pOrgKey,
    NvDdkAesKeySize KeySize,
    NvU8 *pOrgIv,
    NvU8 *pWrappedKey,
    NvU8 *pWrappedIv)
{
    NvError e = NvSuccess;
    NvU8 n, i, j, k, t;
    NvU8 *A, *B;
    NvU8 R[AES_RFC_3394_NUM_OF_BLOCKS_FOR_256BIT_KEY][AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES];
    AesHwContext *pAesHwCtxt;

    NV_ASSERT(pAes);
    NV_ASSERT(pOrgKey);
    NV_ASSERT(pOrgIv);
    NV_ASSERT(pWrappedKey);
    NV_ASSERT(pWrappedIv);
    NV_ASSERT(pAes->pAesCoreEngine);

    switch (KeySize)
    {
        case NvDdkAesKeySize_128Bit:
            n = AES_RFC_3394_NUM_OF_BLOCKS_FOR_128BIT_KEY;
            break;
        case NvDdkAesKeySize_192Bit:
            n = AES_RFC_3394_NUM_OF_BLOCKS_FOR_192BIT_KEY;
            break;
        case NvDdkAesKeySize_256Bit:
            n = AES_RFC_3394_NUM_OF_BLOCKS_FOR_256BIT_KEY;
            break;
        default:
            n = AES_RFC_3394_NUM_OF_BLOCKS_FOR_128BIT_KEY;
            break;
    }

    pAesHwCtxt = &pAes->pAesCoreEngine->AesHwCtxt;

    NV_ASSERT(pAesHwCtxt);

    // Local buffers which are used for processing should be in IRAM.
    // Use KeyTable buffer which is in IRAM.
    // The local variables should be of following format and sizes.
    // NvU8 A[AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES];
    // NvU8 B[AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES * AES_RFC_3394_NUM_OF_BLOCKS_FOR_128BIT_KEY];
    // NvU8 R[AES_RFC_3394_NUM_OF_BLOCKS_FOR_128BIT_KEY][AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES];

    A = pAesHwCtxt->pKeyTableVirAddr[0];
    B = A + AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES;

    //Set A = IV
    for (i = 0; i < AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES; i++)
        A[i] =  pOrgIv[i];

    //For i = 1 to n 	R[i] = P[i]
    for (i = 0; i < n; i++)
    {
        for (j = 0; j < AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES; j++)
            R[i][j] = pOrgKey[j + (i *AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES)];
    }

    // Calculate intermediate values.
    //  For j = 0 to 5
    //      For i=1 to n
    //          B = AES(K, A | R[i])
    //          A = MSB(64, B) ^ t where t = (n*j)+i
    //          R[i] = LSB(64, B)
    for (j = 0; j <= 5; j++)
    {
        for (i = 0; i < n; i++)
        {
            for (k = 0; k < AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES; k++)
            {
                B[k] = A[k];
                B[AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES+k] = R[i][k];
            }
            NV_CHECK_ERROR(AesCoreEcbProcessBuffer(pAes, B, B, NvDdkAesKeySize_128Bit, NV_TRUE));
            for (k = 0; k < AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES; k++)
            {
                A[k] =  B[k];
                R[i][k] = B[AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES + k];
            }
            t = (n * j) + (i+1);
            A[AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES-1] ^=  t;
        }
    }

    // Output the results.
       //Set C[0] = A
       //For i = 1 to n
       //    C[i] = R[i]
    for (k = 0; k < AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES; k++) {
        pWrappedIv[k] = A[k];
    }
    for (i = 0; i < n; i++)
    {
        for (k = 0; k < AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES; k++)
        {
            pWrappedKey[(AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES*i) + k] = R[i][k];
        }
    }

    // Clear the buffers.
    NvOsMemset(pAesHwCtxt->pKeyTableVirAddr[0], 0, g_AesHwKeySlotLengthBytes);
    return e;
}

/**
 * UnWrapKey the given key data using RFC 3394 algoritham.
 *
 * Follwing is RFC3394 key unwrap algoritham.
 *  Inputs:  Ciphertext, (n+1) 64-bit values {C0, C1, ..., Cn}, and
 *             Key, K (the KEK).
 *    Outputs: Plaintext, n 64-bit values {P0, P1, K, Pn}.
 *
 *    1) Initialize variables.
 *        Set A = C[0]
 *        For i = 1 to n
 *            R[i] = C[i]
 *    2) Compute intermediate values.
 *        For j = 5 to 0
 *            For i = n to 1
 *                B = AES-1(K, (A ^ t) | R[i]) where t = n*j+i
 *                A = MSB(64, B)
 *                R[i] = LSB(64, B)
 *    3) Output results.
 *    If A is an appropriate initial value (see 2.2.3),
 *    Then
 *        For i = 1 to n
 *            P[i] = R[i]
 *    Else
 *        Return an error
 *
 * @param pAes Aes client handle.
 * @param pWrappedKey Pointer to wrapped key
 * @param KeySize key length specified in bytes
 * @param pWrappedIv pointer to wrapped Iv
 * @param pOrgKey Pointer to Original Key.
 * @param pOrgIv pointer to Original Iv which is used in RFC3394 algoritham.
 *
 */
NvError
AesCoreUnWrapKey(
    NvDdkAes *pAes,
    NvU8 *pWrappedKey,
    NvDdkAesKeySize KeySize,
    NvU8 *pWrappedIv,
    NvU8 *pOrgKey,
    NvU8 *pOrgIv)
{
    NvError e = NvSuccess;
    NvS32 n, i, j, k, t;
    NvU8 *A, *B;
    NvU8 R[AES_RFC_3394_NUM_OF_BLOCKS_FOR_256BIT_KEY][AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES];
    AesHwContext *pAesHwCtxt;

    NV_ASSERT(pAes);
    NV_ASSERT(pWrappedKey);
    NV_ASSERT(pWrappedIv);
    NV_ASSERT(pOrgKey);
    NV_ASSERT(pOrgIv);
    NV_ASSERT(pAes->pAesCoreEngine);

    switch (KeySize)
    {
        case NvDdkAesKeySize_128Bit:
            n = AES_RFC_3394_NUM_OF_BLOCKS_FOR_128BIT_KEY;
            break;
        case NvDdkAesKeySize_192Bit:
            n = AES_RFC_3394_NUM_OF_BLOCKS_FOR_192BIT_KEY;
            break;
        case NvDdkAesKeySize_256Bit:
            n = AES_RFC_3394_NUM_OF_BLOCKS_FOR_256BIT_KEY;
            break;
        default:
            n = AES_RFC_3394_NUM_OF_BLOCKS_FOR_128BIT_KEY;
            break;
    }

    pAesHwCtxt = &pAes->pAesCoreEngine->AesHwCtxt;

    NV_ASSERT(pAesHwCtxt);

    // Local buffers which are used for processing should in IRAM.
    // Use KeyTable buffer which is in IRAM.
    // The local variables should be of following format and sizes.
    // NvU8 A[AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES];
    // NvU8 B[AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES * AES_RFC_3394_NUM_OF_BLOCKS_FOR_128BIT_KEY];
    // NvU8 R[AES_RFC_3394_NUM_OF_BLOCKS_FOR_128BIT_KEY][AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES];

    A = pAesHwCtxt->pKeyTableVirAddr[0];
    B = A + AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES;

    // Set A = C[0]
    for (i = 0; i < AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES; i++)
        A[i] = pWrappedIv[i];

    // For i = 1 to n R[i] = C[i]
    for (i = 0; i < n; i++) {
        for (j = 0; j < AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES; j++)
            R[i][j] = pWrappedKey[j + (i *AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES)];
    }

    // Compute intermediate values.
    //   For j = 5 to 0
    //       For i = n to 1
    //           B = AES-1(K, (A ^ t) | R[i]) where t = n*j+i
    //           A = MSB(64, B)
    //           R[i] = LSB(64, B)
    for (j = 5; j >= 0; j--)
    {
        for (i = n; i > 0; i--)
        {
            t = (n * j) + (i);
            A[AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES-1] ^=  t;
            for (k = 0; k < AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES; k++)
            {
                B[k] = A[k];
                B[AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES+k] = R[i-1][k];
            }
            NV_CHECK_ERROR(AesCoreEcbProcessBuffer(pAes, B, B, NvDdkAesKeySize_128Bit, NV_FALSE));
            for (k = 0; k < AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES; k++)
            {
                A[k] =  B[k];
                R[i-1][k] = B[AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES + k];
            }
        }
    }

    //Output results.
    //For i = 1 to n
    //P[i] = R[i]
    for (k = 0; k < AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES; k++)
    {
        pOrgIv[k] = A[k];
    }
    for (i = 0; i < n; i++)
    {
        for (k = 0; k < AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES; k++)
        {
            pOrgKey[(AES_RFC_3394_KEY_WRAP_BLOCK_SIZE_BYTES*i) + k] = R[i][k];
        }
    }

    // Clear the buffers.
    NvOsMemset(pAesHwCtxt->pKeyTableVirAddr[0], 0, g_AesHwKeySlotLengthBytes);
    return e;
}

// Following funcs are used by BlockDevice API. They should go away once this API is deprecated.
// Reasons for being defined in this file:
// 1. They take NvDdkAes as argument and not a BlockDevice client,
// 2. They use static variables and funcs declared in this file and shared with NvDdkAes.

void AesCoreOpen(AesCoreEngine **ppAesCoreEngine)
{
    NV_ASSERT(ppAesCoreEngine);

    NvOsMutexLock(s_AesCoreEngineMutex);

    // Add client
    s_pAesCoreEngine->OpenCount++;
    *ppAesCoreEngine = s_pAesCoreEngine;

    NvOsMutexUnlock(s_AesCoreEngineMutex);
}

void AesCoreClose(NvDdkAes *pAes)
{
    NV_ASSERT(pAes);
    NV_ASSERT(pAes->pAesCoreEngine);

    NvOsMutexLock(s_AesCoreEngineMutex);

    if (pAes->pAesCoreEngine->OpenCount)
    {
        // Decrement the Open count
        pAes->pAesCoreEngine->OpenCount--;

        // Check if client is using USER key with dedicated slot
        // then free the associated client slot
        if ((NV_TRUE == pAes->IsDedicatedSlot) &&
            (NvDdkAesKeyType_UserSpecified == pAes->KeyType))
        {
            AesCorePowerUp(pAes->pAesCoreEngine);
            // Client is using USER key with dedicated slot. So
            // clear the key in hardware slot and free the associated client slot
            if (!AesCoreClearUserKey(pAes))
                NV_ASSERT(!"Failed to clear User Key");
            AesCorePowerDown(pAes->pAesCoreEngine);
            pAes->pAesCoreEngine->IsKeySlotUsed[pAes->Engine][pAes->KeySlot] = NV_FALSE;
        }
    }

    NvOsMutexUnlock(s_AesCoreEngineMutex);
}

NvError AesCoreInit(NvRmDeviceHandle hRmDevice)
{
    NvError e = NvSuccess;

    // Initialize the H/W requirements and global resources if any
    if (NULL == s_pAesCoreEngine)
    {
        // Create mutex to guard the global data
        NV_CHECK_ERROR_CLEANUP(NvOsMutexCreate(&s_AesCoreEngineMutex));

        NV_CHECK_ERROR_CLEANUP(AesCoreInitEngine(hRmDevice));
    }
    return NvSuccess;

fail:
    AesCoreFreeUpEngine();
    NvOsMutexDestroy(s_AesCoreEngineMutex);
    NvOsFree(s_pAesCoreEngine);
    s_pAesCoreEngine = NULL;
    return e;
}

void AesCoreDeinit(void)
{
    if (NULL == s_pAesCoreEngine)
        return;

    NvOsMutexLock(s_AesCoreEngineMutex);

    // If still any clients are open then just return
    if (s_pAesCoreEngine->OpenCount)
    {
        NvOsMutexUnlock(s_AesCoreEngineMutex);
        return;
    }

    AesCoreFreeUpEngine();

    NvOsMutexUnlock(s_AesCoreEngineMutex);

    // Destroy mutex
    NvOsMutexDestroy(s_AesCoreEngineMutex);

    NvOsFree(s_pAesCoreEngine);
    s_pAesCoreEngine = NULL;
}

NvBool AesCoreInitIsDone(void)
{
    return (NULL != s_pAesCoreEngine);
}
