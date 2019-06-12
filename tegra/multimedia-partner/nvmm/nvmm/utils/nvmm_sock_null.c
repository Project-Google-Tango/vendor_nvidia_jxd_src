/* Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <ctype.h>
#include "nvmm_sock.h"

#define SOCK_TCP 1
#define SOCK_UDP 2

typedef struct NvMMSock
{
    int fd;
    int type;

    int port;
} NvSocket;

NvError NvMMCreateSock(NvMMSock **s)
{
    return NvError_BadParameter;
}

void NvMMDestroySock(NvMMSock *s)
{
}

void NvMMSetNonBlocking(NvMMSock *pSock, int nonblock)
{
}

NvError NvMMOpenTCP(NvMMSock *pSock, const char *url)
{
    return NvError_BadParameter;
}

void NvMMCloseTCP(NvMMSock *pSock)
{
}

NvError NvMMOpenUDP(NvMMSock *pSock, const char *url, int localport)
{
    return NvError_BadParameter;
}

void NvMMSetUDPPort(NvMMSock *pSock, int remoteport)
{
}

void NvMMCloseUDP(NvMMSock *pSock)
{
}

int NvMMReadSock(NvMMSock *pSock, char *buffer, int size, int timeout)
{
    return -1;
}

int NvMMWriteSock(NvMMSock *pSock, char *buffer, int size, int timeout)
{
    return -1;
}

NvU32 NvMMNToHL(NvU32 in)
{
    return 0;
}

NvU32 NvMMHToNL(NvU32 in)
{
    return 0;
}

int NvMMReadSockMultiple(NvMMSock **sockets, char *buffer, int size, 
                         int timeout, NvMMSock **sockReadFrom)
{
    return -1;
}

