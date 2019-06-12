/* Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef RTP_H_
#define RTP_H_

#include <nverror.h>
#include <nvos.h>

#include "nvmm.h"
#include "nvmm_queue.h"
#include "nvcustomprotocol.h"
#include "nvmm_sock.h"

#define MAX_REORDER_PACKETS 20
#define MAX_REORDER_PACKETS_AMR 5
#define MAX_REORDER_PACKETS_AAC 5

typedef struct RTPPacket
{
    int size;
    unsigned char *buf;

    NvU64 timestamp;
    NvU64 serverts;

    int seqnum;
    int fuseqnum;
#define PACKET_LAST 1
#define PACKET_SKIP 2
    int flags;

    int streamNum;
    int M;
} RTPPacket;

// codec specific processing
typedef struct LatmContext
{   
    NvS8 audioMuxVersion;
    NvS8 audioMuxVersionA;
    NvS64 taraFullness;
    NvS32 numChunk;
    NvS32 objectType;
    NvS32 samplingFreqIndex;
    NvS32 samplingFreq;
    NvS32 noOfChannels;
    NvS32 sampleSize;
    NvS32 channelConfiguration;
    NvS32 sbrPresentFlag;
    NvS32 otherDataPresent ;
    NvS32 otherDataLenBits;
    NvS32 frameLengthType;
    NvS32 CELPFrameLengthTableIndex;
    NvS32 HVXCFrameLengthTableIndex;
    NvS32 allStreamsSameTimeFraming;
    NvS32 streamCnt;
    NvS32 numSubFrames;
    NvBool prevMarkerBit;
  

} LatmContext;

typedef enum NvMimeMediaTypeRec
{
    NvMimeMediaType_OTHER = 0x0,   // Other, not supported tracks
    NvMimeMediaType_MP4ALATM,       // Mpeg-4 LATM
    NvMimeMediaType_MPEG4GENERIC,    // Mpeg-4 Generic

    NvMimeMediaType_Force32 = 0x7FFFFFFF
} NvMimeMediaType;

typedef enum NvRtcpPacketTypeRec
{
    RTCP_SR = 200,
    RTCP_RR = 201,
    RTCP_SDES = 202,
    RTCP_BYE = 203,
    RTCP_APP = 204,

    RTCP_Force32 = 0x7FFFFFFF
}NvRtcpPacketType;

typedef enum NvRtcpSdesTypeRec
{
    RTCP_SDES_END = 0,
    RTCP_SDES_CNAME = 1,
    RTCP_SDES_NAME = 2,
    RTCP_SDES_EMAIL = 3,
    RTCP_SDES_PHONE = 4,
    RTCP_SDES_LOC = 5,
    RTCP_SDES_TOOL = 6,
    RTCP_SDES_NOTE = 7,
    RTCP_SDES_PRIV = 8,

    RTCP_SDES_Force32 = 0x7FFFFFFF
}NvRtcpSdesType;

typedef struct RTPStreamRec
{
    NvStreamType eCodecType;
    NvMimeMediaType mediaType;
    NvMMSock *rtpSock;
    NvMMSock *rtpcSock;

    NvBool bUdpSetup;
    NvBool bTcpSetup;

#define MAX_CONTROLURL 4096
    char controlUrl[MAX_CONTROLURL];

    NvU32 rtp_stream_type;
    NvU32 port;

    NvU64 firstts;
    int firstseq;
    int lastseq;
    int highseq;
    int nLostPackets;
    NvU64 ntpTS;
    NvU32 rtpTS;
    NvU32 ntpH;
    NvU32 ntpL;

/*Application dependent data*/
    char *appdata;
    NvU32 appdatalen;
    NvU32 subtype;

/*Sdes data*/
    char *sdestypevalue[RTCP_SDES_PRIV + 1];

    int rawFirstSeq;
    int rawLastSeq;
    int rawLostPackets;

    NvU32 ssrc;
    NvBool isAtEOS;

    NvU64 firstRTCPts;

    NvU64 lastTS;
    NvU64 curTs;
    NvS64 TSOffset;
    NvBool bResetTimeStamp;

    int failures;
    NvBool muxconfigpresent;

    NvU8 *configData;
    NvU32 nConfigLen;

    NvU32 nClockRate;
    NvU32 nChannels;
    NvU32 profileLevelId;
    NvBool cpresent;
    NvU32 object;
    NvBool sbrPresent;
    NvU32 Width;
    NvU32 Height;

    int sizelength;
    int indexlength;
    int indexdeltalength;

    char *pReconPacket;
    int nReconPacketLen;
    NvU64 nReconPacketTS;
    int nMaxPacketLen;
    int nReconDataLen;

    NvBool bSkipStream;

    int nStreamNum;
    NvU32 bitRate;
    NvBool brFlag;
    NvU32 retry;
    void  *codecContext;
    NvU32 nCodecContextLen;

#define RTP_PACKET_SIZE 1024*20

    NvError (*ParseFMTPLine)(struct RTPStreamRec *stream, char *line);
    int (*ProcessPacket)(int M, int seq, NvU32 timestamp, char *buf, int size,
                         RTPPacket *packet, struct RTPStreamRec *stream);
    void *oPacketQueue;
    void *oRawPacketQueue;
    NvU32 maxReorderedPackets;
    NvU32 SeqNumRollOverCount;
} RTPStream;

#define START_PORT 35562 //FIXME: make this random

void InitRTPStream(RTPStream *pRtp);

NvError SetupRTPStreams(RTPStream *pRtp, int port);
void SetRTPStreamServerPorts(RTPStream *pRtp, int port1, int port2);
void CloseRTPStreams(RTPStream *pRtp);

NvError SetRTPCodec(RTPStream *pRtp, char *codec);

NvU64 GetTimestampDiff(RTPStream *pRtp, NvU32 curTS);

void SetRTPPacketSize(RTPStream *pRtp, int nMaxASFPacket);

void ParseRTCP(RTPStream *rtp, char *buf, int len);

void ParseRTP(RTPStream *rtp, char *buf, int len, NvBool isAtEOS);

typedef void *ListHandle;

NvError CreateList(ListHandle *pList, NvBool bAllowSameSeq);

NvError InsertInOrder(ListHandle hList, RTPPacket *pPacket);

NvError Remove(ListHandle hList, RTPPacket *pPacket);

NvError DestroyList(ListHandle hList);

NvU32 GetListSize(ListHandle hList);

NvError PeekListElement(ListHandle hList, RTPPacket *pPacket, NvU32 nElement);

NvError RemoveListElement(ListHandle hList, RTPPacket *pPacket,  NvU32 nElement);

int DecodeBase64(char *out_str, char *in_str, NvU32 length);
#endif

