/*
 * Copyright (c) 2007-2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvmm_mp3parser.h"
#include "nvmm_contentpipe.h"
#include "nvmm_logger.h"

/* MACROS for extracting Frame Header bit fields */
#define GET_FRAME_SYNC(FrameHdr)                       (NvU32)(((FrameHdr>>21)&0x7ff))
#define GET_FRAME_VERSION(FrameHdr)                 (NvU32)(((FrameHdr>>19)&0x3))
#define GET_FRAME_LAYER(FrameHdr)                     (NvU32)(((FrameHdr>>17)&0x3))
#define GET_FRAME_PROTECTION(FrameHdr)           (NvU32)(((FrameHdr>>16)&0x1))
#define GET_FRAME_BITRATEINDEX(FrameHdr)        (NvU32)(((FrameHdr>>12)&0xf))
#define GET_FRAME_SAMPLINGRATE(FrameHdr)       (NvU32)(((FrameHdr>>10)&0x3))
#define GET_FRAME_PADDING(FrameHdr)                 (NvU32)(((FrameHdr>>9)&0x1))
#define GET_FRAME_PRIVATE(FrameHdr)                  (NvU32)(((FrameHdr>>8)&0x1))
#define GET_FRAME_CHANNELMODE(FrameHdr)        (NvU32)(((FrameHdr>>6)&0x3))
#define GET_FRAME_CHANNELMODEEXT(FrameHdr)  (NvU32)(((FrameHdr>>4)&0x3))
#define GET_FRAME_COPYRIGHT(FrameHdr)              (NvU32)(((FrameHdr>>3)&0x1))
#define GET_FRAME_ORIGINAL(FrameHdr)                (NvU32)(((FrameHdr>>2)&0x1))
#define GET_FRAME_EMPHASIS(FrameHdr)                (NvU32)(((FrameHdr)&0x3))

/* Frame Comparision */
#define MAX_LAYER_NO_CHANGE_COUNT 64
#define MAX_SAMPLERATE_NO_CHANGE_COUNT 16
#define MAX_VERSION_NO_CHANGE_COUNT 16
#define MAX_BITRATE_NO_CHANGE_COUNT 16

#define MAX_BITRATE_CHANGE_COUNT 16
#define MAX_LAYER_CHANGE_COUNT 512
#define MAX_VERSION_CHANGE_COUNT 512
#define MAX_SAMPLERATE_CHANGE_COUNT 512

/* Frame Identification */
#define MAX_SYNC_NOT_FOUND_COUNT 640*1024
#define MAX_HEADER_PARSING_SIZE 64
#define MAXIMUM_VALID_FRAME_COUNT 16
#define MAX_ID3_TAG_COUNT   5

/* GetLastFrame Identification */
#define BUFFER_CHUNK_SIZE 2048
#define BUFFER_MIN_CHUNK_SIZE 32
#define BUFFER_MAX_CHUNK_SIZE 4*1024*1024 // 4MB

/* XING Header */
#define XING_HEADER_TOC_TABLE_SIZE  100
#define XING_TOC_TABLE_ZEROS_MAX_SIZE  1

typedef enum
{
    NvMp3VbrFlag_FRAMES     =   0x0001,
    NvMp3VbrFlag_BYTES        =   0x0002,
    NvMp3VbrFlag_TOC            =   0x0004,
    NvMp3VbrFlag_SCALE        =   0x0008,
    NvMp3VbrFlag_Force32     =   0x7FFFFFFF
}NvMP3VbrFlag;

typedef enum
{
    /// Specifies MP3 version 1.0 .
    MP3_Ver1_0 = 0,
    /// Specifies MP3 version 1.1.
    MP3_Ver1_1,
    /// Specifies MP3 version 2.2.
    MP3_Ver2_2,
    /// Specifies MP3 version 2.3.
    MP3_Ver2_3,
    /// Specifies MP3 version 2.4.
    MP3_Ver2_4,
}Mp3Version;

static const NvS32 s_MygaSampleRate[3][4] =
{
    {44100, 48000, 32000, 0}, // MPEG-1
    {22050, 24000, 16000, 0}, // MPEG-2
    {11025, 12000,  8000, 0}, // MPEG-2.5
};

static const NvS32 s_MygaBitrate[2][3][15] =
{
    {
        // MPEG-1
        {  0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448}, // Layer 1
        {  0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384}, // Layer 2
        {  0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320}, // Layer 3
    },

    {
        // MPEG-2, MPEG-2.5
        {  0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256}, // Layer 1
        {  0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}, // Layer 2
        {  0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}, // Layer 3
    },
};

static const NvU32 s_AllowedModes[15][2] =
{
    // {stereo, intensity stereo, dual channel allowed,single channel allowed}
    {1, 1},     // free mode
    {0, 1},     // 32
    {0, 1},     // 48
    {0, 1},     // 56
    {1, 1},     // 64
    {0, 1},     // 80
    {1, 1},     // 96
    {1, 1},     // 112
    {1, 1},     // 128
    {1, 1},     // 160
    {1, 1},     // 192
    {1, 0},     // 224
    {1, 0},     // 256
    {1, 0},     // 320
    {1, 0}      // 384
};

static const NvU32 s_SamplingFreqTable[9] =
{
    48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000
};

static const NvU64 s_FrameDuration[3][9] =
{
        {8000, 8707, 12000, 16000, 17414, 24000, 32000, 34829, 48000},
        {24000, 26120, 36000, 48000, 52244, 72000, 96000, 104489, 144000},
        {24000, 26120, 36000, 24000, 26120, 36000, 48000, 52240, 72000}
};

static NvS32 s_XingSampleRateTable[4] = {44100, 48000, 32000, 99999 };
    
static const char *s_GenreTable[] =
{
    "Blues",
    "Classic Rock",
    "Country",
    "Dance",
    "Disco",
    "Funk",
    "Grunge",
    "Hip-Hop",
    "Jazz",
    "Metal",
    "New Age",
    "Oldies",
    "Other",
    "Pop",
    "R&B",
    "Rap",
    "Reggae",
    "Rock",
    "Techno",
    "Industrial",
    "Alternative",
    "Ska",
    "Death Metal",
    "Pranks",
    "Soundtrack",
    "Euro-Techno",
    "Ambient",
    "Trip-Hop",
    "Vocal",
    "Jazz+Funk",
    "Fusion",
    "Trance",
    "Classical",
    "Instrumental",
    "Acid",
    "House",
    "Game",
    "Sound Clip",
    "Gospel",
    "Noise",
    "Alternative Rock",
    "Bass",
    "Soul",
    "Punk",
    "Space",
    "Meditative",
    "Instrumental Pop",
    "Instrumental Rock",
    "Ethnic",
    "Gothic",
    "Darkwave",
    "Techno-Industrial",
    "Electronic",
    "Pop-Folk",
    "Eurodance",
    "Dream",
    "Southern Rock",
    "Comedy",
    "Cult",
    "Gangsta",
    "Top 40",
    "Christian Rap",
    "Pop/Funk",
    "Jungle",
    "Native American",
    "Cabaret",
    "New Wave",
    "Psychadelic",
    "Rave",
    "Showtunes",
    "Trailer",
    "Lo-Fi",
    "Tribal",
    "Acid Punk",
    "Acid Jazz",
    "Polka",
    "Retro",
    "Musical",
    "Rock & Roll",
    "Hard Rock",
    "Folk",
    "Folk-Rock",
    "National Folk",
    "Swing",
    "Fast Fusion",
    "Bebob",
    "Latin",
    "Revival",
    "Celtic",
    "Bluegrass",
    "Avantgarde",
    "Gothic Rock",
    "Progressive Rock",
    "Psychedelic Rock",
    "Symphonic Rock",
    "Slow Rock",
    "Big Band",
    "Chorus",
    "Easy Listening",
    "Acoustic",
    "Humour",
    "Speech",
    "Chanson",
    "Opera",
    "Chamber Music",
    "Sonata",
    "Symphony",
    "Booty Bass",
    "Primus",
    "Porn Groove",
    "Satire",
    "Slow Jam",
    "Club",
    "Tango",
    "Samba",
    "Folklore",
    "Ballad",
    "Power Ballad",
    "Rhythmic Soul",
    "Freestyle",
    "Duet",
    "Punk Rock",
    "Drum Solo",
    "Acapella",
    "Euro-House",
    "Dance Hall",
    "Goa",
    "Drum & Bass",
    "Club-House",
    "Hardcore",
    "Terror",
    "Indie",
    "BritPop",
    "Negerpunk",
    "Polsk Punk",
    "Beat",
    "Christian Gangsta Rap",
    "Heavy Metal",
    "Black Metal",
    "Crossover",
    "Contemporary Christian",
    "Christian Rock",
    "Merengue",
    "Salsa",
    "Thrash Metal",
    "Anime",
    "Jpop",
    "Synthpop"
};

//Structure Definitions for ID3V1
typedef struct Id3v1_0
{
    NvS8 Id[3];
    NvS8 Title[30];
    NvS8 Artist[30];
    NvS8 Album[30];
    NvU8 Year[4];
    NvS8 Comment[30];
    NvU8 Genre;
} NvMp3Id3v1_0;

typedef struct Id3v1_1
{
    NvS8 Id[3];
    NvS8 Title[30];
    NvS8 Artist[30];
    NvS8 Album[30];
    NvU8 Year[4];
    NvS8 Comment[28];
    NvS8 zero;
    NvU8 track;
    NvU8 Genre;
} NvMp3Id3v1_1;

typedef struct Id3v1
{
    union
    {
        NvMp3Id3v1_0 v1_0;
        NvMp3Id3v1_1 v1_1;
    } id3;
} NvMp3Id3v1;

typedef struct
{
    NvS8 TagID[3];
    NvS8 Version[2];
    NvS8 Flags;
    NvS8 Size[4];
} NvMp3Id3Header;

//Structure Definitions for ID3V2
typedef struct NvMp3Id3V2HeaderRec
{
    NvS8 Tag[3];
    NvU8 VersionMajor;
    NvU8 VersionRevision;
    NvU8 Flags;
    NvU8 Size[4];
} NvMp3Id3V2Header;

typedef struct NvMp3FrameHeaderV234Rec
{
    NvS8  Tag[4];
    NvU8  Size[4];
    NvU8  Flags[2];
} NvMp3FrameHeaderV234;

typedef struct NvMp3FrameHeaderV22Rec
{
    NvS8  Tag[3];
    NvU8  Size[3];
} NvMp3FrameHeaderV22;

typedef struct NvMp3FrameCompareDataRec
{
    NvU32 LayerChangeCount;
    NvU32 LayerUnChangeCount;
    NvMp3Layer LayerFinal;
    NvU32 SampleRateChangeCount;
    NvU32 SampleRateUnChangeCount;
    NvU32 SampleRateFinal;
    NvU32 VersionChangeCount;
    NvU32 VersionUnChangeCount;
    NvMp3Version VersionFinal;
    NvU32 BitRateChangeCount;
    NvU32 BitRateUnChangeCount;
    NvMp3Version BitRateFinal;
    NvU32 FrameMatchCount;
    NvU32 DataOffsetFinal;
}NvMp3FrameCompareData;


static void
Mp3ParserGetMetaData (
    NvMMMetaDataInfo *pMData,
    NvU8 *pInBuffer,
    NvU32 MetaSize,
    NvMMMetaDataCharSet EncType)
{
    if (MetaSize > 0)
    {
        pMData->pClientBuffer = NvOsAlloc (MetaSize);
        NvOsMemcpy (pMData->pClientBuffer, pInBuffer, MetaSize);
        pMData->nBufferSize = MetaSize;
        pMData->eEncodeType = EncType;
    }
}

static NvU32
Mp3ParserGetMetaStrLen (
    const NvS8* pMetaStr)
{
    NvU32 Len = 0;

    while (pMetaStr[Len] && Len < 30)
        Len++;

    return Len;
}

static NvError
Mp3ParserGetId3V1 (
    NvMp3Parser *pNvMp3Parser)
{
    NvError Status = NvSuccess;
    NvMp3Id3v1 Id3v1Data;
    NvU32 MetaSize;
    NvMp3MetaData *pId3TagInfo = &pNvMp3Parser->Id3TagData;

    NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.SetPosition (pNvMp3Parser->hContentPipe, -128, CP_OriginEnd));
    NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.Read (pNvMp3Parser->hContentPipe, (CPbyte *) &Id3v1Data, 128));

    if (!NvOsStrncmp ( (char*) Id3v1Data.id3.v1_0.Id, "TAG", 3))
    {
        pNvMp3Parser->IsTAGDetected = NV_TRUE;

        //ARTIST
        if (pId3TagInfo->Id3MetaData[NvMp3MetaDataType_ARTIST].nBufferSize == 0)
        {
            MetaSize = Mp3ParserGetMetaStrLen (Id3v1Data.id3.v1_0.Artist);
            Mp3ParserGetMetaData (&pId3TagInfo->Id3MetaData[NvMp3MetaDataType_ARTIST],
                                  (NvU8*) Id3v1Data.id3.v1_0.Artist, MetaSize, NvMMMetaDataEncode_Ascii);
        }

        //ALBUM
        if (pId3TagInfo->Id3MetaData[NvMp3MetaDataType_ALBUM].nBufferSize == 0)
        {
            MetaSize = Mp3ParserGetMetaStrLen ( (const NvS8 *) Id3v1Data.id3.v1_0.Album);
            Mp3ParserGetMetaData (&pId3TagInfo->Id3MetaData[NvMp3MetaDataType_ALBUM],
                                  (NvU8*) Id3v1Data.id3.v1_0.Album, MetaSize, NvMMMetaDataEncode_Ascii);
        }

        //TITLE
        if (pId3TagInfo->Id3MetaData[NvMp3MetaDataType_TITLE].nBufferSize == 0)
        {
            MetaSize = Mp3ParserGetMetaStrLen ( (const NvS8 *) Id3v1Data.id3.v1_0.Title);
            Mp3ParserGetMetaData (&pId3TagInfo->Id3MetaData[NvMp3MetaDataType_TITLE],
                                  (NvU8*) Id3v1Data.id3.v1_0.Title, MetaSize, NvMMMetaDataEncode_Ascii);
        }

        //YEAR
        if (pId3TagInfo->Id3MetaData[NvMp3MetaDataType_YEAR].nBufferSize == 0)
        {
            MetaSize = 4;
            Mp3ParserGetMetaData (&pId3TagInfo->Id3MetaData[NvMp3MetaDataType_YEAR],
                                  (NvU8*) Id3v1Data.id3.v1_0.Year, MetaSize, NvMMMetaDataEncode_Ascii);
        }

        //COMMENT
        if (pId3TagInfo->Id3MetaData[NvMp3MetaDataType_COMMENT].nBufferSize == 0)
        {
            MetaSize = ( (Id3v1Data.id3.v1_1.zero == 0) ? 30 : 28);
            Mp3ParserGetMetaData (&pId3TagInfo->Id3MetaData[NvMp3MetaDataType_COMMENT],
                                  (NvU8*) Id3v1Data.id3.v1_0.Comment, MetaSize, NvMMMetaDataEncode_Ascii);
        }

        //TRACK
        if (pId3TagInfo->Id3MetaData[NvMp3MetaDataType_TRACKNUM].nBufferSize == 0)
        {
            if ( (Id3v1Data.id3.v1_1.zero == 0x00) && (Id3v1Data.id3.v1_1.track != 0x00))
            {
                MetaSize = 1;
                Mp3ParserGetMetaData (&pId3TagInfo->Id3MetaData[NvMp3MetaDataType_TRACKNUM],
                                      & (Id3v1Data.id3.v1_1.track), MetaSize, NvMMMetaDataEncode_Ascii);
            }
        }

        // GENRE
        if (pId3TagInfo->Id3MetaData[NvMp3MetaDataType_GENRE].nBufferSize == 0)
        {
            if (Id3v1Data.id3.v1_0.Genre < sizeof (s_GenreTable) / sizeof (NvS8*))
            {
                MetaSize = NvOsStrlen (s_GenreTable[Id3v1Data.id3.v1_0.Genre]);
                Mp3ParserGetMetaData (&pId3TagInfo->Id3MetaData[NvMp3MetaDataType_GENRE],
                                      (NvU8 *) s_GenreTable[Id3v1Data.id3.v1_0.Genre], MetaSize, NvMMMetaDataEncode_Ascii);
            }
        }

    }
    else
    {
        pNvMp3Parser->IsTAGDetected = NV_FALSE;
        Status = NvError_ParserFailedToGetData;
    }

cleanup:
    return Status;
}

static NvS8*
Mp3GetMetaDataTagName (
    NvMp3MetaDataType MetaDataType,
    NvBool IsV2)
{
    switch (MetaDataType)
    {
        case NvMp3MetaDataType_TITLE:
            return (NvS8*)((IsV2) ? "TT2" : "TIT2");
        case NvMp3MetaDataType_ALBUM:
            return (NvS8*)((IsV2) ? "TAL" : "TALB");
        case NvMp3MetaDataType_ARTIST:
            return (NvS8*)((IsV2) ? "TP1" : "TPE1");
        case NvMp3MetaDataType_TRACKNUM:
            return (NvS8*)((IsV2) ? "TRK" : "TRCK");
        case NvMp3MetaDataType_YEAR:
            return (NvS8*)((IsV2) ? "TYE" : "TYER");
        case NvMp3MetaDataType_GENRE:
            return (NvS8*)((IsV2) ? "TCO" : "TCON");
        case NvMp3MetaDataType_COMMENT:
            return (NvS8*)((IsV2) ? "COM" : "COMM");
        case NvMp3MetaDataType_COMPOSER:
            return (NvS8*)((IsV2) ? "TCM" : "TCOM");
        case NvMp3MetaDataType_COPYRIGHT:
            return (NvS8*)((IsV2) ? "TCR" : "TCOP");
        case NvMp3MetaDataType_URL:
            return (NvS8*)((IsV2) ? "WXX" : "WXXX");
        case NvMp3MetaDataType_ENCODED:
            return (NvS8*)((IsV2) ? "TEN" : "TENC");
        case NvMp3MetaDataType_BPM:
            return (NvS8*)((IsV2) ? "TBP" : "TBPM");
        case NvMp3MetaDataType_PUBLISHER:
            return (NvS8*)((IsV2) ? "TPB" : "TPUB");
        case NvMp3MetaDataType_ORGARTIST:
            return (NvS8*)((IsV2) ? "TOA" : "TOPE");
        case NvMp3MetaDataType_ALBUMARTIST:
            return (NvS8*)((IsV2) ? "TP2" : "TPE2");
        default:
            return NULL;
    }
}

static NvError
Mp3ParserGetCoverArt (
    NvMMMetaDataInfo *pInfo,
    NvU8 *pInBuffer,
    NvU32 MetaSize)
{
    NvError Status = NvSuccess;
    // APIC frame data format.
    // text encoding: 1 byte, readBuffPtr should begin here.
    // mime type: zero end string.
    // picture type: 1 byte.
    // description: zero end string.
    // data: the remain bytes.
    NvU32 CoverArtSize = MetaSize;
    NvU8* pDataPtr = NULL;
    char* pMimeTypePtr = NULL;
    char* pDescPtr = NULL;

    if (pInBuffer)
    {
        if (*pInBuffer == 0x00 || *pInBuffer == 0x01) // iso-8859-1
        {
            pMimeTypePtr = (char *) (pInBuffer + 1);
            CoverArtSize -= 1;
            if (!NvOsStrcmp ("image/jpeg", pMimeTypePtr) ||
                    !NvOsStrcmp ("image/jpg", pMimeTypePtr))
            {
                pDescPtr = pMimeTypePtr + NvOsStrlen (pMimeTypePtr) + 2;
                CoverArtSize -= NvOsStrlen (pMimeTypePtr) + 2;
                pDataPtr = (NvU8*) (pDescPtr + NvOsStrlen (pDescPtr) + 1);
                CoverArtSize -= NvOsStrlen (pDescPtr) + 1;

                pInfo->eEncodeType = NvMMMetaDataEncode_JPEG;
            }
            else if (!NvOsStrcmp ("image/png", pMimeTypePtr))
            {
                pDescPtr = pMimeTypePtr + NvOsStrlen (pMimeTypePtr) + 2;
                CoverArtSize -= NvOsStrlen (pMimeTypePtr) + 2;
                pDataPtr = (NvU8*) (pDescPtr + NvOsStrlen (pDescPtr) + 1);
                CoverArtSize -= NvOsStrlen (pDescPtr) + 1;

                pInfo->eEncodeType = NvMMMetaDataEncode_PNG;
            }
            else
                pInfo->eEncodeType = NvMMMetaDataEncode_Other;

        }

        pInfo->nBufferSize = 0;
        if (CoverArtSize > 0 && pDataPtr)
        {
            pInfo->nBufferSize = CoverArtSize;
            pInfo->pClientBuffer = NvOsAlloc (CoverArtSize);
            NvOsMemcpy (pInfo->pClientBuffer, pDataPtr, CoverArtSize);
        }
    }

    return Status;
}

static NvError
Mp3ParserGetCoverArtV2 (
    NvMMMetaDataInfo *pInfo,
    NvU8 *pInBuffer,
    NvU32 MetaSize)
{
    NvError Status = NvSuccess;
    NvU32 CoverArtSize = MetaSize;
    NvU8* pDataPtr = NULL;
    char* pImageTypePtr = NULL;
    char* pDescPtr = NULL;

    if (pInBuffer)
    {
        if (*pInBuffer == 0x00 || *pInBuffer == 0x01) // iso-8859-1
        {
            pImageTypePtr = (char *) (pInBuffer + 1);
            CoverArtSize -= 1;
            if (!NvOsStrcmp ("JPG", pImageTypePtr))
            {
                pDescPtr = pImageTypePtr + NvOsStrlen (pImageTypePtr) + 1;
                CoverArtSize -= NvOsStrlen (pImageTypePtr) + 1;
                pDataPtr = (NvU8*) (pDescPtr + NvOsStrlen (pDescPtr) + 1);
                CoverArtSize -= NvOsStrlen (pDescPtr) + 1;
                pInfo->eEncodeType = NvMMMetaDataEncode_JPEG;
            }
            else if (!NvOsStrcmp ("PNG", pImageTypePtr))
            {
                pDescPtr = pImageTypePtr + NvOsStrlen (pImageTypePtr) + 1;
                CoverArtSize -= NvOsStrlen (pImageTypePtr) + 1;
                pDataPtr = (NvU8*) (pDescPtr + NvOsStrlen (pDescPtr) + 1);
                CoverArtSize -= NvOsStrlen (pDescPtr) + 1;
                pInfo->eEncodeType = NvMMMetaDataEncode_PNG;
            }
            else
                pInfo->eEncodeType = NvMMMetaDataEncode_Other;

        }

        pInfo->nBufferSize = 0;
        if (CoverArtSize > 0 && pDataPtr)
        {
            pInfo->nBufferSize = CoverArtSize;
            pInfo->pClientBuffer = NvOsAlloc (CoverArtSize);
            NvOsMemcpy (pInfo->pClientBuffer, pDataPtr, CoverArtSize);
        }
    }
    return Status;
}

static NvError
Mp3ParserReadId3Frame (const NvS8* pMetaTag,
                       NvU32  FrameSize,
                       NvU8 *pFrameData,
                       NvMp3MetaData *pId3TagInfo,
                       NvBool IsV2)
{
    NvError Status = NvSuccess;
    NvMMMetaDataCharSet EncType;
    NvU8 FrameIndex;
    NvS8* pMetaStr;
    NvU8 MetaTagLen;
    NvU32 i;

    if (pFrameData[0] == 0x00)  //If 0 then non-Unicode and if 1 then Unicode
    {
        FrameIndex = 1;
        EncType = NvMMMetaDataEncode_Utf8;
    }
    else if ( (pFrameData[0] == 0x01) && (pFrameData[1] == 0xFF) && (pFrameData[2] == 0xFE))  //We support only little endian Unicode 01FFFE
    {
        FrameIndex = 3;
        EncType = NvMMMetaDataEncode_Utf16;
    }
    else
    {
        Status = NvError_ParserFailedToGetData;
        goto cleanup;
    }

    MetaTagLen = (IsV2 == NV_TRUE) ? 3 : 4;
    for (i = 0; i < NvMp3MetaDataType_MAXMETADATA; i++)
    {
        pMetaStr = Mp3GetMetaDataTagName ( (NvMp3MetaDataType) i, IsV2);
        if (!NvOsStrncmp ((char *)pMetaTag, (char *)pMetaStr, MetaTagLen))
        {
            Mp3ParserGetMetaData (&pId3TagInfo->Id3MetaData[i], &pFrameData[FrameIndex], FrameSize - FrameIndex, EncType);
            break;
        }
    }

    if (IsV2 == NV_FALSE)
    {
        if (NvOsStrncmp ((char *)pMetaTag, (char *)"APIC", MetaTagLen) == 0)
        {
            Mp3ParserGetCoverArt (& pId3TagInfo->Mp3CoverArt, pFrameData, FrameSize);
        }
    }
    else
    {
        if (NvOsStrncmp ((char *)pMetaTag, (char *)"PIC", MetaTagLen) == 0)
        {
            Mp3ParserGetCoverArtV2 (& pId3TagInfo->Mp3CoverArt, pFrameData, FrameSize);
        }
    }

cleanup:
    return Status;
}

static NvError
Mp3ParserGetId3V2 (
    NvMp3Parser *pNvMp3Parser)
{
    NvError Status = NvSuccess;
    NvMp3Id3V2Header Id3V2Hdr;
    NvU32 ID3Size;
    NvU32 ExtHdrSize;
    NvU32 FrameSize;
    NvS32 NullChars = 0;
    NvU8 *pFrameData = NULL;
    NvS32 Length;
    NvS8 *pMetaTag = NULL;
    NvMp3FrameHeaderV22 FrameV22;
    NvMp3FrameHeaderV234 FrameV234;

    NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.SetPosition (pNvMp3Parser->hContentPipe, 0, CP_OriginBegin));
    NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.Read (pNvMp3Parser->hContentPipe, (CPbyte *) &Id3V2Hdr, sizeof (NvMp3Id3V2Header)));

    if (!NvOsStrncmp ( (char*) Id3V2Hdr.Tag, "ID3", 3))
    {
        NVMM_CHK_ARG (Id3V2Hdr.VersionMajor == MP3_Ver2_2 ||
                Id3V2Hdr.VersionMajor == MP3_Ver2_3 ||
                Id3V2Hdr.VersionMajor == MP3_Ver2_4);

        ID3Size = (Id3V2Hdr.Size[3] & 0x7F) |
                  ( (Id3V2Hdr.Size[2] & 0x7F) << 7) |
                  ( (Id3V2Hdr.Size[1] & 0x7F) << 14) |
                  ( (Id3V2Hdr.Size[0] & 0x7F) << 21);

        if (Id3V2Hdr.Flags & (1 << 6))
        {
            NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.Read (pNvMp3Parser->hContentPipe, (CPbyte *) &ExtHdrSize, sizeof (NvU32)));
            NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.SetPosition (pNvMp3Parser->hContentPipe, ExtHdrSize, CP_OriginCur));
        }

        while (ID3Size > 0)
        {
            if (Id3V2Hdr.VersionMajor == MP3_Ver2_2)
            {
                Length = sizeof (NvMp3FrameHeaderV22);
                NVMM_CHK_CP (pNvMp3Parser->pPipe->cpipe.Read (pNvMp3Parser->hContentPipe, (CPbyte *) &FrameV22, Length));
                if (!NvOsMemcmp (FrameV22.Tag, &NullChars, 3)) break;
                pMetaTag = FrameV22.Tag;
                FrameSize = FrameV22.Size[2] | (FrameV22.Size[1] << 8) | (FrameV22.Size[0] << 16);
            }
            else if (Id3V2Hdr.VersionMajor == MP3_Ver2_3 || MP3_Ver2_4)
            {
                Length = sizeof (NvMp3FrameHeaderV234);
                NVMM_CHK_CP (pNvMp3Parser->pPipe->cpipe.Read (pNvMp3Parser->hContentPipe, (CPbyte *) &FrameV234, Length));
                if (!NvOsMemcmp (FrameV234.Tag, &NullChars, 4)) break;
                pMetaTag = FrameV234.Tag;
                if (Id3V2Hdr.VersionMajor == MP3_Ver2_3)
                {
                    FrameSize = NV_BE_TO_INT_32 (FrameV234.Size);
                }
                else // version 4
                {
                    FrameSize = (FrameV234.Size[3] & 0x7F) |
                                ( (FrameV234.Size[2] & 0x7F) << 7) |
                                ( (FrameV234.Size[1] & 0x7F) << 14) |
                                ( (FrameV234.Size[0] & 0x7F) << 21);
                }
            }

            pFrameData = (NvU8 *) NvOsAlloc (FrameSize);
            NVMM_CHK_MEM (pFrameData);
            NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.Read (pNvMp3Parser->hContentPipe, (CPbyte *) pFrameData, FrameSize));

            //If the data is right, ie, if first character is zero that means
            //it is nonUnicode, and if first char is 1 then unicode and size >= 3
            if ( ( (pFrameData[0] == 0) && (FrameSize >= 1)) ||
                    ( (pFrameData[0] == 1) && (FrameSize >= 3)))
            {
                Mp3ParserReadId3Frame (pMetaTag, FrameSize, pFrameData, &pNvMp3Parser->Id3TagData, (Id3V2Hdr.VersionMajor == MP3_Ver2_2));
            }

            NvOsFree (pFrameData);
            pFrameData = NULL;
            ID3Size -= (Length + FrameSize);
        }
    }

cleanup:
    if (pFrameData != NULL)
    {
        NvOsFree (pFrameData);
        pFrameData = NULL;
    }

    return Status;
}

static NvError
Mp3ParserGetID3MetaData (
    NvMp3Parser *pNvMp3Parser)
{
    NvError Status = NvSuccess;

    Status = Mp3ParserGetId3V2 (pNvMp3Parser);
    Status = Mp3ParserGetId3V1 (pNvMp3Parser);

    return Status;
}

static void
Mp3ParserPrintTrackInfo (
    NvMp3TrackInfo *pTrackInfo,
    NvU32 FrameCounter)
{
    NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "\nTrackInfo Details:\n"));
    NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "********************\n"));

    if (pTrackInfo->Version == NvMp3Version_MPEG1)
        NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "Version = NvMp3Version_MPEG1\n"));
    else if (pTrackInfo->Version == NvMp3Version_MPEG2)
        NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "Version = NvMp3Version_MPEG2\n"));
    else if (pTrackInfo->Version == NvMp3Version_MPEG25)
        NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "Version = NvMp3Version_MPEG2.5\n"));
    else
        NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "Version = Un_Supported_Version\n"));

    if (pTrackInfo->Layer == NvMp3Layer_LAYER1)
        NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "Layer = Layer I\n"));
    else if (pTrackInfo->Layer == NvMp3Layer_LAYER2)
        NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "Layer = Layer II\n"));
    else if (pTrackInfo->Layer == NvMp3Layer_LAYER3)
        NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "Layer = Layer III\n"));
    else
        NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "Layer = Un_Supported_Layer\n"));

    if (pTrackInfo->VbrType == NvMp3VbrType_NONE)
        NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "VbrType = NvMp3VbrType_NONE\n"));
    else if (pTrackInfo->VbrType == NvMp3VbrType_OTHER)
        NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "VbrType = NvMp3VbrType_OTHER\n"));
    else if (pTrackInfo->VbrType == NvMp3VbrType_XING)
        NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "VbrType = NvMp3VbrType_XING\n"));
    else
        NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "VbrType = NvMp3VbrType_FHG\n"));

    NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "TotalFrames = %d\n", FrameCounter));
    NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "BitRate = %d\n", pTrackInfo->BitRate));
    NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "SamplingFrequency = %d\n", pTrackInfo->SamplingFrequency));
    NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "TotalTime in mSec = %lld\n", pTrackInfo->TotalTime));
    NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "Channels = %d\n", pTrackInfo->NumChannels));
    NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "DataOffset = %d\n", pTrackInfo->DataOffset));
    NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "UnPaddedFrames = %d\n", pTrackInfo->UnPaddedFrames));
    NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "PaddedFrames = %d\n", pTrackInfo->PaddedFrames));
    NV_LOGGER_PRINT ( (NVLOG_MP3_PARSER, NVLOG_DEBUG, "********************"));

}

NvError NvMp3ParserInit (
    NvMp3Parser* pNvMp3Parser,
    NvString pFilePath)
{
    NvError Status = NvSuccess;

    NvmmGetFileContentPipe (&pNvMp3Parser->pPipe);
    NVMM_CHK_ARG (pNvMp3Parser->pPipe);

    NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.Open (&pNvMp3Parser->hContentPipe, (CPstring) pFilePath, CP_AccessRead));
    NVMM_CHK_ARG (pNvMp3Parser->hContentPipe);

    pNvMp3Parser->NumberOfTracks = 1;
    
cleanup:
    return Status;
}

void NvMp3ParserDeInit (NvMp3Parser * pNvMp3Parser)
{
    NvU8 i;

    if (pNvMp3Parser)
    {
        for (i = 0; i < NvMp3MetaDataType_MAXMETADATA; i++)
        {
            NvOsFree (pNvMp3Parser->Id3TagData.Id3MetaData[i].pClientBuffer);
        }

        if (pNvMp3Parser->Id3TagData.ShoutCast.pClientBuffer)
            NvOsFree (pNvMp3Parser->Id3TagData.ShoutCast.pClientBuffer);

        if (pNvMp3Parser->Id3TagData.Mp3CoverArt.pClientBuffer)
            NvOsFree (pNvMp3Parser->Id3TagData.Mp3CoverArt.pClientBuffer);

        if (pNvMp3Parser->hContentPipe)
        {
            (void) pNvMp3Parser->pPipe->cpipe.Close (pNvMp3Parser->hContentPipe);
            pNvMp3Parser->hContentPipe = 0;
        }
    }
}

static NvError
Mp3ParserCheckFrameHeader(
    NvU8* pFrameHdr)
{
    NvError Status = NvSuccess;
    NvU32 FrameHdr;
    NvU32 NumChannels;

    FrameHdr = NV_BE_TO_INT_32 (pFrameHdr);

    // Check Frame sync
    if (GET_FRAME_SYNC (FrameHdr) != 0x7ff)
    {
        Status = NvError_BadValue;
        goto cleanup;
    }
    // Check for free bitrate or bad bitrate
    if ( (GET_FRAME_BITRATEINDEX (FrameHdr) == 0) || (GET_FRAME_BITRATEINDEX (FrameHdr) == 0xf))
    {
        Status = NvError_BadValue;
        goto cleanup;
    }
    // Check for invalid sampling frequency
    if (GET_FRAME_SAMPLINGRATE (FrameHdr) == 0x3)
    {
        Status = NvError_BadValue;
        goto cleanup;
    }

    if (GET_FRAME_LAYER (FrameHdr) == 0x0)
    {
        Status = NvError_BadValue;
        goto cleanup;
    }

    if (GET_FRAME_VERSION (FrameHdr) == 0x1)
    {
        Status = NvError_BadValue;
        goto cleanup;
    }

    if (GET_FRAME_EMPHASIS (FrameHdr) == 0x2)
    {
        goto cleanup;
    }

    NumChannels = (GET_FRAME_CHANNELMODE (FrameHdr) == 3) ? 1 : 2;
    if ( (GET_FRAME_LAYER (FrameHdr) == 0x2) && (GET_FRAME_VERSION (FrameHdr) == 3))
    {
        if (!s_AllowedModes[GET_FRAME_BITRATEINDEX (FrameHdr) ][NumChannels-1])
        {
            goto cleanup;
        }
    }

cleanup:
    return Status;
}

static NvU32
Mp3ParserGetFrameHeader (
    NvU8* pFrameHdr, 
    NvMp3TrackInfo* pTrackInfo, 
    NvU32 *pProtectionBit)
{
    NvS32 FrameSize = 0;
    NvU32 FrameHdr;

    FrameHdr = NV_BE_TO_INT_32 (pFrameHdr);

    pTrackInfo->Version = NvMp3Version_MPEG1;
    switch (GET_FRAME_VERSION (FrameHdr))
    {
        case 0:
            pTrackInfo->Version = NvMp3Version_MPEG25;
            break;
        case 2:
            pTrackInfo->Version = NvMp3Version_MPEG2;
            break;
        case 3:
            pTrackInfo->Version = NvMp3Version_MPEG1;
            break;
    }
    // Sample rate table has 3 diferent entries for Mpeg Versions
    pTrackInfo->SamplingFrequency = s_MygaSampleRate[pTrackInfo->Version][GET_FRAME_SAMPLINGRATE (FrameHdr) ];

    // Convert index to Layer type
    pTrackInfo->Layer = 3 - GET_FRAME_LAYER (FrameHdr);
    if (pTrackInfo->Layer == NvMp3Layer_NONE)
        pTrackInfo->Layer = NvMp3Layer_LAYER3;

    // Now for bitrateMpeg2 & Mpeg2.5 have only one set of entries.
    if (pTrackInfo->Version == NvMp3Version_MPEG25)
        pTrackInfo->BitRate = s_MygaBitrate[NvMp3Version_MPEG2][pTrackInfo->Layer][GET_FRAME_BITRATEINDEX (FrameHdr) ] * 1000;
    else
        pTrackInfo->BitRate = s_MygaBitrate[pTrackInfo->Version][pTrackInfo->Layer][GET_FRAME_BITRATEINDEX (FrameHdr) ] * 1000;

    pTrackInfo->NumChannels = (GET_FRAME_CHANNELMODE (FrameHdr) == 3) ? 1 : 2;

    *pProtectionBit = GET_FRAME_PROTECTION (FrameHdr);

    if (pTrackInfo->Layer == NvMp3Layer_LAYER1)
    {
        if (GET_FRAME_PADDING (FrameHdr))
        {
            FrameSize = pTrackInfo->SamplingFrequency == 0 ? 417 : ( ( (pTrackInfo->BitRate * 12) / pTrackInfo->SamplingFrequency + 1)) * 4;
            pTrackInfo->PaddedFrames++;
        }
        else
        {
            FrameSize = pTrackInfo->SamplingFrequency == 0 ? 417 : ( (pTrackInfo->BitRate * 12) / pTrackInfo->SamplingFrequency) * 4;
            pTrackInfo->UnPaddedFrames++;
        }
    }
    else //Layer2 & Layer3
    {
        if (GET_FRAME_PADDING (FrameHdr))
        {
            FrameSize = (pTrackInfo->SamplingFrequency == 0) ? 417 : ((((pTrackInfo->Version == NvMp3Version_MPEG25)||(pTrackInfo->Version == NvMp3Version_MPEG2)) && (pTrackInfo->Layer == NvMp3Layer_LAYER3)) ? 
                ((pTrackInfo->BitRate * 72)/pTrackInfo->SamplingFrequency + 1) : ((pTrackInfo->BitRate * 144)/pTrackInfo->SamplingFrequency + 1));

            pTrackInfo->PaddedFrames++;
        }
        else
        {
            FrameSize = (pTrackInfo->SamplingFrequency == 0) ? 417 : ((((pTrackInfo->Version == NvMp3Version_MPEG25)||(pTrackInfo->Version == NvMp3Version_MPEG2)) && (pTrackInfo->Layer == NvMp3Layer_LAYER3)) ? 
                ((pTrackInfo->BitRate * 72)/pTrackInfo->SamplingFrequency) : ((pTrackInfo->BitRate * 144)/pTrackInfo->SamplingFrequency));

            pTrackInfo->UnPaddedFrames++;
        }
    }

    return FrameSize;;
}

static NvError
Mp3ParserCheckXingTableValues (
    NvMp3Parser *pNvMp3Parser, 
    NvU8 *pTOC)
{
    NvError Status = NvSuccess;
    NvU32 Count = 0;
    NvU32 Position = 0;
    NvU32 ZerosCount = 0;
    NvU8 *pTOCPtr = pTOC;

    while (1)
    {
        if (Count >= XING_HEADER_TOC_TABLE_SIZE)
        {
            break;
        }
        Position = *pTOCPtr++;
        if (Position == 0xFF && (Count >= XING_HEADER_TOC_TABLE_SIZE))
        {
            NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "\nPosition : 0x%0x Count: %d\n", Position, Count));
            Count++;
            break;
        }
        else
        {
            if (Position == 0)
            {
                ZerosCount++;
            }
        }
        Count++;
    }

    if ( (Count < XING_HEADER_TOC_TABLE_SIZE) || (ZerosCount > XING_TOC_TABLE_ZEROS_MAX_SIZE))
    {
        NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "\nFound Xing Header is Corrupted : InSufficient TOC Table Values\n"));
        pNvMp3Parser->IsXingHeaderCorrupted = NV_TRUE;
    }
    else
    {
        NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "\nFound Xing Header is Not Corrupted : Sufficient TOC Table Values\n"));
        pNvMp3Parser->IsXingHeaderCorrupted = NV_FALSE;
    }

    return Status;

}

static NvS32
Mp3ParserGetSampleRate (
    NvU8 *pBuffer)
{
    NvU8 Id, Idx, Mpeg;

    Id  = (0xC0 & (pBuffer[1] << 3)) >> 4;
    Idx = (0xC0 & (pBuffer[2] << 4)) >> 6;

    Mpeg = Id | Idx;

    switch (Mpeg)
    {
        case 0 :
            return 11025;
        case 1 :
            return 12000;
        case 2 :
            return 8000;
        case 8 :
            return 22050;
        case 9 :
            return 24000;
        case 10:
            return 16000;
        case 12:
            return 44100;
        case 13:
            return 48000;
        case 14:
            return 32000;
        default:
            return 0;
    }
}

static NvError
Mp3ParserReadVbriHeader (
    NvMp3VbriHeader *pFhgVbr,
    NvU8 *pHBuffer)
{
    NvError Status = NvSuccess;
    NvU32 i ;
    
    // MPEG header
    pFhgVbr->pVBHeader->SampleRate = Mp3ParserGetSampleRate (pHBuffer);
    NVMM_CHK_ARG (pFhgVbr->pVBHeader->SampleRate);  // check: probably no MPEG header
    
    pFhgVbr->Position += sizeof (NvU32) ;
    // data indicating silence
    pFhgVbr->Position += (8 * sizeof (NvU32)) ;

    if (NvOsStrncmp((char*)pHBuffer + pFhgVbr->Position, (char *)"VBRI", 4))  // check: header mismatch
    {
        NV_LOGGER_PRINT((NVLOG_MP3_PARSER, NVLOG_DEBUG, "VBRI header not found"));
        Status = NvError_BadParameter;
        goto cleanup;
    }
    
    pFhgVbr->Position += sizeof (NvU32) ;
    pFhgVbr->pVBHeader->VbriVersion      = NV_BE_TO_INT_16 (&pHBuffer[pFhgVbr->Position]);
    pFhgVbr->pVBHeader->VbriDelay        = (float) NV_BE_TO_INT_16 (&pHBuffer[pFhgVbr->Position + sizeof (NvU16) ]);
    pFhgVbr->pVBHeader->VbriQuality      = NV_BE_TO_INT_16 (&pHBuffer[pFhgVbr->Position + 2*sizeof (NvU16) ]);
    pFhgVbr->pVBHeader->VbriStreamBytes  = NV_BE_TO_INT_32 (&pHBuffer[pFhgVbr->Position + 3*sizeof (NvU16) ]);
    pFhgVbr->pVBHeader->VbriStreamFrames = NV_BE_TO_INT_32 (&pHBuffer[pFhgVbr->Position + 3*sizeof (NvU16) + sizeof (NvU32) ]);
    pFhgVbr->pVBHeader->VbriTableSize    = NV_BE_TO_INT_16 (&pHBuffer[pFhgVbr->Position + 3*sizeof (NvU16) + 2*sizeof (NvU32) ]);
    pFhgVbr->pVBHeader->VbriTableScale   = NV_BE_TO_INT_16 (&pHBuffer[pFhgVbr->Position + 4*sizeof (NvU16) + 2*sizeof (NvU32) ]);
    pFhgVbr->pVBHeader->VbriEntryBytes   = NV_BE_TO_INT_16 (&pHBuffer[pFhgVbr->Position + 5*sizeof (NvU16) + 2*sizeof (NvU32) ]);
    pFhgVbr->pVBHeader->VbriEntryFrames  = NV_BE_TO_INT_16 (&pHBuffer[pFhgVbr->Position + 6*sizeof (NvU16) + 2*sizeof (NvU32) ]);
    pFhgVbr->Position += (7 * sizeof (NvU16) + 2 * sizeof (NvU32));

    NVMM_CHK_ARG (pFhgVbr->pVBHeader->VbriTableSize <= 512) // check: not more than 512 entries

    for (i = 0 ; i < pFhgVbr->pVBHeader->VbriTableSize ; i++)
    {
        if (pFhgVbr->pVBHeader->VbriEntryBytes == sizeof (NvU32))
        {
            pFhgVbr->pVBHeader->VbriTable[i] = NV_BE_TO_INT_32 (&pHBuffer[pFhgVbr->Position]) * pFhgVbr->pVBHeader->VbriTableScale;
        }
        else if (pFhgVbr->pVBHeader->VbriEntryBytes == sizeof (NvU16))
        {
            pFhgVbr->pVBHeader->VbriTable[i] = NV_BE_TO_INT_16 (&pHBuffer[pFhgVbr->Position]) * pFhgVbr->pVBHeader->VbriTableScale;
        }
        else if (pFhgVbr->pVBHeader->VbriEntryBytes == sizeof (NvU8))
        {
            pFhgVbr->pVBHeader->VbriTable[i] = pHBuffer[pFhgVbr->Position] * pFhgVbr->pVBHeader->VbriTableScale;
        }
        pFhgVbr->Position += pFhgVbr->pVBHeader->VbriEntryBytes;
    }

cleanup:
    return Status;
}

static NvError
Mp3ParserGetXingHeader (
    NvMp3XingHeader *pXingHdr,
    NvU8 *pBuf)
{
    NvS32 i, ID, HMode, HsrIndex, HeadFlags;

    pXingHdr->Flags = 0;

    // get selected MPEG header data
    ID       = (pBuf[1] >> 3) & 1;
    HsrIndex = (pBuf[2] >> 2) & 3;
    HMode     = (pBuf[3] >> 6) & 3;

    // determine offset of header
    if (ID)           // mpeg1
    {
        if (HMode != 3) pBuf += (32 + 4);
        else              pBuf += (17 + 4);
    }
    else        // mpeg2
    {
        if (HMode != 3) pBuf += (17 + 4);
        else              pBuf += (9 + 4);
    }

    if (NvOsStrncmp((char *)pBuf, (char *)"Xing", 4))
    {
        return NvError_ParserFailure;  // fail - header not found 
    }
    
    pBuf += 4;
    
    pXingHdr->ID = ID;
    pXingHdr->SampleRate = s_XingSampleRateTable[HsrIndex];
    if (ID == 0) pXingHdr->SampleRate >>= 1;

    HeadFlags = pXingHdr->Flags = NV_BE_TO_INT_32 (pBuf);
    pBuf += 4;    // get flags

    if (HeadFlags & NvMp3VbrFlag_FRAMES)
    {
        pXingHdr->Frames   = NV_BE_TO_INT_32 (pBuf);
        pBuf += 4;
    }
    if (HeadFlags & NvMp3VbrFlag_BYTES)
    {
        pXingHdr->Bytes = NV_BE_TO_INT_32 (pBuf);
        pBuf += 4;
    }

    if (HeadFlags & NvMp3VbrFlag_TOC)
    {
        if (pXingHdr->pTOCBuf != NULL)
        {
            for (i = 0; i < 100; i++) pXingHdr->pTOCBuf[i] = pBuf[i];
        }
        pBuf += 100;
    }

    pXingHdr->VbrScale = -1;
    if (HeadFlags & NvMp3VbrFlag_SCALE)
    {
        pXingHdr->VbrScale = NV_BE_TO_INT_32 (pBuf);
        pBuf += 4;
    }

    return NvSuccess;
}

static NvError
Mp3ParserGetVBRHeader (
    NvMp3Parser *pNvMp3Parser,
    NvMp3VbrHeaderGroup *pVbrObj,
    NvU8 *pFrameHdrBytes,
    NvMp3TrackInfo *pTrackInfo,
    NvU32 *pTotalFrames)
{
    NvError Status = NvSuccess;
    NvMp3XingHeader* pXingStruct = & (pVbrObj)->Mp3InfoXing.XingHeader;
    NvU32 CurrentPosition = 0;
    NvU32 Length = 0;
    NvU32 SamplesPerFrame = 0;

    NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.GetPosition (pNvMp3Parser->hContentPipe, (CPuint*) &CurrentPosition));  // Save the file position

    /* Get XING Header */
    pXingStruct->pTOCBuf = pVbrObj->Mp3InfoXing.XHDToc;
    NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.SetPosition (pNvMp3Parser->hContentPipe, pTrackInfo->FrameStart, CP_OriginBegin));
    NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.Read (pNvMp3Parser->hContentPipe, (CPbyte *) pVbrObj->XingBuff, MP3_XING_BUFF_SIZE));

    pVbrObj->XingResult = Mp3ParserGetXingHeader (pXingStruct, pVbrObj->XingBuff);
    Status = Mp3ParserCheckXingTableValues (pNvMp3Parser, pXingStruct->pTOCBuf);

    if (pVbrObj->XingResult == NvSuccess)
    {
        switch (pTrackInfo->SamplingFrequency)
        {
            case 48000:
                pVbrObj->Mp3InfoXing.XingHeader.FrameDuration = 24.0f;
                break;
            case 44100:
                pVbrObj->Mp3InfoXing.XingHeader.FrameDuration = 26.12f;
                break;
            case 32000:
                pVbrObj->Mp3InfoXing.XingHeader.FrameDuration = 36.0f;
                break;
            case 24000:
                pVbrObj->Mp3InfoXing.XingHeader.FrameDuration = 24.0f;
                break;
            case 22050:
                pVbrObj->Mp3InfoXing.XingHeader.FrameDuration = 26.12f;
                break;
            case 16000:
                pVbrObj->Mp3InfoXing.XingHeader.FrameDuration = 36.0f;
                break;
            case 12000:
                pVbrObj->Mp3InfoXing.XingHeader.FrameDuration = 48.0f;
                break;
            case 11025:
                pVbrObj->Mp3InfoXing.XingHeader.FrameDuration = 52.24f;
                break;
            case 8000:
                pVbrObj->Mp3InfoXing.XingHeader.FrameDuration = 72.0f;
                break;
            default:
                pVbrObj->Mp3InfoXing.XingHeader.FrameDuration = 26.12f;
                break;
        }

        pTrackInfo->TotalTime = (NvU64) (pVbrObj->Mp3InfoXing.XingHeader.Frames * pVbrObj->Mp3InfoXing.XingHeader.FrameDuration * 1000) / 1000;
        pTrackInfo->VbrType = NvMp3VbrType_XING;
        *pTotalFrames = (NvU32) (pVbrObj->Mp3InfoXing.XingHeader.Frames);        
    }
    else 
    {
        /* Get FHG header */
        NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.SetPosition (pNvMp3Parser->hContentPipe, pTrackInfo->FrameStart, CP_OriginBegin));

        Length = MP3_FHG_BUFF_SIZE + MP3_XING_BUFF_SIZE;

        if (pNvMp3Parser->FileSizeInBytes < Length)
            Length = (NvU32) (pNvMp3Parser->FileSizeInBytes - 4);

        if (pNvMp3Parser->FileSizeInBytes - pTrackInfo->FrameStart < Length)
        {
            Length = (NvU32) pNvMp3Parser->FileSizeInBytes - pTrackInfo->FrameStart;
        }

        NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.Read (pNvMp3Parser->hContentPipe, (CPbyte *) pVbrObj->XingBuff, Length));

        pVbrObj->FhgVbr.Position = 0;
        pVbrObj->FhgVbr.pVBHeader = (NvMp3VbriHeaderTable *) & pVbrObj->Mp3InfoFhg.FhgVbr;
        pVbrObj->FhgResult = Mp3ParserReadVbriHeader (&pVbrObj->FhgVbr, pVbrObj->XingBuff);

        if (pVbrObj->FhgResult == NvSuccess)
        {
            (pVbrObj->Mp3InfoFhg.FhgVbr.SampleRate >= 32000) ? (SamplesPerFrame = 1152) : (SamplesPerFrame = 576) ;

            pTrackInfo->TotalTime    = (NvU64) ( ( (float) pVbrObj->Mp3InfoFhg.FhgVbr.VbriStreamFrames * (float) SamplesPerFrame * 1000)
                                                 / (float) pVbrObj->Mp3InfoFhg.FhgVbr.SampleRate) ;

            pTrackInfo->VbrType = NvMp3VbrType_FHG;
            *pTotalFrames = (NvU32) (pVbrObj->Mp3InfoFhg.FhgVbr.VbriStreamFrames);
        }

    }

cleanup:
    NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.SetPosition (pNvMp3Parser->hContentPipe, CurrentPosition, CP_OriginBegin));
    return Status;
}

static NvU64 Mp3ParserGetFrameDuration(NvMp3TrackInfo* pTrackInfo)
{
    NvU32 SampleFreqIndex = 1; // defaults to 44100
    NvU32 LayerIndex = 2; // defaults to NvMp3Layer_LAYER3
    NvU32 i;
    
    for (i = 0; i < 9; i++)
    {
        if (pTrackInfo->SamplingFrequency == s_SamplingFreqTable[i])
        {
            SampleFreqIndex = i;
            break;
        }
    }

    if (pTrackInfo->Layer == NvMp3Layer_LAYER1 || pTrackInfo->Layer == NvMp3Layer_LAYER2)
    {
            LayerIndex = pTrackInfo->Layer;
    }

    return s_FrameDuration[LayerIndex][SampleFreqIndex];
}

static void 
Mp3ParserFrameCompare(
    NvU32 FrameCounter,
    NvMp3TrackInfo *pCurrent, 
    NvMp3TrackInfo *pPrevious,
    NvMp3FrameCompareData* pFrameCompareData)
{
    if (FrameCounter > 1)
    {
        if (pCurrent->Layer != pPrevious->Layer)
        {
            pFrameCompareData->LayerChangeCount++;
            NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "\nLayer Changed in Frame %d from Layer %d To Layer %d\n", FrameCounter, pPrevious->Layer + 1, pCurrent->Layer + 1));

        }
        else
        {
            if (pFrameCompareData->LayerUnChangeCount <= MAX_LAYER_NO_CHANGE_COUNT)
            {
                pFrameCompareData->LayerUnChangeCount++;
            }
            else
            {
                pFrameCompareData->LayerFinal = pCurrent->Layer;
            }
        }

        if (pCurrent->SamplingFrequency != pPrevious->SamplingFrequency)
        {
            pFrameCompareData->SampleRateChangeCount++;
            NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "\nSampleRate Changed in Frame %d from %d To %d\n", FrameCounter, pPrevious->SamplingFrequency, pCurrent->SamplingFrequency));
        }
        else
        {
            if (pFrameCompareData->SampleRateUnChangeCount <= MAX_SAMPLERATE_NO_CHANGE_COUNT)
            {
                pFrameCompareData->SampleRateUnChangeCount++;
            }
            else
            {
                pFrameCompareData->SampleRateFinal = pCurrent->SamplingFrequency;
            }            
        }

        if (pCurrent->Version != pPrevious->Version)
        {
            pFrameCompareData->VersionChangeCount++;
            NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "MPEG Version Changed in Frame %d from MPEG %d To MPEG %d\n", FrameCounter, pPrevious->Version, pCurrent->Version));
        }
        else
        {
            if (pFrameCompareData->VersionUnChangeCount <= MAX_VERSION_NO_CHANGE_COUNT)
            {
                pFrameCompareData->VersionUnChangeCount++;
            }
            else
            {
                pFrameCompareData->VersionFinal = pCurrent->Version;
            }           
        }

        if (pCurrent->BitRate != pPrevious->BitRate)
        {
            pFrameCompareData->BitRateChangeCount++;
            NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "BitRate Changed in Frame %d from %d To %d\n", FrameCounter, pPrevious->BitRate, pCurrent->BitRate));
        }
        else
        {
            if (pFrameCompareData->BitRateUnChangeCount <= MAX_BITRATE_NO_CHANGE_COUNT)
            {
                pFrameCompareData->BitRateUnChangeCount++;
            }
            else
            {
                pFrameCompareData->BitRateFinal = pCurrent->BitRate;
            }           
        }

        if (pCurrent->FrameStart != (pPrevious->FrameStart + pPrevious->FrameSize))
        {
            NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "Frame %d header expected at byte %d, but found at byte %d\n", 
                FrameCounter, pPrevious->FrameStart + pPrevious->FrameSize, pCurrent->FrameStart));
            
            NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "Frame %d (bytes %d-%d) was %d bytes long (expected %d bytes)\n", 
                FrameCounter - 1, pPrevious->FrameStart, pCurrent->FrameStart, (pCurrent->FrameStart - pPrevious->FrameStart), pPrevious->FrameSize));
        }
        else
        {
            if ( (pCurrent->Layer == pPrevious->Layer) || (pCurrent->SamplingFrequency == pPrevious->SamplingFrequency) || (pCurrent->Version == pPrevious->Version))
            {
                pFrameCompareData->FrameMatchCount++;
                if (pFrameCompareData->FrameMatchCount == 1)
                {
                    pFrameCompareData->DataOffsetFinal = pPrevious->FrameStart;
                }
            }
        }
    }
    else
    {
        pFrameCompareData->DataOffsetFinal = pCurrent->FrameStart;
    }
    
}

static NvError
Mp3ParserGetLastFrameBoundary (
    NvMp3Parser *pNvMp3Parser)
{
    NvError Status = NvSuccess;
    NvU64 ChunkOffset=0;
    NvU32 SyncFound = 0;
    NvU32 FrameStart = 0, FrameSize = 0, LastFrameSync = 0;
    NvU32 ProtectionBit;
    NvMp3TrackInfo TrackInfo;
    NvU8 Buffer[BUFFER_CHUNK_SIZE];
    NvU32 ChunkPos = 0, ChunkSize = 0;
    NvU64 FileSizeLimit = 0;
  
    FileSizeLimit = pNvMp3Parser->FileSizeInBytes - 128*pNvMp3Parser->IsTAGDetected;
    
    if (FileSizeLimit >= BUFFER_CHUNK_SIZE)
    {
        ChunkOffset = FileSizeLimit - BUFFER_CHUNK_SIZE;
        ChunkSize = BUFFER_CHUNK_SIZE;
    }
    else if (FileSizeLimit >= BUFFER_MIN_CHUNK_SIZE)
    {
        ChunkOffset = FileSizeLimit - BUFFER_MIN_CHUNK_SIZE;
        ChunkSize = BUFFER_MIN_CHUNK_SIZE;
    }
    if(!pNvMp3Parser->IsStreaming)
        pNvMp3Parser->pPipe->StopCaching (pNvMp3Parser->hContentPipe);

    while (!SyncFound && ((FileSizeLimit - ChunkOffset) < BUFFER_MAX_CHUNK_SIZE))
    {
        NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.SetPosition (pNvMp3Parser->hContentPipe, (NvU32) ChunkOffset, CP_OriginBegin));
        NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.Read (pNvMp3Parser->hContentPipe, (CPbyte *) Buffer, ChunkSize));

        ChunkPos = 0;
        while (ChunkPos < ChunkSize)
        {
            if (Mp3ParserCheckFrameHeader (&Buffer[ChunkPos]) == NvSuccess)
            {
                FrameSize = Mp3ParserGetFrameHeader (&Buffer[ChunkPos], &TrackInfo, &ProtectionBit);
                SyncFound++;
                if ( (pNvMp3Parser->StreamInfo.TrackInfo.VbrType == NvMp3VbrType_XING) || (pNvMp3Parser->StreamInfo.TrackInfo.VbrType == NvMp3VbrType_FHG))
                {
                    LastFrameSync = ChunkPos;
                    ChunkPos++;
                }
                else
                {
                    FrameStart = ChunkPos;
                    ChunkPos += FrameSize;
                }
            }
            else
            {
                ChunkPos++;
            }
        }

        if (!SyncFound)
        {
            ChunkOffset -= ChunkSize;
        }
    }

    if (!SyncFound)
    {
        Status = NvError_ParserCorruptedStream;
        goto cleanup;
    }
    else
    {
        if ( (pNvMp3Parser->StreamInfo.TrackInfo.VbrType == NvMp3VbrType_XING) || (pNvMp3Parser->StreamInfo.TrackInfo.VbrType == NvMp3VbrType_FHG))
        {
            pNvMp3Parser->LastFrameBoundary = ChunkOffset + LastFrameSync;
        }
        else
        {
            pNvMp3Parser->LastFrameBoundary = ChunkOffset + FrameStart + FrameSize;
            if (pNvMp3Parser->LastFrameBoundary > FileSizeLimit)
            {
                pNvMp3Parser->LastFrameBoundary = FileSizeLimit;
            }
        }
    }

cleanup:
    if(!pNvMp3Parser->IsStreaming)
        pNvMp3Parser->pPipe->StartCaching (pNvMp3Parser->hContentPipe);
    return Status;
}

static NvError
Mp3ParserGetTrackInfo (
    NvMp3Parser *pNvMp3Parser)
{
    NvError Status = NvSuccess;
    NvU32 FrameCounter = 0;
    NvU64 TotalBitRate = 0;
    NvU32 AverageBitRate = 0;
    NvU32 ProtectionBit = 0;
    NvU32 Id3Count = 0;
    NvU32 HandleVBR = 0;
    NvU32 VBRFrames = 0;
    NvU32 DataOffsetCurrentPosition = 0;
    NvU8 FrameData[MAX_HEADER_PARSING_SIZE];
    NvU8 ValidFrameFound = 0;
    NvU8* ReadBuff;
    NvU32 ReadLen;
    NvU32 FrameIndex;
    NvU32 SyncNotFoundCount = 0;
    NvU32 Id3Size = 0;
    NvMp3Id3Header Id3Hdr;
    NvMp3TrackInfo CurrentTrackInfo, PreviousTrackInfo;
    NvMp3FrameCompareData FrameCompareData;
    NvMp3TrackInfo *pTrackInfo = NULL;
    NvMp3VbrHeaderGroup *pVbrObj = NULL;

    pTrackInfo = &pNvMp3Parser->StreamInfo.TrackInfo;
    pVbrObj = &pNvMp3Parser->StreamInfo.VBRHeader;

    NVMM_CHK_ERR (pNvMp3Parser->pPipe->GetSize(pNvMp3Parser->hContentPipe, (CPuint64*)&pNvMp3Parser->FileSizeInBytes));
       
    /* init track info */
    pTrackInfo->BitRate      = 128000;
    pTrackInfo->SamplingFrequency = 44100;
    pTrackInfo->TotalTime   = 0;
    pTrackInfo->NumChannels = 2;
    pTrackInfo->SampleSize   = 16;
    pTrackInfo->DataOffset  = 0;
    pTrackInfo->Layer = NvMp3Layer_LAYER3;
    pTrackInfo->Version = NvMp3Version_MPEG1;
    pTrackInfo->UnPaddedFrames = 0;
    pTrackInfo->PaddedFrames = 0;
    pTrackInfo->FrameStart = 0;
    pTrackInfo->FrameSize = 0;
    pTrackInfo->VbrType = NvMp3VbrType_NONE;
    NvOsMemcpy (&CurrentTrackInfo, pTrackInfo, sizeof (NvMp3TrackInfo));

    /* init  frame compare data */
    NvOsMemset(&FrameCompareData, 0, sizeof(NvMp3FrameCompareData));
    FrameCompareData.LayerFinal=NvMp3Layer_LAYER3;
    FrameCompareData.VersionFinal = NvMp3Version_MPEG1;    
    
    /* extract metadata. ignore the error as the metadata presence is optional */
    (void) Mp3ParserGetID3MetaData (pNvMp3Parser);
    
    NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.SetPosition (pNvMp3Parser->hContentPipe, 0, CP_OriginBegin));

    /* search for multiple ID3 tags */
    for (Id3Count = 0; Id3Count < MAX_ID3_TAG_COUNT; Id3Count++)
    {
        NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.Read (pNvMp3Parser->hContentPipe,
                (CPbyte *) &Id3Hdr, sizeof (NvMp3Id3Header)));

        if (!NvOsStrncmp ( (char *) Id3Hdr.TagID, "ID3", 3))
        {
            Id3Size = Id3Hdr.Size[0] * 2097152 + Id3Hdr.Size[1] * 16384 + Id3Hdr.Size[2] * 128 + Id3Hdr.Size[3]; //compute the ID3 data size
            pTrackInfo->DataOffset += Id3Size + sizeof (NvMp3Id3Header);
            NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.SetPosition (pNvMp3Parser->hContentPipe,
                    Id3Size, CP_OriginCur));
        }
        else
        {
            NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.SetPosition (pNvMp3Parser->hContentPipe,
                    pTrackInfo->DataOffset, CP_OriginBegin));
            break;
        }
    }

    DataOffsetCurrentPosition = pTrackInfo->DataOffset;
    NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "Data offset before searching for valid frame: %u at line %u\n", 
                                      DataOffsetCurrentPosition, __LINE__));

    while (1)
    {
        /* search for a valid frame */
        ValidFrameFound = 0;
        ReadBuff = FrameData;
        ReadLen = MAX_HEADER_PARSING_SIZE;
        while (!ValidFrameFound)
        {
            NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.Read (pNvMp3Parser->hContentPipe, (CPbyte *) ReadBuff, ReadLen));

            // Check for valid frame
            for (FrameIndex = 0; FrameIndex < MAX_HEADER_PARSING_SIZE - 3; FrameIndex++)
            {
                Status = Mp3ParserCheckFrameHeader (&FrameData[FrameIndex]); // frame header needs 4 bytes to process
                if (Status == NvSuccess)
                {
                    ValidFrameFound = 1;
                    break;
                }
                else
                {
                    SyncNotFoundCount++;
                    NVMM_CHK_ARG (SyncNotFoundCount <= MAX_SYNC_NOT_FOUND_COUNT);
                    DataOffsetCurrentPosition++;
                }
            }
            if (!ValidFrameFound)
            {
                NvOsMemcpy (&FrameData[0], &FrameData[MAX_HEADER_PARSING_SIZE-3], 3), // copy the remaining 3 bytes into the start of the buffer
                ReadBuff = &FrameData[3];
                ReadLen = MAX_HEADER_PARSING_SIZE - 3;
            }
        }

        FrameCounter++;
        NvOsMemcpy (&PreviousTrackInfo, &CurrentTrackInfo, sizeof (NvMp3TrackInfo));
        CurrentTrackInfo.FrameStart = DataOffsetCurrentPosition;
        CurrentTrackInfo.FrameSize = Mp3ParserGetFrameHeader (&FrameData[FrameIndex], &CurrentTrackInfo, &ProtectionBit);
        DataOffsetCurrentPosition += CurrentTrackInfo.FrameSize;
        NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_VERBOSE, "Frame %u : FrameStart position (%u), framesize (%u) \n", 
                    FrameCounter, CurrentTrackInfo.FrameStart, CurrentTrackInfo.FrameSize));
        
        if (!HandleVBR)
        {
            Status = Mp3ParserGetVBRHeader(pNvMp3Parser, pVbrObj, &FrameData[FrameIndex], &CurrentTrackInfo, &VBRFrames);
            HandleVBR = 1;

            if ((pVbrObj->XingResult == NvSuccess) || (pVbrObj->FhgResult == NvSuccess))
            {
                NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_VERBOSE, "Equating DataOffsetFinal (%u) and FrameStart (%u) at line %u\n", 
                        FrameCompareData.DataOffsetFinal, CurrentTrackInfo.FrameStart, __LINE__));
                FrameCompareData.DataOffsetFinal = CurrentTrackInfo.FrameStart;
            }

            if (CurrentTrackInfo.VbrType == NvMp3VbrType_FHG || CurrentTrackInfo.VbrType == NvMp3VbrType_XING)
            {
                goto vbrexit;
            }
        }

        
        // Calculate the next frame file position based on Remaining bytes after obtaining the valid frame
        // i.e. Remaining Bytes = MAX_HEADER_PARSING_SIZE -FrameIndex
        NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.SetPosition (pNvMp3Parser->hContentPipe, CurrentTrackInfo.FrameSize - (MAX_HEADER_PARSING_SIZE - FrameIndex), CP_OriginCur));

        TotalBitRate += CurrentTrackInfo.BitRate / 1000;

        Mp3ParserFrameCompare(FrameCounter, &CurrentTrackInfo, &PreviousTrackInfo, &FrameCompareData);

        if (FrameCounter > MAXIMUM_VALID_FRAME_COUNT)
        {
            if ( (FrameCompareData.LayerChangeCount <= 1) && 
                  (FrameCompareData.SampleRateChangeCount <= 1) && 
                  (FrameCompareData.VersionChangeCount <= 1) && 
                  (FrameCompareData.BitRateChangeCount <= 1))
            {
                CurrentTrackInfo.VbrType = NvMp3VbrType_NONE;
                FrameCompareData.BitRateFinal = CurrentTrackInfo.BitRate;
                goto validframexit;
            }
        }
    }

cleanup:
    if (Status == NvError_EndOfFile)
    {
        Status =  NvError_ParserEndOfStream;
    }

    if ((PreviousTrackInfo.FrameStart + PreviousTrackInfo.FrameSize) < pNvMp3Parser->FileSizeInBytes)
    {
        pNvMp3Parser->LastFrameBoundary = PreviousTrackInfo.FrameStart + PreviousTrackInfo.FrameSize;
        NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "\nFree space or unrecognized Tag after last frame %d (bytes %d-%d)\n", FrameCounter, PreviousTrackInfo.FrameStart + PreviousTrackInfo.FrameSize, pNvMp3Parser->FileSizeInBytes));
    }
    else
    {
        pNvMp3Parser->LastFrameBoundary = pNvMp3Parser->FileSizeInBytes;
    }

validframexit:
    if (FrameCounter == 0)
    {
        Status = NvError_ParserCorruptedStream;

        pTrackInfo->BitRate = 0;
        pTrackInfo->SamplingFrequency = 0;
        pTrackInfo->TotalTime = 0;
        pTrackInfo->Layer = NvMp3Layer_NONE;
        pTrackInfo->NumChannels = 0;

        NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "\nNo Frames Found - ParserCorruptedStream\n"));
        goto errorexit;
    }

    if (FrameCounter <= MAXIMUM_VALID_FRAME_COUNT)
    {
        if ((FrameCompareData.LayerChangeCount == 0) && 
            (FrameCompareData.SampleRateChangeCount == 0) && 
            (FrameCompareData.VersionChangeCount == 0) && 
            (FrameCompareData.BitRateChangeCount == 0))
        {
            CurrentTrackInfo.VbrType = NvMp3VbrType_NONE;
            FrameCompareData.BitRateFinal = CurrentTrackInfo.BitRate;
        }
        else if(FrameCompareData.BitRateChangeCount < MAX_BITRATE_CHANGE_COUNT)
        {
            CurrentTrackInfo.VbrType = NvMp3VbrType_OTHER;
            FrameCompareData.BitRateFinal = (NvU32) (TotalBitRate * 1000 / FrameCounter);
        }
    }

    if (FrameCompareData.BitRateChangeCount > MAX_BITRATE_CHANGE_COUNT)
    {
        CurrentTrackInfo.VbrType = NvMp3VbrType_OTHER;
        AverageBitRate = (NvU32) (TotalBitRate * 1000 / FrameCounter);
        CurrentTrackInfo.BitRate = AverageBitRate;
    }
    else
        CurrentTrackInfo.BitRate = FrameCompareData.BitRateFinal;

vbrexit:
    CurrentTrackInfo.DataOffset = FrameCompareData.DataOffsetFinal;

    if ( (FrameCounter >= MAX_LAYER_NO_CHANGE_COUNT) && (FrameCounter >= MAX_VERSION_NO_CHANGE_COUNT) && (FrameCounter >= MAX_SAMPLERATE_NO_CHANGE_COUNT))
    {
        CurrentTrackInfo.Layer = FrameCompareData.LayerFinal;
        CurrentTrackInfo.Version = FrameCompareData.VersionFinal;
        CurrentTrackInfo.SamplingFrequency = FrameCompareData.SampleRateFinal;
    }

    NvOsMemcpy (pTrackInfo, &CurrentTrackInfo, sizeof (NvMp3TrackInfo));

    if ( (CurrentTrackInfo.VbrType == NvMp3VbrType_XING) || (CurrentTrackInfo.VbrType == NvMp3VbrType_FHG))
    {
        FrameCounter = VBRFrames;
        if (FrameCounter == 0)
        {
            pTrackInfo->TotalTime = (NvU64) ( (pNvMp3Parser->FileSizeInBytes - pTrackInfo->DataOffset) * 8.0f * 1000 / pTrackInfo->BitRate);
        }
    }
    else if (CurrentTrackInfo.VbrType == NvMp3VbrType_OTHER)
    {
        NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "FrameCounter=%d,Mp3ParserGetFrameDuration=%d ", FrameCounter, Mp3ParserGetFrameDuration(pTrackInfo)));
        pTrackInfo->TotalTime = (FrameCounter * Mp3ParserGetFrameDuration(pTrackInfo)) / 1000;
    }
    else
    {
        pTrackInfo->TotalTime = (NvU64) ( (pNvMp3Parser->FileSizeInBytes - pTrackInfo->DataOffset) * 8.0f * 1000 / pTrackInfo->BitRate);
    }

    Mp3ParserPrintTrackInfo (pTrackInfo, FrameCounter);
    // Get Last frame boundary only for non-streaming cases
    if(!pNvMp3Parser->IsStreaming)
    {
        Status = Mp3ParserGetLastFrameBoundary (pNvMp3Parser);
        if (Status != NvSuccess)
        {
            NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "\nFailed to GetLastFrameBoundary - 0x%X\n", Status));
        }
    }

    Status = pNvMp3Parser->pPipe->cpipe.SetPosition (pNvMp3Parser->hContentPipe, pTrackInfo->DataOffset , CP_OriginBegin);
    if (Status != NvSuccess)
    {
        return Status;
    }
    
errorexit:
    if ( (pTrackInfo->TotalTime == 0) || (pTrackInfo->Layer == NvMp3Layer_NONE) || (pTrackInfo->SamplingFrequency == 0) || (pTrackInfo->BitRate == 0) || (pTrackInfo->NumChannels == 0))
    {
        Status = NvError_ParserCorruptedStream;
        NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "\n ParserCorruptedStream\n"));
    }

    if ((FrameCompareData.LayerChangeCount > MAX_LAYER_CHANGE_COUNT) && 
        (FrameCompareData.VersionChangeCount > MAX_VERSION_CHANGE_COUNT) && 
        (FrameCompareData.SampleRateChangeCount > MAX_SAMPLERATE_CHANGE_COUNT) && 
        (pTrackInfo->Layer != NvMp3Layer_LAYER3))
    {
        Status = NvError_ParserCorruptedStream;

        pTrackInfo->BitRate = 0;
        pTrackInfo->SamplingFrequency = 0;
        pTrackInfo->TotalTime = 0;
        pTrackInfo->Layer = NvMp3Layer_NONE;
        pTrackInfo->NumChannels = 0;

        NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "\nWorst File - ParserCorruptedStream LayerChangeCount : %d    VersionChangeCount : %d   SampleRateChangeCount : %d\n",
            FrameCompareData.LayerChangeCount, FrameCompareData.VersionChangeCount, FrameCompareData.SampleRateChangeCount));
    }

    return Status;
}

static NvError
Mp3ParserGetHeader (
    NvMp3Parser *pNvMp3Parser)
{
    NvError Status = NvSuccess;
    NvS32 TocDelta, TocDelta1, TocDelta2, TocCount;
    NvBool XingHeaderTest = 0;
    NvMp3TrackInfo *pTrackInfo = &pNvMp3Parser->StreamInfo.TrackInfo;
    NvMp3VbrHeaderGroup *pVbrObj = &pNvMp3Parser->StreamInfo.VBRHeader;
    NvMp3XingHeader *pXingHdr = &pVbrObj->Mp3InfoXing.XingHeader;
    
    pVbrObj->VbrTocError = 0;
    pVbrObj->VbrFirstTocError = 0;
    pVbrObj->XingResult = 0;
    pVbrObj->FhgResult = 0;

    NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.SetPosition (pNvMp3Parser->hContentPipe, pTrackInfo->DataOffset, CP_OriginBegin));

    if (!XingHeaderTest)
    {
        pXingHdr->pTOCBuf = pVbrObj->Mp3InfoXing.XHDToc;

        NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.Read (pNvMp3Parser->hContentPipe, (CPbyte *) pVbrObj->XingBuff, MP3_FHG_BUFF_SIZE));
        pVbrObj->XingResult = Mp3ParserGetXingHeader (pXingHdr, (unsigned char*) pVbrObj->XingBuff);

        switch (pTrackInfo->SamplingFrequency)
        {
            case 48000:
                pVbrObj->Mp3InfoXing.XingHeader.FrameDuration = 24.0f;
                break;
            case 44100:
                pVbrObj->Mp3InfoXing.XingHeader.FrameDuration = 26.12f;
                break;
            case 32000:
                pVbrObj->Mp3InfoXing.XingHeader.FrameDuration = 36.0f;
                break;
            case 24000:
                pVbrObj->Mp3InfoXing.XingHeader.FrameDuration = 24.0f;
                break;
            case 22050:
                pVbrObj->Mp3InfoXing.XingHeader.FrameDuration = 26.12f;
                break;
            case 16000:
                pVbrObj->Mp3InfoXing.XingHeader.FrameDuration = 36.0f;
                break;
            case 12000:
                pVbrObj->Mp3InfoXing.XingHeader.FrameDuration = 48.0f;
                break;
            case 11025:
                pVbrObj->Mp3InfoXing.XingHeader.FrameDuration = 52.24f;
                break;
            case 8000:
                pVbrObj->Mp3InfoXing.XingHeader.FrameDuration = 72.0f;
                break;
            default:
                pVbrObj->Mp3InfoXing.XingHeader.FrameDuration = 26.12f;
                break;
        }

        //check if first toc entry is set to zero
        if (pXingHdr->pTOCBuf[0] != 0)
        {
            pVbrObj->VbrFirstTocError = 1;
        }

        for (TocCount = 0; TocCount < 96; TocCount++)
        {
            TocDelta = TocDelta1 = TocDelta2 = 0;
            if (pXingHdr->pTOCBuf[TocCount] > pXingHdr->pTOCBuf[TocCount+1])
            {
                if (pXingHdr->pTOCBuf[TocCount+1] < pXingHdr->pTOCBuf[TocCount+2])
                {
                    TocDelta1 = pXingHdr->pTOCBuf[TocCount+2] - pXingHdr->pTOCBuf[TocCount+1];
                    if (pXingHdr->pTOCBuf[TocCount+2] < pXingHdr->pTOCBuf[TocCount+3])
                    {
                        TocDelta2 = pXingHdr->pTOCBuf[TocCount+3] - pXingHdr->pTOCBuf[TocCount+2];
                    }
                }
                TocDelta = (TocDelta1 + TocDelta2) / 2;
                pXingHdr->pTOCBuf[TocCount] = pXingHdr->pTOCBuf[TocCount+1] - (OMX_U8) TocDelta;
                pVbrObj->VbrTocError = 1;
            }

        }

        XingHeaderTest = 1;
    }

    if (pVbrObj->XingResult)
    {
        pVbrObj->FhgVbr.Position = 0;
        pVbrObj->FhgVbr.pVBHeader = (NvMp3VbriHeaderTable *) & pVbrObj->Mp3InfoFhg.FhgVbr;
        pVbrObj->FhgResult = Mp3ParserReadVbriHeader (&pVbrObj->FhgVbr, pVbrObj->XingBuff);
    }

    NVMM_CHK_ERR (pNvMp3Parser->pPipe->cpipe.SetPosition (pNvMp3Parser->hContentPipe, 0, CP_OriginBegin));

cleanup:
    return Status;
}

NvError
NvMp3ParserParse (
    NvMp3Parser *pNvMp3Parser)
{
    NvError Status = NvSuccess;
    NvU32 MillSec = 0;

    NVMM_CHK_ARG (pNvMp3Parser);

    Status = Mp3ParserGetTrackInfo (pNvMp3Parser);
    if (Status == NvSuccess)
    {
        Mp3ParserGetHeader (pNvMp3Parser);
        NvMp3ParserSeekToTime (pNvMp3Parser, &MillSec);
    }

cleanup:
    return Status;
}

static NvS32
Mp3ParserGetSeekPoint (
    NvU8 *pTOCBuf,
    NvS32 FileBytes,
    float Percent)
{
    // interpolate in TOC to get file seek point in bytes
    NvS32 a, SeekPoint;
    float fa, fb, fx;

    if (Percent < 0.0f)   Percent = 0.0f;
    if (Percent > 100.0f) Percent = 100.0f;

    a = (NvS32) Percent;
    if (a > 99) a = 99;
    fa = pTOCBuf[a];
    if (a < 99)
    {
        fb = pTOCBuf[a+1];
    }
    else
    {
        fb = 256.0f;
    }

    fx = fa + (fb - fa) * (Percent - a);

    SeekPoint = (NvS32) ( (1.0f / 256.0f) * fx * FileBytes);

    return SeekPoint;
}


static float
Mp3ParserGetDurationPerVbriFrames (
    NvMp3VbrHeaderGroup *pVbrObj)
{
    NvU32 SamplesPerFrame;
    float TotalDuration ;
    float DurationPerVbriFrames;

    (pVbrObj->FhgVbr.pVBHeader->SampleRate >= 32000) ? (SamplesPerFrame = 1152) : (SamplesPerFrame = 576) ;

    TotalDuration     = ( (float) (pVbrObj->FhgVbr.pVBHeader->VbriStreamFrames) * (float) SamplesPerFrame)
                        / (float) (pVbrObj->FhgVbr.pVBHeader->SampleRate) * 1000.0f ;
    DurationPerVbriFrames = (float) TotalDuration / (float) (pVbrObj->FhgVbr.pVBHeader->VbriTableSize + 1) ;

    return DurationPerVbriFrames;
}

static NvS32
Mp3ParserSeekPointByTime (
    float EntryTimeInMilliSeconds,
    NvMp3VbrHeaderGroup *pVbrObj)
{
    float TotalDuration ;
    float DurationPerVbriFrames ;
    float AccumulatedTime = 0.0f ;
    NvU32 SamplesPerFrame, Fraction = 0, i = 0;
    NvS32 SeekPoint = 0;

    (pVbrObj->FhgVbr.pVBHeader->SampleRate >= 32000) ? (SamplesPerFrame = 1152) : (SamplesPerFrame = 576) ;

    TotalDuration     = ( (float) (pVbrObj->FhgVbr.pVBHeader->VbriStreamFrames) * (float) SamplesPerFrame)
                        / (float) (pVbrObj->FhgVbr.pVBHeader->SampleRate) * 1000.0f ;
    DurationPerVbriFrames = (float) TotalDuration / (float) (pVbrObj->FhgVbr.pVBHeader->VbriTableSize + 1) ;

    if (EntryTimeInMilliSeconds > TotalDuration) EntryTimeInMilliSeconds = TotalDuration;

    while ( (AccumulatedTime <= EntryTimeInMilliSeconds) && (i <= pVbrObj->FhgVbr.pVBHeader->VbriTableSize))
    {
        SeekPoint         += pVbrObj->FhgVbr.pVBHeader->VbriTable[i] ;
        AccumulatedTime += DurationPerVbriFrames;
        i++;

    }

    // Searched too far; correct result
    Fraction = ( (NvS32) ( ( ( (AccumulatedTime - EntryTimeInMilliSeconds) / DurationPerVbriFrames)
                             + (1.0f / (2.0f * (float) pVbrObj->FhgVbr.pVBHeader->VbriEntryFrames))) * (float) pVbrObj->FhgVbr.pVBHeader->VbriEntryFrames));


    SeekPoint -= (NvS32) ( (float) pVbrObj->FhgVbr.pVBHeader->VbriTable[i-1] * (float) (Fraction)
                         / (float) pVbrObj->FhgVbr.pVBHeader->VbriEntryFrames) ;

    return SeekPoint;
}

NvError
NvMp3ParserSeekToTime (
    NvMp3Parser *pNvMp3Parser,
    NvU32 *pMilliSec)
{
    NvError Status = NvSuccess;
    NvS32 SeekPointInBytes;
    NvU32 Offset;
    NvU32 MilliSec = 0;
    float SeekTime;
    float SeekTimePerFract;
    float FractTimeMillSec;
    float FrameDuration;
    float LeftTimeInMilliSec;
    float VbriTablePrecision;
    static NvS32  FrameSkipVbr = 0;
    NvMp3TrackInfo *pTrackInfo = NULL;
    NvMp3VbrHeaderGroup *pVbrObj = NULL;
    NvMp3XingHeader *pXingHdr = NULL;

    NVMM_CHK_ARG(pNvMp3Parser);
    NVMM_CHK_ARG(pMilliSec);

    pTrackInfo = &pNvMp3Parser->StreamInfo.TrackInfo;
    pVbrObj = &pNvMp3Parser->StreamInfo.VBRHeader;
    pXingHdr = &pVbrObj->Mp3InfoXing.XingHeader;
    pXingHdr->pTOCBuf = pVbrObj->Mp3InfoXing.XHDToc;
    MilliSec = *pMilliSec;

    if (pVbrObj->XingResult == NvSuccess)
    {
        if ( (!pNvMp3Parser->IsXingHeaderCorrupted) && (pXingHdr->Frames > 0))
        {
            SeekTime       = (MilliSec / (pXingHdr->Frames * pXingHdr->FrameDuration)) * 100;
            SeekTimePerFract = SeekTime - (int) SeekTime;
            SeekTime      -= SeekTimePerFract;

            SeekPointInBytes = Mp3ParserGetSeekPoint (pXingHdr->pTOCBuf, pXingHdr->Bytes, SeekTime);

            FractTimeMillSec     = (SeekTimePerFract * pXingHdr->Frames * pXingHdr->FrameDuration) / 100;

            if (FractTimeMillSec > pXingHdr->FrameDuration)
            {
                if (FrameSkipVbr > 0)
                    FrameSkipVbr += (OMX_S32) (FractTimeMillSec / pXingHdr->FrameDuration);
                else
                    FrameSkipVbr = (OMX_S32) (FractTimeMillSec / pXingHdr->FrameDuration);
            }
            else
                FrameSkipVbr = 0;

            if (MilliSec == 0)
            {
                if ( (pVbrObj->VbrFirstTocError) || (pVbrObj->VbrTocError))
                {
                    SeekPointInBytes = 0;
                }

            }
            MilliSec = (NvU32)((SeekTime * (pXingHdr->Frames * pXingHdr->FrameDuration))/100);
            // Assigning back the Millisec to the seek point
            *pMilliSec = MilliSec;
            
            NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "NvMp3ParserSeekToTime : Xing Header Not Corrupted\n"));
        }
        else
        {
            SeekPointInBytes = (NvS32) ( (float) MilliSec * ( (float) pTrackInfo->BitRate / 1000.0f) / 8.0f);
            NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "NvMp3ParserSeekToTime : Xing Header Corrupted\n"));
        }
    }
    else if (pVbrObj->FhgResult == NvSuccess)
    {
        switch (pTrackInfo->SamplingFrequency)
        {
            case 48000:
                FrameDuration = 24.0f;
                break;
            case 44100:
                FrameDuration = 26.12f;
                break;
            case 32000:
                FrameDuration = 36.0f;
                break;
            case 24000:
                FrameDuration = 24.0f;
                break;
            case 22050:
                FrameDuration = 26.12f;
                break;
            case 16000:
                FrameDuration = 36.0f;
                break;
            case 12000:
                FrameDuration = 48.0f;
                break;
            case 11025:
                FrameDuration = 52.24f;
                break;
            case  8000:
                FrameDuration = 72.0f;
                break;
            default:
                FrameDuration = 26.12f;
        }

        pVbrObj->FhgVbr.Position = 0;
        pVbrObj->FhgVbr.pVBHeader = (NvMp3VbriHeaderTable *) & pVbrObj->Mp3InfoFhg.FhgVbr;
        VbriTablePrecision    = Mp3ParserGetDurationPerVbriFrames (pVbrObj);
        LeftTimeInMilliSec      = (float) (MilliSec % (OMX_S32) VbriTablePrecision);
        FrameSkipVbr      = (OMX_S32) (LeftTimeInMilliSec / FrameDuration);
        SeekPointInBytes = Mp3ParserSeekPointByTime ( (float) MilliSec, pVbrObj);

    }
    else
    {
        SeekPointInBytes = (NvS32) ( (float) MilliSec * ( (float) pTrackInfo->BitRate / 1000.0f) / 8.0f);
    }

    Offset = pTrackInfo->DataOffset + SeekPointInBytes;

    Status = pNvMp3Parser->pPipe->cpipe.SetPosition (pNvMp3Parser->hContentPipe, Offset, CP_OriginBegin);
    pNvMp3Parser->CurrFileOffset = (NvU64) Offset;
    NV_LOGGER_PRINT ((NVLOG_MP3_PARSER, NVLOG_DEBUG, "Seek FileOffset: %llu at line %u", 
        pNvMp3Parser->CurrFileOffset , __LINE__));

cleanup:
    return Status;
}

