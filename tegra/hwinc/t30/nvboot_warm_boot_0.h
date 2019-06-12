/*
 * Copyright (c) 2007 - 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * Defines the warm boot 0 information for the boot rom.
 */

#ifndef INCLUDED_NVBOOT_WARM_BOOT_0_H
#define INCLUDED_NVBOOT_WARM_BOOT_0_H

#include "t30/nvboot_config.h"
#include "t30/nvboot_hash.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Defines the recovery code header information for the boot rom.
 *
 * The recovery code immediately follows the recovery code header.
 *
 * Note that the recovery code header needs to be 16 bytes aligned to preserve
 * the alignment of relevant data for hash and decryption computations without
 * requiring extra copies to temporary memory areas.
 */
typedef struct NvBootWb0RecoveryHeaderRec
{
    /// Specifies the length of the recovery code header
    NvU32      LengthInsecure;

    /// Specifies the reserved words to maintain alignment
    NvU32      Reserved[3];

    /// Specifies the hash computed over the header and code, starting at
    /// RandomAesBlock.
    NvBootHash Hash;

    /// Specifies the random block of data which is not validated but
    /// aids security.
    NvBootHash RandomAesBlock;

    /// Specifies the length of the recovery code header
    NvU32      LengthSecure;

    /// Specifies the starting address of the recovery code in the
    /// destination area.
    NvU32      Destination;

    /// Specifies the entry point of the recovery code in the destination area.
    NvU32      EntryPoint;

    /// Specifies the length of the recovery code
    NvU32      RecoveryCodeLength;
} NvBootWb0RecoveryHeader;

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_WARM_BOOT_0_H */
