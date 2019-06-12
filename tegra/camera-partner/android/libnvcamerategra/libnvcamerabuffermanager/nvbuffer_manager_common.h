/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef BUFFERMANAGERCOMMON_H
#define BUFFERMANAGERCOMMON_H

#include "nvmm.h"
#include "nvmm_event.h"
#include "nvmm_util.h"
#include "nvcommon.h"
#include "nverror.h"
#include "nvos.h"
#include "nvrm_surface.h"
#include <utils/threads.h>
#include <utils/Log.h>

#include "runtimelogs.h"

using namespace android;

#define NV_DEBUG_MSG(...) \
    do { \
        ALOGDD(__VA_ARGS__); \
    } while (0)

#define NV_RETURN_FAIL(TYPE) \
    do { ALOGE(" !!!ERROR!!! " #TYPE " in FILE = %s, FUNCTION = %s, LINE = %d", __FILE__,__FUNCTION__,__LINE__); \
    return TYPE; \
    } while (0)

#define NV_ERROR_MSG(MSG) \
    do { ALOGE(" !!!ERROR!!! %s, FILE = %s,  FUNCTION = %s, LINE = %d", MSG, __FILE__,__FUNCTION__,__LINE__); \
    } while (0)

#define MAX_PORTS 5
#define MAX_COMPONENTS 3
#if NV_CAMERA_V3
#define MAX_OUTPUT_BUFFERS_PER_PORT 100
#else
#define MAX_OUTPUT_BUFFERS_PER_PORT 20
#endif
#define MAX_TOTAL_OUTPUT_PORTS MAX_COMPONENTS * MAX_PORTS

#define MAX_NUM_SENSORS 5

/**
* Defines a NvBufferStream type
* This is used when creating the buffer requirements and configurations
* It tells the factory class how to build the NvBufferStream
* CAMERA_STANDARD_CAPTURE defines a NvBufferStream
* with a camera block a dz block with valid buffers on:
* camera->preview
* camera->still
* dz->preview
* dz->still
* dz->video
* If there is a need to change which buffers are allocated and negotiated
* for example for Corona use a new StreamType should be defined
* and the appropriate cases should be added in the factory class to handle
* the creation of the new stream type.
**/
enum NvBufferStreamType
{
    CAMERA_STANDARD_CAPTURE,
    CAMERA_NUMBER_OF_STREAM_TYPES
};

/**
 * This defines what components the buffer manager knows about.
 * Buffer locations are arranged in component->port groups
 * each component can have multiple ports.
 * Ports are split as input and output ports:
 * output ports, contain buffer requirements configurations and buffers
 * input ports, only contain their requirements and are only meant to be
 * used when communicating with the driver and keeping configurations.
**/
enum NvBufferManagerComponent
{
    COMPONENT_CAMERA = 0,   // camera block
    COMPONENT_DZ,           // DZ block
    COMPONENT_HOST,         // host block
    COMPONENT_NUMBER_OF_COMPONENTS
};

/**
 * Defines the output ports for the CAMERA
 * component, output ports hold buffer requirements,
 * buffer configuration, and buffers.
**/
enum NvCameraOutputPort
{
    CAMERA_OUT_PREVIEW = 0,    // camera output 1
    CAMERA_OUT_CAPTURE,        // camera output 2
    CAMERA_OUT_NUMBER_OF_PORTS
};

/**
 * Defines the input ports for the CAMERA
 * These do not hold buffers but hold buffer configuration
 * if required in order to coordinate with the driver.
**/
enum NvCameraInputPort
{
    CAMERA_IN_HOST = 0,        // camera input 1
    CAMERA_IN_NUMBER_OF_PORTS
};

/**
 * Defines the output ports for the COMPONENT_HOST
**/
enum NvHostOutputPort
{
    HOST_OUT_1 = 0,      // host output1
    HOST_OUT_NUMBER_OF_PORTS
};

/**
 * Defines the input ports for the COMPONENT_HOST
**/
enum NvHostInputPort
{
    HOST_IN_NUMBER_OF_PORTS = 0
};


/**
 * Defines the output ports for the COMPONENT_DZ
**/
enum NvDzOutputPort
{
    DZ_OUT_PREVIEW = 0,
    DZ_OUT_STILL,
    DZ_OUT_VIDEO,
    DZ_OUT_THUMBNAIL,
    DZ_OUT_NUMBER_OF_PORTS
};

/**
 * Defines the input ports for the COMPONENT_DZ
**/
enum NvDzInputPort
{
    DZ_IN_PREVIEW = 0,
    DZ_IN_STILL,
    DZ_IN_NUMBER_OF_PORTS
};

/**
 * Defines the pixel formats for buffer negotiation
**/
enum NvFOURCCFormat
{
    FOURCC_YV16 = 1, //YUV422 Planar
    FOURCC_NV16,     //YUV422 Semi-planar U followed by V
    FOURCC_NV61,     //YUV422 Semi-planar V followed by U
    FOURCC_YV12,     //YUV420 Planar
    FOURCC_NV12,     //YUV420 Semi-planar U followed by V
    FOURCC_NV21      //YUV420 Semi-planar V followed by U
};

/**
 * Used as a link between the buffer manager and the underlying driver
 * since the HAL owns the driver blocks this is used to pass
 * blocks communication back to the buffer manager.
 * Since blocks communication is handled by the HAL
 * the conditions defined here are how BufferManager know what
 * configurations are done or if new buffer negotiations are happening.
 **/
typedef struct
{
    Condition *pCondBlockAbortDone;
    Condition *pCondBufferConfigDoneCond;
    Condition *pCondBufferNegotiationDoneCond;
    NvBool    *pBufferConfigDone;
    NvBool    *pBufferNegotiationDone;
    NvMMNewBufferConfigurationInfo *pBuffCfg;
    NvMMNewBufferRequirementsInfo  *pBuffReq;
}NvMMBlockPortConditions;

/**
 * Defines references to the blocks drivers so that the buffer manager can
 * communicate with the blocks.
**/
typedef struct {
    NvMMBlockHandle Cam;
    NvMMBlockHandle DZ;
    NvBool   isCamUSB;
    mutable Mutex       *pLock; //need this in case someone else wants to wait on the events
    NvMMBlockPortConditions DZOutputPorts[DZ_OUT_NUMBER_OF_PORTS];
    NvMMBlockPortConditions CAMOutputPorts[CAMERA_OUT_NUMBER_OF_PORTS];
    NvMMBlockPortConditions DZInputPorts[DZ_IN_NUMBER_OF_PORTS];
} NvCameraDriverInfo;

/**
 * Class NvBufferOutputLocation:
 * This class defines a output location that is used when accessing NvBufferStream
 * for example an output location of:
 *      COMPONENT_CAMERA == component
 *      CAMERA_OUT_PREVIEW == port
 * will access the preview buffers of the camera component, this is used
 * when communicating to the NvBufferStream object.
**/
class NvBufferOutputLocation
{
public:

    /**
    * Constructor
    **/
    NvBufferOutputLocation();

    /**
    * Sets the location of that this class represents.
    * @param component, A valid component location.
    * @param port A valid port for that location.
    * @return NvError, various
    **/
    NvError SetLocation(NvBufferManagerComponent component, NvU32 port);

    /**
    * Returns the component this class represents.
    * @return NvBufferManagerComponent that this class holds
    **/
    NvBufferManagerComponent GetComponent();

    /**
    * Returns the port this class represents.
    * @return port number that this class holds
    **/
    NvU32 GetPort();

    /**
    * Returns the last port number that the component this class
    * is set to contains.
    * @return last port number for this component
    **/
    NvU32 GetLastPortNumber();

    /**
    * Returns the total number of output ports for the provided component.
    **/
    static NvU32 GetNumberOfOuputPorts(NvBufferManagerComponent component);

    /**
    * Returns the total of components.
    **/
    static NvU32 GetNumberOfComponents();

private:
    NvBufferManagerComponent    Component;
    NvU32                       Port;
};

/**
 * Class NvBufferInputLocation:
 * This is a helper class that defines a input location it
 * is used when accessing NvBufferStream
 * for example an input location of:
 *      COMPONENT_CAMERA == component
 *      CAMERA_IN_HOST == port
**/
class NvBufferInputLocation
{
public:

    /**
    * Sets the location of that this class represents.
    * @param component A valid component location.
    * @param port A valid port for that location.
    * @return NvError, various
    **/
    NvError SetLocation(NvBufferManagerComponent component, NvU32 port);

    /**
    * Returns the component this class represents.
    * @return NvBufferManagerComponent that this class holds
    **/
    NvBufferManagerComponent GetComponent();

    /**
    * Returns the port this class represents.
    * @return port number that this class holds
    **/
    NvU32 GetPort();

    /**
    * Returns the last port number that the component this class
    * is set to contains.
    * @return last port number for this component
    **/
    NvU32 GetLastPortNumber();

    /**
    * Returns the total number of input ports for the provided component.
    **/
    NvU32 static GetNumberOfInputPorts(NvBufferManagerComponent component);

    /**
    * Returns the total of components.
    **/
    NvU32 static GetNumberOfComponents();

private:
    NvBufferManagerComponent    Component;
    NvU32                       Port;
};

/**
 * Struct used to define a custom buffer request
 * this is used to set the WxH and number of buffers
 * for a given output port location.
 **/
struct NvBufferRequest
{
    NvU32                   MinBuffers;
    NvU32                   MaxBuffers;
    NvBufferOutputLocation  Location;
    NvU32                   Width;
    NvU32                   Height;

    NvBufferRequest()
    {
        MinBuffers = 0;
        MaxBuffers = 0;
        Width = 0;
        Height = 0;
    }
};

/**
 * Class NvStreamRequest:
 * Class holds a collection of NvBufferRequest
 * is used to define the custom buffer configuration for
 * an NvBufferStream.
  **/
class NvStreamRequest
{
    friend class NvBufferStreamFactory;

public:

    NvStreamRequest();

    /**
     * Adds a NvBuffer Request to the custom configuration.
     * @param request A populated NvBufferRequest.
     * @return NvError, various
     **/
    NvError AddCustomBufferRequest(NvBufferRequest request);

private:

    NvBool  PopBufferRequest(NvBufferRequest *request, NvBufferManagerComponent component);
    NvError PushBufferRequest(NvBufferRequest request, NvBufferManagerComponent component);
    NvS32   m_RequestIndex[MAX_COMPONENTS];
    NvBufferRequest m_RequestStack[MAX_COMPONENTS][MAX_PORTS];
};
#endif
