/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "nvassert.h"
#include "nvrm_transport.h"
#include "nvrm_xpc.h"
#include "nvcommon.h"
#include "nvos.h"
#include <linux/tegra_rpc.h>
#include <string.h>

#define TEGRA_RPC_DEV   "/dev/tegra_rpc"

/* Global functions */
NvError NvRpcStubInit(void);
void NvRpcStubDeInit(void);

NvError NvRmTransportOpen(
    NvRmDeviceHandle hRmDevice,
    char* pPortName,
    NvOsSemaphoreHandle RecvMessageSemaphore,
    NvRmTransportHandle* phTransport)
{
    struct tegra_rpc_port_desc desc = {{0}};
    int fd;
    int ret;

    NV_ASSERT(phTransport);
    if (!phTransport)
        return NvError_BadValue;

    fd = open(TEGRA_RPC_DEV, O_RDWR);
    if (fd < 0) {
        ret = errno;
        goto err;
    }

    desc.notify_fd = (int)RecvMessageSemaphore;
    if (pPortName) {
        NvOsStrncpy(desc.name, pPortName, TEGRA_RPC_MAX_NAME_LEN-1);
        desc.name[TEGRA_RPC_MAX_NAME_LEN-1] = 0;
    }
    else
        desc.name[0] = '\0';

    ret = ioctl(fd, TEGRA_RPC_IOCTL_PORT_CREATE, &desc);
    if (ret < 0) {
        ret = errno;
        close(fd);
        goto err;
    }

    *phTransport = (NvRmTransportHandle)fd;
    return NvSuccess;

err:
    if (ret == EPERM)
        return NvError_AccessDenied;
    else if (ret == ENOMEM)
        return NvError_InsufficientMemory;
    return NvError_NotInitialized;
}

void NvRmTransportClose(
    NvRmTransportHandle hTransport)
{
    int fd = (int)hTransport;

    if (fd >= 0)
        close(fd);
}

NvError NvRmTransportInit(
    NvRmDeviceHandle hRmDevice)
{
    return NvSuccess;
}

void NvRmTransportDeInit(
    NvRmDeviceHandle hRmDevice)
{
}

void NvRmTransportGetPortName(
    NvRmTransportHandle hTransport,
    NvU8 * PortName,
    NvU32 PortNameSize)
{
    int fd = (int)hTransport;
    char name[TEGRA_RPC_MAX_NAME_LEN];
    int ret;

    NvOsMemset(name, 0, TEGRA_RPC_MAX_NAME_LEN);
    name[0] = '\0';

    if (fd < 0)
        return;

    ret = ioctl(fd, TEGRA_RPC_IOCTL_PORT_GET_NAME, name);
    name[TEGRA_RPC_MAX_NAME_LEN - 1] = '\0';
    if (ret < 0)
        PortName[0] = '\0';
    else {
        NvOsStrncpy((char *)PortName, name, (size_t)PortNameSize-1);
        PortName[(size_t)PortNameSize-1] = 0;
    }
}

NvError NvRmTransportWaitForConnect(
    NvRmTransportHandle hTransport,
    NvU32 TimeoutMS)
{
    int fd = (int)hTransport;
    int ret;

    ret = ioctl(fd, TEGRA_RPC_IOCTL_PORT_LISTEN, (long)TimeoutMS);
    if (ret < 0) {
        if (errno == EPERM)
            return NvError_AccessDenied;
        else if (errno == ENOMEM)
            return NvError_InsufficientMemory;
        else if (errno == EINVAL)
            return NvError_NotInitialized;
        else if (errno == ETIMEDOUT)
            return NvError_Timeout;
        return NvError_IoctlFailed;
    }

    return NvSuccess;
}

NvError NvRmTransportConnect(
    NvRmTransportHandle hTransport,
    NvU32 TimeoutMS)
{
    int fd = (int)hTransport;
    int ret;

    ret = ioctl(fd, TEGRA_RPC_IOCTL_PORT_CONNECT, (long)TimeoutMS);
    if (ret < 0) {
        if (errno == EPERM)
            return NvError_AccessDenied;
        else if (errno == ENOMEM)
            return NvError_InsufficientMemory;
        else if (errno == EINVAL)
            return NvError_NotInitialized;
        else if (errno == ETIMEDOUT)
            return NvError_Timeout;
        return NvError_IoctlFailed;
    }

    return NvSuccess;
}

NvError NvRmTransportSetQueueDepth(
    NvRmTransportHandle hTransport,
    NvU32 MaxQueueDepth,
    NvU32 MaxMessageSize)
{
    /* does this matter? */

    return NvSuccess;
}

NvError NvRmTransportSendMsg(
    NvRmTransportHandle hTransport,
    void* pMessageBuffer,
    NvU32 MessageSize,
    NvU32 TimeoutMS)
{
    int fd = (int)hTransport;
    int ret;

    ret = write(fd, pMessageBuffer, MessageSize);
    if (ret < 0) {
        if (errno == EPERM)
            return NvError_AccessDenied;
        else if (errno == ENOMEM)
            return NvError_InsufficientMemory;
        else if (errno == EINVAL)
            return NvError_NotInitialized;
        return NvError_IoctlFailed;
    }

    return NvSuccess;
}

NvError NvRmTransportSendMsgInLP0(
    NvRmTransportHandle hPort,
    void* message,
    NvU32 MessageSize)
{
    return NvSuccess;
}

NvError NvRmTransportRecvMsg(
    NvRmTransportHandle hTransport,
    void* pMessageBuffer,
    NvU32 MaxSize,
    NvU32* pMessageSize)
{
    int fd = (int)hTransport;
    int ret;

    ret = read(fd, pMessageBuffer, MaxSize);
    if (ret < 0) {
        if (errno == EPERM)
            return NvError_AccessDenied;
        else if (errno == ENOMEM)
            return NvError_InsufficientMemory;
        else if (errno == EINVAL)
            return NvError_NotInitialized;
        return NvError_IoctlFailed;
    } else if (ret == 0)
        return NvError_TransportMessageBoxEmpty;

    *pMessageSize = ret;
    return NvSuccess;
}

NvError NvRmXpcInitArbSemaSystem(NvRmDeviceHandle hDevice)
{
    abort();
}

void NvRmXpcDeinitArbSemaSystem(NvRmDeviceHandle hDevice)
{
    abort();
}

void NvRmXpcModuleAcquire(NvRmModuleID modId)
{
    abort();
}

void NvRmXpcModuleRelease(NvRmModuleID modId)
{
    abort();
}

NvU32 NvRmPrivXpcGetMessage(NvRmPrivXpcMessageHandle hXpcMessage)
{
    abort();
}

NvError NvRmPrivXpcSendMessage(NvRmPrivXpcMessageHandle hXpcMessage,
                               NvU32 data)
{
    abort();
}

void NvRmPrivXpcDestroy(NvRmPrivXpcMessageHandle hXpcMessage)
{
    abort();
}

NvError NvRmPrivXpcCreate(NvRmDeviceHandle hDevice,
                          NvRmPrivXpcMessageHandle* phXpcMessage)
{
    abort();
}

NvError NvRpcStubInit(void)
{
    int rc;

    rc = access(TEGRA_RPC_DEV, R_OK | W_OK);
    if (rc)
        return NvError_KernelDriverNotFound;
    return NvSuccess;
}

void NvRpcStubDeInit(void)
{
}
