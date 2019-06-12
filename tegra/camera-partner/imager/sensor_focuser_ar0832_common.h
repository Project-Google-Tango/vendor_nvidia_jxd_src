/*
* Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software and related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/


#ifndef SENSOR_FOCUSER_AR0832_COMMON_H
#define SENSOR_FOCUSER_AR0832_COMMON_H

#include <nvodm_services.h>
#include "nvodm_imager.h"

enum {
    AR0832_DRIVER_OPEN_NONE  = 0,
    AR0832_DRIVER_OPEN_LEFT  = (1 << 0),
    AR0832_DRIVER_OPEN_RIGHT = (1 << 1),
};

typedef struct _AR0832_Driver {
    NvOdmOsMutexHandle  fd_mutex;
    NvS32               open_cnt;
    NvS32               fd;
} AR0832_Driver;

typedef struct _AR0832_Stereo_Info AR0832_Stereo_Info;

struct _AR0832_Stereo_Info {
    NvOdmImagerStereoCameraMode StereoCameraMode;
    AR0832_Driver left_drv;
    AR0832_Driver right_drv;
    NvBool right_on;
    NvBool left_on;
    NvS32  (*drv_open)(AR0832_Stereo_Info *pStereoInfo);
    void   (*drv_close)(AR0832_Stereo_Info *pStereoInfo);
    NvS32  (*drv_ioctl)(AR0832_Stereo_Info *pStereoInfo, int request, void *arg);
};

#endif // SENSOR_FOCUSER_AR0832_COMMON_H
