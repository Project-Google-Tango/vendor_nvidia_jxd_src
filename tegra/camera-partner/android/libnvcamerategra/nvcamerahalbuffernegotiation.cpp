/*
 * Copyright (c) 2012 - 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define LOG_TAG "NvCameraHalBufferNegotiation"

#define LOG_NDEBUG 0

#include "nvcamerahal.h"

#include <nvgr.h>
// if NV_GRALLOC_USAGE_FLAGS_CAMERA is not already defined, we use the default
// camera usage flags. Do nothing for now.
#define NV_GRALLOC_USAGE_FLAGS_CAMERA_DEFAULT (0)

#ifndef NV_GRALLOC_USAGE_FLAGS_CAMERA
#define NV_GRALLOC_USAGE_FLAGS_CAMERA NV_GRALLOC_USAGE_FLAGS_CAMERA_DEFAULT
#endif

#define DISPLAY_BUFFER_COUNTS() \
    do { \
        ALOGVV("Number of buffers for COMPONENT_CAMERA, CAMERA_OUT_PREVIEW"); \
        DebugBufferCounts(COMPONENT_CAMERA, CAMERA_OUT_PREVIEW); \
        ALOGVV("Number of buffers for COMPONENT_CAMERA, CAMERA_OUT_CAPTURE"); \
        DebugBufferCounts(COMPONENT_CAMERA, CAMERA_OUT_CAPTURE); \
        ALOGVV("Number of buffers for COMPONENT_DZ,DZ_OUT_PREVIEW"); \
        DebugBufferCounts(COMPONENT_DZ,DZ_OUT_PREVIEW); \
        ALOGVV("Number of buffers for COMPONENT_DZ,DZ_OUT_STILL"); \
        DebugBufferCounts(COMPONENT_DZ,DZ_OUT_STILL); \
        ALOGVV("Number of buffers for COMPONENT_DZ,DZ_OUT_VIDEO"); \
        DebugBufferCounts(COMPONENT_DZ,DZ_OUT_VIDEO); \
    } while (0)

namespace android {

// Initialize the resolutions to app's default
NvCameraUserResolution NvCameraHal::persistentPreview[MAX_NUM_SENSORS] = {{960, 720},
                                                                          {960, 720},
                                                                          {960, 720},
                                                                          {960, 720},
                                                                          {960, 720}};
NvCameraUserResolution NvCameraHal::persistentStill[MAX_NUM_SENSORS] = {{2592, 1944},
                                                                        {1280, 960},
                                                                        {2592, 1944},
                                                                        {2592, 1944},
                                                                        {2592, 1944}};
NvCameraUserResolution NvCameraHal::persistentVideo[MAX_NUM_SENSORS] = {{1280, 720},
                                                                        {1280, 720},
                                                                        {1280, 720},
                                                                        {1280, 720},
                                                                        {1280, 720}};

NvCameraMemProfileConfigurator *NvCameraHal::m_pMemProfileConfigurators[] =
    {NULL, NULL, NULL, NULL, NULL};
NvBufferManager *NvCameraHal::m_pBufferManagers[] = {NULL, NULL, NULL, NULL, NULL};
NvBufferStream *NvCameraHal::m_pBufferStreams[] = {NULL, NULL, NULL, NULL, NULL};
NvBool           NvCameraHal::mBuffersInitialized = NV_FALSE;

void NvCameraHal::InitBufferManager()
{
    NvCameraDriverInfo info;
    info.DZ = DZ.Block;
    info.Cam = Cam.Block;
    info.isCamUSB = (getCamType(mSensorId) == NVCAMERAHAL_CAMERATYPE_USB);
    info.pLock = &mLock; // Buffer manager needs to wait on some condtions

    // TODO, change the way the conditions work having a flag and a condition
    // can lead to a race where a thread is checking the flag while the other
    // is setting. This should be changed to some sort of s/r event or a class
    // that can lock itself.


    //preview events
    info.DZOutputPorts[DZ_OUT_PREVIEW].pBuffCfg =
                        &DZ.Ports[NvMMDigitalZoomStreamIndex_OutputPreview].BuffCfg;
    info.DZOutputPorts[DZ_OUT_PREVIEW].pBuffReq =
                        &DZ.Ports[NvMMDigitalZoomStreamIndex_OutputPreview].BuffReq;
    info.DZOutputPorts[DZ_OUT_PREVIEW].pCondBlockAbortDone =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_OutputPreview].BlockAbortDone;
    info.DZOutputPorts[DZ_OUT_PREVIEW].pCondBufferConfigDoneCond =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_OutputPreview].BufferConfigDoneCond;
    info.DZOutputPorts[DZ_OUT_PREVIEW].pCondBufferNegotiationDoneCond =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_OutputPreview].BufferNegotiationDoneCond;

    info.DZOutputPorts[DZ_OUT_PREVIEW].pBufferConfigDone =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_OutputPreview].BufferConfigDone;
    info.DZOutputPorts[DZ_OUT_PREVIEW].pBufferNegotiationDone =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_OutputPreview].BufferNegotiationDone;

    //still events
    info.DZOutputPorts[DZ_OUT_STILL].pBuffCfg =
                        &DZ.Ports[NvMMDigitalZoomStreamIndex_OutputStill].BuffCfg;
    info.DZOutputPorts[DZ_OUT_STILL].pBuffReq =
                        &DZ.Ports[NvMMDigitalZoomStreamIndex_OutputStill].BuffReq;
    info.DZOutputPorts[DZ_OUT_STILL].pCondBlockAbortDone =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_OutputStill].BlockAbortDone;
    info.DZOutputPorts[DZ_OUT_STILL].pCondBufferConfigDoneCond =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_OutputStill].BufferConfigDoneCond;
    info.DZOutputPorts[DZ_OUT_STILL].pCondBufferNegotiationDoneCond =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_OutputStill].BufferNegotiationDoneCond;

    info.DZOutputPorts[DZ_OUT_STILL].pBufferConfigDone =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_OutputStill].BufferConfigDone;
    info.DZOutputPorts[DZ_OUT_STILL].pBufferNegotiationDone =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_OutputStill].BufferNegotiationDone;

    //video events
    info.DZOutputPorts[DZ_OUT_VIDEO].pBuffCfg =
                        &DZ.Ports[NvMMDigitalZoomStreamIndex_OutputVideo].BuffCfg;
    info.DZOutputPorts[DZ_OUT_VIDEO].pBuffReq=
                        &DZ.Ports[NvMMDigitalZoomStreamIndex_OutputVideo].BuffReq;
    info.DZOutputPorts[DZ_OUT_VIDEO].pCondBlockAbortDone =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_OutputVideo].BlockAbortDone;
    info.DZOutputPorts[DZ_OUT_VIDEO].pCondBufferConfigDoneCond =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_OutputVideo].BufferConfigDoneCond;
    info.DZOutputPorts[DZ_OUT_VIDEO].pCondBufferNegotiationDoneCond =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_OutputVideo].BufferNegotiationDoneCond;

    info.DZOutputPorts[DZ_OUT_VIDEO].pBufferConfigDone =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_OutputVideo].BufferConfigDone;
    info.DZOutputPorts[DZ_OUT_VIDEO].pBufferNegotiationDone =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_OutputVideo].BufferNegotiationDone;

   //Dz input ports
    info.DZInputPorts[DZ_IN_PREVIEW].pBuffCfg =
                        &DZ.Ports[NvMMDigitalZoomStreamIndex_InputPreview].BuffCfg;
    info.DZInputPorts[DZ_IN_PREVIEW].pBuffReq=
                        &DZ.Ports[NvMMDigitalZoomStreamIndex_InputPreview].BuffReq;
    info.DZInputPorts[DZ_IN_PREVIEW].pCondBlockAbortDone =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_InputPreview].BlockAbortDone;
    info.DZInputPorts[DZ_IN_PREVIEW].pCondBufferConfigDoneCond =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_InputPreview].BufferConfigDoneCond;
    info.DZInputPorts[DZ_IN_PREVIEW].pCondBufferNegotiationDoneCond =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_InputPreview].BufferNegotiationDoneCond;

    info.DZInputPorts[DZ_IN_PREVIEW].pBufferConfigDone =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_InputPreview].BufferConfigDone;
    info.DZInputPorts[DZ_IN_PREVIEW].pBufferNegotiationDone =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_InputPreview].BufferNegotiationDone;



    info.DZInputPorts[DZ_IN_STILL].pBuffCfg =
                        &DZ.Ports[NvMMDigitalZoomStreamIndex_Input].BuffCfg;
    info.DZInputPorts[DZ_IN_STILL].pBuffReq=
                        &DZ.Ports[NvMMDigitalZoomStreamIndex_Input].BuffReq;
    info.DZInputPorts[DZ_IN_STILL].pCondBlockAbortDone =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_Input].BlockAbortDone;
    info.DZInputPorts[DZ_IN_STILL].pCondBufferConfigDoneCond =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_Input].BufferConfigDoneCond;
    info.DZInputPorts[DZ_IN_STILL].pCondBufferNegotiationDoneCond =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_Input].BufferNegotiationDoneCond;
    info.DZInputPorts[DZ_IN_STILL].pBufferConfigDone =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_Input].BufferConfigDone;
    info.DZInputPorts[DZ_IN_STILL].pBufferNegotiationDone =
                    &DZ.Ports[NvMMDigitalZoomStreamIndex_Input].BufferNegotiationDone;

    //Cam Preview out
    info.CAMOutputPorts[CAMERA_OUT_PREVIEW].pBuffCfg =
                    &Cam.Ports[mCameraStreamIndex_OutputPreview].BuffCfg;
    info.CAMOutputPorts[CAMERA_OUT_PREVIEW].pBuffReq =
                    &Cam.Ports[mCameraStreamIndex_OutputPreview].BuffReq;
        info.CAMOutputPorts[CAMERA_OUT_PREVIEW].pCondBlockAbortDone =
                    &Cam.Ports[mCameraStreamIndex_OutputPreview].BlockAbortDone;
    info.CAMOutputPorts[CAMERA_OUT_PREVIEW].pCondBufferConfigDoneCond =
                    &Cam.Ports[mCameraStreamIndex_OutputPreview].BufferConfigDoneCond;
    info.CAMOutputPorts[CAMERA_OUT_PREVIEW].pCondBufferNegotiationDoneCond =
                    &Cam.Ports[mCameraStreamIndex_OutputPreview].BufferNegotiationDoneCond;
    info.CAMOutputPorts[CAMERA_OUT_PREVIEW].pBufferConfigDone =
                    &Cam.Ports[mCameraStreamIndex_OutputPreview].BufferConfigDone;
    info.CAMOutputPorts[CAMERA_OUT_PREVIEW].pBufferNegotiationDone =
                    &Cam.Ports[mCameraStreamIndex_OutputPreview].BufferNegotiationDone;

    //Cam Capture out
    info.CAMOutputPorts[CAMERA_OUT_CAPTURE].pBuffCfg =
                    &Cam.Ports[mCameraStreamIndex_Output].BuffCfg;
    info.CAMOutputPorts[CAMERA_OUT_CAPTURE].pBuffReq =
                    &Cam.Ports[mCameraStreamIndex_Output].BuffReq;
    info.CAMOutputPorts[CAMERA_OUT_CAPTURE].pCondBlockAbortDone =
                    &Cam.Ports[mCameraStreamIndex_Output].BlockAbortDone;
    info.CAMOutputPorts[CAMERA_OUT_CAPTURE].pCondBufferConfigDoneCond =
                    &Cam.Ports[mCameraStreamIndex_Output].BufferConfigDoneCond;
    info.CAMOutputPorts[CAMERA_OUT_CAPTURE].pCondBufferNegotiationDoneCond =
                    &Cam.Ports[mCameraStreamIndex_Output].BufferNegotiationDoneCond;
    info.CAMOutputPorts[CAMERA_OUT_CAPTURE].pBufferConfigDone =
                    &Cam.Ports[mCameraStreamIndex_Output].BufferConfigDone;
    info.CAMOutputPorts[CAMERA_OUT_CAPTURE].pBufferNegotiationDone =
                    &Cam.Ports[mCameraStreamIndex_Output].BufferNegotiationDone;

    if (m_pMemProfileConfigurators[mSensorId] == NULL)
    {
        m_pMemProfileConfigurators[mSensorId] = new NvCameraMemProfileConfigurator();
    }

    if (m_pBufferManagers[mSensorId] == NULL && m_pBufferStreams[mSensorId]  == NULL)
    {
        m_pBufferManagers[mSensorId] = NvBufferManager::Instance(mSensorId, info);
        m_pBufferStreams[mSensorId] = new NvBufferStream();
    }
    else
    {
        m_pBufferManagers[mSensorId]->ReInitializeDriverInfo(m_pBufferStreams[mSensorId],info);
    }

    mBuffersInitialized = NV_TRUE;
    return;
}

NvError NvCameraHal::BufferManagerConfigureStream(
    NvStreamRequest &streamReq,
    NvDzOutputPort Port,
    NvCameraUserResolution &persistentResolution,
    NvCameraUserResolution &maxResolution,
    NvCameraUserResolution &settingsResolution,
    NvBool *DirtyBit,
    const char *StreamName,
    NvU32 maxOutputBuffers)
{
    NvError e = NvSuccess;
    NvBufferRequest CustomRequest;
    NvBufferOutputLocation location;

    location.SetLocation(COMPONENT_DZ, Port);

    if (persistentResolution.width > maxResolution.width ||
        persistentResolution.height > maxResolution.height)
    {
        ALOGWW("%s: Persistent %s resolution (%dx%d) is larger than max"
            "supported resolution for this sensor (%dx%d)\n",
            __FUNCTION__,
            StreamName,
            persistentResolution.width,
            persistentResolution.height,
            maxResolution.width, maxResolution.height);
        persistentResolution = maxResolution;
    }

    CustomRequest.Width = persistentResolution.width;
    CustomRequest.Height = persistentResolution.height;

    if (settingsResolution.width != persistentResolution.width ||
        settingsResolution.height != persistentResolution.height)
    {
        *DirtyBit = NV_TRUE;
    }

    settingsResolution = persistentResolution;

    ALOGVV("%s: Buffer Manager %s is initializing to W = %d, H = %d",
        __FUNCTION__, StreamName,
        CustomRequest.Width , CustomRequest.Height);

    CustomRequest.Location = location;
    CustomRequest.MinBuffers = maxOutputBuffers;
    CustomRequest.MaxBuffers = maxOutputBuffers;
    NV_CHECK_ERROR(
            streamReq.AddCustomBufferRequest(CustomRequest)
    );
    return e;
}

NvError NvCameraHal::BufferManagerConfigureCaptureBuffers(
    NvStreamRequest &streamReq,
    NvU32 maxCapturePreviewBuffers,
    NvU32 maxCaptureStillBuffers)
{
    NvError e = NvSuccess;
    NvBufferRequest CustomRequest;
    NvBufferOutputLocation location;

    ALOGDD("%s maxCapturePreviewBuffers %d, maxCaptureStillBuffers %d",
        __FUNCTION__, maxCapturePreviewBuffers, maxCaptureStillBuffers);

    // We allow to have 0 buffers configured
    location.SetLocation(COMPONENT_CAMERA, CAMERA_OUT_CAPTURE);
    CustomRequest.Location = location;
    CustomRequest.MinBuffers = maxCaptureStillBuffers;
    CustomRequest.MaxBuffers = maxCaptureStillBuffers;
    NV_CHECK_ERROR(
        streamReq.AddCustomBufferRequest(CustomRequest)
    );

    location.SetLocation(COMPONENT_CAMERA, CAMERA_OUT_PREVIEW);
    CustomRequest.Location = location;
    CustomRequest.MinBuffers = maxCapturePreviewBuffers;
    CustomRequest.MaxBuffers = maxCapturePreviewBuffers;
    NV_CHECK_ERROR(
            streamReq.AddCustomBufferRequest(CustomRequest)
    );
    return e;
}

void NvCameraHal::DebugBufferCounts(NvBufferManagerComponent component, NvU32 port)
{
    NvBufferOutputLocation location;
    NvU32 totalAllocated = 0;
    NvU32 totalRequested = 0;
    NvU32 totalInUse = 0;
    location.SetLocation(component, port);
    m_pBufferStream->GetNumberOfBuffers(location,
                                        &totalAllocated,
                                        &totalRequested,
                                        &totalInUse);
    ALOGVV("TotalAllocated = %d, TotalRequested = %d, TotalInUse = %d",
        totalAllocated,
        totalRequested,
        totalInUse);
}

// This is function is only needed in LEAN perf profile
//  which we only allocates buffer when we start capture.
// If we are not using this mode, just return with Success
NvError NvCameraHal::SetupLeanModeVideoBuffer(int VSTABBufferNum)
{
    ALOGDD("%s++", __FUNCTION__);
    NvError e = NvSuccess;
    NvCameraBufferConfigStage stage;
    NvU32 camCaptureBufferNum = 0, dzStillBufferNum = 0, dzVideoBufferNum = 0;
    NvU32 allocatedNumberOfBuffers = 0;
    NvBufferOutputLocation location;

    NvCameraBufferPerfScheme perfScheme =
        m_pMemProfileConfigurator->GetBufferConfigPerfScheme();
    if (perfScheme != NVCAMERA_BUFFER_PERF_LEAN)
        return e;

    // Set up the stage
    m_pMemProfileConfigurator->SetBufferConfigStage(
        NVCAMERA_BUFFERCONFIG_STAGE_LEAN_VIDEO);

    // Camera block capture buffer
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            NVCAMERA_BUFFERCONFIG_STAGE_LEAN_VIDEO,
            NVCAMERA_BUFFERCONFIG_CAMERA_CAPTURE,
            0, &camCaptureBufferNum)
    );

    if (VSTABBufferNum)
        camCaptureBufferNum += VSTABBufferNum;
    // Only allocate buffer if we need to
    BufferManagerGetNumberOfStillBuffers(
        &allocatedNumberOfBuffers);
    if (allocatedNumberOfBuffers < camCaptureBufferNum)
    {
        location.SetLocation(COMPONENT_CAMERA, CAMERA_OUT_CAPTURE);
        NV_CHECK_ERROR_CLEANUP(
            m_pBufferStream->SetNumberOfBuffers(
                location,
                camCaptureBufferNum,
                camCaptureBufferNum,
                &allocatedNumberOfBuffers)
        );
    }

    // DZ Camera buffer
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            NVCAMERA_BUFFERCONFIG_STAGE_LEAN_VIDEO,
            NVCAMERA_BUFFERCONFIG_DZ_STILL,
            0, &dzStillBufferNum)
    );
    location.SetLocation(COMPONENT_DZ, DZ_OUT_STILL);
    NV_CHECK_ERROR_CLEANUP(
        m_pBufferStream->SetNumberOfBuffers(
            location,
            dzStillBufferNum,
            dzStillBufferNum,
            &allocatedNumberOfBuffers)
    );

    // DZ Video buffer
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            NVCAMERA_BUFFERCONFIG_STAGE_LEAN_VIDEO,
            NVCAMERA_BUFFERCONFIG_DZ_VIDEO,
            0, &dzVideoBufferNum)
    );
    location.SetLocation(COMPONENT_DZ, DZ_OUT_VIDEO);
    NV_CHECK_ERROR_CLEANUP(
        m_pBufferStream->SetNumberOfBuffers(
            location,
            dzVideoBufferNum,
            dzVideoBufferNum,
            &allocatedNumberOfBuffers)
    );

    ALOGDD("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

// This is function is only needed in LEAN perf profile
//  which we only allocates buffer when we start capture.
// If we are not using this mode, just return with Success
NvError NvCameraHal::ReleaseLeanModeVideoBuffer()
{
    ALOGDD("%s++", __FUNCTION__);
    NvError e = NvSuccess;
    NvCameraBufferConfigStage stage;
    NvU32 camCaptureBufferNum = 0, dzStillBufferNum = 0, dzVideoBufferNum = 0;
    NvU32 allocatedNumberOfBuffers = 0;
    NvBufferOutputLocation location;

    NvCameraBufferPerfScheme perfScheme =
        m_pMemProfileConfigurator->GetBufferConfigPerfScheme();
    if (perfScheme != NVCAMERA_BUFFER_PERF_LEAN)
        return e;

    // Camera block capture buffer
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            NVCAMERA_BUFFERCONFIG_STAGE_AFTER_FIRST_PREVIEW,
            NVCAMERA_BUFFERCONFIG_CAMERA_CAPTURE,
            0, &camCaptureBufferNum)
    );
    location.SetLocation(COMPONENT_CAMERA, CAMERA_OUT_CAPTURE);
    NV_CHECK_ERROR_CLEANUP(
        m_pBufferStream->SetNumberOfBuffers(
            location,
            camCaptureBufferNum,
            camCaptureBufferNum,
            &allocatedNumberOfBuffers)
    );

    // DZ Camera buffer
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            NVCAMERA_BUFFERCONFIG_STAGE_AFTER_FIRST_PREVIEW,
            NVCAMERA_BUFFERCONFIG_DZ_STILL,
            0, &dzStillBufferNum)
    );
    location.SetLocation(COMPONENT_DZ, DZ_OUT_STILL);
    NV_CHECK_ERROR_CLEANUP(
        m_pBufferStream->SetNumberOfBuffers(
            location,
            dzStillBufferNum,
            dzStillBufferNum,
            &allocatedNumberOfBuffers)
    );

    // DZ Video buffer
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            NVCAMERA_BUFFERCONFIG_STAGE_AFTER_FIRST_PREVIEW,
            NVCAMERA_BUFFERCONFIG_DZ_VIDEO,
            0, &dzVideoBufferNum)
    );
    location.SetLocation(COMPONENT_DZ, DZ_OUT_VIDEO);
    NV_CHECK_ERROR_CLEANUP(
        m_pBufferStream->SetNumberOfBuffers(
            location,
            dzVideoBufferNum,
            dzVideoBufferNum,
            &allocatedNumberOfBuffers)
    );

    //Reset the stage after finish
    m_pMemProfileConfigurator->SetBufferConfigStage(
        NVCAMERA_BUFFERCONFIG_STAGE_AFTER_FIRST_PREVIEW);

    ALOGDD("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

// This is function is only needed in LEAN perf profile
//  which we only allocates buffer when we start capture.
// If we are not using this mode, just return with Success
NvError NvCameraHal::SetupLeanModeStillBuffer(int NSLBufferNum)
{
    ALOGDD("%s++", __FUNCTION__);
    NvError e = NvSuccess;
    NvCameraBufferConfigStage stage;
    NvU32 camCaptureBufferNum = 0, dzStillBufferNum = 0;
    NvU32 allocatedNumberOfBuffers = 0;
    NvBufferOutputLocation location;

    NvCameraBufferPerfScheme perfScheme =
        m_pMemProfileConfigurator->GetBufferConfigPerfScheme();
    if (perfScheme != NVCAMERA_BUFFER_PERF_LEAN)
        return e;

    // Set up the stage
    m_pMemProfileConfigurator->SetBufferConfigStage(
        NVCAMERA_BUFFERCONFIG_STAGE_LEAN_STILL);

    // Camera block capture buffer
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            NVCAMERA_BUFFERCONFIG_STAGE_LEAN_STILL,
            NVCAMERA_BUFFERCONFIG_CAMERA_CAPTURE,
            0, &camCaptureBufferNum)
    );

    // Add up NSL Buffer if we need to
    if (NSLBufferNum)
        camCaptureBufferNum += NSLBufferNum;

    // Only allocate buffer if we need to
    BufferManagerGetNumberOfStillBuffers(
        &allocatedNumberOfBuffers);
    if (allocatedNumberOfBuffers < camCaptureBufferNum)
    {
        location.SetLocation(COMPONENT_CAMERA, CAMERA_OUT_CAPTURE);
        NV_CHECK_ERROR_CLEANUP(
            m_pBufferStream->SetNumberOfBuffers(
                location,
                camCaptureBufferNum,
                camCaptureBufferNum,
                &allocatedNumberOfBuffers)
        );
    }

    // DZ Camera buffer
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            NVCAMERA_BUFFERCONFIG_STAGE_LEAN_STILL,
            NVCAMERA_BUFFERCONFIG_DZ_STILL,
            0, &dzStillBufferNum)
    );
    location.SetLocation(COMPONENT_DZ, DZ_OUT_STILL);
    NV_CHECK_ERROR_CLEANUP(
        m_pBufferStream->SetNumberOfBuffers(
            location,
            dzStillBufferNum,
            dzStillBufferNum,
            &allocatedNumberOfBuffers)
    );

    ALOGDD("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

// This is function is only needed in LEAN perf profile
//  which we only allocates buffer when we start capture.
// If we are not using this mode, just return with Success
NvError NvCameraHal::ReleaseLeanModeStillBuffer()
{
    ALOGDD("%s++", __FUNCTION__);
    NvError e = NvSuccess;
    NvCameraBufferConfigStage stage;
    NvU32 camCaptureBufferNum = 0, dzStillBufferNum = 0;
    NvU32 allocatedNumberOfBuffers = 0;
    NvBufferOutputLocation location;

    NvCameraBufferPerfScheme perfScheme =
        m_pMemProfileConfigurator->GetBufferConfigPerfScheme();
    if (perfScheme != NVCAMERA_BUFFER_PERF_LEAN)
        return e;

    // Camera block capture buffer
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            NVCAMERA_BUFFERCONFIG_STAGE_AFTER_FIRST_PREVIEW,
            NVCAMERA_BUFFERCONFIG_CAMERA_CAPTURE,
            0, &camCaptureBufferNum)
    );
    location.SetLocation(COMPONENT_CAMERA, CAMERA_OUT_CAPTURE);
    NV_CHECK_ERROR_CLEANUP(
        m_pBufferStream->SetNumberOfBuffers(
            location,
            camCaptureBufferNum,
            camCaptureBufferNum,
            &allocatedNumberOfBuffers)
    );

    // DZ Camera buffer
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            NVCAMERA_BUFFERCONFIG_STAGE_AFTER_FIRST_PREVIEW,
            NVCAMERA_BUFFERCONFIG_DZ_STILL,
            0, &dzStillBufferNum)
    );
    location.SetLocation(COMPONENT_DZ, DZ_OUT_STILL);
    NV_CHECK_ERROR_CLEANUP(
        m_pBufferStream->SetNumberOfBuffers(
            location,
            dzStillBufferNum,
            dzStillBufferNum,
            &allocatedNumberOfBuffers)
    );

    //Reset the stage after finish
    m_pMemProfileConfigurator->SetBufferConfigStage(
        NVCAMERA_BUFFERCONFIG_STAGE_AFTER_FIRST_PREVIEW);

    ALOGDD("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::BufferManagerNegotiateAndSupplyBuffers(NvCombinedCameraSettings &defaultSettings)
{
    NvError e = NvSuccess;
    NvBufferStreamType streamType = CAMERA_STANDARD_CAPTURE;
    NvBufferOutputLocation location;
    NvStreamRequest streamReq;
    NvCameraBufferConfigStage stage =
        m_pMemProfileConfigurator->GetBufferConfigStage();
    //DZ block output config
    NvU32 previewBufferNum;
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            stage, NVCAMERA_BUFFERCONFIG_DZ_PREVIEW,
            NULL, &previewBufferNum)
    );
    NvU32 stillBufferNum;
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            stage, NVCAMERA_BUFFERCONFIG_DZ_STILL,
            NULL, &stillBufferNum)
    );
    NvU32 videoBufferNum;
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            stage, NVCAMERA_BUFFERCONFIG_DZ_VIDEO,
            NULL, &videoBufferNum)
    );
    //Camera block output config
    NvU32 capturePreviewBufferNum;
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            stage, NVCAMERA_BUFFERCONFIG_CAMERA_PREVIEW,
            NULL, &capturePreviewBufferNum)
    );
    NvU32 captureStillBufferNum;
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            stage, NVCAMERA_BUFFERCONFIG_CAMERA_CAPTURE,
            NULL, &captureStillBufferNum)
    );

    ALOGDD("%s CAM_PREV: %d, CAM_STILL %d", __FUNCTION__,
        capturePreviewBufferNum, captureStillBufferNum);
    ALOGDD("%s DZ_PREV: %d, DZ_STILL: %d, DZ_VIDEO: %d",
        __FUNCTION__,
        previewBufferNum, stillBufferNum, videoBufferNum);

    // Configure Preview
    NV_CHECK_ERROR_CLEANUP(BufferManagerConfigureStream(
        streamReq,
        DZ_OUT_PREVIEW,
        persistentPreview[mSensorId],
        mMaxPreview,
        defaultSettings.previewResolution,
        &defaultSettings.isDirtyForParser.previewResolution,
        "Preview",
        previewBufferNum));

    // Configure Still
    NV_CHECK_ERROR_CLEANUP(BufferManagerConfigureStream(
        streamReq,
        DZ_OUT_STILL,
        persistentStill[mSensorId],
        mMaxStill,
        defaultSettings.imageResolution,
        &defaultSettings.isDirtyForParser.imageResolution,
        "Still",
        stillBufferNum));

    // Configure Video
    NV_CHECK_ERROR_CLEANUP(BufferManagerConfigureStream(
        streamReq,
        DZ_OUT_VIDEO,
        persistentVideo[mSensorId],
        mMaxVideo,
        defaultSettings.videoResolution,
        &defaultSettings.isDirtyForParser.videoResolution,
        "Video",
        videoBufferNum));

    // Configure Buffer counts for Capture Still/Preview
    NV_CHECK_ERROR_CLEANUP(BufferManagerConfigureCaptureBuffers(streamReq,
                                         capturePreviewBufferNum,
                                         captureStillBufferNum));

    // this will create the settings and initialize the drivers but not allocate the buffers
    NV_CHECK_ERROR_CLEANUP(
            m_pBufferManager->InitializeStream(m_pBufferStream, streamType, streamReq)
    );

    // Just some sanity check code todo remove
    location.SetLocation(COMPONENT_DZ,DZ_OUT_PREVIEW);
    NvMMNewBufferConfigurationInfo BufCfg;
    m_pBufferStream->GetOutputPortBufferCfg(location, &BufCfg);
    debugBufferCfg((char *)"--------Buffer Manager: PREVIEW Configuration--------------", &BufCfg);

    location.SetLocation(COMPONENT_DZ,DZ_OUT_STILL);
    m_pBufferStream->GetOutputPortBufferCfg(location, &BufCfg);
    debugBufferCfg((char *)"--------Buffer Manager: STILL Configuration--------------", &BufCfg);

    location.SetLocation(COMPONENT_DZ,DZ_OUT_VIDEO);
    m_pBufferStream->GetOutputPortBufferCfg(location, &BufCfg);
    debugBufferCfg((char *)"--------Buffer Manager: VIDEO Configuration--------------", &BufCfg);

    location.SetLocation(COMPONENT_CAMERA,CAMERA_OUT_PREVIEW);
    m_pBufferStream->GetOutputPortBufferCfg(location, &BufCfg);
    debugBufferCfg((char *)"--------Buffer Manager: CAPTURE PREVIEW Configuration----", &BufCfg);

    location.SetLocation(COMPONENT_CAMERA,CAMERA_OUT_CAPTURE);
    m_pBufferStream->GetOutputPortBufferCfg(location, &BufCfg);
    debugBufferCfg((char *)"--------Buffer Manager: CAPTURE STILL Configuration------", &BufCfg);
// End check code

    NV_CHECK_ERROR_CLEANUP(
        BufferManagerSupplyCamBuffers()
    );

    DISPLAY_BUFFER_COUNTS();
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::BufferManagerReconfigureStillBuffersResolution(NvU32 width, NvU32 height)
{
    NvBufferStreamType streamType = CAMERA_STANDARD_CAPTURE;
    NvBufferOutputLocation location;
    NvBufferRequest CustomRequest;
    NvStreamRequest streamReq;
    NvError e = NvSuccess;
    NvBool restartPreview = NV_FALSE;

    NvCameraBufferConfigStage stage =
        m_pMemProfileConfigurator->GetBufferConfigStage();
    NvU32 stillBufferNum;
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            stage, NVCAMERA_BUFFERCONFIG_DZ_STILL,
            NULL, &stillBufferNum)
    );

    ALOGVV("%s++", __FUNCTION__);

    if (this->persistentStill[this->mSensorId].width == (NvS32)width &&
        this->persistentStill[this->mSensorId].height == (NvS32)height)
    {
        ALOGVV("%s-- (early exit)", __FUNCTION__);
        return e;
    }

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

    m_pBufferStream->RecoverBuffersFromComponent(COMPONENT_CAMERA);
    m_pBufferStream->RecoverBuffersFromComponent(COMPONENT_DZ);

    ResetCamStreams();

    NV_CHECK_ERROR_CLEANUP(
        ConnectCaptureGraph()
    );

    // Configure Still
    location.SetLocation(COMPONENT_DZ,DZ_OUT_STILL);
    CustomRequest.Width = width;
    CustomRequest.Height = height;
    this->persistentStill[this->mSensorId].width = width;
    this->persistentStill[this->mSensorId].height = height;
    CustomRequest.Location = location;
    ALOGE("Buffer Manager: Still is re-initializing to W = %d, H = %d",
                    CustomRequest.Width ,
                    CustomRequest.Height);

    CustomRequest.MinBuffers = stillBufferNum;
    CustomRequest.MaxBuffers = stillBufferNum;
    NV_CHECK_ERROR_CLEANUP(
        streamReq.AddCustomBufferRequest(CustomRequest)
    );
    NV_CHECK_ERROR_CLEANUP(
        m_pBufferManager->InitializeStream(m_pBufferStream, streamType, streamReq)
    );

    //TODO remove
    NvMMNewBufferConfigurationInfo BufCfg;
    m_pBufferStream->GetOutputPortBufferCfg(location, &BufCfg);
    debugBufferCfg((char *)"-------------Buffer Manger: New STILL Configuration----------", &BufCfg);
    // end remove

    // supply camera buffer if necessary
    NV_CHECK_ERROR_CLEANUP(
        BufferManagerSupplyCamBuffers()
    );

    if (restartPreview == NV_TRUE)
    {
        NV_CHECK_ERROR_CLEANUP(
            StartPreviewInternal()
        );
    }

    ALOGVV("%s--", __FUNCTION__);
    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}


NvError NvCameraHal::BufferManagerReconfigureVideoBuffersResolution(NvU32 width, NvU32 height)
{
    NvBufferStreamType streamType = CAMERA_STANDARD_CAPTURE;
    NvBufferOutputLocation location;
    NvBufferRequest CustomRequest;
    NvStreamRequest streamReq;
    NvError e = NvSuccess;
    NvBool restartPreview = NV_FALSE;

    NvCameraBufferConfigStage stage =
        m_pMemProfileConfigurator->GetBufferConfigStage();
    NvU32 videoBufferNum;
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            stage, NVCAMERA_BUFFERCONFIG_DZ_VIDEO,
            NULL, &videoBufferNum)
    );

    ALOGVV("%s++", __FUNCTION__);

    if (this->persistentVideo[this->mSensorId].width == (NvS32)width &&
        this->persistentVideo[this->mSensorId].height == (NvS32)height)
    {
        ALOGVV("%s-- (early exit)", __FUNCTION__);
        return e;
    }

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

    ResetCamStreams();

    NV_CHECK_ERROR_CLEANUP(
        ConnectCaptureGraph()
    );

    location.SetLocation(COMPONENT_DZ,DZ_OUT_VIDEO);
    CustomRequest.Width = width;
    CustomRequest.Height = height;
    this->persistentVideo[this->mSensorId].width = width;
    this->persistentVideo[this->mSensorId].height = height;
    CustomRequest.Location = location;
    ALOGE("Buffer Manager: VIDEO is re-initializing to W = %d, H = %d",
                    CustomRequest.Width ,
                    CustomRequest.Height);

    CustomRequest.MinBuffers = videoBufferNum;
    CustomRequest.MaxBuffers = videoBufferNum;
    NV_CHECK_ERROR_CLEANUP(
                streamReq.AddCustomBufferRequest(CustomRequest)
    );
    NV_CHECK_ERROR_CLEANUP(
        m_pBufferManager->InitializeStream(m_pBufferStream, streamType, streamReq)
    );

    //TODO remove
    NvMMNewBufferConfigurationInfo BufCfg;
    m_pBufferStream->GetOutputPortBufferCfg(location, &BufCfg);
    debugBufferCfg((char *)"-------------Buffer Manger: VIDEO Configuration-------------", &BufCfg);
    // end remove

    // supply camera buffer if necessary
    NV_CHECK_ERROR_CLEANUP(
        BufferManagerSupplyCamBuffers()
    );

    if (restartPreview == NV_TRUE)
    {
        NV_CHECK_ERROR_CLEANUP(
            StartPreviewInternal()
        );
    }

    ALOGVV("%s--", __FUNCTION__);
    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::BufferManagerReconfigurePreviewBuffersResolution(NvU32 width, NvU32 height)
{
    NvBufferStreamType streamType = CAMERA_STANDARD_CAPTURE;
    NvBufferOutputLocation location;
    NvBufferRequest CustomRequest;
    NvStreamRequest streamReq;
    const NvCombinedCameraSettings &Settings =
                mSettingsParser.getCurrentSettings();
    NvError e = NvSuccess;
    NvBool restartPreview = NV_FALSE;

    NvCameraBufferConfigStage stage =
        m_pMemProfileConfigurator->GetBufferConfigStage();
    NvU32 previewBufferNum;
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            stage, NVCAMERA_BUFFERCONFIG_DZ_PREVIEW,
            NULL, &previewBufferNum)
    );

    ALOGVV("%s++", __FUNCTION__);

    // In slow motion, a new sensor mode will be selected so reconfiguration
    // of preview buffers is required. For this we have to continue with
    // this function even if preview size remains same.
    // This extra check of videoSpeed == 1.0f will make sure that early exit
    // does not happen if we are in slow motion.
    if ((this->persistentPreview[this->mSensorId].width == (NvS32)width &&
        this->persistentPreview[this->mSensorId].height == (NvS32)height) &&
        (Settings.videoSpeed == 1.0f))
    {
        ALOGVV("%s-- (early exit)", __FUNCTION__);
        return e;
    }

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

    m_pBufferStream->RecoverBuffersFromComponent(COMPONENT_CAMERA);
    m_pBufferStream->RecoverBuffersFromComponent(COMPONENT_DZ);

    ResetCamStreams();

    NV_CHECK_ERROR_CLEANUP(
        ConnectCaptureGraph()
    );

    // Configure Still
    location.SetLocation(COMPONENT_DZ,DZ_OUT_PREVIEW);
    if( width > (NvU32)mMaxPreview.width ||
        height > (NvU32)mMaxPreview.height )
    {
        CustomRequest.Width = mMaxPreview.width;
        CustomRequest.Height = mMaxPreview.height;
        this->persistentPreview[this->mSensorId].width = mMaxPreview.width;
        this->persistentPreview[this->mSensorId].height = mMaxPreview.height;
    }
    else
    {
        CustomRequest.Width = width;
        CustomRequest.Height = height;
        this->persistentPreview[this->mSensorId].width = width;
        this->persistentPreview[this->mSensorId].height = height;
    }
    ALOGE("Buffer Manager: Preview is re-initializing to W = %d, H = %d",
                    CustomRequest.Width ,
                    CustomRequest.Height);

    CustomRequest.Location = location;
    CustomRequest.MinBuffers = previewBufferNum;
    CustomRequest.MaxBuffers = previewBufferNum;

    NV_CHECK_ERROR_CLEANUP(
                streamReq.AddCustomBufferRequest(CustomRequest)
    );
    NV_CHECK_ERROR_CLEANUP(
        m_pBufferManager->InitializeStream(m_pBufferStream, streamType, streamReq)
    );

    //TODO remove
    NvMMNewBufferConfigurationInfo BufCfg;
    m_pBufferStream->GetOutputPortBufferCfg(location, &BufCfg);
    debugBufferCfg((char *)"-------------Buffer Manger: PREVIEW Configuration-----------", &BufCfg);
    // end remove

    // incase a preview change changes the preview cam buffer
    NV_CHECK_ERROR_CLEANUP(
        BufferManagerSupplyCamBuffers()
    );

    if (restartPreview == NV_TRUE)
    {
        NV_CHECK_ERROR_CLEANUP(
            StartPreviewInternal()
        );
    }

    ALOGVV("%s--", __FUNCTION__);
    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::BufferManagerSupplyCamBuffers()
{
    NvError e = NvSuccess;
    NvBufferOutputLocation location;
    location.SetLocation(COMPONENT_CAMERA,CAMERA_OUT_CAPTURE);

    NV_CHECK_ERROR_CLEANUP(
        m_pBufferStream->AllocateBuffers(location)
    );

    NV_CHECK_ERROR_CLEANUP(
        m_pBufferStream->SendBuffersToLocation(location)
    );

    location.SetLocation(COMPONENT_CAMERA,CAMERA_OUT_PREVIEW);

    NV_CHECK_ERROR_CLEANUP(
        m_pBufferStream->AllocateBuffers(location)
    );

    NV_CHECK_ERROR_CLEANUP(
        m_pBufferStream->SendBuffersToLocation(location)
    );


    location.SetLocation(COMPONENT_DZ,DZ_OUT_PREVIEW);

    NV_CHECK_ERROR_CLEANUP(
        m_pBufferStream->AllocateBuffers(location)
    );

    // do not supply on preview since they have to be linked wiht ANW first

    location.SetLocation(COMPONENT_DZ,DZ_OUT_STILL);

    NV_CHECK_ERROR_CLEANUP(
        m_pBufferStream->AllocateBuffers(location)
    );

    NV_CHECK_ERROR_CLEANUP(
        m_pBufferStream->SendBuffersToLocation(location)
    );

    location.SetLocation(COMPONENT_DZ,DZ_OUT_VIDEO);

    NV_CHECK_ERROR_CLEANUP(
        m_pBufferStream->AllocateBuffers(location)
    );
#if !NV_CAMERA_V3
    NV_CHECK_ERROR_CLEANUP(
        m_pBufferStream->SendBuffersToLocation(location)
    );
#endif
    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

// returns buffers to allocator for preview cases
NvError NvCameraHal::BufferManagerReclaimPreviewBuffers()
{
    NvError e = NvSuccess;
    NvBufferOutputLocation location;
    location.SetLocation(COMPONENT_DZ,DZ_OUT_PREVIEW);

    NV_CHECK_ERROR_CLEANUP(
        m_pBufferStream->RecoverBuffersFromLocation(location)
    );

    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

// returns buffers to allocator for preview cases
NvError NvCameraHal::BufferManagerReclaimALLBuffers()
{
    NvError e = NvSuccess;

    if (DZ.Block)
    {
        NV_CHECK_ERROR_CLEANUP(
            m_pBufferStream->RecoverBuffersFromComponent(COMPONENT_DZ)
        );
    }
    else
    {
        ALOGE("%s: DZ component already released", __FUNCTION__);
    }

    if (Cam.Block)
    {
        NV_CHECK_ERROR_CLEANUP(
            m_pBufferStream->RecoverBuffersFromComponent(COMPONENT_CAMERA)
        );
    }
    else
    {
        ALOGE("%s: Camera component already released", __FUNCTION__);
    }

    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::ResetCameraStream(NvMMCameraStreamIndex StreamIndex)
{
    NvMMCameraResetBufferNegotiation ResetStream;
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    NvOsMemset(&ResetStream,0,sizeof(ResetStream));
    ResetStream.StreamIndex = StreamIndex;

    UNLOCK_EVENTS();
    e = Cam.Block->Extension(
            Cam.Block,
            NvMMCameraExtension_ResetBufferNegotiation,
            sizeof(ResetStream),
            &ResetStream,
            0,
            NULL);
    RELOCK_EVENTS();

    ALOGVV("%s--", __FUNCTION__);
    return e;
}

NvError NvCameraHal::BufferManagerCheckNumberOfBuffersAtLocation(
    NvCameraBufferConfigStage stage,
    NvCameraBufferConfigPortID id,
    NvBufferOutputLocation location,
    NvBool *compatible)
{
    NvError e = NvSuccess;

    NvU32 totalBuffersAllocated;
    NvU32 totalBuffersRequested;
    NvU32 buffersInUse;

    NvU32 minBufferAmount;
    NvU32 maxBufferAmount;

    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            stage, id,
            &minBufferAmount, &maxBufferAmount)
    );
    NV_CHECK_ERROR_CLEANUP(
        m_pBufferStream->GetNumberOfBuffers(
            location,
            &totalBuffersAllocated,
            &totalBuffersRequested,
            &buffersInUse)
    );

    if (m_pMemProfileConfigurator->GetBufferConfigPerfScheme()
            != NVCAMERA_BUFFER_PERF_PERF)
    {
        *compatible = (maxBufferAmount == totalBuffersAllocated);
    }
    else
    {
        // When at performance level, we don't deallocate buffer if
        // it's more than we needed.
        *compatible = (totalBuffersAllocated >= maxBufferAmount);
    }

    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::BufferManagerCheckNumberOfBuffers(
    NvCameraBufferConfigStage stage,
    NvBool *compatible)
{
    NvBufferOutputLocation location;
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    // Capture preview
    location.SetLocation(COMPONENT_CAMERA, CAMERA_OUT_PREVIEW);
    NV_CHECK_ERROR_CLEANUP(
        BufferManagerCheckNumberOfBuffersAtLocation(
            stage, NVCAMERA_BUFFERCONFIG_CAMERA_PREVIEW, location,
            compatible)
    );
    if (*compatible == NV_FALSE)
        return e;

    // Capture still
    location.SetLocation(COMPONENT_CAMERA, CAMERA_OUT_CAPTURE);
    NV_CHECK_ERROR_CLEANUP(
        BufferManagerCheckNumberOfBuffersAtLocation(
            stage, NVCAMERA_BUFFERCONFIG_CAMERA_CAPTURE, location,
            compatible)
    );
    if (*compatible == NV_FALSE)
        return e;

    // DZ Preview
    location.SetLocation(COMPONENT_DZ, DZ_OUT_PREVIEW);
    NV_CHECK_ERROR_CLEANUP(
        BufferManagerCheckNumberOfBuffersAtLocation(
            stage, NVCAMERA_BUFFERCONFIG_DZ_PREVIEW, location,
            compatible)
    );
    if (*compatible == NV_FALSE)
        return e;

    // DZ Still
    location.SetLocation(COMPONENT_DZ, DZ_OUT_STILL);
    NV_CHECK_ERROR_CLEANUP(
        BufferManagerCheckNumberOfBuffersAtLocation(
            stage, NVCAMERA_BUFFERCONFIG_DZ_STILL, location,
            compatible)
    );
    if (*compatible == NV_FALSE)
        return e;

    // DZ Video
    location.SetLocation(COMPONENT_DZ, DZ_OUT_VIDEO);
    NV_CHECK_ERROR_CLEANUP(
        BufferManagerCheckNumberOfBuffersAtLocation(
            stage, NVCAMERA_BUFFERCONFIG_DZ_VIDEO, location,
            compatible)
    );
    if (*compatible == NV_FALSE)
        return e;

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::BufferManagerUpdateNumberOfBuffersAtLocation(
    NvCameraBufferConfigStage stage,
    NvCameraBufferConfigPortID id,
    NvBufferOutputLocation location)
{
    NvError e = NvSuccess;
    NvU32 minBufferAmount = 0;
    NvU32 maxBufferAmount = 0;
    NvU32 allocatedNumberOfBuffers = 0;

    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            stage, id,
            &minBufferAmount, &maxBufferAmount)
    );
    NV_CHECK_ERROR_CLEANUP(
        m_pBufferStream->SetNumberOfBuffers(
            location,
            minBufferAmount,
            maxBufferAmount,
            &allocatedNumberOfBuffers)
    );

    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::BufferManagerUpdateNumberOfBuffers()
{
    NvBufferOutputLocation location;
    NvError e = NvSuccess;
    NvBool compatible = NV_FALSE;
    NvBool restartPreview = NV_FALSE;
    NvCameraBufferConfigStage stage =
        m_pMemProfileConfigurator->GetBufferConfigStage();

    ALOGVV("%s++", __FUNCTION__);

    //Check whether buffer amounts are changed or not
    //If not, just return
    BufferManagerCheckNumberOfBuffers(stage, &compatible);
    if (compatible)
    {
        ALOGVV("%s Compatible buffer configure, early exit",
            __FUNCTION__);
        return e;
    }

    // Capture preview
    location.SetLocation(COMPONENT_CAMERA, CAMERA_OUT_PREVIEW);
    NV_CHECK_ERROR_CLEANUP(
        BufferManagerUpdateNumberOfBuffersAtLocation(
            stage, NVCAMERA_BUFFERCONFIG_CAMERA_PREVIEW, location)
    );

    // Capture still
    location.SetLocation(COMPONENT_CAMERA, CAMERA_OUT_CAPTURE);
    NV_CHECK_ERROR_CLEANUP(
        BufferManagerUpdateNumberOfBuffersAtLocation(
            stage, NVCAMERA_BUFFERCONFIG_CAMERA_CAPTURE, location)
    );

    // DZ Preview
    location.SetLocation(COMPONENT_DZ, DZ_OUT_PREVIEW);
    NV_CHECK_ERROR_CLEANUP(
        BufferManagerUpdateNumberOfBuffersAtLocation(
            stage, NVCAMERA_BUFFERCONFIG_DZ_PREVIEW, location)
    );

    // DZ Still
    location.SetLocation(COMPONENT_DZ, DZ_OUT_STILL);
    NV_CHECK_ERROR_CLEANUP(
        BufferManagerUpdateNumberOfBuffersAtLocation(
            stage, NVCAMERA_BUFFERCONFIG_DZ_STILL, location)
    );

    // DZ Video
    location.SetLocation(COMPONENT_DZ, DZ_OUT_VIDEO);
    NV_CHECK_ERROR_CLEANUP(
        BufferManagerUpdateNumberOfBuffersAtLocation(
            stage, NVCAMERA_BUFFERCONFIG_DZ_VIDEO, location)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::BufferManagerReconfigureNumberOfStillBuffers(
    NvU32 minNumberOfBuffers,
    NvU32 maxNumberOfBuffers,
    NvU32 *allocatedNumberOfBuffers)
{
    NvBufferOutputLocation location;
    NvError e = NvSuccess;
    NvBool restartPreview = NV_FALSE;

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

    location.SetLocation(COMPONENT_CAMERA,CAMERA_OUT_CAPTURE);
    NV_CHECK_ERROR_CLEANUP(
        m_pBufferStream->SetNumberOfBuffers(
            location,
            minNumberOfBuffers,
            maxNumberOfBuffers,
            allocatedNumberOfBuffers)
    );
    //This only affects the configuration for
    //NVCAMERA_BUFFERCONFIG_STAGE_AFTER_FIRST_PREVIEW
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->SetBufferAmount(
            NVCAMERA_BUFFERCONFIG_STAGE_AFTER_FIRST_PREVIEW,
            NVCAMERA_BUFFERCONFIG_CAMERA_CAPTURE,
            *allocatedNumberOfBuffers, *allocatedNumberOfBuffers)
    );

    if (restartPreview == NV_TRUE)
    {
        NV_CHECK_ERROR_CLEANUP(
            StartPreviewInternal()
        );
    }

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::BufferManagerGetNumberOfStillBuffers(
    NvU32 *totalBuffersAllocated)
{
    NvError e = NvSuccess;
    NvBufferOutputLocation location;
    NvU32 totalBuffersRequested = 0;
    NvU32 buffersInUse = 0;

    location.SetLocation(COMPONENT_CAMERA, CAMERA_OUT_CAPTURE);

    e = m_pBufferStream->GetNumberOfBuffers(
        location,
        totalBuffersAllocated,
        &totalBuffersRequested,
        &buffersInUse);

    return e;
}

// allocates the NvMM buffer header, fills out the relevant fields
// uses surfaces allocated in the ANB to
// rough translation of ImportANB in the OMX layer
NvError
NvCameraHal::InitializeNvMMWithANB(
    PreviewBuffer *Buffer,
    NvU32 bufferNum)
{
    buffer_handle_t handle;
    NvU32 width;
    NvU32 height;
    NvU32 i;
    int err;
    NvMMBuffer *nvmmbuf = NULL;
    NvMMSurfaceDescriptor *pSurfaces;
    const NvRmSurface *Surf;
    size_t SurfCount;

    ALOGVV("%s++", __FUNCTION__);

    if (!Buffer || !Buffer->anb)
    {
        ALOGE("%s-- fail @ line %d", __FUNCTION__, __LINE__);
        return NvError_BadParameter;
    }

    handle = (buffer_handle_t)(*Buffer->anb);

    nvgr_get_surfaces(handle, &Surf, &SurfCount);
    width = Surf[0].Width;
    height = Surf[0].Height;

    // TODO sanity check ANB wxh vs. internal camera state's wxh?

    // TODO how much of this is duplicated when we return the buffer after
    // it's delivered?


    // TODO make sure there's a free for this in teardown
    // only alloc/init once on first time

    if (!Buffer->nvmm)
    {
        // this should never run when using buffer manager
        // todo remove
        Buffer->nvmm = (NvMMBuffer *)NvOsAlloc(sizeof(NvMMBuffer));

        NvOsMemset(Buffer->nvmm, 0, sizeof(NvMMBuffer));
        Buffer->nvmm->StructSize = sizeof(NvMMBuffer);
        Buffer->nvmm->BufferID = bufferNum;
        Buffer->nvmm->PayloadType = NvMMPayloadType_SurfaceArray;
    }

    // copy the surface pointers from the ANB to our NvMM buffer
    pSurfaces = &(Buffer->nvmm->Payload.Surfaces);
    NvOsMemcpy(pSurfaces->Surfaces, Surf, sizeof(NvRmSurface) * SurfCount);

    pSurfaces->SurfaceCount = SurfCount;
    pSurfaces->Empty = NV_TRUE;

    ALOGVV("%s--", __FUNCTION__);
    return NvSuccess;
}


NvError NvCameraHal::BufferManagerConfigureANBWindow()
{
    int err = 0;
    NvError e = NvSuccess;
    NvCombinedCameraSettings Settings;
    NvU32 width;
    NvU32 height;

    ALOGVV("%s++", __FUNCTION__);

    if (!mPreviewWindow)
    {
        ALOGE("%s: Native window not set", __FUNCTION__);
        return e; //ok case
    }

    Settings = mSettingsParser.getCurrentSettings();
    width = this->persistentPreview[this->mSensorId].width;//Settings.previewResolution.width;
    height = this->persistentPreview[this->mSensorId].height;//Settings.previewResolution.height;

    err = mPreviewWindow->set_usage(
            mPreviewWindow,
            NV_GRALLOC_USAGE_FLAGS_CAMERA);

    if (err != 0)
    {
        ALOGE("%s: set_usage failed", __FUNCTION__);
        NV_CHECK_ERROR_CLEANUP(NvError_BadParameter);
    }

    err = mPreviewWindow->set_buffers_geometry(
            mPreviewWindow,
            width,
            height,
            // TODO color format
            HAL_PIXEL_FORMAT_YV12);  //??

    if (err != 0)
    {
        ALOGE("%s: set_buffers_geometry failed", __FUNCTION__);
        NV_CHECK_ERROR_CLEANUP(NvError_BadParameter);
    }

    // TODO account for stereo in the crop rect
    err = mPreviewWindow->set_crop(mPreviewWindow, 0, 0, width, height);

    if (err != 0)
    {
        ALOGE("%s: set_crop failed", __FUNCTION__);
        NV_CHECK_ERROR_CLEANUP(NvError_BadParameter);
    }

    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}


NvError NvCameraHal::BufferManagerResetANWBuffers(NvBool reInit)
{

    int err = 0;
    unsigned int i = 0;

    NvU32 cancelStart;
    NvU32 cancelEnd;
    NvError e = NvSuccess;
    NvCombinedCameraSettings Settings;
    NvCameraBufferConfigStage stage =
        m_pMemProfileConfigurator->GetBufferConfigStage();
    NvU32 previewBufferNum;
    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            stage, NVCAMERA_BUFFERCONFIG_DZ_PREVIEW,
            NULL, &previewBufferNum)
    );

    ALOGVV("%s++", __FUNCTION__);
    if (!mPreviewWindow)
    {
        ALOGE("%s: Native window not set", __FUNCTION__);
        return e; //ok case just do nothing
    }

    if (!reInit)
    {

        err = mPreviewWindow->get_min_undequeued_buffer_count(
            mPreviewWindow,
            &mMinUndequeuedPreviewBuffers);

        if (err != 0)
        {
            ALOGE("%s: get_min_undequeued_buffer_count failed", __FUNCTION__);
            NV_CHECK_ERROR_CLEANUP(NvError_BadParameter);
        }

        // Android Native window requires a minimum number of buffers to be
        // undequeued. This is why we set the buffer count to a higher value.
        // Need to dequeue these buffers and then cancel the extra
        // buffers. Refer to NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS in
        // frameworks/base/include/ui/egl/android_natives.h
        err = mPreviewWindow->set_buffer_count(
            mPreviewWindow,
            previewBufferNum + mMinUndequeuedPreviewBuffers);

        if (err != 0)
        {
            ALOGE("%s: set_buffer_count failed", __FUNCTION__);
            NV_CHECK_ERROR_CLEANUP(NvError_BadParameter);
        }
    }

    // Clear out the ANB queue's
    // dequeue everything.  supply  MAX_OUTPUT_BUFFERS to NvMM, cancel the rest
    for (i = 0; i < previewBufferNum + mMinUndequeuedPreviewBuffers; i++)
    {
        int stride;
        // dequeue into our array so that we can track them
        err = mPreviewWindow->dequeue_buffer(
            mPreviewWindow,
            &(mPreviewBuffers[i].anb),
            &stride);
        if (err != 0)
        {
            break;
        }
    }

    if ( err != 0)
    {
        cancelStart = 0;
        cancelEnd = i;
    }
    else
    {
        cancelStart = previewBufferNum;
        cancelEnd = previewBufferNum + mMinUndequeuedPreviewBuffers;
    }
    // now cancel the min undequeued bufs after dequeuing them ALL
    for (i = cancelStart; i < cancelEnd; i++)
    {
        // cancel, it's one of the minimum undequeued bufs
        err = mPreviewWindow->cancel_buffer(
                                            mPreviewWindow,
                                            mPreviewBuffers[i].anb);
        if (err != 0)
        {
            ALOGE("%s: cancel_buffer failed", __FUNCTION__);
            NV_CHECK_ERROR_CLEANUP(NvError_BadParameter);
        }

        mPreviewBuffers[i].anb = NULL;
        mPreviewBuffers[i].nvmm = NULL;
    }
    return e;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

// assumes a valid preview window has been set
NvError NvCameraHal::BufferManagerSupplyPreviewBuffers()
{
    int err = 0;
    unsigned int i = 0;
    NvError e = NvSuccess;
    NvBufferOutputLocation location;
    NvCameraBufferConfigStage stage =
        m_pMemProfileConfigurator->GetBufferConfigStage();
    NvU32 previewBufferNum;
    NvMMBuffer *nvBuffers[NvCameraMemProfileConfigurator::MAX_NUMBER_OF_BUFFERS];
    NvU32 numberFilled = 0;

    DISPLAY_BUFFER_COUNTS();
    ALOGVV("%s++", __FUNCTION__);

    NV_CHECK_ERROR_CLEANUP(
        m_pMemProfileConfigurator->GetBufferAmount(
            stage, NVCAMERA_BUFFERCONFIG_DZ_PREVIEW,
            NULL, &previewBufferNum)
    );

    if (!mPreviewWindow)
    {
        ALOGE("%s: Native window not set", __FUNCTION__);
        return e; //ok case just do nothing
    }

    NV_CHECK_ERROR_CLEANUP(
        BufferManagerConfigureANBWindow()
    );

    NV_CHECK_ERROR_CLEANUP(
            BufferManagerResetANWBuffers(NV_FALSE)
    );

    location.SetLocation(COMPONENT_DZ,DZ_OUT_PREVIEW);
    NV_CHECK_ERROR_CLEANUP(
            m_pBufferStream->GetUnusedBufferPointers(location,
                                    nvBuffers,
                                    previewBufferNum,
                                    &numberFilled)
                            );
    // todo make sure the cfg in buffer manager wxh == curretn wxh

    if (numberFilled != previewBufferNum)
    {
        NV_CHECK_ERROR_CLEANUP(NvError_CountMismatch);
    }

    for (i = 0; i < previewBufferNum; i++)
    {
        err = mPreviewWindow->lock_buffer(mPreviewWindow, mPreviewBuffers[i].anb);
        if (err != 0)
        {
            ALOGE("%s: lock_buffer failed [%d]", __FUNCTION__, err);
        }

        InitializeNvMMBufferWithANB(&mPreviewBuffers[i], nvBuffers[i], i);
    }

    NV_CHECK_ERROR_CLEANUP(
           m_pBufferStream->SendBuffersToLocation(location)
        );

    ALOGVV("%s--", __FUNCTION__);
    return NvSuccess;
fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError
NvCameraHal::InitializeNvMMBufferWithANB(
    PreviewBuffer *previewBuffer,
    NvMMBuffer *nvBuffer,
    NvU32 bufferNum)
{
    NvU32 width;
    NvU32 height;
    NvMMSurfaceDescriptor *pSurfaces;
    const NvRmSurface *Surf;
    size_t SurfCount;

    ALOGVV("%s++", __FUNCTION__);

    if (!previewBuffer || !previewBuffer->anb || !nvBuffer)
    {
        ALOGVV("%s-- fail @ line %d", __FUNCTION__, __LINE__);
        return NvError_BadParameter;
    }

    nvgr_get_surfaces(*previewBuffer->anb, &Surf, &SurfCount);
    width = Surf[0].Width;
    height = Surf[0].Height;

    // set the new pointers to NvMM buffers
    previewBuffer->nvmm = nvBuffer;
    previewBuffer->nvmm->BufferID = bufferNum;

    // copy the surface pointers from the ANB to our NvMM buffer
    // todo use InitializeNvMMWithANB reneame that function so all it does is
    // copy the surfaces.
    pSurfaces = &(previewBuffer->nvmm->Payload.Surfaces);
    NvOsMemcpy(pSurfaces->Surfaces, Surf, sizeof(NvRmSurface) * SurfCount);
    pSurfaces->SurfaceCount = SurfCount;
    pSurfaces->Empty = NV_TRUE;

    ALOGVV("%s--", __FUNCTION__);
    return NvSuccess;
}

} // namespace android
