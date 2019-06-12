/*
 * Copyright (c) 2007 - 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NV_MP4PARSER_H
#define INCLUDED_NV_MP4PARSER_H

#include "nvmm.h"
#include "nvmm_parser.h"
#include "nvmm_ulp_util.h"
#include "nv_mp4parser_defines.h"
#include "nvlocalfilecontentpipe.h"
#include "nvdrm.h"
#define MAX_AVCC_COUNT MAX_DECSPECINFO_COUNT
typedef struct NvMP4ParserRec
{
    CPhandle hContentPipe;
    CP_PIPETYPE_EXTENDED *pPipe;
    NvBool IsStreaming;
    NvU64 FileSize;

    NvMp4MovieData MvHdData;
    NvMp4MediaMetadata MediaMetadata;
    NvMp4SynchUnitData VideoSyncData;
    NvAVCConfigData *pAVCConfigData[MAX_AVCC_COUNT];
    NvU32 nAVCConfigCount;
    NvAVCConfigData *pCurAVCConfigData; /*keep track of the current activated avcC*/
    NvMp4TrackInformation TrackInfo[MAX_TRACKS];
    NvMp4FramingInfo FramingInfo[MAX_TRACKS];
    NvMp4AacConfigurationData AACConfigData;

    NvU32 NALSize;
    NvU32 AudioIndex;
    NvU32 VideoIndex;
    NvU64 MDATSize;

    NvString URIList[MAX_REF_TRACKS + 1]; // Reference movie location
    NvU32 URICount;
    NvString EmbeddedURL;
    NvMp4ParserPlayState PlayState;
    NvU32 AudioFPS;
    NvS32 rate;
    NvU32 SeekVideoFrameCounter;
    NvBool IsFirstFrame;
    NvU64 CurrentFrameOffset;
    NvU64 PreviousFrameOffset;
    NvU8 pTempBuffer[ReadBufferSize_256KB];
    NvU32 TempCount;

    /* For SINF atom*/
    NvBool IsSINF;
    NvU32 SINFSize;
    void *pSINF;

    /* DRM handles*/
    NvU32 DrmError;
    NvDrmContextHandle pDrmContext ;
    NvDrmInterface DrmInterface;
} NvMp4Parser;


NvError
NvMp4ParserInit (
    NvMp4Parser *pNvMp4Parser);


void
NvMp4ParserDeInit (
    NvMp4Parser *pNvMp4Parser);


NvError
NvMp4ParserParse (
    NvMp4Parser *pNvMp4Parser);


NvU32
NvMp4ParserGetNumTracks (
    NvMp4Parser *pNvMp4Parser);


NvMp4TrackTypes
NvMp4ParserGetTracksInfo (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex);


NvError
NvMp4ParserSetTrack (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex) ;

void*
NvMp4ParserGetDecConfig (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex,
    NvU32 *pSize);

NvU64
NvMp4ParserGetMediaTimeScale (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex);

NvU64
NvMp4ParserGetMediaDuration (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex);

NvU32
NvMp4ParserGetTotalMediaSamples (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex);

NvError
NvMp4ParserGetBitRate (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex,
    NvU32 *pAvgBitRate,
    NvU32 *pMaxBitRate);

NvError
NvMp4ParserGetNALSize (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex,
    NvU32 *pNALSize);

NvError
NvMp4ParserGetCoverArt (
    NvMp4Parser *pNvMp4Parser,
    void *pBuffer);

NvS32
NvMp4ParserGetNextSyncUnit (
    NvMp4Parser *pNvMp4Parser,
    NvS32 Reference,
    NvBool Direction);


NvError
NvMp4ParserSetMediaDuration (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex,
    NvU64 Duration);


NvError NvMp4ParserGetNextAccessUnit (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex,
    void * pBuffer,
    NvU32 *pFrameSize,
    NvU64 *pDTS,
    NvS32 *pCTS,
    NvU64 *pELST);

NvError NvMp4ParserGetCurrentAccessUnit (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex ,
    void *pBuffer,
    NvU32 *pFrameSize,
    NvU64 *pDTS,
    NvS32 *pCTS,
    NvU64 *pELST);

NvError
NvMp4ParserGetMultipleFrames (
    NvMp4Parser *pNvMp4Parser,
    NvU32 TrackIndex,
    NvMMBuffer *pOffsetListBuffer,
    NvU32 VBase,
    NvU32 PBase);

NvError
NvMp4ParserSetRate (
    NvMp4Parser *pNvMp4Parser);

NvError
NvMp4ParserGetAudioProps (
    NvMp4Parser*pNvMp4Parser,
    NvMMStreamInfo *pCurrentStream,
    NvU32 TrackIndex,
    NvU32* pMaxBitRate);

NvError
NvMp4ParserGetAtom (
    NvMp4Parser * pNvMp4Parser,
    NvU32 AtomType,
    NvU8 *pAtom,
    NvU32 *pAtomSize);

NvError
NvMp4ParserGetAVCConfigData (
    NvMp4Parser *pNvMp4Parser,
    NvU32 StreamIndex,
    NvAVCConfigData **ppAVCConfigData);

NvMp4AvcEntropy
NvMp4ParserParsePPS (
    NvU8 *p,
    NvU32 size);

#endif // INCLUDED_NV_MP4PARSER_H

