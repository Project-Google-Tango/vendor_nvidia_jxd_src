/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
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
 *                 NvDDK T30 SDIO Driver Implementation</b>
 *
 * @b Description: Implementation of the NvDDK SDIO API.
 *
 */

#include "nvddk_sdio_private.h"
#include "t30/arsdmmc.h"
#include "t30/arapbpm.h"

static void T30SdioSetUhsmode(NvDdkSdioDeviceHandle hSdio, NvDdkSdioUhsMode Uhsmode)
{
    NvU32 val = 0;

    val = SDMMC_REGR(hSdio->pSdioVirtualAddress, AUTO_CMD12_ERR_STATUS);
    val = NV_FLD_SET_DRF_NUM(SDMMC, AUTO_CMD12_ERR_STATUS, UHS_MODE_SEL, Uhsmode, val);
    SDMMC_REGW(hSdio->pSdioVirtualAddress, AUTO_CMD12_ERR_STATUS, val);
}

static void T30SdioSetTapValue(NvDdkSdioDeviceHandle hSdio, NvU32 TapValue)
{
    NvU32 val = 0;

    NV_ASSERT(hSdio);
    NV_ASSERT(TapValue < 0xFF);

    val = SDMMC_REGR(hSdio->pSdioVirtualAddress, VENDOR_CLOCK_CNTRL);
    val = NV_FLD_SET_DRF_NUM(SDMMC, VENDOR_CLOCK_CNTRL, TAP_VAL, TapValue, val);
    val |= NV_DRF_DEF(SDMMC, VENDOR_CLOCK_CNTRL, INPUT_IO_CLK, INTERNAL);
    SDMMC_REGW(hSdio->pSdioVirtualAddress, VENDOR_CLOCK_CNTRL, val);
}

static void T30SdioSetTrimValue(NvDdkSdioDeviceHandle hSdio, NvU32 TrimValue)
{
   // dummy implementation for t30
}


static void T30SdioGetHigherCapabilities(NvDdkSdioDeviceHandle hSdio,
    NvDdkSdioHostCapabilities *pHostCaps)
{
    NvU32 capabilities = 0;

    capabilities = SDMMC_REGR(hSdio->pSdioVirtualAddress, CAPABILITIES_HIGHER);
    if (NV_DRF_VAL(SDMMC, CAPABILITIES_HIGHER, DDR50, capabilities))
        pHostCaps->IsDDR50modeSupported = NV_TRUE;
}

static void T30SdioDpdEnable(NvDdkSdioDeviceHandle hSdio)
{
    NvU32 PmcBaseAddress = hSdio->PmcBaseAddress;
    NvU32 Val = 0,DpdStatus = 0;

    PMC_REGW(PmcBaseAddress,DPD_SAMPLE,0x1);
    PMC_REGW(PmcBaseAddress,SEL_DPD_TIM,0x10);
    Val = PMC_REGR(PmcBaseAddress, IO_DPD2_REQ);
    Val = NV_FLD_SET_DRF_DEF(APBDEV_PMC, IO_DPD2_REQ, CODE, DPD_ON, Val);
    switch(hSdio->Instance)
    {
        case 0:
            Val = NV_FLD_SET_DRF_DEF(APBDEV_PMC, IO_DPD2_REQ, SDMMC1, ON, Val);
            break;
        case 2:
            Val = NV_FLD_SET_DRF_DEF(APBDEV_PMC, IO_DPD2_REQ, SDMMC3, ON, Val);
            break;
        case 3:
            Val = NV_FLD_SET_DRF_DEF(APBDEV_PMC, IO_DPD2_REQ, SDMMC4, ON, Val);
            break;
        default:
            break;
    }
    PMC_REGW(PmcBaseAddress, IO_DPD2_REQ,Val);

    NvOsWaitUS(1);

    Val = PMC_REGR(PmcBaseAddress, IO_DPD2_STATUS);
    switch(hSdio->Instance)
    {
        case 0:
            DpdStatus = NV_DRF_VAL(APBDEV_PMC,IO_DPD2_STATUS,SDMMC1,Val);
            break;
        case 2:
            DpdStatus = NV_DRF_VAL(APBDEV_PMC,IO_DPD2_STATUS,SDMMC3,Val);
            break;
        case 3:
            DpdStatus = NV_DRF_VAL(APBDEV_PMC,IO_DPD2_STATUS,SDMMC4,Val);
            break;
        default:
            break;
    }
    if(!DpdStatus)
        NvOsDebugPrintf("Enable DPD Mode failed\n");
    // Reset Sample Register for next sample operation
    PMC_REGW(PmcBaseAddress,DPD_SAMPLE,0x0);
    return;
}

static void T30SdioDpdDisable(NvDdkSdioDeviceHandle hSdio)
{

    NvU32 PmcBaseAddress = hSdio->PmcBaseAddress;
    NvU32 Val = 0,DpdStatus = 0;

    Val = PMC_REGR(PmcBaseAddress, IO_DPD2_REQ);
    Val = NV_FLD_SET_DRF_DEF(APBDEV_PMC, IO_DPD2_REQ, CODE, DPD_OFF, Val);
    switch(hSdio->Instance)
    {
        case 0:
            Val = NV_FLD_SET_DRF_DEF(APBDEV_PMC, IO_DPD2_REQ, SDMMC1, ON, Val);
            break;
        case 2:
            Val = NV_FLD_SET_DRF_DEF(APBDEV_PMC, IO_DPD2_REQ, SDMMC3, ON, Val);
            break;
        case 3:
            Val = NV_FLD_SET_DRF_DEF(APBDEV_PMC, IO_DPD2_REQ, SDMMC4, ON, Val);
            break;
        default:
            break;
    }
    PMC_REGW(PmcBaseAddress, IO_DPD2_REQ,Val);

    NvOsWaitUS(1);

    Val = PMC_REGR(PmcBaseAddress, IO_DPD2_STATUS);
    switch(hSdio->Instance)
    {
        case 0:
            DpdStatus = NV_DRF_VAL(APBDEV_PMC,IO_DPD2_STATUS,SDMMC1,Val);
            break;
        case 2:
            DpdStatus = NV_DRF_VAL(APBDEV_PMC,IO_DPD2_STATUS,SDMMC3,Val);
            break;
        case 3:
            DpdStatus = NV_DRF_VAL(APBDEV_PMC,IO_DPD2_STATUS,SDMMC4,Val);
            break;
        default:
            break;
    }
    if(DpdStatus)
        NvOsDebugPrintf("Disable DPD Mode failed\n");
    return;
}

void T30SdioOpenHWInterface(NvDdkSdioDeviceHandle hSdio)
{
    hSdio->NvDdkSdioGetHigherCapabilities = (void *)T30SdioGetHigherCapabilities;
    hSdio->NvDdkSdioSetTapValue = (void *)T30SdioSetTapValue;
    hSdio->NvDdkSdioSetTrimValue = (void *)T30SdioSetTrimValue;
    hSdio->NvDdkSdioConfigureUhsMode = (void *)T30SdioSetUhsmode;
    hSdio->NvDdkSdioDpdEnable = (void*)T30SdioDpdEnable;
    hSdio->NvDdkSdioDpdDisable = (void*)T30SdioDpdDisable;
}

