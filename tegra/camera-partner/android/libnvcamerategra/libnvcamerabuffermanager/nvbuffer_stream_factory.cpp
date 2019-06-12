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

#include "nvbuffer_stream_factory.h"
#include "nvbuffer_hw_allocator_tegra.h"

#define CAMERA_DEFAULT_OUTPUT_WIDTH            176
#define CAMERA_DEFAULT_OUTPUT_HEIGHT           144
#define CAMERA_DEFAULT_OUTPUT_BUFFERS          4
#define MAX_BUFFER_SIZE 1024



NvBufferStreamFactory* NvBufferStreamFactory::m_instance[] = {0, 0, 0, 0, 0};

NvBool BufferConfigurationsEqual(NvOuputPortConfig *pOldPortCfg, NvOuputPortConfig *pNewPortCfg)
{
    NvMMNewBufferConfigurationInfo *pOldCfg = &pOldPortCfg->currentBufCfg;
    NvMMNewBufferConfigurationInfo *pNewCfg = &pNewPortCfg->currentBufCfg;
    return BufferConfigurationsEqual(pOldCfg, pNewCfg);
}

NvBool BufferConfigurationsEqual(NvMMNewBufferConfigurationInfo *pOldCfg,
                                 NvMMNewBufferConfigurationInfo *pNewCfg)
{
    NvBool retVal = NV_FALSE;
    NvRmSurface *pOldSurface = &pOldCfg->format.videoFormat.SurfaceDescription[0];
    NvRmSurface *pNewSurface = &pNewCfg->format.videoFormat.SurfaceDescription[0];
//check cfg
   if (!(pNewCfg->byteAlignment              == pOldCfg->byteAlignment               &&
         pNewCfg->bPhysicalContiguousMemory  == pOldCfg->bPhysicalContiguousMemory   &&
         pNewCfg->bInSharedMemory            == pOldCfg->bInSharedMemory             &&
         pNewCfg->memorySpace                == pOldCfg->memorySpace                 &&
         pNewCfg->endianness                 == pOldCfg->endianness                  &&
         pNewCfg->formatId                   == pOldCfg->formatId))
       {
           return NV_FALSE;
       }

// check surface
   if (!(pNewSurface->Width                  == pOldSurface->Width                   &&
         pNewSurface->Height                 == pOldSurface->Height                  &&
         pNewSurface->ColorFormat            == pOldSurface->ColorFormat             &&
         pNewSurface->Layout                 == pOldSurface->Layout                  &&
         pNewSurface->Pitch                  == pOldSurface->Pitch))
       {
           return NV_FALSE;
       }

    return NV_TRUE;
}


NvBufferStreamFactory::NvBufferStreamFactory()
{
    NvOsMemset(&m_DriverInfo,0 , sizeof(m_DriverInfo));
    m_DriverInitialized = NV_FALSE;
    m_BufferCfg = NULL;
}


NvBufferStreamFactory::~NvBufferStreamFactory()
{

}

NvBufferStreamFactory* NvBufferStreamFactory::Instance(
    NvU32 id,
    NvCameraDriverInfo const &driverInfo)
{
    if (id >= MAX_NUM_SENSORS)
        return NULL;

    if(m_instance[id] == 0)
    {
        m_instance[id] = new NvBufferStreamFactory();
        m_instance[id]->m_DriverInfo = driverInfo;
        m_instance[id]->m_BufferCfg = new TegraBufferConfig(driverInfo);
        m_instance[id]->m_DriverInitialized = NV_TRUE;
    }

    return m_instance[id];
}

void NvBufferStreamFactory::Release(NvU32 id)
{
    if (id >= MAX_NUM_SENSORS)
        return;

    if (m_instance[id])
    {
        if (m_instance[id]->m_DriverInitialized == NV_TRUE)
        {
            delete m_instance[id]->m_BufferCfg;
            m_instance[id]->m_BufferCfg = NULL;
        }
        delete m_instance[id];
    }

    m_instance[id] = NULL;
}

NvError NvBufferStreamFactory::InvalidateDriverInfo(NvBufferStream *pStream)
{
    if (m_DriverInitialized)
    {
        delete m_BufferCfg;
        m_BufferCfg = NULL;
        m_DriverInitialized = NV_FALSE;
    }
    if (pStream->m_Initialized)
    {
        delete pStream->m_pAllocator;
        delete pStream->m_pBufferHandler;
        pStream->m_pAllocator = NULL;
        pStream->m_pBufferHandler = NULL;
        pStream->m_Initialized = NV_FALSE;
    }
    return NvSuccess;
}

NvError NvBufferStreamFactory::ReInitializeDriverInfo(NvBufferStream *pStream,
                                                      NvCameraDriverInfo const &driverInfo)
{
    NvError err = NvSuccess;

    if (m_DriverInitialized || pStream->m_Initialized)
    {
         NV_RETURN_FAIL(NvError_InvalidState);
    }

    m_DriverInfo = driverInfo;
    m_BufferCfg = new TegraBufferConfig(driverInfo);
    m_DriverInitialized = NV_TRUE;

    pStream->m_pAllocator  = new TegraBufferAllocator(m_DriverInfo);
    err = pStream->m_pAllocator->Initialize();
    if (err != NvSuccess)
    {
        delete pStream->m_pAllocator;
        NV_RETURN_FAIL(err);
    }
    pStream->m_pBufferHandler  = new TegraBufferHandler(m_DriverInfo);
    pStream->m_Initialized = NV_TRUE;

    return err;
}



NvError NvBufferStreamFactory::InitializeStream(NvBufferStream *stream,
                                                NvBufferStreamType streamType,
                                                NvStreamRequest const &streamReq)
{
    NvStreamBufferConfig  NewStreamBufferCfgs;
    NvError err = NvSuccess;

    if (m_DriverInitialized == NV_FALSE)
    {
        NV_RETURN_FAIL(NvError_InvalidState);
    }

    // if an initialized stream is passed it means that we should just reparse
    // any user requests get the new buffer cfg's
    if (stream->m_Initialized && stream->m_StreamType == streamType)
    {
        err = ReInitializeStream(stream, streamReq);
        if(err != NvSuccess)
        {
            NV_RETURN_FAIL(err);
        }
        return err;
    }

    switch (streamType)
    {
        case CAMERA_STANDARD_CAPTURE:
        {
            err = ConfigureStandardCapture(&NewStreamBufferCfgs, streamReq);
            if(err != NvSuccess)
            {
                NV_RETURN_FAIL(err);
            }
            break;
        }

        default:
        {
            NV_RETURN_FAIL(NvError_InvalidState);
            break;
        }
    }

    // Since this is the first time through, set the "original" configuration from
    // the current values
    for (NvU32 port = 0; port < NvBufferOutputLocation::GetNumberOfOuputPorts(COMPONENT_DZ); port++)
    {
        NvOuputPortConfig& cfg = NewStreamBufferCfgs.component[COMPONENT_DZ].outputPort[port];
        if (cfg.used)
        {
            cfg.originalBufCfg = cfg.currentBufCfg;
        }
    }
    for (NvU32 port = 0; port < NvBufferOutputLocation::GetNumberOfOuputPorts(COMPONENT_CAMERA); port++)
    {
        NvOuputPortConfig& cfg = NewStreamBufferCfgs.component[COMPONENT_CAMERA].outputPort[port];
        if (cfg.used)
        {
            cfg.originalBufCfg = cfg.currentBufCfg;
        }
    }

    if(!stream->m_pAllocator)
    {
        stream->m_pAllocator  = new TegraBufferAllocator(m_DriverInfo);
        err = stream->m_pAllocator->Initialize();
        if (err != NvSuccess)
        {
            delete stream->m_pAllocator;
            stream->m_pAllocator = NULL;
            NV_RETURN_FAIL(err);
        }
    }
    if (!stream->m_pBufferHandler)
    {
        stream->m_pBufferHandler  = new TegraBufferHandler(m_DriverInfo);
    }

    stream->m_BufferStreamCfg = NewStreamBufferCfgs;
    stream->m_Initialized = NV_TRUE;
    stream->m_StreamType = streamType;
    return err;
}

NvError NvBufferStreamFactory::ReInitializeStream(NvBufferStream *stream,
                                                  NvStreamRequest const &streamReq)
{
    NvError err = NvSuccess;
    NvStreamBufferConfig NewStreamBufferCfgs;
    NvU32 numberComponents = NvBufferOutputLocation::GetNumberOfComponents();
    NvU32 numberOutputDZPorts =
            NvBufferOutputLocation::GetNumberOfOuputPorts(COMPONENT_DZ);

    // copy configs from stream
    NewStreamBufferCfgs = stream->m_BufferStreamCfg;

    // disable all ports we will enable only the ones that are user set
    // also make sure we clear any buffer stuff
    m_BufferCfg->ConfigureDrivers();

    for ( NvU32 i = 0; i < numberComponents; i++)
    {
        NvU32 numberOutputPorts =
            NvBufferOutputLocation::GetNumberOfOuputPorts((NvBufferManagerComponent)i);
        for (NvU32 j = 0; j < numberOutputPorts; j ++)
        {
            NewStreamBufferCfgs.component[i].outputPort[j].used = NV_TRUE;
            NewStreamBufferCfgs.component[i].outputPort[j].totalBuffersAllocated = 0;
            NvOsMemset(NewStreamBufferCfgs.component[i].outputPort[j].buffer,
                       0,
                       sizeof(NewStreamBufferCfgs.component[i].outputPort[j].buffer));
            NewStreamBufferCfgs.component[i].outputPort[j].buffer[i].bufferAllocated = NV_FALSE;
        }
    }

    // Parse new user settings
    err = SetUserGeneratedSettings(COMPONENT_DZ,
                                   &NewStreamBufferCfgs.component[COMPONENT_DZ],
                                   streamReq, NV_TRUE);

    if (err != NvSuccess)
    {
        NV_RETURN_FAIL(err);
    }

    // Save any updated buffer Req's back
    for (NvU32 j = 0; j < numberOutputDZPorts; j ++)
    {
        stream->m_BufferStreamCfg.component[COMPONENT_DZ].outputPort[j].bufReq =
            NewStreamBufferCfgs.component[COMPONENT_DZ].outputPort[j].bufReq;
    }

    // Call into DZ with requirements, get the output buffer configurations
    // at this point we have all of the output requirements, call into driver to get the configs
    // and set the input requirements.
    err = m_BufferCfg->GetOutputConfiguration(COMPONENT_DZ,
                                              &NewStreamBufferCfgs.component[COMPONENT_DZ]);
    if (err != NvSuccess)
    {
        NV_RETURN_FAIL(err);
    }

    NvBufferOutputLocation location;
    location.SetLocation(COMPONENT_CAMERA,CAMERA_OUT_PREVIEW);
    // since we never know if cam<-> dz buffers will change lets abort all of them
    err = stream->RecoverBuffersFromLocation(location);
    if (err != NvSuccess)
    {
        NV_RETURN_FAIL(err);
    }

    location.SetLocation(COMPONENT_CAMERA,CAMERA_OUT_CAPTURE);
    // since we never know if cam<-> dz buffers will change lets abort all of them
    err = stream->RecoverBuffersFromLocation(location);
    if (err != NvSuccess)
    {
        NV_RETURN_FAIL(err);
    }

    //since we never know if the camdz cfg will change enable them
    NewStreamBufferCfgs.component[COMPONENT_CAMERA].outputPort[CAMERA_OUT_CAPTURE].used = NV_TRUE;
    NewStreamBufferCfgs.component[COMPONENT_CAMERA].outputPort[CAMERA_OUT_PREVIEW].used = NV_TRUE;

    err = m_BufferCfg->GetOutputConfiguration(COMPONENT_CAMERA,
                                              &NewStreamBufferCfgs.component[COMPONENT_CAMERA]);
    if (err != NvSuccess)
    {
        NV_RETURN_FAIL(err);
    }

    //copy the originial buffer counts as they would be reset to defaults by blocks
    NewStreamBufferCfgs.component[COMPONENT_CAMERA].outputPort[CAMERA_OUT_CAPTURE].bufReq.maxBuffers =
        stream->m_BufferStreamCfg.component[COMPONENT_CAMERA].outputPort[CAMERA_OUT_CAPTURE].bufReq.maxBuffers;
    NewStreamBufferCfgs.component[COMPONENT_CAMERA].outputPort[CAMERA_OUT_CAPTURE].bufReq.minBuffers =
        stream->m_BufferStreamCfg.component[COMPONENT_CAMERA].outputPort[CAMERA_OUT_CAPTURE].bufReq.minBuffers;

    NewStreamBufferCfgs.component[COMPONENT_CAMERA].outputPort[CAMERA_OUT_PREVIEW].bufReq.maxBuffers =
        stream->m_BufferStreamCfg.component[COMPONENT_CAMERA].outputPort[CAMERA_OUT_PREVIEW].bufReq.maxBuffers;
    NewStreamBufferCfgs.component[COMPONENT_CAMERA].outputPort[CAMERA_OUT_PREVIEW].bufReq.minBuffers =
        stream->m_BufferStreamCfg.component[COMPONENT_CAMERA].outputPort[CAMERA_OUT_PREVIEW].bufReq.minBuffers;

    // set custom settings from hal
    // Since Camera and DZ negotiate over size this just sets the number of buffers
    // in the camera requirements, this cannot change the resolution of that image.
    // The min/max buffers will be set here.
    err = SetUserGeneratedSettings(COMPONENT_CAMERA,
                                   &NewStreamBufferCfgs.component[COMPONENT_CAMERA],
                                   streamReq,
                                   NV_TRUE);
    if (err != NvSuccess)
    {
        NV_RETURN_FAIL(err);
    }

    // now we look to see if we have major differances
    err = UpdateBufferConfigurations(COMPONENT_CAMERA, NewStreamBufferCfgs, stream);
    if (err != NvSuccess)
    {
        NV_RETURN_FAIL(err);
    }

    err = UpdateBufferConfigurations(COMPONENT_DZ, NewStreamBufferCfgs, stream);
    if (err != NvSuccess)
    {
        NV_RETURN_FAIL(err);
    }

    return err;
}

NvError NvBufferStreamFactory::UpdateBufferConfigurations(NvBufferManagerComponent component,
                                                          NvStreamBufferConfig &NewStreamBufferCfgs,
                                                          NvBufferStream *stream)
{
    NvBufferOutputLocation location;
    NvError err = NvSuccess;
    NvU32 numberOutputPorts =
        NvBufferOutputLocation::GetNumberOfOuputPorts(component);

    for (NvU32 j = 0; j < numberOutputPorts; j ++)
    {
        NvOuputPortConfig *pNewPortCfg =
            &NewStreamBufferCfgs.component[component].outputPort[j];
        NvOuputPortConfig *pOldPortCfg =
            &stream->m_BufferStreamCfg.component[component].outputPort[j];

#ifndef DISABLE_REPURPOSE_BUFFERS   // [
        ALOGDD("REPURPOSE: UpdateBufferConfigurations: component %d, port %d, used=%d, equal=%d\n",
                        component, j, pNewPortCfg->used, BufferConfigurationsEqual(pOldPortCfg, pNewPortCfg));
#endif // DISABLE_REPURPOSE_BUFFERS    ]

        if (pNewPortCfg->used)
        {

            NvU32 totalAllocated = 0;
            NvU32 totalRequested = 0;
            NvU32 inUse = 0;
            location.SetLocation(component,j);

            if (!BufferConfigurationsEqual(pOldPortCfg, pNewPortCfg))
            {
                err = stream->GetNumberOfBuffers(location, &totalAllocated, &totalRequested, &inUse);
                if (err != NvSuccess)
                {
                    NV_RETURN_FAIL(err);
                }
                // New Configuration is different recover and realloc
                if (inUse != 0)
                {
                    err = stream->RecoverBuffersFromLocation(location);
                    if (err != NvSuccess)
                    {
                        NV_RETURN_FAIL(err);
                    }
                }

#ifndef DISABLE_REPURPOSE_BUFFERS   // [
                ALOGDD("REPURPOSE: calling stream->RepurposeBuffers(), component=%d, port=%d\n",
                                location.GetComponent(), location.GetPort());
                if (stream->RepurposeBuffers(location, pOldPortCfg, pNewPortCfg))
                {
                    continue;
                }
#endif // DISABLE_REPURPOSE_BUFFERS    ]

                err = stream->FreeBuffersFromLocation(location);
                if (err != NvSuccess)
                {
                    NV_RETURN_FAIL(err);
                }
                // finally copy the new config
                *pOldPortCfg = *pNewPortCfg;
                pOldPortCfg->originalBufCfg = pOldPortCfg->currentBufCfg;
            }
            else
            {
                // always use the new port buffer counts even if the buffers are the same
                pOldPortCfg->bufReq.minBuffers = pNewPortCfg->bufReq.minBuffers;
                pOldPortCfg->bufReq.maxBuffers = pNewPortCfg->bufReq.maxBuffers;
            }
        }
    }
    return err;
}


NvError NvBufferStreamFactory::ConfigureStandardCapture(
                                            NvStreamBufferConfig *pNewStreamBufferCfgs,
                                            NvStreamRequest const &streamReq)
{
    NvMMNewBufferRequirementsInfo *pBufReq;
    NvMMNewBufferConfigurationInfo *pCfg;
    NvError err = NvSuccess;

    // This function sets up all the buffers for the still capture case
    // but only for the DZ outputs

    // clear the stream config
    NvOsMemset((void *)pNewStreamBufferCfgs, 0, sizeof(NvStreamBufferConfig));

    // Configure Preview
    // Set defaults to DZ_Output Preview
    pNewStreamBufferCfgs->component[COMPONENT_DZ].outputPort[DZ_OUT_PREVIEW].used = NV_TRUE;
    pBufReq = &pNewStreamBufferCfgs->component[COMPONENT_DZ].outputPort[DZ_OUT_PREVIEW].bufReq;
    ClearAndSetDefaultBufferRequirements(pBufReq);
    pCfg = &pNewStreamBufferCfgs->component[COMPONENT_DZ].outputPort[DZ_OUT_PREVIEW].currentBufCfg;
    ClearAndSetDefaultBufferConfiguration(pCfg);

    // Configure Output Video
    // Set defaults to DZ_Output_Video
    pNewStreamBufferCfgs->component[COMPONENT_DZ].outputPort[DZ_OUT_VIDEO].used = NV_TRUE;
    pBufReq = &pNewStreamBufferCfgs->component[COMPONENT_DZ].outputPort[DZ_OUT_VIDEO].bufReq;
    ClearAndSetDefaultBufferRequirements(pBufReq);
    pCfg = &pNewStreamBufferCfgs->component[COMPONENT_DZ].outputPort[DZ_OUT_VIDEO].currentBufCfg;
    ClearAndSetDefaultBufferConfiguration(pCfg);

    // Configure Output Still
    // Set defaults to DZ_Output_Video
    pNewStreamBufferCfgs->component[COMPONENT_DZ].outputPort[DZ_OUT_STILL].used = NV_TRUE;
    pBufReq = &pNewStreamBufferCfgs->component[COMPONENT_DZ].outputPort[DZ_OUT_STILL].bufReq;
    ClearAndSetDefaultBufferRequirements(pBufReq);
    pCfg = &pNewStreamBufferCfgs->component[COMPONENT_DZ].outputPort[DZ_OUT_STILL].currentBufCfg;
    ClearAndSetDefaultBufferConfiguration(pCfg);

    // get any specific setting that are required by the hw module
    m_BufferCfg->GetOutputRequirements(COMPONENT_DZ, &pNewStreamBufferCfgs->component[COMPONENT_DZ]);

    // set custom settings from hal
    err = SetUserGeneratedSettings(COMPONENT_DZ,
                                   &pNewStreamBufferCfgs->component[COMPONENT_DZ],
                                   streamReq,
                                   NV_FALSE);
    if (err != NvSuccess)
    {
        NV_RETURN_FAIL(err);
    }

    // Call into DZ with requirements, get the output bufer configurations
    // at this point we have all of the output requirements, call into driver to get the configs
    // and set the input requirements.
    err = m_BufferCfg->GetOutputConfiguration(COMPONENT_DZ,
                                              &pNewStreamBufferCfgs->component[COMPONENT_DZ]);
    if (err != NvSuccess)
    {
        NV_RETURN_FAIL(err);
    }

    // Now since DZ and Cam drivers will negotiate we will be able to just get the
    // Cam output buffer configurations as long as we wait for the negotiations to
    // finish.

    pNewStreamBufferCfgs->component[COMPONENT_CAMERA].outputPort[CAMERA_OUT_PREVIEW].used =
                                                                                        NV_TRUE;
    pNewStreamBufferCfgs->component[COMPONENT_CAMERA].outputPort[CAMERA_OUT_PREVIEW].bufReq =
                    pNewStreamBufferCfgs->component[COMPONENT_DZ].inputPort[DZ_IN_PREVIEW].bufReq;
    pCfg =
        &pNewStreamBufferCfgs->component[COMPONENT_CAMERA].outputPort[CAMERA_OUT_PREVIEW].currentBufCfg;
    ClearAndSetDefaultBufferConfiguration(pCfg);


    pNewStreamBufferCfgs->component[COMPONENT_CAMERA].outputPort[CAMERA_OUT_CAPTURE].used = NV_TRUE;
    pNewStreamBufferCfgs->component[COMPONENT_CAMERA].outputPort[CAMERA_OUT_CAPTURE].bufReq =
        pNewStreamBufferCfgs->component[COMPONENT_DZ].inputPort[DZ_IN_STILL].bufReq;
    pCfg = &pNewStreamBufferCfgs->component[COMPONENT_CAMERA].outputPort[CAMERA_OUT_CAPTURE].currentBufCfg;
    ClearAndSetDefaultBufferConfiguration(pCfg);

    // todo have a way to add the # of required buffers for capture streams here
    // all we need to do is to modify the returning buffCfg's with user settings
    err = m_BufferCfg->GetOutputConfiguration(COMPONENT_CAMERA,
                                              &pNewStreamBufferCfgs->component[COMPONENT_CAMERA]);
    if (err != NvSuccess)
    {
        NV_RETURN_FAIL(err);
    }

    // set custom settings from hal
    // Since Camera and DZ negotiate over size this just sets the number of buffers
    // in the camera requirements, this cannot change the resolution of that image.
    // The min/max buffers will be set here.
    err = SetUserGeneratedSettings(COMPONENT_CAMERA,
                                   &pNewStreamBufferCfgs->component[COMPONENT_CAMERA],
                                   streamReq,
                                   NV_FALSE);

    if (err != NvSuccess)
    {
        NV_RETURN_FAIL(err);
    }

    return err;
}

NvError NvBufferStreamFactory::SetUserGeneratedSettings(NvBufferManagerComponent component,
                                                        NvComponentBufferConfig *pStreamBuffCfg,
                                                        NvStreamRequest streamReq,
                                                        NvBool setUsed)
{
    NvBufferRequest             customRequest;
    NvRmSurface                 *pSurf;
    NvMMNewBufferRequirementsInfo *pBufReq;

    while (streamReq.PopBufferRequest(&customRequest,component))
    {
        NvU32 port = customRequest.Location.GetPort();
        pBufReq = &pStreamBuffCfg->outputPort[port].bufReq;
        pSurf =  &pBufReq->format.videoFormat.SurfaceDescription[NvMMPlanar_Single];

        if (setUsed)
            pStreamBuffCfg->outputPort[port].used = NV_TRUE;
        //We allow 0 buffer requested in the requirement
        pBufReq->minBuffers  = customRequest.MinBuffers;
        pBufReq->maxBuffers  = customRequest.MaxBuffers;
        if ( customRequest.Height != 0 && component != COMPONENT_CAMERA)
            pSurf->Height        = customRequest.Height;
        if ( customRequest.Width != 0 && component != COMPONENT_CAMERA)
            pSurf->Width         = customRequest.Width;
    };
    return NvSuccess;
}

NvError NvBufferStreamFactory::ClearAndSetDefaultBufferRequirements(
                                                    NvMMNewBufferRequirementsInfo *pBufReq)
{
    NvRmSurface *pSurf;
    // clear the requirements
    NvOsMemset(pBufReq, 0, sizeof(NvMMNewBufferRequirementsInfo));
    pSurf =  &pBufReq->format.videoFormat.SurfaceDescription[NvMMPlanar_Single];

    // set up default width and height for now
    pSurf->Width = CAMERA_DEFAULT_OUTPUT_WIDTH;
    pSurf->Height = CAMERA_DEFAULT_OUTPUT_HEIGHT;

    // set the correct default format and restrictions
    pSurf->ColorFormat = NvColorFormat_Y8;
    pSurf->Layout = NvRmSurfaceLayout_Pitch;
    pBufReq->format.videoFormat.SurfaceRestriction |=
                            NVMM_SURFACE_RESTRICTION_YPITCH_EQ_2X_UVPITCH;

    // set the default number of surfaces
    pBufReq->format.videoFormat.NumberOfSurfaces = 1;

    // set up default number of buffers
    pBufReq->minBuffers = CAMERA_DEFAULT_OUTPUT_BUFFERS;
    pBufReq->maxBuffers = CAMERA_DEFAULT_OUTPUT_BUFFERS;

    // set up defaults
    pBufReq->bPhysicalContiguousMemory = NV_FALSE;
    pBufReq->byteAlignment = 0;
    pBufReq->endianness = NvMMBufferEndianess_LE;
    pBufReq->minBufferSize = sizeof(NvMMSurfaceDescriptor);
    pBufReq->maxBufferSize = sizeof(NvMMSurfaceDescriptor);
    pBufReq->bInSharedMemory = 0;
    pBufReq->memorySpace = NvMMMemoryType_SYSTEM;
    pBufReq->formatId = NvMMBufferFormatId_Video;
    pBufReq->structSize = sizeof(NvMMNewBufferRequirementsInfo);
    pBufReq->event = NvMMEvent_NewBufferRequirements;

    return NvSuccess;
}

NvError NvBufferStreamFactory::ClearAndSetDefaultBufferConfiguration(
                                        NvMMNewBufferConfigurationInfo *pCfg)
{

    NvOsMemset(pCfg, 0, sizeof(NvMMNewBufferConfigurationInfo));
    pCfg->event = NvMMEvent_NewBufferConfiguration;
    pCfg->structSize = sizeof(NvMMNewBufferConfigurationInfo);
    return NvSuccess;
}

