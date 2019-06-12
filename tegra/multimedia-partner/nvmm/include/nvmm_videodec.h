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
 *           NvMM Video Decoder APIs</b>
 *
 * @b Description: Declares Interface for NvMM Video Decoder APIs.
 */

#ifndef INCLUDED_NVMM_VIDEODEC_H
#define INCLUDED_NVMM_VIDEODEC_H

/** @defgroup nvmm_videodec Video Decode API
 *
 * Describe the APIs here
 *
 * @ingroup nvmm_modules
 * @{
 */

#include "nvcommon.h"
#include "nvmmlite_video.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/** Video decoder sw driver access output surface
*/
#define NvMMVideoDec_OutSurfNoSWAccess  NV_FALSE
#define NvMMVideoDec_OutSurfSWAccess  NV_TRUE

/** videodec block's stream indices.
    Stream indices must start at 0.
*/
typedef enum
{
    NvMMVideoDecStream_In = 0, //!< Input
    NvMMVideoDecStream_Out,    //!< Output
    NvMMVideoDecStreamCount

} NvMMVideoDecStream;

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
     * enum from NvMMVideoDecPowerSaveProfile, refers to decoding profile to be
     * used.  Default Power saving decoder mode is
     * 'NvMMVideoDecPowerSaveProfile1' It is possible to configure multiple
     * power saving modes(except for 'NvMMVideoDecPowerSaveProfile1') in one
     * API call.
     * @see NvMMVideoDecPowerSaveProfile
     */
    NvMMVideoDecAttribute_PowerSaveProfile =
                            (NvMMAttributeVideoDec_Unknown + 1),

    /* Attribute helps application to specify to decode jpeg in interleaved mode
     */
    NvMMVideoDecAttribute_JpegInterleave,

    /** Attribute helps application to specify that it wants to decode Thumbnail Image
      */
    NvMMVideoDecAttribute_Decode_Thumbnail,

     /** Attribute to enable VDE cache statistics
      */
     NvMMVideoDecAttribute_Enable_VDE_Cache_Profiling,

     /** Attribute helps to choose the VDE Rotation Mode */
    NvMMVideoDecAttribute_Rotation_Mode,

    /** Attribute helps application to enable profiling
      */
    NvMMVideoDecAttribute_EnableProfiling,

    /** Attribute helps to configure video surface list for a given video decoder.
      * In this case, 2nd argument of is of NvMMVideoDecMPEG4SetAttribute(), is a
      * pointer to NvMMVideoDecMPEG4SurfaceList and reffers to application provided video surface list
      * @see NvMMVideoDecMPEG4SurfaceList
      */
    NvMMVideoDecAttribute_SurfaceList,

    /** Attribute helps application to specify the dumping of H/W commands and register values for
      * analysis
      */
    NvMMVideoDecAttribute_TraceDump,

    /** Attribute helps to Skip frames in the stream
      * Client to send an attribute value in percentage [0-100]. Decoder will skip those many percent of all
      * skippable (B & unused for reference) frames.
      */
    NvMMVideoDecAttribute_SkipFrames,

    /** This Attribute helps to enable the Error Concealment for the decoders
    */
    NvMMVideoDecAttribute_H264NALLen,

    /** This Attribute helps to init video decoders
    */
    NvMMVideoDecAttribute_VideoStreamInit,

    /** This Attribute helps to let decoder know that stream end has occured.
    */
    NvMMVideoDecAttribute_EndOfStream,

    /** This attribute is used  by the Decoder blocks in Look Ahead case.
    */
    NvMMVideoDecAttribute_LAStatus,

    /** This attribute is used by Dshow to let the block know that
        it is sending one input buffer at a time in Look Ahead case.
    */
    NvMMVideoDecAttribute_OneInpBuff,

    /** This attribute intimates the decoder to output skipped frames
     */
    NvMMVideoDecAttribute_OutputSkippedFrames,

    /** This attribute is used for setting maximum resolution to be supported by mpeg2 decoder.
        This is mpeg2 decoder specific attribute.
    */
    NvMMVideoDecAttribute_MPEG2MaxResolution,

    /** This attribute is used to know how many free surfaces the decoder has.
    */
    NvMMVideoDecAttribute_NumFreeSurfaces,

    /** This attribute is used to know how many frames have been skipped by thedecoder.
    */
    NvMMVideoDecAttribute_NumSkippedFrames,

    /** This attribute is used to know the total number of surfaces the decoder has.
    */
    NvMMVideoDecAttribute_TotalNumSurfaces,

    /** This attribute is used tell decoder to skip frame data and search for header data eg sps (h264), vol (mpeg4)
    */
    NvMMVideoDecAttribute_SearchForHeaderData,

    /** This attribute is used to explicitly release the HW lock if aquired by the block.
    */
    NvMMVideoDecAttribute_ReleaseHwLock,

    /** This attribute is used to explicitly set NvH264BufferFormat as NvH264BufferFormat_RTPStreamFormat
        and global NALUSizeFieldWidthInBytes value
    */
    NvMMVideoDecAttribute_H264SetRTPStreamFormat,

    /** This attribute is used to enable AVP+CPU side decoding path on AVP
    */
    NvMMVideoDecAttribute_EnableAVPCPUDecode,

    /** This attribute is used to enable in loop deblocking ON for H264
    */
    NvMMVideoDecAttribute_SetInloopDeblockingOn,

    /** This attribute is used to transfer SPS info decoded by CPU to AVP for H.264 AVP only config
    */
    NvMMVideoDecAttribute_H264SPS,

    /** This attribute is used to transfer PPS info decoded by CPU to AVP for H.264 AVP only config
    */
    NvMMVideoDecAttribute_H264PPS,

    /** This attribute can be used to get VUI parameters decoded by H.264 codec
    * This should be queried only after first decoded frame has been received
    * otherwise correctness of received VUI parameters is not guaranteed
    */
    NvMMVideoDecAttribute_H264VUI,

    /** This attribute is used to set the resource based on profile_idc so that
    * before decoding PPS the block comes to know whether it has sufficient resource for decoding
    */
    NvMMVideoDecAttribute_AllocateResource,

    /** This attribute is used to guide the decoders to use low memory decoding path
    */
    NvMMVideoDecAttribute_UseLowResource,

    /** This attribute is used to get whether resource allocation for a particular h264 flavor is
    * success or failure
    */
    NvMMVideoDecAttribute_ResourceAllocStatus,

    /** This attribute is used to check if Hw is busy
    */
    NvMMVideoDecAttribute_HwBusy,

    /** This attribute is used to display each scan in case of Progressive JPEG Decoder
    */
    NvMMVideoDecAttribute_DisplayEachScan,
    /** This attribute is used by superJPEG to request the input buffer to peek
    */
    NvMMVideoDecAttribute_SuperJPEGInpBuffStatus,

    /** This attribute is used to specify the error concealment mode.
    */
    NvMMVideoDecAttribute_EcMode,

    /** This attribute is used to initialize the output surface before decoding
     *  frame data into it. Used for CRC check of erroneous streams.
    */
    NvMMVideoDecAttribute_InitOutputSurface,

    /**
     * This attribute is used to set Timing info decoded on CPU to avp.
     */
     NvMMVideoDecAttribute_InitTimingInfo,

    /**
     * This attribute is used for disabling the auto EC  (HW EC) for mpeg2 decoder
     This is very specific to mpeg2 decoder
     */
     NvMMVideoDecAttribute_Disable_AutoEC,

     /** This attribute can be used to get AVC parameters decoded by H.264 codec
        * This should be queried only after first decoded frame has been received
        * otherwise correctness of received AVC parameters is not guaranteed
        */
    NvMMVideoDecAttribute_AVCTYPE,

    /** This attribute can be used to get Mpeg4 parameters decoded by mpeg4 codec
        * This should be queried only after first decoded frame has been received
        * otherwise correctness of received MPEG4 parameters is not guaranteed
        */
    NvMMVideoDecAttribute_MPEG4TYPE,

    /** This attribute can be used to get H263 parameters decoded by mpeg4 codec
        * This should be queried only after first decoded frame has been received
        * otherwise correctness of received H263 parameters is not guaranteed
        */
    NvMMVideoDecAttribute_H263TYPE,

    /** This attribute can be used to get MPEG2 parameters decoded by MPEG2 codec
        * This should be queried only after first decoded frame has been received
        * otherwise correctness of received MPEG2 parameters is not guaranteed
        */
    NvMMVideoDecAttribute_MPEG2TYPE,

    /**
     * This attribute is used to query MPEG2 decoder if it needs additional
     * Reconstruction block to successfully decode MPEG2 video (based on internal
     * hardware capability)
     */
     NvMMVideoDecAttribute_MPEG2NeedsRecon,

    /** This attribute is used to set / get stereo metadata to / from the
        decoder context. It uses NvMMVideoDecH264StereoInfo
        */
    NvMMVideoDecAttribute_H264StereoInfo,

    /**
     * This attribute is used for disabling DPB logic for h264 decoder.
     * This is used only when client knows that decode and display order is same.
     * Using it for other cases can cause undesired result.
     */
    NvMMVideoDecAttribute_H264_DisableDPB,

    /**
     * This attribute is used for enabling/disabling SXE-S mode in h264 decoder.
     */
    NvMMVideoDecAttribute_H264_SxeSMode,

    /**
     * This attribute is used for Programming Aes Key
     */
    NvMMVideoDecAttribute_H264_Programe_Aes_Key,

    /**
     * This attribute is used for setting number of input buffers in the decoder
     */
    NvMMVideoDecAttribute_NumInputBuffers,

    /**
     * This attribute is used to tell decoder that all input buffers
     * conatins full frame data.
     */
     NvMMVideoDecAttribute_CompleteFrameInputBuffer,

    /**
     * This attribute is used to set FullFrameData mode in mpeg2 decoder
     */
    NvMMVideoDecAttribute_FullFrameDataMode,
    /**
     *This attribute is used to check if sw driver will access output surface.
     */
    NvMMVideoDecAttribute_SurfaceSWAccess,
    /**
     *This attribute is used to check the profile and level supported by the
     *codec.
     */
    NvMMVideoDecAttribute_ProfileLevelQuery,
    /**
     *This attribute is used to get the maximum capability of a codec.
     */
    NvMMVideoDecAttribute_MaxCapabilities,
    /**
     *This attribute is used to used to disable deblocking for h264 high profile streams.
     */
    NvMMVideoDecAttribute_Enable_H264_HP_Crc_Check,

    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMVideoDecAttribute_Force32 = 0x7FFFFFFF
} NvMMVideoDecAttribute;


/**
 * @brief Defines various enum values for power saving. API SetAttribute is to
 * be used to set the profile. Higher the profile number, more will be the
 * power-saving.
 * @see SetAttribute
 * @see NvMMVideoDecAttribute_PowerSaveProfile
 */
typedef enum
{
    /// Holds default value for normal decoding mode.
    /// this power save profile 1 is used for Low-power-enable mode.
    NvMMVideoDecPowerSaveProfile1 = 0x1,
    ///  this power save profile 2 is used for skipping the B frames in the decoders.
    NvMMVideoDecPowerSaveProfile2,
    NvMMVideoDecPowerSaveProfile3,
    NvMMVideoDecPowerSaveProfile4,

    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMVideoDecPowerSaveProfile_Force32 = 0x7FFFFFFF
} NvMMVideoDecPowerSaveProfile;

/**
 * @brief Defines various enum values for vde rotation. API SetAttribute is to
 * be used to set the rotation Mode.
 * @see SetAttribute
 * @see NvMMVideoDecAttribute_Rotation_Mode
 */

typedef enum
{
    NvMMVideoDecNoRotation = 0,
    NvMMVideoDecHFlip,
    NvMMVideoDecVFlip,
    NvMMVideoDecRotate180,
    NvMMVideoDecXYSwap,
    NvMMVideoDecRotate270,
    NvMMVideoDecRotate90,
    NvMMVideoDecRotateXySwap_HVFlip
}NvMMVideoDecRotationMode;


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
    NvMMVideoDecEvent_NewStreamProperties = 0x1,
    /// Event to indicate completion of requested Set-Attribute
    NvMMVideoDecEvent_SetAttributeDone,

    NvMMVideoDecEvent_Force32 = 0x7FFFFFFF
} NvMMVideoDecEvents;

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

} NvMMVideoDecCapabilities;

/**
 * @brief Defines the structure for holding the configuartion for the decoder
 * capabilities. These are stream independent properties. Decoder fills this
 * structure on GetCodecCapabilities() function being called by Client.
 * @see GetCodecCapabilities
 */
typedef struct
{
    NvU32 StructSize;
    /// Holds maximum resolution supported by decoder
    NvU32 MaxWidth;
    NvU32 MaxHeight;

} NvMMVideoDecMaxResolution;

/**
 * @brief Defines the enums to be used as decode related attributes for decoded
 * video surface.
 * @see NvMMVideoDecDecodedPictureInfo
 */
typedef enum
{
    /** The video surface contains an I/IDR frame.
      * For H.264, this attribute will be set if frame is IDR OR frame contains
      * all I type MBs. For JPEG Decoder always attribute is set to I frame or
      * corrupted frame. Application can ignore attribute I frame.
      */
    NvMMVideoDecSurfDecodeAttr_IFrame = 0x1,
    /// The video surface contains an P frame.
    NvMMVideoDecSurfDecodeAttr_PFrame,
    /// The video surface contains an B bi-directionally predicted frame
    NvMMVideoDecSurfDecodeAttr_BFrame,
    /**
     * The video surface contains an key frame. Key frame refers to new decoder
     * cycle, any temporarily older frame earlier to key frame is not used by
     * decoder anymore. Eg. For MPEG-4 baseline profile, I frame is key frame.
     * For H.264 and IDR frame is key frame.
     */
    NvMMVideoDecSurfDecodeAttr_KeyFrame,
    /**
     * The video surface contains a frame which will be used as reference frame
     * by decoder.
     */
    NvMMVideoDecSurfDecodeAttr_ReferenceFrame,
    /**
     * The video surface contains a partially decoded frame due to bitstream
     * errors.  The decoder is supposed to apply some error concealment over
     * the lost region of frame.
     */
    NvMMVideoDecSurfDecodeAttr_CorruptFrame,

    NvMMVideoDecSurfDecodeAttr_Force32 = 0x7FFFFFFF
} NvMMVideoDecSurfDecodeAttributes;


/**
 * @brief Defines the structure for holding attributes of video surface which
 * is decoded recently by codec.
 * The structure is filled by codec and used by client for video surface
 * rendering purpose. This structure will be used as metadata variable defined
 * in NvMMBuffer in nvmm.h
 * @see NvMMBuffer
 */
typedef struct
{
    NvU32 StructSize;
    /** Holds decoding functionality related attributes for video Surface
      * refer to enums NvMMVideoDecSurfDecodeAttributes
      * @see NvMMVideoDecSurfDecodeAttributes
      */
    NvMMVideoDecSurfDecodeAttributes SurfaceDecodeAttributes;
    /// Holds time stamp in msec
    NvU32 TimeStamp;

} NvMMVideoDecMetadata;

/**
 * @brief Defines the attribute with physical address of the SPS info struct
 * sent by CPU that AVP can set to itself for H.264 AVP only config
 * @see NvMMVideoDecAttribute_H264SPS
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

} NvMMVideoDec_H264SPSInfo;

/**
 * @brief Defines the attribute with physical address of the PPS info struct
 * sent by CPU that AVP can set to itself for H.264 AVP only config
 * @see NvMMVideoDecAttribute_H264PPS
 */
typedef struct
{
    NvU32 StructSize;
    NvU32 NumPPS;
    NvU32 PhyAddress;

} NvMMVideoDec_H264PPSInfo;

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
} NvMMVideoDec_H264VUIInfo;

/**
 * Enum Defining Resolution units
 */

typedef enum
{
    // No units, aspect ratio only specified
    NvMMExif_ResolutionUnit_AspectRatioOnly = 0,
    // Pixels per inch
    NvMMExif_ResolutionUnit_Inch = 1,
    // Pixels per centimetre
    NvMMExif_ResolutionUnit_Centimeter = 2,
    // unspecified
    NvMMExif_ResolutionUnit_Uncalibrated = 0x7FFF
} NvMMExif_ResolutionUnit;

/**
 * @brief error concealment mode enumerations.
 * Following enum are used by video decoders for deciding the
 * error concealment mode - either SW computed motion vectors or
 * (0, 0) MVs
 */
typedef enum
{
    NvMMVideoDecEcMode_ZeroMvs,
    NvMMVideoDecEcMode_SwComputedMvs,
    NvMMVideoDecEcMode_Force32 = 0x7FFFFFFF
} NvMMVideoDecEcMode;

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
} NvMMVideoDec_AVCParamsInfo;

/**
* @brief Structure defined to store & query AVC profile & level.
*/
typedef struct
{
    NvMMLiteVideoAVCProfileType eProfile;
    NvMMLiteVideoAVCLevelType eLevel;
} NvMMAVC_SupportedProfileLevel;

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
}NvMMVideoDec_MPEG4ParamsInfo;

/**
* @brief Structure defined to store & query MPEG-4 profile & level .
*/
typedef struct
{
    NvMMLiteVideoMPEG4ProfileType eProfile;
    NvMMLiteVideoMPEG4LevelType eLevel;
} NvMMMPEG4_SupportedProfileLevel;

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
} NvMMVideoDec_H263ParamsInfo;

/**
* @brief Structure defined to store & query H263 profile & level.
*/
typedef struct
{
    NvMMLiteVideoH263ProfileType eProfile;
    NvMMLiteVideoH263LevelType eLevel;
} NvMMH263_SupportedProfileLevel;

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
} NvMMVideoDec_Mpeg2ParamsInfo;

/**
 * Defines the Stereo Flags that are sent from the App via an OMX Extension.
 * Sent to the decoder by a SetAttribute() call with NvMMVideoDecAttribute_H264StereoInfo
 * Read from the decoder by a GetAttribute() call with NvMMVideoDecAttribute_H264StereoInfo
 */
typedef struct
{
    NvU32   StructSize;
    // Stereo Flags: The bit positions are setup to match what is used in
    // BufferFlags in the NvMMBuffer strutcure. See nvmm.h for more details on the
    // exact bit definitions. Bits [20:15] of StereoFlags are used, rest are unused
    NvU32   StereoFlags;
} NvMMVideoDecH264StereoInfo;


#if defined(__cplusplus)
}
#endif

    /** @page NvMMVideoDecAppNotes Video Decoder Related Notes

    @section NvMMVideoDecAppNotes1 Input Buffer Usage

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

    @section NvMMVideoDecAppNotes2 Scaling of Output Surface for JPEG Decoder

    JPEG decoder allows output surface to be scaled down by HW built feature
    (via IDCT based scaling down).
    No scaling up is provided.
    The scaling down is only possible by factor of 1x(default), 2x, 4x, 8x.
    The client/Application is not required to set the explicit scale down
    factor, rather scaling decision is taken implicitly by NvMM block itself.
    The decision is based on width attribute of the output surface and original
    image. Appropriate scaling is choosen to fit the image to given surface.

    @section NvMMVideoDecAppNotes2 Partial Output of Surface for JPEG Decoder

    TBD...
    */

    /** @} */

#endif // INCLUDED_NVMM_VIDEODEC_H
