/*
 * Copyright (c) 2008-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit:
 *           NvMM Camera APIs</b>
 *
 * @b Description: Declares Interface for NvMM Camera APIs.
 */

#ifndef INCLUDED_NVMM_CAMERA_H
#define INCLUDED_NVMM_CAMERA_H

/** @defgroup nvmm_camera Camera API
 *
 * NvMM Camera is the source block of still and video image data.
 * It provides the functionality needed for capturing
 * image buffers in memory for further processing before
 * encoding or previewing. (For instance, one may wish
 * to scale or rotate the buffer before encoding or previewing.)
 * NvMM Camera has the ability to capture raw streams of
 * image data straight to memory, perform image processing,
 * reprogram the sensor for exposure and focusing (via calls to
 * nvodm_imager, and other behaviors expected of a digital camera.
 * Dependency: nvodm_imager.h is a companion to nvmm_camera.h as
 * it provides the interface to the sensor and focuser.
 *
 * @ingroup nvmm_modules
 * @{
 */

#include "nvcommon.h"
#include "nvos.h"
#include "nvodm_imager.h"
#include "nvrm_surface.h"
#include "nvmm.h"
#include "nvmm_event.h"
#include "nvfxmath.h"
#include "nvmm_camera_types.h"
#include "nvcamera_isp.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @brief Camera Attribute enumerations.
 * Following enum are used by nvmm_camera for setting the block's attributes.
 * They are used as 'eAttributeType' variable in SetAttribute() API.
 * These attributes are applicable to the whole camera block.
 * @see SetAttribute
 * @ingroup nvmmcamerablock
 */
typedef enum
{
    /**
     * AvailableSensors.
     * Returns an NvU32 count of available sensors.
     * On a device that has no sensors, the value will be 0.
     * On a device with one, it will be 1, and 2 for a device with 2.
     * Can be accessed before setting up the device.
     * Read-only attribute.
     * Parameter type: NvU32.
     */
    NvMMCameraAttribute_AvailableSensors = 1,
    /*
     * The selection of an input device (imager) is done via a GUID
     * or allowing the nvodm to use the default.
     * The Open block call can choose to give a CreationParameter
     * field BlockSpecific of NVMMCAMERA_NO_IMAGER which defers setting up
     * the camera and sensor hardware until this Attribute is given,
     * which supplies the GUID selection.  Only specific test programs
     * would ever need to set this to anything but DEFAULT or SECOND,
     * as knowledge of the specific GUIDs is not necessary and is not
     * recommended.
     * Setting this when it is not needed will result in it being ignored.
     * Write only attribute.
     * Parameter type: NvU64.
     */
#define NVMMCAMERA_NO_IMAGER ((NvU64)-1)
#define NVMMCAMERA_DEFAULT_IMAGER (0ULL)
#define NVMMCAMERA_SECOND_IMAGER  (1ULL)
    NvMMCameraAttribute_ImagerGUID,

    /**
     * @brief Preview can only be enabled/paused/disabled.
     * There is no reset needed. Resetting the block by setting
     * the nvmm block state to Stop is sufficient.
     * Parameter type: NvMMCameraPinState.
     */
    NvMMCameraAttribute_Preview,

    /* Sensor normalization performs a number of operations that convert the
     * incoming signal from the sensor into a normalized RGB image, such as:
     *    - Optical Black Correction
     *    - Lens Shading Correction
     *    - De-knee Correction
     *    - Bad Pixel Concealment
     *    - Demosaicing
     *    - Noise reduction
     *    - Color Correction
     *    - White Balance Correction
     *    - Gamma Correction
     *
     * Depending on the hardware implementation (camera module and ISP),
     * more or fewer operations may be performed to generate the
     * normalized RGB image.
     *
     * Sensor normalization must be enabled for any additional image-processing
     * functions to have an effect.  If sensor normalization is disabled, the
     * settings for all other ISP functions are ignored.  To perform RAW Bayer
     * image capture, sensor normalization should be disabled.
     *
     * Parameter type: NvBool
     * Default value: NV_TRUE
     */
    NvMMCameraAttribute_SensorNormalization,

    /* If sensor normalization is enabled, enabling partial normalization
     * will cause partially-processed Bayer data to be output.  The value
     * given for partial normalization determines which operations are
     * still performed on the Bayer data.
     *
     * Disabling partial normalization (i.e., setting it to 0) allows
     * sensor normalization to have its normal effect.
     *
     * If sensor normalization is disabled, this attribute has no effect.
     *
     * Parameter type: NvU32. (Use the bit mask below)
     */
    NvMMCameraAttribute_PartialNormalization,
#define NVMMCAMERA_PROCESS_OPTICAL_BLACK    (1<<1)
#define NVMMCAMERA_PROCESS_LENS_SHADING     (1<<2)
#define NVMMCAMERA_PROCESS_DEKNEE           (1<<3)
#define NVMMCAMERA_PROCESS_WHITE_BALANCE    (1<<4)

    /* Enabling cropping will cause the camera to process a subrectangle
     * of the input sensor image.  This will be applied before the
     * digital zoom.
     * Parameter type: NvBool.
     */
    NvMMCameraAttribute_Crop,

    /*  CurrentFrameRate indicates the current frame rate being used.
     *  Querying this parameter will give the frame rate of the imager.
     *  If the auto frame rate function is enabled, then the value queried
     *  is the one selected to match the exposure needs.  Otherwise it is
     *  the maximum allowed frame rate.  This parameter is read-only, and an
     *  assert encountered in debug mode if the application attempts
     *  to set this value via SetAttribute. Values are fixed point
     *  numbers.
     *  Parameter type: NvSFX.
     */
    NvMMCameraAttribute_CurrentFrameRate,

    /* PowerManagement instructs the NvMMCamera to attempt to save
     * power whenever possible. Off will have no power savings opportunities
     * taken.  FULL will sacrifice performance and image quality and frame
     * rate wherever possible, to achieve any power savings.
     * SUSPEND will instuct camera to suspend the imager and hw.
     * RESUME will instruct camera to wake the hw and imager and resume
     *  normal operations.  Previous OFF to FULL setting will be restored.
     *  Parameter type: NvU32. Use parameter defined below.
     */
    NvMMCameraAttribute_PowerManagement,
#define NVMMCAMERA_POWERMANAGEMENT_OFF     0
#define NVMMCAMERA_POWERMANAGEMENT_FULL    10
#define NVMMCAMERA_POWERMANAGEMENT_SUSPEND 100
#define NVMMCAMERA_POWERMANAGEMENT_RESUME  200

    /* FlashMode instructs the NvMMCamera to use flash to illuminate
     * dark scenes.
     * NVMMCAMERA_FLASHMODE_OFF disables any flash.
     * NVMMCAMERA_FLASHMODE_TORCH enables the flash in "torch mode".
     * See also NvMMCameraAttribute_ManualTorchAmount.
     * NVMMCAMERA_FLASHMODE_MOVIE enables the flash in "torch mode"
     * during video capture, and disables it otherwise.
     * See also NvMMCameraAttribute_ManualTorchAmount.
     * NVMMCAMERA_FLASHMODE_STILL_MANUAL strobes the flash for each
     * image capture.  See also NvMMCameraAttribute_ManualFlashAmount.
     * NVMMCAMERA_FLASHMODE_STILL_AUTO strobes the flash for each
     * image capture if one is present and AE determines that one
     * would be helpful for the current scene.
     * NVMMCAMERA_FLASHMODE_REDEYE_REDUCTION provides same feature as
     * NVMMCAMERA_FLASHMODE_STILL_AUTO plus red eye reduction
     * strobe if necessary
     *
     * NVMMCAMERA_FLASHMODE_FORCE and NVMMCAMERA_FLASHMODE_AUTO are
     * deprecated and will soon be removed.
     * Parameter type: NvU32
     */
    NvMMCameraAttribute_FlashMode,
#define NVMMCAMERA_FLASHMODE_OFF                            0
#define NVMMCAMERA_FLASHMODE_TORCH                          1
#define NVMMCAMERA_FLASHMODE_MOVIE                          2
#define NVMMCAMERA_FLASHMODE_STILL_MANUAL                   3
#define NVMMCAMERA_FLASHMODE_STILL_AUTO                     4
#define NVMMCAMERA_FLASHMODE_REDEYE_REDUCTION               5

// Obselete, but alias to new values to facilitate the transition
#define NVMMCAMERA_FLASHMODE_FORCE              3
#define NVMMCAMERA_FLASHMODE_AUTO               4

    /* User may attributes such as NvMMCameraAttribute_ManualTorchAmount
     * or NvMMCameraAttribute_FlashMixingRatio to control torch or flash,
     * DisableManualFlashControl is used to disable all manual settings at
     * once and allow flash algorithm to control them.
     */
    NvMMCameraAttribute_DisableManualFlashControl,

    /* ManualTorchAmount describes what fraction of maximum possible
     * infinitely-sustainable flash to use when FlashMode is set to
     * NVMMCAMERA_FLASHMODE_TORCH or NVMMCAMERA_FLASHMODE_MOVIE.
     * Type is NvF32, range is 0 to 1.
     * Specifying a value of 0 will cause the flash to be turned off.
     * Note that the values supported by a particular flash unit may not
     * be continuous, in which case NvMM will round to the nearest value
     * supported by the HW.  If the requested value was greater than zero but
     * the closest supported value is zero, NvMM will instead round up.
     * This means that for a device that only supports on/off, .0001 would
     * be rounded all the way up to 1.0!
     * GetAttribute of ManualTorchAmount will return the value post-rounding,
     * unless the imager has not yet been powered up, in which case it will
     * simply return the last-requested-value.
     * Default value is 1.0.
     * Parameter type: NvF32
     */
    NvMMCameraAttribute_ManualTorchAmount,

    /* ManualFlashAmount describes what fraction of maximum possible
     * flash to use when FlashMode is set to NVMMCAMERA_FLASHMODE_STILL_MANUAL.
     * Type is NvF32, range is 0 to 1.
     * Specifying a ManualFlashAmount of 0 will cause
     * NVMMCAMERA_FLASHMODE_STILL_MANUAL behave the same as
     * NVMMCAMERA_FLASHMODE_OFF.
     * Note that the values supported by a particular flash unit may not
     * be continuous, in which case NvMM will round to the nearest value
     * supported by the HW.  If the requested value was greater than zero but
     * the closest supported value is zero, NvMM will instead round up.
     * This means that for a device that only supports on/off, .0001 would
     * be rounded all the way up to 1.0!
     * GetAttribute of ManualFlashAmount will return the value post-rounding,
     * unless the imager has not yet been powered up, in which case it will
     * simply return the last-requested-value.
     * Default value is 1.0.
     * Note that some settings for this attribute may affect the usable range
     * of auto-frame rate, due to such considerations as flash sustain time.
     * Parameter type: NvF32
     */
    NvMMCameraAttribute_ManualFlashAmount,

    /* AutoFlashAmount allows the caller to specify what fraction of
     * maximum possible flash to use when FlashMode is set to
     * NVMMCAMERA_FLASHMODE_STILL_AUTO, if NvMMCamera chooses to flash.
     * Specifying an AutoFlashAmount of 0 will allow NvMMCamera to
     * determine how much flash to use as well as whether or not to flash.
     * See also the comments for NvMMCameraAttribute_ManualFlashAmount.
     * Parameter type: NvF32
     * Parameter range: 0 to 1.0
     */
#define NVMMCAMERA_AUTOFLASHAMOUNT_AUTO     (0.0f)
    NvMMCameraAttribute_AutoFlashAmount,

    /* FlashColorTemperature allows the caller to specify flash color
     * temperature in unit of CCT (Correlated Color Temperature). If specified
     * temperature is not available, the closest one will be used.
     * Setting a value of zero means allowing algorithm to automatically
     * select the best CCT that matches the ambient light.
     * Parameter type: NvU32
     * Parameter range: 0 to 20000
     * Default value: 0
     */
    NvMMCameraAttribute_FlashColorTemperature,

    /**
     * @brief Attributes which take data structures to set/get the
     * values.  Each comment will describe the expected data structure.
     */

    /*  When cropping is enabled, CropRect specifies the subrectangle of the
     *  image which will be processed.  Changes to this parameter will take
     *  effect the next frame.  This setting has no effect if cropping is not
     *  enabled.  When specifying (or querying) CropRect, the value parameter
     *  should be a pointer to an NvRect structure.  Legal values for the
     *  rectangle are integer pixel coordinates.  If the specified crop
     *  rectangle exceeds the image resolution, an error is generated and
     *  the call is ignored.  The default value for the crop rectangle
     *  corresponds to a rectangle which exposes the entire image resolution.
     *  Parameter type: (NvRect * )
     */
    NvMMCameraAttribute_CropRect,

    /* IntermediateColorFormat
     * Use color for buffer between Camera and DZ
     * Use this attribute to override the default of YUV420.
     * Set IntermediateColorFormat to UYVY or RGB
     * This applies to the Encoding buffer only.  The preview buffer
     * is always RGB.  This is for debug use only.
     */
    NvMMCameraAttribute_IntermediateColorFormat,

    /* InitialConvergence
     * If non-zero, up to this many milliseconds will be spent trying to
     * converge the ISP algorithms between when camera is enabled and when
     * the first frames are sent on for preview/capture.  This attribute
     * is ignored if the ISP algorithms are not being run or the input is
     * host mode.
     * Larger values improve the quality of the first several frames, at the
     * cost of an increased startup time.
     * Parameter type: NvU32, default value is 400.
     */
    NvMMCameraAttribute_InitialConvergence,

    /* DirectToSensorGains
     * Write-only.  Sets 4-channel analog gains directly to the sensor, and
     * prevents AE from ever writing the sensor gains again.  After setting
     * this, neither AE nor the EffectiveISO attribute will work correctly.
     * CALIBRATION USE ONLY.  Behavior undefined if used for normal camera.
     * Format: 4 floats, in the order R, Gr, Gb, B
     */
    NvMMCameraAttribute_DirectToSensorGains,

    /* NvMMCameraAttribute_PinEnable indicates which pin will be enabled and
     * which output stream Camera block should expect to be tunnelled.
     * By default, all pins will be enabled and Camera block expects
     * all output streams to be tunnelled.
     * This should be set before Camera block starts buffer negotiation.
     * Parameter type: NvMMCameraPin.
     */
    NvMMCameraAttribute_PinEnable,

    /* Test pattern enable */
    /* When set to NV_TRUE, then the sensor adaptation code will
     * be requested to turn on whatever test pattern it supports
     * for this mode. For instance, it may be color bars.
     * Expected values can then be requested, and a comparison
     * can be done by the application, facilitating any self-check logic.
     * Parameter type: NvBool
     */
    NvMMCameraAttribute_TestPattern,

    /* Test pattern expected values */
    /* Returns values that can be used for comparison for any self-test
     * application using the Test pattern mode. The values are dependent
     * on the current resolution used.
     * Returns a const ptr to an NvMMCameraImagerExpectedValues data
     * structure.
     */
    NvMMCameraAttribute_TestPatternExpectedValues,

    /* Pause preview after still capture
     * When set to NV_TRUE, preview will be put into NvMMCameraPinState_Pause
     * after a still capture or a still burst capture is done. To change
     * preview's state, set NvMMCameraAttribute_Preview to one of
     * NvMMCameraPinState.
     * Parameter type: NvBool
     * Default is NV_FALSE.
     */
    NvMMCameraAttribute_PausePreviewAfterStillCapture,

    /* NvMMCameraAttribute_EXIFInfo
     * Parameter type: NvBool
     */
    NvMMCameraAttribute_EXIFInfo,

    /* NvMMCameraAttribute_EXIFInfoHandle
     * Parameter type: NvRmMemHandle
     */
    NvMMCameraAttribute_EXIFInfoHandle,

    /* NvMMCameraAttribute_CaptureLinked
     * When this attribute is set to value x, camera block sends capture output
     * buffers with OutputStill and OutputVideo true in the buffer's metadata
     * for every x frames. (See NvMMCameraBufferMetadata.)
     * Parameter type: NvU32. Default is 0
     */
    NvMMCameraAttribute_CaptureLinked,
#define NVMMCAMERA_CAPTURELINKED_FIRSTFRAME (0xFFFFFFFF)

    /* NvMMCameraAttribute_AdvancedNoiseReductionMode
     * Set ANR mode.
     * Parameter type: NvU32.
    */
    NvMMCameraAttribute_AdvancedNoiseReductionMode,

    /* ChromaFiltering
     * Set to TRUE to enable chroma filtering
     *         Set to FALSE to disable chroma filtering (default)
     * Returns Error if unable to enable.
     * Parameter type: NvBool.
     * Applies to ISP HW filtering
     */
    NvMMCameraAttribute_ChromaFiltering,

    ////////////////////////////////////////////////////////////////////////
    // Deprecated attributes
    // To be removed when configuration file parsing for new parameters completed
    // and all dependencies converted

    /* ANRFilterParams
     * First entry: gaussian threshold value for luma
     * second entry: gaussian threshold value for chroma
     * useful values:
     *   0 - do no filtering for corresponding color channels
     *   (0,0.2] Used to control how picky we are about considering
     *           data values "similar".  Lower values = more picky
     * Parameter type: NvF32[2]
     * Default: anr.xxxThreshold in the ISP config file */
    NvMMCameraAttribute_ANRFilterParams,

    /* NvMMCameraAttribute_ANRFilterLumaIsoThreshold
     * ANR filtering of the luma channel will only run if the
     * current ISO is greater than or equal to this threshold.
     * A value of zero effectively disables this thresholding.
     * The chroma channel is not affected by this attribute.
     * Parameter type: NvU32. Default is 0
     */
    NvMMCameraAttribute_ANRFilterLumaIsoThreshold,

    /* NvMMCameraAttribute_ANRFilterChromaIsoThreshold
     * ANR filtering of the chroma channel will only run if the
     * current ISO is greater than or equal to this threshold.
     * A value of zero effectively disables this thresholding.
     * The luma channel is not affected by this attribute.
     * Parameter type: NvU32. Default is 0
     */
    NvMMCameraAttribute_ANRFilterChromaIsoThreshold,

    // End of deprecated attributes
    ////////////////////////////////////////////////////////////////////////

    // NvMMCameraAttribute_NrV1LumaEnable
    // Enables V1 (GPU Bilateral) luma processing
    NvMMCameraAttribute_NrV1LumaEnable,

    // NvMMCameraAttribute_NrV1LumaIsoThreshold
    // ANR filtering of the luma channel will only run if the
    // current ISO is greater than or equal to this threshold.
    NvMMCameraAttribute_NrV1LumaIsoThreshold,

    // NvMMCameraAttribute_NrV1LumaThreshold
    // Gaussian threshold value for luma
    NvMMCameraAttribute_NrV1LumaThreshold,

    // NvMMCameraAttribute_NrV1LumaSlope
    // Controls how we increase the thresholds as a function of the
    // gain applied by AE. Thresholds specified above apply when the
    // gain = 1.0, and increase with gain according to these values.
    NvMMCameraAttribute_NrV1LumaSlope,

    // NvMMCameraAttribute_NrV1ChromaEnable
    // Enables V1 (GPU Bilateral) chroma processing
    NvMMCameraAttribute_NrV1ChromaEnable,

    // NvMMCameraAttribute_NrV1ChromaIsoThreshold
    // ANR filtering of the chroma channel will only run if the
    // current ISO is greater than or equal to this threshold.
    NvMMCameraAttribute_NrV1ChromaIsoThreshold,

    // NvMMCameraAttribute_NrV1LumaThreshold
    // Gaussian threshold value for luma
    NvMMCameraAttribute_NrV1ChromaThreshold,

    // NvMMCameraAttribute_NrV1ChromaSlope
    // Controls how we increase the thresholds as a function of the
    // gain applied by AE. Thresholds specified above apply when the
    // gain = 1.0, and increase with gain according to these values.
    NvMMCameraAttribute_NrV1ChromaSlope,

    // NvMMCameraAttribute_NrV2ChromaEnable
    // Enables V2 (Neon Pyramid) chroma processing
    NvMMCameraAttribute_NrV2ChromaEnable,

    // NvMMCameraAttribute_NrV2ChromaSpeed
    // Enables fast 2x2 averafing downsampling instead of 3x3
    NvMMCameraAttribute_NrV2ChromaSpeed,

    // NvMMCameraAttribute_NrV2ChromaIsoThreshold
    // ANR filtering of the chroma channel will only run if the
    // current ISO is greater than or equal to this threshold.
    NvMMCameraAttribute_NrV2ChromaIsoThreshold,

    // NvMMCameraAttribute_NrV2ChromaThreshold
    // NvMMCameraAttribute_NrV2ChromaSlope
    // Adjusts for sensor gain for gain index computation
    NvMMCameraAttribute_NrV2ChromaThreshold,
    NvMMCameraAttribute_NrV2ChromaSlope,

    // NvMMCameraAttribute_NrV2LumaConvolutionEnable
    // Enables V2 5x5 luma convolution
    NvMMCameraAttribute_NrV2LumaConvolutionEnable,

    /* The following attributes are only for development and debug purposes.
     * Use of these is undocumented, and not guaranteed to be supported.
     */
    NvMMCameraAttribute_M3Stats = 0x1000,
    NvMMCameraAttribute_M2Stats,
    NvMMCameraAttribute_Illuminant,
    NvMMCameraAttribute_Gains,
    NvMMCameraAttribute_AutoFocusRegionSharpness,
    NvMMCameraAttribute_BasicAutoFocus,
    NvMMCameraAttribute_AFSweepOffset,
    NvMMCameraAttribute_AFSweepType,
    NvMMCameraAttribute_AFSweepInterleave,
    NvMMCameraAttribute_FocuserCapabilities,
    // Ignoring timeouts is not a production-worthy mode, use
    // only for debug of a sensor that is continually hitting timeouts
    // as a data gathering tool. It is on by default for debug builds,
    // so expect release mode to be less tolerant of hw issues with the sensor
    // Get Attribute of IgnoreTimeouts will report the number of frames
    // errors that were ignored.
    NvMMCameraAttribute_IgnoreTimeouts,
    // When using IgnoreTimeouts, one can set DropBadFrames to false to
    // get the frames that were detected as bad to be dropped.
    NvMMCameraAttribute_DropBadFrames,
    // Internal debug
    NvMMCameraAttribute_IntermediateResults,
    /*
     * Sets the stereo camera method. Must be set before passing resolution
     * information to camera driver. Has no effect for non-stereo camera and
     * will return NV_FALSE.This parameter can be only set to Bool values.
     *
     * Parameter type: NvMMCameraStereoCameraMethodAlternating
     */
    NvMMCameraAttribute_StereoCameraMethodAlternating,

    /*gets the capture sensor mode*/
    NvMMCameraAttribute_CaptureSensorMode,
    /*configures the preview sensor mode*/
    NvMMCameraAttribute_PreviewSensorMode,

    /**
     * SensorHandle attribute. Deprecated and will be removed.
     */
    NvMMCameraAttribute_SensorHandle,

    /* NvMMCameraAttribute_SensorModeList is read-only and gives a list of
     * modes (resolution and frame rate) the current sensor supports. This
     * is an array ofNvMMCameraSensorMode. The number of modes this gives
     * depends on the attribute size (should be multiples of
     * sizeof(NvMMCameraSensorMode). If the asked number of mode, say N,
     * is smaller than the number of supported sensor modes, say M, we
     * will return the largest N modes. If N > M, (N-M) entries will be set
     * to 0.
     * Deprecated.  GUIDs are now used, and this information is no longer
     * available.
     */
    NvMMCameraAttribute_SensorModeList,

     /**
     * NvMMCameraAttribute_CustomizedBlockInfo provides a custom block
     * of info to be messaged to and from odm imager. The block of info
     * which is sent/returned is private and is known to the odm
     * implementation and the caller of this interface.
     * Parameter type : NvMMImagerCustomInfo
     */
    NvMMCameraAttribute_CustomizedBlockInfo,

    /* If non-zero, when the flash fires, White Balance correction will use
     * some mixture of the current settings and the correction factors
     * appropriate for a scene wholly illuminated by the flash module.
     * For example, at .30, it would use 30% of the flash color correction
     * and 70% of the previously-calculated ambient color correction.
     * Note that if the ambient scene was very dark, AWB may choose to
     * mix in more flash correction than specified.
     * Default value: Per config file
     * Parameter type: NvF32
     * Parameter range: [0.0f, 1.0f]
     */
    NvMMCameraAttribute_FlashMixingRatio,

    /* Sync the timestamps camera block generates with this value. This value
     * indicates the elapsed time since capture starts in units of 100 ns. If
     * Base is set in SyncCaptureTimestamp, the elapsed time will be used as
     * the base elapsed time
     *
     * Write-only
     * Parameter type: SyncCaptureTimestamp
     */
    NvMMCameraAttribute_SyncCaptureTimestampElapsedTime,

    /* Write Only Attribute
     * Parameter type: NvU8*
     * Only the specified three strings are allowed :
     *
     * "ae.ConvergeSpeed=%f"
     * "awb.ConvergeSpeed=%f"
     * "ispFunction.lensShading=%s"
     *
     * where : %f is a float value
     *       : %s is a string
     * The float value for
     * - ae.ConvergeSpeed must be [0.01-1.0]
     * - awb.ConvergeSpeed must be [0.01-1.0]
     * The string value for ispFunction.lensShading must be
     *    : TRUE/true to enable lens shading
     *    : FALSE/false to disable lens shading
     * For ex : "ispFunction.lensShading=TRUE"
     *          "ispFunction.lensShading=FALSE"
     *
     * If the ConfigString passed does not "EXACTLY" match any of the
     * above three strings, then an error is returned.
     */
     NvMMCameraAttribute_CameraConfiguration,

    /* Indicate if this is a stereo camera.
     * Read-only
     * Parameter type: NvBool
     */
    NvMMCameraAttribute_StereoCapable,

    /* Select stereo camera mode. Must be set before sensor power on.
     * Has no effect for non-stereo camera and will return NV_FALSE.
     * Parameter type: NvMMCameraStereoCameraMode
     */
    NvMMCameraAttribute_StereoCameraMode,

    /* When stereo is selected, stereo camera capture buffer information
     * is required or only preview with side-by-side is used as default. Must be
     * set before sensor power on and after NvMMCameraAttribute_StereoCameraMode
     * is set.
     * Parameter type: NvMMCameraStereoCaptureInfo
     */
    NvMMCameraAttribute_StereoCaptureInfo,

    /* In capture priority mode, camera block starts to capture a frame when
     * a capture buffer is avaiable and still or video is enabled even when
     * there is no preview buffer. If not in capture priority mode, camera
     * block starts to capture a frame only when it has a buffer for each
     * enabled pin.
     * In capture priority mode, capture frame rate may increase if preview
     * stream is the bottleneck in the pipeline but preview frame rate may
     * decrease because preview may not keep up with video's fill-up rate.
     *
     * Parameter type: NvBool
     * Default: NV_FALSE
     */
    NvMMCameraAttribute_CapturePriority,

    /*
     * Camera focal length in millimeters.
     * Read-only
     * Parameter type: NvF32
     */
    NvMMCameraAttribute_FocalLength,

    /*
     * Horizontal and vertical view angles in degrees.
     * Read-only
     * Parameter type: NvMMCameraFieldOfView
     */
    NvMMCameraAttribute_FieldOfView,

    /*
     * Sets the user selected sensor Mode (resolution and frame rate)
     * The default sensor mode is auto i.e WxHxFramerate is 0x0x0
     * The default auto mode (0x0x0) basically implies that the camera
     * driver picks up the right sensor mode using its own justification.
     * Once the sensor mode is set to user requested one using this attribute,
     * it can be set back to the default auto mode by setting this attribute
     * again with sensor mode 0x0x0.
     *
     * Write-only
     * Parameter type: NvMMCameraSensorMode
     * Default : auto mode i.e WxHxFrameRate as 0x0x0
     */
    NvMMCameraAttribute_SensorMode,

    /*
     * True if sensor has flash capability.
     *
     * Read-only
     * Parameter type: NvBool
     */
    NvMMCameraAttribute_FlashPresent,

    /* Turns ON/OFF VI/ISP pre-scaling for capture output.
     * Several of the Tegra ISP features needs to be disabled
     * when pre-scaling is ON. For instance, EdgeEnhancement and Emboss.
     *
     * Write-only
     * ON by default
     * Parameter type: NvBool.
     */
    NvMMCameraAttribute_PreScalerEnableForCapture,

    /* Turns ON/OFF bayer raw dump at the same time as still capture.
     *
     * OFF by default
     * Parameter type: NvBool
     */
    NvMMCameraAttribute_DumpRaw,

    /* Turns ON/OFF dumped bayer raw to file.
     *
     * OFF by default
     * Parameter type: NvBool
     *     True: dump bayer raw to file in nvmm_camera
     *     False: transfer bayer raw to hal/app using callback
     */
    NvMMCameraAttribute_DumpRawFile,

    /* Turns ON/OFF transfer bayer raw header to hal/app.
     *
     * OFF by default
     * Parameter type: NvBool
     */
    NvMMCameraAttribute_DumpRawHeader,

    /*
     * Specifies the number of buffers that should be used for negative
     * shutter lag.  When set to a value (N), the driver will attempt to
     * allocate N buffers.  If it cannot allocate all N buffers, it will store
     * the number of successfully allocated buffers, which can be queried by a
     * get.  If any buffers are allocated, the driver will immediately begin
     * filling them with still capture data.
     * Clients should query this parameter immediately after
     * setting it to confirm the number of buffers that were successfully
     * allocated.
     *
     * Read/Write
     * Parameter type: NvU32
     * Default: 0
     */
    NvMMCameraAttribute_NumNSLBuffers,

    /* Turns ON/OFF thunmbnail
     * If thumbnail is turned on, each capture buffer's metadata will
     * be marked NVMM_CAMERA_BUFFER_METADATA_OUTPUT_THUMBNAIL
     *
     * Parameter type: NvBool
     * Default: NV_FALSE
     */
    NvMMCameraAttribute_Thumbnail,

    /* Configures Bracket capture.
     *
     * OFF by default
     * Parameter type: NvBracketSettings
     */
    NvMMCameraAttribute_BracketCapture,

     /*
     * Queries sensor ISP support from ODM
     *
     * Read-only
     * Parameter type: NvBool
     * Default: NV_TRUE
     */
    NvMMCameraAttribute_SensorIspSupport,

     /*
     * Queries sensor bracket capture support
     *
     * Read-only
     * Parameter type: NvBool
     */
    NvMMCameraAttribute_SensorBracketCaptureSupport,

    /*
     * Sets the user selected Speed for video Mode (0.25x/0.5x/1x/2x/3x/4x)
     * The default video speed is 1x i.e. all the frames delivered by the
     * sensor will be passed on video port. 2x means alternate frames will be
     * skipped and so on...
     *
     * Write-only
     * Parameter type: NvMMCameraVideoSpeed
     * Default : 1 ie. normal speed
     */
    NvMMCameraAttribute_VideoSpeed,

     /*

     * Queries the remaining still images of last capture
     *
     * Read-only
     * Parameter type: NvU32
     * Default: 0
     */
    NvMMCameraAttribute_RemainingStillImages,

    /*
     * Sets the Current capture type to either Still or Video Capture.
     * The default capture type is still capture.
     * Write-only
     * Parameter type: NvMMCameraCaptureType
     * Default :  NvMMCameraCaptureType_Still
     */
    NvMMCameraAttribute_CaptureType,

    NvMMCameraAttribute_CaptureMode,

    /*
     * Specify how many faces the user wants.
     *
     * Read/Write
     * Parameter type: NvS32
     */
    NvMMCameraAttribute_FDLimit,

    /*
     * Specify how many faces can be detected at most.
     *
     * Read-only
     * Parameter type: NvS32
     */
    NvMMCameraAttribute_FDMaxLimit,

    /*
     * Turn FD debug mode on which displays box-shape indicators
     * around faces.
     *
     * Write-only
     * Parameter type: NvBool
     */
    NvMMCameraAttribute_FDDebug,

    /*
     * Get number of detected / tracked faces.
     *
     * Read-only
     * Parameter type: NvS32
     */
    NvMMCameraAttribute_NumFaces,

    /*
     * Get the first face from the ordered list.
     *
     * Read-only
     * Parameter type: NvS32
     */
    NvMMCameraAttribute_FaceRect,

    /*
     * Set a special FD operation mode for test.
     *
     * Write-only
     * Parameter type: NvBool
     */
    NvMMCameraAttribute_FDTest,

    /*
     * Specifies the number of NSL frames to be dropped between each frame
     * that is stored to the NSL buffer.
     *
     * Read/Write
     * Parameter type: NvU32
     * Default: 0
     */
    NvMMCameraAttribute_NSLSkipCount,

    /*
     * Specifies current camera rotation.
     *
     * Write only
     * Parameter type: NvS32
     * Default: 0 (0 degree)
     */
    NvMMCameraAttribute_CommonRotation,

    /*
     * Set the still captue in passthrough mode.
     *
     * Write-only
     * Parameter type: NvBool
     */
#define NVMMCAMERA_ENABLE_STILL_PASSTHROUGH (1<<0)
    NvMMCameraAttribute_StillPassthrough,

    /*
     * Set still capture sensor mode even if its size is smaller than
     * the preview sensor mode during buffer negotiation.
     *
     * Write-only
     * Parameter type: NvBool
     */
    NvMMCameraAttribute_CaptureSensorModePref,

    /*
     * Allows for HAL call to power up sensor explicitly
     *
     * Write - writing triggers power up
     * Parameter type: NvBool (no effect)
     */
    NvMMCameraAttribute_SensorPowerOn,

    /* This flag is set when taking fast burst captures
     * This mode would influence the AE breakdown logic
     * accordingly to select faster exposures.
     */
    NvMMCameraAttribute_FastBurstMode,

    NvMMCameraAttribute_CameraMode,

    NvMMCameraAttribute_ForcePostView,

    /*
     * Allows for HAL call to set concurrent capture request
     *
     * Write - writing triggers concurrent capture
     * Parameter type: NvU32 (no of images)
     */
    NvMMCameraAttribute_ConcurrentCaptureRequests,

    /*
     * Disable returning buffers to input stream automatically.
     *
     * Write-only
     * Parameter type: NvBool
     */
    NvMMCameraAttribute_EnableInputBufferReturn,

    /*
     * Set Video Port surface Layout.
     *
     * Write-only
     * Parameter type: NvRmSurfaceLayout
     */
    NvMMCameraAttribute_VideoSurfLayout,

    /*
     * Enable the use of an external buffer manager.
     * When using an external manager, the nvmm layer should no longer
     * allocate buffers, but simply communicate buffer configurations
     * to the manager.
     *
     * Read/Write (as almost ALL of these should be)
     * Parameter type: NvBool
     * Default: NV_FALSE
     */
    NvMMCameraAttribute_ExternalBufferManager,

    /*
     * Enable to help camera Suggest Buffer Config Based on the
     * Activity of NSL and Burst Captures
     */
    NvMMCameraAttribute_NSLDirtyBuffers,

    /*
     * attribute used as backdoor to pass Trident pipeline from HAL into driver-land
     */
    NvMMCameraAttribute_TridentPipeline,

    /*
     * Tell the driver whether it should run video stabilization in lean mode
     *
     * Read/write
     * Parameter type: NvBool
     * Deafult: NV_FALSE
     */
    NvMMCameraAttribute_VideoStabilizationLeanMode,

    /*
     * On chips with 2 ISPs this will enable using both ISPs to capture still/preview
     * output streams simultaneously. This will save power at the cost of not being
     * able to use the second ISP on a different input stream.
     *
     * Read/Write
     * Parameter type: NvBool
     * Default: NV_FALSE
     */
    NvMMCameraAttribute_UseBothISPs,

    /* Indicator whether a Pass Through mode is supported from sensor
     * Read Only
     * Parameter type: NvBool
     */
    NvMMCameraAttribute_IsPassThroughSupported,

    /* Indicator whether a video recording is about to start. Clients can use
     * this to indicate that images on the output port will most likely be
     * video frames and not stills. Note that even during video recording, still
     * captures can be requested, so this is really just a hint.
     * Write Only
     * Parameter type: NvBool
     */
    NvMMCameraAttribute_RecordingHint,

    /* Max should always be the last value, used to pad the enum to 32bits */
    NvMMCameraAttribute_Force32 = 0x7FFFFFFF
} NvMMCameraAttribute;

#define NVMM_CAM_UNASSIGNED_BUFFER_ID (0xFFFFFFFF)

/**
 * @brief Defines events that will be sent from codec to client.
 * Client can use these events to take appropriate actions
 * @see SendEvent
 * @ingroup nvmmcamerablock
 */
typedef enum
{
    /**
     * @brief Alert shim when algorithms have converged or timed out.
     * TODO: If SomeFlag is set, the AlgorithmsLocked event will be sent for
     * any condition that causes one or more algorithms to lock.
     * Else, each algorithm will send its respective "Achieved" event if it
     * converged within the time limit, or "TimedOut" event if it did not.
     */
    NvMMEventCamera_AlgorithmsLocked = NvMMEventCamera_EventOffset,
    NvMMEventCamera_AutoFocusAchieved,
    NvMMEventCamera_AutoExposureAchieved,
    NvMMEventCamera_AutoWhiteBalanceAchieved,
    NvMMEventCamera_AutoFocusTimedOut,
    NvMMEventCamera_AutoExposureTimedOut,
    NvMMEventCamera_AutoWhiteBalanceTimedOut,
    NvMMEventCamera_CaptureAborted,
    NvMMEventCamera_CaptureAborted_FlashLimitExceeded,
    NvMMEventCamera_CaptureStarted,

    /**
     * @brief Notify shim that it can send another still capture request.
     */
    NvMMEventCamera_StillCaptureReady,

    /**
     * @brief Notify shim that it can send another video capture request.
     */
    NvMMEventCamera_VideoCaptureReady,

    /**
     * @brief Notify shim that a still capture has ocurred and is being
     * processed.  This can be used to play a shutter sound, etc.
     */
    NvMMEventCamera_StillCaptureProcessing,

    /**
     * @brief Notify shim that preview is paused after still capture is done
     */
    NvMMEventCamera_PreviewPausedAfterStillCapture,

    /**
     * @brief Notify shim that Camera has finished buffer (re)negotiation on one of its streams
     */
    NvMMEventCamera_StreamBufferNegotiationComplete,

    /**
     * @brief Notify shim that Camera has finished raw buffer dump
     */
    NvMMEventCamera_RawFrameCopy,

    /**
     * @brief Notify shim that a sensor mode change has been completed.
     */
    NvMMEventCamera_SensorModeChanged,

    /**
     * @brief Notify shim that Camera has entered low light conditions
     */
    NvMMEventCamera_EnterLowLight,

    /**
     * @brief Notify shim that Camera has exited low light conditions
     */
    NvMMEventCamera_ExitLowLight,

    /**
     * @brief Notify shim that Camera has entered macro mode
     */
    NvMMEventCamera_EnterMacroMode,

    /**
     * @brief Notify shim that Camera has exited macro mode
     */
    NvMMEventCamera_ExitMacroMode,

    /**
     * @brief Notify shim that powering on VI/ISP is complete
     */
    NvMMEventCamera_PowerOnComplete,

    /**
     * @brief Notify shim that Camera has start moving focuser
     */
    NvMMEventCamera_FocusStartMoving,

    /**
     * @brief Notify shim that Camera has stopped moving focuser
     */
    NvMMEventCamera_FocusStopped,

    /**
     * @brief Notify shim that NSL stop is complete
     */
    NvMMEventCamera_NSLStopped,

    /**
     * @brief Notify shim that raw dump triggered by
     *        NvMMCameraExtension_CaptureRaw (i.e. non-concurrent raw capture)
     *        is complete.
     */
    NvMMEventCamera_CaptureRawDone,

    NvMMCameraEvent_Force32 = 0x7FFFFFFF
} NvMMCameraEvents;

/**
 * @brief Camera Interface
 * Use the NvMM Block 'Extension' to call the
 * function listed in this enum.
 * @see Extension
 */
typedef enum
{
    NvMMCameraExtension_ConvergeAndLockAlgorithms,
    NvMMCameraExtension_UnlockAlgorithms,
    NvMMCameraExtension_StatusAlgorithms,
    NvMMCameraExtension_CaptureStillImage,
    NvMMCameraExtension_CaptureVideo,
    NvMMCameraExtension_CaptureRaw,

    /* Use NvMMCameraResetBufferNegotiation with this extension.
     * With this extension, the buffers allocated for this stream
     * will be freed and the buffer negotiation for the stream will be reset.
     * This has to be called before buffer renegotiation.
     */
    NvMMCameraExtension_ResetBufferNegotiation,

    /* Private, move to internal header? */
    NvMMCameraExtension_SensorRegisterDump, // for calibration process

    /**
     * Start face detection
     */
    NvMMCameraExtension_TrackFace,

    /*
     * EnablePreviewOut configs nvmm_camera to pass preview buffers to DZ
     * when enabling; it configs nvmm_camera to swallow preview buffers when
     * disabling. It can only be true/false.
     *
     * Write-only
     */
    NvMMCameraExtension_EnablePreviewOut,

    NvMMCameraExtension__Force32 = 0x7FFFFFFF,
} NvMMCameraExtension;

typedef NvOdmImagerCustomInfo NvMMImagerCustomInfo;

/*
 * A method to do the camera device auto-detection.
 */
NvBool NvMMCameraDeviceDetect(void);

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_CAMERA_H
