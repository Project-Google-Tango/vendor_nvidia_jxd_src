/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef INCLUDED_NVCAM_PROPERTIES_PUBLIC_H
#define INCLUDED_NVCAM_PROPERTIES_PUBLIC_H

#include "nvmm.h"
#include "nvmm_exif.h"
#include "nvmm_camera_types.h"
#include "nvcamera_isp.h"

#ifdef __cplusplus
extern "C"
{
#endif
#define DEFAULT_LIST_SIZE 32
#define MAX_NUM_SENSOR_MODES 30
#define MAKERNOTE_DATA_SIZE 2048
#define USB_CAMERA_GUID 0xFF
#define MAX_FM_WIN_X  (64)
#define MAX_FM_WIN_Y  (64)
#define MAX_FM_WINDOWS (MAX_FM_WIN_X * MAX_FM_WIN_Y)
// Currently we send partial results of AE/AWB/AF hence the count is 3
// set the correct count as more clients are added.
#define SUPPORTED_PARTIAL_RESULT_COUNT (3)

/**
 * @var NvCamSupportedHardwareLevel_Limited Limited mode has hardware requirements roughly in line
 *      with those for a camera HAL device v1 implementation, and is expected from older or inexpensive
 *      devices.
 *
 * @var NvCamSupportedHardwareLevel_Full Full support is expected from new higher-end devices.
 */
typedef enum
{
    NvCamSupportedHardwareLevel_Limited = 1,
    NvCamSupportedHardwareLevel_Full,
    NvCamSupportedHardwareLevel_Force32 = 0x7FFFFFF
} NvCamSupportedHardwareLevel;


/**
 * @var NvCamColorCorrectionMode_Matrix Use the ColorCorrectionTransform matrix to do color conversion
 * @var NvCamColorCorrectionMode_Fast Must not slow down frame rate relative to raw bayer output
 * @var NvCamColorCorrectionMode_HighQuality Frame rate may be reduced by high quality
 */
typedef enum
{
    NvCamColorCorrectionMode_Matrix = 1,
    NvCamColorCorrectionMode_Fast,
    NvCamColorCorrectionMode_HighQuality,
    NvCamColorCorrectionMode_Force32 = 0x7FFFFFF
} NvCamColorCorrectionMode;

/**
 * Controls Flicker detection/correction modes.
 */
typedef enum
{
    NvCamAeAntibandingMode_Off,
    NvCamAeAntibandingMode_50Hz,
    NvCamAeAntibandingMode_60Hz,
    NvCamAeAntibandingMode_AUTO,
    NvCamAeAntibandingMode_Force32 = 0x7FFFFFF
} NvCamAeAntibandingMode;

/**
 * @var NvCamAeMode_Off Autoexposure is disabled; SensorExposureTime and
 *      SensorSensitivity controls are used
 *
 * @var NvCamAeMode_On Autoexposure is active, no flash control
 *
 * @var NvCamAeMode_OnAutoFlash If flash exists Autoexposure is active, auto flash control;
 *      flash may be fired when precapture trigger is activated, and for captures for
 *      which captureIntent = STILL_CAPTURE
 *
 * @var NvCamAeMode_onAlwaysFlash If flash exists Autoexposure is active, auto flash control
 *      for precapture trigger and always flash when captureIntent = STILL_CAPTURE
 *
 * @var NvCamAeMode_On_AutoFlashRedEye Optional Automatic red eye reduction with flash. If
 *      deemed necessary, red eye reduction sequence should fire when precapture trigger
 *      is activated, and final flash should fire when captureIntent = STILL_CAPTURE
*/
typedef enum
{
    NvCamAeMode_Off = 0x1,
    NvCamAeMode_On,
    NvCamAeMode_OnAutoFlash,
    NvCamAeMode_OnAlwaysFlash,
    NvCamAeMode_On_AutoFlashRedEye,
    NvCamAeMode_Force32 = 0x7FFFFFF
} NvCamAeMode;


/**
 * @var NvCamAePrecaptureTrigger_Idle The trigger is idle.
 *
 * @var NvCamAePrecaptureTrigger_Start The precapture metering sequence must be started.
 *      The exact effect of the precapture trigger depends on the current AE mode and state.
 */
typedef enum
{
    NvCamAePrecaptureTrigger_Idle = 0x1,
    NvCamAePrecaptureTrigger_Start,
    NvCamAePrecaptureTrigger_Force32 = 0x7FFFFFF
} NvCamAePrecaptureTrigger;


/**
 * @var NvCamAfTrigger_Idle The trigger is idle.
 *
 * @var NvCamAfTrigger_Start Autofocus must trigger now.
 *
 * @var NvCamAfTrigger_Cancel Autofocus must return to initial state, and cancel any active trigger.
 *
 */
typedef enum
{
    NvCamAfTrigger_Idle = 0x1,
    NvCamAfTrigger_Start,
    NvCamAfTrigger_InProgress,
    NvCamAfTrigger_Cancel,
    NvCamAfTrigger_Force32 = 0x7FFFFFF
} NvCamAfTrigger;


/**
 * @var NvCamAfMode_Off The 3A routines do not control the lens.
 *      FocusPosition is controlled by the application
 *
 * @var NvCamAfMode_Auto If lens is not fixed focus. Use MinimumFocusDistance to determine
 *      if lens is fixed focus In this mode, the lens does not move unless the autofocus
 *      trigger action is called. When that trigger is activated, AF must transition to
 *      ACTIVE_SCAN, then to the outcome of the scan (FOCUSED or NOT_FOCUSED). Triggering
 *      cancel AF resets the lens position to default, and sets the AF state to INACTIVE.
 *
 * @var NvCamAfMode_Macro In this mode, the lens does not move unless the autofocus trigger
 *      action is called. When that trigger is activated, AF must transition to ACTIVE_SCAN,
 *      then to the outcome of the scan (FOCUSED or NOT_FOCUSED). Triggering cancel AF resets
 *      the lens position to default, and sets the AF state to INACTIVE.
 *
 * @var NvCamAfMode_ContinuousVideo In this mode, the AF algorithm modifies the lens position
 *      continually to attempt to provide a constantly-in-focus image stream. The focusing
 *      behavior should be suitable for good quality video recording; typically this means
 *      slower focus movement and no overshoots. When the AF trigger is not involved, the AF
 *      algorithm should start in INACTIVE state, and then transition into PASSIVE_SCAN and
 *      PASSIVE_FOCUSED states as appropriate. When the AF trigger is activated, the algorithm
 *      should immediately transition into AF_FOCUSED or AF_NOT_FOCUSED as appropriate, and lock
 *      the lens position until a cancel AF trigger is received. Once cancel is received, the
 *      algorithm should transition back to INACTIVE and resume passive scan. Note that this
 *      behavior is not identical to CONTINUOUS_PICTURE, since an ongoing PASSIVE_SCAN must
 *      immediately be canceled.
 *
 * @var NvCamAfMode_ContinuousPicture In this mode, the AF algorithm modifies the lens position
 *      continually to attempt to provide a constantly-in-focus image stream. The focusing behavior
 *      should be suitable for still image capture; typically this means focusing as fast as possible.
 *      When the AF trigger is not involved, the AF algorithm should start in INACTIVE state, and then
 *      transition into PASSIVE_SCAN and PASSIVE_FOCUSED states as appropriate as it attempts to maintain
 *      focus. When the AF trigger is activated, the algorithm should finish its PASSIVE_SCAN if active,
 *      and then transition into AF_FOCUSED or AF_NOT_FOCUSED as appropriate, and lock the lens position
 *      until a cancel AF trigger is received. When the AF cancel trigger is activated, the algorithm
 *      should transition back to INACTIVE and then act as if it has just been started.
 *
 * @var NvCamAfMode_ExtDepthOfField Extended depth of field (digital focus). AF trigger is ignored,
 *      AF state should always be INACTIVE.
 */
typedef enum
{
    NvCamAfMode_Off = 0x1,
    NvCamAfMode_Auto,
    NvCamAfMode_Macro,
    NvCamAfMode_ContinuousVideo,
    NvCamAfMode_ContinuousPicture,
    NvCamAfMode_ExtDepthOfField,
    NvCamAfMode_Manual,
    NvCamAfMode_Force32 = 0x7FFFFFF
} NvCamAfMode;


/**
 * @brief Camera Use Case enumerations.
 * Defines the use case of the capture requests. It will be used to get
 * the default control properties for a certain use case.
 */
typedef enum
{
    /* Preview use case */
    NvCameraCoreUseCase_Preview = 0x1,

    /* Still capture use case */
    NvCameraCoreUseCase_Still,

    /* Video recording use case */
    NvCameraCoreUseCase_Video,

    /* Still capture during video recording. */
    NvCameraCoreUseCase_VideoSnapshot,

    /* Zero shutter lag use case */
    NvCameraCoreUseCase_ZSL,

    NvCameraCoreUseCase_Force32 = 0x7FFFFFFF,

} NvCameraCoreUseCase;

/**
 * Defines the various available Auto White Balance modes.
 *
 */
typedef enum
{
    NvCamAwbMode_Off = 0,
    NvCamAwbMode_Auto,
    NvCamAwbMode_Incandescent,
    NvCamAwbMode_Fluorescent,
    NvCamAwbMode_WarmFluorescent,
    NvCamAwbMode_Daylight,
    NvCamAwbMode_CloudyDaylight,
    NvCamAwbMode_Twilight,
    NvCamAwbMode_Shade,
    NvCamAwbMode_Manual,
    NvCamAwbMode_Num_Total_Modes,
    NvCamAwbMode_Force32 = 0x7FFFFFF
} NvCamAwbMode;


/**
 * Defines the various available color effects modes.
 *
 */
typedef enum
{
    NvCamColorEffectsMode_Off = 0x1,
    NvCamColorEffectsMode_Mono,
    NvCamColorEffectsMode_Negative,
    NvCamColorEffectsMode_Solarize,
    NvCamColorEffectsMode_Sepia,
    NvCamColorEffectsMode_Posterize,
    NvCamColorEffectsMode_Aqua,
    NvCamColorEffectsMode_Force32 = 0x7FFFFFF
} NvCamColorEffectsMode;


/**
 * @var NvCamControlMode_Off Full application control of pipeline. All 3A routines are disabled,
 *      no other settings in NvCamProperty_Controls have any effect.
 *
 * @var NvCamControlMode_Auto Use settings for each individual 3A routine. Manual control of
 *      capture parameters is disabled. All controls in NvCamProperty_Controls besides sceneMode
 *      take effect.
 *
 * @var NvCamControlMode_UseSceneMode Use specific scene mode. Enabling this disables AeMode,
 *      AwbMode and AfMode controls; the driver must ignore those settings while NvCamControlMode_UseSceneMode
 *      is active (except for FACE_PRIORITY scene mode). Other control entries are still active.
 *      This setting can only be used if availableSceneModes != UNSUPPORTED
 */
typedef enum
{
    NvCamControlMode_Off = 0x1,
    NvCamControlMode_Auto,
    NvCamControlMode_UseSceneMode,
    NvCamControlMode_Force32 = 0x7FFFFFF
} NvCamControlMode;


/**
 * Defines the various supported scene modes.
 */
typedef enum
{
    NvCamSceneMode_FacePriority,
    NvCamSceneMode_Action,
    NvCamSceneMode_Portrait,
    NvCamSceneMode_Landscape,
    NvCamSceneMode_Night,
    NvCamSceneMode_NightPortrait,
    NvCamSceneMode_Theatre,
    NvCamSceneMode_Beach,
    NvCamSceneMode_Snow,
    NvCamSceneMode_Sunset,
    NvCamSceneMode_SteadyPhoto,
    NvCamSceneMode_Fireworks,
    NvCamSceneMode_Sports,
    NvCamSceneMode_Party,
    NvCamSceneMode_CandleLight,
    NvCamSceneMode_Barcode,
    NvCamSceneMode_Size,
    NvCamSceneMode_Unsupported,
    NvCamSceneMode_Force32 = 0x7FFFFFF
} NvCamSceneMode;


/**
 * @var NvCamDemosaicMode_Fast Minimal or no slowdown of frame rate compared to Bayer RAW output.
 *
 * @var NvCamDemosaicMode_HighQuality High-quality may reduce output frame rate.
 */
typedef enum
{
    NvCamDemosaicMode_Fast = 0x1,
    NvCamDemosaicMode_HighQuality,
    NvCamDemosaicMode_Force32 = 0x7FFFFFF
} NvCamDemosaicMode;


/**
 * @var NvCamEdgeEnhanceMode_Off No edge enhancement is applied.
 *
 * @var NvCamEdgeEnhanceMode_Fast Must not slow down frame rate relative to raw bayer output.
 *
 * @var NvCamEdgeEnhanceMode_HighQuality High-quality may reduce output frame rate.
 */
typedef enum
{
    NvCamEdgeEnhanceMode_Off,
    NvCamEdgeEnhanceMode_Fast,
    NvCamEdgeEnhanceMode_HighQuality,
    NvCamEdgeEnhanceMode_Force32 = 0x7FFFFFF
} NvCamEdgeEnhanceMode;


/**
 * @var NvCamGeometricMode_Off No geometric correction is applied.
 *
 * @var NvCamGeometricMode_Fast Must not slow down frame rate relative to raw bayer output.
 *
 * @var NvCamGeometricMode_HighQuality High-quality may reduce output frame rate.
 */
typedef enum
{
    NvCamGeometricMode_Off,
    NvCamGeometricMode_Fast,
    NvCamGeometricMode_HighQuality,
    NvCamGeometricMode_Force32 = 0x7FFFFFF
} NvCamGeometricMode;

/**
 * @var NvCamHotPixelMode_Off No hot pixel correction is applied.
 *
 * @var NvCamHotPixelMode_Fast Must not slow down frame rate relative to raw bayer output.
 *
 * @var NvCamHotPixelMode_HighQuality High-quality may reduce output frame rate.
 */
typedef enum
{
    NvCamHotPixelMode_Off,
    NvCamHotPixelMode_Fast,
    NvCamHotPixelMode_HighQuality,
    NvCamHotPixelMode_Force32 = 0x7FFFFFF
} NvCamHotPixelMode;


/**
 * @var NvCamNoiseReductionMode_Off No noise reduction is applied.
 *
 * @var NvCamNoiseReductionMode_Fast Must not slow down frame rate relative to raw bayer output.
 *
 * @var NvCamNoiseReductionMode_HighQuality High-quality may reduce output frame rate.
 */
typedef enum
{
    NvCamNoiseReductionMode_Off,
    NvCamNoiseReductionMode_Fast,
    NvCamNoiseReductionMode_HighQuality,
    NvCamNoiseReductionMode_Force32 = 0x7FFFFFF
} NvCamNoiseReductionMode;


/**
 * @var NvCamShadingMode_Off No shading correction is applied.
 *
 * @var NvCamShadingMode_Fast Must not slow down frame rate relative to raw bayer output.
 *
 * @var NvCamShadingMode_HighQuality High-quality may reduce output frame rate.
 */
typedef enum
{
    NvCamShadingMode_Off,
    NvCamShadingMode_Fast,
    NvCamShadingMode_HighQuality,
    NvCamShadingMode_Force32 = 0x7FFFFFF
} NvCamShadingMode;


/**
 * @var NvCamFlashMode_Off Do not fire the flash for this capture.
 *
 * @var NvCamFlashMode_Single If FlashAvailable is true Fire flash for this capture
 *      based on FlashFiringPower, FlashFiringTime.
 *
 * @var NvCamFlashMode_Torch If FlashAvailable is true Flash continuously on, power set by
 *      FlashFiringPower
 */
typedef enum
{
    NvCamFlashMode_Off,
    NvCamFlashMode_Single,
    NvCamFlashMode_Torch,
    NvCamFlashMode_Force32 = 0x7FFFFFF
} NvCamFlashMode;


/**
 * @var NvCamFaceDetectMode_Off Face detection is off.
 *
 * @var NvCamFaceDetectMode_Simple Return rectangle and confidence only.
 *
 * @var NvCamFaceDetectMode_Full Return all face metadata.
 */
typedef enum
{
    NvCamFaceDetectMode_Off,
    NvCamFaceDetectMode_Simple,
    NvCamFaceDetectMode_Full,
    NvCamFaceDetectMode_Force32 = 0x7FFFFFF
} NvCamFaceDetectMode;


/**
 * Holds face detection landmark coordinates
 */
typedef struct
{
    NvPoint LeftEye;
    NvPoint RightEye;
    NvPoint Mouth;
} NvCamFaceLandmarks;

/**
 * @var NvCamMetadataMode_None No metadata should be produced on output, except for
 *      application-bound buffer data. If no application-bound streams exist, no frame
 *      should be placed in the output frame queue. If such streams exist, a frame should
 *      be placed on the output queue with null metadata but with the necessary output
 *      buffer information. Timestamp information should still be included with any output
 *      stream buffers.
 *
 * @var NvCamMetadataMode_Full All metadata should be produced. Statistics will only be
 *      produced if they are separately enabled.
 */
typedef enum
{
    NvCamMetadataMode_None,
    NvCamMetadataMode_Full,
    NvCamMetadataMode_Force32 = 0x7FFFFFF
} NvCamMetadataMode;


/**
 * @var NvCamRequestType_Capture Capture a new image from the imaging hardware, and
 *      process it according to the settings.
 *
 * @var NvCamRequestType_Reprocess Process previously captured data; the RequestInputStream
 *      parameter determines the source reprocessing stream.
 */
typedef enum
{
    NvCamRequestType_Capture,
    NvCamRequestType_Reprocess,
    NvCamRequestType_Force32 = 0x7FFFFFF
} NvCamRequestType;


#define NVCAMERAISP_MAX_REGIONS  8
/**
 * Holds camera 3A regions definition.
 */
typedef struct
{
    NvU32 numOfRegions;
    NvRect regions[NVCAMERAISP_MAX_REGIONS];
    NvF32 weights[NVCAMERAISP_MAX_REGIONS];
} NvCamRegions;

/**
 * This is a single precision floating point version
 * of NvIspMathMatrix.
 */
typedef struct _NvIspMathF32MatrixElem
{
    /* First row of 4x4 matrix */
    NvF32 m00, m01, m02, m03;
    /* Second row of 4x4 matrix */
    NvF32 m10, m11, m12, m13;
    /* Third row of 4x4 matrix */
    NvF32 m20, m21, m22, m23;
    /* Fourth row of 4x4 matrix */
    NvF32 m30, m31, m32, m33;
} NvIspMathF32MatrixElem;

/**
 * Holds a 4x4 matrix of Floats.
 */
typedef union _NvIspMathF32Matrix
{
    /* 2d array */
    NvF32 array[4][4];
    /* matrix */
    NvIspMathF32MatrixElem matrix;
} NvIspMathF32Matrix;

/**
 * Represents a rational number by using a numerator and denominator
 */
typedef struct
{
    NvS32 Numerator;
    NvS32 Denominator;
} NvCamRational;

/**
 * @var NvCamAeState_Inactive AE is off.
 *
 * @var NvCamAeState_Searching AE doesn't yet have a good set
 *      of control values for the current scene.
 *
 * @var NvCamAeState_Converged AE has a good set of
 *      control values for the current scene.
 *
 * @var NvCamAeState_Locked AE has benn locked aeMode=LOCKED
 *
 * @var NvCamAeState_FlashRequired AE has good set of control
 *      values,but flash needs to be fired for good
 *      quality still capture.
 *
 * @var NvCamAeState_Precapture AE has been asked to do a precapture
 *      sequence.
*/
typedef enum
{
    NvCamAeState_Inactive,
    NvCamAeState_Searching,
    NvCamAeState_Converged,
    NvCamAeState_Locked,
    NvCamAeState_FlashRequired,
    NvCamAeState_Precapture,
} NvCamAeState;

/**
 * Defines various AF state
*/
typedef enum
{
    NvCamAfState_Inactive,
    NvCamAfState_PassiveScan,
    NvCamAfState_PassiveFocused,
    NvCamAfState_ActiveScan,
    NvCamAfState_FocusLocked,
    NvCamAfState_NotFocusLocked
} NvCamAfState;

/**
 * Defines various AWB state
*/
typedef enum
{
    NvCamAwbState_Inactive,
    NvCamAwbState_Searching,
    NvCamAwbState_Converged,
    NvCamAwbState_Locked,
    NvCamAwbState_Timeout //can beremoved later.
} NvCamAwbState;

/**
 * Defines White Balance Gains
 * R, GR, GB, B gains in numerical order
*/
typedef struct
{
    NvF32 WbGains[4];
} NvCamWbGains;


/**
 * Triggers for the AwbMode when it is set to NvCamAwbMode_Manual
 * _AlgControlOff   : Algorithm control in manual mode is off
 * _ResetReinitAlgs : Reset and re-initialize the WB algorithms
 * _RestoreAlgs     : Restore the WB Algorithm
 *
*/
typedef enum
{
    NvCamWbManualMode_AlgControlOff,
    NvCamWbManualMode_ResetReinitAlgs,
    NvCamWbManualMode_RestoreAlgs,
} NvCamWbManualMode;


/**
 * Defines various Flash state
*/
typedef enum
{
    NvCamFlashState_Unavailable,
    NvCamFlashState_Charging,
    NvCamFlashState_Ready,
    NvCamFlashState_Fired
} NvCamFlashState;

/**
 * Common struct to pass list of enums to a client
 */
typedef struct
{
    NvU32 Property[DEFAULT_LIST_SIZE];
    NvU32 Size;
} NvCamPropertyList;


/**
 * Common struct to pass list of floats to a client
 */
typedef struct
{
    NvF32 Values[DEFAULT_LIST_SIZE];
    NvU32 Size;
} NvCamPropertyFloatList;

/**
 * Common struct to pass list of 64-bit ints to a client
 */
typedef struct
{
    NvU64 Values[DEFAULT_LIST_SIZE];
    NvU32 Size;
} NvCamPropertyU64List;

#define NVCAM_RANGE_MAKE(_range, _low, _high) {     \
    _range.low = _low;                              \
    _range.high = _high;                            \
}

#define NVCAM_SIZE_MAKE(_size, _left, _right, _top, _bottom) {  \
    _size.left = _left;                                         \
    _size.right = _right;                                       \
    _size.top = _top;                                           \
    _size.bottom = _bottom;                                     \
}

/**
 * Common struct to pass list of ranges to a client
 */
typedef struct
{
    NvCameraIspRangeF32 Range[DEFAULT_LIST_SIZE];
    NvU32 Size;
} NvCamPropertyRangeList;

/**
 * Common struct to pass list of sizes to a client
 */
typedef struct
{
    NvSize Dimensions[DEFAULT_LIST_SIZE];
    NvU32 Size;
} NvCamPropertySizeList;


/**
 * Common struct to pass list of points to a client
 */
typedef struct
{
    NvPointF32 Points[DEFAULT_LIST_SIZE];
    NvU32 Size;
} NvCamPropertyPointList;

/**
 * Struct to pass face landmarks to a client
 */
typedef struct
{
    NvCamFaceLandmarks Landmarks[DEFAULT_LIST_SIZE];
    NvU32 Size;
} NvCamPropertyFaceLandmarkList;

/**
 * Common struct to pass list of rects to a client
 */
typedef struct
{
    NvRect Rects[DEFAULT_LIST_SIZE];
    NvU32 Size;
} NvCamPropertyRectList;


/**
 * 2d array for geometric correction map
 */
typedef struct
{
    NvPointF32 CorrectionMap[3];
} NvCamPropertyCorrectionMap;


/**
 * Makernote data
 */
typedef struct
{
    NvU32 DataSize;
    void *pMakerNoteData;
}NvCamMakerNoteData;

/**
 * Reference illuminant value for sensor
 */
typedef enum
{
    NvCamRefIlluminant_Daylight = 1,
    NvCamRefIlluminant_Fluorescent,
    NvCamRefIlluminant_Tungsten,
    NvCamRefIlluminant_Flash,
    NvCamRefIlluminant_FineWeather,
    NvCamRefIlluminant_CloudyWeather,
    NvCamRefIlluminant_Shade,
    NvCamRefIlluminant_DaylightFluorescent, // - D, 5700 - 7100K
    NvCamRefIlluminant_DayWhiteFluorescent, // - N, 4600 - 5400K
    NvCamRefIlluminant_CoolWhiteFluorescent, // - W, 3900 - 4500K
    NvCamRefIlluminant_WhiteFluorescent, // WW, 3200 - 3700K
    NvCamRefIlluminant_StandardA,
    NvCamRefIlluminant_StandardB,
    NvCamRefIlluminant_StandardC,
    NvCamRefIlluminant_D55,
    NvCamRefIlluminant_D65,
    NvCamRefIlluminant_D75,
    NvCamRefIlluminant_D50,
    NvCamRefIlluminant_ISOStudioTungsten
} NvCamRefIlluminant;

typedef struct
{
    NvS32   positionPhysicalLow;
    NvS32   positionPhysicalHigh;
    NvS32   positionInf;
    NvS32   positionInfOffset;
    NvS32   positionMacro;
    NvS32   positionMacroOffset;
    NvS32   settleTime;
} NvCamConfigFocuserInfo;

/**
 * Fuse Id information from the ODM
 */

#define NVCAMERA_FUSE_ID_SIZE  16

typedef struct
{
    NvU32   size;
    NvU8    data[NVCAMERA_FUSE_ID_SIZE];
} NvCamFuseId;


/**
 * AoHdr enable
 */
typedef enum
{
    NvCamAoHdrMode_Off,
    NvCamAoHdrMode_RowInterleaved,
    NvCamAoHdrMode_Preprocessed,
} NvCamAoHdrMode;



/**
 * Defines the control properties associated with
 * the camera. These properties map to the controls
 * defined by Android camera framework v3. This may
 * also include public controls exposed by NvCamera
 * which currently do not exist in the android framework.
 * Use these properties to control the settings of ISP
 * and the camera.
 *
 * @see system/media/camera/docs/docs.html
 */
typedef struct NvCamProperty_Public_Controls
{

    // Holds All android camera framework specific
    // controls as well Nv defined camera controls.

    // maps to android.control.aeLock
    NvBool AeLock;

    // maps to android.control.aeMode
    NvCamAeMode AeMode;

    // android.control.aeTargetFpsRange
    NvCameraIspRangeF32 AeTargetFpsRange;

    // maps to android.control.captureIntent
    NvCameraCoreUseCase CaptureIntent;

    // maps to android.sensor.sensitivity
    // In ISO arithmetic units.
    NvU32 SensorSensitivity;

    // maps to android.sensor.exposureTime
    // Value in seconds.
    NvF32 SensorExposureTime;

    // Custom property: Sensor analog gain
    // This is used for controlling all 4 bayer sensor channels.
    // Value should be within SensorAnalogGainRange limits
    NvF32   SensorAnalogGain;

    // maps to android.control.aeAntibandingMode
    NvCamAeAntibandingMode AeAntibandingMode;

    // maps to android.control.aeExposureCompensation
    // after conversion into android specific data type.
    NvF32 AeExposureCompensation;

    // maps to android.control.aeRegions
    NvCamRegions AeRegions;

    // maps to android.control.aePrecaptureTrigger
    NvCamAePrecaptureTrigger AePrecaptureTrigger;

    // maps to android.control.afMode
    NvCamAfMode AfMode;

    // maps to android.control.afRegions
    NvCamRegions AfRegions;

    // maps to android.control.afTrigger
    NvCamAfTrigger AfTrigger;

    // maps to android.control.afTriggerId
    NvS32 AfTriggerId;

    // maps to android.control.awbLock
    NvBool AwbLock;

    // maps to android.control.awbMode
    NvCamAwbMode AwbMode;

    // maps to android.control.awbRegions
    NvCamRegions AwbRegions;

    // Custom property: Does not map to any android property
    NvCamWbManualMode WbManualMode;

    // maps to android.colorCorrection.gains
    NvCamWbGains WbGains;

    // android.request.frameCount
    // A frame counter set by the framework. Must be maintained
    // unchanged in output frame
    NvU32 RequestFrameCount;

    // maps to android.request.id
    // An application-specified ID for the current request.
    // Must be maintained unchanged in output frame
    NvU32 RequestId;

    // maps to android.request.inputStreams
    // List which camera reprocess stream is used for the source
    // of reprocessing data.
    NvU32 RequestInputStreams;

    // maps to android.request.metadataMode
    NvCamMetadataMode RequestMetadataMode;

    // maps to android.request.outputStreams
    // Lists which camera output streams image data from this capture
    // must be sent to.
    NvU32 RequestOutputStreams;

    // maps to android.request.type
    NvCamRequestType RequestType;

    // Pull these properties out as they are implemented.
    // ######################### NOT IMPLEMENTED ################################

    // maps to android.colorCorrection.mode
    NvCamColorCorrectionMode ColorCorrectionMode;

    // maps to android.colorCorrection.transform
    NvIspMathF32Matrix ColorCorrectionTransform;

    // maps to android.control.effectMode
    NvCamColorEffectsMode ColorEffectsMode;

    // maps to android.control.mode
    NvCamControlMode ControlMode;

    // maps to android.control.sceneMode
    NvCamSceneMode SceneMode;

    // maps to android.control.videoStabilizationMode
    NvBool VideoStabalizationMode;

    // maps to android.demosaic.mode
    NvCamDemosaicMode DemosaicMode;

    // maps to android.edge.mode
    NvCamEdgeEnhanceMode EdgeEnhanceMode;

    // maps to android.edge.strength. Range: [1,10]
    // Valid range in core - [0,1]
    NvF32 EdgeEnhanceStrength;

    // maps to android.flash.firingPower. Range [1,10]
    NvF32 FlashFiringPower;

    // maps to android.flash.firingTime.
    // Firing time(in seconds) is relative to start of the exposure.
    NvF32 FlashFiringTime;

    // maps to android.flash.mode
    NvCamFlashMode FlashMode;

    // maps to android.geometric.mode
    NvCamGeometricMode  GeometricMode;

    // maps to android.geometric.strength. Range [1,10]
    NvF32 GeometricStrength;

    // maps to android.hotPixel.mode
    NvCamHotPixelMode HotPixelMode;

    // TODO: Define the JPEG controls later.
    // android.jpeg.gpsCoordinates (controls)
    // android.jpeg.gpsProcessingMethod (controls)
    // android.jpeg.gpsTimestamp (controls)
    // android.jpeg.orientation (controls)
    // android.jpeg.quality (controls)
    // android.jpeg.thumbnailQuality (controls)
    // android.jpeg.thumbnailSize (controls)

    // maps to android.lens.aperture
    // Size of the lens aperture.
    NvF32 LensAperture;

    // maps to android.lens.filterDensity
    NvF32 LensFilterDensity;

    // maps to android.lens.focalLength
    NvF32 LensFocalLength;

    // maps to android.lens.focusDistance
    NvF32 LensFocusDistance;

    // Valid only in NvCamAfMode_Manual
    NvS32  LensFocusPos;

    // maps to android.lens.opticalStabilizationMode
    NvBool LensOpticalStabalizationMode;

    // maps to android.noiseReduction.mode
    NvCamNoiseReductionMode NoiseReductionMode;

    // maps to android.noiseReduction.strength. Range[1,10]
    NvF32 NoiseReductionStrength;

    // android.scaler.cropRegion
    NvRect ScalerCropRegion;

    // maps to android.shading.mode
    NvCamShadingMode ShadingMode;

    // maps to android.shading.strength. Range [1,10]
    NvF32 ShadingStrength;

    // maps to android.statistics.faceDetectMode
    NvCamFaceDetectMode StatsFaceDetectMode;

    // maps to android.statistics.histogramMode
    NvBool StatsHistogramMode;

    // maps to android.statistics.sharpnessMapMode
    NvBool StatsSharpnessMode;

    // TODO: Define tonemap data items later. Needs some discussion.
    // android.tonemap.curveBlue
    // android.tonemap.curveGreen (controls)
    // android.tonemap.curveRed (controls)
    // android.tonemap.mode (controls)

    // maps to android.led.transmit
    // This LED is nominally used to indicate to the user that the camera
    // is powered on and may be streaming images back to the Application Processor.
    // In certain rare circumstances, the OS may disable this when video is processed
    // locally and not transmitted to any untrusted applications.In particular, the LED
    // *must* always be on when the data could be transmitted off the device. The LED
    // *should* always be on whenever data is stored locally on the device. The LED *may*
    // be off if a trusted application is using the data that doesn't violate the above rules.
    NvBool LedTransmit;

    // Range of exposure time in AE
    NvCameraIspRangeF32 ExposureTimeRange;

    // This contains range of combination of these:
    // ISO gain, Analog gain, ISP digital gain
    NvCameraIspRangeF32 GainRange;

    // Used by face detection
    NvU32 DeviceOrientation;

    // Custom property
    NvCamAoHdrMode AohdrMode;

    // Valid range from 0f to 2f
    NvF32 SaturationBias;

    // Valid range from 0f to 1f
    NvF32 ContrastBias;

    // Custom property
    NvBool highFpsRecording;

}NvCamProperty_Public_Controls;

/**
 * Defines the dynamic properties associated with
 * the camera. These properties map to the dynamic
 * properties defined by Android camera framework v3.
 * This also include public properties exposed by
 * NvCamera which may not exist in the android
 * camera framework.
 *
 * @see system/media/camera/docs/docs.html
 */
typedef struct NvCamProperty_Public_Dynamic
{

    // maps to android.colorCorrection.mode
    NvCamColorCorrectionMode ColorCorrectionMode;

    struct
    {
        // Flag to indicate if all the settings in the
        // struct have been updated for current frame.
        NvBool PartialResultUpdated;

        // maps to android.control.aeRegions
        NvCamRegions AeRegions;

        // maps to android.control.aeState
        NvCamAeState AeState;

        // maps to android.sensor.exposureTime
        // Value in seconds.
        NvF32 SensorExposureTime;

        // maps to android.sensor.sensitivity
        // In ISO arithmetic units.
        NvU32 SensorSensitivity;

        // Custom property: Sensor analog gain
        // Value should be within SensorAnalogGainRange limits
        NvF32 SensorAnalogGain;

        // Custom property: Sensor Analog gain range
        NvCameraIspRangeF32 SensorAnalogGainRange;

        // This contains range of combination of these:
        // ISO gain, Analog gain, ISP digital gain
        NvCameraIspRangeF32 IsoRange;

    } Ae;

    struct
    {
        // Flag to indicate if all the settings in the
        // struct have been updated for current frame.
        NvBool PartialResultUpdated;

        // maps to android.control.awbMode
        NvCamAwbMode AwbMode;

        // maps to android.control.awbRegions
        NvCamRegions AwbRegions;

        // maps to android.control.awbState
        NvCamAwbState AwbState;

        // Custom property: Does not map to any android property
        NvCamWbManualMode WbManualMode;

        // maps to android.colorCorrection.gains
        NvCamWbGains WbGains;

        // Will map to a cct related vendor tag
        NvU32 AwbCCT;

    } Awb;

    struct
    {
        // Flag to indicate if all the settings in the
        // struct have been updated for current frame.
        NvBool PartialResultUpdated;

        // maps to android.control.afMode
        NvCamAfMode AfMode;

        // maps to android.control.afRegions
        NvCamRegions AfRegions;

        // maps to android.control.afstate
        NvCamAfState AfState;

        // maps to android.control.afTriggerId
        NvS32 AfTriggerId;

    } Af;

    // maps to android.control.mode
    NvCamControlMode ControlMode;

    // maps to android.edge.mode
    NvCamEdgeEnhanceMode EdgeEnhanceMode;

    // maps to android.flash.firingPower. Range [1,10]
    NvF32 FlashFiringPower;

    // maps to android.flash.firingTime.
    // Firing time(in seconds) is relative to start of the exposure.
    NvF32 FlashFiringTime;

    // maps to android.flash.mode
    NvCamFlashMode FlashMode;

    // maps to android.flash.state
    NvCamFlashState FlashState;

    // maps to android.hotPixel.mode
    NvCamHotPixelMode HotPixelMode;

    // TODO: Define the JPEG controls later.
    // android.jpeg.gpsCoordinates (controls)
    // android.jpeg.gpsProcessingMethod (controls)
    // android.jpeg.gpsTimestamp (controls)
    // android.jpeg.orientation (controls)
    // android.jpeg.quality (controls)
    // android.jpeg.thumbnailQuality (controls)
    // android.jpeg.thumbnailSize (controls)

    // maps to android.lens.aperture
    // Size of the lens aperture.
    NvF32 LensAperture;

    // maps to android.lens.filterDensity
    NvF32 LensFilterDensity;

    // maps to android.lens.focalLength
    NvF32 LensFocalLength;

    // maps to android.lens.focusDistance
    NvF32 LensFocusDistance;

    // Valid only in NvCamAfMode_Manual
    NvS32  LensFocusPos;

    // maps to android.lens.opticalStabilizationMode
    NvBool LensOpticalStabalizationMode;

    // maps to android.lens.focusrange
    NvCameraIspRangeF32 LensFocusRange;

    // maps to android.lens.state
    NvU32 LensState;

    // maps to android.noiseReduction.mode
    NvCamNoiseReductionMode NoiseReductionMode;

    // android.request.frameCount
    // A frame counter set by the framework. Must be maintained
    // unchanged in output frame
    NvU32 RequestFrameCount;

    // maps to android.request.id
    // An application-specified ID for the current request.
    // Must be maintained unchanged in output frame
    NvU32 RequestId;

    // maps to android.request.metadataMode
    NvCamMetadataMode RequestMetadataMode;

    // maps to android.request.outputStreams
    // Lists which camera output streams image data from this capture
    // must be sent to.
    NvU32 RequestOutputStreams;

    // android.scaler.cropRegion
    NvRect ScalerCropRegion;

    // maps to android.sensor.timestamp
    NvU64 SensorTimestamp;

    // WAR: vstab requires timestamp from gettimeofday()  Bug 1513785
    NvU64 SensorTimeOfDay;

    // maps to android.shading.mode
    NvCamShadingMode ShadingMode;

    // maps to android.statistics.faceDetectMode
    NvCamFaceDetectMode StatsFaceDetectMode;

    // maps to android.statistics.histogramMode
    NvBool StatsHistogramMode;

    // maps to android.statistics.sharpnessMapMode
    NvBool StatsSharpnessMode;

    // maps to android.statistics.faceIds
    NvCamPropertyList FaceIds;

    // maps to android.statistics.faceLandmarks
    NvCamPropertyFaceLandmarkList FaceLandmarks;

    // maps to android.statistics.faceRectangles
    NvCamPropertyRectList FaceRectangles;

    // maps to android.statistics.faceScores
    NvCamPropertyList FaceScores;

    // TODO:
    // android.statistics.histogram

    // android.statistics.sharpnessMap
    NvS32 StatsSharpnessMap[MAX_FM_WIN_X * MAX_FM_WIN_Y * 3];

    // TODO: Define tonemap data items later. Needs some discussion.
    // android.tonemap.curveBlue
    // android.tonemap.curveGreen (controls)
    // android.tonemap.curveRed (controls)
    // android.tonemap.mode (controls)

    // maps to android.led.transmit
    // This LED is nominally used to indicate to the user that the camera
    // is powered on and may be streaming images back to the Application Processor.
    // In certain rare circumstances, the OS may disable this when video is processed
    // locally and not transmitted to any untrusted applications.In particular, the LED
    // *must* always be on when the data could be transmitted off the device. The LED
    // *should* always be on whenever data is stored locally on the device. The LED *may*
    // be off if a trusted application is using the data that doesn't violate the above rules.
    NvBool LedTransmit;

    // Filled for a reprocessing request when JPEG encoding is requested.
    NvMMEXIFInfo ExifInfo;

    // MakerNote data. Not encrypted for now.
    // TODO: Convert to encrypted data type and
    // do jpeg encoding as a pnode in camera core.
    char MakerNoteData[MAKERNOTE_DATA_SIZE];

    // This contains range of combination of these:
    // ISO gain, Analog gain, ISP digital gain
    NvCameraIspRangeF32 GainRange;

    // Custom property
    NvCamAoHdrMode AohdrMode;

    // Parameters for saving in DNG format
    NvF32 RawCaptureCCT;
    NvF32 RawCaptureWB[4];
    NvF32 RawCaptureGain;
    NvF32 RawCaptureCCM3000[16];
    NvF32 RawCaptureCCM6500[16];
}NvCamProperty_Public_Dynamic;


/**
 * Defines the static properties associated with
 * the camera. These properties map to the static
 * properties defined by Android camera framework v3.
 * This also includes static properties exposed by
 * NvCamera which may not not exist in the android
 * camera framework.
 *
 * @see system/media/camera/docs/docs.html
 */
typedef struct NvCamProperty_Public_Static
{

    // Holds all android camera framework specific
    // static properties as well Nv defined camera
    // static properties.

    // Indicates if core supports sending more than 1
    // results (including partial) per frame.
    // 1 count shows partial result feature is unsupported
    // Update SUPPORTED_PARTIAL_RESULT_COUNT when adding
    // more clients which support sending partial results.
    NvU32 NoOfResultsPerFrame;

    // maps to android.control.aeAvailableAntibandingModes
    NvCamPropertyList  AeAvailableAntibandingModes;

    // maps to android.control.aeAvailableModes
    NvCamPropertyList AeAvailableModes;

    // maps to android.control.aeAvailableTargetFpsRanges
    NvCamPropertyRangeList AeAvailableTargetFpsRanges;

    // maps to android.control.afAvailableModes
    // List of AF modes that can be selected
    NvCamPropertyList AfAvailableModes;

    // maps to android.control.availableEffects
    // subset of the full color effect enum list
    NvCamPropertyList AvailableEffects;

    // maps to android.control.availableSceneModes
    // subset of the scene mode enum list
    NvCamPropertyList AvailableSceneModes;

    // maps to android.control.availableVideoStabilizationModes
    // List of video stabilization modes that can be supported
    NvCamPropertyList AvailableVideoStabilizationModes;

    // maps to android.control.awbAvailableModes
    NvCamPropertyList AwbAvailableModes;

    // maps to android.control.maxRegions
    // Max number of regions that can be listed for metering.
    NvU32 MaxRegions;

    // maps to android.flash.info.available
    // Can camera support Flash
    NvBool FlashAvailable;

    // maps to android.flash.info.chargeDuration
    // Time taken before flash can fire again in nanoseconds.
    NvU64 FlashChargeDuration;

    // maps to android.flash.colorTemperature
    // x,y white point of flash
    NvPointF32 FlashColorTemperature;

    // maps to android.flash.maxEnergy
    // Max energy output of the flash
    NvU32 FlashMaxEnergy;

    // maps to android.hotPixel.info.map
    // List of co-ordinates of bad pixel in the sensor
    // TODO: Revisit later. Number of entires may need to
    // be increased or a better data structure needed.
    NvCamPropertyPointList HotPixelInfoMap;

    // maps to android.lens.info.availableApertures
    // List of supported aperture values
    NvCamPropertyFloatList LensAvailableApertures;

    // maps to android.lens.info.availableFilterDensities
    // List of supported ND filter values
    NvCamPropertyFloatList LensAvailableFilterDensities;

    // maps to android.lens.info.availableFocalLengths
    // Availble focal length of HW
    NvCamPropertyFloatList LensAvailableFocalLengths;

    // maps to android.lens.info.availableOpticalStabilization
    // List of supported optical image stablization modes
    NvCamPropertyList LensAvailableOpticalStabilization;

    // TODO: Define later.
    // maps to android.lens.info.geometricCorrectionMap
    // Map for correction of Geometric distortions/color channel
    //  LensGeometricCorrectionMap;

    // maps to android.lens.info.geometricCorrectionMapSize
    // Dimensions of geometric map
    // NvSize LensGeometricCorrectionMapSize;

    // maps to android.lens.info.hyperfocalDistance
    // Hyperfocaldistance of lens
    NvF32 LensHyperfocalDistance;

    // maps to android.lens.info.minimumFocusDistance
    // Shortest Focus distance possible
    NvF32 LensMinimumFocusDistance;

    // maps to android.lens.info.shadingMap
    // TODO: Revisit this later. Needs more discussion.
    // Low res lens shading map
    // NvF32 LensShadingMap[3][DEFAULT_LIST_SIZE][DEFAULT_LIST_SIZE];

    // TODO: Revisit later.
    // maps to android.lens.info.shadingMapSize
    // Dimensions of Lens shading map
    // NvSize ShadingMapSize;

    // maps to android.lens.facing
    // Direction of camera relative to device screen
    NvBool LensFacing;

    // maps to android.lens.opticalAxisAngle
    // Relative angle of camera optical axis to the
    // perpendicular axis from the display. First
    // defines the angle of separation between the
    // perpendicular to the screen and the camera optical axis.
    // The second then defines the clockwise rotation of the
    // optical axis from native device up.
    NvPointF32 LensOpticalAxisAngle;

    // maps to android.lens.position
    // Coordinates of camera optical axis on device.
    NvF32 LensPosition[3];

    // maps to android.request.maxNumOutputStreams
    // O/p stream per type of stream
    // How many output streams can be allocated at the same
    // time for each type of stream
    NvU32 RequestMaxNumOutputStreams[3];

    // maps to android.request.maxNumReprocessStreams
    // How many reprocessing streams of any type
    // can be allocated at the same time
    NvU32 RequestMaxNumReprocessStreams;

    // maps to android.scaler.availableProcessedMinDurations
    // Min frame duration for each resolution in nano seconds.
    NvCamPropertyU64List ScalerAvailableProcessedMinDurations;

    // maps to android.scaler.availableProcessedSizes
    // resolutions available for use with processed output stream
    NvCamPropertySizeList ScalerAvailableProcessedSizes;

    // maps to android.scaler.availableRawMinDurations
    // minimum frame duration that is supported for each raw resolution
    NvCamPropertyU64List ScalerAvailableRawMinDurations;

    // maps to android.scaler.availableRawSizes
    // The resolutions available for use with
    // raw sensor output streams
    NvCamPropertySizeList ScalerAvailableRawSizes;

    NvMMCameraSensorMode SensorModeList[MAX_NUM_SENSOR_MODES];

    // maps to android.sensor.info.activeArraySize
    // Area of raw data which corresponds to only active pixels
    NvRect SensorActiveArraySize;

    // maps to android.sensor.info.availableSensitivities
    NvCamPropertyList SensorAvailableSensitivities;

    // maps to android.sensor.info.colorFilterArrangement
    // Arrangement of color filters on sensor
    NvCamPropertyList SensorColorFilterArrangement;

    // maps to android.sensor.info.exposureTimeRange
    // Range of valid exposure times in nanoseconds
    NvU64 SensorExposureTimeRange[2];

    // maps to android.sensor.info.maxFrameDuration
    // Maximum frame duration
    NvU64 SensorMaxFrameDuration;

    // maps to android.sensor.info.physicalSize
    // The physical dimensions of the full pixel array in
    // millimeters.
    NvPointF32 SensorPhysicalSize;

    // maps to android.sensor.info.pixelArraySize
    // Dimensions of full pixel array
    NvSize SensorPixelArraySize;

    // maps to android.sensor.info.whiteLevel
    // Maximum raw value output by sensor
    NvU32 SensorWhiteLevel;

    // maps to android.sensor.baseGainFactor
    // Gain factor from electrons to raw units
    NvF32 SensorBaseGainFactor;

    // maps to android.sensor.blackLevelPattern
    // A fixed black level offset for each of the Bayer mosaic channels
    NvU32 BlackLevelPattern[2][2];

    // maps to android.sensor.calibrationTransform1
    // Per-device calibration on
    // top of color space transform1
    NvIspMathF32Matrix SensorCalibrationTransform1;

    // maps to android.sensor.calibrationTransform2
    // Per-device calibration on
    // top of color space transform2
    NvIspMathF32Matrix SensorCalibrationTransform2;

    // maps to android.sensor.colorTransform1/2
    // Linear mapping from XYZ (D50) color space
    // to reference linear sensor color, for first
    // reference illuminant
    NvIspMathF32Matrix SensorColorTransform1;

    NvIspMathF32Matrix SensorColorTransform2;

    // maps to android.sensor.forwardMatrix1/2
    // Used by DNG for better WB adaptation
    NvIspMathF32Matrix SensorForwardMatrix1;

    NvIspMathF32Matrix SensorForwardMatrix2;

    // maps to android.sensor.maxAnalogSensitivity
    // Maximum sensitivity that is implemented purely through analog gain
    NvU32 SensorMaxAnalogSensitivity;

    // maps to android.sensor.noiseModelCoefficients
    // Estimation of sensor noise characteristics
    NvPointF32 SensorNoiseModelCoefficients;

    // maps to android.sensor.orientation
    // Clockwise angle through which the output
    // image needs to be rotated to be upright
    // on the device screen in its native orientation.
    NvU32 SensorOrientation;

    // maps to android.sensor.referenceIlluminant1/2
    // Light source used to define transform 1
    NvCamRefIlluminant SensorReferenceIlluminant1;


    NvCamRefIlluminant ReferenceIlluminant2;

    // maps to android.statistics.info.availableFaceDetectModes
    // Which face detection modes are available
    NvCamPropertyList StatsAvailableFaceDetectModes;

    // maps to android.statistics.info.histogramBucketCount
    // Number of histogram buckets supported
    NvU32 StatsHistogramBucketCount;

    // maps to android.statistics.info.maxFaceCount
    // Max faces can be detected
    NvU32 StatsMaxFaceCount;

    // maps to android.statistics.info.maxHistogramCount
    // Maximum value possible for a histogram bucket
    NvU32 StatsMaxHistogramCount;

    // maps to android.statistics.info.maxSharpnessMapValue
    // Maximum value possible for a sharpness map region.
    NvU32 StatsMaxSharpnessMapValue;

    // maps to android.statistics.info.sharpnessMapSize
    // Dimensions of the sharpness map
    NvSize StatsSharpnessMapSize;

    // maps to android.tonemap.maxCurvePoints
    // Maximum number of supported points in the tonemap curve
    NvU32 TonemapMaxCurvePoints;

    // maps to android.led.availableLeds
    // A list of camera LEDs that are available on this system.
    NvCamPropertyList AvailableLeds;

    // maps to android.info.supportedHardwareLevel
    NvCamSupportedHardwareLevel SupportedHardwareLevel;

    // Custom properties
    // Does not map to any android structures
    NvCamConfigFocuserInfo FocuserInfo;

    // Custom property
    NvCamFuseId FuseId;

}NvCamProperty_Public_Static;


#ifdef __cplusplus
}
#endif
#endif // INCLUDED_NVCAM_PROPERTIES_PUBLIC_H

