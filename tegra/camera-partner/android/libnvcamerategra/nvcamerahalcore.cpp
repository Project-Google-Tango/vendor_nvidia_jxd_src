/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

// holds core functionality that many API functions will reference

#define LOG_TAG "NvCameraHalCore"

#define LOG_NDEBUG 0

#include "nvcamerahal.h"

namespace android {

typedef struct ResolutionRec {
    int width;
    int height;
} Resolution;

static const Resolution stdStillRes[] = {
           { 320, 240 },
           { 480, 480 },
           { 640, 480 },
           { 640, 368 },
           { 800, 600 },
           { 960, 720 },
           { 1024, 768 },
           { 1280, 720 },
           { 1280, 752 },
           { 1280, 960 },
           { 1440, 1080 },
           { 1600, 1200 },
           { 1836, 1080 },
           { 1920, 1080 },
           { 2048, 1152 },
           { 2048, 1360 },
           { 2048, 1536 },
           { 2592, 1456 },
           { 2592, 1520 },
           { 2592, 1920 },
           { 2592, 1944 },
           { 2592, 1952 },
           { 3264, 1840 },
           { 3264, 2448 },
           { 4096, 3072 },
           { 4208, 3120 },
           { -1, -1 }
       };

/*
 * Please coordinate the change of the list of the supported preview
 * sizes and video sizes with the change of the preferred preview size,
 * to avoid video recording performance issue or incorrect preview
 * aspect size issue.
 */
static const Resolution stdVideoRes[] = {
           { 176, 144 },
           { 320, 240 },
           { 352, 288 },
           { 640, 480 },
           { 704, 576 },
           { 720, 480 },
           { 720, 576 },
           { 768, 432 },
           { 960, 540 },
           { 1280, 720 },
           { 1920, 1080 },
           { 1920, 1088 },
           { -1, -1 }
       };

static const Resolution stdPreviewRes[] = {
           { 176, 144 },
           { 320, 240 },
           { 352, 288 },
           { 480, 480 },
           { 640, 480 },
           { 704, 576 },
           { 720, 408 },
           { 720, 480 },
           { 720, 576 },
           { 768, 432 },
           { 800, 448 },
           { 960, 720 },
           { 1024, 768 },
           { 1280, 720 },
           { 1280, 752 },
           { 1280, 960 },
           { 1360, 720 },
           { 1440, 1080 },
           { 1920, 1080 },
           { 1920, 1088 },
           { 1920, 1440 },
           { 2104, 1560 },
           { -1, -1 }
       };

static void UpdateSettingsParserFocusModes(
    const NvCameraIspFocusingParameters& fParams,
    NvCombinedCameraSettings& newSettings,
    NvCameraCapabilities& parseCaps)
{
    if (fParams.minPos == fParams.maxPos ||
            fParams.sensorispAFsupport == NV_FALSE)
    {
        ALOGVV("%s: Focus not supported by this sensor, setting supported "\
            "focus modes to Fixed",
            __FUNCTION__);
        parseCaps.supportedFocusModes.add(NvCameraFocusMode_Fixed);
        // update the default, since it's restricted to fixed focus now
        newSettings.focusMode = NvCameraFocusMode_Fixed;
        newSettings.isDirtyForNvMM.focusMode = NV_TRUE;
        newSettings.isDirtyForParser.focusMode = NV_TRUE;
    }
    else
    {
        ALOGVV("%s: Focus supported by this sensor, setting supported "\
            "focus modes to all options",
            __FUNCTION__);
        parseCaps.supportedFocusModes.add(NvCameraFocusMode_Auto);
        parseCaps.supportedFocusModes.add(NvCameraFocusMode_Infinity);
        parseCaps.supportedFocusModes.add(NvCameraFocusMode_Macro);
        parseCaps.supportedFocusModes.add(NvCameraFocusMode_Fixed);
        parseCaps.supportedFocusModes.add(NvCameraFocusMode_Continuous_Video);
        parseCaps.supportedFocusModes.add(NvCameraFocusMode_Continuous_Picture);
    }
}

static void UpdateSettingsParserFlashModes(
    NvBool hasFlash,
    NvCombinedCameraSettings& newSettings,
    NvCameraCapabilities& parseCaps)
{
    if(hasFlash)
    {
        ALOGVV("%s: Flash supported by this sensor, setting supported "\
            "flash modes to all options",
            __FUNCTION__);
        parseCaps.supportedFlashModes.add(NvCameraFlashMode_Off);
        parseCaps.supportedFlashModes.add(NvCameraFlashMode_On);
        parseCaps.supportedFlashModes.add(NvCameraFlashMode_Auto);
        parseCaps.supportedFlashModes.add(NvCameraFlashMode_Torch);
        parseCaps.supportedFlashModes.add(NvCameraFlashMode_RedEyeReduction);
    }
    else
    {
        ALOGVV("%s: Flash not supported by this sensor, setting supported "\
            "flash modes to Off",
            __FUNCTION__);
        parseCaps.supportedFlashModes.add(NvCameraFlashMode_Off);
        newSettings.flashMode = NvCameraFlashMode_Off;
        newSettings.isDirtyForNvMM.flashMode = NV_TRUE;
        newSettings.isDirtyForParser.flashMode = NV_TRUE;
    }
}

static void UpdateSettingsParserStereoModes(
    NvBool stereoSupported,
    NvCombinedCameraSettings& newSettings,
    NvCameraCapabilities& parseCaps)
{
    if(stereoSupported)
    {
        parseCaps.supportedStereoModes.add(NvCameraStereoMode_LeftOnly);
        parseCaps.supportedStereoModes.add(NvCameraStereoMode_RightOnly);
        parseCaps.supportedStereoModes.add(NvCameraStereoMode_Stereo);
    }
    else
    {
        ALOGVV("%s: No stereo mode supported", __FUNCTION__);
        parseCaps.supportedStereoModes.add(NvCameraStereoMode_LeftOnly);
        newSettings.stereoMode = NvCameraStereoMode_LeftOnly;
        newSettings.isDirtyForNvMM.stereoMode = NV_TRUE;
        newSettings.isDirtyForParser.stereoMode = NV_TRUE;
    }
}

// Acquires lock and attempts to acquire var, using
// cond as the condition variable. Returns true if lock is successful.
// passing in a NULL owner thread ID pointer will take the lock, but skip all
// owner updating / waiting



NvBool NvCameraHal::Lock(Mutex *lock, android_thread_id_t *var, Condition *cond)
{
    if (!lock || !cond)
    {
        ALOGVV("%s-- (lock or condition variable doesn't exist)", __FUNCTION__);
        return NV_FALSE;
    }

    // Acquire lock. This will block until lock is unlocked.
    lock->lock();

    if (!var)
    {
        ALOGVV("%s-- (tid pointer is NULL, not setting owner)", __FUNCTION__);
        return NV_TRUE;
    }

    // Wait until no thread owns var. cond->wait will unlock 'lock'
    // until broadcast.
    while(*var != 0)
    {
        cond->wait(*lock);
    }

    // Set var to this thread's ID to indicate it owns it.
    *var = androidGetThreadId();

    return NV_TRUE;
}

// Releases lock. If we own var, release it, broadcast cond.
// passing a NULL owner ID thread pointer will still unlock, but it will skip
// broadcasting and updating the owner
NvBool NvCameraHal::Unlock(Mutex *lock, android_thread_id_t *var, Condition *cond)
{
    if (!lock)
    {
        ALOGVV("%s-- (lock doesn't exist)", __FUNCTION__);
        return NV_FALSE;
    }

    // Check that we actually own the variable.
    // If not, just unlock.
    if (var && cond && *var == androidGetThreadId())
    {
        *var = 0;
        cond->broadcast();
    }

    lock->unlock();

    return NV_TRUE;
}

NvError NvCameraHal::SendCapabilitiesToParser(NvCombinedCameraSettings &defaultSettings)
{
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    GetSensorCapabilities(mDefaultCapabilities, defaultSettings);
    mSettingsParser.setCapabilities(mDefaultCapabilities);

    ALOGVV("%s--", __FUNCTION__);
    return e;
}

void NvCameraHal::PowerOnCamera()
{
    ALOGVV("%s++", __FUNCTION__);
    NvBool cameraPowerOn = NV_TRUE;

    Cam.Block->SetAttribute(
              Cam.Block,
              NvMMCameraAttribute_SensorPowerOn,
              0,
              sizeof(cameraPowerOn),
              &(cameraPowerOn));

    ALOGVV("%s--", __FUNCTION__);
}

// This thread is to wait until buffer update is completed, and then
// start recording.
void NvCameraHal::StartRecordingAfterVideoBufferUpdate(void *args)
{
    NvError e = NvSuccess;
    NvCameraHal *pThis = (NvCameraHal *)args;

    AutoMutex lock(pThis->mLock);
    NV_CHECK_ERROR_CLEANUP(
       pThis->CheckAndWaitForCondition(!(pThis->mDelayedSettingsComplete),
           pThis->mDelayedSettingsDone)
    );

    NV_CHECK_ERROR_CLEANUP(
       pThis->StartRecordingStream()
    );
    return;

fail:
    ALOGE("%s:%d failed. Error: 0x%x", __FUNCTION__, __LINE__, e);
}

void NvCameraHal::DoStuffThatNeedsPreview(void *args)
{
    NvCameraHal *pThis = (NvCameraHal *)args;

    ALOGVV("%s++", __FUNCTION__);
    APILock lock(pThis);

    // For LEAN video perf scheme, we don't update buffer here
    NvCameraBufferConfigStage stage =
        pThis->m_pMemProfileConfigurator->GetBufferConfigStage();
    const NvCombinedCameraSettings &Settings =
        pThis->mSettingsParser.getCurrentSettings();

    if (stage != NVCAMERA_BUFFERCONFIG_STAGE_LEAN_VIDEO &&
        stage != NVCAMERA_BUFFERCONFIG_STAGE_LEAN_STILL)
    {
        // Apply buffer changes
        pThis->m_pMemProfileConfigurator->SetBufferConfigStage(
            NVCAMERA_BUFFERCONFIG_STAGE_AFTER_FIRST_PREVIEW);
        pThis->BufferManagerUpdateNumberOfBuffers();
    }
    // App will calls startPreview after taking Picture. We
    //  are relying on this to release the still buffers
    else if (stage == NVCAMERA_BUFFERCONFIG_STAGE_LEAN_STILL)
    {
        // When in NSL or vstab mode, we are controlling buffer
        //  numbers through SetNSLNumBuffers instead of here
        if (!Settings.nslNumBuffers && !Settings.videoStabOn)
            pThis->ReleaseLeanModeStillBuffer();
    }

    if (pThis->mDelayJpegEncoderOpen)
    {
        pThis->OpenJpegEncoder();
        pThis->mDelayJpegEncoderOpen = NV_FALSE;
    }
    if (pThis->mDelayJpegStillEncoderSetup)
    {
        pThis->SetupJpegEncoderStill();
        pThis->mDelayJpegStillEncoderSetup = NV_FALSE;
    }
    if (pThis->mDelayJpegThumbnailEncoderSetup)
    {
        pThis->SetupJpegEncoderThumbnail();
        pThis->mDelayJpegThumbnailEncoderSetup = NV_FALSE;
    }

    pThis->mDelayedSettingsComplete = NV_TRUE;
    pThis->mDelayedSettingsDone.broadcast();

    ALOGVV("%s--", __FUNCTION__);
}

NvError NvCameraHal::WaitForStuffThatNeedsPreview()
{
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    if (!mPreviewStarted)
    {
        ALOGVV("%s skipping wait for preview as it's not started yet.", __FUNCTION__);
        return e;
    }

    NV_CHECK_ERROR_CLEANUP(
        CheckAndWaitForCondition(!mPreviewStreaming && !mPreviewPaused, mFirstPreviewFrameReceived)
    );

    if (mThreadToDoStuffThatNeedsPreview)
    {
        NV_CHECK_ERROR_CLEANUP(
            CheckAndWaitForCondition(!mDelayedSettingsComplete, mDelayedSettingsDone)
        );
        NvOsThreadJoin(mThreadToDoStuffThatNeedsPreview);
        mThreadToDoStuffThatNeedsPreview = NULL;
    }

    ALOGVV("%s--", __FUNCTION__);

    return e;

fail:
    ALOGE("%s failed!", __FUNCTION__);
    return e;
}

void NvCameraHal::SetupSensorListenerThread(void *args)
{
    NvCameraHal *pThis = (NvCameraHal *)args;
    pThis->SetupSensorListener();
}

NvError NvCameraHal::StartPreviewInternal()
{
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);
    // nothing to do here if:
    //   - preview is already started
    if (mPreviewStarted || mPreviewStreaming || !mPreviewWindow)
    {
        ALOGVV("%s-- (preview already started)", __FUNCTION__);
        return e;
    }

#if ENABLE_NVIDIA_CAMTRACE
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_HAL_START_PREVIEW_ENTRY,
            NvOsGetTimeUS(),
            3);
#endif
    if (!mPreviewPaused)
    {
        // allocate buffers from preview window and send to DZ's preview output
        NV_CHECK_ERROR_CLEANUP(
            BufferManagerSupplyPreviewBuffers()
        );
    }

    // send start command to NvMM
    NV_CHECK_ERROR_CLEANUP(
        StartPreviewStream()
    );

#if ENABLE_NVIDIA_CAMTRACE
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_HAL_START_PREVIEW_EXIT,
            NvOsGetTimeUS(),
            3);
#endif

    mPreviewPaused = NV_FALSE;
    mPreviewStarted = NV_TRUE;

    ALOGVV("%s--", __FUNCTION__);

    return e;
fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::StopPreviewInternal()
{
    NvError e = NvSuccess;
    NvBool PreviewWasStarted = mPreviewStarted;

    ALOGVV("%s++", __FUNCTION__);
    // nothing to do here if:
    //   - preview was already stopped

    if ((!mPreviewStarted || !mPreviewWindow) && !mPreviewPaused)
    {
        ALOGVV("%s-- (preview already stopped)", __FUNCTION__);
        return e;
    }

#if ENABLE_NVIDIA_CAMTRACE
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_HAL_SWITCH_ENTRY,
            NvOsGetTimeUS(),
            3);
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_HAL_STOP_PREVIEW_ENTRY,
            NvOsGetTimeUS(),
            3);
#endif

    // If the preview is started but we haven't gotten the first frame back yet
    // we need to wait until the first frame arrives. Otherwise we won't return
    // empty buffers to DZ which will cause it to not send us the EOS event when
    // we tell it to stop.
    NV_CHECK_ERROR_CLEANUP(
        CheckAndWaitForCondition(
            !mPreviewStreaming && !mPreviewPaused, mFirstPreviewFrameReceived)
    );

    mPreviewStarted = NV_FALSE;

    // TODO stop smooth zoom?

    if (PreviewWasStarted || mPreviewPaused)
    {
        NV_CHECK_ERROR_CLEANUP(
            StopPreviewStream()
        );
    }
    mPreviewPaused = NV_FALSE;

    NV_CHECK_ERROR_CLEANUP(
        FlushPreviewBuffers()
    );

#if ENABLE_NVIDIA_CAMTRACE
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_HAL_STOP_PREVIEW_EXIT,
            NvOsGetTimeUS() ,
            3);
#endif

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::FlushPreviewBuffers()
{
    NvError e = NvSuccess;
    NvU32 BufferIndex = 0;

    ALOGVV("%s++", __FUNCTION__);
    NvCameraBufferConfigStage stage =
        m_pMemProfileConfigurator->GetBufferConfigStage();
    NvU32 previewBufferNum;
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            stage, NVCAMERA_BUFFERCONFIG_DZ_PREVIEW,
            NULL, &previewBufferNum)
    );

    NV_CHECK_ERROR_CLEANUP(
      BufferManagerReclaimPreviewBuffers()
    );

    // Once reclaimed cancel all ANR buffers
    for (BufferIndex = 0; BufferIndex < previewBufferNum; BufferIndex++)
    {
        if (mPreviewWindowDestroyed == NV_FALSE)
        {
            int err = mPreviewWindow->cancel_buffer(
                                                mPreviewWindow,
                                                mPreviewBuffers[BufferIndex].anb);
            if (err != 0)
            {
                ALOGE("%s: cancel_buffer failed [%d]", __FUNCTION__, err);
            }

            mPreviewBuffers[BufferIndex].anb = NULL;
        }
        else
        {
            ALOGE("%s: Ignore Preview Buffer since window is already destroyed\n",
                   __FUNCTION__);
            mPreviewBuffers[BufferIndex].anb = NULL;
        }
    }

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::StartRecordingInternal()
{
    NvError e = NvSuccess;
    NvCombinedCameraSettings settings = mSettingsParser.getCurrentSettings();

    ALOGVV("%s++", __FUNCTION__);

    // nothing to do here if recording already started
    if (mVideoStarted)
    {
        ALOGVV("%s-- (recording already started)", __FUNCTION__);
        return e;
    }

    SetPreviewPauseAfterStillCapture(
        settings,
        NV_FALSE);
    mSettingsParser.updateSettings(settings);

    // Start recording only if video buffer update is completed.
    if (mDelayedSettingsComplete == NV_TRUE) {
        NV_CHECK_ERROR_CLEANUP(
            StartRecordingStream()
        );
    }
    else {
        // This is for the scenario where HAL receives startRecording
        // call before preview start streaming, i.e., before number of video
        // buffers are updated.
        // Start the recording after number of video buffers are updated, which
        // happens in DoStuffThatNeedsPreview, until then wait in the thread,
        // and start recording after that.
        NvOsThreadCreate(StartRecordingAfterVideoBufferUpdate, this,
            &mThreadToStartRecordingAfterVideoBufferUpdate);
    }
    mFrameRateChanged = NV_TRUE;

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::StopRecordingInternal()
{
    NvError e = NvSuccess;
    NvCombinedCameraSettings settings = mSettingsParser.getCurrentSettings();

    ALOGVV("%s++", __FUNCTION__);

    // nothing to do here if recording already stopped
    if (!mVideoStarted)
    {
        ALOGVV("%s-- (recording already stopped)", __FUNCTION__);
        return e;
    }

    mNumOfVideoFramesDelivered = 0;

    NV_CHECK_ERROR_CLEANUP(
        StopRecordingStream()
    );

    SetPreviewPauseAfterStillCapture(
        settings,
        NV_TRUE);

    mSettingsParser.updateSettings(settings);

    mVideoStarted = NV_FALSE;
    mFrameRateChanged = NV_FALSE;

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::StartRecordingStream()
{
    NvError e = NvSuccess;
    NvU32 allocatedNumBuffers = 0;
    const NvCombinedCameraSettings &Settings =
        mSettingsParser.getCurrentSettings();

    ALOGVV("%s++", __FUNCTION__);

    // camera block depends on this hint, make sure it's set to the right
    // value
    NvMMCameraCaptureMode CaptureMode = NvMMCameraCaptureMode_Video;
    NvMMCameraCaptureVideoRequest Request;

    // Turn off NSL before starting recording
    if (Settings.nslNumBuffers)
    {
        SetNSLNumBuffers(0, &allocatedNumBuffers);
    }

    // When in nsl or vstab mode, buffers are set through
    // SetNSLNumBuffers or SetVSTABNumBuffers
    if (!Settings.nslNumBuffers && !Settings.videoStabOn)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetupLeanModeVideoBuffer()
        );
    }

    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvMMCameraAttribute_CaptureMode,
            0,
            sizeof(NvMMCameraCaptureMode),
            &CaptureMode)
    );

    NvOsMemset(&Request, 0, sizeof(Request));
    Request.State = NvMMCameraPinState_Enable;
    Request.Resolution.width = Settings.videoResolution.width;
    Request.Resolution.height = Settings.videoResolution.height;

    {
        UNLOCK_EVENTS();
        e = Cam.Block->Extension(
                Cam.Block,
                NvMMCameraExtension_CaptureVideo,
                sizeof(NvMMCameraCaptureVideoRequest),
                &Request,
                0,
                NULL);
        RELOCK_EVENTS();
        if (e != NvSuccess)
        {
            goto fail;
        }
    }
    mVideoStarted = NV_TRUE;

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::StopRecordingStream()
{
    NvError e = NvSuccess;
    NvU32 requestedNSLNumBuffers = 0;
    NvU32 allocatedNSLNumBuffers = 0;

    ALOGVV("%s++", __FUNCTION__);
    // camera block depends on this hint, make sure we switch it back when
    // video recording is done
    NvMMCameraCaptureMode CaptureMode = NvMMCameraCaptureMode_StillCapture;
    NvMMCameraCaptureVideoRequest Request;
    NvCombinedCameraSettings Settings;

    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvMMCameraAttribute_CaptureMode,
            0,
            sizeof(NvMMCameraCaptureMode),
            &CaptureMode)
    );

    Settings = mSettingsParser.getCurrentSettings();

    NvOsMemset(&Request, 0, sizeof(Request));
    Request.State = NvMMCameraPinState_Disable;
    // yes, it looks like NvMM wants us to set this, even when we're *stopping*
    // capture.  it's kind of silly.
    Request.Resolution.width = Settings.videoResolution.width;
    Request.Resolution.height = Settings.videoResolution.height;

    {
        UNLOCK_EVENTS();
        e = Cam.Block->Extension(
                Cam.Block,
                NvMMCameraExtension_CaptureVideo,
                sizeof(NvMMCameraCaptureVideoRequest),
                &Request,
                0,
                NULL);
        RELOCK_EVENTS();
        if (e != NvSuccess)
        {
            goto fail;
        }
    }

    // need to wait for video EOS before flush preview buffers
    // or else video buffer will be stuck in pipeline
    NV_CHECK_ERROR_CLEANUP(
        CheckAndWaitForCondition(mVideoStreaming, mVideoEOSReceived)
    );

    // reset nsl
    if (Settings.nslNumBuffers)
    {
        requestedNSLNumBuffers = Settings.nslNumBuffers;
        SetNSLNumBuffers(requestedNSLNumBuffers, &allocatedNSLNumBuffers);
        Settings.nslNumBuffers = allocatedNSLNumBuffers;
        if (requestedNSLNumBuffers > allocatedNSLNumBuffers)
        {
            ALOGWW("%s requests %d NSL buffers, and get %d instead",
                __func__,
                requestedNSLNumBuffers,
                Settings.nslNumBuffers);
        }
            Settings.isDirtyForParser.nslNumBuffers = NV_TRUE;
            mSettingsParser.updateSettings(Settings);
    }

    // When in nsl or vstab mode, buffers are set up & released through
    // SetNSLNumBuffers or SetVSTABNumBuffers
    if (!Settings.nslNumBuffers && !Settings.videoStabOn)
    {
        NV_CHECK_ERROR_CLEANUP(
            ReleaseLeanModeVideoBuffer()
        );
    }

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvBool NvCameraHal::isUSB()
{
    int fd;

    if (getCamType(mSensorId) != NVCAMERAHAL_CAMERATYPE_USB)
        return NV_FALSE;

    /* Try opening first device */
    fd = open ("/dev/video0", O_RDWR | O_NONBLOCK, 0);
    if (fd < 0)
    {
       /* First device opening failed so  trying with the next device */
       fd = open ("/dev/video1", O_RDWR | O_NONBLOCK, 0);
       if (fd < 0)
       {
          return NV_FALSE;
       }
    }
    close(fd);

    return NV_TRUE;
}

NvError NvCameraHal::StartPreviewStream()
{
    NvError e = NvSuccess;
    NvBool bEnablePreviewOut = NV_TRUE;
    NvMMCameraPinState State = NvMMCameraPinState_Enable;

    ALOGVV("%s++", __FUNCTION__);

    // This needs to unlock the API since it will receive a set state event
    // and if that event is not serviced this will not exit

    UNLOCK_EVENTS();
    Cam.Block->Extension(
        Cam.Block,
        NvMMCameraExtension_EnablePreviewOut,
        sizeof(NvBool),
        &bEnablePreviewOut,
        0,
        NULL);
    RELOCK_EVENTS();
    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvMMCameraAttribute_Preview,
            0,
            sizeof(NvMMCameraPinState),
            &State)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;
fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::StopPreviewStream()
{
    NvError e = NvSuccess;
    NvBool bEnablePreviewOut = NV_FALSE;
    NvMMCameraPinState State = NvMMCameraPinState_Disable;
    status_t err = NO_ERROR;

    ALOGVV("%s++", __FUNCTION__);

    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvMMCameraAttribute_Preview,
            0,
            sizeof(NvMMCameraPinState),
            &State)
    );

    // need to wait if preview is started
    NV_CHECK_ERROR_CLEANUP(
        WaitForCondition(mPreviewEOSReceived)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;
fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

void NvCameraHal::SetupSensorListener()
{
    ALOGVV("%s++", __FUNCTION__);

    mSensorListener = new NvSensorListener();
    if (mSensorListener.get())
    {
        if (mSensorListener->initialize() == NO_ERROR)
        {
            // start listening sensor events.
            mSensorListener->setCallbacks(&orientation_cb, this);
            mSensorListener->enableSensor(NvSensorListener::SENSOR_ORIENTATION);
        }
        else
        {
            ALOGE("Fail to initialize SensorListener\n");
            mSensorListener.clear();
            mSensorListener = NULL;
        }
    }
    else
        ALOGE("%s failed to get SensorListener", __FUNCTION__);

    ALOGVV("%s--", __FUNCTION__);
}

void NvCameraHal::CleanupSensorListener()
{
    ALOGVV("%s++", __FUNCTION__);

    if (mSensorListener.get())
    {
        // stop listening sensor events.
        mSensorListener->disableSensor(NvSensorListener::SENSOR_ORIENTATION);
        mSensorListener.clear();
        mSensorListener = NULL;
    }

    ALOGVV("%s--", __FUNCTION__);
}

void NvCameraHal::StartFaceDetectorThread(void *args)
{
    NvError e = NvSuccess;
    NvCameraHal *This = (NvCameraHal *)args;
    NvS32 NumFaces = 0;
    NvS32 Debug;
    char Value[PROPERTY_VALUE_MAX];

    ALOGVV("%s++", __FUNCTION__);

    property_get("camera.debug.fd.test", Value, "0");
    Debug = atoi(Value);

    property_get("camera.debug.fd.numTargetFaces", Value, DEFAULT_FACES_DETECTED);
    NumFaces = atoi(Value);
    if (NumFaces > MAX_FACES_DETECTED)
        NumFaces = MAX_FACES_DETECTED;
    else if (NumFaces < 0)
        NumFaces = 0;

    e = This->StartFaceDetection(NumFaces, Debug);
    if (e != NvSuccess)
    {
        ALOGE("failed starting delayed face detector!\n");
    }
    else
    {
        ALOGVV("face detector is requested to be started.\n");
    }

    ALOGVV("%s--", __FUNCTION__);
}

void NvCameraHal::StopFaceDetectorThread(void *args)
{
    NvError e = NvSuccess;
    NvCameraHal *This = (NvCameraHal *)args;

    ALOGVV("%s++", __FUNCTION__);

    e = This->StartFaceDetection(0, 0);
    if (e != NvSuccess)
    {
        ALOGE("%s failed starting delayed face detector!", __FUNCTION__);
    }
    else
    {
        ALOGVV("face detector is requested to be stopped.");
    }

    ALOGVV("%s--", __FUNCTION__);
}

NvError NvCameraHal::StartFaceDetection(NvS32 NumFaces, NvS32 Debug)
{
    NvError e = NvSuccess;
    NvS32 corrected;
    NvMMCameraStartFaceDetect StartFd;

    ALOGVV("%s++", __FUNCTION__);

    StartFd.NumFaces = NumFaces;
    StartFd.Debug    = Debug;

    // Unlocking and relocking events does not
    // seem to work well here if it's done in
    // a separate thread
    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->Extension(
            Cam.Block,
            NvMMCameraExtension_TrackFace,
            sizeof(NvMMCameraStartFaceDetect),
            &StartFd,
            0,
            NULL)
    );

    if (NumFaces != 0)
    {
        // program latest orientation
        if (mSensorId == CAMERA_FACING_FRONT)
        {
            corrected = (mSensorOrientation - mOrientation + 360) % 360;
        }
        else
        {
            corrected = (mSensorOrientation + mOrientation) % 360;
        }

        NV_CHECK_ERROR_CLEANUP(
            ProgramDeviceRotation(corrected)
        );

        NV_CHECK_ERROR_CLEANUP(
            SetFdState(FaceDetectionState_Running, NV_TRUE)
        );
    }
    else
    {
        NV_CHECK_ERROR_CLEANUP(
            SetFdState(FaceDetectionState_Idle, NV_TRUE)
        );
    }

    ALOGVV("%s--", __FUNCTION__);

    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::StopFaceDetection()
{
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    // stop face detector thread if
    // it is still running
    NV_CHECK_ERROR_CLEANUP(
        SetFdState(FaceDetectionState_Stop, NV_TRUE)
    );

    ALOGVV("%s--", __FUNCTION__);

    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::SetFdState(FaceDetectionState NewState, NvBool Immediate)
{
    NvError e = NvSuccess;
    FaceDetectionState PrevReq;

    ALOGVV("%s++", __FUNCTION__);

    mLockFdState.lock();
    PrevReq = mFdReq;
    mFdReq = NewState;
    mLockFdState.unlock();
    if (Immediate)
    {
        // Join a related thread if it is running.
        if (NewState == FaceDetectionState_Stop &&
            mThreadToHandleFaceDetector != NULL)
        {
            NvOsThreadJoin(mThreadToHandleFaceDetector);
            mThreadToHandleFaceDetector = NULL;
        }

        // Run state machine.
        NV_CHECK_ERROR_CLEANUP(
            RunFdStateMachine()
        );

        mLockFdState.lock();
        if (NewState == FaceDetectionState_Pause ||
            NewState == FaceDetectionState_Resume)
        {
            mFdReq = PrevReq;
        }
        mLockFdState.unlock();
    }

    // Otherwise, state machine is executed when next preview frame arrives.
    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::RunFdStateMachine()
{
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    mLockFdState.lock();
    switch (mFdReq)
    {
        case FaceDetectionState_Start:
            if (mFdState != FaceDetectionState_Idle)
            {
                break;
            }

            NV_CHECK_ERROR_CLEANUP(
                NvOsThreadCreate(StartFaceDetectorThread,
                                 this,
                                 &mThreadToHandleFaceDetector)
            );

            mFdState = FaceDetectionState_Starting;
            break;

        case FaceDetectionState_Running:
            if (mFdState == FaceDetectionState_Starting)
            {
                mFdState = mFdReq;
            }
            break;

        case FaceDetectionState_Stop:
            if (mFdState == FaceDetectionState_Running)
            {
                // need to stop face detection
                NV_CHECK_ERROR_CLEANUP(
                    NvOsThreadCreate(StopFaceDetectorThread,
                                     this,
                                     &mThreadToHandleFaceDetector)
                );

                mFdState = FaceDetectionState_Stopping;
            }
            break;

        case FaceDetectionState_Pause:
            if (!mFdPaused)
                mFdPaused = NV_TRUE;
            break;

        case FaceDetectionState_Resume:
            if (mFdPaused)
                mFdPaused = NV_FALSE;
            break;

        case FaceDetectionState_Idle:
            mFdState = mFdReq;
            break;

        case FaceDetectionState_NoRequest:
            break;

        default:
            ALOGVV("%s: 0x%X is unknown.", __FUNCTION__, mFdReq);
            break;
    }

    // clear a request
    mFdReq = FaceDetectionState_NoRequest;
    mLockFdState.unlock();

    ALOGVV("%s--", __FUNCTION__);

    return e;

fail:
    mLockFdState.unlock();
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::EncodePreviewMetaDataForFaces(Face *pFace, NvU32 numFaceDetected)
{
    NvError e = NvSuccess;
    NvU32 i, j;

    ALOGVV("%s++", __FUNCTION__);

#if NV_FACE_VERBOSE_LOGGING
    ALOGVV("%s: Num of face Detected: %d", __FUNCTION__, (int)numFaceDetected);
#endif
    if (numFaceDetected > MAX_FACES_DETECTED)
    {
        ALOGE("%s: number of detected face is too large: %d\n",__FUNCTION__, (int)numFaceDetected);
        return NvError_BadParameter;
    }
    // Clear the face detection data
    mFaceMetaData->number_of_faces = 0;
    if (mFaceMetaData->faces)
    {
        NvOsMemset(mFaceMetaData->faces, 0, sizeof(camera_face_t)*MAX_FACES_DETECTED);
    }
    else
    {
        ALOGE("%s: mFaceMetaData->faces pointer is invalid\n",__FUNCTION__);
        return NvError_BadParameter;
    }

    // Encode the face info into the face meta data
    mFaceMetaData->number_of_faces = numFaceDetected;
    for (i = 0; i < numFaceDetected; i++)
    {
        mFaceMetaData->faces[i].id = pFace->id;
        // FIXME: android range: 1--100, our range 0--100, also need add range check
        mFaceMetaData->faces[i].score = pFace->confidence;
        if (mFaceMetaData->faces[i].score == 0)
            mFaceMetaData->faces[i].score = 1;

        mFaceMetaData->faces[i].rect[0] = (int32_t)pFace->location.x;
        mFaceMetaData->faces[i].rect[1] = (int32_t)pFace->location.y;
        mFaceMetaData->faces[i].rect[2] = (int32_t)(pFace->location.x + pFace->location.width);
        mFaceMetaData->faces[i].rect[3] = (int32_t)(pFace->location.y + pFace->location.height);

        // Eyes/mouth locations are not supported
        mFaceMetaData->faces[i].left_eye[0] = FACE_UNSUPPORTED_COORD;
        mFaceMetaData->faces[i].left_eye[1] = FACE_UNSUPPORTED_COORD;
        mFaceMetaData->faces[i].right_eye[0] = FACE_UNSUPPORTED_COORD;
        mFaceMetaData->faces[i].right_eye[1] = FACE_UNSUPPORTED_COORD;
        mFaceMetaData->faces[i].mouth[0] = FACE_UNSUPPORTED_COORD;
        mFaceMetaData->faces[i].mouth[1] = FACE_UNSUPPORTED_COORD;
#if NV_FACE_VERBOSE_LOGGING
        dumpFaceInfo(&(mFaceMetaData->faces[i]), i);
#endif
        pFace++;
    }

    ALOGVV("%s--", __FUNCTION__);

    return e;
}

NvError NvCameraHal::Program3AForFaces(Face *pFaces, NvU32 numFaceDetected)
{
    NvError e = NvSuccess;
    NvU32 idx = 0;
    NvCameraUserWindow regions[MAX_FACES_DETECTED];
    NvBool ProgramRegions = NV_FALSE;
    NvCombinedCameraSettings Settings =
                mSettingsParser.getCurrentSettings();
    NvBool focusSupported = mSettingsParser.isFocusSupported();

    NvOsMemset(regions, 0, sizeof(NvCameraUserWindow)*MAX_FACES_DETECTED);

    if (numFaceDetected > 0)
    {
        mNeedToReset3AWindows = NV_TRUE;
        for (idx = 0; idx < numFaceDetected; ++idx)
        {
            regions[idx].top = pFaces[idx].location.y;
            regions[idx].left = pFaces[idx].location.x;
            regions[idx].right
                = pFaces[idx].location.x + pFaces[idx].location.width;
            regions[idx].bottom
                = pFaces[idx].location.y + pFaces[idx].location.height;
            regions[idx].weight = 1.f;

            if (abs(regions[idx].top - mLastFaceRegions[idx].top) > MIN_DISTANCE_TO_UPDATE)
            {
                ProgramRegions = NV_TRUE;
            }
        }

        if (ProgramRegions)
        {
            if (focusSupported)
            {
                NV_CHECK_ERROR_CLEANUP(
                    SetFocusWindowsForFaces(&regions[0], numFaceDetected)
                );
            }
            NV_CHECK_ERROR_CLEANUP(
                SetExposureWindowsForFaces(&regions[0], numFaceDetected)
            );
            NvOsMemcpy(&mLastFaceRegions[0],
                       &regions[0],
                       MAX_FACES_DETECTED * sizeof(NvCameraUserWindow));
        }
    }
    else if (mNeedToReset3AWindows)
    {
        mNeedToReset3AWindows = NV_FALSE;

        // restore AE, AF and WB
        if (focusSupported)
        {
            NV_CHECK_ERROR_CLEANUP(
                SetFocusWindows(Settings)
            );
        }
        NV_CHECK_ERROR_CLEANUP(
            SetExposureWindows(Settings)
        );

        if (mWBModeCached)
        {
            Settings.whiteBalance = mSettingsCache.whiteBalance;
            NV_CHECK_ERROR_CLEANUP(
                SetWhitebalance(Settings.whiteBalance)
            );
            Settings.isDirtyForNvMM.whiteBalance = NV_FALSE;
            Settings.isDirtyForParser.whiteBalance = NV_TRUE;
            mSettingsParser.updateSettings(Settings);
            mWBModeCached = NV_FALSE;
        }
    }

    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

#if NV_FACE_VERBOSE_LOGGING
void NvCameraHal::dumpFaceInfo(camera_face_t *pFace, NvU32 i)
{
    ALOGE("+++++++++++++++++++++For this face (%d)+++++++++++++++++++++", (int)i);
    ALOGE("Face id:%d Face score:%d", (int)pFace->id, (int)pFace->score);
    ALOGE("Face location(left, top, right, bottom):[%d, %d, %d, %d]", (int)pFace->rect[0],
        (int)pFace->rect[1], (int)pFace->rect[2], (int)pFace->rect[3]);
    ALOGE("Left eye center(x, y):[%d, %d]", (int)pFace->left_eye[0], (int)pFace->left_eye[1]);
    ALOGE("Right eye center(x, y):[%d, %d]", (int)pFace->right_eye[0], (int)pFace->right_eye[1]);
    ALOGE("Mouth center(x, y):[%d, %d]", (int)pFace->mouth[0], (int)pFace->mouth[1]);
    ALOGE("---------------------For this face (%d)---------------------", (int)i);
}
#endif

void NvCameraHal::orientation_cb(uint32_t orientation, uint32_t tilt, void *cookie)
{
    if (cookie != NULL)
    {
        ((NvCameraHal *)cookie)->onOrientationEvent(orientation, tilt);
    }
}

void NvCameraHal::onOrientationEvent(uint32_t orientation, uint32_t tilt)
{
    NvS32 corrected;

    if (mFdState == FaceDetectionState_Running)
    {
        // map [0, 359] to {0, 90, 180, 270}
        NvS32 NewOrientation = ((orientation + 45) / 90 * 90) % 360;

        if (NewOrientation != mOrientation)
        {
            // correct the orientation
            if (mSensorId == CAMERA_FACING_FRONT)
            {
                corrected = (mSensorOrientation - NewOrientation + 360) % 360;
            }
            else
            {
                corrected = (mSensorOrientation + NewOrientation) % 360;
            }

            if (mPreviewStarted)
                ProgramDeviceRotation(corrected);

            mOrientation = NewOrientation;
        }

        mTilt = tilt;
    }
}

static void filterResolutions(
    Vector<NvCameraUserResolution> &list,
    Vector<NvCameraUserSensorMode> &SensorModeList,
    const Resolution *resList,
    NvBool FpsLimitNeeded)
{
    NvCameraUserResolution elem;
    int i, NumSensorModes;

    if (!resList)
    {
        return;
    }

    elem.width = 0;
    elem.height = 0;

    list.clear();
    NumSensorModes = SensorModeList.size();
    while (resList->width > 0)
    {
        for(i = 0; i < NumSensorModes; i++)
        {
            if ((int)resList->width <= SensorModeList[i].width &&
                (int)resList->height <= SensorModeList[i].height &&
                (!FpsLimitNeeded || (int)SensorModeList[i].fps >= CAMERA_PREVIEW_FPS_LIMIT))
            {
                elem.width = (int)resList->width;
                elem.height = (int)resList->height;
                list.add(elem);
                break;
            }
        }
        resList += 1;
    }
}

// this function queries sensor capabilities from the driver.nn
// if the capabilities are more restrictive than the defaults, it overrides
// the default settings as well so that we can send an updated version to
// the parser.
// NOTE: there should be something in here for every entry that exists inside
// of the parser's setCapabilities function...otherwise the parser might pick
// up some NULL data if we haven't explicitly filled it here, which could
// lead to some Bad Things.
void
NvCameraHal::GetSensorCapabilities(
    NvCameraCapabilities& parseCaps,
    NvCombinedCameraSettings& newSettings)
{
    NvError e = NvSuccess;
    // Sensor Resolution
    if((e = GetSensorModeList(parseCaps.supportedSensorMode)) == NvSuccess)
    {
        int n = (int)parseCaps.supportedSensorMode.size();

        // preview, still, and video
        filterResolutions(
            parseCaps.supportedPicture,
            parseCaps.supportedSensorMode,
            &stdStillRes[0],
            NV_FALSE);
        filterResolutions(
            parseCaps.supportedVideo,
            parseCaps.supportedSensorMode,
            &stdVideoRes[0],
            NV_FALSE);
        filterResolutions(
            parseCaps.supportedPreview,
            parseCaps.supportedSensorMode,
            &stdPreviewRes[0],
            NV_TRUE);

        // store the max preview/still/video resolutions
        mMaxPreview = parseCaps.supportedPreview[parseCaps.supportedPreview.size() - 1];
        mMaxStill = parseCaps.supportedPicture[parseCaps.supportedPicture.size() - 1];
        mMaxVideo = parseCaps.supportedVideo[parseCaps.supportedVideo.size() - 1];

        //VideoPreviewFPS
        // This code looks like it's assuming the resolution lists are sorted by
        // size which is not a safe assumption to make.
        // TODO update default if it doesn't exist inside here
        NvCameraUserVideoPreviewFps  validPreviewResForVideoRes;
        n = (int)parseCaps.supportedVideo.size();
        for (int i = 0; i < n; i++) {
            validPreviewResForVideoRes.VideoWidth =
                parseCaps.supportedVideo[i].width;
            validPreviewResForVideoRes.VideoHeight =
                parseCaps.supportedVideo[i].height;
            int m = (int)parseCaps.supportedSensorMode.size();
            int fr = 0;
            for (int j = 0; j < m; j++) {
                if ((validPreviewResForVideoRes.VideoWidth <=
                        parseCaps.supportedSensorMode[j].width) &&
                    (validPreviewResForVideoRes.VideoHeight <=
                        parseCaps.supportedSensorMode[j].height))
                {
                    if( parseCaps.supportedSensorMode[j].fps > fr) {
                        fr = parseCaps.supportedSensorMode[j].fps;
                        int l = (int)parseCaps.supportedPreview.size();
                        for (int k =0; k < l; k++){
                            if (((int)parseCaps.supportedPreview[k].width <=
                                    parseCaps.supportedSensorMode[j].width) &&
                              ((int)parseCaps.supportedPreview[k].height <=
                                    parseCaps.supportedSensorMode[j].height))
                            {
                                validPreviewResForVideoRes.PreviewWidth =
                                    parseCaps.supportedPreview[k].width;
                                validPreviewResForVideoRes.PreviewHeight =
                                    parseCaps.supportedPreview[k].height;
                            }
                        }
                    }
                }
            }
            validPreviewResForVideoRes.Maxfps = fr;
            parseCaps.supportedVideoPreviewfps.add(validPreviewResForVideoRes);
        }
    }
    else
    {
        ALOGE("%s: GetSensorModeList failed(%d)", __FUNCTION__, e);
    }

    // Focuser
    NvCameraIspFocusingParameters fParams;
    e = GetSensorFocuserParam(fParams);
    if(e == NvSuccess)
    {
        UpdateSettingsParserFocusModes(fParams, newSettings, parseCaps);
    }
    else
    {
        ALOGE("%s: GetSensorFocuserParam failed(%d)", __FUNCTION__, e);
    }

    // Get default AF focus windows
    UNLOCK_EVENTS();
    e = Cam.Block->GetAttribute(Cam.Block, NvCameraIspAttribute_AutoFocusRegions,
                                   sizeof(mDefFocusRegions), &mDefFocusRegions);
    RELOCK_EVENTS();
    if (e == NvSuccess)
    {
        ALOGVV("%s: Def Focus window: Left %f Right %f Top %f Bottom %f",
            __FUNCTION__,
            mDefFocusRegions.regions[0].left,
            mDefFocusRegions.regions[0].right,
            mDefFocusRegions.regions[0].top,
            mDefFocusRegions.regions[0].bottom
        );
    }
    else
        ALOGE("%s: get default focus window failed(%d)", __FUNCTION__, e);

    //Flash mode
    NvBool hasFlash;
    e = GetSensorFlashParam(hasFlash);
    if(e == NvSuccess)
    {
        UpdateSettingsParserFlashModes(hasFlash, newSettings, parseCaps);
    }
    else
    {
        ALOGE("%s: GetSensorFlashParam failed(%d)", __FUNCTION__, e);
    }

    // ISP
    e = GetSensorIspSupport(parseCaps.hasIsp);
    if(e == NvSuccess)
    {
        ALOGVV("%s: Camera ISP capability: %d\n", __FUNCTION__, parseCaps.hasIsp);
    }
    else
    {
        ALOGE("%s: GetSensorIspSupport failed(%d)", __FUNCTION__, e);
    }

    e = GetSensorBracketCaptureSupport(parseCaps.hasBracketCapture);
    if (e == NvSuccess)
    {
        ALOGVV("%s: Camera bracket capture capability: %d",
            __FUNCTION__, parseCaps.hasBracketCapture);
    }
    else
    {
        ALOGE("%s: GetSensorBracketCaptureSupport failed (%d)", __FUNCTION__, e);
    }

    // scene modes (for now assume that ISP and AF support are all that we need
    // for scene mode support, this could probably be smarter, as scene modes
    // depend on AF, AE, AWB, and some other params, but it's a good approx for
    // starters)
    if (parseCaps.hasIsp)
    {
        parseCaps.hasSceneModes = NV_TRUE;
    }
    else
    {
        parseCaps.hasSceneModes = NV_FALSE;
        newSettings.sceneMode = NvCameraSceneMode_Invalid;
        newSettings.isDirtyForNvMM.sceneMode = NV_FALSE;
        newSettings.isDirtyForParser.sceneMode = NV_FALSE;
    }

    // Stereo mode
    NvBool stereoSupported;
    e = GetSensorStereoCapable(stereoSupported);
    if(e == NvSuccess)
    {
        UpdateSettingsParserStereoModes(stereoSupported, newSettings, parseCaps);
    }
    else
    {
        ALOGE("%s: GetSensorStereoCapable failed(%d)", __FUNCTION__, e);
    }

    // FD max limit
    parseCaps.fdMaxNumFaces = (int)GetFdMaxLimit();

    e = GetPassThroughSupport(mIsPassThroughSupported);
    if(e == NvSuccess)
    {
        if (mIsPassThroughSupported)
        {
            ALOGVV("%s: sensor support Pass through Mode\n", __FUNCTION__);
        }
        else
        {
            ALOGVV("%s: sensor does not support Pass through Mode\n", __FUNCTION__);
        }
    }
    else
    {
        ALOGE("%s: Pass through Mode support check failed(%d)", __FUNCTION__, e);
    }
}

// if flag is true, we'll wait on the condition variable
NvError NvCameraHal::CheckAndWaitForCondition(NvBool Flag, Condition &Cond) const
{
    status_t err = NO_ERROR;

    ALOGVV("%s++", __FUNCTION__);

    if (Flag)
    {
        ALOGVV("%s: waiting...", __FUNCTION__);
        err = Cond.waitRelative(mLock, GENERAL_CONDITION_TIMEOUT);
        if (err == TIMED_OUT)
        {
            ALOGVV("%s-- (timeout)", __FUNCTION__);
            return NvError_Timeout;
        }
        else if (err != NO_ERROR)
        {
            ALOGVV("%s-- (error)", __FUNCTION__);
            return NvError_BadParameter;
        }
    }
    ALOGVV("%s--", __FUNCTION__);
    return NvSuccess;
}

// always wait
NvError NvCameraHal::WaitForCondition(Condition &Cond)
{
    return CheckAndWaitForCondition(NV_TRUE, Cond);
}

void NvCameraHal::NotifyCb(int msgType, int ext1, int ext2, void* user)
{
    NvCameraClientCallback *cb =
        new NvCameraClientCallback(mNotifyCb, msgType, ext1, ext2, user);
    if (!mCbQueue.add(cb))
    {
        mCbQueue.stopInput();

        // calling this here would be bad, since it could recurse indefinitely
        // if it keeps failing to add to the queue and then calling itself
        //NOTIFY_ERROR();
    }
}

void
NvCameraHal::DataCb(
    int msgType,
    camera_memory_t *shmem,
    void* user,
    camera_frame_metadata_t *metadata)
{
    NvCameraClientCallback *cb =
        new NvCameraClientCallback(mDataCb, msgType, shmem, user, metadata);
    if (!mCbQueue.add(cb))
    {
        mCbQueue.stopInput();
        NOTIFY_ERROR();
    }
}

void
NvCameraHal::DataCbTimestamp(
    nsecs_t timestamp,
    int32_t msgType,
    camera_memory_t *shmem,
    void* user)
{
    NvCameraClientCallback *cb =
        new NvCameraClientCallback(
            mDataCbTimestamp,
            msgType,
            timestamp,
            shmem,
            user);
    if (!mCbQueue.add(cb))
    {
        mCbQueue.stopInput();
        NOTIFY_ERROR();
    }
}

NvError NvCameraHal::HandleEnableMsgType(int32_t msgType)
{
    NvError e = NvSuccess;
    int32_t msgToEnable;

    ALOGVV("%s++", __FUNCTION__);

    // clear out any bits for messages that were already enabled,
    // we don't need to enable things twice
    msgToEnable = msgType & ~mMsgEnabled;

    // some of these need to be turned on by special API calls into the driver,
    // handle those here
    if (msgToEnable & CAMERA_MSG_PREVIEW_FRAME)
    {
        mPreviewFrameCopy->Enable(NV_TRUE);
    }

    if (msgToEnable & CAMERA_MSG_POSTVIEW_FRAME)
    {
        mPostviewFrameCopy->Enable(NV_TRUE);
    }

    if (msgToEnable & CAMERA_MSG_RAW_IMAGE)
    {
        const NvCombinedCameraSettings &Settings =
                mSettingsParser.getCurrentSettings();
        NvMMDigitalZoomFrameCopy FrameCopy;

        NvOsMemset(&FrameCopy, 0, sizeof(FrameCopy));
        // When non of bits in rawDumpFlag is set, yuv fils is output to raw
        // callback. Otherwise, bayer raw file is output to raw callback
        if (Settings.rawDumpFlag == 0)
        {
            FrameCopy.Enable = NV_TRUE;

            //asked YUV data by app so setting passthrough mode
            mIsPassThroughSupported = NV_TRUE;

            if (mIsPassThroughSupported)
            {
                NvU32 enablePassThrough = 0;
                enablePassThrough |= ENABLE_STILL_PASSTHROUGH;
                DZ.Block->SetAttribute(DZ.Block, NvMMDigitalZoomAttributeType_StillPassthrough,
                                            1, sizeof(NvU32), &enablePassThrough);
                Cam.Block->SetAttribute(Cam.Block, NvMMCameraAttribute_StillPassthrough,
                                            1, sizeof(NvU32), &enablePassThrough);
            }
        }
        else
        {
            NvU32 enablePassThrough = 0;
            DZ.Block->SetAttribute(DZ.Block, NvMMDigitalZoomAttributeType_StillPassthrough,
                                        1, sizeof(NvU32), &enablePassThrough);
            Cam.Block->SetAttribute(Cam.Block, NvMMCameraAttribute_StillPassthrough,
                                        1, sizeof(NvU32), &enablePassThrough);
            FrameCopy.Enable = NV_FALSE;
        }
        FrameCopy.Width = Settings.imageResolution.width;
        FrameCopy.Height = Settings.imageResolution.height;
        NV_CHECK_ERROR_CLEANUP(
            DZ.Block->SetAttribute(
                DZ.Block,
                NvMMDigitalZoomAttributeType_StillFrameCopy,
                0,
                sizeof(FrameCopy),
                &FrameCopy)
        );
    }

    if (!(msgToEnable & CAMERA_MSG_RAW_IMAGE) && (msgToEnable & CAMERA_MSG_COMPRESSED_IMAGE))
    {
            NvU32 enablePassThrough = 0;
            DZ.Block->SetAttribute(DZ.Block, NvMMDigitalZoomAttributeType_StillPassthrough,
                                        1, sizeof(NvU32), &enablePassThrough);
            Cam.Block->SetAttribute(Cam.Block, NvMMCameraAttribute_StillPassthrough,
                                        1, sizeof(NvU32), &enablePassThrough);
    }

    // others are either on by default in the driver, or determined as a
    // runtime decision as to whether we'll actually send the callback
    // on to the app or just swallow it when it comes through to the HAL.
    // they don't require any special driver switches to be flipped,
    // but we still need to update the state so that the HAL can make the
    // right decision as to which callbacks will be sent to the app.
    mMsgEnabled |= msgToEnable;

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::HandleDisableMsgType(int32_t msgType)
{
    NvError e = NvSuccess;
    int32_t msgToDisable;

    ALOGVV("%s++", __FUNCTION__);

    // clear out any bits for messages that were already disabled,
    // we don't need to disable things twice
    msgToDisable = msgType & mMsgEnabled;

    if (msgToDisable & CAMERA_MSG_PREVIEW_FRAME)
    {
        mPreviewFrameCopy->Enable(NV_FALSE);
    }

    if (msgToDisable & CAMERA_MSG_POSTVIEW_FRAME)
    {
        mPostviewFrameCopy->Enable(NV_FALSE);
    }

    if (msgToDisable & CAMERA_MSG_RAW_IMAGE)
    {
        const NvCombinedCameraSettings &Settings =
                mSettingsParser.getCurrentSettings();
        NvMMDigitalZoomFrameCopy FrameCopy;

        NvOsMemset(&FrameCopy, 0, sizeof(FrameCopy));
        FrameCopy.Enable = NV_FALSE;
        FrameCopy.Width = Settings.imageResolution.width;
        FrameCopy.Height = Settings.imageResolution.height;
        NV_CHECK_ERROR_CLEANUP(
            DZ.Block->SetAttribute(
                DZ.Block,
                NvMMDigitalZoomAttributeType_StillFrameCopy,
                0,
                sizeof(FrameCopy),
                &FrameCopy)
        );
    }

    if (msgToDisable & CAMERA_MSG_VIDEO_FRAME)
    {
        // android API doesn't guarantee that releaseRecordingFrame will get
        // called for any more buffers once this msg has been disabled,
        // so we need to reclaim them all on our end.
        ReclaimDeliveredVideoBuffers();
    }

    // for all of them (even the ones we didn't do anything special for),
    // flip the switch
    mMsgEnabled &= ~msgToDisable;

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

// compares old set to new set, applies anything that changed
// modeled after the old ApplyUserSettings, but more direct and less
// hacky.  also updates the parser's state directly at the end, instead
// of relying on the client code to update things
NvError NvCameraHal::ApplyChangedSettings()
{
    ALOGVV("%s++", __FUNCTION__);

    NvError e = NvSuccess;
    NvCombinedCameraSettings newSettings;
    NvBool FlashEnabled = UsingFlash();
    // some other settings might be different depending on whether this was
    // changed, cache the dirty check into here for readability
    NvBool ChangeStill, ChangePreview;
    NvU32 requestedNSLNumBuffers = 0;
    NvU32 allocatedNSLNumBuffers = 0;

#if ENABLE_NVIDIA_CAMTRACE
    if (!mPreviewStreaming)
    {
        TRACE_EVENT(__func__,
                CAM_EVENT_TYPE_HAL_SETTINGS_ENTRY,
                NvOsGetTimeUS(),
                3);
    }
#endif
    newSettings = mSettingsParser.getCurrentSettings();

    // Slow Motion programming
    {
        // If user just switches between slow motion option where only
        // videoSpeed is changed and recording not started,
        // Buffer Reconfiguration does not take place, so need to
        // program videoSpeed separately.
        if ((newSettings.isDirtyForNvMM.videoSpeed) ||
            (newSettings.isDirtyForNvMM.videoHighSpeedRec))
        {
            SetVideoSpeed(newSettings);
            newSettings.isDirtyForNvMM.videoSpeed = NV_FALSE;
            newSettings.isDirtyForNvMM.videoHighSpeedRec = NV_FALSE;
        }
        if (newSettings.isDirtyForNvMM.previewFpsRange)
        {
            if ((newSettings.videoSpeed != 1.0f) || (newSettings.videoHighSpeedRec))
            {
                if (!mFrameRateChanged)
                {
                    NV_CHECK_ERROR_CLEANUP(
                        SetFrameRateRange(newSettings)
                    );
                    newSettings.isDirtyForNvMM.previewFpsRange = NV_FALSE;
                    newSettings.isDirtyForParser.previewFpsRange = NV_TRUE;
                    // In slow motion, Reconfigure preview buffers even if preview
                    // size remains same so that the high speed mode gets
                    // selected and buffers size can be changed.
                    newSettings.isDirtyForNvMM.previewResolution = NV_TRUE;
                }
            }
        }
    }

    if (newSettings.isDirtyForNvMM.recordingHint)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetRecordingHint(newSettings.recordingHint));

        newSettings.isDirtyForNvMM.recordingHint = NV_FALSE;
    }

    ChangePreview = newSettings.isDirtyForNvMM.previewResolution;
    if (ChangePreview)
    {
        if (mPreviewEnabled == NV_TRUE)
        {
            NV_CHECK_ERROR_CLEANUP(
                StopPreviewInternal()
            );

            NV_CHECK_ERROR_CLEANUP(
                BufferManagerResetANWBuffers(NV_TRUE)
            );

            NV_CHECK_ERROR_CLEANUP(
                BufferManagerReconfigurePreviewBuffersResolution(
                    newSettings.previewResolution.width,
                    newSettings.previewResolution.height)
            );

            NV_CHECK_ERROR_CLEANUP(
                StartPreviewInternal()
            );
        }
        else
        {
            if (mPreviewPaused)
            {
                NV_CHECK_ERROR_CLEANUP(
                    StopPreviewInternal()
                );
            }
            NV_CHECK_ERROR_CLEANUP(
                BufferManagerResetANWBuffers(NV_TRUE)
            );

            NV_CHECK_ERROR_CLEANUP(
                BufferManagerReconfigurePreviewBuffersResolution(
                    newSettings.previewResolution.width,
                    newSettings.previewResolution.height)
            );
        }

        newSettings.isDirtyForNvMM.previewResolution = NV_FALSE;
    }

    ChangeStill = newSettings.isDirtyForNvMM.imageResolution;
    if (ChangeStill || newSettings.isDirtyForNvMM.imageQuality)
    {
        if (ChangeStill)
        {
            NV_CHECK_ERROR_CLEANUP(
                BufferManagerReconfigureStillBuffersResolution(
                    newSettings.imageResolution.width,
                    newSettings.imageResolution.height)
            );
        }

        if (mPreviewStreaming && !mDelayJpegStillEncoderSetup)
        {
            NV_CHECK_ERROR_CLEANUP(
                WaitForStuffThatNeedsPreview()
            );
            NV_CHECK_ERROR_CLEANUP(
                SetupJpegEncoderStill()
            );
        }
        else
        {
            mDelayJpegStillEncoderSetup = NV_TRUE;
        }
        newSettings.isDirtyForNvMM.imageResolution = NV_FALSE;
        newSettings.isDirtyForNvMM.imageQuality = NV_FALSE;
    }

    // We can't change the video resolution while recording
    // (and the app occasionally attempts to do this during video snapshot)
    if (newSettings.isDirtyForNvMM.videoResolution && !mVideoStarted)
    {
        NV_CHECK_ERROR_CLEANUP(
            BufferManagerReconfigureVideoBuffersResolution(
                newSettings.videoResolution.width,
                newSettings.videoResolution.height)
        );
        newSettings.isDirtyForNvMM.videoResolution = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.thumbnailResolution ||
        newSettings.isDirtyForNvMM.thumbnailEnable ||
        newSettings.isDirtyForNvMM.thumbnailSupport ||
        newSettings.isDirtyForNvMM.thumbnailQuality)
    {
        if (newSettings.isDirtyForNvMM.thumbnailEnable)
        {
            // restart encoder to pick up new thumbnail on/off config
            // no need to CloseJpegEncoder due to OpenJpegEncoder will close it internally
            if (mPreviewStreaming &&
                !mDelayJpegEncoderOpen &&
                !mDelayJpegStillEncoderSetup)
            {
                NV_CHECK_ERROR_CLEANUP(
                    WaitForStuffThatNeedsPreview()
                );
                OpenJpegEncoder();
                SetupJpegEncoderStill();
            }
            else
            {
                mDelayJpegEncoderOpen = NV_TRUE;
                mDelayJpegStillEncoderSetup = NV_TRUE;
            }
        }

        if (newSettings.thumbnailEnable && newSettings.thumbnailSupport)
        {
            // configure thumbnail size and quality
            if (mPreviewStreaming && !mDelayJpegThumbnailEncoderSetup)
            {
                NV_CHECK_ERROR_CLEANUP(
                    WaitForStuffThatNeedsPreview()
                );
                SetupJpegEncoderThumbnail();
            }
            else
            {
                mDelayJpegThumbnailEncoderSetup = NV_TRUE;
            }
        }
        newSettings.isDirtyForNvMM.thumbnailResolution = NV_FALSE;
        newSettings.isDirtyForNvMM.thumbnailEnable = NV_FALSE;
        newSettings.isDirtyForNvMM.thumbnailSupport = NV_FALSE;
        newSettings.isDirtyForNvMM.thumbnailQuality = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.sceneMode)
    {
        // this will modify the settings struct, and the settings logic that
        // follows this will naturally catch the changes and apply them
        // as it compares the difference between the modified newSettings struct
        // and the oldSettings struct for all settings that could be changed
        // by this operation
        GetSceneModeSettings(newSettings);
        newSettings.isDirtyForNvMM.sceneMode = NV_FALSE;

        // lock up the parser's capabilities for anything non-auto
        if (newSettings.sceneMode != NvCameraSceneMode_Auto)
        {
            mSettingsParser.lockSceneModeCapabilities(
                newSettings.flashMode,
                newSettings.focusMode,
                newSettings.whiteBalance);
        }
        else
            mSettingsParser.unlockSceneModeCapabilities();
    }

    if (newSettings.isDirtyForNvMM.CaptureMode)
    {
        //this will modify settings struct
        //including postview, ANR, burst & skip, frame rate,
        //preview pause and face detection
        NV_CHECK_ERROR_CLEANUP(
            EnableShot2Shot(newSettings)
        );
        newSettings.isDirtyForNvMM.CaptureMode = NV_FALSE;
    }

    // All settings could possibly be modified by scene mode should be
    // processed after scene mode is processed
    // TODO what happens if both of these are set at the same time?
    if (newSettings.isDirtyForNvMM.WhiteBalanceCCTRange)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetWhitebalanceCCTRange(newSettings)
        );
        newSettings.isDirtyForNvMM.WhiteBalanceCCTRange = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.whiteBalance)
    {
        NV_CHECK_ERROR_CLEANUP(
             SetWhitebalance(newSettings.whiteBalance)
        );
        newSettings.isDirtyForNvMM.whiteBalance = NV_FALSE;
    }

    // Besides NSL, video stabilization also have different buffer number
    // requirement so we should check it here
    // Don't clear videoStabOn bit before this check
    if ((ChangeStill || ChangePreview ||
            newSettings.isDirtyForNvMM.nslNumBuffers) &&
            !newSettings.videoStabOn)
    {
        if (newSettings.stillCapHdrOn &&
            newSettings.nslNumBuffers > 0)
        {
            NV_CHECK_ERROR_CLEANUP(
                DisableStillHDR(newSettings)
            );
        }

        requestedNSLNumBuffers = newSettings.nslNumBuffers;

        NV_CHECK_ERROR_CLEANUP(
            SetNSLNumBuffers(requestedNSLNumBuffers, &allocatedNSLNumBuffers)
        );

        newSettings.nslNumBuffers = allocatedNSLNumBuffers;

        if (requestedNSLNumBuffers != allocatedNSLNumBuffers)
        {
            newSettings.isDirtyForParser.nslNumBuffers = NV_TRUE;
            //Only warns if we can't get enough buffers as requested
            if (requestedNSLNumBuffers > allocatedNSLNumBuffers)
            {
                ALOGWW("%s requests %d NSL buffers, and get %d instead",
                    __func__,
                    requestedNSLNumBuffers,
                    newSettings.nslNumBuffers);
            }
        }
        newSettings.isDirtyForNvMM.nslNumBuffers = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.nslSkipCount)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetNSLSkipCount(newSettings)
        );
        newSettings.isDirtyForNvMM.nslSkipCount = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.nslBurstCount)
    {
        if (newSettings.stillCapHdrOn &&
            newSettings.nslBurstCount > 0)
        {
            NV_CHECK_ERROR_CLEANUP(
                DisableStillHDR(newSettings)
            );
        }
        //Reset flash mode now that NSL has changed
        newSettings.isDirtyForNvMM.flashMode = NV_TRUE;

        if ((newSettings.nslBurstCount == 0) &&
            (newSettings.burstCaptureCount == 0))
        {
            NV_CHECK_ERROR_CLEANUP(
                StopStillCapture()
            );
        }
        newSettings.isDirtyForNvMM.nslBurstCount = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.skipCount)
    {
        if (newSettings.stillCapHdrOn &&
            newSettings.skipCount > 0)
        {
            NV_CHECK_ERROR_CLEANUP(
                DisableStillHDR(newSettings)
            );
        }
        newSettings.isDirtyForNvMM.skipCount = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.burstCaptureCount)
    {

        if (newSettings.stillCapHdrOn &&
            newSettings.burstCaptureCount != 1)
        {
            NV_CHECK_ERROR_CLEANUP(
                DisableStillHDR(newSettings)
            );
        }

        if ((newSettings.nslBurstCount == 0) &&
            (newSettings.burstCaptureCount == 0))
        {
            NV_CHECK_ERROR_CLEANUP(
                StopStillCapture()
            );
        }
        newSettings.isDirtyForNvMM.burstCaptureCount = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.bracketCaptureCount)
    {
        if (newSettings.stillCapHdrOn)
        {
            NV_CHECK_ERROR_CLEANUP(
                DisableStillHDR(newSettings)
            );
        }

        NV_CHECK_ERROR_CLEANUP(
                SetBracketCapture(newSettings)
        );
        newSettings.isDirtyForNvMM.bracketCaptureCount = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.rawDumpFlag)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetRawDumpFlag(newSettings)
        );
        newSettings.isDirtyForNvMM.rawDumpFlag = NV_FALSE;
    }

    // This bit should always be cleared after nslNumBuffers check
    if (newSettings.isDirtyForNvMM.videoStabOn)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetVSTABNumBuffers()
        );
        SetStabilizationMode(newSettings);
        newSettings.isDirtyForNvMM.videoStabOn = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.customPostviewOn)
    {
        ALOGWW("nv-custom-postview attribute is deprecated\n"
              "Please use preview postprocessing instead.");
        newSettings.isDirtyForNvMM.customPostviewOn = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.colorEffect)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetColorEffect(newSettings)
        );
        newSettings.isDirtyForNvMM.colorEffect = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.antiBanding)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetAntiBanding(newSettings)
        );
        newSettings.isDirtyForNvMM.antiBanding = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.PreviewFlip)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetImageFlipRotation(newSettings, NV_FALSE)
        );
        newSettings.isDirtyForNvMM.PreviewFlip = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.exposureCompensationIndex)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetExposureCompensation(newSettings)
        );
        newSettings.isDirtyForNvMM.exposureCompensationIndex = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.fdLimit)
    {
        // There is just a GetAttribute interface for
        // FDLimit, so do nothing.
        newSettings.isDirtyForNvMM.fdLimit = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.fdDebug)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetFdDebug(newSettings)
        );
        newSettings.isDirtyForNvMM.fdDebug = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.imageRotation ||
        newSettings.isDirtyForNvMM.autoRotation ||
        newSettings.isDirtyForNvMM.StillFlip)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetImageFlipRotation(newSettings, NV_TRUE)
        );
        if (mPreviewStreaming && !mDelayJpegStillEncoderSetup)
        {
            NV_CHECK_ERROR_CLEANUP(
                WaitForStuffThatNeedsPreview()
            );
            NV_CHECK_ERROR_CLEANUP(
                SetupJpegEncoderStill()
            );
        }
        else
        {
            mDelayJpegStillEncoderSetup = NV_TRUE;
        }

        // Program thumbnail when rotation occurs
        if (newSettings.thumbnailEnable)
        {
            if (mPreviewStreaming && !mDelayJpegThumbnailEncoderSetup)
            {
                NV_CHECK_ERROR_CLEANUP(
                    WaitForStuffThatNeedsPreview()
                );
                NV_CHECK_ERROR_CLEANUP(
                    SetupJpegEncoderThumbnail()
                );
            }
            else
            {
                mDelayJpegThumbnailEncoderSetup = NV_TRUE;
            }
        }
        newSettings.isDirtyForNvMM.imageRotation = NV_FALSE;
        newSettings.isDirtyForNvMM.autoRotation = NV_FALSE;
        newSettings.isDirtyForNvMM.StillFlip = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.PreviewFormat)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetPreviewFormat(newSettings)
        );
        newSettings.isDirtyForNvMM.PreviewFormat = NV_FALSE;
    }

    // don't process HDR if shot 2 shot is on.  they conflict.  wait until
    // shot 2 shot is over
    if (newSettings.isDirtyForNvMM.stillCapHdrOn &&
        (newSettings.CaptureMode != NvCameraUserCaptureMode_Shot2Shot))
    {
        if ( (newSettings.nslBurstCount > 0       ||
             newSettings.skipCount > 0            ||
             newSettings.burstCaptureCount != 1   ||
             newSettings.bracketCaptureCount > 0) &&
             newSettings.stillCapHdrOn == NV_TRUE)
        {
            // do not allow HDR to enable if any of the above are set
            newSettings.stillCapHdrOn = NV_FALSE;
            newSettings.isDirtyForParser.stillCapHdrOn = NV_TRUE;
        }

        // turn ANR off if HDR is on, to help perf.
        // restore it when HDR is off.
        if (newSettings.stillCapHdrOn)
        {
            NV_CHECK_ERROR_CLEANUP(
                DisableANR(newSettings)
            );
        }
        else
        {
            NV_CHECK_ERROR_CLEANUP(
                RestoreANR(newSettings)
            );
        }

        NV_CHECK_ERROR_CLEANUP(
            SetStillHDRMode(newSettings)
        );

        newSettings.isDirtyForNvMM.stillCapHdrOn = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.stillHdrSourceFramesToEncode)
    {
        mPostProcessStill->ConfigureInputFrameEncoding(
            newSettings.stillHdrSourceFramesToEncode,
            newSettings.stillHdrSourceFrameCount);
        newSettings.isDirtyForNvMM.stillHdrSourceFramesToEncode = NV_FALSE;
    }

    if ((newSettings.isDirtyForNvMM.previewFpsRange) &&
        ((newSettings.videoSpeed == 1.0f) &&
        (!newSettings.videoHighSpeedRec)))
    {
        NV_CHECK_ERROR_CLEANUP(
            SetFrameRateRange(newSettings)
        );
        newSettings.isDirtyForNvMM.previewFpsRange = NV_FALSE;
        newSettings.isDirtyForParser.previewFpsRange = NV_TRUE;
    }

    if (newSettings.isDirtyForNvMM.AELocked && !FlashEnabled)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetAELock(newSettings)
        );
        newSettings.isDirtyForNvMM.AELocked = NV_FALSE;
        // query the AE state and store for later queries from app
        if (getCamType(mSensorId) != NVCAMERAHAL_CAMERATYPE_USB)
        {
            updateAEParameters(newSettings);
        }
    }

    if (newSettings.isDirtyForNvMM.AWBLocked && !FlashEnabled)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetAWBLock(newSettings)
        );
        newSettings.isDirtyForNvMM.AWBLocked = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.contrast)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetContrast(newSettings)
        );
        newSettings.isDirtyForNvMM.contrast = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.saturation)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetSaturation(newSettings)
        );
        newSettings.isDirtyForNvMM.saturation = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.edgeEnhancement)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetEdgeEnhancement(newSettings)
        );
        newSettings.isDirtyForNvMM.edgeEnhancement = NV_FALSE;
    }

    // TODO what should happen if both exposure time and exposure range
    // are set?
    if (newSettings.isDirtyForNvMM.exposureTimeRange)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetExposureTimeRange(newSettings)
        );
        newSettings.isDirtyForNvMM.exposureTimeRange = NV_FALSE;
    }
    if (newSettings.isDirtyForNvMM.exposureTimeMicroSec)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetExposureTimeMicroSec(newSettings)
        );
        newSettings.isDirtyForNvMM.exposureTimeMicroSec = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.flashMode)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetFlashMode(newSettings)
        );
        newSettings.isDirtyForNvMM.flashMode = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.focusMode)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetFocusMode(newSettings)
        );
        newSettings.isDirtyForNvMM.focusMode = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.focusPosition)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetFocusPosition(newSettings)
        );
        newSettings.isDirtyForNvMM.focusPosition = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.colorCorrectionMatrix)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetColorCorrectionMatrix(newSettings)
        );
        newSettings.isDirtyForNvMM.colorCorrectionMatrix = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.zoomLevel)
    {
        if (mSmoothZoomInProgress)
        {
            ALOGWW("%s: Got new zoom value while smooth zoom in progress, ignoring", __FUNCTION__);
        }
        else
        {
            mSmoothZoomStopping = NV_FALSE;
            NV_CHECK_ERROR_CLEANUP(
                SetZoom(newSettings.zoomLevel, NV_FALSE)
            );
            mSmoothZoomCurrentLevel = newSettings.zoomLevel;
        }
        newSettings.isDirtyForNvMM.zoomLevel = NV_FALSE;
    }

    // TODO what should happen if both iso and isoRange are set?
    if (newSettings.isDirtyForNvMM.isoRange)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetIsoSensitivityRange(newSettings)
        );
        newSettings.isDirtyForNvMM.isoRange = NV_FALSE;
    }
    if (newSettings.isDirtyForNvMM.iso)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetIsoSensitivity(newSettings)
        );
        newSettings.isDirtyForNvMM.iso = NV_FALSE;
    }

    // if any of the GPS info changes, we need to re-apply all gps settings
    if (newSettings.isDirtyForNvMM.GPSBitMapInfo ||
        newSettings.isDirtyForNvMM.gpsLatitude ||
        newSettings.isDirtyForNvMM.latitudeDirection ||
        newSettings.isDirtyForNvMM.gpsLongitude ||
        newSettings.isDirtyForNvMM.longitudeDirection ||
        newSettings.isDirtyForNvMM.gpsAltitude ||
        newSettings.isDirtyForNvMM.gpsAltitudeRef ||
        newSettings.isDirtyForNvMM.gpsTimestamp ||
        newSettings.isDirtyForNvMM.gpsProcMethod)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetGPSSettings(newSettings)
       );

        newSettings.isDirtyForNvMM.GPSBitMapInfo = NV_FALSE;
        newSettings.isDirtyForNvMM.gpsLatitude = NV_FALSE;
        newSettings.isDirtyForNvMM.latitudeDirection = NV_FALSE;
        newSettings.isDirtyForNvMM.gpsLongitude = NV_FALSE;
        newSettings.isDirtyForNvMM.longitudeDirection = NV_FALSE;
        newSettings.isDirtyForNvMM.gpsAltitude = NV_FALSE;
        newSettings.isDirtyForNvMM.gpsAltitudeRef = NV_FALSE;
        newSettings.isDirtyForNvMM.gpsTimestamp = NV_FALSE;
        newSettings.isDirtyForNvMM.gpsProcMethod = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.sensorMode)
    {
        newSettings.isDirtyForNvMM.sensorMode = NV_FALSE;
        ALOGWW("nv-sensor-mode attribute is deprecated\n"
              "Please use setPictureSize and setPreviewFpsRange instead.");
    }

    if (newSettings.isDirtyForNvMM.FocusWindows && mSettingsParser.isFocusSupported())
    {
        NV_CHECK_ERROR_CLEANUP(
            SetFocusWindows(newSettings)
        );
        newSettings.isDirtyForNvMM.FocusWindows = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.ExposureWindows)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetExposureWindows(newSettings)
        );
        newSettings.isDirtyForNvMM.ExposureWindows = NV_FALSE;
    }

#ifdef FRAMEWORK_HAS_MACRO_MODE_SUPPORT
    if (newSettings.isDirtyForNvMM.LowLightThreshold)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetLowLightThreshold(newSettings)
        );
        newSettings.isDirtyForNvMM.LowLightThreshold = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.MacroModeThreshold)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetMacroModeThreshold(newSettings)
        );
        newSettings.isDirtyForNvMM.MacroModeThreshold = NV_FALSE;
    }
#endif // def FRAMEWORK_HAS_MACRO_MODE_SUPPORT

    if (newSettings.isDirtyForNvMM.FocusMoveMsg)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetFocusMoveMsg(newSettings.FocusMoveMsg)
        );
    }

    // wait until shot2shot is done before processing any new ANR settings from
    // the app
    if (newSettings.isDirtyForNvMM.AnrInfo &&
        (newSettings.CaptureMode != NvCameraUserCaptureMode_Shot2Shot))
    {
        // user overrode ANR, it's active again
        mANRCached = NV_FALSE;

        // HDR requires ANR disabled.  if anyone tries to turn it
        // back on after HDR has disabled it, we'll assume they don't
        // want HDR anymore, and turn HDR off.
        if (newSettings.stillCapHdrOn &&
            (newSettings.AnrInfo != AnrMode_Off))
        {
            DisableStillHDR(newSettings);
        }

        NV_CHECK_ERROR_CLEANUP(
            SetAdvancedNoiseReductionMode(
                newSettings.AnrInfo)
        );
        newSettings.isDirtyForNvMM.AnrInfo = NV_FALSE;
    }

    if (newSettings.isDirtyForNvMM.DisablePreviewPause)
    {
        // note that the API that we use to disable preview pause is
        // the inverse of the arg to this function, which is specifying
        // whether preview is paused.
        if (!mVideoEnabled)
        {
            NV_CHECK_ERROR_CLEANUP(
                SetPreviewPauseAfterStillCapture(
                    newSettings,
                    !newSettings.DisablePreviewPause)
            );
        }
        else
        {
            //Do not allow preview pause if recording video
            newSettings.DisablePreviewPause = NV_TRUE;
            newSettings.isDirtyForParser.DisablePreviewPause = NV_TRUE;
        }
        newSettings.isDirtyForNvMM.DisablePreviewPause = NV_FALSE;
    }

    // TODO NSL in action scene mode

    // TODO get successful NSL buffers if they were set


    // since things may have changed as we've applied the settings
    // (e.g. scene mode change triggering a WB change),
    // give our settings struct back to the parser.  even if things
    // didn't change, this will let the parser know it can finally move all of
    // these into the previous settings that it's keeping track of
    mSettingsParser.updateSettings(newSettings);

    if (mWaitForManualSettings)
    {
        // wait for the requested number of frames
        // to ensure that the settings are programmed
        // to the sensor before taking a picture
        mMasterWaitFlagForSettings = NV_TRUE;
        mWaitForManualSettings = NV_FALSE;
    }

#if ENABLE_NVIDIA_CAMTRACE
    if (!mPreviewStreaming)
    {
        TRACE_EVENT(__func__,
                CAM_EVENT_TYPE_HAL_SETTINGS_EXIT,
                NvOsGetTimeUS() ,
                3);
    }
#endif
    ALOGVV("%s--", __FUNCTION__);
    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::EnableShot2Shot(NvCombinedCameraSettings &newSettings)
{
    NvError e = NvSuccess;
    ALOGVV("%s++", __FUNCTION__);

    NvBool enable = (newSettings.CaptureMode == NvCameraUserCaptureMode_Shot2Shot);

    if (enable)
    {
        // disable HDR in Shot2Shot, they are incompatible
        if (newSettings.stillCapHdrOn)
        {
            DisableStillHDR(newSettings);
        }

        //we use fast burst only when nsl is off
        if (newSettings.nslNumBuffers == 0)
        {
            mFastburstEnabled = NV_TRUE;
            NV_CHECK_ERROR_CLEANUP(
                NvOsSemaphoreCreate(&mhSemPictureRequest, 0)
            );
        }

        //disable face detection in cont.shot
        //we can not stop FD immediately since it will join the
        //FD start thread which cause deadlock
        NV_CHECK_ERROR_CLEANUP(
            SetFdState(FaceDetectionState_Stop, NV_FALSE)
        );
    }

    GetShot2ShotModeSettings(newSettings, enable, mFastburstEnabled);

    if (!enable && mFastburstEnabled)
    {
        NV_CHECK_ERROR_CLEANUP(
            StopStillCapture()
        );

        mFastburstEnabled = NV_FALSE;
        mFastburstStarted = NV_FALSE;
        if (mhSemPictureRequest)
        {
            NvOsSemaphoreSignal(mhSemPictureRequest);
            NvOsSemaphoreDestroy(mhSemPictureRequest);
            mhSemPictureRequest = NULL;
        }
    }

    NV_CHECK_ERROR_CLEANUP(
        SetPreviewPauseAfterStillCapture(newSettings, !enable)
    );

    NV_CHECK_ERROR_CLEANUP(
        SetFastburst(newSettings)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::DisableStillHDR(NvCombinedCameraSettings &newSettings)
{
    NvError e = NvSuccess;
    ALOGVV("%s++", __FUNCTION__);

    newSettings.stillCapHdrOn = NV_FALSE;
    newSettings.isDirtyForParser.stillCapHdrOn = NV_TRUE;
    newSettings.isDirtyForNvMM.stillCapHdrOn = NV_FALSE;
    NV_CHECK_ERROR_CLEANUP(
        SetStillHDRMode(newSettings)
    );

    RestoreANR(newSettings);

    ALOGVV("%s--", __FUNCTION__);
    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::DisableANR(NvCombinedCameraSettings &newSettings)
{
    NvError e = NvSuccess;

    // don't double-cache, or we'll lose the state we need to restore to
    if (mANRCached)
    {
        ALOGVV("%s-- (already disabled)", __FUNCTION__);
        return e;
    }

    ALOGVV("%s++", __FUNCTION__);
    mSettingsCache.AnrInfo = newSettings.AnrInfo;
    newSettings.AnrInfo = AnrMode_Off;
    mANRCached = NV_TRUE;

    NV_CHECK_ERROR_CLEANUP(
        SetAdvancedNoiseReductionMode(
            newSettings.AnrInfo)
    );

    newSettings.isDirtyForNvMM.AnrInfo = NV_FALSE;
    newSettings.isDirtyForParser.AnrInfo = NV_TRUE;

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::RestoreANR(NvCombinedCameraSettings &newSettings)
{

    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);
    newSettings.AnrInfo = mSettingsCache.AnrInfo;
    mANRCached = NV_FALSE;

    NV_CHECK_ERROR_CLEANUP(
        SetAdvancedNoiseReductionMode(
            newSettings.AnrInfo)
    );

    newSettings.isDirtyForNvMM.AnrInfo = NV_FALSE;
    newSettings.isDirtyForParser.AnrInfo = NV_TRUE;

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

VideoBuffer *NvCameraHal::GetEmptyVideoBuffer()
{
    NvU32 i;
    ALOGVV("%s++", __FUNCTION__);
    for (i = 0; i < NvCameraMemProfileConfigurator::MAX_NUMBER_OF_BUFFERS; i++)
    {
        // return the first buffer that's available
        if (!mVideoBuffers[i].isDelivered)
        {
            ALOGVV("%s--", __FUNCTION__);
            return &mVideoBuffers[i];
        }
    }
    ALOGE("%s-- (no video buffers available!)", __FUNCTION__);
    return NULL;
}

// returns all "delivered" video buffers to DZ, should only be called
// when we are SURE that the app won't call releaseRecordingFrame on any
// more buffers
void NvCameraHal::ReclaimDeliveredVideoBuffers()
{
    NvU32 i;
    for (i = 0; i < NvCameraMemProfileConfigurator::MAX_NUMBER_OF_BUFFERS; i++)
    {
        if (mVideoBuffers[i].isDelivered)
        {
            SendEmptyVideoBufferToDZ((NvMMBuffer *)mVideoBuffers[i].OmxBufferHdr.pBuffer);
            mVideoBuffers[i].isDelivered = NV_FALSE;
        }
    }
}

NvError NvCameraHal::StopSmoothZoomInternal(NvBool RightNow)
{
    NvError e = NvSuccess;
    NvU32 DummyInt;

    if (!mSmoothZoomInProgress)
    {
        return e;
    }

    if (RightNow)
    {
        // Stop smooth zoom where it is by calling abort

        // Parameter value doesn't actually matter, but the param has to
        // be something reasonable (ie, non-NULL).
        DummyInt = 1;
        NV_CHECK_ERROR_CLEANUP(
            DZ.Block->SetAttribute(
                DZ.Block,
                NvMMDigitalZoomAttributeType_SmoothZoomAbort,
                0,
                sizeof(NvU32),
                &DummyInt)
        );

        // Since we are aborting zoom *right now*,
        // snap back to the last reported value.
        NV_CHECK_ERROR_CLEANUP(
            SetZoom(mSmoothZoomCurrentLevel, NV_FALSE)
        );

        mSmoothZoomTargetLevel = mSmoothZoomCurrentLevel;

        // Report that smooth zoom has "finished".
        // No need to update the settings parser, it was already updated
        // when we set mSmoothZoomCurrentLevel to this value in the first
        // place.
        if (mMsgEnabled & CAMERA_MSG_ZOOM)
        {
            NotifyCb(CAMERA_MSG_ZOOM, mSmoothZoomCurrentLevel, 1, mCbCookie);
        }
    }
    else
    {
        // Just set the flag
        mSmoothZoomStopping = NV_TRUE;
    }

    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::takePictureInternal()
{
    NvError e = NvSuccess;
    ALOGVV("%s++", __FUNCTION__);

    const NvCombinedCameraSettings &Settings =
        mSettingsParser.getCurrentSettings();

    // Android API requires preview active before takePicture
    // Here is one special case that we need preview off for raw capture case
    if (!((mMsgEnabled & CAMERA_MSG_RAW_IMAGE) ||
          (mMsgEnabled & CAMERA_MSG_RAW_IMAGE_NOTIFY)))
    {
        e = CheckAndWaitForCondition(
                !mPreviewStreaming && !mPreviewPaused,
                mFirstPreviewFrameReceived);
        if (e == NvError_Timeout)
        {
            ALOGE("%s-- (timed out waiting for preview to start)", __FUNCTION__);
            return e;
        }
    }

    if (mFastburstStarted)
    {
        ALOGVV("%s: signal SendJpegBuffer loop to send Jpeg in fastburst cont.shot", __FUNCTION__);
        NvOsSemaphoreSignal(mhSemPictureRequest);
        return e;
    }

    // When in nsl or vstab mode, buffers are set through
    // SetNSLNumBuffers or SetVSTABNumBuffers
    if (!Settings.nslNumBuffers && !Settings.videoStabOn)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetupLeanModeStillBuffer()
        );
    }

    NV_CHECK_ERROR_CLEANUP(
        ProgramExifInfo()
    );

    NV_CHECK_ERROR_CLEANUP(
        StartStillCapture()
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::StartStillCapture()
{
    NvError e = NvSuccess;
    NvMMCameraCaptureStillRequest Request;
    const NvCombinedCameraSettings &Settings =
            mSettingsParser.getCurrentSettings();

    ALOGVV("%s++", __FUNCTION__);

    mIsCapturing = NV_TRUE;

    if (UsingFlash())
    {
        if (!mIsTorchConverged)
        {
            NV_CHECK_ERROR_CLEANUP(
                SetupTorch()
            );
        }

        if (mPreviewStreaming)
        {
            // Wait for AE/AWB/AF to converge. AF not explicity covereged for video
            // snapshot and thus not to be waited for here.
            if (CheckAndWaitForCondition(!mGotAeComplete, mAELockReceived) != NvSuccess)
                ALOGE("%s: Warning: AE status update missing, continuing", __FUNCTION__);
            if (CheckAndWaitForCondition(!mGotAwbComplete, mAWBLockReceived) != NvSuccess)
                ALOGE("%s: Warning: AWB status update missing, continuing", __FUNCTION__);
            if (!mVideoEnabled && (CheckAndWaitForCondition(!mGotAfComplete, mAFLockReceived) !=
                NvSuccess))
                ALOGE("%s: Warning: AF status update missing, continuing", __FUNCTION__);
        }
        else
        {
            ALOGE("%s: Preview not streaming skipping wait for 3A convergence", __FUNCTION__);
        }
    }

    if (mFastburstEnabled)
    {
        ALOGVV("%s: begin burst capture for fastburst cont.shot", __FUNCTION__);
        mFastburstStarted = NV_TRUE;
        NvOsSemaphoreSignal(mhSemPictureRequest);
    }

    if (mDNGEnabled)
    {
        GetRawCaptureAttributes();
    }

    NvOsMemset(&Request, 0, sizeof(NvMMCameraCaptureStillRequest));

    mNumBracketPreviewRecieved = 0;
    mStillCaptureCount = Settings.burstCaptureCount + Settings.nslBurstCount;
    mStillCaptureCount *= mPostProcessStill->GetOutputCount();

    Request.NumberOfImages = Settings.burstCaptureCount * mPostProcessStill->GetInputCount();
    Request.BurstInterval = Settings.skipCount;
    Request.NslNumberOfImages = Settings.nslBurstCount * mPostProcessStill->GetInputCount();
    Request.NslPreCount = 0;
    // NvMM camera block has all of its timestamps in 100 ns units
    Request.NslTimestamp = mUsCaptureRequestTimestamp * 10;

    Request.Resolution.width = Settings.imageResolution.width;
    Request.Resolution.height = Settings.imageResolution.height;

    if (mIsPassThroughSupported)
    {
        NvU32 enablePassThrough = 0;
        enablePassThrough |= ENABLE_STILL_PASSTHROUGH;
        DZ.Block->SetAttribute(DZ.Block, NvMMDigitalZoomAttributeType_StillPassthrough,
                                    1, sizeof(NvU32), &enablePassThrough);
        Cam.Block->SetAttribute(Cam.Block, NvMMCameraAttribute_StillPassthrough,
                                    1, sizeof(NvU32), &enablePassThrough);
    }

    // NSL sends events directly from its capture path, so we should unlock
    // here to let them through.  (the NvMM machinery is messy, and causes
    // problems with our HAL threadsafety code, if we don't unlock/relock here)
    {
        UNLOCK_EVENTS();
        e = Cam.Block->Extension(
            Cam.Block,
            NvMMCameraExtension_CaptureStillImage,
            sizeof(NvMMCameraCaptureStillRequest),
            &Request,
            0,
            NULL);
        RELOCK_EVENTS();
    }
    if (e != NvSuccess)
    {
        mIsCapturing = NV_FALSE;
        goto fail;
    }

    ALOGVV("%s--", __FUNCTION__);
    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::StopStillCapture()
{
    NvError e = NvSuccess;
    NvMMCameraCaptureStillRequest Request;
    NvU32 RemainingStillImages = 0;

    ALOGVV("%s++", __FUNCTION__);
    NvOsMutexLock(mStillCaptureStopMutex);

    if (mStillCaptureCount == 0)
    {
        mIsCapturing = NV_FALSE;
        ALOGVV("%s-- (capture already stopped)", __FUNCTION__);
        NvOsMutexUnlock(mStillCaptureStopMutex);
        return e;
    }
    NvOsMemset(&Request, 0, sizeof(NvMMCameraCaptureStillRequest));
    NvOsMutexUnlock(mStillCaptureStopMutex);

    // During extension call, nvmm-camera can send some events
    // so we should unlock and relock to avoid the deadlock.
    UNLOCKED_EVENTS_CALL_CLEANUP(
        Cam.Block->Extension(
            Cam.Block,
            NvMMCameraExtension_CaptureStillImage,
            sizeof(NvMMCameraCaptureStillRequest),
            &Request, 0, NULL)
    );

    UNLOCKED_EVENTS_CALL_CLEANUP(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvMMCameraAttribute_RemainingStillImages,
            sizeof(RemainingStillImages),
            &RemainingStillImages)
    );

    NvOsMutexLock(mStillCaptureStopMutex);
    mStillCaptureCount -= RemainingStillImages;

    if (mStillCaptureCount <= 0)
    {
        mIsCapturing = NV_FALSE;
        ALOGVV("%s-- (capture already stopped)", __FUNCTION__);
        NvOsMutexUnlock(mStillCaptureStopMutex);
        return e;
    }

    mStillCaptureStop = NV_TRUE;
    NvOsMutexUnlock(mStillCaptureStopMutex);

    e = WaitForCondition(mStillCaptureFinished);
    if (e != NvSuccess)
    {
        ALOGE("%s-- (timed out waiting for still capture to finish)", __FUNCTION__);
        goto fail;
    }

    mIsCapturing = NV_FALSE;

    ALOGVV("%s--", __FUNCTION__);
    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

void NvCameraHal::SendJpegBuffer(
    void *args)
{
    NvCameraHal *This = (NvCameraHal *)args;
    NvError e = NvSuccess;
    // For Jpeg Encoder, we only use the launch stage value
    NvU32 jpegBufferNum;
    NV_CHECK_ERROR_CLEANUP(
        This->m_pMemProfileConfigurator->GetBufferAmount(
            NVCAMERA_BUFFERCONFIG_STAGE_LAUNCH, NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT,
            NULL, &jpegBufferNum)
    );

    ALOGVV("%s++", __FUNCTION__);

    // while JPEG Library context is present
    while (This->mImageEncoder != NULL)
    {
        NvMMBuffer *JpegBuffer = NULL;
        // Wait until encoded data is available
        NvOsSemaphoreWait(This->mhSemFilledOutputBufAvail);

        if (This->mFastburstStarted)
        {
            NvOsSemaphoreWait(This->mhSemPictureRequest);
            if (This->mMsgEnabled & CAMERA_MSG_SHUTTER)
            {
                ALOGVV("%s: sending shutter callback for fastburst cont.shot", __FUNCTION__);
                This->NotifyCb(CAMERA_MSG_SHUTTER, 0, 0, This->mCbCookie);
            }

        }

        // if JPEG Library is destroyed come out of the thread
        if (This->mImageEncoder == NULL)
            break;

        NV_CHECK_ERROR_CLEANUP(
            NvMMQueueDeQ(This->mFilledOutputBufferQueue, &JpegBuffer)
        );

        if(!(This->mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE))
        {
            ALOGE("%s: got output, but app doesn't want it",__FUNCTION__);
        }
        else
        {
            // alloc shmem
            This->mClientJpegBuffer =
                This->mGetMemoryCb(
                    -1,
                    JpegBuffer->Payload.Ref.sizeOfValidDataInBytes,
                    1,
                    This->mCbCookie);
            if (!This->mClientJpegBuffer)
            {
                ALOGE(
                    "%s: failed to allocate a shmem buffer for the JPEG!",
                    __FUNCTION__);
                return;
            }

#if ENABLE_NVIDIA_CAMTRACE
            TRACE_EVENT(__func__,
                    CAM_EVENT_TYPE_HAL_JPEG_ENCODE_EXIT,
                    NvOsGetTimeUS(),
                    2);
#endif
            // read from encoder output into shmem
#if ENABLE_NVIDIA_CAMTRACE
            TRACE_EVENT(__func__,
                    CAM_EVENT_TYPE_HAL_MEMCPY_ENTRY,
                    NvOsGetTimeUS(),
                    2);
#endif
            if(This->m_bSharedMemory)
            {
                NvRmMemRead(
                    (NvRmMemHandle)(JpegBuffer->Payload.Ref.hMem),
                    JpegBuffer->Payload.Ref.startOfValidData,
                    This->mClientJpegBuffer->data,
                    JpegBuffer->Payload.Ref.sizeOfValidDataInBytes);
            }
            else
            {
                // If allocated memory type is cached, NvRmMemRead can not be
                // used to read data, because in this case it simply gets allocated
                // using NvOsAlloc (see NvMMUtilAllocateBuffer Implementation).
                NvOsMemcpy(This->mClientJpegBuffer->data,
                    (NvU8 *)((NvU32)JpegBuffer->Payload.Ref.pMem +
                        JpegBuffer->Payload.Ref.startOfValidData),
                    JpegBuffer->Payload.Ref.sizeOfValidDataInBytes);
            }
#if ENABLE_NVIDIA_CAMTRACE
            TRACE_EVENT(__func__,
                    CAM_EVENT_TYPE_HAL_MEMCPY_EXIT,
                    NvOsGetTimeUS(),
                    2);
            TRACE_EVENT(__func__,
                    CAM_EVENT_TYPE_HAL_MEMCPY_ENTRY,
                    NvOsGetTimeUS(),
                    2);
#endif

            This->mHalJpegCallbackTime = NvOsGetTimeMS();
            This->mHalJpegLag = This->mHalJpegCallbackTime - This->mHalCaptureStartTime;
            ALOGI("mHalJpegLag = %d ms", This->mHalJpegLag);
            ALOGI(
                "Camera block takes %d ms, Jpeg Encoding takes %d ms",
                This->mHalStillYUVBufferFromDZLag,
                This->mHalJpegCallbackTime - This->mHalStillYUVBufferFromDZTime);

            // send shmem to app
            ALOGVV("%s: got output stream buffer, sending to app", __FUNCTION__);
            This->DataCb(
                CAMERA_MSG_COMPRESSED_IMAGE,
                This->mClientJpegBuffer,
                This->mCbCookie);

#if ENABLE_NVIDIA_CAMTRACE
            TRACE_EVENT(__func__,
                    CAM_EVENT_TYPE_HAL_MEMCPY_EXIT,
                    NvOsGetTimeUS(),
                    2);
            TRACE_EVENT(__func__,
                    CAM_EVENT_TYPE_HAL_TAKE_PICTURE_EXIT,
                    NvOsGetTimeUS(),
                    2);
#endif
        }

        NV_CHECK_ERROR_CLEANUP(
            NvMMQueueEnQ(This->mEmptyOutputBufferQueue, &JpegBuffer, 0)
        );

        NvOsSemaphoreSignal(This->mhSemEmptyOutputBufAvail);

        if (NvMMQueueGetNumEntries(This->mEmptyOutputBufferQueue) == jpegBufferNum)
        {
            This->mAreAllBuffersBack = NV_TRUE;
            This->mAllBuffersReturnedToQueue.signal();
        }
    }

    ALOGVV("%s--", __FUNCTION__);
    return;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return;
}

NvError NvCameraHal::OpenJpegEncoder()
{
    NvError e = NvSuccess;
    NvImageEncOpenParameters OpenParams;
    NvCombinedCameraSettings Settings = mSettingsParser.getCurrentSettings();
    // For Jpeg Encoder, we only use the launch stage value
    NvU32 jpegBufferNum;
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            NVCAMERA_BUFFERCONFIG_STAGE_LAUNCH, NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT,
            NULL, &jpegBufferNum)
    );

    ALOGVV("%s++", __FUNCTION__);

    if (mImageEncoder)
    {
        // close old one, to avoid resource leaks on double-open
        CloseJpegEncoder();
    }

    OpenParams.pContext = this;
    OpenParams.Type = NvImageEncoderType_HwEncoder;
    OpenParams.ClientIOBufferCB = JpegEncoderDeliverFullOutput;

    if (Settings.thumbnailEnable)
    {
        OpenParams.LevelOfSupport = PRIMARY_ENC | THUMBNAIL_ENC;
    }
    else
    {
        OpenParams.LevelOfSupport = PRIMARY_ENC;
    }

    NV_CHECK_ERROR_CLEANUP(
        NvImageEnc_Open(&mImageEncoder, &OpenParams)
    );

    // Create queue for output buffer from Encoder Callback
    NV_CHECK_ERROR_CLEANUP(
        NvMMQueueCreate(
            &mFilledOutputBufferQueue,
            jpegBufferNum,
            sizeof(NvMMBuffer*), NV_TRUE)
    );

    // create semaphore to signal availability of empty output buffer in the queue
    NV_CHECK_ERROR_CLEANUP(
        NvOsSemaphoreCreate(&mhSemEmptyOutputBufAvail, 0)
    );

    // create semaphore to signal availability of filled output buffer in the queue
    NV_CHECK_ERROR_CLEANUP(
        NvOsSemaphoreCreate(&mhSemFilledOutputBufAvail, 0)
    );

    // Create queue for output buffer from Encoder Callback
    NV_CHECK_ERROR_CLEANUP(
        NvMMQueueCreate(
            &mEmptyOutputBufferQueue,
            jpegBufferNum,
            sizeof(NvMMBuffer*), NV_TRUE)
    );

    // create thread to send JPEG encoded data back to Android layer
    NV_CHECK_ERROR_CLEANUP(
        NvOsThreadCreate(SendJpegBuffer, this, &mSendJpegDataThread)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    if (mSendJpegDataThread)
    {
        NvOsThreadJoin(mSendJpegDataThread);
        mSendJpegDataThread = NULL;
    }

    if (mhSemFilledOutputBufAvail)
    {
        NvOsSemaphoreDestroy(mhSemFilledOutputBufAvail);
        mhSemFilledOutputBufAvail = NULL;
    }

    if (mhSemEmptyOutputBufAvail)
    {
        NvOsSemaphoreDestroy(mhSemEmptyOutputBufAvail);
        mhSemEmptyOutputBufAvail = NULL;
    }

    if (mFilledOutputBufferQueue)
    {
        NvMMQueueDestroy(&mFilledOutputBufferQueue);
        mFilledOutputBufferQueue = NULL;
    }

    if (mEmptyOutputBufferQueue)
    {
        NvMMQueueDestroy(&mEmptyOutputBufferQueue);
        mEmptyOutputBufferQueue = NULL;
    }
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;

}

NvError NvCameraHal::AllocateJpegOutputBuffers(NvJpegEncParameters &Params)
{
    NvError e = NvSuccess;
    NvU32 i = 0;
    ALOGVV("%s++", __FUNCTION__);

    // For Jpeg Encoder, we only use the launch stage value
    NvU32 jpegBufferNum;
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            NVCAMERA_BUFFERCONFIG_STAGE_LAUNCH, NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT,
            NULL, &jpegBufferNum)
    );

    // Set m_bSharedMemory to false to make output buffer of
    // cached type to improve jpeg KPIs.
    m_bSharedMemory = NV_FALSE;

    // allocate output buffers, make sure old buffers are freed before
    // wait to alloc the client one until just before sending the callback,
    // or else we can't guarantee that it would be freed
    for (i = 0; i < jpegBufferNum; i++)
    {
        if (!mImageEncoderOutput[i])
        {
            mImageEncoderOutput[i] = (NvMMBuffer *)NvOsAlloc(sizeof(NvMMBuffer));
        }

        if (mMemProfilePrintEnabled)
        {
            //Jpeg output doesn't have any specific ports or components. The memory profiling script
            //  right now is looking for these information for parsing data. So these informations
            //  are made up to make the script work.
            //port: assigned to 0 since it's the first buffer this kind we track;
            //component: COMPONENT_NUMBER_OF_COMPONENTS since it's always the biggest defined component
            //  number which is not used.
            NV_DEBUG_MSG("BufferType - Port: %d, Component: %d, SurfaceType: %d, Height: %d, Width: %d ",
                0, COMPONENT_NUMBER_OF_COMPONENTS,
                NvColorFormat_Y8,   //Since InputFormat above is hardcoded, we hardcoded the surface type too
                Params.Resolution.height,
                Params.Resolution.width);

            NV_DEBUG_MSG("%s (Jpeg): %d, %d ", MEM_PROFILE_TAG, Params.EncodedSize, JPEG_ENCODER_MEM_ALIGN);
        }

        NV_CHECK_ERROR_CLEANUP(
            NvMMUtilAllocateBuffer(
                mRmDevice,
                Params.EncodedSize,
                JPEG_ENCODER_MEM_ALIGN,
                NvMMMemoryType_SYSTEM,
                m_bSharedMemory,
                &mImageEncoderOutput[i])
        );
        NV_CHECK_ERROR_CLEANUP(
            NvMMQueueEnQ(
                mEmptyOutputBufferQueue,
                &mImageEncoderOutput[i],
                0)
        );
        NvOsSemaphoreSignal(mhSemEmptyOutputBufAvail);
    }
    ALOGVV("%s allocates %d Jpeg Buffers", __FUNCTION__, jpegBufferNum);

    if (NvMMQueueGetNumEntries(mEmptyOutputBufferQueue) == jpegBufferNum)
    {
        mAreAllBuffersBack = NV_TRUE;
        mAllBuffersReturnedToQueue.signal();
    }

    ALOGVV("%s--", __FUNCTION__);
    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;

}

NvError NvCameraHal::FreeJpegOutputBuffers()
{
    NvU32 i = 0;
    NvError e = NvSuccess;
    // For Jpeg Encoder, we only use the launch stage value
    NvU32 jpegBufferNum;
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            NVCAMERA_BUFFERCONFIG_STAGE_LAUNCH, NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT,
            NULL, &jpegBufferNum)
    );

    ALOGVV("%s++", __FUNCTION__);

    if (mImageEncoderOutput[0])
    {
        //wait until all of the buffers have been returned
        NV_CHECK_ERROR_CLEANUP(
            CheckAndWaitForCondition(
                !mAreAllBuffersBack,
                mAllBuffersReturnedToQueue
            )
        );

        // decrement the semaphore after all the buffers have
        // been returned so that the count goes to 0
        while (NvMMQueueGetNumEntries(mEmptyOutputBufferQueue))
        {
            NvMMBuffer *JpegOutputBuffer;
            NV_CHECK_ERROR_CLEANUP(
                NvMMQueueDeQ(mEmptyOutputBufferQueue, &JpegOutputBuffer)
            );
            NvOsSemaphoreWait(mhSemEmptyOutputBufAvail);
        }
    }

    for (i = 0; i < jpegBufferNum; i++)
    {
        NvMMUtilDeallocateBuffer(mImageEncoderOutput[i]);
        NvOsFree(mImageEncoderOutput[i]);
        mImageEncoderOutput[i] = NULL;
    }

    ALOGVV("%s--", __FUNCTION__);
    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;

}

static NvMMExif_Orientation
GetOrientation(
    NvU16 rotation)
{
    NvMMExif_Orientation orientation;
    // We don't need to consider flip here because flip is always
    // done physically by DZ block
    switch(rotation)
    {
        case 90:
            orientation = NvMMExif_Orientation_270_Degrees;
            break;
        case 180:
            orientation = NvMMExif_Orientation_180_Degrees;
            break;
        case 270:
            orientation = NvMMExif_Orientation_90_Degrees;
            break;
        case 0:
        default:
            orientation = NvMMExif_Orientation_0_Degrees;
    }
    return orientation;
}

NvError NvCameraHal::GetDzStillOutputSurfaceType(void)
{
    NvError e = NvSuccess;
    NvMMNewBufferConfigurationInfo BufCfg;
    NvBufferOutputLocation location;

    location.SetLocation(COMPONENT_DZ, DZ_OUT_STILL);
    NV_CHECK_ERROR(NV_SHOW_ERROR(
                m_pBufferStream->GetOutputPortBufferCfg(location, &BufCfg)));

    NvMMVideoFormat *pVideoFormat = &BufCfg.format.videoFormat;
    NvRmSurface *pSurf = &pVideoFormat->SurfaceDescription[0];
    NvU32 NumOfSurfaces = pVideoFormat->NumberOfSurfaces;

    switch (NumOfSurfaces)
    {
        // currently we only consider 2 and 3 surfaces cases
        case 0:
        case 1:
            mDzStillOutputSurfaceType = NVMM_FRAME_FormatInvalid;
            NV_ASSERT(!"Wrong Format!");
            break;
        case 2:
            if (pSurf[0].ColorFormat == NvColorFormat_Y8 &&
                    pSurf[1].ColorFormat == NvColorFormat_U8_V8)
                mDzStillOutputSurfaceType = NVMM_STILL_FRAME_FormatNV12;
            else
            {
                mDzStillOutputSurfaceType = NVMM_FRAME_FormatInvalid;
                NV_ASSERT(!"Wrong Format!");
            }
            break;
        case 3:
            if (pSurf[0].ColorFormat == NvColorFormat_Y8 &&
                    pSurf[1].ColorFormat == NvColorFormat_U8 &&
                    pSurf[2].ColorFormat == NvColorFormat_V8)
                mDzStillOutputSurfaceType = NVMM_STILL_FRAME_FormatYV12;
            else
            {
                mDzStillOutputSurfaceType = NVMM_FRAME_FormatInvalid;
                NV_ASSERT(!"Wrong Format!");
            }
            break;
        default:
            mDzStillOutputSurfaceType = NVMM_FRAME_FormatInvalid;
            NV_ASSERT(!"Wrong Format!");
            break;
    }

    if (mDzStillOutputSurfaceType == NVMM_FRAME_FormatInvalid)
        e = NvError_BadParameter;

    return e;
}

NvError NvCameraHal::SetupJpegEncoderStill()
{
    NvError e = NvSuccess;
    NvJpegEncParameters Params;
    const NvCombinedCameraSettings &Settings =
        mSettingsParser.getCurrentSettings();
    NvS32 physicalRotation = 0;
    ALOGVV("%s++", __FUNCTION__);

    NvOsMemset(&Params, 0, sizeof(NvJpegEncParameters));

    // set parameters
    Params.Quality = Settings.imageQuality;
    Params.InputFormat = NV_IMG_JPEG_COLOR_FORMAT_420;

    if (mDzStillOutputSurfaceType == NVMM_STILL_FRAME_FormatNV12)
        Params.EncSurfaceType = NV_IMG_JPEG_SURFACE_TYPE_NV12;
    else
        Params.EncSurfaceType = NV_IMG_JPEG_SURFACE_TYPE_YV12;

    // DZ block will rotate image physically
    if (Settings.autoRotation)
    {
        physicalRotation = (NvS32) Settings.imageRotation;
        Params.Orientation.eRotation = NvJpegImageRotate_Physically;
        Params.Orientation.orientation = NvMMExif_Orientation_0_Degrees;
    }
    else // Rotate image by exif tag
    {
        physicalRotation = 0;
        Params.Orientation.eRotation = NvJpegImageRotate_ByExifTag;
        Params.Orientation.orientation = GetOrientation(Settings.imageRotation);
    }

    if ((physicalRotation == 90)||(physicalRotation == 270))
    {
        Params.Resolution.width = Settings.imageResolution.height;
        Params.Resolution.height = Settings.imageResolution.width;
    }
    else
    {
        Params.Resolution.width = Settings.imageResolution.width;
        Params.Resolution.height = Settings.imageResolution.height;
    }

    // bug in encoder library, this needs to be false to tell it
    // that we're setting still params, even if we enable thumbnail eventually
    Params.EnableThumbnail = NV_FALSE;

    // TODO this sets the Make and Model correctly as verified by a Get after this
    // set but the exif data in jpeg file is still messed up.
    NvOsMemcpy(
        Params.EncParamExif.ExifInfo.Make,
        Settings.exifMake,
        EXIF_STRING_LENGTH);
    NvOsMemcpy(
        Params.EncParamExif.ExifInfo.Model,
        Settings.exifModel,
        EXIF_STRING_LENGTH);

    NV_CHECK_ERROR_CLEANUP(
        NvImageEnc_SetParam(mImageEncoder, &Params)
    );

    // get new output size from encoder
    NV_CHECK_ERROR_CLEANUP(
        NvImageEnc_GetParam(mImageEncoder, &Params)
    );

    NvOsMutexLock(mStillCaptureMutex);

    NV_CHECK_ERROR_CLEANUP(
        FreeJpegOutputBuffers()
    );
    NV_CHECK_ERROR_CLEANUP(
        AllocateJpegOutputBuffers(Params)
    );

    NvOsMutexUnlock(mStillCaptureMutex);

    ALOGVV("%s--", __FUNCTION__);
    return e;
fail:

    NvOsMutexUnlock(mStillCaptureMutex);
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::SetupJpegEncoderThumbnail()
{
    NvError e = NvSuccess;
    NvJpegEncParameters Params;
    const NvCombinedCameraSettings &Settings =
        mSettingsParser.getCurrentSettings();
    NvS32 physicalRotation = 0;
    ALOGVV("%s++", __FUNCTION__);

    NvOsMemset(&Params, 0, sizeof(NvJpegEncParameters));
    // not because we're enabling it, but because
    // these settings are for thumbnail
    Params.EnableThumbnail = NV_TRUE;

    if (mDzStillOutputSurfaceType == NVMM_STILL_FRAME_FormatNV12)
        Params.EncSurfaceType = NV_IMG_JPEG_SURFACE_TYPE_NV12;
    else
        Params.EncSurfaceType = NV_IMG_JPEG_SURFACE_TYPE_YV12;

    // DZ block will rotate image physically
    if (Settings.autoRotation)
    {
        physicalRotation = (NvS32) Settings.imageRotation;
        Params.Orientation.eRotation = NvJpegImageRotate_Physically;
        Params.Orientation.orientation = NvMMExif_Orientation_0_Degrees;
    }
    else // Rotate image by exif tag
    {
        physicalRotation = 0;
        Params.Orientation.eRotation = NvJpegImageRotate_ByExifTag;
        Params.Orientation.orientation = GetOrientation(Settings.imageRotation);
    }

    // Thumbnail rotate with orientation
    if (((physicalRotation == 90)||(physicalRotation == 270))
        && NV_FALSE) // For the CTS WAR the rotation is handled in the parser
    {
        Params.Resolution.width = Settings.thumbnailResolution.height;
        Params.Resolution.height = Settings.thumbnailResolution.width;
    }
    else
    {
        Params.Resolution.width = Settings.thumbnailResolution.width;
        Params.Resolution.height = Settings.thumbnailResolution.height;
    }

    Params.Quality = Settings.thumbnailQuality;

    NV_CHECK_ERROR_CLEANUP(
        NvImageEnc_SetParam(mImageEncoder, &Params)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::FeedJpegEncoder(NvMMBuffer *InputBuffer)
{
    NvError e = NvSuccess;
    NvMMBuffer *Buffer = NULL;
    NvMMBuffer *ThumbnailBuffer = NULL;
    const NvCombinedCameraSettings &Settings =
        mSettingsParser.getCurrentSettings();
    NvS32 physicalRotation = (NvS32)
        (Settings.autoRotation ? Settings.imageRotation : 0);

    // copy before sending to jpeg encoder
    // the copy buffer will get freed when the jpeg encoder sends us the pointer
    // back via its JpegEncoderDeliverFullOutput func.
    // we need to copy because jpeg runs on a separate thread, and NvMM will
    // reuse the pointer that it used to send us the original buffer as soon
    // as control returns to it from the NvMMDeliverFullOutput func
    Buffer = (NvMMBuffer *)NvOsAlloc(sizeof(NvMMBuffer));
    if (!Buffer)
    {
        ALOGE("%s: failed to allocate jpeg copy buffer", __FUNCTION__);
        goto fail;
    }

    NvOsMemcpy(Buffer, InputBuffer, sizeof(NvMMBuffer));

    // WARNING:
    // always feed still first, to work around known issues with the ordering
    // of thumbnail/still in the encoder lib
    FeedJpegEncoderStill(Buffer);

    // alloc thumbnail buffer and surfaces, these will also be freed when the
    // jpeg encoder returns them to us
    if (Settings.thumbnailEnable)
    {
        ThumbnailBuffer = (NvMMBuffer *)NvOsAlloc(sizeof(NvMMBuffer));
        if (!ThumbnailBuffer)
        {
            ALOGE("%s: failed to allocate jpeg thumbnail buffer", __FUNCTION__);
            goto fail;
        }

        // initialize thumbnail buffer
        NvOsMemset(ThumbnailBuffer, 0, sizeof(NvMMBuffer));
        ThumbnailBuffer->StructSize = sizeof(NvMMBuffer);
        ThumbnailBuffer->PayloadType = NvMMPayloadType_SurfaceArray;
        // we think all other fields (except surfaces) don't matter for thumbnail,
        // but if the encoder decides to use them for anything, we'll need to
        // init them properly here

        // alloc surfaces
        // thumbnail rotate with orientation
        if (((physicalRotation == 90)||(physicalRotation == 270))
            && NV_FALSE) // For the CTS WAR the rotation is handled in the parser
        {
            if (mDzStillOutputSurfaceType == NVMM_STILL_FRAME_FormatNV12)
                NV_CHECK_ERROR_CLEANUP(
                        mScaler.AllocateNV12NvmmSurface(
                            &ThumbnailBuffer->Payload.Surfaces,
                            (NvU32)Settings.thumbnailResolution.height,
                            (NvU32)Settings.thumbnailResolution.width)
                        );
            else
                NV_CHECK_ERROR_CLEANUP(
                        mScaler.AllocateYuv420NvmmSurface(
                            &ThumbnailBuffer->Payload.Surfaces,
                            (NvU32)Settings.thumbnailResolution.height,
                            (NvU32)Settings.thumbnailResolution.width)
                        );
        }
        else
        {
            if (mDzStillOutputSurfaceType == NVMM_STILL_FRAME_FormatNV12)
                NV_CHECK_ERROR_CLEANUP(
                        mScaler.AllocateNV12NvmmSurface(
                            &ThumbnailBuffer->Payload.Surfaces,
                            (NvU32)Settings.thumbnailResolution.width,
                            (NvU32)Settings.thumbnailResolution.height)
                        );
            else
                NV_CHECK_ERROR_CLEANUP(
                        mScaler.AllocateYuv420NvmmSurface(
                            &ThumbnailBuffer->Payload.Surfaces,
                            (NvU32)Settings.thumbnailResolution.width,
                            (NvU32)Settings.thumbnailResolution.height)
                        );
        }

        // scale full buffer into it
        mScaler.Scale(
            &Buffer->Payload.Surfaces,
            &ThumbnailBuffer->Payload.Surfaces);

        // send to encoder
        FeedJpegEncoderThumbnail(ThumbnailBuffer);
    }

    return e;

fail:
    NvOsFree(Buffer);
    if (ThumbnailBuffer)
    {
        mScaler.FreeSurface(&ThumbnailBuffer->Payload.Surfaces);
    }
    NvOsFree(ThumbnailBuffer);
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::FeedJpegEncoderStill(NvMMBuffer *Buffer)
{
    NvError e = NvSuccess;
    NvMMBuffer *JpegOutputBuffer;

    ALOGVV("%s++", __FUNCTION__);

    NvOsMutexLock(mStillCaptureMutex);

    NvOsSemaphoreWait(mhSemEmptyOutputBufAvail);
    NV_CHECK_ERROR_CLEANUP(
        NvMMQueueDeQ(mEmptyOutputBufferQueue, &JpegOutputBuffer)
    );

    mAreAllBuffersBack = NV_FALSE;

#if ENABLE_NVIDIA_CAMTRACE
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_HAL_JPEG_ENCODE_ENTRY,
            NvOsGetTimeUS(),
            3);
#endif
    NV_CHECK_ERROR_CLEANUP(
        NvImageEnc_Start(
            mImageEncoder,
            Buffer,
            JpegOutputBuffer,
            NV_FALSE)
    );

    NvOsMutexUnlock(mStillCaptureMutex);

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    NvOsMutexUnlock(mStillCaptureMutex);

    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::FeedJpegEncoderThumbnail(NvMMBuffer *Buffer)
{
    NvError e = NvSuccess;
    ALOGVV("%s++", __FUNCTION__);

    e = NvImageEnc_Start(
            mImageEncoder,
            Buffer,
            // as long as we send the still buffer first and give an output
            // with that, jpeg lib says this can be NULL
            NULL,
            NV_TRUE);
    ALOGVV("%s--", __FUNCTION__);
    return e;
}

NvError NvCameraHal::CloseJpegEncoder()
{
    NvError e = NvSuccess;
    ALOGVV("%s++", __FUNCTION__);

    NvOsMutexLock(mStillCaptureMutex);

#if ENABLE_NVIDIA_CAMTRACE
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_HAL_CLOSE_ENCODER_ENTRY,
            NvOsGetTimeUS(),
            3);
#endif

    NV_CHECK_ERROR_CLEANUP(
        FreeJpegOutputBuffers()
    );

    NvImageEnc_Close(mImageEncoder);
    mImageEncoder = NULL;

    if (mhSemFilledOutputBufAvail)
    {
        NvOsSemaphoreSignal(mhSemFilledOutputBufAvail);
    }

    if (mSendJpegDataThread)
    {
        NvOsThreadJoin(mSendJpegDataThread);
        mSendJpegDataThread = NULL;
    }

    if (mhSemFilledOutputBufAvail)
    {
        NvOsSemaphoreDestroy(mhSemFilledOutputBufAvail);
        mhSemFilledOutputBufAvail = NULL;
    }

    if (mhSemEmptyOutputBufAvail)
    {
        NvOsSemaphoreDestroy(mhSemEmptyOutputBufAvail);
        mhSemEmptyOutputBufAvail = NULL;
    }

    if (mFilledOutputBufferQueue)
    {
        NvMMQueueDestroy(&mFilledOutputBufferQueue);
        mFilledOutputBufferQueue = NULL;
    }

    if (mEmptyOutputBufferQueue)
    {
        NvMMQueueDestroy(&mEmptyOutputBufferQueue);
        mEmptyOutputBufferQueue = NULL;
    }

#if ENABLE_NVIDIA_CAMTRACE
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_HAL_CLOSE_ENCODER_EXIT,
            NvOsGetTimeUS() ,
            3);
#endif

    NvOsMutexUnlock(mStillCaptureMutex);

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    NvOsMutexUnlock(mStillCaptureMutex);
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;

}

void NvCameraHal::EventsUpdateCameraBlock(void *args)
{
    NvCameraHal *This = (NvCameraHal *)args;
    APILock lock(This);
    const NvCombinedCameraSettings &settings =
        This->mSettingsParser.getCurrentSettings();
    NvError e = NvSuccess;

    ALOGVV("%s++ , serving event 0x%x",
        __FUNCTION__, This->mEventTypeUpdatingCameraBlock);

    switch (This->mEventTypeUpdatingCameraBlock)
    {
        case NvMMDigitalZoomEvents_SmoothZoomFactor:
            // Update AE/AF regions when dzoom changed
            e = This->SetExposureWindows(settings);
            if (e != NvSuccess)
                ALOGE("Update AE region in DZ thread fail! Error: 0x%x", e);

            e = This->SetFocusWindows(settings);
            if (e != NvSuccess)
                ALOGE("Update AF region in DZ thread fail! Error: 0x%x", e);
            if (This->mSmoothZoomStopping &&
                (This->mSmoothZoomTargetLevel == This->mSmoothZoomCurrentLevel))
            {
                This->mSmoothZoomStopping = NV_FALSE;
                This->mSmoothZoomDoneStopping.signal();
                e = This->SetZoom(This->mSmoothZoomTargetLevel, NV_FALSE);
                if (e != NvSuccess)
                    ALOGE("Update zoom in DZ thread fail! Error: 0x%x", e);
            }
            break;
        case NvMMDigitalZoomEvents_FaceInfo:
            e = This->Program3AForFaces(This->mpFaces, This->mNumFaces);
            if (e != NvSuccess)
            {
                ALOGE("Program3AForFaces failed in the %s thread! Error: 0x%x",
                    __FUNCTION__, e);
            }
            NvOsFree(This->mpFaces);
            This->mpFaces = NULL;
            break;
        case NvMMEventCamera_StillCaptureReady:
            e = This->UnlockAeAwbAfterTorch();
            if (e != NvSuccess)
                ALOGE("Reset torch failed! Error: 0x%x", e);
            break;
        default:
            ALOGE(
                "Unexpected Event 0x%x is using UpdateCameraBlock thread!",
                This->mEventTypeUpdatingCameraBlock);
            NV_ASSERT(0);
            break;
    }
    ALOGVV("%s--", __FUNCTION__);
}

void NvCameraHal::GetRawCaptureAttributes()
{
    mRawCaptureCCM3000[0] = 3000;
    mRawCaptureCCM6500[0] = 6500;
    if ((Cam.Block->GetAttribute(Cam.Block, NvCameraIspAttribute_ColorCalibrationMatrix,
         sizeof(mRawCaptureCCM3000), &mRawCaptureCCM3000) != NvSuccess) ||
        (Cam.Block->GetAttribute(Cam.Block, NvCameraIspAttribute_ColorCalibrationMatrix,
         sizeof(mRawCaptureCCM6500), &mRawCaptureCCM6500) != NvSuccess))
    {
        for(int i=0; i<16; i++) // fall back to 4x4 identity
        {
            mRawCaptureCCM3000[i] =
            mRawCaptureCCM6500[i] = (i&3) == (i>>2) ? 1.0 : 0.0;
        }
    }

    mRawCaptureGain = 1.0;
    NvCameraIspAeHwSettings ae;
    if (Cam.Block->GetAttribute(Cam.Block, NvCameraIspAttribute_AeHwSettings,
                                sizeof(ae), &ae) == NvSuccess)
    {
        mRawCaptureGain = ae.AnalogGain;
    }

    mRawCaptureCCT = 0;
    Cam.Block->GetAttribute(Cam.Block, NvMMCameraAttribute_Illuminant,
                            sizeof(NvF32), &mRawCaptureCCT);
    if (mRawCaptureCCT == 0)
        mRawCaptureCCT = 5000; // fall back default

    Cam.Block->GetAttribute(Cam.Block, NvMMCameraAttribute_Illuminant,
                            sizeof(NvF32)*4, &mRawCaptureWB);
}

NvBool NvCameraHal::CheckCameraSceneBrightness(void)
{
    NvF32 sceneBrightness;
    NvError e = NvSuccess;
    NvBool needFlash = NV_FALSE;

    ALOGVV("%s++", __FUNCTION__);

    UNLOCKED_EVENTS_CALL_CLEANUP(
        Cam.Block->GetAttribute(
            Cam.Block,
            NvCameraIspAttribute_SceneBrightness,
            sizeof(sceneBrightness),
            &sceneBrightness)
    );

    if (sceneBrightness >= 0.0f)
    {
        UNLOCKED_EVENTS_CALL_CLEANUP(
            Cam.Block->GetAttribute(
                Cam.Block,
                NvCameraIspAttribute_FlashNeeded,
                sizeof(needFlash),
                &needFlash)
        );
    }

    ALOGVV("%s--", __FUNCTION__);
    return needFlash;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return NV_FALSE;
}

NvBool NvCameraHal::UsingFlash(void)
{
    //Flash is fundamentally incompatible with NSL
    if (mSettingsParser.getCurrentSettings().nslBurstCount > 0)
    {
        return NV_FALSE;
    }
    switch (mSettingsParser.getCurrentSettings().flashMode)
    {
        case NvCameraFlashMode_Auto:
            return CheckCameraSceneBrightness();

        case NvCameraFlashMode_On:
        case NvCameraFlashMode_RedEyeReduction:
            return NV_TRUE;

        case NvCameraFlashMode_Off:
        default:
            return NV_FALSE;
    }
}

NvError NvCameraHal::SetupTorch()
{
    NvU32 nvFlashMode = NVMMCAMERA_FLASHMODE_OFF;
    NvError e = NvSuccess;
    NvCombinedCameraSettings settings = mSettingsParser.getCurrentSettings();
    NvCameraUserFlashMode userFlashMode = settings.flashMode;
    NvU32 afSubType = NvMMCameraAlgorithmSubType_AFFullRange;
    NvU32 algLockTimeout = ALG_LOCK_TIMEOUT_LONG;
    NvU32 algLocks = 0;

    ALOGVV("%s++", __FUNCTION__);

    if (settings.stillCapHdrOn)
    {
        nvFlashMode = NVMMCAMERA_FLASHMODE_OFF;
    }
    else
    {
        NV_CHECK_ERROR_CLEANUP(
            NvCameraUserFlashModeToNvMMFlashMode(userFlashMode, nvFlashMode)
        );

        // In video mode turn on torch for capture, when done with capture
        // it will be set to user selection.
        if (mVideoEnabled)
        {
            nvFlashMode = NVMMCAMERA_FLASHMODE_TORCH;
        }
   }

    NV_CHECK_ERROR_CLEANUP(
        Cam.Block->SetAttribute(
            Cam.Block,
            NvMMCameraAttribute_FlashMode,
            0,
            sizeof(NvU32),
            &nvFlashMode)
    );

    if (settings.focusMode == NvCameraFocusMode_Infinity)
    {
        afSubType = NvMMCameraAlgorithmSubType_AFInfMode;
    }
    else if (settings.focusMode == NvCameraFocusMode_Macro)
    {
        afSubType = NvMMCameraAlgorithmSubType_AFMacroMode;
    }

    // This has to do with backwards compatibility...previous Android
    // API designs required AF to implicitly lock AWB/AE
    // if we wanted a more traditional halfpress behavior.
    // AE/AWB/AF need to re-converge for flash computation to work.
    mGotAeComplete = NV_FALSE;
    mGotAwbComplete = NV_FALSE;
    mGotAfComplete = NV_FALSE;

    // if we are converging in video mode timeout needs to be short
    // in order not to block the return of video frames
    if (mVideoEnabled)
    {
        algLockTimeout = ALG_LOCK_TIMEOUT_VIDEO_SNAPSHOT;
        algLocks = AlgLock_AE | AlgLock_AWB;
    }
    else
    {
        algLockTimeout = ALG_LOCK_TIMEOUT_LONG;
        algLocks = AlgLock_AE | AlgLock_AWB | AlgLock_AF;
    }

    // AE/AWB is locked by flash here. They need to be unlocked either in
    // cancelAutoFocus or after snapshot (stillCaptureReady event)
    mAeAwbUnlockNeeded = NV_TRUE;
    NV_CHECK_ERROR_CLEANUP(
        programAlgLock(
            algLocks,
            NV_TRUE,
            NV_FALSE,
            algLockTimeout,
            afSubType)
    );

    mIsTorchConverged = NV_TRUE;
    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::AutoFocusInternal()
{
    NvError e = NvSuccess;
    NvBool focusSupported = mSettingsParser.isFocusSupported();
    NvCombinedCameraSettings settings = mSettingsParser.getCurrentSettings();
    NvCameraUserFocusMode fMode = settings.focusMode;
    NvCameraIspFocusingParameters focusParameters;
    NvBool FlashEnabled = UsingFlash();

    // if focus not supported
    // send notify CB CAMERA_MSG_FOCUS with a "converged" event if in fixed mode
    // otherwise send a failure
    if (!focusSupported)
    {
        NotifyCb(
            CAMERA_MSG_FOCUS,
            (fMode == NvCameraFocusMode_Fixed),
            0,
            mCbCookie);
        return NvSuccess;
    }



    /*
     * Macro Mode and Auto Mode -> AF converge and lock
     * Continous Mode (video and picture)
     *      if CAF converged    -> Lock CAF and send callback
     *      else                -> AF converge and lock
     *
     * Infinity Mode            -> Send callback immediately
     * Fixed Mode               -> Send callback immediately
     * Hyperfocal Mode          -> Send callback immediately
     */
    switch (fMode)
    {
        case NvCameraFocusMode_Macro:
        case NvCameraFocusMode_Auto:
            {
                // flash is expected only for autofocus, not for fixed focus.
                if (FlashEnabled)
                {
                    NV_CHECK_ERROR_CLEANUP(
                        SetupTorch());
                }
                NV_CHECK_ERROR_CLEANUP(
                    StartAutoFocus(FlashEnabled)
                );
            }
        break;
        case NvCameraFocusMode_Continuous_Video:
            // fall-through intentional here, video and picture have same
            // behavior when autoFocus is called in continuous mode
        case NvCameraFocusMode_Continuous_Picture:
        {
            // flash is expected only for autofocus, not for fixed focus.
            if (FlashEnabled)
            {
                NV_CHECK_ERROR_CLEANUP(
                    SetupTorch());
            }

            NvU32 focusState;
            // query state of focus
            NV_CHECK_ERROR_CLEANUP(
                Cam.Block->GetAttribute(
                    Cam.Block,
                    NvCameraIspAttribute_ContinuousAutoFocusState,
                    sizeof(focusState),
                    &focusState)
            );
            if (FlashEnabled || focusState != NVCAMERAISP_CAF_CONVERGED)
            {   // CAF under ambient light is ignored when torch is expected
                NV_CHECK_ERROR_CLEANUP(
                    StartAutoFocus(FlashEnabled)
                );
            }
            else
            {
                // CAF already converged: pause current state of focus
                NvBool lockCAF = NV_TRUE;
                NV_CHECK_ERROR_CLEANUP(
                    Cam.Block->SetAttribute(
                        Cam.Block,
                        NvCameraIspAttribute_ContinuousAutoFocusPause,
                        0,
                        sizeof(lockCAF),
                        &lockCAF)
                );
                NotifyCb(CAMERA_MSG_FOCUS, 1, 0, mCbCookie);
            }
        }
        break;
        case NvCameraFocusMode_Fixed:
        case NvCameraFocusMode_Infinity:
        case NvCameraFocusMode_Hyperfocal: // Depreciated from Android API as of 7/24,2013
            NotifyCb(CAMERA_MSG_FOCUS, 1, 0, mCbCookie);
            break;
        default:
            // handle error for unsupported mode
            ALOGE(
                "%s: called with invalid focus mode 0x%x",
                __FUNCTION__,
                settings.focusMode);
            goto fail;
            break;
    }
    return e;

fail:
    NotifyCb(CAMERA_MSG_FOCUS, 0, 0, mCbCookie);
    return e;
}

NvError NvCameraHal::CancelAutoFocusInternal()
{
    NvError e = NvSuccess;
    NvCameraUserFocusMode fMode;
    NvBool lockCAF = NV_FALSE;

    ALOGVV("%s++", __FUNCTION__);
    mGotAfComplete = NV_FALSE;

    if (mAeAwbUnlockNeeded)
    {
        // AE/AWB might be locked by torch during AutoFocus()
        // If it is the case, unlock them now
        NV_CHECK_ERROR_CLEANUP(
            UnlockAeAwbAfterTorch()
        );
        mAeAwbUnlockNeeded = NV_FALSE;
    }

    fMode = mSettingsParser.getCurrentSettings().focusMode;
    switch(fMode)
    {
        case NvCameraFocusMode_Auto:
        case NvCameraFocusMode_Macro:
            NV_CHECK_ERROR_CLEANUP(
                StopAutoFocus()
            );
            break;
        case NvCameraFocusMode_Continuous_Video:
        case NvCameraFocusMode_Continuous_Picture:
            NV_CHECK_ERROR_CLEANUP(
                StopAutoFocus()
            );
            NV_CHECK_ERROR_CLEANUP(
                Cam.Block->SetAttribute(
                    Cam.Block,
                    NvCameraIspAttribute_ContinuousAutoFocusPause,
                    0,
                    sizeof(lockCAF),
                    &lockCAF)
            );
            break;
        default:
            // Do nothing if focus is fixed, macro, or infinity
            break;
    }
    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::StartAutoFocus(NvBool FlashEnabled)
{
    NvError e = NvSuccess;
    NvCombinedCameraSettings Settings = mSettingsParser.getCurrentSettings();

    mGotAfComplete  = NV_FALSE;

    if (!FlashEnabled)
    {
        mGotAeComplete = Settings.AELocked;
        mGotAwbComplete = Settings.AWBLocked;
        e = programAlgLock(
            AlgLock_AF,
            NV_TRUE,
            NV_FALSE,
            ALG_LOCK_TIMEOUT_SHORT,
            NvMMCameraAlgorithmSubType_AFFullRange);
        if (e != NvSuccess)
        {
            ALOGE("%s: Autofocus Lock failed 0x%x", __FUNCTION__, e);
        }
    }
    return e;
}

// AE/AWB might be locked by torch during AutoFocus() or TakePicture()
// This function will unlock AE/AWB and reset related flags
NvError NvCameraHal::UnlockAeAwbAfterTorch()
{
    NvError e = NvSuccess;

    mGotAeComplete  = NV_FALSE;
    mGotAwbComplete = NV_FALSE;
    mIsTorchConverged = NV_FALSE;

    e = programAlgLock(
        AlgLock_AE | AlgLock_AWB,
        NV_FALSE,
        NV_FALSE,
        ALG_UNLOCK_TIMEOUT,
        NvMMCameraAlgorithmSubType_None);
    if (e != NvSuccess)
    {
        ALOGE("%s: AE/AWB Unlock failed 0x%x", __FUNCTION__, e);
    }

    return e;
}

NvError NvCameraHal::StopAutoFocus()
{
    NvError e = NvSuccess;
    mGotAfComplete = NV_FALSE;

    e = programAlgLock(
        AlgLock_AF,
        NV_FALSE,
        NV_FALSE,
        ALG_UNLOCK_TIMEOUT,
        NvMMCameraAlgorithmSubType_None);
    if (e != NvSuccess)
    {
        ALOGE("%s: Autofocus Unlock failed 0x%x", __FUNCTION__, e);
    }

    return e;
}


NvError NvCameraHal::AcquireWakeLock()
{
    int status = 0;

    ALOGVV("%s++", __FUNCTION__);

    if (mWakeLocked)
    {
        ALOGVV("%s-- (already locked)", __FUNCTION__);
        return NvSuccess;
    }

    status = acquire_wake_lock(PARTIAL_WAKE_LOCK, CAMERA_WAKE_LOCK_ID);
    if (status < 0)
    {
        ALOGE("%s-- (error: could not acquire camera wake lock)", __FUNCTION__);
        return NvError_BadParameter;
    }

    mWakeLocked = NV_TRUE;
    ALOGVV("%s--", __FUNCTION__);
    return NvSuccess;
}

NvError NvCameraHal::ReleaseWakeLock()
{
    int status = 0;

    ALOGVV("%s++", __FUNCTION__);

    if (!mWakeLocked)
    {
        ALOGVV("%s-- (already released)", __FUNCTION__);
        return NvSuccess;
    }

    status = release_wake_lock(CAMERA_WAKE_LOCK_ID);
    if (status < 0)
    {
        ALOGE("%s-- (error: could not release camera wake lock)", __FUNCTION__);
        return NvError_BadParameter;
    }

    mWakeLocked = NV_FALSE;
    ALOGVV("%s--", __FUNCTION__);
    return NvSuccess;
}

static NvError
CameraCopySurfaceForUser(
    NvRmSurface *Surfaces,
    void* userBuffer,
    NvU32 *size,
    NVMM_FRAME_FORMAT format)
{
    NvU32 lumaSize, reqSize;
    NvU8 *dataBuffer;

    NvU32 lumaStride = 0;
    NvU32 chromaStride = 0;
    NvU32 chromaSize = 0;
    NvU32 chromaW = 0, chromaH = 0;
    NvBool IsAlreadyAlligned = NV_TRUE;

    if (size == NULL || Surfaces == NULL)
    {
        return NvError_BadParameter;
    }

    if ((format == NVMM_VIDEO_FRAME_FormatNV21) ||
        (format == NVMM_PREVIEW_FRAME_FormatNV21) ||
        (format == NVMM_VIDEO_FRAME_FormatIYUV))
    {
        // For NV21 or IYUV, total memory required is
        // sizeof(Y) * 1.5 = SurfaceWidth * SurfaceHeight * 1.5
        lumaSize = Surfaces[0].Width * Surfaces[0].Height;
        reqSize = lumaSize * 3 / 2;
    }
    else if (format == NVMM_PREVIEW_FRAME_FormatYV12)
    {
        // YV12 (YUV420p) format, strides need to be 16 byte alligned
        lumaStride = (Surfaces[0].Width + 15) & ~15;
        lumaSize = lumaStride * Surfaces[0].Height;
        chromaStride = ((Surfaces[0].Width / 2) + 15) & ~15;

        // If the surface has no need for allignment, we can save time by
        // copying the whole surfaces instead of doing a line by line copy
        if ((lumaStride != Surfaces[0].Width) || (chromaStride != (Surfaces[0].Width / 2)))
        {
            IsAlreadyAlligned = NV_FALSE;
        }
        chromaSize = chromaStride * (Surfaces[0].Height / 2);
        reqSize = lumaSize + chromaSize * 2;
    }
    else
    {
        ALOGE("%s invalid format in CameraCopySurfaceForUser", __FUNCTION__);
        return NvError_BadParameter;
    }

    if (userBuffer == NULL)
    {
        *size = (NvU32)reqSize;
        return NvSuccess;
    }

    if (*size < reqSize)
    {
        // Size of user-supplied buffer is too small for our data
        return NvError_InsufficientMemory;
    }

    dataBuffer = (NvU8 *)userBuffer;

    // Check that input surface follows the conventions of a YUV420 surface
    if (! ((Surfaces[0].Width == Surfaces[1].Width * 2) &&
           (Surfaces[0].Height == Surfaces[1].Height * 2) &&
           (Surfaces[0].Width == Surfaces[2].Width * 2) &&
           (Surfaces[0].Height == Surfaces[2].Height * 2)))
    {
        return NvError_BadParameter;
    }
    if (! (NV_COLOR_GET_BPP(Surfaces[0].ColorFormat) == 8 &&
           NV_COLOR_GET_BPP(Surfaces[1].ColorFormat) == 8 &&
           NV_COLOR_GET_BPP(Surfaces[2].ColorFormat) == 8))
    {
        return NvError_BadParameter;
    }

    // Already check the format before
    if (format == NVMM_VIDEO_FRAME_FormatIYUV)
    {
        // YCbCr format, direct copy
        // no restrictions on stride allignment
        // copy data in
        NvRmSurfaceRead(
            &(Surfaces[0]),
            0,
            0,
            Surfaces[0].Width,
            Surfaces[0].Height,
            dataBuffer);
        NvRmSurfaceRead(
            &(Surfaces[1]),
            0,
            0,
            Surfaces[1].Width,
            Surfaces[1].Height,
            dataBuffer + lumaSize);
        NvRmSurfaceRead(
            &(Surfaces[2]),
            0,
            0,
            Surfaces[2].Width,
            Surfaces[2].Height,
            dataBuffer + lumaSize + lumaSize / 4);
    }
    else if (format == NVMM_PREVIEW_FRAME_FormatYV12 && IsAlreadyAlligned)
    {
        // YCrCb format, if surfaces are already alligned, do a direct
        // surface read instead of a line by line read.
        // copy data in
        NvRmSurfaceRead(
            &(Surfaces[0]),
            0,
            0,
            Surfaces[0].Width,
            Surfaces[0].Height,
            dataBuffer);
        NvRmSurfaceRead(
            &(Surfaces[2]),
            0,
            0,
            Surfaces[2].Width,
            Surfaces[2].Height,
            dataBuffer + lumaSize);
        NvRmSurfaceRead(
            &(Surfaces[1]),
            0,
            0,
            Surfaces[1].Width,
            Surfaces[1].Height,
            dataBuffer + lumaSize + lumaSize / 4);
    }
    else if (format == NVMM_PREVIEW_FRAME_FormatYV12 && !IsAlreadyAlligned)
    {
        // Surface strides are greater than widths,
        // do a line by line copy.
        NvU8 *pDstY = NULL;
        NvU8 *pDstV = NULL;
        NvU8 *pDstU = NULL;
        NvU32 i = 0;

        chromaW = Surfaces[0].Width / 2;
        chromaH = Surfaces[0].Height / 2;

        pDstY = dataBuffer;
        pDstV = pDstY + lumaSize;
        pDstU = pDstV + chromaSize;

        // Copy luma
        for (i = 0; i < Surfaces[0].Height; i++)
        {
            NvRmSurfaceRead(&(Surfaces[0]), 0, i, Surfaces[0].Width, 1, pDstY);
            pDstY += lumaStride;
        }

        // Copy chroma
        for (i = 0; i < chromaH; i++)
        {
            NvRmSurfaceRead(&(Surfaces[1]), 0, i, chromaW, 1, pDstU);
            NvRmSurfaceRead(&(Surfaces[2]), 0, i, chromaW, 1, pDstV);
            pDstU += chromaStride;
            pDstV += chromaStride;
        }
    }
    else if ((format == NVMM_PREVIEW_FRAME_FormatNV21) ||
             (format == NVMM_VIDEO_FRAME_FormatNV21))
    {
        NvU8 *tmpVBuffer = (NvU8 *)NvOsAlloc(lumaSize / 4);
        int chromaSize = lumaSize / 4;
        NvU8 *srcU, *srcV, *dst;

        if (tmpVBuffer == NULL)
        {
            // Couldn't get all necessary working memory
            return NvError_InsufficientMemory;
        }

        // Read Y to Y, read U to the tail-end of the VU plane,
        // read V to a temporary buffer
        dst = dataBuffer + lumaSize;
        srcU = dataBuffer + lumaSize + chromaSize;
        srcV = tmpVBuffer;

        // copy data in
        NvRmSurfaceRead(
            &(Surfaces[0]),
            0,
            0,
            Surfaces[0].Width,
            Surfaces[0].Height,
            dataBuffer);
        NvRmSurfaceRead(
            &(Surfaces[1]),
            0,
            0,
            Surfaces[1].Width,
            Surfaces[1].Height,
            srcU);
        NvRmSurfaceRead(
            &(Surfaces[2]),
            0,
            0,
            Surfaces[2].Width,
            Surfaces[2].Height,
            srcV);

        while (dst < srcU)
        {
            *dst++ = *srcV++;
            *dst++ = *srcU++;
        }

        NvOsFree(tmpVBuffer);
    }

    return NvSuccess;
}

NvError
NvCameraHal::CameraGetUserYUV(
    NvMMBuffer *Buffer,
    camera_memory_t **shmem,
    NVMM_FRAME_FORMAT format)
{
    NvError e = NvSuccess;
    NvU32 DataSize = 0;
    NvMMSurfaceDescriptor *pSurfaceDesc = NULL;
    NvBool IsPreviewCbSizeEnable = NV_FALSE;

    // Get Preview Callback Size settings:
    const NvCombinedCameraSettings &Settings =
            mSettingsParser.getCurrentSettings();

    // Enable frame downscaling/resizing only for preview callback.
    if (Settings.PreviewCbSizeEnable &&
        mPreviewFrameCopy->Enabled() &&
        !mPostviewFrameCopy->Enabled())
            IsPreviewCbSizeEnable = NV_TRUE;

    ALOGVV("%s++", __FUNCTION__);

    if (Buffer == NULL)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    // If application needs, resize/scale preview callback frames.
    if (IsPreviewCbSizeEnable)
    {
        NvU32 RequestedWidth = Settings.PreviewCbSize.width;
        NvU32 RequestedHeight = Settings.PreviewCbSize.height;

        NvMMSurfaceDescriptor *pInput = &Buffer->Payload.Surfaces;
        pSurfaceDesc = &ScaledSurfaces;

        if(!pSurfaceDesc->Surfaces[0].hMem)
            NV_CHECK_ERROR_CLEANUP(
                mScaler.AllocateYuv420NvmmSurface(pSurfaceDesc,
                            RequestedWidth, RequestedHeight, format)
            );

        NV_CHECK_ERROR_CLEANUP(
            mScaler.Scale(pInput, pSurfaceDesc)
        );
    }
    else
        pSurfaceDesc = &Buffer->Payload.Surfaces;

    NV_CHECK_ERROR_CLEANUP(
        CameraCopySurfaceForUser(
                 pSurfaceDesc->Surfaces,
                 NULL,
                 &DataSize,
                 format)
    );

    // We now have required data size in bytes:
    *shmem = mGetMemoryCb(-1, DataSize, 1, mCbCookie);
    if (!(*shmem))
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    // If Preview callback downscaling/resizing is set and format is NV21,
    // then do memcpy directly, else go for surface-read.
    if (IsPreviewCbSizeEnable &&
        format == NVMM_PREVIEW_FRAME_FormatNV21)
    {
        NvU8 *dataBuffer = (NvU8*)(*shmem)->data;
        NvU32 SurfaceSize = 0;
        NvS32 i = 0;

        for (i = 0; i < pSurfaceDesc->SurfaceCount; i++)
        {
            SurfaceSize = pSurfaceDesc->Surfaces[i].Width *
                                pSurfaceDesc->Surfaces[i].Height;
            NvRmMemRead(pSurfaceDesc->Surfaces[i].hMem,
                                0, dataBuffer, SurfaceSize);
            dataBuffer = dataBuffer + SurfaceSize;
        }
    }
    else
    {
        NV_CHECK_ERROR_CLEANUP(
            CameraCopySurfaceForUser(
                     pSurfaceDesc->Surfaces,
                     (*shmem)->data,
                     &DataSize,
                     format)
        );
    }

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvFrameCopyDataCallback::NvFrameCopyDataCallback(NvCameraHal *cam, NvS32 CallbackId)
{
    mEnabled = NV_FALSE;
    mFrameCopyInProgress = NV_FALSE;
    mThreadForFrameCopy = NULL;
    mCamera = cam;
    mCallbackId = CallbackId;
}

NvError NvFrameCopyDataCallback::DoFrameCopyAndCallback(NvMMBuffer *pBuffer, NvBool SendBufferDownstream)
{
    NvMMBuffer *CopyBuffer = NULL;
    // This allocation is required to get the struct safely passed
    // to thread and it will be freed in the thread function
    FrameCopyArgs *pFrameCopyStruct =
        (FrameCopyArgs *) NvOsAlloc(sizeof(FrameCopyArgs));
    if (!pFrameCopyStruct)
    {
        return NvError_InsufficientMemory;
    }

    // This allocation is required
    // it will be freed in the thread function
    CopyBuffer = (NvMMBuffer *) NvOsAlloc(sizeof(NvMMBuffer));
    if (!CopyBuffer)
    {
        ALOGE("%s: failed to allocate preview copy buffer", __FUNCTION__);
        return NvError_InsufficientMemory;
    }
    NvOsMemcpy(CopyBuffer, pBuffer, sizeof(NvMMBuffer));

    pFrameCopyStruct->BufferPtr = CopyBuffer;
    pFrameCopyStruct->This = this;
    // if SendBufferDownstream is true, frame copy thread need to Deliver buffer to ANW
    pFrameCopyStruct->SendBufferDownstream = SendBufferDownstream;

    NV_ASSERT(mThreadForFrameCopy == NULL);
#if ENABLE_NVIDIA_CAMTRACE
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_HAL_FRAMECOPY_ENTRY,
            NvOsGetTimeUS(),
            3);
#endif
    mFrameCopyInProgress = NV_TRUE;
    NvOsThreadCreate(FrameCopyCallbackThread,
                     (void *)(pFrameCopyStruct),
                     &mThreadForFrameCopy);
    return NvSuccess;
}

void NvFrameCopyDataCallback::CheckAndWaitWorkDone()
{
    mCamera->CheckAndWaitForCondition(mFrameCopyInProgress, mFrameCopyDone);
    if (mThreadForFrameCopy)
    {
        NvOsThreadJoin(mThreadForFrameCopy);
        mThreadForFrameCopy = NULL;
    }
}

void NvFrameCopyDataCallback::FrameCopyCallbackThread(void *args)
{
    NvError err = NvSuccess;
    FrameCopyArgs *pFrameCopy = (FrameCopyArgs *)args;
    NvMMBuffer  *Buffer = pFrameCopy->BufferPtr;
    NvFrameCopyDataCallback   *pThis  = pFrameCopy->This;
    NvCameraHal *pHAL   = pThis->mCamera;
    NvBool SendBufferDownstream = pFrameCopy->SendBufferDownstream;

    camera_memory_t *shmem;
    const NvCombinedCameraSettings &Settings = pHAL->mSettingsParser.getCurrentSettings();

    ALOGV("%s++", __FUNCTION__);
    // Free the memory allocated by caller
    NvOsFree(pFrameCopy);
    pFrameCopy = NULL;

    err = pHAL->CameraGetUserYUV(Buffer, &shmem,
              (Settings.PreviewFormat == NvCameraPreviewFormat_Yuv420sp) ?
              NVMM_PREVIEW_FRAME_FormatNV21 : NVMM_PREVIEW_FRAME_FormatYV12);

    if (err != NvSuccess)
    {
        ALOGE("%s failed, cannot get a frame copy", __FUNCTION__);
        // may need to send buffer back below
    }
    else
    {
        pHAL->DataCb(
            (int) pThis->mCallbackId,
            shmem,
            pHAL->mCbCookie);
    }

    if(SendBufferDownstream == NV_TRUE)
    {
        pHAL->ProcessPreviewBufferAfterFrameCopy(
            Buffer);
    }

    pThis->mFrameCopyInProgress = NV_FALSE;
    // Use broadcast here because HandleDisableMsgType and EmptyPreviewBuffer
    // might wait at same time
    pThis->mFrameCopyDone.broadcast();
#if ENABLE_NVIDIA_CAMTRACE
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_HAL_FRAMECOPY_EXIT,
            NvOsGetTimeUS() ,
            3);
#endif

    NvOsFree(Buffer);
    Buffer = NULL;

    ALOGV("%s--", __FUNCTION__);
}

} // namespace android
