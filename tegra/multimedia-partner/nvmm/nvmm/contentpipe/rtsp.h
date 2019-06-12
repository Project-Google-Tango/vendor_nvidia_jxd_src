/* Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef RTSP_H_
#define RTSP_H_

#include <nverror.h>
#include <nvos.h>

#include "nvmm_sock.h"
#include "rtp.h"

typedef struct RTSPSession
{
#define MAX_URL 4096
    char url[MAX_URL];
    NvMMSock *controlSock;

    NvU32 sequence_id;
#define SESSION_LEN 1024
    char session_id[SESSION_LEN];

#define RESPONSE_LEN (16384*4)
    char response_buf[RESPONSE_LEN];

    NvU32 nNumStreams;
#define MAX_RTP_STREAMS 8
    RTPStream oRtpStreams[MAX_RTP_STREAMS];

    char controlUrl[MAX_CONTROLURL]; // from rtp.h
    NvBool bLiveStreaming;
    NvU64 nDuration; // in ms
#define RTSP_STATE_READY  1
#define RTSP_STATE_PAUSED 2
#define RTSP_STATE_PLAY   3
    NvU32 state;
    int nReadError;
    int bIsASF;
    NvU8 *pASFHeader;
    int nASFHeaderLen;
    int nMaxASFPacket;
    NvBool bServerIsWMServer;

    NvBool bHaveVideo;
    NvBool bHaveAudio;
    NvOsThreadHandle thread;
    NvBool runthread;
    NvBool bEnd;
    NvBool bAtStart;
    NvU32 tmp;
    NvBool bGotBye;
    NvOsMutexHandle controlSockLock;
    NvBool bReconnectedSession;
} RTSPSession;

NvError ParseSDP(RTSPSession *pServer, char *contentbuf);

NvError CreateRTSPSession(RTSPSession **pServer);
void DestroyRTSPSession(RTSPSession *pServer);

NvError ConnectToRTSPServer(RTSPSession *pServer, const char *url);
void CloseRTSPSession(RTSPSession *pServer);

NvError DescribeRTSP(RTSPSession *pServer);
NvError SetupRTSPStreams(RTSPSession *pServer);

NvError RTSPPlayFrom(RTSPSession *pServer, NvS64 ts, NvU64 *newts);
NvError RTSPPause(RTSPSession *pServer);

void TeardownRTSPStreams(RTSPSession *pServer);

NvError GetNextPacket(RTSPSession *pServer,
                      RTPPacket *packet, int *eos);

void ClearStreamData(RTSPSession *pServer, int stream);

void UpdateStreamTimes(RTSPSession *pServer, NvU64 timestamp);

NvError RunRTPThread(RTSPSession *pServer);
void ShutdownRTPThread(RTSPSession *pServer);
void SendRTCPReceiveReport(RTSPSession *pServer);
NvError SendOptionsRequest(RTSPSession *pServer);
//void PauseRTPThread(RTSPSession *pServer);
//void UnpauseRTPThread(RTSPSession *pServer);

void
GetRTCPSdesData(
    RTSPSession *pServer,
    NvRtcpPacketType packetType,
    NvRtcpSdesType sdestype,
    void *pData,
    NvU32 nDataSize);
#endif
