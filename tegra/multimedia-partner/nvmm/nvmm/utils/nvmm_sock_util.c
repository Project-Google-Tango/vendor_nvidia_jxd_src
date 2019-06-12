/* Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvmm_sock.h"
#include "nvutil.h"
#include "nvmm_logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


void NvMMSplitURL(const char *url, char *protocol, char *hostname,
                  int *port, char *file, char *headers)
{
    const char *s = url;
    char *h = hostname;
    char *f = file;
    char *ch =NULL;
    int urllength = NvOsStrlen(url) + 1 + 2;

    *port = -1;

    // get protocol
    s = strchr(url, ':');
    if (s)
    {
        const char *s1 = url;
        char *p = protocol;
        while (s1 != s && p < protocol + MAX_NVMM_HOSTNAME)
        {
            *p = tolower(*s1);
            p++; s1++;
        }
        *p = '\0';

        s++;
        while (*s == '/')
            s++;
    }

    if (!s)
        s = url;

    while (*s != '/' && *s != '\0' && *s != ':' && 
           h < hostname + MAX_NVMM_HOSTNAME)
    {
        *h = *s;
        h++; s++;
    }

    *h = '\0';

    if (*s == ':')
    {
        s++;
        *port = atoi(s);
    }

    while (*s != '/' && *s != '\0')
        s++;

    if (*s == '\0')
    {
        if (file)
        {
            file[0] = '/';
            file[1] = '\0';
        }
    }

    ch = strstr(url, "NV_CK_HEADER");

    while (file && *s != '\0' && s != ch && f < (file +urllength ))
    {
        *f = *s;
        f++; s++;
    }
    if ( ch )
    {
        s=s+NvOsStrlen( "NV_CK_HEADER" );
        ch = headers;

        while (headers && *s != '\0' && ch < headers + MAX_NVMM_FILENAMELEN)
        {
            *ch = *s;
             ch++; s++;
        }
    }
}

NvError NvMMSockReadFullBuffer(NvMMSock *sock, char *buffer, NvS64 *size, int timeout)
{
    NvS64 len = 0;
    NvS64 tmp = 0;
    NvS64 realsize = *size;

    *size = 0;

    while (realsize > 0)
    {
        tmp = NvMMReadSock(sock, buffer + len, (int)realsize, timeout);
        if (tmp == 0)
            return NvError_EndOfFile;
        else if (tmp < 0)
            return NvError_BadParameter;
        

        realsize -= tmp;
        len += tmp;
        *size = len;

        if (realsize > 0)
            NvOsThreadYield();
    }

    return NvSuccess;
}

NvError NvMMSockGetReply(NvMMSock *pSock, char *response_buf, 
                         int response_len)
{
    int len = 0;
    char *s = response_buf;
    s[0] = '\0';

    // look for /r/n/r/n
    while (len < response_len)
    {
        if (NvMMReadSock(pSock, s, 1, 2000) < 0)
        {
            return NvError_BadParameter;
        }

        s++;
        len++;

        if (s - 4 >= response_buf)
        {
            if (*(s-4) == '\r' && *(s-3) == '\n' &&
                *(s-2) == '\r' && *(s-1) == '\n')
            {
                *s = '\0';
                return NvSuccess;
            }
        }
    }
    return NvError_BadParameter;
}

NvError NvMMSockGetLine(char **linestart, int *linelen)
{
    char *s;

    if (!linestart || !linelen)
        return NvError_BadParameter;

    *linelen = 0;

    while (**linestart == '\r' || **linestart == '\n' || **linestart == '\0')
    {
        if (**linestart == '\0')
            return NvError_BadParameter;
        (*linestart)++;
    }

    s = *linestart;
    while (*s != '\r' && *s != '\n' && *s != '\0')
    {
        s++; 
        (*linelen)++;
    }
    *s = '\0';

    return NvSuccess;
}


NvError NvMMSockReadLine(NvMMSock *pSock, char *response_buf, 
                        int response_len)
{
    int len = 0;
    char *s = response_buf;
    s[0] = '\0';

    // look for /r/n/r/n
    while (len < response_len)
    {
        if (NvMMReadSock(pSock, s, 1, 2000) < 0)
        {
            return NvError_BadParameter;
        }

        s++;
        len++;

        if (s - 2 >= response_buf)
        {
            if (*(s-2) == '\r' && *(s-1) == '\n')
            {
                if (s == response_buf + 2)
                {
                    // keep reading, but from the beginning
                    s = response_buf;
                    *s = '\0';
                    len = 0;
                }
                else
                {
                    *s = '\0';
                    return NvSuccess;
                }
            }
        }
    }
    return NvError_BadParameter;
}

NvError NvMMSockReadChunks(NvMMSock *sock, char **buffer, NvS64 *size, int timeout)
{
    NvU64 len = 0;
    int allocSize = (16 << 10) - 1;
    NvS64 chunksize = 0;
    int bufferSize = allocSize;
#define LINE_SIZE 128
    char line[LINE_SIZE];

    // completely arbitrary transfer max chunk limit of 50MB. 
#define MAX_CHUNKED_SIZE 50 * 1024 * 1024 

    *size = 0;

    // initial allocation
    *buffer = NvOsAlloc(allocSize + 1);
    if (!*buffer)
    {
        return NvError_InsufficientMemory;
    }
    NvOsMemset(*buffer, 0, allocSize + 1);
    bufferSize = allocSize;

    do
    {
        if (NvSuccess != NvMMSockReadLine(sock, line, LINE_SIZE))
            break;

        chunksize = -1;
        chunksize = NvUStrtoul(line, NULL, 16);
        if (chunksize == 0)
            return NvSuccess;
        else if (chunksize == -1)
            break;

        //while (*buffer + len + chunkSize >= bufferSize)
        while (((len + chunksize) >= bufferSize) && (bufferSize < MAX_CHUNKED_SIZE))
        {
            char *tmp = NULL;
            bufferSize += allocSize;
            tmp = NvOsRealloc(*buffer, bufferSize + 1);
            if (!tmp)
                return NvError_InsufficientMemory;
            *buffer = tmp;
        }

        if (NvSuccess != NvMMSockReadFullBuffer(sock, (char *)(*buffer + len), &chunksize, 2000))
            break;

        *(*buffer + len + chunksize) = 0;
        len += chunksize;
        *size = len;
     
    } while (bufferSize < MAX_CHUNKED_SIZE);

    return NvSuccess;
}

NvError NvMMSockConnectHTTP(char *szURI, NvS64 *contentlen, int *responsecode,
                            char **actualURL, NvMMSock **pSock, NvS64 startfrom,
                            char *contenttype, int maxcontenttypelen, int *metainterval)
{
    char *cmd;
    char *line;
    NvError e = NvError_BadParameter;
    int len = 0;
    char hostname[MAX_NVMM_HOSTNAME];
    char proto[MAX_NVMM_HOSTNAME];
    char *filename;
    char headers[MAX_NVMM_FILENAMELEN];
    int port;
    int redirectcount = 0;
    char *newurl;
#define RESPONSE_LEN 16384
    char *response_buf;
#define MAX_REDIRECTS 10
    int MSWMSP = 0;
    NvU32 retries = 3;
    int urllen;
    int cmdlen;
    int headerslen = 0;

    urllen = NvOsStrlen(szURI) + 1 ;
    cmdlen = urllen + 512; //extrabytes needed for http commands like GET

    newurl = NvOsAlloc(urllen);
    filename = NvOsAlloc(urllen + 2);  // 2 extra bytes for empty string
    cmd  = NvOsAlloc(cmdlen);
    if (!newurl || !cmd || !filename)
    {
        if (newurl)
            NvOsFree(newurl);
        if (cmd)
            NvOsFree(cmd);
        if (filename)
            NvOsFree(filename);
        return NvError_InsufficientMemory;
    }

    NvOsMemset(newurl, 0, urllen);
    NvOsMemset(filename, 0, (urllen + 2));
    NvOsMemset(cmd, 0, cmdlen );

    response_buf = NvOsAlloc(RESPONSE_LEN);
    if (!response_buf)
    {
        NvOsFree(newurl);
        NvOsFree(cmd);
        return NvError_InsufficientMemory;
    }

    NvOsMemset(response_buf, 0, RESPONSE_LEN);

    NvOsStrncpy(newurl, szURI, urllen);
    
    NV_CHECK_ERROR_CLEANUP(
        NvMMCreateSock(pSock)
    );

redirect:
    if (redirectcount > MAX_REDIRECTS)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    do
    {
        e = NvMMOpenTCP(*pSock, newurl);
        if (NvSuccess == e) 
           break;
        retries--;
    } while(retries);

    if(!retries && (NvSuccess != e))
    {
        goto fail;
    }
    NvOsMemset(hostname, 0, MAX_NVMM_HOSTNAME);
    NvOsMemset(headers, 0, MAX_NVMM_FILENAMELEN);
    NvMMSplitURL(newurl, proto, hostname, &port, filename, headers);
    headerslen = NvOsStrlen(headers);

    if (MSWMSP & 1)
    {
        NvOsSnprintf(cmd, cmdlen,
            "GET %s HTTP/1.0\r\n"
            "Host: %s\r\n"
            "Accept: */*\r\n"
            "Range: bytes=%lld-\r\n"
            "User-Agent: NSPlayer/10.0\r\n"
            "Pragma: no-cache\r\n"
            "Pragma: xPlayStrm=1\r\n"
            "Pragma: LinkBW=2147483647, AccelBW=4194304, AccelDuration=30000\r\n"
            "Pragma: rate=1.000,stream-time=0, stream-offset=4294967295:4294967295, packet-num=4294967295\r\n"
            "\r\n",
            filename,
            hostname,
            startfrom
            );
        MSWMSP &= ~1;
    }

    else
    {
        char extras[256];
        if (startfrom > 0)
        {
            NvOsSnprintf(extras, 256,
                         "Accept: */*\r\n"
                         "Icy-MetaData:%d\r\n"
                         "Range: bytes=%lld-\r\n"
                         "User-Agent: QuickTime;NvMM HTTP Client v0.1\r\n",
                         metainterval ? 1 : 0,
                         startfrom);
        }
        else
        {
            NvOsSnprintf(extras, 256,
                         "Accept: */*\r\n"
                         "Icy-MetaData:%d\r\n"
                         "User-Agent: QuickTime;NvMM HTTP Client v0.1\r\n",
                         metainterval ? 1 : 0);
        }

        if (headerslen > 0)
        {
            NvOsSnprintf(cmd, cmdlen,
                "GET %s HTTP/1.1\r\n"
                "%s\r\n"
                "Host: %s\r\n"
                "%s"
                "\r\n",
                filename,
                headers,
                hostname,
                extras);
        }
        else
        {
            NvOsSnprintf(cmd, cmdlen,
                "GET %s HTTP/1.1\r\n"
                "Host: %s\r\n"
                "%s"
                "\r\n",
                filename,
                hostname,
                extras);
        }
    }

    NvMMWriteSock(*pSock, cmd, NvOsStrlen(cmd), 3000);

    NvOsMemset(response_buf, 0, RESPONSE_LEN);

    if (NvMMSockGetReply(*pSock, response_buf, RESPONSE_LEN) != NvSuccess)
    {
        NvOsDebugPrintf("NvMMSockGetReply failed :: redirecting \n");
        NvMMCloseTCP(*pSock);
        redirectcount++;
        if (NvMMSockGetBlockActivity())
        {
            NvOsDebugPrintf("redirecting  cancelled\n");
            goto fail;
         }
        goto redirect;
    }
    line = response_buf;
    while (NvSuccess == NvMMSockGetLine(&line, &len))
    {
        // convert the response line to lowercase for comparison
        char *p = line;
        while (*p != '\0' && *p != ':')
        {
            *p = tolower(*p);
            p++;
        }

        if (!NvOsStrncmp("http", line, 4) ||
            !NvOsStrncmp("icy", line, 3))
        {
            char *p = line;
            while (*p != ' ' && *p != '\0')
                p++;
            if (*p != '\0')
            {
                *responsecode = atoi(p);
            }

            if (!NvOsStrncmp("icy", line, 3))
            {
                if (contenttype)
                    NvOsStrncpy(contenttype, "audio/mpeg", maxcontenttypelen);
            }
        }
        else if (!NvOsStrncmp("icy-metaint:", line, 12))
        {
            char *p = line + 12;

            if (*p == ' ')
                p++;

            if ((*p != '\0') && (metainterval))
            {
                *metainterval = atoi(p);
            }
        }
        else if (!NvOsStrncmp("content-length: ", line, 16))
        {
            char *p = line + 16;
            // atoll - converts *str into a long long int value ...
            *contentlen = NvUStrtoull(p, NULL, 10); //_atoi64(p); //atoll(p);
        }
        else if (!NvOsStrncmp("transfer-encoding: ", line, 19))
        {
            char *p = line + 19;

            while (*p != '\0')
            {
                *p = tolower(*p);
                p++;
            }

            p = line + 19;

            if (strstr(p, "chunked") && *contentlen == 0)
            {
                *contentlen = -1;
            }
        }
        else if (!NvOsStrncmp("content-type: ", line, 14))
        {
            char *p = line + 14;

            while (*p != '\0')
            {
                *p = tolower(*p);
                p++;
            }

            p = line + 14;

            if (contenttype)
                NvOsStrncpy(contenttype, p, maxcontenttypelen);
            
            if (!NvOsStrncmp(p, "audio/x-ms-wax", 14) ||
                !NvOsStrncmp(p, "video/x-ms-wvx", 14) ||
                (!NvOsStrncmp(p, "video/x-ms-asf", 14) &&
                    strstr(szURI, "MSWMExt=.asf")))
                MSWMSP |= 3;
        }
        else if (!NvOsStrncmp("location: ", line, 10))
        {
            char *p = line + 10;
            if (*responsecode >= 300 && *responsecode < 400)
            {
                urllen = NvOsStrlen(p) + 1;
                if (headerslen > 0)
                    urllen += headerslen + 16;
                cmdlen = urllen + 128;

                NvOsFree(newurl);
                NvOsFree(cmd);

                newurl = NvOsAlloc(urllen);
                cmd  = NvOsAlloc(cmdlen);
                if (!newurl || !cmd)
                {
                    if (newurl)
                        NvOsFree(newurl);
                    return NvError_InsufficientMemory;
                }

                NvOsMemset(newurl, 0, urllen);
                NvOsMemset(cmd, 0, cmdlen);

                if (headerslen > 0)
                {
                    NvOsSnprintf(newurl, urllen, "%sNV_CK_HEADER%s",
                                 p, headers);
                }
                else
                {
                    NvOsStrncpy(newurl, p, urllen);
                }
            }
        }
        line += len + 1;
    }

    if (*responsecode >= 300 && *responsecode < 400)
    {
        NvMMCloseTCP(*pSock);
        redirectcount++;
        if (NvMMSockGetBlockActivity())
        {
            NvOsDebugPrintf("redirecting  cancelled\n");
            goto fail;
        }
        goto redirect;
    }
    else if (*responsecode >= 400 && *responsecode <= 510)
    {
        NvOsDebugPrintf("Network error response status code is %x\n", *responsecode);
        e = NvError_BadParameter;
        goto fail;
    }

    if (MSWMSP & 1)
    {
        NvMMCloseTCP(*pSock);
        
        *contentlen = -2;
        redirectcount++;
        if (NvMMSockGetBlockActivity())
        {
            NvOsDebugPrintf("redirecting  cancelled\n");
            goto fail;
        }
        goto redirect;
    }

    if (MSWMSP & 2)
    {
        if (contenttype)
            NvOsStrncpy(contenttype, "audio/x-ms-wax", 15);
    }

    *actualURL = newurl;
    NvOsFree(response_buf);
    NvOsFree(cmd);
    NvOsFree(filename);
    return NvSuccess;

fail:
    NvOsFree(newurl);
    NvOsFree(cmd);
    NvOsFree(response_buf);
    NvOsFree(filename);
    if (*pSock)
    {
        NvMMCloseTCP(*pSock);
        NvMMDestroySock(*pSock);
        *pSock = NULL;
    }
    return e;
}


NvError NvMMSockConnectHTTPPOST(char *szURI,NvU32 cchUrl, NvS64 *contentlen, NvU32 *responsecode,
                            char **actualURL, NvMMSock **pSock, NvS64 startfrom,
                            char *contenttype, NvU32 maxcontenttypelen, NvU32 *metainterval,char  *pszChallenge,NvU32 cchChallenge)
{
    char cmd[2048];
    char *line;
    NvError e = NvError_BadParameter;
    int len = 0;
    char hostname[MAX_NVMM_HOSTNAME];
    char proto[MAX_NVMM_HOSTNAME];
    char filename[MAX_NVMM_FILENAMELEN];
    int port;
    int RedirectResCount = 0;
    char *newurl;
#define RESPONSE_LEN 16384
    char *response_buf;
#define MAX_URL 4096
#define MAX_REDIRECTS 10
    NvU32 RetriesForOpen = 3;

    newurl = NvOsAlloc(cchUrl);
    if (!newurl)
        return NvError_InsufficientMemory;
    NvOsMemset(newurl, 0, cchUrl);

    response_buf = NvOsAlloc(RESPONSE_LEN);
    if (!response_buf)
    {
        NvOsFree(newurl);
        return NvError_InsufficientMemory;
    }

    NvOsMemset(response_buf, 0, RESPONSE_LEN);

    NvOsStrncpy(newurl, szURI, (size_t)cchUrl);

    NV_CHECK_ERROR_CLEANUP(
        NvMMCreateSock(pSock)
    );

Redirect:
    if (RedirectResCount > MAX_REDIRECTS)
    {
        e = NvError_BadParameter;
        goto fail;
    }
    do
    {
        e = NvMMOpenTCP(*pSock, newurl);
        if (NvSuccess == e) 
           break;
        NvOsDebugPrintf("Connection failure::Retrying..\n");
        RetriesForOpen--;
    } while(RetriesForOpen);

    NvOsMemset(hostname, 0, MAX_NVMM_HOSTNAME);
    NvOsMemset(filename, 0, MAX_NVMM_FILENAMELEN);
    NvMMSplitURL(newurl, proto, hostname, &port, filename, NULL);
    NvOsMemset(cmd, 0, 2048);
    NvOsSnprintf(cmd, 2048, 
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Length: %d\r\n"
        "Accept: */*\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "User-Agent: Windows-Media-DRM/11.0.5721.5145\r\n"
        "Connection: Keep-Alive\r\n"
        "Cache-Control: no-cache\r\n"
        "\r\n"
        "\r\n",
        filename,
        hostname,
        cchChallenge + NvOsStrlen("\r\n")
        );

    NvMMWriteSock(*pSock, cmd, NvOsStrlen(cmd),4000);
    NvMMWriteSock(*pSock, pszChallenge, cchChallenge,4000);
    NvMMWriteSock(*pSock, "\r\n", NvOsStrlen("\r\n"), 4000);


    NvOsMemset(response_buf, 0, RESPONSE_LEN);

    if (NvMMSockGetReply(*pSock, response_buf, RESPONSE_LEN) != NvSuccess)
    {
        NvMMCloseTCP(*pSock);
        RedirectResCount++;
        if (NvMMSockGetBlockActivity())
        {
            NvOsDebugPrintf("redirecting  cancelled\n");
            goto fail;
        }
        goto Redirect;
    }
    line = response_buf;
    while (NvSuccess == NvMMSockGetLine(&line, &len))
    {
        // convert the response line to lowercase for comparison
        char *p = line;
        while (*p != '\0')
        {
            *p = tolower(*p);
            p++;
        }

        if (!NvOsStrncmp("http", line, 4) ||
            !NvOsStrncmp("icy", line, 3))
        {
            char *p = line;
            while (*p != ' ' && *p != '\0')
                p++;
            if (*p != '\0')
            {
                *responsecode = atoi(p);
            }
            if (!NvOsStrncmp("icy", line, 3))
            {
                if (contenttype)
                    NvOsStrncpy(contenttype, "audio/mpeg", maxcontenttypelen);
            }
        }
        else if (!NvOsStrncmp("icy-metaint:", line, 12))
        {
            char *p = line + 12;

            if (*p == ' ')
                p++;

            if ((*p != '\0') && (metainterval))
            {
                *metainterval = atoi(p);
            }
        }
        else if (!NvOsStrncmp("content-length: ", line, 16))
        {
            char *p = line + 16;
            *contentlen = NvUStrtoull(p, NULL, 10); //_atoi64(p) or atoll(p);
        }
        else if (!NvOsStrncmp("content-type: ", line, 14))
        {
            char *p = line + 14;
            if (contenttype)
                NvOsStrncpy(contenttype, p, maxcontenttypelen);
            
        }
        else if (!NvOsStrncmp("location: ", line, 10))
        {
            char *p = line + 10;
            if (*responsecode >= 300 && *responsecode < 400)
                NvOsStrncpy(newurl, p, MAX_URL);
        }
        line += len + 1;
    }
    if (*responsecode >= 300 && *responsecode < 400)
    { 
        RedirectResCount++;
        if (NvMMSockGetBlockActivity())
        {
            NvOsDebugPrintf("redirecting  cancelled\n");
            goto fail;
        }
        goto Redirect;
    }
    else if (*responsecode >= 400 && *responsecode < 500)
    {
        e = NvError_BadParameter;
        goto fail;
    }
    *actualURL = newurl;
    NvOsFree(response_buf);
    return NvSuccess;
fail:
    NvOsFree(newurl);
    NvOsFree(response_buf);
    if (*pSock)
    {
        NvMMCloseTCP(*pSock);
        NvMMDestroySock(*pSock);
        *pSock = NULL;
    }
    return e;
}

NvError NvMMSockPOSTHTTPFile(char  *pwszUrl,NvU32 cchUrl, char   *pszChallenge,NvU32 cchChallenge,pfnPostCallback postCalback)
{
    NvError Status = NvError_BadParameter;
    char *newurl = NULL;
    NvS64 contentlen = 0;
    NvU32 responsecode = 0;
    NvMMSock *pSock = NULL;
    char *buffer =NULL;

    Status = NvMMSockConnectHTTPPOST(pwszUrl, cchUrl, &contentlen, &responsecode,
                            &newurl, &pSock, 0, NULL, 0, NULL,pszChallenge,cchChallenge);

    if (newurl)
        NvOsFree(newurl);

    if (NvSuccess != Status || contentlen == 0 || contentlen < -1)
        return Status;
   if (contentlen == -1) // chunked
   {
        Status = NvMMSockReadChunks(pSock,&buffer, &contentlen, 5000);
   }
   else
   {
        buffer = NvOsAlloc((size_t)contentlen + 1);
        if (!buffer)
        {
            Status = NvError_InsufficientMemory;
            goto fail;
        }
        NvOsMemset(buffer, 0, (size_t)contentlen + 1);
        Status = NvMMSockReadFullBuffer(pSock, buffer, &contentlen, 5000);
        if(Status != NvSuccess)
           return Status;
    }
    if(postCalback)
       postCalback(buffer,contentlen);
    NvOsFree(buffer);
fail:
    if (pSock)
    {
        NvMMCloseTCP(pSock);
        NvMMDestroySock(pSock);
    }
    return Status;
}

NvError NvMMSockGetHTTPFile(char *szURI, char **buffer, NvS64 *buflen)
{
    NvError e = NvError_BadParameter;
    char *newurl = NULL;
    NvS64 contentlen = 0;
    int responsecode = 0;
    NvMMSock *pSock = NULL;
    e = NvMMSockConnectHTTP(szURI, &contentlen, &responsecode,
                            &newurl, &pSock, 0, NULL, 0, NULL);
    if (newurl)
        NvOsFree(newurl);

    if (NvSuccess != e || contentlen == 0 || contentlen < -1)
        return e;

    if (contentlen == -1) // chunked
    {
        e = NvMMSockReadChunks(pSock, buffer, &contentlen, 5000);
    }
    else
    {
        *buffer = NvOsAlloc((size_t)contentlen + 1);
        if (!*buffer)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }

        NvOsMemset(*buffer, 0, (size_t)contentlen + 1);
        e = NvMMSockReadFullBuffer(pSock, *buffer, &contentlen, 5000);
    }
    *buflen = contentlen;
fail:
    if (pSock)
    {
        NvMMCloseTCP(pSock);
        NvMMDestroySock(pSock);
    }
    return e;
}

static char pSockUtilUA[256] = { 0 };
static NvU32 nUAStrLen = 0;

NvError NvMMSetUserAgentString(char* pUserAgentStr)
{
    NvError status = NvSuccess;
    NvU32 len = 0;

    if(!pUserAgentStr)
    {
         status = NvError_BadParameter;
         goto cleanup;
    }

    len = NvOsStrlen(pUserAgentStr);
    if((len == 0) || (len > 255))
    {
        status = NvError_BadValue;
        goto cleanup;
    }

    nUAStrLen = len;
    NvOsMemset(pSockUtilUA, 0, 256);
    NvOsStrncpy(pSockUtilUA, pUserAgentStr, nUAStrLen);
    NvOsDebugPrintf("NvMMSetUserAgentString:: Len: %d: String: %s", nUAStrLen, pSockUtilUA);

cleanup:
    return status;

}

NvError NvMMGetUserAgentString(NvMMSock *pSock, char **pUserAgentStr, NvU32 *pUserAgentStrlen)
{
    *pUserAgentStr = NULL;
    *pUserAgentStrlen = 0;
    if(nUAStrLen)
    {
        *pUserAgentStr = pSockUtilUA;
        *pUserAgentStrlen = nUAStrLen;
    }
    return NvSuccess;
}

static char pSockUtilUAProf[256] = { 0 };
static NvU32 nUAProfStrLen = 0;

NvError NvMMSetUAProfAgentString(char* pUserAgentProfStr)
{
    NvError status = NvSuccess;
    NvU32 len = 0;

    if(!pUserAgentProfStr)
    {
         status = NvError_BadParameter;
         goto cleanup;
    }

    len = NvOsStrlen(pUserAgentProfStr);
    if((len == 0) || (len > 255))
    {
        status = NvError_BadValue;
        goto cleanup;
    }

    nUAProfStrLen = len;
    NvOsMemset(pSockUtilUAProf, 0, 256);
    NvOsStrncpy(pSockUtilUAProf, pUserAgentProfStr, nUAProfStrLen);
    NvOsDebugPrintf("NvMMSetUAProfAgentString:: Len: %d: String: %s", nUAProfStrLen, pSockUtilUAProf);

cleanup:
    return status;

}

NvError NvMMGetUAProfAgentString(NvMMSock *pSock, char **pUserAgentProfStr, NvU32 *pUserAgentProfStrlen)
{
    *pUserAgentProfStr = NULL;
    *pUserAgentProfStrlen = 0;
    if(nUAProfStrLen)
    {
        *pUserAgentProfStr = pSockUtilUAProf;
        *pUserAgentProfStrlen = nUAProfStrLen;
    }
    return NvSuccess;
}


static int sBlockSocketStuff = 0;

void NvMMSockSetBlockActivity(int block)
{
    NvOsDebugPrintf("setting block activity: %d\n", block);
    sBlockSocketStuff = block;
}

int NvMMSockGetBlockActivity(void)
{
    //NvOsDebugPrintf("getting block activity: %d\n", sBlockSocketStuff);
    return sBlockSocketStuff;
}

