/**
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef PCL_API_SETTINGS_DATA_H
#define PCL_API_SETTINGS_DATA_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvos.h"
#include "nvpcl_driver.h"
#include "nvpcl_driver_sensor.h"
#include "nvpcl_driver_focuser.h"
#include "nvpcl_driver_flash.h"

#define PCL_MAX_FLASH_NUMBER (8)

// PCL_FLASH_TORCH_MODE is to be used to enable torch in SustainTime
#define PCL_FLASH_TORCH_MODE (0)

#define PCL_INIT_VERSION_PSETTING(_settings) \
    (_settings)->Version = (NvPclSettingsVersion_CURRENT-1)

/**
 * This structure is a part of the primary exposure structure.
 */
typedef struct NvPclExposureValuesRec
{
    NvF32 AnalogGain[4];
    NvF32 DigitalGain[4];
    NvF32 ET;
} NvPclExposureValues;

#define NVPCL_MAX_EXPOSURES (8)
/**
 * FrameDataItem
 * Container for exposure list configurations.
 */
typedef struct NvPclExposureListRec
{
    NvU32 NoOfExposures;
    NvPclExposureValues ExposureVal[NVPCL_MAX_EXPOSURES];
} NvPclExposureList;

/**
 * FrameDataItem
 * Container used to identify sensor mode.
 */
typedef struct  NvPclSensorModeRec
{
    NvU32 Id;
} NvPclSensorMode;

/**
 * This structure is a part of the primary sensor clock profile structure.
 */
typedef struct NvPclSensorClockRec
{
    NvU32 ExternalClockKHz;
    NvF32 ClockMultiplier;
} NvPclSensorClock;

/**
 * FrameDataItem
 * Container used to specify sensor clock profile calibration.
 */
typedef struct NvPclClockProfileRec
{
    NvPclSensorClock SensorClock;
    NvF32 ReadOutTime;
    NvF32 LinesPerSecond;
} NvPclClockProfile;

/**
 * FrameDataItem
 * Container used to specify framerate.
 */
typedef struct NvPclFrameRateSchemeRec
{
    NvU32 FrameRateScheme;
} NvPclFrameRateScheme;

/**
 * FrameDataItem
 * Container used to specify framerate.
 */
typedef struct NvPclFrameRateRec
{
    NvF32 FrameRate;
} NvPclFrameRate;

/**
 * Container to group sensor specific settings.
 * Not necessarily intended for FrameDataItem, but to pass
 * a more complete structure through PCL
 */
typedef struct NvPclSensorSettingsRec
{
    NvPclSensorMode Mode;
    NvPclClockProfile ClockProfile;
    NvU32 FrameRateScheme;
    NvPclFrameRate FrameRate;
    NvPclExposureList ExposureList;
} NvPclSensorSettings;

/*
 * FrameDataItem
 * for data exchange about focuser.
 */
typedef struct NvPclFocuserSettingsRec
{
    NvS32 Locus;
} NvPclFocuserSettings;

/*
 * This structure is a part of the primary
 * Flash structure.
 */
typedef struct NvPclFlashLevelRec
{
    NvF32 CurrentLevel;
} NvPclFlashLevel;

/*
 * FrameDataItem
 * for data exchange about flash.
 */
typedef struct NvPclFlashSettingsRec
{
    NvPclFlashLevel Source[PCL_MAX_FLASH_NUMBER];
    NvF32 SustainTime;
} NvPclFlashSettings;

// Drivers should say if version is < the last validated version its good
typedef enum NvPclSettingsVersion
{
    NvPclSettingsVersion_1V0 = 0,

    // Used for easy macro (PCL_INIT_VERSION_PSETTING)
    NvPclSettingsVersion_CURRENT,

    NvPclSettingsVersion_FORCE32 = 0x7FFFFFFF
} NvPclSettingsVersion;

typedef struct NvPclModuleSettingRec
{
    NvU32 Version;
    NvBool Initialized;
    NvPclSensorSettings Sensor;
    NvPclFocuserSettings Focuser;
    NvPclFlashSettings Flash;
} NvPclModuleSetting;

typedef struct NvPclRunningModuleInfoRec
{
    NvPclModuleSetting Settings;
    NvPclSensorObject SensorMode;
    NvPclFocuserObject FocuserMode;
    NvPclFlashObject FlashList[PCL_MAX_FLASH_NUMBER];
}NvPclRunningModuleInfo;

/**
 * enum to be used in conjunction with NvPcl querying for
 * static information that profile the platform devices
 */
typedef enum NvPclModuleInfoType{
    /**
     * ModuleProfile provides NvPclModuleProfile as a snapshot of
     * the physical system.  FuseIDs are communicated through this
     * mechanism via the NvPclDriverProfile instances in each module.
     */
    NvPclModuleInfoType_ModuleProfile = 0,

    /**
     * SensorCapabilities provides a list of sensor modes capable
     * by the sensor driver. Each mode is of type NvPclSensorObject.
     */
    NvPclModuleInfoType_SensorCapabilities,
    /**
     * FocuserCapabilities currently provides a list of 1 NvPclFocuserObject.
     * This is the static capabilities of the focuser.  Can expand in the
     * future if we support multiple run-time mode-of-operation switching,
     * or if we operate more granularity of focuser instance handles
     * (potentially similar to our independent control of LEDs on a multi-LED
     * system--technology not invented or used right now :) )
     */
    NvPclModuleInfoType_FocuserCapabilities,
    /**
     * FlashCapabilities provides a list of light sources (LEDs) supported
     * by the module.  Each light source is of type NvPclFlashObject that
     * contains the driver's abilities to support that light source.
     */
    NvPclModuleInfoType_FlashCapabilities,
    /**
     * The calibration config .h file included by a driver in the module
     */
    NvPclModuleInfoType_CalibrationData,
    /**
     * The calibration override .isp file loaded off of the file system
     */
    NvPclModuleInfoType_CalibrationOverride,
    /**
     * Traditionally the .bin file on the file system, but is
     * sometimes overloaded by OTP/ROM data.
     */
    NvPclModuleInfoType_FactoryCalibration,
    NvPclModuleInfo_FORCE32 = 0x7FFFFFFF
} NvPclModuleInfoType;

#if defined(__cplusplus)
}
#endif

#endif  //PCL_API_SETTINGS_DATA_H

