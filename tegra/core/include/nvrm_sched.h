/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVSCHED_H
#define INCLUDED_NVSCHED_H

#include "nvrm_channel.h"

 
typedef struct NvSchedClientRec NvSchedClient;
typedef NvU32 NvSchedVirtualSyncPoint;
typedef void (*NvSchedCallbackType)(void *);
typedef void (*NvSchedPreFlushCallbackType)(NvData32 **, void *);

/**
 * Allocates and initializes an RM stream and a scheduler client.  Use
 * sc->stream to get to the allocated RM stream.
 *
 * @param hDevice device handle
 * @param hChannel channel the scheduler will run in
 * @param ModuleID hardware module this client will be using
 * @param sc scheduler client handle returned
 */
NvError NvSchedClientInit(NvRmDeviceHandle hDevice, NvRmChannelHandle hChannel,
                          NvRmModuleID ModuleID,
                          NvSchedClient *sc);

NvError NvSchedClientInitEx(NvRmDeviceHandle hDevice, NvRmChannelHandle hChannel,
                          NvRmModuleID ModuleID,
                          const NvRmStreamInitParams* pInitParams,
                          NvSchedClient *sc);

/**
 * Sets a priority for submits in the channel. Requires the caller to take
 * the returned sync point and wait base into use for all future streams.
 *
 * @param hChannel Handle to the channel for NvRmStream submits
 * @param Priority Priority of all further submits.
 *        Use values from NvRmStreamPriority.
 * @param SyncPointIndex Index of sync point to return
 * @param WaitBaseIndex Index of wait base to return
 * @param SyncPointID The returned sync point id
 * @param WaitBase The returned wait wait base
 */
NvError NvSchedSetPriority(
    NvSchedClient *sc,
    NvU32 Priority,
    NvU32 SyncPointIndex,
    NvU32 WaitBaseIndex,
    NvU32 *SyncPointID,
    NvU32 *WaitBase);
void NvSchedClientClose(NvSchedClient *sc);


/**
 * Registers a callback called during RM stream flush.
 *
 * See pSyncPointBaseCallback field in NvRmStreamRec structure for more
 * information.
 * @see NvRmStreamRec
 *
 * @param sc pointer to scheduler client object initialized with
 * NvSchedClientInit
 * @param pCallback callback function
 * @param data extra data passed to callback function
 * @returns NvError_NotSupported error if a callback has already been
 * registered
 */
NvError NvSchedClientRegisterCallback(NvSchedClient *sc,
                                      NvSchedCallbackType pCallback,
                                      void *data);

/** 
 * Registers a callback called just before RM stream flush. It can be used to
 * insert last words to command buffer being sent to hardware.
 * 
 * See pPreFlushCallback in NvRmStreamRec structure for more information.
 * @see NvRmStreamRec
 *
 * @param sc pointer to scheduler client object initialized with
 * NvSchedClientInit
 * @param pCallback callback function
 * @param ReservedWords number of words to reserve in command buffer
 * @param privData extra data passed to callback function
 *
 * Callback should use the following convetion with pointers to push words to
 * the stream:
 * void MyPreFlushCallback(NvData32 **ppCurrent, void *privData)
 * {
 *     NvData32 *nvCurrent = *ppCurrent;
 *
 *     NVRM_STREAM_PUSH_U(nvCurrent, value1);
 *     NVRM_STREAM_PUSH_I(nvCurrent, value2);
 *     NVRM_STREAM_PUSH_F(nvCurrent, value3);
 *
 *     *ppCurrent = nvCurrent;
 * }
 */
NvError NvSchedClientRegisterPreFlushCallback(
    NvSchedClient *sc,
    NvSchedPreFlushCallbackType pCallback,
    NvU32 reservedWords,
    void *privData);

/**
 * @returns NV_TRUE if a <= b < c using wrapping comparison; NV_FALSE otherwise
 */
static NV_INLINE NvBool NvSchedValueIsBetween(const NvU32 a, const NvU32 b,
                                              const NvU32 c)
{
    // If we imagine numbers between 0 and (1<<32)-1 placed along a circle,
    // then a-b is exactly the distance from b to a along the circle moving
    // clockwise.  This test checks that the distance between a and b is
    // strictly smaller than the distance between a and c.
    return b-a < c-a;
}

// Returns NV_TRUE if x >= y (mod 1<<32).
static NV_INLINE NvBool NvSchedValueWrappingComparison(const NvU32 x,
                                                       const NvU32 y)
{
    return NvSchedValueIsBetween(y, x, (1UL<<31UL)+y);
}




/**
 * Allocates a virtual sync point.
 *
 * @param sc scheduler client handle
 * @param pt virtual sync point to initialize
 * @param SyncPointID sync point ID as returned by NvRmChannelSyncPointAlloc
 * and NvRmChannelGetModuleSyncPoint
 * @returns some kind of error if a virtual sync point is initialized more
 * than once
 */
NvError NvSchedVirtualSyncPointInit(NvSchedClient *sc,
                                    NvSchedVirtualSyncPoint pt,
                                    NvU32 SyncPointID);

/**
 * Retrieve fence from most recent flush.
 * (Fence contains *REAL* SyncPointValue (NOT virtual value))
 *
 * @param sc scheduler client handle
 * @param pt virtual sync point to query
 * @param fence returned fence
 */
static NV_INLINE void NvSchedVirtualSyncPointGetLastFence(
                                            const NvSchedClient *sc,
                                            NvSchedVirtualSyncPoint pt,
                                            NvRmFence *fence);

/**
 * Requests RM to use the given virtual sync point for buffer tracking.  This
 * is done by assigning to pStream->SyncPointID the hardware sync point ID of
 * this virtual sync point.  Similar to pStream->SyncPointID, this function
 * can be called only immediately after NvRmStreamInit or NvRmStreamFlush
 * calls.
 *
 * @param sc scheduler client handle
 * @param pt virtual sync point for buffer tracking
 */
static NV_INLINE void NvSchedVirtualSyncPointStreamSet(NvSchedClient *sc,
                                              NvSchedVirtualSyncPoint pt);

/**
 * Increments the sync point.
 *
 * @param pt virtual sync point
 * @returns new client value of the sync point
 */
#define NV_SCHED_SYNC_POINT_INCREMENT(sc, p) \
    ((sc)->stream.SyncPointsUsed++, (sc)->pt[p].NextSyncPoint++)

// Following what's done in NvRmPrivAppendIncr.
#define NV_SCHED_SYNCPT_COND_OP_DONE      (0x1UL)
#define NV_SCHED_SYNCPT_COND_RD_DONE      (0x2UL)
//Why are these macros defined here? There are similar macros defined
//in nvrm_stream.h. Long-term the NV_SCHED* macros should be moved
//to an nvrm-private header file which can then include nvrm_stream.h
//to access the syncpt macros.

#define NV_SCHED_SYNC_POINT_OP_DONE_INCREMENT_COMMAND(pCurrent, sc, p) \
    do { \
        NVRM_STREAM_PUSH_U((pCurrent), NVRM_CH_OPCODE_NONINCR(0x0, 1)); \
        NVRM_STREAM_PUSH_U((pCurrent), \
           ((NV_SCHED_SYNCPT_COND_OP_DONE << 8) | \
           (NvU8)((sc)->pt[p].SyncPointID & 0xff))); \
     } while (0)

#define NV_SCHED_SYNC_POINT_RD_DONE_INCREMENT_COMMAND(pCurrent, sc, p) \
    do { \
        NVRM_STREAM_PUSH_U((pCurrent), NVRM_CH_OPCODE_NONINCR(0x0, 1)); \
        NVRM_STREAM_PUSH_U((pCurrent), \
           ((NV_SCHED_SYNCPT_COND_RD_DONE << 8) | \
           (NvU8)((sc)->pt[p].SyncPointID & 0xff))); \
     } while (0)

/**
 * Returns the current value of the virtual sync point.
 */
NvU32 NvSchedVirtualSyncPointReadLatest(NvSchedClient *sc,
                                        NvSchedVirtualSyncPoint pt);

/**
 * Returns the most recently read value of the virtual sync point.
 */
NvU32 NvSchedVirtualSyncPointReadCached(NvSchedClient *sc,
                                        NvSchedVirtualSyncPoint pt);

NvError NvSchedVirtualSyncPointCpuWaitTimeout(NvSchedClient *sc,
                                              NvSchedVirtualSyncPoint pt,
                                              NvU32 waitValue,
                                              NvU32 msec);

/**
 * Stalls the thread until the sync point reaches the given virtual sync point
 * value.
 */
static NV_INLINE void NvSchedVirtualSyncPointCpuWait(NvSchedClient *sc,
                                                     NvSchedVirtualSyncPoint pt,
                                                     NvU32 waitValue)
{
    (void)NvSchedVirtualSyncPointCpuWaitTimeout(sc, pt, waitValue,
                                                NV_WAIT_INFINITE);
}

/* Deprecated - use NvSchedPushHostWaitLast */
void NvSchedHostWaitLast(NvSchedClient *sc, NvSchedVirtualSyncPoint *points,
                         int count, NvBool opDoneFlag);
/* Deprecated - use NvSchedFlushAndCpuWait */
void NvSchedCpuWaitLast(NvSchedClient *sc,
                        NvSchedVirtualSyncPoint *points, int count);

/**
 * Push a sync point increment to stream.
 *
 * Uses 2 words of space in stream.
 *
 * @param pStream The stream
 * @param pCurrent The cursor
 * @param SyncPointID The id of sync point
 * @param The register number to use. If unsure, use 0x000
 * @param Cond The condition
 * @param True if this is a stream sync point
 * @return New cursor
 */
NvData32 *NvSchedPushIncr(NvSchedClient *sc,
    NvData32 *pCurrent,
    NvU32 Cond,
    NvU32 *VirtualFence);


/* Pushes a sync point increment, and a host wait for the increment. Can be
 * used only in ardrv.
 *
 * @param sc The nvsched client
 * @param pCurrent Reference to point in stream
 * @param Conditional for increment, true for OP_DONE, false for RD_DONE
 */
NvData32 *NvSchedPushHostWaitLast(NvSchedClient *sc,
    NvData32 *pCurrent,
    NvBool opDoneFlag);
/* Pushes a sync point increment, and a host wait for the increment. Can be
 * used only in ardrv.
 *
 * @param sc The nvsched client
 */
void NvSchedFlushAndCpuWait(NvSchedClient *sc);

// TODO: this doesn't seem equlivalent to the current implementation of
// NvSchedVirtualSyncPointCpuWait.

/**
 * @returns 1 if waitValue is newer than the current value of the virtual sync
 * point (equivalently, if NvSchedVirtualSyncPointCpuWait would stall).
 */
NvBool NvSchedVirtualSyncPointCpuWouldWait(NvSchedClient *sc,
                                           NvSchedVirtualSyncPoint pt,
                                           NvU32 waitValue);

/**
 * @returns 1 if waitValue is newer than most recently read value of the
 * virtual sync point.
 */
NvBool NvSchedVirtualSyncPointCpuWouldWaitCached(NvSchedClient *sc,
                                                 NvSchedVirtualSyncPoint pt,
                                                 NvU32 waitValue);




// Scheduler private

enum { NVSCHED_VIRTUAL_SYNC_POINT_NUMBER = 2 };
enum { NVSCHED_MAPPING_TABLE_SIZE = 16 };

struct NvSchedSyncPointMapping {
    NvU32 hw;
    NvU32 client;
    NvU32 clientCount;
};

struct NvSchedVirtualSyncPointRec {
    NvU32 SyncPointID;
    NvU32 WaitBase;

    // The next virtual sync point value.
    NvU32 NextSyncPoint;

    // Last submitted hardware sync point value.
    NvU32 LastHWSyncPointSubmitted;

    // Mapping between hardware and client sync point values.
    //
    // To find a hardware sync point value given a client value c, find an
    // index i such that (using wrapping comparison):
    //   SyncPointMapping[i].client <= c < SyncPointMapping[i].client +
    //                                     SyncPointMapping[i].size
    // Then the hardware sync point value is equal to:
    //   h = c + SyncPointMapping[i].hw - SyncPointMapping[i].client
    struct NvSchedSyncPointMapping SyncPointMapping[
                                        NVSCHED_MAPPING_TABLE_SIZE];

    // Number of entries in SyncPointMapping.
    NvU32 MappingSize;

    // Last updated entry in SyncPointMapping.
    NvU32 LastMappingEntry;
};

struct NvSchedClientRec {
    // Must be the first entry, used in NvSchedSyncPointBaseCallback and
    // NvSchedPreFlushCallback
    NvRmStream stream;

    // The rest is private.

    NvRmDeviceHandle hDevice;
    NvOsSemaphoreHandle hSema;

    // Bitmask of initialized virtual sync points.
    NvU32 SyncPointMask;

    struct NvSchedVirtualSyncPointRec pt[NVSCHED_VIRTUAL_SYNC_POINT_NUMBER];

    NvSchedCallbackType pCallbackFunction;
    void *CallbackData;

    NvSchedPreFlushCallbackType pPreFlushCallbackFunction;
    void *PreFlushCallbackPrivData;

    NvSchedCallbackType pSyncptIncr;
    void *SyncptIncrData;

    //private data member available for each chip to 
    //do whatever you want
    void *pClientPriv;
};

static NV_INLINE void NvSchedVirtualSyncPointGetLastFence(
                                            const NvSchedClient *sc,
                                            NvSchedVirtualSyncPoint pt,
                                            NvRmFence *fence)
{
    fence->SyncPointID = sc->pt[pt].SyncPointID;
    fence->Value       = sc->pt[pt].LastHWSyncPointSubmitted;
}

static NV_INLINE void NvSchedVirtualSyncPointStreamSet(NvSchedClient *sc,
                                               NvSchedVirtualSyncPoint pt)
{
    sc->stream.SyncPointID = sc->pt[pt].SyncPointID;
}

NvU32 NvSchedHwToVirtualDebug(const NvSchedClient *sc,
                              NvSchedVirtualSyncPoint pt, NvU32 hwValue);
NvU32 NvSchedVirtualToHwDebug(const NvSchedClient *sc,
                              NvSchedVirtualSyncPoint pt, NvU32 clientValue);

#endif // INCLUDED_NVSCHED_H
