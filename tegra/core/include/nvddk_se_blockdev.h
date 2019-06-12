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
 * <b> NVIDIA Driver Development Kit: Security Engine (SE) Block Driver
 *     Definitions</b>
 */

#ifndef INCLUDED_NVDDK_SE_BLOCKDEV_H
#define INCLUDED_NVDDK_SE_BLOCKDEV_H

/**
 * @defgroup nvddk_se_group Security Engine (SE) APIs
 *
 * This file defines the public data structures for the
 *     Tegra SE block driver.
 *
 * <h2>Introduction</h2>
 *
 * Security Engine is a Hardware module in Tegra SoCs, which supports
 *     different cryptographic operations at the hardware level. These
 *     operations supported by the Tegra Security Engine hardware module include:
 * - Secure Hash Algorithm (SHA)
 * - Advanced Encryption Standard (AES)
 * - Random Number Generation (RNG)
 * - Public Key Cryptography (PKC)
 *
 * SHA hardware module supports different variants of the SHA algorithm, namely:
 * - SHA1
 * - SHA224
 * - SHA256
 * - SHA384
 * - SHA512
 *
 * The above variants differ mainly in the number of bytes given as output
 *     hash for the given data. SHA hardware supports calculation of hash in
 *     multiple operations instead of a single operation.
 *
 * <b>Section Overview</b>
 *
 * This topic discusses the background and driver framework for SE operations
 *     followed by SE API usage information. It includes the following
 *     sections:
 * - <a href="#se_hw_back" class="el">SE Hardware Background</a>
 * - <a href="#se_driver_frame" class="el">Driver Framework</a>
 * - <a href="#se_sha_back" class="el">SHA Hardware background</a>
 * - <a href="#se_frame" class="el">Framework</a>
 * - <a href="#se_usage" class="el">Usage</a>
 * - <a href="#se_api" class="el">API Documentation</a>
 *
 * <a name="se_hw_back"></a><h2>SE Hardware Background</h2>
 *
 * SE hardware exposes a set of registers that are directly addressable by
 *     the CPU. Input and output data operations are done through linked lists.
 *     Data is first copied to physically-contiguous memory and a linked list
 *     is created that points to the input data. Another linked list is
 *     created with empty memory locations for the hardware engine to output
 *     the data to those locations. Based on the type of crypto operation, data
 *     can sometimes be collected from SE registers instead of from the output
 *     linked list.
 *
 * Various interrupts are generated from SE hardware module to the CPU based on
 *     the state of the engine. Examples are OP_DONE (gets generated when a
 *     particular operation is done), ERR (gets generated when there is any
 *     error in accessing the memory), etc.
 *
 * <a name="se_driver_frame"><h2>Driver Framework</h2>
 *
 * The SE NvDDK driver is a typical DDK block driver. It gets registered with
 *     the system during initialization. The complete SE driver has two layers:
 * - Top layer is the block driver layer that is common for all the crypto
 *     algorithms supported by the security engine.
 * - Bottom layer is specific to a particular crypto operation.
 *
 * <a name="se_sha_back"><h2>SHA Hardware background</h2>
 *
 * The SHA module takes input data from the input linked list gives output
 *     data either in HASH registers or in memory that can be programmed
 *     dynamically. It supports splitting SHA calculation into multiple
 *     operations, in which case, there is overhead on the software to take
 *     the intermediate state of the SHA engine and reprogram with same when
 *     it has to be continued from the previous operation.
 *
 * <a name="se_frame"><h2>Framework</h2>
 *
 * NvDDK driver makes use of the capability of the hardware to split the SHA
 *     calculation into multiple operations. Interrupts are used instead of
 *     polling to effectively use the hardware and to increase performance.
 *     Two input linked lists, each with one buffer (ping-pong buffers),
 *     are used to parallelize the operations.
 *
 * \image html nvddk_se_buff.jpg "Linked Lists for Parallelized Operations"
 *
 * Whenever there is a buffer submission from upper layer to the driver, it
 *     checks for the size of the data. If the size is less than the linked list
 *     buffer size, the data is copied to one of the ping-pong buffers and SHA
 *     hardware engine will be started after configuring all the relevant
 *     registers with appropriate values.
 *
 * If the size of the received buffer is more than the size of linked list
 *     buffer, both of ping pong buffers are used to process the data. First,
 *     data equal to the size of one buffer (Buffer-A) will be copied and
 *     submitted to the hardware engine. An ISR will be registered with the
 *     DDK framework to let the driver know when the HW is done processing the
 *     buffer. Meanwhile, the driver will go ahead and fill the second buffer
 *     (Buffer-B) and wait for the HW to be done with processing the already
 *     submitted buffer, i.e., Buffer-A. This goes on until the driver is done
 *     processing of all the received data.
 *
 * The usage of the hardware module is protected by mutex, so that multiple
 *     instances of the SE device opening will not fail and affect the driver
 *     working.
 *
 * <a name="se_usage"><h2>Usage</h2>
 * The access to the SE-SHA hardware is through SE block driver interface. Any
 *     application wanting to use SHA hardware must use the below IOCTLs and
 *     follow the below sequence.
 *
 * 1. Open the SE block driver.
 * @code
 * NvDdkBlockDevMgrDeviceOpen(
 *                            NvDdkBlockDevMgrDeviceId_Se,
 *                            0,                    // Instance
 *                            0,                    // MinorInstance
 *                            &sehandle);
 * @endcode
 *
 * 2. Do not call by passing the \a Initinfo argument.
 * @code
 * sehandle->NvDdkBlockDevIoctl(
 *                              sehandle1,
 *                              NvDdkSeBlockDevIoctlType_SHAInit,
 *                              sizeof(NvDdkSeShaInitInfo),
 *                              0,
 *                              &Initinfo,
 *                              0);
 * @endcode
 *
 * 3. Call the update any number of times by appropriately passing the
 *     \a UpdateInfo.
 * @code
 * sehandle->NvDdkBlockDevIoctl(
 *                              sehandle1,
 *                              NvDdkSeBlockDevIoctlType_SHAUpdate,
 *                              sizeof(NvDdkSeShaUpdateArgs),
 *                              0,
 *                              &UpdateInfo,
 *                              0);
 * @endcode
 *
 * 4. Call this final API to collect the hash for the data submitted so far.
 * @code
 * sehandle->NvDdkBlockDevIoctl(
 *                              sehandle1,
 *                              NvDdkSeBlockDevIoctlType_SHAFinal,
 *                              0,
 *                              0,
 *                              0,
 *                              &FinalInfo);
 * @endcode
 *
 * <a name="se_api"><h2>API Documentation</h2>
 *
 * The following sections provide data and function definitions for the SE
 * block driver.
 *
 * @ingroup nvddk_modules
 * @{
 */
#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvddk_blockdev_defs.h"

/**
 * Defines the SHA types.
 */
typedef enum
{
    /**
     *
     */
    NvDdkSeBlockDevIoctlType_Invalid = NvDdkBlockDevIoctlType_Num,

    /**
     * Initializes the SHA engine with the initial values specified by the
     * SHA algorithm.
     *
     * @note \c NvDdkSeBlockDevIoctlType_SHAInit must be called before calling
     * any other IOCTL for SHA operation.
     *
     * @par Inputs:
     *     ::NvDdkSeShaInitInfo
     *
     * @par Outputs:
     * None.
     *
     * @retval NvError_Success Indicates initialization is successful.
     * @retval NvError_BadParameter Indicates mode of operation is invalid.
     * @retval NvError_InvalidSize Indicates the total message length is zero.
     */
    NvDdkSeBlockDevIoctlType_SHAInit,

    /**
     * Updates the SHA engine state with the given data.
     *
     * This can be called multiple times. Every time a call is made with this
     * IOCTL, the SHA engine internal states are updated as per the given data.
     *
     * @par Inputs:
     *     ::NvDdkSeShaUpdateArgs
     *
     * @par Outputs:
     * None.
     *
     * @retval NvError_Success Indicates initialization is successful.
     * @retval NvError_BadParameter Indicates mode of operation is invalid.
     * @retval NvError_InvalidState Indicates the SHA engine is in an invalid
     *    state.
     */
    NvDdkSeBlockDevIoctlType_SHAUpdate,

    /**
     * Returns the final hash value for the data that has been hashed so far.
     *
     * This IOCTL finalizes the current hashing operation and returns
     * the final hash value of the data that has been hashed since
     * initialization.
     *
     * @par Inputs:
     *     ::NvDdkSeShaFinalInfo
     *
     * @par Outputs:
     * None.
     *
     * @retval NvError_Success Indicates initialization is successful.
     * @retval NvError_BadParameter Indicates mode of operation is invalid.
     */
    NvDdkSeBlockDevIoctlType_SHAFinal,
    /**
     * Selects the key value for use in subsequent AES cipher operations. Caller
     * selects one of the pre-defined keys (SBK, SSK) or provides an explicit
     * key value:
     * - ::NvDdkSeAesKeyType_UserSpecified stores the key data you provide in a key slot,
     *   which can be slot 0 to 13. The stored key remains the selected key.
     * - ::NvDdkSeAesKeyType_SecureStorageKey the Secure Storage Key (SSK) unique
     *   per-chip key, which is in key slot 15.
     * - ::NvDdkSeAesKeyType_SecureBootKey selects the Secure Boot Key (SBK),
     *   which is in key slot 14.
     *
     * @note ::NvDdkSeAesBlockDevIoctlType_SelectOperation must be called before
     * calling ::NvDdkSeAesBlockDevIoctlType_SelectKey IOCTL.
     *
     * @par Example:
     *
     * The following example selects the user-defined key slot that has
     * the provided key information.
     * @code
     * KeyInfo.KeyType = NvDdkSeAesKeyType_UserSpecified;
     * KeyInfo.KeyLength = pAesVec[i]->KeyLen;
     * NvOsMemcpy(KeyInfo.Key, pAesVec[i]->Key, pAesVec[i]->KeyLen);
     * SeHandle->NvDdkBlockDevIoctl(
     *     SeHandle,
     *     NvDdkSeAesBlockDevIoctlType_SelectKey,
     *     sizeof(NvDdkSeAesKeyInfo),
     *     0,
     *     (const void *)&KeyInfo,
     *     NULL)
     * @endcode
     *
     * @par Inputs:
     * ::NvDdkAesBlockDevIoctl_SelectKeyInputArgs
     *
     * @par Outputs:
     * None.
     *
     * @retval NvError_Success Indicates key selection is successful.
     * @retval NvError_BadParameter Indicates key type or length is invalid.
     * @retval NvError_InvalidState Indicates AES engine is disabled or operating
     *     mode is not set yet.
     */
    NvDdkSeAesBlockDevIoctlType_SelectKey,
    /**
     * Gets the initialization vector (IV) for the AES engine.
     *
     * @par Inputs:
     * None.
     *
     * @par Outputs:
     * ::NvDdkAesBlockDevIoctl_GetInitialVectorOutputArgs
     *
     * @retval NvError_Success Indicates initial vector fetched successfully.
     * @retval NvError_InvalidState Indicates AES engine is disabled.
     */
    NvDdkSeAesBlockDevIoctlType_GetInitialVector,

    /**
     * Selects cipher operation to perform.
     *
     * @par Inputs:
     * ::NvDdkAesBlockDevIoctl_SelectOperationInputArgs
     *
     * @par Outputs:
     * None.
     *
     * @retval NvError_Success Indicates cipher operation selection is
     *     successful.
     * @retval NvError_BadParameter Indicates invalid operand value.
     * @retval NvError_InvalidState Indicates AES engine is disabled.
     */
    NvDdkSeAesBlockDevIoctlType_SelectOperation,
    /**
     * Processes specified source data buffer using selected operation, cipher
     * key, and initial vector.
     *
     * Stores processed data in destination buffer. Destination data size is the
     * same as source data size.
     *
     * For optimal performance, source and destination buffers must meet the
     * alignment constraints reported by the
     * ::NvDdkAesBlockDevIoctl_GetCapabilitiesOutputArgs IOCTL.
     *
     * @note ::NvDdkSeAesBlockDevIoctlType_SelectOperation must be called before
     * calling ::NvDdkSeAesBlockDevIoctlType_ProcessBuffer IOCTL.
     *
     * @par Inputs:
     * ::NvDdkAesBlockDevIoctl_ProcessBufferInputArgs
     *
     * @par Outputs:
     * None.
     *
     * @retval NvError_Success Indicates crypto operation completed successfully.
     * @retval NvError_InvalidSize Indicates \a BufferSize or \a SkipSize is
     *     illegal.
     * @retval NvError_InvalidAddress Indicates illegal buffer pointer (NULL).
     * @retval NvError_BadValue Indicates other illegal parameter value.
     * @retval NvError_InvalidState Indicates key and/or operating mode not set
     *     yet.
     * @retval NvError_InvalidState Indicates AES engine is disabled.
     */
    NvDdkSeAesBlockDevIoctlType_ProcessBuffer,

    /**
     * Overwrites Secure Boot Key (SBK) in AES key slot with zeroes.
     *
     * After this operation has completed, the SBK value will no longer be
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
     * @retval NvError_Success Indicates Secure Boot Key cleared successfully.
    * @retval NvError_InvalidState Indicates AES engine is disabled.
     */
    NvDdkSeAesBlockDevIoctlType_ClearSecureBootKey,
    /**
     * Performs CMAC operation.
     *
     * @par Inputs:
     * Message for which hash is to be calculated.
     *
     * @par Outputs:
     * Message Authentication code.
     *
     * @retval NvError_Success Indicates AES CMAC is successful.
     * @retval NvError_InvalidSize Indicates \a BufferSize or \a SkipSize is
     *     illegal.
     * @retval NvError_InvalidAddress Indicates illegal buffer pointer (NULL).
     * @retval NvError_BadValue Indicates other illegal parameter value.
     * @retval NvError_InvalidState Indicates AES engine is disabled.
     */
    NvDdkSeAesBlockDevIoctlType_CalculateCMAC,
    /**
     * Write-locks the secure storage key (SSK).
     *
     * @retval NvSuccess Indicates the SSK key in key slot has been
     * successfully write locked.
     */
    NvDdkSeAesBlockDevIoctlType_LockSecureStorageKey,
    /**
     * Sets and write-locks secure storage key (SSK).
     *
     * @retval NvSuccess Indicates the SSK in key slot has been successfully
     * set and write locked.
     * @retval NvError_BadParameter Indicates key buffer is invalid.
     */
    NvDdkSeAesBlockDevIoctlType_SetAndLockSecureStorageKey,
    /**
     * Sets the specified initialization vector (IV) in the key slot previously
     * selected with the ::NvDdkSeAesBlockDevIoctlType_SelectKey operation.
     * The IV must be set as zero for a key slot at the beginning of the operation.
     * While the crypto process is going on for different chunks of input data,
     * The crypto engine stores and restores IV values.
     *
     * @par Inputs:
     *     ::NvDdkSeAesSetIvInfo
     *
     * @par Outputs:
     * None.
     *
     * @retval NvSuccess Indicates the operation was successful.
     * @retval NvError_NotSupported Indicates slot number is out of range.
     * @retval NvError_BadParameter Indicates the input data structure is missing or bad.
     */
    NvDdkSeAesBlockDevIoctlType_SetIV,
    /**
     * Gets the initialization vector (IV) in the key slot previously
     * selected with the ::NvDdkSeAesBlockDevIoctlType_SelectKey operation.
     *
     * @par Inputs:
     *     None.
     * @par Outputs:
     *     ::NvDdkSeAesSetIvInfo
     * @retval NvSuccess Indicates the operation was successful.
     * @retval NvError_NotSupported Indicates the slot number is out of range.
     */
    NvDdkSeAesBlockDevIoctlType_GetIV,
    /**
     * Performs the modular exponentiation part of RSA.
     *
     * Accepts both key (modular and exponent) and data as
     * input arguments and performs modular exponentiation
     * operation.
     *
     * @par Inputs:
     *     ::NvSeRsaKeyDataInfo
     *
     * @par Outputs:
     *     ::NvSeRsaOutDataInfo
     *
     * @retval NvSuccess Indicates the operation was successful.
     * @retval NvError_BadParameter Indicates the mode of operation is
     * invalid, or input/output data is missing.
     * @retval NvError_InvalidSize Indicates the modulus/exponent size
     * is invalid.
     * @retval NvError_NotSupported Indicates the PKC is unsupported.
     * @retval NvError_InvalidSize Indicates the key data is the wrong
     * size.
     */
     NvDdkSeBlockDevIoctlType_RSAModularExponentiation,
     /**
      * Sets up the context for RNG operation.
      *
      * Accepts the parameters (\a mode, \a keysize, \a seed) to setup the
      * initial context of the operation.
      *
      * @par Inputs:
      *      ::NvDdkSeRngInitInfo
      *
      * @retval NvError_BadParameter Indicates the parameter is invalid.
      */
     NvDdkSeBlockDevIoctlType_RNGSetUpContext,
     /**
      *Gets the random number of the size specified.
      *
      * Accepts the size and the pointer to the buffer for generated random
      * number.
      *
      * @par Inputs:
      *      \a pProcessInInfo
      *
      * @par Outputs:
      *      \a pProcessOutInfo
      *
      * @retval NvError_BadParameter Indicates the parameter is invalid.
      */
     NvDdkSeBlockDevIoctlType_RNGGetRandomNum,
    NvDdkSeBlockDevIoctlType_Force32 = 0x7FFFFFFF
} NvDdkSeIoctlType;

/**
 * Defines the crypto algorithms supported by SE engine.
 */
typedef enum
{
    /** Specifies the AES (Advanced Encryption Standard) Algorithm. */
    NvDdkSeOperatingMode_Aes,
    /** Specifies SHA (SHA1, SHA224, SHA256, SHA384, SHA512) Algorithms. */
    NvDdkSeOperatingMode_Sha,
    /**  Specifies the RSA algorithm. */
    NvDdkSeOperatingMode_Rsa,
    /** Specifies the Pseudo-Random Number Generator. */
    NvDdkSeOperatingMode_Rng
} NvDdkSeOperatingMode;

/** Specifies the SHA operating modes. */
typedef enum
{
    NvDdkSeShaOperatingMode_Sha1,
    NvDdkSeShaOperatingMode_Sha224,
    NvDdkSeShaOperatingMode_Sha256,
    NvDdkSeShaOperatingMode_Sha384,
    NvDdkSeShaOperatingMode_Sha512
} NvDdkSeShaEncMode;

/** Specifies the SHA hash result sizes in bits. */
typedef enum
{
    NvDdkSeShaResultSize_Sha1 = 160,
    NvDdkSeShaResultSize_Sha224 = 224,
    NvDdkSeShaResultSize_Sha256 = 256,
    NvDdkSeShaResultSize_Sha384 = 384,
    NvDdkSeShaResultSize_Sha512 = 512
} NvDdkSeShaHashResultSize;

/// Holds the information required for SHA engine initialization.
typedef struct
{
    /// Holds the variant of SHA algorithm in operation.
    NvDdkSeShaEncMode SeSHAEncMode;
    /// Holds the length of the total message to be hashed.
    NvU32 TotalMsgSizeInBytes;
} NvDdkSeShaInitInfo;

/// Holds the information for updating the SHA engine with a new hash.
typedef struct
{
    /// Holds the size of the data buffer currently being submitted.
    /// If this is not the last buffer, size should be a multiple of
    /// the SHA block size.
    NvU32 InputBufSize;
    /// Holds a pointer to the source buffer.
    NvU8 *SrcBuf;
} NvDdkSeShaUpdateArgs;

/// Holds information required for collecting the final hash value.
typedef struct
{
    /// Holds the size of the hash expected for the selected SHA algorithm.
    NvU32 HashSize;
    /// Holds a pointer to the output buffer. At least the size mentioned in
    /// the \a HashSize argument must be available through this pointer.
    NvU8 *OutBuf;
} NvDdkSeShaFinalInfo;

/**
 * Defines AES key constants.
 */
typedef enum
{
    /**
     * Maximum supported AES key length, in bytes.
     */
    NvDdkSeAesConst_MaxKeyLengthBytes = 32,

    /**
     * AES initial vector length, in bytes.
     */
    NvDdkSeAesConst_IVLengthBytes = 16,

    /**
     * AES block size, in bytes.
     */
    NvDdkSeAesConst_BlockLengthBytes = 16,
    NvDdkSeAesConst_Num,
    NvDdkSeAesConst_Force32 = 0x7FFFFFFF
} NvDdkSeAesConst;

/**
 * Defines AES key lengths.
 */
typedef enum
{
    NvDdkSeAesKeySize_Invalid,

    /**
     * Defines 128-bit key length, in bytes.
     */
    NvDdkSeAesKeySize_128Bit = 16,

    /**
     * Defines 192-bit key length, in bytes.
     */
    NvDdkSeAesKeySize_192Bit = 24,

    /**
     * Defines 256-bit key length, in bytes.
     */
    NvDdkSeAesKeySize_256Bit = 32,
    NvDdkSeAesKeySize_Num,
    NvDdkSeAesKeySize_Force32 = 0x7FFFFFFF
} NvDdkSeAesKeySize;

/**
 * Defines AES key types.
 */
typedef enum
{
    NvDdkSeAesKeyType_Invalid,

    /**
     * Specifies the Secure Boot Key (SBK) used to protect boot-related data
     * when the chip is in ODM production mode.
     */
    NvDdkSeAesKeyType_SecureBootKey,

    /**
     * Specifies the Secure Storage Key (SSK) unique per-chip key, unless
     * explicitly overridden; it is intended for data that must be tied to a
     * specific instance of the chip.
     */
    NvDdkSeAesKeyType_SecureStorageKey,

    /**
     * Specifies an arbitrary, user-specified key supplied explicitly by the
     * caller.
     */
    NvDdkSeAesKeyType_UserSpecified,
    NvDdkSeAesKeyType_Num,
    NvDdkSeAesKeyType_Force32 = 0x7FFFFFFF
} NvDdkSeAesKeyType;

/**
 * Defines supported cipher algorithms.
 */
typedef enum
{
    NvDdkSeAesOperationalMode_Invalid,

    /**
     * Specifies the AES with Cipher Block Chaining (CBC)--
     * For AES spec, see FIPS Publication 197;
     * For CBC spec, see NIST Special Publication 800-38A.
     */
    NvDdkSeAesOperationalMode_Cbc,
    /**
     * Specifies the AES with Electronic Codebook (ECB)--
     * For AES spec, see FIPS Publication 197;
     * For ECB spec, see NIST Special Publication 800-38A.
     */
    NvDdkSeAesOperationalMode_Ecb,
   /**
     * Specifies the AES with Output FeedBack (OFB)
     * For AES spec, see FIPS Publication 197;
     * For OFB spec, see NIST Special Publication 800-38A.
     */
    NvDdkSeAesOperationalMode_Ofb,
    /**
     * Specifies the AES with Counter Mode (CTR)
     * For AES spec, see FIPS Publication 197;
     * For CTR spec, see NIST Special Publication 800-38A.
     */
    NvDdkSeAesOperationalMode_Ctr,
    NvDdkSeAesOperationalMode_Num,
    NvDdkSeAesOperationalMode_Force32 = 0x7FFFFFFF
} NvDdkSeAesOperationalMode;

/**
 * Holds the key argument.
*/
typedef struct NvDdkSeAesKeyInfoRec
{

    /**
     * Select key type -- SBK, SSK, user-specified.
     */
    NvDdkSeAesKeyType KeyType;

    /**
     * Length of key; ignored unless key type is user-specified.
     */
    NvDdkSeAesKeySize KeyLength;

    /**
     * Key value; ignored unless key type is user-specified.
     */
    NvU8 Key[32];
} NvDdkSeAesKeyInfo;

/**
 * Input to the SeAesProcessBuffer IOCTL.
 */
typedef struct NvDdkSeAesProcessBufferRec
{
    /** A pointer to source buffer. */
    NvU8 *pSrcBuffer;
    /** A pointer to destination buffer. */
    NvU8 *pDstBuffer;
    /** The length of the buffer to be processed. */
    NvU32  SrcBufSize;
} NvDdkSeAesProcessBufferInfo;

/**
 * Input to the GetIv IOCTL.
 */
typedef struct NvDdkSeAesGetIvRec
{
    /** A pointer to an empty buffer that will be filled
     * with the initialization vector (IV). */
    NvU8 *Iv;
    /** Length of IV. */
    NvU32 VectorSize;
} NvDdkSeAesGetIvArgs;

/**
 * Holds NvDdk AES operation argument.
 */
typedef struct NvDdkSeAesOperationRec
{
    /**
     * Block cipher mode of operation.
     */
    NvDdkSeAesOperationalMode OpMode;

    /**
     * Select NV_TRUE to perform an encryption operation, NV_FALSE to perform a
     * decryption operation.
     *
     * @note @a IsEncrypt is ignored when AES operation mode is
     * NvDdkSeAesOperationalMode_AnsiX931.
     */
    NvBool IsEncrypt;
} NvDdkSeAesOperation;

/**
 * Holds CMAC input information.
 */
typedef struct NvDdkSeAesComputeCMACInRec
{
    /**
     * A pointer to the key being used.
     */
    NvU8 *Key;
    /**
     * Length of the key been used (128, 192, or 256 bytes).
     */
    NvU32 KeyLen;
    /**
     * A pointer to the input buffer.
     */
    NvU8 *pBuffer;
    /**
     * Length of the input buffer.
     */
    NvU32 BufSize;
    /**
     * A flag specifying whether the chunk is the first chunk.
     */
    NvBool IsFirstChunk;
    /**
     * A flag specifying whether the chunk is the last chunk
     */
    NvBool IsLastChunk;
    /**
     * A flag specifying whether or not this is SBK key.
     */
    NvBool IsSbkKey;
} NvDdkSeAesComputeCmacInInfo;

/**
 * Holds CMAC output information.
 */
typedef struct NvDdkSeAesComputeCMACOutRec
{
    /**
     * A pointer to the CMAC.
     */
    NvU8 *pCMAC;
    /**
     * Length of the CMAC.
     */
    NvU32 CMACLen;
} NvDdkSeAesComputeCmacOutInfo;

/**
 * Holds SSK.
 */
typedef struct NvDdkSeAesSskRec
{
    /**
     * A pointer to SSK.
     */
    NvU8 *pSsk;
} NvDdkSeAesSsk;

/**
 * Holds IV.
 */
typedef struct NvDdkSeAesSetIvRec
{
    /**
     * A pointer to IV.
     */
    NvU8 *pIV;
    /* IV size in bytes. */
    NvU32 VectorSize;
} NvDdkSeAesSetIvInfo;

/**
 * Holds information about an RSA key that is stored in a key slot.
 */
typedef struct
{
    /// Specifies which key slot to use.
    NvU32 KeySlot;
    /// Holds the modulus size in bytes.
    NvU32 ModulusSizeInBytes;
    /// Holds the exponent size in bytes (could be private or public).
    NvU32 ExponentSizeInBytes;
    /// Holds a pointer to the modulus buffer.
    NvU8 *Modulus;
    /// Holds a pointer to the exponent buffer.
    NvU8 *Exponent;
    /// Holds a pointer to the input data buffer.
    NvU8 *InputBuf;
} NvSeRsaKeyDataInfo;

/**
 * Holds the output data buffer.
 */
typedef struct
{
    /// Holds a pointer to the output data buffer.
    NvU8 *OutputBuf;
} NvSeRsaOutDataInfo;

/// Specifies different modes of RNG operation.
typedef enum
{
    /// Specifies to force instantiation.
    NvDdkSeRngMode_ForceInstantaite,
    /// Specifies to force reseed.
    NvDdkSeRngMode_ForceReseed,
    /// Specifies normal mode.
    NvDdkSeRngMode_Normal,
} NvDdkSeRngMode;

/// RNG key sizes. RNG keys are used as AES keys
/// for symmetric-key algorithms.
typedef enum
{
    /// 128-bit key.
    NvDdkSeRngKeySize_128,
    /// 192-bit key.
    NvDdkSeRngKeySize_192,
    /// 256-bit key.
    NvDdkSeRngKeySize_256
} NvDdkSeRngKey;

/// Holds the random number generator (RNG) initialization information.
typedef struct
{
    /// Specifies the mode to be used.
    NvDdkSeRngMode RngMode;
    /// Holds the key to be used.
    NvDdkSeRngKey RngKeyUsed;
    /// Holds a pointer to the RNG seed.
    NvU8 *RngSeed;
    /// Holds the RNG seed size.
    NvU32 RngSeedSize;
    /// Holds a flag specifying whether we use source as memory.
    NvU32 IsSrcMem;
    /// Holds a flag specifying whether we use destination as memory.
    NvU32 IsDestMem;
} NvDdkSeRngInitInfo;

/// Holds the input parameter for random number generator.
typedef struct
{
    /// Holds the size of the random number to be generated in bytes.
    NvU32 OutPutSizeReq;
} NvDdkSeRngProcessInInfo;

/// Holds the output parameter where the random number is to be placed.
typedef struct
{
    /// Holds a pointer to the generated random number.
    NvU8 *pRandomNumber;
} NvDdkSeRngProcessOutInfo;

/**
 * Allocates and initializes the required global resources.
 *
 * @warning This method must be called once before using any NvDdkSeBlockDev
 *          APIs.
 *
 * @param hRmDevice RM device handle.
 *
 * @retval NvSuccess Indicates global resources have been allocated and
 *     initialized.
 * @retval NvError_DeviceNotFound Indicates the device is not present or not
 *     supported.
 * @retval NvError_InsufficientMemory Indicates memory cannot be allocated.
 */
NvError
NvDdkSeBlockDevInit(NvRmDeviceHandle hRmDevice);

/**
 * Allocates resources, powers on the device, and prepares the device for
 * I/O operations.
 *
 * A valid \c NvDdkBlockDevHandle is returned only if the
 * device is found. The same handle must be used for further operations,
 * except for NvDdkSeBlockDevInit(). Multiple invocations from multiple
 * concurrent clients must be supported.
 *
 * @warning This method must be called once before using other NvDdkBlockDev
 *     APIs.
 *
 * @param Instance Specifies the instance of specific device.
 * @param MinorInstance Specifies the minor instance of the specific device.
 * @param phBlockDevInstance Returns a pointer to device handle.
 *
 * @retval NvSuccess Indicates device is present and ready for I/O operations.
 * @retval NvError_DeviceNotFound Indicates device is either not present, not
 *     responding, or not supported.
 * @retval NvError_InsufficientMemory Indicates memory cannot be allocated.
 */
NvError
NvDdkSeBlockDevOpen(NvU32 Instance,
                    NvU32 MinorInstance,
                    NvDdkBlockDevHandle *phBlockDevInstance);

/**
 * Releases all global resources allocated.
 */
void
NvDdkSeBlockDevDeinit(void);

/**
 * Performs an I/O control (IOCTL) operation on the device.
 *
 * @param hBlockDev Handle acquired during the ::NvDdkSeBlockDevOpen call.
 * @param Opcode Type of control operation to perform.
 * @param InputSize Size of input arguments buffer, in bytes.
 * @param OutputSize Size of output arguments buffer, in bytes.
 * @param InputArgs Pointer to input arguments buffer.
 * @param OutputArgs Pointer to output arguments buffer.
 *
 * @retval NvError_Success IOCTL is successful.
 * @retval NvError_NotSupported Opcode is not recognized.
 * @retval NvError_InvalidSize \a InputSize or \a OutputSize is incorrect.
 * @retval NvError_InvalidParameter \a InputArgs or\a  OutputArgs is incorrect.
 * @retval NvError_IoctlFailed IOCTL failed for other reason.
 */

NvError
NvDdkSeBlockDevIoctl(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 Opcode,
    NvU32 InputSize,
    NvU32 OutputSize,
    const void *InputArgs,
    void *OutputArgs);


/**
 * Frees the client's resources.
 *
 * @param hBlockDev Handle acquired during the NvDdkSeBlockDevOpen() call.
 *
 */
void NvDdkSeBlockDevClose(NvDdkBlockDevHandle hBlockDev);

/**
 * Performs an RSASSA-PSS-VERIFY signature verification operation.
 *
 *  @note The key must be loaded into a RSA key slot before calling
 *      this function.
 *
 *  @param RsaKeySlot The SE RSA key slot to use.
 *  @param RsaKeySizeBits The size of the RSA key in bits.
 *  @param pInputMessage A pointer to the message whose
 *         signature is to be verified
 *  @param pMessageHash Allows the caller to provide the SHA hash of the
 *         input message. If @a pMessageHash is @c NULL, then this function
 *         calculates the SHA hash of the input message.
 *  @param InputMessageLengthBytes The length of the message in bytes.
 *  @param pSignature A pointer to the signature to be verified.
 *         The length of the Signature
 *         pointed to by @a pSignature must be of length k, where
 *         k is the length in octets of the RSA modulus n.
 *  @param HashAlgorithm The hash algorithm to be used.
 *         ::NvDdkSeShaEncMode defines supported values.
 *         This parameter is for both MGF and mHash.
 *  @param PublicKeyModulus The public key modulus to use to verify the signature.
 *         If the public key modulus is provided, it is an array of 256 bytes.
 *         If this argument is NULL, then the boot loader uses the PKC
 *         public key modulus from the BCT.
 *  @param sLen The salt length to be used.
 *         ::NvDdkSeShaHashResultSize defines supported values.
 *
 *  @retval NvError_Success Indicates a valid signature was calculated.
 *  @retval NvError_RsaPssVerify_Inconsistent Indicates an invalid signature
 *  was calculated.
 *
 */

NvError
NvDdkSeRsaPssSignatureVerify(
    NvU8 RsaKeySlot,
    NvU32 RsaKeySizeBits,
    NvU32 *pInputMessage,
    NvU32 *pMessageHash,
    NvU32 InputMessageLengthBytes,
    NvU32 *pSignature,
    NvDdkSeShaEncMode HashAlgorithm,
    NvU8 *PublicKeyModulus,
    NvS8 sLen
    );


#if defined(__cplusplus)
}
#endif

/** @} */
#endif
