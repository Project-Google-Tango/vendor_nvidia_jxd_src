/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 /**
  * @file
  * <b>NVIDIA Tegra ODM Kit: Odm PMU driver based on AP GPIO</b>
  *
  * @b Description: Implements the AP GPIO based rail controls API.
  */

#include "nvodm_pmu.h"
#include "nvodm_pmu_apgpio.h"
#include "nvodm_services.h"
#include "nvodm_query_gpio.h"

#if (NV_DEBUG)
#define NVODMPMU_PRINTF(x)   NvOdmOsDebugPrintf x
#else
#define NVODMPMU_PRINTF(x)
#endif

typedef struct NvOdmPmuApGpioDriverRec {
    NvU32 PmuVddId;
    NvU32 GpioPort;
    NvU32 GpioPin;
    NvU32 UserCount;
    NvU32 TotalRails;
    NvU32 CurrentLevel;
    NvOdmServicesGpioHandle hOdmGpio;
    NvOdmGpioPinHandle hPinGpio;
    ApGpioInfo GpioInfo;
    NvOdmOsMutexHandle hMutex;
    NvOdmPmuDeviceHandle hPmuDevice;
    NvOdmServicesPmuHandle hPmuService;
} NvOdmPmuApGpioDriver;

static NvOdmPmuApGpioDriver *s_pApGpioDriver = NULL;
static void EnableRail(NvOdmPmuApGpioDriver *pApGpioDriver)
{
    NvU32 Value;

    pApGpioDriver->CurrentLevel = 1;
    if (pApGpioDriver->GpioInfo.Enable)
    {
        pApGpioDriver->GpioInfo.Enable(pApGpioDriver->hOdmGpio,
                                pApGpioDriver->hPinGpio,
                                pApGpioDriver->GpioPort,
                                pApGpioDriver->GpioPin);
        return;
    }
    Value = (pApGpioDriver->GpioInfo.IsActiveLow) ?
                NvOdmGpioPinActiveState_Low : NvOdmGpioPinActiveState_High;
    NvOdmGpioSetState(pApGpioDriver->hOdmGpio, pApGpioDriver->hPinGpio, Value);
}

static void DisableRail(NvOdmPmuApGpioDriver *pApGpioDriver)
{
    NvU32 Value;
    pApGpioDriver->CurrentLevel = 0;
    if (pApGpioDriver->GpioInfo.Disable)
    {
        pApGpioDriver->GpioInfo.Disable(pApGpioDriver->hOdmGpio,
                                pApGpioDriver->hPinGpio,
                                pApGpioDriver->GpioPort,
                                pApGpioDriver->GpioPin);
       return;
    }
    Value = (pApGpioDriver->GpioInfo.IsActiveLow) ?
                 NvOdmGpioPinActiveState_High : NvOdmGpioPinActiveState_Low;
    NvOdmGpioSetState(pApGpioDriver->hOdmGpio,pApGpioDriver->hPinGpio, Value);
}

NvOdmPmuApGpioDriverHandle OdmPmuApGpioOpen(NvOdmPmuDeviceHandle hPmuDevice,
                ApGpioInfo *pInitInfo, int NrOfRails)
{
    NvOdmPmuApGpioDriver *pApGpioDriver;
    int i;
    NvOdmServicesGpioHandle hOdmGpio;
    NvOdmOsMutexHandle hMutex;
    NvOdmServicesPmuHandle hPmuService;

    NV_ASSERT(pInitInfo);
    NV_ASSERT(NrOfRails);

    pApGpioDriver = NvOdmOsAlloc(NrOfRails * sizeof(NvOdmPmuApGpioDriver));
    if (!pApGpioDriver)
    {
        NVODMPMU_PRINTF(("%s:Error in malloc\n", __func__));
        return NULL;
    }

    NvOdmOsMemset(pApGpioDriver, 0, NrOfRails * sizeof(NvOdmPmuApGpioDriver));

    hPmuService = NvOdmServicesPmuOpen();
    if (!hPmuService)
    {
        NVODMPMU_PRINTF(("%s:Error in Opening pmu service\n", __func__));
        goto FreeMalloc;
    }

    hMutex = NvOdmOsMutexCreate();
    if (!hMutex)
    {
        NVODMPMU_PRINTF(("%s:Error in creating mutex\n", __func__));
        goto FreePmuService;
    }

    hOdmGpio = NvOdmGpioOpen();
    if (!hOdmGpio)
    {
        NVODMPMU_PRINTF(("%s:Error in opening gpio handle\n", __func__));
        goto FreeMutex;
    }

    for (i = 0; i < NrOfRails; ++i)
    {
        NvOdmOsMemcpy(&pApGpioDriver[i].GpioInfo, &pInitInfo[i], sizeof(ApGpioInfo));
        pApGpioDriver[i].UserCount = 0;
        pApGpioDriver[i].PmuVddId = pInitInfo[i].RailId;
        pApGpioDriver[i].GpioPort = pInitInfo[i].GpioPort;
        pApGpioDriver[i].GpioPin = pInitInfo[i].GpioPin;
        pApGpioDriver[i].TotalRails = NrOfRails;
        pApGpioDriver[i].hPmuDevice = hPmuDevice;
        pApGpioDriver[i].hPmuService = hPmuService;

        pApGpioDriver[i].hOdmGpio = hOdmGpio;
        pApGpioDriver[i].hMutex = hMutex;

        /* index 0 is not a valid entry */
        if (i == 0)
             continue;
        pApGpioDriver[i].hPinGpio = NvOdmGpioAcquirePinHandle(hOdmGpio,
                        pApGpioDriver[i].GpioPort, pApGpioDriver[i].GpioPin);
        if (!pApGpioDriver[i].hPinGpio)
        {
            NVODMPMU_PRINTF(("%s:Error in acquiring gpio pin handle\n", __func__));
            goto FailPinHandle;
        }
    }

    for (i = 1; i < NrOfRails; ++i)
    {
        if (pApGpioDriver[i].GpioInfo.IsInitEnable)
            EnableRail(&pApGpioDriver[i]);
        else
            DisableRail(&pApGpioDriver[i]);

        if (!(pApGpioDriver[i].GpioInfo.Enable ||
                    pApGpioDriver[i].GpioInfo.Disable))
            NvOdmGpioConfig(hOdmGpio, pApGpioDriver[i].hPinGpio,
                    NvOdmGpioPinMode_Output);

    }

    s_pApGpioDriver = pApGpioDriver;
    return pApGpioDriver;

FailPinHandle:
    while(--i > 0)
        NvOdmGpioReleasePinHandle(hOdmGpio, pApGpioDriver[i].hPinGpio);
    NvOdmGpioClose(hOdmGpio);

FreeMutex:
    NvOdmOsMutexDestroy(hMutex);

FreePmuService:
    NvOdmServicesPmuClose(hPmuService);

FreeMalloc:
    NvOdmOsFree(pApGpioDriver);
    return NULL;
}

void OdmPmuApGpioClose(NvOdmPmuApGpioDriverHandle hApGpioDriver)
{
    int NrOfRails;
    int i;
    NvOdmPmuApGpioDriver *pApGpioDriver = hApGpioDriver;

    NrOfRails = pApGpioDriver[0].TotalRails;
    for (i = 1; i< NrOfRails; ++i)
    {
        NvOdmGpioReleasePinHandle(pApGpioDriver[i].hOdmGpio,
                                pApGpioDriver[i].hPinGpio);
    }
    NvOdmGpioClose(pApGpioDriver[0].hOdmGpio);
    NvOdmOsMutexDestroy(pApGpioDriver[0].hMutex);
    NvOdmServicesPmuClose(pApGpioDriver[0].hPmuService);
    NvOdmOsFree(pApGpioDriver);
    s_pApGpioDriver = NULL;
}

void
OdmPmuApGpioGetCapabilities(
    NvOdmPmuApGpioDriverHandle hApGpioDriver,
    NvU32 VddRail,
    NvOdmPmuVddRailCapabilities* pCapabilities)
{
    NvOdmPmuApGpioDriver *pApGpioDriver = hApGpioDriver;

    if (VddRail > pApGpioDriver[0].TotalRails)
    {
        NvOdmOsPrintf("%s(): The VddRail %d is more than registered "
                        "rails\n", __func__, VddRail);
        NvOdmOsMemset(pCapabilities, 0, sizeof(NvOdmPmuVddRailCapabilities));
        return;
    }

    pCapabilities->OdmProtected = pApGpioDriver[VddRail].GpioInfo.IsOdmProtected;
    pCapabilities->MinMilliVolts = pApGpioDriver[VddRail].GpioInfo.OffValue_mV;
    pCapabilities->MaxMilliVolts = pApGpioDriver[VddRail].GpioInfo.OnValue_mV;
    pCapabilities->StepMilliVolts = pApGpioDriver[VddRail].GpioInfo.OnValue_mV -
                                pApGpioDriver[VddRail].GpioInfo.OffValue_mV;
    pCapabilities->requestMilliVolts = pApGpioDriver[VddRail].GpioInfo.OnValue_mV;
}

NvBool
OdmPmuApGpioGetVoltage(
    NvOdmPmuApGpioDriverHandle hApGpioDriver,
    NvU32 VddRail,
    NvU32* pMilliVolts)
{
    NvOdmPmuApGpioDriver *pApGpioDriver = hApGpioDriver;

    if (VddRail > pApGpioDriver[0].TotalRails)
    {
        NvOdmOsPrintf("%s(): The VddRail %d is more than registered "
                        "rails\n", __func__, VddRail);
        return NV_FALSE;
    }

    if (pApGpioDriver[VddRail].CurrentLevel)
        *pMilliVolts = pApGpioDriver[VddRail].GpioInfo.OnValue_mV;
    else
        *pMilliVolts = pApGpioDriver[VddRail].GpioInfo.OffValue_mV;
    return NV_TRUE;
}

NvBool
OdmPmuApGpioSetVoltage(
    NvOdmPmuApGpioDriverHandle hApGpioDriver,
    NvU32 VddRail,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds)
{
    NvOdmPmuApGpioDriver *pApGpioDriver = hApGpioDriver;

    if (VddRail > pApGpioDriver[0].TotalRails)
    {
        NvOdmOsPrintf("%s(): The VddRail %d is more than registered "
                        "rails\n", __func__, VddRail);
        return NV_FALSE;
    }

    if (pApGpioDriver[VddRail].GpioInfo.IsOdmProtected)
        return NV_TRUE;

    *pSettleMicroSeconds = 0;
    NvOdmOsMutexLock(pApGpioDriver[VddRail].hMutex);
    if (MilliVolts == NVODM_VOLTAGE_OFF)
    {
        if (!pApGpioDriver[VddRail].UserCount)
            goto End;
        pApGpioDriver[VddRail].UserCount--;
        if (!pApGpioDriver[VddRail].UserCount)
        {
            NvOdmServicesPmuSetSocRailPowerState(
                pApGpioDriver[VddRail].hPmuService,
                pApGpioDriver[VddRail].GpioInfo.SystemRailId, NV_FALSE);
            DisableRail(&pApGpioDriver[VddRail]);
            *pSettleMicroSeconds = pApGpioDriver[VddRail].GpioInfo.Delay_uS;
        }
        goto End;
    }

    if (!pApGpioDriver[VddRail].UserCount)
    {
        NvOdmServicesPmuSetSocRailPowerState(
            pApGpioDriver[VddRail].hPmuService,
            pApGpioDriver[VddRail].GpioInfo.SystemRailId, NV_TRUE);
        EnableRail(&pApGpioDriver[VddRail]);
        *pSettleMicroSeconds = pApGpioDriver[VddRail].GpioInfo.Delay_uS;
    }
    pApGpioDriver[VddRail].UserCount++;

End:
    NvOdmOsMutexUnlock(pApGpioDriver[VddRail].hMutex);
    return NV_TRUE;
}
