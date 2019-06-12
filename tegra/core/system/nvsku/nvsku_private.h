/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __SKUINFO_PRIV
#define __SKUINFO_PRIV

typedef struct NvSkuInfoRec
{
    NvU8 Version;
    NvU8 TestConfig;
    NvU16 BomPrefix;
    NvU32 Project;
    NvU32 SkuId;
    NvU16 Revision;
    NvU8 SkuVersion[2];
    NvU8 Reserved[8];
} NvSkuInfo;

#endif
