/*
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * Copyright (C) 2013 The Android Open Source Project
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
#include "nvcamerahal3.h"
#include "nvrm_init.h"
#include <ui/Fence.h>
#include "nvcamerahal3_tags.h"
#include "nv_log.h"
#include "camera_trace.h"

namespace android {

DefaultKeyedVector< int, const camera_metadata_t * >  NvCameraHal3::staticInfoMap(NULL);

status_t NvCameraHal3::getStaticCharacteristics(int cameraId,
                        int orientation,
                        const camera_metadata_t* &cameraInfo)
{
    status_t res;

    NV_TRACE_CALL_D(HAL3_TAG);
    NV_LOGD(HAL3_TAG, "%s: ++ for cameraId=%d",__FUNCTION__, cameraId);

    if (staticInfoMap.valueFor(cameraId) != NULL)
    {
        cameraInfo = staticInfoMap.valueFor(cameraId);
        NV_LOGV(HAL3_TAG, "%s: existing cameraInfo[%p] for cameraId=%d with size=%d", __FUNCTION__,
                cameraInfo, cameraId, staticInfoMap.size());
        return OK;
    }

    // Populate static info for the first time
    res = NvCameraHal3Core::getStaticInfo(cameraId, orientation, cameraInfo);
    if (res != OK)
    {
        NV_LOGE("%s: Unable to allocate static info: %s (%d)",
            __FUNCTION__, strerror(-res), res);
        res = -ENODEV;
    }

    staticInfoMap.add(cameraId, cameraInfo);
    NV_LOGV(HAL3_TAG, "%s: new cameraInfo[%p] inserted at key[%d]",
            __FUNCTION__, cameraInfo, cameraId);

    NV_LOGD(HAL3_TAG, "%s: -- for cameraId=%d with size = %d",__FUNCTION__, cameraId,
            staticInfoMap.size());
    return res;
}

NvCameraHal3::NvCameraHal3(NvU32 sensorId, camera3_device_t *dev)
    :mSensorId(sensorId),
    mStatus(STATUS_CLOSED),
    mNvCameraCore(this, sensorId, staticInfoMap.valueFor(sensorId), 0)
{
    NV_TRACE_CALL_D(HAL3_TAG);
    NV_LOGD(HAL3_TAG, "%s: ++",__FUNCTION__);

    mNvCameraCore.registerCallback(this);

    NV_LOGD(HAL3_TAG, "%s: --",__FUNCTION__);
}

NvCameraHal3::~NvCameraHal3()
{
    NV_TRACE_CALL_D(HAL3_TAG);
    NV_LOGD(HAL3_TAG, "%s: ++",__FUNCTION__);

    mStatus = STATUS_CLOSED;

    NV_LOGD(HAL3_TAG, "%s: --",__FUNCTION__);
}

const camera_metadata_t* NvCameraHal3::constructDefaultRequestSettings(
        int type)
{
    Mutex::Autolock l(mLock);
    return mNvCameraCore.getDefaultRequestSettings(type);
}

status_t NvCameraHal3::open()
{
    NV_TRACE_CALL_D(HAL3_TAG);
    NV_LOGD(HAL3_TAG, "%s: ++",__FUNCTION__);
    Mutex::Autolock lock(mLock);
    status_t ret = OK;
    if (mNvCameraCore.open() == NvSuccess)
    {
        mStatus = STATUS_OPEN;
    }
    else
    {
        ret = NO_INIT;
        mStatus = STATUS_ERROR;
        NV_LOGE("%s: Camera Open Failed", __FUNCTION__);
    }
    NV_LOGD(HAL3_TAG, "%s: --",__FUNCTION__);
    return ret;
}

status_t NvCameraHal3::initialize(
         const camera3_callback_ops_t *callback_ops)
{
    NV_TRACE_CALL_D(HAL3_TAG);
    NV_LOGD(HAL3_TAG, "%s: ++",__FUNCTION__);
    Mutex::Autolock lock(mLock);
    status_t ret = OK;
    if (callback_ops != 0)
    {
        mCallbackOps = callback_ops;
        mStatus = STATUS_READY;
    }
    else
    {
        ret = NO_INIT;
        mStatus = STATUS_ERROR;
    }
    NV_LOGD(HAL3_TAG, "%s: --",__FUNCTION__);
    return ret;
}

status_t NvCameraHal3::configureStreams(
        camera3_stream_configuration *streamList)
{
    NvError error;
    Mutex::Autolock lock(mLock);

    NV_TRACE_CALL_D(HAL3_TAG);
    NV_LOGD(HAL3_TAG, "%s: ++",__FUNCTION__);

    if (mStatus != STATUS_OPEN && mStatus != STATUS_READY)
    {
        NV_LOGE("%s: Cannot configure streams in state %d",
                __FUNCTION__, mStatus);
        return NO_INIT;
    }

    /*
     * Sanity-check input list.
     */
    if (streamList == NULL)
    {
        NV_LOGE("%s: NULL stream configuration", __FUNCTION__);
        return BAD_VALUE;
    }

    if (streamList->streams == NULL)
    {
        NV_LOGE("%s: NULL stream list", __FUNCTION__);
        return BAD_VALUE;
    }

    if (streamList->num_streams < 1)
    {
        NV_LOGE("%s: Bad number of streams requested: %d", __FUNCTION__,
                streamList->num_streams);
        return BAD_VALUE;
    }

    camera3_stream_t *inputStream = NULL;
    for (size_t i = 0; i < streamList->num_streams; i++)
    {
        camera3_stream_t *newStream = streamList->streams[i];

        if (newStream == NULL)
        {
            NV_LOGE("%s: Stream index %d was NULL",
                  __FUNCTION__, i);
            return BAD_VALUE;
        }

        NV_LOGV(HAL3_TAG, "%s: stream %p (id %d), type %d, usage 0x%x, format 0x%x width=%d height=%d",
                __FUNCTION__, newStream, i,
                newStream->stream_type, newStream->usage,
                newStream->format, newStream->width, newStream->height);

        if (newStream->stream_type == CAMERA3_STREAM_INPUT ||
            newStream->stream_type == CAMERA3_STREAM_BIDIRECTIONAL)
        {
            if (inputStream != NULL)
            {
                NV_LOGE("%s: Multiple input streams requested!", __FUNCTION__);
                return BAD_VALUE;
            }
            inputStream = newStream;
        }

        bool validFormat = false;
        for (size_t f = 0; f < sizeof(kAvailableFormats)/sizeof(kAvailableFormats[0]); f++)
        {
            if ((uint32_t)newStream->format == kAvailableFormats[f])
            {
              if (newStream->format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)
              {
                    newStream->format = HAL_PIXEL_FORMAT_YV12;
              }

              validFormat = true;
              break;
            }
        }
        if (!validFormat)
        {
            NV_LOGE("%s: Unsupported stream format 0x%x requested",
                    __FUNCTION__, newStream->format);
            return BAD_VALUE;
        }
    }
    mInputStream = inputStream;

    for (size_t i = 0; i < streamList->num_streams; i++)
    {
        camera3_stream_t *newStream = streamList->streams[i];

        switch (newStream->stream_type)
        {
            case CAMERA3_STREAM_OUTPUT:
                newStream->usage |= GRALLOC_USAGE_HW_CAMERA_WRITE;
                break;
            case CAMERA3_STREAM_INPUT:
                newStream->usage |= GRALLOC_USAGE_HW_CAMERA_READ;
                break;
            case CAMERA3_STREAM_BIDIRECTIONAL:
                newStream->usage |= GRALLOC_USAGE_HW_CAMERA_READ |
                        GRALLOC_USAGE_HW_CAMERA_WRITE;
                break;
        }

        newStream->max_buffers =
            mNvCameraCore.getMaxBufferCount(newStream->width, newStream->height);
        if (newStream->format == HAL_PIXEL_FORMAT_BLOB)
        {
            newStream->max_buffers = 2;
            ALOGE("new stream: got a blob, set specific buffer count to %d", newStream->max_buffers);
        }

        ALOGV("%s: new stream %p, priv=%p", __FUNCTION__, newStream, newStream->priv);
        ALOGV("%s: new stream %p, usage %x, tagged as NULL", __FUNCTION__,
            newStream, newStream->usage);
    }

    error = mNvCameraCore.addStreams(streamList->streams,
                        streamList->num_streams);
    if (error != NvSuccess)
    {
        NV_LOGE("%s: Failed to add streams %d",
                __FUNCTION__, streamList->num_streams);
        return BAD_VALUE;
    }

    NV_LOGD(HAL3_TAG, "%s: --",__FUNCTION__);
    return OK;
}

status_t NvCameraHal3::registerStreamBuffers(
    const camera3_stream_buffer_set *bufferSet)
{
    NV_TRACE_CALL_D(HAL3_TAG);
    NV_LOGD(HAL3_TAG, "%s: ++", __FUNCTION__);
    NvError e = NvSuccess;
    Mutex::Autolock lock(mLock);

    NV_LOGV(HAL3_TAG, "%s: stream %p, num_buffers/max_buffers=%d/%d", __FUNCTION__,
        bufferSet->stream,
        bufferSet->num_buffers,
        bufferSet->stream->max_buffers);

    /*
     * Sanity checks
     */

    // OK: register streams at any time during configure
    // (but only once per stream)
    if (mStatus != STATUS_READY && mStatus != STATUS_ACTIVE)
    {
        NV_LOGE("%s: Cannot register buffers in state %d",
                __FUNCTION__, mStatus);
        return NO_INIT;
    }

    if (bufferSet == NULL)
    {
        NV_LOGE("%s: NULL buffer set!", __FUNCTION__);
        return BAD_VALUE;
    }

    if (bufferSet->stream->priv == NULL)
    {
        NV_LOGE("%s: Trying to register buffers for a non-configured stream!",
                __FUNCTION__);
        return BAD_VALUE;
    }

    e = mNvCameraCore.addBuffers(bufferSet->stream, bufferSet->buffers,
            bufferSet->num_buffers);
    if (e != NvSuccess)
    {
        NV_LOGE("%s: Failed to add buffers %d",
                __FUNCTION__, bufferSet->num_buffers);
        return NO_MEMORY;
    }

    NV_LOGD(HAL3_TAG, "%s: --", __FUNCTION__);
    return OK;
}


/*
 * mLock should be acquired before calling this.
 */
status_t NvCameraHal3::processCaptureRequest(
        camera3_capture_request *request)
{
    Mutex::Autolock l(mLock);
    status_t res;

    NV_LOGD(HAL3_TAG, "%s: ++", __FUNCTION__);

    /* Validation */
    if (mStatus < STATUS_READY)
    {
        NV_LOGE("%s: Can't submit capture requests in state %d", __FUNCTION__,
                mStatus);
        return NO_INIT;
    }

    if (request == NULL)
    {
        NV_LOGE("%s: NULL request!", __FUNCTION__);
        return BAD_VALUE;
    }

    uint32_t frameNumber = request->frame_number;

    if (request->input_buffer != NULL &&
            request->input_buffer->stream != mInputStream)
    {
        NV_LOGE("%s: Request %d: Input buffer not from input stream!",
                __FUNCTION__, frameNumber);
        NV_LOGV(HAL3_TAG, "%s: Bad stream %p, expected: %p",
              __FUNCTION__, request->input_buffer->stream,
              mInputStream);
        NV_LOGV(HAL3_TAG, "%s: Bad stream type %d, expected stream type %d",
              __FUNCTION__, request->input_buffer->stream->stream_type,
              mInputStream ? mInputStream->stream_type : -1);

        return BAD_VALUE;
    }

    if (request->num_output_buffers < 1 || request->output_buffers == NULL)
    {
        NV_LOGE("%s: Request %d: No output buffers provided!",
                __FUNCTION__, frameNumber);
        return BAD_VALUE;
    }

    // Validate all buffers, starting with input buffer if it's given

    ssize_t idx;
    const camera3_stream_buffer_t *b;
    if (request->input_buffer != NULL)
    {
        idx = -1;
        b = request->input_buffer;
    }
    else
    {
        idx = 0;
        b = request->output_buffers;
    }
    do {
        if (b->stream->priv == NULL)
        {
            NV_LOGE("%s: Request %d: Buffer %d: Unconfigured stream!",
                    __FUNCTION__, frameNumber, idx);
            return BAD_VALUE;
        }
        if (b->status != CAMERA3_BUFFER_STATUS_OK)
        {
            NV_LOGE("%s: Request %d: Buffer %d: Status not OK!",
                    __FUNCTION__, frameNumber, idx);
            return BAD_VALUE;
        }
        if (b->release_fence != -1)
        {
            NV_LOGE("%s: Request %d: Buffer %d: Has a release fence!",
                    __FUNCTION__, frameNumber, idx);
            return BAD_VALUE;
        }
        if (b->buffer == NULL)
        {
            NV_LOGE("%s: Request %d: Buffer %d: NULL buffer handle!",
                    __FUNCTION__, frameNumber, idx);
            return BAD_VALUE;
        }

        idx++;
        b = &(request->output_buffers[idx]);
    } while (idx < (ssize_t)request->num_output_buffers);

    // TODO: Validate settings parameters

    /*
     * Start processing this request
     */
    HalBufferVector *nbufs = new HalBufferVector;
    nbufs->setCapacity(request->num_output_buffers);

    NvU64 frameCaptureRequestId = 0;

    for (size_t i = 0; i < request->num_output_buffers; i++)
    {
        camera3_stream_buffer *buf =
                    (camera3_stream_buffer *)&request->output_buffers[i];
        const camera3_stream_buffer &srcBuf = request->output_buffers[i];

        NV_LOGV(HAL3_TAG, "%s: requested fn=%d (%d of %d) size = %dx%d, anb=%p", __FUNCTION__, request->frame_number, i,
                request->num_output_buffers, srcBuf.stream->width,
                srcBuf.stream->height, buf->buffer);

        // Wait on fence
        sp<Fence> bufferAcquireFence = new Fence(srcBuf.acquire_fence);
        res = bufferAcquireFence->wait(kFenceTimeoutMs);
        if (res == TIMED_OUT)
        {
            NV_LOGE("%s: Request %d: Buffer %d: Fence timed out after %d ms",
                    __FUNCTION__, frameNumber, i, kFenceTimeoutMs);
        }
        if (srcBuf.stream->stream_type == CAMERA3_STREAM_INPUT ||
            srcBuf.stream->stream_type == CAMERA3_STREAM_BIDIRECTIONAL)
        {
            // save the buf handle as the Id for the Core to save that metadata
            // as this frame might come later as an input buffer for
            // reprocessing
            frameCaptureRequestId = (NvU64)(buf->buffer);
        }

        nbufs->push_back(srcBuf);
    }

    Request r;
    NvOsMemset(&r, 0, sizeof(Request));
    r.frameNumber = request->frame_number;
    r.settings = request->settings;
    r.buffers = nbufs;
    r.input_buffer = request->input_buffer;
    r.num_buffers = request->num_output_buffers;
    r.frameCaptureRequestId = frameCaptureRequestId;

    status_t ret;
    NvError error = mNvCameraCore.processRequest(r);
    switch (error)
    {
        case NvSuccess:
            ret = OK;
            break;
        case NvError_InsufficientMemory:
            //fatal error: camera device will be closed
            ret = NO_INIT;
            break;
        default:
            ret = BAD_VALUE;
    }

    delete nbufs;

    if (ret == OK)
    {
        mNvCameraCore.waitAvailableBuffers();
    }
    NV_LOGD(HAL3_TAG, "%s: --", __FUNCTION__);
    return ret;
}

status_t NvCameraHal3::flush()
{
    NV_TRACE_CALL_D(HAL3_TAG);
    NV_LOGD(HAL3_TAG, "%s: ++",__FUNCTION__);
    Mutex::Autolock l(mLock);
    status_t ret = OK;

    NvError error = mNvCameraCore.flush();
    switch (error)
    {
        case NvSuccess:
            ret = OK;
            break;
        default:
            ret = BAD_VALUE;
    }

    NV_LOGD(HAL3_TAG, "%s: --",__FUNCTION__);
    return ret;
}

status_t NvCameraHal3::dump(int fd)
{
    NV_TRACE_CALL_D(HAL3_TAG);
    NV_LOGD(HAL3_TAG, "%s: ++",__FUNCTION__);
    Mutex::Autolock lock(mLock);
    NV_LOGD(HAL3_TAG, "%s: --",__FUNCTION__);
    return OK;
}

void NvCameraHal3::notifyError(
    int frame, camera3_error_msg_code_t error,
    camera3_stream* stream)
{
    NV_TRACE_CALL_D(HAL3_TAG);
    camera3_notify_msg_t msg;
    msg.type = CAMERA3_MSG_ERROR;
    msg.message.error.frame_number = frame;
    msg.message.error.error_stream = stream;
    msg.message.error.error_code = error;

    if (error == CAMERA3_MSG_ERROR_DEVICE)
    {
        {
            Mutex::Autolock l(mLock);
            mStatus = STATUS_ERROR;
        }
    }

    NV_LOGE("%s Error %d (0x%08x) in frame: %d", __FUNCTION__,
        msg.message.error.error_code, error, frame);
    mCallbackOps->notify(mCallbackOps, &msg);
}

void NvCameraHal3::notifyShutterMsg(int frame, NvU64 ts)
{
    NV_TRACE_CALL_D(HAL3_TAG);
    camera3_notify_msg_t msg;
    msg.type = CAMERA3_MSG_SHUTTER;
    msg.message.shutter.frame_number = frame;
    msg.message.shutter.timestamp = (int64_t)ts;
    mCallbackOps->notify(mCallbackOps, &msg);
}

void NvCameraHal3::sendResult(camera3_capture_result* result)
{
    // Send it off to the framework
    NV_TRACE_CALL_D(HAL3_TAG);
    mCallbackOps->process_capture_result(mCallbackOps, result);
}


} // namespace android


