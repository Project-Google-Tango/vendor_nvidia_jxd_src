/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __SHA_H__
#define __SHA_H__


#define SHA_1_HASH_SIZE (20)
#define SHA_1_CACHE_BUFFER_SIZE (1024*1024)

typedef struct _SHA_Data
{
    NvU64 Count;        /*  maintains the bytes of data hashed so far */
    NvU8  InputBuf[64];
    NvU32 h_state[5];   /*  maintains the values for h-variables state */
} NvSHA_Data;


typedef struct _SHA_Hash
{
    NvU8 Hash[SHA_1_HASH_SIZE];   /*  stores the Hash value of the input Msg */
} NvSHA_Hash;

/**
*   Initializes the SHA-1 engine internal state machine
*
*   @param pSHAData pointer to the SHA_Data object
*   @retval 0 on success and -1 on failure
*/
NvS32 NvSHA_Init(NvSHA_Data *pSHAData);

/**
*   Updates the SHA-1 engine internal state machine based on the Msg
*
*   @param pSHAData pointer to the SHA_Data object
*   @param data pointer to the pMsg
*   @param len Length of the input Msg
*   @retval 0 on success and -1 on failure
*/
NvS32 NvSHA_Update(NvSHA_Data *pSHAData, NvU8 *pMsg, NvU32 MsgLen);


/**
*   Updates the SHA-1 engine internal state machine based on the Msg
*
*   @param pSHAData pointer to the SHA_Data object
*   @param pSHAHash pointer to the SHA_Hash object
*   @retval 0 on success and -1 on failure
*/
NvS32 NvSHA_Finalize(NvSHA_Data *pSHAData, NvSHA_Hash *pSHAHash);

#endif /*   __SHA_H__ */
