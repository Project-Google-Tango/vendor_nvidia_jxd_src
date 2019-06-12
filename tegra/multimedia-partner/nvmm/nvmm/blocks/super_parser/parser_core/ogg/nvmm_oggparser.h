/**
 * @file nvmm_aviparser.h
 * @brief <b>NVIDIA Media Parser Package:ogg parser/b>
 *
 * @b Description:   This file outlines the API's provided by ogg parser
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

#ifndef INCLUDED_NVMM_OGGPARSER_H
#define INCLUDED_NVMM_OGGPARSER_H

#include "nvmm_oggparser_defines.h"
#include "nvmm.h"
#include "nvmm_contentpipe.h"

extern NvS32 TotalPcmOutputBytes;

#include "nvmm_metadata.h"

typedef struct OggMetaData
{     
     NvMMMetaDataInfo oArtistMetaData;
     NvMMMetaDataInfo oAlbumMetaData;
     NvMMMetaDataInfo oGenreMetaData;
     NvMMMetaDataInfo oTitleMetaData;
     NvMMMetaDataInfo oYearMetaData;
     NvMMMetaDataInfo oTrackNoMetaData;
     NvMMMetaDataInfo oEncodedMetaData;     
     NvMMMetaDataInfo oCommentMetaData;
     NvMMMetaDataInfo oComposerMetaData;
     NvMMMetaDataInfo oPublisherMetaData;    
     NvMMMetaDataInfo oOrgArtistMetaData;
     NvMMMetaDataInfo oCopyRightMetaData;
     NvMMMetaDataInfo oUrlMetaData;
     NvMMMetaDataInfo oBpmMetaData;
     NvMMMetaDataInfo oAlbumArtistMetaData;     
} NvOggMetaData;

typedef struct OggMetaDataParams
{
   NvS32 pArtistMetaData; 
   NvS32 pAlbumMetaData; 
   NvS32 pGenreMetaData; 
   NvS32 pTitleMetaData;    
   NvS32 pYearMetaData; 
   NvS32 pTrackMetaData; 
   NvS32 pEncodedMetaData; 
   NvS32 pCommentMetaData; 
   NvS32 pComposerMetaData; 
   NvS32 pPublisherMetaData; 
   NvS32 pOrgArtistMetaData; 
   NvS32 pCopyRightMetaData; 
   NvS32 pUrlMetaData; 
   NvS32 pBpmMetaData; 
   NvS32 pAlbumArtistMetaData; 
}NvOggMetaDataParams;

typedef struct
{
     NvS32 numberOfTracks;
     NvOggStreamInfo *pInfo;
     CPhandle cphandle;
     CP_PIPETYPE_EXTENDED *pPipe;
     NvOggParams OggInfo;
     NvBool bOggHeader;
     NvOggMetaData pOggMetaData;
     NvOggMetaDataParams pData;
}NvOggParser;

NvOggParser * NvGetOggReaderHandle(CPhandle hContent, CP_PIPETYPE_EXTENDED *pPipe );
void NvReleaseOggReaderHandle(NvOggParser * pState);
NvError NvParseOggTrackInfo(NvOggParser *pState);
NvError NvOggSeekToTime(NvOggParser *pState, NvS64 mSec);
NvError NvOggGetTotalTime(NvOggParser *pState, NvS32 *TotalTrackTime);
NvError  NvOggSeek(NvOggParser *pState,OggVorbis_File *vf,ogg_int64_t milliseconds);
NvError NvParseOggMetaData(NvOggParser *pState);
NvMMMetaDataTypes NvOggGetFieldName(NvS8 *pData, NvS32 *Len,NvS32 CommentLen);

NvError NvOGGParserInit(NvOggParser *pState, void **OggParams);





#endif  //INCLUDED_NVMM_OGGPARSER_H

























