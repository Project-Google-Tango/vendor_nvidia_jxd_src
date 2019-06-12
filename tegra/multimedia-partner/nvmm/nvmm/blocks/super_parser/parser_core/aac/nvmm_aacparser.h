/**
 * @file nvmm_aacparser.h
 * @brief <b>NVIDIA Media Parser Package:aac parser/b>
 *
 * @b Description:   This file outlines the API's provided by aac parser
 */

/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all NvS32ellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef INCLUDED_NVMM_AACPARSER_H
#define INCLUDED_NVMM_AACPARSER_H


#include "nvmm_aacparser_defines.h"
#include "nvlocalfilecontentpipe.h"
#include "nvmm.h"
#include "nvmm_adtsheader.h"
#include "nvmm_logger.h"
#include "nvmm_common.h"

#define NVLOG_CLIENT NVLOG_AAC_PARSER

enum
{
    NV_AAC_MAX_PACKET_SIZE = 2048,
    SYNC_ERROR = 1,
};

/**
 * @brief       Bit stream information
 */
typedef struct NvBitStreamRec
{
    NvU8  *Buffer;
    NvS32 BitsLeft;
    NvS32 BitsConsumed;
} NvBitStream;


typedef struct
{
     NvS32 numberOfTracks;
     NvAacStreamInfo *pInfo;
     CPhandle cphandle;
     CP_PIPETYPE_EXTENDED *pPipe;
     NvAacStream stream;
     NvU64 m_Frame;
     NvS64 m_CurrentTime;
     NvS64 m_PreviousTime;
     NvU64 m_TotalRDB;
     NvS32 m_FirstTime;
     NvU8 m_cBuffer[MAX_ADTSAAC_BUFFER_SIZE];
     NvU8 *m_pc, *m_pcBuff;
     NvU32 m_SeekSet;
     NvS32 m_GBytesConsumed;
     NvBool InitFlag;
     NvBool SeekInitFlag;
     NvBool bAacHeader;
     
}NvAacParser;

typedef enum
{
    TIMESTAMP_SR_48000 = 426666, 
    TIMESTAMP_SR_44100 = 464399,   
    TIMESTAMP_SR_32000 = 640000,
    TIMESTAMP_SR_24000 = 853333,
    TIMESTAMP_SR_22050 = 928798,
    TIMESTAMP_SR_16000 = 1280000,
    TIMESTAMP_SR_12000 = 1706666,
    TIMESTAMP_SR_11025 = 1857596,
    TIMESTAMP_SR_8000  = 2560000
}NvAacTimeStamps;

NvAacParser * NvGetAacReaderHandle(CPhandle hContent, CP_PIPETYPE_EXTENDED *pPipe );
void NvReleaseAacReaderHandle(NvAacParser * pState);
NvError NvParseAacTrackInfo(NvAacParser *pState);
NvError NvAacInit(NvAacParser *pState);
NvS64 NvAacGetDuration(NvAacParser *pState);
NvS64 NvAacGetCurrentPosition(NvAacParser *pState);
NvError NvAacSeek(NvAacParser *pState, NvS64 seekTime);
NvError NvGetAdtsHeader(NvAacParser *pState,NvADTSHeader *pH);
void NvAacStreamInit(NvAacStream *stream);
NvError NvAdts_AacGetNextFrame(NvAacParser *pState, NvU8 *pData, NvU32 askBytes, NvU32 *ulBytesRead);
NvError NvAacSeekToTime(NvAacParser *pState, NvS64 mSec);
NvU64 GetAacTimeStamp(NvS32 sampleRate);


#endif //INCLUDED_NVMM_AACPARSER_H




























































