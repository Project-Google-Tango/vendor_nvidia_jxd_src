/* Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "nvcustomprotocol.h"
#include "nvmm_protocol_builtin.h"
#include "nvmm_sock.h"
#include "nvos.h"
#include "nvassert.h"
#include "nvmm_file_util.h"
#include "nvmm_logger.h"
#include "nvmm_common.h"

#define NVLOG_CLIENT NVLOG_CONTENT_PIPE // required for logging contentpipe traces

typedef enum
{
    eHTTPType_standard = 0,
    eHTTPType_ms,
} EHTTPType;

typedef enum
{
    eMSWMSPPacketType_None      = 0,
    eMSWMSPPacketType_M         = 0x4D,
    eMSWMSPPacketType_H         = 0x48,
    eMSWMSPPacketType_D         = 0x44,
    eMSWMSPPacketType_E         = 0x45,
    eMSWMSPPacketType_Invalid   = 0x100,
} EMSWMSPPacketType;

typedef struct HTTPProtoHandle
{
    NvMMSock *pSock;
    NvU64 nSize;

    NvU64 nPosition;

#define MAX_URL 4096
    char url[MAX_URL];

    NvBool bAllowSeeking;

    EHTTPType eHTTPType;
    EMSWMSPPacketType eMSWMSPPacketType;
    NvS64 nMSWMSPBytesToRead;
    int lastH_Packet;
    NvS64 nMinimumDataPacketSize;
    NvS64 nMSWMSPBytesToPad;
    int nMetaDataInterval;
} HTTPProtoHandle;

static NvError Reconnect(HTTPProtoHandle *pHTTP);

static NvError HTTPProtoGetVersion(NvS32 *pnVersion)
{
    *pnVersion = NV_CUSTOM_PROTOCOL_VERSION;
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "++--HTTPProtoGetVersion = NV_CUSTOM_PROTOCOL_VERSION"));    
    return NvSuccess;
}

static NvError HTTPProtoOpen(NvCPRHandle* hHandle, char *szURI, 
                              NvCPR_AccessType eAccess)
{
    HTTPProtoHandle *pHTTP = NULL;
    NvError Status;
    char *newurl = NULL;
    NvS64 contentlen = 0;
    int responsecode = 0;
    char contenttype[128] = {0};

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "++HTTPProtoOpen "));

    NVMM_CHK_ARG (eAccess == NvCPR_AccessRead);
    
    pHTTP = NvOsAlloc(sizeof(HTTPProtoHandle));
    NVMM_CHK_MEM(pHTTP);

    NvOsMemset(pHTTP, 0, sizeof(HTTPProtoHandle));
    NvOsStrncpy(pHTTP->url, szURI, NvOsStrlen(szURI));

    Status = NvMMSockConnectHTTP(pHTTP->url, &contentlen, &responsecode,
                                 &newurl, &pHTTP->pSock, 0,
                                 contenttype, 128, &pHTTP->nMetaDataInterval);

    if (!NvOsStrncmp(contenttype, "audio/x-ms-wax", 14) ||
        !NvOsStrncmp(contenttype, "video/x-ms-wvx", 14))
    {
        pHTTP->eHTTPType = eHTTPType_ms;
    }

    if (newurl)
    {
        NvOsMemset(pHTTP->url, 0, MAX_URL);
        NvOsStrncpy(pHTTP->url, newurl, NvOsStrlen(newurl));
        NvOsFree(newurl);
    }
    
    if (contentlen <= 0)
    {
        pHTTP->bAllowSeeking = NV_FALSE;
        pHTTP->nSize = -1;
    }
    else
    {
        pHTTP->bAllowSeeking = NV_TRUE;
        pHTTP->nSize = contentlen;
    }

    // Server chokes on HTTP/1.1 Range requests.
    if ((strstr(pHTTP->url, "last.fm")) ||
        (strstr(pHTTP->url, "cyworld.com")))
    {
        pHTTP->bAllowSeeking = NV_FALSE;
    }

    pHTTP->nPosition = 0;

    if (Status != NvSuccess)
        goto cleanup;

    *hHandle = pHTTP;
    
cleanup:
    if (Status != NvSuccess)
    {
        NvOsFree(pHTTP);
    }
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "--HTTPProtoOpen - Status =0x%x ", Status));    
    return Status;
}

static NvError HTTPProtoClose(NvCPRHandle hContent)
{
    HTTPProtoHandle *pHTTP = (HTTPProtoHandle *)hContent;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "++HTTPProtoClose"));
    
    if (pHTTP)
    {
        if (pHTTP->pSock)
        {
            NvMMCloseTCP(pHTTP->pSock);
            NvMMDestroySock(pHTTP->pSock);
        }

        NvOsFree(pHTTP);    
    }

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "--HTTPProtoClose"));
    return NvSuccess;
}

static NvError HTTPProtoSetPosition(NvCPRHandle hContent, 
                                     NvS64 nOffset, 
                                     NvCPR_OriginType eOrigin)
{
    
    NvError status = NvSuccess;
    HTTPProtoHandle *pHTTP = (HTTPProtoHandle *)hContent;
    NvS64 nRealOffset = 0;
    char *newurl = NULL;
    NvS64 contentlen = 0;
    int responsecode = 0;

    NV_ASSERT(pHTTP);

    if (!pHTTP->bAllowSeeking)
    {
        return NvError_NotSupported;
    }

    if (eOrigin == NvCPR_OriginBegin)
        nRealOffset = nOffset;
    else if (eOrigin == NvCPR_OriginCur)
        nRealOffset = pHTTP->nPosition + nOffset;
    else if (eOrigin == NvCPR_OriginEnd && pHTTP->nSize > 0)
        nRealOffset = pHTTP->nSize + nOffset;
    else
        return NvError_NotSupported;

    if (pHTTP->nSize <= (NvU64)nRealOffset)
    {
        return NvError_BadParameter;
    }

    if (pHTTP->nPosition == (NvU64)nRealOffset)
    {
        return NvSuccess;
    }

    NvMMCloseTCP(pHTTP->pSock);
    NvMMDestroySock(pHTTP->pSock);
    pHTTP->pSock = NULL;

    status = NvMMSockConnectHTTP(pHTTP->url, &contentlen, 
                                 &responsecode, &newurl, 
                                 &pHTTP->pSock, (NvS64)nRealOffset,
                                 NULL, 0, NULL);
    
  
    if (newurl)
        NvOsFree(newurl);

    if (status == NvSuccess)
    {
        pHTTP->nPosition = nRealOffset;
        
    }

    return status;
}

static NvError HTTPProtoGetPosition(NvCPRHandle hContent, 
                                     NvU64 *pPosition)
{
    HTTPProtoHandle *pHTTP = (HTTPProtoHandle *)hContent;

    NV_ASSERT(pHTTP);

    *pPosition = pHTTP->nPosition;
    return NvSuccess;
}

static int HTTPProtoMSWMSPIsValidPacketType(int type)
{
    return (int)(type == eMSWMSPPacketType_M ||
        type == eMSWMSPPacketType_H ||
        type == eMSWMSPPacketType_D ||
        type == eMSWMSPPacketType_E);
}

static NvError HTTPProtoMSWMSPReadPacketHeader(HTTPProtoHandle *pHTTP, int timeout)
{
    NvS64 bytesread = 4;
    NvU8 readbuf[8] = {0};
    NvError status;

    status = NvMMSockReadFullBuffer(pHTTP->pSock, (char*)readbuf, &bytesread, timeout);
    if (status != NvSuccess)
        return status;

    if ((readbuf[0] & 0x7f) != 0x24)
        return NvError_BadParameter;

    if (!HTTPProtoMSWMSPIsValidPacketType(readbuf[1]))
    {
        pHTTP->eMSWMSPPacketType = eMSWMSPPacketType_Invalid;
        pHTTP->nMSWMSPBytesToRead = 0;
        return NvError_BadParameter;
    }
    pHTTP->eMSWMSPPacketType = readbuf[1];

    pHTTP->nMSWMSPBytesToRead = readbuf[2] + (readbuf[3] << 8);

    if (pHTTP->eMSWMSPPacketType == eMSWMSPPacketType_H ||
        pHTTP->eMSWMSPPacketType == eMSWMSPPacketType_D)
    {
#if 0
        int LocationId;
        int Incarnation;
        int AFFlags;
        int PacketSize;
#endif
        bytesread = 8;
        
        status = NvMMSockReadFullBuffer(pHTTP->pSock, (char*)readbuf, &bytesread, timeout);
        if (status != NvSuccess)
            return status;
#if 0
        LocationId = readbuf[0] + (readbuf[1] << 8) + (readbuf[2] << 16) + (readbuf[3] << 24);
        Incarnation = readbuf[4];
        AFFlags = readbuf[5];
        PacketSize = readbuf[6] + (readbuf[7] << 8);
#endif
        pHTTP->nMSWMSPBytesToRead -= bytesread;
        if (pHTTP->eMSWMSPPacketType == eMSWMSPPacketType_H)
        {
            pHTTP->lastH_Packet = !!(readbuf[5] & 0x08);
        }
        else if (pHTTP->eMSWMSPPacketType == eMSWMSPPacketType_D)
        {
            pHTTP->nMSWMSPBytesToPad = pHTTP->nMinimumDataPacketSize - pHTTP->nMSWMSPBytesToRead;

            // to turn on padding, just uncomment the line below
            if (pHTTP->nMSWMSPBytesToPad < 0)
                pHTTP->nMSWMSPBytesToPad = 0;
        }
    }

    return NvSuccess;
}

static NvU32 HTTPProtoMSWMSPRead(HTTPProtoHandle *pHTTP, char *pData, NvS64 *pbytesread, int timeout)
{
    NvS64 bytesremain = *pbytesread;
    NvS64 bytesdesire = *pbytesread;
    NvError status;
    char readbuf[8] = {0};
    NvS64 bytesread;
     NvBool BroadCast=NV_FALSE;

    NV_ASSERT(pHTTP);

    *pbytesread = 0;
    while (bytesremain > 0)
    {
        switch (pHTTP->eMSWMSPPacketType)
        {
        case eMSWMSPPacketType_M:
            {
                if (pHTTP->nMSWMSPBytesToRead > 0)
                {
                    while (pHTTP->nMSWMSPBytesToRead > 8)
                    {
                        bytesread = 8;
                        status = NvMMSockReadFullBuffer(pHTTP->pSock, readbuf, &bytesread, timeout);
                        if (status != NvSuccess)
                            return status;
                        pHTTP->nMSWMSPBytesToRead -= bytesread;
                    }
                    bytesread = pHTTP->nMSWMSPBytesToRead;
                    status = NvMMSockReadFullBuffer(pHTTP->pSock, readbuf, &bytesread, timeout);
                    if (status != NvSuccess)
                        return status;
                    pHTTP->nMSWMSPBytesToRead = 0;
                }
                status = HTTPProtoMSWMSPReadPacketHeader(pHTTP, timeout);
                if (status != NvSuccess)
                    return status;
            }
            break;

        case eMSWMSPPacketType_H:
            {
                if (pHTTP->nMSWMSPBytesToRead > 0)
                {
                    bytesread = (pHTTP->nMSWMSPBytesToRead <= bytesremain) ? 
                        pHTTP->nMSWMSPBytesToRead : bytesremain;
                    status = NvMMSockReadFullBuffer(pHTTP->pSock, pData + *pbytesread, &bytesread, timeout);
                    if (status != NvSuccess)
                        return status;
                    *pbytesread += bytesread;
                    bytesremain -= bytesread;
                    pHTTP->nMSWMSPBytesToRead -= bytesread;
                }

                if (pHTTP->nMSWMSPBytesToRead <= 0)
                {
                    if (pHTTP->lastH_Packet)
                    {
                        char FilePropertiesObject[] =
                        { 0xA1, 0xDC, 0xAB, 0x8C, 0x47, 0xA9, 0xCF, 0x11, 0x8E, 0xE4, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65 };
                        char *ptr = pData + 30;
                        char *ptrEnd = pData + bytesdesire - 16;
                        int size;
                        int found = 0;

                        while (ptr < ptrEnd)
                        {
                            if (memcmp(ptr, FilePropertiesObject, 16) == 0)
                            {
                                found = 1;
                                break;
                            }
                            ptr += 16;
                            size = ptr[0] + (ptr[1] << 8) + (ptr[2] << 16) + (ptr[3] << 24);
                            ptr += size - 16;
                        }

                        if (found && ptr + 96 <= pData + bytesdesire)
                        {
                            ptr += 88;
                            BroadCast=(ptr[0] & 0x01);
                            ptr += 4;
                            // take only 4 bytes
                            pHTTP->nMinimumDataPacketSize = ptr[0] + (ptr[1] << 8) + (ptr[2] << 16) + (ptr[3] << 24);
                            if (BroadCast==NV_TRUE)
                                pHTTP->nMinimumDataPacketSize -= 3;
                        }
                    }
                    status = HTTPProtoMSWMSPReadPacketHeader(pHTTP, timeout);
                    if (status != NvSuccess)
                        return status;
                }
            }
            break;

        case eMSWMSPPacketType_D:
            {
                if (pHTTP->nMSWMSPBytesToRead > 0)
                {
                    bytesread = (pHTTP->nMSWMSPBytesToRead <= bytesremain) ? 
                        pHTTP->nMSWMSPBytesToRead : bytesremain;
                    status = NvMMSockReadFullBuffer(pHTTP->pSock, pData + *pbytesread, &bytesread, timeout);
                    if (status != NvSuccess)
                        return status;
                    *pbytesread += bytesread;
                    bytesremain -= bytesread;
                    pHTTP->nMSWMSPBytesToRead -= bytesread;
                }
                else if (pHTTP->nMSWMSPBytesToPad > 0)
                {
                    bytesread = (pHTTP->nMSWMSPBytesToPad <= bytesremain) ? 
                        pHTTP->nMSWMSPBytesToPad : bytesremain;
                    *pbytesread += bytesread;
                    bytesremain -= bytesread;
                    pHTTP->nMSWMSPBytesToPad -= bytesread;
                }

                if (pHTTP->nMSWMSPBytesToRead <= 0 && pHTTP->nMSWMSPBytesToPad <= 0)
                {
                    status = HTTPProtoMSWMSPReadPacketHeader(pHTTP, timeout);
                    if (status != NvSuccess)
                        return status;
                }
            }
            break;

        case eMSWMSPPacketType_E:
            {
#if 0
                if (pHTTP->nMSWMSPBytesToRead > 0)
                {
                    bytesread = pHTTP->nMSWMSPBytesToRead;
                    status = NvMMSockReadFullBuffer(pHTTP->pSock, readbuf, &bytesread, timeout);
                    if (status != NvSuccess)
                        return status;
                    pHTTP->nMSWMSPBytesToRead -= bytesread;
                }

                status = HTTPProtoMSWMSPReadPacketHeader(pHTTP, timeout);
                if (status != NvSuccess)
                    return status;
#endif
                //NvOsDebugPrintf("MSWMSP read: ended at %f\n", (float)(NvOsGetTimeUS()/1000000.0));
                return NvSuccess;
            }
 //           break;

        case eMSWMSPPacketType_None:
            {
                //NvOsDebugPrintf("MSWMSP read: started at %f\n", (float)(NvOsGetTimeUS()/1000000.0));
                status = HTTPProtoMSWMSPReadPacketHeader(pHTTP, timeout);
                if (status != NvSuccess)
                    return status;
            }
            break;
        case eMSWMSPPacketType_Invalid:
        default:
            return NvError_BadParameter;
      //      break;
        }
    }

    return NvSuccess;
}

static NvU32 HTTPProtoRead(NvCPRHandle hContent, NvU8 *pData, NvU32 nSize)
{
    NvS64 bytesread = nSize;
    NvError status;
    HTTPProtoHandle *pHTTP = (HTTPProtoHandle *)hContent;
#define MAX_RETRIES 3
    NvU32 ReconnectRetries = MAX_RETRIES;
    NvU32 ReadRetries = 0;

    NV_ASSERT(pHTTP);
    
retry:
    if (ReadRetries > MAX_RETRIES)
    {
        return 0;
    }

    if (pHTTP->eHTTPType == eHTTPType_ms)
    {
        status = HTTPProtoMSWMSPRead(pHTTP, (char *)pData, (NvS64*)&bytesread, 60000);
    }
    else
    {
        status = 
            NvMMSockReadFullBuffer(pHTTP->pSock, (char *)pData, &bytesread, 60000);
    }
    if (status != NvSuccess)
    {
        if ((status == NvError_EndOfFile) && (pHTTP->nSize == (pHTTP->nPosition + bytesread)))
        {
            //NvOsDebugPrintf("NvMMSockReadFullBuffer::EOF \n");
            return 0;
        }
        else
        {
            do
            {
                status = Reconnect(pHTTP);
                if (NvSuccess == status) 
                   break;
                NvOsDebugPrintf("Reconnect failure::Retrying..\n");
                ReconnectRetries--;
            } while(ReconnectRetries);

            if(!ReconnectRetries && (NvSuccess != status))
            {
                NvOsDebugPrintf("Reconnect failure::Retries failed\n");
                return 0;
            }
            ReadRetries++;
            goto retry;
        }
    }
    pHTTP->nPosition += bytesread;
    return (NvU32)bytesread;
}

static NvU32 HTTPProtoWrite(NvCPRHandle hContent, NvU8 *pData, NvU32 nSize)
{
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "++--HTTPProtoWrite -NOT IMPLEMENTED "));        
    return 0;
}

static NvError HTTPProtoGetSize(NvCPRHandle hContent, NvU64 *pHTTPSize)
{
    HTTPProtoHandle *pHTTP = (HTTPProtoHandle *)hContent;

    if (pHTTP)
    {
        *pHTTPSize = pHTTP->nSize;
    }

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "++--HTTPProtoGetSize - Size =0x%x ", *pHTTPSize));
    return NvSuccess;
}

static NvBool HTTPProtoIsStreaming(NvCPRHandle hContent)
{
    return NV_TRUE;
}

static NvParserCoreType HTTPProtoProbeParserType(char *szURI)
{
    NvError e = NvError_BadParameter;
    NvMMParserCoreType eMMParserType = NvMMParserCoreTypeForce32;
    NvParserCoreType eParserType;
    NvMMSuperParserMediaType eMediaType = NvMMSuperParserMediaType_Force32;
    char *newurl = NULL;
    NvS64 contentlen = 0;
    int responsecode = 0;
    NvMMSock *pSock = NULL;
#define MAX_CONTENTTYPE 128
    char ctype[MAX_CONTENTTYPE+1];
    NV_CUSTOM_PROTOCOL *proto = NULL;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "HTTPProtoProbeParserType ::URI - %s", szURI));
    
    NvOsMemset(ctype, 0, MAX_CONTENTTYPE);

    // if we know the extension, just use that
    NvMMUtilFilenameToParserType((NvU8*)szURI, &eMMParserType, &eMediaType);
    eParserType = (NvParserCoreType)eMMParserType;
    if (eParserType != NvMMParserCoreTypeForce32)
    {
        return NvParserCoreType_UnKnown;
    }

    // otherwise, check the content-type of the file from the server
    e = NvMMSockConnectHTTP(szURI, &contentlen, &responsecode,
                            &newurl, &pSock, 0, ctype, MAX_CONTENTTYPE, NULL);

    if (newurl)
        NvOsFree(newurl);

    if (pSock)
    {
        NvMMCloseTCP(pSock);
        NvMMDestroySock(pSock);
    }

    if (NvSuccess != e)
        return e;

    eParserType = NvParserCoreType_UnKnown;
    eMediaType = NvMMSuperParserMediaType_Force32;
    // Get the HTTP protocol
    NvGetHTTPProtocol(&proto);

    if (!NvOsStrcmp(ctype, "audio/mpeg"))
        eParserType = NvMMParserCoreType_Mp3Parser;
    else if (!NvOsStrcmp(ctype, "audio/3gpp") ||
             !NvOsStrcmp(ctype, "video/mp4") ||
             !NvOsStrcmp(ctype, "video/3gpp") ||
             !NvOsStrcmp(ctype, "video/quicktime"))
        eParserType = NvParserCoreType_Mp4Parser;
    else if (!NvOsStrcmp(ctype, "video/x-ms-wmv") ||
        !NvOsStrcmp(ctype, "video/x-ms-asf") ||
        !NvOsStrcmp(ctype, "audio/x-ms-wma") ||
        !NvOsStrcmp(ctype, "video/x-ms-wvx") ||
        !NvOsStrcmp(ctype, "audio/x-ms-wax"))
    {
        eParserType= NvParserCoreType_AsfParser;
    }
    else if (!NvOsStrcmp(ctype, "video/x-msvideo"))
        eParserType = NvParserCoreType_AviParser;
    else if (!NvOsStrcmp(ctype, "audio/ogg"))
        eParserType= NvParserCoreType_OggParser;
    else if (!NvOsStrcmp(ctype, "audio/wav"))
        eParserType= NvParserCoreType_WavParser;
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,
                                    "HTTPProtoProbeParserType ::Core Type - 0x%x Content type = %s", eParserType,ctype));

    return eParserType;

}

static NvError HTTPProtoQueryConfig(NvCPRHandle hContent, 
                                    NvCPR_ConfigQueryType eQuery,
                                    void *pData, int nDataSize)
{
    HTTPProtoHandle *pHTTP = (HTTPProtoHandle *)hContent;

#define MAX_CONTENTTYPE 128
    char ctype[MAX_CONTENTTYPE+1];
    
    NvOsMemset(ctype, 0, MAX_CONTENTTYPE);

    switch (eQuery)
    {
        case NvCPR_ConfigPreBufferAmount:
        {
            NvU32 prebuffersize = 32 * 1024;

            if (nDataSize != sizeof(NvU32))
                return NvError_BadParameter;

            if (pHTTP && pHTTP->nSize > 0 && pHTTP->nSize != (NvU64)-1)
            {
                if ((pHTTP->nSize / 4) < (prebuffersize * 3 / 2))
                    prebuffersize = (NvU32)(pHTTP->nSize / 4);
            }

            *((NvU32*)pData) = prebuffersize;
            return NvSuccess;
        }
        case NvCPR_ConfigCanSeekByTime:
        {
            if (nDataSize != sizeof(NvU32))
                return NvError_BadParameter;
            *((NvU32*)pData) = 0;
            return NvSuccess;
        }
        case NvCPR_ConfigGetMetaInterval:
        {
            if (nDataSize != sizeof(NvU32))
                return NvError_BadParameter;

            *((NvU32*)pData) = pHTTP->nMetaDataInterval;
            return NvSuccess;
        }
        case NvCPR_ConfigGetChunkSize:
        {
            if (nDataSize != sizeof(NvU32))
            {
                return NvError_BadParameter;
            }
            *((NvU32*)pData) = 32 * 1024;
            return NvSuccess;
        }
        default:
            break;
    }

    return NvError_NotSupported;
}

static NvError HTTPProtoSetPause(NvCPRHandle hContent, int bPause)
{
    return NvError_NotSupported;
}

static NV_CUSTOM_PROTOCOL s_HTTPProtocol =
{
    HTTPProtoGetVersion,
    HTTPProtoOpen,
    HTTPProtoClose,
    HTTPProtoSetPosition,
    HTTPProtoGetPosition,
    HTTPProtoRead,
    HTTPProtoWrite,
    HTTPProtoGetSize,
    HTTPProtoIsStreaming,
    HTTPProtoProbeParserType,
    HTTPProtoQueryConfig,
    HTTPProtoSetPause
};

void NvGetHTTPProtocol(NV_CUSTOM_PROTOCOL **proto)
{
    *proto = &s_HTTPProtocol;
}

static NvError Reconnect(HTTPProtoHandle *pHTTP)
{
    char *newurl = NULL;
    NvS64 contentlen = 0;
    int responsecode = 0;
    NvError status = NvSuccess;

    //NvOsDebugPrintf("In Reconnect \n");
    NvMMCloseTCP(pHTTP->pSock);
    NvMMDestroySock(pHTTP->pSock);
    pHTTP->pSock = NULL;

    status = NvMMSockConnectHTTP(pHTTP->url, &contentlen, 
                                 &responsecode, &newurl, 
                                 &pHTTP->pSock, (NvS64)pHTTP->nPosition,
                                 NULL, 0, NULL);
    if (newurl)
        NvOsFree(newurl);
    return status;
}

