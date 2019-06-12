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
 * @file: <b>NVIDIA Tegra ODM Kit: BQ27441-driver Interface</b>
 *
 * @b Description: Defines the interface for BQ27441 gas gauge driver.
 *
 */

#ifndef ODM_PMU_BQ27441_FUEL_GAUGE_DRIVER_H
#define ODM_PMU_BQ27441_FUEL_GAUGE_DRIVER_H

#include "nvodm_pmu.h"
#include "pmu/nvodm_fuel_gauge_driver.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#ifdef  READ_BATTERY_SOC
#define BQ27441_BATTERY_FULL       100
#define BQ27441_BATTERY_LOW        1
#define BQ27441_BATTERY_CRITICAL   0

#else
#define BQ27441_BATTERY_FULL       5000
#define BQ27441_BATTERY_LOW        3300
#define BQ27441_BATTERY_CRITICAL   3000
#endif
NvOdmFuelGaugeDriverInfoHandle
Bq27441FuelGaugeDriverOpen(NvOdmPmuDeviceHandle hDevice);

void
Bq27441FuelGaugeDriverClose(void);

NvBool
Bq27441FuelGaugeDriverReadSoc(NvU32 *pData, NvU32 *voltage);

NvBool Bq27441FgReset(void);

#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_BQ27441_FUEL_GAUGE_DRIVER_H */
