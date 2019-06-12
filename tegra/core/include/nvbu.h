/*
 * Copyright (c) 2011-2012, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file nvbu.h
 * @brief <b> Nv Boot Update Management Framework.</b>
 *
 * @b Description: This file declares the API's for updating BCT's and
 *    boot loaders (BL's).
 */

#ifndef INCLUDED_NVBU_H
#define INCLUDED_NVBU_H

#include "nvbct.h"
#include "nvbit.h"
#include "nvddk_fuse.h"

#include "nvcommon.h"
#include "nverror.h"
#include "nvcrypto_common_defs.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/// Crypto Ops to be performed
typedef enum NvBuBlCryptoOpRec
{
    /// Verify signature for data
    NvBuBlCryptoOp_Verify = 0x0,
    /// Decrypt and verify signature for data
    NvBuBlCryptoOp_VerifyAndDecrypt,
    /// Encrypt and sign data
    NvBuBlCryptoOp_EncryptAndSign,
    /// Max number of operations possible
    NvBuBlCryptoOp_Num = NvBuBlCryptoOp_EncryptAndSign
}NvBuBlCryptoOp;

/// Boot loader activation information
typedef struct NvBuBlInfoRec
{
    /// NV_TRUE if BL instance was used to boot the currently-running BL
    NvBool IsActive;
    /// NV_TRUE if BL instance is enabled to be loaded by the Boot ROM
    NvBool IsBootable;
    /// Partition where BL content is stored
    NvU32 PartitionId;
    /// Load Address of BL instance
    NvU32 LoadAddress;
    /// Version number of BL instance
    NvU32 Version;
    /// Generation number of BL instance.  A generation is a group of BL
    /// instances with the same version number that are located in adjacent
    /// slots in the BCT's table of BL's.  The generation numbers start from
    /// zero.
    NvU32 Generation;
} NvBuBlInfo;

/**
 * Register partition id in the BCT's table of BL's
 *
 * @param hBct handle to BCT instance
 * @param PartitionId partition containing BL
 *
 * @retval NvSuccess Partition id successfully added
 * @retval NvError_InvalidAddress Illegal pointer value
 * @retval NvError_InsufficientMemory BCT is full; BL cannot be added
 * @retval NvError_BadParameter Illegal or out-of-range parameter
 */

NvError
NvBuAddBlPartition(
    NvBctHandle hBct,
    NvU32 PartitionId);

/**
 * Query a list of BL partitions sorted according to the order in which
 * the Boot ROM will attempt to load them.  The first BL in the list is
 * first in the load order.
 *
 * To determine the number of boot loaders, specify a *NumBootloaders of zero
 * and a NULL BlInfo.  As a result, *NumBootloaders will be set to the actual
 * number of installed boot loaders and NvSuccess will be reported.
 *
 * Note that the following capabilities will be lost is any of the active BL's
 * are modified other than to shift their location in the boot order --
 *
 * 1. recovery from an interrupted update (due to unexpected power loss)
 * 2. fallback to an earlier working BL in the case that the newly-installed
 *    BL's fail to authenticate (due to a mass storage error)
 *
 * @param hBct handle to BCT instance
 * @param hBit handle to BIT instance
 * @param NumBootloaders pointer to number of installed boot loaders
 * @param BlInfo array of *NumBootloaders elements; contains information
 *        each installed boot loader; ordered according to the order in
 *        which the Boot ROM will attempt to load them
 *
 * @retval NvSuccess BL list query successful
 * @retval NvError_InvalidAddress Illegal pointer value
 * @retval NvError_InsufficientMemory Not enough memory for BlInfo array
 * @retval NvError_BadParameter Illegal or out-of-range parameter
 */

NvError
NvBuQueryBlOrder(
    NvBctHandle hBct,
    NvBitHandle hBit,
    NvU32 *NumBootloaders,
    NvBuBlInfo *BlInfo);

/**
 * Set boot loaders as bootable or unbootable, then update the load order.
 *
 * Note that only the PartitionId and IsBootable fields in BlInfo are used; all
 * other fields are ignored.
 *
 * All bootable BL's must adjacent, starting at the beginning of the list.  At
 * least one BL must be bootable.  The specified number of boot loaders must be
 * the same as reported by NvBuQueryBlOrder().
 *
 * @param hBct handle to BCT instance
 * @param hBit handle to BIT instance
 * @param NumBootloaders pointer to number of installed boot loaders
 * @param BlInfo array of *NumBootloaders elements; contains information
 *        each installed boot loader; ordered according to the order in
 *        which the Boot ROM will attempt to load them
 *
 * @retval NvSuccess BL list query successful
 * @retval NvError_InvalidAddress Illegal pointer value
 * @retval NvError_InsufficientMemory Not enough memory for BlInfo array
 * @retval NvError_BadParameter Illegal or out-of-range parameter
 */


NvError
NvBuUpdateBlOrder(
    NvBctHandle hBct,
    NvBitHandle hBit,
    NvU32 *NumBootloaders,
    NvBuBlInfo *BlInfo);

/**
 * Update boot loader information in BCT for BL in specified partition
 *
 * @param hBct handle to BCT instance
 * @param PartitionId partition containing BL
 * @param Version BL version
 * @param StartBlock block number where BL is located on boot device
 * @param StartSector sector number where BL is located on boot device
 * @param Length length of BL, in bytes
 * @param LoadAddress memory address where BL is to be loaded
 * @param EntryPoint memory address where BL execution is to begin
 * @param CryptoHashSize size of CryptoHash, in bytes
 * @param CryptoHash hash of BL contents
 *
 * @retval NvSuccess Data for specified BL partition has been updated
 * @retval NvError_InvalidAddress Illegal pointer value
 * @retval NvError_BadParameter Illegal or out-of-range parameter
 * @retval NvError_NotInitialized Partition id does not exist in BCT
 */

NvError
NvBuUpdateBlInfo(
    NvBctHandle hBct,
    NvU32 PartitionId,
    NvU32 Version,
    NvU32 StartBlock,
    NvU32 StartSector,
    NvU32 Length,
    NvU32 LoadAddress,
    NvU32 EntryPoint,
    NvU32 CryptoHashSize,
    NvU8 *CryptoHash);

/**
 * Make specified partition bootable and place it first in the boot order.
 *
 * The partition must correspond to one of the NvBctDataType_BootLoaderInfo
 * instances in the BCT.
 *
 * @param hBct handle to BCT instance
 * @param hBit handle to BIT instance
 * @param PartitionId partition to be made bootable
 *
 * @retval NvSuccess Specified partition is now bootable
 * @retval NvError_BadValue Specified partition is not a bootloader partition
 */

NvError
NvBuSetBlBootable(
    NvBctHandle hBct,
    NvBitHandle hBit,
    NvU32 PartitionId);

/**
 * Performs crypto operations on the BCT as required for the specified
 * operating mode.Supported crypto operations are Encrypt-and-Sign
 * and Decrypt-and-Verify.
 *
 * The en/de-crypted and signed BCT is stored in the caller-supplied Buffer.
 * To query the required size of Buffer, invoke the routine with *Size set to
 * zero.On return, NvSuccess will be reported and *Size will have been set
 * equal to the required buffer size.
 * Note that if *Size is larger than the required size and the BCT processing
 * completes successfully, then *Size will updated to reflect the actual size
 * of the encoded BCT in the Buffer.
 *
 * @param hBct handle to BCT instance
 * @param OperatingMode system's operating mode
 * @param Size pointer to size of Buffer, in bytes
 * @param Buffer buffer where BCT before and after crypto operations is stored
 * @param CryptoOp signifies the crypto operations to be performed.
 *
 * @retval NvSuccess BCT encrypted and signed successfully
 * @retval NvError_BadParameter illegal operating mode (or fuses aren't burned)
 * @retval NvError_InsufficientMemory not enough memory to carry out operation
 * @retval NvError_* Crypto error
 */
NvError
NvBuBctCryptoOps(
    NvBctHandle hBct,
    NvDdkFuseOperatingMode OperatingMode,
    NvU32 *Size,
    void *Buffer,
    NvBuBlCryptoOp CryptoOp);

/**
 * Write BCT to device at specified block/slot location.
 *
 * Set Device to NvBctBootDevice_Current to use the current boot device.
 * The current boot device is typically determined by inspecting fuse settings.
 * Update Algorithm:
 * 1) Take the new BCT (provided by the user) and write it to Block 0, Slot 1.
 * 2) Write the new BCT to Block X, Slot 0 if an identical BCT is not already
 *    present. X = [1-MaxBcts]. MaxBcts is determined by the BctPartitonSize.
 * 3) Write the new BCT to Block 0, Slot 0.
 *
 * @param hBct handle to BCT instance
 * @param hBit handle to BIT instance
 * @param PartitionId The partition on which the bct is present
 * @param Block block address on device where BCT is to be stored
 * @param Slot slot address on device where BCT is to be stored; legal values
 *        are [0, 1] for block 0 and [0] for all other blocks
 * @param The buffer containing the BCT
 *
 * @retval NvSuccess BCT encrypted and signed successfully
 * @retval NvError_BadParameter illegal boot device (or fuses aren't burned),
 *         or Block/Slot value is out of range
 * @retval NvError_InsufficientMemory not enough memory to carry out operation
 * @retval NvError_* Media write error
 */

NvError
NvBuBctUpdate(
    NvBctHandle hBct,
    NvBitHandle hBit,
    NvU32 PartitionId,
    NvU32 size,
    void *buffer);

/**
 * Read BCT from device and verify whether it's been written correctly.
 *
 * Set Device to NvBctBootDevice_Current to use the current boot device.
 * The current boot device is typically determined by inspecting fuse settings.
 * Update Algorithm:
 * 1) Read Bct from Block X, Slot 0 where X = [1-MaxBcts], MaxBcts
 *     determined by the BCT partition size.
 * 2) For each BCT read, decrypt and verify hash.
 *
 * @param hBct handle to BCT instance
 * @param hBit handle to BIT instance
 * @param PartitionId The partition on which the bct is present
 * @param size The size of the BCT
 * @param The buffer containing the BCT
 *
 * @retval NvSuccess All BCT copies are decrypted and verified successfully
 * @retval NvError_BadParameter illegal boot device (or fuses aren't burned),
 *         or Block/Slot value is out of range
 * @retval NvError_InsufficientMemory not enough memory to carry out operation
 * @retval NvError_* Media write error
 */
NvError
NvBuBctReadVerify(
    NvBctHandle hBct,
    NvBitHandle hBit,
    NvU32 PartitionId,
    NvDdkFuseOperatingMode OperatingMode,
    NvU32 size);
/**
 * Properly deals with the BCTs when recovering from an interrupted update.
 *
 * Recovery Algorithm:
 * 1) Read the Active BCT from the device using BctPage/Block info in the BIT,
 *    and write it to Block 0, Slot 1 if the data already present in
 *    Block 0, Slot 1 is not an identical BCT
 * 2) Write the active BCT to Block X, Slot 0 if an identical BCT is not already
 *    present. X = [1-MaxBcts]. MaxBcts is determined by the BctPartitonSize.
 * 3) Write the active BCT to Block 0, Slot 0.
 *
 * @param hBct handle to BCT instance
 * @param hBit handle to BIT instance
 * @param Block block address on device where BCT is to be stored
 * @param PartitionId The partition on which the bct is present
 *
 * @retval NvSuccess BCT encrypted and signed successfully
 * @retval NvError_BadParameter illegal boot device (or fuses aren't burned),
 *         or Block/Slot value is out of range
 * @retval NvError_InsufficientMemory not enough memory to carry out operation
 * @retval NvError_* Media write error
 */
NvError
NvBuBctRecover(
    NvBctHandle hBct,
    NvBitHandle hBit,
    NvU32 PartitionId);

/**
 * Create a table indicating the bad blocks on the device.
 * The table is a bit array, where each bit represents a block.
 * A '1' indicates a bad block.
 *
 *
 * @param hBct The partition on which the bct is present
 * @param PartitionId The partition on which the bct is present
 *
 * @retval NvSuccess Bad block table successfully created
 * @retval NvError_InsufficientMemory not enough memory to carry out operation
 */
NvError
NvBuBctCreateBadBlockTable(
    NvBctHandle hBct,
    NvU32 PartitionId);

/**
 * Query whether a BCT update is in progress, but not yet completed
 *
 * @param hBit handle to BIT instance
 * @param pIsInterruptedUpdate pointer to boolean, set to NV_TRUE if a BCT
 *        update was interrupted; else NV_FALSE
 *
 * @retval NvSuccess Interrupt status reported successfully
 * @retval NvError_InvalidAddress Illegal pointer value
 */

NvError
NvBuQueryIsInterruptedUpdate(
    NvBitHandle hBit,
    NvBool *pIsInterruptedUpdate);

/**
 * Query whether boot loader (BL) actively running on the system belongs to
 * the primary generation (i.e., first generation in load order)
 *
 * @param hBct handle to BCT instance
 * @param hBit handle to BIT instance
 * @param IsPrimaryBlRunning pointer to NvBool; NV_TRUE is running BL belongs
 *        to primary generation, else NV_FALSE
 *
 * @retval NvSuccess Query completed successfully
 * @retval NvError_InvalidAddress Illegal pointer value
 */

NvError
NvBuQueryIsPrimaryBlRunning(
    NvBctHandle hBct,
    NvBitHandle hBit,
    NvBool *IsPrimaryBlRunning);

/**
 * Query whether boot loader updates are allowed.
 *
 * Updates are allowed when --
 * 1. recovery from a previously-interrupted update is in progress
 * 2. the active BL running on the system doesn't belong to the primary
 * generation (i.e., first generation in load order)
 *
 * @param hBct handle to BCT instance
 * @param hBit handle to BIT instance
 * @param IsUpdateAllowed pointer to NvBool; NV_TRUE if BL updates are
 *        allowed, else NV_FALSE
 *
 * @retval NvSuccess Query completed successfully
 * @retval NvError_InvalidAddress Illegal pointer value
 */

NvError
NvBuQueryIsUpdateAllowed(
    NvBctHandle hBct,
    NvBitHandle hBit,
    NvBool *IsUpdateAllowed);

/**
 * Find the index number (aka, slot number) in the BCT's table of boot loaders
 * corresponding to the specified BL partition
 *
 * @param hBct handle to BCT instance
 * @param PartitionId partition containing BL
 * @param Slot pointer to index number in BCT's table of BL's
 *
 * @retval NvSuccess Specified BL partition exists in BCT and slot number
 *         found successfully
 * @retval NvError_InvalidAddress Illegal pointer value
 * @retval NvError_BadParameter Illegal or out-of-range parameter
 */

NvError
NvBuQueryBlSlotByPartitionId(
    NvBctHandle hBct,
    NvU32 PartitionId,
    NvU32 *Slot);

/**
 * Read BCT from device and verify  it and fill the buffer passed with the bct
 *
 * Set Device to NvBctBootDevice_Current to use the current boot device.
 * The current boot device is typically determined by inspecting fuse settings.
 * Update Algorithm:
 * 1) Read Bct from Block X, Slot 0 where X = [1-MaxBcts], MaxBcts
 *     determined by the BCT partition size.
 * 2) Decrypt and verify hash
 * 3) Fill in buffer parameter with the hash verified bct
 *
 * @param hBct handle to BCT instance
 * @param hBit handle to BIT instance
 * @param PartitionId The partition on which the bct is present
 * @param pBuffer The buffer to be populated with bct
 *
 * @retval NvSuccess BCT
 * @retval NvError_BadParameter illegal boot device (or fuses aren't burned),
 *         or Block/Slot value is out of range
 * @retval NvError_* Media read error
 */

NvError
NvBuBctRead(
    NvBctHandle hBct,
    NvBitHandle hBit,
    NvU32 PartitionId,
    NvU8 *pBuffer);

/**
 * Get the BCT size
 *
 * Set Device to NvBctBootDevice_Current to use the current boot device.
 * The current boot device is typically determined by inspecting fuse settings.
 * @param hBct handle to BCT instance
 * @param hBit handle to BIT instance
 * @param PartitionId The partition on which the bct is present
 * @param pBctSize The buffersize to be returned
 *
 * @retval NvSuccess bct size query succeeded
 * @retval NvError_BadParameter illegal boot device (or fuses aren't burned),
 *         or Block/Slot value is out of range
 * @retval NvError_* Media write error
 */
NvError
NvBuBctGetSize(
    NvBctHandle hBct,
    NvBitHandle hBit,
    NvU32 PartitionId,
    NvU32 *pBctSize);

/**
 * Register partition id in the BCT's table of partition's for which hash needs to be calculated
 *
 * @param hBct handle to BCT instance
 * @param PartitionId partition containing BL
 *
 * @retval NvSuccess Partition id successfully added
 * @retval NvError_InvalidAddress Illegal pointer value
 * @retval NvError_InsufficientMemory BCT is full; BL cannot be added
 * @retval NvError_BadParameter Illegal or out-of-range parameter
 */
NvError
NvBuAddHashPartition(NvBctHandle hBct,
    NvU32 PartitionId);

/**
 * Update Partition information in BCT for partition specified
 *
 * @param hBct handle to BCT instance
 * @param PartitionId partition containing BL
 * @param CryptoHashSize size of CryptoHash, in bytes
 * @param CryptoHash hash of BL contents
 *
 * @retval NvSuccess Data for specified BL partition has been updated
 * @retval NvError_InvalidAddress Illegal pointer value
 * @retval NvError_BadParameter Illegal or out-of-range parameter
 * @retval NvError_NotInitialized Partition id does not exist in BCT
 */

NvError
NvBuUpdateHashPartition(NvBctHandle hBct,
    NvU32 PartitionId,
    NvU32 CryptoHashSize,
    NvU8 *CryptoHash);

/**
 * Encrypt or decrypt the BCT
 *
 * @param Buffer used to store input to decryption and also the
 *         encrypted/decrypted BCT
 * @param Size size of data to be encrypted/decrypted
 * @param IsEncrypt Flag to indicate if it's an encrypt operation or
 *         decrypt operation. NV_TRUE for encrypt and NV_FALSE for
 *         decrypt.
 * @param KeyType type of cipher key to be used for the operation.
 *@param Key for encryption or decryption
 *
 * @retval NvSuccess BCT was encrypted/decrypted successfully.
 * @retval NvError_* An error occurred during the operation.
 */

NvError
BctEncryptDecrypt(
    NvU8 *Buffer,
    NvU32 Size,
    NvBool IsEncrypt,
    NvCryptoCipherAlgoAesKeyType KeyType,
    NvU8 *Key);

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

#endif // INCLUDED_NVBU_H

