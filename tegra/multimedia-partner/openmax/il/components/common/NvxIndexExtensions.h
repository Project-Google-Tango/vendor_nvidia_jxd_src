/* Copyright (c) 2006-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVX_INDEX_EXTENSIONS_H
#define NVX_INDEX_EXTENSIONS_H

#include "OMX_Index.h"
#include "OMX_Other.h"

#include "NVOMX_IndexExtensions.h"
#include "NVOMX_TrackList.h"

/* This file contains definitions for NVIDIA extensions to OpenMAX common to multiple components.
 * Each extension has a define for the string to pass to OMX_GetExtensionIndex to retrieve the OMX_INDEXTYPE id for NVIDIA extension.   When appropriate, the associated structure is also defined in this file.
 */

typedef enum NVX_INDEXTYPE_ENUM {
    /* The bottom of this range is component specific */
    NVX_IndexParamComponentSpecificExtension = (OMX_IndexVendorStartUnused | 0x000000),
    /* start of non domain specific extensions */
    NVX_IndexParamExtensionStartUnused       = (OMX_IndexVendorStartUnused | 0x800000),

// String in NVOMX_IndexExtensions
    NVX_IndexParamFilename,                      /**< reference NVX_PARAM_FILENAME */
    NVX_IndexParamOutputFormat,                  /**reference NVX_PARAM_OUTPUTFORMAT */
    /* audio extensions */
    NVX_IndexParamAudioExtensionStartUnused  = (OMX_IndexVendorStartUnused | 0x900000),

    NVX_IndexConfigAudioDrcProperty          = (OMX_IndexVendorStartUnused | 0x900001),
    NVX_IndexConfigAudioParaEqFilter         = (OMX_IndexVendorStartUnused | 0x900002),
    NVX_IndexConfigAudioGraphicEq            = (OMX_IndexVendorStartUnused | 0x900003),
    NVX_IndexConfigAudioSilenceOutput        = (OMX_IndexVendorStartUnused | 0x900004),
    NVX_IndexParamAudioAc3                   = (OMX_IndexVendorStartUnused | 0x900005),
    NVX_IndexConfigAudioSessionId            = (OMX_IndexVendorStartUnused | 0x900006),
    NVX_IndexParamAudioDts                   = (OMX_IndexVendorStartUnused | 0x900007),
    NVX_IndexConfigAudioCaps                 = (OMX_IndexVendorStartUnused | 0x900008),

    /* video extensions */
    NVX_IndexParamVideoExtensionStartUnused  = (OMX_IndexVendorStartUnused | 0xA00000),
    /* image extension */
    NVX_IndexParamImageExtensionStartUnsued  = (OMX_IndexVendorStartUnused | 0xB00000),
    /* extensions common to image and video */
    NVX_IndexParamIVCommonExtensionStartUnsued = (OMX_IndexVendorStartUnused | 0xC00000),
    /* other domain extensions */
    NVX_IndexParamOtherExtensionStartUnused  = (OMX_IndexVendorStartUnused | 0xD00000),

    /* time domain extensions */
    NVX_IndexParamTimeExtensionStartUnused   = (OMX_IndexVendorStartUnused | 0xE00000),
    /* vendor specific tunneling */
    NVX_IndexParamVendorSpecificTunneling    = (OMX_IndexVendorStartUnused | 0xF00000),
    /* information for component collapsing */
    NVX_IndexConfigDestinationSurface        = (OMX_IndexVendorStartUnused | 0xA00001),

#define NVX_INDEX_CONFIG_TIMESTAMPOVERRIDE "OMX.Nvidia.index.config.tsoverride"
    /* switching between external and internal timestamps */
    NVX_IndexConfigTimestampOverride         = (OMX_IndexVendorStartUnused | 0xA00002),
    /* custom camera context switch notification */
    NVX_IndexConfigCameraContextSwitch       = (OMX_IndexVendorStartUnused | 0xA00003),
#define NVX_INDEX_CONFIG_FRAMESTATTYPE "OMX.Nvidia.index.config.framestats"
    /* custom video frame statistic gather enable/disable option */
    NVX_IndexConfigGatherFrameStats          = (OMX_IndexVendorStartUnused | 0xA00004),
#define NVX_INDEX_CONFIG_CAMERAIMAGER "OMX.Nvidia.index.config.cameraimager"
    NVX_IndexConfigCameraImager              = (OMX_IndexVendorStartUnused | 0xA00005),
    NVX_IndexConfigEncoderStop               = (OMX_IndexVendorStartUnused | 0xA00006),

#define NVX_INDEX_CONFIG_VIDEO_CONFIGINFO "OMX.Nvidia.index.config.obtainconfiginfo"
    NVX_IndexConfigVideoConfigInfo        = (OMX_IndexVendorStartUnused | 0xA00007),

#define NVX_INDEX_CONFIG_VIDEO_CAPABILITIES "OMX.Nvidia.index.config.video.capabilities"
    NVX_IndexConfigVideoCapabilities      = (OMX_IndexVendorStartUnused | 0xA00008),

#define NVX_INDEX_PARAM_COMMON_CAP_PORT_RESOLUTION "OMX.Nvidia.param.common.port.res"
    NVX_IndexParamCommonCapPortResolution = (OMX_IndexVendorStartUnused | 0xA00009),

#define NVX_INDEX_CONFIG_H264_NAL_LEN "OMX.Nvidia.index.config.h264.nal.len"
    NVX_IndexConfigH264NALLen             = (OMX_IndexVendorStartUnused | 0xA0000A),

#define NVX_INDEX_CONFIG_GET_NVMM_BLOCK "OMX.Nvidia.index.config.get.nvmm.block"
    NVX_IndexConfigGetNVMMBlock           = (OMX_IndexVendorStartUnused | 0xA0000B),

// string in NVOMX_IndexExtensions
    NVX_IndexConfigProfile                = (OMX_IndexVendorStartUnused | 0xA0000C),

#define NVX_INDEX_CONFIG_GET_CLOCK "OMX.Nvidia.index.config.get.clock"
    NVX_IndexConfigGetClock               = (OMX_IndexVendorStartUnused | 0xA0000D),

#define NVX_INDEX_CONFIG_RENDERTYPE "OMX.Nvidia.index.config.rendertype"
    NVX_IndexConfigRenderType             = (OMX_IndexVendorStartUnused | 0xA0000E),

    NVX_IndexConfigPreviewEnable          = (OMX_IndexVendorStartUnused | 0xA00010),

    NVX_IndexConfigAutoFrameRate          = (OMX_IndexVendorStartUnused | 0xA00011),
// string in public header
    NVX_IndexConfigAudioOutputType        = (OMX_IndexVendorStartUnused | 0xA00012),
    NVX_IndexParamVideoEncodeProperty     = (OMX_IndexVendorStartUnused | 0xA00014),

    NVX_IndexParamVideoEncodeStringentBitrate  = (OMX_IndexVendorStartUnused | 0xA00015),
    NVX_IndexParamDuration                = (OMX_IndexVendorStartUnused | 0xA00016),
    NVX_IndexParamStreamType              = (OMX_IndexVendorStartUnused | 0xA00017),
    NVX_IndexParamLowMemMode              = (OMX_IndexVendorStartUnused | 0xA00018),
    NVX_IndexConfigUlpMode                = (OMX_IndexVendorStartUnused | 0xA00019),
    NVX_IndexConfigConvergeAndLock        = (OMX_IndexVendorStartUnused | 0xA0001A),
    NVX_IndexConfigQueryMetadata          = (OMX_IndexVendorStartUnused | 0xA0001B),
    NVX_IndexConfigCapturePause           = (OMX_IndexVendorStartUnused | 0xA0001C),
    NVX_IndexParamRecordingMode           = (OMX_IndexVendorStartUnused | 0xA0001D),
    NVX_IndexParamChannelID               = (OMX_IndexVendorStartUnused | 0xA0001E),
    NVX_IndexConfigKeepAspect             = (OMX_IndexVendorStartUnused | 0xA0001F),
    NvxIndexConfigTracklist               = (OMX_IndexVendorStartUnused | 0xA00020),
    NvxIndexConfigTracklistTrack          = (OMX_IndexVendorStartUnused | 0xA00021),
    NvxIndexConfigTracklistDelete         = (OMX_IndexVendorStartUnused | 0xA00022),
    NvxIndexConfigTracklistSize           = (OMX_IndexVendorStartUnused | 0xA00023),
    NvxIndexConfigTracklistCurrent        = (OMX_IndexVendorStartUnused | 0xA00024),

    NvxIndexConfigWindowDispOverride      = (OMX_IndexVendorStartUnused | 0xA00025),

    NVX_IndexConfigPausePlayback          = (OMX_IndexVendorStartUnused | 0xA00026),
    NVX_IndexParamSource                  = (OMX_IndexVendorStartUnused | 0xA00027),

    NVX_IndexConfigCaptureRawFrame        = (OMX_IndexVendorStartUnused | 0xA00028),
    NVX_IndexConfigDisableTimestampUpdates= (OMX_IndexVendorStartUnused | 0xA00029),
    NVX_IndexConfigAudioOnlyHint          = (OMX_IndexVendorStartUnused | 0xA0002A),
    NVX_IndexConfigFileCacheSize          = (OMX_IndexVendorStartUnused | 0xA0002B),
    NvxIndexConfigSingleFrame             = (OMX_IndexVendorStartUnused | 0xA0002C),
    NVX_IndexConfigTemporalTradeOff       = (OMX_IndexVendorStartUnused | 0xA0002D),
    NVX_IndexConfigDecoderConfigInfo      = (OMX_IndexVendorStartUnused | 0xA0002E),
    NVX_IndexParamAudioParams             = (OMX_IndexVendorStartUnused | 0xA0002F),
    NVX_IndexConfigFileCacheInfo          = (OMX_IndexVendorStartUnused | 0xA00030),
    NVX_IndexConfigNumRenderedFrames      = (OMX_IndexVendorStartUnused | 0xA00031),
    NVX_IndexConfigRenderHintMixedFrames  = (OMX_IndexVendorStartUnused | 0xA00032),
    NVX_IndexParamSyncDecodeMode          = (OMX_IndexVendorStartUnused | 0xA00033),

    NVX_IndexParamDeinterlaceMethod       = (OMX_IndexVendorStartUnused | 0xA00034),
    NVX_IndexConfigExternalOverlay        = (OMX_IndexVendorStartUnused | 0xA00035),
    NVX_IndexConfigExposureTimeRange      = (OMX_IndexVendorStartUnused | 0xA00036),
    NVX_IndexConfigISOSensitivityRange    = (OMX_IndexVendorStartUnused | 0xA00037),
    NVX_IndexConfigWhitebalanceCCTRange   = (OMX_IndexVendorStartUnused | 0xA00038),
    NVX_IndexConfigFocuserParameters      = (OMX_IndexVendorStartUnused | 0xA00039),
    NVX_IndexConfigSmartDimmer            = (OMX_IndexVendorStartUnused | 0xA0003A),
    NVX_IndexConfigVirtualDesktop         = (OMX_IndexVendorStartUnused | 0xA0003B),
    NVX_IndexParamLowResourceMode         = (OMX_IndexVendorStartUnused | 0xA0003C),
    NVX_IndexParamRateControlMode         = (OMX_IndexVendorStartUnused | 0xA0003D),
    NVX_IndexConfigStereoRendMode         = (OMX_IndexVendorStartUnused | 0xA0003E),
    NVX_IndexConfigCheckResources         = (OMX_IndexVendorStartUnused | 0xA0003F),
    NVX_IndexConfigRenderHintCameraPreview= (OMX_IndexVendorStartUnused | 0xA00040),
    NVX_IndexConfigVideoEncodeQuantizationRange  = (OMX_IndexVendorStartUnused | 0xA00041),
    NVX_IndexConfigFlashParameters        = (OMX_IndexVendorStartUnused | 0xA00042),
    NVX_IndexConfigVideoStereoInfo        = (OMX_IndexVendorStartUnused | 0xA00043),
    NVX_IndexParamWriterFileSize          = (OMX_IndexVendorStartUnused | 0xA00044),
    NVX_IndexParamUserAgent               = (OMX_IndexVendorStartUnused | 0xA00045),
    NVX_IndexParamUserAgentProf           = (OMX_IndexVendorStartUnused | 0xA00046),
    NVX_IndexParamStreamCount             = (OMX_IndexVendorStartUnused | 0xA00047),
    NVX_IndexConfigHeader                 = (OMX_IndexVendorStartUnused | 0xA00048),
    NVX_IndexConfigDisableBuffConfig      = (OMX_IndexVendorStartUnused | 0xA00049),
    NVX_IndexConfigMp3Enable              = (OMX_IndexVendorStartUnused | 0xA00050),

    NVX_IndexParamVideoEncodeH264QualityParams = (OMX_IndexVendorStartUnused | 0xA00051),
    NVX_IndexParamH264DisableDPB          = (OMX_IndexVendorStartUnused | 0xA00052),
    NVX_IndexConfigExternalAVSync         = (OMX_IndexVendorStartUnused | 0xA00053),
#define NVX_INDEX_PARAM_VDEC_FULL_FRAME_INPUT_DATA "OMX.Nvidia.index.param.vdecfullframedata"
    NVX_IndexParamVideoDecFullFrameData   = (OMX_IndexVendorStartUnused | 0xA00054),
    NVX_IndexConfigForceAspect            = (OMX_IndexVendorStartUnused | 0xA00055),
    NVX_IndexConfigAlgorithmWarmUp        = (OMX_IndexVendorStartUnused | 0xA00056),
    NVX_IndexConfigAribConstraints        = (OMX_IndexVendorStartUnused | 0xA00057),
    NVX_IndexConfigVideoDecodeIFrames     = (OMX_IndexVendorStartUnused | 0xA00058),
    NVX_IndexConfigMVCStitchInfo          = (OMX_IndexVendorStartUnused | 0xA00059),
    NVX_IndexConfigSensorETRange          = (OMX_IndexVendorStartUnused | 0xA0005A),
    NVX_IndexConfigFuseId                 = (OMX_IndexVendorStartUnused | 0xA0005B),
    NVX_IndexConfigAEOverride             = (OMX_IndexVendorStartUnused | 0xA0005C),
    NVX_IndexParamFilterTimestamps        = (OMX_IndexVendorStartUnused | 0xA00060),
// string in NVOMX_DecoderExtensions
    NVX_IndexConfigWaitOnFence            = (OMX_IndexVendorStartUnused | 0xA00061),
#define NVX_INDEX_PARAM_VDEC_FULL_SLICE_INPUT_DATA "OMX.Nvidia.index.param.vdecfullslicedata"
    NVX_IndexParamVideoDecFullSliceData   = (OMX_IndexVendorStartUnused | 0xA00062),
#define NVX_INDEX_PARAM_VIDEO_DISABLE_DVFS "OMX.Nvidia.index.param.videodisabledvfs"
    NVX_IndexParamVideoDisableDvfs        = (OMX_IndexVendorStartUnused | 0xA00063),
    NVX_IndexConfigVideoProtect           = (OMX_IndexVendorStartUnused | 0xA00064),
    NVX_IndexConfigSliceLevelEncode       = (OMX_IndexVendorStartUnused | 0xA00065),
    NVX_IndexParamVideoVP8                = (OMX_IndexVendorStartUnused | 0xA00066),
#define NVX_INDEX_PARAM_VIDEO_MJOLNIR_STREAMING "OMX.Nvidia.index.param.videomjolnirstreaming"
    NVX_IndexParamVideoMjolnirStreaming   = (OMX_IndexVendorStartUnused | 0xA00067),
    NVX_IndexParamSkipFrame               = (OMX_IndexVendorStartUnused | 0xA00068),
    NVX_IndexConfigVideoEncodeLastFrameQP = (OMX_IndexVendorStartUnused | 0xA00069),
#define NVX_INDEX_PARAM_VIDEO_DRC_OPTZ "OMX.Nvidia.index.param.videodrcoptimzation"
    NVX_IndexParamVideoDRCOptimization    = (OMX_IndexVendorStartUnused | 0xA0006A),
    NVX_IndexParamVideoPostProcessType    = (OMX_IndexVendorStartUnused | 0xA0006B),
#define NVX_INDEX_PARAM_SYNC_PT_IN_BUFFER "OMX.Nvidia.index.param.useSyncPtInNativeBuffer"
    NVX_IndexParamUseSyncFdFromNativeBuffer = (OMX_IndexVendorStartUnused | 0xA0006C),
    NVX_IndexConfigUseNvBuffer2           = (OMX_IndexVendorStartUnused | 0xA0006D),


    NVX_IndexConfigExifInfo               = (OMX_IndexVendorStartUnused | 0xB00001),
    NVX_IndexConfigEncodeExifInfo         = (OMX_IndexVendorStartUnused | 0xB00002),
    NVX_IndexConfigEncodeGPSInfo          = (OMX_IndexVendorStartUnused | 0xB00003),
    NVX_IndexConfigEncodeInterOpInfo      = (OMX_IndexVendorStartUnused | 0xB00004),
    NVX_IndexConfigThumbnailQF            = (OMX_IndexVendorStartUnused | 0xB00005),
    NVX_IndexConfigThumbnailEnable        = (OMX_IndexVendorStartUnused | 0xB00006),
    NVX_IndexConfigJpegInfo               = (OMX_IndexVendorStartUnused | 0xB00007),
    NVX_IndexParamThumbnailQTable         = (OMX_IndexVendorStartUnused | 0xB00008),
    NVX_IndexConfigThumbnailMode          = (OMX_IndexVendorStartUnused | 0xB00009),

    NVX_IndexConfigThumbnail              = (OMX_IndexVendorStartUnused | 0xC00001),
    NVX_IndexConfigCustomColorFormt       = (OMX_IndexVendorStartUnused | 0xC00002),
    NVX_IndexConfigAndroidWindow          = (OMX_IndexVendorStartUnused | 0xC00003),
#define NVX_INDEX_PARAM_ENABLE_ANB "OMX.google.android.index.enableAndroidNativeBuffers"
    NVX_IndexParamEnableAndroidBuffers    = (OMX_IndexVendorStartUnused | 0xC00004),
#define NVX_INDEX_PARAM_USE_ANB "OMX.google.android.index.useAndroidNativeBuffer"
    NVX_IndexParamUseAndroidNativeBuffer  = (OMX_IndexVendorStartUnused | 0xC00005),
#define NVX_INDEX_PARAM_GET_ANB_USAGE "OMX.google.android.index.getAndroidNativeBufferUsage"
    NVX_IndexParamGetAndroidNativeBufferUsage = (OMX_IndexVendorStartUnused | 0xC00006),
#define NVX_INDEX_PARAM_STORE_METADATA_BUFFER "OMX.google.android.index.storeMetaDataInBuffers"
    NVX_IndexParamStoreMetaDataBuffer     = (OMX_IndexVendorStartUnused | 0xC00007),
#define NVX_INDEX_PARAM_ENABLE_TIMELAPSE_VIEW "OMX.Nvidia.index.param.timelapse"
    NVX_IndexParamTimeLapseView           = (OMX_IndexVendorStartUnused | 0xC00008),
    NVX_IndexParamUseLowBuffer            = (OMX_IndexVendorStartUnused | 0xC00009),
    NVX_IndexConfigCameraPreview          = (OMX_IndexVendorStartUnused | 0xC0000A),
#define NVX_INDEX_PARAM_EMBEDRMSURACE "OMX.Nvidia.index.param.embedrmsurface"
    NVX_IndexParamEmbedRmSurface          = (OMX_IndexVendorStartUnused | 0xC0000A),
#define NVX_INDEX_PARAM_EMBEDMMBUFFER "OMX.Nvidia.index.param.embedmmbuffer"
    NVX_IndexParamEmbedMMBuffer           = (OMX_IndexVendorStartUnused | 0xC0000B),
#define NVX_INDEX_PARAM_ENCMAXCLOCK "OMX.Nvidia.index.param.encmaxclock"
    NVX_IndexParamEncMaxClock             = (OMX_IndexVendorStartUnused | 0xC0000C),
#define NVX_INDEX_PARAM_ENC2DCOPY "OMX.Nvidia.index.param.enc2dcopy"
    NVX_IndexParamEnc2DCopy               = (OMX_IndexVendorStartUnused | 0xC0000D),
    NVX_IndexParamUseNativeBufferHandle   = (OMX_IndexVendorStartUnused | 0xC0000E),
    NVX_IndexParamCodecCapability         = (OMX_IndexVendorStartUnused | 0xC0000F),
    NVX_IndexParamAudioCodecCapability    = (OMX_IndexVendorStartUnused | 0xC00010),
    NVX_IndexConfigVideo2DProcessing      = (OMX_IndexVendorStartUnused | 0xC00011),
#define NVX_INDEX_CONFIG_VIDEO_SUPER_FINE_QUALITY "OMX.Nvidia.index.config.superFineQuality"
    NVX_IndexConfigVideoSuperFineQuality  = (OMX_IndexVendorStartUnused | 0xC00012),
#define NVX_INDEX_PARAM_INSERT_SPSPPS_AT_IDR "OMX.google.android.index.prependSPSPPSToIDRFrames"
    NVX_IndexParamInsertSPSPPSAtIDR       = (OMX_IndexVendorStartUnused | 0xC00013),
    NVX_IndexParamNativeWindowData        = (OMX_IndexVendorStartUnused | 0xC00014),
    NVX_IndexConfigOverlayIndex           = (OMX_IndexVendorStartUnused | 0xc00015),
    NVX_IndexConfigAllowSecondaryWindow   = (OMX_IndexVendorStartUnused | 0xc00016),
    NVX_IndexConfigVideoDecodeState       = (OMX_IndexVendorStartUnused | 0xC00017),

#define NVX_INDEX_PARAM_SENSORNAME "OMX.Nvidia.index.param.sensor.name"
    /* camera sensor name */
    NVX_IndexParamSensorName              = (OMX_IndexVendorStartUnused | 0xD00001),

    NVX_IndexParam3GPMuxBufferConfig         ,

    NVX_IndexConfigCameraRawCapture          ,

    NVX_IndexConfigIspData                   ,

    NVX_IndexConfigFlicker                   ,

    NVX_IndexConfigPreCaptureConverge        ,
    NVX_IndexConfigFocusRegionsRect          ,
    NVX_IndexConfigCameraTestPattern         ,


    NVX_IndexConfigDZScaleFactorMultiplier   ,
    NVX_IndexConfig3GPMuxGetAVRecFrames      ,
    NVX_IndexConfigHue                       ,
    NVX_IndexParamSensorId                   ,
    NVX_IndexConfigEdgeEnhancement           ,
    NVX_IndexConfigProgramsList              ,
    NVX_IndexConfigCurrentProgram            ,
    NVX_IndexConfigFocusRegionsSharpness     ,
    NVX_IndexConfigFocuserCapabilities       ,
    NVX_IndexConfigFocuserInfo               ,
    NVX_IndexConfigAutofocusSpeed            ,
    NVX_IndexParamMaxFrames                  ,
    NVX_IndexConfigSmoothZoomTime            ,
    NVX_IndexConfigZoomAbort                 ,
    NVX_IndexConfigSensorModeList            ,
    NVX_IndexConfigSplitFilename             ,
    NVX_IndexConfigUseNvBuffer               ,
    NVX_IndexParamTempFilePath               ,
    NVX_IndexParamAllocateRmSurf             ,
    NVX_IndexParamAvailableSensors           ,
    NVX_IndexConfigPreviewFrameCopy          ,
    NVX_IndexConfigStillConfirmationFrameCopy,
    NVX_IndexConfigCameraConfiguration       ,
    NVX_IndexConfigCILThreshold              ,
    NVX_IndexConfigStillFrameCopy            ,
    NVX_IndexConfigStereoCapable             ,
    NVX_IndexConfigSceneBrightness           ,
    NVX_IndexParamStereoCameraMode           ,
    NVX_IndexParamStereoCaptureInfo          ,
    NVX_IndexConfigCustomizedBlockInfo       ,
    NVX_IndexConfigAdvancedNoiseReduction    ,
    NVX_IndexConfigBayerGains                ,
    NVX_IndexConfigGainRange                 ,
    NVX_IndexConfigVideoFrameDataConversion  ,
    NVX_IndexConfigCapturePriority           ,
    NVX_IndexConfigLensPhysicalAttr          ,
    NVX_IndexParamSensorMode                 ,
    NVX_IndexConfigFocusDistances            ,
    NVX_IndexConfigManualTorchAmount         ,
    NVX_IndexConfigManualFlashAmount         ,
    NVX_IndexConfigAutoFlashAmount           ,
    NVX_IndexConfigCommonScaleType           ,
    NVX_IndexParamPreScalerEnableForCapture  ,
    NVX_IndexConfigFrameCopyColorFormat      ,
    NVX_IndexConfigExposureRegionsRect       ,
    NVX_IndexParamSensorGuid                 ,
    NVX_IndexConfigNSLBuffers                ,
    NVX_IndexConfigCombinedCapture           ,
    NVX_IndexConfigBurstSkipCount            ,

    NVX_IndexConfigBracketCapture            ,
    NVX_IndexConfigContinuousAutoFocus       ,
    NVX_IndexConfigExposureTime              ,
    NVX_IndexConfigConcurrentRawDumpFlag     ,

    NVX_IndexConfigFocusPosition             ,
    NVX_IndexConfigColorCorrectionMatrix     ,

    NVX_IndexConfigSensorIspSupport          ,

    NVX_IndexConfigCameraVideoSpeed          ,

    NVX_IndexConfigCustomPostview            ,

    NVX_IndexConfigMaxOutputChannels         ,

    NVX_IndexConfigRemainingStillImages      ,
    NVX_IndexConfigNoiseReductionV1          ,
    NVX_IndexConfigNoiseReductionV2          ,

    NVX_IndexConfigAudioDualMonoOuputMode    ,

    NVX_IndexConfigLowLightThreshold         ,
    NVX_IndexConfigMacroModeThreshold        ,
    NVX_IndexConfigContinuousAutoFocusPause  ,
    NVX_IndexConfigContinuousAutoFocusState  ,
    NVX_IndexConfigFocusMoveMsg              ,

    NVX_IndexConfigFDLimit                   ,
    NVX_IndexConfigFDMaxLimit                ,
    NVX_IndexConfigFDResult                  ,
    NVX_IndexConfigFDDebug                   ,
    NVX_IndexConfigCaptureMode               ,
    NVX_IndexParamPreviewMode                ,
    NVX_IndexConfigNSLSkipCount              ,
    NVX_IndexConfigStillPassthrough          ,
    NVX_IndexParamSurfaceLayout              ,
    NVX_IndexConfigConcurrentCaptureRequests ,
    NVX_IndexConfigCaptureSensorModePref     ,
    NVX_IndexConfigSensorPowerOn             ,
    NVX_IndexConfigAllocateRemainingBuffers  , /* deprecated */
    NVX_IndexConfigFastBurstEn               ,
    NVX_IndexConfigCameraMode                ,
    NVX_IndexConfigForcePostView             ,

    NVX_IndexParamRawOutput                  ,
    NVX_IndexConfigDeviceRotate              ,

    PV_OMX_COMPONENT_CAPABILITY_TYPE_INDEX   = 0xFF7A347,
    /* Drm specific extentions */
    NVX_IndexParamLicenseChallenge          = (OMX_IndexVendorStartUnused | 0xE00001),
    NVX_IndexParamLicenseResponse           = (OMX_IndexVendorStartUnused | 0xE00002),
    NVX_IndexParamDeleteLicense             = (OMX_IndexVendorStartUnused | 0xE00003),
    NVX_IndexExtraDataEncryptionMetadata    = (OMX_IndexVendorStartUnused | 0xE00004),
    NVX_IndexExtraDataInitializationVector  = (OMX_IndexVendorStartUnused | 0xE00005),
    NVX_IndexExtraDataInitializationOffset  = (OMX_IndexVendorStartUnused | 0xE00006),
    NVX_IndexConfigSetSurface               = (OMX_IndexVendorStartUnused | 0xE00007),

    /* vendor specific buffer formats */
    NVX_OTHER_FormatStart                   = OMX_OTHER_FormatVendorReserved,
#define NVX_OTHER_FORMAT_BYTESTREAM "OMX.Nvidia.format.bytestream" /**<  format type for raw byte streams */
    NVX_OTHER_FormatByteStream
} NVX_INDEXTYPE;

typedef enum
{
    ENVX_TRANSACTION_DEFAULT,           /**< Normal buffer transactions  */
    ENVX_TRANSACTION_GFSURFACES,        /**< Pass GFSDK surface payloads */
    ENVX_TRANSACTION_AUDIO,
    ENVX_TRANSACTION_CLOCK,
    ENVX_TRANSACTION_NVMM_TUNNEL,       /**< NvMM supported communication */
    ENVX_TRANSACTION_NONE               /**< No transaction required     */
}ENvxTunnelTransactionType;

typedef struct NVX_PARAM_NVIDIATUNNEL
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32                     nPort;              /**< [in] Port to query/set */
    OMX_BOOL                    bNvidiaTunnel;      /**< NVIDIA tunneling supported? */
    ENvxTunnelTransactionType   eTransactionType;   /**< If supported, what type of transaction? */
} NvxParamNvidiaTunnel;

typedef struct NVX_CONFIG_DEST_SURF
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    void *pSurfaces[2];
} NvxConfigDestSurface;

typedef struct NVX_CONFIG_TIMESTAMPTYPE
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bOverrideTimestamp;
} NVX_CONFIG_TIMESTAMPTYPE;

// Specify VI rotation property, auto-blt, stop camera/VI, etc
#define NVX_CAMERA_CONTEXT_ACTION_STOP         0
#define NVX_CAMERA_CONTEXT_ACTION_RESTART      1
#define NVX_CAMERA_CONTEXT_ACTION_AUTOBLT      2
#define NVX_CAMERA_CONTEXT_ACTION_ROTATE       3
#define NVX_CAMERA_CONTEXT_ACTION_CAPTURE      4
#define NVX_CAMERA_CONTEXT_ACTION_COMPLETE     5
#define NVX_CAMERA_CONTEXT_ACTION_TRUEDIM      6
#define NVX_CAMERA_CONTEXT_ACTION_STARTVIP     7
#define NVX_CAMERA_CONTEXT_ACTION_STARTNOW     8
#define NVX_CAMERA_CONTEXT_ACTION_SETUPOVERLAY 9
#define NVX_CAMERA_CONTEXT_ACTION_TRUEDIM_DEC 10

typedef struct NVX_CONFIG_CAMERACONTEXTTYPE
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 eCameraAction;     /**< Specifies what type of action to take */
    OMX_U32 nValue;            /**< If appropriate, contains value corresponding to action */
    OMX_U32 nValue2;
    void *pPtrVal;
    float fVal;
    float fVal2;
} NVX_CONFIG_CAMERACONTEXTTYPE;

// Sets which camera to use.  Switches a component to internal/external camera.
typedef struct NVX_CONFIG_CAMERAIMAGERTYPE
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nImagerIndex;       /**< Valid values 0, 1 */
} NVX_CONFIG_CAMERAIMAGERTYPE;

typedef struct NVX_CONFIG_FRAMESTATTYPE
{
    OMX_BOOL bEnable;           /**<Enable/Disable Frame statistic gathering */
    OMX_BOOL bRawFps;           /**<Enable/Disable Raw frame statistic gathering */
    OMX_BOOL bWriteOnceAtEnd;   /**<Whether to dump data continuously or wait till end */
} NVX_CONFIG_FRAMESTATTYPE;




// Customized structures for compatibilty with outside vendor extensions

#define NVX_VIDEO_CONFIGINFO_SIZE 40
typedef struct NVX_VIDEO_CONFIG_VIDEO_CONFIGINFOTYPE
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U16 nBytes;
    OMX_U8  nConfigInfo[NVX_VIDEO_CONFIGINFO_SIZE];
} NVX_VIDEO_CONFIG_VIDEO_CONFIGINFOTYPE;

typedef struct NVX_VIDEO_CONFIG_H263_PROPERTIES
{
    OMX_VIDEO_CODINGTYPE      eCodec;
    OMX_VIDEO_H263PROFILETYPE eProfile;
    OMX_VIDEO_H263LEVELTYPE   eLevel;
    OMX_U16                   nWidth;
    OMX_U16                   nHeight;
} NVX_VIDEO_CONFIG_H263_PROPERTIES;

typedef struct NVX_VIDEO_CONFIG_AVC_PROPERTIES
{
    OMX_VIDEO_CODINGTYPE       eCodec;
    OMX_U16                    nWidth;
    OMX_U16                    nHeight;
    OMX_VIDEO_MOTIONVECTORTYPE eMotionType;
} NVX_VIDEO_CONFIG_AVC_PROPERTIES;

typedef struct NVX_VIDEO_CONFIG_H264NALLEN
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;

    OMX_U32 nNalLen;
} NVX_VIDEO_CONFIG_H264NALLEN;

typedef struct NVX_VIDEO_CONFIG_CAPABILITIES
{
    OMX_VIDEO_CODINGTYPE eCodec;
    OMX_U32 nWidth;
    OMX_U32 nHeight;
    OMX_U32 nBitrate;
    OMX_U32 nFps;
    union
    {
        OMX_VIDEO_H263PROFILETYPE  eH263Profile;
        OMX_VIDEO_MPEG4PROFILETYPE eMpeg4Profile;
        OMX_VIDEO_AVCPROFILETYPE   eAvcProfile;
        OMX_VIDEO_WMVFORMATTYPE    eWmvFormat;
        OMX_VIDEO_RVFORMATTYPE     eRvFormat;
    } profile;
    union
    {
        OMX_VIDEO_H263LEVELTYPE  eH263Level;
        OMX_VIDEO_MPEG4LEVELTYPE eMpeg4Level;
        OMX_VIDEO_AVCLEVELTYPE   eAvcLevel;
    } level;
} NVX_VIDEO_CONFIG_CAPABILITIES;

/**
* extension for querying camera resolutions
*
* STRUCT MEMBERS:
* nSize           : Size of the structure in bytes
* nVersion        : OMX specification version information
* nPortIndex      : Port that this structure applies to
* bOneShot        : Port's one shot attribute
* nIndex          : resolution index want to query
* sResolution     : resolution capacity
* xMaxDigitalZoom : max digital zoom supported in this resolution
*/
typedef struct NVX_PARAM_COMMON_CAP_PORTRESOLUTION
{
    OMX_U32           nSize;
    OMX_VERSIONTYPE   nVersion;
    OMX_U32           nPortIndex;
    OMX_BOOL          bOneShot;
    OMX_U32           nIndex;
    OMX_FRAMESIZETYPE sResolution;
    OMX_S32           xMaxDigitalZoom;
} NVX_PARAM_COMMON_CAP_PORTRESOLUTION;

typedef struct NVX_CONFIG_GETNVMMBLOCK
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;

    void *pNvMMTransform;
    int nNvMMPort;
} NVX_CONFIG_GETNVMMBLOCK;

typedef struct NVX_CONFIG_GETCLOCK
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;

    OMX_HANDLETYPE hClock;
} NVX_CONFIG_GETCLOCK;


typedef enum NVX_RENDERTARGETTYPE
{
    OMX_RENDERTARGET_NativeWindow,
    OMX_RENDERTARGET_Max = 0x7FFFFFFF
} NVX_RENDERTARGETTYPE;

typedef struct NVX_CONFIG_RENDERTARGETTYPE
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;

    NVX_RENDERTARGETTYPE eType;
    OMX_NATIVE_WINDOWTYPE hWindow;
    OMX_NATIVE_DEVICETYPE hDevice;
} NVX_CONFIG_RENDERTARGETTYPE;

typedef struct PV_OMXComponentCapabilityFlagsType
{
    ////////////////// OMX COMPONENT CAPABILITY RELATED MEMBERS
    OMX_BOOL iIsOMXComponentMultiThreaded;
    OMX_BOOL iOMXComponentSupportsExternalOutputBufferAlloc;
    OMX_BOOL iOMXComponentSupportsExternalInputBufferAlloc;
    OMX_BOOL iOMXComponentSupportsMovableInputBuffers;
    OMX_BOOL iOMXComponentSupportsPartialFrames;
    OMX_BOOL iOMXComponentUsesNALStartCodes;
    OMX_BOOL iOMXComponentCanHandleIncompleteFrames;
    OMX_BOOL iOMXComponentUsesFullAVCFrames;

} PV_OMXComponentCapabilityFlagsType;


#endif /* NVX_INDEX_EXTENSIONS_H */
