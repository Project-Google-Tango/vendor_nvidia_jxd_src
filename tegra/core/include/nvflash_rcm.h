/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVFLASH_RCM_H
#define INCLUDED_NVFLASH_RCM_H

#include "nvcommon.h"


#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * NvFlashRcmResponse - These values are used either as return codes
 * directly or as indices into a table of return values.
 */
typedef enum
{
    /// Specifies successful execution of a command.
    NvFlashRcmResponse_Success = 0,

    /// Specifies incorrect data padding.
    NvFlashRcmResponse_BadDataPadding,

    /// Specifies incorrect message padding.
    NvFlashRcmResponse_BadMsgPadding,

    /// Specifies incorrect RCM protocol version number was provided.
    NvFlashRcmResponse_RcmVersionMismatch,

    /// Specifies that the hash check failed.
    NvFlashRcmResponse_HashCheckFailed,

    /// Specifies an attempt to perform fuse operations in an illegal
    /// operating mode.
    NvFlashRcmResponse_IllegalModeForFuseOps,

    /// Specifies an invalid entry point was provided.
    NvFlashRcmResponse_InvalidEntryPoint,

    /// Specifies invalid fuse data was provided.
    NvFlashRcmResponse_InvalidFuseData,

    /// Specifies that the insecure length was invalid.
    NvFlashRcmResponse_InvalidInsecureLength,

    /// Specifies that the message contained an invalid opcode
    NvFlashRcmResponse_InvalidOpcode,

    /// Specifies that the secure & insecure lengths did not match.
    NvFlashRcmResponse_LengthMismatch,

    /// Specifies that the payload was too large.
    NvFlashRcmResponse_PayloadTooLarge,

    /// Specifies that there was an error with USB.
    NvFlashRcmResponse_UsbError,

    /// Specifies that USB setup failed.
    NvFlashRcmResponse_UsbSetupFailed,

    /// Specifies that the fuse verification operation failed
    NvFlashRcmResponse_VerifyFailed,

    /// Specifies a transfer overflow.
    NvFlashRcmResponse_XferOverflow,

    ///
    /// New in Version 2.0
    ///

    /// Specifies an unsupported opcode was received.  Non-fatal error.
    NvFlashRcmResponse_UnsupportedOpcode,

    /// Specifies that the command's payload was too small
    NvFlashRcmResponse_PayloadTooSmall,

    /// The following two values must appear last
    NvFlashRcmResponse_Num,
    NvFlashRcmResponse_Force32 = 0x7ffffff
} NvFlashRcmResponse;

/**
 * Defines the RCM command opcodes.
 */
typedef enum
{
    /// Specifies that no opcode was provided.
    NvFlashRcmOpcode_None = 0,

    /// Specifes a sync command.
    NvFlashRcmOpcode_Sync,

    /// Specifes a command that programs fuses.
    /// Deprecated. Use ProgramFuseArray
    NvFlashRcmOpcode_ProgramFuses,

    /// Specifes a command that verifies fuse data.
    /// Deprecated. Use VerifyFuseArray.
    NvFlashRcmOpcode_VerifyFuses,

    /// Specifes a command that downloads & executes an applet.
    NvFlashRcmOpcode_DownloadExecute,

    /// Specifes a command that queries the BR code version number.
    NvFlashRcmOpcode_QueryBootRomVersion,

    /// Specifes a command that queries the RCM protocol version number.
    NvFlashRcmOpcode_QueryRcmVersion,

    /// Specifes a command that queries the BR data structure version number.
    NvFlashRcmOpcode_QueryBootDataVersion,

    ///
    /// New w/v2.0
    ///

    /// Specifes a command that programs fuses.
    NvFlashRcmOpcode_ProgramFuseArray,

    /// Specifes a command that verifies fuse data.
    NvFlashRcmOpcode_VerifyFuseArray,

    /// Specifies a command that enables JTAG at BR exit.
    NvFlashRcmOpcode_EnableJtag,

    NvFlashRcmOpcode_Force32 = 0x7fffffff,
} NvFlashRcmOpcode;

/**
 * Defines the data needed by the DownloadExecute command.
 */
typedef struct NvFlashRcmDownloadDataRec
{
    /// Specifies the entry point address in the downloaded applet.
    NvU32 EntryPoint;

} NvFlashRcmDownloadData;


#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_RCM_H */
