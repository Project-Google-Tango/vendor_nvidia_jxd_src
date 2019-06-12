/*
 * Copyright (c) 2006-2013, NVIDIA CORPORATION.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file nvboothost.h
 *
 * Helper routines for host-side tools that interact with the Boot ROM.
 *
 * Support is provided for --
 * * creation of Recovery Mode (RCM) messages
 * * encrypting/signing data for Boot ROM
 *
 *
 * Usage model for creating RCM messages --
 * 1. establish the size of the payload to be sent
 * 2. call NvBootHostRcmGetMsgBufferLength() to determine the buffer size
 *    needed to hold the message
 * 3. allocate a buffer to hold the message
 * 4. call NvBootHostRcmInitMsg() to initialize the message
 * 5. call NvBootHostRcmGetMsgPayloadBuffer() to get a pointer to the payload
 *    buffer within the message
 * 6. copy the payload data into the payload buffer within the message
 * 7. call NvBootHostRcmEncryptSignMsg() to sign and (optionally) encrypt
 *    the message
 * 8. call NvBootHostRcmGetMsgLength() to get the length of the message 
 *    (presumably needed by communications routines used to send the message)
 * 
 * High-level routines are provided to carry out steps 1 through 7, given
 * the payload data via a buffer, file name, or file handle --
 * * NvBootHostRcmCreateMsgFromBuffer()
 * * NvBootHostRcmCreateMsgFromFile()
 * * NvBootHostRcmCreateMsgFromFileHandle()
 *
 * Usage model for encrypting/signing data for Boot ROM --
 * 1. call NvBootHostCryptoEncryptSignBuffer to encrypt and/or sign buffer
 *
 *
 */

#ifndef INCLUDED_NVBOOTHOST_H
#define INCLUDED_NVBOOTHOST_H

#include "nvos.h"
#include "nverror.h"
#include "t30/nvboot_hash.h"
#include "nvflash_rcm.h"
#include "nvddk_fuse.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Compute size of buffer needed to hold a Recovery Mode message with a 
 * payload of the given size.
 *
 * @param PayloadLength length of the payload, in bytes
 *
 * @return required length of buffer to hold the message, in bytes
 */

NvU32
NvBootHostRcmGetMsgBufferLength(NvU32 PayloadLength);
    
/**
 * Initialize Recovery Mode message.
 *
 * @param MsgBuffer buffer where message is stored
 * @param MsgBufferLength length of MsgBuffer, in bytes.  Must be
 *        at least as big as required by NvBootHostRcmGetMsgBufferLength()
 * @param OpCode operation code to be specified in the message header
 * @param RandomAesBlock random data to be included in the message header.
 *        If NULL, zeroes will be used.
 * @param Args Arguments to be passed in the message header, specified as
 *        an array of bytes
 * @param ArgsLength Length of Args, in bytes.  Must not exceed the size
 *        of the arguments field in the message header.
 * @param PayloadLength length of the payload, in bytes
 *
 * @return void
 */

void
NvBootHostRcmInitMsg(
    NvU8 *MsgBuffer,
    NvU32 MsgBufferLength,
    NvU32 OpCode,
    NvBootHash *RandomAesBlock,
    void *Args,
    NvU32 ArgsLength,
    NvU32 PayloadLength);

/**
 * Get pointer to Payload buffer within the Recovery Mode message.  The
 * payload data should be copied into this buffer.
 *
 * @param MsgBuffer buffer where message is stored
 *
 * @return pointer to Payload buffer
 */

NvU8 *
NvBootHostRcmGetMsgPayloadBuffer(
    NvU8 *MsgBuffer);

/**
 * Sign and (optionally) encrypt the Recovery Mode message.
 *
 * @param MsgBuffer buffer where message is stored
 * @param OpMode Operating mode of the chip that will be sent the message
 * @param Sbk Secure Boot Key for the chip that will be sent the message.
 *        This parameter will be ignored if the OpMode is 
 *        NvDdkFuseOperatingMode_NvProduction.
 *
 * @return NvError_Success on success; nonzero value on error
 */

NvError
NvBootHostRcmSignMsg(NvU8 *MsgBuffer,
                    NvDdkFuseOperatingMode OpMode,
                    NvU8 *Sbk);

/**
 * Determine length of Recovery Mode message in bytes
 *
 * Note: the routine should only be called after NvBootHostRcmInitMsg()
 *
 * @param MsgBuffer buffer where message is stored
 *
 * @return Message length in bytes
 */

NvU32
NvBootHostRcmGetMsgLength(NvU8 *MsgBuffer);

/**
 * Create a Recovery Mode message, where the payload is provided in a buffer.
 *
 * Note that this routine allocates memory from the heap.  If the routine
 * completes successfully, the caller must call NvOsFree(*pMsgBuffer) to
 * free the allocated memory.
 *
 * @param OpCode operation code to be specified in the message header
 * @param RandomAesBlock random data to be included in the message header.
 *        If NULL, zeroes will be used.
 * @param Args Arguments to be passed in the message header, specified as
 *        an array of bytes
 * @param ArgsLength Length of Args, in bytes.  Must not exceed the size
 *        of the arguments field in the message header.
 * @param PayloadLength length of the payload, in bytes
 * @param PayloadBuffer Data buffer containing payload
 * @param OpMode Operating mode of the chip that will be sent the message
 * @param Key Crypto key used to encrypt/sign the message.  This parameter
 *        is ignored if the OpMode is NvDdkFuseOperatingMode_NvProduction.
 * @param pMsgBuffer Address of pointer to buffer where the resulting 
 *        message is stored.  The buffer is allocated from the heap.  The
 *        buffer is freed internally if any error occurs; otherwise, the
 *        caller is responsible for freeing the buffer.
 *
 * @return NvSuccess on success
 * @return NvError_InsufficientMemory if memory allocation fails
 * @return NvError_BadParameter invalid argument
 *
 */

NvError 
NvBootHostRcmCreateMsgFromBuffer(
    NvU32 OpCode, 
    NvBootHash *RandomAesBlock,
    NvU8 *Args, 
    NvU32 ArgsLength, 
    NvU32 PayloadLength,
    NvU8 *PayloadBuffer,
    NvDdkFuseOperatingMode OpMode,
    NvU8 *Key, 
    NvU8 **pMsgBuffer);

/**
 * Create a Recovery Mode message, where the payload is provided via an
 * open file handle.  The payload length must also be specified.
 *
 * Note that this routine allocates memory from the heap.  If the routine
 * completes successfully, the caller must call NvOsFree(*pMsgBuffer) to
 * free the allocated memory.
 *
 * @param OpCode operation code to be specified in the message header
 * @param RandomAesBlock random data to be included in the message header.
 *        If NULL, zeroes will be used.
 * @param Args Arguments to be passed in the message header, specified as
 *        an array of bytes
 * @param ArgsLength Length of Args, in bytes.  Must not exceed the size
 *        of the arguments field in the message header.
 * @param PayloadLength length of the payload, in bytes
 * @param PayloadHandle file handle where payload data will be read.
 *        PayloadLength bytes of data will be read from the file handle.
 * @param OpMode Operating mode of the chip that will be sent the message
 * @param Key Crypto key used to encrypt/sign the message.  This parameter
 *        is ignored if the OpMode is NvDdkFuseOperatingMode_NvProduction.
 * @param pMsgBuffer Address of pointer to buffer where the resulting 
 *        message is stored.  The buffer is allocated from the heap.  The
 *        buffer is freed internally if any error occurs; otherwise, the
 *        caller is responsible for freeing the buffer.
 *
 * @return NvSuccess on success
 * @return NvError_InsufficientMemory if memory allocation fails
 * @return NvError_BadParameter invalid argument
 * @return other errors as reported by NvOsFread()
 *
 */

NvError 
NvBootHostRcmCreateMsgFromFileHandle(
    NvU32 OpCode, 
    NvBootHash *RandomAesBlock,
    NvU8 *Args, 
    NvU32 ArgsLength, 
    NvU32 PayloadLength, 
    NvOsFileHandle PayloadHandle, 
    NvDdkFuseOperatingMode OpMode,
    NvU8 *Key, 
    NvU8 **pMsgBuffer);

/**
 * Create a Recovery Mode message, where the payload is provided as a
 * file.  The entire contents of the file will be used as the payload.
 *
 * Note that this routine allocates memory from the heap.  If the routine
 * completes successfully, the caller must call NvOsFree(*pMsgBuffer) to
 * free the allocated memory.
 *
 * @param OpCode operation code to be specified in the message header
 * @param RandomAesBlock random data to be included in the message header.
 *        If NULL, zeroes will be used.
 * @param Args Arguments to be passed in the message header, specified as
 *        an array of bytes
 * @param ArgsLength Length of Args, in bytes.  Must not exceed the size
 *        of the arguments field in the message header.
 * @param PayloadFile File name containing payload data.  The entire
 *        contents of the file will be read and used as the payload.
 * @param OpMode Operating mode of the chip that will be sent the message
 * @param Key Crypto key used to encrypt/sign the message.  This parameter
 *        is ignored if the OpMode is NvDdkFuseOperatingMode_NvProduction.
 * @param pMsgBuffer Address of pointer to buffer where the resulting 
 *        message is stored.  The buffer is allocated from the heap.  The
 *        buffer is freed internally if any error occurs; otherwise, the
 *        caller is responsible for freeing the buffer.
 *
 * @return NvSuccess on success
 * @return NvError_InsufficientMemory if memory allocation fails
 * @return NvError_BadParameter invalid argument
 * @return NvError_FileOperationFailed NvOsFopen() failed
 * @return other errors as reported by NvOsStat() and NvOsFread()
 *
 */

NvError 
NvBootHostRcmCreateMsgFromFile(
    NvU32 OpCode, 
    NvBootHash *RandomAesBlock,
    NvU8 *Args, 
    NvU32 ArgsLength, 
    char *PayloadFile, 
    NvDdkFuseOperatingMode OpMode,
    NvU8 *Key, 
    NvU8 **pMsgBuffer);

/**
 * Encrypt and/or Sign buffer using supplied key.  Buffer contents are modified.
 *
 * Encryption algorithm is AES-128.
 *
 * Signing/Hashing algorithm is AES-CMAC-128 (see RFC 4493).
 *
 * @param DoEncrypt flag specifying whether to encrypt buffer
 * @param DoSign flag specifying whether to sign buffer
 * @param Key 16-byte key used for encrypting/signing
 * @param Src pointer to data buffer to be encrypted/signed; on exit, 
 *        buffer will contain encrypted/signed data
 * @param NumAesBlocks amount of data to be signed/encrypted, in units
 *        of AES blocks (i.e., 16 bytes)
 * @param SigDst pointer to buffer to hold the computed signature; buffer
 *        must be able to hold at least 16 bytes
 * @param EnableDebug flag specifying whether debug output should be displayed
 *
 * @return none
 */

void
NvBootHostCryptoEncryptSignBuffer(NvBool DoEncrypt,
                                 NvBool DoSign,
                                 NvU8 *Key,
                                 NvU8 *Src,
                                 NvU32 NumAesBlocks,
                                 NvU8 *SigDst,
                                 NvBool EnableDebug);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVBOOTHOST_H
