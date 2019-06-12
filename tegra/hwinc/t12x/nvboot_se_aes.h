/*
 * Copyright (c) 2006 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file nvboot_se_aes.h
 *
 * Defines the parameters and data structure for SE's AES engine.
 *
 * AES is used for encryption, decryption, and signatures.
 */

#ifndef INCLUDED_NVBOOT_SE_AES_H
#define INCLUDED_NVBOOT_SE_AES_H

#include "nvcommon.h"
#include "nvboot_config.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Defines SE AES Key Slots
 */
typedef enum
{
    //Each key-slot can be configured as secure or non-secure as controlled by
    //the corresponding bit in register SE_CRYPTO_SECURITY_PERKEY.
    //Furthermore, it is possible to enable or disable direct register read/write
    //permission for keys and IV in each key-slot and this is controlled via
    //registers SE_CRYPTO_KEYTABLE_ACCESS[keyIndex].
    //See SE IAS section 3.4.1.4 Key-slots Access Control for more info.

    /// Specifies SE AES Key Slot "0"
    NvBootSeAesKeySlot_0,
    /// Specifies SE AES Key Slot "1"
    NvBootSeAesKeySlot_1,
    /// Specifies SE AES Key Slot "2"
    NvBootSeAesKeySlot_2,
    /// Specifies SE AES Key Slot "3"
    NvBootSeAesKeySlot_3,
    /// Specifies SE AES Key Slot "4"
    NvBootSeAesKeySlot_4,
    /// Specifies SE AES Key Slot "5"
    NvBootSeAesKeySlot_5,
    /// Specifies SE AES Key Slot "6"
    NvBootSeAesKeySlot_6,
    /// Specifies SE AES Key Slot "7"
    NvBootSeAesKeySlot_7,
    /// Specifies SE AES Key Slot "8"
    NvBootSeAesKeySlot_8,
    /// Specifies SE AES Key Slot "9"
    NvBootSeAesKeySlot_9,
    /// Specifies SE AES Key Slot "10"
    NvBootSeAesKeySlot_10,
    /// Specifies SE AES Key Slot "11"
    NvBootSeAesKeySlot_11,
    /// Specifies SE AES Key Slot "12"
    NvBootSeAesKeySlot_12,
    /// Specifies SE AES Key Slot "13"
    NvBootSeAesKeySlot_13,
    /// Specifies SE AES Key Slot "14"
    NvBootSeAesKeySlot_14,
    /// Specifies SE AES Key Slot "15"
    NvBootSeAesKeySlot_15,

    // Specifies max number of SE AES Key Slots
    NvBootSeAesKeySlot_Num,

    // Reserve Key slot 14 for exclusive storage of the SBK
    NvBootSeAesKeySlot_SBK = NvBootSeAesKeySlot_14,

    // Reserve Key slot 13 for TEMPORARY storage of the SBK for use by BR only.
    // To facilitate AES-CMAC hash and AES-Decryption of the BCT and Bootloader
    // as data chunks are read in from secondary storage, we use two key slots
    // loaded with the SBK. This reduces the overhead of having to switch
    // between AES-CMAC and AES decrypt operations if we use one keyslot.
    // The keyslot NvBootSeAesKeySlot_SBK_AES_CMAC_Hash MUST BE
    // cleared before Boot ROM exit!!
    NvBootSeAesKeySlot_SBK_AES_Decrypt = NvBootSeAesKeySlot_SBK,
    NvBootSeAesKeySlot_SBK_AES_CMAC_Hash = NvBootSeAesKeySlot_13,

    // Reserve Key slot 15 for exclusive storage of the SSK
    NvBootSeAesKeySlot_SSK = NvBootSeAesKeySlot_15,
} NvBootSeAesKeySlot;

/**
 * Defines the maximum length of an Initial Vector (IV) in units of
 * 32 bit words.
 */
enum {NVBOOT_SE_AES_MAX_IV_LENGTH = 8}; //todo: needed?

/**
 * Defines the length of an Initial Vector (IV) as used by the Boot ROM
 * in units of 32 bit words.
 */
enum {NVBOOT_SE_AES_IV_LENGTH = 4};

/**
 * Defines the length of an original Initial Vector (IV) as used by the Boot ROM
 * in units of 32 bit words.
 */
enum {NVBOOT_SE_AES_ORIG_IV_LENGTH = 4};

/**
 * Defines the length of an original Initial Vector (IV) as used by the Boot ROM
 * in units of bytes.
 */
enum {NVBOOT_SE_AES_ORIG_IV_LENGTH_BYTES = 16};

/**
 * Defines the length of an updated Initial Vector (IV) as used by the Boot ROM
 * in units of 32 bit words.
 */
enum {NVBOOT_SE_AES_UPDATED_IV_LENGTH = 4};

/**
 * Defines the length of an updated Initial Vector (IV) as used by the Boot ROM
 * in units of bytes.
 */
enum {NVBOOT_SE_AES_UPDATED_IV_LENGTH_BYTES = 16};

/**
 * Defines the max length of a key in units of 32 bit words.
 */
enum {NVBOOT_SE_AES_MAX_KEY_LENGTH = 8};    // 256-bits

/**
 * Defines the max length of a key in units of bytes
 */
enum {NVBOOT_SE_AES_MAX_KEY_LENGTH_BYTES = 32}; // 256-bits

/**
 * Defines the length of a 128-bit key in units 32-bit words.
 */
enum {NVBOOT_SE_AES_KEY128_LENGTH = 4};

/**
 * Defines the length of a 128-bit key in units of bytes
 */
enum {NVBOOT_SE_AES_KEY128_LENGTH_BYTES = 16};

/**
 * Defines the length of a 192-bit key in units 32-bit words.
 */
enum {NVBOOT_SE_AES_KEY192_LENGTH = 6};

/**
 * Defines the length of a 192-bit key in units of bytes
 */
enum {NVBOOT_SE_AES_KEY192_LENGTH_BYTES = 24};

/**
 * Defines the length of a 256-bit key in units 32-bit words.
 */
enum {NVBOOT_SE_AES_KEY256_LENGTH = 8};

/**
 * Defines the length of a 256-bit key in units of bytes
 */
enum {NVBOOT_SE_AES_KEY256_LENGTH_BYTES = 32};


/**
 * Defines the length of an AES block in units of 32 bit words.
 */
enum {NVBOOT_SE_AES_BLOCK_LENGTH = 4};

/**
 * Defines the length of an AES block in units of log2 bytes.
 */
enum {NVBOOT_SE_AES_BLOCK_LENGTH_LOG2 = 4};

/**
 * Defines the length of an AES block in units of bytes.
 * 128-bits = 16 bytes.
 */
enum {NVBOOT_SE_AES_BLOCK_LENGTH_BYTES = 16};

/**
 * Defines a struct for a 128-bit AES Key.
 */
typedef struct NvBootAes128KeyRec
{
    /// Specifies the key data.
    NvU32 Key[NVBOOT_SE_AES_KEY128_LENGTH];
} NvBootAes128Key;

/**
 * Defines a struct for a 128-bit AES Iv.
 */
typedef struct NvBootAes128IvRec
{
    /// Specifies the key data.
    NvU32 Iv[NVBOOT_SE_AES_KEY128_LENGTH];
} NvBootAes128Iv;

/**
 * Defines a struct for a 256-bit AES Key.
 */
typedef struct NvBootAes256KeyRec
{
    /// Specifies the key data.
    NvU32 Key[NVBOOT_SE_AES_KEY256_LENGTH];
} NvBootAes256Key;

/**
 * Defines the value of const_Rb, as specified in the AES-CMAC specification.
 * See RFC 4493: http://tools.ietf.org/html/rfc4493.
 */
enum {NVBOOT_SE_AES_CMAC_CONST_RB = 0x87};

/**
 * Define a macro to check if the MSB of the input is set.
 */
#define NVBOOT_SE_AES_CMAC_IS_MSB_SET(x)  ( (x) >> (8*sizeof(x)-1) )

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVBOOT_SE_AES_H
