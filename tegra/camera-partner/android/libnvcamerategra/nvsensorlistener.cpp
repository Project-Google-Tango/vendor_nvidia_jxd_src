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

#define LOG_TAG "NvSensorListener"

#include "nvsensorlistener.h"

#include <stdint.h>
#include <math.h>
#include <sys/types.h>

#define LOG_FUNCTION_ENTRY  ALOGV("%s++\n", __FUNCTION__)
#define LOG_FUNCTION_EXIT   ALOGV("%s--\n", __FUNCTION__)

#define EVENT_RATE_IN_MS                (10)
#define SENSOR_LISTENER_THREAD_PRIORITY (PRIORITY_BACKGROUND)
#define ONE_EIGHTY_OVER_PI              (57.29577957855f)

namespace android {

static int sensor_events_listener(int fd, int events, void *data)
{
    NvSensorListener *listener = (NvSensorListener *)data;
    ssize_t num_sensors;
    ASensorEvent sen_events[8];

    while ((num_sensors = listener->mSensorEventQueue->read(sen_events, 8)) > 0)
    {
        for (int i = 0; i < num_sensors; ++i)
        {
            // see http://en.wikipedia.org/wiki/Spherical_coordinate_system#Cartesian_coordinates
            // about conversion from cartesian to spherical for orientation calculations.
            if (sen_events[i].type == Sensor::TYPE_ACCELEROMETER)
            {
                float x = -sen_events[i].vector.azimuth;
                float y = -sen_events[i].vector.pitch;
                float z = -sen_events[i].vector.roll;
                float mag = x * x + y * y;
                float tilt = 0.f;
                int   orient = 0;

                if (mag * 4 >= z * z)
                {
                    tilt   = (float)atan2(-y, x) * ONE_EIGHTY_OVER_PI;
                    orient = 90 - (int)round(tilt);

                    while (orient >= 360)
                        orient -= 360;

                    while (orient < 0)
                        orient += 360;

                    listener->handleOrientation(orient, (int)tilt);
                }
            }
            else if (sen_events[i].type == Sensor::TYPE_GYROSCOPE)
            {
                ALOGE("GYROSCOPE_EVENT");
            }
        }
    }

    if ((num_sensors < 0) && (num_sensors != -EAGAIN))
    {
        ALOGE("Reading events failed: %s", strerror(-num_sensors));
    }

    return 1;
}

NvSensorListener::NvSensorListener()
{
    LOG_FUNCTION_ENTRY;

    sensorsEnabled      = 0;
    mOrientationCb      = NULL;
    mSensorEventQueue   = NULL;
    mNvSensorLooperThread = NULL;
    mCbCookie = NULL;

    LOG_FUNCTION_EXIT;
}

NvSensorListener::~NvSensorListener()
{
    LOG_FUNCTION_ENTRY;

    if (mNvSensorLooperThread.get())
    {
        // 1. request exit
        // 2. wake up looper which should be polling for an event
        // 3. wait for exit
        mNvSensorLooperThread->requestExit();
        mNvSensorLooperThread->wake();
        mNvSensorLooperThread->join();
        mNvSensorLooperThread.clear();
        mNvSensorLooperThread = NULL;
    }

    if (mLooper.get())
    {
        mLooper->removeFd(mSensorEventQueue->getFd());
        mLooper.clear();
        mLooper = NULL;
    }

    LOG_FUNCTION_EXIT;
}

status_t NvSensorListener::initialize()
{
    status_t ret = NO_ERROR;
    SensorManager &mgr(SensorManager::getInstance());

    LOG_FUNCTION_ENTRY;

    sp<Looper> mLooper;

    mSensorEventQueue = mgr.createEventQueue();
    if (mSensorEventQueue == NULL)
    {
        ALOGE("createEventQueue returned NULL\n");
        ret = NO_INIT;
        goto out;
    }
    else
    {
        Sensor const* const* list;
        ALOGV("Find %d sensors.\n", (int)mgr.getSensorList(&list));
    }

    mLooper = new Looper(false);
    mLooper->addFd(mSensorEventQueue->getFd(), 0,
                   ALOOPER_EVENT_INPUT, sensor_events_listener, this);

    if (mNvSensorLooperThread.get() == NULL)
    {
        mNvSensorLooperThread = new NvSensorLooperThread(mLooper.get());
        if (mNvSensorLooperThread.get() == NULL)
        {
            ALOGE("Couldn't create sensor looper thread\n");
            ret = NO_MEMORY;
            goto out;
        }
    }

    ret = mNvSensorLooperThread->run("sensor looper thread",
                                    SENSOR_LISTENER_THREAD_PRIORITY);
    if (ret == INVALID_OPERATION)
    {
        ALOGE("thread already running?!\n");
    }
    else if (ret != NO_ERROR)
    {
        ALOGE("Couldn't run thread\n");
        goto out;
    }

out:
    LOG_FUNCTION_EXIT;

    return ret;
}

void NvSensorListener::setCallbacks(orientation_callback_t orientation_cb, void *cookie)
{
    LOG_FUNCTION_ENTRY;

    if (orientation_cb != NULL)
        mOrientationCb = orientation_cb;

    mCbCookie = cookie;

    LOG_FUNCTION_EXIT;
}

void NvSensorListener::handleOrientation(uint32_t orientation, uint32_t tilt)
{
    LOG_FUNCTION_ENTRY;

    Mutex::Autolock lock(&mLock);

    if (mOrientationCb && (sensorsEnabled & SENSOR_ORIENTATION))
        mOrientationCb(orientation, tilt, mCbCookie);

    LOG_FUNCTION_EXIT;
}

void NvSensorListener::enableSensor(sensor_type_t type)
{
    Sensor const *sensor;
    SensorManager &mgr(SensorManager::getInstance());

    LOG_FUNCTION_ENTRY;

    Mutex::Autolock lock(&mLock);

    if ((type & SENSOR_ORIENTATION) && !(sensorsEnabled & SENSOR_ORIENTATION))
    {
        sensor = mgr.getDefaultSensor(Sensor::TYPE_ACCELEROMETER);
        if (sensor != NULL)
        {
            ALOGV("orientation = %p (%s)\n", sensor, sensor->getName().string());
            mSensorEventQueue->enableSensor(sensor);
            mSensorEventQueue->setEventRate(sensor, ms2ns(EVENT_RATE_IN_MS));
            sensorsEnabled |= SENSOR_ORIENTATION;
        }
        else
        {
            ALOGE("Cannot detect the accelerometer.\n");
        }
    }

    LOG_FUNCTION_EXIT;
}

void NvSensorListener::disableSensor(sensor_type_t type)
{
    Sensor const *sensor;
    SensorManager &mgr(SensorManager::getInstance());

    LOG_FUNCTION_ENTRY;

    Mutex::Autolock lock(&mLock);

    if ((type & SENSOR_ORIENTATION) && (sensorsEnabled & SENSOR_ORIENTATION))
    {
        sensor = mgr.getDefaultSensor(Sensor::TYPE_ACCELEROMETER);
        ALOGV("orientation = %p (%s)\n", sensor, sensor->getName().string());
        mSensorEventQueue->disableSensor(sensor);
        sensorsEnabled &= ~SENSOR_ORIENTATION;
    }

    LOG_FUNCTION_EXIT;
}

} // namespace android

