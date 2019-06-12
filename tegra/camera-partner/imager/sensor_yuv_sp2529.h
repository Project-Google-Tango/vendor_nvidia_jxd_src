/**
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef SENSOR_YUV_SP2529_H
#define SENSOR_YUV_SP2529_H

#include "imager_hal.h"
#include "nvodm_query_discovery_imager.h"

#if defined(__cplusplus)
extern "C"
{
#endif

struct SP2529_mode {
	int xres;
	int yres;
};
enum {
    YUV_Whitebalance_Invalid = 0,
    YUV_Whitebalance_Auto,
    YUV_Whitebalance_Incandescent,
    YUV_Whitebalance_Fluorescent,
    YUV_Whitebalance_WarmFluorescent,
    YUV_Whitebalance_Daylight,
    YUV_Whitebalance_CloudyDaylight,
    YUV_Whitebalance_Shade,
    YUV_Whitebalance_Twilight,
    YUV_Whitebalance_Custom
};

#define SP2529_INVALID_COARSE_TIME    -1

#define SP2529_POWER_LEVEL_OFF			0
#define SP2529_POWER_LEVEL_ON			1

#define SP2529_IOCTL_SET_SENSOR_MODE	_IOW('o', 1, struct SP2529_mode)
#define SP2529_IOCTL_GET_SENSOR_STATUS	_IOR('o', 2, __u8)
#define SP2529_IOCTL_POWER_LEVEL		_IOW('o', 3, __u32)
#define SP2529_IOCTL_GET_AF_STATUS		_IOR('o', 4, __u8)
#define SP2529_IOCTL_SET_AF_MODE		_IOR('o', 5, __u8)
#define SP2529_IOCTL_SET_COLOR_EFFECT	_IOR('o', 6, __u8)
#define SP2529_IOCTL_SET_WHITE_BALANCE	_IOR('o', 7, __u8)
#define SP2529_IOCTL_SET_EXPOSURE		_IOR('o', 8, __u8)
#define SP2529_IOCTL_SET_SCENE_MODE		_IOR('o', 9, __u8)

NvBool SensorYuvSP2529_GetHal(NvOdmImagerHandle hImager);

#if defined(__cplusplus)
}
#endif

#endif  //SENSOR_BAYER_OV5693_H
