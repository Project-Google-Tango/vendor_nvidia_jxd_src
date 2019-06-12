/*
 * Copyright (c) 2013 - 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvcamerahallogging.h"
#include <math.h>
#include "nvcamerahal3_tags.h"
#include "nv_log.h"

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
            NV_LOGI(HAL3_LOGGING_TAG, "%s - preview frame rate is %.2f fps\n",
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

void FrameJitterStatistics::clear()
{
    maxFramePeriod = 0;
    minFramePeriod = (NvU64) -1;
    sumOfFramePeriod = 0;
    sumOfSquaresOfFramePeriod = 0;
    numberOfFrames = 0;
    recordingOnPrior = 0;
    nsFrameTimeStampPrior = 0;
}

void FrameJitterStatistics::updateStatistics(NvU64 nsFrameTimeStamp)
{
    NvU64 framePeriod;
    char value[PROPERTY_VALUE_MAX];

    recordingOnPrior = recordingOn;

    property_get("camera.debug.jitter.statistics", value, "0");
    recordingOn = atoi(value);

    if (recordingOnPrior && !recordingOn)
    {
        printStatistics();
    }
    else if (!recordingOnPrior && recordingOn)
    {
        clear();
        nsFrameTimeStampPrior = nsFrameTimeStamp;
    }

    if (recordingOn)
    {
        framePeriod = (nsFrameTimeStamp - nsFrameTimeStampPrior)/1000;
        nsFrameTimeStampPrior = nsFrameTimeStamp;
        if (framePeriod > 0)
        {
            if (maxFramePeriod < framePeriod)
                maxFramePeriod = framePeriod;
            if (minFramePeriod > framePeriod)
                minFramePeriod = framePeriod;
            sumOfFramePeriod += framePeriod;
            sumOfSquaresOfFramePeriod += framePeriod * framePeriod;
            ++numberOfFrames;
        }
    }
}

void FrameJitterStatistics::printStatistics()
{
    NvF32 meanFramePeriod = 0;
    NvF32 jitterVariance = 0;
    NvF32 avgFrameRate = 0;

    if (numberOfFrames > 0 && sumOfFramePeriod > 0)
    {
        meanFramePeriod = (1.f * sumOfFramePeriod) / numberOfFrames;
        jitterVariance = (1.f * sumOfSquaresOfFramePeriod)/numberOfFrames -
                meanFramePeriod * meanFramePeriod;
        avgFrameRate = 1000000.f / meanFramePeriod;

        NV_LOGI(HAL3_LOGGING_TAG, "----- Jitter stats -----");
        NV_LOGI(HAL3_LOGGING_TAG, "Jitter stats: Max Frame Period: %f ms", maxFramePeriod / 1000.f);
        NV_LOGI(HAL3_LOGGING_TAG, "Jitter stats: Min Frame Period: %f ms", minFramePeriod / 1000.f);
        NV_LOGI(HAL3_LOGGING_TAG, "Jitter stats: Mean Frame Period: %f ms", meanFramePeriod / 1000.f);
        NV_LOGI(HAL3_LOGGING_TAG, "Jitter stats: Jitter Variance: %f ms^2", jitterVariance / 1000000.f);
        NV_LOGI(HAL3_LOGGING_TAG, "Jitter stats: Jitter Standard Deviation: %f ms",
                sqrt((NvF64) jitterVariance)/1000.f);
        NV_LOGI(HAL3_LOGGING_TAG, "Jitter stats: Frame rate from Jitter: %f", avgFrameRate);
    }
    else
    {
        NV_LOGE("Error: Jitter stats: not enough frame data");
    }
}

} // namespace android
