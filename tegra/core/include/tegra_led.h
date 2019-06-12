/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_LED_H
#define INCLUDED_LED_H

#if defined(__cplusplus)
extern "C"
{
#endif
/*
 * NvOdmInitLed: Initialize the LED in bootloader using PWM signal
 */
void NvOdmInitLed(void);

#define DEFAULT_PWM_SIGNAL_FREQUENCY 25000

#if defined(__cplusplus)
}
#endif

#endif
