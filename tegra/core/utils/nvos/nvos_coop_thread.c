/*
 * Copyright (c) 2007-2009 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvos_coop_thread.h"
#include "nvos.h"
#include "nvos_internal.h"
#include "nvassert.h"

static ThreadQueue s_Runnable;
static ThreadQueue s_LowPri;
static ThreadQueue s_Wait;
static ThreadQueue s_WaitTimeout;
static NvOsThread *s_CurrentThread;
static NvU32 s_NumThreads;
static NvOsThread s_MainThread;
static NvOsIntrMutexHandle s_IntrMutex;
static NvU32 s_TlsUsedMask = 0;

void
CoopEnqueue( ThreadQueueNode *n, ThreadQueue *q )
{
    NV_ASSERT( n && q );
    NV_ASSERT( n->next == 0 && n->prev == 0 );

    n->next = &q->tail;
    n->prev = q->tail.prev;

    q->tail.prev->next = n;
    q->tail.prev = n;
}

NvOsThreadHandle
CoopDequeue( ThreadQueue *q )
{
    NvOsThreadHandle tmp;
    ThreadQueueNode *n = q->head.next;
    if( n == &q->tail )
    {
        return 0;
    }

    tmp = n->thread;
    n->prev->next = n->next;
    n->next->prev = n->prev;
    n->next = 0;
    n->prev = 0;
    return tmp;
}

void
CoopQueueRemove( ThreadQueueNode *n )
{
    NV_ASSERT( n );
    NV_ASSERT( n->next != n );
    NV_ASSERT( n->prev != n );

    n->prev->next = n->next;
    n->next->prev = n->prev;
    n->next = 0;
    n->prev = 0;
}

void
CoopQueueInit( ThreadQueue *q )
{
    NV_ASSERT( q );

    q->head.thread = 0;
    q->tail.thread = 0;

    q->head.next = &q->tail;
    q->head.prev = 0;
    q->tail.prev = &q->head;
    q->tail.next = 0;
}

static void
CoopEnqueuePri( NvOsThreadHandle t )
{
    if( t->priority == CoopThreadPriority_Normal )
    {
        CoopEnqueue( &t->sched_queue, &s_Runnable );
    }
    else if( t->priority == CoopThreadPriority_Low )
    {
        CoopEnqueue( &t->sched_queue, &s_LowPri );
    }
    else
    {
        NV_ASSERT( !"bad thread priority" );
    }
}

void
CoopThreadSwitch(void)
{
    NvOsThreadHandle t, old;
    old = s_CurrentThread;

    NV_ASSERT( old->sched_queue.next == 0 &&
               old->sched_queue.prev == 0 );

    /* put the current thread back on a thread queue */
    switch( old->state ) {
    case CoopThreadState_Running:
        CoopEnqueuePri( old );
        break;
    case CoopThreadState_Waiting:
        CoopEnqueue( &old->sched_queue, &s_Wait );
        break;
    case CoopThreadState_WaitingTimeout:
        CoopEnqueue( &old->sched_queue, &s_WaitTimeout );
        break;
    case CoopThreadState_Stopped:
        /* don't do anything */
        break;
    default:
        NV_ASSERT( !"bad thread state" );
        return;
    }

    /* switch the threads */
    do
    {
        /* check for timed-out threads */
        CoopThreadTimeout();

        t = CoopDequeue( &s_Runnable );
        if( !t )
        {
            t = CoopDequeue( &s_LowPri );
        }
    } while( !t );

    NV_ASSERT( t->state == CoopThreadState_Running );
    NV_ASSERT( t->sched_queue.next == 0 && t->sched_queue.prev == 0 );
    NV_ASSERT( t->sync_queue.next == 0 && t->sync_queue.prev == 0 );

    s_CurrentThread = t;

    if (s_NumThreads > 1)
    {
        /* signal the next thread */
        NvOsSemaphoreSignalInternal( t->sem );

        if( old->state != CoopThreadState_Stopped )
        {
            /* wait for this thread to become active again */
            NvOsSemaphoreWaitInternal( old->sem );
        }
    }
}

static void
CoopResumeFromSync( NvOsThreadHandle t )
{
    NV_ASSERT( t );
    NV_ASSERT( t->sched_queue.next == 0 &&
               t->sched_queue.prev == 0 );
    NV_ASSERT( t->state != CoopThreadState_Running );

    t->state = CoopThreadState_Running;
    CoopEnqueuePri( t );
}

void
CoopThreadTimeout(void)
{
    ThreadQueueNode *n;
    NvOsThreadHandle t;
    NvU32 time;

    time = NvOsGetTimeMS();

    n = s_WaitTimeout.head.next;
    while( n != &s_WaitTimeout.tail )
    {
        t = n->thread;
        NV_ASSERT( t->state == CoopThreadState_WaitingTimeout );

        if( t->timeout <= time )
        {
            ThreadQueueNode *tmp;

            /* remove the thread from the wait-timeout queue */
            tmp = n->next;
            CoopQueueRemove( n );
            n = tmp;

            /* set the expired flags and resume the thread */
            t->time_expired = NV_TRUE;
            t->timeout = 0;

            /* handle re-scheduling, sync object clean up, etc. */
            if( t->sync_queue.next )
            {
                CoopQueueRemove( &t->sync_queue );
            }

            CoopResumeFromSync( t );
        }
        else
        {
            n = n->next;
        }
    }
}

static void
coop_thread_wrapper(void *p)
{
    NvOsThread *t = p;

    /* thread starts off enqueued to the end of the run queue.  when it
     * reaches the head, it will be signaled.
     */
    NvOsSemaphoreWaitInternal(t->sem);

    t->function(t->args);

    /* prevent this from getting scheduled */
    t->state = CoopThreadState_Stopped;

    /* signal the join semaphore */
    CoopSemaphoreSignal(&t->join_sem);

    /* run the next thread */
    CoopThreadSwitch();
}

static NvError
CoopThreadInit(NvOsThread *t, NvOsThreadFunction function, 
    void *args)
{
    NvError err;

    t->function = function;
    t->args = args;
    t->state = CoopThreadState_Running;
    t->priority = CoopThreadPriority_Normal;
        
    /* setup the thread queues */
    t->sync_queue.thread = t;
    t->sync_queue.next = 0;
    t->sync_queue.prev = 0;
    t->sched_queue.thread = t;
    t->sched_queue.next = 0;
    t->sched_queue.prev = 0;

    CoopSemaphoreInit(&t->join_sem, 0);

    NvOsMemset(t->Tls, 0, sizeof(t->Tls));

    err = NvOsSemaphoreCreateInternal(&t->sem, 0);
    if (err != NvSuccess)
    {
        return err;
    }

    return NvSuccess;
}

NvError
CoopThreadCreate(NvOsThreadFunction function, 
    void *args, NvOsThreadHandle *thread)
{
    NvError err;
    NvOsThread *t = 0;

    NV_ASSERT(function);
    NV_ASSERT(thread);

    s_NumThreads++;

    t = NvOsAlloc(sizeof(NvOsThread));
    if (!t)
    {
        err = NvError_InsufficientMemory;
        goto fail;
    }

    err = CoopThreadInit(t, function, args);
    if (err != NvSuccess)
    {
        goto fail;
    }

    CoopEnqueue( &t->sched_queue, &s_Runnable );

    err = NvOsThreadCreateInternal(coop_thread_wrapper, t,
        &t->thread, NvOsThreadPriorityType_Normal);
    if (err != NvSuccess)
    {
        err = NvError_InsufficientMemory;
        goto fail;
    }

    *thread = t;

    err = NvSuccess;
    goto clean;

fail:
    s_NumThreads--;
    NvOsFree(t);

clean:
    return err;
}

NvError
CoopThreadSetLowPriority(void)
{
    NvOsThread *t;
    t = s_CurrentThread;

    t->priority = CoopThreadPriority_Low;
    CoopThreadSwitch();
    return NvSuccess;
}

void
CoopThreadYield(void)
{
    if (s_NumThreads > 1)
    {
        CoopThreadSwitch();
    }
}

void
CoopThreadJoin(NvOsThreadHandle t)
{
    NV_ASSERT( t );
    NV_ASSERT( t->thread );

    CoopSemaphoreWait(&t->join_sem, NV_WAIT_INFINITE);

    NvOsThreadJoinInternal(t->thread);

    s_NumThreads--;

    /* finally free the local storage */
    NvOsFree(t);
}

void
CoopSemaphoreInit( NvOsSemaphore *sem, NvU32 count )
{
    CoopQueueInit( &sem->wait_queue );
    sem->count = count;
    sem->key[0] = 0;
    sem->refcount = 1;
}

NvError CoopSemaphoreCreate(NvOsSemaphoreHandle *semaphore, NvU32 value) 
{
    NvOsSemaphore *s = NvOsAlloc(sizeof(NvOsSemaphore));
    if (!s)
        return NvError_InsufficientMemory;
    CoopSemaphoreInit(s, value);
    *semaphore = s;
    return NvSuccess;
}

NvError
CoopSemaphoreWait(NvOsSemaphoreHandle sem, NvU32 timeout)
{
    NvError err = NvSuccess;
    NvBool signaled = NV_FALSE;
    NvOsThread *t;

    NvOsIntrMutexLock( s_IntrMutex );
    if (sem->count)
    {
        sem->count--;
        signaled = NV_TRUE;
    }
    NvOsIntrMutexUnlock( s_IntrMutex );

    if( signaled )
    {
        return NvSuccess;
    }

    t = s_CurrentThread;

    /* add the thread to the wait list */
    CoopEnqueue( &t->sync_queue, &sem->wait_queue );

    if( timeout == NV_WAIT_INFINITE )
    {
        t->state = CoopThreadState_Waiting;
        t->timeout = 0;
    }
    else
    {
        t->state = CoopThreadState_WaitingTimeout;
        t->timeout = NvOsGetTimeMS() + timeout;
    }

    /* when this returns, the semaphore has been signaled */
    CoopThreadSwitch();

    NV_ASSERT( sem->count != (NvU32)-1 );
    NV_ASSERT( t->state == CoopThreadState_Running );
    NV_ASSERT( t->sched_queue.next == 0 && t->sched_queue.prev == 0 );
    NV_ASSERT( t->sync_queue.next == 0 && t->sync_queue.prev == 0 );

    /* check for expired timeout and cleanup */
    if( timeout != NV_WAIT_INFINITE )
    {
        t->timeout = 0;

        if( t->time_expired )
        {
            t->time_expired = NV_FALSE;
            err = NvError_Timeout;
        }
    }

    return err;
}

void
CoopSemaphoreSignal( NvOsSemaphoreHandle sem )
{
    NvBool incr = NV_TRUE;

    NV_ASSERT( sem );

    NvOsIntrMutexLock( s_IntrMutex );

    /* signal the first waiting thread if count is zero */
    if( sem->count == 0 )
    {
        NvOsThreadHandle t = CoopDequeue( &sem->wait_queue );
        if( t )
        {
            NV_ASSERT( t->state == CoopThreadState_Waiting ||
                       t->state == CoopThreadState_WaitingTimeout );
            NV_ASSERT( t->sync_queue.next == 0 && t->sync_queue.prev == 0 );

            CoopQueueRemove( &t->sched_queue );
            CoopResumeFromSync( t );
            /* don't increment if a thread was signaled */
            incr = NV_FALSE;

            NV_ASSERT( t->sched_queue.next != 0 &&
                       t->sched_queue.prev != 0 );
            NV_ASSERT( t->state == CoopThreadState_Running );
        }
    }

    if( incr )
    {
        /* increment the semahpore value */
        sem->count++;
    }

    NvOsIntrMutexUnlock( s_IntrMutex );
}

NvError
CoopSemaphoreClone(NvOsSemaphoreHandle orig, NvOsSemaphoreHandle *clone)
{
    orig->refcount++;
    *clone = orig;
    return NvSuccess;
}

void
CoopSemaphoreDestroy(NvOsSemaphoreHandle s)
{
    s->refcount--;
    if (!s->refcount)
    {
        NvOsFree(s);
    }
}

NvError
CoopMutexCreate(const char *key, NvOsMutexHandle *mutex)
{
    NvOsMutex *m = NvOsAlloc(sizeof(NvOsMutex));
    if (!m)
    {
        return NvError_InsufficientMemory;
    }

    CoopQueueInit( &m->wait_queue );
    m->owner = 0;
    m->count = 0;
    m->key[0] = 0;
    m->refcount = 1;

    *mutex = m;
    return NvSuccess;
}

void
CoopMutexLock( NvOsMutexHandle mutex )
{
    NV_ASSERT ( mutex );
    if ( !mutex ) return;

    if( mutex->owner == 0 )
    {
        mutex->owner = s_CurrentThread;
        mutex->count = 1;
    }
    else if( mutex->owner == s_CurrentThread )
    {
        mutex->count++;
    }
    else
    {
        CoopEnqueue( &s_CurrentThread->sync_queue, &mutex->wait_queue );
        s_CurrentThread->state = CoopThreadState_Waiting;

        CoopThreadSwitch();
    }
}

void
CoopMutexUnlock( NvOsMutexHandle mutex )
{
    NvOsThreadHandle t;

    NV_ASSERT ( mutex );
    NV_ASSERT( mutex->owner == s_CurrentThread );
    NV_ASSERT( mutex->count != 0 );
    if ( !mutex ) return;

    mutex->count--;
    if( !mutex->count )
    {
        /* give the lock to the next waiter */
        t = CoopDequeue( &mutex->wait_queue );
        if( t )
        {
            mutex->owner = t;
            mutex->count = 1;
            CoopQueueRemove( &t->sched_queue );
            CoopResumeFromSync( t );
        }
        else
        {
            mutex->owner = 0;
        }
    }
}

void
CoopMutexDestroy(NvOsMutexHandle mutex)
{
    NV_ASSERT ( mutex );
    if ( !mutex ) return;

    NvOsFree(mutex);
}

NvU32 CoopTlsAlloc(void)
{
    // Find an unused Thread Local Storage index.
    // Use AtomicExchange to avoid needing a mutex
    for (;;)
    {
        NvU32 mask = s_TlsUsedMask;
        NvU32 mask2;
        NvU32 idx;
        for (idx=0; mask & (1<<idx); idx++)
        {
            if (idx >= NVOS_TLS_CNT)
                return NVOS_INVALID_TLS_INDEX;
        }
        mask2 = mask | (1<<idx);
        if ((NvU32)NvOsAtomicCompareExchange32(
                            (NvS32*)&s_TlsUsedMask,
                            (NvS32)mask,
                            (NvS32)mask2) == mask)
        {
            return idx;
        }
    }
}

void CoopTlsFree(NvU32 TlsIndex)
{
    NV_ASSERT(TlsIndex < NVOS_TLS_CNT || TlsIndex == NVOS_INVALID_TLS_INDEX);
    if (TlsIndex >= NVOS_TLS_CNT)
        return;
    NV_ASSERT(s_TlsUsedMask & (1<<TlsIndex));
    for (;;)
    {
        NvU32 mask = s_TlsUsedMask;
        NvU32 mask2 = mask & ~(1<<TlsIndex);
        NV_ASSERT(mask & (1<<TlsIndex));
        if ((NvU32)NvOsAtomicCompareExchange32(
                            (NvS32*)&s_TlsUsedMask,
                            (NvS32)mask,
                            (NvS32)mask2) == mask)
            return;
    }
}

void *CoopTlsGet(NvU32 TlsIndex)
{
    NV_ASSERT(s_TlsUsedMask & (1<<TlsIndex));
    if (TlsIndex >= NVOS_TLS_CNT)
        return NULL;
    return s_CurrentThread->Tls[TlsIndex];
}

void CoopTlsSet(NvU32 TlsIndex, void *Value)
{
    NV_ASSERT(s_TlsUsedMask & (1<<TlsIndex));
    if (TlsIndex >= NVOS_TLS_CNT)
        return;
    s_CurrentThread->Tls[TlsIndex] = Value;
}

NvError CoopTlsAddTerminator(void (*func)(void *), void *context)
{
    return NvError_NotSupported;
}

NvBool CoopTlsRemoveTerminator(void (*func)(void *), void *context)
{
    //Not Supported
    return NV_FALSE;
}

void
CoopSleepMS(NvU32 msec)
{
    if (s_NumThreads > 1)
    {
        NvOsThread *t;
        t = s_CurrentThread;

        t->state = CoopThreadState_WaitingTimeout;
        t->timeout = NvOsGetTimeMS() + msec;

        CoopThreadSwitch();
    }
    else
    {
        NvOsSleepMSInternal(msec);
    }
}

/* from nvos_thread.c */
extern NvBool g_UseCoopThread;

NvError
NvOsThreadMode(int coop)
{
    static int first = 1;
    if (!first)
        return NvError_AlreadyAllocated;
    first = 0;

    NV_ASSERT(!g_UseCoopThread);

    if (coop)
    {
        NvError e;
        NV_CHECK_ERROR( NvOsIntrMutexCreate( &s_IntrMutex ) );

        /* initialize thread queues */
        CoopQueueInit(&s_Runnable);
        CoopQueueInit(&s_LowPri);
        CoopQueueInit(&s_Wait);
        CoopQueueInit(&s_WaitTimeout);

        g_UseCoopThread = NV_TRUE;

        CoopThreadInit(&s_MainThread, 0, 0);
        s_CurrentThread = &s_MainThread;
        s_NumThreads = 1;
    }
    return NvSuccess;
}
