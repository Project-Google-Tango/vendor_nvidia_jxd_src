/**
 * @file nvmm_mtsparser.h
 * @brief <b>NVIDIA Media Parser Package:mts parser/b>
 *
 * @b Description:   This file outlines the API's provided by mts parser
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

#ifndef INCLUDED_NVMM_MTSPARSER_H
#define INCLUDED_NVMM_MTSPARSER_H

#include "nvmm_mtsparser_defines.h"
#include "nvlocalfilecontentpipe.h"
#include "nvmm.h"

enum
{
    NV_MTS_MAX_PACKET_SIZE = 2048,
    SYNC_ERROR = 1,
};

/**
 * @brief       Bit stream information
 */
typedef struct NvMtsBitStreamRec
{
    NvU8  *Buffer;
    NvS32 BitsLeft;
    NvS32 BitsConsumed;
} NvMtsBitStream;

typedef struct
{
     NvS32 numberOfTracks;
     NvMtsStreamInfo *pInfo;
     CPhandle cphandle;
     CP_PIPETYPE *pPipe;
     NvMtsStream stream;
     NvU64 m_Frame;
     NvS64 m_CurrentTime;
     NvS64 m_PreviousTime;
     NvU64 m_TotalRDB;
     NvS32 m_FirstTime;
     NvU8 *m_pc, *m_pcBuff;
     NvU32 m_SeekSet;
     NvS32 m_GBytesConsumed;
     NvBool InitFlag;
     NvU8* pSrc;  //MPEG2 variables 
     NvU32 FlushBytes;
     NvU32  StartOffset;
     NvBool toggle;     
}NvMtsParser;

NvMtsParser * NvGetMtsReaderHandle(CPhandle hContent, CP_PIPETYPE *pPipe );
void NvReleaseMtsReaderHandle(NvMtsParser * pState);
NvError NvParseMtsTrackInfo(NvMtsParser *pState);
NvError NvMtsInit(NvMtsParser *pState);
NvS64 NvMtsGetDuration(NvMtsParser *pState);
NvError NvMtsSeek(NvMtsParser *pState, NvS64 seekTime);
void NvMtsStreamInit(NvMtsStream *stream);
NvError NvMtsSeekToTime(NvMtsParser *pState, NvS64 mSec);



#endif //INCLUDED_NVMM_MTSPARSER_H






















































