/*
* Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
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

#ifndef INCLUDED_NVMM_EVENT_H
#define INCLUDED_NVMM_EVENT_H

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
#include "nvmm.h"

#if defined(__cplusplus)
extern "C"
{
#endif

    /**
    * @brief NvMM event enumerations.
    *
    */
    typedef enum {
        NvMMEvent_Unknown = 0,
        NvMMEvent_BlockError,             /* uses NvMMBlockErrorInfo */
        NvMMEvent_StreamError,            /* uses NvMMStreamErrorInfo */
        NvMMEvent_SetAttributeError,      /* uses NvMMSetAttributeErrorInfo */
        NvMMEvent_BlockWarning,           /* uses NvMMBlockWarningInfo */
        NvMMEvent_NewBufferRequirements,  /* uses NvMMNewBufferRequirementsInfo */
        NvMMEvent_NewBufferConfiguration, /* uses NvMMNewBufferConfigurationInfo */
        NvMMEvent_NewBufferConfigurationReply,/* uses NvMMNewBufferConfigurationReplyInfo */
        NvMMEvent_BufferNegotiationFailed,/* uses NvMMBufferNegotiationFailedInfo */
        NvMMEvent_NewStreamFormat,        /* uses ?? (TBD: formats differ b/w and
                                          * within audio/video/image)
                                          */
        NvMMEvent_StreamShutdown,         /* uses NvMMStreamShutdownInfo */
        NvMMEvent_BlockClose,             /* uses NvMMBlockCloseInfo */
        NvMMEvent_StreamEnd,              /* uses NvMMEventStreamEndInfo */
        NvMMEvent_BlockAbortDone,         /* uses NvMMEventBlockAbortDoneInfo */
        NvMMEvent_SetStateComplete,       /* uses NvMMSetStateCompleteInfo */
        NvMMEvent_SetAttributeComplete,   /* uses NvMMEventSetAttributeCompleteInfo */
        NvMMEvent_ProfilingData,          /* uses NvMMEventProfilingData */
        NvMMEvent_TimeStampUpdate,        /* uses NvMMEventTimeStampUpdate */
        NvMMEvent_BlockStreamEnd,         /* uses NvMMEventBlockStreamEndInfo */
        NvMMEvent_ResetStreamEnd,         /* Uses NvMMEventStreamEndInfo */
        NvMMEvent_MarkerHit,              /* Uses NvMMEventMarkerHitInfo */
        NvMMEvent_VideoStreamInit,        /* Uses NvMMEventVideoStreamInitInfo */
        NvMMEvent_AudioStreamInit,        /* Uses NvMMEventAudioStreamInitInfo */
        NvMMEvent_VideoFastUpdate,        /* Doesn't use any structrue for time being */
        NvMMEvent_DRMPlayCount,           /* Uses NvMMEventDRMPlayCntInfo */
        NvMMEvent_TracklistError,         /* Uses NvMMEventTrackListErrorInfo */
        NvMMEvent_SetBlockLocale,         /*Uses NvMMEvent_SetBlockLocaleInfo*/
        NvMMEvent_BlockBufferNegotiationComplete,   /*Uses NvMMEventBlockBufferNegotiationCompleteInfo*/
        NvMMEvent_NewMetaDataAvailable,   /*Uses NvMMEventNewMetaDataAvailable*/
        NvMMEvent_ForBuffering,           /*Uses NvMMEventForBuffering*/
        NvMMEvent_BufferAllocationFailed, /*Uses NvMMEventBufferAllocationFailedInfo*/

        /* Events specific to video, image, audio, etc. If included, an offset for
        * that block will be defined here, and then the events will be defined in
        * separate headers, starting with this offset.  By convention, the offsets
        * will use the top 16 bits.
        */
        NvMMEventVideoEnc_EventOffset       =   0x10000,    /* videoenc events defined in nvmm_videoenc.h */
        NvMMEventCamera_EventOffset         =   0x20000,    /* camera events defined in nvmm_camera.h */
        NvMMEventDigitalZoom_EventOffset    =   0x30000,    /* digitalZoom events defined in nvmm_digitalzoom.h */

        NvMMEvent_Force32 = 0x7fffffff
    }NvMMEventType;


   typedef enum {
        NvMMErrorDomain_Invalid = 0,
        NvMMErrorDomain_DRM,                /* See NvMMErrorDomainDRMInfo */
        NvMMErrorDomain_Parser,
        NvMMErrorDomain_ContentPipe,
        NvMMErrorDomain_TrackList,
        NvMMErrorDomain_Other
    } NvMMErrorEventFromDomain;


    typedef struct NvMMErrorDomainDRMInfo_
    {
        NvU32 error;                //!< Error code
        NvU32 TrackIndex;           //!< TrackIndex

    } NvMMErrorDomainDRMInfo;


    /**
    * @brief NvMM Buffer Format identifier
    */
    typedef enum {
        NvMMBufferFormatId_Unknown = 0,
        NvMMBufferFormatId_Audio,
        NvMMBufferFormatId_Video,

        NvMMBufferFormatId_Force32 = 0x7fffffff
    } NvMMBufferFormatId;


    /**
    * @brief Audio Format
    */
    typedef enum {
        NvMMAudioFormat_Unknown = 0,
        NvMMAudioFormat_8Bit_PCM_Mono,
        NvMMAudioFormat_8Bit_PCM_Stereo,
        NvMMAudioFormat_16Bit_PCM_Mono,
        NvMMAudioFormat_16Bit_PCM_Stereo,
        NvMMAudioFormat_24Bit_PCM_Mono,             /*24 bit data in 32bit container left justified */
        NvMMAudioFormat_24Bit_PCM_Stereo,           /*24 bit data in 32bit container left justified */
        NvMMAudioFormat_32Bit_PCM_Mono,
        NvMMAudioFormat_32Bit_PCM_Stereo,
        NvMMAudioFormat_32BitFloat_PCM_Mono,
        NvMMAudioFormat_32BitFloat_PCM_Stereo,
        NvMMAudioFormat_MP3,
        NvMMAudioFormat_AAC,
        NvMMAudioFormat_WMA,
        NvMMAudioFormat_AMRNB,
        NvMMAudioFormat_AMRNB_IF1,
        NvMMAudioFormat_AMRNB_IF2,
        NvMMAudioFormat_AMRWB,
        NvMMAudioFormat_REAL8,
        NvMMAudioFormat_MIDI,
        NvMMAudioFormat_XMF,
        NvMMAudioFormat_OGG,

        NvMMAudioFormat_Force32 = 0x7fffffff
    } NvMMAudioFormat;

    typedef enum
    {
        NvMMPlanar_Y = 0,
        NvMMPlanar_U = 1,
        NvMMPlanar_V = 2,
        NvMMPlanar_UV = 1,
        NvMMPlanar_Single = 0,

        NvMMPlanar_Force32 = 0x7fffffff
    } NvMMPlanar;

    /**
    * @brief Types of block warnings
    */
    typedef enum {
        NvMMBlockWarning_InsufficientMemoryForImageStabilization = 1,
        NvMMBlockWarning_InvalidLensShadingCalibration,
        NvMMBlockWarning_SuspiciousLensShadingCalibration,
        NvMMBlockWarning_DelayedSetAttributeFailed,
        NvMMBlockWarning_InvalidOperation,

        //etc

        NvMMBlockWarning_Force32 = 0x7fffffff
    } NvMMBlockWarning;

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
    * V. (See NvMMPlanar enum definitions)
    * For conventional RGB and packed YUV surfaces, only the first surface
    * description is used.
    * SurfaceDescriptions, these are NvRmSurfaces whose mem handles, offsets,
    * and pBase ptrs are nulled and not used.  The pitch, tiling, dimensions,
    * format are used to communicate video formats between nvmm blocks.
    * @see NvMMPlanar
    */
    typedef struct {
        NvRmSurface SurfaceDescription[NVMMSURFACEDESCRIPTOR_MAX_SURFACES];
        NvU32 NumberOfSurfaces;
        NvU32 SurfaceRestriction;
        NvBool bImage;
        //NvSamplePhase PhaseH;  // add when they have been defined
        //NvSamplePhase PhaseV;
    } NvMMVideoFormat;

    /**
    * @brief Buffer Format
    * The NvMM buffer format can be one of three things,
    * Audio (used by the audio blocks), Video (used by the image and
    * video blocks), or raw data.
    * In the case of raw data, the NvMMBufferFormat is not referenced,
    * so no provision has been made for it.
    * There is no enum to indicate whether to use NvMMBufferFormat.videoFormat
    * or NvMMBufferFormat.audioFormat, as it should be obvious, based off
    * the type of block you are.  (If this assumption is incorrect, we can
    * add an identifier.)
    */
    typedef union {
        NvMMVideoFormat videoFormat;
        NvMMAudioFormat audioFormat;
    } NvMMBufferFormat;

    enum {
        NvMMError_BufferConfigurationRejected = 1,
        NvMMError_FatalError,
        /* TBD: other errors */

        NvMMError_Force32 = 0x7fffffff
    };

    /**
    *  The base stream event definition. All event info structure definitions must at least
    * have these fields and definitions and in the same order.
    */
    typedef struct NvMMStreamEventBaseInfo
    {
        NvU32           structSize; //!< Size in bytes, 0 indicates non-initialized struct
        NvMMEventType   event;      //!< Event ID

    } NvMMStreamEventBase;

    /**
    * @brief Defines the NvMMEvent_BlockError information concerning an error occurring in the block
    */
    typedef struct NvMMBlockErrorInfo_
    {
        // first 2 members must match NvMMStreamEventBase
        NvU32 structSize;           //!< Size in bytes, 0 indicates non-initialized struct
        NvMMEventType event;        //!< Event ID
        NvMMErrorEventFromDomain  domainName;
        NvU32 error;                //!< Error code
        union
        {
             NvMMErrorDomainDRMInfo drmInfo;
        } domainErrorInfo;      //!<domain specific error info.
    } NvMMBlockErrorInfo;

    /**
    * @brief Defines the NvMMEvent_StreamError information concerning an error occurring in a stream.
    */
    typedef struct NvMMStreamErrorInfo_
    {
        // first 2 members must match NvMMStreamEventBase
        NvU32 structSize;           //!< Size in bytes, 0 indicates non-initialized struct
        NvMMEventType event;        //!< Event ID

        NvError error;              //!< Error code

    } NvMMStreamErrorInfo;

    /**
    * brief Defines the NvMMEvent_SetAttributeError information concerning a
    * failed call to SetAttribute
    */
    typedef struct NvMMSetAttributeErrorInfo_
    {
        // first 2 members must match NvMMStreamEventBase
        NvU32 structSize;           //!< Size in bytes, 0 indicates non-initialized struct
        NvMMEventType event;        //!< Event ID

        NvU32 AttributeType;        //!< AttributeType passed in to failed call
        NvU32 AttributeSize;        //!< AttributeSize passed in to failed call
        NvError error;              //!< Error code generated by failed call
    } NvMMSetAttributeErrorInfo;

    /**
    * @brief Defines the NvMMEvent_BlockWarning information concerning a
    * non-fatal error occurring in the block
    */
    typedef struct NvMMBlockWarningInfo_
    {
        // first 2 members must match NvMMStreamEventBase
        NvU32 structSize;           //!< Size in bytes, 0 indicates non-initialized struct
        NvMMEventType event;        //!< Event ID

        NvMMBlockWarning warning;   //!< Warning code
        char additionalInfo[200];   //!< text string with additional information, if any

    } NvMMBlockWarningInfo;

    /**
    * @brief Defines the NvMMEvent_BufferNegotiationFailed information.
    */
    typedef struct NvMMBufferNegotiationFailedInfo_
    {
        // first 2 members must match NvMMStreamEventBase
        NvU32 structSize;           //!< Size in bytes, 0 indicates non-initialized struct
        NvMMEventType event;        //!< always NvMMEvent_NewBufferRequirements

        NvU32 StreamIndex;          //!< Stream index the negotiation failed on

    } NvMMBufferNegotiationFailedInfo;



    /** Endianess constants for NvMMBuffer negotiation. */
    typedef enum
    {
        NvMMBufferEndianess_LE = 0, //!< Words in little endian format
        NvMMBufferEndianess_BE,     //!< Words in big endian format

        NvMMBufferEndianess_Force32 = 0x7fffffff
    } NvMMBufferEndianess;

    /**
    * @brief Defines the information concerning a set of new requirements for a
    * stream.
    */
    typedef struct NvMMNewBufferRequirementsInfo_
    {
        // first 2 members must match NvMMStreamEventBase
        NvU32 structSize;               //!< Size in bytes, 0 indicates non-initialized struct
        NvMMEventType event;            //!< always NvMMEvent_NewBufferRequirements

        NvU32 minBuffers;               //!< Minimum supported number of buffers on stream
        NvU32 maxBuffers;               //!< Maximum supported number of buffers on stream
        NvU32 minBufferSize;            //!< Minimum supported buffer size in bytes
        NvU32 maxBufferSize;            //!< Maximum supported buffer size in bytes
        NvU16 byteAlignment;            //!< Buffer payload start byte alignment requirement in bytes
        NvU8  bPhysicalContiguousMemory;//!< NV_TRUE if physical memory needs to be contiguous
        NvU8  bInSharedMemory;          //!< NV_TRUE if shared memory (valid PhyAddress) is required
        NvMMMemoryType memorySpace;     //!< Required memory type
        NvMMBufferEndianess endianness; //!< Byte endianess requirement
        NvMMBufferFormatId formatId;    //!< identifier to indicate the NvMMBufferFormat used
        NvMMBufferFormat format;        //!< Union to define audio, video, and image formats

    } NvMMNewBufferRequirementsInfo;

    /**
    * @brief Defines the information concerning a new configuration of the stream
    * (sent by buffer allocator to non-allocator).
    */
    typedef struct NvMMNewBufferConfigurationInfo_
    {
        // first 2 members must match NvMMStreamEventBase
        NvU32 structSize;               //!< Size in bytes
        NvMMEventType event;            //!< always NvMMEvent_NewBufferConfiguration

        NvU32 numBuffers;               //!< Number of buffers
        NvU32 bufferSize;               //!< Buffer size in bytes
        NvU16 byteAlignment;            //!< Buffer start alignment requirement in bytes
        NvU8  bPhysicalContiguousMemory;//!< NV_TRUE if physical memory is contiguous
        NvU8  bInSharedMemory;          //!< NV_TRUE if shared memory (valid PhyAddress) is used
        NvMMMemoryType memorySpace;     //!< Memory type
        NvMMBufferEndianess endianness; //!< Byte endianess
        NvMMBufferFormatId formatId;    //!< identifier to indicate the NvMMBufferFormat used
        NvMMBufferFormat format;        //!< Union to define audio, video, and image formats

    } NvMMNewBufferConfigurationInfo;

    /**
    * @brief Defines the information concerning the acceptance or rejection of
    *        a new configuration of the stream (sent by buffer non-allocator to
    *        allocator).
    */
    typedef struct NvMMNewBufferConfigurationReplyInfo_
    {
        // first 2 members must match NvMMStreamEventBase
        NvU32 structSize;               //!< Size in bytes
        NvMMEventType event;            //!< Event ID

        NvBool bAccepted;               //!< NV_TRUE if buffer configuration was accepted, NV_FALSE otherwise

    } NvMMNewBufferConfigurationReplyInfo;

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

    } NvMMJPEGBitStreamProperty, *NvMMJPEGBitStreamPropertyHandle;


    /**
    * @brief Defines the information concerning the video stream attributes
    */
    typedef struct NvMMEventNewVideoStreamFormatInfo_{
        NvU32 StructSize;
        NvMMEventType event;
        // Holds profile and level of input bitstream. Refer decoder-specific
        // header for respective values
        NvU32 VideoProfileAndLevel;
        // Holds original width of sequence
        NvU32 Width;
        // Holds original height of sequence
        NvU32 Height;
        //Holds color Format
        NvU32 ColorFormat;
        // Holds surface Layout
        NvU32 Layout;
        // Holds bitrate of sequence in kbps
        NvU32 BitRate;
        // The rate at which frames are output from the decoding process
        NvU32 FrameRate;
        // Holds the Number of surfaces required by the codec. This should be send in
        // this variable rather than in minBuffers of the structure NvMMNewBufferRequirementsInfo.
        NvU32 NumOfSurfaces;
        // Holds decoder specific bitstream property structure
        union
        {
            NvMMJPEGBitStreamProperty   JPEGBitStreamProperty;
        } BitStreamProperty;
        // Holds the info rgding whether the stream is progressive or interlaced
        NvBool InterlaceStream;
    } NvMMEventNewVideoStreamFormatInfo;

    /**
    * @brief Defines the information concerning the audio stream format change event
    */
    typedef struct NvMMEventNewAudioStreamFormatInfo_{
        NvU32 StructSize;
        NvMMEventType event;
    } NvMMEventNewAudioStreamFormatInfo;

    /**
    * @brief Defines the information concerning the video stream resolution attributes
    */
    typedef struct NvMMEventVideoStreamInitInfo_{
        NvU32           structSize; //!< Size in bytes, 0 indicates non-initialized struct
        NvMMEventType   event;      //!< Event ID
        NvU32 AspectRatioX; // specifies pixel aspect ratio for the case of non square pixel content
        NvU32 AspectRatioY; // specifies pixel aspect ratio for the case of non square pixel content
        NvRect resolutionRect;      //!< video stream resolution struct
    } NvMMEventVideoStreamInitInfo;


    /**
    * @brief Defines the information concerning the video stream resolution attributes
    */
    typedef struct NvMMEventAudioStreamInitInfo_{
        NvU32           structSize; //!< Size in bytes, 0 indicates non-initialized struct
        NvMMEventType   event;      //!< Event ID

    } NvMMEventAudioStreamInitInfo;

    /**
    * @brief Defines the information concerning the shutdown of a stream.
    */
    typedef struct NvMMStreamShutdownInfo_{
        NvU32 structSize;
        NvMMEventType event;
        NvU32 streamIndex;
        /* other fields TBD */
    } NvMMStreamShutdownInfo;

    /**
    * @brief Defines the information concerning the close of a block.
    */
    typedef struct NvMMBlockCloseInfo_{
        NvU32 structSize;
        NvMMEventType event;
        NvBool blockClosed;
    } NvMMBlockCloseInfo;

    /**
    * @brief Defines the information concerning profiling on the block.
    */

    typedef struct NvMMEventProfilingData_{
        NvU32 structSize;
        NvMMEventType event;
        NvU8 InstanceName[16];
        NvU32 FrameType;
        NvU32 Index;
        NvU32 AccumulatedTime;
        NvU32 NoOfTimes;
        NvU32 inputStarvationCount;
    }NvMMEventProfilingData;

    /**
    * @brief Defines the information concerning the stream end of the block.
    */
    typedef struct NvMMEventStreamEndInfo_{
        NvU32 structSize;
        NvMMEventType event;
        /* other fields TBD */
    } NvMMEventStreamEndInfo;

    /**
    * @brief Defines the information concerning the DRM playcount for committing.
    */
    typedef struct NvMMEventDRMPlayCntInfo_{
        NvU32 structSize;
        NvMMEventType event;
        NvU32 BufferID;
        NvU32 coreContext;
        NvU32 drmContext;
        NvBool bDoMetering;
    } NvMMEventDRMPlayCntInfo;

    /**
    * @brief Defines the information concerning the Marker Hit of the Parser block.
    */
    typedef struct NvMMEventMarkerHitInfo_{
        NvU32 structSize;
        NvU32 MarkerNumber;
        NvMMEventType event;
        /* other fields TBD */
    } NvMMEventMarkerHitInfo;

    /**
    * @brief Defines the information concerning the NewMetaDataAvailable of the Parser block.
    */
    typedef struct NvMMEventNewMetaDataAvailable_{
        NvU32 structSize;
        NvU32 MarkerNumber;
        NvMMEventType event;
        /* other fields TBD */
    } NvMMEventNewMetaDataAvailable;

    typedef struct NvMMEventForBuffering_{
        NvU32 structSize;
        NvMMEventType event;
        NvU32 BufferingPercent;
        NvBool ChangeStateToPause;
        /* other fields TBD */
    } NvMMEventForBuffering;

    /**
    * @brief Defines the information concerning the track end of a block.
    */
    typedef struct NvMMEventBlockTrackEndInfo_{
        NvU32 structSize;
        NvMMEventType event;
        NvU32 streamIndex;
        NvU32 clientPrivateData;
        NvBool bStaleEOT;
        /* other fields TBD */
    } NvMMEventBlockTrackEndInfo;

    /**
    * @brief Defines the information concerning the block's stream end.
    */
    typedef struct NvMMEventBlockStreamEndInfo_{
        NvU32 structSize;
        NvMMEventType event;
        NvU32 streamIndex;
        /* other fields TBD */
    } NvMMEventBlockStreamEndInfo;

    /**
    * @brief Defines the information concerning the AbortDone from the block for a stream.
    */
    typedef struct NvMMEventBlockAbortDoneInfo_{
        NvU32 structSize;
        NvMMEventType event;
        NvU32 streamIndex;
        /* other fields TBD */
    } NvMMEventBlockAbortDoneInfo;

    /**
    * @brief Defines the information for the state change of the block.
    */
    typedef struct NvMMSetStateCompleteInfo_{
        NvU32 structSize;
        NvMMEventType event;
        NvMMState State;
        /* other fields TBD */
    } NvMMSetStateCompleteInfo;

    /**
    * @brief Defines the information concerning the close of a block.
    */
    typedef struct NvMMEventSetAttributeCompleteInfo_{
        NvU32 structSize;
        NvMMEventType event;
        NvU32 attributeType;
        /* other fields TBD */
    } NvMMEventSetAttributeCompleteInfo;

    typedef struct NvMMEventTimeStampUpdate_ {
        NvU32 structSize;
        NvMMEventType event;
        NvU64 timeStamp;
        NvU32 index;
    } NvMMEventTimeStampUpdate;

      typedef struct NvMMEvent_SetBlockLocaleInfo_ {
        NvU32 structSize;
        NvMMEventType event;
        NvMMLocale BlockLocale;
      }NvMMEvent_SetBlockLocaleInfo;

      typedef struct NvMMEventBlockBufferNegotiationCompleteInfoRec {
          NvU32 structSize;
          NvMMEventType event;
          NvU32 streamIndex;
          NvMMBlockType eBlockType;
          NvError error;
      }NvMMEventBlockBufferNegotiationCompleteInfo;

    /**
    * @brief Defines the NvMMEvent_TracklistError information.
    */
    typedef struct NvMMEventTracklistErrorInfo_
    {
        // first 2 members must match NvMMStreamEventBase
        NvU32 structSize;           //!< Size in bytes, 0 indicates non-initialized struct
        NvMMEventType event;
        NvMMErrorEventFromDomain  domainName;
        NvU32 trackListIndex;   //Index of the item that failed
        NvError error;              //!< Error code
        NvU32 licenseUrlSize;
        NvU32 licenseChallengeSize;
        NvU16 *licenseUrl;
        NvU8  *licenseChallenge;

    } NvMMEventTracklistErrorInfo;

    /**
    * @brief Defines the information for NvMMEvent_BufferAllocationFailed event
    */
    typedef struct NvMMEventBufferAllocationFailedInfo_{
        NvU32 structSize;
        NvMMEventType event;
        NvU32 streamIndex;
    } NvMMEventBufferAllocationFailedInfo;

    /**
    * Event definition for NvMMEventCamera_RawFrameCopy,
    * NvMMDigitalZoomEvents_PreviewFrameCopy and
    * NvMMDigitalZoomEvents_StillConfirmationFrameCopy.
    * Consists of the base fields common to all events, and a buffer size/pointer.
    * The event handler MUST free this pointer when it is done using it!
    */
    typedef struct NvMMFrameCopyInfo
    {
        NvU32           structSize; //!< Size in bytes, 0 indicates non-initialized struct
        NvMMEventType   event;      //!< Event ID

        NvU32           bufferSize;
        void            *userBuffer;
    } NvMMFrameCopy;

    typedef struct NvMMFaceInfo
    {
        NvU32           structSize; //!< Size in bytes, 0 indicates non-initialized struct
        NvMMEventType   event;      //!< Event ID

        NvS32           bufSize;    //!< buffer size
        NvS32           numOfFaces; //!< Number of detected faces
        void            *faces;     //!< pointer to the face list
    } NvMMFaceInfo;

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_EVENT_H
