/* Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include "nvmm_sock.h"

#define SOCK_TCP 1
#define SOCK_UDP 2

typedef struct NvMMSock
{
    int fd;
    int type;

    int port;
    struct sockaddr_in dest;
} NvSocket;

NvError NvMMCreateSock(NvMMSock **s)
{
    NvSocket *sock;
    char* pUAStr = NULL;
    NvU32 nUALen = 0;

    if (!s)
        return NvError_BadParameter;

    sock = NvOsAlloc(sizeof(NvSocket));

    if (!sock)
        return NvError_InsufficientMemory;

    NvOsMemset(sock, 0, sizeof(NvSocket));
    sock->fd = -1;

    NvMMGetUserAgentString(sock, &pUAStr, &nUALen);
    if (!pUAStr || !nUALen)
    {
        NvMMSetUserAgentString("NvMM Client v0.1");
    }

    *s = (void*)sock;
    return NvSuccess;
}

void NvMMDestroySock(NvMMSock *s)
{
    NvOsFree(s);
}

static NvError Resolve(const char *hostname, struct in_addr *sin_addr)
{
    struct hostent *host;

    if (inet_aton(hostname, sin_addr))
        return NvSuccess;

    host = gethostbyname(hostname);
    if (!host)
        return NvError_BadParameter;

    NvOsMemcpy(sin_addr, host->h_addr, sizeof(struct in_addr));
    return NvSuccess;    
}

void NvMMSetNonBlocking(NvMMSock *pSock, int nonblock)
{
    NvSocket *sock = (NvSocket *)pSock;
    int flags;

    flags = fcntl(sock->fd, F_GETFL, 0);
    if (nonblock)
        flags |= O_NONBLOCK;
    else
        flags = flags & ~O_NONBLOCK;

    fcntl(sock->fd, F_SETFL, flags);
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

    if (NvMMSockGetBlockActivity())
    {
        NvOsDebugPrintf("connect cancelled\n");
        return NvError_BadParameter;
    }

    NvOsMemset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    if (NvSuccess != Resolve(hostname, &(dest.sin_addr)))
        return NvError_BadParameter;

    sock->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock->fd < 0)
        return NvError_BadParameter;

    NvMMSetNonBlocking(sock, 1);
    // convert to nonblocking
    if (connect(sock->fd, (struct sockaddr*)&dest, sizeof(dest)) < 0)
    {
        if (errno == EAGAIN || errno == EINPROGRESS)
        {
            fd_set connfds;
            struct timeval t;
            int retval;
            int connectiontries = 0;

            /* arbitrary timeout, feel free to change */
            while (connectiontries++ < 300)
            {
                if (NvMMSockGetBlockActivity())
                {
                    NvOsDebugPrintf("connect cancelled\n");
                    return NvError_BadParameter;
                }

                FD_ZERO(&connfds);
                FD_SET(sock->fd, &connfds);
                t.tv_sec = 0;
                t.tv_usec = 100000; /* .1 sec */

                retval = select(sock->fd + 1, NULL, &connfds, NULL, &t);
                if (retval < 0 && errno != EINTR)
                {
                    NvOsDebugPrintf("connect failed\n");
                    return NvError_BadParameter;
                }
                else if (retval > 0)
                {
                    socklen_t len;
                    int sockerr = 0;
                    len = sizeof(sockerr);
                    if (getsockopt(sock->fd, SOL_SOCKET, SO_ERROR,
                                   &sockerr, &len) < 0)
                    {
                        NvOsDebugPrintf("connect failed, getsockopt\n");
                        return NvError_BadParameter;
                    }

                    if (sockerr != 0)
                    {
                        NvOsDebugPrintf("connect failed, err: %x\n", sockerr);
                        return NvError_BadParameter;
                    }

                    break;
                }
            }
        }
        else
        {
            NvOsDebugPrintf("connect failed\n");
            return NvError_BadParameter;
        }
    }

    return NvSuccess;
}

void NvMMCloseTCP(NvMMSock *pSock)
{
    NvSocket *sock = (NvSocket *)pSock;
    int retval = 0;
    if (!sock)
        return;

    if (sock->fd >= 0)
    {
        // shutdown() call will make sure that connection close is complete
        retval = shutdown(sock->fd, SHUT_RDWR);
        if(retval < 0)
        {
            // Print error information
            NvOsDebugPrintf("TCP Shutdown Failed with %d \n", errno);
        }
        close(sock->fd);
    }

    sock->fd = -1;
}

NvError NvMMOpenUDP(NvMMSock *pSock, const char *url, int localport)
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

    NvOsMemset(&dest, 0, sizeof(dest));
    sock->dest.sin_family = AF_INET;
    sock->dest.sin_port = htons(port);
    if (NvSuccess != Resolve(hostname, &(sock->dest.sin_addr)))
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
        //closesocket(sock->fd);
        close(sock->fd);

    sock->fd = -1;
}

int NvMMReadSock(NvMMSock *pSock, char *buffer, int size, int timeout)
{
    NvSocket *sock = (NvSocket *)pSock;
    int len, retval;
    fd_set readfds;
    struct timeval t;
    unsigned int readRetries = 0;

    if (!sock || sock->fd < 0)
        return -1;

    readRetries = timeout / 100;
    if(readRetries == 0)
        readRetries = 1;

    do
    {
        //NvOsDebugPrintf("NvMMReadSock::readRetries %d", readRetries);
        readRetries--;

readagain:

       FD_ZERO(&readfds);
       FD_SET(sock->fd, &readfds);
       t.tv_sec = 0;
       t.tv_usec = 100000;

       if (NvMMSockGetBlockActivity())
       {
           //NvOsDebugPrintf("NvMMReadSock::BlockActivity Set\n");
           return -1;
       }

       retval = select(sock->fd + 1, &readfds, NULL, NULL, &t);
       if (retval > 0 && FD_ISSET(sock->fd, &readfds))
       {
          if (NvMMSockGetBlockActivity())
          {
              //NvOsDebugPrintf("NvMMReadSock::BlockActivity Set\n");
              return -1;
          }

          len = recv(sock->fd, buffer, size, 0);
          if (len >= 0)
              return len;
          if (errno == EINTR || errno == EAGAIN)
              goto readagain;
          else
              return -1;
       }
       else if (retval < 0)
                return -1;

   } while(readRetries > 0);
   if (readRetries == 0)
   {
       //NvOsDebugPrintf("NvMMReadSock::readRetries = 0 \n");
       return -1;
   }
   //NvOsDebugPrintf("NvMMReadSock::goto readagain \n");
   goto readagain;
}

int NvMMReadSockMultiple(NvMMSock **sockets, char *buffer, int size, 
                         int timeout, NvMMSock **sockReadFrom)
{
    int len, retval;
    fd_set readfds;
    struct timeval t;
    int i = 0;
    int max = 0;

    if (!sockets || !sockets[0])
        return -1;

    //FIXME: Recover properly
readagain:

    if (NvMMSockGetBlockActivity())
        return -1;

    FD_ZERO(&readfds);
    while (sockets[i])
    {
        NvSocket *sock = (NvSocket *)sockets[i];
        if (NvMMSockGetBlockActivity())
            return -1;

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
        if (NvMMSockGetBlockActivity())
            return -1;

        i = 0;
        while (sockets[i])
        {
            NvSocket *sock = (NvSocket *)sockets[i];
            if (NvMMSockGetBlockActivity())
                return -1;

            if (sock->fd >= 0 && FD_ISSET(sock->fd, &readfds))
            {
                len = recv(sock->fd, buffer, size, 0);
                if (len >= 0)
                {
                    *sockReadFrom = sockets[i];
                    return len;
                }
                if (errno == EINTR || errno == EAGAIN)
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
        FD_SET(sock->fd, &writefds);
        t.tv_sec = 0;
        t.tv_usec = timeout * 1000;

        if (NvMMSockGetBlockActivity())
           return -1;

        retval = select(sock->fd + 1, NULL, &writefds, NULL, &t);
        if (retval > 0 && FD_ISSET(sock->fd, &writefds))
        {
            if (NvMMSockGetBlockActivity())
                return -1;

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
                if (errno == EINTR || errno == EAGAIN)
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

