/*
 * Copyright (c) 2007 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVOS_COOP_THREAD_H
#define INCLUDED_NVOS_COOP_THREAD_H

#include "nvcommon.h"
#include "nvos.h"

typedef enum
{
    CoopThreadState_Running = 1,
    CoopThreadState_Waiting,
    CoopThreadState_WaitingTimeout,
    CoopThreadState_Stopped,

    CoopThreadState_Num,
    CoopThreadState_Force32 = 0x7FFFFFFF,
} CoopThreadState;

typedef enum
{
    CoopThreadPriority_Normal = 1,
    CoopThreadPriority_Low = 2,

    CoopThreadPriority_Num,
    CoopThreadPriority_Force32 = 0x7FFFFFFF,
} CoopThreadPriority;

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
    char key[NVOS_KEY_MAX];
} NvOsSemaphore;

typedef struct NvOsMutexRec
{
    NvOsThreadHandle owner;
    NvU32 count;
    ThreadQueue wait_queue;
    NvU32 refcount;
    char key[NVOS_KEY_MAX];
} NvOsMutex;

typedef struct NvOsThreadRec
{
    /* stuff for the pre-emptive thread package */
    NvOsThreadHandle thread;
    NvOsSemaphoreHandle sem;
    NvOsThreadFunction function;
    void *args;

    /* coop thread state, etc. */
    CoopThreadState state;
    CoopThreadPriority priority;
    NvU32 timeout; /* in milliseconds */
    NvBool time_expired;
    NvOsSemaphore join_sem;

    /* Thread Local Storage */
    void *Tls[NVOS_TLS_CNT];

    /* for synch object wait lists */
    ThreadQueueNode sync_queue;

    /* thread scheduler queue */
    ThreadQueueNode sched_queue;
} NvOsThread;

void CoopEnqueue( ThreadQueueNode *n, ThreadQueue *q );
NvOsThreadHandle CoopDequeue( ThreadQueue *q );
void CoopQueueInit( ThreadQueue *q );
void CoopQueueRemove( ThreadQueueNode *n );

void CoopThreadSwitch(void);
void CoopThreadTimeout(void);
void CoopSemaphoreInit(NvOsSemaphore *sem, NvU32 value);
NvError CoopThreadCreate(NvOsThreadFunction function, 
    void *args, NvOsThreadHandle *thread);
NvError CoopThreadSetLowPriority(void);
void CoopThreadYield(void);
void CoopThreadJoin(NvOsThreadHandle thread);
NvError CoopSemaphoreCreate(NvOsSemaphoreHandle *semaphore, NvU32 value);
NvError CoopSemaphoreWait(NvOsSemaphoreHandle semaphore, NvU32 TimeInMS);
void CoopSemaphoreSignal(NvOsSemaphoreHandle semphore);
NvError CoopSemaphoreClone(NvOsSemaphoreHandle orig,
    NvOsSemaphoreHandle *clone);
void CoopSemaphoreDestroy(NvOsSemaphoreHandle semaphore);
NvError CoopMutexCreate(const char *key, NvOsMutexHandle *mutex);
void CoopMutexLock(NvOsMutexHandle mutex);
void CoopMutexUnlock(NvOsMutexHandle mutex);
void CoopMutexDestroy(NvOsMutexHandle mutex);
NvU32 CoopTlsAlloc(void);
void CoopTlsFree(NvU32 TlsIndex);
void *CoopTlsGet(NvU32 TlsIndex);
void CoopTlsSet(NvU32 TlsIndex, void *Value);
NvError CoopTlsAddTerminator(void (*func)(void *), void *context);
NvBool CoopTlsRemoveTerminator(void (*func)(void *), void *context);
void CoopSleepMS(NvU32 msec);

#endif // INCLUDED_NVOS_COOP_THREAD_H
