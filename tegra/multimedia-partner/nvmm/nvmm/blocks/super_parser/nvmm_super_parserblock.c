/*
* Copyright (c) 2008 - 2013 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

#include <ctype.h>
#include <string.h>
#include "nvmm.h"
#include "nvmm_event.h"
#include "nvmm_block.h"
#include "nvrm_transport.h"
#include "nvmm_queue.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvmm_debug.h"
#include "nvmm_util.h"
#include "nvmm_ulp_util.h"
#include "nvmm_ulp_kpi_logger.h"
#include "nvmm_super_parserblock.h" 
#include "nvmm_bufmgr.h"
#include "nvmm_videodec.h"
#include "nvmm_file_util.h"
#include "nvmm_customprotocol_internal.h"
#include "nvmm_contentpipe.h"
#include "nvmm_sock.h"

#include "nvmm_aacparser_core.h"
#include "nvmm_amrparser_core.h"
#include "nvmm_asfparser_core.h"
#include "nvmm_aviparser_core.h"
#include "nvmm_mp3parser_core.h"
#include "nvmm_mp4parser_core.h"
#include "nvmm_oggparser_core.h"
#include "nvmm_nemparser_core.h"
#include "nvmm_m2vparser_core.h"
#include "nvmm_wavparser_core.h"
#include "nvmm_mpsparser_core.h"
#include "nvmm_parser.h"
#include "nvmm_common.h"

#define NVLOG_CLIENT NVLOG_SUPER_PARSER // Required for logging information
#define NV_INITIALBUFFERING_CUTOFF_COUNT 20 // Max count to iterate thr StreamingCacheCalculation before putting the graph to PLAY mode.

CPresult NvMMSuperParserCPNotify(void *pClientContext, CP_EVENTTYPE eEvent, CPuint iParam);
static NvError NvMMCreateParserCoreFromTrackInfo(NvMMBlockHandle hBlock, NvMMTrackInfo *pTrackInfo, NvMMParserCoreContext **ppCoreContext);
static void NvMMSuperParserBlockPrivateClose(NvMMBlockHandle hBlock);
static NvError NvMMSuperParserBlockDoWork(NvMMBlockHandle hBlock, NvMMDoWorkCondition condition);
static NvError SuperParserReleaseOffsetList(NvMMBuffer* pListBuffer, NvMMSuperParserBlockContext* pContext, NvMMParserCoreHandle pCore);
static NvError NvMMSuperParserTBTBCallBack(void  *pContext,  NvU32 StreamIndex, NvMMBuffer* buffer);
static NvError 
NvMMSuperParserBlockPrepareNextWorkUnitOffsets(NvMMBlockHandle hBlock,
                                               NvMMParserCoreHandle hParserCore, 
                                               NvU32 streamIndex, 
                                               NvMMBuffer *pBuffer, 
                                               NvU32 *size);

static NvError 
NvMMSuperParserBlockSetAttribute(NvMMBlockHandle hBlock, 
                                 NvU32 AttributeType, 
                                 NvU32 SetAttrFlag, 
                                 NvU32 AttributeSize, 
                                 void *pAttribute);

static NvBool  
NvMMSuperParserBlockGetBufferRequirements(NvMMBlockHandle hBlock, 
                                          NvU32 StreamIndex, 
                                          NvU32 Retry, 
                                          NvMMNewBufferRequirementsInfo *pBufReq);

static NvError 
NvMMSuperParserBlockTransferBufferEventFunction(void *hBlock, 
                                                NvU32 StreamIndex, 
                                                NvMMBufferType BufferType, 
                                                NvU32 BufferSize, 
                                                void* pBuffer,
                                                NvBool *bEventHandled);

static NvError NvMMSuperParserBlockCreateStreams(NvMMBlockHandle hBlock, NvMMParserCoreHandle hParserCore);
static void NvMMDeleteCore(NvMMSuperParserBlockContext *pContext, NvMMParserCoreContext *pCoreContext);
NvError SuperParserCreateCore(NvMMBlockHandle hBlock, NvMMTrackInfo *pTrackInfo, NvMMParserCoreHandle *pCore);
NvError SuperParserCreateCorePvt(NvMMBlockHandle hBlock, NvMMTrackInfo *pTrackInfo,  NvMMParserCoreHandle *pCore);
NvError SuperParserDeleteCore(NvMMBlockHandle hBlock, NvMMParserCoreHandle pCore);
NvError SuperParserDeleteCorePvt(NvMMBlockHandle hBlock, NvMMParserCoreHandle pCore);
NvError SuperParserCreateOffsets(NvMMBlockHandle hBlock, NvMMParserCoreHandle pCore);
NvBool  SuperParserAnyControlMessagesInPrecachingThread(NvMMBlockHandle hBlock, NvBool *bCurrentCoreNeeded);
NvError SuperParserCreateOffsetsPvt(NvMMBlockHandle hBlock, NvMMParserCoreHandle pCore);
NvError SuperParserDeleteOffsets(NvMMBlockHandle hBlock, NvMMParserCoreHandle pCore);
NvError SuperParserDeleteOffsetsPvt(NvMMBlockHandle hBlock, NvMMParserCoreHandle pCore);
NvError SuperParserAdjustParserCoreForSeek(NvMMBlockHandle hBlock, NvMMParserCoreHandle pCore, NvU64 SeekPos);
NvError SuperParserAdjustParserCoreForSeekPvt(NvMMBlockHandle hBlock, NvMMParserCoreHandle pCore, NvU64 SeekPos);
NvError NvMMSuperParserGetCurrentPlaybackTime(NvMMBlockHandle hBlock,  NvU64 *time, NvU32 *TrackNumber);
NvError NvMMSuperParserGetCurrentCoreHandle(NvMMSuperParserBlockContext *pContext, NvMMParserCoreHandle *pCore, NvU32 * pIndex);

#if NV_LOGGER_ENABLED
static const char *StreamTypeString(NvMMStreamType StreamType)
{
    switch (StreamType)
    {
        case NvMMStreamType_MPEG4:
        case NvMMStreamType_MPEG4Ext: return "MPEG4";
        case NvMMStreamType_H264:
        case NvMMStreamType_H264Ext: return "H264";
        case NvMMStreamType_H263: return "H263";
        case NvMMStreamType_WMV: return "WMV";
        case NvMMStreamType_MPEG2V: return "MPEG-2V";
        case NvMMStreamType_MJPEG: return "MJPEG";
        case NvMMStreamType_JPG: return "JPG";
        case NvMMStreamType_MP3: return "MP3";
        case NvMMStreamType_MP2: return "MP2";
        case NvMMStreamType_WAV: return "WAV";
        case NvMMStreamType_AAC: return "AAC";
        case NvMMStreamType_AACSBR: return "AAC-SBR";
        case NvMMStreamType_BSAC: return "BSAC";
        case NvMMStreamType_WMA: return "WMA";
        case NvMMStreamType_WMAPro: return "WMA-PRO";
        case NvMMStreamType_WMALSL: return "WMA-LOSSLESS";
        case NvMMStreamType_WAMR: return "AMR-WB";
        case NvMMStreamType_NAMR: return "AMR-NB";
        case NvMMStreamType_Vorbis: return "VORBIS";
        case NvMMStreamType_ADPCM: return "ADPCM";
        case NvMMStreamType_AC3: return "AC3";
        case NvMMStreamType_MS_MPG4: return "MS-MPG4";
        case NvMMStreamType_QCELP: return "QCELP";
        case NvMMStreamType_EVRC: return "EVRC";
        default: break;
    }

    return "UNKNOWN";
}

void
SuperParserPrintTrackInfo(
    NvMMParserCoreContext *pCoreContext)
{
    NvU32 i;
    NvMMStreamInfo *pStreamInfo;
    NvU64 TotalTime=0;
    
    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "****************** Track Details ********************"));
    
    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "FilePath = %s", pCoreContext->pTrackInfo->pPath));
    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "FileSize = %lld bytes", pCoreContext->FileSize));

    for (i = 0; i < pCoreContext->pCore->totalStreams; i++)
    {
        if (NVMM_ISSTREAMAUDIO(pCoreContext->pCore->pStreamInfo[i].StreamType) || 
            NVMM_ISSTREAMVIDEO(pCoreContext->pCore->pStreamInfo[i].StreamType))
        {        
            if (TotalTime < pCoreContext->pCore->pStreamInfo[i].TotalTime) 
            {
                TotalTime = pCoreContext->pCore->pStreamInfo[i].TotalTime;
            }     
        }
    }
    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "FileDuration = %lld milli sec", TotalTime/10000));

    if (pCoreContext->pCore->pPipe && pCoreContext->pCore->cphandle)
    {
        NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "Streaming = %s", 
            pCoreContext->pCore->pPipe->IsStreaming(pCoreContext->pCore->cphandle) ? "YES" : "NO"));
    }
    else
    {
        NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "Streaming = NO"));     
    }
    
    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "TotalStreams = %d", pCoreContext->pCore->totalStreams));
    for (i = 0; i < pCoreContext->pCore->totalStreams; i++)
    {
        NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "Stream[%d]:", i));

        pStreamInfo = &pCoreContext->pCore->pStreamInfo[i];
    
        if (NVMM_ISSTREAMAUDIO(pStreamInfo->StreamType))
        {            
            NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "\tType = %s AUDIO", StreamTypeString(pStreamInfo->StreamType)));
            NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "\tTotalTime = %lld milli sec", pStreamInfo->TotalTime/10000));
            
            NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "\tSampleRate = %d Hz", pStreamInfo->NvMMStream_Props.AudioProps.SampleRate));
            NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "\tBitRate = %d bps", pStreamInfo->NvMMStream_Props.AudioProps.BitRate));            
            NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "\tNumChannels = %d", pStreamInfo->NvMMStream_Props.AudioProps.NChannels));                   
            NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "\tBitsPerSample = %d bits", pStreamInfo->NvMMStream_Props.AudioProps.BitsPerSample));                               

        }
        else if (NVMM_ISSTREAMVIDEO(pStreamInfo->StreamType))
        {
            NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "\tType = %s VIDEO", StreamTypeString(pStreamInfo->StreamType)));        
            NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "\tTotalTime = %lld milli sec", pStreamInfo->TotalTime/10000));
    
            NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "\tResolution = %dx%d", pStreamInfo->NvMMStream_Props.VideoProps.Width,
                pStreamInfo->NvMMStream_Props.VideoProps.Height));
            NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "\tFrameRate = %.2f fps", (float)pStreamInfo->NvMMStream_Props.VideoProps.Fps/FLOAT_MULTIPLE_FACTOR));            
            NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "\tVideoBitRate = %d", pStreamInfo->NvMMStream_Props.VideoProps.VideoBitRate));                        
        }
        else
        {
            NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "\tType = %s", StreamTypeString(pStreamInfo->StreamType)));  
        }       
    }        

    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "*****************************************************"));    
}
#endif

static NvError NvMMCalculateWaterMarks(NvMMSuperParserBlockContext* pContext, NvMMParserCoreHandle pCore)
{
    NvError Status = NvSuccess;    
    NvMMAttrib_FileCacheSize FileCacheSize;
    NvMMAttrib_FileCacheThreshold StreamingThreshold;
    NvU64 CCPSize = 0;
    NvU64 HighCutOff = 0, LowCutOff = 0, StartCutOff = 0;
    NvU64 CPHighMark = 0, CPLowMark = 0, CPStartMark = 0;
    pContext->SInfo.m_FirstTimeCache = NV_TRUE;
    pContext->SInfo.m_Buffering = NV_FALSE;
    pContext->SInfo.m_Streaming = NV_FALSE;
    pContext->SInfo.m_FirstTimeCache = NV_TRUE;
    pContext->SInfo.m_InitialBuffering = NV_TRUE;
    pContext->SInfo.m_BufferingPercent = 0;
    pContext->SInfo.m_bBufferBeforePlay = NV_TRUE;
    pContext->SInfo.m_StreamingLowMark = 0;
    pContext->SInfo.m_StreamingHighMark = 0;
    pContext->SInfo.m_StreamingStartMark = 0;
    pContext->SInfo.m_nFileSize = 0;
    pContext->SInfo.m_InterTransition = NV_FALSE;
    pContext->SInfo.m_BufferingThreshold = 100;
    pContext->SInfo.bCanSeekByTime = NV_FALSE;
    pContext->SInfo.nSeekBytes = 0;

    NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "++NvmmCalculateWaterMarks"));

    Status = pCore->pPipe->GetConfig(pCore->cphandle, CP_ConfigIndexCacheSize, &FileCacheSize.nFileCacheSize);    

    CCPSize = (NvU64)FileCacheSize.nFileCacheSize;
    CPHighMark = (CCPSize * STREAMING_CACHE_HIGH_THRESHOLD_PERCENT) / 100; // 80% of CCPSize
    CPLowMark = (CCPSize * STREAMING_CACHE_LOW_THRESHOLD_PERCENT) / 100; // 50% of CCPSize
    CPStartMark = (CCPSize * STREAMING_CACHE_LOW_THRESHOLD_PERCENT) / 100; // 50% of CCPSize

    NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "CCP size: %lld, CCP High: %lld, CCP Low: %lld, CCP Start Mark: %lld",
        CCPSize, 
        CPHighMark,
        CPLowMark,
        CPStartMark));

    if (pContext->SInfo.m_FileBitRate )
    {
        if (pContext->SInfo.m_AudioOnly)
        {
            // calculate the amount of data that can fit in NvBufferingDuration
            HighCutOff = (NvU64)(NvBufferingDuration_High * pContext->SInfo.m_FileBitRate) / 8;
            LowCutOff = (NvU64)(NvBufferingDuration_Low_AudioOnly * pContext->SInfo.m_FileBitRate) / 8;
            StartCutOff = LowCutOff;
            NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "Audio Only:: HighCutOff: %lld, LowCutOff: %lld, StartCutOff: %lld",
                HighCutOff,
                LowCutOff,
                StartCutOff));

        }
        else if (pContext->SInfo.m_GotVBitRate)
        {
            HighCutOff = (NvU64)(NvBufferingDuration_High * pContext->SInfo.m_FileBitRate) / 8;
            LowCutOff = (NvU64)(NvBufferingDuration_Low * pContext->SInfo.m_FileBitRate) / 8;
            StartCutOff = (NvU64)(NvBufferingDuration_Start * pContext->SInfo.m_FileBitRate) / 8;
            if (pContext->SInfo.m_rtDuration)
            {
                if((NvU32)(pContext->SInfo.m_rtDuration/10000000) <= NvBufferingDuration_Start)
                    StartCutOff = LowCutOff;
            }
            NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "Got Video BitRate:: HighCutOff: %lld, LowCutOff: %lld, StartCutOff: %lld",
                HighCutOff,
                LowCutOff,
                StartCutOff));
        }

        if (pContext->SInfo.m_rtDuration)
        {
            NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "m_rtDuration: %lld", pContext->SInfo.m_rtDuration));
            pContext->SInfo.m_nFileSize = (NvU64) (pContext->SInfo.m_rtDuration * pContext->SInfo.m_FileBitRate) / 80000000;
        }
    }
    else 
    {
        if (pContext->SInfo.VideoWidth != 0 && pContext->SInfo.VideoHeight != 0)
        {
            HighCutOff = (NvU64)(NvBufferingDuration_High * pContext->SInfo.VideoWidth * pContext->SInfo.VideoHeight);// Assuming average compreesion ratio 45:1 and 30 fps.
            LowCutOff = (NvU64)(NvBufferingDuration_Low * pContext->SInfo.VideoWidth * pContext->SInfo.VideoHeight); //so Bytes per second is (Width * Height * 1.5 * fps/ compression ratio)
            StartCutOff = (NvU64)(NvBufferingDuration_Start * pContext->SInfo.VideoWidth * pContext->SInfo.VideoHeight);
            NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "Got Video Width and Height:: HighCutOff: %lld, LowCutOff: %lld, StartCutOff: %lld",
                HighCutOff,
                LowCutOff,
                StartCutOff));
        }
        else
        {
            HighCutOff = (NvU64)(NvBufferingDuration_High * 128 * 1024) / 8;// Assuming 128kbps audio stream.
            LowCutOff = (NvU64)(NvBufferingDuration_Low * 128 * 1024) / 8;
            StartCutOff = (NvU64)(NvBufferingDuration_Start * 128 * 1024) / 8;
            NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "Got Video Width and Height:: HighCutOff: %lld, LowCutOff: %lld, StartCutOff: %lld",
                HighCutOff,
                LowCutOff,
                StartCutOff));
        }
        if (HighCutOff && LowCutOff && StartCutOff)
        {
            pContext->SInfo.m_StreamingHighMark = (CPHighMark < HighCutOff ? CPHighMark : HighCutOff);
            pContext->SInfo.m_StreamingLowMark = (CPLowMark < LowCutOff ? CPLowMark : LowCutOff);
            pContext->SInfo.m_StreamingStartMark = (CPStartMark < StartCutOff ? CPStartMark : StartCutOff);
            NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "Streaming Marks:: High: %lld, Low:%lld, Start: %lld",
                pContext->SInfo.m_StreamingHighMark,
                pContext->SInfo.m_StreamingLowMark,
                pContext->SInfo.m_StreamingStartMark));
        }
    }

    StreamingThreshold.nHighMark = CPHighMark;
    if(pContext->SInfo.m_StreamingLowMark > CPLowMark)
        StreamingThreshold.nLowMark = pContext->SInfo.m_StreamingLowMark;
    else
        StreamingThreshold.nLowMark = CPLowMark;

    NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "StreamingThreshold:: High: %lld, Low: %lld",
        StreamingThreshold.nHighMark,
        StreamingThreshold.nLowMark));

    if(pCore->eCoreType == NvMMParserCoreType_NemParser)
    {
        pContext->SInfo.bTimeStampBuffering = NV_TRUE;
        pContext->SInfo.bCanSeekByTime = NV_TRUE;
    }

    Status = pCore->pPipe->SetConfig(pCore->cphandle, CP_ConfigIndexThreshold, &StreamingThreshold);    

    NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "--NvxCalculateWaterMarks"));
    return Status;
}

static NvError NvmmSendBufferingEvent(NvMMSuperParserBlockContext* pContext, NvU32 nBufferPercent,  NvBool bChangeToPause)
{
    NvMMEventForBuffering EventForBuffering;

    if(pContext == NULL)
    {
        return NvError_InsufficientMemory;
    }

     EventForBuffering.structSize = sizeof(NvMMEventForBuffering);
     EventForBuffering.event = NvMMEvent_ForBuffering;   
     EventForBuffering.BufferingPercent =  nBufferPercent;    
     EventForBuffering.ChangeStateToPause = bChangeToPause;
     
     pContext->block.SendEvent(pContext->block.pSendEventContext, 
                      NvMMEvent_ForBuffering, 
                      sizeof(NvMMEventForBuffering), 
                      &EventForBuffering);    

     return NvSuccess;


}

static NvError NvMMStreamingCacheCalculation(NvMMSuperParserBlockContext* pContext)
{
    NvError Status = NvSuccess;    
    NvMMAttrib_FileCacheInfo FileCacheInfo;
    CP_ConfigTS LastTs;
    static NvBool PauseForBuffering = NV_TRUE;
    static NvBool bEOF = NV_FALSE;
    static NvU32 CacheHalt = 0;
    static NvU64 LastCacheByte = 0;
    NvU64 BytesAvailable = 0, nBytesTowardsEnd = 0, TotalBytesRead = 0;
    static NvU64 StreamingHighMarkOrig = 0, StreamingLowMarkOrig = 0, StreamingStartMarkOrig = 0;
    NvU32 i, NumOfEntries =0, NumOfBufferThreshold =0;
    NvMMStream *pStream = NULL;
    NvBool ProceedToPause = NV_TRUE;
    NvMMParserCoreHandle pCore = NULL;

    if((pContext == NULL) || (pContext->hBlock == NULL))
    {
        NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_ERROR, "SCC :: pContext is NULL"));
        Status = NvError_InsufficientMemory;
        return Status;
    }
    
    if ((pContext->block.State != NvMMState_Running) && (pContext->block.State != NvMMState_Paused) ) 
    {
        NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "SCC :: pNvComp->eState is not NvMMState_Running or NvMMState_Paused"));
        return NvSuccess;
    }

    pCore = pContext->pCurPlaybackCore;

    NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "++StreamingCacheCalculation"));

    if (pContext->SInfo.m_FirstTimeCache == NV_TRUE)
    {
        pContext->SInfo.m_FirstTimeCache = NV_FALSE;

        StreamingHighMarkOrig = pContext->SInfo.m_StreamingHighMark;
        StreamingLowMarkOrig = pContext->SInfo.m_StreamingLowMark;
        StreamingStartMarkOrig = pContext->SInfo.m_StreamingStartMark;

        NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "SCC :: OriHigh: %lld, OriLow: %lld, OriStart: %lld",
                                          StreamingHighMarkOrig,
                                          StreamingLowMarkOrig,
                                          StreamingStartMarkOrig));
    }

    pContext->hBlock->GetAttribute(pContext->hBlock,
                                               NvMMAttribute_FileCacheInfo, 
                                               sizeof(NvMMAttrib_FileCacheInfo),
                                               &FileCacheInfo);

    NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "%llu, %llu, %llu, %llu, %llu, %llu", FileCacheInfo.nDataBegin, FileCacheInfo.nDataFirst,
                                       FileCacheInfo.nDataCur, FileCacheInfo.nDataLast,
                                       FileCacheInfo.nDataEnd, pContext->SInfo.m_nFileSize));

    if(pContext->SInfo.bTimeStampBuffering)
    {
        pCore->pPipe->GetConfig(pCore->cphandle, CP_ConfigQueryTimeStamps, &LastTs);

        NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "Audio: %u, %llu, Video %u, %llu, SeekTs: %llu",
                                         LastTs.bAudio, LastTs.audioTs,
                                         LastTs.bVideo, LastTs.videoTs, pContext->SInfo.SeekTS));

        if(pContext->SInfo.m_InitialBuffering == NV_TRUE)
        {
            if(LastTs.bAudio && LastTs.bVideo)
            {
                pContext->SInfo.CurrentTS = (LastTs.audioTs > LastTs.videoTs)?LastTs.audioTs:LastTs.videoTs;
                NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG,
                    "InitiaBuffering::AudioVideo::Current Ts: %llu", pContext->SInfo.CurrentTS));
            }
            else if(LastTs.bAudio)
            {
                pContext->SInfo.CurrentTS = LastTs.audioTs;
                NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG,
                    "InitiaBuffering::Audio::Current Ts: %llu", pContext->SInfo.CurrentTS));
            }
            else if(LastTs.bVideo)
            {
                pContext->SInfo.CurrentTS = LastTs.videoTs;
                NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG,
                    "InitiaBuffering::Video::Current Ts: %llu", pContext->SInfo.CurrentTS));
            }

            if(pContext->SInfo.CurrentTS > (NvU64)((NvBufferingDuration_Start * 10000000) + pContext->SInfo.SeekTS))
            {
                pContext->SInfo.bStartPlayback = NV_TRUE;
                NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG,
                    "Current Ts(%llu) > SeekTS(%llu) + 8", pContext->SInfo.CurrentTS, pContext->SInfo.SeekTS));
            }
            pContext->SInfo.nWaitCounter++;
        }
        NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "bStartPlayback :  %u", pContext->SInfo.bStartPlayback));

    }

    if ((FileCacheInfo.nDataEnd == (NvU64)-1) && (pContext->SInfo.m_nFileSize))  
    {
        NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "SCC :: Did not get the file size still !!"));
        FileCacheInfo.nDataEnd = pContext->SInfo.m_nFileSize;
    }

    if(pContext->SInfo.bCanSeekByTime)
    {
        TotalBytesRead = (FileCacheInfo.nDataLast - FileCacheInfo.nDataFirst) + pContext->SInfo.nSeekBytes;

        if((!pContext->SInfo.m_nFileSize) && (FileCacheInfo.nDataEnd == (NvU64)-1))
        {
            nBytesTowardsEnd = (NvU64)-1;
        }
        else if((pContext->SInfo.m_nFileSize) && (TotalBytesRead < pContext->SInfo.m_nFileSize))
        {
            nBytesTowardsEnd = pContext->SInfo.m_nFileSize - TotalBytesRead;
        }
        else
            nBytesTowardsEnd = 0;
    }
    else
    {
        if (FileCacheInfo.nDataLast > FileCacheInfo.nDataEnd)
        {
            FileCacheInfo.nDataEnd = FileCacheInfo.nDataLast;
            nBytesTowardsEnd = 0;
        }
        else
            nBytesTowardsEnd = FileCacheInfo.nDataEnd - FileCacheInfo.nDataLast;
    }

    if (nBytesTowardsEnd <= pContext->SInfo.m_StreamingLowMark)
    {
        NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "SCC :: End of file condition: Start running"));
        pContext->SInfo.m_StreamingHighMark = nBytesTowardsEnd;
        pContext->SInfo.m_StreamingLowMark = nBytesTowardsEnd;
        bEOF = NV_TRUE;

        if (pContext->block.State == NvMMState_Paused) 
            PauseForBuffering = NV_FALSE;
    }
    else if (bEOF == NV_TRUE)
    {
        NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "SCC :: EOF:: Reassign back the original marks"));
        pContext->SInfo.m_StreamingHighMark = StreamingHighMarkOrig;
        pContext->SInfo.m_StreamingLowMark = StreamingLowMarkOrig;
        pContext->SInfo.m_StreamingStartMark = StreamingStartMarkOrig;
        bEOF = NV_FALSE;
    }      

    if (FileCacheInfo.nDataEnd < pContext->SInfo.m_StreamingHighMark)
    {
        NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "SCC  :: Data End < High Mark"));
        pContext->SInfo.m_StreamingHighMark = FileCacheInfo.nDataEnd;
    }

    if (FileCacheInfo.nDataEnd < pContext->SInfo.m_StreamingLowMark)
    {
        NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "SCC  :: Data End < Low Mark"));
        pContext->SInfo.m_StreamingLowMark = FileCacheInfo.nDataEnd;
    }
    if (FileCacheInfo.nDataEnd < pContext->SInfo.m_StreamingStartMark)
    {
        NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "SCC  :: Data End < Start Mark"));
        pContext->SInfo.m_StreamingStartMark = FileCacheInfo.nDataEnd;
    }

    BytesAvailable = (FileCacheInfo.nDataLast - FileCacheInfo.nDataCur) + pContext->pCurPlaybackCore->StoredDataSize;

    if(FileCacheInfo.nDataEnd && (FileCacheInfo.nDataEnd != (NvU64)-1))
        pContext->SInfo.m_BufferingPercent = (NvU32)((FileCacheInfo.nDataLast * 100)/FileCacheInfo.nDataEnd);
    else
        pContext->SInfo.m_BufferingPercent = 50; // ??Or it should be 100 for the case where file size is not known

    NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG,
                      "Data Last = %lld, Data End = %lld, Bytes Available = %llu, Buffering Percent: %d",
                      FileCacheInfo.nDataLast, FileCacheInfo.nDataEnd, BytesAvailable,
                      pContext->SInfo.m_BufferingPercent));

    if(pContext->SInfo.m_InitialBuffering == NV_TRUE)
    {
        PauseForBuffering = NV_TRUE;
    }

    if(pContext->SInfo.m_InitialBuffering)
    {
        if(((pContext->SInfo.bTimeStampBuffering == NV_TRUE) && (pContext->SInfo.bStartPlayback == NV_TRUE)) ||
            ((pContext->SInfo.bTimeStampBuffering == NV_FALSE) && (BytesAvailable >= pContext->SInfo.m_StreamingStartMark)) ||
           (bEOF == NV_TRUE) || (pContext->SInfo.nWaitCounter >= NV_INITIALBUFFERING_CUTOFF_COUNT))
        {
            NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "SCC :: cached content above start Mark ! Playing"));
            NvOsDebugPrintf("Buffered upto %llu secs\n", pContext->SInfo.CurrentTS);

            pContext->SInfo.m_InitialBuffering = NV_FALSE;
            PauseForBuffering = NV_FALSE;
            pContext->SInfo.m_bBufferBeforePlay = NV_FALSE;
            pContext->SInfo.bStartPlayback = NV_FALSE;
            pContext->SInfo.nWaitCounter = 0;
            NvOsSemaphoreSignal(pContext->block.hBlockEventSema);
        }
    }
     if ((BytesAvailable < pContext->SInfo.m_StreamingHighMark) && (PauseForBuffering == NV_TRUE))
     {
         if ( (FileCacheInfo.nDataLast == LastCacheByte) || (CacheHalt >= 30) )
         {
             CacheHalt++;
             // If the CCP file cache size does not update for more than 30 sec, start playback
             if (CacheHalt >= 30)
             {
                 pContext->SInfo.m_Buffering = NV_FALSE;
                 NvmmSendBufferingEvent(pContext,pContext->SInfo.m_BufferingPercent, NV_FALSE);                
                 NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "SCC :: CacheHalt running"));
                 return NvSuccess;  
             }

             if(CacheHalt == 1)
             {
                 NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "CacheHalt hit"));
             }
         }
         else 
         {  
             CacheHalt = 0;
             LastCacheByte = FileCacheInfo.nDataLast;
            

         }
         
         if ((pContext->block.State == NvMMState_Running) || (pContext->SInfo.m_bBufferBeforePlay))
         {
             NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "SCC :: Initial Pause for HIGHMARK BufferPercent : %d",pContext->SInfo.m_BufferingPercent));                 
             pContext->SInfo.m_bBufferBeforePlay = NV_FALSE;
             pContext->SInfo.m_Buffering = NV_TRUE;
             pContext->SInfo.m_InterTransition = NV_TRUE;
         }
          // Sending Buffering Percent frequently to client to display 
          NvmmSendBufferingEvent(pContext, pContext->SInfo.m_BufferingPercent, NV_TRUE);

     }
     else if ((PauseForBuffering == NV_FALSE) && (BytesAvailable < pContext->SInfo.m_StreamingLowMark)
         && (bEOF == NV_FALSE))
     {

        for (i = 0; i < pContext->pCurPlaybackCore->totalStreams; i++)
        {
            pStream = pContext->block.pStreams[i];
            if(pStream->bBuffersAllocated == NV_TRUE)
            {
                NumOfEntries = NvMMQueueGetNumEntries(pStream->BufQ);

                // Because some kinds of supported stream(ex: NB-AMR) have lower bit-rate, 
                // we use higher threshold to prevent unnecessary buffering pause. The 
                // default threshold is 50% of transfer buffer size.
                switch(pContext->pCurPlaybackCore->pStreamInfo[i].StreamType)
                {
                    case NvStreamType_NAMR:
                        NumOfBufferThreshold = pStream->NumBuffers*8/10;
                        break;
                    default:
                        NumOfBufferThreshold = pStream->NumBuffers/2;
                        break;
                }

                // If the number of entries with parser is less, it means decoder is holding the buffers
                // Donot pause if there are more than NumOfBufferThreshold of buffers with decoder
                if((NumOfEntries <= NumOfBufferThreshold))
                {
                    ProceedToPause = NV_FALSE;
                    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG,
                              "SCC:: StreamType: %u, QEntries: %u, Stream Index: %u, We have enough buffers at decoder. Not Pausing!!!!!!\n",
                              pContext->pCurPlaybackCore->pStreamInfo[i].StreamType, NumOfEntries, i));
                }
                else
                {
                    // Proceed to PAUSE even if we have one stream starving.
                    ProceedToPause = NV_TRUE;
                    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG,
                              "SCC:: StreamType: %u, QEntries: %u, Stream Index: %u, Pause threshold: %u, Allowed to Pause!!!!!!\n",
                              pContext->pCurPlaybackCore->pStreamInfo[i].StreamType, NumOfEntries, i, NumOfBufferThreshold));
                   break;
                }
            }
        }

        if (ProceedToPause == NV_TRUE)
        {
            if ((pContext->block.State == NvMMState_Running) || (pContext->SInfo.m_bBufferBeforePlay))
            {
                 NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "SCC :: Caching content below low mark --- Pausing  BufferPercent : %d",pContext->SInfo.m_BufferingPercent));

                 pContext->SInfo.m_bBufferBeforePlay = NV_FALSE;
                 pContext->SInfo.m_Buffering = NV_TRUE;
                 pContext->SInfo.m_InterTransition = NV_TRUE;

                 NvmmSendBufferingEvent(pContext, pContext->SInfo.m_BufferingPercent, NV_TRUE);
             }

            PauseForBuffering = NV_TRUE;
        }
     }
     else
     {           
         PauseForBuffering = NV_FALSE;       
         //if (pContext->block.State == NvMMState_Paused) // To send buffering events only in PAUSE state
         {
             NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "SCC :: Caching content above high mark --- Playing"));               
             CacheHalt = 0;
             pContext->SInfo.m_Buffering = NV_FALSE;
             NvmmSendBufferingEvent(pContext, pContext->SInfo.m_BufferingPercent, NV_FALSE);
         }  
     }        
    
    NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "--StreamingCacheCalculation"));         
        
    return Status;
}

static void NvMMBufferingThread(void *arg)
{
    NvError Status = NvSuccess;
    NvError Error = NvSuccess;
    NvMMSuperParserBlockContext* pContext = (NvMMSuperParserBlockContext*)arg;
    NvU32 Count = 0;
    NvBool bShutDown = NV_FALSE;

    if(pContext == NULL)
    {
        NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "NvMMBufferingThread ::  pContext is NULL - Terminating\n"));
        bShutDown = NV_TRUE;
        return;
     } 

    NvOsSemaphoreWait(pContext->SInfo.BufferingStartSema);

    while(bShutDown == NV_FALSE)
    {
        NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "NvMMBufferingThread: Count : %d\n", Count));

        Status = NvMMStreamingCacheCalculation(pContext);
        if(Status != NvSuccess)
        {
            NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_ERROR, "SCC Error! NvError  = 0x%08x  Breaking from NvMMBufferingThread\n", Status));
            bShutDown = NV_TRUE;
            break;
        }
        
        Count++;
        
        Error = NvOsSemaphoreWaitTimeout(pContext->SInfo.BufferingShutdownSema, 1000);

        if (Error != NvError_Timeout)
        {
            NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG,"NvMMBufferingThread : ShutDown\n"));
            bShutDown = NV_TRUE;
            break;
        }       

    }
}

static NvError NvMMSendEndOfStreamEvent(NvMMStream *pStream, NvU32 StreamIndex, NvMMSuperParserBlockContext* pContext)
{
    NvError Status = NvSuccess;
    NvMMEventStreamEndInfo StreamEndEvent;
    NV_LOGGER_PRINT((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "++NvMMSendEndOfStreamEvent : pStream - 0x%x", pStream));

    if(pStream)
    {
        if(pStream->TransferBufferFromBlock)
        {
            StreamEndEvent.structSize = sizeof(NvMMEventStreamEndInfo);
            StreamEndEvent.event = NvMMEvent_StreamEnd;
            Status = pStream->TransferBufferFromBlock(pStream->pOutgoingTBFContext,
                pStream->OutgoingStreamIndex,
                NvMMBufferType_StreamEvent,
                sizeof(NvMMEventStreamEndInfo),
                &StreamEndEvent);
        }
    }

    if (pContext->ParserCore.bEmpty == NV_FALSE)
    {
        /* Reset the end of stream on parser stream*/
        pContext->ParserCore.bEosEventSent[StreamIndex] = NV_TRUE;
        pContext->ParserCore.bEndOfStream[StreamIndex]  = NV_TRUE;
    }

    NV_LOGGER_PRINT((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "--NvMMSendEndOfStreamEvent : Status = 0x%x", Status));    
    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "Sending NvMMSendEndOfStreamEvent "));
    return Status;
}

static NvError NvMMGetParserCore(NvMMBlockHandle hBlock, NvMMParserCoreHandle *pCore, NvMMTrackInfo *pTrackInfo)
{
    NvError Status = NvSuccess;
    NvMMSuperParserBlockContext* pContext = (NvMMSuperParserBlockContext*)hBlock->pContext;
    NvU32 i = 0, j = 0, k = 0;
    NvMMSuperParserStartParsingMsg *pMessage;
    NvMMParserCoreContext *pCoreContext;
    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "++NvMMGetParserCore"));
    NvOsMutexLock(pContext->hCoreLock); 

find_core:
    if (pContext->ParserCore.bEmpty == NV_FALSE)
    {
        if ((!NvOsStrcmp(pTrackInfo->pPath,
                         pContext->ParserCore.pTrackInfo->pPath)))
        {
            *pCore = pContext->ParserCore.pCore;
            pContext->CurrentCoreIndex = i;
            pCoreContext = &pContext->ParserCore;
            NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "--NvMMGetParserCore: Core Type - %d, CoreIndex - %d",
                           pContext->ParserCore.eCoreType, i));
            // Reset the state
            pCoreContext->bPlayBackDone = NV_FALSE;
            for (j = 0; j <  pCoreContext->pCore->totalStreams; j++)
            {
                pCoreContext->bEndOfStream[j] = NV_FALSE;
                if (pCoreContext->pCore->bEnableUlpMode == NV_TRUE)
                {
                    if (pCoreContext->bPreCacheOffsets)
                    {
                        pCoreContext->ppOffsetListPerStream[j]->ConsumeIndex = 0;
                    }
                }
            }
            for (k =0; k < pContext->getCoreFailureCount; k++)
                NvOsSemaphoreSignal(pContext->block.hBlockEventSema);

            NvOsMutexUnlock(pContext->hCoreLock);
            return Status;
        }
    }

    pMessage = (NvMMSuperParserStartParsingMsg *)NvOsAlloc(MAX_MSG_LEN * sizeof(NvU32));
    if (pMessage == NULL)
    {
        Status = NvError_InsufficientMemory;
        NvOsMutexUnlock(pContext->hCoreLock);
        return Status;
    }
    pMessage->bNeedAck = NV_TRUE;
    pMessage->eMessageType = NvMMSuperParserMsgType_StartParsing;
    pMessage->bUpdateParseIndex = NV_FALSE;
    pMessage->StructSize = sizeof(NvMMSuperParserStartParsingMsg);
    pMessage->bDeleteCores = NV_FALSE;
    pMessage->bPrepareOffsets = NV_TRUE;
    pMessage->bUpdateParseIndex = NV_TRUE;
    pMessage->ParseIndex = pContext->PlayIndex;
    Status = NvOsSemaphoreCreate(&pMessage->hMessageAckSema, 0);
    if (Status != NvSuccess)
    {
        NvOsSemaphoreDestroy(pMessage->hMessageAckSema);
        NvOsFree(pMessage);
        NvOsMutexUnlock(pContext->hCoreLock);
        return Status;
    }
    pMessage->pParsingStatus = &pMessage->ParsingStatus;
    Status = NvMMQueueEnQ(pContext->hMsgQueue, pMessage, 0);
    if (Status != NvSuccess)
    {
        NvOsSemaphoreDestroy(pMessage->hMessageAckSema);
        NvOsFree(pMessage);
        NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_ERROR, "--NvMMGetParserCore: !!! Error EnQ failed"));
        NvOsMutexUnlock(pContext->hCoreLock);
        return Status;
    }
    NvOsSemaphoreSignal(pContext->hOffsetThreadSema);
    NvOsSemaphoreWait(pMessage->hMessageAckSema);
    if (pMessage->ParsingStatus != NvSuccess)
    {
        Status = pMessage->ParsingStatus;
        NvOsSemaphoreDestroy(pMessage->hMessageAckSema);
        NvOsFree(pMessage);
        NvOsMutexUnlock(pContext->hCoreLock);
        return Status;
    }
    else
    {
        NvOsSemaphoreDestroy(pMessage->hMessageAckSema);
        NvOsFree(pMessage);
        goto find_core;
    }
}

static NvError NvMMGetParserCoreAndMediaType(char *inFile, 
                                             NvMMParserCoreType *eCoreParserType, 
                                             NvMMSuperParserMediaType *eMediaType, NvBool guessFormat)
{
    NV_CUSTOM_PROTOCOL *pProtocol = NULL;
    NvS32 nProtoVer = 1;
    NvError status = NvSuccess;

    *eMediaType = NvMMSuperParserMediaType_Force32;
    *eCoreParserType = NvMMParserCoreTypeForce32;

    NvGetProtocolForFile(inFile, &pProtocol);
    pProtocol->GetVersion(&nProtoVer);
    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "++NvMMGetParserCoreAndMediaType"));
    if (nProtoVer >= 2)
    {
        NvMMParserCoreType cprParserType;
        NvParserCoreType eParserType;

        eParserType = pProtocol->ProbeParserType(inFile);
        NvMMUtilConvertNvMMParserCoreType(eParserType, &cprParserType);

        if (cprParserType == NvMMParserCoreType_UnKnown)
            cprParserType = NvMMParserCoreTypeForce32;

        *eCoreParserType = cprParserType;
    }

    if (*eCoreParserType == NvMMParserCoreTypeForce32)
    {
        if (guessFormat == NV_FALSE)
        {
            NvMMUtilFilenameToParserType((NvU8*)inFile, eCoreParserType, 
                                     eMediaType);
        }
        else
        {
            NvParserCoreType eParserType;
            NvMMUtilGuessFormatFromFile(inFile, pProtocol,
                                &eParserType, eMediaType);
            NvMMUtilConvertNvMMParserCoreType(eParserType, eCoreParserType);
        }
    }
    if (*eCoreParserType == NvMMParserCoreTypeForce32)
    {
        /* We were not able to determine a good guess for content type */
        status = NvError_UnSupportedStream;
    }
    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "--NvMMGetParserCoreAndMediaType : Core Type - 0x%x, MediaType - 0x%x", 
               *eCoreParserType, *eMediaType));
    return status;
}

/* 
* Function to create parser Cores
*/
static NvError 
NvMMCreateParserCore( NvMMSuperParserBlockContext *pContext,
                      NvMMParserCoreHandle *pCore,
                      NvMMParserCoreType eCoreParserType,
                      NvMMParserCoreCreationParameters *pParams)
{
    NvError Status = NvSuccess;

    switch (eCoreParserType)
    {
    case NvMMParserCoreType_Mp3Parser:
        Status = NvMMCreateMp3CoreParser(pCore, pParams);
        break;

    case NvMMParserCoreType_Mp4Parser:
    case NvMMParserCoreType_3gppParser:
        Status = NvMMCreateMp4CoreParser(pCore, pParams);
        break;

    case NvMMParserCoreType_AsfParser:
        {
            Status = NvOsLibraryLoad("libnvmm_asfparser.so", &pContext->hAsfHandle);

            if(Status == NvSuccess)
            {
                NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG,"\n Successfully loaded ASF library "));

                pContext->CreateCore = (NvError (*)(NvMMParserCoreHandle *,
                    NvMMParserCoreCreationParameters *))
                    NvOsLibraryGetSymbol(pContext->hAsfHandle,"NvMMCreateAsfCoreParser");

                pContext->DestroyCore = (void (*)(NvMMParserCoreHandle ))
                    NvOsLibraryGetSymbol(pContext->hAsfHandle,"NvMMDestroyAsfCoreParser");

                if (pContext->CreateCore)
                    pContext->CreateCore(pCore,pParams );
            }
            else
            {
                NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "failed to load ASF library"));
                Status = NvError_UnSupportedStream;
            }
        }
        break;

    case NvMMParserCoreType_AviParser:
        {
            Status = NvOsLibraryLoad("libnvmm_aviparser.so", &pContext->hAviHandle);

            if(Status == NvSuccess)
            {
                NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG,"\n Successfully loaded Avi library "));

                pContext->CreateCore = (NvError (*)(NvMMParserCoreHandle *,
                    NvMMParserCoreCreationParameters *))
                    NvOsLibraryGetSymbol(pContext->hAviHandle,"NvMMCreateAviCoreParser");

                pContext->DestroyCore = (void (*)(NvMMParserCoreHandle ))
                    NvOsLibraryGetSymbol(pContext->hAviHandle,"NvMMDestroyAviCoreParser");

                if (pContext->CreateCore)
                    pContext->CreateCore(pCore,pParams );
            }
            else
            {
                NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "failed to load AVI  library"));
                Status = NvError_UnSupportedStream;
            }
        }
        break;
    case NvMMParserCoreType_MpsParser:
        Status = NvMMCreateMpsCoreParser(pCore, pParams);
        break;
    case NvMMParserCoreType_AmrParser:
        Status = NvMMCreateAmrCoreParser(pCore, pParams);
        break;

    case NvMMParserCoreType_AacParser:
        Status = NvMMCreateAacCoreParser(pCore, pParams);
        break;
    case NvMMParserCoreType_OggParser:
        Status = NvMMCreateOggCoreParser(pCore, pParams);
        break;
    case NvMMParserCoreType_WavParser:
        Status = NvMMCreateWavCoreParser(pCore, pParams);
        break;

    case NvMMParserCoreType_NemParser:
        Status = NvMMCreateNemCoreParser(pCore, pParams);
        break;
    case NvMMParserCoreType_M2vParser:
        Status = NvMMCreateM2vCoreParser(pCore, pParams);
        break;

    case NvMMParserCoreType_FlvParser:
    case NvMMParserCoreType_MkvParser:
    case NvMMParserCoreTypeForce32:
    default:
        Status = NvError_UnSupportedStream;
        break;
    }
   
    return Status;
}

/* 
* Function to destroy parser Cores
*/
static void NvMMDestroyParserCore(NvMMSuperParserBlockContext *pContext,
                                  NvMMParserCoreHandle hCore,
                                  NvMMParserCoreType eCoreParserType)
{
    switch (eCoreParserType)
    {
    case NvMMParserCoreType_Mp3Parser:
        NvMMDestroyMp3CoreParser(hCore);
        break;

    case NvMMParserCoreType_Mp4Parser:
    case NvMMParserCoreType_3gppParser:
        NvMMDestroyMp4CoreParser(hCore);
        break;

    case NvMMParserCoreType_AsfParser:
        {
            if (pContext->hAsfHandle && pContext->DestroyCore)
            {
                NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG,"destroying the asf parser"));
                pContext->DestroyCore(hCore);
            }
        }
        break;

    case NvMMParserCoreType_MpsParser:
        NvMMDestroyMpsCoreParser(hCore);
        break;

    case NvMMParserCoreType_AmrParser:
        NvMMDestroyAmrCoreParser(hCore);
        break;

    case NvMMParserCoreType_AacParser:
        NvMMDestroyAacCoreParser(hCore);
        break;

    case NvMMParserCoreType_OggParser:
        NvMMDestroyOggCoreParser(hCore);
        break;

    case NvMMParserCoreType_WavParser:
        NvMMDestroyWavCoreParser(hCore);
        break;

    case NvMMParserCoreType_AviParser:
       {
            if (pContext->hAviHandle && pContext->DestroyCore)
            {
                NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG,"destroying the avi parser"));
                pContext->DestroyCore(hCore);
            }
        }
        break;

    case NvMMParserCoreType_NemParser:
        NvMMDestroyNemCoreParser(hCore);
        break;
    case NvMMParserCoreType_M2vParser:
        NvMMDestroyM2vCoreParser(hCore);
        break;
    case NvMMParserCoreType_MkvParser:
    case NvMMParserCoreType_FlvParser:
    default:
        break;
    }
}

/* 
* Function to set up streams for the block. We may increase streams when new core is created.
*/
static NvError NvMMSuperParserBlockCreateStreams(NvMMBlockHandle hBlock, NvMMParserCoreHandle hParserCore)
{
    NvMMSuperParserBlockContext *pContext = hBlock->pContext;
    NvU32 nStreamCount = pContext->block.StreamCount;
    NvU32 i;
    NvError Status = NvSuccess;

    if (nStreamCount < hParserCore->totalStreams)
    {
        for (i = nStreamCount; i < hParserCore->totalStreams; i++)
        {
            Status = NvMMBlockCreateStream(hBlock, i, NvMMStreamDir_Out, NVMMSTREAM_MAXBUFFERS);
            if (Status != NvSuccess)
            {
                return Status;
            }
        }
    }
    return Status;
}

/* 
* Function to Create Core parser using NvMM track info.
*/
static NvError NvMMCreateParserCoreFromTrackInfo(NvMMBlockHandle hBlock, NvMMTrackInfo *pTrackInfo, NvMMParserCoreContext **ppCoreContext)
{
    NvError Status = NvSuccess;
    NvBool GuessFormat = NV_FALSE, bFoundAMatch = NV_FALSE;
    NvBool bDeleteContext = NV_FALSE;
    NvU64 FileSize = 0;
    NvU32 i;
    NvBool bEmptySlot;
    NvU32 pTracks = 0;
    NvString URLPath = NULL;
    NvMMParserCoreContext *pCoreContext = NULL;
    NvMMParserCoreHandle pCore = NULL;
    NvMMSuperParserMediaType eMediaType;
    NvMMParserCoreCreationParameters pParams;
    NvMMParserCoreType eCoreParserType;
    NvMMParserCoreType eTempCoreParserType;
    NvMMSuperParserBlockContext *pContext = hBlock->pContext;

    URLPath=pTrackInfo->pPath;
    eCoreParserType = pTrackInfo->eParserCore;
    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "++NvMMCreateParserCoreFromTrackInfo: File = %s", pTrackInfo->pPath));

create_newcore:
    Status = NvMMGetParserCoreAndMediaType(URLPath, &eTempCoreParserType, &eMediaType, GuessFormat);
    if (Status != NvSuccess) 
    {
        if (GuessFormat == NV_FALSE)
        {
            GuessFormat = NV_TRUE;
            goto create_newcore;
        }
        NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "NvMMGetParserCoreAndMediaType:fails"));
        return Status;
    }

    eCoreParserType = eTempCoreParserType;
    pParams.pPrivate = NULL;
    pParams.bEnableUlpMode = pContext->block.bEnableUlpMode;
    pParams.UlpKpiMode = pContext->UlpKpiMode;
    pParams.bReduceVideoBuffers = pContext->bReduceVideoBuffers;
    pParams.hDrmHandle = pContext->hDrmHandle;
    NVMM_CHK_ERR (NvMMCreateParserCore(pContext,&pCore, eCoreParserType, &pParams));

    pCore->MinCacheSize     = MIN_CACHE_SIZE;
    pCore->SpareCacheSize   = CACHE_SPARE_SIZE;
    pCore->pActualCacheSize = 0;

    if (pContext->bCacheSizeSetByClient == NV_TRUE)
    {
        if ((pContext->MaxCacheSize == 0) || (pContext->MaxCacheSize < MIN_CACHE_SIZE))
        {
            pCore->MinCacheSize = 0;
            pCore->MaxCacheSize = 0;
            pCore->SpareCacheSize = 0;
            pCore->bEnableUlpMode = NV_FALSE; 
            goto open_core;
        }
    }
    else
    {
        pContext->MaxCacheSize = MAX_VIDEO_CACHE_SIZE;
    }

    /* if audio and tracklist check for total available memory */
    if ((eMediaType == NvMMSuperParserMediaType_Audio) && (pContext->bCacheSizeSetByClient == NV_FALSE))
        pContext->MaxCacheSize = MAX_AUDIO_CACHE_SIZE;

    pCore->MaxCacheSize = pContext->MaxCacheSize;

open_core:
    /* try to delete DRM contexts if no longer used */
    if (pContext->DrmContextInfo.bEmpty == NV_FALSE)
    {
        if (pContext->ParserCore.bEmpty == NV_FALSE)
        {
            if (pContext->ParserCore.pCore != NULL &&
                pContext->ParserCore.pCore->NvDrmContext != 0 &&
                pContext->ParserCore.pCore->NvDrmContext == pContext->DrmContextInfo.NvDrmContext)
            {
                bFoundAMatch = NV_TRUE;
                if (pContext->ParserCore.bPlayBackDone == NV_TRUE)
                {
                    bDeleteContext = NV_TRUE;
                }
            }
        }
    }

    if ((bFoundAMatch == NV_TRUE && bDeleteContext == NV_TRUE) ||
        (bFoundAMatch == NV_FALSE &&
         pContext->DrmContextInfo.bEmpty == NV_FALSE))
    {
        NV_LOGGER_PRINT((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "Destroy DRM context %x", pDrmContextInfo.NvDrmContext));
        ((NvError (*)(NvDrmContextHandle ))pContext->DrmContextInfo.NvDrmDestroyContext)((void *)pContext->DrmContextInfo.NvDrmContext);
        pContext->DrmContextInfo.bEmpty = NV_TRUE;
        pContext->DrmContextInfo.NvDrmContext = 0;
        pContext->DrmContextInfo.bPlayBackDone = NV_FALSE;
        pContext->DrmContextInfo.bMeteringDone = NV_FALSE;
    }

    Status = pCore->Open(pCore, URLPath);
    if (Status == NvError_RefURLAvailable)
    {
        NvMMAttrib_ParseUri EmbeddedURI;
        Status = pCore->GetAttribute(pCore, NvMMAttribute_FileName, sizeof(EmbeddedURI), &EmbeddedURI);     
        NV_LOGGER_PRINT((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "URL is  %s",URLPath));
        pCore->Close(pCore);
        if(Status == NvSuccess)
        {
            URLPath=EmbeddedURI.szURI;
        }
        else  
        {  
            NvOsFree(EmbeddedURI.szURI);
            goto cleanup;
        }
        
        NV_LOGGER_PRINT((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "updated URL is  %s",URLPath));
        goto create_newcore;
    }
    else if ((Status == NvError_UnSupportedStream) && (GuessFormat == NV_FALSE))
    {
        GuessFormat = NV_TRUE;
        goto create_newcore;
    }
    if(URLPath != pTrackInfo->pPath)
        NvOsFree(URLPath);

    pTrackInfo->domainName = pCore->domainName;
    /* Store DRM handle if created */
    if((pContext->hDrmHandle == NULL) && (pCore->hDrmHandle != NULL))
        pContext->hDrmHandle = pCore->hDrmHandle;

    if (Status != NvSuccess)
    {
        if (pCore->domainName == NvMMErrorDomain_DRM)
        {
            if(Status == NvError_DrmLicenseNotFound)
            {
                pContext->licenseUrlSize = pCore->licenseUrlSize;
                pContext->licenseChallengeSize= pCore->licenseChallengeSize;

                /* In case SP close is not called free the old memory*/
                /*In case of pre caching freeing below memory is required???*/
                if(pContext->licenseUrl)
                   NvOsFree(pContext->licenseUrl);
                if(pContext->licenseChallenge)
                   NvOsFree(pContext->licenseChallenge);

                pContext->licenseUrl = (NvU16 *) NvOsAlloc(pContext->licenseUrlSize * sizeof (NvU16));
                NVMM_CHK_MEM(pContext->licenseUrl);
                
                pContext->licenseChallenge = (NvU8  *) NvOsAlloc(pContext->licenseChallengeSize);
                NVMM_CHK_MEM(pContext->licenseChallenge);

                NvOsMemcpy(pContext->licenseUrl, pCore->licenseUrl,pContext->licenseUrlSize* sizeof (NvU16));
                NvOsMemcpy(pContext->licenseChallenge, pCore->licenseChallenge,pContext->licenseChallengeSize);
            }
            goto cleanup;       
        }
        else if (Status == NvError_ContentPipeInsufficientMemory)
        {
            // if AvailableMemory is 0 or lees than  required, disable Cached CP.
            pCore->bEnableUlpMode = NV_FALSE; 
            pCore->MinCacheSize = 0;
            pCore->MaxCacheSize = 0;
            pCore->SpareCacheSize = 0;
            pCore->pActualCacheSize = 0;
            Status = pCore->Open(pCore, pTrackInfo->pPath);
        } 
        
        if (Status != NvSuccess)
        {
            NV_LOGGER_PRINT((NVLOG_SUPER_PARSER, NVLOG_ERROR, "Core Parser Open Failed! NvError = 0x%08x", Status));
            goto cleanup;
        }
    }
   
    /* Store DRM context only if created */
    if (pCore->NvDrmContext != 0)
    {
        if (pContext->DrmContextInfo.bEmpty == NV_TRUE)
        {
            pContext->DrmContextInfo.NvDrmContext = pCore->NvDrmContext;
            pContext->DrmContextInfo.NvDrmUpdateMeteringInfo = pCore->NvDrmUpdateMeteringInfo;
            pContext->DrmContextInfo.NvDrmDestroyContext = pCore->NvDrmDestroyContext;
            pContext->DrmContextInfo.bMeteringDone = NV_FALSE;
            pContext->DrmContextInfo.bPlayBackDone = NV_FALSE;
            pContext->DrmContextInfo.bEmpty = NV_FALSE;
        }
    }

    if (pCore->pPipe && pCore->cphandle)
    {
        pCore->pPipe->GetSize(pCore->cphandle, (CPuint64 *)&FileSize);
    }

    pCore->CacheSize = pCore->pActualCacheSize;
    if (FileSize > pCore->pActualCacheSize)
        pCore->bPatiallyCached = NV_TRUE;
    else
        pCore->bPatiallyCached = NV_FALSE;

    Status = pCore->GetNumberOfStreams(pCore, &pCore->totalStreams);
    if ((pCore->totalStreams == 0) || (pCore->totalStreams > MAXSTREAMS))
    {
        Status = NvError_ParserCorruptedStream;
        NV_LOGGER_PRINT((NVLOG_SUPER_PARSER, NVLOG_ERROR, "Corrupt Stream Found!Invalid Track Count  = %d", pCore->totalStreams));
        goto cleanup;
    }

    pCore->pStreamInfo = (NvMMStreamInfo *)NvOsAlloc(pCore->totalStreams * sizeof(NvMMStreamInfo));
    NVMM_CHK_MEM(pCore->pStreamInfo);

    NvOsMemset(pCore->pStreamInfo, 0, pCore->totalStreams * sizeof(NvMMStreamInfo));
    NVMM_CHK_ERR (pCore->GetStreamInfo(pCore, &pCore->pStreamInfo));

    // If one of the steam is Video stream  then Disable the offsetlist Use and reset the UlpMode variables
    for (i = 0; i < pCore->totalStreams; i++)
    {
        if (((NVMM_ISSTREAMVIDEO(pCore->pStreamInfo[i].StreamType)) && pContext->block.bEnableUlpMode) || (pCore->pStreamInfo[i].StreamType == NvMMStreamType_WAV))
        {
            pContext->block.bEnableUlpMode = NV_FALSE;
            pCore->bEnableUlpMode = NV_FALSE; 
            break;
        }
    }

    for (i = 0; i < pCore->totalStreams; i++)
    {
        if (SuperParserIsUlpSupported(eCoreParserType, pCore->pStreamInfo[i].StreamType) == NV_FALSE)
        {
            pContext->block.bEnableUlpMode = NV_FALSE;
            pCore->bEnableUlpMode = NV_FALSE; 
        }
    }

    if (pCore->bUsingCachedCP)
    {
        if (pCore->cphandle)
            Status = pCore->pPipe->RegisterClientCallback(pCore->cphandle, pContext, NvMMSuperParserCPNotify);
        if (Status != NvSuccess)
        {
            Status = NvError_InsufficientMemory;
            goto cleanup;
        }
    }
    
    if(pCore && pCore->pPipe && pCore->cphandle)
        pContext->SInfo.IsStreaming = pCore->pPipe->IsStreaming(pCore->cphandle);     
    
    if((pContext->SInfo.IsStreaming) && (pContext->bEnableBufferingForStreaming))            
    {  
        for (i = 0; i < pCore->totalStreams; i++)
        {        
            if (NVMM_ISSTREAMAUDIO(pCore->pStreamInfo[i].StreamType))
            {
                pContext->SInfo.AudioBitRate = pCore->pStreamInfo[i].NvMMStream_Props.AudioProps.BitRate;
                pContext->SInfo.m_AudioOnly = NV_TRUE;
                pContext->SInfo.m_rtDuration  = pCore->pStreamInfo[i].TotalTime;
            }
           else if ((NVMM_ISSTREAMVIDEO(pCore->pStreamInfo[i].StreamType) )&&
                     (pCore->pStreamInfo[i].StreamType != NvMMStreamType_UnsupportedVideo))
            {
                pContext->SInfo.VideoBitRate = pCore->pStreamInfo[i].NvMMStream_Props.VideoProps.VideoBitRate;
                pContext->SInfo.m_GotVBitRate = NV_TRUE;
                pContext->SInfo.m_rtDuration  = pCore->pStreamInfo[i].TotalTime;
                pContext->SInfo.VideoWidth = pCore->pStreamInfo[i].NvMMStream_Props.VideoProps.Width;
                pContext->SInfo.VideoHeight = pCore->pStreamInfo[i].NvMMStream_Props.VideoProps.Height;
                break;
            }
        }

        pContext->SInfo.m_FileBitRate = pContext->SInfo.AudioBitRate + pContext->SInfo.VideoBitRate;

        pContext->SInfo.m_FileBitRate = pContext->SInfo.AudioBitRate + pContext->SInfo.VideoBitRate;

        if(pContext->SInfo.m_GotVBitRate == NV_TRUE) 
            pContext->SInfo.m_AudioOnly = NV_FALSE;


        Status = NvOsSemaphoreCreate(&pContext->SInfo.BufferingStartSema, 0);
        if (Status != NvSuccess)
        {
            goto cleanup;
        }

        Status = NvOsSemaphoreCreate(&pContext->SInfo.BufferingShutdownSema, 0);
        if (Status != NvSuccess)
        {
            goto cleanup;
        }

        Status = NvOsThreadCreate(NvMMBufferingThread, pContext, &pContext->SInfo.bufferingMonitorThread);
        if (Status != NvSuccess)
        {
            goto cleanup;
        }

        NvMMCalculateWaterMarks(pContext, pCore);    
        NvOsSemaphoreSignal(pContext->SInfo.BufferingStartSema);

    }   

    bEmptySlot = NV_FALSE;
    if (pContext->ParserCore.bEmpty == NV_TRUE)
    {
        bEmptySlot = NV_TRUE;
    }

    if (bEmptySlot == NV_TRUE)
    {
        pCoreContext = &pContext->ParserCore;
        pCoreContext->eCoreType = eCoreParserType;
        pCoreContext->pCore = pCore;
        pCoreContext->FileSize = FileSize;
        pCoreContext->bPlayBackDone = NV_FALSE;
        pCoreContext->pTrackInfo = NvOsAlloc(sizeof(NvMMTrackInfo));
        NVMM_CHK_MEM (pCoreContext->pTrackInfo);
        pCoreContext->pTrackInfo->pPath = NvOsAlloc(pTrackInfo->uSize + 1);
        NVMM_CHK_MEM (pCoreContext->pTrackInfo->pPath);
        pCoreContext->pTrackInfo->eTrackFlag = pTrackInfo->eTrackFlag;
        pCoreContext->pTrackInfo->pClientPrivate = pTrackInfo->pClientPrivate;
        pCoreContext->pTrackInfo->uSize = pTrackInfo->uSize;
        pCoreContext->pTrackInfo->eParserCore = pTrackInfo->eParserCore;
        pCoreContext->pTrackInfo->trackListIndex = pTrackInfo->trackListIndex;
        NvOsMemset(pCoreContext->pTrackInfo->pPath, 0, pTrackInfo->uSize + 1);
        NvOsStrncpy(pCoreContext->pTrackInfo->pPath, pTrackInfo->pPath, pTrackInfo->uSize);

        if (pCoreContext->pCore->bEnableUlpMode == NV_TRUE)
        {
            if ((pCoreContext->pCore->totalStreams > 1) || 
                (NVMM_ISSTREAMVIDEO(pCoreContext->pCore->pStreamInfo[0].StreamType)) || 
                (pCoreContext->pCore->bPatiallyCached == NV_TRUE) ||
                (pTracks <= 1))
                pCoreContext->bPreCacheOffsets = NV_FALSE;
            else
                pCoreContext->bPreCacheOffsets = NV_TRUE;

            pCoreContext->bPreCacheOffsets = NV_FALSE;
            if (pCoreContext->bPreCacheOffsets == NV_TRUE)
            {
                pCoreContext->ppOffsetListPerStream = NvOsAlloc(pCoreContext->pCore->totalStreams * sizeof(OffsetListPerStream*));
                NVMM_CHK_MEM (pCoreContext->ppOffsetListPerStream);
                NvOsMemset(pCoreContext->ppOffsetListPerStream , 0, pCoreContext->pCore->totalStreams * sizeof(OffsetListPerStream*));
                for (i = 0; i < pCoreContext->pCore->totalStreams; i++)
                {
                    pCoreContext->ppOffsetListPerStream[i] = NvOsAlloc(sizeof(OffsetListPerStream));
                    NVMM_CHK_MEM (pCoreContext->ppOffsetListPerStream[i]);
                    NvOsMemset((OffsetListPerStream*)pCoreContext->ppOffsetListPerStream[i], 0, sizeof(OffsetListPerStream));
                    pCoreContext->ppOffsetListPerStream[i]->ppOffsetList = NvOsAlloc(sizeof(OffsetList*) * MAX_SP_OFFSET_LISTS);
                    NVMM_CHK_MEM (pCoreContext->ppOffsetListPerStream[i]->ppOffsetList);
                    NvOsMemset(pCoreContext->ppOffsetListPerStream[i]->ppOffsetList, 0, sizeof(OffsetList*) * MAX_SP_OFFSET_LISTS);
                }
            }
        }
        pCoreContext->bEmpty = NV_FALSE;
        for (i = 0; i < pCoreContext->pCore->totalStreams; i++)
        {
            pCoreContext->bEndOfStream[i] = NV_FALSE;
            pCoreContext->bEosEventSent[i] = NV_FALSE;
        }
    }
    else
    {
        Status = NvError_ParserFailure;
        goto cleanup;
    }

    *ppCoreContext = pCoreContext;

#if NV_LOGGER_ENABLED
    SuperParserPrintTrackInfo(pCoreContext); // dump the debug info
#endif

cleanup:
    if (Status != NvSuccess)
    {
        if (pCore)
        {
            if(pCore->NvDrmDestroyContext && pCore->NvDrmContext)
            {
                NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "Destroy DRM context %x", pCore->NvDrmContext));
                ((NvError (*)(NvDrmContextHandle ))pCore->NvDrmDestroyContext)((void *)pCore->NvDrmContext);
            }
            NvOsFree(pCore->pStreamInfo);       
            pCore->pStreamInfo = NULL;
            pCore->Close(pCore);
            NvMMDestroyParserCore(pContext,pCore, pCore->eCoreType);
            pCore = NULL;    
        }

        if (pContext)
        {
            if (pContext->licenseUrl)
            {
                NvOsFree (pContext->licenseUrl);
                pContext->licenseUrl = NULL;
            }
        }

        if (pCoreContext)
        {
            if (pCoreContext->pTrackInfo)
            {
                if (pCoreContext->pTrackInfo->pPath)
                {
                    NvOsFree(pCoreContext->pTrackInfo->pPath);
                    pCoreContext->pTrackInfo->pPath = NULL;
                }
                NvOsFree(pCoreContext->pTrackInfo);
                pCoreContext->pTrackInfo = NULL;
            }
            if (pCoreContext->ppOffsetListPerStream)
            {
                for (i = 0; i < pCoreContext->pCore->totalStreams; i++)
                {
                    if (pCoreContext->ppOffsetListPerStream[i])
                    {
                        NvOsFree(pCoreContext->ppOffsetListPerStream[i]->ppOffsetList);
                        pCoreContext->ppOffsetListPerStream[i]->ppOffsetList = NULL;
                    }
                    NvOsFree(pCoreContext->ppOffsetListPerStream[i]);
                    pCoreContext->ppOffsetListPerStream[i] = NULL;
                }
                NvOsFree(pCoreContext->ppOffsetListPerStream);
                pCoreContext->ppOffsetListPerStream = NULL;
            }
        }
    }

    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "--NvMMCreateParserCoreFromTrackInfo: Status - 0x%x pCoreContext = 0x%x", 
        Status, pCoreContext));
    
    return Status;
}

NvError SuperParserCreateCore(NvMMBlockHandle hBlock, NvMMTrackInfo *pTrackInfo, NvMMParserCoreHandle *pCore)
{
    NvError Status = NvSuccess;
    NvMMSuperParserCreateParserCoreMsg CreateCoreMessage;
    NvMMSuperParserBlockContext* pContext = (NvMMSuperParserBlockContext*)hBlock->pContext;

    NvOsMemset(&CreateCoreMessage, 0 , sizeof(NvMMSuperParserCreateParserCoreMsg));
    CreateCoreMessage.eMessageType = NvMMSuperParserMsgType_CreateParserCore;
    CreateCoreMessage.StructSize = sizeof(NvMMSuperParserCreateParserCoreMsg);
    CreateCoreMessage.bNeedAck = NV_TRUE;
    CreateCoreMessage.pStatus = &CreateCoreMessage.Status;
    CreateCoreMessage.pTrackInfo = *pTrackInfo;
    Status = NvOsSemaphoreCreate(&CreateCoreMessage.hMessageAckSema, 0);
    if (Status != NvSuccess)
        return Status;
    Status = NvMMQueueEnQ(pContext->hMsgQueue, &CreateCoreMessage, 0);
    if (Status != NvSuccess)
    {
        NvOsSemaphoreDestroy(CreateCoreMessage.hMessageAckSema);
        return Status;
    }
    NvOsSemaphoreSignal(pContext->hOffsetThreadSema);
    NvOsSemaphoreWait(CreateCoreMessage.hMessageAckSema);
    NvOsSemaphoreDestroy(CreateCoreMessage.hMessageAckSema);
    if (CreateCoreMessage.Status != NvSuccess)
        Status = *CreateCoreMessage.pStatus;

    return Status;
}


NvError SuperParserCreateCorePvt(NvMMBlockHandle hBlock, NvMMTrackInfo *pTrackInfo,  NvMMParserCoreHandle *pCore)
{
    NvError Status = NvSuccess;
    NvMMParserCoreContext *pCoreContext;

    Status = NvMMCreateParserCoreFromTrackInfo(hBlock, pTrackInfo, &pCoreContext);
    if (Status == NvSuccess)
        *pCore = pCoreContext->pCore;
    else
        *pCore = NULL;

    return Status;
}

NvError SuperParserDeleteCore(NvMMBlockHandle hBlock, NvMMParserCoreHandle pCore)
{
    NvError Status = NvSuccess;
    NvMMSuperParserDeleteParserCoreMsg DeleteCoreMessage;
    NvMMSuperParserBlockContext* pContext = (NvMMSuperParserBlockContext*)hBlock->pContext;

    NvOsMemset(&DeleteCoreMessage, 0 , sizeof(NvMMSuperParserDeleteParserCoreMsg));
    DeleteCoreMessage.bNeedAck = NV_TRUE;
    DeleteCoreMessage.eMessageType = NvMMSuperParserMsgType_DeleteParserCore;
    DeleteCoreMessage.StructSize = sizeof(NvMMSuperParserDeleteParserCoreMsg);
    Status = NvOsSemaphoreCreate(&DeleteCoreMessage.hMessageAckSema, 0);
    if (Status != NvSuccess)
        return Status;
    DeleteCoreMessage.pCore = pCore;
    Status = NvMMQueueEnQ(pContext->hMsgQueue, &DeleteCoreMessage, 0);
    if (Status != NvSuccess)
    {
        NvOsSemaphoreDestroy(DeleteCoreMessage.hMessageAckSema);
        return Status;
    }
    NvOsSemaphoreSignal(pContext->hOffsetThreadSema);
    NvOsSemaphoreWait(DeleteCoreMessage.hMessageAckSema);
    NvOsSemaphoreDestroy(DeleteCoreMessage.hMessageAckSema);

    return Status;
}
NvError SuperParserDeleteCorePvt(NvMMBlockHandle hBlock, NvMMParserCoreHandle pCore)
{
    NvError Status = NvSuccess;
    NvMMSuperParserBlockContext* pContext = (NvMMSuperParserBlockContext*)hBlock->pContext;
    NvMMParserCoreContext *pCoreContext;

    pCoreContext = &pContext->ParserCore;

    if ((pContext->block.State == NvMMState_Stopped) && (!pContext->block.bBlockProcessingData))
    {
        NvMMDeleteCore(pContext, pCoreContext); 
    }

    return Status;
}

NvError SuperParserCreateOffsets(NvMMBlockHandle hBlock, NvMMParserCoreHandle pCore)
{
    NvError Status = NvSuccess;
    NvMMSuperParserCreateOffsetsMsg CreateOffsetsMessage;
    NvMMSuperParserBlockContext* pContext = (NvMMSuperParserBlockContext*)hBlock->pContext;

    NvOsMemset(&CreateOffsetsMessage, 0 , sizeof(NvMMSuperParserCreateOffsetsMsg));
    CreateOffsetsMessage.eMessageType = NvMMSuperParserMsgType_CreateOffsets;
    CreateOffsetsMessage.StructSize = sizeof(NvMMSuperParserCreateOffsetsMsg);
    CreateOffsetsMessage.pCore = pCore;
    Status = NvMMQueueEnQ(pContext->hMsgQueue, &CreateOffsetsMessage, 0);
    if (Status != NvSuccess)
        return Status;
    NvOsSemaphoreSignal(pContext->hOffsetThreadSema);

    return Status;
}

NvBool SuperParserAnyControlMessagesInPrecachingThread(NvMMBlockHandle hBlock, NvBool *bCurrentCoreNeeded)
{
    NvError Status = NvSuccess;
    NvMMSuperParserBlockContext* pContext = (NvMMSuperParserBlockContext*)hBlock->pContext;
    NvMMSuperParserMsg *pMessage;
    NvMMSuperParserMsgType eMessageType;
    NvBool bReturnValue = NV_FALSE;

    *bCurrentCoreNeeded = NV_TRUE;
    pMessage = NvOsAlloc(MAX_MSG_LEN * sizeof(NvU32));
    if (pMessage == NULL)
    {
        return bReturnValue;
    }
    Status = NvMMQueuePeek(pContext->hMsgQueue, pMessage);
    if (Status != NvSuccess)
    {
        NvOsFree(pMessage);
        return bReturnValue;
    }

    eMessageType = ((NvMMSuperParserMsg*)pMessage)->eMessageType;
    switch (eMessageType)
    {
    case NvMMSuperParserMsgType_StartParsing:
        if(((NvMMSuperParserStartParsingMsg*)pMessage)->bDeleteCores)    
        {
            *bCurrentCoreNeeded = NV_FALSE;            
            bReturnValue = NV_TRUE;
        }
        else
            bReturnValue = NV_FALSE;
        NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "PCT: NvMMSuperParserMsgType_StartParsing\n"));
        break;

    case NvMMSuperParserMsgType_DestroyThread:
        *bCurrentCoreNeeded = NV_FALSE;
        bReturnValue = NV_TRUE;
        NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "PCT: NvMMSuperParserMsgType_DestroyThread\n"));
        break;

    case NvMMSuperParserMsgType_UlpMode:
        NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "PCT: NvMMSuperParserMsgType_UlpMode\n"));
        bReturnValue = NV_TRUE;
        break;

    case NvMMSuperParserMsgType_CreateParserCore:
        NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "PCT: NvMMSuperParserMsgType_CreateParserCore\n"));
        bReturnValue = NV_TRUE;
        break;

    case NvMMSuperParserMsgType_DeleteParserCore:
        NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "PCT: NvMMSuperParserMsgType_DeleteParserCore\n"));
        bReturnValue = NV_TRUE;
        break;

    case NvMMSuperParserMsgType_CreateOffsets:
        NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "PCT: NvMMSuperParserMsgType_CreateOffsets\n"));
        bReturnValue = NV_TRUE;
        break;

    case NvMMSuperParserMsgType_DeleteOffsets:
        NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "PCT: NvMMSuperParserMsgType_DeleteOffsets\n"));
        bReturnValue = NV_TRUE;
        break;

    case NvMMSuperParserMsgType_AdjustOffsetsForSeek:
        NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "PCT: NvMMSuperParserMsgType_AdjustOffsetsForSeek\n"));
        bReturnValue = NV_TRUE;
        break;

    default:
        bReturnValue = NV_FALSE;
        break;
    }

    NvOsFree(pMessage);
    return bReturnValue;
}

NvError SuperParserCreateOffsetsPvt(NvMMBlockHandle hBlock, NvMMParserCoreHandle pCore)
{
    NvError Status = NvSuccess;
    NvU32 nOffsets = 0;
    NvU32 StreamIndex;
    NvU32 ProduceIndex=0;
    NvMMBuffer *pBuffer; 
    NvMMSuperParserBlockContext* pContext = (NvMMSuperParserBlockContext*)hBlock->pContext;
    NvBool bEndOfAllStreams;
    NvU32 StreamDataSize;
    NvU32 MaxOffsetsPerOffsetList;
    NvU32 NvMMBufferSize;
    NvMMParserCoreContext *pCoreContext;
    NvBool bCurrentCoreNeeded;

    pContext->bParsingPaused = NV_FALSE;
    pContext->pPausedCore = NULL;

    pCoreContext = &pContext->ParserCore;

    MaxOffsetsPerOffsetList = 0;
    pCoreContext->pCore->GetMaxOffsets(pCoreContext->pCore, &MaxOffsetsPerOffsetList);
    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "PCT: Start Preparing Offsets Core Context:0x%x Core:0x%x\n",
        pCoreContext, pCoreContext->pCore));
    if(pCoreContext->bPrepareOffsets == NV_FALSE)
    {
        if(pCoreContext->eCoreType == NvMMParserCoreType_Mp4Parser)
            return Status;
        else if(pCoreContext->NumberofOffsetLists >= MAX_PAUSE_PRECACHE_OFFSETS)
            return Status;
    }
    if ((pCoreContext->pCore->bEnableUlpMode == NV_TRUE) && (pCoreContext->bPreCacheOffsets == NV_TRUE))
    {
        while (1)
        {
            for (StreamIndex = 0; StreamIndex < pCoreContext->pCore->totalStreams; StreamIndex++)
            {
                ProduceIndex = pCoreContext->ppOffsetListPerStream[StreamIndex]->ProduceIndex;
                pCoreContext->ppOffsetListPerStream[StreamIndex]->ppOffsetList[ProduceIndex] = NvOsAlloc(sizeof(OffsetList));
                if (pCoreContext->ppOffsetListPerStream[StreamIndex]->ppOffsetList[ProduceIndex] == NULL)
                {
                    pCoreContext->ppOffsetListPerStream[StreamIndex]->bEndOfStream = NV_TRUE;
                    if (ProduceIndex == 0)
                    {
                        pCoreContext->bEndOfStream[StreamIndex] = NV_TRUE;
                    }
                    continue;
                }

                pBuffer = NvOsAlloc(sizeof(NvMMBuffer));
                if (pBuffer == NULL)
                {
                    NvOsFree(pCoreContext->ppOffsetListPerStream[StreamIndex]->ppOffsetList[ProduceIndex]);
                    pCoreContext->ppOffsetListPerStream[StreamIndex]->bEndOfStream = NV_TRUE;
                    if (ProduceIndex == 0)
                    {
                        pCoreContext->bEndOfStream[StreamIndex] = NV_TRUE;
                    }
                    continue;
                }
                NvMMBufferSize = sizeof(NvMMOffsetList) + (sizeof(NvMMOffset) * MaxOffsetsPerOffsetList);
                Status = NvMMUtilAllocateBuffer(pContext->hRmDevice, NvMMBufferSize, 4, NvMMMemoryType_SYSTEM, NV_FALSE, &pBuffer);
                if (Status != NvSuccess)
                {
                    NvOsFree(pBuffer);
                    NvOsFree(pCoreContext->ppOffsetListPerStream[StreamIndex]->ppOffsetList[ProduceIndex]);
                    pCoreContext->ppOffsetListPerStream[StreamIndex]->bEndOfStream = NV_TRUE;
                    if (ProduceIndex == 0)
                    {
                        pCoreContext->bEndOfStream[StreamIndex] = NV_TRUE;
                    }
                    continue;
                }
                pCoreContext->ppOffsetListPerStream[StreamIndex]->ppOffsetList[ProduceIndex]->pOffsetList = pBuffer;
                StreamDataSize = 0;
                Status = NvMMSuperParserBlockPrepareNextWorkUnitOffsets(hBlock, pCoreContext->pCore, StreamIndex, pBuffer, &StreamDataSize);
                NvmmGetNumOfValidOffsets(pBuffer, &nOffsets);
                if ((Status != NvSuccess) && !nOffsets)
                {
                    if (Status == NvError_ContentPipeNotReady)
                    {
                        NvMMUtilDeallocateBuffer(pBuffer);
                        NvOsFree(pBuffer);
                        NvOsFree(pCoreContext->ppOffsetListPerStream[StreamIndex]->ppOffsetList[ProduceIndex]);

                        if (pCoreContext->pCore->bUsingCachedCP)
                        {
                            NvOsSemaphoreWait(pContext->hCpWaitSema);
                            NvOsSemaphoreSignal(pContext->block.hBlockEventSema);
                        }
                        else
                        {
                            pCoreContext->ppOffsetListPerStream[StreamIndex]->bEndOfStream = NV_TRUE;
                            if (ProduceIndex == 0)
                            {
                                pCoreContext->bEndOfStream[StreamIndex] = NV_TRUE;
                            }
                        }
                        continue;
                    }
                    else
                    {
                        pCoreContext->ppOffsetListPerStream[StreamIndex]->bEndOfStream = NV_TRUE;
                        NvMMUtilDeallocateBuffer(pBuffer);
                        NvOsFree(pBuffer);
                        NvOsFree(pCoreContext->ppOffsetListPerStream[StreamIndex]->ppOffsetList[ProduceIndex]);
                        if (ProduceIndex == 0)
                        {
                            pCoreContext->bEndOfStream[StreamIndex] = NV_TRUE;
                        }
                        break;
                    }
                }
                else
                {
                    pCoreContext->ppOffsetListPerStream[StreamIndex]->ppOffsetList[ProduceIndex]->bDataReady = NV_TRUE;
                    pCoreContext->ppOffsetListPerStream[StreamIndex]->ProduceIndex ++;
                    pCoreContext->ppOffsetListPerStream[StreamIndex]->NumberofOffsetLists ++;
                    if (pCoreContext->pCore->totalStreams == 1)
                        pCoreContext->NumberofOffsetLists = pCoreContext->ppOffsetListPerStream[StreamIndex]->NumberofOffsetLists;
                    if (pCoreContext->ppOffsetListPerStream[StreamIndex]->NumberofOffsetLists >= MAX_SP_OFFSET_LISTS)
                    {
                        NV_ASSERT(!"More than MAX_SP_OFFSET_LISTS Offset lists.\n");
                    }
                    NvOsSemaphoreSignal(pContext->block.hBlockEventSema);
                }
            }

            bEndOfAllStreams = NV_TRUE;
            for (StreamIndex = 0; StreamIndex < pCoreContext->pCore->totalStreams; StreamIndex++)
            {
                if (pCoreContext->ppOffsetListPerStream)
                {
                    if (pCoreContext->ppOffsetListPerStream[StreamIndex]->bEndOfStream == NV_FALSE)
                    {
                        bEndOfAllStreams = NV_FALSE;
                    }
                }
            }

            if (bEndOfAllStreams == NV_TRUE)
            {
                break;
            }

            //Peek next messgage if any, abort preparing offsets if any control messages found and service them.
            if (SuperParserAnyControlMessagesInPrecachingThread(hBlock , &bCurrentCoreNeeded) == NV_TRUE)
            {
                if (bCurrentCoreNeeded == NV_TRUE)
                {
                    pContext->bParsingPaused = NV_TRUE;
                    pContext->pPausedCore = pCoreContext->pCore;
                }
                else
                {
                    pContext->bParsingPaused = NV_FALSE;
                    pContext->pPausedCore = NULL;
                }

                Status = NvError_ParserFailure;
                NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "PCT: Going Out TO serve Control message:ProduceIndex:%d\n",ProduceIndex));
                break;
            }
        }
    }
    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "PCT: Core Context:0x%x Core:0x%x  No Offsets:%d\n",
        pCoreContext, pCoreContext->pCore,pCoreContext->NumberofOffsetLists));

    return Status;
}

NvError SuperParserDeleteOffsets(NvMMBlockHandle hBlock, NvMMParserCoreHandle pCore)
{
    NvError Status = NvSuccess;
    NvMMSuperParserDeleteOffsetsMsg DeleteOffsetsMessage;
    NvMMSuperParserBlockContext* pContext = (NvMMSuperParserBlockContext*)hBlock->pContext;

    NvOsMemset(&DeleteOffsetsMessage, 0, sizeof(NvMMSuperParserDeleteOffsetsMsg));
    DeleteOffsetsMessage.eMessageType = NvMMSuperParserMsgType_DeleteOffsets;
    DeleteOffsetsMessage.StructSize = sizeof(NvMMSuperParserDeleteOffsetsMsg);
    DeleteOffsetsMessage.pCore = pCore;
    Status = NvMMQueueEnQ(pContext->hMsgQueue, &DeleteOffsetsMessage, 0);
    if (Status != NvSuccess)
        return Status;
    NvOsSemaphoreSignal(pContext->hOffsetThreadSema);

    return Status;
}

NvError SuperParserDeleteOffsetsPvt(NvMMBlockHandle hBlock, NvMMParserCoreHandle pCore)
{
    NvError Status = NvSuccess;
    NvU32 StreamIndex,j;
    NvMMBuffer *pBuffer;
    NvU32 ProduceIndex;
    NvMMParserCoreContext *pCoreContext;
    NvMMSuperParserBlockContext *pContext = (NvMMSuperParserBlockContext*)hBlock->pContext;

    pCoreContext = &pContext->ParserCore;

    if ((pCoreContext->pCore->bEnableUlpMode) && (pCoreContext->bPreCacheOffsets == NV_TRUE))
    {
        for (StreamIndex = 0; StreamIndex < pCoreContext->pCore->totalStreams; StreamIndex++)
        {
            ProduceIndex = pCoreContext->ppOffsetListPerStream[StreamIndex]->ProduceIndex;
            for (j = 0; j < ProduceIndex; j++)
            {
                pBuffer = pCoreContext->ppOffsetListPerStream[StreamIndex]->ppOffsetList[j]->pOffsetList;

                if (pCoreContext->pCore->eCoreType == NvMMParserCoreType_AsfParser)
                    pCoreContext->pCore->ReleaseOffsetList(pCoreContext->pCore, pBuffer, StreamIndex);
                else
                    SuperParserReleaseOffsetList(pBuffer, pContext, pCoreContext->pCore);

                NvMMUtilDeallocateBuffer(pBuffer);
                NvOsFree(pBuffer);
                NvOsFree(pCoreContext->ppOffsetListPerStream[StreamIndex]->ppOffsetList[j]);
            }
            pCoreContext->ppOffsetListPerStream[StreamIndex]->ProduceIndex = 0;
            pCoreContext->ppOffsetListPerStream[StreamIndex]->ConsumeIndex = 0;
            pCoreContext->ppOffsetListPerStream[StreamIndex]->NumberofOffsetLists = 0;
            pCoreContext->ppOffsetListPerStream[StreamIndex]->bEndOfStream = NV_FALSE;

        }
    }
    return Status;
}


NvError SuperParserAdjustParserCoreForSeek(NvMMBlockHandle hBlock, NvMMParserCoreHandle pCore, NvU64 SeekPos)
{
    NvError Status = NvSuccess;
    NvMMSuperParserAdjustOffsetsMsg AdjustOffsetsMsg;
    NvMMSuperParserBlockContext* pContext = (NvMMSuperParserBlockContext*)hBlock->pContext;

    NvOsMemset(&AdjustOffsetsMsg, 0, sizeof(NvMMSuperParserAdjustOffsetsMsg));
    AdjustOffsetsMsg.eMessageType = NvMMSuperParserMsgType_AdjustOffsetsForSeek;
    AdjustOffsetsMsg.StructSize = sizeof(NvMMSuperParserAdjustOffsetsMsg);
    AdjustOffsetsMsg.pCore = pCore;
    AdjustOffsetsMsg.SeekPos = SeekPos;
    AdjustOffsetsMsg.bNeedAck = NV_TRUE;
    AdjustOffsetsMsg.Status = NvSuccess;
    AdjustOffsetsMsg.pStatus = &AdjustOffsetsMsg.Status;
    Status = NvOsSemaphoreCreate(&AdjustOffsetsMsg.hMessageAckSema, 0);
    if (Status != NvSuccess)
        return Status;
    Status = NvMMQueueEnQ(pContext->hMsgQueue, &AdjustOffsetsMsg, 0);
    if (Status != NvSuccess)
    {
        NvOsSemaphoreDestroy(AdjustOffsetsMsg.hMessageAckSema);
        return Status;
    }
    NvOsSemaphoreSignal(pContext->hOffsetThreadSema);
    NvOsSemaphoreWait(AdjustOffsetsMsg.hMessageAckSema);
    NvOsSemaphoreDestroy(AdjustOffsetsMsg.hMessageAckSema);
    Status = AdjustOffsetsMsg.Status;
    return Status;
}

static void SuperParserDeleteOffselistStucture(NvMMSuperParserBlockContext *pContext, NvMMParserCoreContext *pCoreContext)
{
    NvU32 StreamIndex;

    if ((pCoreContext->pCore->bEnableUlpMode) && (pCoreContext->bPreCacheOffsets == NV_TRUE))
    {
        for (StreamIndex = 0; StreamIndex < pCoreContext->pCore->totalStreams; StreamIndex++)
        {
            NvOsFree(pCoreContext->ppOffsetListPerStream[StreamIndex]->ppOffsetList);
            NvOsFree(pCoreContext->ppOffsetListPerStream[StreamIndex]);
        }
        NvOsFree(pCoreContext->ppOffsetListPerStream);
    }
    pCoreContext->bPreCacheOffsets = NV_FALSE;
}



NvError SuperParserAdjustParserCoreForSeekPvt(NvMMBlockHandle hBlock, NvMMParserCoreHandle pCore, NvU64 SeekPos)
{
    NvError Status = NvSuccess;
    NvU32 ProduceIndex;
    NvU32 i;
    NvU64 SeekPosTozero = 0;
    NvBool bOffsetsProduced;
    NvMMParserCoreContext *pCoreContext;
    NvMMSuperParserBlockContext *pContext = (NvMMSuperParserBlockContext*)hBlock->pContext;

    pCoreContext = &pContext->ParserCore;
    for (i = 0; i < pCoreContext->pCore->totalStreams; i++)
    {
        /* Reset the end of stream on parser stream*/
        pCoreContext->bEosEventSent[i]    = NV_FALSE;
        pCoreContext->bEndOfStream[i]     = NV_FALSE;
        pContext->block.pStreams[i]->bEndOfStream = NV_FALSE;
        pContext->block.pStreams[i]->bEndOfTrack = NV_FALSE;
    }

    pContext->isVideoEOS = NV_FALSE;

    if ((pCoreContext->pCore->bEnableUlpMode) && (pCoreContext->bPreCacheOffsets == NV_TRUE))
    {
        bOffsetsProduced = NV_FALSE;
        for (i = 0; i < pCoreContext->pCore->totalStreams; i++)
        {
            ProduceIndex = pCoreContext->ppOffsetListPerStream[i]->ProduceIndex;
            if (ProduceIndex > 0)
                bOffsetsProduced = NV_TRUE;
        }

        if (bOffsetsProduced == NV_TRUE)
        {
            SuperParserDeleteOffsetsPvt(hBlock, pCoreContext->pCore);
            SuperParserDeleteOffselistStucture(pContext, pCoreContext);
            pCoreContext->pCore->SetPosition(pCoreContext->pCore, &SeekPosTozero);
        }
    }

    Status = pCoreContext->pCore->SetPosition(pCoreContext->pCore, &SeekPos);
    if(Status == NvSuccess)
    {
        NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "SP:: SetPostion: %llu ", SeekPos));
        pContext->SInfo.SeekTS = SeekPos;
        // After seek, we want to resume playback after buffering till start mark instead of high mark
        if (pContext->bEnableBufferingForStreaming && pContext->SInfo.IsStreaming)
        {
            pContext->SInfo.nWaitCounter = 0;
            pContext->SInfo.m_InitialBuffering = NV_TRUE;
            if(pContext->SInfo.bCanSeekByTime)
            {
                pContext->SInfo.nSeekBytes = (SeekPos * pContext->SInfo.m_FileBitRate) / 80000000;
                NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO,
                      "SeekPos: %llu, TotalDuration: %llu, SeekBytes: %llu, fileSize: %llu Bitrate: %u\n ",
                       SeekPos, pContext->SInfo.m_rtDuration,
                       pContext->SInfo.nSeekBytes, pContext->SInfo.m_nFileSize,
                       pContext->SInfo.m_FileBitRate));
            }
        }
    }

    return Status;
}


NvError NvMMSuperParserGetCurrentPlaybackTime(NvMMBlockHandle hBlock,  NvU64 *time, NvU32 *TrackNumber)
{
    NvError Status = NvSuccess;
    *time = NvOsGetTimeUS() * 10;
    return Status;
}

static NvError 
NvMMSuperParserBlockGetNextWorkUnitOffsets(NvMMBlockHandle hBlock,
                                           NvMMParserCoreHandle hParserCore, 
                                           NvU32 StreamIndex, 
                                           NvMMBuffer *pBuffer, 
                                           NvU32 *size)
{
    NvError Status = NvSuccess;
    NvMMSuperParserBlockContext* pContext = (NvMMSuperParserBlockContext*)hBlock->pContext;
    NvMMParserCoreContext *pCoreContext;
    NvMMOffsetList *pSrcOffsetList;
    NvMMOffsetList *pDestOffsetList;
    NvU32 MaxOffsetsPerOffsetList = 0;
    NvU32 OffsetToTableOrg = 0;
    NvU32 OffsetToTableDest = 0;
    NvU32 pCount = 0;
    NvU32 ConsumeIndex;
    NvBool bCpNotReady;

    hParserCore->GetMaxOffsets(hParserCore, &MaxOffsetsPerOffsetList);
    pCoreContext = &pContext->ParserCore;
    ConsumeIndex = pCoreContext->ppOffsetListPerStream[StreamIndex]->ConsumeIndex;

    OffsetToTableDest = OffsetToTableOrg = sizeof(NvMMOffsetList);
    pDestOffsetList = (NvMMOffsetList *)pBuffer->Payload.Ref.pMem;
    if(pCoreContext->bPrepareOffsets == NV_FALSE)
    {
        pCoreContext->bPrepareOffsets = NV_TRUE;
        SuperParserCreateOffsets(hBlock, pCoreContext->pCore);
        Status = NvError_ContentPipeNotReady;
        *size = 0;
        return Status;
    }

    if (ConsumeIndex < pCoreContext->ppOffsetListPerStream[StreamIndex]->NumberofOffsetLists)
    {
        bCpNotReady = NV_TRUE;
        while ((ConsumeIndex < pCoreContext->ppOffsetListPerStream[StreamIndex]->NumberofOffsetLists) &&
               (pCoreContext->ppOffsetListPerStream[StreamIndex]->ppOffsetList[ConsumeIndex]->bDataReady == NV_TRUE))
        {
            pSrcOffsetList = pCoreContext->ppOffsetListPerStream[StreamIndex]->ppOffsetList[ConsumeIndex]->pOffsetList->Payload.Ref.pMem;
            pCount += pSrcOffsetList->numOffsets;
            if (pCount > MaxOffsetsPerOffsetList)
            {
                pCount -= pSrcOffsetList->numOffsets;
                break;
            }
            NvOsMemcpy(((NvU8 *)pDestOffsetList + OffsetToTableDest), ((NvU8*)pSrcOffsetList + OffsetToTableOrg), (sizeof(NvMMOffset) * pSrcOffsetList->numOffsets));
            OffsetToTableDest += (sizeof(NvMMOffset) * pSrcOffsetList->numOffsets);
            pCoreContext->ppOffsetListPerStream[StreamIndex]->ConsumeIndex ++;
            ConsumeIndex = pCoreContext->ppOffsetListPerStream[StreamIndex]->ConsumeIndex;
            pDestOffsetList->maxOffsets = pSrcOffsetList->maxOffsets;
            pDestOffsetList->tableIndex = pSrcOffsetList->tableIndex;
            pDestOffsetList->ConsumedSize = pSrcOffsetList->ConsumedSize;
            pDestOffsetList->VBase = pSrcOffsetList->VBase;
            pDestOffsetList->PBase = pSrcOffsetList->PBase;
            pDestOffsetList->BuffMgrIndex = pSrcOffsetList->BuffMgrIndex;
            pDestOffsetList->pParserCore = pSrcOffsetList->pParserCore ;
            bCpNotReady = NV_FALSE;
            if (ConsumeIndex == 1)
                break;
        }
        if (bCpNotReady == NV_TRUE)
            Status = NvError_ContentPipeNotReady;
        else
            pDestOffsetList->numOffsets = pCount;
        *size = sizeof(NvMMOffsetList);
    }
    else if (pCoreContext->ppOffsetListPerStream[StreamIndex]->bEndOfStream == NV_TRUE)
    {
        Status = NvError_ParserEndOfStream;
        NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "PCT: EOS: Stream:%d, Track:%s\n ",StreamIndex, pCoreContext->pTrackInfo->pPath));
    }
    else
    {
        NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "PCT: No data available, Offsets not prepared\n "));
        NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "PCT:NumberofOffsetLists  %d ConsumeIndex:%d\n ",pCoreContext->NumberofOffsetLists, ConsumeIndex));
        Status = NvError_ContentPipeNotReady;
    }

    return Status;
}

static NvError 
NvMMSuperParserBlockPrepareNextWorkUnitOffsets(NvMMBlockHandle hBlock,
                                               NvMMParserCoreHandle hParserCore, 
                                               NvU32 streamIndex, 
                                               NvMMBuffer *pBuffer, 
                                               NvU32 *size)
{
    NvError Status = NvSuccess;
    NvMMSuperParserBlockContext* pContext = (NvMMSuperParserBlockContext*)hBlock->pContext;
    NvMMStream *pStream = NULL;
    NvU32 StreamDataSize = 0;
    NvU32 MaxOffsetsPerOffsetList = 0;
    NvBool DoMoreWork = NV_FALSE;

    pStream = pContext->block.pStreams[streamIndex];
    if (!pStream)
    {
        return NvError_InvalidAddress;
    }

    pBuffer->PayloadInfo.BufferFlags |= NvMMBufferFlag_OffsetList;
    hParserCore->GetMaxOffsets(hParserCore, &MaxOffsetsPerOffsetList);
    NvmmSetMaxOffsets(pBuffer, MaxOffsetsPerOffsetList);
    NvmmResetOffsetList(pBuffer);

    hParserCore->pPipe->GetBaseAddress(hParserCore->cphandle, 
                                       &hParserCore->pBufferMgrBaseAddress, 
                                       (void **)&hParserCore->vBufferMgrBaseAddress);

    NvmmSetBaseAddress(hParserCore->pBufferMgrBaseAddress, hParserCore->vBufferMgrBaseAddress, pBuffer);   

    Status = hParserCore->GetNextWorkUnit(hParserCore, streamIndex, pBuffer, &StreamDataSize, &DoMoreWork);
    *size = StreamDataSize;
    return Status;
}

static void NvMMDeleteCore(NvMMSuperParserBlockContext *pContext, NvMMParserCoreContext *pCoreContext)
{
    NvU32 StreamIndex, j;
    NvMMBuffer *pBuffer;
    NvU32 ProduceIndex;
    NvError Status = NvSuccess;
    NvMMSuperParserDRMContextInfo *pDrmContext = NULL;
    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "++NvMMDeleteCore"));

    pCoreContext->bEmpty = NV_TRUE;

    /* Try to Destroy DRM context */
    pDrmContext = &pContext->DrmContextInfo;
    /* Destroy if any DRM context is done with playback  and committed*/
    if ((pDrmContext->bEmpty == NV_FALSE) &&
        (pDrmContext->NvDrmContext != 0) &&
        (pDrmContext->bPlayBackDone == NV_TRUE) &&
        (pDrmContext->bMeteringDone == NV_TRUE))
    {
        NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "NvMMDeleteCore: Destroy DRM context %x", pDrmContext->NvDrmContext));
        Status = ((NvError (*)(NvDrmContextHandle ))pContext->DrmContextInfo.NvDrmDestroyContext)((void *)pDrmContext->NvDrmContext);
        if (Status != NvError_Success)
        {
            NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_WARN, "NvMMDeleteCore: Failed to Destroy DRM context%x", pDrmContext->NvDrmContext));
        }
        else
        {
            pDrmContext->bEmpty = NV_TRUE;
            pDrmContext->NvDrmContext = 0;
            pDrmContext->bPlayBackDone = NV_FALSE;
            pDrmContext->bMeteringDone = NV_FALSE;
        }
    }

    if ((pCoreContext->pCore->bEnableUlpMode) && (pCoreContext->bPreCacheOffsets == NV_TRUE))
    {
        for (StreamIndex = 0; StreamIndex < pCoreContext->pCore->totalStreams; StreamIndex++)
        {
            ProduceIndex = pCoreContext->ppOffsetListPerStream[StreamIndex]->ProduceIndex;
            for (j = 0; j < ProduceIndex; j++)
            {
                pBuffer = pCoreContext->ppOffsetListPerStream[StreamIndex]->ppOffsetList[j]->pOffsetList;
                NvMMUtilDeallocateBuffer(pBuffer);
                NvOsFree(pBuffer);
                NvOsFree(pCoreContext->ppOffsetListPerStream[StreamIndex]->ppOffsetList[j]);
            }
            NvOsFree(pCoreContext->ppOffsetListPerStream[StreamIndex]->ppOffsetList);
            NvOsFree(pCoreContext->ppOffsetListPerStream[StreamIndex]);
        }
        NvOsFree(pCoreContext->ppOffsetListPerStream);
    }    
  
    if(pContext)
    {
        if(pContext->SInfo.IsStreaming)
        {    
            if(pContext->SInfo.BufferingShutdownSema)
                NvOsSemaphoreSignal(pContext->SInfo.BufferingShutdownSema);      
            
            if(pContext->SInfo.bufferingMonitorThread)
            {
                NvOsThreadJoin (pContext->SInfo.bufferingMonitorThread);
                pContext->SInfo.bufferingMonitorThread = NULL;
            }

            NV_LOGGER_PRINT ((NVLOG_NVMM_BUFFERING, NVLOG_DEBUG, "DeleteCore : NvOsThreadJoin\n"));
            
            if(pContext->SInfo.BufferingShutdownSema)
            {
                NvOsSemaphoreDestroy(pContext->SInfo.BufferingShutdownSema);
                pContext->SInfo.BufferingShutdownSema = NULL;
            }           
            
            if(pContext->SInfo.BufferingStartSema)
            {
                NvOsSemaphoreDestroy(pContext->SInfo.BufferingStartSema);
                pContext->SInfo.BufferingStartSema = NULL;
            }          
        }
    }

    if (pCoreContext->pCore->pStreamInfo)
    {
        NvOsFree(pCoreContext->pCore->pStreamInfo);
    }
    pCoreContext->pCore->Close(pCoreContext->pCore);
    NvMMDestroyParserCore(pContext,pCoreContext->pCore, pCoreContext->pCore->eCoreType);
    pCoreContext->pCore = NULL;
    if (pCoreContext->pTrackInfo)
    {
        NvOsFree(pCoreContext->pTrackInfo->pPath);
        NvOsFree(pCoreContext->pTrackInfo);
        pCoreContext->pTrackInfo = NULL;
    }
    pCoreContext->bPlayBackDone = NV_FALSE;
    pCoreContext->bPreCacheOffsets = NV_FALSE;
    pCoreContext->bDRMContextSentToDecoder = NV_FALSE;
    pCoreContext->NumberofOffsetLists = 0;

    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "--NvMMDeleteCore"));
}


static void NvMMSuperParserThread(void *arg)
{
    NvError Status = NvSuccess;
    NvMMBlockHandle hBlock = (NvMMBlockHandle)arg;
    NvMMSuperParserBlockContext* pContext = (NvMMSuperParserBlockContext*)hBlock->pContext;
    NvMMSuperParserMsg *pMessage=NULL;
    NvMMTrackInfo pTrackInfo;
    NvBool bShutDown = NV_FALSE;
    NvU32 pTracks;
    NvMMParserCoreContext *pCoreContext;
    NvMMSuperParserMsgType eMessageType;
    NvMMEventTracklistErrorInfo trackListError;

    pContext->bParsingPaused = NV_FALSE;
    pContext->pPausedCore = NULL;
    pMessage = NvOsAlloc(MAX_MSG_LEN * sizeof(NvU32));
    if (pMessage == NULL)
    {
        return;
    }
    NvOsMemset(pMessage, 0, (MAX_MSG_LEN * sizeof(NvU32)) );
    //Setting path equal to NULL before allocation to avoid crashing while freeing
    pTrackInfo.pPath = NULL;

    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "++NvMMSuperParserThread"));
    
    while (bShutDown == NV_FALSE)
    {
        NvOsSemaphoreWait(pContext->hOffsetThreadSema);
        Status = NvMMQueueDeQ(pContext->hMsgQueue, pMessage);
        if (Status != NvSuccess)
        {
            continue;
        }

        eMessageType = ((NvMMSuperParserMsg*)pMessage)->eMessageType;
        switch (eMessageType)
        {
        case NvMMSuperParserMsgType_StartParsing:
            {
                NvMMSuperParserStartParsingMsg *pParseMessage = (NvMMSuperParserStartParsingMsg*)pMessage;
                if (pParseMessage->bDeleteCores)
                {
                    //delete the cores to free memory
                    pCoreContext = &pContext->ParserCore;
                    if (pCoreContext->pCore)
                        NvMMDeleteCore(pContext, pCoreContext);
                    pParseMessage->bDeleteCores = NV_FALSE;
                }
                if (pParseMessage->bUpdateParseIndex == NV_TRUE)
                {
                    pContext->ParseIndex = pParseMessage->ParseIndex;
                    pParseMessage->bUpdateParseIndex = NV_FALSE;
                }
                do
                {
                    pTracks = 1;
                    if (pTracks > pContext->ParseIndex)
                    {
                        pTrackInfo.eTrackFlag     = pContext->pTrackInfo.eTrackFlag;
                        pTrackInfo.pClientPrivate = pContext->pTrackInfo.pClientPrivate;
                        pTrackInfo.uSize          = pContext->pTrackInfo.uSize;
                        pTrackInfo.eParserCore    = pContext->pTrackInfo.eParserCore;
                        pTrackInfo.trackListIndex = 0;
                        pTrackInfo.pPath = NvOsAlloc(sizeof(NvU8) * (pTrackInfo.uSize + 1));
                        if (pTrackInfo.pPath == NULL)
                        {
                            if (pParseMessage->bNeedAck == NV_TRUE)
                            {
                                pParseMessage->bNeedAck = NV_FALSE;
                                *pParseMessage->pParsingStatus = NvError_InsufficientMemory;
                                NvOsSemaphoreSignal(pParseMessage->hMessageAckSema);
                            }
                            break;
                        }
                        else
                        {
                            NvOsMemset(pTrackInfo.pPath, 0,  sizeof(NvU8) * (pTrackInfo.uSize + 1));
                            NvOsStrncpy(pTrackInfo.pPath, pContext->pTrackInfo.pPath, pContext->pTrackInfo.uSize);
                        }

                        pCoreContext = NULL;
                        Status = NvMMCreateParserCoreFromTrackInfo(hBlock, &pTrackInfo, &pCoreContext);
                        /*delete memory for path no more needed*/
                        if(pTrackInfo.pPath)
                        {
                            NvOsFree(pTrackInfo.pPath);
                            pTrackInfo.pPath = NULL;
                        }
                        if (Status != NvSuccess)
                        {
                            // no memory let us stop parsing untill memory is available.
                            if (Status == NvError_InsufficientMemory)
                            {
                                if (pParseMessage->bNeedAck == NV_TRUE)
                                {
                                    pParseMessage->bNeedAck = NV_FALSE;
                                    *pParseMessage->pParsingStatus = Status;
                                    NvOsSemaphoreSignal(pParseMessage->hMessageAckSema);
                                }
                                break;
                            }

                            if (pParseMessage->bNeedAck == NV_TRUE)
                            {
                                pParseMessage->bNeedAck = NV_FALSE;
                                *pParseMessage->pParsingStatus = Status;
                                NvOsSemaphoreSignal(pParseMessage->hMessageAckSema);
                            }
                            trackListError.structSize = sizeof(NvMMEventTracklistErrorInfo) ;
                            trackListError.event = NvMMEvent_TracklistError;
                            trackListError.error = Status;
                            trackListError.trackListIndex = pTrackInfo.trackListIndex;
                            trackListError.domainName = pTrackInfo.domainName;
                            if(trackListError.error == NvError_DrmLicenseNotFound)
                            {
                                /*Below memory is being sent to the client for processing,is it OK??*/
                                /*Also can SP close(memory is freed in SP close) be called even before event is processed ??*/
                                trackListError.licenseChallengeSize = pContext->licenseChallengeSize;
                                trackListError.licenseUrlSize = pContext->licenseUrlSize;
                                trackListError.licenseChallenge = pContext->licenseChallenge;
                                trackListError.licenseUrl = pContext->licenseUrl;
                             }
                                NV_LOGGER_PRINT ((NVLOG_TRACK_LIST, NVLOG_DEBUG, "Thread - Sending %x tracklist error event during create core ",Status ));
                            pContext->block.SendEvent( pContext->block.pSendEventContext, NvMMEvent_TracklistError, sizeof(NvMMEventTracklistErrorInfo), &trackListError);
                            break;
                        }

                        if (pCoreContext != NULL)
                        {
                            NV_LOGGER_PRINT ((NVLOG_TRACK_LIST, NVLOG_DEBUG, "Thread - Core Created %s Parse Index: %d ",pCoreContext->pTrackInfo->pPath, pContext->ParseIndex));
                            pCoreContext->bPrepareOffsets = pParseMessage->bPrepareOffsets;
                            Status = NvMMSuperParserBlockCreateStreams(hBlock, pCoreContext->pCore);
                            if (Status != NvSuccess)
                            {
                                if (pParseMessage->bNeedAck == NV_TRUE)
                                {
                                    pParseMessage->bNeedAck = NV_FALSE;
                                    //*pParseMessage->pParsingStatus = Status;
                                    pParseMessage->ParsingStatus = Status;
                                    NvOsSemaphoreSignal(pParseMessage->hMessageAckSema);
                                }
                                break;
                            }

                            /* Signal that core is created and ready to go */
                            if (pParseMessage->bNeedAck == NV_TRUE)
                            {
                                pParseMessage->bNeedAck = NV_FALSE;
                                *pParseMessage->pParsingStatus = Status;
                                NvOsSemaphoreSignal(pParseMessage->hMessageAckSema);
                            }
                            Status = SuperParserCreateOffsetsPvt(hBlock, pCoreContext->pCore);
                            if (Status != NvSuccess)
                            {
                                break;
                            }
                        }

                        break;
                    }
                    else
                    {
                        if (pParseMessage->bNeedAck == NV_TRUE)
                        {
                            pParseMessage->bNeedAck = NV_FALSE;
                            *pParseMessage->pParsingStatus = NvError_ParserFailure;
                            NV_LOGGER_PRINT ((NVLOG_TRACK_LIST, NVLOG_ERROR, "Thread - NvError_ParserFailure while creating core "));
                            NvOsSemaphoreSignal(pParseMessage->hMessageAckSema);
                        }
                        break;
                    }
                }while (pContext->block.bEnableUlpMode);
            }
            break;

        case NvMMSuperParserMsgType_DestroyThread:
            {
                NvMMSuperParserDestroyThreadMsg *Message = (NvMMSuperParserDestroyThreadMsg*)pMessage;
                bShutDown = NV_TRUE;
                NvOsSemaphoreSignal(Message->hMessageAckSema);
            }
            break;

        case NvMMSuperParserMsgType_UlpMode:
            {
                NvMMSuperParserUlpModeMsg *pModeMessage = (NvMMSuperParserUlpModeMsg*)pMessage;
                if (!pModeMessage->bUlpMode)
                {
                    pContext->ParseIndex = pContext->PlayIndex;
                    if (pModeMessage->bNeedAck)
                    {
                        NvOsSemaphoreSignal(pModeMessage->hMessageAckSema);
                    }
                }
                if ((pContext->bParsingPaused == NV_TRUE) && (pContext->pPausedCore))
                    SuperParserCreateOffsetsPvt(hBlock, pContext->pPausedCore);
            }
            break;

        case NvMMSuperParserMsgType_CreateParserCore:
            {
                NvMMSuperParserCreateParserCoreMsg *pCreateParserCoreMsg = (NvMMSuperParserCreateParserCoreMsg*)pMessage;
                NvMMParserCoreHandle pCore = pCreateParserCoreMsg->pCore; 
                NvMMTrackInfo *pTrackInfo = &pCreateParserCoreMsg->pTrackInfo;
                SuperParserCreateCorePvt(hBlock, pTrackInfo,  &pCore);
                if (pCreateParserCoreMsg->bNeedAck == NV_TRUE)
                    NvOsSemaphoreSignal(pCreateParserCoreMsg->hMessageAckSema);
                if ((pContext->bParsingPaused == NV_TRUE) && (pContext->pPausedCore))
                    SuperParserCreateOffsetsPvt(hBlock, pContext->pPausedCore);
            }
            break;

        case NvMMSuperParserMsgType_DeleteParserCore:
            {
                NvMMSuperParserDeleteParserCoreMsg *pDeleteParserCoreMsg = (NvMMSuperParserDeleteParserCoreMsg*)pMessage;
                NvMMParserCoreHandle pCore = pDeleteParserCoreMsg->pCore; 
                SuperParserDeleteCorePvt(hBlock, pCore);
                if (pDeleteParserCoreMsg->bNeedAck == NV_TRUE)
                    NvOsSemaphoreSignal(pDeleteParserCoreMsg->hMessageAckSema);
                if ((pContext->bParsingPaused == NV_TRUE) && (pContext->pPausedCore))
                    SuperParserCreateOffsetsPvt(hBlock, pContext->pPausedCore);
            }
            break;

        case NvMMSuperParserMsgType_CreateOffsets:
            {
                NvMMSuperParserCreateOffsetsMsg *pCreateOffsetsMsg = (NvMMSuperParserCreateOffsetsMsg*)pMessage;
                NvMMParserCoreHandle pCore = pCreateOffsetsMsg->pCore; 
                if (pCreateOffsetsMsg->bNeedAck == NV_TRUE)
                    NvOsSemaphoreSignal(pCreateOffsetsMsg->hMessageAckSema);
                SuperParserCreateOffsetsPvt(hBlock, pCore);
                if ((pContext->bParsingPaused == NV_TRUE) && (pContext->pPausedCore))
                    SuperParserCreateOffsetsPvt(hBlock, pContext->pPausedCore);
            }
            break;

        case NvMMSuperParserMsgType_DeleteOffsets:
            {
                NvMMSuperParserDeleteOffsetsMsg *pDeleteOffsetsMsg = (NvMMSuperParserDeleteOffsetsMsg*)pMessage;
                NvMMParserCoreHandle pCore = pDeleteOffsetsMsg->pCore; 
                SuperParserCreateOffsetsPvt(hBlock, pCore);
                if (pDeleteOffsetsMsg->bNeedAck == NV_TRUE)
                    NvOsSemaphoreSignal(pDeleteOffsetsMsg->hMessageAckSema);
                if ((pContext->bParsingPaused == NV_TRUE) && (pContext->pPausedCore))
                    SuperParserCreateOffsetsPvt(hBlock, pContext->pPausedCore);
            }
            break;

        case NvMMSuperParserMsgType_AdjustOffsetsForSeek:
            {
                NvMMSuperParserAdjustOffsetsMsg *AdjustOffsetsMsg = (NvMMSuperParserAdjustOffsetsMsg*)pMessage;
                NvMMParserCoreHandle pCore = AdjustOffsetsMsg->pCore; 
                NvU64 SeekPos = AdjustOffsetsMsg->SeekPos; 
                *(AdjustOffsetsMsg->pStatus) = SuperParserAdjustParserCoreForSeekPvt(hBlock, pCore, SeekPos);
                if (AdjustOffsetsMsg->bNeedAck == NV_TRUE)
                    NvOsSemaphoreSignal(AdjustOffsetsMsg->hMessageAckSema);
                if ((pContext->bParsingPaused == NV_TRUE) && (pContext->pPausedCore))
                    SuperParserCreateOffsetsPvt(hBlock, pContext->pPausedCore);
            }
            break;

        default:
            break;
        }
    }
    NvOsFree(pMessage);    
    NvOsFree(pTrackInfo.pPath);

    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "--NvMMSuperParserThread"));
}


/*
* EXTERNAL MANDATED OPEN/CLOSE ENTRYPOINTS
* 
* Every custom block must implement its version of
* these 3 functions and add them to nvmm.c.
*/

/**
*  Constructor for the block which is used by NvMMOpenMMBlock.
*/
NvError 
NvMMSuperParserBlockOpen(NvMMBlockHandle *phBlock,
                         NvMMInternalCreationParameters *pParams,
                         NvOsSemaphoreHandle BlockEventSema,
                         NvMMDoWorkFunction *pDoWorkFunction)
{
    NvError Status = NvSuccess;
    NvMMBlock* pBlock = NULL; /* MM block referenced by handle */ 
    NvMMSuperParserBlockContext* pContext = NULL; /* Context of this block implementation */
    NvMMBlockContext* blockContext = NULL;
    NvMMSuperParserDRMContextInfo *pDrmContext = NULL;

    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "++NvMMSuperParserBlockOpen"));
    
    // Call base class open function.
    // Allocates memory and populates NvMMBlock and our private context struct
    // with default values and function pointers for default implementations.
    NVMM_CHK_ERR (NvMMBlockOpen(&pBlock,
                           sizeof(NvMMSuperParserBlockContext),
                           pParams, 
                           BlockEventSema, 
                           NvMMSuperParserBlockDoWork,
                           NvMMSuperParserBlockPrivateClose,
                           NvMMSuperParserBlockGetBufferRequirements));

    // Override default methods
    pBlock->SetAttribute = NvMMSuperParserBlockSetAttribute;
    pBlock->GetAttribute = NvMMSuperParserBlockGetAttribute;

    pContext        = (NvMMSuperParserBlockContext*)pBlock->pContext;
    blockContext    = (NvMMBlockContext *)&pContext->block;
    blockContext->NvMMBlockTBTBCallBack = NvMMSuperParserTBTBCallBack;
    blockContext->TransferBufferEventFunction = NvMMSuperParserBlockTransferBufferEventFunction;
    blockContext->BlockType = NvMMBlockType_SuperParser;
    pContext->errorFlag = NV_TRUE;

    pDrmContext = &pContext->DrmContextInfo;
    pDrmContext->NvDrmContext = 0;
    pDrmContext->NvDrmDestroyContext = 0;
    pDrmContext->NvDrmUpdateMeteringInfo = 0;
    pDrmContext->bPlayBackDone = NV_FALSE;
    pDrmContext->bMeteringDone = NV_FALSE;
    pDrmContext->bEmpty = NV_TRUE;

    pContext->ParserCore.pCore = NULL;
    pContext->ParserCore.bEmpty = NV_TRUE;
    pContext->ParserCore.bPlayBackDone = NV_FALSE;
    pContext->bSendFlushBufferEvent = NV_FALSE;
    pContext->prevStreamType   = NvMMStreamType_OTHER;
    pContext->pCurPlaybackCore = NULL;
    pContext->bFlushBufferEventSend = NV_FALSE;
    pContext->flushBufferAckRequired = NV_FALSE;
    pContext->getCoreFailureCount = 0;
    pContext->bResetEOS = NV_FALSE;
    pContext->bInvalidateIndex  = NV_FALSE;
    pContext->bEnableBufferingForStreaming = NV_TRUE;

    // Success - return block handle and work function
    if (pDoWorkFunction)
    {
        *pDoWorkFunction = NvMMBlockDoWork; // move callback into NvMMBlock
    }

    NVMM_CHK_ERR (NvOsSemaphoreCreate(&pContext->hOffsetThreadSema, 0));

    NVMM_CHK_ERR (NvOsSemaphoreCreate(&pContext->hCpWaitSema, 0));

    NVMM_CHK_ERR (NvOsSemaphoreCreate(&pContext->hFlushBufferCompleteWaitSema, 0));

    NVMM_CHK_ERR (NvMMQueueCreate(&pContext->hMsgQueue, MAX_QUEUE_LEN, MAX_MSG_LEN * sizeof(NvU32), NV_TRUE));

    NVMM_CHK_ERR (NvOsThreadCreate(NvMMSuperParserThread, pBlock, &pContext->hThread));
    
    NVMM_CHK_ERR (NvOsMutexCreate(&pContext->hLock));

    NVMM_CHK_ERR (NvOsMutexCreate(&pContext->hCoreLock));

    pContext->hBlock = pBlock;
    *phBlock = pBlock;
    
cleanup:
    if (Status != NvSuccess)
    {
        NvMMSuperParserBlockPrivateClose(pBlock);
    }
    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "--NvMMSuperParserBlockOpen: Status=0x%x pBlock=0x%x", Status, pBlock));
    return Status;
}

/** 
* Destructor for the block.
* First block implementation specific resources are freed, and
* then the NvMMBlock base class close function is called.
*/
static void NvMMSuperParserBlockPrivateClose(NvMMBlockHandle hBlock)
{
    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "++NvMMSuperParserBlockPrivateClose: hBlock=0x%x", hBlock));
    
    if (hBlock)
    {
        NvMMSuperParserBlockContext* const pContext = hBlock->pContext;
        NvU32 i;
        NvMMParserCoreContext *pCoreContext;
        NvMMSuperParserDestroyThreadMsg Message;
        NvError Status = NvSuccess;
        NvMMSuperParserDRMContextInfo *pDrmContext = NULL;

        if (pContext)
        {
            NvU32 nStreamCount = pContext->block.StreamCount;
            if(pContext->licenseUrl)
                 NvOsFree(pContext->licenseUrl);
            if(pContext->licenseChallenge)
                  NvOsFree(pContext->licenseChallenge);

            pCoreContext = &pContext->ParserCore;
            if ((pCoreContext->bEmpty == NV_FALSE) && (pCoreContext->pCore))
            {
                SuperParserDeleteCore(hBlock, pCoreContext->pCore);
            }

            NvOsMemset(&Message, 0 , sizeof(NvMMSuperParserDestroyThreadMsg));
            Message.eMessageType = NvMMSuperParserMsgType_DestroyThread;
            Message.StructSize = sizeof(NvMMSuperParserDestroyThreadMsg);
            Status = NvOsSemaphoreCreate(&Message.hMessageAckSema, 0);
            if (Status != NvSuccess)
                return;
            NvMMQueueEnQ(pContext->hMsgQueue, &Message, 0);
            NvOsSemaphoreSignal(pContext->hOffsetThreadSema);
            NvOsSemaphoreWait(Message.hMessageAckSema);
            NvOsSemaphoreDestroy(Message.hMessageAckSema);

            for (i = 0; i< nStreamCount; i++)
            {
                NvMMBlockDestroyStream(hBlock, i);
            }
            NvOsThreadJoin(pContext->hThread);

            /* Destroy any remaining DRM context */
            pDrmContext = &pContext->DrmContextInfo;
            if ((pDrmContext->bEmpty == NV_FALSE) &&
                (pDrmContext->NvDrmContext != 0) &&
                (pDrmContext->NvDrmDestroyContext != 0))
            {
                Status = ((NvError (*)(NvDrmContextHandle ))pDrmContext->NvDrmDestroyContext)((void *)pDrmContext->NvDrmContext);
                if (Status == NvError_Success)
                {
                    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "Destroy DRM context %x", pDrmContext->NvDrmContext));
                    pDrmContext->bEmpty = NV_TRUE;
                    pDrmContext->NvDrmContext = 0;
                    pDrmContext->NvDrmDestroyContext = 0;
                    pDrmContext->NvDrmUpdateMeteringInfo = 0;
                    pDrmContext->bPlayBackDone = NV_FALSE;
                    pDrmContext->bMeteringDone = NV_FALSE;
                }
            }
            if (pContext->hDrmHandle)
            {
                NvOsLibraryUnload(pContext->hDrmHandle);
                pContext->hDrmHandle = NULL;
            }

            if (pContext->hAsfHandle)
            {
                NvOsLibraryUnload(pContext->hAsfHandle);
                pContext->hAsfHandle = NULL;
            }
            if(pContext->pTrackInfo.pPath)
               NvOsFree(pContext->pTrackInfo.pPath);
            NvOsSemaphoreDestroy(pContext->hOffsetThreadSema);
            NvOsSemaphoreDestroy(pContext->hCpWaitSema);
            NvOsSemaphoreDestroy(pContext->hFlushBufferCompleteWaitSema);

            NvMMQueueDestroy(&pContext->hMsgQueue);
            NvOsMutexDestroy(pContext->hLock);
            NvOsMutexDestroy(pContext->hCoreLock);
        }

        // call base class close function at the end, will free memory for handles
        NvMMBlockClose(hBlock);
    }

    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "--NvMMSuperParserBlockPrivateClose: hBlock=0x%x", hBlock));
}

/** 
* Close function which is called by NvMMCloseBlock().
* 
* If possible block will be closed here, otherwise it will be marked for close.
* Block will be closed whenever possible in future.
*/
void NvMMSuperParserBlockClose(NvMMBlockHandle hBlock, NvMMInternalDestructionParameters *pParams)
{
    NvMMBlockTryClose(hBlock);
}

static NvError 
NvMMSuperParserBlockTransferBufferEventFunction(void *hBlock, 
                                                NvU32 StreamIndex, 
                                                NvMMBufferType BufferType, 
                                                NvU32 BufferSize, 
                                                void* pBuffer,
                                                NvBool *bEventHandled)
{
    NvError Status = NvSuccess;
    NvU32 EventType;

    *bEventHandled = NV_FALSE;
    EventType = (NvU32)(((NvMMStreamEventBase*)pBuffer)->event);
    switch (EventType)
    {
    case NvMMEvent_StreamEnd:
        *bEventHandled = NV_TRUE;
        break;

    case NvMMEvent_StreamShutdown:
        break;    

    default:
        break;
    }

    return Status;
}

static NvBool 
NvMMSuperParserBlockGetBufferRequirements(NvMMBlockHandle hBlock, 
                                          NvU32 StreamIndex, 
                                          NvU32 Retry, 
                                          NvMMNewBufferRequirementsInfo *pBufReq)
{
    NvMMSuperParserBlockContext* const pContext = hBlock->pContext;
    NvMMParserCoreContext *pCoreContext = NULL;

    if (Retry > 0)
    {
        return NV_FALSE;
    }

    pCoreContext = &pContext->ParserCore;
    if (pCoreContext && pCoreContext->pCore && pCoreContext->pCore->GetBufferRequirements)
    {
        pCoreContext->pCore->GetBufferRequirements(pCoreContext->pCore, StreamIndex, Retry, pBufReq);
        if (NVMM_ISSTREAMAUDIO(pCoreContext->pCore->pStreamInfo[StreamIndex].StreamType))
            pBufReq->bInSharedMemory = NV_TRUE;
    }
    else
    {
        NvOsMemset (pBufReq, 0, sizeof(NvMMNewBufferRequirementsInfo));
        pBufReq->structSize    = sizeof(NvMMNewBufferRequirementsInfo);
        pBufReq->event         = NvMMEvent_NewBufferRequirements;
        pBufReq->minBuffers    = 4;
        pBufReq->maxBuffers    = NVMMSTREAM_MAXBUFFERS;
        pBufReq->memorySpace   = NvMMMemoryType_SYSTEM;
        pBufReq->byteAlignment = 4;

        //Default Requirements, no core exists.
        switch (StreamIndex)
        {
        case NvMMBlockSuperParserStream_VidOut :
            pBufReq->minBufferSize = 32768;
            pBufReq->maxBufferSize = 524288;
            break;
        case NvMMBlockSuperParserStream_AudOut :
            pBufReq->minBufferSize = 512;
            pBufReq->maxBufferSize = 65536;
            break;
        default:
            NV_ASSERT(0);
            break;
        }
    }
    return NV_TRUE;
}

static NvError HandleReturnFromGetNextWU(NvMMSuperParserBlockContext*  pContext, 
                                         NvU32 pCount, NvU32 StreamDataSize,
                                         NvMMBuffer *pStreamBufNext,
                                         NvMMStream *pStream, NvU32 nStreamNum,
                                         NvError RetStatus, NvBool bReEnqueue)
{   
    NvBool UlpMode = pContext->pCurPlaybackCore->bEnableUlpMode;
    NvMMParserCoreContext *pCoreContext = &pContext->ParserCore;
    NvS32 rate = pContext->pCurPlaybackCore->GetRate(pContext->pCurPlaybackCore);
    NvError Status = NvSuccess;
    NvMMEventMarkerHitInfo MarkerHitEvent;
    NvMMEventNewMetaDataAvailable NewMetaDataAvailableEvent; 
    NvS64 deltaTimeStamp = 0, ThisTimeStamp = 0;
    NvMMErrorDomainDRMInfo info;

HandleGetNextWUStart:

    // Go inside this loop only when there is no markerhit and normal case
    if ((NvSuccess == RetStatus || pCount) && (RetStatus != NvError_ParserMarkerHit))
    {
        if (RetStatus == NvError_InsufficientMemory)
        {
            pContext->waitForBufferRelease = NV_TRUE;
        }
        // If sending the header, update time stamp to zero
        if (pStreamBufNext->PayloadInfo.BufferFlags & NvMMBufferFlag_HeaderInfoFlag)
        {
            pStream->LastTimeStamp = 0;
        }
        // Skip the I-frames if they are too near.
        // Calculate the delta timestamp between this timestamp and last time stamp to make decision
        if ((rate > 2000 || rate < 0) &&
            (NVMM_ISSTREAMVIDEO(pContext->pCurPlaybackCore->pStreamInfo[nStreamNum].StreamType)) &&
            (UlpMode == NV_FALSE) &&
            !(pStreamBufNext->PayloadInfo.BufferFlags & NvMMBufferFlag_HeaderInfoFlag))
        {
            ThisTimeStamp = pStreamBufNext->PayloadInfo.TimeStamp;
            // Make sure we send the I-frame at zero timestamp
            if (rate && ThisTimeStamp)
            {
                deltaTimeStamp = (((ThisTimeStamp - pStream->LastTimeStamp)*1000)/rate);
                //If the difference between timestamps is negative, make it positive
                if (deltaTimeStamp < 0)
                    deltaTimeStamp = -deltaTimeStamp;

                if (deltaTimeStamp >= pContext->pCurPlaybackCore->perFrameTime)
                {
                    //Send this frame to decoder
                    pStream->LastTimeStamp = ThisTimeStamp;
                }
                else
                {
                    //Skip the frame
                    StreamDataSize = 0;
                }
            }
        }
        // Think of sending the buffer out only if its NvSuccess
        if (StreamDataSize)
        {
            if ((pCoreContext->pCore->NvDrmContext) && pCoreContext->bDRMContextSentToDecoder == NV_FALSE)
            {
                pCoreContext->bDRMContextSentToDecoder = NV_TRUE;
                NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "DRM Context has been sent to DECODER:%x \n",pCoreContext->pCore->NvDrmContext));
            }
            // Nonzero parsed data means, need to send out.
            // Parsers should make StreamDataSize zero if parsed data is to be ignored.
            if (!UlpMode)
            {
                // Update the current time stamp
                pStream->LastTimeStamp = pStreamBufNext->PayloadInfo.TimeStamp;
            }

            pContext->block.pStreams[0]->bEndOfTrack = NV_FALSE;

            if (NVMM_ISSTREAMAUDIO(pContext->pCurPlaybackCore->pStreamInfo[nStreamNum].StreamType))
                NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_VERBOSE,
                                "AudioTSMsec - %lld, size : %u, Buffer Id: %u\n",
                                pStreamBufNext->PayloadInfo.TimeStamp/10000,
                                StreamDataSize,
                                pStreamBufNext->BufferID));
            else if (NVMM_ISSTREAMVIDEO(pContext->pCurPlaybackCore->pStreamInfo[nStreamNum].StreamType))
                NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_VERBOSE,
                                "VideoTSMsec - %lld, size : %u, Buffer Id: %u\n",
                                pStreamBufNext->PayloadInfo.TimeStamp/10000,
                                StreamDataSize,
                                pStreamBufNext->BufferID));
            
            if(pStream->TransferBufferFromBlock)
            {
            Status = pStream->TransferBufferFromBlock(pStream->pOutgoingTBFContext, 
                                                      pStream->OutgoingStreamIndex, 
                                                      NvMMBufferType_Payload,
                                                      sizeof(NvMMBuffer),
                                                      pStreamBufNext);
            }

            if (Status != NvSuccess)
            {
                return Status;
            }
        }
        else if (bReEnqueue)
        {
            // EnQ the buffer back to Q, No point in sending empty buffer.
            Status = NvMMQueueEnQ(pStream->BufQ, &pStreamBufNext, 0);
            if (Status != NvSuccess)
            {
                return Status;
            }
        }

        if (NvError_ContentPipeNotReady == RetStatus)
        {
            return Status;
        }
    }
    else if (NvError_ParserEndOfStream == RetStatus || NvError_EndOfFile == RetStatus)
    {
        if (pStream->bEndOfTrack == NV_TRUE)
        {
            
            Status = NvMMQueueEnQ(pStream->BufQ, &pStreamBufNext, 0);
            return Status;
        }

        pCoreContext->bEndOfStream[nStreamNum] = NV_TRUE;
        if ((rate != 1000) && 
            (NVMM_ISSTREAMVIDEO(pContext->pCurPlaybackCore->pStreamInfo[nStreamNum].StreamType)))
        {
            pContext->isVideoEOS = NV_TRUE;
        }

        pStream->bEndOfTrack = NV_TRUE;

        Status = NvMMQueueEnQ(pStream->BufQ, &pStreamBufNext, 0);
        if(Status != NvSuccess)
        {
            return Status;
        }

        pStream->bEndOfStream = NV_TRUE;
        if(pCoreContext->bEosEventSent[nStreamNum] == NV_FALSE)
        {
            NV_LOGGER_PRINT ((NVLOG_TRACK_LIST, NVLOG_DEBUG, "Block -  Super Parser Sending EOS(1): PlayIndex = %d ", pContext->PlayIndex));
            NvMMSendEndOfStreamEvent(pStream, nStreamNum, pContext);
            pCoreContext->bEosEventSent[nStreamNum] = NV_TRUE;
        }
    }
    else if (RetStatus == NvError_ParserMarkerHit)
    {
        // Sending MarkerHit event
        MarkerHitEvent.structSize = sizeof(NvMMEventMarkerHitInfo);
        MarkerHitEvent.event = NvMMEvent_MarkerHit;
        MarkerHitEvent.MarkerNumber = StreamDataSize; //FIXME WORKAROUND
        StreamDataSize = pStreamBufNext->Payload.Ref.sizeOfValidDataInBytes;

        // Send MarkerHit Blockevent To client
        pContext->block.SendEvent(pContext->block.pSendEventContext, 
                                  NvMMEvent_MarkerHit, 
                                  sizeof(NvMMEventMarkerHitInfo), 
                                  &MarkerHitEvent);

        // Think of sending the buffer out only if its NvSuccess
        if (StreamDataSize && pStream->TransferBufferFromBlock)
        {
            // Nonzero parsed data means, need to send out.
            // Parsers should make StreamDataSize zero if parsed data is to be ignored.
            Status = pStream->TransferBufferFromBlock(pStream->pOutgoingTBFContext, 
                                                      pStream->OutgoingStreamIndex, 
                                                      NvMMBufferType_Payload,
                                                      sizeof(NvMMBuffer),
                                                      pStreamBufNext);
            if (Status != NvSuccess)
                return Status;
        }
    }
    else if (NvError_NewMetaDataAvailable == RetStatus)
    {
         // Sending MarkerHit event
        NewMetaDataAvailableEvent.structSize = sizeof(NvMMEventNewMetaDataAvailable);
        NewMetaDataAvailableEvent.event = NvMMEvent_NewMetaDataAvailable;            
        
         // Send NewMetaDataAvailable Blockevent To client
        pContext->block.SendEvent(pContext->block.pSendEventContext, 
                                  NvMMEvent_NewMetaDataAvailable, 
                                  sizeof(NvMMEventNewMetaDataAvailable), 
                                  &NewMetaDataAvailableEvent);
         
        if (bReEnqueue)
            Status = NvMMQueueEnQ(pStream->BufQ, &pStreamBufNext, 0);
        if (Status != NvSuccess)
            return Status;
    }
    else if (NvError_ContentPipeNotReady == RetStatus)
    {
        if (bReEnqueue)
            Status = NvMMQueueEnQ(pStream->BufQ, &pStreamBufNext, 0);
        return Status;
    }
    else if (RetStatus == NvError_InsufficientMemory)
    {
        pContext->waitForBufferRelease = NV_TRUE;
        // Any other Error: means no data to be passed out of parser block.
        // EnQ the buffer back to Q
        if (bReEnqueue)
            Status = NvMMQueueEnQ(pStream->BufQ, &pStreamBufNext, 0);
        if (Status != NvSuccess)
            return Status;
    }
    else // Any other Error
    {
        
        pContext->errorCount++;
        if ((RetStatus == NvError_FileOperationFailed)||(RetStatus == NvError_FileReadFailed))
        {
            if (pContext->errorFlag)
            {
                info.error = RetStatus;
                info.TrackIndex = pContext->pTrackInfo.trackListIndex;
                SuperparserBlockReportError(pContext->block,  RetStatus, pContext->pTrackInfo.domainName, (void*)&info );
                pContext->errorFlag = NV_FALSE;
            }
        }
        else
        {
            info.error = RetStatus;
            info.TrackIndex = pContext->pTrackInfo.trackListIndex;
            SuperparserBlockReportError(pContext->block,  RetStatus, pContext->pTrackInfo.domainName, (void*)&info );
        }
        if(pContext->errorCount > 5 || NvError_DrmFailure == RetStatus)
        {
            RetStatus = NvError_ParserEndOfStream;
            goto HandleGetNextWUStart;
        }
        // Any other Error: means no data to be passed out of parser block.
        // EnQ the buffer back to Q
        if (bReEnqueue)
            Status = NvMMQueueEnQ(pStream->BufQ, &pStreamBufNext, 0);
        if (Status != NvSuccess)
            return Status;
    }

    return Status;
}


/*
* This function tries to do something with the data.
*
* The parser base just populates the audio and video output buffers with the 
* next audio and video frames.
* 
*/
static NvError NvMMSuperParserBlockDoWork(NvMMBlockHandle hBlock, NvMMDoWorkCondition condition)
{
    NvMMSuperParserBlockContext* const pContext = hBlock->pContext;
    NvError Status = NvSuccess; 
    NvError RetStatus = NvSuccess;
    NvU32 StreamDataSize, i, j;
    NvS32 rate = 1;
    NvBool bEmptyStreamBufferPending = NV_FALSE;
    NvBool DoMoreWork = NV_FALSE;
    NvMMStream *pStream = NULL;
    NvMMBuffer *pStreamBufNext = NULL;
    NvU32 EntriesStreamQ, pCount = 0, MaxOffsetsPerOffsetList = 0;
    NvBool UlpMode;
    NvBool bAllStreamsEnded;
    NvBool bAllStreamEndEventSent;
    NvMMParserCoreContext *pCoreContext;
    NvMMStream *pCurStream = NULL;
    NvU32 TotalStreams;
    
    if((pContext->block.State == NvMMState_Paused) &&  
        (pContext->SInfo.IsStreaming) &&
        (pContext->pCurPlaybackCore) &&
        (pContext->pCurPlaybackCore->pPipe) &&
        (pContext->pCurPlaybackCore->cphandle))
    {
        pContext->pCurPlaybackCore->pPipe->StartCaching(pContext->pCurPlaybackCore->cphandle);
    }
    // If state is not running just return.
    if(pContext->block.State != NvMMState_Running)
    {
        return NvSuccess;
    }

    // Restrict sending buffers to decoder till we have enough buffer for playback.
    if (pContext->bEnableBufferingForStreaming &&
        pContext->SInfo.IsStreaming &&
        pContext->SInfo.m_InitialBuffering)
    {
        NvmmSendBufferingEvent(pContext,pContext->SInfo.m_BufferingPercent, NV_TRUE);
        NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "Returning without any work !!!!!!!!!!!!!!!!!!!!"));
        return NvSuccess;
    }

    pCoreContext = &pContext->ParserCore;
    if((pCoreContext->bEmpty == NV_TRUE) || (pCoreContext->pCore == NULL))
    {
        NV_LOGGER_PRINT ((NVLOG_TRACK_LIST, NVLOG_DEBUG, "Block -  pCoreContext is Empty\n"));
    }
    else
    {
        pContext->pCurPlaybackCore = pCoreContext->pCore;
    }

    // we are not deleting streams during parsing of next track, so base parser block may have 
    // less number of streams then NvMM block stream count. Error out only when NVMM block have 
    // less streams than parser  streams
    if (pContext->block.StreamCount < pContext->pCurPlaybackCore->totalStreams)
    {
        return NvError_CountMismatch;
    }

    if (!pContext->pCurPlaybackCore->isVideoInitEventSent)
    {
        for (i = 0; i < pContext->pCurPlaybackCore->totalStreams; i++)
        {
            if ((NVMM_ISSTREAMVIDEO(pContext->pCurPlaybackCore->pStreamInfo[i].StreamType) && 
                 pContext->block.pStreams[i]->TransferBufferFromBlock))
            {
                NvMMEventVideoStreamInitInfo streamInitEvent;
                streamInitEvent.structSize = sizeof(NvMMEventVideoStreamInitInfo);
                streamInitEvent.event = NvMMEvent_VideoStreamInit;
                streamInitEvent.resolutionRect.left = 0;
                streamInitEvent.resolutionRect.top = 0;
                streamInitEvent.resolutionRect.right = pContext->pCurPlaybackCore->pStreamInfo[i].NvMMStream_Props.VideoProps.Width;
                streamInitEvent.resolutionRect.bottom = pContext->pCurPlaybackCore->pStreamInfo[i].NvMMStream_Props.VideoProps.Height;
                streamInitEvent.AspectRatioX = pContext->pCurPlaybackCore->pStreamInfo[i].NvMMStream_Props.VideoProps.AspectRatioX;
                streamInitEvent.AspectRatioY = pContext->pCurPlaybackCore->pStreamInfo[i].NvMMStream_Props.VideoProps.AspectRatioY;
                pContext->block.pStreams[i]->TransferBufferFromBlock(
                                                                    pContext->block.pStreams[i]->pOutgoingTBFContext,
                                                                    pContext->block.pStreams[i]->OutgoingStreamIndex,
                                                                    NvMMBufferType_StreamEvent,
                                                                    sizeof(NvMMEventVideoStreamInitInfo),
                                                                    &streamInitEvent);

                pContext->pCurPlaybackCore->isVideoInitEventSent = NV_TRUE;
            }
        }
    }

    TotalStreams = pContext->pCurPlaybackCore->totalStreams;

    // check if EOS reset event has to be sent
    if (pContext->bResetEOS)
    {
        // if EOS is sent reset all the flags
        pContext->bResetEOS = NV_FALSE;
    }

    UlpMode = pCoreContext->pCore->bEnableUlpMode;
    rate = pCoreContext->pCore->GetRate(pCoreContext->pCore);

    // Logic to use the multiple buffers method in an attempt to try and get
    // more linear read patterns
    if (pContext->pCurPlaybackCore->bUsingMultipleStreamReads && 
        !UlpMode)
    {
        NvBool bAtLeastOne = NV_FALSE;
        NvU32 StreamIndex = 0;
        NvMMBuffer *pBufs[32]; // is there a max # of streams?
        NvBool bReEnqueue = NV_FALSE;

        for (i = 0; i < pContext->pCurPlaybackCore->totalStreams; i++)
        {
            pStream = pContext->block.pStreams[i];
            EntriesStreamQ = NvMMQueueGetNumEntries(pStream->BufQ);

            pBufs[i] = NULL;

            // If no buffer available, turn to next stream
            if (!EntriesStreamQ)
            {
                if (!pStream->TransferBufferFromBlock)
                    pCoreContext->bEndOfStream[i] = NV_TRUE;

                if (!pCoreContext->bEndOfStream[i])
                {
                    continue;
                }
            }
            else if ((rate != 1000) &&
                     (NVMM_ISSTREAMAUDIO(pContext->pCurPlaybackCore->pStreamInfo[i].StreamType)))
            {
                if (pContext->isVideoEOS)
                    RetStatus = NvError_ParserEndOfStream;
                if (pContext->pCurPlaybackCore->totalStreams == 1)
                    RetStatus = NvError_NotSupported;
            }
            else if ((NV_FALSE == pStream->bEndOfStream) && (pContext->waitForBufferRelease == NV_FALSE))
            {
                Status = NvMMQueuePeek(pStream->BufQ, &(pBufs[i]));
                if (Status != NvSuccess)
                    return Status;

                if (pBufs[i])
                    bAtLeastOne = NV_TRUE;
            }
        }

        if (!bAtLeastOne || pContext->waitForBufferRelease)
            goto endofdelivery;

        StreamDataSize = 0;
        if (RetStatus == NvSuccess)
        {
            RetStatus = pContext->pCurPlaybackCore->GetNextWorkUnitMultipleBufs(
                                                                               pContext->pCurPlaybackCore,
                                                                               &StreamIndex, 
                                                                               pBufs, 
                                                                               &StreamDataSize, 
                                                                               &DoMoreWork);
        }


        if (RetStatus == NvError_ParserReadFailure)
        {
           SuperparserBlockReportError(pContext->block,  RetStatus, pContext->pTrackInfo.domainName, NULL);
           RetStatus = NvError_ParserEndOfStream;
        }

        if (StreamIndex < pContext->pCurPlaybackCore->totalStreams)
        {
            pStream = pContext->block.pStreams[StreamIndex];
            if (RetStatus == NvSuccess)
            {
                Status = NvMMQueueDeQ(pStream->BufQ, &pStreamBufNext);
                if (Status != NvSuccess)
                    return Status;
                bReEnqueue = NV_TRUE;
            }

            if (pStreamBufNext)
            {
                Status = HandleReturnFromGetNextWU(pContext, pCount, StreamDataSize,
                                                   pStreamBufNext, pStream, StreamIndex,
                                                   RetStatus, bReEnqueue);
            }
            else
                Status = NvError_InvalidAddress;
            if (Status != NvSuccess)
                return Status;
        }
        else if (RetStatus == NvError_ParserEndOfStream)
        {
            for (i = 0; i < pContext->pCurPlaybackCore->totalStreams; i++)
            {
                pStream = pContext->block.pStreams[i];
                EntriesStreamQ = NvMMQueueGetNumEntries(pStream->BufQ);

                if (EntriesStreamQ > 0 && !pCoreContext->bEndOfStream[i])
                {
                    Status = NvMMQueueDeQ(pStream->BufQ, &pStreamBufNext);
                    if (Status != NvSuccess)
                        return Status;
                    bReEnqueue = NV_TRUE;

                    Status = HandleReturnFromGetNextWU(pContext, pCount,
                                                       0,
                                                       pStreamBufNext, pStream,
                                                       i, RetStatus,
                                                       bReEnqueue);
                }
            }
        }

        for (i = 0; i < pContext->pCurPlaybackCore->totalStreams; i++)
        {
            pStream = pContext->block.pStreams[i];
            EntriesStreamQ = NvMMQueueGetNumEntries(pStream->BufQ);

            if ((rate != 1000) && 
                (NVMM_ISSTREAMAUDIO(pContext->pCurPlaybackCore->pStreamInfo[i].StreamType)))
            {
                continue;
            }

            EntriesStreamQ = NvMMQueueGetNumEntries(pStream->BufQ);
            if (EntriesStreamQ)
            {
                bEmptyStreamBufferPending = NV_TRUE;
            }
        }

        if (RetStatus == NvSuccess && 
            StreamIndex == pContext->pCurPlaybackCore->totalStreams)
        {
            bEmptyStreamBufferPending = NV_FALSE;
        }

        goto endofdelivery;
    }

    // Try to produce at most one buffer for each stream
    for (i = 0; i < pContext->pCurPlaybackCore->totalStreams; i++)
    {
        pStream = pContext->block.pStreams[i];
        if (!pStream)
        {
            return NvError_InvalidAddress;
        }

        // Get number of pending empty output buffers
        EntriesStreamQ = NvMMQueueGetNumEntries(pStream->BufQ);

        // If no buffer available, turn to next stream
        if (!EntriesStreamQ)
        {
            // No TBF, assume End-Of-Stream. From a AV content only audio or video stream is being played.
            if (!pStream->TransferBufferFromBlock)
            {
                pCoreContext->bEndOfStream[i] = NV_TRUE;
            }

            continue;
        }

        if ((NV_FALSE == pStream->bEndOfStream) && (pContext->waitForBufferRelease == NV_FALSE))
        {
            // Pop up next stream output buffer
            Status = NvMMQueueDeQ(pStream->BufQ, &pStreamBufNext);
            if (Status != NvSuccess)
            {
                return Status;
            }
            pStreamBufNext->PayloadInfo.BufferFlags = 0;
            pStreamBufNext->pCore = (void*)pContext->pCurPlaybackCore;
            // Populate next stream output bufffer
            // Here 2nd arg is streamIndex. Parser code should correctly identify which stream data to parse.
            StreamDataSize = 0;
            // Audio is muted during FFW/REW.
            if (pContext->pCurPlaybackCore->pStreamInfo == NULL)
            {
                return NvError_InvalidAddress;
            }
            if ((rate != 1000) &&
                (NVMM_ISSTREAMAUDIO(pContext->pCurPlaybackCore->pStreamInfo[i].StreamType)))
            {
                if (pContext->isVideoEOS)
                    RetStatus = NvError_ParserEndOfStream;

                if (pContext->pCurPlaybackCore->totalStreams == 1)
                    RetStatus = NvError_NotSupported;
            }
            else
            {
                if (UlpMode == NV_TRUE)
                {
                    pStreamBufNext->PayloadInfo.BufferFlags |= NvMMBufferFlag_OffsetList;
                    pContext->pCurPlaybackCore->GetMaxOffsets(pContext->pCurPlaybackCore, &MaxOffsetsPerOffsetList);
                    NvmmSetMaxOffsets(pStreamBufNext, MaxOffsetsPerOffsetList);
                    NvmmResetOffsetList(pStreamBufNext);

                    pContext->pCurPlaybackCore->pPipe->GetBaseAddress(pContext->pCurPlaybackCore->cphandle, 
                                                                      &pContext->pCurPlaybackCore->pBufferMgrBaseAddress, 
                                                                      (void **)&pContext->pCurPlaybackCore->vBufferMgrBaseAddress);

                    NvmmSetBaseAddress(pContext->pCurPlaybackCore->pBufferMgrBaseAddress, 
                                       pContext->pCurPlaybackCore->vBufferMgrBaseAddress, 
                                       pStreamBufNext);

                    if (pCoreContext->bPreCacheOffsets == NV_TRUE)
                    {
                        RetStatus = NvMMSuperParserBlockGetNextWorkUnitOffsets(hBlock,
                                                                               pContext->pCurPlaybackCore, 
                                                                               i, 
                                                                               pStreamBufNext, 
                                                                               &StreamDataSize);
                    }
                    else
                    {
                        RetStatus = pContext->pCurPlaybackCore->GetNextWorkUnit(pContext->pCurPlaybackCore,
                                                                                i, 
                                                                                pStreamBufNext, 
                                                                                &StreamDataSize, 
                                                                                &DoMoreWork);
                    }
                }
                else
                {
                    RetStatus = pContext->pCurPlaybackCore->GetNextWorkUnit(pContext->pCurPlaybackCore,
                                                                            i, 
                                                                            pStreamBufNext, 
                                                                            &StreamDataSize, 
                                                                            &DoMoreWork);
                }
                if (UlpMode == NV_TRUE)
                {
                    pCount = 0;
                    NvmmGetNumOfValidOffsets(pStreamBufNext, &pCount);
                    if (pCount)
                    {
                        NvMMOffsetList *pList = (NvMMOffsetList*)pStreamBufNext->Payload.Ref.pMem;
                        pList->pParserCore = (void*)pContext->pCurPlaybackCore;
                    }
                }
            }
            Status = HandleReturnFromGetNextWU(pContext, pCount, StreamDataSize,
                                               pStreamBufNext, pStream, i,
                                               RetStatus, NV_TRUE);
            if (Status != NvSuccess)
                return Status;

            // Skip the check for buffers when rate != 1 and audio stream, 
            // else it'll continually call the worker function
            if ((rate != 1000 ) && 
                (NVMM_ISSTREAMAUDIO(pContext->pCurPlaybackCore->pStreamInfo[i].StreamType)))
            {
                continue;
            }

            EntriesStreamQ = NvMMQueueGetNumEntries(pStream->BufQ);
            if (EntriesStreamQ && RetStatus != NvError_ContentPipeNotReady)
            {
                bEmptyStreamBufferPending = NV_TRUE;
            }
        }
    }


endofdelivery:
    TotalStreams = pContext->pCurPlaybackCore->totalStreams;
    bAllStreamsEnded = NV_TRUE;
    for (j = 0; j < TotalStreams; j++)
    {
        if (pCoreContext->bEndOfStream[j] == NV_FALSE)
        {
            bAllStreamsEnded = NV_FALSE;
            break;
        }
    }
    if (bAllStreamsEnded == NV_TRUE)
    {
        bAllStreamEndEventSent = NV_TRUE;
        for (j = 0; j < TotalStreams; j++)
        {
            if (pCoreContext->bEosEventSent[j] == NV_FALSE)
            {
                bAllStreamEndEventSent = NV_FALSE;
                break;
            }
        }
        if (bAllStreamEndEventSent == NV_TRUE)
        {
            return NvSuccess;
        }

        bEmptyStreamBufferPending = NV_FALSE;
        for (j = 0; j < pContext->pCurPlaybackCore->totalStreams; j++)
        {
            pCurStream = pContext->block.pStreams[j];
            if (pCurStream)
            {
                pCurStream->bEndOfStream = NV_TRUE;
                if (pCurStream->TransferBufferFromBlock)
                {
                    if (pCoreContext->bEosEventSent[j] == NV_FALSE)
                    {
                        NV_LOGGER_PRINT ((NVLOG_TRACK_LIST, NVLOG_DEBUG, "Block -  Sending EOS(3): PlayIndex = %d ", pContext->PlayIndex));
                        NvMMSendEndOfStreamEvent(pCurStream, j, pContext);
                        pCoreContext->bEosEventSent[j] = NV_TRUE;
                    }
                }
                else
                {
                    pCoreContext->bEosEventSent[j] = NV_TRUE;
                }
            }
        }
    }

    // Are more empty buffers are in queue
    if (NV_TRUE == bEmptyStreamBufferPending)
    {
        return NvError_Busy;
    }

    return NvSuccess;
}

static NvError 
NvMMSuperParserBlockSetAttribute(NvMMBlockHandle hBlock, 
                                 NvU32 AttributeType, 
                                 NvU32 SetAttrFlag, 
                                 NvU32 AttributeSize, 
                                 void *pAttribute)
{
    NvError Status, Status1;
    NvMMSuperParserBlockContext* const pContext = hBlock->pContext;
    NvMMParserCoreContext *pParserCoreContext = NULL;
    NvU64 SeekPos = ((NvMMAttrib_ParsePosition*)pAttribute)->Position;
    NvMMErrorDomainDRMInfo info;
    NvMMTrackInfo pTrackInfo;
    pTrackInfo.pPath = NULL;
    Status = Status1 = NvSuccess;

    NvOsMemset(&info, 0 , sizeof(NvMMErrorDomainDRMInfo));
    NvOsMemset(&pTrackInfo, 0 , sizeof(NvMMTrackInfo));
    // Block specific attributes
    switch (AttributeType)
    {
    case NvMMAttribute_Position:
        pParserCoreContext = &pContext->ParserCore;
        Status = SuperParserAdjustParserCoreForSeek(hBlock, pParserCoreContext->pCore, SeekPos);
        pContext->bResetEOS = NV_FALSE;
        break;

    case NvMMAttribute_Rate:
        if (pContext->pCurPlaybackCore)
        {
            NvS32 PlayRate = ((NvMMAttrib_ParseRate*)pAttribute)->Rate;

            if (PlayRate == 1000)
            {
                NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "Set Rate: NORMAL"));
            }
            else if (PlayRate > 1000)
            {
                NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "Set Rate: %dx FF", PlayRate/1000));
            }
            else
            {
                NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_INFO, "Set Rate: %dx REW", -1*PlayRate/1000));
            }
            
            Status = pContext->pCurPlaybackCore->SetRate(pContext->pCurPlaybackCore, PlayRate);
        }
        break;

    case NvMMAttribute_Buffering:
        if (pContext) 
            pContext->bEnableBufferingForStreaming = ((NvMMAttrib_Buffering*)pAttribute)->bEnableBuffering;

        break;
    case NvMMAttribute_UserAgent:
        if (pContext)
        {
            NvMMAttrib_UserAgentStr *pConfig = (NvMMAttrib_UserAgentStr*)pAttribute;
            Status = NvMMSetUserAgentString(pConfig->pUserAgentStr);
            NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG,
                              "NvMMAttribute_UserAgent: Len: %d String:%s ",
                               NvOsStrlen(pConfig->pUserAgentStr), pConfig->pUserAgentStr));

        }
        break;
    case NvMMAttribute_UserAgentProf:
        if (pContext)
        {
            NvMMAttribute_UserAgentProfStr *pConfig = (NvMMAttribute_UserAgentProfStr*)pAttribute;
            Status = NvMMSetUAProfAgentString(pConfig->pUAProfStr);
            NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG,
                              "NvMMAttribute_UserAgentProfStr: Len: %d String:%s ",
                               NvOsStrlen(pConfig->pUAProfStr), pConfig->pUAProfStr));
        }
        break;
    case NvMMAttribute_FileCacheSize:
        {
            NvU32 nSize = ((NvMMAttrib_FileCacheSize*)pAttribute)->nFileCacheSize;
            /* If size set to 1, use default cache configurations */
            if (nSize == 1)
            {
                pContext->bCacheSizeSetByClient = NV_FALSE;
            }
            else
            {
                pContext->MaxCacheSize = nSize;
                pContext->bCacheSizeSetByClient = NV_TRUE;
            }
        }
        break;
    case NvMMAttribute_DisableBuffConfig:
        {
            NvBool flag =((NvMMAttribute_DisableBuffConfigFlag *)pAttribute)->bDisableBuffConfig;
            if ( pContext->pCurPlaybackCore  &&  pContext->pCurPlaybackCore->EnableConfig)
            {
                pContext->pCurPlaybackCore->bDisableBuffConfig = flag;
                pContext->pCurPlaybackCore->EnableConfig( pContext->pCurPlaybackCore);
            }
        }
        break;
    case NvMMAttribute_Mp3EnableFlag:
        {
            NvBool flag =((NvMMAttribute_Mp3EnableConfigFlag *)pAttribute)->bEnableMp3TSflag;

            if ( pContext->pCurPlaybackCore  &&  pContext->pCurPlaybackCore->EnableConfig)
            {

                pContext->pCurPlaybackCore->Mp3Enable = flag;
                pContext->pCurPlaybackCore->EnableConfig( pContext->pCurPlaybackCore);
            }
        }
        break;

    case NvMMAttribute_UlpMode:
        {
            NvBool mode = ((NvMMAttrib_EnableUlpMode*)pAttribute)->bEnableUlpMode;
            // Set the kpimode only if the ulpmode is being set
            if (mode != 0)
            {
                pContext->UlpKpiMode = ((NvMMAttrib_EnableUlpMode*)pAttribute)->ulpKpiMode; 
            }
            if (mode != pContext->block.bEnableUlpMode)
            {
                pContext->block.bEnableUlpMode = mode;
                pContext->UlpKpiMode = ((NvMMAttrib_EnableUlpMode*)pAttribute)->ulpKpiMode;
                pContext->pCurPlaybackCore->bEnableUlpMode = mode;
                pContext->pCurPlaybackCore->ResetUlpMode(pContext->pCurPlaybackCore, pContext->block.bEnableUlpMode); 

                if (!pContext->block.bEnableUlpMode)
                {
                    NvMMSuperParserUlpModeMsg Message;
                    NvOsMemset(&Message, 0 , sizeof(NvMMSuperParserUlpModeMsg));
                    Message.eMessageType = NvMMSuperParserMsgType_UlpMode;
                    Message.StructSize = sizeof(NvMMSuperParserUlpModeMsg);
                    Status = NvOsSemaphoreCreate(&Message.hMessageAckSema, 0);
                    if (Status != NvSuccess)
                        break;
                    Message.bNeedAck = NV_TRUE;
                    Message.bUlpMode = pContext->block.bEnableUlpMode;
                    Message.bUlpKpiMode = NV_FALSE;
                    Status = NvMMQueueEnQ(pContext->hMsgQueue, &Message, 0);
                    if (Status != NvSuccess)
                    {
                        NvOsSemaphoreDestroy(Message.hMessageAckSema);
                        break;
                    }
                    NvOsSemaphoreSignal(pContext->hOffsetThreadSema);
                    NvOsSemaphoreWait(Message.hMessageAckSema);
                    NvOsSemaphoreDestroy(Message.hMessageAckSema);
                }
                else
                {
                    NvMMSuperParserStartParsingMsg Message;
                    Message.bNeedAck = NV_TRUE;
                    Message.bUpdateParseIndex = NV_FALSE;  
                    Message.eMessageType = NvMMSuperParserMsgType_StartParsing;
                    Message.bDeleteCores = NV_FALSE;
                    Message.StructSize = sizeof(NvMMSuperParserStartParsingMsg);
                    Status = NvOsSemaphoreCreate(&Message.hMessageAckSema, 0);
                    if (Status != NvSuccess)
                        break;
                    Message.pParsingStatus = &Message.ParsingStatus;
                    Status = NvMMQueueEnQ(pContext->hMsgQueue, &Message, 0);
                    if (Status != NvSuccess)
                    {
                        NvOsSemaphoreDestroy(Message.hMessageAckSema);
                        break;
                    }
                    NvOsSemaphoreWait(Message.hMessageAckSema);
                    NvOsSemaphoreDestroy(Message.hMessageAckSema);
                }
            }
        }
        break;

    case NvMMAttribute_ReduceVideoBuffers:
        if (pContext->pCurPlaybackCore)
            pContext->pCurPlaybackCore->bReduceVideoBuffers = ((NvMMAttrib_ReduceVideoBuffers*)pAttribute)->bReduceVideoBufs;
        else
            pContext->bReduceVideoBuffers = ((NvMMAttrib_ReduceVideoBuffers*)pAttribute)->bReduceVideoBufs;
        break;

    case NvMMAttribute_FileName:
        {
            NvMMSuperParserStartParsingMsg Message;
            NvMMAttrib_ParseUri *pURI = (NvMMAttrib_ParseUri*)pAttribute;
            NvOsMemset(&Message, 0 , sizeof(NvMMSuperParserStartParsingMsg));
            pContext->pTrackInfo.uSize = NvOsStrlen(pURI->szURI);
            pContext->pTrackInfo.pPath = NvOsAlloc(sizeof(NvU8) * pContext->pTrackInfo.uSize + 1);
            if(pContext->pTrackInfo.pPath == NULL)
            {
                break;
            }
            else
            {
                NvOsMemset(pContext->pTrackInfo.pPath, 0, pContext->pTrackInfo.uSize + 1);
                NvOsStrncpy(pContext->pTrackInfo.pPath, pURI->szURI, pContext->pTrackInfo.uSize);
                pContext->pTrackInfo.trackListIndex=  0;
                pContext->pTrackInfo.eParserCore = pURI->eParserCore;
                Message.bNeedAck = NV_TRUE;
                Message.bUpdateParseIndex = NV_FALSE;  
                Message.eMessageType = NvMMSuperParserMsgType_StartParsing;
                Message.StructSize = sizeof(NvMMSuperParserStartParsingMsg);
                Message.bDeleteCores = NV_TRUE;
                Status = NvOsSemaphoreCreate(&Message.hMessageAckSema, 0);
                if (Status != NvSuccess)
                    break;
                Message.pParsingStatus = &Message.ParsingStatus;
                Status = NvMMQueueEnQ(pContext->hMsgQueue, &Message, 0);
                if (Status != NvSuccess)
                {
                    NvOsSemaphoreDestroy(Message.hMessageAckSema);
                    break;
                }
                NvOsSemaphoreSignal(pContext->hOffsetThreadSema);
                NvOsSemaphoreWait(Message.hMessageAckSema);
                NvOsSemaphoreDestroy(Message.hMessageAckSema);
                Status = NvMMGetParserCore(hBlock, &pContext->pCurPlaybackCore, &pContext->pTrackInfo);
                if (Status != NvSuccess)
                {
                    break;
                }
            }
        }
        break;
    case NvMMAttribute_FileCacheThreshold:
        {
            if((pContext->pCurPlaybackCore) &&
               (pContext->pCurPlaybackCore->pPipe) &&
               (pContext->pCurPlaybackCore->cphandle))
            {
                CP_ConfigThreshold CPThreshold;

                CPThreshold.HighMark = ((NvMMAttrib_FileCacheThreshold *)pAttribute)->nHighMark;
                CPThreshold.LowMark = ((NvMMAttrib_FileCacheThreshold *)pAttribute)->nLowMark;

                Status = pContext->pCurPlaybackCore->pPipe->SetConfig(pContext->pCurPlaybackCore->cphandle, CP_ConfigIndexThreshold, &CPThreshold);
            }
        }
        break;
    }

    if (Status == NvError_ParserEndOfStream )
        Status = NvSuccess;

    if (Status != NvSuccess)
    {
        info.error = Status;
        info.TrackIndex = 0;
        if(pTrackInfo.pPath)
            SuperparserBlockReportError(pContext->block, Status, pTrackInfo.domainName, (void*)&info );
        else
            SuperparserBlockReportError(pContext->block, Status, pContext->pTrackInfo.domainName, (void*)&info );
    }
    if(pContext->pTrackInfo.pPath)
    {
        NvOsFree(pContext->pTrackInfo.pPath);
        pContext->pTrackInfo.pPath = NULL;
    }
    if(pTrackInfo.pPath)
    {
        NvOsFree(pTrackInfo.pPath);
        pTrackInfo.pPath = NULL;
    }

    // Call base class implementation
    // - for already handled AttributeType: to handle SetAttrFlag
    // - for not yet handled AttributeType: possibly base class attribute
    Status1 = NvMMBlockSetAttribute(hBlock, AttributeType, SetAttrFlag, AttributeSize, pAttribute);
    if (Status1 != NvSuccess)
        return Status1;

    return Status;
}

NvError NvMMSuperParserGetCurrentCoreHandle(NvMMSuperParserBlockContext *pContext, NvMMParserCoreHandle *pCore, NvU32 * pIndex)
{
    NvError Status = NvError_ParserCoreNotCreated;
    NvMMParserCoreContext *pCoreContext;

    pCoreContext = &pContext->ParserCore;
    if (pCoreContext->bEmpty == NV_FALSE)
    {
        *pCore = pCoreContext->pCore;
        Status = NvSuccess;
    }

    return Status;
}

NvError 
NvMMSuperParserBlockGetAttribute(NvMMBlockHandle hBlock, 
                                 NvU32 AttributeType, 
                                 NvU32 AttributeSize, 
                                 void *pAttribute)
{
    NvError Status = NvSuccess;
    NvMMSuperParserBlockContext* const pContext = hBlock->pContext;
    NvS32 rate;   
    NvMMParserCoreHandle pCoreHandle;
    NvMMErrorDomainDRMInfo info;

    NvOsMutexLock(pContext->hCoreLock); 

    Status = NvMMSuperParserGetCurrentCoreHandle(pContext, &pCoreHandle, NULL);
    if (Status == NvSuccess)
    {
        Status = pCoreHandle->GetAttribute(pCoreHandle, AttributeType, AttributeSize, pAttribute);                   
    }

    if (Status != NvSuccess)
    {
        NvOsMutexUnlock(pContext->hCoreLock); 
        return Status;
    }

    switch (AttributeType)
    {
    case NvMMAttribute_NoOfStreams:
        if (pCoreHandle)
            Status = pCoreHandle->GetNumberOfStreams(pCoreHandle, (NvU32 *)pAttribute);
        else
            Status = NvError_ParserFailure;                        
        break;

    case NvMMAttribute_StreamInfo:
        if (pCoreHandle)
        {
            NvMMStreamInfo* pCurStreamInfo = *((NvMMStreamInfo**)pAttribute);
            if (pCoreHandle->pStreamInfo)
            {
                NvOsMemcpy(pCurStreamInfo, pCoreHandle->pStreamInfo, sizeof(NvMMStreamInfo) * pCoreHandle->totalStreams);
                Status = NvSuccess;
            }
            else
            {
                Status = pCoreHandle->GetStreamInfo(pCoreHandle, (NvMMStreamInfo**)pAttribute);
            }
        }
        else
            Status = NvError_ParserFailure;                        
        break;

    case NvMMAttribute_Rate:
        if (pCoreHandle)
        {
            rate = pCoreHandle->GetRate (pCoreHandle);
            *((NvS32*)pAttribute)= rate;
        }
        else
            Status = NvError_ParserFailure;                        
        break;

    case NvMMAttribute_Position:
        if (pCoreHandle)
            Status = pCoreHandle->GetPosition(pCoreHandle, &((NvMMAttrib_ParsePosition*)pAttribute)->Position);
        else
            Status = NvError_ParserFailure;                        
        break;

    case NvMMAttribute_FileCacheInfo:
        if((pContext->pCurPlaybackCore) &&
           (pContext->pCurPlaybackCore->pPipe) &&
           (pContext->pCurPlaybackCore->cphandle))
        {
            NvMMAttrib_FileCacheInfo *pCacheInfo = (NvMMAttrib_FileCacheInfo *)pAttribute;
            Status = pContext->pCurPlaybackCore->pPipe->GetPositionEx(pContext->pCurPlaybackCore->cphandle, (CP_PositionInfoType *)pCacheInfo);
        }
        else
            Status = NvError_ParserFailure;                        
        break;

    case NvMMAttribute_FileCacheSize:
        if((pContext->pCurPlaybackCore) &&
           (pContext->pCurPlaybackCore->pPipe) &&
           (pContext->pCurPlaybackCore->cphandle))
        {
            NvMMAttrib_FileCacheSize *pCacheSizeInfo = (NvMMAttrib_FileCacheSize *)pAttribute;
            Status = pContext->pCurPlaybackCore->pPipe->GetConfig(pContext->pCurPlaybackCore->cphandle, CP_ConfigIndexCacheSize, &pCacheSizeInfo->nFileCacheSize);
        }
        else
            Status = NvError_ParserFailure;                        
        break;

    default:
        // Handle unknown attribute types in base class
        Status = NvMMBlockGetAttribute(hBlock, AttributeType, AttributeSize, pAttribute);
        break;

    }
    NvOsMutexUnlock(pContext->hCoreLock); 


    if (Status != NvSuccess)
    {
        info.error = Status;
        info.TrackIndex = 0;
        SuperparserBlockReportError(pContext->block,  Status, pCoreHandle->domainName, (void*)&info );
    }

    return Status;
}

NvError NvMMSuperParserTBTBCallBack(void *pContext, NvU32 StreamIndex, NvMMBuffer* offsetListBuffer)
{
    NvMMSuperParserBlockContext *pSuperParserBlockContext = (NvMMSuperParserBlockContext*)pContext;
    NvMMParserCoreContext *pCoreContext = NULL;
    NvMMParserCoreHandle pCoreForThisBuffer;

    if (offsetListBuffer->PayloadInfo.BufferFlags & NvMMBufferFlag_DRMPlayCount)
    {

        pCoreForThisBuffer = offsetListBuffer->pCore;
        if (pCoreForThisBuffer->eCoreType == NvMMParserCoreType_Mp4Parser)
        {
            pCoreForThisBuffer->commitDrm(pCoreForThisBuffer,StreamIndex);
            goto ExitTBTB;
        }

        /*
         * What a mess, this DRM track has not been played and decoder did not touch it,
         * came back because of flush/Abort mark flags so that DRM context can be deleted 
         * so that we will not encounter max secrets reached error
         */
        if (pSuperParserBlockContext->DrmContextInfo.bEmpty == NV_FALSE)
        {
            if (pSuperParserBlockContext->DrmContextInfo.NvDrmContext == StreamIndex)
            {
                if (offsetListBuffer->BufferID == 1)
                {
                    ((NvError (*)(NvDrmContextHandle))pSuperParserBlockContext->DrmContextInfo.NvDrmUpdateMeteringInfo)((void *)StreamIndex);
                    NV_LOGGER_PRINT ((NVLOG_SUPER_PARSER, NVLOG_DEBUG, "DRM Context Received back TBTB:%x for Metering \n", StreamIndex));
                }
                pSuperParserBlockContext->DrmContextInfo.bMeteringDone = NV_TRUE;
            }
        }

        // Reset pListBuffer->PayloadInfo.BufferFlags & NvMMBufferFlag_DRMPlayCount)
        // so that it doesn't do committing every time
        offsetListBuffer->PayloadInfo.BufferFlags &= ~NvMMBufferFlag_DRMPlayCount;
        goto ExitTBTB;
    }

    if (offsetListBuffer->PayloadInfo.BufferFlags & NvMMBufferFlag_EndOfTrack)
    {
        pCoreForThisBuffer = (NvMMParserCoreHandle)offsetListBuffer->pCore;

        NV_LOGGER_PRINT ((NVLOG_TRACK_LIST, NVLOG_DEBUG, "pCoreForThisBuffer = %x", pCoreForThisBuffer));
        if (pCoreForThisBuffer == NULL)
            goto ExitTBTB;

         pSuperParserBlockContext->ParserCore.bPlayBackDone = NV_TRUE;
         NV_LOGGER_PRINT ((NVLOG_TRACK_LIST, NVLOG_DEBUG, "bPlayBackDone marked for core  = %s", pSuperParserBlockContext->ParserCore.pTrackInfo->pPath));

         if (pSuperParserBlockContext->DrmContextInfo.bEmpty == NV_FALSE)
         {
             if (pSuperParserBlockContext->DrmContextInfo.NvDrmContext == pCoreForThisBuffer->NvDrmContext)
             {
                 pSuperParserBlockContext->DrmContextInfo.bPlayBackDone = NV_TRUE;
             }
         }
         NvOsSemaphoreSignal(pSuperParserBlockContext->block.hBlockEventSema);
    }

    if (offsetListBuffer->PayloadInfo.BufferFlags & NvMMBufferFlag_OffsetList)
    {
        // Get the Parser core for this buffer(current playback core may be different).
        pCoreContext = &pSuperParserBlockContext->ParserCore;
        if (pCoreContext)
        {
            // If offsets are not pre-cached release offsets
            if (pCoreContext->bPreCacheOffsets == NV_FALSE)
            {
                if (pCoreContext->pCore)
                {
                    if (pCoreContext->pCore->eCoreType == NvMMParserCoreType_AsfParser)
                    {
                        pCoreContext->pCore->ReleaseOffsetList(pCoreContext->pCore, offsetListBuffer, StreamIndex);
                        pSuperParserBlockContext->waitForBufferRelease = NV_FALSE;
                        NvOsSemaphoreSignal(pSuperParserBlockContext->block.hBlockEventSema);
                    }
                    else
                    {
                        SuperParserReleaseOffsetList(offsetListBuffer, pSuperParserBlockContext, pCoreContext->pCore);
                    }

                    offsetListBuffer->PayloadInfo.BufferFlags = 0;
                    offsetListBuffer->PayloadInfo.TimeStamp = 0;
                    offsetListBuffer->pCore = NULL;
                }
            }
        }
    }

ExitTBTB:
    offsetListBuffer->Payload.Ref.sizeOfValidDataInBytes = 0;
    offsetListBuffer->Payload.Ref.startOfValidData = 0;

    return NvSuccess;
}

void SuperparserBlockReportError(NvMMBlockContext block, NvError RetType, NvMMErrorEventFromDomain  domainName, void* pErrorInfo)
{
    NvMMBlockErrorInfo BlockError;
    BlockError.structSize = sizeof(NvMMBlockErrorInfo);
    BlockError.event = NvMMEvent_BlockError;
    BlockError.error = RetType;
    BlockError.domainName = domainName;

    if(pErrorInfo)
    {
        BlockError.domainErrorInfo.drmInfo.error = ((NvMMErrorDomainDRMInfo *)pErrorInfo)->error;
        BlockError.domainErrorInfo.drmInfo.TrackIndex = ((NvMMErrorDomainDRMInfo *)pErrorInfo)->TrackIndex;
    }
    NV_LOGGER_PRINT ((NVLOG_TRACK_LIST, NVLOG_ERROR, "SuperparserBlockReportError = %x ", RetType));

    block.SendEvent(block.pSendEventContext, NvMMEvent_BlockError,sizeof(NvMMBlockErrorInfo), &BlockError);
}

static NvError SuperParserReleaseOffsetList(NvMMBuffer* pListBuffer, NvMMSuperParserBlockContext* pContext, NvMMParserCoreHandle pCore) 
{
    NvError Status = NvSuccess;
    NvU32 count = 0;
    NvBool consumed = NV_FALSE;
    NvMMBuffer dataBuff;
    NvU32 i = 0;
    NvMMOffsetList* pList = (NvMMOffsetList*)pListBuffer->Payload.Ref.pMem;
    NvmmGetOffsetListStatus(pListBuffer, &consumed);

    if(pCore)
    {
        if(consumed == NV_TRUE) 
        {
            NvmmGetNumOfOffsets(pListBuffer, &count);
            pList->tableIndex = 0;
            if(pCore->eCoreType == NvMMParserCoreType_Mp4Parser)
            {
                for(i = 0; i < count; i++) 
                {
                    Status = NvmmPopFromOffsetList(pListBuffer, &dataBuff, NV_FALSE);
                    if(Status != NvSuccess) 
                    {
                         goto ReleaseFail;
                    }
                   
                    if( dataBuff.PayloadInfo.BufferFlags & NvMMBufferFlag_CPBuffer)
                    {
                        Status = pCore->pPipe->cpipe.ReleaseReadBuffer(pCore->cphandle, dataBuff.Payload.Ref.pMem);
                        if(Status != NvSuccess) 
                        {
                            goto ReleaseFail;
                        }
                    }
                }
            }
            else
            {
                for(i = 0; i < count; i++) 
                {
                    Status = NvmmPopFromOffsetList(pListBuffer, &dataBuff, NV_FALSE);
                    if(Status != NvSuccess) 
                    {
                        
                        goto ReleaseFail;
                    }
                    Status = pCore->pPipe->cpipe.ReleaseReadBuffer(pCore->cphandle, dataBuff.Payload.Ref.pMem);
                    if(Status != NvSuccess) 
                    {
                        goto ReleaseFail;
                    }
                }
            }
            pContext->waitForBufferRelease = NV_FALSE;
        }
    }

    return NvSuccess;
    ReleaseFail:
    return Status;
}

CPresult NvMMSuperParserCPNotify(void *pClientContext, CP_EVENTTYPE eEvent, CPuint iParam)
{
    NvMMSuperParserBlockContext *pSuperParserBlockContext = (NvMMSuperParserBlockContext*)pClientContext;
    NvOsSemaphoreSignal(pSuperParserBlockContext->block.hBlockEventSema);
    /* TODO: signal one or the other based on which thread is waiting */
    NvOsSemaphoreSignal(pSuperParserBlockContext->hCpWaitSema);
    return NvSuccess;
}

NvBool SuperParserIsUlpSupported(NvMMParserCoreType eCoreParserType, NvMMStreamType eStreamType)
{
    NvBool IsUlpSupported = NV_FALSE;

#if OFFSET_DISABLED
    return IsUlpSupported;
#endif

    switch (eCoreParserType)
    {
    case NvMMParserCoreType_3gppParser:
    case NvMMParserCoreType_Mp4Parser:
        if ((eStreamType == NvMMStreamType_AAC) || 
            (eStreamType == NvMMStreamType_AACSBR))
        {
            IsUlpSupported = NV_TRUE;
        }
        break;
    case NvMMParserCoreType_AsfParser:
        IsUlpSupported = NV_TRUE;
        break;
    case NvMMParserCoreType_AviParser:
        IsUlpSupported = (eStreamType == NvMMStreamType_MP3) ? NV_TRUE : NV_FALSE;
        break;
   case NvMMParserCoreType_MpsParser:
        IsUlpSupported = NV_FALSE;
        break;
    case NvMMParserCoreType_Mp3Parser:
        if (eStreamType == NvMMStreamType_MP2)
        {
            IsUlpSupported = NV_FALSE;
        }
        else
        {
            IsUlpSupported = NV_TRUE;
        }
        break;
    case NvMMParserCoreType_MkvParser:
    default:
        IsUlpSupported = NV_FALSE;
    };
    return IsUlpSupported;
}



