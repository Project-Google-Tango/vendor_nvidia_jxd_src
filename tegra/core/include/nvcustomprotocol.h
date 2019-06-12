/* 
 * Copyright (c) 2008 - 2010 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property 
* and proprietary rights in and to this software, related documentation 
* and any modifications thereto.  Any use, reproduction, disclosure or 
* distribution of this software and related documentation without an 
* express license agreement from NVIDIA Corporation is strictly prohibited.
*/

/** 
 * @file
 * <b>NVIDIA Tegra: Custom Protocol Registration Plug-in Interface</b>
 *
 */

/**
 * @addtogroup nv_omx_il_protocol Custom Protocol Plug-in Interface
 *
 * @ingroup nvomx_plugins
 * @{
 */

#ifndef NVCUSTOMPROTOCOL_H_
#define NVCUSTOMPROTOCOL_H_

#include "nvcommon.h"
#include "nvos.h"

#if defined(__cplusplus)
extern "C"
{
#endif


/** Defines various supported parser cores. */

    /*
    ************* Do *NOT* add new types to the middle of this enum.          
    ** WARNING ** Only add to the end, before the VALIDTOP entry.
    ************* Conversely, do *not* delete types from the middle.
    */
    typedef enum NvParserCoreTypeRec
    {
        NvParserCoreType_UnKnown = 0x00, /* Unknown parser */

    NvParserCoreType_VALIDBASE = 0x001, 
    NvParserCoreType_AsfParser,    /**< ASF parser */
    NvParserCoreType_3gppParser,   /**< @deprecated in favor of MP4 */
    NvParserCoreType_AviParser,    /**< AVI parser */
    NvParserCoreType_Mp3Parser,    /**< MP3 parser */
    NvParserCoreType_Mp4Parser,    /**< MP4 parser */
    NvParserCoreType_OggParser,    /**< OGG parser */
    NvParserCoreType_AacParser,    /**< AAC parser */
    NvParserCoreType_AmrParser,    /**< AMR parser */
    NvParserCoreType_RtspParser,   /**< @deprecated */
    NvParserCoreType_M2vParser,    /**< M2V parser */
    NvParserCoreType_MtsParser,    /**< MTS parser */
    NvParserCoreType_NemParser,    /**< NV generic simple format parser */
    NvParserCoreType_WavParser,    /**< WAV parser */
    NvParserCoreType_FlvParser,    /**< FLV parser */
    NvParserCoreType_Dummy1,       /**< dummy parser */
    NvParserCoreType_Dummy2,       /**< dummy parser */
    NvParserCoreType_MkvParser,    /**< MKV parser */    
    NvParserCoreType_VALIDTOP, 
    NvParserCoreTypeForce32 = 0x7FFFFFFF
} NvParserCoreType;

    /** Defines NVIDIA stream types.
    * @warning Must add new types to the end of the appropriate section.
    */
    typedef enum NvStreamTypeRec
    {
        NvStreamType_OTHER = 0x0,   // Other, not supported tracks

        /* Audio codecs */
        NvStreamType_AUDIOBASE = 0x001, 
        NvStreamType_MP3,       /**< MP3 track */
        NvStreamType_WAV,       /**< Wave track */
        NvStreamType_AAC,       /**< MPEG-4 AAC */
        NvStreamType_AACSBR,    /**< MPEG-4 AAC SBR  */
        NvStreamType_BSAC,      /**< MPEG-4 ER BSAC track */
        NvStreamType_QCELP,     /**< 13K (QCELP) speech track */
        NvStreamType_WMA,       /**< WMA track */
        NvStreamType_WMAPro,    /**< WMAPro Track */
        NvStreamType_WMALSL,    /**< WMA Lossless */
        NvStreamType_WMAV,      /**< WMA Voice Stream */
        NvStreamType_WAMR,      /**< Wide band AMR track */
        NvStreamType_NAMR,      /**< Narrow band AMR track  */
        NvStreamType_OGG,       /**< OGG Vorbis */
        NvStreamType_A_LAW,     /**< G711  ALaw */
        NvStreamType_U_LAW,     /**< G711  ULaw */
        NvStreamType_AC3,       /**< AC3 Track */
        NvStreamType_MP2,       /**< MP3 track */
        NvStreamType_EVRC,      /**< Enhanced Variable Rate Coder */
        NvStreamType_ADPCM,     /**< ADPCM track */
        NvStreamType_UnsupportedAudio, /**< Unsupported Audio Track */

        NvStreamType_VIDEOBASE = 0x100,   
        NvStreamType_MPEG4,     /**< MPEG-4 track */
        NvStreamType_H264,      /**< H.264 track */
        NvStreamType_H263,      /**< H263 track */
        NvStreamType_WMV,       /**< WMV 9 track  */
        NvStreamType_WMV7,      /**< WMV 7 track */
        NvStreamType_WMV8,      /**< WMV 8 track */
        NvStreamType_JPG,       /**< JPEG file */
        NvStreamType_BMP,       /**< BMP type  */
        NvStreamType_TIFF,      /**< TIFF type */
        NvStreamType_MJPEGA,    /**< M-JPEG Format A  */
        NvStreamType_MJPEGB,    /**< M-JPEG Format B */
        NvStreamType_MJPEG,     /**< M-JPEG Format */
        NvStreamType_MPEG2V,    /**< MPEG2 track */
        NvStreamType_UnsupportedVideo, /**< Unsupported Video */

        NvStreamType_Force32 = 0x7FFFFFFF
    } NvStreamType;

#define NV_ISSTREAMAUDIO(x) ((x) > NvStreamType_AUDIOBASE && (x) < NvStreamType_VIDEOBASE)
#define NV_ISSTREAMVIDEO(x) ((x) > NvStreamType_VIDEOBASE)

/** 
 * Defines origin types used in the Seek function.
 * Seeking is time in 100ns units.
 */
typedef enum NvCPR_OriginTypeRec {
    NvCPR_OriginBegin,      
    NvCPR_OriginCur,      
    NvCPR_OriginEnd,      
    NvCPR_OriginTime,
    NvCPR_OriginMax = 0X7FFFFFFF
} NvCPR_OriginType;

/** 
 * Defines contact access types used in the Open function. 
 */
typedef enum NvCPR_AccessTypeRec {
    NvCPR_AccessRead,      
    NvCPR_AccessWrite,  
    NvCPR_AccessReadWrite,  
    NvCPR_AccessMax = 0X7FFFFFFF
} NvCPR_AccessType;
    /** 
    * Defines possible configs to query.
    */
    typedef enum NvCPR_ConfigQueryRec {
        /// Amount in bytes to prebuffer before
        /// allowing playback to begin.
        /// Uses NvU32 data.
        NvCPR_ConfigPreBufferAmount,
        /// Boolean indicating if time-based.
        /// seeking is supported.
        /// Uses NvU32 data.
        NvCPR_ConfigCanSeekByTime,
        /// To get the meta interval for ShoutCast ICY.
        NvCPR_ConfigGetMetaInterval,
        /// To get size of the smallest chunk used by the client to read data.
        NvCPR_ConfigGetChunkSize,
        NvCPR_GetActualSeekTime,
        /// To get SDES app data(text) the client to read data, it is null terminated.
        NvCPR_GetRTCPAppData,
        /// To get SDES cname data(text) the client to read data, it is null terminated.
        NvCPR_GetRTCPSdesCname,
        /// To get SDES name data(text) the client to read data, it is null terminated.
        NvCPR_GetRTCPSdesName,
        /// To get SDES email data(text) the client to read data, it is null terminated.
        NvCPR_GetRTCPSdesEmail,
        /// To get SDES phone data(text) the client to read data, it is null terminated.
        NvCPR_GetRTCPSdesPhone,
        /// To get SDES loc data(text) the client to read data, it is null terminated.
        NvCPR_GetRTCPSdesLoc,
        /// To get SDES tool data(text) the client to read data, it is null terminated.
        NvCPR_GetRTCPSdesTool,
        /// To get SDES note data(text) the client to read data, it is null terminated.
        NvCPR_GetRTCPSdesNote,
        /// To get SDES private data(text) the client to read data, it is null terminated.
        NvCPR_GetRTCPSdesPriv,
        /// To get latest stream timestamps received from the server.
        NvCPR_GetTimeStamps,
        NvCPR_ConfigMax = 0X7FFFFFFF
    } NvCPR_ConfigQueryType;

    typedef void *NvCPRHandle;

typedef struct CP_QueryConfigTS
{
    NvU64 audioTs;
    NvU64 videoTs;
    NvBool bAudio;
    NvBool bVideo;
}CP_QueryConfigTS;


#define NV_CUSTOM_PROTOCOL_VERSION 2

/** Holds definition of NVIDIA custom protocol plug-in interface. */
typedef struct NV_CUSTOM_PROTOCOL
{
    /** Returns the version of NV_CUSTOM_PROTOCOL this implementation 
     * supports.
     *
     * @param [out] pnVersion The version of the ::NVMM_CUSTOM_PROTOCOL interface.
     */
    NvError (*GetVersion)(NvS32 *pnVersion);
    /** Opens a URI, returns handle in hHandle.
     *
     * @param [in] hHandle The returned handle to the opened ::NVCustomPRHandle object.
     * @param [in] szURI   The URI to open.
     * @param [in] eAccess The mode to open the URI in.
     */
    NvError (*Open)(NvCPRHandle* hHandle, char *szURI, 
                    NvCPR_AccessType eAccess);
    /** Closes a content stream. 
     *
     * @param [in] hContent The handle to close.
     */ 
    NvError (*Close)(NvCPRHandle hContent);
    /** Seeks to certain position in the content relative to the specified
     *  origin. This may fail for certain network streaming situations.
     *
     * @param [in] hContent The handle to operate on.
     * @param [in] nOffset  The offset to seek to.
     * @param [in] eOrigin  From what origin point to perform the seek operation.
     */
    NvError (*SetPosition)(NvCPRHandle hContent, NvS64 nOffset, 
                           NvCPR_OriginType eOrigin);
    /** Gets the current position relative to the start of the content. 
     *
     * @param [in] hContent  The handle to operate on.
     * @param [out] pPosition Returns the current position.
     */
    NvError (*GetPosition)(NvCPRHandle hContent, NvU64 *pPosition);
    /** Gets data of the specified size from the content stream.
     *
     * @param [in] hContent The handle to operate on.
     * @param [out] pData    A pointer to a buffer to read into.
     * @param [in] nSize    Number of bytes to read.
     * 
     * Returns number of bytes read.
     */
    NvU32 (*Read)(NvCPRHandle hContent, NvU8 *pData, NvU32 nSize);
    /** Writes data to the stream.
     * Included for completeness, may be stubbed in most implementations.
     *
     * @param [in] hContent The handle to operate on.
     * @param [in] pData    A pointer to a buffer of data to write.
     * @param [in] nSize    Size of the \a pData buffer to write.
     *
     * @return The number of bytes written.
     */
    NvU32 (*Write)(NvCPRHandle hContent, NvU8 *pData, NvU32 nSize);
    /** Gets the file size. 
     *
     * @param [in] hContent  The handle to operate on.
     * @param [out] pFileSize The total size of the file, or -1 if unknown.
     */
    NvError (*GetSize)(NvCPRHandle hContent, NvU64 *pFileSize);
    /** Returns true if the file is streaming over a network.
     * When this is set to true, more buffering is performed, seeking happens
     * considerably less often (or not at all), and various timeout values 
     * are increased.
     *
     * @param [in] hContent  The handle to operate on.
     */
    NvBool (*IsStreaming)(NvCPRHandle hContent);
    /** Returns the parser type needed by the passed in URI, or 
     * ::NvParserCoreType_UnKnown to indicate matching by file extension.
     *
     * @param [in] szURI   The URI to open
     *
     * @return The parser type; use ::NvMMParserCoreType_UnKnown to match by 
     * extension instead.
     */
    NvParserCoreType (*ProbeParserType)(char *szURI);
    /** Queries a config type specified by the eQuery field.
     * 
     * @param [in] hContent  The handle to operate on, may be NULL.
     * @param [in] eQuery    The type of the query.
     * @param [out] pData     The data type associated with the query, specified in
     *                  the documentation for ::NvCPR_ConfigQueryType.
     * @param [in] nDataSize The size of the \a pData value.
     *
     * @retval NvError_NotSupported If the query is unknown.
     */
    NvError (*QueryConfig)(NvCPRHandle hContent, 
                           NvCPR_ConfigQueryType eQuery,
                           void *pData, int nDataSize);
    /** Pauses a protocol layer.
     *
     * A protocol layer may need to inform the server that it will not be
     * retrieving data for a while. This function provides a hint to do so.
     *
     * Any read/seek function performed after a pause shall be considered to be
     * an implicit unpause.
     *
     * @param [in] hContent The handle to operate on.
     * @param [in] bPause   Pause or unpause.
     */
    NvError (*SetPause)(NvCPRHandle hContent, int bPause);

    } NV_CUSTOM_PROTOCOL;

/** @name Simple File Format Definition for Container-less Protocols (e.g., RTSP/RTP)
 *
 * NEM
 *
 * Corresponds to ::NvParserCoreType_NemParser.
 *
 * A typical audio/video stream will have the following layout:
 * <pre>
 *   NEM_FILE_HEADER
 *   NEM_PACKET_HEADER (packetType 'ah', streamIndex 0)
 *   NEM_AUDIO_FORMAT_HEADER (streamIndex 0)
 *   NEM_PACKET_HEADER (packetType 'vh', streamIndex 1)
 *   NEM_VIDEO_FORMAT_HEADER (streamIndex 1)
 *   NEM_PACKET_HEADER (packetType 'da', streamIndex 0)
 *   .. data of size in previous header ..
 *   NEM_PACKET_HEADER (packetType 'da', streamIndex 1)
 *   .. data of size in previous header ..
 *   NEM_PACKET_HEADER (packetType 'da', streamIndex 1)
 *   .. data of size in previous header ..
 *   .... etc ....
 * </pre>
 *
 * To allow seeking, etcetera, in a local file playback situation, a
 * yet-to-be defined format for an index table can be appended at the end.
 *
 * All time values are in 100 ns units.
 */
/*@{*/


/** Holds an NVIDIA NEM format file header.
 *  Must appear as the first chunk. 
 */
typedef struct NEM_FILE_HEADER
{
    NvU32 magic;       /**< Must be 'NvMM'. */
    NvU32 size;        /**< Size of this file header structure. */
    NvU32 version;     /**< Must be '1'. */
    NvU32 numStreams;  /**< Number of streams. */
    NvU64 indexOffset; /**< If 0, no index table present; format to be defined. */
} NEM_FILE_HEADER;

/** Holds an NVIDIA NEM format packet header.
 * Appears before every packet, including
 * the two format headers below, but not including the file header.
 */
typedef struct NEM_PACKET_HEADER
{
    /// Possible values:
    /// - 'ah' - audio header
    /// - 'vh' - video header
    /// - 'da' - data packet
    NvU16 packetType;
    /// Which stream the packet corresponds to.
    NvU16 streamIndex;
    /// Size of the data packet, not including this header.
    NvU32 size;

/// Some packets may be split into 
/// smaller pieces; this flag indicates
/// the last piece of a larger packet.
#define NEM_PACKET_FLAG_ENDOFPACKET 1
/// This packet should be skipped,
/// possibly corrupted, etc.
#define NEM_PACKET_FLAG_SKIPPACKET  2
        NvU32 flags;       /**< Bitfield of flags. */
        NvU64 timestamp;   /**< Timestamp in 100ns units. */
    } NEM_PACKET_HEADER;

/** Holds an NVIDIA NEM format audio format header.
 * Most fields should be self-explanatory.
 */
typedef struct NEM_AUDIO_FORMAT_HEADER
{
    NvStreamType type;  /**< Codec type. */
    NvU64 duration;     /**< Duration in 100ns units. */
    NvU32 sampleRate;
    NvU32 bitRate;
    NvU32 channels;
    NvU32 bitsPerSample; 
} NEM_AUDIO_FORMAT_HEADER;
/** Holds an NVIDIA NEM format video format header.
 * Most fields should be self-explanatory.
 */
typedef struct NEM_VIDEO_FORMAT_HEADER
{
    NvStreamType type;  /**< Codec type, see nvmm_streamtypes.h. */
    NvU64 duration;     /**< Duration in 100ns units. */
    NvU32 width;
    NvU32 height;
    NvU32 fps; // as a Q16.16
    NvU32 bitRate;
} NEM_VIDEO_FORMAT_HEADER;
/** Holds an AAC Audio format header.
 * 
 */
typedef struct NEM_AAC_FORMAT_HEADER
{
        NvU32 objectType;
        NvU32 samplingFreqIndex;
        NvU32 samplingFreq;
        NvU32 noOfChannels;
        NvU32 channelConfiguration;
} NEM_AAC_FORMAT_HEADER;

/// Helper macros
#define NEM_FOURCC(a, b, c, d) ((NvU32)(NvU8)(d) | ((NvU32)(NvU8)(c) << 8) | ((NvU32)(NvU8)(b) << 16) | ((NvU32)(NvU8)(a) << 24))
#define NEM_TWOCC(a, b) ((NvU32)(NvU8)(b) | ((NvU32)(NvU8)(a) << 8))
/*@}*/


/** @} */

/* Register a NV_CUSTOM_PROTOCOL pReader to handle protocol szProto. 
 *  
 * This is for internal use only. For OpenMAX, use NVOMX_RegisterNVCustomProtocol.
 *
 * Associates a protocol (eg, "drm://") with a NV_CUSTOM_PROTOCOL 
 * structure.  Any files then opened with the protocol prefix will use the
 * functions in pProtocol for all file access.
 *
 * @param proto   The protocol name to register, must include '://'
 * @param pReader   Pointer to an NV_CUSTOM_PROTOCOL implementation
 */
NvError NvRegisterProtocol(const char *proto, NV_CUSTOM_PROTOCOL *pReader);

/* Free all the registered custom protocols. 
 *
 *  This is for internal use only
 */
void NvFreeAllProtocols(void);

#if defined(__cplusplus)
}
#endif

#endif

/* File EOF */
