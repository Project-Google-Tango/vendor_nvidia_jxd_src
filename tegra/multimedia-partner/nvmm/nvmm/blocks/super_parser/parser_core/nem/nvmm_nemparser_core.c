/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvassert.h"
#include "nvos.h"

#include "nvrm_transport.h"
#include "nvrm_hardware_access.h"

#include "nvmm.h"
#include "nvmm_event.h"
#include "nvmm_block.h"
#include "nvmm_queue.h"
#include "nvmm_debug.h"
#include "nvmm_util.h"
#include "nvmm_parser_core.h"
#include "nvmm_nemparser_core.h"
#include "nvcustomprotocol.h"
#include "nvmm_contentpipe.h"
#include "nvmm_aac.h"
#include "nvmm_common.h"
#include "nvmm_metadata.h"
#include "nvmm_sock.h"

#define NVLOG_CLIENT NVLOG_NEM_PARSER

#define MAX_STREAMS 2

#define NUM_CHANNELS_SUPPORTED      6

#define AAC_DEC_MAX_INPUT_BUFFER_SIZE       (768 * NUM_CHANNELS_SUPPORTED)
#define AAC_DEC_MIN_INPUT_BUFFER_SIZE       (768 * NUM_CHANNELS_SUPPORTED)

typedef struct NvMMNemParserPacketRec
{
    NEM_PACKET_HEADER header;
    NvU8 *buffer;
    NvU32 bufSize;
} NvMMNemParserPacket;

/* Context for Nvmm parser core */
typedef struct NvMMNemParserCoreContextRec
{
    /* Members for Rtsp core parser */
    NvString szFilename;

    CPhandle cphandle;
    CP_PIPETYPE_EXTENDED *pPipe;

    NvMMAttrib_ParseRate rate;
    NvMMAttrib_ParsePosition position;

    NEM_FILE_HEADER oFileHeader;
    NEM_AUDIO_FORMAT_HEADER oAudioHeader;
    NEM_VIDEO_FORMAT_HEADER oVideoHeader;

    NEM_PACKET_HEADER oStoredHeader;
    int hasStoredHeader;

    NvU32 SkipFrameCount;

#define TYPE_AUDIO 1
#define TYPE_VIDEO 2
    NvU32 streamMapping[MAX_STREAMS];
    NvU32 numstreams;
    NvU64 duration;
    NvU8 seenFirstPacket[MAX_STREAMS];
    NvU8 sentFirstPacket[MAX_STREAMS];

#define PACKET_QUEUE_SIZE 512
    NvMMQueueHandle emptyPacketQueue[MAX_STREAMS];
    NvMMQueueHandle fullPacketQueue[MAX_STREAMS];
    NvMMNemParserPacket *packetList[MAX_STREAMS][PACKET_QUEUE_SIZE];

#define PACKET_LEN 2048
    NvU8 tmpBuffer[PACKET_LEN];
} NvMMNemParserCoreContext;

static NvError NemCoreParserOpen(NvMMParserCoreHandle hParserCore, NvString pFilename)
{
    NvError Status = NvSuccess;
    NvU32 StringLength = 0;
    NvMMNemParserCoreContext *pContext;
    NvU32 i;
    NEM_PACKET_HEADER header;

    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));

    pContext = NvOsAlloc(sizeof(NvMMNemParserCoreContext));
    NVMM_CHK_MEM(pContext);
    NvOsMemset(pContext, 0, sizeof(NvMMNemParserCoreContext));

    StringLength = NvOsStrlen(pFilename) + 1;

    pContext->szFilename = NvOsAlloc(StringLength);
    NVMM_CHK_MEM(pContext->szFilename);
    NvOsStrncpy(pContext->szFilename, pFilename, StringLength);

    NvmmGetFileContentPipe(&pContext->pPipe);

    NVMM_CHK_ERR (pContext->pPipe->cpipe.Open(&pContext->cphandle,
                                                            pContext->szFilename, CP_AccessRead));

    NVMM_CHK_ERR (pContext->pPipe->InitializeCP(pContext->cphandle,
                                           hParserCore->MinCacheSize,
                                           hParserCore->MaxCacheSize,
                                           hParserCore->SpareCacheSize,
                                           (CPuint64 *)&hParserCore->pActualCacheSize));

    NVMM_CHK_ERR (pContext->pPipe->cpipe.Read(pContext->cphandle,
                                   (CPbyte *)&pContext->oFileHeader,
                                   sizeof(NEM_FILE_HEADER)));

    NVMM_CHK_ARG(pContext->oFileHeader.magic == NEM_FOURCC('N', 'v', 'M', 'M'))

    pContext->numstreams = pContext->oFileHeader.numStreams;
    NVMM_CHK_ARG(pContext->numstreams > 0 && pContext->numstreams <= MAX_STREAMS);

    for (i = 0; i < pContext->numstreams; i++)
    {
        NVMM_CHK_ERR (pContext->pPipe->cpipe.Read(pContext->cphandle,
                                       (CPbyte *)&header,
                                       sizeof(NEM_PACKET_HEADER)));

        switch (header.packetType)
        {
            case NEM_TWOCC('a', 'h'):
                NVMM_CHK_ERR (pContext->pPipe->cpipe.Read(pContext->cphandle,
                                               (CPbyte *)&pContext->oAudioHeader,
                                               sizeof(NEM_AUDIO_FORMAT_HEADER)));

                pContext->streamMapping[header.streamIndex] = TYPE_AUDIO;
                pContext->duration = pContext->oAudioHeader.duration;

                NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "Audio Stream: duration = %lld, type = 0x%x",
                    pContext->duration, pContext->oAudioHeader.type));

                break;
            case NEM_TWOCC('v', 'h'):
                NVMM_CHK_ERR (pContext->pPipe->cpipe.Read(pContext->cphandle,
                                               (CPbyte *)&pContext->oVideoHeader,
                                               sizeof(NEM_VIDEO_FORMAT_HEADER)));

                pContext->streamMapping[header.streamIndex] = TYPE_VIDEO;
                pContext->duration = pContext->oVideoHeader.duration;
                NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "Video Stream: duration = %lld, type = 0x%x",
                    pContext->duration, pContext->oVideoHeader.type));

                break;
            default:
                NvOsMemcpy(&pContext->oStoredHeader, &header, sizeof(NEM_PACKET_HEADER));
                pContext->hasStoredHeader = NV_TRUE;
                i=pContext->numstreams;
                break;
        }
    }

    for (i = 0; i < pContext->numstreams; i++)
    {
        int j;

       NVMM_CHK_ERR (NvMMQueueCreate(&pContext->emptyPacketQueue[i],
                                         PACKET_QUEUE_SIZE,
                                         sizeof(NvMMNemParserPacket *),
                                         NV_FALSE));

        NVMM_CHK_ERR (NvMMQueueCreate(&pContext->fullPacketQueue[i],
                                         PACKET_QUEUE_SIZE,
                                         sizeof(NvMMNemParserPacket *),
                                         NV_FALSE));

        for (j = 0; j < PACKET_QUEUE_SIZE; j++)
        {
            pContext->packetList[i][j] = NvOsAlloc(sizeof(NvMMNemParserPacket));
            NVMM_CHK_MEM(pContext->packetList[i][j]);

            NvOsMemset(pContext->packetList[i][j], 0,
                       sizeof(NvMMNemParserPacket));
            pContext->packetList[i][j]->bufSize = PACKET_LEN;
            pContext->packetList[i][j]->buffer = NvOsAlloc(PACKET_LEN + 1);
            NVMM_CHK_MEM(pContext->packetList[i][j]->buffer);

            NvMMQueueEnQ(pContext->emptyPacketQueue[i],
                         &(pContext->packetList[i][j]), 0);
        }
    }

    pContext->rate.Rate  = 1000;
    hParserCore->pContext = pContext;

    /* FIX this , why declare these handles everywhere? */
    hParserCore->pPipe    = pContext->pPipe;
    hParserCore->cphandle = pContext->cphandle;
    hParserCore->bUsingMultipleStreamReads = NV_TRUE;

cleanup:
    if (Status != NvSuccess)
    {
        NvOsFree(pContext->szFilename);
        pContext->szFilename = NULL;

        if (pContext->cphandle)
        {
            pContext->pPipe->cpipe.Close(pContext->cphandle);
            pContext->cphandle = NULL;
        }

        for (i = 0; i < pContext->numstreams; i++)
        {
            int j;

            if (pContext->emptyPacketQueue[i])
                NvMMQueueDestroy(&pContext->emptyPacketQueue[i]);
            if (pContext->fullPacketQueue[i])
                NvMMQueueDestroy(&pContext->fullPacketQueue[i]);

            for (j = 0; j < PACKET_QUEUE_SIZE; j++)
            {
                if (pContext->packetList[i][j])
                    NvOsFree(pContext->packetList[i][j]->buffer);
                NvOsFree(pContext->packetList[i][j]);
            }
        }

        NvOsFree(pContext);
    }

    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
    return Status;
}

static NvError NemCoreParserGetNumberOfStreams(NvMMParserCoreHandle hParserCore, NvU32 *pStreams)
{
    NvError Status = NvSuccess;
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext && pStreams);

    *pStreams = ( (NvMMNemParserCoreContext *)hParserCore->pContext)->numstreams;

cleanup:
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
    return Status;
}

static NvError NemCoreParserGetStreamInfo(NvMMParserCoreHandle hParserCore, NvMMStreamInfo **pInfo)
{
    NvError Status = NvSuccess;
    NvU32 i;
    NvMMStreamInfo *curStreamInfo = NULL;
    NvMMNemParserCoreContext* pContext= NULL;
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext && pInfo);

    curStreamInfo = *pInfo;
    pContext = (NvMMNemParserCoreContext *)hParserCore->pContext;

    for (i = 0; i < pContext->numstreams; i++)
    {
        if (pContext->streamMapping[i] == TYPE_AUDIO)
        {
            curStreamInfo[i].StreamType = pContext->oAudioHeader.type;
            curStreamInfo[i].BlockType  = NvMMBlockType_SuperParser;
            curStreamInfo[i].TotalTime  = pContext->oAudioHeader.duration;
            curStreamInfo[i].NvMMStream_Props.AudioProps.BitRate  = pContext->oAudioHeader.bitRate;
            NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "AUDIO :: Stream type = 0x%x\nTotal Time = %lld\nBitrate = %d",
                curStreamInfo[i].StreamType,
                curStreamInfo[i].TotalTime,
                curStreamInfo[i].NvMMStream_Props.AudioProps.BitRate));
        }
        else if (pContext->streamMapping[i] == TYPE_VIDEO)
        {
            curStreamInfo[i].StreamType = pContext->oVideoHeader.type;
            if ((curStreamInfo[i].StreamType == NvStreamType_H263) || (curStreamInfo[i].StreamType == NvStreamType_MPEG4))
                 curStreamInfo[i].StreamType = NvMMStreamType_MPEG4;
            curStreamInfo[i].BlockType  = NvMMBlockType_SuperParser;
            curStreamInfo[i].TotalTime  = pContext->oVideoHeader.duration;
            curStreamInfo[i].NvMMStream_Props.VideoProps.Width =  pContext->oVideoHeader.width;
            curStreamInfo[i].NvMMStream_Props.VideoProps.Height = pContext->oVideoHeader.height;
            if( (pContext->oVideoHeader.width == 0) &&
                (pContext->oVideoHeader.height == 0))
            {
                curStreamInfo[i].NvMMStream_Props.VideoProps.Width = 176;
                curStreamInfo[i].NvMMStream_Props.VideoProps.Height = 144;
            }
            curStreamInfo[i].NvMMStream_Props.VideoProps.VideoBitRate  = pContext->oVideoHeader.bitRate;
            NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "VIDEO :: Stream type = 0x%x\nTotal Time = %lld\nBitrate = %d",
                curStreamInfo[i].StreamType,
                curStreamInfo[i].TotalTime,
                curStreamInfo[i].NvMMStream_Props.VideoProps.VideoBitRate));
        }
        // fill in other info if possible
    }

cleanup:
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));
    return Status;
}

static NvError NemCoreParserSetRate(NvMMParserCoreHandle hParserCore, NvS32 rate)
{
    NvError Status = NvSuccess;
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext);

    ((NvMMNemParserCoreContext *)hParserCore->pContext)->rate.Rate = rate;

cleanup:
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
    return Status;
}

static NvS32 NemCoreParserGetRate(NvMMParserCoreHandle hParserCore)
{
    NvError Status = NvSuccess;
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext);

cleanup:
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
    return (Status == NvSuccess) ?
                    ((NvMMNemParserCoreContext *)hParserCore->pContext)->rate.Rate : 0;
}

static NvError NemCoreParserSetPosition(NvMMParserCoreHandle hParserCore, NvU64 *pTimeStamp)
{
    NvError Status = NvSuccess;
    NvMMNemParserCoreContext* pContext = NULL;
    NvU32 i;
    NvU64 uDiffTime = 0;

    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext && pTimeStamp);
    pContext = (NvMMNemParserCoreContext *)hParserCore->pContext;

    if (pContext->duration <= 0 ||
        *pTimeStamp >= pContext->duration)
    {
         NV_LOGGER_PRINT((NVLOG_NEM_PARSER,NVLOG_DEBUG, "%s[%d] : Status = NvError_ParserEndOfStream, line = %d",
            __FUNCTION__, __LINE__));
         Status = NvError_ParserEndOfStream;
         goto cleanup;
    }

    if (*pTimeStamp <= pContext->position.Position)
        uDiffTime = pContext->position.Position - *pTimeStamp;
    else
        uDiffTime = *pTimeStamp -  pContext->position.Position;

    if (uDiffTime <= 10000000)
        return NvSuccess;

    Status = pContext->pPipe->SetPosition64(pContext->cphandle,
                                            *pTimeStamp,
                                            CP_OriginTime);
    pContext->pPipe->GetConfig(pContext->cphandle,
                               CP_ConfigQueryActualSeekTime,
                               &(pContext->position.Position));

    pContext->hasStoredHeader = NV_FALSE;

    // discard stored packets
    for (i = 0; i < pContext->numstreams; i++)
    {
        while (NvMMQueueGetNumEntries(pContext->fullPacketQueue[i]) > 0)
        {
            NvMMNemParserPacket *storedPacket;
            NvError err;

            err = NvMMQueueDeQ(pContext->fullPacketQueue[i],
                               &storedPacket);
            if (NvSuccess == err)
            {
                NvMMQueueEnQ(pContext->emptyPacketQueue[i],
                             &storedPacket, 0);
            }
        }
        pContext->seenFirstPacket[i]=0; // discard the consumption of first packet so that new config packets are resent
    }

    hParserCore->StoredDataSize = 0;

    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "%s[%d] : SeekTime %lld\tActualSeekTime %lld\n",
                __FUNCTION__, __LINE__, *pTimeStamp, pContext->position.Position));

cleanup:
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
    return Status;
}

static NvError
NemCoreParserGetPosition(
    NvMMParserCoreHandle hParserCore,
    NvU64 *pTimeStamp)
{
    NvError Status = NvSuccess;
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext && pTimeStamp);

    *pTimeStamp = ((NvMMNemParserCoreContext *)hParserCore->pContext)->position.Position;

cleanup:
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
    return Status;
}

static NvError
NemCoreParserGetNextWorkUnit(
    NvMMParserCoreHandle hParserCore,
    NvU32 streamIndex,
    NvMMBuffer *pBuffer,
    NvU32 *size,
    NvBool *pMoreWorkPending)
{
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "++--%s[%d]", __FUNCTION__, __LINE__));
    return NvError_ParserFailure;
}

static NvError
NemCoreParserGetNextWorkUnitMultiple(
    NvMMParserCoreHandle hParserCore,
    NvU32 *streamIndex,
    NvMMBuffer **pBuffers,
    NvU32 *size,
    NvBool *pMoreWorkPending)
{
    NvError Status = NvSuccess;
    NvMMNemParserCoreContext* pContext = NULL;
    NvU8 EOH = 0, EOD = 0;
    NvU32 i;
    NEM_PACKET_HEADER header;
    NvMMBuffer *pCurBuffer;
    CP_CHECKBYTESRESULTTYPE eResult;
    int checktimes = 0;
    NvU32 readFromStore = NV_FALSE;
    NvU8 *storeBuffer = NULL;
    NvU8 SkipFrame = 0;
    // we require minimum 2K bytes to read from CCP
    NvU32 nMinBytes = 2048;

    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext);
    pContext = (NvMMNemParserCoreContext *)hParserCore->pContext;

    *pMoreWorkPending = NV_TRUE;
    *streamIndex = pContext->numstreams;
    *size = 0;

    while (!EOH && !EOD)
    {
        storeBuffer = NULL;
        readFromStore = NV_FALSE;

        // check for stored packets that we can process
        for (i = 0; i < pContext->numstreams; i++)
        {
            if (NvMMQueueGetNumEntries(pContext->fullPacketQueue[i]) > 0 &&
                pBuffers[i])
            {
                NvMMNemParserPacket *storedPacket;
                NvError err;

                err = NvMMQueueDeQ(pContext->fullPacketQueue[i],
                                   &storedPacket);
                if (NvSuccess == err)
                {
                    NvOsMemcpy(&header, &storedPacket->header,
                               sizeof(NEM_PACKET_HEADER));
                    readFromStore = NV_TRUE;
                    storeBuffer = storedPacket->buffer;

                    NvMMQueueEnQ(pContext->emptyPacketQueue[i],
                                 &storedPacket, 0);
                    hParserCore->StoredDataSize -= storedPacket->header.size;

                    goto headerdone;
                }
                Status = NvError_ParserEndOfStream;
                goto cleanup;
            }
        }

        if (pContext->hasStoredHeader)
        {
            NvOsMemcpy(&header, &pContext->oStoredHeader, sizeof(NEM_PACKET_HEADER));
            pContext->hasStoredHeader = NV_FALSE;
            goto headerdone;
        }

        if (pContext->duration != 0  &&  pContext->duration < 120000000)// required only for low duration streams < 12Sec's
        {
            for (i = 0; i < pContext->numstreams; i++)
            {
                if (NvMMQueueGetNumEntries(pContext->fullPacketQueue[i]) > 0)
                    return Status;
            }
        }

        if((pContext->numstreams == 1) && (pContext->streamMapping[0] == TYPE_AUDIO))
        {
            // If only AUDIO, check for availability of atleast 512 bytes
            nMinBytes = 512;
        }

        checktimes = 600;
        while (checktimes >= 0)
        {
            if (NvMMSockGetBlockActivity())
                return NvError_ParserEndOfStream;

            pContext->pPipe->cpipe.CheckAvailableBytes(pContext->cphandle,
                                                       nMinBytes,
                                                       &eResult);
            if (eResult == CP_CheckBytesNotReady)
            {
                checktimes--;
                NV_LOGGER_PRINT((NVLOG_NEM_PARSER,NVLOG_DEBUG,  "%s[%d] : CP_CheckBytesNotReady, checktimes = %d",
                    __FUNCTION__, __LINE__, checktimes));
                NvOsSleepMS(10);
            }
            else
                break;
        }
        if (eResult == CP_CheckBytesAtEndOfStream)
        {
            // If we have reached to the end of stream, check if there are any stored packets in the fullqueue 
            // if so, return Success so that when buffers are available these stored packets can be sent to the decoder
            // else return EndOfStream.
            NV_LOGGER_PRINT((NVLOG_NEM_PARSER,NVLOG_DEBUG,  "%s[%d] : CP_CheckBytesAtEndOfStream, checktimes = %d",
                __FUNCTION__, __LINE__, checktimes));
            Status = NvError_ParserEndOfStream;
            for (i = 0; i < pContext->numstreams; i++)
            {
                if (NvMMQueueGetNumEntries(pContext->fullPacketQueue[i]) != 0)
                {
                    Status = NvSuccess;
                }
            }
            goto cleanup;
        }


        // read packet header
        Status = pContext->pPipe->cpipe.Read(pContext->cphandle,
                                       (CPbyte *)&header,
                                       sizeof(NEM_PACKET_HEADER));
        if (Status != NvSuccess)
        {
             NV_LOGGER_PRINT((NVLOG_NEM_PARSER,NVLOG_DEBUG,  "%s[%d] : Status = 0x%x, NvError_ParserEndOfStream",
                __FUNCTION__, __LINE__, Status));
                Status = NvError_ParserEndOfStream;
                goto cleanup;
        }

headerdone:

        if (header.packetType != NEM_TWOCC('d', 'a') ||
            header.streamIndex >= pContext->numstreams)
        {
             NV_LOGGER_PRINT((NVLOG_NEM_PARSER,NVLOG_DEBUG,  "%s[%d] : Status = 0x%x, NvError_ParserEndOfStream",
                __FUNCTION__, __LINE__, Status));
                Status = NvError_ParserEndOfStream; //fixme handle this
                goto cleanup;
        }

        pCurBuffer = pBuffers[header.streamIndex];

        if (!pCurBuffer)
        {
            NvMMNemParserPacket *storePacket;
            NvError err;

            if (!pContext->seenFirstPacket[header.streamIndex])
                goto juststore;

            err = NvMMQueueDeQ(pContext->emptyPacketQueue[header.streamIndex],
                               &storePacket);
            if (NvSuccess == err)
            {
                if (storePacket->bufSize < header.size)
                {
                    NvU8 *buf;

                    buf = NvOsAlloc(header.size + 1);
                    if (!buf)
                        return NvError_ParserEndOfStream;
                    NvOsFree(storePacket->buffer);
                    storePacket->buffer = buf;
                    storePacket->bufSize = header.size;
                }

                NvOsMemcpy(&storePacket->header, &header,
                           sizeof(NEM_PACKET_HEADER));

                while (header.size > 0)
                {
                    NvU32 readsize = header.size;
                    Status = pContext->pPipe->cpipe.Read(pContext->cphandle,
                                                 (CPbyte *)storePacket->buffer,
                                                         readsize);
                    header.size -= readsize;
                    if (Status != NvSuccess)
                    {
                         NV_LOGGER_PRINT((NVLOG_NEM_PARSER,NVLOG_DEBUG,  "%s[%d] : Status = 0x%x, NvError_ParserEndOfStream",
                            __FUNCTION__, __LINE__, Status));
                            Status = NvError_ParserEndOfStream;
                            goto cleanup;
                    }
                }

                hParserCore->StoredDataSize += storePacket->header.size;
                NvMMQueueEnQ(pContext->fullPacketQueue[header.streamIndex],
                             &storePacket, 0);

                continue;
            }

juststore:
            // if we're out of queue space, just bail
            NvOsMemcpy(&pContext->oStoredHeader, &header, sizeof(NEM_PACKET_HEADER));
            pContext->hasStoredHeader = NV_TRUE;
            return NvSuccess;
        }

        if (SkipFrame)
        {
            header.flags |= NEM_PACKET_FLAG_SKIPPACKET;
        }
        else if (header.size + pCurBuffer->Payload.Ref.sizeOfValidDataInBytes >
            pCurBuffer->Payload.Ref.sizeOfBufferInBytes)
        {
            SkipFrame = 1;
            header.flags |= NEM_PACKET_FLAG_SKIPPACKET;
        }

        if (header.flags & NEM_PACKET_FLAG_ENDOFPACKET)
        {
            NV_LOGGER_PRINT((NVLOG_NEM_PARSER,NVLOG_DEBUG,  "%s[%d] : NEM_PACKET_FLAG_ENDOFPACKET",
                __FUNCTION__, __LINE__));

                EOD = 1;
                *streamIndex = header.streamIndex;
        }

        if (header.flags & NEM_PACKET_FLAG_SKIPPACKET)
        {

            NV_LOGGER_PRINT((NVLOG_NEM_PARSER,NVLOG_DEBUG,  "%s[%d] : NEM_PACKET_FLAG_SKIPPACKET",
                __FUNCTION__, __LINE__));

            while (header.size > 0 && !readFromStore)
            {
                NvU32 readsize = PACKET_LEN;
                if (readsize > header.size)
                    readsize = header.size;
                Status = pContext->pPipe->cpipe.Read(pContext->cphandle,
                                                     (CPbyte *)pContext->tmpBuffer, readsize);
                header.size -= readsize;
                if (Status != NvSuccess)
                {
                     NV_LOGGER_PRINT((NVLOG_NEM_PARSER,NVLOG_DEBUG,  "%s[%d] : Status = 0x%x, NvError_ParserEndOfStream",
                        __FUNCTION__, __LINE__, Status));
                        Status = NvError_ParserEndOfStream;
                        goto cleanup;
                }
            }

            if (pCurBuffer)
            {
                pCurBuffer->Payload.Ref.sizeOfValidDataInBytes = 0;
            }

            continue;
        }

        if (!pContext->seenFirstPacket[header.streamIndex])
        {
            pContext->seenFirstPacket[header.streamIndex] = 1;
            NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG,  "%s[%d] : First Packet Seen",
                __FUNCTION__, __LINE__));
            if (pContext->streamMapping[header.streamIndex] == TYPE_AUDIO &&
                (pContext->oAudioHeader.type == NvStreamType_AAC ||
                pContext->oAudioHeader.type == NvStreamType_AACSBR))
            {
                NvMMAudioAacTrackInfo aacTrackInfo;
                NEM_AAC_FORMAT_HEADER aacHeader;

                NvOsMemset(&aacHeader, 0, sizeof(NEM_AAC_FORMAT_HEADER));

                pContext->pPipe->cpipe.Read(pContext->cphandle,
                                            (CPbyte *)(&aacHeader), sizeof(NEM_AAC_FORMAT_HEADER));

                if (!pContext->sentFirstPacket[header.streamIndex])
                {
                    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG,
                        "--NemCoreParserGetNextWorkUnitMultiple :: AAC HeaderInfo  Sent To Decoder"));
                    NvOsMemset(&aacTrackInfo, 0, sizeof(NvMMAudioAacTrackInfo));
                    aacTrackInfo.profile = 0x40;
                    aacTrackInfo.objectType = aacHeader.objectType;
                    aacTrackInfo.samplingFreqIndex = aacHeader.samplingFreqIndex;
                    aacTrackInfo.samplingFreq = aacHeader.samplingFreq;
                    aacTrackInfo.channelConfiguration = aacHeader.channelConfiguration;
                    NvOsMemcpy(pCurBuffer->Payload.Ref.pMem, &aacTrackInfo, sizeof(NvMMAudioAacTrackInfo));
                    *size = sizeof(NvMMAudioAacTrackInfo);
                    pCurBuffer->Payload.Ref.sizeOfValidDataInBytes = *size;
                    pCurBuffer->PayloadInfo.BufferFlags |= NvMMBufferFlag_HeaderInfoFlag;

                    EOH = 1;
                    *streamIndex = header.streamIndex;
                    pContext->sentFirstPacket[header.streamIndex]=1;
                    break;
                }
                else // skip the packets as they are already consumed
                {
                    pCurBuffer->Payload.Ref.sizeOfValidDataInBytes = 0;
                    continue;
                }
            } 
        }

        if (header.size + pCurBuffer->Payload.Ref.sizeOfValidDataInBytes <=
            pCurBuffer->Payload.Ref.sizeOfBufferInBytes)
        {
            NvU8 *cur = (NvU8*)(pCurBuffer->Payload.Ref.pMem) +
                        pCurBuffer->Payload.Ref.sizeOfValidDataInBytes;

            if (readFromStore && storeBuffer)
            {
                NvOsMemcpy(cur, storeBuffer, header.size);
                readFromStore = NV_FALSE;
                storeBuffer = NULL;
            }
            else
            {
                Status = pContext->pPipe->cpipe.Read(pContext->cphandle,
                                               (CPbyte *)cur, header.size);
            }

            pCurBuffer->Payload.Ref.sizeOfValidDataInBytes += header.size;
        }
    }

    if (EOD)
    {
        if (SkipFrame)
        {
            pCurBuffer->Payload.Ref.sizeOfValidDataInBytes=0;
            pContext->SkipFrameCount++;
            if (pContext->SkipFrameCount > 30)
            {
                Status = NvError_ParserEndOfStream;
                goto cleanup;
            }
        }
        *size = pCurBuffer->Payload.Ref.sizeOfValidDataInBytes;
        pCurBuffer->PayloadInfo.TimeStamp = header.timestamp;
        pContext->position.Position = header.timestamp;
        pCurBuffer->PayloadInfo.BufferFlags = 0;
        if ((pContext->streamMapping[header.streamIndex] == TYPE_VIDEO) &&
            (pContext->oVideoHeader.type == NvStreamType_H264))
        {
            pCurBuffer->PayloadInfo.BufferMetaDataType = NvMBufferMetadataType_H264;
            pCurBuffer->PayloadInfo.BufferMetaData.H264BufferMetadata.NALUSizeFieldWidthInBytes = 4 ;
            pCurBuffer->PayloadInfo.BufferMetaData.H264BufferMetadata.BufferFormat = NvH264BufferFormat_RTPStreamFormat;
        }
    }

    pCurBuffer->Payload.Ref.startOfValidData = 0;

cleanup:
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));

    if (Status == NvError_ParserEndOfStream  && pContext->position.Position < pContext->duration -6000000 && pContext->duration)
    {
        Status = NvError_ParserReadFailure;
    }
    else if (Status == NvError_ParserEndOfStream && pContext->position.Position < 10000000) //one second
    {
           Status = NvError_ParserReadFailure;
    }
   return Status;
}


static NvError
NemCoreParserClose(
    NvMMParserCoreHandle hParserCore)
{
    NvError Status = NvSuccess;
    NvMMNemParserCoreContext* pContext = NULL;
    NvU32 i;

    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext);
    pContext = (NvMMNemParserCoreContext *)hParserCore->pContext;

    if (pContext->cphandle)
    {
        pContext->pPipe->cpipe.Close(pContext->cphandle);
        pContext->cphandle = NULL;
    }

    for (i = 0; i < pContext->numstreams; i++)
    {
        int j;

        if (pContext->emptyPacketQueue[i])
            NvMMQueueDestroy(&pContext->emptyPacketQueue[i]);
        if (pContext->fullPacketQueue[i])
            NvMMQueueDestroy(&pContext->fullPacketQueue[i]);

        for (j = 0; j < PACKET_QUEUE_SIZE; j++)
        {
            if (pContext->packetList[i][j])
                NvOsFree(pContext->packetList[i][j]->buffer);
            NvOsFree(pContext->packetList[i][j]);
        }
    }

    NvOsFree(pContext->szFilename);
    NvOsFree(pContext);

    hParserCore->pContext = NULL;

cleanup:
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
    return Status;
}

static void
NemCoreParserGetMaxOffsets(
    NvMMParserCoreHandle hParserCore,
    NvU32 *pMaxOffsets)
{
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "++--%s[%d]", __FUNCTION__, __LINE__));
    *pMaxOffsets = 0;
}


static NvBool
NemCoreParserGetBufferRequirements(
    NvMMParserCoreHandle hParserCore,
    NvU32 StreamIndex,
    NvU32 Retry,
    NvMMNewBufferRequirementsInfo *pBufReq)
{
    NvError Status = NvSuccess;
    NvMMNemParserCoreContext* pContext = NULL;
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext && Retry == 0);
    pContext = (NvMMNemParserCoreContext *)hParserCore->pContext;

    NvOsMemset (pBufReq, 0, sizeof(NvMMNewBufferRequirementsInfo));

    pBufReq->structSize    = sizeof(NvMMNewBufferRequirementsInfo);
    pBufReq->event         = NvMMEvent_NewBufferRequirements;
    pBufReq->minBuffers    = 8;
    pBufReq->maxBuffers    = NVMMSTREAM_MAXBUFFERS;
    pBufReq->memorySpace   = NvMMMemoryType_SYSTEM;
    pBufReq->byteAlignment = 4;

    pBufReq->minBufferSize = 65536;
    pBufReq->maxBufferSize = 65536;

    if (pContext && StreamIndex < pContext->numstreams)
    {
        if (pContext->streamMapping[StreamIndex] == TYPE_AUDIO)
        {
            if (pContext->oAudioHeader.type == NvStreamType_AAC ||
                pContext->oAudioHeader.type == NvStreamType_AACSBR)
            {
                pBufReq->minBufferSize = AAC_DEC_MIN_INPUT_BUFFER_SIZE;
                pBufReq->maxBufferSize = AAC_DEC_MAX_INPUT_BUFFER_SIZE;
            }
            else
            {
                pBufReq->minBufferSize = 1024;
                pBufReq->maxBufferSize = 8192;
            }
        }
        else if (pContext->streamMapping[StreamIndex] == TYPE_VIDEO)
        {
            if (pContext->oVideoHeader.type == NvStreamType_MPEG4 ||
                pContext->oVideoHeader.type == NvStreamType_H263 ||
                pContext->oVideoHeader.type == NvStreamType_H264 ||
                pContext->oVideoHeader.type == NvStreamType_MJPEGA ||
                pContext->oVideoHeader.type == NvStreamType_MJPEGA)
            {
                if ( (pContext->oVideoHeader.width == 0) || (pContext->oVideoHeader.height== 0))
                {
                    pBufReq->minBufferSize = 32768;
                    pBufReq->maxBufferSize = 512000 * 2 - 32768;
                }
                else
                {
                    if ( (pContext->oVideoHeader.width > 320) && (pContext->oVideoHeader.height > 240))
                    {
                        pBufReq->minBufferSize = (pContext->oVideoHeader.width * pContext->oVideoHeader.height * 3) >> 2;
                        pBufReq->maxBufferSize = (pContext->oVideoHeader.width * pContext->oVideoHeader.height * 3) >> 2;
                    }
                    else
                    {
                        pBufReq->minBufferSize = (pContext->oVideoHeader.width * pContext->oVideoHeader.height * 3) >> 1;
                        pBufReq->maxBufferSize = (pContext->oVideoHeader.width * pContext->oVideoHeader.height * 3) >> 1;
                    }
                }
                if(pBufReq->maxBufferSize < 1024)
                {
                    pBufReq->maxBufferSize = 1024;
                    pBufReq->minBufferSize = 1024;
                }
            }
        }
    }

cleanup:
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
    return (Status == NvSuccess) ? NV_TRUE : NV_FALSE;
}

static NvError
NemCoreParserGetAttribute(
    NvMMParserCoreHandle hParserCore,
    NvU32 AttributeType,
    NvU32 AttributeSize,
    void *pAttribute)
{
    NvError Status = NvSuccess;
    NvMMNemParserCoreContext* pContext = NULL;
    NvMMMetaDataInfo *pMetaAttr = NULL;
    CP_ConfigProp rtcpProp;
    CP_ConfigIndex nIndex;
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext);
    pContext = (NvMMNemParserCoreContext *)hParserCore->pContext;

    switch (AttributeType)
    {
        case NvMMAttribute_Metadata:
        {
            pMetaAttr = (NvMMMetaDataInfo *) pAttribute;
            switch (pMetaAttr->eMetadataType)
            {
                case NvMMMetaDataInfo_RtcpAppData:
                {
                    nIndex = CP_ConfigQueryRTCPAppData;
                    break;
                }
                case NvMMMetaDataInfo_RTCPSdesCname:
                {
                    nIndex = CP_ConfigQueryRTCPSdesCname;
                    break;
                }
                case NvMMMetaDataInfo_RTCPSdesName:
                {
                    nIndex = CP_ConfigQueryRTCPSdesName;
                    break;
                }
                case NvMMMetaDataInfo_RTCPSdesEmail:
                {
                    nIndex = CP_ConfigQueryRTCPSdesEmail;
                    break;
                }
                case NvMMMetaDataInfo_RTCPSdesPhone:
                {
                    nIndex = CP_ConfigQueryRTCPSdesPhone;
                    break;
                }
                case NvMMMetaDataInfo_RTCPSdesLoc:
                {
                    nIndex = CP_ConfigQueryRTCPSdesLoc;
                    break;
                }
                case NvMMMetaDataInfo_RTCPSdesTool:
                {
                    nIndex = CP_ConfigQueryRTCPSdesTool;
                    break;
                }
                case NvMMMetaDataInfo_RTCPSdesNote:
                {
                    nIndex = CP_ConfigQueryRTCPSdesNote;
                    break;
                }
                case NvMMMetaDataInfo_RTCPSdesPriv:
                {
                    nIndex = CP_ConfigQueryRTCPSdesPriv;
                    break;
                }
                default:
                return NvError_UnSupportedMetadata;
            }
            NvOsMemset(&rtcpProp, 0, sizeof(CP_ConfigProp));
            rtcpProp.prop = (char *)pMetaAttr->pClientBuffer;
            rtcpProp.propSize= pMetaAttr->nBufferSize;
            if ( NvSuccess == (NvError)pContext->pPipe->GetConfig (pContext->cphandle, nIndex, &rtcpProp) )
            {
                if(NULL == rtcpProp.prop && 0 == rtcpProp.propSize )
                    return NvError_UnSupportedMetadata;

                if (pMetaAttr->pClientBuffer == NULL || (pMetaAttr->nBufferSize < rtcpProp.propSize))
                {
                    pMetaAttr->nBufferSize = rtcpProp.propSize;
                    return NvError_InSufficientBufferSize;
                }
                pMetaAttr->eEncodeType = NvMMMetaDataEncode_Utf8;
            }
            else
                return NvError_MetadataFailure;

            break;
        }
        default:
            break;
    }

cleanup:
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
    return Status;
}

NvError
NvMMCreateNemCoreParser(
    NvMMParserCoreHandle *hParserCore,
    NvMMParserCoreCreationParameters *pParams)
{
    NvError Status = NvSuccess;
    NvMMParserCore *pParserCore = NULL;
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));

    pParserCore = NvOsAlloc(sizeof(NvMMParserCore));
    NVMM_CHK_MEM(pParserCore);

    *hParserCore = pParserCore;
    NvOsMemset(pParserCore, 0, sizeof(NvMMParserCore));

    pParserCore->Open               = NemCoreParserOpen;
    pParserCore->GetNumberOfStreams = NemCoreParserGetNumberOfStreams;
    pParserCore->GetStreamInfo      = NemCoreParserGetStreamInfo;
    pParserCore->SetRate            = NemCoreParserSetRate;
    pParserCore->GetRate            = NemCoreParserGetRate;
    pParserCore->SetPosition        = NemCoreParserSetPosition;
    pParserCore->GetPosition        = NemCoreParserGetPosition;
    pParserCore->GetNextWorkUnit    = NemCoreParserGetNextWorkUnit;
    pParserCore->Close              = NemCoreParserClose;
    pParserCore->GetMaxOffsets      = NemCoreParserGetMaxOffsets;
    pParserCore->GetAttribute       = NemCoreParserGetAttribute;
    pParserCore->GetBufferRequirements = NemCoreParserGetBufferRequirements;
    pParserCore->eCoreType          = NvMMParserCoreType_NemParser;
    pParserCore->UlpKpiMode         = pParams->UlpKpiMode;
    pParserCore->bEnableUlpMode     = pParams->bEnableUlpMode;
    pParserCore->bReduceVideoBuffers= pParams->bReduceVideoBuffers;
    pParserCore->GetNextWorkUnitMultipleBufs = NemCoreParserGetNextWorkUnitMultiple;

cleanup:
    if (Status != NvSuccess)
    {
        *hParserCore = NULL;
    }
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
    return Status;
}

void NvMMDestroyNemCoreParser(NvMMParserCoreHandle hParserCore)
{
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));
    if (hParserCore)
    {
        NvOsFree(hParserCore);
    }
    NV_LOGGER_PRINT((NVLOG_NEM_PARSER, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
}

