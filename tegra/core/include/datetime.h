/*
 * Copyright (c) 2013 NVIDIA Corporation. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA DateTIme API</b>
 *
 * @b Description: This file declares the API for DateTime Manipulation.
 */

#ifndef INCLUDED_DATETIME_H
#define INCLUDED_DATETIME_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvcommon.h"

/*
 * Defines the Time at the moment
 */
typedef struct DateTimeRec
{
    /// Specifies the Year
    NvU32 Year;
    /// Specifies the Month
    NvU8 Month;
    /// Specifies the Day
    NvU8 Day;
    /// Specifies the Hour
    NvU8 Hour;
    /// Specifies the Minute
    NvU8 Minute;
    /// Specifies the Second
    NvU8 Second;
} DateTime;

/**
 * @brief Add seconds to the current time wrt to reference year
 *
 * This API adds the seconds to the intime and
 * stores it in out time
 *
 * @param OutTime Time after adding the seconds to Initime
 * @param Intime Current time passed
 * @param Seconds.Seconds to be added to current time
 */
void AddSeconds(DateTime *OutTime, DateTime InTime, NvU32 Seconds);

/**
 * @brief Add seconds to the current time
 *
 * This API adds the seconds to the time and
 * Updates it
 *
 * @param time Current time passed
 * @param Secs.Secs to be added to time
 *
 * @retval Updated DateTime after adding secs
 */
DateTime NvAppAddAlarmSecs(DateTime time, NvU32 secs);


#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_DATETIME_H
