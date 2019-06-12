/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
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
#include "nvddk_se_core.h"
#include "nvddk_se_hw.h"
#include "nvddk_se_private.h"

// Macro definitions //
// SE register read macro
#define SE_REGR(hSE, reg) \
     NV_READ32(hSE->pVirtualAddress + ((SE_##reg##_0) / 4))

// SE register write macro
#define SE_REGW(hSE, reg, data) \
do \
{ \
    NV_WRITE32(hSE->pVirtualAddress + ((SE_##reg##_0) / 4), (data)); \
}while (0)

#define SE_KEYTABLE_DATA_WRITE(hSE, Num, Data) \
do \
{ \
    NV_WRITE32((hSE->pVirtualAddress + ((SE_CRYPTO_KEYTABLE_DATA_0) / 4) + Num), (Data)); \
}while(0)

#define SE_KEY_ACCESS_REG_READ(hSE, KeySlotIndex) \
    NV_READ32(hSE->pVirtualAddress + ((SE_CRYPTO_KEYTABLE_ACCESS_0) / 4) +  KeySlotIndex);

#define SE_KEY_ACCESS_REG_WRITE(hSE, KeySlotIndex, RegVal) \
    NV_WRITE32((hSE->pVirtualAddress + ((SE_CRYPTO_KEYTABLE_ACCESS_0) / 4) + KeySlotIndex), (RegVal));

#define SE_KEYTABLE_DATA_READ(hSE, Num) \
do \
{ \
NV_READ32((hSE->pVirtualAddress + ((SE_CRYPTO_KEYTABLE_DATA_0) / 4) + Num)); \
}while(0)

extern NvDdkSeHWCtx *s_pSeHWCtx;
extern NvOsSemaphoreHandle s_DualBufSem;

static NvError WaitForSeIdleState(void)
{
    volatile NvU32 SeStatusReg = 0;
    volatile NvU32 SeIntStatusReg = 0;
    NvU32 CurTime, EndTime;

    EndTime = NvOsGetTimeMS() + SE_OP_MAX_TIME;
    CurTime = NvOsGetTimeMS();
    while(CurTime <= EndTime)
    {
        SeStatusReg = SE_REGR(s_pSeHWCtx, STATUS);
        if(NV_DRF_VAL(SE, STATUS, STATE, SeStatusReg) == SE_STATUS_0_STATE_IDLE)
        {
            /* Clear the INT_STATUS register*/
            SeIntStatusReg = SE_REGR(s_pSeHWCtx, INT_STATUS);
            SE_REGW(s_pSeHWCtx, INT_STATUS, SeIntStatusReg);

            if (NV_DRF_VAL(SE, INT_STATUS, ERR_STAT, SeIntStatusReg)
                    == SE_INT_STATUS_0_ERR_STAT_ACTIVE)
            {
                return NvError_InvalidState;
            }
            break;
        }
        CurTime = NvOsGetTimeMS();
    }
    if (CurTime > EndTime)
        return NvError_Timeout;
    return NvSuccess;
}

NvError SeKeySchedReadDisable(void)
{
    NvError e = NvSuccess;
    NvU32 reg_val = 0;

    if (s_pSeHWCtx == NULL)
    {
        e = NvError_NotInitialized;
        goto fail;
    }
    reg_val = SE_REGR(s_pSeHWCtx, SE_SECURITY);
    reg_val = NV_FLD_SET_DRF_DEF( SE, SE_SECURITY, KEY_SCHED_READ,\
                                  DISABLE, reg_val);
    SE_REGW(s_pSeHWCtx, SE_SECURITY, reg_val);

fail:
    return e;
}

void SeClearInterrupts(void)
{
    /* Clearing all interrupts */
    SE_REGW(s_pSeHWCtx, INT_ENABLE, SE_INT_ENABLE_0_SW_DEFAULT_VAL);
}

NvError SeSHAProcessBuffer(NvDdkSeShaHWCtx *hSeShaHwCtx)
{
    NvError e = NvSuccess;
    NvU32 val = 0;
    NvU32 i = 0;

    if (hSeShaHwCtx == NULL)
    {
        e = NvError_NotInitialized;
        goto fail;
    }

    /*  Program Registers */

    /*  SE_CONFIG */
    val =   NV_DRF_NUM( SE, CONFIG, ENC_MODE, hSeShaHwCtx->ShaPktMode) |
            NV_DRF_NUM( SE, CONFIG, DEC_MODE, 0x0) |
            NV_DRF_NUM( SE, CONFIG, ENC_ALG, SE_CONFIG_0_ENC_ALG_SHA) |
            NV_DRF_NUM( SE, CONFIG, DEC_ALG, SE_CONFIG_0_DEC_ALG_NOP) |
            NV_DRF_NUM( SE, CONFIG, DST, SE_CONFIG_0_DST_HASH_REG);
    SE_REGW(s_pSeHWCtx, CONFIG, val);

    /*  IN_LL_ADDR */
    SE_REGW(s_pSeHWCtx, IN_LL_ADDR, (NvU32)(hSeShaHwCtx->LLAddr));

    /*  SHA_CONFIG */
    val = NV_DRF_NUM(SE, SHA_CONFIG, HW_INIT_HASH, hSeShaHwCtx->IsSHAInitHwHash);
    SE_REGW(s_pSeHWCtx, SHA_CONFIG, val);

    /*  Initializing MSG_LENGTH & MSG_LEFT with zeros */
    for (i = 0; i < 4; i++)
    {
        NV_WRITE32(s_pSeHWCtx->pVirtualAddress + (SE_SHA_MSG_LENGTH_0 / 4) + i, 0);
        NV_WRITE32(s_pSeHWCtx->pVirtualAddress + (SE_SHA_MSG_LEFT_0 / 4) + i, 0);
    }

    /*  SE_SHA_MSG_LENGTH */
    NV_WRITE32(s_pSeHWCtx->pVirtualAddress + (SE_SHA_MSG_LENGTH_0 / 4), \
                                            (NvU32)(hSeShaHwCtx->MsgLength));
    NV_WRITE32(s_pSeHWCtx->pVirtualAddress + (SE_SHA_MSG_LENGTH_1 / 4), \
                                            *(((NvU32*)(&hSeShaHwCtx->MsgLength)) + 1));
    /*  SE_SHA_MSG_LEFT */
    NV_WRITE32(s_pSeHWCtx->pVirtualAddress + (SE_SHA_MSG_LEFT_0 / 4), \
                                            (NvU32)(hSeShaHwCtx->MsgLeft));
    NV_WRITE32(s_pSeHWCtx->pVirtualAddress + (SE_SHA_MSG_LEFT_1 / 4), \
                                            *(((NvU32*)(&hSeShaHwCtx->MsgLeft)) + 1));

    if (!(hSeShaHwCtx->IsSHAInitHwHash))
    {
        /*  Loading Previous Hash values */
        for (i = 0; i < 16; i++)
        {
            NV_WRITE32((s_pSeHWCtx->pVirtualAddress + (SE_HASH_RESULT / 4) + i),\
                        hSeShaHwCtx->HashResult[i]);
        }
    }

    /*  INT_ENABLE */
    val = NV_DRF_NUM(SE,INT_ENABLE, SE_OP_DONE, SE_INT_ENABLE_0_SE_OP_DONE_ENABLE) |
          NV_DRF_NUM(SE,INT_ENABLE, ERR_STAT, SE_INT_ENABLE_0_ERR_STAT_ENABLE);
    SE_REGW(s_pSeHWCtx, INT_ENABLE, val);

    /*  OPERATION */
    SE_REGW(s_pSeHWCtx, OPERATION, SE_OPERATION_0_OP_START);

    if (hSeShaHwCtx->IsSHAInitHwHash)
        hSeShaHwCtx->IsSHAInitHwHash = 0;
    e = WaitForSeIdleState();

fail:
    return e;
}

NvError SeSHABackupRegisters(NvDdkSeShaHWCtx *hSeShaHwCtx)
{
    NvU32 i = 0;
    NvError e = NvSuccess;

    if (hSeShaHwCtx == NULL)
    {
        e = NvError_NotInitialized;
        goto fail;
    }

    /*  Reading 'msgleft' value */
    (hSeShaHwCtx->MsgLeft) = NV_READ32(s_pSeHWCtx->pVirtualAddress +
                                                        (SE_SHA_MSG_LEFT_0 / 4));
    *((NvU32*)(&hSeShaHwCtx->MsgLeft) + 1) = NV_READ32(s_pSeHWCtx->pVirtualAddress +
                                                        (SE_SHA_MSG_LEFT_1 / 4));

    /*  Loading Hash values from registers */
    for (i = 0; i < 16; i++)
    {
        hSeShaHwCtx->HashResult[i] = NV_READ32(s_pSeHWCtx->pVirtualAddress +
                                                        (SE_HASH_RESULT / 4) + i);
    }
fail:
    return e;
}

void SeIsr(void* args)
{
    NvU32 int_status = 0;

    int_status = SE_REGR(s_pSeHWCtx, INT_STATUS);
    /*  write the status register with the value red from it */
    SE_REGW(s_pSeHWCtx, INT_STATUS, int_status);

    /*  Check for OP_DONE status */
    if (!(int_status & SE_INT_STATUS_0_SE_OP_DONE_FIELD))
        s_pSeHWCtx->ErrFlag = NV_TRUE;

    /*  Unlock the mutex for submitting the next buffer */
    NvOsSemaphoreSignal(s_DualBufSem);
}

static void FillUpKeyTableDataRegs(NvU32 *Data, NvU32 NumOfRegsToFill)
{
    NvU32 RegNum;
    /* Making All Registers zero to ensure Correctness */
    for (RegNum = 0; RegNum < ARSE_CRYPTO_KEYTABLE_DATA_WORDS; RegNum++)
        SE_KEYTABLE_DATA_WRITE(s_pSeHWCtx, RegNum, 0x0);
    /* Write Data into Data Registers */
    for (RegNum = 0; RegNum < NumOfRegsToFill; RegNum++)
    {
        SE_KEYTABLE_DATA_WRITE(s_pSeHWCtx, RegNum, *Data);
        Data++;
    }
}

static NvU32 ConvertKeyLenToPktMode(NvU32 KeyLength)
{
    switch (KeyLength)
    {
        case NvDdkSeAesKeySize_128Bit:
            return SE_MODE_PKT_AESMODE_KEY128;
        case NvDdkSeAesKeySize_192Bit:
            return SE_MODE_PKT_AESMODE_KEY192;
        case NvDdkSeAesKeySize_256Bit:
            return SE_MODE_PKT_AESMODE_KEY256;
        default:
            return SE_MODE_PKT_AESMODE_KEY128;
    }
    return SE_MODE_PKT_AESMODE_KEY128;
}

NvError SeAesSetKey(NvDdkSeAesHWCtx *hSeAesHwCtx)
{
    NvError e = NvSuccess;
    NvU32 SeAesConfigReg;
    NvU32 KeyWordIndex;
    NvU32 Pkt;
    NvU32 WordQuadNum;
    NvU32 NumOfWordsToWrite;

    if (hSeAesHwCtx == NULL)
    {
        e = NvError_NotInitialized;
        goto fail;
    }
    NumOfWordsToWrite = hSeAesHwCtx->KeyLenInBytes >> 2;

    /* Write Key In to Key slot */
    KeyWordIndex = 0;
    for (WordQuadNum = 0; WordQuadNum <
            ((NumOfWordsToWrite + ARSE_CRYPTO_KEYTABLE_DATA_WORDS - 1)
             / ARSE_CRYPTO_KEYTABLE_DATA_WORDS);
            WordQuadNum++)
    {
        FillUpKeyTableDataRegs((NvU32 *)hSeAesHwCtx->Key + KeyWordIndex,
               (NumOfWordsToWrite - (WordQuadNum * ARSE_CRYPTO_KEYTABLE_DATA_WORDS) >=
                ARSE_CRYPTO_KEYTABLE_DATA_WORDS) ? ARSE_CRYPTO_KEYTABLE_DATA_WORDS :
                (NumOfWordsToWrite - (WordQuadNum * ARSE_CRYPTO_KEYTABLE_DATA_WORDS)));

       /* Preapare PKT Structure to program key into proper key chunck */
        Pkt = 0;
        Pkt = ((Pkt | hSeAesHwCtx->KeySlotNum) <<
                SE_CRYPTO_KEYIV_PKT_KEY_INDEX_SHIFT) |
              ((Pkt | SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_KEY)
               << SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_SHIFT) |
              ((Pkt | WordQuadNum) << SE_CRYPTO_KEYIV_PKT_WORD_QUAD_SHIFT);

        /* Program KEYTABLE_ADDR */
        SeAesConfigReg = NV_DRF_NUM(SE, CRYPTO_KEYTABLE_ADDR, TABLE_SEL,
                         SE_CRYPTO_KEYTABLE_ADDR_0_TABLE_SEL_KEYIV)  |
                         NV_DRF_NUM(SE, CRYPTO_KEYTABLE_ADDR, OP,
                                 SE_CRYPTO_KEYTABLE_ADDR_0_OP_WRITE) |
                         NV_DRF_NUM(SE, CRYPTO_KEYTABLE_ADDR, PKT, Pkt);
        SE_REGW(s_pSeHWCtx, CRYPTO_KEYTABLE_ADDR, SeAesConfigReg);

        KeyWordIndex += ARSE_CRYPTO_KEYTABLE_DATA_WORDS;
    }
fail:
    return e;
}

NvError SeAesProcessBuffer(NvDdkSeAesHWCtx *hSeAesHwCtx)
{
    NvError e = NvSuccess;
    NvU32 SeAesConfigReg;
    NvU32 int_enable;

    if (hSeAesHwCtx == NULL)
    {
        e = NvError_NotInitialized;
        goto fail;
    }
    if (hSeAesHwCtx->IsEncryption)
    {
        SeAesConfigReg = NV_DRF_NUM(SE, CONFIG, ENC_MODE,
                ConvertKeyLenToPktMode(hSeAesHwCtx->KeyLenInBytes)) |
                         NV_DRF_NUM(SE, CONFIG, DEC_MODE,
                                 SE_CONFIG_0_DEC_MODE_SW_DEFAULT) |
                         NV_DRF_NUM(SE, CONFIG, ENC_ALG,
                                 SE_CONFIG_0_ENC_ALG_AES_ENC) |
                         NV_DRF_NUM(SE, CONFIG, DEC_ALG,
                                 SE_CONFIG_0_DEC_ALG_NOP) |
                         NV_DRF_NUM(SE, CONFIG, DST, SE_CONFIG_0_DST_MEMORY);
        SE_REGW(s_pSeHWCtx, CONFIG, SeAesConfigReg);

        /* Pack Algo Dependent Information */
        switch (hSeAesHwCtx->OpMode)
        {
            case NvDdkSeAesOperationalMode_Cbc:
                {
                    SeAesConfigReg = NV_DRF_NUM(SE, CRYPTO_CONFIG,
                            XOR_POS, SE_CRYPTO_CONFIG_0_XOR_POS_TOP) |
                                     NV_DRF_NUM(SE, CRYPTO_CONFIG,
                                             INPUT_SEL,
                                             SE_CRYPTO_CONFIG_0_INPUT_SEL_AHB) |
                                     NV_DRF_NUM(SE, CRYPTO_CONFIG,
                                             VCTRAM_SEL,
                                             SE_CRYPTO_CONFIG_0_VCTRAM_SEL_INIT_AESOUT) |
                                     NV_DRF_NUM(SE, CRYPTO_CONFIG,
                                             IV_SELECT,
                                             SE_CRYPTO_CONFIG_0_IV_SELECT_UPDATED) |
                                     NV_DRF_NUM(SE, CRYPTO_CONFIG,
                                             KEY_INDEX, hSeAesHwCtx->KeySlotNum) |
                                     NV_DRF_NUM(SE, CRYPTO_CONFIG, CORE_SEL,
                                             SE_CRYPTO_CONFIG_0_CORE_SEL_ENCRYPT);
                    break;
                }
            case NvDdkSeAesOperationalMode_Ecb:
                {
                    SeAesConfigReg = NV_DRF_NUM(SE, CRYPTO_CONFIG, XOR_POS,
                            SE_CRYPTO_CONFIG_0_XOR_POS_BYPASS) |
                                     NV_DRF_NUM(SE, CRYPTO_CONFIG,
                                            INPUT_SEL, SE_CRYPTO_CONFIG_0_INPUT_SEL_AHB) |
                                     NV_DRF_NUM(SE, CRYPTO_CONFIG, VCTRAM_SEL,
                                             SE_CRYPTO_CONFIG_0_VCTRAM_SEL_SW_DEFAULT) |
                                     NV_DRF_NUM(SE, CRYPTO_CONFIG,
                                             KEY_INDEX, hSeAesHwCtx->KeySlotNum) |
                                     NV_DRF_NUM(SE, CRYPTO_CONFIG,
                                             CORE_SEL, SE_CRYPTO_CONFIG_0_CORE_SEL_ENCRYPT);
                    break;
                }
            default:
                e = NvError_NotSupported;
                goto fail;
        }
    }
    else
    {
        SeAesConfigReg = NV_DRF_NUM(SE, CONFIG,
                ENC_MODE, SE_CONFIG_0_ENC_ALG_SW_DEFAULT) |
                         NV_DRF_NUM(SE, CONFIG,
                                 DEC_MODE,
                                 ConvertKeyLenToPktMode(hSeAesHwCtx->KeyLenInBytes)) |
                         NV_DRF_NUM(SE, CONFIG,
                                 ENC_ALG, SE_CONFIG_0_ENC_ALG_NOP) |
                         NV_DRF_NUM(SE, CONFIG,
                                 DEC_ALG, SE_CONFIG_0_DEC_ALG_AES_DEC) |
                         NV_DRF_NUM(SE, CONFIG, DST, SE_CONFIG_0_DST_MEMORY);
        SE_REGW(s_pSeHWCtx, CONFIG, SeAesConfigReg);

        /* Pack Algo Dependent Information */
       switch(hSeAesHwCtx->OpMode)
       {
            case NvDdkSeAesOperationalMode_Cbc:
                {

                    SeAesConfigReg = NV_DRF_NUM(SE, CRYPTO_CONFIG,
                            XOR_POS, SE_CRYPTO_CONFIG_0_XOR_POS_BOTTOM) |
                                     NV_DRF_NUM(SE, CRYPTO_CONFIG,
                                             INPUT_SEL, SE_CRYPTO_CONFIG_0_INPUT_SEL_AHB) |
                                     NV_DRF_NUM(SE, CRYPTO_CONFIG,
                                             VCTRAM_SEL, SE_CRYPTO_CONFIG_0_VCTRAM_SEL_INIT_PREVAHB) |
                                     NV_DRF_NUM(SE, CRYPTO_CONFIG,
                                             IV_SELECT, SE_CRYPTO_CONFIG_0_IV_SELECT_UPDATED) |
                                     NV_DRF_NUM(SE, CRYPTO_CONFIG,
                                             KEY_INDEX, hSeAesHwCtx->KeySlotNum) |
                                     NV_DRF_NUM(SE, CRYPTO_CONFIG,
                                             CORE_SEL, SE_CRYPTO_CONFIG_0_CORE_SEL_DECRYPT);
                    break;
                }
            case NvDdkSeAesOperationalMode_Ecb:
                {
                    SeAesConfigReg = NV_DRF_NUM(SE, CRYPTO_CONFIG, XOR_POS,
                            SE_CRYPTO_CONFIG_0_XOR_POS_BYPASS) |
                                     NV_DRF_NUM(SE, CRYPTO_CONFIG,
                                             INPUT_SEL, SE_CRYPTO_CONFIG_0_INPUT_SEL_AHB) |
                                     NV_DRF_NUM(SE, CRYPTO_CONFIG, VCTRAM_SEL,
                                             SE_CRYPTO_CONFIG_0_VCTRAM_SEL_SW_DEFAULT) |
                                     NV_DRF_NUM(SE, CRYPTO_CONFIG,
                                             KEY_INDEX, hSeAesHwCtx->KeySlotNum) |
                                     NV_DRF_NUM(SE, CRYPTO_CONFIG,
                                             CORE_SEL, SE_CRYPTO_CONFIG_0_CORE_SEL_DECRYPT);
                    break;

                }
            default:
                e = NvError_NotSupported;
                goto fail;
        }
    }

    /* Program CRYPTO_CONFIG Register */
    SE_REGW(s_pSeHWCtx, CRYPTO_CONFIG, SeAesConfigReg);

    /* Program Input LinkedList Address in IN_LL_ADDR */
    SE_REGW(s_pSeHWCtx, IN_LL_ADDR, hSeAesHwCtx->InLLAddr);

    /* Program Output LinkedList Address in OUT_LL_ADDR */
    SE_REGW(s_pSeHWCtx, OUT_LL_ADDR, hSeAesHwCtx->OutLLAddr);

    /* Program Number of blocks to be processed */
    SE_REGW(s_pSeHWCtx, CRYPTO_LAST_BLOCK,
           (hSeAesHwCtx->SrcBufSize / NvDdkSeAesConst_BlockLengthBytes) - 1);

    /*  Enable interrupts */
    int_enable = NV_DRF_NUM(SE,INT_ENABLE, SE_OP_DONE, SE_INT_ENABLE_0_SE_OP_DONE_ENABLE) |
               NV_DRF_NUM(SE,INT_ENABLE, ERR_STAT, SE_INT_ENABLE_0_ERR_STAT_ENABLE);
    SE_REGW(s_pSeHWCtx, INT_ENABLE, int_enable);

    /* Start Operation */
    SE_REGW(s_pSeHWCtx, OPERATION, SE_OPERATION_0_OP_START);

    /* poll for SE_STATUS_0 */
    e = WaitForSeIdleState();
fail:
    return e;
}

NvError SeAesCMACProcessBuffer(NvDdkSeAesHWCtx *hSeAesHwCtx)
{
    NvError e = NvSuccess;
    NvU32 SeAesConfigReg;
    NvU32 int_enable;

    if (hSeAesHwCtx == NULL)
    {
        e = NvError_NotInitialized;
        goto fail;
    }

    SeAesConfigReg = NV_DRF_NUM(SE, CONFIG,
            ENC_MODE, ConvertKeyLenToPktMode(hSeAesHwCtx->KeyLenInBytes)) |
                     NV_DRF_NUM(SE, CONFIG,
                             ENC_ALG, SE_CONFIG_0_ENC_ALG_AES_ENC) |
                     NV_DRF_NUM(SE, CONFIG, DEC_ALG, SE_CONFIG_0_DEC_ALG_NOP) |
                     NV_DRF_NUM(SE, CONFIG, DST, SE_CONFIG_0_DST_HASH_REG);
    SE_REGW(s_pSeHWCtx, CONFIG, SeAesConfigReg);

    SeAesConfigReg = NV_DRF_NUM(SE, CRYPTO_CONFIG,
            XOR_POS, SE_CRYPTO_CONFIG_0_XOR_POS_TOP) |
                     NV_DRF_NUM(SE, CRYPTO_CONFIG, INPUT_SEL,
                             SE_CRYPTO_CONFIG_0_INPUT_SEL_AHB) |
                     NV_DRF_NUM(SE, CRYPTO_CONFIG, VCTRAM_SEL,
                             SE_CRYPTO_CONFIG_0_VCTRAM_SEL_INIT_AESOUT) |
                     NV_DRF_NUM(SE, CRYPTO_CONFIG, IV_SELECT,
                             SE_CRYPTO_CONFIG_0_IV_SELECT_UPDATED) |
                     NV_DRF_NUM(SE, CRYPTO_CONFIG,
                             KEY_INDEX, hSeAesHwCtx->KeySlotNum) |
                     NV_DRF_NUM(SE, CRYPTO_CONFIG, HASH_ENB,
                             SE_CRYPTO_CONFIG_0_HASH_ENB_ENABLE) |
                     NV_DRF_NUM(SE, CRYPTO_CONFIG, CORE_SEL,
                             SE_CRYPTO_CONFIG_0_CORE_SEL_ENCRYPT);
    /* Program CRYPTO_CONFIG Register */
    SE_REGW(s_pSeHWCtx, CRYPTO_CONFIG, SeAesConfigReg);

    /* Program Number of blocks of Output to be genrated */
    SE_REGW(s_pSeHWCtx, CRYPTO_LAST_BLOCK,
            (hSeAesHwCtx->SrcBufSize / NvDdkSeAesConst_BlockLengthBytes) - 1);

    /* Program Input LinkedList Address in IN_LL_ADDR */
    SE_REGW(s_pSeHWCtx, IN_LL_ADDR, hSeAesHwCtx->InLLAddr);

    /*  INT_ENABLE */
    int_enable = NV_DRF_NUM(SE,INT_ENABLE, SE_OP_DONE, SE_INT_ENABLE_0_SE_OP_DONE_ENABLE)|
        NV_DRF_NUM(SE,INT_ENABLE, ERR_STAT, SE_INT_ENABLE_0_ERR_STAT_ENABLE);
    SE_REGW(s_pSeHWCtx, INT_ENABLE, int_enable);


   /* Start Operation */
    SE_REGW(s_pSeHWCtx, OPERATION, SE_OPERATION_0_OP_START);

    /* poll for SE_STATUS_0 */
    e = WaitForSeIdleState();
fail:
   return e;
}

NvError CollectCMAC(NvU32 *pBuffer)
{
    NvU32 i;

    if (pBuffer == NULL)
        return NvError_BadParameter;

    for (i = 0; i < 4; i++)
    {
        pBuffer[i] = NV_READ32(s_pSeHWCtx->pVirtualAddress + (SE_HASH_RESULT / 4) + i);
    }

    return NvSuccess;
}

void SeAesWriteLockKeySlot(SeAesHwKeySlot KeySlotIndex)
{
    NvU32 SeKeyAccessReg;

    SeKeyAccessReg = SE_KEY_ACCESS_REG_READ(s_pSeHWCtx, KeySlotIndex);
    SeKeyAccessReg = NV_FLD_SET_DRF_NUM(SE, CRYPTO_KEYTABLE_ACCESS, KEYUPDATE,
                         SE_CRYPTO_KEYTABLE_ACCESS_0_KEYUPDATE_DISABLE,
                         SeKeyAccessReg);
    SE_KEY_ACCESS_REG_WRITE(s_pSeHWCtx, KeySlotIndex, SeKeyAccessReg);
    SeKeyAccessReg = SE_KEY_ACCESS_REG_READ(s_pSeHWCtx, KeySlotIndex);
}

void SeAesSetIv(NvDdkSeAesHWCtx *hSeAesHwCtx)
{
    NvU32 Pkt;
    NvU32 SeAesConfigReg;

    FillUpKeyTableDataRegs((NvU32 *)hSeAesHwCtx->IV, ARSE_CRYPTO_KEYTABLE_DATA_WORDS);

    Pkt = 0;
    Pkt = ((Pkt | hSeAesHwCtx->KeySlotNum) <<  SE_CRYPTO_KEYIV_PKT_KEY_INDEX_SHIFT) |
          ((Pkt | SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_IV) <<
           SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_SHIFT) |
          ((Pkt | SE_CRYPTO_KEYIV_PKT_WORD_QUAD_UPDATED_IVS) <<
           SE_CRYPTO_KEYIV_PKT_WORD_QUAD_SHIFT);

    SeAesConfigReg = NV_DRF_NUM(SE, CRYPTO_KEYTABLE_ADDR, TABLE_SEL,
                     SE_CRYPTO_KEYTABLE_ADDR_0_TABLE_SEL_KEYIV) |
                     NV_DRF_NUM(SE, CRYPTO_KEYTABLE_ADDR, OP,
                             SE_CRYPTO_KEYTABLE_ADDR_0_OP_WRITE) |
                     NV_DRF_NUM(SE, CRYPTO_KEYTABLE_ADDR, PKT, Pkt);
    SE_REGW(s_pSeHWCtx, CRYPTO_KEYTABLE_ADDR, SeAesConfigReg);
}

void SeAesGetIv(NvDdkSeAesHWCtx *hSeAesHwCtx)
{
    NvU32 Pkt;
    NvU32 SeAesConfigReg;
    NvU32 RegNum;
    NvU32 Data[4]={0};

    Pkt = 0;
    Pkt = ((Pkt | hSeAesHwCtx->KeySlotNum) <<  SE_CRYPTO_KEYIV_PKT_KEY_INDEX_SHIFT) |
          ((Pkt | SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_IV) <<
           SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_SHIFT) |
          ((Pkt | SE_CRYPTO_KEYIV_PKT_WORD_QUAD_UPDATED_IVS) <<
           SE_CRYPTO_KEYIV_PKT_WORD_QUAD_SHIFT);

    SeAesConfigReg = NV_DRF_NUM(SE, CRYPTO_KEYTABLE_ADDR, TABLE_SEL,
                     SE_CRYPTO_KEYTABLE_ADDR_0_TABLE_SEL_KEYIV) |
                     NV_DRF_NUM(SE, CRYPTO_KEYTABLE_ADDR, OP, 0) |
                     NV_DRF_NUM(SE, CRYPTO_KEYTABLE_ADDR, PKT, Pkt);
    SE_REGW(s_pSeHWCtx, CRYPTO_KEYTABLE_ADDR, SeAesConfigReg);

    /* Read Data from Data Registers */
    for (RegNum = 0; RegNum < ARSE_CRYPTO_KEYTABLE_DATA_WORDS; RegNum++)
    {
        Data[RegNum] = NV_READ32((s_pSeHWCtx->pVirtualAddress +
                    ((SE_CRYPTO_KEYTABLE_DATA_0) / 4) + RegNum));
    }
    NvOsMemcpy(hSeAesHwCtx->IV, Data, NvDdkSeAesConst_IVLengthBytes);
}
