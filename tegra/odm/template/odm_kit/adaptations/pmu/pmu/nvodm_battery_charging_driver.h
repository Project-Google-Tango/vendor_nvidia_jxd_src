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
  * @file: <b>NVIDIA Tegra ODM Kit: PMU-driver Interface</b>
  *
  * @b Description: Defines the typical interface for battery driver and some
  * common structure related to battery driver initialization.
  *
  */

#ifndef INCLUDED_NVODM_BATTERY_CHARGING_DRIVER_H
#define INCLUDED_NVODM_BATTERY_CHARGING_DRIVER_H

#include "nvodm_services.h"
#include "nvodm_pmu.h"

typedef void *NvOdmBatChargingDriverInfoHandle;

typedef struct OdmBatteryChargingPropsRec
{
    NvU32 MaxChargeCurrent_mA;
    NvU32 MaxChargeVoltage_mV;
} OdmBatteryChargingProps;

#if defined(__cplusplus)
extern "C"
{
#endif

/* Typical Interface for the battery drivers, battery driver needs to implement
 * following function with same set of argument.*/
typedef NvOdmBatChargingDriverInfoHandle (*BChargingDriverOpen)(NvOdmPmuDeviceHandle hDevice,
      OdmBatteryChargingProps *pBChargingProps);

typedef void (*BChargingDriverClose)(NvOdmBatChargingDriverInfoHandle hBChargingDriverInfo);

typedef NvBool
(*BChargingDriverSetCurrent)(
    NvOdmBatChargingDriverInfoHandle hBChargingDriverInfo,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType);

#if defined(__cplusplus)
}
#endif

/** @} */
#endif // INCLUDED_NVODM_BATTERY_CHARGING_DRIVER_H
