/*
 * Copyright (c) 2011-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * @brief <b>NVIDIA Driver Development Kit:
 *           AES encryption/Decryption driver </b>
 *
 * @b Description: Interface file for NvDdk AES driver.
 *
 */

#ifndef INCLUDED_NVDDK_AES_PRIV_H
#define INCLUDED_NVDDK_AES_PRIV_H

#include "nverror.h"
#include "nvcommon.h"
#include "nvrm_memmgr.h"
#include "nvddk_aes_hw.h"

#if defined(__cplusplus)
extern "C" {
#endif

// As of now only 5 commands are USED for AES encryption/Decryption
#define AES_HW_MAX_ICQ_LENGTH 5

// AES RFC 3394 initial vector length, in bytes.
#define AES_RFC_IV_LENGTH_BYTES 8

/**
 * AES Engine instances.
 */
typedef enum
{
    // AES Engine "A" BSEV
    AesHwEngine_A,
    // AES Engine "B" BSEA
    AesHwEngine_B,
    AesHwEngine_Num,
    AesHwEngine_Force32 = 0x7FFFFFFF
} AesHwEngine;

/**
 * AES Key Slot instances (per AES engine).
 */
typedef enum
{
    // AES Key Slot "0" this is insecure slot
    AesHwKeySlot_0,
    // AES Key Slot "1" this is insecure slot
    AesHwKeySlot_1,
    // AES Key Slot "2" this is secure slot
    AesHwKeySlot_2,
    // AES Key Slot "3" this is secure slot
    AesHwKeySlot_3,
    // Total number of slots
    AesHwKeySlot_Num,
    // AES Key Slot "4" this is insecure slot
    AesHwKeySlot_4 = AesHwKeySlot_Num,
    // AES Key Slot "5" this is insecure slot
    AesHwKeySlot_5,
    // AES Key Slot "6" this is secure slot
    AesHwKeySlot_6,
    // AES Key Slot "7" this is secure slot
    AesHwKeySlot_7,
    // Total number of slots in extended AES engine
    AesHwKeySlot_NumExt,
    AesHwKeySlot_Force32 = 0x7FFFFFFF
} AesHwKeySlot;

// H/W Context
typedef struct AesHwContextRec
{
    // RM device handle
    NvRmDeviceHandle hRmDevice;
    // Controller registers physical base address
    NvRmPhysAddr PhysAdr[AesHwEngine_Num];
    // Holds the virtual address for accessing registers.
    NvU32 *pVirAdr[AesHwEngine_Num];
    // Holds the register map size.
    NvU32 BankSize[AesHwEngine_Num];
    // Memory handle to the key table
    NvRmMemHandle  hKeyTableMemBuf;
    // Pointer to the key table virtual buffer
    NvU8 *pKeyTableVirAddr[AesHwEngine_Num];
    // Key table physical addr
    NvRmPhysAddr KeyTablePhyAddr[AesHwEngine_Num];
    // Memory handle to the Aes h/w Buffer
    NvRmMemHandle hDmaMemBuf;
    // Contains the physical Address of the h/w buffer
    NvRmPhysAddr DmaPhyAddr[AesHwEngine_Num];
    // virtual pointer to the Aes buffer
    NvU8 *pDmaVirAddr[AesHwEngine_Num];
    // Icq commands length
    NvU32 CommandQueueLength[AesHwEngine_Num];
    // Holds the Icq commands for the AES operation
    NvU32 CommandQueueData[AesHwEngine_Num][AES_HW_MAX_ICQ_LENGTH];
    // mutex to support concurrent operation on the AES engine
    NvOsMutexHandle mutex[AesHwEngine_Num];
} AesHwContext;

/* AES Core Engine record */
typedef struct AesCoreEngineRec
{
    // Keps the count of open handles
    NvU32 OpenCount;
    // Id returned from driver's registration with Power Manager
    NvU32 RmPwrClientId[AesHwEngine_Num];
    // Indicates whether key slot is used or not
    NvBool IsKeySlotUsed[AesHwEngine_Num][AesHwKeySlot_NumExt];
    // SSK engine where SSK is stored
    AesHwEngine SskEngine[AesHwEngine_Num];
    // SSK encrypt key slot
    AesHwKeySlot SskEncryptSlot;
    // SSK Decrypt key slot
    AesHwKeySlot SskDecryptSlot;
    // SBK engine where SBK is stored
    AesHwEngine SbkEngine[AesHwEngine_Num];
    // SBK encrypt key slot
    AesHwKeySlot SbkEncryptSlot;
    // SSK Decrypt key slot
    AesHwKeySlot SbkDecryptSlot;
    // Aes H/W Engine context
    AesHwContext AesHwCtxt;
    // Indicates engine is disabled or not
    NvBool IsEngineDisabled;
    // Indicates whether SSK update is allowed or not
    NvBool SskUpdateAllowed;
} AesCoreEngine;

/* Set of function pointers to be used to access the hardware
 * interface for the AES engines.
*/
typedef struct AesHwInterfaceRec
{

    /**
     * Disables the selected AES engine.  No further operations can be performed
     * using the AES engine until the entire chip is reset.
     *
     * @param pAesHw Pointer to the AES H/W context
     * @param engine which AES engine to disable
     *
     * @retval NV_TRUE if successfully disabled the engine else NV_FALSE
     */
    NvBool (*AesHwDisableEngine)(
        AesHwContext *pAesHw,
        AesHwEngine engine);

    /**
     * Over-writes the key schedule and Initial Vector in the in the specified
     * key slot with zeroes.
     * Convenient for preventing subsequent callers from gaining access
     * to a previously-used key.
     *
     * @param pAesHw Pointer to the AES H/W context
     * @param engine AES engine
     * @param slot key slot to clear
     *
     * @retval None
     */
    void (*AesHwClearKeyAndIv)(
        AesHwContext *pAesHw,
        AesHwEngine engine,
        AesHwKeySlot slot);

    /**
     * Over-writes the initial vector in the specified AES engine with zeroes.
     * Convenient to prevent subsequent callers from gaining access to a
     * previously-used initial vector.
     *
     * @param pAesHw Pointer to the AES H/W context
     * @param engine AES engine
     * @param slot key slot for which Iv needs to be cleared
     *
     * @retval None
     */
    void (*AesHwClearIv)(
        AesHwContext *pAesHw,
        AesHwEngine engine,
        AesHwKeySlot /*slot*/);

    /**
     * Computes key schedule for the given key, then loads key schedule and
     * initial vector into the specified key slot.
     *
     * @param pAesHw Pointer to the AES H/W context
     * @param engine AES engine
     * @param slot key slot to load
     * @param pointer to the key
     * @param KeySize key length specified in bytes
     * @param pointer to the iv
     * @param IsEncryption If set to NV_TRUE indicates key schedule computation
     *        id for encryption else for decryption.
     *
     * @retval None
     */
    void (*AesHwSetKeyAndIv)(
        AesHwContext *pAesHw,
        AesHwEngine engine,
        AesHwKeySlot slot,
        AesHwKey *pKey,
        AesKeySize KeySize,
        AesHwIv *pIv,
        NvBool IsEncryption);

    /**
     * Loads an initial vector into the specified AES engine for using it
     * during encryption or decryption.
     *
     * @param pAesHw Pointer to the AES H/W context
     * @param engine AES engine
     * @param pointer to the initial vector
     * @param slot key slot for which Iv needs to be set
     *
     * @retval None
     */
    void (*AesHwSetIv)(
        AesHwContext *pAesHw,
        AesHwEngine engine,
        AesHwIv *pIv,
        AesHwKeySlot slot);

    /**
    * Retrieves the initial vector for the specified AES engine. This will get the
    * IV that is stored in the specified engine registers. To get the original Iv
    * stored in the key table slot, AesHwSelectKeyIvSlot() API must be called
    * before calling AesHwGetIv().
    *
    * NOTE:
    * For Ap20, the currnet Iv is preserved within the driver because updated
    * Iv is locked down for dedicated key slots against reads.
    * Since the current Iv has no valid value/use/relevance before an encryption
    * /decryption operation is performed, the current Iv for any key slot
    * shall be returned as all zeroes in case no encryption/decryption
    * operation has been performed with the concerned key slot within the
    * engine.
    *
    * @param engine AES engine
    * @param pointer to the initial vector
    * @param slot key slot for which Iv is to be retrieved
    *

    * @retval None
    */
     void (*AesHwGetIv)(
        AesHwContext *pAesHw,
        AesHwEngine engine,
        AesHwIv *pIv,
        AesHwKeySlot /*slot*/);

    /**
     * Locks the Secure Session Key (SSK) slots.
     * This API disables the read/write permissions to the secure key slots.
     *
     * @param pAesHw Pointer to the AES H/W context
     * @param SskEngine SSK engine number
     *
     * @retval None
     */
     void (*AesHwLockSskReadWrites)(
        AesHwContext *pAesHw,
        AesHwEngine SskEngine);

    /**
     * Selects the key and iv from the internal key table for a specified key slot.
     *
     * @param pAesHw Pointer to the AES H/W context
     * @param engine AES engine
     * @param slot key slot for which key and IV needs to be selected
     * @param KeySize key length specified in bytes
     *
     * @retval None
     */
     void (*AesHwSelectKeyIvSlot)(
        AesHwContext *pAesHw,
        AesHwEngine engine,
        AesHwKeySlot slot,
        AesKeySize KeySize);

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
     NvError (*AesHwStartEngine)(
        AesHwContext *pAesHw,
        AesHwEngine engine,
        NvU32 DataSize,
        const NvU8 *src,
        NvU8 *dst,
        NvBool IsEncryption,
        NvDdkAesOperationalMode OpMode);

    /**
     * Loads the SSK key into secure scratch resgister and disables the write permissions.
     * Note: If Key is not specified then this API locks the Secure Scratch registers.
     *
     * @param PmicBaseAddr PMIC base address
     * @param pKey pointer to the key. If pKey=NULL then key will not be set to the
     * secure scratch registers, but locks the Secure scratch register.
     * @param Size length of the aperture in bytes
     *
     * @retval None
     */
     void (*AesHwLoadSskToSecureScratchAndLock)(
        NvRmPhysAddr PmicBaseAddr,
        AesHwKey *pKey,
        size_t Size);

    /**
     * Marks all dedicated slots as used
     *
     * @param pAesCoreEngine Pointer to Aes Core Engine handle.
     *
     * @retval None
     *
     */
    void (*AesGetUsedSlots)(AesCoreEngine *pAesCoreEngine);

    /**
     * Reads the AES engine disable status.
     *
     * @param pAesHw Pointer to the AES H/W context
     * @param engine which AES engine to disable
     *
     * @return NV_TRUE if successfully disabled the engine else NV_FALSE
     */
    NvBool (*AesHwIsEngineDisabled)(AesHwContext *pAesHw, AesHwEngine engine);

    /**
     * Disables read access to all key slots for the given engine.
     *
     * @param pAesHw Pointer to the AES H/W context
     * @param Engine Engine for which key reads needs to be disabled
     * @param NumSlotsSupported Number of key slots supported in the engine
     *
     * @retval None
     */
    void (*AesHwDisableAllKeyRead)(
        AesHwContext *pAesHw,
        AesHwEngine engine,
        AesHwKeySlot NumSlotsSupported);

    /**
     * Queries whether SSK update is allowed or not
     *
     * @param hRmDevice RM device handle
     *
     * @retval NV_TRUE if SSK update is allowed
     * @retval NV_FALSE if SSK update is not allowed
     */
    NvBool (*AesHwIsSskUpdateAllowed)(const NvRmDeviceHandle hRmDevice);

    /**
     * Decrypt the given data using Electronic Codebook (ECB)
     * mode into key slot
     *
     * @param pAesHw Pointer to the AES H/W context
     * @param engine AES engine to be used
     * @param DataSize Size of data in source buffer
     *        It should be less than 32 bytes
     * @param src pointer to source buffer
     * @param DestSlotNum Destination key slot where the decrypted data is stored
     *
     * @retval NvSuccess if AES operation is successful
     * @retval NvError_BadParameter if data size is less than one AES block
     *         size or greater than 2 AES block size
    */
     NvError (*AesHwDecryptToSlot)(
        AesHwContext *pAesHw,
        AesHwEngine engine,
        const NvU32 DataSize,
        const NvU8 *src,
        const AesHwKeySlot DestSlotNum);

}AesHwInterface;

// AES engine capabilities
typedef struct AesHwCapabilitiesRec
{
    // Number of slots supported in each AES instance
    AesHwKeySlot NumSlotsSupported;
    // Key schedule generation in hardware to be supported.
    NvBool IsHwKeySchedGenSupported;
    // Hashing to be supported within the engine.
    NvBool isHashSupported;
    // Minimum Alignment for the key table/schedule buffer in IRAM or VRAM
    NvU32 MinKeyTableAlignment;
    // Min Input/Output buffer alignment for AES.
    NvU32 MinBufferAlignment;
    // Pointer to the AES engine low level  interface
    AesHwInterface *pAesInterf;
} AesHwCapabilities;

// AES client state: this structure is common to all clients.
typedef struct NvDdkAesRec
{
    // Algorithm type set for this client
    NvDdkAesOperationalMode OpMode;
    // Select key type -- SBK, SSK, User-specified, etc.
    NvDdkAesKeyType KeyType;
    // Aes key length; must be 128Bit for SBK or SSK
    NvDdkAesKeySize KeySize;
    // Specified Aes key value is KeyType if UserSpecified; else ignored
    NvU8 Key[NvDdkAesConst_MaxKeyLengthBytes];
    // Initial vector to use when encrypting/decrypting last IV will be stored
    NvU8 Iv[NvDdkAesConst_IVLengthBytes];
    // Initial vector to use in RFC3394 key unwrapping
    NvU8 WrappedIv[AES_RFC_IV_LENGTH_BYTES];
    // client selected AES engine, depends on the key slot selected
    AesHwEngine Engine;
    // client selected Key slot
    AesHwKeySlot KeySlot;
    // operation type selected
    NvBool IsEncryption;
    // If user key is specified client can tell to use the dedicated slot
    // If set to NV_TRUE then key slot is dedicated else shared with other keys
    NvBool IsDedicatedSlot;
    /// pointer to the AES core engine
    AesCoreEngine *pAesCoreEngine;
} NvDdkAes;

/**
 * Returns the interface to be used for T30 AES engine.
 *
 * @param pT30AesHw Pointer to the interface
 *
 * @retval None
 */
void NvAesIntfT30GetHwInterface(AesHwInterface *pT30AesHw);

#if !ENABLE_SECURITY

/**
 * Get address of the engine.
 *
 * @param Engine engine on which encryption and decryption need to be performed
 *
 * @retval Base address.
 */
NvU32 NvAesCoreGetEngineAddress(NvU32 Engine);

#endif // !ENABLE_SECURITY

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVDDK_AES_PRIV_H
