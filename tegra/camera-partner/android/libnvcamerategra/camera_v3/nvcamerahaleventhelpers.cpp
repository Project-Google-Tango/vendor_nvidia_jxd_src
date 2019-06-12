/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvcamerahal3core.h"
#include "nvgr.h"
#include "nvformatconverter.h"
#include "nvmetadatatranslator.h"
#include "nvcamerahal3_tags.h"
#include "nv_log.h"

namespace android {

static void NvDumpSurface(
    NvRmSurface **ppSurface,
    NvU32 NumSurface,
    NvU8 *pFileName);

// debug code to dump a surface, can extract from NvMM buffer or ANB like this:
/*
    // nvmm
    NvRmSurface *pSurface[3] = {NULL, NULL, NULL};
    NVMMDIGITALZOOM_GET_SURFACE_PTR(pSurface, pNvMMBuffer->Payload.Surfaces.Surfaces);
    NvDumpSurface(pSurface, 3, NULL);

    // anb
    buffer_handle_t handle = (buffer_handle_t)*pANB;
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

        NV_LOGV(HAL3_EVENT_HELPERS_TAG, "Surface[%d] = %d\n", i, *(pData + SurfaceSize));

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


NvError
NvCameraHal3Core::NvMMEventHandler(
    void *Context,
    NvU32 EventType,
    NvU32 EventSize,
    void *EventInfo)
{
    NvError e = NvSuccess;
    NvCameraHal3Core *This = (NvCameraHal3Core *)Context;

    if (!This)
    {
        NV_LOGE(
            "%s:%d NULL context passed to event handler, ignoring event",
            __FUNCTION__,
            __LINE__);
        // don't return error, it might break the NvMM machinery, and if the
        // context is NULL it's *probably* the HAL's fault for not setting it
        // properly during init
        return e;
    }

    NV_LOGD(HAL3_EVENT_HELPERS_TAG, "%s: ++ (event = 0x%x)", __FUNCTION__, EventType);
    switch (EventType)
    {
        // handle camera v3 events
        case NvCameraCoreEvent_Shutter:
        {
            NvCameraCoreShutterEventInfo *Info = (NvCameraCoreShutterEventInfo *)EventInfo;
            This->doNotifyShutterMsg(Info->FrameNumber, Info->Timestamp);
        }
            break;

        case NvCameraCoreEvent_CompletedBuffer:
        {
            NvCameraCoreFrameCaptureResult *Info = (NvCameraCoreFrameCaptureResult *)EventInfo;
            This->EmptyBuffer(Info->FrameNumber, Info->FrameDynamicProps,
                                Info->NumCompletedOutputBuffers, Info->ppOutputBuffers);
        }
            break;
        case NvCameraCoreEvent_PartialResultReady:
        {
            NvCameraCoreFrameCaptureResult *Info = (NvCameraCoreFrameCaptureResult *)EventInfo;
            ALOGV("%s: Partial Result Ready event received for FN = %d\n",
                    __FUNCTION__, Info->FrameNumber);
            This->SendPartialResult(Info->FrameDynamicProps, Info->FrameNumber);
        }

        // Keeping focuser movement events around - may be valuable for future HAL signaling.
        case NvMMEventCamera_FocusStartMoving:
            NV_LOGV(HAL3_EVENT_HELPERS_TAG, "%s: Event handler called FOCUS_START_MOVING\n", __FUNCTION__);
            break;
        case NvMMEventCamera_FocusStopped:
            NV_LOGV(HAL3_EVENT_HELPERS_TAG, "%s: Event handler called FOCUS_STOP_MOVING\n", __FUNCTION__);
            break;

        default:
            NV_LOGE("%s: unhandled event! [0x%x]", __FUNCTION__, EventType);
            break;
    }

    NV_LOGD(HAL3_EVENT_HELPERS_TAG, "%s: --", __FUNCTION__);
    return e;
}

void NvCameraHal3Core::SendPartialResult(const NvCamProperty_Public_Dynamic&
                                          frameDynamicProps, uint32_t frameNumber)
{
    // construct metadata here
    CameraMetadata metadata;
    NvCameraHal3_Public_Dynamic frameDynProp;
    frameDynProp.CoreDynProps = frameDynamicProps;
    if (frameDynamicProps.Ae.PartialResultUpdated ||
        frameDynamicProps.Af.PartialResultUpdated ||
        frameDynamicProps.Awb.PartialResultUpdated)
    {
        // update AePrecaptureId and AfTriggerId
        const NvCamProperty_Public_Static &statProp =
            mStaticCameraInfoMap.valueFor((int)mSensorId);
        update3AIds(frameDynProp);
        NvMetadataTranslator::translatePartialFromNvCamProperty(frameDynProp,
                metadata, mSensorMode, statProp);
        sendPartialResultToFrameworks(metadata, frameNumber);
    }
}

void NvCameraHal3Core::sendPartialResultToFrameworks(CameraMetadata& settings,
                                                        uint32_t frameNumber)
{
    //send the result metadata to framework
    camera3_capture_result result;

    result.frame_number = frameNumber;
    result.result = settings.getAndLock();
    result.num_output_buffers = 0;
    result.output_buffers = NULL;

    // Send it off to the framework
    ALOGV("%s: Sending Partial Result for frameNumber = %d", __FUNCTION__, frameNumber);
    doSendResult(&result);
    settings.unlock(result.result);
}


void NvCameraHal3Core::EmptyBuffer(NvU32 frameNumber,
                                    const NvCamProperty_Public_Dynamic& frameDynamicProps,
                                    NvU32 numCompletedOutputBuffers,
                                    NvMMBuffer **ppOutputBuffers)
{
    buffer_handle_t *anb = NULL;
    camera3_error_msg_code_t errCode;
    NvError err;
    camera3_stream_buffer *pTNRBuffer = NULL;

    if (!ppOutputBuffers)
    {
        // a potentially fatal error. Will force the client to reopen
        // the device
        errCode = CAMERA3_MSG_ERROR_DEVICE;
        doNotifyError(frameNumber, errCode, NULL);
        return;
    }

    // Only care about one buffer for now.
    anb = NVMMBufferToNative(ppOutputBuffers[0]);
    mCameraLogging.NvCameraPreviewProfile();
    mFrameJitterStatistics.updateStatistics(frameDynamicProps.SensorTimestamp);

    err = routeResultFromCAM(frameNumber, anb, frameDynamicProps);
    if (err != NvSuccess)
    {
        errCode = nvErrorToHal3Error(err);
        doNotifyError(frameNumber, errCode, NULL);
    }

    // Post-processing
    pTNRBuffer = dequeueTNRBuffer(frameNumber);
    if (pTNRBuffer)
    {
        NvMMBuffer *nvmmTNRBuffer = NativeToNVMMBuffer(pTNRBuffer->buffer);

        if (getBufferWidth(ppOutputBuffers[0]) == getBufferWidth(nvmmTNRBuffer) &&
            getBufferHeight(ppOutputBuffers[0]) == getBufferHeight(nvmmTNRBuffer))
        {
            mTNR_Processor.DoTNR(ppOutputBuffers[0], nvmmTNRBuffer);
        }
        else
        {
            NvMMBuffer *intermediateTNRBuffer = getIntermediateTNRBuffer(pTNRBuffer);
            mScaler.Scale(&ppOutputBuffers[0]->Payload.Surfaces, &intermediateTNRBuffer->Payload.Surfaces);
            mTNR_Processor.DoTNR(intermediateTNRBuffer, nvmmTNRBuffer);
        }

        err = routeResultFromCAM(frameNumber, pTNRBuffer->buffer, frameDynamicProps);
        if (err != NvSuccess)
        {
            errCode = nvErrorToHal3Error(err);
            doNotifyError(frameNumber, errCode, NULL);
        }
        delete pTNRBuffer;
    }

}

} // namespace android
