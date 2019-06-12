/* Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "rtsp.h"
#include "rtp_audio.h"
#include "nvmm_logger.h"
#include "nvmm_common.h"

#ifdef _WIN32
//#pragma warning( disable : 4996 )
#endif

#define NVLOG_CLIENT NVLOG_CONTENT_PIPE // required for logging contentpipe traces

#define NV_ENABLE_RECEIVEREPORTS 1

typedef  struct Node {
    RTPPacket   *packet;
    struct Node *next;
}Node;

typedef  struct List{
    Node *Head;
    NvU32 size;
    NvBool bAllowSameSeq;
    NvOsMutexHandle hListMutexLock;         /* mutex lock for queue */
}List;

NvError GetNextPacketFromList(RTSPSession *pServer, RTPPacket *pPacket, NvBool bNoRetry);

NvError CreateRTSPSession(RTSPSession **pServer)
{
    NvError Status = NvSuccess;

    RTSPSession *serv = NULL;
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "++CreateRTSPSession"));

    NVMM_CHK_ARG(pServer);

    serv = NvOsAlloc(sizeof(RTSPSession));
    NVMM_CHK_MEM(serv);

    NvOsMemset(serv, 0, sizeof(RTSPSession));
    *pServer = serv;

cleanup:
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "--CreateRTSPSession:: Status = 0x%x Server = 0x%x", Status, serv));
    return Status;
}

void DestroyRTSPSession(RTSPSession *pServer)
{
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "++--DestroyRTSPSession"));

    NvOsFree(pServer);
}

NvError ConnectToRTSPServer(RTSPSession *pServer, const char *url)
{
    NvError Status = NvSuccess;
    NvU32 retries = 3;
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "++ConnectToRTSPServer"));

    NVMM_CHK_ARG(!pServer->controlSock);

    NVMM_CHK_ERR(NvMMCreateSock(&pServer->controlSock));

    NVMM_CHK_ERR(NvOsMutexCreate(&pServer->controlSockLock));

    NvOsStrncpy(pServer->url, (char*)url, MAX_URL);
    NvOsStrncpy(pServer->controlUrl, (char*)url, MAX_CONTROLURL);
    pServer->nDuration = -1;
    pServer->sequence_id = 1;

    do
    {
        Status = NvMMOpenTCP(pServer->controlSock, pServer->url);

        if (NvSuccess == Status)
           break;
        NvOsDebugPrintf("Connection failure::Retrying..\n");
        retries--;
    } while(retries);

    if(!retries && (NvSuccess != Status))
    {
        NvOsDebugPrintf("Connection failure::Retries failed\n");
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR,"Connection failure::Retries failed"));
    }

cleanup:
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "--ConnectToRTSPServer :: Status = 0x%x, Line = %d", Status));
    return Status;
}

void CloseRTSPSession(RTSPSession *pServer)
{
    NvU32 i;
    RTPPacket packet;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "++CloseRTSPSession"));

    ShutdownRTPThread(pServer);

    for (i = 0; i < pServer->nNumStreams; i++)
    {
        RTPStream *stream = &(pServer->oRtpStreams[i]);

        if (stream->oPacketQueue)
        {
            while (NvSuccess == (Remove(stream->oPacketQueue, &packet)))
            {
                if (packet.buf)
                {
                    NvOsFree(packet.buf);
                    packet.buf = NULL;
                }
            }
        }
        if (stream->oRawPacketQueue)
        {
            while (NvSuccess == (Remove(stream->oRawPacketQueue, &packet)))
            {
                if (packet.buf)
                {
                    NvOsFree(packet.buf);
                    packet.buf = NULL;
                }
            }
        }
        DestroyList(stream->oPacketQueue);
        stream->oPacketQueue = NULL;

        DestroyList(stream->oRawPacketQueue);
        stream->oRawPacketQueue = NULL;

        CloseRTPStreams(stream);
    }
    if(pServer->pASFHeader)
        NvOsFree(pServer->pASFHeader);
    pServer->pASFHeader = NULL;

    NvMMCloseTCP(pServer->controlSock);
    NvOsMutexDestroy(pServer->controlSockLock);
    NvMMDestroySock(pServer->controlSock);
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "--CloseRTSPSession"));
    pServer->controlSock = NULL;
    pServer->bHaveAudio = 0;
    pServer->bHaveVideo = 0;
}

static NvError GetRTSPReply(RTSPSession *pServer)
{
    int len = 0;
    char *s = pServer->response_buf;
    s[0] = '\0';
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "++GetRTSPReply"));

    // look for /r/n/r/n
    while (len < RESPONSE_LEN)
    {
        if (NvMMReadSock(pServer->controlSock, s, 1, 60000) < 0)
        {
             NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "--GetRTSPReply :: Read Sock Failed"));
            return NvError_BadParameter;
        }

        s++;
        len++;

        if (s - 4 >= pServer->response_buf)
        {
            if (*(s-4) == '\r' && *(s-3) == '\n' &&
                *(s-2) == '\r' && *(s-1) == '\n')
            {
                *s = '\0';
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "--GetRTSPReply :: NvSuccess"));
                return NvSuccess;
            }
        }
    }
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "--GetRTSPReply :: NvError_BadParameter, line - %d", __LINE__));

    return NvError_BadParameter;
}

static NvError ReadFullBuffer(NvMMSock *sock, char *buffer, int size)
{
    int len = 0, tmp = 0;
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "++ReadFullBuffer :: buffer = 0x%x, size = %d", buffer, size));

    while (size > 0)
    {
        tmp = NvMMReadSock(sock, buffer + len, size, 500);
        if (tmp < 0)
        {
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR,"--ReadFullBuffer :: NvError_BadParameter:: tmp = %d", tmp));
            return NvError_BadParameter;
        }

        size -= tmp;
        len += tmp;
    }
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"--ReadFullBuffer"));
    return NvSuccess;
}

static NvError GetLine(char **linestart, int *linelen)
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

static int FromHex(char val)
{
    int ret = 0;
    val = toupper(val);
    if (val >= '0' && val <= '9')
        ret = val - '0';
    else if (val >= 'A' && val <= 'F')
        ret = val - 'A' + 10;
    return ret;
}

static const char sBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int DecodeBase64(char *out_str, char *in_str, NvU32 length)
{
    int i;
    int toIndex[256] = {0};
    char *s = NULL, *sp = NULL;
    NvU8 *hdr = (NvU8 *)out_str;
    int declen = 0;

    if (length < 1)
        return -1;

    s = NvOsAlloc(length + 1);
    if (s != NULL)
    {
        NvOsMemset(s, 0, length + 1);
        NvOsStrncpy(s, in_str, length);
        sp = s;
    }
    else
    {
        sp = NULL;
        s  = in_str;
    }
    for (i = 0; i < 64; i++)
        toIndex[(int)(sBase64[i])] = i;

    while (length > 0)
    {
        NvU8 out[3] = {0, 0, 0};
        NvU8 in[4]  = {0, 0, 0, 0};
        int numpad  = 0;

        for (i = 0; i < 4; i++)
        {
            if (length > 0)
            {
                if (*s != '=')
                    in[i] = (NvU8)(toIndex[(int)(*s)]);
                else
                    numpad++;

                s++;
                length--;
            }
        }
        out[0] = (in[0] << 2) | (in[1] >> 4);
        out[1] = (in[1] << 4) | (in[2] >> 2);
        out[2] = (in[2] << 6) | (in[3]);

        for (i = 0; i < 3 - numpad; i++)
        {
            *hdr = out[i];
            hdr++;
            declen++;
        }
    }
    if(sp)
        NvOsFree(sp);

    return declen;
}


static NvError DecodeAACHeader(RTPStream *stream)
{
        NvU8 *buffer,  *hdrstart;
        simpleBitstream bs;
        NvU32 FreqTable[] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350, -1, -1, 0 };
        NEM_AAC_FORMAT_HEADER aacTrackInfo;

        NvOsMemset(&aacTrackInfo, 0, sizeof(NEM_AAC_FORMAT_HEADER));
        stream->codecContext = NvOsAlloc(sizeof(LatmContext));
        NvOsMemset(stream->codecContext, 0, sizeof(LatmContext));

        if(!stream->codecContext)
            return NvError_InsufficientMemory;

        if(stream->mediaType== NvMimeMediaType_MP4ALATM)
        {
            //Parse and store the config
            hdrstart = stream->configData ;
            bs.buffer = hdrstart;
            bs.readBits = bs.dataWord = bs.validBits = 0;
            StreamMuxConfig(&bs, (LatmContext*)stream->codecContext);
            aacTrackInfo.objectType = ((LatmContext*)stream->codecContext)->objectType;

            aacTrackInfo.samplingFreqIndex = ((LatmContext*)stream->codecContext)->samplingFreqIndex;

            if (aacTrackInfo.samplingFreqIndex == 0xf)
                 aacTrackInfo.samplingFreq = ((LatmContext*)stream->codecContext)->samplingFreq;
            else
                 aacTrackInfo.samplingFreq = FreqTable[aacTrackInfo.samplingFreqIndex];

             aacTrackInfo.channelConfiguration = ((LatmContext*)stream->codecContext)->channelConfiguration;

            switch(aacTrackInfo.objectType)
            {
                case 1: // AAC main object
                case 2: // AAC_LC object
                     stream->eCodecType = NvStreamType_AAC;
                     NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"NvStreamType_AAC"));
                     if(stream->sbrPresent == 1)
                     {
                         stream->eCodecType = NvStreamType_AACSBR;
                         NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"NvStreamType_AACSBR"));
                     }

                     break;
                case 5: //SBR object
                     stream->eCodecType = NvStreamType_AACSBR;
                     NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"NvStreamType_AACSBR"));
                     break;
                default:
                     stream->eCodecType = NvStreamType_OTHER;
                     break;
            }

            if(!stream->eCodecType == NvStreamType_OTHER)
            {
                stream->ProcessPacket = ProcessAACLATMPacket;
                stream->maxReorderedPackets = MAX_REORDER_PACKETS_AAC;
                ((LatmContext*)stream->codecContext)->prevMarkerBit = 1;

                if (stream->configData)
                    NvOsFree(stream->configData);

                stream->configData = NvOsAlloc(sizeof(NEM_AAC_FORMAT_HEADER));
                NvOsMemset(stream->configData, 0, sizeof(NEM_AAC_FORMAT_HEADER) );
                stream->nConfigLen = sizeof(NEM_AAC_FORMAT_HEADER);
                NvOsMemcpy(stream->configData, &aacTrackInfo, sizeof(NEM_AAC_FORMAT_HEADER));
            }

         }

         else if(stream->mediaType== NvMimeMediaType_MPEG4GENERIC)
         {
            buffer = stream->configData ;
            aacTrackInfo.objectType = (buffer[0] >> 3);
            aacTrackInfo.samplingFreqIndex = (((buffer[0]) & (0x07))<<1) | (((buffer[1]) & (0x80))>>7);
            if (aacTrackInfo.samplingFreqIndex == 0xf)
            {
                aacTrackInfo.samplingFreq = (((buffer[1]) & (0x7f))<<17) | (buffer[2]<<9) | (buffer[3]<<1) | ((buffer[4] & 0x80) >> 7);
                aacTrackInfo.channelConfiguration = (((buffer[4]) & (0x78))>>3);
            }
            else
            {
                aacTrackInfo.samplingFreq = FreqTable[aacTrackInfo.samplingFreqIndex];
                aacTrackInfo.channelConfiguration = (((buffer[1]) & (0x78))>>3);
            }
            stream->ProcessPacket = ProcessAACPacket;
            stream->maxReorderedPackets = MAX_REORDER_PACKETS_AAC;
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"NvStreamType_AAC : Generic"));
            if (stream->configData)
                NvOsFree(stream->configData);

            stream->configData = NvOsAlloc(sizeof(NEM_AAC_FORMAT_HEADER));
            NvOsMemset(stream->configData, 0, sizeof(NEM_AAC_FORMAT_HEADER) );
            stream->nConfigLen = sizeof(NEM_AAC_FORMAT_HEADER);
            NvOsMemcpy(stream->configData, &aacTrackInfo, sizeof(NEM_AAC_FORMAT_HEADER));

         }

        return NvSuccess;
}

static void DecodeASFHeader(RTSPSession *pServer, char *line)
{
    NvU32 len = (int)NvOsStrlen(line);
    char *s = line;

    if (len < 1) // FIXME: minimum ASF header len?
        return;

    if(pServer->pASFHeader == NULL)
        pServer->pASFHeader = NvOsAlloc(len + 1); // actually, 3/4 of this but the extra doesn't hurt
    NvOsMemset(pServer->pASFHeader, 0, len + 1);
    pServer->nASFHeaderLen = DecodeBase64((char *)pServer->pASFHeader, s, len);
}

static NvError ParseFMTPLine(RTSPSession *pServer, RTPStream *stream,
                             char *line)
{
    char *s = line;
    NvError status  = NvSuccess;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"++ParseFMTPLine"));

    while (*s == ' ' &&  *s != '\0')
        s++;

    while (*s != '\0')
    {
        char *p = s;
        while (*p != '=' && *p != ' ' &&  *p != '\0')
        {
            *p = tolower(*p);
            p++;
        }
        if (!NvOsStrncmp("config=", s, 7))
        {
            char *config;
            int i, configlen = 0;

            s += 7;

            config = s;

            while (*s != ';' && *s != '\0')
            {
                s++;
                configlen++;
            }

            configlen = (configlen + 1) / 2;

            if (stream->configData)
                NvOsFree(stream->configData);
            stream->configData = NvOsAlloc(configlen + 1);
            NvOsMemset(stream->configData, 0, configlen + 1);
            stream->nConfigLen = configlen;

            for (i = 0; i < configlen; i++)
            {
                int tmp = 0, val;

                val = FromHex(config[0]);
                tmp = (val & 0xf) << 4;
                if (config[1] != '\0')
                {
                    val = FromHex(config[1]);
                    tmp |= (val & 0xf);
                }

                stream->configData[i] = tmp;
                config++;
                config++;
            }
        }
        else if (!NvOsStrncmp("mode=", s, 5))
        {
            s += 5;
            if (!NvOsStrncmp("AAC-hbr", s, 7) ||
                !NvOsStrncmp("AAC-lbr", s, 7))
            {
                stream->eCodecType = NvStreamType_AAC;
            }
        }
        else if (!NvOsStrncmp("sizelength=", s, 11))
        {
            s += 11;
            stream->sizelength = atoi(s);
        }
        else if (!NvOsStrncmp("indexlength=", s, 12))
        {
            s += 12;
            stream->indexlength = atoi(s);
        }
        else if (!NvOsStrncmp("indexdeltalength=", s, 17))
        {
            s += 17;
            stream->indexdeltalength = atoi(s);
        }
        else if (!NvOsStrncmp("profile-level-id=", s, 17))
        {
            s += 17;
            stream->profileLevelId= atoi(s);
        }
        else if (!NvOsStrncmp("bitrate=", s, 8))
        {
            s += 8;
            /* Bit rate information can be present part of SDP line
             * as well as FMTP line.Here in FTMP the unit is kilo
             * since we are multiplying by another 1000 in
             * GenerateHeader in nvmm_protocol_rtsp.c we are dividing
             * by 1000 here.
             */
            if(stream->bitRate == 0)
            {
                stream->bitRate= atoi(s)/1000;
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"ParseFMTPLine: Bitrate=%d\n",stream->bitRate));
            }
        }
        else if (!NvOsStrncmp("cpresent=", s, 9))
        {
            s += 9;
            stream->cpresent = atoi(s);
        }
        else if (!NvOsStrncmp("object=", s, 7))
        {
            s += 7;
            stream->object = atoi(s);
        }
        else if (!NvOsStrncmp("sbr-enabled=", s, 12))
        {
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"ParseFMTPLine: Detected SBR enabled\n"));
            s += 12;
            stream->sbrPresent = atoi(s);
        }

        // add others - split into codec specific functions?
        if(stream->ParseFMTPLine)
            stream->ParseFMTPLine(stream, s);
        // didn't match, look for next ;
        while (*s != ';' && *s != '\0')
            s++;
        if (*s == ';')
            s++;
        while (*s == ' ' && *s != '\0')
            s++;
    }

   if( (stream->mediaType== NvMimeMediaType_MP4ALATM)
         ||(stream->eCodecType == NvStreamType_AAC))
   {
        if(stream->configData)
            status = DecodeAACHeader(stream);
        else
             NvOsDebugPrintf("\n\n config paramtere not present: Parsing from packet is not supported\n\n");

    }


    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"--ParseFMTPLine"));

    return status;
}

static NvError ParseSDPLine(RTSPSession *pServer, char *line, int len)
{
    char *s = line;
    char *p;
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"++ParseSDPLine"));

    switch (*s)
    {
        case 'a':
        {
            s++; s++;
            if (!NvOsStrncmp("control:", s, 8))
            {
                char *url = pServer->controlUrl;
                if (pServer->nNumStreams > 0)
                    url = pServer->oRtpStreams[pServer->nNumStreams-1].controlUrl;

                s += 8;

                if (*s == '*')
                    break;

                p = strchr(s, ':'); // check if this is a full url or not
                if (p)
                    NvOsStrncpy(url, s, MAX_CONTROLURL);
                else
                {
                    NvU32 length = NvOsStrlen(url);
                    if (url[length-1] != '/')
                    {
                        url[length] = '/'; //strcat(url, "/");
                        length++;
                    }

                    NvOsSnprintf(url+length, MAX_CONTROLURL - length, s); //strcat(url, s);
                }
            }
            else if (!NvOsStrncmp("range:", s, 6))
            {
                s += 6;

                //FIXME: handle other formats
                if (NvOsStrncmp("npt=", s, 4))
                    break;

                s += 4;
                if ( (!NvOsStrncmp("now=", s, 4)) || (!NvOsStrncmp("now-", s, 4)))
                {
                    pServer->bLiveStreaming = NV_TRUE;
                    s += 4;
                    break;
                }

                while (*s != '-' && *s != '\0')
                    s++;

                if (*s == '\0')
                    break;

                s++;
                pServer->nDuration = (NvU64)(atof(s) * 1000);
                if (pServer->nDuration > 86400000) //24hours=86400000msec
                    pServer->nDuration = 0;

                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"nDuration = %lu",pServer->nDuration));
                if(pServer->nDuration == 0)
                {
                    pServer->bLiveStreaming = NV_TRUE;
                }
            }
            else if (!NvOsStrncmp("rtpmap:", s, 7))
            {
                int type;
                RTPStream *stream = NULL;
                NvU32 i;

                s += 7;

                type = atoi(s);

                while (*s != ' ' && *s != '\0')
                    s++;
                if (*s == '\0')
                    break;
                s++;

                for (i = 0; i < pServer->nNumStreams; i++)
                {
                    stream = &(pServer->oRtpStreams[i]);
                    if (stream->rtp_stream_type == (NvU32)type)
                        SetRTPCodec(stream, s);
                }
            }
            else if (!NvOsStrncmp("fmtp:", s, 5))
            {
                RTPStream *stream = NULL;
                NvU32 i;
                int type;

                s += 5;

                type = atoi(s);

                while (*s != ' ' && *s != '\0')
                    s++;
                if (*s == '\0')
                    break;
                s++;

                for (i = 0; i < pServer->nNumStreams; i++)
                {
                    stream = &(pServer->oRtpStreams[i]);
                    if (stream->rtp_stream_type == (NvU32)type)
                        break;
                    stream = NULL;
                }

                if (!stream)
                    break;

                ParseFMTPLine(pServer, stream, s);
            }
            else if (!NvOsStrncmp("pgmpu:", s, 6))
            {
                pServer->bIsASF = NV_TRUE;

                s += 6;
                if (!NvOsStrncmp("data:application/vnd.ms.wms-hdr.asfv1;base64,",
                                 s, 45))
                {
                    s += 45;
                    DecodeASFHeader(pServer, s);
                }
            }
            else if (!NvOsStrncmp("maxps:", s, 6))
            {
                s += 6;
                pServer->nMaxASFPacket = atoi(s);
            }
            else if (!NvOsStrncmp("Height:integer;", s, 15))
            {
                RTPStream *stream = NULL;

                s += 15;

                stream = &(pServer->oRtpStreams[pServer->nNumStreams-1]);
                stream->Height = atoi(s);
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"Height = %d",stream->Height));
            }
            else if (!NvOsStrncmp("Width:integer;", s, 14))
            {
                RTPStream *stream = NULL;

                s += 14;

                stream = &(pServer->oRtpStreams[pServer->nNumStreams-1]);
                stream->Width = atoi(s);
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"Width = %d",stream->Width));
            }
            else if (!NvOsStrncmp("AvgBitRate:integer;", s, 19))
            {
                RTPStream *stream = NULL;
                stream = &(pServer->oRtpStreams[pServer->nNumStreams-1]);
                s += 19;
                stream->bitRate = atoi(s)/1000;
            }

            /* a=cliprect:0,0,<height>,<width> Eg: a=cliprect:0,0,144,176*/
            else if (!NvOsStrncmp("cliprect:", s, 9))
            {
                RTPStream *stream = NULL;

                s += 9;
                stream = &(pServer->oRtpStreams[pServer->nNumStreams-1]);
                s += 2;
                s += 2;

                if(stream->Height == 0)
                {
                    stream->Height = atoi(s);
                    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_INFO,"cliprect:Height = %d",stream->Height));
                }

                while (*s != ',' && *s != '\0')
                    s++;

                if (*s == '\0')
                    break;
                s++;
                if(stream->Width == 0)
                {
                    stream->Width = atoi(s);
                    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_INFO,"cliprect:Width = %d",stream->Width));
                }
            }
            /* a=framesize:96 <width>-<height> Eg: a=framesize:96 176-144 */
            else if (!NvOsStrncmp("framesize:", s, 10))
            {
                RTPStream *stream = NULL;
                NvU32 type;
                NvU32 i;

                s += 10;

                stream = &(pServer->oRtpStreams[pServer->nNumStreams-1]);

                type = atoi(s);

                while (*s != ' ' && *s != '\0')
                    s++;

                if (*s == '\0')
                    break;

                s++;

                for (i = 0; i < pServer->nNumStreams; i++)
                {
                    stream = &(pServer->oRtpStreams[i]);
                    if (stream->rtp_stream_type == (NvU32)type)
                        break;
                    stream = NULL;
                }

                if(stream->Width == 0)
                {
                    stream->Width = atoi(s);
                    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_INFO,"FRAMESIZE:Width = %d",stream->Width));
                }

                while (*s != '-' && *s != '\0')
                    s++;

                if (*s == '\0')
                    break;

                s++;

                if(stream->Height == 0)
                {
                    stream->Height = atoi(s);
                    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_INFO,"FRAMESIZE:Height = %d",stream->Height));
                }
            }
            break;
        }
        case 'm':
        {
            int i = pServer->nNumStreams;
            RTPStream *stream;
            NvBool skipstream = 0,brFlag = NV_FALSE;

            if (i >= MAX_RTP_STREAMS)
                break;

            s++; s++;

            // skip type (audio/video)
            if (!NvOsStrncmp("audio ", s, 6))
            {
                if (pServer->bHaveAudio)
                    skipstream = NV_TRUE;

                pServer->bHaveAudio = NV_TRUE;
                brFlag = NV_TRUE;
                s += 6;
            }
            else if (!NvOsStrncmp("video ", s, 6))
            {
                if (pServer->bHaveVideo)
                    skipstream = NV_TRUE;

                pServer->bHaveVideo = NV_TRUE;
                brFlag = NV_TRUE;
                s += 6;
            }
            else if (!NvOsStrncmp("application ", s, 12))
            {
                s += 12;
                skipstream = NV_TRUE;
            }
            else
                break;

            stream = &(pServer->oRtpStreams[i]);
            InitRTPStream(stream);
            stream->brFlag = brFlag;
            stream->bSkipStream = skipstream;
            stream->port = atoi(s);
            stream->nStreamNum = i;

            while (*s != ' ' && *s != '\0')
                s++;

            if (*s == '\0')
                break;

            s++; // space
            if (NvOsStrncmp("RTP/AVP ", s, 8))
                return NvError_BadParameter;

            s += 8;

            stream->rtp_stream_type = atoi(s);
            NvOsStrncpy(stream->controlUrl, pServer->controlUrl, MAX_CONTROLURL);

            pServer->nNumStreams++;

            break;
        }
        case 'b':
        {
            s++; s++;

            if (NvOsStrncmp("AS:", s, 3))
            {
                break;
            }
            else
            {
                s+= 3;
                if (pServer->nNumStreams > 0)
                {
                    RTPStream *stream;
                    stream = &(pServer->oRtpStreams[pServer->nNumStreams-1]);

                    if(stream->brFlag == NV_TRUE)
                    {
                        if(stream->bitRate == 0)
                        {
                            stream->bitRate =  atoi(s);
                            stream->brFlag = NV_FALSE;
                        }
                    }
                }
            }
            s++;
            break;
        }
        case 'c':
            break; // TODO: Handle multicast
        default:
            break;
    }

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"--ParseSDPLine"));
    return NvSuccess;
}

NvError ParseSDP(RTSPSession *pServer, char *contentbuf)
{
    char *line;
    int len;
    NvError err = NvSuccess;
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"++ParseSDP"));

    line = contentbuf;
    while (NvSuccess == GetLine(&line, &len))
    {
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"%s\n", line));
        if (NvSuccess != (err = ParseSDPLine(pServer, line, len)))
            return err;
        line += len + 1;
    }
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"--ParseSDP"));

    return err;
}

NvError DescribeRTSP(RTSPSession *pServer)
{
    char *cmd = NULL;
    char *line;
    char *contentbuf = NULL;
    int len = 0;
    int contentlen = 0;
    int havecontentbase = 0;
    int cmdlen =0;
    NvError err = NvSuccess;
    int writeSockCount = 0;
    int redirectcount = 0;
    int responsecode = 0;
#define MAX_REDIRECTS 10
    char *newurl = NULL;
    char *UserAgentStr;
    NvU32 UserAgentStrlen;
    char *UAProfStr;
    NvU32 UAProfStrlen;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"++DescribeRTSP"));

redirect:
    if (redirectcount > MAX_REDIRECTS)
    {
        return NvError_BadParameter;
    }

    NvMMGetUserAgentString(pServer->controlSock, &UserAgentStr, &UserAgentStrlen);
    NvMMGetUAProfAgentString(pServer->controlSock, &UAProfStr, &UAProfStrlen);

   cmdlen = NvOsStrlen("DESCRIBE  RTSP/1.0\r\n") + 1
                   + NvOsStrlen("x-wap-profile:\r\n\r\n") + 1
                   + NvOsStrlen("CSeq: \r\n") + 1 + sizeof(int)
                   + NvOsStrlen("Accept: application/sdp\r\n") + 1
                   + NvOsStrlen("User-Agent: \r\n\r\n")+ 1
                   + NvOsStrlen(pServer->url) + 1
                   + UserAgentStrlen + UAProfStrlen
                   + 128;//buffer for any unforeseen erroneous spaces added in the command;
    cmd = NvOsAlloc(cmdlen);

    if(!cmdlen)
        return NvError_InsufficientMemory;
    NvOsMemset(cmd, 0, cmdlen);
    if(UAProfStrlen!=0)
    {
        NvOsSnprintf(cmd, cmdlen,
                 "DESCRIBE %s RTSP/1.0\r\n"
                 "x-wap-profile: %s\r\n"
                 "CSeq: %d\r\n"
                 "Accept: application/sdp\r\n"
                 "User-Agent: %s\r\n\r\n",
                 pServer->url, UAProfStr, pServer->sequence_id, UserAgentStr);
    }
    else
    {
        NvOsSnprintf(cmd, cmdlen,
                 "DESCRIBE %s RTSP/1.0\r\n"
                 "CSeq: %d\r\n"
                 "Accept: application/sdp\r\n"
                 "User-Agent: %s\r\n\r\n",
                 pServer->url, pServer->sequence_id, UserAgentStr);
    }

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"%s \n", cmd));

    writeSockCount = NvMMWriteSock(pServer->controlSock, cmd, NvOsStrlen(cmd), 100);
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"NvMMWriteSock retval = %d", writeSockCount));

    if (NvSuccess != (err = GetRTSPReply(pServer)))
    {
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR,"--DescribeRTSP:: err = 0x%x, line = %d", err, __LINE__));
        NvOsFree(cmd);
        return err;
    }

    // TODO: Handle redirect!
    line = pServer->response_buf;
    while (NvSuccess == GetLine(&line, &len))
    {
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"DescribeRTSP::GetLine::%s\n", line));
        if (!NvOsStrncmp("RTSP", line, 4))
        {
            char *p = line;
            while (*p != ' ' && *p != '\0')
                p++;
            if (*p != '\0')
            {
                responsecode = atoi(p);
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"atoi_responsecode = %d\n", responsecode));
            }
        }

        if (!NvOsStrncmp("Content-Length: ", line, 16) ||
            !NvOsStrncmp("Content-length: ", line, 16))
        {
            char *p = line + 16;
            contentlen = atoi(p);
        }
        else if (!NvOsStrncmp("Content-Base: ", line, 14))
        {
            char *p = line + 14;
            NvOsStrncpy(pServer->controlUrl, p, MAX_CONTROLURL);
            havecontentbase = 1;
        }
        else if (!NvOsStrncmp("Content-Location: ", line, 18))
        {
            char *p = line + 18;
            if (!havecontentbase)
                NvOsStrncpy(pServer->controlUrl, p, MAX_CONTROLURL);
        }
        else if (!NvOsStrncmp("Location: ", line, 10))
        {
            char *p = line + 10;
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"\r\nbefore_responsecode:newurl = %s\n", pServer->url));
            if (responsecode >= 300 && responsecode < 400)
            {
                newurl = NvOsAlloc(MAX_URL);
                if (!newurl)
                {
                    NvOsFree(cmd);
                    return NvError_InsufficientMemory;
                 }

                NvOsStrncpy(newurl, p, MAX_URL);
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"\r\nAfter_responsecode:newurl = %s\n", newurl));
            }
        }
        line += len + 1;
    }
    if (responsecode >= 300 && responsecode < 400)
    {
        if (newurl)
        {
            redirectcount++;
            NvOsDebugPrintf("REDIRECTING::%s\n", newurl);

            if (pServer->state >= RTSP_STATE_READY)
                TeardownRTSPStreams(pServer);
            CloseRTSPSession(pServer);

            err = ConnectToRTSPServer(pServer, newurl);
            if (err != NvSuccess)
            {
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR," ConnectToRTSPServer ::Err! status - 0x%x", err));
                NvOsFree(newurl);
                newurl = NULL;
                NvOsFree(cmd);
                return err;
            }
            NvOsFree(newurl);
            newurl = NULL;
            goto redirect;
        }
    }
    else if (responsecode >= 400 && responsecode <= 510)
    {
        NvOsDebugPrintf("Network error response status code is %x\n", responsecode);
        NvOsFree(cmd);
        return NvError_BadParameter;
    }

    if (contentlen <= 0)
    {
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "--DescribeRTSP:: NvError_BadParameter:: contentlen <= 0"));
        NvOsFree(cmd);
        return NvError_BadParameter;
    }
    pServer->sequence_id++;

    contentbuf = NvOsAlloc(contentlen+1);
    if (!contentbuf)
    {
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "--DescribeRTSP:: NvError_InsufficientMemory:: !contentbuf"));
        NvOsFree(cmd);
        return NvError_InsufficientMemory;
    }

    NvOsMemset(contentbuf, 0, contentlen+1);
    if (NvSuccess != (err = ReadFullBuffer(pServer->controlSock,
                                           contentbuf,
                                           contentlen)))
    {
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "--DescribeRTSP:: err = 0x%x, line = %d", err, __LINE__));
        goto cleanup;
    }

    err = ParseSDP(pServer, contentbuf);

cleanup:
    NvOsFree(contentbuf);
    NvOsFree(cmd);
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "--DescribeRTSP: Err = 0x%x", err));
    return err;
}

NvError SetupRTSPStreams(RTSPSession *pServer)
{
    NvU32 i;
    RTPStream *stream;
    char cmd[2048];
    char *line;
    int len = 0;
    NvError err = NvSuccess;
    NvBool bFirst = NV_TRUE;
    NvU32 nFirstPort = 0;
    char *pCmd = cmd;
    char *UserAgentStr;
    NvU32 UserAgentStrlen;
    NvU32 retryPortCount = 0;
    int port = 0;
    int min_port = 7000;
    int max_port = 7998;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"++SetupRTSPStreams"));

    srand(NvOsGetTimeMS());
    port = rand();
    port = (port % (max_port - min_port +1)) + min_port;
    if((port % 2) != 0)
        port += 1;
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "Port Number: %d", port));
    if (pServer->nNumStreams == 0 || pServer->nNumStreams > MAX_RTP_STREAMS)
    {
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "--SetupRTSPStreams:: NvError_BadParameter, line = %d", __LINE__));
        return NvError_BadParameter;
    }

    for (i = 0; i < pServer->nNumStreams; i++)
    {
        stream = &(pServer->oRtpStreams[i]);
        if (stream->eCodecType == NvStreamType_OTHER || stream->bSkipStream)
            continue;

        if (!pServer->bServerIsWMServer || bFirst)
        {
            while (port > 0)
            {
                if (NvSuccess == SetupRTPStreams(stream, port))
                    break;
                port = rand();
                retryPortCount++;
                port = (port % (max_port - min_port +1)) + min_port;
                if((port % 2) != 0)
                    port += 1;
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "Port Number: %d", port));
                if(retryPortCount  > 1000)
                    port = 0;
            }
            if (port == 0)
            {
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR,"--SetupRTSPStreams:: NvError_BadParameter, (port == 0) line = %d", __LINE__));
                return NvError_BadParameter;
            }
            nFirstPort = port;
        }
        else
        {
            stream->bSkipStream = NV_TRUE;
            port = nFirstPort;
        }

        if (pServer->bIsASF)
            SetRTPPacketSize(stream, pServer->nMaxASFPacket);

        CreateList(&(stream->oPacketQueue), NV_TRUE);
        CreateList(&(stream->oRawPacketQueue), NV_FALSE);

        //FIXME: support more than udp unicast
        NvOsMemset(cmd, 0, 2048);

        NvMMGetUserAgentString(pServer->controlSock, &UserAgentStr, &UserAgentStrlen);
        UserAgentStrlen += NvOsStrlen("User-Agent: %s\r\n\r\n");

        pCmd = cmd;
        pCmd += NvOsSnprintf(pCmd, 2048 - 256 - UserAgentStrlen,
                     "SETUP %s RTSP/1.0\r\n"
                     "CSeq: %d\r\n"
                     "Transport: RTP/AVP/UDP;unicast;client_port=%d-%d;mode=play\r\n",
                     stream->controlUrl, pServer->sequence_id, port, port + 1);

        if (NvOsStrlen(pServer->session_id) > 0)
        {
            pCmd += NvOsSnprintf(pCmd, 256, "Session: %s\r\n", pServer->session_id);
        }
        NvOsSnprintf(pCmd, UserAgentStrlen, "User-Agent: %s\r\n\r\n", UserAgentStr);
        pServer->sequence_id++;

        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"setup: %s\n", cmd));

        NvMMWriteSock(pServer->controlSock, cmd, NvOsStrlen(cmd), 100);

        if (NvSuccess != (err = GetRTSPReply(pServer)))
        {
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR,"--SetupRTSPStreams:: GetRTSPReply, err = 0x%x, line = %d", err, __LINE__));
            return err;
        }

        line = pServer->response_buf;
        while (NvSuccess == GetLine(&line, &len))
        {
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"setuprtsp: %s\n", line));
            if (line == pServer->response_buf &&
                NvOsStrcmp("RTSP/1.0 200 OK", line))
            {
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR,"--SetupRTSPStreams:: err = 0x%x, line = %d", err, __LINE__));
                return NvError_BadParameter;
            }
            else if (!NvOsStrncmp("Session: ", line, 9))
            {
                char *s = line;
                char *p = pServer->session_id;
                s += 9;

                while (*s != '\0' && *s != '\r' && *s != '\n' && *s != ';')
                {
                    *p = *s;
                    p++; s++;
                }
            }
            else if (!NvOsStrncmp("Server: ", line, 8))
            {
                char *s = line;
                s += 8;

                if (!NvOsStrncmp("WMServer", s, 8))
                    pServer->bServerIsWMServer = NV_TRUE;
            }
            else if (!NvOsStrncmp("Transport: ", line, 11) &&
                     (bFirst || !pServer->bServerIsWMServer))
            {
                char *s = line;
                int port1, port2;
                s += 11;

                while (NvOsStrncmp("server_port=", s, 12) && *s != '\0')
                    s++;
                if (*s == '\0')
                    break;
                s += 12;

                port1 = atoi(s);

                while (*s != '-' && *s != '\0')
                    s++;
                if (*s == '\0')
                {
                    SetRTPStreamServerPorts(stream, port1, -1);
                    break;
                }
                s++;

                port2 = atoi(s);

                SetRTPStreamServerPorts(stream, port1, port2);
            }

            line += len + 1;
        }

        if (!pServer->bServerIsWMServer || bFirst)
        {
            bFirst = NV_FALSE;
        }
        port += 2;
    }

    if (NvSuccess != (err = RunRTPThread(pServer)))
    {
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR,"--SetupRTSPStreams:: err = 0x%x, line = %d", err, __LINE__));
        return err;
    }

    pServer->state = RTSP_STATE_READY;
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"--SetupRTSPStreams:: END:: err = 0x%x, line = %d", err, __LINE__));
    return err;
}

void TeardownRTSPStreams(RTSPSession *pServer)
{
    NvU32 i;
    RTPStream *stream;
    char cmd[2048];
    char *UserAgentStr;
    NvU32 UserAgentStrlen;
    char *pCmd = cmd;
    NvError err = NvSuccess;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"++TeardownRTSPStreams"));

    if (pServer->nNumStreams == 0 || pServer->nNumStreams > MAX_RTP_STREAMS)
    {
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR,"--SetupRTSPStreams numStream = 0 or max RTP streams"));
        return;
    }

    NvOsMemset(cmd, 0, 2048);

    NvOsMutexLock(pServer->controlSockLock);

    NvMMGetUserAgentString(pServer->controlSock, &UserAgentStr, &UserAgentStrlen);
    UserAgentStrlen += NvOsStrlen("User-Agent: %s\r\n\r\n");

    pCmd += NvOsSnprintf(pCmd, 2048 - 256 - UserAgentStrlen,
                 "TEARDOWN %s RTSP/1.0\r\n"
                 "CSeq: %d\r\n",
                 pServer->controlUrl, pServer->sequence_id);
    if (NvOsStrlen(pServer->session_id) > 0)
    {
        pCmd += NvOsSnprintf(pCmd, 256, "Session: %s\r\n", pServer->session_id);
    }
    NvOsSnprintf(pCmd, UserAgentStrlen, "User-Agent: %s\r\n\r\n", UserAgentStr);
    pServer->sequence_id++;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"TeardownRTSPStreams: %s\n", cmd));

    NvMMWriteSock(pServer->controlSock, cmd, NvOsStrlen(cmd), 100);

    if (NvSuccess != (err = GetRTSPReply(pServer)))
    {
        NvOsMutexUnlock(pServer->controlSockLock);
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR,"--TeardownRTSPStreams:: err = 0x%x, line = %d", err, __LINE__));
        return;
    }

    NvOsMutexUnlock(pServer->controlSockLock);
    ShutdownRTPThread(pServer);

    for (i = 0; i < pServer->nNumStreams; i++)
    {
        stream = &(pServer->oRtpStreams[i]);
        CloseRTPStreams(stream);
    }
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"--SetupRTSPStreams"));

}

static char *GetNextPart(char *src, char *dest, int destmax)
{
    char *s = src, *d = dest;

    while (*s != '\0' && d - dest < destmax && *s != ';' && *s != ',')
    {
        *d = *s;
        d++; s++;
    }

    *d = '\0';
    if (*s == ';' || *s == ',')
        s++;
    return s;
}

NvError RTSPPlayFrom(RTSPSession *pServer, NvS64 ts, NvU64 *newts)
{
    char cmd[2048];
    char *line;
    int len = 0;
    char *pCmd = cmd;
    char *UserAgentStr;
    NvU32 UserAgentStrlen;
    NvU32 numUrlsFound = 0;
    NvU32 detectUrl = 0;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"++RTSPPlayFrom %lld\n", ts));

    if (pServer->state < RTSP_STATE_READY)
        return NvError_BadParameter;

    NvOsMemset(cmd, 0, 2048);
    NvOsMutexLock(pServer->controlSockLock);
    NvMMGetUserAgentString(pServer->controlSock, &UserAgentStr, &UserAgentStrlen);
    UserAgentStrlen += NvOsStrlen("User-Agent: %s\r\n\r\n");

    pCmd += NvOsSnprintf(pCmd, 2048 - 256 - 256 - UserAgentStrlen,
                 "PLAY %s RTSP/1.0\r\n"
                 "CSeq: %d\r\n",
                 pServer->controlUrl, pServer->sequence_id);
    if (NvOsStrlen(pServer->session_id) > 0)
    {
        pCmd += NvOsSnprintf(pCmd, 256, "Session: %s\r\n", pServer->session_id);
    }
    if (ts >= 0)
    {
        pCmd += NvOsSnprintf(pCmd, 256, "Range: npt=%0.3f-\r\n",
                     ts / 1000.0);
    }
    NvOsSnprintf(pCmd, UserAgentStrlen, "User-Agent: %s\r\n\r\n", UserAgentStr);
    pServer->sequence_id++;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"RTSPPlayFrom: %s\n", cmd));

    NvMMWriteSock(pServer->controlSock, cmd, NvOsStrlen(cmd), 100);

    if (NvSuccess != GetRTSPReply(pServer))
    {
        NvOsMutexUnlock(pServer->controlSockLock);
        return NvError_BadParameter;
    }
    line = pServer->response_buf;
    while (NvSuccess == GetLine(&line, &len))
    {
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"RTSPPlayFrom: %s\n", line));

        if (!NvOsStrncmp("Range: ", line, 7))
        {
            NvU32 i;
            char *s = line;
            s += 7;

            //FIXME: handle other formats
            if (NvOsStrncmp("npt=", s, 4))
                break;

            s += 4;

            if (*s == '\0')
                break;

            if (ts >= 0 && pServer->state >= RTSP_STATE_READY)
            {
                *newts = (NvU64)(atof(s) * 1000) * 1000 * 10;
                for (i = 0; i < pServer->nNumStreams; i++)
                {
                    RTPStream *rtp = &(pServer->oRtpStreams[i]);
                    rtp->TSOffset = *(NvS64 *)newts;
                    // Assign CurrentTs with the seek point sent by RTP info
                    rtp->curTs = rtp->TSOffset;
                    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"RTSPPlayFrom::TSOffset = %lld\n", rtp->TSOffset));
                }
            }
        }

        else if ((!NvOsStrncmp("RTP-Info:", line, 9)) || detectUrl)
        {
            NvU32 i;
            char *part = NvOsAlloc(MAX_CONTROLURL + 1), *p;
            char *s = line;
            RTPStream *stream = NULL;

             if(!detectUrl)
                 s += 9;

            detectUrl = 0;
            do
            {
                s = GetNextPart(s, part, MAX_CONTROLURL);
                p = (*part == ' ') ? part+1 : part;
                if (!NvOsStrncmp(p, "url=", 4))
                {
                    stream = NULL;
                    p = p + 4;
                    for (i = 0; i < pServer->nNumStreams; i++)
                    {
                        if (!NvOsStrncmp(pServer->oRtpStreams[i].controlUrl, p,
                                         NvOsStrlen(p)))
                        {
                            stream = &(pServer->oRtpStreams[i]);
                            // Reset firstts so that it can take the time stamp of either rtptime
                            // or the first packet timestamp that is received after seek
                            stream->bResetTimeStamp = 1;
                            stream->firstts = (NvU64)-1;
                            numUrlsFound++;
                            break;
                        }
                    }
                }
                else if (!NvOsStrncmp(part, "seq=", 4))
                {
                    p = part + 4;
                    if (stream)
                    {
                        stream->firstseq = (stream->SeqNumRollOverCount * 65535) + atoi(p);
                        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,
                                      "RTSPPlayFrom:: First Seq = %d ", stream->firstseq));
                    }
                }
                else if (!NvOsStrncmp(part, "rtptime=", 8))
                {
                    p = part + 8;
                    if (stream)
                    {
                        stream->firstts= atoi(p);
                        stream->bResetTimeStamp = 0;
                        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,
                                      "RTSPPlayFrom:: rtptime = %lld ", stream->firstts));
                    }
                }
            } while (*s != '\0' && s < line + len);

            if(numUrlsFound < pServer->nNumStreams)
                detectUrl = 1;

            NvOsFree(part);
        }
        line += len + 1;
    }

    if (!NvOsStrncmp("RTSP/1.0 200 OK", pServer->response_buf, 15))
    {
        NvOsMutexUnlock(pServer->controlSockLock);
        pServer->state = RTSP_STATE_PLAY;
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"--RTSPPlayFrom:: RTSP_STATE_PLAY , NvSuccess line = %d", __LINE__));
        return NvSuccess;
    }
    NvOsMutexUnlock(pServer->controlSockLock);
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR,"--RTSPPlayFrom:: NvError_BadParameter , line = %d", __LINE__));
    return NvError_BadParameter;
}

NvError RTSPPause(RTSPSession *pServer)
{
    char cmd[2048];
    NvError err = NvSuccess;
    char *pCmd = cmd;
    char *UserAgentStr;
    NvU32 UserAgentStrlen;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"++RTSPPause"));

    if (pServer->state != RTSP_STATE_PLAY)
    {
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR,"--RTSPPause :: NvError_BadParameter, line = %d", __LINE__));
        return NvError_BadParameter;
    }

    NvOsMemset(cmd, 0, 2048);
    NvOsMutexLock(pServer->controlSockLock);
    NvMMGetUserAgentString(pServer->controlSock, &UserAgentStr, &UserAgentStrlen);
    UserAgentStrlen += NvOsStrlen("User-Agent: %s\r\n\r\n");

    pCmd += NvOsSnprintf(pCmd, 2048 - 256 - UserAgentStrlen,
                 "PAUSE %s RTSP/1.0\r\n"
                 "CSeq: %d\r\n",
                 pServer->controlUrl, pServer->sequence_id);
    if (NvOsStrlen(pServer->session_id) > 0)
    {
        pCmd += NvOsSnprintf(pCmd, 256, "Session: %s\r\n", pServer->session_id);
    }
    NvOsSnprintf(pCmd, UserAgentStrlen, "User-Agent: %s\r\n\r\n", UserAgentStr);
    pServer->sequence_id++;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"RTSPPause: %s\n", cmd));

    NvMMWriteSock(pServer->controlSock, cmd, NvOsStrlen(cmd), 100);

    if (NvSuccess != (err = GetRTSPReply(pServer)))
    {
        NvOsMutexUnlock(pServer->controlSockLock);
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR,"--RTSPPause :: err = 0x%x, line = %d", err, __LINE__));
        return NvError_BadParameter;
    }

    if (!NvOsStrncmp("RTSP/1.0 200 OK", pServer->response_buf, 15))
    {
        NvOsMutexUnlock(pServer->controlSockLock);
        pServer->state = RTSP_STATE_PAUSED;
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"--RTSPPause :: RTSP_STATE_PAUSED, line = %d", err, __LINE__));
        return NvSuccess;
    }
    NvOsMutexUnlock(pServer->controlSockLock);
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR,"--RTSPPause :: err = NvError_BadParameter, line = %d", err, __LINE__));
    return NvError_BadParameter;
}

NvError GetNextPacket(RTSPSession *pServer,
                      RTPPacket *packet, int *eos)
{
    RTPStream *rtp;
    int count;
    //NvU32 entries = 0;
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"++GetNextPacket"));

    if (!eos)
    {
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR,"--GetNextPacket :: !eos, line = %d", __LINE__));
        return NvError_BadParameter;
    }

    *eos = 0;

    if (pServer->state != RTSP_STATE_PLAY)
    {
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR,"--GetNextPacket :: !RTSP_STATE_PLAY, line = %d", __LINE__));
        return NvError_BadParameter;
    }

getdata:
    count = 50;
    while (count >= 0)
    {
        if (NvSuccess == GetNextPacketFromList(pServer, packet, pServer->bEnd))
        {
            rtp = &(pServer->oRtpStreams[0]);
            rtp->failures = 0;
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"--GetNextPacket %d:: ts %lld", packet->streamNum, packet->timestamp));
            return NvSuccess;
        }
        count--;
        NvOsSleepMS(10);
    }

    rtp = &(pServer->oRtpStreams[0]);
    rtp->failures++;
    if (!pServer->bEnd)
    {
        pServer->bEnd = NV_TRUE;
        goto getdata;
    }

    if ((rtp->failures >= 10) || (pServer->nReadError >= 5))
    {
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"--GetNextPacket::failures >= %d\n", rtp->failures));
        *eos = 1;
        return NvSuccess;
    }

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR,"--GetNextPacket::TimeOut" ));
    return NvError_Timeout;
}

void UpdateStreamTimes(RTSPSession *pServer, NvU64 timestamp)
{
    RTPStream *rtp;
    NvU32 i;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"++UpdateStreamTime::timestamp = %lld", timestamp));
    for (i = 0; i < pServer->nNumStreams; i++)
    {
        rtp = &(pServer->oRtpStreams[i]);
        rtp->lastTS = timestamp;
    }
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"--UpdateStreamTime::timestamp = %lld", timestamp));
}

static void SendConfig(RTPStream *pStream, NvU32 StreamIndex)
{
    NvError enqStatus = NvSuccess;

    if (pStream->nConfigLen > 0 && pStream->configData)
    {
        RTPPacket packet;
        NvOsMemset(&packet, 0, sizeof(RTPPacket));

        packet.size = pStream->nConfigLen;
        packet.buf = NvOsAlloc(packet.size + 1);
        packet.streamNum = StreamIndex;
        NvOsMemcpy(packet.buf, pStream->configData, packet.size);
        packet.flags = PACKET_LAST;

        enqStatus = InsertInOrder(pStream->oPacketQueue, &packet);
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"SteamList: Status = 0x%x", enqStatus));
    }
}

void ClearStreamData(RTSPSession *pServer, int stream)
{
    RTPStream *rtp;
    RTPPacket packet;
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"++ClearStreamData:: stream = %d\n", stream));

    if (stream >= (int)(pServer->nNumStreams))
        return;

    rtp = &(pServer->oRtpStreams[stream]);
    while (NvSuccess == Remove(rtp->oPacketQueue, &packet))
    {
        if (packet.buf)
            NvOsFree(packet.buf);
        packet.buf = NULL;
    }
    if (rtp->oRawPacketQueue)
    {
        while (NvSuccess == (Remove(rtp->oRawPacketQueue, &packet)))
        {
            if (packet.buf)
            {
                NvOsFree(packet.buf);
                packet.buf = NULL;
            }
        }
    }

    rtp->lastseq = 0;
    rtp->lastTS = 0;
    SendConfig(rtp, stream); // resend the config packets to streamline the nem parsing

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"--ClearStreamData\n"));
}

static void *RTPThread(void *param)
{
    RTSPSession *pServer = (RTSPSession *)param;
    char buf[RTP_PACKET_SIZE];
    int len;
    NvU32 lastUDPTime = 0, timeNow = 0;
    int timecheckcounter = 0;
    NvU32 i;
    RTPStream *stream;
    NvMMSock *sockList[MAX_RTP_STREAMS * 2];
    int writeSockCount = 0;
    NvBool bFirstRead = NV_TRUE;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"++RTPThread"));

    for (i = 0; i < pServer->nNumStreams; i++)
    {
        stream = &(pServer->oRtpStreams[i]);
        if (stream && stream->bUdpSetup)
        {
            writeSockCount = NvMMWriteSock(stream->rtpSock, NULL, 0, 100);
            if(writeSockCount < 0)
            {
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"!!!NvMMWriteSock count = %d, line = %d", writeSockCount, __LINE__));
            }
            writeSockCount = NvMMWriteSock(stream->rtpcSock, NULL, 0, 100);
            if(writeSockCount < 0)
            {
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"!!!NvMMWriteSock count = %d, line = %d", writeSockCount, __LINE__));
            }
            lastUDPTime = NvOsGetTimeMS();
            if (pServer->bReconnectedSession != NV_TRUE || stream->mediaType != NvMimeMediaType_MP4ALATM)
            {
                SendConfig(stream, i);
            }
        }
    }

    pServer->bAtStart = NV_TRUE;
    while (pServer->runthread)
    {
        int sockcount = 0;
        NvMMSock *sock = NULL;
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"\n  pServer->runthread :: pServer->nNumStreams = %d \n ", pServer->nNumStreams));

        for (i = 0; i < pServer->nNumStreams; i++)
        {
            stream = &(pServer->oRtpStreams[i]);
            if (stream && stream->bUdpSetup && stream->ProcessPacket)
            {
                sockList[sockcount++] = stream->rtpSock;
                sockList[sockcount++] = stream->rtpcSock;
            }
        }

        sockList[sockcount++] = NULL;

        if (sockList[0] == NULL)
            break;

        len = NvMMReadSockMultiple(sockList, buf, RTP_PACKET_SIZE, 1000, &sock);
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"NvMMReadSockMultiple :: len = %d ", len));

        if(len <0)
        {
            pServer->nReadError++;
            for (i = 0; i < pServer->nNumStreams; i++)
            {
                stream = &(pServer->oRtpStreams[i]);
                if (stream && stream->bUdpSetup)
                    ParseRTP(stream, buf, len, pServer->bGotBye);
            }
        }
        else
            pServer->nReadError = 0;

        for (i = 0; i < pServer->nNumStreams; i++)
        {
            stream = &(pServer->oRtpStreams[i]);
            if (stream && stream->bUdpSetup)
            {
                if (sock == stream->rtpSock)
                {
                    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"++ParseRTP :: stream %d len = %d ", i, len));
                    ParseRTP(stream, buf, len, pServer->bGotBye);
                    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"--ParseRTP"));
                }
                else if (sock == stream->rtpcSock)
                {
                    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"++ParseRTCP :: stream %d len = %d ", i, len));
                    ParseRTCP(stream, buf, len);
                    if (stream->isAtEOS == 1)
                    {
                        pServer->bGotBye = NV_TRUE;
                    }
                    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"--ParseRTCP"));
                }
            }
        }

        if ((timecheckcounter++ >= 200) || bFirstRead) // throttle checking the time
        {
            timeNow = NvOsGetTimeMS();

#if NV_ENABLE_RECEIVEREPORTS
            if ((timeNow - lastUDPTime >= 15000) || bFirstRead) // every 15 seconds
            {
                bFirstRead = NV_FALSE;
                SendRTCPReceiveReport(pServer);
                SendOptionsRequest(pServer);
                lastUDPTime = timeNow;
            }
#else
            if ((timeNow - lastUDPTime >= 15000) || bFirstRead) // every 15 seconds
            {
                bFirstRead = NV_FALSE;
                for (i = 0; i < (int)(pServer->nNumStreams); i++)
                {
                    stream = &(pServer->oRtpStreams[i]);
                    if (stream && stream->bUdpSetup)
                    {

                        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"Throttle Time:: Write Sock"));
                        writeSockCount = NvMMWriteSock(stream->rtpSock, NULL, 0, 100);
                        if(writeSockCount < 0)
                        {
                            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"!!!NvMMWriteSock count = %d, line = %d", writeSockCount, __LINE__));
                        }
                        writeSockCount = NvMMWriteSock(stream->rtpcSock, NULL, 0, 100);
                        if(writeSockCount < 0)
                        {
                            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"!!!NvMMWriteSock count = %d, line = %d", writeSockCount, __LINE__));
                        }
                        lastUDPTime = timeNow;
                    }
                }
            }
#endif
        }
    }

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"--RTPThread"));
    return NULL;
}

NvError RunRTPThread(RTSPSession *pServer)
{
    pServer->runthread = 1;
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"++--RunRTPThread"));
    return NvOsThreadCreate((NvOsThreadFunction)RTPThread, pServer,
                            &(pServer->thread));
}

void ShutdownRTPThread(RTSPSession *pServer)
{
    pServer->runthread = 0;
    if (pServer->thread)
        NvOsThreadJoin(pServer->thread);
    pServer->thread = NULL;
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"++--ShutdownRTPThread"));
}

void SendRTCPReceiveReport(RTSPSession *pServer)
{
    NvU32 buffer[256];
    NvU32 index = 0;
    NvU32 fractionlost = 0;
    NvU32 jitter = 0;
    int i;
    RTPStream *stream;
    //NvU8 *cmd = buffer;
    NvU32 header;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"++SendRTCPReceiveReport"));
    for (i = 0; i < (int)(pServer->nNumStreams); i++)
    {
        stream = &(pServer->oRtpStreams[i]);
        if (stream && stream->bUdpSetup)
        {
            //Receiver report
            header = 0x80000000; // V = 2, P = 0
            header |= (1<<24); // RC
            header |= (201<<16); //PT=RR
            header |= (1 + 6*1); //len
            buffer[index++] = NvMMHToNL(header);
            buffer[index++] = NvMMHToNL((NvU32)pServer);

            // calculate Report Block
            // SSRC of source
            buffer[index++] = NvMMHToNL(stream->ssrc);

            // calculate fraction lost
            // cumulative number of packets lost
            //buffer[index++] = NvMMHToNL(fractionlost<<24 | stream->nLostPackets);
            buffer[index++] = NvMMHToNL(fractionlost<<24);

            //extended highest sequence number received
            buffer[index++] = NvMMHToNL(stream->highseq);

            // interarrival jitter
            buffer[index++] = NvMMHToNL(jitter);

            // last SR (LSR)
            buffer[index++] = NvMMHToNL(((stream->ntpH & 0xFFFF)<<16) | (stream->ntpL>>16));

            // delay since last SR (DLSR)
            buffer[index++] = NvMMHToNL(0);
            NvMMWriteSock(stream->rtpcSock, (char *)buffer, index*sizeof(NvU32), 100);
        }
        index = 0;
    }
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"--SendRTCPReceiveReport"));
}

NvError SendOptionsRequest(RTSPSession *pServer)
{
    char cmd[2048];
    NvError err = NvSuccess;
    char *pCmd = cmd;
    char *UserAgentStr;
    NvU32 UserAgentStrlen;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"++RTSPOptions"));

    if (pServer->state < RTSP_STATE_READY)
        return NvError_BadParameter;

    NvOsMemset(cmd, 0, 2048);
    NvOsMutexLock(pServer->controlSockLock);
    NvMMGetUserAgentString(pServer->controlSock, &UserAgentStr, &UserAgentStrlen);
    UserAgentStrlen += NvOsStrlen("User-Agent: %s\r\n\r\n");

    pCmd += NvOsSnprintf(pCmd, 2048 - 256 - UserAgentStrlen,
                 "OPTIONS %s RTSP/1.0\r\n"
                 "CSeq: %d\r\n",
                 pServer->controlUrl, pServer->sequence_id);
    if (NvOsStrlen(pServer->session_id) > 0)
    {
        pCmd += NvOsSnprintf(pCmd, 256, "Session: %s\r\n", pServer->session_id);
    }
    NvOsSnprintf(pCmd, UserAgentStrlen, "User-Agent: %s\r\n\r\n", UserAgentStr);
    pServer->sequence_id++;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"RTSPOptions: %s\n", cmd));

    NvMMWriteSock(pServer->controlSock, cmd, NvOsStrlen(cmd), 100);

    if (NvSuccess != (err = GetRTSPReply(pServer)))
    {
        NvOsMutexUnlock(pServer->controlSockLock);
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR,"--RTSPOptions :: err = 0x%x, line = %d", err, __LINE__));
        return NvError_BadParameter;
    }

    if (!NvOsStrncmp("RTSP/1.0 200 OK", pServer->response_buf, 15))
    {
        NvOsMutexUnlock(pServer->controlSockLock);
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,"--RTSPOptions :: Success, line = %d", err, __LINE__));
        return NvSuccess;
    }
    NvOsMutexUnlock(pServer->controlSockLock);
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR,"--RTSPOptions :: err = NvError_BadParameter, line = %d", err, __LINE__));
    return NvError_BadParameter;

}

NvError CreateList(ListHandle *phList, NvBool bAllowSameSeq)
{
    NvError status = NvSuccess;
    List *pList;

    pList = NvOsAlloc(sizeof(List));
    if (!pList)
        return NvError_InsufficientMemory;

    NvOsMemset(pList, 0, sizeof(List));

    pList->bAllowSameSeq = bAllowSameSeq;

    status = NvOsMutexCreate(&(pList->hListMutexLock));
    if (status != NvSuccess)
    {
        NvOsFree(pList);
        return NvError_InsufficientMemory;
    }

    *phList = pList;

    return NvSuccess;
}

NvError InsertInOrder(ListHandle hList, RTPPacket *pPacket)
{
    RTPPacket *pNewPacket;
    Node *pNode, *pNewNode, *pPrev;
    List *pList = (List *)hList;

    pNewPacket = NvOsAlloc(sizeof(RTPPacket));
    if (!pNewPacket)
    {
        return NvError_InsufficientMemory;
    }
    NvOsMemcpy(pNewPacket, pPacket, sizeof(RTPPacket));
    //NvOsDebugPrintf("Insert packet %d\n", pPacket->seqnum);

    pNewNode = NvOsAlloc(sizeof(Node));
    if (!pNewNode)
    {
        NvOsFree(pNewPacket);
        return NvError_InsufficientMemory;
    }
    NvOsMemset(pNewNode, 0, sizeof(Node));

    pNewNode->packet = pNewPacket;

    NvOsMutexLock(pList->hListMutexLock);

    if (pList->Head == NULL ||
        pList->Head->packet->seqnum > pNewNode->packet->seqnum)
    {
        pNewNode->next = pList->Head;
        pList->Head = pNewNode;
    }
    else
    {
        pPrev = pList->Head;
        pNode = pList->Head->next;
        while ((pNode != NULL) &&
               (pNode->packet->seqnum <= pNewNode->packet->seqnum))
        {
            if (!pList->bAllowSameSeq &&
                pNode->packet->seqnum == pNewNode->packet->seqnum)
            {
                //NvOsDebugPrintf("Same Seq %d\n", pNode->packet->seqnum);
                NvOsMutexUnlock(pList->hListMutexLock);
                return NvError_BadParameter;
            }
            //NvOsDebugPrintf("pNode = %d\n", pNode->packet->seqnum);
            pPrev = pNode;
            pNode = pNode->next;
        }
        pNewNode->next = pPrev->next;
        pPrev->next = pNewNode;
    }
    pList->size++;
    NvOsMutexUnlock(pList->hListMutexLock);
    return NvSuccess;
}

NvError Remove(ListHandle hList, RTPPacket *pPacket)
{

    Node *pNode;
    List *pList = (List *)hList;

    if (!pList)
    {
        return NvError_BadParameter;
    }

    NvOsMutexLock(pList->hListMutexLock);

    pNode = pList->Head;
    if (!pList->Head)
    {
        NvOsMutexUnlock(pList->hListMutexLock);
        return NvError_BadParameter;
    }
    pList->Head = pNode->next;

    NvOsMemcpy(pPacket, pNode->packet, sizeof(RTPPacket));

    NvOsFree(pNode->packet);
    NvOsFree(pNode);
    pList->size--;
    //NvOsDebugPrintf("Removing packet %d\n", pPacket->seqnum);
    NvOsMutexUnlock(pList->hListMutexLock);
    return NvSuccess;
}

NvError DestroyList(ListHandle hList)
{
    Node *pNode, *pNext;
    NvError status = NvSuccess;
    List *pList = (List *)hList;

    if (!pList)
        return status;

    pNode = pList->Head;
    while ( pNode != NULL )
    {
        pNext = pNode->next;
        if (pNode->packet->buf)
            NvOsFree(pNode->packet->buf);
        NvOsFree(pNode->packet);
        NvOsFree(pNode);
        pNode = pNext;
    }

    NvOsMutexDestroy(pList->hListMutexLock);
    NvOsFree(pList);
    return status;
}

NvError GetNextPacketFromList(RTSPSession *pServer, RTPPacket *pPacket, NvBool bNoRetry)
{

    NvError status = NvError_Timeout;
    NvU32 retry = 0;
    NvU32 InitialBufferingLoop = 10000;

    RTPStream *rtp;
    List *pList;
    NvU32 i;

    if (!bNoRetry)
    {
        while (retry < 5)
        {
Loop:
            if (pServer->nNumStreams > 1 &&
                pServer->bAtStart)
            {
                NvBool bWait =  NV_FALSE;
initial_buffering:
                while (InitialBufferingLoop)
                {
                    for (i = 0; i < pServer->nNumStreams; i++)
                    {
                        rtp = &(pServer->oRtpStreams[i]);
                        if (!rtp->oPacketQueue)
                            continue;

                        pList = rtp->oPacketQueue;
                        if (pList->size < 20) // check if both AV lists have enough packets
                        {
                            bWait = NV_TRUE;
                            break;
                        }
                    }
                    if (bWait)
                    {
                        NvOsThreadYield();
                        bWait = NV_FALSE;
                        InitialBufferingLoop--;
                        goto initial_buffering;
                    }
                    else
                    {
                        InitialBufferingLoop = 0;
                    }
                }
                pServer->bAtStart = NV_FALSE;
/*
                if (pServer->nNumStreams == 2)
                {
                    rtp = &(pServer->oRtpStreams[0]);
                    NvOsDebugPrintf("Stream 0 list size = %d\n", ((List *)rtp->oPacketQueue)->size);
                    rtp = &(pServer->oRtpStreams[1]);
                    NvOsDebugPrintf("Stream 1 list size = %d\n", ((List *)rtp->oPacketQueue)->size);
                }
*/
            }
            for (i = 0; i < pServer->nNumStreams; i++)
            {
                if (pServer->nNumStreams == 2)
                {
                    if (pServer->tmp)
                    {
                        i = 1;
                        pServer->tmp = 0;
                    }
                    else
                    {
                        i = 0;
                        pServer->tmp = 1;
                    }
                }
                rtp = &(pServer->oRtpStreams[i]);
                if (!rtp->oPacketQueue)
                    continue;

                pList = rtp->oPacketQueue;

                if (pList->size > 5)
                {
                    status = Remove(pList, pPacket);
                    if (status == NvSuccess)
                    {
                        break;
                    }
                }
            }
            if (status == NvSuccess)
            {
                break;
            }
            NvOsThreadYield();
            retry++;
        }
    }
    else
    {
        for (i = 0; i < pServer->nNumStreams; i++)
        {
            rtp = &(pServer->oRtpStreams[i]);
            if (!rtp->oPacketQueue)
                continue;
            pList = rtp->oPacketQueue;
            status = Remove(pList, pPacket);
            if (status == NvSuccess)
            {
                break;
            }
        }
    }
    if (status == NvSuccess)
    {
        int compseq;
        rtp = &(pServer->oRtpStreams[pPacket->streamNum]);
        if (rtp->firstseq)
        {
            //NvOsDebugPrintf("GNPFL::%d firstseq %d\tseq %d\n", rtp->nStreamNum, rtp->firstseq, pPacket->seqnum);
            if (pPacket->seqnum != 0)
            {
                if(pPacket->seqnum < rtp->firstseq)
                {
                    status = NvError_Timeout;
                    goto Loop;
                }
                else
                {
                    if ((pPacket->serverts + rtp->TSOffset) != pPacket->timestamp)
                    {
                        pPacket->timestamp = pPacket->serverts + rtp->TSOffset;
                        //NvOsDebugPrintf("GNPFL::%d pts: %lld serverts %lld TSOffset %lld\n", pPacket->streamNum, pPacket->timestamp, pPacket->serverts, rtp->TSOffset);
                    }
                    else
                    {
                        rtp->firstseq = 0;
                        //NvOsDebugPrintf("GNPFL::pts : %lld\n", pPacket->timestamp);
                    }

                }
            }
        }
        //NvOsDebugPrintf("GetNextPacketFromList stream %d\tpacket %d\n", pPacket->streamNum, pPacket->seqnum);
        if(pPacket->fuseqnum)
            rtp->lastseq = pPacket->fuseqnum - 1;
        compseq = rtp->lastseq + 1;
        //if (compseq >= 65536)
        //    compseq = 0;

        if ((compseq < pPacket->seqnum && rtp->lastseq != 0) && ( pPacket->flags != PACKET_LAST ))
        {
           // NvOsDebugPrintf("Skipped packet: curseq: %d lastseq: %d streamNum: %d\n", pPacket->seqnum, rtp->lastseq,pPacket->streamNum );
            pPacket->flags = PACKET_SKIP;
            rtp->nLostPackets++;
        }
        rtp->lastseq = pPacket->seqnum;
        return NvSuccess;
    }
    return status;
}

NvU32 GetListSize(ListHandle hList)
{
    List *pList = (List *)hList;
    if(pList)
        return pList->size;
    else
        return 0;
}


NvError PeekListElement(ListHandle hList, RTPPacket *pPacket, NvU32 nElement)
{
    NvError status = NvSuccess;
    Node *pNode, *pNext;
    List *pList = (List *)hList;
    NvU32 i = 0;
    NvBool bFoundElement = NV_FALSE;

    NvOsMutexLock(pList->hListMutexLock);
    if (!pList->Head)
    {
        NvOsMutexUnlock(pList->hListMutexLock);
        return NvError_BadParameter;
    }

    pNode = pList->Head;
    while (pNode != NULL)
    {
        if(i == nElement)
        {
            NvOsMemcpy(pPacket, pNode->packet, sizeof(RTPPacket));
            bFoundElement = NV_TRUE;
            break;
        }
        pNext = pNode->next;
        pNode = pNext;
        i ++;
    }
    NvOsMutexUnlock(pList->hListMutexLock);

    if(bFoundElement == NV_FALSE)
        status = NvError_InvalidSize;

    return status;
}

NvError RemoveListElement(ListHandle hList, RTPPacket *pPacket,  NvU32 nElement)
{
    NvError status = NvSuccess;
    Node *pNode, *pNext, *pPrev;
    List *pList = (List *)hList;
    NvU32 i = 0;
    NvBool bFoundElement = NV_FALSE;

    NvOsMutexLock(pList->hListMutexLock);
    if (!pList->Head)
    {
        NvOsMutexUnlock(pList->hListMutexLock);
        return NvError_BadParameter;
    }

    pNode = pList->Head;
    pPrev = pNode;
    while (pNode != NULL)
    {
        if(i == nElement)
        {
            NvOsMemcpy(pPacket, pNode->packet, sizeof(RTPPacket));
            pPrev->next = pNode->next;
            if(pNode == pList->Head)
                pList->Head = pNode->next;
            NvOsFree(pNode->packet);
            NvOsFree(pNode);
            pList->size--;
            bFoundElement = NV_TRUE;
            break;
        }
        pPrev = pNode;
        pNext = pNode->next;
        pNode = pNext;
        i ++;
    }
    NvOsMutexUnlock(pList->hListMutexLock);

    if(bFoundElement == NV_FALSE)
        status = NvError_InvalidSize;

    return status;
}

void
GetRTCPSdesData(
    RTSPSession *pServer,
    NvRtcpPacketType packetType,
    NvRtcpSdesType sdestype,
    void *pData,
    NvU32 nDataSize)
{
    int i;
    RTPStream *stream;
    NvU32 size = 0;

    for (i = 0; i < (int)(pServer->nNumStreams); i++)
    {
        stream = &(pServer->oRtpStreams[i]);
        if (stream && stream->bUdpSetup)
        {
            if ( RTCP_APP == packetType)
            {
                if (NULL != stream->appdata)
                {
                    size = NvOsStrlen(stream->appdata) + 1;
                    if(0 == nDataSize)
                      *((NvU32*)pData) = size;
                    else
                    {
                        NvOsMemcpy(pData, stream->appdata, size);
                    }
                    break;
                }
            }
            else if ( (RTCP_SDES == packetType) && (sdestype >= RTCP_SDES_CNAME && sdestype <= RTCP_SDES_PRIV) )
            {
                if (stream->sdestypevalue[sdestype])
                {
                    size = NvOsStrlen(stream->sdestypevalue[sdestype]) + 1;
                    if(0 == nDataSize)
                      *((NvU32*)pData) = size;
                    else
                    {
                       NvOsMemcpy(pData, stream->sdestypevalue[sdestype], size);
                    }
                    break;
                }
            }
            else
                *((NvU32*)pData) = 0;
        }
    }
}
