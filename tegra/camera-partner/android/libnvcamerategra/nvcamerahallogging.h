/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NV_CAMERA_HAL_LOGGING_H
#define NV_CAMERA_HAL_LOGGING_H

#include <stdlib.h>
#include <cutils/properties.h>

#include "nvcommon.h"
#include "nvos.h"

namespace android {

typedef struct NvStreamProfileDataRec {
    NvU32 ElapsedFrames;
    NvU32 StartTime;
    NvU32 FramesToMonitor;
} NvStreamProfileData;

class NvCameraHalLogging
{
public:
    void NvCameraPreviewProfile();
    void setPropLogs();

private:
    NvStreamProfileData mPreviewProfile;

};

} //namespace android

#endif // NV_CAMERA_HAL_LOGGING_H
