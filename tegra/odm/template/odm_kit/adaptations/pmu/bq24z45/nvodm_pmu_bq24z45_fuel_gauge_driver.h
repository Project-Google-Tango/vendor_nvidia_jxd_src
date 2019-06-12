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
 * @file: <b>NVIDIA Tegra ODM Kit: BQ24Z45-driver Interface</b>
 *
 * @b Description: Defines the interface for BQ24Z45 gas gauge driver.
 *
 */

#ifndef ODM_PMU_BQ24Z45_FUEL_GAUGE_DRIVER_H
#define ODM_PMU_BQ24Z45_FUEL_GAUGE_DRIVER_H

#include "nvodm_pmu.h"
#include "pmu/nvodm_fuel_gauge_driver.h"

#if defined(__cplusplus)
extern "C"
{
#endif


#define BQ24Z45_BATTERY_FULL       100
#define BQ24Z45_BATTERY_LOW        15
#define BQ24Z45_BATTERY_CRITICAL   5

NvOdmFuelGaugeDriverInfoHandle
Bq24z45FuelGaugeDriverOpen(NvOdmPmuDeviceHandle hDevice);

void
Bq24z45FuelGaugeDriverClose(void);

NvBool
Bq24z45FuelGaugeDriverReadSoc(NvU32 *pData, NvU32 *voltage);

NvBool Bq24z45FgReset(void);

#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_BQ24Z45_FUEL_GAUGE_DRIVER_H */
