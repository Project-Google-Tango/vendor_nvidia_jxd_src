/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define LOG_TAG "NvCameraCallbacks"

#define LOG_NDEBUG 0

#include <utils/Log.h>
#include <utils/KeyedVector.h>
#include <utils/String8.h>

#include "nvcameracallbacks.h"

//=========================================================
// Header to add runtime logging support
//=========================================================
#include "runtimelogs.h"


namespace android {

void NvCameraClientCallback::doCallback()
{
    switch (cbType)
    {
        case NvCameraClientCb_Event:
            if (mNotifyCb != NULL)
            {
                ALOGVV("%s: call event callback [%d]\n",__FUNCTION__,msgType);
                mNotifyCb(msgType,ext1,ext2,user);
            }
            break;

        case NvCameraClientCb_Data:
            if (mDataCb != NULL)
            {
                mDataCb(msgType,shmem,0,mMetadata,user);
                shmem->release(shmem);
            }
            break;

        case NvCameraClientCb_DataTimestamp:
            if (mDataCbTimestamp != NULL)
            {
                mDataCbTimestamp(timestamp,msgType,shmem,0,user);
                shmem->release(shmem);
            }
            break;

        default:
            ALOGE("%s: invalid client callback - %d\n",__FUNCTION__,cbType);
            break;
    }
}

NvCameraClientCallback::NvCameraClientCallback(
                camera_notify_callback cb,
                int32_t msgType, int ext1, int ext2, void *user)
{
    NvOsMemset(this,0,sizeof(*this));
    this->cbType = NvCameraClientCb_Event;
    this->mNotifyCb = cb;
    this->msgType = msgType;
    this->ext1 = ext1;
    this->ext2 = ext2;
    this->user = user;
}

NvCameraClientCallback::NvCameraClientCallback(
                camera_data_callback cb,
                int32_t msgType,
                camera_memory_t *shmem, void *user,
                camera_frame_metadata_t *metadata)
{
    NvOsMemset(this,0,sizeof(*this));
    this->cbType = NvCameraClientCb_Data;
    this->mDataCb = cb;
    this->msgType = msgType;
    this->shmem = shmem;
    this->user = user;
    this->mMetadata = metadata;
}

NvCameraClientCallback::NvCameraClientCallback(
                camera_data_timestamp_callback cb,
                int32_t msgType, nsecs_t timeStamp,
                camera_memory_t *shmem, void *user)
{
    NvOsMemset(this,0,sizeof(*this));
    this->cbType = NvCameraClientCb_DataTimestamp;
    this->mDataCbTimestamp = cb;
    this->msgType = msgType;
    this->shmem = shmem;
    this->timestamp = timeStamp;
    this->user = user;
}

NvCameraCallbackQueue::NvCameraCallbackQueue()
{
    isReleased = NV_TRUE;
    requestStopExecuting = NV_TRUE;
    acceptingInput = NV_FALSE;
    head = NULL;
    tail = NULL;
    queueSem = NULL;
    cbThread = NULL;
    listLock = NULL;
}

NvCameraCallbackQueue::~NvCameraCallbackQueue()
{
    NvError e = NvSuccess;
    ALOGVV("%s++\n",__FUNCTION__);
    e = release();
    if (e != NvSuccess)
    {
        ALOGE("%s: release() failed [0x%0x]\n",__FUNCTION__,e);
    }
    ALOGVV("%s--\n",__FUNCTION__);
}

NvError NvCameraCallbackQueue::init()
{
    NvError e = NvSuccess;
    NvError err = NvSuccess;

    isReleased = NV_FALSE;
    requestStopExecuting = NV_FALSE;
    head = NULL;
    tail = NULL;

    err = NvOsSemaphoreCreate(&queueSem,0);
    if (err != NvSuccess)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    err = NvOsMutexCreate(&listLock);
    if (err != NvSuccess)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    acceptingInput = NV_TRUE;
    err = NvOsThreadCreate(execute,this,&cbThread);
    if (err != NvSuccess)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    return NvSuccess;

 fail:
    requestStopExecuting = NV_TRUE;
    acceptingInput = NV_FALSE;
    NvOsSemaphoreDestroy(queueSem);
    queueSem = NULL;
    NvOsMutexDestroy(listLock);
    listLock = NULL;
    return e;
}

void NvCameraCallbackQueue::stopExecution()
{
    NvOsMutexLock(listLock);
    requestStopExecuting = NV_TRUE;
    acceptingInput = NV_FALSE;
    NvOsMutexUnlock(listLock);
    NvOsSemaphoreSignal(queueSem);
}

void NvCameraCallbackQueue::stopInput()
{
    NvOsMutexLock(listLock);
    acceptingInput = NV_FALSE;
    NvOsMutexUnlock(listLock);
}

NvError NvCameraCallbackQueue::release()
{
    NvError e = NvSuccess;
    NvCameraClientCallback *cb = NULL;

    ALOGVV("%s: shutting down callbacks queue + thread +++.\n",__FUNCTION__);
    if (isReleased)
    {
        ALOGVV("%s: already released.\n",__FUNCTION__);
        return e;
    }

    // Stop callbacks thread:
    stopExecution();
    NvOsThreadJoin(cbThread);
    cbThread = NULL;
    ALOGVV("%s: callbacks thread finished.\n",__FUNCTION__);

    NvOsSemaphoreDestroy(queueSem);
    queueSem = NULL;
    NvOsMutexDestroy(listLock);
    listLock = NULL;

    // Clean up all remaining callback requests:
    while (head != NULL)
    {
        ALOGVV("%s: deleting un-executed client callback.\n",__FUNCTION__);
        cb = head->next;
        delete head;
        head = cb;
    }
    tail = NULL;

    isReleased = NV_TRUE;
    ALOGVV("%s: done! ---\n",__FUNCTION__);
    return e;
}

NvBool NvCameraCallbackQueue::add(NvCameraClientCallback *cb)
{
    if (cb == NULL)
        return NV_FALSE;
    if (putNext(cb))
    {
        NvOsSemaphoreSignal(queueSem);
    }
    else // Queue input is closed
    {
        delete cb;
    }
    return NV_TRUE;
}

NvCameraClientCallback *NvCameraCallbackQueue::getNext(NvBool& stopRun)
{
    NvCameraClientCallback *nextCb = NULL;

    NvOsMutexLock(listLock);
    nextCb = this->head;
    if (nextCb != NULL)
    {
        this->head = this->head->next;
        if (this->tail == nextCb)
            this->tail = NULL;
    }
    stopRun = requestStopExecuting;
    NvOsMutexUnlock(listLock);
    return nextCb;
}

NvBool NvCameraCallbackQueue::putNext(NvCameraClientCallback *cb)
{
    NvOsMutexLock(listLock);
    if (!acceptingInput)
    {
        NvOsMutexUnlock(listLock);
        return NV_FALSE;
    }

    cb->next = NULL;
    if (this->head == NULL)
    {
        this->head = cb;
    }
    else
    {
        this->tail->next = cb;
    }
    this->tail = cb;
    NvOsMutexUnlock(listLock);
    return NV_TRUE;
}

void NvCameraCallbackQueue::execute(void *args)
{
    NvCameraCallbackQueue *cbQueue =
        (NvCameraCallbackQueue *)args;
    NvCameraClientCallback *cb = NULL;
    NvBool stopRun = NV_FALSE;
    NvBool executeTail = NV_TRUE;
    NvError err = NvSuccess;

    while (!stopRun)
    {
        err = NvOsSemaphoreWaitTimeout(cbQueue->queueSem, semTimeout);
        if (err != NvSuccess && err != NvError_Timeout)
        {
            ALOGE("%s: semaphore wait failed! [%d]\n",__FUNCTION__,err);
            executeTail = NV_FALSE;
            break;
        }
        cb = cbQueue->getNext(stopRun);
        if (cb != NULL)
        {
            cb->doCallback();
            delete cb;
            cb = NULL;
        }
    }
    cbQueue->stopInput();
    // Now execute the remaining callbacks if any:
    cb = cbQueue->getNext(stopRun);
    while (cb != NULL)
    {
        if (executeTail)
            cb->doCallback();
        delete cb;
        cb = cbQueue->getNext(stopRun);
    }
}

}
