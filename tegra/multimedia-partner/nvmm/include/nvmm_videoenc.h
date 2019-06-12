/*
 * Copyright (c) 2007-2014 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit:
 *           NvMM Video Encoder APIs</b>
 *
 * @b Description: Declares Interface for NvMM Video Encoder APIs.
 */

#ifndef INCLUDED_NVMM_VIDEOENC_H
#define INCLUDED_NVMM_VIDEOENC_H

/**
 * @defgroup nvmm_videoenc Video Encode API
 *
 *
 * @ingroup nvmm_modules
 * @{
 */

#include "nvcommon.h"
#include "nvmm_event.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define PROFILE_VIDEO_ENCODER 0
#define PROFILE_NVRmStreamFlush 0

#if (PROFILE_VIDEO_ENCODER)
#define ENABLE_PROFILING 1
#endif

// Enable this macro for Power Measurement work
#define POWER_MEASUREMENT 0

// Enable this macro for MPE speed characterization work
//#define MPE_SPEED_CHAR 1

/**
  * @brief Video Encoder block's stream indices.
  * Stream indices must start at 0.
*/
typedef enum
{
    NvMMBlockVideoEncStream_In = 0, /* Input */
    NvMMBlockVideoEncStream_Out,    /* Output */
    NvMMBlockVideoEncStreamCount
} NvMMBlockVideoEncStream;

/**
 * @brief Video Encoder Attribute enumerations.
 * Following enum are used by video encoders for setting the attributes.
 * They are used as 'eAttributeType' variable in SetAttribute() API.
 * @see SetAttribute
 */
enum
{
    NvMMAttributeVideoEnc_CommonStreamProperty =
                    (NvMMAttributeVideoEnc_Unknown + 1),    /* uses NvMMVideoEncBitStreamProperty */
    NvMMAttributeVideoEnc_Configuration,                    /* uses NvMMVideoEncConfiguration */
    NvMMAttributeVideoEnc_MacroblockErrorMap,               /* uses NvMMVideoEncMacroblockErrorMap */
    NvMMAttributeVideoEnc_PowerManagement,                  /* uses NvMMVideoEncPowerManagement */
    NvMMAttributeVideoEnc_H264StreamProperty,               /* uses NvMMVideoEncH264Params */
    NvMMAttributeVideoEnc_Mpeg4StreamProperty,              /* uses NvMMVideoEncMPEG4Params */
    NvMMAttributeVideoEnc_Rotation,                         /* uses NvMMVideoEncRotation */
    NvMMAttributeVideoEnc_Mirroring,                        /* uses NvMMVideoEncMirroring */
    NvMMAttributeVideoEnc_Lookahead,                        /* uses NvMMVideoEncLookahead */

    NvMMAttributeVideoEnc_EnableProfiling,                  /* Attribute helps application to enable profiling */
    NvMMAttributeVideoEnc_ForceIntraFrame,

    NvMMAttributeVideoEnc_Packet,                           /* uses NvMMVideoEncPacket */
    NvMMAttributeVideoEnc_VideoTemporalTradeOff,            /* uses NvMMVideoEncTemporalTradeOff */
    NvMMAttributeVideoEnc_DciInfo,
    NvMMAttributeVideoEnc_H264CropRect,                     /* uses NvMMVideoEncH264CropRect */
    NvMMAttributeVideoEnc_InitialQP,                        /* uses NvMMVideoEncInitialQP */
    NvMMAttributeVideoEnc_QPRange,                          /* uses NvMMVideoEncQPRange */
    NvMMAttributeVideoEnc_H264StereoInfo,                   /* uses NvMMVideoEncH264StereoInfo */
    NvMMAttributeVideoEnc_H264QualityParams,                /* uses NvMMH264EncQualityParams */
    NvMMAttributeVideoEnc_ProfileLevelQuery,
    NvMMAttributeVideoEnc_MaxCapabilities,
    NvMMAttributeVideoEnc_VP8StreamProperty,                /* uses NvMMVideoEncVP8Params */
    NvMMAttributeVideoEnc_VP8ExtraStreamProperty,           /* uses NvMMVideoEncVP8ExtraParams */
    NvMMAttributeVideoEnc_PendingInputBuffers,
    NvMMAttributeVideoEnc_MaxInputBuffers,
    NvMMAttributeVideoEnc_EnableNoiseReduction,
    NvMMAttributeVideoEnc_ActiveSliceLevelEncode,
    NvMMAttributeVideoEnc_SkipFrame,
    NvMMAttributeVideoEnc_FrameError,                       /* uses NvMMVideoEncFrameErrorParams */
    NvMMAttributeVideoEnc_AvgEncTime,                       /* uses NvMMVideoEncAvgEncTime */
    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMAttributeVideoEnc_Force32 = 0x7FFFFFFF
};

/**
 * @brief Video Encode Profiling.
 */
enum
{
    NvMMAttributeVideoEncProfile_None = 0x0,
    NvMMAttributeVideoEncProfile_EncodeStart_2_RegWriteSafe_Done = 0x1,   /* New Buffer to Reg-Wr Done */
    NvMMAttributeVideoEncProfile_RegWriteSafe_2_EBMEOF_Done = 0x2,        /* Reg-Wr to EBM-EOF */
    NvMMAttributeVideoEncProfile_EBMEOF_2_ReadEBMData = 0x4,              /* EBM-EOF to Read EBM Data */
    NvMMAttributeVideoEncProfile_NvMM_FrameEncode = 0x8,                   /* Complete NvMM Profile for one frame encode */
    NvMMAttributeVideoEncProfile_MPE_FRAME_MB_Cycle_Count = 0x10
};

/**
 * @brief Application Type for Video Encoder.
 */
typedef enum
{
    NvMMVideoEncAppType_Camcorder = 1,
    NvMMVideoEncAppType_VideoTelephony,
}NvMMVideoEncAppType;

/**
 * @brief Quality Level for Application Type of Video Encoder.
 */
typedef enum
{
    NvMMVideoEncQualityLevel_Low = 1,
    NvMMVideoEncQualityLevel_Medium,
    NvMMVideoEncQualityLevel_High
}NvMMVideoEncQualityLevel;

/**
 * @brief Error Level for Application Type of Video Encoder.
 */
typedef enum
{
    NvMMVideoEncErrorResiliencyLevel_None = 0,
    NvMMVideoEncErrorResiliencyLevel_Low,
    NvMMVideoEncErrorResiliencyLevel_High
}NvMMVideoEncErrorResiliencyLevel;


/**
* @brief TemporalTradeOffLevel for Application Type of Video Encoder.
*/
typedef enum
{
    NvMMVideoEncTemporalTradeoffLevel_DropNone= 0,
    NvMMVideoEncTemporalTradeoffLevel_Drop1in5,
    NvMMVideoEncTemporalTradeoffLevel_Drop1in3,
    NvMMVideoEncTemporalTradeoffLevel_Drop1in2,
    NvMMVideoEncTemporalTradeoffLevel_Drop2in3
}NvMMVideoEncTemporalTradeoffLevel;


/**
 * @brief Defines the structure for encode configuration
 * that can be set by SetAttribute() call with NvMMAttributeVideoEnc_Configuration
 * @see GetAttribute
 */
typedef struct NvMMVideoEncConfigurationInfo_{
    NvU32 structSize;
    NvMMVideoEncAppType AppType;
    NvMMVideoEncQualityLevel QualityLevel;
    NvMMVideoEncErrorResiliencyLevel ErrResLevel;
    NvBool  bSetMaxEncClock;
}NvMMVideoEncConfiguration;

typedef enum
{
    NvMMVideoEncPacketType_MBLK = 0,
    NvMMVideoEncPacketType_BITS
}NvMMVideoEncPacketType;

/**
 * @brief Defines the Rate Control Mode for video encode
 * that can be set by SetAttribute() call with NvMMAttributeVideoEnc_H264StreamProperty
 * @see GetAttribute
 */
typedef enum
{
    NvMMVideoEncRateCtrlMode_ConstQp = 0,
    NvMMVideoEncRateCtrlMode_VBR2 = 1,
    NvMMVideoEncRateCtrlMode_VBR_PicConstQp = 1,
    NvMMVideoEncRateCtrlMode_CBR_PicConstQp = 2,
    NvMMVideoEncRateCtrlMode_VBR_PicMinQp = 3,

    NvMMVideoEncRateCtrlMode_VBR = 17,              // default VBR = MB row const Qp
    NvMMVideoEncRateCtrlMode_VBR_MbRowConstQp = 17,
    NvMMVideoEncRateCtrlMode_CBR = 18,              // default CBR = MB row const Qp
    NvMMVideoEncRateCtrlMode_CBR_MbRowConstQp = 18,
    NvMMVideoEncRateCtrlMode_VBR_MbRowMinQp = 19,

    NvMMVideoEncRateCtrlMode_MbAdaptive = 32,
    NvMMVideoEncRateCtrlMode_CBR_TwoPass_MbRowConstQp = 34,

    NvMMVideoEncRateCtrlMode_Force32 = 0x7FFFFFFF
}NvMMVideoEncRateCtrlMode;

/**
 * @brief Defines the structure for packet type
 * that can be set by SetAttribute() call with NvMMAttributeVideoEnc_Packet
 * @see GetAttribute
 */
typedef struct
{
    NvU32   StructSize;
    // packet type can be either in macroblocks or bits
    NvMMVideoEncPacketType    PacketType;
    // length of packet depending upon its type
    NvU32   PacketLength;
} NvMMVideoEncPacket;

/**
 * @brief Defines events that will be sent from codec to client.
 * Client can use these events to take appropriate actions
 */
typedef enum
{
    NvMMEventVideoEnc_FrameSkipped = NvMMEventVideoEnc_EventOffset,
    NvMMEventVideoEnc_Force32 = 0x7FFFFFFF
} NvMMVideoEncEvents;

/**
 * @brief Defines frame skip info that will be sent with
 * NvMMEventVideoEnc_FrameSkipped if encoder decides to skip a frame.
 * Client can use the event information to take appropriate actions
 */
typedef struct NvMMVideoEncFrameSkippedEventInfo_{
    NvU32           structSize;
    NvMMEventType   event;
    /// Skipped frame timestamp
    NvU64           TimeStamp;
    /// Skipped frame number
    NvU32           SkippedFrame;
}NvMMVideoEncFrameSkippedEventInfo;

/**
 * @brief Defines the structure for video resolution
 */
typedef struct
{
    NvU32   width;
    NvU32   height;
}NvMMVideoEncResolution;

typedef enum
{
    NvMMBlockVEnc_FrameRate             = 0x1,      /* to set Frame-Rate */
    NvMMBlockVEnc_BitRate               = 0x2,      /* to set BitRate */
    NvMMBlockVEnc_FrameSkip             = 0x4,      /* to enable/disable FrameSkip */
    NvMMBlockVEnc_StringentBitRate      = 0x8,      /* to enable/disable StringentBitRate */
    NvMMBlockVEnc_IntraFrameInterval    = 0x10,     /* to set IntraFrame Interval */
    NvMMBlockVEnc_Resolution            = 0x20,     /* to set Width and Height */
    NvMMBlockVEnc_SVC                   = 0x40,      /* to enable/disable SVC Encoding */
    NvMMBlockVEnc_AllIFrames            = 0x80,      /* to encode all frames as I Frames */
    NvMMBlockVEnc_InsertSPSPPSAtIDR     = 0x100,    /* to insert SPS/PPS at every IDR */
    NvMMBlockVEnc_UseConstrainedBP      = 0x200,    /* to Use Contrained Baseline profile */
    NvMMBlockVEnc_PeakBitRate           = 0x400,    /* to set PeakBitRate for VBR*/
    NvMMBlockVEnc_ByteStuffing          = 0x800,    /* to enable ByteStuffing for CBR*/
    NvMMBlockVEnc_SliceLevelEncode      = 0x1000,    /* to enable slice level encode */

    /// Max should always be the last value, used to pad the enum to 32bits
    NvMMBlockVEnc_Force32 = 0x7FFFFFFF
}NvMMVideoEncBitStreamFlags;

/*
H264 Levels For Baseline Profile
 */
typedef enum
{
    H264_LevelIdc_Undefined = 1,
    H264_LevelIdc_10 = 10,
    H264_LevelIdc_1B = 9,
    H264_LevelIdc_11 = 11,
    H264_LevelIdc_12 = 12,
    H264_LevelIdc_13 = 13,
    H264_LevelIdc_20 = 20,
    H264_LevelIdc_21 = 21,
    H264_LevelIdc_22 = 22,
    H264_LevelIdc_30 = 30,
    H264_LevelIdc_31 = 31,
    H264_LevelIdc_32 = 32,
    H264_LevelIdc_40 = 40,
    H264_LevelIdc_41 = 41,
    H264_LevelIdc_42 = 42,
    H264_LevelIdc_50 = 50,
    H264_LevelIdc_51 = 51,
}H264EncLevelIdc;

/*
MPEG4 Levels For Simple Profile
 */
typedef enum
{
    MPEG4_Level_Undefined = 0xFF,
    MPEG4_Level_0 = 0,
    MPEG4_Level_1 = 1,
    MPEG4_Level_2 = 2,
    MPEG4_Level_3 = 3,
    MPEG4_Level_4a = 4,
    MPEG4_Level_5 = 5,
    MPEG4_Level_Unsupported = 0x7FFFFFFF
}MPEG4EncLevelIdc;

typedef union
{
    MPEG4EncLevelIdc MPEG4Level;
    H264EncLevelIdc  H264Level;
}NvMMVideoEncLevel;

/*
Size of Decoder Configartion Information
*/
#define VIDEOENC_DCI_SIZE 80
typedef struct
{
    NvU32 StructSize;
    NvU8 DecodeConfigInfo[VIDEOENC_DCI_SIZE];
    NvU8 SizeOfDecodeConfigInfo;
}NvMMVideoEncDecodeConfigInfo;

/**
 * @brief Defines the structure for holding the encoding attributes
 * that can be set by SetAttribute() call with NvMMAttributeVideoEnc_CommonStreamProperty
 * @see GetAttribute
 */
typedef struct
{
    NvU32   StructSize;
    /// Its individual bits is set based upon NvMMVideoEncBitStreamFlags enum definition.
    NvU32   UpdateParams;
    /// Index of stream with given properties
    NvU32   StreamIndex;
    /// Frame rate of stream in fps
    double  FrameRate;
    /// Bitrate of stream in bits/second
    NvU32   BitRate;
    /// Boolean to enable frame skipping
    NvU32   bFrameSkip;
    /// Boolean to enable StringentBitrate option
    NvU32   bStringentBitrate;
    /// IDR Frame generation Interval
    NvU32   IntraFrameInterval;
    /// Mona Support
    NvU32   bMona;
    //Profile Level Info
    NvMMVideoEncLevel Level;
    /// Video Resolution
    NvMMVideoEncResolution   VideoEncResolution;
    /// All Intra Frames
    NvBool  bAllIFrames;
    /// Peak Bitrate for VBR
    NvU32   PeakBitrate;
    /// To enable stuff bytes to maintain bitrate
    NvBool bEnableStuffing;
    /// To enable slice level encoding
    NvBool bSliceLevelEncode;
#if POWER_MEASUREMENT
    /// Boolean to bitstream processing. Needed only for power measurement work.
    NvU32   DisableBitstreamProcessing;
    /// Boolean for MPE clock. EnableMpeClk =  Needed only for power measurement work.
    NvU32   EnableMpeClk;
    /// Desired MPE Clk frequency
    NvU32   MpeClkKhz;
#endif
} NvMMVideoEncBitStreamProperty;

/**
 * @brief Defines the structure for holding the configuartion for the encoder
 * capabilities. These are stream independent properties. Encoder fills this
 * structure on NvMMGetMMBlockCapabilities() function being called by client.
 * @see NvMMGetMMBlockCapabilities
 */
typedef struct
{
    NvU32   StructSize;
    /// Holds maximum bitrate supported by encoder in bps
    NvU32   MaxBitRate;
    /// Holds maximum framerate supported by encoder in fps
    NvU32   MaxFrameRate;
    /// Holds Level Idc for particular encoder
    NvU32   Level;
    /// Bitmask of supported profile and levels (see
    /// NvMMVideoProfileAndLevel for bit positions)
    NvU32   ProfilesAndLevels;
} NvMMVideoEncCapabilities;

/**
 * @brief Defines the structure for holding encoder attributes
 * related to MB error map (Intra Refresh pattern) that can set
 * by SetAttribute() call with NvMMAttributeVideoEnc_MacroblockErrorMap
 * SetResolution should be called before setting this attribute
 */
typedef struct
{
    NvU32   StructSize;
    /// Percentage macroblocks with in the frame should be made intra cyclical
    NvU8    PercentageIntraMB;
}NvMMVideoEncMacroblockErrorMap;

/**
 * @brief Defines the structure for managing power state of video
 * encoder that can be set by SetAttribute() call with NvMMAttributeVideoEnc_PowerManagement
 * To suspend- Suspend = NV_TRUE
 * To resume from suspended state- Suspend = NV_FALSE
 * Encoder should be running before setting this attribute
 */
typedef struct
{
    NvU32   StructSize;
    /// Boolean to toggle the suspend-resume state
    NvBool  Suspend;
}NvMMVideoEncPowerManagement;

/**
 * @brief Defines the structure for Forcing Intra Frame dynamically
 * using SetAttribute call with NvMMAttributeVideoEnc_ForceIntraFrame
 * The very next frame which Encoder takes from HW/SW queue will be forced to Intra
 * This implementation does not mark any specific frame to be forced as Intra.
 */
typedef struct
{
    NvU32   StructSize;
}NvMMVideoEncForceIntraFrame;

/**
 * @brief Defines the structure for sending VideoTemporalTradeOff level.
 * using SetAttribute call with NvMMAttributeVideoEnc_VideoTemporalTradeOff
 * Supported values given by NvMMVideoEncTemporalTradeoffLevel
 */
typedef struct
{
    NvU32   StructSize;
    NvMMVideoEncTemporalTradeoffLevel   VideoTemporalTradeOff;
}NvMMVideoEncTemporalTradeOff;

typedef enum
{
    H264_Profile_Undefined = 0x0,
    H264_Profile_Baseline = 66,
    H264_Profile_Main = 77,
    H264_Profile_High = 100,
    H264_Profile_Stereo_High = 128,

    H264_Profile_Force32 = 0x7FFFFFFF
}NvMMH264EncProfile;

/**
 * @brief Defines the structure for H264 Encoder Setup
 * that can be set by SetAttribute() call with NvMMAttributeVideoEnc_H264StreamProperty
 */
typedef struct
{
    NvU32   StructSize;
    /// Virtual buffer size to be supplied by application in bits
    NvU32   VirtualBufferSize;
    // Specifies either 0:BYTE Stream Mode or 1:NAL Stream Mode.
    NvU32   StreamMode;
    // bRTPMode override StreamMode to NAL stream mode, and replace start code with marker field.
    NvU32   bRTPMode;
    // CBR and VBR Rate Control Mode.
    NvMMVideoEncRateCtrlMode   RateCtrlMode;
    // SVC Encoding
    NvU32   bSvcEncode;
    /// Insert SPS/PPS at every IDR
    NvBool  bInsertSPSPPSAtIDR;
    /// Use Constrained Baseline profile
    NvBool  bUseConstrainedBP;
    /// Insert VUI into the bitstream
    NvBool  bInsertVUI;
    /// Insert AUD into the bitstream
    NvBool  bInsertAUD;
    /// Reduce latency by lowering peak frame sizes (for both I and P frames)
    NvBool  bLowLatency;

    /* Following parameters are applicable for MSENC only
       For others they will not have any effect */

    // Profile
    NvMMH264EncProfile Profile;
    // Peak bitrate of stream in bits/second
    NvU32 IFrameInterval;
    // Number of B frames between 2 I/P frames
    NvU32 NumBFrames;
    // Number of reference frames
    NvU32 NumReferenceFrames;
    // Display Aspect Ratio
    NvMMDisplayResolution DAR;
    // Enable Weighted Prediction for B frames
    NvU32 bWeightedPrediction;
    // Enable Monochrome for High Profile
    NvU32 bMonochrome;
    // Disable CABAC, enabled by default for Main and High profile
    NvU32 bCabacDisable;
    // Deblock Idc, enabled by default
    NvU32 DisableDeblockIdc;
    // Enable Reference picture invalidation
    NvBool bEnableRefPicInvalidation;
    // Enable Slice intra refresh wave
    NvBool bSliceIntraRefreshEnable;
    // Slice intra refresh wave interval in number of frames
    NvU32 SliceIntraRefreshInterval;
    // Disable SPS constraint bits setting for Constrained High Profile
    NvBool bChpBitsDisable;
    // Enable Cyclic intra refresh wave
    NvBool bCyclicIntraRefreshEnable;
    //number of MBs in Cyclic intra refresh wave
    NvU32 Cirmbs;
}NvMMVideoEncH264Params;

/**
 * @brief Defines the structure for MPEG4 Encoder Setup
 * that can be set by SetAttribute() call with NvMMAttributeVideoEnc_Mpeg4StreamProperty
 */
typedef struct
{
    NvU32   StructSize;
    /// Virtual buffer size to be supplied by application in bits
    NvU32   VirtualBufferSize;
    /// Initial buffer fullness in integer percentage of virtual buffer size
    NvU32   InitBufferFullPerc;
    /// Boolean to enable H.263 encoding
    NvU32   bH263Encode;
}NvMMVideoEncMPEG4Params;

/**
 * @brief Defines the structure for VP8 Encoder Setup
 * that can be set by SetAttribute() call with NvMMAttributeVideoEnc_VP8StreamProperty
 */
typedef struct
{
    NvU32   StructSize;
    /// Virtual buffer size to be supplied by application in bits
    NvU32   VirtualBufferSize;
    // CBR and VBR Rate Control Mode.
    NvMMVideoEncRateCtrlMode   RateCtrlMode;
    // Number of reference frame
    NvU32 NumReferenceFrames;
    // VP8 version type
    NvU32 version;
    // VP8 filter type
    NvU32 filter_type;
    // VP8 filter level
    NvU32 filter_level;
    // VP8 sharpness level
    NvU32 sharpness_level;
}NvMMVideoEncVP8Params;

/**
 * @brief Defines the structure for VP8 Encoder Setup
 * that can be set by SetAttribute() call with NvMMAttributeVideoEnc_VP8ExtraStreamProperty
 */
typedef struct
{
    NvU32   StructSize;
    // Array to store ref_lf_deltas
    NvS8 ref_lf_deltas[4];
    // Array to store vp8_mode_lf_deltas
    NvS8 mode_lf_deltas[4];
    // VP8 quant base index (used only when rc_mode = 0)
    NvU32 base_qindex;
    // VP8 delta (subtracted from baseline quant index)
    // VP8 Y1 dc delta
    NvS32 y1dc_delta_q;
    // VP8 Y2 dc delta
    NvS32 y2dc_delta_q;
    // VP8 Y2 ac delta
    NvS32 y2ac_delta_q;
    // VP8 UV dc delta
    NvS32 uvdc_delta_q;
    // VP8 UV ac delta
    NvS32 uvac_delta_q;
    // VP8 refresh entropy probs
    NvU32 refresh_entropy_probs;
    // VP8 segmentation enabled
    NvU32 segmentation_enabled;
    // VP8 update mb segmentation map
    NvU32 update_mb_segmentation_map;
    // VP8 update mb segmentation data
    NvU32 update_mb_segmentation_data;
    // VP8 mb segment abs delta
    NvU32 mb_segment_abs_delta;
    // VP8 segment filter level
    NvS8 seg_filter_level[4];
    // VP8 segment qp index
    NvS8 seg_qindex[4];
    // Frame header field mode_ref_lf_delta_enabled
    NvBool bmode_ref_lf_delta_enabled;
    // Frame header fileld mode_ref_lf_delta_update
    NvBool bmode_ref_lf_delta_update;
}NvMMVideoEncVP8ExtraParams;

typedef enum
{
    /** No transformation */
    NvMMVideoEncRotation_None = 0x0,

    /** Rotation by 90 degrees */
    NvMMVideoEncRotation_90,

    /** Rotation by 180 degrees */
    NvMMVideoEncRotation_180,

    /** Rotation by 270 degrees */
    NvMMVideoEncRotation_270,

    NvMMVideoEncRotation_Force32 = 0x7FFFFFFF
} NvMMVideoEncRotationAngle;
/**
 * @brief Defines the structure for holding the encoding attributes
 * that can be set by SetAttribute() call with NvMMAttributeVideoEnc_Rotation
 */
typedef struct
{
    NvU32   StructSize;
    NvMMVideoEncRotationAngle Rotation;
} NvMMVideoEncRotation;

typedef enum
{
    /** No transformation */
    NvMMVideoEncMirror_None = 0x0,

    /** Rotation by V FLIP  */
    NvMMVideoEncMirror_Vertical,

    /** Rotation by H FLIP */
    NvMMVideoEncMirror_Horizontal,

    /** Rotation by H+V FLIP */
    NvMMVideoEncMirror_Both,

    NvMMVideoEncMirror_Force32 = 0x7FFFFFFF
} NvMMVideoEncMirroring;

/**
 * @brief Defines the structure for holding the encoding attributes
 * that can be set by SetAttribute() call with NvMMAttributeVideoEnc_Mirroring
 */
typedef struct
{
    NvU32   StructSize;
    NvMMVideoEncMirroring Mirror;
} NvMMVideoEncInputMirroring;

/**
 * @brief Defines the structure for holding the encoding attributes
 * that can be set by SetAttribute() call with NvMMAttributeVideoEnc_Lookahead
 */
typedef struct
{
    NvU32   StructSize;
    NvU32   EnableLookahead;
} NvMMVideoEncLookahead;

/**
 * @brief Defines the structure for holding the crop rectangle attributes for
 * H264 Encoder, that can be set by set SetAttribute() call with
 * NvMMAttributeVideoEnc_H264CropRect
 */
typedef struct
{
    NvU32 StructSize;
    NvBool FlagCropRect;
    NvRect Rect;
}NvMMVideoEncH264CropRect;

/**
 * @brief Defines the structure for holding initial QP Values for I and P frames
 * of H264 and Mpeg4 Encoder that can be set by SetAttribute call with
 * NvMMAttributeVideoEnc_InitialQP
 */
 typedef struct
 {
     NvU32 StructSize;
     NvU32 InitialQPI;
     NvU32 InitialQPP;
     NvU32 InitialQPB;
 }NvMMVideoEncInitialQP;

/**
 * @brief Defines the structure for holding QP Range Values for I and P frames
 * of H264 and Mpeg4 Encoder that can be set by SetAttribute call with
 * NvMMAttributeVideoEnc_InitialQP
 */
 typedef struct
 {
     NvU32 StructSize;
    // Minimum QP value for Intra frame
    NvU32   IMinQP;
    // Maximum QP value for Intra frame
    NvU32   IMaxQP;
    // Minimum QP value for P frame
    NvU32   PMinQP;
    // Maximum QP value for P frame
    NvU32   PMaxQP;
    // Minimum QP value for B frame
    NvU32   BMinQP;
    // Maximum QP value for B frame
    NvU32   BMaxQP;
 }NvMMVideoEncQPRange;

/**
 * @brief Defines the Stereo Flags that are sent from the App via an OMX Extension.
 * Sent to the encoder by a SetAttribute() call with NvMMAttributeVideoEnc_H264StereoInfo
 * Read from the encoder by a GetAttribute() call with NvMMAttributeVideoEnc_H264StereoInfo
 */
typedef struct
{
    NvU32   StructSize;
    // Stereo Flags: The bit positions are setup to match what is used in
    // BufferFlags in the NvMMBuffer strutcure. See nvmm.h for more details on the
    // exact bit definitions. Bits [20:15] of StereoFlags are used, rest are unused
    NvU32   StereoFlags;
}NvMMVideoEncH264StereoInfo;

typedef struct
{
    NvU32 StructSize;
    // Bias for Inter Macroblocks, used for Intra-Inter mode decision.
    NvU32 FavorInterBias;
    // Bias for Intra Macroblocks on top of Bias set for 16X16 and 4X4 modes.
    NvU32 FavorIntraBias;
    // Bias for 16X16 mode for Intra macroblocks mode decision.
    NvU32 FavorIntraBias_16X16;
}NvMMH264EncQualityParams;

/**
 * @brief Defines the structure for holding the encoding attributes
 * that can be set by SetAttribute() call with NvMMAttributeVideoEnc_MEmode_MVOnly
 */
typedef struct
{
    NvU32   StructSize;
    // MEmode_MVOnly: Set to get motion vectors only in MEmode
    NvU32   EnableMEmodeMVOnly;
}NvMMVideoEncMEmodeMVOnly;

/**
 * @brief Defines the structure for holding the encoding attributes
 * that can be set by SetAttribute() call with NvMMAttributeVideoEnc_FrameError
 */
typedef struct
{
    NvU32 StructSize;
    // picture_idx: To signal corrupted frame number. Needs to change it as timestamp
    NvU32 picture_idx;
} NvMMVideoEncFrameErrorParams;

/**
 * @brief Defines the structure for holding the encoding attributes
 * that can be set by SetAttribute() call with NvMMAttributeVideoEnc_AvgEncTime
 */
typedef struct
{
    NvU32   StructSize;
    NvU32   MaxFrames;
    NvU32   AvgEncTime;
    NvU32   AvgFetchTime;
}NvMMVideoEncAvgEncTime;

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_VIDEOENC_H
