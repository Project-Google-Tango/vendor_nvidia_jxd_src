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

/* \file CCameraDevice.CameraDevice class */

#include "sles_allinclusive.h"
#ifdef ANDROID
#include "android/android_cameradevice.h"
#include "android/android_common.h"
#endif
#ifdef LINUX_OMXAL
#include "linux_objects.h"
#endif

XAresult CCameraDevice_Realize(void *self, XAboolean async)
{
    XAresult result = XA_RESULT_SUCCESS;
    CCameraDevice *pCameraDevice = (CCameraDevice *) self;
#ifdef ANDROID
    result = android_cameraDevice_realize(pCameraDevice, async);
#endif
#ifdef LINUX_OMXAL
    InitializeFramework();
    result = ALBackendCameraDeviceRealize(pCameraDevice, async);
#endif
    return result;
}

XAresult CCameraDevice_Resume(void *self, XAboolean async)
{
    XAresult result = XA_RESULT_SUCCESS;
    CCameraDevice *pCameraDevice = (CCameraDevice *) self;
#ifdef ANDROID
    result = android_cameraDevice_resume(pCameraDevice, async);
#endif
#ifdef LINUX_OMXAL
    result = ALBackendCameraDeviceResume(pCameraDevice, async);
#endif
    return result;
}

void CCameraDevice_Destroy(void *self)
{
    CCameraDevice *pCameraDevice = (CCameraDevice *) self;
#ifdef ANDROID
    android_cameraDevice_destroy(pCameraDevice);
#endif
#ifdef LINUX_OMXAL
    ALBackendCameraDeviceDestroy(pCameraDevice);
#endif
}

predestroy_t CCameraDevice_PreDestroy(void *self)
{
    return predestroy_ok;
}

