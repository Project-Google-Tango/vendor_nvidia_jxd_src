/* Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef SOCK_H_
#define SOCK_H_

#include <nverror.h>
#include <nvos.h>
#include "nvmm_parser_types.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef void NvMMSock;

typedef NvError ( *pfnPostCallback)( char *postResponse, NvS64 responseSize);

NvError NvMMCreateSock(NvMMSock **s);

void NvMMDestroySock(NvMMSock *s);

void NvMMSetNonBlocking(
    NvMMSock *sock, int nonblock);

int 
NvMMReadSock(
    NvMMSock *sock, 
    char *buffer,
    int size, 
    int timeout);

int 
NvMMWriteSock(
    NvMMSock *sock,
    char *buffer,
    int size,
    int timeout);

int 
NvMMReadSockMultiple(
    NvMMSock **sockets,
    char *buffer,
    int size, 
    int timeout,
    NvMMSock **sockReadFrom);

NvError 
NvMMOpenTCP(
    NvMMSock *sock,
    const char *url);

void NvMMCloseTCP(NvMMSock *sock);

NvError 
NvMMOpenUDP(
    NvMMSock *socket, 
    const char *url,
    int localport);

void 
NvMMSetUDPPort(
    NvMMSock *socket,
    int remoteport);

void NvMMCloseUDP(NvMMSock *socket);

NvU32 NvMMNToHL(NvU32 in);
NvU32 NvMMHToNL(NvU32 in);

#define MAX_NVMM_HOSTNAME 256
#define MAX_NVMM_FILENAMELEN 512

void NvMMSplitURL(
    const char *url,
    char *protocol,
    char *hostname,
    int *port,
    char *file,
    char *headers);


NvError
NvMMSockReadFullBuffer(
    NvMMSock *sock,
    char *buffer,
    NvS64 *size,
    int timeout);

NvError 
NvMMSockGetReply(
NvMMSock *pSock, 
char *response_buf,
 int response_len);

NvError 
NvMMSockGetLine(
char **linestart,
int *linelen);

NvError 
NvMMSockReadLine(
NvMMSock *pSock,
char *response_buf,
int response_len);

NvError 
NvMMSockReadChunks(
    NvMMSock *sock,
    char **buffer,
    NvS64 *size, 
    int timeout);

NvError 
NvMMSockConnectHTTP(
    char *szURI,
    NvS64 *contentlen,
    int *responsecode,
    char **actualURL, 
    NvMMSock **pSock,
    NvS64 startfrom,
    char *contenttype,
    int maxcontentlen,
    int *metainterval);

NvError 
NvMMSockGetHTTPFile(
    char *szURI,
    char **buffer,
    NvS64 *buflen);

NvError NvMMSockPOSTHTTPFile(
    char  *pwszUrl,
    NvU32 cchUrl,
    char   *pszChallenge,
    NvU32 cchChallenge,
    pfnPostCallback postCalback);

NvError 
NvMMSockConnectHTTPPOST(
    char *szURI,
    NvU32 cchUrl,
    NvS64 *contentlen,
    NvU32 *responsecode,
    char **actualURL,
    NvMMSock **pSock,
    NvS64 startfrom,
    char *contenttype,
    NvU32 maxcontenttypelen,
    NvU32 *metainterval,
    char  *pszChallenge,
    NvU32 cchChallenge);

NvError 
NvMMGetUserAgentString(
    NvMMSock *pSock,
    char **pUserAgentStr,
    NvU32 *pUserAgentStrlen);

NvError NvMMSetUserAgentString(char* pUserAgentStr);

NvError NvMMSetUAProfAgentString(char* pUserAgentProfStr);

NvError NvMMGetUAProfAgentString(NvMMSock *pSock, char **pUserAgentProfStr, NvU32 *pUserAgentProfStrlen);

void NvMMSockSetBlockActivity(int block);

int NvMMSockGetBlockActivity(void);

#if defined(__cplusplus)
}
#endif

#endif

