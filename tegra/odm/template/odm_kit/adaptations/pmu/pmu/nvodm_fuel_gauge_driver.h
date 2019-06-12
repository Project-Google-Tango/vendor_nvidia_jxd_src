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
  * @file: <b>NVIDIA Tegra ODM Kit: PMU-driver Interface</b>
  *
  * @b Description: Defines the typical interface for Adc driver and some
  * common structure related to Adc driver initialization.
  *
  */

#ifndef INCLUDED_NVODM_FUEL_GAUGE_DRIVER_H
#define INCLUDED_NVODM_FUEL_GAUGE_DRIVER_H

typedef void *NvOdmFuelGaugeDriverInfoHandle;


#if defined(__cplusplus)
extern "C"
{
#endif

/* Typical Interface for the battery drivers, battery driver needs to implement
 * following function with same set of argument.*/
typedef NvOdmFuelGaugeDriverInfoHandle (*FuelGaugeDriverOpen)(NvOdmPmuDeviceHandle hDevice);

typedef void (*FuelGaugeDriverClose)(NvOdmFuelGaugeDriverInfoHandle hAdcDriverInfo);

#if defined(__cplusplus)
}
#endif

/** @} */
#endif // INCLUDED_NVODM_FUEL_GAUGE_DRIVER_H

