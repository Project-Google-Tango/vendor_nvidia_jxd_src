/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_AVP_H
#define INCLUDED_AVP_H

#define AVP_CACHE_PA_BASE               0x6000C000
#define AVP_CACHE_CONTROL_0             0x0
#define AVP_MMU_TLB_PA_BASE             0xf000f000
#define AVP_MMU_TLB_CACHE_WINDOW_0      0x40
#define AVP_MMU_TLB_CACHE_OPTIONS_0     0x44

#define VECTOR_BASE     0x6000F200

#define AVP_WDT_RESET   0x2F00BAD0

#define APXX_EXT_MEM_START      0x00000000
#define APXX_EXT_MEM_END        0x40000000

#define APXX_MMIO_START         0x40000000
#define APXX_MMIO_END           0xFFF00000

#define TXX_EXT_MEM_START       0x80000000
#define TXX_EXT_MEM_END         0xFFF00000

#define TXX_MMIO_START          0x40000000
#define TXX_MMIO_END            0x80000000

#define T30_AVP_CACHE_BASE      0x50040000
#define T30_MMU_MAX_SEGMENTS    32
#define T30_MMU_ATTRIBUTES      0xF

#define AVP_CACHE_LINE_SIZE     32
#define AVP_CACHE_LINE_MASK     (AVP_CACHE_LINE_SIZE - 1)

#endif
