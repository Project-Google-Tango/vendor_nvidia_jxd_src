/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_MAX8907B_BATTERYCHARGER_HEADER
#define INCLUDED_MAX8907B_BATTERYCHARGER_HEADER

/* the battery charger functions */
#if defined(__cplusplus)
extern "C"
{
#endif

/* check main battery presence */
NvBool
Max8907bBatteryChargerMainBatt(
    NvOdmPmuDeviceHandle hDevice,
    NvBool *status);

/* check charger input voltage */
NvBool
Max8907bBatteryChargerOK(
    NvOdmPmuDeviceHandle hDevice,
    NvBool *status);

/* check charger enable status */
NvBool
Max8907bBatteryChargerEnabled(
    NvOdmPmuDeviceHandle hDevice,
    NvBool *status);

/* check main battery voltage status */
NvBool
Max8907bBatteryChargerMainBattFull(
    NvOdmPmuDeviceHandle hDevice,
    NvBool *status);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_MAX8907B_BATTERYCHARGER_HEADER

