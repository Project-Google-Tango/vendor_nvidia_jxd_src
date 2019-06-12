/*
* Copyright (c) 2006 - 2007 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/
#ifndef INCLUDED_NVMM_PARSER_H
#define INCLUDED_NVMM_PARSER_H

#include "nvcommon.h"
#include "nvmm_parser_types.h"
#include "nvmm_streamtypes.h"
#include "nvmm.h"

#define MAXSTREAMS 10

#define NVPARSER_MBCNT_VGA          1200    // ((640>>4)  * (480>>4))
#define NVPARSER_MBCNT_720P         3600    // ((720>>4) * (1280>>4))
#define NVPARSER_MBCNT_1080P        8160    // ((1920>>4) * (1088>>4))

#if defined(__cplusplus)
extern "C"
{
#endif

typedef char * NvString;

/* Maximum number of URI's a parser can accept for parsing at a time */
#define NVMM_MAX_URI 32
/* To convert float variable to integer and vice versa in Q16 format */
#define FLOAT_MULTIPLE_FACTOR 65536

typedef struct
{
    NvU32 SampleRate; // Audio Sample Rate
    NvU32 BitRate; // Audio BitRate.
    NvU32 NChannels;  // Specifies Total Channels. Mono or stereo
    NvU32 BitsPerSample; // BitsPerSample specifies sample size - 8 or 16bit.,
    // Place Holder To add minimal set of audio propertries required by base parser block.
} NvMMAudio_Prop;

typedef struct
{
    NvU32 Width; // specifies vertical size of pixel aspect ratio
    NvU32 Height; // specifies horizontal size of pixel aspect ratio
    NvU32 AspectRatioX; // specifies pixel aspect ratio for the case of non square pixel content
    NvU32 AspectRatioY; // specifies pixel aspect ratio for the case of non square pixel content
    NvU32 Fps; //frame rate
    NvU32 VideoBitRate; //video bit rtae
    // Place Holder To add minimal set of video propertries required by base parser block.
} NvMMVideo_Prop;

typedef struct
{
    NvU32 Width; //Specifies the width of the source image in pixels.
    NvU32 Height; //Specifies the height of the source image in pixels.
    NvU32 ColorFormat; // Specifies the Color format of the source image(420/422/422T/444 /400)
    // Place Holder To add minimal set of image propertries required by base parser block.
} NvMMImage_Prop;

/** NvMMStreamInfo structure */
// All parser blocks time units should be in 100nsec.
typedef struct
{
    NvMMStreamType StreamType; //streamType Object specifies the type of compression.
    NvU64 TotalTime; // Total track time in 100nsec
    NvMMBlockType BlockType;
    // Union with Stream Properties Audio/Video/Image.
    union
    {
        NvMMAudio_Prop AudioProps; //Audio Properties
        NvMMVideo_Prop VideoProps; //Video Properties
        NvMMImage_Prop ImageProps;//Image Properties
    } NvMMStream_Props;
} NvMMStreamInfo;

/** NvMMParserCoreProperties structure */
typedef struct NvMMParserCorePropertiesRec
{
    NvU32 numberOfStreams;
    NvMMStreamInfo streamInfo[MAXSTREAMS];
} NvMMParserCoreProperties;

/**
 * @brief Parser Attribute enumerations.
 * Following enum are used by parsers for setting/getting the attributes.
 * They are used as 'eAttributeType' variable in SetAttribute() and GetAttribute() APIs.
 * @see SetAttribute
 */
typedef enum
{
    /** AttributeType for Base paser Block
     *     To get NvMMAttrib_NoOfStreams in the currently parsed file for base parser block.
     */
    NvMMAttribute_NoOfStreams = (NvMMAttributeParser_Unknown + 1),

    /** AttributeType for Base paser Block
     *     To get NvMMAttribute_StreamInfo in the currently parsed file for base parser block.
     */
    NvMMAttribute_StreamInfo,

    /** AttributeType for Base paser Block
     *     To set NvMMAttrib_Position postion for base parser block.
     */
    NvMMAttribute_Position,

    /** AttributeType for Base paser Block
     *     To set NvMMAttrib_Position Rate for base parser block.
     */
    NvMMAttribute_Rate,

    /** AttributeType for Base paser Block
     *     To set NvMMAttribute_FileName for base parser block.
     */
    NvMMAttribute_FileName,

    NvMMAttribute_ReduceVideoBuffers,

    NvMMAttribute_FileCacheSize,

    NvMMAttribute_FileCacheInfo,

    NvMMAttribute_FileCacheThreshold,

    NvMMAttribute_Buffering,

    /** AttributeType for super paser Block
     *     To set NvMMAttribute_UserAgent for super parser block.
     */
    NvMMAttribute_UserAgent,
    /** AttributeType for super paser Block
     *     To enable/disable dynamic buff config
     */
    NvMMAttribute_DisableBuffConfig,

    /** AttributeType for super paser Block
     *   To set NvMMAttribute_UAProf for super parser block.
     */
    NvMMAttribute_UserAgentProf,
} NvMMParserAttribute;

typedef struct
{
    NvU64   nDataBegin; //Read: The beginning of the available content, specifically the position of the first (still) available byte of the content's data stream. Typically zero for file reading.
                        //Write: The beginning of content, specifically the position of the first writable byte of the content's data stream. Typically zero

    NvU64   nDataFirst; //Read: The beginning of the available content, specifically the position of the first (still) available byte of the content's data stream. Typically zero for file reading.
                        //Write: Not applicable

    NvU64   nDataCur;   //Read/Write: Current position

    NvU64   nDataLast;  //Read: The end of the available content, specifically the position of the last available byte of the content's data stream. Typically equaling nDataEnd for file reading.
                        //Write: Not applicable

    NvU64   nDataEnd;   //Read: The end of content, specifically the position of the last byte of the content's data stream. This may return CPE_POSITION_NA if the position is unknown. Typically equaling the file size for file reading.
                        //Write: Not applicable
} NvMMAttrib_FileCacheInfo;

typedef struct
{
    NvU32 nFileCacheSize;
} NvMMAttrib_FileCacheSize;

typedef struct
{
    NvBool bEnableBuffering; // Attribute used to enable nvmm buffering for streaming
} NvMMAttrib_Buffering;

typedef struct
{
    NvU64 nHighMark;
    NvU64 nLowMark;
} NvMMAttrib_FileCacheThreshold;

typedef struct
{
    char *pUserAgentStr;
    NvU32 len;
} NvMMAttrib_UserAgentStr;

typedef struct
{
    NvU32 bReduceVideoBufs;
} NvMMAttrib_ReduceVideoBuffers;

/** NvMMAttribute for parser to set/get postion.*/
typedef struct
{
    NvU64 Position; //Byte position relative to stream start
} NvMMAttrib_ParsePosition;

/** NvMMAttribute for parser to enable/disable dynamic buff config.*/
typedef struct
{
    NvBool bDisableBuffConfig;
} NvMMAttribute_DisableBuffConfigFlag;

typedef struct
{
    NvBool bEnableMp3TSflag;
} NvMMAttribute_Mp3EnableConfigFlag;

/** NvMMAttribute for parser to set/get rate.
 *  rate/1000 = rate, e.g. 0= PAUSE, 1000 = 1X, 2000 = 2X FF, -2000 = 2X // REW
 */
typedef struct
{
    NvS32 Rate; //rate/1000 = rate, e.g. 0= PAUSE, 1000 = 1X, 2000 = 2X FF, -2000 = 2X // REW
} NvMMAttrib_ParseRate;

/** NvMMAttribute for parser to access Uri. URI path for the parser to get content specific info */
typedef struct
{
    NvString szURI; //pFileName
    NvMMParserCoreType eParserCore;
} NvMMAttrib_ParseUri;

typedef struct
{
    char *pUAProfStr;
    NvU32 len;
} NvMMAttribute_UserAgentProfStr;

#if defined(__cplusplus)
}
#endif

#endif

