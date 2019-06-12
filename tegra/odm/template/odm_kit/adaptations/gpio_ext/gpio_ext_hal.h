/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** 
 * @file
 * <b>NVIDIA Tegra ODM Kit:
 *         Abstraction layer stub for external gpio
 *         adaptation</b>
 */

#ifndef INCLUDED_NVODM_GPIO_EXT_ADAPTATION_HAL_H
#define INCLUDED_NVODM_GPIO_EXT_ADAPTATION_HAL_H

#include "nvodm_gpio_ext.h"

#ifdef __cplusplus
extern "C"
{
#endif

//  A simple HAL for the External GPIO adaptations.
typedef void (*pfnExternalGpioWritePins)(NvU32, NvU32, NvU32);
typedef NvU32 (*pfnExternalGpioReadPins)(NvU32, NvU32);

typedef struct NvOdmGpioExtDeviceRec
{
    pfnExternalGpioWritePins    pfnWritePins;
    pfnExternalGpioReadPins     pfnReadPins;

} NvOdmGpioExtDevice;


#ifdef __cplusplus
}
#endif

#endif
