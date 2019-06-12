/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** 
 * @file
 * <b>NVIDIA Tegra: OpenMAX Graph Helper Library Graph Interface/b>
 *
 */

#ifndef NVX_GRAPH_H
#define NVX_GRAPH_H

/** defgroup nv_omx_graphhelper_graph Graph Interface
 *
 * This is the NVIDIA OpenMAX Graph Helper Library, Graph Interface.
 * 
 * This graph interface abstracts the concept of an OMX IL graph and 
 * provides a number of setup/helper functions to create and manipulate
 * graphs of IL components.
 *
 * @ingroup nvomx_graphhelper
 * @{
 */

#include "OMX_Component.h"
#include "nvxframework.h"
#include "NVOMX_IndexExtensions.h"

/** Forward declarations of various structures */
struct NvxComponentRec;
struct NvxComponentPrivRec;
struct NvxPortRec;
struct NvxGraphRec;
struct NvxGraphPrivRec;

/** Wrapper around the standard FillBufferDone OMX IL callback.  
 * Uses a NvxComponent instead of an OMX IL Handle, otherwise identical.
 */
typedef OMX_ERRORTYPE (*NvxFillBufferDone)(struct NvxComponentRec *hComponent, 
                                           OMX_PTR pAppData, 
                                           OMX_BUFFERHEADERTYPE* pBuffer);

/** Wrapper around the standard EmptyBufferDone OMX IL callback.  
 * Uses a NvxComponent instead of an OMX IL Handle, otherwise identical.
 */
typedef OMX_ERRORTYPE (*NvxEmptyBufferDone)(struct NvxComponentRec *hComponent, 
                                            OMX_PTR pAppData, 
                                            OMX_BUFFERHEADERTYPE* pBuffer);

/** Wrapper around the standard EventHandler OMX IL callback.
 * Uses a NvxComponent instead of an OMX IL Handle, otherwise identical.
 */
typedef OMX_ERRORTYPE (*NvxEventHandler)(struct NvxComponentRec *hComponent, 
                                         OMX_PTR pAppData, 
                                         OMX_EVENTTYPE eEvent, 
                                         OMX_U32 nData1, OMX_U32 nData2, 
                                         OMX_PTR pEventData);


/** Max number of ports a component may have */
enum { NVX_MAX_COMPONENTPORTS = 8 };

/** Abstracts the concept of an OMX IL component's port
 */
typedef struct NvxPortRec {
    OMX_U32                       portNum;     /**< OMX port number */
    struct NvxComponentRec       *component;   /**< owner of this port */
    OMX_BOOL                      isConnected; /**< if this port is tunneled */
    struct NvxPortRec            *tunnelPort;  /**< what port this is tunneled 
                                                    to, if isConnected is 
                                                    true */
 
    OMX_DIRTYPE                   direction;   /**< output/input port */
    OMX_PORTDOMAINTYPE            domain;      /**< domain of the port */
    OMX_BOOL                      enabled;     /**< if the port is enabled */
    OMX_BUFFERSUPPLIERTYPE        supplierType;/**< this port supplier type */

    union {
        OMX_AUDIO_CODINGTYPE audio;
        OMX_VIDEO_CODINGTYPE video;
        OMX_IMAGE_CODINGTYPE image;
        OMX_OTHER_FORMATTYPE other;
    } formattype;                              /**< type of the port */

} NvxPort;

/** Abstracts the concept of an OMX IL component
 */
typedef struct NvxComponentRec {
    char                    *name;   /**< name of this component in OMX IL */
    OMX_HANDLETYPE           handle; /**< OMX component handle */
    char                    *id;     /**< symbolic id of this component,
                                          defined at creation time */
    OMX_STATETYPE            state;  /**< Current Component state */
    struct NvxGraphRec      *graph;  /**< Graph - owner of this component */

    struct NvxComponentRec  *next;   /**< Next compoennt in Graph's list */

    OMX_U32                  portsNo; /**< number of ports */
    NvxPort                  ports[NVX_MAX_COMPONENTPORTS]; /**< list of ports
                                           filled on component creation. */
                                           

    struct NvxComponentPrivRec  *priv; /**< internal data */
} NvxComponent;

/** Abstracts the concept of an OMX IL graph 
 */
typedef struct NvxGraphRec {
    NvxFramework  framework;      /**< Framework this Graph was created for */
    OMX_STATETYPE state;          /**< Current Graph state */
    NvxComponent *components;     /**< Head of linear list of components. */

    NvxComponent *clock;          /**< clock component, if created at init */

    struct NvxGraphPrivRec *priv; /**< internal data */
} NvxGraph;

/** Initialize a Graph structure for the specified framework
 *
 * @param [in] framework 
 *     The framework created by NvxFrameworkInit earlier
 * @param [out] graph
 *     Pointer to a Graph structure, will be created in this call
 * @param [in] createClock
 *     If true, create a clock for this graph
 * @retval OMX_ERRORTYPE 
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxGraphInit(NvxFramework framework, NvxGraph **graph,
                           OMX_BOOL createClock);

/** Deinitialize a Graph structure, freeing all resources
 *
 * @param [in] graph
 *     Pointer to the NvxGraph structure
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxGraphDeinit(NvxGraph *graph);

/** Set a graph-wide event handler, that will be called with all events
 * sent by the OMX IL components in this graph.
 *
 * @param [in] graph
 *     Pointer to the NvxGraph structure
 * @param [in] handlerData
 *     Pointer that will be returned as part of the event callback
 * @param [in] handler
 *     Pointer to the new event handler function
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxGraphSetGraphEventHandler(NvxGraph *graph,
                                           OMX_PTR handlerData,
                                           NvxEventHandler handler);


/** Create a component with given OMX IL name and symbolic component ID
 * 
 * @param [in] graph                       
 *     Pointer to the NvxGraph structure
 * @param [in] componentName
 *     OMX IL name of the component to create
 * @param [in] componentId
 *     A caller specified identifer, should be something like "CAMERA" or 
 *     "AUDIODECODER" to allow identification later on via NvxLookupComponent
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxGraphCreateComponentByName(NvxGraph *graph,
                                            const char *componentName,
                                            const char *componentId,
                                            NvxComponent **component);

/** Create a component with given OMX IL role and symbolic component ID
 * 
 * @param [in] graph                       
 *     Pointer to the NvxGraph structure
 * @param [in] componentRole
 *     OMX IL standard component role of the component to create
 * @param [in] componentId
 *     A caller specified identifer, should be something like "CAMERA" or 
 *     "AUDIODECODER" to allow identification later on via NvxLookupComponent
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxGraphCreateComponentByRole(NvxGraph *graph,
                                            const char *componentRole,
                                            const char *componentId,
                                            NvxComponent **component);


/** Remove a component from the graph.
 * 
 * The graph must be in such a state that the component can be successfully
 * removed.  The component must be in loaded, and not tunnelled with other 
 * components. 
 *
 * @param [in] graph                        
 *     Pointer to the NvxGraph structure    
 * @param [in] component
 *     Pointer to the NvxComponent to remove
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxGraphRemoveComponent(NvxGraph *graph,
                                      NvxComponent *component);

/** Look up an existing graph component by its ID.
 * 
 * @param [in] graph
 *     Pointer to the NvxGraph structure
 * @param [in] compId
 *     Caller specified unique identifier, set at component creation time 
 * @retval NvxComponent*
 *     The component, if found.  NULL if not.
 */
NvxComponent *NvxGraphLookupComponent(NvxGraph *graph,
                                      const char *compId);

/** Tell the graph that a component is a valid endpoint.  OMX_EventBufferFlag
 * events with OMX_BUFFERFLAG_EOS flags set will trigger the
 * NvxGraphWaitForEndOfStream() function, if waiting on it.
 *
 * Multiple components may be endpoints - in that case, all components must
 * receive the EOS event to trigger NvxGraphWaitForEndOfStream
 *
 * @param [in] graph
 *     Pointer to the NvxGraph structure
 * @param [in] component
 *     Pointer to the NvxComponent structure
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxGraphSetComponentAsEndpoint(NvxGraph *graph,
                                             NvxComponent *component);

/** Transition a single component to the requested state
 *
 * @param [in] component
 *     Pointer to the NvxComponent structure
 * @param [in] newState
 *     The component's new state
 * @param timeOutMillisecs 
 *     Timeout for this state transition
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxGraphTransitionComponentToState(NvxComponent *component,
                                                 OMX_STATETYPE newState,
                                                 OMX_U32 timeOutMillisecs);


/** Set the per-component emptybuffer/fillbuffer done callbacks
 *
 * @param [in] component
 *     Pointer to the NvxComponent structure
 * @param [in] fillBufData
 *     Pointer to data to be returned in the callback
 * @param [in] fillBuffer
 *     Pointer to function to use for the fillBufferDone callback
 * @param [in] emptyBufData
 *     Pointer to data to be returned in the callback
 * @param [in] emptyBuffer
 *     Pointer to function to use for the emptyBufferDone callback
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxGraphSetCompBufferCallbacks(NvxComponent *component,
                                             OMX_PTR fillBufData,
                                             NvxFillBufferDone fillBuffer,
                                             OMX_PTR emptyBufData,
                                             NvxEmptyBufferDone emptyBuffer);

/** Get the emptybuffer/fillbuffer done callbacks
 *   which are currently registered for this component
 *
 * @param [in] component
 *     Pointer to the NvxComponent structure
 * @param [out] fillBufData
 *     Pointer to data to be returned in the callback
 * @param [out] fillBuffer
 *     Pointer to function used for the fillBufferDone callback
 * @param [out] emptyBufData
 *     Pointer to data to be returned in the callback
 * @param [out] emptyBuffer
 *     Pointer to function used for the emptyBufferDone callback
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxGraphGetCompBufferCallbacks(NvxComponent *component,
                                             OMX_PTR *fillBufData,
                                             NvxFillBufferDone *fillBuffer,
                                             OMX_PTR *emptyBufData,
                                             NvxEmptyBufferDone *emptyBuffer);

/** Set the per-component event handler callback
 *
 * @param [in] component
 *     Pointer to the NvxComponent structure
 * @param [in] handlerData
 *     Pointer to be returned in the event callback
 * @param [in] handler
 *     Pointer to function to use for the event handler callback
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxGraphSetCompEventHandler(NvxComponent *component,
                                          OMX_PTR handlerData,
                                          NvxEventHandler handler);


/** Transition the entire Graph to the requested state
 *
 * @param [in] graph
 *     Pointer to the NvxGraph structure
 * @param [in] newState
 *     The graph's new state
 * @param timeOutMillisecs 
 *     Timeout for this state transition, per component
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxGraphTransitionAllToState(NvxGraph *graph,
                                           OMX_STATETYPE newState,
                                           OMX_U32 timeOutMillisecs);


/** Flush all components
 *
 * @param [in] graph
 *     Pointer to the NvxGraph structure
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxGraphFlushAllComponents(NvxGraph *graph);

/** Wait for an EndOfStream buffer flag (or some select other events), 
 * or time out.
 * 
 * @param [in] graph
 *     Pointer to the NvxGraph structure
 * @param [in] timeOutMillisecs
 *     Maximum length of time to wait
 */
void NvxGraphWaitForEndOfStream(NvxGraph *graph, OMX_U32 timeOutMillisecs);

/** Returns the end of stream status.
 * 
 * @param [in] graph
 *     Pointer to the NvxGraph structure
 * @retval OMX_BOOL
 *     If the graph is at EOS or not
 */
OMX_BOOL NvxGraphIsAtEndOfStream(NvxGraph *graph);

/** Wake up anything waiting on NvxGraphWaitForEndOfStream
 *
 * @param [in] graph
 *     Pointer to the NvxGraph structure
 */
void NvxGraphSignalEndOfStreamEvent(NvxGraph *graph);

/** Force EOS status - will not wake up anything waiting in 
 * NvxGraphWaitForEndOfStream
 *
 * @param [in] graph
 *     Pointer to the NvxGraph structure
 * @param [in] eos
 *     New end of stream status for the graph
 */
void NvxGraphSetEndOfStream(NvxGraph *graph, OMX_BOOL eos);

/** Tell the graph to wait for state conditions when receiving an error from 
 * a component in the graph (or not to wait)
 *
 * @param [in] graph
 *     Pointer to the NvxGraph structure
 * @param [in] waitOnError
 *     If the graph should wait on an error or not
 */
void NvxGraphSetWaitForErrorsOnState(NvxGraph *graph, OMX_BOOL waitOnError);

/** Return the error type if we've ever received one from a component
 *
 * @param [in] graph
 *     Pointer to the NvxGraph structure
 * @retval OMX_ERRORTYPE
 *     Most recent error type, if any, received from any component in the graph
 */
OMX_ERRORTYPE NvxGraphGetError(NvxGraph *graph);

/** Clear any stored error types received by the graph
 * 
 * @param [in] graph
 *     Pointer to the NvxGraph structure
 */
void NvxGraphClearError(NvxGraph *graph);

/** Complete Graph teardown and deletion of all components
 *
 * @param [in] graph
 *     Pointer to the NvxGraph structure
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */ 
OMX_ERRORTYPE NvxGraphTeardown(NvxGraph *graph);

/** Establish tunneled connection between (compA, portA) and (compB, portB)
 * 
 * portA should be an output port, port B should be an input port
 *
 * @param [in] compA
 *     First component to connect
 * @param [in] portA
 *     The port on compA to connect
 * @param [in] portB
 *     Second component to connect
 * @param [in] portB
 *     The port on compB to connect
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxConnectTunneled(NvxComponent *compA, OMX_U32 portA,
                                 NvxComponent *compB, OMX_U32 portB);

/** Remove tunneled connection for (compA, portA)
 *   Note that in order to properly break tunneled connection
 *   between two ports: (compA,portA) and (compB,portB)
 *   NvxSetUntunneled must be called explicitly for both ports.
 * 
 * @param [in] compA
 *     Component to disconnect
 * @param [in] portA
 *     The port on compA to disconnect
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxSetUntunneled(NvxComponent *compA, OMX_U32 portA);

/** Wait until port transitions to enabled/disabled state
 *
 * @param [in] port
 *     port to wait for
 * @param [in] enable
 *     OMX_TRUE to wait for "enabled" port state,
 *     OMX_FALSE to wait for "disabled" state
 * @param [in] portTimeOut
 *     wait timeout in millisecs
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxWaitForPortState(NvxPort *port, OMX_BOOL enable,
                            OMX_U32 portTimeOut);


/** Enable or disable port
 *   without changing the state of whole OMX component
 *
 * @param [in] port
 *     port to enable/disable
 * @param [in] enable
 *     OMX_TRUE to enable ports, OMX_FALSE to disable
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxEnablePort(NvxPort *port, OMX_BOOL enable);


/** Enable or disable tunneled port 
 *   without changing the state of whole OMX component
 * 
 * port is expected to be tunneled, and both ports of this tunneled connection
 * will be enabled or disabled.
 *
 * @param [in] port
 *     port to enable/disable
 * @param [in] enable
 *     OMX_TRUE to enable ports, OMX_FALSE to disable
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxEnableTunneledPort(NvxPort *port, OMX_BOOL enable);

/** Connect a component to the clock
 *
 * This will automatically find the clock port of a component, enable it 
 * (if necessary), and connect it to the next available port of the graph's
 * clock component if it exists.
 *
 * @param [in] comp
 *     Component to connect
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxConnectComponentToClock(NvxComponent *comp);

/** Disconnect a component from the clock
 * 
 * Not yet implemented.
 *
 * @param [in] comp
 *     Component to disconnect
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxDisconnectComponentFromClock(NvxComponent *comp);

/** For non-tunneled ports, automatically allocate enough buffers to 
 * satisfy the requirements of the port.
 * 
 * Buffer pointers may be accessed via NvxGetAllocatedBuffers after this
 * NvxAllocateBuffersForPort returns successfully.
 *
 * Must call NvxFreeBuffersForPort to clean up before freeing the component
 *
 * @param [in] comp
 *     Pointer to the component
 * @param [in] port
 *     Port number to allocate buffers on
 * @param [out] bufCount
 *     Returns the number of buffers allocated.
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxAllocateBuffersForPort(NvxComponent *comp, OMX_U32 port,
                                        OMX_U32 *bufCount);

/** Fill in the 'buffers' array with the list of buffers allocated by
 * NvxAllocateBuffersForPort().  bufCount must equal the bufCount returned
 * by that function.  The 'buffers' array is allocated by the caller, but
 * buffers are still owned by the component itself.
 * 
 * @param [in] comp
 *     Pointer to the component
 * @param [in] port
 *     Port number to allocate buffers on
 * @param [out] buffers
 *     Pointer to an array of buffers to be filled in
 * @param [in] bufCount
 *     Size of the buffers array - must equal the buffer count returned earlier
 *     by NvxAllocateBuffersForPort
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxGetAllocatedBuffers(NvxComponent *comp, OMX_U32 port,
                                     OMX_BUFFERHEADERTYPE **buffers,
                                     OMX_U32 bufCount);

// FIXME: add something for using external buffers

/** Free all buffers previously allocated with NvxAllocateBuffersForPort
 *
 * @param [in] comp
 *     Pointer to the component
 * @param [in] port
 *     Port number to free buffers on
 * @retval OMX_ERRORTYPE
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxFreeBuffersForPort(NvxComponent *comp, OMX_U32 port);

/** Start the clock at the specified time
 * 
 * @param [in] graph
 *     Pointer to the graph
 * @param [in] timeInMillisec
 *     Time to start the clock at
 */
void NvxGraphStartClock(NvxGraph *graph, OMX_TICKS timeInMilliSec);

/** Stop the clock
 * 
 * @param [in] graph
 *     Pointer to the graph
 */
void NvxGraphStopClock(NvxGraph *graph);

/** Pause or unpause the clock
 * 
 * @param [in] graph
 *     Pointer to the graph
 * @param [in] pause
 *     Pause or unpause the graph
 */
void NvxGraphPauseClock(NvxGraph *graph, OMX_BOOL pause);

/** Get the current time from the clock, in OMX_TICKS units
 * 
 * @param [in] graph
 *     Pointer to the graph
 * @retval OMX_TICKS
 *     Current time of the graph
 */
OMX_TICKS NvxGraphGetClockTime(NvxGraph *graph);

/** setRate of Play for all components
 *
 * @param [in] graph
 *     Pointer to the graph
 * @param [in] playspeed
 *     Speed at which components need to run
 */
OMX_ERRORTYPE NvxGraphSetRate(NvxGraph *graph, float playSpeed);

/** Free a Component from the Graph
 *
 * @param [in] Component
 *     Pointer to the Component
 */
void GraphFreeComp(NvxComponent *comp);

#endif

/** @} */
/* File EOF */
