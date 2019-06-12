/*
 * Copyright (c) 2007 - 2009 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvos.h"
#include "nvos_internal.h"
#include "nvos_coop_thread.h"
#include "nvassert.h"

#if NVOS_TRACE || NV_DEBUG
#undef NvOsMutexCreate
#undef NvOsMutexLock
#undef NvOsMutexUnlock
#undef NvOsMutexDestroy
#undef NvOsSemaphoreCreate
#undef NvOsSemaphoreWait
#undef NvOsSemaphoreWaitTimeout
#undef NvOsSemaphoreSignal
#undef NvOsSemaphoreClone
#undef NvOsSemaphoreDestroy
#undef NvOsThreadCreate
#undef NvOsInterruptPriorityThreadCreate
#undef NvOsThreadJoin
#undef NvOsThreadSetLowPriority
#undef NvOsThreadYield
#endif

NvBool g_UseCoopThread = NV_FALSE;

NvError NvOsMutexCreate(NvOsMutexHandle *mutex)
{
    if (g_UseCoopThread)
        return CoopMutexCreate(NULL, mutex);
    else
        return NvOsMutexCreateInternal(mutex);
}

void
NvOsMutexLock(NvOsMutexHandle mutex)
{
    if (g_UseCoopThread)
        CoopMutexLock(mutex);
    else
        NvOsMutexLockInternal(mutex);
}

void 
NvOsMutexUnlock(NvOsMutexHandle mutex)
{
    if (g_UseCoopThread)
        CoopMutexUnlock(mutex);
    else
        NvOsMutexUnlockInternal(mutex);
}

void NvOsMutexDestroy(NvOsMutexHandle mutex)
{
    if (!mutex)
        return;
    if (g_UseCoopThread)
        CoopMutexDestroy(mutex);
    else
        NvOsMutexDestroyInternal(mutex);
}

NvError NvOsSemaphoreCreate(NvOsSemaphoreHandle *semaphore, NvU32 value)
{
    if (g_UseCoopThread)
        return CoopSemaphoreCreate(semaphore, value);
    else
        return NvOsSemaphoreCreateInternal(semaphore, value);
}

void
NvOsSemaphoreWait(NvOsSemaphoreHandle semaphore)
{
    if (g_UseCoopThread)
        CoopSemaphoreWait(semaphore, NV_WAIT_INFINITE);
    else
        NvOsSemaphoreWaitInternal(semaphore);
}

NvError
NvOsSemaphoreWaitTimeout(NvOsSemaphoreHandle semaphore, NvU32 msec)
{
#ifndef BUILD_FOR_COSIM
    if (g_UseCoopThread)
        return CoopSemaphoreWait(semaphore, msec);
    else
        return NvOsSemaphoreWaitTimeoutInternal(semaphore, msec);
#else
    return NvSuccess;
#endif
}

void
NvOsSemaphoreSignal(NvOsSemaphoreHandle semaphore)
{
    if (g_UseCoopThread)
        CoopSemaphoreSignal(semaphore);
    else
        NvOsSemaphoreSignalInternal(semaphore);
}

NvError
NvOsSemaphoreClone(NvOsSemaphoreHandle orig, NvOsSemaphoreHandle *clone)
{
    if (g_UseCoopThread)
        return CoopSemaphoreClone(orig, clone);
    else
        return NvOsSemaphoreCloneInternal(orig, clone);
}

void
NvOsSemaphoreDestroy(NvOsSemaphoreHandle semaphore)
{
    if (!semaphore)
        return;
    if (g_UseCoopThread)
        CoopSemaphoreDestroy(semaphore);
    else
        NvOsSemaphoreDestroyInternal(semaphore);
}

NvU32 NvOsTlsAlloc(void)
{
    if (g_UseCoopThread)
        return CoopTlsAlloc();
    else
        return NvOsTlsAllocInternal();
}

void NvOsTlsFree(NvU32 TlsIndex)
{
    if (g_UseCoopThread)
        CoopTlsFree(TlsIndex);
    else
        NvOsTlsFreeInternal(TlsIndex);
}

void *NvOsTlsGet(NvU32 TlsIndex)
{
    if (g_UseCoopThread)
        return CoopTlsGet(TlsIndex);
    else
        return NvOsTlsGetInternal(TlsIndex);
}

void NvOsTlsSet(NvU32 TlsIndex, void *Value)
{
    if (g_UseCoopThread)
        CoopTlsSet(TlsIndex, Value);
    else
        NvOsTlsSetInternal(TlsIndex, Value);
}

NvError NvOsTlsAddTerminator(void (*func)(void *), void *context)
{
    if (g_UseCoopThread)
        return CoopTlsAddTerminator(func, context);
    else
        return NvOsTlsAddTerminatorInternal(func, context);
}

NvBool NvOsTlsRemoveTerminator(void (*func)(void *), void *context)
{
    if (g_UseCoopThread)
        return CoopTlsRemoveTerminator(func, context);
    else
        return NvOsTlsRemoveTerminatorInternal(func, context);
}

NvError
NvOsThreadCreate(NvOsThreadFunction function,
    void *args, NvOsThreadHandle *thread)
{
    if (g_UseCoopThread)
        return CoopThreadCreate(function, args, thread);
    else
        return NvOsThreadCreateInternal(function, args, thread,
                                        NvOsThreadPriorityType_Normal);
}

NvError
NvOsInterruptPriorityThreadCreate(NvOsThreadFunction function,
    void *args, NvOsThreadHandle *thread)
{
    if (g_UseCoopThread)
        return CoopThreadCreate(function, args, thread );
    else
        return NvOsThreadCreateInternal(function, args, thread,
                                        NvOsThreadPriorityType_NearInterrupt);
}

void
NvOsThreadJoin(NvOsThreadHandle thread)
{
    if (!thread)
        return;
    if (g_UseCoopThread)
        CoopThreadJoin(thread);
    else
        NvOsThreadJoinInternal(thread);
}

NvError
NvOsThreadSetLowPriority(void)
{
    if (g_UseCoopThread)
    {
        return CoopThreadSetLowPriority();
    }
    else
    {
        // FIXME: implement
        return NvError_NotSupported;
    }
}

void
NvOsThreadYield(void)
{
    if (g_UseCoopThread)
        CoopThreadYield();
    else
        NvOsThreadYieldInternal();
}

void NvOsSleepMS(NvU32 msec)
{
    if (g_UseCoopThread)
        CoopSleepMS(msec);
    else
        NvOsSleepMSInternal(msec);
}
