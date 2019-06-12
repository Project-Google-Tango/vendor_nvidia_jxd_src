/* Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvos.h"
#include "nvmm_mediaclock.h"

#define VERYLATETIME 0x7ffffff

/* Media clock implementation */

static NvS64 GetWallclockTime(void)
{
    return NvOsGetTimeUS() * 10; // convert to 100 nanosecond units
}

/* Set of internal state required for one media clock */
typedef struct {
    NvBool bFree;
    NvMMClockState State;
    NvU32 Mask;
    NvOsMutexHandle Lock;
    NvMMRate Rate;
    NvMMClockClientType RefType;
    NvS64 MinStartTime;
    NvMMTimestamp RefTime[NvMMClockClientType_Max];
    NvMMClockCallback Callback[NvMMClockClientType_Max];
    void *CallbackContext[NvMMClockClientType_Max];

    NvS64 wtDiffSave;

    NvOsSemaphoreHandle oWaitAudioSem;
    NvOsSemaphoreHandle oWaitVideoSem;
    NvU32 bWaitForAudio;
    NvU32 bWaitForVideo;
    NvU32 bDoneFirstFrame;
    NvU32 bSetFirstAudio;

    NvU32 nFramesSinceRateChange;
    NvBool bDontWait;
    NvBool bWaiting;
} MediaClock;

// Global set of clocks (todo: dynamically create clocks)
#define MAX_CLOCKS 32
static MediaClock g_Clock[MAX_CLOCKS]; 

// Lock for clocks' creation/destruction 
static NvOsMutexHandle g_Lock;   

/* Initialize all state maintained for media clocks. 
   This function should only be called once at NvMM init time */
NvError 
NvMMInitMediaClocks(void)
{
    int i; 

    // initially all clocks are free
    for (i=0;i<MAX_CLOCKS;i++)
    {
        NvOsMemset(&(g_Clock[i]), 0, sizeof(MediaClock));
        g_Clock[i].bFree = NV_TRUE;
    }
    
    /* Create the mutex used to safely create/destroy clocks */
    return NvOsMutexCreate( &g_Lock );    
}

/* De-initialize all state maintained for media clocks.
   This function should only be called once at NvMM de-init time */
NvError 
NvMMDeInitMediaClocks(void)
{
    /* Only need to destroy the lock. All clocks should be detroyed by now */
    NvOsMutexDestroy(g_Lock );    
    return NvSuccess;
}

/* "Create" a new media clock by returing the next available one. 
   Perform any necessary clock initialization. */
NvError 
NvMMCreateMediaClock(
    NvMMClockHandle *phClock) 
{ 
    int i,j;
    MediaClock *pClock;
    NvError Status;

    NvOsMutexLock(g_Lock);

    /* look for an available clock */
    for (i=0; i< MAX_CLOCKS;i++)
    {
        /* if this one is free initialize it */
        if (g_Clock[i].bFree){
            *phClock = i;
            pClock = &g_Clock[i];
            pClock->State = NvMMClockState_Stopped;
            pClock->Mask = 0;
            pClock->RefType = NvMMClockClientType_Audio;
            pClock->RefTime[pClock->RefType].WallTime = GetWallclockTime();
            pClock->RefTime[pClock->RefType].MediaTime = 0;
            pClock->bFree = NV_FALSE;
            pClock->Rate = 1000;
            pClock->wtDiffSave = 0;
            pClock->bDoneFirstFrame = 0;
            pClock->bSetFirstAudio = 0;
            pClock->bWaitForAudio = 0;
            pClock->bWaitForVideo = 0;
            pClock->nFramesSinceRateChange = 0;

            if (NvSuccess != NvOsSemaphoreCreate(&pClock->oWaitAudioSem, 0))
                return NvError_InsufficientMemory;
            if (NvSuccess != NvOsSemaphoreCreate(&pClock->oWaitVideoSem, 0))
            {
                NvOsSemaphoreDestroy(pClock->oWaitAudioSem);
                return NvError_InsufficientMemory;
            }
            for (j=0;j<NvMMClockClientType_Max;j++){
                pClock->Callback[j] = NULL;
            }

            Status = NvOsMutexCreate(&pClock->Lock);
            if (Status != NvSuccess)
            {
                NvOsSemaphoreDestroy(pClock->oWaitAudioSem);
                NvOsSemaphoreDestroy(pClock->oWaitVideoSem);
            }
            
            NvOsMutexUnlock(g_Lock);
            return Status;
        }   
    }

    NvOsMutexUnlock(g_Lock);
    return NvError_InsufficientMemory;
}

/* Destroy the given media clock */
NvError 
NvMMDestroyMediaClock(
    NvMMClockHandle hClock)
{
    MediaClock *pClock = &g_Clock[hClock];

    NvOsMutexLock(g_Lock);

    if (hClock >= MAX_CLOCKS) return NvError_BadParameter;
    pClock->bFree = NV_TRUE; // mark the clock as available
    NvOsMutexDestroy(pClock->Lock); 
    NvOsSemaphoreDestroy(pClock->oWaitAudioSem);
    NvOsSemaphoreDestroy(pClock->oWaitVideoSem);
    
    NvOsMutexUnlock(g_Lock);
    return NvSuccess;
}


/* Register a callback from the clock to a client to inform it of 
   new clock state or rate. */
NvError
NvMMRegisterClockCallback(
    NvMMClockHandle hClock,
    NvMMClockClientType Type,
    void *pContext,
    NvMMClockCallback callback )
{
    MediaClock *pClock = &g_Clock[hClock];

    NvOsMutexLock(pClock->Lock);
    
    // save the supplied callback and its context
    pClock->Callback[Type] = callback;
    pClock->CallbackContext[Type] = pContext;

    NvOsMutexUnlock(pClock->Lock);
    return NvSuccess;
}

/* Internal helper function to notify the clock clients of big changes */
static NvError 
UpdateClients(
    NvMMClockHandle hClock)
{
    MediaClock *pClock = &g_Clock[hClock];
    int i;
    NvMMClockCallback callback;
    void *pContext;
    NvMMClockState State;
    NvMMRate Rate;

    NvOsMutexLock(pClock->Lock);    

    // tell each client that registered a callback */
    for(i=0;i<NvMMClockClientType_Max;i++)
    {
        if (pClock->Callback[i])
        {
            /* copy everything to local vars so we can release mutex during callback */
            callback = pClock->Callback[i];
            pContext = pClock->CallbackContext[i];
            State = pClock->State;
            Rate = pClock->Rate;
            
            /* call client */
            NvOsMutexUnlock(pClock->Lock);
            callback(pContext, hClock, State, Rate);
            NvOsMutexLock(pClock->Lock);    
        }
    }

    NvOsMutexUnlock(pClock->Lock);
    return NvSuccess;
}

/* Supply the media clock the given reference from the given reference
   clock */
NvError 
NvMMSupplyReferenceTime(
    NvMMClockHandle hClock, 
    NvMMClockClientType Type, 
    NvMMTimestamp RefTime)
{
    MediaClock *pClock = &g_Clock[hClock];
    NvMMTimestamp tsNow;

    // save the supplied reference time
    //NvOsMutexLock(pClock->Lock);
    
    NvMMGetTime(hClock, &tsNow);

    if (Type == NvMMClockClientType_Audio)
    {
        if (RefTime.MediaTime != 0 || !pClock->bSetFirstAudio)
        {
//NvOsDebugPrintf("updating audio master: %f %f\n", RefTime.MediaTime / 10000.0, tsNow.MediaTime / 10000.0);
            if (tsNow.MediaTime > 15000000 && 
                RefTime.MediaTime > 15000000) // 1.5 seconds
            {
                RefTime.MediaTime = (RefTime.MediaTime + tsNow.MediaTime) / 2;
            }

            RefTime.WallTime = tsNow.WallTime;
            pClock->RefTime[Type] = RefTime;
            pClock->bSetFirstAudio = 1;
        }
        NvOsSemaphoreSignal(pClock->oWaitAudioSem);
    }
    else 
        pClock->RefTime[Type] = RefTime;

    //NvOsMutexUnlock(pClock->Lock);

    return NvSuccess;
}

/* Set the reference clock type. This defaults to audio. */
NvError 
NvMMSetReference(
    NvMMClockHandle hClock, 
    NvMMClockClientType Type)
{
    MediaClock *pClock = &g_Clock[hClock];
    NvMMTimestamp temp;

    // Get current time
    NvMMGetTime(hClock, &temp);

    // Reset reference time to now, change the reference type
    NvOsMutexLock(pClock->Lock);
    pClock->RefTime[Type] = temp;
    pClock->RefType = Type;
    NvOsMutexUnlock(pClock->Lock);
    
    return NvSuccess;
}

/* Get the current time: both media time and wallclock time */
NvError 
NvMMGetTime(
    NvMMClockHandle hClock, 
    NvMMTimestamp *pTime)
{
    MediaClock *pClock = &g_Clock[hClock];
    NvMMTimestamp RefTime;
    NvMMRate Rate;

    /* copy everything to local vars so we can release mutex */
    //NvOsMutexLock(pClock->Lock);
    RefTime = pClock->RefTime[pClock->RefType];
    Rate = pClock->Rate;
    //NvOsMutexUnlock(pClock->Lock);

    if (pClock->State == NvMMClockState_Stopped)
        Rate = 0;

    /* Get current wallclock time.
       Extrapolate current media time from reference and wall time. 
       Accommodate current rate in calculation. */
    pTime->WallTime = GetWallclockTime();
    pTime->MediaTime = RefTime.MediaTime + 
        ((pTime->WallTime - RefTime.WallTime) * Rate)/1000; 

    return NvSuccess;
}

/* Calculate the amount of wallclock time until the given media time */
NvError 
NvMMGetTimeUntilMediaTime(
    NvMMClockHandle hClock, 
    NvS64 MediaTime,
    NvS64 *pWallTime)
{   
    MediaClock *pClock = &g_Clock[hClock];
    NvS64 MTNow;
    NvMMTimestamp RefTime;
    NvMMRate Rate;

    /* copy everything to local vars so we can release mutex */
    //NvOsMutexLock(pClock->Lock);
    RefTime = pClock->RefTime[pClock->RefType];
    Rate = pClock->Rate;
    //NvOsMutexUnlock(pClock->Lock);

    /* Calculate the current media time. 
       Calculate the duration until the given media time from 
       current media time and rate. */
    MTNow = RefTime.MediaTime + 
        ((GetWallclockTime() - RefTime.WallTime) * Rate)/1000;

    if (Rate != 0)
        *pWallTime = ((MediaTime - MTNow)*1000)/Rate; 
    else
        *pWallTime = 0;   

    return NvSuccess;
}

NvError
NvMMWaitUntilMediaTime(
    NvMMClockHandle hClock,
    NvS64 MediaTime,
    NvMMClockFrameStatus *eStatus)
{
    MediaClock *pClock = &g_Clock[hClock];
    NvU32 bReady = 0;
    NvS64 sleepTime = 33;
    NvS64 ticksDue = 0;
    NvMMRate saveRate;

    *eStatus = NvMMClock_FrameOK;

    if (!pClock->bDoneFirstFrame)
    {
        if (pClock->bWaitForVideo)
        {
            NvOsSemaphoreSignal(pClock->oWaitVideoSem);
            pClock->bWaitForVideo = 0;
        }

        if (!pClock->bWaitForAudio)
        {
            NvMMTimestamp RefTime;
            NvMMGetTime(hClock, &RefTime);

            RefTime.MediaTime = MediaTime;
            RefTime.WallTime = GetWallclockTime();

            pClock->RefTime[pClock->RefType] = RefTime;
        }
        pClock->bDoneFirstFrame = 1;
    }

    if (pClock->bWaitForAudio)
    {
        if (NvOsSemaphoreWaitTimeout(pClock->oWaitAudioSem, 2000) == NvError_Timeout)
        {
            NvMMTimestamp RefTime;
            NvMMGetTime(hClock, &RefTime);

            RefTime.MediaTime = MediaTime;
            RefTime.WallTime = GetWallclockTime();

            pClock->RefTime[pClock->RefType] = RefTime; 
        }
        pClock->bWaitForAudio = 0;
    }

    NvMMGetTimeUntilMediaTime(hClock, MediaTime, &ticksDue);

//{
//NvMMTimestamp mtNow;
//NvMMGetTime(hClock, &mtNow);
//NvOsDebugPrintf("Incoming buffer ts: %f -- audio clock %f -- video ahead %f\n", MediaTime / 10000.0, mtNow.MediaTime / 10000.0, ticksDue / 10000.0);
//}

    if ((ticksDue / 10000) < -15) // 15ms late is behind
        *eStatus |= NvMMClock_FrameBehind;
    if ((ticksDue / 10000) < -80) // 80ms late is _late_
        *eStatus |= NvMMClock_FrameShouldDrop;

    sleepTime = ticksDue / 10000;
    if (sleepTime > 1000)
        sleepTime = 1000;

    if (ticksDue < 0)
        bReady = 1;
   
    saveRate = pClock->Rate;

    if (pClock->nFramesSinceRateChange > 0)
    {
        pClock->nFramesSinceRateChange--;

        if (saveRate != 1.0 && ticksDue / 10000 > 40000)
            return NvSuccess;
    }

    pClock->bWaiting = NV_TRUE;
    while (!bReady && sleepTime > 33 && (NvU32)sleepTime > 33 && 
           pClock->Rate == saveRate)
    {
        (void)NvOsSemaphoreWaitTimeout(pClock->oWaitAudioSem,
                                       (NvU32)sleepTime - 33);
        NvMMGetTimeUntilMediaTime(hClock, MediaTime, &ticksDue);
        if (ticksDue < 0)
            bReady = 1;
        sleepTime = ticksDue / 10000;

        // Arbitrary, if it's more than 11 (previous 1 second wait) in the 
        // future, just exit
        if ((sleepTime > 10000 && saveRate == 1.0) || sleepTime > 40000)
            break;
        if (pClock->bDontWait)
            break;
    }

    pClock->bWaiting = NV_FALSE;
    pClock->bDontWait = NV_FALSE;
    return NvSuccess;
}

void
NvMMUnblockWaitUntilMediaTime(
    NvMMClockHandle hClock)
{
    MediaClock *pClock = &g_Clock[hClock];

    if (pClock->bWaiting)
    {
        pClock->bDontWait = NV_TRUE;
        NvOsSemaphoreSignal(pClock->oWaitAudioSem);
    }
}

/* Set the rate at which the clock runs. Default is 1X. */
NvError 
NvMMSetClockRate(
    NvMMClockHandle hClock, 
    NvMMRate Rate )
{
    MediaClock *pClock = &g_Clock[hClock];
    NvMMTimestamp temp;

    // get current time
    NvMMGetTime(hClock, &temp);

    // reset reference time to now and apply the rate change
    NvOsMutexLock(pClock->Lock);
    pClock->RefTime[pClock->RefType] = temp;

    if (Rate == 0 && pClock->Rate != 0)
    {
        NvS64 wtNow = GetWallclockTime();
        pClock->wtDiffSave = (wtNow - temp.WallTime);
    }
    else if (pClock->Rate == 0 && Rate != 0)        
    {
        NvS64 wtNow = GetWallclockTime();
        temp.WallTime = wtNow - pClock->wtDiffSave;
        pClock->wtDiffSave = 0;
    }

    if ((pClock->Rate > 0 && Rate < 0) ||
        (Rate > 0 && pClock->Rate < 0))
    {
        pClock->nFramesSinceRateChange = 2;
    }

    pClock->Rate = Rate;
    NvOsMutexUnlock(pClock->Lock);

    // tell the clients
    UpdateClients(hClock);

    return NvSuccess;
}

/* Retrieve the current rate of the clock */
NvError 
NvMMGetClockRate(
    NvMMClockHandle hClock, 
    NvMMRate *pRate )
{
    MediaClock *pClock = &g_Clock[hClock];

    // retreive the current rate
    NvOsMutexLock(pClock->Lock);
    *pRate = pClock->Rate;
    NvOsMutexUnlock(pClock->Lock);

    return NvSuccess;
}

/* Set the start time of the given stream */
NvError 
NvMMSetStartTime(
    NvMMClockHandle hClock, 
    NvMMClockClientType Type, 
    NvS64 StartTime
    )
{
    MediaClock *pClock = &g_Clock[hClock];

    NvOsMutexLock(pClock->Lock);
    
    // track the earliest starttime
    if (StartTime < pClock->MinStartTime){
        pClock->MinStartTime = StartTime;
    }
    pClock->Mask = pClock->Mask & (~(1<<Type)); // clear this startTime in mask
    
    if ((pClock->Mask == 0) && 
        pClock->State == NvMMClockState_WaitingForStarttimes)
    {
        // set the reference time 
        pClock->RefTime[pClock->RefType].WallTime = GetWallclockTime();
        pClock->RefTime[pClock->RefType].MediaTime = pClock->MinStartTime;

        // start the clock
        pClock->State = NvMMClockState_Running;

        // tell the clients
        NvOsMutexUnlock(pClock->Lock);
        UpdateClients(hClock);
    }
    else {
        NvOsMutexUnlock(pClock->Lock);
    }

    return NvSuccess;
}

/* Start the clock */
NvError 
NvMMStartClock(
    NvMMClockHandle hClock,
    NvU32 Mask, 
    NvS64 StartTime)
{
    MediaClock *pClock = &g_Clock[hClock];

    NvOsMutexLock(pClock->Lock);

    // apply the given mask (wait on streams set in the mask) 
    pClock->Mask = Mask;

    // are we waiting or starting immediately?
    if (Mask)
    {
        // wait for the prescribed streams
        pClock->State = NvMMClockState_WaitingForStarttimes; 
        pClock->MinStartTime = VERYLATETIME;
    } 
    else 
    {
        // start clock immediately and use supplied time
        pClock->State = NvMMClockState_Running;
        pClock->RefTime[pClock->RefType].WallTime = GetWallclockTime();
        pClock->RefTime[pClock->RefType].MediaTime = StartTime;
    }
    
    pClock->bSetFirstAudio = 0;

    NvOsMutexUnlock(pClock->Lock);

    // tell the clients
    UpdateClients(hClock);

    return NvSuccess;
}

/* Stop the clock */
NvError 
NvMMStopClock(
    NvMMClockHandle hClock)
{
    MediaClock *pClock = &g_Clock[hClock];
   
    NvOsMutexLock(pClock->Lock);

    // Stop the clock
    pClock->State = NvMMClockState_Stopped;
    pClock->bSetFirstAudio = 0;

    NvOsMutexUnlock(pClock->Lock);

    // tell the clients
    UpdateClients(hClock);

    return NvSuccess;
}

void
NvMMWaitForVideoToStart(
    NvMMClockHandle hClock)
{
    MediaClock *pClock = &g_Clock[hClock];

    if (pClock->bWaitForVideo)
    {
        NvOsSemaphoreWaitTimeout(pClock->oWaitVideoSem, 2000);
    }
}


void 
NvMMSetWaitForAudio(
    NvMMClockHandle hClock)
{
    MediaClock *pClock = &g_Clock[hClock];

    pClock->bWaitForAudio = 1;
}

void
NvMMSetWaitForVideo(
    NvMMClockHandle hClock)
{
    MediaClock *pClock = &g_Clock[hClock];

    pClock->bWaitForVideo = 1;
}

