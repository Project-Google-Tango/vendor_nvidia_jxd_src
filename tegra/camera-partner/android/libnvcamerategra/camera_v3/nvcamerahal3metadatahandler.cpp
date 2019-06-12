/*
 * Copyright (c) 2013 - 2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvcamerahal3metadatahandler.h"
#include "nvcamerahal3_tags.h"
#include "nv_log.h"

namespace android {

MetadataHandler::MetadataHandler():Thread(false)
{
    NV_LOGD(HAL3_META_DATA_PROCESS_TAG, "%s: ++", __FUNCTION__);

    mNextFrameNumber = 0;
    NvOsSemaphoreCreate(&mWorkSema, 0);

    NV_LOGD(HAL3_META_DATA_PROCESS_TAG, "%s: --", __FUNCTION__);
}

MetadataHandler::~MetadataHandler()
{
    NV_LOGD(HAL3_META_DATA_PROCESS_TAG, "%s: ++", __FUNCTION__);

    // Clear the list - free each entry.
    SettingsList::iterator s = mSettingsList.begin();
    while (s != mSettingsList.end())
    {
        Settings *entry = (*s);
        entry->settings.clear();
        s = mSettingsList.erase(s);
        delete entry; entry = NULL;
    }

    NvOsSemaphoreDestroy(mWorkSema);

    NV_LOGD(HAL3_META_DATA_PROCESS_TAG, "%s: --", __FUNCTION__);
}

bool MetadataHandler::threadLoop()
{
    NvOsSemaphoreWait(mWorkSema);
    NV_LOGD(HAL3_META_DATA_PROCESS_TAG, "%s: ++",__FUNCTION__);

    settingsListMutex.lock();
    SettingsList::iterator s;

    for (s = mSettingsList.begin(); s != mSettingsList.end();)
    {
        Settings *entry = (*s);
        if (entry->frameNumber == mNextFrameNumber)
        {
            //send the result metadata back to framework
            camera3_capture_result result;

            result.frame_number = entry->frameNumber;
            result.result = entry->settings.getAndLock();
            result.num_output_buffers = 0;
            result.output_buffers = NULL;

            // Send it off to the framework
            NV_LOGV(HAL3_META_DATA_PROCESS_TAG, "%s: send %dth frame metadata to framework", __FUNCTION__,
                result.frame_number);
            doSendResult(&result);

            entry->settings.unlock(result.result);
            entry->settings.clear();

            //erase the entry
            s = mSettingsList.erase(s);
            delete entry;
            entry = NULL;

            mNextFrameNumber++;
            // loop again if we already have the next frame setting available.
            s = mSettingsList.begin();
        }
        else
        {
            // continue looping
            s++;
        }
    }

    settingsListMutex.unlock();

    NV_LOGD(HAL3_META_DATA_PROCESS_TAG, "%s: --",__FUNCTION__);
    return true;
}

void MetadataHandler::flush()
{
    Mutex::Autolock l(settingsListMutex);
    NvOsSemaphoreSignal(mWorkSema);
}

void MetadataHandler::addSettings(CameraMetadata &settings, uint32_t frameNumber)
{
    Mutex::Autolock l(settingsListMutex);
    NV_LOGD(HAL3_META_DATA_PROCESS_TAG, "%s: ++",__FUNCTION__);

    // Add to setttingsList
    Settings *entry = new Settings;
    entry->settings.acquire(settings);
    entry->frameNumber = frameNumber;

    NV_LOGV(HAL3_META_DATA_PROCESS_TAG, "%s: adding result settings for %dth frame to handler", __FUNCTION__,
        frameNumber);
    mSettingsList.push_back(entry);

    if (frameNumber == mNextFrameNumber)
    {
        NvOsSemaphoreSignal(mWorkSema);
    }
    NV_LOGD(HAL3_META_DATA_PROCESS_TAG, "%s: --",__FUNCTION__);
}

} //namespace android
