/*
 * Copyright (c) 2009 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NV_CAMERA_CALLBACKS_H
#define NV_CAMERA_CALLBACKS_H

#include <hardware/camera.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <android/native_window.h>
#include <utils/Timers.h>

#include "nvos.h"

namespace android {

typedef enum NvCameraClientCallbackTypeEnum {
    NvCameraClientCb_Event = 0,
    NvCameraClientCb_Data,
    NvCameraClientCb_DataTimestamp,
    NvCameraClientCb_Invalid = 0x7FFFFFFF
} NvCameraClientCallbackType;

class NvCameraClientCallback {
public:
    NvCameraClientCallbackType cbType;

    // Client callback parameters:
    camera_notify_callback mNotifyCb;
    camera_data_callback   mDataCb;
    camera_data_timestamp_callback mDataCbTimestamp;
    camera_frame_metadata_t *mMetadata;
    int32_t msgType;
    nsecs_t timestamp;
    camera_memory_t *shmem;
    int ext1;
    int ext2;
    void* user;

    NvCameraClientCallback *next;

    NvCameraClientCallback(camera_notify_callback cb,
                    int32_t msgType, int ext1, int ext2, void *user);

    NvCameraClientCallback(camera_data_callback cb,
                    int32_t msgType,
                    camera_memory_t *shmem, void *user,
                    camera_frame_metadata_t *metadata = NULL);

    NvCameraClientCallback(camera_data_timestamp_callback cb,
                    int32_t msgType, nsecs_t timeStamp,
                    camera_memory_t *shmem, void *user);

    void doCallback();
};

class NvCameraCallbackQueue {
public:
    NvCameraCallbackQueue();
    ~NvCameraCallbackQueue();

    NvError init();
    NvError release();

    NvBool add(NvCameraClientCallback *cb);

    void stopInput();
    void stopExecution();

private:
    mutable NvOsMutexHandle listLock; // Lock for our list of pending client callbacks
    NvCameraClientCallback *head;
    NvCameraClientCallback *tail;
    NvOsSemaphoreHandle queueSem;
    NvOsThreadHandle cbThread;
    NvBool isReleased;
    NvBool requestStopExecuting;
    NvBool acceptingInput;

    NvCameraClientCallback *getNext(NvBool &stopRun);
    NvBool putNext(NvCameraClientCallback *cb);

    static void execute(void *args);

    static const NvU32 semTimeout = 250;
};

}; // namespace android

#endif // NV_CAMERA_CALLBACKS_H
