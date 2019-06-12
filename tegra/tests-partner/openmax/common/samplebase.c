/* Copyright (c) 2006 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property 
 * and proprietary rights in and to this software, related documentation 
 * and any modifications thereto.  Any use, reproduction, disclosure or 
 * distribution of this software and related documentation without an 
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "testassert.h"
#include "samplebase.h"
#include "nvmm_camera.h"

#define NV_DEBUG_OMXTESTS (0)

#if (NV_DEBUG_OMXTESTS)
#define NvDebugOmxtests(x) NvOsDebugPrintf x
#else
#define NvDebugOmxtests(x)
#endif

OMX_VERSIONTYPE vOMX;

// Initialize the OMX core
OMX_ERRORTYPE NvOMXSampleInit(TestContext *pContext, OMX_CALLBACKTYPE *pCallbacks)
{
    NvError err;
    OMX_ERRORTYPE eError;
    OMX_U32 i;

    vOMX.s.nVersionMajor = 1;
#if OMXVERSION > 1
    vOMX.s.nVersionMinor = 1;
#else
    vOMX.s.nVersionMinor = 0;
#endif
    vOMX.s.nRevision = 0;
    vOMX.s.nStep = 0;

    eError = OMX_Init();
    TEST_ASSERT_NOTERROR(eError);

    pContext->nExecuting     = 0;
    pContext->nInitialized   = 0;
    pContext->nFinished      = 0;
    pContext->nFinished2     = 0;

    pContext->eventError    = OMX_FALSE;
    pContext->errorType     = OMX_ErrorNone;

    pContext->hGeneralEvent  = 0;
    pContext->hFlushEvent    = 0;
    pContext->hEndComponent  = 0;
    pContext->hEndComponent2 = 0;
    pContext->hDecoder       = 0;
    pContext->hEncoder       = 0;

    err = NvOsSemaphoreCreate(&pContext->hGeneralEvent, 0);
    if (err != NvSuccess)
        return OMX_ErrorInsufficientResources;

    err = NvOsSemaphoreCreate(&pContext->hFlushEvent, 0);
    if (err != NvSuccess)
        return OMX_ErrorInsufficientResources;

    err = NvOsSemaphoreCreate(&pContext->hMainEndEvent, 0);
    if (err != NvSuccess)
        return OMX_ErrorInsufficientResources;

    err = NvOsSemaphoreCreate(&pContext->hErrorEvent, 0);
    if (err != NvSuccess)
        return OMX_ErrorInsufficientResources;

    err = NvOsSemaphoreCreate(&pContext->hPortEvent, 0);
    if (err != NvSuccess)
        return OMX_ErrorInsufficientResources;

    pCallbacks->EventHandler    = Test_EventHandler;
    pCallbacks->EmptyBufferDone = Test_EmptyBufferDone;
    pCallbacks->FillBufferDone  = Test_FillBufferDone;

    for (i = 0; i < NVX_MAX_OMXPLAYER_GRAPHS; i++)
    {
        pContext->pCompGraph[i] = NULL;
    }

    return OMX_ErrorNone;
}

// Close OMX
void NvOMXSampleDeinit(TestContext *pContext)
{
    OMX_ERRORTYPE eError;

    eError = OMX_Deinit();
    TEST_ASSERT_NOTERROR(eError);

    NvOsSemaphoreDestroy(pContext->hGeneralEvent);
    NvOsSemaphoreDestroy(pContext->hFlushEvent);
    NvOsSemaphoreDestroy(pContext->hMainEndEvent);
    NvOsSemaphoreDestroy(pContext->hErrorEvent);
    NvOsSemaphoreDestroy(pContext->hPortEvent);
}

// Generic event handler for all OMX components
OMX_ERRORTYPE Test_EventHandler(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
                                OMX_EVENTTYPE eEvent, OMX_U32 nData1,
                                OMX_U32 nData2, OMX_PTR pEventData)
{
    TestContext* pContext = pAppData;
    OMX_BOOL bSetEvent = OMX_TRUE;
    OMX_U32 i;

    switch (eEvent)
    {
    case OMX_EventCmdComplete:
        if (nData1 == OMX_CommandStateSet && nData2 == OMX_StateIdle)
            pContext->nInitialized++;
        else if (nData1 == OMX_CommandStateSet && nData2 == OMX_StateExecuting)
            pContext->nExecuting++;
        else if ((nData1 == OMX_CommandFlush) && ((OMX_ALL == nData2) || pContext->bSignalSinglePortFlush))
            NvOsSemaphoreSignal(pContext->hFlushEvent);
        else if (nData1 == OMX_CommandPortDisable)
            NvOsSemaphoreSignal(pContext->hPortEvent); 
        break;

    case OMX_EventError:
        if (OMX_ErrorPortUnpopulated == nData1)
        {
            bSetEvent=OMX_FALSE;
            break;
        }
        if (OMX_ErrorMbErrorsInFrame == nData1)
        {
            pContext->eventError = OMX_TRUE;
            pContext->errorType = OMX_ErrorMbErrorsInFrame;
            break;
        }
        if(nData1 == NvxError_WriterFileSizeLimitExceeded)
        {
            int flag=1;
            for (i = 0; i < NVX_MAX_OMXPLAYER_GRAPHS; i++)
            {
                if (pContext->pCompGraph[i])
                {
                    if (OMX_ErrorNone == FindComponentInGraph(pContext->pCompGraph[i], hComponent))
                    {
                        pContext->pCompGraph[i]->eErrorEvent = (OMX_ErrorNone != nData1 ? nData1 : OMX_ErrorUndefined);
                    }
                    if(flag)
                    {
                        NvOsSemaphoreSignal(pContext->hErrorEvent);
                    }
                    if(nData1 == NvxError_WriterFileSizeLimitExceeded)
                        flag = 0;
                }
            }
            break;
        }

        NvTestPrintf("OMX_EventError %lx, handle %p, info = %s%s", 
                     nData1, hComponent, (char *)pEventData, "");

        pContext->nFinished = 1;
        pContext->nFinished2 = 1 << 16;

        // treat errors like EOS
        for (i = 0; i < NVX_MAX_OMXPLAYER_GRAPHS; i++)
        {
            if (pContext->pCompGraph[i])
            {
                pContext->pCompGraph[i]->EosEvent = OMX_TRUE;
                pContext->pCompGraph[i]->eos = 1;
                pContext->pCompGraph[i]->needEOS = 0;

                if (OMX_ErrorNone == FindComponentInGraph(pContext->pCompGraph[i], hComponent))
                {
                    pContext->pCompGraph[i]->eErrorEvent = (OMX_ErrorNone != nData1 ? nData1 : OMX_ErrorUndefined);
                }

                NvOsSemaphoreSignal(pContext->pCompGraph[i]->hTimeEvent);
                NvOsSemaphoreSignal(pContext->hMainEndEvent);
                NvOsSemaphoreSignal(pContext->hErrorEvent);

                if (pContext->pCompGraph[i]->hCameraStillCaptureReady)
                    NvOsSemaphoreSignal(pContext->pCompGraph[i]->hCameraStillCaptureReady);

                if (pContext->pCompGraph[i]->hCameraHalfPress3A)
                {
                    NvOsSemaphoreSignal(pContext->pCompGraph[i]->hCameraHalfPress3A);
                    NvOsSemaphoreSignal(pContext->pCompGraph[i]->hCameraHalfPress3A);
                    NvOsSemaphoreSignal(pContext->pCompGraph[i]->hCameraHalfPress3A);
                }
                
                if (pContext->pCompGraph[i]->hCameraPreviewPausedAfterStillCapture)
                    NvOsSemaphoreSignal(pContext->pCompGraph[i]->hCameraPreviewPausedAfterStillCapture);
            }
        }
        break;

    case OMX_EventBufferFlag:
        if (nData2 & OMX_BUFFERFLAG_EOS) 
        {
            if (pContext->hEndComponent == hComponent)
            {
                pContext->nFinished = 1;
            }
            if (pContext->hEndComponent2 == hComponent)
            {
                pContext->nFinished2 ++;
                pContext->nTSBufferEOSReceived = NvOsGetTimeUS() * 10; 
            }

            for (i=0; i<NVX_MAX_OMXPLAYER_GRAPHS; i++)
            {
                if (pContext->pCompGraph[i])
                {
                    pContext->pCompGraph[i]->EosEvent = OMX_TRUE;

                    if (pContext->pCompGraph[i]->hEndComponent == hComponent)
                    {
                        NvDebugOmxtests(("OMXPLAYER: EOS received on hEndComponent"));
                        pContext->pCompGraph[i]->needEOS--;
                    }

                    if (pContext->pCompGraph[i]->hEndComponent2 == hComponent)
                    {
                        NvDebugOmxtests(("OMXPLAYER: EOS received on hEndComponent2"));
                        pContext->pCompGraph[i]->needEOS--;
                    }

                    if (0 == pContext->pCompGraph[i]->needEOS)
                    {
                        pContext->pCompGraph[i]->eos = 1;
                        NvOsSemaphoreSignal(pContext->pCompGraph[i]->hTimeEvent);
                        NvOsSemaphoreSignal(pContext->hMainEndEvent);
                    }
                }
            }
        }
        break;

    case OMX_EventMark:
    case OMX_EventPortSettingsChanged:
        break;

    case OMX_EventDynamicResourcesAvailable:
        if (nData1)
        {
            for (i=0; i<NVX_MAX_OMXPLAYER_GRAPHS; i++)
            {
                if (pContext->pCompGraph[i])
                {
                    if (pContext->pCompGraph[i]->hCameraComponent == hComponent)
                    {
                        switch (nData1)
                        {
                            case NVX_EventCamera_AutoFocusAchieved:
                                pContext->pCompGraph[i]->nTSAutoFocusAchieved = *(OMX_U64*)pEventData;
                                NvOsSemaphoreSignal(pContext->pCompGraph[i]->hCameraHalfPress3A);
                                break;
                            case NVX_EventCamera_AutoFocusTimedOut:
                                NvOsSemaphoreSignal(pContext->pCompGraph[i]->hCameraHalfPress3A);
                                break;
                            
                            case NVX_EventCamera_AutoExposureAchieved:
                                pContext->pCompGraph[i]->nTSAutoExposureAchieved = *(OMX_U64*)pEventData;
                                NvOsSemaphoreSignal(pContext->pCompGraph[i]->hCameraHalfPress3A);
                                break;
                            case NVX_EventCamera_AutoExposureTimedOut:
                                NvOsSemaphoreSignal(pContext->pCompGraph[i]->hCameraHalfPress3A);
                                break;
                            
                            case NVX_EventCamera_AutoWhiteBalanceAchieved:
                                pContext->pCompGraph[i]->nTSAutoWhiteBalanceAchieved = *(OMX_U64*)pEventData;
                                NvOsSemaphoreSignal(pContext->pCompGraph[i]->hCameraHalfPress3A);
                                break;
                            case NVX_EventCamera_AutoWhiteBalanceTimedOut:
                                NvOsSemaphoreSignal(pContext->pCompGraph[i]->hCameraHalfPress3A);
                                break;
                            
                            case NVX_EventCamera_StillCaptureReady:
                                pContext->pCompGraph[i]->TimeStampCameraReady = *(OMX_U64*)pEventData;
                                NvOsSemaphoreSignal(pContext->pCompGraph[i]->hCameraStillCaptureReady);
                                break;
                            
                            case NVX_EventCamera_PreviewPausedAfterStillCapture:
                                pContext->pCompGraph[i]->TimeStampPreviewPausedAfterStillCapture = *(OMX_U64*)pEventData;
                                NvOsSemaphoreSignal(pContext->pCompGraph[i]->hCameraPreviewPausedAfterStillCapture);
                                break;
                        }
                    }
                }
            }
        }
        break;

    case NVX_EventBlockWarning:
        NvTestPrintf("NVX_EventBlockWarning %lx, handle %p, info = %s%s\n", 
                     nData1, hComponent, (char *)pEventData, "");
        break;

    case NVX_EventDRM_DirectLicenseAcquisition:
         if (pContext->pDLACallback)
         {
             NvMMEventTracklistErrorInfo *trackListError = (NvMMEventTracklistErrorInfo*)pEventData;
             pContext->pDLACallback(trackListError->licenseUrl,
             trackListError->licenseUrlSize,
             trackListError->licenseChallenge,
             trackListError->licenseChallengeSize);
          }
        break;

    case NVX_EventForBuffering:
        {                 
            for (i=0; i<NVX_MAX_OMXPLAYER_GRAPHS; i++)
            {
                if (OMX_ErrorNone == FindComponentInGraph(pContext->pCompGraph[i], hComponent))
                {
                    pContext->pCompGraph[i]->mProcessBufferingEvent = OMX_TRUE;
                    pContext->pCompGraph[i]->BufferingPercent = nData2;    
                    if ((nData1) && (pContext->pCompGraph[i]->ChangeStateToPause == OMX_FALSE))
                    {
                        NvTestPrintf("NVX_EventForBuffering: Change State to PAUSE"); 
                        pContext->pCompGraph[i]->ChangeStateToPause = OMX_TRUE;
                        NvOsSemaphoreSignal(pContext->hMainEndEvent); 
                    }
                    else if((!nData1) && (pContext->pCompGraph[i]->ChangeStateToPause == OMX_TRUE))
                    {
                        NvTestPrintf("NVX_EventForBuffering: Change State to PLAY"); 
                        pContext->pCompGraph[i]->ChangeStateToPause = OMX_FALSE;
                        NvOsSemaphoreSignal(pContext->hMainEndEvent); 
                    }                                                 
                }
            }
        }

        break; 
    
    default:
        TEST_ASSERT_LT(eEvent, OMX_EventMax);
    }

    if (bSetEvent)
        NvOsSemaphoreSignal(pContext->hGeneralEvent);

    return OMX_ErrorNone;
}

// Generic callback for all OMX components
OMX_ERRORTYPE Test_EmptyBufferDone(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
                                   OMX_BUFFERHEADERTYPE* pBuffer)
{
    return OMX_ErrorNotImplemented;
}

// Generic callback for all OMX components
OMX_ERRORTYPE Test_FillBufferDone(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
                                  OMX_BUFFERHEADERTYPE* pBuffer)
{
    return OMX_ErrorNotImplemented;
}

// Wait for a component to transition to the specified state
OMX_ERRORTYPE WaitForState(OMX_HANDLETYPE hComponent, OMX_STATETYPE eTestState,
                           OMX_STATETYPE eTestState2, TestContext *pAppData,
                           NvxCompGraph *pCompGraph)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_STATETYPE eState;
    int tries = 0;
    NvError semerr = NvSuccess;

    eError = OMX_GetState(hComponent, &eState);
    TEST_ASSERT_NOTERROR(eError);


    while (eState != eTestState && eState != eTestState2) 
    {
        semerr = NvOsSemaphoreWaitTimeout(pAppData->hGeneralEvent, 1000);
        eError = OMX_GetState(hComponent, &eState);
        TEST_ASSERT_NOTERROR(eError);

        if (semerr == NvError_Timeout)
            tries++;

        if (pCompGraph && OMX_ErrorNone != pCompGraph->eErrorEvent && !pCompGraph->bWaitOnError)
            return OMX_ErrorUndefined;
        else if (pCompGraph && OMX_ErrorNone != pCompGraph->eErrorEvent && !pCompGraph->bNoTimeOut && tries > 5)
            return OMX_ErrorUndefined;
    }

    return eError;
}

static void WaitForEndInt(TestContext *pAppData, int *pnFinished, int count)
{
    while (*pnFinished < count) 
    {
        NvOsSemaphoreWait(pAppData->hGeneralEvent);
        if (pAppData->eventError && pAppData->pOnError) 
        {
            pAppData->pOnError(pAppData->hDecoder, pAppData->hEncoder, 
                               pAppData->errorType);
        } 
    }
} 

// Wait for hEndComponent to hit EOS
void WaitForEnd(TestContext *pAppData)
{
     WaitForEndInt(pAppData, &(pAppData->nFinished), 1);
}

// Wait for hEndComponent2 to hit EOS
void WaitForEnd2(TestContext *pAppData, int count)
{
    WaitForEndInt(pAppData, &(pAppData->nFinished2), count);
}

// Platform specific hacks go here
OMX_ERRORTYPE SampleInitPlatform(void)
{
    return OMX_ErrorNone;
}

void SampleDeinitPlatform(void)
{
}

// Start the clock
void StartClock(OMX_HANDLETYPE hClock, NvS64 timeInMilliSec)
{
    OMX_ERRORTYPE eError = OMX_ErrorNotReady;
    OMX_TIME_CONFIG_CLOCKSTATETYPE oClockState;
    
    INIT_PARAM(oClockState);
    oClockState.nOffset = 0;
    oClockState.nStartTime = (OMX_TICKS)(timeInMilliSec)*1000;
    oClockState.nWaitMask = 0;
    oClockState.eState = OMX_TIME_ClockStateRunning;
 
    eError = OMX_SetConfig(hClock, OMX_IndexConfigTimeClockState, &oClockState);
    while (OMX_ErrorNotReady == eError)
    {
        NvOsSleepMS(10);
        eError = OMX_SetConfig(hClock, OMX_IndexConfigTimeClockState,
                               &oClockState);
    }
    TEST_ASSERT_NOTERROR(eError);
}

// Stop the clock
void StopClock(OMX_HANDLETYPE hClock)
{
    OMX_ERRORTYPE eError = OMX_ErrorNotReady;
    OMX_TIME_CONFIG_CLOCKSTATETYPE oClockState;

    INIT_PARAM(oClockState);
    oClockState.nOffset = 0;
    oClockState.nStartTime = 0;
    oClockState.nWaitMask = 0;
    oClockState.eState = OMX_TIME_ClockStateStopped;

    eError = OMX_SetConfig(hClock, OMX_IndexConfigTimeClockState, &oClockState);
    while (OMX_ErrorNotReady == eError) 
    {
        NvOsSleepMS(10);
        eError = OMX_SetConfig(hClock, OMX_IndexConfigTimeClockState,
                               &oClockState);
    }
    TEST_ASSERT_NOTERROR(eError);
}

static CompList *pCompHead = NULL;

// Add a component to the global list
void AddComponent(OMX_HANDLETYPE hComp)
{
    CompList *tmp;
    tmp = malloc(sizeof(CompList));
    tmp->hComp = hComp;
    tmp->next = pCompHead;
    pCompHead = tmp;
}

// Transition all components to the given state, and optionally wait until
// they all get to that state
void TransitionAllToState(OMX_STATETYPE eState, OMX_BOOL bWait, 
                          TestContext *pAppData)
{
    OMX_ERRORTYPE eError;
    CompList *tmp = pCompHead;

    while (tmp)
    {
        if (tmp->hComp)
        {
            eError = OMX_SendCommand(tmp->hComp, OMX_CommandStateSet, eState, 
                                     0);
            TEST_ASSERT_NOTERROR(eError);
        }
        tmp = tmp->next;
    }

    if (!bWait)
        return;

    tmp = pCompHead;
    while (tmp)
    {
        if (tmp->hComp)
            WaitForState(tmp->hComp, eState, OMX_StateInvalid, pAppData, NULL);
        tmp = tmp->next;
    }
}

// Free all components
void FreeAllComponents(void)
{
    CompList *tmp;
    while (pCompHead)
    {
        if (pCompHead->hComp)
            OMX_FreeHandle(pCompHead->hComp);
        tmp = pCompHead->next;
        free(pCompHead);
        pCompHead = tmp;
    }
}

// Add a component to a graph
OMX_ERRORTYPE AddComponentToGraph(NvxCompGraph *pCompGraph,
                                  OMX_HANDLETYPE hComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    CompList *tmp;
    CompList *pComp = pCompGraph->head;

    tmp = malloc(sizeof(CompList));
    tmp->hComp = hComp;
    tmp->next = pComp;
    pComp = tmp;
    pCompGraph->head = pComp;

    return eError;
}

// Free all known components from a graph
OMX_ERRORTYPE FreeAllComponentsFromGraph(NvxCompGraph *pCompGraph)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    CompList *tmp;
    CompList *pComp = pCompGraph->head;
    while (pComp)
    {
        if (pComp->hComp)
        {
            OMX_FreeHandle(pComp->hComp);
        }
        tmp = pComp->next;
        free(pComp);
        pComp = tmp;
    }

    return eError;
}

// Transition one component to a given state.  Optionally, wait until 
// it reaches that state
OMX_ERRORTYPE TransitionComponentToState(OMX_HANDLETYPE hComponent, 
                                         OMX_STATETYPE eState, OMX_BOOL bWait,
                                         TestContext *pAppData)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_STATETYPE eCurrentState;

    if (!hComponent)
    {
        return eError;
    }
    eError = OMX_GetState(hComponent, &eCurrentState);
    TEST_ASSERT_NOTERROR(eError);

    if (eCurrentState != eState)
    {
        eError = OMX_SendCommand(hComponent, OMX_CommandStateSet, eState, 0);
        TEST_ASSERT_NOTERROR(eError);
    }

    if (!bWait)
    {
        return eError;
    }
    WaitForState(hComponent, eState, OMX_StateInvalid, pAppData, NULL);
    return eError;
}

// Transition all components to a given state.  Optionally, wait until 
// they all reach that state
OMX_ERRORTYPE TransitionAllComponentsToState(NvxCompGraph *pCompGraph, 
                                             OMX_STATETYPE eState, 
                                             OMX_BOOL bWait, 
                                             TestContext *pAppData)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_STATETYPE eCurrentState;
    CompList *pComp = pCompGraph->head;

    while (pComp)
    {
        if (pComp->hComp)
        {
            eError = OMX_GetState(pComp->hComp, &eCurrentState);
            TEST_ASSERT_NOTERROR(eError);
            if (eCurrentState != eState)
            {
                eError = OMX_SendCommand(pComp->hComp, OMX_CommandStateSet,
                                         eState, 0);
                if (OMX_ErrorNone != eError)
                    return eError;
                //TEST_ASSERT_NOTERROR(eError);
            }
        }
        pComp = pComp->next;
    }

    if (bWait)
    {
        pComp = pCompGraph->head;
        while (pComp)
        {
            if (pComp->hComp)
            {
                eError = WaitForState(pComp->hComp, eState, OMX_StateInvalid, pAppData, pCompGraph);
                if (OMX_ErrorNone != eError)
                    return eError;
            }
            pComp = pComp->next;
        }
    }

    return eError;
}

OMX_ERRORTYPE FindComponentInGraph(NvxCompGraph *pCompGraph, OMX_HANDLETYPE hComp)
{
    CompList *pComp = pCompGraph->head;
    while (pComp)
    {
        if (pComp->hComp == hComp)
        {
            return OMX_ErrorNone;
        }
        pComp = pComp->next;
    }

    return OMX_ErrorComponentNotFound;
}

// Wait for a given length of time or EOS, then return
void WaitForEndTime(NvxCompGraph *pCompGraph, NvxTimeMs uPlayTime,
                    TestContext *pAppData)
{
    (void)NvOsSemaphoreWaitTimeout(pCompGraph->hTimeEvent, uPlayTime);
}

void WaitForAnyEndOrTime(TestContext *pAppData, NvxTimeMs uPlayTime)
{
    (void)NvOsSemaphoreWaitTimeout(pAppData->hMainEndEvent, uPlayTime);
}

void WaitForAnyErrorOrTime(TestContext *pAppData, NvxTimeMs uPlayTime)
{
    (void)NvOsSemaphoreWaitTimeout(pAppData->hErrorEvent, uPlayTime);
}

void SetRateOnGraph(NvxCompGraph *pCompGraph, OMX_HANDLETYPE hClock,
                    OMX_HANDLETYPE hComp, float playSpeed, 
                    TestContext *pAppData)
{
    OMX_TIME_CONFIG_SCALETYPE oTimeScale;
    CompList *tmp = pCompGraph->head;
    OMX_TIME_CONFIG_TIMESTAMPTYPE now;
    OMX_ERRORTYPE eError;

    // reset EOS count
    if (pCompGraph->hEndComponent && pCompGraph->hEndComponent2)
        pCompGraph->needEOS = 2;
    else
        pCompGraph->needEOS = 1;

    INIT_PARAM(oTimeScale);
    oTimeScale.xScale = NvSFxFloat2Fixed(playSpeed);

    // Pause all components in the graph
    TransitionAllComponentsToState(pCompGraph, OMX_StatePause,
                                   OMX_TRUE, pAppData);
    // Stop the clock
    StopClock(hClock);

    INIT_PARAM(now);
    now.nPortIndex = 0;

    OMX_GetConfig(hClock, OMX_IndexConfigTimeCurrentMediaTime, &now);

    // set the new rate
    while (tmp)
    {
        if (tmp->hComp)
            OMX_SetConfig(tmp->hComp, OMX_IndexConfigTimeScale, &oTimeScale);
        tmp = tmp->next;
    }

    // seek to the new position
    eError = OMX_SetConfig(hComp, OMX_IndexConfigTimePosition, &now);
    TEST_ASSERT_NOTERROR(eError);

    // flush all the buffers from all the ports
    tmp = pCompGraph->head;
    while (tmp)
    {
        if (tmp->hComp)
        {
            eError = OMX_SendCommand(tmp->hComp, OMX_CommandFlush, OMX_ALL, 0);
            TEST_ASSERT_NOTERROR(eError);
            NvOsSemaphoreWait(pAppData->hFlushEvent);
        }
        tmp = tmp->next;
    }

     // Start the clock
    StartClock(hClock, (NvU32)(now.nTimestamp / 1000));

    // Start all components in the graph
    TransitionAllComponentsToState(pCompGraph, OMX_StateExecuting, 
                                   OMX_TRUE, pAppData);
}

void SeekToTime(NvxCompGraph *pCompGraph, OMX_HANDLETYPE hClock, OMX_HANDLETYPE hComp, int timeInMilliSec, TestContext *pAppData)
{
    OMX_ERRORTYPE eError;
    CompList *tmp = pCompGraph->head;
    OMX_TIME_CONFIG_TIMESTAMPTYPE timestamp;

    if (pCompGraph->EosEvent)
        return;

    // reset EOS count
    if (pCompGraph->hEndComponent && pCompGraph->hEndComponent2)
        pCompGraph->needEOS = 2;
    else
        pCompGraph->needEOS = 1;

    INIT_PARAM(timestamp);
    timestamp.nPortIndex = 0;
    timestamp.nTimestamp = (OMX_TICKS)(timeInMilliSec)*1000;

    // Pause all components in the graph
    TransitionAllComponentsToState(pCompGraph, OMX_StatePause,
                                   OMX_TRUE, pAppData);
    // Stop the clock
    StopClock(hClock);

    // seek to the new position
    eError = OMX_SetConfig(hComp, OMX_IndexConfigTimePosition, &timestamp);
    if(eError != OMX_ErrorNone)
    {
        pCompGraph->eos = 1;
        pCompGraph->needEOS = 0;
        pCompGraph->eErrorEvent = eError;
        return;
    }

//    TEST_ASSERT_NOTERROR(eError);

    // flush all the buffers from all the ports
    while (tmp)
    {
        if (tmp->hComp)
        {
            eError = OMX_SendCommand(tmp->hComp, OMX_CommandFlush, OMX_ALL, 0);
            TEST_ASSERT_NOTERROR(eError);
            NvOsSemaphoreWait(pAppData->hFlushEvent);
        }
        tmp = tmp->next;
    }

     // Start the clock
    StartClock(hClock, (NvU32)(timestamp.nTimestamp / 1000));

    // Start all components in the graph
    TransitionAllComponentsToState(pCompGraph, OMX_StateExecuting, 
                                   OMX_TRUE, pAppData);
}

void SetRateAndReplay(NvxCompGraph *pCompGraph, OMX_HANDLETYPE hClock, OMX_HANDLETYPE hComp,
                      float playSpeed, TestContext *pAppData)
{
    OMX_TIME_CONFIG_SCALETYPE oTimeScale;
    CompList *tmp = pCompGraph->head;
    NVX_PARAM_DURATION duration;
    OMX_TIME_CONFIG_TIMESTAMPTYPE timetoseek;
    OMX_ERRORTYPE eError;
    OMX_INDEXTYPE eParam;

    // reset all the EOS state variables
    pCompGraph->eos = 0;
    pCompGraph->EosEvent = OMX_FALSE;
    if (pCompGraph->hEndComponent && pCompGraph->hEndComponent2)
        pCompGraph->needEOS = 2;
    else
        pCompGraph->needEOS = 1;

    INIT_PARAM(oTimeScale);
    oTimeScale.xScale = NvSFxFloat2Fixed(playSpeed);

    // Pause all components in the graph
    TransitionAllComponentsToState(pCompGraph, OMX_StatePause,
                                   OMX_TRUE, pAppData);
    // Stop the clock
    StopClock(hClock);

    // set the new rate
    while (tmp)
    {
        if (tmp->hComp)
            OMX_SetConfig(tmp->hComp, OMX_IndexConfigTimeScale, &oTimeScale);
        tmp = tmp->next;
    }

    // seek to the new position
    INIT_PARAM(timetoseek);
    timetoseek.nPortIndex = 0;

    if (playSpeed < 0.0)
    {
        // REW case, set the new position to the end of stream
        INIT_PARAM(duration);
        eError = OMX_GetExtensionIndex(hComp, NVX_INDEX_PARAM_DURATION, &eParam);
        TEST_ASSERT_NOTERROR(eError);
        eError = OMX_GetParameter(hComp, eParam, &duration);
        TEST_ASSERT_NOTERROR(eError);
        timetoseek.nTimestamp = duration.nDuration - 1000; // to do not trigger EOS again on parser side
    }
    else
    {
        // FF case, set the new position to 0
        timetoseek.nTimestamp = 0;
    }

    eError = OMX_SetConfig(hComp, OMX_IndexConfigTimePosition, &timetoseek);
    TEST_ASSERT_NOTERROR(eError);

    // flush all the buffers from all the ports
    tmp = pCompGraph->head;
    while (tmp)
    {
        if (tmp->hComp)
        {
            eError = OMX_SendCommand(tmp->hComp, OMX_CommandFlush, OMX_ALL, 0);
            TEST_ASSERT_NOTERROR(eError);
            NvOsSemaphoreWait(pAppData->hFlushEvent);
        }
        tmp = tmp->next;
    }

     // Start the clock
    StartClock(hClock, (NvU32)(timetoseek.nTimestamp / 1000));

    // Start all components in the graph
    TransitionAllComponentsToState(pCompGraph, OMX_StateExecuting, 
                                   OMX_TRUE, pAppData);
}

// Set the clock to 0x speed
void PauseClockGraph(OMX_HANDLETYPE hClock)
{
    OMX_ERRORTYPE eError;
    OMX_TIME_CONFIG_SCALETYPE pScale;
    INIT_PARAM(pScale);
    pScale.xScale = 0;

    eError = OMX_SetConfig(hClock, OMX_IndexConfigTimeScale, &pScale);
    TEST_ASSERT_NOTERROR(eError);
}

// Set the clock to 1.0x
void UnPauseClockGraph(OMX_HANDLETYPE hClock)
{
    OMX_ERRORTYPE eError;
    OMX_TIME_CONFIG_SCALETYPE pScale;
    INIT_PARAM(pScale);
    pScale.xScale = 0x10000;

    eError = OMX_SetConfig(hClock, OMX_IndexConfigTimeScale, &pScale);
    TEST_ASSERT_NOTERROR(eError);
}

void FlushAllPorts(NvxCompGraph *pCompGraph, TestContext *pAppData)
{
    CompList *tmp = pCompGraph->head;
    OMX_ERRORTYPE eError;
    // flush all the buffers from all the ports
    while (tmp)
    {
        if (tmp->hComp)
        {
            eError = OMX_SendCommand(tmp->hComp, OMX_CommandFlush, OMX_ALL, 0);
            TEST_ASSERT_NOTERROR(eError);
            NvOsSemaphoreWait(pAppData->hFlushEvent);
        }
        tmp = tmp->next;
    }
}
