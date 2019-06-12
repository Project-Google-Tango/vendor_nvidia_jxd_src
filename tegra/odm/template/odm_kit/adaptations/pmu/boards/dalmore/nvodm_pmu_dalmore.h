/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_NVODM_PMU_DALMORE_H
#define INCLUDED_NVODM_PMU_DALMORE_H

#include "nvodm_pmu.h"

#if defined(__cplusplus)
extern "C"
{
#endif

void
DalmorePmuGetCapabilities(
    NvU32 vddRail,
    NvOdmPmuVddRailCapabilities* pCapabilities);

NvBool
DalmorePmuGetVoltage(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 vddRail,
    NvU32* pMilliVolts);

NvBool
DalmorePmuSetVoltage(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 vddRail,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds);

NvBool DalmorePmuSetup(NvOdmPmuDeviceHandle hDevice);

void DalmorePmuRelease(NvOdmPmuDeviceHandle hDevice);

void DalmorePmuSuspend(void);

NvBool
DalmorePmuGetAcLineStatus(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuAcLineStatus *pStatus);

NvBool
DalmorePmuGetBatteryStatus(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvU8 *pStatus);

NvBool
DalmorePmuGetBatteryData(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvOdmPmuBatteryData *pData);

void
DalmorePmuGetBatteryFullLifeTime(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvU32 *pLifeTime);

void
DalmorePmuGetBatteryChemistry(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvOdmPmuBatteryChemistry *pChemistry);

NvBool DalmorePmuSetChargingCurrent(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType);

void DalmorePmuInterruptHandler(NvOdmPmuDeviceHandle  hDevice);

NvBool DalmorePmuReadRtc(NvOdmPmuDeviceHandle hDevice, NvU32 *Count);

NvBool DalmorePmuWriteRtc( NvOdmPmuDeviceHandle  hDevice, NvU32 Count);

NvBool DalmorePmuIsRtcInitialized( NvOdmPmuDeviceHandle  hDevice);

NvBool DalmorePmuPowerOff( NvOdmPmuDeviceHandle  hDevice);

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_NVODM_PMU_DALMORE_H */
