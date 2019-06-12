/*
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvrm_init.h"
#include "nvgr.h"
#include <math.h>

#include "nvcamerahal3streamport.h"
#include "nvmetadatatranslator.h"
#include "nvcamera_core.h"
#include "nvcamerahal3_tags.h"
#include "nv_log.h"


// compare the two ratios upto two decimal places
#define ASPECT_RATIO_PRECISION 0.01

namespace android {

int StreamPort::offset_id = 0;
Mutex StreamPort::offs_mutx;

StreamPort::StreamPort(
    camera3_stream_t *stream,
    NvCameraStreamType _type,
    NvColorFormat SensorFormat)

    :alive(true),
    bypass(false),
    mNumBuf(0),
    registered(false),
    mIsBayer(false),
    mSensorFormat(SensorFormat),
    type(_type),
    mStream(stream),
    cameraBuffer(NULL),
    mConverter(mStream->format)
{
    NvOsSemaphoreCreate(&mWorkSema, 0);
    NvOsSemaphoreCreate(&mAvailableSema, 0);
    NvOsSemaphoreCreate(&mFlushSema, 0);

    mFlushing = 0;

    mTNRAllocated = false;

    NV_LOGD(HAL3_STREAM_PORT_TAG, "%s: ++ stream %p configured with %dx%d type=%s", __FUNCTION__,
        stream,
        mStream->width, mStream->height,
        type == CAMERA_STREAM_ZOOM ? "CAMERA_STREAM_ZOOM" :
        "CAMERA_STREAM_NONE");

    if (mStream->format == HAL_PIXEL_FORMAT_BLOB)
    {
        // create a jpeg helper thread
        mJpegProcessor = new JpegProcessor(this);
        mJpegProcessor->run("JpegWorkThread");
    }

    if (mStream->format == HAL_PIXEL_FORMAT_RAW_SENSOR)
    {
        mIsBayer = true;
    }

    NV_LOGD(HAL3_STREAM_PORT_TAG, "%s: --", __FUNCTION__);
}

// TODO
StreamPort::~StreamPort()
{
    NV_LOGD(HAL3_STREAM_PORT_TAG, "%s: ++", __FUNCTION__);

    // Request exit for jpeg processor
    if (mJpegProcessor != NULL)
    {
        mJpegProcessor->requestExit();
        mJpegProcessor->flush();
        mJpegProcessor->join();
    }

    BufferInfoQueue::iterator itr = OutputQ.begin();
    while (itr != OutputQ.end())
    {
        BufferInfo *pBufferInfo = (*itr);
        itr = OutputQ.erase(itr);
        delete pBufferInfo->outBuffer;
        pBufferInfo->outBuffer = NULL;
        delete pBufferInfo->pJpegSettings;
        pBufferInfo->pJpegSettings = NULL;
        delete pBufferInfo;
        pBufferInfo = NULL;
    }

    if (mTNRAllocated)
    {
        NvImageScaler::FreeSurface(&mTNRBuffer.Payload.Surfaces);
    }

    NvOsSemaphoreDestroy(mWorkSema);
    NvOsSemaphoreDestroy(mAvailableSema);
    NvOsSemaphoreDestroy(mFlushSema);

    delete [] cameraBuffer;
    cameraBuffer = NULL;

    NV_LOGD(HAL3_STREAM_PORT_TAG, "%s: --", __FUNCTION__);
}

NvError StreamPort::processResult(
    uint32_t frameNumber,
    RouterBuffer *inBuffer,
    const NvCamProperty_Public_Dynamic *pFrameDynamicProps)
{
    // running in the context of the callback from NvMm
    // Router has passed us the frameNumber that has been processed
    // iterate over the queue and check
    Mutex::Autolock lock(bufferQMutex);
    if (inBuffer != NULL)
    {
        // Update the corresponding BufferInfo with inBuffer
        BufferInfoQueue::iterator itr = OutputQ.begin();
        while (itr != OutputQ.end())
        {
            BufferInfo *pOutBufferInfo = (*itr);
            if (frameNumber == pOutBufferInfo->frameNumber)
            {
                pOutBufferInfo->inBuffer = inBuffer;

                if(streamFormat() == HAL_PIXEL_FORMAT_BLOB)
                {
                    pOutBufferInfo->state = BUFFERINFO_STATE_JPEG_START;
                }
                else
                {
                    pOutBufferInfo->state = BUFFERINFO_STATE_READY;
                }

                if (pFrameDynamicProps && streamFormat() == HAL_PIXEL_FORMAT_BLOB)
                {
                    pOutBufferInfo->pJpegSettings->exifInfo =
                        pFrameDynamicProps->ExifInfo;

                    strncpy(pOutBufferInfo->pJpegSettings->makerNoteData,
                        pFrameDynamicProps->MakerNoteData, MAKERNOTE_DATA_SIZE);
                }
                break;
            }
            ++itr;
        }
    }
    NvOsSemaphoreSignal(mWorkSema);
    return NvSuccess;
}

bool StreamPort::isOwner(buffer_handle_t *buffer)
{
    Mutex::Autolock lock(bufferQMutex);
    BufferInfoQueue::iterator itr = OutputQ.begin();
    while (itr != OutputQ.end())
    {
        BufferInfo *pOutBufferInfo = (*itr);
        camera3_stream_buffer *outBuffer = pOutBufferInfo->outBuffer;
        if (outBuffer && outBuffer->buffer == buffer)
        {
            return true;
        }
        ++itr;
    }

    return false;
}

bool StreamPort::threadLoop()
{
    status_t res;
    NvError err;
    NvOsSemaphoreWait(mWorkSema);

    bufferQMutex.lock();
    BufferInfoQueue::iterator itr = OutputQ.begin();

    while (itr != OutputQ.end())
    {
        NvError err = NvSuccess;

        // Read the first buffer from OutputQ
        BufferInfo *pOutBufferInfo = (*itr);
        RouterBuffer *inBuffer = pOutBufferInfo->inBuffer;
        camera3_stream_buffer *outBuffer = pOutBufferInfo->outBuffer;
        int frameNumber = pOutBufferInfo->frameNumber;
        NvU32 bufferState = pOutBufferInfo->state;

        NvRectF32 rect;
        NvOsMemset(&rect, 0, sizeof(NvRectF32));

        if(inBuffer == NULL)
        {
            // The first frame in queue hasn't received buffer yet
            bufferQMutex.unlock();
            return true;
        }

        NV_LOGV(HAL3_STREAM_PORT_TAG, "%s: Processing frameNumber[%d] buffer state [%d]",
            __FUNCTION__, frameNumber, bufferState);

        NvMMBuffer *inNvmmBuffer = inBuffer->nvmm_buffer;
        NvMMBuffer *outNvmmBuffer = NativeToNVMMBuffer(outBuffer->buffer);

        if(outNvmmBuffer == NULL)
        {
            NV_LOGE("%s: Error(0x%x) dereferencing anb buffer(%p)", __FUNCTION__,
                err, outBuffer->buffer);
            err = NvError_BadValue;
        }

        bufferQMutex.unlock();

        if (!inBuffer->isCropped)
        {
            inBuffer->rect.left = 0;
            inBuffer->rect.top = 0;
            inBuffer->rect.right = 1;
            inBuffer->rect.bottom = 1;
        }

        setCropRegion(inBuffer, inNvmmBuffer, &rect);
        NV_LOGV(HAL3_STREAM_PORT_TAG, "%s: input (%d, %d) (%d %d) output: %dx%d",
                __FUNCTION__,
                (NvU32)rect.left, (NvU32)rect.top,
                (NvU32)rect.right, (NvU32)rect.bottom,
                mStream->width, mStream->height);

        // bypass all buffer encoding/scaling if we hit an error earlier
        if (!bypass && (err == NvSuccess))
        {
            //do the job: scaling, format conversion
            if (outBuffer->stream->format == HAL_PIXEL_FORMAT_BLOB)
            {
                switch (bufferState)
                {
                    case BUFFERINFO_STATE_JPEG_START:
                    {
                        // do format conversion and thumbnail blit
                        NvMMBuffer *thumbnailBuffer = NULL;
                        err = prepareJpegEncoding(inNvmmBuffer, rect, outNvmmBuffer,
                            &thumbnailBuffer, pOutBufferInfo->pJpegSettings);
                        if (err != NvSuccess)
                        {
                            NV_LOGE("%s: Error(0x%x) in prepareJpegEncoding",
                                __FUNCTION__, err);
                            break;
                        }

                        // mark as WAIT first to avoid race condition
                        pOutBufferInfo->state = BUFFERINFO_STATE_JPEG_WAIT;

                        // send it to jpeg work thread
                        mJpegProcessor->enqueueJpegWork(pOutBufferInfo,thumbnailBuffer);
                        thumbnailBuffer = NULL;

                        // early skip
                        bufferQMutex.lock();
                        itr ++;
                        continue;
                    }
                    case BUFFERINFO_STATE_JPEG_WAIT:
                    {
                        // skip processing
                        bufferQMutex.lock();
                        itr ++;
                        continue;
                    }
                    case BUFFERINFO_STATE_READY:
                    {
                        // proceed to send the buffer
                        NV_LOGV(HAL3_STREAM_PORT_TAG, "%s: received JPEG buffer", __FUNCTION__);
                        break;
                    }
               }
            }
            else if (streamType() == CAMERA_STREAM_ZOOM)
            {
                // don't do anything if this is zoom StreamPort.
                NV_LOGV(HAL3_STREAM_PORT_TAG, "%s: received ZOOM stream", __FUNCTION__);
            }
            else if (inBuffer->buffer != outBuffer->buffer)
            {
                err = doCropAndScale(inNvmmBuffer, rect, outNvmmBuffer);
            }
        }
        else if (bypass && (err == NvSuccess))
        {
            if (outBuffer->stream->format == HAL_PIXEL_FORMAT_BLOB)
            {
                // flush to all jpeg finish
                err = mJpegProcessor->flush();
                // if in middle of OutputQ, go to first node to flush
                if (itr != OutputQ.begin())
                {
                    bufferQMutex.lock();
                    itr = OutputQ.begin();
                    continue;
                }
            }
        }

        inBuffer->RefDec();

        if (streamType() == CAMERA_STREAM_ZOOM)
        {
            pushZoomBuffer(inBuffer->buffer);
        }

        // done with everything, can return the buffer to the upper layer
        // if we own the input buffer then wait on its users
        if(inBuffer->owner == this)
        {
            inBuffer->Wait();
            delete inBuffer;
        }

        if (err != NvSuccess)
        {
            NV_LOGE("%s: Error(0x%x) processing buffer(%p) for frame #=%d", __FUNCTION__,
                err, outBuffer->buffer, frameNumber);
            reportError(frameNumber, err);
        }

        if (streamType() != CAMERA_STREAM_ZOOM)
        {
            NV_LOGV(HAL3_STREAM_PORT_TAG, "%s: return in anb=%p out anb=%p, format=%x, frame #=%d",
                    __FUNCTION__, inBuffer->buffer, outBuffer->buffer,
                    outBuffer->stream->format, frameNumber);
           sendResult(outBuffer, frameNumber, !bypass);
        }

        err = dequeueOutputBuffer();
        if (err != NvSuccess)
        {
            NV_LOGE("%s: Error(0x%x) dequeueing output buffer(%p)", __FUNCTION__,
                err, outBuffer->buffer);
            reportError(frameNumber, err);
        }

        bufferQMutex.lock();
        itr = OutputQ.begin();
    }

    bufferQMutex.unlock();
    return true;
}

NvMMBuffer* StreamPort::NativeToNVMMBuffer(buffer_handle_t *buffer)
{
    NvU32 i = 0;
    NV_LOGV(HAL3_STREAM_PORT_TAG, "%s", __FUNCTION__);
    while (i < mNumBuf)
    {
        if (cameraBuffer[i].anb == buffer)
        {
            return &cameraBuffer[i].nvmm;
        }
        i++;
    }
    return NULL;
}

NvMMBuffer* StreamPort::NativeToNVMMBuffer(
    buffer_handle_t *buffer,
    NvBool *isCropped,
    NvRectF32 *pRect)
{
    NvU32 i = 0;
    NV_LOGV(HAL3_STREAM_PORT_TAG, "%s", __FUNCTION__);
    while (i < mNumBuf)
    {
        if (cameraBuffer[i].anb == buffer)
        {
            Mutex::Autolock lock(cameraBuffer[i].mLock);
            *isCropped = cameraBuffer[i].isCropped;
            NvOsMemcpy(pRect, &cameraBuffer[i].rect, sizeof(NvRectF32));
            return &cameraBuffer[i].nvmm;
        }
        i++;
    }
    return NULL;
}

buffer_handle_t* StreamPort::NvMMBufferToNative(NvMMBuffer *buffer)
{
    NvU32 i = 0;
    NV_LOGV(HAL3_STREAM_PORT_TAG, "%s", __FUNCTION__);
    while (i < mNumBuf)
    {
        if (cameraBuffer[i].nvmm.BufferID == buffer->BufferID)
        {
            return cameraBuffer[i].anb;
        }
        i++;
    }
    return NULL;
}

NvError StreamPort::doCropAndScale(
    NvMMBuffer *pNvMMSrc,
    NvRectF32 rect,
    NvMMBuffer *pNvMMDst)
{
    NvError err = NvSuccess;
    err = mConverter.Convert(pNvMMSrc, rect, pNvMMDst);
    return err;
}

NvError StreamPort::notifyJpegDone(
    BufferInfo *pBufferInfo,
    NvMMBuffer *thumbnailBuffer)
{
    NvError e;
    NV_LOGD(HAL3_STREAM_PORT_TAG, "%s: ++", __FUNCTION__);

    if (thumbnailBuffer != NULL)
    {
        NvImageScaler::FreeSurface(&thumbnailBuffer->Payload.Surfaces);
        NvOsFree(thumbnailBuffer);
        thumbnailBuffer = NULL;
    }

    Mutex::Autolock lock(bufferQMutex);
    BufferInfoQueue::iterator itr = OutputQ.begin();
    while (itr != OutputQ.end())
    {
        BufferInfo *pOriginBufferInfo = (*itr);

        if (pBufferInfo->frameNumber == pOriginBufferInfo->frameNumber)
        {
            pOriginBufferInfo->state = BUFFERINFO_STATE_READY;
            NvOsSemaphoreSignal(mWorkSema);
            NV_LOGD(HAL3_STREAM_PORT_TAG, "%s: --", __FUNCTION__);
            return NvSuccess;
        }
        ++itr;
    }

   e = NvError_BadParameter;

fail:
    NV_LOGE("%s: -- (error 0x%x)", __FUNCTION__, e);
    return e;
}


NvMMBuffer *StreamPort::getTNRBuffer()
{
    NvError e = NvSuccess;

    if (mTNRAllocated==false)
    {
        NV_CHECK_ERROR_CLEANUP(
            NvImageScaler::AllocateYuv420NvmmSurface(
                &mTNRBuffer.Payload.Surfaces,
                mStream->width, mStream->height)
            );
        mTNRAllocated = true;
    }

    return &mTNRBuffer;

fail:
    ALOGE("%s: -- (error 0x%x)", __FUNCTION__, e);
    return NULL;
}

NvError StreamPort::prepareJpegEncoding(
    NvMMBuffer *pNvMMSrc,
    NvRectF32 rect,
    NvMMBuffer *pNvMMDst,
    NvMMBuffer **ppThumbnailBuffer,
    NvCameraHal3_JpegSettings *pJpegSettings)
{
    NV_LOGD(HAL3_STREAM_PORT_TAG, "%s: ++",__FUNCTION__);

    NvError e;

    if (*ppThumbnailBuffer)
    {
        return NvError_BadParameter;
    }

    NvMMBuffer *pThumbnail = NULL;
    NvJpegEncParameters thumbJpegParams;
    NvOsMemset(&thumbJpegParams, 0, sizeof(thumbJpegParams));

    NV_CHECK_ERROR_CLEANUP(mConverter.Convert(
        pNvMMSrc, rect, pNvMMDst));

    if (pJpegSettings->JpegControls.thumbnailSize.width &&
            pJpegSettings->JpegControls.thumbnailSize.height)
    {
        pThumbnail = (NvMMBuffer *)NvOsAlloc(sizeof(NvMMBuffer));
        if (!pThumbnail)
        {
            pThumbnail = NULL;
            NV_LOGE("%s: failed to allocate jpeg thumbnail buffer", __FUNCTION__);
        }
        else
        {
            mJpegProcessor->constructThumbnailParams(&thumbJpegParams, pJpegSettings);
            // allocate and copy thumbnail surfaces
            e = constructThumbnailBuffer(&thumbJpegParams, pThumbnail, pNvMMSrc);
            if (e != NvSuccess)
            {
                NV_LOGE(" Error creating thumbnail buffer");
                // Don't return; go ahead without thumbnail
                NvOsFree(pThumbnail);
                pThumbnail = NULL;
                return NvError_InsufficientMemory;
            }
         }
    }

    *ppThumbnailBuffer = pThumbnail;

    NV_LOGD(HAL3_STREAM_PORT_TAG, "%s: --",__FUNCTION__);
    return e;
fail:
    NV_LOGE("%s: -- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError StreamPort::constructThumbnailBuffer(
    NvJpegEncParameters *params,
    NvMMBuffer *pThumbnailBuffer, NvMMBuffer *Buffer)
{
    NvError e = NvSuccess;
    NvRectF32 zeroCrop;
    NvOsMemset(&zeroCrop, 0, sizeof(NvRectF32));

    NV_LOGD(HAL3_STREAM_PORT_TAG, "%s: ++ at %d", __FUNCTION__, __LINE__);
    // initialize thumbnail buffer
    NvOsMemset(pThumbnailBuffer, 0, sizeof(NvMMBuffer));
    pThumbnailBuffer->StructSize = sizeof(NvMMBuffer);
    pThumbnailBuffer->PayloadType = NvMMPayloadType_SurfaceArray;
    // we think all other fields (except surfaces) don't matter for thumbnail,
    // but if the encoder decides to use them for anything, we'll need to
    // init them properly here
    // alloc surfaces
    // thumbnail rotate with orientation
#if ENABLE_BLOCKLINEAR_SUPPORT
    NV_LOGV(HAL3_STREAM_PORT_TAG, "%s: Allocating NV12 for w=%d, h=%d for thumbnail", __FUNCTION__,
            params->Resolution.width, params->Resolution.height);
    NV_CHECK_ERROR_CLEANUP(
            NvImageScaler::AllocateNV12NvmmSurface(
                &pThumbnailBuffer->Payload.Surfaces,
                params->Resolution.width,
                params->Resolution.height)
            );
#else
    NV_CHECK_ERROR_CLEANUP(
            NvImageScaler::AllocateYuv420NvmmSurface(
                &pThumbnailBuffer->Payload.Surfaces,
                params->Resolution.width,
                params->Resolution.height)
            );
#endif
    // scale full buffer into it
    NV_LOGV(HAL3_STREAM_PORT_TAG, "%s: Scaling buffer into thumbnail", __FUNCTION__);
    mConverter.Convert(Buffer, zeroCrop, pThumbnailBuffer);
    NV_LOGD(HAL3_STREAM_PORT_TAG, "%s: --", __FUNCTION__);
    return e;
fail:
    NvImageScaler::FreeSurface(&pThumbnailBuffer->Payload.Surfaces);
    NV_LOGE("%s: -- (error 0x%x)", __FUNCTION__, e);
    return e;
}

void StreamPort::waitAvailableBuffers()
{
    bufferQMutex.lock();
    while(OutputQ.size() >= mStream->max_buffers)
    {
        bufferQMutex.unlock();
        NvOsSemaphoreWait(mAvailableSema);
        bufferQMutex.lock();
    }
    bufferQMutex.unlock();
}

void StreamPort::flushBuffers()
{
    bufferQMutex.lock();
    mFlushing = 1;
    while(OutputQ.size() != 0)
    {
        bufferQMutex.unlock();
        NvOsSemaphoreWait(mFlushSema);
        bufferQMutex.lock();
    }
    mFlushing = 0;
    bufferQMutex.unlock();
}

NvError StreamPort::enqueueOutputBuffer(camera3_stream_buffer *buffer,
            int frameNumber, const NvCameraHal3_Public_Controls& prop)
{
    Mutex::Autolock lock(bufferQMutex);

    if(OutputQ.size() >= mStream->max_buffers)
    {
        NV_LOGE("%s: Error: Too many buffers in flight, max size allowed[%d]",
                __FUNCTION__, mStream->max_buffers);
        return NvError_BadValue;
    }

    BufferInfo *pBufferInfo = new BufferInfo;

    pBufferInfo->outBuffer = new camera3_stream_buffer(*buffer);
    pBufferInfo->frameNumber = frameNumber;
    pBufferInfo->inBuffer = NULL;
    pBufferInfo->state = BUFFERINFO_STATE_INITIAL;

    if ((streamFormat() == HAL_PIXEL_FORMAT_BLOB))
    {
        pBufferInfo->pJpegSettings = new NvCameraHal3_JpegSettings;
        // reset
        NvOsMemset(pBufferInfo->pJpegSettings, 0, sizeof(NvCameraHal3_JpegSettings));
        pBufferInfo->pJpegSettings->JpegControls = prop.JpegControls;
        pBufferInfo->pJpegSettings->jpegResolution.width = mStream->width;
        pBufferInfo->pJpegSettings->jpegResolution.height = mStream->height;
    }
    else
    {
        pBufferInfo->pJpegSettings = NULL;
    }

    OutputQ.push_back(pBufferInfo);

    return NvSuccess;
}


NvError StreamPort::dequeueOutputBuffer()
{
    Mutex::Autolock lock(bufferQMutex);

    if(OutputQ.size() == 0)
    {
        NV_LOGE("%s: Error: Failed to dequeue, OutputQ is empty!",
                __FUNCTION__);
        return NvError_BadValue;
    }

    BufferInfoQueue::iterator itr = OutputQ.begin();
    BufferInfo *pBufferInfo = (*itr);
    OutputQ.erase(itr);
    delete pBufferInfo->outBuffer;
    pBufferInfo->outBuffer = NULL;
    delete pBufferInfo->pJpegSettings;
    pBufferInfo->pJpegSettings = NULL;
    delete pBufferInfo;
    pBufferInfo = NULL;

    if(OutputQ.size() == (mStream->max_buffers - 1))
    {
        NvOsSemaphoreSignal(mAvailableSema);
    }

    if(OutputQ.size() == 0 && mFlushing)
    {
        NvOsSemaphoreSignal(mFlushSema);
    }

    return NvSuccess;
}

NvError StreamPort::InitializeNvMMBufferWithANB(CameraBuffer *buffer)
{
    NvU32 width;
    NvU32 height;
    buffer_handle_t handle;
    NvMMSurfaceDescriptor *pSurfaces;
    const NvRmSurface *Surf;
    size_t SurfCount;

    NV_LOGD(HAL3_STREAM_PORT_TAG, "%s: ++", __FUNCTION__);

    if (!buffer || !buffer->anb)
    {
        NV_LOGE("%s-- fail @ line %d", __FUNCTION__, __LINE__);
        return NvError_BadParameter;
    }

    handle = (*buffer->anb);
    if (!handle)
    {
        NV_LOGE("%s-- fail @ line %d", __FUNCTION__, __LINE__);
        return NvError_BadParameter;
    }

    nvgr_get_surfaces(handle, &Surf, &SurfCount);

    // copy the surface pointers from the ANB to our NvMM buffer
    // todo use InitializeNvMMWithANB rename that function so all it does is
    // copy the surfaces.
    pSurfaces = &(buffer->nvmm.Payload.Surfaces);
    NvOsMemset(pSurfaces, 0, sizeof(NvMMSurfaceDescriptor));
    NvOsMemcpy(pSurfaces->Surfaces, Surf, sizeof(NvRmSurface) * SurfCount);
    pSurfaces->SurfaceCount = SurfCount;
    pSurfaces->Empty = NV_TRUE;

    NV_LOGD(HAL3_STREAM_PORT_TAG, "%s: --", __FUNCTION__);
    return NvSuccess;
}

NvError StreamPort::InitializeANBWithNvMMBuffer(CameraBuffer *buffer)
{
    NV_LOGD(HAL3_STREAM_PORT_TAG, "%s: ++", __FUNCTION__);

    if (!buffer)
    {
        NV_LOGE("%s-- fail @ line %d", __FUNCTION__, __LINE__);
        return NvError_BadParameter;
    }

    buffer->anb = (buffer_handle_t *)(&(buffer->nvmm));

    NV_LOGD(HAL3_STREAM_PORT_TAG, "%s: --", __FUNCTION__);
    return NvSuccess;
}

NvError StreamPort::linkBuffers(buffer_handle_t **ppBuffers, NvU32 count)
{
    int buffer_id;
    NvError e = NvSuccess;
    NV_LOGD(HAL3_STREAM_PORT_TAG, "%s: ++", __FUNCTION__);

    {
        Mutex::Autolock al(&offs_mutx);
        buffer_id = offset_id;
        offset_id += count;
    }

    // Register the buffers
    mNumBuf = count;
    cameraBuffer = new CameraBuffer[mNumBuf];
    for (int i = 0; i < (int)mNumBuf; i++)
    {
        cameraBuffer[i].anb = (ppBuffers != NULL)? ppBuffers[i] : NULL;
        cameraBuffer[i].nvmm.BufferID = buffer_id++;
        cameraBuffer[i].isAllocated = NV_FALSE;
    }

    registered = true;

    // Determine raw format and allocate intermediate buffers where necessary
    if (mStream->format == HAL_PIXEL_FORMAT_RAW_SENSOR)
    {
        for (int i = 0; i < (int)mNumBuf; i++)
        {
            const NvRmSurface *Surf;
            size_t SurfCount;
            nvgr_get_surfaces(*ppBuffers[i], &Surf, &SurfCount);
            const_cast<NvRmSurface *>(Surf)->ColorFormat = mSensorFormat;
        }
    }
    else if (mStream->format == HAL_PIXEL_FORMAT_BLOB)
    {
        for (int i = 0; i < (int)mNumBuf; i++)
        {
            NV_LOGV(HAL3_STREAM_PORT_TAG, "%s: allocating NV12 %dx%d for BLOB format",
                __FUNCTION__, mStream->width, mStream->height);
            NV_CHECK_ERROR_CLEANUP(
                NvMemAllocator::AllocateNvMMSurface(
                    &cameraBuffer[i].nvmm.Payload.Surfaces,
                    mStream->width,
                    mStream->height,
                    NV_CAMERA_HAL3_COLOR_NV12)
            );
            cameraBuffer[i].isAllocated = NV_TRUE;
        }
    }
    else if (streamType() == CAMERA_STREAM_ZOOM)
    {
        for (int i = 0; i < (int)mNumBuf; i++)
        {
            NV_LOGV(HAL3_STREAM_PORT_TAG, "%s: allocating YV12 %dx%d for Zoom Stream",
                __FUNCTION__, mStream->width, mStream->height);
            NV_CHECK_ERROR_CLEANUP(
                NvMemAllocator::AllocateNvMMSurface(
                    &cameraBuffer[i].nvmm.Payload.Surfaces,
                    mStream->width,
                    mStream->height,
                    NV_CAMERA_HAL3_COLOR_YV12)
            );
            cameraBuffer[i].isAllocated = NV_TRUE;
        }
    }

    // Link the buffers
    for (int i = 0; i < (int)mNumBuf; i++)
    {
        Mutex::Autolock lock(cameraBuffer[i].mLock);
        cameraBuffer[i].isCropped = NV_FALSE;
        if (mStream->format != HAL_PIXEL_FORMAT_BLOB)
        {
            if (streamType() == CAMERA_STREAM_ZOOM)
            {
                // Zoom stream is internal to HAL, we need to
                // instantiate the anb handles for this stream.
                 NV_CHECK_ERROR_CLEANUP (
                    InitializeANBWithNvMMBuffer(&cameraBuffer[i])
                );
                pushZoomBuffer(cameraBuffer[i].anb);
            }
            else
            {
                // For every other stream we need to initialize
                // NvMM surfaces with the given anb handles
                NV_CHECK_ERROR_CLEANUP (
                    InitializeNvMMBufferWithANB(&cameraBuffer[i])
                );
            }
        }

        NV_LOGV(HAL3_STREAM_PORT_TAG, "%s: init i=%d anb[%p]/nvmm[%p][%d]",
            __FUNCTION__, i, cameraBuffer[i].anb, &cameraBuffer[i].nvmm,
            cameraBuffer[i].nvmm.BufferID);
    }

    NV_LOGD(HAL3_STREAM_PORT_TAG, "%s: --", __FUNCTION__);
    return NvSuccess;
fail:
    // Unlink any buffers we may have linked uptill now
    unlinkBuffers(NULL);

    NV_LOGE("%s: -- error [0x%x]", __FUNCTION__, e);
    return e;
}

void StreamPort::unlinkBuffers(NvCameraCoreHandle hNvCameraCore)
{
    NV_LOGD(HAL3_STREAM_PORT_TAG, "%s: ++", __FUNCTION__);

    // Clear the zoom queue.
    zoomBufferQMutex.lock();
    ANBQueue::iterator itr = mZoomBufferQ.begin();

    while (itr != mZoomBufferQ.end())
    {
        buffer_handle_t *anb = *itr;
        itr = mZoomBufferQ.erase(itr);
    }

    zoomBufferQMutex.unlock();
    NvU64 pFrameCaptureRequestId[mNumBuf];

    // Unlink the surfaces
    for (int i = 0; i < (int)mNumBuf; i++)
    {
        if (cameraBuffer[i].isAllocated == NV_TRUE)
        {
            NvMemAllocator::FreeSurface(&cameraBuffer[i].nvmm.Payload.Surfaces);
        }
        pFrameCaptureRequestId[i] = (NvU64)cameraBuffer[i].anb;
        cameraBuffer[i].anb = NULL;
        cameraBuffer[i].isAllocated = NV_FALSE;
    }
    // call core to release the saved metadata info in case of input stream
    if (mStream->stream_type == CAMERA3_STREAM_BIDIRECTIONAL ||
            mStream->stream_type == CAMERA3_STREAM_INPUT)
    {
        if (hNvCameraCore)
        {
            NvCameraCore_ReleaseCapturedFrameMetadataList(hNvCameraCore,
                    pFrameCaptureRequestId, mNumBuf);
        }
    }

    mNumBuf = 0;
    registered = false;

    NV_LOGD(HAL3_STREAM_PORT_TAG, "%s: --", __FUNCTION__);
}


void StreamPort::sendResult(camera3_stream_buffer *buffer, int frame, bool status)
{
    camera3_capture_result result;

    buffer->status = status? CAMERA3_BUFFER_STATUS_OK :
              CAMERA3_BUFFER_STATUS_ERROR;
    buffer->acquire_fence = -1;
    buffer->release_fence = -1;

    camera3_stream_buffer &buf = *buffer;
    HalBufferVector *nbufs = new HalBufferVector;
    nbufs->setCapacity(1);
    nbufs->push_back(buf);

    result.frame_number = frame;
    result.result = NULL;
    result.num_output_buffers = 1;
    result.output_buffers = nbufs->array();

    doSendResult(&result);

    delete nbufs;
}

buffer_handle_t *StreamPort::getZoomBuffer()
{
    Mutex::Autolock lock(zoomBufferQMutex);
    buffer_handle_t *anb = NULL;

    if (!mZoomBufferQ.size())
    {
        NV_LOGE("%s: there is no available anb buffer", __FUNCTION__);
    }
    else
    {
        ANBQueue::iterator itr = mZoomBufferQ.begin();
        anb = (*itr);
        mZoomBufferQ.erase(itr);
    }

    return anb;
}

void StreamPort::pushZoomBuffer(buffer_handle_t *anb)
{
    Mutex::Autolock lock(zoomBufferQMutex);
    NV_LOGV(HAL3_STREAM_PORT_TAG, "%s: mZoomBufferQ.size=%d", __FUNCTION__, mZoomBufferQ.size());
    mZoomBufferQ.push_back(anb);
}

void StreamPort::setCropInZoomBuffer(
    buffer_handle_t *anb,
    NvRectF32 *pRect)
{
    NvU32 i = 0;

    while (i < mNumBuf)
    {
        if (cameraBuffer[i].anb == anb)
        {
            Mutex::Autolock lock(cameraBuffer[i].mLock);
            cameraBuffer[i].isCropped = NV_TRUE;
            NvOsMemcpy(&cameraBuffer[i].rect, pRect, sizeof(NvRectF32));
            break;
        }
        i++;
    }
    if (i == mNumBuf)
    {
        NV_LOGE("%s: there is no anb[%p]", __FUNCTION__, anb);
    }
}

void StreamPort::reportError(int frame, NvError error)
{
    camera3_error_msg_code_t errorCode = nvErrorToHal3Error(error);
    doNotifyError(frame, errorCode, mStream);
}

camera3_error_msg_code_t
StreamPort::nvErrorToHal3Error(NvError error)
{
    camera3_error_msg_code_t ret;
    switch (error)
    {
        case NvError_InsufficientMemory:
            // fall-through. Treat Insufficient memory
            // as a non-fatal error.
        default:
            ret = CAMERA3_MSG_ERROR_BUFFER;
    }
    return ret;
}

void StreamPort::setCropRegion(RouterBuffer *inBuffer,
    NvMMBuffer *inNvmmBuffer, NvRectF32 *pRect)
{
    NvF32 inLeft = (inBuffer->rect.left *
                  (NvF32)inNvmmBuffer->Payload.Surfaces.Surfaces[0].Width);
    NvF32 inTop = (inBuffer->rect.top *
                  (NvF32)inNvmmBuffer->Payload.Surfaces.Surfaces[0].Height);
    NvF32 inRight = (inBuffer->rect.right *
                    (NvF32)inNvmmBuffer->Payload.Surfaces.Surfaces[0].Width);
    NvF32 inBottom = (inBuffer->rect.bottom *
                    (NvF32)inNvmmBuffer->Payload.Surfaces.Surfaces[0].Height);
    NvF32 outLeft = inLeft;
    NvF32 outTop = inTop;
    NvF32 outRight = inRight;
    NvF32 outBottom = inBottom;
    NvRect *pCropRect = &(inNvmmBuffer->Payload.Surfaces.CropRect);
    NvU32 outWidth = mStream->width;
    NvU32 outHeight = mStream->height;
    NvF32 inCropAspectRatio = (inRight - inLeft) / (inBottom - inTop);

    NvF32 outCropAspectRatio = (NvF32)outWidth / (NvF32)outHeight;

    NV_LOGV(HAL3_STREAM_PORT_TAG, "%s: inCropAspectRatio = %.2f outCropAspectRatio = %.2f",
            __FUNCTION__, inCropAspectRatio, outCropAspectRatio);

    NV_LOGV(HAL3_STREAM_PORT_TAG, "%s: in CropRegion (%d %d %d %d)",
            __FUNCTION__,
            (NvU32)inLeft, (NvU32)inTop,
            (NvU32)inRight, (NvU32)inBottom);
    if (fabs(inCropAspectRatio - outCropAspectRatio) >
        ASPECT_RATIO_PRECISION)
    {


        /*
         *  When aspect ratio of input is larger than that of output.
         *
         *  Input rectangle
         *
         * -> dx <-
         * ->        Win         <-
         *  +----************----+   \
         *  |    *          *    |   |
         *  |    *          *    |   Hin
         *  |    *          *    |   |
         *  +----************----+   /
         *
         *  Output rectangle
         *
         * ->     Wout     <-
         *  +--------------+  \
         *  |              |  |
         *  |              |  Hout
         *  |              |  |
         *  +--------------+  /
         *
         *  AspectOut = Wout/Hout
         *  dx = (Win - Hin * (Wout/Hout))/2
         *     = (Win - Hin * AspectOut)/2
         *  new left = left + dx
         *  new right = right - dx
         *
         */
        if (inCropAspectRatio > outCropAspectRatio)
        {
            NvF32 dx = ((NvF32)(inRight - inLeft) -
                outCropAspectRatio *
                (NvF32)(inBottom - inTop))/ 2.;

            outLeft += dx;
            outRight -= dx;
        }
        /*
         *  When aspect ratio of input is smaller than that of output.
         *
         *  Input rectangle
         *
         * ->       Win        <-
         *  +------------------+   \      +
         *  |                  |   |      dy
         *  ********************   |      +
         *  *                  *   Hin
         *  *                  *   |
         *  *******************|   |
         *  |                  |   |
         *  +------------------+   /
         *
         *  Output rectangle
         *
         * ->       Wout      <-
         *  +-----------------+  \
         *  |                 |  |
         *  |                 |  Hout
         *  |                 |  |
         *  +-----_-----------+  /
         *
         *  AspectOut = Wout/Hout
         *  dy = (Hin - Win * (Hout/Wout))/2
         *     = (Hin - Win / AspectOut)/2
         *  new top = top + dy
         *  new bottom = bottom - dy
         *
         */
        else
        {
            NvF32 dy = ((NvF32)(inBottom - inTop) -
                (NvF32)(inRight - inLeft) /
                outCropAspectRatio) / 2.;

            outTop += dy;
            outBottom -= dy;
        }
    }

    pRect->left = outLeft;
    pRect->right = outRight;
    pRect->top = outTop;
    pRect->bottom = outBottom;

    NV_LOGV(HAL3_STREAM_PORT_TAG, "%s: out CropRegion (%d %d %d %d)",
            __FUNCTION__, (NvU32)pRect->left, (NvU32)pRect->top,
            (NvU32)pRect->right, (NvU32)pRect->bottom);

}

} // namespace android
