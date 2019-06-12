/*
* Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

/** @file
* @brief <b>NVIDIA Driver Development Kit:
*           NvDDK Multimedia APIs</b>
*
* @b Description: NvMM AES format.
*/

#ifndef INCLUDED_NVMM_AES_H
#define INCLUDED_NVMM_AES_H

#include "nvcommon.h"
#include "nvos.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define NUM_PACKETS     16

typedef enum {
    NvMMMAesAlgorithmID_AES_ALGO_ID_NOT_ENCRYPTED = 0,
    NvMMMAesAlgorithmID_AES_ALGO_ID_CTR_MODE,
    NvMMMAesAlgorithmID_AES_ALGO_ID_CBC_MODE,
    NvMMMAesAlgorithmID_AES_ALGO_ID_WIDEVINE_CBC_MODE,
    NvMMMAesAlgorithmID_AES_ALGO_ID_MARLIN_CBC_MODE,
    NvMMMAesAlgorithmID_AES_ALGO_ID_MARLIN_CTR_MODE,
    NvMMMAesAlgorithmID_AES_ALGO_ID_CFF_CTR_MODE,
    NvMMMAesAlgorithmID_AES_ALGO_ID_PIFF_CBC_MODE,
    NvMMMAesAlgorithmID_AES_ALGO_ID_PIFF_CTR_MODE,
    NvMMMAesAlgorithmID_Force32 = 0x7FFFFFFF
} NvMMMAesAlgorithmID;

typedef struct NvMMAesPacketRec {
    NvU32 BytesOfClearData;
    NvU32 BytesOfEncryptedData;
    NvU32 IvValid;
} NvMMAesPacket;

typedef struct NvMMAesClientMetadataRec
{
    NvU8    BufferMarker [16];
    NvU8    Iv[NUM_PACKETS][16];
    NvU32   MetaDataCount;
    NvMMMAesAlgorithmID AlgorithmID;
    NvMMAesPacket       AesPacket[NUM_PACKETS];
    NvU32   KID;    // Key slot number
    // size of struct till here is 476, pad to make it 512 bytes
    // change padding when size changes
    NvU8    Padding[36];
} NvMMAesClientMetadata;

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_AES_H


