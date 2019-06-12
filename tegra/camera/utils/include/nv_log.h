/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef _NV_LOGS_H_
#define _NV_LOGS_H_

/* Adjust NV_LOG_LEVEL in the Makefile */

#ifdef ANDROID
#include <android/log.h>
#endif

#ifndef NV_LOG_LEVEL
#define NV_LOG_LEVEL 0
#endif

#ifndef NV_LOGV
#if NV_LOG_LEVEL < 4
#define NV_LOGV(tag, ...)
#else
#define NV_LOGV(tag, ...) \
    do { \
        if (tag != 0) __android_log_print(ANDROID_LOG_VERBOSE, tag, __VA_ARGS__); \
    } while (0)
#endif
#endif

#ifndef ANDROID
#undef NV_LOG_LEVEL
#define NV_LOG_LEVEL 0
#endif

#ifndef NV_LOGD
#if NV_LOG_LEVEL < 3
#define NV_LOGD(tag, ...)
#else
#define NV_LOGD(tag, ...)  \
    do { \
        if (tag != 0) __android_log_print(ANDROID_LOG_DEBUG, tag, __VA_ARGS__); \
    } while (0)
#endif
#endif

#ifndef NV_LOGI
#if NV_LOG_LEVEL < 2
#define NV_LOGI(tag, ...)
#else
#define NV_LOGI(tag, ...)  \
    do { \
        if (tag != 0) __android_log_print(ANDROID_LOG_INFO, tag, __VA_ARGS__); \
    } while (0)
#endif
#endif

#ifndef NV_LOGW
#if NV_LOG_LEVEL < 1
#define NV_LOGW(tag, ...)
#else
#define NV_LOGW(tag, ...)  \
    do { \
        if (tag != 0) __android_log_print(ANDROID_LOG_WARN, tag, __VA_ARGS__); \
    } while (0)
#endif
#endif

#ifdef ANDROID
#define NV_LOGE(...) \
    do { \
        __android_log_print(ANDROID_LOG_ERROR, "Error", __VA_ARGS__); \
    } while (0)
#else
#define NV_LOGE(...) NvOsDebugPrintf(__VA_ARGS__)
#endif

#endif
