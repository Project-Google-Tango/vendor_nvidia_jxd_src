/*
 * Copyright (c) 2009-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * @file
 * <b>NVIDIA Tegra: OpenMAX Camera Extension Interface</b>
 *
 */

/**
 * @defgroup nv_omx_il_camera Camera
 *
 * This is the NVIDIA OpenMAX camera class extension interface.
 *
 * These extensions include auto-focus, auto-exposure, auto-whitebalance, half-press,
 * focus regions, sharpness, hue, framerate, encoder control, edge enhancement and more.
 *
 * @ingroup nvomx_camera_extension
 * @{
 */

#ifndef NVOMX_CameraExtensions_h_
#define NVOMX_CameraExtensions_h_

#include "NVOMX_ColorFormatExtensions.h"

typedef float NVX_F32;

/** Holds a floating point rectangle */
typedef struct NVX_RectF32
{
    NVX_F32 left;
    NVX_F32 top;
    NVX_F32 right;
    NVX_F32 bottom;
} NVX_RectF32;

#define NVX_MAX_FOCUS_REGIONS  8
#define NVX_MAX_EXPOSURE_REGIONS 8
#define NVX_MAX_DIRECT_FOCUSER_CONTROL_LENGTH  16
#define NVX_VIDEOENC_DCI_SIZE 80
#define MAX_NUM_SENSOR_MODES  30
#define NVX_MAX_CAMERACONFIGURATION_LENGTH 64
#define NVX_MAX_FD_OUTPUT_LENGTH 1024
#define NVX_MAX_FUSE_ID_SIZE 16
#define NVX_MAX_EXPOSURE_COUNT 2

typedef enum  NVX_WHITEBALCONTROLTYPE {
   NVX_WhiteBalControlVendorStartUnused  = OMX_WhiteBalControlVendorStartUnused,
   NVX_WhiteBalControlWarmFluorescent,
   NVX_WhiteBalControlTwilight,
   NVX_WhiteBalControlMax = 0x7FFFFFFF
}  NVX_WHITEBALCONTROLTYPE;

typedef enum NVX_VIDEO_ERROR_RESILIENCY_LEVEL_TYPE {
    NVX_VIDEO_ErrorResiliency_None = 0,
    NVX_VIDEO_ErrorResiliency_Low,
    NVX_VIDEO_ErrorResiliency_High,
    NVX_VIDEO_ErrorResiliency_Invalid = 0x7FFFFFFF
} NVX_VIDEO_ERROR_RESILIENCY_LEVEL_TYPE;

typedef enum NVX_VIDEO_APPLICATION_TYPE {
    NVX_VIDEO_Application_Camcorder = 0,    /**< Timestamps set for camcorder */
    NVX_VIDEO_Application_VideoTelephony,   /**< Timestamps set for telephony */
    NVX_VIDEO_Application_Invalid = 0x7FFFFFFF
} NVX_VIDEO_APPLICATION_TYPE;

typedef enum NVX_VIDEO_RATECONTROL_MODE{
    NVX_VIDEO_RateControlMode_CBR = 0,
    NVX_VIDEO_RateControlMode_VBR,
    NVX_VIDEO_RateControlMode_VBR2,
    NVX_VIDEO_RateControlMode_Invalid = 0x7FFFFFFF
}NVX_VIDEO_RATECONTROL_MODE;

/** Param extension index to fine tune video encoder configuration.
 *  See ::NVX_PARAM_VIDENCPROPERTY
 */
#define NVX_INDEX_PARAM_VIDEO_ENCODE_PROPERTY "OMX.Nvidia.index.param.video.encode.prop"
/** Holds data to fine tune video encoder configuration. */
typedef struct NVX_PARAM_VIDENCPROPERTY
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */

    NVX_VIDEO_APPLICATION_TYPE eApplicationType;                    /**< Application Type */
    NVX_VIDEO_ERROR_RESILIENCY_LEVEL_TYPE eErrorResiliencyLevel;    /**< Error Resiliency Level */
    OMX_BOOL bSvcEncodeEnable;                                      /**< Boolean to enable H.264 Scalable Video Codec mode */
    OMX_BOOL bSetMaxEncClock;                                       /**< Set Maximum clock for encoder hardware */
    OMX_BOOL bFrameSkip;                                            /**< Enable skipping of Frames */
    OMX_BOOL bAllIFrames;                                           /**< Encode all frames as I Frame */
    OMX_BOOL bBitBasedPacketization;                                /**< Packet size is based upon Number Of bits */
    OMX_BOOL bInsertSPSPPSAtIDR;                                    /**< Insert SPS/PPS at IDR */
    OMX_BOOL bUseConstrainedBP;                                     /**< Use Constrained BP */
    OMX_BOOL bInsertVUI;                                            /**< Insert VUI in the bitstream */
    OMX_BOOL bInsertAUD;                                            /**< Insert AUD in the bitstream */
    OMX_U32  nPeakBitrate;                                           /**< Peak Bitrate for VBR, if set to 0, encoder derive it from Level Idc */
    OMX_BOOL bEnableStuffing;                                       /**< Enable byte stuffing to maintain bitrate */
    OMX_BOOL bLowLatency;                                           /**< Reduce latency by lowering peak frame size (for both I and P frames) */
    OMX_BOOL bSliceLevelEncode;                                     /**< Reduce latency by delivering packet size based slices (for both I and P frames) */
    OMX_BOOL bSliceIntraRefreshEnable;                              /**< Enable Slice Intra Refresh Wave */
    OMX_U32  SliceIntraRefreshInterval;                             /**< Slice Intra Refresh Interval in number of frames */
    OMX_U32  nVirtualBufferSize;                                    /**< Virtual Buffer Size specified by the app in bits */
} NVX_PARAM_VIDENCPROPERTY;
/** Param extension index to set video encoder rate control mode.
 *  See ::NVX_PARAM_RATECONTROLMODE
 */
 #define NVX_INDEX_PARAM_RATECONTROLMODE "OMX.Nvidia.index.param.ratecontrolmode"
/** Holds data to set video encoder rate control mode.
 * See ::NVX_IndexParamRateControlMode
 */
typedef struct NVX_PARAM_RATECONTROLMODE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */

    NVX_VIDEO_RATECONTROL_MODE eRateCtrlMode;
}NVX_PARAM_RATECONTROLMODE;

/** Holds boolean parameter.
 */
typedef struct NVX_PARAM_BOOLEAN
{
    OMX_U32 nSize;                              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;                   /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;                         /**< Port that this struct applies to */
    OMX_BOOL enable;                            /**< on/off */
} NVX_PARAM_BOOLEAN;

/** Param extension index to configure the video encode quantization range.
 *  See ::NVX_CONFIG_VIDENC_QUANTIZATION_RANGE
 */
#define NVX_INDEX_CONFIG_VIDEO_ENCODE_QUANTIZATION_RANGE "OMX.Nvidia.index.config.video.encode.quantizationrange"
/**
 * Holds information for configuring video compression quantization parameter limits
 * parameter values. Codecs may support different Min/Max QP values for different
 * frame types. Default value used by all MinQp/MaxQp should be set to some invalid
 * value. So that it will not affect encoder rate control parameter. If some control
 * is required for MinQp or MaxQp or Both, then set valid value for that param.
 */
typedef struct NVX_CONFIG_VIDENC_QUANTIZATION_RANGE {
    OMX_U32 nSize;                /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;     /**< OMX specification version info */
    OMX_U32 nPortIndex;           /**< Port that this structure applies to */
    OMX_U32 nMinQpI;              /**< Minimum QP value to use for index frames */
    OMX_U32 nMaxQpI;              /**< Maximum QP value to use for index frames */
    OMX_U32 nMinQpP;              /**< Minimum QP value to use for P frames */
    OMX_U32 nMaxQpP;              /**< Maximum QP value to use for P frames */
    OMX_U32 nMinQpB;              /**< Minimum QP value to use for B frames */
    OMX_U32 nMaxQpB;              /**< Maximum QP value to use for B frames */
} NVX_CONFIG_VIDENC_QUANTIZATION_RANGE;

/** Param extension index to get the quantization index used for last frame encoding
 *  See ::NVX_INDEX_CONFIG_VIDEO_ENCODE_LAST_FRAME_QP
 **/
#define NVX_INDEX_CONFIG_VIDEO_ENCODE_LAST_FRAME_QP "OMX.Nvidia.index.config.video.encode.lastframeqp"
/** Holds Quantization Index used in last frame encoding.
 */
typedef struct NVX_PARAM_LASTFRAMEQP
{
    OMX_U32 nSize;                              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;                   /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;                         /**< Port that this struct applies to */
    OMX_U32  LastFrameQP;                            /**< on/off */
} NVX_PARAM_LASTFRAMEQP;

/** Param extension index to configure the h264 encoder quality parameters.
 *  See ::NVX_CONFIG_VIDENC_H264_QUALITY_PARAMS
 */
#define NVX_INDEX_PARAM_VIDENC_H264_QUALITY_PARAMS "OMX.Nvidia.index.param.video.encode.h264qualityparams"
/**
 * Holds information for configuring h264 quality control parameters
 * \a nFavorInterBias is used for giving bias towards Inter macroblocks for Intra/Inter mode decision
 * \a nFavorIntraBias is used for giving bias towards Intra macroblocks for Intra/Inter mode decision.
 * \a nFavorIntraBias_16X16 is used for giving bias towards Intra16x16 macroblocks for I16x16/I4x4 mode decision.
 */
typedef struct NVX_PARAM_VIDENC_H264_QUALITY_PARAMS {
    OMX_U32 nSize;                /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;     /**< OMX specification version info */
    OMX_U32 nPortIndex;           /**< Port that this structure applies to */
    OMX_U32 nFavorInterBias;      /**< To set bias towards Inter Macroblocks for I/P mode decision */
    OMX_U32 nFavorIntraBias;      /**< To set bias towards Intra macroblocks for I/P mode decision */
    OMX_U32 nFavorIntraBias_16X16;              /**< To set bias toward Intra16x16 macroblocks for Intra16x16/Intra4x4 mode decision */
} NVX_PARAM_VIDENC_H264_QUALITY_PARAMS;


/** Param extension index to enable/disable stringent bitrate for encoder.
 *  See ::OMX_CONFIG_BOOLEANTYPE
 */
#define NVX_INDEX_PARAM_VIDEO_ENCODE_STRINGENTBITRATE "OMX.Nvidia.index.param.video.encode.stringentbitrate"

/**
 * Config extension index to set/get stereo metadata.
 */
#define NVX_INDEX_CONFIG_VIDEO_STEREOINFO "OMX.Nvidia.index.config.videostereoinfo"

#define NVX_INDEX_CONFIG_VIDEO_MVCINFO "OMX.Nvidia.index.config.videomvcinfo"

// TODO: Need to finalize on the exact defines to expose to the app
// Frame Packing Format
typedef enum _NVX_VIDEO_FRAMEPACK_TYPE {
    NVX_VIDEO_FRAMEPACK_Type_Checker  = 0, // Checkerboard
    NVX_VIDEO_FRAMEPACK_Type_ColInt   = 1, // Column Interleaved
    NVX_VIDEO_FRAMEPACK_Type_RowInt   = 2, // Row Interleaved
    NVX_VIDEO_FRAMEPACK_Type_SbS      = 3, // Side by Side
    NVX_VIDEO_FRAMEPACK_Type_TB       = 4, // Top Bottom
    NVX_VIDEO_FRAMEPACK_Type_FrameInt = 5, // Temporal Frame Interleaved
    NVX_VIDEO_FRAMEPACK_TypeInvalid   = 0x7FFFFFFF
} NVX_VIDEO_FRAMEPACK_TYPE;

// Content Interpretation Type
typedef enum _NVX_VIDEO_CONTENT_TYPE {
    NVX_VIDEO_CONTENT_Type_Undef    = 0, // Undefined
    NVX_VIDEO_CONTENT_Type_LR       = 1, // Left first
    NVX_VIDEO_CONTENT_Type_RL       = 2, // Right first
    NVX_VIDEO_CONTENT_TypeInvalid   = 0x7FFFFFFF
} NVX_VIDEO_CONTENT_TYPE;

// Stereo Metadata structure
typedef struct NVX_CONFIG_VIDEO_STEREOINFO
{
    OMX_U32 nSize;                    /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;         /**< NVX extensions specification version */
    OMX_U32 nPortIndex;               /**< Port that this struct applies to */

    OMX_U32 stereoEnable;             /**< Stereo Enable / Disable for this buffer */
    NVX_VIDEO_FRAMEPACK_TYPE fpType;  /**< Stereo Frame-packing type */
    NVX_VIDEO_CONTENT_TYPE contentType; /**< Stereo Content Type */
} NVX_CONFIG_VIDEO_STEREOINFO;

// Stitch MVC views
typedef struct NVX_CONFIG_VIDEO_MVC_INFO
{
    OMX_U32 nSize;                    /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;         /**< NVX extensions specification version */
    OMX_U32 nPortIndex;               /**< Port that this struct applies to */

    OMX_BOOL stitch_MVCViews_Flag;     /**< Specifies if the 2 MVC views should be  stitched*/
} NVX_CONFIG_VIDEO_MVC_INFO;

/** Param extension index to configure slice level encode.
*   see: NVX_CONFIG_VIDEO_SLICELEVELENCODE
*/
#define NVX_INDEX_CONFIG_VIDEO_SLICELEVELENCODE "OMX.Nvidia.index.config.video.slicelevelencode"
/**   Holds flag to enable/disable slice level encode
*/
typedef struct NVX_CONFIG_VIDEO_SLICELEVELENCODE {
    OMX_U32 nSize; /** Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion; /** NVX extensions specification versions information */
    OMX_U32 nPortIndex; /** Port that this struct applies to */
    OMX_BOOL SliceLevelEncode; /** Slice level encode turn on/off */
} NVX_CONFIG_VIDEO_SLICELEVELENCODE;

/** Param extension index to enable the video protected mode.
 *  See ::OMX_CONFIG_BOOLEANTYPE
 */
#define NVX_INDEX_CONFIG_VIDEO_PROTECT "OMX.Nvidia.index.config.video.protect"

/** Config extension index to enable camera test pattern.
 *  See ::NVX_CONFIG_CAMERATESTPATTERN
 */
#define NVX_INDEX_CONFIG_CAMERATESTPATTERN "OMX.Nvidia.index.config.cameratestpattern"
/** Holds data to enable camera test pattern. */
typedef struct NVX_CONFIG_CAMERATESTPATTERN
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_U32 TestPatternIndex;       /**< Boolean to enable test pattern */
}NVX_CONFIG_CAMERATESTPATTERN;

/** Config extension index to set duration in milliseconds for smooth zooming.
 *  See ::OMX_PARAM_U32TYPE
 */
#define NVX_INDEX_CONFIG_SMOOTHZOOMTIME "OMX.Nvidia.index.config.smoothzoomtime"

/** Config extension index to trigger sensor power up.
 *  See ::OMX_CONFIG_BOOLEANTYPE
 */
#define NVX_INDEX_CONFIG_SENSORPOWERON "OMX.Nvidia.index.config.sensorpoweron"

/** Config extension index to set THS-Settle time for camera connected via MIPI CSI.
 *  See ::OMX_PARAM_U32TYPE
 */
#define NVX_INDEX_CONFIG_CILTHRESHOLD "OMX.Nvidia.index.config.cilthreshold"

/** Config extension index to abort zooming.
 *  See ::OMX_CONFIG_BOOLEANTYPE
 */
#define NVX_INDEX_CONFIG_ZOOMABORT "OMX.Nvidia.index.config.zoomabort"

/** Config extension index to set scale factor multiplier in digital zoom.
 *  See ::NVX_CONFIG_DZSCALEFACTORMULTIPLIER
 */
#define NVX_INDEX_CONFIG_DZSCALEFACTORMULTIPLIER "OMX.Nvidia.index.config.dzscalefactormultiplier"
/** Holds data to set scale factor multiplier in digital zoom. */
typedef struct NVX_CONFIG_DZSCALEFACTORMULTIPLIER
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_S32 ZoomFactorMultiplier;   /**< Scale factor from 100 to 800 (1.0-8.0) up to 8x digital */
}NVX_CONFIG_DZSCALEFACTORMULTIPLIER;

/** Config extension index to enable/disable camera preview.
 *  See ::OMX_CONFIG_BOOLEANTYPE
 */
#define NVX_INDEX_CONFIG_PREVIEWENABLE "OMX.Nvidia.index.config.previewenable"

/** Config extension index to enable/disable force camera postview output.
 *  See ::OMX_CONFIG_BOOLEANTYPE
 */
#define NVX_INDEX_CONFIG_FORCEPOSTVIEW "OMX.Nvidia.index.config.forcepostview"

/** Config extension index to start camera preview without outputing preview
 *  pictures. This allows AE/AWB converge and stay converged as soon as the
 *  preview window appears.
 *  See ::OMX_CONFIG_BOOLEANTYPE
 *
 *  AlgorithmWarmup is a command (not a state change) to start up nvmm
 *  camera. When nvmm camera receives this command, it starts preview
 *  capture and does AE/AWB converging. And it does not output either
 *  buffers or EOS to DZ. Then it puts the nvmm camera into a preview
 *  paused state.
 *
 *  AlgorithmWarmup is independent of PreviewEnable. 1. After Preview is
 *  enabled, there should be no need for AlgorithmWarmup.  2. PreviewEnable
 *  may be sent without preceeded by AlgorithmWarmup. The nvmm camera warms up
 *  AE/AWB by itself.
 *
 */
#define NVX_INDEX_CONFIG_ALGORITHMWARMUP "OMX.Nvidia.index.config.algorithmwarmup"

/** Config extension index to enable/disable capture pause.
 *  See ::OMX_CONFIG_BOOLEANTYPE
 */
#define NVX_INDEX_CONFIG_CAPTUREPAUSE "OMX.Nvidia.index.config.capturepause"


/** Config extension index to setup converge and lock (half-press).
 * When this extension is called, the camera component will start
 * converging (achieving) auto focus, auto exposure, and/or auto
 * white balance. If the camera component achieves one or
 * more of these properties then it will lock settings of those
 * properties. If the camera component was unable to achieve one or
 * more of those properties in a certain time then it times out
 * for that property.
 *
 * The camera component notifies the application about
 * those properties that were achieved and those that timed out
 * using events. The following lists shows the different
 * events that are sent.
 * - Auto focus achieved (See ::NVX_EventCamera_AutoFocusAchieved)
 * - Auto focus timed out (See ::NVX_EventCamera_AutoFocusTimedOut)
 * - Auto exposure achieved (See ::NVX_EventCamera_AutoExposureAchieved)
 * - Auto exposure timed out (See ::NVX_EventCamera_AutoExposureTimedOut)
 * - Auto white balance achieved (See
 *      ::NVX_EventCamera_AutoWhiteBalanceAchieved)
 * - Auto white balance timed out (See
 *      ::NVX_EventCamera_AutoWhiteBalanceTimedOut)
 *
  * Although converge and lock is used with auto focus, auto exposure, and
 * auto white balance properties, an application can choose to
 * enable any one or any combination of these properties. When
 * any of these properties is disabled then half press will not
 * attempt to achieve that property.
 *
 *  See ::NVX_CONFIG_CONVERGEANDLOCK
 */
#define NVX_INDEX_CONFIG_CONVERGEANDLOCK "OMX.Nvidia.index.config.convergeandlock"


/** Algorithm subtypes
 *  Has subtype of the algorithm that is used.
 */
#define NvxAlgSubType_None            0         /**<  Algorithm does nothave subtype */
#define NvxAlgSubType_AFFullRange     (1 << 0)   /**<  AF that uses entire range */
#define NvxAlgSubType_AFInfMode       (1 << 1)   /**<  AF only in the Infinity range */
#define NvxAlgSubType_AFMacroMode     (1 << 2)   /**<  AF only in the Macro range */
#define NvxAlgSubType_TorchDisable    (1 << 3)   /**<  Disable torch */

/** Holds data to setup converge and lock. */
typedef struct NVX_CONFIG_CONVERGEANDLOCK
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_BOOL bUnlock;               /**< Boolean to unlock AF, AE, AWB settings */
    OMX_BOOL bAutoFocus;            /**< Boolean to enable auto focus */
    OMX_BOOL bAutoExposure;         /**< Boolean to enable auto exposure */
    OMX_BOOL bAutoWhiteBalance;     /**< Boolean to enable auto white balance */
    OMX_U32  nTimeOutMS;            /**< Timeout in milliseconds */
    OMX_BOOL bRelock;               /**< Boolean hint to restore previous alg settings during lock */
    OMX_U32 algSubType;
} NVX_CONFIG_CONVERGEANDLOCK;

/** Config extension index to setup pre-capture converge.
 *  @deprecated This index is deprecated.
 */
#define NVX_INDEX_CONFIG_PRECAPTURECONVERGE "OMX.Nvidia.index.config.precaptureconverge"
/** Holds data to setup pre-capture converge. */
typedef struct NVX_CONFIG_PRECAPTURECONVERGE
{
    OMX_U32 nSize;                      /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;           /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;                 /**< Port that this struct applies to */
    OMX_BOOL bPrecaptureConverge;       /**< Boolean to enable pre-capture converge */
    OMX_BOOL bContinueDuringCapture;    /**< Boolean to enable continous converge during capture */
    OMX_U32  nTimeOutMS;                /**< Timeout in milliseconds */
} NVX_CONFIG_PRECAPTURECONVERGE;

#define NVX_INDEX_CONFIG_AEOVERRIDE "OMX.Nvidia.index.config.aeoverride"
/** Holds data to specify exposure time ratio for camera AOHDR. */
typedef struct NVX_CONFIG_AEOVERRIDE
{
    OMX_U32  nSize;                        /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;              /**< NVX extensions specification version information */
    OMX_U32  nPortIndex;                   /**< Port that this struct applies to */
    OMX_BOOL analog_gains_en;              /**< Indicates we are setting analog bayer gains */
    NVX_F32  analog_gains[4];              /**< Specifies per-channel analog gains
                                                [0]: gain for R, [1]: gain for GR,
                                                [2]: gain for GB, [3]: gain for B. */
    OMX_BOOL digital_gains_en;             /**< Indicates we are setting digital bayer gains */
    NVX_F32  digital_gains[4];             /**< Specifies per-channel digital gains
                                                [0]: gain for R, [1]: gain for GR,
                                                [2]: gain for GB, [3]: gain for B. */
    OMX_BOOL exposure_en;                  /**< Indicates we are setting exposure time(s) */
    OMX_U32  no_of_exposures;              /**< Number of exposures passed > */
    NVX_F32  exposures[NVX_MAX_EXPOSURE_COUNT]; /**< Exposure Times */
} NVX_CONFIG_AEOVERRIDE;

/** Config extension index to setup min and max frame rate for camera.
 *  See ::NVX_CONFIG_AUTOFRAMERATE
 */
#define NVX_INDEX_CONFIG_AUTOFRAMERATE "OMX.Nvidia.index.config.autoframerate"
/** Holds data to setup auto frame rate for camera. */
typedef struct NVX_CONFIG_AUTOFRAMERATE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_BOOL bEnabled;              /**< Boolean to enable auto frame rate */
    OMX_S32  low;                   /**< Lowest frame rate allowed */
    OMX_S32  high;                  /**< Highest frame rate allowed */
} NVX_CONFIG_AUTOFRAMERATE;

/** Config extension index to setup burst skip count.
 */
#define NVX_INDEX_CONFIG_BURSTSKIPCOUNT "OMX.Nvidia.index.config.burstskipcount"

/** Config extension index to control exposure regions.
 * The rectangle coordinates are normalized to -1.0 to 1.0 on the
 * X and Y axis, and given in fixed-point representation.
 * The weights should be positive floating-point values.
 * Auto exposure region will work only when auto exposure is enabled.
 * Setting nRegions to 0 will clear out any existing exposure regions
 * and restore the driver's default exposure algorithm.
 *
 *  See ::NVX_CONFIG_ExposureRegionsRect
 */
#define NVX_INDEX_CONFIG_EXPOSUREREGIONSRECT "OMX.Nvidia.index.config.exposureregionsrect"
/** Holds data to control exposure regions. */

typedef struct NVX_CONFIG_ExposureRegionsRect
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32      nRegions;                          /**< Number of regions */
    NVX_RectF32  regions[NVX_MAX_EXPOSURE_REGIONS];  /**< Array of NVX_RectF32 to specify each exposure region */
    NVX_F32      weights[NVX_MAX_EXPOSURE_REGIONS];  /**< Array of the relative weightings to apply to each exposure region */
} NVX_CONFIG_ExposureRegionsRect;

/** Config extension index to specify exposure time range for camera.
 *
 * Sets the minimum and maximum exposure time or ISO sensitivty from which the
 * capture component's auto exposure picks values.
 *
 * The camera component supports auto exposure, where it
 * dynamically changes exposure time and sensor's sensitivty between a
 * minimum value and a maximum value specified
 * by config property data structure.
 * Minimum and maximum possible exposure times and ISO gains are dependent
 * on the camera sensor. Values above the maximum and below the minimum
 * supported by the camera sensor are clipped.
 *
 * Values are in units of milliseconds. To limit the exposure to nothing
 * slower than 1/30 second, a setting like {0, 33} would be used. 0 would
 * be replaced with the sensors fastest exposure limit by the driver.
 *
 * To limit the exposure to nothing faster than 1/30 second, set the
 * range to {33, 0}. The 0 for "high" would be replaced with the
 * sensor's slowest exposer limit.
 *
 *  See ::NVX_CONFIG_EXPOSURETIME_RANGE
 */
#define NVX_INDEX_CONFIG_EXPOSURETIMERANGE "OMX.Nvidia.index.config.exposuretimerange"
/** Holds data to specify exposure time range for camera. */
typedef struct NVX_CONFIG_EXPOSURETIME_RANGE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_U32  low;                   /**< Shortest exposure time allowed */
    OMX_U32  high;                  /**< Longest exposure time allowed */
} NVX_CONFIG_EXPOSURETIME_RANGE;

#define NVX_INDEX_CONFIG_SENSORETRANGE "OMX.Nvidia.index.config.sensoretrange"
typedef struct NVX_CONFIG_SENSOR_ET_RANGE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    NVX_F32  low;                   /**< Shortest exposure time allowed */
    NVX_F32  high;                  /**< Longest exposure time allowed */
} NVX_CONFIG_SENSOR_ET_RANGE;

/** Config extension index to set/get exposure time in float seconds. */
#define NVX_INDEX_CONFIG_EXPOSURETIME "OMX.Nvidia.index.config.exposuretime"
typedef struct NVX_CONFIG_EXPOSURETIME
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    NVX_F32 nExposureTime;          /**< Exposure time in seconds */
    OMX_BOOL bAutoShutterSpeed;     /**< Whether shutter speed is defined automatically */
} NVX_CONFIG_EXPOSURETIME;

#define NVX_INDEX_CONFIG_FUSEID "OMX.Nvidia.index.config.fuseid"
typedef struct NVX_CONFIG_SENSOR_FUSE_ID
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_U32 fidSize;                /**< Number of fuse ID bytes populated */
    OMX_U8 fidBytes[NVX_MAX_FUSE_ID_SIZE];  /**< Fuse ID bytes */
} NVX_CONFIG_SENSOR_FUSE_ID;

/** Config extension index to set/get focus postion. */
#define NVX_INDEX_CONFIG_FOCUSPOSITION "OMX.Nvidia.index.config.focusposition"

/** Config extension index to set/get color correction matrix. */
#define NVX_INDEX_CONFIG_COLORCORRECTIONMATRIX "OMX.Nvidia.index.config.colorcorrectionmatrix"
typedef struct NVX_CONFIG_COLORCORRECTIONMATRIX
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    float ccMatrix[16];             /**< Color Correction Matrix */
} NVX_CONFIG_COLORCORRECTIONMATRIX;

/** Config extension index to specify ISO sensitivity range for camera.
 *
 * Sets the minimum and maximum exposure time or ISO sensitivty from which the
 * capture component's auto exposure picks values.
 *
 * The camera component supports auto exposure, where it
 * dynamically changes exposure time and sensor's sensitivty between a
 * minimum value and a maximum value specified
 * by config property data structure.
 * Minimum and maximum possible exposure times and ISO gains are dependent
 * on the camera sensor. Values above the maximum and below the minimum
 * supported by the camera sensor are clipped.
 *
 * Values are in units of ISO. To limit the ISO gain to nothing more
 * than ISO400, a setting like {0, 400} would be used. 0 would be
 * replaced with the sensor's lowest supported gain limit by the
 * driver. To limit the ISO gain to nothing less than ISO400, a
 * setting like {400, 0} would be used. 0 would be replaced with the
 * sensor's highest supported gain limit by the driver. To reset to
 * initialization value, set to {0, 0}.
 *
 * Since the Auto exposure algorithm is 'exposure time priority', limiting
 * the ISO range will only dim the image if the ISO is limited by setting
 * the range. The exposure time is not recalculated based off of the
 * ISO range limit.
 *
 *  See ::NVX_CONFIG_ISOSENSITIVITY_RANGE
 */
#define NVX_INDEX_CONFIG_ISOSENSITIVITYRANGE "OMX.Nvidia.index.config.isosensitivityrange"
/** Holds data to specify ISO sensitivity range for camera. */
typedef struct NVX_CONFIG_ISOSENSITIVITY_RANGE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_U32  low;                   /**< Lowest ISO value allowed */
    OMX_U32  high;                  /**< Highest ISO value allowed */
} NVX_CONFIG_ISOSENSITIVITY_RANGE;

/** Config extension index to specify white balance correlated color
 * temperature (CCT) range for camera.
 *
 * Auto white balance will normally search a large set of illuminants to
 * estimate the CCT of a scene. This range can be limited to a subset
 * of the defined illuminants by setting this parameter.
 * The config property's low and high values in the range
 * specify the minimum and maximum CCT values to be searched. Legal values
 * are integers in units of degrees K. To select a specific illuminant,
 * the low and high members should be set to identical values. By default,
 * the low range value is 0, and the high value is MAX_INT.
 *
 *  See ::NVX_CONFIG_CCTWHITEBALANCE_RANGE
 */
#define NVX_INDEX_CONFIG_CCTWHITEBALANCERANGE "OMX.Nvidia.index.config.cctwhitebalancerange"
/** Holds data to specify whitebalance CCT range for camera. */
typedef struct NVX_CONFIG_CCTWHITEBALANCE_RANGE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_U32  low;                   /**< Low CCT range value */
    OMX_U32  high;                  /**< High CCT range value */
} NVX_CONFIG_CCTWHITEBALANCE_RANGE;

/** Config extension index to setup raw capture.
 *  See ::NVX_CONFIG_CAMERARAWCAPTURE
 */
#define NVX_INDEX_CONFIG_CAMERARAWCAPTURE "OMX.Nvidia.index.config.camera.rawcapture"
/** Holds data to setup raw capture. */
typedef struct NVX_CONFIG_CAMERARAWCAPTURE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_U32 nCaptureCount;          /**< Number of frames to capture */
    OMX_STRING Filename;            /**< File name to store into */
} NVX_CONFIG_CAMERARAWCAPTURE;

/** Config extension index to enable/disable concurrent raw dump.
 *  See ::OMX_CONFIG_BOOLEANTYPE
 */
#define NVX_INDEX_CONFIG_CONCURRENTRAWDUMPFLAG "OMX.Nvidia.index.config.concurrentrawdumflag"

typedef enum
{
    NvxFlicker_Off,
    NvxFlicker_Auto,
    NvxFlicker_50HZ,
    NvxFlicker_60HZ
} ENvxFlickerType;

/** Config extension index to setup flicker.
 *  See ::NVX_CONFIG_FLICKER
 */
#define NVX_INDEX_CONFIG_FLICKER "OMX.Nvidia.index.config.flicker"
/** Holds data to setup flicker. */
typedef struct NVX_CONFIG_FLICKER
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    ENvxFlickerType eFlicker;       /**< Flicker type */
} NVX_CONFIG_FLICKER;

typedef enum NVX_IMAGEFILTERTYPE {
    NVX_ImageFilterPosterize = 0x30000000,
    NVX_ImageFilterSepia,
    NVX_ImageFilterBW,
    NVX_ImageFilterManual,
    NVX_ImageFilterAqua,
    NVX_ImageFilterMax = 0x7FFFFFFF
} NVX_IMAGEFILTERTYPE;

/** Config extension index to setup hue.
 *  See ::NVX_CONFIG_HUETYPE
 */
#define NVX_INDEX_CONFIG_HUE "OMX.Nvidia.index.config.hue"
/** Holds data to setup hue. */
typedef struct NVX_CONFIG_HUETYPE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_S32 nHue;                   /**< Hue in degrees, 0 to 359 */
} NVX_CONFIG_HUETYPE;


/** Config extension index to control focus regions.
 * The rectangle coordinates are normalized to -1.0 to 1.0 on the
 * X and Y axis.
 * Auto focus region will work only when auto focus is enabled.
 *
 *  See ::NVX_CONFIG_FocusRegionsRect
 */
#define NVX_INDEX_CONFIG_FOCUSREGIONSRECT "OMX.Nvidia.index.config.focusregionsrect"
/** Holds data to control focus regions. */
typedef struct NVX_CONFIG_FocusRegionsRect
{
    OMX_U32     nRegions;                          /**< Number of regions */
    NVX_RectF32   regions[NVX_MAX_FOCUS_REGIONS];  /**< Array of NVX_RectF32 to specify each focus region */
} NVX_CONFIG_FocusRegionsRect;

/** Config extension index to control camera focusing regions sharpness values.
 *  You can use this interface to measure focus sharpness in manual mode.
 *  This interface fills in an ::NVX_CONFIG_FOCUSREGIONS_SHARPNESS structure.
 *  @note NVIDIA currently supports only one region, so the value of interest will be in:
 *  ::NVX_CONFIG_FOCUSREGIONS_SHARPNESS.nSharpness[0]
 */
#define NVX_INDEX_CONFIG_FOCUSREGIONSSHARPNESS "OMX.Nvidia.index.config.focusregionssharpness"
/** Holds data to control camera focusing regions sharpness values.
 * @sa NVX_INDEX_CONFIG_FOCUSREGIONSSHARPNESS
 */
typedef struct NVX_CONFIG_FOCUSREGIONS_SHARPNESS
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_U32           nRegions;                             /**< Number of currently active focusing regions */
    NVX_F32           nSharpness[NVX_MAX_FOCUS_REGIONS];    /**< Sharpness values for active focusing regions. @note Currently, NVIDIA supports only one region, so the value of interest will be in \c NVX_CONFIG_FOCUSREGIONS_SHARPNESS.nSharpness[0]. You can use the ::NVX_INDEX_CONFIG_FOCUSREGIONSSHARPNESS interface to measure focus sharpness in manual mode.  This interface fills in an ::NVX_CONFIG_FOCUSREGIONS_SHARPNESS structure.  */
} NVX_CONFIG_FOCUSREGIONS_SHARPNESS;

/** Config extension index for direct focuser control.
 *  @note This extension is deprecated. What you probably want is
 *  OMX_IndexConfigFocusControl.
 */
#define NVX_INDEX_CONFIG_DIRECTFOCUSERCONTROL "OMX.Nvidia.index.config.directfocusercontrol"
/** Holds a direct focuser control. */
typedef struct NVX_CONFIG_DIRECT_FOCUSERCONTROL
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_U32           nLength;                                              /**< Length of focuser control sequence */
    OMX_U32           nControl[NVX_MAX_DIRECT_FOCUSER_CONTROL_LENGTH];      /**< Focuser control sequence */
} NVX_CONFIG_DIRECT_FOCUSERCONTROL;

/** Config extension for querying camera focusing parameters.
 *
 *  See ::NVX_CONFIG_FOCUSER_PARAMETERS
 */
#define NVX_INDEX_CONFIG_FOCUSERPARAMETERS "OMX.Nvidia.index.config.focuserparameters"

typedef struct NVX_CONFIG_FOCUSER_PARAMETERS
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_S32           minPosition;          /**< Minimum focus position supported by Camera */
    OMX_S32           maxPosition;          /**< Maximum focus position supported by Camera */
    OMX_S32           hyperfocal;           /**< Hyperfocal focus position */
    OMX_S32           macro;                /**< Macro focus position */
    OMX_S32           powerSaving;          /**< Most power-efficient focus position */
                                            /**< (-1) if any position is good */
    OMX_BOOL sensorispAFsupport;
    OMX_BOOL          infModeAF;            /**< AF support in Infinity mode */
    OMX_BOOL          macroModeAF;          /**< AF support in Macro mode */
} NVX_CONFIG_FOCUSER_PARAMETERS;

/** Config extension for querying camera focusing parameters.
 *  Following variables map to the coresponding variables in structure NvIspAfConfigParams
 *  File camera/core/camera/alg/af/autofocus_common.h
 *
 *  See ::NVX_CONFIG_FOCUSER_INFO
 */
#define NVX_INDEX_CONFIG_FOCUSER_INFO "OMX.Nvidia.index.config.focuserinfo"

typedef struct NVX_CONFIG_FOCUSER_INFO
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_S32 positionPhysicalLow;
    OMX_S32 positionPhysicalHigh;
    OMX_S32 positionInf;
    OMX_S32 positionInfOffset;
    OMX_S32 positionMacro;
    OMX_S32 positionMacroOffset;
    OMX_S32 positionHyperFocal;
    OMX_S32 positionResting;
    OMX_S32 infMin;                 /**< Percentage value of the working range */
    OMX_S32 macroMax;               /**< Percentage value of the working range */
    OMX_S32 infInitSearch;          /**< Percentage value of the working range */
    OMX_S32 macroInitSearch;        /**< Percentage value of the working range */
    OMX_BOOL rangeEndsReversed;
    OMX_S32 hysteresis;
    OMX_S32 slewRate;
    OMX_S32 settleTime;
} NVX_CONFIG_FOCUSER_INFO;

/** Config extension for querying camera flash parameters.
 *
 *  See ::NVX_CONFIG_FLASH_PARAMETERS
 */
#define NVX_INDEX_CONFIG_FLASHPARAMETERS "OMX.Nvidia.index.config.flashparameters"

typedef struct NVX_CONFIG_FLASH_PARAMETERS
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_BOOL bPresent;              /**< Flash available for this sensor. */
} NVX_CONFIG_FLASH_PARAMETERS;

/** Config extension for querying camera low-level focuser device capabilities.
 *  See ::NVX_CONFIG_FOCUSER_CAPABILITIES
 */
#define NVX_INDEX_CONFIG_FOCUSERCAPABILITIES "OMX.Nvidia.index.config.focusercapabilities"
/** Holds a querying focuser capabilities. */
typedef struct NVX_CONFIG_FOCUSER_CAPABILITIES
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_U32           nRange;               /**< Range of focuser movement in device-dependent steps */
    NVX_F32           directionFactor;      /**< Scaling factor for focuser device movement in "infinity" and "macro" directions */
    OMX_U32           nSettleTime;          /**< Guaranteed settling time in millisecs */
} NVX_CONFIG_FOCUSER_CAPABILITIES;

/** Config extension to set the AF auto focusing speed. Fast is potentially
 *  less accurate; not fast is accurate but potentially quite slow.
 */
#define NVX_INDEX_CONFIG_AUTOFOCUS_SPEED "OMX.Nvidia.index.config.autofocusspeed"
typedef struct  {
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_BOOL bFast;                 /**< OMX_TRUE if you want fast focusing mode
                                         @note Slow mode may take several seconds
                                         to settle. */
} NVX_CONFIG_AUTOFOCUS_SPEED;

/** Holds a sensor mode configuration. */
typedef struct NVX_SENSORMODE
{
    OMX_S32 width;
    OMX_S32 height;
    NVX_F32 FrameRate;
} NVX_SENSORMODE;

/** Config extension index to control sensor mode list of supported resolution and frame rate.
 *  See ::NVX_CONFIG_SENSORMODELIST
 */
#define NVX_INDEX_CONFIG_SENSORMODELIST "OMX.Nvidia.index.config.sensormodelist"
/** Holds sensor modes. */
typedef struct NVX_CONFIG_SENSORMODELIST
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */

    OMX_U32 nSensorModes;                                   /**< Number of sensor modes */
    NVX_SENSORMODE SensorModeList[MAX_NUM_SENSOR_MODES];    /**< Array of sensor modes */
} NVX_CONFIG_SENSORMODELIST;

#define NVX_INDEX_CONFIG_CAPTUREMODE "OMX.Nvidia.index.config.capturemode"
/** Holds Capture Sensor Mode . */
typedef struct NVX_CONFIG_CAPTUREMODE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */

    NVX_SENSORMODE mMode;           /** Sensor Mode */
} NVX_CONFIG_CAPTUREMODE;

#define NVX_INDEX_PARAM_PREVIEWMODE "OMX.Nvidia.index.param.previewmode"
/** Holds Preview Sensor Mode . */
typedef struct NVX_PARAM_PREVIEWMODE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */

    NVX_SENSORMODE mMode;           /** Sensor Mode */
} NVX_PARAM_PREVIEWMODE;

/** Config extension index to get sensor ID.
 *  See ::OMX_PARAM_U32TYPE
 */
#define NVX_INDEX_PARAM_SENSORID "OMX.Nvidia.index.config.sensorid"

/** Param extension index to set full 64-bit sensor GUID
 * See ::NVX_PARAM_SENSOR_GUID
 */
#define NVX_INDEX_PARAM_SENSOR_GUID "OMX.Nvidia.index.param.sensorguid"

/** Config/Param extension index to get number of available sensors on current platform.
 *   (Read-only)
 *  See ::OMX_PARAM_U32TYPE
 */
#define NVX_INDEX_PARAM_AVAILABLESENSORS "OMX.Nvidia.index.param.availablesensors"

/** Param extension index to set sensor mode.
 *  Sets the user-selected sensor mode (width, height, and frame rate).
 *  The default sensor mode is auto, i.e., \a WxHxFramerate is 0x0x0.
 *  The default auto mode (0x0x0) basically implies that the camera
 *  driver picks up the correct sensor mode using its own justification.
 *  Once the user-selected sensor mode is set using this interface,
 *  the default auto mode can be achieved again by setting 0x0x0 as
 *  sensor mode.
 *  See ::NVX_PARAM_SENSORMODE
 */
#define NVX_INDEX_PARAM_SENSORMODE "OMX.Nvidia.index.config.sensormode"

/** Holds the data for sensor mode setting. */
typedef struct NVX_PARAM_SENSORMODE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    NVX_SENSORMODE SensorMode;      /**< Sensor mode */
}NVX_PARAM_SENSORMODE;

/** Param extension index to set the video speed on camera component.
 *  Sets the user-selected video speed (0.25x/0.5x/1x/2x/3x/4x).
 *  The default speed on the video port of camera is 1.
 *  See ::NVX_PARAM_VIDEOSPEED
 */
#define NVX_INDEX_CONFIG_VIDEOSPEED "OMX.Nvidia.index.config.videospeed"

/** Holds the data for camera frame speed setting on video port. */
typedef struct NVX_PARAM_VIDEOSPEED
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_BOOL bEnable;               /**< Boolean to enable/disable recording speed */
    NVX_F32 nVideoSpeed;            /**< Video speed*/
}NVX_PARAM_VIDEOSPEED;

/** Config extension index to enable custom postview processing.
 *  See ::OMX_PARAM_U32TYPE
 */
#define NVX_INDEX_CONFIG_CUSTOMPOSTVIEW "OMX.Nvidia.index.config.custompostview"
typedef struct NVX_PARAM_CUSTOMPOSTVIEW
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_BOOL bEnable;               /**< Boolean to enable/disable recording speed */
}NVX_PARAM_CUSTOMPOSTVIEW;


/**
 * Defines the stereo camera modes.
 */
typedef enum {
    NvxLeftOnly = 0x01,                         /**< Only the sensor on the left is on. */
    NvxRightOnly = 0x02,                        /**< Only the sensor on the right is on. */
    NvxStereo = (NvxLeftOnly | NvxRightOnly),   /**< Sets the stereo camera to stereo mode. */
    NvxStereoCameraForce32 = 0x7FFFFFFF
} ENvxCameraStereoCameraMode;

/** Param extension index to set camera mode, if the camera is stereo.
    Must be set before sensor power on.
    Has no effect for non-stereo cameras.
*/
#define NVX_INDEX_PARAM_STEREOCAMERAMODE "OMX.Nvidia.index.param.stereocameramode"
/** Holds the data for stereo mode setting. */
typedef struct NVX_PARAM_STEREOCAMERAMODE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    ENvxCameraStereoCameraMode StereoCameraMode;      /**< ENvxCameraStereoCameraMode to select stereo camera mode */
}NVX_PARAM_STEREOCAMERAMODE;

/**
 * Defines the stereo camera capture type.
 */
typedef enum {
    NvxCameraCaptureType_None,              // No capture.
    NvxCameraCaptureType_Video,             // Video capture.
    NvxCameraCaptureType_Still,             // Photo capture.
    NvxCameraCaptureType_StillBurst,        // Photo burst capture.
    NvxCameraCaptureType_VideoSquashed,     // Video capture (Sqashed frame).
    NvxCameraCaptureType_Force32 = 0x7FFFFFFFL
} ENvxCameraCaptureType;

/**
 * Defines the stereo camera type.
 */
typedef enum {
     Nvx_BufferFlag_Stereo_None,
     Nvx_BufferFlag_Stereo_LeftRight,
     Nvx_BufferFlag_Stereo_RightLeft,
     Nvx_BufferFlag_Stereo_TopBottom,
     Nvx_BufferFlag_Stereo_BottomTop,
     Nvx_BufferFlag_Stereo_SeparateLR,
     Nvx_BufferFlag_Stereo_SeparateRL
} ENvxCameraStereoType;

/** Holds the stereo camera information. */
typedef struct NVX_STEREOCAPTUREINFO
{
    ENvxCameraCaptureType CameraCaptureType;
    ENvxCameraStereoType CameraStereoType;
} NVX_STEREOCAPTUREINFO;

/** Param extension index to set camera mode, if the camera is stereo.
    Must be set before sensor power on.
    Has no effect for non-stereo cameras.
*/
#define NVX_INDEX_PARAM_STEREOCAPTUREINFO "OMX.Nvidia.index.param.stereocaptureinfo"
/** Holds the data for stereo info setting. */
typedef struct NVX_PARAM_STEREOCAPTUREINFO
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    NVX_STEREOCAPTUREINFO StCaptureInfo;
} NVX_PARAM_STEREOCAPTUREINFO;

/** Param extension index to set use native handle (for camera preview)
*/
#define NVX_INDEX_PARAM_USE_NBH "OMX.Nvidia.index.param.useNativeBufferHandles"
/** Holds the data for stereo mode setting. */
typedef struct NVX_PARAM_USENATIVEBUFFERHANDLE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_BUFFERHEADERTYPE **bufferHeader;
    OMX_PTR oNativeBufferHandlePtr; /**< pointer to native buffer handle */
}NVX_PARAM_USENATIVEBUFFERHANDLE;


/** Config extension index to setup edge enhancement.
 * Edge enhancement increases the apparent sharpness of full
 * resolution images. It has no effect on downscaled images.
 *
 *  See ::NVX_CONFIG_EDGEENHANCEMENT
 */
#define NVX_INDEX_CONFIG_EDGEENHANCEMENT "OMX.Nvidia.index.config.edgeenhancement"
/** Holds data to setup edge enhancement. */
typedef struct NVX_CONFIG_EDGEENHANCEMENT
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */

    OMX_BOOL bEnable;               /**< Boolean to enable/disable edge enhancement */
    NVX_F32 strengthBias;               /**< Bias the Strength of edge enhancement (float: -1 to 1) */
} NVX_CONFIG_EDGEENHANCEMENT;

/** Config extension index for ISP data dump.
 *  @deprecated This index is deprecated.
 */
#define NVX_INDEX_CONFIG_ISPDATA "OMX.Nvidia.index.config.ispdata"
/** @deprecated This structure is deprecated. */
typedef struct NVX_CONFIG_ISPDATA
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    NVX_F32 ilData;                 /**< ilData */
    NVX_F32 ilData2;                /**< ilData2 */
} NVX_CONFIG_ISPDATA;

/** Config extension index to set temporal tradeoff level for video encoder.
 *
 * For example:
 *  - 0 = Encoder delivers frames as fast as possible
 *  - 1 = Encoder drops 1 in 5 frames
 *  - 2 = Encoder drops 1 in 3 frames
 *  - 3 = Encoder drops 1 in 2 frames
 *  - 4 = Encoder drops 2 in 3 frames
 *
 *  See ::NVX_ENCODE_VIDEOTEMPORALTRADEOFF for values for video encode
 *       temporal tradeoff level.
 *
 *  @sa ::NVX_CONFIG_TEMPORALTRADEOFF
 */
#define NVX_INDEX_CONFIG_VIDEO_ENCODE_TEMPORALTRADEOFF "OMX.Nvidia.index.config.video.encode.temporaltradeoff"
/** Holds data to set temporal tradeoff. */
typedef struct NVX_CONFIG_TEMPORALTRADEOFF
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_U32 TemporalTradeOffLevel;  /**< Temporal tradeoff level */
} NVX_CONFIG_TEMPORALTRADEOFF;

/** Defines the encode video temporal tradeoff types used to
  * set the video temporal tradeoff level.
  * @sa ::NVX_CONFIG_TEMPORALTRADEOFF
  */
typedef enum NVX_ENCODE_VIDEOTEMPORALTRADEOFF{
    NVX_ENCODE_VideoEncTemporalTradeoffLevel_DropNone= 0,
    NVX_ENCODE_VideoEncTemporalTradeoffLevel_Drop1in5,
    NVX_ENCODE_VideoEncTemporalTradeoffLevel_Drop1in3,
    NVX_ENCODE_VideoEncTemporalTradeoffLevel_Drop1in2,
    NVX_ENCODE_VideoEncTemporalTradeoffLevel_Drop2in3,
    NVX_ENCODE_VideoEncTemporalTradeoffLevel_Force32 = 0x7FFFFFFF
} NVX_ENCODE_VIDEOTEMPORALTRADEOFF;

/** Config extension index to get decoder configartion information from video encoder.
 *  See ::NVX_CONFIG_DecoderConfigInfo
 */
#define NVX_INDEX_CONFIG_VIDEO_ENCODE_DCI "OMX.Nvidia.index.config.video.encode.dci"
/** Holds data to get decoder configuration. */
typedef struct NVX_CONFIG_DecoderConfigInfo
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_U8 DecoderConfigInfo[NVX_VIDEOENC_DCI_SIZE];    /**< Decoder configuration info*/
    OMX_U8 SizeOfDecoderConfigInfo;                     /**< Size of decoder configuration info*/
}NVX_CONFIG_DecoderConfigInfo;


/** Config extension index for enabling per-frame user-space copies of the preview buffer.
 *
 *  See ::NVX_CONFIG_PREVIEW_FRAME_COPY
 */
#define NVX_INDEX_CONFIG_PREVIEW_FRAME_COPY "OMX.Nvidia.index.config.previewframecopy"
/** Holds data to enable/disable copying of preview frames */
typedef struct NVX_CONFIG_PREVIEW_FRAME_COPY
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_BOOL enable;                /**< enable/disable bit */
    OMX_U32 skipCount;            /**<  skipcount : Number of frame to be skip for preview frame copy */
} NVX_CONFIG_PREVIEW_FRAME_COPY;

/** Config extension index for enabling per-still user-space copies of the
 *  still confirmation buffer.
 *
 *  See ::NVX_CONFIG_STILL_CONFIRMATION_FRAME_COPY
 */
#define NVX_INDEX_CONFIG_STILL_CONFIRMATION_FRAME_COPY "OMX.Nvidia.index.config.stillconfirmationframecopy"
/** Holds data to enable/disable copying of still-confirmation frames */
typedef struct NVX_CONFIG_STILL_CONFIRMATION_FRAME_COPY
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_BOOL enable;                /**< enable/disable bit */
} NVX_CONFIG_STILL_CONFIRMATION_FRAME_COPY;

/** Config extension index to set the camera configuration
 *
 * The ConfigString can be one of the below specified strings:
 *
 * - "ae.ConvergeSpeed=%f"
 * - "awb.ConvergeSpeed=%f"
 * - "ispFunction.lensShading=%s"
 *
 * where:
 * - %f is a float value
 * - %s is a string
 *
 * The float value for
 * - ae.ConvergeSpeed must be [0.01-1.0]
 * - awb.ConvergeSpeed must be [0.01-1.0]
 *
 * The string value for ispFunction.lensShading must be:
 * - TRUE/true to enable lens shading
 * - FALSE/false to disable lens shading
 * For ex : "ispFunction.lensShading=TRUE"
 *          "ispFunction.lensShading=FALSE"
 *
 * If the ConfigString passed does not "EXACTLY" match any of the above three strings, then
 * an error is returned.
 *
 * See ::NVX_CONFIG_CAMERACONFIGURATION
 */
#define NVX_INDEX_CONFIG_CAMERACONFIGURATION "OMX.Nvidia.index.config.cameraconfiguration"
/** Holds camera configuration string */
typedef struct NVX_CONFIG_CAMERACONFIGURATION
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_STRING ConfigString;        /**< Camera Configuration string */
} NVX_CONFIG_CAMERACONFIGURATION;

/** Config extension index for enabling per-still, user-space copies of the
 *  still YUV.
 *
 *  See ::NVX_CONFIG_STILL_YUV_FRAME_COPY
 */
#define NVX_INDEX_CONFIG_STILL_YUV_FRAME_COPY "OMX.Nvidia.index.config.stillYUVframecopy"
/** Holds data to enable/disable copying of still YUV frames. */
typedef struct NVX_CONFIG_STILL_YUV_FRAME_COPY
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_BOOL enable;                /**< Enable/disable bit */
} NVX_CONFIG_STILL_YUV_FRAME_COPY;

/** Config extension index to querying if the camera is stereo.
 */
#define NVX_INDEX_CONFIG_STEREOCAPABLE "OMX.Nvidia.index.config.stereocapable"
/** Holds the data from stereo capability query. */
typedef struct NVX_CONFIG_STEREOCAPABLE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_BOOL StereoCapable;         /**< Boolean to indicate if this is a stereo camera */
} NVX_CONFIG_STEREOCAPABLE;

/** Config extension index for querying the scene brightness.
  * The brightness of the scene is the average scene luminance divided by the exposure value.
  * The higher the value, the brighter the scene.
  *
  * Read-only
 */
#define NVX_INDEX_CONFIG_SCENEBRIGHTNESS "OMX.Nvidia.index.config.scenebrightness"
/** Holds the data from scene brightness query. */
typedef struct NVX_CONFIG_SCENEBRIGHTNESS
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    NVX_F32 SceneBrightness;        /**< Scene brightness level */
    OMX_BOOL FlashNeeded;             /**< If flash light is needed for next capture */
} NVX_CONFIG_SCENEBRIGHTNESS;

/** Holds the custom block info from imager.  */
typedef struct NVX_CUSTOMINFO
{
  OMX_U32 SizeofData;
  OMX_STRING Data;
} NVX_CUSTOMINFO;

/** Config extension index to query custom block info from imager.
 */
#define NVX_INDEX_CONFIG_CUSTOMIZEDBLOCKINFO "OMX.Nvidia.index.config.customizedblockinfo"
/** Holds custom block info from imager. */
typedef struct NVX_CONFIG_CUSTOMIZEDBLOCKINFO
{
    OMX_U32 nSize;               /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;    /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;          /**< Port that this struct applies to */
    NVX_CUSTOMINFO CustomInfo;   /**< Custom info from imager */
} NVX_CONFIG_CUSTOMIZEDBLOCKINFO;

/** Config extension index for enabling advanced noise reduction.
 *
 *  See ::NVX_CONFIG_ADVANCED_NOISE_REDUCTION
 */
#define NVX_INDEX_CONFIG_ADVANCED_NOISE_REDUCTION "OMX.Nvidia.index.config.advancedNoiseReduction"
/** Holds data to control advanced noise reduction.

    The thresholds below are used to control the shape of the data
    Gaussian use to measure similarity between colors. Useful values
    will be between 0 and 0.2. The smaller the value, the more picky
    we are about considering something similar. The value of 0 means
    to disable filtering altogether for the corresponding channels.

    For each of luma, chroma:
      threshold[0] is used for horizontal filtering
      threshold[1] is used for vertical filtering
 */
typedef struct NVX_CONFIG_ADVANCED_NOISE_REDUCTION {
    OMX_U32 nSize;                      /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;           /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;                 /**< Port that this struct applies to */
    OMX_BOOL enable;                    /**< Enable/disable bit */
    NVX_F32  lumaThreshold[2];          /**< Filter parameters for luma */
    NVX_F32  chromaThreshold[2];        /**< Filter parameters for chroma */
    OMX_U32  lumaIsoThreshold;          /**< Disable luma filtering if ISO below this thresh */
    OMX_U32  chromaIsoThreshold;        /**< Disable chroma filtering if ISO below this thresh */
} NVX_CONFIG_ADVANCED_NOISE_REDUCTION;

// Post-processing noise reduction stuff

#define NVX_INDEX_CONFIG_NOISE_REDUCTION_V1 "OMX.Nvidia.index.config.nr.V1"
#define NVX_INDEX_CONFIG_NOISE_REDUCTION_V2 "OMX.Nvidia.index.config.nr.V2"

enum { NVX_NR_V2_PRESET_LEVELS   = 5 };
enum { NVX_NR_V2_GAIN_INDEX_SIZE = 16 };
enum { NVX_NR_V2_REDUCTION_DEPTH = 3 }; // Pyramid depth (not counting top band)
enum { NVX_NR_V2_DOWNSCALE_TAPS  = 2 };

typedef struct {
    OMX_U32 nSize;                      /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;           /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;                 /**< Port that this struct applies to */

    // Enables the Bilateral Noise reduction
    OMX_BOOL lumaEnable;
    OMX_BOOL chromaEnable;

    // Turns on the noise reduction only above specified ISO
    OMX_U32 lumaIsoThreshold;
    OMX_U32 chromaIsoThreshold;

    // Threshold parameters used to control how picky we are about
    // what a similar pixel is. Used to control the width of the
    // gaussian tent function. Useful values will be between 0 and 0.2.
    // If 0, then the corresponding planes will not be filtered.
    // Only the horizontal (1st pass) filtering is specified;
    // the vertical (2nd pass) is now forced to 0.58 * horizontal
    NVX_F32 lumaThreshold;
    NVX_F32 chromaThreshold;

    // Controls how we increase the thresholds as a function of the
    // gain applied by AE. Thresholds specified above apply when the
    // gain = 1.0, and increase with gain according to these values.
    NVX_F32 lumaSlope;
    NVX_F32 chromaSlope;
} NVX_CONFIG_NOISE_REDUCTION_V1;

typedef struct {
    OMX_U32 nSize;                      /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;           /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;                 /**< Port that this struct applies to */

    // Enables the Pyramid Noise reduction
    OMX_BOOL chromaEnable;

    // Turns on the chroma noise reduction only above specified ISO
    OMX_U32 chromaIsoThreshold;

    // Adjusts for sensor gain for gain index computation
    NVX_F32 chromaThreshold;
    NVX_F32 chromaSlope;

    // chromaSpeed == FALSE then disable 3x3 downsampler and use averaging downsampler
    OMX_BOOL chromaSpeed;

    // Enables the 5x5 luma convolution
    OMX_BOOL lumaConvolutionEnable;
} NVX_CONFIG_NOISE_REDUCTION_V2;

#define NVX_INDEX_CONFIG_BAYERGAINS "OMX.Nvidia.index.config.bayergains"
/** Reserved. Adjusts the per-channel bayer gains. */
typedef struct NVX_CONFIG_BAYERGAINS
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    NVX_F32 gains[4];                 /**< Gains per Bayer channel */
} NVX_CONFIG_BAYERGAINS;

/** Config extension index to get gain range of sensor mode. */
#define NVX_INDEX_CONFIG_GAINRANGE "OMX.Nvidia.index.config.gainrange"
typedef struct NVX_CONFIG_GAINRANGE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    NVX_F32 low;                    /**< Minimum gain of mode */
    NVX_F32 high;                   /**< Maximum gain of mode */
} NVX_CONFIG_GAINRANGE;

#define NVX_INDEX_CONFIG_VIDEOFRAME_CONVERSION "OMX.Nvidia.index.config.videoframeconversion"
/** Reserved. Produces video frame data in user-specified format from OMX buffer. */

typedef enum NVX_VIDEOFRAME_FORMAT {
    NVX_VIDEOFRAME_FormatYUV420 = 0,
    NVX_VIDEOFRAME_FormatNV21,
    NVX_VIDEOFRAME_FormatInvalid = 0x7FFFFFFF
} NVX_VIDEOFRAME_FORMAT;

typedef struct NVX_CONFIG_VIDEOFRAME_CONVERSION
{
    OMX_U32 nSize;                    /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;         /**< NVX extensions specification version */
    OMX_U32 nPortIndex;               /**< Port that this struct applies to */
    NVX_VIDEOFRAME_FORMAT userFormat; /**< Requested YUV data format */
    OMX_PTR omxBuffer;                /**< OMX buffer to take data from */
    OMX_U32 nDataSize;                /**< Converted data size in bytes */
    OMX_PTR data;                     /**< Pointer to data */
} NVX_CONFIG_VIDEOFRAME_CONVERSION;

/** Config extension index for enabling capture priority.
 *  In capture priority mode, still/video capture has a higher priority than
 *  preview, so still/video frame rate may increase while preview frame rate
 *  may decrease.
 */
#define NVX_INDEX_CONFIG_CAPTURE_PRIORITY "OMX.Nvidia.index.config.capturepriority"
/** Holds data to enable/disable capture priority. */
typedef struct NVX_CONFIG_CAPTURE_PRIORITY
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_BOOL Enable;                /**< Enable/disable bit */
} NVX_CONFIG_CAPTURE_PRIORITY;

#define NVX_INDEX_CONFIG_LENS_PHYSICAL_ATTR "OMX.Nvidia.index.config.lensPhysicalAttr"
/** Config extension index for getting focal length. */
typedef struct NVX_CONFIG_LENS_PHYSICAL_ATTR
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    NVX_F32 eFocalLength;           /**< Focal length */
    NVX_F32 eHorizontalAngle;       /**< Horizontal view angle */
    NVX_F32 eVerticalAngle;         /**< Vertical view angle */
} NVX_CONFIG_LENS_PHYSICAL_ATTR;

/** Config extension index to get focus distances.
 *  Gets the distances from the camera to where an object appears to be
 *  in focus. The object is sharpest at the optimal focus distance. The
 *  depth of field is the far focus distance minus near focus distance.
 *  See ::NVX_CONFIG_FOCUSDISTANCES
 */
#define NVX_INDEX_CONFIG_FOCUSDISTANCES "OMX.Nvidia.index.config.focusdistances"
/** Holds data to setup focus distance. */
typedef struct NVX_CONFIG_FOCUSDISTANCES
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version
                                         information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    NVX_F32 NearDistance;           /**< Near distance */
    NVX_F32 OptimalDistance;        /**< Optimal distance */
    NVX_F32 FarDistance;            /**< Far distance */
} NVX_CONFIG_FOCUSDISTANCES;

/** Config extension index to set a exposure bracket capture.
 *  This capture returns a burst of images each with a specified
 *  AE adjustment.  The avaliable range is (-3.0, +3.0)
 *  See ::NVX_INDEX_CONFIG_BRACKETCAPTURE
 */
#define NVX_INDEX_CONFIG_BRACKETCAPTURE "OMX.Nvidia.index.config.bracketcapture"
/** Holds data to setup focus distance. */
typedef struct NVX_CONFIG_EXPOSUREBRACKETCAPTURE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version
                                         information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_U32 NumberOfCaptures;       /**< Number of valid adjustments */
    NVX_F32 FStopAdjustments[32];      /**< Array of adjustments*/
} NVX_CONFIG_EXPOSUREBRACKETCAPTURE;

/** Config extension index to set a continuous autofocus.
 *  See ::OMX_CONFIG_BOOLEANTYPE
 */
#define NVX_INDEX_CONFIG_CONTINUOUSAUTOFOCUS "OMX.Nvidia.index.config.continuousautofocus"

/** Config extension index to set a face-assisted (continuous) autofocus.
 *  See ::NVX_INDEX_CONFIG_FACEAUTOFOCUS
 */
#define NVX_INDEX_CONFIG_FACEAUTOFOCUS "OMX.Nvidia.index.config.faceautofocus"

/** Config extension index to pause continuous autofocus.
 *  See ::OMX_CONFIG_BOOLEANTYPE
 */
#define NVX_INDEX_CONFIG_CONTINUOUSAUTOFOCUS_PAUSE "OMX.Nvidia.index.config.continuousautofocuspause"

/** Config extension index to query the continuous autofocus alg state.
 *  See ::NVX_CONFIG_CONTINUOUSAUTOFOCUS_STATE
 */
#define NVX_INDEX_CONFIG_CONTINUOUSAUTOFOCUS_STATE "OMX.Nvidia.index.config.continuousautofocusstate"
/** Holds information about the CAF alg state */
typedef struct NVX_CONFIG_CONTINUOUSAUTOFOCUS_STATE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version
                                         information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_BOOL bConverged;            /**< Tells whether the alg is converged */
} NVX_CONFIG_CONTINUOUSAUTOFOCUS_STATE;

#define NVX_INDEX_CONFIG_MANUALTORCHAMOUNT  "OMX.Nvidia.index.config.manualtorchamount"
#define NVX_INDEX_CONFIG_MANUALFLASHAMOUNT  "OMX.Nvidia.index.config.manualflashamount"
#define NVX_INDEX_CONFIG_AUTOFLASHAMOUNT    "OMX.Nvidia.index.config.autoflashamount"

/** Holds data to specify the amount of manual torch, manual flash, or auto flash for camera. */
typedef struct NVX_CONFIG_FLASHTORCHAMOUNT
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    NVX_F32  value;                 /**< Flash/torch value */
} NVX_CONFIG_FLASHTORCHAMOUNT;

/**
 * Defines the Camera ScaleTypes
 */
typedef enum NVX_CAMERA_SCALETYPE
{
    /**
     * Don't preserve aspect ratio. Output may be stretched if input does
     * not have the same aspect ratio as output.
     */
    NVX_CAMERA_ScaleType_Normal=0x1,

    /**
     * Preserve aspect ratio. Stretch the input image to the center
     * of output surface. Pad black color on output surface outside the
     * stretched input image.
     */
    NVX_CAMERA_ScaleType_Pad,

    /**
     * Preserve aspect ratio. Crop input image to fit output surface so
     * the whole output surface will be filled with cropped input image.
     */
    NVX_CAMERA_ScaleType_Crop,

    /**
     * Don't preserve aspect ratio. Output may be stretched if input does
     * not have the same aspect ratio as output. For rotation involving
     * XY swap like 90/270, the output surface doesn't get transformed.
     */
    NVX_CAMERA_ScaleType_Stretched,

    NVX_CAMERA_ScaleType_Force32 = 0x7FFFFFFF
}NVX_CAMERA_SCALETYPE;

#define NVX_INDEX_CONFIG_COMMONSCALETYPE  "OMX.Nvidia.index.config.commonscaletype"

/** Holds scale type for preview, still and video output. */
typedef struct NVX_CONFIG_COMMONSCALETYPE
{
    OMX_U32 nSize;                       /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;            /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;                  /**< Port that this struct applies to */
    NVX_CAMERA_SCALETYPE type;           /**< NVX_CAMERA_SCALETYPE */
} NVX_CONFIG_COMMONSCALETYPE;

/** Param extension index to enable/disable VI/ISP pre-scaling in camera for
 *  capture output.
 *  Several of the Tegra ISP features need to be disabled when pre-scaling is
 *  ON. For instance, EdgeEnhancement and Emboss.
 *  By default, pre-scaling is turned ON.
 */
#define NVX_INDEX_PARAM_PRESCALER_ENABLE_FOR_CAPTURE "OMX.Nvidia.index.param.prescalerenableforcapture"

/** Holds data to enable/disable VI/ISP pre-scaling in camera for capture output. */
typedef struct NVX_PARAM_PRESCALER_ENABLE_FOR_CAPTURE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_BOOL Enable;                /**< Enable/disable bit */
} NVX_PARAM_PRESCALER_ENABLE_FOR_CAPTURE;

/** Param extension index to fine tune video encoder and decoder buffer configuration.
 *  See ::NVX_PARAM_USELOWBUFFER
 */
#define NVX_INDEX_PARAM_USELOWBUFFER "OMX.Nvidia.index.param.uselowbuffer"
/** Holds data to fine tune video encoder and decoder buffer configuration. */
typedef struct NVX_PARAM_USELOWBUFFER
{
    OMX_U32 nSize;                 /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;      /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;            /**< Port that this struct applies to */
    OMX_BOOL bUseLowBuffer;         /**< Boolean to enable low buffer configuration */
} NVX_PARAM_USELOWBUFFER;

#define NVX_INDEX_CONFIG_FRAME_COPY_COLOR_FORMAT "OMX.Nvidia.index.config.framecopycolorformat"

/** Holds data to specify the color formats of preview, still confirmation,
 *  and still raw frame copies
 */
typedef struct NVX_CONFIG_FRAME_COPY_COLOR_FORMAT
{
    OMX_U32 nSize;                 /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;      /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;            /**< Port that this struct applies to */
    NVX_IMAGE_COLOR_FORMATTYPE PreviewFrameCopy;           /**< preview frame copy format */
    NVX_IMAGE_COLOR_FORMATTYPE StillConfirmationFrameCopy; /**< still confirmation frame copy format */
    NVX_IMAGE_COLOR_FORMATTYPE StillFrameCopy;             /**< still frame copy format */
} NVX_CONFIG_FRAME_COPY_COLOR_FORMAT;

#define NVX_INDEX_CONFIG_NSLBUFFERS "OMX.Nvidia.index.config.nslbuffers"
/** Holds data to specify the number of buffers to use for negative shutter lag */
typedef struct NVX_CONFIG_NSLBUFFERS
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_U32 numBuffers;             /**< Number of buffers to use for NSL */
} NVX_CONFIG_NSLBUFFERS;

#define NVX_INDEX_CONFIG_NSLSKIPCOUNT "OMX.Nvidia.index.config.nslskipcount"
/** Holds data to specify the number of buffers to skip between each buffer that
 *  is saved to the NSL buffer list.
 */
typedef struct NVX_CONFIG_NSLSKIPCOUNT
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_U32 skipCount;              /**< Number of buffers to skip before the next saved buffer */
} NVX_CONFIG_NSLSKIPCOUNT;


#define NVX_INDEX_CONFIG_COMBINEDCAPTURE "OMX.Nvidia.index.config.combinedcapture"
/** Holds data to specify the capture request parameters */
typedef struct NVX_CONFIG_COMBINEDCAPTURE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_U32 nNslImages;             /**< Number of images to pull from NSL buffer.
                                         Only applies to still port.*/
    OMX_U32 nNslPreFrames;          /**< Number of most-recent NSL frames to discard
                                         before sending frames to encoder.
                                         Only applies to still port.*/
    OMX_U32 nNslSkipCount;          /**< Number of NSL frames to skip between frames
                                         that are sent to encoder.  THIS FIELD IS
                                         DEPRECATED, NVX_CONFIG_NSLSKIPCOUNT should be
                                         used instead.*/
    OMX_U64 nNslTimestamp;          /**< Latest "valid" NSL capture time.  Driver
                                         will never give a frame that is newer
                                         than this as part of the NSL capture.
                                         Units of microseconds.
                                         Only applies to still port.*/
    OMX_U32 nImages;                /**< When this config is set on the still port, this
                                         sets the number of burst frames to capture and send
                                         to the encoder. When this config is set on the
                                         video port, a nonzero value will start capture,
                                         and a zero value will stop capture.*/
    OMX_U32 nSkipCount;             /**< Number of frames to skip between frames
                                         that are sent to encoder.
                                         Only applies to still port.*/

} NVX_CONFIG_COMBINEDCAPTURE;

#define NVX_INDEX_CONFIG_SENSORISPSUPPORT "OMX.Nvidia.index.config.sensorispsupport"
/** Config extension for querying sensor ISP support */
typedef struct NVX_CONFIG_SENSORISPSUPPORT
{
    OMX_U32 nSize;               /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;    /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;          /**< Port that this struct applies to */
    OMX_BOOL bIspSupport;        /**< Boolean to signify whether the sensor uses ISP */
} NVX_CONFIG_SENSORISPSUPPORT;

#define NVX_INDEX_CONFIG_THUMBNAILENABLE "OMX.Nvidia.index.config.thumnailenable"
/** Holds data to enable and to disable thumbnails */
typedef struct NVX_CONFIG_THUMBNAILENABLE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */

    OMX_BOOL bThumbnailEnabled;      /**< Boolean to enable/disable thumbnails */
} NVX_CONFIG_THUMBNAILENABLE;

#define NVX_INDEX_CONFIG_REMAININGSTILLIMAGES "OMX.Nvidia.index.config.remainingstillimages"

#define NVX_INDEX_CONFIG_LOWLIGHTTHRESHOLD "OMX.Nvidia.index.config.lowlightthreshold"
typedef struct NVX_CONFIG_LOWLIGHTTHRESHOLD
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_S32 nLowLightThreshold;     /**< Low light threshold in microseconds (exposure time) */
} NVX_CONFIG_LOWLIGHTTHRESHOLD;

#define NVX_INDEX_CONFIG_MACROMODETHRESHOLD "OMX.Nvidia.index.config.macromodethreshold"
typedef struct NVX_CONFIG_MACROMODETHRESHOLD
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_S32 nMacroModeThreshold;     /**< macro mode threshold in percents */
} NVX_CONFIG_MACROMODETHRESHOLD;

#define NVX_INDEX_CONFIG_FOCUSMOVEMSG "OMX.Nvidia.index.config.focusmovemsg"

typedef struct NVX_CONFIG_FDLIMIT
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_S32 limit;                  /**< Number of faces FD can detect at most */
} NVX_CONFIG_FDLIMIT;
#define NVX_INDEX_CONFIG_FDLIMIT "OMX.Nvidia.index.config.fdlimit"
#define NVX_INDEX_CONFIG_FDMAXLIMIT "OMX.Nvidia.index.config.fdmaxlimit"

typedef struct NVX_CONFIG_FDRESULT
{
    OMX_U32 nSize;                              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;                   /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;                         /**< Port that this struct applies to */
    OMX_S32 num;                                /**< Tell how many faces are detected. */
    OMX_S8  fdResult[NVX_MAX_FD_OUTPUT_LENGTH]; /**< FD results will be copied here. */
} NVX_CONFIG_FDRESULT;
#define NVX_INDEX_CONFIG_FDRESULT "OMX.Nvidia.index.config.fdresult"

typedef struct NVX_CONFIG_BOOLEAN
{
    OMX_U32 nSize;                              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;                   /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;                         /**< Port that this struct applies to */
    OMX_BOOL enable;                            /**< on/off */
} NVX_CONFIG_BOOLEAN;

typedef NVX_CONFIG_BOOLEAN NVX_CONFIG_FDDEBUG;
#define NVX_INDEX_CONFIG_FDDEBUG "OMX.Nvidia.index.config.fddebug"


/** Config extension index for bypassing Dz.
 *  See ::OMX_CONFIG_BOOLEANTYPE
 */
#define NVX_INDEX_CONFIG_STILLPASSTHROUGH "OMX.Nvidia.index.config.stillpassthrough"

/** Config extension index for giving preference to capture mode.
*  See ::OMX_CONFIG_BOOLEANTYPE
*/
#define NVX_INDEX_CONFIG_CAPTURESENSORMODEPREF "OMX.Nvidia.index.config.capturesensormodepref"

typedef struct NVX_CONFIG_FASTBURSTEN
{
    OMX_U32 nSize;                              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;                   /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;                         /**< Port that this struct applies to */
    OMX_BOOL enable;
} NVX_CONFIG_FASTBURSTEN;
#define NVX_INDEX_CONFIG_FASTBURSTEN "OMX.Nvidia.index.config.fastburst"

#define NVX_INDEX_CONFIG_CAMERAMODE "OMX.Nvidia.index.config.cameramode"
/** Param extension index to enable/disable RAW output.
 *  When RAW ouput is enabled preview port should be disabled.
 *  See ::OMX_CONFIG_BOOLEANTYPE
 */
#define NVX_INDEX_PARAM_RAWOUTPUT "OMX.Nvidia.index.param.rawoutput"

/** Config extension index for programming device rotation.  Write-only.
 *
 *  See: OMX_CONFIG_ROTATIONTYPE
 */
#define NVX_INDEX_CONFIG_DEVICEROTATE "OMX.Nvidia.index.config.devicerotate"

/** Config extension index for to set concurrent capture requests sent to VI HW.
* Must not be called when capture is active. Call only when camera component is in idle state.
* See ::OMX_PARAM_U32TYPE
*/
#define NVX_INDEX_CONFIG_CONCURRENTCAPTUREREQUESTS "OMX.Nvidia.index.config.concurrentcapturerequests"

/**Config extension index for allocating remaining buffers during runtime
*  @deprecated This index is deprecated.
*/
#define NVX_INDEX_CONFIG_ALLOCATEREMAININGBUFFERS "OMX.Nvidia.index.config.allocateremainingbuffers"
typedef struct NVX_CONFIG_REMAININGBUFFERTYPE
{
    OMX_U32 nSize; /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion; /**< NVX extensions specification version information */
    OMX_U32 nPortIndex; /**< Port that this struct applies to */
    OMX_BOOL enableAllocateBuffers; /**< Flag to enable or disable */
    OMX_U32 numBuffers; /**< number of remaining Buffers to be allocated */
} NVX_CONFIG_REMAININGBUFFERTYPE;

typedef struct NVX_CONFIG_STILLPASSTHROUGH
{
    OMX_U32 nSize; /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion; /**< NVX extensions specification version information */
    OMX_U32 nPortIndex; /**< Port that this struct applies to */
    OMX_BOOL enablePassThrough; /**< Still Pass Through mode on/off */
} NVX_CONFIG_STILLPASSTHROUGH;

#endif

/** @} */
/* File EOF */
