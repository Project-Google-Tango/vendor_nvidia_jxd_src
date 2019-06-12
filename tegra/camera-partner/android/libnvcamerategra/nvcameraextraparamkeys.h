/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

// this header contains keys for CameraParameters and NvCameraParameters
// objects that the HAL uses for parsing, but are not currently defined
// in the same places as the rest of the keys.  they should eventually be
// put in the right place so that they can be shared with the framework
// properly, and not duplicated here.

#ifndef __NVCAMERA_EXTRA_PARAM_KEYS_H__
#define __NVCAMERA_EXTRA_PARAM_KEYS_H__

// strings we use that aren't defined in CameraParameters.h
// TODO: move these to CameraParameters.h?  maybe NvCameraParameters.h?
#define KEY_ROTATION_VALUES "rotation-values"
#define KEY_VIDEO_STABILIZATION_VALUES "video-stabilization-values"
#define KEY_VIDEO_FRAME_FORMAT_VALUES "video-frame-format-values"
#define KEY_RECORDING_HINT_VALUES "recording-hint-values"
#define KEY_AUTO_WHITEBALANCE_LOCK_VALUES "auto-whitebalance-lock-values"
#define KEY_AUTO_EXPOSURE_LOCK_VALUES "auto-exposure-lock-values"

// strings that used to be defined in NvCameraParameters.h, but
// don't seem to be in the JB tree yet
#define NV_BURST_PICTURE_COUNT_MAX "nv-burst-picture-count-max"
#define NV_EV_BRACKET_CAPTURE "nv-ev-bracket-capture"
#define NV_DATATAP_FMT "nv-datatap-capformat"
#define NV_DATATAP_SIZE "nv-datatap-size"
#define NV_DATATAP "nv-datatap"
#define NV_ANR_MODE "nv-advanced-noise-reduction-mode"
#define NV_EXPOSURE_STATUS "nv-exposure-status"

// strings we use that aren't defined in NvCameraParameters.h
// TODO: move these to NvCameraParameters.h
#define NV_FLIP_PREVIEW "nv-flip-preview"
#define NV_FLIP_PREVIEW_VALUES "nv-flip-preview-values"
#define NV_FLIP_STILL "nv-flip-still"
#define NV_FLIP_STILL_VALUES "nv-flip-still-values"
#define NV_FOCUS_POSITION_VALUES "nv-focus-position-values"
#define NV_CUSTOM_POSTVIEW "nv-custom-postview"
#define NV_CUSTOM_POSTVIEW_VALUES "nv-custom-postview-values"
#define NV_TIMESTAMP_MODE "nv-timestamp-mode"
#define NV_TIMESTAMP_MODE_VALUES "nv-timestamp-mode-values"
#define NV_ZOOM_STEP "nv-zoom-step"
#define NV_CONTINUOUS_ZOOM_STEP_VALUES "nv-continuous-zoom-step-values"
#define NV_ZOOM_SPEED "nv-zoom-speed"
#define NV_NSL_NUM_BUFFERS_MAX "nv-nsl-num-buffers-max"
#define NV_VIDEO_SPEED_VALUES "nv-video-speed-values"
#define NV_VIDEO_HIGHSPEED_RECORDING_VALUES "nv-video-highspeed-recording-values"
#define NV_NSL_SKIP_COUNT_MAX "nv-nsl-burst-skip-count-max"
#define NV_NSL_BURST_PICTURE_COUNT_MAX "nv-nsl-burst-picture-count-max"
#define NV_SKIP_COUNT_MAX "nv-burst-skip-count-max"
#define NV_STILL_HDR "nv-still-hdr"
#define NV_STILL_HDR_VALUES "nv-still-hdr-values"
#define NV_STILL_HDR_ENCODE_SOURCE_FRAMES "nv-still-hdr-enc-source-frames"
#define NV_STILL_HDR_SOURCE_FRAME_COUNT "nv-still-hdr-source-frame-count"
#define NV_STILL_HDR_ENCODE_COUNT "nv-still-hdr-encode-count"
#define NV_STILL_HDR_EXPOSURE_VALUES "nv-still-hdr-exposure-values"
#define NV_RAW_DUMP_FLAG_VALUES "nv-raw-dump-flag-values"
#define NV_EXIF_MAKE "nv-exif-make"
#define NV_EXIF_MODEL "nv-exif-model"
#define NV_GPS_MAP_DATUM "nv-gps-map-datum"
#define NV_USER_COMMENT "nv-user-comment"
#define NV_PICTURE_ISO_VALUES "nv-picture-iso-values"
#define NV_CONTRAST_VALUES "nv-contrast-values"
#define NV_SATURATION_VALUES "nv-saturation-minmax"
#define NV_EDGE_ENHANCEMENT_VALUES "nv-edge-enhancement-values"
#define NV_CAPABILITY_FOR_VIDEO_SIZE_VALUES "nv-capabilities-for-video-size-values"
#define NV_USE_NVBUFFER "usenvbuffer"
#define NV_SENSOR_MODE "nv-sensor-mode"
#define NV_SENSOR_MODE_VALUES "nv-sensor-mode-values"
#define NV_CAMERA_RENDERER "nv-camera-renderer"
#define NV_CAMERA_RENDERER_VALUES "nv-camera-renderer-values"
#define NV_STEREO_MODE_VALUES "nv-stereo-mode-values"
#define NV_LOWLIGHT_THRESHOLD "nv-lowlight-threshold"
#define NV_LOWLIGHT_THRESHOLD_VALUES "nv-lowlight-threshold-values"
#define NV_MACROMODE_THRESHOLD "nv-macromode-threshold"
#define NV_MACROMODE_THRESHOLD_VALUES "nv-macromode-threshold-values"
#define NV_FD_LIMIT "nv-fd-limit"
#define NV_FD_LIMIT_VALUES "nv-fd-limit-values"
#define NV_FD_RESULT "nv-fd-result"
#define NV_FD_DEBUG "nv-fd-debug"
#define NV_FD_DEBUG_VALUES "nv-fd-debug-values"
#define NV_AUTO_ROTATION_VALUES "nv-auto-rotation-values"
#define NV_CAPTURE_MODE "nv-capture-mode"
#define NV_CAPTURE_MODE_VALUES "nv-capture-mode-values"
#define NV_DATATAP_FMT_VALUES "nv-datatap-fmt-values"
#define NV_DATATAP_SIZE_VALUES "nv-datatap-size-values"
#define NV_DATATAP_VALUES "nv-datatap-values"
#define NV_EXPOSURE_TIME_VALUES "nv-exposure-time-values"
#define NV_FOCUS_MOVE_MSG "nv-focus-move-msg"
#define NV_FOCUS_MOVE_MSG_VALUES "nv-focus-move-msg-values"
#define NV_ANR_MODE_VALUES "nv-advanced-noise-reduction-mode-values"
#define NV_DISABLE_PREVIEW_PAUSE "nv-disable-preview-pause"
#define NV_DISABLE_PREVIEW_PAUSE_VALUES "nv-disable-preview-pause-values"
#define NV_PREVIEW_CALLBACK_SIZE_ENABLE_VALUES "nv-preview-callback-size-enable-values"
#define NV_SUPPORTED_PREVIEW_CALLBACK_SIZES "nv-preview-callback-size-values"

// strings that are defined in NvCameraParameters.h but we can't use from that
// header because partner builds will break
#define NV_FOCUS_POSITION "nv-focus-position"
#define NV_NSL_NUM_BUFFERS "nv-nsl-num-buffers"
#define NV_VIDEO_SPEED "nv-video-speed"
#define NV_VIDEO_HIGHSPEED_RECORDING "nv-video-highspeed-recording"
#define NV_NSL_SKIP_COUNT "nv-nsl-burst-skip-count"
#define NV_NSL_BURST_PICTURE_COUNT "nv-nsl-burst-picture-count"
#define NV_SKIP_COUNT "nv-burst-skip-count"
#define NV_BURST_PICTURE_COUNT "nv-burst-picture-count"
#define NV_RAW_DUMP_FLAG "nv-raw-dump-flag"
#define NV_PICTURE_ISO "nv-picture-iso"
#define NV_CONTRAST "nv-contrast"
#define NV_SATURATION "nv-saturation"
#define NV_EDGE_ENHANCEMENT "nv-edge-enhancement"
#define NV_CAPABILITY_FOR_VIDEO_SIZE "nv-capabilities-for-video-size"
#define NV_STEREO_MODE "nv-stereo-mode"
#define NV_AUTO_ROTATION "nv-auto-rotation"
#define NV_EXPOSURE_TIME "nv-exposure-time"
#define NV_COLOR_CORRECTION "nv-color-correction"
#define NV_PREVIEW_CALLBACK_SIZE_ENABLE "nv-preview-callback-size-enable"
#define NV_PREVIEW_CALLBACK_SIZE "nv-preview-callback-size"

#endif // __NVCAMERA_EXTRA_PARAM_KEYS_H__
