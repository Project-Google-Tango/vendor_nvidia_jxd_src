/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define LOG_TAG "NvCameraHalBlockHelpers"

#define LOG_NDEBUG 0

#include "nvcamerahal.h"
#include "nvodm_query_discovery.h"

namespace android {

NvError NvCameraHal::ResetCamStreams()
{
    NvMMCameraResetBufferNegotiation ResetStream;
    // This should probably move inside the buffermanager
    // since all of these operations are vital for proper buffer flow
    m_pBufferStream->RecoverBuffersFromComponent(COMPONENT_CAMERA);
    m_pBufferStream->RecoverBuffersFromComponent(COMPONENT_DZ);

    NvOsMemset(&ResetStream,0,sizeof(ResetStream));
    ResetStream.StreamIndex = mCameraStreamIndex_OutputPreview;
    NvError e = NvSuccess;

    UNLOCKED_EVENTS_CALL_CLEANUP(
        Cam.Block->Extension(
            Cam.Block,
            NvMMCameraExtension_ResetBufferNegotiation,
            sizeof(ResetStream),
            &ResetStream,
            0,
            NULL)
    );

    NvOsMemset(&ResetStream,0,sizeof(ResetStream));
    ResetStream.StreamIndex = mCameraStreamIndex_Output;

    UNLOCKED_EVENTS_CALL_CLEANUP(
        Cam.Block->Extension(
            Cam.Block,
            NvMMCameraExtension_ResetBufferNegotiation,
            sizeof(ResetStream),
            &ResetStream,
            0,
            NULL)
    );

    NvMMDigitalZoomResetBufferNegotiation ResetStreamDZ;
    NvOsMemset(&ResetStreamDZ,0,sizeof(ResetStreamDZ));
    ResetStreamDZ.StreamIndex = NvMMDigitalZoomStreamIndex_InputPreview;
    UNLOCKED_EVENTS_CALL_CLEANUP(
        DZ.Block->Extension(
            DZ.Block,
            NvMMDigitalZoomExtension_ResetBufferNegotiation,
            sizeof(ResetStreamDZ),
            &ResetStreamDZ,
            0,
            NULL)
    );

    NvOsMemset(&ResetStreamDZ,0,sizeof(ResetStreamDZ));
    ResetStreamDZ.StreamIndex = NvMMDigitalZoomStreamIndex_Input;
    UNLOCKED_EVENTS_CALL_CLEANUP(
        DZ.Block->Extension(
            DZ.Block,
            NvMMDigitalZoomExtension_ResetBufferNegotiation,
            sizeof(ResetStreamDZ),
            &ResetStreamDZ,
            0,
            NULL)
    );

    NvOsMemset(&ResetStreamDZ,0,sizeof(ResetStreamDZ));
    ResetStreamDZ.StreamIndex = NvMMDigitalZoomStreamIndex_OutputPreview;
    UNLOCKED_EVENTS_CALL_CLEANUP(
        DZ.Block->Extension(
            DZ.Block,
            NvMMDigitalZoomExtension_ResetBufferNegotiation,
            sizeof(ResetStreamDZ),
            &ResetStreamDZ,
            0,
            NULL)
    );

    NvOsMemset(&ResetStreamDZ,0,sizeof(ResetStreamDZ));
    ResetStreamDZ.StreamIndex = NvMMDigitalZoomStreamIndex_OutputVideo;
    UNLOCKED_EVENTS_CALL_CLEANUP(
        DZ.Block->Extension(
            DZ.Block,
            NvMMDigitalZoomExtension_ResetBufferNegotiation,
            sizeof(ResetStreamDZ),
            &ResetStreamDZ,
            0,
            NULL)
    );

    NvOsMemset(&ResetStreamDZ,0,sizeof(ResetStreamDZ));
    ResetStreamDZ.StreamIndex = NvMMDigitalZoomStreamIndex_OutputStill;
    UNLOCKED_EVENTS_CALL_CLEANUP(
        DZ.Block->Extension(
            DZ.Block,
            NvMMDigitalZoomExtension_ResetBufferNegotiation,
            sizeof(ResetStreamDZ),
            &ResetStreamDZ,
            0,
            NULL)
    );

    NV_CHECK_ERROR_CLEANUP(
        InitializeNvMMBlockContainer(Cam)
    );
    NV_CHECK_ERROR_CLEANUP(
        InitializeNvMMBlockContainer(DZ)
    );

    EnableCameraBlockPins(
            (NvMMCameraPin)(NvMMCameraPin_Preview | NvMMCameraPin_Capture)
    );

    NV_CHECK_ERROR_CLEANUP(
        EnableDZBlockPins(
            (NvMMDigitalZoomPin)(
                NvMMDigitalZoomPin_Preview |
                NvMMDigitalZoomPin_Still   |
                NvMMDigitalZoomPin_Video))
);
fail:

    ALOGVV("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::ConnectCaptureGraph()
{
    NvError e = NvSuccess;
    TunnelCamDZPreview();
    ALOGVV("%s  : %d", __FUNCTION__, __LINE__);
    // set TBF output
    NV_CHECK_ERROR_CLEANUP(
        SetupDZPreviewOutput()
    );
    // End of preview connections

    // Connect Still
    TunnelCamDZCapture();
    NV_CHECK_ERROR_CLEANUP(
        SetupDZStillOutput()
    );
    // End of still connections

    // Connect video
    NV_CHECK_ERROR_CLEANUP(
        SetupDZVideoOutput()
    );
fail:

    ALOGVV("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}


NvU64 NvCameraHal::GetSensorId()
{
    char value[PROPERTY_VALUE_MAX];

    property_get("nv-camera-enable-tpg", value, "0");

    if (strncmp(value, "bayer", 5) == 0)
    {
        if (mSensorId == 0)
        {
            return NV_IMAGER_TPGABAYER_ID;
        }
        else
        {
            return NV_IMAGER_TPGBBAYER_ID;
        }
    }
    else if (strncmp(value, "rgb", 3) == 0)
    {
        if (mSensorId == 0)
        {
            return NV_IMAGER_TPGARGB_ID;
        }
        else
        {
            return NV_IMAGER_TPGBRGB_ID;
        }
    }

    return mSensorId;
}

NvError NvCameraHal::CreateNvMMCameraBlockContainer()
{
    NvError e = NvSuccess;
    NvMMBlockType BlockType = NvMMBlockType_SrcCamera;
    NvMMCreationParameters CreationParams;

    ALOGVV("%s++", __FUNCTION__);

    Cam.NumStreams = NUM_STREAMS_CSI;

    if (isUSB())
    {
        BlockType = NvMMBlockType_SrcUSBCamera;
        Cam.NumStreams = NUM_STREAMS_USB;
    }

    NvOsMemset(&CreationParams, 0, sizeof(CreationParams));
    CreationParams.Locale = NvMMLocale_CPU;
    CreationParams.Type = BlockType;
    CreationParams.SetULPMode = NV_FALSE;
    CreationParams.BlockSpecific = GetSensorId();
    CreationParams.structSize = sizeof(NvMMCreationParameters);

    NV_CHECK_ERROR_CLEANUP(
        NvMMOpenBlock(&Cam.Block, &CreationParams)
    );

    NV_CHECK_ERROR_CLEANUP(
        InitializeNvMMBlockContainer(Cam)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    CloseNvMMBlockContainer(Cam);

    ALOGVV("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::CreateNvMMDZBlockContainer()
{
    NvError e = NvSuccess;
    NvMMCreationParameters CreationParams;

    ALOGVV("%s++", __FUNCTION__);

    DZ.NumStreams = NUM_STREAMS_DZ;

    NvOsMemset(&CreationParams, 0, sizeof(CreationParams));
    CreationParams.Locale = NvMMLocale_CPU;
    CreationParams.Type = NvMMBlockType_DigitalZoom;
    CreationParams.SetULPMode = NV_FALSE;
    CreationParams.BlockSpecific = 0;
    CreationParams.structSize = sizeof(NvMMCreationParameters);

    NV_CHECK_ERROR_CLEANUP(
        NvMMOpenBlock(&DZ.Block, &CreationParams)
    );

    NV_CHECK_ERROR_CLEANUP(
        InitializeNvMMBlockContainer(DZ)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    CloseNvMMBlockContainer(DZ);
    ALOGVV("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError
NvCameraHal::InitializeNvMMBlockContainer(
    NvMMBlockContainer &BlockContainer)
{
    NvError e = NvSuccess;
    NvU32 i = 0;
    NvU32 j = 0;

    ALOGVV("%s++", __FUNCTION__);

    BlockContainer.MyHal = this;

    BlockContainer.BlockCloseDone = NV_FALSE;

    for (i = 0; i < BlockContainer.NumStreams; i++)
    {
        BlockContainer.Ports[i].BufferConfigDone = NV_FALSE;
        BlockContainer.Ports[i].BufferNegotiationDone = NV_FALSE;
        for (j = 0; j < NvCameraMemProfileConfigurator::MAX_NUMBER_OF_BUFFERS; j++)
        {
            BlockContainer.Ports[i].Buffers[j] = NULL;
        }
    }

    BlockContainer.TransferBufferToBlock =
        BlockContainer.Block->GetTransferBufferFunction(
            BlockContainer.Block,
            0,
            NULL);

    BlockContainer.Block->SetSendBlockEventFunction(
        BlockContainer.Block,
        (void *) &BlockContainer, // NvMM will pass this back up to us in evts
        NvMMEventHandler);

    ALOGVV("%s--", __FUNCTION__);
    return e;
}

NvError
NvCameraHal::CloseNvMMBlockContainer(
    NvMMBlockContainer &BlockContainer)
{
    NvError e = NvSuccess;
    NvU32 i = 0;
    ALOGVV("%s++", __FUNCTION__);
    // close NvMM block, allow any events that are triggered to make it through
    if (BlockContainer.Block)
    {
        UNLOCK_EVENTS();
        NvMMCloseBlock(BlockContainer.Block);
        RELOCK_EVENTS();
    }
    else
    {
        ALOGE("%s: Component handle is invalid", __FUNCTION__);
    }

    CheckAndWaitForCondition(
        !BlockContainer.BlockCloseDone,
        BlockContainer.BlockCloseDoneCond);
    BlockContainer.Block = NULL;
    ALOGVV("%s--", __FUNCTION__);
    return e;
}


// rough translation of NvxCameraOpen 2nd half for now, since that does all
// the port enabling and tunneling
void NvCameraHal::ConnectCameraToDZ()
{
    ALOGVV("%s++", __FUNCTION__);
    TunnelCamDZPreview();
    TunnelCamDZCapture();
}

NvError NvCameraHal::EnableCameraBlockPins(NvMMCameraPin EnabledCameraPins)
{
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    if (Cam.Block)
    {
        e = Cam.Block->SetAttribute(
                Cam.Block,
                NvMMCameraAttribute_PinEnable,
                0,
                sizeof(NvMMCameraPin),
                &EnabledCameraPins);
    }
    else
    {
        ALOGE("%s: Camera component already released", __FUNCTION__);
    }

    ALOGVV("%s--", __FUNCTION__);
    return e;
}

NvError NvCameraHal::EnableDZBlockPins(NvMMDigitalZoomPin EnabledDZPins)
{
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    if (DZ.Block)
    {
        e = DZ.Block->SetAttribute(
                DZ.Block,
                NvMMDigitalZoomAttributeType_PinEnable,
                0,
                sizeof(NvMMDigitalZoomPin),
                &EnabledDZPins);
    }
    else
    {
        ALOGE("%s: DZ Component already released", __FUNCTION__);
    }

    ALOGVV("%s--", __FUNCTION__);
    return e;
}

#ifdef UNTUNNEL_CAMERA_DZ // [

/**
 * An NvMMTransferBufferFunction for handling buffers being transferred
 * between the camera and digitalzoom blocks.  It simply forwards the call
 * to a method on the NvCameraHal object specified in the pContext object.
 */
static NvError CameraDzForwardBufferFunc(void *pContext,
                                         NvU32 StreamIndex,
                                         NvMMBufferType BufferType,
                                         NvU32 BufferSize,
                                         void *pBuffer)
{
    NvMMBlockContainer *container = (NvMMBlockContainer *)pContext;
    NvCameraHal *hal = container->MyHal;

    return hal->CameraDzForwardBuffer(container, StreamIndex, BufferType, BufferSize, pBuffer);
}

/**
 * Forward a single buffer from camera to digitalzoom or vice versa.
 * (Note that almost this entire method is debug spew - the real work
 * is done by the TransferBufferToBlock() call)
 */
NvError NvCameraHal::CameraDzForwardBuffer(NvMMBlockContainer *pContext,
                                           NvU32 StreamIndex,
                                           NvMMBufferType BufferType,
                                           NvU32 BufferSize,
                                           void *pBuffer)
{
    const bool DEBUG_SPEW = true;
    NvError result = NvSuccess;

    if (pContext == &DZ)
    {
        // Camera -> DZ
        result = ProcessCamToDZMessages(pContext,StreamIndex,BufferType,BufferSize,pBuffer);
    }
    else if (pContext == &Cam)
    {
        // DZ -> Camera
        result = ProcessDzToCamMessages(pContext,StreamIndex,BufferType,BufferSize,pBuffer);
    }
    else
    {
        ALOGE("ERROR: unknown context in %s: %p\n", __FUNCTION__, pContext);
        return NvError_BadParameter;
    }
    return result;
}

NvError NvCameraHal::ProcessCamToDZMessages(NvMMBlockContainer *pContext,
                                           NvU32 StreamIndex,
                                           NvMMBufferType BufferType,
                                           NvU32 BufferSize,
                                           void *pBuffer)
{
        char prefix[256];
        char middle[256];
        NvOsSnprintf(prefix, sizeof(prefix), "buffer camera->DZ[%d]: ", StreamIndex);
        NvMMBuffer *nvmmBuffer = (NvMMBuffer *)pBuffer;
        NvError result = NvSuccess;

        if (BufferType == NvMMBufferType_StreamEvent)
        {
            NvMMEventType eventType = ((NvMMStreamEventBase *)pBuffer)->event;
            switch (eventType)
            {
            case NvMMEvent_NewBufferRequirements:
                // buffer manager handles buffers in referance to the ouput ports
                // so it will look at the cam.out configurations and flags.
                if (StreamIndex == NvMMDigitalZoomStreamIndex_InputPreview)
                {
                    Cam.Ports[mCameraStreamIndex_OutputPreview].BufferConfigDone = NV_FALSE;
                }
                if (StreamIndex == NvMMDigitalZoomStreamIndex_Input)
                {
                    Cam.Ports[mCameraStreamIndex_Output].BufferConfigDone = NV_FALSE;
                }
                NvOsStrcpy(middle, "NewBufferRequirements");
                break;
            case NvMMEvent_NewBufferConfiguration:
                // buffer manager handles buffers in referance to the ouput ports
                // so it will look at the cam.out configurations and flags.
                if (StreamIndex == NvMMDigitalZoomStreamIndex_InputPreview)
                {
                    Cam.Ports[mCameraStreamIndex_OutputPreview].BuffCfg =
                             *((NvMMNewBufferConfigurationInfo *)pBuffer);
                }
                if (StreamIndex == NvMMDigitalZoomStreamIndex_Input)
                {
                    Cam.Ports[mCameraStreamIndex_Output].BuffCfg =
                             *((NvMMNewBufferConfigurationInfo *)pBuffer);
                }

                NvOsStrcpy(middle, "NewBufferConfiguration Saved");
                break;
            case NvMMEvent_NewBufferConfigurationReply:
                NvOsStrcpy(middle, "NewBufferConfigurationReply NOT SET config done");
                break;
            case NvMMEvent_BufferNegotiationFailed:
                NvOsStrcpy(middle, "BufferNegotiationFailed");
                break;
            case NvMMEvent_StreamShutdown:
                NvOsStrcpy(middle, "StreamShutdown");
                break;
            case NvMMEvent_StreamEnd:
                NvOsStrcpy(middle, "StreamEnd");
                break;
            default:
                NvOsSnprintf(middle, sizeof(middle), "unknown event %d", eventType);
                break;
            }
        }
        else if (BufferType  == NvMMBufferType_Payload)
        {
            NvOsSnprintf(middle, sizeof(middle), "payload (id=%d)", nvmmBuffer->BufferID);
        }
        else
        {
            NvOsSnprintf(middle, sizeof(middle), "unknown buffer type (%d)", BufferType);
        }

        ALOGVV("%s%s ... result=%d\n", prefix, middle, result);

        result = pContext->TransferBufferToBlock(pContext->Block,
                                                StreamIndex,
                                                BufferType,
                                                BufferSize,
                                                pBuffer);
        return result;
}

NvError NvCameraHal::ProcessDzToCamMessages(NvMMBlockContainer *pContext,
                                           NvU32 StreamIndex,
                                           NvMMBufferType BufferType,
                                           NvU32 BufferSize,
                                           void *pBuffer)
{
        char prefix[256];
        char middle[256];
        NvOsSnprintf(prefix, sizeof(prefix), "buffer DZ->Cam[%d]: ", StreamIndex);
        NvMMBuffer *nvmmBuffer = (NvMMBuffer *)pBuffer;
        NvError result = NvSuccess;

        NV_ASSERT(pContext && pContext->Block && pBuffer);

        if (!pContext || !pContext->Block || !pBuffer)
            return  NvError_BadParameter;

        if (BufferType == NvMMBufferType_StreamEvent)
        {
            NvMMEventType eventType = ((NvMMStreamEventBase *)pBuffer)->event;
            switch (eventType)
            {
            case NvMMEvent_NewBufferRequirements:
                NvOsStrcpy(middle, "NewBufferRequirements");
                pContext->Ports[StreamIndex].BufferConfigDone = NV_FALSE;
                pContext->Ports[StreamIndex].BuffReq =
                            *((NvMMNewBufferRequirementsInfo *)pBuffer);
                if (StreamIndex == mCameraStreamIndex_OutputPreview)
                {
                    DZ.Ports[NvMMDigitalZoomStreamIndex_InputPreview].BufferConfigDone = NV_FALSE;
                }
                if (StreamIndex == mCameraStreamIndex_Output )
                {
                    DZ.Ports[NvMMDigitalZoomStreamIndex_Input].BufferConfigDone = NV_FALSE;
                }
                break;
            case NvMMEvent_NewBufferConfiguration:
                NvOsStrcpy(middle, "NewBufferConfiguration");
                break;
            case NvMMEvent_NewBufferConfigurationReply:
                NvOsStrcpy(middle, "NewBufferConfigurationReply ");
                pContext->Ports[StreamIndex].BufferConfigDone = NV_TRUE;  // set cam port to true
                pContext->Ports[StreamIndex].BufferConfigDoneCond.signal();
                break;
            case NvMMEvent_BufferNegotiationFailed:
                NvOsStrcpy(middle, "BufferNegotiationFailed");
                break;
            case NvMMEvent_StreamShutdown:
                NvOsStrcpy(middle, "StreamShutdown");
                break;
            case NvMMEvent_StreamEnd:
                NvOsStrcpy(middle, "StreamEnd");
                break;
            default:
                NvOsSnprintf(middle, sizeof(middle), "unknown event %d", eventType);
                break;
            }
        }
        else if (BufferType  == NvMMBufferType_Payload)
        {
            NvOsSnprintf(middle, sizeof(middle), "payload (id=%d)", nvmmBuffer->BufferID);
        }
        else
        {
            NvOsSnprintf(middle, sizeof(middle), "unknown buffer type (%d)", BufferType);
        }

        ALOGVV("%s%s ... result=%d\n", prefix, middle, result);

        result = pContext->TransferBufferToBlock(pContext->Block,
                                                StreamIndex,
                                                BufferType,
                                                BufferSize,
                                                pBuffer);
        return result;
}


#endif // UNTUNNEL_CAMERA_DZ ]



void NvCameraHal::TunnelCamDZPreview()
{
    ALOGVV("%s++", __FUNCTION__);

#ifdef UNTUNNEL_CAMERA_DZ // [
    // manually forward cam preview output <-> DZ preview input
    Cam.Block->SetTransferBufferFunction(
        Cam.Block,
        mCameraStreamIndex_OutputPreview,
        CameraDzForwardBufferFunc,
        (void *)&DZ,
        NvMMDigitalZoomStreamIndex_InputPreview);
    // and the return path
    DZ.Block->SetTransferBufferFunction(
        DZ.Block,
        NvMMDigitalZoomStreamIndex_InputPreview,
        CameraDzForwardBufferFunc,
        (void *)&Cam,
        mCameraStreamIndex_OutputPreview);
#else // ][
    // tunnel cam preview output <-> DZ preview input
    Cam.Block->SetTransferBufferFunction(
        Cam.Block,
        mCameraStreamIndex_OutputPreview,
        DZ.TransferBufferToBlock,
        (void *)DZ.Block,
        NvMMDigitalZoomStreamIndex_InputPreview);
    // and the return path
    DZ.Block->SetTransferBufferFunction(
        DZ.Block,
        NvMMDigitalZoomStreamIndex_InputPreview,
        Cam.TransferBufferToBlock,
        (void *)Cam.Block,
        mCameraStreamIndex_OutputPreview);
#endif // UNTUNNEL_CAMERA_DZ ]

    // camera block is buffer allocator for this stream
    Cam.Block->SetBufferAllocator(
        Cam.Block,
        mCameraStreamIndex_OutputPreview,
        NV_TRUE);

    ALOGVV("%s--", __FUNCTION__);
}
void NvCameraHal::TunnelCamDZCapture()
{
    ALOGVV("%s++", __FUNCTION__);

#ifdef UNTUNNEL_CAMERA_DZ // [
    // manually tunnel cam capture output <-> DZ capture input
    Cam.Block->SetTransferBufferFunction(
        Cam.Block,
        mCameraStreamIndex_Output,
        CameraDzForwardBufferFunc,
        (void *)&DZ,
        NvMMDigitalZoomStreamIndex_Input);
    // and the return path
    DZ.Block->SetTransferBufferFunction(
        DZ.Block,
        NvMMDigitalZoomStreamIndex_Input,
        CameraDzForwardBufferFunc,
        (void *)&Cam,
        mCameraStreamIndex_Output);
#else // ][
    // tunnel cam capture output <-> DZ capture input
    Cam.Block->SetTransferBufferFunction(
        Cam.Block,
        mCameraStreamIndex_Output,
        DZ.TransferBufferToBlock,
        (void *)DZ.Block,
        NvMMDigitalZoomStreamIndex_Input);
    // and the return path
    DZ.Block->SetTransferBufferFunction(
        DZ.Block,
        NvMMDigitalZoomStreamIndex_Input,
        Cam.TransferBufferToBlock,
        (void *)Cam.Block,
        mCameraStreamIndex_Output);
#endif // UNTUNNEL_CAMERA_DZ ]

    // camera block is buffer allocator for this stream
    Cam.Block->SetBufferAllocator(
        Cam.Block,
        mCameraStreamIndex_Output,
        NV_TRUE);

    ALOGVV("%s--", __FUNCTION__);
}

// rough translation / minimal subset of NvxNvMMTransformSetupOutput
NvError NvCameraHal::SetupDZPreviewOutput()
{
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    // tell block where to send the output buffers
    NV_CHECK_ERROR_CLEANUP(
        DZ.Block->SetTransferBufferFunction(
            DZ.Block,
            NvMMDigitalZoomStreamIndex_OutputPreview,
            NvMMDeliverFullOutput,
            (void *)&DZ,
            NvMMDigitalZoomStreamIndex_OutputPreview)
    );
    // don't allocate buffers here like we do for still and video,
    // because preview uses ANBs, where the buffers are supplied by the app.

    ALOGVV("%s--", __FUNCTION__);
    return e;
fail:
    ALOGVV("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

// rough translation / minimal subset of NvxNvMMTransformSetupOutput
NvError NvCameraHal::SetupDZStillOutput()
{
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    NV_CHECK_ERROR_CLEANUP(
        SetupCaptureOutput(
            DZ,
            NvMMDigitalZoomStreamIndex_OutputStill)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;
fail:
    ALOGVV("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal::SetupDZVideoOutput()
{
    NvError e = NvSuccess;

    ALOGVV("%s++", __FUNCTION__);

    NV_CHECK_ERROR_CLEANUP(
        SetupCaptureOutput(
            DZ,
            NvMMDigitalZoomStreamIndex_OutputVideo)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;
fail:
    ALOGVV("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

// takes BlockContainer in the hopes that it will be easy to adjust its clients
// when DZ logic moves to the HAL and we only want to use the NvMM stuff to
// deal with the camera block
NvError NvCameraHal::SetupCaptureOutput(
    NvMMBlockContainer &BlockContainer,
    NvU32 StreamNum)
{
    NvError e = NvSuccess;
    NvU32 i = 0;

    ALOGVV("%s++", __FUNCTION__);

    // tell block where to send the output buffers
    NV_CHECK_ERROR_CLEANUP(
        BlockContainer.Block->SetTransferBufferFunction(
            BlockContainer.Block,
            StreamNum,
            NvMMDeliverFullOutput,
            (void *)&BlockContainer,
            StreamNum)
    );

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    // how should we fail elegantly in case of partial success?
    ALOGVV("%s-- (error [0x%x])", __FUNCTION__, e);
    return e;
}

void NvCameraHal::SendEmptyPreviewBufferToDZ(NvMMBuffer *Buffer)
{
    ReturnEmptyBufferToDZ(Buffer, NvMMDigitalZoomStreamIndex_OutputPreview);
}

void NvCameraHal::SendEmptyStillBufferToDZ(NvMMBuffer *Buffer)
{
    ReturnEmptyBufferToDZ(Buffer, NvMMDigitalZoomStreamIndex_OutputStill);
}

void NvCameraHal::SendEmptyVideoBufferToDZ(NvMMBuffer *Buffer)
{
    ReturnEmptyBufferToDZ(Buffer, NvMMDigitalZoomStreamIndex_OutputVideo);
}

void
NvCameraHal::ReturnEmptyBufferToDZ(
    NvMMBuffer *Buffer,
    NvU32 StreamIndex)
{
    // mark empty and return
    Buffer->Payload.Surfaces.Empty = NV_TRUE;
    DZ.TransferBufferToBlock(
        DZ.Block,
        StreamIndex,
        NvMMBufferType_Payload,
        sizeof(NvMMBuffer),
        Buffer);
}

NvError
NvCameraHal::TransitionBlockToState(
    NvMMBlockContainer &BlockContainer,
    NvMMState State)
{
    NvError e = NvSuccess;
    ALOGVV("%s++", __FUNCTION__);

    if (BlockContainer.Block)
    {
        NV_CHECK_ERROR_CLEANUP(
            BlockContainer.Block->SetState(BlockContainer.Block, State)
        );
        WaitForCondition(BlockContainer.StateChangeDone);
    }
    else
    {
        ALOGE("%s: Component handle is invalid", __FUNCTION__);
    }

    ALOGVV("%s--", __FUNCTION__);
    return e;

fail:
    ALOGE("%s-- (error 0x%x)", __FUNCTION__, e);
    return e;
}

// close down our NvMM setup
NvError NvCameraHal::CleanupNvMM()
{
    NvError e = NvSuccess;
    ALOGVV("%s++", __FUNCTION__);

    // transition out of running state
    TransitionBlockToState(Cam, NvMMState_Stopped);
    TransitionBlockToState(DZ, NvMMState_Stopped);

    BufferManagerReclaimALLBuffers();
    // Do not free have buffers be persistant
    //m_pBufferStream->FreeUnusedBuffers();


    // clear out pins
    EnableCameraBlockPins((NvMMCameraPin)0);
    EnableDZBlockPins((NvMMDigitalZoomPin)0);

    // close blocks
    CloseNvMMBlockContainer(Cam);
    CloseNvMMBlockContainer(DZ);

    ALOGVV("%s--", __FUNCTION__);
    return e;
}

} // namespace android
