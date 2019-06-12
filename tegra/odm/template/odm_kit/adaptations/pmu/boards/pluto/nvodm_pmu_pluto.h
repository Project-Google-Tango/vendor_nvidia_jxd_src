/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_NVODM_PMU_PLUTO_H
#define INCLUDED_NVODM_PMU_PLUTO_H

#include "nvodm_pmu.h"
#include "nvodm_query_discovery.h"

#if defined(__cplusplus)
extern "C"
{
#endif

void
PlutoPmuGetCapabilities(
    NvU32 vddRail,
    NvOdmPmuVddRailCapabilities* pCapabilities);

NvBool
PlutoPmuGetVoltage(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 vddRail,
    NvU32* pMilliVolts);

NvBool
PlutoPmuSetVoltage(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 vddRail,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds);

NvBool
PlutoPmuReadRstReason(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuResetReason *rst_reason);

NvBool PlutoPmuSetup(NvOdmPmuDeviceHandle hDevice);

void PlutoPmuRelease(NvOdmPmuDeviceHandle hDevice);

void PlutoPmuSuspend(void);

NvBool
PlutoPmuGetAcLineStatus(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuAcLineStatus *pStatus);

NvBool
PlutoPmuGetBatteryStatus(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvU8 *pStatus);

NvBool
PlutoPmuGetBatteryData(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvOdmPmuBatteryData *pData);

void
PlutoPmuGetBatteryFullLifeTime(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvU32 *pLifeTime);

void
PlutoPmuGetBatteryChemistry(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvOdmPmuBatteryChemistry *pChemistry);

NvBool PlutoPmuSetChargingCurrent(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType);

void
*PlutoPmuBatterySetup(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmBoardInfo *pProcBoard,
    NvOdmBoardInfo *pPmuBoard);

void PlutoPmuBatteryRelease(void);

void PlutoPmuInterruptHandler(NvOdmPmuDeviceHandle  hDevice);

NvBool PlutoPmuReadRtc(NvOdmPmuDeviceHandle hDevice, NvU32 *Count);

NvBool PlutoPmuWriteRtc( NvOdmPmuDeviceHandle  hDevice, NvU32 Count);

NvBool PlutoPmuIsRtcInitialized( NvOdmPmuDeviceHandle  hDevice);

NvBool PlutoPmuPowerOff( NvOdmPmuDeviceHandle  hDevice);

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_NVODM_PMU_PLUTO_H */
