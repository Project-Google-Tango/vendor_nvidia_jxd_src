/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_gpio_ext.h"
#include "gpio_ext_null.h"

void
null_NvOdmExternalGpioWritePins(
    NvU32 Port,
    NvU32 Pin,
    NvU32 PinValue)
{
    // NULL implementation that does nothing.
    return;
}

NvU32
null_NvOdmExternalGpioReadPins(
    NvU32 Port,
    NvU32 Pin)
{
    // NULL implementation that does nothing.
    return 0;
}

