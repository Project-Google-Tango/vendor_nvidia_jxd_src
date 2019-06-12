/*
 * Copyright (c) 2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvdevorientation.h"
#include <stdint.h>
#include <math.h>
#include <sys/types.h>
#include "nvcamerahal3common.h"
#include "nvcamerahal3_tags.h"
#include "nv_log.h"

#define EVENT_RATE_IN_MS 10

namespace android
{

class LooperThread:public Thread
{
public:
    LooperThread(const sp<Looper>& looper);
    ~LooperThread();

    // force looper wake up
    void wake();

private:
    bool threadLoop();

private:
    sp<Looper> mLooper;
};

LooperThread::LooperThread(const sp<Looper>& looper)
    :Thread(false)
    , mLooper(looper)
{
}

LooperThread::~LooperThread()
{
    mLooper.clear();
}

bool LooperThread::threadLoop()
{
    mLooper->pollOnce(-1);
    return true;
}

void LooperThread::wake()
{
    mLooper->wake();
}

NvDeviceOrientation::NvDeviceOrientation()
    :mOrientation(0),
    mDispOrientation(0),
    mEventQueue(0)
{
    mLooper = new Looper(false);
    mLooperThread = new LooperThread(mLooper);
}

NvDeviceOrientation::~NvDeviceOrientation()
{
    disable();

    // 1. request exit
    // 2. wake up looper which should be polling for an event
    // 3. wait for exit
    mLooperThread->requestExit();
    mLooperThread->wake();
    mLooperThread->join();
    mLooperThread.clear();
    mLooperThread = 0;

    mLooper->removeFd(mEventQueue->getFd());
    mLooper.clear();
    mLooper = 0;
}

bool NvDeviceOrientation::initialize()
{
    SensorManager &man(SensorManager::getInstance());

    mEventQueue = man.createEventQueue();
    if (mEventQueue == 0)
    {
        NV_LOGE("%s: createEventQueue returned NULL", __FUNCTION__);
        return false;
    }

    mDispOrientation = NvCameraHal_getDisplayOrientation();
    int err = mLooper->addFd(mEventQueue->getFd(), 0, ALOOPER_EVENT_INPUT,
                NvDeviceOrientation::eventCallback, this);
    if (!err)
    {
        NV_LOGE("%s: Looper failed to add fd!", __FUNCTION__);
        return false;
    }

    status_t ret = mLooperThread->run("sensor looper thread", PRIORITY_BACKGROUND);
    if (ret == INVALID_OPERATION)
    {
        NV_LOGE("%s: thread already running?!", __FUNCTION__);
    }
    else if (ret != NO_ERROR)
    {
        NV_LOGE("%s: Couldn't run thread", __FUNCTION__);
        return false;
    }

    enable();

    return true;
}

void NvDeviceOrientation::enable()
{
    Mutex::Autolock lock(&mLock);
    SensorManager &man = SensorManager::getInstance();
    Sensor const *sensor = man.getDefaultSensor(Sensor::TYPE_ACCELEROMETER);
    if (sensor != 0)
    {
        NV_LOGV(HAL3_DEVICE_ORIENTATION_TAG, "%s: orientation = %p (%s)\n",
            __FUNCTION__, sensor, sensor->getName().string());
        mEventQueue->enableSensor(sensor);
        mEventQueue->setEventRate(sensor, ms2ns(EVENT_RATE_IN_MS));
    }
    else
    {
        NV_LOGE("Cannot detect the accelerometer.\n");
    }
}

void NvDeviceOrientation::disable()
{
    Mutex::Autolock lock(&mLock);
    SensorManager &man = SensorManager::getInstance();
    Sensor const *sensor = man.getDefaultSensor(Sensor::TYPE_ACCELEROMETER);
    NV_LOGV(HAL3_DEVICE_ORIENTATION_TAG, "%s: orientation = %p (%s)\n",
        __FUNCTION__, sensor, sensor->getName().string());
    mEventQueue->disableSensor(sensor);
}

int32_t NvDeviceOrientation::orientation() const
{
    Mutex::Autolock lock(&mLock);
    return mOrientation;
}

int NvDeviceOrientation::eventCallback(int, int, void *data)
{
    ASensorEvent event;
    NvDeviceOrientation *orient = static_cast<NvDeviceOrientation*>(data);

    int count = orient->mEventQueue->read(&event, 1);
    if (count == 1 && event.type == Sensor::TYPE_ACCELEROMETER)
    {
        float x = -event.vector.azimuth;
        float y = -event.vector.pitch;
        float z = -event.vector.roll;
        float mag = (x*x)+(y*y);

        if ((mag*4) >= (z*z))
        {
            int angle = 90-(int)round((float)atan2(-y, x)*(180.0/M_PI));
            angle += orient->mDispOrientation;

            if (angle >= 360)
            {
                angle %= 360;
            }

            while (angle < 0)
            {
                angle += 360;
            }

            orient->mOrientation = angle;
        }
    }
    else if (count != -EAGAIN)
    {
        NV_LOGE("%s: Failed to read sensor event. (Error: %d)",
            __FUNCTION__, count);
    }

    return 1;
}

}
