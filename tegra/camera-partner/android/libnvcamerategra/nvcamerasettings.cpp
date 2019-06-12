/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define LOG_TAG "NvCameraSettings"

#define LOG_NDEBUG 0

#include "nvcamerahal.h"
#include "nvcamera_isp.h"
#include "math.h"

#if ENABLE_TRIDENT
#include "tridentexample.h"
#endif

#define SMOOTH_ZOOM_TIME 1000

// Define the prefixes that we need to pass down to the JPEG encoder
// for proper handling of some EXIF header data.
// As of now, android only uses ASCII, but the others are supported by the JPEG
// encoder, so we will place hooks for them here to aid future expansion.
#define EXIF_PREFIX_LENGTH    8
#define EXIF_PREFIX_ASCII     {0x41, 0x53, 0x43, 0x49, 0x49, 0x00, 0x00, 0x00}
#define EXIF_PREFIX_JIS       {0x4A, 0x49, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00}
#define EXIF_PREFIX_UNICODE   {0x55, 0x4E, 0x49, 0x43, 0x4F, 0x44, 0x45, 0x00}
#define EXIF_PREFIX_UNDEFINED {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}

#define WINDOW_USER_2_NVMM(_f) ((float)(_f) / (ANDROID_WINDOW_PRECISION))
#define WINDOW_NVMM_2_USER(_f) ((int)round((_f) * (ANDROID_WINDOW_PRECISION)))
#define USER_2_NVMM(_a) (((_a) << 16))
#define NVMM_2_USER(_a) (((_a) + 0x8000) >> 16)

#define FASTBURST_COUNT 65535
#define FASTBURST_SKIP 3
#define FASTBURST_MAX_FRAMERATE 22000
#define FASTBURST_MIN_FRAMERATE 15000

static NvDdk2dTransform
ConvertRotationMirror2Transform(
    NvS32 Rotation,
    NvMirrorType Mirror)
{
    NvDdk2dTransform TransformSet[2][4] =
        {{ NvDdk2dTransform_None, NvDdk2dTransform_Rotate90,
           NvDdk2dTransform_Rotate180, NvDdk2dTransform_Rotate270},
         { NvDdk2dTransform_FlipVertical, NvDdk2dTransform_Transpose,
           NvDdk2dTransform_FlipHorizontal, NvDdk2dTransform_InvTranspose}};

    NvU32 i = 0;
    NvU32 j = 0;

    switch (Mirror)
    {
        case NvMirrorType_MirrorVertical:
            i = 1;
            j = 0;
           break;

        case NvMirrorType_MirrorHorizontal:
            i = 1;
            j = 2;
            break;

        case NvMirrorType_MirrorBoth:
            i = 0;
            j = 2;
            break;

        case NvMirrorType_MirrorNone:
        default:
            i = 0;
            j = 0;
            break;
    }

    switch (Rotation)
    {
        case 90:
            j += 1;
            break;

        case 180:
            j += 2;
            break;

        case 270:
            j += 3;
            break;

        case 0:
        default:
            break;
    }

    return TransformSet[i%4][j%4];
}

static NvS32 GetMappedRotation(NvS32 rotation)
{
   NvS32 value = 0;

   switch(rotation)
   {
     case 90:
         value = 270;
         break;
     case 270:
         value = 90;
         break;
     default:
         value = rotation;
         break;
   }
   return value;
}

namespace android {

// queries defaults for all sorts of things where our driver defaults
// should override whatever defaults the parser tries to come up with
NvError
NvCameraHal::GetDriverDefaults(
    NvCombinedCameraSettings& defaultSettings)
{
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    // Focal Length & FOV
    NV_CHECK_ERROR_CLEANUP(
        GetLensPhysicalAttr(
            defaultSettings.focalLength,
            defaultSettings.horizontalViewAngle,
            defaultSettings.verticalViewAngle)
    );
    defaultSettings.isDirtyForParser.focalLength = NV_TRUE;
    defaultSettings.isDirtyForParser.horizontalViewAngle = NV_TRUE;
    defaultSettings.isDirtyForParser.verticalViewAngle = NV_TRUE;

    //Hue&Brightness not in settings anymore
    //Contrast
    NV_CHECK_ERROR_CLEANUP(
        GetContrast(defaultSettings.contrast)
    );
    defaultSettings.isDirtyForParser.contrast = NV_TRUE;

    //Saturation
    NV_CHECK_ERROR_CLEANUP(
        GetSaturation(defaultSettings.saturation)
    );
    defaultSettings.isDirtyForParser.saturation = NV_TRUE;

    //White balance CCT Range
    NV_CHECK_ERROR_CLEANUP(
        GetWhitebalanceCCTRange(defaultSettings.WhiteBalanceCCTRange)
    );
    defaultSettings.isDirtyForParser.WhiteBalanceCCTRange = NV_TRUE;

    //Exposure time range
    NV_CHECK_ERROR_CLEANUP(
        GetExposureTimeRange(defaultSettings.exposureTimeRange)
    );
    defaultSettings.isDirtyForParser.exposureTimeRange = NV_TRUE;

    //ISO Sensitivity range
    NV_CHECK_ERROR_CLEANUP(
        GetIsoSensitivityRange(defaultSettings.isoRange)
    );
    defaultSettings.isDirtyForParser.isoRange = NV_TRUE;

    //FocusPosition
    NV_CHECK_ERROR_CLEANUP(
        GetFocusPosition(defaultSettings)
    );
    defaultSettings.isDirtyForParser.focusPosition = NV_TRUE;

    //TODO: CCM, seems not fully implemented yet.

    //EdgeEnhancement
    NV_CHECK_ERROR_CLEANUP(
        GetEdgeEnhancement(defaultSettings.edgeEnhancement)
    );
    defaultSettings.isDirtyForParser.edgeEnhancement = NV_TRUE;

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::updateAEParameters(NvCombinedCameraSettings &settings) const
{
    NvError e = NvSuccess;
    NvCameraIspAeHwSettings aes;
    NvCameraIspISO ISO;

    ALOGVV("%s++", __FUNCTION__);

    // update the Exposure Status
    NV_CHECK_ERROR_CLEANUP(
       Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_AeHwSettings,
            sizeof(NvCameraIspAeHwSettings),
            &aes)

    );
    NvOsMemcpy(&settings.exposureStatus, &aes, sizeof(NvCameraIspAeHwSettings));
    settings.isDirtyForParser.exposureStatus = NV_TRUE;

    // update the ISO status
    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_EffectiveISO,
            sizeof(ISO),
            &ISO)
    );
    // compensate for binning gain
    ISO.value *= aes.BinningGain;
    NvOsMemcpy(&settings.isoStatus, &ISO, sizeof(NvCameraIspISO));
    settings.isDirtyForParser.isoStatus = NV_TRUE;

    ALOGV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

#ifdef FRAMEWORK_HAS_MACRO_MODE_SUPPORT

NvError NvCameraHal::SetLowLightThreshold(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvS32 LowLightThreshold = settings.LowLightThreshold;

    ALOGVV("%s++", __FUNCTION__);

    e = Cam.Block->SetAttribute(
        Cam.Block,
        NvCameraIspAttribute_LowLightThreshold,
        0,
        sizeof(LowLightThreshold),
        &LowLightThreshold);

    ALOGVV("%s--", __FUNCTION__);
    return e;
}

NvError NvCameraHal::SetMacroModeThreshold(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvS32 MacroModeThreshold = settings.MacroModeThreshold;

    e = Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_MacroModeThreshold,
            0,
            sizeof(MacroModeThreshold),
            &MacroModeThreshold);
    return e;
}

#endif

NvError NvCameraHal::SetFocusMoveMsg(NvBool focusMoveMsg)
{
    NvBool FocusMoveMsg = focusMoveMsg;
    NvError e = NvSuccess;

    e = Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_FocusMoveMsg,
            0,
            sizeof(NvBool),
            &FocusMoveMsg);
    return e;
}

// modifies the settings struct to correspond to the desired
// scene mode settings, and locks out capabilities for any restrictive scene
// modes.
NvError
NvCameraHal::GetSceneModeSettings(NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    const NvCombinedCameraSettings& prevSettings = mSettingsParser.getPrevSettings();

    // every time a scene mode changes, this function will be called.
    // whenever that happens, we should start with a clean slate
    // (all auto settings) and then adjust the few settings that the desired
    // scene mode will change.  this is necessary because not all scene modes
    // change all possible settings.  if one only changed flash, and the next
    // one only changed WB, we don't want to be stuck with the flash setting
    // from the old one.  so this settings flush is the most elegant thing to
    // do on a scene mode change.
    GetAutoSceneModeSettings(settings);

    if (prevSettings.sceneMode == NvCameraSceneMode_Auto)
    {
        // backup white balance setting in auto mode
        mSettingsCache.whiteBalance = prevSettings.whiteBalance;
    }

    if (settings.sceneMode == NvCameraSceneMode_Auto)
    {
         // set auto scene mode's WB as last Auto mode setting
        settings.whiteBalance = mSettingsCache.whiteBalance;
        settings.isDirtyForNvMM.whiteBalance = NV_TRUE;
        settings.isDirtyForParser.whiteBalance = NV_TRUE;
    }

    switch (settings.sceneMode)
    {
        case NvCameraSceneMode_Auto:
            // don't do anything, we've already applied auto settings
            break;
        case NvCameraSceneMode_Action:
            GetActionSceneModeSettings(settings);
            break;
        case NvCameraSceneMode_Portrait:
            GetPortraitSceneModeSettings(settings);
            break;
        case NvCameraSceneMode_Landscape:
            GetLandscapeSceneModeSettings(settings);
            break;
        case NvCameraSceneMode_Beach:
            GetBeachSceneModeSettings(settings);
            break;
        case NvCameraSceneMode_Candlelight:
            GetCandlelightSceneModeSettings(settings);
            break;
        case NvCameraSceneMode_Fireworks:
            GetFireworksSceneModeSettings(settings);
            break;
        case NvCameraSceneMode_Night:
            GetNightSceneModeSettings(settings);
            break;
        case NvCameraSceneMode_NightPortrait:
            GetNightPortraitSceneModeSettings(settings);
            break;
        case NvCameraSceneMode_Party:
            GetPartySceneModeSettings(settings);
            break;
        case NvCameraSceneMode_Snow:
            GetSnowSceneModeSettings(settings);
            break;
        case NvCameraSceneMode_Sports:
            GetSportsSceneModeSettings(settings);
            break;
        case NvCameraSceneMode_SteadyPhoto:
            GetSteadyPhotoSceneModeSettings(settings);
            break;
        case NvCameraSceneMode_Sunset:
#if ENABLE_TRIDENT
            // Bug 1048966: override the sunset scene mode to call co-ISP demo code
            TridentDemo(Cam.Block);
#else  // !ENABLE_TRIDENT
            GetSunsetSceneModeSettings(settings);
#endif  // !ENABLE_TRIDENT
            break;
        case NvCameraSceneMode_Theatre:
            GetTheatreSceneModeSettings(settings);
            break;
        case NvCameraSceneMode_Barcode:
            GetBarcodeSceneModeSettings(settings);
            break;
        case NvCameraSceneMode_Backlight:
            GetBacklightSceneModeSettings(settings);
            break;
        case NvCameraSceneMode_HDR:
            GetHDRSceneModeSettings(settings);
            break;
        default:
            ALOGE(
                "%s: unrecognized scene mode, using auto settings",
                __FUNCTION__);
            // don't do anything, we've already applied auto settings
            break;
    }
    return e;
}

static NV_INLINE
NvBool isNullWindow(
    NvCameraUserWindow window)
{
    if ((window.left   == 0) &&
        (window.right  == 0) &&
        (window.top    == 0) &&
        (window.bottom == 0) &&
        (window.weight == 0))
    {
        return NV_TRUE;
    }
    return NV_FALSE;
}

// For matrix metering, customer reference device always meters full RoI
// with some weight. A window is added for the background area outside
// of the RoI windows after zoom (cropping).
// This value defines the background weight ratio
// percentage of total weight from all regions
// Set to 0 restore NV default behavior.
// For centering metering, this value has no effect.
#define ROI_BACKGROUND_WEIGHT_RATIO (0.005f)

NvError NvCameraHal::SetExposureWindows(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    const NvCameraUserWindow *ExposureWindows = settings.ExposureWindows;
    NvCameraIspExposureRegions exposureRegionsIn;
    NvF32 TotalWeight = 0.0f;
    NvS32 i;
    NvOsMemset(&exposureRegionsIn, 0, sizeof(exposureRegionsIn));

    // API specs that a (0,0,0,0,0) window will turn off windowing
    if (isNullWindow(ExposureWindows[0]))
    {
        // use default metering region/mode
        exposureRegionsIn.numOfRegions = 1;
        exposureRegionsIn.meteringMode = NVCAMERAISP_METERING_CENTER;
        TotalWeight = 1.0f;
    }
    else
    {
        for (i = 0; i < MAX_EXPOSURE_WINDOWS; i++)
        {
            // Parse until first null window found
            if (isNullWindow(ExposureWindows[i]))
                break;
            exposureRegionsIn.regions[i].left =
                WINDOW_USER_2_NVMM(ExposureWindows[i].left);
            exposureRegionsIn.regions[i].top =
                WINDOW_USER_2_NVMM(ExposureWindows[i].top);
            exposureRegionsIn.regions[i].right =
                WINDOW_USER_2_NVMM(ExposureWindows[i].right);
            exposureRegionsIn.regions[i].bottom =
                WINDOW_USER_2_NVMM(ExposureWindows[i].bottom);
            exposureRegionsIn.weights[i] = ExposureWindows[i].weight;
            TotalWeight += exposureRegionsIn.weights[i];
        }
        // Add one more region for background region
        // For now it is okay since camera core can take up to 8 regions
        // and we only allow 4 regions in HAL
        exposureRegionsIn.numOfRegions = i + 1;
        exposureRegionsIn.meteringMode = NVCAMERAISP_METERING_MATRIX;
        // Move the first region because it will be replaced by background
        // region
        exposureRegionsIn.regions[i] = exposureRegionsIn.regions[0];
        exposureRegionsIn.weights[i] = exposureRegionsIn.weights[0];
    }

    // Region 0 is reserved for background ROI
    exposureRegionsIn.weights[0] =
        ROI_BACKGROUND_WEIGHT_RATIO * TotalWeight;
    exposureRegionsIn.regions[0].top    = -1.0;
    exposureRegionsIn.regions[0].left   = -1.0;
    exposureRegionsIn.regions[0].bottom =  1.0;
    exposureRegionsIn.regions[0].right  =  1.0;

    NV_CHECK_ERROR(
        ApplyExposureRegions(&exposureRegionsIn)
    );
    return e;
}

NvError NvCameraHal::SetExposureWindowsForFaces(NvCameraUserWindow *ExposureWindows, NvU32 NumFaces)
{
    NvError e = NvSuccess;
    NvCameraIspExposureRegions exposureRegionsIn;
    NvF32 TotalWeight = 0.0f;
    NvU32 i;
    NvOsMemset(&exposureRegionsIn, 0, sizeof(exposureRegionsIn));

    if (NumFaces >= (NvU32)MAX_EXPOSURE_WINDOWS)
        NumFaces = MAX_EXPOSURE_WINDOWS - 1;

    for (i = 0; i < NumFaces; i++)
    {
        if (isNullWindow(ExposureWindows[i]))
        {
            NV_ASSERT(!"Null face window!");
        }

        exposureRegionsIn.regions[i].left =
            WINDOW_USER_2_NVMM(ExposureWindows[i].left);
        exposureRegionsIn.regions[i].top =
            WINDOW_USER_2_NVMM(ExposureWindows[i].top);
        exposureRegionsIn.regions[i].right =
            WINDOW_USER_2_NVMM(ExposureWindows[i].right);
        exposureRegionsIn.regions[i].bottom =
            WINDOW_USER_2_NVMM(ExposureWindows[i].bottom);
        exposureRegionsIn.weights[i] = ExposureWindows[i].weight;
        TotalWeight += exposureRegionsIn.weights[i];
    }
    // Add one more region for background region
    // For now it is okay since camera core can take up to 8 regions
    // and we only use 4 best face regions among detected faces.
    exposureRegionsIn.numOfRegions = i + 1;
    exposureRegionsIn.meteringMode = NVCAMERAISP_METERING_MATRIX;
    // Move the first region because it will be replaced by background
    // region
    exposureRegionsIn.regions[i] = exposureRegionsIn.regions[0];
    exposureRegionsIn.weights[i] = exposureRegionsIn.weights[0];

    // Region 0 is reserved for background ROI
    exposureRegionsIn.weights[0] =
        ROI_BACKGROUND_WEIGHT_RATIO * TotalWeight;
    exposureRegionsIn.regions[0].top    = -1.0;
    exposureRegionsIn.regions[0].left   = -1.0;
    exposureRegionsIn.regions[0].bottom =  1.0;
    exposureRegionsIn.regions[0].right  =  1.0;

    NV_CHECK_ERROR(
        ApplyExposureRegions(&exposureRegionsIn)
    );
    return e;
}

//Saturation
//value range in app, NvS32 [-100, 100]
//value range in block camera, NvS32 [0, 2]
NvError NvCameraHal::GetSaturation(NvS32 &value)
{
    NvError e = NvSuccess;
    NvF32 data;

    UNLOCKED_EVENTS_CALL(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_Saturation,
            sizeof(NvF32),
            &data)
    );

    if (e == NvSuccess)
    {
        value = (NvS32)((data - 1.0f) * 100.0f);
        return e;
    }

    //For bayer, we return the error if
    //GetAttribute fails
    if (mDefaultCapabilities.hasIsp)
        return e;

    //For YUV, the parser default is
    //set if the GetAttribute fails
    value = 0;
    return NvSuccess;
}

NvError NvCameraHal::SetSaturation(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvS32 value = settings.saturation;
    NvF32 data =
        (NvF32)(value / 100.0) + 1;

    ALOGVV("%s++ (%d)", __FUNCTION__, value);

    UNLOCKED_EVENTS_CALL_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_Saturation,
            0,
            sizeof(NvF32),
            &data)
    );

    ALOGVV("%s--", __FUNCTION__);

fail:
    return e;
}

// Table to Map values from NvCameraWhitebalance to NvCameraIspWhiteBalanceMode
static const NvCameraIspWhiteBalanceMode NvCameraWBToNvCameraIspWBMode[] = {
    NvCameraIspWhiteBalanceMode_None,            // NvCameraWhitebalance_Invalid
    NvCameraIspWhiteBalanceMode_Auto,            // NvCameraWhitebalance_Auto
    NvCameraIspWhiteBalanceMode_Incandescent,    // NvCameraWhitebalance_Incandescent
    NvCameraIspWhiteBalanceMode_Fluorescent,     // NvCameraWhitebalance_Fluorescent
    NvCameraIspWhiteBalanceMode_Sunlight,        // NvCameraWhitebalance_Daylight
    NvCameraIspWhiteBalanceMode_Cloudy,          // NvCameraWhitebalance_CloudyDaylight
    NvCameraIspWhiteBalanceMode_Shade,           // NvCameraWhitebalance_Shade
    NvCameraIspWhiteBalanceMode_Flash,           // NvCameraWhitebalance_Flash
    NvCameraIspWhiteBalanceMode_Tungsten,        // NvCameraWhitebalance_Tungsten
    NvCameraIspWhiteBalanceMode_Horizon,         // NvCameraWhitebalance_Horizon
    NvCameraIspWhiteBalanceMode_WarmFluorescent, // NvCameraWhitebalance_WarmFluorescent
    NvCameraIspWhiteBalanceMode_Twilight,        // NvCameraWhitebalance_Twilight
};

// Table to Map values from NvCameraIspWhiteBalanceMode to NvCameraWhitebalance
static const NvCameraUserWhitebalance NvCameraIspWBModeToNvCameraWB[] = {
    NvCameraWhitebalance_Daylight,              // NvCameraIspWhiteBalanceMode_Sunlight
    NvCameraWhitebalance_CloudyDaylight,        // NvCameraIspWhiteBalanceMode_Cloudy
    NvCameraWhitebalance_Shade,                 // NvCameraIspWhiteBalanceMode_Shade
    NvCameraWhitebalance_Tungsten,              // NvCameraIspWhiteBalanceMode_Tungsten
    NvCameraWhitebalance_Incandescent,          // NvCameraIspWhiteBalanceMode_Incandescent
    NvCameraWhitebalance_Fluorescent,           // NvCameraIspWhiteBalanceMode_Fluorescent
    NvCameraWhitebalance_Flash,                 // NvCameraIspWhiteBalanceMode_Flash
    NvCameraWhitebalance_Horizon,               // NvCameraIspWhiteBalanceMode_Horizon
    NvCameraWhitebalance_WarmFluorescent,       // NvCameraIspWhiteBalanceMode_WarmFluorescent
    NvCameraWhitebalance_Twilight,              // NvCameraIspWhiteBalanceMode_Twilight
    NvCameraWhitebalance_Auto,                  // NvCameraIspWhiteBalanceMode_Auto
    NvCameraWhitebalance_Invalid,               // NvCameraIspWhiteBalanceMode_None
};

// compile time check that the above table has all of the white balance modes
NV_CT_ASSERT((sizeof(NvCameraWBToNvCameraIspWBMode) / sizeof(NvCameraIspWhiteBalanceMode)) == NvCameraWhitebalance_Max);

//Whitebalance
//value range in app, NvCameraUserWhitebalance [NvCameraWhitebalance_Auto, NvCameraWhitebalance_Max]
//value range in block camera, NvCameraIspWhiteBalanceMode
//  [NvCameraIspWhiteBalanceMode_Sunlight, NvCameraIspWhiteBalanceMode_Auto]
NvError NvCameraHal::GetWhitebalance(NvCameraUserWhitebalance &userWb)
{
    NvError e = NvSuccess;
    NvCameraIspWhiteBalanceMode setting;

    UNLOCKED_EVENTS_CALL_CLEANUP(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_WhiteBalanceMode,
            sizeof(setting),
            &setting)
    );

    if (setting < NvCameraIspWhiteBalanceMode_Sunlight ||
        setting > NvCameraIspWhiteBalanceMode_None)
    {
        ALOGE(
            "%s : Unsupported WhiteBalanceMode value %d",
            __FUNCTION__,
            setting);
        return NvError_BadParameter;
    }

    userWb = NvCameraIspWBModeToNvCameraWB[setting];

fail:
    return e;
}

NvError NvCameraHal::SetWhitebalance(NvCameraUserWhitebalance userWb)
{
    NvError e = NvSuccess;
    NvBool AWB_Enable = NV_TRUE;
    NvCameraIspWhiteBalanceMode setting;

    ALOGVV("%s++ (%d)",__FUNCTION__, userWb);

    if (userWb < NvCameraWhitebalance_Auto ||
        userWb >= NvCameraWhitebalance_Max)
    {
        ALOGE(
            "%s : Unsupported WhiteBalanceMode value %d",
            __FUNCTION__,
            userWb);
        return NvError_BadParameter;
    }


    NV_CHECK_ERROR(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_ContinuousAutoWhiteBalance,
            0,
            sizeof(AWB_Enable),
            &AWB_Enable)
    );

    setting = NvCameraWBToNvCameraIspWBMode[userWb];

    NV_CHECK_ERROR(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_WhiteBalanceMode,
            0,
            sizeof(setting),
            &setting)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;
}

// Table to Map values from NvCameraColorEffect to NVCAMERAISP_STYLIZE
static const NvU32 ColorEffectToIspStylize[] = {
      0,                                          // NvCameraColorEffect_Invalid
      NVCAMERAISP_STYLIZE_AQUA,                   // NvCameraColorEffect_Aqua
      0,                                          // NvCameraColorEffect_Blackboard
      NVCAMERAISP_STYLIZE_BW,                     // NvCameraColorEffect_Mono
      NVCAMERAISP_STYLIZE_NEGATIVE,               // NvCameraColorEffect_Negative
      0,                                          // NvCameraColorEffect_None
      NVCAMERAISP_STYLIZE_POSTERIZE,              // NvCameraColorEffect_Posterize
      NVCAMERAISP_STYLIZE_SEPIA,                  // NvCameraColorEffect_Sepia
      NVCAMERAISP_STYLIZE_SOLARIZE,               // NvCameraColorEffect_Solarize
      0,                                          // NvCameraColorEffect_Whiteboard
      0,                                          // NvCameraColorEffect_Vivid
      NVCAMERAISP_STYLIZE_EMBOSS,                 // NvCameraColorEffect_Emboss
};

NvError NvCameraHal::SetColorEffect(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvCameraUserColorEffect effect = settings.colorEffect;
    NvU32 StylizeMode;
    NvBool stylizeEnable = NV_TRUE;

    ALOGVV("%s++ (%d)",__FUNCTION__, effect);

    if (effect < NvCameraColorEffect_Aqua ||
        effect > NvCameraColorEffect_Emboss)
    {
        ALOGE(
            "%s : Unsupported ColorEffect value %d",
            __FUNCTION__,
            effect);
        return NvError_BadParameter;
    }

    StylizeMode = ColorEffectToIspStylize[effect];

    if (StylizeMode == 0)
    {
        stylizeEnable = NV_FALSE;
    }

    if(stylizeEnable)
    {
        NV_CHECK_ERROR(
            Cam.Block->SetAttribute(
                Cam.Block,
                NvCameraIspAttribute_StylizeMode,
                0,
                sizeof(NvU32),
                &StylizeMode)
        );
    }

    NV_CHECK_ERROR(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_Stylize,
            0,
            sizeof(stylizeEnable),
            &stylizeEnable)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;
}

void
NvCameraHal::GetAutoSceneModeSettings(
    NvCombinedCameraSettings &settings)
{
    if (mSettingsParser.isFlashSupported())
    {
        settings.flashMode = NvCameraFlashMode_Off;
        settings.isDirtyForNvMM.flashMode = NV_TRUE;
        settings.isDirtyForParser.flashMode = NV_TRUE;
    }

    if (mSettingsParser.isFocusSupported())
    {
        settings.focusMode = NvCameraFocusMode_Auto;
        settings.isDirtyForNvMM.focusMode = NV_TRUE;
        settings.isDirtyForParser.focusMode = NV_TRUE;
    }

    settings.whiteBalance = NvCameraWhitebalance_Auto;
    settings.isDirtyForNvMM.whiteBalance = NV_TRUE;
    settings.isDirtyForParser.whiteBalance = NV_TRUE;

    settings.stillCapHdrOn = NV_FALSE;
    settings.isDirtyForNvMM.stillCapHdrOn = NV_TRUE;

    // For some of these default values we have hard-coded values, for others
    // we use the default values read in the HAL constructor. If using the
    // default values don't forget to update GetDriverDefaults() to actually
    // query NvMM for the default values.
    settings.exposureTimeRange = mSettingsCache.exposureTimeRange;
    settings.isDirtyForNvMM.exposureTimeRange = NV_TRUE;
    settings.isDirtyForParser.exposureTimeRange = NV_TRUE;

    settings.isoRange = mSettingsCache.isoRange;
    settings.isDirtyForNvMM.isoRange = NV_TRUE;
    settings.isDirtyForParser.isoRange = NV_TRUE;

    settings.WhiteBalanceCCTRange = mSettingsCache.WhiteBalanceCCTRange;
    settings.isDirtyForNvMM.WhiteBalanceCCTRange = NV_TRUE;
    settings.isDirtyForParser.WhiteBalanceCCTRange = NV_TRUE;

    settings.AnrInfo = AnrMode_Auto;
    settings.isDirtyForNvMM.AnrInfo = NV_TRUE;
    settings.isDirtyForParser.AnrInfo = NV_TRUE;

    // zero out the windows to restore default
    for (int i = 0; i < MAX_EXPOSURE_WINDOWS; i++)
    {
        settings.ExposureWindows[i].weight = 0;
        settings.ExposureWindows[i].top = 0;
        settings.ExposureWindows[i].bottom = 0;
        settings.ExposureWindows[i].left = 0;
        settings.ExposureWindows[i].right = 0;
    }
    settings.isDirtyForNvMM.ExposureWindows = NV_TRUE;
    settings.isDirtyForParser.ExposureWindows = NV_TRUE;
    // TODO add more settings that other scene modes might touch
}

void
NvCameraHal::GetActionSceneModeSettings(
    NvCombinedCameraSettings &settings)
{
    if (mSettingsParser.isFlashSupported())
    {
        settings.flashMode = NvCameraFlashMode_Auto;
        settings.isDirtyForNvMM.flashMode = NV_TRUE;
        settings.isDirtyForParser.flashMode = NV_TRUE;
    }

    if (mSettingsParser.isFocusSupported())
    {
        settings.focusMode = NvCameraFocusMode_Infinity;
        settings.isDirtyForNvMM.focusMode = NV_TRUE;
        settings.isDirtyForParser.focusMode = NV_TRUE;
    }

    settings.AnrInfo = AnrMode_Off;
    settings.isDirtyForNvMM.AnrInfo = NV_TRUE;
    settings.isDirtyForParser.AnrInfo = NV_TRUE;

    settings.exposureTimeRange.min = 0;
    settings.exposureTimeRange.max = 5;
    settings.isDirtyForNvMM.exposureTimeRange = NV_TRUE;
    settings.isDirtyForParser.exposureTimeRange = NV_TRUE;

    settings.isoRange.min = 100;
    settings.isoRange.max = 800;
    settings.isDirtyForNvMM.isoRange = NV_TRUE;
    settings.isDirtyForParser.isoRange = NV_TRUE;
}

void
NvCameraHal::GetPortraitSceneModeSettings(
    NvCombinedCameraSettings &settings)
{
    if (mSettingsParser.isFlashSupported())
    {
        settings.flashMode = NvCameraFlashMode_Auto;
        settings.isDirtyForNvMM.flashMode = NV_TRUE;
        settings.isDirtyForParser.flashMode = NV_TRUE;
    }

    if (mSettingsParser.isFocusSupported())
    {
        settings.focusMode = NvCameraFocusMode_Auto;
        settings.isDirtyForNvMM.focusMode = NV_TRUE;
        settings.isDirtyForParser.focusMode = NV_TRUE;
    }

    settings.exposureTimeRange.min = 0;
    settings.exposureTimeRange.max = 1000;
    settings.isDirtyForNvMM.exposureTimeRange = NV_TRUE;
    settings.isDirtyForParser.exposureTimeRange = NV_TRUE;

    settings.isoRange.min = 100;
    settings.isoRange.max = 800;
    settings.isDirtyForNvMM.isoRange = NV_TRUE;
    settings.isDirtyForParser.isoRange = NV_TRUE;
}

void
NvCameraHal::GetLandscapeSceneModeSettings(
    NvCombinedCameraSettings &settings)
{
    if (mSettingsParser.isFlashSupported())
    {
        settings.flashMode = NvCameraFlashMode_Auto;
        settings.isDirtyForNvMM.flashMode = NV_TRUE;
        settings.isDirtyForParser.flashMode = NV_TRUE;
    }

    if (mSettingsParser.isFocusSupported())
    {
        settings.focusMode = NvCameraFocusMode_Hyperfocal;
        settings.isDirtyForNvMM.focusMode = NV_TRUE;
        settings.isDirtyForParser.focusMode = NV_TRUE;
    }

    settings.exposureTimeRange.min = 0;
    settings.exposureTimeRange.max = 1000;
    settings.isDirtyForNvMM.exposureTimeRange = NV_TRUE;
    settings.isDirtyForParser.exposureTimeRange = NV_TRUE;

    settings.isoRange.min = 100;
    settings.isoRange.max = 800;
    settings.isDirtyForNvMM.isoRange = NV_TRUE;
    settings.isDirtyForParser.isoRange = NV_TRUE;
}

void
NvCameraHal::GetBeachSceneModeSettings(
    NvCombinedCameraSettings &settings)
{
    settings.whiteBalance = NvCameraWhitebalance_Daylight;
    settings.isDirtyForNvMM.whiteBalance = NV_TRUE;
    settings.isDirtyForParser.whiteBalance = NV_TRUE;

    if (mSettingsParser.isFlashSupported())
    {
        settings.flashMode = NvCameraFlashMode_Auto;
        settings.isDirtyForNvMM.flashMode = NV_TRUE;
        settings.isDirtyForParser.flashMode = NV_TRUE;
    }

    if (mSettingsParser.isFocusSupported())
    {
        settings.focusMode = NvCameraFocusMode_Auto;
        settings.isDirtyForNvMM.focusMode = NV_TRUE;
        settings.isDirtyForParser.focusMode = NV_TRUE;
    }
}

void
NvCameraHal::GetCandlelightSceneModeSettings(
    NvCombinedCameraSettings &settings)
{
    if (mSettingsParser.isFlashSupported())
    {
        settings.flashMode = NvCameraFlashMode_Off;
        settings.isDirtyForNvMM.flashMode = NV_TRUE;
        settings.isDirtyForParser.flashMode = NV_TRUE;
    }

    if (mSettingsParser.isFocusSupported())
    {
        settings.focusMode = NvCameraFocusMode_Auto;
        settings.isDirtyForNvMM.focusMode = NV_TRUE;
        settings.isDirtyForParser.focusMode = NV_TRUE;
    }

    settings.exposureTimeRange.min = 0;
    settings.exposureTimeRange.max = 2000;
    settings.isDirtyForNvMM.exposureTimeRange = NV_TRUE;
    settings.isDirtyForParser.exposureTimeRange = NV_TRUE;

    settings.isoRange.min = 100;
    settings.isoRange.max = 800;
    settings.isDirtyForNvMM.isoRange = NV_TRUE;
    settings.isDirtyForParser.isoRange = NV_TRUE;
}

void
NvCameraHal::GetFireworksSceneModeSettings(
    NvCombinedCameraSettings &settings)
{
    settings.whiteBalance = NvCameraWhitebalance_Auto;
    settings.isDirtyForNvMM.whiteBalance = NV_TRUE;
    settings.isDirtyForParser.whiteBalance = NV_TRUE;

    if (mSettingsParser.isFlashSupported())
    {
        settings.flashMode = NvCameraFlashMode_Off;
        settings.isDirtyForNvMM.flashMode = NV_TRUE;
        settings.isDirtyForParser.flashMode = NV_TRUE;
    }

    if (mSettingsParser.isFocusSupported())
    {
        settings.focusMode = NvCameraFocusMode_Hyperfocal;
        settings.isDirtyForNvMM.focusMode = NV_TRUE;
        settings.isDirtyForParser.focusMode = NV_TRUE;
    }

    settings.exposureTimeRange.min = 0;
    settings.exposureTimeRange.max = 2000;
    settings.isDirtyForNvMM.exposureTimeRange = NV_TRUE;
    settings.isDirtyForParser.exposureTimeRange = NV_TRUE;
}

void
NvCameraHal::GetNightSceneModeSettings(
    NvCombinedCameraSettings &settings)
{
    if (mSettingsParser.isFlashSupported())
    {
        settings.flashMode = NvCameraFlashMode_On;
        settings.isDirtyForNvMM.flashMode = NV_TRUE;
        settings.isDirtyForParser.flashMode = NV_TRUE;
    }

    if (mSettingsParser.isFocusSupported())
    {
        settings.focusMode = NvCameraFocusMode_Hyperfocal;
        settings.isDirtyForNvMM.focusMode = NV_TRUE;
        settings.isDirtyForParser.focusMode = NV_TRUE;
    }

    settings.AnrInfo = AnrMode_ForceOn;
    settings.isDirtyForNvMM.AnrInfo = NV_TRUE;
    settings.isDirtyForParser.AnrInfo = NV_TRUE;

    settings.exposureTimeRange.min = 0;
    settings.exposureTimeRange.max = 2000;
    settings.isDirtyForNvMM.exposureTimeRange = NV_TRUE;
    settings.isDirtyForParser.exposureTimeRange = NV_TRUE;
}

void
NvCameraHal::GetNightPortraitSceneModeSettings(
    NvCombinedCameraSettings &settings)
{
    if (mSettingsParser.isFlashSupported())
    {
        settings.flashMode = NvCameraFlashMode_RedEyeReduction;
        settings.isDirtyForNvMM.flashMode = NV_TRUE;
        settings.isDirtyForParser.flashMode = NV_TRUE;
    }

    if (mSettingsParser.isFocusSupported())
    {
        settings.focusMode = NvCameraFocusMode_Auto;
        settings.isDirtyForNvMM.focusMode = NV_TRUE;
        settings.isDirtyForParser.focusMode = NV_TRUE;
    }

    settings.AnrInfo = AnrMode_ForceOn;
    settings.isDirtyForNvMM.AnrInfo = NV_TRUE;
    settings.isDirtyForParser.AnrInfo = NV_TRUE;

    settings.exposureTimeRange.min = 0;
    settings.exposureTimeRange.max = 2000;
    settings.isDirtyForNvMM.exposureTimeRange = NV_TRUE;
    settings.isDirtyForParser.exposureTimeRange = NV_TRUE;

    settings.isoRange.min = 100;
    settings.isoRange.max = 800;
    settings.isDirtyForNvMM.isoRange = NV_TRUE;
    settings.isDirtyForParser.isoRange = NV_TRUE;
}

void
NvCameraHal::GetPartySceneModeSettings(
    NvCombinedCameraSettings &settings)
{
    if (mSettingsParser.isFlashSupported())
    {
        settings.flashMode = NvCameraFlashMode_Auto;
        settings.isDirtyForNvMM.flashMode = NV_TRUE;
        settings.isDirtyForParser.flashMode = NV_TRUE;
    }

    if (mSettingsParser.isFocusSupported())
    {
        settings.focusMode = NvCameraFocusMode_Auto;
        settings.isDirtyForNvMM.focusMode = NV_TRUE;
        settings.isDirtyForParser.focusMode = NV_TRUE;
    }

    settings.exposureTimeRange.min = 0;
    settings.exposureTimeRange.max = 2000;
    settings.isDirtyForNvMM.exposureTimeRange = NV_TRUE;
    settings.isDirtyForParser.exposureTimeRange = NV_TRUE;

    settings.isoRange.min = 100;
    settings.isoRange.max = 800;
    settings.isDirtyForNvMM.isoRange = NV_TRUE;
    settings.isDirtyForParser.isoRange = NV_TRUE;
}

void
NvCameraHal::GetSnowSceneModeSettings(
    NvCombinedCameraSettings &settings)
{
    NvCameraIspRange wbRange;

    if (mSettingsParser.isFlashSupported())
    {
        settings.flashMode = NvCameraFlashMode_Auto;
        settings.isDirtyForNvMM.flashMode = NV_TRUE;
        settings.isDirtyForParser.flashMode = NV_TRUE;
    }

    if (mSettingsParser.isFocusSupported())
    {
        settings.focusMode = NvCameraFocusMode_Auto;
        settings.isDirtyForNvMM.focusMode = NV_TRUE;
        settings.isDirtyForParser.focusMode = NV_TRUE;
    }

    settings.WhiteBalanceCCTRange.min = 5500;
    settings.WhiteBalanceCCTRange.max = 8500;
    settings.isDirtyForNvMM.WhiteBalanceCCTRange = NV_TRUE;
    settings.isDirtyForParser.WhiteBalanceCCTRange = NV_TRUE;
}

void
NvCameraHal::GetSportsSceneModeSettings(
    NvCombinedCameraSettings &settings)
{
    if (mSettingsParser.isFlashSupported())
    {
        settings.flashMode = NvCameraFlashMode_Off;
        settings.isDirtyForNvMM.flashMode = NV_TRUE;
        settings.isDirtyForParser.flashMode = NV_TRUE;
    }

    if (mSettingsParser.isFocusSupported())
    {
        settings.focusMode = NvCameraFocusMode_Infinity;
        settings.isDirtyForNvMM.focusMode = NV_TRUE;
        settings.isDirtyForParser.focusMode = NV_TRUE;
    }

    settings.AnrInfo = AnrMode_Off;
    settings.isDirtyForNvMM.AnrInfo = NV_TRUE;
    settings.isDirtyForParser.AnrInfo = NV_TRUE;

    settings.exposureTimeRange.min = 0;
    settings.exposureTimeRange.max = 5;
    settings.isDirtyForNvMM.exposureTimeRange = NV_TRUE;
    settings.isDirtyForParser.exposureTimeRange = NV_TRUE;

    settings.isoRange.min = 100;
    settings.isoRange.max = 800;
    settings.isDirtyForNvMM.isoRange = NV_TRUE;
    settings.isDirtyForParser.isoRange = NV_TRUE;
}

void
NvCameraHal::GetSteadyPhotoSceneModeSettings(
    NvCombinedCameraSettings &settings)
{
    if (mSettingsParser.isFlashSupported())
    {
        settings.flashMode = NvCameraFlashMode_Off;
        settings.isDirtyForNvMM.flashMode = NV_TRUE;
        settings.isDirtyForParser.flashMode = NV_TRUE;
    }

    if (mSettingsParser.isFocusSupported())
    {
        settings.focusMode = NvCameraFocusMode_Auto;
        settings.isDirtyForNvMM.focusMode = NV_TRUE;
        settings.isDirtyForParser.focusMode = NV_TRUE;
    }

    settings.exposureTimeRange.min = 0;
    settings.exposureTimeRange.max = 2000;
    settings.isDirtyForNvMM.exposureTimeRange = NV_TRUE;
    settings.isDirtyForParser.exposureTimeRange = NV_TRUE;

    settings.isoRange.min = 100;
    settings.isoRange.max = 800;
    settings.isDirtyForNvMM.isoRange = NV_TRUE;
    settings.isDirtyForParser.isoRange = NV_TRUE;
}

void
NvCameraHal::GetSunsetSceneModeSettings(
    NvCombinedCameraSettings &settings)
{
    if (mSettingsParser.isFlashSupported())
    {
        settings.flashMode = NvCameraFlashMode_Off;
        settings.isDirtyForNvMM.flashMode = NV_TRUE;
        settings.isDirtyForParser.flashMode = NV_TRUE;
    }

    if (mSettingsParser.isFocusSupported())
    {
        settings.focusMode = NvCameraFocusMode_Auto;
        settings.isDirtyForNvMM.focusMode = NV_TRUE;
        settings.isDirtyForParser.focusMode = NV_TRUE;
    }

    settings.exposureTimeRange.min = 0;
    settings.exposureTimeRange.max = 2000;
    settings.isDirtyForNvMM.exposureTimeRange = NV_TRUE;
    settings.isDirtyForParser.exposureTimeRange = NV_TRUE;

    settings.whiteBalance = NvCameraWhitebalance_Horizon;
    settings.isDirtyForNvMM.whiteBalance = NV_TRUE;
    settings.isDirtyForParser.whiteBalance = NV_TRUE;
}

void
NvCameraHal::GetTheatreSceneModeSettings(
    NvCombinedCameraSettings &settings)
{
    if (mSettingsParser.isFlashSupported())
    {
        settings.flashMode = NvCameraFlashMode_Off;
        settings.isDirtyForNvMM.flashMode = NV_TRUE;
        settings.isDirtyForParser.flashMode = NV_TRUE;
    }

    if (mSettingsParser.isFocusSupported())
    {
        settings.focusMode = NvCameraFocusMode_Infinity;
        settings.isDirtyForNvMM.focusMode = NV_TRUE;
        settings.isDirtyForParser.focusMode = NV_TRUE;
    }

    settings.isoRange.min = 100;
    settings.isoRange.max = 800;
    settings.isDirtyForNvMM.isoRange = NV_TRUE;
    settings.isDirtyForParser.isoRange = NV_TRUE;
}

void
NvCameraHal::GetBarcodeSceneModeSettings(
    NvCombinedCameraSettings &settings)
{
    if (mSettingsParser.isFlashSupported())
    {
        settings.flashMode = NvCameraFlashMode_Off;
        settings.isDirtyForNvMM.flashMode = NV_TRUE;
        settings.isDirtyForParser.flashMode = NV_TRUE;
    }

    if (mSettingsParser.isFocusSupported())
    {
        settings.focusMode = NvCameraFocusMode_Macro;
        settings.isDirtyForNvMM.focusMode = NV_TRUE;
        settings.isDirtyForParser.focusMode = NV_TRUE;
    }

    settings.edgeEnhancement = 72;
    settings.isDirtyForNvMM.edgeEnhancement = NV_TRUE;
    settings.isDirtyForParser.edgeEnhancement = NV_TRUE;

    settings.contrast = 100;
    settings.isDirtyForNvMM.contrast = NV_TRUE;
    settings.isDirtyForParser.contrast = NV_TRUE;
}

void
NvCameraHal::GetBacklightSceneModeSettings(
    NvCombinedCameraSettings &settings)
{
    if (mSettingsParser.isFlashSupported())
    {
        settings.flashMode = NvCameraFlashMode_On;
        settings.isDirtyForNvMM.flashMode = NV_TRUE;
        settings.isDirtyForParser.flashMode = NV_TRUE;
    }

    if (mSettingsParser.isFocusSupported())
    {
        settings.focusMode = NvCameraFocusMode_Auto;
        settings.isDirtyForNvMM.focusMode = NV_TRUE;
        settings.isDirtyForParser.focusMode = NV_TRUE;
    }

    settings.exposureTimeRange.min = 0;
    settings.exposureTimeRange.max = 1000;
    settings.isDirtyForNvMM.exposureTimeRange = NV_TRUE;
    settings.isDirtyForParser.exposureTimeRange = NV_TRUE;

    settings.isoRange.min = 100;
    settings.isoRange.max = 1600;
    settings.isDirtyForNvMM.isoRange = NV_TRUE;
    settings.isDirtyForParser.isoRange = NV_TRUE;

    // center-weighted window, to properly expose the foreground
    // and de-emphasize background illumination
    settings.ExposureWindows[0].weight = 1;
    settings.ExposureWindows[0].top = -250;
    settings.ExposureWindows[0].bottom = 250;
    settings.ExposureWindows[0].left = -250;
    settings.ExposureWindows[0].right = 250;
    // zero out the rest of the windows so that our center-window will
    // be dominant
    for (int i = 1; i < MAX_EXPOSURE_WINDOWS; i++)
    {
        settings.ExposureWindows[i].weight = 0;
        settings.ExposureWindows[i].top = 0;
        settings.ExposureWindows[i].bottom = 0;
        settings.ExposureWindows[i].left = 0;
        settings.ExposureWindows[i].right = 0;
    }
    settings.isDirtyForNvMM.ExposureWindows = NV_TRUE;
    settings.isDirtyForParser.ExposureWindows = NV_TRUE;
}

void
NvCameraHal::GetHDRSceneModeSettings(
    NvCombinedCameraSettings &settings)
{
    settings.stillCapHdrOn = NV_TRUE;
    settings.isDirtyForNvMM.stillCapHdrOn = NV_TRUE;

    if (mSettingsParser.isFlashSupported())
    {
        settings.flashMode = NvCameraFlashMode_Off;
        settings.isDirtyForNvMM.flashMode = NV_TRUE;
        settings.isDirtyForParser.flashMode = NV_TRUE;
    }
}

NvError
NvCameraHal::GetShot2ShotModeSettings(
    NvCombinedCameraSettings &settings,
    NvBool enable,
    NvBool useFastburst)
{
    NvError e = NvSuccess;

    if (enable)
    {
        //consider cont.shot as transients state, not update parser to avoid error
        //disable customPostviewOn
        mSettingsCache.customPostviewOn = settings.customPostviewOn;
        settings.customPostviewOn = NV_FALSE;
        settings.isDirtyForNvMM.customPostviewOn = NV_TRUE;

        DisableANR(settings);

        if (useFastburst)
        {
            //set burst capture count to a large number to support long time cont.shot
            mSettingsCache.burstCaptureCount = settings.burstCaptureCount;
            settings.burstCaptureCount = FASTBURST_COUNT;
            settings.isDirtyForNvMM.burstCaptureCount = NV_TRUE;

            //capture rate can be controlled by skip count and framerate.
            //we set skip count as 3 and frame rate between 15-22 in order to reach
            //capture rate 4-5fps which is the current capture limit
            mSettingsCache.skipCount = settings.skipCount;
            settings.skipCount = FASTBURST_SKIP;
            settings.isDirtyForNvMM.skipCount = NV_TRUE;

            mSettingsCache.previewFpsRange.min = settings.previewFpsRange.min;
            mSettingsCache.previewFpsRange.max = settings.previewFpsRange.max;
            mSettingsCache.flashMode = settings.flashMode;
            settings.previewFpsRange.min = FASTBURST_MIN_FRAMERATE;
            settings.previewFpsRange.max = FASTBURST_MAX_FRAMERATE;
            settings.isDirtyForNvMM.previewFpsRange = NV_TRUE;

            //Disable flash
            settings.flashMode = NvCameraFlashMode_Off;
            settings.isDirtyForNvMM.flashMode = NV_TRUE;
        }
    }
    else
    {
        //restore customPostviewOn
        settings.customPostviewOn = mSettingsCache.customPostviewOn;
        settings.isDirtyForNvMM.customPostviewOn = NV_TRUE;
        settings.isDirtyForParser.customPostviewOn  = NV_TRUE;

        RestoreANR(settings);

        if (useFastburst)
        {
            //restore previous settings
            settings.burstCaptureCount = mSettingsCache.burstCaptureCount;
            settings.isDirtyForNvMM.burstCaptureCount = NV_TRUE;
            settings.isDirtyForParser.burstCaptureCount = NV_TRUE;

            settings.skipCount = mSettingsCache.skipCount;
            settings.isDirtyForNvMM.skipCount = NV_TRUE;
            settings.isDirtyForParser.skipCount = NV_TRUE;

            settings.previewFpsRange.min = mSettingsCache.previewFpsRange.min;
            settings.previewFpsRange.max = mSettingsCache.previewFpsRange.max;
            settings.isDirtyForNvMM.previewFpsRange = NV_TRUE;
            settings.isDirtyForParser.previewFpsRange = NV_TRUE;

            settings.flashMode = mSettingsCache.flashMode;
            settings.isDirtyForNvMM.flashMode = NV_TRUE;
            settings.isDirtyForParser.flashMode = NV_TRUE;
        }
    }
    return e;
}

NvS32 NvCameraHal::GetFdMaxLimit()
{
    NvS32 limit = 0;
    NvError e = NvSuccess;

    UNLOCKED_EVENTS_CALL_CLEANUP(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvMMCameraAttribute_FDMaxLimit,
            sizeof(NvS32),
            &limit)
    );

    return limit;

fail:
    ALOGE(
        "%s: error getting FD max limit! (error 0x%x)",
        __FUNCTION__,
        e);
    // setting limit to 0 if lower levels error seems like a sane thing
    // to do, as the higher levels will just disable FD when they see a 0
    return 0;
}

NvError
NvCameraHal::GetSensorModeList(
    Vector<NvCameraUserSensorMode> &oSensorModeList)
{
    NvMMCameraSensorMode SensorModeList[MAX_NUM_SENSOR_MODES];
    NvCameraUserSensorMode SensorMode;
    NvError e = NvSuccess;
    int id;

    NvOsMemset(SensorModeList, 0, sizeof(SensorModeList));
    UNLOCKED_EVENTS_CALL_CLEANUP(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvMMCameraAttribute_SensorModeList,
            sizeof(SensorModeList),
            &SensorModeList[0])
    );

    for (id = 0; id < MAX_NUM_SENSOR_MODES; id++)
    {
        if ((SensorModeList[id].Resolution.width == 0) ||
            (SensorModeList[id].Resolution.height == 0))
        {
            break;
        }
        SensorMode.width     = SensorModeList[id].Resolution.width;
        SensorMode.height    = SensorModeList[id].Resolution.height;
        SensorMode.fps       = SensorModeList[id].FrameRate;
        oSensorModeList.add(SensorMode);
    }
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::GetSensorStereoCapable(NvBool &stereoSupported)
{
    NvError e = NvSuccess;
    UNLOCKED_EVENTS_CALL_CLEANUP(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvMMCameraAttribute_StereoCapable,
            sizeof(NvBool),
            &stereoSupported)
    );

fail:
    return e;
}

NvError NvCameraHal::GetSensorIspSupport(NvBool &IspSupport)
{
    NvError e = NvSuccess;
    UNLOCKED_EVENTS_CALL_CLEANUP(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvMMCameraAttribute_SensorIspSupport,
            sizeof(IspSupport),
            &IspSupport)
    );

fail:
    return e;
}

NvError NvCameraHal::GetSensorBracketCaptureSupport(NvBool &BracketSupport)
{
    NvError e = NvSuccess;
    e = Cam.Block->GetAttribute(
            Cam.Block,
            NvMMCameraAttribute_SensorBracketCaptureSupport,
            sizeof(BracketSupport),
            &BracketSupport);
    return e;
}

NvError NvCameraHal::GetSensorFocuserParam(NvCameraIspFocusingParameters &params)
{
     NvError e = NvSuccess;
     UNLOCKED_EVENTS_CALL_CLEANUP(
         Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_FocusPositionRange,
            sizeof(params),
            &params)
    );

fail:
     return e;
}

NvError NvCameraHal::SetSensorFocuserParam(NvCameraIspFocusingParameters &params)
{
     NvError e = NvSuccess;
     UNLOCK_EVENTS();
     e = Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_FocusPositionRange,
            0,
            sizeof(params),
            &params);
     RELOCK_EVENTS();
     return e;
}

NvError NvCameraHal::GetSensorFlashParam(NvBool &present)
{
     NvError e = NvSuccess;
     UNLOCKED_EVENTS_CALL_CLEANUP(
         Cam.Block->GetAttribute(
            Cam.Block,
            NvMMCameraAttribute_FlashPresent,
            sizeof(present),
            &present)
     );

fail:
     return e;
}

NvError
NvCameraHal::GetLensPhysicalAttr(
    NvF32 &focalLength,
    NvF32 &horizontalViewAngle,
    NvF32 &verticalViewAngle)
{
    NvError e = NvSuccess;
    NvMMCameraFieldOfView fov;

    UNLOCKED_EVENTS_CALL_CLEANUP(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvMMCameraAttribute_FocalLength,
            sizeof(NvF32),
            &focalLength)
    );

    UNLOCKED_EVENTS_CALL_CLEANUP(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvMMCameraAttribute_FieldOfView,
            sizeof(NvMMCameraFieldOfView),
            &fov)
    );

    horizontalViewAngle = fov.HorizontalViewAngle;
    verticalViewAngle = fov.VerticalViewAngle;

    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

//Contrast
//value range in app, int [-100, 100]
//value range in block camera, NvF32 [-1, 1]
NvError NvCameraHal::GetContrast(int &value)
{
    NvError e = NvSuccess;
    NvF32 contrastValue;

    UNLOCKED_EVENTS_CALL(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_ContrastEnhancement,
            sizeof(contrastValue),
            &contrastValue)
    );

    if (e == NvSuccess)
    {
        value = (int)(contrastValue * 100.f);
        return e;
    }

    //For bayer, we return the error if
    //GetAttribute fails
    if (mDefaultCapabilities.hasIsp)
        return e;

    //For YUV, the parser default is
    //set if the GetAttribute fails
    value = 0;
    return NvSuccess;
}

NvError NvCameraHal::SetContrast(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    int value = settings.contrast;
    NvF32 contrastValue;

    ALOGVV("%s++", __FUNCTION__);

    contrastValue = ((NvF32)value) / 100.f;

    e = Cam.Block->SetAttribute(
        Cam.Block,
        NvCameraIspAttribute_ContrastEnhancement,
        0,
        sizeof(NvF32),
        &contrastValue);

    ALOGVV("%s--", __FUNCTION__);

    return e;
}

NvError NvCameraHal::SetFastburst(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvBool enable = mFastburstEnabled;

    ALOGVV("%s++", __FUNCTION__);

    e = Cam.Block->SetAttribute(
        Cam.Block,
        NvMMCameraAttribute_FastBurstMode,
        0,
        sizeof(enable),
        &enable);

    ALOGVV("%s--", __FUNCTION__);

    return e;
}

//Zoom
//value range in app, NvS32, zoom value
//value range in block camera, NvSFx, zoom factor
NvError NvCameraHal::GetZoom(NvS32 &value)
{
    NvError e = NvSuccess;
    NvSFx zoomFactor;

    UNLOCKED_EVENTS_CALL_CLEANUP(
        DZ.Block->GetAttribute(
            DZ.Block,
            NvMMDigitalZoomAttributeType_ZoomFactor,
            sizeof(NvSFx),
            &zoomFactor)
    );

    value = ZOOM_FACTOR_TO_ZOOM_VALUE(zoomFactor);

fail:
    return e;
}

NvError NvCameraHal::SetZoom(NvS32 value, NvBool smoothZoom)
{
    NvError e = NvSuccess;
    NvU32 smoothTime;
    NvSFx newZoomFactor = ZOOM_VALUE_TO_ZOOM_FACTOR(value);
    NvSFx Scaler;

    ALOGVV("%s++", __FUNCTION__);

    // Turn smooth zoom on or off by setting smooth zoom time
    if (smoothZoom)
    {
        // Use zoom time based on log of zoomBigger/zoomSmaller
        // First, get the current zoom
        NvSFx oldZoomFactor;
        NvF32 zoomRatio;

       UNLOCKED_EVENTS_CALL_CLEANUP(
            DZ.Block->GetAttribute(
                DZ.Block,
                NvMMDigitalZoomAttributeType_ZoomFactor,
                sizeof(NvSFx),
                &Scaler)
        );

        oldZoomFactor = Scaler;

        // Calculate the ratio
        if (oldZoomFactor > newZoomFactor)
        {
            zoomRatio = oldZoomFactor / (float)newZoomFactor;
        }
        else
        {
            zoomRatio = newZoomFactor / (float)oldZoomFactor;
        }

        // Take the logarithm, multiply by constant factor
        smoothTime = (NvU32)(log(zoomRatio) * SMOOTH_ZOOM_TIME);
        ALOGVV(
            "%s: zoomRatio is %f, using time of %d",
            __FUNCTION__,
            zoomRatio,
            (int)smoothTime);
    }
    else
    {
        smoothTime = 0;
    }

    NV_CHECK_ERROR_CLEANUP(
        DZ.Block->SetAttribute(
            DZ.Block,
            NvMMDigitalZoomAttributeType_SmoothZoomTime,
            0,
            sizeof(NvU32),
            &smoothTime)
    );

    NV_CHECK_ERROR_CLEANUP(
        DZ.Block->SetAttribute(
            DZ.Block,
            NvMMDigitalZoomAttributeType_ZoomFactor,
            0,
            sizeof(NvSFx),
            &newZoomFactor)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::SetFocusWindows(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvBool focuserSupported = mSettingsParser.isFocusSupported();
    const NvCameraUserWindow *focusingWindows = settings.FocusWindows;
    NvCameraIspFocusingRegions focusRegionsIn;

    ALOGVV("%s++", __FUNCTION__);
    if (!focuserSupported)
    {
        ALOGVV("Focuser is not supported. Skipping focus window programming");
        ALOGVV("%s--", __FUNCTION__);
        return e;
    }

    NvOsMemset(&focusRegionsIn, 0, sizeof(focusRegionsIn));

    // all 0 window means AF algo can pick AF region automatically
    if ((focusingWindows[0].left == 0) &&
        (focusingWindows[0].right == 0) &&
        (focusingWindows[0].top == 0) &&
        (focusingWindows[0].bottom == 0) &&
        (focusingWindows[0].weight == 0))
    {
        // restore the default window that we saved during init, the driver
        // isn't any more clever about the windows it chooses right now
        for (int i = 0; i < MAX_FOCUS_WINDOWS; i++)
        {
            focusRegionsIn.regions[i] = mDefFocusRegions.regions[i];
        }
        focusRegionsIn.numOfRegions = MAX_FOCUS_WINDOWS;
    }
    else
    {
        for (int i = 0; i < MAX_FOCUS_WINDOWS; i++)
        {
            focusRegionsIn.regions[i].left =
                WINDOW_USER_2_NVMM(focusingWindows[i].left);
            focusRegionsIn.regions[i].top =
                WINDOW_USER_2_NVMM(focusingWindows[i].top);
            focusRegionsIn.regions[i].right =
                WINDOW_USER_2_NVMM(focusingWindows[i].right);
            focusRegionsIn.regions[i].bottom =
                WINDOW_USER_2_NVMM(focusingWindows[i].bottom);
        }
        focusRegionsIn.numOfRegions = MAX_FOCUS_WINDOWS;
    }
    e = ApplyFocusRegions(&focusRegionsIn);
    if (e != NvSuccess)
    {
        ALOGE("%s: error (0x%x) applying focus regions",__FUNCTION__, e);
    }
    ALOGVV("%s--", __FUNCTION__);
    return e;
}

NvError NvCameraHal::SetFocusWindowsForFaces(NvCameraUserWindow *focusingWindows, NvU32 NumFaces)
{
    NvError e = NvSuccess;
    NvCameraIspFocusingRegions focusRegionsIn;
    NvU32 i;

    NvOsMemset(&focusRegionsIn, 0, sizeof(focusRegionsIn));

    if (NumFaces > NVCAMERAISP_MAX_FOCUSING_REGIONS)
        NumFaces = NVCAMERAISP_MAX_FOCUSING_REGIONS;

    for (i = 0; i < NumFaces; i++)
    {
        focusRegionsIn.regions[i].left =
            WINDOW_USER_2_NVMM(focusingWindows[i].left);
        focusRegionsIn.regions[i].top =
            WINDOW_USER_2_NVMM(focusingWindows[i].top);
        focusRegionsIn.regions[i].right =
            WINDOW_USER_2_NVMM(focusingWindows[i].right);
        focusRegionsIn.regions[i].bottom =
            WINDOW_USER_2_NVMM(focusingWindows[i].bottom);
    }
    focusRegionsIn.numOfRegions = NumFaces;

    e = ApplyFocusRegions(&focusRegionsIn);
    if (e != NvSuccess)
    {
        ALOGE("%s: error applying focus regions",__FUNCTION__);
    }
    return e;
}

// ISPmode: should be one of the NVCameraIspAtrrtibute_StylizeMode
// defines in nvcamera_isp.h:
// #define NVCAMERAISP_STYLIZE_COLOR_MATRIX   (1L)
// #define NVCAMERAISP_STYLIZE_EMBOSS         (2L)
// #define NVCAMERAISP_STYLIZE_NEGATIVE       (3L)
// #define NVCAMERAISP_STYLIZE_SOLARIZE       (4L)
// #define NVCAMERAISP_STYLIZE_POSTERIZE      (5L)
// #define NVCAMERAISP_STYLIZE_SEPIA          (6L)
// #define NVCAMERAISP_STYLIZE_BW             (7L)
// #define NVCAMERAISP_STYLIZE_AQUA           (8L)
// manual and no filter modes handled separately
NvError
NvCameraHal::SetStylizeMode(
    NvU32 ISPmode,
    NvBool manualMode,
    NvBool noFilterMode)
{
    NvError e = NvSuccess;
    NvBool stylizeEnable = NV_TRUE;

    ALOGVV("%s++", __FUNCTION__);

    if (!manualMode)    // don't set style for manual.  it isn't supported
    {
        if (noFilterMode)    // don't set style and disable filtering
        {
            stylizeEnable = NV_FALSE;
        }
        else    // set style
        {
            NV_CHECK_ERROR_CLEANUP(
                Cam.Block->SetAttribute(
                    Cam.Block,
                    NvCameraIspAttribute_StylizeMode,
                    0,
                    sizeof(NvU32),
                    &ISPmode)
            );
        }
    }

    // enable/disable filtering
    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_Stylize,
            0,
            sizeof(stylizeEnable),
            &stylizeEnable)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::SetColorCorrectionMatrix(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    e = SetStylizeMode(NVX_ImageFilterManual, NV_FALSE, NV_FALSE);

    ALOGVV("%s--", __FUNCTION__);

    return e;
}

NvError NvCameraHal::SetVSTABNumBuffers()
{
    NvError e = NvSuccess;
    NvBool restartPreview = NV_FALSE;
    NvU32 bufferRequired, allocatedTotalNumBuffers;
    NvU32 width, height;
    const NvCombinedCameraSettings &Settings =
            mSettingsParser.getCurrentSettings();
    NvCameraBufferConfigStage stage =
        m_pMemProfileConfigurator->GetBufferConfigStage();
    NvCameraBufferPerfScheme perfScheme =
        m_pMemProfileConfigurator->GetBufferConfigPerfScheme();

    ALOGVV("%s++", __FUNCTION__);

    if (mPreviewStarted || mPreviewPaused)
    {
        if (mPreviewStarted)
        {
            restartPreview = NV_TRUE;
        }
        NV_CHECK_ERROR_CLEANUP(
            StopPreviewInternal()
        );
    }

    if (Settings.videoStabOn)
    {
        NV_CHECK_ERROR_CLEANUP(
            m_pMemProfileConfigurator->GetBufferAmount(
                NVCAMERA_BUFFERCONFIG_STAGE_AFTER_FIRST_PREVIEW,
                NVCAMERA_BUFFERCONFIG_VSTAB,
                NULL, &bufferRequired)
        );

        // Reset the resolution
        width  = mSettingsParser.getCurrentSettings().imageResolution.width;
        height = mSettingsParser.getCurrentSettings().imageResolution.height;

        /* forcing reconfiguration of still Buffers because NSL
         * is turned on. Lower Level Need to Suggest Proper still
         * size when NSL is on. This will also make sure NSL+Burst
         * has correct buffer size. */
        if (persistentStill[mSensorId].width == (NvS32)width &&
            persistentStill[mSensorId].height == (NvS32)height)
        {
            persistentStill[mSensorId].width -= 1;
            persistentStill[mSensorId].height -= 1;
        }
        // configure new buffers
        NV_CHECK_ERROR_CLEANUP(
            BufferManagerReconfigureStillBuffersResolution(
                width,
                height)
        );
    }
    else
    {
        NV_CHECK_ERROR_CLEANUP(
            m_pMemProfileConfigurator->GetBufferAmount(
                stage, NVCAMERA_BUFFERCONFIG_CAMERA_CAPTURE,
                NULL, &bufferRequired)
        );
    }

    if (perfScheme != NVCAMERA_BUFFER_PERF_LEAN)
    {
        // configure correct number of buffers
        NV_CHECK_ERROR_CLEANUP(
            BufferManagerReconfigureNumberOfStillBuffers(
                bufferRequired,
                bufferRequired,
                &allocatedTotalNumBuffers)
        );
    }
    else
    {
        if (bufferRequired)
            SetupLeanModeVideoBuffer(bufferRequired);
        else
            ReleaseLeanModeVideoBuffer();
    }

    if (restartPreview == NV_TRUE)
    {
        NV_CHECK_ERROR_CLEANUP(
            StartPreviewInternal()
        );
    }

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGVV("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

// requestedNumBuffersForNSL is the requested number of buffers from above level
// allocatedNumBuffersForNSL is the final allocated number of buffers for NSL
NvError NvCameraHal::SetNSLNumBuffers(
    NvU32 requestedNumBuffersForNSL,
    NvU32 *allocatedNumBuffersForNSL)
{
    NvError e = NvSuccess;
    NvU32 alreadyAllocated = 0;
    NvU32 minTotalNumBuffers = 0;
    NvU32 requestedTotalNumBuffers = 0;
    NvU32 allocatedTotalNumBuffers = 0;
    NvBool IsDirtyNslBuffer = NV_FALSE;//still or NSL
    NvBool ChangeStill = NV_FALSE;//still resolution change
    NvBool ChangeNSL = NV_FALSE;// NSL on or Off
    NvU32 width = 0;
    NvU32 height = 0;
    NvU32 bufferRequiredWithoutNSL = 0;
    const NvCombinedCameraSettings &Settings =
            mSettingsParser.getCurrentSettings();
    NvCameraBufferPerfScheme perfScheme =
        m_pMemProfileConfigurator->GetBufferConfigPerfScheme();

    ChangeStill = Settings.isDirtyForNvMM.imageResolution;
    NvU32 prevNslNumBuffers = 0;
    NvBool restartPreview = NV_FALSE;

    NvCameraBufferConfigStage stage =
        m_pMemProfileConfigurator->GetBufferConfigStage();

    ALOGVV("%s++", __FUNCTION__);

    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            stage, NVCAMERA_BUFFERCONFIG_CAMERA_CAPTURE,
            NULL, &bufferRequiredWithoutNSL)
    );

    // prevNslNumBuffers is the camera block level NSL buffer number,
    // it includes NUM_NSL_PADDING_BUFFERS
    UNLOCKED_EVENTS_CALL(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvMMCameraAttribute_NumNSLBuffers,
            sizeof(prevNslNumBuffers),
            &prevNslNumBuffers)
    );

    // Are we changing into or away from NSL?
    ChangeNSL = (prevNslNumBuffers == 0 && requestedNumBuffersForNSL > 0) ||
            (prevNslNumBuffers > 0 && requestedNumBuffersForNSL == 0);
    IsDirtyNslBuffer = requestedNumBuffersForNSL > 0;

    // We require at least MIN_NUMBER_OF_BUFFERS_FOR_NSL buffers for NSL
    if (ChangeNSL && requestedNumBuffersForNSL > 0)
    {
        // pad to ensure that the number that apps request will always be
        // available to them (usually there's 1-2 buffers that might be out to
        // VI and wouldn't necessarily be available for a NSL burst capture)
        requestedNumBuffersForNSL += NUM_NSL_PADDING_BUFFERS;

        // when in NSL mode, we have a range which we allowed
        minTotalNumBuffers = NUM_NSL_PADDING_BUFFERS + bufferRequiredWithoutNSL;
        requestedTotalNumBuffers =
            requestedNumBuffersForNSL + bufferRequiredWithoutNSL;
    }
    else
    {
        // when not or already in NSL mode, the number of buffers is already there
        minTotalNumBuffers = bufferRequiredWithoutNSL;
        requestedTotalNumBuffers = bufferRequiredWithoutNSL;
    }

    NV_CHECK_ERROR_CLEANUP(
        BufferManagerGetNumberOfStillBuffers(
            &alreadyAllocated)
    );

    if (mPreviewStarted || mPreviewPaused)
    {
        if (mPreviewStarted)
        {
            restartPreview = NV_TRUE;
        }
        NV_CHECK_ERROR_CLEANUP(
            StopPreviewInternal()
        );
    }

    allocatedTotalNumBuffers = alreadyAllocated;

    if (prevNslNumBuffers != 0)
    {
        NvU32 clear = 0;
        // need to tell NSL to disable while we change it's buffers
        NV_CHECK_ERROR_CLEANUP(
            Cam.Block->SetAttribute(
                Cam.Block,
                NvMMCameraAttribute_NumNSLBuffers,
                NvMMSetAttrFlag_Notification,
                sizeof(NvU32),
                &clear)
        );
        WaitForCondition(Cam.SetAttributeDone);
    }

    if(ChangeNSL)
    {
        NV_CHECK_ERROR_CLEANUP(
            Cam.Block->SetAttribute(
                Cam.Block,
                NvMMCameraAttribute_NSLDirtyBuffers,
                NvMMSetAttrFlag_Notification,
                sizeof(NvBool),
                &IsDirtyNslBuffer)
        );
        WaitForCondition(Cam.SetAttributeDone);
    }

    // reconfigure only when either flag is set.
    if (ChangeStill || ChangeNSL)
    {
        width  = mSettingsParser.getCurrentSettings().imageResolution.width;
        height = mSettingsParser.getCurrentSettings().imageResolution.height;

        /* forcing reconfiguration of still Buffers because NSL
         * is turned on. Lower Level Need to Suggest Proper still
         * size when NSL is on. This will also make sure NSL+Burst
         * has correct buffer size. */
        if (persistentStill[mSensorId].width == (NvS32)width &&
            persistentStill[mSensorId].height == (NvS32)height)
        {
            if(ChangeNSL)
            {
                persistentStill[mSensorId].width -= 1;
                persistentStill[mSensorId].height -= 1;
            }
        }
        // configure new buffers
        NV_CHECK_ERROR_CLEANUP(
            BufferManagerReconfigureStillBuffersResolution(
                width,
                height)
        );
    }

    if (perfScheme != NVCAMERA_BUFFER_PERF_LEAN)
    {
        // configure correct number of buffers
        NV_CHECK_ERROR_CLEANUP(
            BufferManagerReconfigureNumberOfStillBuffers(
                minTotalNumBuffers,
                requestedTotalNumBuffers,
                &allocatedTotalNumBuffers)
        );

        if (requestedNumBuffersForNSL != 0)
        {
            *allocatedNumBuffersForNSL =
                allocatedTotalNumBuffers - bufferRequiredWithoutNSL;
        }
        else
        {
            *allocatedNumBuffersForNSL = 0;
        }
    }
    else
    {
        if (ChangeNSL)
        {
            if (requestedNumBuffersForNSL)
                SetupLeanModeStillBuffer(requestedNumBuffersForNSL);
            else
                ReleaseLeanModeStillBuffer();
        }

        *allocatedNumBuffersForNSL = requestedNumBuffersForNSL;
    }

    if (*allocatedNumBuffersForNSL != 0)
    {
        NV_CHECK_ERROR_CLEANUP(
            Cam.Block->SetAttribute(
                Cam.Block,
                NvMMCameraAttribute_NumNSLBuffers,
                NvMMSetAttrFlag_Notification,
                sizeof(NvU32),
                allocatedNumBuffersForNSL)
        );
        WaitForCondition(Cam.SetAttributeDone);

        // subtract off the padding buffers we added
        *allocatedNumBuffersForNSL -= NUM_NSL_PADDING_BUFFERS;
    }

    if (requestedNumBuffersForNSL == 0 || prevNslNumBuffers != 0)
    {
         NV_CHECK_ERROR_CLEANUP(
            m_pMemProfileConfigurator->ResetBufferAmount(
                NVCAMERA_BUFFERCONFIG_STAGE_AFTER_FIRST_PREVIEW,
                NVCAMERA_BUFFERCONFIG_CAMERA_CAPTURE)
        );
    }

    if (restartPreview == NV_TRUE)
    {
        NV_CHECK_ERROR_CLEANUP(
            StartPreviewInternal()
        );
    }

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGVV("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}


NvError
NvCameraHal::SetStillHDRMode(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvBool isStillHDROn = settings.stillCapHdrOn;

    ALOGVV("%s++", __FUNCTION__);

    NV_CHECK_ERROR(
        mPostProcessStill->Enable(isStillHDROn)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;
}

NvError NvCameraHal::SetBracketCapture(
    const NvCombinedCameraSettings &settings)
{
    NvMMBracketCaptureSettings BracketCapture;
    NvU32 i;
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);
    BracketCapture.NumberImages = settings.bracketCaptureCount;
    for (i = 0; i < BracketCapture.NumberImages; i++)
    {
        BracketCapture.ExpAdj[i] = (NvF32) settings.bracketFStopAdj[i];
        BracketCapture.IspGainEnabled[i] = NV_TRUE;
    }
    NV_CHECK_ERROR(
        SetBracketCapture(BracketCapture)
    );
    ALOGVV("%s--", __FUNCTION__);
    return e;
}

NvError NvCameraHal::SetBracketCapture(NvMMBracketCaptureSettings &BracketCapture)
{
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    NV_CHECK_ERROR(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvMMCameraAttribute_BracketCapture,
            0,
            sizeof(BracketCapture),
            &BracketCapture)
    );
    ALOGVV("%s--", __FUNCTION__);
    return e;
}


NvError NvCameraHal::SetNSLSkipCount(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvU32 skipCount = (NvU32)settings.nslSkipCount;

    ALOGVV("%s++", __FUNCTION__);

    e = Cam.Block->SetAttribute(
        Cam.Block,
        NvMMCameraAttribute_NSLSkipCount,
        0,
        sizeof(skipCount),
        &skipCount);

    ALOGVV("%s--", __FUNCTION__);
    return e;
}

NvError NvCameraHal::SetRawDumpFlag(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    int rawDumpFlag = settings.rawDumpFlag;
    NvBool enable;

    ALOGVV("%s++", __FUNCTION__);

    enable = (rawDumpFlag & NvCameraRawDumpBitMap_Raw) ?
             NV_TRUE : NV_FALSE;

    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvMMCameraAttribute_DumpRaw,
            0,
            sizeof(enable),
            &enable)
    );

    enable = (rawDumpFlag & NvCameraRawDumpBitMap_RawFile) ?
             NV_TRUE : NV_FALSE;
    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvMMCameraAttribute_DumpRawFile,
            0,
            sizeof(enable),
            &enable)
    );

    enable = (rawDumpFlag & NvCameraRawDumpBitMap_RawHeader) ?
             NV_TRUE : NV_FALSE;
    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvMMCameraAttribute_DumpRawHeader,
            0,
            sizeof(enable),
            &enable)
    );

    mDNGEnabled = (rawDumpFlag & NvCameraRawDumpBitMap_DNG) ?
             NV_TRUE : NV_FALSE;

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

void NvCameraHal::SetGpsLatitude(
    bool Enable,
    unsigned int lat,
    bool dir,
    NvJpegEncParameters *Params)
{
    if (dir == true)
    {
        Params->EncParamGps.GpsInfo.GPSLatitudeRef[0] = 'N';
    }
    else
    {
        Params->EncParamGps.GpsInfo.GPSLatitudeRef[0] = 'S';
    }
    Params->EncParamGps.GpsInfo.GPSLatitudeRef[1] = '\0';

    Params->EncParamGps.GpsInfo.GPSLatitudeNumerator[0] = (lat & (0x00ff << 24)) >> 24;
    Params->EncParamGps.GpsInfo.GPSLatitudeDenominator[0] = 1;
    Params->EncParamGps.GpsInfo.GPSLatitudeNumerator[1] = (lat & (0x00ff << 16)) >> 16;
    Params->EncParamGps.GpsInfo.GPSLatitudeDenominator[1] = 1;
    Params->EncParamGps.GpsInfo.GPSLatitudeNumerator[2] = lat & 0xffff;
    Params->EncParamGps.GpsInfo.GPSLatitudeDenominator[2] = 1000;

    if (Enable)
    {
        Params->EncParamGps.GpsInfo.GPSBitMapInfo |= OMX_ENCODE_GPSBitMapLatitudeRef;
    }
    else
    {
        Params->EncParamGps.GpsInfo.GPSBitMapInfo &= ~OMX_ENCODE_GPSBitMapLatitudeRef;
    }
}

void NvCameraHal::SetGpsLongitude(
    bool Enable,
    unsigned int lon,
    bool dir,
    NvJpegEncParameters *Params)
{
    if (dir == true)
    {
        Params->EncParamGps.GpsInfo.GPSLongitudeRef[0] = 'E';
    }
    else
    {
        Params->EncParamGps.GpsInfo.GPSLongitudeRef[0] = 'W';
    }
    Params->EncParamGps.GpsInfo.GPSLongitudeRef[1] = '\0';

    Params->EncParamGps.GpsInfo.GPSLongitudeNumerator[0] = (lon & (0x00ff << 24)) >> 24;
    Params->EncParamGps.GpsInfo.GPSLongitudeDenominator[0] = 1;
    Params->EncParamGps.GpsInfo.GPSLongitudeNumerator[1] = (lon & (0x00ff << 16)) >> 16;
    Params->EncParamGps.GpsInfo.GPSLongitudeDenominator[1] = 1;
    Params->EncParamGps.GpsInfo.GPSLongitudeNumerator[2] = lon & 0xffff;
    Params->EncParamGps.GpsInfo.GPSLongitudeDenominator[2] = 1000;

    if (Enable)
    {
        Params->EncParamGps.GpsInfo.GPSBitMapInfo |=
            OMX_ENCODE_GPSBitMapLongitudeRef;
    }
    else
    {
        Params->EncParamGps.GpsInfo.GPSBitMapInfo &=
            ~OMX_ENCODE_GPSBitMapLongitudeRef;
    }
}

void NvCameraHal::SetGpsAltitude(
    bool Enable,
    float altitude,
    bool ref,
    NvJpegEncParameters *Params)
{
    Params->EncParamGps.GpsInfo.GPSAltitudeRef = (ref == true) ? 1 : 0;
    Params->EncParamGps.GpsInfo.GPSAltitudeNumerator = (int)altitude;
    Params->EncParamGps.GpsInfo.GPSAltitudeDenominator = 1;

    if (Enable)
    {
        Params->EncParamGps.GpsInfo.GPSBitMapInfo |=
            OMX_ENCODE_GPSBitMapAltitudeRef;
    }
    else
    {
        Params->EncParamGps.GpsInfo.GPSBitMapInfo &=
            ~OMX_ENCODE_GPSBitMapAltitudeRef;
    }
}

void NvCameraHal::SetGpsTimestamp(
    bool Enable,
    NvCameraUserTime time,
    NvJpegEncParameters *Params)
{
    NvOsStrncpy(
        (char *)Params->EncParamGps.GpsInfo.GPSDateStamp,
        time.date,
        sizeof(Params->EncParamGps.GpsInfo.GPSDateStamp));

    Params->EncParamGps.GpsInfo.GPSTimeStampNumerator[0] = time.hr;
    Params->EncParamGps.GpsInfo.GPSTimeStampDenominator[0] = 1;
    Params->EncParamGps.GpsInfo.GPSTimeStampNumerator[1] = time.min;
    Params->EncParamGps.GpsInfo.GPSTimeStampDenominator[1] = 1;
    Params->EncParamGps.GpsInfo.GPSTimeStampNumerator[2] = time.sec;
    Params->EncParamGps.GpsInfo.GPSTimeStampDenominator[2] = 1;

    if (Enable)
    {
        Params->EncParamGps.GpsInfo.GPSBitMapInfo |=
            (OMX_ENCODE_GPSBitMapTimeStamp | OMX_ENCODE_GPSBitMapDateStamp);
    }
    else
    {
        Params->EncParamGps.GpsInfo.GPSBitMapInfo &=
            ~(OMX_ENCODE_GPSBitMapTimeStamp | OMX_ENCODE_GPSBitMapDateStamp);
    }
}

void NvCameraHal::SetGpsProcMethod(
    bool Enable,
    const char *str,
    NvJpegEncParameters *Params)
{
    char prefix[EXIF_PREFIX_LENGTH] = EXIF_PREFIX_ASCII;
    NvOsMemcpy(
        Params->EncParamGpsProc.GpsProcMethodInfo.GPSProcessingMethod,
        prefix,
        EXIF_PREFIX_LENGTH);
    NvOsStrncpy(
        (char *)(Params->EncParamGpsProc.GpsProcMethodInfo.GPSProcessingMethod +
            EXIF_PREFIX_LENGTH),
        str,
        sizeof(Params->EncParamGpsProc.GpsProcMethodInfo.GPSProcessingMethod) -
            EXIF_PREFIX_LENGTH);
    if (Enable)
    {
        Params->EncParamGps.GpsInfo.GPSBitMapInfo |=
            OMX_ENCODE_GPSBitMapProcessingMethod;
    }
    else
    {
        Params->EncParamGps.GpsInfo.GPSBitMapInfo &=
            ~OMX_ENCODE_GPSBitMapProcessingMethod;
    }
}

NvError
NvCameraHal::SetGPSSettings(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvMMGPSInfo test;
    NvJpegEncParameters Params;

    ALOGVV("%s++", __FUNCTION__);

    Params.EnableThumbnail = NV_FALSE;
    NvImageEnc_GetParam(mImageEncoder, &Params);
    SetGpsLatitude(
        (settings.GPSBitMapInfo & NvCameraGpsBitMap_LatitudeRef),
        settings.gpsLatitude,
        settings.latitudeDirection,
        &Params);
    SetGpsLongitude(
        (settings.GPSBitMapInfo & NvCameraGpsBitMap_LongitudeRef),
        settings.gpsLongitude,
        settings.longitudeDirection,
        &Params);
    SetGpsAltitude(
        (settings.GPSBitMapInfo & NvCameraGpsBitMap_AltitudeRef),
        settings.gpsAltitude,
        settings.gpsAltitudeRef,
        &Params);
    SetGpsTimestamp(
        (settings.GPSBitMapInfo & (NvCameraGpsBitMap_TimeStamp |
        NvCameraGpsBitMap_DateStamp)),
        settings.gpsTimestamp,
        &Params);
    SetGpsProcMethod(
        (settings.GPSBitMapInfo & NvCameraGpsBitMap_ProcessingMethod),
        settings.gpsProcMethod,
        &Params);
    Params.EncParamGps.Enable = NV_TRUE;
    NvImageEnc_SetParam(mImageEncoder, &Params);

    ALOGVV("%s--", __FUNCTION__);
    return e;
}

//ExposureTimeMicroSec
//value range in app, int in micro seconds
//value range in block, NvCameraIspExposureTime in seconds
NvError NvCameraHal::GetExposureTimeMicroSec(int &exposureTimeMicroSec)
{
    NvError e = NvSuccess;
    NvCameraIspExposureTime et;

    UNLOCKED_EVENTS_CALL_CLEANUP(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_ExposureTime,
            sizeof(et),
            &et)
    );

    if (et.isAuto)
    {
        exposureTimeMicroSec = 0;
    }
    else
    {
        exposureTimeMicroSec = (int)(et.value * 1000000.0f);
    }

fail:
    return e;
}

NvError NvCameraHal::SetExposureTimeMicroSec(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    int exposureTimeMicroSec = settings.exposureTimeMicroSec;
    NvCameraIspExposureTime et;

    ALOGVV("%s++", __FUNCTION__);

    if (exposureTimeMicroSec == 0)
    {
        et.isAuto = true;
    }
    else
    {
        et.isAuto = false;
        // set this flag to make takePicture wait for sometime
        // to ensure that the settings are programmed to the sensor
        // and program the requested wait time for this setting
        mWaitForManualSettings = NV_TRUE;
        SetFinalWaitCountInFrames(EXP_TIME_FRAME_WAIT_COUNT);
    }

    et.value = exposureTimeMicroSec/1000000.0f;
    NV_CHECK_ERROR(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_ExposureTime,
            NvMMSetAttrFlag_Notification,
            sizeof(et),
            &et)
    );
    WaitForCondition(Cam.SetAttributeDone);

    ALOGVV("%s--", __FUNCTION__);
    return e;
}

NvError
NvCameraHal::SetPreviewPauseAfterStillCapture(
    NvCombinedCameraSettings &settings,
    NvBool pause)
{
    NvError e = NvSuccess;
    e = Cam.Block->SetAttribute(
            Cam.Block,
            NvMMCameraAttribute_PausePreviewAfterStillCapture,
            0,
            sizeof(NvBool),
            &pause);

    // note that the API that we use to disable preview pause is
    // the inverse of the arg to this function, which is specifying
    // whether preview is paused.
    if (settings.DisablePreviewPause != !pause)
    {
        settings.isDirtyForParser.DisablePreviewPause = NV_TRUE;
        settings.DisablePreviewPause = !pause;
    }
    return e;
}

NvError NvCameraHal::GetWhitebalanceCCTRange(NvCameraUserRange &wbRange)
{
    NvError e = NvSuccess;
    NvCameraIspRange range;


    UNLOCKED_EVENTS_CALL(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_WhiteBalanceCCTRange,
            sizeof(range),
            &range)
    );

    if (e == NvSuccess)
    {
        wbRange.min = range.low;
        wbRange.max = range.high;
        return e;
    }

    //For bayer, we return the error if
    //GetAttribute fails
    if (mDefaultCapabilities.hasIsp)
        return e;

    //For YUV, the parser default is
    //set if the GetAttribute fails
    //we don't have parse default for
    //this parameter so we put dummy
    //value here
    wbRange.min = 0;
    wbRange.max = 0;
    return NvSuccess;
}

NvError NvCameraHal::SetWhitebalanceCCTRange(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvCameraUserRange wbRange = settings.WhiteBalanceCCTRange;
    NvCameraIspRange range;

    range.low = wbRange.min;
    range.high = wbRange.max;

    NV_CHECK_ERROR(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_WhiteBalanceCCTRange,
            0,
            sizeof(range),
            &range)
    );
    return e;
}

NvError NvCameraHal::GetExposureTimeRange(NvCameraUserRange &ExpRange)
{
    NvError e = NvSuccess;
    NvCameraIspRange oRange;

    UNLOCKED_EVENTS_CALL(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_ExposureTimeRange,
            sizeof(oRange),
            &oRange)
    );

    if (e == NvSuccess)
    {
        ExpRange.min = NV_SFX_TO_WHOLE(oRange.low);
        ExpRange.max = NV_SFX_TO_WHOLE(oRange.high);
        return e;
    }

    //For bayer, we return the error if
    //GetAttribute fails
    if (mDefaultCapabilities.hasIsp)
        return e;

    //For YUV, the parser default is
    //set if the GetAttribute fails
    //we don't have parse default for
    //this parameter so we put dummy
    //value here
    ExpRange.min = 0;
    ExpRange.max = 0;
    return NvSuccess;
}

NvError NvCameraHal::SetExposureTimeRange(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvCameraIspRange oRange;
    NvCameraUserRange ExpRange = settings.exposureTimeRange;

    oRange.low = NV_SFX_WHOLE_TO_FX((NvS32)ExpRange.min);
    oRange.high = NV_SFX_WHOLE_TO_FX((NvS32)ExpRange.max);

    NV_CHECK_ERROR(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_ExposureTimeRange,
            0,
            sizeof(oRange),
            &oRange)
    );
    return e;
}

NvError NvCameraHal::GetIsoSensitivityRange(NvCameraUserRange &IsoRange)
{
    NvError e = NvSuccess;
    NvCameraIspRange oRange;

    UNLOCKED_EVENTS_CALL(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_ISORange,
            sizeof(oRange),
            &oRange)
    );

    if (e == NvSuccess)
    {
        IsoRange.min = NV_SFX_TO_WHOLE(oRange.low);
        IsoRange.max = NV_SFX_TO_WHOLE(oRange.high);
        return e;
    }

    //For bayer, we return the error if
    //GetAttribute fails
    if (mDefaultCapabilities.hasIsp)
        return e;

    //For YUV, the parser default is
    //set if the GetAttribute fails
    //we don't have parse default for
    //this parameter so we put dummy
    //value here
    IsoRange.min = 0;
    IsoRange.max = 0;
    return NvSuccess;
}

NvError NvCameraHal::SetIsoSensitivityRange(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvCameraUserRange IsoRange = settings.isoRange;
    NvCameraIspRange oRange;

    oRange.low = NV_SFX_WHOLE_TO_FX((NvS32)IsoRange.min);
    oRange.high = NV_SFX_WHOLE_TO_FX((NvS32)IsoRange.max);

    NV_CHECK_ERROR(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_ISORange,
            0,
            sizeof(oRange),
            &oRange)
    );
    return e;
}

NvError NvCameraHal::GetFrameRateRange(NvS32 &lowFPS, NvS32 &highFPS)
{
    NvCameraIspRange Range;
    NvError e = NvSuccess;

    UNLOCKED_EVENTS_CALL_CLEANUP(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_AutoFrameRateRange,
            sizeof(Range),
            &Range)
    );

    lowFPS = NVMM_2_USER(Range.low) * 1000;
    highFPS = NVMM_2_USER(Range.high) * 1000;

fail:
    return e;
}

NvError NvCameraHal::SetFrameRateRange(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvCameraIspRange Range = {0, 0};
    NvBool data = NV_TRUE;

    NV_CHECK_ERROR(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_AutoFrameRate,
            0,
            sizeof(data),
            &data)
    );

    // casting to NvS64 first to prevent overflow
    Range.low  = (NvS32) (USER_2_NVMM((NvS64) settings.previewFpsRange.min) / 1000);
    Range.high = (NvS32) (USER_2_NVMM((NvS64) settings.previewFpsRange.max) / 1000);
    NV_CHECK_ERROR(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_AutoFrameRateRange,
            0,
            sizeof(Range),
            &Range)
    );

    return e;
}

NvError NvCameraHal::SetRecordingHint(const NvBool enable)
{
    NvError e = NvSuccess;

    NV_CHECK_ERROR(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvMMCameraAttribute_RecordingHint,
            0,
            sizeof(NvBool),
            const_cast<NvBool *>(&enable)));

    return e;
}

NvError NvCameraHal::SetAdvancedNoiseReductionMode(AnrMode anrMode)
{
    NvError e = NvSuccess;
    NvBool enable = NV_FALSE;
    NvU32 aMode;

    ALOGVV("%s : anrMode=%s",
        __FUNCTION__,
        (anrMode == AnrMode_Off) ? "AnrMode_Off" :
        (anrMode == AnrMode_ForceOn) ? "AnrMode_ForceOn"  :
        (anrMode == AnrMode_Auto) ? "AnrMode_Auto": "AnrMode_Other");

    // check if blocklet has started up
    UNLOCKED_EVENTS_CALL_CLEANUP(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvMMCameraAttribute_ChromaFiltering,
            sizeof(enable),
            &enable)
    );

    // start blocklet if it hasn't started yet
    if (!enable)
    {
        enable = NV_TRUE;
        NV_CHECK_ERROR(
            Cam.Block->SetAttribute(
                Cam.Block,
                NvMMCameraAttribute_ChromaFiltering,
                0,
                sizeof(enable),
                &enable)
        );
    }

    if (mIsVideo)
        anrMode = AnrMode_Off;
    aMode = (NvU32)anrMode;

    NV_CHECK_ERROR(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvMMCameraAttribute_AdvancedNoiseReductionMode,
            0,
            sizeof(aMode),
            &aMode)
    );

fail:
    return e;
}


NvError NvCameraHal::InformVideoMode(NvBool isVideo)
{
    NvError e = NvSuccess;
    mIsVideo = isVideo;

    // set all of the things that make different decisions based
    // on whether it is in video mode or not
    NV_CHECK_ERROR(
        SetAdvancedNoiseReductionMode(
            mAnrMode)
    );
    return e;
}


NvError NvCameraHal::SetVideoSpeed(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvF32 videoSpeed = (NvF32) settings.videoSpeed;
    NvMMCameraVideoSpeed sVideoSpeed = {0, 0};
    sVideoSpeed.nVideoSpeed = videoSpeed;
    // Enable video speed only for following 2 conditions
    // 1. If Slow motion is set, i,e., 0.25x and 0.5x
    // 2. High Speed recording is enabled
    if((sVideoSpeed.nVideoSpeed < 1.0) ||
        (settings.videoHighSpeedRec == NV_TRUE))
    {
        sVideoSpeed.bEnable = NV_TRUE;
    }
    e = Cam.Block->SetAttribute(
            Cam.Block,
            NvMMCameraAttribute_VideoSpeed,
            0,
            sizeof(NvMMCameraVideoSpeed),
            &sVideoSpeed);

    mSettingsParser.updateCapabilitiesForHighSpeedRecording(
        sVideoSpeed.bEnable);

    return e;
}

//EdgeEnhancementBias
//value range in app, int [-101, 100]
//value range in block camera, NvF32 [-1, 1]
NvError NvCameraHal::GetEdgeEnhancement(int &edgeEnhancement)
{
    NvError e = NvSuccess;
    NvBool enable;
    NvF32 data;

    UNLOCKED_EVENTS_CALL(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_EdgeEnhancement,
            sizeof(enable),
            &enable)
    );

    if (e != NvSuccess)
    {
        if (mDefaultCapabilities.hasIsp)
            return e;

        //For YUV, if the parameter is
        //not supported, we return a dummy
        //default
        edgeEnhancement = 0;
        return NvSuccess;
    }

    if (enable)
    {
        UNLOCKED_EVENTS_CALL_CLEANUP(
            Cam.Block->GetAttribute(
                Cam.Block,
                NvCameraIspAttribute_EdgeEnhanceStrengthBias,
                sizeof(data),
                &data)
        );

        if (data > 1.f || data < -1.f)
        {
            ALOGVV("Clamp strength bias to 0 from %f", data);
            edgeEnhancement = 0;
        }
        else
        {
            edgeEnhancement = (int)(data * 100.f);
        }
    }
    else
    {
        edgeEnhancement = EdgeEnhancementDisable;
    }

fail:
    return e;
}

NvError NvCameraHal::SetEdgeEnhancement(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvF32 strengthBias;
    NvBool enable;

    if (settings.edgeEnhancement == EdgeEnhancementDisable)
    {
        enable = NV_FALSE;
        strengthBias = 0.f;
    }
    else
    {
        enable = NV_TRUE;
        strengthBias = settings.edgeEnhancement / 100.f;
    }

    ALOGVV("%s: enable %d, strengthBias %f", __FUNCTION__, enable, strengthBias);
    NV_CHECK_ERROR(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_EdgeEnhancement,
            0,
            sizeof(enable),
            &enable)
    );

    if (enable) {
        NV_CHECK_ERROR(
            Cam.Block->SetAttribute(
                Cam.Block,
                NvCameraIspAttribute_EdgeEnhanceStrengthBias,
                0,
                sizeof(strengthBias),
                &strengthBias)
        );
    }
    return e;
}

static NvError GetNvMirrorType(
    NvCameraUserFlip UserFlip,
    NvMirrorType     *NvFlip)
{
    NvError e = NvSuccess;
    switch (UserFlip)
    {
        case NvCameraUserFlip_None:
            *NvFlip = NvMirrorType_MirrorNone;
            break;
        case NvCameraUserFlip_Vertical:
            *NvFlip = NvMirrorType_MirrorVertical;
            break;
        case NvCameraUserFlip_Horizontal:
            *NvFlip = NvMirrorType_MirrorHorizontal;
            break;
        case NvCameraUserFlip_Both:
            *NvFlip = NvMirrorType_MirrorBoth;
            break;
        default:
            ALOGE("Undefined UserFlip Settings!");
            e = NvError_BadParameter;
            break;
    }
    return e;
}

NvError NvCameraHal::SetImageFlipRotation(
    const NvCombinedCameraSettings &settings,
    const NvBool isStill)
{
    NvError e = NvSuccess;
    NvDdk2dTransform transform = NvDdk2dTransform_None;
    // we don't use rotation in video mode
    NvBool autoRotation = (settings.autoRotation && isStill);
    // When autoRotation = false, image is not rotated physically
    NvS32 rotation = (NvS32) (autoRotation ? settings.imageRotation : 0);
    NvCameraUserFlip userFlip =
        (isStill) ? settings.StillFlip : settings.PreviewFlip;
    NvMirrorType mirror =  NvMirrorType_MirrorNone;

    ALOGVV("%s++", __FUNCTION__);

    NV_CHECK_ERROR_CLEANUP(
        GetNvMirrorType(userFlip, &mirror)
    );
    rotation = GetMappedRotation(rotation);
    transform = ConvertRotationMirror2Transform(rotation, mirror);

    if (isStill)
    {
        NV_CHECK_ERROR_CLEANUP(
            DZ.Block->SetAttribute(
                DZ.Block,
                NvMMDigitalZoomAttributeType_StillTransform,
                NvMMSetAttrFlag_Notification,
                sizeof(NvDdk2dTransform),
                &transform)
        );
        WaitForCondition(DZ.SetAttributeDone);
        NV_CHECK_ERROR_CLEANUP(
            DZ.Block->SetAttribute(
                DZ.Block,
                NvMMDigitalZoomAttributeType_ThumbnailTransform,
                NvMMSetAttrFlag_Notification,
                sizeof(NvDdk2dTransform),
                &transform)
        );
        WaitForCondition(DZ.SetAttributeDone);
    }
    else
    {
        NV_CHECK_ERROR_CLEANUP(
            DZ.Block->SetAttribute(
                DZ.Block,
                NvMMDigitalZoomAttributeType_PreviewTransform,
                NvMMSetAttrFlag_Notification,
                sizeof(NvDdk2dTransform),
                &transform)
        );
        WaitForCondition(DZ.SetAttributeDone);
    }

    // Update AE/AF region since we might change preview transform
    // Since we always get current dz transform in update functions
    // we dont need to consider isStill here
    // Todo: figure out if it is correct to only update region when preview
    //       transform is changed. If so we can save these two NvMM calls here.
    NV_CHECK_ERROR_CLEANUP(
        SetExposureWindows(settings)
    );
    NV_CHECK_ERROR_CLEANUP(
        SetFocusWindows(settings)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

// Table to Map values from NvCameraFlashMode into NVMMCAMERA_FLASHMODE
static const NvU32 FlashModeMapping [] = {
    0,                                     // Illegal value
    NVMMCAMERA_FLASHMODE_STILL_AUTO,       // NvCameraFlashMode_Auto
    NVMMCAMERA_FLASHMODE_STILL_MANUAL,     // NvCameraFlashMode_On
    NVMMCAMERA_FLASHMODE_OFF,              // NvCameraFlashMode_Off
    NVMMCAMERA_FLASHMODE_TORCH,            // NvCameraFlashMode_Torch
    NVMMCAMERA_FLASHMODE_REDEYE_REDUCTION, // NvCameraFlashMode_RedEyeReduction
};

NvError
NvCameraHal::NvCameraUserFlashModeToNvMMFlashMode(
    NvCameraUserFlashMode userFlash,
    NvU32 &nvmmFlash)
{
    if (userFlash < NvCameraFlashMode_Auto ||
        userFlash >= NvCameraFlashMode_Max)
    {
        ALOGE(
            "%s: Unsupported FlashMode value %d",
            __FUNCTION__,
            userFlash);
        return NvError_BadParameter;
    }

    nvmmFlash = FlashModeMapping[userFlash];
    return NvSuccess;
}

// Table to Map values from NVMMCAMERA_FLASHMODE into NvCameraFlashMode
static const NvCameraUserFlashMode FlashModeInvMapping [] = {
    NvCameraFlashMode_Off,            // NVMMCAMERA_FLASHMODE_OFF
    NvCameraFlashMode_Torch,          // NVMMCAMERA_FLASHMODE_TORCH
    NvCameraFlashMode_Invalid,        // NVMMCAMERA_FLASHMODE_MOVIE Note: No match in NvCameraUserFlashMode
    NvCameraFlashMode_On,             // NVMMCAMERA_FLASHMODE_STILL_MANUAL
    NvCameraFlashMode_Auto,           // NVMMCAMERA_FLASHMODE_STILL_AUTO
    NvCameraFlashMode_RedEyeReduction,// NVMMCAMERA_FLASHMODE_REDEYE_REDUCTION
};

NvError
NvCameraHal::NvMMFlashModeToNvCameraUserFlashMode(
    NvU32 nvmmFlash,
    NvCameraUserFlashMode &userFlash)
{
    if (nvmmFlash > NVMMCAMERA_FLASHMODE_REDEYE_REDUCTION)
    {
        ALOGE(
            "%s: Unsupported FlashMode value %d",
            __FUNCTION__,
            userFlash);
        return NvError_BadParameter;
    }

    userFlash = FlashModeInvMapping[nvmmFlash];
    return NvSuccess;
}

NvError
NvCameraHal::GetFlashMode(
    NvCameraUserFlashMode &userFlash)
{
    NvError e = NvSuccess;
    NvU32 nvmmFlash;
    NvCameraUserFlashMode tmpUserFlash = NvCameraFlashMode_Invalid;

    if (!mSettingsParser.isFlashSupported())
    {
        userFlash = NvCameraFlashMode_Off;
    }
    else
    {
        UNLOCKED_EVENTS_CALL_CLEANUP(
            Cam.Block->GetAttribute(
                Cam.Block,
                NvMMCameraAttribute_FlashMode,
                sizeof(nvmmFlash),
                &nvmmFlash)
        );

        // Mapping NVMMCAMERA_FLASHMODE into NvCameraFlashMode values
        NV_CHECK_ERROR(
            NvMMFlashModeToNvCameraUserFlashMode(nvmmFlash, tmpUserFlash)
        );
        userFlash = tmpUserFlash;
    }

fail:
    return e;
}

// Setting Flashmode attribute, based on the user selected FlashMode
// Also checking for burstCapture.If BurstCaptures are performed, then
// flash is disabled.
NvError
NvCameraHal::SetFlashMode(
    const NvCombinedCameraSettings &settings)
{
    NvU32 FlashMode = 0;
    NvError e = NvSuccess;
    NvBool burstCapture = NV_FALSE;
    NvCameraUserFlashMode userFlash = settings.flashMode;

    if (settings.burstCaptureCount > 1 || settings.nslBurstCount > 0)
    {
       burstCapture = NV_TRUE;
    }
    // Override the userFlash value to disable flash if there is no flash
    // or if the camera is performing NSL or standard burst captures.
    if (!mSettingsParser.isFlashSupported() || burstCapture)
    {
        userFlash = NvCameraFlashMode_Off;
    }

    ALOGVV("%s: %d", __FUNCTION__, userFlash);

    // Mapping NvCameraFlashMode into NVMMCAMERA_FLASHMODE values
    NV_CHECK_ERROR(
        NvCameraUserFlashModeToNvMMFlashMode(userFlash, FlashMode)
    );

    NV_CHECK_ERROR(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvMMCameraAttribute_FlashMode,
            0,
            sizeof(NvU32),
            &FlashMode)
    );

    return e;
}

/*
 * UserMode    Control fPos        CAF_EN
 * Auto        Auto    NOOP        off
 * Infinity    On      Inf         off
 * Macro       On      macro       off
 * HyperFocal  On      hyperfocal  off
 * Fixed       On      hyperfocal  off
 * ContVideo   NOOP    NOOP        on
 * ContPic     NOOP    NOOP        on
 */
NvError NvCameraHal::SetFocusMode(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvCameraUserFocusMode userFocusMode = settings.focusMode;
    NvCameraIspFocusControl focusControl;
    NvS32 focusPos;
    NvBool focuserSupported = mSettingsParser.isFocusSupported();
    NvBool cafEnable = NV_FALSE;
    NvBool setFocus = NV_FALSE;
    NvBool setCAF = NV_FALSE;
    NvBool setFocusPos = NV_FALSE;
    NvS32 hyperFocalPos = -1;
    NvS32 infPos = -1;
    NvS32 macroPos = -1;
    NvCameraIspFocusingParameters focusParameters;

    ALOGVV("%s++", __FUNCTION__);

    UNLOCKED_EVENTS_CALL_CLEANUP(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_FocusPositionRange,
            sizeof(focusParameters),
            &focusParameters)
    );

    hyperFocalPos = focusParameters.hyperfocal;
    macroPos = focusParameters.macro;
    // AF TODO: use minPos for now because no infinity position
    // defined yet.
    infPos = focusParameters.minPos;

    // translate user focus mode to nv focus control
    if (focuserSupported)
    {
        setFocus = NV_TRUE;
        setCAF = NV_TRUE;
    }
    else
    {
        userFocusMode = NvCameraFocusMode_Fixed;
    }

    switch (userFocusMode)
    {
        case NvCameraFocusMode_Auto:
            focusControl = NvCameraIspFocusControl_Auto;
            break;

        case NvCameraFocusMode_Infinity:
            focusControl = NvCameraIspFocusControl_On;
            focusPos = infPos;
            setFocusPos = NV_TRUE;
            break;

        case NvCameraFocusMode_Macro:
            focusControl = NvCameraIspFocusControl_On;
            focusPos = macroPos;
            // Android API: macro mode should be applied in Autofocus()
            // switch to manual mode but set position in autoFocusInternal()
            // Comment: Android API is ambiguous on Macro mode.
            // The current implementation: enable focuser to move to MACRO
            // position upon start and stay there. The focus position can be
            // refined around the MACRO position, but it's not done.
            setFocusPos = NV_TRUE;
            break;

        case NvCameraFocusMode_Hyperfocal:
            focusControl = NvCameraIspFocusControl_On;
            userFocusMode = NvCameraFocusMode_Infinity;
            focusPos = hyperFocalPos;
            setFocusPos = NV_TRUE;
            break;

        case NvCameraFocusMode_Fixed:
            if (focuserSupported)
            {
                // For a camera with focuser, fixed focus mode usually means
                // that the focus is at hyperfocal distance
                focusControl = NvCameraIspFocusControl_On;
                focusPos = hyperFocalPos;
                setFocusPos = NV_TRUE;
            }
            else
            {
                // Nothing to do, it's a fixed-focus module
                setFocus = NV_FALSE;
                setCAF = NV_FALSE;
            }
            break;
        case NvCameraFocusMode_Continuous_Video:
            focusControl = NvCameraIspFocusControl_Continuous_Video;
            cafEnable = NV_TRUE;
            break;
        case NvCameraFocusMode_Continuous_Picture:
            focusControl = NvCameraIspFocusControl_Continuous_Picture;
            cafEnable = NV_TRUE;
            break;

        default:
            ALOGDD(
                "%s: unsupported user setting [%d]",
                __FUNCTION__,
                userFocusMode);
            userFocusMode = NvCameraFocusMode_Infinity;
            setFocus = NV_FALSE;
            break;
    }

    if (setFocus)
    {
        NV_CHECK_ERROR_CLEANUP(
            Cam.Block->SetAttribute(
                Cam.Block,
                NvCameraIspAttribute_AutoFocusControl,
                0,
                sizeof(NvCameraIspFocusControl),
                &focusControl)
        );
    }

    if (setFocusPos)
    {
        NV_CHECK_ERROR_CLEANUP(
            Cam.Block->SetAttribute(
                Cam.Block,
                NvCameraIspAttribute_FocusPosition,
                0,
                sizeof(focusPos),
                &focusPos)
        );
    }

    if (setCAF)
    {
        NV_CHECK_ERROR(
            Cam.Block->SetAttribute(
                Cam.Block,
                NvCameraIspAttribute_ContinuousAutoFocus,
                NvMMSetAttrFlag_Notification,
                sizeof(cafEnable),
                &cafEnable)
        );
        WaitForCondition(Cam.SetAttributeDone);
    }

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

//FocusPosition
//value range in app, NvS32
//value range in block camera, NvS32
NvError NvCameraHal::GetFocusPosition(
    NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvS32 blockFocusPosition;

    UNLOCKED_EVENTS_CALL(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_FocusPosition,
            sizeof(blockFocusPosition),
            &blockFocusPosition)
    );

    if (e == NvSuccess)
    {
        settings.focusPosition = (int)blockFocusPosition;
        return e;
    }

    //For bayer, we return the error if
    //GetAttribute fails
    if (mDefaultCapabilities.hasIsp)
    {
        ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
        return e;
    }

    //For YUV, the parser default is
    //set if the GetAttribute fails
    settings.focusPosition = 0;
    return NvSuccess;
}

NvError NvCameraHal::SetFocusPosition(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvS32 focusPosition = (NvS32) settings.focusPosition;

    ALOGVV("%s++", __FUNCTION__);

    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_FocusPosition,
            0,
            sizeof(focusPosition),
            &focusPosition)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::SetPreviewFormat(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvCameraUserPreviewFormat previewFormat = settings.PreviewFormat;
    NvMMDigitalZoomFrameCopyColorFormat colorFormat;

    ALOGVV("%s++", __FUNCTION__);

    NvOsMemset(&colorFormat, 0, sizeof(NvMMDigitalZoomFrameCopyColorFormat));

    switch (previewFormat)
    {
        case NvCameraPreviewFormat_Yuv420sp:
            colorFormat.NumSurfaces = 2;
            colorFormat.SurfaceColorFormat[0] = NvColorFormat_Y8;
            colorFormat.SurfaceColorFormat[1] = NvColorFormat_V8_U8;
            break;
        case NvCameraPreviewFormat_YV12:
            colorFormat.NumSurfaces = 3;
            colorFormat.SurfaceColorFormat[0] = NvColorFormat_Y8;
            colorFormat.SurfaceColorFormat[1] = NvColorFormat_V8;
            colorFormat.SurfaceColorFormat[2] = NvColorFormat_U8;
            break;
        default:
            ALOGE("%s: Undefined preview format (0x%d)",
                __FUNCTION__, previewFormat);
            return NvError_BadParameter;
    }

    NV_CHECK_ERROR_CLEANUP(
        DZ.Block->SetAttribute(
            DZ.Block,
            NvMMDigitalZoomAttributeType_PreviewFrameCopyColorFormat,
            0,
            sizeof(colorFormat),
            &colorFormat)
    );

    NV_CHECK_ERROR_CLEANUP(
        DZ.Block->SetAttribute(
            DZ.Block,
            NvMMDigitalZoomAttributeType_StillConfirmationFrameCopyColorFormat,
            0,
            sizeof(colorFormat),
            &colorFormat)
    );

    NV_CHECK_ERROR_CLEANUP(
        DZ.Block->SetAttribute(
            DZ.Block,
            NvMMDigitalZoomAttributeType_StillFrameCopyColorFormat,
            0,
            sizeof(colorFormat),
            &colorFormat)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::GetStabilizationMode(
    NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvCameraIspStabilizationMode sMode;

    UNLOCKED_EVENTS_CALL_CLEANUP(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_Stabilization,
            sizeof(sMode),
            &sMode)
    );

    if (sMode == NvCameraIspStabilizationMode_Video)
    {
        settings.videoStabOn = NV_TRUE;
    }
    else
    {
        settings.videoStabOn = NV_FALSE;
    }

fail:
    return e;
}

NvError NvCameraHal::SetStabilizationMode(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvCameraIspStabilizationMode sMode = NvCameraIspStabilizationMode_None;

    ALOGVV("%s++ StabOn %d", __FUNCTION__, settings.videoStabOn);

    if (settings.videoStabOn)
    {
        NvBool Lean = NV_FALSE;
        // vstab can't run at full power if we're in lean mode
        NvCameraBufferFootprintScheme FootprintScheme =
            m_pMemProfileConfigurator->GetBufferFootprintScheme();

        if (FootprintScheme == NVCAMERA_BUFFER_FOOTPRINT_LEAN)
        {
            Lean = NV_TRUE;
        }

        NV_CHECK_ERROR_CLEANUP(
            Cam.Block->SetAttribute(
                Cam.Block,
                NvMMCameraAttribute_VideoStabilizationLeanMode,
                0,
                sizeof(NvBool),
                &Lean)
        );

        sMode = NvCameraIspStabilizationMode_Video;
    }

    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_Stabilization,
            0,
            sizeof(sMode),
            &sMode)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

//AntiBanding
//value range in app, NvCameraUserAntiBanding
//value range in block camera, NvU32
NvError NvCameraHal::GetAntiBanding(NvCameraUserAntiBanding &mode)
{
    NvError e = NvSuccess;
    NvBool enable;
    NvU32 value;

    UNLOCKED_EVENTS_CALL_CLEANUP(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_AntiFlicker,
            sizeof(enable),
            &enable)
    );

    if (enable)
    {
        UNLOCKED_EVENTS_CALL_CLEANUP(
            Cam.Block->GetAttribute(
                Cam.Block,
                NvCameraIspAttribute_FlickerFrequency,
                sizeof(value),
                &value)
        );

        switch (value)
        {
            case NVCAMERAISP_FLICKER_AUTO:
                mode = NvCameraAntiBanding_Auto;
                break;

            case NVCAMERAISP_FLICKER_50HZ:
                mode = NvCameraAntiBanding_50Hz;
                break;

            case NVCAMERAISP_FLICKER_60HZ:
                mode = NvCameraAntiBanding_60Hz;
                break;

            default:
                mode = NvCameraAntiBanding_Off;
                break;
        }
    }
    else
    {
        mode = NvCameraAntiBanding_Off;
    }

fail:
    return e;
}

NvError NvCameraHal::SetAntiBanding(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvCameraUserAntiBanding mode = settings.antiBanding;
    NvBool enable;
    NvU32 value;

    ALOGVV("%s++ (%d)", __FUNCTION__, mode);

    if (mode != NvCameraAntiBanding_Off)
    {
        enable = NV_TRUE;
    }
    else
    {
        enable = NV_FALSE;
    }

    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_AntiFlicker,
            0,
            sizeof(enable),
            &enable)
    );

    if (enable == NV_TRUE)
    {
        value = NvCameraAntiBanding_Auto;
        if (mode == NvCameraAntiBanding_50Hz)
        {
            value = NvCameraAntiBanding_50Hz;
        }
        else if (mode == NvCameraAntiBanding_60Hz)
        {
            value = NvCameraAntiBanding_60Hz;
        }

        NV_CHECK_ERROR_CLEANUP(
            Cam.Block->SetAttribute(
                Cam.Block,
                NvCameraIspAttribute_FlickerFrequency,
                0,
                sizeof(value),
                &value)
        );
    }

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::SetIsoSensitivity(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    int iso = settings.iso;
    NvSFx data;
    NvSFx xEVCompensation;
    NvCameraIspISO ISO;
    NvCameraIspExposureTime et;

    ALOGVV("%s++", __FUNCTION__);

    UNLOCKED_EVENTS_CALL(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_ExposureTime,
            sizeof(et),
            &et)
    );

    if (e != NvSuccess)
    {
        // If driver does not support this GetAttribute,
        // Provide default values
        ALOGWW("%s Get ExposureTime fails, setting defaults",
                __func__);
        et.isAuto = NV_FALSE;
        et.value = 0;
        e = NvSuccess;
    }

    UNLOCKED_EVENTS_CALL(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_EffectiveISO,
            sizeof(ISO),
            &ISO)
    );

    if (e != NvSuccess)
    {
        // If driver does not support this GetAttribute,
        // Provide default values
        ALOGWW("%s Get EffectiveISO fails, setting defaults",
                __func__);
        ISO.isAuto = NV_FALSE;
        ISO.value = 0;
        e = NvSuccess;
    }

    UNLOCKED_EVENTS_CALL(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_ExposureCompensation,
            sizeof(NvSFx),
            &(xEVCompensation))
    );

    if (e != NvSuccess)
    {
        // If driver does not support this GetAttribute,
        // Provide default values
        ALOGWW("%s Get ExposureCompensation fails, setting defaults",
                __func__);
        xEVCompensation = 0;
        e = NvSuccess;
    }

    UNLOCKED_EVENTS_CALL(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_MeteringMode,
            sizeof(NvS32),
            &data)
    );

    if (e != NvSuccess)
    {
        // If driver does not support this GetAttribute,
        // Provide default values
        ALOGWW("%s Get MeteringMode fails, setting defaults",
                __func__);
        data = 0;
        e = NvSuccess;
    }


    // Set the bAutoSensitivity to TRUE in case user
    // selects ISO sensitivity to Auto(0)
    if (iso == 0)
    {
        ISO.isAuto = NV_TRUE;
        ISO.value = 100;
        ALOGVV("%s: Setting ISO to AUTO",__FUNCTION__);
    }
    else
    {
        ISO.isAuto = NV_FALSE;
        ISO.value = iso;
        // set this flag to make takePicture wait for sometime
        // to ensure that the settings are programmed to the sensor
        // and program the requested wait time for this setting
        mWaitForManualSettings = NV_TRUE;
        SetFinalWaitCountInFrames(ISO_FRAME_WAIT_COUNT);
        ALOGVV("%s: Setting ISO to [%d]",__FUNCTION__,(int)iso);
    }

    //Always apply exposure time to auto
    et.isAuto = NV_TRUE;

    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_ExposureTime,
            0,
            sizeof(et),
            &et)
    );
    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_ExposureCompensation,
            0,
            sizeof(NvSFx),
            &(xEVCompensation))
    );
    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_MeteringMode,
            0,
            sizeof(NvS32),
            &data)
    );
    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_EffectiveISO,
            NvMMSetAttrFlag_Notification,
            sizeof(ISO),
            &ISO)
    );
    // Wait for SetAttributes to complete
    WaitForCondition(Cam.SetAttributeDone);

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::SetExposureCompensation(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    int exposureCompensationIndex = settings.exposureCompensationIndex;
    NvSFx data;
    NvSFx xEVCompensation;
    NvCameraIspISO ISO;
    NvCameraIspExposureTime et;
    NvBool updateConfig = NV_FALSE;
    float step = 0.0;
    float exposureCompensation = 0.0;

    ALOGVV("%s++", __FUNCTION__);

    UNLOCKED_EVENTS_CALL(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_ExposureTime,
            sizeof(et),
            &et)
    );

    if (e != NvSuccess)
    {
        // If driver does not support this GetAttribute,
        // Provide default values
        ALOGWW("%s Get ExposureTime fails, setting defaults",
                __func__);
        et.isAuto = NV_FALSE;
        et.value = 0;
        e = NvSuccess;
    }

    UNLOCKED_EVENTS_CALL(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_EffectiveISO,
            sizeof(ISO),
            &ISO)
    );

    if (e != NvSuccess)
    {
        // If driver does not support this GetAttribute,
        // Provide default values
        ALOGWW("%s Get EffectiveISO fails, setting defaults",
                __func__);
        ISO.isAuto = NV_FALSE;
        ISO.value = 0;
        e = NvSuccess;
    }

    UNLOCKED_EVENTS_CALL(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_ExposureCompensation,
            sizeof(NvSFx),
            &(xEVCompensation))
    );

    if (e != NvSuccess)
    {
        // If driver does not support this GetAttribute,
        // Provide default values
        ALOGWW("%s Get ExposureCompensation fails, setting defaults",
                __func__);
        xEVCompensation = 0;
        e = NvSuccess;
    }

    UNLOCKED_EVENTS_CALL(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_MeteringMode,
            sizeof(NvS32),
            &data)
    );

    if (e != NvSuccess)
    {
        // If driver does not support this GetAttribute,
        // Provide default values
        ALOGWW("%s Get MeteringMode fails, setting defaults",
                __func__);
        data = 0;
        e = NvSuccess;
    }

    // ExposureValue is a large struct that controls
    // all the settings for auto exposure. Please see
    // OMX_IVCommon.h for more details.

    // Leave auto exposure running
    et.isAuto = NV_TRUE;
    ISO.isAuto = NV_TRUE;

    step = strtof(EXP_COMPENSATION_STEP, 0);
    exposureCompensation = exposureCompensationIndex * step;
    exposureCompensationIndex = (NvS32)(exposureCompensation * (1 << 16));
    // Update the exposure compensation
    if (xEVCompensation != exposureCompensationIndex)
    {
        xEVCompensation = exposureCompensationIndex;
        updateConfig = NV_TRUE;
        // set this flag to make takePicture wait for sometime
        // to ensure that the settings are programmed to the sensor
        // and program the requested wait time for this setting
        mWaitForManualSettings = NV_TRUE;
        SetFinalWaitCountInFrames(EXP_COMPENSATION_FRAME_WAIT_COUNT);
    }

    if (updateConfig == NV_TRUE)
    {
        NV_CHECK_ERROR_CLEANUP(
            Cam.Block->SetAttribute(
                Cam.Block,
                NvCameraIspAttribute_ExposureTime,
                0,
                sizeof(et),
                &et)
        );
        NV_CHECK_ERROR_CLEANUP(
            Cam.Block->SetAttribute(
                Cam.Block,
                NvCameraIspAttribute_EffectiveISO,
                0,
                sizeof(ISO),
                &ISO)
        );
        NV_CHECK_ERROR_CLEANUP(
            Cam.Block->SetAttribute(
                Cam.Block,
                NvCameraIspAttribute_MeteringMode,
                0,
                sizeof(NvS32),
                &data)
        );
        NV_CHECK_ERROR_CLEANUP(
            Cam.Block->SetAttribute(
                Cam.Block,
                NvCameraIspAttribute_ExposureCompensation,
                NvMMSetAttrFlag_Notification,
                sizeof(NvSFx),
                &(xEVCompensation))
        );
        WaitForCondition(Cam.SetAttributeDone);
    }

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::SetAELock(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvBool locked = settings.AELocked;
    NvBool relock = !(mPreviewStarted || mPreviewStreaming);

    NV_CHECK_ERROR(
        programAlgLock(
            AlgLock_AE,
            locked,
            relock,
            ALG_LOCK_TIMEOUT_IMMEDIATE,
            NvMMCameraAlgorithmSubType_None)
    );
    // this function call will always trigger an event handler
    // when we're locking.  the app expects the alg to be locked
    // when we're done applying the settings, so we should block
    // until it's done.
    // however, if (1) preview isn't already running,
    // (2) in video recording (frames are returned by API calls),
    // we can not wait on the event as
    // the alg won't have enough frames to converge

    if (locked && mPreviewStreaming && !mVideoStreaming)
    {
        WaitForCondition(mAELockReceived);
    }
    return e;
}

NvError NvCameraHal::SetAWBLock(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvBool locked = settings.AWBLocked;
    NvBool relock = !(mPreviewStarted || mPreviewStreaming);

    NV_CHECK_ERROR(
        programAlgLock(
            AlgLock_AWB,
            locked,
            relock,
            locked,
            NvMMCameraAlgorithmSubType_None)
    );
    // this function call will always trigger an event handler
    // when we're locking.  the app expects the alg to be locked
    // when we're done applying the settings, so we should block
    // until it's done.
    // however, if (1) preview isn't already running,
    // (2) in video recording (frames are returned by API calls),
    // we can not wait on the event as
    // the alg won't have enough frames to converge

    if (locked && mPreviewStreaming && !mVideoStreaming)
    {
        WaitForCondition(mAWBLockReceived);
    }
    return e;
}

NvError NvCameraHal::ProgramDeviceRotation(NvS32 rotation)
{
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvMMCameraAttribute_CommonRotation,
            0,
            sizeof(NvS32),
            &rotation)
    );

    ALOGVV("%s--", __FUNCTION__);

    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::SetFdDebug(
    const NvCombinedCameraSettings &settings)
{
    NvError e = NvSuccess;
    NvBool enableFdDebug = NV_FALSE;

    ALOGVV("%s++", __FUNCTION__);

    if (settings.fdDebug)
        enableFdDebug = NV_TRUE;

    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvMMCameraAttribute_FDDebug,
            0,
            sizeof(NvBool),
            &enableFdDebug)
    );

    ALOGVV("%s--", __FUNCTION__);

    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::programAlgLock(
    NvU32 algs,
    NvBool lock,
    NvBool relock,
    NvU32 timeout,
    NvU32 algSubType)
{
    NvU32 Algs;
    NvError e = NvSuccess;
    NvMMCameraAlgorithmFlags algorithms;
    NvCameraIspExposureRegions AeExposureRegions;
    NvBool bAutoFocus = (algs & AlgLock_AF) ? NV_TRUE : NV_FALSE;
    NvBool bAutoExposure = (algs & AlgLock_AE) ? NV_TRUE : NV_FALSE;
    NvBool bAutoWhiteBalance = (algs & AlgLock_AWB) ? NV_TRUE : NV_FALSE;

    ALOGVV("%s++", __FUNCTION__);

    // prevent redundant lock requests
    if (lock)
    {
        // Note: the silly looking casts are needed to make the compiler not
        // emit a warning about these statments
        bAutoExposure = mAELocked ? (NvBool) NV_FALSE : bAutoExposure;
        bAutoWhiteBalance = mAWBLocked ? (NvBool) NV_FALSE : bAutoWhiteBalance;
    }

    mAELocked = bAutoExposure ? lock : mAELocked;
    mAWBLocked = bAutoWhiteBalance ? lock : mAWBLocked;

    Algs = bAutoFocus ? NvMMCameraAlgorithmFlags_AutoFocus : 0;
    Algs |= bAutoExposure ? NvMMCameraAlgorithmFlags_AutoExposure : 0;
    Algs |= bAutoWhiteBalance ? NvMMCameraAlgorithmFlags_AutoWhiteBalance : 0;
    algorithms = (NvMMCameraAlgorithmFlags)Algs;

    if (!lock)
    {
        UNLOCKED_EVENTS_CALL(
            Cam.Block->Extension(
                Cam.Block,
                NvMMCameraExtension_UnlockAlgorithms,
                sizeof(algorithms),
                &algorithms,
                0,
                NULL)
        );
        if (e != NvSuccess)
        {
            ALOGE("%s: unlock fail! error:0x%x", __FUNCTION__, e);
            return e;
        }
    }
    else
    {
        NvMMCameraConvergeAndLockParamaters parameters;

        // AF don't like the subtype to be None
        if (bAutoFocus && algSubType == NvMMCameraAlgorithmSubType_None)
        {
            algSubType = NvMMCameraAlgorithmSubType_AFFullRange;
        }
        // Only one bit of subtype should be set. If not, set it to AFFullRange
        if (((NvU32)algSubType & ((NvU32)algSubType - 1)) != 0)
        {
            algSubType = NvMMCameraAlgorithmSubType_AFFullRange;
        }

        parameters.algs = algorithms;
        parameters.milliseconds = timeout;
        parameters.relock = relock;
        parameters.algSubType = algSubType;
        UNLOCKED_EVENTS_CALL(
            Cam.Block->Extension(
                Cam.Block,
                NvMMCameraExtension_ConvergeAndLockAlgorithms,
                sizeof(parameters),
                &parameters,
                0,
                NULL)
        );
        if (e != NvSuccess)
        {
            ALOGE("%s: lock fail! error:0x%x", __FUNCTION__, e);
            return e;
        }
    }

    ALOGVV("%s--", __FUNCTION__);
    return e;
}

NvError
NvCameraHal::ApplyExposureRegions(NvCameraIspExposureRegions *exposureRegionsIn)
{
    NvCameraIspExposureRegions ExposureRegions;
    NvMMDigitalZoomOperationInformation DZInfo;
    NvU32 i = 0;
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    NvOsMemset(&ExposureRegions, 0, sizeof(ExposureRegions));

    // get the preview information in order to compute the correct
    // exposure region w.r.t camera's preview buffer
    UNLOCKED_EVENTS_CALL_CLEANUP(
        DZ.Block->GetAttribute(
            DZ.Block,
            NvMMDigitalZoomAttributeType_OperationInformation,
            sizeof(DZInfo),
            &DZInfo)
    );

    ExposureRegions.numOfRegions = exposureRegionsIn->numOfRegions;
    // copy the regions, accounting for whatever DZ transforms we need
    CameraMappingPreviewOutputRegionToInputRegion(
        ExposureRegions.regions,
        &DZInfo,
        exposureRegionsIn->regions,
        ExposureRegions.numOfRegions);

    // copy the weights and metering modes
    for (i = 0; i < ExposureRegions.numOfRegions; i++)
    {
        ExposureRegions.weights[i] = exposureRegionsIn->weights[i];
    }
    ExposureRegions.meteringMode = exposureRegionsIn->meteringMode;

    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_AutoExposureRegions,
            0,
            sizeof(ExposureRegions),
            &ExposureRegions)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError
NvCameraHal::ApplyFocusRegions(NvCameraIspFocusingRegions *focusRegionsIn)
{
    NvCameraIspFocusingRegions fRegionsOut;
    NvMMDigitalZoomOperationInformation DZInfo;
    NvU32 i = 0;
    NvError e = NvSuccess;

    NvOsMemset(&fRegionsOut, 0, sizeof(fRegionsOut));

    // get the preview information in order to compute the correct
    // exposure region w.r.t camera's preview buffer
    UNLOCKED_EVENTS_CALL_CLEANUP(
        DZ.Block->GetAttribute(
            DZ.Block,
            NvMMDigitalZoomAttributeType_OperationInformation,
            sizeof(DZInfo),
            &DZInfo)
    );

    fRegionsOut.numOfRegions = focusRegionsIn->numOfRegions;
    // copy the regions, accounting for whatever DZ transforms we need
    CameraMappingPreviewOutputRegionToInputRegion(
        fRegionsOut.regions,
        &DZInfo,
        focusRegionsIn->regions,
        focusRegionsIn->numOfRegions);

    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvCameraIspAttribute_AutoFocusRegions,
            0,
            sizeof(fRegionsOut),
            &fRegionsOut)
    );

    return e;

fail:

    ALOGE("%s: (error 0x%x)",__FUNCTION__, e);
    return e;
}

void
NvCameraHal::CameraMappingPreviewOutputRegionToInputRegion(
    NvRectF32 *pCameraRegions,
    NvMMDigitalZoomOperationInformation *pDZInfo,
    NvRectF32 *pPreviewOutputRegions,
    NvU32 NumOfRegions)
{
    NvU32 i = 0;
    NvF32 CamW = 0.0f;
    NvF32 CamH = 0.0f;
    NvF32 CropW = 0.0f;
    NvF32 CropH = 0.0f;
    NvF32 CropStartX = 0.0f;
    NvF32 CropStartY = 0.0f;

    CamW = (NvF32)pDZInfo->PreviewInputSize.width;
    CamH = (NvF32)pDZInfo->PreviewInputSize.height;
    CropW = (NvF32)(pDZInfo->PreviewInputCropRect.right -
                    pDZInfo->PreviewInputCropRect.left);
    CropH = (NvF32)(pDZInfo->PreviewInputCropRect.bottom -
                    pDZInfo->PreviewInputCropRect.top);
    CropStartX = (NvF32)pDZInfo->PreviewInputCropRect.left;
    CropStartY = (NvF32)pDZInfo->PreviewInputCropRect.top;

    for (i = 0; i < NumOfRegions; i++)
    {
        NvRectF32 *pRectCam = &pCameraRegions[i];
        NvRectF32 *pRectClient = &pPreviewOutputRegions[i];

        if (CamW == 0 && CamH == 0)
        {
            // preview isn't enabled yet
            pRectCam->left   = pRectClient->left;
            pRectCam->top    = pRectClient->top;
            pRectCam->right  = pRectClient->right;
            pRectCam->bottom = pRectClient->bottom;

            continue;
        }
        // consider transform
        switch (pDZInfo->PreviewTransform)
        {
            case NvDdk2dTransform_Rotate90:
                pRectCam->left = -pRectClient->bottom;
                pRectCam->top = pRectClient->left;
                pRectCam->right = -pRectClient->top;
                pRectCam->bottom = pRectClient->right;
                break;
            case NvDdk2dTransform_Rotate180:
                pRectCam->left = -pRectClient->right;
                pRectCam->top = -pRectClient->bottom;
                pRectCam->right = -pRectClient->left;
                pRectCam->bottom = -pRectClient->top;
                break;
            case NvDdk2dTransform_Rotate270:
                pRectCam->left = pRectClient->top;
                pRectCam->top = -pRectClient->right;
                pRectCam->right = pRectClient->bottom;
                pRectCam->bottom = -pRectClient->left;
                break;
            case NvDdk2dTransform_FlipHorizontal:
                pRectCam->left = -pRectClient->right;
                pRectCam->top = pRectClient->top;
                pRectCam->right = -pRectClient->left;
                pRectCam->bottom = pRectClient->bottom;
                break;
            case NvDdk2dTransform_FlipVertical:
                pRectCam->left = pRectClient->left;
                pRectCam->top = -pRectClient->bottom;
                pRectCam->right = pRectClient->right;
                pRectCam->bottom = -pRectClient->top;
                break;
            case NvDdk2dTransform_Transpose:
                pRectCam->left = pRectClient->top;
                pRectCam->top = pRectClient->left;
                pRectCam->right = pRectClient->bottom;
                pRectCam->bottom = pRectClient->right;
                break;
            case NvDdk2dTransform_InvTranspose:
                pRectCam->left = -pRectClient->bottom;
                pRectCam->top = -pRectClient->right;
                pRectCam->right = -pRectClient->top;
                pRectCam->bottom = -pRectClient->left;
                break;
            case NvDdk2dTransform_None:
                pRectCam->left = pRectClient->left;
                pRectCam->top = pRectClient->top;
                pRectCam->right = pRectClient->right;
                pRectCam->bottom = pRectClient->bottom;
            default:
                break;
        }

        // consider input crop
        pRectCam->left = (CropW + CropW * pRectCam->left +
                          2 * CropStartX - CamW) / CamW;
        pRectCam->right = (CropW + CropW * pRectCam->right +
                           2 * CropStartX - CamW) / CamW;
        pRectCam->top = (CropH + CropH * pRectCam->top +
                         2 * CropStartY - CamH) / CamH;
        pRectCam->bottom = (CropH + CropH * pRectCam->bottom +
                            2 * CropStartY - CamH) / CamH;
    }
}

NvError NvCameraHal::ProgramExifInfo()
{
    NvError e = NvSuccess;
    NvJpegEncParameters Params;
    NvImageExifParams *pExifParams;
    NvCombinedCameraSettings settings = mSettingsParser.getCurrentSettings();

    ALOGVV("%s++", __FUNCTION__);

    NvOsMemset(&Params, 0, sizeof(NvJpegEncParameters));

    NV_CHECK_ERROR(
        NvImageEnc_GetParam(mImageEncoder, &Params)
    );
    pExifParams = &Params.EncParamExif;

    // set user comment (needs prefix)
    char prefix[EXIF_PREFIX_LENGTH] = EXIF_PREFIX_ASCII;
    NvOsMemcpy(pExifParams->ExifInfo.UserComment, prefix, EXIF_PREFIX_LENGTH);
    NvOsStrncpy(pExifParams->ExifInfo.UserComment + EXIF_PREFIX_LENGTH,
        settings.userComment,
        sizeof(pExifParams->ExifInfo.UserComment) - EXIF_PREFIX_LENGTH);

    // set make (no prefix)
    NvOsStrncpy(pExifParams->ExifInfo.Make, settings.exifMake,
        sizeof(pExifParams->ExifInfo.Make));

    // set model (no prefix)
    NvOsStrncpy(pExifParams->ExifInfo.Model, settings.exifModel,
        sizeof(pExifParams->ExifInfo.Model));

    // set all three timestamp fields with current system time
    time_t currTime;
    time(&currTime);

    struct tm *datetime = localtime(&currTime);

    if (datetime == NULL)
    {
        ALOGE("programExifInfo: error getting current time from system");
    }
    else
    {
        NvOsSnprintf(pExifParams->ExifInfo.DateTime,
            sizeof(pExifParams->ExifInfo.DateTime),
            "%04d:%02d:%02d %02d:%02d:%02d",
            1900 + datetime->tm_year,
            1 + datetime->tm_mon,
            datetime->tm_mday,
            datetime->tm_hour,
            datetime->tm_min,
            datetime->tm_sec);
        NvOsStrncpy(pExifParams->ExifInfo.DateTimeOriginal,
                    pExifParams->ExifInfo.DateTime,
                    sizeof(pExifParams->ExifInfo.DateTimeOriginal));
        NvOsStrncpy(pExifParams->ExifInfo.DateTimeDigitized,
                    pExifParams->ExifInfo.DateTime,
                    sizeof(pExifParams->ExifInfo.DateTimeDigitized));
    }

    // TODO set image orientation in EXIF

    // 3 for DSC, spec'd in the EXIF standard.  3 is the only valid number
    // as of EXIF 2.2, so we'll just hardcode it for now
    pExifParams->ExifInfo.filesource = 3;

    NV_CHECK_ERROR(
        NvImageEnc_SetParam(mImageEncoder, &Params)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;
}

NvError NvCameraHal::GetPreviewSensorMode(NvMMCameraSensorMode &mode)
{
    NvError e = NvSuccess;

    UNLOCKED_EVENTS_CALL_CLEANUP(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvMMCameraAttribute_PreviewSensorMode,
            sizeof(mode),
            &mode)
    );

fail:
    return e;
}

NvError NvCameraHal::GetCaptureSensorMode(NvMMCameraSensorMode &mode)
{
    NvError e = NvSuccess;

    UNLOCKED_EVENTS_CALL_CLEANUP(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvMMCameraAttribute_CaptureSensorMode,
            sizeof(mode),
            &mode)
    );

fail:
    return e;
}

// returns the highest framerate sensor mode capable of a capture at this resolution
NvError NvCameraHal::GetNSLCaptureSensorMode(
    NvMMCameraSensorMode &mode,
    const NvCombinedCameraSettings &newSettings)
{
    NvBool NoneMatched = NV_TRUE;

    for (NvU32 i = 0; i < mDefaultCapabilities.supportedSensorMode.size(); i++)
    {
        NvCameraUserSensorMode CurSensorMode = mDefaultCapabilities.supportedSensorMode[i];

        // the sensor mode can actually handle the resolution
        if (CurSensorMode.width >= newSettings.imageResolution.width &&
            CurSensorMode.height >= newSettings.imageResolution.height)
        {
            if (NoneMatched || (CurSensorMode.fps > mode.FrameRate))
            {
                mode.Resolution.width = CurSensorMode.width;
                mode.Resolution.height = CurSensorMode.height;
                mode.FrameRate = CurSensorMode.fps;
                NoneMatched = NV_FALSE;
            }
            // if the framerate is the same pick the largere one
            else if (!NoneMatched && (CurSensorMode.fps == mode.FrameRate) &&
                ((CurSensorMode.width * CurSensorMode.height) >
                    (mode.Resolution.width * mode.Resolution.height)))
            {
                mode.Resolution.width = CurSensorMode.width;
                mode.Resolution.height = CurSensorMode.height;
                mode.FrameRate = CurSensorMode.fps;
            }
        }
    }

    return NoneMatched ? NvError_NotSupported : NvSuccess;
}

NvError NvCameraHal::SetConcurrentCaptureRequestNumber(
    NvU32 num)
{
    NvError e = NvSuccess;

    ALOGVV("%s++ num %d", __FUNCTION__, num);

    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvMMCameraAttribute_ConcurrentCaptureRequests,
            0,
            sizeof(num),
            &num)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

// Selects maximum of the requested wait times
void NvCameraHal::SetFinalWaitCountInFrames(NvU32 RequestedWaitCount)
{
    if (mFinalWaitCountInFrames < RequestedWaitCount)
    {
        mFinalWaitCountInFrames = RequestedWaitCount;
    }
}

NvError NvCameraHal::GetPassThroughSupport(NvBool &PassThroughSupport)
{
    NvError e = NvSuccess;
    UNLOCK_EVENTS();
    e = Cam.Block->GetAttribute(
            Cam.Block,
            NvMMCameraAttribute_IsPassThroughSupported,
            sizeof(PassThroughSupport),
            &PassThroughSupport);
    RELOCK_EVENTS();
    return e;
}

} // namespace android

