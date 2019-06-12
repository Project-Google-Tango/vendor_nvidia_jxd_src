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

/*
* (1) keep halving (p-1) * (q-1) until you get an odd number,
*    to obtain the following factorization:
*
*            (p-1) * (q-1) = n' * 2a
*
* here n' is odd, and a is a positive integer.
*
* In RSA, usually a is very small such as 2 (in that case, (p-1) * (q-1) = n' * 4).
*
* (2) use bigIntInverseMod to calculate d' = e-1 mod n'
*
* (3) calculate d" = d' mod 2^a  , and n" = n' mod 2^a, e" = e mod 2^a
*
* this is trivial, just bitwise-AND the lowest word with (2a - 1)
*
* (4) find a value b in [0, ..., 2a - 1] so that (d" + b * n")*e" mod 2^a  = 1
*
* for small values of a, you can just use a for loop to do an exhausive search.
* No need to use modular inverse which is even slower
*
* (5) the final value is d = d' + b * n'
*
**/

void NvRSAPrivateKeyGeneration(NvU32* result, NvU32* e,
        NvU8* p, NvU8* q, NvU32 digit)
{
  NvU32 pPrime[BIGINT_MAX_DW], qPrime[BIGINT_MAX_DW];
  NvU32 dPrime[BIGINT_MAX_DW];
  NvU32 nPrime[BIGINT_MAX_DW];
  NvU32 dDoublePrime[BIGINT_MAX_DW];
  NvU32 nDoublePrime[BIGINT_MAX_DW];
  NvU32 phi[BIGINT_MAX_DW];
  NvU32 a=1;
  NvU32 b[BIGINT_MAX_DW];
  NvU32 eDoublePrime;
  NvS32 i = 0;
  NvU32 terminate=NV_FALSE;
  NvBigIntModulus nPrimeMod;

  memset(&nPrimeMod, 0, sizeof(nPrimeMod));

  memcpy(pPrime, p, digit*sizeof(NvU32));
  memcpy(qPrime, q, digit*sizeof(NvU32));

  pPrime[0]--;
  qPrime[0]--;


  NvBigIntMultiply(phi, pPrime, qPrime, digit/2);

  memcpy(nPrime, phi, digit*sizeof(NvU32));

  //(1) keep halving (p-1) * (q-1) until you get an odd number,
  // to obtain the following factorization: (p-1) * (q-1) = n' * 2a
  do {
    NvBigIntHalve(nPrime, nPrime, 0, digit);
    a *= 2;
    if(nPrime[0] & 1)
      terminate = NV_TRUE;
  } while (!terminate);

  NvInitBigIntModulus(&nPrimeMod, (NvU8 *)nPrime, digit*sizeof(NvU32),
    NV_FALSE);

  //(2) use bigIntInverseMod to calculate d' = e^-1 mod n'
  NvBigIntInverseMod(dPrime, e, &nPrimeMod);

  //(3) calculate d" = d' mod 2^a  , and n" = n' mod 2^a, e" = e mod 2^a
  dDoublePrime[0] = dPrime[0] & (a-1);
  nDoublePrime[0] = nPrime[0] & (a-1);
  eDoublePrime = e[0] & (a-1);

  //(4) find a value b in [0, ..., 2a - 1] so that (d" + b * n")*e" mod 2^a  = 1
  terminate=NV_FALSE;
  do {
    if((((dDoublePrime[0] + i*nDoublePrime[0])*eDoublePrime) & (a-1)) == 1)
      terminate = NV_TRUE;
    else
      i++;
  } while(!terminate);

  memset(b, 0, digit*sizeof(NvU32));
  b[0] = i;

  //(5) the final value is d = d' + b * n'
  memset(result, 0, digit*4);
  NvBigIntMultiply(result, b, nPrime, digit);
  NvBigIntAdd(result, dPrime, result, digit);

}

void NvRSAInitModulusFromPQ
    (NvBigIntModulus* n, NvU32* p, NvU32* q, NvU32 bitSize, NvU32 swap) {
  //NvU8 array[BIGINT_MAX_DW*sizeof(NvU32)];
  NvU32 array[BIGINT_MAX_DW];
  if(swap) {
    NvSwapEndianness((NvU8*)p, (NvU8*)p, bitSize/8/2);
    NvSwapEndianness((NvU8*)q, (NvU8*)q, bitSize/8/2);
  }
  NvBigIntMultiply(array, p, q, bitSize/8/2/sizeof(NvU32));
  memset(n, 0, sizeof(NvBigIntModulus));
  NvInitBigIntModulus(n, (NvU8*)array, bitSize/8, NV_FALSE);
}

void NvRSAEncDecMessage
    (NvU32* output, NvU32* message, NvU32* exponent, NvBigIntModulus* n) {
  NvBigIntPowerMod(output, message, exponent, n, n->digits);
}
