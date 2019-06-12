/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_pmu_tps6586x_rtc.h"
#include "nvodm_pmu_tps6586x_i2c.h"
#include "tps6586x_reg.h"

static NvBool bRtcNotInitialized = NV_TRUE;

/* Read RTC count register */
NvBool
Tps6586xRtcCountRead(
    NvOdmPmuDeviceHandle hDevice,
    NvU32* Count)
{
    NvU32 ReadBuffer[2];
    
    // 1) The I2C address pointer must not be left pointing in the range 0xC6 to 0xCA
    // 2) The maximum time for the address pointer to be in this range is 1ms
    // 3) Always read RTC_ALARM2 in the following order to prevent the address pointer
    // from stopping at 0xC6: RTC_ALARM2_LO, then RTC_ALARM2_HI        
    
    if (Tps6586xRtcWasStartUpFromNoPower(hDevice) && bRtcNotInitialized)
    {
        Tps6586xRtcCountWrite(hDevice, 0);
        *Count = 0;
    }
    else
    {
        // The unit of the RTC count is second!!! 1024 tick = 1s.
        // Read all 40 bit and right move 10 = Read the hightest 32bit and right move 2
        Tps6586xI2cRead32(hDevice, TPS6586x_RC6_RTC_COUNT4, &ReadBuffer[0]);
               
        Tps6586xI2cRead8(hDevice, TPS6586x_RCA_RTC_COUNT0, &ReadBuffer[1]);
        
        Tps6586xI2cRead8(hDevice, TPS6586x_RC0_RTC_CTRL, &ReadBuffer[1]);
        
        // return second
        *Count = ReadBuffer[0]>>2;
    }
  
    return NV_TRUE;
}

/* Write RTC count register */

NvBool 
Tps6586xRtcCountWrite(
    NvOdmPmuDeviceHandle hDevice, 
    NvU32 Count)
{
    NvU32 ReadBuffer = 0;
    
    // To enable incrementing of the RTC_COUNT[39:0] from an initial value set by the host,
    // the RTC_ENABLE bit should be written to 1 only after the RTC_OUT voltage reaches 
    // the operating range
    
    // Clear RTC_ENABLE before writing RTC_COUNT
    Tps6586xI2cRead8(hDevice, TPS6586x_RC0_RTC_CTRL, &ReadBuffer);
    ReadBuffer = ReadBuffer & 0xDF;
    Tps6586xI2cWrite8(hDevice, TPS6586x_RC0_RTC_CTRL, ReadBuffer);
        
    Tps6586xI2cWrite32(hDevice, TPS6586x_RC6_RTC_COUNT4, (Count<<2));
    Tps6586xI2cWrite8(hDevice,  TPS6586x_RCA_RTC_COUNT0, 0);       
    
    // Set RTC_ENABLE after writing RTC_COUNT
    Tps6586xI2cRead8(hDevice, TPS6586x_RC0_RTC_CTRL, &ReadBuffer);
    ReadBuffer = ReadBuffer | 0x20;
    Tps6586xI2cWrite8(hDevice, TPS6586x_RC0_RTC_CTRL, ReadBuffer);
    
    if (bRtcNotInitialized)
        bRtcNotInitialized = NV_FALSE;
    
    return NV_TRUE;
}

/* Read RTC alarm count register */

NvBool 
Tps6586xRtcAlarmCountRead(
    NvOdmPmuDeviceHandle hDevice, 
    NvU32* Count)
{
    return NV_FALSE;
}

/* Write RTC alarm count register */

NvBool 
Tps6586xRtcAlarmCountWrite(
    NvOdmPmuDeviceHandle hDevice, 
    NvU32 Count)
{
    return NV_FALSE;
}

/* Reads RTC alarm interrupt mask status */

NvBool 
Tps6586xRtcIsAlarmIntEnabled(NvOdmPmuDeviceHandle hDevice)
{
    return NV_FALSE;
}

/* Enables / Disables the RTC alarm interrupt */

NvBool 
Tps6586xRtcAlarmIntEnable(
    NvOdmPmuDeviceHandle hDevice, 
    NvBool Enable)
{
    return NV_FALSE;
}

/* Checks if boot was from nopower / powered state */

NvBool 
Tps6586xRtcWasStartUpFromNoPower(NvOdmPmuDeviceHandle hDevice)
{
    NvU32 Data = 0;

    if ((Tps6586xI2cRead8(hDevice, TPS6586x_RC0_RTC_CTRL, &Data)) == NV_TRUE)
    {
        return ((Data & 0x20)? NV_FALSE : NV_TRUE);
    }
    return NV_FALSE;
}
