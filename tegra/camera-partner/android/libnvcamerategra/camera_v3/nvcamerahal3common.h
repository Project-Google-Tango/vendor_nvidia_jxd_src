/*
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * Copyright 2008, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef NV_CAMERA_HAL_COMMON_H
#define NV_CAMERA_HAL_COMMON_H

#include "camera/CameraParameters.h"
#include "CameraMetadata.h"
#include <hardware/camera2.h>
#include <hardware/camera3.h>
#include "system/camera_metadata.h"
#include <hardware/camera_common.h>
#include <hardware/camera.h>
#include <hardware/hardware.h>

#include "nvcameramemprofileconfigurator.h"
#include "nvcamerahallogging.h"
#include <nvmm_buffertype.h>
#include <nvcam_properties_public.h>
#include "nvmm_exif.h"

namespace android {

#define MAX_SENSORS_SUPPORTED 5
#define MAX_STREAMS_SUPPORTED 5
#define MAX_REPROCESS_STREAMS_SUPPORTED 5
#define MAX_STREAM_BUFFERS 100
#define MAX_JPEG_SIZE 10000000
#define USER_COMMENT_LENGTH  192
#define EXIF_STRING_LENGTH  200
#define MAX_DATE_LENGTH  12
#define GPS_PROC_METHOD_LENGTH  32
#define GPS_MAP_DATUM_LENGTH  32
#define ASPECT_TOLERANCE 1.0e-3
#define MAX_TNR_XRES 1920
#define MAX_TNR_YRES 1080

extern NvU64 camGUID[MAX_SENSORS_SUPPORTED];

static const nsecs_t USEC = 1000;
static const nsecs_t MSEC = 1000*USEC;
static const nsecs_t SEC = 1000*MSEC;


// stream assignment to DZ ports (or a copy buffer)
typedef enum {
    CAMERA_STREAM_ZOOM,
    CAMERA_STREAM_NONE
} NvCameraStreamType;


// struct to link NvMM buffers with their corresponding ANB
// they're inherently linked by memcpying the surface ptr, but
// it's cumbersome for readability to try to check that all the time,
// much simpler to just keep the pointers associated by belonging
// to the same array element, so we'll have "preview buffers"
// be arrays of this struct
struct CameraBuffer
{
    camera3_stream_buffer *camera3_buffer;
    buffer_handle_t *anb;
    NvMMBuffer nvmm;
    NvBool isCropped;
    NvBool isAllocated;
    NvRectF32 rect;
    Mutex mLock;
};

const uint32_t kAvailableFormats[] =
{
    HAL_PIXEL_FORMAT_RAW_SENSOR,
    HAL_PIXEL_FORMAT_BLOB,
    HAL_PIXEL_FORMAT_RGBA_8888,
    HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
    // These are handled by YCbCr_420_888
    HAL_PIXEL_FORMAT_YV12,
    HAL_PIXEL_FORMAT_YCrCb_420_SP,
    HAL_PIXEL_FORMAT_YCbCr_420_888
};


typedef Vector<camera3_stream_buffer> HalBufferVector;

typedef enum {
    NVCAMERAHAL_CAMERATYPE_UNDEFINED,
    NVCAMERAHAL_CAMERATYPE_MONO,
    NVCAMERAHAL_CAMERATYPE_STEREO,
    NVCAMERAHAL_CAMERATYPE_USB,
    NVCAMERAHAL_CAMERATYPE_MAX,
    NVCAMERAHAL_CAMERATYPE_FORCE32 = 0x7fffffff
} NvCameraHal_CameraType;

typedef struct _NvCameraHal_CameraInfo
{
    char *deviceLocation;
    NvCameraHal_CameraType camType;
    struct camera_info camInfo;
} NvCameraHal_CameraInfo;

typedef struct JpegControls
{
  double gpsCoordinates[3];
  char gpsProcessingMethod[36];
  NvS64 gpsTimeStamp;
  NvS32 jpegOrientation;
  NvU32 jpegQuality;
  NvU32 thumbnailQuality;
  NvSize thumbnailSize;
  //unsigned int GPSBitMapInfo;

} NvCameraHal3_JpegControls;

// Jpeg settings that HAL needs to save as they are not returned by the core
typedef struct JpegSettings
{
  NvCameraHal3_JpegControls JpegControls;
  // this dynamic prop will be updated just before/after the request to jpeg enc
  NvS32 jpegSize;
  NvSize jpegResolution;
  // Core will return the following with its capture result
  // The following will be set by core in the exifInfo struct passed by the core
  // NvS32 exposureCompensation;
  // SensorSensitivity;
  // SensorExposureTime
  // LensFocalLength
  // exposureCompStep;
  NvMMEXIFInfo exifInfo;
  char makerNoteData[MAKERNOTE_DATA_SIZE];
} NvCameraHal3_JpegSettings;

typedef struct CntrlProps
{
    NvCamProperty_Public_Controls CoreCntrlProps;
    // Control Props not in core - e.g jpeg
    NvCameraHal3_JpegControls JpegControls;
    // maps to android.control.aePrecaptureId
    NvS32 AePrecaptureId;
} NvCameraHal3_Public_Controls;

typedef struct DynamicProps
{
    NvCamProperty_Public_Dynamic CoreDynProps;
    NvMMCameraSensorMode sensorMode;
    // maps to android.control.aePrecaptureId
    NvS32 AePrecaptureId;
    bool send3AResults;
} NvCameraHal3_Public_Dynamic;

typedef struct CameraTime
{
    char date[MAX_DATE_LENGTH];
    int hr;
    int min;
    int sec;
} NvCameraUserTime;

NvCameraHal_CameraType getCamType(int cameraId);

int NvCameraHal_parseCameraConf(NvCameraHal_CameraInfo **sCameraInfo);

int NvCameraHal_getDisplayOrientation();

void NvCameraHal_UpdateUsbExistence(
    NvCameraHal_CameraInfo **sCameraInfo,
    int *numCameras);


inline int32_t getBufferWidth(NvMMBuffer *pBuffer)
{
    NvMMSurfaceDescriptor *pSurfDesc = &(pBuffer->Payload.Surfaces);
    return pSurfDesc->Surfaces[0].Width;
}

inline int32_t getBufferHeight(NvMMBuffer *pBuffer)
{
    NvMMSurfaceDescriptor *pSurfDesc = &(pBuffer->Payload.Surfaces);
    return pSurfDesc->Surfaces[0].Height;
}

} // namespace android

#endif // NV_CAMERA_HAL_COMMON_H

