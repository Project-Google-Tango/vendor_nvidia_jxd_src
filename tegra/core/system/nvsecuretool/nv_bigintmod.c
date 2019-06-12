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
#include "nv_rsa.h"
#include "nv_cryptolib.h"
#include "nvassert.h"


/*
* Finding inverse modulo 2^32, input must be odd number
*/
static NvU32 NvInverseDigit(NvU32 x)
{
    NvU32 i = 1;
    NvU32 j = 2;
    do
    {
        i ^= (j ^ (j & (x * i)));
        j += j;
    }
    while (j);
    return i;
}


/*
* Big Int Modulus functions
*/
static NvU32 NvBigIntIsZero(const NvU32* x, const NvU32 digits)
{
    NvU32 i;
    for (i = 0; i < digits; i++)
    {
        if (x[i] != 0)
        {
            return NV_FALSE;
        }
    }
    return NV_TRUE;
}


static NvU32 NvBigIntSubtract(NvU32* z, const NvU32* x, const NvU32* y, const NvU32 digits)
{
    NvU32 i, j, k;
    for (i = k = 0; i < digits; i ++)
    {
        j = x[i] - k;
        k = (j > (~k)) ? 1 : 0;
        j -= y[i];
        k += (j > (~y[i])) ? 1 : 0;
        z[i] = j;
    }
    return k;
}

static int NvBigIntCompare(const NvU32* x, const NvU32* y, const NvU32 digits)
{
    int i;
    for (i = digits - 1; i >= 0; i--)
    {
        if (x[i] > y[i])
            return 1;
        else if (x[i] < y[i])
            return -1;
    }
    return 0;
}

static void NvBigIntSubtractMod(NvU32 *z, const NvU32* x, const NvU32* y, const NvBigIntModulus* p)
{
    if (NvBigIntSubtract(z, x, y, p->digits))
    {
        NvBigIntAdd(z, z, p->n, p->digits);
    }
}

static void NvBigIntHalveMod(NvU32 *z, const NvU32* x, const NvBigIntModulus* p)
{
    if (*x & 1)
    {
        NvU32 carry = NvBigIntAdd(z, x, p->n, p->digits);
        NvBigIntHalve(z, z, carry, p->digits);
    }
    else
    {
        NvBigIntHalve(z, z, 0, p->digits);
    }
}

static void NvBigIntDoubleMod(NvU32 *z, const NvU32* x, const NvBigIntModulus* p)
{
    NvU32 i, j, k;
    for (i = k = 0; i < p->digits; i++)
    {
        j = (x[i] >> 31);
        z[i] = (x[i] << 1) | k;
        k = j;
    }
    if (k || NvBigIntCompare(z, p->n, p->digits) >= 0)
    {
        NvBigIntSubtract(z, z, p->n, p->digits);
    }
}

static void NvBigIntMontgomeryProduct(NvU32* z, const NvU32* x, const NvU32* y, const NvBigIntModulus* p)
{
  const NvU32 *n = p->n;
  const NvU32 digits   = p->digits;
  const NvU32 invdigit = p->invdigit;
  NvU32 t[BIGINT_MAX_DW], t0, m;
  NvU32 i, j, t1, k[2];
  memset(t, 0, digits * sizeof(NvU32));
  for (i = t0 = 0; i < digits; i++)
  {
    NvU64 temp;
    k[1] = 0;
    for (j = 0; j < digits; j++)
    {
        temp = (NvU64)y[j] * x[i] + t[j] + k[1];
        k[0] = *((NvU32*)(&temp));
        k[1] = *((NvU32*)(&temp) + 1);
        t[j]         = k[0];
    }
    temp = (NvU64)t0 + k[1];
    k[0] = *((NvU32*)(&temp));
    k[1] = *((NvU32*)(&temp) + 1);
    //*((NvU64*)k)  = (NvU64)t0 + k[1];
    t0            = k[0];
    t1            = k[1];
    m             = (NvU32)(t[0] * invdigit);
    temp = (NvU64)m * n[0] + t[0];
    k[0] = *((NvU32*)(&temp));
    k[1] = *((NvU32*)(&temp) + 1);
    //*((NvU64*)k)  = (NvU64)m * n[0] + t[0];
    for (j = 1; j < digits; j++)
    {
        temp = (NvU64)m * n[j] + t[j] + k[1];
        k[0] = *((NvU32*)(&temp));
        k[1] = *((NvU32*)(&temp) + 1);
        //*((NvU64*)k) = (NvU64)m * n[j] + t[j] + k[1];
        t[j-1]       = k[0];
    }
    temp = (NvU64)t0 + k[1];
    k[0] = *((NvU32*)(&temp));
    k[1] = *((NvU32*)(&temp) + 1);
    //*((NvU64*)k)  = (NvU64)t0 + k[1];
    t[digits - 1] = k[0];
    t0            = t1 + k[1];
  }

  if (t0 || NvBigIntCompare(t, n, digits) >= 0)
  {
    NvBigIntSubtract(z, t, n, digits);
  }
  else
  {
    memcpy(z, t, digits * sizeof(NvU32));
  }
}

static NvU32 NvBigIntGetBit(const NvU32* x, NvU32 i)
{
  NvU32 b = 0;
  return (((x[i >> 5] >> (i & 0x1f)) ^ b) & 1);
}

/*
* Swapping endianness, works in place or out-of-place
*/
void NvSwapEndianness(void* pOutData, const void* pInData, const NvU32 size)
{
    NvU32 i;
    for (i = 0; i < size / 2; i++)
    {
        NvU8 b1 = ((NvU8*)pInData)[i];
        NvU8 b2 = ((NvU8*)pInData)[size - 1 - i];
        ((NvU8*)pOutData)[i] = b2;
        ((NvU8*)pOutData)[size - 1 - i] = b1;
    }
}

NvU32 NvBigIntAdd(NvU32* z, const NvU32* x, const NvU32* y, const NvU32 digits)
{
    NvU32 i, j, k;
    for (i = k = 0; i < digits; i ++)
    {
        j = x[i] + k;
        k = (j < k) ? 1 : 0;
        j += y[i];
        k += (j < y[i]) ? 1 : 0;
        z[i] = j;
    }
    return k;
}

void NvBigIntMultiply(NvU32 *z, const NvU32* x, const NvU32* y, const NvU32 digits)
{
    NvU32 i, j, t[BIGINT_MAX_DW*2], k[2];
    memset(t, 0,  sizeof(t));
    for (i = 0; i < digits; i++)
    {
        k[1] = 0;
        for (j = 0; j < digits; j++){
            NvU64 temp = (NvU64)y[j] * x[i] + t[i + j] + k[1];
            k[0] = *((NvU32*)(&temp));
            k[1] = *((NvU32*)(&temp) + 1);
            t[i + j]     = k[0];
        }
        t[i + digits] = k[1];
    }
    memcpy(z, t, digits * 2 * sizeof(NvU32));
}

void NvBigIntHalve(NvU32 *z, const NvU32* x, const NvU32 carry, const NvU32 digits)
{
    NvU32 i;
    for (i = 0; i < digits - 1; i++)
    {
        z[i] = (x[i+1] << 31) | (x[i] >> 1);
    }
    z[i] = (carry << 31) | (x[i] >> 1);
}

void NvBigIntAddMod(NvU32 *z, const NvU32* x, const NvU32* y, const NvBigIntModulus* p)
{
    if (NvBigIntAdd(z, x, y, p->digits) || NvBigIntCompare(z, p->n, p->digits) >= 0)
    {
        NvBigIntSubtract(z, z, p->n, p->digits);
    }
}

void NvBigIntInverseMod(NvU32 *z, const NvU32* x, const NvBigIntModulus* p)
{
    NvU32 b[BIGINT_MAX_DW], d[BIGINT_MAX_DW];
    NvU32 u[BIGINT_MAX_DW], v[BIGINT_MAX_DW];
    memset(b, 0, sizeof(b));
    memset(d, 0, sizeof(d));
    d[0] = 1;
    memcpy(u, p->n, p->digits * sizeof(NvU32));
    memcpy(v, x, p->digits * sizeof(NvU32));
    while (!NvBigIntIsZero(u, p->digits))
    {
        while (!(*u & 1))
        {
            NvBigIntHalveMod(u, u, p);
            NvBigIntHalveMod(b, b, p);
        }
        while (!(*v & 1))
        {
            NvBigIntHalveMod(v, v, p);
            NvBigIntHalveMod(d, d, p);
        }
        if (NvBigIntCompare(u, v, p->digits) >= 0)
        {
            NvBigIntSubtractMod(u, u, v, p);
            NvBigIntSubtractMod(b, b, d, p);
        }
        else
        {
            NvBigIntSubtractMod(v, v, u, p);
            NvBigIntSubtractMod(d, d, b, p);
        }
    }
    memcpy(z, d, p->digits * sizeof(NvU32));
}

void NvBigIntPowerMod(NvU32* z, const NvU32* x, const NvU32* e, const NvBigIntModulus* p, const NvU32 e_digits)
{
  int i;
  NvU32 one[BIGINT_MAX_DW], rx[BIGINT_MAX_DW];

  memset(one, 0, sizeof(NvU32)*BIGINT_MAX_DW);
  one[0] = 1;

  for (i = e_digits * 32 - 1; !NvBigIntGetBit(e, i) && i >= 0; i--);
  if (i < 0)
  {
    memcpy(z, one, p->digits * sizeof(NvU32));
    return;
  }
  NvBigIntMontgomeryProduct(rx, x, p->r2, p);
  memcpy(z, rx, p->digits * sizeof(NvU32));
  for (i--; i >= 0; i--)
  {
    NvBigIntMontgomeryProduct(z, z, z, p);
    if (NvBigIntGetBit(e, i)) NvBigIntMontgomeryProduct(z, z, rx, p);
  }
  NvBigIntMontgomeryProduct(z, z, one, p);
}

void NvInitBigIntModulus(NvBigIntModulus* p, const NvU8* data, const NvU32 size, NvU32 swap)
{
  NvU32 i, j;
  // clear struct
  memset(p, 0, sizeof(NvBigIntModulus));

  // initialize digits
  p->digits = (size + sizeof(NvU32) - 1) / sizeof(NvU32);

  // initialize p->n
  if(swap)
    NvSwapEndianness(p->n, data, size);
  else
    memcpy(p->n, data, size);

  // calculate p->invdigit
  p->invdigit = NvInverseDigit(p->n[0]);

  // calculate p->r2
  NvBigIntSubtract(p->r2, p->r2, p->n, p->digits);
  for (i = p->digits * 32, j = 0; j < 32; j++)
  {
    if (i & ~((unsigned)~0 >> j))
    {
      NvBigIntMontgomeryProduct(p->r2, p->r2, p->r2, p);
    }
    if (i & (1 << (31 - j)))
    {
      NvBigIntDoubleMod(p->r2, p->r2, p);
    }
  }
}

/**
 * Translation table for base64 decoding
 */
static const char s_cd64[]="|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";

/**
 * NvOsBase64Decode - Convert an encoded char buffer to binary
 */
NvError NvOsBase64Decode(const char *pinStringPtr, NvU32 inStringSize,
                         unsigned char *pbinaryBufPtr, NvU32 *pbinaryBufSize)
{
    NvU32 i = 0;
    const char *in = pinStringPtr;
    unsigned char *out = (unsigned char *)pbinaryBufPtr;

    if ((in == NULL) || (pbinaryBufSize == NULL) || (inStringSize == 0))
    {
        return NvError_BadParameter;
    }

    if (out == NULL)
    {
        // Valid if the caller is requesting the size of the decoded
        // binary buffer so they know how much to alloc.
        *pbinaryBufSize = 0;
    }
    else
    {
        // Validate the size of the passed in binary data buffer.
        // In theory the binaryBufSize should be 3/4 the size of
        // the input string buffer.  But if there were some white
        // space chars in the buffer then it's possible that the
        // binary buffer is smaller.
        // We'll validate against 2/3rds the size of the inStringSize
        // here.  That allows for some slop.
        // Below we have code that makes sure we don't overrun the
        // output buffer.  (Which sort of makes this here irrelevant...)

        // NV_ASSERT(binaryBufSize >= (inStringSize * 3)/4);
        NV_ASSERT(*pbinaryBufSize >= (inStringSize * 2)/3);
        if (*pbinaryBufSize < (inStringSize * 2)/3)
        {
            return NvError_InsufficientMemory;
        }
    }

    // This loop is less efficient than it could be because
    // it's designed to tolerate bad characters (like whitespace)
    // in the input buffer.
    while (i < inStringSize)
    {
        int vlen = 0;
        unsigned char vbuf[4], v;  // valid chars

        // gather 4 good chars from the input stream
        while ((i < inStringSize) && (vlen < 4))
        {
            v = 0;
            while ((i < inStringSize) && (v == 0))
            {
                // This loop skips bad chars
                v = (unsigned char) in[i++];
                v = (unsigned char) ((v < 43 || v > 122) ? 0 : s_cd64[v - 43]);
                if (v != 0)
                {
                    v = (unsigned char) ((v == '$') ? 0 : v - 61);
                }
            }
            if (v != 0)
            {
                vbuf[vlen++] = (unsigned char)(v - 1);
            }
        }

        if (out == NULL)
        {
            // just measuring the size of the destination buffer
            if ((vlen > 1) && (vlen <= 4))   // only valid values
            {
                *pbinaryBufSize += vlen - 1;
            }
            continue;
        }

        NV_ASSERT((out + vlen - 1) <= (pbinaryBufPtr + *pbinaryBufSize));
        if ((out + vlen - 1) > (pbinaryBufPtr + *pbinaryBufSize))
        {
            return NvError_InsufficientMemory;
        }

        switch (vlen)
        {
            case 4:
                // 4 input chars equals 3 output chars
                out[2] = (unsigned char ) (((vbuf[2] << 6) & 0xc0) | vbuf[3]);
                // fall through
            case 3:
                // 3 input chars equals 2 output chars
                out[1] = (unsigned char ) (vbuf[1] << 4 | vbuf[2] >> 2);
                // fall through
            case 2:
                // 2 input chars equals 1 output char
                out[0] = (unsigned char ) (vbuf[0] << 2 | vbuf[1] >> 4);
                out += vlen - 1;
                break;
            case 1:
                // Unexpected
                NV_ASSERT(vlen != 1);
                break;
            case 0:
                // conceivable if white space follows the end of valid data
                break;
            default:
                // Unexpected
                NV_ASSERT(vlen <= 4);
                break;
        }
    }

    return NvSuccess;
}

