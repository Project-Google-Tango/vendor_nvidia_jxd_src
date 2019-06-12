/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_CPU_H
#define INCLUDED_CPU_H

// System Control Register (SCTLR) definitions
#define SCTLR_AFE       (1<<29) // Access Flag Enable
#define SCTLR_V         (1<<13) // high vectors
#define SCTLR_I         (1<<12) // instruction cache
#define SCTLR_Z         (1<<11) // branch prediction
#define SCTLR_C         (1<<2)  // data cache
#define SCTLR_A         (1<<1)  // strict alignment
#define SCTLR_M         (1<<0)  // MMU enable

#define VECTOR_BASE             ( 0x9FF000 )

#define APXX_EXT_MEM_START      0x00000000
#define APXX_EXT_MEM_END        0x40000000       // always 1GB for AOS/CPU

#define APXX_MMIO_START         0x40000000
#define APXX_MMIO_END           0xFFF00000

#define TXX_EXT_MEM_START       0x80000000
#define TXX_EXT_MEM_END         0xC0000000       // always 1GB for AOS/CPU

#define TXX_MMIO_START          0x40000000
#define TXX_MMIO_END            0x80000000

#endif
