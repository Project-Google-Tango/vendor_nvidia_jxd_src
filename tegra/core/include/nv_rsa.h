/*
 * Copyright (c) 2011-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NV_RSA_H__
#define __NV_RSA_H__

#include "nvos.h"

#define RSANUMBYTES (256)
#define RSA_PUBLIC_EXPONENT_VAL (65537)
#define RSA_LENGTH (2048)

//big int mod
#define BIGINT_MAX_DW 64

typedef struct _BigIntModulus
{
    NvU32 digits;
    NvU32 invdigit;
    NvU32 n[BIGINT_MAX_DW];
    NvU32 r2[BIGINT_MAX_DW];
} NvBigIntModulus;

/**
*   @param Dgst Message digest for which signature needs to be generated
*   @param PrivKeyN Private key modulus
*   @param PrivkeyD Private key exponent
*   @param Sign Generated signature for the message digest and privatekey
*   @retval 0 on success and -1 on failure
*/
NvS32 NvRSASignSHA1Dgst(NvU8 *Dgst, NvBigIntModulus *PrivKeyN,
                        NvU32 *PrivKeyD, NvU32 *Sign);

/**
*   @param Dgst Message digest for which signature to be verified
*   @param PrivKeyN Private key modulus
*   @param PrivkeyD Private key exponent
*   @param Sign signature which has to be verified for the given message
*               digest with the given public key
*   @retval 0 on success and -1 on failure
*/

NvS32 NvRSAVerifySHA1Dgst(NvU8 *Dgst, NvBigIntModulus *PubKeyN,
                          NvU32 *PubKeyE, NvU32 *Sign);

/**
*   @param buf This is the pointer to the buffer where the data from pk8 can be found
*   @param BufSize This represents the size of the pk8 data buffer
*   @param P Pointer to the Buffer for P data
*   @param PSize Size of the P buffer
*   @param Q Pointer to the Buffer for Q data
*   @param QSize Size of the Q buffer
*   @retval Returns 0 on success and -1 on failure
*/
NvS32 NvGetPQFromPrivateKey(NvU8 *buf, NvU32 BufSize, NvU8 *P, NvU32 PSize, NvU8 *Q, NvU32 QSize);

/**
* Initializes the modules from P and Q values
* @param n modulus with its inverse
* @param p one of the prime numbers
* @param q secondprime number
* @param bitsize bitsize of the number
* @param swap to specify whether swap is required
*/
void NvRSAInitModulusFromPQ(NvBigIntModulus* n, NvU32* p, NvU32* q, NvU32 bitSize, NvU32 swap);

/**
* Generates the PrivateKey given the values of p and q.
* @param result contains the resultant private key modulus
* @param e exponent
* @param p one of the prime numbers
* @param q another prime number
* @param digit number of digits
*/
void NvRSAPrivateKeyGeneration(NvU32* result, NvU32* e, NvU8* p, NvU8* q, NvU32 digit);

#endif  /*  #ifdef __NV_RSA_H__ */
