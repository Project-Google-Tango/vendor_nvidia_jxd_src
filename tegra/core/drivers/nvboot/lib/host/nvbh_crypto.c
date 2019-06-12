/**
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * TODO / Notes
 * - Add doxygen commentary
 */

#include "nvboothost.h"

#include "nvassert.h"
#include "nvos.h"
#include "nvapputil.h"
#include "nvaes_ref.h"

/* Lengths, in bytes */
#define KEY_LENGTH (128/8)

#define ICEIL(a, b) (((a) + (b) - 1)/(b))

#define AES_CMAC_CONST_RB 0x87  // from RFC 4493, Figure 2.2

/* Local function declarations */
static void
ApplyCbcChainData(NvU8 *CbcChainData, NvU8 *src, NvU8 *dst);

/* Implementation */
static NvU8 ZeroKey[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static void
PrintVector(char *Name, NvU32 NumBytes, NvU8 *Data) 
{
    NvU32 i;
    
    NvAuPrintf("%s [%d] @%p", Name, NumBytes, Data);
    for (i=0; i<NumBytes; i++) {
        if ( i % 16 == 0 )
            NvAuPrintf(" = ");
        
        NvAuPrintf("%02x", Data[i]);

        if ( (i+1) % 16 != 0 )
            NvAuPrintf(" ");
    }
    NvAuPrintf("\n");
}


static void
ApplyCbcChainData(NvU8 *CbcChainData, NvU8 *src, NvU8 *dst)
{
    NvU32 i;

    for (i = 0; i < 16; i++)
    {
        *dst++ = *src++ ^ *CbcChainData++;
    }
}

static void
GenerateKeySchedule(NvU8 *Key, NvU8 *KeySchedule)
{
    /* Expand the key to produce a key schedule. */
    NvAesExpandKey(Key, KeySchedule);
}

static void
EncryptBuffer(
    NvU8 *KeySchedule,
    NvU8 *Src,
    NvU8 *Dst,
    NvU32 NumAesBlocks,
    NvBool EnableDebug)
{
    NvU32 i;
    NvU8 *CbcChainData;
    NvU8 TmpData[KEY_LENGTH];

    NV_ASSERT(KeySchedule != NULL);
    NV_ASSERT(Src != NULL);
    NV_ASSERT(Dst != NULL);

    CbcChainData = ZeroKey; /* Convenient array of 0's for IV */

    for (i = 0; i < NumAesBlocks; i++)
    {
        if (EnableDebug)
        {
            NvAuPrintf("EncryptBuffer: block %d of %d\n", i, NumAesBlocks);
            PrintVector("AES Src", KEY_LENGTH, Src);
        }

        /* Apply the chain data */
        ApplyCbcChainData(CbcChainData, Src, TmpData);

        if (EnableDebug) PrintVector("AES Xor", KEY_LENGTH, TmpData);

        /* Encrypt the AES block */
        NvAesEncrypt(TmpData, KeySchedule, Dst);

        if (EnableDebug) PrintVector("AES Dst", KEY_LENGTH, Dst);
        
        /* Update pointers for next loop. */
        CbcChainData = Dst;
        Src += KEY_LENGTH;
        Dst += KEY_LENGTH;
    }
}

static void
LeftShiftVector(
    NvU8 *In,
    NvU8 *Out,
    NvU32 Size)
{
    NvU32 i;
    NvU8 Carry = 0;
    
    for (i=0; i<Size; i++) {
        NvU32 j = Size-1-i;
        Out[j] = (In[j] << 1) | Carry;
        Carry = In[j] >> 7; // get most significant bit
    }
}

static void
SignBuffer(
    NvU8 *KeySchedule,
    NvU8 *Src,
    NvU8 *Dst,
    NvU32 NumAesBlocks,
    NvBool EnableDebug)
{
    NvU32 i;
    NvU8 *CbcChainData;

    NvU8 L[KEY_LENGTH];
    NvU8 K1[KEY_LENGTH];
    NvU8 TmpData[KEY_LENGTH];
    
    NV_ASSERT(KeySchedule != NULL);
    NV_ASSERT(Src != NULL);
    NV_ASSERT(Dst != NULL);
    
    CbcChainData = ZeroKey; /* Convenient array of 0's for IV */

    // compute K1 constant needed by AES-CMAC calculation

    for (i=0; i<KEY_LENGTH; i++)
        TmpData[i] = 0;
    
    EncryptBuffer(KeySchedule, TmpData, L, 1, EnableDebug);

    if (EnableDebug) PrintVector("AES(key, nonce)", KEY_LENGTH, L);

    LeftShiftVector(L, K1, sizeof(L));

    if (EnableDebug) PrintVector("L", KEY_LENGTH, L);
    
    if ( (L[0] >> 7) != 0 ) // get MSB of L
        K1[KEY_LENGTH-1] ^= AES_CMAC_CONST_RB;
    
    if (EnableDebug) PrintVector("K1", KEY_LENGTH, K1);

    // compute the AES-CMAC value
    
    for (i = 0; i < NumAesBlocks; i++)
    {
        /* Apply the chain data */
        ApplyCbcChainData(CbcChainData, Src, TmpData);

        /* for the final block, XOR K1 into the IV */
        if ( i == NumAesBlocks-1 )
            ApplyCbcChainData(TmpData, K1, TmpData);
            
        /* Encrypt the AES block */
        NvAesEncrypt(TmpData, KeySchedule, (NvU8*)Dst);

        if (EnableDebug)
        {
            NvAuPrintf("SignBuffer: block %d of %d\n", i, NumAesBlocks);

            PrintVector("AES-CMAC Src", KEY_LENGTH, Src);
            PrintVector("AES-CMAC Xor", KEY_LENGTH, TmpData);
            PrintVector("AES-CMAC Dst", KEY_LENGTH, (NvU8*)Dst);
        }

        /* Update pointers for next loop. */
        CbcChainData = (NvU8*)Dst;
        Src += KEY_LENGTH;
    }


    if (EnableDebug) PrintVector("AES-CMAC Hash", KEY_LENGTH, (NvU8*)Dst);
}

void
NvBootHostCryptoEncryptSignBuffer(
    NvBool DoEncrypt,
    NvBool DoSign,
    NvU8 *Key,
    NvU8 *Src,
    NvU32 NumAesBlocks,
    NvU8 *SigDst,
    NvBool EnableDebug)
{
    NvU8 KeySchedule[4*NVAES_STATECOLS*(NVAES_ROUNDS+1)];
    
    NV_ASSERT(Key != NULL);
    NV_ASSERT(Src != NULL);
    NV_ASSERT(SigDst != NULL);

    if (EnableDebug)
    {
        NvAuPrintf("EncryptAndSign: Length = %d blocks\n", NumAesBlocks);
        PrintVector("AES Key", KEY_LENGTH, Key);
    }

    GenerateKeySchedule(Key, KeySchedule);

//    NumAesBlocks = ICEIL(Length, KEY_LENGTH);

    if (DoEncrypt)
    {
        if (EnableDebug)
            NvAuPrintf("EncryptAndSign: begin encryption\n");

        /* Perform this in place, resulting in Src being encrypted. */
        EncryptBuffer(KeySchedule, Src, Src, NumAesBlocks, EnableDebug);

        if (EnableDebug)
            NvAuPrintf("EncryptAndSign: end encryption\n");
    }

    if (DoSign)
    {
        if (EnableDebug) NvAuPrintf("EncryptAndSign: begin signing\n");

        /* Encrypt the encrypted data, overwriting the result in signature. */
        SignBuffer(KeySchedule, Src, SigDst, NumAesBlocks, EnableDebug);

        if (EnableDebug) NvAuPrintf("EncryptAndSign: end signing\n");
    }

}

