/**
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvos.h"
#include "nvassert.h"
#include "nvboothost_t12x.h"
#include "t12x/nvboot_aes.h"
#include "t12x/nvboot_rcm.h"
#include "t12x/nvboot_version.h"

/* static function prototypes */

/**
 * Compute size of payload padding require to ensure that the following
 * requirements are met --
 *
 * 1. message length >= NVBOOT_RCM_MIN_MSG_LENGTH
 * 2. length of payload + padding is a multiple of the AES block size
 *
 * @param PayloadLength length of the payload in bytes
 *
 * @return required length of payload padding, in bytes
 */

static NvU32 ComputePaddingLength(NvU32 PayloadLength);

/**
 * Write padding pattern into given buffer
 *
 * @param Data buffer where padding pattern is to be written
 * @param Length length of padding pattern to write, in bytes
 *
 * @return void
 */

static void CreateMsgPadding(NvU8 *Data, NvU32 Length);

void
NvBootHostCryptoEncryptSignBuffer(NvBool DoEncrypt,
                                 NvBool DoSign,
                                 NvU8 *Key,
                                 NvU8 *Src,
                                 NvU32 NumAesBlocks,
                                 NvU8 *SigDst,
                                 NvBool EnableDebug);

/* function definitions */


static NvU32
ComputePaddingLength(
    NvU32 PayloadLength)
{
    NvU32 PaddingLength = 0;
    NvU32 MsgLength = sizeof(NvBootRcmMsg) + PayloadLength;

    /* First, use padding to bump the message size up to the minimum. */
    if (MsgLength < NVBOOT_RCM_MIN_MSG_LENGTH)
    {
        PaddingLength = NVBOOT_RCM_MIN_MSG_LENGTH - MsgLength;
        MsgLength += PaddingLength;
    }

    /*
     * Next, add any extra padding needed to bump the relevant subset
     * of the data up to a multiple of 16 bytes.  Subtracting off the
     * NvBootRcmMsg size handles the initial data that is not part of
     * the hashing and encryption.
     */
    PaddingLength += 16 - ((MsgLength - sizeof(NvBootRcmMsg)) & 0xf);

    return PaddingLength;
}

static void
CreateMsgPadding(
    NvU8 *Data,
    NvU32 Length)
{
    NvU8 PadValue = 0x80;

    while (Length > 0)
    {
        *Data++  = PadValue;
        PadValue = 0x00;
        Length--;
    }
}

NvU32
NvBootHostT12xRcmGetMsgBufferLength(
    NvU32 PayloadLength)
{
    NvU32 PaddingLength = ComputePaddingLength(PayloadLength);
    NvU32 BufferLength = sizeof(NvBootRcmMsg) + PayloadLength + PaddingLength;

    return BufferLength;
}

void
NvBootHostT12xRcmInitMsg(
    NvU8 *MsgBuffer,
    NvU32 MsgBufferLength,
    NvU32 OpCode,
    NvBootHash *RandomAesBlock,
    void *Args,
    NvU32 ArgsLength,
    NvU32 PayloadLength)
{
    NvU32 PaddingLength;
    NvBootRcmMsg *Msg;

    NV_ASSERT(MsgBuffer != NULL);
    NV_ASSERT(ArgsLength <= sizeof(Msg->Args));
    NV_ASSERT(ArgsLength == 0 || Args != NULL);

    Msg = (NvBootRcmMsg*)MsgBuffer;

    PaddingLength = ComputePaddingLength(PayloadLength);

    Msg->LengthInsecure = sizeof(NvBootRcmMsg) + PayloadLength + PaddingLength;

    NV_ASSERT(Msg->LengthInsecure <= MsgBufferLength);

    NvOsMemset(&(Msg->Signature.CryptoHash.hash), 0x0, sizeof(Msg->Signature.CryptoHash.hash));

    // Note: it would be better to generate a random value if the user doesn't
    //       supply one; however, there's currently no good, portable random
    //       number generator available.
    if (RandomAesBlock)
        NvOsMemcpy(&(Msg->RandomAesBlock), RandomAesBlock, sizeof(NvBootHash));
    else
        NvOsMemset(&(Msg->RandomAesBlock), 0x0, sizeof(NvBootHash));

    Msg->Opcode         = (NvBootRcmOpcode)OpCode;
    Msg->LengthSecure   = Msg->LengthInsecure;
    Msg->PayloadLength  = PayloadLength;
    Msg->RcmVersion     = NVBOOT_RCM_VERSION;

    if (Msg->Opcode == NvBootRcmOpcode_EnableJtag)
    {
        NvBootECID *UniqueId = (NvBootECID *)Args;
        Msg->UniqueChipId.ECID_0 = UniqueId->ECID_0;
        Msg->UniqueChipId.ECID_1 = UniqueId->ECID_1;
        Msg->UniqueChipId.ECID_2 = UniqueId->ECID_2;
        Msg->UniqueChipId.ECID_3 = UniqueId->ECID_3;
    }
    else
    {
        if (ArgsLength)
            NvOsMemcpy(&(Msg->Args), Args, ArgsLength);

        NvOsMemset((NvU8 *)&(Msg->Args) + ArgsLength, 0x0,
            sizeof(Msg->Args) - ArgsLength);
    }
    CreateMsgPadding(Msg->Padding, NVBOOT_RCM_MSG_PADDING_LENGTH);

    CreateMsgPadding(MsgBuffer+sizeof(NvBootRcmMsg)+PayloadLength, PaddingLength);
}

NvU8 *
NvBootHostT12xRcmGetMsgPayloadBuffer(
    NvU8 *Buffer)
{
    NV_ASSERT(Buffer != NULL);

    return Buffer + sizeof(NvBootRcmMsg);
}

NvError
NvBootHostT12xRcmSignMsg(
    NvU8 *MsgBuffer,
    NvDdkFuseOperatingMode OpMode,
    NvU8 *Sbk)
{
    NvBootRcmMsg *Msg;
    NvError e = NvSuccess;
    NvBool DoEncrypt, DoSign;
    NvU32 ZeroKey[NVBOOT_AES_KEY_LENGTH];
    NvU8 *pKey;
    NvU32 CryptoLength;
    NvU32 NumAesBlocks;

    NV_ASSERT(MsgBuffer != NULL);

    Msg = (NvBootRcmMsg*)MsgBuffer;

    switch (OpMode)
    {

    case NvDdkFuseOperatingMode_NvProduction:
    case NvDdkFuseOperatingMode_OdmProductionOpen:
        DoEncrypt = NV_FALSE;
        DoSign = NV_TRUE;
        NvOsMemset(ZeroKey, 0x0, sizeof(ZeroKey));
        pKey = (NvU8 *)ZeroKey;
        break;

    case NvDdkFuseOperatingMode_OdmProductionSecure:
        DoEncrypt = NV_TRUE;
        DoSign = NV_TRUE;
        pKey = Sbk;
        break;

    default:
        e = NvError_BadParameter;
        goto done;
        break;
    }

    // encryption and signing does not include the LengthInsecure and
    // Hash fields at the beginning of the message.

    CryptoLength = Msg->LengthInsecure-sizeof(NvU32)-
                                sizeof(NvBootRsaKeyModulus)-
                                sizeof(NvBootObjectSignature);

    if (CryptoLength % (1<<NVBOOT_AES_BLOCK_LENGTH_LOG2))
    {
        e = NvError_BadParameter;
        goto done;
    }

    NumAesBlocks = CryptoLength / (1<<NVBOOT_AES_BLOCK_LENGTH_LOG2);

    NvBootHostCryptoEncryptSignBuffer(DoEncrypt,
                                      DoSign,
                                      pKey,
                                      (NvU8*)Msg->RandomAesBlock.hash,
                                      NumAesBlocks,
                                      (NvU8*)Msg->Signature.CryptoHash.hash,
                                      NV_FALSE);

done:

    return e;
}

NvU32
NvBootHostT12xRcmGetMsgLength(NvU8 *MsgBuffer)
{
    NvBootRcmMsg *Msg;

    NV_ASSERT(MsgBuffer);

    Msg = (NvBootRcmMsg*)MsgBuffer;

    return Msg->LengthInsecure;
}

NvError
NvBootHostT12xRcmCreateMsgFromBuffer(
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
    NvError e = NvSuccess;

    NvU32 MsgBufferLength;
    NvU8 *MsgBuffer = NULL;
    NvU8 *MsgPayloadBuffer;

    // create message buffer

    MsgBufferLength = NvBootHostT12xRcmGetMsgBufferLength(PayloadLength);

    MsgBuffer = NvOsAlloc(MsgBufferLength);
    if (!MsgBuffer) {
        e = NvError_InsufficientMemory;
        goto done;
    }

    // initialize message

    NvBootHostT12xRcmInitMsg(MsgBuffer, MsgBufferLength, OpCode, RandomAesBlock,
                         Args, ArgsLength, PayloadLength);

    // fill message payload

    MsgPayloadBuffer = NvBootHostT12xRcmGetMsgPayloadBuffer(MsgBuffer);

    if (PayloadLength)
        NvOsMemcpy(MsgPayloadBuffer, PayloadBuffer, PayloadLength);

    // sign message

    e = NvBootHostT12xRcmSignMsg(MsgBuffer, OpMode, Sbk);
    if ( e != NvSuccess )
        goto done;

done:

    if (e != NvError_Success) {
        NvOsFree(MsgBuffer);
        MsgBuffer = NULL;
    }

    *pMsgBuffer = MsgBuffer;

    return e;
}

NvError
NvBootHostT12xRcmCreateMsgFromFileHandle(
    NvU32 OpCode,
    NvBootHash *RandomAesBlock,
    NvU8 *Args,
    NvU32 ArgsLength,
    NvU32 PayloadLength,
    NvOsFileHandle PayloadHandle,
    NvDdkFuseOperatingMode OpMode,
    NvU8 *Sbk,
    NvU8 **pMsgBuffer)
{
    NvError e = NvSuccess;

    NvU32 MsgBufferLength;
    NvU8 *MsgBuffer = NULL;
    NvU8 *PayloadBuffer;

    // create message buffer

    MsgBufferLength = NvBootHostT12xRcmGetMsgBufferLength(PayloadLength);

    MsgBuffer = NvOsAlloc(MsgBufferLength);
    if (!MsgBuffer) {
        e = NvError_InsufficientMemory;
        goto done;
    }

    // initialize message

    NvBootHostT12xRcmInitMsg(MsgBuffer, MsgBufferLength, OpCode, RandomAesBlock,
                         Args, ArgsLength, PayloadLength);

    // fill message payload

    PayloadBuffer = NvBootHostT12xRcmGetMsgPayloadBuffer(MsgBuffer);

    while (PayloadLength)
    {
        size_t BytesRead;
        e = NvOsFread(PayloadHandle, PayloadBuffer, PayloadLength, &BytesRead);
        if (e != NvError_Success)
            goto done;
        PayloadLength -= BytesRead;
        PayloadBuffer += BytesRead;
    }

    // sign message

    e = NvBootHostT12xRcmSignMsg(MsgBuffer, OpMode, Sbk);
    if ( e != NvSuccess )
        goto done;

done:

    if (e != NvError_Success) {
        NvOsFree(MsgBuffer);
        MsgBuffer = NULL;
    }

    *pMsgBuffer = MsgBuffer;

    return e;
}

NvError
NvBootHostT12xRcmCreateMsgFromFile(
    NvU32 OpCode,
    NvBootHash *RandomAesBlock,
    NvU8 *Args,
    NvU32 ArgsLength,
    char *PayloadFile,
    NvDdkFuseOperatingMode OpMode,
    NvU8 *Sbk,
    NvU8 **pMsgBuffer)
{
    NvError e = NvSuccess;

    NvOsFileHandle h = NULL;
    NvOsStatType fstat;

    NvU32 PayloadLength;

    *pMsgBuffer = (NvU8 *)NULL;

    // verify that data file exists and determine its size

    e = NvOsStat(PayloadFile, &fstat);
    if ( e != NvError_Success )
        return e;

    PayloadLength = (NvU32) fstat.size;

    // open data file

    e = NvOsFopen(PayloadFile, NVOS_OPEN_READ, &h);

    if (h == NULL)
        return NvError_FileOperationFailed;

    if (e != NvError_Success)
        return e;

    // create message and load payload from file

    e = NvBootHostT12xRcmCreateMsgFromFileHandle(OpCode, RandomAesBlock, Args,
                                             ArgsLength, PayloadLength, h,
                                             OpMode, Sbk, pMsgBuffer);
    if (e != NvError_Success)
        goto done;

done:

    if (h != NULL)
        NvOsFclose(h);

    return e;
}
