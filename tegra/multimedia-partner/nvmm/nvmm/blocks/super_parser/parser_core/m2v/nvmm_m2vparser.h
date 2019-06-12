/**
 * @file nvmm_m2vparser.h
 * @brief <b>NVIDIA Media Parser Package:m2v parser/b>
 *
 * @b Description:   This file outlines the API's provided by m2v parser
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
#ifndef INCLUDED_NVMM_M2VPARSER_H
#define INCLUDED_NVMM_M2VPARSER_H


#include "nvmm_m2vparser_defines.h"
#include "nvlocalfilecontentpipe.h"
#include "nvmm.h"

#define MAX_PICTURE_LIST_NUM    3000

#define ENABLE_PRE_PARSING  0
#define ENABLE_PROFILING    0

enum
{
    NV_M2V_MAX_PACKET_SIZE = 2048,
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


typedef struct PicList
{
    NvU32 PictureSize[MAX_PICTURE_LIST_NUM];
    NvU32 PictureNum;
    struct PicList* pNext;
}PictureList;

typedef struct
{
     NvS32 numberOfTracks;
     NvM2vStreamInfo *pInfo;
     CPhandle cphandle;
     CP_PIPETYPE *pPipe;
     NvM2vStream stream;
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
     PictureList* pPicListStart;
     PictureList* pPicListRunning;
     
}NvM2vParser;

NvM2vParser * NvGetM2vReaderHandle(CPhandle hContent, CP_PIPETYPE *pPipe );
void NvReleaseM2vReaderHandle(NvM2vParser * pState);
NvError NvParseM2vTrackInfo(NvM2vParser *pState);
NvError NvM2vInit(NvM2vParser *pState);
NvS64 NvM2vGetDuration(NvM2vParser *pState);
NvError NvM2vSeek(NvM2vParser *pState, NvS64 seekTime);
void NvM2vStreamInit(NvM2vStream *stream);
NvError NvM2vSeekToTime(NvM2vParser *pState, NvS64 mSec);

#endif //INCLUDED_NVMM_M2vPARSER_H
