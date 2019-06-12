/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
/**
 * @file: <b>NVIDIA Tegra ODM Kit: CW2015-driver Interface</b>
 *
 * @b Description: Defines the interface for CW2015 gas gauge driver.
 *
 */

#ifndef ODM_PMU_CW2015_FUEL_GAUGE_DRIVER_H
#define ODM_PMU_CW2015_FUEL_GAUGE_DRIVER_H

#include "nvodm_pmu.h"
#include "pmu/nvodm_fuel_gauge_driver.h"

#if defined(__cplusplus)
extern "C"
{
#endif


#define CW2015_BATTERY_FULL       100
#define CW2015_BATTERY_LOW        15
#define CW2015_BATTERY_CRITICAL   5

NvOdmFuelGaugeDriverInfoHandle
CW2015FuelGaugeDriverOpen(NvOdmPmuDeviceHandle hDevice);

void
CW2015FuelGaugeDriverClose(void);

NvBool
CW2015FuelGaugeDriverReadSoc(NvU32 *pData, NvU32 *voltage);

NvBool
CW2015FuelGaugeConfigbatteryProfile(void);

#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_CW2015_FUEL_GAUGE_DRIVER_H */
