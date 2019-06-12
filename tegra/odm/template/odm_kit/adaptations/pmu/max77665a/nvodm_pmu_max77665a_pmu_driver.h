/*
 * Copyright (c) 2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_MAX77665A_CORE_DRIVER_H
#define INCLUDED_MAX77665A_CORE_DRIVER_H

#include "nvodm_pmu.h"
#include "nvodm_query.h"
#include "pmu/nvodm_battery_charging_driver.h"

typedef struct Max77665aCoreDriverRec *Max77665aCoreDriverHandle;

#if defined(__cplusplus)
extern "C"
{
#endif

#define MAX77665_CHG_DTLS_01    0xb4
#define MAX77665_CHG_CNFG_00    0xb7
#define MAX77665_CHG_CNFG_02    0xb9
#define MAX77665_CHG_CNFG_04    0xbb
#define MAX77665_CHG_CNFG_06    0xbd
#define MAX77665_CHG_CNFG_09    0xc0

#define LEFT_MASK 0xf0

NvOdmBatChargingDriverInfoHandle
Max77665aDriverOpen(NvOdmPmuDeviceHandle hDevice);

void Max77665aDriverClose(NvOdmBatChargingDriverInfoHandle hBChargingDriver);

NvBool
Max77665aDisableBatteryCharging(NvOdmBatChargingDriverInfoHandle hBChargingDriver);

NvBool
Max77665aBatChargingDriverSetCurrent(
    NvOdmBatChargingDriverInfoHandle hBChargingDriverInfo,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType);

#if defined(__cplusplus)
}
#endif

#endif
