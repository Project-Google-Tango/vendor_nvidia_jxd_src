/*
 * Copyright (c) 2005 - 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvfxmath.h"

/*
 * NvSFxMod is the fixed-point equivalent of the ANSI-C function fmod(). It 
 * computes dividend - trunc(dividend / divisor) * divisor, where trunc(x) = 
 * (x >= 0.0) ? floor(x) : ceil(x)). The remainder has the same sign as the
 * dividend. The result is undefined if 
 * abs(dividend) > (7FFF.FFFF * abs(divisor)).
 */
NvSFx NvSFxMod(NvSFx dividend, NvSFx divisor)
{
    NvSFx quot = dividend / divisor;
    return (dividend - divisor * quot);
}

/* 
 * NvSFxDiv returns the quotient of its parameters.  Division is exact
 * and the result is equal to the quotient of dividend and divisor
 * rounded towards 0, i.e. truncated after 16 fraction bits.  The
 * quotient is undefined if abs(dividend) > (7FFF.FFFF * abs(divisor)).
 * The divisor must satisfy abs(divisor) <= 7FFF.FFFF.
 */
NvSFx NvSFxDiv (NvSFx dividend, NvSFx divisor)
{
    return (NvSFx)((((NV_FX_LONGLONG)(dividend)) << 16) / divisor);
}

/* 
 * NvSFxRecip returns the reciprocal of its parameter.  The reciprocal
 * is exact and the result is equal to the quotient of 1 / divisor 
 * rounded towards 0, i.e. truncated after 16 fraction bits.  The
 * quotient is undefined if abs(dividend) > (7FFF.FFFF * abs(divisor)).
 */
NvSFx NvSFxRecip (NvSFx divisor)
{
    return (NvSFx)((((NV_FX_LONGLONG)(0x10000)) << 16) / divisor);
}
