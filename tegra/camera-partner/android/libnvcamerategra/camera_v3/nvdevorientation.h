/*
 * Copyright (c) 2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NV_DEVORIENTATION_H
#define NV_DEVORIENTATION_H

#include <android/sensor.h>
#include <gui/Sensor.h>
#include <gui/SensorManager.h>
#include <gui/SensorEventQueue.h>
#include <utils/Looper.h>

namespace android
{

class LooperThread;

class NvDeviceOrientation
{
public:
    NvDeviceOrientation();
    ~NvDeviceOrientation();

public:
    bool initialize();
    int32_t orientation() const;

private:
    void enable();
    void disable();

    static int eventCallback(int, int, void *data);

private:
    int32_t mOrientation;
    int mDispOrientation;

    mutable Mutex mLock;
    sp<Looper> mLooper;
    sp<LooperThread> mLooperThread;
    sp<SensorEventQueue> mEventQueue;
};

}

#endif
