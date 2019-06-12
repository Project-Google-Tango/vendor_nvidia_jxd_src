/**
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef PCL_DRIVER_SENSOR_H
#define PCL_DRIVER_SENSOR_H

#include "nvos.h"

#if defined(__cplusplus)
extern "C"
{
#endif


//TODO: not cleaned up yet, should inspect meaning of each value and redetermine use/placement.
// --A lot of this has been copy/pasted from nvodm_imager.h and old-driver values should be able to map
// to new struct organization without issue at this point in time.



/**
 * Defines the imager sync edge.
 */
typedef enum {
   NvPclSensorSyncEdge_Rising = 0,
   NvPclSensorSyncEdge_Falling = 1,
   NvPclSensorSyncEdge_Force32 = 0x7FFFFFFF,
} NvPclSensorSyncEdge;
/**
 * Defines parallel timings for the sensor device.
 * For your typical parallel interface, the following timings
 * for the imager are sent to the driver, so it can program the
 * hardware accordingly.
 */
typedef struct
{
  /// Does horizontal sync start on the rising or falling edge.
  NvPclSensorSyncEdge HSyncEdge;
  /// Does vertical sync start on the rising or falling edge.
  NvPclSensorSyncEdge VSyncEdge;
  /// Use VGP0 for MCLK. If false, the VCLK pin is used.
  NvBool MClkOnVGP0;
} NvPclSensorParallelSensorTiming;
/**
 * Defines MIPI timings. For a MIPI imager, the following timing
 * information is sent to the driver, so it can program the hardware
 * accordingly.
 */
typedef struct
{
#define PCLSENSORMIPITIMINGS_PORT_CSI_A (0)
#define PCLSENSORMIPITIMINGS_PORT_CSI_B (1)
    /// Specifies MIPI-CSI port type.
    /// @deprecated; do not use.
    NvU8 CsiPort;
#define PCLSENSORMIPITIMINGS_DATALANES_ONE     (1)
#define PCLSENSORMIPITIMINGS_DATALANES_TWO     (2)
#define PCLSENSORMIPITIMINGS_DATALANES_THREE   (3)
#define PCLSENSORMIPITIMINGS_DATALANES_FOUR    (4)
    /// Specifies number of data lanes used.
    NvU8 DataLanes;
#define PCLSENSORMIPITIMINGS_VIRTUALCHANNELID_ONE   (0)
#define PCLSENSORMIPITIMINGS_VIRTUALCHANNELID_TWO   (1)
#define PCLSENSORMIPITIMINGS_VIRTUALCHANNELID_THREE (2)
#define PCLSENSORMIPITIMINGS_VIRTUALCHANNELID_FOUR  (3)

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
} NvPclSensorMipiSensorTiming;
/**
 * Holds a region, for use with
 * NvOdmImagerParameter_RegionUsedByCurrentResolution.
 */
typedef struct NvPclSensorRegionRec
{
    /// Specifies the upper left corner of the region.
    NvPoint RegionStart;
    /// Specifies the horizontal/vertical downscaling used
    /// (eg., 2 for a 2x downscale, 1 for no downscaling).
    NvU32 xScale;
    NvU32 yScale;

} NvPclSensorRegion;
/**
 * Defines camera imager types.
 * Each ODM imager implementation will support one imager type.
 * @note A system designer should be aware of the limitations of using
 * both an imager device and a bitstream device, because they may share pins.
 */
typedef enum
{
    /// Specifies standard VIP 8-pin port; used for RGB/YUV data.
    NvPclSensorInterface_Parallel8 = 1,
    /// Specifies standard VIP 10-pin port; used for Bayer data, because
    /// ISDB-T in serial mode is possible only if Parallel10 is not used.
    NvPclSensorInterface_Parallel10,
    /// Specifies MIPI-CSI, Port A.
    NvPclSensorInterface_SerialA,
    /// Specifies MIPI-CSI, Port B
    NvPclSensorInterface_SerialB,
    /// Specifies MIPI-CSI, Port C
    NvPclSensorInterface_SerialC,
    /// Specifies MIPI-CSI, Port A&B. This is special case for T30 only
    /// where both CSI A&B must be enabled to make use of 4 lanes.
    NvPclSensorInterface_SerialAB,
    /// Specifies CCIR protocol (implicitly Parallel8)
    NvPclSensorInterface_CCIR,
    /// Specifies not a physical imager.
    NvPclSensorInterface_Host,
    /// Temporary: CSI via host interface testing
    /// This serves no purpose in the final driver,
    /// but is needed until a platform exists that
    /// has a CSI sensor.
    NvPclSensorInterface_HostCsiA,
    NvPclSensorInterface_HostCsiB,
    NvPclSensorInterface_Num,
    NvPclSensorInterface_Force32 = 0x7FFFFFFF
} NvPclSensorInterface;

#define PCLSENSORPIXELTYPE_YUV      0x0010
#define PCLSENSORPIXELTYPE_BAYER    0x0100
#define PCLSENSORPIXELTYPE_RGB      0x1000
#define PCLSENSORPIXELTYPE_HDR      0x2000
#define PCLSENSORPIXELTYPE_IS_YUV(_PT)   ((_PT) & PCLSENSORPIXELTYPE_YUV)
#define PCLSENSORPIXELTYPE_IS_BAYER(_PT) ((_PT) & PCLSENSORPIXELTYPE_BAYER)
#define PCLSENSORPIXELTYPE_IS_RGB(_PT)   ((_PT) & PCLSENSORPIXELTYPE_RGB)
#define PCLSENSORPIXELTYPE_IS_HDR(_PT)   ((_PT) & PCLSENSORPIXELTYPE_HDR)
/**
 * Defines the ODM imager pixel type.
 */
typedef enum {
    NvPclSensorPixelType_Unspecified = 0,
    NvPclSensorPixelType_UYVY = PCLSENSORPIXELTYPE_YUV,
    NvPclSensorPixelType_VYUY,
    NvPclSensorPixelType_YUYV,
    NvPclSensorPixelType_YVYU,
    NvPclSensorPixelType_BayerRGGB = PCLSENSORPIXELTYPE_BAYER,
    NvPclSensorPixelType_BayerBGGR,
    NvPclSensorPixelType_BayerGRBG,
    NvPclSensorPixelType_BayerGBRG,
    NvPclSensorPixelType_BayerRGGB8,
    NvPclSensorPixelType_BayerBGGR8,
    NvPclSensorPixelType_BayerGRBG8,
    NvPclSensorPixelType_BayerGBRG8,
    NvPclSensorPixelType_RGB888 = PCLSENSORPIXELTYPE_RGB,
    NvPclSensorPixelType_A8B8G8R8,
    NvPclSensorPixelType_HDR_RGGB = PCLSENSORPIXELTYPE_HDR,
    NvPclSensorPixelType_HDR_BGGR,
    NvPclSensorPixelType_HDR_GRBG,
    NvPclSensorPixelType_HDR_GBRG,
    NvPclSensorPixelType_Force32 = 0x7FFFFFFF
} NvPclSensorPixelType;
/// Defines the ODM imager orientation.
/// The imager orientation is with respect to the display's
/// top line scan direction. A display could be tall or wide, but
/// what really counts is in which direction it scans the pixels
/// out of memory. The camera block must place the pixels
/// in memory for this to work correctly, and so the ODM
/// must provide info about the board placement of the imager.
typedef enum {
    NvPclSensorOrientation_0_Degrees = 0,
    NvPclSensorOrientation_90_Degrees,
    NvPclSensorOrientation_180_Degrees,
    NvPclSensorOrientation_270_Degrees,
    NvPclSensorOrientation_Force32 = 0x7FFFFFFF
} NvPclSensorOrientation;

/// Defines the structure used to describe the mclk and pclk
/// expectations of the sensor. The sensor likely has PLL's in
/// its design, which allows it to get a low input clock and then
/// provide a higher speed output clock to achieve the needed
/// bandwidth for frame data transfer. Ideally, the clocks will differ
/// so as to avoid noise on the bus, but early versions of
/// Tegra Application Processor required the clocks be the same.
/// The capabilities structure gives which profiles the NVODM
/// supports, and \a SetParameter of ::NvOdmImagerParameter_SensorInputClock
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
} NvPclSensorClockProfile;

/**
 * Holds mode-specific sensor information.
 */
typedef struct NvPclSensorModeInfoRec
{
    /// Specifies the scan area (left, right, top, bottom)
    /// on the sensor array that the output stream covers.
    NvRect rect;
    /// Specifies x/y scales if the on-sensor binning/scaling happens.
    NvPointF32 scale;
} NvPclSensorModeInfo;

/**
 * Defines imager crop modes for NvOdmImagerSensorMode::CropMode.
 */
typedef enum
{
    NvPclSensorNoCropMode = 1,
    NvPclSensorPartialCropMode,
    NvPclSensorCropModeForce32 = 0x7FFFFFFF,
} NvPclSensorCropMode;

/** The maximum length of imager identifier string. */
#define PCLSENSOR_IDENTIFIER_MAX (32)
/** The maximum length of imager format string. */
#define PCLSENSOR_FORMAT_MAX (4)
/** The list of supported clock profiles maximum array size. */
#define PCLSENSOR_CLOCK_PROFILE_MAX (2)
/**
 *
 */
typedef struct {
    /// Specifies orientation and direction.
    NvPclSensorOrientation Orientation;
    /// Specifies the pixel types;
    NvPclSensorPixelType PixelType;
    /// Specifies the active scan region.
    NvSize ActiveDimensions;
    /// Specifies the active start.
    NvPoint ActiveStart;
    ///
    /// Proclaim the minimum blank time that this
    /// sensor will honor. The camera driver will check this
    /// against the underlying hardware's requirements.
    /// width = horizontal blank time in pixels.
    /// height = vertical blank time in lines.
    NvSize MinimumBlankTime;
    /// Specifies the:
    /// <pre> pixel_aspect_ratio = pixel_width / pixel_height </pre>
    /// So pixel_width = k and pixel_height = t means 1 correct pixel consists
    /// of 1/k sensor output pixels in x direction and 1/t sensor output pixels
    /// in y direction. One of pixel_width and pixel_height must be 1.0 and
    /// the other one must be less than or equal to 1.0. Can be 0 for 1.0.
    NvF32 PixelAspectRatio;
    /// Specifies the crop mode. This can be partial or full.
    /// In partial crop mode, a portion of sensor array is taken while
    /// configuring sensor mode. While in full mode, almost the entire sensor array is
    /// taken while programing the sensor mode. Thus, in crop mode, the final image looks
    /// croped with respect to the full mode image.
    NvPclSensorCropMode CropMode;
    /// Specifies mode-specific information.
    NvPclSensorModeInfo ModeInfo;

    // Importing static properties
    NvRect SensorActiveArraySize;
    NvPointF32 SensorPhysicalSize;
    NvSize SensorPixelArraySize;
    NvU32 SensorWhiteLevel;
    NvU32 BlackLevelPattern[2][2];

} NvPclSensorPixelInfo;
/**
 *
 */
typedef struct {
    ///
    /// Specifies the clock profiles supported by the imager.
    /// For instance: {24, 2.66}, {64, 1.0}. The first is preferred
    /// but the second may be used for backward compatibility.
    /// Unused profiles must be zeroed out.
    NvPclSensorClockProfile ClockProfile;
    /// Specifies the clock frequency that the sensor
    /// requires for initialization and mode changes.
    /// Also can be used for times when performance
    /// is not important (self-test, calibration, bringup).
    NvU32 InitialSensorClockRateKHz;
    /// Specifies the sensor PLL multiplier so VI/ISP clock can be set accordingly.
    /// <pre> MCLK * PLL </pre> multipler will be calculated to set VI/ISP clock accordingly.
    /// If not specified, VI/ISP clock will set to the maximum.
    NvF32 PLL_Multiplier;
    /// Specifies parallel and serial timing settings.
    NvPclSensorParallelSensorTiming ParallelTiming;
    NvPclSensorMipiSensorTiming MipiTiming;
    /// Specifies imager interface types.
    NvPclSensorInterface SensorOdmInterface;
} NvPclSensorInterfaceProperties;

/**
 *
 */
typedef struct {

    /// Specifies the frame rate limits.
    NvF32 MinFrameRate;
    NvF32 MaxFrameRate;
    /// Specifies the exposure time limits in nanoseconds.
    NvU64 MinExposureTime;
    NvU64 MaxExposureTime;
    /// Specifies the gain limits.
    NvF32 MinAnalogGain;
    NvF32 MaxAnalogGain;
    /// Specifies the digital gain limits.
    NvF32 MinDigitalGain;
    NvF32 MaxDigitalGain;
    /// Specifies the HDR ratio limits.
    NvF32 MinHDRRatio;
    NvF32 MaxHDRRatio;
} NvPclSensorLimits;

/**
 * Holds ODM imager modes for sensor device.
 */
typedef struct NvPclSensorObjectRec
{
    NvU32 Version;
    NvU32 Id;
    NvPclSensorPixelInfo PixelInfo;
    NvPclSensorInterfaceProperties Interface;
    NvPclSensorLimits Limits;
} NvPclSensorObject;

typedef struct NvPclSensorObjectRec *NvPclSensorObjectHandle;

#if defined(__cplusplus)
}
#endif




#endif  //PCL_DRIVER_SENSOR_H

