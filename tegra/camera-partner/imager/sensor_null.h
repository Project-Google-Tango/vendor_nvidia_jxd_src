/*
 * Copyright (c) 2010-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef SENSOR_NULL_H
#define SENSOR_NULL_H

#include "nvodm_imager.h"
#include "nvodm_imager_guid.h"

#if defined(__cplusplus)
extern "C"
{
#endif

NvBool SensorNullBayer_GetHal(NvOdmImagerHandle hImager);
NvBool SensorNullYuv_GetHal(NvOdmImagerHandle hImager);
NvBool SensorNullCSIA_GetHal(NvOdmImagerHandle hImager);
NvBool SensorNullCSIB_GetHal(NvOdmImagerHandle hImager);
NvBool SensorNullTPGABayer_GetHal(NvOdmImagerHandle hImager);
NvBool SensorNullTPGARgb_GetHal(NvOdmImagerHandle hImager);
NvBool SensorNullTPGBBayer_GetHal(NvOdmImagerHandle hImager);
NvBool SensorNullTPGBRgb_GetHal(NvOdmImagerHandle hImager);



#if defined(__cplusplus)
}
#endif

#endif  //SENSOR_NULL_H
