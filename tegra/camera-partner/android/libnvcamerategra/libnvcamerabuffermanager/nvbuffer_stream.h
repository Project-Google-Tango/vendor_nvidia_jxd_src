/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVBUFFER_STREAM_H
#define NVBUFFER_STREAM_H

#include "nvbuffer_manager_common.h"
#include "nvbuffer_buffer_info.h"
#include "nvbuffer_driver_interface.h"

/**
* Class NvBufferStream:
* This is the main class that holds collections of buffers and their requirements
* this class is responsible for allocating buffers and sending them to and from components.
 * */
class NvBufferStream
{
friend class NvBufferStreamFactory;

public:
    NvBufferStream();
    virtual ~NvBufferStream();

    /**
    * Returns the output port buffer configuration info.
    * @param location The location from which to retrieve the buffer info.
    * @param bufCfg A non-NULL pointer; buffer configuration info will be returned here.
    * @return NvError, various
    */
    NvError GetOutputPortBufferCfg(NvBufferOutputLocation location,
                                     NvMMNewBufferConfigurationInfo *bufCfg);

    /**
    * Returns the input port buffer configuration info.
    * @param location The location from which to retrieve the buffer info.
    * @param bufCfg A non-NULL pointer; buffer configuration info will be returned here.
    * @return NvError, various
    */
    NvError GetInputPortBufferCfg(NvBufferInputLocation location,
                                     NvMMNewBufferConfigurationInfo *bufCfg);

    /**
    * Iterates through all configured output location and allocates until the
    * defined maxBuffer number is reached, if it cannot be reached but the minBuffer
    * threshold is reached the function will succeed, otherwise a oom error will be
    * returned. Calling this function multiple times will not result in an error, if
    * the min number of buffers are allocated.
    * @return NvError, various
    */
    NvError AllocateAllBuffers();

    /**
    * Allocates the configured buffers at the specified location until the
    * defined maxBuffer number is reached, if it cannot be reached but the minBuffer
    * threshold is reached the function sill succeed, otherwise a oom error will be returned
    * Calling this function multiple times will not result in an error if
    * the min number of buffers are allocated.
    * will be returned.
    * @param location The location where to allocate.
    * @return NvError, various
    */
    NvError AllocateBuffers(NvBufferOutputLocation location);

    /**
    * Returns the buffer allocation status at a specified location.
    * @param location The location where to get data from.
    * @param totalAllocated non-NULL, Will return the number of allocated buffers.
    * @param totalRequested non-NULL, Will return the maxBuffers parameter.
    * @param inUse non-NULL, Returns the number of buffers in use by the driver.
    * @return NvError, various
    */
    NvError GetNumberOfBuffers(NvBufferOutputLocation location,
                                        NvU32 *totalAllocated,
                                        NvU32 *totalRequested,
                                        NvU32 *inUse);

    /**
    * Returns NvMMBuffer pointers from a specified location, the returned
    * buffers must not be in use. Buffer manager still owns the NvMMBuffers
    * and they should not be freed outside buffer manager. Also the buffers
    * returned should pushed to the driver via SendBuffersTo... if the caller
    * of this function is modifying the buffers.
    * @param location, The location where to get data from.
    * @param buffer non-NULL Array where the buffer references will be stored.
    * @param numberRequested The amount of buffers to return.
    * @param numberFilled non-NULL Returns the number of buffers returned.
    * @return NvError, various
    */
    NvError GetUnusedBufferPointers(NvBufferOutputLocation location,
                                        NvMMBuffer *buffer[],
                                        NvU32 numberRequested,
                                        NvU32 *numberFilled);

    /**
    * Returns unused NvMMBuffer pointers from a specified location and marks
    * and marks them used. Buffer manager still owns the NvMMBuffers
    * and they should not be freed outside buffer manager. Also the buffers
    * returned should pushed to the driver via SendBuffersTo... if the caller
    * of this function is modifying the buffers.
    * @param location, The location where to get data from.
    * @param buffer non-NULL Array where the buffer references will be stored.
    * @param numberRequested The amount of buffers to return.
    * @param numberFilled non-NULL Returns the number of buffers returned.
    * @return NvError, various
    */
    NvError UseBufferPointers(NvBufferOutputLocation location,
                                        NvMMBuffer *buffer[],
                                        NvU32 numberRequested,
                                        NvU32 *numberFilled);

    /**
    * Marks NvMMBuffer pointers unused from a specified location and marks
    * and marks them used. Buffer manager still owns the NvMMBuffers
    * and they should not be freed outside buffer manager. Also the buffers
    * returned should pushed to the driver via SendBuffersTo... if the caller
    * of this function is modifying the buffers.
    * @param location, The location where to get data from.
    * @param buffer non-NULL Array where the buffer references will be stored.
    * @param numberRequested The amount of buffers to return.
    * @param numberFilled non-NULL Returns the number of buffers returned.
    * @return NvError, various
    */
    NvError UnuseBufferPointers(NvBufferOutputLocation location,
                                        NvMMBuffer *buffer[],
                                        NvU32 numberRequested,
                                        NvU32 *numberFilled);


    /**
    * Sends allocated buffers to driver.
    * @param component, The component to send the buffers from.
    * @return NvError, various
    */
    NvError SendBuffersToComponent(NvBufferManagerComponent component);

    /**
    * Sends allocated buffers to driver, set buffers to used state.
    * @param location The location where to send buffers from.
    * @return NvError, various
    */
    NvError SendBuffersToLocation(NvBufferOutputLocation location);

    /**
    * Retrive buffers from driver, set buffers to used state.
    * @param component The component where to get buffers from.
    * @return NvError, various
    */
    NvError RecoverBuffersFromComponent(NvBufferManagerComponent component);

    /**
    * Retrive buffers from driver, sets to non used state.
    * @param location The location where to get buffers from
    * @return NvError, various
    */
    NvError RecoverBuffersFromLocation(NvBufferOutputLocation location);

    /**
    * Adjusts the number of buffers at a specified location, function will
    * try to recover the buffers from the driver and will send them back to
    * the driver once the re allocation finishes, it is up to the user to
    * make sure that the buffers are not in use and can be recovered.
    * @param location The location where to adjust buffers.
    * @param requiredMinNumBuffers The minimum number of buffers to allocate.
    * @param requestedNumBuffers The amount of buffers we will try to allocate.
    * @param allocatedNumBuffers non-NULL Returns the actual amount of buffers allocated.
    * @return NvError, various
    */
    NvError SetNumberOfBuffers(NvBufferOutputLocation location,
                                        NvU32 requiredMinNumBuffers,
                                        NvU32 requestedNumBuffers,
                                        NvU32 *allocatedNumBuffers);

    /**
    * Return a list of all locations where buffers are configured
    * @param locations. An array that will hold the locations.
    * @param numberOflocations non-NULL Returns the actual amount locations set.
    * @return NvError, various
    */
    NvError GetAllBufferLocations(NvBufferOutputLocation locations[MAX_TOTAL_OUTPUT_PORTS]
                                        ,NvU32 *numberOflocations);

    /**
    * Frees all buffers that are allocated but not used by the driver.
    * @return NvError, various
    */
    NvError FreeUnusedBuffers();

    /**
    * Frees all buffers that are allocated but not used by the driver in a
    * specific location.
    * @param location from where we will free the buffers
    * @return NvError, various
    */
    NvError FreeBuffersFromLocation(NvBufferOutputLocation location);

private:

    NvStreamBufferConfig        m_BufferStreamCfg;
    NvBool                      m_Initialized;
    NvBufferAllocator           *m_pAllocator;
    NvBufferHandler             *m_pBufferHandler;
    NvBufferStreamType          m_StreamType;

    NvBool  RepurposeBuffers(NvBufferOutputLocation location,
                             NvOuputPortConfig *existingConfig,
                             const NvOuputPortConfig *newConfig);
    NvError ReSizeBufferPool(NvBufferOutputLocation location);
    NvU32   GetBufferIdFromLocation(NvBufferOutputLocation location,
                                    NvU32 index);
    NvError GetLocationFromBufferID(NvU32 bufferID,
                                    NvBufferOutputLocation *location,
                                    NvU32 *index);
    NvOuputPortConfig* GetOutputPortConfig(NvBufferOutputLocation location);
    NvInputPortConfig* GetInputPortConfig(NvBufferInputLocation location);
};






#endif // BUFFERSTREAM_H
