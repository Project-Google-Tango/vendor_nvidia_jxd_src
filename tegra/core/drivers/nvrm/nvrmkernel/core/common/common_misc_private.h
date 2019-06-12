/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef COMMON_MISC_PRIVATE_H
#define COMMON_MISC_PRIVATE_H

/*
 * common_misc_private.h defines the common miscellenious private implementation
 * functions for the resource manager.
 */

#include "nvcommon.h"
#include "nvos.h"

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

typedef struct NvBootECID
{
    NvU32 ECID_0;
    NvU32 ECID_1;
    NvU32 ECID_2;
    NvU32 ECID_3;
} NvBootECID;

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // COMMON_MISC_PRIVATE_H
