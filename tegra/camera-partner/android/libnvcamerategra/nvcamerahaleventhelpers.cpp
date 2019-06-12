/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define LOG_TAG "NvCameraHalEventHelpers"

#define LOG_NDEBUG 0

#include "nvcamerahal.h"
#include <nvgr.h>
#include <fcamdng.h>
#include "nvrawfile.h"

namespace android {

static void NvDumpSurface(
    NvRmSurface **ppSurface,
    NvU32 NumSurface,
    NvU8 *pFileName);

static NvBool isEventUpdateCameraBlock(NvU32 EventType);

// debug code to dump a surface, can extract from NvMM buffer or ANB like this:
/*
    // nvmm
    NvRmSurface *pSurface[3] = {NULL, NULL, NULL};
    NVMMDIGITALZOOM_GET_SURFACE_PTR(pSurface, pNvMMBuffer->Payload.Surfaces.Surfaces);
    NvDumpSurface(pSurface, 3, NULL);

    // anb
    buffer_handle_t handle;
    handle = (buffer_handle_t)(*pANB);
    const NvRmSurface *Surf;
    size_t SurfCount;
    nvgr_get_surfaces(handle, &Surf, &SurfCount);
    NVMMDIGITALZOOM_GET_SURFACE_PTR(pSurface, Surf);
    NvDumpSurface(pSurface, SurfCount, NULL);
*/
void NvDumpSurface(
    NvRmSurface **ppSurface,
    NvU32 NumSurface,
    NvU8 *pFileName)
{
    NvU32 i, j;
    char Filename[256] = {0};
    NvU32 Timestamp = NvOsGetTimeMS();
    NvError Status;
    NvOsFileHandle hFile = NULL;
    NvU8 *pData = NULL;
    NvU32 SurfaceSize = 0;
    NvU8 Size[3];

    NvOsSleepMS(1000);

    if (pFileName)
        NvOsSnprintf(Filename, 256, "/data/yuvdumps/%s_%u",
            pFileName, Timestamp);
    else
        NvOsSnprintf(Filename, 256, "/data/yuvdumps/%u", Timestamp);

    SurfaceSize = 0;
    pData = (NvU8 *)NvOsAlloc(ppSurface[0]->Width * ppSurface[0]->Height +
                      ppSurface[1]->Width * ppSurface[1]->Height +
                      ppSurface[2]->Width * ppSurface[2]->Height);
    if (!pData)
        goto cleanup;

    for (i = 0; i < NumSurface; i++)
    {
        NvRmSurfaceRead(ppSurface[i], 0, 0,
                    ppSurface[i]->Width, ppSurface[i]->Height,
                    pData + SurfaceSize);

        ALOGDD("Surface[%d] = %d\n", i, *(pData + SurfaceSize));

        SurfaceSize += ppSurface[i]->Width * ppSurface[i]->Height;
    }

    NvOsSnprintf(Filename, 256, "%s_%dx%d.yuv", Filename,
        ppSurface[0]->Width, ppSurface[0]->Height);

    Status = NvOsFopen(Filename, NVOS_OPEN_CREATE | NVOS_OPEN_WRITE, &hFile);
    if (Status != NvSuccess)
        goto cleanup;

    Status = NvOsFwrite(hFile, pData, SurfaceSize);
    if (Status != NvSuccess)
        goto cleanup;

cleanup:
    NvOsFree(pData);
    NvOsFclose(hFile);
}

void NvCameraHal::SendDataCallback(
    NvCameraHal   *This,
    NvMMFrameCopy *FrameCopy,
    NvS32          MsgToCallback)
{
    camera_memory_t *shmem;
    shmem = This->mGetMemoryCb(
        -1,
        FrameCopy->bufferSize,
        1,
        This->mCbCookie);
    memcpy(
        shmem->data,
        FrameCopy->userBuffer,
        FrameCopy->bufferSize);
    This->DataCb(
        (int) MsgToCallback,
        shmem,
        This->mCbCookie);
}

void NvCameraHal::SendRawCallback(NvMMFrameCopy *FrameCopy)
{
    if (!mDNGEnabled)
    {
        SendDataCallback(this, FrameCopy, CAMERA_MSG_RAW_IMAGE);
    }
    else
    {
        FCamFrameParam param;
        memset(&param, 0, sizeof(param));

        NvU32 headerSize = 0x164; // TODO: this should come from nvrawfile.h

        if (FrameCopy->bufferSize < headerSize || memcmp(FrameCopy->userBuffer, "NVRAWFILE", 9))
            return;

        param.buffer = (const char*)FrameCopy->userBuffer + headerSize;

        param.width            = *(NvU32*)((NvU8*)FrameCopy->userBuffer + 0x24);
        param.height           = *(NvU32*)((NvU8*)FrameCopy->userBuffer + 0x28);
        param.fourcc           = *(NvU32*)((NvU8*)FrameCopy->userBuffer + 0x2C);
        param.bpp              = *(NvU32*)((NvU8*)FrameCopy->userBuffer + 0x30);
        param.timeSeconds      = *(NvU32*)((NvU8*)FrameCopy->userBuffer + 0x3C);
        param.timeMicroseconds = *(NvU32*)((NvU8*)FrameCopy->userBuffer + 0x40);

        NvF32 exposure = *(NvF32*)((NvU8*)FrameCopy->userBuffer + 0x70); // sec
        param.exposure = (int)(exposure * 1000000); // microseconds
        param.gain         = mRawCaptureGain; // EXIF_TAG_ISOSpeedRatings = gain * 100
        param.whiteBalance = mRawCaptureCCT;
        param.wbNeutral[0] = mRawCaptureWB[0];
        param.wbNeutral[1] = (mRawCaptureWB[1] + mRawCaptureWB[2]) / 2;
        param.wbNeutral[2] = mRawCaptureWB[3];

        for(unsigned i=0; i<12; i++)
        {
            param.rawToRGB3000K[i] = mRawCaptureCCM3000[i];
            param.rawToRGB6500K[i] = mRawCaptureCCM6500[i];
        }

        char str_manufacturer[PROPERTY_VALUE_MAX];
        property_get("ro.product.manufacturer", str_manufacturer, "NVIDIA");
        param.manufacturer = str_manufacturer;

        char str_model[PROPERTY_VALUE_MAX];
        property_get("ro.product.model", str_model, "Tegra");
        param.model = str_model;

        struct str_parms* tags = str_parms_create();
        if (!tags)
            return;

        NvF32 flashPower = *(NvU32*)((NvU8*)FrameCopy->userBuffer + 0x98);
        if (flashPower > 0)
            str_parms_add_int(tags, "flash.brightness", 1);

        NvMMFrameCopy DNGFrameCopy = *FrameCopy;
        int err = FCamSaveDNG(&param, tags, (char**)&DNGFrameCopy.userBuffer, &DNGFrameCopy.bufferSize);
        str_parms_destroy(tags);

        SendDataCallback(this, &DNGFrameCopy, CAMERA_MSG_RAW_IMAGE);

        FCamFreeBuffer((char*)DNGFrameCopy.userBuffer);
    }
}

static NvBool isEventUpdateCameraBlock(NvU32 EventType)
{
    switch(EventType)
    {
        case NvMMDigitalZoomEvents_FaceInfo:
        case NvMMDigitalZoomEvents_SmoothZoomFactor:
        case NvMMEventCamera_StillCaptureReady:
            return NV_TRUE;
            break;
        default:
            return NV_FALSE;
            break;
    }
}

NvError NvCameraHal::StartEventsUpdateCameraBlockThread(NvU32 EventType)
{
    NvError e = NvSuccess;
    mLockEventsUpdateCameraState.lock();
    // No one should be using UpdateCameraBlock thread
    NV_ASSERT(!mEventsUpdateCameraBlockThread);
    if (!mDontCreateEventsUpdateCameraBlockThread)
    {
        // Update AE/AF region in a different thread to prevent deadlock
        mEventTypeUpdatingCameraBlock = EventType;
        e = NvOsThreadCreate(
                EventsUpdateCameraBlock,
                this,
                &(mEventsUpdateCameraBlockThread));
    }
    mLockEventsUpdateCameraState.unlock();
    return e;
}

void NvCameraHal::StopEventsUpdateCameraBlockThread(void)
{
    mLockEventsUpdateCameraState.lock();
    if (mEventsUpdateCameraBlockThread)
    {
        NvOsThreadJoin(mEventsUpdateCameraBlockThread);
        mEventsUpdateCameraBlockThread = NULL;
        mEventTypeUpdatingCameraBlock  = 0;
    }
    mLockEventsUpdateCameraState.unlock();
}

// most mappings in il/nvmm/components/common/nvmmtransformbase.c : NvxNvMMTransformSendBlockEventFunction
// few others in il/nvmm/components/nvxcamera.c : nvxCameraEventCallback
// TODO: get rid of unused events?
NvError
NvCameraHal::NvMMEventHandler(
    void *Context,
    NvU32 EventType,
    NvU32 EventSize,
    void *EventInfo)
{
    NvError e = NvSuccess;
    NvMMBlockContainer *BlockContainer = (NvMMBlockContainer *)Context;
    NvCameraHal *This = NULL;

    ALOGVV("%s++ (event = 0x%x)", __FUNCTION__, EventType);
    if (!BlockContainer)
    {
        ALOGE(
            "%s:%d NULL context passed to event handler, ignoring event",
            __FUNCTION__,
            __LINE__);
        // don't return error, it might break the NvMM machinery, and if the
        // context is NULL it's *probably* the HAL's fault for not setting it
        // properly during init
        return e;
    }

    This = BlockContainer->MyHal;

    //Check if there exists another previous event using
    // eventsUpdateCameraBlockThread. If so and the incoming event will use
    // that thread, wait until the previous execution finished
    if (isEventUpdateCameraBlock(EventType))
    {
        This->StopEventsUpdateCameraBlockThread();
    }

    EventLock lock(This);

    switch (EventType)
    {
        // handle standard NvMM block events
        case NvMMEvent_SetStateComplete:
            BlockContainer->StateChangeDone.signal();
            break;
        case NvMMEvent_BlockClose:
            BlockContainer->BlockCloseDone = NV_TRUE;
            BlockContainer->BlockCloseDoneCond.signal();
            break;
        case NvMMEvent_SetAttributeComplete:
            BlockContainer->SetAttributeDone.signal();
            break;
        case NvMMEvent_BlockAbortDone:
        {
            NvMMEventBlockAbortDoneInfo *Info =
                (NvMMEventBlockAbortDoneInfo*)EventInfo;
            BlockContainer->Ports[Info->streamIndex].BlockAbortDone.signal();
            break;
        }
        case NvMMEvent_BlockWarning:
        {
            NvMMBlockWarningInfo *Info = (NvMMBlockWarningInfo *)EventInfo;
            ALOGWW("%s: %s threw BlockWarning 0x%x (%s)",
                __FUNCTION__,
                (BlockContainer == &This->Cam) ? "Camera" : "DZ",
                Info->warning, Info->additionalInfo);
            break;
        }
        case NvMMEvent_SetAttributeError:
        {
            NvMMSetAttributeErrorInfo *Info = (NvMMSetAttributeErrorInfo *)EventInfo;
            ALOGWW("%s: %s threw SetAttributeError 0x%x for attribute 0x%x",
                __FUNCTION__,
                (BlockContainer == &This->Cam) ? "Camera" : "DZ",
                Info->error, Info->AttributeType);
            break;
        }
        case NvMMEvent_BlockError:
        {
            NvMMBlockErrorInfo *Info = (NvMMBlockErrorInfo *)EventInfo;
            ALOGE("%s: %s threw BlockError 0x%x",
                __FUNCTION__,
                (BlockContainer == &This->Cam) ? "Camera" : "DZ",
                Info->error);
            break;
        }

        // buffer negotiation events all lump together
        case NvMMEventCamera_StreamBufferNegotiationComplete:
        {
            NvMMEventBlockBufferNegotiationCompleteInfo *Info =
                (NvMMEventBlockBufferNegotiationCompleteInfo *)EventInfo;
            BlockContainer->Ports[Info->streamIndex].BufferConfigDone = NV_TRUE;
            BlockContainer->Ports[Info->streamIndex].BufferConfigDoneCond.signal();
        }
        // intentional fall through
        case NvMMEvent_BlockBufferNegotiationComplete:
        case NvMMDigitalZoomEvents_StreamBufferNegotiationComplete:
        {
            NvMMEventBlockBufferNegotiationCompleteInfo *Info =
                (NvMMEventBlockBufferNegotiationCompleteInfo *)EventInfo;
            BlockContainer->Ports[Info->streamIndex].BufferNegotiationDone = NV_TRUE;
            BlockContainer->Ports[Info->streamIndex].BufferNegotiationDoneCond.signal();
            break;
        }

        // handle camera block events
        case NvMMEventCamera_AutoFocusAchieved:
            // win
            This->mGotAfComplete = NV_TRUE;
            This->NotifyCb(CAMERA_MSG_FOCUS, 1, 0, This->mCbCookie);
            This->mAFLockReceived.signal();
            break;
        case NvMMEventCamera_AutoFocusTimedOut:
            // fail
            This->mGotAfComplete = NV_TRUE;
            This->NotifyCb(CAMERA_MSG_FOCUS, 0, 0, This->mCbCookie);
            This->mAFLockReceived.signal();
            break;

        case NvMMEventCamera_AutoExposureAchieved:
            // intentional fall-through
        case NvMMEventCamera_AutoExposureTimedOut:
            // for now, just signal the condition variable that *something*
            // completed, regardless of win or fail
            This->mGotAeComplete = NV_TRUE;
            // use broadcast here since there are at least two places in
            // our code will wait for this condition, though they are not
            // likely to run at the same time.
            This->mAELockReceived.broadcast();
            break;
        case NvMMEventCamera_AutoWhiteBalanceAchieved:
            // intentional fall-through
        case NvMMEventCamera_AutoWhiteBalanceTimedOut:
            // for now, just signal the condition variable that *something*
            // completed, regardless of win or fail
            This->mGotAwbComplete = NV_TRUE;
            This->mAWBLockReceived.broadcast();

            break;
        case NvMMEventCamera_StillCaptureReady:
            ALOGVV("%s: Event handler got StillCaptureReady event\n", __FUNCTION__);
            if (This->mAeAwbUnlockNeeded)
            {
                This->mAeAwbUnlockNeeded = NV_FALSE;
                NV_CHECK_ERROR(This->StartEventsUpdateCameraBlockThread(EventType));
            }
            This->mIsCapturing = NV_FALSE;
            if (This->mMsgEnabled & CAMERA_MSG_SHUTTER)
            {
                This->mHalShutterCallbackTime = NvOsGetTimeMS();
                This->mHalShutterLag = This->mHalShutterCallbackTime - This->mHalCaptureStartTime;
                ALOGI("mHalShutterLag = %d ms", This->mHalShutterLag);
                This->NotifyCb(CAMERA_MSG_SHUTTER, 0, 0, This->mCbCookie);
            }
            This->mCaptureDone.broadcast();
            break;
        case NvMMEventCamera_StillCaptureProcessing:
            ALOGVV("%s: Event handler got StillCaptureProcessing event\n", __FUNCTION__);
            if (This->mMsgEnabled & CAMERA_MSG_RAW_IMAGE_NOTIFY)
            {
                This->NotifyCb(
                    CAMERA_MSG_RAW_IMAGE_NOTIFY,
                    0,
                    0,
                    This->mCbCookie);
            }
            break;
        case NvMMEventCamera_PreviewPausedAfterStillCapture:
            //Using NvMMDigitalZoomEvents_PreviewPausedAfterStillCapture event instead of
            //NvMMEventCamera_PreviewPausedAfterStillCapture for PostView Event happened
            break;
        case NvMMEventCamera_SensorModeChanged:
            ALOGI("%s: Event handler got SensorModeChanged event\n", __FUNCTION__);
            break;
#ifdef FRAMEWORK_HAS_MACRO_MODE_SUPPORT
        case NvMMEventCamera_EnterLowLight:
            if (This->mMsgEnabled & CAMERA_MSG_LOWLIGHT_ENTER)
            {
                ALOGI("%s: Event handler called LOWLIGHT ENTER\n", __FUNCTION__);
                This->NotifyCb(CAMERA_MSG_LOWLIGHT_ENTER, 0, 0, This->mCbCookie);
            }
            break;
        case NvMMEventCamera_ExitLowLight:
            if (This->mMsgEnabled & CAMERA_MSG_LOWLIGHT_EXIT)
            {
                ALOGI("%s: Event handler called LOWLIGHT EXIT\n", __FUNCTION__);
                This->NotifyCb(CAMERA_MSG_LOWLIGHT_EXIT, 0, 0, This->mCbCookie);
            }
            break;
        case NvMMEventCamera_EnterMacroMode:
            if (This->mMsgEnabled & CAMERA_MSG_MACROMODE_ENTER)
            {
                ALOGI("%s: Event handler called MACROMODE ENTER\n", __FUNCTION__);
                This->NotifyCb(CAMERA_MSG_MACROMODE_ENTER, 0, 0, This->mCbCookie);
            }
            break;
        case NvMMEventCamera_ExitMacroMode:
            if (This->mMsgEnabled & CAMERA_MSG_MACROMODE_EXIT)
            {
                ALOGI("%s: Event handler called MACROMODE EXIT\n", __FUNCTION__);
                This->NotifyCb(CAMERA_MSG_MACROMODE_EXIT, 0, 0, This->mCbCookie);
            }
            break;
#endif
        case NvMMEventCamera_FocusStartMoving:
            ALOGI("%s: Event handler called FOCUS_START_MOVING\n", __FUNCTION__);
            if (This->mPreviewStreaming && !This->mNumFaces)
            {
                This->SetFdState(FaceDetectionState_Pause, NV_TRUE);
            }
            This->NotifyCb(CAMERA_MSG_FOCUS_MOVE, 1, 0, This->mCbCookie);
            break;
        case NvMMEventCamera_FocusStopped:
            ALOGI("%s: Event handler called FOCUS_STOP_MOVING\n", __FUNCTION__);
            if (This->mFdPaused)
            {
                This->SetFdState(FaceDetectionState_Resume, NV_TRUE);
            }
            This->NotifyCb(CAMERA_MSG_FOCUS_MOVE, 0, 0, This->mCbCookie);
            break;
        case NvMMEventCamera_PowerOnComplete:
        {
            NvMMBlockErrorInfo *Info = (NvMMBlockErrorInfo *)EventInfo;
            ALOGI("%s received PowerOnComplete event", __FUNCTION__);

            char value[PROPERTY_VALUE_MAX];
            property_get("camera.mode.hotplug", value, 0);
            if (atoi(value) == 1)
            {
                if (Info != NULL)
                {
                   This->mSensorStatus = (NvError)Info->error;
                   This->mIsSensorPowerOnComplete = NV_TRUE;
                   This->mSensorStatusReceived.signal();
                }
            }
            This->mIsPowerOnComplete = NV_TRUE;
            This->mPowerOnCompleteReceived.signal();
            break;
        }
        case NvMMEventCamera_RawFrameCopy:
        {
            NvMMFrameCopy *FrameCopy = (NvMMFrameCopy *)EventInfo;
            ALOGVV("Event handler got NvMMEventCamera_RawFrameCopy event");
            if ((This->mMsgEnabled & CAMERA_MSG_RAW_IMAGE) || This->mDNGEnabled)
            {
                const NvCombinedCameraSettings &settings = This->mSettingsParser.getCurrentSettings();
                if (settings.CaptureMode == NvCameraUserCaptureMode_Shot2Shot)
                {
                    ALOGE("raw image callback in shot to shot mode isn't implemented!");
                }
                else
                {
                    This->SendRawCallback(FrameCopy);
                }
            }
            NvOsFree(FrameCopy->userBuffer);
            break;
        }

        // handle DZ block events
        // Should not receive this event anymore since we handle it in HAL now
        // todo: remove this event after cleanup DZ
        case NvMMDigitalZoomEvents_PreviewFrameCopy:
        {
            NvMMFrameCopy *FrameCopy = (NvMMFrameCopy *)EventInfo;
            ALOGW("Event handler received obsolete event"
                  "NvMMDigitalZoomEvents_PreviewFrameCopy!");
            if ((This->mMsgEnabled & CAMERA_MSG_PREVIEW_FRAME) && This->mPreviewStarted)
            {
                SendDataCallback(This, FrameCopy, CAMERA_MSG_PREVIEW_FRAME);
            }
            NvOsFree(FrameCopy->userBuffer);
            break;
        }
        // Should not receive this event anymore since we handle it in HAL now
        // todo: remove this event after cleanup DZ
        case NvMMDigitalZoomEvents_StillConfirmationFrameCopy:
        {
            NvMMFrameCopy *FrameCopy = (NvMMFrameCopy *)EventInfo;
            ALOGW("Event handler received obsolete event"
                  "NvMMDigitalZoomEvents_StillConfirmationFrameCopy!");
            if (This->mMsgEnabled & CAMERA_MSG_POSTVIEW_FRAME)
            {
                This->mHalPostViewCallbackTime = NvOsGetTimeMS();
                This->mHalPostViewLag = This->mHalPostViewCallbackTime - This->mHalCaptureStartTime;
                ALOGI("mHalPostViewLag = %d ms", This->mHalPostViewLag);

                SendDataCallback(This, FrameCopy, CAMERA_MSG_POSTVIEW_FRAME);
            }
            NvOsFree(FrameCopy->userBuffer);
            break;
        }
        case NvMMDigitalZoomEvents_FaceInfo:
        {
            NvMMFaceInfo *FaceEvent = (NvMMFaceInfo *)EventInfo;
            NvBool CreatedThread = NV_FALSE;

            This->mpFaces = (Face *)FaceEvent->faces;
            if (This->mMsgEnabled & CAMERA_MSG_PREVIEW_METADATA)
            {
                if (This->mFdPaused)
                {   // clear face overlay with a dummy call when FD is paused
                    This->mFaceMetaData->number_of_faces = 0;
                }
                else if (This->mPreviewStarted)
                {   // Encode face info
                    This->EncodePreviewMetaDataForFaces((Face *)FaceEvent->faces,
                                                    FaceEvent->numOfFaces);
                    // Program 3A regions with faces
                    if (!This->mDontCreateEventsUpdateCameraBlockThread &&
                        (FaceEvent->numOfFaces > 0 || This->mNeedToReset3AWindows))
                    {
                        // Update AE/AF region in a different thread to prevent deadlock
                        CreatedThread = NV_TRUE;
                        NV_CHECK_ERROR(This->StartEventsUpdateCameraBlockThread(EventType));
                    }
                }

                // Send callback
                if (This->mNumFaces || FaceEvent->numOfFaces || This->mFdPaused)
                {
                    camera_memory_t *shmem = This->mGetMemoryCb(-1,1,1,This->mCbCookie);
                    This->mNumFaces = FaceEvent->numOfFaces;
                    This->DataCb(CAMERA_MSG_PREVIEW_METADATA,
                                 shmem,
                                 This->mCbCookie,
                                 This->mFaceMetaData);
                }
            }
            // The thread will handle freeing the face data
            if (!CreatedThread)
            {
                NvOsFree(FaceEvent->faces);
                This->mpFaces = NULL;
            }
            break;
        }
        case NvMMDigitalZoomEvents_StillFrameCopy:
        {
            NvMMFrameCopy *FrameCopy = (NvMMFrameCopy *)EventInfo;
            ALOGVV("Event handler got NvMMDigitalZoomEvents_StillFrameCopy event");
            if (This->mMsgEnabled & CAMERA_MSG_RAW_IMAGE)
            {
                const NvCombinedCameraSettings &settings =
                    This->mSettingsParser.getCurrentSettings();
                if (settings.CaptureMode == NvCameraUserCaptureMode_Shot2Shot)
                {
                    ALOGE("raw image callback in shot to shot mode isn't implemented!");
                }
                else
                {
                    SendDataCallback(This, FrameCopy, CAMERA_MSG_RAW_IMAGE);
                }
            }
            NvOsFree(FrameCopy->userBuffer);
            break;
        }
        case NvMMDigitalZoomEvents_SmoothZoomFactor:
        {
            NvMMEventSmoothZoomFactorInfo *Info = (NvMMEventSmoothZoomFactorInfo *)EventInfo;
            NvS32 zoomValue = ZOOM_FACTOR_TO_ZOOM_VALUE(Info->ZoomFactor);
            NvBool ZoomAchieved = Info->SmoothZoomAchieved;
            if (Info->IsSmoothZoom)
            {
                UpdateSmoothZoom(This, zoomValue, ZoomAchieved);
            }

            if (This->mPreviewStarted)
            {
                // Update AE/AF region in a different thread to prevent deadlock
                NV_CHECK_ERROR(This->StartEventsUpdateCameraBlockThread(EventType));
            }
            break;
        }
        case NvMMDigitalZoomEvents_PreviewPausedAfterStillCapture:
            ALOGI("%s: Event handler got NvMMDigitalZoomEvents_PreviewPausedAfterStillCapture event\n", __FUNCTION__);
            if (This->mPreviewStarted)
            {
                This->mPreviewStarted = NV_FALSE;
                This->mPreviewEnabled = NV_FALSE;
                This->mPreviewStreaming = NV_FALSE;
                This->mPreviewPaused = NV_TRUE;
            }
            break;
        default:
            ALOGE("%s: unhandled event! [0x%x]", __FUNCTION__, EventType);
            break;
    }

    ALOGVV("%s--", __FUNCTION__);

    return e;
}

NvError
NvCameraHal::HandleStreamEvent(
    NvMMBlockContainer &BlockContainer,
    NvU32 StreamIndex,
    NvU32 EventType,
    void *EventInfo)
{
    ALOGVV("%s++ EventType=%d", __FUNCTION__, EventType);
    // NOTE: caller NvMMDeliverFullOutput assumes DZ is only block that
    // calls this for now.  if that changes, much of this function will need
    // to be updated
    switch (EventType)
    {
        case NvMMEvent_NewBufferConfiguration:
            // copy the buffer configuration
            BlockContainer.Ports[StreamIndex].BuffCfg =
                            *((NvMMNewBufferConfigurationInfo *)EventInfo);
        // intentional fall-through
        case NvMMEvent_NewBufferConfigurationReply:
            BlockContainer.Ports[StreamIndex].BufferConfigDone = NV_TRUE;
            BlockContainer.Ports[StreamIndex].BufferConfigDoneCond.signal();
            break;
        case NvMMEvent_NewBufferRequirements:
            ALOGI("%s: received NewBufferRequirements for stream index %d\n",
                __FUNCTION__, StreamIndex);
            break;
        case NvMMEvent_StreamShutdown:
            ALOGI("%s: received StreamShutdown for stream index %d\n",
                __FUNCTION__, StreamIndex);
            break;
        case NvMMEvent_StreamEnd:
        {
            if (StreamIndex == NvMMDigitalZoomStreamIndex_OutputPreview)
            {
                ALOGI("%s: received preview EOS event", __FUNCTION__);
                mPreviewStreaming = NV_FALSE;
                mPreviewEOSReceived.broadcast();
            }
            else if (StreamIndex == NvMMDigitalZoomStreamIndex_OutputVideo)
            {
                ALOGI("%s: received video EOS event", __FUNCTION__);
                mVideoStreaming = NV_FALSE;
                mVideoEOSReceived.broadcast();
            }
            else
            {
                ALOGI(
                    "%s: received EOS event on unhandled stream",
                    __FUNCTION__);
            }
            break;
        }
        case NvMMEvent_BlockError:
        {
            NvMMBlockErrorInfo *Info = (NvMMBlockErrorInfo *)EventInfo;
            ALOGE("%s: received BlockError 0x%x for stream index %d\n",
                __FUNCTION__, Info->error, StreamIndex);
            break;
        }
            break;
        default:
            ALOGE("%s: unhandled event! [0x%x]", __FUNCTION__, EventType);
            break;
    }
    ALOGVV("%s--", __FUNCTION__);
    return NvSuccess;
}

NvError
NvCameraHal::ProcessPreviewBufferAfterFrameCopy(
    NvMMBuffer *pBuffer)
{
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    //here we need APILock
    AutoMutex lock(mLock);

    NV_CHECK_ERROR_CLEANUP (
        HandlePostProcessPreview(pBuffer)
    );

    NV_CHECK_ERROR_CLEANUP (
        SendPreviewBufferDownstream(pBuffer)
    );

    ALOGVV("%s--", __FUNCTION__);

    return e;
fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError
NvCameraHal::NvMMDeliverFullOutput(
    void *Context,
    NvU32 StreamIndex,
    NvMMBufferType BufferType,
    NvU32 BufferSize,
    void *pBuffer)
{
    NvError e = NvSuccess;
    NvMMBlockContainer *BlockContainer = (NvMMBlockContainer *)Context;
    NvCameraHal *This = NULL;

    ALOGVV("%s++", __FUNCTION__);

    if (!BlockContainer)
    {
        ALOGE(
            "%s:%d NULL context passed to event handler, ignoring event",
            __FUNCTION__,
            __LINE__);
        // don't return error, it might break the NvMM machinery, and if the
        // context is NULL it's *probably* the HAL's fault for not setting it
        // properly during init
        return e;
    }

    This = BlockContainer->MyHal;

    // make sure we finish postprocessing the last payload buffer before
    // we grab the lock to start processing this one.
    This->JoinPostProcessingThreads(BufferType, StreamIndex);

    EventLock lock(This);

    ALOGVV("%s: %d", __FUNCTION__, __LINE__);
    if (BufferType == NvMMBufferType_StreamEvent)
    {
        NvMMEventType EventType = ((NvMMStreamEventBase *)pBuffer)->event;
        NV_CHECK_ERROR_CLEANUP(
            This->HandleStreamEvent(
                // DZ should be the only block calling this, since this func is
                // on the output stream.  if DZ goes away, this should change
                // to Cam
                This->DZ,
                StreamIndex,
                EventType,
                pBuffer)
        );
    }
    else if (BufferType  == NvMMBufferType_Payload)
    {
        NvMMBuffer *Buffer = (NvMMBuffer *)pBuffer;
        if (StreamIndex == NvMMDigitalZoomStreamIndex_OutputPreview)
        {
            ALOGV("%s: Preview Buffer arrives", __FUNCTION__);
            NV_CHECK_ERROR_CLEANUP(
                This->EmptyPreviewBuffer(Buffer)
            );
        }
        else if (StreamIndex == NvMMDigitalZoomStreamIndex_OutputStill)
        {
            This->mHalStillYUVBufferFromDZTime = NvOsGetTimeMS();
            This->mHalStillYUVBufferFromDZLag =
                This->mHalStillYUVBufferFromDZTime - This->mHalCaptureStartTime;
            ALOGI("mHalStillYUVBufferFromDZLag = %d ms", This->mHalStillYUVBufferFromDZLag);
            NV_CHECK_ERROR_CLEANUP(
                This->EmptyStillBuffer(Buffer)
            );
            const NvCombinedCameraSettings &settings =
                This->mSettingsParser.getCurrentSettings();
            NV_CHECK_ERROR_CLEANUP(
                This->SetFlashMode(settings)
            );
        }
        else if (StreamIndex == NvMMDigitalZoomStreamIndex_OutputVideo)
        {
            NV_CHECK_ERROR_CLEANUP(
                This->EmptyVideoBuffer(Buffer)
            );
        }
    }
    else
    {
        ALOGVV("%s received something else!", __FUNCTION__);
    }

    ALOGVV("%s--", __FUNCTION__);

    return e;
fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

void NvCameraHal::JoinPostProcessingThreads(NvU32 BufferType, NvU32 StreamIndex)
{
    NvOsThreadHandle *pHandle = NULL;
    // this only makes sense for payload buffers
    if (BufferType != NvMMBufferType_Payload)
    {
        return;
    }

    // this function can be called from multiple threads, we need to make
    // sure we only try to join existing postproc threads once to avoid
    // deadlock...safest way to ensure that is to just mutex protect the
    // state checking + thread join
    NvOsMutexLock(mPostProcThreadJoinMutex);

    switch (StreamIndex)
    {
        case NvMMDigitalZoomStreamIndex_OutputPreview:
            pHandle = &mPreviewPostProcThreadHandle;
            break;
        case NvMMDigitalZoomStreamIndex_OutputStill:
            pHandle = &mStillPostProcThreadHandle;
            break;
        case NvMMDigitalZoomStreamIndex_OutputVideo:
            pHandle = &mVideoPostProcThreadHandle;
            break;
        default:
            // nothing to do here
            pHandle = NULL;
            break;
    }

    if (pHandle && *pHandle)
    {
        NvOsThreadJoin(*pHandle);
        *pHandle = NULL;
    }

    NvOsMutexUnlock(mPostProcThreadJoinMutex);

    return;
}

NvBool NvCameraHal::CheckPostviewCallback(NvMMBuffer *Buffer)
{
    const NvCombinedCameraSettings &Settings =
        mSettingsParser.getCurrentSettings();
    NvBool isLast = Buffer->PayloadInfo.BufferMetaData.
        DigitalZoomBufferMetadata.isEndOfStillSession;
    NvBool sendPostviewCb = NV_FALSE;
    NvU32 bracketMidFrameIdx = 0;

    if (Settings.stillCapHdrOn)
    {
        // For HDR we don't generate postview image from preview frames
        // This is because HDR algorithm is expensive so should only be ran
        // on still frames. The postview callback will be sent in still frame
        // handling code.
        sendPostviewCb = NV_FALSE;
    }
    else if (Settings.bracketCaptureCount != 0)
    {
        // For bracket capture we send the middle frame with an implicit
        // assumption that the middle frame has least exposure bias
        bracketMidFrameIdx = (Settings.bracketCaptureCount + 1) / 2;
        mNumBracketPreviewRecieved++;
        if (mNumBracketPreviewRecieved == bracketMidFrameIdx)
        {
            sendPostviewCb = NV_TRUE;
        }
    }
    else if (isLast)
    {
        // For all other capture flows we just send the last frame
        sendPostviewCb = NV_TRUE;
    }

    return sendPostviewCb;
}

NvError NvCameraHal::HandlePostProcessPreview(NvMMBuffer *Buffer)
{
    NvError e = NvSuccess;
    NvBool OutputReady = NV_FALSE;

    // if we're in the middle of stopping, skip this and send the frames
    // downstream like we normally would.  TODO maybe we should swallow and
    // send directly back to DZ instead?
    if (mPostProcessPreview->Enabled() && !mStopPostProcessingPreview)
    {
        NV_CHECK_ERROR_CLEANUP(
            LaunchPostProcThread(
                Buffer,
                NvMMDigitalZoomStreamIndex_OutputPreview,
                &mPreviewPostProcThreadHandle)
        );
        // exit immediately, preview postproc thread will send the buffer
        // downstream once it's done processing it.
        return e;
    }
    return e;
fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::EmptyPreviewBuffer(NvMMBuffer *Buffer)
{
    NvError e = NvSuccess;
    NvBool DoPostviewCallback = NV_FALSE;
    NvBool SendBufferDownstream = NV_FALSE;

    NvMMPayloadMetadata &PayloadInfo = Buffer->PayloadInfo;
    NvBufferMetadataType BufMetaType = PayloadInfo.BufferMetaDataType;
    NvMMDigitalZoomBufferMetadata &DzMetaData =
        PayloadInfo.BufferMetaData.DigitalZoomBufferMetadata;

    ALOGV("%s: ++", __FUNCTION__);
    // join PostviewFrameCopy thread before processing following preview
    if (mPostviewFrameCopy && DzMetaData.isStillPreview)
    {
        mPostviewFrameCopy->CheckAndWaitWorkDone();
    }

    // Handle postview callaback
    if (mPostviewFrameCopy &&
        mPostviewFrameCopy->Enabled() &&
        (BufMetaType == NvMMBufferMetadataType_DigitalZoom) &&
         DzMetaData.isStillPreview)
    {
        DoPostviewCallback = CheckPostviewCallback(Buffer);
        if (DoPostviewCallback)
        {
            // if SendBufferDownstream is true, it means this thread will exit,
            // postview frame copy thread will send buffer to postprocessing and ANW.
            // if SendBufferDownstream is false, postview frame copy will not send
            // buffer, this thread will wait postview frame copy finish
            // keep this variable in order to select between these two choices
            SendBufferDownstream = NV_FALSE;
            NV_CHECK_ERROR_CLEANUP (
                mPostviewFrameCopy->DoFrameCopyAndCallback(Buffer, SendBufferDownstream)
            );
            // exit here
            if (SendBufferDownstream == NV_TRUE)
            {
                return e;
            }
        }
    }

    if (mPostviewFrameCopy && DoPostviewCallback && !SendBufferDownstream)
    {
        // check and wait until postview is done
        mPostviewFrameCopy->CheckAndWaitWorkDone();
    }

    NV_CHECK_ERROR_CLEANUP (
        HandlePostProcessPreview(Buffer)
    );

#if !NV_CAMERA_V3
    if (DzMetaData.isBracketedBurstPreview)
    {
        SendEmptyPreviewBufferToDZ(Buffer);
    }
    else
    {
        NV_CHECK_ERROR_CLEANUP (
            SendPreviewBufferDownstream(Buffer)
            );
    }
#else
    NV_CHECK_ERROR_CLEANUP (
        getPreviewBufferFromDZ(Buffer)
    );
#endif

    return e;
fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::SendPreviewBufferDownstream(NvMMBuffer *Buffer)
{
    NvError e = NvSuccess;
    buffer_handle_t *anbToFill;
    // Initialize it to an invalid index so when this function fails
    // we know we have found the correct index or not
    NvU32 BufferIndex = NvCameraMemProfileConfigurator::MAX_NUMBER_OF_BUFFERS;
    int err;
    int stride;
    NvCameraBufferConfigStage stage =
        m_pMemProfileConfigurator->GetBufferConfigStage();
    NvU32 previewBufferNum;
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            stage, NVCAMERA_BUFFERCONFIG_DZ_PREVIEW,
            NULL, &previewBufferNum)
    );

    ALOGV("%s++", __FUNCTION__);

    // find the preview buffer array element that matches
    // loop is fine since this array should only have a few elements
    for (BufferIndex = 0;
         BufferIndex < previewBufferNum;
         BufferIndex++)
    {
        if (mPreviewBuffers[BufferIndex].nvmm->BufferID == Buffer->BufferID)
        {
            break;
        }
    }
    if (BufferIndex == previewBufferNum)
    {
        ALOGE("%s: can't find matching ANB for NvMM buffer!", __FUNCTION__);
        return NvError_BadParameter;
    }

    // TODO refactor this, it's long and handles too many things
    // (could probably split into logic to empty one buffer, and then to supply
    //  the next)
    if (mPreviewStarted || mPreviewStreaming || mPreviewPaused)
    {
         if (!mPreviewStreaming && !mPreviewPaused)
        {
            mDelayedSettingsComplete = NV_FALSE;
            NvOsThreadCreate(DoStuffThatNeedsPreview, this, &mThreadToDoStuffThatNeedsPreview);
            mPreviewStreaming = NV_TRUE;
            mFirstPreviewFrameReceived.broadcast();
            ALOGI("%s received first Preview frame", __FUNCTION__);
#if ENABLE_NVIDIA_CAMTRACE
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_HAL_SWITCH_EXIT,
            NvOsGetTimeUS() ,
            3);
    TRACE_EVENT(__func__,
            CAM_EVENT_TYPE_HAL_LAUNCH_EXIT,
            NvOsGetTimeUS() ,
            3);
#endif
        }
        else
        {
            RunFdStateMachine();
        }

        // wait for mFinalWaitCountInFrames
        if (mMasterWaitFlagForSettings)
        {
            if (mCurrentPreviewFramesForWait < mFinalWaitCountInFrames)
            {
                mCurrentPreviewFramesForWait++;
            }
            else
            {
                mMasterWaitFlagForSettings = NV_FALSE;
                mCurrentPreviewFramesForWait = 0;
                mFinalWaitCountInFrames = 0;
                mMasterWaitForSettingsCond.signal();
            }
        }
        mCameraLogging.NvCameraPreviewProfile();

        // Store the buffer handle that DZ returned, and use it
        // to stopPreview if preview window is destroyed.
        anbToFill = mPreviewBuffers[BufferIndex].anb;

        if(mPreviewWindowDestroyed == NV_FALSE)
        {
            // if we have called stoppreview, we don't want to send
            // preview frame copies, but EmptyPreviewBuffer will still
            // be called untill preview stream is completely stopped.
            if (mPreviewFrameCopy->Enabled() && mPreviewStarted)
            {
                // blocking wait on preview frame copy
                NV_CHECK_ERROR_CLEANUP (
                    mPreviewFrameCopy->DoFrameCopyAndCallback(Buffer, NV_FALSE)
                );
            }

            // send this buffer to app
            err = mPreviewWindow->enqueue_buffer(
                mPreviewWindow,
                mPreviewBuffers[BufferIndex].anb);

            if (err != 0)
            {
                ALOGE("%s: enqueue_buffer failed [%d]", __FUNCTION__, err);
                mPreviewBuffers[BufferIndex].anb = anbToFill;
                ANWDestroyed();
            }
        }

        if(mPreviewWindowDestroyed == NV_FALSE)
        {
            // clear reference, these are refcounted!
            mPreviewBuffers[BufferIndex].anb = NULL;

            // get the next one
            err = mPreviewWindow->dequeue_buffer(
                    mPreviewWindow,
                    &(mPreviewBuffers[BufferIndex].anb),
                    &stride);
            if (err != 0)
            {
                ALOGE("%s: dequeue_buffer failed [%d]", __FUNCTION__, err);
                mPreviewBuffers[BufferIndex].anb = anbToFill;
                ANWDestroyed();
            }
        }

        if(mPreviewWindowDestroyed == NV_FALSE)
        {
            err = mPreviewWindow->lock_buffer(
                mPreviewWindow,
                mPreviewBuffers[BufferIndex].anb);
            if (err != 0)
            {
                ALOGE("%s: lock_buffer failed [%d]", __FUNCTION__, err);
            // try to cancel the buffer, we don't care if this fails because
            // we've already failed and it's the best we can do
                mPreviewWindow->cancel_buffer(
                    mPreviewWindow,
                    mPreviewBuffers[BufferIndex].anb);

                mPreviewBuffers[BufferIndex].anb = anbToFill;
                ANWDestroyed();
            }
        }

        //make sure frame copy is done before we return the buffer
        mPreviewFrameCopy->CheckAndWaitWorkDone();

        /*
         * Initialize ANBs only if we were successful in dequeing and locking a
         * new buffer. Else we pass the same buffer that DZ sent, hence
         * no need to import again.
         */
        if(mPreviewWindowDestroyed == NV_FALSE)
        {
            InitializeNvMMWithANB(&mPreviewBuffers[BufferIndex], BufferIndex);
        }

        SendEmptyPreviewBufferToDZ(mPreviewBuffers[BufferIndex].nvmm);
    }
    else if (Buffer->PayloadInfo.BufferMetaDataType ==
             NvMMBufferMetadataType_DigitalZoom)
    {
        // Shouldn't reach here!
        ALOGE("Received preview frame in wrong state!");
        NV_ASSERT(0);
        // Still send buffer back to DZ to prevent leakage
        SendEmptyPreviewBufferToDZ(mPreviewBuffers[BufferIndex].nvmm);
    }

    ALOGV("%s--", __FUNCTION__);
    return e;

fail:
    if (BufferIndex < previewBufferNum)
    {
        SendEmptyPreviewBufferToDZ(mPreviewBuffers[BufferIndex].nvmm);
    }
    else
    {
        ALOGE("%s fails to return preview buffer to DZ!", __FUNCTION__);
    }
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

void NvCameraHal::ANWDestroyed()
{
    mPreviewWindowDestroyed = NV_TRUE;
    NvOsThreadCreate(ANWFailureHandler,this,&mANWFailureHandlerThread);
}

void NvCameraHal::ANWFailureHandler(void *args)
{
    NvCameraHal *This = (NvCameraHal*)args;
    This->stopPreview();
}

NvError NvCameraHal::EmptyStillBuffer(NvMMBuffer *Buffer)
{
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    // encode.  don't return buffer to DZ yet, we need to wait until jpeg
    // encoder is done with it.  jpeg encoder will let us know when it's done
    // with this buffer.

    // For the video snapshot case, NvMM will not send us the
    // NvMMEventCamera_StillCaptureProcessing event. So we send the raw notification
    // here.
    if (mVideoEnabled && (mMsgEnabled & CAMERA_MSG_RAW_IMAGE_NOTIFY))
    {
        NotifyCb(
            CAMERA_MSG_RAW_IMAGE_NOTIFY,
            0,
            0,
            mCbCookie);
    }

    // handle postproc separately, it follows its own set of encoding rules
    if (mPostProcessStill->Enabled())
    {
        NV_CHECK_ERROR_CLEANUP(
            LaunchPostProcThread(
                Buffer,
                NvMMDigitalZoomStreamIndex_OutputStill,
                &mStillPostProcThreadHandle)
        );

        ALOGVV("%s--", __FUNCTION__);
        return e;
    }

    NV_CHECK_ERROR_CLEANUP(
            ProcessOutBuffer(Buffer)
            );

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE(
        "%s-- something failed, releasing resources and aborting!",
        __FUNCTION__);
    // most of the potential errors here are from allocs
    return NvError_InsufficientMemory;
}

NvError NvCameraHal::LaunchPostProcThread(
    NvMMBuffer *Buffer,
    NvU32 StreamIndex,
    NvOsThreadHandle *pHandle)
{
    NvError e = NvSuccess;

    // alloc some space to give thread args, guarantees they remain in valid
    // memory until the thread has received them.  thread will free them.
    PostProcThreadArgs *args =
        (PostProcThreadArgs *)NvOsAlloc(sizeof(PostProcThreadArgs));
    if (!args)
    {
        ALOGE(
            "%s: failed to allocate memory for postproc thread arg struct!",
            __FUNCTION__);
        e = NvError_InsufficientMemory;
        return e;
    }

    args->This = this;
    args->StreamIndex = StreamIndex;
    // this memcpy makes it so that we don't need to worry about creating
    // special copy buffers...it's built into the args struct
    NvOsMemcpy(&args->Buffer, Buffer, sizeof(NvMMBuffer));

    e = NvOsThreadCreate(PostProcThread, args, pHandle);

    return e;
}

void NvCameraHal::PostProcThread(void *args)
{
    NvError e = NvSuccess;

    if (!args)
    {
        // can't goto fail here, it throws a compile error about skipping over
        // initialization of vars.  but since this is a NULL check that
        // we can't init vars because of, we have to return immediately and
        // log the error.
        e = NvError_BadParameter;
        ALOGE("%s-- error, args invalid! [0x%x]", __FUNCTION__, e);
        return;
    }

    PostProcThreadArgs *myArgs = (PostProcThreadArgs *)args;
    NvCameraHal *This = myArgs->This;
    NvMMBuffer *Buffer = &myArgs->Buffer;
    NvU32 StreamIndex = myArgs->StreamIndex;

    APILock lock(This);

    switch (StreamIndex)
    {
        case NvMMDigitalZoomStreamIndex_OutputPreview:
            NV_CHECK_ERROR_CLEANUP(
                This->EmptyPreviewPostProcBuffer(Buffer)
            );
            break;
        case NvMMDigitalZoomStreamIndex_OutputStill:
            NV_CHECK_ERROR_CLEANUP(
                This->EmptyStillPostProcBuffer(Buffer)
            );
            break;
        case NvMMDigitalZoomStreamIndex_OutputVideo:
            NV_CHECK_ERROR_CLEANUP(
                This->EmptyVideoPostProcBuffer(Buffer)
            );
            break;
        default:
            // any others streams don't make sense
            e = NvError_BadParameter;
            goto fail;
            break;
    }

    // free the struct that the caller allocated to give us the args
    NvOsFree(args);

    ALOGVV("%s--", __FUNCTION__);
    return;

fail:
    NvOsFree(args);
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return;
}

NvError NvCameraHal::EmptyPreviewPostProcBuffer(NvMMBuffer *Buffer)
{
    NvError e = NvSuccess;
    NvBool OutputReady = NV_FALSE;
    ALOGV("%s++", __FUNCTION__);

    // send buffer to post processing
    NV_CHECK_ERROR_CLEANUP (
        mPostProcessPreview->ProcessBuffer(Buffer, OutputReady)
    );

    if (OutputReady == NV_FALSE)
    {
        ALOGVV("%s--", __FUNCTION__);
        return e;
    }
    NV_CHECK_ERROR_CLEANUP(
        mPostProcessPreview->GetOutputBuffer(Buffer)
    );

    NV_CHECK_ERROR_CLEANUP(
        SendPreviewBufferDownstream(Buffer)
    );

    ALOGV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::EmptyStillPostProcBuffer(NvMMBuffer *Buffer)
{
    NvBool OutputReady = NV_FALSE;
    NvError e = NvSuccess;
    ALOGVV("%s++", __FUNCTION__);

    // send source buffer to post processing
    NV_CHECK_ERROR_CLEANUP (
        mPostProcessStill->ProcessBuffer(Buffer, OutputReady)
    );

    // if postproc class encodes output on its own, we're all done!
    if (mPostProcessStill->EncodesOutput())
    {
        ALOGVV("%s--", __FUNCTION__);
        return e;
    }

    // if the postproc class doesn't encode the output on its own,
    // we have to get the output when it's ready and feed it to the encoder.
    if (OutputReady)
    {
        NV_CHECK_ERROR_CLEANUP(
            mPostProcessStill->GetOutputBuffer(Buffer)
        );

        // JPEG encoder frees this buffer on its own when it comes back
        NV_CHECK_ERROR_CLEANUP(
            FeedJpegEncoder(Buffer)
        );
    }

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    NvOsFree(Buffer);
    return e;
}

NvError NvCameraHal::EmptyVideoPostProcBuffer(NvMMBuffer *Buffer)
{
    NvError e = NvSuccess;
    NvBool OutputReady = NV_FALSE;
    ALOGV("%s++", __FUNCTION__);

    // send buffer to post processing
    NV_CHECK_ERROR_CLEANUP (
            mPostProcessVideo->ProcessBuffer(Buffer, OutputReady)
    );

    if (OutputReady == NV_FALSE)
    {
        ALOGV("%s--", __FUNCTION__);
        return e;
    }

    NV_CHECK_ERROR_CLEANUP (
        mPostProcessVideo->GetOutputBuffer(Buffer)
    );

    SendVideoBufferDownstream(Buffer);

    ALOGV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::EmptyVideoBuffer(NvMMBuffer *Buffer)
{
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    if (!mVideoStarted)
    {
        // we should be stopped, but we got a buffer anyways.
        // politely return it and exit
        SendEmptyVideoBufferToDZ(Buffer);
        ALOGVV("%s-- (video not started)", __FUNCTION__);
        return e;
    }

    if (!mVideoStreaming)
    {
        mVideoStreaming = NV_TRUE;
        mFirstVideoFrameReceived.broadcast();
        ALOGI("%s received first video frame", __FUNCTION__);
    }

    // this check is intentionally placed *after* the broadcast,
    // since we should still do that in the case where the app doesn't want
    // the frames.
    // if app doesn't want the frames, and we're just going to swallow them,
    // don't bother postprocessing them.
    if (!(mMsgEnabled & CAMERA_MSG_VIDEO_FRAME))
    {
        // app doesn't want any frames, politey return it and exit
        SendEmptyVideoBufferToDZ(Buffer);
        ALOGVV("%s-- (video frame msgs disabled)", __FUNCTION__);
        return e;
    }

    // if we're in the middle of stopping, skip this and send the frames
    // downstream like we normally would.  TODO maybe we should swallow and
    // send directly back to DZ instead?
    if (mPostProcessVideo->Enabled() && !mStopPostProcessingVideo)
    {
        NV_CHECK_ERROR_CLEANUP(
            LaunchPostProcThread(
                Buffer,
                NvMMDigitalZoomStreamIndex_OutputVideo,
                &mVideoPostProcThreadHandle)
        );
        // exit immediately, video postproc thread will send the buffer
        // downstream once it's done processing it.
        return e;
    }

    NV_CHECK_ERROR_CLEANUP(
        SendVideoBufferDownstream(Buffer)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::SendVideoBufferDownstream(NvMMBuffer *Buffer)
{
    NvError e = NvSuccess;
    ALOGV("%s++", __FUNCTION__);

    if (mUsingEmbeddedBuffers)
    {
        NV_CHECK_ERROR_CLEANUP(
            SendEmbeddedVideoBufferToApp(Buffer)
        );
    }
    else
    {
        NV_CHECK_ERROR_CLEANUP(
            SendNonEmbeddedVideoBufferToApp(Buffer)
        );
    }

    ALOGV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::SendEmbeddedVideoBufferToApp(NvMMBuffer *nvmmBuffer)
{
    NvError e = NvSuccess;
    VideoBuffer *videoBuffer = NULL;
    camera_memory_t *shmem = NULL;
    OMX_U32 type = kMetadataBufferTypeCameraSource;

    // this is the typical case

    // get pointer to the empty video buffer
    videoBuffer = GetEmptyVideoBuffer();
    if (!videoBuffer)
    {
        //error!
        ALOGE(
            "%s: no empty video buffers available to send full buffer",
            __FUNCTION__);
        return NvError_InvalidState;
    }

    // copy NvMM buffer info into the OMX buffer
    // loosely based on code in NvxNvMMTransformDeliverFullOutputInternal from
    // old OMX code
    NvOsMemcpy(
        videoBuffer->OmxBufferHdr.pBuffer,
        nvmmBuffer,
        sizeof(NvMMBuffer));
    // old OMX code did |= here, but this should be the only flag we care about
    videoBuffer->OmxBufferHdr.nFlags = OMX_BUFFERFLAG_NV_BUFFER;
    videoBuffer->OmxBufferHdr.nFilledLen = sizeof(NvMMBuffer);
    // buffer timestamp...do we need this?
    // can we merge it with internal timestamp tracking code?
    if ((NvS64)nvmmBuffer->PayloadInfo.TimeStamp >= 0)
    {
        videoBuffer->OmxBufferHdr.nTimeStamp =
            ((OMX_TICKS)(nvmmBuffer->PayloadInfo.TimeStamp)/ 10);
    }
    else
    {
        videoBuffer->OmxBufferHdr.nTimeStamp = -1;
    }

    // update our internal timestamp
    updateVideoTimeStampFromBuffer(&videoBuffer->OmxBufferHdr);

    // mark, allocate, copy, and deliver
    videoBuffer->isDelivered = NV_TRUE;
    shmem = mGetMemoryCb(
        -1,
        // we'll copy the buffer header, and also the type (OMX_U32 is + 4)
        sizeof(OMX_BUFFERHEADERTYPE) + 4,
        1,
        mCbCookie);
    if (!shmem)
    {
        return NvError_InsufficientMemory;
    }
    NvOsMemcpy(shmem->data, &type, 4);
    NvOsMemcpy(
        (NvU8 *)(shmem->data) + 4,
        &videoBuffer->OmxBufferHdr,
        sizeof(OMX_BUFFERHEADERTYPE));

    DataCbTimestamp(
        mNsCurVideoTimeStamp,
        CAMERA_MSG_VIDEO_FRAME,
        shmem,
        mCbCookie);

    return e;
}

NvError NvCameraHal::SendNonEmbeddedVideoBufferToApp(NvMMBuffer *Buffer)
{
    NvError e = NvSuccess;
    camera_memory_t *shmem = NULL;
    OMX_BUFFERHEADERTYPE OmxBufferHdr;

    ALOGVV("%s++", __FUNCTION__);

    // spoof an OMX buffer to update our timestamp.  it'd be good to clean
    // this up a bit later to share code with the embedded buffers in a way
    // that didn't require any spoofing anywhere.
    if ((NvS64)Buffer->PayloadInfo.TimeStamp >= 0)
    {
        OmxBufferHdr.nTimeStamp =
            ((OMX_TICKS)(Buffer->PayloadInfo.TimeStamp)/ 10);
    }
    else
    {
        OmxBufferHdr.nTimeStamp = -1;
    }

    updateVideoTimeStampFromBuffer(&OmxBufferHdr);

    // allocates shmem for us and copies YUV data into it.
    NV_CHECK_ERROR_CLEANUP(
        CameraGetUserYUV(Buffer, &shmem, NVMM_VIDEO_FRAME_FormatIYUV)
    );

    // send to the app
    DataCbTimestamp(
       mNsCurVideoTimeStamp,
       CAMERA_MSG_VIDEO_FRAME,
       shmem,
       mCbCookie);

    // return used buffer to DZ.  since we did a direct copy, we don't
    // need to wait for the app to return it.
    SendEmptyVideoBufferToDZ(Buffer);

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

void NvCameraHal::updateVideoTimeStampFromBuffer(OMX_BUFFERHEADERTYPE *pBuffer)
{
    OMX_S64 usFrameTimeInterval = 0;

    if (mNumOfVideoFramesDelivered == 0)
    {
        mNsCurVideoTimeStamp = systemTime();
    }
    else
    {
        usFrameTimeInterval = pBuffer->nTimeStamp - mUsPrevVideoTimeStamp;
        if (usFrameTimeInterval > 0)
        {
            mNsCurVideoTimeStamp += usFrameTimeInterval * 1000;
        }
        else
        {
            ALOGE(
                "Video frame #%lu time stamp is out of order!!!",
                    mNumOfVideoFramesDelivered);
        }
        // If this frame jitter is larger than threshold
        if (usFrameTimeInterval > FRAME_JITTER_THRESHOLD_US)
            ALOGE("Video frame #%lu has big jitter: %fmiliseconds",
                mNumOfVideoFramesDelivered, usFrameTimeInterval / 1000.0);
    }
    if (usFrameTimeInterval >= 0)
    {
        mUsPrevVideoTimeStamp = pBuffer->nTimeStamp;
    }
    mNumOfVideoFramesDelivered++;
}



void NvCameraHal::debugBuffer(char *msg, NvMMBuffer *Buf)
{
    ALOGDD("%s",msg);
    ALOGDD("NvU32 structSize                       = %d ", Buf->StructSize);
    ALOGDD("NvMMEventType event                    = %d ", Buf->BufferID);
    ALOGDD("NvU32 PayloadType                      = %d ", Buf->PayloadType);
    ALOGDD("NvMMBuffer surface ColorFormat         = %d ",
                            Buf->Payload.Surfaces.Surfaces[0].ColorFormat);
    ALOGDD("NvMMBuffer surface Height              = %d ",
                                Buf->Payload.Surfaces.Surfaces[0].Height);
    ALOGDD("NvMMBuffer surface Width               = %d ",
                                Buf->Payload.Surfaces.Surfaces[0].Width);


}

void NvCameraHal::debugBufferCfg(char *msg, NvMMNewBufferConfigurationInfo *BufCfg)
{
    ALOGDD("%s",msg);
    ALOGDD("NvU32 structSize                = %d ", BufCfg->structSize);
    ALOGDD("NvMMEventType event             = %d ", BufCfg->event);
    ALOGDD("NvU32 numBuffers                = %d ", BufCfg->numBuffers);
    ALOGDD("NvU32 bufferSize                = %d ", BufCfg->bufferSize);
    ALOGDD("NvU16 byteAlignment             = %d ", BufCfg->byteAlignment);
    ALOGDD("NvU8  bPhysicalContiguousMemory = %d ",
                                                        BufCfg->bPhysicalContiguousMemory);
    ALOGDD("NvU8  bInSharedMemory           = %d ", BufCfg->bInSharedMemory);
    ALOGDD("NvMMMemoryType memorySpace      = %d ", BufCfg->memorySpace);
    ALOGDD("NvMMBufferEndianess endianness  = %d ", BufCfg->endianness);
    ALOGDD("NvMMBufferFormatId formatId     = %d ", BufCfg->formatId);
    ALOGDD("NvMMBufferFormat format  - ColorFormat       = %d ",
                            BufCfg->format.videoFormat.SurfaceDescription[0].ColorFormat);
    ALOGDD("NvMMBufferFormat format  - Height            = %d ",
                                BufCfg->format.videoFormat.SurfaceDescription[0].Height);
    ALOGDD("NvMMBufferFormat format  - Width             = %d ",
                                BufCfg->format.videoFormat.SurfaceDescription[0].Width);
}


void
NvCameraHal::JpegEncoderDeliverFullOutput(
    void *ClientContext,
    NvU32 StreamIndex,
    NvU32 BufferSize,
    void *pBuffer,
    NvError EncStatus)
{
    NvMMBuffer *Buffer = (NvMMBuffer *)pBuffer;
    NvCameraHal *This = (NvCameraHal *)ClientContext;
    NvU32 NumJpegBuffers = 0;
    NvMMBuffer *DiscardedBuffer = NULL;

    EventLock lock(This);
    ALOGVV("%s++", __FUNCTION__);

    switch (StreamIndex)
    {
        case NvImageEncoderStream_INPUT:
        {
            ALOGVV("%s: got input stream buffer, returning to DZ", __FUNCTION__);

            if (This->mPostProcessStill->Enabled() &&
                This->mPostProcessStill->EncodesOutput())
            {
                // postproc class handles encode/free on its own, just give
                // it the buffer back and let it know that we've given it
                // a buffer
                This->mPostProcessStill->ReturnBufferAfterEncoding(Buffer);
                This->mJpegBufferReturnedToPostProc.signal();
            }
            else
            {
#if !NV_CAMERA_V3
                // encoder is done with the input buffer, we can give it back to DZ
                This->SendEmptyStillBufferToDZ(Buffer);
                NvOsFree(Buffer);
#endif
            }
            break;
        }
        case NvImageEncoderStream_THUMBNAIL_INPUT:
            ALOGVV("%s: got thumbnail input buffer, freeing", __FUNCTION__);
            This->mScaler.FreeSurface(&Buffer->Payload.Surfaces);
            NvOsFree(Buffer);
            break;
        case NvImageEncoderStream_OUTPUT:
        {
            NvError status = NvSuccess;
            // For Jpeg Encoder, we only use the launch stage value
            NvU32 jpegBufferNum;
            status = This->m_pMemProfileConfigurator->GetBufferAmount(
                NVCAMERA_BUFFERCONFIG_STAGE_LAUNCH, NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT,
                NULL, &jpegBufferNum);
            if (status != NvSuccess)
            {
                ALOGVV("%s: Getting buffer configuration stage failed", __FUNCTION__);
                return;
            }

            NumJpegBuffers = NvMMQueueGetNumEntries(This->mFilledOutputBufferQueue);

            if ((NumJpegBuffers == jpegBufferNum - 1) && (This->mFastburstStarted))
            {
                NvOsSemaphoreWait(This->mhSemFilledOutputBufAvail);

                status = NvMMQueueDeQ(This->mFilledOutputBufferQueue, &DiscardedBuffer);
                if (status != NvSuccess)
                {
                    ALOGVV("%s: Dequeuing of Buffer failed", __FUNCTION__);
                    return;
                }

                status = NvMMQueueEnQ(This->mEmptyOutputBufferQueue, &DiscardedBuffer, 0);
                if (status != NvSuccess)
                {
                    ALOGVV("%s: Enqueuing of Buffer failed", __FUNCTION__);
                    return;
                }

                NvOsSemaphoreSignal(This->mhSemEmptyOutputBufAvail);
            }

            // enqueue the JPEG Encoder output buffer pointer to the queue
            status = NvMMQueueEnQ(This->mFilledOutputBufferQueue, &Buffer, 0);
            if (status != NvSuccess)
            {
                ALOGVV("%s: Enqueuing of Buffer failed", __FUNCTION__);
                return;
            }

            // signal the sendBuffer thread to procss the output buffer
            NvOsSemaphoreSignal(This->mhSemFilledOutputBufAvail);

            NvOsMutexLock(This->mStillCaptureStopMutex);
            This->mStillCaptureCount--;

            if (This->mStillCaptureCount == 0)
            {
                if (This->mPostProcessStill->Enabled())
                {
                    This->mPostProcessStill->DoPostCaptureOperations();
                }
            }

            if ((This->mStillCaptureStop) && (This->mStillCaptureCount == 0))
            {
                This->mStillCaptureFinished.signal();
                This->mStillCaptureStop = NV_FALSE;
            }
            NvOsMutexUnlock(This->mStillCaptureStopMutex);

            break;
        }
        default:
            break;
    }

    ALOGVV("%s--", __FUNCTION__);
}

void
NvCameraHal::UpdateSmoothZoom(NvCameraHal *This, NvS32 zoomValue, NvBool ZoomAchieved)
{
    // Update the settings stored in the parser for future
    // getParameter() calls.  We can't just do this during
    // getParameter() because that would violate const.
    if (This->mSmoothZoomInProgress)
    {
        NvCombinedCameraSettings updatedSettings;
        updatedSettings = This->mSettingsParser.getCurrentSettings();
        updatedSettings.zoomLevel = zoomValue;
        updatedSettings.isDirtyForParser.zoomLevel = NV_TRUE;
        This->mSettingsParser.updateSettings(updatedSettings);
    }

    // If ZoomAchieved == 0 (Still going), then zoom value shouldn't be
    // updated until ZoomAchieved == 1 (Done)
    // Smoothzoom is exception. See the comment below
    if (!ZoomAchieved)
    {
        This->mSetZoomPending = NV_TRUE;
    }
    else if (!This->mSmoothZoomInProgress)
    {
        This->mSetZoomPending = NV_FALSE;
        This->mSetZoomDoneCond.broadcast();
    }

    // If we're stopping, and we reached a threshold value,
    // then mark this as "done" and stop it.
    if (This->mSmoothZoomStopping &&
        (zoomValue != This->mSmoothZoomCurrentLevel))
    {
        ZoomAchieved = NV_TRUE;
        This->mSmoothZoomTargetLevel = zoomValue;
    }

    // Send a message if they asked for one (they always do)
    // and we haven't sent one for that zoom level already,
    // and if it's the last level, wait for done=true.
    // I.e: we get multiple events for each level, but API specifies
    // only one callback per level. That is, zoom 0 to 5, we see:
    // Events:   0 0 0 0 1 1 1 1 2 2 2 3 3 3 4 4 5 5
    // doneflag: F F F F F F F F F F F F F F F F F T
    // Callback:         *       *     *     *     *  (1,2,3,4,5 only).
    if (This->mMsgEnabled & CAMERA_MSG_ZOOM)
    {
        if (zoomValue != This->mSmoothZoomCurrentLevel
            && (zoomValue != This->mSmoothZoomTargetLevel || ZoomAchieved) )
        {
             NvS32 Step = (zoomValue > This->mSmoothZoomCurrentLevel) ? 1 : -1;

            if(This->mSmoothZoomInProgress)
            {
                This->mSetZoomPending = NV_TRUE;

                do
                {
                    This->mSmoothZoomCurrentLevel += Step;
                    This->NotifyCb(CAMERA_MSG_ZOOM,
                        This->mSmoothZoomCurrentLevel,
                        ZoomAchieved && zoomValue ==
                            This->mSmoothZoomCurrentLevel,
                        This->mCbCookie);
                } while (zoomValue != This->mSmoothZoomCurrentLevel);
            }

        }
    }

    // If mSmoothZoomInProgress is true, then getparameter should be able
    // to access zoomvalue. So I need to release mSetZoomPending here.
    // This one should be set to false after NotifyCb() is called to ensure
    // that returned value with Notify() is same as that from getparameter.

    if (This->mSmoothZoomInProgress)
    {
        This->mSetZoomPending = NV_FALSE;
        This->mSetZoomDoneCond.broadcast();
    }

    // Note if we're done and can accept immediate zooms again
    if (ZoomAchieved && This->mSmoothZoomTargetLevel == zoomValue)
    {
        This->mSmoothZoomInProgress = NV_FALSE;
    }
}
NvError NvCameraHal::ProcessOutBuffer(NvMMBuffer *Buffer)
{
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    if (!mIsPassThroughSupported && !IsStillJpegData(Buffer))
    {
        NV_CHECK_ERROR_CLEANUP(
            FeedJpegEncoder(Buffer)
        );
    }
    else
    {
        NvU32 bufferNum;
        NV_CHECK_ERROR_CLEANUP(
            m_pMemProfileConfigurator->GetBufferAmount(
                NVCAMERA_BUFFERCONFIG_STAGE_LAUNCH, NVCAMERA_BUFFERCONFIG_JPEG_OUTPUT,
                NULL, &bufferNum)
        );

        // alloc shmem
        this->mClientJpegBuffer =
            this->mGetMemoryCb(
                -1,
                Buffer->Payload.Ref.sizeOfValidDataInBytes,
                1,
                this->mCbCookie);
        if (!this->mClientJpegBuffer)
        {
            ALOGE(
                "%s: failed to allocate a shmem buffer for the JPEG!",
                __FUNCTION__);
            return NvError_InsufficientMemory;
        }

        NvRmMemRead(
            (NvRmMemHandle)(Buffer->Payload.Ref.hMem),
            Buffer->Payload.Ref.startOfValidData,
            this->mClientJpegBuffer->data,
            Buffer->Payload.Ref.sizeOfValidDataInBytes);

        // send shmem to app
        ALOGVV("%s: got output stream buffer, sending to app", __FUNCTION__);

        if (mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE)
        {
            this->DataCb(
                CAMERA_MSG_COMPRESSED_IMAGE,
                this->mClientJpegBuffer,
                this->mCbCookie);
        }
        else
        {
            this->DataCb(
                CAMERA_MSG_RAW_IMAGE,
                this->mClientJpegBuffer,
                this->mCbCookie);
        }

        NV_CHECK_ERROR_CLEANUP(
            NvMMQueueEnQ(this->mEmptyOutputBufferQueue, &Buffer, 0)
        );

        NvOsSemaphoreSignal(this->mhSemEmptyOutputBufAvail);

        if (NvMMQueueGetNumEntries(this->mEmptyOutputBufferQueue) == bufferNum)
        {
            this->mAreAllBuffersBack = NV_TRUE;
            this->mAllBuffersReturnedToQueue.signal();
        }
    }

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE(
        "%s-- something failed, releasing resources and aborting!",
        __FUNCTION__);
    // most of the potential errors here are from allocs
    return NvError_InsufficientMemory;
}

NvBool NvCameraHal::IsStillJpegData(NvMMBuffer *Buffer)
{
    NvU8 SOIBuffer[2];

    // Remove this to enable jpeg data, customer can add there own code here
    return NV_FALSE;

    if (!mIsPassThroughSupported)
        return NV_FALSE;

    if (Buffer->Payload.Surfaces.Surfaces[0].hMem)
    {
        NvRmMemRead(Buffer->Payload.Surfaces.Surfaces[0].hMem,
                Buffer->Payload.Surfaces.Surfaces[0].Offset,
                SOIBuffer, 2);

        // JPEG buffer header starts from 0xff 0xd8
        if (SOIBuffer[0] == 0xff && SOIBuffer[1] == 0xd8)
            return NV_TRUE;
        else
            return NV_FALSE;
    }

    if (Buffer->Payload.Ref.pMem)
    {
        NvU8 EOIBuffer[2];
        NvOsMemcpy(SOIBuffer,
            (NvU8 *)((NvU32)Buffer->Payload.Ref.pMem +
                Buffer->Payload.Ref.startOfValidData), 2);

        NvOsMemcpy(EOIBuffer,
            (NvU8 *)((NvU32)Buffer->Payload.Ref.pMem +
                Buffer->Payload.Ref.startOfValidData +
                Buffer->Payload.Ref.sizeOfValidDataInBytes - 2), 2);

        // JPEG buffer header starts from 0xff 0xd8
        if ((SOIBuffer[0] == 0xff && SOIBuffer[1] == 0xd8)
                && (EOIBuffer[0] == 0xff && EOIBuffer[1] == 0xd9))
            return NV_TRUE;
        else
            return NV_FALSE;
    }

    return NV_FALSE;
}

} // namespace android
