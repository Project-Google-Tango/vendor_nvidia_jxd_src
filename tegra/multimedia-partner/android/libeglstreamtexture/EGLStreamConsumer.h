/*
 * Copyright (C) 2012-2013 NVIDIA Corporation
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

#ifndef ANDROID_GUI_EGLSTREAMCONSUMER_H
#define ANDROID_GUI_EGLSTREAMCONSUMER_H

#include <gui/ISurfaceTexture.h>

#include <utils/threads.h>
#include <utils/Vector.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_nv.h>

#include <nvos.h>
#include <nvrm_channel.h>
#include <utils/List.h>
#include "eglapiinterface.h"
#include <nvimagescaler.h>
#include "EglStreamTextureWrapper.h"
#include "EglStreamWrapper.h"
#include <binder/MemoryDealer.h>

namespace android {

#define DISALLOW_EVIL_CONSTRUCTORS_1(name) \
    name(const name &); \
    name &operator=(const name &)

class IGraphicBufferAlloc;
class String8;
class GraphicBuffer;
class NvImageScaler;


class EglStreamConsumerAPI : public virtual RefBase  {
public:
    EglStreamConsumerAPI() {}
    virtual ~EglStreamConsumerAPI() {}

    // to get next frame for recording
    virtual sp <IMemory>  getNextFrame(void *pTimestamp /* ts of retframe */,
                                       void *pRetStatus /* eFrameReqStatus */,
                                       void* pTimeOut   /* NvU64 */) = 0;

    // to return the recorded frame
    virtual status_t returnRecordedFrame(sp<IMemory>& frame) = 0;

    // to initialize
    virtual status_t init(void) = 0;

    // to stop
    virtual status_t stop(void) = 0;

private:
    EglStreamConsumerAPI(const EglStreamConsumerAPI &);
    EglStreamConsumerAPI &operator=(const EglStreamConsumerAPI &);
};

class EGLStreamConsumer: public EglStreamConsumerAPI {

public:

    EGLStreamConsumer(EGLStreamKHR stream, EGLDisplay dpy,
                                int nWidth, /* consumer width */
                                int nHeight /* consumer height */);

    virtual ~EGLStreamConsumer();

    //EGL Stream imports
    int32_t setEglStreamAttrib(u_int32_t attr, int32_t val);
    int32_t eglStreamDisconnect();

    // dump our state in a String
    void dump(String8& result) const;
    void dump(String8& result, const char* prefix, char* buffer,
        size_t SIZE) const;
    void close();

     // to get next frame for recording
    sp <IMemory>  getNextFrame(void *pTimestamp /* ts of retframe */,
                                void *pRetStatus /* eFrameReqStatus */,
                                void* pTimeOut   /* NvU64 */);

    // to return the recorded frame
    status_t returnRecordedFrame(sp<IMemory>& frame) ;

    // to initialize
    status_t init(void);

    // to stop
    status_t stop(void);

private:

    // mDefaultWidth holds the default width of allocated buffers. It is used
    // in requestBuffers() if a width and height of zero is specified.
    uint32_t mDefaultWidth;

    // mDefaultHeight holds the default height of allocated buffers. It is used
    // in requestBuffers() if a width and height of zero is specified.
    uint32_t mDefaultHeight;

    // mPixelFormat holds the pixel format of allocated buffers. It is used
    // in requestBuffers() if a format of zero is specified.
    uint32_t mPixelFormat;

    // mSynchronousMode whether we're in synchronous mode or not
    bool mSynchronousMode;

    // mDequeueCondition condition used for dequeueBuffer in synchronous mode
    mutable Condition mDequeueCondition;

    // mMutex is the mutex used to prevent concurrent access to the member
    // variables of EglStreamTexture objects. It must be locked whenever the
    // member variables are accessed.
    mutable Mutex mMutex;


    // mStopped is a flag to check if the recording is going on
    bool mStopped;

    // mNumFramesReceived indicates the number of frames recieved from
    // the client side
    int mNumFramesReceived;

    // mFirstFrameTimestamp is the timestamp of the first received frame.
    // It is used to offset the output timestamps so recording starts at time 0.
    int64_t mFirstFrameTimestamp;
    // mStartTimeNs is the start time passed into the source at start, used to
    // offset timestamps.
    int64_t mStartTimeNs;
    int     mLatency;

    EGLStreamKHR mEglStream; //set by user of this class
    EGLDisplay mDisplay;//set by user of this class
    bool mStarted;
    pthread_t mThread;

    status_t start();

    enum {
        kNumBuffers = 4,
        kBufferSize = 4 + 80 + 224  // 4 + sizeof(OMX_BUFFERHEADERTYPE) + sizeof(NvMMBuffer)
    };

    // actual entry point for the thread
    void readFromSource();

    // static function to wrap the actual thread entry point
    static void *threadWrapper(void *pthis);


    enum eFrameState {
        kFREE = 0,
        kFILLED = 1,
        kGIVEN_TO_CLIENT = 2,
    };
    //Frame
    //  <const + OMXBUFHDR + NVMMBUFFER >+ <ID> + <status>
    //   state: Free -- Filled -- GivenToRecorder.
    typedef struct AframeRec {
        sp<IMemory> mPtr;
        int mFrameId;
        eFrameState status; // 0 - Free 1-Filled 2-givenToRecorder
        // timestamp given by eglstream
        NvU64 timestamp;
    } Aframe;

    //FramesList  -- book keeping purpose!
    List<Aframe*> mFramesList;

    //FreeList
    List<Aframe*> mFreeFramesList; //when started freelist is eq to FramesList
    //cond; mutex
    mutable Condition mFreeListCondition;
    mutable Mutex     mFreeListMutex;

    //FilledList
    List<Aframe*> mFilledFramesList;
    //cond; mutex
    mutable Condition mFilledListCondition;
    mutable Mutex     mFilledListMutex;

    int  AllocateFrameFully(Aframe *pFrame);
    int  DeAllocateFrameFully(Aframe *pFrame);
    int mEnableFullFlow;

    EglStreamWrapper mWrapper;
    NvRmDeviceHandle m_hRmDev;
    NvOsSemaphoreHandle     mWaitSem;
    NvImageScaler m_Converter;

    void DumpSurfaces(NvMMBuffer *pMMBuffer,NvxSurface *pargSurface);

    sp<MemoryDealer> mMemoryDealer;
    NvEglApiStreamFrame *pStreamFrame;
    int mFramesGiven;
    int mFramesReceivedBack;
    DISALLOW_EVIL_CONSTRUCTORS_1(EGLStreamConsumer);
};

// ----------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_GUI_EGLSTREAMCONSUMER_H
