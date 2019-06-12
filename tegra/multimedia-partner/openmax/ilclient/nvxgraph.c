/*
 * Copyright (c) 2010-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvxframework_int.h"
#include "nvxgraph.h"
#include "nvmm_logger.h"
#include "nvfxmath.h"

/*
 * Helper macro to init an OMX IL param/config struct
 */
#define INIT_PARAM(a, b) do { NvOsMemset(&(a), 0, sizeof((a))); (a).nSize = sizeof((a)); (a).nVersion = NvxFrameworkGetOMXILVersion((b)); } while (0)

/* 
 * Helper macro to compute time difference between "from" and "to" readings
 * taking into account possible 32-bit values wrap-around.
 * "to" reading is assumed to be taken later than "from" reading.
 */
#define OMX_TIMEDIFF(from,to) \
           (((from) <= (to))? ((to)-(from)):((NvU32)~0-((from)-(to))+1))

typedef struct NvxPortBuffersRec
{
    OMX_BUFFERHEADERTYPE **List;
    OMX_U32                Count;
    NvOsMutexHandle        hMutex;
} NvxPortBuffers;

/*
 * Internal data for a NvxComponent
 */
typedef struct NvxComponentPrivRec
{
    NvxComponent *parent;

    // End of stream status
    OMX_BOOL isEndComponent;
    OMX_BOOL seenEOSEvent;

    // Store a list of any allocated buffers used by non-tunneled communication
    NvxPortBuffers         buffers[NVX_MAX_COMPONENTPORTS];

    // per-component callbacks
    NvxFillBufferDone   fillBufferDone;
    void               *fillBufferDoneData;

    NvxEmptyBufferDone  emptyBufferDone;
    void               *emptyBufferDoneData;

    NvxEventHandler     eventHandler;
    void               *eventHandlerData;

} NvxComponentPriv;

/*
 * Internal data for the NvxGraph struct
 */
typedef struct NvxGraphPrivRec
{
    NvxGraph *parent;

    // End of stream status variables.  needEOSEvents counts down to 0 as
    // the events are received
    OMX_BOOL atEOS;
    int needEOSEvents;
    int maxEOSEvents;

    // Store any errors sent as events
    OMX_ERRORTYPE errorEvent;
    // If waitOnError is true, don't bail out of waiting for state changes on
    // errors
    OMX_BOOL waitOnError;

    // semaphores to signal various events
    NvOsSemaphoreHandle flushEvent;
    NvOsSemaphoreHandle mainEndEvent;
    NvOsSemaphoreHandle generalEvent;
    NvOsSemaphoreHandle portEvent;
    
    NvOsMutexHandle hStateMutex;

    // callbacks to give to OMX IL components
    OMX_CALLBACKTYPE callbacks;

    // graph specific event handler
    NvxEventHandler     eventHandler;
    void               *eventHandlerData;

} NvxGraphPriv;

/*
 * Internal default event handler.  Deals with state changing, flushing, errors,
 * EOS, etc
 *
 */
static OMX_ERRORTYPE Graph_EventHandler(OMX_HANDLETYPE hComponent,
                                        OMX_PTR pAppData,
                                        OMX_EVENTTYPE eEvent, OMX_U32 nData1,
                                        OMX_U32 nData2, OMX_PTR pEventData)
{
    OMX_BOOL sendGeneralEvent = OMX_TRUE;
    NvxComponentPriv *comp = (NvxComponentPriv *)pAppData;
    NvxGraphPriv *graph = NULL;

    if (!comp) {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_WARN, "WARNING --%s[%d]",
            __FUNCTION__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    graph = comp->parent->graph->priv;

    switch (eEvent)
    {
        case OMX_EventCmdComplete:
            //NvOsDebugPrintf("Graph_EventHandler: Cmd Complete for %s [%d]\n",comp->parent->id,nData1);
            if (OMX_CommandFlush == nData1 && OMX_ALL == nData2)
                NvOsSemaphoreSignal(graph->flushEvent);
            if (OMX_CommandPortDisable == nData1 || 
                OMX_CommandPortEnable == nData1)
            {
                if (graph->portEvent != NULL)
                {
                    NvOsSemaphoreSignal(graph->portEvent);
                }
            }
            break;

        case OMX_EventError:
            NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR Graph_EventHandler: "
                "OMX_EventError for %s [0x%0x]", comp->parent->id, nData1));
            if ((OMX_ErrorPortUnpopulated == (OMX_S32)nData1) || \
                (NvxError_WriterFileSizeLimitExceeded == (NvxError)nData1) ||
                (NvxError_WriterTimeLimitExceeded == (NvxError)nData1) ||
                (OMX_ErrorNotReady == (OMX_S32)nData1))
            {
                // ignore this error event, we don't really care about it
                sendGeneralEvent = OMX_FALSE;
                break;
            }

            if (nData1)
            {
                NvOsDebugPrintf("Graph_EventHandler: ERROR for %s [0x%0x]\n",comp->parent->id,nData1);
                graph->errorEvent = (OMX_ERRORTYPE)nData1;
            }
            else
            {
                // treat all other errors like an EOS
                graph->atEOS = OMX_TRUE;
                graph->needEOSEvents = 0;
            }
            NvOsSemaphoreSignal(graph->mainEndEvent);
            break;

        case OMX_EventBufferFlag:
            if (nData2 & OMX_BUFFERFLAG_EOS)
            {
                // if this component is an endpoint, then decrease the
                // graph's needEOSEvent counter
                if (comp->isEndComponent && !comp->seenEOSEvent)
                {
                    comp->seenEOSEvent = OMX_TRUE;
                    graph->needEOSEvents--;
                }

                // once the EOSEvent counter is at zero, we're done
                if (0 == graph->needEOSEvents)
                {
                    graph->atEOS = OMX_TRUE;
                    NvOsSemaphoreSignal(graph->mainEndEvent);
                }
            }
            break;

        default:
            break;
    }

    // call component and graph specific handlers, if any
    if (comp->eventHandler)
        comp->eventHandler(comp->parent, comp->eventHandlerData,
                           eEvent, nData1, nData2, pEventData);

    if (graph->eventHandler)
        graph->eventHandler(comp->parent, graph->eventHandlerData,
                            eEvent, nData1, nData2, pEventData);

    // signal the general event semaphore
    if (sendGeneralEvent) {
        //NvOsDebugPrintf("Graph_EventHandler: Sem signalled for %s \n",comp->parent->id);
        NvOsSemaphoreSignal(graph->generalEvent);
    }

    return OMX_ErrorNone;
}

// Generic callback for all OMX components
static OMX_ERRORTYPE Graph_EmptyBufferDone(OMX_HANDLETYPE hComponent,
                                           OMX_PTR pAppData,
                                           OMX_BUFFERHEADERTYPE* pBuffer)
{
    NvxComponentPriv *comp = (NvxComponentPriv *)pAppData;

    if (!comp)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return OMX_ErrorNotImplemented;
    }

    // call the component specific empty buffer done, if it exists
    if (comp->emptyBufferDone)
        return comp->emptyBufferDone(comp->parent, comp->emptyBufferDoneData, 
                                     pBuffer);

    return OMX_ErrorNotImplemented;
}

// Generic callback for all OMX components
static OMX_ERRORTYPE Graph_FillBufferDone(OMX_HANDLETYPE hComponent,
                                          OMX_PTR pAppData,
                                          OMX_BUFFERHEADERTYPE* pBuffer)
{
    NvxComponentPriv *comp = (NvxComponentPriv *)pAppData;

    if (!comp)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_WARN, "WARNING --%s[%d]",
            __FUNCTION__, __LINE__));
        return OMX_ErrorNotImplemented;
    }

    // call the component specific fill buffer done, if it exists
    if (comp->fillBufferDone)
        return comp->fillBufferDone(comp->parent, comp->fillBufferDoneData, 
                                    pBuffer);

    return OMX_ErrorNotImplemented;
}


/*
 * Deinit buffer
 */
static OMX_ERRORTYPE NvxDeinitBufferForPort(NvxComponent *comp, OMX_U32 port)
{
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    NvxPortBuffers *pBuf = NULL;

    if (!comp || port >= comp->portsNo)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    pBuf = &comp->priv->buffers[port];

    if (pBuf->hMutex)
        NvOsMutexDestroy(pBuf->hMutex);

    return omxErr;
}

/*
 * Delete all resources associated with a component
 */
void GraphFreeComp(NvxComponent *comp)
{
    OMX_U32 port;

    if (!comp)
        return;

    // If this was an end component, remove it from the master count of
    // end components
    if (comp->priv->isEndComponent)
    {
        if (!comp->priv->seenEOSEvent)
            comp->graph->priv->needEOSEvents--;
        comp->graph->priv->maxEOSEvents--;
    }

    // free any buffers we allocated, that the client forgot to call 
    // FreeBuffersForPort on
    for (port = 0; port < comp->portsNo; port++)
    {
        NvxFreeBuffersForPort(comp, port);
        NvxDeinitBufferForPort(comp, port);
    }

    // Close the component
    if (comp->handle)
        comp->graph->framework->O_FreeHandle(comp->handle);

    // Free stuff
    NvOsFree(comp->name);
    NvOsFree(comp->id);
    NvOsFree(comp->priv);
    NvOsFree(comp);
}

/*
 * Delete all components from a graph
 */
static void GraphFreeAllComps(NvxGraph *graph)
{
    NvxComponent *comp, *next;

    comp = graph->components;
    while (comp)
    {
        next = comp->next;
        GraphFreeComp(comp);
        comp = next;
    }

    graph->components = NULL;
}

/*
 * Wait for a state transition to complete, with timeout
 */
static OMX_ERRORTYPE GraphWaitForState(NvxGraphPriv *grpriv, NvxComponent *comp,
                                       OMX_STATETYPE newState, OMX_U32 timeOut)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_STATETYPE curState;
    NvError semerr = NvSuccess;

    err = OMX_GetState(comp->handle, &curState);
    if (OMX_ErrorNone != err)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_WARN, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return err;
    }

    while (curState != newState && curState != OMX_StateInvalid)
    {
        // This may actually end up taking longer than timeOut MS to time-out,
        // due to other events waking up the semaphore wait
        if (timeOut != (OMX_U32)-1)
        {
            semerr = NvOsSemaphoreWaitTimeout(grpriv->generalEvent, timeOut);
            if (NvError_Timeout == semerr)
            {
                NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR,
                    "GraphWaitState: WaitTimeOut error %s", comp->name));
                return OMX_ErrorTimeout;
            }
        }
        else
            NvOsSemaphoreWait(grpriv->generalEvent);

        err = OMX_GetState(comp->handle, &curState);
        if (OMX_ErrorNone != err)
        {
            NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "%s:ERROR --%s[%d]",
                comp->name, __FUNCTION__, __LINE__));
            return err;
        }

        // bail out if we've gotten a graph error event, and we're not 
        // waiting on errors
        if (OMX_ErrorNone != grpriv->errorEvent && !grpriv->waitOnError)
        {
            NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR %s --%s[%d]",
                comp->name, __FUNCTION__, __LINE__));
            return grpriv->errorEvent;
        }
    }

    return err;
}

/*********************** EXTERNAL INTERFACE START *********************/

/*
 * Initialize a Graph structure and create a clock component if asked
 */
OMX_ERRORTYPE NvxGraphInit(NvxFramework framework, NvxGraph **graph,
                           OMX_BOOL createClock)
{
    NvxGraph *gr = NULL;
    NvxGraphPriv *priv = NULL;

    if (!framework || !graph)
        return OMX_ErrorBadParameter;

    // Allocate structures
    *graph = NvOsAlloc(sizeof(NvxGraph));
    if (!*graph)
        return OMX_ErrorInsufficientResources;

    gr = *graph;

    NvOsMemset(gr, 0, sizeof(NvxGraph));

    gr->priv = NvOsAlloc(sizeof(NvxGraphPriv));
    if (!gr->priv)
    {
        NvOsFree(gr);
        *graph = NULL;
        return OMX_ErrorInsufficientResources;
    }

    NvOsMemset(gr->priv, 0, sizeof(NvxGraphPriv));

    priv = gr->priv;

    // Set defaults
    gr->framework = framework;
    gr->state = OMX_StateLoaded;
    priv->parent = gr;
    priv->atEOS = OMX_FALSE;
    priv->needEOSEvents = 0;
    priv->maxEOSEvents = 0;
    priv->errorEvent = OMX_ErrorNone;
    priv->waitOnError = OMX_FALSE;

    // Set the default callbacks
    priv->callbacks.EventHandler    = Graph_EventHandler;
    priv->callbacks.EmptyBufferDone = Graph_EmptyBufferDone;
    priv->callbacks.FillBufferDone  = Graph_FillBufferDone;

    // Create all necessary semaphores
    if (NvSuccess != NvOsSemaphoreCreate(&priv->flushEvent, 0))
        return OMX_ErrorInsufficientResources;

    if (NvSuccess != NvOsSemaphoreCreate(&priv->mainEndEvent, 0))
        return OMX_ErrorInsufficientResources;

    if (NvSuccess != NvOsSemaphoreCreate(&priv->generalEvent, 0))
        return OMX_ErrorInsufficientResources;
    
    if (NvSuccess != NvOsMutexCreate(&priv->hStateMutex))
        return OMX_ErrorInsufficientResources;

    if (NvSuccess != NvOsSemaphoreCreate(&priv->portEvent, 0))
        return OMX_ErrorInsufficientResources;

    // Create a clock component for the graph, if requested
    if (createClock)
    {
        NvxGraphCreateComponentByRole(gr, "clock.binary", "clock", &gr->clock);
        if (gr->clock)
        {
            OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE activeClock;
            OMX_U32 i;

            // disable all clock ports by default.  re-enable as needed
            OMX_SendCommand(gr->clock->handle, OMX_CommandPortDisable, 
                            (OMX_U32)-1, 0);
            for (i = 0; i < gr->clock->portsNo; i++)
                gr->clock->ports[i].enabled = OMX_FALSE;

            // audio is the master, always
            INIT_PARAM(activeClock, gr->framework);
            activeClock.eClock = OMX_TIME_RefClockAudio;
            OMX_SetConfig(gr->clock->handle, OMX_IndexConfigTimeActiveRefClock,
                          &activeClock);
        }
    }

    return OMX_ErrorNone;
}

/*
 * Free all data associated with the NvxGraph structure.
 */
OMX_ERRORTYPE NvxGraphDeinit(NvxGraph *graph)
{
    if (!graph)
        return OMX_ErrorBadParameter;

    // FIXME: bail if not all in loaded?

    GraphFreeAllComps(graph);

    NvOsSemaphoreDestroy(graph->priv->flushEvent);
    NvOsSemaphoreDestroy(graph->priv->mainEndEvent);
    NvOsSemaphoreDestroy(graph->priv->generalEvent);
    NvOsSemaphoreDestroy(graph->priv->portEvent);
    
    if (graph->priv->hStateMutex)
        NvOsMutexDestroy(graph->priv->hStateMutex);

    NvOsFree(graph->priv);
    NvOsFree(graph);

    return OMX_ErrorNone;
}

/*
 * Set the graph-specific event handler
 */
OMX_ERRORTYPE NvxGraphSetGraphEventHandler(NvxGraph *graph,
                                           OMX_PTR handlerData,
                                           NvxEventHandler handler)
{
    if (!graph)
        return OMX_ErrorBadParameter;

    graph->priv->eventHandlerData = handlerData;
    graph->priv->eventHandler     = handler;

    return OMX_ErrorNone;
}

/*
 * Create a component and add it to the graph
 */
OMX_ERRORTYPE NvxGraphCreateComponentByName(NvxGraph *graph,
                                            const char *componentName,
                                            const char *componentId,
                                            NvxComponent **component)
{
    NvxComponent *comp = NULL, *iter = NULL;
    OMX_HANDLETYPE handle;
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_ERRORTYPE perr = OMX_ErrorNone;
    OMX_U32 len, i;

    if (!graph || !componentName || !component)
        return OMX_ErrorBadParameter;

    // Allocate component structs
    comp = NvOsAlloc(sizeof(NvxComponent));
    if (!comp)
    {
        err = OMX_ErrorInsufficientResources;
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        goto reterr;
    }

    NvOsMemset(comp, 0, sizeof(NvxComponent));

    comp->priv = NvOsAlloc(sizeof(NvxComponentPriv));
    if (!comp->priv)
    {
        err = OMX_ErrorInsufficientResources;
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        goto reterr;
    }

    NvOsMemset(comp->priv, 0, sizeof(NvxComponentPriv));

    comp->priv->parent = comp;

    // Try to get the OMX component
    err = graph->framework->O_GetHandle(&handle, (OMX_STRING)componentName, 
                                        comp->priv, &graph->priv->callbacks);
    if (OMX_ErrorNone != err)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        goto reterr;
    }

    // Set various defaults
    len = NvOsStrlen(componentName);
    comp->name = NvOsAlloc(len + 1);
    if (!comp->name)
    {
        err = OMX_ErrorInsufficientResources;
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        goto reterrcomp;
    }
    NvOsStrncpy(comp->name, componentName,  len + 1);

    if (componentId)
    {
        len = NvOsStrlen(componentId);
        comp->id = NvOsAlloc(len + 1);
        if (!comp->id)
        {
            err = OMX_ErrorInsufficientResources;
            NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
                __FUNCTION__, __LINE__));
            goto reterrcomp;
        }
        NvOsStrncpy(comp->id, componentId,  len + 1);
    }

    comp->handle = handle;
    comp->state = OMX_StateLoaded;
    comp->graph = graph;

    i = 0;
    // probe all the component ports
    while (OMX_ErrorNone == err && comp->portsNo < NVX_MAX_COMPONENTPORTS)
    {
        OMX_PARAM_PORTDEFINITIONTYPE portDef;
        OMX_PARAM_BUFFERSUPPLIERTYPE portSupplierType;

        INIT_PARAM(portDef, graph->framework);
        portDef.nPortIndex = i;

        // collect interesting port data
        err = OMX_GetParameter(handle, OMX_IndexParamPortDefinition, &portDef);
        if (OMX_ErrorNone == err)
        {
            comp->ports[i].portNum = i;
            comp->ports[i].component = comp;
            comp->ports[i].isConnected = OMX_FALSE;
            comp->ports[i].tunnelPort = NULL;
            comp->ports[i].direction = portDef.eDir;
            comp->ports[i].domain = portDef.eDomain;
            comp->ports[i].enabled = portDef.bEnabled;

            switch (portDef.eDomain)
            {
                case OMX_PortDomainAudio: 
                    comp->ports[i].formattype.audio = 
                                portDef.format.audio.eEncoding;
                    break;
                case OMX_PortDomainVideo:
                    comp->ports[i].formattype.video = 
                                portDef.format.video.eCompressionFormat;
                    break;
                case OMX_PortDomainImage:
                    comp->ports[i].formattype.image =
                                portDef.format.image.eCompressionFormat;
                    break;
                case OMX_PortDomainOther:
                    comp->ports[i].formattype.other =
                                portDef.format.other.eFormat;
                    break;
                default:
                    break;
            }

            INIT_PARAM(portSupplierType, graph->framework);
            portSupplierType.nPortIndex = i;
            perr = OMX_GetParameter(handle,
                        OMX_IndexParamCompBufferSupplier,
                        &portSupplierType);
            if (perr == OMX_ErrorNone)
                comp->ports[i].supplierType = portSupplierType.eBufferSupplier;
            else
                comp->ports[i].supplierType = OMX_BufferSupplyUnspecified;

            comp->portsNo++;
            i++;
        }
    }

    // add the component to the graph
    iter = graph->components;
    if (!iter)
        graph->components = comp;
    else
    {
        while (iter)
        {
            if (!iter->next)
            {
                iter->next = comp;
                break;
            }
            iter = iter->next;
        }
    }

    *component = comp;
    err = OMX_ErrorNone;
    return err;

reterrcomp:
    graph->framework->O_FreeHandle(handle);
    NvOsFree(comp->name);
    NvOsFree(comp->id);

reterr:
    NvOsFree(comp->priv);
    NvOsFree(comp);

    return err;
}

/*
 * Find a component by role, not name
 */
OMX_ERRORTYPE NvxGraphCreateComponentByRole(NvxGraph *graph,
                                            const char *componentRole,
                                            const char *componentId,
                                            NvxComponent **component)
{
    OMX_U8 *name;
    OMX_U32 numComp = 1;
    OMX_ERRORTYPE err;

    if (!graph || !componentRole || !component)
        return OMX_ErrorBadParameter;

    name = NvOsAlloc(128); // max size per omx spec
    if (!name)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return OMX_ErrorInsufficientResources;
    }

    // Get the first component matching the passed in role
    err = graph->framework->O_GetComponentsOfRole((OMX_STRING)componentRole, 
                                                  &numComp, &name);

    if (err != OMX_ErrorNone)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        NvOsFree(name);
        return err;
    }

    // Now that we have the name, just go create the component using that
    err = NvxGraphCreateComponentByName(graph, (char *)name, componentId, 
                                        component);

    NvOsFree(name);
    return err;
}

/* 
 * Remove a component from the graph, delete all resources
 */
OMX_ERRORTYPE NvxGraphRemoveComponent(NvxGraph *graph,
                                      NvxComponent *component)
{
    NvxComponent *prev;

    if (!graph || !component)
        return OMX_ErrorBadParameter;

    // FIXME bail if not in loaded?

    if (component == graph->components)
    {
        graph->components = component->next;
    }
    else
    {
        prev = graph->components;
        while (prev)
        {
            if (prev->next == component)
            {
                prev->next = component->next;
                break;
            }
            prev = prev->next;
        }

        if (!prev)
            return OMX_ErrorBadParameter;
    }

    GraphFreeComp(component);
    return OMX_ErrorNone;
}

/*
 * Find a component by the user-specified component id
 */
NvxComponent *NvxGraphLookupComponent(NvxGraph *graph,
                                      const char *compId)
{
    NvxComponent *comp;

    if (!graph || !compId)
        return NULL;

    // find the specified component
    comp = graph->components;
    while (comp)
    {
        if (!NvOsStrncmp(comp->id, compId, NvOsStrlen(comp->id)))
            return comp;
        comp = comp->next;
    }

    return NULL;
}

/* 
 * If called, this component must receive an EOS buffer flag for the graph to
 * be considered at end of stream
 */
OMX_ERRORTYPE NvxGraphSetComponentAsEndpoint(NvxGraph *graph,
                                             NvxComponent *component)
{
    if (!graph || !component)
        return OMX_ErrorBadParameter;

    if (!component->priv->isEndComponent)
    {
        component->priv->isEndComponent = OMX_TRUE;
        graph->priv->needEOSEvents++;
        graph->priv->maxEOSEvents++;
    }

    return OMX_ErrorNone;
}

/*
 * Transition component to requested state
 */
OMX_ERRORTYPE NvxGraphTransitionComponentToState(NvxComponent *component,
                                                 OMX_STATETYPE newState,
                                                 OMX_U32 timeOutMillisecs)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_STATETYPE curState;

    if (!component)
        return OMX_ErrorBadParameter;

    err = OMX_GetState(component->handle, &curState);
    if (OMX_ErrorNone != err)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return err;
    }

    if (curState != newState)
    {
        err = OMX_SendCommand(component->handle, OMX_CommandStateSet, 
                              newState, 0);
        if (OMX_ErrorNone != err)
        {
            NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
                __FUNCTION__, __LINE__));
            return err;
        }
    }

    if (!timeOutMillisecs)
        return err;

    err = GraphWaitForState(component->graph->priv, component, newState, 
                            timeOutMillisecs);
    return err;
}

/*
 * Component callbacks setup
 */
OMX_ERRORTYPE NvxGraphSetCompBufferCallbacks(NvxComponent *component,
                                             OMX_PTR fillBufData,
                                             NvxFillBufferDone fillBuffer,
                                             OMX_PTR emptyBufData,
                                             NvxEmptyBufferDone emptyBuffer)
{
    if (!component)
        return OMX_ErrorBadParameter;

    component->priv->fillBufferDoneData  = fillBufData;
    component->priv->fillBufferDone      = fillBuffer;
    component->priv->emptyBufferDoneData = emptyBufData;
    component->priv->emptyBufferDone     = emptyBuffer;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxGraphGetCompBufferCallbacks(NvxComponent *component,
                                             OMX_PTR *fillBufData,
                                             NvxFillBufferDone *fillBuffer,
                                             OMX_PTR *emptyBufData,
                                             NvxEmptyBufferDone *emptyBuffer)
{
    if (!component)
        return OMX_ErrorBadParameter;
    if (!component->priv)
        return OMX_ErrorInvalidState;

    *fillBufData = component->priv->fillBufferDoneData;
    *fillBuffer = component->priv->fillBufferDone;
    *emptyBufData = component->priv->emptyBufferDoneData;
    *emptyBuffer = component->priv->emptyBufferDone;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxGraphSetCompEventHandler(NvxComponent *component,
                                          OMX_PTR handlerData,
                                          NvxEventHandler handler)
{
    if (!component)                       
        return OMX_ErrorBadParameter;

    component->priv->eventHandlerData = handlerData;
    component->priv->eventHandler     = handler;

    return OMX_ErrorNone;
}

/*
 * Transition all components to the specified state
 * timeout is per-component, not overall
 */
OMX_ERRORTYPE NvxGraphTransitionAllToState(NvxGraph *graph,
                                           OMX_STATETYPE newState,
                                           OMX_U32 timeOutMillisecs)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_STATETYPE curState;
    NvxComponent *comp;

    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_INFO, "NvxGraph state changing to %s",
        (newState==OMX_StateLoaded)?"LOADED":(newState==OMX_StateIdle)?"IDLE":(newState==OMX_StateExecuting)?"EXEC":(newState==OMX_StatePause)?"PAUSE":"???"));

    NvOsMutexLock(graph->priv->hStateMutex);
    if (!graph)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        err = OMX_ErrorBadParameter;
        goto fail;
    }        

    comp = graph->components;

    // send all the state set commands first
    while (comp)
    {
        err = OMX_GetState(comp->handle, &curState);
        if (OMX_ErrorNone != err)
        {
            NvOsDebugPrintf("ERROR --%s[%d] comp %s",
                __FUNCTION__, __LINE__, comp->name);
            goto fail;
        }

        if (curState != newState)
        {
            err = OMX_SendCommand(comp->handle, OMX_CommandStateSet, newState,
                                  0);
            if (OMX_ErrorNone != err)
            {
                NvOsDebugPrintf("ERROR --%s[%d] comp %s",
                    __FUNCTION__, __LINE__, comp->name);
                goto fail;
            }
        }

        comp = comp->next;
    }

    // then wait if desired
    if (timeOutMillisecs > 0)
    {
        comp = graph->components;
        while (comp)
        {
            err = GraphWaitForState(graph->priv, comp, newState, 
                                    timeOutMillisecs);
            if (OMX_ErrorNone != err)
            {
                NvOsDebugPrintf("ERROR --%s[%d] comp %s",
                    __FUNCTION__, __LINE__, comp->name);

                goto fail;
            }

            comp = comp->next;
        }
    }

    graph->state = newState;

    NvOsMutexUnlock(graph->priv->hStateMutex);
    NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_INFO, "NvxGraph state changed from %s to %s",
        (curState==OMX_StateLoaded)?"LOADED":(curState==OMX_StateIdle)?"IDLE":(curState==OMX_StateExecuting)?"EXEC":(curState==OMX_StatePause)?"PAUSE":"???",
        (newState==OMX_StateLoaded)?"LOADED":(newState==OMX_StateIdle)?"IDLE":(newState==OMX_StateExecuting)?"EXEC":(newState==OMX_StatePause)?"PAUSE":"???"));
   
    return OMX_ErrorNone;
    
fail:
    NvOsMutexUnlock(graph->priv->hStateMutex);
    NvOsDebugPrintf("NvxGraph state change from %s to %s Failed! NvError = 0x%08x",
        (curState==OMX_StateLoaded)?"LOADED":(curState==OMX_StateIdle)?"IDLE":(curState==OMX_StateExecuting)?"EXEC":(curState==OMX_StatePause)?"PAUSE":"???",
        (newState==OMX_StateLoaded)?"LOADED":(newState==OMX_StateIdle)?"IDLE":(newState==OMX_StateExecuting)?"EXEC":(newState==OMX_StatePause)?"PAUSE":"???",
        err);
    return err;
}

/*
 * Flush all components in the graph
 */
OMX_ERRORTYPE NvxGraphFlushAllComponents(NvxGraph *graph)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    NvxComponent *comp;

    if (!graph)
        return OMX_ErrorBadParameter;

    NvOsMutexLock(graph->priv->hStateMutex);
    comp = graph->components;

    while (comp)
    {
        err = OMX_SendCommand(comp->handle, OMX_CommandFlush, OMX_ALL, 0);
        if (OMX_ErrorNone != err)
        {
            NvOsMutexUnlock(graph->priv->hStateMutex);
            NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
                __FUNCTION__, __LINE__));
            return err;
        }
        NvOsSemaphoreWait(graph->priv->flushEvent);

        comp = comp->next;
    }
    NvOsMutexUnlock(graph->priv->hStateMutex);
    return err;
}

/*
 * Wait for end of stream (or an error)
 */
void NvxGraphWaitForEndOfStream(NvxGraph *graph, OMX_U32 timeOutMillisecs)
{
    if (!graph)
        return;

    (void)NvOsSemaphoreWaitTimeout(graph->priv->mainEndEvent, timeOutMillisecs);
}

OMX_BOOL NvxGraphIsAtEndOfStream(NvxGraph *graph)
{
    if (!graph)
        return OMX_FALSE;

    return graph->priv->atEOS;
}

void NvxGraphSignalEndOfStreamEvent(NvxGraph *graph)
{
    if (!graph)
        return;

    NvOsSemaphoreSignal(graph->priv->mainEndEvent);
}

/*
 * Does *not* signal the event 
 */
void NvxGraphSetEndOfStream(NvxGraph *graph, OMX_BOOL eos)
{
    NvxComponent *comp;

    if (!graph)
        return;

    graph->priv->atEOS = eos;
    graph->priv->needEOSEvents = (eos) ? 0 : graph->priv->maxEOSEvents;

    comp = graph->components;
    while (comp && !eos)
    {
        comp->priv->seenEOSEvent = OMX_FALSE;
        comp = comp->next;  
    }  
}

void NvxGraphSetWaitForErrorsOnState(NvxGraph *graph, OMX_BOOL waitOnError)
{
    if (!graph)
        return;

    graph->priv->waitOnError = waitOnError;
}

OMX_ERRORTYPE NvxGraphGetError(NvxGraph *graph)
{
    if (!graph)
        return OMX_ErrorBadParameter;

    return graph->priv->errorEvent;
}

void NvxGraphClearError(NvxGraph *graph)
{
    if (!graph)
        return;

    graph->priv->errorEvent = OMX_ErrorNone;
}

/* 
 * Transition all components back to loaded and then free them
 */
OMX_ERRORTYPE NvxGraphTeardown(NvxGraph *graph)
{
    if (!graph)
        return OMX_ErrorBadParameter;

//NvOsDebugPrintf("NvxGraphTeardown ++++++\n");
//{
//    NvxComponent *comp = graph->components;
//    while (comp != NULL) {
//        NvOsDebugPrintf(">>> %s\n",comp->name);
//        comp = comp->next;
//    }
//}

    graph->priv->atEOS = OMX_TRUE;
    graph->priv->errorEvent = OMX_ErrorNone;

    if (graph->state == OMX_StateExecuting ||
        graph->state == OMX_StatePause)
    {
        NvxGraphStopClock(graph);
        NvxGraphTransitionAllToState(graph, OMX_StateIdle, 10000);
        graph->priv->errorEvent = OMX_ErrorNone;
    }

    if (graph->state == OMX_StateIdle)
    {
        NvxGraphTransitionAllToState(graph, OMX_StateLoaded, 10000);
    }

    GraphFreeAllComps(graph); 

//NvOsDebugPrintf("NvxGraphTeardown ------\n");

    return OMX_ErrorNone;
}

/*
 * Remove port tunneled connection. 
 */
OMX_ERRORTYPE NvxSetUntunneled(NvxComponent *compA, OMX_U32 portA)
{
    OMX_ERRORTYPE err;

    if (!compA || portA >= compA->portsNo)
        return OMX_ErrorBadParameter;

    err = compA->graph->framework->O_SetupTunnel(compA->handle, portA,
                                                 NULL, 0);

    if (OMX_ErrorNone != err)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return err;
    }

    compA->ports[portA].isConnected = OMX_FALSE;
    compA->ports[portA].tunnelPort = NULL;
    return err;
}

/*
 * Connect two specified components together
 */
OMX_ERRORTYPE NvxConnectTunneled(NvxComponent *compA, OMX_U32 portA,
                                 NvxComponent *compB, OMX_U32 portB)
{
    OMX_ERRORTYPE err;

    if (!compA || portA >= compA->portsNo)
        return OMX_ErrorBadParameter;

    if (!compB || portB >= compB->portsNo)
        return OMX_ErrorBadParameter;

    if (compA->graph != compB->graph)
        return OMX_ErrorBadParameter;

    // FIXME add more checks for validity

    err = compA->graph->framework->O_SetupTunnel(compA->handle, portA,
                                                 compB->handle, portB);

    if (OMX_ErrorNone != err)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return err;
    }

    compA->ports[portA].isConnected = OMX_TRUE;
    compA->ports[portA].tunnelPort = &(compB->ports[portB]);
    compB->ports[portB].isConnected = OMX_TRUE;
    compB->ports[portB].tunnelPort = &(compA->ports[portA]);

    return err;
}

/*
 * Enable or disable a port and wait for the state to change.
 */
OMX_ERRORTYPE NvxEnablePort(NvxPort *port, OMX_BOOL enable)
{
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_COMMANDTYPE portCmd = enable ? OMX_CommandPortEnable :
                                       OMX_CommandPortDisable;

    if (!port || !port->component || !port->component->graph)
        return OMX_ErrorBadParameter;

    if (port->isConnected)
        return OMX_ErrorInvalidState;

    omxErr = OMX_SendCommand(port->component->handle,
                         portCmd, port->portNum, 0);
    if (omxErr != OMX_ErrorNone)
        return omxErr;

    omxErr = NvxWaitForPortState(port,enable,2000);
    return omxErr;
}

/*
 * Wait for required port state with given timeout.
 */
OMX_ERRORTYPE NvxWaitForPortState(NvxPort *port, OMX_BOOL enable, OMX_U32 portTimeOut)
{
    NvxGraphPriv *gPriv = NULL;
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE portDef;
    OMX_HANDLETYPE hOmx = NULL;
    NvError semerr = NvSuccess;
    NvU32 prevTime;
    NvU32 curTime = 0;
    NvS32 timeOut = (NvS32)portTimeOut;

    if (!port || !port->component || !port->component->graph)
        return OMX_ErrorBadParameter;

    gPriv = port->component->graph->priv;
    if (!gPriv)
        return OMX_ErrorBadParameter;

    hOmx = port->component->handle;

    INIT_PARAM(portDef, port->component->graph->framework);
    portDef.nPortIndex = port->portNum;
    omxErr = OMX_GetParameter(hOmx, OMX_IndexParamPortDefinition, &portDef);
    if (omxErr != OMX_ErrorNone)
        return omxErr;

    curTime = NvOsGetTimeMS();

    while (portDef.bEnabled != enable)
    {
        // Make sure total time waiting is no longer than original timeOut.
        prevTime = curTime;
        curTime = NvOsGetTimeMS();
        timeOut -= (NvS32)OMX_TIMEDIFF(prevTime, curTime);
        if (timeOut < 0)
            return OMX_ErrorTimeout;

        // Now wait for port commands completion
        semerr = NvOsSemaphoreWaitTimeout(gPriv->portEvent, timeOut);

        // Timeout or invalid state
        if (semerr != NvSuccess)
        {
            NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
                __FUNCTION__, __LINE__));
            return (NvError_Timeout == semerr)? OMX_ErrorTimeout :
                                                OMX_ErrorInvalidState;
        }

        // Check port def again
        // The semaphore may have had an old signal sitting around in it.
        // if a previous Wait had timed out. So, check whether the port def
        // actually changed (while loop), and if not, try waiting again.
        omxErr = OMX_GetParameter(hOmx, OMX_IndexParamPortDefinition, &portDef);
        if (omxErr != OMX_ErrorNone)
            return omxErr;
    }

    // Set new ports state:
    port->enabled = enable;
    return omxErr;
}

static OMX_BOOL NvxPortIsSupplier(NvxPort *port)
{
    if (port->supplierType == OMX_BufferSupplyInput &&
        port->direction == OMX_DirInput)
        return OMX_TRUE;
    if (port->supplierType == OMX_BufferSupplyOutput &&
        port->direction == OMX_DirOutput)
        return OMX_TRUE;
    return OMX_FALSE;
}

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
OMX_ERRORTYPE NvxEnableTunneledPort(NvxPort *port, OMX_BOOL enable)
{
    NvxPort *otherPort = NULL;
    NvxPort *firstPort = NULL;
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    OMX_COMMANDTYPE portCmd = enable ? OMX_CommandPortEnable :
                                       OMX_CommandPortDisable;
    OMX_BOOL isSupplier = OMX_FALSE;
    NvU32 prevTime, curTime = 0;
    NvS32 timeOut = 5000;

    if (!port || !port->component || !port->component->graph)
        return OMX_ErrorBadParameter;

    if (!port->isConnected || !port->tunnelPort)
        return OMX_ErrorInvalidState;

    otherPort = port->tunnelPort;
    if (!otherPort || !otherPort->component || !otherPort->component->graph)
        return OMX_ErrorBadParameter;

    if (port->component->graph != otherPort->component->graph)
        return OMX_ErrorInvalidState;

    // Who is the supplier in this tunneled connection?
    isSupplier = NvxPortIsSupplier(port);

    // OMX specifies which port should receive enable/disable command first.
    // For port disabling, it is supplier in tunneled connection,
    // for port enabling, it is non-supplier.
    // Not sure how important this is given async nature of OMX machinery,
    // but we are trying to be as strict as possible here.
    if ((isSupplier && !enable) || (!isSupplier && enable))
        firstPort = port;
    else
    {
        firstPort = otherPort;
        otherPort = port;
    }

    omxErr = OMX_SendCommand(firstPort->component->handle,
                             portCmd, firstPort->portNum, 0);
    if (omxErr != OMX_ErrorNone)
        return omxErr;
    omxErr = OMX_SendCommand(otherPort->component->handle,
                             portCmd, otherPort->portNum, 0);
    if (omxErr != OMX_ErrorNone)
        return omxErr;

    prevTime = NvOsGetTimeMS();
    omxErr = NvxWaitForPortState(firstPort, enable, timeOut);
    if (omxErr != OMX_ErrorNone)
        return omxErr;
    curTime = NvOsGetTimeMS();
    timeOut -= (NvS32)OMX_TIMEDIFF(prevTime, curTime);
    if (timeOut < 0)
        timeOut = 0;
    // Even if we are out of time according to timeOut,
    // give NvxWaitForPortState() a chance to check the state
    // of second port - it could be done already.
    omxErr = NvxWaitForPortState(otherPort, enable, timeOut);

    return omxErr;
}

/*
 * Connect a component to the next available clock port
 */
OMX_ERRORTYPE NvxConnectComponentToClock(NvxComponent *comp)
{
    OMX_U32 port, clockPort;
    OMX_ERRORTYPE err;

    if (!comp) {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    if (!comp->graph->clock) {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    // Look for a clock port on the component
    for (port = 0; port < comp->portsNo; port++)
    {
        if (OMX_PortDomainOther == comp->ports[port].domain &&
            OMX_OTHER_FormatTime == comp->ports[port].formattype.other)
        {
            break;
        }
    }
    if (port >= comp->portsNo)
        return OMX_ErrorNotImplemented;

    // Look for the next available clock port on the clock
    for (clockPort = 0; clockPort < comp->graph->clock->portsNo; clockPort++)
    {
        if (! comp->graph->clock->ports[clockPort].enabled)
            break;
    }
    if (clockPort >= comp->graph->clock->portsNo)
        return OMX_ErrorNoMore;

    OMX_SendCommand(comp->graph->clock->handle, OMX_CommandPortEnable,
                    clockPort, 0);
    comp->graph->clock->ports[clockPort].enabled = OMX_TRUE;

    // enable the clock port on the component, if it's disabled
    if (!comp->ports[port].enabled)
    {
        OMX_SendCommand(comp->handle, OMX_CommandPortEnable, port, 0);
        comp->ports[port].enabled = OMX_TRUE;
    }

    // tunnel stuff
    err = NvxConnectTunneled(comp->graph->clock, clockPort,
                             comp, port);
    if (OMX_ErrorNone != err)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return err;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxDisconnectComponentFromClock(NvxComponent *comp)
{
    OMX_U32 port;
    NvxComponent *clock = NULL;
    NvxPort *clockPort = NULL;
    OMX_ERRORTYPE err;

    if (!comp) {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    if (!comp->graph->clock) {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return OMX_ErrorBadParameter;
    }
    clock = comp->graph->clock;

    // Look for a clock port on the component
    for (port = 0; port < comp->portsNo; port++)
    {
        if (OMX_PortDomainOther == comp->ports[port].domain &&
            OMX_OTHER_FormatTime == comp->ports[port].formattype.other)
        {
            break;
        }
    }
    if (port >= comp->portsNo)
        return OMX_ErrorNotImplemented;

    // If our port is tunneled to Clock component,
    // untunnel this connection.
    if (comp->ports[port].isConnected)
    {
        clockPort = comp->ports[port].tunnelPort;
        if (!clockPort || !clockPort->component ||
            (clockPort->component != clock))
        {
            return OMX_ErrorInvalidState;
        }
        err = NvxSetUntunneled(comp, port);
        if (err != OMX_ErrorNone)
            return err;
        err = NvxSetUntunneled(clock, clockPort->portNum);
        if (err != OMX_ErrorNone)
            return err;
    }

    if (comp->ports[port].enabled)
    {
        OMX_SendCommand(comp->handle, OMX_CommandPortDisable, port, 0);
        comp->ports[port].enabled = OMX_FALSE;
    }
    if (clockPort && clockPort->enabled)        
    {
        OMX_SendCommand(clock->handle, OMX_CommandPortDisable, clockPort->portNum, 0);
        clockPort->enabled = OMX_FALSE;
    }
    return OMX_ErrorNone;
}

/*
 * Allocate the mininum required buffers for a component's port
 */
OMX_ERRORTYPE NvxAllocateBuffersForPort(NvxComponent *comp, OMX_U32 port,
                                        OMX_U32 *bufCount)
{
    OMX_PARAM_PORTDEFINITIONTYPE portDef;
    OMX_ERRORTYPE err;
    OMX_U32 i, buflistsize;
    NvxComponentPriv *priv = NULL;
    NvxPortBuffers *pBuf = NULL;

    if (comp == NULL || port >= comp->portsNo || !bufCount)
        return OMX_ErrorBadParameter;

    *bufCount = 0;

    priv = comp->priv;

    pBuf = &priv->buffers[port];
    pBuf->Count = 0;
    pBuf->List = NULL;

    if (!pBuf->hMutex && NvSuccess != NvOsMutexCreate(&pBuf->hMutex))
    {
        return OMX_ErrorInsufficientResources;
    }


    INIT_PARAM(portDef, comp->graph->framework);
    portDef.nPortIndex = port;

    // Get the port defs
    err = OMX_GetParameter(comp->handle, OMX_IndexParamPortDefinition, 
                           &portDef);
    if (OMX_ErrorNone != err)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        goto fail;
    }

    // Allocate buffer list 
    buflistsize = sizeof(OMX_BUFFERHEADERTYPE *) * portDef.nBufferCountMin;

    pBuf->List = NvOsAlloc(buflistsize);
    if (!pBuf->List)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        err = OMX_ErrorInsufficientResources;
        goto fail;
    }

    NvOsMemset(pBuf->List, 0, buflistsize);

    // Allocate the minimum number of buffers
    for (i = 0; i < portDef.nBufferCountMin; i++)
    {
        err = OMX_AllocateBuffer(comp->handle, &(pBuf->List[i]),
                                 port, priv, portDef.nBufferSize);
        if (err != OMX_ErrorNone)
        {
            NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
                __FUNCTION__, __LINE__));
            goto fail;
        }
        pBuf->Count += 1;
    }

    *bufCount = pBuf->Count;
    return OMX_ErrorNone;

 fail:
    (void)NvxFreeBuffersForPort(comp, port);
    return err;
}

/*
 * Return the previously allocated buffers in a user-supplied list
 */
OMX_ERRORTYPE NvxGetAllocatedBuffers(NvxComponent *comp, OMX_U32 port,
                                     OMX_BUFFERHEADERTYPE **buffers, 
                                     OMX_U32 bufCount)
{
    OMX_U32 i;
    NvxPortBuffers *pBuf = NULL;

    if (!comp || port >= comp->portsNo || !buffers || !bufCount)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    pBuf = &comp->priv->buffers[port];

    if (bufCount != pBuf->Count)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    if (!pBuf->List)
        return OMX_ErrorNone;

    for (i = 0; i < pBuf->Count; i++)
    {
        buffers[i] = pBuf->List[i];
    }

    return OMX_ErrorNone;
}

/*
 * Free previously allocated buffers
 */
OMX_ERRORTYPE NvxFreeBuffersForPort(NvxComponent *comp, OMX_U32 port)
{
    OMX_U32 i;
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    NvxPortBuffers *pBuf = NULL;

    if (!comp || port >= comp->portsNo)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    pBuf = &comp->priv->buffers[port];
    if (!pBuf->List)
    {
        pBuf->Count = 0;
        return OMX_ErrorNone;
    }

    NvOsMutexLock(pBuf->hMutex);

    for (i = 0; i < pBuf->Count; i++)
    {
        if (pBuf->List[i])
        {
            omxErr = OMX_FreeBuffer(comp->handle, port, pBuf->List[i]);
            if (omxErr != OMX_ErrorNone) 
            {
                NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
                    __FUNCTION__, __LINE__));
                NvOsMutexUnlock(pBuf->hMutex);
                return omxErr;
            }
        }
        pBuf->List[i] = NULL;
    }

    NvOsFree(pBuf->List);
    pBuf->List = NULL;
    pBuf->Count = 0;

    NvOsMutexUnlock(pBuf->hMutex);

    return omxErr;
}

/*
 * Start the clock component at the specified timestamp
 */
void NvxGraphStartClock(NvxGraph *graph, OMX_TICKS timeInMilliSec)
{
    OMX_ERRORTYPE err = OMX_ErrorNotReady;
    OMX_TIME_CONFIG_CLOCKSTATETYPE clockState;

    if (!graph || !graph->clock)
        return;

    INIT_PARAM(clockState, graph->framework);
    clockState.nOffset = 0;
    clockState.nStartTime = (OMX_TICKS)(timeInMilliSec)*1000;
    clockState.nWaitMask = 0;
    clockState.eState = OMX_TIME_ClockStateRunning;

    do
    {
        err = OMX_SetConfig(graph->clock->handle,
                            OMX_IndexConfigTimeClockState,
                            &clockState);
        if (OMX_ErrorNotReady == err)
            NvOsSleepMS(10);
    }
    while (OMX_ErrorNotReady == err);
}

/* 
 * Stop the clock
 */
void NvxGraphStopClock(NvxGraph *graph)
{
    OMX_ERRORTYPE err = OMX_ErrorNotReady;
    OMX_TIME_CONFIG_CLOCKSTATETYPE clockState;

    if (!graph || !graph->clock)
        return;

    INIT_PARAM(clockState, graph->framework);
    clockState.nOffset = 0;
    clockState.nStartTime = 0;
    clockState.nWaitMask = 0;
    clockState.eState = OMX_TIME_ClockStateStopped;

    do
    {
        err = OMX_SetConfig(graph->clock->handle,
                            OMX_IndexConfigTimeClockState,
                            &clockState);
        if (OMX_ErrorNotReady == err)
            NvOsSleepMS(10);
    }
    while (OMX_ErrorNotReady == err);
}

/*
 * Pause = rate 0, not stopped
 */
void NvxGraphPauseClock(NvxGraph *graph, OMX_BOOL pause)
{
    OMX_TIME_CONFIG_SCALETYPE scale;

    if (!graph || !graph->clock)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return;
    }

    INIT_PARAM(scale, graph->framework);
    scale.xScale = (pause) ? 0 : 0x10000;

    (void)OMX_SetConfig(graph->clock->handle, OMX_IndexConfigTimeScale, 
                        &scale);
}

/* 
 * Get the current timestamp
 */
OMX_TICKS NvxGraphGetClockTime(NvxGraph *graph)
{
    OMX_TIME_CONFIG_TIMESTAMPTYPE stamp;

    if (!graph || !graph->clock)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return 0;
    }

    INIT_PARAM(stamp, graph->framework);
    stamp.nPortIndex = OMX_ALL;

    (void)OMX_GetConfig(graph->clock->handle, 
                        OMX_IndexConfigTimeCurrentMediaTime, &stamp);
    return stamp.nTimestamp;
}

/*
 * Set all components at  rate specified by playSpeed
 */
OMX_ERRORTYPE NvxGraphSetRate(NvxGraph *graph, float playSpeed)
{
    NvxComponent *comp;
    OMX_TIME_CONFIG_SCALETYPE oTimeScale;
    OMX_ERRORTYPE  err = OMX_ErrorNone;
    comp = graph->components;
    INIT_PARAM(oTimeScale, graph->framework);
    oTimeScale.xScale = NvSFxFloat2Fixed(playSpeed);

    while (comp)
    {
        if (comp->handle)
        {
            err= OMX_SetConfig(comp->handle, OMX_IndexConfigTimeScale, &oTimeScale);
            if (err != OMX_ErrorNone)
                NvOsDebugPrintf("\nError at setconfig in setrate: 0x%x compname:%s",err,comp->name);
        }
        comp = comp->next;
    }
    return OMX_ErrorNone;
}
