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

#define IEEE_SGL_EXPO_BIAS 127

/*
 * NvSFxFloat2Fixed converts the floating-point number f into a S15.16 fixed
 * point number. The result is identical to ((int)(f * 65536.0f + 0.5f)) for
 * f >= 0, and ((int)(f * 65536.0f + 0.5f)) for f < 0. Using this function 
 * avoids floating-point computation which is slow without FPU.
 *
 * If the conversion result underflows the S15.16 range, zero is returned. If 
 * the conversion result overflows, the smallest 32-bit integer (0x80000000) 
 * returned if f < 0, and the largest 32-bit integer (0x7FFFFFFF) is returned
 * if f > 0.
 */
NvSFx NvSFxFloat2Fixed (float f)
{
    /*
     * There is no portable, C standard compliant code to reinterprete a float 
     * as an integer. Employing a union is the safest way in my experience and
     * this 'type punning' is specifically supported by gcc.
     */
    union {
        float f;
        int i;
    } cvt;
    int res;
    int exp;
    cvt.f = f;
    exp = ((unsigned)cvt.i >> 23) & 0xff;
    /* if absolute value of number less that 2^-17, converts to 0 */
    if (exp < (IEEE_SGL_EXPO_BIAS - 17)) {
        return 0;
    } 
    /* if number >= 2^15, clamp to -2^15 or 2^15-1/65536 */
    else if (exp >= (IEEE_SGL_EXPO_BIAS + 15)) {
        return (cvt.i >> 31) ? 0x80000000 : 0x7FFFFFFF;
    } else {
        /* normalise mantissa and add hidden bit */
        res = (cvt.i << 8) | 0x80000000;
        /* shift result into position based on magnitude */
        res = ((unsigned int)res) >> (IEEE_SGL_EXPO_BIAS + 14 - exp);
        /*
         * Round to fixed-point. Overflow during rounding is impossible for 
         * float source. Because there are only 24 mantissa bits, the lsbs 
         * of res for operands close to 2^15 are zero.
         */
        res = ((unsigned int)(res + 1)) >> 1;
        /* apply sign bit */
        return (cvt.i >> 31) ? -res : res;
    }
}

/*
 * NvSFxFixed2Float converts the fixed-point number x into IEEE-754 single
 * precision floating- point number. The result is identical to x/65536.0f.
 * Using this function avoids floating-point computation which is slow 
 * without an FPU.
 */
float NvSFxFixed2Float(NvSFx x)
{
    union {
        float f;
        unsigned int i;
    } cvt;
    unsigned int u, t;
    int log2 = 0;

    if (!x) {
        return 0.0f;
    }
    cvt.i = (x < 0) ? 0xC7000000 : 0x47000000;
    t = u = (x < 0) ? -x : x;
    if (t & 0xffff0000) {
        t >>= 16;
        log2 |= 16;
    } 
    if (t & 0xff00) {
        t >>= 8;
        log2 |=  8;
    } 
    if (t & 0xf0) {
        t >>= 4;
        log2 |=  4;
    } 
    if (t & 0xC) {
        t >>= 2;
        log2 |=  2;
    } 
    if (t & 0x2) {
        log2 |=  1;
    } 
    /* compute exponent of result */
    cvt.i -= (31 - log2) << 23;
    /* normalize mantissa */
    t = (u << (31 - log2));
    /* remove mantissa hidden bit and round mantissa */
    if ((t & 0xFF) == 0x80) {
        u = (t >> 8) & 1;
    } else {
        u = (t >> 7) & 1;
    }
    cvt.i += (((t << 1) >> 9) + u);
    return cvt.f;
}


