/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef SENSOR_YUV_MT9M114_H
#define SENSOR_YUV_MT9M114_H

#include "nvodm_imager.h"
#include "nvodm_query_discovery_imager.h"

#ifndef SENSOR_YUV_MT9M114_GUID
#define SENSOR_YUV_MT9M114_GUID NV_ODM_GUID('s', '_', 'A', 'S', '1', '0', '4', '0')
#endif

#if defined(__cplusplus)
extern "C"
{
#endif

NvBool SensorYuvMT9M114_GetHal(NvOdmImagerHandle hImager);

#if defined(__cplusplus)
}
#endif

#endif  //SENSOR_YUV_H
