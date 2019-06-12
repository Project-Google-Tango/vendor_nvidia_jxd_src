/**
 * Copyright (c) 2007-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvbuildbct_util.h"

/*
 * nvbuildbct.h - Definitions for the nvbuildbct code.
 */

/*
 * TODO / Notes
 * - Add doxygen commentary
 */
 /*
  * Utility functions
  */

unsigned long
ICeil(unsigned long a, unsigned long b)
{
    return ((a) + (b) - 1) / (b);
}

unsigned long
Log2(unsigned long value)
{
unsigned long Result = 0;

while (value >>= 1)
{
    Result++;
}
 return Result;
}

unsigned long
ICeilLog2(unsigned long a, unsigned long b)
{
return (a + (1 << b) - 1) >> b;
}

/* Returns the smallest power of 2 >= a */
unsigned long
CeilLog2(unsigned long a)
{
unsigned long result;

result = Log2(a);
if ((1UL << result) < a) result++;

return result;
}
