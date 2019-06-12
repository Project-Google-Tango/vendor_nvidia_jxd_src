/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef _NV_ODM_TAGS_H_
#define _NV_ODM_TAGS_H_

#define NV_LOG_LEVEL 0
// Systrace output control
// Disable = 0
// Direct to atrace = 1
// Direct to logcat = 2
#define NV_TRACE_OUTPUT 0

//Enable/disable logs
#define NV_ODM_SENSOR_TAG_ENABLE 0
#define NV_ODM_FOCUSER_TAG_ENABLE 0

//Define all the tags below
#if NV_ODM_SENSOR_TAG_ENABLE
#define NV_ODM_SENSOR_TAG "ODM_SENSOR"
#else
#define NV_ODM_SENSOR_TAG 0
#endif

#if NV_ODM_FOCUSER_TAG_ENABLE
#define NV_ODM_FOCUSER_TAG "ODM_FOCUSER"
#else
#define NV_ODM_FOCUSER_TAG 0
#endif

#endif
