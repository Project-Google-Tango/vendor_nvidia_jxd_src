/*
 * Copyright (c) 2011-2013 NVIDIA Corporation. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef RUNTIME_LOGS_H
#define RUNTIME_LOGS_H
//----------------------------------------------------------------------------
// Logging support -- this is for debugging only
// Use "adb shell dumpsys media.camera -v 0" to change the current
// logging behaviour, while camera is running.
//
// Here "0" is just a placeholder, everytime user executes this
// command with value "0",logging behaviour gets toggled.
//
// Use "adb shell setprop enableLogs 0/1/2/3/4" to change the current
// logging behaviour, before camera starts, freshly, for camera init logs.
// A lower level number we set, less logs will be displayed.
// level 0 - Only shows error messages.
// level 1 - Besides the logs of level 0, warning messages are displayed
// level 2 - Besides the logs of level 1, information messages are displayed.
//           This level is reserved for KPI usage.
// level 3 - Besides the logs of level 2, debugging messages are displayed.
// level 4 - Besides the logs of level 3, verbose messages are displayed.
//
// This command does not have any effect while camera service is running, but it
// changes the logging behaviour for next run.
//
// By default logs are disabled.
extern int32_t glogLevel;

#define LOG1(...) ALOGW_IF(glogLevel >=1, __VA_ARGS__);
#define LOG2(...) ALOGI_IF(glogLevel >=2, __VA_ARGS__);
#define LOG3(...) ALOGD_IF(glogLevel >=3, __VA_ARGS__);
#define LOG4(...) ALOGV_IF(glogLevel >=4, __VA_ARGS__);

#define ALOGWW LOG1
#define ALOGII LOG2
#define ALOGDD LOG3
#define ALOGVV LOG4

#ifdef ALOGW
#undef ALOGW
#define ALOGW ALOGWW
#endif

#ifdef ALOGI
#undef ALOGI
#define ALOGI ALOGII
#endif

#ifdef ALOGD
#undef ALOGD
#define ALOGD ALOGDD
#endif

#ifdef ALOGV
#undef ALOGV
#define ALOGV ALOGVV
#endif

#define ALOGEE ALOGE
#endif

//-----------------------------------------------------------------------------

