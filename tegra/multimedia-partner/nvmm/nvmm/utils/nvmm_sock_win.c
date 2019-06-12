/* Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <winsock2.h>
#include <windows.h>
#include <ctype.h>
#include <urlmon.h>

#include "nvmm_sock.h"

#define SOCK_TCP 1
#define SOCK_UDP 2

typedef struct NvMMSock
{
    int fd;
    int type;

    int port;
    struct sockaddr_in dest;

    char userAgent[256];

} NvSocket;

NvError NvMMCreateSock(NvMMSock **s)
{
    NvSocket *sock;
    HRESULT hResult;
    NvU32 UserAgentStrlen;

    if (!s)
        return NvError_BadParameter;

    {
        WSADATA wsaData;
        int err;

        err = WSAStartup(WINSOCK_VERSION, &wsaData);
        if (err != 0)
            return NvError_BadParameter;        
    }

    sock = NvOsAlloc(sizeof(NvSocket));

    if (!sock)
        return NvError_InsufficientMemory;

    NvOsMemset(sock, 0, sizeof(NvSocket));
    sock->fd = -1;


    UserAgentStrlen = sizeof(sock->userAgent);
    hResult = ObtainUserAgentString(0, sock->userAgent, &UserAgentStrlen);
    if (hResult != S_OK)
    {
        NvOsStrncpy(sock->userAgent, "NvMM Client v0.1", 17);
    }

    NvMMSetUserAgentString(sock->userAgent);

    *s = (void*)sock;
    return NvSuccess;
}

void NvMMDestroySock(NvMMSock *s)
{
    NvOsFree(s);
    WSACleanup();
}

static NvError Resolve(const char *hostname, struct in_addr *sin_addr)
{
    struct hostent *host;
    NvU32 addr;

    addr = inet_addr(hostname);
    if (addr != INADDR_NONE)
    {
        sin_addr->s_addr = addr;
        return NvSuccess;
    }

    host = gethostbyname(hostname);
    if (!host)
        return NvError_BadParameter;

    NvOsMemcpy(sin_addr, host->h_addr, sizeof(struct in_addr));
    return NvSuccess;    
}

void NvMMSetNonBlocking(NvMMSock *pSock, int nonblock)
{
    NvSocket *sock = (NvSocket *)pSock;
    ioctlsocket(sock->fd, FIONBIO, &nonblock);
}

NvError NvMMOpenTCP(NvMMSock *pSock, const char *url)
{
    NvSocket *sock = (NvSocket *)pSock;
    char hostname[MAX_NVMM_HOSTNAME];
    char proto[MAX_NVMM_HOSTNAME];
    int port;
    struct sockaddr_in dest;

    if (!sock)
        return NvError_BadParameter;

    sock->type = SOCK_TCP;

    NvOsMemset(hostname, 0, MAX_NVMM_HOSTNAME);
    NvMMSplitURL(url, proto, hostname, &port, NULL, NULL);

    if (port == -1)
    {
        if (!NvOsStrcmp(proto, "rtsp"))
            port = 554;
        if (!NvOsStrcmp(proto, "http"))
            port = 80;
    }

    if (port <= 0 || port > 65535 || strlen(hostname) == 0)
        return NvError_BadParameter;

    NvOsMemset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    if (NvSuccess != Resolve(hostname, &(dest.sin_addr)))
        return NvError_BadParameter;

    sock->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock->fd < 0)
        return NvError_BadParameter;

    // convert to nonblocking
    if (connect(sock->fd, (struct sockaddr*)&dest, sizeof(dest)) < 0)
    {
        NvOsDebugPrintf("connect failed %d\n", WSAGetLastError());
        NvMMCloseTCP(sock);
        return NvError_BadParameter;
    }

    {
        int size = 32768;
        setsockopt(sock->fd, SOL_SOCKET, SO_RCVBUF, (void*)&size, sizeof(size));
    }

    return NvSuccess;
}

void NvMMCloseTCP(NvMMSock *pSock)
{
    NvSocket *sock = (NvSocket *)pSock;
    if (!sock)
        return;

    if (sock->fd >= 0)
        closesocket(sock->fd);

    sock->fd = -1;
}

int NvMMOpenUDP(NvMMSock *pSock, const char *url, int localport)
{
    NvSocket *sock = (NvSocket *)pSock;
    char hostname[MAX_NVMM_HOSTNAME];
    char proto[MAX_NVMM_HOSTNAME];
    int port;
    struct sockaddr_in dest;

    if (!sock)
        return NvError_BadParameter;

    sock->type = SOCK_UDP;

    NvOsMemset(hostname, 0, MAX_NVMM_HOSTNAME);
    NvMMSplitURL(url, proto, hostname, &port, NULL, NULL);

    if (strlen(hostname) == 0)
        return NvError_BadParameter;

    sock->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock->fd < 0)
        return NvError_BadParameter;

    NvOsMemset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = htonl(INADDR_ANY);
    dest.sin_port = htons(localport);

    if (bind(sock->fd, (struct sockaddr *)&dest, sizeof(dest)) < 0)
        return NvError_NotSupported;

    NvOsMemset(&(sock->dest), 0, sizeof(sock->dest));
    sock->dest.sin_family = AF_INET;
    sock->dest.sin_port = htons(port);
    if (Resolve(hostname, &(sock->dest.sin_addr)) < 0)
        return NvError_BadParameter;

    // convert to nonblocking
    NvMMSetNonBlocking(sock, 1);

    return NvSuccess;
}

void NvMMSetUDPPort(NvMMSock *pSock, int remoteport)
{
    NvSocket *sock = (NvSocket *)pSock;
    if (!sock)
        return;

    sock->dest.sin_port = htons(remoteport);
}

void NvMMCloseUDP(NvMMSock *pSock)
{
    NvSocket *sock = (NvSocket *)pSock;
    if (!sock)
        return;

    if (sock->fd >= 0)
        closesocket(sock->fd);

    sock->fd = -1;
}

int NvMMReadSock(NvMMSock *pSock, char *buffer, int size, int timeout)
{
    NvSocket *sock = (NvSocket *)pSock;
    int len, retval;
    fd_set readfds;
    struct timeval t;
    int err;

    if (!sock || sock->fd < 0)
        return -1;

    //FIXME: Recover properly
readagain:

    FD_ZERO(&readfds);
    FD_SET(((unsigned int)(sock->fd)), &readfds);
    t.tv_sec = (timeout / 1000);
    t.tv_usec = (timeout - (t.tv_sec * 1000)) * 1000;

    retval = select(sock->fd + 1, &readfds, NULL, NULL, &t);
    if (retval > 0 && FD_ISSET(sock->fd, &readfds))
    {
        len = recv(sock->fd, buffer, size, 0);
        if (len >= 0)
            return len;
        err = WSAGetLastError();
        if (err == WSAEINTR || err == WSAEWOULDBLOCK)
            goto readagain;
        else
            return -1;
    }

    if (retval <= 0)
        return -1;

    goto readagain;
}

int NvMMReadSockMultiple(NvMMSock **sockets, char *buffer, int size, 
                         int timeout, NvMMSock **sockReadFrom)
{
    int len, retval;
    fd_set readfds;
    struct timeval t;
    int err, i = 0;
    int max = 0;

    if (!sockets || !sockets[0])
        return -1;

    //FIXME: Recover properly
readagain:
    i = 0;
    max = 0;
    FD_ZERO(&readfds);
    while (sockets[i])
    {
        NvSocket *sock = (NvSocket *)sockets[i];
        if (sock->fd >= 0)
            FD_SET(((unsigned int)(sock->fd)), &readfds);
        if (sock->fd > max)
            max = sock->fd;
        i++;
    }

    t.tv_sec = (timeout / 1000);
    t.tv_usec = (timeout - (t.tv_sec * 1000)) * 1000;

    retval = select(max + 1, &readfds, NULL, NULL, &t);
    if (retval > 0)
    {
        i = 0;
        while (sockets[i])
        {
            NvSocket *sock = (NvSocket *)sockets[i];
            if (sock->fd >= 0 && FD_ISSET(sock->fd, &readfds))
            {
                len = recv(sock->fd, buffer, size, 0);
                if (len >= 0)
                {
                    *sockReadFrom = sockets[i];
                    return len;
                }
                err = WSAGetLastError();
                if (err == WSAEINTR || err == WSAEWOULDBLOCK)
                    goto readagain;
                else
                    return -1;
            }
            i++;
        }
    }

    if (retval <= 0)
        return -1;

    goto readagain;
}

int NvMMWriteSock(NvMMSock *pSock, char *buffer, int size, int timeout)
{
    NvSocket *sock = (NvSocket *)pSock;
    int len, retval;
    fd_set writefds;
    struct timeval t;
    int err;

    if (!sock || sock->fd < 0)
        return -1;

    if ((sock->type == SOCK_UDP && size < 0) ||
        (sock->type != SOCK_UDP && size <= 0))
    {
        goto writeend;
    }

    do
    {
        FD_ZERO(&writefds);
        FD_SET(((unsigned int)(sock->fd)), &writefds);
        t.tv_sec = (timeout / 1000);
        t.tv_usec = (timeout - (t.tv_sec * 1000)) * 1000;

        retval = select(sock->fd + 1, NULL, &writefds, NULL, &t);
        if (retval > 0 && FD_ISSET(sock->fd, &writefds))
        {
            if (sock->type == SOCK_TCP)
                len = send(sock->fd, buffer, size, 0);
            else if (sock->type == SOCK_UDP)
                len = sendto(sock->fd, buffer, size, 0, 
                             (struct sockaddr *)&sock->dest,
                             sizeof(sock->dest));
            else
                return -1;

            if (len < 0)
            {
                err = WSAGetLastError();
                if (err == WSAEINTR || err == WSAEWOULDBLOCK)
                    continue;
                return -1;
            }

            size -= len;
            buffer += len;
        }
        else if (retval <= 0)
           break;
    }
    while (size > 0);

writeend:

    if (size == 0)
        return 0;

    return -1;

}

NvU32 NvMMNToHL(NvU32 in)
{
    return ntohl(in);
}

NvU32 NvMMHToNL(NvU32 in)
{
    return htonl(in);
}

