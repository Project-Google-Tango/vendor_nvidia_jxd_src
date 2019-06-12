/*
 * Copyright (c) 2007-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit:
 *           NvMM Camera types</b>
 *
 * @b Description: Declares types used by NvMM Camera APIs.
 */

#ifndef INCLUDED_NVMM_CAMERA_TYPES_H
#define INCLUDED_NVMM_CAMERA_TYPES_H

/** @defgroup nvmm_camera_types Camera types
 *
 * Types used by the NvMM Camera API.
 *
 * @ingroup nvmm_camera
 * @{
 */

#include "nvcommon.h"
#include "nvos.h"
#include "nvodm_imager.h"
#include "nvrm_surface.h"
#include "nvmm.h"
#include "nvmm_event.h"
#include "nvfxmath.h"
#include "camera_rawdump.h"
#include "nvcamera_isp.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define MAX_BRACKET_CAPTURES 32
#define PCL_MAX_EXPOSURES 2

/**
 * @brief Defines the information concerning camera's event.
*/
typedef struct NvMMCameraEventInfo_
{
    NvU32 structSize;
    NvMMEventType event;

    /**
     * Timestamp of the event in 100 nanosecond units
     */
    NvU64 TimeStamp;
} NvMMCameraEventInfo;


typedef enum NvMMCameraAlgStatus_t {

    /*  Searching indicates that the specified algorithm is still searching for
     *  an optimal set of values.  Convergence has not occurred, but we have
     *  not failed, yet.  If the specified algorithm is in continuous-update
     *  mode, this indicates that fewer than
     *  NvMMCameraLimit_ContinuousConvergeTime milliseconds have elapsed
     *  since the algorithm was previously converged.  This status may be
     *  returned by all of the 3A algorithms.
     */
    NvMMCameraAlgStatus_Searching = 1,

    /*  Converged indicates that the specific algorithm is in a converged
     *  (i.e., steady) state.  This applies to both continuous and single-shot
     *  operation.  This status may be returned by all of the 3A algorithms.
     */
    NvMMCameraAlgStatus_Converged,

    /*  Underexposed indicates that the 3A algorithm has failed to converge
     *  because the image is underexposed.  In this situation, applications
     *  should consider lowering the framerate to allow longer exposure
     *  times (if possible), to turn on a flash / torch to increase the
     *  light intensity, or to increase the exposure time / ISO (in manual
     *  mode).  This status may be returned by all of the 3A algorithms.
     */
    NvMMCameraAlgStatus_Underexposed,

    /*  Overexposed indicates that the 3A algorithm has failed to converge
     *  because the image is overexposed.  In this situation, applications
     *  should consider increasing the framerate to allow shorter exposure
     *  times (if possible), to disable the flash / torch (if enabled),
     *  or to reduce the exposure time / ISO (in manual exposure mode).
     *  This status may be returned by all of the 3A algorithms.
     */
    NvMMCameraAlgStatus_Overexposed,

    /*  NoAutoFocus indicates that the auto-focus algorithm was unable
     *  to find any significant indicators in any of the auto-focus
     *  regions.  Normally this will be caused by over- and/or under-
     *  exposure, and reported as such; however, in some situations
     *  auto-focusing may fail to converge even in properly-exposed
     *  images.  This status will only be returned by the Auto-Focus
     *  algorithm.
     */
    NvMMCameraAlgStatus_NoAutoFocus,

    /*  Timeout indicates that the 3A algorithm has taken too long to
     *  converge.  This will only be returned if the specified algorithm
     *  is in continuous mode, and has been searching for more than
     *  NvMMCameraLimit_ContinuousConvergeTime milliseconds.  The algorithm
     *  will continue searching.  This status will only be returned by
     *  the Auto White Balance algorithm.
     */
    NvMMCameraAlgStatus_Timeout,

    /* The algorithm isn't initialized.
     */
    NvMMCameraAlgStatus_NotInitialized,

    /*  Num should always be the second-to-last enumerant */
    NvMMCameraAlgStatus_Num,

    /*  Max should always be the last enumerant, used to pad to 32 bits */
    NvMMCameraAlgStatus_Max = 0x7FFFFFFFL,
} NvMMCameraAlgStatus;

typedef enum
{
    NvMMCameraTimeoutAction_StartCapture = 1,
    NvMMCameraTimeoutAction_Abort,
    NvMMCameraTimeoutAction_Force32 = 0x7FFFFFFF,
} NvMMCameraTimeoutAction;


#define NVMM_ALGORITHMS(_) \
    _(AutoWhiteBalance, 0), _(AutoExposure, 1), _(AutoFocus, 2), \
    _(BasicFocus, 3)

#define ALG(x, i) NvMMCameraAlgorithms_##x = i
#define FLAG(x, i) NvMMCameraAlgorithmFlags_##x = 1 << NvMMCameraAlgorithms_##x

typedef enum {
    NVMM_ALGORITHMS(ALG),
    NvMMCameraAlgorithms_Count,
    NvMMCameraAlgorithms_Force32 = 0x7FFFFFFF,
} NvMMCameraAlgorithms;

typedef enum {
    NVMM_ALGORITHMS(FLAG),
    NvMMCameraAlgorithmFlags_Mask = (1 << NvMMCameraAlgorithms_Count) - 1,
    NvMMCameraAlgorithmFlags_Force32 = 0x7FFFFFFF,
} NvMMCameraAlgorithmFlags;


/*  Algorithm subtypes */
#define NvMMCameraAlgorithmSubType_None            0
#define NvMMCameraAlgorithmSubType_AFFullRange     (1 << 0)
#define NvMMCameraAlgorithmSubType_AFInfMode       (1 << 1)
#define NvMMCameraAlgorithmSubType_AFMacroMode     (1 << 2)
#define NvMMCameraAlgorithmSubType_TorchDisable    (1 << 3)

#define NvMMCameraAlgorithmSubType_AWBv1           0
#define NvMMCameraAlgorithmSubType_AWBv4           3


#undef ALG
#undef FLAG

typedef enum
{
    NvMMCameraPinState_Enable = 0x1,
    NvMMCameraPinState_Pause,
    NvMMCameraPinState_Disable,

    NvMMCameraPinState_Force32 = 0x7FFFFFFF


}NvMMCameraPinState;

#define NVMMCAMERA_MAX_SENSOR_LIST 20
typedef struct NvMMCameraAvailableSensorsRec
{
    NvS32 NumberOfSensors;
    NvOdmImagerHandle ListOfSensors[NVMMCAMERA_MAX_SENSOR_LIST];
} NvMMCameraAvailableSensors;

typedef struct NvMMCameraFieldOfViewRec
{
    NvF32 HorizontalViewAngle;
    NvF32 VerticalViewAngle;
} NvMMCameraFieldOfView;

typedef struct NvMMCameraConvergeAndLockParamatersRec
{
    NvMMCameraAlgorithmFlags algs;
    NvU32 milliseconds;
    NvBool relock;
    NvU32 algSubType;
} NvMMCameraConvergeAndLockParamaters;

typedef struct NvMMCameraAlgorithmParametersRec
{
    NvBool Enable;      // If NV_FALSE, ignore the rest of this struct
    NvU32 TimeoutMS;    // timeout for this algorithm

    // Start capture even on Timeout, or give up entirely
    NvMMCameraTimeoutAction TimeoutAction;

    // If NV_TRUE, algorithm will continue to check convergence
    // once capture has started, and will attempt to reconverge
    // if conditions change.
    // If NV_FALSE, algorithm will lock once capture has started.
    NvBool ContinueDuringCapture;

    // How often to check convergence (in Hz) once capture has
    // started.  If we are at 30fps, and this is 2Hz, then once
    // every 15 frames we will check convergence, and potentially
    // reconverge.
    // No effect if ContinueDuringCapture is NV_FALSE.
    NvF32 FrequencyOfConvergence;
} NvMMCameraAlgorithmParameters;

// TODO: write a lot of comments explaining how to set these.
typedef struct NvMMCameraCaptureStillRequestRec
{
    NvU32 NumberOfImages;
    NvU32 BurstInterval;
    // If settings need to wait for the sensor/focuser to take effect
    // then it makes the most sense to request a burst capture, but only
    // have the last image be processed and sent to downstream block
    // Images sent will be (N = NumberOfImages - FlushPipeWithCaptureCount)
    // and will be the last N of the sequence.
    NvU32 FlushPipeWithCaptureCount;
    // For each algorithm, any special instructions for what to do
    // before and during capture.
    // Indexed by NvMMCameraAlgorithms enum.
    NvMMCameraAlgorithmParameters AlgorithmControl[NvMMCameraAlgorithms_Count];

    NvU32 NslNumberOfImages;
    NvU32 NslPreCount;
    NvU64 NslTimestamp;
    NvSize Resolution;
} NvMMCameraCaptureStillRequest;

typedef struct NvMMCameraCaptureVideoRequestRec
{
    NvMMCameraPinState State;

    // For each algorithm, any special instructions for what to do
    // before and during capture.
    // Indexed by NvMMCameraAlgorithms enum.
    // AlgorithmControl has no effect if State (above) is not
    // NvMMCameraPinState_Enable.
    NvMMCameraAlgorithmParameters AlgorithmControl[NvMMCameraAlgorithms_Count];
    NvSize Resolution;
} NvMMCameraCaptureVideoRequest;

// TODO: hide? it is only useful for automated testing, but harmless
// for others to use, but do we want to maintain/support?
// This structure gives the driver a specific set of capture
// instructions. Delay for N frames, then capture M frames.
// Used in combination to Preview, it can set up complex
// testing scenarios.
// When used for Still or Video, same behavior expected.
typedef struct {
    NvU32 DelayFrames;
    NvU32 CaptureCount;
    NvU32 BurstInterval;
    NvU32 FlushPipeWithCaptureCount;
    // If anything in AlgorithmControl is enabled, DelayFrames is 0.
    NvMMCameraAlgorithmParameters AlgorithmControl[NvMMCameraAlgorithms_Count];
    NvSize Resolution;
} NvMMCameraCaptureComplexRequest;


/**
 * Defines raw capture request that will used in nvmm camera block
 */
typedef struct NvMMCameraCaptureRawRequestRec
{
    /* Number of raw images */
    NvU32 CaptureCount;

    //TODO: Bug #677895
    //Holding cropping information here is a temporary fix to allow
    //cropping data that is not well supported in nvmm_camera to be
    //pushed through for the only current use case.
    //when nvmm_camera has proper capture crop attributes, these
    //members should be removed.
    NvBool Crop;
    NvRect CropRect;

    /* Filename to dump to */
#define NVMMCAMERA_MAX_FILENAME_LEN (64)
    char Filename[NVMMCAMERA_MAX_FILENAME_LEN];
} NvMMCameraCaptureRawRequest;

/**
 * Defines stream index that will used in nvmm camera block
 */
typedef enum
{
    /* index of input stream */
    NvMMCameraStreamIndex_Input = 0x0, // index starts from 0

    /* index of output Preview stream */
    NvMMCameraStreamIndex_OutputPreview,

    /* index of output stream */
    NvMMCameraStreamIndex_Output,

    /* index of output bayer stream */
    NvMMCameraStreamIndex_OutputBayer,

    /*number of supported streams*/
    NvMMCameraStreamIndex_Count,

    NvMMCameraStreamIndex_Force32 = 0x7FFFFFFF

} NvMMCameraStreamIndex;

typedef NvOdmImagerFocuserCapabilities NvMMCameraFocuserCapabilities;

/**
 * Defines camera focusing parameters
 */

// Definition moved into 'camera_rawdump.h' for external use
typedef CameraRawDump NvCamRawDump;

typedef enum
{
    /* Preview pin */
    NvMMCameraPin_Preview = 0x1,

    /* Still or Video pin */
    NvMMCameraPin_Capture = 0x2,

    NvMMCameraPin_Force32 = 0x7FFFFFFF

}NvMMCameraPin;

typedef struct NvMMCameraSensorModeRec
{
    NvSize Resolution;
    NvF32 FrameRate;
    NvColorFormat ColorFormat;
}NvMMCameraSensorMode;

typedef struct
{
    NvBool bEnable;
    NvF32  nVideoSpeed;
}NvMMCameraVideoSpeed;


#define NVMMCAMERA_M2_BINS 8
#define NVMMCAMERA_M2_CHANNELS 4
#define NVMMCAMERA_M2_SIZE (NVMMCAMERA_M2_BINS*NVMMCAMERA_M2_CHANNELS)
#define NVMMCAMERA_M2_BINPTS_SIZE (NVMMCAMERA_M2_BINS+1)
typedef struct NvMMCameraM2StatsRec
{
    NvU32 Samples[NVMMCAMERA_M2_SIZE];
    NvU32 TotalPerChannel;
    NvU16 BinPoints[NVMMCAMERA_M2_BINPTS_SIZE];
    NvU32 PostSamples[NVMMCAMERA_M2_SIZE];
    NvU32 TotalForPost;
} NvMMCameraM2Stats;

typedef struct NvMMCameraIntermediateResultsRec
{
    NvBool aeHist;
    NvBool aeBusy;
    NvF32 aeDistanceFromGoal;
    NvF32 aeEV;
    NvU32 autoBlackLevel;
} NvMMCameraIntermediateResults;

typedef struct SyncCaptureTimestampRec
{
    NvU64 ElapsedTime;
    NvBool Base;

} SyncCaptureTimestamp;

typedef struct NvMMAEOverrideRec
{
    NvBool AnalogGainsEnable;
    NvF32  AnalogGains[4];
    NvBool DigitalGainsEnable;
    NvF32  DigitalGains[4];
    NvBool ExposureEnable;
    NvU32  NoOfExposures;
    NvF32  Exposures[PCL_MAX_EXPOSURES];
}NvMMAEOverride;

typedef struct NvMMBracketCaptureSettingsRec
{
    NvU32 NumberImages;
     //Ev offset of bracket image
    NvF32 ExpAdj[MAX_BRACKET_CAPTURES];
    // Allows for the use of ISP gain
    // ISP gain must be enabled with AE_USE_ADDITIONAL_ISP_GAIN
    // for this flag to have effect
    NvBool IspGainEnabled[MAX_BRACKET_CAPTURES];
} NvMMBracketCaptureSettings;

/**
 * NvMMCameraResetBufferNegotiation is used with
 * NvMMCameraExtension_ResetBufferNegotiation.
 * * The buffer negotiation will be reset for the specified stream.
 */
typedef struct NvMMCameraResetBufferNegotiationRec
{
    NvMMCameraStreamIndex StreamIndex;

} NvMMCameraResetBufferNegotiation;

// Alias the nvodm typedef, so users of nvmm don't need to #include it also.
typedef NvOdmImagerExpectedValues NvMMCameraImagerExpectedValues,
        *NvMMCameraImagerExpectedValuesPtr;
typedef NvOdmImagerRegionValue NvMMCameraImagerRegionValue;

typedef NvOdmImagerStereoCameraMode NvMMCameraStereoCameraMode;

typedef struct NvMMCameraFaceTrackerRequestRec
{
    NvU32 minFaceSize;
    NvU32 maxFaceSize;
    NvU32 maxFaces;
} NvMMCameraFaceTrackerRequest;

// This is understanding, whether nvmm block is running in
// Still Capture Mode or Video Capture Mode
typedef enum
{
    /* Still Capture Mode*/
    NvMMCameraCaptureMode_StillCapture = 0x1,
    /* Video or Camcoder Mode*/
    NvMMCameraCaptureMode_Video = 0x2,
    NvMMCameraCaptureMode_Force32 = 0x7FFFFFFF
}NvMMCameraCaptureMode;

/**
 * NvMMCameraStereoCameraInfo is used with
 * NvMMCameraAttribute_StereoCameraInfo.
 * The camera driver's output will be configured based on this.
 */
typedef enum
{
    NvMMCameraCaptureType_None,             // No capture.
    NvMMCameraCaptureType_Preview,          // Preview capture for AOHDR
    NvMMCameraCaptureType_Video,            // Video capture (Full frame).
    NvMMCameraCaptureType_Still,            // Still capture.
    NvMMCameraCaptureType_StillBurst,       // Still burst capture.
    NvMMCameraCaptureType_VideoSquashed,    // Video capture (Squashed frame).
    NvMMCameraCaptureType_Force32 = 0x7FFFFFFFL
} NvMMCameraCaptureType;

/**
 * NvMMCameraStereoCaptureInfo is used with
 * NvMMCameraAttribute_StereoCameraInfo. It specifies the format
 * of stereo video or photo to be captured.
 */
typedef struct NvMMCameraStereoCaptureInfoRec
{
    // This shows how stereo capture buffers are configured.
    NvMMStereoType StCapBufferType;
    // The type of stereo capture.
    NvMMCameraCaptureType StCapType;
} NvMMCameraStereoCaptureInfo;

/**
 * NvMMCameraStartFaceDetect is used to start face detection algorithm.
 */
typedef struct NvMMCameraStartFaceDetectRec
{
    NvS32 NumFaces;
    NvS32 Debug;
} NvMMCameraStartFaceDetect;

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_CAMERA_TYPES_H
