/*
 * Copyright 2006 - 2011 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef NVOS_LINIUX_H
#define NVOS_LINIUX_H

#include "nvcommon.h"
#include "nvassert.h"
#include "nvos.h"
#include "linux/nvos_ioctl.h"

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

typedef struct NvOsLinuxKernelDriverRec
{
    void    (*nvOsDebugString)(const char*);
    NvError (*nvOsPhysicalMemMap)(
                  NvOsPhysAddr,
                  size_t,
                  NvOsMemAttribute,
                  NvU32,
                  void **);
    NvError (*nvOsSemaphoreCreate)(NvOsSemaphoreHandle *, NvU32);
    NvError (*nvOsSemaphoreClone)(NvOsSemaphoreHandle, NvOsSemaphoreHandle *);
    NvError (*nvOsSemaphoreUnmarshal)(
                  NvOsSemaphoreHandle,
                  NvOsSemaphoreHandle *);
    void    (*nvOsSemaphoreWait)(NvOsSemaphoreHandle);
    NvError (*nvOsSemaphoreWaitTimeout)(NvOsSemaphoreHandle, NvU32);
    void    (*nvOsSemaphoreSignal)(NvOsSemaphoreHandle);
    void    (*nvOsSemaphoreDestroy)(NvOsSemaphoreHandle);
    NvError (*nvOsInterruptRegister)(
                  NvU32,
                  const NvU32 *,
                  const NvOsInterruptHandler *,
                  void *,
                  NvOsInterruptHandle *,
                  NvBool);
    void    (*nvOsInterruptUnregister)(NvOsInterruptHandle);
    NvError (*nvOsInterruptEnable)(NvOsInterruptHandle);
    void    (*nvOsInterruptDone)(NvOsInterruptHandle);
    void    (*nvOsInterruptMask)(NvOsInterruptHandle, NvBool);
    void    (*close)(void);
} NvOsLinuxKernelDriver;

extern const NvOsLinuxKernelDriver *g_NvOsKernel;

NvBool NvOsLinuxErrnoToNvError(NvError *e);
NvError NvOsLinuxPhysicalMemMapFd(
    int fd,
    NvOsPhysAddr     phys,
    size_t           size,
    NvOsMemAttribute attrib,
    NvU32            flags,
    void           **ptr);

#if defined(ANDROID)
void  NvOsAndroidDebugString(const char* str);
#endif // ANDROID

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // NVOS_LINIUX_H 
