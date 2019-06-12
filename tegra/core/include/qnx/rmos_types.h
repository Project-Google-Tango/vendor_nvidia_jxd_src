/*
 * Copyright (c) 2011-2013 NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef __RMOS_TYPES_H_INCLUDED
#define __RMOS_TYPES_H_INCLUDED

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

typedef uint8_t  rmos_u8;
typedef uint16_t rmos_u16;
typedef uint32_t rmos_u32;
typedef uint64_t rmos_u64;
typedef int8_t   rmos_s8;
typedef int16_t  rmos_s16;
typedef int32_t  rmos_s32;

typedef paddr_t  rmos_phys;

#endif
