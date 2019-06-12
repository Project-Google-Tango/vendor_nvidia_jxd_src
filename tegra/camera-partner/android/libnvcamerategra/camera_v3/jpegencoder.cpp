/*
 * Copyright (c) 2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcamerahal3common.h"
#include "jpegencoder.h"
#include "nvcamerahal3_tags.h"
#include "nv_log.h"
#include "camera_trace.h"

namespace android
{

JpegEncoder::JpegEncoder()
    :hImageEnc(NULL),
    mEncoding(NV_FALSE)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: ++", __FUNCTION__);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: --", __FUNCTION__);
}

JpegEncoder::~JpegEncoder()
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: ++", __FUNCTION__);
    Close();
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: --", __FUNCTION__);
}

NvError JpegEncoder::Open(NvU32 levelOfSupport, NvImageEncoderType type)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: ++", __FUNCTION__);
    NvError err;
    // just for safety; carried over from V1
    Close();
    NvImageEncOpenParameters params;
    NvOsSemaphoreCreate(&mJpegSema, 0);

    params.pContext = this;
    params.Type = type;
    params.LevelOfSupport = levelOfSupport;
    params.ClientIOBufferCB = JpegEncoder::imageEncoderCallback;

    err = NvImageEnc_Open(&hImageEnc, &params);

    NV_LOGV(HAL3_JPEG_PROCESS_TAG, "%s: 0x%x", __FUNCTION__, err);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: --", __FUNCTION__);
    return err;
}

void JpegEncoder::Close()
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: ++", __FUNCTION__);
    if (hImageEnc)
    {
        // wait for any pending job to complete
        mutex.lock();
        while (mEncoding)
        {
            cond.wait(mutex);
        }
        mutex.unlock();
        // Encoding is complete; close the handle
        NvImageEnc_Close(hImageEnc);
        hImageEnc = NULL;
        if (mJpegSema)
        {
            NvOsSemaphoreDestroy(mJpegSema);
        }
    }
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: --", __FUNCTION__);
}

NvError JpegEncoder::GetParam(NvJpegEncParameters *params)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: ++", __FUNCTION__);
    if (!hImageEnc)
    {
        return NvError_NotInitialized;
    }
    NvError err;
    err = NvImageEnc_GetParam(hImageEnc, params);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: -- with error: 0x%x", __FUNCTION__, err);
    return err;
}

NvError JpegEncoder::SetParam(NvJpegEncParameters *params)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: ++", __FUNCTION__);
    if (!hImageEnc)
    {
        return NvError_NotInitialized;
    }
    NvError err;
    err = NvImageEnc_SetParam(hImageEnc, params);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: -- with error: 0x%x", __FUNCTION__, err);
    return err;
}

NvError JpegEncoder::Encode(NvMMBuffer *InputBuffer, NvMMBuffer *OutPutBuffer,
    NvMMBuffer *thumbBuffer)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s ++", __FUNCTION__);
    mutex.lock();
    mEncoding = NV_TRUE;
    mutex.unlock();
    if (!hImageEnc)
    {
        return NvError_NotInitialized;
    }
    NvError err = NvSuccess;
    NvError errThumb = NvSuccess;
    NV_LOGV(HAL3_JPEG_PROCESS_TAG, "%s: Submitting primary encoding request InputBuffer = %p",
                __FUNCTION__, InputBuffer);
    err = NvImageEnc_Start(hImageEnc, InputBuffer, OutPutBuffer, NV_FALSE);
    if (err != NvSuccess)
    {
        NV_LOGE("%s: Failed to submit Primary Jpeg encoding request, err = 0x%x",
                __FUNCTION__, err);
        NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s --", __FUNCTION__);
        return err;
    }
    // check if thumbnail is requested
    else if (thumbBuffer)
    {
        NV_LOGV(HAL3_JPEG_PROCESS_TAG, "%s: Submitting thumbnail encoding request thumbBuffer = %p",
                    __FUNCTION__, thumbBuffer);
        // as long as we send the still buffer first and give an output with
        // that, jpeg lib says this can be NULL
        errThumb = NvImageEnc_Start(hImageEnc, thumbBuffer, NULL, NV_TRUE);
        if (errThumb != NvSuccess)
        {
            NV_LOGE("%s: Failed to submit Thumbnail Jpeg encoding request, err = 0x%0x",
                    __FUNCTION__, errThumb);
            return errThumb;
        }
    }
    NV_LOGV(HAL3_JPEG_PROCESS_TAG, "%s: Waiting for Primary encode", __FUNCTION__);
    // Wait for all the Jpeg requests to get complete
    NvOsSemaphoreWait(mJpegSema);
    NV_LOGV(HAL3_JPEG_PROCESS_TAG, "%s: Primary done", __FUNCTION__);
    mutex.lock();
    mEncoding = NV_FALSE;
    cond.signal();
    mutex.unlock();
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: -- with error: 0x%x", __FUNCTION__, errThumb);
    return errThumb;
}

void JpegEncoder::imageEncoderCallback(void* pContext, NvU32 streamIndex,
        NvU32 BufferSize, void *pBuffer, NvError EncStatus)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: ++", __FUNCTION__);
    JpegEncoder *jpEnc = (JpegEncoder *)pContext;
    Mutex::Autolock lock(jpEnc->mutex);
    switch (streamIndex)
    {
        case  NvImageEncoderStream_INPUT:
            NV_LOGV(HAL3_JPEG_PROCESS_TAG, "JpegEncoder - NvImageEncoderStream_INPUT");
            break;
        case NvImageEncoderStream_THUMBNAIL_INPUT:
            NV_LOGV(HAL3_JPEG_PROCESS_TAG, "JpegEncoder - NvImageEncoderStream_THUMBNAIL_INPUT");
            break;
        case NvImageEncoderStream_OUTPUT:
            NV_LOGV(HAL3_JPEG_PROCESS_TAG, "JpegEncoder - NvImageEncoderStream_OUTPUT");
            NvOsSemaphoreSignal(jpEnc->mJpegSema);
            break;
    }
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: --", __FUNCTION__);
}

};
