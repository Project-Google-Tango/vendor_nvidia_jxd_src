/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "datetime.h"
#include "nvos.h"

static NvU32 secondsPerDay       = 60 * 60 * 24;
static NvU32 secondsPerHour      = 60 * 60;
static NvU32 secondsPerMinute    = 60;

static NvU32 daysPer400Years     = (365 * 400) + (400 / 4) - 3;
static NvU32 daysPer100Years     = (365 * 100) + (100 / 4) - 1;
static NvU32 daysPer4Years       = (365 *   4) + 1;
static NvU32 daysPerYear         = (365 *   1);
static NvU32 referenceYear       = 1601;

static NvBool isLeapYear(NvU32 year)
{
    return (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0);
}

// mon is the month of the year: Jan = 1
static NvU32 daysInMonth(NvU32 mon, NvU32 year)
{
    NvU32 n;
    static NvU32 days[] = {
        31,        // Jan
        28,        // Feb
        31,        // Mar
        30,        // Apr
        31,        // May
        30,        // Jun
        31,        // Jul
        31,        // Aug
        30,        // Sep
        31,        // Oct
        30,        // Nov
        31,        // Dec
    };

    n = days[mon-1];

    // For leap years, February contains 29 days instead of 28.
    if( mon == 2 && isLeapYear(year) )
        n += 1;

    return n;
}

/*
    yearFromDays notes:

    d400yr = days/400yr = 400(365 + 1/4) -3
    d100yr = days/100yr = 100(365 + 1/4) -1
    d4yr   = days/  4yr =   4(365 + 1/4)
    d1yr   = days/  1yr =     365

    If Dec 31 of any leap year or any year divisible by 100,
    then the number of days per divisor (daysPerXXXYears)
    will appear to have enough days in it to exceed the divisor.

    Specifically:
    d400yr -1 = d100yr * 4
    d100yr -1 = d4yr   *25
    d4yr   -1 = d1yr   * 4

    Call the factors *4, *25, *4 the multipliers.

    Therefore, December 31 of years which evenly divide by the
    factors 400, 100, and 4 must be handled a bit differently.
*/
static NvU32 yearFromDays(NvU32 *days)
{
    NvU32 year = referenceYear;
    NvU32 y;

    y = *days / daysPer400Years;
    if(y)
    {
        year += y * 400;
        *days -= y * daysPer400Years;
    }

    y = *days / daysPer100Years;
    if(y)
    {
        if(*days == daysPer400Years - 1)
        {
            y -= 1;
        }
        year += y * 100;
        *days -= y * daysPer100Years;
    }

    y = *days / daysPer4Years;
    if(y)
    {
        // There is no check here for
        // (*days == daysPer100Years - 1)
        // since 100 year intervals are not leap years.
        year += y * 4;
        *days -= y * daysPer4Years;
    }

    y = *days / daysPerYear;
    if(y)
    {
        if(*days == daysPer4Years - 1 )
        {
            y -= 1;
        }
        year += y;
        *days -= y * daysPerYear;
    }

    return year;
}

// Return 1-12 representing the month given a number of days in the year.
// Decrease days by the number of days required to reach the month.
static NvU32 monthFromDaysInYear(NvU32 *days, NvU32 year)
{
    NvU32 i;
    for(i = 1; i <= 12; i++ ) {
        NvU32 mdays = daysInMonth(i, year);
        if(*days >= mdays )
            *days -= mdays;
        else
            return i;
    }

    return 12;
}

static NvU32 daysSinceReferenceYear(NvU32 year)
{
    // Using 32 bits for days is OK,
    // as 32 bits of days represents 5.8 million years.
    // Time::UTCTime uses 16 bits for years which is only 32K years.
    NvU32 days = 0;
    NvU32 years = year - referenceYear;

    if( year < referenceYear ) {
        return 0;
    }

    days  += daysPer400Years * (years / 400);
    years -= (years / 400) * 400;

    days  += daysPer100Years * (years / 100);
    years -= (years / 100) * 100;

    days  += daysPer4Years * (years / 4);
    years -= (years / 4) * 4;

    days  += daysPerYear * years;
    years -= years;

    return days;
}

/*
 * input -> Number of Seconds From reference Year
 * output -> DateTime
 */
static void timeFromSec(DateTime *time, NvU64 tsec)
{
    NvU32 days;
    NvU32 secDay;

    // 32 bits for days represents >367K years.
    days = (NvU32)(tsec / secondsPerDay);

    // Number of seconds remaining in the current day.
    secDay = tsec - ((NvU64)days * secondsPerDay);

    time->Year   = yearFromDays(&days);
    time->Month  = monthFromDaysInYear(&days, time->Year);
    time->Day    = days + 1;

    time->Hour   = secDay / secondsPerHour;
    secDay     %=          secondsPerHour;

    time->Minute = secDay / secondsPerMinute;
    secDay     %=          secondsPerMinute;

    time->Second = secDay;
}

/*
 * input -> DateTime
 * output -> Number of Seconds From reference year
 */
static NvU64 SecFromTime(DateTime time)
{
    NvU32 mon;
    NvU32 days;
    NvU64 tsec;

    days = daysSinceReferenceYear(time.Year);

    // Do not count the month that is being passed in.
    // Only the months prior to the time.month get counted for their full set of days.
    for(mon = 1; mon < time.Month; mon++)
    {
        days += daysInMonth(mon, time.Year);
    }

    // For counting the day of the month, only count the days leading up to the
    // day requested, not the "current" day - which has not completed yet.
    // UTCTime "day" is based on 1 for the first day of the month.
    days += time.Day - 1;

    // Convert days to seconds, and add the current days time to the total.
    tsec = (NvU64)days * secondsPerDay;
    tsec += (NvU64)time.Hour * secondsPerHour;
    tsec += (NvU64)time.Minute * secondsPerMinute;
    tsec += time.Second;

    return tsec;
}

void AddSeconds(DateTime *OutTime, DateTime InTime, NvU32 Seconds)
{
    NvU64 tsec;

    if (OutTime && (Seconds > 0))
    {
        tsec = SecFromTime(InTime);
        tsec += Seconds;
        timeFromSec(OutTime, tsec);
    }
}

DateTime NvAppAddAlarmSecs(DateTime time, NvU32 secs)
{
    time.Year += 2000;
    AddSeconds(&time, time, secs);
    time.Year -= 2000;
    return time;
}
