/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
/**
 * @file: <b>NVIDIA Tegra ODM Kit: PMU TPS8003X-driver Interface</b>
 *
 * @b Description: Defines the interface for pmu TPS8003X battery charging driver.
 *
 */

#ifndef ODM_PMU_TPS8003X_BATTERY_CHARGING_DRIVER_H
#define ODM_PMU_TPS8003X_BATTERY_CHARGING_DRIVER_H

#include "pmu/nvodm_battery_charging_driver.h"

#if defined(__cplusplus)
extern "C"
{
#endif

NvOdmBatChargingDriverInfoHandle
Tps8003xBatChargingDriverOpen(
    NvOdmPmuDeviceHandle hDevice,
    OdmBatteryChargingProps *pBChargingProps);

void Tps8003xBatChargingDriverClose(NvOdmBatChargingDriverInfoHandle hBChargingDriver);

NvBool
Tps8003xBatChargingDriverSetCurrent(
    NvOdmBatChargingDriverInfoHandle hBChargingDriverInfo,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType);

NvBool
Tps8003xBatChargingDriverUpdateWatchDog(
NvOdmBatChargingDriverInfoHandle hBChargingDriverInfo,
NvU32 value);


#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_TPS8003X_BATTERY_CHARGING_DRIVER_H */
