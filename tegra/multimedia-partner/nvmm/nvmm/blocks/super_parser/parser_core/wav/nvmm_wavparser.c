/*
* Copyright (c) 2007 - 2010 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto. Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

/**
* @nvmm_wavparser.c
* @brief IMplementation of wav parser Class.
*
* @b Description: Implementation of wav parser public API's.
*
*/

#include "nvos.h"
#include "nvassert.h"
#include "nvmm_wavparser.h"


NvWavParser * NvGetWavReaderHandle(CPhandle hContent, CP_PIPETYPE_EXTENDED *pPipe )
{
    NvWavParser *pState = NULL;

    pState = (NvWavParser *)NvOsAlloc(sizeof(NvWavParser));
    if (pState == NULL)
    {
        goto Exit;
    }

    pState->pInfo = (NvWavStreamInfo*)NvOsAlloc(sizeof(NvWavStreamInfo));
    if (pState->pInfo == NULL)
    {
       NvOsFree( pState);
       pState = NULL;
       goto Exit;
    }

    NvOsMemset(pState->pInfo,0,sizeof(NvWavStreamInfo));

   pState->pInfo->pTrackInfo = (NvWavTrackInfo*)NvOsAlloc(sizeof(NvWavTrackInfo));
   if ((pState->pInfo->pTrackInfo) == NULL)
   {
           NvOsFree(pState->pInfo);
           pState->pInfo = NULL;
           NvOsFree(pState);
           pState = NULL;
           goto Exit;
   }

   NvOsMemset(((pState)->pInfo)->pTrackInfo,0,sizeof(NvWavTrackInfo));

   pState->pInfo->pHeader = (NvWavHdr*)NvOsAlloc(sizeof(NvWavHdr));
   if ((pState->pInfo->pHeader) == NULL)
   {
           NvOsFree(pState->pInfo);
           pState->pInfo = NULL;
           NvOsFree(pState);
           pState = NULL;
           goto Exit;
   }

    NvOsMemset(((pState)->pInfo)->pHeader,0,sizeof(NvWavHdr));

    pState->cphandle = hContent;
    pState->pPipe = pPipe;
    pState->numberOfTracks = 1;
    pState->bWavHeader = NV_TRUE;
    pState->dataOffset = 0;
    pState->bytesRead = 0;

Exit:
    return pState;
}

void NvReleaseWavReaderHandle(NvWavParser * pState)
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
            if (pState->pInfo->pHeader != NULL)
            {
                NvOsFree(pState->pInfo->pHeader);
                pState->pInfo->pHeader = NULL;
            }

            NvOsFree(pState->pInfo);
            pState->pInfo = NULL;
        }
    }
}

NvError NvWavSeekToTime(NvWavParser *pState, NvS64 mSec)
{
    NvError status = NvSuccess;
    NvU64 Offset;

    Offset = (NvU64)(pState->pInfo->pHeader->SampleRate *
        pState->pInfo->pHeader->NumChannels * pState->pInfo->pHeader->BitsPerSample)/8000;
    Offset = (NvU64)Offset * mSec;
    if(WAVE_FORMAT_ADPCM == pState->pInfo->pHeader->AudioFormat &&
        (pState->pInfo->pTrackInfo->blockAlign != 0))
    {
        Offset = (Offset - (Offset % pState->pInfo->pTrackInfo->blockAlign));
    }
    status = pState->pPipe->cpipe.SetPosition(pState->cphandle,
        (NvU32)(Offset+pState->dataOffset), CP_OriginBegin);
    if(status != NvSuccess)
    {
        status = NvError_ParserFailure;
    }
    pState->bytesRead = (NvU32)(Offset);
    return status;
}

NvU64 GetWavTimeStamp( NvWavParser *pState,NvU32 askBytes)
{
    NvU64 wTimeStamp = 0;

    if(WAVE_FORMAT_PCM == pState->pInfo->pHeader->AudioFormat)
    {
        askBytes = askBytes/(pState->pInfo->pHeader->NumChannels *
            (pState->pInfo->pHeader->BitsPerSample >> 3));
    }
    else if(WAVE_FORMAT_ALAW == pState->pInfo->pHeader->AudioFormat ||
        WAVE_FORMAT_MULAW == pState->pInfo->pHeader->AudioFormat)
    {
        askBytes = askBytes * 2;
        askBytes = askBytes/(pState->pInfo->pHeader->NumChannels * 2);
    }
    else if(WAVE_FORMAT_ADPCM == pState->pInfo->pHeader->AudioFormat)
    {
        askBytes = askBytes * 4;
        askBytes = askBytes/(pState->pInfo->pHeader->NumChannels * 2);
    }

    //timestamp in 100 nSec
    wTimeStamp = (askBytes * (10000000 / pState->pInfo->pHeader->SampleRate));
//   NvOsDebugPrintf("TimeStamp:%d  askbites=%d\n", (NvU32)wTimeStamp,askBytes);
    return wTimeStamp;
}
NvError NvParseWav(NvWavParser *pState)
{
    NvError Status = NvSuccess;
    NvS8 wavHeaderbuff[INITIAL_HEADER_SIZE];
    NvU64 FileSize = 0;
    NvU32 InitialSeek = 0;
    NvU32 HeaderSize = INITIAL_HEADER_SIZE;
    NvS16 wavICoefs[14];
    NvU32 ExtendedFormatlength = 0;
    NvU32   bytesPerBlock = 0;
    NvU16 wExtSize = 0;
    NvU16 nCoefs =0;
    NvU16 samplesPerBlock=0;
    NvU32 ReadBytesCount = 0;
    NvU32 RiffFmtValue = 0;

    Status = pState->pPipe->cpipe.SetPosition(pState->cphandle, 0, CP_OriginEnd);
    if (Status != NvSuccess)
    {
        return Status;
    }

    Status = pState->pPipe->cpipe.GetPosition(pState->cphandle, (CPuint*)&FileSize);
    if (Status != NvSuccess)
    {
        return Status;
    }

    if (FileSize < HeaderSize)
    {
        HeaderSize = (NvU32)FileSize;
    }

    InitialSeek = 0;
    Status = pState->pPipe->cpipe.SetPosition(pState->cphandle, InitialSeek, CP_OriginBegin);
    if (Status != NvSuccess)
    {
        return Status;
    }
    //to search "RIFF"
    while((RiffFmtValue != RIFF_TAG) && (InitialSeek < RIFF_TAG_SEARCH_RANGE))
    {
        Status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte *)wavHeaderbuff, 1);
        if (Status != NvSuccess)
        {
            return Status;
        }
        RiffFmtValue >>= 8;
        RiffFmtValue |= wavHeaderbuff[0]<< 24;
        InitialSeek++;
    }

    InitialSeek -= 4;   //Read header data from where RIFF start
    Status = pState->pPipe->cpipe.SetPosition(pState->cphandle, InitialSeek, CP_OriginBegin);
    if (Status != NvSuccess)
    {
        return Status;
    }

    Status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte *)wavHeaderbuff, HeaderSize);
    if (Status != NvSuccess)
    {
        return Status;
    }

    NvOsMemcpy((char*)&pState->pInfo->pHeader->ChunkId, &wavHeaderbuff[ReadBytesCount], sizeof(NvU32));
    ReadBytesCount += 4;
    //to identify "RIFF"
    if(pState->pInfo->pHeader->ChunkId != RIFF_TAG)
    {
        return NvError_ParserFailure;
    }

    NvOsMemcpy((char*)&pState->pInfo->pHeader->ChunkSize, &wavHeaderbuff[ReadBytesCount], sizeof(NvU32));
    ReadBytesCount += 4;

    NvOsMemcpy((char*)&pState->pInfo->pHeader->Format, &wavHeaderbuff[ReadBytesCount], sizeof(NvU32));
    ReadBytesCount += 4;
     //to identify "WAVE"
    if(pState->pInfo->pHeader->Format != WAVE_TAG)
    {
        return NvError_ParserFailure;
    }

    NvOsMemcpy((char*)&pState->pInfo->pHeader->SubChunk1Id, &wavHeaderbuff[ReadBytesCount], sizeof(NvU32));
    ReadBytesCount += 4;
    //to identify "fmt"
    if(pState->pInfo->pHeader->SubChunk1Id != FMT_TAG)
    {
        return NvError_ParserFailure;
    }

    NvOsMemcpy((char*)&pState->pInfo->pHeader->SubChunk1Size, &wavHeaderbuff[ReadBytesCount], sizeof(NvU32));
    ReadBytesCount += 4;
    ExtendedFormatlength = pState->pInfo->pHeader->SubChunk1Size;

    NvOsMemcpy((char*)&pState->pInfo->pHeader->AudioFormat, &wavHeaderbuff[ReadBytesCount],2);
    ReadBytesCount += 2;
    if(!(pState->pInfo->pHeader->AudioFormat == WAVE_FORMAT_PCM) &&
        !(pState->pInfo->pHeader->AudioFormat == WAVE_FORMAT_ADPCM) &&
        !(pState->pInfo->pHeader->AudioFormat == WAVE_FORMAT_ALAW) &&
        !(pState->pInfo->pHeader->AudioFormat == WAVE_FORMAT_MULAW) )
    {
        return NvError_UnSupportedStream;
    }

    NvOsMemcpy((char*)&pState->pInfo->pHeader->NumChannels, &wavHeaderbuff[ReadBytesCount], sizeof(NvU16));
    ReadBytesCount += 2;

    NvOsMemcpy((char*)&pState->pInfo->pHeader->SampleRate, &wavHeaderbuff[ReadBytesCount], sizeof(NvU32));
    ReadBytesCount += 4;
    if ((pState->pInfo->pHeader->SampleRate < 8000) || (pState->pInfo->pHeader->SampleRate > 48000))
    {
        return NvError_UnSupportedStream;
    }
    NvOsMemcpy((char*)&pState->pInfo->pHeader->Byterate, &wavHeaderbuff[ReadBytesCount], sizeof(NvU32));
    ReadBytesCount += 4;

    NvOsMemcpy((char*)&pState->pInfo->pHeader->BlockAlign, &wavHeaderbuff[ReadBytesCount], sizeof(NvU16));
    ReadBytesCount += 2;

    NvOsMemcpy((char*)&pState->pInfo->pHeader->BitsPerSample, &wavHeaderbuff[ReadBytesCount], sizeof(NvU16));
    ReadBytesCount += 2;
    ExtendedFormatlength  -= 16;

    if((pState->pInfo->pHeader->AudioFormat != WAVE_FORMAT_ADPCM) && (pState->pInfo->pHeader->SubChunk1Size == 18))
    {
        NvOsMemcpy((char*)&wExtSize, &wavHeaderbuff[ReadBytesCount], sizeof(NvU16));
        ReadBytesCount += 2;
    }

    if(pState->pInfo->pHeader->AudioFormat == WAVE_FORMAT_ADPCM)
    {
        if (ExtendedFormatlength > 0)
        {
            if (ExtendedFormatlength >= 2)
            {
                NvOsMemcpy((char*)&wExtSize, &wavHeaderbuff[ReadBytesCount], sizeof(NvU16));
                ReadBytesCount += 2;
                ExtendedFormatlength -= 2;
            }
            if ((wExtSize > ExtendedFormatlength) ||(wExtSize < 4) || (pState->pInfo->pHeader->BitsPerSample != 4))
            {
                return NvError_UnSupportedStream;
            }

            NvOsMemcpy((char*)&samplesPerBlock, &wavHeaderbuff[ReadBytesCount], sizeof(NvU16));
            ReadBytesCount += 2;

            bytesPerBlock = AdpcmBytesPerBlock(pState->pInfo->pHeader->NumChannels, samplesPerBlock);
            if(bytesPerBlock > pState->pInfo->pHeader->BlockAlign)
            {
                return NvError_UnSupportedStream;
            }

            NvOsMemcpy((char*)&nCoefs, &wavHeaderbuff[ReadBytesCount], sizeof(NvU16));
            ReadBytesCount += 2;

            if (nCoefs < 7 || nCoefs > 0x100)
            {
                return NvError_UnSupportedStream;
            }
            NV_ASSERT(nCoefs <= 7);

            ExtendedFormatlength -= 4;

            if (wExtSize < 4 + 4* nCoefs)
            {
                return NvError_UnSupportedStream;
            }

            NvOsMemcpy((char*)wavICoefs, &wavHeaderbuff[ReadBytesCount],28);
            ReadBytesCount += 28;
        }
        else
        {
            nCoefs = 7;
            samplesPerBlock = (((pState->pInfo->pHeader->BlockAlign - (7 * pState->pInfo->pHeader->NumChannels)) * 8)/
                (pState->pInfo->pHeader->BitsPerSample * pState->pInfo->pHeader->NumChannels)) + 2;
        }
    }

    ReadBytesCount += InitialSeek;
    Status = pState->pPipe->cpipe.SetPosition(pState->cphandle, ReadBytesCount, CP_OriginBegin);
    if (Status != NvSuccess)
    {
        return Status;
    }

    Status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte *)&pState->pInfo->pHeader->SubChunk2Id, 4);
    if (Status != NvSuccess)
    {
        return Status;
    }
    ReadBytesCount += 4;

    //to search "DATA chunk"
    while((pState->pInfo->pHeader->SubChunk2Id != DATA_TAG) && (ReadBytesCount < FileSize))
    {
        Status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte *)&pState->pInfo->pHeader->tempChunkSize, 4);
        if (Status != NvSuccess)
        {
            return Status;
        }
        ReadBytesCount += (4 + pState->pInfo->pHeader->tempChunkSize);

        Status = pState->pPipe->cpipe.SetPosition(pState->cphandle, ReadBytesCount, CP_OriginBegin);
        if (Status != NvSuccess)
        {
            return Status;
        }

        Status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte *)&pState->pInfo->pHeader->SubChunk2Id, 4);
        if (Status != NvSuccess)
        {
            return Status;
        }
        ReadBytesCount += 4;
    }

    if(pState->pInfo->pHeader->SubChunk2Id != DATA_TAG)
    {
        return NvError_UnSupportedStream;
    }

    Status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte *)&pState->pInfo->pHeader->SubChunk2Size, 4);
    if (Status != NvSuccess)
    {
        return Status;
    }
    ReadBytesCount += 4;

    pState->dataOffset = ReadBytesCount;

    Status = pState->pPipe->cpipe.Read(pState->cphandle, (CPbyte *)&pState->pInfo->pHeader->SubChunk2Size, 4);
    if (Status != NvSuccess)
    {
        return Status;
    }
    ReadBytesCount += 4;

    pState->dataOffset = ReadBytesCount;

    switch(pState->pInfo->pHeader->AudioFormat)
    {
        case WAVE_FORMAT_PCM:
            pState->pInfo->pTrackInfo->sampleSize = pState->pInfo->pHeader->BitsPerSample;
            pState->pInfo->pTrackInfo->nCoefs = 0;
            pState->pInfo->pTrackInfo->uDataLength = pState->pInfo->pHeader->SubChunk2Size;
            pState->pInfo->pTrackInfo->samplesPerBlock = 0;
            break;
        case WAVE_FORMAT_ADPCM:
        case WAVE_FORMAT_ALAW:
        case WAVE_FORMAT_MULAW:
            pState->pInfo->pTrackInfo->sampleSize = 16;
            pState->pInfo->pTrackInfo->nCoefs = nCoefs;
            pState->pInfo->pTrackInfo->uDataLength = pState->pInfo->pHeader->SubChunk2Size;
            pState->pInfo->pTrackInfo->samplesPerBlock = samplesPerBlock;
            break;
        default:
            return NvError_UnSupportedStream;
    }
    pState->pInfo->pTrackInfo->structSize = 0;
    pState->pInfo->pTrackInfo->frameSize = 0;
    pState->pInfo->pTrackInfo->streamIndex = 0;
    pState->pInfo->pTrackInfo->containerSize = 0;
    pState->pInfo->pTrackInfo->blockAlign = pState->pInfo->pHeader->BlockAlign;
    pState->bitRate = pState->pInfo->pHeader->Byterate *8;
    pState->pInfo->pTrackInfo->samplingFreq = pState->pInfo->pHeader->SampleRate;
    pState->pInfo->pTrackInfo->noOfChannels = pState->pInfo->pHeader->NumChannels;
    pState->pInfo->pTrackInfo->audioFormat = pState->pInfo->pHeader->AudioFormat;
    pState->total_time = FileSize/pState->pInfo->pHeader->Byterate;

    Status = pState->pPipe->cpipe.SetPosition(pState->cphandle,pState->dataOffset, CP_OriginBegin);

    return Status;
}

NvU32 AdpcmBytesPerBlock(NvU16 chans, NvU16 samplesPerBlock )
{
    NvU32  n;
    n = 7*chans;  /* header */
    if (samplesPerBlock > 2)
            n += (((NvU32)samplesPerBlock-2)*chans + 1)/2;
    return n;
}

