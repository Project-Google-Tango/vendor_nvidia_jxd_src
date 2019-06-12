/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "max8907b.h"
#include "max8907b_batterycharger.h"
#include "max8907b_reg.h"
#include "max8907b_i2c.h"

NvBool
Max8907bBatteryChargerMainBatt(
    NvOdmPmuDeviceHandle hDevice,
    NvBool *status)
{
    NvU8 data = 0;
    
    if(!Max8907bI2cRead8(hDevice, MAX8907B_CHG_STAT, &data))
        return NV_FALSE;

    data = (data >> MAX8907B_CHG_STAT_MBDET_SHIFT) & MAX8907B_CHG_STAT_MBDET_MASK;
    *status = (data == 0 ? NV_TRUE : NV_FALSE );    // MBDET low (0) = present
    return NV_TRUE;
}

NvBool
Max8907bBatteryChargerOK(
    NvOdmPmuDeviceHandle hDevice,
    NvBool *status)
{
    NvU8 data = 0;

    if(!Max8907bI2cRead8(hDevice, MAX8907B_CHG_STAT, &data))
        return NV_FALSE;

    data = (data >> MAX8907B_CHG_STAT_VCHG_OK_SHIFT) & MAX8907B_CHG_STAT_VCHG_OK_MASK;
    *status = (data == 0 ? NV_FALSE : NV_TRUE ); 
    return NV_TRUE;
}

NvBool
Max8907bBatteryChargerEnabled(
    NvOdmPmuDeviceHandle hDevice,
    NvBool *status)
{
    NvU8 data = 0;

    if(!Max8907bI2cRead8(hDevice, MAX8907B_CHG_STAT, &data))
        return NV_FALSE;

    data = (data >> MAX8907B_CHG_STAT_CHG_EN_STAT_SHIFT) & MAX8907B_CHG_STAT_CHG_EN_STAT_MASK;
    *status = (data == 0 ? NV_FALSE : NV_TRUE ); 
    return NV_TRUE;
}

NvBool
Max8907bBatteryChargerMainBattFull(
    NvOdmPmuDeviceHandle hDevice,
    NvBool *status)
{
    NvU8 data = 0;

    if(!Max8907bI2cRead8(hDevice, MAX8907B_CHG_STAT, &data))
        return NV_FALSE;

    data = (data >> MAX8907B_CHG_STAT_MBATTLOW_SHIFT) & MAX8907B_CHG_STAT_MBATTLOW_MASK;
    *status = (data == 0 ? NV_FALSE : NV_TRUE ); 
    return NV_TRUE;
}

