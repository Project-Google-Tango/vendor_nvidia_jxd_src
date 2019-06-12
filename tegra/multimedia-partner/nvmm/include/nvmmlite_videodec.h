/*
 * Copyright (c) 2007-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit:
 *           NvMM Video Decoder APIs</b>
 *
 * @b Description: Declares Interface for NvMM Video Decoder APIs.
 */

#ifndef INCLUDED_NVMMLITE_VIDEODEC_H
#define INCLUDED_NVMMLITE_VIDEODEC_H

/** @defgroup nvmmlite_videodec Video Decode API
 *
 * Describe the APIs here
 *
 * @ingroup nvmmlite_modules
 * @{
 */

#include "nvcommon.h"
#include "nvmmlite_video.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/** videodec block's stream indices.
    Stream indices must start at 0.
*/
typedef enum
{
    NvMMLiteVideoDecStream_In = 0, //!< Input
    NvMMLiteVideoDecStream_Out,    //!< Output
    NvMMLiteVideoDecStreamCount

} NvMMLiteVideoDecStream;

/**
 * @brief Video Decoder Attribute enumerations.
 * Following enum are used by video decoders for setting the attributes.
 * They are used as 'eAttributeType' variable in SetAttribute() and GetAttribute() APIs.
 * @see SetAttribute
 */
typedef enum
{
    /** Attribute helps to configure video decoder to a required power saving
     * decode profile.  In this case, 2nd argument of SetAttribute(), is an
     * enum from NvMMLiteVideoDecPowerSaveProfile, refers to decoding profile to be
     * used.  Default Power saving decoder mode is
     * 'NvMMLiteVideoDecPowerSaveProfile1' It is possible to configure multiple
     * power saving modes(except for 'NvMMLiteVideoDecPowerSaveProfile1') in one
     * API call.
     * @see NvMMLiteVideoDecPowerSaveProfile
     */
    NvMMLiteVideoDecAttribute_PowerSaveProfile =
                            (NvMMLiteAttributeVideoDec_Unknown + 1),

    /* Attribute helps application to specify to decode jpeg in interleaved mode
     */
    NvMMLiteVideoDecAttribute_JpegInterleave,

    /** Attribute helps application to specify that it wants to decode Thumbnail Image
      */
    NvMMLiteVideoDecAttribute_Decode_Thumbnail,

     /** Attribute to enable VDE cache statistics
      */
     NvMMLiteVideoDecAttribute_Enable_VDE_Cache_Profiling,

     /** Attribute helps to choose the VDE Rotation Mode */
    NvMMLiteVideoDecAttribute_Rotation_Mode,

    /** Attribute helps application to enable profiling
      */
    NvMMLiteVideoDecAttribute_EnableProfiling,

    /** Attribute helps to Skip frames in the stream
      * Client to send an attribute value in percentage [0-100]. Decoder will skip those many percent of all
      * skippable (B & unused for reference) frames.
      */
    NvMMLiteVideoDecAttribute_SkipFrames,

     /** This attribute can be used to get AVC parameters decoded by H.264 codec
        * This should be queried only after first decoded frame has been received
        * otherwise correctness of received AVC parameters is not guaranteed
        */
    NvMMLiteVideoDecAttribute_AVCTYPE,

    /** This attribute can be used to get Mpeg4 parameters decoded by mpeg4 codec
        * This should be queried only after first decoded frame has been received
        * otherwise correctness of received MPEG4 parameters is not guaranteed
        */
    NvMMLiteVideoDecAttribute_MPEG4TYPE,

    /** This attribute can be used to get H263 parameters decoded by mpeg4 codec
        * This should be queried only after first decoded frame has been received
        * otherwise correctness of received H263 parameters is not guaranteed
        */
    NvMMLiteVideoDecAttribute_H263TYPE,

    /** This attribute can be used to get MPEG2 parameters decoded by MPEG2 codec
        * This should be queried only after first decoded frame has been received
        * otherwise correctness of received MPEG2 parameters is not guaranteed
        */
    NvMMLiteVideoDecAttribute_MPEG2TYPE,

    /** This attribute is used to set / get stereo metadata to / from the
        decoder context. It uses NvMMLiteVideoDecH264StereoInfo
        */
    NvMMLiteVideoDecAttribute_H264StereoInfo,

    /**
     * This attribute is used for disabling DPB logic for h264 decoder.
     * This is used only when client knows that decode and display order is same.
     * Using it for other cases can cause undesired result.
     */
    NvMMLiteVideoDecAttribute_H264_DisableDPB,

    /**
     * This attribute is used for enabling/disabling deinterlacing also used for
     * deinterlace method selection.
     */
    NvMMLiteVideoDecAttribute_Deinterlacing,

    /**
     * This attribute is used for enabling/disabling SXE-S mode in h264 decoder.
     */
    NvMMLiteVideoDecAttribute_Decoder_SxeSMode,

    /**
     * This attribute is used to tell decoder that all input buffers
     * conatins full frame data.
     */
    NvMMLiteVideoDecAttribute_CompleteFrameInputBuffer,

    /**
     * This attribute is used to get profile and level info from the codec
     */
    NvMMLiteVideoDecAttribute_ProfileLevelQuery,

    /**
     * This attribute is used to get maximum capabilities of a decoder
     */
    NvMMLiteVideoDecAttribute_MaxCapabilities,

    /**
    * This attribute is used to get the ARIB specific information from decoders
    */
    NvMMLiteVideoDecAttribute_AribConstraints,

    /**
    * This attribute is used to tell decoder to skip frame data and search for header data eg sps (h264), vol (mpeg4)
    */
    NvMMLiteVideoDecAttribute_SearchForHeaderData,

    /**
    * This attribute is used to set decoder surface layout. Reference: NvRmSurfaceLayout;
    */
    NvMMLiteVideoDecAttribute_SurfaceLayout,

    /**
    * This attribute is used to set the block height for the block linear surface
    */
    NvMMLiteVideoDecAttribute_BlockHeightLog2,

    /**
    * This attribute is used to set the semi planar or planar format for the block linear surface
    */
    NvMMLiteVideoDecAttribute_SemiPlanar,

    /**
    * This attribute is used to tell decoder to decode only IDR frames
    */
    NvMMLiteVideoDecAttribute_DecodeIDRFramesOnly,

     /** Attribute helps to choose the VDE Mirror Mode */
    NvMMLiteVideoDecAttribute_Mirror_Mode,

    /** Attribute helps application to specify that it wants jpeg decode output in RGB format
      */
    NvMMLiteVideoDecAttribute_JpegOutputRGB_Mode,

    /** This attribute is used for disabling DFS logic */
    NvMMLiteVideoDecAttribute_DisableDFS,
    /** Attribute helps to specify if the stitching should be enabled or not for H264-MVC
     * */
    NvMMLiteVideoDecAttribute_H264StitchMVCViews,
    /**
    * This attribute is used to tell decoder to use low number of output buffer
    */
    NvMMLiteVideoDecAttribute_UseLowOutputBuffer,

    /** This attribute is used for setting/resetting the filtering of timestamp logic , set to FALSE by default */
    NvMMLiteVideoDecAttribute_FilterTimestamp,

    /**
     * This attribute is used to tell decoder that input buffers would
     * contain slice data so, do slice based decoding.
     * Also, each buffer would contain exactly 1 slice data.
     */
    NvMMLiteVideoDecAttribute_CompleteSliceInputBuffer,

    /**
     * This attribute is used to tell decoder that input buffers which contain clear data has to be treated as
     * encrypted i.e to allow the clear data to go through the OTF path of BSEV.
     */
    NvMMLiteVideoDecAttribute_ClearAsEncrypted,

    /**
     * This attribute is used to tell decoder that only one decoded frame is required.
     */
    NvMMLiteVideoDecAttribute_ThumbnailDecode,

    /**
     * This attribute is used to tell decoder that this usecase is for Mjonir.
     * Decoder will do the setting for best perf/power for this.
     */
    NvMMLiteVideoDecAttribute_Mjolnir_Streaming_playback,

    /**
     * This attribute is used to tell decoder that parse the input buffers of secure playback session which contain
     * clear data using VDE by allowing the clear data to go through the OTF path of BSEV.
     */
    NvMMLiteVideoDecAttribute_ParseClearUsingVde,
    /**
    * This attribute will enable DRC optimization from higher to lower resolution change
    */
    NvMMLiteVideoDecAttribute_EnableDRCOptimization,
    /**
    * This attribute is used to tell decoder to decode all the frames or skip all frames till next IDR frame
    */
    NvMMLiteVideoDecAttribute_DecodeState,

    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMLiteVideoDecAttribute_Force32 = 0x7FFFFFFF
} NvMMLiteVideoDecAttribute;


/**
 * @brief Defines various enum values for power saving. API SetAttribute is to
 * be used to set the profile. Higher the profile number, more will be the
 * power-saving.
 * @see SetAttribute
 * @see NvMMLiteVideoDecAttribute_PowerSaveProfile
 */
typedef enum
{
    /// Holds default value for normal decoding mode.
    /// this power save profile 1 is used for Low-power-enable mode.
    NvMMLiteVideoDecPowerSaveProfile1 = 0x1,
    ///  this power save profile 2 is used for skipping the B frames in the decoders.
    NvMMLiteVideoDecPowerSaveProfile2,
    NvMMLiteVideoDecPowerSaveProfile3,
    NvMMLiteVideoDecPowerSaveProfile4,

    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMLiteVideoDecPowerSaveProfile_Force32 = 0x7FFFFFFF
} NvMMLiteVideoDecPowerSaveProfile;

/**
 * @brief Defines various enum values for vde rotation. API SetAttribute is to
 * be used to set the rotation Mode.
 * @see SetAttribute
 * @see NvMMLiteVideoDecAttribute_Rotation_Mode
 */
typedef enum
{
    NvMMLiteDecRotate_0 = 0, // 0: ROTATE_0
    NvMMLiteDecRotate_90, // 1: ROTATE_90 CW
    NvMMLiteDecRotate_180, // 2: ROTATE_180 CW
    NvMMLiteDecRotate_270, // 3: ROTATE_270 CW
    NvMMLiteDecRotate_Force32 = 0x7FFFFFFF
} NvMMLiteVideoDecRotationMode;

/**
 * @brief Defines various enum values for vde mirroring. API SetAttribute is to
 * be used to set the Mirror Mode.
 * @see SetAttribute
 * @see NvMMLiteVideoDecAttribute_Mirror_Mode
 */
typedef enum
{
    NvMMLiteDecMirror_NoFlip = 0, // 0: FLIP_H
    NvMMLiteDecMirror_HFlip, // 1: FLIP_H
    NvMMLiteDecMirror_VFlip, // 2: FLIP_V
    NvMMLiteDecMirror_HVFlip, // 3: FLIP_H | FLIP_V
    NvMMLiteDecMirror_Force32 = 0x7FFFFFFF
} NvMMLiteVideoDecMirrorMode;

/**
 * @brief Defines events that will be sent from codec to client.
 * Client can use these events to take appropriate actions
 * @see SendEvent
 */
typedef enum
{
    /** Event to indicate sequence header in stream is detected. In case stream
     * detects repeated stream headers, those will not be reported back. Any
     * subseqnet occurence of this event means, new sequence header in stream.
     * In this case, decoder goes into stopped state. Client needs to
     * reconfigure the decoder.  In case client forces to continue decoding by
     * changing state by SetState(), Decoder will neglect the new sequence
     * header and try to continue decoding
     */
    NvMMLiteVideoDecEvent_NewStreamProperties = 0x1,
    /// Event to indicate completion of requested Set-Attribute
    NvMMLiteVideoDecEvent_SetAttributeDone,

    NvMMLiteVideoDecEvent_Force32 = 0x7FFFFFFF
} NvMMLiteVideoDecEvents;

/**
 * @brief defines the various color formats: Will be replaced by RM provided color formats
 * please do not modify enum values as they are used by vde hw
 */
typedef enum
{
    NvMMLiteVideoDecColorFormat_YUV420 = 0x0,
    NvMMLiteVideoDecColorFormat_YUV422,
    NvMMLiteVideoDecColorFormat_YUV422T,
    NvMMLiteVideoDecColorFormat_YUV444,
    NvMMLiteVideoDecColorFormat_GRAYSCALE,
    NvMMLiteVideoDecColorFormat_YUV420SemiPlanar,
    NvMMLiteVideoDecColorFormat_YUV422SemiPlanar,
    NvMMLiteVideoDecColorFormat_YUV422TSemiPlanar,
    NvMMLiteVideoDecColorFormat_YUV444SemiPlanar,
    NvMMLiteVideoDecColorFormat_R8G8B8A8,
    NvMMLiteVideoDecColorFormat_Force32 = 0x7FFFFFFF
} NvMMLiteVideoDecColorFormats;

// Deinterlacing methods enum
typedef enum
{
    /** NO deinterlacing */
    NvMMLite_DeintMethod_NoDeinterlacing,

    /** Bob on full frame. Two field output one frame. */
    NvMMLite_DeintMethod_BobAtFrameRate,

    /** Bob on full frame. Two field output two frames. */
    NvMMLite_DeintMethod_BobAtFieldRate,

    /** Weave on full frame. Two field output one frame. (This is same as NO deinterlacing) */
    NvMMLite_DeintMethod_WeaveAtFrameRate,

    /** Weave on full frame. Two field output two frames. */
    NvMMLite_DeintMethod_WeaveAtFieldRate,

    /** Advanced1. DeintMethod decided at MB level. Two field output one frame. */
    NvMMLite_DeintMethod_Advanced1AtFrameRate,

    /** Advanced1. DeintMethod decided at MB level. Two field output two frames. */
    NvMMLite_DeintMethod_Advanced1AtFieldRate,

    /** Advanced2. DeintMethod decided at MB level. Two field output one frame. */
    NvMMLite_DeintMethod_Advanced2AtFrameRate,

    /** Advanced2. DeintMethod decided at MB level. Two field output two frames. */
    NvMMLite_DeintMethod_Advanced2AtFieldRate,

    NvMMLite_DeintMethod_Force32 = 0x7FFFFFFF
} NvMMLiteDeinterlaceMethod;

typedef struct
{
    // StructSize
    NvU32   StructSize;

    // Deinterlacing Method
    NvMMLiteDeinterlaceMethod DeintMethod;
} NvMMLiteVDec_DeinterlacingOptions;

/**
 * @brief Defines the structure for holding the configuartion for the decoder
 * capabilities. These are stream independent properties. Decoder fills this
 * structure on GetCodecCapabilities() function being called by Client.
 * @see GetCodecCapabilities
 */
typedef struct
{
    NvU32 StructSize;
    /// Holds profile and level supported by current decoder. Refer decoder-
    /// specific header for respective values
    NvU32 VideoDecoderProfileAndLevel;
    /// Holds maximum bitrate supported by decoder in kbps
    NvU32 MaximumVideoBitRate;
    /// Holds maximum resolution supported by decoder
    NvU32 MaximumVideoFrameWidth;
    NvU32 MaximumVideoFrameheight;

} NvMMLiteVideoDecCapabilities;

/**
 * @brief Defines the attribute with physical address of the SPS info struct
 * sent by CPU that AVP can set to itself for H.264 AVP only config
 * @see NvMMLiteVideoDecAttribute_H264SPS
 */
typedef struct
{
    NvU32 StructSize;
    NvU32 NumSPS;
    NvU32 PhyAddress;
    NvU32 PicSizeInMbs;
    NvU32 MaxFrameNum;
    NvS32 PicWidthInMbs;
    NvRect DisplayRect;
    NvBool LatestSPSId;

} NvMMLiteVideoDec_H264SPSInfo;

/**
 * @brief Defines the attribute with physical address of the PPS info struct
 * sent by CPU that AVP can set to itself for H.264 AVP only config
 * @see NvMMLiteVideoDecAttribute_H264PPS
 */
typedef struct
{
    NvU32 StructSize;
    NvU32 NumPPS;
    NvU32 PhyAddress;

} NvMMLiteVideoDec_H264PPSInfo;

/**
* @brief Defines the decoded H264 VUI parameters useful for renderer.
* This should be queried only after first decoded frame has been received
* otherwise correctness of received VUI parameters is not guaranteed.
*/
typedef struct {
    NvBool  VUIParametersPresentFlag;   // following parameters valid if this is set

    NvBool  AspectRatioInfoPresentFlag;
    NvU8    AspectRatioIdc;             // valid if AspectRatioInfoPresentFlag is set
    NvU16   SarWidth;                   // valid if AspectRatioInfoPresentFlag is set
    NvU16   SarHeight;                  // valid if AspectRatioInfoPresentFlag is set

    NvBool  VideoSignalTypePresentFlag;
    NvBool  VideoFullRangeFlag;         // valid if VideoSignalTypePresentFlag is set
    NvU8    VideoFormat;                // valid if VideoSignalTypePresentFlag is set
    NvBool  ColorDescriptionPresentFlag;// valid if VideoSignalTypePresentFlag is set
    NvU8    ColorPrimaries;             // further, valid if ColorDescriptionPresentFlag is also set
    NvU8    TransferCharacteristics;    // further, valid if ColorDescriptionPresentFlag is also set
    NvU8    MatrixCoefficients;         // further, valid if ColorDescriptionPresentFlag is also set
} NvMMLiteVideoDec_H264VUIInfo;

/**
* @brief Defines the decoded H264 AVC parameters .
* This should be queried only after first decoded frame has been received
* otherwise correctness of received AVC parameters is not guaranteed.
*/
typedef struct
{
    NvU32 nRefFrames;
    NvU32 nRefIdx10ActiveMinus1;
    NvU32 nRefIdx11ActiveMinus1;
    NvMMLiteVideoAVCProfileType eProfile;
    NvMMLiteVideoAVCLevelType eLevel;
    NvBool bFrameMBsOnly;
    NvBool bMBAFF;
    NvBool bEntropyCodingCABAC;
    NvBool bWeightedPPrediction;
    NvU32  nWeightedBipredicitonMode;
    NvBool bconstIpred;
    NvBool bDirect8x8Inference;
    NvBool bDirectSpatialTemporal;
    NvU32 nCabacInitIdc;
    NvU32 LoopFilterMode;
} NvMMLiteVideoDec_AVCParamsInfo;

/**
* @brief Structure defined to store & query AVC profile & level.
*/
typedef struct
{
    NvMMLiteVideoAVCProfileType eProfile;
    NvMMLiteVideoAVCLevelType eLevel;
} NvMMLiteAVC_SupportedProfileLevel;

/**
 * MPEG-4 configuration.  This structure handles configuration options
 * which are specific to MPEG4 algorithms
 *
 * STRUCT MEMBERS:
 *  bSVH                 : Enable Short Video Header mode
 *  nIDCVLCThreshold     : Value of intra DC VLC threshold
 *  nTimeIncRes          : Used to pass VOP time increment resolution for MPEG4.
 *                         Interpreted as described in MPEG4 standard.
 *  eProfile             : MPEG-4 profile(s) to use.
 *  eLevel               : MPEG-4 level(s) to use.
 *  bReversibleVLC       : Specifies whether reversible variable length coding
 *                         is in use
 */
typedef struct
{
    NvBool bSVH;
    NvU32 nIDCVLCThreshold;
    NvU32 nTimeIncRes;
    NvMMLiteVideoMPEG4ProfileType eProfile;
    NvMMLiteVideoMPEG4LevelType eLevel;
    NvBool bReversibleVLC;
}NvMMLiteVideoDec_MPEG4ParamsInfo;

/**
* @brief Structure defined to store & query MPEG-4 profile & level .
*/
typedef struct
{
    NvMMLiteVideoMPEG4ProfileType eProfile;
    NvMMLiteVideoMPEG4LevelType eLevel;
} NvMMLiteMPEG4_SupportedProfileLevel;

/**
 * H.263 Params
 *
 * STRUCT MEMBERS:
 *  eProfile                 : H.263 profile(s) to use
 *  eLevel                   : H.263 level(s) to use
 */
typedef struct
{
    NvMMLiteVideoH263ProfileType eProfile;
    NvMMLiteVideoH263LevelType eLevel;
} NvMMLiteVideoDec_H263ParamsInfo;

/**
* @brief Structure defined to store & query H263 profile & level.
*/
typedef struct
{
    NvMMLiteVideoH263ProfileType eProfile;
    NvMMLiteVideoH263LevelType eLevel;
} NvMMLiteH263_SupportedProfileLevel;

/**
 * MPEG-2 params
 *
 * STRUCT MEMBERS:
 *  eProfile   : MPEG-2 profile(s) to use
 *  eLevel     : MPEG-2 levels(s) to use
 */
typedef struct
{
    NvMMLiteVideoMPEG2ProfileType eProfile;
    NvMMLiteVideoMPEG2LevelType eLevel;
} NvMMLiteVideoDec_Mpeg2ParamsInfo;

/**
 * Defines the Stereo Flags that are sent from the App via an OMX Extension.
 * Sent to the decoder by a SetAttribute() call with NvMMLiteVideoDecAttribute_H264StereoInfo
 * Read from the decoder by a GetAttribute() call with NvMMLiteVideoDecAttribute_H264StereoInfo
 */
typedef struct
{
    NvU32   StructSize;
    // Stereo Flags: The bit positions are setup to match what is used in
    // BufferFlags in the NvMMBuffer strutcure. See nvmm.h for more details on the
    // exact bit definitions. Bits [20:15] of StereoFlags are used, rest are unused
    NvU32   StereoFlags;
} NvMMLiteVideoDecH264StereoInfo;

/**
 * To provide security for slice based decoding.
 * nAuthentication authenticates identity of APP.
 */
typedef struct
{
    NvU32   StructSize;
    NvU32   nAuthentication;
    NvBool  bEnabled;
} NvMMLiteVDec_SliceDecode;

/**
 * Defines the ARIB Specific params which are required for ISDBT use case
 * Read from decoder by a GetAttribute() call with NvMMVideoDecAttribute_AribConstraints
 */
typedef struct
{
    NvU32 Width;
    NvU32 Height;
} NvMMLiteVideoDec_AribConstraintsInfo;

#if defined(__cplusplus)
}
#endif

    /** @page NvMMLiteVideoDecAppNotes Video Decoder Related Notes

    @section NvMMLiteVideoDecAppNotes1 Input Buffer Usage

    The Video decoders expect the input bitstream to contain whole integer
    amount of basic decode units. Basic decode units are ones which can be
    decoded completely without relying on anymore upcoming information.
    Hence types of basic decode units are:
    a). SPS, PPS - Picture and Sequence parameter sets
    b). Frame data - whole frame data (possibly with multiple slices/NAL)
    c). Slice/NAL data - one slice/NAL unit of current frame.

    Note that it is possible to club multiple such basic decode units into
    single buffer irrespective of frame boundaries. That is, one buffer can
    have one NAL unit from (N-1)th frame accompanied by all NAL units of (N)th
    frame.

    In case rule is not followed, and basic decode unit is split into two
    buffers, the part from 2nd buffer will be skipped till new basic decode
    unit is found.

    Only exception to this rule if JPEG decoder, where client/Application can
    send any size of buffer data.

    @section NvMMLiteVideoDecAppNotes2 Scaling of Output Surface for JPEG Decoder

    JPEG decoder allows output surface to be scaled down by HW built feature
    (via IDCT based scaling down).
    No scaling up is provided.
    The scaling down is only possible by factor of 1x(default), 2x, 4x, 8x.
    The client/Application is not required to set the explicit scale down
    factor, rather scaling decision is taken implicitly by NvMM block itself.
    The decision is based on width attribute of the output surface and original
    image. Appropriate scaling is choosen to fit the image to given surface.

    @section NvMMLiteVideoDecAppNotes2 Partial Output of Surface for JPEG Decoder

    TBD...
    */

    /** @} */

#endif // INCLUDED_NVMM_VIDEODEC_H
