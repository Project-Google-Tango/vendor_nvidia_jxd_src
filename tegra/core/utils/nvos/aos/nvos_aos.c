/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include "nvos.h"
#include "nvos_internal.h"
#include "nvassert.h"
#include "nvutil.h"
#include "aos_semihost.h"
#include "nvintr.h"
#include "aos.h"

#if NVOS_TRACE || NV_DEBUG
#undef NvOsAlloc
#undef NvOsRealloc
#undef NvOsFree
#undef NvOsExecAlloc
#undef NvOsExecFree
#undef NvOsSharedMemAlloc
#undef NvOsSharedMemHandleFromFd
#undef NvOsSharedMemGetFd
#undef NvOsSharedMemMap
#undef NvOsSharedMemUnmap
#undef NvOsSharedMemFree
#undef NvOsIntrMutexCreate
#undef NvOsIntrMutexLock
#undef NvOsIntrMutexUnlock
#undef NvOsIntrMutexDestroy
#undef NvOsSemaphoreUnmarshal
#undef NvOsInterruptRegister
#undef NvOsInterruptUnregister
#undef NvOsInterruptEnable
#undef NvOsInterruptDone
#undef NvOsPhysicalMemMapIntoCaller
#undef NvOsSemaphoreUnmarshal
#endif


/** NvOs impementation for AOS */
void
NvOsStrcpy( char *dest, const char *src )
{
    NV_ASSERT( dest );
    NV_ASSERT( src );

    (void)strcpy( dest, src );
}

void
NvOsStrncpy( char *dest, const char *src, size_t size )
{
    NV_ASSERT( dest );
    NV_ASSERT( src );

    (void)strncpy( dest, src, size );
}

size_t
NvOsStrlen( const char *s )
{
    NV_ASSERT( s );
    return strlen( s );
}

NvOsCodePage
NvOsStrGetSystemCodePage(void)
{
    return NvOsCodePage_Windows1252;
}

int
NvOsStrcmp( const char *s1, const char *s2 )
{
    NV_ASSERT( s1 );
    NV_ASSERT( s2 );

    return strcmp( s1, s2 );
}

int
NvOsStrncmp( const char *s1, const char *s2, size_t size )
{
    NV_ASSERT( s1 );
    NV_ASSERT( s2 );

    return strncmp( s1, s2, size );
}

#if !__GNUC__
/**
 * Bug 310180 and 317788 (arm's memcpy is broken)
 */
void *
$Sub$$__aeabi_memcpy( void *dst, const void *src, size_t size )
{
    NvU8 *d;
    NvU8 *s;

    NvU32 *d32 = (NvU32 *)dst;
    NvU32 *s32 = (NvU32 *)src;

    if ( (( (NvUPtr)src) & 0x3) == 0 &&
         (( (NvUPtr)dst) & 0x3) == 0 )
    {
        // it's aligned, do 16-bytes at a time (RVDS optimizes this as LDM/STM)
        while (size >= 16)
        {
            *d32++ = *s32++;
            *d32++ = *s32++;
            *d32++ = *s32++;
            *d32++ = *s32++;

            size = size - 16;
        }

        // do the trailing words
        while (size >= 4)
        {
            *d32++ = *s32++;
            size = size - 4;
        }
    }

    if (!size)
        return d32;

    /* if it's unaligned or there are trailing bytes to be copied, copy them
     * here
     */
    d = (NvU8 *)d32;
    s = (NvU8 *)s32;

    do
    {
        // copy any residual bytes
        *d++ = *s++;
        size = size - 1;

    } while( size );

    return d;
}

/* NOTE: be sure to not call the traced version of NvOsAlloc. */
void *
$Sub$$malloc(size_t numBytes)
{
    // NvOsAlloc asserts that size is not zero.  Make it happy.
    // Shaderfix actually callocs 0 members sometimes.
    size_t size = numBytes ? numBytes : 1;
    return (NvOsAlloc)(size);
}

void *
$Sub$$calloc(size_t nmemb, size_t numBytes)
{
    void *p;

    size_t size = nmemb * numBytes;
    size = size ? size : 1;
    p = (NvOsAlloc)(size);
    if( p )
    {
        NvOsMemset(p, 0, size);
    }
    return p;
}

void
$Sub$$free(void *p)
{
    (NvOsFree)(p);
}

void *
$Sub$$realloc(void *p, size_t numBytes)
{
    return (NvOsRealloc)(p, numBytes);
}
#endif

void NvOsMemcpy(void *dest, const void *src, size_t size)
{
    (void)memcpy(dest, src, size);
}

int
NvOsMemcmp( const void *s1, const void *s2, size_t size )
{
    NV_ASSERT( s1 );
    NV_ASSERT( s2 );

    return memcmp( s1, s2, size );
}

void
NvOsMemset( void *s, NvU8 c, size_t size )
{
    NV_ASSERT( s );

    (void)memset( s, (int)c, size );
}

void
NvOsMemmove( void *dest, const void *src, size_t size )
{
    if ((src >= dest) || (((NvU8 *)src + size) <= (NvU8 *)dest))
    {
        (void)memcpy(dest, src, size);
    }
    else
    {
        NvU8 *pSrc = (NvU8 *)src + size;
        NvU8 *pDst = (NvU8 *)dest + size;

        while (pDst > (NvU8 *)dest)
            *(--pDst) = *(--pSrc);
    }
}

NvError
NvOsCopyIn( void *pDst, const void *pSrc, size_t Bytes )
{
    NvOsMemcpy(pDst, pSrc, Bytes);
    return NvError_Success;
}

NvError
NvOsCopyOut( void *pDst, const void *pSrc, size_t Bytes )
{
    NvOsMemcpy(pDst, pSrc, Bytes);
    return NvError_Success;
}

NvError
NvOsFsyncInternal( NvOsFileHandle stream )
{
    return NvSuccess;
}

NvError
NvOsFremoveInternal( const char *filename )
{
    return NvSuccess;
}

NvError
NvOsFlockInternal( NvOsFileHandle stream, NvOsFlockType type )
{
    return NvSuccess;
}

NvError
NvOsFtruncateInternal( NvOsFileHandle stream, NvU64 length )
{
    return NvError_NotImplemented;
}

NvError
NvOsIoctl( NvOsFileHandle hFile, NvU32 IotclCode, void *pBuffer,
    NvU32 InBufferSize, NvU32 InOutBufferSize, NvU32 OutBufferSize )
{
    return NvError_NotImplemented;
}

void *NvOsAllocInternal(size_t size)
{
    return nvaosAllocate(size, 4, NvOsMemAttribute_WriteBack,
        NvAosHeap_External);
}

void *NvOsReallocInternal(void *ptr, size_t size)
{
    if( !ptr )
    {
        return NvOsAllocInternal( size );
    }
    if( !size )
    {
        NvOsFreeInternal( ptr );
        return 0;
    }

    return nvaosRealloc(ptr, size);
}

void NvOsFreeInternal(void *ptr)
{
    nvaosFree(ptr);
}

void *
NvOsExecAlloc( size_t size )
{
    return NULL;
}

void
NvOsExecFree( void *ptr, size_t size )
{
}

NvError
NvOsSharedMemAlloc( const char *key, size_t size,
    NvOsSharedMemHandle *descriptor )
{
    return NvError_NotImplemented;
}

NvError
NvOsSharedMemHandleFromFd( int fd,
    NvOsSharedMemHandle *descriptor )
{
    return NvError_NotImplemented;
}

NvError
NvOsSharedMemGetFd( NvOsSharedMemHandle descriptor,
    int *fd )
{
    return NvError_NotImplemented;
}

NvError
NvOsSharedMemMap( NvOsSharedMemHandle descriptor, size_t offset,
    size_t size, void **ptr )
{
    return NvError_NotImplemented;
}

void
NvOsSharedMemUnmap( void *ptr, size_t size )
{
}

void
NvOsSharedMemFree( NvOsSharedMemHandle descriptor )
{

}

NvError
NvOsLibraryLoad( const char *name, NvOsLibraryHandle *library )
{
    NV_ASSERT( name );
    NV_ASSERT( library );

    *library = 0;

    return NvError_NotImplemented;
}

void *
NvOsLibraryGetSymbol( NvOsLibraryHandle library, const char *symbol)
{
    NV_ASSERT( symbol );

    return NULL;
}

void
NvOsLibraryUnload( NvOsLibraryHandle library )
{
}

void
NvOsSleepMSInternal( NvU32 msec )
{
    nvaosThreadSleep( msec );
}

void
NvOsWaitUS( NvU32 usec )
{
    NvU64 t0;
    NvU64 t1;

    t0 = NvOsGetTimeUS();
    t1 = t0;

    // FIXME: it's possible that if we got preempted for a long time,
    // then t1 would wrap.
    while( (t1 - t0) < (NvU64)usec )
    {
        t1 = NvOsGetTimeUS();
    }
}

NvError
NvOsMutexCreateInternal( NvOsMutexHandle *mutex )
{
    NvOsMutexHandle m = 0;
    NvError ret = NvSuccess;

    NV_ASSERT( mutex );

    m = NvOsAlloc( sizeof(NvOsMutex) );
    if( !m )
    {
        ret = NvError_InsufficientMemory;
        goto fail;
    }

    nvaosMutexInit( m );

    *mutex = m;
    return NvSuccess;

fail:

    *mutex = 0;
    NvOsFree( m );
    return ret;
}

void
NvOsMutexLockInternal( NvOsMutexHandle m )
{
    NV_ASSERT( m );
    nvaosMutexLock( m );
}

void
NvOsMutexUnlockInternal( NvOsMutexHandle m )
{
    NV_ASSERT( m );
    nvaosMutexUnlock( m );
}

void
NvOsMutexDestroyInternal( NvOsMutexHandle mutex )
{
    NvBool bfree = NV_FALSE;
    NvU32 state;

    state = nvaosDisableInterrupts();

    mutex->refcount--;
    if( mutex->refcount == 0 )
    {
        bfree = NV_TRUE;
    }

    nvaosRestoreInterrupts(state);

    if( bfree )
    {
        NvOsFree( mutex );
    }
}

NvError
NvOsIntrMutexCreate( NvOsIntrMutexHandle *mutex )
{
    /* any non-zero value is good */
    *mutex = (NvOsIntrMutexHandle)1;
    return NvSuccess;
}

NvU32 g_IntState = 0;

void
NvOsIntrMutexLock( NvOsIntrMutexHandle mutex )
{
    g_IntState = nvaosDisableInterrupts();
}

void
NvOsIntrMutexUnlock( NvOsIntrMutexHandle mutex )
{
    nvaosRestoreInterrupts(g_IntState);
}

void
NvOsIntrMutexDestroy( NvOsIntrMutexHandle mutex )
{
}

NvError NvOsConditionCreate(NvOsConditionHandle *cond)
{
    return NvError_NotImplemented;
}

NvError NvOsConditionDestroy(NvOsConditionHandle cond)
{
    return NvError_NotImplemented;
}

NvError NvOsConditionBroadcast(NvOsConditionHandle cond)
{
    return NvError_NotImplemented;
}

NvError NvOsConditionSignal(NvOsConditionHandle cond)
{
    return NvError_NotImplemented;
}

NvError NvOsConditionWait(NvOsConditionHandle cond, NvOsMutexHandle mutex)
{
    return NvError_NotImplemented;
}

NvError NvOsConditionWaitTimeout(NvOsConditionHandle cond, NvOsMutexHandle mutex, NvU32 microsecs)
{
    return NvError_NotImplemented;
}

NvError NvOsSemaphoreCreateInternal(NvOsSemaphoreHandle *semaphore,
    NvU32 value)
{
    NvOsSemaphoreHandle s = NvOsAlloc(sizeof(NvOsSemaphore));
    if (!s)
    {
        return NvError_InsufficientMemory;
    }

    nvaosSemaphoreInit(s, value);
    *semaphore = s;
    return NvSuccess;
}

NvError
NvOsSemaphoreCloneInternal( NvOsSemaphoreHandle orig,
    NvOsSemaphoreHandle *semaphore )
{
    NvU32 state;

    NV_ASSERT( orig );
    NV_ASSERT( semaphore );

    state = nvaosDisableInterrupts();

    orig->refcount++;
    *semaphore = orig;

    nvaosRestoreInterrupts(state);

    return NvSuccess;
}

NvError
NvOsSemaphoreUnmarshal(
    NvOsSemaphoreHandle hClientSema,
    NvOsSemaphoreHandle *phDriverSema)
{
    return NvError_NotImplemented;
}

void
NvOsSemaphoreWaitInternal( NvOsSemaphoreHandle semaphore )
{
    (void)nvaosSemaphoreWait( semaphore, NV_WAIT_INFINITE );
}

NvError
NvOsSemaphoreWaitTimeoutInternal( NvOsSemaphoreHandle semaphore, NvU32 msec )
{
    return nvaosSemaphoreWait( semaphore, msec );
}

void
NvOsSemaphoreSignalInternal( NvOsSemaphoreHandle semaphore )
{
    NV_ASSERT( semaphore );
    nvaosSemaphoreSignal( semaphore );
}

void
NvOsSemaphoreDestroyInternal( NvOsSemaphoreHandle semaphore )
{
    NvBool bfree = NV_FALSE;
    NvU32 state;

    state = nvaosDisableInterrupts();

    semaphore->refcount--;
    if( semaphore->refcount == 0 )
    {
        bfree = NV_TRUE;
    }

    nvaosRestoreInterrupts(state);

    if( bfree )
    {
        NvOsFree( semaphore );
    }
}

NvU32 NvOsTlsAllocInternal(void)
{
    return nvaosTlsAlloc();
}

void NvOsTlsFreeInternal(NvU32 TlsIndex)
{
    nvaosTlsFree(TlsIndex);
}

void *NvOsTlsGetInternal(NvU32 TlsIndex)
{
    return nvaosTlsGet(TlsIndex);
}

void NvOsTlsSetInternal(NvU32 TlsIndex, void *Value)
{
    nvaosTlsSet(TlsIndex, Value);
}

NvError NvOsTlsAddTerminatorInternal(void (*term)(void *), void *ctx)
{
    return nvaosTlsAddTerminator(term, ctx);
}

NvBool NvOsTlsRemoveTerminatorInternal(void (*term)(void *), void *ctx)
{
    return nvaosTlsRemoveTerminator(term, ctx);
}

static void
thread_wrapper( void *v )
{
    NvOsThreadHandle t = g_CurrentThread;

    /* jump to user thread */
    t->function( t->args );

    /* threadKill/Yield will signal the join semaphore */
}

NvError
NvOsThreadCreateInternal(NvOsThreadFunction function,
    void *args, NvOsThreadHandle *thread, NvOsThreadPriorityType priority)
{
    static NvU32 s_id = 2;

    NvOsThreadHandle t;
    NvU8 *stack;
    NvU32 state;
    NvU32 stack_size;

    NV_ASSERT( thread );
    NV_ASSERT( function );

    stack_size = nvaosGetStackSize();

    /* allocate both the thread and the stack in one shot */
    t = NvOsAlloc(sizeof(NvOsThread) + stack_size );
    if( !t )
    {
        return NvError_InsufficientMemory;
    }

    stack = (NvU8 *)t + sizeof(NvOsThread);

    /* init thread members */
    nvaosThreadInit( t );

    t->wrap = thread_wrapper;
    t->function = function;
    t->args = args;
    t->magic = NVAOS_MAGIC;

    state = nvaosDisableInterrupts();

    /* init the stack */
    nvaosThreadStackInit( t, (NvU32 *)stack, stack_size / 4, NV_FALSE );

    t->id = s_id++;
    *thread = t;

    /* put the thread on the run queue */
    t->state = NvAosThreadState_Running;
    nvaosEnqueue( &t->sched_queue, &g_Runnable, NV_TRUE );

#if NV_DEBUG
    nvaosEnqueue( &t->global_queue, &g_Threads, NV_TRUE );
#endif

    nvaosRestoreInterrupts(state);

    return NvSuccess;
}

void
NvOsThreadJoinInternal( NvOsThreadHandle thread )
{
    NvOsSemaphoreWaitInternal( &thread->join_sem );

#if NV_DEBUG
    nvaosQueueRemove( &thread->global_queue );
#endif

    NvOsFree( thread );
}

void
NvOsThreadYieldInternal( void )
{
    nvaosThreadYield( NvAosThreadState_Running );
}

NvS32
NvOsAtomicCompareExchange32(NvS32 *pTarget, NvS32 OldValue, NvS32 NewValue)
{
    return nvaosAtomicCompareExchange32(pTarget, OldValue, NewValue);
}

NvS32
NvOsAtomicExchange32(NvS32 *pTarget, NvS32 Value)
{
    return nvaosAtomicExchange32(pTarget, Value);
}

NvS32
NvOsAtomicExchangeAdd32(NvS32 *pTarget, NvS32 Value)
{
    return nvaosAtomicExchangeAdd32(pTarget, Value);
}


NvError
NvOsGetSystemTime(NvOsSystemTime *hNvOsSystemtime)
{
    return NvError_NotImplemented;
}

NvError
NvOsSetSystemTime(NvOsSystemTime *hNvOsSystemtime)
{
    return NvError_NotImplemented;
}

NvU32
NvOsGetTimeMS( void )
{
    NvU64 time;

    time = NvOsGetTimeUS();
    time /= 1000ULL;    // -> msec
    return (NvU32)time;     // high 32 bits truncated!
}

NvU64
NvOsGetTimeUS( void )
{
    NvU64 time;
    NvU32 state;

    state = nvaosDisableInterrupts();
    time = nvaosGetTimeUS();
    nvaosRestoreInterrupts(state);

    return time;
}

NvError
NvOsInterruptRegister( NvU32 IrqListSize,
    const NvU32 *pIrqList,
    const NvOsInterruptHandler *pIrqHandlerList,
    void *context,
    NvOsInterruptHandle *handle,
    NvBool InterruptEnable)
{
    if(!IrqListSize)
    {
        return NvError_BadParameter;
    }
    NV_ASSERT( pIrqList );
    NV_ASSERT( pIrqHandlerList );

    return NvIntrRegister( pIrqHandlerList , context,
                IrqListSize , pIrqList, handle, InterruptEnable);
}

void NvOsInterruptMask(NvOsInterruptHandle handle, NvBool mask)
{
    return;
}

NvError
NvOsInterruptEnable(NvOsInterruptHandle handle)
{
    return NvIntrSet( handle , NV_TRUE );
}

void
NvOsInterruptDone(NvOsInterruptHandle handle)
{
    NvIntrSet( handle , NV_TRUE );
}


void
NvOsInterruptUnregister(NvOsInterruptHandle handle)
{
    if(handle)
    {
        NvIntrUnRegister(handle);
    }
}

NvError
NvOsPhysicalMemMapIntoCaller( void *pCallerPtr,
                             NvOsPhysAddr phys,
                             size_t size,
                             NvOsMemAttribute attrib,
                             NvU32 flags)
{
    NV_ASSERT(!"Should never get here under aos");
    return NvError_NotImplemented;
}

NvError NvOsGetOsInformation(NvOsOsInfo *pOsInfo)
{
    if (pOsInfo)
    {
        NvOsMemset(pOsInfo, 0, sizeof(NvOsOsInfo));
        pOsInfo->OsType = NvOsOs_Aos;
    }
    else
        return NvError_BadParameter;

    return NvSuccess;
}

void NvOsThreadSetAffinity(NvU32 CpuHint)
{
    // Not yet supported
    return ;
}

NvU32 NvOsThreadGetAffinity(void)
{
    return NVOS_INVALID_CPU_AFFINITY;
}

void NvOsGetProcessInfo(char *buf, NvU32 len)
{
    if (buf && len>0)
    {
        NvOsThreadHandle t = g_CurrentThread;
        NvOsSnprintf(buf, len, "Aos(%d)",
            (int)(t ? t->id : 0xFFFFFFFF));
        buf[len-1] = 0;
    }
}

/** POSIX compatibility layer */
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    NvError err;

    NV_ASSERT(attr == NULL);

    err = NvOsMutexCreate((NvOsMutexHandle *)mutex);

    return (err == NvSuccess) ? 0 : -1;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    NvOsMutexDestroy(*(NvOsMutexHandle *)mutex);
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    NvOsMutexLock(*(NvOsMutexHandle *)mutex);
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    NvOsMutexUnlock(*(NvOsMutexHandle *)mutex);
    return 0;
}

int pthread_key_create(pthread_key_t *key, void (*destructor_function)(void *))
{
    NV_ASSERT(destructor_function == NULL);

    *(NvU32 *)key = NvOsTlsAlloc();
    return 0;
}

int pthread_key_delete(pthread_key_t key)
{
    NvOsTlsFree((NvU32)key);
    return 0;
}

int pthread_setspecific(pthread_key_t key, const void *value)
{
    NvOsTlsSet((NvU32)key, (void *)value);
    return 0;
}

void *pthread_getspecific(pthread_key_t key)
{
    return NvOsTlsGet((NvU32)key);
}

int stat(const char *path, struct stat *buf)
{
    NvError err;
    NvOsFileHandle hFile;
    NvOsStatType statType;

    err = NvOsFopen(path, NVOS_OPEN_READ, &hFile);
    if (err)
        return -1;

    err = NvOsFstat(hFile, &statType);
    if (err)
        return -1;

    buf->st_size = (off_t)statType.size;

    NvOsFclose(hFile);
    return 0;
}

int pthread_create(pthread_t *thread,
                   const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg)
{
    NvError err;

    NV_ASSERT(attr == NULL);

    err = NvOsThreadCreate((NvOsThreadFunction)start_routine,
                           arg, (NvOsThreadHandle *)thread);

    return (err == NvSuccess) ? 0 : -1;
}

int pthread_join(pthread_t thread, void **value_ptr)
{
    NV_ASSERT(value_ptr == NULL);

    NvOsThreadJoin((NvOsThreadHandle)thread);
    return 0;
}

int sched_yield(void)
{
    NvOsThreadYield();
    return 0;
}

int sem_init(sem_t *sem, int pshared, unsigned int value)
{
    NvError err;

    NV_ASSERT(pshared == 0);

    err = NvOsSemaphoreCreate((NvOsSemaphoreHandle *)sem, value);

    return (err == NvSuccess) ? 0 : -1;
}

int sem_destroy(sem_t *sem)
{
    NvOsSemaphoreDestroy(*(NvOsSemaphoreHandle *)sem);
    return 0;
}

int sem_post(sem_t *sem)
{
    NvOsSemaphoreSignal(*(NvOsSemaphoreHandle *)sem);
    return 0;
}

int sem_wait(sem_t *sem)
{
    NvOsSemaphoreWait(*(NvOsSemaphoreHandle *)sem);
    return 0;
}

int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout)
{
    struct timespec ts;
    NvU32 nowMS;
    NvU32 absMS;
    int err;

    absMS = abs_timeout->tv_sec * 1000 + abs_timeout->tv_nsec / 1000000;

    err = clock_gettime(CLOCK_REALTIME, &ts);
    if (err)
        return err;
    nowMS = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

    err = NvOsSemaphoreWaitTimeout(*(NvOsSemaphoreHandle *)sem, absMS - nowMS);
    if (err == NvError_Timeout)
    {
        errno = ETIMEDOUT;
        return -1;
    }

    return 0;
}

int clock_gettime(clockid_t clock_id, struct timespec *tp)
{
    NvU64 timeUS;

    NV_ASSERT(clock_id == CLOCK_REALTIME);
    NV_ASSERT(tp != NULL);

    timeUS = NvOsGetTimeUS();
    tp->tv_sec = timeUS / 1000000;
    tp->tv_nsec = (timeUS % 1000000) * 1000;
    return 0;
}

volatile int *__errno(void);
volatile int *__errno(void)
{
    static NvU32 ErrnoTlsIndex = NVOS_INVALID_TLS_INDEX;

    if (ErrnoTlsIndex == NVOS_INVALID_TLS_INDEX)
    {
        ErrnoTlsIndex = NvOsTlsAlloc();
    }
    return (int *)nvaosTlsAddr(ErrnoTlsIndex);
}

int NvOsSetFpsTarget(int target) { return -1; }
int NvOsModifyFpsTarget(int fd, int target) { return -1; }
void NvOsCancelFpsTarget(int fd) { }
int NvOsSendThroughputHint (const char *usecase, NvOsTPHintType type, NvU32 value, NvU32 timeout_ms) { return 0; }
int NvOsVaSendThroughputHints (NvU32 client_tag, ...) { return 0; }
void NvOsCancelThroughputHint (const char *usecase) { }
