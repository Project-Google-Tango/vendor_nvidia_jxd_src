/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NVCRYPTOLIB_H__
#define __NVCRYPTOLIB_H__

#include "nvos.h"

void NvInitBigIntModulus(NvBigIntModulus* p, const NvU8* data, const NvU32 size, NvU32 swap);
void NvBigIntPowerMod(NvU32* z, const NvU32* x, const NvU32* e,
                    const NvBigIntModulus* p, const NvU32 e_digits);
void NvSwapEndianness(void* pOutData, const void* pInData, const NvU32 size);
void NvBigIntInverseMod(NvU32 *z, const NvU32* x, const NvBigIntModulus* p);
void NvBigIntHalve(NvU32 *z, const NvU32* x, const NvU32 carry, const NvU32 digits);
void NvBigIntMultiply(NvU32 *z, const NvU32* x, const NvU32* y, const NvU32 digits);
void NvBigIntAddMod(NvU32 *z, const NvU32* x, const NvU32* y, const NvBigIntModulus* p);
NvU32 NvBigIntAdd(NvU32* z, const NvU32* x, const NvU32* y, const NvU32 digits);

//RSA

/**
* Does c = m^e (mod n)
* @param output will contain the result of the modular exponentiation
* @param message on which the exponentiation will be applied
* @param exponent exponent value
* @param n modulus of the operation
*/
void NvRSAEncDecMessage(NvU32* output, NvU32* message, NvU32* exponent, NvBigIntModulus* n);

/**
 * NvOsBase64Decode - Convert an encoded char buffer to binary
 */
NvError NvOsBase64Decode(const char *inStringPtr, NvU32 inStringSize,
                         unsigned char *binaryBufPtr, NvU32 *binaryBufSize);


#endif // NVCRYPTOLIB

