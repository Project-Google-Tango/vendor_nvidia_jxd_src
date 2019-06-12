/*
 * Copyright (c) 2006 - 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file nvboot_sku_int.h
 *
 * SKU support interface for NvBoot
 *
 */

#ifndef INCLUDED_NVBOOT_SKU_INT_H
#define INCLUDED_NVBOOT_SKU_INT_H

#include "nvcommon.h"
#include "t30/nvboot_error.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/*
 * NvBootSku_SkuId -- Identifies the supported SKU
 */
typedef enum
{
    NvBootSku_IdeOn_PcieOn   = 0,
    NvBootSku_IdeOn_PcieOff  = 1,
    NvBootSku_IdeOff_PcieOn  = 2,
    NvBootSku_IdeOff_PcieOff = 3,

    // Marketing names from AP15
    NvBootSku_1 = NvBootSku_IdeOn_PcieOn,
    NvBootSku_2 = NvBootSku_IdeOn_PcieOff,
    NvBootSku_3 = NvBootSku_IdeOff_PcieOn,

    NvBootSku_SkuRange = 4,  // must be smallest power of two larger than
                             // last valid encoding

    NvBootSku_SkuId_Force32 = 0x7fffffff
} NvBootSku_SkuId;

#define NVBOOT_SKU_MASK ( ((int) NvBootSku_SkuRange) - 1 )

/**
 * Enforcing the SKU restrictions as defined in bug 398101
 * Relevant extracts from the bug comments
 *
 * fuse value description
 * ---------- -----------
 * 2'b00 full features
 * 2'b01 IDE on  / PCIe off
 * 2'b10 IDE off / PCIe on
 * 2'b11 IDE off / PCIe off
 *
 * In other words, bit 0 is a PCIe disable and bit 1 is the IDE disable.
 * This is still treated as an enumerated value for future flexibility.
 *
 * HW removal is by using sticky clock disable bits
 *
 * @param Sku identification
 *
 * @return void
 */
void
NvBootSkuEnforceSku(NvBootSku_SkuId Sku) ;

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVBOOT_SKU_INT_H
