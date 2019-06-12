/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef INCLUDED_PWM_TEST_SOC_H
#define INCLUDED_PWM_TEST_SOC_H

#include "nvrm_pwm.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define PWM_TEST_PIN_PORT             (NvU32)('h' - 'a')
#define PWM_TEST_PIN_NUM              0
#define PWM_TEST_INSTANCE             NvRmPwmOutputId_PWM0

#if defined(__cplusplus)
}
#endif

#endif //INCLUDED_PWM_TEST_SOC_H
