/*
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvcamerahal3.h"
#include "nvcamerahal3core.h"
#include "nvcamerahal3streamport.h"
#include "nvcamerahal3metadatahandler.h"
#include "nvmetadatatranslator.h"

#include "nvcamerahal3_tags.h"
#include "nv_log.h"

namespace android {

#if NV_POWERSERVICE_ENABLE
void NvCameraHal3Core::sendCamPowerHint(NvS32 type, NvBool isHighFps)
{
    NvU32 hint = POWER_HINT_CAMERA, data = CAMERA_HINT_COUNT;
    switch(type)
    {
        case CAMERA3_TEMPLATE_PREVIEW:
            data = CAMERA_HINT_STILL_PREVIEW_POWER;
            break;
        case CAMERA3_TEMPLATE_VIDEO_RECORD:
            if (isHighFps)
            {
                data = CAMERA_HINT_HIGH_FPS_VIDEO_RECORD_POWER;
            }
            else
            {
                data = CAMERA_HINT_VIDEO_RECORD_POWER;
            }
            break;
        case CAMERA3_TEMPLATE_VIDEO_SNAPSHOT:
        case CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG:
        case CAMERA3_TEMPLATE_STILL_CAPTURE:
            data = CAMERA_HINT_PERF;
            break;

        default:
            break;
    }

    if (data != CAMERA_HINT_COUNT)
    {
        mPowerServiceClient->sendPowerHint(hint, &data);
    }

    mPrevType = type;

}
#endif

NvError NvCameraHal3Core::routeRequestToCAM(Request& request)
{
    NV_LOGD(HAL3_ROUTER_TAG, "%s: ++",__FUNCTION__);
    NvError e = NvSuccess;
    // TODO: this handle is not being initialized anywhere in the code right now

    size_t max_res = 0;
    size_t max_tnr_res = 0;
    StreamPort *maxres_priv = NULL;
    camera3_stream_buffer *maxres_buf = NULL;
    StreamPort *maxres_tnr_priv = NULL;
    camera3_stream_buffer *maxres_tnr_buf = NULL;
    PortMap *pTNRMap = NULL;

    HalBufferVector::iterator buf;
    buffer_handle_t *in_buffer = NULL;
    NvRectF32 crop_ratio  = {0.0, 0.0, 0.0, 0.0};

    if (NULL == request.settings && mCachedSettings.isEmpty())
    {
        NV_LOGE("%s: Request %d: NULL settings for first request after"
                "configureStreams()", __FUNCTION__, request.frameNumber);
        return NvError_BadParameter;
    }

    mPortMutex.lock();

    if (NULL != request.settings)
    {
        mCachedSettings = request.settings;
        NvS32 type = NvMetadataTranslator::captureIntent(mCachedSettings);
        if (type >= CAMERA3_TEMPLATE_PREVIEW &&
            type < CAMERA3_TEMPLATE_COUNT)
        {
            mCachedControls = mPublicControls[type];
        }
        else
        {
            mCachedControls = mPublicControls[CAMERA3_TEMPLATE_PREVIEW];
            NV_LOGE("%s: Failed to get capture intent", __FUNCTION__);
        }
        const NvCamProperty_Public_Static& statProp =
            mStaticCameraInfoMap.valueFor((int)mSensorId);
        NvMetadataTranslator::translateToNvCamProperty(
            mCachedSettings, mCachedControls,
            statProp, mSensorMode);

#if NV_POWERSERVICE_ENABLE
        //send power hints when there is a usecase change
        if (((mPowerServiceClient) && (type != mPrevType)) ||
            (mHighFpsMode != mCachedControls.CoreCntrlProps.highFpsRecording))
        {
            sendCamPowerHint(type, (mCachedControls.CoreCntrlProps.highFpsRecording));
            mHighFpsMode = mCachedControls.CoreCntrlProps.highFpsRecording;
        }
#endif
    }

    // Create a portMap between physical and max-res streamPort.
    PortMap *pMap = new PortMap;
    pMap->frameNumber = request.frameNumber;
    pMap->settings = mCachedSettings;
    pMap->ctrlProp = mCachedControls;

    buf = request.buffers->begin();
    while (buf != request.buffers->end())
    {
        StreamPort* port = static_cast<StreamPort*>(buf->stream->priv);

        NV_LOGV(HAL3_ROUTER_TAG, "%s: frame #: %d, anb = %p", __FUNCTION__,
            request.frameNumber, buf->buffer);
        if ( mTNREnabled && buf->stream->height <= MAX_TNR_YRES && buf->stream->format == HAL_PIXEL_FORMAT_YV12)
        {
            if (pTNRMap == NULL)
            {
                pTNRMap = new PortMap;
                pTNRMap->frameNumber = request.frameNumber;
                pTNRMap->settings = mCachedSettings;
                pTNRMap->ctrlProp = mCachedControls;
            }

            if ((buf->stream->width * buf->stream->height) >= max_tnr_res)
            {
                max_tnr_res = buf->stream->width * buf->stream->height;
                maxres_tnr_priv = port;
                maxres_tnr_buf = buf;
            }

            pTNRMap->ports.push_back(port);
        }
        else
        {
            if ((buf->stream->width * buf->stream->height) >= max_res)
            {
                max_res = buf->stream->width * buf->stream->height;
                maxres_priv = port;
                maxres_buf = buf;
            }

            pMap->ports.push_back(port);
        }


        NV_CHECK_ERROR_CLEANUP(
            port->enqueueOutputBuffer(buf, request.frameNumber,
                pMap->ctrlProp)
        );
        buf++;
    }

    /*
     * Use a zoom stream if needed
     */
    if((maxres_tnr_buf&&!maxres_buf)||
       (maxres_buf&&(!maxres_priv->isBayer() && !request.input_buffer &&
                     ((isNewCrop(pMap->ctrlProp.CoreCntrlProps.ScalerCropRegion,
                                 maxres_buf->stream->width, maxres_buf->stream->height,
                                 &crop_ratio) ||
                       ((NvS32)maxres_buf->stream->width != mSensorMode.Resolution.width ||
                        (NvS32)maxres_buf->stream->height != mSensorMode.Resolution.height) ||
                       (maxres_priv->streamFormat() == HAL_PIXEL_FORMAT_BLOB) ||
                       (maxres_priv->streamFormat() == HAL_PIXEL_FORMAT_YCrCb_420_SP))))))
    {
        ALOGD("%s: new crop region (%f, %f, %f, %f)", __FUNCTION__,
              crop_ratio.left, crop_ratio.top,
              crop_ratio.right, crop_ratio.bottom);

        // Max-res stream will copy from zoom stream.
        maxres_priv = (StreamPort *)mZoomStream.priv;
        maxres_buf = &mZoomStreamBuffer;

        // get one available buffer in zoom stream.
        maxres_buf->buffer = maxres_priv->getZoomBuffer();
        // set crop region in zoom buffer.
        // crop region will be used in scaling in threadloop.
        maxres_priv->setCropInZoomBuffer(
            maxres_buf->buffer, &crop_ratio);
        NV_CHECK_ERROR_CLEANUP(
            maxres_priv->enqueueOutputBuffer(maxres_buf, request.frameNumber,
                                             pMap->ctrlProp)
            );

        // Add zoom stream to portMap
        sp<StreamPort> port =
            static_cast<StreamPort*>(maxres_buf->stream->priv);
        pMap->ports.push_back(port);
        ALOGD("%s: max res %dx%d", __FUNCTION__,
              maxres_buf->stream->width,
              maxres_buf->stream->height);
    }
    if (maxres_tnr_buf)
    {
        if( !request.input_buffer &&
            (
            (maxres_buf && isNewCrop(pMap->ctrlProp.CoreCntrlProps.ScalerCropRegion,
            maxres_buf->stream->width, maxres_buf->stream->height,
            &crop_ratio)) ||
            (isNewCrop(pMap->ctrlProp.CoreCntrlProps.ScalerCropRegion,
                maxres_tnr_buf->stream->width, maxres_tnr_buf->stream->height,
                &crop_ratio)) ||
            (maxres_tnr_buf->stream->width != mTNRWidth ||
             maxres_tnr_buf->stream->height != mTNRHeight)) )
        {
            NV_LOGV(HAL3_ROUTER_TAG, "%s: new crop region (%f, %f, %f, %f)", __FUNCTION__,
                crop_ratio.left, crop_ratio.top,
                crop_ratio.right, crop_ratio.bottom);

            maxres_tnr_priv = (StreamPort *)mTNRZoomStream.priv;
            maxres_tnr_buf = &mTNRZoomStreamBuffer;
            // get one available buffer in zoom stream.
            maxres_tnr_buf->buffer = maxres_tnr_priv->getZoomBuffer();
            // set crop region in zoom buffer.
            // crop region will be used in scaling in threadloop.
            maxres_tnr_priv->setCropInZoomBuffer(
                maxres_tnr_buf->buffer, &crop_ratio);
            NV_CHECK_ERROR_CLEANUP(
                maxres_tnr_priv->enqueueOutputBuffer(maxres_tnr_buf, request.frameNumber,
                                                     pMap->ctrlProp)
                );
            // Add zoom stream to portMap
            sp<StreamPort> port =
                static_cast<StreamPort*>(maxres_tnr_buf->stream->priv);
            pTNRMap->ports.push_back(port);
        }
    }

    pMap->isReprocessing = false;
    if (request.input_buffer)
    {
        in_buffer = request.input_buffer->buffer;
        pMap->isReprocessing = true;
    }
    else if(maxres_buf)
    {
        in_buffer = maxres_buf->buffer;
    }

    if (in_buffer) {
        pMap->inBuffer = in_buffer;
        mPortMapList.push_back(pMap);
    }

    if (maxres_tnr_buf)
    {
        pTNRMap->inStreamBuffer = maxres_tnr_buf;
        pTNRMap->inBuffer = maxres_tnr_buf->buffer;
        mPortMapList.push_back(pTNRMap);
    }

    mPortMutex.unlock();

    if ( !request.input_buffer )
    {
        NV_CHECK_ERROR_CLEANUP(
            pushCameraBufferToCAM(request.frameNumber, maxres_buf,  maxres_tnr_buf, NULL,
                pMap->ctrlProp, request.frameCaptureRequestId));
    }
    else
    {
        // pass output buffers as NULL for now
        // NvCameraCore will simply return input buffer back to HAL
        NV_CHECK_ERROR_CLEANUP(
            pushCameraBufferToCAM(request.frameNumber, NULL, NULL,
                request.input_buffer, pMap->ctrlProp,
                request.frameCaptureRequestId));
    }

    NV_LOGD(HAL3_ROUTER_TAG, "%s: --",__FUNCTION__);
    return NvSuccess;

fail:
    NV_LOGE("%s: -- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvMMBuffer *NvCameraHal3Core::getIntermediateTNRBuffer(camera3_stream_buffer *buffer)
{
    StreamPort* port = static_cast<StreamPort*>(buffer->stream->priv);
    return port->getTNRBuffer();
}

NvError NvCameraHal3Core::routeResultFromCAM(NvU32 frameNumber,
                                            buffer_handle_t *buffer,
                                            const NvCamProperty_Public_Dynamic& frameDynamicProps)
{
    NV_LOGD(HAL3_ROUTER_TAG, "%s: ++",__FUNCTION__);
    NvError e = NvSuccess;

    HalBufferVector::iterator buf;
    buffer_handle_t *pixelbuf = 0;
    NvBool buffer_owner = true;

    mPortMutex.lock();

    PortMap *portMap = NULL;
    PortMapList::iterator p;
    PortList::iterator pl;

    RouterBuffer *inBuffer = new RouterBuffer;
    inBuffer->buffer = buffer;
    inBuffer->nvmm_buffer = NativeToNVMMBuffer(
                                buffer,
                                &inBuffer->isCropped,
                                &inBuffer->rect);

    if (inBuffer->nvmm_buffer == NULL)
    {
        NV_CHECK_ERROR_CLEANUP(NvError_BadValue);
    }

    for (p = mPortMapList.begin(); p != mPortMapList.end(); ++p)
    {
        if ((*p)->inBuffer == buffer)
        {
            portMap = *p;
            for (pl = portMap->ports.begin(); pl != portMap->ports.end(); ++pl)
            {
                sp<StreamPort> port = *pl;
                inBuffer->RefInc();
                if (port->isOwner(buffer))
                {
                    inBuffer->owner = port;
                    buffer_owner = false;
                }
            }
            break;
        }
    }

    if (portMap)
    {
        NvCameraHal3_Public_Dynamic frameDynProp;
        frameDynProp.CoreDynProps = frameDynamicProps;
        frameDynProp.sensorMode = mSensorMode;
        process3ADynamics(frameDynProp);

        const NvCamProperty_Public_Static &statProp =
            mStaticCameraInfoMap.valueFor((int)mSensorId);

        // Check if we need to send 3A results with this final metadata
        frameDynProp.send3AResults = false;
        if (portMap->isReprocessing || (statProp.NoOfResultsPerFrame == 1))
        {
            frameDynProp.send3AResults = true;
        }
        NV_CHECK_ERROR_CLEANUP(
            NvMetadataTranslator::translateFromNvCamProperty(frameDynProp,
                                                             portMap->ctrlProp,
                                                             portMap->settings,
                                                             statProp)
        );
        // Send settings to metadata handler.
        mMetadataHandler->addSettings(portMap->settings, portMap->frameNumber);

        for (pl = portMap->ports.begin(); pl != portMap->ports.end();)
        {
            sp<StreamPort> port = *pl;
            port->processResult(portMap->frameNumber, inBuffer,
                    &frameDynamicProps);
            pl = portMap->ports.erase(pl);
        }

        p = mPortMapList.erase(p);
        delete portMap;
        portMap = 0;
    }

    mPortMutex.unlock();
    // TODO: spawn a thread for this cleanup
    if (buffer_owner == true)
    {
        inBuffer->Wait();
        delete inBuffer;
    }

    NV_LOGD(HAL3_ROUTER_TAG, "%s: --",__FUNCTION__);
    return NvSuccess;

fail:
    NV_LOGE("%s: -- error [0x%x]", __FUNCTION__, e);
    return e;
}

void NvCameraHal3Core::updateDebugControls(NvCamProperty_Public_Controls& ctrlProp)
{
    int index = mStaticCameraInfoMap.indexOfKey((int)mSensorId);
    if (index < 0 || index >= (int)mStaticCameraInfoMap.size())
    {
        NV_LOGE("%s Failed to retrieve Static properties", __FUNCTION__);
        return;
    }

    const NvCamProperty_Public_Static& statProp =
        mStaticCameraInfoMap.valueFor((int)mSensorId);

    if (mDbgPreviewFps)
    {
        ctrlProp.AeTargetFpsRange.low = (NvF32)mDbgPreviewFps;
        ctrlProp.AeTargetFpsRange.high = (NvF32)mDbgPreviewFps;
        ctrlProp.SensorExposureTime = 1 / (NvF32)mDbgPreviewFps;
    }

    /*
     * Fix focus position.
     * Move focus to max or min position.
     * Usage: adb shell setprop camera.debug.fixfocus <mode>
     * <mode> 1: macro, 2: infinite
     */
    if (mDbgFocusMode != 0)
    {
        ctrlProp.AfMode = NvCamAfMode_Off;
        if (mDbgFocusMode == 1)
        {
            ctrlProp.LensFocusDistance =
                statProp.LensAvailableFocalLengths.Values[statProp.LensAvailableFocalLengths.Size-1];
        }
        else if (mDbgFocusMode == 2)
        {
            ctrlProp.LensFocusDistance = 0;
        }
    }
}

NvError NvCameraHal3Core::pushCameraBufferToCAM(uint32_t frameNumber,
            camera3_stream_buffer *pOutputStreamBuffer,
            camera3_stream_buffer *pOutputStreamTNRBuffer,
            camera3_stream_buffer *pInputStreamBuffer,
            NvCameraHal3_Public_Controls& ctrlProp,
            NvU64 frameCaptureRequestId)
{
    NvError e = NvSuccess;
    StreamPort* port = NULL;
    NvMMBuffer* pOutputBuffer = NULL;
    NvMMBuffer* pInputBuffer = NULL;
    NvCameraCoreFrameCaptureRequest frameRequest;
    NvOsMemset(&frameRequest, 0, sizeof(NvCameraCoreFrameCaptureRequest));

    NV_LOGD(HAL3_ROUTER_TAG, "%s: ++", __FUNCTION__);

    if (pInputStreamBuffer != NULL)
    {
        if (pInputStreamBuffer->stream->priv == NULL)
        {
            NV_LOGE("%s: input stream[%p] has not been configured",
                __FUNCTION__, pInputStreamBuffer->stream);
            NV_CHECK_ERROR_CLEANUP(NvError_BadValue);
        }

        port = static_cast<StreamPort*> (pInputStreamBuffer->stream->priv);
        pInputBuffer = port->NativeToNVMMBuffer(pInputStreamBuffer->buffer);
        frameRequest.FrameCaptureRequestId = (NvU64)pInputStreamBuffer->buffer;

        if (!pInputBuffer)
        {
            NV_LOGE("%s: input stream[%p] - buffer[%p] is not registered",
                __FUNCTION__, pInputStreamBuffer->stream, pInputBuffer);
            NV_CHECK_ERROR_CLEANUP(NvError_BadValue);
        }

        NV_LOGV(HAL3_ROUTER_TAG, "%s: push input anb[%p] nvmm[%p] %dx%d,", __FUNCTION__,
            pInputStreamBuffer->buffer, pInputBuffer,
            pInputStreamBuffer->stream->width,
            pInputStreamBuffer->stream->height);

        // NvCameraCore only returns output buffer as a part of result.
        // Temporarily, set the output buffer to be the same as input
        // since Core doesn't reprocess input stream atm.
        pOutputBuffer = pInputBuffer;
    }
    // TODO: Remove this else statement.
    // With full reprocessing support from NvCamerCore, we will also send
    // the ouput buffer down with the input request. Until then, we make
    // a distinction betwee input/output request with this condition.
    else
    {
        if (pOutputStreamBuffer) {
            if (pOutputStreamBuffer->stream->priv == NULL)
            {
                NV_LOGE("%s: output stream[%p] - buffer[%p] is not registered",
                    __FUNCTION__, pOutputStreamBuffer->stream, pOutputBuffer);
                NV_CHECK_ERROR_CLEANUP(NvError_BadValue);
            }

            port = static_cast<StreamPort*> (pOutputStreamBuffer->stream->priv);
            pOutputBuffer = port->NativeToNVMMBuffer(pOutputStreamBuffer->buffer);

            if (!pOutputBuffer)
            {
                NV_LOGE("%s: output stream[%p] - buffer[%p] is not registered",
                      __FUNCTION__, pOutputStreamBuffer->stream, pOutputBuffer);
                NV_CHECK_ERROR_CLEANUP(NvError_BadValue);
            }
            frameRequest.FrameCaptureRequestId = frameCaptureRequestId;

            NV_LOGD("%s: push anb[%p] nvmm[%p] %dx%d,", __FUNCTION__,
                  pOutputStreamBuffer->buffer, pOutputBuffer,
                  pOutputStreamBuffer->stream->width,
                  pOutputStreamBuffer->stream->height);
        }
    }

    NV_CHECK_ERROR_CLEANUP(
        process3AControls(ctrlProp)
    );
    updateDebugControls(ctrlProp.CoreCntrlProps);
    updateDeviceOrientation(ctrlProp.CoreCntrlProps);

    if (pOutputStreamTNRBuffer)
    {
        enqueueTNRBuffer(frameNumber, pOutputStreamTNRBuffer);
    }

    frameRequest.FrameNumber = frameNumber;
    frameRequest.pInputBuffer = pInputBuffer;
    frameRequest.NumOfOutputBuffers = 1;
    frameRequest.ppOutputBuffers = &pOutputBuffer;
    frameRequest.FrameControlProps = ctrlProp.CoreCntrlProps;

    NV_CHECK_ERROR_CLEANUP(
        NvCameraCore_FrameCaptureRequest(
            hCameraCore,
            &frameRequest)
    );

    NV_LOGD(HAL3_ROUTER_TAG, "%s: --", __FUNCTION__);
    return NvSuccess;
fail:
    NV_LOGE("%s: -- error [0x%x]", __FUNCTION__, e);
    return e;
}


NvError NvCameraHal3Core::enqueueTNRBuffer(int frameNumber, camera3_stream_buffer *buffer)
{
    Mutex::Autolock lock(mTNRbufferQMutex);

    BufferInfo *pBufferInfo = new BufferInfo;

    pBufferInfo->outBuffer = new camera3_stream_buffer(*buffer);
    pBufferInfo->frameNumber = frameNumber;

    mTNR_Queue.push_back(pBufferInfo);

    return NvSuccess;
}


camera3_stream_buffer *NvCameraHal3Core::dequeueTNRBuffer(int frameNumber)
{
    Mutex::Autolock lock(mTNRbufferQMutex);

    BufferInfoQueue::iterator itr = mTNR_Queue.begin();
    while (itr != mTNR_Queue.end())
    {
        BufferInfo *pBufferInfo = (*itr);
        if (pBufferInfo->frameNumber == frameNumber)
        {
            camera3_stream_buffer *buffer;
            buffer = pBufferInfo->outBuffer;
            mTNR_Queue.erase(itr);
            delete pBufferInfo;
            return buffer;
        }
        ++itr;
    }

    return NULL;
}



} //namespace android
