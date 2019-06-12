/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "nvos.h"
#include "nvassert.h"
#include "nvrm_drf.h"
#include "nvrm_hardware_access.h"
#include "nvrm_init.h"
#include "nvddk_aes_common.h"
#include "nvddk_aes_priv.h"
#include "t30/nvboot_aes.h"
#include "nvsecureservices.h"

static void T30AesSetupTable_Int(AesHwContext *pAesHw,
                                  AesHwEngine engine,
                                  AesHwKeySlot slot);
static void T30AesHwSelectKeyIvSlot(AesHwContext *pAesHw,
                                     AesHwEngine engine,
                                     AesHwKeySlot slot,
                                     AesKeySize KeySize);
static void T30AesHwClearKeyAndIv(AesHwContext *pAesHw,
                                   AesHwEngine engine,
                                   AesHwKeySlot slot);
static void T30AesHwClearIv(AesHwContext *pAesHw,
                             AesHwEngine engine,
                             AesHwKeySlot slot);
static void T30AesHwGetIv(AesHwContext *pAesHw,
                           AesHwEngine engine,
                           AesHwIv *pIv,
                           AesHwKeySlot slot);
static void T30AesHwLockSskReadWrites(AesHwContext *pAesHw, AesHwEngine SskEngine);
static void T30AesHwLoadSskToSecureScratchAndLock(NvRmPhysAddr PmicBaseAddr,
                                                   AesHwKey *pKey,
                                                   size_t Size);
static void T30AesGetUsedSlots(AesCoreEngine *pAesCoreEngine);

static NvBool T30AesHwDisableEngine(AesHwContext *pAesHw, AesHwEngine engine);
static NvBool T30AesHwIsEngineDisabled(AesHwContext *pAesHw, AesHwEngine engine);
static NvBool T30AesHwIsSskUpdateAllowed(const NvRmDeviceHandle hRmDevice);

static void
T30AesHwSetKeyAndIv(
    AesHwContext *pAesHw,
    AesHwEngine engine,
    AesHwKeySlot slot,
    AesHwKey *pKey,
    AesKeySize KeySize,
    AesHwIv *pIv,
    NvBool IsEncryption);

static void
T30AesHwSetIv(
    AesHwContext *pAesHw,
    AesHwEngine engine,
    AesHwIv *pIv,
    AesHwKeySlot slot);

static void
T30AesHwSetKey(
    AesHwContext *pAesHw,
    AesHwEngine engine,
    AesHwKey *pKey,
    AesKeySize KeySize,
    AesHwKeySlot slot);

static void
T30AesHwDisableAllKeyRead(
    AesHwContext *pAesHw,
    AesHwEngine engine,
    AesHwKeySlot NumSlotsSupported);

static NvError
T30AesHwStartEngine(
    AesHwContext *pAesHw,
    AesHwEngine engine,
    NvU32 DataSize,
    const NvU8 *src,
    NvU8 *dst,
    NvBool IsEncryption,
    NvDdkAesOperationalMode OpMode);

static NvError
T30AesDecryptToSlot(
    AesHwContext *pAesHw,
    AesHwEngine engine,
    const NvU32 DataSize,
    const NvU8 *src,
    const AesHwKeySlot DestSlotNum);

//---------------------Private Variables Declaration---------------------------
/**
* @brief Structure to preserve the Iv for each key slot in each engine
*           The Iv is preserved within the driver because updated Iv is locked
*            down for dedicated key slots against reads.
*            The updated Iv has no valid value/use/relevance before an encryption
*           /decryption operation is performed.The updated Iv for any key slot
*           shall be returned as all zeroes in case no encryption/decryption
*           operation has been performed with the concerned key slot within the
*           engine.
**/
typedef struct T30AesPrivIVRec{
    // Updated Iv for each key slot in each AES engine
    NvU32 CurrentIv[AesHwKeySlot_NumExt][AES_HW_IV_LENGTH];
    // The key slot in use
    AesHwKeySlot KeySlotInUse;
}T30AesPrivIV;

static NvBool isEngineInitialized[AesHwEngine_Num];
static T30AesPrivIV gs_T30AesIv[AesHwEngine_Num];

/**
 * Set the Setup Table command required for the AES engine
 *
 * @param engine which AES engine to setup the Key table
 * @param slot For which AES Key slot to use for setting up the key table
 *
 * @retval None
 */
void T30AesSetupTable_Int(AesHwContext *pAesHw, AesHwEngine engine, AesHwKeySlot slot)
{
    NV_ASSERT(pAesHw);

    NvSecureServiceAesSetupTable(pAesHw->KeyTablePhyAddr[engine], engine, slot);

    NvOsMemcpy(gs_T30AesIv[engine].CurrentIv[slot],
        (void *)(&pAesHw->pKeyTableVirAddr[engine][AES_HW_KEY_TABLE_LENGTH - AES_HW_IV_LENGTH]),
        NvDdkAesConst_BlockLengthBytes);

    // Clear key table in the memory after updating to the H/W
    NvOsMemset(pAesHw->pKeyTableVirAddr[engine], 0, g_AesHwKeySlotLengthBytes);
}

/**
 * Select the key and iv from the internal key table for a specified key slot.
 *
 * @param pAesHw Pointer to the AES H/W context
 * @param engine AES engine
 * @param slot key slot for which key and IV needs to be selected
 *
 * @retval None
 */
void T30AesHwSelectKeyIvSlot(AesHwContext *pAesHw,
                              AesHwEngine engine,
                              AesHwKeySlot slot,
                              AesKeySize KeySize)
{
    NV_ASSERT(pAesHw);

    NvOsMutexLock(pAesHw->mutex[engine]);

    // Wait till engine becomes IDLE
    NvSecureServiceAesWaitTillEngineIdle(engine);

    // Select the KEY slot for updating the IV vectors
    NvSecureServiceAesSelectKeyIvSlot(engine, slot);
    // Update the key length
    NvSecureServiceAesSetKeySize(engine,KeySize);

    gs_T30AesIv[engine].KeySlotInUse = slot;

    NvOsMutexUnlock(pAesHw->mutex[engine]);
}

/**
 * Disable the selected AES engine.  No further operations can be
 * performed using the AES engine until the entire chip is reset.
 *
 * @param pAesHw Pointer to the AES H/W context
 * @param engine which AES engine to disable
 *
 * @retval NV_TRUE if successfully disabled the engine else NV_FALSE
 */
NvBool T30AesHwDisableEngine(AesHwContext *pAesHw, AesHwEngine engine)
{
    NvBool result = NV_FALSE;

    NV_ASSERT(pAesHw);

    NvOsMutexLock(pAesHw->mutex[engine]);

    // Wait till engine becomes IDLE
    NvSecureServiceAesWaitTillEngineIdle(engine);

    result = NvSecureServiceAesDisableEngine(engine);

    NvOsMutexUnlock(pAesHw->mutex[engine]);

    return result;
}

/**
 * Over-write the key schedule and Initial Vector in the in the specified key slot with zeroes.
 * Convenient for preventing subsequent callers from gaining access to a previously-used key.
 *
 * @param pAesHw Pointer to the AES H/W context
 * @param engine AES engine
 * @param slot key slot to clear
 *
 * @retval None
 */
void T30AesHwClearKeyAndIv(AesHwContext *pAesHw,
                            AesHwEngine engine,
                            AesHwKeySlot slot)
{
    NV_ASSERT(pAesHw);

    NvOsMutexLock(pAesHw->mutex[engine]);

    // Wait till engine becomes IDLE
    NvSecureServiceAesWaitTillEngineIdle(engine);

    // Clear Key table this clears both Key Schedule and IV in key table
    NvOsMemset(pAesHw->pKeyTableVirAddr[engine], 0, g_AesHwKeySlotLengthBytes);

    // Setup the key table with Zero key and Zero Iv
    T30AesHwSelectKeyIvSlot(pAesHw, engine, slot, AesKeySize_256Bit);
    T30AesSetupTable_Int(pAesHw, engine, slot);

    NvOsMutexUnlock(pAesHw->mutex[engine]);
}

/**
 * Over-write the initial vector in the specified AES engine with zeroes.
 * Convenient to prevent subsequent callers from gaining access to a
 * previously-used initial vector.
 *
 * @param pAesHw Pointer to the AES H/W context
 * @param engine AES engine
 * @param slot key slot for which Iv needs to be cleared
 *
 * @retval None
 */
void T30AesHwClearIv(AesHwContext *pAesHw, AesHwEngine engine, AesHwKeySlot slot)
{

    NV_ASSERT(pAesHw);
    NV_ASSERT(pAesHw->pKeyTableVirAddr[engine]);

    NvOsMemset((void *)pAesHw->pKeyTableVirAddr[engine], 0,
                        NvDdkAesConst_IVLengthBytes);

    T30AesHwStartEngine(
                pAesHw,
                engine,
                NvDdkAesConst_IVLengthBytes,
                pAesHw->pKeyTableVirAddr[engine],
                pAesHw->pKeyTableVirAddr[engine],
                NV_FALSE,
                NvDdkAesOperationalMode_Cbc);
}

/**
 * Retrieve the initial vector for the specified AES engine.
 *
 * @param engine AES engine
 * @param pointer to the initial vector
 * @param slot key slot for which Iv is to be retrieved
 *
 * @retval None
 */
void T30AesHwGetIv(AesHwContext *pAesHw, AesHwEngine engine, AesHwIv *pIv, AesHwKeySlot slot)
{
    NV_ASSERT(pAesHw);
    NV_ASSERT(pIv);

    NvOsMutexLock(pAesHw->mutex[engine]);

    NvOsMemcpy(&pIv->iv[0],
        gs_T30AesIv[engine].CurrentIv[slot],
        NvDdkAesConst_BlockLengthBytes);

    NvOsMutexUnlock(pAesHw->mutex[engine]);
}

/**
 * Lock the Secure Session Key (SSK) slots.
 * This API disables the read/write permissions to the secure key slots.
 *
 * @param pAesHw Pointer to the AES H/W context
 * @param SskEngine SSK engine number
 *
 * @retval None
 */
void T30AesHwLockSskReadWrites(AesHwContext *pAesHw, AesHwEngine SskEngine)
{
    /* The SSK keys are kept in "secure" key  slots because both
     * read and write protection is needed.
     *
     * Slots 2 and 3 are "secure" slots, meaning that read and/or write
     *   protection can be provided.
     *
     */
    // Lock down the permissions for SSK (secure key slot pair).
    // 1. Read AES config register.
    // 2. Disable read/Write access to the selected *secure* key slot for selected
    //    engine.
    // 3. Update the config register with the permissions
    // Note: SSK encrypt and decrypt key slots are on the same engine

    NvOsMutexLock(pAesHw->mutex[SskEngine]);

    NvSecureServiceAesLockSskReadWrites(SskEngine);

    NvOsMutexUnlock(pAesHw->mutex[SskEngine]);
}

/**
 * Encrypt/Decrypt a specified number of blocks of cyphertext using
 * Cipher Block Chaining (CBC) mode.  A block is 16 bytes.
 * This is non-blocking API and need to call AesHwEngineIdle()
 * to check the engine status to confirm the AES engine operation is
 * done and comes out of the BUSY state.
 * Also make sure before calling this API engine must be IDLE.
 *
 * @param pAesHw Pointer to the AES H/W context
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
 * @param OpMode Specifies the AES operational mode
 *
 * @retval NvSuccess if AES operation is successful
 * @retval NvError_InvalidState if operation mode is not supported.
 */
NvError
T30AesHwStartEngine(
    AesHwContext *pAesHw,
    AesHwEngine engine,
    NvU32 DataSize,
    const NvU8 *src,
    NvU8 *dst,
    NvBool IsEncryption,
    NvDdkAesOperationalMode OpMode)
{
    NvU32 TotalBytes = DataSize;
    NvU32 NumBlocks = 0;
    NvU32 BytesToProcess = 0;
    NvU8 *pSourceBuffer = (NvU8 *)src;
    NvU8 *pDestBuffer = dst;

    NV_ASSERT(pAesHw);
    NV_ASSERT(src);
    NV_ASSERT(dst);

    switch (OpMode)
    {
        case NvDdkAesOperationalMode_AnsiX931:
        case NvDdkAesOperationalMode_Cbc:
        case NvDdkAesOperationalMode_Ecb:
            break;
        default:
            return NvError_InvalidState;
    }

    NvOsMutexLock(pAesHw->mutex[engine]);

    /*
     * If DataSize is zero, Iv would remain unchanged.
     * For an encryption operation, the current Iv will be the last block of
     * ciphertext - input to decryption operation.
     */
    if ((DataSize) && (!IsEncryption))
    {
        NvOsMemcpy(gs_T30AesIv[engine].CurrentIv[gs_T30AesIv[engine].KeySlotInUse],
            (src + DataSize - NvDdkAesConst_BlockLengthBytes),
            NvDdkAesConst_BlockLengthBytes);
    }
    while (TotalBytes)
    {
        if (TotalBytes > AES_HW_DMA_BUFFER_SIZE_BYTES)
        {
            BytesToProcess = AES_HW_DMA_BUFFER_SIZE_BYTES;
        }
        else
        {
            BytesToProcess = TotalBytes;
        }

        // Copy data to DMA buffer from the client buffer
        NvOsMemcpy(pAesHw->pDmaVirAddr[engine], (void *)pSourceBuffer, BytesToProcess);
        NvOsFlushWriteCombineBuffer();

        NumBlocks = BytesToProcess / NvDdkAesConst_BlockLengthBytes;

        NvSecureServiceAesProcessBuffer(
            engine,
            pAesHw->DmaPhyAddr[engine],
            pAesHw->DmaPhyAddr[engine],
            NumBlocks,
            IsEncryption,
            OpMode);
        NvOsFlushWriteCombineBuffer();
        // Copy data from DMA buffer to the client buffer
        NvOsMemcpy(pDestBuffer, pAesHw->pDmaVirAddr[engine], BytesToProcess);

        // Increment the buffer pointer
        pSourceBuffer += BytesToProcess;
        pDestBuffer += BytesToProcess;
        TotalBytes -= BytesToProcess;
    }

    /*
     * If DataSize is zero, Iv would remain unchanged.
     * For an encryption operation, the current Iv will be the last block of
     * ciphertext.
     */
    if ((DataSize) && (IsEncryption))
    {
        NvOsMemcpy(gs_T30AesIv[engine].CurrentIv[gs_T30AesIv[engine].KeySlotInUse],
            (dst + DataSize - NvDdkAesConst_BlockLengthBytes),
            NvDdkAesConst_BlockLengthBytes);
    }
    NvOsMutexUnlock(pAesHw->mutex[engine]);

    return NvSuccess;
}

/**
 * Load the SSK key into secure scratch resgister and disables the write permissions.
 * Note: If Key is not specified then this API locks the Secure Scratch registers.
 *
 * @param PmicBaseAddr PMIC base address
 * @param pKey pointer to the key. If pKey=NULL then key will not be set to the
 * secure scratch registers, but locks the Secure scratch register.
 * @param Size length of the aperture in bytes
 *
 * @retval None
 */
void T30AesHwLoadSskToSecureScratchAndLock(NvRmPhysAddr PmicBaseAddr, AesHwKey *pKey, size_t Size)
{
    NvSecureServiceAesLoadSskToSecureScratchAndLock(PmicBaseAddr, (pKey ? pKey->key : 0), Size);
}

/**
 * Mark all dedicated slots as used.
 *
 * @param pAesCoreEngine Pointer to AES engine
 *
 * @retval None
 *
 */
static void T30AesGetUsedSlots(AesCoreEngine *pAesCoreEngine)
{
    // For ap20, SBK and SSK reside on both engines
    pAesCoreEngine->SbkEngine[0] = NVBOOT_SBK_ENGINEA;
    pAesCoreEngine->SbkEncryptSlot = NVBOOT_SBK_ENCRYPT_SLOT;
    pAesCoreEngine->SbkDecryptSlot = NVBOOT_SBK_DECRYPT_SLOT;
    pAesCoreEngine->IsKeySlotUsed[NVBOOT_SBK_ENGINEA][NVBOOT_SBK_ENCRYPT_SLOT] = NV_TRUE;

    pAesCoreEngine->SbkEngine[1] = NVBOOT_SBK_ENGINEB;
    pAesCoreEngine->IsKeySlotUsed[NVBOOT_SBK_ENGINEB][NVBOOT_SBK_DECRYPT_SLOT] = NV_TRUE;

    pAesCoreEngine->SskEngine[0] = NVBOOT_SSK_ENGINEA;
    pAesCoreEngine->SskEncryptSlot = NVBOOT_SSK_ENCRYPT_SLOT;
    pAesCoreEngine->SskDecryptSlot = NVBOOT_SSK_DECRYPT_SLOT;
    pAesCoreEngine->IsKeySlotUsed[NVBOOT_SSK_ENGINEA][NVBOOT_SSK_ENCRYPT_SLOT] = NV_TRUE;

    pAesCoreEngine->SskEngine[1] = NVBOOT_SSK_ENGINEB;
    pAesCoreEngine->IsKeySlotUsed[NVBOOT_SSK_ENGINEB][NVBOOT_SSK_DECRYPT_SLOT] = NV_TRUE;
}

/**
 * Read the AES engine disable status.
 *
 * @param pAesHw Pointer to the AES H/W context
 * @param engine which AES engine to disable
 *
 * @return NV_TRUE if successfully disabled the engine else NV_FALSE
 */
NvBool T30AesHwIsEngineDisabled(AesHwContext *pAesHw, AesHwEngine engine)
{
    NvBool IsEngineDisabled = NV_FALSE;

    NV_ASSERT(pAesHw);

    NvOsMutexLock(pAesHw->mutex[engine]);

    IsEngineDisabled = NvSecureServiceAesIsEngineDisabled(engine);

    NvOsMutexUnlock(pAesHw->mutex[engine]);

    return IsEngineDisabled;
}

/**
 * Disables read access to all key slots for the given engine.
 *
 * @param pAesHw Pointer to the AES H/W context
 * @param Engine Engine for which key reads needs to be disabled
 * @param NumSlotsSupported Number of key slots supported in the engine
 *
 * @retval None
 */
void
T30AesHwDisableAllKeyRead(
    AesHwContext *pAesHw,
    AesHwEngine engine,
    AesHwKeySlot NumSlotsSupported)
{
    NvU32 Slot;
    NvOsMutexLock(pAesHw->mutex[engine]);
    NvSecureServiceAesWaitTillEngineIdle(engine);

    // Disable read access to key slots
    for(Slot = 0; Slot < (NvU32)NumSlotsSupported; Slot++)
    {
        // DO NOT disable read access to slot-7. This is required
        // to read back the OTF key. Read access will be disabled after that.
        if (Slot != 7)
            NvSecureServiceAesKeyReadDisable(engine, Slot);
    }
    NvOsMutexUnlock(pAesHw->mutex[engine]);
}

/**
 * Compute key schedule for the given key, then loads key schedule and
 * initial vector into the specified key slot if both key and IV are given
 * If only key is given, then key schedule is computed and only the key
 * schdule is loaded
 * If only IV is given, then only the IV is loaded
 *
 * @param engine AES engine
 * @param slot key slot to load
 * @param pointer to the key
 * @param pointer to the iv
 * @param IsEncryption If set to NV_TRUE indicates key schedule computation
 *        is for encryption else for decryption.
 *
 * @retval None
 */
void
T30AesHwSetKeyAndIv(
    AesHwContext *pAesHw,
    AesHwEngine engine,
    AesHwKeySlot slot,
    AesHwKey *pKey,
    AesKeySize KeySize,
    AesHwIv *pIv,
    NvBool IsEncryption)
{
    NV_ASSERT(pAesHw);
    NvOsMutexLock(pAesHw->mutex[engine]);

    // Wait till engine becomes IDLE
    NvSecureServiceAesWaitTillEngineIdle(engine);

    if (pIv && !pKey)
    {
        // Set IV
        T30AesHwSetIv(pAesHw, engine, pIv, slot);
        NvOsMutexUnlock(pAesHw->mutex[engine]);
        return;
    }

    if (!pIv && pKey)
    {
        // Set key
        NvSecureServiceAesKeyReadDisable(engine, slot);
        NvSecureServiceAesControlKeyScheduleGeneration(engine, NV_FALSE);
        T30AesHwSetKey(pAesHw, engine, pKey, KeySize, slot);
        NvOsMutexUnlock(pAesHw->mutex[engine]);
        return;
    }

    NV_ASSERT(pKey && pIv);

    NvSecureServiceAesKeyReadDisable(engine, slot);
    NvSecureServiceAesControlKeyScheduleGeneration(engine, NV_FALSE);

    T30AesHwSelectKeyIvSlot(pAesHw, engine, slot, KeySize);
    //Clear key table first before expanding the Key
    NvOsMemset(
        pAesHw->pKeyTableVirAddr[engine],
        0,
        AES_HW_KEY_TABLE_LENGTH_BYTES);

    NvOsMemcpy(
        &pAesHw->pKeyTableVirAddr[engine][0],
        &pKey->key[0],
        sizeof(AesHwKey));

    NvOsMemcpy(
        &pAesHw->pKeyTableVirAddr[engine]
            [NvDdkAesConst_MaxKeyLengthBytes + NvDdkAesConst_IVLengthBytes],
        &pIv->iv[0],
        sizeof(AesHwIv));

    T30AesSetupTable_Int(pAesHw, engine, slot);
    NvSecureServiceAesSetKeySize(engine, KeySize);

    NvOsMutexUnlock(pAesHw->mutex[engine]);
}

/**
 * Load key into the specified AES engine for using it during
 * encryption or decryption.
 *
 * @param pAesHw Pointer to the AES H/W context
 * @param engine AES engine
 * @param pointer to the key
 * @param slot key slot for which key needs to be set
 *
 * @retval None
 */
void T30AesHwSetKey(
    AesHwContext *pAesHw,
    AesHwEngine engine,
    AesHwKey *pKey,
    AesKeySize KeySize,
    AesHwKeySlot slot)
{

    NV_ASSERT(pAesHw);
    NV_ASSERT(pKey);
    NV_ASSERT(pAesHw->pKeyTableVirAddr[engine]);

    T30AesHwSelectKeyIvSlot(pAesHw, engine, slot, KeySize);

    NvOsMemcpy(
        &pAesHw->pKeyTableVirAddr[engine][0],
        &pKey->key[0],
        sizeof(AesHwKey));

    NvSecureServiceAesSetKey(pAesHw->KeyTablePhyAddr[engine], engine, slot);
    NvSecureServiceAesSetKeySize(engine, KeySize);

    NvOsMemset((void *)pAesHw->pKeyTableVirAddr[engine],
        0,
        AES_HW_KEY_TABLE_LENGTH);
}

/**
 * Loads an initial vector into the specified AES engine for using it during
 * encryption or decryption.
 *
 * @param pAesHw Pointer to the AES H/W context
 * @param engine AES engine
 * @param pointer to the initial vector
 * @param slot key slot for which Iv needs to be set
 *
 * @retval None
 */
void T30AesHwSetIv(
    AesHwContext *pAesHw,
    AesHwEngine engine,
    AesHwIv *pIv,
    AesHwKeySlot slot)
{
    NV_ASSERT(pAesHw);
    NV_ASSERT(pIv);
    NV_ASSERT(pAesHw->pKeyTableVirAddr[engine]);

    NvOsMutexLock(pAesHw->mutex[engine]);
    T30AesHwSelectKeyIvSlot(pAesHw, engine, slot, AesKeySize_128Bit);

    NvOsMemcpy(
        (void *)&pAesHw->pKeyTableVirAddr[engine] [NvDdkAesConst_MaxKeyLengthBytes + NvDdkAesConst_IVLengthBytes],
        (NvU8 *)(&pIv->iv[0]),
        sizeof(AesHwIv));

    NvSecureServiceAesSetUpdatedIv(pAesHw->KeyTablePhyAddr[engine], engine, slot);

    NvOsMemset(
        (void *)&pAesHw->pKeyTableVirAddr[engine] [NvDdkAesConst_MaxKeyLengthBytes + NvDdkAesConst_IVLengthBytes],
        0,
        sizeof(AesHwIv));
    NvOsMutexUnlock(pAesHw->mutex[engine]);
}

/**
 * Decrypt the given data using Electronic Codebook (ECB)
 * mode into key slot
 *
 * @param pAesHw Pointer to the AES H/W context
 * @param engine AES engine to be used
 * @param DataSize Size of data in source buffer
 * @param src pointer to source buffer
 * @param DestSlotNum Destination key slot where the decrypted data is stored
 *
 * @retval NvSuccess if AES operation is successful
 * @retval NvError_BadParameter if data size is less than one AES block
 *        size or greater than 2 AES block size
 */
NvError
T30AesDecryptToSlot(
    AesHwContext *pAesHw,
    AesHwEngine engine,
    const NvU32 DataSize,
    const NvU8 *src,
    const AesHwKeySlot DestSlotNum)
{
    NvU32 NumBlocks;

    if ((DataSize < NvDdkAesKeySize_128Bit) ||
        (DataSize > NvDdkAesKeySize_256Bit))
        return NvError_BadParameter;

    NvOsMutexLock(pAesHw->mutex[engine]);
    NvSecureServiceAesWaitTillEngineIdle(engine);
    NvOsMemcpy(pAesHw->pDmaVirAddr[engine], (void *)src, DataSize);
    NvOsFlushWriteCombineBuffer();
    NumBlocks = (DataSize / NvDdkAesConst_BlockLengthBytes);
    NvSecureServiceAesDecryptToSlot(
        engine,
        pAesHw->DmaPhyAddr[engine],
        DestSlotNum,
        NumBlocks);
    NvOsFlushWriteCombineBuffer();

    NvOsMutexUnlock(pAesHw->mutex[engine]);
    return NvSuccess;
}

/**
 * Queries whether SSK update is allowed or not
 *
 * @param hRmDevice RM device handle
 *
 * @retval NV_TRUE if SSK update is allowed
 * @retval NV_FALSE if SSK update is not allowed
 */
NvBool T30AesHwIsSskUpdateAllowed(const NvRmDeviceHandle hRmDevice)
{
    return NV_TRUE;
}

void NvAesIntfT30GetHwInterface(AesHwInterface *pT30AesHw)
{
    AesHwEngine engine;
    AesHwKeySlot keySlot;

    pT30AesHw->AesHwDisableEngine = T30AesHwDisableEngine;
    pT30AesHw->AesHwClearKeyAndIv = T30AesHwClearKeyAndIv;
    pT30AesHw->AesHwClearIv = T30AesHwClearIv;
    pT30AesHw->AesHwSetKeyAndIv = T30AesHwSetKeyAndIv;
    pT30AesHw->AesHwSetIv = T30AesHwSetIv;
    pT30AesHw->AesHwGetIv = T30AesHwGetIv;
    pT30AesHw->AesHwLockSskReadWrites = T30AesHwLockSskReadWrites;
    pT30AesHw->AesHwSelectKeyIvSlot = T30AesHwSelectKeyIvSlot;
    pT30AesHw->AesHwStartEngine = T30AesHwStartEngine;
    pT30AesHw->AesHwLoadSskToSecureScratchAndLock =
        T30AesHwLoadSskToSecureScratchAndLock;
    pT30AesHw->AesGetUsedSlots = T30AesGetUsedSlots;
    pT30AesHw->AesHwIsEngineDisabled = T30AesHwIsEngineDisabled;
    pT30AesHw->AesHwDisableAllKeyRead = T30AesHwDisableAllKeyRead;
    pT30AesHw->AesHwIsSskUpdateAllowed = T30AesHwIsSskUpdateAllowed;
    pT30AesHw->AesHwDecryptToSlot = T30AesDecryptToSlot;

    // This would result in a wrong Iv if GetIv is called before call to
    // ProcessBuffer Iv for all key slots is not stored at any point of time.
    //  It's assumed Iv only for the key slotis queried.
    for ( engine = AesHwEngine_A; engine < AesHwEngine_Num; engine++)
    {
        if (!isEngineInitialized[engine])
        {
            for (keySlot = AesHwKeySlot_0;
                keySlot < AesHwKeySlot_NumExt;
                keySlot++)
            {
                NvOsMemset(gs_T30AesIv[engine].CurrentIv[keySlot],
                         0,
                         NvDdkAesConst_BlockLengthBytes);
            }
            gs_T30AesIv[engine].KeySlotInUse = AesHwKeySlot_Force32;
            isEngineInitialized[engine] = NV_TRUE;
        }
    }
    /**
     * Hardware key slot length is 64 bytes
     */

    g_AesHwKeySlotLengthBytes = 64;
}

