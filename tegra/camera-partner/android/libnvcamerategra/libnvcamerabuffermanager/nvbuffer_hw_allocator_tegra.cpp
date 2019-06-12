/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define LOG_TAG "NvCameraBufferManager"

#include "nvbuffer_hw_allocator_tegra.h"
#include "nvmm_digitalzoom.h"
#include "nvmm_camera_types.h"
#include "nvmm_usbcamera.h"
#include "nvmm_camera.h"
#include "nvmm_exif.h"
#include "nvcamera_makernote_extension.h"
#include "nvassert.h"


#define CAMERA_MAX_OUTPUT_BUFFERS_PREVIEW 2
#define CAMERA_MAX_OUTPUT_BUFFERS_STILL 4
#define CAMERA_MAX_OUTPUT_BUFFERS_VIDEO 6

#define CAMERA_DEFAULT_PREVIEW_WIDTH 1920
#define CAMERA_DEFAULT_PREVIEW_HEIGHT 1088

// define a max wait in nanosec
#define GENERAL_CONDITION_TIMEOUT 5000000000LL //5sec

#define PITCH_ALIGNMENT   0x40

NvError WaitForCondition(Condition &Cond,Mutex &mLock);
NvMMDigitalZoomStreamIndex  GetDzOutStreamIndex(NvU32 port);
NvMMCameraStreamIndex       GetCamOutStreamIndex(NvU32 port, NvBool isCamUSB);
// a little helper
NvError WaitForCondition(Condition &Cond,Mutex &mLock)
{
    NvError err = NvSuccess;
    android::status_t waitResult;
    waitResult = Cond.waitRelative(mLock, GENERAL_CONDITION_TIMEOUT);
    if (waitResult == TIMED_OUT)
    {
        NV_RETURN_FAIL(NvError_Timeout);
    }
    else if (waitResult != NO_ERROR)
    {
        NV_RETURN_FAIL(NvError_BadParameter);
    }
    return NvSuccess;
}


NvMMDigitalZoomStreamIndex GetDzOutStreamIndex(NvU32 port)
{
    switch (port)
    {
    case  DZ_OUT_PREVIEW:
        return NvMMDigitalZoomStreamIndex_OutputPreview;
    case  DZ_OUT_STILL:
        return NvMMDigitalZoomStreamIndex_OutputStill;
    case  DZ_OUT_VIDEO:
        return NvMMDigitalZoomStreamIndex_OutputVideo;
    case  DZ_OUT_THUMBNAIL:
        return NvMMDigitalZoomStreamIndex_OutputThumbnail;
    default:
        NV_ERROR_MSG("Invalid Port Specified");
        return NvMMDigitalZoomStreamIndex_OutputPreview;
    }
}

NvMMCameraStreamIndex GetCamOutStreamIndex(NvU32 port, NvBool isCamUSB)
{
    switch (port)
    {
    case  CAMERA_OUT_PREVIEW:
        return (isCamUSB)?(NvMMCameraStreamIndex)NvMMUSBCameraStreamIndex_OutputPreview:
                      NvMMCameraStreamIndex_OutputPreview;
    case  CAMERA_OUT_CAPTURE:
        return (isCamUSB)?(NvMMCameraStreamIndex)NvMMUSBCameraStreamIndex_Output:
                      NvMMCameraStreamIndex_Output;
    default:
        NV_ERROR_MSG("Invalid Port Specified");
        return (isCamUSB)?(NvMMCameraStreamIndex)NvMMUSBCameraStreamIndex_OutputPreview:
                      NvMMCameraStreamIndex_OutputPreview;
    }
}



TegraBufferConfig::TegraBufferConfig(NvCameraDriverInfo const &driverInfo) :
                    NvBufferConfigurator(driverInfo)
{

    ConfigureDrivers();
    DZTransferBufferToBlockFunct = driverInfo.DZ->GetTransferBufferFunction(driverInfo.DZ,0,NULL);
}

NvError TegraBufferConfig::ConfigureDrivers()
{
    NvBool externalManager = NV_TRUE;

    // Tell DZ it is not an allocator on any of the ports
    // since we are using the buffer manager
    m_DriverInfo.DZ->SetBufferAllocator(
            m_DriverInfo.DZ,
            NvMMDigitalZoomStreamIndex_OutputPreview,
            NV_FALSE);

    m_DriverInfo.DZ->SetBufferAllocator(
           m_DriverInfo.DZ,
           NvMMDigitalZoomStreamIndex_OutputVideo,
#if NV_CAMERA_V3
           NV_FALSE);
#else
           NV_TRUE);
#endif

    m_DriverInfo.DZ->SetBufferAllocator(
            m_DriverInfo.DZ,
            NvMMDigitalZoomStreamIndex_OutputStill,
#if NV_CAMERA_V3
            NV_FALSE);
#else
            NV_TRUE);
#endif

    m_DriverInfo.DZ->SetAttribute(
            m_DriverInfo.DZ,
            NvMMDigitalZoomAttributeType_ExternalBufferManager,
            0,
            sizeof(NvBool),
            &externalManager);


    m_DriverInfo.Cam->SetBufferAllocator(
            m_DriverInfo.Cam,
            (m_DriverInfo.isCamUSB)?
            (NvMMCameraStreamIndex)NvMMUSBCameraStreamIndex_OutputPreview:
            NvMMCameraStreamIndex_OutputPreview,
            NV_TRUE);

    m_DriverInfo.Cam->SetBufferAllocator(
            m_DriverInfo.Cam,
            (m_DriverInfo.isCamUSB)?
            (NvMMCameraStreamIndex)NvMMUSBCameraStreamIndex_Output:
            NvMMCameraStreamIndex_Output,
            NV_TRUE);

    m_DriverInfo.Cam->SetAttribute(
            m_DriverInfo.Cam,
            NvMMCameraAttribute_ExternalBufferManager,
            0,
            sizeof(NvBool),
            &externalManager);

    return NvSuccess;
}


NvError TegraBufferConfig::GetOutputRequirements(NvBufferManagerComponent component,
                                                 NvComponentBufferConfig *pStreamBuffCfg)
{
    NvError err = NvSuccess;
    switch (component)
    {
        case COMPONENT_CAMERA:
            //GetCapturequirement(pStreamBuffCfg);
            break;
        case COMPONENT_DZ:
            err = GetDZRequirements(pStreamBuffCfg);
            if (err != NvSuccess)
            {
                NV_RETURN_FAIL(err);
            }
            break;
        case COMPONENT_HOST:
            //GetHostCfgFromRequirement(pStreamBuffCfg);
            break;
        default:
            NV_DEBUG_MSG("Error unknown component" );
            break;
    }

    return err;
}

NvError TegraBufferConfig::GetOutputConfiguration(NvBufferManagerComponent component,
                                                  NvComponentBufferConfig *pStreamBuffCfg)
{
    NvError err = NvSuccess;
    switch (component)
    {
        case COMPONENT_CAMERA:
            err = GetCaptureCfgAndReq(pStreamBuffCfg);
            if (err != NvSuccess)
            {
                NV_RETURN_FAIL(err);
            }
            break;
        case COMPONENT_DZ:
            err = GetDzCfgFromRequirement(pStreamBuffCfg);
            if (err != NvSuccess)
            {
                NV_RETURN_FAIL(err);
            }
            break;
        case COMPONENT_HOST:
            //GetHostCfgFromRequirement(pStreamBuffCfg);
            break;
        default:
            NV_DEBUG_MSG("Error unknown component" );
            break;
    }

    return err;
}

NvError TegraBufferConfig::GetInputRequirement(NvBufferManagerComponent component,
                                               NvComponentBufferConfig *pStreamBuffCfg)
{
    // TODO IMPL
    switch (component)
    {
        case COMPONENT_CAMERA:
            //GetCaptureInputBufferFromReq(pStreamBuffCfg);
            break;
        case COMPONENT_DZ:
            //GetDzInputBufferFromReq(pStreamBuffCfg);
            break;
        case COMPONENT_HOST:
            //GetHostInputBufferFromReq(pStreamBuffCfg);
            break;
        default:
            NV_DEBUG_MSG("Error unknown component" );
            break;
    }
    return NvSuccess;
}

/*
 *  Use "adb shell setprop camera.mode.videolayout.flags n" to
 *  change video surface layout to
 *  n = 1 -> Pitch
 *  n = 2 -> Tiled
 *  n = 3 -> Blocklinear
 */
NvRmSurfaceLayout
TegraBufferConfig::GetUserSetSurfaceLayout(void)
{
    NvRmSurfaceLayout layout;
    char value[PROPERTY_VALUE_MAX];

    if (property_get("camera.mode.videolayout.flags", value, "") == 0)
        return (NvRmSurfaceLayout)0;

    layout = (NvRmSurfaceLayout) atoi(value);

    if (layout <= 0 || layout >= NvRmSurfaceLayout_Num)
    {
        NvOsDebugPrintf("Error: Invalid user set surface layout.\n");
        return (NvRmSurfaceLayout)0;
    }

    return layout;
}

// Fill out the surface format info
// including NumberOfSurfaces, ColorFormat,
// Layout and etc. for DZ buffer requirement
NvError TegraBufferConfig::PopulateDZSurfaceFormatInfo(
                                NvMMNewBufferRequirementsInfo *pReq,
                                NvU32 port)
{
    NV_ASSERT(pReq);

    if (pReq == NULL)
        return NvError_BadParameter;

    NvError e = NvSuccess;

    NvMMVideoFormat *pVideoFormat = &pReq->format.videoFormat;
    NvRmSurface *pSurf = &pVideoFormat->SurfaceDescription[0];
    NvFOURCCFormat PixelFormat;

    // fill in the most common settings
    pVideoFormat->NumberOfSurfaces = 3;
    pSurf[0].ColorFormat = NvColorFormat_Y8;
    pSurf[1].ColorFormat = NvColorFormat_U8;
    pSurf[2].ColorFormat = NvColorFormat_V8;
    pSurf[0].Layout = NvRmSurfaceLayout_Pitch;
    PixelFormat = FOURCC_YV12;

    // allow user to use setprop to set custom video
    // layout format
    if (port == DZ_OUT_VIDEO)
    {
        //User specified layout format
        NvRmSurfaceLayout layout = GetUserSetSurfaceLayout();
        if (layout != 0)
            pSurf[0].Layout = layout;
    }

    // chip specific ones
#ifdef CAMERA_T124
    if (port == DZ_OUT_STILL)
    {
        PixelFormat = FOURCC_NV12;
        pVideoFormat->NumberOfSurfaces = 2;
        pSurf[1].ColorFormat = NvColorFormat_U8_V8;
        pSurf[0].Layout = NvRmSurfaceLayout_Blocklinear;
    }
#elif defined CAMERA_T114 || defined CAMERA_T148
    if (port == DZ_OUT_VIDEO)
        pSurf[0].Layout = NvRmSurfaceLayout_Tiled;
#endif

    // special attributes for different layout format in surface[0]
    if (pSurf[0].Layout == NvRmSurfaceLayout_Blocklinear)
    {
        pSurf[0].Kind = NvRmMemKind_Generic_16Bx2;
        pSurf[0].BlockHeightLog2 = 2;
    }

    //update the secondary fields
    NvU32 i;
    for (i = 1; i < pVideoFormat->NumberOfSurfaces; i++)
    {
        if (PixelFormat == FOURCC_YV12 || PixelFormat == FOURCC_NV12)
        {
            pSurf[i].Layout = pSurf[0].Layout;
        }

        if (pSurf[0].Layout == NvRmSurfaceLayout_Blocklinear)
        {
            pSurf[i].Kind = pSurf[0].Kind;
            pSurf[i].BlockHeightLog2 = pSurf[0].BlockHeightLog2;
        }
    }
    return e;
}

NvError TegraBufferConfig::GetDZRequirements(NvComponentBufferConfig *pStreamBuffCfg)
{
    NvMMNewBufferRequirementsInfo *pReq;
    NvError RetType = NvSuccess;

    for (NvU32 port = 0; port < DZ_OUT_NUMBER_OF_PORTS; port++)
    {
        if (! pStreamBuffCfg->outputPort[port].used)
        {
            continue; // Not using this port continue
        }
        pReq = &pStreamBuffCfg->outputPort[port].bufReq;
        NvMMVideoFormat *pVideoFormat = &pReq->format.videoFormat;
        NvRmSurface *pSurf = &pVideoFormat->SurfaceDescription[0];

        PopulateDZSurfaceFormatInfo(pReq, port);

        switch (port)
        {

                case DZ_OUT_PREVIEW:
                {
                    pReq->minBuffers = CAMERA_MAX_OUTPUT_BUFFERS_PREVIEW;
                    pReq->maxBuffers = CAMERA_MAX_OUTPUT_BUFFERS_PREVIEW;
                    break;
                }
                case DZ_OUT_STILL:
                {
                    // Set custom requirements for OutputStill
                    pReq->format.videoFormat.SurfaceRestriction |=
                            NVMM_SURFACE_RESTRICTION_WIDTH_16BYTE_ALIGNED |
                            NVMM_SURFACE_RESTRICTION_HEIGHT_16BYTE_ALIGNED |
                            NVMM_SURFACE_RESTRICTION_NEED_TO_CROP;

                    // set up default number of buffers
                    pReq->minBuffers = CAMERA_MAX_OUTPUT_BUFFERS_STILL;
                    pReq->maxBuffers = CAMERA_MAX_OUTPUT_BUFFERS_STILL;
                    break;
                }
                case DZ_OUT_VIDEO:
                {
#if !NV_CAMERA_V3
                    pReq->format.videoFormat.SurfaceRestriction |=
                            NVMM_SURFACE_RESTRICTION_WIDTH_16BYTE_ALIGNED |
                            NVMM_SURFACE_RESTRICTION_HEIGHT_16BYTE_ALIGNED |
                            NVMM_SURFACE_RESTRICTION_NEED_TO_CROP;
#endif
                    // set up default number of buffers
                    pReq->minBuffers = CAMERA_MAX_OUTPUT_BUFFERS_VIDEO;
                    pReq->maxBuffers = CAMERA_MAX_OUTPUT_BUFFERS_VIDEO;
                    break;
                }
                case DZ_OUT_THUMBNAIL:
                default:
                {
                    break;
                }
        }

    }
    return RetType;

}

NvError TegraBufferConfig::GetDzCfgFromRequirement(NvComponentBufferConfig *pStreamBuffCfg)
{
    NvMMNewBufferConfigurationInfo *pCfg;
    NvMMNewBufferRequirementsInfo *pReq;
    NvRmSurface *pSurfaceCfg = NULL;
    NvError err = NvSuccess;

    for ( NvU32 port = 0; port < DZ_OUT_NUMBER_OF_PORTS; port++)
    {
        if (! pStreamBuffCfg->outputPort[port].used)
        {
            continue; // Not using this port continue
        }
        pReq = &pStreamBuffCfg->outputPort[port].bufReq;
        pCfg = &pStreamBuffCfg->outputPort[port].currentBufCfg;

        switch(port)
        {
            case DZ_OUT_PREVIEW:
#if NV_CAMERA_V3
            case DZ_OUT_VIDEO:
            case DZ_OUT_STILL:
#endif
            {
                err = UpdateDzWithReqs(pReq,port);
                if (err != NvSuccess)
                {
                     NV_RETURN_FAIL(err);
                }
                err = DZPreviewBufferConfig(pReq,pCfg);
                if (err != NvSuccess)
                {
                     NV_RETURN_FAIL(err);
                }
                err = UpdateDzWithConfig(pCfg,port);
                if (err != NvSuccess)
                {
                     NV_RETURN_FAIL(err);
                }
                break;
            }
#if !NV_CAMERA_V3
            case DZ_OUT_VIDEO:

            case DZ_OUT_STILL:
            {
                // clear any configuration flags
                *m_DriverInfo.DZOutputPorts[port].pBufferConfigDone = NV_FALSE;
                err = UpdateDzWithReqs(pReq,port);
                if (err != NvSuccess)
                {
                     NV_RETURN_FAIL(err);
                }
                break;
            }
#endif
            case DZ_OUT_THUMBNAIL:
            default:
            {
                break;
            }
        }
    }

    // since we sent all BR for still and video we can now see if the configuration is back.
    for ( NvU32 port = 0; port < DZ_OUT_NUMBER_OF_PORTS; port++)
    {
        if (! pStreamBuffCfg->outputPort[port].used)
        {
            continue; // Not using this port continue
        }
        pReq = &pStreamBuffCfg->outputPort[port].bufReq;
        pCfg = &pStreamBuffCfg->outputPort[port].currentBufCfg;

        switch(port)
        {
            case DZ_OUT_PREVIEW:
#if NV_CAMERA_V3
            case DZ_OUT_VIDEO:
#endif
            {
                break;
            }
            case DZ_OUT_STILL:
#if !NV_CAMERA_V3
            case DZ_OUT_VIDEO:
#endif
            {
                // todo rework the condition + flag
                if (! *m_DriverInfo.DZOutputPorts[port].pBufferConfigDone)
                {
                    err =
                    WaitForCondition(*(m_DriverInfo.DZOutputPorts[port].pCondBufferConfigDoneCond),
                        *(m_DriverInfo.pLock));
                    if (err != NvSuccess)
                    {
                        NV_ERROR_MSG("Failed WaitForCondition()");
                    }
                }
                *pCfg = *m_DriverInfo.DZOutputPorts[port].pBuffCfg;
                ReplyToDZBufferConfig(port);
                break;
            }
            case DZ_OUT_THUMBNAIL:
            default:
            {
                break;
            }
        }
    }

    return err;
}

NvError TegraBufferConfig::ReplyToDZBufferConfig(NvU32 port)
{
    NvMMNewBufferConfigurationReplyInfo BufConfigReply;
    NvMMDigitalZoomStreamIndex index = GetDzOutStreamIndex(port);
    NvError err = NvSuccess;

    // send reply
    // TODO: what if we want to reject
    BufConfigReply.event = NvMMEvent_NewBufferConfigurationReply;
    BufConfigReply.structSize = sizeof(BufConfigReply);
    BufConfigReply.bAccepted = NV_TRUE;
    err = DZTransferBufferToBlockFunct(
        m_DriverInfo.DZ,
        index,
        NvMMBufferType_StreamEvent,
        sizeof(BufConfigReply),
        &BufConfigReply);
    return err;
}

NvError TegraBufferConfig::GetCaptureCfgAndReq(NvComponentBufferConfig *pStreamBuffCfg)
{
    NvMMNewBufferConfigurationInfo *pCfg;
    NvMMNewBufferRequirementsInfo *pReq;
    NvError err = NvSuccess;

    for ( NvU32 port = 0; port < CAMERA_OUT_NUMBER_OF_PORTS; port++)
    {
        if (! pStreamBuffCfg->outputPort[port].used)
        {
            continue; // Not using this port continue
        }

        pCfg = &pStreamBuffCfg->outputPort[port].currentBufCfg;
        pReq = &pStreamBuffCfg->outputPort[port].bufReq;

        if (*m_DriverInfo.CAMOutputPorts[port].pBufferConfigDone == NV_FALSE)
        {
            err = WaitForCondition(*(m_DriverInfo.CAMOutputPorts[port].pCondBufferConfigDoneCond),
                        *(m_DriverInfo.pLock));
            if (err != NvSuccess)
            {
                NV_ERROR_MSG("Failed WaitForCondition()");
            }
        }
        *pCfg = *m_DriverInfo.CAMOutputPorts[port].pBuffCfg;
        *pReq = *m_DriverInfo.CAMOutputPorts[port].pBuffReq;
    }
    return err;
}

NvError TegraBufferConfig::DZPreviewBufferConfig(
                            NvMMNewBufferRequirementsInfo *pReq,
                            NvMMNewBufferConfigurationInfo *pCfg)
{
    NvError err = NvSuccess;
    NvRmSurface *SurfaceCfg = NULL;
    NvU32 i = 0;
    NvU32 StreamIndex = NvMMDigitalZoomStreamIndex_OutputPreview;

    NvOsMemset(pCfg, 0, sizeof(NvMMNewBufferConfigurationInfo));

    pCfg->memorySpace = pReq->memorySpace;
    pCfg->numBuffers = pReq->minBuffers;
    pCfg->byteAlignment = pReq->byteAlignment;
    pCfg->endianness = pReq->endianness;
    pCfg->bufferSize = pReq->minBufferSize;
    pCfg->bPhysicalContiguousMemory = pReq->bPhysicalContiguousMemory;
    pCfg->event = NvMMEvent_NewBufferConfiguration;
    pCfg->structSize = sizeof(NvMMNewBufferConfigurationInfo);

    /* JPEG needs this while negotiating */
    // ^ old comment, not sure if it's relevant anymore in the preview case,
    // but we still want to be functionally equivalent to what OMX was doing
    // for now
    pCfg->bInSharedMemory = pReq->bInSharedMemory;

    SurfaceCfg = &pCfg->format.videoFormat.SurfaceDescription[0];

    // accept client's requirement
    pCfg->format = pReq->format;

    if (SurfaceCfg->Width == 0)
    {
        SurfaceCfg->Width = CAMERA_DEFAULT_PREVIEW_WIDTH;
    }

    if (SurfaceCfg->Height == 0)
    {
        SurfaceCfg->Height = CAMERA_DEFAULT_PREVIEW_HEIGHT;
    }

    // EPP doesn't support odd width and height dimensions
    // take care of such cases by making the width and height even(incrementing by one)
    // This applies only for YUV420 surfaces
    if(SurfaceCfg->ColorFormat == NvColorFormat_Y8)
    {
        if(SurfaceCfg->Width & 0x01)
        {
            SurfaceCfg->Width += 1;
        }
        if(SurfaceCfg->Height & 0x01)
        {
            SurfaceCfg->Height +=1;
        }
    }

    if (pCfg->format.videoFormat.SurfaceRestriction &
        NVMM_SURFACE_RESTRICTION_WIDTH_16BYTE_ALIGNED)
    {
        SurfaceCfg->Width = (SurfaceCfg->Width + 15) & ~15;
    }

    if (pCfg->format.videoFormat.SurfaceRestriction &
        NVMM_SURFACE_RESTRICTION_HEIGHT_16BYTE_ALIGNED)
    {
        SurfaceCfg->Height = (SurfaceCfg->Height + 15) & ~15;
    }

    return err;
}

NvError TegraBufferConfig::UpdateDzWithReqs(NvMMNewBufferRequirementsInfo *pReq, NvU32 port)
{
   NvMMDigitalZoomStreamIndex index = GetDzOutStreamIndex(port);
   NvError err = DZTransferBufferToBlockFunct(
            m_DriverInfo.DZ,
            index,
            NvMMBufferType_StreamEvent,
            sizeof(NvMMNewBufferRequirementsInfo),
            pReq);
   return err;
}

NvError TegraBufferConfig::UpdateDzWithConfig(NvMMNewBufferConfigurationInfo *pCfg,NvU32 port)
{
    NvMMDigitalZoomStreamIndex index = GetDzOutStreamIndex(port);
    NvError err =  DZTransferBufferToBlockFunct(
            m_DriverInfo.DZ,
            index,
            NvMMBufferType_StreamEvent,
            sizeof(NvMMNewBufferConfigurationInfo),
            pCfg);
    return err;
}


NvError TegraBufferAllocator::AllocateBuffer(NvBufferOutputLocation location,
                                             const NvMMNewBufferConfigurationInfo *bufferCfg,
                                             NvMMBuffer **buffer)
{
    NvError err = NvSuccess;
    NvBufferManagerComponent    component;
    NvU32                       port;

    component = location.GetComponent();
    port  = location.GetPort();

    *buffer = (NvMMBuffer *)NvOsAlloc(sizeof(NvMMBuffer));
    if (*buffer == NULL)
    {
        NV_RETURN_FAIL(NvError_InsufficientMemory);
    }

    if (MemProfilePrintEnabled)
    {
        NV_DEBUG_MSG("BufferType - Port: %d, Component: %d, SurfaceType: %d, Height: %d, Width: %d", port, component,
            bufferCfg->format.videoFormat.SurfaceDescription[0].ColorFormat,
            bufferCfg->format.videoFormat.SurfaceDescription[0].Height,
            bufferCfg->format.videoFormat.SurfaceDescription[0].Width);
    }

    switch (component)
    {
        case COMPONENT_CAMERA:
            err = InitializeCamOutputBuffer(bufferCfg,
                     GetCamOutStreamIndex(port, m_DriverInfo.isCamUSB),
                     *buffer, NV_TRUE);
            break;

        case COMPONENT_DZ:
#if NV_CAMERA_V3
            {
                err = InitializeANWBuffer(bufferCfg, *buffer, NV_TRUE);
            }
#else
            if (port == DZ_OUT_PREVIEW)
            {
                err = InitializeANWBuffer(bufferCfg, *buffer, NV_TRUE);
            }
            else
            {
                NvMMDigitalZoomStreamIndex index = GetDzOutStreamIndex(port);
                err = InitializeDZOutputBuffer(bufferCfg, index, *buffer, NV_TRUE);
            }
#endif
            break;

        case COMPONENT_HOST:
            //GetHostInputBufferFromReq(pStreamBuffCfg);
            break;

        default:
            NV_DEBUG_MSG("Error unknown component");
            break;
    }

    return err;
}

NvError TegraBufferAllocator::SetBufferCfg(NvBufferOutputLocation location,
                                             const NvMMNewBufferConfigurationInfo *bufferCfg,
                                             NvMMBuffer *pBuffer)
{
    NvError err = NvSuccess;
    NvBufferManagerComponent    component;
    NvU32                       index;

    component = location.GetComponent();

    if (pBuffer == NULL)
    {
        NV_RETURN_FAIL(NvError_BadParameter);
    }

    switch (component)
    {
        case COMPONENT_CAMERA:
            index = GetCamOutStreamIndex(location.GetPort(), m_DriverInfo.isCamUSB);
            err = InitializeCamOutputBuffer(bufferCfg, index, pBuffer, NV_FALSE);
            break;

        case COMPONENT_DZ:
            index = GetDzOutStreamIndex(location.GetPort());
            err = InitializeDZOutputBuffer(bufferCfg, index, pBuffer, NV_FALSE);
            break;

        case COMPONENT_HOST:
            //GetHostInputBufferFromReq(pStreamBuffCfg);
            break;

        default:
            NV_DEBUG_MSG("Error unknown component");
            break;
    }
    return err;
}


NvError TegraBufferAllocator::FreeBuffer(NvBufferOutputLocation location,
                                         NvMMBuffer *buffer)
{
    NvError err = NvSuccess;
    NvBufferManagerComponent    component;
    NvU32                       port;

    component = location.GetComponent();
    port  = location.GetPort();

    switch (component)
    {
        case COMPONENT_CAMERA:
            err = CleanupCamBuffer(buffer);
            break;

        case COMPONENT_DZ:
#if NV_CAMERA_V3
            if ((port == DZ_OUT_PREVIEW) || (port == DZ_OUT_VIDEO))
#else
            if (port == DZ_OUT_PREVIEW)
#endif
            {
                err = CleanupANWBuffer(buffer);
            }
            else
            {
                err = CleanupDZBuffer(buffer);
            }
            break;

        case COMPONENT_HOST:
            //GetHostInputBufferFromReq(pStreamBuffCfg);
            break;

        default:
            NV_DEBUG_MSG("Error unknown component" );
            break;
    }

    NvOsFree(buffer);
    return err;
}

TegraBufferAllocator::TegraBufferAllocator(NvCameraDriverInfo const &driverInfo)
    : NvBufferAllocator(driverInfo),
      hRm(NULL),
      h2d(NULL)
{
    char value[PROPERTY_VALUE_MAX];
    property_get("camera-memory-profile", value, "0");
    MemProfilePrintEnabled = atoi(value) == 1 ? NV_TRUE : NV_FALSE;
}

TegraBufferAllocator::~TegraBufferAllocator()
{

    if (h2d)
    {
        NvDdk2dClose(h2d);
        h2d = 0;
    }

    if (hRm)
    {
        NvRmClose(hRm);
        hRm = 0;
    }
}

NvError TegraBufferAllocator::Initialize()
{
    NvError err = NvSuccess;

    err = NvRmOpen(&hRm, 0);
    if (err != NvSuccess)
    {
        NV_RETURN_FAIL(err);
    }

    err = NvDdk2dOpen(hRm, NULL, &h2d);
    if (err != NvSuccess)
    {
        NV_RETURN_FAIL(err);
    }

    return err;
}

NvBool TegraBufferAllocator::RepurposeBuffers(NvBufferOutputLocation location,
                                              const NvMMNewBufferConfigurationInfo& originalCfg,
                                              const NvMMNewBufferConfigurationInfo& newCfg,
                                              NvOuputPortBuffers *buffers,
                                              NvU32 bufferCount)
{
    NvError err;

    if (bufferCount == 0)
        return NV_FALSE;

    // Create prototype NvMMBuffers (without allocating actual surface pointers)
    // for both the original and requested configurations

    NvMMBuffer origBufferProto, newBufferProto;
    NvU32 index;
    switch (location.GetComponent())
    {
        case COMPONENT_DZ:
        {
            // For now, we will not repurpose ANBs
#if NV_CAMERA_V3
            if ((location.GetPort() == DZ_OUT_PREVIEW) || (location.GetPort() == DZ_OUT_VIDEO))
#else
            if (location.GetPort() == DZ_OUT_PREVIEW)
#endif
                return NV_FALSE;

            index = GetDzOutStreamIndex(location.GetPort());

            err = InitializeDZOutputBuffer(&originalCfg, index, &origBufferProto, NV_FALSE);
            if (err != NvSuccess)
            {
                NV_ERROR_MSG("failed to initialize origBufferProto");
                return NV_FALSE;
            }
            err = InitializeDZOutputBuffer(&newCfg, index, &newBufferProto, NV_FALSE);
            if (err != NvSuccess)
            {
                NV_ERROR_MSG("failed to initialize newBufferProto");
                return NV_FALSE;
            }

            break;
        }

        case COMPONENT_CAMERA:
        {
            index = GetCamOutStreamIndex(location.GetPort(), m_DriverInfo.isCamUSB);

            err = InitializeCamOutputBuffer(&originalCfg, index, &origBufferProto, NV_FALSE);
            if (err != NvSuccess)
            {
                NV_ERROR_MSG("failed to initialize origBufferProto");
                return NV_FALSE;
            }
            err = InitializeCamOutputBuffer(&newCfg, index, &newBufferProto, NV_FALSE);
            if (err != NvSuccess)
            {
                NV_ERROR_MSG("failed to initialize newBufferProto");
                return NV_FALSE;
            }

            break;
        }

        default:
        {
            ALOGDD("REPURPOSE: %s does not handle component %d\n",
                            __FUNCTION__, location.GetComponent());
            return NV_FALSE;
        }
    }

    // Bail out if the existing surfaces cannot be repurposed to support the new configuration

    if (originalCfg.bInSharedMemory != newCfg.bInSharedMemory ||
        originalCfg.formatId != newCfg.formatId)
    {
        ALOGDD("REPURPOSE: repurpose failed because of flag delta (component = %d port %d)\n",
                        location.GetComponent(),
                        location.GetPort());
        return NV_FALSE;
    }

    const NvRmSurface& origSurf = origBufferProto.Payload.Surfaces.Surfaces[0];
    const NvRmSurface& newSurf  = newBufferProto.Payload.Surfaces.Surfaces[0];
    if (newSurf.Pitch * newSurf.Height > origSurf.Pitch * origSurf.Height)
    {
        ALOGDD("REPURPOSE: buffers are too small (%dx%d) to repurpose as %dx%d (component %d port %d)\n",
                        origSurf.Pitch,
                        origSurf.Height,
                        newSurf.Pitch,
                        newSurf.Height,
                        location.GetComponent(),
                        location.GetPort());
        return NV_FALSE;
    }


    // We can repurpose the buffers, so update all of them to reflect the new configuration

    for (NvU32 i = 0; i < bufferCount; i++)
    {
        if (buffers[i].bufferInUse)
        {
            ALOGDD("REPURPOSE: skipping repurpose of buffer %d\n", i);
            continue;
        }

        if (location.GetComponent() == COMPONENT_DZ)
            err = InitializeDZOutputBuffer(&newCfg, index, buffers[i].pBuffer, NV_FALSE);
        else
            err = InitializeCamOutputBuffer(&newCfg, index, buffers[i].pBuffer, NV_FALSE);

        if (err != NvSuccess)
        {
            NV_ERROR_MSG("failed to update existing buffer??");
            return NV_FALSE;
        }
    }

    ALOGDD("REPURPOSE: %d buffers on component %d port %d have been repurposed! (%dx%d -> %dx%d)\n",
                    bufferCount,
                    location.GetComponent(),
                    location.GetPort(),
                    origSurf.Width,
                    origSurf.Height,
                    newSurf.Width,
                    newSurf.Height);
    return NV_TRUE;
}

NvError TegraBufferAllocator::InitializeANWBuffer(const NvMMNewBufferConfigurationInfo *bufferCfg,
                                                  NvMMBuffer *pBuffer,
                                                  NvBool Allocate)
{
    NvOsMemset(pBuffer, 0, sizeof(NvMMBuffer));
    pBuffer->StructSize = sizeof(NvMMBuffer);
    pBuffer->BufferID = 0;
    pBuffer->PayloadType = NvMMPayloadType_SurfaceArray;
    return NvSuccess;
}

NvError TegraBufferAllocator::CleanupANWBuffer(NvMMBuffer *pBuffer)
{
    return NvSuccess;
}

NvError TegraBufferAllocator::CleanupDZBuffer(NvMMBuffer *pBuffer)
{
    NvMMUtilDestroySurfaces(&pBuffer->Payload.Surfaces);
    return NvSuccess;
}

NvU32 TegraBufferAllocator::ComputeSurfaceSizePostSwapDZ(NvRmSurface *pSurface)
{
    NvRmSurface SwapSurface;
    NvU32 SurfaceSize;

    NvOsMemset(&SwapSurface, 0, sizeof(NvRmSurface));

    SwapSurface.Width = pSurface->Height;
    SwapSurface.Height = pSurface->Width;
    SwapSurface.Layout = pSurface->Layout;
    SwapSurface.ColorFormat = pSurface->ColorFormat;
    SwapSurface.Offset = pSurface->Offset;

    NvRmSurfaceComputePitch(NULL, 0, &SwapSurface);
    SurfaceSize = NvRmSurfaceComputeSize(&SwapSurface);

    return SurfaceSize;
}

NvError TegraBufferAllocator::MemAllocDZ(
    NvRmMemHandle* phMem,
    NvU32 Size,
    NvU32 Alignment,
    NvU32 *pPhysicalAddress)
{
    NvError Err;
    NvRmMemHandle hMem = 0;
    static const NvRmHeap Heaps[] =
    {
      NvRmHeap_Camera,
      NvRmHeap_ExternalCarveOut,
      NvRmHeap_External,
    };

    Err = NvRmMemHandleAlloc(hRm, Heaps, NV_ARRAY_SIZE(Heaps), Alignment,
              NvOsMemAttribute_InnerWriteBack, Size, 0, 0, &hMem);
    if(Err != NvSuccess)
    {
      NV_ERROR_MSG("Memory Handle Alloc failed");
      goto fail;
    }

    if (MemProfilePrintEnabled)
    {
        NV_DEBUG_MSG("%s (DZ): %d, %d", MEM_PROFILE_TAG, Size, Alignment);
    }

    *phMem = hMem;
    *pPhysicalAddress = NvRmMemPin(hMem);

    return NvSuccess;

fail:
    NvRmMemHandleFree(hMem);
    return Err;
}

NvError TegraBufferAllocator::AllocateDZSurfaces(
    NvMMSurfaceDescriptor *pSurfaceDesc,
    NvU16 byteAlignment)
{
    NvError err;
    NvS32 ComponentSize, i, ComponentSizePostSwap;
    NvS32 surfCnt = pSurfaceDesc->SurfaceCount;
    NvU32 phAddr = 0;
    NvRmMemHandle hMem = 0;

    for (i=0; i<surfCnt; i++)
    {
        pSurfaceDesc->Surfaces[i].hMem = 0;
        pSurfaceDesc->Surfaces[i].Offset = 0;
    }

    for (i=0; i<surfCnt; i++)
    {
        if (byteAlignment == 0)
        {
            byteAlignment = NvRmSurfaceComputeAlignment(hRm,
                                            &pSurfaceDesc->Surfaces[i]);
        }
        ComponentSize = NvRmSurfaceComputeSize(&pSurfaceDesc->Surfaces[i]);
        ComponentSizePostSwap =
                    ComputeSurfaceSizePostSwapDZ(&pSurfaceDesc->Surfaces[i]);

        ComponentSize = ComponentSize > ComponentSizePostSwap ?
                                        ComponentSize : ComponentSizePostSwap;

        err = MemAllocDZ(&pSurfaceDesc->Surfaces[i].hMem,
                            ComponentSize, byteAlignment,
                            &pSurfaceDesc->PhysicalAddress[i]);
        if (err != NvSuccess)
        {
            NV_ERROR_MSG("Alloc Failed");
            goto fail;
        }
    }

    return NvSuccess;

fail:
    for (i=0; i<pSurfaceDesc->SurfaceCount; i++)
    {
        NvRmMemHandleFree(pSurfaceDesc->Surfaces[i].hMem);
    }
    return err;
}

NvError TegraBufferAllocator::InitializeDZOutputBuffer(const NvMMNewBufferConfigurationInfo *pBufCfg,
                                                       NvU32 StreamIndex,
                                                       NvMMBuffer *pBuffer,
                                                       NvBool Allocate)
{
    const NvRmSurface *SurfaceDescription = &pBufCfg->format.videoFormat.SurfaceDescription[0];
    NvU32 SurfaceRestriction = pBufCfg->format.videoFormat.SurfaceRestriction;
    NvU32 NumSurface = pBufCfg->format.videoFormat.NumberOfSurfaces;
    NvRect CropRect = {0, 0, 0, 0};
    NvError RetType = NvSuccess;

    if (Allocate)
    {
        // If we're not allocating the surface, but simply updating the other fields
        // (presumably for buffer repurposing), don't do this - it wipes out the
        // previously-allocated buffer pointers.
        NvOsMemset(pBuffer, 0, sizeof(NvMMBuffer));
    }

    pBuffer->StructSize = sizeof(NvMMBuffer);
    pBuffer->PayloadType = NvMMPayloadType_SurfaceArray;
    pBuffer->BufferID = NVMM_DZ_UNASSIGNED_BUFFER_ID;
    pBuffer->Payload.Surfaces.SurfaceCount = NumSurface;
    pBuffer->Payload.Surfaces.Empty = NV_TRUE;
    pBuffer->PayloadInfo.TimeStamp = 0;
    pBuffer->PayloadInfo.BufferFlags = 0;

    if (StreamIndex == NvMMDigitalZoomStreamIndex_OutputVideo)
    {
        if (SurfaceRestriction & NVMM_SURFACE_RESTRICTION_NEED_TO_CROP)
        {
            CropRect.left = 0;
            CropRect.top = 0;
            CropRect.right = SurfaceDescription[0].Width;
            CropRect.bottom = SurfaceDescription[0].Height;
        }
    }

    for (NvU32 i = 0; i < NumSurface; i++)
    {
        pBuffer->Payload.Surfaces.Surfaces[i].Width = SurfaceDescription[i].Width;
        pBuffer->Payload.Surfaces.Surfaces[i].Height= SurfaceDescription[i].Height;
        pBuffer->Payload.Surfaces.Surfaces[i].ColorFormat = SurfaceDescription[i].ColorFormat;
        pBuffer->Payload.Surfaces.Surfaces[i].Layout = SurfaceDescription[i].Layout;
        pBuffer->Payload.Surfaces.Surfaces[i].Pitch = SurfaceDescription[i].Pitch;

        //NV12 special, no harm for other formats
        pBuffer->Payload.Surfaces.Surfaces[i].Kind = SurfaceDescription[i].Kind;
        pBuffer->Payload.Surfaces.Surfaces[i].BlockHeightLog2 = SurfaceDescription[i].BlockHeightLog2;
    }

    //16 Byte Width Alignment
    if (SurfaceRestriction &
                NVMM_SURFACE_RESTRICTION_WIDTH_16BYTE_ALIGNED)
    {
        pBuffer->Payload.Surfaces.Surfaces[0].Width =
                (pBuffer->Payload.Surfaces.Surfaces[0].Width + 15) & ~15;
        if (NumSurface == 3)
        {
            pBuffer->Payload.Surfaces.Surfaces[1].Width =
                    (pBuffer->Payload.Surfaces.Surfaces[0].Width)/2 ;
            pBuffer->Payload.Surfaces.Surfaces[2].Width =
                    (pBuffer->Payload.Surfaces.Surfaces[0].Width)/2 ;
        }
        else if (NumSurface == 2)
        {
            pBuffer->Payload.Surfaces.Surfaces[1].Width =
                    (pBuffer->Payload.Surfaces.Surfaces[0].Width)/2 ;
        }
    }

    //16 Byte Height Alignment
    if (SurfaceRestriction &
                NVMM_SURFACE_RESTRICTION_HEIGHT_16BYTE_ALIGNED)
    {
        pBuffer->Payload.Surfaces.Surfaces[0].Height =
                (pBuffer->Payload.Surfaces.Surfaces[0].Height + 15) & ~15;
        if (NumSurface == 3)
        {
            pBuffer->Payload.Surfaces.Surfaces[1].Height =
                    (pBuffer->Payload.Surfaces.Surfaces[0].Height)/2 ;
            pBuffer->Payload.Surfaces.Surfaces[2].Height =
                    (pBuffer->Payload.Surfaces.Surfaces[0].Height)/2 ;
        }
        else if (NumSurface == 2)
        {
            pBuffer->Payload.Surfaces.Surfaces[1].Height =
                    (pBuffer->Payload.Surfaces.Surfaces[0].Height)/2 ;
        }
    }

    if (Allocate)
    {
        RetType = AllocateDZSurfaces(&pBuffer->Payload.Surfaces,
                    pBufCfg->byteAlignment);

        if (RetType != NvSuccess)
        {
            NV_ERROR_MSG("Alloc Failed");
            goto fail;
        }
    }

    if ((StreamIndex == NvMMDigitalZoomStreamIndex_OutputVideo) &&
        (SurfaceRestriction & NVMM_SURFACE_RESTRICTION_NEED_TO_CROP))
    {
        NvRect *DestCropRect = &pBuffer->Payload.Surfaces.CropRect;

        DestCropRect->left = CropRect.left;
        DestCropRect->top = CropRect.top;
        DestCropRect->right = CropRect.right;
        DestCropRect->bottom = CropRect.bottom;

        if ((NvS32)pBuffer->Payload.Surfaces.Surfaces[0].Height > DestCropRect->bottom)
        {
            NvU8 *pData = NULL;
            NvS32 i = 0;
            NvU32 SurfaceSizeToFill = pBuffer->Payload.Surfaces.Surfaces[0].Width *
                (pBuffer->Payload.Surfaces.Surfaces[0].Height - DestCropRect->bottom);

            pData = (NvU8 *)NvOsAlloc(SurfaceSizeToFill);
            if(pData)
            {
                NvOsMemset(pData, 0x10, SurfaceSizeToFill); // Fill with black color
            }
            else
            {
                NV_ERROR_MSG("Alloc Failed");
                goto fail;
            }

            for (i = 0; i < pBuffer->Payload.Surfaces.SurfaceCount; i++)
            {
                NvU32 Bottom = DestCropRect->bottom;
                if (i)
                {
                    Bottom >>= 1;
                    NvOsMemset(pData, 0x80, SurfaceSizeToFill); // Fill with black color
                }

                if (pBuffer->Payload.Surfaces.Surfaces[i].hMem)
                {
                    NvRmSurfaceWrite(&pBuffer->Payload.Surfaces.Surfaces[i],
                                    0, Bottom,
                                    pBuffer->Payload.Surfaces.Surfaces[i].Width,
                                    pBuffer->Payload.Surfaces.Surfaces[i].Height - Bottom,
                                    pData);
                }
            }
            NvOsFree(pData);
        }
    }

    return RetType;

fail:

    if (Allocate)
    {
        NvMMUtilDestroySurfaces(&pBuffer->Payload.Surfaces);
    }
    NvOsMemset(pBuffer, 0, sizeof(NvMMBuffer));
    return NvError_InsufficientMemory;
}

NvError TegraBufferAllocator::InitializeCamOutputBuffer(const NvMMNewBufferConfigurationInfo *pBufCfg,
                                                        NvU32 StreamIndex,
                                                        NvMMBuffer *pBuffer,
                                                        NvBool Allocate)
{
    NvError e = NvSuccess;
    const NvRmSurface *pSurfaceReq = &pBufCfg->format.videoFormat.SurfaceDescription[0];
    const NvU32 NumSurface = pBufCfg->format.videoFormat.NumberOfSurfaces;
    NvU32 freeBufInd = 0;
    NvMMCameraStreamIndex StreamIndexOutput =  GetCamOutStreamIndex(CAMERA_OUT_CAPTURE,
                                                                    m_DriverInfo.isCamUSB);
    // Initialize attributes that input and output buffers share
    if (Allocate)
    {
        // If we're not allocating the surface, but simply updating the other fields
        // (presumably for buffer repurposing), don't do this - it wipes out the
        // previously-allocated buffer pointers.
        NvOsMemset(pBuffer, 0, sizeof(NvMMBuffer));
    }
    pBuffer->StructSize = sizeof(NvMMBuffer);

    pBuffer->BufferID                   = NVMM_CAM_UNASSIGNED_BUFFER_ID;
    pBuffer->PayloadInfo.TimeStamp      = 0;
    pBuffer->PayloadInfo.BufferFlags    = 0;
    pBuffer->PayloadType                = NvMMPayloadType_SurfaceArray;
    pBuffer->Payload.Surfaces.Empty     = NV_TRUE;
    pBuffer->Payload.Surfaces.SurfaceCount = NumSurface;

    for (NvU32 i = 0; i < NumSurface; i++)
    {
        pBuffer->Payload.Surfaces.Surfaces[i].Width = pSurfaceReq[i].Width;
        pBuffer->Payload.Surfaces.Surfaces[i].Height= pSurfaceReq[i].Height;
        pBuffer->Payload.Surfaces.Surfaces[i].ColorFormat = pSurfaceReq[i].ColorFormat;
        pBuffer->Payload.Surfaces.Surfaces[i].Layout = pSurfaceReq[i].Layout;
        pBuffer->Payload.Surfaces.Surfaces[i].Pitch = pSurfaceReq[i].Pitch;

        //NV12 special, no harm for other formats
        pBuffer->Payload.Surfaces.Surfaces[i].Kind = pSurfaceReq[i].Kind;
        pBuffer->Payload.Surfaces.Surfaces[i].BlockHeightLog2 = pSurfaceReq[i].BlockHeightLog2;
    }

    if (Allocate)
    {
        e = CameraBlockAllocSurfaces(&pBuffer->Payload.Surfaces,
                    (StreamIndex == StreamIndexOutput));

        if (e != NvSuccess)
        {
            pBuffer->Payload.Surfaces.SurfaceCount = 0;
            goto fail;
        }
    }

    if (Allocate)
    {
        NV_CHECK_ERROR_CLEANUP(CameraAllocateMetadata(
                &pBuffer->PayloadInfo.BufferMetaData.CameraBufferMetadata.EXIFInfoMemHandle,
                sizeof(NvMMEXIFInfo)));
        NV_CHECK_ERROR_CLEANUP(CameraAllocateMetadata(
                &pBuffer->PayloadInfo.BufferMetaData.CameraBufferMetadata.MakerNoteExtensionHandle,
                sizeof(NvCameraMakerNoteExtension)));
    }

    pBuffer->PayloadInfo.BufferMetaDataType = NvMMBufferMetadataType_Camera;

    return e;

fail:

    if (Allocate)
    {
        CleanupCamBuffer(pBuffer);
    }
    return NvError_InsufficientMemory;
}

NvError TegraBufferAllocator::CameraBlockAllocSurfaces(NvMMSurfaceDescriptor *pSurfaceDesc,
                                                       NvBool StillOutputAlignmentNeeded)
{
    NvError err;
    NvS32 size, align, i;
    NvS32 padHeight = 0;
    NvU8 *pData = NULL;
    NvU32 SurfaceSizeToFill;
    #define MIN_SURF_ALIGN      1024
    #define MIN_HEIGHT_ALIGN      16
    #define MIN_PITCH_ALIGN       64
    #define MIN_TEX_SLICE_ALIGN 1024

    for (i = 0; i < pSurfaceDesc->SurfaceCount; i++)
    {
        pSurfaceDesc->Surfaces[i].hMem = 0;
        pSurfaceDesc->Surfaces[i].Offset = 0;
    }

    for (i = 0; i < pSurfaceDesc->SurfaceCount; i++)
    {
        align =
            NvRmSurfaceComputeAlignment(hRm,
                                        &pSurfaceDesc->Surfaces[i]);

        if(StillOutputAlignmentNeeded)
        {
            if (align < MIN_SURF_ALIGN)
            {
                align = MIN_SURF_ALIGN;
            }

            size = pSurfaceDesc->Surfaces[i].Pitch;

            if (size & (MIN_PITCH_ALIGN - 1))
            {
                pSurfaceDesc->Surfaces[i].Pitch =
                    MIN_PITCH_ALIGN + (size & ~(MIN_PITCH_ALIGN - 1));
            }

            size = pSurfaceDesc->Surfaces[i].Height;
            padHeight = size;

            if (size & (MIN_HEIGHT_ALIGN - 1))
            {
                size =
                    MIN_HEIGHT_ALIGN + (size & ~(MIN_HEIGHT_ALIGN - 1));
            }
            padHeight = size - padHeight;

            size *= pSurfaceDesc->Surfaces[i].Pitch;
        }
        else
        {
           size = NvRmSurfaceComputeSize(&pSurfaceDesc->Surfaces[i]);
        }

        {
            NvBool ddk2dUseSysmem = NV_FALSE;
            NvS32 ddk2dAlign;
            NvS32 ddk2dPitch;
            NvS32 ddk2dSize;
            err = NvDdk2dSurfaceComputeBufferParams(h2d,
                                                    &pSurfaceDesc->Surfaces[i],
                                                    &ddk2dUseSysmem,
                                                    &ddk2dAlign,
                                                    &ddk2dPitch,
                                                    &ddk2dSize);

            if (err != NvSuccess)
                goto fail;

            size = NV_MAX(size, ddk2dSize);
            align = NV_MAX(align, ddk2dAlign);

            pSurfaceDesc->Surfaces[i].Pitch =
                NV_MAX((NvS32)pSurfaceDesc->Surfaces[i].Pitch, ddk2dPitch);
        }

        err = CameraBlockMemAlloc(&pSurfaceDesc->Surfaces[i].hMem,
                                  size, align,
                                  &pSurfaceDesc->PhysicalAddress[i]);
        if (err != NvSuccess)
        {
             goto fail;
        }

        if (padHeight > 0)
        {
            // Initialize the surface bottom padding part to black color to
            // avoid encoding artifacts
            SurfaceSizeToFill = pSurfaceDesc->Surfaces[i].Width * padHeight;
            pData = (NvU8*)NvOsAlloc(SurfaceSizeToFill);
            if(pData && i == 0)
            {
                // Y: Fill with black color
                NvOsMemset(pData, 0x10, SurfaceSizeToFill);
            }
            if (i && pData)
            {
                // UV: Fill with black color
                NvOsMemset(pData, 0x80, SurfaceSizeToFill);
            }
            if (pData)
            {
                // NvRmSurfaceWrite won't let us write to the padding pixels
                // unless we tell it the full, padded height.
                pSurfaceDesc->Surfaces[i].Height += padHeight;
                NvRmSurfaceWrite(
                    &pSurfaceDesc->Surfaces[i],
                    0,
                    pSurfaceDesc->Surfaces[i].Height - padHeight,
                    pSurfaceDesc->Surfaces[i].Width,
                    padHeight,
                    pData);
                // the rest of this block expects the surface height to be
                // the unpadded value, so we need to change it back.
                pSurfaceDesc->Surfaces[i].Height -= padHeight;

                NvOsFree(pData);
                pData = NULL;
            }
        }
    }
    return NvSuccess;
 fail:
    for (i = 0; i < pSurfaceDesc->SurfaceCount - 1; i++) {
        NvRmMemHandleFree(pSurfaceDesc->Surfaces[i].hMem);
    }
    return err;
}

NvError TegraBufferAllocator::CameraBlockMemAlloc(NvRmMemHandle* phMem,
                                                  NvU32 Size,
                                                  NvU32 Alignment,
                                                  NvU32 *PhysicalAddress)
{
    NvError e;
    NvRmMemHandle hMem = 0;
    static const NvRmHeap Heaps[] =
    {
       NvRmHeap_Camera,
       NvRmHeap_ExternalCarveOut,
       NvRmHeap_External,
    };

    e = NvRmMemHandleAlloc(hRm, Heaps, NV_ARRAY_SIZE(Heaps), Alignment,
            NvOsMemAttribute_InnerWriteBack, Size, 0, 0, &hMem);

    if (e != NvSuccess)
    {
        goto fail;
    }

    if (MemProfilePrintEnabled)
    {
        NV_DEBUG_MSG("%s (Camera): %d, %d", MEM_PROFILE_TAG, Size, Alignment);
    }

    *phMem = hMem;
    *PhysicalAddress = NvRmMemPin(hMem);

    return NvSuccess;

fail:
    NvRmMemHandleFree(hMem);
    return e;
}

NvError TegraBufferAllocator::CameraAllocateMetadata(NvRmMemHandle *phMem, NvU32 size)
{
    NvError status = NvSuccess;

    status = NvRmMemHandleAlloc(hRm, NULL, 0, 32, NvOsMemAttribute_InnerWriteBack,
                 size, 0, 0, phMem);
    if (status != NvSuccess)
    {
        NV_RETURN_FAIL(status);
    }
    if (MemProfilePrintEnabled)
    {
        NV_DEBUG_MSG("%s (Metadata): 0, 32", MEM_PROFILE_TAG);
    }

    return status;
}

NvError TegraBufferAllocator::CleanupCamBuffer(NvMMBuffer *pBuffer)
{
    NvError err = NvSuccess;

    if (pBuffer != NULL)
    {
        if ((pBuffer->PayloadType == NvMMPayloadType_SurfaceArray) &&
            (pBuffer->Payload.Surfaces.SurfaceCount > 0))
        {
            // Destroy rm surfaces
            NvMMUtilDestroySurfaces(&pBuffer->Payload.Surfaces);
        }

        if ((pBuffer->PayloadInfo.BufferMetaDataType == NvMMBufferMetadataType_Camera) &&
            (pBuffer->PayloadInfo.BufferMetaData.CameraBufferMetadata.EXIFInfoMemHandle))
        {
                NvRmMemHandleFree(
                        pBuffer->PayloadInfo.BufferMetaData.CameraBufferMetadata.EXIFInfoMemHandle);
        }

        if ((pBuffer->PayloadInfo.BufferMetaDataType == NvMMBufferMetadataType_Camera) &&
            (pBuffer->PayloadInfo.BufferMetaData.CameraBufferMetadata.MakerNoteExtensionHandle))
        {
            NvRmMemHandleFree(
                pBuffer->PayloadInfo.BufferMetaData.CameraBufferMetadata.MakerNoteExtensionHandle);
        }
   }

    return err;
}


TegraBufferHandler::TegraBufferHandler(NvCameraDriverInfo const &driverInfo)
                                       : NvBufferHandler(driverInfo)
{

    DZTransferBufferToBlockFunct =
                driverInfo.DZ->GetTransferBufferFunction(driverInfo.DZ,0,NULL);
    CamTransferBufferToBlockFunct =
                driverInfo.Cam->GetTransferBufferFunction(driverInfo.Cam,0,NULL);
}


NvError TegraBufferHandler::GiveBufferToComponent(NvBufferOutputLocation location,
                                                  NvMMBuffer *buffer)
{
    NvError err = NvSuccess;
    NvBufferManagerComponent    component;
    NvU32                       port;
    // check if a configuration already exists for this location
    // throw error if it does since we do not know what to do

    component = location.GetComponent();
    port  = location.GetPort();

    switch (component)
    {
        case COMPONENT_CAMERA:
            err = SendBufferToCam(buffer, port);
            break;
        case COMPONENT_DZ:
            err = SendBufferToDZ(buffer, port);
            break;
        case COMPONENT_HOST:
            //Give Buffers to host
            break;
        default:
            NV_DEBUG_MSG("Error unknown component" );
            break;
    }

    return err;
}

NvError TegraBufferHandler::ReturnBuffersToManager(NvBufferOutputLocation location)
{
    NvError err = NvSuccess;
    NvBufferManagerComponent    component;
    NvU32                       port;
    // check if a configuration already exists for this location
    // throw error if it does since we do not know what to do

    component = location.GetComponent();
    port  = location.GetPort();

    switch (component)
    {
        case COMPONENT_CAMERA:
            err = ReturnBuffersFromCam(port);
            break;
        case COMPONENT_DZ:
            err = ReturnBuffersFromDZ(port);
            break;
        case COMPONENT_HOST:
            //GetHostInputBufferFromReq(pStreamBuffCfg);
            break;
        default:
            NV_DEBUG_MSG("Error unknown component" );
            break;
    }

    return err;
}

NvError TegraBufferHandler::SendBufferToDZ(NvMMBuffer *buffer,NvU32 port)
{
    NvMMDigitalZoomStreamIndex index = GetDzOutStreamIndex(port);
    NvError err =  DZTransferBufferToBlockFunct(
                                m_DriverInfo.DZ,
                                index,
                                NvMMBufferType_Payload,
                                sizeof(NvMMBuffer),
                                buffer);
    return err;
}

NvError TegraBufferHandler::SendBufferToCam(NvMMBuffer *buffer,NvU32 port)
{
    NvMMCameraStreamIndex index = GetCamOutStreamIndex(port, m_DriverInfo.isCamUSB);
    NvError err =  CamTransferBufferToBlockFunct(
                                m_DriverInfo.Cam,
                                index,
                                NvMMBufferType_Payload,
                                sizeof(NvMMBuffer),
                                buffer);
    return err;
}


NvError TegraBufferHandler::ReturnBuffersFromCam(NvU32 port)
{
    NvMMCameraStreamIndex index = GetCamOutStreamIndex(port, m_DriverInfo.isCamUSB);
    NvMMCameraStreamIndex StreamIndexPreview = GetCamOutStreamIndex(CAMERA_OUT_PREVIEW,
                                                                    m_DriverInfo.isCamUSB);
    NvMMCameraStreamIndex StreamIndexOutput = GetCamOutStreamIndex(CAMERA_OUT_CAPTURE,
                                                                    m_DriverInfo.isCamUSB);
    NvMMDigitalZoomStreamIndex indexDz;
    NvError err = NvSuccess;

    m_DriverInfo.Cam->AbortBuffers(m_DriverInfo.Cam, index);
    err = WaitForCondition(*(m_DriverInfo.CAMOutputPorts[port].pCondBlockAbortDone),
                           *(m_DriverInfo.pLock));
    if (err != NvSuccess)
    {
        NV_ERROR_MSG("Failed WaitForCondition()");
    }
    // now abort the dz side input buffers
    if (index == StreamIndexPreview)
    {
        m_DriverInfo.DZ->AbortBuffers(m_DriverInfo.DZ, NvMMDigitalZoomStreamIndex_InputPreview);
        err = WaitForCondition(*(m_DriverInfo.DZInputPorts[DZ_IN_PREVIEW].pCondBlockAbortDone),
                               *(m_DriverInfo.pLock));
    }
    if (index == StreamIndexOutput)
    {
        m_DriverInfo.Cam->AbortBuffers(m_DriverInfo.Cam, index);
        err = WaitForCondition(*(m_DriverInfo.CAMOutputPorts[port].pCondBlockAbortDone),
                               *(m_DriverInfo.pLock));
        if (err != NvSuccess)
        {
            NV_ERROR_MSG("Failed WaitForCondition()");
        }

        m_DriverInfo.DZ->AbortBuffers(m_DriverInfo.DZ, NvMMDigitalZoomStreamIndex_Input);
        err = WaitForCondition(*(m_DriverInfo.DZInputPorts[DZ_IN_STILL].pCondBlockAbortDone),
                               *(m_DriverInfo.pLock));
    }
    if (err != NvSuccess)
    {
        NV_ERROR_MSG("Failed WaitForCondition()");
    }

    return err;
}

NvError TegraBufferHandler::ReturnBuffersFromDZ(NvU32 port)
{
    NvMMDigitalZoomStreamIndex index = GetDzOutStreamIndex(port);
    NvError err = NvSuccess;
    m_DriverInfo.DZ->AbortBuffers(m_DriverInfo.DZ, index);
    err = WaitForCondition(*(m_DriverInfo.DZOutputPorts[port].pCondBlockAbortDone),
                           *(m_DriverInfo.pLock));
    if (err != NvSuccess)
    {
        NV_ERROR_MSG("Failed WaitForCondition()");
    }
    return err;
}
