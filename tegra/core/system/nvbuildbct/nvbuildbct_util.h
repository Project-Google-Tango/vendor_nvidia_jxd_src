/**
 * Copyright (c) 2007-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * nvbuildbct.h - Definitions for the nvbuildbct code.
 */

/*
 * TODO / Notes
 * - Add doxygen commentary
 */

#ifndef INCLUDED_NVBUILDBCT_UTIL_H
#define INCLUDED_NVBUILDBCT_UTIL_H

#include <stdlib.h> /* For definitions of exit(), rand(), srand() */

#if defined(__cplusplus)
extern "C"
{
#endif

unsigned long ICeil(unsigned long a, unsigned long b);
unsigned long Log2(unsigned long value);
unsigned long ICeilLog2(unsigned long a, unsigned long b);
unsigned long CeilLog2(unsigned long a);

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBUILDBCT_H */
