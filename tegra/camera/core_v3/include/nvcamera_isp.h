/*
 * Copyright (c) 2008-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVCAMERA_ISP_H
#define INCLUDED_NVCAMERA_ISP_H

/**
 * @file
 * <b>NVIDIA Camera: Camera Driver Definitions</b>
 *
 * @b Description: Defines common datatypes and enums used by NVIDIA
 * Camera Driver.
 */

#if defined(__cplusplus)
extern "C"
{
#endif

/*
 * This header is split into sections: Camera ISP Attributes, Attributes to
 * Set/Get, Data Types for Attributes to Set/Get.
 */

#define NVCAMERAISP_AUTO         (-1L)

#define NVCAMERAISP_AUTO_FLOAT   (-1.f)

/**
 * @defgroup nvcamera_isp_group Camera Driver Definitions
 *
 * Camera controls like color modes, exposure, and focus, are passed
 * to the ODM adaptations for YUV sensors whenever the Tegra Image Signal
 * Processor (ISP) is not used. Architecturally, the NVIDIA camera solution
 * has supported the Tegra ISP, internal to our chip. This is seen with sensors
 * that provide Bayer output that is then processed into YUV inside the Tegra
 * processor. Historically, the support for YUV sensor (sensors that have their
 * own ISP and therefore provide only the YUV pixels) has been sufficient
 * only to capture the images and pass them on to encoders and the viewfinder.
 *
 * This file defines types shared by the NVIDIA Multimedia Camera (nvmm_camera)
 * code and the NVIDIA ODM Imager code, so that external ISP designs can
 * get the same settings and controls that the NVIDIA ISP gets. For instance,
 * if Sepia is chosen or if an exposure is manually set by an application, this
 * can be passed to an ODM Imager adaptation for a YUV sensor.
 *
 * Applications have access to the supported camera parameters and data types
 * defined in this header. The following shows an example inside the
 * \c SetParameter() function in an imager adaptation:
 *
 * @code
 *
 * switch(Param)
 * {
 *     case NvOdmImagerParameter_ISPSetting:
 *         if (pValue &&
 *             SizeOfValue == sizeof(NvOdmImagerISPSetting))
 *         {
 *             NvOdmImagerISPSetting *pSetting =
 *                 (NvOdmImagerISPSetting *)pValue;
 *
 *             LOGD("imager: ISP Setting: 0x%x %d\n",
 *                  pSetting->attribute, pSetting->dataSize);
 *
 *             switch(pSetting->attribute)
 *             {
 *                 case NvCameraIspAttribute_FlickerFrequency:
 *                 break;
 *             }
 *          }
 *          break;
 * }
 * @endcode
 *
 * @ingroup camera_modules
 * @{
 */

/** @name Camera ISP Attributes
 */
/*@{*/

/**
 * Defines camera ISP attributes.
 *
 * These attributes are specific to the image processing functionality
 * that would be handled in either the NVIDIA camera driver for image
 * processing or be passed to the nvodm_imager adaptation if the sensor
 * is YUV in type. (External ISP used.)
 *
 * The description of each attribute will indicate the name of the
 * data type to use and whether it is read only, write only, or read/write.
 *
 * Attributes to set/get beginning at ::NvCameraIspAttribute_WhiteBalanceCCTRange
 * take data structures to set/get the values.
 */
typedef enum
{
    /**
     * Offset so these definitions do not intersect the existing nvmm_camera
     * attributes.
     *
     * - Do not change.
     */
    NvCameraIspAttribute_ImageProcessingOffset = 0x4000,

    /**
     * Enabling anti-flicker causes the ISP to compensate for visible
     * flicker caused by AC signals (50 and/or 60 Hz). By default, the ISP
     * does not attempt to determine the AC frequency automatically;
     * applications may specify the AC frequency by calling \c SetAttribute
     * with the \c FlickerFrequency attribute.
     *
     * - Parameter type: NvBool.
     */
    NvCameraIspAttribute_AntiFlicker,

    /**
     * Enabling auto frame-rate control allows the ISP to adjust the sensor
     * frame rate in low-lux situations, to allow longer exposure times.
     * Applications may limit the frame-rate variations by calling
     * \c SetAttribute to set the allowable frame rate range.
     *
     * - Parameter type: NvBool.
     */
    NvCameraIspAttribute_AutoFrameRate,

    /**
     * Edge-enhancement increases the contrast between object edges in the
     * image. Turn edge enhancement on/off with this attribute. The strength
     * of the enhancement can be set by calling \c SetAttribute with the
     * \c EdgeEnhanceStrength parameter.
     *
     * - Parameter type: NvBool.
     */
    NvCameraIspAttribute_EdgeEnhancement,

    /**
     * Enabling \c Stylize causes the resulting image to be processed by
     * a stylize filter (e.g., emboss, negative, color transformations).
     * The type of filter can be chosen by calling \c SetAttribute with
     * the \c StylizeMode parameter.
     *
     * - Parameter type: NvBool.
     */
    NvCameraIspAttribute_Stylize,

    /**
     * Enabling continuous auto-focus results in the focuser continually
     * adjusting to get the optimally focused imaged. If not supported,
     * enabling this attribute returns an error. Use
     * \c NvmmCameraExtension_ConvergeAndLockAlgorithms to trigger final
     * focusing before image capture and locking it.
     *
     * - Parameter type: NvBool.
     */
    NvCameraIspAttribute_ContinuousAutoFocus,

    /**
     * Pause continuous auto-focus in its current state and focus position.
     * This differs from \c NvmmCameraExtension_ConvergeAndLockAlgorithms in
     * that it will not trigger an additional focus search. Un-pause to
     * start continuous auto focus again. No effect when continuous autofocus
     * is disabled.
     *
     * - Parameter type: NvBool.
     */
    NvCameraIspAttribute_ContinuousAutoFocusPause,

    /**
     * Read only. Report continuous auto-focus searching state. If continuous
     * auto-focus is paused, last status before the pause is reported.
     * Return NvError_InvalidState if continuous auto focus is disabled.
     * - Parameter type: NvU32.
     */
    NvCameraIspAttribute_ContinuousAutoFocusState,
#define NVCAMERAISP_CAF_CONVERGED      (1)
#define NVCAMERAISP_CAF_SEARCHING      (2)


    /**
     * @deprecated Please use ::NvCameraIspAttribute_ExposureMode.
     */
    NvCameraIspAttribute_ContinuousAutoExposure,

    /**
     * @deprecated Please use ::NvCameraIspAttribute_WhiteBalanceMode.
     */
    NvCameraIspAttribute_ContinuousAutoWhiteBalance,

    /**
     * Enabling \c AutoNoiseReduction will allow the imager to automatically
     * select and apply noise reduction algorithms to the image, based
     * on the amount of gain and exposure time used to capture the image.
     *
     * - Parameter type: NvBool.
     */
    NvCameraIspAttribute_AutoNoiseReduction,

    /**
     * The effective ISO is the sensor's gain multiplied by its nominal ISO
     * sensitivity. Although this can be calculated separately by querying the
     * gain, this parameter is provided in case an application does not know
     * the nominal sensitivity. Legal values for this parameter are defined
     * ISO values between the minimum and maximum supported ISO values for
     * the current camera mode.
     *
     * Additionally, if the ::NvCameraIspISO.isAuto flag is set,
     * it will result in the auto-exposure algorithm computing the ISO
     * internally. When ISO is set to auto (the default value), querying
     * this parameter will return the effective ISO value
     * that has been computed by the auto-exposure algorithm.
     *
     * - Parameter size/type: NvCameraIspISO, as listed below.
     */
    NvCameraIspAttribute_EffectiveISO,
#define NVCAMERAISP_ISO_50     (50L)
#define NVCAMERAISP_ISO_100    (100L)
#define NVCAMERAISP_ISO_200    (200L)
#define NVCAMERAISP_ISO_400    (400L)
#define NVCAMERAISP_ISO_800    (800L)
#define NVCAMERAISP_ISO_1600   (1600L)
#define NVCAMERAISP_ISO_3200   (3200L)

    /**
     * The exposure time is the time (in seconds) that the camera shutter
     * opens to expose the film (or CMOS/CCD, in digital cameras). For
     * electronic shutters, this value will be translated into a number
     * of scanlines (and framerate) internally. Legal values for this
     * parameter are floating-point values between the minimum and
     * maximum exposure time.
     *
     * Additionally, if the ::NvCameraIspExposureTime.isAuto flag is set,
     * it will result in the auto-exposure algorithm computing the exposure
     * time internally. When exposure time is set to auto (the
     * default value), querying this parameter will return the exposure time
     * that has been computed by the auto-exposure algorithm, before exposure
     * compensation is applied. Units for this parameter are seconds, constant
     * values for typical exposure times are provided for convenience.
     *
     * @note If auto-frame rate is unable to adjust the sensor frame rate to
     * accommodate the specified exposure time (because auto frame-rate has
     * been disabled or the auto-frame rate limits would be exceeded), the
     * exposure time will be clamped to the maximum possible value.
     *
     * - Parameter size/type: NvCameraIspExposureTime.
     */
    NvCameraIspAttribute_ExposureTime,

    /**
     * This sets the gains, exposure, short exposure, and/or exposure ratio
     * to AE in one call.
     */
    NvCameraIspAttribute_AEOverride,

    /**
     * Setting the flicker-frequency controls the camera's anti-flicker behavior
     * (when anti-flicker is enabled). Supported values are \c FLICKER_AUTO
     * (which will auto-detect the flicker band), \c FLICKER_50HZ (50 Hz flicker
     * band compensation) and \c FLICKER_60HZ (60 Hz flicker band compensation).
     *
     * - The default value is \c FLICKER_AUTO.
     * - Parameter size/type: NvS32, as listed below.
     */
    NvCameraIspAttribute_FlickerFrequency,
#define NVCAMERAISP_FLICKER_50HZ   (1L)
#define NVCAMERAISP_FLICKER_60HZ   (2L)
#define NVCAMERAISP_FLICKER_AUTO   (3L)

    /**
     * White balance compensation causes the auto-white balance
     * algorithm to bias its selected illuminant by the specified value.
     * Legal values for \c WhiteBalanceCompensation are signed integers
     * in degrees K.
     *
     * - The default value is 0.
     * - Parameter size/type: NvS32, as listed below.
     */
    NvCameraIspAttribute_WhiteBalanceCompensation,
#define NVCAMERAISP_WHITEBALANCECOMPENSATION_NONE       (0L)
#define NVCAMERAISP_WHITEBALANCECOMPENSATION_PLUS_500K  (500L)
#define NVCAMERAISP_WHITEBALANCECOMPENSATION_MINUS_500K (-500L)

    /**
     * Exposure compensation causes the auto-exposure algorithm to bias
     * its computed scene exposure by the specified value. Legal values for
     * this parameter are fixed point numbers, with units in f/stops.
     *
     * @note Exposure compensation will be applied in both auto-exposure and
     * manual exposure modes. If ISP auto-frame rate control can not
     * adjust the frame rate to support the specified exposure time
     * (because auto frame-rate is disabled or the specified auto frame
     * rate limits would be exceeded), the exposure will be clamped to the
     * maximum supported amount, and gain will be applied for any
     * remaining intensity adjustment.
     *
     * - The default value is 0.
     * - Parameter size/type: NvSFX. Use macro defined below.
     */
    NvCameraIspAttribute_ExposureCompensation,
#define NVCAMERAISP_EXPOSURECOMPENSATION_NONE NV_SFX_FP_TO_FX((0.f))
#define NVCAMERAISP_EXPOSURECOMPENSATION_PLUS_HALF_FSTOP NV_SFX_FP_TO_FX((0.5f))
#define NVCAMERAISP_EXPOSURECOMPENSATION_MINUS_HALF_FSTOP NV_SFX_FP_TO_FX((-0.5f))

    /**
     * Metering-mode controls how the ISP computes the proper exposure time
     * and gain for the scene. Legal values are \c METERING_SPOT,
     * \c METERING_CENTER, and \c METERING_MATRIX. Changes to this
     * value will take effect the next time exposure is computed. This setting
     * has no effect if exposure is specified manually.
     *
     * - The default value is \c METERING_MATRIX.
     */
    NvCameraIspAttribute_MeteringMode,
#define NVCAMERAISP_METERING_SPOT       (1L)
#define NVCAMERAISP_METERING_CENTER     (2L)
#define NVCAMERAISP_METERING_MATRIX     (3L)

    /**
     * When the ISP stylize function is enabled, a variety of post-processing
     * special effects may be applied to the image prior to returning it to
     * the rest of the video pipeline. Legal values are \c COLOR_MATRIX,
     * \c EMBOSS, and \c NEGATIVE. \c COLOR_MATRIX multiplies the image (in
     * RGB, prior to YUV conversion) by a user-specified 3x3 matrix. The matrix
     * can be set by calling \c SetAttribute with the \c StylizeColorMatrix
     * parameter vector. \c EMBOSS embosses the image; the strength of the
     * effect can be controlled by calling \c SetAttribute with the
     * \c EmbossStrength parameter. \c SOLARIZE solarizes the image.
     *
     * - The default value is \c COLOR_MATRIX.
     * - Parameter size/type: NvU32. Use macro defined below.
     */
    NvCameraIspAttribute_StylizeMode,
#define NVCAMERAISP_STYLIZE_COLOR_MATRIX   (1L)
#define NVCAMERAISP_STYLIZE_EMBOSS         (2L)
#define NVCAMERAISP_STYLIZE_NEGATIVE       (3L)
#define NVCAMERAISP_STYLIZE_SOLARIZE       (4L)
#define NVCAMERAISP_STYLIZE_POSTERIZE      (5L)
#define NVCAMERAISP_STYLIZE_SEPIA          (6L)
#define NVCAMERAISP_STYLIZE_BW             (7L)
#define NVCAMERAISP_STYLIZE_AQUA           (8L)

    /**
     * \c LumaBias controls the amount to add or subtract to the luma channel,
     * when YUV conversion is enabled. This value is added to the Y channel
     * after the YUV conversion matrix has been applied.
     * Legal values are integers ranging from -128 to 127. Any values
     * specified outside of the supported range will be clamped, and no error
     * generated. The default value is suitable for transforming
     * gamma-corrected sRGB colors into ITU-R BT.601 YUV.
     *
     * - Parameter type: NvS32
     */
    NvCameraIspAttribute_LumaBias,

    /**
     * \c EdgeEnhanceStrengthBias controls the bias of the strength of the
     * edge-enhancement effect, when edge enhancement is enabled. Legal values
     * are floats ranging from -1 to 1. When edge enhancement is
     * disabled, changing this parameter has no effect on the resulting
     * image. Any value specified outside of the supported range will be
     * clamped, and no error generated.
     *
     * - The default value is 0.
     * - Parameter type: NvF32
     */
    NvCameraIspAttribute_EdgeEnhanceStrengthBias,

    /**
     * \c EmbossStrength controls the strength of the emboss effect, when ISP
     *  stylize is enabled and the stylize mode is set to \c EMBOSS.  Legal
     *  values are floats ranging from the minimum emboss strength to
     *  the maximum with a certain size step. When stylize is disabled, or set
     *  to a different mode, changing this parameter has no effect on the
     *  resulting image. Any value specified outside of the supported range
     *  will be clamped, and no error generated.
     *
     * - The default value is the minimum emboss strength.
     * - Parameter type: NvF32.
     */
    NvCameraIspAttribute_EmbossStrength,

    /**
     * \c ImageGamma specifies the gamma-correction curve which should be
     * applied to the image RGB values (after stylizing, prior to YUV
     * conversion). Legal values are positive fixed point
     * numbers, plus the special value \c NVCAMERAISP_IMAGEGAMMA_SRGB
     * (corresponding to a gamma of approximately 0.45).
     *
     * - The default value is \c NVCAMERAISP_IMAGEGAMMA_SRGB.
     * - Parameter type: NvSFX.
     */
    NvCameraIspAttribute_ImageGamma,
#define NVCAMERAISP_IMAGEGAMMA_SRGB   (0L)
#define NVCAMERAISP_IMAGEGAMMA_LINEAR (NV_SFX_ONE)

    /**
     * \c ContrastEnhancement may be applied to the gamma-correction curve,
     * boosting the high end and reducing the low end. This is actually a bias
     * value applied on top of the contrast enhancement value calculated by the
     * camera driver. Legal values are floating point numbers between negative
     * one and positive one, inclusive. Zero specifies that the value calculated
     * by the camera driver should be used. Negative one specifies the least
     * amount of contrast enhancement and positive one the most. Fractional
     * values result in interpolation. So a value of .5 would specify the amount
     * of contrast enhancement that is half-way between the value calculated by
     * the camera driver and the maximum value.
     *
     * - The default value is 0.0.
     * - Parameter type: NvF32.
     */
    NvCameraIspAttribute_ContrastEnhancement,

    /**
     * \c FocusPosition: read/write. On write, instructs the imager to
     * set lens focus to specified fixed position. On read, reports
     * current position. A write immediately followed by read may not
     * report the set value, since the focuser must finish seeking
     * before it reports that position. Fixed position
     * \c NVCAMERAISP_FOCUSPOS_INFINITE corresponds to "infinity" focus
     * setting.
     *
     * @note
     * - Calling \c NvmmCameraExtension_ConvergeAndLockAlgorithms with
     * the \c Focus flag set will overwrite this focus position.
     * - Additionally, setting this attribute while there is an outstanding
     * call to \c NvmmCameraExtension_ConvergeAndLockAlgorithms will have
     * undefined results.
     * - Imager will not necessarily keep manually set focus position
     * indefinitely. Focus could be reset to the most power-efficient
     * position after a certain period of inactivity.
     *
     * - Parameter type: NvS32
     */
    NvCameraIspAttribute_FocusPosition,
#define NVCAMERAISP_FOCUSPOS_INFINITE    0

    /**
     * Read-only attribute used to query for camera focusing parameters:
     * the range of lens focus positions which could be set by
     * ::NvCameraIspAttribute_FocusPosition attribute, as well as
     * specific values for \c Hyperfocal and \c Macro positions.
     *
     * Attribute value must be a pointer to an ::NvCameraIspFocusingParameters
     * structure.
     *
     * @note Lens focus positions defined by:
     * @code
     *
     * [ NvCameraIspFocusingParameters.minPos,
     *      NvCameraIspFocusingParameters.maxPos ]
     * @endcode
     * are manually set positions, that is, value \c NVCAMERAISP_FOCUSPOS_AUTO
     * is guaranteed not to be included in this range,
     * and value \c NVCAMERAISP_FOCUSPOS_INFINITE is guaranteed to be included.
     */
    NvCameraIspAttribute_FocusPositionRange,

// Attributes to Set/Get

    /**
     * Auto white balance searches a large set of illuminants to
     * estimate the correlated color temperature (CCT) of a scene. This range
     * can be limited to a subset of the defined illuminants by setting the
     * \c WhiteBalanceCCTRange parameter vector (this is used to implement white
     * balance modes, such as "Sunny" and "Cloudy"). Changes to this parameter
     * will take effect the next time the image white balance is computed.
     * When specifying (or querying) \c WhiteBalanceCCTRange, the value
     * Parameter must be a pointer to an ::NvCameraIspRange structure. The
     * low and high values in the range specify the minimum and maximum CCT
     * values to be searched. Legal values are integers in units of degrees K.
     * To select a specific illuminant, the low and high members should be set
     * to identical values. An error is returned if high is less than low.
     * By default, the low range value is 0, and the high value is \c MAX_INT.
     *
     * - Parameter type: (NvCameraIspRange * )
     */
    NvCameraIspAttribute_WhiteBalanceCCTRange,

    /**
     * The auto-frame rate upper and lower bounds specify the maximum (and
     * minimum) frame rates that may be selected by the auto-frame rate
     * algorithm. Changes to this parameter vector will take effect the next
     * time exposure is computed (either by continuous auto-exposure or a call
     * to ComputeAlgorithms). This setting has no effect if auto-frame rate
     * control is disabled. When specifying (or querying) \c AutoFrameRateRange,
     * the value parameter must be a pointer to an ::NvCameraIspRange struct.
     * Legal values for the low and high fields are S15.16 fixed-point numbers
     * ranging from the minimum to the maximum frame rate.
     *
     * - Default low value is the minimum framerate, and high value
     *   is the maximum.
     * - Parameter type: (NvCameraIspRange * )
     */
    NvCameraIspAttribute_AutoFrameRateRange,


    /**
     * When stylize is enabled and the stylize mode is set to \c COLOR_MATRIX,
     * the \c StylizeColorMatrix parameter provides a 3x3 matrix that is used
     * to transform each pixel in the image. This can be used to create
     * special colorization effects such as sepia and antique. When
     * specifying (or querying) \c StylizeColorMatrix, the value parameter
     * must be a pointer to an ::NvCameraIspMatrix structure. Legal values
     * for the matrix elements are S15.16 fixed-point numbers. This matrix is
     * applied prior to YUV conversion (if enabled), so it should always be
     * specified as an RGB-to-RGB transformation. If any values in the
     * matrix are outside of the ISP's supported range, they will be silently
     * clamped, and no error generated. The default value is an identity
     * transform.
     *
     * - Parameter type: (NvCameraIspMatrix * )
     */
    NvCameraIspAttribute_StylizeColorMatrix,

    /**
     * Read-only. Returns the default YUV conversion matrix used by NvMM.
     * This is provided for convenience, in case the user wishes to tweak
     * the original matrix in one way, then in another way, but doesn't
     * want to store off the original matrix before changing it.
     *
     * - Parameter type: (NvCameraIspMatrix * )
     */
    NvCameraIspAttribute_DefaultYUVConversionMatrix,

    /**
     * To provide users with visual feedback about auto-focus behavior,
     * the \c AutoFocusRegions parameter may be queried to determine
     * the placement of the ISP's auto-focus regions. When querying this
     * Parameter, the value must be a pointer to ::NvCameraIspFocusingRegions
     * structure, which will be filled with actual number of focusing regions
     * (N) and N \c NvRectF32 structures describing each focusing region.
     * The floating values in \c NvRectF32 are normalized to the [-1.0, 1.0]
     * range, and are expressed relative to the active cropping rectangle.
     * This parameter is read/write, and when used in \c SetAttribute,
     * it allows setting focusing regions configuration to be used by camera.
     *
     * @note On fixed-focus camera modules,
     * the single auto-focus region covers the entire cropping rectangle.
     */
    NvCameraIspAttribute_AutoFocusRegions,

    /**
     * \c AutoFocusRegionMask specifies a subset of focus regions to use
     * for subsequent focusing operations. This parameter
     * value must be a pointer to ::NvCameraIspActiveFocusingRegions
     * structure, which will be filled with actual number of focusing
     * regions (numOfRegions) and numOfRegions NvBool values - NV_TRUE for each
     * focusing region included currently in the active subset, NV_FALSE for all
     * excluded regions. Order of focusing regions is that of the order
     * specified by the latest \c SetAttribute call for
     * ::NvCameraIspAttribute_AutoFocusRegions (or the order of default
     * Camera focusing regions).
     * This parameter is read/write, and when used in \c SetAttribute,
     * it allows a change to an active subset of focusing regions from existing
     * focusing regions configuration.
     *
     * @note The camera block can limit the number of active focusing regions
     * (for example, allow a single region to be active at any given time).
     */
    NvCameraIspAttribute_AutoFocusRegionMask,

    /**
     * \c AutoFocusRegionStatus indicates the focus status (in- or out-of-focus)
     * for each of the focus regions. When querying this parameter, the value
     * must be a pointer to an array of \c NVCAMERAISP_MAX_FOCUSING_REGIONS
     * NvBools, which will be filled with Booleans indicating whether the
     * region is in- or out-of-focus (NV_TRUE indicates in-focus, NV_FALSE
     * indicates out-of-focus). This parameter is read-only, and an
     * assert will be encountered in debug mode if the application attempts
     * to set these values via \c SetAttribute.
     *
     * @note On fixed-focus camera modules, the
     * single auto-focus region will always be in focus.
     */
    NvCameraIspAttribute_AutoFocusRegionStatus,

    /**
     * Hue rotates the colors around a gray axis.
     * float type, range [0,360) in degrees.
     *
     * - Default value is 0. Changing the value will have no effect if the
     * sensor input is YUV format.
     * - Parameter type: NvF32
     */
    NvCameraIspAttribute_Hue,

    /**
     * Saturation boosts or mutes the colors.
     * float type, the effect is:
     * <pre>
     *   < 0    Invert colors
     *   = 0    Grayscale
     *   0 - 1  Desaturate colors
     *   1      No change
     *   > 1    Boost saturation
     * </pre>
     *
     * - Default value is 1 (no change). Changing the value will have no effect
     * if the sensor input is YUV format.
     * - Parameter type: NvF32
     */
    NvCameraIspAttribute_Saturation,

    /**
     * \c Brightness either boosts the entire image towards white (if positve)
     * or towards black (if negative). float type, range -1 to 1
     *
     * - Default is zero (no change). Changing the value will have no effect if
     * the sensor input is YUV format.
     * - Parameter type: NvF32
     */
    NvCameraIspAttribute_Brightness,

    /**
     * \c Warmth rotates the colors around a green axis.
     * float type, range [-10,10] in degrees.
     *
     * - Default value is 0. Changing the value will have no effect if the
     * sensor input is YUV format.
     * - Parameter type: NvF32
     */
    NvCameraIspAttribute_Warmth,

    /**
     * \c Stabilization has the value \c None or \c Image. By
     * default, it is \c None, which means no stabilization is done.
     * \c Image turns on image stabilization, which is an operation
     * reserved for the still capture output pin.
     * When setting or querying, the argument must be a pointer to an
     * \c NvMMStabilization_Mode enum.
     */
    NvCameraIspAttribute_Stabilization,

    /**
     * The exposure time upper and lower bounds specify the maximum (and
     * minimum) exposure times that may be selected by the auto exposure
     * algorithm. Changes to this parameter vector will take effect the next
     * time exposure is computed (either by continuous auto-exposure or a call
     * to \c ComputeAlgorithms). This setting has no effect if manual exposure
     * time is set. When specifying (or querying) \c ExposureTimeRange,
     * the value parameter must be a pointer to an ::NvCameraIspRange
     * structure. Legal values for the low and high fields are millisecond
     * values in S15.16 fixed-point format. Values are clamped to
     * the limits of the minimum and maximum exposure time.
     *
     * - Parameter type: (NvCameraIspRange * )
     */
    NvCameraIspAttribute_ExposureTimeRange,

    /**
     * The exposure time upper and lower bounds specify the maximum (and
     * minimum) exposure times that may be selected by the sensor driver.
     * When querying \c ExposureTimeRange, the value parameter must be a
     * pointer to a ::NvCameraIspRangeF32 structure. The SetAttribute
     * is not supported.
     *
     * - Parameter type: (NvCameraIspRangeF32 * )
     */
    NvCameraIspAttribute_SensorETRange,

    /**
     * Similar to the \c ExposureTimeRange, \c ISORange will limit the
     * amount of gain used. For 'normal' photos, the app may not
     * want to allow the gain to go past ISO200, but for 'night mode'
     * the app may want it to go to ISO800.
     * Valid values are natural numbers between 0 and the maximum ISO,
     * stored as S15.16 fixed point numbers.
     * Values beyond what the sensor supports are clamped to the valid range.
     * So, using (0, 1600) will be treated as (100, 800) if that is the true
     * range.
     *
     * @note Setting the ISO range will typically only dim the image.
     * This is because AE is 'exposure time priority' and the gain is calculated
     * after exposure time. It should only be used to avoid noise at the
     * cost of brightness.
     *
     * - Parameter type: (NvCameraIspRange * )
     */
    NvCameraIspAttribute_ISORange,

    /**
     * Fuse ID refers to the unique ID stored in a sensor's OTP memory
     * that is used to verify calibration file integrity. It is
     * stored as a structure containing a 16-byte array to hold the
     * fuse ID's byte value and a u32 to hold the size of the
     * fuse ID (the number of bytes).
     *
     * -  Parameter type: (NvCameraFuseId * )
     */
    NvCameraIspAttribute_FuseId,

    /**
     * Read-only. While the auto exposure algorithm is running, it internally
     * tracks the calculated brightness of the scene. This value is the product
     * of the average scene luminance and the reciprocal of the exposure
     * in use. If a scene is low key but in bright light, this value will be
     * settings smaller than a high key scene in bright light. The lowest value
     * will be low key scenes with very long exposures, the highest value
     * will be high key scenes in bright light.
     *
     * - Parameter type: NvF32
     */
    NvCameraIspAttribute_SceneBrightness,

    /**
     * Read-only. Autofocus moves a lens closer or farther to the sensor,
     * attempting to optimize the image sharpness. Autofocus algorithms usually
     * take time to run, and they assume the preview is active.
     *
     * \c AutoFocusControl informs the driver of the intent of the application,
     * whether the focus will be in auto mode, auto mode with locking,
     * manual mode, or off.
     * - Parameter type ::NvCameraIspFocusControl enumeration
     *
     * \c AutoFocusTrigger informs the driver to initiate the auto focus
     * searching. \c AutoFocusTrigger takes a signed integer, which gives
     * the allowed amount of time for the focus to lock. If the focus has
     * not locked, then the driver will report an autofocus timeout to
     * the application. Giving zero time tells the autofocus to abort.
     * The time is in milliseconds.
     * - Parameter type: signed integer
     *
     * \c AutoFocusStatus reports current status.
     * \c NvError_BadParameter is returned if AutoFocus was not enabled, or
     * because it is Read-Only, \c NvError_BadParameter is returned if a write
     * is tried.
     * - Parameter type: ::NvCameraIspFocusStatus enumeration
     *
     * \c AutoFocusMode sets/gets the current focus mode.
     * \c NvError_BadParameter is returned if AutoFocus is not valid.
     * - Parameter type: ::NvCameraIspFocusMode enumeration
     *
     * \c PreAutoFocusMode gets the previous focus mode.
     * \c NvError_BadParameter is returned if the previous focus mode is not set.
     * NvError_ReadOnlyAttribute is returned if a write is attempted as it is Read-Only.
     * - Parameter type: ::NvCameraIspFocusMode enumeration
     *
     * ODMs for External ISPs must support the \c AutoFocusStatus query,
     * because the NVIDIA camera driver will periodically query this while
     * Autofocus is active. Once it is found to be locked, an event will be
     * sent to the OpenMAX layer, allowing OpenMAX IL clients like the
     * Android camera service to initiate image capture if it was waiting on
     * autofocus during its halfpress.
     */
    NvCameraIspAttribute_AutoFocusControl,
    NvCameraIspAttribute_AutoFocusTrigger,
    NvCameraIspAttribute_AutoFocusStatus,
    NvCameraIspAttribute_AutoFocusMode,
    NvCameraIspAttribute_PreAutoFocusMode,
    /**
     * Exposure program that the camera used when the image was taken.
     * - 1 - manual control
     * - 2 - program normal
     * - 3 - aperture priority
     * - 4 - shutter priority
     * - 5 - program creative (slow program)
     * - 6 - program action (high-speed program)
     * - 7 - potrait mode
     * - 8 - landscape mode
     *
     * - Parameter type: NvU16
     */
    NvCameraIspAttribute_ExposureProgram,
#define NVCAMERAISP_EXPOSURE_PROGRAM_MANUAL     (1)
#define NVCAMERAISP_EXPOSURE_PROGRAM_NORMAL     (2)
#define NVCAMERAISP_EXPOSURE_PROGRAM_APERTURE   (3)
#define NVCAMERAISP_EXPOSURE_PROGRAM_SHUTTER    (4)
#define NVCAMERAISP_EXPOSURE_PROGRAM_CREATIVE   (5)
#define NVCAMERAISP_EXPOSURE_PROGRAM_ACTION     (6)
#define NVCAMERAISP_EXPOSURE_PROGRAM_POTRAIT    (7)
#define NVCAMERAISP_EXPOSURE_PROGRAM_LANDSCAPE  (8)

    /**
     * Exposure mode is required for corresponding Exif tag.
     * ODMs for External ISPs must support the \a xposureMode query.
     *
     * - Parameter type: NvU16
     */
    NvCameraIspAttribute_ExposureMode,
#define NVCAMERAISP_EXPOSURE_MODE_AUTO      (0)
#define NVCAMERAISP_EXPOSURE_MODE_MANUAL    (1)
#define NVCAMERAISP_EXPOSURE_MODE_BRACKET   (2)
#define NVCAMERAISP_EXPOSURE_MODE_NONE      (3)

    /**
     * Specifies the scene capture type on the sensor side.
     * This Exif tag indicates the type of scene that was shot.
     * It can also be used to record the mode in which the image was
     * shot. @note This differs from the scene type (\a SceneType).
     *
     * - Parameter type: NvU16
     */
    NvCameraIspAttribute_SceneCaptureType,
#define NVCAMERAISP_SCENECAPTURE_TYPE_STANDARD      (0)
#define NVCAMERAISP_SCENECAPTURE_TYPE_LANDSCAPE     (1)
#define NVCAMERAISP_SCENECAPTURE_TYPE_PORTRAIT      (2)
#define NVCAMERAISP_SCENECAPTURE_TYPE_NIGHT         (3)

    /**
     * Read-only. Specifies the flash status, a.k.a. Flash in Exif2.2.
     * This field's 7 bits describe the state of flash as:
     * - bit 0 - Flash fired
     * - bit 1,2 - Flash return
     * - bit 3,4 - Flash mode
     * - bit 5 - Flash function
     * - bit 6 - Red eye mode
     *
     * Defines can be combined to report the entire status.
     * - 0x19: fired in auto mode
     * - 0x18 not fired in auto mode
     * - 0x1: flash fired
     * - 0x0: not fired
     * - Etc.
     *
     * For more information, see Exif2.2.
     *
     * - Parameter type: NvU16
     */
    NvCameraIspAttribute_FlashStatus,
#define NVCAMERAISP_FLASHSTATUS_FIRED                   (0x1)
#define NVCAMERAISP_FLASHSTATUS_RETURN_NOSTROBE         (0x0)
#define NVCAMERAISP_FLASHSTATUS_RETURN_STROBE_NOLIGHT   (0x4)
#define NVCAMERAISP_FLASHSTATUS_RETURN_STROBE_LIGHT     (0x6)
#define NVCAMERAISP_FLASHSTATUS_MODE_UNKNOWN            (0x0)
#define NVCAMERAISP_FLASHSTATUS_MODE_FIRING             (0x8)
#define NVCAMERAISP_FLASHSTATUS_MODE_SUPPRESSION        (0x10)
#define NVCAMERAISP_FLASHSTATUS_MODE_AUTO               (0x18)
#define NVCAMERAISP_FLASHSTATUS_NOFUNCTION              (0x20)
#define NVCAMERAISP_FLASHSTATUS_REDEYE                  (0x40)

    /**
     * Read-only. Indicates the white balance mode used from the ISP.
     * - 0 = Auto
     * - 1 = manual
     * - Other = reserved
     *
     * This is mainly required for the Exif tag.
     *
     * - Parameter type: NvU16
     */
    NvCameraIspAttribute_WhiteBalanceMode,

    /** Read-only. Obtains the ID from external ISP.
     * It is recorded as an ASCII string equivalent to hexadecimal notation
     * with 128-bit fixed length.
     *
     * This is mainly required for Exif tag (0xA420).
     *
     * - Parameter type: NvU8*33
     */
    NvCameraIspAttribute_ImageUniqueID,

    /** To provide users with visual feedback about auto-exposure behavior,
     *  the \a AutoExposureRegions parameter may be queried to determine
     *  the placement of the ISP's auto-exposure regions. When querying this
     *  parameter, the value must be a pointer to an
     *  ::NvCameraIspExposureRegions structure, which will be filled with actual
     *  number of exposure regions (N) and N \c NvRectF32 structures describing
     *  each exposure region. The floating values in \c NvRectF32 are normalized
     *  to the [-1.0, 1.0] range, and are expressed relative to the active
     *  cropping rectangle. This parameter is read/write, and when used in
     *  \c SetAttribute, it allows setting exposure regions configuration to be
     *  used by the camera.
     */
    NvCameraIspAttribute_AutoExposureRegions,


    /**
     * Enabling stereo rectification is only available for video capture.
     * By default, stereo rectification is disabled.
     *
     * - Parameter type: NvBool.
     */
    NvCameraIspAttribute_StereoRectification,

    /**
     * Low-light threshold in microseconds (exposure time).
     *
     * - Parameter type: NvS32.
     */
    NvCameraIspAttribute_LowLightThreshold,
    NvCameraIspAttribute_MacroModeThreshold,

    /**
     * Directly writes the VI THS settle time.
     *
     * - Parameter type: NvU8.
     */
    NvCameraIspAttribute_CILThreshold,

    /**
     * Disables/Enables AE bracketed burst mode
     *
     * - Parameter type: NvBool.
     */
    NvCameraIspAttribute_AEBracketedBurstMode,

    /**
     * Directly writes AE bracketed burst mode settings
     * to the sensor and ISP.
     *
     * - Parameter type: NvCameraIspAeSettings.
     */
    NvCameraIspAttribute_AEBracketedBurstSettings,

    /**
     * Disables/Enables CAF focus move message
     *
     * - Parameter type: NvBool.
     */
    NvCameraIspAttribute_FocusMoveMsg,

    /**
     * Read-only.  Reads the state of the AE algorithm.
     *
     * - Parameter type: NvCameraIspAeHwSettings.
    */
    NvCameraIspAttribute_AeHwSettings,

    /**
     * Read-only. While the auto exposure algorithm is running, it internally
     * tracks the calculated brightness of the scene. Based on this value, it
     * can determine if flash light is needed for the coming image capture.
     * This attribute is for checking if flash is needed.
     *
     * - Parameter type: NvBool
     */
    NvCameraIspAttribute_FlashNeeded,

    /**
     * Read and Write. Access to the auto focuser algorithm parameters.
     *
     * - Parameter type: NvCameraIspAfConfig.
     */
    NvCameraIspAttribute_AfConfig,

    /**
     * Read-only. Specifies the valid gain range of sensor mode.
     * Queries AeWorkbench to compensate for binning, not direct
     * to sensor.
     *
     * - Parameter type: NvCameraIspRangeF32.
     */
    NvCameraIspAttribute_GainRange,

    /**
     * Read-only. Read sensor color calibration matrix at color temperature.
     * Queries static ISP tuning data.
     *
     * - Parameter in:  NvF32 color temperature in Kelvin
     * - Parameter out: NvF32[16] 4x4 matrix, top-left 3x3 sub-matrix is CCM, and
     *                  last 1x3 column is black-level compensation (in corrected RGB space)
     */
    NvCameraIspAttribute_ColorCalibrationMatrix,

    /** Ignore -- Forces compilers to make 32-bit enums. */
    NvCameraIspAttribute_Force32 = 0x7FFFFFFF
} NvCameraIspAttribute; // enum

/*@}*/

/** @name Data Types for Attributes to Set/Get
 *
 * These data types are used for the attributes defined above.
 */
/*@{*/

typedef struct NvCameraIspISORec
{
    NvBool isAuto;  /**< If true, value is ignored on set. */
    NvS32 value;
} NvCameraIspISO;

typedef struct NvCameraIspExposureTimeRec
{
    NvBool isAuto;  /**< If true, value is ignored on set. */
    NvF32 value;
} NvCameraIspExposureTime;

typedef struct NvCameraIspFocusPositionRec
{
    NvBool isAuto;  /**< If true, value is ignored on set. */
    NvS32 value;
} NvCameraIspFocusPosition;

/**
 * Defines stabilization modes.
 */
typedef enum
{
    /** No stabilization. */
    NvCameraIspStabilizationMode_None = 0x0, // index starts from 0

    // 0x1 was obsolete image stabilization algo

    /** Video stabilization active. */
    NvCameraIspStabilizationMode_Video = 0x2, // force to 0x2 for backward
                                              // compatibility

    /** Ignore -- Forces compilers to make 32-bit enums. */
    NvCameraIspStabilizationMode_Force32 = 0x7FFFFFFF

} NvCameraIspStabilizationMode;

#define NVCAMERAISP_MAX_FOCUSING_REGIONS  8
#define NVCAMERAISP_MAX_EXPOSURE_REGIONS  8

/**
 * Holds camera focusing regions.
 */
typedef struct NvCameraIspFocusingRegionsRec
{
    NvU32 numOfRegions;
    NvRectF32 regions[NVCAMERAISP_MAX_FOCUSING_REGIONS];
} NvCameraIspFocusingRegions;

/**
 * Holds a set of active focusing regions.
 */
typedef struct NvCameraIspActiveFocusingRegionsRec
{
    NvU32 numOfRegions;
    NvBool regions[NVCAMERAISP_MAX_FOCUSING_REGIONS];
} NvCameraIspActiveFocusingRegions;

/**
 * Holds sharpness values for camera focusing regions.
 */
typedef struct NvCameraIspFocusingRegionsSharpnessRec
{
    NvU32 numOfRegions;
    NvF32 regions[NVCAMERAISP_MAX_FOCUSING_REGIONS];
} NvCameraIspFocusingRegionsSharpness;

/**
 * Holds camera focusing parameters.
 */
typedef struct NvCameraIspFocusingParametersRec
{
    NvS32 minPos;            /**< Minimum supported position. */
    NvS32 maxPos;            /**< Maximum supported position. */
    NvS32 hyperfocal;        /**< Hyperfocal position. */
    NvS32 macro;             /**< "Macro" position. */
    NvS32 powerSaving;       /**< Power-efficient position. */
    NvBool sensorispAFsupport; /**< Sensor ISP AutoFocus support. */
    NvBool infModeAF;        /**< AF in Inf mode */
    NvBool macroModeAF;      /**< AF in Macro mode */
} NvCameraIspFocusingParameters;

/**
 * Holds camera auto focusing configuration.
 */
typedef struct NvCameraIspAfConfig
{
    NvS32 positionPhysicalLow;
    NvS32 positionPhysicalHigh;
    NvS32 hysteresis;
    NvS32 slewRate;
    NvS32 settleTime;
    NvS32 positionHyperFocal;
    NvS32 positionResting;
    NvS32 positionInf;
    NvS32 positionInfOffset;
    NvS32 positionMacro;
    NvS32 positionMacroOffset;
    NvS32 infMin;
    NvS32 macroMax;
    NvBool rangeEndsReversed;
    NvS32 infInitSearch;    // Not Implemented
    NvS32 macroInitSearch;  // Not Implemented
} NvCameraIspAfConfig;

/**
 * Holds the output of AE in terms of
 * sensor and ISP settings.
 */
typedef struct
{
    /// Boolean Flags to indicate the fields to write.
    NvBool ExposureTimeEn;
    NvBool BinningGainEn;
    NvBool AnalogGainEn;
    NvBool SensorDigitalGainEn;
    NvBool IspDigitalGainEn;

    /// Flag to indicate immediate I2C write and not ISP gain.
    NvBool ImmediateSensorWriteOnly;

    NvBool BracketedBurstMode;

    /// Fields.
    NvF32 ExpComp;
    NvF32 ExposureTime;
    NvF32 BinningGain;           /**< Gain caused by binning summation etc. in sensor. */
    NvF32 AnalogGain;
    NvF32 SensorDigitalGain;
    NvF32 IspDigitalGain;
    NvF32 FrameRate;
}NvCameraIspAeHwSettings;

typedef struct
{
    NvF32 DRE; // >= 1.0f
    NvF32 PreIspCompression; // <= 1.0f
    void *GdcParams;
} NvIspAeAOHDRSettings;

/* awb frame data */
typedef struct
{
    NvF32   WBGains[4];
    NvU32   Cct;
} NvIspAWBOutputs;


/**
 * Holds camera exposure regions definition.
 */
typedef struct NvCameraIspExposureRegionsRec
{
    NvS32 meteringMode;
    NvU32 numOfRegions;
    NvRectF32 regions[NVCAMERAISP_MAX_EXPOSURE_REGIONS];
    NvF32 weights[NVCAMERAISP_MAX_EXPOSURE_REGIONS];
} NvCameraIspExposureRegions;

/**
 * Holds a one-dimensional range (i.e., an upper and lower
 * limit for some value).
 */
typedef struct NvCameraIspRangeRec
{
    /**  Lower limit for the range. */
    NvS32 low;
    /**  Upper limit for the range. */
    NvS32 high;

} NvCameraIspRange;

/**
 * Holds a two-dimensional range (i.e., an upper and lower
 * limit for some value).
 */
typedef struct NvCameraIspRangeF32Rec
{
    /**  Lower limit for the range. */
    NvF32 low;
    /**  Upper limit for the range. */
    NvF32 high;

} NvCameraIspRangeF32;

/**
 * Holds a one-dimensional range (i.e., an upper and lower
 * limit for some value).
 */
typedef struct NvCameraIspRangePointF32Rec
{
    /**  Lower limit for the range. */
    NvPointF32 low;
    /**  Upper limit for the range. */
    NvPointF32 high;

} NvCameraIspRangePointF32;

/**
 * Holds a fuse ID (16 x 1 bytes).
 */
typedef struct NvCameraFuseIdRec
{
    NvU32 size;
    NvU8 data[16];
} NvCameraFuseId;

/**
 * Holds a two-dimensional 4x4 matrix.
 * The range of valid offset is -1.0 to 1.0.
 */
typedef struct NvCameraIspMatrixRec
{
    /** First row of 4x4 matrix:
       red-to-red; green-to-red; blue-to-red; red-offset;
       red-to-u; green-to-u; blue-to-u; u-offset. */
    NvS32 m00, m01, m02, m03;
    /** Second row of 4x4 matrix:
       red-to-green; green-to-green; blue-to-green; green-offset;
       red-to-v; green-to-v; blue-to-v; v-offset. */
    NvS32 m10, m11, m12, m13;
    /** Third row of 4x4 matrix:
       red-to-blue; green-to-blue; blue-to-blue; blue-offset;
       red-to-y; green-to-y; blue-to-y; y-offset. */
    NvS32 m20, m21, m22, m23;
    /** Forth row of 4x4 matrix:
       m30, m31, m32 should be zeros; m33 should be 1. */
    NvS32 m30, m31, m32, m33;

} NvCameraIspMatrix;

/**
 * Specifies the focus status used with the \a AutoFocusStatus parameter.
 */
typedef enum {
    /// Busy : the focuser is still moving.
    NvCameraIspFocusStatus_Busy = 0,
    /// Locked : the focuser has stopped at the best position.
    NvCameraIspFocusStatus_Locked,
    /// FailedToFind : the focuser could not find a best position.
    NvCameraIspFocusStatus_FailedToFind,
    /// Error : an error occurred, for instance I2C read error.
    NvCameraIspFocusStatus_Error,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvCameraIspFocusStatus_Force32 = 0x7FFFFFFF
} NvCameraIspFocusStatus;

/**
 * Specifies the focus mode used with the \a AutoFocusControl parameter.
 */
typedef enum {
    /// Manual : the focuser is still moving.
    NvCameraIspFocusMode_Manual = 1,
    /// Fast : fast CAF for still capture.
    NvCameraIspFocusMode_Fast,
    /// Smooth : smooth CAF for video recording.
    NvCameraIspFocusMode_Smooth,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvCameraIspFocusMode_Force32 = 0x7FFFFFFF
} NvCameraIspFocusMode;
/**
 * Specifies the auto focus control used with the \a AutoFocusControl parameter.
 * The commands directly map to the OpenMAX IL \c OMX_IMAGE_FOCUSCONTROLTYPE
 * definition.
 */
typedef enum {
    /// On :  Manual focus is used;
    /// See ::NvCameraIspAttribute_FocusPosition.
    NvCameraIspFocusControl_On = 0,
    /// Off : Cancel autofocus.
    NvCameraIspFocusControl_Off,
    /// Auto : Adjust the focuser automatically.
    NvCameraIspFocusControl_Auto,
    /// AutoLock : Adjust the focuser automatically and lock when
    ///            the optimal position is found.
    NvCameraIspFocusControl_AutoLock,
    /// Continuous_Video: smooth AF for video recording
    NvCameraIspFocusControl_Continuous_Video,
    /// Continuous_Picture: fast AF
    NvCameraIspFocusControl_Continuous_Picture,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvCameraIspFocusControl_Force32 = 0x7FFFFFFF,
}  NvCameraIspFocusControl;

/**
 * Specifies the white balance mode.
 * Note: this enum is used for indexing an array.
 * Do not change Auto or None to 0.
 */
typedef enum {
    NvCameraIspWhiteBalanceMode_Sunlight = 0,
    NvCameraIspWhiteBalanceMode_Cloudy,
    NvCameraIspWhiteBalanceMode_Shade,
    NvCameraIspWhiteBalanceMode_Tungsten,
    NvCameraIspWhiteBalanceMode_Incandescent,
    NvCameraIspWhiteBalanceMode_Fluorescent,
    NvCameraIspWhiteBalanceMode_Flash,
    NvCameraIspWhiteBalanceMode_Horizon,
    NvCameraIspWhiteBalanceMode_WarmFluorescent,
    NvCameraIspWhiteBalanceMode_Twilight,

    NvCameraIspWhiteBalanceMode_Auto,
    NvCameraIspWhiteBalanceMode_None,

    NvCameraIspWhiteBalanceMode_Force32 = 0x7FFFFFFF,
} NvCameraIspWhiteBalanceMode;

// number of white balance mode except auto and none
#define NUM_MANUAL_WHITEBALANCE_MODES   (NvCameraIspWhiteBalanceMode_Auto)

/*@}*/
#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_CAMERA_H
