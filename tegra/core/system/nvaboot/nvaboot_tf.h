/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVABOOT_TF_H
#define NVABOOT_TF_H

#include "nvcommon.h"
#include "nverror.h"
#include "nvaboot.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define TF_BOOT_PARAMS_MAX_STRING   1024

/* Bootargs of Trusted Foundations */
typedef struct {
    NvU32 nBootParamsHeader;
    NvU32 nDeviceVersion;
    NvU32 nNormalOSArg;

    NvU32 nWorkspaceAddress;
    NvU32 nWorkspaceSize;
    NvU32 nWorkspaceAttributes ;
    NvU32 pReserved[16];

    NvU32 nParamStringSize;
    NvU8  pParamString[TF_BOOT_PARAMS_MAX_STRING];
} TF_BOOT_PARAMS;

/* Size of buffer used to store keys (TF_SSK, TF_SBK, Seed) */
#define TF_SEED_SIZE         0x40
#define TF_KEYS_BUFFER_SIZE  (0x40 + TF_SEED_SIZE)

/*
 * Bootloader internal struct
 * Keep TFSW_PARAMS_OFFSET_* in sync (used in .asm files)
 */
typedef struct {
    NvU64         TFStartAddr;
    NvUPtr         pTFAddr;
    NvU32          TFSize;
    NvUPtr         pEncryptedKeys;
    NvU32          nNumOfEncryptedKeys;
    NvU64          encryptedKeysSize;
    NvU32          pClkAddr;
    NvU32          pClkSize;
    NvU32          pTFKeysAddr;
    NvU32          pTFKeysSize;
    NvU8           pTFKeys[TF_KEYS_BUFFER_SIZE];
    TF_BOOT_PARAMS sTFBootParams;
    NvU64          RamBase;
    NvUPtr         ColdBootAddr;
    NvU64          RamSize;
    NvU32          L2TagLatencyLP;
    NvU32          L2DataLatencyLP;
    NvU32          L2TagLatencyG;
    NvU32          L2DataLatencyG;
    NvU32          ConsoleUartId;
} TFSW_PARAMS;

/*
 * Initialize TF_BOOT_PARAMS with data computed by the bootloader
 * and stored in TFSW_PARAM.
 */
void NvAbootTFPrepareBootParams(NvAbootHandle hAboot,
                                TFSW_PARAMS *pTfswParams,
                                NvU32 *pKernelRegisters);
#ifdef __cplusplus
}
#endif
#endif  // !NVABOOT_TF_H
