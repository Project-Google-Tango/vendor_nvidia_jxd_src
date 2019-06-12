/*
* Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

#ifndef INCLUDED_NVMM_METADATA_H_
#define INCLUDED_NVMM_METADATA_H_

#include "nvmm.h"

#define    PORT_VIDEO  0
#define    PORT_AUDIO  1

/**
* Types of metadata available 
*/
typedef enum {
    NvMMMetaDataInfo_Artist = 1,   
    NvMMMetaDataInfo_Album,
    NvMMMetaDataInfo_Genre,
    NvMMMetaDataInfo_Title,
    NvMMMetaDataInfo_Year,
    NvMMMetaDataInfo_TrackNumber,
    NvMMMetaDataInfo_Encoded,
    NvMMMetaDataInfo_Comment,
    NvMMMetaDataInfo_Composer,
    NvMMMetaDataInfo_Publisher,
    NvMMMetaDataInfo_OrgArtist,
    NvMMMetaDataInfo_Copyright,
    NvMMMetaDataInfo_Url,
    NvMMMetaDataInfo_Bpm,
    NvMMMetaDataInfo_AlbumArtist,
    NvMMMetaDataInfo_CoverArt,
    NvMMMetaDataInfo_CoverArtURL,
    NvMMMetaDataInfo_ThumbnailSeekTime,
    NvMMMetaDataInfo_RtcpAppData,
    NvMMMetaDataInfo_RTCPSdesCname,
    NvMMMetaDataInfo_RTCPSdesName,
    NvMMMetaDataInfo_RTCPSdesEmail,
    NvMMMetaDataInfo_RTCPSdesPhone,
    NvMMMetaDataInfo_RTCPSdesLoc,
    NvMMMetaDataInfo_RTCPSdesTool,
    NvMMMetaDataInfo_RTCPSdesNote,
    NvMMMetaDataInfo_RTCPSdesPriv,
    NvMMMetaDataInfo_SeekNotPossible,
    NvMMMetaDataInfo_Force32 = 0x7FFFFFFF    
} NvMMMetaDataTypes;

/**
* MetaData character sets
*/
typedef enum {
    NvMMMetaDataEncode_Utf8 = 1,
    NvMMMetaDataEncode_Utf16,
    NvMMMetaDataEncode_Ascii,
    NvMMMetaDataEncode_NvU32, 
    NvMMMetaDataEncode_Binary, 
    NvMMMetaDataEncode_NvU64,
    NvMMMetaDataEncode_JPEG = 0x100,
    NvMMMetaDataEncode_PNG,
    NvMMMetaDataEncode_BMP,
    NvMMMetaDataEncode_GIF,
    NvMMMetaDataEncode_Other,
    NvMMMetaDataEncode_Force32 = 0x7FFFFFFF
} NvMMMetaDataCharSet;

/**
* @brief Defines of MetaData.
*/
typedef struct 
{
    NvMMMetaDataCharSet eEncodeType;
    NvMMMetaDataTypes eMetadataType;
    NvU32 nBufferSize;
    NvS8 *pClientBuffer; 
} NvMMMetaDataInfo;


/** Maximum size of  marker name */
#define NVMM_MAX_MARKERNAME_SIZE 128

/**
* @brief Defines of MarkerData
*/

typedef struct 
{
    NvU64    MarkerPresentationTime;
    NvU16    MarkerNameLength;
    NvS8     MarkerName[NVMM_MAX_MARKERNAME_SIZE];     
} NvMMMarkerData;

typedef struct
{
    NvU32 MarkerCount;
    NvMMMarkerData *pData;
}NvMMMarkerDataInfo;



typedef struct
{
    NvU32 BufferLen;
    NvU8 *pBuffer;
    NvU32 nPortIndex; // to determine codecinfo is for audio or video
}NvMMCodecInfo;

#endif

