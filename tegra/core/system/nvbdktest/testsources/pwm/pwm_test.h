/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef INCLUDED_PWM_TEST_H
#define INCLUDED_PWM_TEST_H

#include "nverror.h"
#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define PWM_TEST_INTERVAL_US          200000
#define PWM_TEST_FREQ_MIN             400
#define PWM_TEST_FREQ_MAX             2000
#define PWM_TEST_FREQ_CHANGE          400

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_PWM_TEST_H
