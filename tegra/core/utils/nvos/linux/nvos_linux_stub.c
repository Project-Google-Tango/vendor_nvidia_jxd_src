/*
 * Copyright (c) 2009-2013, NVIDIA Corporation.  All Rights Reserved.
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

#include <linux/tegra_sema.h>

#define TEGRA_SEMA_DEV  "/dev/tegra_sema"

static NvBool s_HaveTrpcSema = NV_FALSE;
static int s_DevConsoleFd = -1;

static void NvOsLinStubDebugString(const char * str)
{
    if (s_DevConsoleFd >= 0)
    {
        if (write(s_DevConsoleFd, str, strlen(str)) < 0)
            NV_ASSERT(!"NvOsLinStubDebugString: write() failed");
    }
}

static NvError NvOsLinStubPhysicalMemMap(
    NvOsPhysAddr     phys,
    size_t           size,
    NvOsMemAttribute attrib,
    NvU32            flags,
    void           **ptr)
{
    int fd;
    int ret;

#if defined(ANDROID)
    NvOsDebugPrintf("%s: map 0x%x bytes @ 0x%lx (attr=%x flags=%x)\n",
         __func__, size, (unsigned long)phys, attrib, flags);
#if !defined(BUILD_FOR_COSIM)
    abort();
#endif
#endif

    fd = open("/dev/mem", O_RDWR);
    if (fd < 0) {
        return NvError_KernelDriverNotFound;
    }
    ret = NvOsLinuxPhysicalMemMapFd(fd, phys, size, attrib, flags, ptr);
    close(fd);
    return ret;
}

static NvError NvOsLinStubSemaphoreCreate(NvOsSemaphoreHandle *Sem, NvU32 Cnt)
{
    NvU32 i;

    if (!s_HaveTrpcSema)
        return NvError_AccessDenied;

    int fd = open(TEGRA_SEMA_DEV, O_RDWR);
    /*
     * reserve fd = 0,1,2 since they normally point to stdin/out/err and closing
     * any one of these can have undesired results for the tegra_sema driver.
     */
    while((fd >= 0)&&(fd < 3))
        fd = open(TEGRA_SEMA_DEV, O_RDWR);
    if (fd < 0)
        return NvError_AccessDenied;
    *Sem = (NvOsSemaphoreHandle)(intptr_t)fd;

    for (i = 0; i < Cnt; i++)
        ioctl((int)fd, TEGRA_SEMA_IOCTL_SIGNAL);

    return NvSuccess;
}

static NvError NvOsLinStubSemaphoreClone(
    NvOsSemaphoreHandle  Orig,
    NvOsSemaphoreHandle *Dupe)
{
    if (!s_HaveTrpcSema)
        return NvError_AccessDenied;

    int fd = dup((int)(intptr_t)Orig);
    if (fd < 0)
        return NvError_BadParameter;

    *Dupe = (NvOsSemaphoreHandle)(intptr_t)fd;
    return NvSuccess;
}

static NvError NvOsLinStubSemaphoreUnmarshal(
    NvOsSemaphoreHandle  Client,
    NvOsSemaphoreHandle *Driver)
{
    if (!s_HaveTrpcSema)
        return NvError_AccessDenied;

    return NvError_KernelDriverNotFound;
}

static NvError NvOsLinStubSemaphoreWaitTimeout(
    NvOsSemaphoreHandle Sem,
    NvU32 Timeout)
{
    int ret;
    long timeout;
    NvError e;

    if (!s_HaveTrpcSema)
        return NvError_AccessDenied;

    timeout = Timeout;
    do {
        /* if interrupted by signal, timeout will have the time remaining */
        if (Timeout == NV_WAIT_INFINITE)
            timeout = -1;
        ret = ioctl((int)(intptr_t)Sem, TEGRA_SEMA_IOCTL_WAIT, &timeout);
    } while (ret == -1 && errno == EINTR);

    if (ret == 0)
        return NvSuccess;
    else if (NvOsLinuxErrnoToNvError(&e))
        return e;
    return NvError_IoctlFailed;
}

static void NvOsLinStubSemaphoreWait(NvOsSemaphoreHandle Sem)
{
    NvError e = NvOsLinStubSemaphoreWaitTimeout(Sem, NV_WAIT_INFINITE);
    if (e != NvSuccess)
        NvOsDebugPrintf("nvos: while waiting for semaphore, nverror %d\n", e);
}

static void NvOsLinStubSemaphoreSignal(NvOsSemaphoreHandle s)
{
    if (!s_HaveTrpcSema)
        return;

    ioctl((int)(intptr_t)s, TEGRA_SEMA_IOCTL_SIGNAL);
}

static void NvOsLinStubSemaphoreDestroy(NvOsSemaphoreHandle s)
{
    if (!s_HaveTrpcSema)
        return;
    int ret = close((int)(intptr_t)s);
    if (ret < 0)
        NvOsDebugPrintf("nvos: error = %d while closing fd = %d", errno, (int)(intptr_t)s);
}

static NvError NvOsLinStubInterruptRegister(
    NvU32 IrqListSize,
    const NvU32 *pIrqList,
    const NvOsInterruptHandler *pIrqHandlerList,
    void *context,
    NvOsInterruptHandle *handle,
    NvBool InterruptEnable)
{
    abort();
}

static void NvOsLinStubInterruptUnregister(NvOsInterruptHandle h)
{
    abort();
}

static NvError NvOsLinStubInterruptEnable(NvOsInterruptHandle h)
{
    abort();
}

static void NvOsLinStubInterruptDone(NvOsInterruptHandle h)
{
    abort();
}

static void NvOsLinStubInterruptMask(NvOsInterruptHandle h, NvBool m)
{
    abort();
}

static void NvOsLinStubClose(void)
{
    if (s_DevConsoleFd >= 0)
        close(s_DevConsoleFd);

    s_DevConsoleFd = -1;
}

const NvOsLinuxKernelDriver s_StubDriver =
{
#if defined(ANDROID)
    NvOsAndroidDebugString,
#else
    NvOsLinStubDebugString,
#endif // ANDROID
    NvOsLinStubPhysicalMemMap,
    NvOsLinStubSemaphoreCreate,
    NvOsLinStubSemaphoreClone,
    NvOsLinStubSemaphoreUnmarshal,
    NvOsLinStubSemaphoreWait,
    NvOsLinStubSemaphoreWaitTimeout,
    NvOsLinStubSemaphoreSignal,
    NvOsLinStubSemaphoreDestroy,
    NvOsLinStubInterruptRegister,
    NvOsLinStubInterruptUnregister,
    NvOsLinStubInterruptEnable,
    NvOsLinStubInterruptDone,
    NvOsLinStubInterruptMask,
    NvOsLinStubClose
};

static void NvOsReadFdLink(int fd, char *buf, ssize_t buf_size)
{
    char path[100];
    ssize_t size;
    snprintf(path, sizeof(path), "/proc/%d/fd/%d",getpid(),fd);
    path[sizeof(path)-1] = 0;
    size = readlink(path, buf, buf_size);

    if (size < 1 || size >= buf_size)
        size = 0;
    buf[size] = 0;
}

static NvBool NvOsStderrIsConsole(void)
{
    char err_link[50];
    NvOsReadFdLink(2, err_link, sizeof(err_link));

    if (strcmp(err_link, "/dev/console") == 0)
        return 1;
    if (strcmp(err_link, "/dev/ttyS0") == 0)
        return 1;

    return 0;
}

const NvOsLinuxKernelDriver *NvOsLinStubInit(void);
const NvOsLinuxKernelDriver *NvOsLinStubInit(void)
{
    int rc;

    rc = access(TEGRA_SEMA_DEV, R_OK | W_OK);
    if (rc)
        return NULL;

    s_HaveTrpcSema = NV_TRUE;

    // open /dev/console for NvOsDebugPrintf, but only if stderr is not already
    // printing to the console.  (NvOsDebugPrintf also prints to stderr, so
    // this would cause 2 copies of each printf on the console.)
    if (!NvOsStderrIsConsole())
        s_DevConsoleFd = open("/dev/console", O_WRONLY);

    return &s_StubDriver;
}
