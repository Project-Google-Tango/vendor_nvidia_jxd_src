/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvrm_sched.h"
#include "nvrm_sched_private.h"
#include "nvassert.h"
#include "t12x/class_ids.h"
#include "t12x/arhost1x_uclass.h"

// RM can insert sync point increments anywhere in our stream.  Since we do
// not know where they are inserted, we must assume the worst: it inserted
// them right before our commands.  This means we need to adjust our sync
// point calculations by the difference
// between the number of sync points submitted by RM (from the callback
// argument) and the number of virtual sync point increments.
//
// RM does guarantee that one sync point increment is always added at the end
// of the buffer.  LAST_RM_SYNCPT_INCR_COUNT is this one.
#define LAST_RM_SYNCPT_INCR_COUNT 1


static void NvSchedSyncPointBaseCallback(struct NvRmStreamRec *pStream,
                                         NvU32 SyncPointBase,
                                         NvU32 SyncPointsUsed);

static void NvSchedPreFlushCallback(struct NvRmStreamRec *pStream);

// Scheduler client stuff.
NvError NvSchedClientInit(NvRmDeviceHandle hDevice, NvRmChannelHandle hChannel,
                          NvRmModuleID ModuleID,
                          NvSchedClient *sc)
{
    return NvSchedClientInitEx(hDevice, hChannel, ModuleID, NULL, sc);
}


NvError NvSchedClientInitEx(NvRmDeviceHandle hDevice, NvRmChannelHandle hChannel,
                          NvRmModuleID ModuleID,
                          const NvRmStreamInitParams* pInitParams,
                          NvSchedClient *sc)
{
    NvError ret;

    NvOsMemset(sc, 0, sizeof(*sc));
    ret = NvRmStreamInitEx(hDevice, hChannel, pInitParams, &sc->stream);
    if (ret != NvSuccess)
        goto failed;
    sc->stream.LastEngineUsed = ModuleID;

    /* Allocate context to enable AOS/cmodel context switching */
    ret = NvRmContextAlloc(hDevice, ModuleID, &sc->stream.hContext);

    ret = NvOsSemaphoreCreate(&sc->hSema, 0);
    if (ret != NvSuccess)
        goto failed;

    sc->hDevice = hDevice;
    sc->stream.pSyncPointBaseCallback = NvSchedSyncPointBaseCallback;

    sc->pCallbackFunction = NULL;
    sc->CallbackData = NULL;

    sc->pPreFlushCallbackFunction = NULL;
    sc->PreFlushCallbackPrivData = NULL;

    ret = NvSchedClientPrivInit(hDevice, hChannel, ModuleID, sc);
    if (ret != NvSuccess)
        goto failed;

    return ret;

failed:
    NvSchedClientClose(sc);
    return ret;
}

NvError NvSchedClientRegisterCallback(NvSchedClient *sc,
                                      NvSchedCallbackType pCallback, void *data)
{
    if (sc->pCallbackFunction)
        return NvError_NotSupported;

    sc->pCallbackFunction = pCallback;
    sc->CallbackData = data;

    return NvSuccess;
}

NvError NvSchedClientRegisterPreFlushCallback(
    NvSchedClient *sc,
    NvSchedPreFlushCallbackType pCallback,
    NvU32 reservedWords,
    void *privData)
{
    if (sc->pPreFlushCallbackFunction)
        return NvError_NotSupported;

    sc->stream.pPreFlushData = NvOsAlloc(reservedWords * sizeof(NvU32));
    if (!sc->stream.pPreFlushData)
        return NvError_InsufficientMemory;

    sc->stream.pPreFlushCallback  = NvSchedPreFlushCallback;
    sc->stream.PreFlushWords      = reservedWords;
    sc->pPreFlushCallbackFunction = pCallback;
    sc->PreFlushCallbackPrivData  = privData;

    return NvSuccess;
}

NvError
NvSchedSetPriority(NvSchedClient *sc, NvU32 Priority,
        NvU32 SyncPointIndex, NvU32 WaitBaseIndex,
        NvU32 *SyncPointID, NvU32 *WaitBase)
{
    NvError err;
    NvU32 WaitBaseShadow;

    err = NvRmStreamSetPriorityEx(&sc->stream, Priority, SyncPointIndex,
        WaitBaseIndex, SyncPointID, &WaitBaseShadow);
    sc->pClientPriv = (void *)WaitBaseShadow;

    if (WaitBase)
        *WaitBase = WaitBaseShadow;

    return err;
}
void NvSchedClientClose(NvSchedClient *sc)
{
    if (sc == NULL)
        return;

    NvSchedClientPrivClose(sc);
    NvOsSemaphoreDestroy(sc->hSema);
    NvRmContextFree(sc->stream.hContext);
#if NV_DEBUG
    {
        struct NvSchedVirtualSyncPointRec *ptRec = &sc->pt[0];
        struct NvSchedSyncPointMapping *m =
            &ptRec->SyncPointMapping[ptRec->LastMappingEntry];
        // Here is a check that RM did not lie about the number of sync point
        // increments -- one extra is added per callback.
        // If check fails, we most likely had failed submissions.
        if(m->client + m->clientCount != ptRec->NextSyncPoint) {
            NvOsDebugPrintf("NvRm scheduler is broken. Failed submits?\n");
        }
    }
#endif
    if (sc->stream.pPreFlushData)
        NvOsFree(sc->stream.pPreFlushData);
    NvRmStreamFree(&sc->stream);
}


// Virtual sync points stuff.


//
// Notes about ptRec->SyncPointMapping
// -----------------------------------
// Each piece represents one flush.  (If a flush has no increments it also has
// no piece in the mapping.)
//
// LastMappingEntry points to the most recent piece that was flushed.
// MappingSize indicates how many flushed entries may not be expired yet.
// If MappingSize is 0 then all flushed increments are expired.
//
// LastMappingEntry always points to a valid entry, even if MappingSize is 0.
//
//
//  Example: Imagine this sequence occurs:
//
//   - HW syncpoint register (min in kernel) starts at 4
//   - HW syncpoint future value (max in kernel) starts at 4
//   - we submit chunk #1 with 10 increments (+1 from nvrm)
//   - we submit chunk #2 with 15 increments (+1 from nvrm)
//   - another process submits a chunk with 20 increments (+1 from nvrm)
//   - we submit chunk #3 with 5 increments (+1 from nvrm)
//
//   Then the mapping will look like:
//
//       ptRec->SyncPointMapping[0]
//           hw          = 5   // 4 + 1
//           client      = 0
//           clientCount = 10
//
//       ptRec->SyncPointMapping[1]
//           hw          = 16  // 5 + 10+1
//           client      = 10
//           clientCount = 15
//
//       ptRec->SyncPointMapping[2]  <--- ptRec->LastMappingEntry
//           hw          = 53  // 16 + 15+1 + 20+1
//           client      = 25
//           clientCount = 5
//
//      ptRec->MappingSize = 3
//      ptRec->NextSyncPoint = 30
//
//  Notes:
//    - ptRec->NextSyncPoint MINUS ONE is the virtual future value.
//    - Mapping[0] maps virtual  0...9  to HW  5...14 (for chunk #1)
//    - Mapping[1] maps virtual 10...24 to HW 16...30 (for chunk #2)
//    - Mapping[2] maps virtual 25...29 to HW 53...57 (for chunk #3)
//    - HW values <5, 14, 31...52, and 58 have no corresponding virtual value
//
//
//  Now lets say that 3 new increments are added to the pushbuffer (but not
//  flushed yet).  The mapping table is the same but now:
//
//      ptRec->NextSyncPoint = 33
//
//  Notes:
//    - Mapping2's client+clientCount (25+5=30) tracks the next virtual value
//        that has not yet been submitted.
//    - ptRec->NextSyncPoint = 33 tracks the next virtual value that has not
//        yet been added to a pushbuffer.
//
//  Now suppose we read the min and max values from the kernel and call
//  NvSchedVirtualSyncPointUpdate().  If the values read are
//      min = 18
//      max = 70
//  then we can eliminate all mappings corresponding to HW values 18 and
//  earlier.  The mapping table is updated as follows:
//
//       ptRec->SyncPointMapping[1]
//           hw          = 19
//           client      = 13
//           clientCount = 12
//
//       ptRec->SyncPointMapping[2]  <--- ptRec->LastMappingEntry
//           hw          = 53
//           client      = 25
//           clientCount = 5
//
//      ptRec->MappingSize = 3
//      ptRec->NextSyncPoint = 33
//
//  Notes:
//    - Mapping[0] is no longer relevant
//    - Mapping[1] is updated to only consider unexpired values
//    - Mapping[2] is unchanged
//    - max = 70 implies other processes have inserted 12 increments since our
//          chunk #3 submit.  (70-58 = 12)
//
//
//  Now suppose we read the min and max values again and get
//      min = 70
//      max = 70
//  then NvSchedVirtualSyncPointUpdate() will update the mapping as:
//
//       ptRec->SyncPointMapping[2]  <--- ptRec->LastMappingEntry
//           hw          = 53
//           client      = 25
//           clientCount = 5
//
//      ptRec->MappingSize = 0
//      ptRec->NextSyncPoint = 33
//
//  Notes:
//    - Mapping[1] is no longer relevant
//    - Mapping[2] is no longer relevant for mapping virtual values, but is
//        still used to keep track of the next virtual value that has not yet
//        been submitted: 30 = 25+5 = client+clientCount.  Actually this can
//        also be calculated as
//        (ptRec->NextSyncPoint - pStream->SyncPointsUsed) = 33-3 = 30
//


// TODO:
//
//   - make the NvSchedSyncPointBaseCallback() callback take an extra argument:
//       the min value of the syncpoint.  This will allow us to cheeply update
//       the virtual syncpoint when a submit occurs.
//   - make a version of NvSchedVirtualSyncPointCpuWaitTimeout() which returns
//       the max value as well as the min value.  This will allow us to cheeply
//       update the virtual syncpoint when a wait occurs.  Also add another
//       return value that indicates whether a wait actually occurred or not
//       (for NvGlAlBufferObjectCopyData -- see below)
//   - can LastHWSyncPointSubmitted be removed?

// TODO
// ----
// These TODO items should help perf and allow some of the below functions to
// be removed.
//
//
// TODO: Remove NvSchedVirtualSyncPointReadLatest()
// this function is ONLY used by
// NvGlAr20Syncpt3DVmemGarbageCollect().   Once we have all waits calling
// NvSchedVirtualSyncPointUpdate(), the call can be changed to
// NvSchedVirtualSyncPointReadCached() and
// NvSchedVirtualSyncPointReadLatest() can be removed.
//
// TODO: Fix NvGlAlBufferObjectCopyData
// Currently this function calls NvGlAr20Syncpt3DCheck() which does a
// NvSchedVirtualSyncPointCpuWouldWait() (kernel call) and a
// NvSchedVirtualSyncPointReadCached(), and then immediately calls
// NvGlAr20Syncpt3DWait() which calls NvSchedVirtualSyncPointCpuWait().  It
// does this so it can determine whether it needed to wait or not.  By adding
// an additional return param on the wait ioctl, the kernel could indicate
// whether a wait was actually needed (i.e. if the process was put to sleep, or
// if it was able to return immediately).  This return value can be propogated
// back to NvGlAlBufferObjectCopyData() so that the NvGlAr20Syncpt3DCheck()
// call can be entirely avoided.
//    Note: this is the ONLY place NvGlAr20Syncpt3DCheck is called, so this fix
// would allow us to eliminate the NvGlAr20Syncpt3DCheck() function.
//    Note: NvGlAr20Syncpt3DCheck() is one of only 2 places
// NvSchedVirtualSyncPointCpuWouldWait() is called from.
//
// TODO: Investigate NvGlAr20Syncpt3DVmemDelete()
// This function frees memory, but only after a syncpoint tells us that is
// safe.  It calls NvSchedVirtualSyncPointCpuWouldWait() (kernel call).  I
// think that is unnecessary.  It could instead call
// NvSchedVirtualSyncPointCpuWouldWaitCached().  This should be perf tested to
// be sure it is not a problem.
//    Note: NvGlAr20Syncpt3DVmemDelete() is the other of only 2 places that
// call NvSchedVirtualSyncPointCpuWouldWait().
//
// TODO: Eliminate NvSchedVirtualSyncPointCpuWouldWait()
// If the above 2 TODO items are fixed then
// NvSchedVirtualSyncPointCpuWouldWait() is no longer needed and can be
// removed.

//===========================================================================
// NvSchedReadMinMax() - read min and max syncpoint value from kernel
//===========================================================================
static NvError NvSchedReadMinMax(
        NvRmDeviceHandle hDevice,
        NvU32 hwSyncPointID,
        NvU32 *returned_min,
        NvU32 *returned_max)
{
    *returned_min = NvRmChannelSyncPointRead(hDevice, hwSyncPointID);
    return NvRmChannelSyncPointReadMax(hDevice, hwSyncPointID, returned_max);
}


//===========================================================================
// NvSchedValueIsExpired() - check if a syncpoint value is expired
//===========================================================================
// basic syncpoint comparison check.  True if thresh is expired based on
// current (min) and future (max) syncpoint values.
//
// False (not-expired) if (min < thresh <= max) (using modulo 2^32 math)
//
// min is the current value of the syncpoint
// max is the future value of the syncpoint
//
// min==max     : TRUE  (expired (for any thresh)
// thresh==min  : TRUE  (expired)
// thresh==max  : FALSE (pending)
//
static NV_INLINE NvU32 NvSchedValueIsExpired(
    NvU32 min,
    NvU32 thresh,
    NvU32 max)
{
    return (max - thresh >= min - thresh);
}

//===========================================================================
// NvSchedMappingPiece() - get mapping piece (Last - idx)
//===========================================================================
static NV_INLINE struct NvSchedSyncPointMapping *NvSchedMappingPiece(
        const struct NvSchedVirtualSyncPointRec *ptRec,
        NvU32 idx)
{
    const struct NvSchedSyncPointMapping *piece;
    NV_ASSERT(NV_IS_POWER_OF_2(NVSCHED_MAPPING_TABLE_SIZE));

    idx = (ptRec->LastMappingEntry - idx) & (NVSCHED_MAPPING_TABLE_SIZE-1);
    piece = &ptRec->SyncPointMapping[idx];

    // const cast so we can use same function for const and non-const
    return (struct NvSchedSyncPointMapping*)piece;
}


//===========================================================================
// NvSchedVirtualSyncPointUpdate() - update mapping for virtual syncpoint
//===========================================================================
// This function should be called periodically with a recent snapshot of the
// kernel's min and max value of the syncpoint.
// If useful, this function could be made non-static so others can call it when
// they know min & max.
static void NvSchedVirtualSyncPointUpdate(
        NvSchedClient *sc,
        NvSchedVirtualSyncPoint pt,
        NvU32 min,   // snapshot of min val for this syncpoint (from kernel)
        NvU32 max)   // snapshot of max val taken at SAME TIME as min snapshot
{
    struct NvSchedVirtualSyncPointRec *ptRec = &sc->pt[pt];
    NvU32 i;

    ptRec->LastHWSyncPointSubmitted = max;

    for (i=0; i!=ptRec->MappingSize; i++) {
        struct NvSchedSyncPointMapping *piece = NvSchedMappingPiece(ptRec, i);

        // is the end of the piece expired?  If so the entire piece and all
        // preceding pieces are expired.
        //
        // IMPORTANT: we need to check the *end* of the piece first (from most
        // recent piece to least recent).  It is possible that the start of the
        // piece is "not expired" and the end IS expired -- that indicates that
        // the current value has wrapped all the way around and the entire
        // piece is now very old and entirely expired.
        if (NvSchedValueIsExpired(min, piece->hw + piece->clientCount-1, max)) {
            // yes - erase this piece and all earlier pieces (they are all
            // expired).  NOTE: This may leave MappingSize==0.
            ptRec->MappingSize = i;
            break;
        }

        // is start of piece expired?
        if (NvSchedValueIsExpired(min, piece->hw, max)) {
            NvU32 off = min - piece->hw + 1;

            // this will always be a small(ish) positive number
            NV_ASSERT(off > 0 && off < piece->clientCount);

            // chop of the first part of the piece
            piece->hw           += off;  // ==min
            piece->client       += off;
            piece->clientCount  -= off;

            // remaining pieces are all expired
            ptRec->MappingSize = i+1;
            break;
        }
    }
}

//===========================================================================
// NvSchedVirtualSyncPointUpdateFromKernel() read from kernel and update mapping
//===========================================================================
static NV_INLINE void NvSchedVirtualSyncPointUpdateFromKernel(
        NvSchedClient *sc,
        NvSchedVirtualSyncPoint pt)
{
    NvU32 min, max;
    if (!NvSchedReadMinMax(sc->hDevice, sc->pt[pt].SyncPointID, &min, &max))
        NvSchedVirtualSyncPointUpdate(sc, pt, min, max);
}


//===========================================================================
// NvSchedSyncPointBaseCallback() - called just after every submission
//===========================================================================
// SyncPointBase:  future value of syncpoint just before submit begins.
// SyncPointsUsed: number of increments in the submission (including the one
//                 added by nvrm).
static void NvSchedSyncPointBaseCallback(struct NvRmStreamRec *pStream,
                                         NvU32 SyncPointBase,
                                         NvU32 SyncPointsUsed)
{
    NvSchedClient *sc = (NvSchedClient *)pStream;
    struct NvSchedVirtualSyncPointRec *ptRec = NULL;
    struct NvSchedSyncPointMapping *prev_piece;
    struct NvSchedSyncPointMapping *new_piece;
    NvU32 min, max;

    if( pStream->ErrorFlag != NvSuccess )
    {
        // We still need to call an old call back function,
        // otherwise stream error might not be handled.
        if (sc->pCallbackFunction) {
            sc->pCallbackFunction(sc->CallbackData);
        }
        return;
    }

    if( !SyncPointsUsed )
    {
        return;
    }

    // bail if there are no virtual syncpoints for this stream
    if (!sc->SyncPointMask)
        return;

    for (ptRec=sc->pt;; ptRec++) {
        // bail if there is no virtual syncpoint for pStream->SyncPointID
        if (ptRec == &sc->pt[NV_ARRAY_SIZE(sc->pt)])
            return;
        if (ptRec->SyncPointID == pStream->SyncPointID)
            break;
    }

    max = SyncPointBase + SyncPointsUsed;
    min = NvRmChannelSyncPointRead(sc->hDevice, ptRec->SyncPointID);
    NvSchedVirtualSyncPointUpdate(sc, ptRec - sc->pt, min, max);

    // nothing to do if no virtual increments were done
    if (SyncPointsUsed - LAST_RM_SYNCPT_INCR_COUNT == 0)
        return;

    prev_piece = &ptRec->SyncPointMapping[ptRec->LastMappingEntry];

    // Add a new entry.
    ptRec->LastMappingEntry = (ptRec->LastMappingEntry+1) %
        NVSCHED_MAPPING_TABLE_SIZE;
    new_piece = &ptRec->SyncPointMapping[ptRec->LastMappingEntry];

    // if mapping table is full, we need to wait until all syncpoints in this
    // old piece have expired so we can reuse it as the new piece.
    if (ptRec->MappingSize == NVSCHED_MAPPING_TABLE_SIZE) {
        NvError err = NvRmChannelSyncPointWaitexTimeout(
                            sc->hDevice,
                            ptRec->SyncPointID,
                            new_piece->hw + new_piece->clientCount - 1,
                            sc->hSema,
                            NV_WAIT_INFINITE,
                            NULL);
	if (err)
            NV_ASSERT(!"Sync point wait failed");
    } else {
        ptRec->MappingSize++;
    }

    new_piece->hw = SyncPointBase + 1;
    new_piece->client = prev_piece->client + prev_piece->clientCount;
    new_piece->clientCount = SyncPointsUsed - LAST_RM_SYNCPT_INCR_COUNT;

    if (sc->pCallbackFunction)
        sc->pCallbackFunction(sc->CallbackData);
}

//===========================================================================
// NvSchedPreFlushCallback() - called just before submit
//===========================================================================
static void NvSchedPreFlushCallback(struct NvRmStreamRec *pStream)
{
    NvSchedClient *sc = (NvSchedClient *)pStream;

    if (sc->pPreFlushCallbackFunction)
        sc->pPreFlushCallbackFunction(&pStream->pPreFlushData,
                                      sc->PreFlushCallbackPrivData);
}

//===========================================================================
// NvSchedVirtualSyncPointCachedMin() - virtual min
//===========================================================================
// Returns the largest known-expired virtual syncpoint value.
// This is the virtual equivalent of the current (min) value.
static NV_INLINE NvU32 NvSchedVirtualSyncPointCachedMin(
        const NvSchedClient *sc,
        NvSchedVirtualSyncPoint pt)
{
    const struct NvSchedVirtualSyncPointRec *ptRec = &sc->pt[pt];
    const struct NvSchedSyncPointMapping *piece;

    // MappingSize==0 means all flushed values have expired.
    if (ptRec->MappingSize == 0) {
        piece = &ptRec->SyncPointMapping[ptRec->LastMappingEntry];
        return piece->client + piece->clientCount - 1;
    }

    piece = NvSchedMappingPiece(ptRec, ptRec->MappingSize-1);
    return piece->client - 1;
}

//===========================================================================
// NvSchedVirtualSyncPointInit() - initialize a virtual syncpoint
//===========================================================================
NvError NvSchedVirtualSyncPointInit(NvSchedClient *sc,
                                    NvSchedVirtualSyncPoint pt,
                                    NvU32 SyncPointID)
{
    struct NvSchedVirtualSyncPointRec *ptRec = &sc->pt[pt];

    // Make sure NvU32 is enough for a mask of used virtual sync points.
    NV_ASSERT(NVSCHED_VIRTUAL_SYNC_POINT_NUMBER < sizeof(NvU32)*8);

    NV_ASSERT(!(sc->SyncPointMask & (1 << pt)));

    sc->SyncPointMask |= 1 << pt;

    NvOsMemset(ptRec, 0, sizeof(*ptRec));
    // SyncPointMapping has zero elements, new elements will come in at flush
    // time.
    ptRec->LastMappingEntry = NVSCHED_MAPPING_TABLE_SIZE - 1;
    ptRec->SyncPointID = SyncPointID;

    return NvSuccess;
}

//===========================================================================
// NvSchedVirtualSyncPointReadLatest() - read and return virtual "current"
//===========================================================================
NvU32 NvSchedVirtualSyncPointReadLatest(NvSchedClient *sc,
                                        NvSchedVirtualSyncPoint pt)
{
    // If all flushed values are known to be expired then don't bother reading
    // the latest value.
    if (sc->pt[pt].MappingSize != 0) {
        NvSchedVirtualSyncPointUpdateFromKernel(sc, pt);
    }

    return NvSchedVirtualSyncPointReadCached(sc,pt);
}

//===========================================================================
// NvSchedVirtualSyncPointReadCached() - return the cached virtual "current"
//===========================================================================
NvU32 NvSchedVirtualSyncPointReadCached(NvSchedClient *sc,
                                        NvSchedVirtualSyncPoint pt)
{
    return NvSchedVirtualSyncPointCachedMin(sc, pt);
}

//===========================================================================
// NvSchedVirtualSyncPointCpuWouldWaitCached() - true if might not be expired
//===========================================================================
// If false, the syncpoint is definitely expired.
// If true, the syncpoint may or may not be expired.
NvBool NvSchedVirtualSyncPointCpuWouldWaitCached(NvSchedClient *sc,
                                                 NvSchedVirtualSyncPoint pt,
                                                 NvU32 virtualWaitThresh)
{
    NvU32 vmin = NvSchedVirtualSyncPointCachedMin(sc,pt);
    NvU32 vmax = sc->pt[pt].NextSyncPoint - 1;
    return !NvSchedValueIsExpired(vmin, virtualWaitThresh, vmax);
}

//===========================================================================
// NvSchedVirtualSyncPointCpuWouldWait() - true if not expired
//===========================================================================
// If false, the syncpoint is expired.
// If true, the syncpint is not expired.
NvBool NvSchedVirtualSyncPointCpuWouldWait(NvSchedClient *sc,
                                           NvSchedVirtualSyncPoint pt,
                                           NvU32 virtualWaitThresh)
{
    if (!NvSchedVirtualSyncPointCpuWouldWaitCached(sc,pt,virtualWaitThresh))
        return NV_FALSE;
    NvSchedVirtualSyncPointUpdateFromKernel(sc, pt);
    return NvSchedVirtualSyncPointCpuWouldWaitCached(sc, pt, virtualWaitThresh);
}

//===========================================================================
// NvSchedVirtualToHw() - map virtual value to HW value - INTERNAL USE ONLY
//===========================================================================
// This function assumes virtualValue is unexpired according to current
// snapshot of min max (from last time NvSchedVirtualSyncPointUpdate was
// called).
static NvU32 NvSchedVirtualToHw(
        const NvSchedClient *sc,
        NvSchedVirtualSyncPoint pt,
        NvU32 virtualValue)
{
    const struct NvSchedVirtualSyncPointRec *ptRec = &sc->pt[pt];
    NvU32 i;

    for (i=0; i!=ptRec->MappingSize; i++) {
        struct NvSchedSyncPointMapping *piece = NvSchedMappingPiece(ptRec, i);

        if (!NvSchedValueIsExpired(piece->client-1,
                                   virtualValue,
                                   piece->client + piece->clientCount - 1)) {
            return piece->hw + virtualValue - piece->client;
        }
    }

    NV_ASSERT(!"Virtual sync point already expired");
    return 0;
}

//===========================================================================
// NvSchedVirtualSyncPointCpuWaitTimeout() - wait for value to expire
//===========================================================================
NvError NvSchedVirtualSyncPointCpuWaitTimeout(NvSchedClient *sc,
                                              NvSchedVirtualSyncPoint pt,
                                              NvU32 virtualWaitThresh,
                                              NvU32 msec)
{
    struct NvSchedVirtualSyncPointRec *ptRec = &sc->pt[pt];
    const struct NvSchedSyncPointMapping *piece =
        &ptRec->SyncPointMapping[ptRec->LastMappingEntry];
    NvU32 Threshold;
    NvU32 min;
    NvError ret = NvSuccess;

    // Do not wait if stream error state is set.
    if(sc->stream.ErrorFlag != NvSuccess) {
        return sc->stream.ErrorFlag;
    }

    // If the sync point increment we're interested in has not yet been
    // submitted to the hardware, flush it out first.
    if (!NvSchedValueIsExpired(piece->client + piece->clientCount - 1,
                                   virtualWaitThresh,
                                   ptRec->NextSyncPoint - 1)) {
        NvRmStreamFlush(&sc->stream, 0);
    }

    // Do not wait if stream error state is set.
    if(sc->stream.ErrorFlag != NvSuccess) {
        return sc->stream.ErrorFlag;
    }

    // If syncpoint is known to already be expired we can ignore it
    if (!NvSchedVirtualSyncPointCpuWouldWaitCached(sc,pt,virtualWaitThresh)) {
        return NvSuccess;
    }

    Threshold = NvSchedVirtualToHw(sc, pt, virtualWaitThresh);
    ret = NvRmChannelSyncPointWaitexTimeout(sc->hDevice, ptRec->SyncPointID,
                              Threshold, sc->hSema, msec, &min);
    NvSchedVirtualSyncPointUpdateFromKernel(sc, pt);

    return ret;
}

NvData32 *NvSchedPushIncr(NvSchedClient *sc,
    NvData32 *pCurrent,
    NvU32 Cond,
    NvU32 *VirtualFence)
{
    NVRM_STREAM_PUSH_INCR(&sc->stream, pCurrent,
        sc->pt[0].SyncPointID, 0x0, Cond, NV_TRUE);

    if (VirtualFence)
        *VirtualFence = sc->pt[0].NextSyncPoint;

    sc->pt[0].NextSyncPoint++;

    return pCurrent;
}

NvData32 *NvSchedPushHostWaitLast(NvSchedClient *sc, NvData32 *pCurrent,
    NvBool opDoneFlag)
{
    const struct NvSchedVirtualSyncPointRec *ptRec = &sc->pt[0];

    NvU32 cond =  opDoneFlag ?
        NV_CLASS_HOST_INCR_SYNCPT_0_COND_OP_DONE :
        NV_CLASS_HOST_INCR_SYNCPT_0_COND_RD_DONE;

    /* Add SETCLASS here now until graphics has been converted to do it */
    NVRM_STREAM_PUSH_SETCLASS(&sc->stream, pCurrent,
        NvRmModuleID_3D, NV_GRAPHICS_3D_CLASS_ID);
    NVRM_STREAM_PUSH_WAIT_LAST(&sc->stream, pCurrent,
        ptRec->SyncPointID, (NvU32)sc->pClientPriv,
        0, cond);
    sc->pt[0].NextSyncPoint++;

    if (sc->pSyncptIncr)
        sc->pSyncptIncr(sc->SyncptIncrData);

    return pCurrent;
}

void NvSchedHostWaitLast(NvSchedClient *sc, NvSchedVirtualSyncPoint *points,
                         int count, NvBool opDoneFlag)
{
    NvData32 *pCurrent;

    NV_ASSERT(count == 1 && *points == 0);
    NVRM_STREAM_BEGIN(&sc->stream, pCurrent, 7);
    pCurrent = NvSchedPushHostWaitLast(sc, pCurrent, opDoneFlag);
    NVRM_STREAM_END(&sc->stream, pCurrent);
}

void NvSchedFlushAndCpuWait(NvSchedClient *sc)
{
    NvRmFence fence;
    NvRmStreamFlush(&sc->stream, &fence);
    NvRmFenceWait(sc->hDevice, &fence, NV_WAIT_INFINITE);
}

void NvSchedCpuWaitLast(NvSchedClient *sc,
                        NvSchedVirtualSyncPoint *points, int count)
{
    NV_ASSERT(count == 1 && *points == 0);
    NvSchedFlushAndCpuWait(sc);
}

NvError NvSchedClientPrivInit(NvRmDeviceHandle hDevice, NvRmChannelHandle hChannel,
                          NvRmModuleID ModuleID,
                          NvSchedClient *sc)
{
    NV_ASSERT(sc);//sc should not be null
    //only 3d module's waitbase ID is needed
    sc->pClientPriv = (void *)NvRmChannelGetModuleWaitBase( hChannel, NvRmModuleID_3D, 0 );
    return NvSuccess;
}

void NvSchedClientPrivClose(NvSchedClient *sc)
{
    return;
}
