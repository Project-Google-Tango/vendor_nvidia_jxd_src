/*
 * Copyright (c) 2010 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_services.h"
#include "nvcommon.h"
#include "nvassert.h"
#include "nvodm_query.h"
#include "nvodm_query_gpio.h"
#include "nvodm_query_discovery.h"
#include "nvrm_pmu.h"


typedef struct NvOdmServicesPmuRec
{
    NvRmDeviceHandle hRmDev;
} NvOdmServicesPmu;

NvOdmServicesPmuHandle NvOdmServicesPmuOpen(void)
{
    static NvOdmServicesPmu OdmServicesPmu;

    return &OdmServicesPmu;
}

void NvOdmServicesPmuClose(NvOdmServicesPmuHandle handle)
{
}

void NvOdmServicesPmuGetCapabilities(
        NvOdmServicesPmuHandle handle,
        NvU32 vddId,
        NvOdmServicesPmuVddRailCapabilities *pCapabilities )
{
    NV_ASSERT(! "not implemented");
}

void NvOdmServicesPmuGetVoltage(
        NvOdmServicesPmuHandle handle,
        NvU32 vddId,
        NvU32 * pMilliVolts )
{
    NV_ASSERT(! "not implemented");
}

void NvOdmServicesPmuSetVoltage(
        NvOdmServicesPmuHandle handle,
        NvU32 vddId,
        NvU32 MilliVolts,
        NvU32 * pSettleMicroSeconds )
{
    NV_ASSERT(! "not implemented");
}

void NvOdmServicesPmuSetSocRailPowerState(
        NvOdmServicesPmuHandle handle,
        NvU32 vddId,
        NvBool Enable )
{
    NV_ASSERT(! "not implemented");
}

NvBool
NvOdmServicesPmuGetBatteryStatus(
    NvOdmServicesPmuHandle handle,
    NvOdmServicesPmuBatteryInstance batteryInst,
    NvU8 * pStatus)
{
    NV_ASSERT(! "not implemented");
    return NV_FALSE;
}

NvBool
NvOdmServicesPmuGetBatteryData(
    NvOdmServicesPmuHandle handle,
    NvOdmServicesPmuBatteryInstance batteryInst,
    NvOdmServicesPmuBatteryData * pData)
{
    NV_ASSERT(! "not implemented");
    return NV_FALSE;
}

void
NvOdmServicesPmuGetBatteryFullLifeTime(
    NvOdmServicesPmuHandle handle,
    NvOdmServicesPmuBatteryInstance batteryInst,
    NvU32 * pLifeTime)
{
    NV_ASSERT(! "not implemented");
}

void
NvOdmServicesPmuGetBatteryChemistry(
    NvOdmServicesPmuHandle handle,
    NvOdmServicesPmuBatteryInstance batteryInst,
    NvOdmServicesPmuBatteryChemistry * pChemistry)
{
    NV_ASSERT(! "not implemented");
}

void
NvOdmServicesPmuSetChargingCurrentLimit(
    NvOdmServicesPmuHandle handle,
    NvOdmServicesPmuChargingPath ChargingPath,
    NvU32 ChargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType)
{
    NV_ASSERT(! "not implemented");
}
