/*
 * Copyright (c) 2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include <math.h>
#include "nvcamerahal3core.h"
#include "nvrm_init.h"
#include "nvcamerahal3streamport.h"
#include "nvcamerahal3metadatahandler.h"
#include "nvmetadatatranslator.h"

#include "nvcamerahal3_tags.h"
#include "nv_log.h"

#define NVCAMERAISP_AUTO_FLOAT (-1.f)

namespace android {

#define NVCAMHAL3_RANGE_MAKE(_range, _low, _high) {     \
    _range.low = _low;                                  \
    _range.high = _high;                                \
}

/* V3 defined AF state machine differs from the state machine
 * implementation in core. Presence of trigger will cause state change
 * in the core for Continuous Picture/Video modes. For avoiding this
 * setting trigger to cancel*/
void NvCameraHal3Core::handleAfTrigger(
        NvCameraHal3_Public_Controls& prop)
{
    NvCamProperty_Public_Controls& cprop = prop.CoreCntrlProps;
    switch (cprop.AfTrigger)
    {
        case NvCamAfTrigger_Start:
            mAfTriggerStart = NV_TRUE;
            mAfTriggerAck = NV_FALSE;
            mAfTriggerId = cprop.AfTriggerId;
            break;
        case NvCamAfTrigger_Cancel:
            mAfTriggerStart = NV_FALSE;
            mAfTriggerAck = NV_FALSE;
            mAfTriggerId = cprop.AfTriggerId;
            break;
        default:
            if (NV_FALSE == mAfTriggerStart)
            {
                mAfTriggerId = 0;
            }
            break;
    }
}

/*
 * Update the AePrecaptureId and AfTriggerId in the partial dynamic data
 * received from Core before sending it to the framework.
*/

void NvCameraHal3Core::update3AIds(NvCameraHal3_Public_Dynamic& dynProp)
{
    NvCamProperty_Public_Dynamic& cDynProp = dynProp.CoreDynProps;
    if (cDynProp.Ae.PartialResultUpdated)
    {
        dynProp.AePrecaptureId = mAePrecaptureId;
    }

    if (cDynProp.Af.PartialResultUpdated)
    {
        if (cDynProp.Af.AfTriggerId != mAfTriggerId &&
            (cDynProp.Af.AfMode == NvCamAfMode_Auto ||
             cDynProp.Af.AfMode == NvCamAfMode_Macro) &&
            (cDynProp.Af.AfState == NvCamAfState_FocusLocked ||
             cDynProp.Af.AfState == NvCamAfState_NotFocusLocked))
        {
            cDynProp.Af.AfState = NvCamAfState_ActiveScan;
        }
    }
}

/*
 * Cache Ae precapture trigger ID when AePrecaptureTrigger_Start request
 * is received.
*/
void NvCameraHal3Core::handleAeTrigger(
        NvCameraHal3_Public_Controls& prop)
{
    if (prop.CoreCntrlProps.AePrecaptureTrigger ==  NvCamAePrecaptureTrigger_Start)
    {
        mAePrecaptureId = (int32_t) prop.AePrecaptureId;
        NV_LOGV(HAL3_META_DATA_PROCESS_TAG, "%s: AE PrecaptureTrigger_Start: PrecaptureId= %d",
            __FUNCTION__, mAePrecaptureId);
    }
}

NvError NvCameraHal3Core::process3AControls(
        NvCameraHal3_Public_Controls& prop)
{
    Mutex::Autolock al(mMetadataLock);
    NvError e = NvSuccess;

    handleAeTrigger(prop);
    handleAfTrigger(prop);

    if (prop.CoreCntrlProps.ControlMode == NvCamControlMode_UseSceneMode)
    {
        NV_CHECK_ERROR_CLEANUP(
            updateSceneModeControls(prop)
        );
    }

    return NvSuccess;
fail:
    NV_LOGE("%s:%d failed to process 3A controls! Error: 0x%x", __FUNCTION__,
        __LINE__, e);
    return e;
}

/* Mapping core defined AF states to google defined ones */
void NvCameraHal3Core::handleAfState(
    NvCameraHal3_Public_Dynamic& prop)
{
    NvCamProperty_Public_Dynamic& dprop = prop.CoreDynProps;

    if (mAfTriggerStart == NV_TRUE && dprop.Af.AfTriggerId == mAfTriggerId)
    {
        mAfTriggerAck = NV_TRUE;
    }

    dprop.Af.AfTriggerId = mAfTriggerId;
    mAfState = dprop.Af.AfState;
    if (dprop.Af.AfMode == NvCamAfMode_Auto || dprop.Af.AfMode == NvCamAfMode_Macro)
    {
        if (!mAfTriggerAck && (dprop.Af.AfState == NvCamAfState_FocusLocked ||
                                dprop.Af.AfState == NvCamAfState_NotFocusLocked))
        {
            mAfState = dprop.Af.AfState = NvCamAfState_Inactive;
        }
        else
        {
            NV_LOGV(HAL3_META_DATA_PROCESS_TAG, "AF FocusPosition %d\n", prop.CoreDynProps.LensFocusPos);
        }
    }

}

/*
 * Cached PrecaptureId is needed to communicate
 * AE state back to frameworks correctly.
 */
void NvCameraHal3Core::handleAePrecapture(
    NvCameraHal3_Public_Dynamic& prop)
{
    prop.AePrecaptureId = mAePrecaptureId;

    if(prop.CoreDynProps.Ae.AeState == NvCamAeState_Precapture)
    {
        mPrecaptureStart = NV_TRUE;
        NV_LOGV(HAL3_META_DATA_PROCESS_TAG, "%s: AE state: NvCamAeState_Precapture", __FUNCTION__);
    }
    else if (prop.CoreDynProps.Ae.AeState != NvCamAeState_Precapture &&
        mPrecaptureStart == NV_TRUE)
    {
        mAePrecaptureId = 0;
        mPrecaptureStart = NV_FALSE;
        NV_LOGV(HAL3_META_DATA_PROCESS_TAG, "%s: AE state changed: Precapture ended", __FUNCTION__);
    }
}

/*
 * Cache face rectangles, since this data is required
 * to drive 3As for FACE_PRIORITY scene mode
 */
void NvCameraHal3Core::cacheFaceStats(
    NvCameraHal3_Public_Dynamic& prop)
{
    mFaceRectangles = prop.CoreDynProps.FaceRectangles;
}

void NvCameraHal3Core::process3ADynamics(
        NvCameraHal3_Public_Dynamic& prop)
{
    Mutex::Autolock al(mMetadataLock);
    handleAePrecapture(prop);
    handleAfState(prop);
    cacheFaceStats(prop);
}

/*
 * Update 3A regions, if scene mode is FACE_PRIORITY
*/
void NvCameraHal3Core::update3ARegions(
        NvCamProperty_Public_Controls& cprop)
{
    if (cprop.SceneMode == NvCamSceneMode_FacePriority)
    {
        // use face rectangles from the most-recent-frame to
        // generate 3A regions
        if (mFaceRectangles.Size > 0)
        {
            //override 3Aregions
            cprop.AeRegions.numOfRegions = mFaceRectangles.Size;
            cprop.AfRegions.numOfRegions = mFaceRectangles.Size;
            cprop.AwbRegions.numOfRegions = mFaceRectangles.Size;

            for(int i=0; i<(int)mFaceRectangles.Size; i++)
            {
                cprop.AeRegions.regions[i] = mFaceRectangles.Rects[i];
                cprop.AfRegions.regions[i] = mFaceRectangles.Rects[i];
                cprop.AwbRegions.regions[i] = mFaceRectangles.Rects[i];

                // assign equal weights to all regions
                cprop.AeRegions.weights[i] = 1.f;
                cprop.AfRegions.weights[i] = 1.f;
                cprop.AwbRegions.weights[i] = 1.f;
            }
        }
        else
        {
            NV_LOGV(HAL3_META_DATA_PROCESS_TAG, "%s: No face detection stats to drive 3A regions",
                __FUNCTION__);
        }
    }
}

static void
updateSceneModeControls_Action(
        NvCamProperty_Public_Controls& cprop)
{
    cprop.NoiseReductionMode = NvCamNoiseReductionMode_Off;
    NVCAMHAL3_RANGE_MAKE(cprop.ExposureTimeRange, 0.0f, 5*1e6f);

    // AfMode is OFF: set the focus position to infinity.
    cprop.LensFocusDistance = 0.0f;
    cprop.EdgeEnhanceStrength = 0.8f;
}

static void
updateSceneModeControls_Portrait(
        NvCamProperty_Public_Controls& cprop)
{
    //TODO: to reduce focus search range and use FD as enabler.
}

static void
updateSceneModeControls_Landscape(
        NvCamProperty_Public_Controls& cprop)
{
    // TODO: to reduce FD frequency

    // TODO: set the focus position to hyperfocal
    // Bug 1428140: set to Infinity (AfMode = OFF) for now as
    // hyperfocal is not calibrated yet.
    cprop.LensFocusDistance = 0.0f;
}

static void
updateSceneModeControls_Night(
        NvCamProperty_Public_Controls& cprop)
{
    // TODO: to provide hint to set _AutoFlashRedEye when face is detected
    //       and to enable HDR.

    // TODO: set the focus position to hyperfocal
    // Bug 1428140: set to Infinity (AfMode = OFF) for now as
    // hyperfocal is not calibrated yet.
    cprop.LensFocusDistance = 0.0f;

    // Note: Noise reduction needs to be implemented
    // in core.
    cprop.NoiseReductionMode = NvCamNoiseReductionMode_HighQuality;

    NVCAMHAL3_RANGE_MAKE(cprop.ExposureTimeRange, 0.0f, 2e9f);
}

static void
updateSceneModeControls_NightPortrait(
        NvCamProperty_Public_Controls& cprop)
{
    // TODO: to reduce the focus range since subject is close.

    // Note: Noise reduction needs to be implemented
    // in core.
    cprop.NoiseReductionMode = NvCamNoiseReductionMode_HighQuality;

    NVCAMHAL3_RANGE_MAKE(cprop.ExposureTimeRange, 0.0f, 2e9f);
}

static void
updateSceneModeControls_Theatre(
        NvCamProperty_Public_Controls& cprop)
{
    // TODO: to enable HDR due to scene high constrast

    // set the focus position to Infinity
    cprop.LensFocusDistance = 0.0f;

    cprop.FlashMode = NvCamFlashMode_Off;
}

static void
updateSceneModeControls_Beach(
        NvCamProperty_Public_Controls& cprop)
{
    //ignore
}

static void
updateSceneModeControls_Snow(
        NvCamProperty_Public_Controls& cprop)
{
    //ignore
}

static void
updateSceneModeControls_Sunset(
        NvCamProperty_Public_Controls& cprop)
{
    cprop.FlashMode = NvCamFlashMode_Off;
    NVCAMHAL3_RANGE_MAKE(cprop.ExposureTimeRange, 0.0f, 2e9f);
}

static void
updateSceneModeControls_SteadyPhoto(
        NvCamProperty_Public_Controls& cprop)
{
    cprop.FlashMode = NvCamFlashMode_Off;
    NVCAMHAL3_RANGE_MAKE(cprop.ExposureTimeRange, 0.0f, 2e9f);
}

static void
updateSceneModeControls_Fireworks(
        NvCamProperty_Public_Controls& cprop)
{
    // TODO: to provide hint to monitor camera motion and to enable HDR

    // Bug 1428140: set to Infinity (AfMode = OFF) for now as
    // hyperfocal is not calibrated yet.
    cprop.LensFocusDistance = 0.0f;

    cprop.FlashMode = NvCamFlashMode_Off;
    NVCAMHAL3_RANGE_MAKE(cprop.ExposureTimeRange, 0.0f, 2e9f);
}

static void
updateSceneModeControls_Sports(
        NvCamProperty_Public_Controls& cprop)
{
    // TODO: how is this different from Action scene mode?

    // set the focus position to infinity
    cprop.LensFocusDistance = 0.0f;

    cprop.FlashMode = NvCamFlashMode_Off;
    cprop.NoiseReductionMode = NvCamNoiseReductionMode_Off;

    NVCAMHAL3_RANGE_MAKE(cprop.ExposureTimeRange, 0.0f, 2e9f);
}

static void
updateSceneModeControls_Party(
        NvCamProperty_Public_Controls& cprop)
{
    // TODO: to provide hint for low exposure since there is high motion

    NVCAMHAL3_RANGE_MAKE(cprop.ExposureTimeRange, 0.0f, 2e9f);
}

static void
updateSceneModeControls_Candlelight(
        NvCamProperty_Public_Controls& cprop)
{
    // TODO: to enable HDR

    cprop.FlashMode = NvCamFlashMode_Off;
    NVCAMHAL3_RANGE_MAKE(cprop.ExposureTimeRange, 0.0f, 2e9f);
}

static void
updateSceneModeControls_BarCode(
        NvCamProperty_Public_Controls& cprop)
{
    //TODO: use tone map to control contast

    cprop.FlashMode = NvCamFlashMode_Off;

    cprop.EdgeEnhanceMode = NvCamEdgeEnhanceMode_HighQuality;
    cprop.EdgeEnhanceStrength = 0.72;

    cprop.ContrastBias = 1;
}

static void
updateSceneModeControls_Default(
        NvCamProperty_Public_Controls& cprop)
{
    if (cprop.AeMode != NvCamAeMode_OnAutoFlash) {
        cprop.FlashMode = NvCamFlashMode_Off;
    }

    NVCAMHAL3_RANGE_MAKE(cprop.ExposureTimeRange, 0.0f, 1e9f);

    // For the moment define -1.f as a macro just like in core's nvcamera_isp.h
    NVCAMHAL3_RANGE_MAKE(cprop.GainRange, NVCAMERAISP_AUTO_FLOAT, NVCAMERAISP_AUTO_FLOAT);

    // TODO: Change this to Automode and add its corresponding support in core
    // For the moment define -1.f as a macro just like in core's nvcamera_isp.h
    cprop.EdgeEnhanceStrength = NVCAMERAISP_AUTO_FLOAT;
}

NvError
NvCameraHal3Core::updateSceneModeControls(
        NvCameraHal3_Public_Controls& prop)
{
    NvError e = NvSuccess;
    NvCamProperty_Public_Controls& cprop = prop.CoreCntrlProps;

    updateSceneModeControls_Default(cprop);

    switch (cprop.SceneMode)
    {
        case NvCamSceneMode_FacePriority:
            update3ARegions(cprop);
            break;
        case NvCamSceneMode_Action:
            updateSceneModeControls_Action(cprop);
            break;
        case NvCamSceneMode_Portrait:
            updateSceneModeControls_Portrait(cprop);
            break;
        case NvCamSceneMode_Landscape:
            updateSceneModeControls_Landscape(cprop);
            break;
        case NvCamSceneMode_Night:
            updateSceneModeControls_Night(cprop);
            break;
        case NvCamSceneMode_NightPortrait:
            updateSceneModeControls_NightPortrait(cprop);
            break;
        case NvCamSceneMode_Theatre:
            updateSceneModeControls_Theatre(cprop);
            break;
        case NvCamSceneMode_Beach:
            updateSceneModeControls_Beach(cprop);
            break;
        case NvCamSceneMode_Snow:
            updateSceneModeControls_Snow(cprop);
            break;
        case NvCamSceneMode_Sunset:
            updateSceneModeControls_Sunset(cprop);
            break;
        case NvCamSceneMode_SteadyPhoto:
            updateSceneModeControls_SteadyPhoto(cprop);
            break;
        case NvCamSceneMode_Fireworks:
            updateSceneModeControls_Fireworks(cprop);
            break;
        case NvCamSceneMode_Sports:
            updateSceneModeControls_Sports(cprop);
            break;
        case NvCamSceneMode_Party:
            updateSceneModeControls_Party(cprop);
            break;
        case NvCamSceneMode_CandleLight:
            updateSceneModeControls_Candlelight(cprop);
            break;
        case NvCamSceneMode_Barcode:
            updateSceneModeControls_BarCode(cprop);
            break;
        default:
            NV_LOGE("%s:%d: Error: Unrecognized scene mode\n", __FUNCTION__, __LINE__);
            e = NvError_BadParameter;
            break;
    }
    return e;
}

} // namespace android
