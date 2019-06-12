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

static const unsigned char lztab[64] = 
{
    32, 31, 31, 16, 31, 30,  3, 31, 15, 31, 31, 31, 29, 10,  2, 31,
    31, 31, 12, 14, 21, 31, 19, 31, 31, 28, 31, 25, 31,  9,  1, 31,
    17, 31,  4, 31, 31, 31, 11, 31, 13, 22, 20, 31, 26, 31, 31, 18,
     5, 31, 31, 23, 31, 27, 31,  6, 31, 24,  7, 31 , 8, 31,  0, 31
};

static const unsigned int rsqrtCubeTab[96] =
{ 
    0xfa0bf8fe, 0xee6b28fa, 0xe5f024f7, 0xdaf268f3,
    0xd2f000f0, 0xc890c0ec, 0xc10378e9, 0xb9a758e6,
    0xb4da40e4, 0xadcea0e1, 0xa6f278de, 0xa279c0dc,
    0x9beb48d9, 0x97a5c4d7, 0x916340d4, 0x8d4fc8d2,
    0x895000d0, 0x8563b8ce, 0x818ac0cc, 0x7dc4e8ca,
    0x7a1200c8, 0x7671d8c6, 0x72e440c4, 0x6f6908c2,
    0x6db240c1, 0x6a523cbf, 0x670424bd, 0x6563c0bc,
    0x623028ba, 0x609ce8b9, 0x5d8364b7, 0x5bfd18b6,
    0x58fd40b4, 0x5783a8b3, 0x560e48b2, 0x533000b0,
    0x51c70caf, 0x506238ae, 0x4da4c0ac, 0x4c4c10ab,
    0x4af768aa, 0x49a6b8a9, 0x485a00a8, 0x471134a7,
    0x45cc58a6, 0x434e40a4, 0x4214f8a3, 0x40df88a2,
    0x3fade0a1, 0x3e8000a0, 0x3d55dc9f, 0x3c2f789e,
    0x3c2f789e, 0x3b0cc49d, 0x39edc09c, 0x38d2609b,
    0x37baa89a, 0x36a68899, 0x35960098, 0x34890497,
    0x34890497, 0x337f9896, 0x3279ac95, 0x31774094,
    0x30784893, 0x30784893, 0x2f7cc892, 0x2e84b091,
    0x2d900090, 0x2d900090, 0x2c9eac8f, 0x2bb0b88e,
    0x2bb0b88e, 0x2ac6148d, 0x29dec08c, 0x29dec08c,
    0x28fab08b, 0x2819e88a, 0x2819e88a, 0x273c5889,
    0x273c5889, 0x26620088, 0x258ad487, 0x258ad487,
    0x24b6d886, 0x24b6d886, 0x23e5fc85, 0x23184084,
    0x23184084, 0x224d9883, 0x224d9883, 0x21860882,
    0x21860882, 0x20c18081, 0x20c18081, 0x20000080
};

/*
 * NvSFxRecipSqrt computes an approximation to 1/sqrt(radicand). The result is
 * undefined if radicand does not satisfy 0000.0000 < radicand < 7FFF.FFF, i.e
 * the argument must be a non-zero non-negative number. Over the supported
 * argument range all results either match the reference function, or are off
 * by at most +/-1/65536. No larger errors were observed in an exhaustive test.
 *
 * No attempt is made to handle division by zero.
 */
NvSFx NvSFxRecipSqrt (NvSFx arg)
{
    unsigned NV_FX_LONGLONG prod;
    unsigned int r, s, f, lz, x = arg;

    if (!x) return x;

    /* use Robert Harley's trick for a portable cntlz */
    lz = x;
    lz |= lz >> 1;
    lz |= lz >> 2;
    lz |= lz >> 4;
    lz |= lz >> 8;
    lz |= lz >> 16;
    lz *= 7U*255U*255U*255U;
    lz = lztab[lz >> 26];
    /* normalize input argument */
    x = x << (lz & 0xfffffffe);
    /* initial approximation */
    r = f = rsqrtCubeTab[(x >> 25) - 32];
    /* first NR iteration */
    prod = ((unsigned NV_FX_LONGLONG)x) * f;
    r = ((r * 3) << 22) - (unsigned)(prod >> 32); 
    /* second NR iteration */
    prod = ((unsigned NV_FX_LONGLONG)r) * r;
    s = (unsigned)(prod >> 32);
    prod = ((unsigned NV_FX_LONGLONG)x) * s;
    f = 0x30000000 - (unsigned)(prod >> 32);
    prod = ((unsigned NV_FX_LONGLONG)f) * r; 
    r = (unsigned)(prod >> 32);
    /* denormalize result */
    return ((r >> (18 - (lz >> 1))) + 1) >> 1;
}

/*
 * NvSFxNormalize3 normalizes a 3-element input vector, i.e. produces a unit
 * length vector. There are no overflows in intermediate computation for all
 * S15.16 inputs. Each of the output elements has an error of at most one ulp
 * compared to a reference function that performs intermediate computations
 * in double precision floating-point. Zero length vectors are returned as is.
 */
void NvSFxNormalize3 (NvSFx const *iv, NvSFx *ov)
{
    unsigned NV_FX_LONGLONG prod = ((((NV_FX_LONGLONG)iv[0]) * iv[0]) + 
                                    (((NV_FX_LONGLONG)iv[1]) * iv[1]) +
                                    (((NV_FX_LONGLONG)iv[2]) * iv[2]));
    NV_FX_LONGLONG sprod;
    unsigned int r, s, f, x, lz = 0;

    r = (unsigned)(prod >> 32);
    s = (unsigned)(prod & 0xffffffff);

    lz = (r) ? r : s;

    /* use Robert Harley's trick for a portable cntlz */
    lz |= lz >> 1;
    lz |= lz >> 2;
    lz |= lz >> 4;
    lz |= lz >> 8;
    lz |= lz >> 16;
    lz *= 7U*255U*255U*255U;
    lz = lztab[lz >> 26];

    /* normalize input argument */
    f = lz & 0xfffffffe;
    if (r) {
        x = (r << f) | ((f) ? (s >> (32 - f)) : 0);
    } else {
        x = (s << f);
        lz += 32;
    }

    /* initial approximation */
    if (x) {
        r = f = rsqrtCubeTab[(x >> 25) - 32];
    }
    /* first NR iteration */
    prod = ((unsigned NV_FX_LONGLONG)x) * f;
    r = ((r * 3) << 22) - (unsigned)(prod >> 32); 
    /* second NR iteration */
    prod = ((unsigned NV_FX_LONGLONG)r) * r;
    s = (unsigned)(prod >> 32);
    prod = ((unsigned NV_FX_LONGLONG)x) * s;
    f = 0x30000000 - (unsigned)(prod >> 32);
    prod = ((unsigned NV_FX_LONGLONG)f) * r; 
    r = (unsigned)(prod >> 32);

    lz = 32 - (lz >> 1);

    /* scale inputs to produce unit vector */
    sprod = (((NV_FX_LONGLONG)iv[0]) * (int)r) >> 10;
    ov[0] = (((int)(sprod >> lz)) + 1) >> 1;
    sprod = (((NV_FX_LONGLONG)iv[1]) * (int)r) >> 10;
    ov[1] = (((int)(sprod >> lz)) + 1) >> 1;
    sprod = (((NV_FX_LONGLONG)iv[2]) * (int)r) >> 10;
    ov[2] = (((int)(sprod >> lz)) + 1) >> 1;
}

/* 
 * S15.16 fixed-point square root implemented via reciprocal square root. The
 * reciprocal square root is computed by a table-based starting approximation
 * followed by two Newton-Raphson iterations. An 8-bit approximation "r" is
 * retrieved from the table, along with the 24-bit cube of the approximation,
 * "r_cubed". The table lookup uses some most significant bits of the input 
 * argument as the index. The first NR iteration computes a refined estimate 
 * r' = 1.5 * r - x * r_cubed. The second iteration then computes the final
 * reciprocal square root as r" = 0.5 * r' * (3 - x * r' * r'). The square 
 * root is then approximated by sqrt = x * r". The remainder is computed to 
 * check whether this square root is exact and the preliminary result is
 * adjusted if necessary.
 */
NvSFx NvSFxSqrt (NvSFx arg)
{
    unsigned NV_FX_LONGLONG prod;
    unsigned int r, s, f, lz, x = arg, oldx = arg;
    int rem;

    /* use Robert Harley's trick for a portable cntlz */
    lz = x;
    lz |= lz >> 1;
    lz |= lz >> 2;
    lz |= lz >> 4;
    lz |= lz >> 8;
    lz |= lz >> 16;
    lz *= 7U*255U*255U*255U;
    lz = lztab[lz >> 26];
    /* normalize input argument */
    x = x << (lz & 0xfffffffe);
    /* initial approximation */
    r = f = rsqrtCubeTab[((unsigned)x >> 25) - 32];
    /* first NR iteration */
    prod = ((unsigned NV_FX_LONGLONG)x) * f;
    r = ((r * 3) << 22) - (unsigned)(prod >> 32);
    /* second NR iteration */
    prod = ((unsigned NV_FX_LONGLONG)r) * r;
    s = (unsigned)(prod >> 32);
    prod = ((unsigned NV_FX_LONGLONG)x) * s;
    f = 0x30000000 - (unsigned)(prod >> 32);
    prod = ((unsigned NV_FX_LONGLONG)f) * r; 
    r = (unsigned)(prod >> 32);
    /* compute sqrt(x) as x * 1/sqrt(x) */
    prod = ((unsigned NV_FX_LONGLONG)x) * r;
    r = (unsigned)(prod >> 32);
    /* denormalize and round result */
    r = ((r >> (lz >> 1)) + 0x4) >> 3;
    /* optional adjustment to get exact result (int)(sqrt(x/65536.0)*65536.0)*/
    rem = (oldx << 16) - (r * r);
    if (rem < 0) r--;
    return r;
}
