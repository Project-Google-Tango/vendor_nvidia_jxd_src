/*
 * POSIX header for AOS
 *
 * Copyright 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef _NVPOSIX_TIME_H_
#define _NVPOSIX_TIME_H_

#define CLOCK_REALTIME 0

#ifndef __clockid_t_defined
#define __clockid_t_defined
typedef int clockid_t;
#endif

#ifndef __time_t_defined
#define __time_t_defined
typedef long time_t;
struct timespec {
    time_t tv_sec;
    long tv_nsec;
};
#endif

extern int clock_gettime(clockid_t __clock_id, struct timespec *__tp);
extern int clock_settime(clockid_t __clock_id, const struct timespec *__tp);

#endif // #ifndef _NVPOSIX_TIME_H_
