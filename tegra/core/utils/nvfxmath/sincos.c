/*
 * Copyright (c) 2005 - 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

//
// sincos.c
//
// 16.16 implementations of sin(degree) and cos(degree)
//

#if NV_DEBUG
#include <stdio.h>  // for printf
#endif

#include "nvfxmath.h"
#include "cos_fx_table.hxx"

NvSFx NvSFxCosDFullRange(NvSFx deg)
{
    /* 
     * To avoid creation of a sudden performance cliff for arguments out of 
     * the range of NvSFxCosD, do a subtraction loop instead of a remainder 
     * operation here. At most we need 90 subtractions.
     */
    while (deg >= (360 << 16)) {
        deg -= (360 << 16);
    }
    while (deg < 0) {
        deg += (360 << 16);
    }
    return NvSFxCosD (deg);
}

NvSFx NvSFxSinDFullRange(NvSFx deg)
{
    /* 
     * To avoid creation of a sudden performance cliff for arguments out of 
     * the range of NvSFxCosD, do a subtraction loop instead of a remainder 
     * operation here. At most we need 90 subtractions.
     */
    while (deg >= (360 << 16)) {
        deg -= (360 << 16);
    }
    while (deg < 0) {
        deg += (360 << 16);
    }
    return NvSFxSinD (deg);
}

/*
 * We follow the common convention that trig functions taking arguments in
 * degrees use a "d" suffix, whereas trig functions accepting arguments in
 * radian use unadorned names.
 */

NvSFx NvSFxCosD(NvSFx deg)
{
    int index;
    int remainder;
    int cosLow, cosHigh;

    if ((unsigned int)deg >= (360 << 16)) {
#if NV_DEBUG
        printf ("NvSFxSinD (0x%08x) out of range [0000.0000, 0167.FFFF]\n", 
                deg);
#endif
        return 0;
    }

    index  = (deg >> 16);             // 0..359
    remainder = deg - (index << 16);  // 0..65535

    if (index <= 179) {
        if (index <= 89) {
            // 0 <= index <= 89
            cosLow  = NvSFx_cos_fx_table[index];      // 0..89
            cosHigh = NvSFx_cos_fx_table[index+1];    // 1..90
        } else {
            // 90 <= index <= 179
            index = 180 - index;
            cosLow  = -NvSFx_cos_fx_table[index];     //  90..1
            cosHigh = -NvSFx_cos_fx_table[index-1];   //  89..0
        }
    } else {
        if (index <= 269) {
            // 180 <= index <= 269
            index = index - 180;
            cosLow  = -NvSFx_cos_fx_table[index];     // 0..89
            cosHigh = -NvSFx_cos_fx_table[index+1];   // 1..90
        } else {
            // 270 <= index <= 359
            index = 360 - index;
            cosLow  = NvSFx_cos_fx_table[index];      // 90..1
            cosHigh = NvSFx_cos_fx_table[index-1];    // 89..0
        }
    }

    // Linearly interpolate using the remainder to get a more accurate
    // value.  Note that we don't need to do a 16.16 * 16.16 multiply here.
    // The largest value for "cosHigh - cosLow" is quite small (see
    // trailing comment in cos_fx_table.h).  For 1 degree step sizes,
    // this delta will be 572.  We can easily multiply this times
    // the 0..65535 remainder and have it fit within 32bits.
    //
    // Note that the "1" in "<<1" and ">>(16-1)" are because the cos
    // table has its values stored as shorts, not ints.
    return (cosLow<<1) + (((cosHigh - cosLow) * remainder + 0x4000) >> (16-1));
}

NvSFx NvSFxSinD(NvSFx deg)
{
    int index;
    int remainder;
    int cosLow, cosHigh;

    if ((unsigned int)deg >= (360 << 16)) {
#if NV_DEBUG
        printf ("NvSFxSinD (0x%08x) out of range [0000.0000, 0167.FFFF]\n", 
                deg);
#endif
        return 0;
    }

    // convert degrees so that we can exploit sin(x) = cos(x-90)
    // TODO: fold this calculation into the if-else cascade

    index  = (deg >> 16);             // 0..359
    remainder = deg - (index << 16);  // 0..65535

    if (index <= 179) {
        if (index <= 89) {
            // 0 <= index <= 89
            index = 90 - index;
            cosLow  = NvSFx_cos_fx_table[index];     // 90..1
            cosHigh = NvSFx_cos_fx_table[index-1];   // 89..0
        } else {
            // 90 <= index <= 179
            index = index - 90;
            cosLow  = NvSFx_cos_fx_table[index];     //  90..1
            cosHigh = NvSFx_cos_fx_table[index+1];   //  89..0
        }
    } else {
        if (index <= 269) {
            // 180 <= index <= 269
            index = 270 - index;
            cosLow  = -NvSFx_cos_fx_table[index];     // 90..1
            cosHigh = -NvSFx_cos_fx_table[index-1];   // 89..0
        } else {
            // 270 <= index <= 359
            index = index - 270;
            cosLow  = -NvSFx_cos_fx_table[index];     // 0..89
            cosHigh = -NvSFx_cos_fx_table[index+1];   // 1..90
        }
    }

    // Linearly interpolate using the remainder to get a more accurate
    // value.  Note that we don't need to do a 16.16 * 16.16 multiply here.
    // The largest value for "cosHigh - cosLow" is quite small (see
    // trailing comment in cos_fx_table.h).  For 1 degree step sizes,
    // this delta will be 572.  We can easily multiply this times
    // the 0..65535 remainder and have it fit within 32bits.
    //
    // Note that the "1" in "<<1" and ">>(16-1)" are because the cos
    // table has its values stored as shorts, not ints.
    return (cosLow<<1) + (((cosHigh - cosLow) * remainder + 0x4000) >> (16-1));
}
