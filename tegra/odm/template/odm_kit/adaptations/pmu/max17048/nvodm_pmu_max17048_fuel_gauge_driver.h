/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
/**
 * @file: <b>NVIDIA Tegra ODM Kit: MAX17048-driver Interface</b>
 *
 * @b Description: Defines the interface for MAX17048 gas gauge driver.
 *
 */

#ifndef ODM_PMU_MAX17048_FUEL_GAUGE_DRIVER_H
#define ODM_PMU_MAX17048_FUEL_GAUGE_DRIVER_H

#include "nvodm_pmu.h"
#include "pmu/nvodm_fuel_gauge_driver.h"

#if defined(__cplusplus)
extern "C"
{
#endif


#define MAX17048_BATTERY_FULL       100
#define MAX17048_BATTERY_LOW        15
#define MAX17048_BATTERY_CRITICAL   5

struct max17048_battery_model {
    NvU8 rcomp;
    NvU8 soccheck_A;
    NvU8 soccheck_B;
    NvU8 bits;
    NvU8 alert_threshold;
    NvU8 one_percent_alerts;
    NvU8 alert_on_reset;
    NvU16 rcomp_seg;
    NvU16 hibernate;
    NvU16 vreset;
    NvU16 valert;
    NvU16 ocvtest;
};

NvOdmFuelGaugeDriverInfoHandle
Max17048FuelGaugeDriverOpen(NvOdmPmuDeviceHandle hDevice);

void
Max17048FuelGaugeDriverClose(void);

NvBool
Max17048FuelGaugeDriverReadSoc(NvU32 *pData, NvU32 *voltage);


#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_MAX17048_FUEL_GAUGE_DRIVER_H */
