/*
 * Copyright (C) 2012 NVIDIA Corporation
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

//#define LOG_NDEBUG 0
#define LOG_TAG "Wrapper"
#include <utils/Log.h>
#include <dlfcn.h>

#include "eglapiinterface.h"
#include "EglStreamWrapper.h"
#include "EglStreamTextureWrapper.h"
#include "EglStreamTexture.h"
#include "EGLStreamConsumer.h"

#if defined(__cplusplus)
extern "C"
{
#endif
#include "nvgralloc.h"
#include "nvrm_memmgr.h"

typedef void (*NvEglRegClientApiType) (NvEglApiIdx apiId, NvEglApiInitFnPtr apiInitFnct);

PFNEGLQUERYSTREAMKHRPROC g_EglStreamWrapperEglQueryStreamKHR;
NvEglApiExports g_EglStreamWrapperExports;

static void* eglLibraryNV = NULL;

#if defined(__cplusplus)
}
#endif

namespace android {

Mutex gEglStreamWrapperLock;

extern "C" NvError EglStreamWrapperStreamAttribImport(NvEglApiStreamInfo info, NvU32 attr, NvS32 val);
extern "C" NvError EglStreamWrapperStreamAttribImport(NvEglApiStreamInfo info, NvU32 attr, NvS32 val)
{
    ALOGV("%s[%d]\n", __FUNCTION__, __LINE__);
    EglStreamWrapper* wr = (EglStreamWrapper*)info;

    if (wr->mType == AL_AS_EGL_PRODUCER)
    {
        EglStreamTexture *p = (EglStreamTexture*)wr->mpDataObject;
        return (NvError)p->setEglStreamAttrib(attr, val);
    }
    else
    {
        EGLStreamConsumer *p = (EGLStreamConsumer*)wr->mpDataObject;
        return (NvError)p->setEglStreamAttrib(attr, val);
    }
}

extern "C" NvError EglStreamWrapperStreamReturnFrameImport(NvEglApiStreamInfo info, NvEglApiStreamFrame *frame);
extern "C" NvError EglStreamWrapperStreamReturnFrameImport(NvEglApiStreamInfo info, NvEglApiStreamFrame *frame)
{
    ALOGV("%s[%d]\n", __FUNCTION__, __LINE__);
    EglStreamWrapper* wr = (EglStreamWrapper*)info;

    if (wr->mType == AL_AS_EGL_PRODUCER)
    {
        EglStreamTexture *p = (EglStreamTexture*)wr->mpDataObject;
        return (NvError)p->returnEglStreamFrame(frame);
    }
    else
    {
        return NvSuccess;
    }
}

extern "C" NvError EglStreamWrapperStreamDisconnectImport(NvEglApiStreamInfo info);
extern "C" NvError EglStreamWrapperStreamDisconnectImport(NvEglApiStreamInfo info)
{
    ALOGV("%s[%d]\n", __FUNCTION__, __LINE__);
    EglStreamWrapper* wr = (EglStreamWrapper*)info;

    if (wr->mType == AL_AS_EGL_PRODUCER)
    {
        EglStreamTexture *p = (EglStreamTexture*)wr->mpDataObject;
        return (NvError)p->eglStreamDisconnect();
    }
    else
    {
        EGLStreamConsumer *p = (EGLStreamConsumer*)wr->mpDataObject;
        return (NvError)p->eglStreamDisconnect();
    }
}

extern "C" NvError NvOMXALEGLInit(NvEglApiImports *imports, const NvEglApiExports *exports);
extern "C" NvError NvOMXALEGLInit(NvEglApiImports *imports, const NvEglApiExports *exports)
{
    ALOGV("%s[%d]\n", __FUNCTION__, __LINE__);
    g_EglStreamWrapperExports = *exports;
    imports->streamSetAttrib = EglStreamWrapperStreamAttribImport;
    imports->streamReturnFrame = EglStreamWrapperStreamReturnFrameImport;
    imports->streamRelease = EglStreamWrapperStreamDisconnectImport;
    return NvSuccess;
}

extern "C" status_t EglStreamWrapperInitializeEgl(void)
{
    static const char* eglNameNV = "egl/libEGL_tegra.so";
    NvEglRegClientApiType regf = NULL;
    EGLBoolean result;
    EGLStreamKHR eglStream;

    ALOGV("%s[%d] + \n", __FUNCTION__, __LINE__);

    Mutex::Autolock lock(gEglStreamWrapperLock);

    // Check if library is already successfully loaded
    if (eglLibraryNV)
    {
        ALOGE("EGL Library is loaded");
    }
    else
    {
        // Load our EGL library and get the registeration function
        eglLibraryNV = dlopen(eglNameNV, RTLD_NOW);
        if (!eglLibraryNV)
        {
            ALOGE("Cannot load EGL");
            return BAD_VALUE;
        }
    }

    regf = (NvEglRegClientApiType)dlsym(eglLibraryNV, "NvEglRegClientApi");

    if ( !regf )
    {
        ALOGE("Cannot find function NvEglRegClientApi");
        dlclose(eglLibraryNV);
        eglLibraryNV = NULL;
        return BAD_VALUE;
    }

    regf(NV_EGL_API_OMX_AL, NvOMXALEGLInit);

#ifdef FRAMEWORK_HAS_DISPLAY_LATENCY_ATTRIBUTE
    g_EglStreamWrapperEglQueryStreamKHR = (PFNEGLQUERYSTREAMKHRPROC)eglGetProcAddress("eglQueryStreamKHR");
    if (!g_EglStreamWrapperEglQueryStreamKHR)
        ALOGI("Failed to find extension function eglQueryStreamKHR");
#endif

    ALOGV("%s[%d] - \n", __FUNCTION__, __LINE__);
    return OK;
}

extern "C" void EglStreamWrapperClose(void)
{
    if (eglLibraryNV != NULL)
    {
        dlclose(eglLibraryNV);
        eglLibraryNV = NULL;
     }
}

extern "C" const
sp<ISurfaceTexture> NvGetEGLStreamTexture(EGLStreamKHR stream, EGLDisplay dpy)
{
    ALOGV("%s[%d]", __FUNCTION__, __LINE__);

    sp<EglStreamTexture> eglStream = new EglStreamTexture(stream, dpy);
    return eglStream;
}


extern "C" const void * NvGetEGLStreamConsumer(EGLStreamKHR stream, EGLDisplay dpy,
                                        int nWidth /* Consumer width */,
                                        int nHeight /* Consumer height */)
{
    ALOGV("%s[%d]", __FUNCTION__, __LINE__);

    EGLStreamConsumer *aConsumerObj = NULL;
    aConsumerObj = new EGLStreamConsumer(stream, dpy, nWidth, nHeight);
    return aConsumerObj;
}

extern "C" sp<IMemory>  NvGetNextBuffer(void* pContext,
                                        void *pTimestamp /* timestamp of retframe */,
                                        void *pRetStatus /* eFrameReqStatus */,
                                        void* pTimeOut   /* NvU64: nanosec */)
{
    EGLStreamConsumer *pCurObj = NULL;

    if(NULL != pContext)
    {
        pCurObj = (EGLStreamConsumer*) pContext;
        return pCurObj->getNextFrame(pTimestamp, pRetStatus, pTimeOut);
    }
    else
        return NULL;
}

extern "C" status_t ReleaseRecordedFrame(void* pContext, sp<IMemory>& frame)
{
    ALOGV("%s[%d]", __FUNCTION__, __LINE__);

    EGLStreamConsumer *pCurObj = NULL;

    if(NULL != pContext)
    {
        pCurObj = (EGLStreamConsumer*) pContext;
        return pCurObj->returnRecordedFrame(frame);
    }
    else
        return NvError_BadParameter;
}

extern "C" status_t ReleaseResources(void* pContext)
{
    ALOGV("%s[%d]", __FUNCTION__, __LINE__);
    EGLStreamConsumer *pCurObj = NULL;

    if(NULL != pContext)
    {
        pCurObj = (EGLStreamConsumer*) pContext;
        pCurObj->stop();
        delete pCurObj;
        return OK;
    }
    return NvError_BadParameter;
}

}
