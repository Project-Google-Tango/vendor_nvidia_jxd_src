
/**
 * @nvmm_aacparser.c
 * @brief IMplementation of Aac parser Class.
 *
 * @b Description: Implementation of Aac parser public API's.
 *
 */

/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvmm_aacparser.h"
#include "nvos.h"

enum
{
    MAX_NUM_SAMPLES = 1024
};

#define S_OK    0
#define S_FALSE 1
/**
 * @brief       Sampling rate table
 */
static const NvS32 AacSamplingRateTable[16] =
{
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
    16000, 12000, 11025,  8000,  7350,    -1,    -1,     0
};

NvU64 GetAacTimeStamp(NvS32 sampleRate)
{
    NvU64 CurrentTimeStamp = 0;

    switch(sampleRate)
    {
        case 48000:
             CurrentTimeStamp = (NvU64)TIMESTAMP_SR_48000;
            break;
        case 44100:
            CurrentTimeStamp = (NvU64)TIMESTAMP_SR_44100;
            break;
        case 32000:
            CurrentTimeStamp = (NvU64)TIMESTAMP_SR_32000;
            break;
        case 24000:
            CurrentTimeStamp = (NvU64)TIMESTAMP_SR_24000;
           break;
        case 22050:
            CurrentTimeStamp = (NvU64)TIMESTAMP_SR_22050;
           break;
        case 16000:
            CurrentTimeStamp = (NvU64)TIMESTAMP_SR_16000;
           break;
        case 12000:
            CurrentTimeStamp = (NvU64)TIMESTAMP_SR_12000;
           break;
        case 11025:
            CurrentTimeStamp = (NvU64)TIMESTAMP_SR_11025;
           break;
        case  8000:
            CurrentTimeStamp = (NvU64)TIMESTAMP_SR_8000;
           break;
        default:
            CurrentTimeStamp = (NvU64)TIMESTAMP_SR_44100;
    }

     return  (CurrentTimeStamp/2);
}


NvAacParser * NvGetAacReaderHandle(CPhandle hContent, CP_PIPETYPE_EXTENDED *pPipe )
{      
      NvAacParser *pState;

      pState = (NvAacParser *)NvOsAlloc(sizeof(NvAacParser));
      if (pState == NULL)
      {           
           goto Exit;
      }

      pState->pInfo = (NvAacStreamInfo*)NvOsAlloc(sizeof(NvAacStreamInfo));
      if ((pState->pInfo) == NULL)
      {
           NvOsFree(pState);
           pState = NULL;
           goto Exit;
      }

      pState->pInfo->pTrackInfo = (NvAacTrackInfo*)NvOsAlloc(sizeof(NvAacTrackInfo));
      if ((pState->pInfo->pTrackInfo) == NULL)
      {
           NvOsFree(pState->pInfo);
           pState->pInfo = NULL;           
           NvOsFree(pState);    
           pState = NULL;
           goto Exit;
      }
     
      NvOsMemset(((pState)->pInfo)->pTrackInfo,0,sizeof(NvAacTrackInfo));

      
      pState->cphandle = hContent;
      pState->pPipe = pPipe; 
      pState->numberOfTracks = 1;
      pState->InitFlag = NV_TRUE;
      pState->SeekInitFlag = NV_TRUE;
      pState->bAacHeader = NV_TRUE;      

Exit:    
  
      return pState;
}

void NvReleaseAacReaderHandle(NvAacParser * pState)
{
    if (pState)
    {
        if(pState->pInfo)
        {
            if (pState->pInfo->pTrackInfo != NULL)
            {
                NvOsFree(pState->pInfo->pTrackInfo);
                pState->pInfo->pTrackInfo = NULL;
            }

            NvOsFree(pState->pInfo);   
            pState->pInfo = NULL;
        }
        NvOsFree(pState);
    }   
}


NvError NvParseAacTrackInfo(NvAacParser *pState)
{
    NvError status = NvSuccess;    
    
    status =  NvAacInit(pState);  

    return status;
}

/**
 * @brief              This function initializes the stream property structure to zero.
 * @param[in]          stream  is the pointer to stream property structure
 * @returns            void
 */
void NvAacStreamInit(NvAacStream *stream)
{
    stream->bNoMoreData = 0;
    stream->CurrentFrame = 0;
    stream->CurrentOffset = 0;
    stream->CurrentSize = 0;
    stream->FileSize = 0;
    stream->TotalSize = 0;
}

NvError NvAacInit(NvAacParser *pState)
{
    NvError status = NvSuccess;    
    NvS32 tempSeek;
    NvADTSHeader pH;
    pState->m_TotalRDB = 0;

    NvAacStreamInit(&(pState->stream));

    status = NvGetAdtsHeader(pState,&pH);
    if (status != NvSuccess)
    {
        return status;
    }

     pState->m_CurrentTime = pState->m_PreviousTime = 0;
     pState->m_SeekSet = 0;
     
     pState->pInfo->pTrackInfo->noOfChannels = pH.channel_config;    
     pState->pInfo->pTrackInfo->samplingFreq = AacSamplingRateTable[pH.sampling_freq_idx];
     pState->pInfo->pTrackInfo->sampleSize = 16;
     pState->pInfo->pTrackInfo->data_offset= 0;
     pState->pInfo->pTrackInfo->total_time = NvAacGetDuration(pState);
      
     pState->pInfo->pTrackInfo->profile = pH.profile;
     pState->pInfo->pTrackInfo->samplingFreqIndex = pH.sampling_freq_idx;
     pState->pInfo->pTrackInfo->channelConfiguration = pH.channel_config;
     pState->pInfo->pTrackInfo->objectType =  2;  

     tempSeek = 0;
     status = pState->pPipe->cpipe.SetPosition(pState->cphandle, tempSeek, CP_OriginBegin);   
     if (status != NvSuccess) return NvError_ParserFailure;
       

    return status;
}

/**
 * @brief          This function gets the current position of the seek bar.
 * @returns        current time
 */
NvS64 NvAacGetCurrentPosition(NvAacParser *pState)
{
    return ((pState->m_TotalRDB * MAX_NUM_SAMPLES * 1000000 * 10) / pState->pInfo->pTrackInfo->samplingFreq);
}

NvError NvGetAdtsHeader(NvAacParser *pState, NvADTSHeader *pH)
{
    NvError Status = NvSuccess;
     // Look for a sync word and then check the frame data using crc check sum
    // If correct fix the fixed header for sync search reasons
    // and use this word for future sync word searches
    NvU32 tagsize = 0;
    NvU32 nBufferSize;
    NvU64 FileSize = 0;
    NvS32 tempSeek;
    NvU32 k;
    NvU32 DataOffset = 0;
    pState->m_FirstTime = 1;

    NVMM_CHK_ERR(pState->pPipe->GetSize(pState->cphandle, (CPuint64*)&FileSize));

    tempSeek = 0;
    NVMM_CHK_ERR(pState->pPipe->cpipe.SetPosition(pState->cphandle, tempSeek, CP_OriginBegin));

    if (FileSize < MAX_ADTSAAC_BUFFER_SIZE)
    {
        nBufferSize =(NvU32) FileSize;
    }
    else
    {
        nBufferSize = MAX_ADTSAAC_BUFFER_SIZE;
    }

    NVMM_CHK_ERR(pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)pState->m_cBuffer, nBufferSize));

    if (!NvOsMemcmp(pState->m_cBuffer, "ID3", 3))
    {
        tagsize = (pState->m_cBuffer[6] << 21) | (pState->m_cBuffer[7] << 14) | (pState->m_cBuffer[8] << 7) | pState->m_cBuffer[9];
        tagsize += 10;
        DataOffset = tagsize;
        if(tagsize > MAX_ADTSAAC_BUFFER_SIZE)
        {
            NVMM_CHK_ERR(pState->pPipe->cpipe.SetPosition(pState->cphandle, (CPint)tagsize, CP_OriginBegin));
            NVMM_CHK_ERR(pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)pState->m_cBuffer, nBufferSize));
            tagsize = 0;
        }
    }

    pState->m_pc = pState->m_cBuffer + tagsize;
    k = tagsize;

    // search the first 2 * 768 bytes for the sync word
    while (((pState->m_pc[0] != 0xFF) || ((pState->m_pc[1] & 0xF6) != 0xF0)) && (k < (MAX_ADTSAAC_BUFFER_SIZE)))
    {
        pState->m_pc++;
        k++;
    };

    if ((pState->m_pc[0] == 0xFF && ((pState->m_pc[1] & 0xF6) == 0xF0)))
    {
        pH->syncword = 0xFFF;
        pH->id = (pState->m_pc[1] >> 3) & 0x1;
        pH->layer  =  (pState->m_pc[1] >> 1) & 0x3;
        pH->protection_abs = pState->m_pc[1] & 0x1;
        pH->profile = (pState->m_pc[2] >> 6) & 0x3;
        pH->sampling_freq_idx = (pState->m_pc[2] >> 2) & 0xF;
        pH->private_bit = (pState->m_pc[2] >> 1) & 0x1;
        pH->channel_config = ((pState->m_pc[2] & 0x1) << 2) | ((pState->m_pc[3] >> 6) & 0x3);
        if (2 < pH->channel_config)
        {
            return NvError_UnSupportedStream;
        }
        pH->original_copy = (pState->m_pc[3] >> 5) & 0x1;
        pH->home = (pState->m_pc[3] >> 4) & 0x1;

        NVMM_CHK_ERR(pState->pPipe->cpipe.SetPosition(pState->cphandle, (CPint)DataOffset, CP_OriginBegin));
    }
    else
    {
        return NvError_UnSupportedStream;
    }

cleanup:

    if (Status == NvError_EndOfFile)
    {
        Status =  NvError_ParserEndOfStream;
    }
    return Status;
}

/**
 * @brief          This function gets the total file duration
 * @returns        File Duration
 */
NvS64 NvAacGetDuration(NvAacParser *pState)
{
    NvError Status = NvSuccess;
    NvADTSHeader Header;
    NvADTSHeader  *pH = NULL;
    NvS32 tempSeek;
    NvU32 t0;
    NvU32 nNewBytes;
    NvU32 frameSize = 0;
    NvU32 nBufferSize;
    NvU32 BytesConsumed = 0;
    NvU32 BytesCon0, BytesCon1;
    NvU64 Total_rdb = 0;
    NvU64 Duration=0;
    NvU32 startSize = 0;
    NvU32 endSize = 0;
    NvU32 preBytes = 0;
    NvU32 readCount = 0;
    pState->m_FirstTime = 1;
    pState->m_GBytesConsumed = MAX_ADTSAAC_BUFFER_SIZE;

    NV_LOGGER_PRINT((NVLOG_AAC_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));
    if (pState->m_FirstTime)
    {
        tempSeek = 0;
        NVMM_CHK_CP(pState->pPipe->cpipe.SetPosition(pState->cphandle, tempSeek, CP_OriginBegin));
        NVMM_CHK_CP(pState->pPipe->cpipe.GetPosition(pState->cphandle, (CPuint*)&startSize));
        NVMM_CHK_CP(pState->pPipe->cpipe.SetPosition(pState->cphandle, tempSeek, CP_OriginEnd));
        NVMM_CHK_CP(pState->pPipe->cpipe.GetPosition(pState->cphandle, (CPuint*)&endSize));

        pState->stream.FileSize = endSize - startSize;
        tempSeek = 0;
        NVMM_CHK_CP(pState->pPipe->cpipe.SetPosition(pState->cphandle, tempSeek, CP_OriginBegin));
     }

    while (!pState->stream.bNoMoreData)
    {
        if (pState->stream.FileSize < MAX_ADTSAAC_BUFFER_SIZE)
        {
            nBufferSize = pState->stream.FileSize;
        }
        else
        {
            nBufferSize = MAX_ADTSAAC_BUFFER_SIZE;
        }

        if (pState->m_FirstTime)
        {
            Status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)pState->m_cBuffer, nBufferSize);
            if(Status != NvSuccess)
            {      
                goto NvMMAacGetDuration_err;
            }

            // For the first time, skip past the ID3 tag, if present
            if (NvOsMemcmp(pState->m_cBuffer, "ID3", 3) == 0)
            {
                NvU32 uId3TagSize = (pState->m_cBuffer[6] << 21) | (pState->m_cBuffer[7] << 14) | (pState->m_cBuffer[8] << 7) | pState->m_cBuffer[9];
                uId3TagSize += 10;

                // Set the file pointer to skip the ID3 tag and read buffer from there                
                Status = pState->pPipe->cpipe.SetPosition(pState->cphandle, uId3TagSize, CP_OriginBegin);
                if(Status != NvSuccess)
                {      
                    goto NvMMAacGetDuration_err;
                }
                
                Status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)pState->m_cBuffer, nBufferSize);
                if(Status != NvSuccess)
                {      
                    goto NvMMAacGetDuration_err;
                }
            }

            pState->m_GBytesConsumed = 0;
            pState->m_FirstTime = 0;
            pState->m_pc = pState->m_cBuffer;   
        }

        pH = &Header;
        BytesCon0 = 0;
        readCount = 0;
        while ((pState->m_pc[0] != 0xFF) || ((pState->m_pc[1] & 0xF6) != 0xF0))
        {
            ++pState->m_pc;
            ++BytesCon0;
            if (BytesCon0 > nBufferSize)
            {
                if (readCount > 2)
                {
                     NV_LOGGER_PRINT((NVLOG_AAC_PARSER, NVLOG_DEBUG, "Sync Word not Found"));
                     Status = NvError_ParserCorruptedStream;
                     goto cleanup;
                }
                pState->m_GBytesConsumed += BytesCon0;
                preBytes += BytesCon0 ;
                BytesCon0 = 0;
                Status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)pState->m_cBuffer, nBufferSize);
                if(Status != NvSuccess)
                {
                    goto NvMMAacGetDuration_err;
                }
                pState->m_pc = pState->m_cBuffer;
                readCount++;
            }
        }
        BytesCon0 += preBytes;
        preBytes = 0;
        pState->m_GBytesConsumed += BytesCon0;

        pH->syncword = 0xFFF;
        pH->id = (pState->m_pc[1] >> 3) & 0x1;
        pH->layer = (pState->m_pc[1] >> 1) & 0x3;
        pH->protection_abs = pState->m_pc[1] & 0x1;
        pH->profile = (pState->m_pc[2] >> 6) & 0x3;
        pH->sampling_freq_idx = (pState->m_pc[2] >> 2) & 0xF;
        pH->private_bit = (pState->m_pc[2] >> 1) & 0x1;
        pH->channel_config = ((pState->m_pc[2] & 0x1) << 2) | ((pState->m_pc[3] >> 6) & 0x3);
        pH->original_copy = (pState->m_pc[3] >> 5) & 0x1;
        pH->home = (pState->m_pc[3] >> 4) & 0x1;
        pH->copyright_id_bit = (pState->m_pc[3] >> 3) & 0x1;
        pH->copyright_id_start = (pState->m_pc[3] >> 1) & 0x1;
        pH->frame_length = (((pState->m_pc[3] & 0x3)) << 11) | (pState->m_pc[4] << 3) | ((pState->m_pc[5] >> 5) & 0x7);
        pH->adts_buffer_fullness = ((pState->m_pc[5] & 0x1F) << 6) | ((pState->m_pc[6] >> 2) & 0x3F);
        pH->num_of_rdb = pState->m_pc[6] & 0x3;

        if (pH->num_of_rdb == 0)
        {
            if (pH->protection_abs == 0)
            {
                pH->crc_check = ((pState->m_pc[7] << 8) & 0xFF00) | (pState->m_pc[8] & 0xFF);
            }
            else
            {
                pH->crc_check = 0;
            }
        }

        Total_rdb += pH->num_of_rdb + 1;
        frameSize = pH->frame_length;
        pState->m_GBytesConsumed += frameSize;
        pState->m_pc += (ADTSAAC_HEADER_SIZE - 2 * pH->protection_abs);
        t0 = frameSize - (ADTSAAC_HEADER_SIZE - 2 * pH->protection_abs);
        pState->m_pcBuff = pState->m_pc;
        pState->m_pc += t0;
        BytesCon1 = 0;
        readCount = 0;
        while ((pState->m_pc[0] != 0xFF) || ((pState->m_pc[1] & 0xF6) != 0xF0))
        {
            ++pState->m_pc;
            ++BytesCon1;
            if (BytesCon1 > MAX_AAC_FRAME_SINGLE_CH_SIZE)
            {
                if (readCount > 2)
                {
                     NV_LOGGER_PRINT((NVLOG_AAC_PARSER, NVLOG_DEBUG, "Sync Word not Found"));
                     Status = NvError_ParserCorruptedStream;
                     goto cleanup;
                }
                BytesCon1 = 0;
                Status = pState->pPipe->cpipe.Read(pState->cphandle,
                                       (CPbyte*)pState->m_cBuffer, nBufferSize);
                if(Status != NvSuccess)
                    goto NvMMAacGetDuration_err;

                pState->m_pc = pState->m_cBuffer;
                readCount++;
            }
        }

        BytesConsumed = BytesCon1 + BytesCon0;
        NvOsMemcpy(pState->m_cBuffer, pState->m_pc, (MAX_ADTSAAC_BUFFER_SIZE - (frameSize + BytesConsumed)));
        Status = pState->pPipe->cpipe.Read(pState->cphandle,
                 (CPbyte*)&pState->m_cBuffer[MAX_ADTSAAC_BUFFER_SIZE - (frameSize + BytesConsumed)],
                               frameSize + BytesConsumed);
        if(Status != NvSuccess)
        {
            goto NvMMAacGetDuration_err;
        }
        nNewBytes = frameSize + BytesConsumed;
        if (nNewBytes == 0)
            goto NvMMAacGetDuration_err;

        pState->stream.FileSize -= nNewBytes;
        pState->m_pc = pState->m_cBuffer;
        pState->stream.CurrentFrame++;
        pState->stream.TotalSize += BytesConsumed + frameSize;

    }
NvMMAacGetDuration_err:
    
    if (pH)
    {
        Duration = ((Total_rdb * MAX_NUM_SAMPLES) / AacSamplingRateTable[pH->sampling_freq_idx]);
    }  
cleanup:
    NV_LOGGER_PRINT((NVLOG_AAC_PARSER, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
    return Duration;
}

NvError NvAacSeek(NvAacParser *pState, NvS64 seekTime)
{
    NvError Status = NvSuccess;
    NvADTSHeader Header;
    NvADTSHeader  *pH = NULL;
    NvU32 t0;
    NvS32 tempSeek;
    NvU32 frameSize = 0;
    NvU32 nBufferSize;
    NvU32 BytesConsumed = 0;
    NvU32 BytesCon0, BytesCon1;
    NvU64 Total_rdb = 0;
    NvS64 Current_time = 0;
    NvU32 BytesCount =0;
    NvU32 tagsize = 0;
    NvU32 startSize = 0;
    NvU32 endSize = 0;
    NvU32 readCount = 0;

    NV_LOGGER_PRINT((NVLOG_AAC_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));
    if( seekTime == 0)
    {
        pState->SeekInitFlag = NV_TRUE;
    }

    if( pState->SeekInitFlag == NV_TRUE)
    {
        NvAacStreamInit(&(pState->stream));
        pState->m_FirstTime = 1;
        pState->m_GBytesConsumed = MAX_ADTSAAC_BUFFER_SIZE;
        pState->m_SeekSet = 1;
        pState->SeekInitFlag = NV_FALSE;        
    }

    if (pState->m_FirstTime)
    {
        tempSeek = 0;

        NVMM_CHK_CP(pState->pPipe->cpipe.SetPosition(pState->cphandle, tempSeek, CP_OriginBegin));
        NVMM_CHK_CP(pState->pPipe->cpipe.GetPosition(pState->cphandle, (CPuint*)&startSize));
        NVMM_CHK_CP(pState->pPipe->cpipe.SetPosition(pState->cphandle, tempSeek, CP_OriginEnd));
        NVMM_CHK_CP(pState->pPipe->cpipe.GetPosition(pState->cphandle, (CPuint*)&endSize));

        pState->stream.FileSize = endSize - startSize;
    }

    tempSeek = 0;
    NVMM_CHK_CP(pState->pPipe->cpipe.SetPosition(pState->cphandle, tempSeek, CP_OriginBegin));
    pState->m_FirstTime = 1;

    while (!pState->stream.bNoMoreData)
    {
        if (pState->stream.FileSize < MAX_ADTSAAC_BUFFER_SIZE)
        {
            nBufferSize = pState->stream.FileSize;
        }
        else
        {
            nBufferSize = MAX_ADTSAAC_BUFFER_SIZE;
        }

        if (pState->m_FirstTime)
        {
            NVMM_CHK_CP(pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte*)pState->m_cBuffer, nBufferSize));
            if (!NvOsMemcmp(pState->m_cBuffer, "ID3", 3))
            {
                tagsize = (pState->m_cBuffer[6] << 21) | (pState->m_cBuffer[7] << 14) | (pState->m_cBuffer[8] << 7) | pState->m_cBuffer[9];
                tagsize += 10;
                BytesCount += tagsize;
                NVMM_CHK_CP(pState->pPipe->cpipe.SetPosition(pState->cphandle,
                                                             tagsize, CP_OriginBegin));
                NVMM_CHK_CP(pState->pPipe->cpipe.Read(pState->cphandle,
                                          (CPbyte*)pState->m_cBuffer, nBufferSize));
            }
            pState->m_pc = pState->m_cBuffer;
            pState->m_GBytesConsumed = 0;
            pState->m_FirstTime = 0;
            pState->m_SeekSet = 1;
        }

        pH = &Header;
        BytesCon0 = 0;
        readCount = 0;
        while ((pState->m_pc[0] != 0xFF) || ((pState->m_pc[1] & 0xF6) != 0xF0))
        {
            ++pState->m_pc;
            ++BytesCon0;
            if (BytesCon0 > nBufferSize)
            {
                if (readCount > 2)
                {
                     NV_LOGGER_PRINT((NVLOG_AAC_PARSER, NVLOG_DEBUG, "Sync Word not Found"));
                     Status = NvError_ParserCorruptedStream;
                     goto cleanup;
                }
                pState->m_GBytesConsumed += BytesCon0;
                BytesCount += BytesCon0;
                NVMM_CHK_CP(pState->pPipe->cpipe.Read(pState->cphandle,
                                          (CPbyte*)pState->m_cBuffer, nBufferSize));
                pState->m_pc = pState->m_cBuffer;
                BytesCon0 = 0;
                readCount++;
            }
        };

        pState->m_GBytesConsumed += BytesCon0;
        BytesCount += BytesCon0;

        pH->syncword = 0xFFF;
        pH->id = (pState->m_pc[1] >> 3) & 0x1;
        pH->layer = (pState->m_pc[1] >> 1) & 0x3;
        pH->protection_abs = pState->m_pc[1] & 0x1;
        pH->profile = (pState->m_pc[2] >> 6) & 0x3;
        pH->sampling_freq_idx = (pState->m_pc[2] >> 2) & 0xF;
        pH->private_bit = (pState->m_pc[2] >> 1) & 0x1;
        pH->channel_config = ((pState->m_pc[2] & 0x1) << 2) | ((pState->m_pc[3] >> 6) & 0x3);

        pH->original_copy = (pState->m_pc[3] >> 5) & 0x1;
        pH->home = (pState->m_pc[3] >> 4) & 0x1;
        pH->copyright_id_bit = (pState->m_pc[3] >> 3) & 0x1;
        pH->copyright_id_start = (pState->m_pc[3] >> 1) & 0x1;
        pH->frame_length = (((pState->m_pc[3] & 0x3)) << 11) | (pState->m_pc[4] << 3) | ((pState->m_pc[5] >> 5) & 0x7);
        pH->adts_buffer_fullness = ((pState->m_pc[5] & 0x1F) << 6) | ((pState->m_pc[6] >> 2) & 0x3F);
        pH->num_of_rdb = pState->m_pc[6] & 0x3;
        Total_rdb += (pH->num_of_rdb + 1);

        Current_time = ((Total_rdb * MAX_NUM_SAMPLES * 10000000) / pState->pInfo->pTrackInfo->samplingFreq);

        if (Current_time >= seekTime)
        {
            NVMM_CHK_CP(pState->pPipe->cpipe.SetPosition(pState->cphandle,
                                                         BytesCount, CP_OriginBegin));
            break;
        }

        if (pH->num_of_rdb == 0)
        {
            if (pH->protection_abs == 0)
            {
                pH->crc_check = ((pState->m_pc[7] << 8) & 0xFF00) | (pState->m_pc[8] & 0xFF);
            }
            else
            {
                pH->crc_check = 0;
            }
        }
        frameSize = pH->frame_length;
        pState->m_GBytesConsumed += frameSize;
        BytesCount += frameSize;
        pState->m_pc += (ADTSAAC_HEADER_SIZE - 2 * pH->protection_abs);
        t0 = frameSize - (ADTSAAC_HEADER_SIZE - 2  * pH->protection_abs);
        pState->m_pcBuff = pState->m_pc;
        pState->m_pc += t0;
        BytesCon1 = 0;
        readCount = 0;
        while ((pState->m_pc[0] != 0xFF) || ((pState->m_pc[1] & 0xF6) != 0xF0))
        {
            ++pState->m_pc;
            ++BytesCon1;
            if (BytesCon1 > MAX_AAC_FRAME_SINGLE_CH_SIZE)
            {
                if (readCount > 2)
                {
                     NV_LOGGER_PRINT((NVLOG_AAC_PARSER, NVLOG_DEBUG, "Sync Word not Found"));
                     Status = NvError_ParserCorruptedStream;
                     goto cleanup;
                }
                BytesCount += BytesCon1;
                NVMM_CHK_CP(pState->pPipe->cpipe.Read(pState->cphandle,
                                          (CPbyte*)pState->m_cBuffer, nBufferSize));
                pState->m_pc = pState->m_cBuffer;
                BytesCon1 = 0;
                readCount++;
            }
        };
        BytesCount += BytesCon1;
        BytesConsumed = BytesCon1 + BytesCon0;
        NvOsMemcpy(pState->m_cBuffer, pState->m_pc, (MAX_ADTSAAC_BUFFER_SIZE-(frameSize + BytesConsumed )));

        NVMM_CHK_CP(pState->pPipe->cpipe.Read(pState->cphandle,
                                  (CPbyte*)&pState->m_cBuffer[MAX_ADTSAAC_BUFFER_SIZE - (frameSize + BytesConsumed )],
                                   frameSize + BytesConsumed));

        pState->m_pc = pState->m_cBuffer;
        pState->stream.CurrentFrame++;
        pState->stream.TotalSize += BytesConsumed + frameSize + tagsize;
        tagsize = 0;
    }    

cleanup:
    NV_LOGGER_PRINT((NVLOG_AAC_PARSER, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
    return Status;
}


NvError NvAdts_AacGetNextFrame(NvAacParser *pState, NvU8 *pData, NvU32 askBytes, NvU32 *ulBytesRead)
{
    NvError Status = NvSuccess;
    NvADTSHeader Header;
    NvADTSHeader  *pH = NULL;
    NvU32 t0;
    NvU32 nNewBytes;
    NvU32 frameSize = 0;
    NvU32 nBufferSize;
    NvU32 BytesConsumed = 0;
    NvU32 BytesCon0, BytesCon1;
    NvS32 Error = 0;
    NvU32 preBytes = 0;
    NvU32 readCount = 0;
    pState->m_GBytesConsumed = MAX_ADTSAAC_BUFFER_SIZE;
    *ulBytesRead = 0;

    NV_LOGGER_PRINT((NVLOG_AAC_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));
    if (pState->stream.bNoMoreData)
    {
        Status = NvSuccess;
        goto cleanup;
    }

    nBufferSize = askBytes;

    if( pState->InitFlag == NV_TRUE)
    {
        pState->m_FirstTime = 1;
        pState->m_SeekSet = 1;
        pState->InitFlag = NV_FALSE;
    }

    if (pState->m_FirstTime || pState->m_SeekSet)
    {
        NVMM_CHK_ERR(pState->pPipe->cpipe.Read(pState->cphandle,
                                   (CPbyte*)pState->m_cBuffer, nBufferSize));

        // For the first time, skip past the ID3 tag, if present
        if (pState->m_FirstTime && (NvOsMemcmp(pState->m_cBuffer, "ID3", 3) == 0))
        {
            NvU32 uId3TagSize = (pState->m_cBuffer[6] << 21) | (pState->m_cBuffer[7] << 14) | (pState->m_cBuffer[8] << 7) | pState->m_cBuffer[9];
            uId3TagSize += 10;

            // Set the file pointer to skip the ID3 tag and read buffer from there     
            NVMM_CHK_ERR(pState->pPipe->cpipe.SetPosition(pState->cphandle, uId3TagSize, CP_OriginBegin));
            NVMM_CHK_ERR(pState->pPipe->cpipe.Read(pState->cphandle,
                                       (CPbyte*)pState->m_cBuffer, nBufferSize));
        }

        pState->m_GBytesConsumed = 0;
        pState->m_FirstTime = 0;
        pState->m_SeekSet = 0;
        pState->m_pc = pState->m_cBuffer;
    }

    pH = &Header;
    BytesCon0 = 0;

    while ((pState->m_pc[0] != 0xFF) || ((pState->m_pc[1] & 0xF6) != 0xF0))
    {
        ++pState->m_pc;
        ++BytesCon0;
        if (BytesCon0 > nBufferSize)
        {
             if (readCount > 2)
             {
                 NV_LOGGER_PRINT((NVLOG_AAC_PARSER, NVLOG_DEBUG, "Sync Word not Found"));
                 Status = NvError_ParserCorruptedStream;
                 goto cleanup;
             }
             pState->m_GBytesConsumed += BytesCon0;
             NVMM_CHK_ERR(pState->pPipe->cpipe.Read(pState->cphandle,
                                        (CPbyte*)pState->m_cBuffer, nBufferSize));
             pState->m_pc = pState->m_cBuffer;
             BytesCon0 = 0;
             readCount++;
        }
    }

    pState->m_GBytesConsumed += BytesCon0;
    if (!Error)
    {
        pH->syncword = 0xFFF;
        pH->id = (pState->m_pc[1] >> 3) & 0x1;
        pH->layer = (pState->m_pc[1] >> 1) & 0x3;
        pH->protection_abs = pState->m_pc[1] & 0x1;
        pH->profile = (pState->m_pc[2] >> 6) & 0x3; // 1 Low complexity
        pH->sampling_freq_idx = (pState->m_pc[2] >> 2) & 0xF;
        pH->private_bit = (pState->m_pc[2] >> 1) & 0x1;
        pH->channel_config = ((pState->m_pc[2] & 0x1) << 2) | ((pState->m_pc[3] >> 6) & 0x3);
        if (2 < pH->channel_config)
        {
            Status = NvError_ParserFailure;
            NV_LOGGER_PRINT((NVLOG_AAC_PARSER, NVLOG_ERROR, "channel config failure :%d",pH->channel_config));
            goto cleanup;
        }
        pH->original_copy = (pState->m_pc[3] >> 5) & 0x1;
        pH->home = (pState->m_pc[3] >> 4) & 0x1;
        pH->copyright_id_bit = (pState->m_pc[3] >> 3) & 0x1;
        pH->copyright_id_start = (pState->m_pc[3] >> 1) & 0x1;
        pH->frame_length = (((pState->m_pc[3] & 0x3)) << 11) | (pState->m_pc[4] << 3) | ((pState->m_pc[5] >> 5) & 0x7);
        pH->adts_buffer_fullness = ((pState->m_pc[5] & 0x1F) << 6) | ((pState->m_pc[6] >> 2) & 0x3F);
        pH->num_of_rdb = pState->m_pc[6] & 0x3;
        pState->m_TotalRDB += (pH->num_of_rdb + 1);
        pState->m_CurrentTime = ((pState->m_TotalRDB * MAX_NUM_SAMPLES * 1000000 * 10) /  pState->pInfo->pTrackInfo->samplingFreq);

        if (pH->num_of_rdb == 0)
        {
            if (pH->protection_abs == 0)
            {
                pH->crc_check = ((pState->m_pc[7] << 8) & 0xFF00) | (pState->m_pc[8] & 0xFF);
            }
            else
            {
                pH->crc_check = 0;
            }
        }

        frameSize = pH->frame_length;
        pState->m_GBytesConsumed += frameSize;
        pState->m_pc += (ADTSAAC_HEADER_SIZE - 2 * pH->protection_abs);
        t0 = frameSize - (ADTSAAC_HEADER_SIZE - 2 * pH->protection_abs);
        pState->m_pcBuff = pState->m_pc;
        pState->m_pc += t0;
        BytesCon1 = 0;
        readCount = 0;
        while ((pState->m_pc[0] != 0xFF) || ((pState->m_pc[1] & 0xF6) != 0xF0))
        {
            ++pState->m_pc;
            ++BytesCon1;
            if (BytesCon1 > MAX_AAC_FRAME_SINGLE_CH_SIZE)
            {
                if (readCount > 2)
                {
                     NV_LOGGER_PRINT((NVLOG_AAC_PARSER, NVLOG_DEBUG, "Sync Word not Found"));
                     Status = NvError_ParserCorruptedStream;
                     goto cleanup;
                }
                NVMM_CHK_ERR(pState->pPipe->cpipe.Read(pState->cphandle,
                                           (CPbyte*)pState->m_cBuffer, nBufferSize));

                pState->m_pc = pState->m_cBuffer;
                preBytes += BytesCon1;
                BytesCon1 = 0;
                readCount++;
            }
        };
        BytesCon1 += preBytes;
        NvOsMemcpy(pData, pState->m_pcBuff, t0 + BytesCon1);
        *ulBytesRead = t0 + BytesCon1;
        BytesConsumed = BytesCon1 + BytesCon0;
        NvOsMemcpy(pState->m_cBuffer, pState->m_pc, (MAX_ADTSAAC_BUFFER_SIZE - (frameSize + BytesConsumed)));
        NVMM_CHK_ERR(pState->pPipe->cpipe.Read(pState->cphandle,
                                   (CPbyte*)&pState->m_cBuffer[MAX_ADTSAAC_BUFFER_SIZE - (frameSize + BytesConsumed)],
                                   frameSize + BytesConsumed));

        nNewBytes = frameSize + BytesConsumed;
        if (nNewBytes == 0)
        {
            Status = NvError_ParserReadFailure;
            NV_LOGGER_PRINT((NVLOG_AAC_PARSER, NVLOG_DEBUG, "ParserReadFailure occured"));
            goto cleanup;
        }
        pState->m_pc = pState->m_cBuffer;
        pState->stream.CurrentFrame++;
    }

cleanup:
    NV_LOGGER_PRINT((NVLOG_AAC_PARSER, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
    return Status;
}

NvError NvAacSeekToTime(NvAacParser *pState, NvS64 mSec)
{
    NvError status = NvSuccess;      

    status = NvAacSeek(pState, mSec);

    return status;
}

