/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_NVODM_PMU_Loki_H
#define INCLUDED_NVODM_PMU_Loki_H

#include "nvodm_query_discovery.h"
#include "nvodm_pmu.h"

#if defined(__cplusplus)
extern "C"
{
#endif

void
LokiPmuGetCapabilities(
    NvU32 vddRail,
    NvOdmPmuVddRailCapabilities* pCapabilities);

NvBool
LokiPmuGetVoltage(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 vddRail,
    NvU32* pMilliVolts);

NvBool
LokiPmuSetVoltage(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 vddRail,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds);

NvBool LokiPmuSetup(NvOdmPmuDeviceHandle hDevice);

void LokiPmuRelease(NvOdmPmuDeviceHandle hDevice);

NvBool
LokiPmuGetAcLineStatus(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuAcLineStatus *pStatus);

NvBool
LokiPmuGetBatteryStatus(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvU8 *pStatus);

NvBool
LokiPmuGetBatteryData(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvOdmPmuBatteryData *pData);

void
LokiPmuGetBatteryFullLifeTime(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvU32 *pLifeTime);

void
LokiPmuGetBatteryChemistry(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvOdmPmuBatteryChemistry *pChemistry);

NvBool LokiPmuSetChargingCurrent(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType);

void
*LokiPmuBatterySetup(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmBoardInfo *pProcBoard,
    NvOdmBoardInfo *pPmuBoard);

void LokiPmuInterruptHandler(NvOdmPmuDeviceHandle  hDevice);

NvBool LokiPmuReadRtc(NvOdmPmuDeviceHandle hDevice, NvU32 *Count);

NvBool LokiPmuWriteRtc( NvOdmPmuDeviceHandle  hDevice, NvU32 Count);

NvBool LokiPmuIsRtcInitialized( NvOdmPmuDeviceHandle  hDevice);

NvBool LokiPmuPowerOff( NvOdmPmuDeviceHandle  hDevice);

NvBool LokiPmuClearAlarm(NvOdmPmuDeviceHandle hDevice);

void LokiPmuDisableInterrupts(NvOdmPmuDeviceHandle  hDevice);

void LokiPmuEnableInterrupts(NvOdmPmuDeviceHandle  hDevice);

NvBool LokiPmuGetDate(NvOdmPmuDeviceHandle  hDevice, DateTime* time);

NvBool LokiPmuSetAlarm(NvOdmPmuDeviceHandle  hDevice, DateTime time);

NvU8 LokiPmuGetPowerOnSource(NvOdmPmuDeviceHandle hPmuDevice);


#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_NVODM_PMU_Loki_H */
