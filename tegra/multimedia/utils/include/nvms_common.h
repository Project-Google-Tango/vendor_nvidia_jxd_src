/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * \mainpage NVIDIA Media Streaming API
 *
 * \section intro Introduction
 *
 * NVIDIA Media Streaming API implements functionality for streaming media over
 * network. The API has two components: Server API and a Source API.
 * Server API (\ref nvms_server.h) implements media encode/mux and
 * network streaming. Source API (\ref nvms_client.h) implements functionality
 * for clients (e.g., Media Player) to connect and send buffers to a Server
 * where these buffers can be optionally encoded and muxed before transport over
 * network
 *
 * \section server_api_usage Media Streaming Server API Usage
 *
 * Following is an example of expected usage of Server API/Source API
 *
 *  - Server application creates a server instance using \ref NvMS_ServerOpen. The
 *    configuration (\ref NvMS_ServerConfig) sets up callback function for
 *    any notifications from Server Implementation to Server Application.
 *  - Server application creates a port instance using \ref NvMS_ServerCreatePort
 *  - \ref NvMS_ServerCreatePort expects Configuration (\ref NvMS_PortConfig) that specifies
 *    network configuration (\ref NvMS_NetworkConfig), Media configuration (\ref NvMS_StreamConfig),
 *    and Muxer configuration (\ref NvMS_MuxConfig)
 *  - Server application sets routing (\ref NvMS_Route) of a client specified by a uSourceId
 *    to port(s), specifying type of routing, by using Route API (\ref NvMS_ServerSetRoute)
 *    - \ref NVMS_SERVER_ROUTE_REMOTE : For remote only streaming
 *    - \ref NVMS_SERVER_ROUTE_NATIVE : For native only playback
 *    - \ref NVMS_SERVER_ROUTE_JOINT : For joint playback
 *  - A client application uses Source API (\ref nvms_common.h) to setup a connection with Server
 *    implementation
 *  - On a successful connection client application sets configuration (\ref NvMS_BufferConfig)
 *    for buffers it can stream by using Configure API (\ref NvMS_SourceConfigure)
 *  - Server implementation on finding route match of client id and a port (\ref NvMS_Port), sets up
 *    an internal pipeline and starts streaming data on a port (\ref NvMS_Port)
 *  - Server Application can stop or start remote streaming for a particular port (\ref NvMS_Port)
 *    with \ref NvMS_ServerStopStreaming or \ref NvMS_ServerStartStreaming
 *  - Server application destroys a port instance with \ref NvMS_ServerDestroyPort
 *  - Server application destroys server instance with \ref NvMS_ServerClose
 *
 */

#ifndef __NVMS_COMMON_H__
#define __NVMS_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Invalid client identifier
 */
#define NVMS_SOURCE_ID_INVALID (0x7FFFFFFF)

/**
 * \brief Maximum number of streams allowed per client instance
 */
#define NVMS_MAX_NUM_STREAMS (3)

/**
 * \brief Invalid PTM
 */
#define NVMS_PTM_INVALID (0x8000000000000000LL)

/**
 * \brief Opaque handle type for a Port
 */
typedef void* NvMS_Port;

/**
 * \brief Status enum
 */
typedef enum {
    NVMS_STATUS_OK = 0,
    NVMS_STATUS_FALSE,
    NVMS_STATUS_FAIL = 0x80000000 + NVMS_STATUS_OK + 1,
    NVMS_STATUS_INVALID_POINTER,
    NVMS_STATUS_INVALID_DATA,
    NVMS_STATUS_BAD_PARAMETER,
    NVMS_STATUS_TIMED_OUT,
    NVMS_STATUS_INSUFFICIENT_MEMORY,
    NVMS_STATUS_NOT_IMPLEMENTED,
} NvMS_Status;

/**
 * \brief Audio Format. Server sets this to indicate expected audio format
 * Source sets this to indicate type of video buffers available
 */
typedef enum {
    NVMS_AUDIO_FORMAT_PCM,
    NVMS_AUDIO_FORMAT_UND = 0x7FFFFFFF
} NvMS_AudioFormat;

/**
 * \brief Video Format. Server sets this to indicate expected video format
 * Source sets this to indicate type of video buffers available
 */
typedef enum {
    NVMS_VIDEO_FORMAT_H264,
    NVMS_VIDEO_FORMAT_MPEG,
    /*! Server sets this to indicate it does not expect re-encoding */
    NVMS_VIDEO_FORMAT_UND = 0x7FFFFFFF
} NvMS_VideoFormat;

/**
 * \brief Video Buffer Format. This is set by client to indicate type
 * of video buffers produced. Server leaves this undefined
 */
typedef enum {
    NVMS_VIDEO_BUFFER_FORMAT_NVMEDIA,
    NVMS_VIDEO_BUFFER_FORMAT_EGLIMAGE,
    NVMS_VIDEO_BUFFER_FORMAT_UND = 0x7FFFFFFF
} NvMS_VideoBufferFormat;

/**
 * \brief Endian enum
 */
typedef enum {
    NVMS_ENDIAN_LITTLE,
    NVMS_ENDIAN_BIG,
    NVMS_ENDIAN_UND = 0x7FFFFFFF
} NvMS_Endian;

/**
 * \brief Audio Configuration.
 */
typedef struct {
    unsigned int uBitsPerSample;
    unsigned int uSampleRate;
    unsigned int uNumChannels;
    unsigned int uPacketSize;
    char *pDevicePath;
    NvMS_Endian eEndian;
    NvMS_AudioFormat eFormat;
} NvMS_AudioConfig;

/**
 * \brief File Configuration.
 */
typedef struct {
    char *pPath;
} NvMS_MiscConfig;
/**
 * \brief Video Configuration. Server sets this to indicate encoding
 * configuration if applicable. Source sets this to indicate details
 * of Video data produced. For re-muxing use cases, these fields can
 * be used by client to share information about video stream
 */
typedef struct {
    /*! Video Encode width */
    unsigned int uWidth;
    /*! Video Encode height */
    unsigned int uHeight;
    /*! Video Encode GOP size */
    unsigned int uGopSize;
    /*! Video Encode bit rate */
    unsigned int uBitRate;
    /*! Video Encode format */
    NvMS_VideoFormat eFormat;
    /*! Video buffer format - This field is applicable only for client */
    NvMS_VideoBufferFormat eBufferFormat;
} NvMS_VideoConfig;

/**
 * \brief Subpicture pixel format. Used to indicate pixel format used for storing subpicture data
 */
typedef enum {
    NVMS_SUBPIC_PIXEL_FORMAT_RGBA = 0x1,
    NVMS_SUBPIC_PIXEL_FORMAT_UND  = 0x7FFFFFFF
} NvMS_SubPicPixelFormat;

/**
 * \brief Subpicture encode format. Used to indicate subpicture encode format
 */
typedef enum {
    NVMS_SUBPIC_ENCODE_FORMAT_RLE = 0x1,
    NVMS_SUBPIC_ENCODE_FORMAT_UND = 0x7FFFFFFF
} NvMS_SubPicEncodeFormat;

/**
 * \brief Subpicture Configuration.
 */
typedef struct {
    unsigned int uWidth;
    unsigned int uHeight;
    NvMS_SubPicPixelFormat ePixelFormat;
    NvMS_SubPicEncodeFormat eEncodeFormat;
} NvMS_SubPicConfig;

/**
 * \brief Media Configuration
 */
typedef union {
    /*! Audio Config */
    NvMS_AudioConfig oAudioConfig;
    /*! Video Config */
    NvMS_VideoConfig oVideoConfig;
    /*! Subpicture Config */
    NvMS_SubPicConfig oSubPicConfig;
    /*! File Config */
    NvMS_MiscConfig oMiscConfig;
} NvMS_MediaConfig;

/**
 * \brief Stream type. Used to indicate stream type for multi stream clients
 */
typedef enum {
    NVMS_STREAM_TYPE_AUDIO  = 0x1,
    NVMS_STREAM_TYPE_VIDEO  = 0x2,
    NVMS_STREAM_TYPE_SUBPIC = 0x4,
    NVMS_STREAM_TYPE_MISC   = 0x8,
    NVMS_STREAM_TYPE_UND = 0x7FFFFFFF
} NvMS_StreamType;

/**
 * \brief Stream Configuration
 */
typedef struct {
    /*! Stream type */
    NvMS_StreamType eType;
    /*! Stream Media config */
    NvMS_MediaConfig oMediaConfig;
} NvMS_StreamInfo;

/**
 * \brief Stream direction. This is used to indicate direction of streaming.
 * Eg., Whether Server will be receiving data on port or sending data on port.
 */
typedef enum {
    NVMS_STREAM_DIR_TO_PORT,
    NVMS_STREAM_DIR_FROM_PORT,
    NVMS_STREAM_DIR_UND = 0x7FFFFFFF
} NvMS_StreamDirection;

/**
 * \brief Stream Configuration. This is a top level data structure used to
 * indicate overall configuration for a given port or a client.
 */
typedef struct {
    /*! Number of streams */
    unsigned int uNumStreams;
    /*! Streaming Direction */
    NvMS_StreamDirection eDir;
    /*! Stream Configuration */
    NvMS_StreamInfo oStreamInfo[NVMS_MAX_NUM_STREAMS];
} NvMS_StreamConfig;

/**
 * \brief Buffer Configuration
 */
typedef struct {
    /*! Source identifier */
    unsigned int uSourceId;
    /*! Stream configuration */
    NvMS_StreamConfig oStreamConfig;
} NvMS_BufferConfig;

/**
 * \brief Routing types
 */
typedef enum {
    /*! No routing specified */
    NVMS_ROUTE_NONE   = 0x0,
    /*! Source is expected to playback media natively only */
    NVMS_ROUTE_NATIVE = 0x1,
    /*! Source is expected to playback media remotely only */
    NVMS_ROUTE_REMOTE = 0x2,
    /*! Source is expected to playback media both remotely and natively */
    NVMS_ROUTE_JOINT  = 0x3,
    NVMS_ROUTE_UND    = 0x7FFFFFFF
} NvMS_RouteType;

/**
 * \brief Routing information
 */
typedef struct {
    /*! Source identifier */
    unsigned int uSourceId;
    /*! Port identifier */
    NvMS_Port hPort;
    /*! Per streaming routing information */
    NvMS_RouteType StreamRoute[NVMS_MAX_NUM_STREAMS];
} NvMS_Route;

/**
 * \brief Buffer Handle. Opaque buffer handle to process buffers. Used only by client
 */
typedef void* NvMS_BufferHandle;

/**
 * \brief Audio Buffer Structure. Buffer structure for audio data. Used only by client
 */
typedef struct {
    /*! Data pointer */
    unsigned char *pData;
    /*! Data size */
    unsigned int uDataSize;
    /*! PTM */
    signed long long int sPtm;
    /*! Packet count */
    unsigned long long int uPacketCount;
} NvMS_ByteBuffer;

/**
 * \brief Buffer Structure. Generic buffer structure for a stream. Used only by client
 */
typedef struct {
    /*! Buffer handle */
    NvMS_BufferHandle hBuffer;
    /*! Stream Index */
    unsigned int uStreamIndex;
} NvMS_Buffer;

#ifdef __cplusplus
};     /* extern "C" */
#endif

#endif /* __NVMS_COMMON_H__ */

