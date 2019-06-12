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

#include "rtp.h"
#include "rtp_audio.h"
#include "rtp_video.h"
#include "rtp_video_h264.h"
#include "nvmm_logger.h"

#define NVLOG_CLIENT NVLOG_CONTENT_PIPE // required for logging contentpipe traces

static int firstPacket = 0;
void InitRTPStream(RTPStream *pRtp)
{
    NvU32 i = 0;

    NvOsMemset(pRtp, 0, sizeof(RTPStream));
    pRtp->eCodecType = NvStreamType_OTHER;
    pRtp->mediaType = NvMimeMediaType_OTHER;
    pRtp->codecContext = NULL;
    pRtp->rtp_stream_type = -1;
    pRtp->firstts = (NvU64)-1;
    pRtp->firstRTCPts = (NvU64)-1;

    for(i = 0; i < RTCP_SDES_PRIV + 1; i++)
        pRtp->sdestypevalue[i] = NULL;
}

void SetRTPPacketSize(RTPStream *pRtp, int nMaxASFPacket)
{
    if (nMaxASFPacket <= 0)
        nMaxASFPacket = 20 * 1024;

    pRtp->nMaxPacketLen = nMaxASFPacket;

    if (pRtp->pReconPacket)
        NvOsFree(pRtp->pReconPacket);

    pRtp->pReconPacket = 0;
    pRtp->nReconPacketLen = 0;
    pRtp->nReconPacketTS = (NvU64)-1;
}

NvError SetupRTPStreams(RTPStream *pRtp, int port)
{
    if (NvSuccess != NvMMCreateSock(&pRtp->rtpSock))
        goto cleanup;
    if (NvSuccess != NvMMCreateSock(&pRtp->rtpcSock))
        goto cleanup;

    if (NvSuccess != NvMMOpenUDP(pRtp->rtpSock, pRtp->controlUrl, port))
        goto cleanup;
    if (NvSuccess != NvMMOpenUDP(pRtp->rtpcSock, pRtp->controlUrl, port+1))
    {
        NvMMCloseUDP(pRtp->rtpSock);
        goto cleanup;
    }
    firstPacket = 1;
    return NvSuccess;

cleanup:
    if (pRtp->rtpSock)
    {
        NvMMDestroySock(pRtp->rtpSock);
        pRtp->rtpSock = NULL;
    }
    if (pRtp->rtpcSock)
    {
        NvMMDestroySock(pRtp->rtpcSock);
        pRtp->rtpcSock = NULL;
    }
    return NvError_BadParameter;
}

void SetRTPStreamServerPorts(RTPStream *pRtp, int port1, int port2)
{
    if (!pRtp || port1 < 0)
        return;

    if (port2 < 0)
        port2 = port1 + 1;

    if (pRtp->rtpSock)
        NvMMSetUDPPort(pRtp->rtpSock, port1);
    if (pRtp->rtpcSock)
        NvMMSetUDPPort(pRtp->rtpcSock, port2);

    pRtp->bUdpSetup = NV_TRUE;
}

void CloseRTPStreams(RTPStream *pRtp)
{

    NvU32 i = 0;

    NvMMCloseUDP(pRtp->rtpSock);
    NvMMCloseUDP(pRtp->rtpcSock);

    NvMMDestroySock(pRtp->rtpSock);
    NvMMDestroySock(pRtp->rtpcSock);

    pRtp->rtpSock = NULL;
    pRtp->rtpcSock = NULL;

    if (pRtp->appdata)
    {
        NvOsFree(pRtp->appdata);
        pRtp->appdata = NULL;
    }


    for(i = 0; i < RTCP_SDES_PRIV + 1; i++)
    {
        if (pRtp->sdestypevalue[i])
        {
            NvOsFree(pRtp->sdestypevalue[i]);
            pRtp->sdestypevalue[i] = NULL;
        }
    }

    NvOsFree(pRtp->configData);
    pRtp->configData = NULL;
    if(pRtp->codecContext)
    {
        if(pRtp->eCodecType == NvStreamType_H264)
        {
            RTSPH264MetaData* pH264MetaData = (RTSPH264MetaData*)pRtp->codecContext;
            if(pH264MetaData->oH264PacketQueue)
            {
                DestroyList(pH264MetaData->oH264PacketQueue);
                pH264MetaData->oH264PacketQueue = NULL;
            }
        }
    }
    if(pRtp->pReconPacket)
    {
        NvOsFree(pRtp->pReconPacket);
        pRtp->pReconPacket = NULL;
    }

    if(pRtp->codecContext)
        NvOsFree(pRtp->codecContext);
    pRtp->codecContext = NULL;
    firstPacket = 0;
}

static void
CopySDESData(
    RTPStream *rtp,
    NvU8 sdestype,
    NvU8 sdestypeLen,
    char *startoffset)
{
    char *data = NULL;
    char *temp = NULL;

    if ( (sdestypeLen > 0 && sdestypeLen < 255) && sdestype <= (RTCP_SDES_PRIV + 1))
    {
        data = NvOsAlloc(sdestypeLen + 1);
        if (data)
        {
            NvOsMemset(data, '\0', sdestypeLen + 1);
            NvOsMemcpy(data, startoffset, sdestypeLen);
            temp = rtp->sdestypevalue[sdestype];
            if (temp)
            {
                NvOsFree(temp);
                temp = NULL;
            }
            rtp->sdestypevalue[sdestype] = data;
        }
    }
    else
    {
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "Error::Wrong SDES Type/Length"));
    }
}
void ParseRTCP(RTPStream *rtp, char *buf, int len)
{
    int header;
    int V, RC, PT, length;
    char *p = buf;
    char * endsdes = NULL;
    NvBool startsdes = NV_TRUE;
    NvU8 sdestype = 0;
    NvU8 sdestypeLen = 0;
    NvU32 nextchunk = 0;

    if (len < 4)
        return;

    while (len > 0)
    {
        header = NvMMNToHL(*(unsigned int*)p);
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "ParseRTCP:: header->%x",header));

        length = ((header & 0xff) +1) * 4;
        PT = (header >> 16) & 0xff;
        RC = (header >> 24) & 0x1f;
        V = (header >> 30) & 0x02;

        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "ParseRTCP:: length->%d",length));
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "ParseRTCP:: PT->%d",PT));
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "ParseRTCP:: RC->%d",RC));
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "ParseRTCP:: V->%d",V));
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "ParseRTCP:: Stream->%d",rtp->nStreamNum));

        p += 4;
        len -= 4;
        length -= 4;

        if (V != 2)
            return;

        if (RTCP_BYE == PT) // BYE
        {
            //NvOsDebugPrintf("Got BYE from server\n");
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "Got BYE from server\n"));
            rtp->isAtEOS = 1;
            p += length;
            len -= length;
        }
        else if (RTCP_SR == PT)
        {
            // skip SSRC
            p += 4;
            length -= 4;

            rtp->ntpH = NvMMNToHL(*(unsigned int*)p);
            p += 4;
            rtp->ntpL = NvMMNToHL(*(unsigned int*)p);
            p += 4;
            rtp->rtpTS = NvMMNToHL(*(unsigned int*)p);
            p += 4;
            rtp->ntpTS = ((NvU64)rtp->ntpH * 1000 * 1000 * 10) + 
                     ((NvU64)rtp->ntpL / 43); // / 4294967296 * 10000000

            //NvOsDebugPrintf("SR: %u %u %d -- %f\n", rtp->ntpH, rtp->ntpL, rtpTS, rtp->ntpTS / 4294967296.0);
            length -= 12;
            len -= 16;
            if (rtp->firstRTCPts == (NvU64)-1)
                rtp->firstRTCPts = rtp->rtpTS;
            //NvOsDebugPrintf("diff: %f\n", (rtp->rtpTS - rtp->firstRTCPts) / 10000.0);

            /*Skip remianing bytes*/
            p += length;
            len -= length;
        }
        else if (RTCP_SDES == PT)
        {
            endsdes = p + length;
            while(p < endsdes)
            {
                if (startsdes)
                {
                    if (p + 4 <= endsdes)
                    {
                        // skip SSRC
                        p += 4;
                        length -= 4;
                        len -= 4;
                    }
                    else
                    {
                        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "Error::Wrong RTCP SDES"));
                        break;
                    }
                    startsdes = NV_FALSE;
                }
                else
                {
                    if ( p + 2 <= endsdes)
                    {
                        sdestype = p[0];
                        sdestypeLen = p[1];
                        if (RTCP_SDES_END == sdestype)
                        {
                             /* Jump to next*/
                             p = (char *)( (int)p + 4);
                             nextchunk++;
                             length -= 4;
                             len -= 4;
                             if (nextchunk < (NvU32)RC)
                             {
                                 startsdes = NV_TRUE;
                                 continue;
                             }
                             else
                                 break;
                         }

                         p += 2;
                         length -= 2;
                         len -= 2;

                         if (p + sdestypeLen <= endsdes)
                         {
                             CopySDESData(rtp,sdestype,sdestypeLen,p);
                             p += sdestypeLen;
                             length -= sdestypeLen;
                             len -= sdestypeLen;
                         }
                         else
                         {
                             NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "Error:Wrong length in RTCP SDES"));
                             break;
                         }
                    }
                    else
                    {
                        /*Reached end of packet */
                        break;
                    }
                }
            }
        }
        else if (RTCP_APP == PT)
        {
            if (length > 0)
            {
                /*Copy SubType*/
                rtp->subtype = RC;
                /* skip SSRC*/
                p += 4;
                length -= 4;

                /*Copy Name*/
                if (rtp->appdata)
                {
                    NvOsFree(rtp->appdata);
                    rtp->appdata = NULL;
                }
                rtp->appdata = NvOsAlloc(length + 1);// Allocate 4 extra for name
                NvOsMemset(rtp->appdata, '\0', length + 1);
                NvOsMemcpy(rtp->appdata, p, 4);
                p += 4;
                length -= 4;
                len -= 8;

                /*Copy app data: May or may not be present*/
                rtp->appdatalen = length;
                if (rtp->appdatalen)
                {
                    if (rtp->appdata)
                    {
                        NvOsMemcpy(rtp->appdata + 4, p, rtp->appdatalen);
                        p += rtp->appdatalen;
                        length -= (int)rtp->appdatalen;
                        len -= rtp->appdatalen;
                    }
                }
            }
        }
        else if (RTCP_RR == PT)
        {
            /*Skip this packet??*/
            p += length;
            len -= length;
        }
     }
}

static void ProcessRTPPacket(int M, int seq, NvU32 timestamp, char *buf, 
                             int size, RTPPacket *packet, RTPStream *rtp)
{
    int ret;

    if (rtp->ProcessPacket)
    {
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,
            "Before ProcessRTPPacket:: seq=%d size=%d raw timestamp: %d\n",
            seq, size, timestamp));

        if(rtp->firstseq == seq)
        {
           if((rtp->bResetTimeStamp) || (rtp->firstts > (NvU64)timestamp))
           {
               NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_INFO,
                "Update First Packet TimeStamp:: seq=%d size=%d raw timestamp: %d\n",
                seq, size, timestamp));

               rtp->firstts = (NvU64)-1;
               rtp->bResetTimeStamp = 0;
           }
        }

        ret = rtp->ProcessPacket(M, seq, timestamp, buf, size, packet, rtp);
        if (ret == 0)
        {
            if (rtp->lastTS == 0)
            {
                rtp->lastTS = packet->timestamp;
            }
            packet->serverts = packet->timestamp;
            packet->timestamp = packet->timestamp + rtp->TSOffset;
            packet->streamNum = rtp->nStreamNum;

            if(seq >= rtp->firstseq)
                rtp->curTs = packet->timestamp;

            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,
                "After ProcessRTPPacket:: stream=%d seq=%d size=%d timestamp: %lld\n",
                packet->streamNum, packet->seqnum, packet->size, packet->timestamp));

            if (NvSuccess != InsertInOrder(rtp->oPacketQueue, packet))
            {
                if (packet->buf)
                    NvOsFree(packet->buf);

                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "dropping packet on stream: %d\n", rtp->nStreamNum));
                NvOsDebugPrintf("dropping packet on stream: %d\n", rtp->nStreamNum);
            }
            //else
            //    NvOsDebugPrintf("Insert in stream %d\tpacket %d\n", rtp->nStreamNum, packet.seqnum);
        }
        else
        {
               NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE,
                "After ProcessRTPPacket:: RetVal: 0x%x\n", ret));
        }
    }
}

void ParseRTP(RTPStream *rtp, char *buf, int len, NvBool isAtEOS)
{
    int header;
    int V, P, X, CSRC, M, PT;
    int seq, timestamp, ssrc;

    char *p = buf;
    RTPPacket rawPacket;
    RTPPacket packet;
    unsigned char *rawbuf;
    NvU32 listsize;
    NvBool bDrainList = NV_FALSE;
    int rawCurSeq;
    NvError status;

    if (len < 0)
    {
         bDrainList = NV_TRUE;
         goto  drain_list;
    }

    if (isAtEOS)
    {
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "ParseRTP::isAtEOS::%d\n", len));
        bDrainList = NV_TRUE;
    }

    if (len < 12)
        return;


    header = NvMMNToHL(*(unsigned int*)p);
    p += 4;
    timestamp = NvMMNToHL(*(unsigned int *)p);
    p += 4;
    ssrc = NvMMNToHL(*(unsigned int *)p);
    p += 4;

    len -= 12;

    V = (header >> 30) & 0x02;
    P = (header >> 29) & 0x01;
    X = (header >> 28) & 0x01;
    CSRC = (header >> 24) & 0xF;
    M = (header >> 23) & 0x1;
    PT = (header >> 16) & 0x7F;
    seq = (header) & 0xFFFF;

    if (V != 2)
        return;

    if (rtp->rtp_stream_type != (NvU32)PT)
        return;

    p += 4 * CSRC;
    len -= 4 * CSRC;

    if (X) // rtp extension
    {
        int Xheader = NvMMNToHL(*(unsigned int*)p);
        int Xlen = (Xheader & 0xFFFF);
        p += 4;
        len -= 4;

        p += Xlen; 
        len -= Xlen;
    }

    if (P) // has padding
    {
        if (len > 0)
        {
            int plen = p[len-1];
            len -= plen;
        }
    }

    // Deal with packet (Using M, seq, + remaining bits)
    rtp->ssrc = ssrc;

    rtp->highseq = (seq > rtp->highseq ? seq : rtp->highseq);
    if(rtp->oRawPacketQueue)
    {
        rawbuf = NvOsAlloc(len+1);
        if(!rawbuf)
            return;

        NvOsMemcpy(rawbuf, p, len);
        NvOsMemset(&rawPacket, 0, sizeof(RTPPacket));

        rawPacket.timestamp = timestamp;
        rawPacket.M               = M;
        rawPacket.seqnum     = seq;
        rawPacket.size            = len;
        rawPacket.buf             = rawbuf;
        if (rawPacket.seqnum == 65535)
        {
             NvOsDebugPrintf("rollover\n");
             rtp->SeqNumRollOverCount += 1;
        }
        rawPacket.seqnum += (rtp->SeqNumRollOverCount * 65535);

        if (NvSuccess != InsertInOrder(rtp->oRawPacketQueue, &rawPacket))
        {
             NvOsFree(rawbuf );
             NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "dropping packet on stream: %d seq num: %d", rtp->eCodecType, seq));
             NvOsDebugPrintf("dropping packet on stream: %d seq num: %d", rtp->eCodecType, seq);
        }
    }

drain_list:
    if(rtp->oRawPacketQueue)
    {
        listsize = GetListSize(rtp->oRawPacketQueue);
        if(listsize <= 0)
            return;

        if((listsize > rtp->maxReorderedPackets) || (bDrainList == NV_TRUE))
        {
            NvOsMemset(&rawPacket, 0, sizeof(RTPPacket));
            status = Remove(rtp->oRawPacketQueue, &rawPacket);
            if (status !=NvSuccess)
                return;

            rawCurSeq = rawPacket.seqnum;
            if(!firstPacket)
            {
                if( (rtp->rawLastSeq +1) != rawCurSeq)
                {
                     rtp->rawLostPackets +=  (rawCurSeq - rtp->rawLastSeq );
                     NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "Missed Receiving packets:  curseq: %d lastseq: %d On stream: %d \n", rawCurSeq , rtp->rawLastSeq, rtp->nStreamNum));
//                  NvOsDebugPrintf("Missed Receiving packets: curseq: %d lastseq: %d On stream: %d \n",rawCurSeq , rtp->rawLastSeq, rtp->nStreamNum);
                }
            }
            else
                firstPacket = 0;

            rtp->rawLastSeq =  rawCurSeq;
        }
        else
             return;

        NvOsMemset(&packet, 0, sizeof(RTPPacket));
        packet.streamNum = rtp->nStreamNum;
        ProcessRTPPacket(rawPacket.M, rawPacket.seqnum, (NvU32) rawPacket.timestamp,  (char *)rawPacket.buf, rawPacket.size, &packet, rtp);
        if(rawPacket.buf)
            NvOsFree(rawPacket.buf);
    }
    else
        bDrainList = NV_FALSE; /* once packet processed */

    /* if EOS drain the raw packet list */
    if(bDrainList == NV_TRUE)
        goto drain_list;
}

NvError SetRTPCodec(RTPStream *pRtp, char *codec)
{
    NvError retval = NvSuccess;
    char *s = codec;

    // Initialize Clockrate to 90K
    pRtp->nClockRate = 90000;

    pRtp->maxReorderedPackets = MAX_REORDER_PACKETS;

    while (*s != '\0' && *s != '/')
    {
        *s = toupper(*s);
        s++;
    }

    if (!NvOsStrncmp("AMR-WB", codec, 6) ||
        !NvOsStrncmp("AMR", codec, 3))
    {
        if (!NvOsStrncmp("AMR-WB", codec, 6))
        {
            pRtp->nClockRate = 16000; // default clock rate for AMR-WB
            pRtp->eCodecType = NvStreamType_WAMR;
        }
        else
        {
            pRtp->nClockRate = 8000; // default clock rate for AMR-NB
            pRtp->eCodecType = NvStreamType_NAMR;
        }
        pRtp->ProcessPacket = ProcessAMRPacket;
        pRtp->maxReorderedPackets = MAX_REORDER_PACKETS_AMR;
    }
    else if (!NvOsStrncmp("H263", codec, 4))
    {
        pRtp->eCodecType = NvStreamType_H263;
        pRtp->ProcessPacket = ProcessH263Packet;
    }
    else if (!NvOsStrncmp("H264", codec, 4))
    {
        RTSPH264MetaData* pH264MetaData;
        pRtp->eCodecType    = NvStreamType_H264;
        pRtp->ProcessPacket = ProcessH264Packet;
        pRtp->ParseFMTPLine = ParseH264FMTPLine;
        if(!pRtp->codecContext)
        {
            pRtp->codecContext = NvOsAlloc(sizeof(RTSPH264MetaData));
            pRtp->nCodecContextLen = sizeof(RTSPH264MetaData);
            NvOsMemset(pRtp->codecContext, 0, pRtp->nCodecContextLen);
            pH264MetaData = (RTSPH264MetaData*)pRtp->codecContext;
            if(pH264MetaData->oH264PacketQueue)
            {
                DestroyList(pH264MetaData->oH264PacketQueue);
                pH264MetaData->oH264PacketQueue = NULL;
            }
            CreateList(&(pH264MetaData->oH264PacketQueue), NV_TRUE);
        }
    }
    else if (!NvOsStrncmp("MP4V-ES", codec, 7))
    {
        pRtp->eCodecType = NvStreamType_MPEG4;
        pRtp->ProcessPacket = ProcessMP4VPacket;
    }
    else if (!NvOsStrncmp("MP4A-LATM", codec, 9))  
    {
        pRtp->mediaType= NvMimeMediaType_MP4ALATM;
        pRtp->ProcessPacket = NULL;
         // RFC 3016, process type in fmtp section
     }
    else if (!NvOsStrncmp("MPEG4-GENERIC", codec, 13))
    {
        pRtp->eCodecType = NvStreamType_OTHER;
        pRtp->mediaType= NvMimeMediaType_MPEG4GENERIC; 
        pRtp->ProcessPacket = NULL;
        // RFC 3640, process type in fmtp section
    }
    else if (!NvOsStrncmp("X-ASF-PF", codec, 8) ||
             !NvOsStrncmp("X-ASF", codec, 5))
    {
        pRtp->eCodecType = NvStreamType_WMV; // could be audio, but...
        pRtp->ProcessPacket = ProcessASFPacket;
    }
    else
        retval = NvError_BadParameter;

    if (retval == NvSuccess)
    {
        s = codec;
        while (*s != '\0' && *s != '/')
            s++;
        if (*s == '/')
        {
            s++;
            pRtp->nClockRate = atoi(s);
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "ClockRate: %d \n", pRtp->nClockRate));
        }
        while (*s != '\0' && *s != '/')
            s++;
        if (*s == '/')
        {
            s++;
            pRtp->nChannels = atoi(s);
        }
    }

    return retval;
}

NvU64 GetTimestampDiff(RTPStream *pRtp, NvU32 curTS)
{
    NvU64 cur = curTS;

    if (pRtp->firstts == (NvU64)-1)
        pRtp->firstts = cur;

    while (cur < pRtp->firstts) // wrapped
    {
        cur += 0xFFFFFFFF;
    }

    return (cur - pRtp->firstts);
}

