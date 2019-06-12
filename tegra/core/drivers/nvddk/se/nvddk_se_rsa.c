/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvos.h"
#include "nvddk_blockdev.h"
#include "nvddk_se_blockdev.h"
#include "nvcommon.h"
#include "nvassert.h"
#include "nvddk_se_core.h"
#include "nvbct.h"
#include "nvddk_fuse.h"
#include "nvddk_se_private.h"

#define NV_ICEIL(a,b)      (((a) +       (b)  - 1) /  (b))
#define MAX_MGF_MASKLEN (256 - 20 - 1)
#define MAX_MGF_COUNTER (NV_ICEIL(MAX_MGF_MASKLEN, 20) - 1)
#define MAX_MGF_COUNTER_LOOPS (MAX_MGF_COUNTER + 1)

#define ARSE_RSA_MAX_MODULUS_SIZE               2048
#define ARSE_RSA_MAX_EXPONENT_SIZE              2048
#define ARSE_RSA_PSS_SALT_LENGTH_BITS           256

/**
 * Defines the Public Key Exponent used by the Boot ROM.
 */
enum {NVDDK_SE_RSA_PUBLIC_KEY_EXPONENT = 0x00010001};


/**
 *Swaps the position one with position two in the given string
 Input
  @param Str Input buffer on which operation is to be performed
  @param one byte position to be swaped with two
  @param two byte position to be swaped with one
 Return:
  NvError_Success If the operation is successful
  NvError_InvalidAddress If the operation fails
 */
static NvError swap(NvU8 *Str, NvU32 one, NvU32 two)
{
    NvU8 temp;

    if (Str == NULL)
        return NvError_InvalidAddress;
    temp = Str[one];
    Str[one] = Str[two];
    Str[two] = temp;
    return NvSuccess;
}

/**
 *Change th endianess of the given buffer
 Input:
  @param str Input Buffer whose endianess is to be changed
  @param Size size of the Input buffer
 Return:
  NvError_Success If the operation is successful
  NvError_InvalidAddress If the operation fails
 */

static NvError ChangeEndian(NvU8 *Str, NvU32 Size)
{
    NvU32 i;
    NvError e = NvSuccess;

    if (Str == NULL)
        return NvError_InvalidAddress;

    for (i = 0; i < Size; i += 4)
    {
        if ((e = swap(Str, i, i + 3)) != NvSuccess)
            return e;
        if ((e = swap(Str, i + 1, i + 2)) != NvSuccess)
            return e;
    }
    return NvSuccess;
}

/**
 *Reverses a NvU32 input data
 Illustration
  Input 0xABCD
  Output 0xDCBA
 Input:
  @param Value Input data to be reserved
 */

static NvU32
NvDdkUtilSwapBytesInNvU32(const NvU32 Value)
{
    NvU32 tmp = (Value << 16) | (Value >> 16); /* Swap halves */
    /* Swap bytes pairwise */
    tmp = ((tmp >> 8) & 0x00ff00ff) | ((tmp & 0x00ff00ff) << 8);
    return (tmp);
}


/**
 *Reverses a given buffer
 Illustration
  Input 0xABCDEF
  Output 0xFEDCBA
 Input:
  @param original Input buffer to be reserved
  @param listSize Input buffer size
 */

static void SeReverseList(NvU8 *original, NvU32 listSize)
{
    NvU8 i, j, temp;

    for(i = 0, j = listSize - 1; i < j; i++, j--)
    {
        temp = original[i];
        original[i] = original[j];
        original[j] = temp;
    }

}

/**
 *Performa SHAHash over given input
 Input:
  @param pInputMessage Input messgae on which SHAHash is to be calculated
  @param InputMessageSizeBytes size of the Input buffer in bytes
  @param Outputsizeinbytes size of the output buffer in bytes
  @param pOutputDestination buffer in which output is stored
  @param HashAlgorithm SHA Algorithm to be performed
 Return:
  NvError_Success If the operation is successful
 */

static NvError NvDdkSeSHAHash(
    NvU32 *pInputMessage,
    NvU32 InputMessageSizeBytes,
    NvU32 Outputsizeinbytes,
    NvU32 *pOutputDestination,
    NvDdkSeShaEncMode HashAlgorithm)
{
    NvDdkSeShaInitInfo seinitinfo;
    NvDdkSeShaUpdateArgs seupdateinfo;
    NvDdkSeShaFinalInfo sefinalinfo;
    NvDdkBlockDevHandle sehandle = NULL;
    NvError e;

   //  open the SE device
    e = NvDdkSeBlockDevOpen(
                            0,
                            0,
                            &sehandle);

    if (e != NvSuccess)
    {
        NvOsDebugPrintf("\nSE device opening failed\n");
        goto fail;
    }

    // SHA Init InputArgs
    seinitinfo.TotalMsgSizeInBytes = InputMessageSizeBytes;
    seinitinfo.SeSHAEncMode = HashAlgorithm;

    //  Init the SHA
    e = NvDdkSeBlockDevIoctl(
                        sehandle,
                        NvDdkSeBlockDevIoctlType_SHAInit,
                        sizeof(NvDdkSeShaInitInfo),
                        0,
                        &seinitinfo,
                        0);

    if (e != NvSuccess)
    {
        NvOsDebugPrintf("\nSE SHA init failed\n");
        goto fail;
    }

    //  Updating the SHA-1 engine hash value
    seupdateinfo.InputBufSize = InputMessageSizeBytes;
    seupdateinfo.SrcBuf = (NvU8 *)pInputMessage;

    //  Update SHA
    e = NvDdkSeBlockDevIoctl(
                        sehandle,
                        NvDdkSeBlockDevIoctlType_SHAUpdate,
                        sizeof(NvDdkSeShaUpdateArgs),
                        0,
                        &seupdateinfo,
                        0);

    if (e != NvSuccess)
    {
        NvOsDebugPrintf("\nSE SHA Update failed\n");
        goto fail;
    }

    //Input Args for ShaFinal Ioctl
    sefinalinfo.HashSize = Outputsizeinbytes;
    sefinalinfo.OutBuf = (NvU8 *)pOutputDestination;

    //  Final SHA
    e = NvDdkSeBlockDevIoctl(
                        sehandle,
                        NvDdkSeBlockDevIoctlType_SHAFinal,
                        0,
                        0,
                        0,
                        &sefinalinfo);

    if (e != NvSuccess)
    {
        NvOsDebugPrintf("\nSE SHA Final failed\n");
        goto fail;
    }
fail:
    //  Close the device
    NvDdkSeBlockDevClose(sehandle);
    return e;
}


/**
 *Performa Modular exponentiation over given input
 Input:
  @param RsaKeySlot Input RsaKeySlot to be used
  @param input Input data on which modular exp will be calculated
  @param output Output of the operation
 Return:
  NvError_Success If the operation is successful
 */

static NvError NvDdkRsaModularExponentiation(
    NvU8 RsaKeySlot,
    NvU32 *input,
    NvU8 *PublicKeyModulus,
    NvU32 RsaKeySizeBits,
    NvU32 *output)
{
    NvError e = NvSuccess;
    NvDdkBlockDevHandle sehandle = NULL;
    NvSeRsaKeyDataInfo RSAKeyDataInfo;
    NvSeRsaOutDataInfo RSAOutDataInfo;
    NvU32 modulus[ARSE_RSA_MAX_MODULUS_SIZE / 8 / 4] = {0};
    NvU32 exponent = 0;
    NvBctHandle hbct = NULL;
    NvU32 BctLength = 0;
    NvU32 size = 0;
    NvU32 instance = 0;

     //open the SE device
     e = NvDdkSeBlockDevOpen(
                             0,
                             0,
                             &sehandle);

    if (e)
    {
        NvOsDebugPrintf("\nSE device opening failed\n");
        goto fail;
    }

    size = sizeof(modulus);

    if (!PublicKeyModulus)
    {
        //reading modulus from BCT
        hbct = NvOsAlloc(sizeof(NvBctHandle));
        NV_CHECK_ERROR_CLEANUP(NvBctInit(&BctLength, NULL, &hbct));
        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(hbct, NvBctDataType_RsaKeyModulus,
                 &size, &instance,(NvU32 *) modulus));
    }
    //filling exponent
    exponent = NVDDK_SE_RSA_PUBLIC_KEY_EXPONENT;

    // reverse & endianess operation over modulus
    ChangeEndian((NvU8 *)modulus,size * 4);
    SeReverseList((NvU8 *)modulus,size * 4);

    //Fill Input arguments
    if (!PublicKeyModulus)
    {
        RSAKeyDataInfo.ModulusSizeInBytes = size * 4;
        RSAKeyDataInfo.Modulus = (NvU8 *)modulus;
    }
    else
    {
        RSAKeyDataInfo.ModulusSizeInBytes = RsaKeySizeBits;
        RSAKeyDataInfo.Modulus = (NvU8 *)PublicKeyModulus;
    }
    RSAKeyDataInfo.KeySlot = RsaKeySlot;
    RSAKeyDataInfo.InputBuf = (NvU8 *)(input);
    RSAKeyDataInfo.ExponentSizeInBytes = sizeof(NvU32);
    RSAKeyDataInfo.Exponent = (NvU8 *)&exponent;
    RSAOutDataInfo.OutputBuf = (NvU8 *)output;

    //Call Driver API to perform Modular Exponentiation
    e = NvDdkSeBlockDevIoctl(
                        sehandle,
                        NvDdkSeBlockDevIoctlType_RSAModularExponentiation,
                        sizeof(NvSeRsaKeyDataInfo),
                        ARSE_RSA_MAX_MODULUS_SIZE/8,
                        &RSAKeyDataInfo,
                        &RSAOutDataInfo);

    if (e != NvSuccess)
    {
        NvOsDebugPrintf("\nSE RSA ModExpo operation failed\n");
        goto fail;
    }

fail:
    NvBctDeinit(hbct);
    //  Close the device
    NvDdkSeBlockDevClose(sehandle);
    return e;
}



static NvError NvDdkSeMaskGenerationFunction(
        NvU8 *mgfSeed,
        NvU32 maskLen,
        NvU8 *dbMaskBuffer,
        NvU8 HashAlgorithm,
        NvU32 hLen)
{
    NvU32 counter = 0;
    NvU32 T[((NvDdkSeShaResultSize_Sha512 / 8)
                        * MAX_MGF_COUNTER_LOOPS) / 4] = {0};
    NvU32 hashInputBuffer[(NvDdkSeShaResultSize_Sha512 / 8 / 4)
                        +  1] = {0}; // +1 for the octet string C of 4 octets

    // Step 1. If maskLen > 2^32 hLen, output "mask too long" and stop.

    // Step 2. Let T be the empty octet string.

    // Step 3. For counter from 0 to Ceiling(maskLen / hLen) - 1, do the
    //         following:
    //         a. Convert counter to an octet string C of length 4 octets:
    //              C = I2OSP(counter, 4)
    //         b. Concatentate the hash of the seed mgfSeed and C to the octet
    //         string T:
    //         T = T || Hash(mgfSeed||C).

    for (counter = 0; counter <= (NV_ICEIL(maskLen, hLen) - 1); counter++)
    {
        // counter is already a NvU32, so no need to convert to
        // length of four octets
        NvOsMemcpy(&hashInputBuffer[0], mgfSeed, hLen);
        hashInputBuffer[(hLen / 4) + 1 - 1] = NvDdkUtilSwapBytesInNvU32(counter);

        //SHAHash over the input data
        NvDdkSeSHAHash(
            &hashInputBuffer[0],
            hLen + 4,
            hLen,
            &T[0],
            HashAlgorithm);
        NvOsMemcpy(&dbMaskBuffer[counter*hLen], &T[0], hLen);
    }

    // Step 4. Output the leading maskLen octets of T as the octet string mask.

    return NvSuccess;
}


/**
 *  NvDdkSeRsaPssSignatureVerify():
 *      Performs an RSASSA-PSS-VERIFY signature verification operation.
 *
 *  Input:
 *      The key must be loaded into a RSA key slot before calling
 *      this function.
 *
 *  @param RsaKeySlot Specifies the SE RSA key slot to use.
 *  @param pInputMessage Specifies the pointer to the message whose
 *          signature is to be verified
 *  @param pMessageHash Allows caller to specify the SHA hash of the
 *                      input message already completed.
 *  @param pSignature Signature to be verified. The length of the Signature
 *                    pointed to by pSignature must be of length k, where
 *                    k is the length in octets of the RSA modulus n.
 *  @param HashAlgorithm Hash algorithm to be used (used for both MGF and mHash)
 *  @param sLen Salt length to be used. Should be either 0 or hLen.
 *
 *  Returns:
 *  @retval NvError_Success : Valid signature calculated.
 *  @retval NvError_RsaPssVerify_Inconsistent : Invalid signature calculated.
 *
 */

NvError
NvDdkSeRsaPssSignatureVerify(
    NvU8 RsaKeySlot,
    NvU32 RsaKeySizeBits,
    NvU32 *pInputMessage,
    NvU32 *pMessageHash,
    NvU32 InputMessageLengthBytes,
    NvU32 *pSignature,
    NvDdkSeShaEncMode HashAlgorithm,
    NvU8 *PublicKeyModulus,
    NvS8 sLen)
{
    NvError e = NvSuccess;
    static NvU32   *pMessageRepresentative = NULL;
    static NvU32   MessageRepresentative[ARSE_RSA_MAX_MODULUS_SIZE / 32] ={0};
    static NvU32   mHash[NvDdkSeShaResultSize_Sha256 / 32] = {0}; // declare mHash to be maximum digest size supported
    static NvU8    *EM, *H;//, *salt;
    static NvU8    *maskedDB;
    static NvU32   maskedDBLen = 0;
    static NvU32   hLen = 0;
    const NvU32   emLen = NV_ICEIL(RsaKeySizeBits - 1, 8);
    const NvU32   maxemLen = NV_ICEIL(ARSE_RSA_MAX_MODULUS_SIZE - 1, 8);
    static NvU8    LowestBits = 0;
    static NvU8    FirstOctetMaskedDB = 0;
    static NvU8    M_Prime[8 + (NvDdkSeShaResultSize_Sha512/8)
                                    + (NvDdkSeShaResultSize_Sha512/8)] = {0};
    static NvU32   H_Prime[(NvDdkSeShaResultSize_Sha512/32)] = {0};

    // dbMaskBuffer size declared to be maximum maskLen supported:
    // emLen - hLen - 1 = (Ceiling(2048-1)/8) - NvDdkSeShaResultSize_Sha512 in bytes - 1
    //                  = 256 - 64 - 1 = 191 bytes.
    NvU8    dbMask[maxemLen - (NvDdkSeShaResultSize_Sha1/8) - 1]; // max possible size of dbMask
    NvU8    DB[maxemLen - (NvDdkSeShaResultSize_Sha1/8) - 1]; // max possible size of DB
                                                        // (maximal emLen - smallest hash)
    NvU32   i;

    //Assert checks for valid data
    NV_ASSERT(RsaKeySizeBits <= ARSE_RSA_MAX_MODULUS_SIZE);
    NV_ASSERT(pInputMessage != NULL);
    NV_ASSERT(pSignature != NULL);
    NV_ASSERT( (sLen == 0) ||
               (sLen == NvDdkSeShaResultSize_Sha1 / 8) ||
               (sLen == NvDdkSeShaResultSize_Sha224 / 8) ||
               (sLen == NvDdkSeShaResultSize_Sha256 / 8) ||
               (sLen == NvDdkSeShaResultSize_Sha384 / 8) ||
               (sLen == NvDdkSeShaResultSize_Sha512 / 8) );

    //SHAHash Length assgined according to algorithm
    switch(HashAlgorithm)
    {
        case NvDdkSeShaOperatingMode_Sha1:
            hLen =  NvDdkSeShaResultSize_Sha1 / 8;
            break;
        case NvDdkSeShaOperatingMode_Sha224:
            hLen = NvDdkSeShaResultSize_Sha224 / 8;
            break;
        case NvDdkSeShaOperatingMode_Sha256:
            hLen = NvDdkSeShaResultSize_Sha256 / 8;
            break;
        case NvDdkSeShaOperatingMode_Sha384:
            hLen = NvDdkSeShaResultSize_Sha384 / 8;
            break;
        case NvDdkSeShaOperatingMode_Sha512:
            hLen = NvDdkSeShaResultSize_Sha512 / 8;
            break;
        default:
            NV_ASSERT(0);
    }

    pMessageRepresentative = &MessageRepresentative[0];

    /**
     *
     *  1. Length checking: If the length of the signature S is not k octets,
     *  where k is the length in octets of the RSA modulus n, output "invalid
     *  signature".
     *
     *  The length of the signature S is assumed to be of the correct length.
     *  The onus is on the calling function.
     */

    /**
     *  2. a) Convert the signature S to an integer signature representative:
     *        s = OS2IP (S).
     *
     *  This is not necessary since the integer signature representative
     *  is already in an octet byte stream.
     */

    /**
     *  2. b) Apply the RSAVP1 verification primitive to the RSA public key (n, e)
     *        and the signature representative s to produce an integer message
     *        representative m:
     *              m = RSAVP1 ((n, e), s)
     *              m = s^e mod n
     *
     *        If RSAVO1 output "signature representative out of range", output
     *        "invalid signature" and stop.
     *
     */

    // Calculate m = s^e mod n

    e = NvDdkRsaModularExponentiation(
        RsaKeySlot,
        (NvU32 *)pSignature,
        (NvU8 *)PublicKeyModulus,
        RsaKeySizeBits,
        (NvU32 *)pMessageRepresentative);
    if (e != NvSuccess)
        goto fail;

    // After the RSAVP1 step, the message representative m is stored in
    // in ascending order in a byte array, i.e. the 0xbc trailer field is the
    // first value of the array, when it is the "last" vlaue in the spec.
    // Reversing the byte order in the array will match the endianness
    // of the PKCS #1 spec and make for code that directly matches the spec.

    SeReverseList((NvU8 *) pMessageRepresentative, emLen);

    /**
     * 2. c) Convert the message representative m to an encoded message EM of
     *       length emLen = Ceiling( (modBits - 1) / 8) octets, where modBits
     *       is the length in bits of the RSA modulus n:
     *          EM = I2OSP(m, emLen)
     *
     */

    EM = (NvU8 *) pMessageRepresentative;

    /**
     * 3. EMSA-PSS verification: Apply the EMSA-PSS verification operation
     *    to the message M and the encoded message EM to determine whether
     *    they are consistent:
     *      Result = EMSA-PSS-VERIFY(M, EM, modBits -1)
     *
     *    if(Result == "consistent")
     *          output "Valid Signature"
     *    else
     *          output "Invlaid Signature"
     */


    // Step 1. If the length of M is greater than the input limitation of for
    // the hash function, output "inconsistent" and stop.

    // Step 2. Let mHash = Hash(M), an octet string of length hLen.
    // Allow calling functions to specify the hash of the message as a parameter.

    if(pMessageHash == NULL)
    {
        e = NvDdkSeSHAHash(
                pInputMessage,
                InputMessageLengthBytes,
                hLen,
                &mHash[0],
                HashAlgorithm);
    }
    else
    {
        NvOsMemcpy(&mHash[0], pMessageHash, hLen);
    }

    // Step 3. If emLen < hLen + sLen + 2, output "inconsistent" and stop.
    // emLen < hLen + sLen + 2
    // Ceiling(emBits/8) < hLen + sLen + 2
    // Ceiling(modBits-1/8) < hLen + sLen + 2
    // 256 octets < 32 octets + 32 octets + 2 (assuming SHA256 as the hash function)
    // Salt length is equal to hash length

    if((emLen*8) < hLen + ARSE_RSA_PSS_SALT_LENGTH_BITS + 2)
    {
        return NvError_RsaPssVerify_Inconsistent;
    }

    // Step 4. If the rightmost octet of EM does not have hexadecimal
    // value 0xbc, output "inconsistent" and stop.

    if(EM[emLen - 1] != 0xbc)
    //if(EM[0] != 0xbc)
    {
        return NvError_RsaPssVerify_Inconsistent;
    }

    // Step 5. Let maskedDB be the leftmost emLen - hLen - 1 octets
    // of EM, and let H be the next hLen octets.

    maskedDBLen = emLen - hLen - 1;
    maskedDB = EM;
    H = EM + maskedDBLen;

    // Step 6. If the leftmost 8emLen - emBits bits of the leftmost
    // octet in maskedDB are not all equal to zero, output "inconsistent"
    // and stop.
    // 8emLen - emBits = 8*256 - 2047 = 1 (assuming 2048 key size and sha256)

    LowestBits = (8*emLen) - (RsaKeySizeBits - 1);
    FirstOctetMaskedDB = maskedDB[0] & (0xFF << (8 - LowestBits));
    if(FirstOctetMaskedDB > 0)
    {
        return NvError_RsaPssVerify_Inconsistent;
    }

    // Step 7. Let dbMask = MGF(H, emLen - hLen - 1).

    e = NvDdkSeMaskGenerationFunction(
            H,
            emLen - hLen - 1,
            &dbMask[0],
            HashAlgorithm,
            hLen);
    if (e != NvSuccess)
    {
        return NvError_RsaPssVerify_Inconsistent;
    }

    // Step 8. Let DB = maskedDB XOR dbMask

    for(i = 0; i < maskedDBLen; i++)
    {
        DB[i] = maskedDB[i] ^ dbMask[i];
    }

    // Step 9. Set the leftmost 8emLen - emBits bits of the leftmost
    // octet in DB to zero.

    DB[0] &= ~(0xFF << (8 - LowestBits));

    // Step 10. If the emLen - hLen - sLen - 2 leftmost octets of DB are not
    // zero or if the octet at position emLen - hLen - sLen - 1 (the leftmost
    // or lower position is "position 1") does not have hexadecimal value

    for(i = 0; DB[i] == 0 && i < (emLen - hLen - sLen - 2); i++)
    {
        if(DB[i] != 0)
        {
            return NvError_RsaPssVerify_Inconsistent;
        }
    }

    // if octet at position emLen - hLen - sLen - 1
    // e.g. 256 - 32 - 32 - 1 = 191th position
    // position 191 is 190th element of the array, so subtract by 1 more.

    if(DB[emLen - hLen - sLen - 1 - 1] != 0x1)
    {
        return NvError_RsaPssVerify_Inconsistent;
    }

    // Step 11. Let salt be the last sLen octets of DB.

    // Step 12. Let M' = 0x 00 00 00 00 00 00 00 00 || mHash || salt;
    // Set eight initial octets to 0.

    NvOsMemset(&M_Prime[0], 0, 8);
    NvOsMemcpy(&M_Prime[8], &mHash[0], hLen);

    // Copy salt to M_Prime. Note: DB is an octet string of length
    // emLen - hLen - 1. Subtract sLen from DB length to get salt location.

    NvOsMemcpy(&M_Prime[8+hLen], &DB[(emLen - hLen - 1) - sLen], hLen);

    // Step 13. Let H' = Hash(M')

    NvDdkSeSHAHash(
        (NvU32 *) M_Prime,
        8 + hLen + sLen,
        hLen,
        &H_Prime[0],
        HashAlgorithm);

    // Step 14. If H = H' output "success". Otherwise, output "inconsistent".

    if(!NvOsMemcmp(H, (NvU8 *) &H_Prime[0], hLen))
    {
        e = NvSuccess;
    }
    else
    {
        e = NvError_RsaPssVerify_Inconsistent;
    }
fail:

return e;
}
