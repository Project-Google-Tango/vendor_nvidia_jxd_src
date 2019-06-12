/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 
#ifndef INCLUDED_NV_ENC_T3_H
#define INCLUDED_NV_ENC_T3_H

#include "nvassert.h"
#include "nvimage_enc.h"
#include "nvimage_enc_pvt.h"

#define NV_MNENC_ROUNDS     32
#define NV_MNENC_BLOCK_SIZE 8
#define NV_MNENC_DELTA      0x9E3779B9
#define NV_MNENC_CYPHER_KEY 0xAAAAB3aC, 0x1c2EAAAA, 0xBAAAEA6C, 0xEF4d29f8

void ImgEnc_Tegra3(NvImageEncHandle pContext);

#endif // INCLUDED_NV_ENC_T3_H