/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NV_SECURE_BLOB_H__
#define __NV_SECURE_BLOB_H__


/*  This is how the signed blob is going to look like   */
/*  |----------------------------------| */
/*  |         SIGNED_BLOB_HEADER       | */
/*  |----------------------------------| */
/*  |            ACTUAL BLOB           | */
/*  |----------------------------------| */
/*  |     SIGNATURE DATA(256Bytes)     | */
/*  |----------------------------------| */

/*  This is how SIGNED_BLOB_HEADER going to look like */
/*  |----------------------------------------------| */
/*  |   MAGIC STRING i.e. "-SIGNED-BY-SIGNBLOB-"   | */
/*  |----------------------------------------------| */
/*  |            SIZE OF THE ACTUAL BLOB           | */
/*  |----------------------------------------------| */
/*  |          SIZE OF THE SIGNATURE DATA          | */
/*  |----------------------------------------------| */


#define SIGNED_UPDATE_MAGIC       "-SIGNED-BY-SIGNBLOB-"
#define SIGNED_UPDATE_MAGIC_SIZE  20

struct __SignedUpdateHeader
{
    NvU8 MAGIC[SIGNED_UPDATE_MAGIC_SIZE];
    NvU32 ActualBlobSize;
    NvU32 SignatureSize;
};

typedef struct __SignedUpdateHeader NvSignedUpdateHeader;


#endif  /*  #ifdef __NV_SECURE_BLOB_H__ */
