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

#ifndef ANDROID_GUI_EGLSTREAMTEXTUREWRAPPER_H
#define ANDROID_GUI_EGLSTREAMTEXTUREWRAPPER_H

#include <gui/ISurfaceTexture.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_nv.h>
#include <binder/IMemory.h>
#include <utils/List.h>
#include "nvmm_buffertype.h"
#include "NvxHelpers.h"
#include <OMX_Core.h>

namespace android {

extern "C"
{
// Exported functions

// Producer
const sp<ISurfaceTexture> NvGetEGLStreamTexture(EGLStreamKHR stream, EGLDisplay dpy);

typedef enum
{
    /* EOS is reached, producer closed*/
    eFrameReqStatus_EOS = 1,
    /* TIMEOUT */
    eFrameReqStatus_TIMEOUT,
    /* ACTUAL FRAME */
    eFrameReqStatus_ACTUALFRAME,
    /* ERROR_UNKNOWN */
    eFrameReqStatus_ERROR_UNKNOWN

} eFrameReqStatus;

// Consumer
// void * is EglStreamConsumerAPI object
const void * NvGetEGLStreamConsumer(EGLStreamKHR stream, EGLDisplay dpy,
                                    int nWidth /* Consumer width */,
                                    int nHeight /* Consumer height */);

sp<IMemory>  NvGetNextBuffer(void* pContext,
                             void* pTimestamp /* NvU64: timestamp of retframe */,
                             void* pRetStatus /* eFrameReqStatus */,
                             void* pTimeOut   /* NvU64: nanosec */);

status_t ReleaseRecordedFrame(void* pContext, sp<IMemory>& frame);

status_t ReleaseResources(void* pContext);

}

}

#endif //ANDROID_GUI_EGLSTREAMTEXTUREWRAPPER_H
