/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define LOG_TAG "NvCameraHalLogging"

#define LOG_NDEBUG 0

#include "nvcamerahallogging.h"
#include <utils/Log.h>

#include "runtimelogs.h"

// Global runtime logging control variable
int32_t glogLevel;

namespace android {

void NvCameraHalLogging::NvCameraPreviewProfile()
{
    char Value[PROPERTY_VALUE_MAX];

    // check if preview profile is on and print preview frame
    // rate accordingly
    property_get("camera-preview-profile", Value, "0");
    mPreviewProfile.FramesToMonitor = atoi(Value);
    if (mPreviewProfile.FramesToMonitor == 0)
    {
         NvOsMemset(&mPreviewProfile, 0,
             sizeof(mPreviewProfile));
    }
    else
    {
        NvU32 Time = NvOsGetTimeMS();

        if (mPreviewProfile.ElapsedFrames >=
            mPreviewProfile.FramesToMonitor)
        {
            ALOGI("%s - preview frame rate is %.2f fps\n",
                __FUNCTION__,
                mPreviewProfile.ElapsedFrames * 1000.0/
                (Time - mPreviewProfile.StartTime));
            mPreviewProfile.ElapsedFrames = 0;
        }

        if (mPreviewProfile.ElapsedFrames == 0)
            mPreviewProfile.StartTime = Time;
        mPreviewProfile.ElapsedFrames++;
    }

}

// To enable/disable logs, even before camera gets started.
// Use "adb shell setprop enableLogs 0/1" to change it.
void NvCameraHalLogging::setPropLogs()
{
    char value[PROPERTY_VALUE_MAX];
    property_get("enableLogs", value, "0");
    glogLevel = atoi(value);
}

} // namespace android
