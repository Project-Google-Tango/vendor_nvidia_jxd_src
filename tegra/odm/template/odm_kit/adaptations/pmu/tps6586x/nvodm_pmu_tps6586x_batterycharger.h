/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_TPS6586X_BATTERYCHARGER_HEADER
#define INCLUDED_TPS6586X_BATTERYCHARGER_HEADER



#include "nvodm_pmu_tps6586x.h"

/* the battery charger functions */
#if defined(__cplusplus)
extern "C"
{
#endif

/* Initliase all registers that related to battery charger */
NvBool 
Tps6586xBatteryChargerSetup(NvOdmPmuDeviceHandle hDevice);

/* check CBC main batt presence */
NvBool
Tps6586xBatteryChargerCBCMainBatt(NvOdmPmuDeviceHandle hDevice, NvBool *status);

/* check batt_ful status */
NvBool
Tps6586xBatteryChargerCBCBattFul(NvOdmPmuDeviceHandle hDevice, NvBool *status);

/* check main charger status */
NvBool
Tps6586xBatteryChargerMainChgPresent(NvOdmPmuDeviceHandle hDevice, NvBool *status);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_TPS6586X_BATTERYCHARGER_HEADER

