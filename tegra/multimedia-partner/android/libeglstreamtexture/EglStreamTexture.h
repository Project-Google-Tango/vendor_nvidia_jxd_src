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

#ifndef ANDROID_GUI_EGLSTREAMTEXTURE_H
#define ANDROID_GUI_EGLSTREAMTEXTURE_H

#include <gui/ISurfaceTexture.h>

#include <utils/threads.h>
#include <utils/Vector.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_nv.h>
#include <nvos.h>
#include <nvrm_channel.h>

namespace android {
// ----------------------------------------------------------------------------

class IGraphicBufferAlloc;
class String8;
class GraphicBuffer;

class EglStreamTexture : public BnSurfaceTexture {
public:
    enum { MIN_UNDEQUEUED_BUFFERS = 2 };
    enum {
        MIN_ASYNC_BUFFER_SLOTS = MIN_UNDEQUEUED_BUFFERS + 1,
        MIN_SYNC_BUFFER_SLOTS  = MIN_UNDEQUEUED_BUFFERS
    };
    enum { NUM_BUFFER_SLOTS = 32 };
    enum { NO_CONNECTED_API = 0 };

    EglStreamTexture(EGLStreamKHR stream, EGLDisplay dpy);

    virtual ~EglStreamTexture();

    //EGL Stream imports
    int32_t setEglStreamAttrib(u_int32_t attr, int32_t val);
    int32_t returnEglStreamFrame(void *frame);
    int32_t eglStreamDisconnect();

    uint32_t getBufferCount( ) const { return mBufferCount;}


    // setBufferCount updates the number of available buffer slots.  After
    // calling this all buffer slots are both unallocated and owned by the
    // EglStreamTexture object (i.e. they are not owned by the client).
    virtual status_t setBufferCount(int bufferCount);

    virtual status_t requestBuffer(int slot, sp<GraphicBuffer>* buf);

    // dequeueBuffer gets the next buffer slot index for the client to use. If a
    // buffer slot is available then that slot index is written to the location
    // pointed to by the buf argument and a status of OK is returned.  If no
    // slot is available then a status of -EBUSY is returned and buf is
    // unmodified.
    virtual status_t dequeueBuffer(int *buf, sp<Fence>& fence,
            uint32_t w, uint32_t h, uint32_t format, uint32_t usage);

    // queueBuffer returns a filled buffer to the EglStreamTexture. In addition, a
    // timestamp must be provided for the buffer. The timestamp is in
    // nanoseconds, and must be monotonically increasing. Its other semantics
    // (zero point, etc) are client-dependent and should be documented by the
    // client.
    virtual status_t queueBuffer(int slot,
                const QueueBufferInput& input, QueueBufferOutput* output);
    virtual void cancelBuffer(int buf, sp<Fence> fence);

    virtual status_t setScalingMode(int mode) { return OK; }
    virtual int query(int what, int* value);

    // Just confirming to the ISurfaceTexture interface as of now
    virtual status_t setCrop(const Rect& reg) { return OK; }
    virtual status_t setTransform(uint32_t transform) {return OK;}

    // setSynchronousMode set whether dequeueBuffer is synchronous or
    // asynchronous. In synchronous mode, dequeueBuffer blocks until
    // a buffer is available, the currently bound buffer can be dequeued and
    // queued buffers will be retired in order.
    // The default mode is synchronous.
    // TODO: Clarify the minute differences bet sycn /async
    // modes (S.Encoder vis-a-vis SurfaceTexture)
    virtual status_t setSynchronousMode(bool enabled);

    // connect attempts to connect a client API to the EglStreamTexture.  This
    // must be called before any other ISurfaceTexture methods are called except
    // for getAllocator.
    //
    // This method will fail if the connect was previously called on the
    // EglStreamTexture and no corresponding disconnect call was made.
    virtual status_t connect(int api, QueueBufferOutput* output);


    // disconnect attempts to disconnect a client API from the EglStreamTexture.
    // Calling this method will cause any subsequent calls to other
    // ISurfaceTexture methods to fail except for getAllocator and connect.
    // Successfully calling connect after this will allow the other methods to
    // succeed again.
    //
    // This method will fail if the the EglStreamTexture is not currently
    // connected to the specified client API.
    virtual status_t disconnect(int api);

    // setBufferCountServer set the buffer count. If the client has requested
    // a buffer count using setBufferCount, the server-buffer count will
    // take effect once the client sets the count back to zero.
    status_t setBufferCountServer(int bufferCount);

    // dump our state in a String
    void dump(String8& result) const;
    void dump(String8& result, const char* prefix, char* buffer,
                                                    size_t SIZE) const;
    void close();

protected:

    // freeAllBuffersLocked frees the resources (both GraphicBuffer and EGLImage) for
    // all slots.
    void freeAllBuffersLocked();

private:

    status_t setBufferCountServerLocked(int bufferCount);

    enum { INVALID_BUFFER_SLOT = -1 };

    struct BufferSlot {

        BufferSlot()
            : mBufferState(BufferSlot::FREE),
              mRequestBufferCalled(false),
              mTimestamp(0) {
            mFence.SyncPointID = NVRM_INVALID_SYNCPOINT_ID;
        }

        // mGraphicBuffer points to the buffer allocated for this slot or is
        // NULL if no buffer has been allocated.
        sp<GraphicBuffer> mGraphicBuffer;

        // BufferState represents the different states in which a buffer slot
        // can be.
        enum BufferState {
            // FREE indicates that the buffer is not currently being used and
            // will not be used in the future until it gets dequeued and
            // subseqently queued by the client.
            FREE = 0,

            // DEQUEUED indicates that the buffer has been dequeued by the
            // client, but has not yet been queued or canceled. The buffer is
            // considered 'owned' by the client, and the server should not use
            // it for anything.
            //
            // Note that when in synchronous-mode (mSynchronousMode == true),
            // the buffer that's currently attached to the texture may be
            // dequeued by the client.  That means that the current buffer can
            // be in either the DEQUEUED or QUEUED state.  In asynchronous mode,
            // however, the current buffer is always in the QUEUED state.
            DEQUEUED = 1,

            // QUEUED indicates that the buffer has been queued by the client,
            // and has not since been made available for the client to dequeue.
            // Attaching the buffer to the texture does NOT transition the
            // buffer away from the QUEUED state. However, in Synchronous mode
            // the current buffer may be dequeued by the client under some
            // circumstances. See the note about the current buffer in the
            // documentation for DEQUEUED.
            QUEUED = 2,
        };

        // mBufferState is the current state of this buffer slot.
        BufferState mBufferState;

        // mRequestBufferCalled is used for validating that the client did
        // call requestBuffer() when told to do so. Technically this is not
        // needed but useful for debugging and catching client bugs.
        bool mRequestBufferCalled;

        // mTimestamp is the current timestamp for this buffer slot. This gets
        // to set by queueBuffer each time this slot is queued.
        int64_t mTimestamp;
        // eglstream will set to this fence to notify when frame is ready for
        // next write.
        NvRmFence mFence;
    };

    // mSlots is the array of buffer slots that must be mirrored on the client
    // side. This allows buffer ownership to be transferred between the client
    // and server without sending a GraphicBuffer over binder. The entire array
    // is initialized to NULL at construction time, and buffers are allocated
    // for a slot when requestBuffer is called with that slot's index.
    BufferSlot mSlots[NUM_BUFFER_SLOTS];

    // mDefaultWidth holds the default width of allocated buffers. It is used
    // in requestBuffers() if a width and height of zero is specified.
    uint32_t mDefaultWidth;

    // mDefaultHeight holds the default height of allocated buffers. It is used
    // in requestBuffers() if a width and height of zero is specified.
    uint32_t mDefaultHeight;

    // mPixelFormat holds the pixel format of allocated buffers. It is used
    // in requestBuffers() if a format of zero is specified.
    uint32_t mPixelFormat;

    // mBufferCount is the number of buffer slots that the client and server
    // must maintain. It defaults to MIN_ASYNC_BUFFER_SLOTS and can be changed
    // by calling setBufferCount or setBufferCountServer
    int mBufferCount;

    // mClientBufferCount is the number of buffer slots requested by the
    // client. The default is zero, which means the client doesn't care how
    // many buffers there are
    int mClientBufferCount;

    // mServerBufferCount buffer count requested by the server-side
    int mServerBufferCount;

    // mCurrentBuf is the graphic buffer of the current slot to be used by
    // buffer consumer. It's possible that this buffer is not associated
    // with any buffer slot, so we must track it separately in order to
    // properly use IGraphicBufferAlloc::freeAllGraphicBuffersExcept.
    sp<GraphicBuffer> mCurrentBuf;

    // mGraphicBufferAlloc is the connection to SurfaceFlinger that is used to
    // allocate new GraphicBuffer objects.
    sp<IGraphicBufferAlloc> mGraphicBufferAlloc;

    // mSynchronousMode whether we're in synchronous mode or not
    bool mSynchronousMode;

    // mConnectedApi indicates the API that is currently connected to this
    // SurfaceTexture.  It defaults to NO_CONNECTED_API (= 0), and gets updated
    // by the connect and disconnect methods.
    int mConnectedApi;

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

    int     mLatency;

    EGLStreamKHR mEglStream;
    EGLDisplay mDisplay;
    NvRmDeviceHandle m_hRmDev;
    NvOsSemaphoreHandle     mWaitSem;
    bool bInitFailed;

    EglStreamWrapper  mWrapper;

    // Avoid copying and equating and default constructor
    //DISALLOW_IMPLICIT_CONSTRUCTORS(EglStreamTexture);
};

// ----------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_GUI_EGLSTREAMTEXTURE_H
