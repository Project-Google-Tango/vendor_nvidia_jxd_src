/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA Tegra ODM Kit:
 *         System Update Interface</b>
 *
 * @b Description: Defines the ODM adaptation interface for system updates.
 *
 */

#ifndef INCLUDED_NVODM_SYSTEM_UPDATE_H
#define INCLUDED_NVODM_SYSTEM_UPDATE_H

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup nvodm_sysupd System Update Adaptation Interface
 *
 * System update data is expected to be cryptographically signed. When the
 * bootloader attempts to install new system data (modem firmware, boot loader,
 * or new operating system image, etc.), the data is first verified to be
 * correct via these interfaces.
 * @ingroup nvodm_adaptation
 * @{
 */

/** Entire signature size in bytes. */
#define NVODM_SYS_UPDATE_SIGNATURE_SIZE    (4)
/** Signature block size, basic unit of encryption. */
#define NVODM_SYS_UPDATE_SIGNATURE_BLOCK   (16)

/**
 * Defines the types of binary updates.
 */
typedef enum
{
    NvOdmSysUpdateBinaryType_Bootloader,

    NvOdmSysUpdateBinaryType_Nums
}NvOdmSysUpdateBinaryType;

/**
 * Signs a block of data.
 *
 * This may be called multiple times. An accumulated signature must be
 * generated. The first and last blocks will be flagged via the \a bFirst
 * and \a bLast parameters.
 *
 * @param Data A pointer to a block of data. This will be a multiple of
 *      ::NVODM_SYS_UPDATE_SIGNATURE_BLOCK.
 * @param Size The size in bytes of the data.
 * @param bFirst \c NV_TRUE specifies this is the first block.
 * @param bLast \c NV_TRUE specifies this is the last block.
 */
NvBool
NvOdmSysUpdateSign( NvU8 *Data, NvU32 Size, NvBool bFirst, NvBool bLast );

/**
 * Retrieves the cryptographic signature that was generated with
 * NvOdmSysUpdateSign().
 *
 * @param Signature A pointer to the buffer that to be filled with the signature.
 * @param Size The size of the signature in bytes. Should be
 *      ::NVODM_SYS_UPDATE_SIGNATURE_SIZE.
 */
NvBool
NvOdmSysUpdateGetSignature( NvU8 *Signature, NvU32 Size );

/**
 * Queries whether or not partition update is allowed.
 *
 * @param binary The type of binary for update.
 * @param OldVersion The version number of the existing binary in the partition.
 * @param NewVersion The version number of the new binary to update.
 *
 * @return NV_TRUE If update is allowed, or NV_FALSE otherwise.
 */
NvBool
NvOdmSysUpdateIsUpdateAllowed(
    NvOdmSysUpdateBinaryType binary,
    NvU32 OldVersion,
    NvU32 NewVersion);

#if defined(__cplusplus)
}
#endif

/** @} */
#endif // INCLUDED_NVODM_SYSTEM_UPDATE_H
