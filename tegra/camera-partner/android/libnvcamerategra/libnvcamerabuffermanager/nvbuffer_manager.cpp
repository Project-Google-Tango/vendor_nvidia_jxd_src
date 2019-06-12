/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvbuffer_manager.h"

NvBufferManager* NvBufferManager::m_instance[] = {0, 0, 0, 0, 0};


NvError NvStreamRequest::AddCustomBufferRequest(NvBufferRequest request)
{
    NvBufferManagerComponent    component;
    NvU32                       port;
    // check if a configuration already exists for this location
    // throw error if it does since we do not know what to do

    component = request.Location.GetComponent();
    port  = request.Location.GetPort();

    if (component >= MAX_COMPONENTS || port >= MAX_PORTS)
    {
        NV_RETURN_FAIL(NvError_BadParameter);
    }

    for ( NvS32 i = 0; i < m_RequestIndex[component]; i ++)
    {
        if (port == m_RequestStack[component][i].Location.GetPort())
        {
            NV_RETURN_FAIL(NvError_BadParameter);
        }
    }

    // add to the stack
    return PushBufferRequest(request, component);
}


NvBool NvStreamRequest::PopBufferRequest(NvBufferRequest *request,
                                         NvBufferManagerComponent component)
{
    m_RequestIndex[component]--;
    if (m_RequestIndex[component] < 0)
    {
        return NV_FALSE;
    }

    *request = m_RequestStack[component][m_RequestIndex[component]];
    return NV_TRUE;
}

NvError NvStreamRequest::PushBufferRequest(NvBufferRequest request,
                                           NvBufferManagerComponent component)
{
    if (m_RequestIndex[component] >= MAX_PORTS)
    {
        NV_RETURN_FAIL(NvError_OverFlow);
    }

    m_RequestStack[component][m_RequestIndex[component]] = request;
    m_RequestIndex[component]++;
    return NvSuccess;
}

NvStreamRequest::NvStreamRequest()
{
    NvOsMemset(m_RequestIndex, 0, sizeof(m_RequestIndex));
    NvOsMemset(m_RequestStack, 0, sizeof(m_RequestStack));
}

NvBufferManager::NvBufferManager()
{
    m_BufferFactory = NULL;
}

NvBufferManager::~NvBufferManager()
{
    m_BufferFactory = NULL;
}

NvBufferManager* NvBufferManager::Instance(NvU32 id, NvCameraDriverInfo const &driverInfo)
{
    if (id >= MAX_NUM_SENSORS)
        return NULL;

    if (m_instance[id] == 0)
    {
        m_instance[id] = new NvBufferManager();
        m_instance[id]->m_BufferFactory = NvBufferStreamFactory::Instance(id, driverInfo);
    }

    return m_instance[id];
}

void NvBufferManager::Release(NvU32 id)
{
    if (id >= MAX_NUM_SENSORS)
        return;

    if (m_instance[id])
    {
        m_instance[id]->m_BufferFactory->Release(id);
        delete m_instance[id];
    }

    m_instance[id] = NULL;
}

NvError NvBufferManager::CloseStream(NvBufferStream* pStream)
{
    NvError ret = NvSuccess;

    if (pStream == NULL)
    {
        NV_RETURN_FAIL(NvError_BadParameter);
    }

    for (NvU32 i = 0; i < COMPONENT_NUMBER_OF_COMPONENTS; i++)
    {
        ret = pStream->RecoverBuffersFromComponent((NvBufferManagerComponent)i);
        if (ret != NvSuccess)
        {
            NV_RETURN_FAIL(ret);
        }
    }

    ret = pStream->FreeUnusedBuffers();
    if (ret != NvSuccess)
    {
        NV_RETURN_FAIL(ret);
    }

    return ret;
}

NvError NvBufferManager::InitializeStream(NvBufferStream* pStream,
                                          NvBufferStreamType streamType,
                                          NvStreamRequest const &streamReq)
{
    NvError ret = NvSuccess;

    if (!pStream)
    {
        NV_RETURN_FAIL(NvError_BadParameter);
    }

    ret = m_BufferFactory->InitializeStream(pStream, streamType, streamReq);
    if (ret != NvSuccess)
    {
        NV_RETURN_FAIL(ret);
    }

    return ret;
}

NvError NvBufferManager::InvalidateDriverInfo(NvBufferStream *pStream)
{
    NvError ret = NvSuccess;

    if (!pStream)
    {
        NV_RETURN_FAIL(NvError_BadParameter);
    }

    ret = m_BufferFactory->InvalidateDriverInfo(pStream);
    if (ret != NvSuccess)
    {
        NV_RETURN_FAIL(ret);
    }

    return ret;
}

NvError NvBufferManager::ReInitializeDriverInfo(NvBufferStream *pStream,
                                                NvCameraDriverInfo const &driverInfo)
{
    NvError ret = NvSuccess;

    if (!pStream)
    {
        NV_RETURN_FAIL(NvError_BadParameter);
    }

    ret = m_BufferFactory->ReInitializeDriverInfo(pStream,driverInfo);
    if (ret != NvSuccess)
    {
        NV_RETURN_FAIL(ret);
    }

    return ret;
}



NvBufferOutputLocation::NvBufferOutputLocation()
{
    Component = COMPONENT_NUMBER_OF_COMPONENTS;
    Port = 0;
}

NvError NvBufferOutputLocation::SetLocation(NvBufferManagerComponent component,
                                            NvU32 outputPort)
{

    if (outputPort >= GetNumberOfOuputPorts(component))
    {
        NV_RETURN_FAIL(NvError_BadParameter);
    }

    Component = component;
    Port = outputPort;
    return NvSuccess;
}

NvU32 NvBufferOutputLocation::GetNumberOfOuputPorts(NvBufferManagerComponent component)
{
    switch (component)
    {
        case COMPONENT_CAMERA:
        {
             return CAMERA_OUT_NUMBER_OF_PORTS;
             break;
        }
        case COMPONENT_DZ:
        {
            return DZ_OUT_NUMBER_OF_PORTS;
            break;
        }
        case COMPONENT_HOST:
        {
            return HOST_OUT_NUMBER_OF_PORTS;
            break;
        }
        default:
        {
            NV_ERROR_MSG("Invalid component");
            break;
        }
    }
    return 0;
}

NvU32  NvBufferOutputLocation::GetNumberOfComponents()
{
     return COMPONENT_NUMBER_OF_COMPONENTS;
}

NvBufferManagerComponent NvBufferOutputLocation::GetComponent()
{
    return Component;
}

NvU32 NvBufferOutputLocation::GetPort()
{
    return Port;
}

NvError NvBufferInputLocation::SetLocation(NvBufferManagerComponent component,
                                           NvU32 inputPort)
{

    if (inputPort >= GetNumberOfInputPorts(component))
    {
        NV_RETURN_FAIL(NvError_BadParameter);
    }

    Component = component;
    Port = inputPort;
    return NvSuccess;
}

NvU32 NvBufferInputLocation::GetNumberOfInputPorts(NvBufferManagerComponent component)
{
    switch (component)
    {
        case COMPONENT_CAMERA:
        {
             return CAMERA_IN_NUMBER_OF_PORTS;
             break;
        }
        case COMPONENT_DZ:
        {
            return DZ_IN_NUMBER_OF_PORTS;
            break;
        }
        case COMPONENT_HOST:
        {
            return HOST_IN_NUMBER_OF_PORTS;
            break;
        }
        default:
        {
            NV_ERROR_MSG("Invalid component");
            break;
        }
    }
    return 0;
}

NvU32 NvBufferInputLocation::GetNumberOfComponents()
{
     return COMPONENT_NUMBER_OF_COMPONENTS;
}

NvBufferManagerComponent NvBufferInputLocation::GetComponent()
{
    return Component;
}

NvU32 NvBufferInputLocation::GetPort()
{
    return Port;
}

