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
#include "max8907b_rtc.h"
#include "max8907b_i2c.h"
#include "max8907b_reg.h"

/** 
* The Maxim 8907B does not have an RTC that simply counts
* seconds from some time t0 (as defined by the OS API).
* Instead, this RTC contains several BCD (Binary Coded Decimal)
* registers, including: seconds, minutes, hours, days, day of
* week, date, etc...  These registers account for leap year and
* the various days of the month as well.
* 
* Since the OS interpretation of seconds to a particular
* date/time from some OS-defined t0 is unknown at this level of
* the implementation, it is not possible to translate the given
* seconds into these registers (at least, not without a
* dependency on some OS-specific information).
* 
* Therefore, this implementation contains a static variable
* (RtcDays) which is derived from the number of seconds given
* when Max8907bRtcCountWrite() is called.  The seconds, minutes
* and hours are then programmed to the RTC and used to keep
* track of the current time within the day.
* 
* TO DO: Increment the day whenever it rolls over (requires
* handling an interrupt at midnight each day).
*/

#define MAX8907B_SECONDS_PER_DAY    (60*60*24)
#define MAX8907B_SECONDS_PER_HOUR   (60*60)
#define MAX8907B_SECONDS_PER_MINUTE (60)

static NvBool bRtcNotInitialized = NV_TRUE;
static NvU32 RtcDays = 0;

NvBool 
Max8907bRtcCountRead(
    NvOdmPmuDeviceHandle hDevice, 
    NvU32* Count)
{
    NvU32 data = 0;
    NvU32 BcdHours, BcdMinutes, BcdSeconds;
    NvU32 Hours, Minutes, Seconds;

    if (Max8907bRtcWasStartUpFromNoPower(hDevice) && bRtcNotInitialized)
    {
        Max8907bRtcCountWrite(hDevice, 0);
        *Count = 0;
    }
    else
    {
        if (Max8907bRtcI2cReadTime(hDevice, MAX8907B_RTC_SEC, &data))
        {

            BcdHours   = (data >>  8) & 0xFF;
            BcdMinutes = (data >> 16) & 0xFF;
            BcdSeconds = (data >> 24) & 0xFF;

            Hours   = ((BcdHours   & 0xF0)>>4)*10 + (BcdHours   & 0xF);
            Minutes = ((BcdMinutes & 0xF0)>>4)*10 + (BcdMinutes & 0xF);
            Seconds = ((BcdSeconds & 0xF0)>>4)*10 + (BcdSeconds & 0xF);

            *Count = (Hours * MAX8907B_SECONDS_PER_HOUR) +
                     (Minutes * MAX8907B_SECONDS_PER_MINUTE) + Seconds;
        }
        else
        {
            NvOdmOsDebugPrintf("Max8907bRtcCountRead() error. ");
            return NV_FALSE;
        }
    }
    return NV_TRUE;
}

NvBool 
Max8907bRtcAlarmCountRead(
    NvOdmPmuDeviceHandle hDevice, 
    NvU32* Count)
{
    return NV_FALSE;
}

NvBool 
Max8907bRtcCountWrite(
    NvOdmPmuDeviceHandle hDevice, 
    NvU32 Count)
{
    NvU32 Hours, Minutes, Seconds;
    NvU32 BcdHours, BcdMinutes, BcdSeconds;
    NvU32 data = 0;

    BcdHours = BcdMinutes = BcdSeconds = 0;

    RtcDays = Count / MAX8907B_SECONDS_PER_DAY;

    Hours   = (Count % MAX8907B_SECONDS_PER_DAY) / MAX8907B_SECONDS_PER_HOUR;
    Minutes = ((Count % MAX8907B_SECONDS_PER_DAY) % MAX8907B_SECONDS_PER_HOUR) / MAX8907B_SECONDS_PER_MINUTE;
    Seconds = Count % MAX8907B_SECONDS_PER_MINUTE;

    BcdHours   = ((  Hours/10) << 4) | (  Hours%10);
    BcdMinutes = ((Minutes/10) << 4) | (Minutes%10);
    BcdSeconds = ((Seconds/10) << 4) | (Seconds%10);

    data = (BcdSeconds << 24) | (BcdMinutes << 16) | (BcdHours << 8);
    if (Max8907bRtcI2cWriteTime(hDevice, MAX8907B_RTC_SEC, data))
    {
        bRtcNotInitialized = NV_FALSE;
        return NV_TRUE;
    }
    else
    {
        NvOdmOsDebugPrintf("Max8907bRtcCountWrite() error. ");
        return NV_FALSE;
    }
}

NvBool 
Max8907bRtcAlarmCountWrite(
    NvOdmPmuDeviceHandle hDevice, 
    NvU32 Count)
{
    return NV_FALSE;
}

NvBool 
Max8907bRtcIsAlarmIntEnabled(NvOdmPmuDeviceHandle hDevice)
{
    return NV_FALSE;
}

NvBool 
Max8907bRtcAlarmIntEnable(
    NvOdmPmuDeviceHandle hDevice, 
    NvBool Enable)
{
    return NV_FALSE;
}

NvBool 
Max8907bRtcWasStartUpFromNoPower(NvOdmPmuDeviceHandle hDevice)
{
    return NV_TRUE;
}

NvBool
Max8907bIsRtcInitialized(NvOdmPmuDeviceHandle hDevice)
{
    return (!bRtcNotInitialized);
}

