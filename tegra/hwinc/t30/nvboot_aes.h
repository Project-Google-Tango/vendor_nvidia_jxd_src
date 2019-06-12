/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file nvboot_aes.h
 *
 * Defines the parameters and data structure for AES.
 *
 * AES is used for encryption, decryption, and signatures.
 */

#ifndef INCLUDED_NVBOOT_AES_H
#define INCLUDED_NVBOOT_AES_H

#include "nvcommon.h"
#include "t30/nvboot_config.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Defines AES Engine instances.
 */
typedef enum
{
    /// Specifies AES Engine "A" (in BSEV).
    NvBootAesEngine_A,

    /// Specifies AES Engine "B" (in BSEA).
    NvBootAesEngine_B,

    NvBootAesEngine_Num,
    NvBootAesEngine_Force32 = 0x7FFFFFFF
} NvBootAesEngine;

/**
 * Defines AES Key Slot instances (per AES engine).
 */
typedef enum
{
    /// Specifies AES Key Slot "0"
    //  This is an insecure slot.
    NvBootAesKeySlot_0,

    /// Specifies AES Key Slot "1"
    //  This is an insecure slot.
    NvBootAesKeySlot_1,

    /// Specifies AES Key Slot "2"
    //  This is an insecure slot.
    NvBootAesKeySlot_2,

    /// Specifies AES Key Slot "3"
    //  This is an insecure slot.
    NvBootAesKeySlot_3,

    /// Specifies AES Key Slot "4"
    //  This is a secure slot.
    NvBootAesKeySlot_4,

    /// Specifies AES Key Slot "5"
    //  This is a secure slot.
    NvBootAesKeySlot_5,

    /// Specifies AES Key Slot "6"
    //  This is a secure slot.
    NvBootAesKeySlot_6,

    /// Specifies AES Key Slot "7"
    //  This is a secure slot.
    NvBootAesKeySlot_7,

    NvBootAesKeySlot_Num,

    /// Specifies the AES Key Schedule Enable
    NvBootAesKeySlot_SchedEnb,

    NvBootAesKeySlot_Force32 = 0x7FFFFFFF
} NvBootAesKeySlot;


/**
 * Defines the maximum length of an Initial Vector (IV) in units of
 * 32 bit words.
 */
enum {NVBOOT_AES_MAX_IV_LENGTH = 8};

/**
 * Defines the length of an Initial Vector (IV) as used by the Boot ROM
 * in units of 32 bit words.
 */
enum {NVBOOT_AES_IV_LENGTH = 4};

/**
 * Defines the length of a key in units of 32 bit words.
 */
enum {NVBOOT_AES_KEY_LENGTH = 4};

/**
 * Defines the length of a key in units of bytes
 */
enum {NVBOOT_AES_KEY_LENGTH_BYTES = 16};

/**
 * Defines the length of an AES block in units of 32 bit words.
 */
enum {NVBOOT_AES_BLOCK_LENGTH = 4};

/**
 * Defines the length of an AES block in units of log2 bytes.
 */
enum {NVBOOT_AES_BLOCK_LENGTH_LOG2 = 4};


/**
 * Defines an AES Key (128 bits).
 */
typedef struct NvBootAesKeyRec
{
    /// Specifies the key data.
    NvU32 key[NVBOOT_AES_KEY_LENGTH];
} NvBootAesKey;

/**
 * Defines an AES Initial Vector (128 bits).
 */
typedef struct NvBootAesIvRec
{
    /// Specifies the initial vector data.
    NvU32 iv[NVBOOT_AES_IV_LENGTH];
} NvBootAesIv;

#if defined(__cplusplus)
}
#endif

#endif

