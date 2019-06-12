/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef INCLUDED_NVDDK_SE_HW_H
#define INCLUDED_NVDDK_SE_HW_H

#include "nvddk_se_core.h"

#if defined(__cplusplus)
extern "C"
{
#endif

// Macro definitions //
// SE register read macro
#define SE_REGR(hSE, reg) \
     NV_READ32(hSE->pVirtualAddress + ((SE_##reg##_0) / 4))

// SE register write macro
#define SE_REGW(hSE, reg, data) \
do \
{ \
    NV_WRITE32(hSE->pVirtualAddress + ((SE_##reg##_0) / 4), (data)); \
}while (0)

#define RSA_OUT_REGR(hSE, Num) \
    NV_READ32(hSE->pVirtualAddress + (((SE_RSA_OUTPUT_0) / 4) + Num));

#define SE_OP_MAX_TIME (1000)

/**
 * Disables the Key Schedule read
 *
 * @retval NvSuccess Key schedule read is disabled.
 * @retval NvError_NotInitialized Context is not initialized.
 */
NvError SeKeySchedReadDisable(void);

/**
 * Clears all h/w interrupts.
 */
void  SeClearInterrupts(void);

/**
 * Processes the Buffers specified in SHA context
 *
 * @param hSeShaHwCtx SHA module context
 *
 * @retval NvSuccess Buffer have been successfully processed.
 * @retval NvError_NotInitialized Context is not initialized.
 */
NvError SeSHAProcessBuffer(NvDdkSeShaHWCtx* const hSeShaHwCtx);

/**
 * Takes backup of MSGLEFT and MSGLENGTH register contents
 *
 * @param hSeShaHwCtx SHA module context
 *
 * @retval NvSuccess Backup is done without any error.
 * @retval NvError_NotInitialized Context is not initialized.
 */
NvError SeSHABackupRegisters(NvDdkSeShaHWCtx* const hSeShaHwCtx);

/**
 * Waits for the operation to complete
 *
 * @param args module context when called by Interrupt handler
 *
 */
void SeIsr(void* args);

/**
 * Sets the Aes key in the keyslot specified.
 *
 * @param hSeAesHwCtx Aes Module Context
 *
 * @retval NvSuccess Buffer has been successfully processed
 * @retval NvError_NotInitialized Context is not initialized.
 */
NvError SeAesSetKey(NvDdkSeAesHWCtx *hSeAesHwCtx);

/**
 * Processes the buffers specified in Aes Context
 *
 * @param hSeAesHwCtx AES module Context
 *
 * @retval NvSuccess Buffers have been successfully processed.
 * @retval NvError_NotInitialized Context is not initialized.
 */
NvError SeAesProcessBuffer(NvDdkSeAesHWCtx *hSeAesHwCtx);

/**
 * Processes the buffers specified in Aes Context
 *
 * @param hSeAesHwCtx AES module Context
 * @retval NvSuccess Buffers have been successfully processed
 * @retval NvError_NotInitialized Context is not initialized.
 */
NvError SeAesCMACProcessBuffer(NvDdkSeAesHWCtx *hSeAesHwCtx);

/**
 * Processes the buffers specified in Aes Context
 *
 * @param hSeAesHwCtx AES module Context
 * @retval NvSuccess Buffers have been successfully processed
 * @retval NvError_NotInitialized Context is not initialized.
 */
NvError CollectCMAC(NvU32 *pBuffer);

/**
 * Write locks the key in specified key slot
 *
 * @param KeySlotIndex to write lock particular key in the slot
 */
void SeAesWriteLockKeySlot(SeAesHwKeySlot KeySlotIndex);

/**
 * Read locks the key in the specified key slot
 *
 * @param KeySlotIndex to Read lock particular key in the slot
 */
void SeAesReadLockKeySlot(NvU32 KeySlotIndex);
/**
 * Writes the Iv in the specified key slot
 *
 * @param hSeAesHwCtx AES module Context
 */
void SeAesSetIv(NvDdkSeAesHWCtx *hSeAesHwCtx);

 /**
  * Read the Iv in the specified key slot
  *
  * @param hSeAesHwCtx AES module Context
  */
 void SeAesGetIv(NvDdkSeAesHWCtx *hSeAesHwCtx);

 /* Installs the RSA Key into the respective key slot
 *
 * @param hSeRsaHwCtx RSA module context
 *
 * @retval NvSuccess Key is successfully installed to keyslot.
 * @retval NvError_NotInitialized Context is not initialized.
 */
NvError SeRSASetKey(NvDdkSeRsaHWCtx* const hSeRsaHwCtx);

/**
 * Evaluates modular exponentiation operation
 *
 * @param hSeRsaHwCtx RSA module context
 *
 * @retval NvSuccess Modular exponentiation operation is done
 * @retval NvError_NotInitialized Context is not initialized.
 */
NvError SeRSAProcessBuffer(NvDdkSeRsaHWCtx* const hSeRsaHwCtx);

/**
 * Collects the data into Buffer after
 *
 * Modular Exponentiation operation.
 *
 * @params Buffer pointer to the buffer
 * @params Size size of the buffer
 * @params DestIsMemory says if the destination is memory or registers
 *
 * @retval NvSuccess collection of Output is done
 * @retval Suitable  Error code for failure.
 */
NvError SeRsaCollectOutputData(NvU32 *Buffer, NvU32 Size, NvBool DestIsMemory);

/* Minumum Random number size(It generates in terms of 16 block units) */
#define SE_RNG_OUT_SIZE (16)

/**
 * Generate random number of the size specifed.
 *
 * @params hSeRngHwCtx Rng H/w context.
 *
 * @retval NvSuccess if generating random number is success.
 * @retval NvError_NotInitialized if the context is not initalized.
 */
NvError SeGenerateRandomNumber(NvDdkSeRngHWCtx *hSeRngHwCtx);
#if defined(__cplusplus)
}
#endif

#endif
