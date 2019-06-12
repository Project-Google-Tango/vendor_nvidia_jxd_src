/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_AOS_H
#define INCLUDED_AOS_H

#include "nvcommon.h"
#include "nvos.h"
#include "dlmalloc.h"
#include "aos_process_args.h"
#include "nvodm_query.h"

#define AOS_REGW(a,d)   *((volatile NvU32 *)(a)) = (d)
#define AOS_REGR(a)     *((const volatile NvU32 *)(a))

#define AOS_REGW08(a,d)   *((volatile NvU8 *)(a)) = (d)
#define AOS_REGR08(a)     *((const volatile NvU8 *)(a))

#define NVAOS_MAGIC (0xdecafbad)
#define NVAOS_IDLE_THRESH 150

#define NVAOS_MAX_HANDLERS 4

#define NVAOS_USE_SEMI_CONFIG 1
#define NVAOS_SUSPEND_ENABLE 1

#define NVAOS_MAX_TLS_TERMINATORS 4

typedef enum
{
    NvAosThreadState_Stopped = 1,
    NvAosThreadState_Running,
    NvAosThreadState_Waiting,
    NvAosThreadState_WaitingTimeout,

    NvAosThreadState_Num,
    NvAosThreadState_Force32 = 0x7FFFFFFF,
} NvAosThreadState;

typedef enum
{
    NvAosHeap_Internal = 1,
    NvAosHeap_External = 2,

    NvAosHeap_Force32 = 0x7FFFFFFF,
} NvAosHeap;

typedef enum
{
    NvAosChip_T30 = 4,
    NvAosChip_T114,
    NvAosChip_T124,

    NvAosChip_Force32 = 0x7FFFFFFF,
} NvAosChip;

typedef enum
{
    NvAosSystemState_Running = 1,
    NvAosSystemState_Idle,
    NvAosSystemState_Suspend,

    NvAosSystemState_Force32 = 0x7FFFFFFF,
} NvAosSystemState;

typedef struct MemoryMapRec
{
    // main external memory (might need other memory pools at some point)
    NvUPtr ext_start;
    NvUPtr ext_end;

    NvUPtr mmio_start;
    NvUPtr mmio_end;
} MemoryMap;

typedef struct AllocatorRec
{
    NvU8 *base;
    NvU32 size;
    mspace mspace;
} Allocator;

typedef struct NvOsFileRec
{
    int fd;
} NvOsFile;

typedef void (*ThreadFunction)( void *arg );

typedef struct ThreadQueueNodeRec
{
    struct NvOsThreadRec *thread;
    struct ThreadQueueNodeRec *next;
    struct ThreadQueueNodeRec *prev;
} ThreadQueueNode;

typedef struct ThreadQueueRec
{
    ThreadQueueNode head;
    ThreadQueueNode tail;
} ThreadQueue;

typedef struct NvOsSemaphoreRec
{
    NvU32 count;
    ThreadQueue wait_queue;
    NvU32 refcount;

    /* time in microseconds */
    NvU32 wait_time; // last time a thread started waiting
    NvU32 sig_time; // last time the semaphore was signaled
    NvU32 resume_time; // when the thread is back on the cpu

    /* number of context switches while waiting */
    NvU32 wait_context;
    NvU32 resume_context;
} NvOsSemaphore;

typedef struct NvOsMutexRec
{
    NvOsThreadHandle owner;
    NvU32 count;
    ThreadQueue wait_queue;
    NvU32 refcount;
} NvOsMutex;

typedef struct NvOsIntrMutexRec
{
    NvU32 lock;
    NvU32 SaveIntState;
} NvOsIntrMutex;

typedef struct NvOsThreadRec
{
    NvU32 sp;
    unsigned char *stack;
    NvAosThreadState state;
    NvU32 id;
    ThreadFunction wrap;
    ThreadFunction function;
    void *args;
    NvOsSemaphore join_sem;

    /* Thread Local Storage */
    void *Tls[NVOS_TLS_CNT];
    void (*TlsTerminators[NVAOS_MAX_TLS_TERMINATORS])(void *);
    void *TlsTerminatorData[NVAOS_MAX_TLS_TERMINATORS];

    NvU32 num_terminators;

    /* timeout support, in microseconds */
    NvU64 timeout_base; // time waiting started
    NvU32 timeout; // amount of time to wait

    /* will be set if the thread timed out */
    NvBool time_expired;

    /* for synch object wait lists */
    ThreadQueueNode sync_queue;

    /* thread scheduler queue */
    ThreadQueueNode sched_queue;

#if NV_DEBUG
    /* keep track of all threads in the system */
    ThreadQueueNode global_queue;
#endif

    /* should always be set to 0xdecafbad */
    NvU32 magic;
} NvOsThread;

typedef struct NvAosEnvRec
{
    char *name;
    char *value;
    struct NvAosEnvRec *next;
} NvAosEnv;

extern ThreadQueue g_Runnable;
extern ThreadQueue g_Wait;
extern ThreadQueue g_WaitTimeout;
extern NvOsThreadHandle g_CurrentThread;
extern NvOsThread g_MainThread;
extern NvOsThread g_BackgroundThread;
extern NvAosEnv *g_Env;
extern NvAosSystemState g_SystemState;

#if NV_DEBUG
extern ThreadQueue g_Threads;
extern NvU32 g_ContextSwitches;
#endif

void nvaosEnqueue( ThreadQueueNode *n, ThreadQueue *q, NvBool tail );
NvOsThreadHandle nvaosDequeue( ThreadQueue *q );
void nvaosQueueInit( ThreadQueue *q );
void nvaosQueueRemove( ThreadQueueNode *n );
void nvaosContextSwitchHook( NvOsThreadHandle t );

void nvaosScheduler( void );
void nvaosTimeout( void );
void nvaosMutexInit( NvOsMutexHandle mutex );
void nvaosMutexLock( NvOsMutexHandle mutex );
void nvaosMutexUnlock( NvOsMutexHandle mutex );
void nvaosSemaphoreInit( NvOsSemaphoreHandle sem, NvU32 count );
NvError nvaosSemaphoreWait( NvOsSemaphoreHandle sem, NvU32 msec );
void nvaosSemaphoreSignal( NvOsSemaphoreHandle sem );
NvU32 nvaosTlsAlloc(void);
void nvaosTlsFree(NvU32 TlsIndex);
void **nvaosTlsAddr(NvU32 TlsIndex);
void *nvaosTlsGet(NvU32 TlsIndex);
void nvaosTlsSet(NvU32 TlsIndex, void *Value);
NvError nvaosTlsAddTerminator(void (*term)(void *), void *ctx);
NvBool nvaosTlsRemoveTerminator(void (*term)(void *), void *ctx);
void nvaosThreadInit( NvOsThreadHandle t );
void nvaosThreadYield( NvAosThreadState state );
void nvaosThreadKill( void );
void nvaosThreadSwitch( void );
void nvaosThreadSleep( NvU32 msec );
void nvaosInit( void );
void nvaosMain( void );
void main_thread( void *arg );
char *nvaosGetenv( const char *env );
NV_WEAK void nvaosGetCommandLine( int *argc, char **argv );
void nvaosProfTimerHandler( void );

void nvaosSemiSetup( void );
NvAosSemihostStyle nvaosGetSemihostStyle( void );

#define NVAOS_LOG_ENTRIES 8
#define NVAOS_LOG_SIZE 128
void nvaosLog( const char *format, ... );
int nvaosSimpleVsnprintf( char* b, size_t size, const char* f, va_list ap );

/* per cpu */
void nvaosCpuPreInit( void );
void nvaosCpuPostInit( void );
void nvaosDecodeIrqAndDispatch( void );
void nvaosTimerInit( void );
void nvaosPageTableInit( void );
NvU32 nvaosIsIsr( void );
void nvaosEnableInterrupts( void );
void nvaosRestoreInterrupts( NvU32 state );
NvU32 nvaosDisableInterrupts( void );
void nvaosWaitForInterrupt( void );
void nvaosInterruptInstall( NvU32 id, NvOsInterruptHandler handler,
    void *context );
void nvaosInterruptUninstall( NvU32 id );
void nvaosThreadStackInit( NvOsThreadHandle t, NvU32 *stackBase, NvU32 words,
    NvBool exec );
void nvaosSetupThreads( void );
void nvaosAllocatorSetup( void );
void* nvaosAllocate( NvU32 size, NvU32 alignment, NvOsMemAttribute attrib,
    NvAosHeap heap );
void nvaosFinalSetup( void );
void *nvaosRealloc( void *ptr, NvU32 size );
void nvaosFree( void *ptr );
NvU64 nvaosGetTimeUS( void );
NvU32 nvaosGetExceptionAddr( void );
void nvaosSuspend( void );
NvU32 nvaosGetStackSize( void );
void nvaosGetMemoryMap( MemoryMap *map );
NvU32 nvaosReadPmcScratch( void );
NvS32 nvaosAtomicCompareExchange32( NvS32 *pTarget, NvS32 OldValue,
    NvS32 NewValue);
NvS32 nvaosAtomicExchange32( NvS32 *pTarget,  NvS32 Value);
NvS32 nvaosAtomicExchangeAdd32( NvS32 *pTarget, NvS32 Value);
void  nvaosEnableMMU(NvU32 TtbAddr);
void  nvaosHaltCpu(void);
void nvaos_ConfigureCpu(void);
void nvaos_ConfigureA15(void);
void nvaos_ConfigureInterruptController(void);
void nvaos_EnableTimer(void);
NvU32 *nvaos_GetFirstLevelPageTableAddr(void);

/**
 * @brief Uses ODM interafce to get debug console option
 *
 * @retval ::returns user selected odmoption for debug console
 */
NvOdmDebugConsole
aosGetOdmDebugConsole(void);

#endif
