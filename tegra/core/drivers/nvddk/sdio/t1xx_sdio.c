/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file
 * @brief <b>NVIDIA Driver Development Kit:
 *                 NvDDK T11x SDIO Driver Implementation</b>
 *
 * @b Description: Implementation of the NvDDK SDIO API.
 *
 */

#include "nvddk_sdio_private.h"
#include "nvodm_query_discovery.h"
#include "nvrm_pmu.h"

#if NV_IF_T148
#include "t14x/arsdmmc.h"
#include "t14x/arapbpm.h"
#else
#include "t11x/arsdmmc.h"
#include "t11x/arapbpm.h"
#endif

#define MAX_TAP_VALUES                   255
#define NVDDK_SDMMC_TUNING_TIMEOUT_MSEC  10000
#define TUNING_RETRY_COUNT               1
#define TUNING_LOW_FREQUENCY_KHZ         82000
#define TUNING_HIGH_FREQUENCY_KHZ        136000

typedef struct TuningWindowRec
{
    NvU32 PartialWin;
    NvU32 FirstFullWinStart;
    NvU32 FirstFullWinEnd;
    NvU32 BestTapValue;
    NvU32 WinCount;
    NvU32 UI;
    NvBool PartialWinFlag;
}TuningWindow;

static void T1xxSdioSetUhsmode(NvDdkSdioDeviceHandle hSdio, NvDdkSdioUhsMode Uhsmode)
{
    NvU32 val = 0;

    val = SDMMC_REGR(hSdio->pSdioVirtualAddress, AUTO_CMD12_ERR_STATUS);
    val = NV_FLD_SET_DRF_NUM(SDMMC, AUTO_CMD12_ERR_STATUS, UHS_MODE_SEL, Uhsmode, val);
    SDMMC_REGW(hSdio->pSdioVirtualAddress, AUTO_CMD12_ERR_STATUS, val);
}

static void T1xxSdioSetTapValue(NvDdkSdioDeviceHandle hSdio, NvU32 TapValue)
{
    NvU32 val = 0;
    NV_ASSERT(hSdio);
    NV_ASSERT(TapValue < 0xFF);

    val = SDMMC_REGR(hSdio->pSdioVirtualAddress, VENDOR_CLOCK_CNTRL);
    val = NV_FLD_SET_DRF_NUM(SDMMC, VENDOR_CLOCK_CNTRL, TAP_VAL, TapValue, val);
    SDMMC_REGW(hSdio->pSdioVirtualAddress, VENDOR_CLOCK_CNTRL, val);
}

static void T1xxSdioSetTrimValue(NvDdkSdioDeviceHandle hSdio, NvU32 TrimValue)
{
    NvU32 val = 0;
    NV_ASSERT(hSdio);
    NV_ASSERT(TrimValue < 0xFF);

    val = SDMMC_REGR(hSdio->pSdioVirtualAddress, VENDOR_CLOCK_CNTRL);
    val = NV_FLD_SET_DRF_NUM(SDMMC, VENDOR_CLOCK_CNTRL, TRIM_VAL, TrimValue, val);
    SDMMC_REGW(hSdio->pSdioVirtualAddress, VENDOR_CLOCK_CNTRL, val);
}

static void T1xxSdioGetHigherCapabilities(NvDdkSdioDeviceHandle hSdio,
    NvDdkSdioHostCapabilities *pHostCaps)
{
    NvU32 capabilities = 0;

    capabilities = SDMMC_REGR(hSdio->pSdioVirtualAddress, CAPABILITIES_HIGHER);
    if (NV_DRF_VAL(SDMMC, CAPABILITIES_HIGHER, DDR50, capabilities))
        pHostCaps->IsDDR50modeSupported = NV_TRUE;
    if (NV_DRF_VAL(SDMMC, CAPABILITIES_HIGHER, SDR104, capabilities))
    {
        pHostCaps->IsSDR104modeSupported = NV_FALSE;
    }
    if (NV_DRF_VAL(SDMMC, CAPABILITIES_HIGHER, SDR50, capabilities))
        pHostCaps->IsSDR50modeSupported = NV_TRUE;
}

static NvError T1xxRunFrequencyTuning(NvDdkSdioDeviceHandle hSdio)
{
    NvError e= NvSuccess;
    NvU32 ctrl;
    NvU32 resp[32];

    NvDdkSdioCommand *pCmd;
    NvU32 status = 0;

    pCmd = NvOsAlloc(sizeof(NvDdkSdioCommand));
    ctrl = SDMMC_REGR(hSdio->pSdioVirtualAddress, AUTO_CMD12_ERR_STATUS);
    ctrl = NV_FLD_SET_DRF_NUM(SDMMC, AUTO_CMD12_ERR_STATUS, SAMPLING_CLK_SEL, 0, ctrl);
    SDMMC_REGW(hSdio->pSdioVirtualAddress, AUTO_CMD12_ERR_STATUS, ctrl);

    ctrl = SDMMC_REGR(hSdio->pSdioVirtualAddress, AUTO_CMD12_ERR_STATUS);
    ctrl = NV_FLD_SET_DRF_NUM(SDMMC, AUTO_CMD12_ERR_STATUS, EXECUTE_TUNING, 1, ctrl);
    SDMMC_REGW(hSdio->pSdioVirtualAddress, AUTO_CMD12_ERR_STATUS, ctrl);

    SD_SET_COMMAND_PARAMETERS(pCmd,
            MmcCommand_ExecuteTuning,
            NvDdkSdioCommandType_Normal,
            NV_TRUE,
            0x0,
            NvDdkSdioRespType_R1,
            128);

    NV_CHECK_ERROR_CLEANUP(NvDdkSdioSendCommand(hSdio, pCmd, &status));

    NV_CHECK_ERROR_CLEANUP(NvDdkSdioGetCommandResponse(hSdio,
                                    pCmd->CommandCode,
                                    pCmd->ResponseType,
                                    resp));
    ctrl = SDMMC_REGR(hSdio->pSdioVirtualAddress, AUTO_CMD12_ERR_STATUS);
    if (!NV_DRF_VAL(SDMMC,AUTO_CMD12_ERR_STATUS, EXECUTE_TUNING, ctrl)  &&
        NV_DRF_VAL(SDMMC,AUTO_CMD12_ERR_STATUS, SAMPLING_CLK_SEL, ctrl) )
        e= NvSuccess;
    else
    {
         e = NvError_NotInitialized;
    }
fail:
    if (pCmd)
    {
        NvOsFree(pCmd);
    }
    return e;
}

static NvError
FindTapValueWindows(
    NvDdkSdioDeviceHandle hSdio,
     TuningWindow *pTapWin)
{
    NvError e = NvSuccess;
    NvU32 i = 0;
    NvU8 *pTapValueStatus;
    NvU8 Retry = TUNING_RETRY_COUNT;

    pTapValueStatus = NvOsAlloc(MAX_TAP_VALUES);
    if (!pTapValueStatus)
    {
        return NvError_NotInitialized;
    }
    NvOsMemset(pTapValueStatus, 0, sizeof(NvDdkSdioInfo));

    // run frequency tuning
    i = 0;
    do
    {
        T1xxSdioSetTapValue(hSdio, i);
        e = T1xxRunFrequencyTuning(hSdio);
        if (e != NvSuccess && Retry)
        {
            Retry --;
            continue;
        }
        else
            Retry = TUNING_RETRY_COUNT;
        pTapValueStatus[i] = (e == NvSuccess)? 1: 0;
        i++;
    }
    while (i < MAX_TAP_VALUES);

    // find partial Win and first full Win
    for (i = 0; i < MAX_TAP_VALUES; i++)
    {
        if (!pTapWin->PartialWinFlag && !pTapValueStatus[i])
        {
            if (i == 0)
                pTapWin->PartialWin = 0;
            else
                pTapWin->PartialWin = i - 1;
            pTapWin->PartialWinFlag = NV_TRUE;
        }
        if ((pTapValueStatus[i] != pTapValueStatus[i + 1]) &&
                (pTapWin->WinCount == 0))
        {
            if (pTapValueStatus[0] == 0)
                pTapWin->FirstFullWinStart = i + 1;
            pTapWin->WinCount++;
            continue;
        }
        else if ((pTapValueStatus[i] != pTapValueStatus[i + 1]) &&
                     (pTapWin->WinCount != 0))
        {
            if (pTapValueStatus[i])
            {
                pTapWin->FirstFullWinEnd = i;
                if ((pTapWin->FirstFullWinEnd > pTapWin->FirstFullWinStart) &&
                    (pTapWin->FirstFullWinEnd - pTapWin->FirstFullWinStart >= 10))
                {
                    break;
                }
            }
            else
                pTapWin->FirstFullWinStart = i + 1;

            pTapWin->WinCount++;
        }
    }
    pTapWin->UI = pTapWin->FirstFullWinEnd - pTapWin->PartialWin;

    SD_DEFAULT_PRINT("Partial Win = %d, First Full Win: start %d, end %d\n",
        pTapWin->PartialWin,
        pTapWin->FirstFullWinStart,
        pTapWin->FirstFullWinEnd);

    if (pTapValueStatus)
        NvOsFree(pTapValueStatus);

    return NvSuccess;
}

static NvError T1xxSdioExecuteHighFrequencyTuning(NvDdkSdioDeviceHandle hSdio)
{
    NvError e;
    NvU32 MilliVolts;
    NvS32 A;
    NvS32 a1 = 0;
    NvS32 b1 = 0;
    NvS32 a2 = 0;
    NvS32 b2 = 0;
    NvS32 BestTapValue;
    NvS32 FullWinQuality;
    NvS32 PartialWinQuality;
    NvS32 PartialWinTapValue;
    NvS32 FullWinTapValue;
    TuningWindow TuningWinAt1250mV;
    TuningWindow TuningWinAt1100mV;
    const NvOdmPeripheralConnectivity *pPeripheralConn = NULL;

    SD_DEBUG_PRINT("%s: entry\n", __func__);
    NvOsMemset(&TuningWinAt1250mV, 0, sizeof(TuningWindow));
    NvOsMemset(&TuningWinAt1100mV, 0, sizeof(TuningWindow));

    // Get the current Vdd Core voltage
    pPeripheralConn = NvOdmPeripheralGetGuid(NV_VDD_CORE_ODM_ID);
    if (pPeripheralConn == NULL)
    {
        e = NvError_NotInitialized;
        goto fail;
    }
    NvRmPmuGetVoltage(hSdio->hRm, pPeripheralConn->AddressList[0].Address,
        &MilliVolts);

    // Find Partial and First Full Windows at 1250 mV
    NvRmPmuSetVoltage(hSdio->hRm, pPeripheralConn->AddressList[0].Address,
                          1250, NULL);
    SD_DEFAULT_PRINT("Tuning window data at 1.25 V\n");
    NV_CHECK_ERROR_CLEANUP(FindTapValueWindows(hSdio, &TuningWinAt1250mV));

    // Find Partial and First Full Windows at 1100 mV
    NvRmPmuSetVoltage(hSdio->hRm, pPeripheralConn->AddressList[0].Address,
                          1100, NULL);
    SD_DEFAULT_PRINT("Tuning window data at 1.1 V\n");
    NV_CHECK_ERROR_CLEANUP(FindTapValueWindows(hSdio, &TuningWinAt1100mV));

    // Set the VDD core voltage back
    NvRmPmuSetVoltage(hSdio->hRm, pPeripheralConn->AddressList[0].Address,
                          MilliVolts, NULL);

    // Full Window Tap Value Calcualtion
    a1 = TuningWinAt1250mV.FirstFullWinStart +
             ((TuningWinAt1250mV.UI * 17) / 100);
    b1 = TuningWinAt1100mV.FirstFullWinEnd -
             ((TuningWinAt1100mV.UI * 17) / 100);
    FullWinTapValue = (a1 + b1) / 2;
    FullWinQuality = (b1 - a1) / 2;

    // Partial Window Tap Value Calculation
    A = TuningWinAt1250mV.PartialWin - (TuningWinAt1250mV.FirstFullWinEnd -
            TuningWinAt1250mV.FirstFullWinStart);

    a2 = A + ((TuningWinAt1250mV.UI * 17) / 100);
    b2 = TuningWinAt1100mV.PartialWin - ((TuningWinAt1100mV.UI * 17) / 100);
    PartialWinTapValue = (a2 + b2) / 2;

    if (PartialWinTapValue < 0)
        PartialWinTapValue = 0;
    PartialWinQuality = b2 - PartialWinTapValue;

    SD_DEBUG_PRINT("a1=%d, b1=%d, FullWindowTapValue=%d, FullWindowQuality=%d,"
                   "A=%d,\na2=%d, b2=%d, PartialWindowTapValue=%d,"
                   " PartialWindowQuality=%d\n",
                   a1, b1, FullWinTapValue, FullWinQuality, A, a2, b2,
                   PartialWinTapValue, PartialWinQuality);

    // Choose Best Tap Value either Partial Window or Full Window Tap Value
    if (FullWinQuality > PartialWinQuality)
    {
        BestTapValue = FullWinTapValue;
        SD_DEFAULT_PRINT("Full Window Chosen\n");
    }
    else
    {
        BestTapValue = PartialWinTapValue;
        SD_DEFAULT_PRINT("Partial Window Chosen\n");
    }

    // Set the best Tap Value
    SD_DEFAULT_PRINT("Best Tap Value = %d\n", BestTapValue);
    T1xxSdioSetTapValue(hSdio, BestTapValue);

    // run frequency tuning
    NV_CHECK_ERROR_CLEANUP(T1xxRunFrequencyTuning(hSdio));
    return NvSuccess;

fail:
    SD_ERROR_PRINT("%s: :Error 0x%x\n", __func__, e);
    return e;
}

static NvError T1xxSdioExecuteLowFrequencyTuning(NvDdkSdioDeviceHandle hSdio)
{
    NvError e;
    NvU32 MilliVolts;
    NvU32 BestTapValue;
    NvS32 a = 0;
    NvS32 b = 0;
    NvS32 c = 0;
    TuningWindow TuningWinAt1250mV;
    const NvOdmPeripheralConnectivity *pPeripheralConn = NULL;

    SD_DEBUG_PRINT("%s: entry\n", __func__);
    // find partial and first full Wins at 1250 mV
    NvOsMemset(&TuningWinAt1250mV, 0, sizeof(TuningWindow));

    pPeripheralConn = NvOdmPeripheralGetGuid(NV_VDD_CORE_ODM_ID);
    if (pPeripheralConn == NULL)
    {
        e = NvError_NotInitialized;
        goto fail;
    }
    NvRmPmuGetVoltage(hSdio->hRm, pPeripheralConn->AddressList[0].Address,
        &MilliVolts);

    // Find Partial and First Full Windows at 1100 mV
    NvRmPmuSetVoltage(hSdio->hRm, pPeripheralConn->AddressList[0].Address,
                          1250, NULL);
    NV_CHECK_ERROR_CLEANUP(FindTapValueWindows(hSdio, &TuningWinAt1250mV));

    // Set the Vdd Voltage back
    NvRmPmuSetVoltage(hSdio->hRm, pPeripheralConn->AddressList[0].Address,
                          MilliVolts, NULL);

    // Calculate the Best Tap Value
    a = TuningWinAt1250mV.PartialWin - (TuningWinAt1250mV.FirstFullWinEnd -
            TuningWinAt1250mV.FirstFullWinStart);

    b = a + ((TuningWinAt1250mV.UI * 82) / 800);
    c = TuningWinAt1250mV.FirstFullWinStart +
            ((TuningWinAt1250mV.UI * 82) / 800);

    if (TuningWinAt1250mV.PartialWin > ((TuningWinAt1250mV.UI * 22) / 100))
    {
        if (b > 0)
            BestTapValue = b;
        else
            BestTapValue = 0;
    }
    else
    {
        BestTapValue = c;
    }

    // Set the Best Tap Value
    T1xxSdioSetTapValue(hSdio, BestTapValue);

    // Run Frequency Tuning at Best Tap Value
    NV_CHECK_ERROR_CLEANUP(T1xxRunFrequencyTuning(hSdio));
    return NvSuccess;

fail:
    SD_ERROR_PRINT("%s: :Error 0x%x\n", __func__, e);
    return e;

}

static NvError T1xxSdioExecuteTuning(NvDdkSdioDeviceHandle hSdio)
{
    NvU32 CurrFrequency = MMC_HS200_TX_CLOCK_KHZ;

    if (CurrFrequency <= TUNING_LOW_FREQUENCY_KHZ)
        return T1xxSdioExecuteLowFrequencyTuning(hSdio);
    else
        return T1xxSdioExecuteHighFrequencyTuning(hSdio);
}

static void T1xxSdioDpdEnableStub(NvDdkSdioDeviceHandle hSdio)
{
    return;
}

static void T1xxSdioDpdDisableStub(NvDdkSdioDeviceHandle hSdio)
{
    return;
}
void T1xxSdioOpenHWInterface(NvDdkSdioDeviceHandle hSdio)
{
    hSdio->NvDdkSdioGetHigherCapabilities = (void *)T1xxSdioGetHigherCapabilities;
    hSdio->NvDdkSdioSetTapValue = (void *)T1xxSdioSetTapValue;
    hSdio->NvDdkSdioSetTrimValue = (void *)T1xxSdioSetTrimValue;
    hSdio->NvDdkSdioConfigureUhsMode = (void *)T1xxSdioSetUhsmode;
    hSdio->NvDdkSdioExecuteTuning = (void *)T1xxSdioExecuteTuning;
    hSdio->NvDdkSdioDpdEnable = (void*)T1xxSdioDpdEnableStub;
    hSdio->NvDdkSdioDpdDisable = (void*)T1xxSdioDpdDisableStub;
}
