/* Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
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
 * <b>NVIDIA Tegra: OpenMAX Container Tracklist Extension Interface</b>
 *
 */

#ifndef NVOMX_TrackList_h
#define NVOMX_TrackList_h

/***************************************************************
 ****  THIS INTERFACE IS DEPRECATED AND NO LONGER SUPPORTED ****
 ***************************************************************/

/**
 * @defgroup nv_omx_il_tracklist Tracklist
 *   
 * This is the NVIDIA OpenMAX container demux class tracklist interface extension.
 *
 * It defines the container extension interface to play multiple files with one graph
 * as well as support for gapless audio playback.
 *
 * @ingroup nvomx_container_extension
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <OMX_Core.h>

/**
 * If NvxTrack_AutoAdvance is set, the next track will automatically be
 * played
 */
typedef enum
{
    NvxTrack_NoFlags        = 0x00000000,
    NvxTrack_AutoAdvance    = 0x00000001,

    NvxTrack_Force32        = 0x7FFFFFFF
} NvxTrackFlags;

/**
 * Filetypes to override the automatic detection.
 */
typedef enum
{
    NvxType_Default = 0, // Generally, use this.
    NvxType_Asf, // asf, wma, wmv
    NvxType_3gp, // mov, 3gp, m4a, m4v
    NvxType_Avi,
    NvxType_Mp3,
    NvxType_Ogg,
    NvxType_Ogm,
    NvxType_Wav,
    NvxType_Mkv, // mkv
    NvxType_Force32 = 0x7FFFFFFF
} NvxFileType;
/**
 * Holds information about a single track.
 *
 * pClientPrivate may be set to anything the IL client desires
 *
 * pPath _must_ be in UTF-8.
 */
typedef struct NvxTrackInfoRec
{
    NvxTrackFlags eTrackFlag;
    void *pClientPrivate;
    OMX_U32 uSize;
    OMX_U8 *pPath;
    NvxFileType eType;
} NvxTrackInfo;

typedef OMX_HANDLETYPE NvxTrackList;

/**
 * Get/Set a tracklist handle.  
 *
 * Get the handle from the parser component, and set it on all other components
 * in the chain (ie, audio decoder, audio renderer).
 */
#define NVX_INDEX_CONFIG_TRACKLIST "OMX.Nvidia.index.config.tracklist"
/** Holds a tracklist handle. */
typedef struct NVX_CONFIG_TRACKLIST
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    NvxTrackList hTrackList;        /**< Tracklist handle */
} NVX_CONFIG_TRACKLIST;

/**
 * Add/Get a track.
 *
 * Add a track to the specified position (uIndex) with SetConfig.
 * Get info on a track at the specified position (uIndex) with GetConfig.
 * 
 * Only valid on the parser component.
 */ 
#define NVX_INDEX_CONFIG_TRACKLIST_TRACK "OMX.Nvidia.index.config.tracklist.track"
/** Holds tracklist information. */
typedef struct NVX_CONFIG_TRACKLIST_TRACK
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 uIndex;                 /**< Index of track */
    NvxTrackInfo *pTrackInfo;       /**< Track information */
} NVX_CONFIG_TRACKLIST_TRACK;

/**
 * Delete tracks from a tracklist.
 *
 * Use -1 for nIndex to delete all tracks from the tracklist.
 *
 * Only valid on the parser component.
 */
#define NVX_INDEX_CONFIG_TRACKLIST_DELETE "OMX.Nvidia.index.config.tracklist.delete"
/** Holds a tracklist index. */
typedef struct NVX_CONFIG_TRACKLIST_DELETE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_S32 nIndex;                 /**< Index of track to delete */
} NVX_CONFIG_TRACKLIST_DELETE;

/**
 * Query the size of the tracklist.
 *
 * Only valid as a GetConfig on the parser component.
 */
#define NVX_INDEX_CONFIG_TRACKLIST_SIZE "OMX.Nvidia.index.config.tracklist.size"
/** Holds a tracklist size. */
typedef struct NVX_CONFIG_TRACKLIST_SIZE
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 uTracklistSize;         /**< Number of tracks in the tracklist */
} NVX_CONFIG_TRACKLIST_SIZE;

/**
 * Get/Set the current track.
 *
 * pTrackInfo will be filled on GetConfig, and unused on SetConfig.
 *
 * Only valid on the parser component.
 */
#define NVX_INDEX_CONFIG_TRACKLIST_CURRENT "OMX.Nvidia.index.config.tracklist.current"
/** Holds tracklist information. */
typedef struct NVX_CONFIG_TRACKLIST_CURRENT
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 uIndex;                 /**< Index of track to query info from */
    NvxTrackInfo *pTrackInfo;       /**< Track information */
} NVX_CONFIG_TRACKLIST_CURRENT;

/**
 * This event will be sent when a track is finished playing
 */
#define NVX_EventTrackFinished 0x7A000001

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */
#endif

/* File EOF */

