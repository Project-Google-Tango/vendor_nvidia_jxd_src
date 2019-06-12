
/**
 * @file nvmm_amrparser.h
 * @brief <b>NVIDIA Media Parser Package:amr parser/b>
 *
 * @b Description:   This file outlines the API's provided by amr parser
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
#ifndef INCLUDED_NVMM_AMRPARSER_H
#define INCLUDED_NVMM_AMRPARSER_H


#include "nvmm_amrparser_defines.h"
#include "nvlocalfilecontentpipe.h"
#include "nvmm.h"
#include "nvmm_logger.h"
#include "nvmm_common.h"

#define NVLOG_CLIENT NVLOG_AMR_PARSER


enum
{
    /* max. num. of serial bits */
    MAX_SERIAL_SIZE = 244,
};

/**
 * @brief       The frame type of the codec
 */
typedef enum
{
    NvAmrFrameType_SpeechGood = 0,
    NvAmrFrameType_SpeechDegraded,
    NvAmrFrameType_Onset,
    NvAmrFrameType_SpeechBad,
    NvAmrFrameType_SidFirst,
    NvAmrFrameType_SidUpdate,
    NvAmrFrameType_SidBad,
    NvAmrFrameType_NoData,
    NvAmrFrameType_N_Frametypes,
    NvAmrFrameType_Force32 = 0x7FFFFFFF
} NvAmrFrameType;

/**
 * @brief       Mode of the codec
 */
typedef enum
{
    NvAmrMode_Mr475 = 0,
    NvAmrMode_Mr515,
    NvAmrMode_Mr59,
    NvAmrMode_Mr67,
    NvAmrMode_Mr74,
    NvAmrMode_Mr795,
    NvAmrMode_Mr102,
    NvAmrMode_Mr122,
    NvAmrMode_Mrdtx,
    NvAmrMode_N_Modes,
    NvAmrMode_Force32 = 0x7FFFFFFF
} NvAmrMode;


typedef struct
{
     NvS32 numberOfTracks;
     NvAmrStreamInfo *pInfo;
     CPhandle cphandle;
     CP_PIPETYPE_EXTENDED *pPipe;
     NvU64 m_Frame;
     NvU64 FileSize;
     NvU64 TotalReadBytes;
     NvBool m_EndOfFile;
     NvBool m_MgkFlag;
     NvBool m_SeekFlag;
     NvS64 m_CurrentTime;
     NvS64 m_PreviousTime;
     NvS16 m_Mode;
     NvBool bAmrAwbHeader;
}NvAmrParser;

NvAmrParser * NvGetAmrReaderHandle(CPhandle hContent, CP_PIPETYPE_EXTENDED *pPipe );
void NvReleaseAmrReaderHandle(NvAmrParser * pState);
NvError NvParseAmrAwbTrackInfo(NvAmrParser *pState);
NvU64 NvAmrGetTotalFrames(NvAmrParser *pState);
NvU64 NvAmrGetDuration(NvAmrParser *pState);
NvU64 NvAwbGetDuration(NvAmrParser *pState);
NvU64 NvAwbGetTotalFrames(NvAmrParser *pState);
NvS64 NvAmrGetCurrentPosition(NvAmrParser *pState);
NvS64 NvAwbGetCurrentPosition(NvAmrParser *pState);
NvError NvAmrInit(NvAmrParser *pState);
NvError NvAwbInit(NvAmrParser *pState);
NvError NvAmrSeek(NvAmrParser *pState, NvS64 seekTime);
NvError NvAwbSeek(NvAmrParser *pState, NvS64 seekTime);
NvAmrFrameType NvAmrUnpackBits (NvU8 q, NvU16 ft, NvU8 PackedBits[], NvAmrMode *pMode, NvU8 bits[]);
NvError NvAmrGetNextFrame(NvAmrParser *pState, NvU8 *pData, NvU32 askBytes, NvU32 *bytesRead);
NvError NvAwbGetNextFrame(NvAmrParser *pState, NvU8 *pData, NvU32 askBytes, NvU32 *bytesRead);



#endif //INCLUDED_NVMM_AMRPARSER_H



























































