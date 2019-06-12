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
#include "nvos.h"
#include "nvassert.h"
#include "rtsp.h"
#include "nvutil.h"
#include "nvmm_logger.h"
#include "nvmm_common.h"

#define NVLOG_CLIENT NVLOG_CONTENT_PIPE // required for logging contentpipe traces

typedef struct RTSPProtoHandle
{
    RTSPSession *pRtsp;

    NvU64 nPosition;

#define MAX_URL 4096
    char url[MAX_URL];


#define STATE_FILE_HEADER  0 
#define STATE_NEW_PACKET   1
#define STATE_MID_PACKET   2
#define STATE_END_PACKET   3
    int nState;
    NvU32 nBytesWrittenInState; // 0 for start of state

    NEM_FILE_HEADER oFileHeader;
    NEM_PACKET_HEADER oAudioHeader;
    NEM_AUDIO_FORMAT_HEADER oAudioData;
    NEM_PACKET_HEADER oVideoHeader;
    NEM_VIDEO_FORMAT_HEADER oVideoData;
    int bCreatedHeaders;

    char tmpData[20 * 1024];
    NvU32 tmpFilledSize;

    int nextStreamIndex;
    RTPPacket lastPacket;

    NvS64 nSeekTime;
    int bUseSeekTime;
    NvU64 nActualSeekTime;

    NvU64 lastSeek;
    NvBool bReconnect;
    NvU64 maxTS;
    NvS32 eos; // to check eos is set or not
    NvU32 ReconnectCount;
} RTSPProtoHandle;

static NvError RTSPProtoGetVersion(NvS32 *pnVersion)
{
    *pnVersion = NV_CUSTOM_PROTOCOL_VERSION;
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "++--RTSPGetVersion = NV_CUSTOM_PROTOCOL_VERSION"));
    return NvSuccess;
}

static NvError RTSPProtoClose(NvCPRHandle hContent)
{
    RTSPProtoHandle *pContext = (RTSPProtoHandle *)hContent;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "++RTSPClose "));

    if (pContext)
    {
        if (pContext->lastPacket.buf)
        {
            NvOsFree(pContext->lastPacket.buf);
            pContext->lastPacket.buf = NULL;
        }
        
        if (pContext->pRtsp)
        {
            if (pContext->pRtsp->state >= RTSP_STATE_READY)
                TeardownRTSPStreams(pContext->pRtsp);
            CloseRTSPSession(pContext->pRtsp);
            DestroyRTSPSession(pContext->pRtsp);
            pContext->pRtsp = NULL;
        }
        NvOsFree(pContext);
    }
    
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "--RTSPClose "));
    
    return NvSuccess;
}

static NvError RTSPProtoOpen(NvCPRHandle* hHandle, char *szURI, 
                             NvCPR_AccessType eAccess)
{
    RTSPProtoHandle *pContext = NULL;
    char *s;
    NvBool bSDP = NV_FALSE;
    char *buf = NULL;
    NvS64 len = 0;
    NvU64 nDuration; // in ms
    NvError Status = NvSuccess;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "++RTSPOpen "));

    NVMM_CHK_ARG (eAccess == NvCPR_AccessRead);

    pContext = NvOsAlloc(sizeof(RTSPProtoHandle));
    NVMM_CHK_MEM(pContext);

    NvOsMemset(pContext, 0, sizeof(RTSPProtoHandle));
    NvOsStrncpy(pContext->url, szURI, NvOsStrlen(szURI));

    NVMM_CHK_ERR(CreateRTSPSession(&pContext->pRtsp));

    if (!NvOsStrncmp("http:", szURI, 5))
    {
        s = NvUStrstr((char *)szURI, ".sdp");
        if (s)
            bSDP = NV_TRUE;
    }

    if (bSDP)
    {
        NVMM_CHK_ERR(NvMMSockGetHTTPFile(szURI, &buf, &len));
        NVMM_CHK_ERR(ParseSDP(pContext->pRtsp, buf));
        nDuration = pContext->pRtsp->nDuration;
        NvOsFree(buf);
        NVMM_CHK_ERR(ConnectToRTSPServer(pContext->pRtsp, pContext->pRtsp->controlUrl));
        pContext->pRtsp->nDuration = nDuration;
    }
    else
    {
        NVMM_CHK_ERR(ConnectToRTSPServer(pContext->pRtsp, pContext->url));
        NVMM_CHK_ERR(DescribeRTSP(pContext->pRtsp)); 
    }

    /* To handle case where "range" attribute is not there.
     * Observed for some Live SDP files.
     */
    if(pContext->pRtsp->nDuration == (NvU64)-1)
    {
        pContext->pRtsp->bLiveStreaming = NV_TRUE;
        pContext->pRtsp->nDuration = 0;
    }

    NVMM_CHK_ERR(SetupRTSPStreams(pContext->pRtsp));

    *hHandle = pContext;

cleanup:
    if (Status != NvSuccess)
    {
        RTSPProtoClose((NvCPRHandle)pContext);
    }
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "--RTSPOpen - Status =0x%x ", Status));
    return Status;
}

static NvError RTSPProtoSetPosition(NvCPRHandle hContent, 
                                    NvS64 nOffset, 
                                    NvCPR_OriginType eOrigin)
{
    NvError Status = NvSuccess;
    RTSPProtoHandle *pContext = (RTSPProtoHandle *)hContent;
    NvU32 i;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "++RTSPSetPosition - nOffset = %lld, eOrigin = %d", nOffset, eOrigin));

    NVMM_CHK_ARG(pContext);

    if (eOrigin != NvCPR_OriginTime)
    {
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "RTSPSetPosition - NvError_NotSupported, line = %d", __LINE__));
        Status = NvError_NotSupported;
        goto cleanup;
    }

    if(!pContext->pRtsp->bLiveStreaming)
    {
        if (pContext->pRtsp->nDuration <= 0 ||
            nOffset >= (NvS64)(pContext->pRtsp->nDuration) * 1000 * 10)
        {
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR,"RTSPSetPosition - NvError_NotSupported, line = %d", __LINE__));
            Status = NvError_NotSupported;
            goto cleanup;
        }
    }

    if (pContext->pRtsp->state < RTSP_STATE_READY)
    {
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR,"RTSPSetPosition - NvError_ParserFailure, line = %d", __LINE__));
        Status = NvError_ParserFailure;
        goto cleanup;
    }

    pContext->pRtsp->bEnd = NV_FALSE;
    pContext->pRtsp->bGotBye = NV_FALSE;

    if (pContext->pRtsp->state != RTSP_STATE_PAUSED)
    {
        RTSPPause(pContext->pRtsp);
    }

    if (pContext->bReconnect != NV_TRUE)
    {
       for (i = 0 ; i < pContext->pRtsp->nNumStreams; i++)
       {
           ClearStreamData(pContext->pRtsp, i);
       }

       if (pContext->lastPacket.buf)
           NvOsFree(pContext->lastPacket.buf);
       pContext->lastPacket.buf = NULL;

       // If client issued a seek 0 and EOS is set
       if (nOffset == 0 && pContext->eos)
       {

          // Tear down RTSP streams and closing RTSP seission
          if (pContext->pRtsp)
          {
              if (pContext->pRtsp->state >= RTSP_STATE_READY)
                 TeardownRTSPStreams(pContext->pRtsp);
              CloseRTSPSession(pContext->pRtsp);
          }

          NvOsMemset(pContext->pRtsp, 0, sizeof(RTSPSession));

          // Reconnect
          NVMM_CHK_ERR(ConnectToRTSPServer(pContext->pRtsp, pContext->url));
          NVMM_CHK_ERR(DescribeRTSP(pContext->pRtsp));
          NVMM_CHK_ERR(SetupRTSPStreams(pContext->pRtsp));

          pContext->bReconnect = NV_TRUE;
          pContext->eos = 0;// reset the eos back
          pContext->lastSeek = 0;

       }
    }
    pContext->nState = STATE_NEW_PACKET;
    pContext->nBytesWrittenInState = 0;
    pContext->nSeekTime = nOffset / 1000 / 10;
    pContext->maxTS = 0;
    RTSPPlayFrom(pContext->pRtsp, pContext->nSeekTime, &(pContext->nActualSeekTime));

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "RTSPSetPosition::nSeekTime - %lld ActualSeekTime - %lld\n", pContext->nSeekTime, pContext->nActualSeekTime));
    cleanup:
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "--RTSPSetPosition - Status - 0x%x", Status));
        return Status;
}

static NvError RTSPProtoGetPosition(NvCPRHandle hContent, 
                                    NvU64 *pPosition)
{
    NvError Status = NvSuccess;
    RTSPProtoHandle *pContext = (RTSPProtoHandle *)hContent;

    NVMM_CHK_ARG(pContext);

    *pPosition = pContext->nPosition;
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "++--RTSPGetPosition = %lld",*pPosition));

cleanup:    
    return Status;
}

static void GenerateHeaders(RTSPProtoHandle *pContext)
{
    NvU32 i = 0;
    int bSeenAudio = 0;
    int bSeenVideo = 0;
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "++RTSPGenerateHeaders"));

    if (pContext->pRtsp->bIsASF)
    {
        NvOsMemcpy(pContext->tmpData,
            pContext->pRtsp->pASFHeader,
            pContext->pRtsp->nASFHeaderLen);
        pContext->tmpFilledSize = pContext->pRtsp->nASFHeaderLen;
        pContext->bCreatedHeaders = 1;
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "--RTSPGenerateHeaders, ASF header , line = %d", __LINE__));

        return;
    }

    // file header
    pContext->oFileHeader.magic = NEM_FOURCC('N', 'v', 'M', 'M');
    pContext->oFileHeader.size = sizeof(NEM_FILE_HEADER);
    pContext->oFileHeader.version = 1;
    pContext->oFileHeader.numStreams = pContext->pRtsp->nNumStreams;
    pContext->oFileHeader.indexOffset = 0;

    pContext->tmpFilledSize = 0;
    NvOsMemcpy(pContext->tmpData,
        (NvU8 *)&(pContext->oFileHeader),
        sizeof(NEM_FILE_HEADER));
    pContext->tmpFilledSize = sizeof(NEM_FILE_HEADER);

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "NEM file header copied"));

    for (i = 0; i < pContext->pRtsp->nNumStreams; i++)
    {
        if (NV_ISSTREAMAUDIO(pContext->pRtsp->oRtpStreams[i].eCodecType) &&
            !bSeenAudio)
        {
            pContext->oAudioHeader.packetType = NEM_TWOCC('a', 'h');
            pContext->oAudioHeader.streamIndex = i;
            pContext->oAudioHeader.size = sizeof(NEM_AUDIO_FORMAT_HEADER);
            pContext->oAudioHeader.flags = 0;
            pContext->oAudioHeader.timestamp = 0;

            pContext->oAudioData.duration = pContext->pRtsp->nDuration * 1000 * 10;
            pContext->oAudioData.type = pContext->pRtsp->oRtpStreams[i].eCodecType;
            pContext->oAudioData.bitRate =  (pContext->pRtsp->oRtpStreams[i].bitRate) * 1000;
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "Audio Type = 0x%x, Duration = %lld, bit rate = %d", 
                pContext->oAudioData.type,
                pContext->oAudioData.duration,
                pContext->oAudioData.bitRate));

            if (pContext->oAudioData.type != 0)
            {
                bSeenAudio = 1;

                NvOsMemcpy(pContext->tmpData + pContext->tmpFilledSize,
                    (NvU8 *)&(pContext->oAudioHeader),
                    sizeof(NEM_PACKET_HEADER));
                pContext->tmpFilledSize += sizeof(NEM_PACKET_HEADER);

                NvOsMemcpy(pContext->tmpData + pContext->tmpFilledSize,
                    (NvU8 *)&(pContext->oAudioData),
                    sizeof(NEM_AUDIO_FORMAT_HEADER));
                pContext->tmpFilledSize += sizeof(NEM_AUDIO_FORMAT_HEADER);
            }
        }
        else if (NV_ISSTREAMVIDEO(pContext->pRtsp->oRtpStreams[i].eCodecType) &&
            !bSeenVideo)
        {
            pContext->oVideoHeader.packetType = NEM_TWOCC('v', 'h');
            pContext->oVideoHeader.streamIndex = i;
            pContext->oVideoHeader.size = sizeof(NEM_VIDEO_FORMAT_HEADER);
            pContext->oVideoHeader.flags = 0;
            pContext->oVideoHeader.timestamp = 0;
            pContext->oVideoData.height = pContext->pRtsp->oRtpStreams[i].Height;
            pContext->oVideoData.width = pContext->pRtsp->oRtpStreams[i].Width;

            pContext->oVideoData.duration = pContext->pRtsp->nDuration * 1000 * 10;
            pContext->oVideoData.type = pContext->pRtsp->oRtpStreams[i].eCodecType;
            pContext->oVideoData.bitRate =  (pContext->pRtsp->oRtpStreams[i].bitRate) * 1000;
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "Video Type = 0x%x, Duration = %lld, bit rate = %d", 
                pContext->oVideoData.type,
                pContext->oVideoData.duration,
                pContext->oVideoData.bitRate));

            if (pContext->oVideoData.type != 0)
            {
                bSeenVideo = 1;

                NvOsMemcpy(pContext->tmpData + pContext->tmpFilledSize,
                    (NvU8 *)&(pContext->oVideoHeader),
                    sizeof(NEM_PACKET_HEADER));
                pContext->tmpFilledSize += sizeof(NEM_PACKET_HEADER);

                NvOsMemcpy(pContext->tmpData + pContext->tmpFilledSize,
                    (NvU8 *)&(pContext->oVideoData),
                    sizeof(NEM_VIDEO_FORMAT_HEADER));
                pContext->tmpFilledSize += sizeof(NEM_VIDEO_FORMAT_HEADER);
            }
        }
    }
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "--RTSPGenerateHeaders"));
    pContext->bCreatedHeaders = 1;
}

static NvU32 RTSPProtoRead(NvCPRHandle hContent, NvU8 *pData, NvU32 nSize)
{
    NvU32 bytesread = 0;
    NvError status = NvSuccess;
    RTSPProtoHandle *pContext = (RTSPProtoHandle *)hContent;

    NV_ASSERT(pContext);
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "++RTSPProtoRead:: nSize = %d", nSize));

    while (bytesread < nSize)
    {
        if (pContext->nState == STATE_FILE_HEADER)
        {
            NvU32 copysize = 0;
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "STATE_FILE_HEADER:: bytes Read(%d) < nSize(%d)", bytesread, nSize));

            if (!pContext->bCreatedHeaders)
                GenerateHeaders(pContext);

            // size of header to copy
            copysize = pContext->tmpFilledSize - pContext->nBytesWrittenInState;
            if (copysize + bytesread >= nSize)
                copysize = nSize - bytesread;

            // copy at least some of the header
            NvOsMemcpy(pData + bytesread,
                pContext->tmpData + pContext->nBytesWrittenInState,
                copysize);

            bytesread += copysize;
            pContext->nBytesWrittenInState += copysize;

            // done with the header?
            if (pContext->nBytesWrittenInState >= pContext->tmpFilledSize)
            {
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "nState = STATE_NEW_PACKET:: nBytesWrittenInState(%d) >= tmpFilledSize(%d)", 
                    pContext->nBytesWrittenInState,
                    pContext->tmpFilledSize));
                pContext->nState = STATE_NEW_PACKET;
                pContext->nBytesWrittenInState = 0;

            }
        }
        else if (pContext->nState == STATE_NEW_PACKET)
        {
            NEM_PACKET_HEADER *pHeader;

            if (pContext->bUseSeekTime)
            {
                pContext->bUseSeekTime = 0;
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "RTSPPlayFrom time - %lld", pContext->nSeekTime));
                pContext->maxTS = 0;
                RTSPPlayFrom(pContext->pRtsp, pContext->nSeekTime, &(pContext->nActualSeekTime));
            }
            else
            {
                if (pContext->pRtsp->state == RTSP_STATE_READY)
                {
                    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "RTSP_STATE_READY, RTSPPlayFrom 0"));
                    RTSPPlayFrom(pContext->pRtsp, 0, &(pContext->nActualSeekTime));
                }
                else if (pContext->pRtsp->state == RTSP_STATE_PAUSED)
                {
                    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "RTSP_STATE_PAUSED, RTSPPlayFrom -1"));
                    RTSPPlayFrom(pContext->pRtsp, -1, &(pContext->nActualSeekTime));
                }
            }


            status = GetNextPacket(pContext->pRtsp, 
                &pContext->lastPacket, &pContext->eos);
            //NvOsDebugPrintf("GNP: %d %d\n", status, pContext->eos);
            if (status == NvSuccess)
            {
                if(!pContext->pRtsp->bLiveStreaming)
                {
                    if ((pContext->lastPacket.timestamp / 1000 / 10 ) >= pContext->pRtsp->nDuration)
                    {
                        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "Received last Packet\n"));
                        pContext->eos = 1;
                        break;
                    }
                }
                pContext->maxTS = (pContext->maxTS > pContext->lastPacket.timestamp ? pContext->maxTS : pContext->lastPacket.timestamp);
                if ((pContext->bReconnect) && (!pContext->eos))
                {
                    if (pContext->lastSeek < pContext->lastPacket.timestamp)
                    {
                        pContext->lastSeek = 0;
                        pContext->bReconnect = NV_FALSE;
                        pContext->ReconnectCount = 0;
                    }
                }
            }
            else
            {
                if (status == NvError_Timeout) {
                    continue;
                }
                else
                {
                    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "RTSPProtoRead :: 0x%x ", status));
                }
                break;
            }

            if (pContext->eos)
            {
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "EOS"));

                if(pContext->pRtsp->bGotBye)
                {
                    break;
                }

                {
                    NvS64 tDiff = pContext->pRtsp->nDuration - (pContext->maxTS / 1000 / 10);
                    if ( tDiff < 600 && pContext->pRtsp->nDuration != 0)
                    {
                        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "Close to end of playback, tDiff = %lld\n", tDiff));
                        break;
                    }
                    else
                    {

                        if (pContext->lastSeek >= pContext->maxTS && pContext->maxTS != 0)
                        {
                            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "LastPacket TS = %lld\n", pContext->maxTS));
                            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "LastSeek = %lld\n", pContext->lastSeek));
                            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "nDuration = %lld\n", pContext->pRtsp->nDuration));
                            //pContext->lastSeek = 0;
                            break;
                        }


                        if (pContext->ReconnectCount >= 3)
                        {
                            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "Reconnect count is %d",pContext->ReconnectCount));
                            break;
                        }
                        pContext->ReconnectCount++;
                        // Reconnect and seek
                        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "Reconnect and seek to %lld\n", pContext->maxTS));
                        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "nDuration = %lld\n", pContext->pRtsp->nDuration));
                        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "tDiff = %lld", tDiff));

                        //Close the session

                        if (pContext->lastPacket.buf)
                            NvOsFree(pContext->lastPacket.buf);
                        pContext->lastPacket.buf = NULL;

                        if (pContext->pRtsp)
                        {
                            if (pContext->pRtsp->state >= RTSP_STATE_READY)
                                TeardownRTSPStreams(pContext->pRtsp);
                            CloseRTSPSession(pContext->pRtsp);
                        }
                        NvOsMemset(pContext->pRtsp, 0, sizeof(RTSPSession));

                        //check if App wants to stop the playback
                        if(NvMMSockGetBlockActivity())
                        {
                           NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "Get out of RTSP Proto Read, line (%u)", __LINE__));
                           return 0;
                        }

                        pContext->pRtsp->bReconnectedSession = NV_TRUE;
                        // Reconnect
                        status = ConnectToRTSPServer(pContext->pRtsp, pContext->url);
                        if (status != NvSuccess)
                        {
                            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, " ConnectToRTSPServer ::Err! status - 0x%x", status));
                            break;
                        }

                        status = DescribeRTSP(pContext->pRtsp); 
                        if (status != NvSuccess)
                        {
                            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, " DescribeRTSP ::Err! status - 0x%x", status));
                            break;
                        }

                        status = SetupRTSPStreams(pContext->pRtsp);
                        if (status != NvSuccess)
                        {
                            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "SetupRTSPStreams Err status - 0x%x, line - %d", status, __LINE__));
                            break;
                        }

                        // seek
                        pContext->lastSeek = pContext->maxTS;
                        pContext->bReconnect = NV_TRUE;
                        status = RTSPProtoSetPosition(pContext, pContext->maxTS, NvCPR_OriginTime);
                        if (status != NvSuccess)
                            break;
                        continue;
                    }
                }
            }

            pHeader = (NEM_PACKET_HEADER *)pContext->tmpData;
            NvOsMemset(pHeader, 0, sizeof(NEM_PACKET_HEADER));

            pHeader->packetType = NEM_TWOCC('d', 'a');
            pHeader->streamIndex = pContext->lastPacket.streamNum;
            pHeader->size = pContext->lastPacket.size;
            if (pContext->lastPacket.flags == PACKET_LAST)
                pHeader->flags |= NEM_PACKET_FLAG_ENDOFPACKET;
            if (pContext->lastPacket.flags == PACKET_SKIP)
                pHeader->flags |= NEM_PACKET_FLAG_SKIPPACKET;
            pHeader->timestamp = pContext->lastPacket.timestamp;
            pContext->tmpFilledSize = sizeof(NEM_PACKET_HEADER);

            pContext->nState = STATE_MID_PACKET;
            pContext->nBytesWrittenInState = 0;

            if (pContext->pRtsp->bIsASF)
                pContext->nState = STATE_END_PACKET; // no header for ASF
        }
        else if (pContext->nState == STATE_MID_PACKET)
        {
            NvU32 copysize;

            // size of header to copy
            copysize = pContext->tmpFilledSize - pContext->nBytesWrittenInState;
            if (copysize + bytesread >= nSize)
                copysize = nSize - bytesread;

            // copy at least some of the header
            NvOsMemcpy(pData + bytesread,
                pContext->tmpData + pContext->nBytesWrittenInState,
                copysize);

            bytesread += copysize;
            pContext->nBytesWrittenInState += copysize;

            // done with the header?
            if (pContext->nBytesWrittenInState >= pContext->tmpFilledSize)
            {
                pContext->nState = STATE_END_PACKET;
                pContext->nBytesWrittenInState = 0;
            }
        }
        else if (pContext->nState == STATE_END_PACKET)
        {
            NvU32 copysize;

            copysize = pContext->lastPacket.size - pContext->nBytesWrittenInState;
            if (copysize + bytesread >= nSize)
                copysize = nSize - bytesread;

            // copy at least some of the packet
            NvOsMemcpy(pData + bytesread,
                pContext->lastPacket.buf + pContext->nBytesWrittenInState,
                copysize);

            bytesread += copysize;
            pContext->nBytesWrittenInState += copysize;

            if (pContext->nBytesWrittenInState >= (NvU32)pContext->lastPacket.size)
            {
                if (pContext->lastPacket.buf)
                    NvOsFree(pContext->lastPacket.buf);
                pContext->lastPacket.buf = NULL;

                pContext->nState = STATE_NEW_PACKET;
                pContext->nBytesWrittenInState = 0;
            }
            NvOsThreadYield();
        }
    }

    pContext->nPosition += bytesread;
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "pContext->nPosition = %lld, bytesread = %d", pContext->nPosition, bytesread ));

    //NvOsDebugPrintf("read: %d\n", bytesread);

#if 0
    {
        FILE *fout = fopen("/test.asf", "ab");
        if (fout)
        {
            fwrite(pData, bytesread, 1, fout);
            fclose(fout);
        }
    }
#endif
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "--RTSPProtoRead: bytesRead = %d", bytesread));

    return (NvU32)bytesread;
}

static NvU32 RTSPProtoWrite(NvCPRHandle hContent, NvU8 *pData, NvU32 nSize)
{
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "++--RTSPProtoWrite"));
    return 0;
}

static NvError RTSPProtoGetSize(NvCPRHandle hContent, NvU64 *pSize)
{
    *pSize = (NvU64)-1;
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "++--RTSPProtoGetSize = %lld", *pSize));
    return NvSuccess;
}

static NvBool RTSPProtoIsStreaming(NvCPRHandle hContent)
{
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "++--RTSPProtoIsStreaming"));
    return NV_TRUE;
}

static NvParserCoreType RTSPProtoProbeParserType(char *szURI)
{
    //RTSPProtoHandle *pContext;
    //NvError Status = NvSuccess;
    NvParserCoreType eType = NvParserCoreType_NemParser;
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "++RTSPProtoProbeParserType ::URI - %s", szURI));

    #if 0
    // Disabling the code for parser detection in rtsp protocol
    pContext = NvOsAlloc(sizeof(RTSPProtoHandle));
    NVMM_CHK_MEM(pContext);

    NvOsMemset(pContext, 0, sizeof(RTSPProtoHandle));
    NvOsStrncpy(pContext->url, szURI, NvOsStrlen(szURI));

    NVMM_CHK_ERR(CreateRTSPSession(&pContext->pRtsp));
    NVMM_CHK_ERR(ConnectToRTSPServer(pContext->pRtsp, pContext->url));
    NVMM_CHK_ERR(DescribeRTSP(pContext->pRtsp)); 

    if (pContext->pRtsp->bIsASF)
    {
        NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, " Is ASF::NvParserCoreType_AsfParser"));
        eType = NvParserCoreType_AsfParser;
    }

cleanup:
    RTSPProtoClose((NvCPRHandle)pContext);
    #endif
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "--RTSPProtoProbeParserType :: eType = 0x%x", eType));
    return eType;
}

static NvError GetRTSPTimeStamps(RTSPSession *pServer, CP_QueryConfigTS *ts)
{
    RTPStream *rtp;
    NvU32 i = 0;

    NvOsMemset(ts, 0, sizeof(CP_QueryConfigTS));
    for(i = 0; i < pServer->nNumStreams; i++)
    {
        rtp = &(pServer->oRtpStreams[i]);
        if (NV_ISSTREAMAUDIO(rtp->eCodecType))
        {
            ts->audioTs = rtp->curTs;
            ts->bAudio = 1;
        }
        if(NV_ISSTREAMVIDEO(rtp->eCodecType))
        {
            ts->videoTs = rtp->curTs;
            ts->bVideo= 1;
        }
    }
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG,
        "Is Audio = %u :: Latest Audio Ts : %llu, Is Video = %u :: Latest Video Ts: %llu",
        ts->bAudio, ts->audioTs, ts->bVideo, ts->videoTs));

    return NvSuccess;
}

static NvError RTSPProtoQueryConfig(NvCPRHandle hContent, 
                                    NvCPR_ConfigQueryType eQuery,
                                    void *pData, int nDataSize)
{
    RTSPProtoHandle *pContext = (RTSPProtoHandle *)hContent;
    NvRtcpPacketType packetType = 0;
    NvRtcpSdesType sdestype = 0;

    NV_ASSERT(pContext);

    switch (eQuery)
    {
    case NvCPR_ConfigPreBufferAmount:
        {
            if (nDataSize != sizeof(NvU32))
            {
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "RTSPProtoQueryConfig :: NvError_BadParameter"));
                return NvError_BadParameter;
            }
            *((NvU32*)pData) = 4 * 1024;
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "RTSPProtoQueryConfig :: Data  = 4 K"));

            return NvSuccess;
        }
    case NvCPR_ConfigCanSeekByTime:
        {
            if (nDataSize != sizeof(NvU32))
            {
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "RTSPProtoQueryConfig :: NvCPR_ConfigCanSeekByTime::NvError_BadParameter"));
                return NvError_BadParameter;
            }
            *((NvU32*)pData) = 1;
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "RTSPProtoQueryConfig :: NvCPR_ConfigCanSeekByTime::1"));
            return NvSuccess;
        }
        case NvCPR_ConfigGetChunkSize:
        {
            if (nDataSize != sizeof(NvU32))
            {
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "RTSPProtoQueryConfig :: NvCPR_ConfigGetChunkSize::NvError_BadParameter"));
                return NvError_BadParameter;
            }
            *((NvU32*)pData) = 4 * 1024;
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "RTSPProtoQueryConfig :: NvCPR_ConfigGetChunkSize::4K"));
            return NvSuccess;
        }
        case NvCPR_GetActualSeekTime:
        {
            if (nDataSize != sizeof(NvU64))
            {
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "RTSPProtoQueryConfig :: NvCPR_ActualSeekTime::NvError_BadParameter"));
                return NvError_BadParameter;
            }
            *(NvU64 *)pData = pContext->nActualSeekTime;
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "RTSPProtoQueryConfig :: NvCPR_ActualSeekTime::%lld", pContext->nActualSeekTime));
            return NvSuccess;
        }
        case NvCPR_GetTimeStamps:
        {
            if (nDataSize != sizeof(CP_QueryConfigTS))
            {
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "RTSPProtoQueryConfig :: NvCPR_GetTimeStamps::NvError_BadParameter"));
                return NvError_BadParameter;
            }
            GetRTSPTimeStamps(pContext->pRtsp, pData);
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "RTSPProtoQueryConfig :: NvCPR_GetTimeStamps"));
            break;
        }
        case NvCPR_GetRTCPAppData:
        {
            packetType = RTCP_APP;
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "RTSPProtoQueryConfig :: NvCPR_GetSDESAppData"));
            break;
        }
        case NvCPR_GetRTCPSdesCname:
        {
            packetType = RTCP_SDES;
            sdestype = RTCP_SDES_CNAME;
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "RTSPProtoQueryConfig :: NvCPR_GetSDESCname"));
            break;
        }
        case NvCPR_GetRTCPSdesName:
        {
            packetType = RTCP_SDES;
            sdestype = RTCP_SDES_NAME;
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "RTSPProtoQueryConfig :: NvCPR_GetSDESName"));
            break;
        }
        case NvCPR_GetRTCPSdesEmail:
        {
            packetType = RTCP_SDES;
            sdestype = RTCP_SDES_EMAIL;
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "RTSPProtoQueryConfig :: NvCPR_GetSDESEmail"));
            break;
        }
        case NvCPR_GetRTCPSdesPhone:
        {
            packetType = RTCP_SDES;
            sdestype = RTCP_SDES_PHONE;
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "RTSPProtoQueryConfig :: NvCPR_GetSDESPhone"));
            break;
        }
        case NvCPR_GetRTCPSdesLoc:
        {
            packetType = RTCP_SDES;
            sdestype = RTCP_SDES_LOC;
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "RTSPProtoQueryConfig :: NvCPR_GetSDESLoc"));
            break;
        }
        case NvCPR_GetRTCPSdesTool:
        {
            packetType = RTCP_SDES;
            sdestype = RTCP_SDES_TOOL;
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "RTSPProtoQueryConfig :: NvCPR_GetSDESTool"));
            break;
        }
        case NvCPR_GetRTCPSdesNote:
        {
            packetType = RTCP_SDES;
            sdestype = RTCP_SDES_NOTE;
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "RTSPProtoQueryConfig :: NvCPR_GetSDESNote"));
            break;
        }
        case NvCPR_GetRTCPSdesPriv:
        {
            packetType = RTCP_SDES;
            sdestype = RTCP_SDES_PRIV;
            NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "RTSPProtoQueryConfig :: NvCPR_GetSDESPriv"));
            break;
        }
    default:
        break;
    }

    if (NvCPR_GetRTCPAppData == eQuery ||
            NvCPR_GetRTCPSdesCname == eQuery ||
            NvCPR_GetRTCPSdesName == eQuery ||
            NvCPR_GetRTCPSdesEmail == eQuery ||
            NvCPR_GetRTCPSdesPhone == eQuery ||
            NvCPR_GetRTCPSdesLoc == eQuery ||
            NvCPR_GetRTCPSdesTool == eQuery ||
            NvCPR_GetRTCPSdesNote == eQuery ||
            NvCPR_GetRTCPSdesPriv == eQuery)
    {
        GetRTCPSdesData(pContext->pRtsp, packetType, sdestype,pData,nDataSize);
        return NvSuccess;
    }
    return NvError_NotSupported;
}

static NvError RTSPProtoSetPause(NvCPRHandle hContent, int bPause)
{
    RTSPProtoHandle *pContext = (RTSPProtoHandle *)hContent;

    NV_ASSERT(pContext);

    if (bPause)
        RTSPPause(pContext->pRtsp);

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_VERBOSE, "RTSPProtoSetPause :: bPause  = %d", bPause));
    return NvSuccess;
}

static NV_CUSTOM_PROTOCOL s_RTSPProtocol =
{
    RTSPProtoGetVersion,
    RTSPProtoOpen,
    RTSPProtoClose,
    RTSPProtoSetPosition,
    RTSPProtoGetPosition,
    RTSPProtoRead,
    RTSPProtoWrite,
    RTSPProtoGetSize,
    RTSPProtoIsStreaming,
    RTSPProtoProbeParserType,
    RTSPProtoQueryConfig,
    RTSPProtoSetPause
};

void NvGetRTSPProtocol(NV_CUSTOM_PROTOCOL **proto)
{
    *proto = &s_RTSPProtocol;
}

