/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NV_SENSOR_LISTENER_H
#define NV_SENSOR_LISTENER_H

#include <android/sensor.h>
#include <gui/Sensor.h>
#include <gui/SensorManager.h>
#include <gui/SensorEventQueue.h>
#include <utils/Looper.h>
#include <utils/Log.h>

namespace android {

/**
 * NvSensorListener class - registers with sensor manager to get sensor events.
 */

typedef void (*orientation_callback_t) (uint32_t orientation, uint32_t tilt, void *cookie);

class NvSensorLooperThread : public Thread {
    public:
        NvSensorLooperThread(Looper* looper)
            : Thread(false) {
                mLooper = sp<Looper>(looper);
        }

        ~NvSensorLooperThread() {
            mLooper.clear();
        }

        virtual bool threadLoop() {
            int32_t ret = mLooper->pollOnce(-1);
            return true;
        }

        // force looper wake up
        void wake() {
            mLooper->wake();
        }

    private:
        sp<Looper> mLooper;
};

class NvSensorListener : public RefBase
{
    public:
        typedef enum {
            SENSOR_ACCELEROMETER    = 1 << 0,
            SENSOR_MAGNETIC_FIELD   = 1 << 1,
            SENSOR_GYROSCOPE        = 1 << 2,
            SENSOR_LIGHT            = 1 << 3,
            SENSOR_PROXIMITY        = 1 << 4,
            SENSOR_ORIENTATION      = 1 << 5,
        } sensor_type_t;

    public:
        NvSensorListener();
        ~NvSensorListener();
        status_t initialize();
        void setCallbacks(orientation_callback_t orientation_cb, void *cookie);
        void enableSensor(sensor_type_t type);
        void disableSensor(sensor_type_t type);
        void handleOrientation(uint32_t orientation, uint32_t tilt);

    public:
        sp<SensorEventQueue> mSensorEventQueue;

    private:
        int sensorsEnabled;
        orientation_callback_t mOrientationCb;
        void *mCbCookie;
        sp<Looper> mLooper;
        sp<NvSensorLooperThread> mNvSensorLooperThread;
        Mutex mLock;
};

}

#endif
