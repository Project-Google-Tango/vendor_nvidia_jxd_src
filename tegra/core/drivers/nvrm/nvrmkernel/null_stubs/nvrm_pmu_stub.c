
#define NV_IDL_IS_STUB

/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvrm_pmu.h"

NvBool NvRmPmuWriteRtc(NvRmDeviceHandle hRmDevice, NvU32 Count)
{
    return NV_FALSE;
}

NvBool NvRmPmuReadRtc(NvRmDeviceHandle hRmDevice, NvU32 *pCount)
{
    if (pCount)
        *pCount = 0;
    return NV_FALSE;
}

void NvRmPmuGetBatteryChemistry(
    NvRmDeviceHandle hRmDevice,
    NvRmPmuBatteryInstance batteryInst,
    NvRmPmuBatteryChemistry *pChemistry)
{
    if (pChemistry)
        *pChemistry = NvRmPmuBatteryChemistry_LION;
}

void NvRmPmuGetBatteryFullLifeTime(
    NvRmDeviceHandle hRmDevice,
    NvRmPmuBatteryInstance batteryInst,
    NvU32 *pLifeTime)
{
    if (pLifeTime)
        *pLifeTime = NV_WAIT_INFINITE;
}

NvBool NvRmPmuGetBatteryData(
    NvRmDeviceHandle hRmDevice,
    NvRmPmuBatteryInstance batteryInst,
    NvRmPmuBatteryData *pData)
{
    return NV_FALSE;
}

NvBool NvRmPmuGetBatteryStatus(
    NvRmDeviceHandle hRmDevice,
    NvRmPmuBatteryInstance batteryInst,
    NvU8 *pStatus )
{
    return NV_FALSE;
}

void NvRmPmuSetChargingCurrentLimit(
    NvRmDeviceHandle hRmDevice,
    NvRmPmuChargingPath ChargingPath,
    NvU32 ChargingCurrentLimitMa,
    NvU32 ChargerType)
{
}

void NvRmPmuSetSocRailPowerState(
    NvRmDeviceHandle hDevice,
    NvU32 vddId,
    NvBool Enable)
{
}

void NvRmPmuSetVoltage(
    NvRmDeviceHandle hDevice,
    NvU32 vddId,
    NvU32 MilliVolts,
    NvU32 *pSettleMicroSeconds )
{
    if (pSettleMicroSeconds)
        *pSettleMicroSeconds = 0;
}

void NvRmPmuGetVoltage(
    NvRmDeviceHandle hDevice,
    NvU32 vddId,
    NvU32 *pMilliVolts)
{
    if (pMilliVolts)
        *pMilliVolts = 1200;
}

void NvRmPmuGetCapabilities(
    NvRmDeviceHandle hDevice,
    NvU32 vddId,
    NvRmPmuVddRailCapabilities *pCapabilities)
{
    const NvRmPmuVddRailCapabilities def_cap =
    {
        .RmProtected = NV_TRUE,
        .MinMilliVolts = 1200,
        .MaxMilliVolts = 1200,
        .StepMilliVolts = 0,
        .requestMilliVolts = 1200,
    };

    *pCapabilities = def_cap;
}
