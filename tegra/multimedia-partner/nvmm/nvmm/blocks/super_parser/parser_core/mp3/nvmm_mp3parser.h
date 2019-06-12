/*
 * Copyright (c) 2007-2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef INCLUDED_NV_MP3PARSER_H
#define INCLUDED_NV_MP3PARSER_H

#include "nvos.h"
#include "nvmm.h"
#include "nvmm_common.h"
#include "nvmm_parser.h"
#include "nvmm_metadata.h"
#include "nvlocalfilecontentpipe.h"

#define NVLOG_CLIENT NVLOG_MP3_PARSER // Required for logging information

#define MP3_XING_BUFF_SIZE                  160
#define MP3_FHG_BUFF_SIZE                   2048
#define MP3_MAX_SHOUT_CAST_META_SIZE     255*16

/**
 * MP3 Metadata Types
 */
typedef enum
{
    NvMp3MetaDataType_ARTIST,
    NvMp3MetaDataType_ALBUM,
    NvMp3MetaDataType_GENRE,
    NvMp3MetaDataType_TITLE,
    NvMp3MetaDataType_COMMENT,
    NvMp3MetaDataType_TRACKNUM,
    NvMp3MetaDataType_YEAR,
    NvMp3MetaDataType_ENCODED,
    NvMp3MetaDataType_COMPOSER,
    NvMp3MetaDataType_PUBLISHER,
    NvMp3MetaDataType_ORGARTIST,
    NvMp3MetaDataType_COPYRIGHT,
    NvMp3MetaDataType_URL,
    NvMp3MetaDataType_BPM,
    NvMp3MetaDataType_ALBUMARTIST,
    // This is be the last entry representing the max metadata. 
    // Any new metadata type should be added just before this not in between.
    NvMp3MetaDataType_MAXMETADATA, 
    NvMp3MetaDataType_Force32 = 0x7FFFFFFF
} NvMp3MetaDataType;

typedef struct NvMp3MetaDataRec
{
    NvMMMetaDataInfo Id3MetaData[NvMp3MetaDataType_MAXMETADATA];
    // Cover data Related
    NvMMMetaDataInfo Mp3CoverArt;
    NvMMMetaDataInfo ShoutCast;
} NvMp3MetaData;

typedef enum
{
    NvMp3VbrType_NONE,
    NvMp3VbrType_XING,
    NvMp3VbrType_FHG,
    NvMp3VbrType_OTHER,
    NvMp3VbrType_Force32 = 0x7FFFFFFF      
} NvMp3VbrType;

typedef enum
{
    NvMp3Version_MPEG1,
    NvMp3Version_MPEG2,
    NvMp3Version_MPEG25,  // Mpeg 2.5 extension for lower bitrates. Since the compiler won't take a ".", hence this name
    NvMp3Version_Force32 = 0x7FFFFFFF    
} NvMp3Version;

typedef enum
{
    NvMp3Layer_LAYER1,
    NvMp3Layer_LAYER2,
    NvMp3Layer_LAYER3,
    NvMp3Layer_NONE,
    NvMp3Layer_Force32 = 0x7FFFFFFF
} NvMp3Layer;

typedef struct
{
    NvU32 BitRate;
    NvU32 SamplingFrequency;
    NvU64 TotalTime;
    NvU32 NumChannels;
    NvU32 SampleSize;
    NvU32 DataOffset;
    NvMp3Layer Layer;
    NvMp3Version Version;
    NvMp3VbrType VbrType;
    NvU32 UnPaddedFrames;
    NvU32 PaddedFrames;
    NvU32 FrameStart;
    NvU32 FrameSize;
} NvMp3TrackInfo;


typedef struct
{
    NvS32 ID;                     // from MPEG header, 0=MPEG2, 1=MPEG1
    NvS32 SampleRate;            // determined from MPEG header
    NvS32 Flags;                   // from Xing header data
    NvS32 Frames;               // total bit stream frames from Xing header data
    NvS32 Bytes;                 // total bit stream bytes from Xing header data
    NvS32 VbrScale;          // encoded vbr scale from Xing header data
    NvU8 *pTOCBuf;                     // pointer to unsigned char toc_buffer[100],may be NULL if toc not desired
    float FrameDuration;   // Added By mks for frame duration in ms
} NvMp3XingHeader;

typedef struct
{
    NvS32 SampleRate;
    NvS8   VbriSignature[5];
    NvU32 VbriVersion;
    float    VbriDelay;
    NvU32 VbriQuality;
    NvU32 VbriStreamBytes;
    NvU32 VbriStreamFrames;
    NvU32 VbriTableSize;
    NvU32 VbriTableScale;
    NvU32 VbriEntryBytes;
    NvU32 VbriEntryFrames;
    NvS32 VbriTable[512];
} NvMp3VbriHeaderTable;

typedef struct NvMp3XingRec
{
    NvMp3XingHeader XingHeader;
    NvU8 XHDToc[100];
    float FrameDuration;
} NvMp3Xing;

typedef struct NvMp3FhgHeaderRec
{
    NvMp3VbriHeaderTable FhgVbr;
} NvMp3FhgHeader;

typedef struct NvMp3VbriHeaderRec
{
    NvU32 Position;
    NvMp3VbriHeaderTable* pVBHeader;
} NvMp3VbriHeader;

typedef struct NvVbrHeaderGroupRec
{
    NvError XingResult;
    NvError FhgResult;
    NvBool VbrFirstTocError;
    NvBool VbrTocError;
    NvMp3Xing Mp3InfoXing;
    NvMp3FhgHeader  Mp3InfoFhg;
    NvMp3VbriHeader FhgVbr;
    NvU8 XingBuff[MP3_XING_BUFF_SIZE+MP3_FHG_BUFF_SIZE];
} NvMp3VbrHeaderGroup;

typedef struct
{
    NvMp3TrackInfo TrackInfo;
    NvMp3VbrHeaderGroup VBRHeader;
} NvMp3StreamInfo;

typedef struct NvMp3ParserRec
{
    NvU32 NumberOfTracks;
    NvMp3StreamInfo StreamInfo;
    CPhandle hContentPipe;
    CP_PIPETYPE_EXTENDED *pPipe;
    NvMp3MetaData Id3TagData;
    NvU64 LastFrameBoundary;
    NvU64 FileSizeInBytes;
    NvU64 CurrFileOffset;
    NvBool IsXingHeaderCorrupted;
    NvBool IsStreaming;
    NvU32 SCMetaDataInterval;
    NvU32 SCOffset;
    NvBool IsShoutCastStreaming;
    NvU32 SCRemainder;
    NvU32 SCDivData;
    NvBool IsTAGDetected;
} NvMp3Parser;

NvError
NvMp3ParserInit (
    NvMp3Parser *pNvMp3Parser,
    NvString pFilePath);

void
NvMp3ParserDeInit (
    NvMp3Parser *pNvMp3Parser);

NvError
NvMp3ParserParse (
    NvMp3Parser *pNvMp3Parser);

NvError
NvMp3ParserSeekToTime (
    NvMp3Parser *pNvMp3Parser,
    NvU32 *pMillisec);

#endif // INCLUDED_NV_MP3PARSER_H

