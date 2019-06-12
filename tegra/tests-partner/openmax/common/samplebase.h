/* Copyright (c) 2006 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property 
 * and proprietary rights in and to this software, related documentation 
 * and any modifications thereto.  Any use, reproduction, disclosure or 
 * distribution of this software and related documentation without an 
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef SAMPLEBASE_H_
#define SAMPLEBASE_H_

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Index.h>
#include <OMX_Video.h>
#include <OMX_Audio.h>
#include <OMX_Other.h>
#include <OMX_Image.h>
#include <NVOMX_IndexExtensions.h>

#include "nvos.h"
#include "nvtest.h"

#include <stdlib.h>
#include <string.h>

// Helpful macros
#define NOTSET_U8 ((OMX_U8)0xDE)
#define NOTSET_U16 ((OMX_U16)0xDEDE)
#define NOTSET_U32 ((OMX_U32)0xDEDEDEDE)
#define INIT_PARAM(_X_)  (memset(&(_X_), NOTSET_U8, sizeof(_X_)), ((_X_).nSize = sizeof (_X_)), (_X_).nVersion = vOMX)

#define NVX_MAX_OMXPLAYER_GRAPHS 5

// Structures for handling a component graph
typedef struct _CompList
{
    OMX_HANDLETYPE hComp;
    struct _CompList *next;
} CompList;

typedef struct 
{
    struct _CompList *head;
    OMX_HANDLETYPE hEndComponent;
    OMX_HANDLETYPE hEndComponent2;
    OMX_HANDLETYPE hCameraComponent;
    NvOsSemaphoreHandle hTimeEvent;
    NvOsSemaphoreHandle hCameraStillCaptureReady;
    NvOsSemaphoreHandle hCameraHalfPress3A;
    NvOsSemaphoreHandle hCameraPreviewPausedAfterStillCapture;
    OMX_U64 TimeStampCameraReady;
    OMX_U64 TimeStampPreviewPausedAfterStillCapture;
    OMX_U64 nTSAutoFocusAchieved;
    OMX_U64 nTSAutoExposureAchieved;
    OMX_U64 nTSAutoWhiteBalanceAchieved;
    unsigned char eos;
    int needEOS;
    OMX_BOOL EosEvent;
    OMX_ERRORTYPE eErrorEvent;
    OMX_BOOL bWaitOnError;
    OMX_BOOL bNoTimeOut;
    OMX_BOOL mProcessBufferingEvent;
    OMX_U32 BufferingPercent;        
    OMX_BOOL ChangeStateToPause;
} NvxCompGraph;

// The main SampleBase test context.
typedef struct {
    int nExecuting;
    int nInitialized;
    int nFinished;
    int nFinished2;
    OMX_U64 nTSBufferEOSReceived;

    OMX_BOOL eventEnd;
    OMX_BOOL eventEnd2;
    OMX_BOOL eventError;
    OMX_BOOL bSignalSinglePortFlush;
    OMX_ERRORTYPE errorType;

    NvOsSemaphoreHandle hGeneralEvent;
    NvOsSemaphoreHandle hFlushEvent;
    NvOsSemaphoreHandle hMainEndEvent;
    NvOsSemaphoreHandle hErrorEvent;
    NvOsSemaphoreHandle hPortEvent;

    OMX_HANDLETYPE hEndComponent;
    OMX_HANDLETYPE hEndComponent2;
    OMX_HANDLETYPE hDecoder;
    OMX_HANDLETYPE hEncoder;

    NvxCompGraph *pCompGraph[NVX_MAX_OMXPLAYER_GRAPHS];
    OMX_ERRORTYPE (*pOnError)(OMX_HANDLETYPE hComp1, OMX_HANDLETYPE hComp2, OMX_ERRORTYPE eError);
    void (*pEOTCallback)(NvxCompGraph *pGraph);
    void (*pDLACallback)(NvU16 *licenseUrl,NvU32 licenseurlSize,NvU8  *licenseChallenge,NvU32 licenseChallengeSize);
} TestContext;

extern OMX_VERSIONTYPE vOMX;

// Initialize the OMX core
OMX_ERRORTYPE NvOMXSampleInit(TestContext *pContext, OMX_CALLBACKTYPE *pCallbacks);
// Close OMX
void NvOMXSampleDeinit(TestContext *pContext);

// Generic event handler for all OMX components
OMX_ERRORTYPE Test_EventHandler(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
                                OMX_EVENTTYPE eEvent, OMX_U32 nData1,
                                OMX_U32 nData2, OMX_PTR pEventData);

// Generic callback for all OMX components
OMX_ERRORTYPE Test_EmptyBufferDone(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
                                   OMX_BUFFERHEADERTYPE* pBuffer);

// Generic callback for all OMX components
OMX_ERRORTYPE Test_FillBufferDone(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
                                  OMX_BUFFERHEADERTYPE* pBuffer);


// Wait for a component to transition to the specified state
OMX_ERRORTYPE WaitForState(OMX_HANDLETYPE hComponent, OMX_STATETYPE eTestState,
                           OMX_STATETYPE eTestState2, TestContext *pAppData,
                           NvxCompGraph *pCompGraph);

// Wait for hEndComponent to hit EOS
void WaitForEnd(TestContext *pAppData);
// Wait for hEndComponent2 to hit EOS
void WaitForEnd2(TestContext *pAppData, int count);

// Platform/system specific code would go here
OMX_ERRORTYPE SampleInitPlatform(void);
void SampleDeinitPlatform(void);

// Seek To Time
void SeekToTime(NvxCompGraph *pCompGraph, OMX_HANDLETYPE hClock, OMX_HANDLETYPE hComp, int timeInMilliSec, TestContext *pAppData);

// Start the clock
void StartClock(OMX_HANDLETYPE hClock, NvS64 timeInMilliSec);
// Stop the clock
void StopClock(OMX_HANDLETYPE hClock);


// Add a component to the global list
void AddComponent(OMX_HANDLETYPE hComp);
// Transition all components in the global list to the given state, and 
// optionally wait until they all get to that state
void TransitionAllToState(OMX_STATETYPE eState, OMX_BOOL bWait, 
                          TestContext *pAppData);
// Free all components from the global list
void FreeAllComponents(void);

// Set the clock to 0.0x speed
void PauseClockGraph(OMX_HANDLETYPE hClock);
// Set the clock to 1.0x
void UnPauseClockGraph(OMX_HANDLETYPE hClock);

// Wait for a given length of time or EOS, then return
void WaitForEndTime(NvxCompGraph *pCompGraph, NvxTimeMs uPlayTime,
                    TestContext *pAppData);

void WaitForAnyEndOrTime(TestContext *pAppData, NvxTimeMs uPlayTime);

void WaitForAnyErrorOrTime(TestContext *pAppData, NvxTimeMs uPlayTime);

// Add a component to a specific graph
OMX_ERRORTYPE AddComponentToGraph(NvxCompGraph *pCompGraph,
                                  OMX_HANDLETYPE hComp);
// Free all known components from a graph
OMX_ERRORTYPE FreeAllComponentsFromGraph(NvxCompGraph *pCompGraph);
// Transition one component to a given state.  Optionally, wait until 
// it reaches that state
OMX_ERRORTYPE TransitionComponentToState(OMX_HANDLETYPE hComponent, 
                                         OMX_STATETYPE eState, OMX_BOOL bWait, 
                                         TestContext *pAppData);
// Transition all components to a given state.  Optionally, wait until 
// they all reach that state
OMX_ERRORTYPE TransitionAllComponentsToState(NvxCompGraph *pCompGraph, 
                                             OMX_STATETYPE eState, 
                                             OMX_BOOL bWait,
                                             TestContext *pAppData);
OMX_ERRORTYPE FindComponentInGraph(NvxCompGraph *pCompGraph, OMX_HANDLETYPE hComp);

void SetRateOnGraph(NvxCompGraph *pCompGraph, OMX_HANDLETYPE hClock,
                    OMX_HANDLETYPE hComp, float playSpeed, 
                    TestContext *pAppData);

void SetRateAndReplay(NvxCompGraph *pCompGraph, OMX_HANDLETYPE hClock, OMX_HANDLETYPE hComp,
                      float playSpeed, TestContext *pAppData);

void FlushAllPorts(NvxCompGraph *pCompGraph, TestContext *pAppData);

#endif
