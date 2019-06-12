/*
 * Copyright (c) 2008-2012 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvmm_util.h"
#include "nvmm_basewriterblock.h"
#include "nvmm_3gpwriterblock.h"
#include "nv_3gp_writer.h"
#include "nvmm_contentpipe.h"

//FTYP is 44 bytes
#define SIZE_OF_FTYP 44
#define MAX_QUEUE_LEN 2
#define NVMM_3GPWRITER_MAX_TRACKS 2
// can be set to 1 for a mono encoder
#define MAX_AAC_CHANNELS 6
#define MAX_AMR_CHANNELS 2
#define MAX_BYTES_PER_AMR_CHANNEL 250
#define MAX_BYTES_PER_AAC_CHANNEL 768
#define AACPLUS_ENC_OUT_BUF_SIZE (MAX_BYTES_PER_AAC_CHANNEL * MAX_AAC_CHANNELS)
//capable of writing(after encoding) upto 20 frames to o/p stream
#define AMR_MAX_OUTPUT_BUFFER_SIZE (20 * MAX_BYTES_PER_AMR_CHANNEL* MAX_AMR_CHANNELS)
// min size of encoded data is 1 frame
#define AMR_MIN_OUTPUT_BUFFER_SIZE (1 * MAX_BYTES_PER_AMR_CHANNEL* MAX_AMR_CHANNELS)
#define AMR_MIN_OUTPUT_BUFFERS 1
#define AMR_MAX_OUTPUT_BUFFERS 10
#define VIDEO_ENC_MAX_OUTPUT_BUFFERS 5
#define VIDEO_ENC_MIN_OUTPUT_BUFFERS 3
#define AMR_QCELP_FRAME_DURATION 20
#define MAX_AUDIO_FLAGS 10
#define MIN_RAW_YUV_BUFFER (32*1024)

typedef struct NvMM3GPWriterBufferCacheRec
{
    //Ping Pong buffer
    NvU8 *pCachedPingPongBufferData[MAX_QUEUE_LEN];
    // current pointer index ping or pong 0 for ping and 1 for pong
    NvU32 IndexPingPong;
    NvU32 SizeOfThePingPongBuffer;
    NvU32 CachedPingPongDataSize ;
} NvMM3GPWriterBufferCache;

typedef struct NvMMFileWriteMsgRec
{
    NvU8 *pData;
    NvU32 DataSize;
    NvBool ThreadShutDown;
} NvMMFileWriteMsg;

typedef struct NvMM3GPWriterStreamInfoRec
{
    NvMMWriterStreamConfig StreamConfig;
    //Maximum supported buffer size in bytes
    NvU32 MaxBufferSize;
} NvMM3GPWriterStreamInfo;

// Context for 3gp writer block
typedef struct NvMM3gpWriterBlockContextRec
{
    NvOsSemaphoreHandle PingPongBufferFilledSema;
    //Only used if MDAT thread is slow
    NvOsSemaphoreHandle PingPongBufferConsumedSema;
    NvMMFileWriteMsg PingPongBufferMsg[MAX_QUEUE_LEN];
    NvOsThreadHandle MDATFileWriterThread;
    NvMMQueueHandle PingPongMsgQ;
    NvMM3GPWriterStreamInfo StreamInfo[NVMM_3GPWRITER_MAX_TRACKS];
    NvMM3GPWriterBufferCache WriterBufferCache;
    NvU32 StreamIndex;
    CPhandle CpHandle;
    CP_PIPETYPE_EXTENDED *pPipe;
    // Params for 3GP core library
    NvMM3GpMuxInputParam InputParam;
    NvMM3GpMuxContext Context3GpMux;
    NvMM3GpAVParam SVParam;
    NvMM3GpMuxSplitMode SplitMode;
    NvU8 *p3gpBufferAlloc;
    NvBool NoEmptyPingPongBuffer;
    NvBool Init3gp;
    NvBool AudioPresent;
    NvBool VideoPresent;
    NvBool VideoInit;
    NvBool EndOfMux; 
    NvBool AtomMode32OR64Bit;
    NvU64 CurrentMDATFileSize;
    char *FileURI;
    char *TempFileURI;
    NvU64 AudioFrameCount;
    NvU64 VideoFrameCount;
    NvU64 MaxMDATFileSizeLimit;
    NvBool FileSizeExceeded;
    NvBool TimeLimitExceeded;
    NvBool MDATWriteInProgress;
} NvMM3gpWriterBlockContext;

static void
NvmmMDATFileWriteThread(
    void *Arg)
{
    NvError Status = NvSuccess;
    NvMM3gpWriterBlockContext *p3gpWriterContext = NULL;
    CP_PIPETYPE *pPipeType = NULL;
    NvMMFileWriteMsg pMessage;
    NvBool ShutDown = NV_FALSE;
    NvU64 CurrentWritePosition = 0;

    p3gpWriterContext = (NvMM3gpWriterBlockContext *)Arg;
    pPipeType = &p3gpWriterContext->pPipe->cpipe;

    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "++NvmmMDATFileWriteThread\n"));
    while(NV_FALSE == ShutDown)
    {
        if(0 == NvMMQueueGetNumEntries(p3gpWriterContext->PingPongMsgQ))
            NvOsSemaphoreWait(p3gpWriterContext->PingPongBufferFilledSema);

        if(NvSuccess == NvMMQueueDeQ(p3gpWriterContext->PingPongMsgQ, &pMessage))
        {
            if( pMessage.ThreadShutDown)
            {
                //Shut down the thread
                ShutDown = NV_TRUE;
                NvOsSemaphoreSignal(p3gpWriterContext->PingPongBufferConsumedSema);
                NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "--NvmmMDATFileWriteThread\n"));
                break;
            }
            else
            {
                //Write MDAT data
                p3gpWriterContext->MDATWriteInProgress = NV_TRUE;
                p3gpWriterContext->InputParam.CpStaus = (NvError)pPipeType->Write(p3gpWriterContext->CpHandle,(CPbyte *)(pMessage.pData),pMessage.DataSize);
                p3gpWriterContext->MDATWriteInProgress = NV_FALSE;
                if (p3gpWriterContext->InputParam.CpStaus != NvSuccess)
                {
                     NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_ERROR, "--NvmmMDATFileWriteThread::Write failed bail ou\n"));
                    // Write failed
                    p3gpWriterContext->InputParam.OverflowFlag = NV_TRUE;

                    // Delete the temporary file to create room for writing pending mdat info
                    Status = NvMM3GpMuxRecDelReservedFile(p3gpWriterContext->Context3GpMux);
                    if(NvSuccess == Status)
                    {
                        Status = (NvError)pPipeType->SetPosition(p3gpWriterContext->CpHandle, (CPint)CurrentWritePosition + 0x2C, CP_OriginBegin);
                       if(NvSuccess == Status)
                       {
                           //Once again try to write the data if it fails, it fails
                           Status = (NvError)pPipeType->Write(p3gpWriterContext->CpHandle, (CPbyte *)(pMessage.pData), pMessage.DataSize);
                           if(Status != NvSuccess)
                               NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "--NvmmMDATFileWriteThread:: 1 Filewrite failed second time:recorded file is unusable\n"));
                       }
                       else
                           NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "--NvmmMDATFileWriteThread::SetPosition failed recorded file is unusable\n"));
                    }
                    else
                        NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "--NvmmMDATFileWriteThread::Deleting reserved partition failed\n"));
                }
                CurrentWritePosition += pMessage.DataSize;
                //If holding all the buffers signal
                if(NV_TRUE == p3gpWriterContext->NoEmptyPingPongBuffer)
                    NvOsSemaphoreSignal(p3gpWriterContext->PingPongBufferConsumedSema);
             }
         }
    }
}

static NvError
NvMM3gpWriterBlockGetBufferRequirements(
    NvmmWriterContext WriterContext,
    NvU32 StreamIndex,
    NvU32 *pMinBuffers,
    NvU32 *pMaxBuffers,
    NvU32 *pMinBufferSize,
    NvU32 *pMaxBufferSize)
{
    NvError Status = NvSuccess;
    NvMM3gpWriterBlockContext *pContext = NULL;
    NvMMWriterStreamConfig *streamConfig = NULL;

    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "++NvMM3gpWriterBlockGetBufferRequirements\n"));
    NVMM_CHK_ARG(
        WriterContext &&
        pMinBuffers &&
        pMaxBuffers &&
        pMinBufferSize &&
        pMaxBufferSize);

    pContext = (NvMM3gpWriterBlockContext *)WriterContext;
    streamConfig = &pContext->StreamInfo[StreamIndex].StreamConfig;

    switch (streamConfig->StreamType)
    {
        case NvMMStreamType_AAC:
        case NvMMStreamType_AACSBR:
        case NvMMStreamType_QCELP:
        {
            *pMinBuffers = 1;
            *pMaxBuffers = NVMMSTREAM_MAXBUFFERS;
            *pMinBufferSize = AACPLUS_ENC_OUT_BUF_SIZE;
            *pMaxBufferSize = AACPLUS_ENC_OUT_BUF_SIZE;
            pContext->StreamInfo[StreamIndex].MaxBufferSize = AACPLUS_ENC_OUT_BUF_SIZE;
        }
        break;
        case NvMMStreamType_NAMR:
        case NvMMStreamType_WAMR:
        {
            *pMinBuffers = AMR_MIN_OUTPUT_BUFFERS;
            *pMaxBuffers = AMR_MAX_OUTPUT_BUFFERS;
            *pMinBufferSize = AMR_MIN_OUTPUT_BUFFER_SIZE;
            *pMaxBufferSize = AMR_MAX_OUTPUT_BUFFER_SIZE;
            pContext->StreamInfo[StreamIndex].MaxBufferSize = AMR_MAX_OUTPUT_BUFFER_SIZE;
        }
        break;
        case NvMMStreamType_MPEG4:
        case NvMMStreamType_H263:
        case NvMMStreamType_H264:
        {
            *pMinBuffers = VIDEO_ENC_MIN_OUTPUT_BUFFERS;
            *pMaxBuffers = VIDEO_ENC_MAX_OUTPUT_BUFFERS;
            // Since the  size for enc / compressed data is not guaranteed what it can be. 
            // It is a function of camera quality, noise, movement in scene and a  lot of factors which are not deterministic. 
            // So the maxbuffer size kept same as raw or uncompressed video frame size
            // i.e. (Resolution (width * height) * 3) / 2 considering raw frame to be  YUV 4:2:0
            // Though this size is unlikerly this size is kept as worst case scenario as enc is not deterministic
            *pMinBufferSize = NV_MAX(MIN_RAW_YUV_BUFFER,
                (streamConfig->NvMMWriter_Config.VideoConfig.width * 
                streamConfig->NvMMWriter_Config.VideoConfig.height*3)/2);
            //min & max both are same
            *pMaxBufferSize = *pMinBufferSize;
            pContext->StreamInfo[StreamIndex].MaxBufferSize = *pMaxBufferSize;
        }
        break;
        default:
            Status = NvError_WriterUnsupportedStream;
        break;
    }

cleanup:
    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "--NvMM3gpWriterBlockGetBufferRequirements\n"));
    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_ERROR, "--NvMM3gpWriterBlockGetBufferRequirements ::Status ->%x \n",Status));
    return Status;
}

static NvError
NvMM3gpWriterSetStreamConfig(
    NvmmWriterContext WriterContext,
    NvMMWriterStreamConfig *pConfig)
{
    NvError Status = NvSuccess;
    NvMM3gpWriterBlockContext* pContext = NULL;
    NvMMMP4WriterAudioConfig *AudConfig = NULL;
    NvMMMP4WriterVideoConfig *VidConfig = NULL;
    NvMMStreamType StreamType = NvMMStreamType_OTHER;

    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "++NvMM3gpWriterSetStreamConfig \n"));
    NVMM_CHK_ARG(WriterContext && pConfig);
    pContext = (NvMM3gpWriterBlockContext *)WriterContext;
    NVMM_CHK_ARG(!(pContext->StreamIndex >= NVMM_3GPWRITER_MAX_TRACKS));

    NvOsMemcpy(
        &pContext->StreamInfo[pContext->StreamIndex].StreamConfig ,
        pConfig,
        sizeof(NvMMWriterStreamConfig));
    StreamType = pContext->StreamInfo[pContext->StreamIndex].StreamConfig.StreamType;

    //Video config's
    if(NvMMStreamType_MPEG4 == StreamType ||
        NvMMStreamType_H263 == StreamType ||
        NvMMStreamType_H264 == StreamType )
    {
        pContext->VideoPresent = NV_TRUE;
        pContext->InputParam.ZeroVideoDataFlag = 1;
        VidConfig = &pContext->StreamInfo[pContext->StreamIndex].StreamConfig.NvMMWriter_Config.VideoConfig;
        pContext->InputParam.VOPWidth = VidConfig->width;
        pContext->InputParam.VOPHeight = VidConfig->height;
        pContext->InputParam.VOPFrameRate = VidConfig->frameRate;
        pContext->InputParam.Profile = VidConfig->profile;
        pContext->InputParam.Level = VidConfig->level;
        pContext->InputParam.VideoBitRate = pContext->StreamInfo[pContext->StreamIndex].StreamConfig.avg_bitrate;
        pContext->InputParam.MaxVideoBitRate = pContext->StreamInfo[pContext->StreamIndex].StreamConfig.max_bitrate;
    }
    if(NvMMStreamType_MPEG4 == StreamType)
        pContext->InputParam.Mp4H263Flag = 0;
    else if(NvMMStreamType_H263 == StreamType)
        pContext->InputParam.Mp4H263Flag = 1;
    else if(NvMMStreamType_H264 == StreamType)
        pContext->InputParam.Mp4H263Flag = 2;

    //Audio config's
    if(NvMMStreamType_NAMR == StreamType ||
        NvMMStreamType_WAMR == StreamType ||
        NvMMStreamType_QCELP == StreamType)
    {
        pContext->AudioPresent = NV_TRUE;
        pContext->InputParam.ZeroSpeechDataFlag= 1;
        pContext->InputParam.FrameDuration = AMR_QCELP_FRAME_DURATION;
     }
    if(NvMMStreamType_NAMR == StreamType)
    {
        pContext->InputParam.SpeechAudioFlag = 0;
        AudConfig = &pContext->StreamInfo[pContext->StreamIndex].StreamConfig.NvMMWriter_Config.AudioConfig;
        if(NvMM_NBAMR_EncodeType_IF1 == AudConfig->NvMMWriter_AudConfig.AMRConfig.encodeType)
            pContext->InputParam.IsAMRIF1 = NV_TRUE;
        else if(NvMM_NBAMR_EncodeType_IF2 == AudConfig->NvMMWriter_AudConfig.AMRConfig.encodeType)
            pContext->InputParam.IsAMRIF2 = NV_TRUE;
    }
    else if(NvMMStreamType_WAMR == StreamType)
        pContext->InputParam.SpeechAudioFlag = 1;
    else if(NvMMStreamType_QCELP == StreamType)
        pContext->InputParam.SpeechAudioFlag = 4;

    if(NvMMStreamType_AAC == StreamType ||
        NvMMStreamType_AACSBR == StreamType)
    {
        pContext->InputParam.SpeechAudioFlag = 2;
        AudConfig = &pContext->StreamInfo[pContext->StreamIndex].StreamConfig.NvMMWriter_Config.AudioConfig;
        pContext->InputParam.AacObjectType = AudConfig->NvMMWriter_AudConfig.AACAudioConfig.aacObjectType;
        pContext->InputParam.AacSamplingFreqIndex = AudConfig->NvMMWriter_AudConfig.AACAudioConfig.aacSamplingFreqIndex;
        pContext->InputParam.AacChannelNumber = AudConfig->NvMMWriter_AudConfig.AACAudioConfig.aacChannelNumber;
        pContext->InputParam.AacAvgBitRate = pContext->StreamInfo[pContext->StreamIndex].StreamConfig.avg_bitrate;
        pContext->InputParam.AacMaxBitRate = pContext->StreamInfo[pContext->StreamIndex].StreamConfig.max_bitrate;
        pContext->AudioPresent = NV_TRUE;
        pContext->InputParam.ZeroSpeechDataFlag= 1;
        // Take given time-stamps
        pContext->InputParam.FrameDuration = 0;
    }

    if(NvMMStreamType_OTHER == StreamType)
    {
        NVMM_CHK_ERR(NvError_WriterUnsupportedStream);
    }
    if(pContext->StreamInfo[pContext->StreamIndex].StreamConfig.setFrames)
    {
        if(NVMM_ISSTREAMVIDEO(pContext->StreamInfo[pContext->StreamIndex].StreamConfig.StreamType))
            pContext->InputParam.MaxVideoFrames = pContext->StreamInfo[pContext->StreamIndex].StreamConfig.numberOfFrames;
        else
            pContext->InputParam.MaxAudioFrames = pContext->StreamInfo[pContext->StreamIndex].StreamConfig.numberOfFrames;
    }
    pContext->StreamIndex++;

    cleanup:
    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "--NvMM3gpWriterSetStreamConfig \n"));
    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_ERROR, "--NvMM3gpWriterSetStreamConfig ->%x\n",Status));
    return Status;
}

static NvError
NvMM3gpWriterInit(
    NvmmWriterContext WriterContext,
    NvMMBuffer *pBuffer)
{
    NvError Status = NvSuccess;
    NvMM3gpWriterBlockContext* pContext = NULL;
    CP_PIPETYPE *pPipeType = NULL;
    NvU32 Offset = 0;
    NvU8 *pTempBuffer = NULL;
    NvU32 TempBufferSize = 0;
    NvU32 i = 0;

    NVMM_CHK_ARG(WriterContext && pBuffer);
    pContext = (NvMM3gpWriterBlockContext *)WriterContext;
    NVMM_CHK_MEM(pContext->FileURI);

    if(NULL == pContext->p3gpBufferAlloc)
    {
        pContext->p3gpBufferAlloc = NvOsAlloc(SIZE_OF_FTYP * sizeof(NvU8));
        NVMM_CHK_MEM(pContext->p3gpBufferAlloc);
        NvOsMemset(pContext->p3gpBufferAlloc, 0, (SIZE_OF_FTYP * sizeof(NvU8)) );
    }
    pTempBuffer = pContext->p3gpBufferAlloc;
    pPipeType = &pContext->pPipe->cpipe;

    NVMM_CHK_ERR((NvError)pPipeType->Open(&pContext->CpHandle,pContext->FileURI,CP_AccessWrite));

    if(pContext->AudioPresent && pContext->VideoPresent)
        pContext->InputParam.AudioVideoFlag = NvMM_AV_AUDIO_VIDEO_BOTH;
    else if(pContext->AudioPresent && !(pContext->VideoPresent))
        pContext->InputParam.AudioVideoFlag = NvMM_AV_AUDIO_PRESENT;
    else
        pContext->InputParam.AudioVideoFlag = NvMM_AV_VIDEO_PRESENT; 

    //Allocate cached buffer;Double of Maxbuffer for video + audio
    for(i = 0; i< pContext->StreamIndex ;i++)
        pContext->WriterBufferCache.SizeOfThePingPongBuffer += (pContext->StreamInfo[i].MaxBufferSize * 2);

    //Check if the  size is 2k aligned
    TempBufferSize = pContext->WriterBufferCache.SizeOfThePingPongBuffer % OPTIMAL_BUFFER_ALIGN;
    if(TempBufferSize != 0)
        pContext->WriterBufferCache.SizeOfThePingPongBuffer += OPTIMAL_BUFFER_ALIGN - TempBufferSize;

    //* Two buffers: Ping + Pong
    for(i =0; i<MAX_QUEUE_LEN;i++)
    {
        pContext->WriterBufferCache.pCachedPingPongBufferData[i] = NvOsAlloc(pContext->WriterBufferCache.SizeOfThePingPongBuffer);
        NVMM_CHK_MEM(pContext->WriterBufferCache.pCachedPingPongBufferData[i]);
    }

    // Initialize 3GP Writer
    NVMM_CHK_ERR(NvMM3GpMuxRec_3gp2WriteInit(
        &(pContext->InputParam),
        pTempBuffer,
        &Offset, &(pContext->Context3GpMux),
        (char *)pContext->FileURI,
        (char *)pContext->TempFileURI,
         pContext->AtomMode32OR64Bit));

    NVMM_CHK_ERR((NvError)pPipeType->Write(
        pContext->CpHandle,
        (CPbyte *)(pTempBuffer),
        (CPuint)(Offset )));

cleanup:
    if(Status != NvSuccess)
    {
        if(pContext->CpHandle)
        {
            pContext->pPipe->cpipe.Close(pContext->CpHandle);
            pContext->CpHandle = 0;
        }
    }
    return Status ;
}

static NvError
NvMM3gpCloseTrack
    (NvmmWriterContext WriterContext)
{
    NvError Status = NvSuccess;
    NvMM3gpWriterBlockContext* pContext = NULL;
    CP_PIPETYPE *pPipeType = NULL;
    NvU32 i = 0;
    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "++NvMM3gpCloseTrack \n"));
    NVMM_CHK_ARG(WriterContext);
    pContext = (NvMM3gpWriterBlockContext *)WriterContext;
    pPipeType = &pContext->pPipe->cpipe;

    // Delete the temporary file to create room for writing pending mdat info
    (void)NvMM3GpMuxRecDelReservedFile(pContext->Context3GpMux);

    // Wait for the buffers to finish in normal case
    while(NvMMQueueGetNumEntries(pContext->PingPongMsgQ) ||
        NV_TRUE == pContext->MDATWriteInProgress)
    {
        //Wait until it completes writing the data 
        NvOsSleepMS(100);
    }

    //Write the remaining MDAT
    if(pContext->WriterBufferCache.CachedPingPongDataSize )
    {
        (void)NvMM3GpMuxMuxWriteDataFromBuffer(
        pContext->WriterBufferCache.pCachedPingPongBufferData[pContext->WriterBufferCache.IndexPingPong],
        pContext->WriterBufferCache.CachedPingPongDataSize, 
        pPipeType,
        pContext->CpHandle);
    }
    pContext->InputParam.OverflowFlag = NV_FALSE;

    if (pContext->InputParam.AudioVideoFlag & NvMM_AV_VIDEO_PRESENT)
       pContext->InputParam.ZeroVideoDataFlag = 1;

    if (pContext->InputParam.AudioVideoFlag & NvMM_AV_AUDIO_PRESENT)
        pContext->InputParam.ZeroSpeechDataFlag = 1;

     // Delete MDAT ping and pong buffers before calling Rec_3gp2WriteClose
     // so that this memory can be resused
     for(i =0; i< MAX_QUEUE_LEN ;i++)
    {
        if(pContext->WriterBufferCache.pCachedPingPongBufferData[i])
        {
            NvOsFree(pContext->WriterBufferCache.pCachedPingPongBufferData[i]);
            pContext->WriterBufferCache.pCachedPingPongBufferData[i] = NULL;
        }
    }
    if(pContext->Init3gp)
    {
        pContext->Init3gp = NV_FALSE;
       //Not checking the status here as CpHandle needs to be freed after this
        (void)NvMM3GpMuxRecWriteClose(
            &pContext->InputParam,
             pContext->CpHandle,
             pPipeType,
             pContext->Context3GpMux);

        if(pContext->CpHandle)
        {
             (void)pContext->pPipe->cpipe.Close(pContext->CpHandle);
             pContext->CpHandle = 0;
        }
    }
    cleanup:
    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "--NvMM3gpCloseTrack \n"));
    return Status;
}

static NvError
NvMM3gpClosePrevTrack(
    NvmmWriterContext WriterContext)
{
    NvError Status = NvSuccess;
    NvMM3gpWriterBlockContext* pContext = NULL;

    NVMM_CHK_ARG(WriterContext);
    pContext = (NvMM3gpWriterBlockContext *)WriterContext;

    (void)NvMM3gpCloseTrack(WriterContext);
    pContext->Init3gp = NV_FALSE;
    pContext->EndOfMux = NV_FALSE;
    pContext->WriterBufferCache.CachedPingPongDataSize = 0;
    pContext->AudioFrameCount = 0;
    pContext->VideoFrameCount = 0;
    pContext->CurrentMDATFileSize =0;
    pContext->SplitMode.TsVideoOffset = 0;
    pContext->SplitMode.TsAudioOffset = 0;
    pContext->WriterBufferCache.SizeOfThePingPongBuffer = 0;

cleanup:
    return Status;
}

static NvError
NvMM3gpWriterCachedDataWrite(
    NvMM3gpWriterBlockContext* pContext,
    NvU8 * pData,
    NvU32 DataSize)
{
    NvError Status = NvSuccess;
    NvU32 CarryForwardData = 0;
    NvU32 ActualDataToWrite = 0;
    NvMM3GPWriterBufferCache * CachedBlob = NULL;
    NvU8 *pPingPongBuffer = NULL;

    NVMM_CHK_ARG(pContext && pData);

    CachedBlob = &pContext->WriterBufferCache;
    pPingPongBuffer = CachedBlob->pCachedPingPongBufferData[
        pContext->WriterBufferCache.IndexPingPong];

    if((CachedBlob->CachedPingPongDataSize + DataSize) > CachedBlob->SizeOfThePingPongBuffer)
    {
        CarryForwardData = CachedBlob->CachedPingPongDataSize % OPTIMAL_BUFFER_ALIGN;
        ActualDataToWrite = CachedBlob->CachedPingPongDataSize - CarryForwardData;

        pContext->PingPongBufferMsg[pContext->WriterBufferCache.IndexPingPong].pData = pPingPongBuffer;
        pContext->PingPongBufferMsg[pContext->WriterBufferCache.IndexPingPong].DataSize = ActualDataToWrite;
        pContext->PingPongBufferMsg[pContext->WriterBufferCache.IndexPingPong].ThreadShutDown = NV_FALSE;

        //Enqueue the Msq for writer thread
        NVMM_CHK_ERR(NvMMQueueEnQ(
            pContext->PingPongMsgQ,
            &pContext->PingPongBufferMsg[pContext->WriterBufferCache.IndexPingPong],
            0));

        if(MAX_QUEUE_LEN == NvMMQueueGetNumEntries(pContext->PingPongMsgQ))
        {
           //Since we don't have buffer left wait here
            pContext->NoEmptyPingPongBuffer = NV_TRUE;
            NvOsSemaphoreWait(pContext->PingPongBufferConsumedSema);
            pContext->NoEmptyPingPongBuffer = NV_FALSE;
        }
        pContext->WriterBufferCache.IndexPingPong = 
            (1 == pContext->WriterBufferCache.IndexPingPong) ? 0 : 1;
        CachedBlob->CachedPingPongDataSize = 0;

        //Signal writer thread
        NvOsSemaphoreSignal(pContext->PingPongBufferFilledSema);
        if (CarryForwardData)
        {
            //If any data needs to copy the data from previous buffer then do it
            NvOsMemcpy(
                CachedBlob->pCachedPingPongBufferData[pContext->WriterBufferCache.IndexPingPong],
                (pPingPongBuffer + ActualDataToWrite),
                CarryForwardData);

           CachedBlob->CachedPingPongDataSize += CarryForwardData ;
        }
        pPingPongBuffer = CachedBlob->pCachedPingPongBufferData[
            pContext->WriterBufferCache.IndexPingPong];
    }

     //Fill the ping pong buffer
    NvOsMemcpy((pPingPongBuffer + CachedBlob->CachedPingPongDataSize),pData, DataSize);
     CachedBlob->CachedPingPongDataSize += DataSize;

    cleanup:
    return Status;
}
static NvError
NvMM3gpWriterAddMediaSample(
    NvmmWriterContext WriterContext,
    NvU32 streamIndex,
    NvMMBuffer *pBuffer)
{
    NvError Status = NvSuccess;
    NvMM3gpWriterBlockContext* pContext = NULL;
    NvU32 Offset = 0;
    NvU8 *pOutputBuffer = NULL;
    Nv3gpWriter *gp3GPPMuxStatus = NULL;
    NvU8 *pData = NULL;
    NvU32 UlSize = 0;
    NvU64 TimeStamp100ns = 0;

    NVMM_CHK_ARG(WriterContext && pBuffer);
    pContext = (NvMM3gpWriterBlockContext *)WriterContext;
    pData = (NvU8 *)pBuffer->Payload.Ref.pMem;
    UlSize = pBuffer->Payload.Ref.sizeOfValidDataInBytes;
    TimeStamp100ns = pBuffer->PayloadInfo.TimeStamp;

    // In split TRack mode close the previous track only when I Frame for 
    //current  track comes. Until then write the non I frames into previous file 
    if((NV_TRUE == pBuffer->PayloadInfo.BufferMetaData.VideoEncMetadata.KeyFrame)
        && (NV_TRUE == pContext->SplitMode.SplitFlag))
    {
        NVMM_CHK_ERR(NvMM3gpClosePrevTrack(WriterContext));
    }

    if (pContext->InputParam.MaxTrakDurationLimit > 0)
    {
        if ( (pContext->InputParam.bAudioDurationLimit &&
            (!pContext->VideoPresent) ) ||
            (pContext->InputParam.bVideoDurationLimit &&
            (!pContext->AudioPresent)) )
        {
            pContext->TimeLimitExceeded = NV_TRUE;
            pContext->EndOfMux = NV_TRUE;
        }

        if (pContext->VideoPresent && pContext->AudioPresent)
        {
            if(pContext->InputParam.bAudioDurationLimit &&
                pContext->InputParam.bVideoDurationLimit)
            {
                pContext->TimeLimitExceeded = NV_TRUE;
                pContext->EndOfMux = NV_TRUE;
            }
            else
            {
                if ((NVMM_ISSTREAMAUDIO(
                    pContext->StreamInfo[streamIndex].StreamConfig.StreamType)
                    && pContext->InputParam.bAudioDurationLimit)
                    || (NVMM_ISSTREAMVIDEO(
                    pContext->StreamInfo[streamIndex].StreamConfig.StreamType)
                    && pContext->InputParam.bVideoDurationLimit))
                    return NvSuccess;
            }
        }
    }

    NVMM_CHK_ARG(!pContext->InputParam.OverflowFlag);
    NVMM_CHK_ARG(!pContext->EndOfMux);

    if(NV_FALSE == pContext->VideoInit &&
        NVMM_ISSTREAMVIDEO(pContext->StreamInfo[streamIndex].StreamConfig.StreamType))
    {
        NvU32 AllocSize;
        if(pContext->p3gpBufferAlloc)
           NvOsFree(pContext->p3gpBufferAlloc);

        AllocSize = (UlSize >= SIZE_OF_FTYP) ? UlSize : SIZE_OF_FTYP;
        pContext->p3gpBufferAlloc = NvOsAlloc( AllocSize * sizeof(NvU8));
        NVMM_CHK_MEM (pContext->p3gpBufferAlloc);
        NvOsMemset(pContext->p3gpBufferAlloc, 0, (AllocSize * sizeof(NvU8)) );
        pOutputBuffer = pContext->p3gpBufferAlloc;
        pContext->VideoInit = NV_TRUE;
     }
    else
        pOutputBuffer = pData;

    if(NV_FALSE == pContext->Init3gp)
    {
        NVMM_CHK_ERR(NvMM3gpWriterInit(WriterContext,pBuffer));
        pContext->Init3gp = NV_TRUE;
    }
    NvOsMemset((void *)&pContext->SVParam, 0, sizeof(NvMM3GpAVParam));

    switch (pContext->StreamInfo[streamIndex].StreamConfig.StreamType)
    {
        case NvMMStreamType_MPEG4:
        case NvMMStreamType_H263:
        case NvMMStreamType_H264:
        {
             // Write data into 3GP stream
            pContext->SVParam.FVideoSyncStatus = 1;
            pContext->SVParam.VideoDataSize = UlSize;// video size
            pContext->SVParam.pVideo = pData;

            pContext->InputParam.ZeroVideoDataFlag = 1;
            pContext->InputParam.ZeroSpeechDataFlag = 0; 
            pContext->InputParam.IsSyncFrame = pBuffer->PayloadInfo.BufferMetaData.VideoEncMetadata.KeyFrame;
            TimeStamp100ns = pBuffer->PayloadInfo.TimeStamp = pBuffer->PayloadInfo.TimeStamp- pContext->SplitMode.TsVideoOffset;

            if((NV_TRUE == pBuffer->PayloadInfo.BufferMetaData.VideoEncMetadata.KeyFrame)
                && (NV_TRUE == pContext->SplitMode.SplitFlag))
            {
                pContext->SplitMode.TsVideoOffset = pBuffer->PayloadInfo.TimeStamp;
                TimeStamp100ns = pBuffer->PayloadInfo.TimeStamp = 0;
                pContext->SplitMode.ResetAudioTS = NV_TRUE;
            }
            pContext->VideoFrameCount++;
            pContext->SVParam.VideoTS = TimeStamp100ns;
        }
        break;
        case NvMMStreamType_NAMR:
        case NvMMStreamType_WAMR:
        case NvMMStreamType_AAC:
        case NvMMStreamType_AACSBR:
        case NvMMStreamType_QCELP:
        {
            // Write data into 3GP stream
            pContext->SVParam.SpeechDataSize = UlSize;
            pContext->SVParam.pSpeech = pData;

            pContext->InputParam.ZeroVideoDataFlag = 0;
            pContext->InputParam.ZeroSpeechDataFlag = 1;
            pContext->AudioFrameCount++;
            TimeStamp100ns = pBuffer->PayloadInfo.TimeStamp -= pContext->SplitMode.TsAudioOffset;
            if(NV_TRUE == pContext->SplitMode.ResetAudioTS)
            {
                pContext->SplitMode.TsAudioOffset = pBuffer->PayloadInfo.TimeStamp;
                TimeStamp100ns = pBuffer->PayloadInfo.TimeStamp = 0;
                pContext->SplitMode.ResetAudioTS = 0;
            }
            pContext->SVParam.AudioTS = TimeStamp100ns;
        }
        break;
        default:
        {
            NVMM_CHK_ERR(NvError_WriterUnsupportedStream);
        }
        break;
    }

    NVMM_CHK_ERR(NvMM3GpMuxRecWriteRun(
        &pContext->SplitMode,
        &pContext->SVParam,
        &pContext->InputParam,
        pOutputBuffer,
        &Offset,
        pContext->Context3GpMux));

    gp3GPPMuxStatus = (Nv3gpWriter *)pContext->Context3GpMux.pIntMem;
    NVMM_CHK_MEM(gp3GPPMuxStatus);
    pContext->CurrentMDATFileSize += Offset;

    if((pContext->CurrentMDATFileSize +
            gp3GPPMuxStatus->HeadSize +
            gp3GPPMuxStatus->VideoBoxesSize +
            gp3GPPMuxStatus->SpeechBoxesSize ) >=
            pContext->MaxMDATFileSizeLimit)
    {
        gp3GPPMuxStatus->MuxMDATBox.Box.BoxSize-= Offset;
        pContext->FileSizeExceeded = NV_TRUE;
        pContext->EndOfMux = NV_TRUE;
    }
    else
        NVMM_CHK_ERR(NvMM3gpWriterCachedDataWrite(pContext,pOutputBuffer, Offset) );

    cleanup:
    if(NvSuccess != Status)
        pContext->EndOfMux = NV_TRUE;
    if (pContext->InputParam.OverflowFlag)
        Status = pContext->InputParam.CpStaus;
    if (pContext->FileSizeExceeded)
        Status = NvError_WriterFileWriteLimitExceeded;
    if (pContext->TimeLimitExceeded)
        Status = NvError_WriterTimeLimitExceeded;
    return Status;
}

static NvError
NvMM3gpCloseWriter
    (NvmmWriterContext WriterContext)
{
    NvError Status = NvSuccess;
    NvMM3GpMuxInpUserData *pInputUserData = NULL;
    NvMM3gpWriterBlockContext* pContext = NULL;
    NvMMFileWriteMsg Message;
    NvU32 i = 0;

    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "++NvMM3gpCloseWriter \n"));
    NVMM_CHK_ARG(WriterContext);
    pContext = (NvMM3gpWriterBlockContext *)WriterContext;

    //Ignoring teh status here as need to fre below resources
    (void)NvMM3gpCloseTrack(WriterContext);

    NvOsMemset(&Message,0, sizeof(NvMMFileWriteMsg));
    Message.ThreadShutDown = NV_TRUE;
    if(pContext->PingPongMsgQ)
        (void)NvMMQueueEnQ(pContext->PingPongMsgQ, &Message, 0);
    if(pContext->PingPongBufferFilledSema)
        NvOsSemaphoreSignal(pContext->PingPongBufferFilledSema);
    //Stop this thread
    if(pContext->PingPongBufferConsumedSema)
        NvOsSemaphoreWait(pContext->PingPongBufferConsumedSema);
    if(pContext->MDATFileWriterThread)
        NvOsThreadJoin (pContext->MDATFileWriterThread);

    if(pContext->PingPongBufferFilledSema)
        NvOsSemaphoreDestroy(pContext->PingPongBufferFilledSema);
    if(pContext->PingPongBufferConsumedSema)
        NvOsSemaphoreDestroy(pContext->PingPongBufferConsumedSema);
    if(pContext->PingPongMsgQ)
        NvMMQueueDestroy(&pContext->PingPongMsgQ);

    if(pContext->SplitMode.ReInitBuffer)
        NvOsFree(pContext->SplitMode.ReInitBuffer);

    if(pContext->p3gpBufferAlloc)
        NvOsFree(pContext->p3gpBufferAlloc);

    for(i=0; i < TOTAL_USER_DATA_BOXES;i++)
    {
        pInputUserData = &pContext->InputParam.InpUserData[i];
        if(pInputUserData->pUserData)
            NvOsFree(pInputUserData->pUserData);
        pInputUserData->boxPresent  = NV_FALSE;
    }

    if(pContext->FileURI)
        NvOsFree(pContext->FileURI);
    if(pContext->TempFileURI)
        NvOsFree(pContext->TempFileURI);

        NvOsFree(pContext);

    cleanup:
    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "--NvMM3gpCloseWriter \n"));
    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_ERROR, "--NvMM3gpCloseWriter->%x \n",Status));
    return Status;
}

static NvError
NvMM3gpWriterSetUserData(
    NvmmWriterContext WriterContext,
    NvMMWriteUserDataConfig *pConfig)
{
    NvError Status = NvSuccess;
    NvMM3gpWriterBlockContext* pContext = NULL;
    NvMM3GpMuxInpUserData *pInputUserData = NULL;

    NVMM_CHK_ARG(WriterContext && pConfig && pConfig->pUserData);
    pContext = (NvMM3gpWriterBlockContext *)WriterContext;

    if(pConfig->dataType < TOTAL_USER_DATA_BOXES)
        pInputUserData = &pContext->InputParam.InpUserData[pConfig->dataType];
    else
    {
        NVMM_CHK_ERR(NvError_WriterUnsupportedUserData);
    }

    pInputUserData->boxPresent = NV_TRUE;
    // Copy the box type into input param structure 
    NvOsMemcpy(pInputUserData->usrBoxType, pConfig->usrBoxType, NVMM_MAX_ATOM_SIZE);

    if(pInputUserData->pUserData && (pConfig->sizeOfUserData > pInputUserData->sizeOfUserData))
        NvOsFree(pInputUserData->pUserData);
    else
    {
        // Allocate buffer within input param structure and use this to copy user data sent from client
        pInputUserData->pUserData = NvOsAlloc(pConfig->sizeOfUserData * sizeof(NvU8));
        NVMM_CHK_MEM(pInputUserData->pUserData);
    }
   // copy size of user data from config structure to input param structure
    pInputUserData->sizeOfUserData = pConfig->sizeOfUserData;
    NvOsMemcpy(pInputUserData->pUserData, pConfig->pUserData, pConfig->sizeOfUserData);

    cleanup: 
    return Status;
}

static NvError
NvMM3gpStoreFileUri(
    char **ExistingFileUri,
    char *NewFileUri)
{
    NvError Status = NvSuccess;

    if(!(*ExistingFileUri) ||
        (NvOsStrlen(NewFileUri) >
         NvOsStrlen(*ExistingFileUri)))
    {
        if(*ExistingFileUri)
            NvOsFree(*ExistingFileUri);
        *ExistingFileUri =(char *) NvOsAlloc(NvOsStrlen(NewFileUri) + 1);
        NVMM_CHK_MEM(*ExistingFileUri);
    }
    NvOsStrncpy(*ExistingFileUri, NewFileUri, NvOsStrlen(NewFileUri) + 1);

cleanup:
    return Status;
}

static NvError
NvMM3gpGetAttribute(
    NvmmWriterContext WriterContext,
    NvU32 AttributeType,
    void *pAttribute)
{
    NvError Status = NvSuccess;
    NvMM3gpWriterBlockContext *p3gpWriterContext = NULL;

    NVMM_CHK_ARG(WriterContext && pAttribute);
    p3gpWriterContext = (NvMM3gpWriterBlockContext *)WriterContext;
    switch (AttributeType)
    {
        case NvMMWriterAttribute_FrameCount:
        {
            ((NvMMWriterAttrib_FrameCount*)pAttribute)->audioFrameCount = 
                p3gpWriterContext->AudioFrameCount;
            ((NvMMWriterAttrib_FrameCount*)pAttribute)->videoFrameCount = 
                p3gpWriterContext->VideoFrameCount;
        }
        break;
        default:
        break;
    }
cleanup:
    return Status;
}

static NvError
NvMM3gpSetAttribute(
    NvmmWriterContext WriterContext,
    NvU32 AttributeType,
    void *pAttribute)
{
    NvError Status = NvSuccess;
    NvMM3gpWriterBlockContext *p3gpWriterContext = NULL;
    NvString Filepath;
    NvU32 FileExtLen = 0;

    NVMM_CHK_ARG(WriterContext && pAttribute);
    p3gpWriterContext = (NvMM3gpWriterBlockContext *)WriterContext;
    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "++NvMM3gpSetAttribute\n"));

    switch(AttributeType)
    {

        case NvMMWriterAttribute_FileExt:
        {
            FileExtLen = NvOsStrlen((char *) ((NvMMWriterAttrib_FileExt*)pAttribute)->szURI);
            NvOsMemset(&p3gpWriterContext->InputParam.FileExt[0], '\0', MAX_FILE_EXT);
            if(FileExtLen < MAX_FILE_EXT)
            NvOsStrncpy(&p3gpWriterContext->InputParam.FileExt[0], (char *)((NvMMWriterAttrib_FileExt*)pAttribute)->szURI, FileExtLen);
        }
        break;
        case NvMMWriterAttribute_FileName:
        {
            Filepath = ((NvMMWriterAttrib_FileName*)pAttribute)->szURI;
            NVMM_CHK_ERR(NvMM3gpStoreFileUri(&p3gpWriterContext->FileURI,
                (char *)Filepath));
        }
        break;
        case NvMMWriterAttribute_StreamConfig:
        {
            NVMM_CHK_ERR(NvMM3gpWriterSetStreamConfig(WriterContext,
                (NvMMWriterStreamConfig*)pAttribute));
        }
        break;
        case NvMMWriterAttribute_TempFilePath:
        {
            NVMM_CHK_ERR(NvMM3gpStoreFileUri(&p3gpWriterContext->TempFileURI,
                ((NvMMWriterAttrib_TempFilePath *)pAttribute)->szURITempPath));
        }
        break;
        case NvMMWriterAttribute_UserData:
        {
             NVMM_CHK_ERR(NvMM3gpWriterSetUserData(WriterContext,
                 (NvMMWriteUserDataConfig*)pAttribute));
        }
        break;
        case NvMMWriterAttribute_FileSize:
        {
            if( ((NvMMWriterAttrib_FileSize *)pAttribute)->maxFileSize >
                 ((NvU64)FILE_SIZE_LIMIT * 2) - (ONE_MB * 100) )// 3.9GB
                p3gpWriterContext->AtomMode32OR64Bit = NvMM_Mode64BitAtom;
              else
                  p3gpWriterContext->AtomMode32OR64Bit = NvMM_Mode32BitAtom;
            p3gpWriterContext->MaxMDATFileSizeLimit = ((NvMMWriterAttrib_FileSize *)pAttribute)->maxFileSize;
        }
        break;
        case NvMMWriterAttribute_SplitTrack:
        {
            NVMM_CHK_ERR(NvMM3gpStoreFileUri(&p3gpWriterContext->FileURI,
                ((NvMMWriterAttrib_FileName*)pAttribute)->szURI));
            p3gpWriterContext->SplitMode.SplitFlag = NV_TRUE;
        }
        break;
        case NvMMWriterAttribute_TrakDurationLimit:
        {
            p3gpWriterContext->InputParam.MaxTrakDurationLimit =
                ((NvMMWriterAttrib_TrakDurationLimit *)pAttribute)->maxTrakDuration;
        }
        break;
        default:
        break;
    }
cleanup:
    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "--NvMM3gpSetAttribute\n"));
    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_ERROR, "--NvMM3gpSetAttribute ->%x\n",Status));
    return Status;
}

static NvError
NvMM3gpOpenWriter(
    NvmmWriterContext WriterContext)
{
    NvError Status = NvSuccess;
    NvMM3gpWriterBlockContext* p3gpWriterBlockContext = NULL;

    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "++NvMM3gpOpenWriter\n"));
    NVMM_CHK_ARG(WriterContext);
    p3gpWriterBlockContext = (NvMM3gpWriterBlockContext *)WriterContext;

    NvmmGetFileContentPipe(&p3gpWriterBlockContext->pPipe);
    NVMM_CHK_ARG(p3gpWriterBlockContext->pPipe);
    NVMM_CHK_ERR(NvOsSemaphoreCreate(&p3gpWriterBlockContext->PingPongBufferFilledSema, 0));
    NVMM_CHK_ERR(NvOsSemaphoreCreate(&p3gpWriterBlockContext->PingPongBufferConsumedSema, 0));
    NVMM_CHK_ERR(NvMMQueueCreate(&p3gpWriterBlockContext->PingPongMsgQ, MAX_QUEUE_LEN, sizeof(NvMMFileWriteMsg), NV_TRUE));

    p3gpWriterBlockContext->SplitMode.ReInitBuffer = NULL;
    p3gpWriterBlockContext->SplitMode.SplitFlag = NV_FALSE;
    p3gpWriterBlockContext->SplitMode.ResetAudioTS = 0;
    NVMM_CHK_ERR(NvOsThreadCreate(NvmmMDATFileWriteThread, (void *)(p3gpWriterBlockContext), &(p3gpWriterBlockContext->MDATFileWriterThread)));

    cleanup:
    if(Status != NvSuccess)
    {
        //Pass the same status as above
        (void)NvMM3gpCloseWriter((NvmmWriterContext)p3gpWriterBlockContext);
    }
    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_INFO, "--NvMM3gpOpenWriter\n"));
    NV_LOGGER_PRINT((NVLOG_MP4_WRITER, NVLOG_ERROR, "--NvMM3gpOpenWriter ->%x\n",Status));
    return Status;
}

NvError
NvMMCreate3gpWriterContext(
    NvmmCoreWriterOps *CoreWriterOps,
    NvmmWriterContext *pWriterHandle)
{
    NvError Status = NvSuccess;
    NvMM3gpWriterBlockContext* p3gpWriterBlockContext = NULL;

    NVMM_CHK_ARG(CoreWriterOps);

    // Populate writer 
    CoreWriterOps->OpenWriter = NvMM3gpOpenWriter;
    CoreWriterOps->CloseWriter = NvMM3gpCloseWriter;
    CoreWriterOps->ProcessRawBuffer = NvMM3gpWriterAddMediaSample;
    CoreWriterOps->GetBufferRequirements = NvMM3gpWriterBlockGetBufferRequirements;
    CoreWriterOps->GetAttribute = NvMM3gpGetAttribute;
    CoreWriterOps->SetAttribute = NvMM3gpSetAttribute;

    p3gpWriterBlockContext = (NvMM3gpWriterBlockContext *)NvOsAlloc(sizeof(NvMM3gpWriterBlockContext));
    NVMM_CHK_MEM(p3gpWriterBlockContext);
    NvOsMemset(p3gpWriterBlockContext, 0, sizeof(NvMM3gpWriterBlockContext));

    p3gpWriterBlockContext->InputParam.MaxAudioFrames = MAX_AUDIO_FRAMES;
    p3gpWriterBlockContext->InputParam.MaxVideoFrames = MAX_VIDEO_FRAMES;
    p3gpWriterBlockContext->InputParam.SpeechAudioFlag = MAX_AUDIO_FLAGS;
    p3gpWriterBlockContext->AtomMode32OR64Bit = NvMM_Mode32BitAtom;
    p3gpWriterBlockContext->MaxMDATFileSizeLimit = ((NvU64)FILE_SIZE_LIMIT * 2) - (ONE_MB * 100);

    *pWriterHandle = (NvmmWriterContext )p3gpWriterBlockContext;
cleanup:
    return Status;
}
