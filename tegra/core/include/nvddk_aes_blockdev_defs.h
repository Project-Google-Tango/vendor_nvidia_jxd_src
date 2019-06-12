/*
 * Copyright (c) 2008-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b> NVIDIA Driver Development Kit: AES Block Driver Definitions</b>
 *
 * @b Description: This file defines the public data structures for the
 *                 AES block driver.
 */

#ifndef INCLUDED_NVDDK_AES_BLOCKDEV_DEFS_H
#define INCLUDED_NVDDK_AES_BLOCKDEV_DEFS_H

#include "nvcommon.h"
#include "nverror.h"
#include "nvddk_blockdev_defs.h"
#include "nvddk_aes_common.h"

#if defined(__cplusplus)
extern "C"
{
#endif
/**
 * @defgroup nvbdk_ddk_aesblkdrvdef AES Block Driver Definitions
 *
 * This defines the public data structures for the AES block driver.
 *
 * @ingroup nvbdk_ddk_aes
 * @{
 */

/**
 * Enum defining AES-specific IOCTL types.
 *
 * @note Device-specific IOCTLs start after standard IOCTLs.
 */
typedef enum
{
    NvDdkAesBlockDevIoctlType_Invalid = NvDdkBlockDevIoctlType_Num,

    /**
     * Select key for cipher operations.
     *
     * Select key value for use in subsequent AES cipher operations. Caller can
     * select one of the pre-defined keys (SBK, SSK, zeroes) or provide an explicit
     * key value.
     *
     * The initial value vector is set to zeroes, and the CTR counter value is set
     * to zeroes.
     *
     * @note ::NvDdkAesBlockDevIoctlType_SelectOperation must be called before
     * calling ::NvDdkAesBlockDevIoctlType_SelectKey IOCTL.
     *
     * @par Inputs:
     * ::NvDdkAesBlockDevIoctl_SelectKeyInputArgs
     *
     * @par Outputs:
     * None.
     *
     * @retval NvError_Success Key selection is successful.
     * @retval NvError_BadParameter Key type or length is invalid.
     * @retval NvError_InvalidState if AES engine is disabled / Operating mode not set yet.
     */
    NvDdkAesBlockDevIoctlType_SelectKey,

     /**
     * Selects the key for cipher operations.
     *
     * Selects the key value for use in subsequent AES cipher operations. The
     * caller can set the provided explicit SBK value.
     *
     * The initial value vector is set to zeroes, and the CTR counter value is set
     * to zeroes.
     *
     * @note ::NvDdkAesBlockDevIoctlType_SelectOperation must be called before
     * calling \c NvDdkAesBlockDevIoctlType_SelectAndSetSbk IOCTL.
     *
     * @par Inputs:
     * ::NvDdkAesBlockDevIoctl_SelectKeyInputArgs
     *
     * @par Outputs:
     * None.
     *
     * @retval NvError_Success Key selection is successful.
     * @retval NvError_BadParameter Key type or length is invalid.
     * @retval NvError_InvalidState If AES engine is disabled or operating mode not set yet.
     */
    NvDdkAesBlockDevIoctlType_SelectAndSetSbk,
    
    /**
     * Get hardware capabilities for cipher operations.
     *
     * @par Inputs:
     * None.
     *
     * @par Outputs:
     * ::NvDdkAesBlockDevIoctl_GetCapabilitiesOutputArgs
     *
     * @retval NvError_Success Capabilities query is successful.
     * @retval NvError_InvalidState if AES engine is disabled.
     */
    NvDdkAesBlockDevIoctlType_GetCapabilities,

    /**
     * Set initial vector (IV) for cipher operation.
     *
     * @par Inputs:
     *  ::NvDdkAesBlockDevIoctl_SetInitialVectorInputArgs
     *
     * @par Outputs:
     * None.
     *
     * @retval NvError_Success Initial vector set successfully.
     * @retval NvError_InvalidState if AES engine is disabled.
     */
    NvDdkAesBlockDevIoctlType_SetInitialVector,

    /**
     * Get current value of initial vector (IV).
     *
     * @par Inputs:
     * None.
     *
     * @par Outputs:
     * ::NvDdkAesBlockDevIoctl_GetInitialVectorOutputArgs
     *
     * @retval NvError_Success Initial vector fetched successfully.
     * @retval NvError_InvalidState if AES engine is disabled.
     */
    NvDdkAesBlockDevIoctlType_GetInitialVector,

    /**
     * Select cipher operation to perform.
     *
     * @par Inputs:
     * ::NvDdkAesBlockDevIoctl_SelectOperationInputArgs
     *
     * @par Outputs:
     * None.
     *
     * @retval NvError_Success Cipher operation selection is successful.
     * @retval NvError_BadParameter Invalid operand value.
     * @retval NvError_InvalidState if AES engine is disabled.
     */
    NvDdkAesBlockDevIoctlType_SelectOperation,
    
    /**
     * Process specified source data buffer using selected operation, cipher key,
     * and initial vector.
     *
     * Store processed data in destination buffer. Destination data size is the
     * same as source data size.
     *
     * For optimal performance, source and destination buffers must meet the
     * alignment constraints reported by the
     * ::NvDdkAesBlockDevIoctl_GetCapabilitiesOutputArgs IOCTL.
     *
     * @note ::NvDdkAesBlockDevIoctlType_SelectOperation must be called before
     * calling ::NvDdkAesBlockDevIoctlType_ProcessBuffer IOCTL.
     *
     * @par Inputs:
     * ::NvDdkAesBlockDevIoctl_ProcessBufferInputArgs
     *
     * @par Outputs:
     * None.
     *
     * @retval NvError_Success Crypto operation completed successfully.
     * @retval NvError_InvalidSize BufferSize or SkipSize is illegal.
     * @retval NvError_InvalidAddress Illegal buffer pointer (NULL).
     * @retval NvError_BadValue Other illegal parameter value.
     * @retval NvError_InvalidState Key and/or operating mode not set yet.
     * @retval NvError_InvalidState if AES engine is disabled.
     */
    NvDdkAesBlockDevIoctlType_ProcessBuffer,

    /**
     * Overwrite Secure Boot Key (SBK) in AES key slot with zeroes.
     *
     * After this operation has been completed, the SBK value will no longer be
     * accessible for any operations until after the system is rebooted. Read
     * access to the AES key slot containing the SBK is always disabled.
     *
     * @warning To minimize the risk of compromising the SBK, this routine
     * should be called as early as practical in the boot loader code. The key
     * purpose of the SBK is to allow a new boot loader to be installed on the
     * system. Thus, as soon as a new boot loader has been installed, or as
     * soon as it has been determined that no new boot loader needs to be
     * installed, the SBK should be cleared.
     *
     * @par Inputs:
     * None.
     *
     * @par Outputs:
     * None.
     *
     * @retval NvError_Success Secure Boot Key cleared successfully.
     * @retval NvError_InvalidState if AES engine is disabled.
     */
    NvDdkAesBlockDevIoctlType_ClearSecureBootKey,

    /**
     * Disable write access to AES key slot containing the Secure Storage Key
     * (SSK).
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
     * @warning To minimize the risk of compromising the SSK, this routine
     * should be called as early as practical in the boot loader code.
     *
     * @par Inputs:
     * None.
     *
     * @par Outputs:
     * None.
     *
     * @retval NvError_Success Secure Storage Key locked successfully.
     * @retval NvError_InvalidState if AES engine is disabled.
     */
    NvDdkAesBlockDevIoctlType_LockSecureStorageKey,

    /**
     * Override Secure Storage Key (SSK) value in AES key slot, then disable
     * write access to the key slot.
     *
     * After this operation has been completed, the SSK value will still remain
     * available for encrypt and decrypt operations, but read access to the key
     * will be disabled (until the system is rebooted).
     *
     * @note Write access to the key slot containing the SSK is always disabled.
     *
     * @warning To minimize the risk of compromising the SSK, this routine
     * should be called as early as practical in the bootInvalidState loader code.
     *
     * @par Inputs:
     * ::NvDdkAesBlockDevIoctl_SetAndLockSecureStorageKeyInputArgs
     *
     * @par Outputs:
     * None.
     *
     * @retval NvError_Success Secure Storage Key overridden and locked
     *         successfully.
     * @retval NvError_InvalidState if AES engine is disabled.
     */
    NvDdkAesBlockDevIoctlType_SetAndLockSecureStorageKey,

    /**
     * Disable all AES operations until the system is rebooted.
     *
     * @par Inputs:
     * None.
     *
     * @par Outputs:
     * None.
     *
     * @retval NvError_Success AES operations disabled successfully.
     * @retval NvError_InvalidState If AES engine is already disabled.
     */
    NvDdkAesBlockDevIoctlType_DisableCrypto,
    
    NvDdkAesBlockDevIoctlType_Num,
    /**
     * Ignore -- Forces compilers to make 32-bit enums.
     */
    NvDdkAesBlockDevIoctlType_Force32 = 0x7FFFFFFF
} NvDdkAesBlockDevIoctlType;

/** @name Input/Output Structures for IOCTL Types
 */
/*@{*/

/// SelectKey IOCTL input arguments.
typedef NvDdkAesKeyInfo NvDdkAesBlockDevIoctl_SelectKeyInputArgs;

/// GetCapabilities IOCTL output arguments.
typedef NvDdkAesCapabilities NvDdkAesBlockDevIoctl_GetCapabilitiesOutputArgs;

/// Select operation IOCTL input arguments.
typedef NvDdkAesOperation NvDdkAesBlockDevIoctl_SelectOperationInputArgs;

/// Set initial vector IOCTL input arguments.
typedef struct NvDdkAesBlockDevIoctl_SetInitialVectorInputArgsRec
{
    /// Initial vector (IV) value.
    NvU8 InitialVector[NvDdkAesConst_IVLengthBytes];
} NvDdkAesBlockDevIoctl_SetInitialVectorInputArgs;

/// Get initial vector IOCTL output arguments.
typedef struct NvDdkAesBlockDevIoctl_GetInitialVectorOutputArgsRec
{
    /// Current value of initial vector (IV).
    NvU8 InitialVector[NvDdkAesConst_IVLengthBytes];
} NvDdkAesBlockDevIoctl_GetInitialVectorOutputArgs;

/// Process buffer IOCTL input arguments.
typedef struct NvDdkAesBlockDevIoctl_ProcessBufferInputArgsRec
{
    /// Size of source (SrcBuffer) and destination (DestBuffer) buffers, in
    /// bytes.  Size must be a multiple of the AES block size, i.e., 16 bytes.
    NvU32 BufferSize;
    /// Source buffer.  Pointer to input data to be processed.
    ///
    /// @note When AES operation mode is
    /// NvDdkAesOperationalMode::NvDdkAesOperationalMode_AnsiX931 then
    /// source buffer is treated as date and time input vector (DT) for
    /// Random Number Generator (RNG) algorithm.
    ///
    /// Best performance is achieved when the SrcBuffer meets the alignment
    /// constraints reported by the
    /// ::NvDdkAesBlockDevIoctl_GetCapabilitiesOutputArgs IOCTL.
    const NvU8 *SrcBuffer;
    /// Destination buffer.  Pointer to output data after processing.
    ///
    /// @note When AES operation mode is
    /// NvDdkAesOperationalMode::NvDdkAesOperationalMode_AnsiX931 then
    /// Destination buffer is used to store the "Pseudo Random Number Generator"
    /// (PRNG) data.
    ///
    /// Best performance is achieved when the DestBuffer meets the alignment
    /// constraints reported by the
    /// ::NvDdkAesBlockDevIoctl_GetCapabilitiesOutputArgs IOCTL.
    NvU8 *DestBuffer;
    /// Skip initial \a SkipOffset bytes of \a SrcBuffer before beginning cipher
    /// operation. \a SkipSize must be a multiple of the AES block size, i.e.,
    /// 16 bytes.
    NvU32 SkipOffset;
} NvDdkAesBlockDevIoctl_ProcessBufferInputArgs;

/// Set and lock secure storage key IOCTL input arguments.
typedef struct NvDdkAesBlockDevIoctl_SetAndLockSecureStorageKeyInputArgsRec
{
    /// Length of key. Must be NvDdkAesKeySize::NvDdkAesKeySize_128Bit.
    NvDdkAesKeySize KeyLength;
    /// Key value.
    NvU8 Key[NvDdkAesConst_MaxKeyLengthBytes];
} NvDdkAesBlockDevIoctl_SetAndLockSecureStorageKeyInputArgs;

/*@}*/

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

/** @} */
#endif // INCLUDED_NVDDK_AES_BLOCKDEV_DEFS_H
