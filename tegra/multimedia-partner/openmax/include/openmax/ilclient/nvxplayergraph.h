/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** 
 * @file
 * <b>NVIDIA Tegra: OpenMAX Graph Helper Library PlayerGraph Interface/b>
 *
 */

#ifndef NVX_PLAYERGRAPH_H
#define NVX_PLAYERGRAPH_H

/** defgroup nv_omx_graphhelper_playergraph PlayerGraph Interface
 *
 * This is the NVIDIA OpenMAX Graph Helper Library, PlayerGraph Interface.
 * 
 * This playergraph interface abstracts the concept of an OMX IL playback
 * graph, and provides a high level playback API built on top of the lower 
 * level Graph interface.
 *
 * @ingroup nvomx_graphhelper
 * @{
 */

#include "nvxgraph.h"
#include "NVOMX_ParserExtensions.h"

#define NVX_ISSTREAMVIDEO(x) (((x) == NvxStreamType_MPEG4) ||   \
    ((x) == NvxStreamType_H264) || ((x) == NvxStreamType_H263) ||   \
    ((x) == NvxStreamType_WMV) || ((x) == NvxStreamType_MJPEG)) \

#define NVX_ISSTREAMAUDIO(x) (((x) == NvxStreamType_MP2) ||   \
    ((x) == NvxStreamType_MP3) || ((x) == NvxStreamType_AAC) ||    \
    ((x) == NvxStreamType_AACSBR) || ((x) == NvxStreamType_WMA) ||    \
    ((x) == NvxStreamType_WMA) || ((x) == NvxStreamType_WMAPro) ||    \
    ((x) == NvxStreamType_AMRWB) || ((x) == NvxStreamType_AMRWB) ||    \
    ((x) == NvxStreamType_AMRNB) || ((x) == NvxStreamType_WAV))    \

/** Media Player Callback Type Enumeration
*/
typedef enum {
    NvxMediaPlayerCallbackType_ERROR,    /**< Error Event */
    NvxMediaPlayerCallbackType_EOS,      /**< EOS Event */
    NvxMediaPlayerCallbackType_StreamChange,  /**<Stream Change */
    NvxMediaPlayerCallbackType_Force32 = 0x7FFFFFFF
} NvxMediaPlayerCallbackType;

typedef struct NvxMediaPlayerCallback {
    void (*eventHandler) (
        NvxMediaPlayerCallbackType type,
        NvU32 param,
        void* pContext);  /**< EventHandler Callback */
} NvxMediaPlayerCallback;

/** Forward declaration of various structures */
struct NvxPlayerGraphPrivRec;

/** Type of playback graph to create */
typedef enum
{
    NvxPlayerGraphType_Normal = 1,       /**< Normal playback */
    NvxPlayerGraphType_Metadata,         /**< Metadata extraction only */
    NvxPlayerGraphType_FrameExtraction,  /**< Metadata and thumbnail creation */
    NvxPlayerGraphType_JpegDecode,       /**< Image Decode */
    NvxPlayerGraphType_Force32 = 0x7FFFFFFF
} NvxPlayerGraphType;

typedef struct NvxPlayerAppCallBack_
{
    void * pCallBack;
    void * pAppData;
} NvxPlayerAppCallBack;

/** The main player graph structure
 */
typedef struct NvxPlayerGraphRec 
{
    NvxFramework framework;   /**< Framework used to create this graph */
    NvxGraph *graph;          /**< Pointer to the low level graph abstraction */

    NvxPlayerGraphType type;  /**< Type of graph this is */
    
    OMX_U8 *uri;              /**< The current URI for this playergraph */
    OMX_U8 *userAgent;              /**< userAgent for streaming playback */

    /** informational values, valid after player init
     * Will be replaced by get functions at some point */
    OMX_TICKS durationInMS;   /**< Duration in milliseconds */
    ENvxStreamType audioType; /**< Type of audio stream */
    ENvxStreamType videoType; /**< Type of video stream */
    OMX_U32 vidWidth;         /**< Width of video stream */
    OMX_U32 vidHeight;        /**< Height of video stream */
    OMX_U32 vidBitrate;       /**< Bitrate of video stream */
    OMX_U32 vidFramerate;
    OMX_U32 audSample;        /**< Samples per second of audio stream */
    OMX_U32 audChannels;      /**< Number of channels in audio stream */
    OMX_U32 audBitrate;       /**< Bitrate of audio stream */
    OMX_U32 audBitsPerSample; /**< Bits per sample of audio stream */
    OMX_U32  StreamCount;    /**<TotalStreams */
    NvxPlayerAppCallBack appCallBack; /**<Register the application Callback*/
    struct NvxPlayerGraphPrivRec *priv; /**< Private internal data */
} NvxPlayerGraph2;

/** Initialize a new PlayerGraph of the specified type and URI
 *
 * Must be created with NvxPlayerGraphCreate after successfull initialization.
 *
 * @param [in] framework
 *     The framework created by NvxFrameworkInit earlier
 * @param [out] player
 *     The newly created PlayerGraph
 * @param [in] type
 *     The type of PlayerGraph to create
 * @param [in] uri
 *     The URI to play.
 * @param [in] userAgent
 *     The userAgent for streaming.
 * @retval OMX_ERRORTYPE 
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxPlayerGraphInit(NvxFramework framework,
                                 NvxPlayerGraph2 **player, 
                                 NvxPlayerGraphType type,
                                 const OMX_U8 *uri,
                                 const OMX_U8 *userAgent);

/** Create the PlayerGraph initialized in NvxPlayerGraphInit
 *
 * @param [in] player
 *     The PlayerGraph from NvxPlayerGraphInit
 * @retval OMX_ERRORTYPE 
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxPlayerGraphCreate(NvxPlayerGraph2 *player);

/** Destroy a PlayerGraph, freeing all resources
 * 
 * @param [in] player
 *     The PlayerGraph structure
 * @retval OMX_ERRORTYPE 
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxPlayerGraphDeinit(NvxPlayerGraph2 *player);

/** events **/

/** Returns the status if there have been any streaming-based buffering events
 *
 * Calling this clears the internal 'has buffering event state'
 *
 * @param [in] player
 *     The PlayerGraph structure
 * @param [out] needPause
 *     Will be true if the player graph thinks that things should pause for
 *     rebuffering
 * @param [out] bufferPercent
 *     The current buffering percentage, expressed as <FIXME>
 * @retval OMX_BOOL
 *     Returns true if there is a buffering event to act upon
 */
OMX_BOOL NvxPlayerGraphHasBufferingEvent(NvxPlayerGraph2 *player,
                                         OMX_BOOL *needPause, 
                                         OMX_U32 *bufferPercent);

/** config options **/

/** Pass in a dispmgr overlay surface to the renderer
 * 
 * @param [in] player
 *     The PlayerGraph structure
 * @param [in] overlay
 *     The dispmgr overlay pointer
 */
void NvxPlayerGraphSetRendererOverlay(NvxPlayerGraph2 *player, 
                                      NvUPtr overlay);

/** Use the specified component name as an audio renderer
 *
 * @param [in] player
 *     The PlayerGraph structure
 * @param [in] audRenderName
 *     The component name to use as an audio renderer
 */
void NvxPlayerGraphUseExternalAudioRenderer(NvxPlayerGraph2 *player,
                                            char *audRenderName);

/** Graph manipulation **/

/** Stop playback
 * 
 * @param [in] player
 *     The PlayerGraph structure
 * @retval OMX_ERRORTYPE 
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxPlayerGraphStop(NvxPlayerGraph2 *player);

/** Pause playback
 * 
 * @param [in] player
 *     The PlayerGraph structure
 * @param [in] pause
 *     Pause or unpause the playback
 * @retval OMX_ERRORTYPE 
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxPlayerGraphPause(NvxPlayerGraph2 *player, OMX_BOOL pause);

/** Seek to the specified time in playback
 * 
 * @param [in] player
 *     The PlayerGraph structure
 * @param [inout] timeInMS
 *     Time in milliseconds to seek to, returns the time actually seeked to
 * @retval OMX_ERRORTYPE 
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxPlayerGraphSeek(NvxPlayerGraph2 *player, int *timeInMS);

/** Initialize a playback graph by putting it into idle from loaded
 * 
 * @param [in] player
 *     The PlayerGraph structure
 * @retval OMX_ERRORTYPE 
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxPlayerGraphToIdle(NvxPlayerGraph2 *player);

/** Destroy a playback graph
 * 
 * @param [in] player
 *     The PlayerGraph structure
 * @retval OMX_ERRORTYPE 
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxPlayerGraphTeardown(NvxPlayerGraph2 *player);

/** Start playback
 * 
 * @param [in] player
 *     The PlayerGraph structure
 * @retval OMX_ERRORTYPE 
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxPlayerGraphStartPlayback(NvxPlayerGraph2 *player);

/** metadata helpers **/

/** Extract the specified metadata key
 *
 * @param [in] player
 *     The PlayerGraph structure
 * @param [in] type
 *     The metadata key to retrieve
 * @param [out] retdata
 *     The returned metadata - buffer must be free()d after use
 * @param [out] retLen
 *     The length of the returned metadata
 * @retval OMX_ERRORTYPE 
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxPlayerGraphExtractMetadata(NvxPlayerGraph2 *player, 
                                            ENvxMetadataType type,
                                            OMX_U8 **retData, int *retLen);

/** Switch a metadata extractor playergraph (type NvxPlayerGraphType_Metadata)
 * to a different URI.
 *
 * @param [in] player
 *     The PlayerGraph structure
 * @param [in] uri
 *     The new URI
 * @retval OMX_ERRORTYPE 
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxPlayerGraphMetaSwitchToTrack(NvxPlayerGraph2 *player,
                                              const OMX_U8 *uri);

/** Start extracting a video frame from the player.  Call NvxPlayerGraphGetFrame
 * to finish extracting the frame.
 *
 * Must be of type NvxPlayerGraphType_FrameExtraction
 *
 * @param [in] player
 *     The PlayerGraph structure
 * @param [in] timeInMS
 *     Time in the URI to extract a frame from
 * @param [out] dataLen
 *     The size of the buffer required to fit the extracted frame
 * @param [out] width
 *     The width of the extracted frame
 * @param [out] height
 *     The height of the extracted frame
 * @retval OMX_ERRORTYPE 
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxPlayerGraphExtractFrame(NvxPlayerGraph2 *player, int timeInMS,
                                         int *dataLen, int *width, int *height);

/** Finish extracting a frame from NvxPlayerGraphExtractFrame.
 * 
 * @param [in] player
 *     The PlayerGraph structure
 * @param [out] data
 *     A buffer allocated to be at least dataLen bytes
 * @param [in] dataLen
 *     Size of the 'data' buffer.  Must by >= the dataLen returned from
 *     NvxPlayerGraphExtractFrame
 * @retval OMX_ERRORTYPE 
 *     Returns an appropriate error
 */
OMX_ERRORTYPE NvxPlayerGraphGetFrame (
    NvxPlayerGraph2 *pGraph,
    char **data,
    int dataLen);

/**Set PlaySpeed
 * @param [in] player
 *     The PlayerGraph structure
 *@param [in] playspeed
 *     The speed can be any value from -32 to 32
 *      excluding 0
 *@param [in] timeInMS
 *      start of play time in milliseconds
 * @retval OMX_ERRORTYPE
 */
OMX_ERRORTYPE NvxPlayerGraphSetRate (
    NvxPlayerGraph2 *player,
    float playSpeed);

/**set Callback  registers callback
 * @param [in] player
 *     The PlayerGraph structure
*@param [in]  pAppCallback
*    callback pointer is registered
*@param [in]  pAppData
*    call back related data parameters
*/
void NvxPlayerGraphSetCallBack (
    NvxPlayerGraph2 *player,
    void *pAppCallback,
    void *pAppData);
/**get Duration of playback
 * * @param [in] player
 *     The PlayerGraph structure
 * @param [out] time
 *    update the total time of playback
*/
OMX_ERRORTYPE NvxPlayerGraphGetDuration (
    NvxPlayerGraph2 *player,
    NvS64 *time);

/**get Position of playback
 * * @param [in] player
 *     The PlayerGraph structure
 * @param [out] time
 *    update the Current time of playback
*/
OMX_ERRORTYPE NvxPlayerGraphGetPosition (
    NvxPlayerGraph2 *player,
    NvS64 *time);

/**Set volume level
 * @param [in] player
 *     The PlayerGraph structure
 *@param [in] volume.
 *     The volume level to be set.
 * @retval OMX_ERRORTYPE
 */
OMX_ERRORTYPE NvxPlayerGraphSetVolume (
    NvxPlayerGraph2 *player,
    OMX_S32 volume);

/**Get volume level
 * @param [in] player
 *     The PlayerGraph structure
 *@param [out] volume.
 *     The volume level to retrieve.
 * @retval OMX_ERRORTYPE
 */
OMX_ERRORTYPE
NvxPlayerGraphGetVolume (
    NvxPlayerGraph2 *player,
    void *volume);

/**Get StreamInfo:  query to get StreamInfo
 * @param [in] player
 *     The PlayerGraph structure
 *@param [in] NvxComponent
 *@param [out] streaminfo structure to be filled.
 * @retval OMX_ERRORTYPE
 */
OMX_ERRORTYPE NvxPlayerGetStreamInfo (
    NvxPlayerGraph2 *player,
    NvxComponent *comp,
    void *pStreamInfo);

/**Set Volume Mute
* @param [in] player
*     The PlayerGraph structure
*@param [in] mute.
*     The enable/disable mute.
* @retval OMX_ERRORTYPE
*/
OMX_ERRORTYPE NvxPlayerGraphSetMute (
    NvxPlayerGraph2 *player,
    OMX_BOOL mute);

/**Get Volume Mute
* @param [in] player
*     The PlayerGraph structure
*@param [out] mute.
* @retval OMX_ERRORTYPE
*/
OMX_ERRORTYPE NvxPlayerGraphGetMute (
    NvxPlayerGraph2 *player,
    void *mute);

/**Set rotation
* @param [in] player
*     The PlayerGraph structure
*@param [in] Angle.
*     The Angle to rotate.
* @retval OMX_ERRORTYPE
*/
OMX_ERRORTYPE NvxPlayerGraphSetRotateAngle (
    NvxPlayerGraph2 *player,
    NvS32 Angle);

/**Set Mirror
* @param [in] player
*     The PlayerGraph structure
*@param [in] mirror.
* @retval OMX_ERRORTYPE
*/
OMX_ERRORTYPE NvxPlayerGraphSetMirror (
    NvxPlayerGraph2 *player,
    NvS32 mirror);

/**Set VideoScaling
* @param [in] player
*     The PlayerGraph structure
*@param [in] videoScale.
* @retval OMX_ERRORTYPE
*/
 OMX_ERRORTYPE NvxPlayerGraphSetVideoScale (
    NvxPlayerGraph2 *player,
    NvU64 videoScale);

/**Set BackgroundColor
* @param [in] player
*     The PlayerGraph structure
*@param [in] Color.
* @retval OMX_ERRORTYPE
*/
OMX_ERRORTYPE NvxPlayerGraphSetBackgroundColor (
    NvxPlayerGraph2 *player,
    NvU32 Color);

/**Set DestRectangle
* @param [in] player
*     The PlayerGraph structure
*@param [in] videoRect.
* @retval OMX_ERRORTYPE
*/
OMX_ERRORTYPE NvxPlayerGraphSetDestRectangle (
    NvxPlayerGraph2 *player,
    void *videoRect);

/**Set SourceRectangle
* @param [in] player
*     The PlayerGraph structure
*@param [in] videoRect.
* @retval OMX_ERRORTYPE
*/
OMX_ERRORTYPE NvxPlayerGraphSetSourceRectangle (
    NvxPlayerGraph2 *player,
    void *videoRect);
#endif

/** @} */
/* File EOF */
