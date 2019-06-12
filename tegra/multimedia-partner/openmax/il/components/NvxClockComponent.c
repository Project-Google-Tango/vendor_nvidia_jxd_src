/* Copyright (c) 2006-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** NvxClockComponent.c */

#include "common/NvxComponent.h"
#include "common/NvxPort.h"
#include "OMX_Audio.h"
#include "NvxMutex.h"
#include "common/NvxTrace.h"

#include "nvos.h"
#include "nvmm_mediaclock.h"

#define MAX_REQUESTS 100
#define MIN_TIMESTAMP (-1000*OMX_TICKS_PER_SECOND)

typedef struct NvxCCState {
    OMX_S32 xScale;                                                 /* media time scale as a Q16 */
    OMX_TIME_REFCLOCKTYPE eActiveClock;                             /* active reference clock */
    OMX_TIME_CONFIG_CLOCKSTATETYPE oPendingClockState;              /* pending clock state */
    OMX_TIME_CONFIG_CLOCKSTATETYPE oClockState;                     /* current clock state */
    OMX_TIME_CONFIG_MEDIATIMEREQUESTTYPE oRequests[MAX_REQUESTS];   /* list of requests */
    OMX_U32 nRequests;                                              /* number of pending requests */
    OMX_TICKS sllStartTime[8];                                      /* start time for each client */
    OMX_U32 uGotStartTimeMask;                                      /* mask indicating the receipt of a start time */
    OMX_HANDLETYPE oLock;                                           /* mutex to prevent collision between threads */
    NvxComponent *pNvComp;                                          /* pointer to parent NvxComponent */

    NvMMClockHandle hClock;
} NvxCCState;

/* prototypes */
OMX_ERRORTYPE NvxClockComponentInit(OMX_IN  OMX_HANDLETYPE hComponent);

static OMX_BOOL NvxCCIsReqReady(NvxCCState *pState, OMX_TICKS mtTarget,
                                OMX_TICKS *pTicksDue)
{
    NvS64 nmtTarget = mtTarget * 10;
    NvS64 ticksDue = 0;

    NvMMGetTimeUntilMediaTime(pState->hClock, nmtTarget, &ticksDue);
    if (ticksDue < 10)
    {
        *pTicksDue = 0;
        return OMX_TRUE;
    }

    *pTicksDue = ticksDue / 10;
    return OMX_FALSE;
}

OMX_BOOL NvxCCWaitUntilTimestamp(OMX_HANDLETYPE hComponent,
                                 OMX_TICKS mtTarget, OMX_BOOL *bDropFrame,
                                 OMX_BOOL *bVideoBehind)
{
    NvxComponent *pNvComp = NvxComponentFromHandle(hComponent);
    NvxCCState *pState;
    NvMMClockFrameStatus eStatus;

    *bDropFrame = OMX_FALSE;
    *bVideoBehind = OMX_FALSE;

    if (!pNvComp)
        return OMX_FALSE;

    pState = (NvxCCState *)pNvComp->pComponentData;

    NvMMWaitUntilMediaTime(pState->hClock, mtTarget * 10, &eStatus);
    if (eStatus & NvMMClock_FrameBehind)
        *bVideoBehind = OMX_TRUE;
    if (eStatus & NvMMClock_FrameShouldDrop)
        *bDropFrame = OMX_TRUE;

    return OMX_TRUE;
}


void NvxCCSetCurrentAudioRefClock(OMX_HANDLETYPE hComponent,
                                  OMX_TICKS mtAudioTS)
{
    NvxComponent *pNvComp = NvxComponentFromHandle(hComponent);
    NvxCCState *pState;
    NvMMTimestamp ref;

    if (!pNvComp)
        return;

    pState = (NvxCCState *)pNvComp->pComponentData;

    ref.MediaTime = mtAudioTS * 10;
    ref.WallTime = 0;

    NvMMSupplyReferenceTime(pState->hClock, NvMMClockClientType_Audio, ref);
}

void NvxCCWaitForVideoToStart(OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp = NvxComponentFromHandle(hComponent);
    NvxCCState *pState;
    if (!pNvComp)
    {
        return;
    }

    pState = (NvxCCState *)pNvComp->pComponentData;
    NvMMWaitForVideoToStart(pState->hClock);
}

void NvxCCGetClockHandle(OMX_HANDLETYPE hComponent, void *hClock)
{
    NvxComponent *pNvComp = NvxComponentFromHandle(hComponent);
    NvxCCState *pState;

    if (!pNvComp)
    {
        return;
    }

    pState = (NvxCCState *)pNvComp->pComponentData;
    hClock = (void *)&pState->hClock;
}

void NvxCCSetWaitForAudio(OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp = NvxComponentFromHandle(hComponent);
    NvxCCState *pState;
    if (!pNvComp)
    {
        return;
    }

    pState = (NvxCCState *)pNvComp->pComponentData;
    NvMMSetWaitForAudio(pState->hClock);
}

void NvxCCSetWaitForVideo(OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp = NvxComponentFromHandle(hComponent);
    NvxCCState *pState;
    if (!pNvComp)
    {
        return;
    }

    pState = (NvxCCState *)pNvComp->pComponentData;
    NvMMSetWaitForVideo(pState->hClock);
}

void NvxCCUnblockWaitUntilTimestamp(OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp = NvxComponentFromHandle(hComponent);
    NvxCCState *pState;
    if (!pNvComp)
    {
        return;
    }

    pState = (NvxCCState *)pNvComp->pComponentData;
    NvMMUnblockWaitUntilMediaTime(pState->hClock);
}

OMX_S32 NvxCCGetClockRate(OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp = NvxComponentFromHandle(hComponent);
    NvxCCState *pState;
    if (!pNvComp)
    {
        return 0x10000;
    }

    pState = (NvxCCState *)pNvComp->pComponentData;
    return pState->xScale;
}

/* NvxCCIsRequestDue - Return true if the given request is due, false otherwise */
static OMX_BOOL NvxCCIsRequestDue(NvxCCState *pState, OMX_TIME_CONFIG_MEDIATIMEREQUESTTYPE *pRequest, OMX_TICKS* pTicksUntilDue)
{
    OMX_TICKS mtTarget = pRequest->nMediaTimestamp - (OMX_TICKS)((pRequest->nOffset * pState->xScale) >> 16);
    NVXTRACE((NVXT_CALLGRAPH,NVXT_CLOCK, "+NvxCCIsRequestDue\n"));

    return NvxCCIsReqReady(pState, mtTarget, pTicksUntilDue);
}

static OMX_ERRORTYPE  NvxCCFulfillRequest(NvxCCState *pState, OMX_TIME_CONFIG_MEDIATIMEREQUESTTYPE *pRequest)
{
    OMX_TIME_MEDIATIMETYPE *pUpdate;
    NvxPort *pPort;
    OMX_TIME_MEDIATIMETYPE update;
    NvMMTimestamp ntNow;

    OMX_TICKS mtNow, wtNow;
    NVXTRACE((NVXT_CALLGRAPH, NVXT_CLOCK, "+NvxCCFulfillRequest\n"));

    /* prepare fulfillment */
    update.nSize = sizeof(update);
    update.nVersion = NvxComponentSpecVersion;
    update.eState = pState->oClockState.eState;
    update.eUpdateType = OMX_TIME_UpdateRequestFulfillment;
    update.nMediaTimestamp = pRequest->nMediaTimestamp;
    update.nClientPrivate = 0;

    NvMMGetTime(pState->hClock, &ntNow);

    mtNow = ntNow.MediaTime / 10;
    wtNow = ntNow.WallTime / 10;

    update.nOffset = (OMX_TICKS)(((pRequest->nMediaTimestamp - mtNow) << 16) / pState->xScale);
    if (mtNow > pRequest->nMediaTimestamp+1000)
        NVXTRACE((NVXT_INFO, NVXT_CLOCK, "clock late by %lld ms\n", -update.nOffset/1000));
    update.nWallTimeAtMediaTime  = wtNow + update.nOffset;
    update.xScale = pState->xScale;

    /* send it */
    pPort = &pState->pNvComp->pPorts[pRequest->nPortIndex];
    if (!pPort->pCurrentBufferHdr){
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxCCFulfillRequest:"
            ":pCurrentBufferHdr = NULL:[%s(%d)]\n",__FILE__, __LINE__));
        return OMX_ErrorInsufficientResources;
    }
    if (!pPort->hTunnelComponent){
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxCCFulfillRequest:"
            ":hTunnelComponent = %d:[%s(%d)]\n",pPort->hTunnelComponent,__FILE__, __LINE__));
        return OMX_ErrorUndefined;
    }
    pUpdate = (OMX_TIME_MEDIATIMETYPE *)(void *)(pPort->pCurrentBufferHdr->pBuffer);
    *pUpdate = update;
    pPort->pCurrentBufferHdr->nFilledLen = sizeof(OMX_TIME_MEDIATIMETYPE);
    return NvxPortDeliverFullBuffer(pPort, pPort->pCurrentBufferHdr);
}

/* NvxCCServiceRequests - See if any requests are due, if so fulfill them. */
static OMX_ERRORTYPE NvxCCServiceRequests(NvxCCState *pState, NvxTimeMs *pMaxMsecToNextCall)
{
    OMX_U32 i,j;
    NvxTimeMs oMinMsUntilDue = ~0;
    OMX_TICKS oTicksUntilDue = 0;
    NVXTRACE((NVXT_CALLGRAPH, NVXT_CLOCK, "+NvxCCServiceRequests\n"));

    /* Does the clock state or scale prevent requests from being fulfilled? */
    if (OMX_TIME_ClockStateRunning != pState->oClockState.eState || 0 == pState->xScale) 
        return OMX_ErrorNone;

    /* examine each request (can't order the requests sequentially given the variability of
     * the effect of offset) */
    for(i=0;i<pState->nRequests;i++)
    {
        /* if the request is due */
        if ( NvxCCIsRequestDue(pState, &pState->oRequests[i], &oTicksUntilDue) )
        {
            if (OMX_ErrorNone == NvxCCFulfillRequest(pState, &pState->oRequests[i]))
            {
                /* remove fulfilled request by shifting rest of list down over fulfilled request. */
                pState->nRequests--;
                for (j=i;j<pState->nRequests;j++)
                {
                    pState->oRequests[j] = pState->oRequests[j+1];
                }
                i--; /* decrement index in anticipation of loop's increment */
            }
        }
        else {
            NvxTimeMs oMsUntilDue = (NvxTimeMs)(oTicksUntilDue/(OMX_TICKS_PER_SECOND/1000));
            if (oMsUntilDue == 0)
                oMsUntilDue = 1;

            if (oMsUntilDue < oMinMsUntilDue)
                oMinMsUntilDue = oMsUntilDue;
        }
    }
    *pMaxMsecToNextCall = oMinMsUntilDue;
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE TCCSendUpdateToAllClients(NvxCCState *pState, OMX_TIME_MEDIATIMETYPE *pSrcUpdate)
{
    OMX_TIME_MEDIATIMETYPE *pUpdate;
    NvxPort *pPort;
    OMX_U32 i;
    NVXTRACE((NVXT_CALLGRAPH, NVXT_CLOCK, "+TCCSendUpdateToAllClients\n"));

    /* send update to each connected port */
    for(i=0;i<8;i++)
    {
        pPort = &pState->pNvComp->pPorts[i];
        if (pPort->hTunnelComponent)
        {
            /* todo: is there a better way to handle running out of buffers other than skipping the client? */
            if (!pPort->pCurrentBufferHdr){
                continue;
            }
            pUpdate = (OMX_TIME_MEDIATIMETYPE *)(void *)(pPort->pCurrentBufferHdr->pBuffer);
            *pUpdate = *pSrcUpdate;
            pPort->pCurrentBufferHdr->nFilledLen = sizeof(OMX_TIME_MEDIATIMETYPE);
            NvxPortDeliverFullBuffer(pPort, pPort->pCurrentBufferHdr);
        }
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvsCCSendStateUpdate(NvxCCState *pState)
{
    OMX_TIME_MEDIATIMETYPE update;
    NvMMTimestamp ntNow;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_CLOCK, "+NvsCCSendStateUpdate\n"));

    update.nSize = sizeof(update);
    update.nVersion = NvxComponentSpecVersion;
    update.eState = pState->oClockState.eState;
    update.eUpdateType = OMX_TIME_UpdateClockStateChanged;

    NvMMGetTime(pState->hClock, &ntNow);
    update.nMediaTimestamp = ntNow.MediaTime / 10;
    update.nWallTimeAtMediaTime = ntNow.WallTime / 10;
    update.nClientPrivate = 0;

    update.nOffset = 0;
    update.xScale = pState->xScale;

    return TCCSendUpdateToAllClients(pState, &update);
}

static OMX_ERRORTYPE NvsCCSendScaleUpdate(NvxCCState *pState)
{
    OMX_TIME_MEDIATIMETYPE update;
    NvMMTimestamp ntNow;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_CLOCK, "+NvsCCSendScaleUpdate\n"));

    update.nSize = sizeof(update);
    update.nVersion = NvxComponentSpecVersion;
    update.eState = pState->oClockState.eState;
    update.eUpdateType = OMX_TIME_UpdateScaleChanged;
    update.nClientPrivate = 0;

    NvMMGetTime(pState->hClock, &ntNow);
    update.nMediaTimestamp = ntNow.MediaTime / 10;
    update.nWallTimeAtMediaTime = ntNow.WallTime / 10;

    update.nOffset = 0;
    update.xScale = pState->xScale;

    return TCCSendUpdateToAllClients(pState, &update);
}

static OMX_ERRORTYPE NvxCCUpdateClockState(NvxCCState *pState)
{
    int i;
    OMX_BOOL bNoMin;
    NvMMTimestamp ntNow;
    NVXTRACE((NVXT_CALLGRAPH, NVXT_CLOCK, "+NvxCCUpdateClockState\n"));

    if (OMX_TIME_ClockStateWaitingForStartTime == pState->oClockState.eState)
    {
        /* Do we have all start times we need? */
        if ((pState->uGotStartTimeMask & pState->oClockState.nWaitMask) == pState->oClockState.nWaitMask)
        {
            NvMMGetTime(pState->hClock, &ntNow);

            /* find min start time */
            bNoMin = OMX_TRUE;
            for (i=0;i<8;i++)
            {
                if (pState->oClockState.nWaitMask & (1<<i))
                {
                    if (bNoMin || (pState->sllStartTime[i] < ntNow.MediaTime))
                    {
                        pState->oClockState.nStartTime = pState->sllStartTime[i];
                        bNoMin = OMX_FALSE;
                    }
                }
            }

            /* transition to running */
            pState->oClockState.eState = OMX_TIME_ClockStateRunning;
            pState->oClockState.nWaitMask = 0;
            pState->uGotStartTimeMask = 0;

            ntNow.MediaTime = pState->oClockState.nStartTime * 10;
            NvMMStartClock(pState->hClock, 0, ntNow.MediaTime);
            NvMMSupplyReferenceTime(pState->hClock, NvMMClockClientType_Audio,
                                    ntNow);
            NvsCCSendStateUpdate(pState);
        }
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxCCPreChangeState(OMX_IN NvxComponent *pNvComp, 
                                         OMX_IN OMX_STATETYPE oNewState)
{
    NvxCCState *pState = (NvxCCState *)(pNvComp->pComponentData);

    if (pState && pState->hClock && oNewState != pNvComp->eState && 
        oNewState == OMX_StatePause)
    {
        NvMMUnblockWaitUntilMediaTime(pState->hClock);
    }

    return OMX_ErrorNone;
}

/* NvxCCWorkerFunction - do necessary component work */
static OMX_ERRORTYPE NvxCCWorkerFunction (
       OMX_IN NvxComponent *pThis,
       OMX_BOOL bAllPortsReady,            /* OMX_TRUE if all ports have a buffer ready */
       OMX_INOUT OMX_BOOL *pbMoreWork,       /* function should set to OMX_TRUE if there is more work */
       OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)  /* function should set to time to wait */
{
    NvxCCState *pState = (NvxCCState *)(pThis->pComponentData);
    NVXTRACE((NVXT_CALLGRAPH, pThis->eObjectType, "+NvxCCWorkerFunction\n"));

    NvxMutexLock(pState->oLock);

    NvxCCUpdateClockState(pState);
    NvxCCServiceRequests(pState, puMaxMsecToNextCall);

    NvxMutexUnlock(pState->oLock);
    return OMX_ErrorNone;
}

/* NvxCCGetConfig - service config queries. */
static OMX_ERRORTYPE NvxCCGetConfig(OMX_IN OMX_IN NvxComponent *pNvComp,
                                    OMX_IN OMX_INDEXTYPE nIndex,
                                    OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    OMX_TIME_CONFIG_SCALETYPE           *pScale;
    OMX_TIME_CONFIG_CLOCKSTATETYPE      *pClockState;
    OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE  *pActiveClock;
    OMX_TIME_CONFIG_TIMESTAMPTYPE       *pTimeStamp;
    NvxCCState *pState = (NvxCCState *)pNvComp->pComponentData;

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxCCGetConfig\n"));

    NvxMutexLock(pState->oLock);

    switch (nIndex)
    {
    case OMX_IndexConfigTimeScale:
        pScale= (OMX_TIME_CONFIG_SCALETYPE *)pComponentConfigStructure;
        pScale->xScale = pState->xScale;
        break;
    case OMX_IndexConfigTimeClockState:
        pClockState = (OMX_TIME_CONFIG_CLOCKSTATETYPE *)pComponentConfigStructure;
        *pClockState = pState->oClockState;
        break;
    case OMX_IndexConfigTimeActiveRefClock:
        pActiveClock = (OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE *)pComponentConfigStructure;
        pActiveClock->eClock = pState->eActiveClock;
        break;
    case OMX_IndexConfigTimeCurrentMediaTime:
    {
        NvMMTimestamp ntNow;
        pTimeStamp = (OMX_TIME_CONFIG_TIMESTAMPTYPE *)pComponentConfigStructure;
        NvMMGetTime(pState->hClock, &ntNow);
        pTimeStamp->nTimestamp = ntNow.MediaTime / 10;
        break;
    }
    case OMX_IndexConfigTimeCurrentWallTime:
    {
        NvMMTimestamp ntNow;
        pTimeStamp = (OMX_TIME_CONFIG_TIMESTAMPTYPE *)pComponentConfigStructure;
        NvMMGetTime(pState->hClock, &ntNow);
        pTimeStamp->nTimestamp = ntNow.WallTime / 10;
        break;
    }
    case OMX_IndexConfigTimeClientStartTime:
        pTimeStamp = (OMX_TIME_CONFIG_TIMESTAMPTYPE *)pComponentConfigStructure;
        pState->sllStartTime[pTimeStamp->nPortIndex] = pTimeStamp->nTimestamp;
        break;
    default:
        NvxMutexUnlock(pState->oLock);
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxCCGetConfig:"
            ":nIndex = %d:[%s(%d)]\n",nIndex,__FILE__, __LINE__));
        return OMX_ErrorUnsupportedIndex;
        /*       return NvxComponentBaseGetConfig( hComponent, nIndex, pComponentConfigStructure );*/
    }

    NvxMutexUnlock(pState->oLock);
    return OMX_ErrorNone;
}

/* NvxCCSetConfig - set a config to a particular value */
static OMX_ERRORTYPE NvxCCSetConfig(OMX_IN NvxComponent *pNvComp,
                                    OMX_IN OMX_INDEXTYPE nIndex,
                                    OMX_IN OMX_PTR pComponentConfigStructure)
{
    OMX_TIME_CONFIG_SCALETYPE           *pScale;
    OMX_TIME_CONFIG_CLOCKSTATETYPE      *pClockState;
    OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE  *pActiveClock;
    OMX_TIME_CONFIG_TIMESTAMPTYPE       *pTimeStamp;
    OMX_TIME_CONFIG_MEDIATIMEREQUESTTYPE *pRequest;
    NvxCCState *pState = (NvxCCState *)pNvComp->pComponentData;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxCCSetConfig\n"));

    NvxMutexLock(pState->oLock);

    /* trigger the worker to pick up any changes (may need to remove this if called from worker function) */
    NvxWorkerTrigger(&(pNvComp->oWorkerData));

    switch (nIndex)
    {
    case OMX_IndexConfigTimeScale:
        pScale= (OMX_TIME_CONFIG_SCALETYPE *)pComponentConfigStructure;
        NvMMSetClockRate(pState->hClock, (((NvS64)pScale->xScale * 1000) >> 16));
        pState->xScale = pScale->xScale;
        NvsCCSendScaleUpdate(pState);
        break;
    case OMX_IndexConfigTimeClockState:
        pClockState = (OMX_TIME_CONFIG_CLOCKSTATETYPE *)pComponentConfigStructure;
        if ((pState->oClockState.eState == OMX_TIME_ClockStateRunning) &&
            (pClockState->eState == OMX_TIME_ClockStateWaitingForStartTime))
        {
            NvxMutexUnlock(pState->oLock);
            return OMX_ErrorBadParameter;
        }
        pState->oClockState = *pClockState;
        pState->nRequests = 0; /* clear requests */
        pState->uGotStartTimeMask = 0;
        if (pClockState->eState == OMX_TIME_ClockStateRunning)
        {
            NvMMStartClock(pState->hClock, 0, pClockState->nStartTime * 10);
//            NVXTRACE((NVXT_INFO, NVXT_CLOCK, "clock running: %lld, %lld, %x\n", pState->mtLast, pState->wtLast, pState->xScale));
        }
        else
        {
            NvMMStopClock(pState->hClock);
//            NVXTRACE((NVXT_INFO, NVXT_CLOCK, "clock not running: %lld, %lld, %x\n", pState->mtLast, pState->wtLast, pState->xScale));
        }

        NvsCCSendStateUpdate(pState);
        break;
    case OMX_IndexConfigTimeActiveRefClock:
        pActiveClock = (OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE *)pComponentConfigStructure;
        pState->eActiveClock = pActiveClock->eClock;
        break;
    case OMX_IndexConfigTimeCurrentAudioReference:
    {
        NvMMTimestamp ts;
        pTimeStamp = (OMX_TIME_CONFIG_TIMESTAMPTYPE *)pComponentConfigStructure;
        ts.MediaTime = pTimeStamp->nTimestamp * 10;
        ts.WallTime = 0;
        NvMMSupplyReferenceTime(pState->hClock, NvMMClockClientType_Audio,
                                ts);
        break;
    }
    case OMX_IndexConfigTimeCurrentVideoReference:
    {
        NvMMTimestamp ts;
        pTimeStamp = (OMX_TIME_CONFIG_TIMESTAMPTYPE *)pComponentConfigStructure;
        ts.MediaTime = pTimeStamp->nTimestamp * 10;
        ts.WallTime = 0;
        NvMMSupplyReferenceTime(pState->hClock, NvMMClockClientType_Audio,
                                ts);
        break;
    }
    case OMX_IndexConfigTimeMediaTimeRequest:
        pRequest = (OMX_TIME_CONFIG_MEDIATIMEREQUESTTYPE*)pComponentConfigStructure;
        pState->oRequests[pState->nRequests++] = *pRequest;         /* add to request list */
        //TRACE_MSG(("clock mt request: %lld %lld, %lld, %x\n", pRequest->nMediaTimestamp, -pRequest->nOffset, pState->mtLast, pState->xScale));
        break;
    case OMX_IndexConfigTimeClientStartTime:
        pTimeStamp = (OMX_TIME_CONFIG_TIMESTAMPTYPE *)pComponentConfigStructure;
        if (pTimeStamp->nPortIndex >= 8){
            NvxMutexUnlock(pState->oLock);
            return OMX_ErrorBadParameter;
        }
        pState->sllStartTime[pTimeStamp->nPortIndex] = pTimeStamp->nTimestamp;
        pState->uGotStartTimeMask |= (1<<(pTimeStamp->nPortIndex));
        NvxCCUpdateClockState(pState);
        break;
    default:
        NvxMutexUnlock(pState->oLock);
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxCCSetConfig:"
            ":nIndex = %d:[%s(%d)]\n",nIndex,__FILE__, __LINE__));
        return OMX_ErrorUnsupportedIndex;
/*        return NvxComponentBaseSetConfig( hComponent, nIndex, pComponentConfigStructure ); */
    }
    NvxMutexUnlock(pState->oLock);
    return OMX_ErrorNone;
}

/* NvxCCDeInit - de-initialize the component */
static OMX_ERRORTYPE NvxCCDeInit(NvxComponent* pThis)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxCCState *pState = (NvxCCState *)pThis->pComponentData;
    NVXTRACE((NVXT_CALLGRAPH, pThis->eObjectType, "+NvxCCDeInit\n"));

    NvxMutexDestroy(pState->oLock);
    NvMMDestroyMediaClock(pState->hClock);

    NvOsFree(pThis->pComponentData);
    return eError;
}

/* NvxClockComponentInit - initialize the clock component */
OMX_ERRORTYPE NvxClockComponentInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    int i;
    NvxCCState *pState;
    NvxComponent* pThis;
    OMX_ERRORTYPE eError = OMX_ErrorNone;


    eError = NvxComponentCreate(hComponent,8,&pThis);
    if (OMX_ErrorNone != eError)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxClockComponentInit:"
            ":eError = %d:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    pThis->pComponentName = "OMX.Nvidia.clock.component";
    pThis->sComponentRoles[0] = "clock.binary";    // check the correct type
    pThis->nComponentRoles = 1;
    pThis->eObjectType = NVXT_CLOCK;
    pThis->DeInit = NvxCCDeInit;
    pThis->GetConfig = NvxCCGetConfig;
    pThis->SetConfig = NvxCCSetConfig;
    pThis->WorkerFunction = NvxCCWorkerFunction;
    pThis->PreChangeState = NvxCCPreChangeState;

    /* allocate and initialize clock component state structure */
    pState = NvOsAlloc(sizeof(NvxCCState));
    if(!pState)
    {
        NVXTRACE((NVXT_ERROR,pThis->eObjectType, "ERROR:NvxClockComponentInit:"
            ":pState = %d:[%s(%d)]\n",pState,__FILE__, __LINE__));
        return OMX_ErrorInsufficientResources;
    }
    NvOsMemset(pState, 0, sizeof(NvxCCState));
    pThis->pComponentData = pState;
    pState->pNvComp = pThis;
    pState->eActiveClock = OMX_TIME_RefClockNone;
    pState->nRequests = 0;
    pState->xScale = 0x10000;
    pState->oClockState.nSize = sizeof(pState->oClockState);
    pState->oClockState.nVersion = NvxComponentSpecVersion;
    pState->oClockState.eState = OMX_TIME_ClockStateStopped;
    pState->oClockState.nStartTime = 0;
    pState->oClockState.nOffset = 0;
    pState->oClockState.nWaitMask = 0;
    pState->oPendingClockState = pState->oClockState;
    pState->uGotStartTimeMask = 0;

    NvxMutexCreate(&pState->oLock);

    if (NvSuccess != NvMMCreateMediaClock(&pState->hClock))
        return OMX_ErrorInsufficientResources; 

    /* initialize clock ports and client start times */
    for (i=0;i<8;i++)
    {
        NvxPortInitOther(pThis->pPorts + i, OMX_DirOutput, 4,
            sizeof(OMX_TIME_MEDIATIMETYPE), OMX_OTHER_FormatTime);
        pThis->pPorts[i].eNvidiaTunnelTransaction = ENVX_TRANSACTION_CLOCK;
        pState->sllStartTime[i] = 0;
    }
    NVXTRACE((NVXT_CALLGRAPH, pThis->eObjectType , "+NvxClockComponentInit\n"));
    return eError;
}

