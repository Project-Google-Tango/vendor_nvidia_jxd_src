/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "nvos.h"
#include "nv_sha256.h"
#include "nv_rsa.h"
#include "nv_cryptolib.h"
#include "nvsecuretool.h"
#include "nvboothost_t12x.h"
#include "nvboothost_t1xx.h"

static unsigned char PubKey[RSA_KEY_SIZE + 1];
static unsigned char PrivKey[RSA_KEY_SIZE + 1];
static unsigned char P[RSA_KEY_SIZE];
static unsigned char Q[RSA_KEY_SIZE];

extern NvU32 g_OptionChipId;

NvBigIntModulus *KeyModulus;

#ifdef DEBUG
#undef DEBUG
#endif

#define DEBUG 0
#define PEMFORMAT_START "-----BEGIN RSA PRIVATE KEY-----"
#define PEMFORMAT_END "-----END RSA PRIVATE KEY-----"

/** Macro for determining offset of element e in struct s */
#define NV_ICEIL(a,b)     (((a) + (b) - 1) / (b))
//TODO: fix magic 256 number.
#define MAX_MGF_MASKLEN (256 - 20 - 1)
#define MAX_MGF_COUNTER (NV_ICEIL(MAX_MGF_MASKLEN, 20) - 1)
#define MAX_MGF_COUNTER_LOOPS (MAX_MGF_COUNTER + 1)

static NvU32
NvRSAUtilSwapBytesInNvU32(const NvU32 Value);
static NvError
NvRSAPSSEncode(NvU8 *Msg, NvU32 EmLength,NvU8 *EncodedMsg);

NvU32
NvRSAUtilSwapBytesInNvU32(const NvU32 Value)
{
    NvU32 tmp = (Value << 16) | (Value >> 16); /* Swap halves */
    /* Swap bytes pairwise */
    tmp = ((tmp >> 8) & 0x00ff00ff) | ((tmp & 0x00ff00ff) << 8);
    return (tmp);
}

static unsigned char hextod(unsigned char *c)
{
    unsigned char val = 0,i = 0;
    for (i = 0; i < 2; i++)
    {
        if (c[i] >= '0' && c[i] <= '9')
        {
            c[i] = c[i] - '0';
        }
        else if (c[i] >= 'A' && c[i] <= 'F')
        {
            c[i] = c[i] - 'A' + 10;
        }
        else if (c[i] >= 'a' && c[i] <= 'f')
        {
            c[i] = c[i] - 'a' + 10;
        }
        else
        {
            c = 0;
        }
    }
    val = c[0] * 16 + c[1];

    return val;
}

static void ExtractPrime(unsigned char *Token , unsigned char *Prime)
{
    unsigned char count=0;
    unsigned char *lptr = Token;

    for (count = 0; count < (RSA_KEY_SIZE / 2); count++)
    {
        *(Prime + count) = hextod(lptr);
        lptr = lptr + 2;
    }
}

static
NvError NvRSAMaskGenerationFunction(
    NvU8 *mgfSeed, NvU32 maskLen,
    NvU8 *dbMaskBuffer, NvU32 hLen)
{
    NvU32 counter = 0;
    NvError e = NvSuccess;
    NvU32 T[((ARSE_SHA512_HASH_SIZE/8) * MAX_MGF_COUNTER_LOOPS) / 4];
    NvU32 hashInputBuffer[(ARSE_SHA512_HASH_SIZE / 8 / 4) +  1];

    /* +1 for the octet string C of 4 octets
     * Step 1. If maskLen > 2^32 hLen, output "mask too long" and stop.
     *
     * Step 2. Let T be the empty octet string.
     *
     * Step 3. For counter from 0 to Ceiling(maskLen / hLen) - 1, do the
     *         following:
     *         a. Convert counter to an octet string C of length 4 octets:
     *              C = I2OSP(counter, 4)
     *         b. Concatentate the hash of the seed mgfSeed and C to the octet
     *         string T:
     *         T = T || Hash(mgfSeed||C).
     */
    for (counter = 0; counter <= (NV_ICEIL(maskLen, hLen) - 1); counter++)
    {
        /* counter is already a NvU32, so no need to convert to
         * length of four octets
         */
        NvOsMemcpy(&hashInputBuffer[0], mgfSeed, hLen);
        hashInputBuffer[(hLen / 4) + 1 - 1] = NvRSAUtilSwapBytesInNvU32(counter);
        e = NvSecureGenerateSHA256Hash((NvU8*)&hashInputBuffer[0],
                                       (NvU8 *)&T[0], hLen + 4);
        NvOsMemcpy(&dbMaskBuffer[counter*hLen], &T[0], hLen);
    }

    /* Step 4. Output the leading maskLen octets
     * of T as the octet string mask.
     */

    return e;
}

NvError
NvRSAPSSEncode(NvU8 *Msg, NvU32 EmLength,NvU8 *EncodedMsg)
{
    NvError e  = NvSuccess;
    NvU32 hLen = HASH_SIZE;
    NvU32 sLen = HASH_SIZE;
    NvU32 i    = 0;
    NvU8 mHash[HASH_SIZE + 1];
    NvU8 Salt[HASH_SIZE + 1];
    NvU8 DB[RSA_KEY_SIZE];
    NvU8 dbMaskBuffer[RSA_KEY_SIZE];
    NvU8 maskedDB[RSA_KEY_SIZE];

    static NvU8 M_Prime[8 + (ARSE_SHA512_HASH_SIZE/8) +
                        (ARSE_SHA512_HASH_SIZE/8)];
    static NvU32 H_Prime[(ARSE_SHA512_HASH_SIZE/32)];

    /* random slat required? */
    NvOsMemset(Salt, 0xff, HASH_SIZE);
    NvOsMemset(DB, 0, RSA_KEY_SIZE);
    NvOsMemset(dbMaskBuffer, 0, RSA_KEY_SIZE);
    NvOsMemset(maskedDB, 0, RSA_KEY_SIZE);

    NvOsMemcpy(mHash, Msg, HASH_SIZE);
    NvOsMemcpy(&M_Prime[8], &mHash[0], hLen);
    NvOsMemcpy(&M_Prime[8 + HASH_SIZE], &Salt[0], sLen);

#if DEBUG
    {
        NvU32 i = 0;
        NvAuPrintf("enc :mHash is \n");
        for (i = 0; i < 32; i++)
            NvAuPrintf("%0x ", mHash[i]);
    }
#endif

#if DEBUG
    {
        NvU32 i = 0;
        NvAuPrintf("enc :M_Prime is \n");
        for (i = 0; i < 72; i++)
            NvAuPrintf("%0x ", M_Prime[i]);
    }
#endif

    e = NvSecureGenerateSHA256Hash((NvU8 *)&M_Prime[0],
                                   (NvU8 *)&H_Prime[0], 8 + hLen + sLen);
    if (e != NvSuccess)
        goto fail;

#if DEBUG
    {
        NvU32 i = 0;
        NvAuPrintf("enc H_Prime is \n");
        for (i = 0; i < 32; i++)
            NvAuPrintf("%0x ", H_Prime[i]);
    }
#endif

    DB[RSA_KEY_SIZE - hLen - sLen - 2] = 0x01;
    NvOsMemcpy(&DB[RSA_KEY_SIZE - hLen - sLen - 1], Salt, HASH_SIZE);
    e = NvRSAMaskGenerationFunction((NvU8 *)&H_Prime[0],
                                     RSA_KEY_SIZE - hLen - 1,
                                     dbMaskBuffer, HASH_SIZE);
    if (e != NvSuccess)
        goto fail;

    for (i = 0; i <223; i++)
    {
        maskedDB[i] = DB[i] ^ dbMaskBuffer[i];
    }

    maskedDB[0] &= ~(0xFF << (8 - 1));
    NvOsMemcpy(EncodedMsg, maskedDB, RSA_KEY_SIZE - hLen - 1);
    NvOsMemcpy(&EncodedMsg[RSA_KEY_SIZE - hLen - 1], &H_Prime[0], HASH_SIZE);
    EncodedMsg[RSA_KEY_SIZE - 1] = 0xbc;
#if DEBUG
    {
        NvU32 i = 0;
        NvAuPrintf("EncodedMsg is \n");
        for (i = 0; i < 256; i++)
            NvAuPrintf("%0x ", EncodedMsg[i]);
    }
#endif
    goto clean;

fail:
    NvAuPrintf("RSA PSS encode failed\n");
clean:
    return e;
}

static NvS32 NvRSASignSHA256Dgst(
        NvU8 *Dgst, NvBigIntModulus *PrivKey_n,
        NvU32 *PrivKey_d, NvU32 *Sign)
{
    NvS32 retval      = -1;
    NvU32 *EncodedMsg = NULL;
    NvError e         = NvSuccess;

    if (Dgst == NULL      || Sign == NULL ||
        PrivKey_n == NULL || PrivKey_d == NULL)
    {
        return -1;
    }

    EncodedMsg = (NvU32*) NvOsAlloc(RSANUMBYTES);
    if (EncodedMsg == NULL)
        goto fail;

    NvOsMemset(EncodedMsg, 0, RSANUMBYTES);

    /** Preparing the Encoded message from Msg Hash and Padding bytes */
    e = NvRSAPSSEncode(Dgst, RSANUMBYTES, (NvU8*)EncodedMsg);

    {
        NvU32 i   = 0;
        NvU8 temp = 0;
        for (i = 0; i < (RSANUMBYTES/2); i++)
        {
            temp = ((NvU8*)EncodedMsg)[i];
            ((NvU8*)EncodedMsg)[i] = ((NvU8*)EncodedMsg)[RSANUMBYTES - 1 - i];
            ((NvU8*)EncodedMsg)[RSANUMBYTES - 1 - i] = temp;
        }
    }
    /** Generating the signature */
    NvRSAEncDecMessage(Sign, EncodedMsg, PrivKey_d, PrivKey_n);

    retval = 1;

fail:
    if (e != NvSuccess)
       NvAuPrintf("NvRSASignSHA256Dgst failed\n");
    NvOsFree(EncodedMsg);
    return (retval);
}

/* TODO: Currently we support generating the key pair only from file of
 * polarssl key format. Extend it to support other format as well.
 */

NvError
NvSecureGenerateKeyPair(const char *KeyFile)
{
    NvError e            = NvSuccess;
    char *err_str        = NULL;
    NvOsFileHandle hFile = NULL;
    NvU8 *tokenp         = NULL;
    NvU8 *tokenq         = NULL;
    char *KeyBuf         = NULL;
    NvU32 FileSize       = 0;
    size_t BytesRead     = 0;
    NvOsStatType FileStat;

    e = NvOsFopen(KeyFile, NVOS_OPEN_READ, &hFile);
    VERIFY(e == NvSuccess, err_str = "file open failed"; goto fail);

    e = NvOsFstat(hFile, &FileStat);
    VERIFY(e == NvSuccess, err_str = "file stat failed"; goto fail);

    FileSize= (NvU32)FileStat.size;

    KeyBuf = (char *)NvOsAlloc(FileSize);
    if (KeyBuf == NULL)
    {
        printf("malloc failed @%d\n",__LINE__);
        goto fail;
    }

    e = NvOsFread(hFile, KeyBuf, FileSize, &BytesRead);
    VERIFY(e == NvSuccess, err_str = "file read failed"; goto fail);
    NvOsFclose(hFile);
    hFile = NULL;

    KeyModulus = (NvBigIntModulus*)NvOsAlloc(sizeof(NvBigIntModulus));
    if (KeyModulus == NULL)
    {
        printf("malloc failed @%d\n",__LINE__);
        goto fail;
    }

    //check the format of key
    if (NvOsMemcmp(KeyBuf, PEMFORMAT_START, sizeof(PEMFORMAT_START) - 1) == 0)
    {
        NvU32 Base64Size = 0;
        NvU8 *Base64Buf = NULL;
        NvU32 DERKeySize = FileSize - sizeof(PEMFORMAT_END);
        char *pBufPEM = KeyBuf + sizeof(PEMFORMAT_START);
        if (NvOsMemcmp(KeyBuf + DERKeySize, PEMFORMAT_END, sizeof(PEMFORMAT_END) - 1) == 0)
        {
            NvAuPrintf("Processing PEM Format Key\n");
            DERKeySize -= sizeof(PEMFORMAT_START);
            e = NvOsBase64Decode(pBufPEM, DERKeySize, NULL, &Base64Size);
            VERIFY(e == NvSuccess, err_str = "Base64 decoding failed"; goto fail);
            Base64Buf = NvOsAlloc(Base64Size);
            if (Base64Buf == NULL)
            {
                NvAuPrintf("malloc failed @%d\n",__LINE__);
                goto fail;
            }
            e = NvOsBase64Decode(pBufPEM, DERKeySize, Base64Buf, &Base64Size);
            if (e != NvSuccess)
            {
                NvAuPrintf("Base64 Decoding failed\n");
                NvOsFree(Base64Buf);
                goto fail;
            }

            if ((NvGetPQFromPrivateKey(Base64Buf, Base64Size, P,RSA_KEY_SIZE/2, Q,RSA_KEY_SIZE/2))== -1)
            {
                NvAuPrintf("PQ extraction failed\n");
                NvOsFree(Base64Buf);
                goto fail;
            }
            NvOsFree(Base64Buf);
        }
        else
            NvAuPrintf("Key Processing Failed\n");
    }
    else
    {
        NvAuPrintf("Processing PolarSSl Format Key\n");
        tokenp = (NvU8 *) strstr(KeyBuf,"P = ");
        tokenq = (NvU8 *) strstr(KeyBuf,"Q = ");
        tokenq = tokenq + NvOsStrlen("Q = ");
        tokenp = tokenp + NvOsStrlen("P = ");

        // check the rsa priv key length
        // prime number hexform should not be less than 256 bits
        if ((tokenq - tokenp) < RSA_KEY_SIZE)
        {
            err_str = "Key length not supported.Use 2048 bit rsa_priv.txt";
            e = NvError_NotImplemented;
            goto fail;
        }

        ExtractPrime(tokenq, Q);
        ExtractPrime(tokenp, P);
    }

#if DEBUG
{
    NvU32 i = 0;
    for (i = 0; i < RSANUMBYTES/2; i++)
        NvAuPrintf("%0x  ", P[i]);
    NvAuPrintf("\n\n");

    for (i = 0; i < RSANUMBYTES/2; i++)
        NvAuPrintf("%0x  ", Q[i]);
    NvAuPrintf("\n\n");
}
#endif

    /*  Updating the SHA-1 engine hash value with the blob content and
     *  writing to the output file simultaneously to the O/p file
     */

    /*  Generating modulus(n) */
    NvRSAInitModulusFromPQ(KeyModulus, (NvU32 *)P,
                           (NvU32 *) Q, RSA_LENGTH, NV_TRUE);
#if DEBUG
{
    NvU32 i = 0;
    NvAuPrintf("KeyModulus is");
    i = 0;
    for (i = 0;i < RSANUMBYTES / 4; i++)
        NvAuPrintf("%0x  ", KeyModulus->n[i]);
    NvAuPrintf("\n\n");
}
#endif

    ((NvU32*)PubKey)[0] = RSA_PUBLIC_EXPONENT_VAL;
    NvRSAPrivateKeyGeneration((NvU32 *)PrivKey, (NvU32*)PubKey,
                              (NvU8*)P, (NvU8*)Q, (RSANUMBYTES / 4));

goto clean;

fail:
    if (hFile)
        NvOsFclose(hFile);
    NvAuPrintf("%s\n",err_str);
    return NvError_BadParameter;

clean:
    NvOsFree(KeyBuf);
    return e;
}

NvError NvSecureGetKeys(const char *spub, const char *spriv)
{
    NvError e             = NvSuccess;
    char *err_str         = NULL;
    NvOsFileHandle hFile1 = NULL;
    NvOsFileHandle hFile2 = NULL;

    e = NvOsFopen(spub, NVOS_OPEN_WRITE | NVOS_OPEN_CREATE, &hFile1);
    VERIFY(e == NvSuccess, err_str = "file open failed"; goto fail);

    e = NvOsFopen(spriv, NVOS_OPEN_WRITE | NVOS_OPEN_CREATE, &hFile2);
    VERIFY(e == NvSuccess, err_str = "file open failed"; goto fail);

    e = NvOsFwrite(hFile1, (NvU8 *)KeyModulus, sizeof(NvBigIntModulus));
    VERIFY(e == NvSuccess, err_str = "file write failed"; goto fail);

    e = NvOsFwrite(hFile2, PrivKey, RSA_KEY_SIZE);
    VERIFY(e == NvSuccess, err_str = "file write failed"; goto fail);

fail:
    if (hFile1)
        NvOsFclose(hFile1);
    if (hFile2)
        NvOsFclose(hFile2);
    if (err_str)
        NvAuPrintf("%s\n", err_str);
    return e;
}


NvError
NvSecureGenerateSHA256Hash(NvU8 *Src, NvU8 *Dst, NvU32 Length)
{
    NvError e              = NvSuccess;
    NvSHA256_Data *SHAData = NULL;
    NvSHA256_Hash *SHAHash = NULL;

    SHAData = (NvSHA256_Data*) NvOsAlloc(sizeof(NvSHA256_Data));
    if (SHAData == NULL)
    {
        printf("malloc failed @%d\n", __LINE__);
        goto fail;
    }

    SHAHash = (NvSHA256_Hash*) NvOsAlloc(sizeof(NvSHA256_Hash));
    if (SHAHash == NULL)
    {
        printf("malloc failed @%d\n", __LINE__);
        goto fail;
    }

    if (NvSHA256_Init(SHAData) == -1)
        goto fail;

    if (NvSHA256_Update(SHAData, Src,Length ) == -1)
       goto fail;

    /*  Finalizing the SHA-1 engine hash value */
    if (NvSHA256_Finalize(SHAData, SHAHash) == -1)
    {
        NvAuPrintf("NvSHA256_Finalize failed...!\n");
        goto fail;
    }
    NvOsMemcpy(Dst,SHAHash->Hash,HASH_SIZE);

#if DEBUG
    NvAuPrintf("Generated Hash is for length is %d\n",Length);
    {
        NvU32 i = 0;
        for (i = 0; i < 32; i++)
            NvAuPrintf(" %0x ", SHAHash->Hash[i]);
    }
#endif
    goto clean;

fail:
    e= NvError_BadParameter;
clean:
    if (SHAData)
        NvOsFree(SHAData);
    if (SHAHash)
        NvOsFree(SHAHash);
    return e;
}

NvError
NvSecurePkcSign(NvU8 *Src, NvU8 *Dst, NvU32 Length)
{
    NvError e = NvSuccess;
    NvU8 Hash[HASH_SIZE +1];
    NvS32 Ret;

    NvOsMemset(Hash,0,HASH_SIZE + 1);
#if DEBUG
    {
        NvU32 i = 0;
        NvAuPrintf("\nData is \n");
        for (i = 0; i < Length; i++)
           NvAuPrintf("%0x  ", Src[i]);
        NvAuPrintf("\n\n");
    }
#endif
    e = NvSecureGenerateSHA256Hash(Src, Hash, Length);
    if (e != NvSuccess)
        goto fail;

    Ret = NvRSASignSHA256Dgst(Hash, KeyModulus, (NvU32 *)PrivKey,(NvU32 *)Dst);
    if (Ret == -1)
    {
        NvAuPrintf("RSA_Sign failed...!\n");
        goto fail;
    }
#if DEBUG
    {
        NvU32 i = 0;
        NvAuPrintf("\nSignature is \n");
        for (i = 0; i < RSANUMBYTES; i++)
            NvAuPrintf("%0x  ", Dst[i]);
        NvAuPrintf("\n\n");
    }
#endif
    goto clean;

fail:
    NvAuPrintf("NvSecurePkcSign failed...!\n");
    e = NvError_BadParameter;
clean:
    return e;
}

void
NvSecurePkcGetPubKey(NvU8 *Dst)
{
    switch (g_OptionChipId)
    {
        case 0x40:
            NvSecurePkcGetPubKeyT12x(Dst);
            break;
        default:
            NvSecurePkcGetPubKeyT1xx(Dst);
    }

}


NvError
NvSecureHostRcmCreateMsgFromBuffer(
    NvU32 OpCode,
    NvBootHash *RandomAesBlock,
    NvU8 *Args,
    NvU32 ArgsLength,
    NvU32 PayloadLength,
    NvU8 *PayloadBuffer,
    NvDdkFuseOperatingMode OpMode,
    NvU8 *Sbk,
    NvU8 **pMsgBuffer)
{
    NvError e              = NvSuccess;
    NvU32 MsgBufferLength  = 0;
    NvU8 *MsgBuffer        = NULL;
    NvU8 *MsgPayloadBuffer = NULL;

    /* create message buffer */
    switch (g_OptionChipId)
    {
        case 0x40:
            MsgBufferLength = NvBootHostT12xRcmGetMsgBufferLength(PayloadLength);
            break;
        default:
            MsgBufferLength = NvBootHostT1xxRcmGetMsgBufferLength(PayloadLength);
            break;
    }

    MsgBuffer = NvOsAlloc(MsgBufferLength);
    if (!MsgBuffer)
    {
        e = NvError_InsufficientMemory;
        goto done;
    }
    NvOsMemset(MsgBuffer, 0, MsgBufferLength);

    /* initialize message */
    switch (g_OptionChipId)
    {
        case 0x40:
            NvBootHostT12xRcmInitMsg(MsgBuffer, MsgBufferLength, OpCode,
                    RandomAesBlock, Args, ArgsLength, PayloadLength);
            MsgPayloadBuffer = NvBootHostT12xRcmGetMsgPayloadBuffer(MsgBuffer);
            break;
        default:
            NvBootHostT1xxRcmInitMsg(MsgBuffer, MsgBufferLength, OpCode,
                    RandomAesBlock, Args, ArgsLength, PayloadLength);
            MsgPayloadBuffer = NvBootHostT1xxRcmGetMsgPayloadBuffer(MsgBuffer);
    }

    if (PayloadLength)
        NvOsMemcpy(MsgPayloadBuffer, PayloadBuffer, PayloadLength);

    /* sign message */
    switch (g_OptionChipId)
    {
        case 0x40:
            e = NvBootSecurePKCRcmSignMsgT12x(MsgBuffer);
            break;
        default:
            e = NvBootSecurePKCRcmSignMsgT1xx(MsgBuffer);
            break;
    }
    if (e != NvSuccess )
        goto done;

done:
    if (e != NvError_Success) {
        NvOsFree(MsgBuffer);
        MsgBuffer = NULL;
    }

    *pMsgBuffer = MsgBuffer;

    return e;
}
