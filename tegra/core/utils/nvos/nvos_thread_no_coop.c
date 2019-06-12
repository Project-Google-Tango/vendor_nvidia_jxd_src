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

NvError NvOsMutexCreate(NvOsMutexHandle *mutex)
{
   return NvOsMutexCreateInternal(mutex);
}

void
NvOsMutexLock(NvOsMutexHandle mutex)
{
    NvOsMutexLockInternal(mutex);
}

void NvOsMutexUnlock(NvOsMutexHandle mutex)
{
    NvOsMutexUnlockInternal(mutex);
}

void NvOsMutexDestroy(NvOsMutexHandle mutex)
{
    if (!mutex)
        return;
    NvOsMutexDestroyInternal(mutex);
}

NvError NvOsSemaphoreCreate(NvOsSemaphoreHandle *semaphore, NvU32 value)
{
    return NvOsSemaphoreCreateInternal(semaphore, value);
}

void
NvOsSemaphoreWait(NvOsSemaphoreHandle semaphore)
{
    NvOsSemaphoreWaitInternal(semaphore);
}

NvError
NvOsSemaphoreWaitTimeout(NvOsSemaphoreHandle semaphore, NvU32 msec)
{
    return NvOsSemaphoreWaitTimeoutInternal(semaphore, msec);
}

void
NvOsSemaphoreSignal(NvOsSemaphoreHandle semaphore)
{
    NvOsSemaphoreSignalInternal(semaphore);
}

NvError
NvOsSemaphoreClone(NvOsSemaphoreHandle orig, NvOsSemaphoreHandle *clone)
{
    return NvOsSemaphoreCloneInternal(orig, clone);
}

void
NvOsSemaphoreDestroy(NvOsSemaphoreHandle semaphore)
{
    NvOsSemaphoreDestroyInternal(semaphore);
}

NvU32 NvOsTlsAlloc(void)
{
    return NvOsTlsAllocInternal();
}

void NvOsTlsFree(NvU32 TlsIndex)
{
    NvOsTlsFreeInternal(TlsIndex);
}

void *NvOsTlsGet(NvU32 TlsIndex)
{
    return NvOsTlsGetInternal(TlsIndex);
}

void NvOsTlsSet(NvU32 TlsIndex, void *Value)
{
    NvOsTlsSetInternal(TlsIndex, Value);
}

NvError NvOsTlsAddTerminator(void (*func)(void *), void *context)
{
    return NvOsTlsAddTerminatorInternal(func, context);
}

NvError
NvOsThreadCreate(NvOsThreadFunction function,
    void *args, NvOsThreadHandle *thread)
{
    return NvOsThreadCreateInternal(function, args, thread,
                                        NvOsThreadPriorityType_Normal);
}

NvError
NvOsInterruptPriorityThreadCreate(NvOsThreadFunction function,
    void *args, NvOsThreadHandle *thread)
{
    return NvOsThreadCreateInternal(function, args, thread,
                                        NvOsThreadPriorityType_NearInterrupt);
}

void
NvOsThreadJoin(NvOsThreadHandle thread)
{
    if (!thread)
        return;
    NvOsThreadJoinInternal(thread);
}

NvError
NvOsThreadSetLowPriority(void)
{
    // FIXME: implement
    return NvError_NotSupported;
}

void
NvOsThreadYield(void)
{
    NvOsThreadYieldInternal();
}

void NvOsSleepMS(NvU32 msec)
{
    NvOsSleepMSInternal(msec);
}

NvError NvOsThreadMode(int coop)
{
    if (coop)
        return NvError_NotSupported;
    return NvSuccess;
}

