/*
 * Copyright (C) 2010 The Android Open Source Project
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

// Logging
#ifndef XALOG_H_
#define XALOG_H_

// In order of decreasing priority, the log priority levels are:
//    Assert
//    E(rror)
//    W(arn)
//    I(nfo)
//    D(ebug)
//    V(erbose)
// Debug and verbose are usually compiled out except during development.

/** These values match the definitions in system/core/include/cutils/log.h */
#define XALogLevel_Unknown 0
#define XALogLevel_Default 1
#define XALogLevel_Verbose 2
#define XALogLevel_Debug   3
#define XALogLevel_Info    4
#define XALogLevel_Warn    5
#define XALogLevel_Error   6
#define XALogLevel_Fatal   7
#define XALogLevel_Silent  8

// USE_LOG is the minimum log priority level that is enabled at build time.
// It is configured in Android.mk but can be overridden per source file.
#ifndef USE_LOG
#define USE_LOG XALogLevel_Info
#endif

#if (USE_LOG <= XALogLevel_Error)
#define XA_LOGE(...) do { printf("XA_LOGE: %s:%s:%d ", __FILE__, __FUNCTION__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } while(0)
#else
#define XA_LOGE(...) do { } while (0)
#endif

#if (USE_LOG <= XALogLevel_Warn)
#define XA_LOGW(...) do { printf("XA_LOGW: %s:%s:%d ", __FILE__, __FUNCTION__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } while(0)
#else
#define XA_LOGW(...) do { } while (0)
#endif

#if (USE_LOG <= XALogLevel_Info)
#define XA_LOGI(...) do { printf("XA_LOGI: %s:%s:%d ", __FILE__, __FUNCTION__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } while(0)
#else
#define XA_LOGI(...) do { } while (0)
#endif

#if (USE_LOG <= XALogLevel_Debug)
#define XA_LOGD(...) do { printf("XA_LOGD: %s:%s:%d ", __FILE__, __FUNCTION__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } while(0)
#else
#define XA_LOGD(...) do { } while (0)
#endif

#if (USE_LOG <= XALogLevel_Verbose)
#define XA_LOGV(...) do { printf("XA_LOGV: %s:%s:%d ", __FILE__, __FUNCTION__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } while(0)
#else
#define XA_LOGV(...) do { } while (0)
#endif

#endif //XA_LOG_H_
