/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NV_CAMERA_HAL_UTILS_H
#define NV_CAMERA_HAL_UTILS_H

#include "nvrm_init.h"
#include "nvgr.h"

namespace android {
NvRmDeviceHandle NvxGetRmDevice(void);
int GrModuleLock(native_handle_t* handle, int usage, int width, int height);
void GrModuleUnlock(native_handle_t* handle);
void GrModuleFenceWait(native_handle_t* handle);
void fillPatternInBuf(buffer_handle_t * pBuffer);
void fillEncodedPattern(buffer_handle_t * pBuffer);
} // namespace android

#endif // NV_CAMERA_HAL_UTILS_H
