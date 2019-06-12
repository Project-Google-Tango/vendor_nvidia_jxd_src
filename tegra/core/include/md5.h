/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_MD5_H
#define INCLUDED_MD5_H

#include "nvcommon.h"

#define NV_MD5_OUTPUT_LENGTH                16
#define NV_MD5_OUTPUT_STRING_LENGTH         (2*NV_MD5_OUTPUT_LENGTH+1)

typedef struct {
    NvU32 buf[4];
    NvU32 bits[2];
    NvU8 in[64];
} MD5Context;

void MD5Init(MD5Context *context);
void MD5Update(MD5Context *context, const NvU8 *buf, NvU32 len);
void MD5Final(NvU8 digest[NV_MD5_OUTPUT_LENGTH], MD5Context *context);

#endif // INCLUDED_MD5_H
