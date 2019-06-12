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

/* (unsigned int)(32768.0*65536.0*log(1.0+(i/32.0))/(log(2.0))+0.5) */
static const unsigned int ltab[66] = {
    0x00000000, 0x02dcf2d1, 0x05aeb4dd, 0x08759c50,
    0x0b31fb7d, 0x0de42120, 0x108c588d, 0x132ae9e2,
    0x15c01a3a, 0x184c2bd0, 0x1acf5e2e, 0x1d49ee4c,
    0x1fbc16b9, 0x22260fb6, 0x24880f56, 0x26e2499d,
    0x2934f098, 0x2b803474, 0x2dc4439b, 0x30014ac6,
    0x32377512, 0x3466ec15, 0x368fd7ee, 0x38b25f5a,
    0x3acea7c0, 0x3ce4d544, 0x3ef50ad2, 0x40ff6a2e,
    0x43041403, 0x450327eb, 0x46fcc47a, 0x48f10751,
    0x4ae00d1d, 0x4cc9f1ab, 0x4eaecfeb, 0x508ec1fa,
    0x5269e12f, 0x5440461c, 0x5612089a, 0x57df3fd0,
    0x59a80239, 0x5b6c65aa, 0x5d2c7f59, 0x5ee863e5,
    0x60a02757, 0x6253dd2c, 0x64039858, 0x65af6b4b,
    0x675767f5, 0x68fb9fce, 0x6a9c23d6, 0x6c39049b,
    0x6dd2523d, 0x6f681c73, 0x70fa728c, 0x72896373,
    0x7414fdb5, 0x759d4f81, 0x772266ad, 0x78a450b8, 
    0x7a231ace, 0x7b9ed1c7, 0x7d17822f, 0x7e8d3846,
    0x80000000, 0x816fe50b
};

/* (unsigned int)(32768.0*65536.0*(exp((i/64.0)*log(2.0))-1.0)+0.5) */
static const unsigned int etab [66] = {
    0x00000000, 0x0164d1f4, 0x02cd8699, 0x043a28c4,
    0x05aac368, 0x071f6197, 0x08980e81, 0x0a14d575,
    0x0b95c1e4, 0x0d1adf5b, 0x0ea4398b, 0x1031dc43,
    0x11c3d374, 0x135a2b2f, 0x14f4efa9, 0x16942d37,
    0x1837f052, 0x19e04593, 0x1b8d39ba, 0x1d3ed9a7,
    0x1ef53261, 0x20b05110, 0x22704303, 0x243515ae,
    0x25fed6aa, 0x27cd93b5, 0x29a15ab5, 0x2b7a39b6,
    0x2d583eea, 0x2f3b78ad, 0x3123f582, 0x3311c413,
    0x3504f334, 0x36fd91e3, 0x38fbaf47, 0x3aff5ab2,
    0x3d08a39f, 0x3f1799b6, 0x412c4cca, 0x4346ccda,
    0x45672a11, 0x478d74c9, 0x49b9bd86, 0x4bec14ff,
    0x4e248c15, 0x506333db, 0x52a81d92, 0x54f35aac,
    0x5744fccb, 0x599d15c2, 0x5bfbb798, 0x5e60f482,
    0x60ccdeec, 0x633f8973, 0x65b906e7, 0x68396a50,
    0x6ac0c6e8, 0x6d4f301f, 0x6fe4b99c, 0x7281773c,
    0x75257d15, 0x77d0df73, 0x7a83b2db, 0x7d3e0c0d,
    0x80000000, 0x82c9a3e7
};

static const unsigned char lztab[64] = {
    32, 31, 31, 16, 31, 30,  3, 31, 15, 31, 31, 31, 29, 10,  2, 31,
    31, 31, 12, 14, 21, 31, 19, 31, 31, 28, 31, 25, 31,  9,  1, 31,
    17, 31,  4, 31, 31, 31, 11, 31, 13, 22, 20, 31, 26, 31, 31, 18,
     5, 31, 31, 23, 31, 27, 31,  6, 31, 24,  7, 31 , 8, 31,  0, 31
};

/*
 * NvSFxLog2 computes an approximation to the logarithm base 2 of x. The result
 * is undefined if the argument x does not satisfy 0000.0000 < x < 7FFF.FFFF,
 * i.e. argument must be a non-zero non-negative number. Over the supported
 * argument range all results either match the reference function, or are off
 * by at most +/-1/65536. No larger errors were observed in an exhaustive test.
 *
 * The result is undefined for arguments <= 0.
 */
NvSFx NvSFxLog2 (NvSFx x)
{
    int lz, f1, f2, dx, a, b, approx, t, idx; 
    /* use Robert Harley's trick for a portable leading zero count */
    lz = x;
    lz |= lz >> 1;
    lz |= lz >> 2;
    lz |= lz >> 4;
    lz |= lz >> 8;
    lz |= lz >> 16;
    lz *= 7U*255U*255U*255U;
    lz = lztab[(unsigned)lz >> 26];
    /* shift off integer bits and normalize fraction: 1 <= frac < 2 */
    t = x << (lz + 1);
    /* index table of log2 value using 5 msbs of fraction */
    idx = (unsigned)t >> (32 - 6);
    /* difference between argument and next smaller sampling point */
    dx = t - (idx << (32 - 6));
    /* fit parabola through closest 3 sampling points; find coefficients a,b */
    f1 = (ltab[idx+1] - ltab[idx]) << 1;
    f2 = (ltab[idx+2] - ltab[idx]);
    a  = f2 - f1;
    b  = f1 - a;
    /* find function value for argment by computing ((a * dx + b) * dx) */
    approx  = (int)((((NV_FX_LONGLONG)a)*dx) >> (32 - 6)) + b;
    approx  = (int)((((NV_FX_LONGLONG)approx)*dx) >> (32 - 6 + 1));
    approx = ltab[idx] + approx;
    /* combine integer and fractional parts of result */
    approx = ((15 - lz) << 16) | (((unsigned)approx) >> 15);
    return approx;
}

/*
 * NvSFxPow2 computes an approximation to 2^x. The result is undefined if the 
 * argument x is > 000E.FFFF (about 14.99998). The approximation provides 
 * about 23.5 bits of accuracy, The result is not off by more than +/-2/65536
 * (as compared to a floating-point reference converted to S15.16) if the 
 * argument is less than 0008.2000. The magnitude of the approximation error 
 * increases for larger arguments, but does not exceed 176/65536 over the 
 * entire supported argument range.
 */
NvSFx NvSFxPow2 (NvSFx x)
{
    int f1, f2, dx, a, b, approx, idx, i, f;

    /* extract integer portion; 2^i is realized as a shift at the end */
    i = (x >> 16);
    /* extract fraction f so we can compute 2^f, 0 <= f < 1 */
    f = (x & 0xffff);
    /* index table of log2 value using 6 msbs of fraction */
    idx = (unsigned)f >> (16 - 6);
    /* difference between argument and next smaller sampling point */
    dx = f - (idx << (16 - 6));
    /* fit parabola through closest 3 sampling points; find coefficients a,b */
    f1 = (etab[idx+1] - etab[idx]) << 1;
    f2 = (etab[idx+2] - etab[idx]);
    a  = f2 - f1;
    b  = f1 - a;
    /* find function value for argment by computing ((a * dx + b) * dx) */
    approx  = (int)((((NV_FX_LONGLONG)a) * dx) >> (16 - 6)) + b;
    approx  = (int)((((NV_FX_LONGLONG)approx) * dx) >> (16 - 6 + 1));
    /* combine integer and fractional parts of result. Flush underflows to 0 */
    approx = ((i < -16) ? 0 :
              ((etab[idx] + (unsigned)approx + 0x80000000U) >> (15 - i)));
    return approx;
}

/* 
 * NvSFxPow approximates A^B (where "^" denotes "raised to the power of"). 
 * In general, the result is undefined if A is not a non-zero, non-negative 
 * number, i.e. A > 0000.0000. However pow(0,0) is 0001.0000 as demanded 
 * by OpenGL-ES. The result is undefined if pow(A,B) > 7FFF.FFFF, The result
 * is also undefined if log2(A)*B is not in range [8000.0000, 7FFF.FFFF].
 *
 * While this pow() function works well in many situations (e.g. for lighting),
 * there are numerical shortcomings when it is used as a general purpose pow() 
 * function, since significantly reduced accuracy can occur for certain pairs
 * of arguments. Errors up to about 0.5% have been observed. The basic issue 
 * when using pow(x,y) = exp(y*log(x)) is that a small error in the logarithm 
 * can lead to a massive error in the function value. This effect is more 
 * pronounced with a fixed point implementation than a floating-point one. 
 * The worst errors occur when A is a number slightly less than 1, and B is 
 * a negative number of fairly large magnitude such that the result is large 
 * but does not overflow.
 */
NvSFx NvSFxPow (NvSFx x, NvSFx y)
{
    int lz, f1, f2, dx, a, b, approx, t, idx, i, f; 
    NV_FX_LONGLONG temp;

    /* use Robert Harley's trick for a portable leading zero count */
    lz = x;
    lz |= lz >> 1;
    lz |= lz >> 2;
    lz |= lz >> 4;
    lz |= lz >> 8;
    lz |= lz >> 16;
    lz *= 7U*255U*255U*255U;
    lz = lztab[(unsigned)lz >> 26];
    /* shift off integer bits and normalize fraction: 1 <= frac < 2 */
    t = x << (lz + 1);
    /* index table of log2 value using 6 msbs of fraction */
    idx = (unsigned)t >> (32 - 6);
    /* difference between argument and next smaller sampling point */
    dx = t - (idx << (32 - 6));
    /* fit parabola through closest 3 sampling points; find coefficients a,b */
    f1 = (ltab[idx+1] - ltab[idx]) << 1;
    f2 = (ltab[idx+2] - ltab[idx]);
    a  = f2 - f1;
    b  = f1 - a;
    /* find function value for argment by computing ((a * dx + b) * dx) */
    approx  = (int)((((NV_FX_LONGLONG)a)*dx) >> (32 - 6)) + b;
    approx  = (int)((((NV_FX_LONGLONG)approx)*dx) >> (32 - 6 + 1));
    approx = ltab[idx] + approx;
    /* combine integer and fractional parts of result in S4.27 format */
    approx = ((15 - lz) << 27) | (((unsigned)approx) >> 4);

    temp = (((NV_FX_LONGLONG)approx) * y) >> 11;
    i = (int)(temp >> 32);
    f = (int)(temp & 0xffffffff);

    /* index table of log2 value using 6 msbs of fraction */
    idx = (unsigned)f >> (32 - 6);
    /* difference between argument and next smaller sampling point */
    dx = f - (idx << (32 - 6));
    /* fit parabola through closest 3 sampling points; find coefficients a,b */
    f1 = (etab[idx+1] - etab[idx]) << 1;
    f2 = (etab[idx+2] - etab[idx]);
    a  = f2 - f1;
    b  = f1 - a;
    /* find function value for argment by computing ((a * dx + b) * dx) */
    approx  = (int)((((NV_FX_LONGLONG)a) * dx) >> (32 - 6)) + b;
    approx  = (int)((((NV_FX_LONGLONG)approx) * dx) >> (32 - 6 + 1));
    /* combine integer and fractional parts of result. Flush underflows to 0 */
    approx = ((((unsigned)(15 - i)) > 31) ? 
              0 : ((etab[idx] + (unsigned)approx + 0x80000000U) >> (15 - i)));

    return approx;
}
