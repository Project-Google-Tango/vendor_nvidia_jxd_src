/*
 * Copyright (c) 2010-2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <time.h>
// FIXME: for HC MR1 or later, enable <linux/nvhdcp.h> and remove "nv_hdcp.h"
// #include <linux/nvhdcp.h>
#include "nv_hdcp.h"
#include "sha1.h"
#include "hdcp_up.h"
#include "hdcp_up_priv.h"

/* always parse the print statments */
#ifndef DEBUG
#define DEBUG 0
#endif
#define DEBUG_PRINT(x) do { if(DEBUG) { printf x; } } while(0)

#define BIT( bits, x ) \
    (((bits) >> (x)) & 1)

/* macros for accessing bit fields */
#define FLD_BASE(fld)           (0?fld)
#define FLD_EXTENT(fld)         (1?fld)
#define FLD64_SHIFTMASK(fld) \
    (0xFFFFFFFFFFFFFFFFULL >> (63 - (FLD_EXTENT(fld) - FLD_BASE(fld))) \
        << FLD_BASE(fld))
#define FLD32_SHIFTMASK(fld) \
    (0xFFFFFFFF >> (31 - (FLD_EXTENT(fld) - FLD_BASE(fld))) << FLD_BASE(fld))

/* 64-bit get and set accessors */
#define FLD64_SET(n,type,part,v) \
    (((__u64)(v) << FLD_BASE(type##_##part)) | \
        ((~FLD64_SHIFTMASK(type##_##part)) & (n)))
#define FLD64_GET(n,type,part) \
    ((FLD64_SHIFTMASK(type##_##part) & (n)) >> FLD_BASE(type##_##part))

/* 32-bit get and set accessors */
#define FLD32_SET(n,type,part,v) \
    (((__u32)(v) << FLD_BASE(type##_##part)) | \
        ((~FLD32_SHIFTMASK(type##_##part)) & (n)))
#define FLD32_GET(n,type,part) \
    ((FLD32_SHIFTMASK(type##_##part) & (n)) >> FLD_BASE(type##_##part))

typedef __u64 __u40;
typedef __u64 __u56;

// The Status of the Attach-Point (HDCP-capable or other)
#define HDCP_STATUS_ENCRYPTING               0x0001
#define HDCP_STATUS_REPEATER                 0x0002

#define HDCP_DEV_PATH "/dev/nvhdcp"

typedef struct tegra_nvhdcp_packet HdcpPacket;

typedef struct
{
    int fb_index;           // instance number of the fb device
    int fd;                 // file descriptor for the target fb device
    void *glob;             // linked client key glob
    HdcpPacket pkt;         // kernel ioctl params
    __u64 last_An;
} HdcpClient;

static void DebugPrintHdcpContext(char * string, HdcpPacket *pkt);

typedef struct HdcpOneWayStateRec
{
    __u32 lfsr[4];
    __u8 shuffle[4][2];
} HdcpOneWayState;

static __u32 s_polynomial_taps[4][6] =
{
    { 22, 19, 15, 11, 8, 5 },
    { 23, 20, 17, 13, 9, 6 },
    { 25, 22, 17, 14, 11, 7 },
    { 26, 23, 20, 16, 12, 7 },
};

static __u32 s_combiner_taps[4][3] =
{
    { 7, 14, 22 },
    { 7, 15, 23 },
    { 8, 16, 25 },
    { 8, 17, 26 },
};

static __u32 s_lfsr_lengths[] =
{
    23, 24, 26, 27
};

/**
 * One Way functions. See section 3 of the HDCP Upstream Specification.
 */
static void
HdcpOneWayInit(char mode, HdcpOneWayState *state, __u64 key, __u64 data)
{
    __u32 i;

    #define MASK(val, start, end) \
        (__u32)((((__u64)(val)) << (63 - (start))) >> (63 - ((start) - (end))))

    if( mode == 'A' )
    {
        state->lfsr[3] = MASK(data, 39, 35) << 22
            | MASK(data, 34, 28) << 14
            | MASK(key, 55, 42);

        state->lfsr[2] = MASK(data, 27, 24) << 22
            | MASK(data, 23, 17) << 14
            | MASK(key, 41, 28);

        state->lfsr[1] = MASK(data, 16, 12) << 19
            | MASK(data, 11, 8) << 14
            | MASK(key, 27, 14);

        state->lfsr[0] = MASK(data, 7, 5) << 20
            | MASK(data, 4, 0) << 14
            | MASK(key, 13, 7) << 7
            | MASK(key, 6, 0);
    }
    else if( mode == 'B' )
    {
        state->lfsr[3] = MASK(data, 34, 30) << 22
            | MASK(data, 29, 23) << 14
            | MASK(key, 48, 35);

        state->lfsr[2] = MASK(data, 22, 19) << 22
            | MASK(data, 18, 12) << 14
            | MASK(key, 34, 21);

        state->lfsr[1] = MASK(data, 11, 7) << 19
            | MASK(data, 6, 3) << 14
            | MASK(key, 20, 7);

        state->lfsr[0] = MASK(data, 2, 0) << 20
            | MASK(data, 39, 35) << 14
            | MASK(key, 6, 0) << 7
            | MASK(key, 55, 49);
    }

    state->lfsr[3] |= (((state->lfsr[3] >> 9) & 1) ^ 1) << 21;
    state->lfsr[2] |= (((state->lfsr[2] >> 8) & 1) ^ 1) << 21;
    state->lfsr[1] |= (((state->lfsr[1] >> 5) & 1) ^ 1) << 18;
    state->lfsr[0] |= (((state->lfsr[0] >> 10) & 1) ^ 1) << 19;

    for( i = 0; i < 4; i++ )
    {
        state->shuffle[i][0] = 0;
        state->shuffle[i][1] = 1;
    }

    #undef MASK
}

static void HdcpLfsr(HdcpOneWayState *state, __u8 idx, __u8 out[3])
{
    __u32 len = s_lfsr_lengths[idx];
    __u32 *comb = s_combiner_taps[idx];
    __u32 *poly = s_polynomial_taps[idx];
    __u32 i;
    __u32 lfsr;
    __u8 xor;

    lfsr = state->lfsr[idx];

    for( i = 0; i < 3; i++ )
    {
        out[i] = (__u8)BIT( lfsr, comb[i] );
    }

    xor = (__u8)( BIT( lfsr, poly[0] ) ^
          BIT( lfsr, poly[1] ) ^
          BIT( lfsr, poly[2] ) ^
          BIT( lfsr, poly[3] ) ^
          BIT( lfsr, poly[4] ) ^
          BIT( lfsr, poly[5] ) );

    lfsr = (lfsr << 1) & ((1ULL << len) - 1);

    state->lfsr[idx] = lfsr | xor;
}

static __u8 HdcpShuffle(HdcpOneWayState *state, __u8 idx, __u8 din, __u8 s)
{
    __u8 out;

    /* if s, output is B, else output is A.
     * if s, B = A, A = din, else A = B, B = din;
     */

    out = state->shuffle[idx][s & 1];
    state->shuffle[idx][s & 1] = state->shuffle[idx][s ^ 1];
    state->shuffle[idx][s ^ 1] = din;

    return out;
}

static __u8 HdcpCombiner(HdcpOneWayState *state, __u8 lfsr[4][3])
{
    __u32 i;
    __u8 tmp;

    tmp = lfsr[0][0] ^ lfsr[1][0] ^ lfsr[2][0] ^ lfsr[3][0];
    for( i = 0; i < 4; i++ )
    {
        tmp = HdcpShuffle( state, i, tmp, lfsr[i][1] );
    }

    return tmp ^ lfsr[0][2] ^ lfsr[1][2] ^ lfsr[2][2] ^ lfsr[3][2];
}

static __u8 HdcpOneWayClk(HdcpOneWayState *state)
{
    __u8 lfsr[4][3];
    __u32 i;

    for( i = 0; i < 4; i++ )
    {
        HdcpLfsr( state, i, lfsr[i] );
    }

    return HdcpCombiner( state, lfsr );
}

static __u64 HdcpOneWay(char mode, __u32 bits, __u64 key, __u64 data )
{
    HdcpOneWayState state;
    __u64 ret = 0;
    __u32 i;

    HdcpOneWayInit( mode, &state, key, data );
    /* need a 32 clock warm-up */
    for( i = 0; i < bits + 32; i++ )
    {
        ret >>= 1;
        ret |= (__u64)HdcpOneWayClk( &state ) << (bits - 1);
    }

    return ret;
}

/**
 * from Table A-1 -- Facsimile Device Keys (keyset C1) in the HDCP Upstream
 * Specification.
 */
static __u64 s_Cksv = 0xb86646fc8cULL;

typedef struct
{
    unsigned char S[256];
    __u32 i;
    __u32 j;
} RC4;

static __u8 *s_glob;
static __u64 s_glob_Header;
static __u64 s_glob_C;
static __u64 s_glob_D;
static RC4 s_glob_rc4;
static RC4 s_rnd_rc4;
static __u64 s_key[4] =
    { 0x75216521ULL,
      0x61536781ULL,
      0x35689165ULL,
      0x27891652ULL
    };

static void rc4_init(RC4 * st, unsigned char *key, __u32 klen)
{
    __u32 i, j;
    unsigned char tmp;

    st->i = 0;
    st->j = 0;

    for( i = 0; i < 256; i++ )
    {
        st->S[i] = i;
    }

    for( i = 0, j = 0; i < 256; i++ )
    {
        j = (j + key[i % klen] + st->S[i]) & 255;
        tmp = st->S[i];
        st->S[i] = st->S[j];
        st->S[j] = tmp;
    }
}

static void rc4_out(RC4 * st, void *out, __u32 len)
{
    unsigned char tmp;
    __u32 i;

    for( i = 0; i < len; i++ )
    {
        st->i = (st->i + 1) & 255;
        st->j = (st->j + st->S[st->i]) & 255;

        tmp = st->S[st->j];
        st->S[st->j] = st->S[st->i];
        st->S[st->i] = tmp;
        ((unsigned char*)out)[i] = st->S[(st->S[st->i] + st->S[st->j]) & 255];
    }
}

/**
 * This is a sample how to generate random number for Cn.
 * Feel free to improve it for the final production solution.
 */
static __u64 GenerateRnd(void)
{
    struct timeval tv;
    __u64 rnd;

    rc4_out(&s_rnd_rc4, &rnd, sizeof(__u64));
    gettimeofday(&tv, NULL);
    rnd ^= (__u64) tv.tv_sec * (__u64) tv.tv_usec;

    return rnd;
}

#define WRITE8( mem, offset, val ) \
    do { \
        *((__u8 *)(mem) + offset) = (val); \
    } while( 0 );

#define READ56( mem, offset, ret ) \
    do { \
        __u64 x; \
        __u8 *tmp = (__u8 *)(mem) + offset; \
        x = (__u64)( tmp[0] | ((__u64)tmp[1] << 8) | \
            ((__u64)tmp[2] << 16) | ((__u64)tmp[3] << 24) | \
            ((__u64)tmp[4] << 32) | ((__u64)tmp[5] << 40) | \
            ((__u64)tmp[6] << 48) ); \
        *(ret) = x; \
    } while( 0 );

#define READ64( mem, offset, ret ) \
    do { \
        *(ret) = *((__u64 *)((__u8 *)(mem) + offset)); \
    } while( 0 );

#define DEBUG_KEYS 0

static int HdcpDecryptGlob(__u32 size, void *mem)
{
    __u32 i;
    __u64 ksv;
    __u8 *tmp;
    __u32 offset;

    if( size != HDCP_GLOB_SIZE )
    {
        DEBUG_PRINT(( "glob size mismatch\n" ));
        return 0;
    }

    tmp = (__u8 *)s_glob;

    rc4_init( &s_glob_rc4, (unsigned char *)s_key, sizeof(s_key) );
    for( i = 0; i < size; i++ )
    {
        __u8 val;
        rc4_out( &s_glob_rc4, &val, 1 );
        val ^= tmp[i];
        WRITE8( mem, i, val );
    }

    offset = 0;
    READ64( mem, offset, &s_glob_Header );
    offset += 8;

    if( s_glob_Header != 1 )
    {
        return 0;
    }

    READ64( mem, offset, &s_glob_C );
    offset += 8;
    READ64( mem, offset, &s_glob_D );
    offset += 8;

    /* ksv */
    READ64( mem, offset, &ksv );
    offset += 8;

    if( DEBUG_KEYS )
    {
        printf( "ksv: 0x%llx\n", ksv );
    }
    s_Cksv = ksv;

    if( DEBUG_KEYS )
    {
        /* keys */
        for( i = 0; i < 40; i++ )
        {
            __u64 key = 0;
            READ56( mem, offset, &key );
            offset += 7;

            printf( "key: 0x%llx\n", key );
        }
    }

    return 1;
}

static HdcpGetGlobType HdcpGetGlob;
static HdcpReleaseGlobType HdcpReleaseGlob;

static void * HdcpGetGlobDefault(__u32 *size)
{
    /**
     * rc4 encrypted facsimile keys, plus the ksv and a header.
     */
    static __u8 facsimile_keys[] =
    {
    0x0e, 0xc7, 0x78, 0x30, 0x59, 0x36, 0xa9, 0x6a, 0x75, 0x42, 0x61, 0x40,
    0xa8, 0x13, 0x44, 0xf8, 0x9b, 0xe6, 0x6b, 0xbc, 0xf0, 0x2c, 0xda, 0x20,
    0x07, 0x80, 0x30, 0x2b, 0xb8, 0x29, 0x60, 0x56, 0xbc, 0x9f, 0x27, 0x06,
    0x45, 0x48, 0xd0, 0x93, 0x8f, 0xce, 0xfa, 0x2b, 0xad, 0x6e, 0x13, 0x20,
    0x82, 0x19, 0xa7, 0xa5, 0x4a, 0x58, 0xd3, 0x1f, 0xbd, 0x4b, 0xb4, 0x28,
    0xaf, 0xad, 0x36, 0xc1, 0xde, 0xfd, 0x21, 0x80, 0xdc, 0xe6, 0x7a, 0xbd,
    0xee, 0x82, 0x3b, 0xa0, 0x49, 0xb4, 0x1d, 0x2a, 0xaf, 0x63, 0x85, 0x89,
    0xa8, 0x57, 0x39, 0x11, 0x60, 0x79, 0x02, 0x71, 0x9d, 0x2c, 0xff, 0x3f,
    0x47, 0xf5, 0xd5, 0xf7, 0x01, 0x55, 0x93, 0x6f, 0x46, 0x91, 0x75, 0x57,
    0x6c, 0x8a, 0xf5, 0xfa, 0xbe, 0x76, 0xdd, 0xc8, 0xbf, 0xf1, 0xe7, 0x2c,
    0xbc, 0xa4, 0x63, 0xd7, 0x93, 0xc6, 0x59, 0xb0, 0x7b, 0x98, 0xd0, 0x8a,
    0xb5, 0xd5, 0xe7, 0x92, 0xac, 0x08, 0x6e, 0xb5, 0x77, 0xbb, 0x77, 0x28,
    0x7c, 0x9c, 0x79, 0x47, 0x5b, 0x02, 0xa6, 0x79, 0xdf, 0x15, 0x12, 0xa7,
    0x98, 0x1f, 0x0b, 0xdf, 0x2f, 0xb9, 0x3f, 0x8b, 0x7b, 0xcd, 0xe3, 0xe5,
    0xd7, 0x8f, 0xaf, 0x5a, 0x4a, 0xc0, 0xac, 0xbf, 0x96, 0x93, 0x53, 0xd9,
    0x7c, 0x00, 0x70, 0xf6, 0xb2, 0xe7, 0x79, 0x38, 0x26, 0xff, 0xec, 0xf9,
    0xde, 0x5e, 0x8b, 0xe7, 0xd5, 0x4c, 0x21, 0x4f, 0x57, 0x25, 0xef, 0x02,
    0x39, 0xbc, 0x91, 0xf9, 0x88, 0x4e, 0x12, 0x54, 0xd0, 0x72, 0xf4, 0x6b,
    0x96, 0x25, 0x4f, 0xdb, 0xee, 0x67, 0xf6, 0x84, 0x71, 0x19, 0xe2, 0x7c,
    0x2b, 0x2b, 0x84, 0x72, 0x56, 0x06, 0xd4, 0xa8, 0x57, 0x32, 0x44, 0xf7,
    0x7e, 0xef, 0xcc, 0x70, 0xe3, 0xe9, 0xa1, 0x86, 0x0b, 0x58, 0x31, 0x4d,
    0xab, 0xff, 0xb5, 0x8b, 0x96, 0x9d, 0xb6, 0xf1, 0x75, 0x2d, 0x9d, 0xbc,
    0xad, 0xb9, 0x80, 0x88, 0x0c, 0xc4, 0x10, 0x74, 0xcb, 0x05, 0x48, 0xef,
    0x77, 0xe8, 0x4b, 0x14, 0x75, 0xe0, 0x44, 0x18, 0x50, 0x3a, 0x8c, 0xa8,
    0x9f, 0x6d, 0x69, 0x0b, 0x58, 0x7f, 0xb5, 0xaf, 0x27, 0x5c, 0x26, 0x29,
    0xc4, 0x12, 0x9c, 0x93, 0x27, 0x14, 0x49, 0xa8, 0xbe, 0xf0, 0xf1, 0xd4,
    };

    *size = sizeof(facsimile_keys);
    return facsimile_keys;
}

static void HdcpReleaseGlobDefault(void *glob)
{
    // do nothing
}

static int HdcpGetAdptation(void)
{
    HdcpGetGlob = HdcpGetGlobDefault;
    HdcpReleaseGlob = HdcpReleaseGlobDefault;

    // TODO: Client should find a secure way to pass glob in for production keys.
    return 1;
}

static void * HdcpGlobOpen(void)
{
    void *mem = 0;
    __u32 size = HDCP_GLOB_SIZE;
    int status;

    status = HdcpGetAdptation();
    if ( !status )
    {
        return NULL;
    }

    mem = malloc( size );
    if( !mem )
    {
        DEBUG_PRINT(( "glob memory alloc failed" ));
        goto fail;
    }

    s_glob = HdcpGetGlob( &size );
    if( !s_glob )
    {
        DEBUG_PRINT(( "glob is null" ));
        goto fail;
    }

    status = HdcpDecryptGlob( size, mem );
    if( !status )
    {
        /* make sure to not leak the glob */
        DEBUG_PRINT(( "glob decrypt failed" ));
        goto fail;
    }

    goto clean;

fail:
    free( mem );
    mem = 0;

clean:
    return mem;
}

static void HdcpGlobClose(void *mem)
{
    __u32 size = HDCP_GLOB_SIZE;

    if( s_glob )
    {
        HdcpReleaseGlob( s_glob );
        s_glob = 0;
        s_glob_C = 0;
        s_glob_D = 0;
        s_Cksv = 0;
    }

    if( mem )
    {
        memset( mem, 0, size );
        free( mem );
    }
}

static __u64 HdcpSumKu(__u64 Dksv, void *mem)
{
    __u64 Ku;
    __u32 i, keyidx;

    /* KU -- sum Ckeys over Dksv */

    keyidx = HDCP_KEY_OFFSET;
    Ku = 0;
    for( i = 0; i < 40; i++ )
    {
        if( Dksv & (1ULL << i) )
        {
            __u64 key = 0;
            READ56( mem, keyidx + (i * 7), &key );
            Ku += key;
            Ku = Ku & ((1ULL << 56)-1);
        }
    }
    Ku = ((Ku + s_glob_D) * s_glob_C) % 0x100000000000000ULL;

    return Ku;
}

/* optional S debugging */
#define TEST_S_VECTORS  (0)

static __u64 s_Dksv_Test = 0xfc5d32906cULL;
static __u64 s_Status_Test = 0x1105ULL;
static __u64 s_Cn_Test = 0x2c72677f652c2f27ULL;
static __u64 s_Bksv_Test = 0xe72697f401ULL;
static __u64 s_An_Test = 0x34271c130cULL;
static __u64 s_Cs_Test = 0x0000000001ULL;
static __u64 s_Ku_Test = 0xa25321f0ee8d21ULL;
static __u64 s_K1_Test = 0xeb09adb5f6dc25ULL;
static __u64 s_K2_Test = 0x71ebb3cc7693d4ULL;
static __u64 s_K3_Test = 0xfd27048ba34cc4ULL;
static __u64 s_K4_Test = 0x2e02408cb8cf44ULL;
static __u64 s_Kp_Test = 0xb1c0a2a4d66570ULL;

static __u64 HdcpCalculateKp(__u64 Dksv, __u64 Bksv, __u64 An, __u64 Cs,
    __u64 Cn, __u64 Status, void *mem, int bUseCs )
{
    __u64 Ku;
    __u64 Kp, K1, K2, K3, K4;
    __u64 status;

    if( TEST_S_VECTORS )
    {
        Dksv = s_Dksv_Test;
        Bksv = s_Bksv_Test;
        An = (s_An_Test << 24);
        Cn = s_Cn_Test;
        Cs = s_Cs_Test;
        status = s_Status_Test;
        bUseCs = 1;
    }
    else
    {
        status = Status & 0xFFFF;
    }

    /* status || msb24(Cn) */
    status = (status << 24) | (Cn >> 40);

    Ku = HdcpSumKu( Dksv, mem );

    /* K1 -- oneWay-A56(Ku, lsb40(Cn)) */
    K1 = HdcpOneWay( 'A', 56, Ku, Cn & ((1ULL << 40)-1));

    /* K2 -- oneWay-A56(K1, Bksv) */
    K2 = HdcpOneWay( 'A', 56, K1, Bksv );

    /* K3 -- oneWay-A56(K2, msb40(An)) */
    K3 = HdcpOneWay( 'A', 56, K2, An >> 24 );

    /* K4 -- oneWay-A56(K3, status || msb24(Cn)) */
    K4 = HdcpOneWay( 'A', 56, K3, status );

    if( bUseCs )
    {
        /* Kp -- oneWay-A56(K4, Cs) */
        Kp = HdcpOneWay( 'A', 56, K4, Cs );
    }
    else
    {
        Kp = K4;
    }

    if( TEST_S_VECTORS )
    {
        if( Ku != s_Ku_Test ) DEBUG_PRINT(( "Ku failed\n" ));
        if( K1 != s_K1_Test ) DEBUG_PRINT(( "K1 failed\n" ));
        if( K2 != s_K2_Test ) DEBUG_PRINT(( "K2 failed\n" ));
        if( K3 != s_K3_Test ) DEBUG_PRINT(( "K3 failed\n" ));
        if( K4 != s_K4_Test ) DEBUG_PRINT(( "K4 failed\n" ));
        if( Kp != s_Kp_Test ) DEBUG_PRINT(( "Kp failed\n" ));
    }

    return Kp;
}

/* optional M debugging */
#define TEST_M_VECTORS  (0)

static __u64 s_K5_Test = 0x5bc1db127f1e27ULL;
static __u64 s_Ke_Test = 0xda4245977574bf86ULL;
static __u64 s_Mprime_Test = 0x8a0d9ab350ca4152ULL;
static __u64 s_M0_Test_0 = 0x504fdf2425befed4ULL;

static int HdcpCalculateM(__u64 Cn, __u64 Dksv, __u64 Mprime, __u64 *M0,
    void *mem)
{
    __u64 Ku;
    __u64 K5;
    __u64 Ke;

    if( TEST_M_VECTORS )
    {
        Dksv = s_Dksv_Test;
        Mprime = s_Mprime_Test;
    }

    Ku = HdcpSumKu( Dksv, mem );

    if( TEST_M_VECTORS && (Ku != s_Ku_Test ) )
    {
        DEBUG_PRINT(( "Ku failed\n" ));
        goto fail;
    }

    if( DEBUG_KEYS )
    {
        printf( "Dksv: 0x%llx Ku: 0x%llx\n", Dksv, Ku );
    }

    /* K5 -- oneWay-B56(Ku, lsb40(Cn)) */
    K5 = HdcpOneWay( 'B', 56, Ku, Cn & ((1ULL << 40)-1) );

    if( TEST_M_VECTORS && (K5 != s_K5_Test ) )
    {
        DEBUG_PRINT(( "K5 failed\n" ));
        goto fail;
    }

    /* Ke -- oneWay-B64(K5, msb24(Cn) */
    Ke = HdcpOneWay( 'B', 64, K5, Cn >> 40 );

    if( TEST_M_VECTORS && (Ke != s_Ke_Test ) )
    {
        DEBUG_PRINT(( "Ke failure\n" ));
        goto fail;
    }

    *M0 = Mprime ^ Ke;

    if( TEST_M_VECTORS && (*M0 != s_M0_Test_0 ) )
    {
        DEBUG_PRINT(( "M0 failure\n" ));
        goto fail;
    }

    return 1;

fail:
    return 0;
}

/* optional V debugging */
#define TEST_V_PRIME_VECTORS    (0)

static __u64 s_KsvFifo_Test[] =
{
    0x35796a172eULL,
    0x478e71e20fULL,
    0x74e85397a6ULL,
};

static __u16 s_Bstatus_Test = 0x0203;
static __u64 s_M0_Test_1 = 0x372d3dce38bbe78fULL;

static __u32 s_Vprime_Test[] =
{
    0x0fcbd586,
    0xefc107ef,
    0xccd70a1d,
    0xb1186dda,
    0x1fb3ff5e,
};

static int
ValidateLink_Core(__u64 Cn, __u64 Cksv, __u64 Dksv, __u64 Mprime, __u64 *KsvFifo,
    __u32 DeviceCount, __u32 *Vprime, __u16 Bstatus, void *glob)
{
    __u64 M0;
    __u32 V[5];
    __u8 *msg = 0;
    __u8 *ptr = 0;
    __u32 size = 0;
    __u32 i, j;
    int b;

    if (TEST_V_PRIME_VECTORS)
    {
        DeviceCount = sizeof(s_KsvFifo_Test) / sizeof(s_KsvFifo_Test[0]);
        KsvFifo = s_KsvFifo_Test;
        Bstatus = s_Bstatus_Test;
        M0 = s_M0_Test_1;

        memcpy(Vprime, s_Vprime_Test, sizeof(s_Vprime_Test));
    }

    /* each device is 5 bytes (ksv fifo), 2 for Bstatus, 8 for M0 */
    size = (DeviceCount * 5) + 2 + 8;

    msg = malloc(size);
    if (NULL == msg)
    {
        goto fail;
    }

    ptr = msg;
    for (i = 0; i < DeviceCount; i++)
    {
        __u64 k = KsvFifo[i];

        for (j = 0; j < 5; j++)
        {
            *ptr = (__u8)(k & 0xFF);
            ptr++;
            k >>= 8;
        }
    }

    ptr[0] = (__u8)(Bstatus & 0xff);
    ptr[1] = (__u8)((Bstatus >> 8));
    ptr += 2;

    /* do upstream to get M0 */
    if (!TEST_V_PRIME_VECTORS)
    {
        if (!HdcpCalculateM(Cn, Dksv, Mprime, &M0, glob))
        {
            DEBUG_PRINT(( "upstream authentication failure" ));
            goto fail;
        }
    }

    for (i = 0; i < 8; i++)
    {
        *ptr = (__u8)(M0 & 0xff);
        ptr++;
        M0 >>= 8;
    }
    M0 = 0;

    /* compute V -- message length is in bits */
    sha1(V, msg, size * 8);

    /* Vprime and V must match */
    if (memcmp( Vprime, V, sizeof(V) ) != 0)
    {
        DEBUG_PRINT(( "V and V' mismatch\n" ));
        goto fail;
    }

    DEBUG_PRINT(( "!!!! V and V' matched\n" ));
    b = 1;
    goto clean;

fail:
    b = 0;

clean:
    if( msg )
    {
        memset( msg, 0, size );
    }
    free( msg );

    return b;
}

static int hdcp_readstatus(HdcpClient *client)
{
    HdcpPacket *pkt;
    __u64 Kp, Cn;
    int b;

    pkt = &client->pkt;

    memset( pkt, 0, sizeof(HdcpPacket) );

    pkt->c_n = Cn = GenerateRnd();
    // this can get over written in the glob decrypt
    pkt->c_ksv = s_Cksv;
    pkt->value_flags |= TEGRA_NVHDCP_FLAG_CN | TEGRA_NVHDCP_FLAG_CKSV;

    if(ioctl(client->fd, TEGRAIO_NVHDCP_READ_S, pkt))
        goto fail;

    DebugPrintHdcpContext("reads", pkt);

    Kp = HdcpCalculateKp(pkt->d_ksv, pkt->b_ksv, pkt->a_n, pkt->cs,
        Cn, pkt->hdcp_status, client->glob,
        (pkt->value_flags & TEGRA_NVHDCP_FLAG_CS) ? 1 : 0);

    if( Kp != pkt->k_prime )
    {
        DEBUG_PRINT(( "!!! Kp mismatched 0x%llx, 0x%llx\n", Kp, pkt->k_prime));
        goto fail;
    }

    b = 1;
    goto clean;

fail:
    b = 0;

clean:
    return b;
}

static int hdcp_readm(HdcpClient *client)
{
    int b;
    HdcpPacket *pkt;
    __u64 Cn;

    pkt = &client->pkt;

    memset( pkt, 0, sizeof(HdcpPacket) );

    /* get Mprime */
    pkt->c_n = Cn = GenerateRnd();
    // this can get over written in the glob decrypt
    pkt->c_ksv = s_Cksv;
    pkt->value_flags |= TEGRA_NVHDCP_FLAG_CN | TEGRA_NVHDCP_FLAG_CKSV;

    if (ioctl(client->fd, TEGRAIO_NVHDCP_READ_M, pkt))
        goto fail;

    DebugPrintHdcpContext("readm", pkt);

    b = ValidateLink_Core(Cn, pkt->c_ksv, pkt->d_ksv, pkt->m_prime,
        pkt->bksv_list, pkt->num_bksv_list, (__u32 *)pkt->v_prime, pkt->b_status,
        client->glob);

    if (!b)
        ioctl(client->fd, TEGRAIO_NVHDCP_RENEGOTIATE, pkt);

    goto clean;

fail:
    b = 0;

clean:
    return b;
}

HDCP_RET_ERROR hdcp_open(HDCP_CLIENT_HANDLE *hClientRef, int fbIndex)
{
    HdcpClient *client;
    char path[64];
    int fd;
    struct timeval tv;
    __u64 seed;

    if (NULL == hClientRef)
        return HDCP_RET_INVALID_PARAMETER;

    DEBUG_PRINT(( "+%s\n", __FUNCTION__ ));
    sprintf(path, HDCP_DEV_PATH "%u", fbIndex);

    DEBUG_PRINT(( " %s opening: %s\n", __FUNCTION__, path ));
    fd = open(path, O_RDWR, 0);
    if (fd < 0)
        return HDCP_RET_UNSUCCESSFUL;

    client = (HdcpClient *)malloc(sizeof(HdcpClient));
    if (NULL == client)
        return HDCP_RET_NO_MEMORY;

    memset(client, 0, sizeof(HdcpClient));

    client->glob = HdcpGlobOpen();
    if (NULL == client->glob)
    {
        free(client);
        return HDCP_RET_UNSUCCESSFUL;
    }

    client->fd = fd;
    client->fb_index = fbIndex;
    *hClientRef = (HDCP_CLIENT_HANDLE)client;

    gettimeofday(&tv, NULL);
    seed = (__u64) tv.tv_sec * (__u64) tv.tv_usec;
    rc4_init( &s_rnd_rc4, (unsigned char *)&seed, sizeof(__u64) );

    DEBUG_PRINT(( " %s opening: %x\n", __FUNCTION__, fd ));
    return HDCP_RET_SUCCESS;
}

HDCP_RET_ERROR hdcp_close(HDCP_CLIENT_HANDLE hClient)
{
    HdcpClient *client = (HdcpClient *)hClient;

    if (NULL == client)
        return HDCP_RET_INVALID_PARAMETER;

    if (client->fd)
        close(client->fd);

    if (client->glob)
        HdcpGlobClose(client->glob);

    memset(client, 0, sizeof(HdcpClient));
    free(client);

    return HDCP_RET_SUCCESS;
}

HDCP_RET_ERROR hdcp_status(HDCP_CLIENT_HANDLE hClient)
{
    HdcpClient *client = (HdcpClient *)hClient;

    if (NULL == client)
        return HDCP_RET_INVALID_PARAMETER;

    if (!hdcp_readstatus(client))
        return HDCP_RET_READS_FAILED;

    if (TEGRA_NVHDCP_RESULT_PENDING == client->pkt.packet_results)
        return HDCP_RET_LINK_PENDING;

    if ((TEGRA_NVHDCP_RESULT_SUCCESS != client->pkt.packet_results) \
        || !(TEGRA_NVHDCP_FLAG_S & client->pkt.value_flags)) \
        return HDCP_RET_READS_FAILED;

    if (!(HDCP_STATUS_REPEATER & client->pkt.hdcp_status))
        return HDCP_RET_SUCCESS;

    // readm to verify repeater
    if (!client->last_An || (client->last_An != client->pkt.a_n))
    {
        if (!hdcp_readm(client))
            return HDCP_RET_READM_FAILED;
        client->last_An = client->pkt.a_n;
    }

    return HDCP_RET_SUCCESS;
}

void DebugPrintHdcpContext(char * string, HdcpPacket *pkt)
{
    __u32 i = 0;
    __u32 *vP = (__u32 *)pkt->v_prime;

    DEBUG_PRINT(( "[%s] : <<<<< debug print start >>>>>\n", string ));
    DEBUG_PRINT(( "[%s] : Cn = 0x%llx\n", string, pkt->c_n ));
    DEBUG_PRINT(( "[%s] : Cksv = 0x%llx\n", string, pkt->c_ksv ));
    DEBUG_PRINT(( "[%s] : Cs = 0x%llx\n", string, pkt->cs ));
    DEBUG_PRINT(( "[%s] : Kp = 0x%llx\n", string, pkt->k_prime ));
    DEBUG_PRINT(( "[%s] : An = 0x%llx\n", string, pkt->a_n ));
    DEBUG_PRINT(( "[%s] : Aksv = 0x%llx\n", string, pkt->a_ksv ));
    DEBUG_PRINT(( "[%s] : Bksv = 0x%llx\n", string, pkt->b_ksv ));
    DEBUG_PRINT(( "[%s] : Dksv = 0x%llx\n", string, pkt->d_ksv ));
    DEBUG_PRINT(( "[%s] : Kp = 0x%llx\n", string, pkt->k_prime ));
    DEBUG_PRINT(( "[%s] : Vprime = %x %x %x %x %x\n", string, \
                  vP[0], vP[1], vP[2], vP[3], vP[4]));
    DEBUG_PRINT(( "[%s] : Mp = 0x%llx\n", string, pkt->m_prime ));
    DEBUG_PRINT(( "[%s] : numBKSVs = 0x%x\n", string, pkt->num_bksv_list ));
    for (i = 0; i < pkt->num_bksv_list; i++)
    {
        DEBUG_PRINT(( "[%s] : Bksv[%d] = 0x%llx\n", string, i, pkt->bksv_list[i] ));
    }
    DEBUG_PRINT(( "[%s] : Bstatus = 0x%x\n", string, pkt->b_status ));
    DEBUG_PRINT(( "\n" ));

    return;
}
