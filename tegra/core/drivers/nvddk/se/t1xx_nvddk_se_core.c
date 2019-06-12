/*
 * Copyright (c) 2012-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvos.h"
#include "nvassert.h"
#include "nvrm_power.h"
#include "nvrm_hardware_access.h"
#include "nvrm_module.h"
#include "nvrm_interrupt.h"
#include "nvddk_blockdev.h"
#include "nvddk_se_blockdev.h"
#include "nvddk_se_hw.h"
#include "nvddk_se_private.h"

#define SE_REGR(hSE, reg) \
     NV_READ32(hSE->pVirtualAddress + ((SE_##reg##_0) / 4))

/* Buffers fields */
#define BUFFER_A (0x1)
#define BUFFER_B (0x2)

/* RSA Key Slots */
#define SE_RSA_KEY_SLOT_0 (0)
#define SE_RSA_KEY_SLOT_1 (1)
#define SE_RSA_DEFAULT_KEY_SLOT SE_RSA_KEY_SLOT_0
#define RNG_HW_RESEED_CNTR_VAL 1

extern NvDdkSeCoreCapabilities *gs_pSeCoreCaps;
extern NvDdkSeHWCtx *s_pSeHWCtx;
extern NvOsMutexHandle s_SeCoreEngineMutex;
extern NvOsSemaphoreHandle s_DualBufSem;

/* The following input needs to be given once the
 * resed exhaust interrupt occurs
 */
static NvU8 ReSeedExhaustInput[48] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
    13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 26, 27, 28, 29,30, 31,32,
    33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48};

NvError
NvDdkSeRSAPerformModExpo(NvDdkSeRsaHWCtx *hSeRsaHwCtx,
                         const NvSeRsaKeyDataInfo* pKeyDataInfo,
                         NvSeRsaOutDataInfo* pOutDataInfo)
{
    NvError e = NvSuccess;
    NvBool IsMutexLocked = NV_FALSE;

    if (!s_pSeHWCtx || !hSeRsaHwCtx || !pKeyDataInfo || !pOutDataInfo)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    if (!gs_pSeCoreCaps->IsPkcSupported)
    {
        e = NvError_NotSupported;
        goto fail;
    }
    if (!((IN_BITS(pKeyDataInfo->ModulusSizeInBytes) >= 512)  &&
          (IN_BITS(pKeyDataInfo->ModulusSizeInBytes) <= 2048) &&
          (IN_BITS(pKeyDataInfo->ModulusSizeInBytes) % 64 == 0)))
    {
        e = NvError_InvalidSize;
        goto fail;
    }

    /* Lock the Engine */
    NvOsMutexLock(s_SeCoreEngineMutex);
    IsMutexLocked = NV_TRUE;

    /**         LOADING THE kEY TO KEY SLOT
     */
    /* Writing Exponent and Modulus sizes to Rsa H/w context */
    hSeRsaHwCtx->RsaExponentLenInBits = IN_BITS(pKeyDataInfo->ExponentSizeInBytes);
    hSeRsaHwCtx->RsaModulusLenInBits  = IN_BITS(pKeyDataInfo->ModulusSizeInBytes);

    /* Pointing modulus and exponent to respective buffers */
    hSeRsaHwCtx->RsaExponent = (void *)pKeyDataInfo->Exponent;
    hSeRsaHwCtx->RsaModulus  = (void *)pKeyDataInfo->Modulus;

    /* Writing register  Programmable informantion */
    hSeRsaHwCtx->RsaKeySlot = pKeyDataInfo->KeySlot;

    /* Set the key into the slot */
    NV_CHECK_ERROR_CLEANUP(gs_pSeCoreCaps->pHwIf->pfNvDdkSeRsaSetKey(hSeRsaHwCtx));

    /**         PERFORM MODULAR EXPONENTIATION OPERATION
     */
    /* Prepare  Input LL structure */
    s_pSeHWCtx->InputLL[1].LLVirAddr->LLBufNum  = 0;
    s_pSeHWCtx->InputLL[1].LLVirAddr->LLBufSize = pKeyDataInfo->ModulusSizeInBytes;

    NvOsMemcpy(s_pSeHWCtx->InputLL[1].LLBufVirAddr,
               pKeyDataInfo->InputBuf,
               pKeyDataInfo->ModulusSizeInBytes);

    hSeRsaHwCtx->InLLAddr = (NvU32)s_pSeHWCtx->InputLL[1].LLPhyAddr;
    hSeRsaHwCtx->RsaDestIsMemory = 1;

    if (hSeRsaHwCtx->RsaDestIsMemory)
    {
        s_pSeHWCtx->OutputLL[1].LLVirAddr->LLBufNum = 0;
        s_pSeHWCtx->OutputLL[1].LLVirAddr->LLBufSize = pKeyDataInfo->ModulusSizeInBytes;
        hSeRsaHwCtx->OutLLAddr = (NvU32)s_pSeHWCtx->OutputLL[1].LLPhyAddr;
    }

    /* Process the data */
    NV_CHECK_ERROR_CLEANUP(gs_pSeCoreCaps->pHwIf->pfNvDdkSeRsaProcessBuffer(hSeRsaHwCtx));
    /* Collect the data */
    NV_CHECK_ERROR_CLEANUP(
            gs_pSeCoreCaps->pHwIf->pfNvDdkSeRsaCollectOutputData((NvU32 *)pOutDataInfo->OutputBuf,
                                                                 pKeyDataInfo->ModulusSizeInBytes,
                                                                 hSeRsaHwCtx->RsaDestIsMemory)
                          );
fail:
    if (IsMutexLocked)
    {
        /* Unlock the Engine */
        NvOsMutexUnlock(s_SeCoreEngineMutex);
    }
    return e;
}

static void SeRngPrepareSrc(NvDdkSeRngHWCtx *hSeRngHwCtx,
        const NvDdkSeRngInitInfo *pInitInfo)
{
    NvU8 *pBuf;

    if (pInitInfo->IsSrcMem)
    {
        pBuf = s_pSeHWCtx->InputLL[BUFFER_A-1].LLBufVirAddr;
        NvOsMemcpy(pBuf, pInitInfo->RngSeed, pInitInfo->RngSeedSize);
        s_pSeHWCtx->InputLL[BUFFER_A-1].LLVirAddr->LLBufSize = pInitInfo->RngSeedSize;
        s_pSeHWCtx->InputLL[BUFFER_A-1].LLVirAddr->LLBufNum  = 0;
        hSeRngHwCtx->InLLAddr = (NvU32)s_pSeHWCtx->InputLL[BUFFER_A-1].LLPhyAddr;
    }
    hSeRngHwCtx->RngSrc  = (pInitInfo->IsSrcMem)?0 : 1;
}

static void SeRngPrepareDest(NvDdkSeRngHWCtx *hSeRngHwCtx,
        const NvDdkSeRngInitInfo *pInitInfo)
{
    if (pInitInfo->IsDestMem)
    {
        s_pSeHWCtx->OutputLL[BUFFER_A-1].LLVirAddr->LLBufNum = 0;
        hSeRngHwCtx->OutLLAddr = (NvU32)s_pSeHWCtx->OutputLL[BUFFER_A-1].LLPhyAddr;
    }
    hSeRngHwCtx->RngDest = (pInitInfo->IsDestMem)?0 : 1;
}

NvError
NvDdkSeRngSetUpContext(NvDdkSeRngHWCtx *hSeRngHwCtx,
        const NvDdkSeRngInitInfo *pInitInfo)
{

    if (!s_pSeHWCtx || !hSeRngHwCtx || !pInitInfo)
        return NvError_BadParameter;

    SeRngPrepareSrc(hSeRngHwCtx, pInitInfo);
    SeRngPrepareDest(hSeRngHwCtx, pInitInfo);

    hSeRngHwCtx->RngInitMode      = (NvU32)pInitInfo->RngMode;
    hSeRngHwCtx->RngKeyUsed       = (NvU32)pInitInfo->RngKeyUsed;
    hSeRngHwCtx->RngReseedCounter = RNG_HW_RESEED_CNTR_VAL;
    hSeRngHwCtx->IsRngContextSet  = NV_TRUE;
    return NvSuccess;
}


/**
 * Remember the input to the RNG must be always be a multiple of 16 bytes,
 * since it uses AES Internally.
 *
 * NOTE: For 192b key the reseed size comes out to be 40bytes but we need to
 * stretch it to 48bytes since the input needs to be multiple of 16bytes.
 */
static NvU32 GetReseedIpsize(NvDdkSeRngHWCtx *hSeRngHwCtx)
{
    NvU32 RessedInputDataSize;

    RessedInputDataSize = 16; ///default size(Additional input)
    switch (hSeRngHwCtx->RngKeyUsed)
    {
        case 0:
            RessedInputDataSize += 16; /// 128bitkey
            break;
        default:
            RessedInputDataSize += 32; /// 256bit key
    }
    return RessedInputDataSize;
}

/* Reseed Exhaust Interrupt handling
 * (a) Prepare Randam data of size = Reseed mode data size
 * (b) when even  interrupt stricks give the Random data
 * as an input through linkedlist
 */
static void SeRngHandleREI(NvDdkSeRngHWCtx *hSeRngHwCtx)
{
     NvU8 *pBuf;
     NvU32 RessedInputDataSize;
     RessedInputDataSize = GetReseedIpsize(hSeRngHwCtx);
     pBuf = s_pSeHWCtx->InputLL[BUFFER_B-1].LLBufVirAddr;
     s_pSeHWCtx->InputLL[BUFFER_B-1].LLVirAddr->LLBufSize = RessedInputDataSize;
     s_pSeHWCtx->InputLL[BUFFER_B-1].LLVirAddr->LLBufNum  = 0;
     NvOsMemcpy (pBuf, ReSeedExhaustInput, RessedInputDataSize);
     hSeRngHwCtx->REIInputLLAddr = (NvU32)s_pSeHWCtx->InputLL[BUFFER_B-1].LLPhyAddr;
}

NvError
NvDdkSeRngGetRandomNumber(NvDdkSeRngHWCtx *hSeRngHwCtx,
        const NvDdkSeRngProcessInInfo *pProcessInInfo,
        const NvDdkSeRngProcessOutInfo *pProcessOutInfo)
{
    NvError e = NvSuccess;

    if (!s_pSeHWCtx ||!hSeRngHwCtx ||!pProcessInInfo ||!pProcessOutInfo
            ||!pProcessOutInfo->pRandomNumber)
        return NvError_BadParameter;

    if (pProcessInInfo->OutPutSizeReq == 0)
        return NvError_InvalidSize;
    if (!hSeRngHwCtx->IsRngContextSet)
        return NvError_NotInitialized;

    /* Lock the engine */
    NvOsMutexLock(s_SeCoreEngineMutex);

    /* Get number of bytes of RNG required*/
    hSeRngHwCtx->RngOutSizeInBytes = pProcessInInfo->OutPutSizeReq;

    /*if the Destintion is memory prepare the output linked list*/
    if (hSeRngHwCtx->RngDest == 0)
    {
        s_pSeHWCtx->OutputLL[BUFFER_A-1].LLVirAddr->LLBufSize =
            hSeRngHwCtx->RngOutSizeInBytes;
    }

    /* Reseed exhaust interrupt Taken care here */
    SeRngHandleREI(hSeRngHwCtx);

    /* Generate Random number */
    NV_CHECK_ERROR_CLEANUP(
            gs_pSeCoreCaps->pHwIf->pfNvDdkSeRngGenerateRandomNumber(hSeRngHwCtx));

    if (s_pSeHWCtx->ErrFlag)
        {
            e = NvError_InvalidState;
            goto fail;
        }

    /* If the destination is memory get the randaom number from the memory */
    if (hSeRngHwCtx->RngDest == 0)
    {
        if (pProcessOutInfo->pRandomNumber)
            NvOsMemcpy(pProcessOutInfo->pRandomNumber,
                    s_pSeHWCtx->OutputLL[BUFFER_A-1].LLBufVirAddr,
                    hSeRngHwCtx->RngOutSizeInBytes);
    }
fail:
    /// Unlock the engine
    NvOsMutexUnlock(s_SeCoreEngineMutex);
    return e;
}
