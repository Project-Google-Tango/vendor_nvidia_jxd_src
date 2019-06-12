/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NV_SHA256_H__
#define __NV_SHA256_H__


#define SHA_256_HASH_SIZE (32)
#define SHA_256_CACHE_BUFFER_SIZE (1024*1024)

typedef struct _SHA_256_Data
{
    NvU64 Count;        /*  maintains the bytes of data hashed so far */
    NvU8  InputBuf[64];
    NvU32 h_state[8];   /*  maintains the values for h-variables state */
} NvSHA256_Data;


typedef struct _SHA_256_Hash
{
    NvU8 Hash[SHA_256_HASH_SIZE];   /*  stores the Hash value of the input Msg */
} NvSHA256_Hash;

/**
*   Initializes the SHA-256 engine internal state machine
*
*   @param pSHAData pointer to the SHA_Data object
*   @retval 0 on success and -1 on failure
*/
NvS32 NvSHA256_Init(NvSHA256_Data *pSHAData);

/**
*   Updates the SHA-256 engine internal state machine based on the Msg
*
*   @param pSHAData pointer to the SHA_Data object
*   @param data pointer to the pMsg
*   @param len Length of the input Msg
*   @retval 0 on success and -1 on failure
*/
NvS32 NvSHA256_Update(NvSHA256_Data *pSHAData, NvU8 *pMsg, NvU32 MsgLen);


/**
*   Updates the SHA-256 engine internal state machine based on the Msg
*
*   @param pSHAData pointer to the SHA_Data object
*   @param pSHAHash pointer to the SHA_Hash object
*   @retval 0 on success and -1 on failure
*/
NvS32 NvSHA256_Finalize(NvSHA256_Data *pSHAData, NvSHA256_Hash *pSHAHash);

#endif /*   __NV_SHA256_H__ */
