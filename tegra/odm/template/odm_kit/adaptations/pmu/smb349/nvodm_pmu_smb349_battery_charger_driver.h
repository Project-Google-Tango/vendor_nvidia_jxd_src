/*
 * Copyright (c) 2012 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
/**
 * @file: <b>NVIDIA Tegra ODM Kit: SMB349-driver Interface</b>
 *
 * @b Description: Defines the interface for SMB349 charger driver.
 *
 */

#ifndef ODM_PMU_SMB349_BATTERY_CHARGING_DRIVER_H
#define ODM_PMU_SMB349_BATTERY_CHARGING_DRIVER_H

#include "pmu/nvodm_battery_charging_driver.h"

#if defined(__cplusplus)
extern "C"
{
#endif

NvOdmBatChargingDriverInfoHandle
Smb349BatChargingDriverOpen(
    NvOdmPmuDeviceHandle hDevice,
    OdmBatteryChargingProps *pBChargingProps);

void
Smb349BatChargingDriverClose(NvOdmBatChargingDriverInfoHandle hBChargingDriver);

NvBool
Smb349BatChargingDriverSetCurrent(
    NvOdmBatChargingDriverInfoHandle hBChargingDriverInfo,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType);


#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_SMB349_BATTERY_CHARGING_DRIVER_H */
