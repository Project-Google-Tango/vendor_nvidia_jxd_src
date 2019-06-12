/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_CHARGING_H
#define INCLUDED_CHARGING_H

#include "nvodm_pmu.h"

#ifdef LPM_BATTERY_CHARGING
/**
 * Defines charging function IDs
 * (also describes any additional parameters)
 */
typedef enum
{
    /// Required functions
    /**
     * Initialize charging interface (called once and first)
     *   (no additional parameters)
     */
    NvOdmChargingID_InitInterface,

    /**
     * Determine if a battery is connected
     *   NvBool *Connected (set to TRUE if connected, FALSE otherwise)
     */
    NvOdmChargingID_IsBatteryConnected,

    /**
     * Get the current charge level of the battery
     *   NvU16 *Charge (0..100%)
     */
    NvOdmChargingID_GetBatteryCharge,

    /**
     * Get the charging config data as specified the odm
     */
    NvOdmChargingID_GetChargingConfigData,

    /**
     * Determines if a charger is connected
     *   NvBool *Connected (set to TRUE if connected, FALSE otherwise)
     */
    NvOdmChargingID_IsChargerConnected,

    /**
     * Get the type of charger connected
     */
    NvOdmChargingID_GetChargerType,

    /**
     * Turns the charging current on
     *   (no other parameters)
     */
    NvOdmChargingID_StartCharging,

    /**
     * Turns the charging current off
     *   (no other parameters)
     */
    NvOdmChargingID_StopCharging,

    /**
     * Checks whether we're charging right now
     *   (no other parameters)
     */
    NvOdmChargingID_IsCharging,

    /// Optional functions

} NvOdmChargingID;

/**
 * @brief Perform a charging related function
 *
 * @param NvOdmChargingID Function to be performed
 * @param ...             additional parameters defined with function IDs above
 *
 * @return NV_TRUE if function successful, NV_FALSE otherwise.
 */
NvBool NvOdmChargingInterface(NvOdmChargingID fcn, ...);

#endif
#endif // INCLUDED_CHARGING_H

