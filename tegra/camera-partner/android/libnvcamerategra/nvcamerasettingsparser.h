/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __CAMERA_SETTINGS_PARSER_H__
#define __CAMERA_SETTINGS_PARSER_H__

#include <utils/SortedVector.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <nvmm_camera.h>
#include "nvos.h"

#include "nvcameraparserinfo.h"
#include "nvcamerahalpostprocessHDR.h"

namespace android {

// defines copied from old custcamerasettingsdefinition.cpp file
#define ARRAY_SIZE(_X_) (sizeof(_X_)/sizeof(_X_[0]))
#define SUPPORT_CONTRAST_LEVELS  (1)

// end defines copied from old custcamerasettingsdefinition.cpp file

// Values/Types for external use
// some constants are duplicated from NVOMX_EncoderExtensions.h
const int USER_COMMENT_LENGTH = 192;
const int EXIF_STRING_LENGTH = 200;
const int MAX_DATE_LENGTH = 12;
const int GPS_PROC_METHOD_LENGTH = 32;
const int GPS_MAP_DATUM_LENGTH = 32;

#define ANDROID_WINDOW_PRECISION (1000)

const int FD_OUTPUT_LEN = 1024;
const int FD_VALUE_LEN = 4;

extern ParserInfoTableEntry ParserInfoTable[];

// must match wkbNrOperatingMode enum in nvcamera_3dpp.h in camera/core/camera/alg/3dpp/
typedef enum
{
    AnrMode_Auto = 0,
    AnrMode_Off,
    AnrMode_ForceOn,

    AnrMode_Force32 = 0x7FFFFFFF
} AnrMode;

typedef enum
{
    AlgLock_AF = (1 << 0),
    AlgLock_AE =  (1 << 1),
    AlgLock_AWB =  (1 << 2),
    AlgLock_Force32 = 0x7FFFFFFF
} AlgLock;

typedef enum NvCameraUserSceneModeEnum {
      NvCameraSceneMode_Invalid = 0,
      NvCameraSceneMode_Auto = 1,
      NvCameraSceneMode_Action,
      NvCameraSceneMode_Portrait,
      NvCameraSceneMode_Landscape,
      NvCameraSceneMode_Beach,
      NvCameraSceneMode_Candlelight,
      NvCameraSceneMode_Fireworks,
      NvCameraSceneMode_Night,
      NvCameraSceneMode_NightPortrait,
      NvCameraSceneMode_Party,
      NvCameraSceneMode_Snow,
      NvCameraSceneMode_Sports,
      NvCameraSceneMode_SteadyPhoto,
      NvCameraSceneMode_Sunset,
      NvCameraSceneMode_Theatre,
      NvCameraSceneMode_Barcode,
      NvCameraSceneMode_Backlight,
      NvCameraSceneMode_HDR,

      NvCameraSceneMode_Force32 = 0x7FFFFFFF
} NvCameraUserSceneMode;

// Always define NvCameraFlashMode_Max atlast just before
// NvCameraFlashMode_Force32, so that it remains the maximum
// possible value.
typedef enum NvCameraUserFlashModeEnum {
      NvCameraFlashMode_Invalid = 0,
      NvCameraFlashMode_Auto = 1,
      NvCameraFlashMode_On,
      NvCameraFlashMode_Off,
      NvCameraFlashMode_Torch,
      NvCameraFlashMode_RedEyeReduction,
      NvCameraFlashMode_Max,

      NvCameraFlashMode_Force32 = 0x7FFFFFFF
} NvCameraUserFlashMode;

typedef enum NvCameraUserStereoModeEnum {
      NvCameraStereoMode_Invalid = 0,
      NvCameraStereoMode_LeftOnly = 1,
      NvCameraStereoMode_RightOnly,
      NvCameraStereoMode_Stereo,

      NvCameraStereoMode_Force32 = 0x7FFFFFFF
} NvCameraUserStereoMode;

typedef enum NvCameraUserFocusModeEnum {
      NvCameraFocusMode_Invalid = 0,
      NvCameraFocusMode_Auto = 1,
      NvCameraFocusMode_Infinity,
      NvCameraFocusMode_Macro,
      NvCameraFocusMode_Hyperfocal,
      NvCameraFocusMode_Fixed,
      NvCameraFocusMode_Continuous_Video,
      NvCameraFocusMode_Continuous_Picture,

      NvCameraFocusMode_Force32 = 0x7FFFFFFF
} NvCameraUserFocusMode;

typedef enum NvCameraUserWhitebalanceEnum {
      NvCameraWhitebalance_Invalid = 0,
      NvCameraWhitebalance_Auto = 1,
      NvCameraWhitebalance_Incandescent,
      NvCameraWhitebalance_Fluorescent,
      NvCameraWhitebalance_Daylight,
      NvCameraWhitebalance_CloudyDaylight,
      NvCameraWhitebalance_Shade,
      NvCameraWhitebalance_Flash,
      NvCameraWhitebalance_Tungsten,
      NvCameraWhitebalance_Horizon,
      NvCameraWhitebalance_WarmFluorescent,
      NvCameraWhitebalance_Twilight,

      //this must be the last thing in this enum before the Force32
      NvCameraWhitebalance_Max,
      NvCameraWhitebalance_Force32 = 0x7FFFFFFF
} NvCameraUserWhitebalance;

typedef enum NvCameraUserContrastEnum {
      Contrast_Lowest = -100,
      Contrast_Low = -50,
      Contrast_Normal = 0,
      Contrast_High = 50,
      Contrast_Highest = 100,

      NvCameraContrast_Force32 = 0x7FFFFFFF
} NvCameraUserContrast;

typedef enum NvCameraUserAntiBandingEnum {
      NvCameraAntiBanding_Invalid = 0,
      NvCameraAntiBanding_50Hz = 1,
      NvCameraAntiBanding_60Hz,
      NvCameraAntiBanding_Auto,
      NvCameraAntiBanding_Off,

      NvCameraAntiBanding_Force32 = 0x7FFFFFFF
} NvCameraUserAntiBanding;

typedef enum NvCameraUserColorEffectEnum {
      NvCameraColorEffect_Invalid = 0,
      NvCameraColorEffect_Aqua = 1,
      NvCameraColorEffect_Blackboard,
      NvCameraColorEffect_Mono,
      NvCameraColorEffect_Negative,
      NvCameraColorEffect_None,
      NvCameraColorEffect_Posterize,
      NvCameraColorEffect_Sepia,
      NvCameraColorEffect_Solarize,
      NvCameraColorEffect_Whiteboard,
      NvCameraColorEffect_Vivid,
      NvCameraColorEffect_Emboss,

      NvCameraColorEffect_Force32 = 0x7FFFFFFF
} NvCameraUserColorEffect;

typedef enum NvCameraUserImageFormatEnum {
      NvCameraImageFormat_Invalid = 0,
      NvCameraImageFormat_Jpeg = 1,
      NvCameraImageFormat_Yuv420i,
      NvCameraImageFormat_Yuv420sp,

      NvCameraImageFormat_Force32 = 0x7FFFFFFF
} NvCameraUserImageFormat;

typedef enum NvCameraUserPreviewFormatEnum {
      NvCameraPreviewFormat_Invalid = 0,
      NvCameraPreviewFormat_Yuv420sp = 1,
      NvCameraPreviewFormat_YV12,

      NvCameraPreviewFormat_Force32 = 0x7FFFFFFF
} NvCameraUserPreviewFormat;

typedef enum NvCameraUserVideoFrameFormatEnum {
      NvCameraVideoFrameFormat_Invalid = 0,
      NvCameraVideoFrameFormat_Yuv420p = 1,

      NvCameraVideoFrameFormat_Force32 = 0x7FFFFFFF
} NvCameraUserVideoFrameFormat;

typedef enum NvCameraUserFlipEnum
{
      NvCameraUserFlip_Invalid = 0,
      NvCameraUserFlip_None,
      NvCameraUserFlip_Vertical,
      NvCameraUserFlip_Horizontal,
      NvCameraUserFlip_Both,

      NvCameraUserFlip_Force32 = 0x7FFFFFFF
} NvCameraUserFlip;

typedef enum NvCameraGpsBitMap {
    NvCameraGpsBitMap_LatitudeRef =      0x01,
    NvCameraGpsBitMap_LongitudeRef =     0x02,
    NvCameraGpsBitMap_AltitudeRef =      0x04,
    NvCameraGpsBitMap_TimeStamp =        0x08,
    NvCameraGpsBitMap_Satellites =       0x10,
    NvCameraGpsBitMap_Status =           0x20,
    NvCameraGpsBitMap_MeasureMode =      0x40,
    NvCameraGpsBitMap_DOP =              0x80,
    NvCameraGpsBitMap_ImgDirectionRef =  0x100,
    NvCameraGpsBitMap_MapDatum =         0x200,
    NvCameraGpsBitMap_ProcessingMethod = 0x400,
    NvCameraGpsBitMap_DateStamp =        0x800,
    NvCameraGpsBitMap_Force32 =          0x7FFFFFFF
} NvCameraGpsBitMap;

typedef enum NvCameraRawDumpBitMapEnum {
    NvCameraRawDumpBitMap_Raw       = 0x01,
    NvCameraRawDumpBitMap_RawFile   = 0x02,
    NvCameraRawDumpBitMap_RawHeader = 0x04,
    NvCameraRawDumpBitMap_DNG       = 0x08,
    NvCameraRawDumpBitMapp_Force32  = 0x7FFFFFFF
} NvCameraRawDumpBitMap;

typedef struct
{
    int width;
    int height;
} NvCameraUserResolution;

typedef struct
{
    int width;
    int height;
    int fps;
} NvCameraUserSensorMode;

typedef struct
{
    int top;
    int left;
    int right;
    int bottom;
    int weight;
} NvCameraUserWindow;

typedef struct
{
    char date[MAX_DATE_LENGTH];
    int hr;
    int min;
    int sec;
} NvCameraUserTime;

typedef struct
{
    float Near;
    float Optimal;
    float Far;
} NvCameraUserFocusDistances;

typedef struct
{
    int min;
    int max;
} NvCameraUserRange;

typedef struct
{
    int VideoWidth;
    int VideoHeight;
    int PreviewWidth;
    int PreviewHeight;
    int Maxfps;
} NvCameraUserVideoPreviewFps;

typedef enum
{
    NvCameraUserCaptureMode_Invalid = 0,
    NvCameraUserCaptureMode_Normal,
    NvCameraUserCaptureMode_Shot2Shot,

    NvCameraUserCaptureMode_Force32 = 0x7FFFFFFF
} NvCameraUserCaptureMode;

typedef enum NvCameraUserDataTapFormatEnum {
      NvCameraUserDataTapFormat_Invalid = 0,
      NvCameraUserDataTapFormat_Yuv420sp = 1,
      NvCameraUserDataTapFormat_YV12,
      NvCameraUserDataTapFormat_Yuv422p,
      NvCameraUserDataTapFormat_Yuv422sp,
      NvCameraUserDataTapFormat_Argb8888,
      NvCameraUserDataTapFormat_Rgb565,
      NvCameraUserDataTapFormat_Luma8,

      NvCameraUserDataTapFormat_Force32 = 0x7FFFFFFF
} NvCameraUserDataTapFormat;

// struct that tracks whether fields in NvCombinedCameraSettings are dirty
// or not, one bool in here should exist for each group of settings in
// NvCombinedCameraSettings.
// these dirty bits are used for the parser to mark things that have
// changed for easy detection by the driver.
// there's probably a better way to correlate and organize the data, but this
// is good enough for a prototype.
typedef struct
{
    NvBool sceneMode;
    NvBool whiteBalance;
    NvBool focusMode;
    NvBool focusPosition;
    NvBool focalLength;
    NvBool horizontalViewAngle;
    NvBool verticalViewAngle;
    NvBool flashMode;
    NvBool antiBanding;
    NvBool colorEffect;
    NvBool colorCorrectionMatrix;
    NvBool iso;
    NvBool contrast;
    NvBool saturation;
    NvBool edgeEnhancement;
    NvBool PreviewFormat;
    NvBool previewResolution;
    NvBool PreferredPreviewResolution;
    NvBool previewFpsRange;
    NvBool exposureTimeRange;
    NvBool isoRange;
    NvBool imageFormat;
    NvBool imageResolution;
    NvBool imageRotation;
    NvBool imageQuality;
    NvBool thumbnailResolution;
    NvBool thumbnailSupport;
    NvBool thumbnailEnable;
    NvBool thumbnailQuality;
    NvBool videoFrameFormat;
    NvBool videoResolution;
    NvBool PreviewFlip;
    NvBool StillFlip;
    NvBool exposureTimeMicroSec;
    NvBool exposureCompensationIndex;
    NvBool videoStabOn;
    NvBool customPostviewOn;
    NvBool timestampEnable;
    NvBool flickerMode;
    NvBool zoomLevel;
    NvBool zoomStep;
    NvBool zoomSpeed;
    NvBool nslNumBuffers;
    NvBool nslSkipCount;
    NvBool nslBurstCount;
    NvBool skipCount;
    NvBool burstCaptureCount;
    NvBool rawDumpFlag;
    NvBool exifMake;
    NvBool exifModel;
    NvBool userComment;
    NvBool GPSBitMapInfo;
    NvBool gpsLatitude;
    NvBool latitudeDirection;
    NvBool gpsLongitude;
    NvBool longitudeDirection;
    NvBool gpsAltitude;
    NvBool gpsAltitudeRef;
    NvBool gpsTimestamp;
    NvBool bracketFStopAdj;
    NvBool bracketCaptureCount;
    NvBool gpsProcMethod;
    NvBool gpsMapDatum;
    NvBool useNvBuffers;
    NvBool recordingHint;
    NvBool stillCapHdrOn;
    NvBool stillHdrSourceFramesToEncode;
    NvBool stillHdrSourceFrameCount;
    NvBool stillHdrFrameSequence;
    NvBool sensorMode;
    NvBool FocusDistances;
    NvBool stereoMode;
    NvBool FocusWindows;
    NvBool ExposureWindows;
    NvBool useCameraRenderer;
    NvBool videoSpeed;
    NvBool videoHighSpeedRec;
    NvBool AELocked;
    NvBool AWBLocked;
#ifdef FRAMEWORK_HAS_MACRO_MODE_SUPPORT
    NvBool LowLightThreshold;
    NvBool MacroModeThreshold;
#endif
    NvBool fdLimit;
    NvBool fdDebug;
    NvBool fdResult;
    NvBool autoRotation;
    NvBool CameraMode;
    NvBool CaptureMode;
    NvBool DataTapResolution;
    NvBool DataTapFormat;
    NvBool mDataTap;
    NvBool FocusMoveMsg;
    NvBool AnrInfo;
    NvBool exposureStatus;
    NvBool isoStatus;
    NvBool WhiteBalanceCCTRange;
    NvBool DisablePreviewPause;
    NvBool PreviewCbSizeEnable;
    NvBool PreviewCbSize;
} NvCombinedCameraSettingsDirty;

// Actual settings stored and used by HAL.
// when adding to this struct, make sure you add a matching field
// to NvCombinedCameraSettingsDirty
// this struct is nasty, because it mixes all sorts of data types.
// TODO: fix up with list macro?
typedef struct
{
    NvCameraUserSceneMode sceneMode;
    NvCameraUserWhitebalance whiteBalance;
    NvCameraUserFocusMode focusMode;
    int focusPosition;
    float focalLength;
    float horizontalViewAngle;
    float verticalViewAngle;
    NvCameraUserFlashMode flashMode;
    NvCameraUserAntiBanding antiBanding;
    NvCameraUserColorEffect colorEffect;
    float colorCorrectionMatrix[16];
    int iso;
    int contrast;
    int saturation;
    int edgeEnhancement;
    NvCameraUserPreviewFormat PreviewFormat;
    NvCameraUserResolution previewResolution;
    NvCameraUserResolution PreferredPreviewResolution;
    //FPS is stored in the unit of 1000. Ex, 30fps = 30000
    NvCameraUserRange previewFpsRange;
    NvCameraUserRange exposureTimeRange;
    NvCameraUserRange isoRange;
    NvCameraUserImageFormat imageFormat;
    NvCameraUserResolution imageResolution;
    int imageRotation;
    int imageQuality;
    NvCameraUserResolution thumbnailResolution;
    NvBool thumbnailSupport;
    NvBool thumbnailEnable;
    int thumbnailQuality;
    NvCameraUserVideoFrameFormat videoFrameFormat;
    NvCameraUserResolution videoResolution;
    NvCameraUserFlip PreviewFlip;
    NvCameraUserFlip StillFlip;
    int exposureTimeMicroSec;
    int exposureCompensationIndex;
    NvBool videoStabOn;
    NvBool customPostviewOn;
    NvBool timestampEnable;
    int flickerMode;
    int zoomLevel;
    float zoomStep;
    int zoomSpeed;
    int nslNumBuffers;
    int nslSkipCount;
    int nslBurstCount;
    int skipCount;
    int burstCaptureCount;
    int rawDumpFlag;
    char exifMake[EXIF_STRING_LENGTH];
    char exifModel[EXIF_STRING_LENGTH];
    char userComment[USER_COMMENT_LENGTH];
    int GPSBitMapInfo;
    unsigned int gpsLatitude;
    NvBool latitudeDirection;
    unsigned int gpsLongitude;
    NvBool longitudeDirection;
    float gpsAltitude;
    NvBool gpsAltitudeRef;
    NvCameraUserTime gpsTimestamp;
    float bracketFStopAdj[MAX_BRACKET_CAPTURES];
    int bracketCaptureCount;
    char gpsProcMethod[GPS_PROC_METHOD_LENGTH];
    char gpsMapDatum[GPS_MAP_DATUM_LENGTH];
    int useNvBuffers;
    NvBool recordingHint;
    NvBool stillCapHdrOn;
    NvBool stillHdrSourceFramesToEncode[MAX_HDR_IMAGES];
    NvU32 stillHdrSourceFrameCount;
    NvF32 stillHdrFrameSequence[MAX_HDR_IMAGES];
    NvCameraUserSensorMode sensorMode;
    NvCameraUserFocusDistances FocusDistances;
    NvCameraUserStereoMode stereoMode;
    NvCameraUserWindow FocusWindows[MAX_FOCUS_WINDOWS];
    NvCameraUserWindow ExposureWindows[MAX_EXPOSURE_WINDOWS];
    NvBool useCameraRenderer;
    float videoSpeed;
    NvBool videoHighSpeedRec;
    NvBool AELocked;
    NvBool AWBLocked;
#ifdef FRAMEWORK_HAS_MACRO_MODE_SUPPORT
    int LowLightThreshold;
    int MacroModeThreshold;
#endif
    NvBool FocusMoveMsg;
    int fdLimit;
    NvBool fdDebug;
    char fdResult[FD_OUTPUT_LEN];
    NvBool autoRotation;
    int CameraMode;
    NvCameraUserCaptureMode CaptureMode;

    //DataTap fmts, size
    NvCameraUserResolution DataTapResolution;
    NvCameraUserDataTapFormat DataTapFormat;
    NvBool mDataTap;

    AnrMode AnrInfo;
    NvCameraIspAeHwSettings exposureStatus;
    NvCameraIspISO isoStatus;

    NvCameraUserRange WhiteBalanceCCTRange;
    NvBool DisablePreviewPause;

    NvBool PreviewCbSizeEnable;
    NvCameraUserResolution PreviewCbSize;

    // This dirty bit indicates that the setting needs to be updated in NvMM.
    // It is checked in ApplyChangedSettings()
    NvCombinedCameraSettingsDirty isDirtyForNvMM;
    // This dirty bit indicates that the setting needs to be updated in the
    // parser. It is checked in UpdateSettings()
    NvCombinedCameraSettingsDirty isDirtyForParser;

} NvCombinedCameraSettings;

// Map parameter strings to meaningful values.
typedef struct
{ const char *strsetting;
  int         intsetting;
} ConversionTable_T;

// Capabilities that will be entered into the
// parameters list and checked against on setParameter
typedef struct
{
    Vector<NvCameraUserResolution> supportedPreview;
    Vector<NvCameraUserResolution> supportedPicture;
    Vector<NvCameraUserResolution> supportedVideo;
    Vector<NvCameraUserFocusMode>  supportedFocusModes;
    Vector<NvCameraUserSensorMode> supportedSensorMode;
    Vector<NvCameraUserFlashMode>  supportedFlashModes;
    Vector<NvCameraUserStereoMode> supportedStereoModes;
    Vector<NvCameraUserVideoPreviewFps> supportedVideoPreviewfps;
    NvBool hasSceneModes;
    int numAvailSensors;
    NvBool hasIsp;
    NvBool hasBracketCapture;
    int fdMaxNumFaces;
    NvBool hasVideoStabilization;
} NvCameraCapabilities;

class NvCameraSettingsParser
{
public:
    NvCameraSettingsParser();
    ~NvCameraSettingsParser();

    NvError
    parseParameters(
        const CameraParameters &params,
        NvBool allowNonStandard);
    CameraParameters getParameters(NvBool allowNonStandard) const;

    const NvCombinedCameraSettings& getCurrentSettings() const;
    const NvCombinedCameraSettings& getPrevSettings() const;

    void updateSettings(NvCombinedCameraSettings& newSettings);
    void addCapabilitySubstring(ECSType Type, const char *ToBeAdded);
    void setCapabilities(const NvCameraCapabilities& capabilities);
    // Update capabilities related to AOHDR
    void setAohdrRelatedCapabilities(const NvCameraCapabilities& capabilities);
    // Update capabilities related to high speed recording
    void updateCapabilitiesForHighSpeedRecording(
        const NvBool highSpeedRecEnable);
    void lockSceneModeCapabilities(NvCameraUserFlashMode newFlashMode,
        NvCameraUserFocusMode newFocusMode,
        NvCameraUserWhitebalance newWhiteBalanceMode);
    void unlockSceneModeCapabilities();

    // Value sanity check methods. Return true if value is within range, else false.
    NvBool checkZoomValue(int value);

    NvBool checkFpsRangeValue(const char *fpsRange);

    NvBool isFlashSupported();
    NvBool isFocusSupported();

private:
    typedef struct
    {
        int idx;
        const char *new_value;
    } NvChanges;

    NvBool  extractChanges(const CameraParameters &params,
        NvChanges changes[],
        NvBool allowNonStandard,
        int *numChanges,
        CameraParameters *newParams);

    void  buildNewSettings(const NvChanges changes[],
        int numChanges, NvCombinedCameraSettings *newSettings);

    void initializeParams(CameraParameters& initialParams);

    void removeCapabilitySubstring(ECSType Type, const char *ToBeRemoved);

    const char* findSettingsKey(ECSType setting);
    const char* findCapsKey(ECSType setting);
    const char* findKeyDefValue(ECSType setting);
    const ConversionTable_T* settingToTbl(ECSType setting);
    const char* settingValToStr(ECSType setting, int val);
    int settingStrToVal(ECSType setting, const char* str);

    void  parseSize(const char *str, int *width, int *height);
    void  parseRange(const char *str, int *min, int *max);
    void  parseInt(const char *str, int *num);
    void  parseFloat(const char *str, float *num);
    void  parseDegrees(const char *str, unsigned int *ddmmss, NvBool *direction);
    void  parseAltitude(const char *str, float *alt, NvBool *ref);
    void  parseTime(const char *str, NvCameraUserTime* time);
    void  parseOnOff(const char *str, NvBool *on);
    void  parseZoomValue(const char *str, int *zoom);
    void  parseFlipMode(const char *str, NvCameraUserFlip *pFlip);
    void  parseAnrMode(const char *str, AnrMode *mode);
    void  parseBracketCapture(const char *str, int *pBracketCaptureCount, float *bracketFStopAdj, int maxBracketCount);
    void  parseWindows(const char *str,
        NvCameraUserWindow *pWindow,
        int maxNumWindows);
    int   parseMatrix4x4(const char *str, float matrix[]);
    void  copyString(const char *str, char *settingStr, int settingStrLen);
    void  parsePictureFormat(const char *str,
         NvCameraUserImageFormat *captureFormat, NvBool *thumbSupport);
    void  parseResolution(const char *str, NvCameraUserResolution *res);
    void  parseSensorMode(const char *str, NvCameraUserSensorMode *res);
    void  parseCameraStereoMode(const char *str, NvCameraUserStereoMode *mode);
    void  parseMode(const char *str, int *width, int *height, int *fps);
    void  parseBool(const char *str, NvBool *value);
    void  parseBoolList(const char *str, NvBool *value, int maxLength);
    void  confirmThumbnailSupport(NvCombinedCameraSettings *settings);
    void  encodeResolutions(const Vector<NvCameraUserResolution>& res,
        char* str, int size);
    void  encodeFocusModes(const Vector<NvCameraUserFocusMode>& fMode,
        char* str, int size);
    void  encodeSensorModes(const Vector<NvCameraUserSensorMode>& res,
        char* str, int size);
    void encodeVideoPreviewfps(const Vector<NvCameraUserVideoPreviewFps>& res,
        char* str, int size);
    void encodePreferredPreviewSizeForVideo(const Vector<NvCameraUserResolution>& res,
        char* str, int size);
    void  encodeFlashModes(const Vector<NvCameraUserFlashMode>& fMode,
        char* str, int size);
    void  encodeStereoModes(const Vector<NvCameraUserStereoMode>& sMode,
        char* str, int size);

    NvBool mThumbnailWidthSet;
    NvBool mThumbnailHeightSet;
    NvBool mFocuserSupported;
    NvBool mFlashSupported;

    // NV settings struct understood by HAL and Handler
    NvCombinedCameraSettings mCurrentSettings;
    NvCombinedCameraSettings mPrevSettings;

    // Corresponding parameters map understood by Android
    CameraParameters   mCurrentParameters;

};

} // namespace android

#endif // __CAMERA_SETTINGS_PARSER_H__
