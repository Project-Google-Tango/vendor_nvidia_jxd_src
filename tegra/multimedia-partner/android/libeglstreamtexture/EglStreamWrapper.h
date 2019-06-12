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

#ifndef ANDROID_EGLSTREAMWRAPPER_H
#define ANDROID_EGLSTREAMWRAPPER_H

#include <gui/ISurfaceTexture.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_nv.h>

namespace android {

extern "C"
{

typedef enum
{
    /* EglStreamTexture */
    AL_AS_EGL_PRODUCER = 1,
    /* EGLStreamConsumer */
    AL_AS_EGL_CONSUMER

} eEglObjType;

typedef struct EglStreamWrapperRec
{
    NvEglApiImports Imports;
    NvEglApiExports Exports;
    //obj type
    eEglObjType mType;
    //Ptr to Actual Object
    void* mpDataObject;
} EglStreamWrapper;

status_t EglStreamWrapperInitializeEgl(void);
void EglStreamWrapperClose(void);

extern PFNEGLQUERYSTREAMKHRPROC g_EglStreamWrapperEglQueryStreamKHR;
extern NvEglApiExports g_EglStreamWrapperExports;


}

}

#endif //ANDROID_EGLSTREAMWRAPPER_H
