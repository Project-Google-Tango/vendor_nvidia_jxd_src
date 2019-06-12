/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

#include "common/NvxComponent.h"
#include "common/NvxTrace.h"
#include "common/NvxHelpers.h"

#include "nvxloopbackrenderer.h"

#include "nvassert.h"
#include "nvos.h"
#include "nvcommon.h"

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */

/* Loopback State information */
typedef struct SNvxLoopbackData
{
    // Render information
    NvColorFormat    eRequestedColorFormat;

    // State information
    OMX_BOOL                bSentFirstFrameEvent;
} SNvxLoopbackData;

#define NVX_VIDREND_VIDEO_PORT 0
#define NVX_VIDREND_CLOCK_PORT 1

/* =========================================================================
 *                     P R O T O T Y P E S
 * ========================================================================= */

/* Core functions */
static OMX_ERRORTYPE NvxLoopbackDeInit(OMX_IN NvxComponent *hComponent);
static OMX_ERRORTYPE NvxLoopbackGetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_INOUT OMX_PTR pComponentConfigStructure);
static OMX_ERRORTYPE NvxLoopbackSetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentConfigStructure);
static OMX_ERRORTYPE NvxLoopbackWorkerFunction(OMX_IN NvxComponent *hComponent, OMX_IN OMX_BOOL bAllPortsReady, OMX_OUT OMX_BOOL *pbMoreWork, OMX_INOUT NvxTimeMs* puMaxMsecToNextCall);
static OMX_ERRORTYPE NvxLoopbackAcquireResources(OMX_IN NvxComponent *hComponent);
static OMX_ERRORTYPE NvxLoopbackReleaseResources(OMX_IN NvxComponent *hComponent);
static OMX_ERRORTYPE NvxLoopbackFlush(OMX_IN NvxComponent *hComponent, OMX_U32 nPort);
static OMX_ERRORTYPE NvxLoopbackChangeState(OMX_IN NvxComponent *hComponent,OMX_IN OMX_STATETYPE hNewState);
static OMX_ERRORTYPE NvxLoopbackPreChangeState(OMX_IN NvxComponent *hComponent,OMX_IN OMX_STATETYPE hNewState);
static OMX_ERRORTYPE NvxLoopbackPreCheckChangeState(OMX_IN NvxComponent *hComponent,OMX_IN OMX_STATETYPE hNewState);
static OMX_ERRORTYPE NvxLoopbackPortEvent(NvxComponent *pNvComp, OMX_U32 nPort, OMX_U32 uEventType);

// Component setup functions
static OMX_ERRORTYPE NvxLoopbackSetComponentRoles( NvxComponent *pNvComp );
static void NvxLoopbackRegisterFunctions( NvxComponent *pNvComp );


/* Internal functions */
static OMX_ERRORTYPE NvxLoopbackSetup(SNvxLoopbackData * pNvxLoopback);
static OMX_ERRORTYPE NvxLoopbackOpen( SNvxLoopbackData * pNvxLoopback );
static OMX_ERRORTYPE NvxLoopbackClose( SNvxLoopbackData * pNvxLoopback );

/* =========================================================================
 *                     F U N C T I O N S
 * ========================================================================= */

OMX_ERRORTYPE NvxLoopbackRendererInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = NULL;
    OMX_ERRORTYPE eError      = OMX_ErrorNone;
    SNvxLoopbackData * pNvxLoopback = NULL;

    eError = NvxComponentCreate(hComponent, 2, &pNvComp);
    if (OMX_ErrorNone != eError)
        return eError;

    pNvComp->DeInit = NvxLoopbackDeInit;
    pNvComp->ChangeState = NvxLoopbackChangeState;
    pNvComp->PreChangeState = NvxLoopbackPreChangeState;
    pNvComp->PreCheckChangeState = NvxLoopbackPreCheckChangeState;
    pNvComp->Flush = NvxLoopbackFlush;

    pNvComp->pComponentData = NvOsAlloc(sizeof(SNvxLoopbackData));
    if (!pNvComp->pComponentData)
    {
        return OMX_ErrorInsufficientResources;
    }

    pNvxLoopback = (SNvxLoopbackData *)pNvComp->pComponentData;
    NvOsMemset(pNvComp->pComponentData, 0, sizeof(SNvxLoopbackData));

    eError = NvxLoopbackSetup((SNvxLoopbackData *)pNvComp->pComponentData);

    /* Register name */
    pNvComp->pComponentName = "OMX.Nvidia.render.loopback";

    /* Register function pointers */
    NvxLoopbackRegisterFunctions(pNvComp);

    // Set component roles
    NvxLoopbackSetComponentRoles(pNvComp);

    NvxPortInitVideo( &pNvComp->pPorts[NVX_VIDREND_VIDEO_PORT], OMX_DirInput,  2, 1024, OMX_VIDEO_CodingAutoDetect);

    pNvComp->pPorts[NVX_VIDREND_VIDEO_PORT].eNvidiaTunnelTransaction = ENVX_TRANSACTION_GFSURFACES;

    NvxPortInitOther(&pNvComp->pPorts[NVX_VIDREND_CLOCK_PORT], OMX_DirInput, 4,
                     sizeof(OMX_TIME_MEDIATIMETYPE), OMX_OTHER_FormatTime);
    pNvComp->pPorts[NVX_VIDREND_CLOCK_PORT].eNvidiaTunnelTransaction = ENVX_TRANSACTION_CLOCK;
    pNvComp->pPorts[NVX_VIDREND_CLOCK_PORT].oPortDef.bEnabled = OMX_FALSE;

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxLoopbackRendererInit\n"));
    return eError;
}

static OMX_ERRORTYPE NvxLoopbackDeInit(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if (pNvComp->pComponentData)
    {
        NvOsFree(pNvComp->pComponentData);
        pNvComp->pComponentData = NULL;
    }
    return eError;
}

static OMX_ERRORTYPE NvxLoopbackGetConfig(
                                             OMX_IN NvxComponent* pNvComp,
                                             OMX_IN OMX_INDEXTYPE nIndex,
                                             OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxLoopbackGetConfig\n"));

    return eError;
}

static OMX_ERRORTYPE NvxLoopbackSetConfig(OMX_IN NvxComponent* pNvComp,
                                      OMX_IN OMX_INDEXTYPE nIndex,
                                      OMX_IN OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxLoopbackSetConfig\n"));

    return eError;
}

static OMX_ERRORTYPE NvxLoopbackWorkerFunction(OMX_IN NvxComponent *hComponent, OMX_BOOL bAllPortsReady, OMX_OUT OMX_BOOL *pbMoreWork,
                                                  OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    SNvxLoopbackData *pNvxLoopback = 0;
    OMX_ERRORTYPE       eError           = OMX_ErrorNone;
    NvxPort* pVideoPort = &hComponent->pPorts[NVX_VIDREND_VIDEO_PORT];
    NvxPort* pClockPort = &hComponent->pPorts[NVX_VIDREND_CLOCK_PORT];

    NVXTRACE((NVXT_CALLGRAPH, hComponent->eObjectType, "+NvxLoopbackWorkerFunction\n"));

    *pbMoreWork = bAllPortsReady;

    if (pClockPort->pCurrentBufferHdr)
    {
        NvxPortReleaseBuffer(pClockPort, pClockPort->pCurrentBufferHdr);
        if (!hComponent->bNeedRunWorker)
            return OMX_ErrorNone;
    }

    pNvxLoopback = (SNvxLoopbackData *)hComponent->pComponentData;
    if (!pNvxLoopback)
        return OMX_ErrorBadParameter;

    if (!pVideoPort->pCurrentBufferHdr)
    {
        NvxPortGetNextHdr(pVideoPort);
        if (!pVideoPort->pCurrentBufferHdr)
        {
            if (!hComponent->bNeedRunWorker)
            {
                return OMX_ErrorNone;
            }
        }
    }

    if (pVideoPort->pCurrentBufferHdr)
    {
        if (!pNvxLoopback->bSentFirstFrameEvent)
        {
            NvxSendEvent(hComponent, NVX_EventFirstFrameDisplayed,
                         pVideoPort->oPortDef.nPortIndex, 0, 0);
            pNvxLoopback->bSentFirstFrameEvent = OMX_TRUE;
        }
        if (pVideoPort->pCurrentBufferHdr->nFlags & OMX_BUFFERFLAG_EOS)
        {
            NvxSendEvent(hComponent, OMX_EventBufferFlag,
                         pVideoPort->oPortDef.nPortIndex,
                         OMX_BUFFERFLAG_EOS, 0);
        }

        NvxPortReleaseBuffer(pVideoPort, pVideoPort->pCurrentBufferHdr);
        pVideoPort->pCurrentBufferHdr = NULL;
    }

    if (!pVideoPort->pCurrentBufferHdr)
        NvxPortGetNextHdr(pVideoPort);

    if (pVideoPort->pCurrentBufferHdr)
        *pbMoreWork = OMX_TRUE;

    return eError;
}

static OMX_ERRORTYPE NvxLoopbackAcquireResources(OMX_IN NvxComponent *hComponent)
{
    SNvxLoopbackData *pNvxLoopback = 0;
    OMX_ERRORTYPE       eError           = OMX_ErrorNone;

    NV_ASSERT(hComponent);
    NVXTRACE((NVXT_CALLGRAPH, hComponent->eObjectType, "+NvxLoopbackAcquireResources\n"));

    eError = NvxComponentBaseAcquireResources(hComponent);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,hComponent->eObjectType, "ERROR:NvxLoopbackAcquireResources:"
                  ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    /* Acquire resources */
    pNvxLoopback = (SNvxLoopbackData *)hComponent->pComponentData;
    eError = NvxLoopbackOpen( pNvxLoopback );

    return eError;
}

static OMX_ERRORTYPE NvxLoopbackReleaseResources(OMX_IN NvxComponent *hComponent)
{
    SNvxLoopbackData *pNvxLoopback = 0;
    OMX_ERRORTYPE       eError           = OMX_ErrorNone;

    NV_ASSERT(hComponent);

    NVXTRACE((NVXT_CALLGRAPH, hComponent->eObjectType, "+NvxLoopbackReleaseResources\n"));

    /* Release resources */
    pNvxLoopback = (SNvxLoopbackData *)hComponent->pComponentData;
    eError = NvxLoopbackClose( pNvxLoopback );
    NvxCheckError(eError, NvxComponentBaseReleaseResources(hComponent));

    return eError;
}

static OMX_ERRORTYPE NvxLoopbackFlush(OMX_IN NvxComponent *hComponent, OMX_U32 nPort)
{
    SNvxLoopbackData * pNvxLoopback = hComponent->pComponentData;

    NVXTRACE((NVXT_CALLGRAPH, hComponent->eObjectType, "+NvxLoopbackFlush\n"));

    pNvxLoopback->bSentFirstFrameEvent = OMX_FALSE;

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxLoopbackPreCheckChangeState(OMX_IN NvxComponent *pNvComp,OMX_IN OMX_STATETYPE oNewState)
{
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxLoopbackPreChangeState(OMX_IN NvxComponent *pNvComp,OMX_IN OMX_STATETYPE oNewState)
{
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxLoopbackChangeState(OMX_IN NvxComponent *pNvComp,OMX_IN OMX_STATETYPE oNewState)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxLoopbackChangeState: %s\n",pNvComp->pComponentName));
    NVXTRACE((NVXT_STATE, pNvComp->eObjectType, "State change started: %s from %ld to %ld\n",
              pNvComp->pComponentName, pNvComp->eState, oNewState));

    return eError;
}

/* =========================================================================
 *                     I N T E R N A L
 * ========================================================================= */

static OMX_ERRORTYPE NvxLoopbackSetup(SNvxLoopbackData * pNvxLoopback)
{
    NVXTRACE((NVXT_CALLGRAPH,NVXT_VIDEO_RENDERER, "+NvxLoopbackSetup\n"));
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxLoopbackOpen( SNvxLoopbackData * pNvxLoopback )
{
    OMX_ERRORTYPE err = OMX_ErrorNone;

    NVXTRACE((NVXT_CALLGRAPH,NVXT_VIDEO_RENDERER, "+NvxLoopbackOpen\n"));

    return err;
}

static OMX_ERRORTYPE NvxLoopbackClose( SNvxLoopbackData * pNvxLoopback )
{
    return OMX_ErrorNone;
}

// Component name/role helper functions
static OMX_ERRORTYPE NvxLoopbackSetComponentRoles(NvxComponent *pNvComp)
{
    pNvComp->nComponentRoles = 1;
    pNvComp->sComponentRoles[0] = "iv_renderer.loopback";
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxLoopbackPortEvent(NvxComponent *pNvComp,
                                              OMX_U32 nPort, OMX_U32 uEventType)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxLoopbackPortEvent\n"));

    return eError;
}

static void NvxLoopbackRegisterFunctions( NvxComponent *pNvComp )
{
    pNvComp->GetConfig          = NvxLoopbackGetConfig;
    pNvComp->SetConfig          = NvxLoopbackSetConfig;
    pNvComp->WorkerFunction     = NvxLoopbackWorkerFunction;
    pNvComp->ReleaseResources   = NvxLoopbackReleaseResources;
    pNvComp->AcquireResources   = NvxLoopbackAcquireResources;
    pNvComp->eObjectType        = NVXT_VIDEO_RENDERER;
    pNvComp->PortEventHandler   = NvxLoopbackPortEvent;
}

