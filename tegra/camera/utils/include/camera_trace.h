/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef _NV_CAMERA_TRACE_H_
#define _NV_CAMERA_TRACE_H_

#ifndef NV_TRACE_OUTPUT
#define NV_TRACE_OUTPUT 0
#endif

#ifndef ANDROID
#undef NV_TRACE_OUTPUT
#define NV_TRACE_OUTPUT 0
#endif

/* Adjust NV_TRACE_OUTPUT in the Makefile
 * Set NV_TRACE_OUTPUT to 0 for disabling trace
 * Set NV_TRACE_OUTPUT to 1 for re-directing trace to systrace
 * Set NV_TRACE_OUTPUT to 2 for re-directing trace to logcat
 */

#if NV_TRACE_OUTPUT == 0

#define NV_TRACE_BEGIN_W(tag)
#define NV_TRACE_BEGIN_I(tag)
#define NV_TRACE_BEGIN_D(tag)
#define NV_TRACE_BEGIN_V(tag)
#define NV_TRACE_END()
#define NV_TRACE_COUNTER(tag, counter)

#define NV_TRACE_ASYNC_BEGIN(tag, cookie)
#define NV_TRACE_ASYNC_END(tag, cookie)

#ifdef __cplusplus
#define NV_TRACE_CALL_W(tag)
#define NV_TRACE_CALL_I(tag)
#define NV_TRACE_CALL_D(tag)
#define NV_TRACE_CALL_V(tag)
#define NV_TRACE_BLOCK_CODE_BEGIN(tag)
#endif

#elif NV_TRACE_OUTPUT == 1

#include <cutils/trace.h>

#define NV_TRACE_BEGIN_W(tag) atrace_begin(ATRACE_TAG_CAMERA, __FUNCTION__)
#define NV_TRACE_BEGIN_I(tag) atrace_begin(ATRACE_TAG_CAMERA, __FUNCTION__)
#define NV_TRACE_BEGIN_D(tag) atrace_begin(ATRACE_TAG_CAMERA, __FUNCTION__)
#define NV_TRACE_BEGIN_V(tag) atrace_begin(ATRACE_TAG_CAMERA, __FUNCTION__)
#define NV_TRACE_BLOCK_CODE_BEGIN(tag) atrace_begin(ATRACE_TAG_CAMERA, tag)
#define NV_TRACE_COUNTER(tag, counter) atrace_int(ATRACE_TAG_CAMERA, tag, counter)
#define NV_TRACE_END() atrace_end(ATRACE_TAG_CAMERA)

#define NV_TRACE_ASYNC_BEGIN(tag, cookie) atrace_async_begin(ATRACE_TAG_CAMERA, tag, cookie)
#define NV_TRACE_ASYNC_END(tag, cookie) atrace_async_end(ATRACE_TAG_CAMERA, tag, cookie)

#ifdef __cplusplus

class NvTraceHelper
{
public:
    NvTraceHelper(int level, const char* tag, const char* name)
    {
        atrace_begin(ATRACE_TAG_CAMERA, name);
    };
    ~NvTraceHelper()
    {
        atrace_end(ATRACE_TAG_CAMERA);
    };
};

#endif

#elif NV_TRACE_OUTPUT == 2

#include <sys/time.h>
#include "nv_log.h"

typedef struct _NvCamProfile
{
    int level;
    const char* pTag;
    const char* pName;
    struct timeval start;
    struct timeval finish;
} NvCamProfile;

#ifdef __cplusplus
extern "C" {
#endif

static inline void NvProfileStart(NvCamProfile* pNvProf)
{
    if (pNvProf->pTag != 0)
    {
        gettimeofday(&pNvProf->start, NULL);
    }
}

static inline void NvProfileEnd(NvCamProfile* pNvProf)
{
    if (pNvProf->pTag == 0)
    {
        return;
    }
    long diff;
    gettimeofday(&pNvProf->finish, NULL);
    diff = pNvProf->finish.tv_usec+(pNvProf->finish.tv_sec*1000000)-
           (pNvProf->start.tv_usec+(pNvProf->start.tv_sec*1000000));
    switch (pNvProf->level)
    {
        case ANDROID_LOG_WARN:
            NV_LOGW(pNvProf->pTag, "%s: %ld", pNvProf->pName, diff);
            break;
        case ANDROID_LOG_INFO:
            NV_LOGI(pNvProf->pTag, "%s: %ld", pNvProf->pName, diff);
            break;
        case ANDROID_LOG_DEBUG:
            NV_LOGD(pNvProf->pTag, "%s: %ld", pNvProf->pName, diff);
            break;
        case ANDROID_LOG_VERBOSE:
            NV_LOGV(pNvProf->pTag, "%s: %ld", pNvProf->pName, diff);
            break;
    }
}

#ifdef __cplusplus
}
#endif

#define NV_TRACE_BEGIN_W(tag) \
    NvCamProfile _nvProf; \
    _nvProf.level = ANDROID_LOG_WARN; \
    _nvProf.pTag = tag; \
    _nvProf.pName = __FUNCTION__; \
    NvProfileStart(&_nvProf);

#define NV_TRACE_BEGIN_I(tag) \
    NvCamProfile _nvProf; \
    _nvProf.level = ANDROID_LOG_INFO; \
    _nvProf.pTag = tag; \
    _nvProf.pName = __FUNCTION__; \
    NvProfileStart(&_nvProf);

#define NV_TRACE_BEGIN_D(tag) \
    NvCamProfile _nvProf; \
    _nvProf.level = ANDROID_LOG_DEBUG; \
    _nvProf.pTag = tag; \
    _nvProf.pName = __FUNCTION__; \
    NvProfileStart(&_nvProf);

#define NV_TRACE_BEGIN_V(tag) \
    NvCamProfile _nvProf; \
    _nvProf.level = ANDROID_LOG_VERBOSE; \
    _nvProf.pTag = tag; \
    _nvProf.pName = __FUNCTION__; \
    NvProfileStart(&_nvProf);

#define NV_TRACE_END() NvProfileEnd(&_nvProf);

#define NV_TRACE_ASYNC_BEGIN(tag, cookie)
#define NV_TRACE_ASYNC_END(tag, cookie)

#ifdef __cplusplus

class NvTraceHelper
{
public:
    NvTraceHelper(int level, const char* tag, const char* name)
    {
        prof.level = level;
        prof.pTag = tag;
        prof.pName = name;
        NvProfileStart(&prof);
    };
    ~NvTraceHelper()
    {
        NvProfileEnd(&prof);
    };
private:
    NvCamProfile prof;
};

#endif

#endif

#ifdef __cplusplus

#if NV_TRACE_OUTPUT == 1 || NV_TRACE_OUTPUT == 2

#define NV_TRACE_CALL_W(tag) \
    NvTraceHelper _traceObj(ANDROID_LOG_WARN, tag, __FUNCTION__)

#define NV_TRACE_CALL_I(tag) \
    NvTraceHelper _traceObj(ANDROID_LOG_INFO, tag, __FUNCTION__)

#define NV_TRACE_CALL_D(tag) \
    NvTraceHelper _traceObj(ANDROID_LOG_DEBUG, tag, __FUNCTION__)

#define NV_TRACE_CALL_V(tag) \
    NvTraceHelper _traceObj(ANDROID_LOG_VERBOSE, tag, __FUNCTION__)

#endif

#endif

#endif
