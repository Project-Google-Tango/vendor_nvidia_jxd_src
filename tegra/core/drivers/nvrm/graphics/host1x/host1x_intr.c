/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvrm_channel.h"
#include "nvrm_drf.h"
#include "nvrm_interrupt.h"
#include "nvos.h"
#include "nvassert.h"
#include "arhost1x.h"
#include "arhost1x_sync.h"
#include "project_arhost1x_sw_defs.h"
#include "host1x_channel.h"
#include "nvrm_graphics_private.h"

// This define controls the timeout of direct access to module  behing host.
// There are modules behind host between address range 0x5400:0000 to
// 0x57ff:ffff.
// Any access to these registers will be interepretted by the host and sends
// the read/write bus cycles to the appropriate modules by decoding the
// address. There is a possibility that the mdoules might not repsond or
// address is just invalid.
//
// HOST1X_SYNC_IP_BUSY_TIMEOUT_0 register controls the access timeout. A value
// of 0 means infinite timeout. Maximum value for the timeout is 0xffff. By
// default it is set to 0.
#define NVRM_DIRECT_ACCESS_TIMEOUT    0x0

typedef struct NvRmWaitListRec
{
    struct NvRmWaitListRec *next;
    NvU32 thresh;
    NvRmWaitListAction action;
    union
    {
        NvOsSemaphoreHandle sem;
        NvRmChannelHandle hChannel;
    } data;
} NvRmWaitList;

typedef struct
{
    NvRmWaitList* WaitHead[NV_HOST1X_SYNCPT_NB_PTS];
    NvRmWaitList* FreeHead;
    NvRmWaitList* PageHead;
    NvOsThreadHandle WorkerThread;
    NvOsMutexHandle Mutex;
    NvOsSemaphoreHandle WakeUp;
    NvBool Shutdown;
    NvU32 ThresholdsPassed;
    NvOsInterruptHandle HostInterruptHandle;
    NvOsInterruptHandle Host1xInterruptHandle;
} NvRmHostContext;

static NvRmHostContext s_NvRmHostContext;


/*** HW sync point threshold interrupt management ***/

/* Technically g_channelState.SyncPoint.thresh_int_mask should be protected by
 * g_channelMutex.  But we need to access it from the ISR, so that's no good.
 * But the only other place that touches it is in NvRmPrivResetSyncpt, and
 * that's only called:
 *  - on init
 *  - after flushing all channels, with g_channelMutex held
 * In either case, no new waits can be added to the ISR so access is OK.
 * TODO: Maybe move it in here, out of the channel state?
 */

/**
 * Enable a hardware interrupt to CPU0 (not AVP) when sync point 'id' passes
 * threshold 'thresh'. Actually the hardware only checks the lower 16 bits!
 */
static void
EnableThresholdInterrupt(NvU32 id, NvU32 thresh)
{
    NvU32 reg;
    NvU32 idx;

    // set the threshold, cpu mask. use sync point 0 for drf macro,
    // as all syncpt registers have the same layout.
    reg = NV_DRF_NUM(HOST1X_SYNC, SYNCPT_INT_THRESH_0, INT_THRESH_0, thresh);
    HOST1X_SYNC_REGW(HOST1X_SYNC_SYNCPT_INT_THRESH_0_0 + (id * 4), reg);

    // FIXME: support CPU1
    idx = id / 16;
    reg = g_channelState.SyncPointShadow.thresh_int_mask[idx];
    reg = reg | (1UL << ((id%16) * 2));
    g_channelState.SyncPointShadow.thresh_int_mask[idx] = reg;
    HOST1X_SYNC_REGW(HOST1X_SYNC_SYNCPT_THRESH_INT_MASK_0_0 + (idx * 4), reg);
}

// Widen each bit in the bottom half of val with zeros: abcd... -> 0a0b0c0d...
static NV_INLINE NvU32 BitWiden16To32(NvU32 val)
{
    val = (val | (val << 8)) & 0x00ff00ff;
    val = (val | (val << 4)) & 0x0f0f0f0f;
    val = (val | (val << 2)) & 0x33333333;
    val = (val | (val << 1)) & 0x55555555;
    return val;
}

/**
 * As EnableThresholdInterrupt, but takes a vector of ids to set,
 * and an array of threshold values.
 */
static void
EnableThresholdInterruptMult(NvU32 ids, NvU32* threshs)
{
    NvU32 idx = HOST1X_SYNC_SYNCPT_INT_THRESH_0_0;
    NvU32 bits, reg;

    for (bits = ids; bits; bits >>= 1, idx += 4, ++threshs)
        if (bits & 1)
        {
            reg = *threshs;
            reg = NV_DRF_NUM(HOST1X_SYNC,SYNCPT_INT_THRESH_0,INT_THRESH_0, reg);
            HOST1X_SYNC_REGW(idx, reg);
        }

    reg = ids & 0xffff;
    if (reg)
    {
        reg = BitWiden16To32(reg);
        reg |= g_channelState.SyncPointShadow.thresh_int_mask[0];
        g_channelState.SyncPointShadow.thresh_int_mask[0] = reg;
        HOST1X_SYNC_REGW(HOST1X_SYNC_SYNCPT_THRESH_INT_MASK_0_0, reg);
    }
    reg = ids >> 16;
    if (reg)
    {
        reg = BitWiden16To32(reg);
        reg |= g_channelState.SyncPointShadow.thresh_int_mask[1];
        g_channelState.SyncPointShadow.thresh_int_mask[1] = reg;
        HOST1X_SYNC_REGW(HOST1X_SYNC_SYNCPT_THRESH_INT_MASK_1_0, reg);
    }
}

/**
 * Disable a hardware interrupt to either CPU or AVP for sync points
 * with bits set in 'ids'
 */
static void
DisableThresholdInterruptMult(NvU32 ids)
{
    NvU32 reg = ids & 0xffff;
    if (reg)
    {
        reg = BitWiden16To32(reg);
        reg |= (reg << 1);
        reg = (~reg) & g_channelState.SyncPointShadow.thresh_int_mask[0];
        g_channelState.SyncPointShadow.thresh_int_mask[0] = reg;
        HOST1X_SYNC_REGW(HOST1X_SYNC_SYNCPT_THRESH_INT_MASK_0_0, reg);
    }
    reg = ids >> 16;
    if (reg)
    {
        reg = BitWiden16To32(reg);
        reg |= (reg << 1);
        reg = (~reg) & g_channelState.SyncPointShadow.thresh_int_mask[1];
        g_channelState.SyncPointShadow.thresh_int_mask[1] = reg;
        HOST1X_SYNC_REGW(HOST1X_SYNC_SYNCPT_THRESH_INT_MASK_1_0, reg);
    }
}

/**
 * Main part of handler for sync point threshold interrupts.
 * Executes at interrupt time, with g_SyncPointIsrMutex held.
 * Does as little as possible, handing work off to HostWorkerThread().
 */
static NvBool
HandleHostInterrupt(NvRmHostContext* ctx)
{
    NvU32 old, new, cmp;

    /* If g_SyncPointWaitListCount does not hold a reference to the host clock
     * then there are not syncpoints in the queue and this must have been a
     * spurious interrupt - ignore it (and do not touch host1x which might
     * be unclocked).
     */
    if (g_SyncPointWaitListCount == 0 ||
        g_SyncPointWaitListCountHoldsHostClockRef == 0)
        return NV_FALSE;

    // Read sync point interrupt status
    new = HOST1X_SYNC_REGR(HOST1X_SYNC_SYNCPT_THRESH_CPU0_INT_STATUS_0);
    if (new == 0)
        return NV_FALSE; // shouldn't happen

    // For each sync point that triggered, clear the interrupt.
    // First, disable the threshold for each triggered sync point.
    DisableThresholdInterruptMult(new);

    // Then, clear the interrupts by writing 1s to the status register
    HOST1X_SYNC_REGW(HOST1X_SYNC_SYNCPT_THRESH_CPU0_INT_STATUS_0, new);

    // Tell the update thread about newly passed thresholds.
    old = ctx->ThresholdsPassed;
    while (1)
    {
        cmp = (NvU32)NvOsAtomicCompareExchange32(
                         (NvS32*)&ctx->ThresholdsPassed, old, old | new);
        if (cmp == old)
            return (old == 0);
        old = cmp;
    }
}


/*** Wait list management ***/

#define WAIT_LISTS_PER_PAGE 64

/**
 * Allocate a WaitList entry.
 * Needs to hold ctx->Mutex
 */
static NvRmWaitList* AllocWaitList(NvRmHostContext* ctx)
{
    NvRmWaitList* waiter = ctx->FreeHead;
    if (!waiter)
    {
        NvRmWaitList* p;
        int i;
        // Alloc WaitList entries in chunks
        waiter = NvOsAlloc(sizeof(NvRmWaitList) * WAIT_LISTS_PER_PAGE);
        if (!waiter)
            return NULL;
        // First entry in page is used to link all pages for later freeing
        waiter->next = ctx->PageHead;
        ctx->PageHead = waiter++;
        // Link all remaining entries in the page into a new free list
        for (i = WAIT_LISTS_PER_PAGE - 2, p = waiter; i; --i, ++p)
            p->next = p + 1;
        p->next = NULL;
    }
    ctx->FreeHead = waiter->next;
    return waiter;
}

/**
 * Free all WaitList entries.
 * The free list can snake through all allocated pages, so a separate list is
 * used to link the pages together. The list links are the first element of
 * each page, so they can be freed directly.
 */
static void FreeAllWaitLists(NvRmHostContext* ctx)
{
    NvRmWaitList* list = ctx->PageHead;
    while (list)
    {
        NvRmWaitList* page = list;
        list = list->next;
        NvOsFree(page);
    }
    ctx->PageHead = NULL;
    ctx->FreeHead = NULL;
}

/**
 * Worker thread for updating the sync point WaitLists as interrupts
 * indicate that thresholds have been reached.
 */
static void
HostWorkerThread(void* arg)
{
    NvRmHostContext* ctx = arg;
    NvRmWaitList* SignalHead = NULL;
    NvRmWaitList* SignalTail = NULL;
    NvRmWaitList* UpdateHead = NULL;
    NvRmWaitList* UpdateTail = NULL;

    while (!ctx->Shutdown)
    {
        NvU32 bits, bit, i, done, reset;
        NvU32 thresh[NV_HOST1X_SYNCPT_NB_PTS];
        NvRmWaitList* waiter;
        NvBool signalAcm;

        NvOsSemaphoreWait(ctx->WakeUp);

        // Atomically fetch & zero bit vector of sync point IDs to process
        bits = (NvU32)NvOsAtomicExchange32((NvS32*)&ctx->ThresholdsPassed, 0);
        if (ctx->Shutdown && !bits)
            break;
        NV_ASSERT(bits);

        NvOsMutexLock(ctx->Mutex);

        // Free leftovers from last iteration
        if (UpdateTail)
        {
            UpdateTail->next = ctx->FreeHead;
            ctx->FreeHead = UpdateHead;
            UpdateHead = UpdateTail = NULL;
        }
        if (SignalTail)
        {
            SignalTail->next = ctx->FreeHead;
            ctx->FreeHead = SignalHead;
            SignalHead = SignalTail = NULL;
        }

        // For each signalled sync point ID...
        done = 0;
        reset = 0;
        for (bit = 1, i = 0; bits; bit <<= 1, ++i)
            if (bits & bit)
            {
                // Read the sync point register
                NvU32 sync = HOST1X_SYNC_REGR(HOST1X_SYNC_SYNCPT_0_0 + (i * 4));

                // run through the wait list and gather all completed waiters
                // into two lists, one for updates and one for signals
                for (waiter = ctx->WaitHead[i];
                     waiter && ((NvS32)(waiter->thresh - sync) <= 0);
                     waiter = waiter->next, ++done)
                    if (waiter->action == NvRmWaitListAction_Wait)
                    {
                        if (SignalTail)
                            SignalTail->next = waiter;
                        else
                            SignalHead = waiter;
                        SignalTail = waiter;
                    }
                    else
                    {
                        NV_ASSERT(waiter->action == NvRmWaitListAction_Submit);
                        if (UpdateTail)
                            UpdateTail->next = waiter;
                        else
                            UpdateHead = waiter;
                        UpdateTail = waiter;
                    }

                /* reset wait list */
                ctx->WaitHead[i] = waiter;
                if (waiter)
                {
                    reset |= bit;
                    thresh[i] = waiter->thresh;
                }

                bits ^= bit;
            }

        NvOsMutexUnlock(ctx->Mutex);

        NvOsIntrMutexLock(g_SyncPointIsrMutex);

        // Update wait list count and determine need to signal ACM thread
        g_SyncPointWaitListCount -= done;
        signalAcm = (g_SyncPointWaitListCount == 0);

        // reset HW threshold registers for all processed, non-empty lists
        EnableThresholdInterruptMult(reset, thresh);

        NvOsIntrMutexUnlock(g_SyncPointIsrMutex);

        // Update channel sync queues first, so resources are freed
        if (UpdateHead)
            for (waiter = UpdateHead; ; waiter = waiter->next)
            {
                NvRmPrivUpdateSyncQueue(waiter->data.hChannel);
                if (waiter == UpdateTail)
                    break;
            }

        // Then signal anyone waiting on a sync point semaphore
        if (SignalHead)
            for (waiter = SignalHead; ; waiter = waiter->next)
            {
                NvOsSemaphoreSignal(waiter->data.sem);
                // Destroy our clone of the user's semaphore
                NvOsSemaphoreDestroy(waiter->data.sem);
                if (waiter == SignalTail)
                    break;
            }

        // Finally signal the ACM thread if everything's finished
        if (signalAcm)
            NvRmPrivScheduleAcmCheck();
    }

    // Free any leftovers
    if (UpdateTail || SignalTail)
    {
        NvOsMutexLock(ctx->Mutex);
        if (UpdateTail)
        {
            UpdateTail->next = ctx->FreeHead;
            ctx->FreeHead = UpdateHead;
        }
        if (SignalTail)
        {
            SignalTail->next = ctx->FreeHead;
            ctx->FreeHead = SignalHead;
        }
        NvOsMutexUnlock(ctx->Mutex);
    }
}

NvError
NvRmPrivWaitListSchedule(
    NvRmDeviceHandle hDevice,
    NvU32 id,
    NvU32 thresh,
    NvRmWaitListAction action,
    void* data)
{
    NvRmHostContext* ctx = &s_NvRmHostContext;
    NvError err;
    NvRmWaitList *waiter = 0;
    NvRmWaitList *list;
    NvBool lowest = NV_FALSE;
    NvBool enableHostClock = NV_FALSE;

    NV_ASSERT(data);

    NvOsMutexLock(ctx->Mutex);

    // Try to pull a waiter off the free list. If that fails, create a new one.
    waiter = AllocWaitList(ctx);
    if (!waiter)
    {
        NvOsMutexUnlock(ctx->Mutex);
        return NvError_InsufficientMemory;
    }

    waiter->next = 0;
    waiter->thresh = thresh;
    waiter->action = action;
    switch (action)
    {
        case NvRmWaitListAction_Wait:
            /* must clone the semaphore */
            err = NvOsSemaphoreClone(*(NvOsSemaphoreHandle*)data,
                                     &waiter->data.sem);
            if (err)
            {
                NV_ASSERT(!"semaphore clone failed");
                waiter->next = ctx->FreeHead;
                ctx->FreeHead = waiter;
                NvOsMutexUnlock(ctx->Mutex);
                return err;
            }
            break;

        case NvRmWaitListAction_Submit:
            waiter->data.hChannel = *(NvRmChannelHandle*)data;
            break;

        default:
            NV_ASSERT(0);
    }

    // add to wait list, sorted by threshold
    // TODO: I did some stats and we almost always add at or near the end of
    // the list.  Almost all entries are for sync queue updates, and those
    // always get added at the end (or would do, if we searched from the
    // end of the list; coming from the head, equal threshold entries stop
    // before the end).  They must go at the end because otherwise someone
    // has done a sync point wait for a value they can't know yet, because
    // their stuff hasn't been submitted!
    // One thing that does currently happen is that context-no-change mini
    // submits don't add an end-of-submit sync point increment as they should,
    // so they end up doing a duplicate sync queue update.  AnttiH is going
    // to fix all the context change stuff anyway.  So yeah, we should
    // rename these things "queue"s, make them doubly linked, and insert
    // new items starting from the tail.  Or from whichever end has the
    // closest threshold?
    list = ctx->WaitHead[id];
    if (!list)
    {
        // Add as a first element
        waiter->next = NULL;
        ctx->WaitHead[id] = waiter;
        lowest = NV_TRUE;
    }
    else
    {
        // Not the first element.
        while (list->next && ((NvS32)(list->next->thresh - thresh) < 0))
            list = list->next;
        waiter->next = list->next;
        list->next = waiter;
    }

    NvOsMutexUnlock(ctx->Mutex);

    HOST_CLOCK_ENABLE(hDevice);

    NvOsIntrMutexLock(g_SyncPointIsrMutex);

    g_SyncPointWaitListCount++;
    if (!g_SyncPointWaitListCountHoldsHostClockRef)
    {
        NV_ASSERT(g_SyncPointWaitListCount==1);
        // keep enabled until g_SyncPointWaitListCount becomes 0
        // Corresponding HOST_CLOCK_DISABLE() is in NvRmChPrivPowerdown()
        g_SyncPointWaitListCountHoldsHostClockRef = NV_TRUE;
        enableHostClock = NV_TRUE;
    }

    // if the given threshold is the smallest then setup the threshold.
    if (lowest)
        EnableThresholdInterrupt(id, thresh);

    NvOsIntrMutexUnlock(g_SyncPointIsrMutex);

    /* If this is the first g_SyncPointWaitListCount (i.e. if
     * g_SyncPointWaitListCountHoldsHostClockRef was false above) then keep the
     * host clock enabled.  In this case the above HOST_CLOCK_ENABLE() is
     * balanced by the HOST_CLOCK_DISABLE() in NvRmChPrivPowerdown().
     *
     * Otherwise the host clock was already enabled for
     * g_SyncPointWaitListCount, so disable it here to balance the above
     * HOST_CLOCK_DISABLE().
     */
    if (!enableHostClock)
    {
        HOST_CLOCK_DISABLE(hDevice);
    }

    return NvSuccess;
}


/*** Interrupt service functions ***/

/**
 * Host1x intterrupt service function
 * Handles read / write failures
 */
static void
Host1xInterruptService(void *arg)
{
    NvRmHostContext* ctx = arg;
    NvU32 stat;
    NvU32 ext_stat;
    NvU32 addr;
    NvU32 read_error, write_error;

    stat = HOST1X_SYNC_REGR( HOST1X_SYNC_HINTSTATUS_0 );

    /* avoid the warning? */
    (void)addr;

    if (1)
    {
        ext_stat = HOST1X_SYNC_REGR( HOST1X_SYNC_HINTSTATUS_EXT_0);

        NV_DEBUG_PRINTF(("Host ext interrupt status register 0x%x", ext_stat));

        read_error = (NvU8)NV_DRF_VAL( HOST1X_SYNC, HINTSTATUS_EXT,
            IP_READ_INT, ext_stat );
        write_error = (NvU8)NV_DRF_VAL( HOST1X_SYNC, HINTSTATUS_EXT,
            IP_WRITE_INT, ext_stat );

        if (read_error)
        {
            addr = HOST1X_SYNC_REGR( HOST1X_SYNC_IP_READ_TIMEOUT_ADDR_0);

            NV_DEBUG_PRINTF(("Read failed in Graphics host aperture at "
                "address 0x%x", addr));
        }

        if (write_error)
        {
            addr = HOST1X_SYNC_REGR( HOST1X_SYNC_IP_WRITE_TIMEOUT_ADDR_0);
            NV_DEBUG_PRINTF(("Write failed in Graphics host aperture at "
                "address 0x%x", addr));
        }

        /* Clear the host1x EXT interrupts */
        HOST1X_SYNC_REGW( HOST1X_SYNC_HINTSTATUS_EXT_0, ext_stat);
    }

    /* Clear the host1x interrupts */
    HOST1X_SYNC_REGW( HOST1X_SYNC_HINTSTATUS_0, stat);

    NvOsBreakPoint(NULL, 0, NULL);
    NvRmInterruptDone(ctx->Host1xInterruptHandle);
}

/**
 * Host interrupt service function
 * Handles sync point threshold triggers
 */
static void
HostInterruptService(void *arg)
{
    NvRmHostContext* ctx = arg;
    NvBool SignalWorkerThread;

    NvOsIntrMutexLock(g_SyncPointIsrMutex);
    SignalWorkerThread = HandleHostInterrupt(ctx);
    NvOsIntrMutexUnlock(g_SyncPointIsrMutex);

    NvRmInterruptDone(ctx->HostInterruptHandle);

    if (SignalWorkerThread)
        NvOsSemaphoreSignal(ctx->WakeUp);
}


/*** Init & shutdown ***/

static void InterruptShutdown(
    NvRmHostContext* ctx,
    NvRmDeviceHandle rm);

static NvError
InterruptInit(
    NvRmHostContext* ctx,
    NvRmDeviceHandle rm)
{
    NvError err;
    NvOsInterruptHandler h;
    NvU32 irq;

    g_SyncPointIsrMutex = NULL;
    g_SyncPointWaitListCount = 0;

    NvOsMemset(ctx, 0, sizeof(NvRmHostContext));

    /* Index 1 is host1x general interrupt */
    irq = NvRmGetIrqForLogicalInterrupt(rm,
        NVRM_MODULE_ID(NvRmModuleID_GraphicsHost, 0), 1);

    h = Host1xInterruptService;
    err = NvRmInterruptRegister(rm, 1, &irq, &h, ctx,
                                &ctx->Host1xInterruptHandle, NV_TRUE);
    if (err != NvSuccess)
    {
        ctx->Host1xInterruptHandle = NULL;
        goto exit_gracefully;
    }

    err = NvOsIntrMutexCreate(&g_SyncPointIsrMutex);
    if (err != NvSuccess)
    {
        g_SyncPointIsrMutex = NULL;
        goto exit_gracefully;
    }

    err = NvOsMutexCreate(&ctx->Mutex);
    if (err != NvSuccess)
    {
        ctx->Mutex = NULL;
        goto exit_gracefully;
    }

    err = NvOsSemaphoreCreate(&ctx->WakeUp, 0);
    if (err != NvSuccess)
    {
        ctx->WakeUp = NULL;
        goto exit_gracefully;
    }

    // TODO: Maybe this should be NvOsInterruptPriorityThreadCreate?
    // Maybe for channel-not-in-kernel? I prefer it to be kinda lazy
    // unless someone's actually waiting for it to do stuff...
    err = NvOsThreadCreate(HostWorkerThread, ctx, &ctx->WorkerThread);
    if (err != NvSuccess)
    {
        ctx->WorkerThread = NULL;
        goto exit_gracefully;
    }

    /* Index 0 is syncPt interrupt */
    irq = NvRmGetIrqForLogicalInterrupt(rm,
        NVRM_MODULE_ID(NvRmModuleID_GraphicsHost, 0), 0);
    h = HostInterruptService;
    err = NvRmInterruptRegister(rm, 1, &irq, &h, ctx,
                                &ctx->HostInterruptHandle, NV_TRUE);
    if (err != NvSuccess)
    {
        ctx->HostInterruptHandle = NULL;
        goto exit_gracefully;
    }
    return NvSuccess;

exit_gracefully:
    InterruptShutdown(ctx, rm);
    return err;
}

static void
InterruptShutdown(
    NvRmHostContext* ctx,
    NvRmDeviceHandle rm)
{
    /* Really, here we need to make sure that all threshold interrupts have been
     * disabled, otherwise we'll get unhandled interrupts. Unfortunately it's a
     * bit hard to arrange, because the worker thread can be re-enabling them.
     * We can hack in some stuff to stop that happening, but really we need to
     * make sure the wait lists are empty before shutting down interrupts.
     * Otherwise we'll leak semaphores and hang their waiters.
     */

    if (ctx->HostInterruptHandle)
    {
        NvRmInterruptUnregister(rm, ctx->HostInterruptHandle);
        ctx->HostInterruptHandle = NULL;
    }

    if (ctx->WorkerThread)
    {
        ctx->Shutdown = NV_TRUE;
        NvOsSemaphoreSignal(ctx->WakeUp);
        NvOsThreadJoin(ctx->WorkerThread);
        ctx->WorkerThread = NULL;
    }

    NvOsSemaphoreDestroy(ctx->WakeUp);
    ctx->WakeUp = NULL;

    NvOsMutexDestroy(ctx->Mutex);
    ctx->Mutex = NULL;

    NvOsIntrMutexDestroy(g_SyncPointIsrMutex);
    g_SyncPointIsrMutex = NULL;

    if (ctx->Host1xInterruptHandle)
    {
        NvRmInterruptUnregister(rm, ctx->Host1xInterruptHandle);
        ctx->Host1xInterruptHandle = NULL;
    }

    FreeAllWaitLists(ctx);
}

/**
 * Host1x configuration.
 */
void
NvRmPrivHostInit(NvRmDeviceHandle rm)
{
    NvRmHostContext* ctx = &s_NvRmHostContext;
    NvU32 reg;

    HOST_CLOCK_ENABLE( rm );

    InterruptInit(ctx, rm);

    /* disable the ip_busy_timeout.  this prevents write drops, etc.  there's
     * no real way to reocver from a hung client anyway.
     */
    reg = NV_DRF_NUM( HOST1X_SYNC, IP_BUSY_TIMEOUT, IP_BUSY_TIMEOUT,
        NVRM_DIRECT_ACCESS_TIMEOUT);
    HOST1X_SYNC_REGW( HOST1X_SYNC_IP_BUSY_TIMEOUT_0, reg );

    /* masking all of the interrupts actually means "enable" */
    reg = NV_DRF_DEF( HOST1X_SYNC, INTMASK, CPU0_INT_MASK_ALL, ENABLE );
    HOST1X_SYNC_REGW( HOST1X_SYNC_INTMASK_0, reg );

    reg = NV_DRF_DEF( HOST1X_SYNC, INTC0MASK, HOST_INT_C0MASK,
            ENABLE );
    HOST1X_SYNC_REGW( HOST1X_SYNC_INTC0MASK_0, reg );

    reg = NV_DRF_DEF( HOST1X_SYNC, HINTMASK, HINTSTATUS_EXT_INTMASK,
            ENABLE );
    HOST1X_SYNC_REGW( HOST1X_SYNC_HINTMASK_0, reg );

    reg = NV_DRF_DEF( HOST1X_SYNC, HINTMASK_EXT, IP_WRITE_INTMASK, ENABLE ) |
        NV_DRF_DEF( HOST1X_SYNC, HINTMASK_EXT, IP_READ_INTMASK, ENABLE);

    HOST1X_SYNC_REGW( HOST1X_SYNC_HINTMASK_EXT_0, reg );

    HOST_CLOCK_DISABLE( rm );
}

void
NvRmPrivHostShutdown(NvRmDeviceHandle rm)
{
    NvRmHostContext* ctx = &s_NvRmHostContext;

    HOST_CLOCK_ENABLE(rm);
    InterruptShutdown(ctx, rm);
    HOST_CLOCK_DISABLE(rm);
}
