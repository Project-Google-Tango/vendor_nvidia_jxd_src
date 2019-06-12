/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_nvrm_pwm_H
#define INCLUDED_nvrm_pwm_H


#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvrm_pinmux.h"
#include "nvrm_module.h"
#include "nvrm_init.h"

#include "nvos.h"
#include "nvcommon.h"

/**
 * NvRmPwmHandle is an opaque handle to the NvRmPwmStructRec interface
 */

typedef struct NvRmPwmRec *NvRmPwmHandle;

/**
 * Defines possible PWM modes.
 */

typedef enum
{

    /// Specifies Pwm disable mode
        NvRmPwmMode_Disable = 1,

    /// Specifies Pwm enable mode
        NvRmPwmMode_Enable,

    /// Specifies Blink LED enabled mode
        NvRmPwmMode_Blink_LED,

    /// Specifies Blink output 32KHz clock enable mode
        NvRmPwmMode_Blink_32KHzClockOutput,

    /// Specifies Blink disabled mode
        NvRmPwmMode_Blink_Disable,
    NvRmPwmMode_Num,
    NvRmPwmMode_Force32 = 0x7FFFFFFF
} NvRmPwmMode;

/**
 * Defines the possible PWM output pin
 */

typedef enum
{

    /// Specifies PWM Output-0
        NvRmPwmOutputId_PWM0 = 1,

    /// Specifies PWM Output-1
        NvRmPwmOutputId_PWM1,

    /// Specifies PWM Output-2
        NvRmPwmOutputId_PWM2,

    /// Specifies PWM Output-3
        NvRmPwmOutputId_PWM3,

    /// Specifies PMC Blink LED
        NvRmPwmOutputId_Blink,
    NvRmPwmOutputId_Num,
    NvRmPwmOutputId_Force32 = 0x7FFFFFFF
} NvRmPwmOutputId;

/**
 * @brief Initializes and opens the pwm channel. This function allocates the
 * handle for the pwm channel and provides it to the client.
 *
 * Assert encountered in debug mode if passed parameter is invalid.
 *
 * @param hDevice Handle to the Rm device which is required by Rm to acquire
 *  the resources from RM.
 * @param phPwm Points to the location where the Pwm handle shall be stored.
 *
 * @retval NvSuccess Indicates that the Pwm channel has successfully opened.
 * @retval NvError_InsufficientMemory Indicates that function fails to allocate
 *  the memory.
 * @retval NvError_NotInitialized Indicates the Pwm initialization failed.
 */

 NvError NvRmPwmOpen(
    NvRmDeviceHandle hDevice,
    NvRmPwmHandle * phPwm );

/**
 * @brief Closes the Pwm channel. This function frees the memory allocated for
 *  the pwm handle for the pwm channel.
 *  This function de-initializes the pwm channel. This API never fails.
 *
 * @param hPwm A handle from NvRmPwmOpen().  If hPwm is NULL, this API does
 *     nothing.
 */

 void NvRmPwmClose(
    NvRmPwmHandle hPwm );

/**
 * @brief Configure PWM module as disable/enable. Also, it is used
 *  to set the PWM duty cycle and frequency. Beside that, it is
 *  used to configure PMC' blinking LED if OutputId is NvRmPwmOutputId_Blink
 *
 * @param hPwm Handle to the PWM channel.
 * * @param OutputId The output pin to config. Allowed OutputId values are
 *   defined in ::NvRmPwmOutputId
 * @param Mode The mode type to config. Allowed mode values are
 *   defined in ::NvRmPwmMode
 * @param DutyCycle The duty cycle is an unsigned 15.16 fixed point
 *   value that represents PWM duty cycle in percentage range from
 *   0.00 to 100.00. For example, 10.5 percentage duty cycle would be
 *   represented as 0x000A8000. This parameter is ignored if NvRmPwmMode
 *   is NvRmPwmMode_Blink_32KHzClockOutput or NvRmPwmMode_Blink_Disable
 * @param RequestedFreqHzOrPeriod The requested frequency in Hz or Period
 *  A requested frequency value beyond the max supported value will be
 *  clamped to the max supported value.
 *  If PMC Blink LED is used, this parameter is represented as
 *  request period time in second unit. This parameter is ignored if
 *  NvRmPwmMode is NvRmPwmMode_Blink_32KHzClockOutput or
 *  NvRmPwmMode_Blink_Disable
 *
 * @param pCurrentFreqHzOrPeriod Pointer to the returns frequency of
 *  that mode. If PMC Blink LED is used then it is the pointer to
 *  the returns period time. This parameter is ignored if NvRmPwmMode
 *  is NvRmPwmMode_Blink_32KHzClockOutput or NvRmPwmMode_Blink_Disable
 *
 * @retval NvSuccess Indicates the configuration succeeded.
 */

 NvError NvRmPwmConfig(
    NvRmPwmHandle hPwm,
    NvRmPwmOutputId OutputId,
    NvRmPwmMode Mode,
    NvU32 DutyCycle,
    NvU32 RequestedFreqHzOrPeriod,
    NvU32 * pCurrentFreqHzOrPeriod );

#if defined(__cplusplus)
}
#endif

#endif
