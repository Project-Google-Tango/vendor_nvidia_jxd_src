/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvbuffer_stream.h"

NvBufferStream::NvBufferStream()
    : m_Initialized(NV_FALSE)
    , m_pAllocator(NULL)
    , m_pBufferHandler(NULL)
    , m_StreamType(CAMERA_STANDARD_CAPTURE)
{
    // class must be made by factory
    NvOsMemset(&m_BufferStreamCfg, 0, sizeof(m_BufferStreamCfg));
}

NvBufferStream::~NvBufferStream()
{
    if (m_Initialized)
    {
        FreeUnusedBuffers();
        for ( NvU32 i = 0; i < NvBufferOutputLocation::GetNumberOfComponents(); i++)
        {
            NvU32 numberOutputPorts = NvBufferOutputLocation::GetNumberOfOuputPorts(
                                                    (NvBufferManagerComponent)i);
            for (NvU32 j = 0; j < numberOutputPorts; j ++)
            {
                NvU32 totalAllocated, totalRequested, inUse;
                NvBufferOutputLocation location;
                location.SetLocation((NvBufferManagerComponent)i,j);
                GetNumberOfBuffers(location, &totalAllocated,&totalRequested, &inUse);
                if (totalAllocated > 0)
                {
                    NV_ERROR_MSG(("Buffers not returned, expect memory leak"));
                }
            }
        }
        delete m_pAllocator;
        m_pAllocator = NULL;
        delete m_pBufferHandler;
        m_pBufferHandler = NULL;
   }

    m_Initialized = NV_FALSE;

}

NvU32 NvBufferStream::GetBufferIdFromLocation(NvBufferOutputLocation location, NvU32 index)
{
    NvU32 comp, port, out;
    comp = location.GetComponent();
    port = location.GetPort();
    comp = comp & 0xff;
    port = port & 0xff;
    index = index & 0xff;
    out = (comp << 16) | (port << 8) | index;
    return out;
}

NvError NvBufferStream::GetLocationFromBufferID(NvU32 bufferID,
                                                NvBufferOutputLocation *location,
                                                NvU32 *index)
{
    NvU32 comp, port;
    *index = 0xff & bufferID;
    port = 0xff & (bufferID >> 8);
    comp = 0xff & (bufferID >> 16);
    location->SetLocation((NvBufferManagerComponent)comp,port);
    return NvSuccess;
}

NvError NvBufferStream::GetOutputPortBufferCfg(NvBufferOutputLocation location,
                                               NvMMNewBufferConfigurationInfo *bufCfg)
{
    if (!m_Initialized)
    {
        NV_RETURN_FAIL(NvError_NotInitialized);
    }

    if (!bufCfg)
    {
        NV_RETURN_FAIL(NvError_BadParameter);
    }

    *bufCfg = GetOutputPortConfig(location)->currentBufCfg;
    return NvSuccess;
}

NvError NvBufferStream::GetInputPortBufferCfg(NvBufferInputLocation location,
                                              NvMMNewBufferConfigurationInfo *bufCfg)
{
    if (!m_Initialized)
    {
        NV_RETURN_FAIL(NvError_NotInitialized);
    }

    if (!bufCfg)
    {
        NV_RETURN_FAIL(NvError_BadParameter);
    }

    *bufCfg = GetInputPortConfig(location)->currentBufCfg;
    return NvSuccess;
}


NvError NvBufferStream::AllocateAllBuffers()
{
    NvU32 numberComponents = NvBufferOutputLocation::GetNumberOfComponents();
    NvError err = NvSuccess;

    if (!m_Initialized)
    {
        NV_RETURN_FAIL(NvError_NotInitialized);
    }


    for ( NvU32 i = 0; i < numberComponents; i++)
    {
        NvU32 numberOutputPorts =
            NvBufferOutputLocation::GetNumberOfOuputPorts((NvBufferManagerComponent)i);
        for (NvU32 j = 0; j < numberOutputPorts; j ++)
        {
            NvBufferOutputLocation location;
            location.SetLocation((NvBufferManagerComponent)i,j);
            err = AllocateBuffers(location);
            if (err != NvSuccess)
            {
                NV_RETURN_FAIL(err);
            }
        }
    }
    return err;
}

NvError NvBufferStream::AllocateBuffers(NvBufferOutputLocation location)
{
    NvError err = NvSuccess;
    NvOuputPortConfig *outPort;

    if (!m_Initialized)
    {
        NV_RETURN_FAIL(NvError_NotInitialized);
    }

    outPort = GetOutputPortConfig(location);

    if (! outPort->used)
    {
        return NvSuccess;
    }

    while (outPort->totalBuffersAllocated < outPort->bufReq.maxBuffers)
    {
        NvU32 i;
        for (i = 0; i < MAX_OUTPUT_BUFFERS_PER_PORT; i++)
        {
            if (! outPort->buffer[i].bufferAllocated)
            {
                // Always allocate with the original configuration
                err = m_pAllocator->AllocateBuffer(location,
                                                   &outPort->originalBufCfg,
                                                   &outPort->buffer[i].pBuffer);

                if (err != NvSuccess)
                {
                    if (outPort->totalBuffersAllocated >=outPort->bufReq.minBuffers)
                    {
                        return NvSuccess;
                    }
                    NV_RETURN_FAIL(err);
                }

                if (BufferConfigurationsEqual(&outPort->currentBufCfg,&outPort->originalBufCfg) ==
                    NV_FALSE)
                {
                    // set the buffer to the current configuration
                    err = m_pAllocator->SetBufferCfg(location,
                                                    &outPort->currentBufCfg,
                                                    outPort->buffer[i].pBuffer);
                }

                outPort->buffer[i].bufferAllocated = NV_TRUE;
                outPort->buffer[i].bufferInUse = NV_FALSE;
                outPort->totalBuffersAllocated++;
                break;
            }
        }
        if (i == MAX_OUTPUT_BUFFERS_PER_PORT)
        {
            NV_RETURN_FAIL(NvError_InsufficientMemory);
        }
    };
    return err;
}

NvBool NvBufferStream::RepurposeBuffers(NvBufferOutputLocation location,
                                        NvOuputPortConfig *existingConfig,
                                        const NvOuputPortConfig *newConfig)
{
    const NvMMNewBufferConfigurationInfo& originalCfg  = newConfig->originalBufCfg;
    const NvMMNewBufferConfigurationInfo& requestedCfg = newConfig->currentBufCfg;
    if (m_pAllocator->RepurposeBuffers(location, originalCfg, requestedCfg,
                                       existingConfig->buffer, existingConfig->totalBuffersAllocated))
    {
        existingConfig->currentBufCfg = requestedCfg;
        return NV_TRUE;
    }
    else
    {
        return NV_FALSE;
    }
}

NvError NvBufferStream::SetNumberOfBuffers(
    NvBufferOutputLocation location,
    NvU32 requiredMinNumBuffers,
    NvU32 requestedNumBuffers,
    NvU32 *allocatedNumBuffers)
{
    NvOuputPortConfig *outPort;
    NvU32 currentAllocatedTotal, currentTotalRequested, inUse, i;
    NvError err = NvSuccess;

    ALOGV("%s ++ Location: (Cmp %d Port %d), requiredNum %d, requestNum %d",
        __FUNCTION__, location.GetComponent(), location.GetPort(),
        requiredMinNumBuffers, requestedNumBuffers);

    *allocatedNumBuffers = 0;
    if (!m_Initialized)
    {
        NV_RETURN_FAIL(NvError_NotInitialized);
    }

    if (requiredMinNumBuffers > requestedNumBuffers)
    {
        NV_RETURN_FAIL(NvError_BadParameter);
    }

    // If the min requirement is bigger than the max we can offer,
    // nothing we can do about it
    if (requiredMinNumBuffers > MAX_OUTPUT_BUFFERS_PER_PORT)
    {
        NV_RETURN_FAIL(NvError_BadParameter);
    }
    // Cap the requested buffer number
    if (requestedNumBuffers > MAX_OUTPUT_BUFFERS_PER_PORT)
    {
        ALOGDD("Requested buffer is capped to %d from %d in %s",
            MAX_OUTPUT_BUFFERS_PER_PORT,
            requestedNumBuffers,
            __func__);
        requestedNumBuffers = MAX_OUTPUT_BUFFERS_PER_PORT;
    }

    GetNumberOfBuffers(location,
        &currentAllocatedTotal,
        &currentTotalRequested,
        &inUse);

    // set the new number of buffers
    outPort = GetOutputPortConfig(location);

    outPort->bufReq.minBuffers = requiredMinNumBuffers;
    outPort->bufReq.maxBuffers = requestedNumBuffers;
    *allocatedNumBuffers = currentAllocatedTotal;

    if (currentAllocatedTotal != requestedNumBuffers)
    {
        err = RecoverBuffersFromLocation(location);
        if (err != NvSuccess)
        {
            NV_RETURN_FAIL(err);
        }

        if ( currentAllocatedTotal > requestedNumBuffers)
        {
            err = ReSizeBufferPool(location);
            *allocatedNumBuffers = outPort->totalBuffersAllocated;
        }
        else
        {
            err = AllocateBuffers(location);
            if (err != NvSuccess)
            {
                // Even though we don't get as many as want, but as long
                // as we can meet the min allowance, we don't return a
                // failure, and let the upper level to decide what to do.
                if (outPort->totalBuffersAllocated >= requiredMinNumBuffers)
                {
                    ALOGDD("%s requests %d buffers, with %d the min requirement",
                        __func__, requestedNumBuffers, requiredMinNumBuffers);

                    *allocatedNumBuffers = outPort->totalBuffersAllocated;
                    err = NvSuccess;
                }
            }
            else
            {
                *allocatedNumBuffers = requestedNumBuffers;
            }
        }

        if (err != NvSuccess)
        {
            NV_RETURN_FAIL(err);
        }
        err = SendBuffersToLocation(location);
    }

    ALOGV("%s -- Location: (Cmp %d Port %d), allocatedNumBuffers %d",
        __FUNCTION__, location.GetComponent(), location.GetPort(),
        *allocatedNumBuffers);
    return err;
}

NvError NvBufferStream::GetNumberOfBuffers(NvBufferOutputLocation location,
                                            NvU32 *totalAllocated,
                                            NvU32 *totalRequested,
                                            NvU32 *inUse)
{
    NvOuputPortConfig *outPort;

    if (!m_Initialized)
    {
        NV_RETURN_FAIL(NvError_NotInitialized);
    }

    if (!inUse || !totalAllocated || !totalRequested)
    {
        NV_RETURN_FAIL(NvError_BadParameter);
    }

    outPort = GetOutputPortConfig(location);
    *totalAllocated = outPort->totalBuffersAllocated;
    *inUse = 0;
    *totalRequested = outPort->bufReq.maxBuffers;
    for (NvU32 i = 0; i < MAX_OUTPUT_BUFFERS_PER_PORT; i++)
    {
        if (outPort->buffer[i].bufferInUse && outPort->buffer[i].bufferAllocated)
        {
            (*inUse)++;
        }
    }
    return NvSuccess;
}

NvError NvBufferStream::GetUnusedBufferPointers(NvBufferOutputLocation location,
                                                NvMMBuffer *buffer[],
                                                NvU32 numberRequested,
                                                NvU32 *numberFilled)
{
    NvOuputPortConfig *outPort = GetOutputPortConfig(location);

    if (!m_Initialized)
    {
        NV_RETURN_FAIL(NvError_NotInitialized);
    }

    if (!buffer || !numberFilled || numberRequested == 0)
    {
        NV_RETURN_FAIL(NvError_BadParameter);
    }

    if (!outPort->used)
    {
        NV_RETURN_FAIL(NvError_InvalidState);
    }

    *numberFilled = 0;

    for (NvU32 i = 0; i < MAX_OUTPUT_BUFFERS_PER_PORT; i++)
    {
        if (outPort->buffer[i].bufferAllocated &&
            !outPort->buffer[i].bufferInUse)
        {
            buffer[*numberFilled] = outPort->buffer[i].pBuffer;
            (*numberFilled)++;
            if (*numberFilled == numberRequested)
            {
                break;
            }
        }
    }
    return NvSuccess;
}

NvError NvBufferStream::UseBufferPointers(NvBufferOutputLocation location,
                                                NvMMBuffer *buffer[],
                                                NvU32 numberRequested,
                                                NvU32 *numberFilled)
{
    NvOuputPortConfig *outPort = GetOutputPortConfig(location);

    if (!m_Initialized)
    {
        NV_RETURN_FAIL(NvError_NotInitialized);
    }

    if (!buffer || !numberFilled || numberRequested == 0)
    {
        NV_RETURN_FAIL(NvError_BadParameter);
    }

    if (!outPort->used)
    {
        NV_RETURN_FAIL(NvError_InvalidState);
    }

    *numberFilled = 0;

    for (NvU32 i = 0; i < MAX_OUTPUT_BUFFERS_PER_PORT; i++)
    {
        if (outPort->buffer[i].bufferAllocated &&
            !outPort->buffer[i].bufferInUse)
        {
            buffer[*numberFilled] = outPort->buffer[i].pBuffer;
            buffer[*numberFilled]->BufferID = i;
            outPort->buffer[i].bufferInUse = NV_TRUE;
            (*numberFilled)++;
            if (*numberFilled == numberRequested)
            {
                break;
            }
        }
    }
    return NvSuccess;
}

NvError NvBufferStream::UnuseBufferPointers(NvBufferOutputLocation location,
                                                NvMMBuffer *buffer[],
                                                NvU32 numberRequested,
                                                NvU32 *numberFilled)
{
    NvOuputPortConfig *outPort = GetOutputPortConfig(location);

    if (!m_Initialized)
    {
        NV_RETURN_FAIL(NvError_NotInitialized);
    }

    if (!buffer || !numberFilled || numberRequested == 0)
    {
        NV_RETURN_FAIL(NvError_BadParameter);
    }

    if (!outPort->used)
    {
        NV_RETURN_FAIL(NvError_InvalidState);
    }

    *numberFilled = 0;

    for (NvU32 i = 0; i < numberRequested; i++)
        for (NvU32 j = 0; j < MAX_OUTPUT_BUFFERS_PER_PORT; j++)
        {
            if (outPort->buffer[j].pBuffer == buffer[i])
            {
                outPort->buffer[j].bufferInUse = NV_FALSE;
                (*numberFilled)++;
                break;
            }
        }

    return NvSuccess;
}


NvError NvBufferStream::SendBuffersToComponent(NvBufferManagerComponent component)
{
    NvError err = NvSuccess;
    NvU32 numberOutputPorts =
            NvBufferOutputLocation::GetNumberOfOuputPorts(component);

    if (!m_Initialized)
    {
        NV_RETURN_FAIL(NvError_NotInitialized);
    }

    if (component == COMPONENT_NUMBER_OF_COMPONENTS)
    {
        NV_RETURN_FAIL(NvError_BadParameter);
    }

    for (NvU32 j = 0; j < numberOutputPorts; j ++)
    {
        NvBufferOutputLocation location;
        location.SetLocation(component,j);
        SendBuffersToLocation(location);
    }
    return err;
}

NvError NvBufferStream::SendBuffersToLocation(NvBufferOutputLocation location)
{
    NvError err = NvSuccess;

    if (!m_Initialized)
    {
        NV_RETURN_FAIL(NvError_NotInitialized);
    }

    NvOuputPortConfig *outPort = GetOutputPortConfig(location);

    for (NvU32 i = 0; i < outPort->totalBuffersAllocated; i++)
    {
        if (outPort->used && !outPort->buffer[i].bufferInUse)
        {
            err = m_pBufferHandler->GiveBufferToComponent(
                                                location,
                                                outPort->buffer[i].pBuffer);
            if (err != NvSuccess)
            {
                NV_RETURN_FAIL(err);
            }
            outPort->buffer[i].bufferInUse = NV_TRUE;
        }
    }
    return err;
}

NvError NvBufferStream::RecoverBuffersFromComponent(NvBufferManagerComponent component)
{
    NvError err = NvSuccess;
    NvU32 numberOutputPorts =
            NvBufferOutputLocation::GetNumberOfOuputPorts(component);

    if (!m_Initialized)
    {
        NV_RETURN_FAIL(NvError_NotInitialized);
    }

    if (component == COMPONENT_NUMBER_OF_COMPONENTS)
    {
        NV_RETURN_FAIL(NvError_BadParameter);
    }

    for (NvU32 j = 0; j < numberOutputPorts; j ++)
    {
        NvBufferOutputLocation location;
        location.SetLocation(component,j);
        err = RecoverBuffersFromLocation(location);
        if (err != NvSuccess)
        {
            return err;
        }

    }
    return err;
}

NvError NvBufferStream::RecoverBuffersFromLocation(NvBufferOutputLocation location)
{
    NvError err = NvSuccess;
    NvU32 totalAllocated = 0;
    NvU32 totalRequested = 0;
    NvU32 inUse = 0;

    if (!m_Initialized)
    {
        NV_RETURN_FAIL(NvError_NotInitialized);
    }
    NvOuputPortConfig *outPort = GetOutputPortConfig(location);

    if(!outPort->used || outPort->totalBuffersAllocated == 0)
    {
        return err;
    }

    err = GetNumberOfBuffers(location,&totalAllocated,&totalRequested,&inUse);
    if (err != NvSuccess)
    {
        NV_RETURN_FAIL(err);
    }

    if(inUse == 0)
    {
        return err;
    }

    err = m_pBufferHandler->ReturnBuffersToManager(location);
    if (err != NvSuccess)
    {
        NV_RETURN_FAIL(err);
    }

    for (NvU32 i = 0; i < outPort->totalBuffersAllocated; i++)
    {
        outPort->buffer[i].bufferInUse = NV_FALSE;
    }

    return err;
}


NvError NvBufferStream::FreeUnusedBuffers()
{
    NvU32 numberComponents = NvBufferOutputLocation::GetNumberOfComponents();
    NvError err = NvSuccess;

    if (!m_Initialized)
    {
        NV_RETURN_FAIL(NvError_NotInitialized);
    }

    for ( NvU32 i = 0; i < numberComponents; i++)
    {
        NvU32 numberOutputPorts =
            NvBufferOutputLocation::GetNumberOfOuputPorts((NvBufferManagerComponent)i);
        for (NvU32 j = 0; j < numberOutputPorts; j ++)
        {
            NvOuputPortConfig *outPort = &m_BufferStreamCfg.component[i].outputPort[j];
            NvBufferOutputLocation location;

            if (!outPort->used)
            {
                continue;
            }

           location.SetLocation((NvBufferManagerComponent)i,j);
           err = FreeBuffersFromLocation(location);
           if (err != NvSuccess)
           {
               NV_RETURN_FAIL(err);
           }
        }
    }
    return err;
}

NvError NvBufferStream::ReSizeBufferPool(NvBufferOutputLocation location)
{
    NvError err = NvSuccess;
    NvOuputPortConfig *outPort = GetOutputPortConfig(location);

    if (!m_Initialized)
    {
        NV_RETURN_FAIL(NvError_NotInitialized);
    }

    if (!outPort->used)
    {
        return err;
    }

    for ( NvU32 k = outPort->bufReq.maxBuffers; k < MAX_OUTPUT_BUFFERS_PER_PORT; k++)
    {
        // if allocated and not in use
        if (outPort->buffer[k].bufferAllocated && !outPort->buffer[k].bufferInUse)
        {
             if (BufferConfigurationsEqual(&outPort->currentBufCfg,&outPort->originalBufCfg) ==
                NV_FALSE)
             {
                // set the buffer to the current configuration
                err = m_pAllocator->SetBufferCfg(location,
                                                &outPort->originalBufCfg,
                                                outPort->buffer[k].pBuffer);
            }

            err = m_pAllocator->FreeBuffer(location, outPort->buffer[k].pBuffer);
            if (err != NvSuccess)
            {
                NV_RETURN_FAIL(err);
            }

            outPort->buffer[k].bufferAllocated = NV_FALSE;
            outPort->totalBuffersAllocated--;
        }
    }

    return err;
}

NvError NvBufferStream::FreeBuffersFromLocation(NvBufferOutputLocation location)
{
    NvError err = NvSuccess;
    NvOuputPortConfig *outPort = GetOutputPortConfig(location);

    if (!m_Initialized)
    {
        NV_RETURN_FAIL(NvError_NotInitialized);
    }

    if (!outPort->used)
    {
        return err;
    }

#ifndef DISABLE_REPURPOSE_BUFFERS   // [
    // Restore buffers to their original values, if necessary
    if (m_pAllocator->RepurposeBuffers(location, outPort->originalBufCfg, outPort->originalBufCfg,
                                       outPort->buffer, outPort->totalBuffersAllocated))
    {
        ALOGDD("REPURPOSE: Restored buffers to their original configurations (component=%d,port=%d)\n",
                        location.GetComponent(), location.GetPort());
    }
#endif // DISABLE_REPURPOSE_BUFFERS    ]

    for ( NvU32 k = 0; k < MAX_OUTPUT_BUFFERS_PER_PORT; k++)
    {
        // if allocated and not in use
        if (outPort->buffer[k].bufferAllocated && !outPort->buffer[k].bufferInUse)
        {
            err = m_pAllocator->FreeBuffer(location, outPort->buffer[k].pBuffer);
            if (err != NvSuccess)
            {
                NV_RETURN_FAIL(err);
            }

            outPort->buffer[k].bufferAllocated = NV_FALSE;
            outPort->totalBuffersAllocated--;
        }
    }

    return err;
}

NvError NvBufferStream::GetAllBufferLocations(
                                NvBufferOutputLocation locations[MAX_TOTAL_OUTPUT_PORTS],
                                NvU32 *numberOflocations)
{
    NvU32 numberComponents = NvBufferOutputLocation::GetNumberOfComponents();

    if (!m_Initialized)
    {
        NV_RETURN_FAIL(NvError_NotInitialized);
    }

    *numberOflocations = 0;
    for ( NvU32 i = 0; i < numberComponents; i++)
    {
        NvU32 numberOutputPorts =
            NvBufferOutputLocation::GetNumberOfOuputPorts((NvBufferManagerComponent)i);
        for (NvU32 j = 0; j < numberOutputPorts; j ++)
        {
            if (m_BufferStreamCfg.component[i].outputPort[j].totalBuffersAllocated > 0)
            {
                locations[*numberOflocations].SetLocation((NvBufferManagerComponent)i,j);
                (*numberOflocations)++;
            }
        }
    }

    return NvSuccess;
}

NvOuputPortConfig *NvBufferStream::GetOutputPortConfig(NvBufferOutputLocation location)
{
    NvBufferManagerComponent comp = location.GetComponent();
    NvU32 port = location.GetPort();
    return &m_BufferStreamCfg.component[comp].outputPort[port];
}

NvInputPortConfig *NvBufferStream::GetInputPortConfig(NvBufferInputLocation location)
{
    NvBufferManagerComponent comp = location.GetComponent();
    NvU32 port = location.GetPort();
    return &m_BufferStreamCfg.component[comp].inputPort[port];
}
