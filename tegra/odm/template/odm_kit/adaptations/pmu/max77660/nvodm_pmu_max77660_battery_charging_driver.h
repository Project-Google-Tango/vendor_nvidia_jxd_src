/*
 * Copyright (c) 2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_MAX77660_BATTERY_CHARGING_DRIVER_H
#define INCLUDED_MAX77660_BATTERY_CHARGING_DRIVER_H

#include "nvodm_pmu.h"
#include "nvodm_query.h"
#include "pmu/nvodm_battery_charging_driver.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct Max77660CoreDriverRec *Max77660CoreDriverHandle;

#define MAX77660_GLBLCNFG1 0x1D
#define MAX77660_DETAILS1 0x60
#define MAX77660_CHGCTRL1 0x64
#define MAX77660_FCHGCRNT 0x65
#define MAX77660_BATREGCTRL 0x67
#define MAX77660_MBATREGMAX 0x6E
#define MAX77660_DCCRNT 0x68
#define MAX77660_CHGCTRL2 0x6B


NvOdmBatChargingDriverInfoHandle
Max77660BatChargingDriverOpen(
    NvOdmPmuDeviceHandle hDevice);

void Max77660BatChargingDriverClose(NvOdmBatChargingDriverInfoHandle hBChargingDriver);

NvBool
Max77660BatChargingDriverSetCurrent(
    NvOdmBatChargingDriverInfoHandle hBChargingDriverInfo,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType);

#if defined(__cplusplus)
}
#endif

#endif

