/*
 * Copyright (c) 2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef IMAGER_DETECT_H
#define IMAGER_DETECT_H

#include "camera.h"

#if defined(__cplusplus)
extern "C"
{
#endif

NvBool
NvOdmImagerGetLayout(struct cam_device_layout **pDev, NvU32 *Count);


#if defined(__cplusplus)
}
#endif

#endif  //IMAGER_DETECT_H
