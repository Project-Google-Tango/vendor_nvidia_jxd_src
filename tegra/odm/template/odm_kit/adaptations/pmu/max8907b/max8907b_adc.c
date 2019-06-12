/*
 * Copyright (c) 2009 - 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "max8907b.h"
#include "max8907b_adc.h"
#include "max8907b_i2c.h"
#include "max8907b_reg.h"

NvBool
Max8907bAdcVBatSenseRead(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 *volt)
{
    // FIXME: Work in progress. 
    *volt = 3210;
    return NV_TRUE;
#if 0
    // Main battery voltage VMBATT is automatically measured by
    // ADC scheduler. So we can just read the result.
    NvU8 VmbattMsb;
    if ( !Max8907bAdcI2cRead8(hDevice, MAX8907B_VMBATT_MSB, &VmbattMsb) )
        return NV_FALSE;

    // Ignore the VMBATT_LSB register value.
    // VMBATT_MSB alone provides 32mV resolution.
    *volt = ((NvU32)VmbattMsb << 4) * (8192 / 4096);
    return NV_TRUE;
#endif
}

NvBool
Max8907bAdcVBatTempRead(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 *volt)
{
#if 0
    NvU32 timeout = 0;
    NvU8  dataS1  = 0;

    NV_ASSERT(hDevice);
    NV_ASSERT(volt);

    // Setup ADC (resolution, etc..)

    // Start conversion

    // Wait for conversion

    // Make sure conversion is complete (or timeout or error)

    // Get result
    *volt = << compute >>;

#endif
    // FIXME: Work in progress. 
    *volt = 0;
    return NV_TRUE;
}

NvU32
Max8907bBatteryTemperature(
    NvU32 VBatSense,
    NvU32 VBatTemp)
{
    return 0;
}

