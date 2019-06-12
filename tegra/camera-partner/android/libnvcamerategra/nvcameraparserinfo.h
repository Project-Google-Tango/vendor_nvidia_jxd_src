/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NV_CAMERA_PARSER_INFO_H__
#define __NV_CAMERA_PARSER_INFO_H__

// contains most of the Google defined keys
#include "camera/CameraParameters.h"
// contains a compile-time #define that we need
#include <hardware/camera.h>
// contains a few extra keys we use that weren't def'd in the main objects
#include "nvcameraextraparamkeys.h"

namespace android {

// this header defines a table that maps the android (and NV) camera params
// and their capabilities to a set of parsing rules and parsing types.  this
// is information that the parser will use to verify that the parameters we
// receive from the app are valid, and also to map the parameter to the
// corresponding HAL-understandable member of the NvCombinedCameraSettings
// struct

// Help avoid copy-paste errors.
const char FOCUS_MODE_VALUES[] = "auto,infinity,macro,fixed,continuous-video,continuous-picture";
const char FLASH_MODE_VALUES[] = "off,on,auto,torch,red-eye";
const char WHITE_BALANCE_VALUES[] = "auto,incandescent,fluorescent,warm-fluorescent,daylight,cloudy-daylight,shade,twilight";

#define EXP_COMPENSATION_STEP "0.1"
#define EXP_COMPENSATION_MIN  "-20"
#define EXP_COMPENSATION_MAX  "20"

// these pairs should always be updated together.
// this could probably be done "elegantly" with stringizing # operator,
// but that may be less readable and could be argued against.
const int MAX_FOCUS_WINDOWS = 1;
#define MAX_FOCUS_WINDOWS_STR "1"
const int MAX_EXPOSURE_WINDOWS = 4;
#define MAX_EXPOSURE_WINDOWS_STR "4"

// If you change ZOOM_RATIOS and MAX_ZOOM_LEVEL,
// you must also change the definitions for
// ZOOM_VALUE_TO_ZOOM_FACTOR and ZOOM_FACTOR_TO_ZOOM_VALUE
// to match.
const char MAX_ZOOM_LEVEL[] = "28";
const char ZOOM_RATIOS[] =
    "100,125,150,175,200,225,250,275,300,325,350,375,400,425,450,"
    "475,500,525,550,575,600,625,650,675,700,725,750,775,800";

// Access rights for parameters.
#define ECSAccess_NS          0x01 // Not Supported
#define ECSAccess_R           0x02 // Allow read
#define ECSAccess_W           0x04 // Allow write
#define ECSAccess_NonStandard 0x08 // Not google standard
#define ECSAccess_Old         0x10 // Deprecated; trip a warning in the logs

// Common access levels
#define ACCESS_RW            (ECSAccess_R | ECSAccess_W )
#define ACCESS_RW_NONSTD     (ECSAccess_R | ECSAccess_W | ECSAccess_NonStandard)
#define ACCESS_RW_NONSTD_DEP (ECSAccess_R | ECSAccess_W | ECSAccess_NonStandard | ECSAccess_Old)
#define ACCESS_RO            (ECSAccess_R)
#define ACCESS_RO_NONSTD     (ECSAccess_R | ECSAccess_NonStandard)

// Used by the parse table to map to settings.
typedef enum
{ ECSType_PreviewFormat,
  ECSType_PreviewSize,
  ECSType_PreviewRate,
  ECSType_PreviewFpsRange,
  ECSType_PictureFormat,
  ECSType_PictureSize,
  ECSType_PictureRotation,
  ECSType_ThumbnailWidth,
  ECSType_ThumbnailHeight,
  ECSType_ThumbnailSize,
  ECSType_ThumbnailQuality,
  ECSType_ImageQuality,
  ECSType_VideoFrameFormat,
  ECSType_VideoSize,
  ECSType_PreferredPreviewSize,
  ECSType_FlipPreview,
  ECSType_FlipStill,
  ECSType_FlashMode,
  ECSType_SceneMode,
  ECSType_ColorEffect,
  ECSType_ColorCorrection,
  ECSType_WhiteBalanceMode,
  ECSType_ExposureTime,
  ECSType_ExposureCompensation,
  ECSType_MaxExpComp,
  ECSType_MinExpComp,
  ECSType_ExpCompStep,
  ECSType_FocusMode,
  ECSType_FocusPosition,
  ECSType_FocalLength,
  ECSType_HorizontalViewAngle,
  ECSType_VerticalViewAngle,
  ECSType_FocusedAreas,
  ECSType_AreasToFocus,
  ECSType_VideoStabEn,
  ECSType_VideoStabSupported,
  ECSType_CustomPostviewEn,
  ECSType_VideoSpeed,
  ECSType_VideoHighSpeedRecording,
  ECSType_VideoPreviewSizefps,
  ECSType_TimestampEn,
  ECSType_LightFreqMode,
  ECSType_ZoomValue,
  ECSType_ZoomStep,
  ECSType_ZoomSpeed,
  ECSType_NSLNumBuffers,
  ECSType_NSLSkipCount,
  ECSType_NSLBurstCount,
  ECSType_SkipCount,
  ECSType_BurstCount,
  ECSType_RawDumpFlag,
  ECSType_ExifMake,
  ECSType_ExifModel,
  ECSType_UserComment,
  ECSType_GpsLatitude,
  ECSType_GpsLongitude,
  ECSType_GpsAltitude,
  ECSType_GpsTimestamp,
  ECSType_GpsProcMethod,
  ECSType_GpsMapDatum,
  ECSType_PictureIso,
  ECSType_Contrast,
  ECSType_Saturation,
  ECSType_EdgeEnhancement,
  ECSType_SensorId,
  ECSType_UseNvBuffers,
  ECSType_RecordingHint,
  ECSType_SensorMode,
  ECSType_FocusDistances,
  ECSType_AreasToMeter,
  ECSType_BracketCapture,
  ECSType_StillHDR,
  ECSType_CameraRenderer,
  ECSType_AELock,
  ECSType_AELockSupport,
  ECSType_AWBLock,
  ECSType_AWBLockSupport,
  ECSType_CameraStereoMode,
#ifdef FRAMEWORK_HAS_MACRO_MODE_SUPPORT
  ECSType_LowLightThreshold,
  ECSType_MacroModeThreshold,
#endif
  ECSType_FDLimit,
  ECSType_FDDebug,
  ECSType_FDResult,
  ECSType_AutoRotation,
  ECSType_VideoSnapSupported,
  ECSType_CaptureMode,
  ECSType_DataTapFormat,
  ECSType_DataTapSize,
  ECSType_DataTap,
  ECSType_ExposureStatus,
  ECSType_FocusMoveMsg,
  ECSType_ANRMode,
  ECSType_StillHDREncodeSourceFrames,
  ECSType_StillHDRSourceFrameCount,
  ECSType_StillHDREncodeCount,
  ECSType_StillHDRExposureVals,
  ECSType_DisablePreviewPause,
  ECSType_Max,
  ECSType_PreviewCallbackSizeEnable,
  ECSType_PreviewCallbackSize,
  ECSType_NVFDLimit,
  ECSType_Invalid
} ECSType;

// Hints on how to parse and validate strings coming from CameraParameters.
typedef enum
{ ECSInvalid,
  ECSNone,
  ECSSubstring,
  ECSPercent,
  ECSRectangles,
  ECSNumberMin,
  ECSNumberMax,
  ECSNumberMinMax,
  ECSMatrix4x4,
  ECSNumberRange,
  ECSString,
  ECSFloat,
  ECSResolution,
  ECSResolutionDimension,
  ECSIso,
} ECSParseType;

typedef struct
{
    const char *key;
    const char *initialDefault;
    const char *capsKey;
    const char *initialCapability;
    ECSType type;
    int access;
    ECSParseType parseType;
} ParserInfoTableEntry;

} // namespace android

#endif // __NV_CAMERA_PARSER_INFO_H__
