/* Copyright (c) 2009-2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/** 
 * @file
 * <b>NVIDIA Tegra: OpenMAX Container Extension Interface</b>
 *
 */

#ifndef NVOMX_ParserExtensions_h_
#define NVOMX_ParserExtensions_h_

/**
 * @defgroup nv_omx_il_parser_reader Container Demux
 *   
 * This is the NVIDIA OpenMAX container demux class extension interface.
 *
 * These extensions include demuxer control and query of meta-data, stream type,
 * cache size, duration, file name and more.
 * 
 * See also \link nv_omx_il_tracklist Container Tracklist Extensions\endlink
 *
 * @ingroup nvomx_container_extension
 * @{
 */

/* MUST ADD NEW TYPES TO END OF ENUM */
typedef enum
{
    NvxStreamType_NONE = 0,
    NvxStreamType_MPEG4,
    NvxStreamType_H264,
    NvxStreamType_H263,
    NvxStreamType_WMV,
    NvxStreamType_JPG,
    NvxStreamType_MP3,
    NvxStreamType_WAV,
    NvxStreamType_AAC,
    NvxStreamType_AACSBR,
    NvxStreamType_BSAC,
    NvxStreamType_WMA,
    NvxStreamType_WMAPro,
    NvxStreamType_WMALossless,
    NvxStreamType_AMRWB,
    NvxStreamType_AMRNB,
    NvxStreamType_VORBIS,
    NvxStreamType_MPEG2V,
    NvxStreamType_AC3,
    NvxStreamType_WMV7,
    NvxStreamType_WMV8,
    NvxStreamType_WMAVoice,
    NvxStreamType_MP2,
    NvxStreamType_ADPCM,
    NvxStreamType_MS_MPG4,
    NvxStreamType_QCELP,
    NvxStreamType_EVRC,
    NvxStreamType_H264Ext,
    NvxStreamType_VP6,
    NvxStreamType_MPEG4Ext,
    NvxStreamType_MJPEG,
    NvxStreamType_Theora,
    NvxStreamType_H264_Secure,
    NvxStreamType_WMV_Secure,
    NvxStreamType_MAX = 0x7FFFFFFF
} ENvxStreamType;

typedef enum
{
    NvxMetadata_Artist,
    NvxMetadata_Album,
    NvxMetadata_Genre,
    NvxMetadata_Title,
    NvxMetadata_Year,
    NvxMetadata_TrackNum,
    NvMMetadata_Encoded,
    NvxMetadata_Comment,
    NvxMetadata_Composer,
    NvxMetadata_Publisher,
    NvxMetadata_OriginalArtist,
    NvxMetadata_AlbumArtist,
    NvxMetadata_Copyright,
    NvxMetadata_Url,
    NvxMetadata_BPM,
    NvxMetadata_CoverArt,
    NvxMetadata_CoverArtURL,
    NvxMetadata_ThumbnailSeektime,
    NvxMetadata_RtcpAppData,
    NvxMetadata_RTCPSdesCname,
    NvxMetadata_RTCPSdesName,
    NvxMetadata_RTCPSdesEmail,
    NvxMetadata_RTCPSdesPhone,
    NvxMetadata_RTCPSdesLoc,
    NvxMetadata_RTCPSdesTool,
    NvxMetadata_RTCPSdesNote,
    NvxMetadata_RTCPSdesPriv,
    NvxMetadata_SeekNotPossible,
    NvxMetadata_MAX = 0x7FFFFFFF
} ENvxMetadataType;

/** Temporary file path parameter for writer components that write files.
 *  See ::NVX_PARAM_FILENAME for details.
 *  @note This is implemented only for 3GP writer.
 */
#define NVX_INDEX_PARAM_TEMPFILEPATH "OMX.Nvidia.index.param.tempfilename"
/** Holds filename for components that read/write files. */
typedef struct NVX_PARAM_TEMPFILEPATH {
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_STRING pTempPath;       /**< Temporary file path as supported by stdio implementation */
} NVX_PARAM_TEMPFILEPATH;


/** Filename parameter for source, demux, and sink components that read/write files.
 *  See ::NVX_PARAM_FILENAME for details.
 */
#define NVX_INDEX_PARAM_FILENAME "OMX.Nvidia.index.param.filename"
/** Holds filename for components that read/write files. */
typedef struct NVX_PARAM_FILENAME {
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_STRING pFilename;       /**< Name of file as supported by stdio implementation */
} NVX_PARAM_FILENAME;

/** Duration parameter for source, demux and sink components that read/write files.
 *  See ::NVX_PARAM_DURATION for details.
 */
#define NVX_INDEX_PARAM_DURATION "OMX.Nvidia.index.param.duration"  
/** Holds duration for components that read/write files. */
typedef struct NVX_PARAM_DURATION {
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_TICKS nDuration;        /**< Duration in microsecs */
} NVX_PARAM_DURATION;

/** Stream type parameter for source, demux and sink components that read/write files.
 *  See ::NVX_PARAM_STREAMTYPE for details.
 */
#define NVX_INDEX_PARAM_STREAMTYPE "OMX.Nvidia.index.param.streamtype"
/** Holds stream type for components that read/write files. */
typedef struct NVX_PARAM_STREAMTYPE {
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_U32 nPort;              /**< Port that this struct applies to */
    ENvxStreamType eStreamType; /**< Stream type */
} NVX_PARAM_STREAMTYPE;
/** Stream Count parameter for finding the no of streams in the file for reader.
 *  See ::NVX_PARAM_STREAMCOUNTfor details.
 */
#define NVX_INDEX_PARAM_STREAMCOUNT "OMX.Nvidia.index.param.streamcount"
/** Holds stream type for components that read/write files. */
typedef struct NVX_PARAM_STREAMCOUNT{
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_U32  StreamCount;               /**< no of streams count  */
} NVX_PARAM_STREAMCOUNT;

/** Audio stream parameters for source, demux and sink components that read/write files.
 *  See ::NVX_PARAM_AUDIOPARAMS for details.
 */
#define NVX_INDEX_PARAM_AUDIOPARAMS "OMX.Nvidia.index.param.audioparams"
/** Holds audio stream info for components that read/write files. */
typedef struct NVX_PARAM_AUDIOPARAMS {
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_U32 nPort;              /**< Port that this struct applies to */
    OMX_U32 nSampleRate;        /**< Sample rate in HZ */
    OMX_U32 nBitRate;           /**< Bits rate in bits per second */
    OMX_U32 nChannels;          /**< Number of channels (mono = 1, stereo = 2, etc) */
    OMX_U32 nBitsPerSample;     /**< Bits per sample */
} NVX_PARAM_AUDIOPARAMS;

/** Metadata config for demux components that read files with meta data.
 * If specified metadata not found, returns empty string or 
 * OMX_ErrorInsufficientResources if sValueStr is too small.
 *
 * See ::NVX_CONFIG_QUERYMETADATA
 */
#define NVX_INDEX_CONFIG_QUERYMETADATA "OMX.Nvidia.index.config.querymetadata"
/** Holds meta data info for demux components. */
typedef struct NVX_CONFIG_QUERYMETADATA {
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    ENvxMetadataType eType;     /**< Meta data type */
    OMX_STRING sValueStr;       /**< String to hold results */
    OMX_U32 nValueLen;          /**< Length of results string */
    OMX_METADATACHARSETTYPE eCharSet;   /**< Character set to use */
} NVX_CONFIG_QUERYMETADATA;
/** Video Header parameter for getting  the video header from parser.
 *  See ::NVX_PARAM_VIDEOHEADER for details.
 */
#define NVX_INDEX_CONFIG_HEADER "OMX.Nvidia.index.param.header"
/** Holds stream type for components that read/write files. */
typedef struct NVX_CONFIG_HEADER{
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;            /**< Port number the structure applies to */
    OMX_STRING  nBuffer;          /**< the buffer which holds header info  */
    OMX_U32 nBufferlen;            /**>Buffer length */
} NVX_CONFIG_HEADER;

/** Audio Header parameter for getting  the audio header from parser.
 *  See ::NVX_CONFIG_AUDIOHEADER for details.
 */
#define NVX_INDEX_CONFIG_AUDIOHEADER "OMX.Nvidia.index.param.audioheader"
/** Holds stream type for components that read/write files. */
typedef struct NVX_CONFIG_AUDIOHEADER{
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_STRING  nBuffer;          /**< the buffer which holds header info  */
    OMX_U32 nBufferlen;            /**>Buffer length */
} NVX_CONFIG_AUDIOHEADER;
/** WMA Header parameter for getting  the audio header from parser.
 *  See ::NVX_CONFIG_WMAHEADER for details.
 */
#define NVX_INDEX_CONFIG_WMAHEADER "OMX.Nvidia.index.param.wmaheader"
/** Holds stream type for components that read/write files. */
typedef struct NVX_CONFIG_WMAHEADER{
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_STRING  nBuffer;          /**< the buffer which holds header info  */
    OMX_U32 nBufferlen;            /**>Buffer length */
} NVX_CONFIG_WMAHEADER;
#define  NVX_INDEX_CONFIG_MP3TSENABLE  "OMX.Nvidia.index.config.mp3tsenable"
/** Holds a MP3TSEnable  handle. */
typedef struct NVX_CONFIG_MP3TSENABLE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 bMp3Enable;             /**< Enable/Disable flag*/
} NVX_CONFIG_MP3TSENABLE;

/** File cache size config for demux components that read files.
 * See ::NVX_CONFIG_FILECACHESIZE
 */
#define NVX_INDEX_CONFIG_FILECACHESIZE "OMX.Nvidia.index.config.filecachesize"
/** Holds cache size configuration. */
typedef struct NVX_CONFIG_FILECACHESIZE {
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_U32 nFileCacheSize;     /**< in MB, 0 = none, 1 = max */
} NVX_CONFIG_FILECACHESIZE;
/** File cache size config for demux components that read files.
 * See ::NVX_CONFIG_FILECACHESIZE
 */
#define NVX_INDEX_CONFIG_DISABLEBUFFCONFIG "OMX.Nvidia.index.config.disablebuffconfig"
/** Holds cache size configuration. */
typedef struct NVX_CONFIG_DISABLEBUFFCONFIG {
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_BOOL bDisableBuffConfig;     /**< , 0 = enable, 1 = disable */
} NVX_CONFIG_DISABLEBUFFCONFIG;

/** File cache info config for demux components that read files.
 * See ::NVX_CONFIG_FILECACHEINFO
 */
#define NVX_INDEX_CONFIG_FILECACHEINFO "OMX.Nvidia.index.config.filecacheinfo"
/** Holds file cache information. */
typedef struct NVX_CONFIG_FILECACHEINFO {
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_U64 nDataBegin;         /**< Data Begin */
    OMX_U64 nDataFirst;         /**< Data First */
    OMX_U64 nDataCur;           /**< Data Current */
    OMX_U64 nDataLast;          /**< Data Last */
    OMX_U64 nDataEnd;           /**< Data End */
} NVX_CONFIG_FILECACHEINFO;

/** User agent parameters required for streaming playback.
 */
#define NVX_INDEX_PARAM_USERAGENT "OMX.Nvidia.index.param.useragent"
/** Holds user agent info for streaming. */
typedef struct NVX_PARAM_USERAGENT {
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_STRING pUserAgentStr;   /**< User agent info */
} NVX_PARAM_USERAGENT;

/** User agent profile parameters required for streaming playback.
 */
#define NVX_INDEX_PARAM_USERAGENT_PROFILE "OMX.Nvidia.index.param.useragentprofile"
/** Holds user agent profile info for streaming. */
typedef struct NVX_PARAM_USERAGENTPROF {
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_STRING pUserAgentProfStr;   /**< User agent profile info */
} NVX_PARAM_USERAGENTPROF;

/* Charset define to extend OMX_METADATACHARSETTYPE for a U32 type */
#define NVOMX_MetadataCharsetU32    0x10000000
#define NVOMX_MetadataFormatJPEG    0x10000001
#define NVOMX_MetadataFormatPNG     0x10000002
#define NVOMX_MetadataFormatBMP     0x10000003
#define NVOMX_MetadataFormatUnknown 0x10000004
#define NVOMX_MetadataFormatGIF     0x10000005
#define NVOMX_MetadataCharsetU64    0x10000006

/** @} */

/**
 * @defgroup nv_omx_il_parser_dtv DTV Source
 *   
 * This is the NVIDIA OpenMAX DTV source class extension interface. 
 *
 * These extensions include recording and pause mode, source selection, channel
 * info and control and more.
 *
 * @ingroup nvomx_container_extension
 * @{
 */

typedef enum
{
    NvxRecordingMode_Enable = 1,
    NvxRecordingMode_EnableExclusive,
    NvxRecordingMode_Disable,
    NvxRecordingMode_Force32 = 0x7FFFFFFF
} ENvxRecordingMode;

typedef enum
{
    NvxSource_AutoDetect = 1,
    NvxSource_File,
    NvxSource_Force32 = 0x7FFFFFFF
} ENvxSource;

/** Recording mode parameter for DTV components.
 * See ::NVX_PARAM_RECORDINGMODE
 */
#define NVX_INDEX_PARAM_RECORDINGMODE "OMX.Nvidia.index.param.recordingmode"
/** Holds DTV recording mode. */
typedef struct NVX_PARAM_RECORDINGMODE
{
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    ENvxRecordingMode RecordingMode;    /**< Recording mode */
} NVX_PARAM_RECORDINGMODE;

/** Source parameter for DTV components.
 * See ::NVX_PARAM_SOURCE
 */
#define NVX_INDEX_PARAM_SOURCE "OMX.Nvidia.index.param.source"
/** Holds DTV source type. */
typedef struct NVX_PARAM_SOURCE
{
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    ENvxSource Source;          /**< Source type */
} NVX_PARAM_SOURCE;

/** Channel ID parameter for DTV components.
 * See ::NVX_PARAM_CHANNELID
 */
#define NVX_INDEX_PARAM_CHANNELID "OMX.Nvidia.index.param.channelid"
/** Holds DTV channel ID. */
typedef struct NVX_PARAM_CHANNELID
{
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_U32 ChannelID;          /**< Channel ID */
} NVX_PARAM_CHANNELID;


/** Pause config for DTV components.
 * See ::NVX_CONFIG_PAUSEPLAYBACK
 */
#define NVX_INDEX_CONFIG_PAUSEPLAYBACK "OMX.Nvidia.index.config.pauseplayback"
/** Holds data to enable DTV pause. */
typedef struct NVX_CONFIG_PAUSEPLAYBACK
{
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_BOOL bPausePlayback;    /**< Pause if TRUE */
} NVX_CONFIG_PAUSEPLAYBACK;

/** Holds DTV program ID. */
typedef struct ENvxDTVProgRec
{
    OMX_U32 ProgID;
} ENvxDTVProg;

/** Holds a DTV program list. */
typedef struct ENvxDTVProgProgsListRec
{
    OMX_U32 NumProgs;
    ENvxDTVProg *pProgs;
} ENvxDTVProgProgsList;

/** Program list config for DTV components.
 * See ::NVX_CONFIG_PROGRAMS_LIST
 */
#define NVX_INDEX_CONFIG_PROGRAMS_LIST "OMX.Nvidia.index.config.programslist"
/** Holds a DTV program list. */
typedef struct NVX_CONFIG_PROGRAMS_LIST
{
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    ENvxDTVProgProgsList ProgsList; /**< Program list */
} NVX_CONFIG_PROGRAMS_LIST;

/** Current program config for DTV components.
 * See ::NVX_CONFIG_CURRENT_PROGRAM
 */
#define NVX_INDEX_CONFIG_CURRENT_PROGRAM "OMX.Nvidia.index.config.currentprogram"
/** Holds a DTV current program ID. */
typedef struct NVX_CONFIG_CURRENT_PROGRAM
{
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_U32 ProgID;             /**< Current program */
} NVX_CONFIG_CURRENT_PROGRAM;

/** @} */

/**
 * @defgroup nv_omx_il_parser_writer Container Mux
 *   
 * This is the NVIDIA OpenMAX container mux class extension interface. 
 *
 * These extensions include muxer control and query of stream type, cache size, 
 * duration, file name and more.
 *
 * @ingroup nvomx_container_extension
 * @{
 */

/** Maximum number of frames to write to file parameter.
 * See ::OMX_PARAM_U32TYPE
 */
#define NVX_INDEX_PARAM_MAXFRAMES "OMX.Nvidia.index.param.maxframes"

/** Muxer buffer config parameter.
 * @deprecated This parameter is deprecated.
 */
#define NVX_INDEX_PARAM_OTHER_3GPMUX_BUFFERCONFIG  "OMX.Nvidia.index.param.other.3gpmux.bufferconfig"
/** @deprecated This structure is deprecated. */
typedef struct NVX_PARAM_OTHER_3GPMUX_BUFFERCONFIG
{
    OMX_U32 nSize;              /**< Size of the structure in bytes. */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information. */
    OMX_U32 nPortIndex;         /**< Port to which this struct applies. */
    OMX_BOOL bUseCache;         /**< @deprecated This field is deprecated. */
    OMX_U32 nBufferSize;        /**< @deprecated This field is deprecated. */
    OMX_U32 nPageSize;          /**< @deprecated This field is deprecated. */
} NVX_PARAM_OTHER_3GPMUX_BUFFERCONFIG;

/** Current number of audio and video frames config.
 * See ::NVX_CONFIG_3GPMUXGETAVRECFRAMES
 */
#define NVX_INDEX_CONFIG_3GPMUXGETAVRECFRAMES "OMX.Nvidia.index.config.3gpmuxgetavrecframes"
/** Holds a number of audio and video frames. */
typedef struct NVX_CONFIG_3GPMUXGETAVRECFRAMES
{
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;         /**< Port that this struct applies to */
    OMX_U32 nAudioFrames;       /**< Number of audio frames muxed */
    OMX_U32 nVideoFrames;       /**< Number of video frames muxed */
}NVX_CONFIG_3GPMUXGETAVRECFRAMES;

/** Split filename config.
 * See ::NVX_CONFIG_SPLITFILENAME
 */
#define NVX_INDEX_CONFIG_SPLITFILENAME "OMX.Nvidia.index.config.splitfilename"
/** Holds split filenames. */
typedef struct NVX_CONFIG_SPLITFILENAME
{
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_STRING pFilename;       /**< Name of file as supported by stdio implementation */
}NVX_CONFIG_SPLITFILENAME;

/** Output format parameter for writer.
 *  See ::NVX_PARAM_OUTPUTFORMAT for details.
 */
#define NVX_INDEX_PARAM_OUTPUTFORMAT "OMX.Nvidia.index.param.outputformat"
#define MAX_EXT_LEN 8
/** Holds filename for components that read/write files. */
typedef struct NVX_PARAM_OUTPUTFORMAT {
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_STRING OutputFormat;
} NVX_PARAM_OUTPUTFORMAT;

/** File size parameters for writer components that write files.
 * See ::NVX_PARAM_WRITERFILESIZE
 */
#define NVX_INDEX_PARAM_WRITERFILESIZE "OMX.Nvidia.index.param.writerfilesize"
/** Holds file size configuration. */
typedef struct NVX_PARAM_WRITERFILESIZE {
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_U64 nFileSize;          /**< Maximum file size to write */
} NVX_PARAM_WRITERFILESIZE;


/* FIXME: Remove when parser code is reworked.  Just added to fix a build
 * break.
 */

/** Video Header parameter for getting  the video header from parser.
 *  See ::NVX_PARAM_VIDEOHEADER for details.
 */
#define NVX_INDEX_CONFIG_VIDEOHEADER "OMX.Nvidia.index.param.videoheader"
/** Holds stream type for components that read/write files. */
typedef struct NVX_CONFIG_VIDEOHEADER{
    OMX_U32 nSize;              /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< NVX extensions specification version information */
    OMX_STRING  nBuffer;          /**< the buffer which holds header info  */
    OMX_U32 nBufferlen;            /**>Buffer length */
} NVX_CONFIG_VIDEOHEADER;

/** @} */


#endif
/* File EOF */
