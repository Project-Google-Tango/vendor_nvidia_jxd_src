/**
 * @file nvmm_mps_parser.h
 * @brief <b>NVIDIA Media Parser Package:mps parser/b>
 *
 * @b Description:   This file outlines the API's provided by mps parser
 */

/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all NvS32ellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef INCLUDED_NVMM_MPS_PARSER_H
#define INCLUDED_NVMM_MPS_PARSER_H

#include "nvmm.h"
#include "nvlocalfilecontentpipe.h"
#include "nvmm_mps_defines.h"
#include "nvmm_mps_reader.h"


/* All Audio Video related informations are stored in this structure */
typedef struct NvMpsParserAvInfoRec
{
    /* Audio related information */
    NvBool bHasAudio;
    NvU32 uAudioStreamID;
    NvU64 lFirstAudioPTS;
    NvBool bFirstAudioPTSAccurate;
    NvBool bAudioPropertiesParsed;

    eNvMpsAudioType eAudioType;
    NvU32 uBitsPerSample;
    NvU32 uSamplingFreq;
    NvU32 uNumChannels;
    NvU32 uAudioBitRate;

    /* Video related information */
    NvBool bHasVideo;
    NvU32 uVideoStreamID;
    NvU64 lFirstVideoPTS;
    NvBool bFirstVideoPTSAccurate;
    eNvMpsVideoType eVideoType;

    NvBool bVideoPropertiesParsed;
    NvU32 uWidth;
    NvU32 uHeight;
    NvU32 uFps;
    NvU32 uAsrWidth;
    NvU32 uAsrHeight;
    NvU64 lVideoBitRate;

    /* Duration related information */
    NvU64 lFirstPTS;
    NvBool bFirstPTSAccurate;

    NvBool bLastPTSFound;
    NvU64  lLastPTS;
    NvBool bLastPTSAccurate;
}NvMpsParserAvInfo;


/* NvMpsParser - This is the main parser structure used by nvmm core parser */
typedef struct NvMpsParserRec
{
    NvMpsPackHead *pFirstPackHead;
    NvMpsReader *pReader;
    NvMpsParserAvInfo *pAvInfo;

    NvU32 uNumStreams;


    NvU64   lDuration;
    NvU64   lAvgBitRate;

    /* File offset variables */
    NvU64 lFirstPESFileOffset;
    NvU64 lLastPESFileOffset;
    NvU64 lFirstVideoPESFileOffset;
    NvU64 lLastVideoPESFileOffset;
    NvU64 lFirstAudioPESFileOffset;
    NvU64 lLastAudioPESFileOffset;

    /* EOS related */
    NvBool bAnyEOS;
    NvBool bVideoEOS;
    NvBool bAudioEOS;

    NvError (*ParseAudioProperties)(struct NvMpsParserRec *pMpsParser, NvMpsParserAvInfo *pAvInfo);
    NvError (*ParseVideoProperties)(struct NvMpsParserRec *pMpsParser, NvMpsParserAvInfo *pAvInfo);

    NvError (*FindAvInfo)(struct NvMpsParserRec *pMpsParser, NvMpsParserAvInfo *pAvInfo, NvU32 uLimit);
    NvError (*FindDuration)(struct NvMpsParserRec *pMpsParser, NvMpsParserAvInfo *pAvInfo);

}NvMpsParser;

/* Global NvMpsParser related functions that are used by nvmm parser core */
NvMpsParser *NvMpsParserCreate(NvU32 uScratchBufMaxSize);
void NvMpsParserDestroy(NvMpsParser *pMpsParser);
NvError NvMpsParserInit(NvMpsParser *pMpsParser, CP_PIPETYPE *pPipe, CPhandle cphandle);
NvError NvMpsParserGetNextPESInfo(NvMpsParser *pMpsParser, eNvMpsMediaType *peMediaType,
                                  NvU32 *puDataSize);
NvError NvMpsParserGetNextPESData(NvMpsParser *pMpsParser, eNvMpsMediaType *peMediaType,
                       NvU8 *pBuf, NvU32 *puBufSize);

#endif //INCLUDED_NVMM_MPS_PARSER_H
