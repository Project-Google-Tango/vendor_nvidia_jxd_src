/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
/**
 * @file: <b>NVIDIA Tegra ODM Kit: LC709203F-driver Interface</b>
 *
 * @b Description: Defines the interface for LC709203F gas gauge driver.
 *
 */

#ifndef ODM_PMU_LC709203F_FUEL_GAUGE_DRIVER_H
#define ODM_PMU_LC709203F_FUEL_GAUGE_DRIVER_H

#include "nvodm_pmu.h"
#include "pmu/nvodm_fuel_gauge_driver.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#ifdef  READ_BATTERY_SOC
#define LC709203F_BATTERY_FULL       100
#define LC709203F_BATTERY_LOW        1
#define LC709203F_BATTERY_CRITICAL   0

#else
#define LC709203F_BATTERY_FULL       5000
#define LC709203F_BATTERY_LOW        3300
#define LC709203F_BATTERY_CRITICAL   3000
#endif
NvOdmFuelGaugeDriverInfoHandle
Lc709203fFuelGaugeDriverOpen(NvOdmPmuDeviceHandle hDevice);

void
Lc709203fFuelGaugeDriverClose(void);

NvBool
Lc709203fFuelGaugeDriverReadIcVersion(NvU32 *pVersion);

NvBool
Lc709203fFuelGaugeDriverReadSoc(NvU32 *pData, NvU32 *voltage);

NvBool Lc709203fFgReset(void);

#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_LC709203F_FUEL_GAUGE_DRIVER_H */
