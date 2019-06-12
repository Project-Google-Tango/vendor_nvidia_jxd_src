/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA CORPORATION
 * is strictly prohibited.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvos.h"
#include "nvos_internal.h"
#include "nvodm_pinmux_init.h"
#include "nvtest.h"
#include "nvassert.h"
#include "nvutil.h"
#include "aos_semihost.h"
#include "aos.h"
#include "nvrm_hardware_access.h"
#include "nvrm_drf.h"

#ifdef BUILD_FOR_COSIM
#include "asim/dev_asim.h"
#endif

/** AOS implementation */

ThreadQueue g_Runnable;
ThreadQueue g_Wait;
ThreadQueue g_WaitTimeout;
NvOsThreadHandle g_CurrentThread;
NvOsThread g_MainThread;
NvOsThread g_BackgroundThread;
NvAosEnv *g_Env;
NvAosSystemState g_SystemState;
static NvU32 s_TlsUsedMask = 0;

NvU32 s_BlStart, s_BlEnd;
// Initializing this variable, to makesure not to fall in bss
NvU32 g_BlCpuTimeStamp = 33;

/* just in case the build system doesn't define this for the main cpu */
#ifndef NV_IS_AVP
#define NV_IS_AVP 0
#endif

#if NV_IS_AVP
// FIXME: can this be smaller?
#define NVAOS_MAIN_STACK_SIZE           (8192)
#define NVAOS_BACKGROUND_STACK_SIZE     (2048)
#else
#define NVAOS_MAIN_STACK_SIZE           (8192)
#define NVAOS_BACKGROUND_STACK_SIZE     (2048)
#endif

/* special thread stacks */
unsigned char main_stack[ NVAOS_MAIN_STACK_SIZE ];
unsigned char background_stack[ NVAOS_BACKGROUND_STACK_SIZE ];

/* circular log */
#if NV_DEBUG
static char s_log[NVAOS_LOG_ENTRIES][NVAOS_LOG_SIZE];
static NvU32 s_log_index;
#endif

/* list of all threads */
#if NV_DEBUG
ThreadQueue g_Threads;
#endif

#if NV_DEBUG
NvU32 g_ContextSwitches;
#endif

void
nvaosEnqueue( ThreadQueueNode *n, ThreadQueue *q, NvBool tail )
{
    NV_ASSERT( n && q );
    NV_ASSERT( n->next == 0 && n->prev == 0 );
    NV_ASSERT( n->thread->magic == NVAOS_MAGIC );

    if( tail )
    {
        n->next = &q->tail;
        n->prev = q->tail.prev;

        q->tail.prev->next = n;
        q->tail.prev = n;
    }
    else
    {
        n->next = q->head.next;
        n->prev = &q->head;

        q->head.next->prev = n;
        q->head.next = n;
    }
}

NvOsThreadHandle
nvaosDequeue( ThreadQueue *q )
{
    NvOsThreadHandle tmp;
    ThreadQueueNode *n = q->head.next;
    if( n == &q->tail )
    {
        return 0;
    }

    tmp = n->thread;
    NV_ASSERT( tmp->magic == NVAOS_MAGIC );

    n->prev->next = n->next;
    n->next->prev = n->prev;
    n->next = 0;
    n->prev = 0;
    return tmp;
}

void
nvaosQueueRemove( ThreadQueueNode *n )
{
    NV_ASSERT( n );
    NV_ASSERT( n->next != n );
    NV_ASSERT( n->prev != n );
    NV_ASSERT( n->thread->magic == NVAOS_MAGIC );

    n->prev->next = n->next;
    n->next->prev = n->prev;
    n->next = 0;
    n->prev = 0;
}

void
nvaosQueueInit( ThreadQueue *q )
{
    NV_ASSERT( q );

    q->head.thread = 0;
    q->tail.thread = 0;

    q->head.next = &q->tail;
    q->head.prev = 0;
    q->tail.prev = &q->head;
    q->tail.next = 0;
}

#if NV_DEBUG
static void nvaosVerifyQueue( NvAosThreadState state, ThreadQueue *q )
{
    ThreadQueueNode *n;
    NvOsThreadHandle t;

    n = q->head.next;
    while( n != &q->tail )
    {
        t = n->thread;
        NV_ASSERT( t->state == state );
        NV_ASSERT( t->magic == NVAOS_MAGIC );

        n = n->next;
    }
}
#endif

void
nvaosContextSwitchHook( NvOsThreadHandle t )
{
#if NV_DEBUG
    g_ContextSwitches++;
#endif
}

/**
 * assumes interrupts are disabled.
 */
void
nvaosScheduler( void )
{
    static NvU32 s_IdleTicks = 0;
    NvOsThreadHandle t;
    t = g_CurrentThread;

    NV_ASSERT( t->magic == NVAOS_MAGIC );

    NV_DEBUG_CODE(
        nvaosVerifyQueue( NvAosThreadState_Running, &g_Runnable ) );
    NV_DEBUG_CODE(
        nvaosVerifyQueue( NvAosThreadState_Waiting, &g_Wait ) );
    NV_DEBUG_CODE(
        nvaosVerifyQueue( NvAosThreadState_WaitingTimeout, &g_WaitTimeout ) );

    NV_ASSERT( t->sched_queue.next == 0 &&
               t->sched_queue.prev == 0 );

    /* put the current thread back on a thread queue, if the current thread
     * is not the background thread.
     */
    if( t != &g_BackgroundThread )
    {
        switch( t->state ) {
        case NvAosThreadState_Running:
            nvaosEnqueue( &t->sched_queue, &g_Runnable, NV_TRUE );
            break;
        case NvAosThreadState_Waiting:
            nvaosEnqueue( &t->sched_queue, &g_Wait, NV_TRUE );
            break;
        case NvAosThreadState_WaitingTimeout:
            nvaosEnqueue( &t->sched_queue, &g_WaitTimeout, NV_TRUE );
            break;
        case NvAosThreadState_Stopped:
            /* don't do anything */
            break;
        default:
            NV_ASSERT( !"bad thread state" );
            return;
        }
    }

    /* check for timed-out threads */
    nvaosTimeout();

    /* switch the threads */
    t = nvaosDequeue( &g_Runnable );
    if( !t )
    {
        s_IdleTicks++;

        if( s_IdleTicks >= NVAOS_IDLE_THRESH )
        {
            g_SystemState = NvAosSystemState_Idle;
            s_IdleTicks = 0;
        }

        t = &g_BackgroundThread;
    }
    else
    {
        s_IdleTicks = 0;
    }

#if NV_DEBUG
    if( ( t != g_CurrentThread ) &&
        ( t != &g_BackgroundThread ) &&
        ( g_CurrentThread != &g_BackgroundThread ) )
    {
        nvaosContextSwitchHook( t );
    }
#endif

    NV_ASSERT( t );
    NV_ASSERT( t->magic == NVAOS_MAGIC );

    g_CurrentThread = t;

    /* either the interrupt handler or nvaosThreadSwitch will swap the
     * thread state after this.
     */
}

/* assumes interrupts are disabled.
 * returns NV_TRUE if the scheduler needs to run.
 */
static NvBool
nvaosResumeFromSync( NvOsThreadHandle t )
{
    NvBool ret;

    NV_ASSERT( t );
    NV_ASSERT( t->sched_queue.next == 0 &&
               t->sched_queue.prev == 0 );
    NV_ASSERT( t->state != NvAosThreadState_Running );
    NV_ASSERT( t->magic == NVAOS_MAGIC );

    /* if the runnable queue is empty, need to run the scheduler */
    ret = ( g_Runnable.head.next == &g_Runnable.tail );

    t->state = NvAosThreadState_Running;
    /* add to the head of the queue */
    nvaosEnqueue( &t->sched_queue, &g_Runnable, NV_FALSE );

    NV_DEBUG_CODE(
        nvaosVerifyQueue( NvAosThreadState_Running, &g_Runnable ) );

    return ret;
}

/**
 * handles threads with timeouts. interrupts must be disabled.
 */
void
nvaosTimeout( void )
{
    ThreadQueueNode *n;
    NvOsThreadHandle t;
    NvU64 time;

    time = nvaosGetTimeUS();

    n = g_WaitTimeout.head.next;
    while( n != &g_WaitTimeout.tail )
    {
        t = n->thread;
        NV_ASSERT( t->state == NvAosThreadState_WaitingTimeout );

        if( time - t->timeout_base >= (NvU64)t->timeout )
        {
            ThreadQueueNode *tmp;

            /* remove the thread from the wait-timeout queue */
            tmp = n->next;
            nvaosQueueRemove( n );
            n = tmp;

            NV_ASSERT( t->sched_queue.next == 0 &&
                       t->sched_queue.prev == 0 );

            /* set the expired flags and resume the thread */
            t->time_expired = NV_TRUE;
            t->timeout = 0;
            t->timeout_base = 0;

            /* handle re-scheduling, sync object clean up, etc. */
            if( t->sync_queue.next )
            {
                /* might just be sleeping (no sync object) */
                nvaosQueueRemove( &t->sync_queue );
            }

            (void)nvaosResumeFromSync( t );

            NV_ASSERT( t->sched_queue.next != 0 &&
                       t->sched_queue.prev != 0 );
            NV_ASSERT( t->state == NvAosThreadState_Running );
        }
        else
        {
            n = n->next;
        }
    }
}

/* called by exiting threads */
void
nvaosThreadKill( void )
{
    NvU32 i;

    for (i=0; i<g_CurrentThread->num_terminators; i++)
    {
        void *ctx = g_CurrentThread->TlsTerminatorData[i];
        g_CurrentThread->TlsTerminators[i](ctx);
    }
    /* prevent this thread from running again - switch to the next runnable
     * thread
     */
    nvaosThreadYield( NvAosThreadState_Stopped );
    NV_ASSERT( !"stopped thread is running!" );
}

void
nvaosThreadYield( NvAosThreadState state )
{
    nvaosDisableInterrupts();
    NV_ASSERT( g_CurrentThread->state == NvAosThreadState_Running );
    g_CurrentThread->state = state;

    /* for the threads waiting to join */
    if( state == NvAosThreadState_Stopped )
    {
        NvOsSemaphoreSignalInternal( &g_CurrentThread->join_sem );
    }

    /* switches to the next runnable thread and reenables interrupts */
    nvaosThreadSwitch();
}

void
nvaosThreadSleep( NvU32 msec )
{
    (void)nvaosDisableInterrupts();

    g_CurrentThread->state = NvAosThreadState_WaitingTimeout;
    g_CurrentThread->timeout_base = nvaosGetTimeUS();
    g_CurrentThread->timeout = msec * 1000;

    nvaosThreadSwitch();

    g_CurrentThread->timeout = 0;
    g_CurrentThread->timeout_base = 0;
    g_CurrentThread->time_expired = NV_FALSE;
}

void
nvaosMutexInit( NvOsMutexHandle mutex )
{
    nvaosQueueInit( &mutex->wait_queue );
    mutex->owner = 0;
    mutex->count = 0;
    mutex->refcount = 1;
}

void
nvaosMutexLock( NvOsMutexHandle mutex )
{
    NvU32 state;

    state = nvaosDisableInterrupts();

    if( mutex->owner == 0 )
    {
        mutex->owner = g_CurrentThread;
        mutex->count = 1;
        nvaosRestoreInterrupts(state);
    }
    else if( mutex->owner == g_CurrentThread )
    {
        mutex->count++;
        nvaosRestoreInterrupts(state);
    }
    else
    {
        nvaosEnqueue( &g_CurrentThread->sync_queue, &mutex->wait_queue,
            NV_TRUE );
        g_CurrentThread->state = NvAosThreadState_Waiting;

        /* waits for the next thread and reenables interrupts */
        nvaosThreadSwitch();

        /* when this returns, the thread has taken ownership of the mutex */
    }
}

void
nvaosMutexUnlock( NvOsMutexHandle mutex )
{
    NvOsThreadHandle t;
    NvU32 state;

    state = nvaosDisableInterrupts();

    NV_ASSERT( mutex->owner == g_CurrentThread );
    NV_ASSERT( mutex->count != 0 );

    mutex->count--;
    if( !mutex->count )
    {
        /* give the lock to the next waiter */
        t = nvaosDequeue( &mutex->wait_queue );
        if( t )
        {
            mutex->owner = t;
            mutex->count = 1;
            nvaosQueueRemove( &t->sched_queue );
            (void)nvaosResumeFromSync( t );
        }
        else
        {
            mutex->owner = 0;
        }
    }

    nvaosRestoreInterrupts(state);
}

void
nvaosSemaphoreInit( NvOsSemaphoreHandle sem, NvU32 count )
{
    nvaosQueueInit( &sem->wait_queue );
    sem->count = count;
    sem->refcount = 1;
    sem->sig_time = 0;
    sem->wait_time = 0;
    sem->resume_time = 0;
    sem->wait_context = 0;
    sem->resume_context = 0;
}

NvError
nvaosSemaphoreWait( NvOsSemaphoreHandle sem, NvU32 msec )
{
    NvError err = NvSuccess;
    NvU32 state;

    NV_ASSERT( sem );

    state = nvaosDisableInterrupts();

    NV_ASSERT( sem->count != (NvU32)-1 );

    /* if the count is > 0, then decrement the count and return */
    if( sem->count )
    {
        sem->count--;
        nvaosRestoreInterrupts(state);
        return NvSuccess;
    }

    if( msec == NV_WAIT_INFINITE )
    {
        g_CurrentThread->state = NvAosThreadState_Waiting;
    }
    else
    {
        if( msec == 0 && sem->count == 0 )
        {
            /* fail with timeout immediately */
            g_CurrentThread->timeout = 0;
            g_CurrentThread->timeout_base = 0;
            g_CurrentThread->time_expired = NV_FALSE;
            err = NvError_Timeout;
            nvaosRestoreInterrupts(state);
            return err;
        }

        g_CurrentThread->state = NvAosThreadState_WaitingTimeout;
        g_CurrentThread->timeout_base = nvaosGetTimeUS();
        g_CurrentThread->timeout = msec * 1000;
    }

    /* add the thread to the wait list */
    nvaosEnqueue( &g_CurrentThread->sync_queue, &sem->wait_queue, NV_TRUE );

    /* set the sig/wait time for debug */
    sem->wait_time = nvaosGetTimeUS();
#if NV_DEBUG
    sem->wait_context = g_ContextSwitches;
#endif

    /* when this returns, the semaphore has been signaled and interrupts
     * are enabled
     */

    nvaosThreadSwitch();

    NV_ASSERT( sem->count != (NvU32)-1 );

    /* check for expired timeout and cleanup */
    state = nvaosDisableInterrupts();
    if( msec != NV_WAIT_INFINITE )
    {
        g_CurrentThread->timeout = 0;
        g_CurrentThread->timeout_base = 0;

        if( g_CurrentThread->time_expired )
        {
            g_CurrentThread->time_expired = NV_FALSE;
            err = NvError_Timeout;
        }
    }

    /* thread is running */
    sem->resume_time = nvaosGetTimeUS();
    nvaosRestoreInterrupts(state);
#if NV_DEBUG
    sem->resume_context = g_ContextSwitches;
#endif

    return err;
}

/* this may be called from the isr -- don't touch the interrupt enable bit
 * in that case.
 */
void
nvaosSemaphoreSignal( NvOsSemaphoreHandle sem )
{
    NvBool incr = NV_TRUE;
    NvU32 state = 0;

    NV_ASSERT( sem );

    /* lock semaphore */
    if( !nvaosIsIsr() )
    {
        state = nvaosDisableInterrupts();
    }

    NV_ASSERT( sem->count != (NvU32)-1 );

    /* signal the first waiting thread if count is zero */
    if( sem->count == 0 )
    {
        NvOsThreadHandle t = nvaosDequeue( &sem->wait_queue );
        if( t )
        {
            NvBool run_sched;

            NV_ASSERT( t->state == NvAosThreadState_Waiting ||
                       t->state == NvAosThreadState_WaitingTimeout );

            nvaosQueueRemove( &t->sched_queue );
            run_sched = nvaosResumeFromSync( t );
            if( nvaosIsIsr() && run_sched )
            {
                nvaosScheduler();
            }

            /* don't increment if a thread was signaled */
            incr = NV_FALSE;
        }
    }

    if( incr )
    {
        /* increment the semahpore value */
        sem->count++;
    }

    /* set the signal time for debugging/profiling */
    sem->sig_time = nvaosGetTimeUS();

    /* unlock semaphore */
    if( !nvaosIsIsr() )
    {
        nvaosRestoreInterrupts(state);
    }
}

NvU32 nvaosTlsAlloc(void)
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
        if (NvOsAtomicCompareExchange32(
                            (NvS32*)&s_TlsUsedMask,
                            (NvS32)mask,
                            (NvS32)mask2) == (NvS32)mask)
            return idx;
    }
}

void nvaosTlsFree(NvU32 TlsIndex)
{
    NV_ASSERT(TlsIndex < NVOS_TLS_CNT || TlsIndex == NVOS_INVALID_TLS_INDEX);
    if (TlsIndex >= NVOS_TLS_CNT)
        return;
    NV_ASSERT(s_TlsUsedMask & (1<<TlsIndex));
    for (;;)
    {
        NvU32 mask = s_TlsUsedMask;
        NvU32 mask2 = mask & ~(1<<TlsIndex);
        if (NvOsAtomicCompareExchange32(
                            (NvS32*)&s_TlsUsedMask,
                            (NvS32)mask,
                            (NvS32)mask2) == (NvS32)mask)
            return;
    }
}

NvError nvaosTlsAddTerminator(void (*func)(void *), void *ctx)
{
    NvU32 num = g_CurrentThread->num_terminators;

    if (num >= NVAOS_MAX_TLS_TERMINATORS)
        return NvError_InsufficientMemory;

    g_CurrentThread->TlsTerminators[num] = func;
    g_CurrentThread->TlsTerminatorData[num] = ctx;
    g_CurrentThread->num_terminators++;

    return NvSuccess;
}

NvBool nvaosTlsRemoveTerminator(void (*func)(void *), void *ctx)
{
    NvU32 i,j;
    NvU32 num = g_CurrentThread->num_terminators;

    for (i = 0; i < num; i++)
    {
        if (g_CurrentThread->TlsTerminators[i] == func &&
            g_CurrentThread->TlsTerminatorData[i] == ctx)
        {
            //Move all subsequent terminators down to fill the gap
            for (j = i; j < num-1; j++)
            {
                g_CurrentThread->TlsTerminators[j] =
                    g_CurrentThread->TlsTerminators[j+1];
                g_CurrentThread->TlsTerminatorData[j] =
                    g_CurrentThread->TlsTerminatorData[j+1];
            }
            g_CurrentThread->num_terminators--;
            return NV_TRUE;
        }
    }
    return NV_FALSE;
}

void **nvaosTlsAddr(NvU32 TlsIndex)
{
    NV_ASSERT(s_TlsUsedMask & (1<<TlsIndex));
    if (TlsIndex >= NVOS_TLS_CNT)
        return NULL;
    return &(g_CurrentThread->Tls[TlsIndex]);
}

void *nvaosTlsGet(NvU32 TlsIndex)
{
    NV_ASSERT(s_TlsUsedMask & (1<<TlsIndex));
    if (TlsIndex >= NVOS_TLS_CNT)
        return NULL;
    return g_CurrentThread->Tls[TlsIndex];
}

void nvaosTlsSet(NvU32 TlsIndex, void *Value)
{
    NV_ASSERT(s_TlsUsedMask & (1<<TlsIndex));
    if (TlsIndex >= NVOS_TLS_CNT)
        return;
    g_CurrentThread->Tls[TlsIndex] = Value;
}

void
nvaosThreadInit( NvOsThreadHandle t )
{
    nvaosSemaphoreInit( &t->join_sem, 0 );

    /* setup the thread queues */
    t->sync_queue.thread = t;
    t->sync_queue.next = 0;
    t->sync_queue.prev = 0;
    t->sched_queue.thread = t;
    t->sched_queue.next = 0;
    t->sched_queue.prev = 0;

#if NV_DEBUG
    t->global_queue.next = 0;
    t->global_queue.prev = 0;
    t->global_queue.thread = t;
#endif

    /* clear the timeout state */
    t->timeout = 0;
    t->timeout_base = 0;
    t->time_expired = NV_FALSE;

    t->num_terminators = 0;

    NvOsMemset(t->Tls, 0, sizeof(t->Tls));
    NvOsMemset(t->TlsTerminators, 0, sizeof(t->TlsTerminators));
    NvOsMemset(t->TlsTerminatorData, 0, sizeof(t->TlsTerminatorData));

    t->state = NvAosThreadState_Stopped;
}

void nvaosLog( const char *format, ... )
{
#if NV_DEBUG
    va_list ap;
    char *log;

    va_start( ap, format );

    log = s_log[s_log_index];
    s_log_index = ( s_log_index + 1 ) & (NVAOS_LOG_ENTRIES - 1);

    NvOsVsnprintf( log, NVAOS_LOG_SIZE, format, ap );

    va_end( ap );
#endif
}

// FIXME: need an environment
char *
nvaosGetenv( const char *env )
{
    NvAosEnv *e;

    if( !g_Env )
    {
        return 0;
    }

    e = g_Env;
    while( e )
    {
        if( NvOsStrcmp( e->name, env ) == 0 )
        {
            return e->value;
        }

        e = e->next;
    }

    return 0;
}

/** AOS boot */
#define NVAOS_PROFILE 0

#if NVAOS_SHELL == 0
/* main2() is in libnvappmain, which will call NvAppMain() */
extern int main2( int argc, char *argv[] );
#else
/* AOS has been compiled against the AVP DDK */
// FIXME: clean up the avp ddk api
#include "nvddk_avp.h"
#endif

void
main_thread( void *arg )
{
    int argc = 0;
    char **argv = NULL;
    char *foo[1];
    NvBool bCmdLine = NV_FALSE;
#if NVAOS_PROFILE
    NvU32 i, tmp;
    void *apertures[NVOS_MAX_PROFILE_APERTURES];
    NvU32 sizes[NVOS_MAX_PROFILE_APERTURES];
#endif

    /* timer init */
    nvaosTimerInit();
    /* semihost setup */
    nvaosSemiSetup();
    if (argc)
        NvOsDebugPrintf("WARNING:: Wrong pinmux init in bootloader\n");
    /* perform final setup (any setup that may sleep) */
    nvaosFinalSetup();

    NvOsDebugPrintf("\nBootloader-Cpu Init at (time stamp): %d us\n\n", g_BlCpuTimeStamp);
    /* BUG 597833: This is just a WAR to make AOS standalone tests
     * work again. By enabling interrupts after nvaosFinalSetup,
     * the interrupt handler will not kick in before module table
     * is initialized.
     *
     * The bug needs to be fixed properly in nvintr.
     */
    /* finally allow the scheduler to run */
    //Please do not remove the below print used for tracking boot time

    NvOsDebugPrintf("Pinmux changes applied in kernel way\n");

    nvaosEnableInterrupts();
    /* get command line for application */
    argc = 0;
#if !__GNUC__
    nvaosGetCommandLine( &argc, NULL );
    if( argc)
    {
        argv = (char **)NvOsAlloc(argc * sizeof(char*));
        NV_ASSERT( argv );
        nvaosGetCommandLine( &argc, argv );
        bCmdLine = NV_TRUE;
    }
#endif
    if (!argc || !argv)
    {
        foo[0] = 0;
        argc = 0;
        argv = foo;
    }

#if NVAOS_PROFILE
    NvOsProfileApertureSizes( &tmp, sizes );
    for( i = 0; i < tmp; i++ )
    {
        apertures[i] = NvOsAlloc( sizes[i] );
        NV_ASSERT( apertures[i] );
    }

    NvOsProfileStart( (void **)apertures );
#endif

#if NVAOS_SHELL == 0
    /* run the application */
    (void)main2( argc, argv );
#else
    // FIXME: remove the rest of the functions in nvddk_avp.h
    /* call the DDK AVP shell thread */

    // TODO: ShellThread calls NvRmOpen...but this *must* be done before
    // interrupts are enabled.
    NvOsAvpShellThread( argc, argv );
#endif

#if NVAOS_PROFILE
    NvOsProfileStop( (void **)apertures );
    for( i = 0; i < tmp; i++ )
    {
        NvError err;
        NvOsFileHandle file;
        char name[NVOS_PATH_MAX];

        NvOsSnprintf( name, sizeof(name), "gmon%d.out", i );
        err = NvOsFopen( name, NVOS_OPEN_CREATE | NVOS_OPEN_WRITE, &file );
        NV_ASSERT( err == NvSuccess );

        (void)NvOsProfileWrite( file, i, apertures[i] );
    }
#endif

    if( bCmdLine )
    {
        NvOsFree(argv);
    }

#ifdef BUILD_FOR_COSIM
    /* Notify the simulator that the application runs to the end.
       Simulators can be shutdown.
    */
    NV_WRITE32(DRF_BASE(NV_ASIM)+NV_ASIM_SHUTDOWN,
               NV_ASIM_SHUTDOWN_SHUTDOWN_ENABLE);
#endif

    /* prevent the pc from running off into the weeds */
    nvaosDisableInterrupts();
    for( ;; )
    {
        nvaosWaitForInterrupt();
    }
}

void
background_thread( void *arg );

/* background thread - waits for an interrupt.
 */
void
background_thread( void *arg )
{
    for( ;; )
    {
        nvaosWaitForInterrupt();
    }
}

void
nvaosSetupThreads( void )
{
    g_SystemState = NvAosSystemState_Running;

    /* init thread queues */
    nvaosQueueInit( &g_Runnable );
    nvaosQueueInit( &g_Wait );
    nvaosQueueInit( &g_WaitTimeout );

#if NV_DEBUG
    nvaosQueueInit( &g_Threads );
#endif

    /* background thread setup -- never enqueue the background thread into
     * the thread queues.
     */
    nvaosThreadInit( &g_BackgroundThread );
    g_BackgroundThread.id = 1;
    g_BackgroundThread.wrap = 0;
    g_BackgroundThread.function = background_thread;
    g_BackgroundThread.args = 0;
    g_BackgroundThread.state = NvAosThreadState_Running;
    g_BackgroundThread.magic = NVAOS_MAGIC;
    nvaosThreadStackInit( &g_BackgroundThread, (NvU32 *)background_stack,
        sizeof(background_stack) / 4, NV_FALSE );

    /* main thread setup */
    nvaosThreadInit( &g_MainThread );
    g_MainThread.id = 0;
    g_MainThread.wrap = 0;
    g_MainThread.function = main_thread;
    g_MainThread.args = 0;
    g_MainThread.state = NvAosThreadState_Running;
    g_MainThread.magic = NVAOS_MAGIC;
    g_CurrentThread = &g_MainThread;
    nvaosThreadStackInit( &g_MainThread, (NvU32 *)main_stack,
        sizeof(main_stack) / 4, NV_TRUE );

#if NV_DEBUG
    nvaosEnqueue( &g_MainThread.global_queue, &g_Threads, NV_TRUE );
#endif
}

void
nvaosMain( void )
{
    /* post initialization: setup interrupt handlers, etc. */
    nvaosCpuPostInit();

    /* configure the allocator */
    nvaosAllocatorSetup();

    /* configure threads -- executes the main thread (doesn't return) */
    nvaosSetupThreads();
}

void
nvaosInit( void )
{
    s_BlStart = (NvU32)NvOsGetTimeUS();
    /* cpu setup */
    nvaosCpuPreInit();

    /* page table init */
    nvaosPageTableInit();

    /* interrupts should stay disabled until main_thread is active */
    s_BlEnd = (NvU32)NvOsGetTimeUS();
}

/* called by __scatterload/__rt_entry */
int main( int argc, char *argv[] )
{
    /* disable interrupts */
    nvaosDisableInterrupts();

    nvaosMain();
    return 0;
}
