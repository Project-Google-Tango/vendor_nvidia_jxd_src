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

#ifndef _NV_CAMERA_STREAMPORT_H_
#define _NV_CAMERA_STREAMPORT_H_

/* StreamPort threads will be created during NvCameraCore::confStreamPort()
 * call.
 * Each StreamPort is associated with each Stream that was registered in the
 * configure_stream call. After the creation the threads immediately go to sleep
 * and are woken up after the router gets the result back from the DZ.
 *
 * After that, each streamport checks if it needs to blit from a source buffer,
 * or it needs to scale or perform jpeg encoding etc. This depends on the output
 * format that is required by the Stream that streamport is associated with.
 *
 * The stream that holds the source buffer waits until other streams that need
 * to blit/scale/jpeg encode from it (dependent streams) are done. This
 * dependency is maintained in thru refcounts, this refcount is a member of the
 * RouterBuffer class in nvcamerahal3core.h.
 *
 * After the required conversions are done and the buffer is ready, each
 * StreamPort returns the buffer to the upper-layer (hal3core) which sends it
 * the layer above it (hal), which returns the result to the frameworks.
 */
#include <nverror.h>
#include <nvassert.h>
#include <nvos.h>

#include <utils/RefBase.h>
#include <utils/Timers.h>
#include <utils/Condition.h>
#include <utils/Mutex.h>
#include <utils/List.h>
#include <utils/Thread.h>

#include "nvabstracts.h"
#include "nvformatconverter.h"
#include "nvcamerahal3common.h"
#include "nvmemallocator.h"
#include "nvcamera_core.h"
#include "nvcamerahal3jpegprocessor.h"

namespace android {

class StreamPort;
class JpegProcessor;
struct RouterBuffer
{
    buffer_handle_t *buffer;
    NvMMBuffer *nvmm_buffer;
    NvBool isCropped;
    NvRectF32 rect;
    sp<StreamPort> owner;
    uint32_t refcount;
    Mutex lock;
    NvOsSemaphoreHandle sema;

    RouterBuffer()
    {
        NvOsSemaphoreCreate(&sema, 0);
        refcount = 0;
        owner = NULL;
    }

    ~RouterBuffer()
    {
        NvOsSemaphoreDestroy(sema);
    }

    void RefInc()
    {
        Mutex::Autolock l(lock);
        refcount++;
    }

    void RefDec()
    {
        Mutex::Autolock l(lock);
        if(refcount)
            refcount--;
        if(refcount==0)
               NvOsSemaphoreSignal(sema);
    }

    void Wait()
    {
        lock.lock();
        while(refcount)
        {
            lock.unlock();
            NvOsSemaphoreWait(sema);
            lock.lock();
        };
        lock.unlock();
    }
};

typedef enum {
    BUFFERINFO_STATE_INITIAL,
    BUFFERINFO_STATE_JPEG_START,
    BUFFERINFO_STATE_JPEG_WAIT,
    BUFFERINFO_STATE_READY
}BUFFERINFO_STATE;

struct BufferInfo
{
    camera3_stream_buffer *outBuffer;
    RouterBuffer *inBuffer;
    uint32_t frameNumber;
    NvCameraHal3_JpegSettings *pJpegSettings;
    BUFFERINFO_STATE state;
};

typedef List<BufferInfo*> BufferInfoQueue;
typedef List<buffer_handle_t*> ANBQueue;

class StreamPort : public Thread, public AbsCaller
{
public:
    StreamPort(camera3_stream_t *stream, NvCameraStreamType _type, NvColorFormat SensorFormat);
    ~StreamPort();

public:
    camera3_stream_t *getStream() const { return mStream; };
    uint32_t streamFormat() const { return mStream->format; };

    bool is_alive() const { return alive; };
    void set_alive(bool _alive) { alive = _alive; };

    int format() const { return mStream->format; };

    NvColorFormat getSensorFormat() const { return mSensorFormat; };

    bool isBayer() const { return mIsBayer; };

    uint32_t width() const { return mStream->width; };
    uint32_t height() const { return mStream->height; };
    uint32_t max_buffers() const { return mStream->max_buffers; };

    NvCameraStreamType streamType() const { return type; };

    NvU32 bufferCount() const { return mNumBuf; };
    bool streamRegistered() const { return registered; };

    NvError linkBuffers(buffer_handle_t **buffers, NvU32 count);
    void unlinkBuffers(NvCameraCoreHandle hNvCameraCore);

    NvMMBuffer *NativeToNVMMBuffer(buffer_handle_t *buffer);
    NvMMBuffer* NativeToNVMMBuffer(buffer_handle_t *buffer,
        NvBool *isCropped, NvRectF32 *pRect);
    buffer_handle_t* NvMMBufferToNative(NvMMBuffer *buffer);
    NvError processResult(uint32_t frameNumber, RouterBuffer *inBuffer,
            const NvCamProperty_Public_Dynamic *frameDynamicProps);
    bool isOwner(buffer_handle_t *buffer);

    void waitAvailableBuffers();
    void flushBuffers();
    NvError enqueueOutputBuffer(camera3_stream_buffer *buffer, int frameNumber,
            const NvCameraHal3_Public_Controls& prop);

    void setBypass(bool pass){ bypass = pass;};
    buffer_handle_t *getZoomBuffer();
    void setCropInZoomBuffer(buffer_handle_t *anb, NvRectF32 *pRect);
    NvError notifyJpegDone(BufferInfo *pOutBufferInfo, NvMMBuffer *thumbnailBuffer);
    NvMMBuffer *getTNRBuffer();

private:
    NvError InitializeNvMMBufferWithANB(CameraBuffer *buffer);
    NvError InitializeANBWithNvMMBuffer(CameraBuffer *buffer);
    NvError doCropAndScale(NvMMBuffer *pNvMmSrc, NvRectF32 rect, NvMMBuffer *pNvMMDst);

    virtual bool threadLoop();
    NvError dequeueOutputBuffer();
    void sendResult(camera3_stream_buffer *buffer, int frame, bool status);
    NvError prepareJpegEncoding(NvMMBuffer *pNvMMSrc, NvRectF32 rect,
        NvMMBuffer *pNvMMDst, NvMMBuffer **ppThumbnailBuffer,
        NvCameraHal3_JpegSettings *pJpegSettings);

    void pushZoomBuffer(buffer_handle_t *anb);

    void reportError(int frame, NvError error);
    void setCropRegion(RouterBuffer *inBuffer,
        NvMMBuffer *inNvmmBuffer, NvRectF32 *pRect);
    NvError constructThumbnailBuffer(NvJpegEncParameters *params,
        NvMMBuffer *pThumbnailNvMMBuffer, NvMMBuffer *Buffer);

    camera3_error_msg_code_t nvErrorToHal3Error(NvError error);

protected:
    bool alive;
    bool bypass;
    NvU32 mNumBuf;
    bool registered;
    bool mIsBayer;
    NvColorFormat mSensorFormat;
    NvCameraStreamType type;
    camera3_stream_t *mStream;
    CameraBuffer* cameraBuffer;

    NvOsSemaphoreHandle mAvailableSema;
    NvOsSemaphoreHandle mFlushSema;
    bool mFlushing;
    // FIFO list of output buffers
    BufferInfoQueue OutputQ;
    ANBQueue mZoomBufferQ;
    FormatConverter mConverter;
    Mutex bufferQMutex;
    Mutex zoomBufferQMutex;
    NvOsSemaphoreHandle mWorkSema;

    static int offset_id;
    static Mutex offs_mutx;

    sp<JpegProcessor> mJpegProcessor;

    NvMMBuffer mTNRBuffer;
    bool mTNRAllocated;
};

}

#endif
