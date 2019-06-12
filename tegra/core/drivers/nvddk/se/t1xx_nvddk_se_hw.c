/*
 * Copyright (c) 2012 - 2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvos.h"
#include "nvassert.h"
#include "nvrm_hardware_access.h"
#include "nvrm_interrupt.h"
#include "nvddk_blockdev.h"
#include "nvddk_se_blockdev.h"
#include "nvddk_se_hw.h"
#include "nvddk_se_private.h"

extern NvDdkSeHWCtx *s_pSeHWCtx;
extern NvOsSemaphoreHandle s_DualBufSem;
static NvU32 REIInputAddr;

static NvError WaitForSeIdleState(void)
{
    NvU32 SeStatus, SeIntStatus;
    NvU32 CurTime, EndTime;

    EndTime = NvOsGetTimeMS() + SE_OP_MAX_TIME;
    CurTime = NvOsGetTimeMS();
    while (CurTime <= EndTime)
    {
        SeStatus = SE_REGR(s_pSeHWCtx, STATUS);
        if (NV_DRF_VAL(SE, STATUS, STATE, SeStatus) ==  SE_STATUS_0_STATE_IDLE)
            break;
        else
        {
            SeIntStatus = SE_REGR(s_pSeHWCtx, INT_STATUS);
            SE_REGW(s_pSeHWCtx, INT_STATUS, SeIntStatus);
            if (SeIntStatus & SE_INT_ENABLE_0_RESEED_CNTR_EXHAUSTED_FIELD)
            {
                SE_REGW(s_pSeHWCtx, IN_LL_ADDR, REIInputAddr);
                SE_REGW(s_pSeHWCtx, OPERATION, SE_OPERATION_0_OP_RESTART_IN);
                EndTime += CurTime;
            }
        }
        CurTime = NvOsGetTimeMS();
    }
    if (CurTime > EndTime)
        return NvError_Timeout;
    return NvSuccess;
}

static NvU32 SeRsaConvertModulusSize(NvU32 ModulusSizeInBits)
{
    switch (ModulusSizeInBits)
    {
        case 2048:
            return SE_RSA_KEY_SIZE_0_VAL_WIDTH_2048;
        case 1536:
            return SE_RSA_KEY_SIZE_0_VAL_WIDTH_1536;
        case 1024:
            return SE_RSA_KEY_SIZE_0_VAL_WIDTH_1024;
        case 512:
            return SE_RSA_KEY_SIZE_0_VAL_WIDTH_512;
        default:
            return 0;
    }
}

NvError SeRSASetKey(NvDdkSeRsaHWCtx* const hSeRsaHwCtx)
{
    NvError e = NvSuccess;
    NvU32 i, RegVal, Pkt;
    NvU32 *data;

    if (!s_pSeHWCtx ||!hSeRsaHwCtx)
    {
        e = NvError_NotInitialized;
        goto fail;
    }

    SE_REGW(s_pSeHWCtx, RSA_EXP_SIZE, (hSeRsaHwCtx->RsaExponentLenInBits / 32));
    SE_REGW(s_pSeHWCtx, RSA_KEY_SIZE,
            SeRsaConvertModulusSize(hSeRsaHwCtx->RsaModulusLenInBits));

    data = (NvU32 *)hSeRsaHwCtx->RsaModulus;
    RegVal = 0;
    Pkt = 0;

    /* Writing Modululus and exponent in respective slots */
    for (i = 0; i < (hSeRsaHwCtx->RsaModulusLenInBits / 32); i++)
    {
        /* Write the data into KEYTABLE_DATA Register */
        SE_REGW(s_pSeHWCtx, RSA_KEYTABLE_DATA, *data);

        /* Program KEYTABLE_ADDR Register */
        Pkt  = (Pkt | hSeRsaHwCtx->RsaKeySlot)            << SE_RSA_KEY_PKT_KEY_SLOT_SHIFT   |
               (Pkt | SE_RSA_KEY_PKT_EXPMOD_SEL_MODULUS)  << SE_RSA_KEY_PKT_EXPMOD_SEL_SHIFT |
               (Pkt | SE_RSA_KEY_PKT_INPUT_MODE_REGISTER) << SE_RSA_KEY_PKT_INPUT_MODE_SHIFT |
               (Pkt | (SE_RSA_KEY_PKT_WORD_ADDR_FIELD & ((hSeRsaHwCtx->RsaModulusLenInBits / 32) - i - 1)));
        RegVal  = NV_DRF_NUM(SE, RSA_KEYTABLE_ADDR, OP, SE_RSA_KEYTABLE_ADDR_0_OP_WRITE) |
                  NV_DRF_NUM(SE, RSA_KEYTABLE_ADDR, PKT, Pkt);
        SE_REGW(s_pSeHWCtx, RSA_KEYTABLE_ADDR, RegVal);

        data++;
        Pkt = 0;
    }

    data = (NvU32 *)hSeRsaHwCtx->RsaExponent;
    for (i = 0; i < (hSeRsaHwCtx->RsaExponentLenInBits / 32); i++)
    {
        /* Write The data into KEYTABLE_DATA register */
        SE_REGW(s_pSeHWCtx, RSA_KEYTABLE_DATA, *data);

        /* Program KEYTABLE_ADDR Register */
        Pkt  = (Pkt | hSeRsaHwCtx->RsaKeySlot)            << SE_RSA_KEY_PKT_KEY_SLOT_SHIFT   |
               (Pkt | SE_RSA_KEY_PKT_EXPMOD_SEL_EXPONENT) << SE_RSA_KEY_PKT_EXPMOD_SEL_SHIFT |
               (Pkt | SE_RSA_KEY_PKT_INPUT_MODE_REGISTER) << SE_RSA_KEY_PKT_INPUT_MODE_SHIFT |
               (Pkt | (SE_RSA_KEY_PKT_WORD_ADDR_FIELD & ((hSeRsaHwCtx->RsaExponentLenInBits / 32) - i - 1)));
        RegVal  = NV_DRF_NUM(SE, RSA_KEYTABLE_ADDR, OP, SE_RSA_KEYTABLE_ADDR_0_OP_WRITE) |
                  NV_DRF_NUM(SE, RSA_KEYTABLE_ADDR, PKT, Pkt);
        SE_REGW(s_pSeHWCtx, RSA_KEYTABLE_ADDR, RegVal);

        data++;
        Pkt = 0;
    }
fail:
    return e;
}

NvError SeRSAProcessBuffer(NvDdkSeRsaHWCtx* const hSeRsaHwCtx)
{
    NvError e           = NvSuccess;
    NvU32   SeConfigReg = 0;
    NvU32   RsaDest     = 0;

    if (!s_pSeHWCtx ||!hSeRsaHwCtx)
    {
        e = NvError_NotInitialized;
        goto fail;
    }

    /* Program the exponent and modulus sizes */
    SE_REGW(s_pSeHWCtx, RSA_EXP_SIZE, (hSeRsaHwCtx->RsaExponentLenInBits / 32));
    SE_REGW(s_pSeHWCtx, RSA_KEY_SIZE,
                        SeRsaConvertModulusSize(hSeRsaHwCtx->RsaModulusLenInBits));

    /* write the LL adress to IN_LL_ADDR */
    SE_REGW(s_pSeHWCtx, IN_LL_ADDR, hSeRsaHwCtx->InLLAddr);

    if (hSeRsaHwCtx->RsaDestIsMemory)
    {
        /* write the LL adress to OUT_LL_ADDR */
        SE_REGW(s_pSeHWCtx, OUT_LL_ADDR, hSeRsaHwCtx->OutLLAddr);
        RsaDest = SE_CONFIG_0_DST_MEMORY;
    }
    else
    {
        RsaDest = SE_CONFIG_0_DST_RSA_REG;
    }

    /* Set Modular exponentiation operation
     * SE_CONFIG.DEC_ALG = NOP
     * SE_CONFIG.ENC_ALG = RSA
     * SE_CONFIG.DEC_MODE = DEFAULT (use SE_MODE_PKT)
     * SE_CONFIG.ENC_MODE = Don't care (using SW_DEFAULT value)
     * SE_CONFIG.DST = RSA_REG or MEMORY depending on OutputDestination
     */
    SeConfigReg  =  NV_DRF_NUM(SE, CONFIG, ENC_ALG, SE_CONFIG_0_ENC_ALG_RSA)       |
                    NV_DRF_NUM(SE, CONFIG, DEC_ALG, SE_CONFIG_0_ENC_ALG_NOP)       |
                    NV_DRF_NUM(SE, CONFIG, DEC_MODE, SE_CONFIG_0_DEC_MODE_DEFAULT) |
                    NV_DRF_NUM(SE, CONFIG, ENC_MODE, SE_CONFIG_0_ENC_MODE_DEFAULT) |
                    NV_DRF_NUM(SE, CONFIG, DST, RsaDest);
   SE_REGW(s_pSeHWCtx, CONFIG, SeConfigReg);

   /* Set SE_CRYPTO_CONFIG.INPUT_SEL = AHB */
   SeConfigReg = NV_DRF_NUM(SE, CRYPTO_CONFIG, INPUT_SEL, SE_CRYPTO_CONFIG_0_INPUT_SEL_AHB);
   SE_REGW(s_pSeHWCtx, CRYPTO_CONFIG, SeConfigReg);

   /* Program the keyslot */
   SeConfigReg = NV_DRF_NUM(SE, RSA_CONFIG, KEY_SLOT, hSeRsaHwCtx->RsaKeySlot);
   SE_REGW(s_pSeHWCtx, RSA_CONFIG, SeConfigReg);

   /* Issue The start command */
   SE_REGW(s_pSeHWCtx, OPERATION, SE_OPERATION_0_OP_START);

   /* poll the status register */
   e = WaitForSeIdleState();
fail:
   return e;
}

NvError SeRsaCollectOutputData(NvU32 *Buffer, NvU32 Size, NvBool DestIsMemory)
{
    NvError e = NvSuccess;
    NvU32 i;

    if (!s_pSeHWCtx || !Buffer)
    {
        e = NvError_NotInitialized;
        goto fail;
    }

    if (DestIsMemory)
    {
        NvOsMemcpy((NvU8 *)Buffer, (NvU8 *)s_pSeHWCtx->OutputLL[1].LLBufVirAddr, Size);
    }
    else
    {
        for (i = 0; i < (Size / 4); i++)
        {
            Buffer[i] = RSA_OUT_REGR(s_pSeHWCtx, i);
        }
    }
fail:
    return e;
}

static NvU32 GetRngKeyMode(NvU32 RngKey)
{
    switch (RngKey)
    {
        case 0:
            return SE_MODE_PKT_AESMODE_KEY128;
        case 1:
            return SE_MODE_PKT_AESMODE_KEY192;
        case 2:
            return SE_MODE_PKT_AESMODE_KEY256;
    }
    return SE_MODE_PKT_AESMODE_KEY128;
}

static NvU32 GetRngDest(NvU32 DestVal)
{
    if (DestVal == 1)
        return SE_CONFIG_0_DST_SRK;

    return SE_CONFIG_0_DST_MEMORY;
}

static NvU32 GetRngSrc(NvU32 SrcVal)
{
    if (SrcVal == 1)
        return SE_CRYPTO_CONFIG_0_INPUT_SEL_RANDOM;

    return SE_CRYPTO_CONFIG_0_INPUT_SEL_AHB;
}

static NvU32 GetRngMode(NvU32 ModeVal)
{
    switch(ModeVal)
    {
        case 0:
            return SE_RNG_CONFIG_0_MODE_FORCE_INSTANTION;
        case 1:
            return SE_RNG_CONFIG_0_MODE_FORCE_RESEED;
        case 2:
            return SE_RNG_CONFIG_0_MODE_NORMAL;
    }
    return SE_RNG_CONFIG_0_MODE_NORMAL;
}


NvError SeGenerateRandomNumber(NvDdkSeRngHWCtx *hSeRngHwCtx)
{
    NvError e = NvSuccess;
    NvU32 RegVal = 0;
    NvU32 RngSrc;
    NvU32 RngDest;

    if (!hSeRngHwCtx)
    {
        e = NvError_NotInitialized;
        goto fail;
    }

    /* Get the destination*/
    RngDest = GetRngDest(hSeRngHwCtx->RngDest);
    /* Program SE_CONFIG_0 register */
    RegVal = NV_DRF_NUM(SE, CONFIG, ENC_ALG, SE_CONFIG_0_ENC_ALG_RNG)                 |
             NV_DRF_NUM(SE, CONFIG, DEC_ALG, SE_CONFIG_0_DEC_ALG_NOP)                 |
             NV_DRF_NUM(SE, CONFIG, ENC_MODE, GetRngKeyMode(hSeRngHwCtx->RngKeyUsed)) |
             NV_DRF_NUM(SE, CONFIG, DEC_MODE, 0x00)                                   |
             NV_DRF_NUM(SE, CONFIG, DST, RngDest);
    SE_REGW(s_pSeHWCtx, CONFIG, RegVal);

    /* Get the source */
    RngSrc = GetRngSrc(hSeRngHwCtx->RngSrc);

    /* Program SE_CRYPTO_CONFIG_0 register */
    /* In T114, RNG operation updates UIV, so program keyslot 13 as a WAR for HW bug 1014605
       This will ensure RNG not to update UIVs of any other key slots which are used for
       other crypto operations */
    RegVal = NV_DRF_NUM(SE, CRYPTO_CONFIG, HASH_ENB, SE_CRYPTO_CONFIG_0_HASH_ENB_DISABLE) |
             NV_DRF_NUM(SE, CRYPTO_CONFIG, XOR_POS, SE_CRYPTO_CONFIG_0_XOR_POS_BYPASS)    |
             NV_DRF_NUM(SE, CRYPTO_CONFIG, CORE_SEL, SE_CRYPTO_CONFIG_0_CORE_SEL_ENCRYPT) |
             NV_DRF_NUM(SE, CRYPTO_CONFIG, KEY_INDEX, SeAesHwKeySlot_13)    |
             NV_DRF_NUM(SE, CRYPTO_CONFIG, INPUT_SEL, RngSrc);
    SE_REGW(s_pSeHWCtx, CRYPTO_CONFIG, RegVal);

    RegVal = NV_DRF_NUM(SE, RNG_CONFIG, MODE, GetRngMode(hSeRngHwCtx->RngInitMode));
    /* Program Input LL address if source is DMA*/
    if (RngSrc == SE_CRYPTO_CONFIG_0_INPUT_SEL_AHB)
    {
        RegVal |= NV_DRF_NUM(SE, RNG_CONFIG, SRC, SE_RNG_CONFIG_0_SRC_NONE);
        SE_REGW(s_pSeHWCtx, IN_LL_ADDR, hSeRngHwCtx->InLLAddr);
    }
    else
    {
        RegVal |= NV_DRF_NUM(SE, RNG_CONFIG, SRC, SE_RNG_CONFIG_0_SRC_ENTROPY);
    }
    /* Program SE_RNG_CONFIG_0 register */
    SE_REGW(s_pSeHWCtx, RNG_CONFIG, RegVal);

    /* Program Output Linked list Address if Destination is Memory */
    if (RngDest == SE_CONFIG_0_DST_MEMORY)
    {
        SE_REGW(s_pSeHWCtx, OUT_LL_ADDR, hSeRngHwCtx->OutLLAddr);
    }
    else
    {
        /* Destination as SRK is been not supported */
        e = NvError_NotSupported;
        goto fail;
    }

    /* Program Number of random numbers of size
     * SE_RNG_OUT_SIZE to be genarated
     */
    SE_REGW(s_pSeHWCtx, CRYPTO_LAST_BLOCK,
            (((hSeRngHwCtx->RngOutSizeInBytes + SE_RNG_OUT_SIZE - 1) / SE_RNG_OUT_SIZE) - 1));

    /* Program Reseed Interval */
    SE_REGW(s_pSeHWCtx, RNG_RESEED_INTERVAL,  hSeRngHwCtx->RngReseedCounter);

    /* REI and its handling */
    REIInputAddr = hSeRngHwCtx->REIInputLLAddr;

    /* Start operation */
    SE_REGW(s_pSeHWCtx, OPERATION, SE_OPERATION_0_OP_START);

    /* Poll for Idle state of engine */
    e = WaitForSeIdleState();
fail:
    return e;
}
