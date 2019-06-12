/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef BUFFERMANAGER_H
#define BUFFERMANAGER_H

#include "nvbuffer_manager_common.h"
#include "nvbuffer_stream_factory.h"
#include "nvbuffer_stream.h"


/**
* Class: NvBufferManager
* Main interface for the buffer manager, this class is responsible
* for the creation and closing of NvBufferStream objects.
* It also configures communication with the driver layer.
*/
class NvBufferManager
{
    static NvBufferManager* m_instance[MAX_NUM_SENSORS];

public:

    /**
    * Creates an instance of the buffer manager, this function
    * cannot fail. Used to access the buffer manager.
    * @param id Camera id of the caller
    * @param driverInfo Provides info on how to talk to the driver.
    * @return BufferManager instance
    */
    static NvBufferManager* Instance(NvU32 id, NvCameraDriverInfo const &driverInfo);

    /**
    * Closes the buffer manager, releases all memory held by the buffer manager
    * This does not release any created NvBufferStreams.
    * @param id Camera id of the caller
    * @return void
    */
    static void Release(NvU32 id);

    /**
    * Closes any open NvBufferStream objects. This will recover and free
    * all buffers and their configurations.
    * @param pStream closes an open NvBufferStream
    * @return NvError, various errors
    */
    NvError CloseStream(NvBufferStream *pStream);

    /**
     * Initializes an allocated stream class, once initialized
     * a stream will be able to allocate buffers and push them
     * to the driver.
     * @param *pStream an allocated stream that is to be initialized
     * @param streamType the type of stream that is to be built.
     * @param streamReq specific configuration of the stream to be built
     * @return NvError, various
    */
    NvError InitializeStream(NvBufferStream *pStream,
                             NvBufferStreamType streamType,
                             NvStreamRequest const &streamReq);

    /**
    * Invalidates the driver info associated with the stream
    * this will also invalidate the buffer manager driver info.
    * Once driver info is invalidated a stream will no longer be
    * able to talk to the driver. Also the buffer manager will no longer
    * be able to initialize streams. This is used to keep buffers alive
    * while the underlying driver is shut down.
    * This needs to be called on any NvBufferStream objects when the underlying
    * driver is destroyed.
    * @param *pStream Referance to an allocated stream.
    * @return NvError, various
    */
    NvError InvalidateDriverInfo(NvBufferStream *pStream);

    /**
    * Reinitialzes the stream and buffer manager with driver info,
    * This is performed if a NvBufferStream is kept alive while the low
    * level driver is destroyed then created again.
    * This will return an error if the stream is not first invalidated
    * @param pStream Pointer to an NvBufferStream object.
    * @param driverInfo Contains driver communication info.
    * @return NvError, various
    */
    NvError ReInitializeDriverInfo(NvBufferStream *pStream,
                                   NvCameraDriverInfo const &driverInfo);

private:

    NvBufferManager();
    ~NvBufferManager();

    NvBufferStreamFactory *m_BufferFactory;
};


#endif // BUFFERSTREAMMANAGER_H
