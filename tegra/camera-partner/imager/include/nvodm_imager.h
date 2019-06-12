/*
 * Copyright (c) 2006-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */
/**
 * @file
 * <b>NVIDIA Tegra ODM Adaptation:
 *         Imager Interface</b>
 *
 * @b Description: Defines the ODM interface for NVIDIA imager adaptation.
 *
 */

#ifndef INCLUDED_NVODM_IMAGER_H
#define INCLUDED_NVODM_IMAGER_H
#if defined(__cplusplus)
extern "C"
{
#endif
#include "nvos.h"
#include "nvcommon.h"
#include "nvcamera_isp.h"
#include "tegra_camera.h"
#include "math.h"
/**
 * @defgroup nvodm_imager Imager Adaptation Interface
 *
 * This API handles the abstraction of physical camera imagers, allowing
 * programming the imager whenever resolution changes occur (for ISP
 * programming), like exposure, gain, frame rate, and power level changes.
 * Other camera devices, like flash modules, focusing mechanisms,
 * shutters, IR focusing devices, and so on, could be programmed via this
 * interface as well.
 *
 * Each imager is required to implement several of the imager adaptation
 * functions.
 *
 * Open and close routines are needed for the imager ODM context,
 * as well as set mode, power level, parameter, and other routines
 * for each imager.
 *
 * @ingroup nvodm_adaptation
 * @{
 */

/**
 * Defines the SOC imager white balance parameter.
 */
typedef enum {
   NvOdmImagerWhiteBalance_Auto = 0,
   NvOdmImagerWhiteBalance_Incandescent,
   NvOdmImagerWhiteBalance_Fluorescent,
   NvOdmImagerWhiteBalance_WarmFluorescent,
   NvOdmImagerWhiteBalance_Daylight,
   NvOdmImagerWhiteBalance_CloudyDaylight,
   NvOdmImagerWhiteBalance_Shade,
   NvOdmImagerWhiteBalance_Twilight,
   NvOdmImagerWhiteBalance_Force32 = 0x7FFFFFFF,
} NvOdmImagerWhiteBalance;

/**
 * Defines imager control modes.
 */
typedef enum
{
    /// Specifies the application control of pipeline.
    /// All 3A routines are disabled, no other settings in
    /// ::NvOdmImagerYUVControlProperty and ::NvOdmImagerYUVDynamicProperty
    /// have any effect.
    NvOdmImagerControlMode_Off = 0x1,
    /// Specifies the settings for each individual 3A routine. Manual control
    /// of capture parameters is disabled. All controls in
    /// @c NvOdmImagerYUVControlProperty and @c NvOdmImagerYUVDynamicProperty
    /// besides @a sceneMode take effect.
    NvOdmImagerControlMode_Auto,
    /// Specifies to use the specific scene mode. Enabling this disables @a AeMode,
    /// @a AwbMode, and @a AfMode controls; the driver must ignore those settings while
    /// @c NvOdmImagerControlMode_UseSceneMode is active (except for @c FACE_PRIORITY
    /// scene mode). Other control entries are still active. This setting can
    /// only be used if @a availableSceneModes != @c UNSUPPORTED.
    NvOdmImagerControlMode_UseSceneMode,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOdmImagerControlMode_Force32 = 0x7FFFFFF
} NvOdmImagerControlMode;

/**
 * Defines the various supported scene modes.
 */
typedef enum
{
    NvOdmImagerSceneMode_FacePriority,
    NvOdmImagerSceneMode_Action,
    NvOdmImagerSceneMode_Portrait,
    NvOdmImagerSceneMode_Landscape,
    NvOdmImagerSceneMode_Night,
    NvOdmImagerSceneMode_NightPortrait,
    NvOdmImagerSceneMode_Theatre,
    NvOdmImagerSceneMode_Beach,
    NvOdmImagerSceneMode_Snow,
    NvOdmImagerSceneMode_Sunset,
    NvOdmImagerSceneMode_SteadyPhoto,
    NvOdmImagerSceneMode_Fireworks,
    NvOdmImagerSceneMode_Sports,
    NvOdmImagerSceneMode_Party,
    NvOdmImagerSceneMode_CandleLight,
    NvOdmImagerSceneMode_Barcode,
    NvOdmImagerSceneMode_Size,
    NvOdmImagerSceneMode_Unsupported,
    NvOdmImagerSceneMode_Force32 = 0x7FFFFFF
} NvOdmImagerSceneMode;

/**
 * Defines the demosaic mode.
 */
typedef enum
{
    /// Specifies minimal or no slowdown of frame rate compared to Bayer raw
    /// output.
    NvOdmImagerDemosaicMode_Fast = 0x1,
    /// Specifies that frame rate may be reduced by high quality.
    NvOdmImagerDemosaicMode_HighQuality,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOdmImagerDemosaicMode_Force32 = 0x7FFFFFF
} NvOdmImagerDemosaicMode;

/**
 * Defines the color correction modes.
 */
typedef enum
{
    /// Specifies To use the @c colorCorrection matrix to do
    /// color conversion.
    NvOdmImagerColorCorrectionMode_Matrix = 1,
    /// Specifies to not slow down frame rate relative to raw Bayer output.
    NvOdmImagerColorCorrectionMode_Fast,
    /// Specifies that frame rate may be reduced by high quality.
    NvOdmImagerColorCorrectionMode_HighQuality,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOdmImagerColorCorrectionMode_Force32 = 0x7FFFFFF
} NvOdmImagerColorCorrectionMode;

/**
 * Defines the various available color effects modes.
 */
typedef enum
{
    NvOdmImagerColorEffectsMode_Off = 0x1,
    NvOdmImagerColorEffectsMode_Mono,
    NvOdmImagerColorEffectsMode_Negative,
    NvOdmImagerColorEffectsMode_Solarize,
    NvOdmImagerColorEffectsMode_Sepia,
    NvOdmImagerColorEffectsMode_Posterize,
    NvOdmImagerColorEffectsMode_Aqua,
    NvOdmImagerColorEffectsMode_Force32 = 0x7FFFFFF
} NvOdmImagerColorEffectsMode;

/**
 * Defines the shading modes.
 */
typedef enum
{
    /// Specifies that no shading correction is applied.
    NvOdmImagerShadingMode_Off,
    /// Specifies to not slow down frame rate relative to raw Bayer output.
    NvOdmImagerShadingMode_Fast,
    /// Specifies that frame rate may be reduced by high quality.
    NvOdmImagerShadingMode_HighQuality,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOdmImagerShadingMode_Force32 = 0x7FFFFFF
} NvOdmImagerShadingMode;

/**
 * Defines the noise reduction modes.
 */
typedef enum
{
    /// Specifies that no noise reduction is applied.
    NvOdmImagerNoiseReductionMode_Off,
    /// Specifies to not slow down frame rate relative to raw Bayer output.
    NvOdmImagerNoiseReductionMode_Fast,
    /// Specifies that frame rate may be reduced by high quality.
    NvOdmImagerNoiseReductionMode_HighQuality,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOdmImagerNoiseReductionMode_Force32 = 0x7FFFFFF
} NvOdmImagerNoiseReductionMode;

/**
 * Defines imager AE modes.
 */
typedef enum
{
    /// Specifies auto exposure (AE) is disabled; @c SensorExposureTime and
    /// @c SensorSensitivity controls are used.
    NvOdmImagerAeMode_Off = 0x1,
    /// Specifies AE is active: no flash control.
    NvOdmImagerAeMode_On,
    /// Specifies if flash exists, AE is active: auto flash control;
    /// flash may be fired when precapture trigger is activated, and also for
    /// captures for which @a captureIntent = @c STILL_CAPTURE.
    NvOdmImagerAeMode_OnAutoFlash,
    /// Specifies if flash exists AE is active: auto flash control
    /// for precapture trigger and always flash when
    /// @a captureIntent = @c STILL_CAPTURE.
    NvOdmImagerAeMode_OnAlwaysFlash,
    /// Specifies optional automatic red eye reduction with flash. If
    /// deemed necessary, red eye reduction sequence should fire when precapture
    /// trigger is activated, and final flash should fire when
    /// @a captureIntent = @c STILL_CAPTURE.
    NvOdmImagerAeMode_On_AutoFlashRedEye,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOdmImagerAeMode_Force32 = 0x7FFFFFF
} NvOdmImagerAeMode;

#define NVCAMERAISP_MAX_REGIONS  8

/**
 * Holds camera 3A regions definition.
 */
typedef struct
{
    NvU32 numOfRegions;
    NvRect regions[NVCAMERAISP_MAX_REGIONS];
    NvF32 weights[NVCAMERAISP_MAX_REGIONS];
} NvOdmImagerRegions;

/**
 * Defines the SOC imager flicker detection/correction modes.
 */
typedef enum
{
    NvOdmImagerAeAntibandingMode_Off,
    NvOdmImagerAeAntibandingMode_50Hz,
    NvOdmImagerAeAntibandingMode_60Hz,
    NvOdmImagerAeAntibandingMode_AUTO,
    NvOdmImagerAeAntibandingMode_Force32 = 0x7FFFFFF
} NvOdmImagerAeAntibandingMode;

/**
 * Defines AE precapture triggers.
 */
typedef enum
{
    /// Specifies the trigger is idle.
    NvOdmImagerAePrecaptureTrigger_Idle = 0x1,
    /// Specifies the precapture metering sequence must be started.
    /// The exact effect of the precapture trigger depends on the current
    /// AE mode and state.
    NvOdmImagerAePrecaptureTrigger_Start,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOdmImagerAePrecaptureTrigger_Force32 = 0x7FFFFFF
} NvOdmImagerAePrecaptureTrigger;

/**
 * Defines the imager auto focus (AF) modes.
 */
typedef enum
{
    /// Specifies that 3A routines do not control the lens.
    /// Focus position is controlled by the application.
    NvOdmImagerAfMode_Off = 0x1,
    /// Specifies the lens is not fixed focus. Use
    /// NvOdmImagerStaticProperty::LensMinimumFocusDistance
    /// to determine if lens is fixed focus. In this mode, the lens does not
    /// move unless the AF trigger action is called. When that trigger is
    /// activated, AF must transition to @c ACTIVE_SCAN, then to the outcome
    /// of the scan (@c FOCUSED or @c NOT_FOCUSED). Triggering cancel AF resets
    /// the lens position to default, and sets the AF state to INACTIVE.
    NvOdmImagerAfMode_Auto,
    /// Specifies the lens does not move unless the AF trigger
    /// action is called. When that trigger is activated, AF must transition
    /// to @c ACTIVE_SCAN, then to the outcome of the scan (@c FOCUSED or
    /// @c NOT_FOCUSED). Triggering cancel AF resets the lens position to default
    /// and sets the AF state to @c INACTIVE.
    NvOdmImagerAfMode_Macro,
    /// Specifies the AF algorithm modifies the lens position continually
    /// to attempt to provide a constantly-in-focus image stream. The focusing
    /// behavior should be suitable for good quality video recording; typically
    /// this means slower focus movement and no overshoots. When the AF trigger
    /// is not involved, the AF algorithm should start in @c INACTIVE state, and
    /// then transition into @c PASSIVE_SCAN and @c PASSIVE_FOCUSED states as
    /// appropriate. When the AF trigger is activated, the algorithm should
    /// immediately transition into @c AF_FOCUSED or @c AF_NOT_FOCUSED as
    /// appropriate, and lock the lens position until a cancel AF trigger is
    /// received. Once cancel is received, the algorithm should transition back
    /// to @c INACTIVE and resume passive scan.
    /// @note This behavior is not identical
    /// to @c CONTINUOUS_PICTURE, because an ongoing @c PASSIVE_SCAN must
    /// immediately be canceled.
    NvOdmImagerAfMode_ContinuousVideo,
    /// Specifies the AF algorithm modifies the lens position continually to
    /// attempt to provide a constantly-in-focus image stream. The focusing
    /// behavior should be suitable for still image capture; typically this
    /// means focusing as fast as possible. When the AF trigger is not involved,
    /// the AF algorithm should start in @c INACTIVE state, and then transition
    /// into @c PASSIVE_SCAN and @c PASSIVE_FOCUSED states as appropriate while
    /// it attempts to maintain focus. When the AF trigger is activated, the
    /// algorithm should finish its @c PASSIVE_SCAN if active, and then
    /// transition into @c AF_FOCUSED or @c AF_NOT_FOCUSED as appropriate,
    /// and lock the lens position until a cancel AF trigger is received. When
    /// the AF cancel trigger is activated, the algorithm should transition back
    /// to I@c NACTIVE and then act as if it has just been started.
    NvOdmImagerAfMode_ContinuousPicture,
    /// Specifies extended depth of field (digital focus). AF trigger is ignored,
    /// AF state should always be @c INACTIVE.
    NvOdmImagerAfMode_ExtDepthOfField,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOdmImagerAfMode_Force32 = 0x7FFFFFF
} NvOdmImagerAfMode;

/**
 * Defines the auto focus (AF) trigger states.
 */
typedef enum
{
    /// Specifies the trigger is idle.
    NvOdmImagerAfTrigger_Idle = 0x1,
    /// Specifies AF must trigger now.
    NvOdmImagerAfTrigger_Start,
    /// Specifies a trigger is in progress.
    NvOdmImagerAfTrigger_InProgress,
    /// Specifies AF must return to initial state and cancel
    /// any active trigger.
    NvOdmImagerAfTrigger_Cancel,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOdmImagerAfTrigger_Force32 = 0x7FFFFFF
} NvOdmImagerAfTrigger;

/**
 * Defines the edge enhance modes.
 */
typedef enum
{
    /// Specifies no edge enhancement is applied.
    NvOdmImagerEdgeEnhanceMode_Off,
    /// Specifies not to slow down frame rate relative to raw Bayer output.
    NvOdmImagerEdgeEnhanceMode_Fast,
    /// Specifies that frame rate may be reduced by high quality.
    NvOdmImagerEdgeEnhanceMode_HighQuality,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOdmImagerEdgeEnhanceMode_Force32 = 0x7FFFFFF
} NvOdmImagerEdgeEnhanceMode;

/**
 * Defines geometric modes.
 */
typedef enum
{
    /// Specifies no geometric correction is applied.
    NvOdmImagerGeometricMode_Off,
    /// Specifies to not slow down frame rate relative to raw Bayer output.
    NvOdmImagerGeometricMode_Fast,
    /// Specifies that frame rate may be reduced by high quality.
    NvOdmImagerGeometricMode_HighQuality,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOdmImagerGeometricMode_Force32 = 0x7FFFFFF
} NvOdmImagerGeometricMode;

/**
 * Defines the hot pixel modes.
 */
typedef enum
{
    /// Specifies no hot pixel correction is applied.
    NvOdmImagerHotPixelMode_Off,
    /// Specifies to not slow down frame rate relative to raw Bayer output.
    NvOdmImagerHotPixelMode_Fast,
    /// Specifies that frame rate may be reduced by high quality.
    NvOdmImagerHotPixelMode_HighQuality,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOdmImagerHotPixelMode_Force32 = 0x7FFFFFF
} NvOdmImagerHotPixelMode;

/**
 * Defines the face detection modes.
 */
typedef enum
{
    /// Specifies face detection is off.
    NvOdmImagerFaceDetectMode_Off,
    /// Specifies to return rectangle and confidence only.
    NvOdmImagerFaceDetectMode_Simple,
    /// Specifies to return all face metadata.
    NvOdmImagerFaceDetectMode_Full,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOdmImagerFaceDetectMode_Force32 = 0x7FFFFFF
} NvOdmImagerFaceDetectMode;

/**
 * Defines the flash modes.
 */
typedef enum
{
    /// Specifies to not fire the flash for this capture.
    NvOdmImagerFlashMode_Off,
    /// Specifies if @a FlashAvailable is true to fire flash for this capture
    /// based on @a FlashFiringPower and @a FlashFiringTime.
    NvOdmImagerFlashMode_Single,
    /// Specifies if @a FlashAvailable is true to flash continuously on,
    /// power set by @a FlashFiringPower.
    NvOdmImagerFlashMode_Torch,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOdmImagerFlashMode_Force32 = 0x7FFFFFF
} NvOdmImagerFlashMode;

/**
 * Defines auto exposure (AE) states.
 */
typedef enum
{
    /// Specifies AE is off.
    NvOdmImagerAeState_Inactive,
    /// Specifies AE doesn't yet have a good set of control values for
    /// the current scene.
    NvOdmImagerAeState_Searching,
    /// Specifies AE has a good set of control values for the current scene.
    NvOdmImagerAeState_Converged,
    /// Specifies AE has been locked @a aeMode = @c LOCKED.
    NvOdmImagerAeState_Locked,
    /// Specifies AE has good set of control values, but flash must be
    /// fired for good quality still capture.
    NvOdmImagerAeState_FlashRequired,
    /// Specifies AE has been asked to do a precapture sequence.
    NvOdmImagerAeState_Precapture,
} NvOdmImagerAeState;

/**
 * Defines various auto focus (AF) states.
*/
typedef enum
{
    NvOdmImagerAfState_Inactive,
    NvOdmImagerAfState_PassiveScan,
    NvOdmImagerAfState_PassiveFocused,
    NvOdmImagerAfState_ActiveScan,
    NvOdmImagerAfState_FocusLocked,
    NvOdmImagerAfState_NotFocusLocked
} NvOdmImagerAfState;

/**
 * Defines various auto white balance (AWB) states.
*/
typedef enum
{
    NvOdmImagerAwbState_Inactive,
    NvOdmImagerAwbState_Searching,
    NvOdmImagerAwbState_Converged,
    NvOdmImagerAwbState_Locked,
    NvOdmImagerAwbState_Timeout //can beremoved later.
} NvOdmImagerAwbState;

/**
 * Defines various flash states.
*/
typedef enum
{
    NvOdmImagerFlashState_Unavailable,
    NvOdmImagerFlashState_Charging,
    NvOdmImagerFlashState_Ready,
    NvOdmImagerFlashState_Fired
} NvOdmImagerFlashState;

/**
 * Defines the SOC imager color effect parameter.
 */
typedef enum {
   NvOdmImagerEffect_None = 0,
   NvOdmImagerEffect_BW,
   NvOdmImagerEffect_Negative,
   NvOdmImagerEffect_Posterize,
   NvOdmImagerEffect_Sepia,
   NvOdmImagerEffect_Solarize,
   NvOdmImagerEffect_Aqua,
   NvOdmImagerEffect_Blackboard,
   NvOdmImagerEffect_Whiteboard,
   NvOdmImagerEffect_Force32 = 0x7FFFFFFF,
} NvOdmImagerEffect;

/**
 * Defines the imager sync edge.
 */
typedef enum {
   NvOdmImagerSyncEdge_Rising = 0,
   NvOdmImagerSyncEdge_Falling = 1,
   NvOdmImagerSyncEdge_Force32 = 0x7FFFFFFF,
} NvOdmImagerSyncEdge;

typedef enum {
   NvOdmImagerFrameRateScheme_Imager = 0,
   NvOdmImagerFrameRateScheme_Core,
} NvOdmImagerFrameRateScheme;

/**
 * Defines parallel timings for the sensor device.
 * For your typical parallel interface, the following timings
 * for the imager are sent to the driver, so it can program the
 * hardware accordingly.
 */
typedef struct
{
  /// Does horizontal sync start on the rising or falling edge.
  NvOdmImagerSyncEdge HSyncEdge;
  /// Does vertical sync start on the rising or falling edge.
  NvOdmImagerSyncEdge VSyncEdge;
  /// Use VGP0 for MCLK. If false, the VCLK pin is used.
  NvBool MClkOnVGP0;
} NvOdmImagerParallelSensorTiming;
/**
 * Defines MIPI timings. For a MIPI imager, the following timing
 * information is sent to the driver, so it can program the hardware
 * accordingly.
 */
typedef struct
{
#define NVODMSENSORMIPITIMINGS_PORT_CSI_A (0)
#define NVODMSENSORMIPITIMINGS_PORT_CSI_B (1)
    /// Specifies MIPI-CSI port type.
    /// @deprecated; do not use.
    NvU8 CsiPort;
#define NVODMSENSORMIPITIMINGS_DATALANES_ONE     (1)
#define NVODMSENSORMIPITIMINGS_DATALANES_TWO     (2)
#define NVODMSENSORMIPITIMINGS_DATALANES_THREE   (3)
#define NVODMSENSORMIPITIMINGS_DATALANES_FOUR    (4)
    /// Specifies number of data lanes used.
    NvU8 DataLanes;
#define NVODMSENSORMIPITIMINGS_VIRTUALCHANNELID_ONE   (0)
#define NVODMSENSORMIPITIMINGS_VIRTUALCHANNELID_TWO   (1)
#define NVODMSENSORMIPITIMINGS_VIRTUALCHANNELID_THREE (2)
#define NVODMSENSORMIPITIMINGS_VIRTUALCHANNELID_FOUR  (3)

    /// Specifies virtual channel identifier.
    NvU8 VirtualChannelID;
    /// For AP15/AP16:
    /// - Discontinuous clock mode is needed for single lane
    /// - Continuous clock mode is needed for dual lane
    ///
    /// For AP20:
    /// Either mode is supported for either number of lanes.
    NvBool DiscontinuousClockMode;
    /// Specifies a value between 0 and 15 to determine how many pixel
    /// clock cycles (viclk cycles) to wait before starting to look at
    /// the data.
    NvU8 CILThresholdSettle;
} NvOdmImagerMipiSensorTiming;

/**
 * Defines imager crop modes for NvOdmImagerSensorMode::CropMode.
 */
typedef enum
{
    NvOdmImagerNoCropMode = 1,
    NvOdmImagerPartialCropMode,
    NvOdmImagerCropModeForce32 = 0x7FFFFFFF,
} NvOdmImagerCropMode;

/**
* Defines sensor mode types for NvOdmImagerSensorMode::Type.
*/
typedef enum
{
    NvOdmImagerModeType_Normal = 0, //Normal mode (preview, still, or movie)
    NvOdmImagerModeType_Preview, //Dedicated preview mode
    NvOdmImagerModeType_Movie, //Dedicated movie mode
    NvOdmImagerModeTypeForce32 = 0x7FFFFFFF,
} NvOdmImagerModeType;

/**
 * Holds mode-specific sensor information.
 */
typedef struct NvOdmImagerSensorModeInfoRec
{
    /// Specifies the scan area (left, right, top, bottom)
    /// on the sensor array that the output stream covers.
    NvRect rect;
    /// Specifies x/y scales if the on-sensor binning/scaling happens.
    NvPointF32 scale;
} NvOdmImagerSensorModeInfo;

/**
 * Holds ODM imager modes for sensor device.
 */
typedef struct NvOdmImagerSensorModeRec
{
    /// Specifies the active scan region.
    NvSize ActiveDimensions;
    /// Specifies the active start.
    NvPoint ActiveStart;
    /// Specifies the peak frame rate.
    NvF32 PeakFrameRate;
    /// Specifies the:
    /// <pre> pixel_aspect_ratio = pixel_width / pixel_height </pre>
    /// So pixel_width = k and pixel_height = t means 1 correct pixel consists
    /// of 1/k sensor output pixels in x direction and 1/t sensor output pixels
    /// in y direction. One of pixel_width and pixel_height must be 1.0 and
    /// the other one must be less than or equal to 1.0. Can be 0 for 1.0.
    NvF32 PixelAspectRatio;
    /// Specifies the sensor PLL multiplier so that VI/ISP clock can be set
    /// accordingly. <pre> MCLK * PLL </pre> multipler will be calculated to
    /// set VI/ISP clock accordingly. If not specified, VI/ISP clock will set to
    /// the maximum.
    NvF32 PLL_Multiplier;

    /// Specifies the crop mode. This can be partial or full.
    /// In partial crop mode, a portion of sensor array is taken while
    /// configuring sensor mode. While in full mode, almost the entire sensor
    /// array is taken while programing the sensor mode. Thus, in crop mode,
    /// the final image looks cropped with respect to the full mode image.
    NvOdmImagerCropMode CropMode;

    /// Specifies mode-specific information.
    NvOdmImagerSensorModeInfo ModeInfo;

    /// Usage type of sensor mode
    NvOdmImagerModeType Type;

    /// Specifies the line length in pixel clocks. This includes the active area
    /// as well as any horizontal blanking.
    NvU32 LineLength;
} NvOdmImagerSensorMode;
/**
 * Holds a region, for use with
 * NvOdmImagerParameter_RegionUsedByCurrentResolution.
 */
typedef struct NvOdmImagerRegionRec
{
    /// Specifies the upper left corner of the region.
    NvPoint RegionStart;
    /// Specifies the horizontal/vertical downscaling used
    /// (eg., 2 for a 2x downscale, 1 for no downscaling).
    NvU32 xScale;
    NvU32 yScale;

} NvOdmImagerRegion;
/**
 * Defines camera imager types.
 * Each ODM imager implementation will support one imager type.
 * @note A system designer should be aware of the limitations of using
 * both an imager device and a bitstream device, because they may share pins.
 */
typedef enum
{
    /// Specifies standard VIP 8-pin port; used for RGB/YUV data.
    NvOdmImagerSensorInterface_Parallel8 = 1,
    /// Specifies standard VIP 10-pin port; used for Bayer data, because
    /// ISDB-T in serial mode is possible only if Parallel10 is not used.
    NvOdmImagerSensorInterface_Parallel10,
    /// Specifies MIPI-CSI, Port A.
    NvOdmImagerSensorInterface_SerialA,
    /// Specifies MIPI-CSI, Port B
    NvOdmImagerSensorInterface_SerialB,
    /// Specifies MIPI-CSI, Port C
    NvOdmImagerSensorInterface_SerialC,
    /// Specifies MIPI-CSI, Port A&B. This is special case for T30 only
    /// where both CSI A&B must be enabled to make use of 4 lanes.
    NvOdmImagerSensorInterface_SerialAB,
    /// Specifies CCIR protocol (implicitly Parallel8)
    NvOdmImagerSensorInterface_CCIR,
    /// Specifies not a physical imager.
    NvOdmImagerSensorInterface_Host,
    /// Temporary: CSI via host interface testing
    /// This serves no purpose in the final driver,
    /// but is needed until a platform exists that
    /// has a CSI sensor.
    NvOdmImagerSensorInterface_HostCsiA,
    NvOdmImagerSensorInterface_HostCsiB,
    NvOdmImagerSensorInterface_Num,
    NvOdmImagerSensorInterface_Force32 = 0x7FFFFFFF
} NvOdmImagerSensorInterface;

#define NVODMIMAGERPIXELTYPE_YUV   0x0010
#define NVODMIMAGERPIXELTYPE_BAYER 0x0100
#define NVODMIMAGERPIXELTYPE_RGB   0x1000
#define NVODMIMAGERPIXELTYPE_IS_YUV(_PT)   ((_PT) & NVODMIMAGERPIXELTYPE_YUV)
#define NVODMIMAGERPIXELTYPE_IS_BAYER(_PT) ((_PT) & NVODMIMAGERPIXELTYPE_BAYER)
#define NVODMIMAGERPIXELTYPE_IS_RGB(_PT)   ((_PT) & NVODMIMAGERPIXELTYPE_RGB)
/**
 * Defines the ODM imager pixel type.
 */
typedef enum {
    NvOdmImagerPixelType_Unspecified = 0,
    NvOdmImagerPixelType_UYVY = NVODMIMAGERPIXELTYPE_YUV,
    NvOdmImagerPixelType_VYUY,
    NvOdmImagerPixelType_YUYV,
    NvOdmImagerPixelType_YVYU,
    NvOdmImagerPixelType_BayerRGGB = NVODMIMAGERPIXELTYPE_BAYER,
    NvOdmImagerPixelType_BayerBGGR,
    NvOdmImagerPixelType_BayerGRBG,
    NvOdmImagerPixelType_BayerGBRG,
    NvOdmImagerPixelType_BayerRGGB8,
    NvOdmImagerPixelType_BayerBGGR8,
    NvOdmImagerPixelType_BayerGRBG8,
    NvOdmImagerPixelType_BayerGBRG8,
    NvOdmImagerPixelType_RGB888 = NVODMIMAGERPIXELTYPE_RGB,
    NvOdmImagerPixelType_A8B8G8R8,
    NvOdmImagerPixelType_Force32 = 0x7FFFFFFF
} NvOdmImagerPixelType;
/// Defines the ODM imager orientation.
/// The imager orientation is with respect to the display's
/// top line scan direction. A display could be tall or wide, but
/// what really counts is in which direction it scans the pixels
/// out of memory. The camera block must place the pixels
/// in memory for this to work correctly, and so the ODM
/// must provide info about the board placement of the imager.
typedef enum {
    NvOdmImagerOrientation_0_Degrees = 0,
    NvOdmImagerOrientation_90_Degrees,
    NvOdmImagerOrientation_180_Degrees,
    NvOdmImagerOrientation_270_Degrees,
    NvOdmImagerOrientation_Force32 = 0x7FFFFFFF
} NvOdmImagerOrientation;
/// Defines the ODM imager direction.
/// The direction this particular imager is pointing with respect
/// to the main display tells the camera driver whether this imager
/// is pointed toward or away from the user. Typically, the main
/// imaging device is pointed away, and if present a secondary imager,
/// if used for video conferencing, is pointed toward.
typedef enum {
    NvOdmImagerDirection_Away = 0,
    NvOdmImagerDirection_Toward,
    NvOdmImagerDirection_Force32 = 0x7FFFFFFF
} NvOdmImagerDirection;
/// Defines the structure used to describe the mclk and pclk
/// expectations of the sensor. The sensor likely has PLL's in
/// its design, which allows it to get a low input clock and then
/// provide a higher speed output clock to achieve the needed
/// bandwidth for frame data transfer. Ideally, the clocks will differ
/// so as to avoid noise on the bus, but early versions of
/// Tegra Application Processor required the clocks be the same.
/// The capabilities structure gives which profiles the NVODM
/// supports, and @a SetParameter of ::NvOdmImagerParameter_SensorInputClock
/// will be how the camera driver communicates which one was used.
/// The ::NvOdmImagerClockProfile structure gives an input clock,
/// 24 MHz, and a clock multiplier that the PLL will do to create
/// the output clock. For instance, if the output clock is 64 MHz, then
/// the clock multiplier is 64/24. Likely, the first item in the array
/// of profiles in the capabilities structure will be used. If an older
/// Tegra Application Processor is being used, then the list will be
/// searched for a multiplier of 1.0.
typedef struct {
    NvU32 ExternalClockKHz; /**< Specifies 'mclk' the clock going into sensor. */
    NvF32 ClockMultiplier; /**< Specifies 'pclk'/'mclk' ratio due to PLL programming. */
} NvOdmImagerClockProfile;

/** The maximum length of imager identifier string. */
#define NVODMIMAGER_IDENTIFIER_MAX 32
#define NVODMSENSOR_IDENTIFIER_MAX NVODMIMAGER_IDENTIFIER_MAX
/** The maximum length of imager format string. */
#define NVODMIMAGER_FORMAT_MAX 4
#define NVODMSENSOR_FORMAT_MAX NVODMIMAGER_FORMAT_MAX
/** The list of supported clock profiles maximum array size. */
#define NVODMIMAGER_CLOCK_PROFILE_MAX 2
/**
 * Holds the ODM imager capabilities.
 */
typedef struct {
    /// Specifies imager identifiers; an example of an identifier is:
    /// "Micron 3120" or "Omnivision 9640".
    char identifier[NVODMIMAGER_IDENTIFIER_MAX];
    /// Specifies imager interface types.
    NvOdmImagerSensorInterface SensorOdmInterface;
    /// Specifies the number of pixel types; this specifies the
    /// imager's ability to output several YUV formats, as well
    /// as Bayer.
    NvOdmImagerPixelType PixelTypes[NVODMIMAGER_FORMAT_MAX];
    /// Specifies orientation and direction.
    NvOdmImagerOrientation Orientation;
    NvOdmImagerDirection Direction;
    /// Specifies the clock frequency that the sensor
    /// requires for initialization and mode changes.
    /// Also can be used for times when performance
    /// is not important (self-test, calibration, bringup).
    NvU32 InitialSensorClockRateKHz;
    ///
    /// Specifies the clock profiles supported by the imager.
    /// For instance: {24, 2.66}, {64, 1.0}. The first is preferred
    /// but the second may be used for backward compatibility.
    /// Unused profiles must be zeroed out.
    NvOdmImagerClockProfile ClockProfiles[NVODMIMAGER_CLOCK_PROFILE_MAX];
    /// Specifies parallel and serial timing settings.
    NvOdmImagerParallelSensorTiming ParallelTiming;
    NvOdmImagerMipiSensorTiming MipiTiming;
    ///
    /// Proclaim the minimum blank time that this
    /// sensor will honor. The camera driver will check this
    /// against the underlying hardware's requirements.
    /// width = horizontal blank time in pixels.
    /// height = vertical blank time in lines.
    NvSize MinimumBlankTime;
    /// Specifies the preferred mode index.
    /// After calling ListModes, the camera driver will select
    /// the preferred mode via this index. The preferred
    /// mode may be the highest resolution for the optimal frame rate.
    NvS32 PreferredModeIndex;

    /// Focuser GUID
    /// If 0, no focusing mechanism is present.
    /// Otherwise, it is a globally unique identifier for the focuser
    /// so that a particular sensor can be associated with it.
    NvU64 FocuserGUID;
    /// Flash GUID, identifies the existence (non-zero) of a
    /// flash mechanism, and gives the globally unique identifier for
    /// it so that a particular sensor can be associated with its flash.
    NvU64 FlashGUID;
    /// Sentinel value.
    NvU32 CapabilitiesEnd;
    NvU8 FlashControlEnabled;
    NvU8 AdjustableFlashTiming;
    /// Flag to report all sensor modes listed are HDR interpolated
    /// Per-mode HDR enabling is not supported in NvOdmImager Sensor drivers
    /// For per-mode distinction of HDR enabled or not, see new PCL Sensor Driver
    /// format.
    NvU8 isHDRSensor;
} NvOdmImagerCapabilities;
#define NVODM_IMAGER_VERSION (2)
#define NVODM_IMAGER_CAPABILITIES_END ((0x3434 << 16) | NVODM_IMAGER_VERSION)
#define NVODM_IMAGER_GET_VERSION(CAPEND) ((CAPEND) & 0xFFFF)
#define NVODM_IMAGER_GET_CAPABILITIES(CAPEND) (((CAPEND) >> 16) & 0xFFFF)

typedef enum {
    NvOdmImagerCal_Undetermined,
    NvOdmImagerCal_NotApplicable,
    NvOdmImagerCal_NotCalibrated,
    NvOdmImagerCal_WrongSensor,
    NvOdmImagerCal_Override,
    NvOdmImagerCal_Calibrated,
    NvOdmImagerCal_Force32 = 0x7FFFFFFF
} NvOdmImagerCalibrationStatus;

typedef enum {
    NvOdmImagerTestPattern_None,
    NvOdmImagerTestPattern_Colorbars,
    NvOdmImagerTestPattern_Checkerboard,
    NvOdmImagerTestPattern_Walking1s,
} NvOdmImagerTestPattern;

typedef enum {
    NvOdmImagerSOCGamma_Enable,
    NvOdmImagerSOCGamma_Disable,
} NvOdmImagerSOCGamma;

typedef enum {
    NvOdmImagerSOCSharpening_Enable,
    NvOdmImagerSOCSharpening_Disable,
} NvOdmImagerSOCSharpening;

typedef enum {
    NvOdmImagerSOCLCC_Enable,
    NvOdmImagerSOCLCC_Disable,
} NvOdmImagerSOCLCC;

/**
 * Holds the ODM imager calibration data.
 * @sa NvOdmImagerParameter_CalibrationData and
 *      NvOdmImagerParameter_CalibrationOverrides
 */
typedef struct {
    /// The @c CalibrationData string may be a pointer to a statically compiled
    /// constant, or an allocated buffer that is filled by (for example)
    /// file I/O or decryption. If @c NeedsFreeing is NV_TRUE, the caller is
    /// responsible for freeing @c CalibrationData when it is no longer needed.
    /// If @c NeedsFreeing is NV_FALSE, the caller will simply discard the pointer
    /// when it is no longer needed.
    NvBool NeedsFreeing;
    /// Points to a null-terminated string containing the calibration data.
    const char *CalibrationData;
} NvOdmImagerCalibrationData;

typedef struct
{
    unsigned long HighLines;
    unsigned long LowLines;
    unsigned long ShortedLines;
} NvOdmImagerParallelDLICheck;

typedef struct
{
    unsigned long correct_pixels;
    unsigned long incorrect_pixels;
} NvOdmImagerDLICheck;

typedef enum {
    NvOdmImager_SensorType_Unknown,
    NvOdmImager_SensorType_Raw,
    NvOdmImager_SensorType_SOC,
} NvOdmImagerSensorType;

typedef struct {
    char SensorSerialNum[13]; /**< Up to 12 char, null terminated. */
    char PartNum[9];          /**< 8 char, null terminated. */
    char LensID[2];           /**< 1 char, null terminated. */
    char ManufactureID[3];    /**< 2 char, null terminated. */
    char FactoryID[3];        /**< 2 char, null terminated. */
    char ManufactureDate[10]; /**< DDMMMYYYY, null terminated. */
    char ManufactureLine[3];  /**< 2 char, null terminated. */
    char ModuleSerialNum[9];  /**< Up to 8 char, null terminated. */
    char FocuserLiftoff[5];   /**< 5 char, null terminated. */
    char FocuserMacro[5];     /**< 5 char, null terminated. */
    char ShutterCal[33];      /**< 32 char, null terminated. */
} NvOdmImagerModuleInfo;

typedef struct {
    NvU16 liftoff;
    NvU16 macro;
} NvOdmImagerFocuserCalibration;

typedef unsigned char NvOdmImager_SOC_TargetedExposure;

/**
 * Defines the ODM imager power levels.
 */
typedef enum {
    NvOdmImagerPowerLevel_Off = 1,
    /// Specifies standby, which keeps the register state.
    NvOdmImagerPowerLevel_Standby,
    NvOdmImagerPowerLevel_On,
    /// For stereo camera capture, power up both sensors, then power down them.
    /// This ensures both sensors will start sending frames at the same time.
    NvOdmImagerPowerLevel_SyncSensors,
    NvOdmImagerPowerLevel_Force32 = 0x7FFFFFFF,
} NvOdmImagerPowerLevel;
/**
 * Enumerates different imager devices.
 */
typedef enum {
    NvOdmImagerDevice_Sensor  = 0x0,
    NvOdmImagerDevice_Focuser,
    NvOdmImagerDevice_Flash,
    NvOdmImagerDevice_Num,
    NvOdmImagerDevice_Force32 = 0x7FFFFFFF,
} NvOdmImagerDevice;
/**
 * Defines self tests.
 * Each imager device can perform its own simple
 * tests for validation of the existence and performance
 * of that device. If the test is not applicable or
 * not possible, then it should just return NV_TRUE.
 */
typedef enum {
    NvOdmImagerSelfTest_Existence = 1,
    NvOdmImagerSelfTest_DeviceId,
    NvOdmImagerSelfTest_InitValidate,
    NvOdmImagerSelfTest_FullTest,
    NvOdmImagerSelfTest_Force32 = 0x7FFFFFFF,
} NvOdmImagerSelfTest;
/**
 * Defines bitmasks for different imager devices,
 * so that a subset of devices can be specified
 * for power management functions.
 */
typedef enum {
    NvOdmImagerDevice_SensorMask  = (1<<NvOdmImagerDevice_Sensor),
    NvOdmImagerDevice_FocuserMask = (1<<NvOdmImagerDevice_Focuser),
    NvOdmImagerDevice_FlashMask   = (1<<NvOdmImagerDevice_Flash),
    NvOdmImagerDeviceMask_All = (1<<NvOdmImagerDevice_Num)-1,
    NvOdmImagerDeviceMask_Force32 = 0x7FFFFFFF,
} NvOdmImagerDeviceMask;

#define NVODM_EPSILON (1.0e-10)
#define NVODM_F32_NEAR_EQUAL(_a, _b) (fabs(((NvF32)(_a) - (NvF32)(_b))) <= NVODM_EPSILON)
#define NVODM_IMAGER_MAX_FLASH_LEVELS 260
#define NVODM_IMAGER_MAX_TORCH_LEVELS 130
#define NVODM_IMAGER_MAX_FLASH_TIMER_LEVELS 260
#define NVODM_IMAGER_MAX_TORCH_TIMER_LEVELS 260

typedef struct NvOdmImagerFlashLevelInfoRec {
    NvF32 guideNum;
    NvU32 sustainTime;
    NvF32 rechargeFactor;
} NvOdmImagerFlashLevelInfo;

/// Defines ODM imager flash capabilities.
typedef struct {
    /// Holds the LED to which this cap setting belongs.
    NvU32 LedIndex;
    /// Holds LED-specific parameters such as color temp.
    NvU32 attribute;
    /// Holds the number of level settings supported in @a levels.
    NvU32 NumberOfLevels;
    NvOdmImagerFlashLevelInfo levels[NVODM_IMAGER_MAX_FLASH_LEVELS];
    /// Holds the number of timeout settings supported in @a timeout_ms.
    NvU32 NumberOfTimers;
    NvU32 timeout_ms[NVODM_IMAGER_MAX_FLASH_TIMER_LEVELS];
} NvOdmImagerFlashCapabilities;

/// Defines ODM imager torch capabilities.
typedef struct NvOdmImagerTorchCapabilitiesRec
{
    /// Holds the LED to which this cap setting belongs.
    NvU32 LedIndex;
    /// Holds the LED-specific parameters such as color temp.
    NvU32 attribute;
    NvU32 NumberOfLevels;
    /// Only the one field per level, @a guideNum, for torch modes.
    NvF32 guideNum[NVODM_IMAGER_MAX_TORCH_LEVELS];
    /// Holds the number of timeout settings supported in @a timeout_ms.
    NvU32 NumberOfTimers;
    NvU32 timeout_ms[NVODM_IMAGER_MAX_FLASH_TIMER_LEVELS];
} NvOdmImagerTorchCapabilities;

typedef struct {
    NvU32 NumberOfFlash;
    NvU32 NumberOfTorch;
} NvOdmImagerFlashTorchQuery;

typedef struct {
    /// Specifies the device index to operate.
    NvU16 ledmask;
    NvU16 timeout;
    /// Specifies the value.
    NvF32 value[2];
} NvOdmImagerFlashSetLevel;

/**
 * The camera driver will precisely time the trigger of the strobe
 * to be in synchronization with the exposure time of the picture
 * being taken. This is done by toggling the PinState via our internal
 * chip register controls. But, the camera driver needs to know from the
 * nvodm what to write to the register.
 * The proper trigger signal for a particular flash device could be
 * active high or active low, so the PinState interface allows for
 * specification of either.
 * PinState uses two bitfields. The first one indicates which pins need
 * to be written; the second indicates which of those pins
 * must be pulled high. For example, if you wanted pin4
 * pulled low and pin5 pulled high, you'd use:
 * <pre>
 * 0000 0000 0011 0000
 * 0000 0000 0010 0000
 * </pre>
 * This interface requires the use of the VGP pins. (VI GPIO)
 * The recommended ones are VGP3, VGP4, VGP5, and VGP6.
 * Any other Tegra GPIOs cannot be controlled by the internal hardware
 * to be triggered in relation to frame capture.
*/
typedef struct {
    /// Specifies the pins to be written.
    NvU16 mask;
    /// Specifies the pins to pull high.
    NvU16 values;
} NvOdmImagerFlashPinState;

/**
 * Defines reset type.
 */
typedef enum {
   NvOdmImagerReset_Hard = 0,
   NvOdmImagerReset_Soft,
   NvOdmImagerReset_Force32 = 0x7FFFFFFF,
} NvOdmImagerReset;

/**
 * Holds the frame rate limit at the specified resolution.
 */
typedef struct NvOdmImagerFrameRateLimitAtResolutionRec
{
    NvSize Resolution;
    NvF32 MinFrameRate;
    NvF32 MaxFrameRate;
} NvOdmImagerFrameRateLimitAtResolution;

/**
 * Holds the parameter for the NvOdmImagerSetSensorMode() function.
 */
typedef struct SetModeParametersRec
{
    /// Specifies the resolution.
    NvSize Resolution;
    /// Specifies the exposure time.
    /// If Exposure = 0.0, sensor can set the exposure to its default value.
    NvF32 Exposure;
    /// Specifies the gains.
    /// [0] for R, [1] for GR, [2] for GB, [3] for B.
    /// If any one of the gains is 0.0, sensor can set the gains to its
    /// default values.
    NvF32 Gains[4];
} SetModeParameters;

#define NVODMIMAGER_AF_FOCUSER_CODE_VERSION                  2
#define NVODMIMAGER_AF_MAX_CONFIG_SET                       10
#define NVODMIMAGER_AF_MAX_CONFIG_DIST_PAIR                 16
#define NVODMIMAGER_AF_MAX_CONFIG_DIST_PAIR_ELEMENTS         2

/// Specifies the slew rate value coming down from the configuration.
// Disabled is the same as fastest. Default is the default slew
// rate configuration in the focuser.
#define NVODMIMAGER_AF_SLEW_RATE_DISABLED                   0
#define NVODMIMAGER_AF_SLEW_RATE_DEFAULT                    1
#define NVODMIMAGER_AF_SLEW_RATE_SLOWEST                    9
#define NVODMIMAGER_AF_MAX_SETTLE_TIME                      30


/// Holds distance information for establishing focus on an object.
typedef struct {
    /// Holds the focuser distance number,
    /// which must be within the focuser range.
    NvS32 fdn;

    /// Holds the distance to the object in centimeters.
    NvS32 distance;
} NvOdmImagerAfSetDistancePair;

typedef struct {
    /// Posture has valid values of 's', 'u' or 'd'
    NvS32 posture;

    /// Working range Macro position value.
    /// Set to AF_POS_INVALID_VALUE if invalid or not known
    NvS32 macro;

    /// Hyper focal position value.
    /// Set to AF_POS_INVALID_VALUE if invalid or not known
    NvS32 hyper;

    /// Working range Infinity position value.
    /// Set to AF_POS_INVALID_VALUE if invalid or not known
    NvS32 inf;

    /// Focuser settle time
    /// Set to INT_MAX if invalid or not known
    NvS32 settle_time;

    /// Hysteresis
    /// Set to INT_MAX if invalid or not known
    NvS32 hysteresis;

    /// Offset to be added to macro position.
    /// Set to 0 if invalid or not known
    NvS32 macro_offset;

    /// Offset to be added to Infinity position.
    /// Set to 0 if invalid or not known
    NvS32 inf_offset;

    /// Total number of valid elements in dist_pair[]
    /// Set to 0 if dist_pair[] is not used.
    NvU32 num_dist_pairs;

    /// Array of distance pairs.
    NvOdmImagerAfSetDistancePair dist_pair[NVODMIMAGER_AF_MAX_CONFIG_DIST_PAIR];
} NvOdmImagerAfSet;



/**
 * Defines the imager focuser capabilities.
 */
typedef struct {
    /// Version of the focuser code implemented.
    NvU32 version;

    /// This will be true if the range boundaries reverse which takes
    /// macro to the low and infinity to the high end.
    /// If it is not known, it is set to INT_MAX
    /// This is a read-only parameter from ODM/kernel
    NvS32 rangeEndsReversed;

    /// Holds the total focuser range
    /// Actual low and high are obtained from focuser ODM
    /// If not known, it is set to AF_POS_INVALID_VALUE
    /// These are maintained in kernel driver and can not
    /// be changed or tuned. During set call (into kernel, it is
    /// OK to pass down an INVALID_VALUE.
    NvS32 positionActualLow;
    NvS32 positionActualHigh;

    /// Holds the working focuser range that is a subset
    /// of the total range
    /// If not known, it is set to AF_POS_INVALID_VALUE
    /// This is tunable/calibrated parameter
    /// This is used only in ISP and not in focuser kernel driver.
    /// In ISP, it is reconstructed from afConfigSet.inf and macro.
    /// Kernel can send up an INVALID_VALUE during a get call.
    NvS32 positionWorkingLow;
    NvS32 positionWorkingHigh;

    /// Holds the slew rate as a value from 0 - 9
    /// Value of 0 : Slew rate is disable
    /// Value of 1:  Slew rate set to default value
    /// Values 2 - 9: Different custom implementations
    /// If not known, it is set to INT_MAX
    NvS32 slewRate;

    /// The circle of confusion is the acceptable amount of defocus/blur for
    /// a point source in pixels.
    /// If not known, it is set to INT_MAX
    NvU32 circleOfConfusion;

    /// Set contains inf and macro values as well as their offsets
    NvOdmImagerAfSet afConfigSet[NVODMIMAGER_AF_MAX_CONFIG_SET];

    /// Total number of valid elements within afConfigSet[].
    /// Set to 0 if afConfigSet[] is not used.
    NvU32 afConfigSetSize;

} NvOdmImagerFocuserCapabilities;


/**
 *  Holds the custom info from the imager.
 */
typedef struct {
    NvU32 SizeOfData;
    NvU8 *pCustomData;
} NvOdmImagerCustomInfo;

typedef enum StereoCameraMode NvOdmImagerStereoCameraMode;

/**
 * Holds the inherent gain at the specified resolution.
 */
typedef struct NvOdmImagerInherentGainAtResolutionRec
{
    NvSize Resolution;
    NvF32 InherentGain;
    NvBool SupportsBinningControl;
    NvBool BinningControlEnabled;
} NvOdmImagerInherentGainAtResolution;

/**
 * Holds the characteristics of the imager for bracket operations.
 */
typedef struct NvOdmImagerBracketConfigurationRec{
    NvU32 FlushCount;
    NvU32 InitialIntraFrameSkip;
    NvU32 SteadyStateIntraFrameSkip;
    NvU32 SteadyStateFrameNumer;
} NvOdmImagerBracketConfiguration;

/**
 * Holds group-hold AE sensor settings.
 *
 * Group hold enables programming the sensor with multiple settings that are
 * grouped together, which the sensor then holds off on using until the whole
 * group can be programmed.
 */
typedef struct {
    NvF32 gains[4];
    NvBool gains_enable;
    NvF32 ET;
    NvBool ET_enable;
    NvF32 HDRRatio;
    NvBool HDRRatio_enable;
} NvOdmImagerSensorAE;

/**
 * Holds on-sensor flash control configuration.
 *
 * Specifies how the flash is going to be triggered.
 */
typedef struct {
    NvBool FlashEnable;         /**< Holds a flag specifying to enable the
                                 * on-sensor flash control.
                                 */
    NvBool FlashPulseEnable;    /**< Holds two types of flash controls:
                                 * - 0 - specifies to use continuous flash
                                 *     level only. This control does not
                                 *     support start edge/repeat/dly.
                                 * - 1 - specifies support of control pulses.
                                 *     The next fields in this structure define the
                                 *     control pulse attributes.
                                 */
    NvBool PulseAtEndofFrame;   /**< Holds the flash control pulse rise position:
                                 * - false - specifies the start of the next frame.
                                 * - true - specifies the effective pixel end position
                                 *   of the next frame.
                                 */
    NvBool PulseRepeats;        /**< Holds the flash control pulse repeat:
                                 * - false - specifies that the pulse repeat
                                 *   triggers only one frame.
                                 * - true - specifies that the pulse repeat
                                 *   triggers every frame until flash_on = false.
                                 */
    NvU8 PulseDelayFrames;      /**< Holds the number of frames that the flash
                                 * control pulse can be delayed in frame units:
                                 * @li (0 - 3) - specifies the framenumbers
                                 * that the pulse is delayed.
                                 */
} NvOdmImagerSensorFlashControl;

/**
 * Defines the ODM imager parameters.
 */
typedef enum
{
    /// Specifies exposure. Value must be an NvF32.
    NvOdmImagerParameter_SensorExposure = 0,
    /// Specifies per-channel gains. Value must be an array of 4 NvF32.
    /// [0]: gain for R, [1]: gain for GR, [2]: gain for GB, [3]: gain for B.
    NvOdmImagerParameter_SensorGain,
    /// Specifies read-only frame rate. Value must be an NvF32.
    NvOdmImagerParameter_SensorFrameRate,
    /// Specifies the maximal sensor frame rate the sensor should run at.
    /// Values must be an NvF32. The actual frame rate will be clamped by
    /// ::NvOdmImagerParameter_SensorFrameRateLimits. Setting 0 means no maximal
    /// frame rate is required.
    NvOdmImagerParameter_MaxSensorFrameRate,

    /// Specifies the input clock rate and PLL's.
    /// @deprecated
    /// Value must be an ::NvOdmImagerClockProfile.
    /// Clock profile has the rate as an integer, specified in kHz,
    /// and a PLL multiplier as a float.
    NvOdmImagerParameter_SensorInputClock,
    /// Specifies focus position. Value must be an NvU32,
    /// in the range [0, actuatorRange - 1]. (The upper-end
    /// limit is specified in the ::NvOdmImagerFocuserCapabilities).
    NvOdmImagerParameter_FocuserLocus,
    /// Specifies high-level flash capabilities.  Read-only.
    /// Value must be an ::NvOdmImagerFlashCapabilities.
    NvOdmImagerParameter_FlashCapabilities,
    /// Specifies the level of flash to be used. NvF32.
    NvOdmImagerParameter_FlashLevel,
    /// Specifies any changes to the VGPs that need to occur
    /// to achieve the requested flash level. Read-only.
    /// Should be queried after setting ::NvOdmImagerParameter_FlashLevel.
    /// Value must be an ::NvOdmImagerFlashPinState value.
    NvOdmImagerParameter_FlashPinState,
    /// Specifies high-level torch capabilities.  Read-only.
    /// Value must be an ::NvOdmImagerTorchCapabilities.
    NvOdmImagerParameter_TorchCapabilities,
    /// Specifies the level of torch to be used. NvF32.
    NvOdmImagerParameter_TorchLevel,
    /// Specifies current focal length of lens. Constant for non-zoom lenses.
    /// Value must be an NvF32. Read-only.
    NvOdmImagerParameter_FocalLength,
    /// Specifies maximum aperture of lens. Value must be an NvF32. Read-only.
    NvOdmImagerParameter_MaxAperture,
    /// Specifies the f-number of the lens.
    /// As the f-number of a lens is the ratio of its focal
    /// length and aperture, rounded to the nearest number
    /// on a standard scale, this parameter is provided for
    /// convenience only. Value must be an NvF32. Read-only.
    NvOdmImagerParameter_FNumber,
    /// Specifies read-only minimum/maximum exposure.
    /// Exposure is specified in seconds. Value must be an array of 2 NvF32.
    /// [0]: minimum, [1]: maximum.
    NvOdmImagerParameter_SensorExposureLimits,
    /// Specifies read-only minimum/maximum gain (ISO value).
    /// Value must be an array of 2 NvF32. [0]: minimum, [1]: maximum.
    NvOdmImagerParameter_SensorGainLimits,
    /// Specifies read-only highest/lowest framerate.
    /// Framerate is specified in FPS, i.e.,  15 FPS is less than 60 FPS.
    /// Value must be an array of 2 NvF32. [0]: minimum, [1]: maximum.
    NvOdmImagerParameter_SensorFrameRateLimits,
    /// Specifies read-only highest/lowest framerate at a certain resolution.
    /// Framerate is specified in FPS, i.e., 15 FPS is less than 60 FPS.
    /// Value must be an ::NvOdmImagerFrameRateLimitAtResolution whose
    /// @a NvOdmImagerFrameRateLimitAtResolution.Resolution is the resolution
    /// at which the frame rate range is calculated.
    NvOdmImagerParameter_SensorFrameRateLimitsAtResolution,
    /// Specifies read-only slowest/fastest output clockrate.
    /// @deprecated
    /// The sensor will use the master clock (input) and possibly
    /// increase or decrease the clock when sending the pixels.
    /// Clock rate is specified in KHz. Value must be an array of 2 NvU32.
    /// [0]: minimum, [1]: maximum.
    NvOdmImagerParameter_SensorClockLimits,
    /// Specifies read-only number of frames for exposure
    /// changes to take effect. Value must be an NvU32.
    NvOdmImagerParameter_SensorExposureLatchTime,
    /// Specifies read-only area used by the current resolution,
    /// in terms of the maximum resolution.
    /// This is used by the ISP to recalculate programming that
    /// is calibrated based on the maximum resolution, such as
    /// lens shading.
    /// Type is NvOdmImagerRegion.
    NvOdmImagerParameter_RegionUsedByCurrentResolution,
    /// Fills an ::NvOdmImagerCalibrationData structure with
    /// all calibration data (Common + Isp1 Calibration Data).
    /// Sensors must be calibrated by NVIDIA in order
    /// to properly program the image signal processing
    /// engine and get favorable image results.
    /// If this particular sensor is not to be calibrated,
    /// then the parameter should return a NULL string.
    /// If the sensor should be calibrated, but a NULL string
    /// is returned, then the image processing will be crippled.
    /// If a string for the filename is returned and file open
    /// fails, the driver will be unable to operate, so the
    /// driver will signal the problem to the application, and
    /// go into a unknown state.
    NvOdmImagerParameter_CalibrationData,
    /// Fills an ::NvOdmImagerCalibrationData structure with the
    /// common calibration data.
    NvOdmImagerParameter_CommonCalibrationData,
    /// Fills an ::NvOdmImagerCalibrationData structure with the
    /// Isp1 calibration data.
    NvOdmImagerParameter_Isp1CalibrationData,
    /// Fills an ::NvOdmImagerCalibrationData structure with the
    /// Isp2 calibration data.
    NvOdmImagerParameter_Isp2CalibrationData,
    /// Fills a NvOdmImagerCalibrationData with any overrides
    /// for the calibration data.  This can be used for factory floor
    /// or field calibration.  Optional.
    NvOdmImagerParameter_CalibrationOverrides,
    /// Specifies the self test.
    /// This will initiate a sequence of writes and reads that
    /// allows our test infrastructure to validate the existence
    /// of a well behaving device. To use, the caller calls with
    /// a self-test request, ::NvOdmImagerSelfTest.
    /// The success or failure of the self test is communicated
    /// back to the caller via the return code. NV_TRUE indicates
    /// pass, NV_FALSE indicates invalid self-test or failure.
    NvOdmImagerParameter_SelfTest,
    /// Specifies device status.
    /// During debug, sometimes it is useful to have the
    /// device return information about the device.
    /// Returned is a generic array of NvU16's which
    /// would then be correlated to what the adaptation source
    /// describes it to be. For instance, if the capture hangs,
    /// the driver (in debug mode) can spit out the values of this
    /// array, allowing the person working with the nvodm source
    /// to have visibility into the sensor state.
    /// Use a pointer to an ::NvOdmImagerDeviceStatus struct.
    NvOdmImagerParameter_DeviceStatus,
    /// Specifies test mode.
    /// If a test pattern is supported, it can be enabled/disabled
    /// through this parameter.
    /// Expects a Boolean to be passed in.
    NvOdmImagerParameter_TestMode,
    /// Specifies test values.
    /// If a self-checking test is run with the test mode enabled,
    /// some expected values at several regions in the image
    /// will be needed. Use the ::NvOdmImagerExpectedValues structure.
    /// If the TestMode being enabled results in color bars, then
    /// these expected values may describe the rectangle for each bar.
    /// The expected values are given for the 2x2 arrangement for a set
    /// of bayer pixels. The Width and Height are expected to be even
    /// values, minimum being 2 by 2. A width or height of zero denotes
    /// an end of the list of expected values in the supplied array.
    /// Each resolution can return a different result; this parameter
    /// will be re-queried if the resolution changes.
    /// Returns a const ptr to an ::NvOdmImagerExpectedValues structure.
    NvOdmImagerParameter_ExpectedValues,
    /// Specifies reset type.
    /// Setting this parameter tiggers a sensor reset. The value should
    /// be one of ::NvOdmImagerReset enumerators.
    NvOdmImagerParameter_Reset,
    /// Specifies if the sensor supports optimized resolution changes.
    /// Return NV_FALSE if it is asked to optimize but the sensor is
    /// not able to optimize resolution change.
    /// Return NV_TRUE otherwise.
    /// Value must be an ::NvBool.
    NvOdmImagerParameter_OptimizeResolutionChange,
    /// Detected Color Temperature
    /// Read-only. If the sensor supports determining the color
    /// temperature, this can serve as a hint to the AWB algorithm.
    /// Each frame where the AWB is performing calculations will
    /// get this parameter, and if @a GetParameter returns ::NV_TRUE,
    /// then the value will be fed into the algorithm. If it returns
    /// ::NV_FALSE, then the value is ignored.
    /// Units are in degrees kelvin. Example: 6000 kelvin is typical daylight.
    /// Value must be an NvU32.
    NvOdmImagerParameter_DetectedColorTemperature,

    /// Gets number of lines per second at current resolution.
    /// Parameter type must be an NvF32.
    NvOdmImagerParameter_LinesPerSecond,

    /// Provides imager-specific low-level capabilities data for a focusing
    /// device (::NvOdmImagerFocuserCapabilities structure).
    /// This can be used for factory-floor or field calibration, if necessary.
    NvOdmImagerParameter_FocuserCapabilities,

    /// (Read-only)
    /// Provides a custom block of info from the ODM imager. The block of info
    /// that is returned is private and is known to the ODM implementation.
    /// Parameter type is ::NvOdmImagerCustomInfo.
    NvOdmImagerParameter_CustomizedBlockInfo,

    /// (Read-only)
    /// Indicate if this is a stereo camera.
    NvOdmImagerParameter_StereoCapable,

    /// Select focuser stereo mode.
    NvOdmImagerParameter_FocuserStereo,

    /// Select stereo camera mode. Must be set before sensor power on.
    /// Has no effect for non-stereo camera and will return NV_FALSE.
    /// Parameter type is ::NvOdmImagerStereoCameraMode.
    NvOdmImagerParameter_StereoCameraMode,

    /// Specifies read-only inherent gain at a certain resolution.
    /// Some sensors use summation when downscaling image data.
    /// Value must be an ::NvOdmImagerInherentGainAtResolution whose
    /// @a NvOdmImagerFrameRateLimitAtResolution.Resolution is the resolution
    /// at which the inherent gain is queried.
    /// A Resolution of 0x0 will instead query the gain for the current
    /// resolution.
    /// This parameter is optional, and only needs to be supported in
    /// adaptations that are written for sensors that use summation,
    /// resulting in brighter pixels.  If the pixels during preview are
    /// at the same brightness as still (full-res), then this is likely
    /// not needed.
    NvOdmImagerParameter_SensorInherentGainAtResolution,

    /// Specifies current horizontal view angle of lens.
    /// Value must be an NvF32. Read-only.
    NvOdmImagerParameter_HorizontalViewAngle,

    /// Specifies current vertical view angle of lens.
    /// Value must be an NvF32. Read-only.
    NvOdmImagerParameter_VerticalViewAngle,

    /// Support for setting an SOC sensor's parameters, specifically the ISP
    /// parameters, is done through ISPSetting.  This parameter sends the
    /// ::NvOdmImagerISPSetting struct to pass along the settings.
    /// Please refer to nvcamera_isp.h for all the types of messages and
    /// datatypes used.
    NvOdmImagerParameter_ISPSetting,

    /// (Write-only)
    /// Specifies the current operational mode for an External ISP.
    /// When an external ISP is used, the driver's knowledge of whether
    /// we are in viewfinder-only mode, taking a picture, taking a burst
    /// capture, or shooting a video is passed down. This allows the ODM to make
    /// intelligent decisions based off of this information.
    /// Essentially, this is just a hint passed from the camera driver to the
    /// imager ODM.
    /// Parameter type is ::NvOdmImagerOperationalMode.
    NvOdmImagerParameter_OperationalMode,

    /// (Read-only)
    /// Queryies sensor ISP support.
    /// Parameter type is NvBool.
    NvOdmImagerParameter_SensorIspSupport,

    /// Support for locking the sensor's AWB algorithm.
    /// Value must be a pointer to an ::NvOdmImagerAlgLock struct.
    NvOdmImagerParameter_AWBLock,

    /// Support for locking the sensor's AE algorithm.
    /// Value must be a pointer to an ::NvOdmImagerAlgLock struct.
    NvOdmImagerParameter_AELock,

    /// Specifies when the last resolution/exposure setting becomes effective.
    NvOdmImagerParameter_SensorResChangeWaitTime,

    /// Factory data can be stored at below 4 potential locations:
    ///  (1) sensor OTP
    ///      Typically used in module calibration with relatively
    ///      small amount of data that can be stored in OTP,
    ///      for example focuser data, or even WB data.
    ///
    ///  (2) EEPROM
    ///      Typically used in module calibration with relatively
    ///      large amount of data than can't be fully stored by
    ///      sensor OTP, for example 1K byte or larger EEPROM is
    ///      needed to save LSC and/or WB and/or focuser data.
    ///
    ///  (3) a reserved location for device calibration;
    ///
    ///  (4) other location(s) specified by odm imager driver;
    ///
    /// If factory is enabled, camera core driver attempts to load
    /// factory data with above ordering. The same entry can exist in
    /// one or multiple factory data storage locations. In such case,
    /// the last successful load takes effect for that entry.

    /// Specifies module calibration data from EEPROM.
    NvOdmImagerParameter_ModuleCalibrationData_OTP,

    /// Specifies module calibration data from EEPROM.
    NvOdmImagerParameter_ModuleCalibrationData_EEPROM,

    /// Specifies device calibration data from a factory file.
    NvOdmImagerParameter_DeviceCalibrationData,

    /// Specifies factory calibration data from a file.
    /// It may contain either module or device calibration
    /// data, or only testing data.
    NvOdmImagerParameter_FactoryCalibrationData,

    /// Specifies the sensor ID is read from the device.
    NvOdmImagerParameter_FuseID,

    /// Specifies sensor group-hold control.
    /// Parameter type is ::NvOdmImagerSensorAE for set parameter.
    /// Parameter type is NvBool for get parameter.
    NvOdmImagerParameter_SensorGroupHold,

    /// Specifies the time it takes to read out the active
    /// region.
    NvOdmImagerParameter_SensorActiveRegionReadOutTime,

    /// Allow adaptations to select the best matching sensor mode for a given
    /// resolution, framerate, and usage type.
    /// Use with NvOdmImagerGetParameter.
    /// Parameter type is ::NvOdmImagerSensorMode.
    ///
    /// On entry, the parameter of type ::NvOdmImagerSensorMode holds the
    /// resolution, framerate, and type of the sensor mode to be selected.
    /// An implementation must write the best matching mode from its mode list
    /// into this same struct and return @c NV_TRUE. NvMM then calls
    /// NvOdmImagerSetMode() with that mode.
    ///
    /// Return NV_FALSE to let NvMM perform the mode selection (default).
    NvOdmImagerParameter_GetBestSensorMode,
    /// Specifies flash/torch numbers. Read-only.
    /// Value must be a pointer to an :: NvOdmImagerFlashTorchQuery struct.
    NvOdmImagerParameter_FlashTorchQuery,

    /// Specifies the build date of the imager library.
    NvOdmImagerParameter_ImagerBuildDate,

    /// Specifies exposure ratio when in HDR mode. Value must be an NvF32.
    NvOdmImagerParameter_SensorHDRRatio,

    /// Specifies whether the sensor is an HDR sensor.
    NvOdmImagerParameter_SensorIsHDRSensor,

    /// Specifies to enable HDR. Value must be an NvBool.
    NvOdmImagerParameter_SensorHDREnable,

    /// Specifies ISP public controls.
    NvOdmImagerParameter_IspPublicControls,

    /// If the @c NvOdmImagerParameter enum list is extended,
    /// put the new definitions after @c BeginVendorExtentions.
    NvOdmImagerParameter_BeginVendorExtensions = 0x10000000,

    /** For libcamera, specifies to get calibration status. */
    NvOdmImagerParameter_CalibrationStatus,

    /** For libcamera, specifies to control imager test patterns. */
    NvOdmImagerParameter_TestPattern,

    /** Retrieves module info. */
    NvOdmImagerParameter_ModuleInfo,

    /** Specifies the maximum power for strobe (during test commands). */
    NvOdmImagerParameter_FlashMaxPower,

    /** Specifies the sensor direction. */
    NvOdmImagerParameter_Direction,

    /** Specifies the sensor type. */
    NvOdmImagerParameter_SensorType,

    /** Specifies the DLI test function. */
    NvOdmImagerParameter_DLICheck,

    /** Specifies the DLI test function. */
    NvOdmImagerParameter_ParallelDLICheck,

    /** Specifies the imager bracket capabilities. */
    NvOdmImagerParameter_BracketCaps,

    /// For sensors which use signle shot mode to trigger
    /// and do not always send data after power on.
    /// This parameter is be set in nvmm-camera
    /// after CSI is triggered and after this an
    /// odm code sensor can be triggered.
    NvOdmImagerParameter_SensorTriggerStill,

    /// (Write-only)
    /// Sets sensor flash state: flash on/off.
    /// Parameter type is NvBool.
    NvOdmImagerParameter_SensorFlashSet,

    ///
    NvOdmImagerParameter_Num,

    /// (Read-only)
    /// Indicates pass through mode support from sensor.
    /// Parameter type is NvBool.
    NvOdmImagerParameter_IsPassThroughSupported,

    /// (Write-only)
    /// Sets frame scheme
    /// Parameter type is NvU32.
    ///
    /// Currently supported schemes:
    /// defined by NvOdmImagerFrameRateScheme
    NvOdmImagerParameter_FrameRateScheme,

    /// Ignore -- Forces compilers to make 32-bit enums.
    NvOdmImagerParameter_Force32 = 0x7FFFFFFF
} NvOdmImagerParameter;

///  Defines functions to be implemented by each imager.
typedef struct NvOdmImagerRec *NvOdmImagerHandle;
#define MAX_STATUS_VALUES 160
typedef struct NvOdmImagerDeviceStatusRec
{
    NvU16 Count;
    NvU16 Values[MAX_STATUS_VALUES];
} NvOdmImagerDeviceStatus;
#define MAX_EXPECTED_VALUES 16
typedef struct NvOdmImagerRegionValueRec
{
    NvU32 X, Y;
    NvU32 Width, Height;
    /// Specifies Bayer pixel values.
    NvU16 R;
    NvU16 Gr;
    NvU16 Gb;
    NvU16 B;
} NvOdmImagerRegionValue;
typedef struct NvOdmImagerExpectedValuesRec
{
    NvOdmImagerRegionValue Values[MAX_EXPECTED_VALUES];
} NvOdmImagerExpectedValues, *NvOdmImagerExpectedValuesPtr;

/// Passes along data defined in
/// nvcamera_isp.h when the internal Tegra ISP is not used.
typedef void *NvOdmImagerISPSettingDataPointer;
typedef struct NvOdmImagerISPSettingRec
{
    NvCameraIspAttribute attribute;
    NvOdmImagerISPSettingDataPointer pData;
    NvU32 dataSize;
} NvOdmImagerISPSetting;

typedef struct NvOdmImagerAlgLockRec
{
    NvBool locked;
    NvBool relock;
} NvOdmImagerAlgLock;

/// Passes information about the current
/// driver state.
/// - If the preview is active and video active, then the
/// @a PreviewActive and @a VideoActive Boolean values will be TRUE.
/// - If taking a picture, then @a PreviewActive may be TRUE, and @a StillCount
/// will be 1.
/// - If a burst capture, then @a StillCount will be a number larger than 1.
/// - If cancelling a burst capture, @a StillCount will be 0.
/// - If AE, AF, and AWB are active @a HalfPress will be TRUE.
/// This structure is passed to ODM when any value has changed. The structure
/// will contain the current, complete state of the driver.
typedef struct NvOdmImagerOperationModeRec
{
    NvBool PreviewActive;
    NvBool VideoActive;
    NvBool HalfPress;
    NvU32 StillCount;
} NvOdmImagerOperationalMode;

#define DEFAULT_LIST_SIZE 32
#define MAX_NUM_SENSOR_MODES 30

/**
 * Holds a list of enums that are passed to a client.
 */
typedef struct
{
    NvU32 Property[DEFAULT_LIST_SIZE];
    NvU32 Size;
} NvOdmImagerPropertyList;

/**
 * Holds a list of floats that are passed to a client.
 */
typedef struct
{
    NvF32 Values[DEFAULT_LIST_SIZE];
    NvU32 Size;
} NvOdmImagerFloatList;

/**
 * Holds a list of NvU64 types that are passed to a client.
 */
typedef struct
{
    NvU64 Values[DEFAULT_LIST_SIZE];
    NvU32 Size;
} NvOdmImagerU64List;

/**
 * Holds a list of sizes that are passed to a client.
 */
typedef struct
{
    NvSize Dimensions[DEFAULT_LIST_SIZE];
    NvU32 Size;
} NvOdmImagerSizeList;

/**
 * Holds a list of points that are passed to a client.
 */
typedef struct
{
    NvPointF32 Points[DEFAULT_LIST_SIZE];
    NvU32 Size;
} NvOdmImagerPointList;

/**
 * Holds static imager properties.
 */
typedef struct NvOdmImagerStaticPropertyRec
{
    /// Holds version control data.
    NvU32 Version;

    /// Specifies whether camera supports flash.
    /// This field maps to @c android.flash.info.available.
    NvBool FlashAvailable;

    /// Holds the time taken before flash can fire again in nanoseconds.
    /// This field maps to @c android.flash.info.chargeDuration.
    NvU64 FlashChargeDuration;

    /// Holds the x,y white point of the flash.
    /// This field maps to @c android.flash.colorTemperature.
    NvPointF32 FlashColorTemperature;

    /// Holds the max energy output of the flash.
    /// This field maps to @c android.flash.maxEnergy.
    NvU32 FlashMaxEnergy;

    /// Holds a list of camera LEDs that are available on this system.
    /// This field maps to @c android.led.availableLeds
    NvOdmImagerPropertyList AvailableLeds;

    /// Specifies whether lens is VCM derived.
    NvBool FocuserAvailable;

    /// Holds a list of supported aperture values.
    /// This field maps to @c android.lens.info.availableApertures.
    NvOdmImagerFloatList LensAvailableApertures;

    /// Holds a list of supported ND filter values.
    /// This field maps to @c android.lens.info.availableFilterDensities.
    NvOdmImagerFloatList LensAvailableFilterDensities;

    /// Holds the available focal length of the imager.
    /// This field maps to @c android.lens.info.availableFocalLengths.
    NvOdmImagerFloatList LensAvailableFocalLengths;

    /// Holds a list of the supported optical image stabilization modes.
    /// This field maps to @c android.lens.info.availableOpticalStabilization.
    NvOdmImagerPropertyList LensAvailableOpticalStabilization;

    // TODO: Define later.
    // Holds a map for correcting geometric distortions or the color channel.
    // This field maps to @c android.lens.info.geometricCorrectionMap.
    //  LensGeometricCorrectionMap;

    // Holds the dimensions of the geometric correction map.
    // This field maps to @c android.lens.info.geometricCorrectionMapSize.
    // NvSize LensGeometricCorrectionMapSize;

    /// Holds the hyperfocaldistance of the lens.
    /// This field maps to @c android.lens.info.hyperfocalDistance.
    NvF32 LensHyperfocalDistance;

    /// Holds the shortest focus distance possible.
    /// This field maps to @c android.lens.info.minimumFocusDistance.
    NvF32 LensMinimumFocusDistance;

    /// Holds the relative angle of the camera optical axis to the
    /// perpendicular axis from the display. The first part of this field
    /// defines the angle of separation between the
    /// perpendicular to the screen and the camera optical axis.
    /// The second then defines the clockwise rotation of the
    /// optical axis from native device up.
    /// This field maps to @c android.lens.opticalAxisAngle.
    NvPointF32 LensOpticalAxisAngle;

    /// Holds the coordinates of the camera optical axis on the device.
    /// This field maps to @c android.lens.position.
    NvF32 LensPosition[3];

    /// Holds a list of coordinates of bad pixels in the sensor.
    /// This field maps to @c android.hotPixel.info.map.
    // TODO: Revisit later. Number of entries may need to
    // be increased or a better data structure needed.
    NvOdmImagerPointList HotPixelInfoMap;

    /// Holds the direction of the camera relative to the device screen.
    /// This field maps to @c android.lens.facing.
    NvBool LensFacing;

    /// Holds a list of the sensor modes.
    NvOdmImagerSensorMode SensorModeList[MAX_NUM_SENSOR_MODES];

    /// Holds the number sensor modes that are defined on the imager.
    NvU32 SensorModeNum;

    /// Holds the minimum frame duration for each resolution in nanoseconds.
    /// This field maps to @c android.scaler.availableProcessedMinDurations.
    NvOdmImagerU64List ScalerAvailableProcessedMinDurations;

    /// Holds the resolutions available for use with processed output stream.
    /// This field maps to @c android.scaler.availableProcessedSizes.
    NvOdmImagerSizeList ScalerAvailableProcessedSizes;

    /// Holds the minimum frame duration that is supported for each raw resolution.
    /// This field maps to @c android.scaler.availableRawMinDurations.
    NvOdmImagerU64List ScalerAvailableRawMinDurations;

    /// Holds the resolutions available for use with
    /// raw sensor output streams.
    /// This field maps to @c android.scaler.availableRawSizes.
    NvOdmImagerSizeList ScalerAvailableRawSizes;

    /// Holds the area of raw data that corresponds to only active pixels.
    /// This field maps to @c android.sensor.info.activeArraySize.
    NvRect SensorActiveArraySize;

    /// Holds a list of the supported sensitivity values.
    /// This field maps to @c android.sensor.info.availableSensitivities.
    NvOdmImagerPropertyList SensorAvailableSensitivities;

    /// Holds the arrangement of color filters on the sensor.
    /// This field maps to @c android.sensor.info.colorFilterArrangement.
    NvOdmImagerPixelType SensorColorFilterArrangement;

    /// Holds the range of valid exposure times in nanoseconds.
    /// This field maps to @c android.sensor.info.exposureTimeRange.
    NvU64 SensorExposureTimeRange[2];

    /// Holds the maximum frame duration.
    /// This field maps to @c android.sensor.info.maxFrameDuration.
    NvU64 SensorMaxFrameDuration;

    /// Holds the physical dimensions of the full pixel array in
    /// millimeters.
    /// This field maps to @c android.sensor.info.physicalSize.
    NvPointF32 SensorPhysicalSize;

    /// Holds the dimensions of the full pixel array.
    /// This field maps to @c android.sensor.info.pixelArraySize.
    NvSize SensorPixelArraySize;

    /// Holds the maximum raw value that the sensor produces.
    /// This field maps to @c android.sensor.info.whiteLevel.
    NvU32 SensorWhiteLevel;

    /// Holds the gain factor from electrons to raw units.
    /// This field maps to @c android.sensor.baseGainFactor.
    NvF32 SensorBaseGainFactor;

    /// Holds a fixed black level offset for each of the Bayer mosaic channels.
    /// This field maps to @c android.sensor.blackLevelPattern.
    NvU32 BlackLevelPattern[2][2];

    /// Holds the maximum sensitivity that is implemented purely through analog gain.
    /// This field maps to @c android.sensor.maxAnalogSensitivity.
    NvU32 SensorMaxAnalogSensitivity;

    /// Holds an estimation of sensor noise characteristics.
    /// This field maps to @c android.sensor.noiseModelCoefficients.
    NvPointF32 SensorNoiseModelCoefficients;

    /// Holds the clockwise angle through which the output
    /// image must be rotated to be upright
    /// on the device screen in its native orientation.
    /// This field maps to @c android.sensor.orientation.
    NvU32 SensorOrientation;

} NvOdmImagerStaticProperty;

/**
 *  Holds the YUV control properties.
 */
typedef struct NvOdmImagerYUVControlPropertyRec
{
    /// Holds the version control.
    NvU32 Version;

    /// Maps to @c android.sensor.sensitivity.
    /// In ISO arithmetic units.
    NvU32 SensorSensitivity;

    /// Maps to @c android.sensor.exposureTime.
    /// Value in seconds.
    NvF32 SensorExposureTime;

    /// Maps to @c android.sensor.frameDuration.
    /// Maximum frame duration
    NvU64 SensorFrameDuration;

    /// Maps to @c android.control.mode.
    NvOdmImagerControlMode ControlMode;

    /// Maps to @c android.control.sceneMode.
    NvOdmImagerSceneMode SceneMode;

    /// Maps to @c android.control.videoStabilizationMode.
    NvBool VideoStabalizationMode;

    /// Maps to @c android.demosaic.mode.
    NvOdmImagerDemosaicMode DemosaicMode;

    /// Maps to @c android.control.effectMode.
    NvOdmImagerColorEffectsMode ColorEffectsMode;

    /// Maps to @c android.colorCorrection.mode.
    NvOdmImagerColorCorrectionMode ColorCorrectionMode;

    /// Maps to @c android.noiseReduction.mode.
    NvOdmImagerNoiseReductionMode NoiseReductionMode;

    /// Maps to @c android.noiseReduction.strength. Range[1,10].
    NvF32 NoiseReductionStrength;

    /// Maps to @c android.shading.mode
    NvOdmImagerShadingMode ShadingMode;

    /// Maps to @c android.shading.strength. Range [1,10].
    NvF32 ShadingStrength;

    /// Maps to @c android.edge.mode
    NvOdmImagerEdgeEnhanceMode EdgeEnhanceMode;

    /// Maps to @c android.edge.strength. Range: [1,10].
    NvF32 EdgeEnhanceStrength;

    /// Maps to @c android.geometric.mode
    NvOdmImagerGeometricMode  GeometricMode;

    /// Maps to @c android.geometric.strength. Range [1,10].
    NvF32 GeometricStrength;

    /// Maps to @c android.hotPixel.mode
    NvOdmImagerHotPixelMode HotPixelMode;

    /// Maps to @c android.flash.firingPower. Range [1,10].
    NvF32 FlashFiringPower;

    /// Maps to @c android.flash.firingTime in Nanoseconds.
    /// Firing time is relative to start of the exposure.
    NvU64 FlashFiringTime;

    /// Maps to @c android.flash.mode.
    NvOdmImagerFlashMode FlashMode;

    /// Maps to @c android.led.transmit.
    NvBool LedTransmit;

    /// Maps to @c android.statistics.faceDetectMode.
    NvOdmImagerFaceDetectMode StatsFaceDetectMode;

    /// Maps to @c android.statistics.histogramMode.
    NvBool StatsHistogramMode;

    /// Maps to @c android.statistics.sharpnessMapMode.
    NvBool StatsSharpnessMode;

    /// Maps to @c android.scaler.cropRegion.
    NvRect ScalerCropRegion;

    /// Maps to @c android.control.aeLock.
    NvBool AeLock;

    /// Maps to @c android.control.aeMode.
    NvOdmImagerAeMode AeMode;

    /// Maps to @c android.control.aeTargetFpsRange.
    NvF32 AeTargetFpsRange[2];

    /// Maps to @c android.control.aeAntibandingMode.
    NvOdmImagerAeAntibandingMode AeAntibandingMode;

    /// Maps to @c android.control.aeExposureCompensation.
    /// after conversion into android specific data type.
    NvF32 AeExposureCompensation;

    /// Maps to @c android.control.aeRegions.
    NvOdmImagerRegions AeRegions;

    /// Maps to @c android.control.aePrecaptureTrigger.
    NvOdmImagerAePrecaptureTrigger AePrecaptureTrigger;

    /// Maps to @c android.control.afMode.
    NvOdmImagerAfMode AfMode;

    /// Maps to @c android.control.afRegions.
    NvOdmImagerRegions AfRegions;

    /// Maps to @c android.control.afTrigger.
    NvOdmImagerAfTrigger AfTrigger;

    /// Maps to @c android.control.awbLock.
    NvBool AwbLock;

    /// Maps to @c android.control.awbMode.
    NvOdmImagerWhiteBalance AwbMode;

    /// Maps to @c android.control.awbRegions.
    NvOdmImagerRegions AwbRegions;

} NvOdmImagerYUVControlProperty;

typedef struct NvOdmImagerYUVDynamicPropertyRec
{
    /// Holds the version control.
    NvU32 Version;

    /// Maps to @c android.colorCorrection.mode.
    NvOdmImagerColorCorrectionMode ColorCorrectionMode;

    /// Maps to @c android.control.aeRegions.
    NvOdmImagerRegions AeRegions;

    /// Maps to @c android.control.aeState.
    NvOdmImagerAeState AeState;   /// not yet

    /// Maps to @c android.control.afMode.
    NvOdmImagerAfMode AfMode;

    /// Maps to @c android.control.afRegions.
    NvOdmImagerRegions AfRegions;

    /// Maps to @c android.control.afState
    NvOdmImagerAfState AfState;   /// not yet

    /// Maps to @c android.control.awbMode.
    NvOdmImagerWhiteBalance AwbMode;

    /// Maps to @c android.control.awbRegions.
    NvOdmImagerRegions AwbRegions;

    /// Maps to @c android.control.awbState.
    NvOdmImagerAwbState AwbState;   /// not yet

    /// Maps to @c android.control.mode.
    NvOdmImagerControlMode ControlMode;

    /// Maps to @c android.edge.mode.
    NvOdmImagerEdgeEnhanceMode EdgeEnhanceMode;

    /// Maps to @c android.flash.firingPower. Range [1,10]
    NvF32 FlashFiringPower;

    /// Maps to @c android.flash.firingTime in Nanoseconds.
    /// Firing time is relative to start of the exposure.
    NvU64 FlashFiringTime;

    /// Maps to @c android.flash.mode.
    NvOdmImagerFlashMode FlashMode;

    /// Maps to @c android.control.FlashState.
    NvOdmImagerFlashState FlashState;   /// not yet

    /// Maps to @c android.hotPixel.mode.
    NvOdmImagerHotPixelMode HotPixelMode;

    /// Maps to @c android.lens.aperture.
    /// Size of the lens aperture.
    NvF32 LensAperture;

    /// Maps to @c android.lens.filterDensity.
    NvF32 LensFilterDensity;

    /// Maps to @c android.lens.focalLength.
    NvF32 LensFocalLength;

    /// Maps to @c android.lens.focusDistance.
    NvF32 LensFocusDistance;

    /// Valid only in NvCamAfMode_Manual.
    NvS32  LensFocusPos;

    /// Maps to @c android.lens.opticalStabilizationMode.
    NvBool LensOpticalStabalizationMode;

    /// Maps to @c android.lens.focusrange.
    NvCameraIspRangeF32 LensFocusRange;

    /// Maps to @c android.lens.state.
    NvU32 LensState;

    /// Maps to @c android.noiseReduction.mode.
    NvOdmImagerNoiseReductionMode NoiseReductionMode;

    /// Maps to @c android.scaler.cropRegion.
    NvRect ScalerCropRegion;

    /// Maps to @c android.sensor.exposureTime.
    /// Value in seconds.
    NvF32 SensorExposureTime;

    /// Maps to @c android.sensor.sensitivity.
    /// In ISO arithmetic units.
    NvU32 SensorSensitivity;

    /// Maps to @c android.sensor.timestamp.
    NvU64 SensorTimestamp;

    /// Maps to @c android.shading.mode.
    NvOdmImagerShadingMode ShadingMode;

    /// Maps to @c android.statistics.faceDetectMode.
    NvOdmImagerFaceDetectMode StatsFaceDetectMode;

    /// Maps to @c android.statistics.histogramMode.
    NvBool StatsHistogramMode;

    /// Maps to @c android.statistics.sharpnessMapMode.
    NvBool StatsSharpnessMode;

    /// Maps to @c android.led.transmit.
    NvBool LedTransmit;

} NvOdmImagerYUVDynamicProperty;

// FOR YUV Sensors
/**
 *  Holds YUV sensor ISP properties.
 */
typedef struct NvOdmImagerISPStaticPropertyRec
{
    /// Maps to @c android.control.aeAvailableModes.
    NvOdmImagerPropertyList SupportedAeModes;
    /// Maps to @c android.control.aeAvailableTargetFpsRanges.
    NvOdmImagerPointList SupportedFpsRanges;
    /// Maps to @c android.control.awbAvailableModes.
    /// List of AWB modes that can be selected.
    NvOdmImagerPropertyList SupportedAwbModes;
    /// Maps to @c android.control.afAvailableModes.
    /// List of AF modes that can be selected.
    NvOdmImagerPropertyList SupportedAfModes;
} NvOdmImagerISPStaticProperty;

/** Gets the capabailities (the details) for the specified imager.
 * This fills out the capabilities structure with the timings (resolutions),
 * the pixel types, the name, and the type of imager.
 *
 * @param Imager The handle to the imager.
 * @param Capabilities A pointer to the capabilities structure to fill.
 */
void NvOdmImagerGetCapabilities(
        NvOdmImagerHandle Imager,
        NvOdmImagerCapabilities *Capabilities);
/** Gets the list of resolutions for the sensor device.
 * This function fills the @a Modes array with
 * resolution information for each supported mode.
 * From this array, you select a single @c NvOdmImagerSensorMode structure,
 * which you pass to the
 * NvOdmImagerSetSensorMode() function to set the resolution.
 * @c %NvOdmImagerGetCapabilities() is intended for query purposes only; no side
 * effects are expected.
 *
 * @param[in] Imager The handle to the imager.
 * @param[in,out] Modes A pointer to an array modes, or @c NULL to get only the NumberOfModes.
 * @param[out] NumberOfModes A pointer to the number of timings.
 */
void NvOdmImagerListSensorModes(
        NvOdmImagerHandle Imager,
        NvOdmImagerSensorMode *Modes,
        NvS32 *NumberOfModes);
/** Sets the resolution and format.
 * This can be used to change the resolution or format at any time,
 * though with most imagers, it will not take effect until the next
 * frame or two.
 * The sensor may or may not support resolution changes, and may or may
 * not support arbitrary resolutions. If it cannot match the resolution,
 * the imager will return the next largest resolution than the one
 * requested.
 *
 * @param[in] hImager The handle to the imager.
 * @param[in] pParameters A pointer to the desired ::SetModeParameters.
 *                    After this function returns, the sensor must be programmed
 *                    to the desired parameters to avoid exposure blip.
 * @param[out] pSelectedMode A pointer to the resolution that was set, and the
 *                     corresponding offset in the pixel stream to start at.
 * @param[out] pResult A pointer to a struct that describes the modes that were
 *                     actually set.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool NvOdmImagerSetSensorMode(
        NvOdmImagerHandle hImager,
        const SetModeParameters *pParameters,
        NvOdmImagerSensorMode *pSelectedMode,
        SetModeParameters *pResult);

/** Sets the power level to on, off, or standby
 *  for a (sub)set of imager devices.
 *
 * @param Imager The handle to the imager.
 * @param Devices The bitmask for imager devices.
 * @param PowerLevel The power level to set.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool NvOdmImagerSetPowerLevel(
        NvOdmImagerHandle Imager,
        NvOdmImagerDeviceMask Devices,
        NvOdmImagerPowerLevel PowerLevel);
/** Gets the power level for specified imager device.
 *
 * @param Imager The handle to the imager.
 * @param Device The imager device.
 * @param PowerLevel A pointer to the returned power level.
*/
void NvOdmImagerGetPowerLevel(
        NvOdmImagerHandle Imager,
        NvOdmImagerDevice Device,
        NvOdmImagerPowerLevel *PowerLevel);
/** Sets the parameter indicating an imager setting.
 * @see NvOdmImagerParameter
 *
 * @param Imager The handle to the imager.
 * @param Param The parameter to set.
 * @param SizeOfValue The size of the value.
 * @param Value A pointer to the value.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool NvOdmImagerSetParameter(
        NvOdmImagerHandle Imager,
        NvOdmImagerParameter Param,
        NvS32 SizeOfValue,
        const void *Value);
/** Gets the parameter, which queries a variable-sized imager setting.
 * @see NvOdmImagerParameter
 *
 * @param Imager The handle to the imager.
 * @param Param The returned parameter.
 * @param SizeOfValue The size of the value.
 * @param Value A pointer to the value.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool NvOdmImagerGetParameter(
        NvOdmImagerHandle Imager,
        NvOdmImagerParameter Param,
        NvS32 SizeOfValue,
        void *Value);

/*  Functions to be implemented once  */
/**
 * Creates an imager handle.
 * The first step in using an imager is to open the imager and get a handle.
 * The supplied globally unique identifier (@a ImagerGUID) tells the Open
 * routine which imager is to be used.
 * To obtain a list of GUIDs available, please use the functions in
 * nvodm_query_discovery.h, like NvOdmPeripheralEnumerate().
 *
 * @param[in] ImagerGUID Specifies how the imager is selected.
 * If /c ImagerGUID==0, then the imager
 * picks what it determines to be the default. This is completely
 * subject to the NVODM imager implementation. If /c ImagerGUID==1, the
 * next available default imager is selected, which is usually a secondary device.
 *
 * @param[out] phImager Handle to an imager object.
 *
 * @return NV_TRUE if successful, or NV_FALSE if GUID is not recognized or
 *         no imagers are available.
 */
#define NVODM_IMAGER_MAX_REAL_IMAGER_GUID 10
NvBool NvOdmImagerOpen(NvU64 ImagerGUID, NvOdmImagerHandle *phImager);

/**
 * Creates an imager handle that includes the specified imager devices:
 * sensor, focuser, and flash.
 * For each sensor/focuser/flash, the function:
 * - Searches for the specified device from the list of imager devices.
 * - If a real device is not found, the function searches for it from the list of
 *   virtual devices.
 *
 * @param[in] SensorGUIDarg Specifies the sensor to use.
 * @param[in] FocuserGUIDarg Specifies the focuser to use.
 * @param[in] FlashGUIDarg Specifies the flash to use.
 * @param[in] UseSensorCapGUIDs Description to be supplied.
 * @param[out] phImager Handle to an imager object.
 *
 * @return NV_TRUE if successful, or NV_FALSE if GUID is not recognized or
 * no imagers are available.
  */
NvBool NvOdmImagerOpenExpanded(NvU64 SensorGUIDarg, NvU64 FocuserGUIDarg, NvU64 FlashGUIDarg,
                        NvBool UseSensorCapGUIDs, NvOdmImagerHandle *phImager);
/**
 * Releases any resources and memory utilized.
 * @param hImager A handle to the imager to release.
 */
void NvOdmImagerClose(NvOdmImagerHandle hImager);

/**
 * Initializes device detection session.
 */
NvBool NvOdmImagerDetInit(void);

/**
 * Destroys device detection session.
 */
NvBool NvOdmImagerDetExit(void);

/**
 * Detects available devices based on the list from ODM Query.
 */
NvBool NvOdmImagerDeviceDetect(void);

/**
 * query the sensor static properties of a camera
 */
NvError NvOdmImagerGetSensorStaticProperties(
    NvOdmImagerHandle hImager,
    NvOdmImagerStaticProperty *pProperties);

/**
 * query the focuser static properties of a camera
 */
NvError NvOdmImagerGetFocuserStaticProperties(
    NvOdmImagerHandle hImager,
    NvOdmImagerStaticProperty *pProperties);

/**
 * query the flash static properties of a camera
 */
NvError NvOdmImagerGetFlashStaticProperties(
    NvOdmImagerHandle hImager,
    NvOdmImagerStaticProperty *pProperties);

/**
 * Gets the static properties of a specific imager.
 *
 * @param[in] hSensor A handle to the imager.
 *    If @c NULL, %NvOdmImagerGetStaticProperty() uses
 *    the @a ImagerGUID parameter to obtain a handle to the imager.
 * @param[in] ImagerGUID The imager ID. If @a hSensor is provided,
 *    this parameter is ignored.
 * @param[out] pStatic A pointer to a structure where
 *    %NvOdmImagerGetStaticProperty() puts the static properties.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmImagerGetStaticProperty(
    NvOdmImagerHandle hSensor,
    NvU64 ImagerGUID,
    NvOdmImagerStaticProperty *pStatic);

/**
 * Queries the ISP static properties of a YUV camera.
 */
NvBool
NvOdmImagerGetISPStaticProperty(
    NvOdmImagerHandle hSensor,
    NvU64 ImagerGUID,
    NvOdmImagerISPStaticProperty *pStatic);

/**
 * query the ISP control properties of a yuv camera.
 */
NvBool
NvOdmImagerGetYUVControlProperty(
    NvOdmImagerHandle hSensor,
    NvU64 ImagerGUID,
    NvOdmImagerYUVControlProperty *pControl);

/**
 * query the ISP dynamic properties of a yuv camera.
 */
NvBool
NvOdmImagerGetYUVDynamicProperty(
    NvOdmImagerHandle hSensor,
    NvU64 ImagerGUID,
    NvOdmImagerYUVDynamicProperty *pControl);

/**
 * @deprecated Please use nvodm_query_discovery.h.
 */
NvBool
NvOdmImagerGetDevices( NvS32 *Count, NvOdmImagerHandle *Imagers);
/**
 * @deprecated Please use nvodm_query_discovery.h.
 */
void
NvOdmImagerReleaseDevices( NvS32 Count, NvOdmImagerHandle *Imagers);

/** Workaround so libcamera code can access libodm_imager code directly. */
NvOdmImagerHandle NvOdmImagerGetHandle(void);

#if defined(__cplusplus)
}
#endif
/** @} */
#endif // INCLUDED_NVODM_SENSOR_H

