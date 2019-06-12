/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_nvddk_aes_H
#define INCLUDED_nvddk_aes_H


#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvddk_aes_common.h"

/** @file
 * @brief <b>NVIDIA Driver Development Kit:
 *           NvDdk AES Driver APIs</b>
 *
 * @b Description: Declares the interface for NvDdk
 *    Advanced Encryption Standard driver.
 */

/**
 * @defgroup nvddk_aes  AES Driver Interface
 *
 * This is the interface for NvDdk Advanced Encryption Standard driver.
 *
 * @ingroup nvddk_modules
 * @{
 */

/**
 *  Opaque handle to the AES engine.
 */

typedef struct NvDdkAesRec *NvDdkAesHandle;

/**
 * @brief Initializes and opens the AES engine.
 *
 * This function allocates the handle for the AES engine and provides it to the
 * client.
 *
 * @param InstanceId Instance of AES to be opened.
 * @param phAes Pointer to the location where the AES handle shall be stored.
 *
 * @retval NvSuccess Indicates that the AES has successfully opened.
 * @retval NvError_InsufficientMemory Indicates that function fails to allocate
 *         the memory.
 * @retval NvError_NotInitialized Indicates the AES initialization failed.
 */

 NvError NvDdkAesOpen(
    NvU32 InstanceId,
    NvDdkAesHandle * phAes );

/**
 * @brief Closes the AES engine.
 *
 * This function frees the memory allocated for the AES handle and
 * de-initializes the AES. This API never fails.
 *
 * @param hAes A handle from NvDdkAesOpen(). If \a hAes is NULL,
 * this API does nothing.
 */

 void NvDdkAesClose(
    NvDdkAesHandle hAes );

/**
 * @brief Selects the key for cipher operations.
 *
 * Select the key value for use in subsequent AES cipher operations. Caller can
 * select one of the pre-defined keys (SBK, SSK) or provide an explicit
 * key value.
 *
 * @note ::NvDdkAesSelectOperation must be called before calling
 *    this function.
 *
 * @param hAes Handle to the AES.
 * @param pKeyInfo A pointer to key attribute.
 *
 * @retval NvError_Success Key operation is successful.
 * @retval NvError_BadParameter Key type or length is invalid.
 * @retval NvError_InvalidState If AES engine is disabled, or operating mode is
 *                              not set yet.
 */

 NvError NvDdkAesSelectKey(
    NvDdkAesHandle hAes,
    const NvDdkAesKeyInfo * pKeyInfo );

/**
 * @brief Selects and set the  key for cipher operations.
 *
 * Selects and set the explicit SBK key value for use in subsequent AES cipher operations
 *
 *
 * @param hAes Handle to the AES.
 * @param pKeyInfo A pointer to key attribute.
 *
 * @retval NvError_Success Key operation is successful.
 * @retval NvError_BadParameter Key type or length is invalid.
 * @retval NvError_InvalidState If AES engine is disabled, or operating mode is
 *                              not set yet.
 */

 NvError NvDdkAesSelectAndSetSbk(
    NvDdkAesHandle hAes,
    const NvDdkAesKeyInfo * pKeyInfo );

/**
 * @brief Selects the cipher operation to perform.
 *
 * @param hAes Handle to the AES.
 * @param pOperation A pointer to operation attribute.
 *
 * @retval NvError_Success Cipher operation is successful.
 * @retval NvError_BadParameter Invalid operand value.
 * @retval NvError_InvalidState If AES engine is disabled.
 */

 NvError NvDdkAesSelectOperation(
    NvDdkAesHandle hAes,
    const NvDdkAesOperation * pOperation );

/**
 * @brief Sets the initial vector (IV) for cipher operation.
 *
 * @param hAes Handle to the AES.
 * @param pInitialVector A pointer to initial vector attribute.
 * @param VectorSize Size of vector in bytes.
 *
 * @retval NvError_Success Initial operation is successful.
 * @retval NvError_InvalidState If AES engine is disabled.
 */

 NvError NvDdkAesSetInitialVector(
    NvDdkAesHandle hAes,
    const NvU8 * pInitialVector,
    NvU32 VectorSize );

/**
 * @brief Gets the initial vector (IV) for cipher operation.
 *
 * @param hAes Handle to the AES.
 * @param VectorSize Size of buffer pointed to by \a pInitialVector (in bytes).
 * @param pInitialVector A pointer to initial vector attribute.
 *
 * @retval NvError_Success Initial operation is successful.
 * @retval NvError_InvalidState If AES engine is disabled.
 */

 NvError NvDdkAesGetInitialVector(
    NvDdkAesHandle hAes,
    NvU32 VectorSize,
    NvU8 * pInitialVector );

/**
 * @brief Processes the specified source data buffer using selected operation,
 * cipher key, and initial vector.
 *
 * Stores processed data in the destination buffer. The buffers pointed to by
 * \a SrcBufferSize and \a DestBufferSize parameters must be a multiple of
 * ::NvDdkAesConst_BlockLengthBytes, and the buffers pointed to by \a
 * pSrcBuffer and \a pDestBuffer must be the same size.
 *
 * @note ::NvDdkAesSelectOperation must be called before calling
 * this function.
 *
 * @param hAes Handle to the AES.
 * @param SrcBufferSize Size of src buffer in bytes. \a SrcBufferSize
 *        and \a DestBufferSize must be a multiple of
 *        ::NvDdkAesConst_BlockLengthBytes.
 * @param DestBufferSize Size of dest buffer in bytes. \a SrcBufferSize
 *        and \a DestBufferSize must be a multiple of
 *        ::NvDdkAesConst_BlockLengthBytes.
 * @param pSrcBuffer A pointer to src buffer. \a pSrcBuffer and
 *        \a pDestBuffer must be the same size.
 * @param pDestBuffer A pointer to dest buffer. \a pSrcBuffer and
 *        \a pDestBuffer must be the same size.
 *
 * @retval NvError_Success Crypto operation is successful.
 * @retval NvError_InvalidSize \a BufferSize is not a multiple of
 *                             ::NvDdkAesConst_BlockLengthBytes.
 * @retval NvError_InvalidAddress Illegal buffer pointer (NULL).
 * @retval NvError_BadValue Other illegal parameter value.
 * @retval NvError_InvalidState Key and/or operating mode not set yet.
 * @retval NvError_InvalidState If AES engine is disabled.
 */

 NvError NvDdkAesProcessBuffer(
    NvDdkAesHandle hAes,
    NvU32 SrcBufferSize,
    NvU32 DestBufferSize,
    const NvU8 * pSrcBuffer,
    NvU8 * pDestBuffer );

/**
 * @brief Gets hardware capabilities for cipher operations.
 *
 * @param hAes Handle to the AES.
 * @param pCapabilities A pointer to capabilities attribute.
 *
 * @retval NvError_Success Capabilities query is successful.
 * @retval NvError_InvalidState If AES engine is disabled.
 */

 NvError NvDdkAesGetCapabilities(
    NvDdkAesHandle hAes,
    NvDdkAesCapabilities * pCapabilities );

/**
 * @brief Overwrites Secure Boot Key (SBK) in AES key slot with zeroes.
 *
 * After this operation has been completed, the SBK value will no longer be
 * accessible for any operations until after the system is rebooted. Read
 * access to the AES key slot containing the SBK is always disabled.
 *
 * @param hAes Handle to the AES.
 *
 * @retval NvError_Success SBK cleared successfully.
 * @retval NvError_InvalidState If AES engine is disabled.
 */

 NvError NvDdkAesClearSecureBootKey(
    NvDdkAesHandle hAes );

/**
 * @brief Disables write access to AES key slot containing the Secure Storage
 * Key (SSK).
 *
 * Prior to this operation, write access is enabled to the AES key slot
 * containing the SSK. This allows the OEM to override the default SSK
 * value.
 *
 * After this operation has been completed, the SSK value will still remain
 * available for encrypt and decrypt operations, but read access to the key
 * will be disabled (until the system is rebooted).
 *
 * @note Write access to the key slot containing the SSK is always disabled.
 *
 * @param hAes Handle to the AES.
 *
 * @retval NvError_Success SSK locked successfully.
 * @retval NvError_InvalidState If AES engine is disabled.
 */

 NvError NvDdkAesLockSecureStorageKey(
    NvDdkAesHandle hAes );

/**
 * @brief Overrides Secure Storage Key (SSK) value in AES key slot, then
 * disables write access to the key slot.
 *
 * @warning A03 versions of the silicon employ a software workaround as
 * documented in "NVIDIA® TEGRA™ 2 Revision A03 LP0 Exit Issue Summary"
 * technical note. This workaround has a side effect whereby the SSK
 * cannot be overridden. On A03 silicon, NvDdkAesSetAndLockSecureStorageKey()
 * will always fail and report an error.
 *
 * After this operation has been completed, the SSK value will still remain
 * available for encrypt and decrypt operations, but read access to the key
 * will be disabled (until the system is rebooted).
 *
 * @note Write access to the key slot containing the SSK is always disabled.
 *
 * @param hAes Handle to the AES.
 * @param KeyLength Length of key in bytes.
 * @param pSecureStorageKey A pointer to key argument.
 *
 * @retval NvError_Success SSK overridden and locked successfully.
 * @retval NvError_InvalidState If AES engine is disabled.
 */

 NvError NvDdkAesSetAndLockSecureStorageKey(
    NvDdkAesHandle hAes,
    NvDdkAesKeySize KeyLength,
    const NvU8 * pSecureStorageKey );

/**
 * @brief Disables all AES operations until the system is rebooted.
 *
 * @param hAes Handle to the AES.
 *
 * @retval NvError_Success AES operations disabled successfully.
 * @retval NvError_InvalidState If AES engine is already disabled.
 */

 NvError NvDdkAesDisableCrypto(
    NvDdkAesHandle hAes );

#if defined(__cplusplus)
}
#endif

#endif
