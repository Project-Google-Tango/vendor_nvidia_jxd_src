/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVML_AES_LOCAL_H
#define INCLUDED_NVML_AES_LOCAL_H

#include "t30/nvboot_aes.h"

#if defined(__cplusplus)
extern "C" {
#endif


/**
 * The key Table length is 256 bytes = 64 words, (32 bit each word)
 * (This includes first 16 bytes key + 224 bytes
 * Key expantion data + 16 bytes Initial vector)
 */
enum {NVBOOT_AES_KEY_SCHED_TABLE_LENGTH = 64,
           NVBOOT_AES_KEY_TABLE_LENGTH = 16};

/**
 * The key Table length is 256 bytes
 * (This includes first 16 bytes key + 224 bytes
 * Key expantion data + 16 bytes Initial vector)
 */
enum {NVBOOT_AES_KEY_SCHED_TABLE_LENGTH_BYTES = 256,
            NVBOOT_AES_KEY_TABLE_LENGTH_BYTES = 64};


// As of now only 5 commands are USED for AES encryption/Decryption
#define AES_HW_MAX_ICQ_LENGTH 5


/*
 *  Defines Ucq opcodes required for AES operation
 */
typedef enum
{
    //Opcode for Block Start Engine command
    AesUcqOpcode_BlockStartEngine = 0x0E,
    //Opcode for Dma setup command
    AesUcqOpcode_DmaSetup = 0x10,
    //Opcode for Dma Finish Command
    AesUcqOpcode_DmaFinish = 0x11,
    //Opcode for Table setup command
    AesUcqOpcode_SetupTable = 0x15,
    //Opcode for WrRegToIram
    AesUcqOpcode_WrRegToIram = 0x1F,
    //Opcode for MemDMA_VD
    AesUcqOpcode_MemDMA_VD = 0x22,
    AesUcqOpcode_Force32 = 0x7FFFFFFF
} AesUcqOpcode;

/*
 *  Defines Aes command values
 */
typedef enum
{
    // command value for Aes Table select
    AesUcqCommand_TableSelect = 0x3,
    // command value for Keytable selection
    AesUcqCommand_KeyTableSelect = 0x8,
    //command value for KeySchedule selection
    AesUcqCommand_KeySchedTableSelect = 0x4,
    // command mask for ketable address mask
    AesUcqCommand_KeyTableAddressMask = 0x1FFFF,
    AesUcqCommand_Force32 = 0x7FFFFFFF
} AesUcqCommand;


/**
 * @brief Defines Aes Interactive command Queue commands Bit postions
 */
typedef enum
{
    // Defines bit postion for command Queue Opcode
    AesIcqBitShift_Opcode = 26,
    // Defines bit postion for AES Table select
    AesIcqBitShift_TableSelect = 24,
    // Defines bit postion for AES Key Table select
    AesIcqBitShift_KeyTableId = 17,
    // Defines bit postion for AES Key Table Address
    AesIcqBitShift_KeyTableAddr = 0,
    // Defines bit postion for 128 bit blocks count
    AesIcqBitShift_BlockCount = 0,
    AesIcqBitShift_Direction = 25,
    AesIcqBitShift_NumWords = 12,
    AesIcqBitShift_Force32 = 0x7FFFFFFF
} AesIcqBitShift;

/*
 * NvBootAesContext - The context structure for the AES driver.
 */
typedef struct NvBootAesContextRec
{
    // Holds the Icq commands for the AES operation
    NvU32 CommandQueueData[AES_HW_MAX_ICQ_LENGTH];
    // Icq commands length
    NvU32 CommandQueueLength;
    // Is Hashing enabled or not.
    NvBool isHashingEnabled[NvBootAesEngine_Num];
    // Flag per engine that indicates whether the orig. IV should be used.
    NvBool UseOriginalIv[NvBootAesEngine_Num];
} NvBootAesContext;



#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVML_AES_LOCAL_H


