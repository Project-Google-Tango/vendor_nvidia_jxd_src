/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvsecuretool.h"
#include "nv_rsa.h"
#include "t11x/nvboot_warm_boot_0.h"
#include "t11x/nvboot_rcm.h"

#define NVFLASH_MINILOADER_ENTRY 0x4000E000 //entry point for miniloader in IRAM
#define NV_AOS_ENTRY_POINT 0x80108000 // entry point for bootloader in SDRAM
#define NV_MICROBOOT_ENTRY_POINT 0x4000E000 // entry point for microboot in IRAM
#define NV_MICROBOOT_SIZE_MAX 0x00024000 // minimum IRAM size

extern NvBigIntModulus *KeyModulus;

/**
* Find the warmboot code offset from the input Buffer
* @param Buffer pointer to the bootloader Buffer
* @param BLLength contains the size of the bootloader Buffer
* @param StartWarmBootLoc pointer to  the beginning location of warmboot code to be updated
* @param WarmBootCodeLength  pointer to  the length of the warmboot code to be updated
*
* Return NvSuccess if successful otherwise appropriate error code
 */
static NvError
NvFindWarmBootOffset(
        NvU8 *Buffer,
        NvU32 BLlength,
        NvU32 *StartWarmBootLoc,
        NvU32 *WarmBootCodeLength);

/**
* Fill Warmboot Header Values
* @param WarmBootCodeLength contains the length of the warmboot code
* @param WarmBootHeadbuf warmboot header values will be updated in the Buffer
*
* Return NvSuccess if successful otherwise appropriate error code
 */
static NvError
NvUpdateWarmBootHeader(
        NvU32 WarmBootCodeLength,
        NvBootWb0RecoveryHeader *WarmBootHeadbuf);

/**
* Sign the warmboot code
* @param BlSrcBuf pointer to the bootloader Buffer
* @param Length contains the size of the bootloader Buffer
*
* Return NvSuccess if successful otherwise appropriate error code
 */
static NvError
NvSecureSignT1xxWarmBootCode(
        NvU8 *BlSrcBuf,
        NvU32 Length);

NvU32 NvSecureGetT1xxMlAddress(void);
NvU32 NvSecureGetT1xxBctSize(void);
NvU32 NvSecureGetT1xxBlLoadAddr(void);
NvU32 NvSecureGetT1xxBlEntryPoint(void);
NvU32 NvSecureGetT1xxMbLoadAddr(void);
NvU32 NvSecureGetT1xxMbEntryPoint(void);
NvU32 NvSecureGetT1xxMaxMbSize(void);

void NvSecureGetBctT1xxInterface(NvSecureBctInterface *pBctInterface)
{
    pBctInterface->NvSecureGetMlAddress = NvSecureGetT1xxMlAddress;
    pBctInterface->NvSecureGetBctSize = NvSecureGetT1xxBctSize;
    pBctInterface->NvSecureGetBlLoadAddr = NvSecureGetT1xxBlLoadAddr;
    pBctInterface->NvSecureGetBlEntryPoint = NvSecureGetT1xxBlEntryPoint;
    pBctInterface->NvSecureGetMbLoadAddr= NvSecureGetT1xxMbLoadAddr;
    pBctInterface->NvSecureGetMbEntryPoint = NvSecureGetT1xxMbEntryPoint;
    pBctInterface->NvSecureGetMaxMbSize = NvSecureGetT1xxMaxMbSize;
    pBctInterface->NvSecureSignWarmBootCode = NvSecureSignT1xxWarmBootCode;
    pBctInterface->NvSecureSignWB0Image = NULL;
}

NvU32 NvSecureGetT1xxMlAddress(void)
{
    return NVFLASH_MINILOADER_ENTRY;
}

NvU32 NvSecureGetT1xxBctSize(void)
{
    return NvBctGetBctSize();
}

NvU32 NvSecureGetT1xxBlLoadAddr(void)
{
    return NV_AOS_ENTRY_POINT;
}

NvU32 NvSecureGetT1xxBlEntryPoint(void)
{
    return NV_AOS_ENTRY_POINT;
}

NvU32 NvSecureGetT1xxMbLoadAddr(void)
{
    return NV_MICROBOOT_ENTRY_POINT;
}

NvU32 NvSecureGetT1xxMbEntryPoint(void)
{
    return NV_MICROBOOT_ENTRY_POINT;
}

NvU32 NvSecureGetT1xxMaxMbSize(void)
{
    return NV_MICROBOOT_SIZE_MAX;
}

NvError
NvSecureSignT1xxWarmBootCode(
        NvU8* BlSrcBuf,
        NvU32 BlLength)
{
    NvError e       = NvSuccess;
    NvU8 *Signature = NULL;
    NvU32 StartWarmBootLoc;
    NvU32 offset;
    NvU32 WarmBootCodeLength;
    NvBootWb0RecoveryHeader WarmBootHeadbuf;

    /* Parse bl code to find out the warmboot code offset */
    if (NvFindWarmBootOffset(BlSrcBuf, BlLength, &StartWarmBootLoc,
                             &WarmBootCodeLength) != NvSuccess)
    {
       NvAuPrintf("Warmboot signing failed\n");
       return NvError_NotImplemented;
    }

    Signature = NvOsAlloc(RSA_KEY_SIZE);
    VERIFY(Signature, goto fail);
    NvOsMemset(Signature, 0x0, RSA_KEY_SIZE);

    NvOsMemset(&WarmBootHeadbuf, 0x0, sizeof(NvBootWb0RecoveryHeader));
    /* Fill the Warmboot header */
    NvUpdateWarmBootHeader(WarmBootCodeLength, &WarmBootHeadbuf);
    /* Copy the Filled header into BL Buffer */
    NvOsMemcpy(&BlSrcBuf[StartWarmBootLoc], &WarmBootHeadbuf,
               sizeof(NvBootWb0RecoveryHeader));

    offset = NV_OFFSETOF(NvBootWb0RecoveryHeader, RandomAesBlock);
    /* Sign the Warmboot Code starting from RandomAesBlock */
    e = NvSecurePkcSign(&BlSrcBuf[StartWarmBootLoc + offset], Signature,
                                              WarmBootCodeLength - offset);
    VERIFY(e == NvSuccess, goto fail);

    /* Copy the RSA PSS Signature into warm boot header */
    NvOsMemcpy(WarmBootHeadbuf.Signature.RsaPssSig.Signature, Signature, 256);
    /* Copy the updated header into BL Buffer */
    NvOsMemcpy(&BlSrcBuf[StartWarmBootLoc], &WarmBootHeadbuf,
                                          sizeof(NvBootWb0RecoveryHeader));

fail:
    if (Signature)
       NvOsFree(Signature);
    return e;
}


NvError NvFindWarmBootOffset(
        NvU8 *Buffer,
        NvU32 BLlength,
        NvU32 *StartWarmBootLoc,
        NvU32 *WarmBootCodeLength)
{
    NvError e = NvSuccess;
    NvU8 flag = 0;
    NvU32 i;

    /* Search for the pattern 0XDEADBEEF  0xDEADBEEF which is stored
     * in four bytes aligned boundary
     */
    for (i = 0; i + 4 < BLlength; i = i + 4)
    {
        if ((Buffer[i] == 0xEF) && (Buffer[i + 1] == 0xBE) &&
            (Buffer[i + 2] == 0xAD) && (Buffer[i + 3] == 0xDE))
        {
            if ((Buffer[i + 4] == 0xEF) && (Buffer[i + 5] == 0xBE) &&
                (Buffer[i + 6] == 0xAD) && (Buffer[i + 7] == 0xDE))
            {
                if (flag == 1)
                {
                    *WarmBootCodeLength = i - *StartWarmBootLoc;
                    flag = 2;
                    break;
                }

                if (flag == 0)
                {
                    *StartWarmBootLoc = i + 8;
                    i = i + 4;
                    flag = 1;
                }
            }
        }
    }

    if (flag != 2)
    {
        e = NvError_NotImplemented;
        goto fail;
    }

    NvAuPrintf("Warm boot code start %d, Length %d\n",
               *StartWarmBootLoc, *WarmBootCodeLength);
fail:
    return e;
}

NvError
NvUpdateWarmBootHeader(
        NvU32 WarmBootCodeLength,
        NvBootWb0RecoveryHeader *WarmBootHeadbuf)
{
    NvError e = NvSuccess;
    NvU32 temp;

    WarmBootHeadbuf->LengthInsecure = WarmBootCodeLength;
    WarmBootHeadbuf->LengthSecure   = WarmBootCodeLength;

    WarmBootHeadbuf->Destination = LP0_EXIT_RUN_ADDRESS;
    WarmBootHeadbuf->EntryPoint  = LP0_EXIT_RUN_ADDRESS;

    temp = WarmBootCodeLength - sizeof(NvBootWb0RecoveryHeader);
    WarmBootHeadbuf->RecoveryCodeLength = temp;

    /* Copy the RSA public key into warm boot header */
    NvOsMemcpy(WarmBootHeadbuf->PublicKey.Modulus, (char*)KeyModulus->n, 256);

    return e;
}

void
NvSecurePkcGetPubKeyT1xx(NvU8 *Dst)
{
   NvOsMemcpy(Dst, KeyModulus->n, sizeof(NvBootRsaKeyModulus));
}

NvError
NvBootSecurePKCRcmSignMsgT1xx(NvU8 *MsgBuffer)
{
    NvError e         = NvSuccess;
    NvBootRcmMsg *Msg = NULL;
    NvU32 CryptoLength;

    NV_ASSERT(MsgBuffer != NULL);

    Msg = (NvBootRcmMsg*) MsgBuffer;

    /* encryption and signing does not include the LengthInsecure and
     * Hash fields at the beginning of the message.
     */
    CryptoLength = Msg->LengthInsecure - sizeof(NvU32) -
                   sizeof(NvBootRsaKeyModulus) - sizeof(NvBootObjectSignature);

    if (CryptoLength % (1 << NVBOOT_AES_BLOCK_LENGTH_LOG2))
    {
        e = NvError_BadParameter;
        goto fail;
    }

    e = NvSecurePkcSign((NvU8*)Msg->RandomAesBlock.hash,
                        (NvU8*)Msg->Signature.RsaPssSig.Signature,
                        CryptoLength);

    VERIFY(e == NvSuccess, goto fail);
    NvSecurePkcGetPubKey((NvU8*)Msg->Key.Modulus);
    goto done;

fail:
    NvAuPrintf("RCM signing failed\n");
done:
    return e;
}

