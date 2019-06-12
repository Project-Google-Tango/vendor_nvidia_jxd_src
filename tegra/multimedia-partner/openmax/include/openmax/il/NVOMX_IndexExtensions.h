/* Copyright (c) 2007-2014 NVIDIA Corporation.  All rights reserved.
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
 * <b>NVIDIA Tegra: OpenMAX Index Extension Interface</b>
 */

/**
 * @defgroup nv_omx_il_index General Index
 *
 * This is the NVIDIA OpenMAX index extensions interface.
 *
 * These extend custom events and error codes.
 *
 * @ingroup nvomx_general_extension
 * @{
 */

#ifndef _NVOMX_IndexExtensions_h_
#define _NVOMX_IndexExtensions_h_

#include <OMX_Core.h>
#include <OMX_Component.h>

#include "OMX_Core.h"

#include "NVOMX_RendererExtensions.h"
#include "NVOMX_ParserExtensions.h"
#include "NVOMX_CameraExtensions.h"
#include "NVOMX_DecoderExtensions.h"
#include "NVOMX_EncoderExtensions.h"
#include "NVOMX_DrmExtensions.h"
#include "NVOMX_ColorFormatExtensions.h"

struct NvOsSemaphoreRec;

/** Representation of timeout values, in milliseconds. */
typedef OMX_U32 NvxTimeMs;

/** Maximum timeout value (Never timeout). */
#define NVX_TIMEOUT_NEVER   0xffffffff
/** Minimum timeout value. */
#define NVX_TIMEOUT_MIN     0

/** color extension */
typedef enum NVX_COLORFORMATTYPE_ENUM {
    NVX_ColorFormatVendorStartUnused = 0x70000000,

    NVX_ColorFormatYUV422T,
    NVX_ColorFormatYUV444,
    NVX_ColorFormatYV16x2,

    NVX_ColorFormatMax = OMX_COLOR_FormatMax
} NVX_COLORFORMATTYPE;

/* Specifies the type of data pointed to by buffer header's pBuffer */
typedef enum
{
    NVX_BUFFERTYPE_NORMAL = 1,

    /* pBuffer is an NvxEglImageSiblingHandle */
    NVX_BUFFERTYPE_EGLIMAGE,

    /* pBuffer is an android_native_buffer_t */
    NVX_BUFFERTYPE_ANDROID_NATIVE_BUFFER_T,

    /* Below 2 types are required for stagefright playback */
    NVX_BUFFERTYPE_NEEDRMSURFACE,
    NVX_BUFFERTYPE_HASRMSURFACE,

    /*This indicates source component that it can send a NVDIA specific buffer embedded within the OMX Buffer Payload Data */
    NVX_BUFFERTYPE_NEEDNVBUFFER,

    /* pBuffer is an android buffer_handle_t */
    NVX_BUFFERTYPE_ANDROID_BUFFER_HANDLE_T,

    NVX_BUFFERTYPE_MAX = 0x7FFFFFFF
}NvxBufferType;

/* OpenMAX internal data associated with a buffer */
typedef struct NvxBufferPlatformPrivateStruct
{
    /* Specifies the type of data pointed to by buffer header's pBuffer */
    NvxBufferType eType;
    /* Specifies display coordinates */
    OMX_CONFIG_RECTTYPE croprect;

    void *nvmmBuffer;
    OMX_BOOL nvmmBufIsPinned;
    /* Stereo layout info */
    OMX_U32 StereoInfo;
    OMX_U32 rawHeaderOffset;
    void *pData;
} NvxBufferPlatformPrivate;

/** Defines custom event extensions. */
typedef enum NVX_EVENTTYPE_ENUM {
    /** Start of extended OpenMAX camera event types */
    NVX_EventVendorStartUnused = 0x70000000,

    /** Image capture started */
    NVX_EventImageStart = (NVX_EventVendorStartUnused | 0xB00000),
    /** Image EXIF information ready */
    NVX_EventImage_EXIFInfo                    = NVX_EventImageStart + 1,
    NVX_EventImage_JPEGInfo                    = NVX_EventImageStart + 2,

    /** Camera component started */
    NVX_EventCameraStart = (NVX_EventVendorStartUnused | 0xD00000),
    /** Camera AE, AF, AWB locked */
    NVX_EventCamera_AlgorithmsLocked           = NVX_EventCameraStart,
    /** Camera auto focus achieved */
    NVX_EventCamera_AutoFocusAchieved,
    /** Camera auto exposure achieved */
    NVX_EventCamera_AutoExposureAchieved,
    /** Camera auto white balance achieved */
    NVX_EventCamera_AutoWhiteBalanceAchieved,
    /** Camera auto focus timed out */
    NVX_EventCamera_AutoFocusTimedOut,
    /** Camera auto exposure timed out */
    NVX_EventCamera_AutoExposureTimedOut,
    /** Camera auto white balance timed out */
    NVX_EventCamera_AutoWhiteBalanceTimedOut,
    /** Camera capture aborted */
    NVX_EventCamera_CaptureAborted,
    /** Camera capture started */
    NVX_EventCamera_CaptureStarted,
    /** Camera still capture completed */
    NVX_EventCamera_StillCaptureReady,
    /** Camera still capture in process */
    NVX_EventCamera_StillCaptureProcessing,
    /** Copy of camera preview frame */
    NVX_EventCamera_PreviewFrameCopy,
    /** Copy of camera still confirmation frame */
    NVX_EventCamera_StillConfirmationFrameCopy,
    /** Copy of camera Still YUV frame*/
    NVX_EventCamera_StillYUVFrameCopy,
    /** Copy of camera Raw Bayer frame*/
    NVX_EventCamera_RawFrameCopy,
    /** Preview paused after still capture */
    NVX_EventCamera_PreviewPausedAfterStillCapture,
    /** Zoom factor during smooth zoom */
    NVX_EventCamera_SmoothZoomFactor,
    /** Sensor resolution mode changed */
    NVX_EventCamera_SensorModeChanged,
    NVX_EventCamera_EnterLowLight,
    NVX_EventCamera_ExitLowLight,
    NVX_EventCamera_EnterMacroMode,
    NVX_EventCamera_ExitMacroMode,
    NVX_EventCamera_FocusStartMoving,
    NVX_EventCamera_FocusStopped,

    /** Face detection result */
    NVX_EventCamera_FaceInfo,

    /** Start of extended OpenMAX renderer event types */
    NVX_EventRendererStart = (NVX_EventVendorStartUnused | 0xE00000),
    /** First video frame displayed */
    NVX_EventFirstFrameDisplayed,
    /** First audio sample played */
    NVX_EventFirstAudioFramePlayed,

    /** Start of extended OpenMAX other event types */
    NVX_EventOtherStart = (NVX_EventVendorStartUnused | 0xF00000),
    /** NVIDIA multimedia block warning */
    NVX_EventBlockWarning,
    NVX_EventForBuffering,

    NVX_EventDRM_DirectLicenseAcquisition,
    NVX_EventDRM_DrmFailure,
    NVX_StreamChangeEvent,

    NVX_EventCamera_PowerOnComplete,
    /** Limit of extended OpenMAX event types */
    NVX_EventMax = OMX_EventMax,
} NVX_EVENTTYPE;

/** Defines custom error extensions. */
typedef enum
{
    /** Start of extended OpenMAX error types */
    NvxError_ExtendedCodesStart = 0x80004000,

    /** Parser returns DRM license not found for particular track */
    NvxError_ParserDRMLicenseNotFound = 0x80004001,

    /** Parser returns DRM license error */
    NvxError_ParserDRMFailure = 0x80004002,

    /** Parser returns DRM license error */
    NvxError_ParserCorruptedStream = 0x80004003,

    /** Parser returns Seek Unsupported */
    NvxError_ParserSeekUnSupported = 0x80004004,

    /** Parser returns Trickmode Unsupported */
    NvxError_ParserTrickModeUnSupported = 0x80004005,

    /** Writer returns insufficient memory */
    NvxError_WriterInsufficientMemory = 0x80004006,

    /** Writer returns file write failed */
    NvxError_FileWriteFailed = 0x80004007,

    /** Writer returns write failure */
    NvxError_WriterFailure = 0x80004008,

    /** Writer returns unsupported stream */
    NvxError_WriterUnsupportedStream = 0x80004009,

    /** Writer returns unsupported user data */
    NvxError_WriterUnsupportedUserData = 0x8000400A,

    /** Writer returns 2GB limit exceeded */
    NvxError_WriterFileSizeLimitExceeded = 0x8000400B,

    /** Writer returns time limit exceeded */
    NvxError_WriterTimeLimitExceeded = 0x8000400C,

    /** Video Decoder does not need multiple nvmm blocks configuration */
    NvxError_VideoDecNormalConfig = 0x8000400D,

    /** Camera HW is not responding */
    NvxError_CameraHwNotResponding = 0x8000400E,

    /** Limit of extended OpenMAX error types */
    NvxError_Max = 0x7FFFFFFF
} NvxError;

/** Profiling config for internal use only. */
#define NVX_INDEX_CONFIG_PROFILE "OMX.Nvidia.index.config.profile"
/** Holds a profiling information. */
typedef struct NVX_CONFIG_PROFILE
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;

    OMX_BOOL bProfile;
#define PROFILE_FILE_NAME_LENGTH 256
    char     ProfileFileName[PROFILE_FILE_NAME_LENGTH];
    OMX_BOOL bVerbose;
    OMX_BOOL bStubOutput;
    OMX_U32  nForceLocale; // 0 - no, 1 -cpu, 2 - avp
    OMX_U32  nNvMMProfile;
    OMX_BOOL bNoAVSync;
    OMX_BOOL enableUlpMode;
    OMX_U32 ulpkpiMode;
    OMX_S32  nAVSyncOffset;
    OMX_BOOL bFlip;
    OMX_U32  nFrameDrop;

    OMX_BOOL bSanity;
    OMX_U32  nAvgFPS;
    OMX_U32  nTotFrameDrops;
    OMX_BOOL bDisableRendering;

    /// For camera:
    OMX_U64 nTSPreviewStart;
    OMX_U64 nTSCaptureStart;
    OMX_U64 nTSCaptureEnd;
    OMX_U64 nTSPreviewEnd;
    OMX_U64 nTSStillConfirmationFrame;
    OMX_U64 nTSFirstPreviewFrameAfterStill;
    OMX_U32 nPreviewStartFrameCount;
    OMX_U32 nPreviewEndFrameCount;
    OMX_U32 nCaptureStartFrameCount;
    OMX_U32 nCaptureEndFrameCount;
    OMX_S32 xExposureTime;
    OMX_S32 nExposureISO;
    OMX_U32 nBadFrameCount;
} NVX_CONFIG_PROFILE;


#define NVX_INDEX_PARAM_EMBEDRMSURACE \
    "OMX.Nvidia.index.param.embedrmsurface"

/** Config extension index NV-specific (i.e., OEM-specific) buffers within OMX_Buffer header.
 *  OMX extension index to EMBED.
 *  See ::NVX_PARAM_USENVBUFFER
 */
#define NVX_INDEX_CONFIG_USENVBUFFER \
    "OMX.Nvidia.index.config.usenvbuffer"       /**< Reference: OMX_PARAM_BOOLEANTYPE */

/** Config extension index NV-specific (i.e., OEM-specific) buffers and memory FD within OMX_Buffer header.
 * This requires NVX_INDEX_CONFIG_USENVBUFFER to be set as well.
 */
#define NVX_INDEX_CONFIG_USENVBUFFER2 \
    "OMX.Nvidia.index.config.usenvbuffer2"       /**< Reference: OMX_PARAM_BOOLEANTYPE */

/** Indicates the config changed on a port (buffer flag version). */
#define NVX_BUFFERFLAG_CONFIGCHANGED  0x00040000

/** Indicates the omx buffer payload holding buffer fd for vpr case */
#define OMX_BUFFERFLAG_NV_BUFFER2  0x00100000

/** MVC flag.
 * Indicates Multiview Video Codec Encoding
 * @ingroup buf
 */
#define OMX_BUFFERFLAG_MVC 0x01000000

/** Skipped data flag.
 * Indicates buffer contains frame data that needs to be skipped
 * @ingroup buf
 */
#define OMX_BUFFERFLAG_SKIP_FRAME 0x02000000

/** Compressed data flag.
 * Indicates buffer contains compressed data
 * @ingroup buf
 */
#define OMX_BUFFERFLAG_COMPRESSED 0x04000000

/** Timestamp flag.
 * Indicates to retain the OMX Buffer timestamp in Nvmm
 * @ingroup buf
 */
#define OMX_BUFFERFLAG_RETAIN_OMX_TS 0x08000000

/** NVIDIA-specific buffer flag.
  *
  * A component sets OMX_BUFFERFLAG_NV_BUFFER to indicate a NVIDIA (i.e., OEM ) specific Buffer
  * is embedded within the OMX Buffer Payload Data. This Buffer Flag is intended to be used across
  * two NVIDIA openmax components in non-Tunneled mode (e.g., Video Capture on Android Camcorder app).
  * @ingroup buf
  */

#define OMX_BUFFERFLAG_NV_BUFFER 0x10000000

/** End-of-track flag.
 * A component sets EOT when it has reached the end of a track for an
 * output port. The component may continue emitting data on that output
 * port from next track.
 * @ingroup buf
 */
#define OMX_BUFFERFLAG_EOT 0x20000000

/** PTS computation required
 * @ingroup buf
 */
#define OMX_BUFFERFLAG_NEED_PTS 0x40000000

/** Post view flag.
 * Indicates image data is for post view image
 * @ingroup buf
 */
#define OMX_BUFFERFLAG_POSTVIEW 0x80000000


/** Holds data to enable proprietary buffer transfers. */
typedef struct NVX_PARAM_USENVBUFFER
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< OMX specification version info */
    OMX_U32 nPortIndex;             /**< Port that this struct applies to */
    OMX_BOOL bUseNvBuffer;
} NVX_PARAM_USENVBUFFER;

/** Holds data to transfer settings to OMX.Nvidia.odm.comp. */
typedef struct NVX_CONFIG_ODM
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< OMX specification version info */
    OMX_U32 nConfigSize;            /**< Size of pConfig */
    void *pConfig;                  /**< Pointer to customer defined config */
} NVX_CONFIG_ODM;

/** Holds the imager GUID. */
typedef struct NVX_PARAM_SENSOR_GUID
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< OMX specification version info */
    OMX_U64 imagerGuid;             /**< GUID for the selected imager */
} NVX_PARAM_SENSOR_GUID;

#define NVX_INDEX_CONFIG_STEREORENDMODE "OMX.Nvidia.index.config.stereorendmode"
/** Enumerate the properietary stereo mode presence in incoming YUV frames. */
typedef enum OMX_STEREORENDMODETYPE {
    OMX_STEREORENDERING_OFF = 0,         /**< Default mode, when OMX operates in Mono channel mode */
    OMX_STEREORENDERING_HOR_STITCHED,    /**< When OMX expected the decoded surfaces to be horizontally stitched */
} OMX_STEREORENDMODETYPE;

/** Holds the stereo mode of the component. */
typedef struct OMX_CONFIG_STEREORENDMODETYPE {
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< OMX specification version information */
    OMX_STEREORENDMODETYPE eType;   /**< The stereo mode */
} OMX_CONFIG_STEREORENDMODETYPE;


#define SetAudioSourceParamExt (OMX_IndexVendorStartUnused | 0xFAFAFE)
typedef struct OMX_PARAM_SETAUDIOSOURCE{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    int AudioSourceParam;
} OMX_PARAM_SETAUDIOSOURCE;

/** Param extension index to get video encoder and decoder capability based on index
 *  See ::NVX_PARAM_CODECCAPABILITY
 */
#define NVX_INDEX_PARAM_CODECCAPABILITY "OMX.Nvidia.index.param.codeccapability"

/** Holds data to fine tune video encoder and decoder buffer configuration. */
typedef struct NVX_PARAM_CODECCAPABILITY
{
    OMX_U32 nSize;                 /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;      /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;            /**< Port that this struct applies to */
    OMX_U32 nCapIndex;             /**< (In) Value should be 0 to N*/
    OMX_U32 nMaxProfile;              /**< Type is OMX_VIDEO_AVCPROFILETYPE, OMX_VIDEO_H263PROFILETYPE,
                                        or OMX_VIDEO_MPEG4PROFILETYPE depending on context */
    OMX_U32 nMaxLevel;                /**< Type is OMX_VIDEO_AVCLEVELTYPE, OMX_VIDEO_H263LEVELTYPE,
                                        or OMX_VIDEO_MPEG4PROFILETYPE depending on context */
    OMX_U32 nMaxWidth;             /**< Maximum frame width supported (in pixels) */
    OMX_U32 nMaxHeight;            /**< Maximum frame height supported (in pixels) */
    OMX_U32 nFrameRate;            /**< Framerate supported for Max res.(in per sec.) */
    OMX_U32 nMaxBitRate;           /**< Maximum bitrate supported (in kbps) */
}NVX_PARAM_CODECCAPABILITY;

/** Param extension index to get audio decoder capability based on index
 *  See ::NVX_PARAM_AUDIOCODECCAPABILITY
 */
#define NVX_INDEX_PARAM_AUDIOCODECCAPABILITY "OMX.Nvidia.index.param.audiocodeccapability"

/**
 * @brief Defines the structure for holding the configuartion for the audio decoder
 * capabilities. These are stream independent properties. Decoder fills this
 * structure and pass to the IL-Client.
 */
typedef struct
{
    OMX_U32 nSize;                    /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;         /**< NVX extensions specification version information */
    OMX_U32 nMaxChannels;             /**< Holds maximum number of channels supported by decoder */
    OMX_U32 nMinBitsPerSample;        /**< Holds minimum number of bits required for each sample supported by decoder */
    OMX_U32 nMaxBitsPerSample;        /**< Holds maximum number of bits required for each sample supported by decoder */
    OMX_U32 nMinSampleRate;           /**< Holds maximum ample rate supported by decoder */
    OMX_U32 nMaxSampleRate;           /**< Holds maximum ample rate supported by decoder */

    OMX_BOOL isFreqRangeContinuous;   /**< Returns XA_BOOLEAN_TRUE if the device supports a continuous range of
                                              sampling rates between minSampleRate and maxSampleRate */
    OMX_U32 * pSampleRatesSupported;  /**< Indexed array containing the supported sampling rates. Ignored if
                                              isFreqRangeContinuous is XA_BOOLEAN_TRUE */
    OMX_U32 nSampleRatesSupported;  /**< Size of the pSamplingRatesSupported array */
    OMX_U32 nMinBitRate;              /**< Holds minimum bitrate supported by decoder in bps */
    OMX_U32 nMaxBitRate;              /**< Holds maximum bitrate supported by decoder in bps */

    OMX_BOOL isBitrateRangeContinuous;/**< Returns XA_BOOLEAN_TRUE if the device supports a continuous range of
                                              bitrates between minBitRate and maxBitRate */
    OMX_U32 * pBitratesSupported;     /**< Indexed array containing the supported bitrates. Ignored if
                                              isBitrateRangeContinuous is XA_BOOLEAN_TRUE */
    OMX_U32 nBitratesSupported;     /**< Size of the pBitratesSupported array. Ignored if
                                            isBitrateRangeContinuous is XA_BOOLEAN_TRUE */
    OMX_U32 nProfileType;             /**< Holds profile type  */
    OMX_U32 nModeType;                /**< Hold Mode type */
    OMX_U32 nStreamFormatType;        /**< Hold StreamFormat type */
} NVX_PARAM_AUDIOCODECCAPABILITY;


/** Blocks/unblocks socket activity.
 *
 * @param block Specify 1 to block all socket communication, 0 to unblock.
 */
void NVOMX_BlockAllSocketActivity(int block);

/** Param extension to get the actual video width, height and aspect ratio for
 * ARIB and similar use-cases
 */
#define NVX_INDEX_CONFIG_ARIBCONSTRAINTS "OMX.Nvidia.index.config.aribconstraints"

typedef struct OMX_CONFIG_ARIBCONSTRAINTS
{
    OMX_U32 nWidth;
    OMX_U32 nHeight;
} OMX_CONFIG_ARIBCONSTRAINTS;

#define NVX_INDEX_PARAM_VPP "OMX.Nvidia.index.param.vpp"

typedef enum {
   NV_VPP_TYPE_CPU = 0,
   NV_VPP_TYPE_EGL,
   NV_VPP_TYPE_CUDA,
   NV_VPP_TYPE_MAX = 0xFFFFFF,
}NVX_VPP_TYPE;


typedef struct OMX_PARAM_VPP
{
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< OMX specification version information */
    NVX_VPP_TYPE  nVppType;
    OMX_BOOL bVppEnable;
} NVX_PARAM_VPP;


/** Config extension index based on the OMX-AL Video Post Processing interface
 *  which will insert a 2D processing stage.
 *  See ::NVX_CONFIG_VIDEO2DPROCESSING
 */
#define NVX_INDEX_CONFIG_VIDEO2DPROC "OMX.Nvidia.index.config.video2dprocessing"

/** Indicates that Rotation is specified */
#define NVX_V2DPROC_FLAG_ROTATION              0x1

/** Indicates that ScalingOptions, background color and Rendering Hints are specified */
#define NVX_V2DPROC_FLAG_SCALEOPTIONS          0x2

/** Indicates that Source Rectangle is specified */
#define NVX_V2DPROC_FLAG_SOURCERECTANGLE       0x4

/** Indicates that Destination Rectangle is specified */
#define NVX_V2DPROC_FLAG_DESTINATIONRECTANGLE  0x8

/** Indicates that Mirror mode is specified */
#define NVX_V2DPROC_FLAG_MIRROR                0x10

/** Indicates that Video is stretched to the Destination Rectangle */
#define NVX_V2DPROC_VIDEOSCALE_STRETCH           1

/** Indicates that Video is fit in the Destination Rectangle */
#define NVX_V2DPROC_VIDEOSCALE_FIT               2

/** Indicates that Video is cropped to fit into the Destination Rectangle */
#define NVX_V2DPROC_VIDEOSCALE_CROP              3

/** Holds Data to setup the 2D processing stage */
typedef struct NVX_CONFIG_VIDEO2DPROCESSING
{
    OMX_U32 nSize;                 /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;      /**< NVX extensions specification version information */

    OMX_U32 nPortIndex;            /**< Port that this structure applies to.
                                        Input Port => Pre process, Output Port => Post Process */
    OMX_U32 nSetupFlags;           /**< See:NVX_V2DPROC_FLAG_XX */
    OMX_U32 nRotation;             /**< Integer Rotation. Valid if NVX_V2DPROC_FLAG_ROTATION present.
                                        Allowed Values: 0, 90, 180, 270 */
    OMX_MIRRORTYPE eMirror;        /**< Mirror mode. Valid if NVX_V2DPROC_FLAG_MIRROR present */
    OMX_U32 nScaleOption;          /**< Scaling of Video into Destionation rectangle.
                                        Refer to NVX_V2DPROC_VIDEOSCALE_XX.
                                        Valid only if NVX_V2DPROC_FLAG_SCALEOPTIONS present.*/
    OMX_U32 nBackgroundColor;      /**< Refers to RGBA value for the background color outside of
                                        the video in the destination rectangle.
                                        Valid only if NVX_V2DPROC_FLAG_SCALEOPTIONS present */
    OMX_U32 nRenderingHint;        /**< Unused. Valid only if NVX_V2DPROC_FLAG_SCALEOPTIONS present */

                                   /**< Source Rectangle coords; Valid only if
                                        NVX_V2DPROC_FLAG_SOURCERECTANGLE is present */
    OMX_U32 nSrcLeft;              /**< X coord of top left of Source Rectangle */
    OMX_U32 nSrcTop;               /**< Y coord of top left of Source Rectangle */
    OMX_U32 nSrcWidth;             /**< Width of Source Rectangle */
    OMX_U32 nSrcHeight;            /**< Height of Source Rectangle */

                                   /**< Destionation Rectangle coords; Valid only if
                                        NVX_V2DPROC_FLAG_DESTINATIONRECTANGLE is present*/
    OMX_U32 nDstLeft;              /**< X coord of top left of Dest Rectangle */
    OMX_U32 nDstTop;               /**< Y coord of top left of Dest Rectangle */
    OMX_U32 nDstWidth;             /**< Width of Dest Rectangle */
    OMX_U32 nDstHeight;            /**< Height of Dest Rectangle */

} NVX_CONFIG_VIDEO2DPROCESSING;

/* OMX extension index to tell decoder to decode only IFrames */
/**< reference: NVX_INDEX_CONFIG_DECODE_IFRAMES
 * Use OMX_CONFIG_BOOLEANTYPE
 */
#define NVX_INDEX_CONFIG_DECODE_IFRAMES "OMX.Nvidia.index.config.decodeiframes"

/* OMX extension index to tell decoder to decode Normally or skip all frames till next IDR*/
#define NVX_INDEX_CONFIG_VIDEO_DECODESTATE "OMX.Nvidia.index.config.video.decodestate"
typedef struct OMX_CONFIG_VIDEODECODESTATE {
    OMX_U32 nSize;                  /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< OMX specification version information */
    OMX_BOOL bDecodeState;
} OMX_CONFIG_VIDEODECODESTATE;

/* To Avoid dependency between IL Driver and Frameworks/base */
/** Defining the constant kMetadataBufferTypeEglStreamSource here rather than
 * <media/stagefright/MetadataBufferType.h>
 * kMetadataBufferTypeEglSource is used to indicate that
 * the source of the metadata buffer is EGL Stream Buffer.
 */
#define kMetadataBufferTypeEglStreamSource 0x7F000000

#endif

/** @} */

/* File EOF */
