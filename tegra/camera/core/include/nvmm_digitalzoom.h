/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file nvmm_digitalzoom.h
 * @brief <b>NVIDIA Driver Development Kit:
 *           NvMM DigitalZoom APIs</b>
 *
 * @b Description: Declares Interface for NvMM DigitalZoom APIs.
 */


#ifndef INCLUDED_NVMM_DIGITALZOOM_H
#define INCLUDED_NVMM_DIGITALZOOM_H

/** @defgroup nvmm_digitalzoom DigitalZoom API
 *
 * NvMM DigitalZoom is the an NvMM block that performs digital zooming
 * It provides the functionality needed for stretching and rotating
 * images using NvDDK 2D.
 *
 * @ingroup nvmm_modules
 * @{
 */

#include "nvfxmath.h"
#include "nvmm_event.h"
#include "nvddk_2d_v2.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * DigitalZoom Attribute enumerations.
 * Following enum are used by nvmm_digitalzoom for setting the block's
 * attributes.They are used as 'eAttributeType' variable in SetAttribute()
 * API. These attributes are applicable to the whole DigitalZoom block.
 * @see SetAttribute
 * @ingroup nvmmdigitalzoomblock
 */
typedef enum
{

    NvMMDigitalZoomAttributeType_None = 0x0,
    /**
     * Zoom factor to change to in the period of time
     * set by NvMMDigitalZoomAttributeType_SmoothZoomTime.
     * If NvMMDigitalZoomAttributeType_SmoothZoomTime is 0,
     * zoom factor will be applied to the next frame.
     * If NvMMDigitalZoomAttributeType_SmoothZoomTime is t,
     * a sequence of zoom factors from current zoom factor to the desired
     * zoom factor will be applied in t ms.
     */
    NvMMDigitalZoomAttributeType_ZoomFactor,

    /**
     * Transform applied to preview output.
     * The attribute value should be an NvDdk2dTransform.
     */
    NvMMDigitalZoomAttributeType_PreviewTransform,

    /**
     * Transform applied to still output.
     * The attribute value should be an NvDdk2dTransform.
     */
    NvMMDigitalZoomAttributeType_StillTransform,

    /**
     * Transform applied to video output.
     * The attribute value should be an NvDdk2dTransform.
     */
    NvMMDigitalZoomAttributeType_VideoTransform,

    /**
     * Transform applied to thumbnail output.
     * The attribute value should be an NvDdk2dTransform.
     */
    NvMMDigitalZoomAttributeType_ThumbnailTransform,

    /**
     * Multiplier to multiply the zoom factor after processing a frame
     */
    NvMMDigitalZoomAttributeType_ZoomFactorMultiplier,

    /**
     * Override typical DZ behavior and force DZ to pass any
     * camera buffer thru on the default output stream.
     * Used for fault isolation and testing.
     */
    NvMMDigitalZoomAttributeType_StillPassthrough,

    /**
     * Scale type for preview output.
     * The attribute value should be an NvMMDigitalZoomScaleType.
     */
    NvMMDigitalZoomAttributeType_PreviewScaleType,

    /**
     * Scale type for still output.
     * The attribute value should be an NvMMDigitalZoomScaleType.
     */
    NvMMDigitalZoomAttributeType_StillScaleType,

    /**
     * Scale type for video output.
     * The attribute value should be an NvMMDigitalZoomScaleType.
     */
    NvMMDigitalZoomAttributeType_VideoScaleType,

    /**
     * Scale type for thumbnail output.
     * The attribute value should be an NvMMDigitalZoomScaleType.
     */
    NvMMDigitalZoomAttributeType_ThumbnailScaleType,

    /**
     * Cropping for preview output
     */
    NvMMDigitalZoomAttributeType_PreviewCrop,
    /**
     * Crop rectangle for preview output
     */
    NvMMDigitalZoomAttributeType_PreviewCropRect,

    /**
     * Cropping for still output
     */
    NvMMDigitalZoomAttributeType_StillCrop,
    /**
     * Crop rectangle for still output
     */
    NvMMDigitalZoomAttributeType_StillCropRect,

    /**
     * Cropping for video output
     */
    NvMMDigitalZoomAttributeType_VideoCrop,
    /**
     * Crop rectangle for video output
     */
    NvMMDigitalZoomAttributeType_VideoCropRect,

    /**
     * Cropping for thumbnail output
     */
    NvMMDigitalZoomAttributeType_ThumbnailCrop,
    /**
     * Crop rectangle for thumbnail output
     */
    NvMMDigitalZoomAttributeType_ThumbnailCropRect,

    /**
     * Enable profiling DigitalZoom block.
     * When this attribute is set to NV_TURE, DigitalZoom block's
     * profiling data will be reset.
     * The attribute value should be an NvBool
     */
    NvMMDigitalZoomAttributeType_Profiling,

    /**
     * Profiling data (read-only)
     * The attribute value should be an NvMMDigitalZoomProfilingData
     */
    NvMMDigitalZoomAttributeType_ProfilingData,

    /**
     * Pin Enable
     * The attribute value should be an NvMMDigitalZoomPin. Use | to specify
     * multiple values like NvMMDigitalZoomPin_Preview | NvMMDigitalZoomPin_Still.
     * This attribute is used as a hint of the output streams that will be
     * tunnelled in DZ. Default is no stream will be tunnelled.
     */
    NvMMDigitalZoomAttributeType_PinEnable,

    /**
     * Smooth Zoom Time (NvU32)
     * When this attribute is 0, zoom factor will jump to the desired zoom
     * factor when NvMMDigitalZoomAttributeType_ZoomFactor is set. When this
     * value is t, a smooth zooming will be performed in t milliseconds when
     * NvMMDigitalZoomAttributeType_ZoomFactor is set.
     * Default is 0.
     */
    NvMMDigitalZoomAttributeType_SmoothZoomTime,

    /**
     * Smooth Zoom Abort (NvBool)
     * Set this attribute to NV_TRUE to abort smooth zooming. Zoom factor will
     * stay at its current zoom factor when DZ receives this SetAttribute.
     * Setting this attribute to NV_FALSE has no effect. This the a write-only
     * attribute. Use NvMMDigitalZoomAttributeType_ZoomFactor to read back
     * where the zoom factor stops at.
     */
    NvMMDigitalZoomAttributeType_SmoothZoomAbort,

    /**
     * User-space preview copy (NvBool)
     * Set this attribute to NV_TRUE to enable a user-space copy of each
     * preview buffer, which will be sent by the event
     * NvMMDigitalZoomEvents_PreviewFrameCopy.
     * Note that this adds an expensive per-frame allocation and copy,
     * and thus is not recommended if performance is a concern.
     * The client is responsible for freeing the buffer contained in
     * the NvMMFrameCopy struct.
     * Default value is NV_FALSE.
     */
    NvMMDigitalZoomAttributeType_PreviewFrameCopy,

    /**
     * User-space still confirmation copy (NvBool)
     * Set this attribute to NV_TRUE to enable a user-space copy of the
     * preview buffer associated with each still capture, which will be
     * sent by the event NvMMDigitalZoomEvents_StillConfirmationFrameCopy.
     * Note that this adds an expensive per-frame allocation and copy,
     * and thus is not recommended if performance is a concern.
     * The client is responsible for freeing the buffer contained in
     * the NvMMFrameCopy struct.
     * Default value is NV_FALSE.
     */
    NvMMDigitalZoomAttributeType_StillConfirmationFrameCopy,

    /**
     * User-space Still copy (NvMMDigitalZoomFrameCopy)
     * Set Size to the desired frame size and Enable to NV_TRUE to enable
     * a user-space copy of the capture buffer associated with each still
     * capture, which will be sent by the event
     * NvMMDigitalZoomEvents_StillFrameCopy.
     * Note that this adds an expensive per-frame allocation and copy,
     * and thus is not recommended if performance is a concern.
     * The client is responsible for freeing the buffer contained in
     * the NvMMFrameCopy struct.
     * It is disabled by default.
     */
    NvMMDigitalZoomAttributeType_StillFrameCopy,

    /**
     * Color format of preview frame copy
     * (NvMMDigitalZoomFrameCopyColorFormat)
     * Default is NV21
     */
    NvMMDigitalZoomAttributeType_PreviewFrameCopyColorFormat,

    /**
     * Color format of still confirmation frame copy
     * (NvMMDigitalZoomFrameCopyColorFormat)
     * Default is NV21
     */
    NvMMDigitalZoomAttributeType_StillConfirmationFrameCopyColorFormat,

    /**
     * Enable for custom postview processing
     * Default is disabled
     */
    NvMMDigitalZoomAttributeType_CustomPostview,

    /**
     * Color format of still frame copy
     * (NvMMDigitalZoomFrameCopyColorFormat)
     * Default is NV21
     */
    NvMMDigitalZoomAttributeType_StillFrameCopyColorFormat,

    /**
     * Operation information in DZ
     * (NvMMDigitalZoomOperationInformation)
     * Read-only attribute.
     * This return the latest operation information in DZ
     * which can be used for controlling camera block.
     * For example, the client can get the preview information
     * to set up the correct focus regions in camera block.
     * All values will be zero if preview has not been enabled.
     */
    NvMMDigitalZoomAttributeType_OperationInformation,

    /**
     * Set upscaling of Zoomed Images
     * Set this prameter to NV_TRUE to enable the feature.
     * Set NV_FALSE to disable the zoom upscaling.
     * Default value is set to NV_TRUE
     */
    NvMMDigitalZoomAttributeType_ZoomUpscale,

    /* When stereo is selected, stereo capture buffer information
     * is required. Must be set before sensor power on and after
     * NvMMCameraAttribute_StereoCameraMode is set.
     *
     * Parameter type: NvMMCameraStereoCaptureInfo
     */
    NvMMDigitalZoomAttribute_StereoCaptureInfo,

    /**
     * Set current camera mode
     */
    NvMMDigitalZoomAttributeType_CameraMode,

    /*
     * Enable the use of an external buffer manager.
     * When using an external manager, the nvmm layer should no longer
     * allocate buffers, but simply communicate buffer configurations
     * to the manager.
     *
     * Read/Write (as almost ALL of these should be)
     * Parameter type: NvBool
     * Default: NV_FALSE
     */
    NvMMDigitalZoomAttributeType_ExternalBufferManager,

    NvMMDigitalZoomAttributeType_Force32 = 0x7FFFFFFF

} NvMMDigitalZoomAttributeType;

/**
 * Defines events that will be sent from codec to client.
 * Client can use these events to take appropriate actions
 * @see SendEvent
 * @ingroup nvmmdigitalzoomblock
 */
typedef enum
{

    /* Preview output is done */
    NvMMDigitalZoomEvents_PreviewDone = NvMMEventDigitalZoom_EventOffset,

    /* Still output is done */
    NvMMDigitalZoomEvents_StillDone,

    /* Preview output is done */
    NvMMDigitalZoomEvents_VideoDone,

    /* Preview dropped a frame */
    NvMMDigitalZoomEvents_PreviewDroppedFrame,

    /* Still dropped a frame */
    NvMMDigitalZoomEvents_StillDroppedFrame,

    /* Video dropped a frame */
    NvMMDigitalZoomEvents_VideoDroppedFrame,

    /* A user-space copy of the preview frame, see NvMMFrameCopy */
    NvMMDigitalZoomEvents_PreviewFrameCopy,

    /* A user-space copy of the still confirmation frame, see NvMMFrameCopy */
    NvMMDigitalZoomEvents_StillConfirmationFrameCopy,

    /* A user-space copy of the Still frame, see NvMMFrameCopy */
    NvMMDigitalZoomEvents_StillFrameCopy,

    /* Notify shim the zoom factor during smooth zoom */
    NvMMDigitalZoomEvents_SmoothZoomFactor,

    /* Notify shim that DZ has finished buffer (re)negotiation on one of its streams */
    NvMMDigitalZoomEvents_StreamBufferNegotiationComplete,

    /* Face detection results */
    NvMMDigitalZoomEvents_FaceInfo,

    /* Inform PreviewPausedAfterStillCapture happened */
    NvMMDigitalZoomEvents_PreviewPausedAfterStillCapture,

    NvMMDigitalZoomEvents_Force32 = 0x7FFFFFFF

} NvMMDigitalZoomEvents;

/**
 * Defines stream index that will used in nvmm digital zoom block
 * @ingroup nvmmdigitalzoomblock
 */
typedef enum
{
    /* index of input Preview Stream */
    NvMMDigitalZoomStreamIndex_InputPreview = 0x0, // index starts from 0

    /* index of input stream */
    NvMMDigitalZoomStreamIndex_Input,

    /* index of output Preview stream */
    NvMMDigitalZoomStreamIndex_OutputPreview,

    /* index of output Still stream */
    NvMMDigitalZoomStreamIndex_OutputStill,

    /* index of output Video stream */
    NvMMDigitalZoomStreamIndex_OutputVideo,

    /* index of output Thumbnail stream */
    NvMMDigitalZoomStreamIndex_OutputThumbnail,

    /* number of supported streams */
    NvMMDigitalZoomStreamIndex_Count,

    NvMMDigitalZoomStreamIndex_Force32 = 0x7FFFFFFF

} NvMMDigitalZoomStreamIndex;


/**
 * NvMMDigitalZoomScaleType enum
 */
typedef enum
{
    /**
     * Don't preserve aspect ratio. Output may be stretched if input does
     * not have the same aspect ratio as output.
     */
    NvMMDigitalZoomScaleType_Normal = 0x1,

    /**
     * Preserve aspect ratio and stretch the whole input image to the center
     * of output surface. Pad black color on output surface outside the
     * stretched input image.
     */
    NvMMDigitalZoomScaleType_PreserveAspectRatio_Pad,

    /**
     * Preserve aspect ratio. Crop input image to fit output surface so
     * the whole output surface will be filled with cropped input image.
     */
    NvMMDigitalZoomScaleType_PreserveAspectRatio_Crop,

    NvMMDigitalZoomScaleType_Force32 = 0x7FFFFFFF
} NvMMDigitalZoomScaleType;


typedef struct NvMMDigitalZoomProfilingDataRec
{
    /**
     * Timestamp of the first preview frame in 100 nanoseconds
     */
    NvU64 PreviewFirstFrame;

    /**
     * Timestamp of the first still frame in 100 nanoseconds
     */
    NvU64 StillFirstFrame;

    /**
     * Timestamp of the first video frame in 100 nanoseconds
     */
    NvU64 VideoFirstFrame;

    /**
     * Timestamp of the last preview frame in 100 nanoseconds
     */
    NvU64 PreviewLastFrame;

    /**
     * Timestamp of the last still frame in 100 nanoseconds
     */
    NvU64 StillLastFrame;

    /**
     * Timestamp of the last video frame in 100 nanoseconds
     */
    NvU64 VideoLastFrame;

    /**
     * Timestamp of the Still Confirmation frame in 100 nanoseconds
     */
    NvU64 StillConfirmationFrame;

    /**
     * Timestamp of the First Preview After Still frame in 100 nanoseconds
     */
    NvU64 FirstPreviewFrameAfterStill;

    /**
     * Frame counts of preview output
     */
    NvU32 PreviewStartFrameCount;
    NvU32 PreviewEndFrameCount;

    /**
     * Frame counts of still output
     */
    NvU32 StillStartFrameCount;
    NvU32 StillEndFrameCount;

    /**
     * Frame counts of video output
     */
    NvU32 VideoStartFrameCount;
    NvU32 VideoEndFrameCount;

    /**
     * Number of times when Digital Zoom block receives a preview output
     * buffer but it has no preview input buffer.
     */
    NvU32 InsufficientInputPreviewBufferCount;

    /**
     * Number of times when Digital Zoom block receives a video or still output
     * buffer but it has no input buffer.
     */
    NvU32 InsufficientInputBufferCount;

    /**
     * Number of times when Digital Zoom block receives a preview input
     * buffer but it has no preview output buffer.
     */
    NvU32 InsufficientOutputPreviewBufferCount;

    /**
     * Number of times when Digital Zoom block receives an input buffer but
     * it has no still output buffer.
     */
    NvU32 InsufficientOutputStillBufferCount;

    /**
     * Number of times when Digital Zoom block receives an input buffer but
     * it has no video output buffer.
     */
    NvU32 InsufficientOutputVideoBufferCount;

    /**
     * Number of times when Digital Zoom block receives an input buffer but
     * it has no thumbnail output buffer.
     */
    NvU32 InsufficientOutputThumbnailBufferCount;

} NvMMDigitalZoomProfilingData;

/**
 * NvMMDigitalZoomPin enum
 */
typedef enum
{
    /* Preview pin */
    NvMMDigitalZoomPin_Preview      = 1,

    /* Still pin */
    NvMMDigitalZoomPin_Still        = 1 << 1,

    /* Video pin */
    NvMMDigitalZoomPin_Video        = 1 << 2,

    /* Thumbnail pin */
    NvMMDigitalZoomPin_Thumbnail    = 1 << 3,

    NvMMDigitalZoomPin_Force32      = 0x7FFFFFFF

}NvMMDigitalZoomPin;



/**
 * NvMMDigitalZoomThumbnail enum
 */
typedef enum
{
    /* no thumbnail would be generated in DZ */
    NvMMDigitalZoomThumbnail_None = 0x1,

    /* DZ generates a thumbnail of lower quality for each still frame */
    NvMMDigitalZoomThumbnail_QualityFaster,

    /* DZ generates a thumbnail of higher quality for each still frame */
    NvMMDigitalZoomThumbnail_QualityBetter,

    /* DZ generates a thumbnail of lower quality for the last still frame */
    NvMMDigitalZoomThumbnail_QualityFasterLastFrameOnly,

    /* DZ generates a thumbnail of higher quality for the last still frame */
    NvMMDigitalZoomThumbnail_QualityBetterLastFrameOnly,

    NvMMDigitalZoomThumbnail_Force32 = 0x7FFFFFFF

}NvMMDigitalZoomThumbnail;


/**
 * Data type for NvMMDigitalZoomAttributeType_StillYUVFrameCopy
 */
typedef struct NvMMDigitalZoomFrameCopyRec
{
    NvBool Enable;
    NvU32 Width;
    NvU32 Height;
} NvMMDigitalZoomFrameCopy;

typedef struct NvMMDigitalZoomFrameCopyEnableRec
{
    NvBool Enable;
    NvU32 skipCount; //Do preview framecopy on alternate frames
} NvMMDigitalZoomFrameCopyEnable;

typedef struct NvMMDigitalZoomFrameCopyColorFormatRec
{
    /**
     * Number of the surfaces and color format of each surface
     * Currently the frame copy supports NV21 and YV12
     * For NV21,
     *   NumSurfaces = 2
     *   SurfaceColorFormat[0] = NvColorFormat_Y8
     *   SurfaceColorFormat[1] = NvColorFormat_V8_U8
     * For YV12,
     *   NumSurfaces = 3
     *   SurfaceColorFormat[0] = NvColorFormat_Y8
     *   SurfaceColorFormat[1] = NvColorFormat_V8
     *   SurfaceColorFormat[2] = NvColorFormat_U8
     */
    NvU32 NumSurfaces;
    NvColorFormat SurfaceColorFormat[NVMMSURFACEDESCRIPTOR_MAX_SURFACES];
} NvMMDigitalZoomFrameCopyColorFormat;

/**
* Event definition for NvMMDigitalZoomEvents_SmoothZoomFactor
*/
typedef struct NvMMEventSmoothZoomFactorInfo_
{
    NvU32           structSize; //!< Size in bytes, 0 indicates non-initialized struct
    NvMMEventType   event;      //!< Event ID

    NvSFx           ZoomFactor;
    NvBool          SmoothZoomAchieved;
    NvBool          IsSmoothZoom;

} NvMMEventSmoothZoomFactorInfo;

/**
 * NvMMDigitalZoomResetBufferNegotiation is used with
 * NvMMDigitalZoomExtension_ResetBufferNegotiation.
 * The buffer negotiation will be reset for the specified stream.
 */
typedef struct NvMMDigitalZoomResetBufferNegotiationRec
{
    NvU32 StreamIndex;

} NvMMDigitalZoomResetBufferNegotiation;


/**
 * NvMMDigitalZoomOperationInformation is used with
 * NvMMDigitalZoomAttributeType_OperationInformation.
 */
typedef struct NvMMDigitalZoomOperationInformationRec
{
    // Size of the preview input buffers
    NvSize PreviewInputSize;

    // Final preview input crop rectangle applied
    // to the input buffer. This is rounded to pixels.
    NvRect PreviewInputCropRect;

    // Final preview transform
    NvDdk2dTransform PreviewTransform;
} NvMMDigitalZoomOperationInformation;

/**
 *
 */
typedef enum
{
    /* Use NvMMDigitalZoomResetBufferNegotiation with this extension.
     * With this extension, the buffers allocated for this stream
     * will be freed and the buffer negotiation for the stream will be reset.
     * This has to be called before buffer renegotiation.
     */
    NvMMDigitalZoomExtension_ResetBufferNegotiation = 0x1,

    NvMMDigitalZoomExtension_Force32 = 0x7FFFFFFF,

} NvMMDigitalZoomExtension;

#define NVMM_DZ_UNASSIGNED_BUFFER_ID    (0xFFFFFFFF)

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_DIGITALZOOM_H

