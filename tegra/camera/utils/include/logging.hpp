/*
 * Copyright (c) 2012-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __LOGGING_HPP__
#define __LOGGING_HPP__

#ifdef ANDROID
// Convenience macros for logging in android native code
#include <android/log.h>

#ifndef LOG_TAG
#define LOG_TAG "OPENCV_DEMO"
#endif

#define LOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))

#define  LOGI_STREAM(x)  {std::stringstream ss; ss << x; LOGI("%s",ss.str().c_str());}
#define  LOGE_STREAM(x)  {std::stringstream ss; ss << x; LOGE("%s",ss.str().c_str());}

#else

#define LOGD(_str, ...) do{printf(_str , ## __VA_ARGS__); printf("\n");fflush(stdout);} while(0)
#define LOGI(_str, ...) do{printf(_str , ## __VA_ARGS__); printf("\n");fflush(stdout);} while(0)
#define LOGW(_str, ...) do{printf(_str , ## __VA_ARGS__); printf("\n");fflush(stdout);} while(0)
#define LOGE(_str, ...) do{printf(_str , ## __VA_ARGS__); printf("\n");fflush(stdout);} while(0)

#define  LOGI_STREAM(x)  {std::stringstream ss; ss << x; LOGI("%s",ss.str().c_str());}
#define  LOGE_STREAM(x)  {std::stringstream ss; ss << x; LOGE("%s",ss.str().c_str());}

#endif

#endif // __LOGGING_HPP__
