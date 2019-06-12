/*
* Copyright (c) 2008-2009 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

#ifndef NVMM_MEDIACLOCK_H_
#define NVMM_MEDIACLOCK_H_

#include "nvmm.h"

/* Clock handle is an integer so it may be easily passed across processor
   boundaries */
typedef unsigned int NvMMClockHandle;

// actual rate = NvMMRate/1000 (e.g. 1000==1X, -2000=2X reverse) 
typedef NvS32 NvMMRate; 

/* Timestamps are (generally) pairs including the media time and the 
   wallclock time at that media time. This allow the clock or its 
   clients to correct for latency */
typedef struct {
     NvS64 MediaTime;
     NvS64 WallTime;
} NvMMTimestamp;

// The states of the clock
typedef enum {
    NvMMClockState_Stopped,
    NvMMClockState_WaitingForStarttimes,
    NvMMClockState_Running,
    NvMMClockState_Force32 = 0x7fffffff
} NvMMClockState;

// Types of clients the media clock can have */
typedef enum {
    NvMMClockClientType_Audio = 0, 
    NvMMClockClientType_Video = 1, 
    NvMMClockClientType_Max
} NvMMClockClientType;

// Frame states, for NvMMWaitUntilMediaTime
typedef enum {
    NvMMClock_FrameOK         = 0, 
    NvMMClock_FrameBehind     = 1, 
    NvMMClock_FrameShouldDrop = 2,
    NvMMClock_FrameForce32    = 0x7fffffff
} NvMMClockFrameStatus;

/* Initialize all state maintained for media clocks. 
   This function should only be called once at NvMM init time */
NvError
NvMMInitMediaClocks(void);

/* De-initialize all state maintained for media clocks.
   This function should only be called once at NvMM de-init time */
NvError
NvMMDeInitMediaClocks(void);

/** 
  *@brief Create a new media clock by returning the next available one 
  * and perform any necessary clock initialization. 
  * 
  *        This function provides a media clock to be shared amongst all clients. Creation mostly entails the allocation of a initialized state structure. Blocks are made aware of their media clock via a dedicated media clock block attribute. 
  *
  * @param [in] phClock is a (non-pointer) handle to the created media clock
  */
NvError 
NvMMCreateMediaClock(
    NvMMClockHandle *phClock); 

/**
  * @brief Destroy the given media clock 
  *
  * This function destroys the given media clock.
  *
  * @param [in] hClock is a (non-pointer) handle to the media clock to 
  * destroy.
  */
NvError 
NvMMDestroyMediaClock(
    NvMMClockHandle hClock);

/**
  * @brief Callback from the a clock to a client to inform it of new clock 
  *    state or rate. 
  * 
  * This function is only called when the clock changes its State or Rate. The client must take appropriate action (see below).        
  *
  * @param [in] pContext is the client context pointer provided upon 
  *    registration
  * @param [in] hClock is the clock performing the callback
  * @param [in] State is the current state of the clock
  * @param [in] Rate is the current rate of the clock
  */
typedef 
NvError 
(*NvMMClockCallback)(
    void *pContext, 
    NvMMClockHandle  hClock,
    NvMMClockState State,
    NvMMRate Rate);

/**
  * @brief Register a callback from the a clock to a client to inform it 
  *    of new clock state or rate. 
  *
  * @param [in] hClock is a (non-pointer) handle to the media clock to 
  *    register against
  * @param [in] Type is the clock client type (e.g. audio or video)
  * @param [in] pContext is the clients context pointer returned on the 
  *    callback
  * @param [in]        Callback is a function of the type NvMMClockCallback
  */
NvError
NvMMRegisterClockCallback(
    NvMMClockHandle hClock,
    NvMMClockClientType Type,
    void *pContext,
    NvMMClockCallback callback );

/**
  * @brief Supply the media clock the given reference from the given 
  *    reference clock 
  * 
  * This function provides the NvMM media clock with a reference from which media time is derived
  *
  * @param [in] hClock is the handle the applicable media clock 
  * @param [in] Type = {audio, video, etc}
  * @param [in] RefTime is a reference time including:
  *   - MediaTime is the timestamp of the sample currently being 
  *     rendered/captured for the stream of the given type.
  *   - WallclockTime is the timestamp of a common clock all of NvMM 
  *     has access to which is not correlated with media playback 
  *     (this is to correct for latency in execution of this function)
  */
NvError 
NvMMSupplyReferenceTime(
    NvMMClockHandle hClock, 
    NvMMClockClientType Type, 
    NvMMTimestamp RefTime); 

/** 
  * @brief Set the reference clock type. This defaults to audio. 
  *
  * This function sets the reference from which the media time will be 
  * derived
  * 
  * @param [in] hClock is the handle the applicable media clock 
  * @param [in] Type =         {audio, video, etc} and is audio by default
  * 
  */
NvError 
NvMMSetReference(
    NvMMClockHandle hClock, 
    NvMMClockClientType Type);

/**
  * @brief Get the current time: both media time and wallclock time 
  *
  * This function allows a block to query the current media time (e.g. 
  * to facilitate correct video frame delivery) 
  *
  * @param [in] hClock is the handle the applicable media clock 
  * @param [out] pTime is the time returned including:
  *    - MediaTime receives the current media time
  *    - WallclockTime receives the wall clock when the given 
  *      media time was sampled (this is to correct for latency in 
  *      execution of this function)
  */
NvError 
NvMMGetTime(
    NvMMClockHandle hClock, 
    NvMMTimestamp *pTime);

/** 
  * @brief Calculate the amount of wallclock time until the given 
  *    media time.
  * 
  * This function computes the amount of (wall clock) time until the 
  * given media time. For example, a client may use this function to 
  * help time the delivery of a frame of a particular timestamp; it 
  * may send the duration returned in call such as 
  *    ScheduleFlip(frame, TimeUntilFlip).
  * 
  * @param [in] hClock is the handle the applicable media clock 
  * @param [in] MediaTime is time the caller provides as input
  * @param [out] pWallTime is the time returned representing the amount 
  *    of time until the given MediaTime
  */
NvError 
NvMMGetTimeUntilMediaTime(
    NvMMClockHandle hClock, 
    NvS64 MediaTime,
    NvS64 *pWallTime);

/**
 * @brief Wait until the specified media time
 *
 * This function sleeps until a little before the specified media time.  It
 * will also give some feedback on the current quality of AV sync with the
 * eStatus value.
 *
 * This is meant to be called by the video renderer, when audio is the master
 * clock.  It will also wait for audio to start if NvMMSetWaitForAudio has been
 * called.
 *
 * @param [in] hClock is the handle to the media clock
 * @param [in] MediaTime is the time to wait until
 * @param [out] eStatus is a measure of the how late the frame was
 */
NvError
NvMMWaitUntilMediaTime(
    NvMMClockHandle hClock,
    NvS64 MediaTime,
    NvMMClockFrameStatus *eStatus);

/** 
  * @brief Set the rate at which the clock runs. Default is 1X. 
  *
  * This function allows an NvMM client to change the rate at which the 
  * media time runs. 
  * 
  * @param [in] hClock is the handle the applicable media clock 
  * @param [in] rate is the signed rate of playback (e.g. -2 = 2X reverse, 
  *    0 = paused, 1 = normal 1X, 16 = 16X playback) default is 1.
  *
  */
NvError 
NvMMSetClockRate(
    NvMMClockHandle hClock, 
    NvMMRate Rate );

/**
  * @brief Retrieve the current rate of the clock 
  *
  * @param [in] hClock is the handle the applicable media clock 
  * @param [out] pRate accepts the current clock rate.
  */
NvError 
NvMMGetClockRate(
    NvMMClockHandle hClock, 
    NvMMRate *pRate );

/**
  * @brief Set the start time of the given stream 
  *
  * This function allows each stream to provide the start time of the first renderable 
  * sample in its stream to the media clock.
  * 
  * @param [in] hClock is the handle the applicable media clock 
  * @param [in] Type =         {audio, video, etc} 
  * @param [in] Starttime is the timestamp of the first renderable sample 
  *    for the given type.
  */
NvError 
NvMMSetStartTime(
    NvMMClockHandle hClock, 
    NvMMClockClientType Type, 
    NvS64 StartTime
    );

/** 
  * @brief Start the clock. 
  *
  * This function commands the media clock to start. If a mask is provided 
  * then the media clock waits until it receives all start times from the
  * specified streams and uses the earliest start time provided by the 
  * streams (in this case the media clock ignores the starttime parameter 
  * from this call). Otherwise the media clock starts immediately using the 
  * provided start time.
  * 
  * @param [in] hClock is the handle the applicable media clock 
  * @param [in] Type = {audio, video, etc}
  * @param [in] Mask is the bitmask of types to wait on start times from 
  * @param [in] Startime is the starting time of the clock if not 
  *    waiting for clients to supply start times.
  */
NvError 
NvMMStartClock(
    NvMMClockHandle hClock,
    NvU32 Mask, 
    NvS64 StartTime);

/**
  * @brief Stop the clock. 
  * 
  * this function commands the media clock to stop.
  *
  * @param [in] hClock is the handle the applicable media clock 
  */
NvError 
NvMMStopClock(
    NvMMClockHandle hClock);

/*
 * @brief Wait for video to start.
 *
 * Video is considered started on the first call to NvMMWaitUntilMediaTime.
 *
  * @param [in] hClock is the handle to the applicable media clock 
 */
void
NvMMWaitForVideoToStart(
    NvMMClockHandle hClock);

/*
 * @brief Will wait for audio to start in NvMMWaitUntilMediaTime.
 *
 * Audio is considered started on the first non-0 reference timestamp update.
 *
 * @param [in] hClock is the handle to the applicable media clock 
 */
void 
NvMMSetWaitForAudio(
    NvMMClockHandle hClock);

/*
 * @brief Wait for video to start in NvMMWaitForVideoToStart, else that function
 * is a no-op.
 *
 * Video is considered started on the first call to NvMMWaitUntilMediaTime.
 *
 * @param [in] hClock is the handle to the applicable media clock 
 */
void
NvMMSetWaitForVideo(
    NvMMClockHandle hClock);

/*
 * @brief Stop waiting in NvMMWaitUntilMediaTime if blocked there.
 *
 * @param [in] hClock is the handle to the applicable media clock
 */
void
NvMMUnblockWaitUntilMediaTime(
    NvMMClockHandle hClock);

#endif

