/*
 * Copyright (C) 2011 NVIDIA Corporation
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

/* \file CMediaRecorder.c MediaRecorder class */

#include "sles_allinclusive.h"
#ifdef ANDROID
#include "android/android_mediarecorder.h"
#include "android/android_cameradevice.h"
#endif

#ifdef LINUX_OMXAL
#include "linux_objects.h"
#endif

XAresult CMediaRecorder_Realize(void *self, XAboolean async)
{
    XAresult result = XA_RESULT_SUCCESS;
    CMediaRecorder *pMediaRecorder = (CMediaRecorder *) self;
#ifdef ANDROID
    if(pMediaRecorder->mode != CMEDIARECORDER_SNAPSHOT)
        result = android_mediaRecorder_realize(pMediaRecorder, async);
    else
        result = android_cameraSnapshot_realize(pMediaRecorder, async);
#endif
#ifdef LINUX_OMXAL
    InitializeFramework();
    result = ALBackendMediaRecorderRealize(pMediaRecorder, async);
#endif
    return result;
}

XAresult CMediaRecorder_Resume(void *self, XAboolean async)
{
    XAresult result = XA_RESULT_SUCCESS;
    CMediaRecorder *pMediaRecorder = (CMediaRecorder *) self;
#ifdef ANDROID
    if(pMediaRecorder->mode != CMEDIARECORDER_SNAPSHOT)
        result = android_mediaRecorder_resume(pMediaRecorder, async);
    else
        result = android_cameraSnapshot_resume(pMediaRecorder, async);
#endif
#ifdef LINUX_OMXAL
    result = ALBackendMediaRecorderResume(pMediaRecorder, async);
#endif
    return result;
}

void CMediaRecorder_Destroy(void *self)
{
    CMediaRecorder *pMediaRecorder = (CMediaRecorder *) self;
#ifdef ANDROID
    if(pMediaRecorder->mode != CMEDIARECORDER_SNAPSHOT)
        android_mediaRecorder_destroy(pMediaRecorder);
    else
        android_cameraSnapshot_destroy(pMediaRecorder);
#endif
#ifdef LINUX_OMXAL
    ALBackendMediaRecorderDestroy(pMediaRecorder);
#endif
}


predestroy_t CMediaRecorder_PreDestroy(void *self)
{
    return predestroy_ok;
}

