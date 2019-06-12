/*
* Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

/** @file
* @brief <b>NVIDIA Driver Development Kit:
*           NvDDK Multimedia APIs, Events </b>
*
* @b Description: Declares Interface for NvDDK multimedia blocks (mmblocks).
*/

#ifndef INCLUDED_NVMMLITE_EVENT_H
#define INCLUDED_NVMMLITE_EVENT_H

/**
* @defgroup nvmm_event Media Event API
*
*
* @ingroup nvmm_modules
* @{
*/

#include "nvcommon.h"
#include "nvos.h"
#include "nvcolor.h"
#include "nvrm_surface.h"
#include "nvmmlite.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @brief NvMMLite event enumerations.
 *
 */
typedef enum {
    NvMMLiteEvent_Unknown = 0,
    NvMMLiteEvent_BlockError,             /* uses NvMMLiteBlockErrorInfo */
    NvMMLiteEvent_StreamError,            /* uses NvMMLiteStreamErrorInfo */
    NvMMLiteEvent_SetAttributeError,      /* uses NvMMLiteSetAttributeErrorInfo */
    NvMMLiteEvent_BlockWarning,           /* uses NvMMLiteBlockWarningInfo */
    NvMMLiteEvent_NewBufferRequirements,  /* uses NvMMLiteNewBufferRequirementsInfo */
    NvMMLiteEvent_NewStreamFormat,        /* uses ?? (TBD: formats differ b/w and
                                       * within audio/video/image)
                                       */
    NvMMLiteEvent_BlockClose,             /* uses NvMMLiteBlockCloseInfo */
    NvMMLiteEvent_StreamEnd,              /* uses NvMMLiteEventStreamEndInfo */
    NvMMLiteEvent_ProfilingData,          /* uses NvMMLiteEventProfilingData */
    NvMMLiteEvent_TimeStampUpdate,        /* uses NvMMLiteEventTimeStampUpdate */
    NvMMLiteEvent_BlockStreamEnd,         /* uses NvMMLiteEventBlockStreamEndInfo */
    NvMMLiteEvent_ResetStreamEnd,         /* Uses NvMMLiteEventStreamEndInfo */
    NvMMLiteEvent_MarkerHit,              /* Uses NvMMLiteEventMarkerHitInfo */
    NvMMLiteEvent_VideoStreamInit,        /* Uses NvMMLiteEventVideoStreamInitInfo */
    NvMMLiteEvent_AudioStreamInit,        /* Uses NvMMLiteEventAudioStreamInitInfo */
    NvMMLiteEvent_VideoFastUpdate,        /* Doesn't use any structrue for time being */
    NvMMLiteEvent_DRMPlayCount,           /* Uses NvMMLiteEventDRMPlayCntInfo */
    NvMMLiteEvent_NewMetaDataAvailable,   /*Uses NvMMLiteEventNewMetaDataAvailable*/
    NvMMLiteEvent_ForBuffering,           /*Uses NvMMLiteEventForBuffering*/
    NvMMLiteEvent_DoMoreWork,

    /* Events specific to video, image, audio, etc. If included, an offset for
     * that block will be defined here, and then the events will be defined in
     * separate headers, starting with this offset.  By convention, the offsets
     * will use the top 16 bits.
     */
    NvMMLiteEventVideoEnc_EventOffset       =   0x10000,    /* videoenc events defined in nvmm_videoenc.h */
    NvMMLiteEventCamera_EventOffset         =   0x20000,    /* camera events defined in nvmm_camera.h */
    NvMMLiteEventDigitalZoom_EventOffset    =   0x30000,    /* digitalZoom events defined in nvmm_digitalzoom.h */

    NvMMLiteEvent_Force32 = 0x7fffffff
} NvMMLiteEventType;

typedef enum {
    NvMMLiteErrorDomain_Invalid = 0,
    NvMMLiteErrorDomain_DRM,                /* See NvMMLiteErrorDomainDRMInfo */
    NvMMLiteErrorDomain_Parser,
    NvMMLiteErrorDomain_ContentPipe,
    NvMMLiteErrorDomain_Other
} NvMMLiteErrorEventFromDomain;


typedef struct NvMMLiteErrorDomainDRMInfo_
{
    NvU32 error;                //!< Error code
    NvU32 TrackIndex;           //!< TrackIndex

} NvMMLiteErrorDomainDRMInfo;

/**
 * @brief NvMMLite Buffer Format identifier
 */
typedef enum {
    NvMMLiteBufferFormatId_Unknown = 0,
    NvMMLiteBufferFormatId_Audio,
    NvMMLiteBufferFormatId_Video,

    NvMMLiteBufferFormatId_Force32 = 0x7fffffff
} NvMMLiteBufferFormatId;

/**
 * @brief Audio Format
 */
typedef enum {
    NvMMLiteAudioFormat_Unknown = 0,
    NvMMLiteAudioFormat_8Bit_PCM_Mono,
    NvMMLiteAudioFormat_8Bit_PCM_Stereo,
    NvMMLiteAudioFormat_16Bit_PCM_Mono,
    NvMMLiteAudioFormat_16Bit_PCM_Stereo,
    NvMMLiteAudioFormat_24Bit_PCM_Mono,             /*24 bit data in 32bit container left justified */
    NvMMLiteAudioFormat_24Bit_PCM_Stereo,           /*24 bit data in 32bit container left justified */
    NvMMLiteAudioFormat_32Bit_PCM_Mono,
    NvMMLiteAudioFormat_32Bit_PCM_Stereo,
    NvMMLiteAudioFormat_32BitFloat_PCM_Mono,
    NvMMLiteAudioFormat_32BitFloat_PCM_Stereo,
    NvMMLiteAudioFormat_MP3,
    NvMMLiteAudioFormat_AAC,
    NvMMLiteAudioFormat_WMA,
    NvMMLiteAudioFormat_AMRNB,
    NvMMLiteAudioFormat_AMRNB_IF1,
    NvMMLiteAudioFormat_AMRNB_IF2,
    NvMMLiteAudioFormat_AMRWB,
    NvMMLiteAudioFormat_REAL8,
    NvMMLiteAudioFormat_MIDI,
    NvMMLiteAudioFormat_XMF,
    NvMMLiteAudioFormat_OGG,

    NvMMLiteAudioFormat_Force32 = 0x7fffffff
} NvMMLiteAudioFormat;

typedef enum
{
    NvMMLitePlanar_Y = 0,
    NvMMLitePlanar_U = 1,
    NvMMLitePlanar_V = 2,
    NvMMLitePlanar_UV = 1,
    NvMMLitePlanar_Single = 0,

    NvMMLitePlanar_Force32 = 0x7fffffff
} NvMMLitePlanar;

/**
 * @brief Types of block warnings
 */
typedef enum {
    NvMMLiteBlockWarning_InsufficientMemoryForImageStabilization = 1,
    NvMMLiteBlockWarning_InvalidLensShadingCalibration,
    NvMMLiteBlockWarning_SuspiciousLensShadingCalibration,
    NvMMLiteBlockWarning_DelayedSetAttributeFailed,
    NvMMLiteBlockWarning_InvalidOperation,

    //etc

    NvMMLiteBlockWarning_Force32 = 0x7fffffff
} NvMMLiteBlockWarning;

/**
 * @brief Video Format
 * The video format utilizes an array of surface descriptions
 * and the chroma phase.  The standards for Jpeg and Mpeg differ
 * on how the chroma is sampled, and the renderer may have a separate
 * desired sampling as well.  For more details on this, please see
 *  <reference document>.
 * There are three surface descriptions, because planar formats
 * have 2 or 3 surfaces. In order to determine YUV420, YUV422 or
 * YUV422R, one must compare the width/height of each surface.
 * The order of the surfaces must be Luma first, followed by U or UV, then
 * V. (See NvMMLitePlanar enum definitions)
 * For conventional RGB and packed YUV surfaces, only the first surface
 * description is used.
 * SurfaceDescriptions, these are NvRmSurfaces whose mem handles, offsets,
 * and pBase ptrs are nulled and not used.  The pitch, tiling, dimensions,
 * format are used to communicate video formats between nvmm blocks.
 * @see NvMMLitePlanar
 */
typedef struct {
    NvRmSurface SurfaceDescription[NVMMSURFACEDESCRIPTOR_MAX_SURFACES];
    NvU32 NumberOfSurfaces;
    NvU32 SurfaceRestriction;
    NvBool bImage;
    //NvSamplePhase PhaseH;  // add when they have been defined
    //NvSamplePhase PhaseV;
} NvMMLiteVideoFormat;

/**
 * @brief Buffer Format
 * The NvMMLite buffer format can be one of three things,
 * Audio (used by the audio blocks), Video (used by the image and
 * video blocks), or raw data.
 * In the case of raw data, the NvMMLiteBufferFormat is not referenced,
 * so no provision has been made for it.
 * There is no enum to indicate whether to use NvMMLiteBufferFormat.videoFormat
 * or NvMMLiteBufferFormat.audioFormat, as it should be obvious, based off
 * the type of block you are.  (If this assumption is incorrect, we can
 * add an identifier.)
 */
typedef union {
    NvMMLiteVideoFormat videoFormat;
    NvMMLiteAudioFormat audioFormat;
} NvMMLiteBufferFormat;

enum {
    NvMMLiteError_FatalError = 1,
    /* TBD: other errors */

    NvMMLiteError_Force32 = 0x7fffffff
};

/**
 *  The base stream event definition. All event info structure definitions must at least
 * have these fields and definitions and in the same order.
 */
typedef struct NvMMLiteStreamEventBaseInfo
{
    NvU32           structSize; //!< Size in bytes, 0 indicates non-initialized struct
    NvMMLiteEventType   event;      //!< Event ID
} NvMMLiteStreamEventBase;

/**
 * @brief Defines the NvMMLiteEvent_BlockError information concerning an error occurring in the block
 */
typedef struct NvMMLiteBlockErrorInfo_
{
    // first 2 members must match NvMMLiteStreamEventBase
    NvU32 structSize;           //!< Size in bytes, 0 indicates non-initialized struct
    NvMMLiteEventType event;        //!< Event ID

    NvMMLiteErrorEventFromDomain  domainName;
    NvU32 error;                //!< Error code
    union
    {
         NvMMLiteErrorDomainDRMInfo drmInfo;
    } domainErrorInfo;      //!<domain specific error info.
} NvMMLiteBlockErrorInfo;

/**
 * @brief Defines the NvMMLiteEvent_StreamError information concerning an error occurring in a stream.
 */
typedef struct NvMMLiteStreamErrorInfo_
{
    // first 2 members must match NvMMLiteStreamEventBase
    NvU32 structSize;           //!< Size in bytes, 0 indicates non-initialized struct
    NvMMLiteEventType event;        //!< Event ID

    NvError error;              //!< Error code
} NvMMLiteStreamErrorInfo;

/**
 * brief Defines the NvMMLiteEvent_SetAttributeError information concerning a
 * failed call to SetAttribute
 */
typedef struct NvMMLiteSetAttributeErrorInfo_
{
    // first 2 members must match NvMMLiteStreamEventBase
    NvU32 structSize;           //!< Size in bytes, 0 indicates non-initialized struct
    NvMMLiteEventType event;        //!< Event ID

    NvU32 AttributeType;        //!< AttributeType passed in to failed call
    NvU32 AttributeSize;        //!< AttributeSize passed in to failed call
    NvError error;              //!< Error code generated by failed call
} NvMMLiteSetAttributeErrorInfo;

/**
 * @brief Defines the NvMMLiteEvent_BlockWarning information concerning a
 * non-fatal error occurring in the block
 */
typedef struct NvMMLiteBlockWarningInfo_
{
    // first 2 members must match NvMMLiteStreamEventBase
    NvU32 structSize;           //!< Size in bytes, 0 indicates non-initialized struct
    NvMMLiteEventType event;        //!< Event ID

    NvMMLiteBlockWarning warning;   //!< Warning code
    char additionalInfo[200];   //!< text string with additional information, if any
} NvMMLiteBlockWarningInfo;

/** Endianess constants for NvMMLiteBuffer negotiation. */
typedef enum
{
    NvMMLiteBufferEndianess_LE = 0, //!< Words in little endian format
    NvMMLiteBufferEndianess_BE,     //!< Words in big endian format

    NvMMLiteBufferEndianess_Force32 = 0x7fffffff
} NvMMLiteBufferEndianess;

/**
 * @brief Defines the information concerning a set of new requirements for a
 * stream.
 */
typedef struct NvMMLiteNewBufferRequirementsInfo_
{
    // first 2 members must match NvMMLiteStreamEventBase
    NvU32 structSize;               //!< Size in bytes, 0 indicates non-initialized struct
    NvMMLiteEventType event;            //!< always NvMMLiteEvent_NewBufferRequirements

    NvU32 minBuffers;               //!< Minimum supported number of buffers on stream
    NvU32 maxBuffers;               //!< Maximum supported number of buffers on stream
    NvU32 minBufferSize;            //!< Minimum supported buffer size in bytes
    NvU32 maxBufferSize;            //!< Maximum supported buffer size in bytes
    NvU16 byteAlignment;            //!< Buffer payload start byte alignment requirement in bytes
    NvU8  bPhysicalContiguousMemory;//!< NV_TRUE if physical memory needs to be contiguous
    NvU8  bInSharedMemory;          //!< NV_TRUE if shared memory (valid PhyAddress) is required
    NvMMMemoryType memorySpace;     //!< Required memory type
    NvMMLiteBufferEndianess endianness; //!< Byte endianess requirement
    NvMMLiteBufferFormatId formatId;    //!< identifier to indicate the NvMMLiteBufferFormat used
    NvMMLiteBufferFormat format;        //!< Union to define audio, video, and image formats
} NvMMLiteNewBufferRequirementsInfo;

/**
 * @brief Defines bitstream property associated with JPEG Decoder.
 */
typedef struct {
    // if isThumbnailMarkerDecode =1, thumbnail marker decoded
    // if isThumbnailMarkerDecode =0, Primary Image marker decoded
    NvU8  isThumbnailMarkerDecoded;
    NvU32 scalingFactorsSupported;
    NvU32 MaxWidthSupported;
    NvU32 MaxHeightSupported;
    // Holding the Name of Manufracturer
    NvU8  Make[32];
    // Holding the Name of Model number of digiCam
    NvU8  Model[32];
    // Holding the resolution unit
    NvU8  ResolutionUnit;
    NvU64 XResolution;
    NvU64 YResolution;
    NvU8  bpp;
    // If this flag is set Resolution related info is successfully decoded
    NvU8 isEXIFDecoded;
    // If this flag is set exif marker is present and successfully decoded
    NvU8 isEXIFPresent;
    // Thumbnail Image Height
    NvU32 ThumbnailImageHeight;
    //Thumbnail Image Width
    NvU32 ThumbnailImageWidth;
    // Thumbnail color format
    NvU32 ThumbnailColorFormat;
} NvMMLiteJPEGBitStreamProperty, *NvMMLiteJPEGBitStreamPropertyHandle;

/**
 * @brief Defines the information concerning the video stream attributes
 */
typedef struct NvMMLiteEventNewVideoStreamFormatInfo_{
    NvU32 StructSize;
    NvMMLiteEventType event;

    // Holds profile and level of input bitstream. Refer decoder-specific
    // header for respective values
    NvU32 VideoProfileAndLevel;
    // Holds original width of sequence
    NvU32 Width;
    // Holds original height of sequence
    NvU32 Height;
    //Holds color Format
    NvU32 ColorFormat;
    // Holds surface Layout, Reference: NvRmSurfaceLayout
    NvU32 Layout;
    // Holds Kind value supported by Block linear surfaces
    NvU16 Kind;
    // Holds Height value programmable to block linear surfaces
    NvU16 BlockHeightLog2;
    // Holds the chroma layout is semiplanar/planar
    NvBool bSemiPlanar;
    // Holds bitrate of sequence in kbps
    NvU32 BitRate;
    // The rate at which frames are output from the decoding process
    NvU32 FrameRate;
    // Holds the Number of surfaces required by the codec. This should be send in
    // this variable rather than in minBuffers of the structure NvMMLiteNewBufferRequirementsInfo.
    NvU32 NumOfSurfaces;
    // Holds decoder specific bitstream property structure
    union
    {
        NvMMLiteJPEGBitStreamProperty   JPEGBitStreamProperty;
    } BitStreamProperty;
    // Holds the info regarding whether the stream is progressive or interlaced
    NvBool InterlaceStream;
    // Holds the info regarding whether the stream is MVC (Mutli View Coded)
    NvBool IsMVC;
} NvMMLiteEventNewVideoStreamFormatInfo;

/**
* @brief Defines the information concerning the audio stream format change event
*/
typedef struct NvMMLiteEventNewAudioStreamFormatInfo_{
    NvU32 StructSize;
    NvMMLiteEventType event;
} NvMMLiteEventNewAudioStreamFormatInfo;

/**
 * @brief Defines the information concerning the video stream resolution attributes
 */
typedef struct NvMMLiteEventVideoStreamInitInfo_{
    NvU32           structSize; //!< Size in bytes, 0 indicates non-initialized struct
    NvMMLiteEventType   event;      //!< Event ID

    NvU32 AspectRatioX; // specifies pixel aspect ratio for the case of non square pixel content
    NvU32 AspectRatioY; // specifies pixel aspect ratio for the case of non square pixel content
    NvRect resolutionRect;      //!< video stream resolution struct
} NvMMLiteEventVideoStreamInitInfo;

/**
 * @brief Defines the information concerning the audio stream attributes
 */
typedef struct NvMMLiteEventAudioStreamInitInfo_{
    NvU32           structSize; //!< Size in bytes, 0 indicates non-initialized struct
    NvMMLiteEventType   event;      //!< Event ID

} NvMMLiteEventAudioStreamInitInfo;

/**
 * @brief Defines the information concerning the close of a block.
 */
typedef struct NvMMLiteBlockCloseInfo_{
    NvU32 structSize;
    NvMMLiteEventType event;
    NvBool blockClosed;
} NvMMLiteBlockCloseInfo;

/**
 * @brief Defines the information concerning profiling on the block.
 */
typedef struct NvMMLiteEventProfilingData_{
    NvU32 structSize;
    NvMMLiteEventType event;

    NvU8 InstanceName[16];
    NvU32 FrameType;
    NvU32 Index;
    NvU32 AccumulatedTime;
    NvU32 NoOfTimes;
    NvU32 inputStarvationCount;
}NvMMLiteEventProfilingData;

/**
 * @brief Defines the information concerning the stream end of the block.
 */
typedef struct NvMMLiteEventStreamEndInfo_{
    NvU32 structSize;
    NvMMLiteEventType event;

    /* other fields TBD */
} NvMMLiteEventStreamEndInfo;

/**
 * @brief Defines the information concerning the DRM playcount for committing.
 */
typedef struct NvMMLiteEventDRMPlayCntInfo_{
    NvU32 structSize;
    NvMMLiteEventType event;
    NvU32 BufferID;
    NvU32 coreContext;
    NvU32 drmContext;
    NvBool bDoMetering;
} NvMMLiteEventDRMPlayCntInfo;

/**
 * @brief Defines the information concerning the Marker Hit of the Parser block.
 */
/*FIXME; wrong order? */
typedef struct NvMMLiteEventMarkerHitInfo_{
    NvU32 structSize;
    NvU32 MarkerNumber;
    NvMMLiteEventType event;
    /* other fields TBD */
} NvMMLiteEventMarkerHitInfo;

/**
 * @brief Defines the information concerning the NewMetaDataAvailable of the Parser block.
 */
typedef struct NvMMLiteEventNewMetaDataAvailable_{
    NvU32 structSize;
    NvU32 MarkerNumber;
    NvMMLiteEventType event;
    /* other fields TBD */
} NvMMLiteEventNewMetaDataAvailable;   

typedef struct NvMMLiteEventForBuffering_{
    NvU32 structSize;
    NvMMLiteEventType event;
    NvU32 BufferingPercent;        
    NvBool ChangeStateToPause;
    /* other fields TBD */
} NvMMLiteEventForBuffering;

/**
 * @brief Defines the information concerning the block's stream end.
 */
typedef struct NvMMLiteEventBlockStreamEndInfo_{
    NvU32 structSize;
    NvMMLiteEventType event;
    NvU32 streamIndex;
    /* other fields TBD */
} NvMMLiteEventBlockStreamEndInfo;

/**
 * @brief Defines the information concerning the AbortDone from the block for a stream.
 */
typedef struct NvMMLiteEventBlockAbortDoneInfo_{
    NvU32 structSize;
    NvMMLiteEventType event;
    NvU32 streamIndex;
    /* other fields TBD */
} NvMMLiteEventBlockAbortDoneInfo;

/**
 * @brief Defines the information for the state change of the block.
 */
typedef struct NvMMLiteSetStateCompleteInfo_{
    NvU32 structSize;
    NvMMLiteEventType event;
    NvMMLiteState State;
    /* other fields TBD */
} NvMMLiteSetStateCompleteInfo;

/**
 * @brief Defines the information concerning the close of a block.
 */
typedef struct NvMMLiteEventSetAttributeCompleteInfo_{
    NvU32 structSize;
    NvMMLiteEventType event;
    /* other fields TBD */
} NvMMLiteEventSetAttributeCompleteInfo;

typedef struct NvMMLiteEventTimeStampUpdate_ {
    NvU32 structSize;
    NvMMLiteEventType event;
    NvU64 timeStamp;
    NvU32 index;
} NvMMLiteEventTimeStampUpdate;

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMMLITE_EVENT_H
