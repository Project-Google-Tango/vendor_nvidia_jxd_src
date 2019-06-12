/*
 * Copyright (c) 2008-2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvmm_mp3parser.h"
#include "nvmm_common.h"
#include "nvrm_transport.h"
#include "nvrm_hardware_access.h"
#include "nvmm.h"
#include "nvmm_event.h"
#include "nvmm_block.h"
#include "nvmm_queue.h"
#include "nvmm_debug.h"
#include "nvmm_util.h"
#include "nvmm_parser_core.h"
#include "nvmm_mp3parser_core.h"
#include "nvmm_ulp_util.h"
#include "nvmm_ulp_kpi_logger.h"
#include "nvmm_contentpipe.h"

#define MP3_DEC_MAX_INPUT_BUFFER_SIZE       2 * 2048
#define MP3_DEC_MIN_INPUT_BUFFER_SIZE       2 * 2048

/* Context for mp3 parser core */
typedef struct NvMMMp3ParserCoreContextRec
{
    NvMp3Parser Parser;
    NvBool bHasBufferToDeliver;
    NvU32 BufferSize;
    NvBool   bSentEOS;
    NvU64 ParserTimeStamp;
    NvS32 UpdataTimeStamp;
    NvMMAttrib_ParseRate rate;
    NvMMAttrib_ParsePosition Position;
    // Indicates file reached to EOF while seeking
    NvBool bFileReachedEOF;
    /* DeferSeek flag will set when actual seek is failed*/
    NvBool DeferSeek;
    NvU64 RequestedSeekTime;
    NvU32 SCBufSize;
    NvBool InitCalled;
} NvMMMp3ParserCoreContext;

static NvError
Mp3CoreParserGetTotalTrackTime (
    NvMMParserCoreHandle hParserCore,
    NvU64* pTotalTime)
{
    NvError Status = NvSuccess;
    NvMMMp3ParserCoreContext* pContext = NULL;

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext && pTotalTime);
    pContext = (NvMMMp3ParserCoreContext *) hParserCore->pContext;

    *pTotalTime =   pContext->Parser.StreamInfo.TrackInfo.TotalTime;

cleanup:
    return Status;
}

static NvError
Mp3CoreParserClose (
    NvMMParserCoreHandle hParserCore)
{
    NvError Status = NvSuccess;
    NvMMMp3ParserCoreContext* pContext = NULL;
    NvU32 count = 0;
    NvU64 time1 =  0;
    NvF64 time2 =  0;

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext);
    pContext = (NvMMMp3ParserCoreContext *) hParserCore->pContext;

    pContext->BufferSize = 0;

    NvMp3ParserDeInit (&pContext->Parser);

    if (hParserCore->UlpKpiMode)
    {
        // Parser is being closed, so set Idle End time.
        NvmmUlpUpdateKpis (KpiFlag_IdleEnd, NV_FALSE, NV_FALSE, 0);

        // Calculate the KPIs and print them on console.
        NvmmUlpKpiGetParseTimeIdleTimeRatio (&time2);
        NvmmUlpKpiGetAverageTimeBwReadRequests (&time1);
        NvmmUlpKpiGetTotalReadRequests (&count);
        NvmmUlpKpiPrintAllKpis();

        // Call KPI logger Deinit here.
        NvmmUlpKpiLoggerDeInit();
    }

    NvOsFree (hParserCore->pContext);
    hParserCore->pContext = NULL;

cleanup:
    return Status;
}

static NvError
Mp3CoreParserOpen (
    NvMMParserCoreHandle hParserCore,
    NvString pFilePath)
{
    NvError Status = NvSuccess;
    NvU64 TotalTrackTime    = 0;
    NvMMMp3ParserCoreContext *pContext = NULL;
    NvU32 MetaDataInterval = 0;

    NVMM_CHK_ARG (hParserCore && pFilePath);

    pContext = NvOsAlloc (sizeof (NvMMMp3ParserCoreContext));
    NVMM_CHK_MEM (pContext);
    NvOsMemset (pContext, 0, sizeof (NvMMMp3ParserCoreContext));
    hParserCore->pContext = (void*) pContext;

    if ( (hParserCore->MinCacheSize == 0) &&
            (hParserCore->MaxCacheSize == 0) &&
            (hParserCore->SpareCacheSize == 0))
    {
        hParserCore->bUsingCachedCP = NV_FALSE;
        hParserCore->UlpKpiMode     = NV_FALSE;
        hParserCore->bEnableUlpMode = NV_FALSE;
    }
    else
    {
        hParserCore->bUsingCachedCP = NV_TRUE;
    }

    NVMM_CHK_ERR (NvMp3ParserInit (&pContext->Parser, pFilePath));

    if (hParserCore->bUsingCachedCP)
    {
        NVMM_CHK_ERR (pContext->Parser.pPipe->InitializeCP (pContext->Parser.hContentPipe,
                hParserCore->MinCacheSize,
                hParserCore->MaxCacheSize,
                hParserCore->SpareCacheSize,
                (CPuint64 *) &hParserCore->pActualCacheSize));
    }

    if (pContext->Parser.pPipe->IsStreaming (pContext->Parser.hContentPipe))
    {
        pContext->Parser.pPipe->GetConfig (pContext->Parser.hContentPipe, CP_ConfigQueryMetaInterval, &MetaDataInterval);
    }

    pContext->Parser.IsStreaming = pContext->Parser.pPipe->IsStreaming (pContext->Parser.hContentPipe);
    if (pContext->Parser.IsStreaming)
    {
        pContext->Parser.SCMetaDataInterval = MetaDataInterval;

        if (pContext->Parser.SCMetaDataInterval > 0)
            pContext->Parser.IsShoutCastStreaming = NV_TRUE;
        else
            pContext->Parser.IsShoutCastStreaming = NV_FALSE;

        if (pContext->Parser.IsShoutCastStreaming)
        {
            pContext->SCBufSize = ReadBufferSize_2KB;
            pContext->Parser.SCRemainder = pContext->Parser.SCMetaDataInterval % pContext->SCBufSize;
            if (pContext->Parser.SCRemainder > 0)
            {
                pContext->Parser.SCDivData = pContext->Parser.SCMetaDataInterval - pContext->Parser.SCRemainder;
            }
        }
    }

    Status = NvMp3ParserParse (&pContext->Parser);
    if (Status != NvSuccess && !pContext->Parser.IsStreaming)
    {
        goto cleanup;
    }

    Status = NvSuccess;

    if ( (pContext->Parser.StreamInfo.TrackInfo.Layer == NvMp3Layer_LAYER2) || (pContext->Parser.StreamInfo.TrackInfo.Layer == NvMp3Layer_LAYER1))
    {
        hParserCore->bEnableUlpMode = NV_FALSE;
    }

    if (hParserCore->bEnableUlpMode == NV_TRUE)
    {
        //If it is ULP mode, set the buffer size to 0K at first.
        //This will be fine tuned during series of read requests.
        pContext->BufferSize = 0;

        if (hParserCore->UlpKpiMode == KpiMode_Normal || hParserCore->UlpKpiMode == KpiMode_Tuning)
        {
            NvmmUlpKpiLoggerInit();
            NvmmUlpSetKpiMode (hParserCore->UlpKpiMode);
            NvmmUlpUpdateKpis (KpiFlag_IdleStart, NV_FALSE, NV_FALSE, 0);
        }
        else
        {
            hParserCore->UlpKpiMode = 0;
        }
    }
    else
    {
        // Kpi Mode is not enabled for non-ULP mode.
        hParserCore->UlpKpiMode = 0;
        //Make the default size as 2KB
        pContext->BufferSize = ReadBufferSize_2KB;
    }

    if (hParserCore->UlpKpiMode)
    {
        Mp3CoreParserGetTotalTrackTime (hParserCore, &TotalTrackTime);
        // Store the song duration in 100 nano seconds.
        NvmmUlpKpiSetSongDuration (TotalTrackTime * SEC_100NS);
    }

    pContext->rate.Rate  = 1000;
    pContext->InitCalled = NV_TRUE;

    /* FIX this , why declare these handles everywhere? */
    hParserCore->pPipe    = pContext->Parser.pPipe;
    hParserCore->cphandle = pContext->Parser.hContentPipe;

cleanup:

    if (Status != NvSuccess)
    {
        (void) Mp3CoreParserClose (hParserCore);
    }

    return Status;
}

static NvError
Mp3CoreParserGetNumberOfStreams (
    NvMMParserCoreHandle hParserCore,
    NvU32 *pStreamCount)
{
    NvError Status = NvSuccess;
    NvMMMp3ParserCoreContext* pContext = NULL;

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext && pStreamCount);
    pContext = (NvMMMp3ParserCoreContext *) hParserCore->pContext;

    *pStreamCount =   pContext->Parser.NumberOfTracks;

cleanup:
    return Status;
}


static NvError
Mp3CoreParserGetStreamInfo (
    NvMMParserCoreHandle hParserCore, 
    NvMMStreamInfo **pStreamInfo)
{
    NvError Status = NvSuccess;
    NvU64 TotalTrackTime = 0;
    NvMMMp3ParserCoreContext* pContext = NULL;

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext &&  pStreamInfo);
    pContext = (NvMMMp3ParserCoreContext *) hParserCore->pContext;

    NVMM_CHK_ARG (pContext->Parser.NumberOfTracks == 1);
    
    Mp3CoreParserGetTotalTrackTime (hParserCore, &TotalTrackTime);

    pStreamInfo[0]->BlockType = NvMMBlockType_SuperParser;
    pStreamInfo[0]->TotalTime = ( (NvU64) TotalTrackTime * 10000);

    if ( (pContext->Parser.StreamInfo.TrackInfo.Layer == NvMp3Layer_LAYER2) || (pContext->Parser.StreamInfo.TrackInfo.Layer == NvMp3Layer_LAYER1))
    {
        pStreamInfo[0]->StreamType = NvMMStreamType_MP2;
    }
    else
    {
        pStreamInfo[0]->StreamType = NvMMStreamType_MP3;
    }

    pStreamInfo[0]->NvMMStream_Props.AudioProps.SampleRate    = pContext->Parser.StreamInfo.TrackInfo.SamplingFrequency;
    pStreamInfo[0]->NvMMStream_Props.AudioProps.BitRate       = pContext->Parser.StreamInfo.TrackInfo.BitRate;
    pStreamInfo[0]->NvMMStream_Props.AudioProps.NChannels     = pContext->Parser.StreamInfo.TrackInfo.NumChannels;
    pStreamInfo[0]->NvMMStream_Props.AudioProps.BitsPerSample = pContext->Parser.StreamInfo.TrackInfo.SampleSize;

cleanup:
    return Status;
}

static NvError
Mp3CoreParserSetRate(
    NvMMParserCoreHandle hParserCore, 
    NvS32 Rate)
{
    NvError Status = NvSuccess;
    NvMMMp3ParserCoreContext* pContext = NULL;

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext);
    pContext = (NvMMMp3ParserCoreContext *) hParserCore->pContext;
    
    pContext->rate.Rate = Rate;

cleanup:
    return Status;
}

static NvS32
Mp3CoreParserGetRate(
    NvMMParserCoreHandle hParserCore)
{
    NvError Status = NvSuccess;
    NvMMMp3ParserCoreContext* pContext = NULL;

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext);
    pContext = (NvMMMp3ParserCoreContext *) hParserCore->pContext;
        
cleanup:
    return (Status == NvSuccess) ? pContext->rate.Rate : NvError_ParserFailure;    
}

static NvError
Mp3CoreParserSetPosition(
    NvMMParserCoreHandle hParserCore, 
    NvU64 *pTimeStamp)
{
    NvError Status = NvSuccess;
    NvU32 MilliSec;
    NvU32 DivFactor = 10000;
    NvU64 TotalTime = 0;
    NvU64 TotalTrackTime = 0;
    NvMMMp3ParserCoreContext* pContext = NULL;

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext);
    pContext = (NvMMMp3ParserCoreContext *) hParserCore->pContext;
    
    pContext->bFileReachedEOF = NV_FALSE;

    /* Timestamp in 100 Nano Seconds and converting in to Milli sec pTimeStamp before calling NvSeekToTime
    since mp3 expects in milli sec */
    MilliSec = (NvU32) ( (*pTimeStamp) / DivFactor);
    NVMM_CHK_ERR (Mp3CoreParserGetTotalTrackTime(hParserCore, &TotalTrackTime));
    TotalTime = ( (NvU64) TotalTrackTime * 10000);
    if (*pTimeStamp > TotalTime)
    {
        pContext->bFileReachedEOF = NV_TRUE;
        // Update the parse position with TotalTime of the track
        pContext->Position.Position = TotalTime;
        return NvSuccess;
    }
    else if (*pTimeStamp == 0)
    {
        // CEPlayer will call seek(0) for repeat mode hence reset the flag
        if (hParserCore->bEnableUlpMode == NV_FALSE)
        {
            pContext->Parser.CurrFileOffset = 0;
            pContext->Parser.LastFrameBoundary = 0;
        }
        pContext->bFileReachedEOF = NV_FALSE;

    }

    Status = NvMp3ParserSeekToTime(&pContext->Parser, &MilliSec);
    *pTimeStamp = (NvU64)((NvU64)MilliSec * DivFactor);
    if (Status == NvError_EndOfFile)
    {
        pContext->bFileReachedEOF = NV_TRUE;
        Status = NvSuccess;
    }
    else if (Status != NvSuccess)
    {
        pContext->DeferSeek = NV_TRUE;
        pContext->RequestedSeekTime = *pTimeStamp;
        return NvError_ParserFailedToGetData;
    }

    pContext->ParserTimeStamp = *pTimeStamp;
    pContext->Position.Position = *pTimeStamp;
    pContext->UpdataTimeStamp = 1;
    if (hParserCore->bEnableUlpMode == NV_TRUE)
    {
        pContext->BufferSize = 0;
    }

cleanup:
    return Status;
}

static NvError
Mp3CoreParserGetPosition(
    NvMMParserCoreHandle hParserCore, 
    NvU64 *pTimeStamp)
{
    NvError Status = NvSuccess;
    NvMMMp3ParserCoreContext* pContext = NULL;

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext);
    pContext = (NvMMMp3ParserCoreContext *) hParserCore->pContext;
    
    * pTimeStamp = pContext->Position.Position;

cleanup:
    return Status;
}

static NvU8 *
SearchSCMetaData (
    NvU8 *pBuf,
    NvU8 *mData)
{
    NvU8 *Pos = pBuf;

    if (!mData)
        return Pos;

    while (*Pos)
    {
        if (NV_TO_UPPER (*pBuf) == NV_TO_UPPER (*mData))
        {
            NvU8 * p1 = Pos + 1;
            NvU8 * p2 = mData + 1;

            while (*p1 && *p2 &&  NV_TO_UPPER (*p1) == NV_TO_UPPER (*p2))
            {
                p1++;
                p2++;
            }
            if (!*p2) return Pos;
        }
        Pos++;
    }

    return NULL;
}

static NvError
ParseICYMetaData(
    NvMp3Parser *pNvMp3Parser, 
    NvMMBuffer *pBuffer)
{
    NvError Status = NvSuccess;
    NvU8* pDataBuff = NULL, *pBuf = NULL, *MetaPtr = NULL;
    NvU32 AskBytes = 1;
    NvU32 MetaDataSize = 0;
    NvU32 Len = 0;

    NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.ReadBuffer (pNvMp3Parser->hContentPipe, (CPbyte**)&pDataBuff , (CPuint*)&AskBytes, 0));
    NVMM_CHK_ARG(pDataBuff != NULL && *pDataBuff != 0);

    MetaDataSize = *pDataBuff * 16;
    NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.ReadBuffer (pNvMp3Parser->hContentPipe, (CPbyte**) & pBuf , (CPuint*)&MetaDataSize, 0));

    /* Search for StreamTitle= */
    MetaPtr = SearchSCMetaData (pBuf, (NvU8*) "StreamTitle=");

    if (MetaPtr)
    {
        MetaPtr += NvOsStrlen ("StreamTitle=") + 1;
        for (Len=0; MetaPtr[Len] != ';'; Len++);
        {
             MetaPtr[Len] = NV_TO_UPPER (MetaPtr[Len]);
        }
        
        if (pNvMp3Parser->Id3TagData.ShoutCast.pClientBuffer)
        {
            NvOsFree (pNvMp3Parser->Id3TagData.ShoutCast.pClientBuffer);
            pNvMp3Parser->Id3TagData.ShoutCast.nBufferSize = 0;
        }

        MetaDataSize =  Len - 1;
        if (MetaDataSize > 0)
        {
            pNvMp3Parser->Id3TagData.ShoutCast.pClientBuffer = NvOsAlloc(MetaDataSize);
            NvOsMemcpy (pNvMp3Parser->Id3TagData.ShoutCast.pClientBuffer, MetaPtr, MetaDataSize);
            pNvMp3Parser->Id3TagData.ShoutCast.nBufferSize = MetaDataSize;
            pNvMp3Parser->Id3TagData.ShoutCast.eEncodeType = NvMMMetaDataEncode_Utf8;
        }

        Status =  NvError_NewMetaDataAvailable;
    }
    else
    {
        pNvMp3Parser->pPipe->cpipe.SetPosition (pNvMp3Parser->hContentPipe, -1*(MetaDataSize + 1), CP_OriginCur);
    }

cleanup:

    if (pDataBuff)
    {
        pNvMp3Parser->pPipe->cpipe.ReleaseReadBuffer (pNvMp3Parser->hContentPipe, (CPbyte*) pDataBuff);
    }

    if (pBuf)
    {
        pNvMp3Parser->pPipe->cpipe.ReleaseReadBuffer (pNvMp3Parser->hContentPipe, (CPbyte*) pBuf);
    }

    return Status;
}


static NvError
Mp3CoreParserGetNextWorkUnit (
    NvMMParserCoreHandle hParserCore,
    NvU32 streamIndex,
    NvMMBuffer *pBuffer,
    NvU32 *pSize,
    NvBool *pMoreWorkPending)
{
    NvError Status = NvSuccess;
    NvU32 AskBytes = 0;
    CPuint64 AskBytes64 = 0;
    CP_CHECKBYTESRESULTTYPE eResult = CP_CheckBytesOk;
    CPbyte* pDataBuff = NULL;
    NvMMPayloadMetadata payload;
    NvU64 TimeStamp = 0;
    NvS32 BitRate = 0;
    NvU32 requiredSize  = 0;
    CP_PIPETYPE* cPipe = NULL;
    NvBool IsEndOfSong = NV_FALSE;
    NvMp3Parser *pMp3Parser = NULL;
    NvBool UlpMode;
    NvMMMp3ParserCoreContext* pContext = NULL;

    NV_LOGGER_PRINT((NVLOG_AVI_PARSER, NVLOG_VERBOSE, "++%s[%d]", __FUNCTION__, __LINE__));

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext && pMoreWorkPending);
    pContext = (NvMMMp3ParserCoreContext *) hParserCore->pContext;

    payload.BufferFlags = 0;
    pMp3Parser = &pContext->Parser;
    UlpMode = hParserCore->bEnableUlpMode;
    cPipe = &pContext->Parser.pPipe->cpipe;
    // Initially pMoreWorkPending is set to FALSE
    *pMoreWorkPending = NV_FALSE;

    if (pContext->bFileReachedEOF)
    {
        NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_INFO, "%s[%d]: NvError_ParserEndOfStream", __FUNCTION__, __LINE__));
        return NvError_ParserEndOfStream;
    }

    if (pBuffer)
    {
        // End of Idle Time and Start of Parse time
        NvmmUlpUpdateKpis ( (KpiFlag_IdleEnd | KpiFlag_ParseStart), NV_FALSE, NV_FALSE, 0);

        if (UlpMode == NV_TRUE)
        {
            if (!hParserCore->bUsingCachedCP)
            {
                // Send first buffer with 1 MB size
                if (pContext->BufferSize < ReadBufferSize_1MB)
                {
                    pContext->BufferSize = ReadBufferSize_1MB;
                }
                else
                {
                    pContext->BufferSize = ReadBufferSize_2MB;
                }
            }
            else
            {
                if (!pMp3Parser->IsStreaming)
                {
                    pContext->BufferSize = ReadBufferSize_256KB;
                }
                else
                {
                    if (pMp3Parser->IsShoutCastStreaming)
                    {
                        pContext->BufferSize = pContext->SCBufSize;

                        if ( (pMp3Parser->SCMetaDataInterval > 0) && (pMp3Parser->SCRemainder > 0))
                        {
                            if (pMp3Parser->SCDivData == pMp3Parser->SCOffset)
                            {
                                pContext->BufferSize = pMp3Parser->SCRemainder;
                            }
                        }
                    }
                    else
                    {
                        pContext->BufferSize = ReadBufferSize_16KB * 2;
                    }
                }
            }
        }
        else //non-ULP mode
        {
            if (pMp3Parser->IsStreaming)
            {
                if (pMp3Parser->IsShoutCastStreaming)
                {
                    pContext->BufferSize = pContext->SCBufSize;

                    if ( (pMp3Parser->SCMetaDataInterval > 0) && (pMp3Parser->SCRemainder > 0))
                    {
                        if (pMp3Parser->SCDivData == pMp3Parser->SCOffset)
                        {
                            pContext->BufferSize = pMp3Parser->SCRemainder;
                        }
                    }
                }
            }
            if (pContext->BufferSize == 0)
                pContext->BufferSize = ReadBufferSize_2KB;
        }

        // Set ask bytes to the calculated buffer size.
        AskBytes = pContext->BufferSize;
        // Calculate the required size and check if the nvmm buffer meets the requirement
        requiredSize = (UlpMode == NV_TRUE) ? sizeof (NvMMOffsetList) : AskBytes;
        //Return the required size, if the buffer received is less.
        if (pBuffer->Payload.Ref.sizeOfBufferInBytes < requiredSize)
        {
            // returning required minimum size
            *pSize = requiredSize;
            NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "%s[%d]: NvError_InSufficientBufferSize", __FUNCTION__, __LINE__));
            return NvError_InSufficientBufferSize;
        }
        // Check if requested size is available in the file.
        Status = cPipe->CheckAvailableBytes (pContext->Parser.hContentPipe, AskBytes, &eResult);
        // Check if we reached end of stream.
        if (eResult == CP_CheckBytesAtEndOfStream)
        {
            NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_INFO, "%s[%d]: NvError_ParserEndOfStream", __FUNCTION__, __LINE__));
            return NvError_ParserEndOfStream;
        }
        else if (eResult == CP_CheckBytesNotReady)
        {
            NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "%s[%d]: NvError_ContentPipeNotReady", __FUNCTION__, __LINE__));        
            return NvError_ContentPipeNotReady;
        }

        //Calculate bitrate
        BitRate = pMp3Parser->StreamInfo.TrackInfo.BitRate;
        //Calculate timestamp.
        TimeStamp = (NvU64) ( (8.0f * AskBytes * 10000000) / BitRate);
        pContext->ParserTimeStamp += TimeStamp;

        if (eResult == CP_CheckBytesInsufficientBytes)
        {
            NVMM_CHK_ERR (pContext->Parser.pPipe->GetAvailableBytes (pContext->Parser.hContentPipe, &AskBytes64));
            NVMM_CHK_ARG (AskBytes64 <= (CPuint64) 0xffffffff);
            AskBytes = (NvU32) AskBytes64;

            if (hParserCore->UlpKpiMode)
            {
                /* We have reached end of song so set the flag to true. This will be used to calculate kpis */
                IsEndOfSong = NV_TRUE;
            }
        }

        //DeferSeek is set when actual SetPostion is failed due to unavalible memory
        if (pContext->DeferSeek)
        {
            NvU32 MilliSec = 0;
            NvU32 DivFactor = 10000;
            MilliSec = (NvU32) ( (pContext->RequestedSeekTime) / DivFactor);

            // Now seek the file to the required new packet (seeking now after the abort buffers(OMX Specific)
            Status = NvMp3ParserSeekToTime (&pContext->Parser, &MilliSec);

            pContext->DeferSeek = NV_FALSE;
        }


        if ( (pMp3Parser->IsStreaming) && (pMp3Parser->IsShoutCastStreaming) && (pMp3Parser->SCMetaDataInterval > 0) && (pMp3Parser->SCOffset >= pMp3Parser->SCMetaDataInterval))
        {
            Status = ParseICYMetaData (pMp3Parser, pBuffer);
            pMp3Parser->SCOffset = 0;
            if (Status == NvError_NewMetaDataAvailable)
            {
                NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "%s[%d]: NvError_NewMetaDataAvailable", __FUNCTION__, __LINE__));
                return Status;
            }
        }

        if ( (eResult == CP_CheckBytesInsufficientBytes) || (eResult == CP_CheckBytesOk))
        {
            if (UlpMode == NV_TRUE)
            {
                pBuffer->PayloadInfo.BufferFlags |= NvMMBufferFlag_OffsetList;

                if (pContext->UpdataTimeStamp == 1)
                {
                    payload.TimeStamp = pContext->ParserTimeStamp - TimeStamp;
                    pContext->UpdataTimeStamp = 0;
                    pContext->ParserTimeStamp = 0;
                }
                else
                    payload.TimeStamp = -1;

                // End of Parse Time and Start of Read time
                NvmmUlpUpdateKpis ( (KpiFlag_ParseEnd | KpiFlag_ReadStart),
                                    NV_FALSE, NV_TRUE, AskBytes);

                if (!pMp3Parser->IsStreaming)
                {
                    if (pContext->InitCalled)
                    {
                        // Only Enable silence algo for Audio files
                        payload.BufferFlags |= NvMMBufferFlag_EnableGapless;
                        pContext->InitCalled = NV_FALSE;
                    }
                    if (pMp3Parser->CurrFileOffset == pContext->Parser.LastFrameBoundary)
                    {
                        NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_VERBOSE, "%s[%d]: curr file offset (%llu) == LastFrameBoundary (%llu)", 
                                                      pMp3Parser->CurrFileOffset,  pContext->Parser.LastFrameBoundary, __FUNCTION__, __LINE__));
                        AskBytes = 0;
                        pMp3Parser->CurrFileOffset = 0;
                        pContext->bFileReachedEOF = 1;
                        NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_INFO, "%s[%d]: NvError_ParserEndOfStream", __FUNCTION__, __LINE__));
                        return NvError_ParserEndOfStream;
                    }

                    if ( (pMp3Parser->CurrFileOffset + AskBytes) > pContext->Parser.LastFrameBoundary)
                    {
                        // Flag to detect silence on last_N_ frames/buffers
                        payload.BufferFlags |= NvMMBufferFlag_Gapless;
                        AskBytes = (NvU32) (pContext->Parser.LastFrameBoundary - pMp3Parser->CurrFileOffset);
                    }
                }

                // Read the data from the file
                NVMM_CHK_ERR (cPipe->ReadBuffer (pContext->Parser.hContentPipe, &pDataBuff , (CPuint*) & AskBytes, 0));
                
                if (!pMp3Parser->IsStreaming)
                {
                    pMp3Parser->CurrFileOffset += AskBytes;
                    NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_VERBOSE, "%s[%d]: CurrFileOffset: %llu", pMp3Parser->CurrFileOffset, __FUNCTION__, __LINE__));
                }
                
                // End of Read Time and Start of Parse time
                NvmmUlpUpdateKpis ( (KpiFlag_ReadEnd | KpiFlag_ParseStart),
                                    IsEndOfSong, NV_TRUE, AskBytes);

                // Setting ReadBuffer Base Address
                (void) NvmmSetReadBuffBaseAddress (pBuffer, pDataBuff);

                NvmmPushToOffsetList (pBuffer, (NvU32) (pDataBuff), AskBytes, payload);
                // Set the OffsetList status as true in the end
                NvmmSetOffsetListStatus (pBuffer, NV_TRUE);
                *pSize = sizeof (NvMMOffsetList);
            }
            else
            {
                pBuffer->Payload.Ref.startOfValidData = 0;
                pBuffer->Payload.Ref.sizeOfValidDataInBytes = 0;

                if (!pMp3Parser->IsStreaming)
                {
                    if (pContext->InitCalled)
                    {
                        // Only Enable silence algo for Audio files
                        payload.BufferFlags |= NvMMBufferFlag_EnableGapless;
                        pContext->InitCalled = NV_FALSE;
                    }
                    if (pContext->Parser.LastFrameBoundary > 0)
                    {
                        if (pMp3Parser->CurrFileOffset == pContext->Parser.LastFrameBoundary)
                        {
                            AskBytes = 0;
                            pMp3Parser->CurrFileOffset = 0;
                            pContext->Parser.LastFrameBoundary = 0;
                            NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_INFO, "%s[%d]: NvError_ParserEndOfStream", __FUNCTION__, __LINE__));
                            return NvError_ParserEndOfStream;
                        }

                        if ( (pMp3Parser->CurrFileOffset + AskBytes) > pContext->Parser.LastFrameBoundary)
                        {
                            // Flag to detect silence on last_N_ frames/buffers
                            payload.BufferFlags |= NvMMBufferFlag_Gapless;
                            AskBytes = (NvU32) (pContext->Parser.LastFrameBoundary - pMp3Parser->CurrFileOffset);
                        }
                    }
                }
                
                NVMM_CHK_CP (cPipe->Read (pContext->Parser.hContentPipe, (CPbyte*) pBuffer->Payload.Ref.pMem, AskBytes));
                if (Status == NvError_EndOfFile)
                {
                    NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_INFO, "%s[%d]: NvError_ParserEndOfStream", __FUNCTION__, __LINE__));
                    return NvError_ParserEndOfStream;
                }
                    

                if (!pMp3Parser->IsStreaming)
                {
                    if (pContext->Parser.LastFrameBoundary > 0)
                    {
                        pMp3Parser->CurrFileOffset += AskBytes;
                        NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_VERBOSE, "%s[%d]: CurrFileOffset: %llu", pMp3Parser->CurrFileOffset, __FUNCTION__, __LINE__));
                    }
                }

                pBuffer->Payload.Ref.sizeOfValidDataInBytes = AskBytes;

                if (pContext->UpdataTimeStamp == 1)
                {
                    pBuffer->PayloadInfo.TimeStamp = pContext->ParserTimeStamp - TimeStamp;
                    pContext->UpdataTimeStamp = 0;
                    pContext->ParserTimeStamp = 0;
                }
                else
                    pBuffer->PayloadInfo.TimeStamp = -1;

                *pSize = AskBytes;
            }

            if ( (pMp3Parser->IsStreaming) && (pMp3Parser->IsShoutCastStreaming) && (pMp3Parser->SCMetaDataInterval > 0))
            {
                pMp3Parser->SCOffset += AskBytes;
            }
        }


        // End of Parse Time and Start of Idle time
        NvmmUlpUpdateKpis ( (KpiFlag_ParseEnd | KpiFlag_IdleStart),
                            NV_FALSE, NV_FALSE, 0);
    }

cleanup:
    NV_LOGGER_PRINT((NVLOG_AVI_PARSER, NVLOG_VERBOSE, "--%s[%d]", __FUNCTION__, __LINE__));    
    return Status;
}

static void
Mp3CoreParserGetMaxOffsets (
    NvMMParserCoreHandle hParserCore, 
    NvU32 *pMaxOffsets)
{
    *pMaxOffsets = MAX_OFFSETS;
}

static NvBool
Mp3CoreParserGetBufferRequirements (
    NvMMParserCoreHandle hParserCore,
    NvU32 StreamIndex,
    NvU32 Retry,
    NvMMNewBufferRequirementsInfo *pBufReq)
{
    NvError Status = NvSuccess;
    
    NVMM_CHK_ARG(hParserCore && Retry == 0 && pBufReq);
    
    NvOsMemset (pBufReq, 0, sizeof (NvMMNewBufferRequirementsInfo));

    pBufReq->structSize    = sizeof (NvMMNewBufferRequirementsInfo);
    pBufReq->event         = NvMMEvent_NewBufferRequirements;
    pBufReq->minBuffers    = 4;
    pBufReq->maxBuffers    = NVMMSTREAM_MAXBUFFERS;
    pBufReq->memorySpace   = NvMMMemoryType_SYSTEM;
    pBufReq->byteAlignment = 4;

    if (hParserCore->bEnableUlpMode == NV_TRUE)
    {
        pBufReq->minBufferSize = sizeof (NvMMOffsetList) + (sizeof (NvMMOffset) * MAX_OFFSETS);
        pBufReq->maxBufferSize = sizeof (NvMMOffsetList) + (sizeof (NvMMOffset) * MAX_OFFSETS);
        pBufReq->minBuffers    = 10;
        pBufReq->maxBuffers    = 10;
    }
    else
    {
        pBufReq->minBufferSize = MP3_DEC_MIN_INPUT_BUFFER_SIZE;
        pBufReq->maxBufferSize = MP3_DEC_MAX_INPUT_BUFFER_SIZE;
    }

cleanup:
    return (Status == NvSuccess) ? NV_TRUE : NV_FALSE;
}

static NvError
Mp3CoreParserGetAttribute (
    NvMMParserCoreHandle hParserCore,
    NvU32 AttributeType,
    NvU32 AttributeSize,
    void *pAttribute)
{
    NvError Status = NvSuccess;
    NvU32 MetaSize = 0;
    NvMMMetaDataInfo *pMetaAttr = NULL;
    NvMMMetaDataInfo *pId3MetaData = NULL;
    NvMMMetaDataInfo *pMetaData = NULL;
    NvMMMp3ParserCoreContext* pContext = NULL;
    
    NVMM_CHK_ARG(hParserCore && hParserCore->pContext && pAttribute);
    pContext = (NvMMMp3ParserCoreContext *) hParserCore->pContext;
    
    switch (AttributeType)
    {
        case NvMMAttribute_Metadata:
        {
            pMetaAttr = (NvMMMetaDataInfo *) pAttribute;
            pId3MetaData = pContext->Parser.Id3TagData.Id3MetaData;
            switch (pMetaAttr->eMetadataType)
            {
                case NvMMMetaDataInfo_Artist:
                    pMetaData = &pId3MetaData[NvMp3MetaDataType_ARTIST];
                    break;
                case NvMMMetaDataInfo_Album:
                    pMetaData = &pId3MetaData[NvMp3MetaDataType_ALBUM];
                    break;
                case NvMMMetaDataInfo_Genre:
                    pMetaData = &pId3MetaData[NvMp3MetaDataType_GENRE];
                    break;
                case NvMMMetaDataInfo_Title:
                {
                    if (pContext->Parser.SCMetaDataInterval > 0)
                        pMetaData = & (pContext->Parser.Id3TagData.ShoutCast);
                    else
                        pMetaData = &pId3MetaData[NvMp3MetaDataType_TITLE];
                }
                break;
                case NvMMMetaDataInfo_Year:
                    pMetaData = &pId3MetaData[NvMp3MetaDataType_YEAR];
                    break;
                case NvMMMetaDataInfo_TrackNumber:
                    pMetaData = &pId3MetaData[NvMp3MetaDataType_TRACKNUM];
                    break;
                case NvMMMetaDataInfo_Encoded:
                    pMetaData = &pId3MetaData[NvMp3MetaDataType_ENCODED];
                    break;
                case NvMMMetaDataInfo_Comment:
                    pMetaData = &pId3MetaData[NvMp3MetaDataType_COMMENT];
                    break;
                case NvMMMetaDataInfo_Composer:
                    pMetaData = &pId3MetaData[NvMp3MetaDataType_COMPOSER];
                    break;
                case NvMMMetaDataInfo_Publisher:
                    pMetaData = &pId3MetaData[NvMp3MetaDataType_PUBLISHER];
                    break;
                case NvMMMetaDataInfo_OrgArtist:
                    pMetaData = &pId3MetaData[NvMp3MetaDataType_ORGARTIST];
                    break;
                case NvMMMetaDataInfo_Copyright:
                    pMetaData = &pId3MetaData[NvMp3MetaDataType_COPYRIGHT];
                    break;
                case NvMMMetaDataInfo_Url:
                    pMetaData = &pId3MetaData[NvMp3MetaDataType_URL];
                    break;
                case NvMMMetaDataInfo_Bpm:
                    pMetaData = &pId3MetaData[NvMp3MetaDataType_BPM];
                    break;
                case NvMMMetaDataInfo_AlbumArtist:
                    pMetaData = &pId3MetaData[NvMp3MetaDataType_ALBUMARTIST];
                    break;
                case NvMMMetaDataInfo_CoverArt:
                    pMetaData = & (pContext->Parser.Id3TagData.Mp3CoverArt);
                    break;
                case NvMMMetaDataInfo_CoverArtURL:
                default:
                    return Status;
            }

            NVMM_CHK_ARG (pMetaData)

            MetaSize = pMetaData->nBufferSize;
            if ( (pMetaData->eEncodeType == NvMMMetaDataEncode_Utf8) || (pMetaData->eEncodeType == NvMMMetaDataEncode_Ascii))
                MetaSize = MetaSize + 1;
            else if (pMetaData->eEncodeType == NvMMMetaDataEncode_Utf16)
                MetaSize = MetaSize + 2;

            if (pMetaAttr->pClientBuffer == NULL || pMetaAttr->nBufferSize < MetaSize)
            {
                pMetaAttr->nBufferSize = MetaSize;
                Status = NvError_InSufficientBufferSize;
                goto cleanup;
            }

            // Copy the MetaData
            NvOsMemcpy (pMetaAttr->pClientBuffer, pMetaData->pClientBuffer, pMetaData->nBufferSize);
            if ( (pMetaData->eEncodeType == NvMMMetaDataEncode_Utf8) || (pMetaData->eEncodeType == NvMMMetaDataEncode_Ascii))
                pMetaAttr->pClientBuffer[MetaSize-1] = '\0';
            else if (pMetaData->eEncodeType == NvMMMetaDataEncode_Utf16)
            {
                pMetaAttr->pClientBuffer[MetaSize-2] = '\0';
                pMetaAttr->pClientBuffer[MetaSize-1] = '\0';
            }
            pMetaAttr->nBufferSize = MetaSize;
            pMetaAttr->eEncodeType = pMetaData->eEncodeType;
            break;
        }
        default:
            break;
    }

cleanup:
    return Status;
}

NvError
NvMMCreateMp3CoreParser (
    NvMMParserCoreHandle *hParserCore,
    NvMMParserCoreCreationParameters *pParams)
{
    NvError Status = NvSuccess;
    NvMMParserCore *pParserCore = NULL;

    NVMM_CHK_ARG(hParserCore);
    
    pParserCore = NvOsAlloc(sizeof(NvMMParserCore));
    NVMM_CHK_MEM(pParserCore);    
    NvOsMemset (pParserCore, 0, sizeof (NvMMParserCore));

    pParserCore->Open               = Mp3CoreParserOpen;
    pParserCore->GetNumberOfStreams = Mp3CoreParserGetNumberOfStreams;
    pParserCore->GetStreamInfo      = Mp3CoreParserGetStreamInfo;
    pParserCore->SetRate            = Mp3CoreParserSetRate;
    pParserCore->GetRate            = Mp3CoreParserGetRate;
    pParserCore->SetPosition        = Mp3CoreParserSetPosition;
    pParserCore->GetPosition        = Mp3CoreParserGetPosition;
    pParserCore->GetNextWorkUnit    = Mp3CoreParserGetNextWorkUnit;
    pParserCore->Close              = Mp3CoreParserClose;
    pParserCore->GetMaxOffsets      = Mp3CoreParserGetMaxOffsets;
    pParserCore->GetAttribute       = Mp3CoreParserGetAttribute;
    pParserCore->GetBufferRequirements = Mp3CoreParserGetBufferRequirements;
    pParserCore->eCoreType          = NvMMParserCoreType_Mp3Parser;
    pParserCore->UlpKpiMode         = pParams->UlpKpiMode;
    pParserCore->bEnableUlpMode     = pParams->bEnableUlpMode;
    pParserCore->bReduceVideoBuffers = pParams->bReduceVideoBuffers;

    *hParserCore = pParserCore;

cleanup:
    return Status;

}

void
NvMMDestroyMp3CoreParser (
    NvMMParserCoreHandle hParserCore)
{
    if (hParserCore)
    {
        NvOsFree (hParserCore);
    }
}

