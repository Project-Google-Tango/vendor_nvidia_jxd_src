/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef _NV_CAMERA_HAL3_TAGS_H_
#define _NV_CAMERA_HAL3_TAGS_H_

/*
 * Set log verbosity. Log lower than NV_LOG_LEVEL will come out to logcat.
 *
 * NV_LOGV = 4
 * NV_LOGD = 3
 * NV_LOGI = 2
 * NV_LOGW = 1
 * NV_LOGE = 0
 */
#define NV_LOG_LEVEL 2

/*
 * Redirect profile log to one of these.
 *
 * Systrace output control
 * Disable = 0
 * Direct to atrace = 1
 * Direct to logcat = 2
 */
#define NV_TRACE_OUTPUT 0

#define HAL3_TAG_ENABLE 1
#define HAL3_API_TAG_ENABLE 1
#define HAL3_CORE_TAG_ENABLE 1
#define HAL3_JPEG_PROCESS_TAG_ENABLE 1
#define HAL3_META_DATA_PROCESS_TAG_ENABLE 1
#define HAL3_ROUTER_TAG_ENABLE 1
#define HAL3_STREAM_PORT_TAG_ENABLE 1
#define HAL3_FORMAT_CONVERTER_TAG_ENABLE 1
#define HAL3_LOGGING_TAG_ENABLE 1
#define HAL3_PARSE_CONFIG_TAG_ENABLE 1
#define HAL3_DEVICE_ORIENTATION_TAG_ENABLE 1
#define HAL3_EVENT_HELPERS_TAG_ENBALE 1
#define HAL3_TNR_TAG_ENABLE 1
#define HAL3_IMAGE_SCALER_TAG_ENABLE 1

//Define all the tags below
#if HAL3_TAG_ENABLE
#define HAL3_TAG "NvCameraHal3"
#else
#define HAL3_TAG 0
#endif

#if HAL3_API_TAG_ENABLE
#define HAL3_API_TAG "NvCameraHal3API"
#else
#define HAL3_API_TAG 0
#endif

#if HAL3_CORE_TAG_ENABLE
#define HAL3_CORE_TAG "NvCameraHal3Core"
#else
#define HAL3_CORE_TAG 0
#endif

#if HAL3_JPEG_PROCESS_TAG_ENABLE
#define HAL3_JPEG_PROCESS_TAG "NvCameraHal3JpegProcess"
#else
#define HAL3_JPEG_PROCESS_TAG 0
#endif

#if HAL3_META_DATA_PROCESS_TAG_ENABLE
#define HAL3_META_DATA_PROCESS_TAG "NvCameraHal3MetaDataProcess"
#else
#define HAL3_META_DATA_PROCESS_TAG 0
#endif

#if HAL3_ROUTER_TAG_ENABLE
#define HAL3_ROUTER_TAG "NvCameraHal3Router"
#else
#define HAL3_ROUTER_TAG 0
#endif

#if HAL3_STREAM_PORT_TAG_ENABLE
#define HAL3_STREAM_PORT_TAG "NvCameraHal3StreamPort"
#else
#define HAL3_STREAM_PORT_TAG 0
#endif

#if HAL3_FORMAT_CONVERTER_TAG_ENABLE
#define HAL3_FORMAT_CONVERTER_TAG "NvFormatConverter"
#else
#define HAL3_FORMAT_CONVERTER_TAG 0
#endif

#if HAL3_LOGGING_TAG_ENABLE
#define HAL3_LOGGING_TAG "NvCameraHalLogging"
#else
#define HAL3_LOGGING_TAG 0
#endif

#if HAL3_PARSE_CONFIG_TAG_ENABLE
#define HAL3_PARSE_CONFIG_TAG "NvCameraParseConfig"
#else
#define HAL3_PARSE_CONFIG_TAG 0
#endif

#if HAL3_DEVICE_ORIENTATION_TAG_ENABLE
#define HAL3_DEVICE_ORIENTATION_TAG "NvDeviceOrientation"
#else
#define HAL3_DEVICE_ORIENTATION_TAG 0
#endif

#if HAL3_EVENT_HELPERS_TAG_ENBALE
#define HAL3_EVENT_HELPERS_TAG "NvCameraHalEventHelpers"
#else
#define HAL3_EVENT_HELPERS_TAG 0
#endif

#if HAL3_TNR_TAG_ENABLE
#define HAL3_TNR_TAG "NvCameraHal3TNR"
#else
#define HAL3_TNR_TAG 0
#endif

#if HAL3_IMAGE_SCALER_TAG_ENABLE
#define HAL3_IMAGE_SCALER_TAG "NvCameraImageScaler"
#else
#define HAL3_IMAGE_SCALER_TAG 0
#endif

#endif
