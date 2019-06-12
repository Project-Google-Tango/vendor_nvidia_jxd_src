/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVODM_BATTERY_INT_H
#define INCLUDED_NVODM_BATTERY_INT_H

#include "nvcommon.h"

/* Module debug msg: 0=disable, 1=enable */
#define NVODMBATTERY_ENABLE_PRINTF (0)

#if (NVODMBATTERY_ENABLE_PRINTF)
#define NVODMBATTERY_PRINTF(x)   NvOdmOsDebugPrintf x
#else
#define NVODMBATTERY_PRINTF(x)
#endif

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct NvOdmBatteryDeviceRec
{
    NvBool                      bBattPresent;
    NvBool                      bBattFull;
} NvOdmBatteryDevice;

#if defined(__cplusplus)
}
#endif

/** @} */

#endif /* INCLUDED_NVODM_BATTERY_INT_H */

