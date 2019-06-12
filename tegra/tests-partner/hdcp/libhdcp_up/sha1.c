/*
 * Copyright (c) 2010-2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <string.h>
#include "sha1.h"

// left-rotates a 32-bit unsigned value <x> by <C> bits
#define ROL(x,C) ((((x)<<(C))&~((1<<(C))-1)) | (((x)>>(32-(C)))&((1<<(C))-1)))

// hashes a single 512-bit message chunk
static void sha1_step( const __u8 *Message, __u32 *H )
{
    __u32 W[80];
    __u32 a,b,c,d,e,f,k;
    int i;

    for (i=0; i<16; i++, Message+=4)
    {
        W[i] = (Message[0]<<24) | (Message[1]<<16) | (Message[2]<<8) |
            (Message[3]);
    }
    for (i=16; i<80; i++)
    {
        W[i] = ROL((((W[i-3] ^ W[i-8]) ^ W[i-14]) ^ W[i-16]),1);
    }
    a = H[0];
    b = H[1];
    c = H[2];
    d = H[3];
    e = H[4];

    for (i=0; i<80; i++)
    {
        if (i<20)
        {
            f = ((b & c) | ((~b) & d));
            k = 0x5A827999UL;
        }
        else if (i<40)
        {
            f = (b ^ c) ^ d;
            k = 0x6ED9EBA1UL;
        }
        else if (i<60)
        {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCUL;
        }
        else
        {
            f = (b ^ c) ^ d;
            k = 0xCA62C1D6UL;
        }
        f += (ROL(a,5) + e + k + W[i]);
        e = d;
        d = c;
        c = ROL(b,30);
        b = a;
        a = f;
    }
    H[0] += a;
    H[1] += b;
    H[2] += c;
    H[3] += d;
    H[4] += e;
}

void sha1( __u32 *Digest, const __u8 *Message, __u64 MsgLenInBits)
{
    __u8 Chunk[64];
    __u32 H[5] =
    {
        0x67452301UL, 0xEFCDAB89UL, 0x98BADCFEUL, 0x10325476UL, 0xC3D2E1F0UL
    };
    __u64 R = MsgLenInBits;

    while (R >= 512ULL)
    {
        memcpy(Chunk,Message,64);
        R -= 512ULL;
        Message += 64;
        sha1_step(Chunk, H);
    }

    memset(Chunk, 0, 64);
    memcpy(Chunk,Message,(__u32)((R+7)>>3));
    Chunk[R>>3] |= 1<<(7-(R&0x7));

    // If R>=448, then two additional chunk hashes will be needed - one padded
    // from the total message length (plus the ending '1') to 512b, and one
    // padded from the beginning to the final 64b;
    if (R>=448)
    {
        sha1_step(Chunk, H);
        memset(Chunk, 0, 64);
    }
    // The last 64b of the message is the original length, stored as a 64b
    // big-endian integer.
    Chunk[63] = (__u8)(MsgLenInBits & 0xffULL);
    Chunk[62] = (__u8)((MsgLenInBits >> 8) & 0xffULL);
    Chunk[61] = (__u8)((MsgLenInBits >> 16) & 0xffULL);
    Chunk[60] = (__u8)((MsgLenInBits >> 24) & 0xffULL);
    Chunk[59] = (__u8)((MsgLenInBits >> 32) & 0xffULL);
    Chunk[58] = (__u8)((MsgLenInBits >> 40) & 0xffULL);
    Chunk[57] = (__u8)((MsgLenInBits >> 48) & 0xffULL);
    Chunk[56] = (__u8)((MsgLenInBits >> 56) & 0xffULL);
    sha1_step(Chunk,H);
    Digest[0] = H[0];
    Digest[1] = H[1];
    Digest[2] = H[2];
    Digest[3] = H[3];
    Digest[4] = H[4];
}
