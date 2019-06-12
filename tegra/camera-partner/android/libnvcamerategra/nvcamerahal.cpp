/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define LOG_TAG "NvCameraHal"

#define LOG_NDEBUG 0

#include "nvcamerahal.h"

namespace android {

NvCameraHal::NvCameraHal(NvU32 AndroidSensorId,
                         NvU32 AndroidSensorOrientation,
                         camera_device_t *dev)
{
    NvError e = NvSuccess;
    NvU32 i = 0;

    ALOGVV("%s++", __FUNCTION__);

#if ENABLE_NVIDIA_CAMTRACE
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_HAL_LAUNCH_ENTRY,
            NvOsGetTimeUS() ,
            3);
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_HAL_CONSTRUCTOR_ENTRY,
            NvOsGetTimeUS() ,
            3);
#endif

    mNotifyCb = 0;
#if NV_CAMERA_V3
    mSensorWidth = 0;
    mSensorHeight = 0;
#endif

    mAPIOwner = 0;
    mEventOwner = 0;
    mPreviewEnabled = NV_FALSE;
    mPreviewStarted = NV_FALSE;
    mPreviewStreaming = NV_FALSE;
    mPreviewPaused = NV_FALSE;

    mFastburstEnabled = NV_FALSE;
    mFastburstStarted = NV_FALSE;
    mFrameRateChanged = NV_FALSE;

    mPreviewWindow = NULL;
    mVideoEnabled = NV_FALSE;
    mVideoStarted = NV_FALSE;
    mVideoStreaming = NV_FALSE;
    mNumOfVideoFramesDelivered = 0;
    mUsPrevVideoTimeStamp = 0;
    mNsCurVideoTimeStamp = 0;
    mMsgEnabled = 0;
    mImageEncoder = NULL;
    mClientJpegBuffer = NULL;
    mMinUndequeuedPreviewBuffers = 0;
    mRmDevice = NULL;
    m_bSharedMemory = NV_TRUE;
    NvOsMemset(mImageEncoderOutput, 0, sizeof(mImageEncoderOutput));

    // assume android sensor ID maps directly to the NvMM sensor ID
    mSensorId = AndroidSensorId;
    mUsCaptureRequestTimestamp = 0;
    mWakeLocked = NV_FALSE;
    mConstructorError = NV_FALSE;
    mSensorStatus = NvSuccess;
    mIsPowerOnComplete = NV_FALSE;

    mMemProfilePrintEnabled = NV_FALSE;

    mIsSensorPowerOnComplete = NV_FALSE;
    mDelayJpegEncoderOpen = NV_TRUE;
    mDelayJpegStillEncoderSetup = NV_TRUE;
    mDelayJpegThumbnailEncoderSetup = NV_TRUE;
    mDelayedSettingsComplete = NV_FALSE;
    mThreadToDoStuffThatNeedsPreview = NULL;
    mThreadToStartRecordingAfterVideoBufferUpdate = NULL;

    mSensorSetupThreadHandle = NULL;

    mAreAllBuffersBack = NV_FALSE;

    mANWFailureHandlerThread = NULL;
    mPreviewWindowDestroyed = NV_FALSE;


    mSendJpegDataThread = NULL;
    mFilledOutputBufferQueue = NULL;
    mEmptyOutputBufferQueue = NULL;
    mhSemFilledOutputBufAvail = NULL;
    mhSemEmptyOutputBufAvail = NULL;
    mStillCaptureMutex = NULL;

    mhSemPictureRequest = NULL;

    // setting default of:
    // advanced noise reduction
    mIsVideo = NV_FALSE;
    mAnrMode = AnrMode_Off;

    mIsCapturing = NV_FALSE;
    mIsTorchConverged = NV_FALSE;
    mGotAeComplete = NV_FALSE;
    mGotAwbComplete = NV_FALSE;
    mGotAfComplete = NV_FALSE;
    mAeAwbUnlockNeeded = NV_FALSE;
    mCancelFocusPending = NV_FALSE;

    mEventsUpdateCameraBlockThread = NULL;
    mDontCreateEventsUpdateCameraBlockThread = NV_FALSE;
    mEventTypeUpdatingCameraBlock  = 0;

    mPreviewFrameCopy = new NvFrameCopyDataCallback(this, CAMERA_MSG_PREVIEW_FRAME);
    mPostviewFrameCopy = new NvFrameCopyDataCallback(this, CAMERA_MSG_POSTVIEW_FRAME);

    mSensorListener = NULL;
    mOrientation = ORIENTATION_INVALID;
    mSensorOrientation = AndroidSensorOrientation;

    mThreadToHandleFaceDetector = NULL;
    mFaceMetaData = NULL;
    mFdState = FaceDetectionState_Idle;
    mFdReq = FaceDetectionState_Invalid;
    mFdPaused = NV_FALSE;
    mNumFaces = 0;
    mpFaces = NULL;
    mNeedToReset3AWindows = NV_FALSE;

    mWaitForManualSettings = NV_FALSE;
    mMasterWaitFlagForSettings = NV_FALSE;
    mFinalWaitCountInFrames = 0;
    mCurrentPreviewFramesForWait = 0;

    NvOsMemset(&mDefFocusRegions, 0, sizeof(mDefFocusRegions));

    mStillCaptureCount = 0;
    mStillCaptureStop = NV_FALSE;
    mStillCaptureStopMutex = NULL;

    mSetZoomPending = NV_FALSE;
    mSmoothZoomInProgress = NV_FALSE;
    mSmoothZoomStopping = NV_FALSE;
    mSmoothZoomTargetLevel = 0;
    mSmoothZoomCurrentLevel = 0;

    mHalCaptureStartTime = 0;
    mHalShutterCallbackTime = 0;
    mHalShutterLag = 0;
    mHalPostViewCallbackTime = 0;
    mHalPostViewLag = 0;
    mHalStillYUVBufferFromDZTime = 0;
    mHalStillYUVBufferFromDZLag = 0;
    mHalJpegCallbackTime = 0;
    mHalJpegLag = 0;
    mDNGEnabled = NV_FALSE;
    mANRCached = NV_FALSE;
    mWBModeCached = NV_FALSE;
    mIsPassThroughSupported = NV_FALSE;

    NvOsMemset(&Cam, 0, sizeof(NvMMBlockContainer));
    NvOsMemset(&DZ, 0, sizeof(NvMMBlockContainer));

    for (i = 0;
         i < NvCameraMemProfileConfigurator::MAX_NUMBER_OF_BUFFERS
             + NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS;
         i++)
    {
        mPreviewBuffers[i].nvmm = NULL;
        mPreviewBuffers[i].anb = NULL;
        mPreviewBuffers[i].isPinned = NV_FALSE;
    }

    for (i = 0; i < NvCameraMemProfileConfigurator::MAX_NUMBER_OF_BUFFERS; i++)
    {
        mVideoBuffers[i].isDelivered = NV_FALSE;
        mVideoBuffers[i].OmxBufferHdr.pAppPrivate = &mVideoBuffers[i];
        mVideoBuffers[i].OmxBufferHdr.pBuffer =
            (OMX_U8 *)NvOsAlloc(sizeof(NvMMBuffer));
    }

    mAELocked = NV_FALSE;
    mAWBLocked = NV_FALSE;

    if (getCamType(mSensorId) == NVCAMERAHAL_CAMERATYPE_USB)
    {
        // USB camera uses different set of stream indexs
        // because of absence of Input stream.
        mCameraStreamIndex_OutputPreview =
                  (NvMMCameraStreamIndex)NvMMUSBCameraStreamIndex_OutputPreview;
        mCameraStreamIndex_Output =
                  (NvMMCameraStreamIndex)NvMMUSBCameraStreamIndex_Output;
    }
    else
    {
        mCameraStreamIndex_OutputPreview = NvMMCameraStreamIndex_OutputPreview;
        mCameraStreamIndex_Output = NvMMCameraStreamIndex_Output;
    }

    // scope the APILock so we can remove it before calling release in case of failure
    {
        // lock after things are initialized a bit
        APILock lock(this);
        NV_CHECK_ERROR_CLEANUP(
            setupHalInLock()
        );
    }

    /*
     *  To enable/disable hotplug feature of the camera, even before camera
     *  gets started.  When enabled the sensor's id is queried prior to
     *  completion of the HAL constructor.  This causes a slight KPI hit but
     *  enables the constructor to abort if the sensor isn't plugged in.
     *  nvbug: 1188665
     *  Use "adb shell setprop camera.mode.hotplug 1" to enable the feature
     *  and "adb shell setprop camera.mode.hotplug 0" to disable the feature
     *  By default the feature will not be enabled.
    */
    char value[PROPERTY_VALUE_MAX];
    property_get("camera.mode.hotplug", value, 0);
    if (atoi(value) == 1)
    {
        CheckAndWaitForCondition(!mIsSensorPowerOnComplete, mSensorStatusReceived);
        if (mSensorStatus != NvSuccess)
        {
            e = mSensorStatus;
            goto fail;
        }
    }

    /*
     * Fix focus position.
     * Move focus to max or min position.
     * Usage: adb shell setprop camera.debug.fixfocus <mode>
     * <mode> 1: macro, 2: infinite
     */
    NvU32 focus_length;
    property_get("camera.debug.fixfocus", value, "0");
    focus_length = atoi(value);
    if (focus_length != 0)
    {
        NvCameraIspFocusingParameters param;
        APILock lock(this);
        NV_CHECK_ERROR_CLEANUP(
            GetSensorFocuserParam(param)
        );
        // set max to both min and max
        if (atoi(value) == 1)
        {
            param.minPos = param.maxPos;
        }
        // set min to both min and max
        else if (atoi(value) == 2)
        {
            param.maxPos = param.minPos;
        }
        NV_CHECK_ERROR_CLEANUP(
            SetSensorFocuserParam(param)
        );
    }

    /*
     * Fix preview fps.
     * Preview fps ranges from 0 to max supported fps given resolution.
     * Usage: adb shell setprop camera.debug.previewfps <fps>
     */
    NvU32 preview_fps;
    property_get("camera.debug.previewfps", value, "0");
    preview_fps = atoi(value);
    if (preview_fps)
    {
        NvCombinedCameraSettings Settings;
        Settings.videoSpeed = 1.0f;
        Settings.previewFpsRange.min = preview_fps*1000;
        Settings.previewFpsRange.max = preview_fps*1000;
        NV_CHECK_ERROR_CLEANUP(
            SetFrameRateRange(Settings)
        );
    }

    property_get("camera-memory-profile", value, "0");
    mMemProfilePrintEnabled = atoi(value) == 1 ? NV_TRUE : NV_FALSE;

    // Initialize ScaledSurfaces
    NvOsMemset(&ScaledSurfaces, 0, sizeof(ScaledSurfaces));
#if NV_CAMERA_V3
    // create semaphore to signal availability of filled output buffer to HAL v3
    NV_CHECK_ERROR_CLEANUP(
        NvOsSemaphoreCreate(&mCameraBufferReady, 0)
    );
#endif

    ALOGVV("%s--", __FUNCTION__);
    return;

fail:
    ALOGE("%s:%d failed to open camera! Error: 0x%x", __FUNCTION__, __LINE__, e);
    // call release() or something here to undo everything that we started
    release();

    // can't send error to framework, since the notify callback won't be
    // set until after the obj is created.  save it, so that we can send the
    // error as soon as the callback is set.
    mConstructorError = NV_TRUE;
}

NvError NvCameraHal::setupHalInLock()
{
    NvError e = NvSuccess;

    NV_CHECK_ERROR_CLEANUP(
        NvRmOpen(&mRmDevice, 0)
    );

    NV_CHECK_ERROR_CLEANUP(
        NvOsMutexCreate(&mStillCaptureMutex)
    );

    NV_CHECK_ERROR_CLEANUP(
        NvOsMutexCreate(&mStillCaptureStopMutex)
    );

    NV_CHECK_ERROR_CLEANUP(
        NvOsMutexCreate(&mPostProcThreadJoinMutex)
    );

    // initialize sensor listener in a separate thread
    e = NvOsThreadCreate(SetupSensorListenerThread,
                         this,
                         &mSensorSetupThreadHandle);
    if (e != NvSuccess)
    {
        // No need to return error, the orientation
        // listener for face detection won't work, but we shouldn't close
        // camera for that
        ALOGE("%s failed to initialize sensor listener for face detection!", __FUNCTION__);
    }

    // open camera and DZ blocks
    NV_CHECK_ERROR_CLEANUP(
        CreateNvMMCameraBlockContainer()
    );
    NV_CHECK_ERROR_CLEANUP(
        CreateNvMMDZBlockContainer()
    );

    PowerOnCamera();

    // Create the buffer manager
    // Right now this has to happen after the blocks are initialized
    InitBufferManager();
    // Set launch stage
    m_pMemProfileConfigurators[mSensorId]->SetBufferConfigStage(
        NVCAMERA_BUFFERCONFIG_STAGE_LAUNCH);

    m_pMemProfileConfigurator = m_pMemProfileConfigurators[mSensorId];
    m_pBufferManager = m_pBufferManagers[mSensorId];
    m_pBufferStream = m_pBufferStreams[mSensorId];

    // connect them up
    // enabling all pins during init, always-on is best config
    NV_CHECK_ERROR_CLEANUP(
        EnableCameraBlockPins(
            (NvMMCameraPin)(NvMMCameraPin_Preview | NvMMCameraPin_Capture))
    );
    NV_CHECK_ERROR_CLEANUP(
        EnableDZBlockPins(
            (NvMMDigitalZoomPin)(
                NvMMDigitalZoomPin_Preview |
                NvMMDigitalZoomPin_Still   |
                NvMMDigitalZoomPin_Video))
    );

    ConnectCaptureGraph();

    // Do initial allocation of face structures,
    // we can realloc them later if we need more
    mFaceMetaData =
        (camera_frame_metadata_t *)NvOsAlloc(sizeof(camera_frame_metadata_t));
    if (mFaceMetaData == NULL)
    {
        ALOGE("%s: !!Unable to allocate camera metadata buffer", __FUNCTION__);
        goto fail;
    }
    NvOsMemset((NvU8*)mFaceMetaData, 0, sizeof(camera_frame_metadata_t));

    mFaceMetaData->faces =
        (camera_face_t *)NvOsAlloc(sizeof(camera_face_t)*MAX_FACES_DETECTED);
    if (mFaceMetaData->faces == NULL)
    {
        ALOGE("%s: !!Unable to allocate face data buffer", __FUNCTION__);
        goto fail;
    }
    NvOsMemset((NvU8*)mFaceMetaData->faces, 0, sizeof(camera_face_t)*MAX_FACES_DETECTED);

    // make sure power-on is done before we ask the driver about supported
    // settings, since many of them will be inaccurate if queried before
    // power-on is complete
    CheckAndWaitForCondition(!mIsPowerOnComplete, mPowerOnCompleteReceived);

    mDefaultSettings = mSettingsParser.getCurrentSettings();
    // get capabilities from driver, and send them to the parser
    NV_CHECK_ERROR_CLEANUP(
        SendCapabilitiesToParser(mDefaultSettings)
    );

    // get default settings from the driver
    NV_CHECK_ERROR_CLEANUP(
        GetDriverDefaults(mDefaultSettings)
    );

    // Buffer logic
    // trigger buffer negotiation
#if ENABLE_NVIDIA_CAMTRACE
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_HAL_ALLOC_ENTRY,
            NvOsGetTimeUS() ,
            3);
#endif
    NV_CHECK_ERROR_CLEANUP(
        BufferManagerNegotiateAndSupplyBuffers(mDefaultSettings)
    );

    // Post processing
    // Once more than 1 post processing filter is
    // used this needs to be made more dynamic so
    // we can change post processing blocks on the
    // fly
    mPostProcessStill   = new NvCameraHDRStill();
    mPostProcessVideo   = new NvCameraPostProcessVideo();
    mPostProcessPreview = new NvCameraPostProcessPreview();
    mPreviewPostProcThreadHandle = NULL;
    mStillPostProcThreadHandle = NULL;
    mVideoPostProcThreadHandle = NULL;
    mStopPostProcessingPreview = NV_FALSE;
    mStopPostProcessingVideo = NV_FALSE;

    NV_CHECK_ERROR_CLEANUP(
        mPostProcessStill->Initialize(this)
    );

    NV_CHECK_ERROR_CLEANUP(
        mPostProcessVideo->Initialize(this)
    );

    NV_CHECK_ERROR_CLEANUP(
        mPostProcessPreview->Initialize(this)
    );

    // would be nice to tuck this into GetDriverDefaults, but this must be done
    // *after* postproc classes have been created and initialized, and those
    // might depend on having a valid set of driver defaults already populated.
    mDefaultSettings.stillHdrSourceFrameCount = mPostProcessStill->GetAlgorithmInputCount();
    mDefaultSettings.isDirtyForParser.stillHdrSourceFrameCount = NV_TRUE;
    NvCameraHDRStill::GetHDRFrameSequence(mDefaultSettings.stillHdrFrameSequence);
    mDefaultSettings.isDirtyForParser.stillHdrFrameSequence = NV_TRUE;

    // wait for preview config/negotiations to finish
    CheckAndWaitForCondition(
        !DZ.Ports[NvMMDigitalZoomStreamIndex_OutputPreview].BufferConfigDone,
        DZ.Ports[NvMMDigitalZoomStreamIndex_OutputPreview].BufferConfigDoneCond);
    CheckAndWaitForCondition(
        !Cam.Ports[mCameraStreamIndex_OutputPreview].BufferNegotiationDone,
        Cam.Ports[mCameraStreamIndex_OutputPreview].BufferNegotiationDoneCond);
    CheckAndWaitForCondition(
        !DZ.Ports[NvMMDigitalZoomStreamIndex_InputPreview].BufferNegotiationDone,
        DZ.Ports[NvMMDigitalZoomStreamIndex_InputPreview].BufferNegotiationDoneCond);
    // wait for still/video negotiations to finish
    CheckAndWaitForCondition(
        !Cam.Ports[mCameraStreamIndex_Output].BufferNegotiationDone,
        Cam.Ports[mCameraStreamIndex_Output].BufferNegotiationDoneCond);
    CheckAndWaitForCondition(
        !DZ.Ports[NvMMDigitalZoomStreamIndex_Input].BufferNegotiationDone,
        DZ.Ports[NvMMDigitalZoomStreamIndex_Input].BufferNegotiationDoneCond);

    SetPreviewPauseAfterStillCapture(mDefaultSettings, NV_TRUE);

#if ENABLE_NVIDIA_CAMTRACE
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_HAL_ALLOC_EXIT,
            NvOsGetTimeUS() ,
            3);
#endif

    NV_CHECK_ERROR_CLEANUP(
        mCbQueue.init()
    );

    // now that we're done modifying the defaultSettings, let the parser know
    // what we've changed
    mSettingsParser.updateSettings(mDefaultSettings);

    mSettingsCache = mDefaultSettings;
    // apply default settings, this needs to happen after the buffer manager
    // is initialized and set up
    NV_CHECK_ERROR_CLEANUP(
        ApplyChangedSettings()
    );

    NV_CHECK_ERROR_CLEANUP(
        TransitionBlockToState(Cam, NvMMState_Running)
    );
    NV_CHECK_ERROR_CLEANUP(
        TransitionBlockToState(DZ, NvMMState_Running)
    );

    if (mSensorSetupThreadHandle)
    {
        NvOsThreadJoin(mSensorSetupThreadHandle);
        mSensorSetupThreadHandle = NULL;
    }

    // store the DZ still output format
    NV_CHECK_ERROR_CLEANUP(GetDzStillOutputSurfaceType());


#if ENABLE_NVIDIA_CAMTRACE
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_HAL_CONSTRUCTOR_EXIT,
            NvOsGetTimeUS() ,
            3);
#endif

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s:%d failed to open camera! Error: 0x%x", __FUNCTION__, __LINE__, e);
    return e;
}

NvCameraHal::~NvCameraHal()
{
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    APILock lock(this);

#if NV_CAMERA_V3
    if (mCameraBufferReady)
    {
        NvOsSemaphoreDestroy(mCameraBufferReady);
        mCameraBufferReady = NULL;
    }
#endif

    // Release buffer management stuff
    m_pMemProfileConfigurator->SetBufferConfigStage(
        NVCAMERA_BUFFERCONFIG_STAGE_EXIT);
    BufferManagerUpdateNumberOfBuffers();

    // TODO should this be done here?  can it be done in release() instead?
    // teardown logic should be in release.
    m_pBufferManager->InvalidateDriverInfo(m_pBufferStream);
    // STUB...should we do anything in the destructor?  maybe call release()
    // as a precaution?
    ALOGVV("%s--", __FUNCTION__);
    return;
}

NvBool NvCameraHal::GetConstructorError()
{
    return mConstructorError;
}

// setPreviewWindow can be called in these cases:
// 1) preview is stopped
//    - if window NULL, do nothing
//    - else if window !NULL, save it for when preview is started
// 2) startPreview was called, but no valid window was set before
//    (in other words, preview "enabled" but not "started")
//    - if window still NULL, do nothing
//    - else if window !NULL, start preview now that we finally can
// 3) startPreview was called, and a valid window had been set before
//    (in other words, preview "enabled" and "started)
//    - stop preview
//    - if window NULL, leave it stopped
//    - else if window !NULL, switch the window and restart preview
// NOTE: cases 1 and 2 are legal according to the android API, case 3
//       is illegal but should still be handled elegantly until CTS forces us
//       to throw an error
android::status_t NvCameraHal::setPreviewWindow(struct preview_stream_ops *window)
{
    NvBool restorePreview = NV_FALSE;
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    // If the mutex has been destroyed we can't join the postprocessing thread
    // so we'd better make sure someone has prevented more postprocessing threads
    // from spawning
    NV_ASSERT(mPostProcThreadJoinMutex || mStopPostProcessingPreview);

    if (mRmDevice == NULL)
    {
        ALOGE("%s--: NvCameraHal Not initialized. Returning error", __FUNCTION__);
        return ERROR_NOT_READY;
    }

    // since this func stops preview, we need to make sure postproc threads
    // have flushed and aren't hanging on to any buffers while we stop/restart
    {
        AutoMutex lock(mLock);
        // make sure no new preview postproc thread gets kicked off after the
        // current thread is joined
        mStopPostProcessingPreview = NV_TRUE;
    }
    // join outside the lock to avoid deadlock
    // don't join if release() has already been called and the mutex has been destroyed
    if (mPostProcThreadJoinMutex)
    {
        JoinPostProcessingThreads(
            NvMMBufferType_Payload,
            NvMMDigitalZoomStreamIndex_OutputPreview);
    }

    APILock lock(this);
    mPreviewWindowDestroyed = NV_FALSE;
    // if startPreview had been called by the app,
    // regardless of whether it started successfully,
    // this flag will be true.
    // remember its value so that we can restore it later
    restorePreview = mPreviewEnabled;

    // make sure it's stopped before we switch the window
    NV_CHECK_ERROR_CLEANUP(
        StopPreviewInternal()
    );

    // switch the window
    mPreviewWindow = window;

    // start it back up if we need to
    if (mPreviewWindow && restorePreview)
    {
        NV_CHECK_ERROR_CLEANUP(
            StartPreviewInternal()
        );
        mStopPostProcessingPreview = NV_FALSE;
    }

    ALOGVV("%s--", __FUNCTION__);
    return 0;

fail:
    ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
    NOTIFY_ERROR();
    return ERROR_NOT_READY;
}

status_t NvCameraHal::startPreview()
{
    NvError e = NvSuccess;
    ALOGVV("%s++", __FUNCTION__);

    APILock lock(this);

    // ignore redundant app calls
    if (mPreviewEnabled)
    {
        ALOGVV("%s-- (%s already called)", __FUNCTION__, __FUNCTION__);
        return 0;
    }

    // Should wait for previous capture to be completed
    if (mIsCapturing)
        WaitForCondition(mCaptureDone);

    // android API sped says this is valid. we should remember the call so
    // preview can start once a valid preview window is supplied,
    // but avoid doing anything else
    if (!mPreviewWindow)
    {
        mPreviewEnabled = NV_TRUE;
        ALOGVV(
            "%s-- (no preview window set, preview not started yet)",
            __FUNCTION__);
        return 0;
    }

    NV_CHECK_ERROR_CLEANUP(
        // this may not actually start preview, if a valid preview
        // window hasn't been set yet
        StartPreviewInternal()
    );

    // record that this function was called, in case conditions aren't
    // met to actually start preview
    mPreviewEnabled = NV_TRUE;

    // let postprocessing happen now that preview is started
    mStopPostProcessingPreview = NV_FALSE;

    ALOGVV("%s--", __FUNCTION__);
    return 0;

fail:
    ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
    NOTIFY_ERROR();
    return ERROR_NOT_READY;
}

void NvCameraHal::stopPreview()
{
    ALOGI("%s++", __FUNCTION__);
    NvError e = NvSuccess;

    {
        // grab the mutex, but not APILock while we wait, so that the other
        // thread can get the APILock
        AutoMutex lock(mLock);

        // can't use NV_CHECK_ERROR_CLEANUP() and goto fail tag here
        // because of variable scoping on APILock
        e = WaitForStuffThatNeedsPreview();
        if (e != NvSuccess)
        {
            ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
            NOTIFY_ERROR();
            return;
        }
        e = CheckAndWaitForCondition(mSmoothZoomStopping, mSmoothZoomDoneStopping);
        if (e != NvSuccess)
        {
            ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
            NOTIFY_ERROR();
            return;
        }

        // make sure no new preview postproc thread gets kicked off after the
        // current thread is joined
        mStopPostProcessingPreview = NV_TRUE;
    }
    // join outside the lock to avoid deadlock
    JoinPostProcessingThreads(
        NvMMBufferType_Payload,
        NvMMDigitalZoomStreamIndex_OutputPreview);

    SetFdState(FaceDetectionState_Stop, NV_TRUE);

    // stop the services before stopping preview.
    StopEventsUpdateCameraBlockThread();
    mLockEventsUpdateCameraState.lock();
    mDontCreateEventsUpdateCameraBlockThread = NV_TRUE;
    mLockEventsUpdateCameraState.unlock();

    APILock lock(this);

    // ignore redundant app calls
    if (!mPreviewEnabled && !mPreviewPaused)
    {
        ALOGVV("%s-- (preview wasn't started)", __FUNCTION__);
        return;
    }
    NV_CHECK_ERROR_CLEANUP(
        StopPreviewInternal()
    );

    mPreviewEnabled = NV_FALSE;

    ALOGI("%s--", __FUNCTION__);

    mLockEventsUpdateCameraState.lock();
    mDontCreateEventsUpdateCameraBlockThread = NV_FALSE;
    mLockEventsUpdateCameraState.unlock();

    return;

fail:
    ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
    NOTIFY_ERROR();
}

status_t NvCameraHal::setParameters(
    const CameraParameters& params,
    NvBool allowNonStandard)
{
    ALOGVV("%s++", __FUNCTION__);
    NvError e = NvSuccess;

    {
        // grab the mutex, but not APILock while we wait, so that the other
        // thread can get the APILock
        AutoMutex lock(mLock);

        // can't use NV_CHECK_ERROR_CLEANUP() and goto fail tag here
        // because of variable scoping on APILock
        e = WaitForStuffThatNeedsPreview();
        if (e != NvSuccess)
        {
            ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
            NOTIFY_ERROR();
            return ERROR_NOT_READY;
        }
        e = CheckAndWaitForCondition(mSmoothZoomStopping, mSmoothZoomDoneStopping);
        if (e != NvSuccess)
        {
            ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
            NOTIFY_ERROR();
            return ERROR_NOT_READY;
        }

        // since this func may stop preview, we need to make sure postproc threads
        // have flushed and aren't hanging on to any buffers while we process
        // new settings.
        // also let video flush, since rotation might change during video record
        // make sure no new preview postproc thread gets kicked off after the
        // current thread is joined
        mStopPostProcessingPreview = NV_TRUE;
        mStopPostProcessingVideo = NV_TRUE;
    }

    {
        APILock lock(this);
        e = mSettingsParser.parseParameters(params, allowNonStandard);
        if (e != NvSuccess)
        {
            ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
            return ERROR_NOT_READY;
        }

        e = SetFdState(FaceDetectionState_Pause, NV_TRUE);
        if (e != NvSuccess)
        {
            ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
            return ERROR_NOT_READY;
        }
    }

    // stop the services before applying new settings.
    StopEventsUpdateCameraBlockThread();
    mLockEventsUpdateCameraState.lock();
    mDontCreateEventsUpdateCameraBlockThread = NV_TRUE;
    mLockEventsUpdateCameraState.unlock();

    // join outside the lock to avoid deadlock
    JoinPostProcessingThreads(
        NvMMBufferType_Payload,
        NvMMDigitalZoomStreamIndex_OutputPreview);
    JoinPostProcessingThreads(
        NvMMBufferType_Payload,
        NvMMDigitalZoomStreamIndex_OutputVideo);

    APILock lock(this);

    NV_CHECK_ERROR_CLEANUP(
        ApplyChangedSettings()
    );

    mLockEventsUpdateCameraState.lock();
    mDontCreateEventsUpdateCameraBlockThread = NV_FALSE;
    mLockEventsUpdateCameraState.unlock();

    NV_CHECK_ERROR_CLEANUP(
        SetFdState(FaceDetectionState_Resume, NV_TRUE)
    );

    mStopPostProcessingPreview = NV_FALSE;
    mStopPostProcessingVideo = NV_FALSE;

    ALOGVV("%s--", __FUNCTION__);
    return 0;

fail:
    ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
    return ERROR_NOT_READY;
}

CameraParameters NvCameraHal::getParameters(NvBool allowNonStandard) const
{
    ALOGVV("%s++", __FUNCTION__);
    // this cast bypasses const-correctness, just to get the lock, which we
    // think is more or less alright
    APILock lock((NvCameraHal *)this);

    CheckAndWaitForCondition(mSetZoomPending, mSetZoomDoneCond);

    CameraParameters params = mSettingsParser.getParameters(allowNonStandard);
    ALOGVV("%s--", __FUNCTION__);
    return params;
}

void
NvCameraHal::setCallbacks(
    camera_notify_callback notify_cb,
    camera_data_callback data_cb,
    camera_data_timestamp_callback data_cb_timestamp,
    camera_request_memory get_memory,
    void* user)
{
    ALOGVV("%s++", __FUNCTION__);
    APILock lock(this);

    mNotifyCb = notify_cb;
    mDataCb = data_cb;
    mDataCbTimestamp = data_cb_timestamp;
    mGetMemoryCb = get_memory;
    mCbCookie = user;

    // constructor hit an error somewhere, let the app know something bad
    // happened now that the callbacks are set
    if (mConstructorError)
    {
        ALOGE("Constructor failed, let app know now that callbacks are set");
        NOTIFY_ERROR();
    }

    ALOGVV("%s--", __FUNCTION__);
}

bool NvCameraHal::previewEnabled()
{
    ALOGVV("%s++", __FUNCTION__);
    APILock lock(this);
    ALOGVV("%s-- (%d)", __FUNCTION__, mPreviewEnabled);
    return mPreviewEnabled ? true : false;
}

void NvCameraHal::enableMsgType(int32_t msgType)
{
    NvError e = NvSuccess;
    ALOGVV("%s++", __FUNCTION__);
    APILock lock(this);

    NV_CHECK_ERROR_CLEANUP(
        HandleEnableMsgType(msgType)
    );
    ALOGVV("%s--", __FUNCTION__);
    return;

fail:
    ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
    NOTIFY_ERROR();
    return;
}

void NvCameraHal::disableMsgType(int32_t msgType)
{
    NvError e = NvSuccess;
    ALOGVV("%s++", __FUNCTION__);
    APILock lock(this);

    NV_CHECK_ERROR_CLEANUP(
        HandleDisableMsgType(msgType)
    );
    ALOGVV("%s--", __FUNCTION__);
    return;

fail:
    ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
    NOTIFY_ERROR();
    return;
}

bool NvCameraHal::msgTypeEnabled(int32_t msgType)
{
    ALOGVV("%s++", __FUNCTION__);
    APILock lock(this);
    ALOGVV("%s--", __FUNCTION__);
    return (mMsgEnabled & msgType) ? true : false;
}

bool NvCameraHal::isMetaDataStoredInVideoBuffers()
{
    ALOGVV("%s++", __FUNCTION__);
    APILock lock(this);
    ALOGVV("%s--", __FUNCTION__);
    return (mUsingEmbeddedBuffers ? true : false);
}

status_t NvCameraHal::storeMetaDataInBuffers(bool enable)
{
    ALOGVV("%s++", __FUNCTION__);
    APILock lock(this);
    mUsingEmbeddedBuffers = enable ? NV_TRUE : NV_FALSE;
    ALOGVV("%s--", __FUNCTION__);
    return 0;
}

status_t NvCameraHal::startRecording()
{
    ALOGVV("%s++", __FUNCTION__);
    NvError e = NvSuccess;

    {
        APILock lock(this);
        // ignore redundant app calls
        if (mVideoEnabled)
        {
            ALOGVV("%s-- (%s already called)", __FUNCTION__, __FUNCTION__);
            return 0;
        }

        InformVideoMode(true);

        NV_CHECK_ERROR_CLEANUP(
            StartRecordingInternal()
        );

        mVideoEnabled = NV_TRUE;

        // let postprocessing happen now that video is started
        mStopPostProcessingVideo = NV_FALSE;
    }

    // Wait for thread, if start recording is handled by thread.
    if (mThreadToStartRecordingAfterVideoBufferUpdate)
    {
        NvOsThreadJoin(mThreadToStartRecordingAfterVideoBufferUpdate);
        mThreadToStartRecordingAfterVideoBufferUpdate = NULL;
        if (mVideoStarted != NV_TRUE)
        {
            e = NvError_Timeout;
            goto fail;
        }
    }

    ALOGVV("%s--", __FUNCTION__);
    return 0;

 fail:
    ALOGVV("%s-- (error 0x%x)", __FUNCTION__, e);
    NOTIFY_ERROR();
    return ERROR_NOT_READY;
}

void NvCameraHal::stopRecording()
{
    ALOGVV("%s++", __FUNCTION__);

    NvError e = NvSuccess;

    {
        AutoMutex lock(mLock);
        // make sure no new video postproc thread gets kicked off after the
        // current thread is joined
        mStopPostProcessingVideo = NV_TRUE;
    }

    // join outside the lock to avoid deadlock
    JoinPostProcessingThreads(
        NvMMBufferType_Payload,
        NvMMDigitalZoomStreamIndex_OutputVideo);

    APILock lock(this);
    // ignore redundant app calls
    if (!mVideoEnabled)
    {
        ALOGVV("%s-- (recording wasn't started)", __FUNCTION__);
        return;
    }

    InformVideoMode(false);

    NV_CHECK_ERROR_CLEANUP(
        StopRecordingInternal()
    );

    mVideoEnabled = NV_FALSE;
    ALOGVV("%s--", __FUNCTION__);
    return;

fail:
    ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
    NOTIFY_ERROR();
}

bool NvCameraHal::recordingEnabled()
{
    ALOGVV("%s++", __FUNCTION__);
    APILock lock(this);

    bool ret = (mVideoEnabled == NV_TRUE);

    ALOGVV("%s--", __FUNCTION__);
    return ret;
}

void NvCameraHal::releaseRecordingFrame(const void *opaque)
{
    OMX_BUFFERHEADERTYPE OmxBufferHdr;
    VideoBuffer *videoBuffer;
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    // old HAL used EventLock here, but that was probably because of OMX
    // locking badness. shouldn't cause a problem with NvMM locking, will it?
    APILock lock(this);

    // nothing to do here if we're not using embedded buffers
    if (!mUsingEmbeddedBuffers)
    {
        ALOGVV("%s-- (not using embedded buffers)", __FUNCTION__);
        return;
    }

    // need the +4 offset because the buffer we gave to the app had a type field
    NvOsMemcpy(&OmxBufferHdr, (NvU8 *)opaque + 4, sizeof(OMX_BUFFERHEADERTYPE));
    // unpack our video buffer ptr from the OMX package
    videoBuffer = (VideoBuffer *)OmxBufferHdr.pAppPrivate;

    // do some sanity checking
    if (!videoBuffer->isDelivered)
    {
        ALOGE(
            "%s: received a buffer that we never delivered to the app!",
            __FUNCTION__);
        goto fail;
    }
    if (!(videoBuffer->OmxBufferHdr.nFlags & OMX_BUFFERFLAG_NV_BUFFER))
    {
        ALOGE(
            "%s: buffer header doesn't have NV buffer flag set!",
            __FUNCTION__);
        goto fail;
    }
    if (!videoBuffer->OmxBufferHdr.pBuffer)
    {
        ALOGE(
            "%s: NvMM buffer ptr is NULL, this shouldn't happen!",
            __FUNCTION__);
        goto fail;
    }

    videoBuffer->isDelivered = NV_FALSE;
    SendEmptyVideoBufferToDZ((NvMMBuffer *)videoBuffer->OmxBufferHdr.pBuffer);

    ALOGVV("%s--", __FUNCTION__);
    return;

fail:
    ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
    NOTIFY_ERROR();
}

status_t NvCameraHal::autoFocus()
{
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);
    APILock lock(this);

    NV_CHECK_ERROR_CLEANUP(
        AutoFocusInternal()
    );

    ALOGVV("%s--", __FUNCTION__);
    return 0;

fail:
    ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
    NOTIFY_ERROR();
    return ERROR_NOT_READY;
}

status_t NvCameraHal::cancelAutoFocus()
{
    NvError e = NvSuccess;
    ALOGVV("%s++", __FUNCTION__);

    {
        // grab the mutex, but not APILock while we wait, so that the other
        // thread can get the APILock
        AutoMutex lock(mLock);

        // There is already another cancelAutoFocus() call pending
        if(mCancelFocusPending)
        {
            ALOGE("%s-- (There was another cancelAutoFocus() call already pending\n", __FUNCTION__);
            return 0;
        }

        if (mIsCapturing)
        {
            ALOGVV("Pending CancelAutoFocusInternal() because still capture is in progress\n");
            mCancelFocusPending = NV_TRUE;
            WaitForCondition(mCaptureDone);
        }
    }

    APILock lock(this);
    // need to do this after getting the APILock to avoid a race condition
    mCancelFocusPending = NV_FALSE;

    NV_CHECK_ERROR_CLEANUP(
        CancelAutoFocusInternal()
    );

    ALOGVV("%s--", __FUNCTION__);
    return 0;

fail:
    ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
    NOTIFY_ERROR();
    return ERROR_NOT_READY;
}

status_t NvCameraHal::takePicture()
{
    NvError e = NvSuccess;
    ALOGI("%s++", __FUNCTION__);

    mHalCaptureStartTime = NvOsGetTimeMS();

#if ENABLE_NVIDIA_CAMTRACE
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_HAL_TAKE_PICTURE_ENTRY,
            NvOsGetTimeUS() ,
            3);
#endif
    {
        // grab the mutex, but not APILock while we wait, so that the other
        // thread can get the APILock
        AutoMutex lock(mLock);

        // can't use NV_CHECK_ERROR_CLEANUP() and goto fail tag here
        // because of variable scoping on APILock
        e = WaitForStuffThatNeedsPreview();
        if (e != NvSuccess)
        {
            ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
            NOTIFY_ERROR();
            return ERROR_NOT_READY;
        }
        e = CheckAndWaitForCondition(mSmoothZoomStopping, mSmoothZoomDoneStopping);
        if (e != NvSuccess)
        {
            ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
            NOTIFY_ERROR();
            return ERROR_NOT_READY;
        }
        // This is where we actually wait for the settings to take effect.
        // For most of the apps, this wait will not be executed as the settings
        // are applied immediately and takePicture comes much later. But some apps
        // may want to apply settings and immediately issue a takePicture.
        // For this case, this wait will provide us enough time to program those
        // settings to the sensor.
        e = CheckAndWaitForCondition(mMasterWaitFlagForSettings, mMasterWaitForSettingsCond);
        if (e != NvSuccess)
        {
            ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
            NOTIFY_ERROR();
            return ERROR_NOT_READY;
        }
    }

    mLockEventsUpdateCameraState.lock();
    mDontCreateEventsUpdateCameraBlockThread = NV_TRUE;
    if (mEventsUpdateCameraBlockThread)
    {
        NvOsThreadJoin(mEventsUpdateCameraBlockThread);
        mEventsUpdateCameraBlockThread = NULL;
    }
    mLockEventsUpdateCameraState.unlock();

    APILock lock(this);

    if (mPostProcessStill->Enabled())
    {
        mPostProcessStill->DoPreCaptureOperations();
    }

    mUsCaptureRequestTimestamp = NvOsGetTimeUS();

    NV_CHECK_ERROR_CLEANUP(
        takePictureInternal()
    );

    mLockEventsUpdateCameraState.lock();
    mDontCreateEventsUpdateCameraBlockThread = NV_FALSE;
    mLockEventsUpdateCameraState.unlock();

    ALOGI("%s--", __FUNCTION__);
    return 0;

fail:
    ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
    NOTIFY_ERROR();
    return ERROR_NOT_READY;
}

status_t NvCameraHal::cancelPicture()
{
    NvError e = NvSuccess;
    ALOGVV("%s++", __FUNCTION__);
    APILock lock(this);
    NV_CHECK_ERROR_CLEANUP(
        StopStillCapture()
    );
    ALOGVV("%s--", __FUNCTION__);
    return 0;

fail:
    ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
    NOTIFY_ERROR();
    return ERROR_NOT_READY;
}

status_t NvCameraHal::sendCommand(int32_t command, int32_t arg1, int32_t arg2)
{
    NvError e = NvSuccess;
    status_t retcode = NO_ERROR;
    ALOGVV("%s++", __FUNCTION__);
    APILock lock(this);

    switch(command)
    {
        case CAMERA_CMD_START_SMOOTH_ZOOM:
            // arg1 should be target zoom value, arg2 is unused
            ALOGVV("%s: Got START_SMOOTH_ZOOM(%d, %d)", __FUNCTION__, arg1, arg2);
            if (!mSettingsParser.checkZoomValue(arg1))
            {
                retcode = BAD_VALUE;
            }
            else
            {
                NV_CHECK_ERROR_CLEANUP(
                    SetZoom(arg1, NV_TRUE)
                );
                mSmoothZoomInProgress = NV_TRUE;
                mSmoothZoomStopping = NV_FALSE;
                mSmoothZoomTargetLevel = arg1;
                ALOGVV("%s: smoothZoom zooming %d -> %d",
                    __FUNCTION__, mSmoothZoomCurrentLevel, arg1);
            }
            break;
        case CAMERA_CMD_STOP_SMOOTH_ZOOM:
            // arg1 and arg2 are both unused
            ALOGVV("%s: Got STOP_SMOOTH_ZOOM(%d, %d)", __FUNCTION__, arg1, arg2);
            NV_CHECK_ERROR_CLEANUP(
                StopSmoothZoomInternal(NV_FALSE)
            );
            break;
        case CAMERA_CMD_ENABLE_FOCUS_MOVE_MSG:
            ALOGVV("Got CAMERA_CMD_ENABLE_FOCUS_MOVE_MSG(%d, %d)", arg1, arg2);
            SetFocusMoveMsg(arg1 ? NV_TRUE : NV_FALSE);
            break;
#if USE_NV_FD
        case CAMERA_CMD_START_FACE_DETECTION:
            ALOGVV("Got CAMERA_CMD_START_FACE_DETECTION(%d, %d)\n", arg1, arg2);
            NV_CHECK_ERROR_CLEANUP(
                SetFdState(FaceDetectionState_Start)
            );
            break;
        case CAMERA_CMD_STOP_FACE_DETECTION:
            // arg1 and arg2 are both unused
            ALOGVV("Got CAMERA_CMD_STOP_FACE_DETECTION(%d, %d)\n", arg1, arg2);
            NV_CHECK_ERROR_CLEANUP(
                SetFdState(FaceDetectionState_Stop)
            );
            break;
#endif
        default:
            retcode = BAD_VALUE;
            break;
    }

    ALOGVV("%s--", __FUNCTION__);
    return retcode;

fail:
    ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
    NOTIFY_ERROR();
    return ERROR_NOT_READY;
}

void NvCameraHal::release()
{
    NvError e = NvSuccess;
    NvU32 i;
    ALOGVV("%s++", __FUNCTION__);

#if ENABLE_NVIDIA_CAMTRACE
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_HAL_RELEASE_ENTRY,
            NvOsGetTimeUS() ,
            3);
#endif

    {
        // grab the mutex, but not APILock while we wait, so that the other
        // thread can get the APILock
        AutoMutex lock(mLock);

        // Stuff that needs preview thread might acquire API lock so it should
        // finish before we acquire API lock here
        e = WaitForStuffThatNeedsPreview();
        if (e != NvSuccess)
        {
            ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
            NOTIFY_ERROR();
            return;
        }

        // FrameCopy may need API Lock
        if (mPostviewFrameCopy)
        {
            mPostviewFrameCopy->CheckAndWaitWorkDone();
        }
        if (mPreviewFrameCopy)
        {
            mPreviewFrameCopy->CheckAndWaitWorkDone();
        }

    }
    // Update Camera Block thread is using API lock, so it should finish
    // before we acquire APILock
    StopEventsUpdateCameraBlockThread();
    e = SetFdState(FaceDetectionState_Stop, NV_TRUE);
    if (e != NvSuccess)
    {
        ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
        NOTIFY_ERROR();
        return;
    }

    // join any remaining postprocessing threads to prevent leaks,
    // also set flags to make sure no remaining spurious callbacks
    // will try to process frames at the same time we're joining the
    // threads.
    {
        AutoMutex lock(mLock);
        mStopPostProcessingPreview = NV_TRUE;
        mStopPostProcessingVideo = NV_TRUE;
    }
    JoinPostProcessingThreads(
         NvMMBufferType_Payload,
         NvMMDigitalZoomStreamIndex_OutputPreview);
    JoinPostProcessingThreads(
         NvMMBufferType_Payload,
         NvMMDigitalZoomStreamIndex_OutputStill);
    JoinPostProcessingThreads(
         NvMMBufferType_Payload,
         NvMMDigitalZoomStreamIndex_OutputVideo);

    APILock lock(this);

    mLockEventsUpdateCameraState.lock();
    mDontCreateEventsUpdateCameraBlockThread = NV_TRUE;
    mLockEventsUpdateCameraState.unlock();

    // old HAL took a wake lock while it was releasing resources.  we didn't
    // have any history of the specific bug that it was placed there for, but
    // it may have taken hours of debugging to find that, so we carried it over
    // into the new HAL
    NV_CHECK_ERROR_CLEANUP(
        AcquireWakeLock()
    );

    if (mSensorSetupThreadHandle)
    {
        NvOsThreadJoin(mSensorSetupThreadHandle);
        mSensorSetupThreadHandle = NULL;
    }

    CleanupSensorListener();

    // free up face meta data
    if (mFaceMetaData)
    {
        if (mFaceMetaData->faces)
        {
            NvOsFree(mFaceMetaData->faces);
            mFaceMetaData->faces = NULL;
        }

        NvOsFree(mFaceMetaData);
        mFaceMetaData = NULL;
    }

    mCbQueue.release();
    NV_CHECK_ERROR_CLEANUP(
        CleanupNvMM()
    );

    for (i = 0; i < NvCameraMemProfileConfigurator::MAX_NUMBER_OF_BUFFERS; i++)
    {
        NvOsFree(mVideoBuffers[i].OmxBufferHdr.pBuffer);
        mVideoBuffers[i].OmxBufferHdr.pBuffer = NULL;
    }

    NV_CHECK_ERROR_CLEANUP(
        CloseJpegEncoder()
    );


    if (mStillCaptureMutex)
    {
        NvOsMutexDestroy(mStillCaptureMutex);
        mStillCaptureMutex = NULL;
    }

    if (mStillCaptureStopMutex)
    {
        NvOsMutexDestroy(mStillCaptureStopMutex);
        mStillCaptureStopMutex = NULL;
    }

    if (mPostProcThreadJoinMutex)
    {
        NvOsMutexDestroy(mPostProcThreadJoinMutex);
        mPostProcThreadJoinMutex = NULL;
    }

    if (mPreviewFrameCopy)
    {
        delete mPreviewFrameCopy;
        mPreviewFrameCopy = NULL;
    }

    if (mPostviewFrameCopy)
    {
        delete mPostviewFrameCopy;
        mPostviewFrameCopy = NULL;
    }

    if (mPostProcessStill)
    {
        delete mPostProcessStill;
        mPostProcessStill = NULL;
    }

    if (mPostProcessVideo)
    {
        delete mPostProcessVideo;
        mPostProcessVideo = NULL;
    }

    if (mPostProcessPreview)
    {
        delete mPostProcessPreview;
        mPostProcessPreview = NULL;
    }

    // Free ScaledSurfaces
    if(ScaledSurfaces.Surfaces[0].hMem)
        mScaler.FreeSurface(&ScaledSurfaces);

    NvRmClose(mRmDevice);
    mRmDevice = NULL;

    NV_CHECK_ERROR_CLEANUP(
        ReleaseWakeLock()
    );

#if ENABLE_NVIDIA_CAMTRACE
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_HAL_RELEASE_EXIT,
            NvOsGetTimeUS() ,
            3);
#endif

    ALOGVV("%s--", __FUNCTION__);
    return;

fail:
    ALOGE("%s-- ERROR [0x%x]", __FUNCTION__, e);
    NOTIFY_ERROR();
    return;
}

status_t NvCameraHal::dump(int fd)
{
    NvError e = NvSuccess;
    ALOGVV("%s++", __FUNCTION__);
    APILock lock(this);
    // STUB TODO
    ALOGVV("%s--", __FUNCTION__);
    return 0;
}

} // namespace android
