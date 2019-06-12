/*
 * Copyright (c) 2011 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_AES_HW_H
#define INCLUDED_NVDDK_AES_HW_H

#include "nverror.h"
#include "nvcommon.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The key table length is 64 bytes
 * (This includes first upto 32  bytes key + 16 bytes original initial vector
 * and 16 bytes updated initial vector)
 */
enum {AES_HW_KEY_TABLE_LENGTH_BYTES = 64};

/**
 * Keytable array, this must be in the IRAM
 * Address must be aligned to 256 Bytes for ap15 and 4 bytes
 * for ap20.
 */
enum {AES_HW_KEY_TABLE_ADDR_ALIGNMENT = 256};


/**
 * The key Table length is 256 bytes = 64 words, (32 bit each word)
 * (This includes first 16 bytes key + 224 bytes
 * Key expantion data + 16 bytes Initial vector)
 */
enum {AES_HW_KEY_SCHEDULE_LENGTH = 64};


/**
 * The key Table length is 64 bytes = 16 words, (32 bit each word)
 */
enum {AES_HW_KEY_TABLE_LENGTH = 16};

/*
 * Key table address must be in the IRAM.
 * !!FIXME!! The keytable is in IRAM
 * but must not be allowed to overlap the BIT.
 * Correct fix is to figure out bug 498349
 */
enum {NVBL_AES_KEY_TABLE_OFFSET = 0x1d00};

/**
 * DMA Buffer size for processing the encryption and decryption with H/W
 * Each engine is allocated 32 Kilo Bytes buffer for data processing
 * This buffer is shared for both input data and out put data
 */
enum {AES_HW_DMA_BUFFER_SIZE_BYTES = 0x8000};

/**
 * Defines AES engine Max process bytes size in one go, which takes 1 msec.
 * AES engine spends about 176 cycles/16-bytes or 11 cycles/byte
 * The duration CPU can use the BSE to 1 msec, then the number of available
 * cycles of AVP/BSE is 216K. In this duration, AES can process 216/11 ~= 19 KBytes
 * Based on this AES_HW_MAX_PROCESS_SIZE_BYTES is configured to 16KB.
 */
enum {AES_HW_MAX_PROCESS_SIZE_BYTES = 0x4000};


/**
 * DMA data buffer address alignment
 */
enum {AES_HW_DMA_ADDR_ALIGNMENT = 16};

/**
 * The Initial Vector length in the 32 bit words. (128 bits = 4 words)
 */
enum {AES_HW_IV_LENGTH = 4};

/**
 * The Key length in the 32 bit words. (256 bits = 8 words)
 */
enum {AES_HW_KEY_LENGTH = 8};

/**
 * Defines Aes block length in the 32 bit words (128-bits = 4 words)
 */
enum {AES_HW_BLOCK_LENGTH = 4};

/**
 * Defines Aes block length in log2 bytes = 2^4 = 16 bytes.
 */
enum {AES_HW_BLOCK_LENGTH_LOG2 = 4};


/**
 * AES Key (128 bits).
 */
typedef struct AesHwKeyRec
{
    NvU32 key[AES_HW_KEY_LENGTH];
} AesHwKey;


/**
 * Defines AES key lengths.
 */

typedef enum
{
    AesKeySize_Invalid,

    /**
     * Defines 128-bit key length size, in bytes.
     */
    AesKeySize_128Bit = 16,

    /**
     * Defines 192-bit key length size, in bytes.
     */
    AesKeySize_192Bit = 24,

    /**
     * Defines 256-bit key length size, in bytes.
     */
    AesKeySize_256Bit = 32,
    AesKeySize_Num,
    AesKeySize_Force32 = 0x7FFFFFFF
} AesKeySize;

/**
 * AES Initial Vector (128 bits).
 */
typedef struct AesHwIvRec
{
    NvU32 iv[AES_HW_IV_LENGTH];
} AesHwIv;

/**
 * Default key schedule length is 256 bytes
 * This gets initialized based on chip.
 */
extern NvU32 g_AesHwKeySlotLengthBytes;

/**
 * Expands AES 128 bit key for encryption in the key table provided
 *
 * @param pAesKeyData pointer to the AES key data
 * @param pKeyTableBuff pointer to the keytable for key expantion
 *
 * @return None
 */
void
AesExpand128KeyEncryption(
    NvU32 *pAesKeyData,
    NvU32 *pKeyTableBuff);

/**
 * Expands AES 128 bit key for decryption in the key table provided
 *
 * @param pAesKeyData pointer to the AES key data
 * @param pKeyTableBuff pointer to the keytable for key expantion
 *
 * @return None
 */
void
AesExpand128KeyDecryption(
    NvU32 *pAesKeyData,
    NvU32 *pKeyTableBuff);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_AES_HW_H

