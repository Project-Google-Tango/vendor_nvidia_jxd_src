/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stdio.h>
#include <string.h>
#include "nvos.h"
#include "nv_sha256.h"

#define LEFT_ROTATE(x,n)  (((x) << (n)) | ((x) >> (32-(n))))
#define RIGHT_ROTATE(x,n) (((x) >> (n)) | ((x) << (32-(n))))

#define LEFT_SHIFT(x,n)  (((x) << (n)))
#define RIGHT_SHIFT(x,n) (((x) >> (n)))

static NvU32 k[64] = {
0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};


static void SHA256_Process(NvSHA256_Data *pSHAData)
{
    NvU32 s0 = 0;
    NvU32 s1 = 0;
    NvU32 t1 = 0;
    NvU32 t2 = 0;
    NvU32 i = 0;
    NvU32 a = 0;
    NvU32 b = 0;
    NvU32 c = 0;
    NvU32 d = 0;
    NvU32 e = 0;
    NvU32 f = 0;
    NvU32 g = 0;
    NvU32 h = 0;
    NvU32 temp_inputbuf[16];
    NvU32 w[64];

    memcpy(&temp_inputbuf, pSHAData->InputBuf, 64);
    /*  copy the input buffer to first 16 words */
    for (i = 0 ; i < 16 ; i++)
    {
        NvU32 temp;
        temp = temp_inputbuf[i];
        w[i] = ( ( (temp & 0X000000FF) << 24 )  |
                 ( (temp & 0X0000FF00) << 8 )  |
                 ( (temp & 0X00FF0000) >> 8 )  |
                 ( (temp & 0XFF000000) >> 24 ) );
    }

    /*  extending 16 words to 64 words */
    for (i = 16 ; i < 64 ; i++)
    {
        s0 = (RIGHT_ROTATE(w[i-15],7)) ^ (RIGHT_ROTATE(w[i-15],18)) ^ (RIGHT_SHIFT(w[i-15],3));
        s1 = (RIGHT_ROTATE(w[i-2],17)) ^ (RIGHT_ROTATE(w[i-2],19)) ^ (RIGHT_SHIFT(w[i-2],10));
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    a = pSHAData->h_state[0];
    b = pSHAData->h_state[1];
    c = pSHAData->h_state[2];
    d = pSHAData->h_state[3];
    e = pSHAData->h_state[4];
    f = pSHAData->h_state[5];
    g = pSHAData->h_state[6];
    h = pSHAData->h_state[7];

    /*  main loop */
    for (i = 0 ; i < 64 ; i++)
    {
        NvU32 maj = 0;
        NvU32 ch = 0;

        s0 = (RIGHT_ROTATE(a,2)) ^ (RIGHT_ROTATE(a,13)) ^ (RIGHT_ROTATE(a,22));
        maj = (a & b) ^ (a & c) ^ (b & c);
        t2 = s0 + maj;
        s1 = (RIGHT_ROTATE(e,6)) ^ (RIGHT_ROTATE(e,11)) ^ (RIGHT_ROTATE(e,25));
        ch = (e & f) ^ ((~e) & g);
        t1 = h + s1 + ch + k[i] + w[i];

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    pSHAData->h_state[0] += a;
    pSHAData->h_state[1] += b;
    pSHAData->h_state[2] += c;
    pSHAData->h_state[3] += d;
    pSHAData->h_state[4] += e;
    pSHAData->h_state[5] += f;
    pSHAData->h_state[6] += g;
    pSHAData->h_state[7] += h;

}


NvS32 NvSHA256_Init(NvSHA256_Data *pSHAData)
{
    if (pSHAData == NULL)
        return -1;
    pSHAData->h_state[0] = 0x6a09e667;
    pSHAData->h_state[1] = 0xbb67ae85;
    pSHAData->h_state[2] = 0x3c6ef372;
    pSHAData->h_state[3] = 0xa54ff53a;
    pSHAData->h_state[4] = 0x510e527f;
    pSHAData->h_state[5] = 0x9b05688c;
    pSHAData->h_state[6] = 0x1f83d9ab;
    pSHAData->h_state[7] = 0x5be0cd19;

    pSHAData->Count = 0;

    return 0;
}

NvS32 NvSHA256_Update(NvSHA256_Data *pSHAData, NvU8 *pMsg, NvU32 MsgLen)
{
    NvU32 i;
    const NvU8* p = (const NvU8*)pMsg;

    if ((pSHAData == NULL) || (pMsg == NULL))
        return -1;

    i = pSHAData->Count % sizeof(pSHAData->InputBuf);
    pSHAData->Count += MsgLen;
    while (MsgLen--)
    {
        pSHAData->InputBuf[i++] = *p++;
        if (i == sizeof(pSHAData->InputBuf))
        {
            SHA256_Process(pSHAData);
            i = 0;
        }
    }
    return 0;
}

NvS32 NvSHA256_Finalize(NvSHA256_Data *pSHAData, NvSHA256_Hash *pSHAHash)
{
    NvU32 BufIndex = 0;
    NvU32 BufSize = 0;
    NvU32 i = 0;
    NvU64 MsgLen = 0;

    if ((pSHAData == NULL) || (pSHAHash == NULL))
        return -1;

    MsgLen = pSHAData->Count * 8;
    BufSize = sizeof(pSHAData->InputBuf);
    BufIndex = (NvU32)((pSHAData->Count) % (NvU64)BufSize);

    pSHAData->InputBuf[BufIndex++] = 0x80;    /*  appending the message with '1' */

    if (BufIndex > (BufSize-8))
    {
        while (BufIndex < BufSize)
            pSHAData->InputBuf[BufIndex++] = 0x00;
        SHA256_Process(pSHAData);
        BufIndex = 0;
    }
    while (BufIndex < (BufSize-8))
        pSHAData->InputBuf[BufIndex++] = 0x00;

    for (i = 0 ; i < 8 ; i++)
        pSHAData->InputBuf[BufIndex++] = (NvU8)(MsgLen >> (7-i) * 8);

    if(BufIndex != BufSize)
        return -1;

    SHA256_Process(pSHAData);
    for (i = 0 ; i < SHA_256_HASH_SIZE ; i++)
    {
        NvU32 temp = pSHAData->h_state[i/4];
        pSHAHash->Hash[i] = temp >> (3-(i%4))*8;
    }
    return 0;
}

