/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_NVODM_PMU_Ardbeg_H
#define INCLUDED_NVODM_PMU_Ardbeg_H

#include "nvodm_pmu.h"

#if defined(__cplusplus)
extern "C"
{
#endif

void
ArdbegPmuGetCapabilities(
    NvU32 vddRail,
    NvOdmPmuVddRailCapabilities* pCapabilities);

NvBool
ArdbegPmuGetVoltage(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 vddRail,
    NvU32* pMilliVolts);

NvBool
ArdbegPmuSetVoltage(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 vddRail,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds);

NvBool ArdbegPmuSetup(NvOdmPmuDeviceHandle hDevice);

void ArdbegPmuRelease(NvOdmPmuDeviceHandle hDevice);

NvBool
ArdbegPmuGetAcLineStatus(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuAcLineStatus *pStatus);

NvBool
ArdbegPmuGetBatteryStatus(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvU8 *pStatus);

NvBool
ArdbegPmuGetBatteryData(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvOdmPmuBatteryData *pData);

void
ArdbegPmuGetBatteryFullLifeTime(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvU32 *pLifeTime);

void
ArdbegPmuGetBatteryChemistry(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvOdmPmuBatteryChemistry *pChemistry);

NvBool ArdbegPmuSetChargingCurrent(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType);

void ArdbegPmuInterruptHandler(NvOdmPmuDeviceHandle  hDevice);

NvBool ArdbegPmuReadRtc(NvOdmPmuDeviceHandle hDevice, NvU32 *Count);

NvBool ArdbegPmuWriteRtc( NvOdmPmuDeviceHandle  hDevice, NvU32 Count);

NvBool ArdbegPmuIsRtcInitialized( NvOdmPmuDeviceHandle  hDevice);

NvBool ArdbegPmuPowerOff( NvOdmPmuDeviceHandle  hDevice);

NvBool ArdbegPmuClearAlarm(NvOdmPmuDeviceHandle hDevice);

void ArdbegPmuDisableInterrupts(NvOdmPmuDeviceHandle  hDevice);

void ArdbegPmuEnableInterrupts(NvOdmPmuDeviceHandle  hDevice);

NvBool ArdbegPmuGetDate(NvOdmPmuDeviceHandle  hDevice, DateTime* time);

NvBool ArdbegPmuSetAlarm(NvOdmPmuDeviceHandle  hDevice, DateTime time);

NvU8 ArdbegPmuGetPowerOnSource(NvOdmPmuDeviceHandle hPmuDevice);

NvBool ArdbegPmuGetBatteryCapacity(NvOdmPmuDeviceHandle hDevice, NvU32* BatCapacity);

NvBool ArdbegPmuGetBatteryAdcVal(NvOdmPmuDeviceHandle hPmuDevice, NvU32 AdcChannelId, NvU32* BatAdcVal);

NvBool ArdbegPmuReadRstReason(NvOdmPmuDeviceHandle hDevice, NvOdmPmuResetReason *rst_reason);

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_NVODM_PMU_Ardbeg_H */
