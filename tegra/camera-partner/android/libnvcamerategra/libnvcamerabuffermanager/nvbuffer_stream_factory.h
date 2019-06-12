/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef NVBUFFER_STREAM_FACTORY_H
#define NVBUFFER_STREAM_FACTORY_H

#include "nvbuffer_buffer_info.h"
#include "nvbuffer_manager_common.h"
#include "nvbuffer_stream.h"
#include "nvbuffer_driver_interface.h"

/**
* Class NvBufferStreamFactory:
* This is a factory class used to initialize NvBufferStream classes
* its main responsibilities are to communicate to the driver
* send buffer requirements and receive buffer configurations from the driver
* it is also responsible for reconfiguring the NvBufferStream objects
*/
class NvBufferStreamFactory
{
    static NvBufferStreamFactory* m_instance[MAX_NUM_SENSORS];

public:

    /**
     * Returns an instance the the factory singleton object
     * @param driverInfo A populated driver info object.
     * @param id Camera id of the caller
     * @return a reference to the NvBufferStreamFactory singleton
    **/
    static NvBufferStreamFactory* Instance(NvU32 id, NvCameraDriverInfo const &driverInfo);

    /**
     * Frees the factory singleton and frees any resources associated with it.
     * @param id Camera id of the caller
     * @return void
    **/
    static void Release(NvU32 id);

    /**
     * Initializes an allocated stream class, once initialized
     * a stream will be able to allocate buffers, and push them
     * to the driver.
     * @param *pStream An allocated stream that is to be initialized.
     * @param streamType The type of stream that is to be built.
     * @param streamReq Specific configuration of the stream to be built.
     * @return NvError, various
    */
    NvError InitializeStream(NvBufferStream *stream,
                             NvBufferStreamType streamType,
                             NvStreamRequest const &streamReq);

    /**
    * Invalidates the driver info associated with the stream
    * this will also invalidate the factory class driver info.
    * Once driver info is invalidated a stream will no longer be
    * able to talk to the driver also the buffer manager will no longer
    * be able to initialize streams. This is used to keep buffers alive
    * while the underlying driver is shut down.
    * This needs to be called on any NvBufferStream objects when the underlying
    * driver is destroyed.
    * @param *pStream referance to an allocated stream.
    * @return NvError, various
    */
    NvError InvalidateDriverInfo(NvBufferStream *pStream);

    /**
    * Reinitializes the stream and buffer manager with driver info,
    * This is performed if a NvBufferStream is kept alive while the low
    * level driver is destroyed then created again.
    * This will return an error if the stream is not first invalidated.
    * @param pStream, A pointer to an NvBufferStream object.
    * @param driverInfo The driver communication info.
    * @return NvError, various
    */
    NvError ReInitializeDriverInfo(NvBufferStream *pStream,
                                   NvCameraDriverInfo const &driverInfo);

private:

    NvError ReInitializeStream(NvBufferStream *stream,
                               NvStreamRequest const &streamReq);

    NvError UpdateBufferConfigurations(NvBufferManagerComponent component,
                                NvStreamBufferConfig &NewStreamBufferCfgs,
                                NvBufferStream *stream);

    NvError ConfigureStandardCapture(NvStreamBufferConfig  *pNewStreamBufferCfgs,
                                        NvStreamRequest const &streamReq);

    NvError SetUserGeneratedSettings(NvBufferManagerComponent component,
                                        NvComponentBufferConfig *pStreamBuffCfg,
                                        NvStreamRequest streamReq,
                                        NvBool setUsed);

    NvError ClearAndSetDefaultBufferRequirements(NvMMNewBufferRequirementsInfo *pBufReq);
    NvError ClearAndSetDefaultBufferConfiguration(NvMMNewBufferConfigurationInfo *pCfg);

    NvBufferStreamFactory();
    ~NvBufferStreamFactory();

    NvBufferConfigurator *m_BufferCfg;

    NvCameraDriverInfo m_DriverInfo;
    NvBool m_DriverInitialized;

};







#endif // NVBUFFER_STREAM_FACTORY_H
