/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVAES_REF_H
#define INCLUDED_NVAES_REF_H

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define NVAES_STATECOLS 4     // number of columns in the state & expanded key
#define NVAES_KEYCOLS   4     // number of columns in a key
#define NVAES_ROUNDS   10     // number of rounds in encryption

void NvAesExpandKey(NvU8 *key, NvU8 *expkey);
void NvAesEncrypt  (NvU8 *in,  NvU8 *expkey, NvU8 *out);
void NvAesDecrypt  (NvU8 *in,  NvU8 *expkey, NvU8 *out);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVAES_REF_H
