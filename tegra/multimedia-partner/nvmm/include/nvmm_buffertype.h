/*
* Copyright (c) 2011-2013, NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

/** @file
* @brief <b>NVIDIA Driver Development Kit:
*           NvDDK Multimedia APIs</b>
*
* @b Description: NvMM Buffer format.
*/

#ifndef INCLUDED_NVMM_BUFFERTYPE_H
#define INCLUDED_NVMM_BUFFERTYPE_H

/**
*  @defgroup nvmm_modules Multimedia Codec APIs
*
*  @ingroup nvmm
* @{
*/

#include "nvcommon.h"
#include "nvos.h"
#include "nvrm_surface.h"
#include "nvrm_channel.h"
#include "nvmm_aes.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @brief Defines the memory from which a buffer is allocated.
 */
typedef enum
{
    NvMMMemoryType_SYSTEM = 1,
    NvMMMemoryType_SDRAM,
    NvMMMemoryType_IRAM,
    NvMMMemoryType_Uncached,
    NvMMMemoryType_WriteBack,
    NvMMMemoryType_WriteCombined,
    NvMMMemoryType_InnerWriteBack,
    NvMMMemoryType_Force32 = 0x7FFFFFFF
} NvMMMemoryType;

/**
 * @brief Defines the payload type of a NvMMBuffer.
 */
typedef enum
{
    /** No payload, NvMMBuffer::Payload is ignored. */
    NvMMPayloadType_None = 0,

    /** Payload is a set of surfaces.
     *  NvMMSurfaceDescriptor describes the payload.
     */
    NvMMPayloadType_SurfaceArray,

    /** Payload is a buffer allocated from the NvRM,
     *  NvMMMemReference describes the payload and refers to a handle
     *  (and associated pointer)
     */
    NvMMPayloadType_MemHandle,

    /** Payload is a buffer allocated from NvOS (the heap),
     *  NvMMMemReference describes the payload and refers to a pointer.
     */
    NvMMPayloadType_MemPointer,

    NvMMPayloadType_Force32 = 0x7FFFFFFF
} NvMMPayloadType;

/**
 * @brief Defines the display resolution when surface needs scaling
 * before display at the renderer.
 */
typedef struct NvMMDisplayResolutionRec
{
    /* Renderer should use this field to find out how the surface
     * (or CropRect, if available) needs to be scaled before display.
     * The ratio of Width/Height is important rather than independent
     * values of Width and Height which may be absurd individually -
     * necessary to provide the correct ratio without truncation.
     * Valid only if non-zero.
     */
    NvU16 Width;
    NvU16 Height;
} NvMMDisplayResolution;

enum { NVMMSURFACEDESCRIPTOR_MAX_SURFACES = 3 };
enum { NVMM_MAX_FENCES = 5 };

typedef struct NvMMSurfaceFencesRec
{
    NvRmFence inFence[NVMM_MAX_FENCES];
    NvU32 numFences;
    NvRmFence outFence;
    NvU32 useOutFence;
} NvMMSurfaceFences;

/**
 * @brief Defines a set of RM surfaces (used as a payload).
 */
typedef struct NvMMSurfaceDescriptorRec
{
    NvRmSurface Surfaces[NVMMSURFACEDESCRIPTOR_MAX_SURFACES];
    /* The crop rectangle for surface in the buffer */
    NvRect CropRect;
    /* Intended Display Resolution */
    NvMMDisplayResolution DispRes;
    /* store physical address of allocated surfaces */
    NvU32 PhysicalAddress[NVMMSURFACEDESCRIPTOR_MAX_SURFACES];
    /* 1 for rgb; 2 for Y, UV; 3 for Y, U, V */
    NvS32 SurfaceCount;
    /* Data mentioning view id in output surface */
    NvU16 ViewId;
    /* indicates if the surfaces are empty */
    NvBool Empty;
    NvMMSurfaceFences *fences;
} NvMMSurfaceDescriptor, *NvMMSurfaceDescriptorHandle;

/**
 * @brief Defines a (non-surface) reference to memory.
 */
typedef struct NvMMMemReferenceRec
{
    /** memory type/space in which the buffer resides. */
    NvMMMemoryType MemoryType;
    /** total size of entire buffer */
    NvU32 sizeOfBufferInBytes;
    /** offset in bytes from the start of the buffer to the first valid byte */
    NvU32 startOfValidData;
    /** size of the valid data from the first to the last valid byte */
    NvU32 sizeOfValidDataInBytes;
    /** The payload type (handle or pointer) identifies appropriate field.
     *  If the payload buffer is backed by an RM memory alloc, the handle of
     *  that memory alloc and the offset where the payload starts within that
     *  alloc. If the surface is not backed by an RM alloc, hMem should be NULL
     *  and Offset is a don't care.
     */
    NvRmMemHandle hMem;
    /** offset in bytes to start of valid data */
    NvU32 Offset;
    /** pointer to the first byte of the buffer */
    void *pMem;

    NvU32 PhyAddress;
} NvMMMemReference;

/**
 * @brief Defines the metadata type of a buffer.
 */
typedef enum
{
    /** No metada associated with this buffer,
     * NvMMPayloadMetadata::BufferMetaData is ignored.
     * Must be 0, so a memset(0) on the NvMMPayloadMetadata is forward
     * compatible.
     */
    NvMBufferMetadataType_None = 0,

    /** Video Encoder buffer Metadata.
     *  NvVideoEncBufferMetadata describes the metadata
     */
    NvMBufferMetadataType_VEnc,

    /** H264 buffer Metadata.
     *  NvH264BufferMetadata describes the metadata
     */
    NvMBufferMetadataType_H264,

    /** Camera buffer Metadata.
     *  NvMMCameraBufferMetadata describes the metadata
     *  NvMMEXIFBufferMetadata describes the exif vallues
     */
    NvMMBufferMetadataType_Camera,
    NvMMBufferMetadataType_EXIFData,

    /** JPEG Encoder buffer Metadata.
     *  NvJpegEncBufferMetadata describes the metadata
     */
    NvMMBufferMetadataType_JPEG,

    /** Deinterlacing block buffer metadata.
     *  NvDeinterlaceBufferMetadata holds the data
     */
    NvMMBufferMetadataType_Deinterlace,

    /** Digital Zoom buffer Metadata
     *  NvMMDigitalZoomBufferMetadata describes the metadata
     */
    NvMMBufferMetadataType_DigitalZoom,

    /** Aes Metadata
     *  NvMMAesMetadata describes the metadata
     */
    NvMMBufferMetadataType_Aes,

    /** Audio Metadata
     */
    NvMMBufferMetadataType_Audio,

    /** Aes Metadata for Encryption
     *  NvMMAesClientMetadata describes the metadata
     */
    NvMMBufferMetadataType_AesWv,

    NvMMBufferMetadataType_Force32 = 0x7FFFFFFF
} NvBufferMetadataType;

/**
 * @brief Defines formats of H264 input buffer data.
 */
typedef enum
{
    /** Format consists of NALU start code followed by NALU data */
    NvH264BufferFormat_ByteStreamFormat = 1,
    /** Format consists of NALU size in NALUSizeFieldWidthInBytes bytes,
     *  followed by NALU data
     */
    NvH264BufferFormat_RTPStreamFormat,
    NvH264BufferFormat_Force32 = 0x7FFFFFFF
} NvH264BufferFormat;

/**
 * @brief Defines metadata associated with H264 input buffer.
 */
typedef struct NvVideoEncBufferMetadataRec
{
    /** Format of input H264 data */
    NvBool KeyFrame;
    /** slice end or frame end in the packet for application to handle packets */
    NvBool EndofFrame;
    /** Average QP index of the encoded frame*/
    NvU16  AvgQP;
} NvVideoEncBufferMetadata;

/**
 * @brief Defines Markerdata associated with Asf input buffer
 */
typedef struct NvAsfBufferMetadataRec
{
    NvU32 MarkerNumber;
} NvAsfBufferMetadata;

/**
 * @brief Defines metadata associated with H264 input buffer.
 */
typedef struct NvH264BufferMetadataRec
{
    /** Format of input H264 data */
    NvH264BufferFormat BufferFormat;
    /* Number of bytes used to specify NALU size. It could be 12, 2 or 4 */
    NvU32 NALUSizeFieldWidthInBytes;
} NvH264BufferMetadata, *NvH264BufferMetadataHandle;

typedef struct NvMMEXIFBufferMetadataRec
{
    // Since the EXIF Info exceeds the message size
    // we write it out to shared memory
    NvRmMemHandle EXIFInfoMemHandle;
    NvRmMemHandle MakerNoteExtensionHandle;
    NvMMSurfaceDescriptor *pThumbnailSurfaceDesc;
    NvU32 EXIFInfoMemHandleId; //this is needed to share the Exif Info with AVP
} NvMMEXIFBufferMetadata;

typedef struct NvJpegEncBufferMetadataRec
{
    /*End of frame data*/
    NvBool bFrameEnd;
} NvJpegEncBufferMetadata;

/**
 * @brief Defines metadata associated with Camera output buffer.
 */

enum { NVMM_CAMERA_BUFFER_METADATA_OUTPUT_PREVIEW             = 1 << 0 };
enum { NVMM_CAMERA_BUFFER_METADATA_OUTPUT_STILL               = 1 << 1 };
enum { NVMM_CAMERA_BUFFER_METADATA_OUTPUT_VIDEO               = 1 << 2 };
enum { NVMM_CAMERA_BUFFER_METADATA_OUTPUT_NSL                 = 1 << 3 };
enum { NVMM_CAMERA_BUFFER_METADATA_OUTPUT_THUMBNAIL           = 1 << 4 };

enum { NVMM_CAMERA_BUFFER_METADATA_OUTPUT_NEEDS_RECIRCULATION = 1 << 8 };
enum { NVMM_CAMERA_BUFFER_METADATA_OUTPUT_PASSTHROUGH         = 1 << 9 };

/* **** WARNING ****
 * this structure cannot take any more members due to size limitation
 */
typedef struct NvMMCameraBufferMetadataRec
{
    /* The crop rectangle for surface in the buffer */
    NvRect CropRect;

    /** Use NVMM_CAMERA_BUFFER_METADATA_OUTPUT_PREVIEW|STILL|VIDEO to
     *  indicate which output streams this buffer should be processed
     *  and output to. Ignored in preview stream.
     */
    NvU16 Output;

    /* Sensor Orientation */
    NvU16  Orientation;

    /* Sensor Direction
     *  the default expectation is that the sensor faces
     *  away from the user (FALSE).  If it is a back facing
     *  camera, then it is facing toward the user. (TRUE).
     *  DZ will then have to adjust the preview and postview.
     */
    NvBool  DirectionIsToward;

    /* If cropping is enabled */
    NvBool Crop;

    // bracketed burst preview frames, used to correlate
    // preview frames with the still frames that are delivered
    // in response to a bracketed burst request.
    // should always be false on still image stream.  true on preview
    // stream when frames match frames output on the still image stream
    // which are part of bracketed burst, false in all other cases.
    NvBool isBracketedBurstPreview;
    // notify DZ to send the preview pause event after a capture session
    NvBool previewPausedAfterCapture;

    // Flags for supporting postview callback logic
    // For both preview and still stream, isStillPreview is set to true when
    // this preview buffer matches a still image, and isEndOfStillSession is set
    // to true for the last image from a capture session
    NvBool isStillPreview;
    NvBool isEndOfStillSession;

    /* In a stereo frame, to differentiate between left and right eye sensor
     * data, ISP introduces a SEAM. The seam width is configurable and cannot
     * be 0.
     */
    // removing to lower structure size as stereo is not supported
    //NvU16 SeamWidth;

    /* EXIF information is stored in shared memory */
    NvRmMemHandle EXIFInfoMemHandle;
    NvRmMemHandle MakerNoteExtensionHandle;

    /* Face detection */
    NvS32 numOfFaces;
    void  *faces;

    /* Flag indicates that DZ was able to avoid doing a blit for this buffer */
    NvBool SkippedDZ;

} NvMMCameraBufferMetadata, *NvMMCameraBufferMetadataHandle;

typedef struct NvMMDigitalZoomBufferMetadataRec
{
    NvBool KeepFrame;

    // see full description in NvMMCameraBufferMetadata, should be copied
    // over from the camera block's metadata to deliver to a client.
    NvBool isBracketedBurstPreview;

    // Flags for supporting postview callback logic
    // see full description in NvMMCameraBufferMetadata, should be copied
    // over from the camera block's metadata to deliver to a client.
    NvBool isStillPreview;
    NvBool isEndOfStillSession;
} NvMMDigitalZoomBufferMetadata, *NvMMDigitalZoomBufferMetadataHandle;

/** Data for each Macroblock for Deinterlacing */
typedef struct DeintMBDataRec
{
    /* Motion vectors. MV[TOP/BOTTOM][X/Y] (One TOP and one BOTTOM MV
     * per 16x16 block as seen in frame. */
    NvS16   MV[2][2];

    /* FRAME/FIELD */
    NvU8    MotionType;

    /* FRAME/FIELD */
    NvU8    DCTType;

    /* INTRA/MOTION_FWD/MOTION_BWD */
    NvU8    MBType;
} DeintMBData;

/** Metadata for buffers to Deinterlace block */
typedef struct NvDeinterlaceBufferMetadataRec
{
    /* I_TYPE/P_TYPE/B_TYPE */
    NvU8    PicCodingType;

    /* FRAME/FIELD */
    NvU8    PicStruct;

    /* bTopFieldFirst: if (PicStruct == FIELD) - NV_TRUE if TOP field is
     * first */
    NvBool  bTopFieldFirst;

    /* If current frame is progressive frame */
    NvBool  bProgressiveFrame;

    /* Width */
    NvU32   Width;

    /* Height */
    NvU32   Height;

    /* Number of MBs */
    NvU32   MBAmax;

    /* Array of MBData of size MBAmax. Contains data for each MB */
    DeintMBData  *pMB;
} NvDeinterlaceBufferMetadata;

/** Per AES Packet Info */
typedef struct NvMMAesPacketInfoRec
{
    /* IV of IV_Size Bytes */
    NvU8 InitializationVector[16];
    NvU32 BytesOfClearData;
    NvU32 BytesOfEncryptedData;
    NvBool IvValid;
} NvMMAesPacketInfo;

/** Data for AES Info */
typedef struct NvMMAesDataRec
{
    /* Algorith used for encryption */
    NvMMMAesAlgorithmID AlgorithmID;

    NvU8 key[16];

    /* 8 Byte / 16 Byte */
    NvU8 IV_size;
    NvU8 KID;
    NvU16 reserved;     //For alignment

    NvMMAesPacketInfo AesPacketInfo[32];
} NvMMAesData;

/** Metadata for AES Info */
typedef struct NvMMAesMetadataRec
{
    NvH264BufferMetadata        H264BufferMetadata;
    NvU32 metadataCount;
    NvMMAesData *pAesData;
    NvRmMemHandle hMem_AesData;
    NvU32 PhysicalAddress_AesData;

    /* bEncrypted: if (encrypted) - NV_TRUE if encrypted */
    NvBool bEncrypted;
    NvU32 SampleSize;
    /** Number of LL Entries carved out of this buffer
     *  here for the AES - Secure decode case
     * */
    NvU32 numOfLLEntries;
    NvU32 NonAlignedOffset;     // Required only for T30
} NvMMAesMetadata;

typedef struct NvMMAudioMetadataRec
{
    NvU32 nSampleRate;
    NvU32 nChannels;
    NvU32 nBitsPerSample;
} NvMMAudioMetadata;

/**
 * @brief Defines metadata associated with the payload.
 *
 * This metadata must be propagated from an input buffer to a corresponding
 * output buffer.
 */
typedef struct NvMMPayloadMetadataRec
{
    /** presentation timestamp of the data in 100 nanosecond units */
    NvU64 TimeStamp;

    /** flags particular to this data */
    NvU32 BufferFlags;

    /** ID to identify NvMMPayloadMetadata::BufferMetaData type. */
    NvBufferMetadataType BufferMetaDataType;

    /** Block implementation specific metadata. */
    union
    {
        NvH264BufferMetadata        H264BufferMetadata;
        NvMMCameraBufferMetadata    CameraBufferMetadata;
        NvMMEXIFBufferMetadata      EXIFBufferMetadata;
        NvVideoEncBufferMetadata    VideoEncMetadata;
        NvJpegEncBufferMetadata     JpegEncMetadata;
        NvAsfBufferMetadata         AsfMarkerdata;
        NvDeinterlaceBufferMetadata DeinterlaceMetadata;
        NvMMDigitalZoomBufferMetadata DigitalZoomBufferMetadata;
        NvMMAesMetadata             AesMetadata;
        NvMMAudioMetadata           AudioMetadata;
    } BufferMetaData;
} NvMMPayloadMetadata;

/**
 * @brief Define the Stereo Frame Packing Type bitfields
 *
 * These are defined in more details in section D.2.25 of the
 * H.264 Spec, syntax element "frame_packing_arrangement_type"
 * They can have values from 0 to 5 to specify different types
 * of frame packed layout. 0 (Checkerboard), 1 (Column interleave),
 * 2 (Row interleave), 3 (side by side), 4 (top bottom), 5 (temporal
 * frame interleaving)
 */
typedef enum
{
    // [18:16]
    NvMMBufferFlag_Stereo_SEI_FPType_StartPos = 16,
    NvMMBufferFlag_Stereo_SEI_FPType_NumBits = 3,
    NvMMBufferFlag_Stereo_SEI_FPType_Mask = ((1 << NvMMBufferFlag_Stereo_SEI_FPType_NumBits) - 1) <<
                                            NvMMBufferFlag_Stereo_SEI_FPType_StartPos
} NvMMBufferFlag_Stereo_SEI_FPType;

/**
 * @brief Define the Stereo Content Interpretation Type bitfields
 *
 * These are defined in more details in section D.2.25 of the
 * H.264 Spec, syntax element "content_interpretation_type".
 * The possible values are 0 - 2, and they specify how the views
 * are to be represented (when used in conjunction with the frame
 * packing arrangement type above). 0 (undefined), 1 (left view first,
 * then right view), 2 (right view first, then left view)
 */
typedef enum
{
    // [20:19]
    NvMMBufferFlag_Stereo_SEI_ContentType_StartPos = 19,
    NvMMBufferFlag_Stereo_SEI_ContentType_NumBits = 2,
    NvMMBufferFlag_Stereo_SEI_ContentType_Mask = ((1 << NvMMBufferFlag_Stereo_SEI_ContentType_NumBits) - 1) <<
                                                 NvMMBufferFlag_Stereo_SEI_ContentType_StartPos
} NvMMBufferFlag_Stereo_SEI_ContentType;

/**
 * @brief Defines the NvMMBufferFlags bitfield
 */
typedef enum
{
    NvMMBufferFlag_EndOfStream  = 0x1,           //!< When set, buffer is beyound end of stream
    NvMMBufferFlag_HeapBase     = 0x02, //Heap base flag 0010b: bit 1
    NvMMBufferFlag_OffsetList   = 0x04, // Offsetlist flag 0100b: bit 2
    NvMMBufferFlag_EndOfTrack   = 0x08, // End-of-track flag 1000b: bit 3
    NvMMBufferFlag_LastBuffer   = 0x10, // Last Buffer flag 10000b: bit 4
    NvMMBufferFlag_DRMPlayCount   = 0x20, // DRM Metering flag 100000b: bit 5
    NvMMBufferFlag_HeaderInfoFlag   = 0x40, // Header information flag 1000000b: bit 6
    NvMMBufferFlag_FrameOutput   = 0x80, // Frame output  flag 10000000b: bit 7
    NvMMBufferFlag_SeekDone      = 0x100, // Seek Done flag 100000000b: bit 8
    NvMMBufferFlag_Marker        = 0x200, // Marker Flag
    NvMMBufferFlag_Gapless       = 0x400, // Flag to detect silence on last_N_ frames/buffers
    NvMMBufferFlag_CPBuffer     = 0x800, // Flag to detect start of readbuffer from Offsetlists
    NvMMBufferFlag_KeyFrame     = 0x1000, // Flag to indicate the keyframe present in the buffer
    NvMMBufferFlag_PTSCalculationRequired = 0x2000, // Flag to indicate the PTSCalculationRequired at decoder
    NvMMBufferFlag_EnableGapless = 0x4000, // Only Enable silence algo for Audio files
    NvMMBufferFlag_StereoEnable = 0x8000, // Stereo Enable / Disable Flag
    // 0x10000 - 0x40000,  // Used as specified in NvMMBufferFlag_Stereo_SEI_FPType
    // 0x80000 - 0x100000, // Used as specified in NvMMBufferFlag_Stereo_SEI_ContentType
    NvMMBufferFlag_FrameContinued = 0x200000, // When set, more data for current frame is pending
    NvMMBufferFlag_MVC          = 0x400000, // MVC encoding
    NvMMBufferFlag_Stereo_SEI_QuincunxSamplingFlag = 0x800000,
    NvMMBufferFlag_Stereo_SEI_SpatialFlippingFlag = 0x1000000,
    NvMMBufferFlag_Stereo_SEI_Frame0FlippedFlag = 0x2000000,
    NvMMBufferFlag_Stereo_SEI_FieldViewsFlag = 0x4000000,
    NvMMBufferFlag_Stereo_SEI_CurrentFrameIsFrame0 = 0x8000000,
    NvMMBufferFlag_SkipFrame  = 0x10000000, // Current frame is same as previous frame
    NvMMBufferFlag_MemMapError  = 0x40000000,
    NvMMBufferFlag_None         = 0x40000001
} NvMMBufferFlags;

/**
 * @brief Defines values for different stereo types.
 *
 * These values fit into bits reserved in NvMMBufferFlags enum.
 * If needed, they can be placed into that enum.
 */
typedef enum
{
     // Supported Stereo combinations, currently same as in nvrm_surface.h
     NvMMBufferFlag_Stereo_None = NV_STEREO_NONE,
     NvMMBufferFlag_Stereo_LeftRight = NV_STEREO_LEFTRIGHT,
     NvMMBufferFlag_Stereo_RightLeft = NV_STEREO_RIGHTLEFT,
     NvMMBufferFlag_Stereo_TopBottom = NV_STEREO_TOPBOTTOM,
     NvMMBufferFlag_Stereo_BottomTop = NV_STEREO_BOTTOMTOP,
     NvMMBufferFlag_Stereo_SeparateLR = NV_STEREO_SEPARATELR,
     NvMMBufferFlag_Stereo_SeparateRL = NV_STEREO_SEPARATERL
} NvMMStereoType;

/**
 * @brief Payload buffer descriptor
 *
 * A mmblock buffer encapsulates units of data used for input or output data
 * and all associated metadata. The mmblock will metabolize input buffers into
 * output buffers and propagate metadata from the input buffers to the output
 * buffers.
 *
 * Max size: 240 bytes
 */
typedef struct NvMMBufferRec
{
    NvU32 StructSize;

    /** unique integer assigned to the buffer by the allocator */
    NvU32   BufferID;

    /** clients context. This will be propagated if pBuffer is set. This
     *  is attached to pBuffer, so they propagate in tandem */
    void *pClientContext;

    NvMMPayloadType PayloadType;
    NvMMPayloadMetadata PayloadInfo;
    union {
        NvMMSurfaceDescriptor Surfaces;
        NvMMMemReference Ref;
    } Payload;

    /* To identify buffer in case of trackList*/
    void *pCore;
} NvMMBuffer;

/**
 * @brief type of buffers for use in transfer buffer functions.
 */
typedef enum
{
    /** Actual data */
    NvMMBufferType_Payload = 1,
    /** Metadata that couldn't be included in the payload info. This should
        not be sent per-frame (to reduce messages). */
    NvMMBufferType_Custom,
    /** A stream event */
    NvMMBufferType_StreamEvent,
    NvMMBufferType_Force32 = 0x7fffffff
} NvMMBufferType;


/**
 * @brief defines the various color formats: Will be replaced by RM provided color formats
 * please do not modify enum values as they are used by vde hw
 */
typedef enum
{
    NvMMVideoDecColorFormat_YUV420 = 0x0,
    NvMMVideoDecColorFormat_YUV422,
    NvMMVideoDecColorFormat_YUV422T,
    NvMMVideoDecColorFormat_YUV444,
    NvMMVideoDecColorFormat_GRAYSCALE,
    NvMMVideoDecColorFormat_YUV420SemiPlanar,
    NvMMVideoDecColorFormat_YUV422SemiPlanar,
    NvMMVideoDecColorFormat_Force32 = 0x7FFFFFFF
} NvMMVideoDecColorFormats;


#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_BUFFERTYPE_H

