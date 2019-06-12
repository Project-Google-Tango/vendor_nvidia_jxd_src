/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef NVBUFFER_DRIVER_INTERFACE_H
#define NVBUFFER_DRIVER_INTERFACE_H

#include "nvbuffer_manager_common.h"

/**
* Class NvBufferAllocator: This is a pure virtual class that defines the interface
* to perform buffer allocates/frees.
**/
class NvBufferAllocator
{
public:
    NvBufferAllocator(NvCameraDriverInfo const &driverInfo)
        : m_DriverInfo(driverInfo)
    {}
    virtual ~NvBufferAllocator() { };

    /**
     * Allocates an NvMMbuffer based on location and configuration info.
     * @param location The component and port where the buffers should be allocated.
     * @param bufferCfg The configuration used to allocate the buffers.
     * @param buffer The buffer to be allocated.
     * @return NvError, various
     */
    virtual NvError AllocateBuffer(NvBufferOutputLocation location,
                                    const NvMMNewBufferConfigurationInfo *bufferCfg,
                                    NvMMBuffer **buffer) = 0;

    /**
     * Reconfigures the buffer with the provided buffer configuration, the buffer
     * must be allocated and the caller must ensure that the surfaces are of sufficient
     * size to handle the new configuration.  This is mainly used when we re-purpose
     * buffers.
     * @param location The component and port where the buffers are.
     * @param bufferCfg The configuration to change the buffer to.
     * @param pBuffer The buffer to update with the new configuration.
     * @return NvError, various
     */
    virtual NvError SetBufferCfg(NvBufferOutputLocation location,
                                    const NvMMNewBufferConfigurationInfo *bufferCfg,
                                    NvMMBuffer *pBuffer) = 0;

    /**
     * Frees the buffer, using any location specific free methods.
     * @param location The location to which the buffer belongs.
     * @param buffer The buffer to free.
     * @return NvError, various
     */
    virtual NvError FreeBuffer(NvBufferOutputLocation location, NvMMBuffer *buffer) = 0;

    /**
     * Does any initialization that is required.
     * @return NvError, various
     */
    virtual NvError Initialize() = 0;

    /**
     * Attempt to "repurpose" the existing buffers for a single port.
     * Repurposing means that the existing buffers are used in a new configuration;
     * the underlying surface allocations are left alone, but the surface parameters
     * are updated to the new configuration.
     * @param location The component and port that contains these buffers.
     * @param originalCfg The configuration used when the buffers were originally allocated.
     * @param newCfg The new configuration being requested.
     * @param buffers The existing buffer set.
     * @param bufferCount The number of valid elements in buffers.
     * @return NV_TRUE if the existing buffers have been repurposed.
     * Otherwise, the existing buffers will have to be deallocated and reallocated.
     */
    virtual NvBool  RepurposeBuffers(NvBufferOutputLocation location,
                                     const NvMMNewBufferConfigurationInfo& originalCfg,
                                     const NvMMNewBufferConfigurationInfo& newCfg,
                                     NvOuputPortBuffers *buffers,
                                     NvU32 bufferCount) = 0;
protected:
    NvCameraDriverInfo m_DriverInfo;
};

/**
* Class NvBufferHandler: This is a pure virtual class that defines the interface for how
* to send buffers to and from the driver.  Buffer manager uses this interface to give and
* retrieve buffers from the driver.
**/
class NvBufferHandler
{
public:
    NvBufferHandler(NvCameraDriverInfo const &driverInfo) : m_DriverInfo(driverInfo) {}
    virtual ~NvBufferHandler()  { };

    /**
     * Gives the buffer to the driver at a specific location.
     * @param location The location to which the buffer belongs.
     * @param buffer The buffer to send to driver.
     * @return NvError, various
     */
    virtual NvError GiveBufferToComponent(NvBufferOutputLocation location,
                                          NvMMBuffer *buffer) = 0;

    /**
     * Returns the buffer from the driver at a specific location.
     * @param location The location to which the buffer belongs.
     * @return NvError, various
     */
    virtual NvError ReturnBuffersToManager(NvBufferOutputLocation location) = 0;

protected:
     NvCameraDriverInfo m_DriverInfo;
};


/**
* Class NvBufferConfigurator: This is a pure virtual class that defines the interface for how
* to set driver buffer requirements and how to get buffer configurations from requirements.
* Buffer manager uses this interface to negotiate buffers.
**/
class NvBufferConfigurator
{

public:
    NvBufferConfigurator (NvCameraDriverInfo const &driverInfo) { m_DriverInfo = driverInfo;};
    virtual ~NvBufferConfigurator() { };

    /**
     * Sets any buffer requirements on all output ports of the component, since
     * a components output requirements may depend on other ports of that component
     * we set all of the requirements at 1 time per component.
     * @param component component to process
     * @param pStreamBuffCfg The buffer configuration for that component.
     * @return NvError, various
    **/
    virtual NvError GetOutputRequirements(NvBufferManagerComponent component,
                                          NvComponentBufferConfig *pStreamBuffCfg) = 0;

    /**
     * Sets the buffer Configuration based on the requirements on all output ports of the component,
     * since a components output configurations may depend on other ports of that component
     * we set all of the configurations at 1 time per component.  This call assumes that
     * all buffer requirements for that component are set correctly.
     * @param component The component to process buffer configurations for.
     * @param pStreamBuffCfg The buffer configuration for that component.
     * @return NvError, various
    **/
    virtual NvError GetOutputConfiguration(NvBufferManagerComponent component,
                                          NvComponentBufferConfig *pStreamBuffCfg) = 0;

    /**
     * Sets input requirements for a given component.
     * @param component The component to process input requirements for.
     * @param pStreamBuffCfg The buffer configuration for that component.
     * @return NvError, various
    **/
    virtual NvError GetInputRequirement(NvBufferManagerComponent component,
                                          NvComponentBufferConfig *pStreamBuffCfg) = 0;

    /**
     * Informs the driver that.it should initialize.
     * @return NvError, various
    **/
    virtual NvError ConfigureDrivers() = 0;

protected:

     NvCameraDriverInfo m_DriverInfo;
};




#endif //INTERFACE
