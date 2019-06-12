
/**
* @nvmm_mpsparser.c
* @brief IMplementation of mps parser Class.
*
* @b Description: Implementation of mps parser API's.
*
*/

/*
* Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto. Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

#include "nvmm_mps_parser.h"
//#include "nvos.h"

/* MP2/MP3 related tables */
static const NvU32 NvMpsParserMp3SampleRates[4][3] = {
    { 11025, 12000, 8000  }, // MPEG 2.5
    { 0,     0,     0     }, // reserved
    { 22050, 24000, 16000 }, // MPEG 2
    { 44100, 48000, 32000 }  // MPEG 1
};

static const NvU32 NvMpsParserMp3Bitrates[2][3][15] = {
    { // MPEG 1
        { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448 }, // Layer 1
        { 0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384 }, // Layer 2
        { 0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320 }  // Layer 3
    },
    { // MPEG 2, 2.5
        { 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256 }, // Layer 1
        { 0,  8, 16, 24, 32, 40, 48,  56,  64,  80,  96, 112, 128, 144, 160 }, // Layer 2
        { 0,  8, 16, 24, 32, 40, 48,  56,  64,  80,  96, 112, 128, 144, 160 }  // Layer 4
    }
};


static NvError NvMpsParserParseAudioProperties(NvMpsParser *pMpsParser, NvMpsParserAvInfo *pAvInfo)
{
    NvMpsReader *pReader = pMpsParser->pReader;
    NvMpsPESHead  *pPESHead = pReader->pPESHead;
    NvMpsPackHead *pPackHead = pReader->pPackHead;
    NvError status;
    eNvMpsResult eRes;
    NvU32 uSize;
    NvU8 *pBuf;
    NvU32 uVer;
    NvU32 uLayerIndex;
    NvU32 uBitRateIndex;
    NvU32 uChIndex;
    NvU32 uSampFreqIndex;
    NvU32 uCount = 0;
    NvU8  bMPASyncFound = 0;

    pAvInfo->uAudioStreamID = pPESHead->uStreamID;


    if (pPESHead->bPtsValid)
    {
        pAvInfo->bFirstAudioPTSAccurate = NV_TRUE;
        pAvInfo->lFirstAudioPTS = pPESHead->lPts;
    }
    else
    {
        pAvInfo->bFirstAudioPTSAccurate = NV_FALSE;
        pAvInfo->lFirstAudioPTS = pPackHead->lScr; //Use last SCR as PTS!
    }

    if (pAvInfo->bHasVideo)
    {  /* Video stream was already found.
        * FirstPTS would be initialized with video
        * Set FirstPTS depending most accurate and least value
        */
        if (pAvInfo->bFirstPTSAccurate)
        {
            if (pAvInfo->bFirstAudioPTSAccurate &&
                (pAvInfo->lFirstAudioPTS < pAvInfo->lFirstPTS))
            {
                pAvInfo->lFirstPTS = pAvInfo->lFirstAudioPTS;
            }
        }
        else
        {
            if (pAvInfo->bFirstAudioPTSAccurate)
            {
                pAvInfo->lFirstPTS = pAvInfo->lFirstAudioPTS;
                pAvInfo->bFirstAudioPTSAccurate = NV_TRUE;
            }
            else if (pAvInfo->lFirstAudioPTS < pAvInfo->lFirstPTS)
            {
                pAvInfo->lFirstPTS = pAvInfo->lFirstAudioPTS;
            }
        }
    }
    else
    {
        pAvInfo->bFirstPTSAccurate = pAvInfo->bFirstAudioPTSAccurate;
        pAvInfo->lFirstPTS = pAvInfo->lFirstAudioPTS;
    }

    /* First set position to start of PES Data */
    status = pReader->SetReaderPosition(pReader, &eRes, pPESHead->lFileOffsetForData);
    if (status != NvSuccess)
    {
        return status;
    }
    else if (eRes != NV_MPS_RESULT_OK)
    {
        return NvError_ParserFailure; //EOF is not expected so early
    }

    uSize = pPESHead->uDataSize;
    if (uSize > (NV_MPS_MAX_SCRATCH_SIZE - NV_MPS_SYNC_MARKER_SIZE))
    {
        /* We can not read entire PES Data into scratch buffer.
         * For the purpose of searching audio header, initial part of PES Data should be fine.
         */
        uSize = (NV_MPS_MAX_SCRATCH_SIZE - NV_MPS_SYNC_MARKER_SIZE);
    }

    /* Now make sure we have uSize data in scratch */
    if (pReader->uUnParsedScratchSize < uSize)
    {
        status = pReader->EnsureDataAvailability(pReader, &eRes, uSize);
        if (status != NvSuccess)
        {
            return status;
        }
        else if (eRes != NV_MPS_RESULT_OK)
        {
            return NvError_ParserFailure; //EOF is not expected so early
        }
    }

    pBuf = pReader->pScratchBufPtr;

     //skip till 0xFFE0 is found in bitstream
     while ((uCount+4) < uSize)
     {
           if ((pBuf[uCount] == 0xFF) && ((pBuf[uCount+1] & 0xE0) == 0xE0))
           {
             bMPASyncFound = 1;
             pBuf = pBuf + uCount;
             break;
           }
           uCount++;
     }
     if (bMPASyncFound)
     {
        uVer = (pBuf[1] & 0x18) >> 3;
        uLayerIndex = (pBuf[1] & 0x6) >> 1;
        uBitRateIndex = (pBuf[2] & 0xF0) >> 4;
        uSampFreqIndex = (pBuf[2] & 0x0C) >> 2;
        uChIndex = (pBuf[3] & 0xC0) >> 6;

        if (uLayerIndex >= 2)
        {
            pAvInfo->eAudioType = NV_MPS_AUDIO_MP2;
        }
        else
        {
            pAvInfo->eAudioType = NV_MPS_AUDIO_MP3;
        }
        if (uChIndex == 3)
        {
            pAvInfo->uNumChannels = 1;
        }
        else
        {
            pAvInfo->uNumChannels = 2;
        }
        pAvInfo->uSamplingFreq = NvMpsParserMp3SampleRates[uVer][uSampFreqIndex];
        pAvInfo->uAudioBitRate = NvMpsParserMp3Bitrates[uVer != 3][3 - uLayerIndex][uBitRateIndex] * 1000;
    }
    else
    {
        pAvInfo->eAudioType = NV_MPS_AUDIO_UNKNOWN;
        pAvInfo->uAudioBitRate = 128000;
        pAvInfo->uNumChannels = 2;
        pAvInfo->uSamplingFreq = 44100;
    }
    pAvInfo->uBitsPerSample = 16;

    return NvSuccess;
}

static NvError NvMpsParserParseVideoProperties(NvMpsParser *pMpsParser, NvMpsParserAvInfo *pAvInfo)
{
    NvMpsReader *pReader = pMpsParser->pReader;
    NvMpsPESHead  *pPESHead = pReader->pPESHead;
    NvMpsPackHead *pPackHead = pReader->pPackHead;
    eNvMpsResult eRes;
    NvError status;
    NvU32 uTemp;
    NvU32 uSize;
    NvU32 uCount;
    NvU8 *pBuf;

    pAvInfo->uVideoStreamID = pPESHead->uStreamID;

    if (pPESHead->bPtsValid)
    {
        pAvInfo->bFirstVideoPTSAccurate = NV_TRUE;
        pAvInfo->lFirstVideoPTS = pPESHead->lPts;
    }
    else
    {
        pAvInfo->bFirstVideoPTSAccurate = NV_FALSE;
        pAvInfo->lFirstVideoPTS = pPackHead->lScr; //Use last SCR as PTS!
    }

    if (pAvInfo->bHasAudio)
    {  /* Audio stream was already found.
        * FirstPTS would be initialized with audio
        * Set FirstPTS depending most accurate and least value
        */
        if (pAvInfo->bFirstPTSAccurate)
        {
            if (pAvInfo->bFirstVideoPTSAccurate &&
                (pAvInfo->lFirstVideoPTS < pAvInfo->lFirstPTS))
            {
                pAvInfo->lFirstPTS = pAvInfo->lFirstVideoPTS;
            }
        }
        else
        {
            if (pAvInfo->bFirstVideoPTSAccurate)
            {
                pAvInfo->lFirstPTS = pAvInfo->lFirstVideoPTS;
                pAvInfo->bFirstVideoPTSAccurate = NV_TRUE;
            }
            else if (pAvInfo->lFirstVideoPTS < pAvInfo->lFirstPTS)
            {
                pAvInfo->lFirstPTS = pAvInfo->lFirstVideoPTS;
            }
        }
    }
    else
    {
        pAvInfo->bFirstPTSAccurate = pAvInfo->bFirstVideoPTSAccurate;
        pAvInfo->lFirstPTS = pAvInfo->lFirstVideoPTS;
    }

    //TODO: Find exact video type - MPEG-1, MPEG-2, MPEG-4(?), AVC(?)
    pAvInfo->eVideoType = NV_MPS_VIDEO_MPEG_2;

    /* Parse video sequence header now */
    /* First set position to start of PES Data */
    status = pReader->SetReaderPosition(pReader, &eRes, pPESHead->lFileOffsetForData);
    if (status != NvSuccess)
    {
        return status;
    }
    else if (eRes != NV_MPS_RESULT_OK)
    {
        return NvError_ParserFailure; //EOF is not expected so early
    }

    uSize = pPESHead->uDataSize;
    if (uSize > (NV_MPS_MAX_SCRATCH_SIZE - NV_MPS_SYNC_MARKER_SIZE))
    {
        /* We can not read entire PES Data into scratch buffer.
         * For the purpose of searching sequence header, initial part of PES Data should be fine.
         */
        uSize = (NV_MPS_MAX_SCRATCH_SIZE - NV_MPS_SYNC_MARKER_SIZE);
    }

    /* Now make sure we have uSize data in scratch */
    if (pReader->uUnParsedScratchSize < uSize)
    {
        status = pReader->EnsureDataAvailability(pReader, &eRes, uSize);
        if (status != NvSuccess)
        {
            return status;
        }
        else if (eRes != NV_MPS_RESULT_OK)
        {
            return NvError_ParserFailure; //EOF is not expected so early
        }
    }

    pBuf = pReader->pScratchBufPtr;

    for (uCount = 0; uCount < uSize; uCount++)
    {
        if ((pBuf[0] == 0) &&
            (pBuf[1] == 0) &&
            (pBuf[2] == 1))
        {
            break;
        }
        pBuf++;
    }

    if ((uCount < uSize) &&
        (pBuf[3] == NV_MPS_VSEQ_HEADER_CODE))
    {
        pAvInfo->uWidth = (((NvU32)pBuf[4]) << 4) |
                           ((((NvU32)pBuf[5]) & 0xF0) >> 4);
        pAvInfo->uHeight = ((((NvU32)pBuf[5]) & 0x0F) << 8) |
                            ((NvU32)pBuf[6]);

        //TODO: Aspect Raio is different for MPEG-1
        uTemp = (pBuf[7] >> 4);
        switch(uTemp)
        {
        case 1: //1:1
            pAvInfo->uAsrWidth = 1;
            pAvInfo->uAsrHeight = 1;
            break;
        case 2: //4:3
            pAvInfo->uAsrWidth = 4;
            pAvInfo->uAsrHeight = 3;
            break;
        case 3: //16:9
            pAvInfo->uAsrWidth = 16;
            pAvInfo->uAsrHeight = 9;
            break;
        case 4: //2.21:1
            pAvInfo->uAsrWidth = 221;
            pAvInfo->uAsrHeight = 100;
            break;
        default:
            pAvInfo->uAsrWidth = 1;
            pAvInfo->uAsrHeight = 1;
            break;
        }

        uTemp = (pBuf[7] & 0x0F);
        switch(uTemp)
        {
        case 1: //23.976
            pAvInfo->uFps = 23976;
            break;
        case 2: //24
            pAvInfo->uFps = 24000;
            break;
        case 3: //25
            pAvInfo->uFps = 25000;
            break;
        case 4: //29.970
            pAvInfo->uFps = 29970;
            break;
        case 5: //30
            pAvInfo->uFps = 30000;
            break;
        case 6: //50
            pAvInfo->uFps = 50000;
            break;
        case 7: //59.940
            pAvInfo->uFps = 59940;
            break;
        case 8: //60
            pAvInfo->uFps = 60000;
            break;
        default: //30
            pAvInfo->uFps = 30000;
            break;
        }

        //TODO: Video bit rate
        pAvInfo->bVideoPropertiesParsed = NV_TRUE;
    }

    return NvSuccess;
}

static NvError NvMpsParserFindAvInfo  (NvMpsParser *pMpsParser,
                                 NvMpsParserAvInfo *pAvInfo,
                                 NvU32 uLimit)
{

    NvMpsReader *pReader = pMpsParser->pReader;
    eNvMpsResult eRes;
    NvError status;
    NvU64 lLimitParseOffSet;


    /* Set reader position to beginning of the file */
    status = pReader->SetReaderPosition(pReader, &eRes, 0);
    if (status != NvSuccess)
    {
        return status;
    }
    else if (eRes != NV_MPS_RESULT_OK)
    {
        return NvError_ParserFailure;
    }

    /* Find first pack */
    status = pReader->SeekNextPackCode(pReader, &eRes, NV_MPS_MAX_PACK_SEARCH_SIZE);
    if (status != NvSuccess)
    {
        return status;
    }
    else if(eRes != NV_MPS_RESULT_OK)
    {
        return NvError_ParserFailure;
    }

    status = pReader->PeekParsePackHead(pReader, &eRes, NV_TRUE);
    if (status != NvSuccess)
    {
        return status;
    }
    else if (eRes != NV_MPS_RESULT_OK)
    {
        return NvError_ParserFailure; //End of file reached
    }

    NvOsMemcpy(pMpsParser->pFirstPackHead, pReader->pPackHead, sizeof(NvMpsPackHead));

    /* Initialize lFirstPESFileOffset to max value */
    pMpsParser->lFirstPESFileOffset = pReader->lFileSize;
    lLimitParseOffSet = pReader->lCurrParseOffSet + uLimit;

    while (1)
    {
        status = pReader->SeekNextPESCode(pReader, &eRes, uLimit);
        if ((status != NvSuccess) || (eRes != NV_MPS_RESULT_OK))
        {
            return status;
        }

        /* One of the known stream ID */
        status = pReader->PeekParsePESHead(pReader, &eRes, NV_TRUE);
        if ((status != NvSuccess) || (eRes == NV_MPS_RESULT_END_OF_FILE))
        {
            return status;
        }
        else if (eRes == NV_MPS_RESULT_OK)
        {
            if ((pAvInfo->bHasVideo == NV_FALSE) &&
                (pReader->pPESHead->uStreamID == NV_MPS_STREAM_ID_VIDEO))
            {
                pAvInfo->bHasVideo = NV_TRUE;
                pMpsParser->uNumStreams++;
                pMpsParser->lFirstVideoPESFileOffset = pReader->pPESHead->lFileOffsetForHead;
                pMpsParser->lLastVideoPESFileOffset = pMpsParser->lFirstVideoPESFileOffset;

                if (pMpsParser->lFirstPESFileOffset > pMpsParser->lFirstVideoPESFileOffset)
                {
                    pMpsParser->lFirstPESFileOffset = pMpsParser->lFirstVideoPESFileOffset;
                    pMpsParser->lLastPESFileOffset =  pMpsParser->lFirstPESFileOffset;
                }

                status = pMpsParser->ParseVideoProperties(pMpsParser, pAvInfo);
                if (status != NvSuccess)
                {
                    return status;
                }
            }
            else if ((pAvInfo->bHasAudio == NV_FALSE) &&
                 ((pReader->pPESHead->uStreamID == NV_MPS_STREAM_ID_AUDIO) ||
                  (pReader->pPESHead->uStreamID == NV_MPS_STREAM_ID_AUDIO_EXT)))
            {
                pAvInfo->bHasAudio = NV_TRUE;
                pMpsParser->uNumStreams++;
                pMpsParser->lFirstAudioPESFileOffset = pReader->pPESHead->lFileOffsetForHead;
                pMpsParser->lLastAudioPESFileOffset = pMpsParser->lFirstAudioPESFileOffset;

                if (pMpsParser->lFirstPESFileOffset > pMpsParser->lLastAudioPESFileOffset)
                {
                    pMpsParser->lFirstPESFileOffset = pMpsParser->lLastAudioPESFileOffset;
                    pMpsParser->lLastPESFileOffset =  pMpsParser->lFirstPESFileOffset;
                }

                status = pMpsParser->ParseAudioProperties(pMpsParser, pMpsParser->pAvInfo);
                if (status != NvSuccess)
                {
                    return status;
                }
            }
            /* Skip this PES Data */
            status = pReader->SetReaderPosition(pReader, &eRes,
                        pReader->pPESHead->lFileOffsetForData + pReader->pPESHead->uDataSize);
            if (status != NvSuccess)
            {
                return status;
            }
            else if (eRes != NV_MPS_RESULT_OK)
            {
                return NvSuccess; //EOF found
            }
        }


        if (pAvInfo->bHasVideo && pAvInfo->bHasAudio)
        {
            return NvSuccess;
        }

        if (pReader->lCurrParseOffSet >= lLimitParseOffSet)
        {
            return NvSuccess;
        }

        uLimit = (lLimitParseOffSet - pReader->lCurrParseOffSet);
    }
}

static NvError NvMpsParserFindDuration(NvMpsParser *pMpsParser, NvMpsParserAvInfo *pAvInfo)
{
    NvMpsReader *pReader = pMpsParser->pReader;
    NvMpsPESHead  *pPESHead =  pReader->pPESHead;
    NvMpsPackHead *pPackHead = pReader->pPackHead;
    eNvMpsResult eRes;
    NvError status;
    NvU64 lFileOffset;
    NvU32 lOffsetFromEnd;
    NvU32 uCount;
    NvBool bFound = NV_FALSE;
    NvU64 lLastPTS = 0;
    NvU64 lTemp;


    lOffsetFromEnd = NV_MPS_SYNC_MARKER_SIZE;

    for (uCount = 0; uCount < NV_MPS_DURATION_SEARCH_COUNT; uCount++)
    {
        lOffsetFromEnd += NV_MPS_MAX_SCRATCH_SIZE;

        if (pReader->lFileSize < lOffsetFromEnd)
        {
            break;
        }

        lFileOffset = pReader->lFileSize - lOffsetFromEnd;
        status = pReader->SetReaderPosition(pReader, &eRes, lFileOffset);
        if (status != NvSuccess)
        {
            return status;
        }
        else if (eRes != NV_MPS_RESULT_OK)
        {
            return NvError_ParserFailure; //EOF is not expected here
        }

        status = pReader->ReadForward(pReader, &eRes, 0, NULL);
        if (status != NvSuccess)
        {
            return status;
        }
        else if (eRes != NV_MPS_RESULT_OK)
        {
            return NvError_ParserFailure; //EOF is not expected here
        }

        bFound = NV_FALSE;
        while(1)
        {
            status = pReader->SeekNextSyncCodeInScratch(pReader, &eRes, NV_MPS_MAX_SCRATCH_SIZE);
            if (status != NvSuccess)
            {
                return status;
            }
            else if (eRes != NV_MPS_RESULT_OK)
            {
                break; //while
            }

            if (pReader->pScratchBufPtr[0] == NV_MPS_PACK_HEADER_CODE)
            {
                status = pReader->PeekParsePackHead(pReader, &eRes, NV_TRUE);
                if (status != NvSuccess)
                {
                    return status;
                }
                else
                {
                    if ((eRes == NV_MPS_RESULT_OK) &&
                        (!bFound || !pAvInfo->bLastPTSAccurate))
                    {
                        bFound = NV_TRUE;
                        lLastPTS = pPackHead->lScr;
                        pAvInfo->bLastPTSAccurate = NV_FALSE;
                    }
                    else
                    {
                        break; //while
                    }
                }
            }
            else if (pReader->pScratchBufPtr[0] == NV_MPS_SYS_HEADER_CODE)
            {
                status = pReader->PeekParseSysHead(pReader, &eRes, NV_TRUE);
                if (status != NvSuccess)
                {
                    return status;
                }
                else if (eRes != NV_MPS_RESULT_OK)
                {
                    break; //while
                }
            }
            else if ((pAvInfo->bHasAudio &&
                     (pReader->pScratchBufPtr[0] == pAvInfo->uAudioStreamID)) ||
                     (pAvInfo->bHasVideo &&
                     (pReader->pScratchBufPtr[0] == pAvInfo->uVideoStreamID)))
            {
                status = pReader->PeekParsePESHead(pReader, &eRes, NV_TRUE);
                if (status != NvSuccess)
                {
                    return status;
                }
                else
                {
                    if (eRes == NV_MPS_RESULT_OK)
                    {
                        if (pPESHead->bPtsValid)
                        {
                            bFound = NV_TRUE;
                            lLastPTS = pPESHead->lPts;
                            pAvInfo->bLastPTSAccurate = NV_TRUE;
                        }
                        /* Skip this PES Data if it is completely in scratch buffer*/
                        lTemp = pPESHead->lFileOffsetForData + pPESHead->uDataSize;
                        if (lTemp < pReader->lCurrFileOffSet)
                        {
                            status = pReader->SetReaderPosition(pReader, &eRes, lTemp);
                            if (status != NvSuccess)
                            {
                                return status;
                            }
                        }
                        else
                        {
                            break; //while
                        }
                    }
                    else
                    {
                        break; //while
                    }
                }
            }
        }

        if (bFound)
        {
            break; //For loop
        }
    }

    if (bFound)
    {
        pAvInfo->bLastPTSFound = NV_TRUE;
        pAvInfo->lLastPTS = lLastPTS;
        if (pAvInfo->lLastPTS > pAvInfo->lFirstPTS)
        {
            pMpsParser->lDuration = pAvInfo->lLastPTS - pAvInfo->lFirstPTS;
        }

        if (pMpsParser->lDuration)
        {
            pMpsParser->lAvgBitRate = (pReader->lFileSize * 90000 * 8)/pMpsParser->lDuration;
        }
    }

    return NvSuccess;
}

void NvMpsParserDestroy(NvMpsParser *pMpsParser)
{
    if (pMpsParser == NULL)
    {
        return;
    }

    if (pMpsParser->pReader)
    {
        NvMpsReaderDestroy(pMpsParser->pReader);
    }
    if (pMpsParser->pFirstPackHead)
    {
        NvOsFree(pMpsParser->pFirstPackHead);
    }
    if (pMpsParser->pAvInfo)
    {
        NvOsFree(pMpsParser->pAvInfo);
    }

    NvOsFree(pMpsParser);
}


NvMpsParser *NvMpsParserCreate(NvU32 uScratchBufMaxSize)
{
    NvMpsParser *pMpsParser;

    pMpsParser = (NvMpsParser *)NvOsAlloc(sizeof(NvMpsParser));
    if (pMpsParser == NULL)
        goto NvMpsParserMemFailure;
    NvOsMemset(pMpsParser, 0, sizeof(NvMpsParser));

    pMpsParser->pAvInfo = (NvMpsParserAvInfo *)NvOsAlloc(sizeof(NvMpsParserAvInfo));
    if (pMpsParser->pAvInfo == NULL)
        goto NvMpsParserMemFailure;
    NvOsMemset(pMpsParser->pAvInfo, 0, sizeof(NvMpsParserAvInfo));

    pMpsParser->pFirstPackHead = (NvMpsPackHead *)NvOsAlloc(sizeof(NvMpsPackHead));
    if (pMpsParser->pFirstPackHead == NULL)
        goto NvMpsParserMemFailure;

    pMpsParser->pReader = NvMpsReaderCreate(uScratchBufMaxSize);
    if (pMpsParser->pReader == NULL)
        goto NvMpsParserMemFailure;

    pMpsParser->FindAvInfo = NvMpsParserFindAvInfo;
    pMpsParser->ParseAudioProperties = NvMpsParserParseAudioProperties;
    pMpsParser->ParseVideoProperties = NvMpsParserParseVideoProperties;
    pMpsParser->FindDuration = NvMpsParserFindDuration;

    return pMpsParser;

NvMpsParserMemFailure:
    NvMpsParserDestroy(pMpsParser);
    return NULL;
}

NvError NvMpsParserInit(NvMpsParser *pMpsParser, CP_PIPETYPE *pPipe, CPhandle cphandle)
{
    NvMpsReader *pReader = pMpsParser->pReader;
    NvMpsParserAvInfo *pAvInfo = pMpsParser->pAvInfo;
    NvError status;
    eNvMpsResult eRes;

    /* Init Reader first */
    status = pReader->Init(pReader, pPipe, cphandle);
    if (status != NvSuccess)
    {
        return status;
    }

    pMpsParser->uNumStreams = 0;

    /* Find Audio and Video stream information */
    status = pMpsParser->FindAvInfo(pMpsParser, pAvInfo, NV_MPS_MAX_AVINFO_SEARCH_SIZE);
    if (status != NvSuccess)
    {
        return status;
    }

    if (pMpsParser->uNumStreams == 0)
    {
#if NV_MSP_DEBUG_PRINT
        NvOsDebugPrintf("No Playable AV streams found!\n");
#endif
        return NvError_ParserFailure;
    }

    /* Find stream duration and avg bitrate*/
    status = pMpsParser->FindDuration(pMpsParser, pAvInfo);
    if (status != NvSuccess)
    {
        return status;
    }

    /* Set reader position to first PES packet*/
    status = pReader->SetReaderPosition(pReader, &eRes, pMpsParser->lFirstPESFileOffset);
    if (status != NvSuccess)
    {
        return status;
    }
    else if (eRes != NV_MPS_RESULT_OK)
    {
        return NvError_ParserFailure; //EOF is not expected so early
    }

#if NV_MSP_DEBUG_PRINT
    NvOsDebugPrintf("\nNumber of Streams = %d\n", pMpsParser->uNumStreams);
    NvOsDebugPrintf("Duration = %f\n\n", pMpsParser->lDuration/(float)90000);
    if(pAvInfo->bHasVideo)
    {
        NvOsDebugPrintf("Video Found!\n");
        NvOsDebugPrintf("\tResolution: %d x %d\n", pAvInfo->uWidth, pAvInfo->uHeight);
        NvOsDebugPrintf("\tPAR %d : %d\n", pAvInfo->uAsrWidth, pAvInfo->uAsrHeight);
        NvOsDebugPrintf("\tFPS = %f\n", pAvInfo->uFps/(float)1000);
    }
    else
    {
        NvOsDebugPrintf("Video Not found\n");
    }
    if(pAvInfo->bHasAudio)
    {
        NvOsDebugPrintf("Audio Found!\n");
        NvOsDebugPrintf("\tAudio Type = %s\n", (pAvInfo->eAudioType == NV_MPS_MPEG_1)? "MPEG-1" : "MPEG-2");
    }
    else
    {
        NvOsDebugPrintf("Audio Not found\n");
    }
#endif
    return NvSuccess;
}

NvError NvMpsParserGetNextPESInfo(NvMpsParser *pMpsParser, eNvMpsMediaType *peMediaType, NvU32 *puDataSize)
{
    NvMpsReader *pReader = pMpsParser->pReader;
    NvMpsPESHead *pPESHead  = pReader->pPESHead;
    NvMpsParserAvInfo *pAvInfo = pMpsParser->pAvInfo;
    NvError status;
    eNvMpsResult eRes;
    NvU32 uLimit = NV_MPS_MAX_PES_SEARCH_SIZE;
    NvU64 lLimitParseOffSet = pReader->lCurrParseOffSet + uLimit;
    NvU64 lStartPosition;

    if (*peMediaType == NV_MPS_STREAM_ANY)
    {
        if (pMpsParser->bAnyEOS)
        {
            return NvError_ParserEndOfStream;
        }

        /*Start position should be minimum of Video and Audio */
        pMpsParser->lLastPESFileOffset = pMpsParser->lLastAudioPESFileOffset;
        if (pMpsParser->lLastPESFileOffset > pMpsParser->lLastVideoPESFileOffset)
            pMpsParser->lLastPESFileOffset = pMpsParser->lLastVideoPESFileOffset;

        lStartPosition = pMpsParser->lLastPESFileOffset;
    }
    else if (*peMediaType == NV_MPS_STREAM_VIDEO)
    {
        if (pMpsParser->bVideoEOS)
        {
            return NvError_ParserEndOfStream;
        }
        lStartPosition = pMpsParser->lLastVideoPESFileOffset;
    }
    else //AUDIO
    {
        if (pMpsParser->bAudioEOS)
        {
            return NvError_ParserEndOfStream;
        }
        lStartPosition = pMpsParser->lLastAudioPESFileOffset;
    }


    if (lStartPosition != pReader->lCurrParseOffSet)
    {
        status = pReader->SetReaderPosition(pReader, &eRes, lStartPosition);
        if (status != NvSuccess)
        {
            return status;
        }
        else if (eRes != NV_MPS_RESULT_OK)
        {
            return NvError_ParserFailure; //EOF is not expected
        }
    }

    *puDataSize = 0;

    while (1)
    {
        status = pReader->SeekNextPESCode(pReader, &eRes, uLimit);
        if (status != NvSuccess)
        {
            return status;
        }
        else if (eRes == NV_MPS_RESULT_END_OF_FILE)
        {
            break;
        }
        else if (eRes == NV_MPS_RESULT_SYNC_NOT_FOUND)
        {
            return NvError_ParserCorruptedStream;
        }
        // else NV_MPS_RESULT_OK

        /* One of the known stream ID */
        status = pReader->PeekParsePESHead(pReader, &eRes, NV_FALSE);
        if (status != NvSuccess)
        {
            return status;
        }
        else if (eRes == NV_MPS_RESULT_END_OF_FILE)
        {
            break;
        }
        else if (eRes == NV_MPS_RESULT_OK)
        {
            if (pAvInfo->bHasVideo && pAvInfo->uVideoStreamID == pPESHead->uStreamID &&
                *peMediaType != NV_MPS_STREAM_AUDIO)
            {
                    *peMediaType = NV_MPS_STREAM_VIDEO;
                    *puDataSize = pPESHead->uDataSize;
                    pMpsParser->lLastVideoPESFileOffset = pPESHead->lFileOffsetForHead;
                    return NvSuccess;
            }
            else if (pAvInfo->bHasAudio && pAvInfo->uAudioStreamID == pPESHead->uStreamID &&
                    *peMediaType != NV_MPS_STREAM_VIDEO)
            {
                    *peMediaType = NV_MPS_STREAM_AUDIO;
                    *puDataSize = pPESHead->uDataSize;
                    pMpsParser->lLastAudioPESFileOffset = pPESHead->lFileOffsetForHead;
                    return NvSuccess;
            }
            /* Skip this PES Data */
            status = pReader->SetReaderPosition(pReader, &eRes, pPESHead->lFileOffsetForData + pPESHead->uDataSize);
            if (status != NvSuccess)
            {
                return status;
            }
            else if (eRes == NV_MPS_RESULT_END_OF_FILE)
            {
                break;
            }
        }

        if (pReader->lCurrParseOffSet >= lLimitParseOffSet)
        {
            return NvError_ParserCorruptedStream;;
        }

        uLimit = (lLimitParseOffSet - pReader->lCurrParseOffSet);
    }

    /* If we here, it means EOF */
    if (*peMediaType == NV_MPS_STREAM_ANY)
    {
        pMpsParser->bAnyEOS = NV_TRUE;
        pMpsParser->bVideoEOS = NV_TRUE;
        pMpsParser->bAudioEOS = NV_TRUE;
    }
    else if (*peMediaType == NV_MPS_STREAM_VIDEO)
    {
        pMpsParser->bVideoEOS = NV_TRUE;
        if (pMpsParser->bAudioEOS)
            pMpsParser->bAnyEOS = NV_TRUE;
    }
    else //AUDIO
    {
        pMpsParser->bAudioEOS = NV_TRUE;
        if (pMpsParser->bVideoEOS)
            pMpsParser->bAnyEOS = NV_TRUE;
    }
    return NvError_ParserEndOfStream;
}


NvError NvMpsParserGetNextPESData(NvMpsParser *pMpsParser, eNvMpsMediaType *peMediaType,
                       NvU8 *pBuf, NvU32 *puBufSize)
{
    NvMpsReader *pReader = pMpsParser->pReader;
    NvMpsPESHead *pPESHead  = pReader->pPESHead;
    NvError status;
    NvBool bCallGetNextPESInfo = NV_FALSE;
    NvU32 uDataSize;

    if (*peMediaType == NV_MPS_STREAM_ANY)
    {
        if (pMpsParser->bAnyEOS)
        {
            status = NvError_ParserEndOfStream;
        }
        //If we here, it means GetNextPESInfo was not called. Call now
        bCallGetNextPESInfo = NV_TRUE;
    }
    else if (*peMediaType == NV_MPS_STREAM_VIDEO)
    {
        if (pMpsParser->bVideoEOS)
        {
            status = NvError_ParserEndOfStream;
        }
        /* Check if GetNextPESInfo was called. Otherwise call now */
        if (pReader->lCurrParseOffSet != (pMpsParser->lLastVideoPESFileOffset + NV_MPS_SYNC_MARKER_SIZE - 1))
            bCallGetNextPESInfo = NV_TRUE;
    }
    else //AUDIO
    {
        if (pMpsParser->bAudioEOS)
        {
            status = NvError_ParserEndOfStream;
        }
        /* Check if GetNextPESInfo was called. Otherwise call now */
        if (pReader->lCurrParseOffSet != (pMpsParser->lLastVideoPESFileOffset + NV_MPS_SYNC_MARKER_SIZE - 1))
            bCallGetNextPESInfo = NV_TRUE;
    }

    if (bCallGetNextPESInfo)
    {
        status = NvMpsParserGetNextPESInfo(pMpsParser, peMediaType, &uDataSize);
        if (status != NvSuccess)
        {
            return status;
        }
    }

    if (*puBufSize < pPESHead->uDataSize)
    {
        return NvError_InSufficientBufferSize;
    }

    /*Check if request if legal */
    if (pReader->lFileSize <= pPESHead->lFileOffsetForData)
    {
        status = NvError_ParserEndOfStream;
    }
    else
    {
        if (pReader->lFileSize <= (pPESHead->lFileOffsetForData + pPESHead->uDataSize))
        {
            /* This PES data can not be copied completely */
            pPESHead->uDataSize = pReader->lFileSize - pPESHead->lFileOffsetForData;
        }

        /* Copy PES data to external buffer */
        *puBufSize = pPESHead->uDataSize;
        status = pReader->CopyDataToExternalBuffer(pReader, pBuf,
            pPESHead->lFileOffsetForData, puBufSize);
    }
    if (status == NvError_ParserEndOfStream)
    {
        if (*peMediaType == NV_MPS_STREAM_VIDEO)
        {
            pMpsParser->bVideoEOS = NV_TRUE;
            if (pMpsParser->bAudioEOS)
                pMpsParser->bAnyEOS = NV_TRUE;
        }
        else //AUDIO
        {
            pMpsParser->bAudioEOS = NV_TRUE;
            if (pMpsParser->bVideoEOS)
                pMpsParser->bAnyEOS = NV_TRUE;
        }
        return NvError_ParserEndOfStream;
    }
    else if (status == NvSuccess)
    {
        if (*peMediaType == NV_MPS_STREAM_VIDEO)
        {
            pMpsParser->lLastVideoPESFileOffset = pReader->lCurrParseOffSet;
        }
        else //AUDIO
        {
            pMpsParser->lLastAudioPESFileOffset = pReader->lCurrParseOffSet;
        }
    }
    return status;
}

