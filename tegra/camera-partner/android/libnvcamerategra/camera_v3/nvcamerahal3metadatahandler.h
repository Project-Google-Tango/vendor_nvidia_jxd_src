/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * Copyright 2008, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _NV_CAMERA_METADATAHANDLER_H_
#define _NV_CAMERA_METADATAHANDLER_H_

#include <utils/RefBase.h>
#include <utils/Timers.h>
#include <utils/Condition.h>
#include <utils/Mutex.h>
#include <utils/List.h>
#include <utils/Thread.h>

#include "nvabstracts.h"
#include "nvcamerahal3common.h"

namespace android {

struct Settings
{
    uint32_t frameNumber;
    CameraMetadata settings;
};
typedef List<Settings *> SettingsList;

class MetadataHandler : public Thread, public AbsCaller
{
public:
    MetadataHandler();
    ~MetadataHandler();
    void flush();
    void addSettings(CameraMetadata &settings, uint32_t frameNumber);

private:
    virtual bool threadLoop();

private:
    Mutex settingsListMutex;
    NvOsSemaphoreHandle mWorkSema;
    SettingsList mSettingsList;
    uint32_t mNextFrameNumber;
};

} //namespace android

#endif
