/*
 * Copyright 2009 - 2011 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "nvos.h"
#include "nvos_internal.h"
#include "nvos_debug.h"
#include "nvassert.h"
#include "nvos_linux.h"

typedef struct NvOsSemaphoreRec
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    NvU32 count; /* the sem value (not num of waiting threads) */
    NvU32 createCount;
} NvOsSemaphore;

static int s_DevMemFd = 0;

static NvError NvOsLinUserPhysicalMemMap(
    NvOsPhysAddr     phys,
    size_t           size,
    NvOsMemAttribute attrib,
    NvU32            flags,
    void           **ptr)
{
    if (s_DevMemFd<=0)
        return NvError_AccessDenied;

    return NvOsLinuxPhysicalMemMapFd(s_DevMemFd,
               phys, size, attrib, flags, ptr);
}

static NvError NvOsLinUserSemaphoreCreate(NvOsSemaphoreHandle *Sem, NvU32 Cnt)
{
    NvOsSemaphore *s;

    s = NvOsAlloc(sizeof(NvOsSemaphore));
    if (!s)
        return NvError_InsufficientMemory;

    (void)pthread_mutex_init( &s->mutex, 0 );
    (void)pthread_cond_init( &s->cond, 0 );
    s->count = Cnt;
    s->createCount = 1;

    *Sem = s;
    return NvSuccess;
}

static NvError NvOsLinUserSemaphoreClone(
    NvOsSemaphoreHandle  Orig,
    NvOsSemaphoreHandle *Dupe)
{
    (void)NvOsAtomicExchangeAdd32((NvS32 *)&Orig->createCount, 1);
    *Dupe = Orig;
    return NvSuccess;
}

static NvError NvOsLinUserSemaphoreWaitTimeout(
    NvOsSemaphore *s,
    NvU32 msec)
{
    NvError ret = NvSuccess;

    pthread_mutex_lock( &s->mutex );

    if( s->count > 0 )
    {
        goto wait_end;
    }

    if( !msec )
    {
        ret = NvError_Timeout;
        goto wait_end;
    }
    else
    {
        struct timeval curTime;
        struct timespec timeoutVal;
        NvU32  timeout_sec;
        NvU32  timeout_nsec;
        int error;

        timeout_sec = msec / 1000;
        /* Timeout values greater than 18 hours would overflow */
        timeout_nsec = msec * 1000 * 1000 - (timeout_sec * 1000000000);
        if( gettimeofday(&curTime, NULL) )
        {
            ret = NvError_NotSupported;
            goto wait_end;
        }

        timeoutVal.tv_sec = curTime.tv_sec + timeout_sec;
        timeoutVal.tv_nsec = (curTime.tv_usec * 1000) + timeout_nsec;

        error = pthread_cond_timedwait( &s->cond, &s->mutex, &timeoutVal );
        if( error )
        {
            if( error == ETIMEDOUT || error == EINTR )
            {
                ret = NvError_Timeout;
            }
            else
            {
                ret = NvError_NotSupported;
            }
        }
    }

wait_end:
    if( ret == NvSuccess )
    {
        s->count--;
    }

    pthread_mutex_unlock( &s->mutex );
    return ret;
}

static void NvOsLinUserSemaphoreWait(NvOsSemaphore *s)
{
    pthread_mutex_lock( &s->mutex );

    while( !s->count )
    {
        pthread_cond_wait( &s->cond, &s->mutex );
    }

    s->count--;

    pthread_mutex_unlock( &s->mutex );
}

static void NvOsLinUserSemaphoreSignal(NvOsSemaphoreHandle s)
{
    pthread_mutex_lock( &s->mutex );
    s->count++;
    pthread_cond_signal( &s->cond );
    pthread_mutex_unlock( &s->mutex );
}

static void NvOsLinUserSemaphoreDestroy(NvOsSemaphoreHandle s)
{
    NvU32 v = NvOsAtomicExchangeAdd32((NvS32*)&s->createCount, -1);

    if (v>1)
        return;

    NV_ASSERT(v==1);

    (void)pthread_mutex_destroy( &s->mutex );
    (void)pthread_cond_destroy( &s->cond );

    NvOsFree(s);
}

static void NvOsLinUserClose(void)
{
    if (s_DevMemFd > 0)
        close(s_DevMemFd);

    s_DevMemFd = 0;
}

const NvOsLinuxKernelDriver s_UserDriver =
{
#if defined(ANDROID)
    NvOsAndroidDebugString,
#else
    NULL, // NvOsLinUserDebugString,
#endif // ANDROID
    NvOsLinUserPhysicalMemMap,
    NvOsLinUserSemaphoreCreate,
    NvOsLinUserSemaphoreClone,
    NULL, //NvOsLinUserSemaphoreUnmarshal,
    NvOsLinUserSemaphoreWait,
    NvOsLinUserSemaphoreWaitTimeout,
    NvOsLinUserSemaphoreSignal,
    NvOsLinUserSemaphoreDestroy,
    NULL, // NvOsLinUserInterruptRegister,
    NULL, // NvOsLinUserInterruptUnregister,
    NULL, // NvOsLinUserInterruptEnable,
    NULL, // NvOsLinUserInterruptDone,
    NULL, // NvOsLinUserInterruptMask
    NvOsLinUserClose
};

const NvOsLinuxKernelDriver *NvOsLinUserInit(void);
const NvOsLinuxKernelDriver *NvOsLinUserInit(void)
{
    s_DevMemFd = open("/dev/mem", O_RDWR | O_SYNC);

    if (s_DevMemFd <= 0)
        s_DevMemFd = 0;

    return &s_UserDriver;
}
