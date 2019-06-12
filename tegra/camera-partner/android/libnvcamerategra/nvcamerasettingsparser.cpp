/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define LOG_TAG "NvCameraSettingsParser"

#define LOG_NDEBUG 1

#include "nvcamerasettingsparser.h"
#include <hardware/camera.h>
#include <nvassert.h>

namespace android {

#define PARSE_TABLE_VALID(e) (e.key || e.capsKey)

// currently either exposure/focus windows, or color correction matrix,
// are the longest param strings we expect to receive.
// this will need to be increased accordingly if anything longer is used
#define TEMP_BUFFER_SIZE 256


ConversionTable_T previewFormats[] =
{ { "yuv420sp", NvCameraPreviewFormat_Yuv420sp },
  { "yuv420p", NvCameraPreviewFormat_YV12 },  // yv12 is defined as yuv420p in android
  { NULL, 0 }
};

ConversionTable_T nvDataTapFormats[] =
{ { "yuv420sp", NvCameraUserDataTapFormat_Yuv420sp },
  { "yuv420p", NvCameraUserDataTapFormat_YV12 },  // yv12 is defined as yuv420p in android
  { "yuv422p", NvCameraUserDataTapFormat_Yuv422p },
  { "yuv422sp", NvCameraUserDataTapFormat_Yuv422sp },
  { "argb8888", NvCameraUserDataTapFormat_Argb8888 },
  { "rgb565", NvCameraUserDataTapFormat_Rgb565 },
  { "luma8", NvCameraUserDataTapFormat_Luma8 },
  { NULL, 0 }
};

ConversionTable_T videoFrameFormats[] =
{ { "yuv420p", NvCameraVideoFrameFormat_Yuv420p },
  { NULL, 0 }
};

ConversionTable_T flashModes[] =
{ { "auto", NvCameraFlashMode_Auto },
  { "on", NvCameraFlashMode_On },
  { "off", NvCameraFlashMode_Off },
  { "torch", NvCameraFlashMode_Torch },
  { "red-eye", NvCameraFlashMode_RedEyeReduction },
  { NULL, 0 }
};

ConversionTable_T rotationValues[] =
{ { "0", 0 },
  { "90", 90 },
  { "180", 180 },
  { "270", 270 },
  { NULL, 0 }
};

ConversionTable_T sceneModes[] =
{ { "auto", NvCameraSceneMode_Auto },
  { "action", NvCameraSceneMode_Action },
  { "portrait", NvCameraSceneMode_Portrait },
  { "landscape", NvCameraSceneMode_Landscape },
  { "beach", NvCameraSceneMode_Beach },
  { "candlelight", NvCameraSceneMode_Candlelight },
  { "fireworks", NvCameraSceneMode_Fireworks },
  { "night", NvCameraSceneMode_Night },
  { "night-portrait", NvCameraSceneMode_NightPortrait },
  { "party", NvCameraSceneMode_Party },
  { "snow", NvCameraSceneMode_Snow },
  { "sports", NvCameraSceneMode_Sports },
  { "steadyphoto", NvCameraSceneMode_SteadyPhoto },
  { "sunset", NvCameraSceneMode_Sunset },
  { "theatre", NvCameraSceneMode_Theatre },
  { "barcode", NvCameraSceneMode_Barcode },
  { "backlight", NvCameraSceneMode_Backlight },
  { "hdr", NvCameraSceneMode_HDR },
  { NULL, 0 }
};

ConversionTable_T colorEffects[] =
{ { "aqua" , NvCameraColorEffect_Aqua },
  { "blackboard", NvCameraColorEffect_Blackboard },
  { "mono", NvCameraColorEffect_Mono },
  { "negative", NvCameraColorEffect_Negative },
  { "none", NvCameraColorEffect_None },
  { "posterize", NvCameraColorEffect_Posterize },
  { "sepia", NvCameraColorEffect_Sepia },
  { "solarize", NvCameraColorEffect_Solarize },
  { "whiteboard", NvCameraColorEffect_Whiteboard },
  { "nv-vivid", NvCameraColorEffect_Vivid },
  { "nv-emboss", NvCameraColorEffect_Emboss },
  { NULL, 0 }
};

ConversionTable_T whitebalanceModes[] =
{ { "auto", NvCameraWhitebalance_Auto },
  { "incandescent", NvCameraWhitebalance_Incandescent },
  { "fluorescent", NvCameraWhitebalance_Fluorescent },
  { "warm-fluorescent", NvCameraWhitebalance_WarmFluorescent },
  { "daylight", NvCameraWhitebalance_Daylight },
  { "cloudy-daylight", NvCameraWhitebalance_CloudyDaylight },
  { "shade", NvCameraWhitebalance_Shade },
  { "twilight", NvCameraWhitebalance_Twilight },
  // Whitebalance modes not defined in the Android API.
  // They must go last in this table so that settingsStrToVal() will match the
  // ones above.
  { "auto", NvCameraWhitebalance_Flash },
  { "incandescent", NvCameraWhitebalance_Tungsten },
  { "auto", NvCameraWhitebalance_Horizon },
  { NULL, 0 }
};

ConversionTable_T fixedFocusModes[] =
{ { "fixed", NvCameraFocusMode_Fixed },
  { NULL, 0 }
};

ConversionTable_T captureModes[] =
{ { "normal", NvCameraUserCaptureMode_Normal },
  { "shot2shot", NvCameraUserCaptureMode_Shot2Shot },
  { NULL, 0 }
};

ConversionTable_T focusModes[] =
{ { "infinity", NvCameraFocusMode_Infinity },
  { "auto", NvCameraFocusMode_Auto },
  { "macro", NvCameraFocusMode_Macro },
  { "fixed", NvCameraFocusMode_Fixed },
  { "continuous-video", NvCameraFocusMode_Continuous_Video },
  { "continuous-picture", NvCameraFocusMode_Continuous_Picture },
  // The Android API doesn't define a hyperfocal mode, so we just use "fixed"
  // as the key here. Note that this means that "fixed" will be doubly-defined
  // in this table. However, settingStrToVal() which searches this table,
  // traverses it from the beginning and stops after the first match. Therefore,
  // "fixed" won't accidentally be matched to NvCameraFocusMode_Hyperfocal as
  // long as it comes after the other entry for "fixed" in this table.
  { "fixed", NvCameraFocusMode_Hyperfocal },
  { NULL, 0 }
};

ConversionTable_T lightFreqModes[] =
{ { "50hz", NvCameraAntiBanding_50Hz },
  { "60hz", NvCameraAntiBanding_60Hz },
  { "auto", NvCameraAntiBanding_Auto },
  { "off", NvCameraAntiBanding_Off },
  { NULL, 0 }
};

ConversionTable_T contrastModes[] =
{
#if !SUPPORT_CONTRAST_LEVELS
  { "normal", 0 },
#else
  { "lowest",  Contrast_Lowest },
  { "low",     Contrast_Low    },
  { "normal",  Contrast_Normal },
  { "high",    Contrast_High   },
  { "highest", Contrast_Highest},
#endif
  { NULL, 0 }
};

ConversionTable_T nvStereoMode[] =
{ { "left", NvCameraStereoMode_LeftOnly },
  { "right", NvCameraStereoMode_RightOnly },
  { "stereo", NvCameraStereoMode_Stereo },
  { NULL, 0 }
};

//=========================================================
// Creation & Destruction
//=========================================================

//=========================================================
// Constructor
//

NvCameraSettingsParser::NvCameraSettingsParser()
{
    ALOGV("%s++", __FUNCTION__);

    CameraParameters initialParams;

    mFocuserSupported = NV_FALSE;
    mFlashSupported = NV_FALSE;
    mThumbnailWidthSet = NV_FALSE;
    mThumbnailHeightSet = NV_FALSE;

    // Initialize all settings to 0
    NvOsMemset(
        &mCurrentSettings,
        0,
        sizeof(NvCombinedCameraSettings));
    NvOsMemset(
        &mPrevSettings,
        0,
        sizeof(NvCombinedCameraSettings));

    initializeParams(initialParams);
    // start the cycle.  mCurrentParameters should be NULL here since
    // the object was just constructed
    parseParameters(initialParams, NV_TRUE);
    mPrevSettings = mCurrentSettings;

    ALOGV("%s--", __FUNCTION__);
}

//=========================================================
// Destructor
//

NvCameraSettingsParser::~NvCameraSettingsParser()
{
}

//=========================================================
// Public Interface
//=========================================================

//=========================================================
// parseParameters
//

NvError NvCameraSettingsParser::parseParameters(
    const CameraParameters &params,
    NvBool allowNonStandard)
{
    NvError                     e = NvSuccess;
    NvChanges                   changes[ECSType_Max];
    int                         numChanges;
    NvCombinedCameraSettings    newSettings;
    CameraParameters   newParams;

    NvOsMemset( changes, 0, sizeof(changes));

    if ( !extractChanges( params, changes, allowNonStandard,
                          &numChanges, &newParams ) )
    {
        ALOGE("extractChanges: Invalid parameter!");
        e = NvError_BadParameter;
    }
    else
    {
        mCurrentParameters = newParams;

        buildNewSettings( changes, numChanges, &newSettings );

        mPrevSettings = mCurrentSettings;
        mCurrentSettings = newSettings;
    }

    return e;
}

//=========================================================
// getParameters
//

CameraParameters NvCameraSettingsParser::getParameters( NvBool allowNonStandard ) const
{
    if (allowNonStandard)
    {
        return mCurrentParameters;
    }
    else
    {
        CameraParameters standardParameters = mCurrentParameters;
        int i = 0;

        // filter out all nonstandard params
        while(PARSE_TABLE_VALID(ParserInfoTable[i]))
        {
            if (ParserInfoTable[i].access & ECSAccess_NonStandard)
            {
                if (ParserInfoTable[i].key)
                {
                    standardParameters.remove(ParserInfoTable[i].key);
                }
                if (ParserInfoTable[i].capsKey)
                {
                    standardParameters.remove(ParserInfoTable[i].capsKey);
                }
            }
            i++;
        }

        return standardParameters;
    }
}

//=========================================================
// getCurrentSettings
//

const NvCombinedCameraSettings& NvCameraSettingsParser::getCurrentSettings( void ) const
{
    return ( mCurrentSettings );
}

//=========================================================
// getPreviousSettings
//

const NvCombinedCameraSettings& NvCameraSettingsParser::getPrevSettings( void ) const
{
    return ( mPrevSettings );
}

//=========================================================
// updateSettings
//

void NvCameraSettingsParser::updateSettings(NvCombinedCameraSettings& newSettings)
{
    // checks all settings in newSettings vs. mCurrentSettings to find and apply
    // changes.  this is used during init to apply the default settings that the
    // settings handler pulls from lower levels, as well as for some places where settings
    // get changed outside of the normal setParameter interface (i.e. during scene
    // mode changes)

    if (newSettings.isDirtyForParser.useNvBuffers)
    {
        if (newSettings.useNvBuffers)
        {
            mCurrentParameters.set
                (findSettingsKey(ECSType_UseNvBuffers), "1");
        }
        else
        {
            mCurrentParameters.set
                (findSettingsKey(ECSType_UseNvBuffers), "0");
        }
    }

    if (newSettings.isDirtyForParser.exposureStatus)
    {
        char exposureStatusString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(exposureStatusString, TEMP_BUFFER_SIZE, "%f,%f,%f,%f,%f,%f,%d",
            newSettings.exposureStatus.ExpComp, newSettings.exposureStatus.ExposureTime,
            newSettings.exposureStatus.BinningGain, newSettings.exposureStatus.AnalogGain,
            newSettings.exposureStatus.SensorDigitalGain,
            newSettings.exposureStatus.IspDigitalGain, newSettings.isoStatus.value);
        mCurrentParameters.set(
            findSettingsKey(ECSType_ExposureStatus), exposureStatusString);
    }

    if (newSettings.isDirtyForParser.whiteBalance)
    {
        // Ported from change 130498. Invalid WB mode shouldn't appear
        // here.
        if (newSettings.whiteBalance == NvCameraWhitebalance_Invalid)
        {
            ALOGE("Invalid Whitebalance mode detected!");
            NV_ASSERT(0);
        }
        mCurrentParameters.set(
            findSettingsKey(ECSType_WhiteBalanceMode),
            settingValToStr(ECSType_WhiteBalanceMode, newSettings.whiteBalance));
    }

    if (newSettings.isDirtyForParser.contrast)
    {
        mCurrentParameters.set(
            findSettingsKey(ECSType_Contrast),
            settingValToStr(ECSType_Contrast, newSettings.contrast));
    }

    if (newSettings.isDirtyForParser.saturation)
    {
        char saturationString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(saturationString, TEMP_BUFFER_SIZE, "%d", newSettings.saturation);
        mCurrentParameters.set(findSettingsKey(ECSType_Saturation), saturationString);
    }

    if (newSettings.isDirtyForParser.edgeEnhancement)
    {
        char edgeEnhancementString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(edgeEnhancementString, TEMP_BUFFER_SIZE, "%d", newSettings.edgeEnhancement);
        mCurrentParameters.set(findSettingsKey(ECSType_EdgeEnhancement), edgeEnhancementString);
    }

    if (newSettings.isDirtyForParser.flashMode)
    {
        if(mFlashSupported)
        {
            mCurrentParameters.set(
                findSettingsKey(ECSType_FlashMode),
                settingValToStr(ECSType_FlashMode, newSettings.flashMode));
        }
        // remove flash param if not supported
        else
        {
            mCurrentParameters.remove(findCapsKey(ECSType_FlashMode));
            mCurrentParameters.remove(findSettingsKey(ECSType_FlashMode));
        }
    }

    if (newSettings.isDirtyForParser.focusMode)
    {
        mCurrentParameters.set(
            findSettingsKey(ECSType_FocusMode),
            settingValToStr(ECSType_FocusMode, newSettings.focusMode));
    }

    if (newSettings.isDirtyForParser.focusPosition)
    {
        char focusPositionString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(focusPositionString, TEMP_BUFFER_SIZE, "%d", newSettings.focusPosition);
        mCurrentParameters.set(findSettingsKey(ECSType_FocusPosition), focusPositionString);
    }

    if (newSettings.isDirtyForParser.focalLength)
    {
        char focalLengthString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(focalLengthString, TEMP_BUFFER_SIZE, "%.3f", newSettings.focalLength);
        mCurrentParameters.set(findSettingsKey(ECSType_FocalLength), focalLengthString);
    }

    if (newSettings.isDirtyForParser.horizontalViewAngle)
    {
        char horizontalViewAngleString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(horizontalViewAngleString, TEMP_BUFFER_SIZE, "%.3f", newSettings.horizontalViewAngle);
        mCurrentParameters.set(findSettingsKey(ECSType_HorizontalViewAngle), horizontalViewAngleString);
    }

    if (newSettings.isDirtyForParser.verticalViewAngle)
    {
        char verticalViewAngleString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(verticalViewAngleString, TEMP_BUFFER_SIZE, "%.3f", newSettings.verticalViewAngle);
        mCurrentParameters.set(findSettingsKey(ECSType_VerticalViewAngle), verticalViewAngleString);
    }

    if (newSettings.isDirtyForParser.zoomLevel)
    {
        char zoomString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(zoomString, TEMP_BUFFER_SIZE, "%d", newSettings.zoomLevel);
        mCurrentParameters.set(findSettingsKey(ECSType_ZoomValue), zoomString);
    }

    if (newSettings.isDirtyForParser.antiBanding)
    {
        mCurrentParameters.set(
            findSettingsKey(ECSType_LightFreqMode),
            settingValToStr(ECSType_LightFreqMode, mCurrentSettings.antiBanding));
    }

    if (newSettings.isDirtyForParser.colorEffect)
    {
        mCurrentParameters.set(
            findSettingsKey(ECSType_ColorEffect),
            settingValToStr(ECSType_ColorEffect, newSettings.colorEffect));
    }

    if (newSettings.isDirtyForParser.PreviewFormat)
    {
        mCurrentParameters.set(
            findSettingsKey(ECSType_PreviewFormat),
            settingValToStr(ECSType_PreviewFormat, newSettings.PreviewFormat));
    }

    if (newSettings.isDirtyForParser.CaptureMode)
    {
        mCurrentParameters.set(
            findSettingsKey(ECSType_CaptureMode),
            settingValToStr(ECSType_CaptureMode, newSettings.CaptureMode));
    }

    if (newSettings.isDirtyForParser.videoStabOn)
    {
        if(newSettings.videoStabOn)
        {
            mCurrentParameters.set
                (findSettingsKey(ECSType_VideoStabEn), "true");
        }
        else
        {
            mCurrentParameters.set
                (findSettingsKey(ECSType_VideoStabEn), "false");
        }
    }

    if (newSettings.isDirtyForParser.stillCapHdrOn)
    {
        if (newSettings.stillCapHdrOn)
        {
            mCurrentParameters.set
                (findSettingsKey(ECSType_StillHDR), "true");
        }
        else
        {
            mCurrentParameters.set
                (findSettingsKey(ECSType_StillHDR), "false");
        }
    }

    if (newSettings.isDirtyForParser.stillHdrSourceFrameCount)
    {
        char hdrFrameCount[TEMP_BUFFER_SIZE];
        NvOsSnprintf(
            hdrFrameCount,
            TEMP_BUFFER_SIZE,
            "%u",
            newSettings.stillHdrSourceFrameCount);
        mCurrentParameters.set(
            findSettingsKey(ECSType_StillHDRSourceFrameCount),
            hdrFrameCount);
    }

    if (newSettings.isDirtyForParser.stillHdrFrameSequence)
    {
        char hdrFrameSeqStart[TEMP_BUFFER_SIZE];
        char *hdrFrameSeqPos = hdrFrameSeqStart;
        unsigned int i;
        for (i = 0; i < newSettings.stillHdrSourceFrameCount - 1; i++)
        {
            int numChars = 0;
            numChars = NvOsSnprintf(
                hdrFrameSeqPos,
                TEMP_BUFFER_SIZE,
                "%f,",
                newSettings.stillHdrFrameSequence[i]);
            hdrFrameSeqPos += numChars;
        }
        NvOsSnprintf(
            hdrFrameSeqPos,
            TEMP_BUFFER_SIZE,
            "%f",
            newSettings.stillHdrFrameSequence[i]);
        mCurrentParameters.set(
            findSettingsKey(ECSType_StillHDRExposureVals),
            hdrFrameSeqStart);
    }

    if (newSettings.isDirtyForParser.customPostviewOn)
    {
        if(newSettings.customPostviewOn)
        {
            mCurrentParameters.set
                (findSettingsKey(ECSType_CustomPostviewEn), "on");
        }
        else
        {
            mCurrentParameters.set
                (findSettingsKey(ECSType_CustomPostviewEn), "off");
        }
    }

    if (newSettings.isDirtyForParser.videoSpeed)
    {
        char speedString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(speedString, TEMP_BUFFER_SIZE, "%f", newSettings.videoSpeed);
        mCurrentParameters.set
                (findSettingsKey(ECSType_VideoSpeed), speedString);
    }

    if (newSettings.isDirtyForParser.previewFpsRange)
    {
        char fpsRangeString[TEMP_BUFFER_SIZE] = {0};

        NvOsSnprintf(fpsRangeString, TEMP_BUFFER_SIZE, "%d,%d", newSettings.previewFpsRange.min,\
            newSettings.previewFpsRange.max);
        mCurrentParameters.set
                (findSettingsKey(ECSType_PreviewFpsRange), fpsRangeString);
    }

    if (newSettings.isDirtyForParser.PreviewFlip)
    {
        switch (newSettings.PreviewFlip)
        {
            case NvCameraUserFlip_Horizontal:
                mCurrentParameters.set(
                    findSettingsKey(ECSType_FlipPreview), "horizontal");
                break;
            case NvCameraUserFlip_Vertical:
                mCurrentParameters.set(
                    findSettingsKey(ECSType_FlipPreview), "vertical");
                break;
            case NvCameraUserFlip_Both:
                mCurrentParameters.set(
                    findSettingsKey(ECSType_FlipPreview), "both");
                break;
            default:
                mCurrentParameters.set(
                    findSettingsKey(ECSType_FlipPreview), "off");
                break;
        }
    }

    if (newSettings.isDirtyForParser.StillFlip)
    {
        switch (newSettings.StillFlip)
        {
            case NvCameraUserFlip_Horizontal:
                mCurrentParameters.set(
                    findSettingsKey(ECSType_FlipStill), "horizontal");
                break;
            case NvCameraUserFlip_Vertical:
                mCurrentParameters.set(
                    findSettingsKey(ECSType_FlipStill), "vertical");
                break;
            case NvCameraUserFlip_Both:
                mCurrentParameters.set(
                    findSettingsKey(ECSType_FlipStill), "both");
                break;
            default:
                mCurrentParameters.set(
                    findSettingsKey(ECSType_FlipStill), "off");
                break;
        }
    }

    if (newSettings.isDirtyForParser.iso)
    {
        mCurrentParameters.set(
            findSettingsKey(ECSType_PictureIso),
            settingValToStr(ECSType_PictureIso, newSettings.iso));
    }

    if (newSettings.isDirtyForParser.exposureTimeMicroSec)
    {
        char expTimeString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(expTimeString, TEMP_BUFFER_SIZE, "%d", newSettings.exposureTimeMicroSec);
        mCurrentParameters.set(
            findSettingsKey(ECSType_ExposureTime), expTimeString);
    }

    if (newSettings.isDirtyForParser.exposureCompensationIndex)
    {
        char expCompString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(expCompString, TEMP_BUFFER_SIZE, "%d", newSettings.exposureCompensationIndex);
        mCurrentParameters.set(
            findSettingsKey(ECSType_ExposureCompensation), expCompString);
    }

    if (newSettings.isDirtyForParser.nslNumBuffers)
    {
        char numBuffersString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(numBuffersString, TEMP_BUFFER_SIZE, "%d", newSettings.nslNumBuffers);
        mCurrentParameters.set(
            findSettingsKey(ECSType_NSLNumBuffers), numBuffersString);
    }

    if (newSettings.isDirtyForParser.nslSkipCount)
    {
        char nslSkipCountString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(nslSkipCountString, TEMP_BUFFER_SIZE, "%d", newSettings.nslSkipCount);
        mCurrentParameters.set(
            findSettingsKey(ECSType_NSLSkipCount), nslSkipCountString);
    }

    if (newSettings.isDirtyForParser.nslBurstCount)
    {
        char nslBurstCountString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(nslBurstCountString, TEMP_BUFFER_SIZE, "%d", newSettings.nslBurstCount);
        mCurrentParameters.set(
            findSettingsKey(ECSType_NSLBurstCount), nslBurstCountString);
    }

    if (newSettings.isDirtyForParser.skipCount)
    {
        char skipCountString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(skipCountString, TEMP_BUFFER_SIZE, "%d", newSettings.skipCount);
        mCurrentParameters.set(
            findSettingsKey(ECSType_SkipCount),
            skipCountString);
    }

    if (newSettings.isDirtyForParser.burstCaptureCount)
    {
        char burstCapString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(burstCapString, TEMP_BUFFER_SIZE, "%d", newSettings.burstCaptureCount);
        mCurrentParameters.set(
            findSettingsKey(ECSType_BurstCount),
            burstCapString);
    }

    if (newSettings.isDirtyForParser.bracketCaptureCount ||
        newSettings.isDirtyForParser.bracketFStopAdj)
    {
        char bracketCaptureCapString[TEMP_BUFFER_SIZE];
        char *strIndx;
        int n = 0;

        strIndx = bracketCaptureCapString;

        for (int i = 0; i < newSettings.bracketCaptureCount; )
        {
            n = NvOsSnprintf(strIndx, TEMP_BUFFER_SIZE, "%f", newSettings.bracketFStopAdj[i]);
            strIndx += n;
            i++;
            if(i < newSettings.bracketCaptureCount)
            {
                n = NvOsSnprintf(strIndx, TEMP_BUFFER_SIZE, ",");
                strIndx += n;
            }
        }

        mCurrentParameters.set(
            findSettingsKey(ECSType_BracketCapture),
            bracketCaptureCapString);
    }

    if (newSettings.isDirtyForParser.rawDumpFlag)
    {
        char rawDumpString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(rawDumpString, TEMP_BUFFER_SIZE, "%d", newSettings.rawDumpFlag);
        mCurrentParameters.set(
            findSettingsKey(ECSType_RawDumpFlag),
            rawDumpString);
    }

    if (newSettings.isDirtyForParser.imageResolution)
    {
        char resString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(resString, TEMP_BUFFER_SIZE, "%dx%d",
            newSettings.imageResolution.width, newSettings.imageResolution.height);
        mCurrentParameters.set(findSettingsKey(ECSType_PictureSize), resString);
    }

    if (newSettings.isDirtyForParser.videoResolution)
    {
        char resString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(resString, TEMP_BUFFER_SIZE, "%dx%d",
            newSettings.videoResolution.width, newSettings.videoResolution.height);
        mCurrentParameters.set(findSettingsKey(ECSType_VideoSize), resString);
    }

    if (newSettings.isDirtyForParser.previewResolution)
    {
        char resString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(resString, TEMP_BUFFER_SIZE, "%dx%d",
            newSettings.previewResolution.width, newSettings.previewResolution.height);
        mCurrentParameters.set(findSettingsKey(ECSType_PreviewSize), resString);
    }

    if (newSettings.isDirtyForParser.PreferredPreviewResolution)
    {
        char resString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(resString, TEMP_BUFFER_SIZE, "%dx%d",
            newSettings.PreferredPreviewResolution.width, newSettings.PreferredPreviewResolution.height);
        mCurrentParameters.set(findSettingsKey(ECSType_PreferredPreviewSize), resString);
    }

    if (newSettings.isDirtyForParser.imageRotation)
    {
        char rotString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(rotString, TEMP_BUFFER_SIZE, "%d", newSettings.imageRotation);
        mCurrentParameters.set(findSettingsKey(ECSType_PictureRotation), rotString);
    }

    if (newSettings.isDirtyForParser.thumbnailResolution)
    {
        char resString[TEMP_BUFFER_SIZE];

        NvOsSnprintf(resString, TEMP_BUFFER_SIZE, "%d", newSettings.thumbnailResolution.width);
        mCurrentParameters.set(findSettingsKey(ECSType_ThumbnailWidth), resString);

        NvOsSnprintf(resString, TEMP_BUFFER_SIZE, "%d", newSettings.thumbnailResolution.height);
        mCurrentParameters.set(findSettingsKey(ECSType_ThumbnailHeight), resString);
    }

    if (newSettings.isDirtyForParser.sensorMode)
    {
        char resString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(resString, TEMP_BUFFER_SIZE, "%dx%dx%d",
            newSettings.sensorMode.width, newSettings.sensorMode.height, newSettings.sensorMode.fps);
        mCurrentParameters.set(findSettingsKey(ECSType_SensorMode), resString);
    }

    if (newSettings.isDirtyForParser.FocusDistances)
    {
        char FocusDistancesString[TEMP_BUFFER_SIZE];
        float Distances[3];
        int i = 0;

        NvOsMemset(FocusDistancesString, 0, TEMP_BUFFER_SIZE * sizeof(char));

        Distances[0] = newSettings.FocusDistances.Near;
        Distances[1] = newSettings.FocusDistances.Optimal;
        Distances[2] = newSettings.FocusDistances.Far;

        for (i = 0; i < 3; i++)
        {
            if (Distances[i] < 0.0f)
            {
                NvOsSnprintf(FocusDistancesString, TEMP_BUFFER_SIZE,
                                "%sInfinity", FocusDistancesString);
            }
            else
            {
                NvOsSnprintf(FocusDistancesString, TEMP_BUFFER_SIZE,
                                "%s%4.2f", FocusDistancesString,
                                newSettings.imageRotation);
            }
            if (i < 2)
            {
                NvOsSnprintf(FocusDistancesString, TEMP_BUFFER_SIZE, "%s,",
                                FocusDistancesString);
            }
        }
        mCurrentParameters.set(findSettingsKey(ECSType_FocusDistances),
                                FocusDistancesString);
    }

    if (newSettings.isDirtyForParser.FocusWindows)
    {
        char resString[TEMP_BUFFER_SIZE];
        int written = 0;
        for (int i = 0; i < MAX_FOCUS_WINDOWS; i++)
        {
            // do this regardless of whether the window changed, since it's
            // cleaner logic than it would be to selectively update the changed
            // parts if anything changed
            written += NvOsSnprintf(
                resString + written,
                TEMP_BUFFER_SIZE - written,
                "(%d,%d,%d,%d,%d),",
                newSettings.FocusWindows[i].left,
                newSettings.FocusWindows[i].top,
                newSettings.FocusWindows[i].right,
                newSettings.FocusWindows[i].bottom,
                newSettings.FocusWindows[i].weight);

            // If the first window is {0,0,0,0,0} we will use the default in
            // NvMM, so just write one window to the string.
            if (i == 0 && newSettings.FocusWindows[0].left == 0 &&
                newSettings.FocusWindows[0].top == 0 &&
                newSettings.FocusWindows[0].right == 0 &&
                newSettings.FocusWindows[0].bottom == 0 &&
                newSettings.FocusWindows[0].weight == 0)
            {
                break;
            }

        }
        if (written > 0)
        {
            // get rid of extra ',' on end of string and update param set
            resString[written - 1] = '\0';
            mCurrentParameters.set(findSettingsKey(ECSType_AreasToFocus), resString);
        }
    }

    if (newSettings.isDirtyForParser.ExposureWindows)
    {
        char resString[TEMP_BUFFER_SIZE];
        int i;
        int written = 0;
        for (i = 0; i < MAX_EXPOSURE_WINDOWS; i++)
        {
            // do this regardless of whether the window changed, since it's
            // cleaner logic than it would be to selectively update the changed
            // parts if anything changed
            written += NvOsSnprintf(
                resString + written,
                TEMP_BUFFER_SIZE - written,
                "(%d,%d,%d,%d,%d),",
                newSettings.ExposureWindows[i].left,
                newSettings.ExposureWindows[i].top,
                newSettings.ExposureWindows[i].right,
                newSettings.ExposureWindows[i].bottom,
                newSettings.ExposureWindows[i].weight);

            // If the first window is {0,0,0,0,0} we will use the default in
            // NvMM, so just write one window to the string.
            if (i == 0 && newSettings.ExposureWindows[0].left == 0 &&
                newSettings.ExposureWindows[0].top == 0 &&
                newSettings.ExposureWindows[0].right == 0 &&
                newSettings.ExposureWindows[0].bottom == 0 &&
                newSettings.ExposureWindows[0].weight == 0)
            {
                break;
            }
        }
        if (written > 0)
        {
            // get rid of extra ',' on end of string and update param set
            resString[written - 1] = '\0';
            mCurrentParameters.set(findSettingsKey(ECSType_AreasToMeter), resString);
        }
    }

    {
        int i;
        int written = 0;
        char matrixString[TEMP_BUFFER_SIZE];

        if (newSettings.isDirtyForParser.colorCorrectionMatrix)
        {
            for (i = 0; i < 16; i++)
            {
                written += NvOsSnprintf(matrixString + written,
                            TEMP_BUFFER_SIZE - written,
                            "%f,", newSettings.colorCorrectionMatrix[i]);
            }

            // double check to prevent NvOsSnprintf failing
            if (written > 0)
            {
                // get rid of extra ',' on end of string and update param set
                matrixString[written - 1] = '\0';
                mCurrentParameters.set(findSettingsKey(ECSType_ColorCorrection), matrixString);
            }
        }
    }

    if (newSettings.isDirtyForParser.useCameraRenderer)
    {
        mCurrentParameters.set(findSettingsKey(ECSType_CameraRenderer),
                           newSettings.useCameraRenderer? "on":"off");
    }

    if (newSettings.isDirtyForParser.AELocked)
    {
        if (newSettings.AELocked)
        {
            mCurrentParameters.set
                (findSettingsKey(ECSType_AELock), "true");
        }
        else
        {
            mCurrentParameters.set
                (findSettingsKey(ECSType_AELock), "false");
        }
    }

    if (newSettings.isDirtyForParser.AWBLocked)
    {
        if (newSettings.AWBLocked)
        {
            mCurrentParameters.set
                (findSettingsKey(ECSType_AWBLock), "true");
        }
        else
        {
            mCurrentParameters.set
                (findSettingsKey(ECSType_AWBLock), "false");
        }
    }

#ifdef FRAMEWORK_HAS_MACRO_MODE_SUPPORT
    if (newSettings.isDirtyForParser.LowLightThreshold)
    {
        char LowLightThresholdString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(LowLightThresholdString, TEMP_BUFFER_SIZE, "%d", newSettings.LowLightThreshold);
        mCurrentParameters.set(
            findSettingsKey(ECSType_LowLightThreshold),
            LowLightThresholdString);
    }

    if (newSettings.isDirtyForParser.MacroModeThreshold)
    {
        char MacroModeThresholdString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(MacroModeThresholdString, TEMP_BUFFER_SIZE, "%d", newSettings.MacroModeThreshold);
        mCurrentParameters.set(findSettingsKey(ECSType_MacroModeThreshold),
            MacroModeThresholdString);
    }
#endif

    if (newSettings.isDirtyForParser.FocusMoveMsg)
    {
        mCurrentSettings.FocusMoveMsg = newSettings.FocusMoveMsg;
        mCurrentParameters.set(
            findSettingsKey(ECSType_FocusMoveMsg),
                mCurrentSettings.FocusMoveMsg ? "true" : "false");
    }

    if (newSettings.isDirtyForParser.fdLimit)
    {
        char sLimitString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(sLimitString, TEMP_BUFFER_SIZE, "%d", newSettings.fdLimit);
        mCurrentParameters.set(findSettingsKey(ECSType_FDLimit),
            sLimitString);
    }

    if (newSettings.isDirtyForParser.fdDebug)
    {
        mCurrentParameters.set(findSettingsKey(ECSType_FDDebug),
                            newSettings.fdLimit ? "on":"off");
    }

    if (newSettings.isDirtyForParser.autoRotation)
    {
        if (newSettings.autoRotation)
        {
            mCurrentParameters.set(findSettingsKey(ECSType_AutoRotation), "true");
        }
        else
        {
            mCurrentParameters.set(findSettingsKey(ECSType_AutoRotation), "false");
        }
    }

    if (newSettings.isDirtyForParser.DataTapResolution)
    {
        char resString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(resString, TEMP_BUFFER_SIZE, "%dx%d",
            newSettings.DataTapResolution.width, newSettings.DataTapResolution.height);
        mCurrentParameters.set(findSettingsKey(ECSType_DataTapSize), resString);
    }

    if (newSettings.isDirtyForParser.DataTapFormat)
    {
        mCurrentParameters.set(
            findSettingsKey(ECSType_DataTapFormat),
            settingValToStr(ECSType_DataTapFormat, newSettings.DataTapFormat));
    }

    if (newSettings.isDirtyForParser.mDataTap)
    {
        mCurrentParameters.set(findSettingsKey(ECSType_DataTap),
                            newSettings.mDataTap ? "on":"off");
    }

    if (newSettings.isDirtyForParser.DisablePreviewPause)
    {
        if(newSettings.DisablePreviewPause)
        {
            mCurrentParameters.set
                (findSettingsKey(ECSType_DisablePreviewPause), "true");
        }
        else
        {
            mCurrentParameters.set
                (findSettingsKey(ECSType_DisablePreviewPause), "false");
        }
    }

    if (newSettings.isDirtyForParser.PreviewCbSizeEnable)
    {
        if(newSettings.PreviewCbSizeEnable)
        {
            mCurrentParameters.set
                (findSettingsKey(ECSType_PreviewCallbackSizeEnable), "true");
        }
        else
        {
            mCurrentParameters.set
                (findSettingsKey(ECSType_PreviewCallbackSizeEnable), "false");
        }
    }

    if (newSettings.isDirtyForParser.PreviewCbSize)
    {
        char resString[TEMP_BUFFER_SIZE];
        NvOsSnprintf(resString, TEMP_BUFFER_SIZE, "%dx%d",
            newSettings.PreviewCbSize.width, newSettings.PreviewCbSize.height);
        mCurrentParameters.set(findSettingsKey(ECSType_PreviewCallbackSize), resString);
    }

    // Clear all of the dirty bits. If anything was dirty it should have been
    // updated above. Note that this means that nothing above is allowed to fail
    // and exit early.
    NvOsMemset(
        &newSettings.isDirtyForParser,
        NV_FALSE,
        sizeof(NvCombinedCameraSettingsDirty));

    // update always
    mCurrentParameters.set(findSettingsKey(ECSType_FDResult), newSettings.fdResult);

    mPrevSettings = mCurrentSettings;
    mCurrentSettings = newSettings;
}

// only works for parameters that use a substring capability
void NvCameraSettingsParser::removeCapabilitySubstring(
    ECSType Type,
    const char *ToBeRemoved)
{
    char NewCapStr[TEMP_BUFFER_SIZE];
    const char *CapStr = mCurrentParameters.get(findCapsKey(Type));
    char *begin = NewCapStr;
    char *end;

    if (!CapStr)
    {
        // return if such capability doesn't exist
        return;
    }

    strncpy(NewCapStr, CapStr, TEMP_BUFFER_SIZE);

    while (begin && *begin != '\0')
    {
        end = strchr(begin, ',');
        if (end)
        {
            if (strncmp(ToBeRemoved, begin, end - begin) == 0)
            {
                strncpy(begin, end + 1, TEMP_BUFFER_SIZE - (begin - NewCapStr));
                break;
            }
        }
        else
        {
            if (strcmp(ToBeRemoved, begin) == 0)
            {
                if (NewCapStr == begin)
                {
                    *begin = '\0';
                }
                else
                {
                    // get rid of the last ','
                    begin[-1] = '\0';
                }
            }
            // The string has ended
            break;
        }
        begin = end + 1;
    }

    mCurrentParameters.set(findCapsKey(Type), NewCapStr);
}

// only works for parameters that use a substring capability
void NvCameraSettingsParser::addCapabilitySubstring(
    ECSType Type,
    const char *ToBeAdded)
{
    char NewCapStr[TEMP_BUFFER_SIZE];
    const char *CapStr = mCurrentParameters.get(findCapsKey(Type));
    char *begin = NewCapStr;
    char *end;

    if (!CapStr)
    {
        // return if such capability doesn't exist
        return;
    }

    strncpy(NewCapStr, CapStr, TEMP_BUFFER_SIZE);

    while ((begin - NewCapStr) < TEMP_BUFFER_SIZE)
    {
        end = strchr(begin, ',');
        if (end)
        {
            if (strncmp(ToBeAdded, begin, end - begin) == 0)
            {
                // Already got this substring. Return
                return;
            }
        }
        else
        {
            // Append ToBeAdded to capability string
            end = strchr(begin, '\0');
            // starting for end, we need one byte for ',' , one byte for '\0'
            // and strlen(ToBeAdded) bytes for ToBeAdded
            if ( (end + 2 + strlen(ToBeAdded) - NewCapStr) >= TEMP_BUFFER_SIZE)
            {
                ALOGE("%s: New string size is too long!", __FUNCTION__);
                return;
            }
            *end = ',';
            strcpy(end + 1, ToBeAdded);
            break;
        }
        begin = end + 1;
    }
    mCurrentParameters.set(findCapsKey(Type), NewCapStr);
}

//=========================================================
// setCapabilities
//  - update the capabilities with the things that the driver supports
void NvCameraSettingsParser::setCapabilities
                                    (const NvCameraCapabilities& capabilities)
{
    char str[512];

    // sensor modes
    encodeSensorModes(capabilities.supportedSensorMode, str, sizeof(str));
    mCurrentParameters.set(findCapsKey(ECSType_SensorMode), str);

    // preview size
    encodeResolutions(capabilities.supportedPreview, str, sizeof(str));
    mCurrentParameters.set(findCapsKey(ECSType_PreviewSize), str);

    // still image size
    encodeResolutions(capabilities.supportedPicture, str, sizeof(str));
    mCurrentParameters.set(findCapsKey(ECSType_PictureSize), str);

    // video size
    encodeResolutions(capabilities.supportedVideo, str, sizeof(str));
    mCurrentParameters.set(findCapsKey(ECSType_VideoSize), str);

    // video preview framerate
    encodeVideoPreviewfps(capabilities.supportedVideoPreviewfps, str, sizeof(str));
    mCurrentParameters.set(findCapsKey(ECSType_VideoPreviewSizefps), str);

    // cap preferred preview size for video
    encodePreferredPreviewSizeForVideo(capabilities.supportedPreview, str, sizeof(str));
    mCurrentParameters.set(findSettingsKey(ECSType_PreferredPreviewSize), str);

    // focus modes
    if(capabilities.supportedFocusModes.size() == 1)
    {
        mFocuserSupported = NV_FALSE;
        mCurrentParameters.set(findCapsKey(ECSType_FocusMode), "fixed");
        // do max num focus areas here too
        // since it's constrained by the same condition
        mCurrentParameters.set(findCapsKey(ECSType_AreasToFocus), "0");
    }
    else
    {
        mFocuserSupported = NV_TRUE;
        encodeFocusModes(capabilities.supportedFocusModes, str, sizeof(str));
        mCurrentParameters.set(findCapsKey(ECSType_FocusMode), str);
    }

    // flash modes
    if(capabilities.supportedFlashModes.size() == 1)
    {
        mFlashSupported = NV_FALSE;

        // remove flash param if not supported
        mCurrentParameters.remove(findCapsKey(ECSType_FlashMode));
        mCurrentParameters.remove(findSettingsKey(ECSType_FlashMode));
    }
    else
    {
        mFlashSupported = NV_TRUE;
        encodeFlashModes(capabilities.supportedFlashModes, str, sizeof(str));
        mCurrentParameters.set(findCapsKey(ECSType_FlashMode), str);
    }

    // if the sensor isn't using our ISP (most likely because it is a YUV sensor),
    // we've got to disable a bunch of stuff that we use our ISP to implement.
    if(!capabilities.hasIsp)
    {
        // these params should return NULL strings through the
        // API if not supported
        mCurrentParameters.remove(findCapsKey(ECSType_WhiteBalanceMode));
        mCurrentParameters.remove(findSettingsKey(ECSType_WhiteBalanceMode));
        // if AWB isn't supported, AWB locking isn't either
        mCurrentParameters.set(findCapsKey(ECSType_AWBLockSupport), "false");

        // these params should return "none" or "off" strings through the
        // API if not supported
        mCurrentParameters.set(findCapsKey(ECSType_ColorEffect), "none");
        mCurrentParameters.set(findCapsKey(ECSType_LightFreqMode), "off");

        // exposure compensation params should return "0" if not supported
        mCurrentParameters.set(findCapsKey(ECSType_MaxExpComp), "0");
        mCurrentParameters.set(findCapsKey(ECSType_MinExpComp), "0");
        mCurrentParameters.set(findCapsKey(ECSType_ExpCompStep), "0");
        mCurrentParameters.set(findCapsKey(ECSType_AELockSupport), "false");

        // these params aren't exposed through the API, so we can do whatever
        // we want for these
        mCurrentParameters.set(findCapsKey(ECSType_Contrast), "normal");
        mCurrentParameters.set(findCapsKey(ECSType_PictureIso), "auto");
    }

    // if NvMM doesn't support bracket capture we need to disable HDR
    if (!capabilities.hasBracketCapture)
    {
        removeCapabilitySubstring(ECSType_SceneMode, "hdr");
        mCurrentParameters.set(findCapsKey(ECSType_StillHDR), "false");
    }

    // scene modes
    if (!capabilities.hasSceneModes)
    {
        mCurrentParameters.remove(findCapsKey(ECSType_SceneMode));
        mCurrentParameters.remove(findSettingsKey(ECSType_SceneMode));
    }

    // stereo modes
    encodeStereoModes(capabilities.supportedStereoModes, str, sizeof(str));
    mCurrentParameters.set(findCapsKey(ECSType_CameraStereoMode), str);

    // face detection
    if (!capabilities.fdMaxNumFaces)
    {
        mCurrentParameters.remove(findCapsKey(ECSType_FDLimit));
    }
    else
    {
        NvOsMemset(str, '\0', sizeof(str));
        NvOsSnprintf(str, FD_VALUE_LEN, "%d", capabilities.fdMaxNumFaces);
        mCurrentParameters.set(findCapsKey(ECSType_FDLimit), str);
    }
}

//=========================================================
// updateCapabilitiesForHighSpeedRecording
//  - update the capabilities which will be affected by high
//    speed recording
//  - currently hard coded to add/remove 60/120 fps
void NvCameraSettingsParser::updateCapabilitiesForHighSpeedRecording(
    const NvBool highSpeedRecEnable)
{
    if (highSpeedRecEnable)
    {
        addCapabilitySubstring(ECSType_PreviewRate, "60");
        addCapabilitySubstring(ECSType_PreviewRate, "120");
    }
    else
    {
        removeCapabilitySubstring(ECSType_PreviewRate, "60");
        removeCapabilitySubstring(ECSType_PreviewRate, "120");
    }
}

//=========================================================
// lockSceneModeCapabilities
//
// - update capabilities string for flash, focus, and
//   whitebalance to lock those settings if scene mode
//   has changed them to something specific (non-auto)
//
void NvCameraSettingsParser::lockSceneModeCapabilities(
    NvCameraUserFlashMode newFlashMode,
    NvCameraUserFocusMode newFocusMode,
    NvCameraUserWhitebalance newWhitebalanceMode)
{
    int i = 0;
    while (PARSE_TABLE_VALID(ParserInfoTable[i]))
    {
        if ((ParserInfoTable[i].type == ECSType_FlashMode) &&
            mFlashSupported)
        {
            mCurrentParameters.set(
                ParserInfoTable[i].capsKey,
                settingValToStr(ECSType_FlashMode, newFlashMode));
        }
        else if ((ParserInfoTable[i].type == ECSType_FocusMode) &&
            mFocuserSupported)
        {
            mCurrentParameters.set(
                ParserInfoTable[i].capsKey,
                settingValToStr(ECSType_FocusMode, newFocusMode));
        }
        else if (ParserInfoTable[i].type == ECSType_WhiteBalanceMode)
        {
            mCurrentParameters.set(
                ParserInfoTable[i].capsKey,
                settingValToStr(ECSType_WhiteBalanceMode, newWhitebalanceMode));
        }

        i++;
    }
}

//=========================================================
// unlockSceneModeCapabilities
//
// - restore the default capabilities strings for flash,
//   focus, and whitebalance
//
void NvCameraSettingsParser::unlockSceneModeCapabilities()
{
    int i = 0;
    while (PARSE_TABLE_VALID(ParserInfoTable[i]))
    {
        if ((((ParserInfoTable[i].type == ECSType_FlashMode) &&
                mFlashSupported) ||
            ((ParserInfoTable[i].type == ECSType_FocusMode) &&
                mFocuserSupported) ||
            (ParserInfoTable[i].type == ECSType_WhiteBalanceMode)) &&
            (ParserInfoTable[i].capsKey != NULL) &&
            (ParserInfoTable[i].initialCapability != NULL))
        {
            mCurrentParameters.set( ParserInfoTable[i].capsKey, ParserInfoTable[i].initialCapability );
        }

        i++;
    }
}

//=========================================================
// Private Interface
//=========================================================

//=========================================================
// extractChanges
//
// - Find and validate the requested parameter changes
// - Build and return the "changes[]" and "newParams"

NvBool NvCameraSettingsParser::extractChanges
( const android::CameraParameters &params
, NvChanges                        changes[]
, NvBool                           allowNonStandard
, int                             *numChanges
, android::CameraParameters       *newParams
)
{
   const char *newparam, *currparam;
   *newParams = mCurrentParameters;
   NvBool imageWasRotated = NV_FALSE;

   // Find changes
   int i, j;
   for(i = 0, j = 0; PARSE_TABLE_VALID(ParserInfoTable[i]); i++)
   {
      NvBool forceChange = NV_FALSE;

      // GPS settings needs to be extracted every time. Otherwise
      // the parser will think it's disabled.
      if (ParserInfoTable[i].type == ECSType_GpsLatitude ||
          ParserInfoTable[i].type == ECSType_GpsLongitude ||
          ParserInfoTable[i].type == ECSType_GpsAltitude ||
          ParserInfoTable[i].type == ECSType_GpsTimestamp ||
          ParserInfoTable[i].type == ECSType_GpsProcMethod)
      {
          forceChange = NV_TRUE;
      }

      if (imageWasRotated &&
          (ParserInfoTable[i].type == ECSType_ThumbnailWidth ||
           ParserInfoTable[i].type == ECSType_ThumbnailHeight))
      {
          forceChange = NV_TRUE;
      }

      if ( ( ParserInfoTable[i].key != 0 ) &&
           ( (newparam = params.get(ParserInfoTable[i].key)) != 0) &&
           ( ((currparam = mCurrentParameters.get(ParserInfoTable[i].key)) == 0) ||
             ((currparam != 0) && (strcmp(newparam, currparam) != 0))  ||
             forceChange) )
      {
         ALOGV(
            "Changed: %s: %s -> %s {%s}\n",
            ParserInfoTable[i].key,
            currparam,
            newparam,
            ParserInfoTable[i].capsKey ? params.get(ParserInfoTable[i].capsKey) : "");

         if(ParserInfoTable[i].type == ECSType_PictureRotation)
         {
             imageWasRotated = NV_TRUE;
         }

         switch ( ParserInfoTable[i].parseType )
         {
             case ECSNone:
                 // No capabilities check needed
                 break;

             case ECSInvalid:
                 ALOGE("RO setting %s set\n", ParserInfoTable[i].key);
                 return ( NV_FALSE );
                 break;

             case ECSSubstring:
                 {
                     const char *begin, *end;
                     int paramlen = NvOsStrlen(newparam);
                     NvBool found = NV_FALSE;
                     const char *capsVal = params.get(ParserInfoTable[i].capsKey);

                     begin = capsVal;
                     while ( begin && *begin != '\0' )
                     {
                         end = strchr(begin, ',');

                         if ( end )
                         {
                             if ( paramlen == end-begin &&
                                  strncmp(newparam, begin, end-begin) == 0 )
                             {
                                found = NV_TRUE;
                                break;
                             }
                         }
                         else
                         {
                             if ( strcmp(newparam, begin) == 0 )
                             {
                                 found = NV_TRUE;
                             }

                             break;
                         }

                         begin = end+1; // skip delim
                     }

                     if ( !found )
                     {
                         ALOGE("Failed substring capabilities check, " \
                              "unsupported parameter: '%s', original: %s \n",
                              newparam,
                              capsVal);
                         return (NV_FALSE );
                     }
                 }
                 break;

             case ECSNumberMin:
                 {
                     int cap, param;
                     const char *capsVal = params.get(ParserInfoTable[i].capsKey);
                     parseInt(capsVal, &cap);
                     parseInt(newparam, &param);
                     if ( param < cap)
                     {
                         ALOGE("Failed number min capabilities check, "\
                              "min='%d', parameter='%d'\n", cap, param);
                         return (NV_FALSE );
                     }
                 }
                 break;

             case ECSNumberMax:
                 {
                     int cap, param;
                     const char *capsVal = params.get(ParserInfoTable[i].capsKey);
                     parseInt(capsVal, &cap);
                     parseInt(newparam, &param);
                     if ( param > cap )
                     {
                         ALOGE("Failed number max capabilities check, "\
                              "max='%d', parameter='%d'\n", cap, param);
                         return (NV_FALSE );
                     }
                 }
                 break;

             case ECSNumberMinMax:
                 {
                     int param_min, param_max, param;
                     const char *capsVal = params.get(ParserInfoTable[i].capsKey);
                     parseRange(capsVal, &param_min, &param_max);
                     parseInt(newparam, &param);
                     if (param < param_min || param > param_max)
                     {
                         ALOGE("Failed number min max capabilities check, "\
                              "min='%d', max='%d', parameter='%d'\n", param_min, param_max, param);
                         return (NV_FALSE);
                     }
                 }
                 break;

             case ECSNumberRange:
                 {
                     int min, max;
                     int param_min, param_max;
                     const char *str = params.get(ParserInfoTable[i].capsKey);
                     NvBool found = NV_FALSE;

                     parseRange(newparam, &param_min, &param_max);

                     while ((str = strchr(str, '(')))
                     {
                         //skip '('
                         str++;
                         parseRange(str, &min, &max);
                         if (param_min == min && param_max == max)
                         {
                             found = NV_TRUE;
                             break;
                         }
                     }

                     if ( !found )
                     {
                         ALOGE("Failed number range capabilities check, "\
                              "min parameter %d, max parameter %d\n", param_min, param_max);
                         return ( NV_FALSE );
                     }
                 }
                 break;

             case ECSPercent:
                 {
                     int low_percent = 0;

                     // Range check the low percent values
                     switch ( ParserInfoTable[i].type )
                     {
                         case ECSType_ZoomSpeed:
                             low_percent = 1;
                             break;
                         default:
                             break;
                     }

                     int param;
                     parseInt(newparam, &param);
                     if ( param > 100 || param < low_percent )
                     {
                         ALOGE("Failed percent capabilities check, "\
                              "range=(%d-100), parameter='%d'\n", low_percent, param);
                         return ( NV_FALSE );
                     }
                 }
                 break;

             case ECSString:
                 // Anything to actually check here?
                 break;

             case ECSFloat:
                 // Anything to actually check here?
                 break;

             case ECSRectangles:
                 {
                     const char *param = newparam;
                     int numRectangles = 0;
                     int maxNumRectangles;
                     const char *capsVal = params.get(ParserInfoTable[i].capsKey);

                     parseInt(capsVal, &maxNumRectangles);

                     NvCameraUserWindow windows[maxNumRectangles];
                     parseWindows(param, windows, maxNumRectangles);

                     while ((param = strchr(param, '(')))
                     {
                         param++;
                         numRectangles++;
                     }

                     // make sure they send us enough, but not too many
                     if (numRectangles == 0 ||
                         numRectangles > maxNumRectangles)
                     {
                         ALOGE("Failed windows capability check, "\
                             "number of windows (%d) must be nonzero and "\
                             "less than the max number of windows (%d)\n",
                             numRectangles,
                             maxNumRectangles);
                         return ( NV_FALSE );
                     }
                     else
                     {
                         // allow special (0,0,0,0,0) case
                         if (numRectangles == 1 &&
                             windows[0].left == 0 &&
                             windows[0].right == 0 &&
                             windows[0].top == 0 &&
                             windows[0].bottom == 0 &&
                             windows[0].weight == 0)
                             break;

                         // make sure they send us window bounds
                         // that follow the API spec
                         for (int i = 0; i < numRectangles; i++)
                         {
                             if (windows[i].left < (-ANDROID_WINDOW_PRECISION) ||
                                 windows[i].right > (ANDROID_WINDOW_PRECISION) ||
                                 windows[i].top < (-ANDROID_WINDOW_PRECISION) ||
                                 windows[i].bottom > (ANDROID_WINDOW_PRECISION) ||
                                 windows[i].left >= windows[i].right ||
                                 windows[i].top >= windows[i].bottom ||
                                 windows[i].weight < 1 ||
                                 windows[i].weight > ANDROID_WINDOW_PRECISION)
                             {
                                 ALOGE("Failed windows capability check, "\
                                     "please check your window bounds and weight: "\
                                     "(%d,%d,%d,%d,%d)",
                                     windows[i].left,
                                     windows[i].top,
                                     windows[i].right,
                                     windows[i].bottom,
                                     windows[i].weight);
                                 return ( NV_FALSE );
                             }
                         }
                     }
                 }
                 break;
             case ECSMatrix4x4:
                 {
                     const char *param = newparam;
                     float matrix[16];

                     if (parseMatrix4x4(param, matrix) < 16)
                     {
                         ALOGE("Failed to get 4x4 matrix\n");
                         return ( NV_FALSE );
                     }
                 }
                 break;
             case ECSResolution:
                 {
                     const char *currRes;
                     const char *capsVal = params.get(ParserInfoTable[i].capsKey);
                     NvCameraUserResolution newRes, capRes;
                     int maxW, maxH;

                     // we used to assume that the resolution capabilities
                     // were stored in a sorted list.  that was partially true,
                     // but they were only sorted by width.  the heights were
                     // not guaranteed to be sorted.  so, unfortunately, we must
                     // traverse the list in order to be sure we are comparing
                     // the requested resolution to the true max that we can
                     // support.
                     maxW = 0;
                     maxH = 0;
                     currRes = capsVal;
                     while(currRes != NULL)
                     {
                         parseResolution(currRes, &capRes);
                         if (capRes.width > maxW)
                             maxW = capRes.width;
                         if (capRes.height > maxH)
                             maxH = capRes.height;
                         currRes = strchr(currRes, ',');
                         if (currRes != NULL)
                             currRes++;
                     }
                     capRes.width = maxW;
                     capRes.height = maxH;

                     parseResolution(newparam, &newRes);

                     // Gralloc supports even width and height only
                     if((newRes.width & 1) || (newRes.height & 1))
                     {
                         ALOGE("requested resolution '%s' is unsupported", newparam);
                         return ( NV_FALSE );
                     }

                     if((newRes.width > capRes.width) || (newRes.height > capRes.height) ||
                         (newRes.width < 1) || (newRes.height < 1))
                     {
                         ALOGE("requested resolution '%s' is unsupported", newparam);
                         return ( NV_FALSE );
                     }
                 }
                 break;
             case ECSResolutionDimension:
                 {
                     int param;
                     parseInt(newparam, &param);
                     if (param % 2) // Resolution dimensions cannot be odd
                     {
                         ALOGE("Failed resolution dimensions capabilities check, "\
                              "parameter='%d' cannot be odd\n", param);
                         return (NV_FALSE );
                     }
                 }
                 break;
             case ECSIso:
                 {
                     int param;
                     int min, max;
                     NvBool found = NV_FALSE;

                     if ((strlen(newparam) == 4) && (strncmp(newparam, "auto", 4)))
                         param = 0;
                     else
                     {
                         char pvalue[5];
                         char *runner;
                         int count;

                         runner =  (char *)ParserInfoTable[i].initialCapability;
                         while (*runner != ',') runner++;
                         runner++;
                         count = 0;
                         while (*(runner+count) != ',') count++;
                         strncpy(pvalue, runner, count);
                         pvalue[count] = '\0';
                         parseInt(pvalue, &min);
                         runner += count + 1;
                         parseInt(runner, &max);
                         parseInt(newparam, &param);
                         if (param < 0)
                         {
                             ALOGE("Failed iso capabilities check, "\
                                  "parameter='%d' cannot be negative\n", param);
                             return (NV_FALSE );
                         }
                         if (((param < min) || (param > max)) && (param != 0))
                         {
                             ALOGE("Failed iso capabilities check, "\
                                  "parameter='%d' is out of range (%d,%d)\n", param, min, max);
                             return (NV_FALSE );
                         }
                     }
                 }
                 break;
             default:
                 ALOGE("Parse type '%d' not valid\n", ParserInfoTable[i].parseType);
                 return ( NV_FALSE );
         }

         if ( j >= ECSType_Max )
         {
             ALOGE("Too many parameters\n");
             return ( NV_FALSE );
         }
         else if (!(ParserInfoTable[i].access & ECSAccess_W))
         {
             ALOGW("Skipping write protected parameter: '%s'\n", ParserInfoTable[i].key);
         }
         else if (ParserInfoTable[i].access & ECSAccess_NS)
         {
              ALOGW("Skipping unsupported parameter: '%s'\n", ParserInfoTable[i].key);
         }
         else if (!allowNonStandard &&
                  (ParserInfoTable[i].access & ECSAccess_NonStandard))
         {
             ALOGE("Skipping non-standard parameter: %s\n", ParserInfoTable[i].key);
         }
         else
         {
             newParams->set(ParserInfoTable[i].key, newparam);
             changes[j].new_value = newparam;
             changes[j].idx = i;
             j++;
         }

         if (ParserInfoTable[i].access & ECSAccess_Old)
         {
             ALOGW("*** DEPRECATED: parameter '%s' is obsolete and will be removed in the future\n", ParserInfoTable[i].key);
         }
      }
   }

   *numChanges = j;

   ALOGD("Extract changes completed, %d total changes\n", *numChanges);

   return ( NV_TRUE );
}

//=========================================================
// buildNewSettings
//
// - build a new settings structure with the changes

void NvCameraSettingsParser::buildNewSettings
( const NvChanges              changes[]
, int                          numChanges
, NvCombinedCameraSettings *newSettings
)
{
   int tmp = 0;

   *newSettings = mCurrentSettings;

   // Gps info may not exist in new setting
   newSettings->GPSBitMapInfo = 0;

   // Update the changed fields
   for (int i = 0; i < numChanges; i++ )
   {
      const char *newParam = changes[i].new_value;

      switch(ParserInfoTable[changes[i].idx].type)
      {
// parseConversionTable handles both the parsing and the setting of the dirty
// bits. ideally we'd move this whole function in this sort of macro-handled
// direction, where we could call some kind of a list macro inside of this
// for-loop that would handle the parsing and settings type mapping
// automatically.  it might be a little harder to follow the patterns for now
// when the dirty bits aren't set inside of the individual case-statements
// for anything that uses this macro, but it needs to stay here as an example
// of the direction this file should move as it evolves.
#define parseConversionTable(_SETTING_, _STRUCT_ELEM_, _TYPE_) \
          { \
              int tmp = settingStrToVal(_SETTING_, newParam); \
              newSettings->_STRUCT_ELEM_ = (_TYPE_)tmp; \
              newSettings->isDirtyForNvMM._STRUCT_ELEM_ = NV_TRUE;\
          }
          case ECSType_PreviewFormat:
              parseConversionTable(ECSType_PreviewFormat
                                 , PreviewFormat
                                 , NvCameraUserPreviewFormat);
              break;

          case ECSType_PreviewSize:
              parseResolution(newParam, &newSettings->previewResolution);
              mCurrentParameters.setPreviewSize(newSettings->previewResolution.width
                                              , newSettings->previewResolution.height);
              newSettings->isDirtyForNvMM.previewResolution = NV_TRUE;
              break;

          case ECSType_PreviewRate:
              parseInt(newParam, &newSettings->previewFpsRange.min);
              newSettings->previewFpsRange.max = newSettings->previewFpsRange.min;
              // The values passed in to previewFPS range API are scaled by 1000
              newSettings->previewFpsRange.min *= 1000;
              newSettings->previewFpsRange.max *= 1000;
              newSettings->isDirtyForNvMM.previewFpsRange = NV_TRUE;
              break;

           case ECSType_PreviewFpsRange:
              parseRange(newParam,
                         &newSettings->previewFpsRange.min,
                         &newSettings->previewFpsRange.max);
              newSettings->isDirtyForNvMM.previewFpsRange = NV_TRUE;
              break;

          case ECSType_VideoHighSpeedRecording:
              parseBool(newParam, &newSettings->videoHighSpeedRec);
              newSettings->isDirtyForNvMM.videoHighSpeedRec = NV_TRUE;
              break;

          case ECSType_PictureFormat:
              parsePictureFormat(newParam
                               , &newSettings->imageFormat
                               , &newSettings->thumbnailSupport);
              confirmThumbnailSupport(newSettings);
              newSettings->isDirtyForNvMM.imageFormat = NV_TRUE;
              newSettings->isDirtyForNvMM.thumbnailSupport = NV_TRUE;
              newSettings->isDirtyForNvMM.thumbnailEnable = NV_TRUE;
              break;

          case ECSType_PictureSize:
              parseResolution(newParam, &newSettings->imageResolution);
              mCurrentParameters.setPictureSize(newSettings->imageResolution.width
                                              , newSettings->imageResolution.height);
              newSettings->isDirtyForNvMM.imageResolution = NV_TRUE;
              break;

          case ECSType_PictureRotation:
              parseConversionTable(ECSType_PictureRotation
                                 , imageRotation
                                 , int);
              break;

          case ECSType_ThumbnailWidth:
              if ( mThumbnailHeightSet )
              {
                  int width, height, imageRotation;
                  NvBool autoRotation;
                  parseInt(newParam, &width);
                  parseInt(mCurrentParameters.get(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT),
                      &height);
                  parseInt(mCurrentParameters.get(CameraParameters::KEY_ROTATION),
                      &imageRotation);
                  parseBool(mCurrentParameters.get(NV_AUTO_ROTATION),
                      &autoRotation);
                  if (width && height)
                  {
                      if ((imageRotation % 180) && autoRotation)
                      {
                          newSettings->thumbnailResolution.width = height;
                          newSettings->thumbnailResolution.height = width;
                          newSettings->isDirtyForParser.thumbnailResolution = NV_TRUE;
                      }
                      else
                      {
                          newSettings->thumbnailResolution.width = width;
                          newSettings->thumbnailResolution.height = height;
                      }
                  }
                  else
                  {
                      newSettings->thumbnailResolution.width = 0;
                      newSettings->thumbnailResolution.height = 0;
                  }
                  confirmThumbnailSupport(newSettings);
                  newSettings->isDirtyForNvMM.thumbnailResolution = NV_TRUE;
                  newSettings->isDirtyForNvMM.thumbnailSupport = NV_TRUE;
                  newSettings->isDirtyForNvMM.thumbnailEnable = NV_TRUE;
              }

              mThumbnailWidthSet = NV_TRUE;
              break;

          case ECSType_ThumbnailHeight:
              if ( mThumbnailWidthSet )
              {
                  int width, height, imageRotation;
                  NvBool autoRotation;
                  parseInt(newParam, &height);
                  parseInt(mCurrentParameters.get(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH),
                      &width);
                  parseInt(mCurrentParameters.get(CameraParameters::KEY_ROTATION),
                      &imageRotation);
                  parseBool(mCurrentParameters.get(NV_AUTO_ROTATION),
                      &autoRotation);
                  if (width && height)
                  {
                      if ((imageRotation % 180) && autoRotation)
                      {
                          newSettings->thumbnailResolution.width = height;
                          newSettings->thumbnailResolution.height = width;
                          newSettings->isDirtyForParser.thumbnailResolution = NV_TRUE;
                      }
                      else
                      {
                          newSettings->thumbnailResolution.width = width;
                          newSettings->thumbnailResolution.height = height;
                      }
                  }
                  else
                  {
                      newSettings->thumbnailResolution.width = 0;
                      newSettings->thumbnailResolution.height = 0;
                  }
                  confirmThumbnailSupport(newSettings);
                  newSettings->isDirtyForNvMM.thumbnailResolution = NV_TRUE;
                  newSettings->isDirtyForNvMM.thumbnailSupport = NV_TRUE;
                  newSettings->isDirtyForNvMM.thumbnailEnable = NV_TRUE;
              }

              mThumbnailHeightSet = NV_TRUE;
              break;
          case ECSType_ThumbnailQuality:
              parseInt(newParam, &newSettings->thumbnailQuality);
              newSettings->isDirtyForNvMM.thumbnailQuality = NV_TRUE;
              break;

          case ECSType_ImageQuality:
              parseInt(newParam, &newSettings->imageQuality);
              newSettings->isDirtyForNvMM.imageQuality = NV_TRUE;
              break;

          case ECSType_VideoFrameFormat:
              parseConversionTable(ECSType_VideoFrameFormat
                                 , videoFrameFormat
                                 , NvCameraUserVideoFrameFormat);
              break;

          case ECSType_VideoSize:
              parseSize(newParam
                      , &newSettings->videoResolution.width
                      , &newSettings->videoResolution.height);
              newSettings->isDirtyForNvMM.videoResolution = NV_TRUE;
              break;

          case ECSType_FlipPreview:
              parseFlipMode(newParam, &newSettings->PreviewFlip);
              newSettings->isDirtyForNvMM.PreviewFlip = NV_TRUE;
              break;

          case ECSType_FlipStill:
              parseFlipMode(newParam, &newSettings->StillFlip);
              newSettings->isDirtyForNvMM.StillFlip = NV_TRUE;
              break;

          case ECSType_FlashMode:
              parseConversionTable(ECSType_FlashMode
                                 , flashMode
                                 , NvCameraUserFlashMode);
              break;

          case ECSType_SceneMode:
              parseConversionTable(ECSType_SceneMode
                                 , sceneMode
                                 , NvCameraUserSceneMode);
              break;

          case ECSType_ColorEffect:
              parseConversionTable(ECSType_ColorEffect
                                 , colorEffect
                                 , NvCameraUserColorEffect);
              break;

          case ECSType_ColorCorrection:
              parseMatrix4x4(newParam, newSettings->colorCorrectionMatrix);
              newSettings->isDirtyForNvMM.colorCorrectionMatrix = NV_TRUE;
              break;

          case ECSType_WhiteBalanceMode:
              parseConversionTable(ECSType_WhiteBalanceMode
                                 , whiteBalance
                                 , NvCameraUserWhitebalance);
              break;

          case ECSType_ExposureTime:
              parseInt(newParam, &newSettings->exposureTimeMicroSec);
              newSettings->isDirtyForNvMM.exposureTimeMicroSec = NV_TRUE;
              break;

          case ECSType_ExposureCompensation:
              parseInt(newParam, &newSettings->exposureCompensationIndex);
              newSettings->isDirtyForNvMM.exposureCompensationIndex = NV_TRUE;
              break;

          case ECSType_FocusMode:
              parseConversionTable(ECSType_FocusMode
                                 , focusMode
                                 , NvCameraUserFocusMode);
              break;

          case ECSType_FocusPosition:
              parseInt(newParam, &newSettings->focusPosition);
              newSettings->isDirtyForNvMM.focusPosition = NV_TRUE;
              break;

          case ECSType_AreasToFocus:
              parseWindows(newParam
                              , newSettings->FocusWindows
                              , MAX_FOCUS_WINDOWS);
              newSettings->isDirtyForNvMM.FocusWindows = NV_TRUE;
              break;

          case ECSType_AreasToMeter:
              parseWindows(newParam
                              , newSettings->ExposureWindows
                              , MAX_EXPOSURE_WINDOWS);
              newSettings->isDirtyForNvMM.ExposureWindows = NV_TRUE;
              break;

          case ECSType_VideoStabEn:
              parseBool(newParam, &newSettings->videoStabOn);
              newSettings->isDirtyForNvMM.videoStabOn = NV_TRUE;
              break;

          case ECSType_StillHDR:
#if ENABLE_NVIDIA_HDR
              parseBool(newParam, &newSettings->stillCapHdrOn);
#else
              newSettings->stillCapHdrOn = NV_FALSE;
#endif
              newSettings->isDirtyForNvMM.stillCapHdrOn = NV_TRUE;
              break;

          case ECSType_StillHDREncodeSourceFrames:
              parseBoolList(
                  newParam,
                  newSettings->stillHdrSourceFramesToEncode,
                  newSettings->stillHdrSourceFrameCount);
              newSettings->isDirtyForNvMM.stillHdrSourceFramesToEncode = NV_TRUE;
              break;

          case ECSType_CustomPostviewEn:
              parseOnOff(newParam, &newSettings->customPostviewOn);
              newSettings->isDirtyForNvMM.customPostviewOn = NV_TRUE;
              break;

          case ECSType_VideoSpeed:
              parseFloat(newParam, &newSettings->videoSpeed);
              newSettings->isDirtyForNvMM.videoSpeed = NV_TRUE;
              break;

          case ECSType_TimestampEn:
              parseOnOff(newParam, &newSettings->timestampEnable);
              newSettings->isDirtyForNvMM.timestampEnable = NV_TRUE;
              break;

          case ECSType_LightFreqMode:
              parseConversionTable(ECSType_LightFreqMode
                                 , antiBanding
                                 , NvCameraUserAntiBanding);
              break;

          case ECSType_ZoomValue:
              parseZoomValue(newParam, &newSettings->zoomLevel);
              newSettings->isDirtyForNvMM.zoomLevel = NV_TRUE;
              break;

          case ECSType_ZoomStep:
              parseFloat(newParam, &newSettings->zoomStep);
              newSettings->isDirtyForNvMM.zoomStep = NV_TRUE;
              break;

          case ECSType_ZoomSpeed:
              parseInt(newParam, &newSettings->zoomSpeed);
              newSettings->isDirtyForNvMM.zoomSpeed = NV_TRUE;
              break;

          case ECSType_NSLNumBuffers:
              parseInt(newParam, &newSettings->nslNumBuffers);
              newSettings->isDirtyForNvMM.nslNumBuffers = NV_TRUE;
              break;

          case ECSType_NSLSkipCount:
              parseInt(newParam, &newSettings->nslSkipCount);
              newSettings->isDirtyForNvMM.nslSkipCount = NV_TRUE;
              break;

          case ECSType_NSLBurstCount:
              parseInt(newParam, &newSettings->nslBurstCount);
              newSettings->isDirtyForNvMM.nslBurstCount = NV_TRUE;
              break;

          case ECSType_SkipCount:
              parseInt(newParam, &newSettings->skipCount);
              newSettings->isDirtyForNvMM.skipCount = NV_TRUE;
              break;

          case ECSType_BurstCount:
              parseInt(newParam, &newSettings->burstCaptureCount);
              newSettings->isDirtyForNvMM.burstCaptureCount = NV_TRUE;
              break;

          case ECSType_BracketCapture:
              parseBracketCapture(newParam,&newSettings->bracketCaptureCount,
                            newSettings->bracketFStopAdj,MAX_BRACKET_CAPTURES);
              newSettings->isDirtyForNvMM.bracketCaptureCount = NV_TRUE;
              break;

          case ECSType_RawDumpFlag:
              parseInt(newParam, &newSettings->rawDumpFlag);
              newSettings->isDirtyForNvMM.rawDumpFlag = NV_TRUE;
              break;

          case ECSType_ExifMake:
              copyString(newParam, newSettings->exifMake, EXIF_STRING_LENGTH);
              newSettings->isDirtyForNvMM.exifMake = NV_TRUE;
              break;

          case ECSType_ExifModel:
              copyString(newParam, newSettings->exifModel, EXIF_STRING_LENGTH);
              newSettings->isDirtyForNvMM.exifModel = NV_TRUE;
              break;

          case ECSType_UserComment:
              copyString(newParam, newSettings->userComment, USER_COMMENT_LENGTH);
              newSettings->isDirtyForNvMM.userComment = NV_TRUE;
              break;

          case ECSType_GpsLatitude:
              parseDegrees(newParam, &newSettings->gpsLatitude, &newSettings->latitudeDirection);
              newSettings->GPSBitMapInfo |= NvCameraGpsBitMap_LatitudeRef;
              newSettings->isDirtyForNvMM.gpsLatitude = NV_TRUE;
              newSettings->isDirtyForNvMM.latitudeDirection = NV_TRUE;
              newSettings->isDirtyForNvMM.GPSBitMapInfo = NV_TRUE;
              break;

          case ECSType_GpsLongitude:
              parseDegrees(newParam, &newSettings->gpsLongitude, &newSettings->longitudeDirection);
              newSettings->GPSBitMapInfo |= NvCameraGpsBitMap_LongitudeRef;
              newSettings->isDirtyForNvMM.gpsLongitude = NV_TRUE;
              newSettings->isDirtyForNvMM.longitudeDirection = NV_TRUE;
              newSettings->isDirtyForNvMM.GPSBitMapInfo = NV_TRUE;
              break;

          case ECSType_GpsAltitude:
              parseAltitude(newParam, &newSettings->gpsAltitude, &newSettings->gpsAltitudeRef);
              newSettings->GPSBitMapInfo |= NvCameraGpsBitMap_AltitudeRef;
              newSettings->isDirtyForNvMM.gpsAltitude = NV_TRUE;
              newSettings->isDirtyForNvMM.gpsAltitudeRef = NV_TRUE;
              newSettings->isDirtyForNvMM.GPSBitMapInfo = NV_TRUE;
              break;

          case ECSType_GpsTimestamp:
              parseTime(newParam, &newSettings->gpsTimestamp);
              newSettings->GPSBitMapInfo |= (NvCameraGpsBitMap_TimeStamp |
                                             NvCameraGpsBitMap_DateStamp);
              newSettings->isDirtyForNvMM.gpsTimestamp = NV_TRUE;
              newSettings->isDirtyForNvMM.GPSBitMapInfo = NV_TRUE;
              break;

          case ECSType_GpsProcMethod:
              copyString(newParam, newSettings->gpsProcMethod, GPS_PROC_METHOD_LENGTH);
              newSettings->GPSBitMapInfo |=
                  NvCameraGpsBitMap_ProcessingMethod;
              newSettings->isDirtyForNvMM.gpsProcMethod = NV_TRUE;
              newSettings->isDirtyForNvMM.GPSBitMapInfo = NV_TRUE;
              break;

          case ECSType_GpsMapDatum:
              copyString(newParam, newSettings->gpsMapDatum, GPS_MAP_DATUM_LENGTH);
              newSettings->GPSBitMapInfo |= NvCameraGpsBitMap_MapDatum;
              newSettings->isDirtyForNvMM.gpsMapDatum = NV_TRUE;
              newSettings->isDirtyForNvMM.GPSBitMapInfo = NV_TRUE;
              break;

          case ECSType_PictureIso:
              parseInt(newParam, &newSettings->iso);
              newSettings->isDirtyForNvMM.iso = NV_TRUE;
              break;

          case ECSType_Contrast:
#if SUPPORT_CONTRAST_LEVELS
              parseConversionTable(ECSType_Contrast, contrast, int);
#endif
              break;

          case ECSType_Saturation:
              parseInt(newParam, &newSettings->saturation);
              newSettings->isDirtyForNvMM.saturation = NV_TRUE;
              break;

          case ECSType_EdgeEnhancement:
              parseInt(newParam, &newSettings->edgeEnhancement);
              newSettings->isDirtyForNvMM.edgeEnhancement = NV_TRUE;
              break;

          case ECSType_UseNvBuffers:
              parseInt(newParam, &newSettings->useNvBuffers);
              newSettings->isDirtyForNvMM.useNvBuffers = NV_TRUE;
              break;

          case ECSType_RecordingHint:
              parseBool(newParam, &newSettings->recordingHint);
              newSettings->isDirtyForNvMM.recordingHint = NV_TRUE;
              break;

          case ECSType_SensorMode:
              parseSensorMode(newParam, &newSettings->sensorMode);
              newSettings->isDirtyForNvMM.sensorMode = NV_TRUE;
              break;

          case ECSType_CameraRenderer:
              parseOnOff(newParam, &newSettings->useCameraRenderer);
              newSettings->isDirtyForNvMM.useCameraRenderer = NV_TRUE;
              break;

          case ECSType_AWBLock:
              parseBool(newParam, &newSettings->AWBLocked);
              newSettings->isDirtyForNvMM.AWBLocked = NV_TRUE;
              break;

          case ECSType_AELock:
              parseBool(newParam, &newSettings->AELocked);
              newSettings->isDirtyForNvMM.AELocked = NV_TRUE;
              break;

          case ECSType_CameraStereoMode:
              parseCameraStereoMode(newParam, &newSettings->stereoMode);
              newSettings->isDirtyForNvMM.stereoMode = NV_TRUE;
              break;

          case ECSType_FDLimit:
              parseInt(newParam, &newSettings->fdLimit);
              newSettings->isDirtyForNvMM.fdLimit = NV_TRUE;
              break;

          case ECSType_FDDebug:
              parseOnOff(newParam, &newSettings->fdDebug);
              newSettings->isDirtyForNvMM.fdDebug = NV_TRUE;
              break;

#ifdef FRAMEWORK_HAS_MACRO_MODE_SUPPORT
          case ECSType_LowLightThreshold:
              parseInt(newParam, &newSettings->LowLightThreshold);
              newSettings->isDirtyForNvMM.LowLightThreshold = NV_TRUE;
              break;
          case ECSType_MacroModeThreshold:
              parseInt(newParam, &newSettings->MacroModeThreshold);
              newSettings->isDirtyForNvMM.MacroModeThreshold = NV_TRUE;
              break;
#endif

          case ECSType_FocusMoveMsg:
              parseBool(newParam, &newSettings->FocusMoveMsg);
              newSettings->isDirtyForNvMM.FocusMoveMsg = NV_TRUE;
              break;

          case ECSType_AutoRotation:
              parseBool(newParam, &newSettings->autoRotation);
              newSettings->isDirtyForNvMM.autoRotation = NV_TRUE;
              break;

          case ECSType_CaptureMode:
              parseConversionTable(ECSType_CaptureMode
                                 , CaptureMode
                                 , NvCameraUserCaptureMode);
              break;

          case ECSType_DataTapFormat:
              parseConversionTable(ECSType_DataTapFormat, DataTapFormat,
                                NvCameraUserDataTapFormat);
              break;
          case ECSType_DataTapSize:
              parseSize(newParam, &newSettings->DataTapResolution.width,
                                &newSettings->DataTapResolution.height);
              newSettings->isDirtyForNvMM.DataTapResolution = NV_TRUE;
              break;
          case ECSType_DataTap:
              parseOnOff(newParam, &newSettings->mDataTap);
              newSettings->isDirtyForNvMM.mDataTap = NV_TRUE;
              break;
          case ECSType_ANRMode:
              parseAnrMode(newParam, &newSettings->AnrInfo);
              newSettings->isDirtyForNvMM.AnrInfo = NV_TRUE;
              break;
          case ECSType_DisablePreviewPause:
              parseBool(newParam, &newSettings->DisablePreviewPause);
              newSettings->isDirtyForNvMM.DisablePreviewPause = NV_TRUE;
              break;

          case ECSType_PreviewCallbackSizeEnable:
              parseBool(newParam, &newSettings->PreviewCbSizeEnable);
              break;

          case ECSType_PreviewCallbackSize:
              parseSize(newParam,
                    &newSettings->PreviewCbSize.width,
                    &newSettings->PreviewCbSize.height);
              break;

#undef parseConversionTable

          default:
              //LOGE("Param type %d not supported\n", ParserInfoTable[changes[i].idx].type);
              break;
      }
   }
}

//=========================================================
// findSettingsKey
//
const char* NvCameraSettingsParser::findSettingsKey( ECSType setting )
{
    const char *key = NULL;
    int i = 0;
    while ( PARSE_TABLE_VALID(ParserInfoTable[i]) )
    {
        if ( ParserInfoTable[i].type == setting )
        {
            key = ParserInfoTable[i].key;
            break;
        }

        i++;
    }

    return key;
}

//=========================================================
// findCapsKey
//
const char* NvCameraSettingsParser::findCapsKey( ECSType setting )
{
    const char *key = NULL;
    int i = 0;
    while ( PARSE_TABLE_VALID(ParserInfoTable[i]) )
    {
        if ( ParserInfoTable[i].type == setting )
        {
            key = ParserInfoTable[i].capsKey;
            break;
        }

        i++;
    }

    return key;
}

//=========================================================
// findKeyDefValue
// This function returns default value of given key.

const char* NvCameraSettingsParser::findKeyDefValue( ECSType setting )
{
    const char *value = NULL;
    int i = 0;
    while ( PARSE_TABLE_VALID(ParserInfoTable[i]) )
    {
        if ( ParserInfoTable[i].type == setting )
        {
            value = ParserInfoTable[i].initialDefault;
            break;
        }

        i++;
    }

    return value;
}

//=========================================================
// parseSize
//
void NvCameraSettingsParser::parseSize(const char *str, int *width, int *height)
{
    // Find the width.
    char *end;
    int w = (int)strtol(str, &end, 10);
    // If an 'x' does not immediately follow, give up.
    if (*end != 'x')
        return;

    // Find the height, immediately after the 'x'.
    int h = (int)strtol(end+1, 0, 10);

    *width = w;
    *height = h;
}

//=========================================================
// parseRange
//
void NvCameraSettingsParser::parseRange(const char *str, int *min, int *max)
{
    // Find the width.
    char *end;
    int lo = (int)strtol(str, &end, 10);
    // If an ',' does not immediately follow, give up.
    if (*end != ',')
        return;

    // Find the height, immediately after the ','.
    int hi = (int)strtol(end+1, 0, 10);

    *min = lo;
    *max = hi;
}

//=========================================================
// parseMode
//
void NvCameraSettingsParser::parseMode(const char *str, int *width, int *height, int *fps)
{
    // Find the width.
    char *end1, *end2;
    int w = (int)strtol(str, &end1, 10);
    // If an 'x' does not immediately follow, give up.
    if (*end1 != 'x')
        return;

    // Find the height, immediately after the 'x'.
    int h = (int)strtol(end1+1, &end2, 10);
    // If an 'x' does not immediately follow, give up.
    if (*end2 != 'x')
        return;

    // Find the fps, immediately after the 'x'.
    int f = (int)strtol(end2+1, 0, 10);

    *width = w;
    *height = h;
    *fps = f;
}

//=========================================================
// parseInt
//
void NvCameraSettingsParser::parseInt(const char *str, int *num)
{
    *num = strtol(str, 0, 0);
}

//=========================================================
// parseFloat
//
void NvCameraSettingsParser::parseFloat(const char *str, float *num)
{
    *num = strtof(str, 0);
}

//=========================================================
// parseDegrees
//
void NvCameraSettingsParser::parseDegrees(const char *str, unsigned int *ddmmss, NvBool *direction)
{
    if ( strcmp(str, "off") != 0 )
    {
        double coord = strtod(str, 0);

        *direction = (coord >= 0) ? NV_TRUE : NV_FALSE;

        coord = fabs(coord);

        int degrees = floor(coord);
        coord -= degrees;
        coord *= 60;
        int minutes = floor(coord);
        coord -= minutes;
        coord *= 60;
        int seconds = round(coord * 1000);

        *ddmmss = ((degrees & 0xff) << 24) |
                  ((minutes & 0xff) << 16) |
                   (seconds & 0xffff);
    }
    else
    {
        *ddmmss = 0xffffffff;
        *direction = NV_FALSE;
    }
}

//=========================================================
// parseAltitude
//
void NvCameraSettingsParser::parseAltitude(const char *str, float *alt, NvBool *ref)
{
    float input = strtof(str, 0);

    *alt = fabs(input);
    *ref = (input >= 0) ? NV_FALSE : NV_TRUE;
}

//=========================================================
// parseTime
//
void NvCameraSettingsParser::parseTime(const char *str, NvCameraUserTime* time)
{
    time_t input = (time_t)strtol(str, 0, 0);
    struct tm *datetime = gmtime(&input);

    if (datetime == NULL)
    {
        memset(time, 0x0, sizeof(NvCameraUserTime));
    }
    else
    {
        time->hr  = datetime->tm_hour;
        time->min = datetime->tm_min;
        time->sec = datetime->tm_sec;

        /* populate date */
        NvOsSnprintf(time->date,
            MAX_DATE_LENGTH,
            "%04d:%02d:%02d", 1900+datetime->tm_year,
                              1+datetime->tm_mon,
                              datetime->tm_mday);
    }
}

//=========================================================
// parseOnOff
//
void NvCameraSettingsParser::parseOnOff(const char *str, NvBool *on)
{
    *on = (strcmp(str, "on") == 0) ? NV_TRUE : NV_FALSE;
}

//=========================================================
// parseBoolList
//
// parses a list of bools in a string format like "true,false,false,true"
// where each bool value is delimited by a comma.
// inputs:
//  str - a pointer to the string of bools that we want to parse
//  value - a pointer to an array of bools, this is where the
//          function will place the results
//  maxLength - size of the array of bools, used to make sure we
//              don't overstep the array bounds if a string is passed
//              in that's longer than we expect
void
NvCameraSettingsParser::parseBoolList(
    const char *str,
    NvBool *value,
    int maxLength)
{
    int i = 0;
    while ((i < maxLength) &&
           str &&
           (*str != '\0'))
    {
        // efficient (but stupid) check...assume that a word starting with 't'
        // is a true, anything else is false.
        if (*str == 't')
        {
            value[i] = NV_TRUE;
        }
        else
        {
            value[i] = NV_FALSE;
        }

        str = strchr(str, ',');
        // strchr can return NULL, we should exit immediately if it
        // does so that we don't increment a NULL pointer to a non-NULL
        // value in the next line and dereference it in the while loop.
        if (!str)
        {
            break;
        }

        str++; // skip the ',' delimiter
        i++;
    }
}

//=========================================================
// parseBool
//
void NvCameraSettingsParser::parseBool(const char *str, NvBool *value)
{
    *value = (strcmp(str, "true") == 0) ? NV_TRUE : NV_FALSE;
}

//=========================================================
// parseZoomValue
//
void NvCameraSettingsParser::parseZoomValue(const char *str, int *zoom)
{
    *zoom = atoi(str);
}

NvBool NvCameraSettingsParser::checkZoomValue(int zoom)
{
    int maxZoom = -1;
    const char* maxZoomStr = mCurrentParameters.get(findCapsKey(ECSType_ZoomValue));

    parseZoomValue(maxZoomStr, &maxZoom);

    return (0 <= zoom && zoom <= maxZoom);
}

NvBool NvCameraSettingsParser::checkFpsRangeValue(const char *fpsRange)
{
    int min = 0, max = 0;

    if (fpsRange == NULL)
    {
        return NV_TRUE;
    }
    parseRange(fpsRange, &min, &max);
    return (min >= 0) && (max >= 0) && (min <= max);
}

//=========================================================
// parseFlipMode
//
void NvCameraSettingsParser::parseFlipMode(
    const char *str,
    NvCameraUserFlip *pFlip)
{
    if (strcmp(str, "horizontal") == 0)
    {
        *pFlip = NvCameraUserFlip_Horizontal;
    }
    else if (strcmp(str, "vertical") == 0)
    {
        *pFlip = NvCameraUserFlip_Vertical;
    }
    else if (strcmp(str, "both") == 0)
    {
        *pFlip = NvCameraUserFlip_Both;
    }
    else
    {
        *pFlip = NvCameraUserFlip_None;
    }
}

//=========================================================
// parseAnrMode
//
void NvCameraSettingsParser::parseAnrMode(const char *str, AnrMode *mode)
{
    if (strcmp(str, "off") == 0)
    {
        *mode = AnrMode_Off;
    }
    else if (strcmp(str, "force_on") == 0)
    {
        *mode = AnrMode_ForceOn;
    }
    else
        *mode = AnrMode_Auto;
}

//=========================================================
// parseBracketCapture
//
void  NvCameraSettingsParser::parseBracketCapture(const char *str,
                                int *pBracketCaptureCount,
                                float *bracketFStopAdj,
                                int maxBracketCount)
{

    char *end, *start;
    int i = 0;

    start = (char *)str;
    end = start;
    *pBracketCaptureCount = 0;

    if (*start == '\0')
    {
        return;
    }

    // master parser does not allow empty strings
    // therefore we use a string with a single space
    // to denote no new values.
    if (*start == ' ' && *(start+1) == '\0')
    {
        return;
    }

    while (*start != '\0' && i < maxBracketCount)
    {
        bracketFStopAdj[i++] = strtof (start, &end);
        start = end;
        if (*start != ',')
        {// next character must be a comma or \0
            if (*start != '\0')
            {
                 ALOGE("Invalid parameter in parsing bracket capture");
            }
            break;
        }
        start++;
    }

    *pBracketCaptureCount = i;

}

//=========================================================
// parseWindows
//
void NvCameraSettingsParser::parseWindows(const char *str,
    NvCameraUserWindow *pWindow,
    int maxNumWindows)
{
    const char *windowStr;
    int vals[5];
    int i = 0;
    windowStr = str;

    NvOsMemset(pWindow, 0, maxNumWindows*sizeof(NvCameraUserWindow));
    while ((i < maxNumWindows) &&
        windowStr &&
        (*windowStr != '\0') &&
        (windowStr = strchr(windowStr, '(')))
    {
        memset(vals, 0, sizeof(vals));
        windowStr++; // skip '('

        for ( int j = 0; j < 5; j++ )
        {
            vals[j] = atoi(windowStr);

            windowStr = strchr(windowStr, ',');
            if (windowStr && (*windowStr != '\0'))
            {
                windowStr++;
            }
            else
            {
                break;
            }
        }

        pWindow[i].left = vals[0];
        pWindow[i].top = vals[1];
        pWindow[i].right = vals[2];
        pWindow[i].bottom = vals[3];
        pWindow[i].weight = vals[4];

        i++;
    }
}

//=========================================================
// parseMatrix4x4
//
int NvCameraSettingsParser::parseMatrix4x4(const char *str, float matrix[])
{
    const char *windowStr;
    int i = 0;
    windowStr = str;

    while ((i < 16) &&
        windowStr &&
        (*windowStr != '\0'))
    {
        matrix[i] = atof(windowStr);
        i++;

        windowStr = strchr(windowStr, ',');
        if (windowStr && (*windowStr != '\0'))
        {
            windowStr++;
        }
        else
        {
            break;
        }
    }

    return i;
}

//=========================================================
// copyString
//
void NvCameraSettingsParser::copyString(const char *str
                              , char *settingStr
                              , int settingStrLen)
{
    NvOsStrncpy(settingStr, str, settingStrLen);
}

//=========================================================
// parsePictureFormat
//
void
NvCameraSettingsParser::parsePictureFormat(
    const char *str,
    NvCameraUserImageFormat *captureFormat,
    NvBool *thumbSupport)
{

    if ( (strcmp("jpeg", str) == 0) ||
         (strcmp("exif", str) == 0) )
    {
        *captureFormat = NvCameraImageFormat_Jpeg;
        *thumbSupport = NV_TRUE;
    }
    else
    {
        *captureFormat = NvCameraImageFormat_Jpeg;
        *thumbSupport = NV_FALSE;
    }
}

//=========================================================
// parseResolution
//

void NvCameraSettingsParser::parseResolution( const char *str
                                           , NvCameraUserResolution *res)
{
    int parsew, parseh;

    parseSize(str, &parsew, &parseh);

    res->width = parsew;
    res->height = parseh;
}
//=========================================================
// parseSensorMode
//

void NvCameraSettingsParser::parseSensorMode( const char *str
                                           , NvCameraUserSensorMode *res)
{
    int parsew, parseh;
    int parsef;

    parseMode(str, &parsew, &parseh, &parsef);

    res->width  = parsew;
    res->height = parseh;
    res->fps    = parsef;
}

//========================================================
// parseCameraStereoMode
//

void NvCameraSettingsParser::parseCameraStereoMode( const char *str
                                            , NvCameraUserStereoMode *mode)
{
    if (0 == strcmp(str, "left"))
    {
        *mode = NvCameraStereoMode_LeftOnly;
    }
    else if (0 == strcmp(str, "right"))
    {
        *mode = NvCameraStereoMode_RightOnly;
    }
    else if (0 == strcmp(str, "stereo"))
    {
        *mode = NvCameraStereoMode_Stereo;
    }
    else
    {
        *mode = NvCameraStereoMode_Invalid;
    }
}

//=========================================================
// confirmThumbnailSupport
//
// Thumbnail support depends on two things:
// 1) the file format supports it
// 2) the application hasn't disabled it with size=0x0 (2.2/API level 8 addition)
//
void NvCameraSettingsParser::confirmThumbnailSupport(
                                    NvCombinedCameraSettings *settings)
{
    settings->thumbnailEnable = (settings->thumbnailSupport &&
                                 (settings->thumbnailResolution.width != 0 &&
                                  settings->thumbnailResolution.height != 0));
}

void NvCameraSettingsParser::encodeResolutions
                                    (const android::Vector<NvCameraUserResolution>& res,
                                     char* str, int size)
{
    char *tmp = str;
    const char *fmt;
    int written, n = (int)res.size();
    //clear the buffer before we start writing to it
    NvOsMemset(str, 0, size);
    for (int i = 0; i < n; i++)
    {
        if(i < (n-1))
            fmt = "%dx%d,";
        else
            fmt = "%dx%d";

        written = NvOsSnprintf(tmp, size, fmt,
                           res[i].width, res[i].height);
        size -= written;
        tmp += written;
    }
}

void NvCameraSettingsParser::encodeSensorModes
                                    (const android::Vector<NvCameraUserSensorMode>& res,
                                     char* str, int size)
{
    char *tmp = str;
    const char *fmt;
    int written, n = (int)res.size();
    //clear the buffer before we start writing to it
    NvOsMemset(str, 0, size);
    for (int i = 0; i < n; i++)
    {
        if(i < (n-1))
            fmt = "%dx%dx%d,";
        else
            fmt = "%dx%dx%d";

        written = NvOsSnprintf(tmp, size, fmt,
                           res[i].width, res[i].height, res[i].fps);
        size -= written;
        tmp += written;
    }
}

void NvCameraSettingsParser::encodeVideoPreviewfps
                                    (const android::Vector<NvCameraUserVideoPreviewFps>& res,
                                     char* str, int size)
{
    char *tmp = str;
    const char *fmt;
    int written, n = (int)res.size();
    //clear the buffer before we start writing to it
    NvOsMemset(str, 0, size);
    for (int i = 0; i < n; i++)
    {
        if(i < (n-1))
            fmt = "%dx%d:%dx%d:%d,";
        else
            fmt = "%dx%d:%dx%d:%d";

        written = NvOsSnprintf(tmp, size, fmt,
                           res[i].VideoWidth, res[i].VideoHeight, res[i].PreviewWidth,
                           res[i].PreviewHeight, res[i].Maxfps);
        size -= written;
        tmp += written;
    }
}

void NvCameraSettingsParser::encodePreferredPreviewSizeForVideo
                                    (const android::Vector<NvCameraUserResolution>& res,
                                     char* str, int size)
{
    int n = (int)res.size();

    //clear the buffer before we start writing to it
    NvOsMemset(str, 0, size);
    NvOsSnprintf(str, size, "%dx%d", res[n-1].width, res[n-1].height);
}

void NvCameraSettingsParser::encodeFlashModes
                                    (const android::Vector<NvCameraUserFlashMode>& fmode,
                                     char* str, int size)
{
    char *tmp = str;
    const char *fmt;
    int written, n = (int)fmode.size();
    //clear the buffer before we start writing to it
    NvOsMemset(str, 0, size);
    for (int i = 0; i < n; i++) {
        if(i < n-1)
            fmt = "%s,";
        else
            fmt = "%s";

        written = NvOsSnprintf(tmp, size, fmt, settingValToStr(ECSType_FlashMode, fmode[i]));
        size -= written;
        tmp += written;
    }

}

void NvCameraSettingsParser::encodeFocusModes
                                    (const android::Vector<NvCameraUserFocusMode>& fmode,
                                     char* str, int size)
{
    char *tmp = str;
    const char *fmt;
    int written, n = (int)fmode.size();
    //clear the buffer before we start writing to it
    NvOsMemset(str, 0, size);
    for (int i = 0; i < n; i++) {
        if(i < n-1)
            fmt = "%s,";
        else
            fmt = "%s";

        written = NvOsSnprintf(tmp, size, fmt, settingValToStr(ECSType_FocusMode, fmode[i]));
        size -= written;
        tmp += written;
    }

}

void NvCameraSettingsParser::encodeStereoModes
                                    (const android::Vector<NvCameraUserStereoMode>& smode,
                                     char* str, int size)
{
    char *tmp = str;
    const char *fmt;
    int written, n = (int)smode.size();
    //clear the buffer before we start writing to it
    NvOsMemset(str, 0, size);
    for (int i = 0; i < n; i++) {
        if(i < n-1)
            fmt = "%s,";
        else
            fmt = "%s";

        written = NvOsSnprintf(tmp, size, fmt, settingValToStr(ECSType_CameraStereoMode, smode[i]));
        size -= written;
        tmp += written;
    }
}

const char* NvCameraSettingsParser::settingValToStr(
                                            ECSType setting, int val)
{
    const char* str = NULL;
    const ConversionTable_T* tbl = settingToTbl(setting);

    int i = 0;
    while(tbl && tbl[i].strsetting) {
        if(tbl[i].intsetting == val) {
            str = tbl[i].strsetting;
            break;
        }
        i++;
    }

    if(!str)
        ALOGE("Couldn't find val %d for setting %d\n", val, setting);

    return str;
}

int NvCameraSettingsParser::settingStrToVal(
                                            ECSType setting, const char* str)
{
    int val = 0x7FFFFFFF;
    const ConversionTable_T* tbl = settingToTbl(setting);

    int i = 0;
    while(tbl && tbl[i].strsetting) {
        if(strcmp(tbl[i].strsetting, str) == 0) {
            val = tbl[i].intsetting;
            break;
        }
        i++;
    }

    if(val == 0x7FFFFFFF)
        ALOGE("Couldn't find str %s for setting %d\n", str, setting);

    return val;
}

const ConversionTable_T* NvCameraSettingsParser::settingToTbl(
                                            ECSType setting)
{
    const ConversionTable_T* tbl = NULL;

    switch(setting) {
        case ECSType_PreviewFormat:
            tbl = previewFormats;
            break;
        case ECSType_VideoFrameFormat:
            tbl = videoFrameFormats;
            break;
        case ECSType_PictureRotation:
            tbl = rotationValues;
            break;
        case ECSType_FlashMode:
            tbl = flashModes;
            break;
        case ECSType_SceneMode:
            tbl = sceneModes;
            break;
        case ECSType_ColorEffect:
            tbl = colorEffects;
            break;
        case ECSType_WhiteBalanceMode:
            tbl = whitebalanceModes;
            break;
        case ECSType_FocusMode:
            tbl = focusModes;
            break;
        case ECSType_LightFreqMode:
            tbl = lightFreqModes;
            break;
        case ECSType_Contrast:
            tbl = contrastModes;
            break;
        case ECSType_CameraStereoMode:
            tbl = nvStereoMode;
            break;
        case ECSType_CaptureMode:
            tbl = captureModes;
            break;
        case ECSType_DataTapFormat:
            tbl = nvDataTapFormats;
            break;
        default:
            ALOGE("No conversion for setting %d\n", setting);
            break;
    }

    return tbl;
}

// program all of the defaults from the info table into the
// CameraParameters object that's passed in
void NvCameraSettingsParser::initializeParams(CameraParameters& initialParameters)
{
    int i = 0;
    while (PARSE_TABLE_VALID(ParserInfoTable[i]))
    {
        if (ParserInfoTable[i].key && ParserInfoTable[i].initialDefault)
        {
            // put read-only params into the current params struct so that
            // parse logic doesn't think we're trying to change them during init
            if (ParserInfoTable[i].access == ACCESS_RO ||
                ParserInfoTable[i].access == ACCESS_RO_NONSTD)
            {
                mCurrentParameters.set(
                    ParserInfoTable[i].key,
                    ParserInfoTable[i].initialDefault);
            }
            initialParameters.set(
                ParserInfoTable[i].key,
                ParserInfoTable[i].initialDefault);

        }

        if (ParserInfoTable[i].capsKey && ParserInfoTable[i].initialCapability)
        {
            mCurrentParameters.set(
                ParserInfoTable[i].capsKey,
                ParserInfoTable[i].initialCapability);
            initialParameters.set(
                ParserInfoTable[i].capsKey,
                ParserInfoTable[i].initialCapability);
        }

        i++;
    }
}

NvBool NvCameraSettingsParser::isFlashSupported()
{
    return mFlashSupported;
}

NvBool NvCameraSettingsParser::isFocusSupported()
{
    return mFocuserSupported;
}

} // namespace android
