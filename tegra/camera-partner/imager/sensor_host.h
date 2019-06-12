/*
 * Copyright (c) 2010-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_SENSOR_HOST_H
#define INCLUDED_SENSOR_HOST_H

#include "nvodm_imager.h"
#include "nvodm_imager_guid.h"

#if defined(__cplusplus)
extern "C"
{
#endif

NvBool SensorHost_GetHal(NvOdmImagerHandle hImager);

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_SENSOR_HOST_H

