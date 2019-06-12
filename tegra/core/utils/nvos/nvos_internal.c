/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "nvos.h"
#include "nvos_internal.h"
#include "nvassert.h"

NvU32 NvOsHashJenkins(const void* data, NvU32 length)
{
    /* TODO [kraita 02/Dec/08] Insert proper Jenkins one at the time hash from, e.g.,
     * http://burtleburtle.net/bob/hash/doobs.html 
     * Once we have permission to do so.
     */
    NvU32 hash = 0;
    NvU32 shift = 0;
    NvU32 i;
    const NvU8* k = (const NvU8*)data;

    for (i=0; i < length; i++)
    {
        hash = (hash + ((k[i] << shift) | k[i]) * 31);
        shift = (shift + 6) & 0xf;        
    }

    return hash;
}

static NvU32 NvOsBitrev32(NvU32 x)
{
#if defined(__ARM_ARCH_7A__) && defined(__GNUC__)
    __asm__ volatile("rbit %[dst], %[src]" \
            : [dst] "=r" (x) \
            : [src] "r" (x) );
#else
#if defined(__GNUC__) && __GNUC__ >= 4
    x = __builtin_bswap32(x);
#else
    x = ((x >> 16) & 0x0000ffff) ^ ((x & 0x0000ffff) << 16);
    x = ((x >>  8) & 0x00ff00ff) ^ ((x & 0x00ff00ff) <<  8);
#endif
    x = ((x >> 4) & 0x0f0f0f0f) ^ ((x & 0x0f0f0f0f) << 4);
    x = ((x >> 2) & 0x33333333) ^ ((x & 0x33333333) << 2);
    x = ((x >> 1) & 0x55555555) ^ ((x & 0x55555555) << 1);
#endif
    return x;
}

static NvU32 NvOsHashMWC(const void *data, NvU32 length)
{
    /* Simple hash based on Marsaglia's multiply-with-carry PRNG.
     * Passed the smhasher test suite after choosing the multiplier more
     * carefully and adding some tempering for avalanche tests.
     *
     * TODO: extend to consume more bits per round (if this is done without
     * further tuning it results in some weakness).
     */
    NvU32 hash = 0xeb88c796;
    NvU32 carry = 0x3b59f3fb;
    NvU32 i;
    const NvU8 *k = (const NvU8*)data;

    for (i=0; i < length; i++)
    {
        NvU64 x;
        hash ^= k[i];
        x = 0xf1da370bULL * hash + carry;
        hash = (NvU32)x;
        carry = (NvU32)(x >> 32);
    }

    /* Arbitrary 1:1 transform to spread the bits around */
    NvOsBitrev32(hash);
    hash *= 0x56763b9f;
    NvOsBitrev32(hash);

    /* Three additional rounds for good avalanche effect */
    {
        NvU64 x;
        x = 0xf1da370bULL * hash + carry;
        hash = (NvU32)x;
        carry = (NvU32)(x >> 32);
        x = 0xf1da370bULL * hash + carry;
        hash = (NvU32)x;
        carry = (NvU32)(x >> 32);
        hash = 0xf1da370b * hash + carry;
    }

    return hash;
}

NvError NvOsSharedMemPath(const char *key, char *output, NvU32 length)
{
    const char prefix[] = "/NvOsSharedMem.";
    NvU32 keylen = strlen(key);
    NvU32 hash0 = NvOsHashMWC(key, keylen);
    NvU32 hash1 = NvOsHashJenkins(key, keylen);

    if (length < sizeof(prefix) + 2 * 8)
        return NvError_BadParameter;

    NvOsSnprintf(output, length, "%s%08x%08x", prefix, hash0, hash1);

    return NvSuccess;
}
