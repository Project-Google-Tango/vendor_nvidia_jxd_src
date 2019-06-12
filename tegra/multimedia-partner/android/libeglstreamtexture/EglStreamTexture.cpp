/*
 * Copyright (C) 2011 The Android Open Source Project
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
/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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

//#define LOG_NDEBUG 0
#define LOG_TAG "EglStreamTexture"
#include <utils/Log.h>

#include "eglapiinterface.h"
#include "EglStreamWrapper.h"
#include "EglStreamTextureWrapper.h"
#include "EglStreamTexture.h"
#include <dlfcn.h>
#include <ui/GraphicBuffer.h>

#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/IGraphicBufferAlloc.h>
#include <utils/String8.h>
#include <private/gui/ComposerService.h>


#define DEBUG_DUMP 0

#if defined(__cplusplus)
extern "C"
{
#endif
#include "nvgralloc.h"
#include "nvrm_memmgr.h"

static PFNEGLQUERYSTREAMKHRPROC  eglQueryStreamKHR = NULL;
static NvEglApiExports eglExports;

#if defined(__cplusplus)
}
#endif

namespace android {

static NvGrModule const *GrModule = NULL;

EglStreamTexture::EglStreamTexture(EGLStreamKHR stream, EGLDisplay dpy)
               : mDefaultWidth(1),
                mDefaultHeight(1),
                mPixelFormat(0),
                mBufferCount(MIN_ASYNC_BUFFER_SLOTS),
                mClientBufferCount(0),
                mServerBufferCount(MIN_ASYNC_BUFFER_SLOTS),
                mSynchronousMode(true),
                mConnectedApi(NO_CONNECTED_API),
                mStopped(false),
                mLatency(0),
                mEglStream(stream),
                mDisplay(dpy),
                m_hRmDev(NULL),
                mWaitSem(NULL),
                bInitFailed(false){
    ALOGV("EglStreamTexture::EglStreamTexture");
    sp<ISurfaceComposer> composer(ComposerService::getComposerService());
    mGraphicBufferAlloc = composer->createGraphicBufferAlloc();
    if (EglStreamWrapperInitializeEgl() != OK)
    {
        ALOGE("Failed to initialize EGLStream");
        bInitFailed = true;
    }
    else
    {
        eglExports = g_EglStreamWrapperExports;
        eglQueryStreamKHR = g_EglStreamWrapperEglQueryStreamKHR;

        if (eglExports.streamProducerALDataLocatorConnect)
        {
            mWrapper.mType = AL_AS_EGL_PRODUCER;
            mWrapper.mpDataObject = this;
            eglExports.streamProducerALDataLocatorConnect((NvEglStreamHandle)mEglStream,
                (NvEglApiStreamInfo *)&mWrapper);
            if (NvSuccess != NvRmOpen(&m_hRmDev, 0))
            {
                ALOGE("NvRmOpen FAILED!");
                bInitFailed = true;
            }
            else if (NvSuccess != NvOsSemaphoreCreate(&mWaitSem, 0))
            {
                ALOGE("NvOsSemaphoreCreate FAILED!");
                bInitFailed = true;
            }
        }
        else
        {
            ALOGE("%s[%d] ERROR OUT NOT INIT \n", __FUNCTION__, __LINE__);
            bInitFailed = true;
        }
    }
    int value = 0;
    if (eglQueryStreamKHR)
        bool result = eglQueryStreamKHR(mDisplay, mEglStream, EGL_STREAM_FIFO_LENGTH_KHR, &value);
    mSynchronousMode = (value > 0);
    ALOGI("%s[%d] fifo length %d \n", __FUNCTION__, __LINE__, value);
}

EglStreamTexture::~EglStreamTexture() {
    ALOGV("EglStreamTexture::~EglStreamTexture +");
#if DEBUG_DUMP
    String8 str;
    ALOGE("dumping now \n");
    dump(str);
    ALOGE("DUMP: \n\n\n %s \n\n\n", str.string());
#endif
    close();
    ALOGV("EglStreamTexture::~EglStreamTexture -");
}

void EglStreamTexture::close() {

    ALOGV("EglStreamTexture::close +");

    if (!mStopped) {
        Mutex::Autolock lock(mMutex);
        ALOGV("EglStreamTexture::~EglStreamTexture NEXT ");
        mStopped = true;
        mDequeueCondition.signal();
        freeAllBuffersLocked();
    }

    if (m_hRmDev)
    {
        NvRmClose(m_hRmDev);
        m_hRmDev = NULL;
    }
    if (mWaitSem)
    {
        NvOsSemaphoreDestroy(mWaitSem);
        mWaitSem = NULL;
    }

    EglStreamWrapperClose();

    ALOGV("EglStreamTexture::close -");

}

status_t EglStreamTexture::setBufferCountServerLocked(int bufferCount) {
    if ((bufferCount > NUM_BUFFER_SLOTS) || bInitFailed)
    {
        ALOGE("%s[%d] error %d \n", __FUNCTION__, __LINE__, bufferCount);
        return BAD_VALUE;
    }

    // special-case, nothing to do
    if (bufferCount == mBufferCount)
        return OK;

    if (!mClientBufferCount &&
        bufferCount >= mBufferCount) {
        // easy, we just have more buffers
        mBufferCount = bufferCount;
        mServerBufferCount = bufferCount;
        mDequeueCondition.signal();
    } else {
        // we're here because we're either
        // - reducing the number of available buffers
        // - or there is a client-buffer-count in effect

        // less than 2 buffers is never allowed
        if (bufferCount < 2)
            return BAD_VALUE;

        // when there is non client-buffer-count in effect, the client is not
        // allowed to dequeue more than one buffer at a time,
        // so the next time they dequeue a buffer, we know that they don't
        // own one. the actual resizing will happen during the next
        // dequeueBuffer.

        mServerBufferCount = bufferCount;
    }
    return OK;
}

// Called from the consumer side
status_t EglStreamTexture::setBufferCountServer(int bufferCount) {
    Mutex::Autolock lock(mMutex);
    return setBufferCountServerLocked(bufferCount);
}

status_t EglStreamTexture::setBufferCount(int bufferCount) {
    ALOGV("EglStreamTexture::setBufferCount %d \n", bufferCount);
    if ((bufferCount > NUM_BUFFER_SLOTS) || (bInitFailed)) {
        ALOGE("setBufferCount: bufferCount is larger than the number of buffer slots");
        return BAD_VALUE;
    }

    Mutex::Autolock lock(mMutex);
    // Error out if the user has dequeued buffers
    for (int i = 0 ; i < mBufferCount ; i++) {
        if (mSlots[i].mBufferState == BufferSlot::DEQUEUED) {
            ALOGE("setBufferCount: client owns some buffers %d ", i);
            return INVALID_OPERATION;
        }
    }

    if (bufferCount == 0) {
        const int minBufferSlots = mSynchronousMode ?
                MIN_SYNC_BUFFER_SLOTS : MIN_ASYNC_BUFFER_SLOTS;
        mClientBufferCount = 0;
        bufferCount = (mServerBufferCount >= minBufferSlots) ?
                mServerBufferCount : minBufferSlots;
        return setBufferCountServerLocked(bufferCount);
    }

    // We don't allow the client to set a buffer-count less than
    // MIN_ASYNC_BUFFER_SLOTS (3), there is no reason for it.
    if (bufferCount < MIN_ASYNC_BUFFER_SLOTS) {
        return BAD_VALUE;
    }

    // here we're guaranteed that the client doesn't have dequeued buffers
    // and will release all of its buffer references.
    mBufferCount = bufferCount;
    mClientBufferCount = bufferCount;
    mDequeueCondition.signal();
    freeAllBuffersLocked();
    return OK;
}

status_t EglStreamTexture::requestBuffer(int slot, sp<GraphicBuffer>* buf) {
    ALOGV("EglStreamTexture::requestBuffer %d \n", slot);
    Mutex::Autolock lock(mMutex);
    if (slot < 0 || mBufferCount <= slot || bInitFailed) {
        ALOGE("requestBuffer: slot index out of range [0, %d]: %d",
                mBufferCount, slot);
        return BAD_VALUE;
    }
    mSlots[slot].mRequestBufferCalled = true;
    *buf = mSlots[slot].mGraphicBuffer;
    ALOGV("requestBuffer: slot %d %p ",
                slot, buf->get());
    return NO_ERROR;
}

status_t EglStreamTexture::dequeueBuffer(int *outBuf, sp<Fence>& syncFence,
                uint32_t w, uint32_t h, uint32_t format, uint32_t usage) {
    ALOGV("dequeueBuffer w %d h %d format 0x%x \n", w, h, format);
    Mutex::Autolock lock(mMutex);

    status_t returnFlags(OK);
    int found, foundSync;
    int dequeuedCount = 0;
    bool tryAgain = true;
    syncFence.clear();
    if (bInitFailed)
    {
        ALOGE("dequeueBuffer w %d h %d format 0x%x INVALID_OPERATION \n",
            w, h, format);
        return INVALID_OPERATION;
    }
    while (tryAgain) {
        // We need to wait for the FIFO to drain if the number of buffer
        // needs to change.
        //
        // The condition "number of buffer needs to change" is true if
        // - the client doesn't care about how many buffers there are
        // - AND the actual number of buffer is different from what was
        //   set in the last setBufferCountServer()
        //                         - OR -
        //   setBufferCountServer() was set to a value incompatible with
        //   the synchronization mode (for instance because the sync mode
        //   changed since)
        //
        // As long as this condition is true AND the FIFO is not empty, we
        // wait on mDequeueCondition.

        int minBufferCountNeeded = mSynchronousMode ?
                MIN_SYNC_BUFFER_SLOTS : MIN_ASYNC_BUFFER_SLOTS;

        if (!mClientBufferCount &&
                ((mServerBufferCount != mBufferCount) ||
                        (mServerBufferCount < minBufferCountNeeded))) {
            freeAllBuffersLocked();
            mBufferCount = mServerBufferCount;
            if (mBufferCount < minBufferCountNeeded)
                mBufferCount = minBufferCountNeeded;
            returnFlags |= ISurfaceTexture::RELEASE_ALL_BUFFERS;
        }

        // look for a free buffer to give to the client
        found = INVALID_BUFFER_SLOT;
        foundSync = INVALID_BUFFER_SLOT;
        dequeuedCount = 0;
        for (int i = 0; i < mBufferCount; i++) {
            const int state = mSlots[i].mBufferState;
            if (state == BufferSlot::DEQUEUED) {
                dequeuedCount++;
                continue; // won't be continuing if could
                // dequeue a non 'FREE' current slot like
                // that in SurfaceTexture
            }
            // In case of Encoding, we do not deque the mCurrentSlot buffer
            //  since we follow synchronous mode (unlike possibly in
            //  SurfaceTexture that could be using the asynch mode
            //  or has some mechanism in GL to be able to wait till the
            //  currentslot is done using the data)
            // Here, we have to wait for the MPEG4Writer(or equiv)
            // to tell us when it's done using the current buffer
            if (state == BufferSlot::FREE) {
                foundSync = i;
                // Unlike that in SurfaceTexture,
                // We don't need to worry if it is the
                // currentslot or not as it is in state FREE
                found = i;
                break;
            }
        }

        // clients are not allowed to dequeue more than one buffer
        // if they didn't set a buffer count.
        if (!mClientBufferCount && dequeuedCount) {
            ALOGE("%s[%d] EINVAL - ", __FUNCTION__, __LINE__);
            return -EINVAL;
        }

        // we're in synchronous mode and didn't find a buffer, we need to wait
        // for for some buffers to be consumed
        tryAgain = mSynchronousMode && (foundSync == INVALID_BUFFER_SLOT);
        if (tryAgain) {
            ALOGV("Waiting..In synchronous mode dequeued %d total %d \n", dequeuedCount, mBufferCount);
            mDequeueCondition.wait(mMutex);
        }
        if (mStopped) {
            ALOGE("%s[%d] stopped - ", __FUNCTION__, __LINE__);
            return NO_INIT;
        }
    }

    if (mSynchronousMode && found == INVALID_BUFFER_SLOT) {
        // foundSync guaranteed to be != INVALID_BUFFER_SLOT
        found = foundSync;
    }

    if (found == INVALID_BUFFER_SLOT) {
        ALOGE("%s[%d] error busy \n", __FUNCTION__, __LINE__);
        return -EBUSY;
    }

    const int bufIndex = found;
    *outBuf = found;

    const bool useDefaultSize = !w && !h;
    if (useDefaultSize) {
        // use the default size
        w = mDefaultWidth;
        h = mDefaultHeight;
    }

    const bool updateFormat = (format != 0);
    if (!updateFormat) {
        // keep the current (or default) format
        format = mPixelFormat;
    }

    // buffer is now in DEQUEUED (but can also be current at the same time,
    // if we're in synchronous mode)
    mSlots[bufIndex].mBufferState = BufferSlot::DEQUEUED;

    if (mSlots[bufIndex].mFence.SyncPointID != NVRM_INVALID_SYNCPOINT_ID)
    {
        NvRmFence* fence = &mSlots[bufIndex].mFence;
        ALOGV("### %s[%d] wait fence id %d value %d \n", __FUNCTION__, __LINE__,
            fence->SyncPointID,
            fence->Value);
        if(NvSuccess != NvRmChannelSyncPointWaitTimeout(m_hRmDev,
                            fence->SyncPointID,
                            fence->Value,
                            mWaitSem,
                            350))
            ALOGI("NvRmChannelSyncPointWaitTimeout wait timed out\n");
        mSlots[bufIndex].mFence.SyncPointID = NVRM_INVALID_SYNCPOINT_ID;
    }
    format = HAL_PIXEL_FORMAT_YV12;
    const sp<GraphicBuffer>& buffer(mSlots[bufIndex].mGraphicBuffer);
    if ((buffer == NULL) ||
        (uint32_t(buffer->width)  != w) ||
        (uint32_t(buffer->height) != h) ||
        (uint32_t(buffer->format) != format) ||
        ((uint32_t(buffer->usage) & usage) != usage)) {
            // XXX: This will be changed to USAGE_HW_VIDEO_ENCODER once driver
            // issues with that flag get fixed.
            usage |= GraphicBuffer::USAGE_HW_TEXTURE;
            status_t error;
            // override user provided value.
            sp<GraphicBuffer> graphicBuffer(
                    mGraphicBufferAlloc->createGraphicBuffer(
                                    w, h, format, usage, &error));
            if (graphicBuffer == 0) {
                ALOGE("dequeueBuffer: SurfaceComposer::createGraphicBuffer failed");
                return error;
            }
            if (updateFormat) {
                mPixelFormat = format;
            }
            if (buffer != NULL)
                ALOGV("dequeueBuffer w %d h %d buffer %p \n",
                    buffer->width, buffer->height, graphicBuffer.get());
            mSlots[bufIndex].mGraphicBuffer = graphicBuffer;
            mSlots[bufIndex].mRequestBufferCalled = false;
            mSlots[bufIndex].mFence.SyncPointID = NVRM_INVALID_SYNCPOINT_ID;
            returnFlags |= ISurfaceTexture::BUFFER_NEEDS_REALLOCATION;
    }
    return returnFlags;
}

// TODO: clean this up
status_t EglStreamTexture::setSynchronousMode(bool enabled) {
    Mutex::Autolock lock(mMutex);
    if (mStopped) {
        ALOGE("setSynchronousMode: EglStreamTexture has been stopped!");
        return NO_INIT;
    }

    if (!enabled) {
        // Async mode is not allowed
        ALOGE("EglStreamTexture can be used only synchronous mode!");
        return INVALID_OPERATION;
    }

    if (mSynchronousMode != enabled) {
        // - if we're going to asynchronous mode, the queue is guaranteed to be
        // empty here
        // - if the client set the number of buffers, we're guaranteed that
        // we have at least 3 (because we don't allow less)
        // This is not supported yet.
        // TODO: to see if this can be supported in future.
        mSynchronousMode = enabled;
        ALOGE("EglStreamTexture setting synch mode %s \n", enabled?"enabled":"disabled");
        mDequeueCondition.signal();
    }
    return OK;
}

status_t EglStreamTexture::connect(int api, QueueBufferOutput* output) {

    ALOGV("EglStreamTexture::connect 0x%x ", api);
    Mutex::Autolock lock(mMutex);

    if (mStopped || bInitFailed) {
        ALOGE("Connect: EglStreamTexture has been stopped!");
        return NO_INIT;
    }

    status_t err = NO_ERROR;
    switch (api) {
        case NATIVE_WINDOW_API_EGL:
        case NATIVE_WINDOW_API_CPU:
        case NATIVE_WINDOW_API_MEDIA:
        case NATIVE_WINDOW_API_CAMERA:
            if (mConnectedApi != NO_CONNECTED_API) {
                ALOGE("%s[%d] error \n", __FUNCTION__, __LINE__);
                err = -EINVAL;
            } else {
                mConnectedApi = api;
                output->inflate(mDefaultWidth, mDefaultHeight, 0, 0);
            }
            break;
        default:
            ALOGE("%s[%d] error \n", __FUNCTION__, __LINE__);
            err = -EINVAL;
            break;
    }
    return err;
}

// This is called by the client side when it is done
// TODO: Currently, this also sets mStopped to true which
// is needed for unblocking the encoder which might be
// waiting to read more frames. So if on the client side,
// the same thread supplies the frames and also calls stop
// on the encoder, the client has to call disconnect before
// it calls stop.
// In the case of the camera,
// that need not be required since the thread supplying the
// frames is separate than the one calling stop.
status_t EglStreamTexture::disconnect(int api) {
    ALOGV("EglStreamTexture::disconnect api 0x%x + \n", api);
    Mutex::Autolock lock(mMutex);

    if (mStopped) {
        ALOGE("disconnect: SurfaceMediaSoource is already stopped!");
        return NO_INIT;
    }

    status_t err = NO_ERROR;
    switch (api) {
        case NATIVE_WINDOW_API_EGL:
        case NATIVE_WINDOW_API_CPU:
        case NATIVE_WINDOW_API_MEDIA:
        case NATIVE_WINDOW_API_CAMERA:
            if (mConnectedApi == api) {
                mConnectedApi = NO_CONNECTED_API;
                mDequeueCondition.signal();
                eglExports.streamProducerDisconnect((NvEglStreamHandle)mEglStream);
            } else {
                ALOGE("%s[%d] error \n", __FUNCTION__, __LINE__);
                err = -EINVAL;
            }
            break;
        default:
            ALOGE("%s[%d] error \n", __FUNCTION__, __LINE__);
            err = -EINVAL;
            break;
    }
    ALOGV("EglStreamTexture::disconnect api 0x%x -  \n", api);
    return err;
}

status_t EglStreamTexture::queueBuffer(int bufIndex,
            const QueueBufferInput& input, QueueBufferOutput* output) {
    ALOGV("queueBuffer %d ", bufIndex);
    NvEglApiStreamFrame streamFrame;
    {
        Mutex::Autolock lock(mMutex);
        Rect crop;
        uint32_t transform;
        int scalingMode;
        int64_t timestamp;
        sp<Fence> fence;

        input.deflate(&timestamp, &crop, &scalingMode, &transform, &fence);
        output->inflate(mDefaultWidth, mDefaultHeight, 0, 1);

        if (bufIndex < 0 || bufIndex >= mBufferCount) {
            ALOGE("queueBuffer: slot index out of range [0, %d]: %d",
                    mBufferCount, bufIndex);
            return -EINVAL;
        } else if (mSlots[bufIndex].mBufferState != BufferSlot::DEQUEUED) {
            ALOGE("queueBuffer: slot %d is not owned by the client (state=%d)",
                    bufIndex, mSlots[bufIndex].mBufferState);
            return -EINVAL;
        } else if (!mSlots[bufIndex].mRequestBufferCalled) {
            ALOGE("queueBuffer: slot %d was enqueued without requesting a "
                    "buffer", bufIndex);
            return -EINVAL;
        }

        mNumFramesReceived++;

        mSlots[bufIndex].mBufferState = BufferSlot::QUEUED;
        mSlots[bufIndex].mTimestamp = timestamp;

        NvError status = NvSuccess;
        NvNativeHandle *nvanb = (NvNativeHandle *)mSlots[bufIndex].mGraphicBuffer->handle;

        memset(&streamFrame, 0, sizeof(streamFrame));
        if (!GrModule)
        {
            hw_module_t const *hwMod;
            hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &hwMod);
            GrModule = (NvGrModule *)hwMod;
        }
        streamFrame.fence.SyncPointID = NVRM_INVALID_SYNCPOINT_ID;
        if (0 == GrModule->Base.lock(&GrModule->Base,
             (const native_handle_t*)nvanb,
             GRALLOC_USAGE_HW_TEXTURE, 0, 0,
             mSlots[bufIndex].mGraphicBuffer->width,
             mSlots[bufIndex].mGraphicBuffer->height, NULL))
        {
            NvRmFence fences[NVGR_MAX_FENCES];
            int numFences = NVGR_MAX_FENCES;

            GrModule->getfences(&GrModule->Base, (const native_handle_t*)nvanb,
                fences, &numFences);
            ALOGV("%s[%d] getting fence id %d value %d num %d \n",
                 __FUNCTION__, __LINE__,
                 fences[0].SyncPointID, fences[0].Value, numFences);
            if (numFences > 0)
            {
                streamFrame.fence.SyncPointID = fences[0].SyncPointID;
                streamFrame.fence.Value = fences[0].Value;
            }
            GrModule->Base.unlock(&GrModule->Base, (const native_handle_t*)nvanb);
            for (int i = 1; i < numFences; i++)
            {
                NvRmFence* fence = &fences[i];
                ALOGV("### %s[%d] wait fence id %d value %d \n", __FUNCTION__, __LINE__,
                    fence->SyncPointID,
                    fence->Value);
                if(NvSuccess != NvRmChannelSyncPointWaitTimeout(m_hRmDev,
                                    fence->SyncPointID,
                                    fence->Value,
                                    mWaitSem,
                                    350))
                    ALOGI("NvRmChannelSyncPointWaitTimeout wait timed out\n");
            }
        }
        streamFrame.info.bufCount =  nvanb->SurfCount;

        memcpy((void *) streamFrame.info.buf, (void *) nvanb->Surf, sizeof(nvanb->Surf));

        streamFrame.info.memFd = NvRmMemGetFd(nvanb->Surf[0].hMem);
        for (unsigned int j = 0; j < streamFrame.info.bufCount; j++)
        {
            streamFrame.info.buf[j].hMem = NULL;
        }

        // set for identification from return frame call
        streamFrame.native = (void *)mSlots[bufIndex].mGraphicBuffer->handle;
        streamFrame.time = (NvU64)mSlots[bufIndex].mTimestamp;

        mCurrentBuf = mSlots[bufIndex].mGraphicBuffer;
        int64_t currentTimestamp = mSlots[bufIndex].mTimestamp;

        ALOGV("Frames rendered = %d, timestamp = %lld ",
            mNumFramesReceived, currentTimestamp / 1000);
    }
    if (mEglStream != EGL_NO_STREAM_KHR)
    {
        ALOGV("bufIndex %d queueBuffer: streamProducerPresentFrame time %.3f \n", bufIndex, streamFrame.time/1E9);
        if (eglExports.streamProducerPresentFrame((NvEglStreamHandle)mEglStream, &streamFrame) == NvError_InvalidState) {
            ALOGE("queueBuffer: streamProducerPresentFrame returned error");
            mSlots[bufIndex].mBufferState = BufferSlot::FREE;
        }
    }
    return OK;
}

void EglStreamTexture::cancelBuffer(int bufIndex, sp<Fence> fence) {
    ALOGV("EglStreamTexture::cancelBuffer %d \n", bufIndex);
    Mutex::Autolock lock(mMutex);
    if (bufIndex < 0 || bufIndex >= mBufferCount) {
        ALOGE("cancelBuffer: slot index out of range [0, %d]: %d",
                mBufferCount, bufIndex);
        return;
    } else if (mSlots[bufIndex].mBufferState != BufferSlot::DEQUEUED) {
        ALOGE("cancelBuffer: slot %d is not owned by the client DEQUEUED (state=%d)",
                bufIndex, mSlots[bufIndex].mBufferState);
        return;
    }
    mSlots[bufIndex].mBufferState = BufferSlot::FREE;
    mDequeueCondition.signal();
}

void EglStreamTexture::freeAllBuffersLocked() {
    ALOGV("freeAllBuffersLocked");
    for (int i = 0; i < NUM_BUFFER_SLOTS; i++) {
        mSlots[i].mGraphicBuffer = 0;
        mSlots[i].mBufferState = BufferSlot::FREE;
    }
}

int EglStreamTexture::query(int what, int* outValue)
{
    ALOGV("query");
    Mutex::Autolock lock(mMutex);
    int value;
    bool result = true;
    switch (what) {
    case NATIVE_WINDOW_WIDTH:
        value = mDefaultWidth;
        // TODO: Revise this code.
        if (!mDefaultWidth && !mDefaultHeight && mCurrentBuf != 0)
            value = mCurrentBuf->width;
        break;
    case NATIVE_WINDOW_HEIGHT:
        value = mDefaultHeight;
        if (!mDefaultWidth && !mDefaultHeight && mCurrentBuf != 0)
            value = mCurrentBuf->height;
        break;
    case NATIVE_WINDOW_FORMAT:
        value = mPixelFormat;
        break;
    case NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS:
        value = mSynchronousMode ?
                (MIN_UNDEQUEUED_BUFFERS-1) : MIN_UNDEQUEUED_BUFFERS;
        break;
#ifdef FRAMEWORK_HAS_DISPLAY_LATENCY_ATTRIBUTE
    case NATIVE_WINDOW_DISPLAY_LATENCY:
        value = 0;
        if (eglQueryStreamKHR)
            result = eglQueryStreamKHR(mDisplay, mEglStream, EGL_CONSUMER_LATENCY_USEC_KHR, &value);
        if ( !result )
        {
            value = 0;
            ALOGE("Failed to query stream state");
        }
        ALOGI("NATIVE_WINDOW_DISPLAY_LATENCY %d \n", value);
        break;
#endif
    default:
        ALOGE("%s[%d] error \n", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }
    outValue[0] = value;
    return NO_ERROR;
}

void EglStreamTexture::dump(String8& result) const
{
    char buffer[1024];
    dump(result, "", buffer, 1024);
}

void EglStreamTexture::dump(String8& result, const char* prefix,
        char* buffer, size_t SIZE) const
{
    Mutex::Autolock _l(mMutex);
    snprintf(buffer, SIZE,
            "%smBufferCount=%d, mSynchronousMode=%d, default-size=[%dx%d], "
            "mPixelFormat=%d, \n",
            prefix, mBufferCount, mSynchronousMode, mDefaultWidth, mDefaultHeight,
            mPixelFormat);
    result.append(buffer);

    struct {
        const char * operator()(int state) const {
            switch (state) {
                case BufferSlot::DEQUEUED: return "DEQUEUED";
                case BufferSlot::QUEUED: return "QUEUED";
                case BufferSlot::FREE: return "FREE";
                default: return "Unknown";
            }
        }
    } stateName;

    for (int i = 0; i < mBufferCount; i++) {
        const BufferSlot& slot(mSlots[i]);
        snprintf(buffer, SIZE,
                "%s%s[%02d] state=%-8s, "
                "timestamp=%lld\n",
                prefix, ":", i, stateName(slot.mBufferState),
                slot.mTimestamp
        );
        result.append(buffer);
    }
}

int32_t EglStreamTexture::setEglStreamAttrib(u_int32_t attr, int32_t val)
{
    ALOGI("setEglStreamAttrib attr = %x val =%x", attr, val);
    switch ( attr )
    {
        case EGL_CONSUMER_LATENCY_USEC_KHR:
            mLatency = val;
            break;
        default:
            ALOGV("TO DO Handle %x attrib", attr);
            break;
    }
    ALOGV("EglStreamTexture::setEglStreamAttrib--");

    return NvSuccess;
}
int32_t EglStreamTexture::returnEglStreamFrame(void *pFrame)
{
    NvEglApiStreamFrame *frame = (NvEglApiStreamFrame *)pFrame;
    ALOGV("EglStreamTexture::returnEglStreamFrame++");
    if (mStopped) {
        ALOGI("returnEglStreamFrame: mStopped = true! Nothing to do!");
        return NvSuccess;
    }
    bool foundBuffer = false;
    Mutex::Autolock autoLock(mMutex);
    ALOGV("EglStreamTexture::returnEglStreamFrame++ next ");
    for (int id = 0; id < NUM_BUFFER_SLOTS; id++) {
        if (mSlots[id].mGraphicBuffer == NULL) {
            continue;
        }
        // TODO: review this code later.
        if (mSlots[id].mGraphicBuffer->handle == (buffer_handle_t)frame->native)
        {
            if (frame->fence.SyncPointID != NVRM_INVALID_SYNCPOINT_ID)
            {
                ALOGV("%s[%d] fence id %d value %d \n", __FUNCTION__, __LINE__,
                    frame->fence.SyncPointID,
                    frame->fence.Value);
                mSlots[id].mFence.SyncPointID = frame->fence.SyncPointID;
                mSlots[id].mFence.Value = frame->fence.Value;
            }
            mSlots[id].mBufferState = BufferSlot::FREE;
            mDequeueCondition.signal();
            foundBuffer = true;
            ALOGV("EglStreamTexture::Frame returned for slot %d", id);
            break;
        }
    }
    if (!foundBuffer) {
        ALOGE("returnEglStreamFrame: bogus buffer 0x%x \n", (int)frame);
    }

    ALOGV("EglStreamTexture::returnEglStreamFrame--");
    return NvSuccess;
}

int32_t EglStreamTexture::eglStreamDisconnect()
{
    ALOGV("eglStreamDisconnect++");

    ALOGV("eglStreamDisconnect--");
    return NvSuccess;
}

} // end of namespace android
