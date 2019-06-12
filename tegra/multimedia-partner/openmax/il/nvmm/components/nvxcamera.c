/*
 * Copyright (c) 2006-2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** NvxCamera.c */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

#include <OMX_Core.h>
#include "common/NvxTrace.h"
#include "components/common/NvxComponent.h"
#include "components/common/NvxCompReg.h"
#include "components/common/NvxPort.h"
#include "nvmm/common/NvxHelpers.h"
#include "nvmm/components/common/nvmmtransformbase.h"

#include "nvddk_2d_v2.h"
#include "nvmm_digitalzoom.h"
#include "nvodm_imager.h"
#include "nvodm_query_discovery.h"
#include "nvos.h"
#include "nvmm_camera.h"
#include "nvassert.h"
#include "nvutil.h"
#include "camerautil.h"
#include "camera_rawdump.h"
#include "nvmm_usbcamera.h"
#include "nvxcamera.h"
#if USE_ANDROID_NATIVE_WINDOW
#include "nvxandroidbuffer.h"
#endif
#include "ft/fd_api.h"

/* =========================================================================
 *                     D E F I N E S
 * ========================================================================= */

// DEBUG defines. TO DO: fix this
#if USE_ANDROID_NATIVE_WINDOW
#define CAMERA_MAX_OUTPUT_BUFFERS_PREVIEW      2
#else
#define CAMERA_MAX_OUTPUT_BUFFERS_PREVIEW      8
#endif
#define CAMERA_MAX_OUTPUT_BUFFERS_THUMBNAIL    4
#define CAMERA_MAX_OUTPUT_BUFFERS_STILL        4
#define CAMERA_MAX_OUTPUT_BUFFERS_VIDEO        6
#define CAMERA_MIN_OUTPUT_BUFFERSIZE_PREVIEW   80 * 1024 // 100*1024
#define CAMERA_MIN_OUTPUT_BUFFERSIZE_CAPTURE   1024 // 320*240*4
#define MAX_OUTPUT_BUFFERSIZE (640*480*3)/2

#define CAMERA_MAX_INPUT_BUFFERS               2
#define CAMERA_MIN_INPUT_BUFFERSIZE            (11 * 0x100000)

#define CAMERA_DEFAULT_OUTPUT_WIDTH            176
#define CAMERA_DEFAULT_OUTPUT_HEIGHT           144
#define CAMERA_DEFAULT_INPUT_WIDTH             2592
#define CAMERA_DEFAULT_INPUT_HEIGHT            1944

#if USE_ANDROID_NATIVE_WINDOW
#define CAMERA_MAX_PREVIEW_WIDTH               1920
#define CAMERA_MAX_PREVIEW_HEIGHT              1088
#endif

#define NVXCAMERA_ABORT_BUFFERS_TIMEOUT        5000
#define NVXCAMERA_NEGOTIATE_BUFFERS_TIMEOUT    5000
#define USB_CAMERA_SENSOR_ID_START             2

#define NVCAMERA_CONF_FILE_PATH                "/etc/nvcamera.conf"

static const NvU32 s_nvx_num_ports               = 6;
static const NvU32 s_nvx_port_preview            = 0;
static const NvU32 s_nvx_port_still_capture      = 1;
static const NvU32 s_nvx_port_clock              = 2;
static const NvU32 s_nvx_port_thumbnail          = 3;
static const NvU32 s_nvx_port_raw_input          = 4;
static const NvU32 s_nvx_port_video_capture      = 5;

static const NvU32 s_nvx_num_ports_sc            = 3;
static const NvU32 s_nvx_num_ports_usbc          = 2;

static const NvU32 s_nvx_num_ports_dz             = 6;
static const NvU32 s_nvx_port_dz_input_preview    = NvMMDigitalZoomStreamIndex_InputPreview;
static const NvU32 s_nvx_port_dz_input_capture    = NvMMDigitalZoomStreamIndex_Input;
static const NvU32 s_nvx_port_dz_output_preview   = NvMMDigitalZoomStreamIndex_OutputPreview;
static const NvU32 s_nvx_port_dz_output_still     = NvMMDigitalZoomStreamIndex_OutputStill;
static const NvU32 s_nvx_port_dz_output_video     = NvMMDigitalZoomStreamIndex_OutputVideo;
static const NvU32 s_nvx_port_dz_output_thumbnail = NvMMDigitalZoomStreamIndex_OutputThumbnail;

#define INIT_STRUCT(_X_, pNvComp)       \
    NvOsMemset(&(_X_), 0, sizeof(_X_)); \
    (((_X_).nSize = sizeof(_X_)), (_X_).nVersion = (pNvComp)->oSpecVersion)

/*
 * Helper macro to compute time difference between "from" and "to" readings
 * taking into account possible 32-bit values wrap-around.
 * "to" reading is assumed to be taken later than "from" reading.
 */
#define OMX_TIMEDIFF(from,to) \
           (((from) <= (to))? ((to)-(from)):((NvU32)~0-((from)-(to))+1))

typedef enum {
    CameraDZOutputs_Preview = 0,
    CameraDZOutputs_Still = NvMMDigitalZoomStreamIndex_OutputStill -
        NvMMDigitalZoomStreamIndex_OutputPreview,
    CameraDZOutputs_Video = NvMMDigitalZoomStreamIndex_OutputVideo -
        NvMMDigitalZoomStreamIndex_OutputPreview,
    CameraDZOutputs_Thumb = NvMMDigitalZoomStreamIndex_OutputThumbnail -
        NvMMDigitalZoomStreamIndex_OutputPreview,
    CameraDZOutputs_Default = CameraDZOutputs_Still,
    CameraDZOutputs_Num = 4,
} CameraDZOutputs;

// The NvCameraIspFocusControl is identical to the OMX definitions
NV_CT_ASSERT(NvCameraIspFocusControl_On == OMX_IMAGE_FocusControlOn);
NV_CT_ASSERT(NvCameraIspFocusControl_Off == OMX_IMAGE_FocusControlOff);
NV_CT_ASSERT(NvCameraIspFocusControl_Auto == OMX_IMAGE_FocusControlAuto);
NV_CT_ASSERT(NvCameraIspFocusControl_AutoLock == OMX_IMAGE_FocusControlAutoLock);

/* Camera State information */
typedef struct _SNvxCameraData
{
    OMX_BOOL bInitialized;
    OMX_BOOL bErrorReportingEnabled;
    OMX_BOOL bTransformDataInitialized;

    NvMMBlockType oBlockType;

    SNvxNvMMTransformData oSrcCamera;
    SNvxNvMMTransformData oDigitalZoom;

    OMX_BOOL bTunnelingPreview;
    OMX_BOOL bTunnelingCapture;

    char SensorName[NVODMSENSOR_IDENTIFIER_MAX];
    OMX_U32 nSurfWidthPreview;
    OMX_U32 nSurfHeightPreview;
    OMX_U32 nSurfWidthStillCapture;
    OMX_U32 nSurfHeightStillCapture;
    OMX_U32 nSurfWidthVideoCapture;
    OMX_U32 nSurfHeightVideoCapture;
    OMX_U32 nSurfWidthThumbnail;
    OMX_U32 nSurfHeightThumbnail;
    OMX_U32 nSurfWidthInput;
    OMX_U32 nSurfHeightInput;

    NvOdmImagerSensorMode HostInputMode;

    OMX_PARAM_SENSORMODETYPE SensorModeParams;
    OMX_U32 nCaptureCount;
    OMX_U32 nBurstInterval;

    OMX_U32 rawDumpFlag;

    OMX_CONFIG_BOOLEANTYPE               bCapturing;                // OMX_IndexConfigCapturing
    OMX_CONFIG_BOOLEANTYPE               bAutoPauseAfterCapture;    // OMX_IndexAutoPauseAfterCapture
    NVX_CONFIG_AUTOFRAMERATE             oAutoFrameRate;            // NVX_IndexConfigAutoFrameRate
    NVX_CONFIG_CONVERGEANDLOCK           oConvergeAndLock;          // NVX_IndexConfigConvergeAndLock
    OMX_BOOL                             bStab;                     // OMX_IndexConfigCommonFrameStabilisation
    NVX_CONFIG_PRECAPTURECONVERGE        oPrecaptureConverge;       // NVX_IndexConfigPreCaptureConverge
    OMX_CONFIG_RECTTYPE                  oCropPreview;              // OMX_IndexConfigCommonOutputCrop, s_nvx_port_preview
    OMX_CONFIG_RECTTYPE                  oCropStillCapture;         // OMX_IndexConfigCommonOutputCrop, s_nvx_port_still_capture
    OMX_CONFIG_RECTTYPE                  oCropVideoCapture;         // OMX_IndexConfigCommonOutputCrop, s_nvx_port_video_capture
    OMX_CONFIG_RECTTYPE                  oCropThumbnail;            // OMX_IndexConfigCommonOutputCrop, s_nvx_port_thumbnail
    OMX_S32                              MeteringMatrix[3][3];
    OMX_MIRRORTYPE                       Mirror[CameraDZOutputs_Num];
    OMX_S32                              Rotation[CameraDZOutputs_Num];
    OMX_BOOL                             bUseUSBCamera;             // to check if USB CAMERA is enabled

    NvBool AlgorithmWarmedUp;   // NVX_IndexConfigAlgorithmWarmUp
    NvBool PreviewEnabled;      // NVX_IndexConfigPreviewEnable
    NvBool ThumbnailEnabled;
    NvBool Passthrough;

    NVX_CONFIG_FocusRegionsRect FocusRegions;
    NVX_CONFIG_ExposureRegionsRect ExposureRegions;

    NvCameraIspMatrix ccm;

    NvOsSemaphoreHandle oStreamBufNegotiationSem;

    NvU32 s_nvx_port_sc_input;
    NvU32 s_nvx_port_sc_output_preview;
    NvU32 s_nvx_port_sc_output_capture;

    // This is the buffer pool that keeps the buffers returned from nvmm_camera block.
    NvMMBuffer *EmptyBuffers[MAX_NVMM_BUFFERS];
    OMX_STRING pConfigFilePath;
    NvU32 nCameraSource;
    NvRmSurfaceLayout VideoSurfLayout;
} SNvxCameraData;

static OMX_ERRORTYPE NvxCameraOpen(SNvxCameraData *pNvxCamera, NvxComponent *pComponent);
static OMX_ERRORTYPE NvxCameraClose(SNvxCameraData *pNvxCamera);
static OMX_ERRORTYPE NvxCameraFillThisBuffer(NvxComponent *pNvComp, OMX_BUFFERHEADERTYPE *pBufferHdr);
#if USE_ANDROID_NATIVE_WINDOW
static OMX_ERRORTYPE NvxCameraFreeBuffer(NvxComponent *pNvComp, OMX_BUFFERHEADERTYPE *pBufferHdr);
static OMX_ERRORTYPE NvxCameraFlush(NvxComponent *pNvComp, OMX_U32 nPort);
#endif
static OMX_ERRORTYPE NvxCameraEmptyThisBuffer(NvxComponent *pComponent, OMX_BUFFERHEADERTYPE* pBufferHdr, OMX_BOOL *bHandled);
static OMX_ERRORTYPE NvxCameraPreChangeState(NvxComponent *pNvComp,
                                             OMX_STATETYPE oNewState);
static NvError CameraPreviewEnableComplex(SNvxCameraData *pNvxCamera, NvBool Enable, NvU32 Count, NvU32 Delay);
static OMX_ERRORTYPE NvxResetCameraStream(SNvxCameraData *pNvxCamera, NvU32 streamNo);
static OMX_ERRORTYPE NvxResetDZStream(SNvxCameraData *pNvxCamera, NvU32 streamNo);
static OMX_ERRORTYPE NvxReturnBuffersToCamera(SNvxCameraData *pNvxCamera, NvU32 streamNo);

static NvError
CameraTakePictureComplex(
    SNvxCameraData *pNvxCamera,
    NvU32 NslCount,
    NvU32 NslPreCount,
    NvU64 NslTimestamp,
    NvU32 Count,
    NvU32 Delay,
    NvU32 SkipCount);

static NvError CameraVideoCaptureComplex(SNvxCameraData *pNvxCamera, NvBool Enable, NvU32 Count, NvU32 Delay, NvBool NoStreamEndEvent);

static NvError s_SetCrop(SNvxCameraData *pNvxCamera, int nPortId);
static NvError s_GetCrop(SNvxCameraData *pNvxCamera, int nPortId);
static NvError s_SetStylizeMode(SNvxCameraData *pNvxCamera, OMX_IMAGEFILTERTYPE eImageFilter);
static NvError s_SetWhiteBalance(SNvxCameraData *pNvxCamera, OMX_WHITEBALCONTROLTYPE eWhiteBalControl);
static NvError s_GetWhiteBalance(SNvxCameraData *pNvxCamera, OMX_WHITEBALCONTROLTYPE *eWhiteBalControl);
static OMX_ERRORTYPE NvxCameraGetTransformData(SNvxCameraData *pNvxCamera, NvxComponent *pComponent);
NvError CameraNegotiateStreamBuffers(
    SNvxNvMMTransformData *oBase,
    NvBool dz,
    NvU32 const * StreamIndexes,
    NvU32 numStreams);
static void NvxCameraInputSurfaceHook(OMX_BUFFERHEADERTYPE *pSrcSurf, NvMMSurfaceDescriptor *pDestSurf);
static OMX_ERRORTYPE
NvxCameraSetDzTransform(
    SNvxCameraData *pNvxCamera,
    CameraDZOutputs Output,
    OMX_S32 *pNewRotation,
    OMX_MIRRORTYPE *pNewMirror);
static void
ConvertTransform2RotationMirror(
    NvDdk2dTransform Transform,
    OMX_MIRRORTYPE *pMirror,
    OMX_S32 *pRotation);
static OMX_ERRORTYPE
NvxCameraInitializeCameraSettings(
    SNvxCameraData *pNvxCamera);

static OMX_ERRORTYPE
NvxCameraCopySurfaceForUser(
    NvRmSurface *Surfaces,
    OMX_PTR userBuffer,
    OMX_U32 *size,
    NVX_VIDEOFRAME_FORMAT format);

static OMX_U32
NvxCameraGetDzOutputIndex(
    OMX_S32 nPortIndex);

static NVX_IMAGE_COLOR_FORMATTYPE
NvxCameraConvertNvColorToNvxImageColor(
    NvMMDigitalZoomFrameCopyColorFormat *pFormat);

static OMX_ERRORTYPE
NvxCameraConvertNvxImageColorToNvColor(
    NVX_IMAGE_COLOR_FORMATTYPE NvxImageColorFormat,
    NvMMDigitalZoomFrameCopyColorFormat *pNvColorFormat);

static void
NvxCameraMappingPreviewOutputRegionToInputRegion(
    NvRectF32 *pCameraRegions,
    NvMMDigitalZoomOperationInformation *pDZInfo,
    NVX_RectF32 *pPreviewOutputRegions,
    NvU32 numOfRegions);


static void NvxParseCameraConfig(NvOsFileHandle fileHandle, SNvxCameraData *pNvxCamera);

typedef struct nvxCameraPortSetRec
{
    SNvxNvMMPort *ports[TF_NUM_STREAMS * 2];
    NvU32 numPorts;
} nvxCameraPortSet;

static void nvxCameraPortSetInit(nvxCameraPortSet *set)
{
    set->numPorts = 0;
}

static void nvxCameraPortSetAdd(nvxCameraPortSet *set, SNvxNvMMPort *port)
{
    if (set->numPorts < (TF_NUM_STREAMS * 2) && port != NULL)
    {
        set->ports[set->numPorts++] = port;
    }
}

static OMX_ERRORTYPE nvxCameraWaitBufferNegotiationComplete(
    SNvxCameraData *pNvxCamera,
    nvxCameraPortSet *portSet,
    NvU32 timeOut)
{
    NvS32 timeLeft = (NvS32)timeOut;
    NvU32 prevTime, currTime;
    NvError err = NvSuccess;
    OMX_BOOL allDone = OMX_FALSE;
    NvU32 i = 0;

    if (portSet == NULL || portSet->numPorts == 0)
        return OMX_ErrorNone;

    if (pNvxCamera == NULL || pNvxCamera->oStreamBufNegotiationSem == NULL)
        return OMX_ErrorBadParameter;

    prevTime = NvOsGetTimeMS();
    do {
        allDone = OMX_TRUE;
        for (i = 0; i < portSet->numPorts; i++)
        {
            allDone = allDone && portSet->ports[i]->bBufferNegotiationDone;
        }
        if (allDone)
        {
            return OMX_ErrorNone;
        }
        if (timeLeft < 0)
        {
            return OMX_ErrorTimeout;
        }

        err = NvOsSemaphoreWaitTimeout(
                    pNvxCamera->oStreamBufNegotiationSem,
                    timeLeft);
        if (err != NvSuccess)
        {
            return NvxTranslateErrorCode(err);
        }
        currTime = NvOsGetTimeMS();
        timeLeft -= (NvS32)OMX_TIMEDIFF(prevTime,currTime);
        prevTime = currTime;
    } while (!allDone);
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxCameraDeInit(NvxComponent *pNvComp)
{
    SNvxCameraData *pNvxCamera;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxCameraDeInit\n"));

    pNvxCamera = (SNvxCameraData *)pNvComp->pComponentData;

    if (pNvxCamera->bTransformDataInitialized)
    {
        eError = NvxNvMMTransformClose(&pNvxCamera->oDigitalZoom);
        if (eError != OMX_ErrorNone)
        {
            return eError;
        }

        eError = NvxNvMMTransformClose(&pNvxCamera->oSrcCamera);
        if (eError != OMX_ErrorNone)
        {
            return eError;
        }

        // can we do anything better than a spinning while loop here?
        while (!pNvxCamera->oDigitalZoom.blockCloseDone);
        while (!pNvxCamera->oSrcCamera.blockCloseDone);

        pNvxCamera->bTransformDataInitialized = OMX_FALSE;
    }

    NvOsFree(pNvxCamera->pConfigFilePath);
    pNvxCamera->pConfigFilePath = NULL;
    NvOsSemaphoreDestroy(pNvxCamera->oStreamBufNegotiationSem);
    NvOsFree(pNvComp->pComponentData);
    pNvComp->pComponentData = 0;
    return eError;
}

static OMX_ERRORTYPE NvxCameraGetFlashMode(SNvxCameraData *pNvxCamera,
                              OMX_IMAGE_PARAM_FLASHCONTROLTYPE *pFlashCtrl)
{
    NvU32 FlashMode = 0;
    NvError Err = NvSuccess;

    Err = pNvxCamera->oSrcCamera.hBlock->GetAttribute(
        pNvxCamera->oSrcCamera.hBlock,
        NvMMCameraAttribute_FlashMode,
        sizeof(NvU32),
        &FlashMode);

    if (Err != NvSuccess)
    {
        return OMX_ErrorUndefined;
    }

    switch (FlashMode)
    {
        case NVMMCAMERA_FLASHMODE_STILL_MANUAL:
            pFlashCtrl->eFlashControl = OMX_IMAGE_FlashControlOn;
            break;
        case NVMMCAMERA_FLASHMODE_STILL_AUTO:
            pFlashCtrl->eFlashControl = OMX_IMAGE_FlashControlAuto;
            break;
        case NVMMCAMERA_FLASHMODE_OFF:
            pFlashCtrl->eFlashControl = OMX_IMAGE_FlashControlOff;
            break;
        case NVMMCAMERA_FLASHMODE_TORCH:
            pFlashCtrl->eFlashControl = OMX_IMAGE_FlashControlTorch;
            break;
        case NVMMCAMERA_FLASHMODE_REDEYE_REDUCTION:
            pFlashCtrl->eFlashControl = OMX_IMAGE_FlashControlRedEyeReduction;
            break;
        default:
            pFlashCtrl->eFlashControl = OMX_IMAGE_FlashControlOff;
            Err = OMX_ErrorNotImplemented;
            break;
    }
    return Err;
}

static OMX_ERRORTYPE NvxCameraSetFlashMode(SNvxCameraData *pNvxCamera,
                           const OMX_IMAGE_PARAM_FLASHCONTROLTYPE *pFlashCtrl)
{
    NvU32 FlashMode = 0;
    NvError Err = NvSuccess;
    OMX_IMAGE_FLASHCONTROLTYPE omxFlash;

    omxFlash = pFlashCtrl->eFlashControl;
    switch (omxFlash)
    {
        case OMX_IMAGE_FlashControlAuto:
            FlashMode = NVMMCAMERA_FLASHMODE_STILL_AUTO;
            break;
        case OMX_IMAGE_FlashControlOn:
            FlashMode = NVMMCAMERA_FLASHMODE_STILL_MANUAL;
            break;
        case OMX_IMAGE_FlashControlOff:
            FlashMode = NVMMCAMERA_FLASHMODE_OFF;
            break;
        case OMX_IMAGE_FlashControlTorch:
            FlashMode = NVMMCAMERA_FLASHMODE_TORCH;
            break;
        case OMX_IMAGE_FlashControlRedEyeReduction:
            FlashMode = NVMMCAMERA_FLASHMODE_REDEYE_REDUCTION;
            break;
        default:
            FlashMode = NVMMCAMERA_FLASHMODE_OFF;
            return OMX_ErrorNotImplemented;
    }
    Err = pNvxCamera->oSrcCamera.hBlock->SetAttribute(
        pNvxCamera->oSrcCamera.hBlock,
        NvMMCameraAttribute_FlashMode,
        0,
        sizeof(NvU32),
        &FlashMode);
    if (Err != NvSuccess) {
        return OMX_ErrorInvalidState;
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxCameraGetSensorModes(SNvxCameraData *pNvxCamera,
                                    NVX_CONFIG_SENSORMODELIST *pList)
{
    NvMMCameraSensorMode SensorModeList[MAX_NUM_SENSOR_MODES];
    NvError Err = NvSuccess;
    int id;

    NvOsMemset(SensorModeList, 0, sizeof(SensorModeList));
    Err = pNvxCamera->oSrcCamera.hBlock->GetAttribute(
        pNvxCamera->oSrcCamera.hBlock,
        NvMMCameraAttribute_SensorModeList,
        sizeof(SensorModeList),
        &SensorModeList[0]);
    if (Err != NvSuccess)
    {
        return OMX_ErrorUndefined;
    }

    for (id = 0; id < MAX_NUM_SENSOR_MODES; id++)
    {
        if (SensorModeList[id].Resolution.width == 0 || SensorModeList[id].Resolution.height == 0)
            break;
        pList->SensorModeList[id].width     = SensorModeList[id].Resolution.width;
        pList->SensorModeList[id].height    = SensorModeList[id].Resolution.height;
        pList->SensorModeList[id].FrameRate = SensorModeList[id].FrameRate;
    }
    pList->nSensorModes = id;
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxCameraGetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    SNvxCameraData *pNvxCamera;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvBool bNeedOpenBlock = NV_FALSE;

    NV_ASSERT(pComponent && pComponentParameterStructure);
    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxCameraGetParameter\n"));

    pNvxCamera = (SNvxCameraData *)pComponent->pComponentData;
    NV_ASSERT(pNvxCamera);

    switch (nParamIndex)
    {
    case OMX_IndexParamCommonSensorMode:
        NvOsMemcpy(pComponentParameterStructure, &(pNvxCamera->SensorModeParams),
                   sizeof(OMX_PARAM_SENSORMODETYPE));
        break;

    case OMX_IndexParamPortDefinition:
        {
            OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
            OMX_ERRORTYPE eError = NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
            if((NvU32)s_nvx_port_still_capture == pPortDef->nPortIndex)
            {
                if (pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_still].bHasSize)
                {
                    pPortDef->format.video.nFrameWidth = pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_still].nWidth;
                    pPortDef->format.video.nFrameHeight = pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_still].nHeight;
                    pPortDef->format.video.nStride = pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_still].nWidth;
                }
            }
            else if ((NvU32)s_nvx_port_video_capture == pPortDef->nPortIndex)
            {
                if (pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_video].bHasSize)
                {
                    pPortDef->format.video.nFrameWidth = pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_video].nWidth;
                    pPortDef->format.video.nFrameHeight = pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_video].nHeight;
                    pPortDef->format.video.nStride = pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_video].nWidth;
                }

            }
            else if((NvU32)s_nvx_port_preview == pPortDef->nPortIndex)
            {
                if (pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_preview].bHasSize)
                {
                    pPortDef->format.video.nFrameWidth = pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_preview].nWidth;
                    pPortDef->format.video.nFrameHeight = pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_preview].nHeight;
                    pPortDef->format.video.nStride = pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_preview].nWidth;
                }
            }
            else if((NvU32)s_nvx_port_raw_input == pPortDef->nPortIndex)
            {
                if (pNvxCamera->oSrcCamera.oPorts[pNvxCamera->s_nvx_port_sc_input].bHasSize)
                {
                    pPortDef->format.video.nFrameWidth = pNvxCamera->oSrcCamera.oPorts[pNvxCamera->s_nvx_port_sc_input].nWidth;
                    pPortDef->format.video.nFrameHeight = pNvxCamera->oSrcCamera.oPorts[pNvxCamera->s_nvx_port_sc_input].nHeight;
                    pPortDef->format.video.nStride = pNvxCamera->oSrcCamera.oPorts[pNvxCamera->s_nvx_port_sc_input].nWidth;
                }
            }
            else
            {
                return eError;
            }
            break;
        }

    case OMX_IndexParamNumAvailableStreams:
        {
            OMX_PARAM_U32TYPE *nas = (OMX_PARAM_U32TYPE*)pComponentParameterStructure;
            nas->nU32 = pNvxCamera->nCaptureCount;
        }
        break;

    case NVX_IndexParamAvailableSensors:
        {
            OMX_PARAM_U32TYPE *availSensors = (OMX_PARAM_U32TYPE*)pComponentParameterStructure;

            // Call Enumerate function
            NvOdmPeripheralSearch FindImagerAttr =
                NvOdmPeripheralSearch_PeripheralClass;
            NvU32 FindImagerVal = (NvU32)NvOdmPeripheralClass_Imager;
            availSensors->nU32 = NvOdmPeripheralEnumerate(&FindImagerAttr,
                &FindImagerVal, 1, NULL, 0);

        }
        break;

    case NVX_IndexParamSensorName:  // pComponentParameterStructure should have enough length
        {
            NVX_PARAM_FILENAME *sn = (NVX_PARAM_FILENAME*)pComponentParameterStructure;
            NvOsMemcpy(sn->pFilename, pNvxCamera->SensorName, sizeof(pNvxCamera->SensorName));
        }
        break;

    case OMX_IndexParamFlashControl:
    case NVX_IndexConfigSensorModeList:
    case NVX_IndexParamStereoCameraMode:
    case NVX_IndexParamStereoCaptureInfo:
    case NVX_IndexParamRawOutput:
    case NVX_IndexParamPreviewMode:
        bNeedOpenBlock = NV_TRUE;
        break;

    default:
        return NvxComponentBaseGetParameter(pComponent, nParamIndex, pComponentParameterStructure);
    }

    if (bNeedOpenBlock == NV_FALSE)
    {
        return eError;
    }

    eError = NvxCameraGetTransformData(pNvxCamera, pComponent);
    if (eError != OMX_ErrorNone)
    {
        return eError;
    }

    switch (nParamIndex)
    {
    case OMX_IndexParamFlashControl:
        {
            OMX_ERRORTYPE omxErr = OMX_ErrorNone;
            OMX_IMAGE_PARAM_FLASHCONTROLTYPE *pfc = (OMX_IMAGE_PARAM_FLASHCONTROLTYPE*)pComponentParameterStructure;

            omxErr = NvxCameraGetFlashMode(pNvxCamera,pfc);
            if (omxErr != OMX_ErrorNone)
            {
                return omxErr;
            }
        }
        break;

    case NVX_IndexConfigSensorModeList:
        {
            OMX_ERRORTYPE omxErr = OMX_ErrorNone;
            omxErr = NvxCameraGetSensorModes(pNvxCamera,
                         (NVX_CONFIG_SENSORMODELIST*)pComponentParameterStructure);
            if (omxErr != OMX_ErrorNone)
            {
                return omxErr;
            }
        }
        break;

    case NVX_IndexParamStereoCameraMode:
        {
            NvError Err = NvSuccess;
            Err = pNvxCamera->oSrcCamera.hBlock->GetAttribute(
                pNvxCamera->oSrcCamera.hBlock,
                NvMMCameraAttribute_StereoCameraMode,
                sizeof(ENvxCameraStereoCameraMode),
                &((NVX_PARAM_STEREOCAMERAMODE*)pComponentParameterStructure)->StereoCameraMode);
            if (Err != NvSuccess)
            {
                return OMX_ErrorUndefined;
            }
        }
        break;

    case NVX_IndexParamStereoCaptureInfo:
        {
            NvError Err = NvSuccess;
            NVX_PARAM_STEREOCAPTUREINFO *StCapInfo =
                    (NVX_PARAM_STEREOCAPTUREINFO*)pComponentParameterStructure;
            NvMMCameraStereoCaptureInfo NvMMCamStCapInfo = {0};
            Err = pNvxCamera->oSrcCamera.hBlock->GetAttribute(
                pNvxCamera->oSrcCamera.hBlock,
                NvMMCameraAttribute_StereoCaptureInfo,
                sizeof(NvMMCameraStereoCaptureInfo),
                &NvMMCamStCapInfo);
            if (Err != NvSuccess)
            {
                return OMX_ErrorUndefined;
            }

            switch (NvMMCamStCapInfo.StCapType)
            {
                case NvMMCameraCaptureType_VideoSquashed:
                    StCapInfo->StCaptureInfo.CameraCaptureType =
                        NvxCameraCaptureType_VideoSquashed;
                    break;
                case NvMMCameraCaptureType_Video:
                    StCapInfo->StCaptureInfo.CameraCaptureType =
                        NvxCameraCaptureType_Video;
                    break;
                case NvMMCameraCaptureType_Still:
                    StCapInfo->StCaptureInfo.CameraCaptureType =
                        NvxCameraCaptureType_Still;
                    break;
                case NvMMCameraCaptureType_StillBurst:
                    StCapInfo->StCaptureInfo.CameraCaptureType =
                        NvxCameraCaptureType_StillBurst;
                    break;
                case NvMMCameraCaptureType_None:
                default:
                    StCapInfo->StCaptureInfo.CameraCaptureType =
                        NvxCameraCaptureType_None;
                    break;
            }

            switch (NvMMCamStCapInfo.StCapBufferType)
            {
                case NvMMBufferFlag_Stereo_LeftRight:
                    StCapInfo->StCaptureInfo.CameraStereoType =
                        Nvx_BufferFlag_Stereo_LeftRight;
                    break;
                case NvMMBufferFlag_Stereo_RightLeft:
                    StCapInfo->StCaptureInfo.CameraStereoType =
                        Nvx_BufferFlag_Stereo_RightLeft;
                    break;
                case NvMMBufferFlag_Stereo_TopBottom:
                    StCapInfo->StCaptureInfo.CameraStereoType =
                        Nvx_BufferFlag_Stereo_TopBottom;
                    break;
                case NvMMBufferFlag_Stereo_BottomTop:
                    StCapInfo->StCaptureInfo.CameraStereoType =
                        Nvx_BufferFlag_Stereo_BottomTop;
                    break;
                case NvMMBufferFlag_Stereo_SeparateLR:
                    StCapInfo->StCaptureInfo.CameraStereoType =
                        Nvx_BufferFlag_Stereo_SeparateLR;
                    break;
                case NvMMBufferFlag_Stereo_SeparateRL:
                    StCapInfo->StCaptureInfo.CameraStereoType =
                        Nvx_BufferFlag_Stereo_SeparateRL;
                    break;
                case NvMMBufferFlag_Stereo_None:
                default:
                    StCapInfo->StCaptureInfo.CameraStereoType =
                        Nvx_BufferFlag_Stereo_None;
                    break;
            }
        }
        break;
#if USE_ANDROID_NATIVE_WINDOW
    case NVX_IndexParamGetAndroidNativeBufferUsage:
        return HandleGetANBUsage(pComponent, pComponentParameterStructure, 0);
#endif
    case NVX_IndexParamPreviewMode:
        {
            NvError Err = NvSuccess;
            NVX_PARAM_PREVIEWMODE *pPreviewMode =
                (NVX_PARAM_PREVIEWMODE*)pComponentParameterStructure;

            NvMMCameraSensorMode Mode;
            NvOsMemset(&Mode, 0, sizeof(Mode));
            Err = pNvxCamera->oSrcCamera.hBlock->GetAttribute(
                 pNvxCamera->oSrcCamera.hBlock,
                 NvMMCameraAttribute_PreviewSensorMode,
                 sizeof(NvMMCameraSensorMode),
                 &Mode);
            if (Err != NvSuccess)
            {
                return OMX_ErrorBadParameter;
            }
            else
            {
                pPreviewMode->mMode.width = Mode.Resolution.width;
                pPreviewMode->mMode.height = Mode.Resolution.height;
                pPreviewMode->mMode.FrameRate = Mode.FrameRate;
            }
            break;
        }

    case NVX_IndexParamRawOutput:
    {
        NvError Err = NvSuccess;
        NvBool Enabled;
        OMX_CONFIG_BOOLEANTYPE *pRawOutput = (OMX_CONFIG_BOOLEANTYPE*)pComponentParameterStructure;

       Err = pNvxCamera->oSrcCamera.hBlock->GetAttribute(pNvxCamera->oSrcCamera.hBlock,
                NvMMCameraAttribute_SensorNormalization, sizeof(NvBool), &Enabled);

        if (Err != NvSuccess)
        {
            return OMX_ErrorUndefined;
        }

        // RAW output enabled when sensor normalization is disabled.
        pRawOutput->bEnabled = (Enabled == NV_TRUE ? OMX_FALSE : OMX_TRUE);
        break;
    }

    default:
        break;
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxCameraSetParameter(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentParameterStructure)
{
    SNvxCameraData *pNvxCamera;
    OMX_ERRORTYPE eError, tError;
    NvMMCameraPin EnabledPin = 0;
    NvMMDigitalZoomPin EnabledDZPin = 0;
    NvMMBlockHandle hNvMMDzBlock = NULL;
    NvMMBlockHandle hNvMMCameraBlock = NULL;

    NV_ASSERT(pComponent && pComponentParameterStructure);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxCameraSetParameter\n"));

    pNvxCamera = (SNvxCameraData *)pComponent->pComponentData;
    NV_ASSERT(pNvxCamera);

    switch(nIndex)
    {
    case OMX_IndexParamCommonSensorMode:
        NvOsMemcpy(&(pNvxCamera->SensorModeParams), pComponentParameterStructure,
                   sizeof(OMX_PARAM_SENSORMODETYPE));

        // this param was largely deprecated by the new architecture of dedicated
        // still/video output ports, so we don't do a whole lot here anymore.

        // throw error if the type of request doesn't line up with the port it was
        // requested on.
        if (!pNvxCamera->SensorModeParams.bOneShot &&
            (pNvxCamera->SensorModeParams.nPortIndex == (OMX_U32)s_nvx_port_still_capture))
        {
            return OMX_ErrorBadPortIndex;
        }
        else if (pNvxCamera->SensorModeParams.bOneShot &&
            (pNvxCamera->SensorModeParams.nPortIndex == (OMX_U32)s_nvx_port_video_capture))
        {
            return OMX_ErrorBadPortIndex;
        }

        break;

    case OMX_IndexParamPortDefinition:
        {
            OMX_PARAM_PORTDEFINITIONTYPE *pPortDef =
               (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;

            if (pPortDef->nPortIndex == (OMX_U32)s_nvx_port_still_capture)
            {
                pNvxCamera->nSurfWidthStillCapture  = pPortDef->format.video.nFrameWidth;
                pNvxCamera->nSurfHeightStillCapture = pPortDef->format.video.nFrameHeight;
                pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_still].bHasSize = OMX_TRUE;
                pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_still].nWidth = pNvxCamera->nSurfWidthStillCapture;
                pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_still].nHeight = pNvxCamera->nSurfHeightStillCapture;
            }
            else if (pPortDef->nPortIndex == (OMX_U32)s_nvx_port_video_capture)
            {
                pNvxCamera->nSurfWidthVideoCapture  = pPortDef->format.video.nFrameWidth;
                pNvxCamera->nSurfHeightVideoCapture = pPortDef->format.video.nFrameHeight;
                pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_video].bHasSize = OMX_TRUE;
                pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_video].nWidth = pNvxCamera->nSurfWidthVideoCapture;
                pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_video].nHeight = pNvxCamera->nSurfHeightVideoCapture;
            }
            else if (pPortDef->nPortIndex == (OMX_U32)s_nvx_port_preview)
            {
                // make sure that camera and DZ block is open
                eError = NvxCameraGetTransformData(pNvxCamera, pComponent);
                if (eError != OMX_ErrorNone)
                {
                    return eError;
                }

                if (((pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_preview].nWidth != pPortDef->format.video.nFrameWidth) ||
                     (pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_preview].nHeight != pPortDef->format.video.nFrameHeight)) &&
                    (!pNvxCamera->PreviewEnabled))
                {
                    // new preview size when preview isn't active
                    // need to release current negotiated buffers and renegotiate for new size

                    // Clear preview pin so that if capture size changes immidiately after this
                    // block-camera should not get false indication that preview buffer
                    // configuration is received or about to receive.

                    hNvMMDzBlock = pNvxCamera->oDigitalZoom.hBlock;
                    hNvMMCameraBlock = pNvxCamera->oSrcCamera.hBlock;

                    if (!hNvMMDzBlock || !hNvMMCameraBlock)
                        return OMX_ErrorBadParameter;

                    hNvMMCameraBlock->GetAttribute(
                        hNvMMCameraBlock,
                        NvMMCameraAttribute_PinEnable,
                        sizeof(NvMMCameraPin),
                        &EnabledPin);

                    hNvMMDzBlock->GetAttribute(
                        hNvMMDzBlock,
                        NvMMDigitalZoomAttributeType_PinEnable,
                        sizeof(EnabledDZPin),
                        &EnabledDZPin);

                    EnabledPin &= ~NvMMCameraPin_Preview;
                    hNvMMCameraBlock->SetAttribute(
                        hNvMMCameraBlock,
                        NvMMCameraAttribute_PinEnable,
                        0,
                        sizeof(NvMMCameraPin),
                        &EnabledPin);

                    EnabledDZPin &= ~NvMMDigitalZoomPin_Preview;
                    hNvMMDzBlock->SetAttribute(
                        hNvMMDzBlock,
                        NvMMDigitalZoomAttributeType_PinEnable,
                        0,
                        sizeof(EnabledDZPin),
                        &EnabledDZPin);

                    // return buffers from dz to camera
                    tError = eError = NvxReturnBuffersToCamera(
                                     pNvxCamera,
                                     s_nvx_port_dz_input_preview);

                    // reset DZ's preview output
                    eError = NvxResetDZStream(pNvxCamera,s_nvx_port_dz_output_preview);
                    if ((eError != OMX_ErrorNone)&&(tError == OMX_ErrorNone))
                    {
                        tError = eError;
                    }

                    // reset DZ's preview input
                    eError = NvxResetDZStream(pNvxCamera,s_nvx_port_dz_input_preview);
                    if ((eError != OMX_ErrorNone)&&(tError == OMX_ErrorNone))
                    {
                        tError = eError;
                    }

                    // reset Camera's preview output
                    eError = NvxResetCameraStream(pNvxCamera,pNvxCamera->s_nvx_port_sc_output_preview);
                    if ((eError != OMX_ErrorNone)&&(tError == OMX_ErrorNone))
                    {
                        tError = eError;
                    }

                    // return the first error that occurred in reset sequence
                    if (tError != OMX_ErrorNone)
                        return tError;
                }
                pNvxCamera->nSurfWidthPreview  = pPortDef->format.video.nFrameWidth;
                pNvxCamera->nSurfHeightPreview = pPortDef->format.video.nFrameHeight;
                pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_preview].bHasSize = OMX_TRUE;
                pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_preview].nWidth = pNvxCamera->nSurfWidthPreview;
                pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_preview].nHeight = pNvxCamera->nSurfHeightPreview;
            }
            else if (pPortDef->nPortIndex == (OMX_U32)s_nvx_port_thumbnail)
            {
                pNvxCamera->nSurfWidthThumbnail  = pPortDef->format.video.nFrameWidth;
                pNvxCamera->nSurfHeightThumbnail = pPortDef->format.video.nFrameHeight;
                pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_thumbnail].bHasSize = OMX_TRUE;
                pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_thumbnail].nWidth = pNvxCamera->nSurfWidthThumbnail;
                pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_thumbnail].nHeight = pNvxCamera->nSurfHeightThumbnail;
                pNvxCamera->ThumbnailEnabled = NV_TRUE;
            }
            else if (pPortDef->nPortIndex == (OMX_U32)s_nvx_port_raw_input)
            {
                pNvxCamera->nSurfWidthInput  = pPortDef->format.video.nFrameWidth;
                pNvxCamera->nSurfHeightInput = pPortDef->format.video.nFrameHeight;
                pNvxCamera->oSrcCamera.oPorts[pNvxCamera->s_nvx_port_sc_input].bHasSize = OMX_TRUE;
                pNvxCamera->oSrcCamera.oPorts[pNvxCamera->s_nvx_port_sc_input].nWidth = pNvxCamera->nSurfWidthInput;
                pNvxCamera->oSrcCamera.oPorts[pNvxCamera->s_nvx_port_sc_input].nHeight = pNvxCamera->nSurfHeightInput;
            }
            return NvxComponentBaseSetParameter(pComponent, nIndex, pComponentParameterStructure);
        }

    case OMX_IndexParamNumAvailableStreams:
        {
            OMX_PARAM_U32TYPE *nas = (OMX_PARAM_U32TYPE*)pComponentParameterStructure;
            pNvxCamera->nCaptureCount = nas->nU32;
        }
        break;

    case NVX_IndexParamSensorName:
        { // borrow NVX_PARAM_FILENAME. This index should be gone finally.
            NVX_PARAM_FILENAME *sn = (NVX_PARAM_FILENAME*)pComponentParameterStructure;
            NvOsMemcpy(pNvxCamera->SensorName, sn->pFilename, sizeof(pNvxCamera->SensorName));
        }
        break;

    case OMX_IndexParamFlashControl:
        break;

    case NVX_IndexParamSensorId:
        {
            OMX_PARAM_U32TYPE *psi = (OMX_PARAM_U32TYPE *)pComponentParameterStructure;
            NvU32 numOfCamera = USB_CAMERA_SENSOR_ID_START;
            NvOsFileHandle fileHandle = NULL;
            pNvxCamera->oSrcCamera.BlockSpecific = (OMX_U64)(psi->nU32);
            //USB camera is disabled default
            pNvxCamera->bUseUSBCamera = OMX_FALSE;
            pNvxCamera->s_nvx_port_sc_input = NvMMCameraStreamIndex_Input;

            if (pNvxCamera->pConfigFilePath)
            {
                if (NvOsFopen((const char *)pNvxCamera->pConfigFilePath,
                        NVOS_OPEN_READ, &fileHandle) == NvSuccess)
                {
                    NvxParseCameraConfig(fileHandle, pNvxCamera);
                    NvOsFclose(fileHandle);
                }
                else
                {
                    NvOsDebugPrintf("%s: failed to open %s\n", __func__,
                                    pNvxCamera->pConfigFilePath);
                }
            }

            if (pNvxCamera->nCameraSource)
                numOfCamera = pNvxCamera->nCameraSource;

            if (psi->nU32 == numOfCamera)
            {
                pNvxCamera->bUseUSBCamera = OMX_TRUE;
                NV_DEBUG_PRINTF(("USB camera detected .......................\n"));
                pNvxCamera->s_nvx_port_sc_output_preview = NvMMUSBCameraStreamIndex_OutputPreview;
                pNvxCamera->s_nvx_port_sc_output_capture = NvMMUSBCameraStreamIndex_Output;
            }
            else {
                NV_DEBUG_PRINTF(("CSI camera detected .......................\n"));
                pNvxCamera->s_nvx_port_sc_output_preview  = NvMMCameraStreamIndex_OutputPreview;
                pNvxCamera->s_nvx_port_sc_output_capture  = NvMMCameraStreamIndex_Output;
            }
        }
        break;
    case NVX_IndexParamSensorGuid:
        {
            NVX_PARAM_SENSOR_GUID *psi = (NVX_PARAM_SENSOR_GUID *)pComponentParameterStructure;
            pNvxCamera->oSrcCamera.BlockSpecific = psi->imagerGuid;
        }
        break;
    case NVX_IndexConfigUseNvBuffer:
        {
            int dzPort;

            // used by camera DZ, in order embed nvmmbuffers in OMX buffer.
            NVX_PARAM_USENVBUFFER *pParam = (NVX_PARAM_USENVBUFFER *)pComponentParameterStructure;

            if ((int)pParam->nPortIndex == s_nvx_port_preview)
            {
                dzPort = s_nvx_port_dz_output_preview;
            }
            else if ((int)pParam->nPortIndex == s_nvx_port_still_capture)
            {
                dzPort = s_nvx_port_dz_output_still;
            }
            else if ((int)pParam->nPortIndex == s_nvx_port_video_capture)
            {
                dzPort = s_nvx_port_dz_output_video;
            }
            else
            {
                return OMX_ErrorUndefined;
            }

            pNvxCamera->oDigitalZoom.oPorts[dzPort].bEmbedNvMMBuffer = pParam->bUseNvBuffer;
        }
        break;
    case NVX_IndexParamStereoCameraMode:
        {
            ENvxCameraStereoCameraMode StCamMode =
                ((NVX_PARAM_STEREOCAMERAMODE*)pComponentParameterStructure)->StereoCameraMode;
            NvError Err = NvSuccess;

            Err = pNvxCamera->oSrcCamera.hBlock->SetAttribute(
                      pNvxCamera->oSrcCamera.hBlock,
                      NvMMCameraAttribute_StereoCameraMode, 0,
                      sizeof(StCamMode), &StCamMode);
            if (Err != NvSuccess)
            {
                return OMX_ErrorUndefined;
            }
        }
        break;

    case NVX_IndexParamStereoCaptureInfo:
        {
            NVX_STEREOCAPTUREINFO StCaptureInfo =
                ((NVX_PARAM_STEREOCAPTUREINFO*)pComponentParameterStructure)->StCaptureInfo;
            NvError Err = NvSuccess;
            NvMMCameraStereoCaptureInfo NvMMCamStCapInfo;

            switch (StCaptureInfo.CameraCaptureType)
            {
                case NvxCameraCaptureType_VideoSquashed:
                    NvMMCamStCapInfo.StCapType = NvMMCameraCaptureType_VideoSquashed;
                    break;
                case NvxCameraCaptureType_Video:
                    NvMMCamStCapInfo.StCapType = NvMMCameraCaptureType_Video;
                    break;
                case NvxCameraCaptureType_Still:
                    NvMMCamStCapInfo.StCapType = NvMMCameraCaptureType_Still;
                    break;
                case NvxCameraCaptureType_StillBurst:
                    NvMMCamStCapInfo.StCapType = NvMMCameraCaptureType_StillBurst;
                    break;
                case NvxCameraCaptureType_None:
                default:
                    NvMMCamStCapInfo.StCapType = NvMMCameraCaptureType_None;
                    break;
            }

            switch (StCaptureInfo.CameraStereoType)
            {
                case Nvx_BufferFlag_Stereo_LeftRight:
                    NvMMCamStCapInfo.StCapBufferType = NvMMBufferFlag_Stereo_LeftRight;
                    break;
                case Nvx_BufferFlag_Stereo_RightLeft:
                    NvMMCamStCapInfo.StCapBufferType = NvMMBufferFlag_Stereo_RightLeft;
                    break;
                case Nvx_BufferFlag_Stereo_TopBottom:
                    NvMMCamStCapInfo.StCapBufferType = NvMMBufferFlag_Stereo_TopBottom;
                    break;
                case Nvx_BufferFlag_Stereo_BottomTop:
                    NvMMCamStCapInfo.StCapBufferType = NvMMBufferFlag_Stereo_BottomTop;
                    break;
                case Nvx_BufferFlag_Stereo_SeparateLR:
                    NvMMCamStCapInfo.StCapBufferType = NvMMBufferFlag_Stereo_SeparateLR;
                    break;
                case Nvx_BufferFlag_Stereo_SeparateRL:
                    NvMMCamStCapInfo.StCapBufferType = NvMMBufferFlag_Stereo_SeparateRL;
                    break;
                case Nvx_BufferFlag_Stereo_None:
                default:
                    NvMMCamStCapInfo.StCapBufferType = NvMMBufferFlag_Stereo_None;
                    break;
            }

            /*
             * It is required for client to pass StereoCaptureInfo to
             * NvMMCamera and NvMMDigitalZoom blocks so that the correct
             * buffers are allocated. Since the blocks operate asynchronously,
             * the stereo information to both DZ and CAM should be passed,
             * if not at the same time, before the buffer allocation.
             */
            Err = pNvxCamera->oSrcCamera.hBlock->SetAttribute(
                      pNvxCamera->oSrcCamera.hBlock,
                      NvMMCameraAttribute_StereoCaptureInfo, 0,
                      sizeof(NvMMCameraStereoCaptureInfo), &NvMMCamStCapInfo);
            if (Err != NvSuccess)
            {
                return OMX_ErrorBadParameter;
            }

            Err = pNvxCamera->oDigitalZoom.hBlock->SetAttribute(
                      pNvxCamera->oDigitalZoom.hBlock,
                      NvMMDigitalZoomAttribute_StereoCaptureInfo, 0,
                      sizeof(NvMMCameraStereoCaptureInfo), &NvMMCamStCapInfo);
            if (Err != NvSuccess)
            {
                return OMX_ErrorBadParameter;
            }
        }
        break;

#if USE_ANDROID_NATIVE_WINDOW
    case NVX_IndexParamEnableAndroidBuffers:
        {
            //Hard coded to Preview port for now. If other port needs to be enabled,
            // then HandleEnableANB needs to be modified
            OMX_ERRORTYPE err = HandleEnableANB(
                pComponent,
                s_nvx_port_preview,
                pComponentParameterStructure);

            pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_preview].bUsesANBs =
                (pComponent->pPorts[s_nvx_port_preview].oPortDef.format.video.eColorFormat != OMX_COLOR_FormatYUV420Planar);

            return err;
        }
    case NVX_IndexParamUseAndroidNativeBuffer:
        return HandleUseANB(pComponent, pComponentParameterStructure);

    case NVX_IndexParamUseNativeBufferHandle:
        {
            pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_preview].bUsesNBHs =
            pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_preview].bUsesANBs;
            return HandleUseNBH(pComponent, pComponentParameterStructure);
        }
#endif

    case NVX_IndexParamSensorMode:
        {
            NVX_SENSORMODE Mode =
                ((NVX_PARAM_SENSORMODE*)pComponentParameterStructure)->SensorMode;
            NvMMCameraSensorMode CamMode;
            NvError Err = NvSuccess;
            OMX_ERRORTYPE eError = OMX_ErrorNone;

            // make sure that camera block is open
            eError = NvxCameraGetTransformData(pNvxCamera, pComponent);
            if (eError != OMX_ErrorNone)
            {
                 return eError;
            }

            NvOsMemset(&CamMode, 0, sizeof(NvMMCameraSensorMode));
            CamMode.Resolution.width  = (NvS32)Mode.width;
            CamMode.Resolution.height = (NvS32)Mode.height;
            CamMode.FrameRate         = (NvF32)Mode.FrameRate;

            Err = pNvxCamera->oSrcCamera.hBlock->SetAttribute(
                pNvxCamera->oSrcCamera.hBlock,
                NvMMCameraAttribute_SensorMode,
                0,
                sizeof(NvMMCameraSensorMode), &CamMode);
            if (Err != NvSuccess)
            {
                return OMX_ErrorUndefined;
            }
        }
        break;
    case NVX_IndexParamPreScalerEnableForCapture:
        {
            NvBool bEnabled =
                (NvBool)((NVX_PARAM_PRESCALER_ENABLE_FOR_CAPTURE*)pComponentParameterStructure)->Enable;
            NvError Err = NvSuccess;
            OMX_ERRORTYPE eError = OMX_ErrorNone;

            // make sure that camera block is open
            eError = NvxCameraGetTransformData(pNvxCamera, pComponent);
            if (eError != OMX_ErrorNone)
            {
                 return eError;
            }

            Err = pNvxCamera->oSrcCamera.hBlock->SetAttribute(
                pNvxCamera->oSrcCamera.hBlock,
                NvMMCameraAttribute_PreScalerEnableForCapture,
                0,
                sizeof(NvBool),
                &bEnabled);
            if (Err != NvSuccess)
            {
                return OMX_ErrorUndefined;
            }
        }
        break;
    case NVX_IndexParamPreviewMode:
        {
            NvError err = NvSuccess;
            NVX_PARAM_PREVIEWMODE *pPreviewMode =
                (NVX_PARAM_PREVIEWMODE*)pComponentParameterStructure;
            OMX_ERRORTYPE eError = OMX_ErrorNone;

            NvMMCameraSensorMode Mode;
            NvOsMemset(&Mode, 0, sizeof(Mode));

            // make sure that camera block is open
            eError = NvxCameraGetTransformData(pNvxCamera, pComponent);
            if (eError != OMX_ErrorNone)
            {
                 return eError;
            }

            Mode.Resolution.width = pPreviewMode->mMode.width;
            Mode.Resolution.height = pPreviewMode->mMode.height;
            Mode.FrameRate = pPreviewMode->mMode.FrameRate;
            err = pNvxCamera->oSrcCamera.hBlock->SetAttribute(
                pNvxCamera->oSrcCamera.hBlock,
                NvMMCameraAttribute_PreviewSensorMode,
                0,
                sizeof(NvMMCameraSensorMode),
                &Mode);
            if (err != NvSuccess)
            {
                return OMX_ErrorBadParameter;
            }
        }
        break;
    case NVX_IndexParamRawOutput:
    {
        // Disable normalization when raw output is enabled
        NvError Err = NvSuccess;
        OMX_ERRORTYPE eError = OMX_ErrorNone;
        OMX_CONFIG_BOOLEANTYPE *pRawOutput = (OMX_CONFIG_BOOLEANTYPE*)pComponentParameterStructure;
        NvBool Enabled = (pRawOutput->bEnabled == OMX_TRUE ? NV_FALSE : NV_TRUE);

        eError = NvxCameraGetTransformData(pNvxCamera, pComponent);
        if (eError != OMX_ErrorNone)
        {
            return eError;
        }

        Err = pNvxCamera->oSrcCamera.hBlock->SetAttribute(pNvxCamera->oSrcCamera.hBlock,
                NvMMCameraAttribute_SensorNormalization, 0, sizeof(NvBool), &Enabled);
        if (Err != NvSuccess)
        {
            return OMX_ErrorUndefined;
        }
    }
    break;
    case NVX_IndexParamEmbedRmSurface:
    {
        OMX_CONFIG_BOOLEANTYPE *param = (OMX_CONFIG_BOOLEANTYPE *)pComponentParameterStructure;
        if (param) {
            pNvxCamera->oSrcCamera.bEmbedRmSurface = param->bEnabled;
            pNvxCamera->oDigitalZoom.bEmbedRmSurface = param->bEnabled;
        }

    }
    break;

    case NVX_IndexParamSurfaceLayout:
    {
        NvError Err = NvSuccess;
        NVX_PARAM_SURFACELAYOUT *VideoLayout = (NVX_PARAM_SURFACELAYOUT*)pComponentParameterStructure;

        if(VideoLayout->nPortIndex != (NvU32)s_nvx_port_video_capture)
            return OMX_ErrorBadParameter;

        pNvxCamera->VideoSurfLayout = VideoLayout->bTiledMode ?
                                        NvRmSurfaceLayout_Tiled : NvRmSurfaceLayout_Pitch;
        NvOsDebugPrintf("NvxCameraSetParameter  VideoSurfLayout = %d\n",pNvxCamera->VideoSurfLayout);

        Err = pNvxCamera->oSrcCamera.hBlock->SetAttribute(pNvxCamera->oSrcCamera.hBlock,
            NvMMCameraAttribute_VideoSurfLayout, 0, sizeof(NvRmSurfaceLayout), &pNvxCamera->VideoSurfLayout);
        if (Err != NvSuccess)
            return OMX_ErrorUndefined;
    }
    break;

    default:
        return NvxComponentBaseSetParameter(pComponent, nIndex, pComponentParameterStructure);

    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxCameraGetConfig(OMX_IN NvxComponent* pNvComp,
                                        OMX_IN OMX_INDEXTYPE nIndex,
                                        OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    SNvxCameraData *pNvxCamera;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvError Err = NvSuccess;
    NvMMBlockHandle cb, zb;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxCameraGetConfig\n"));

    pNvxCamera = (SNvxCameraData *)pNvComp->pComponentData;
    NV_ASSERT(pNvxCamera);

    eError = NvxCameraGetTransformData(pNvxCamera, pNvComp);
    if (eError != OMX_ErrorNone)
    {
        return eError;
    }

    cb = pNvxCamera->oSrcCamera.hBlock;
    zb = pNvxCamera->oDigitalZoom.hBlock;

    switch (nIndex)
    {
        case NVX_IndexConfigGetNVMMBlock:
        {
            NVX_CONFIG_GETNVMMBLOCK *pGetBlock = (NVX_CONFIG_GETNVMMBLOCK *)pComponentConfigStructure;

            if (pGetBlock->nPortIndex != (NvU32)s_nvx_port_preview &&
                pGetBlock->nPortIndex != (NvU32)s_nvx_port_still_capture &&
                pGetBlock->nPortIndex != (NvU32)s_nvx_port_video_capture &&
                pGetBlock->nPortIndex != (NvU32)s_nvx_port_thumbnail)
            {
                return OMX_ErrorBadParameter;
            }

            pGetBlock->pNvMMTransform = &pNvxCamera->oDigitalZoom;

            if (pGetBlock->nPortIndex == (NvU32)s_nvx_port_preview)
            {
                pGetBlock->nNvMMPort = s_nvx_port_dz_output_preview;
            }
            else if (pGetBlock->nPortIndex == (NvU32)s_nvx_port_still_capture)
            {
                pGetBlock->nNvMMPort = s_nvx_port_dz_output_still;
            }
            else if (pGetBlock->nPortIndex == (NvU32)s_nvx_port_video_capture)
            {
                pGetBlock->nNvMMPort = s_nvx_port_dz_output_video;
            }
            else if (pGetBlock->nPortIndex == (NvU32)s_nvx_port_thumbnail)
            {
                pGetBlock->nNvMMPort = s_nvx_port_dz_output_thumbnail;
            }

            return eError;
        }

        case NVX_IndexConfigSmoothZoomTime:
            {
                OMX_PARAM_U32TYPE *nsztms = (OMX_PARAM_U32TYPE*)pComponentConfigStructure;
                NvU32 ztime = 0;
                Err = zb->GetAttribute(zb, NvMMDigitalZoomAttributeType_SmoothZoomTime,
                                       sizeof(NvU32), &ztime);
                if (Err != NvSuccess)
                    return OMX_ErrorUndefined;
                nsztms->nU32 = (OMX_U32)ztime;
            }
            break;

        case NVX_IndexConfigSensorModeList:
          eError = NvxCameraGetSensorModes
              (pNvxCamera, (NVX_CONFIG_SENSORMODELIST *) pComponentConfigStructure);
          break;

        case NVX_IndexConfigProfile:
        {
            NVX_CONFIG_PROFILE *pProf = (NVX_CONFIG_PROFILE *)pComponentConfigStructure;
            NvMMDigitalZoomProfilingData ProfilingData;

            if (!pNvxCamera->oDigitalZoom.bProfile)
                return OMX_ErrorBadParameter;

            NvOsMemset(&ProfilingData, 0, sizeof(NvMMDigitalZoomProfilingData));
            Err = zb->GetAttribute(zb, NvMMDigitalZoomAttributeType_ProfilingData,
                                   sizeof(NvMMDigitalZoomProfilingData),
                                   &ProfilingData);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            pProf->nTSPreviewStart = (OMX_U64)ProfilingData.PreviewFirstFrame;
            pProf->nTSPreviewEnd = (OMX_U64)ProfilingData.PreviewLastFrame;
            pProf->nPreviewStartFrameCount = (OMX_U32)ProfilingData.PreviewStartFrameCount;
            pProf->nPreviewEndFrameCount = (OMX_U32)ProfilingData.PreviewEndFrameCount;
            if (pProf->nPortIndex == s_nvx_port_still_capture)
            {
                pProf->nTSCaptureStart = (OMX_U64)ProfilingData.StillFirstFrame;
                pProf->nTSCaptureEnd = (OMX_U64)ProfilingData.StillLastFrame;
                pProf->nCaptureStartFrameCount = (OMX_U32)ProfilingData.StillStartFrameCount;
                pProf->nCaptureEndFrameCount = (OMX_U32)ProfilingData.StillEndFrameCount;
                pProf->nTSStillConfirmationFrame = (OMX_U64)ProfilingData.StillConfirmationFrame;
                pProf->nTSFirstPreviewFrameAfterStill = (OMX_U64)ProfilingData.FirstPreviewFrameAfterStill;
            }
            else if (pProf->nPortIndex == s_nvx_port_video_capture)
            {
                pProf->nTSCaptureStart = (OMX_U64)ProfilingData.VideoFirstFrame;
                pProf->nTSCaptureEnd = (OMX_U64)ProfilingData.VideoLastFrame;
                pProf->nCaptureStartFrameCount = (OMX_U32)ProfilingData.VideoStartFrameCount;
                pProf->nCaptureEndFrameCount = (OMX_U32)ProfilingData.VideoEndFrameCount;
            }
            else
            {
                // only supported on still/video capture ports
                return OMX_ErrorBadPortIndex;
            }

            {
                NvU32 BadFrameCount;
                NvCameraIspISO iso;
                NvCameraIspExposureTime et;
                OMX_S32 exposureMS;
                Err = cb->GetAttribute(cb, NvCameraIspAttribute_ExposureTime,
                                       sizeof(et), &et);
                if (Err != NvSuccess)
                    return OMX_ErrorUndefined;

                //need to round up the exposure time returned
                //since otherwise 10ms in becomes 9ms out.
                //this is v. bad if a user is calling this get to
                //initialize their range data for e.g. setting ISO
                //since they will inadvertantly change their exposure
                //along with the ISO value when they call set again.
                exposureMS=(OMX_S32)(et.value * 1000 + 0.5);
                pProf->xExposureTime = exposureMS;
                Err = cb->GetAttribute(cb, NvCameraIspAttribute_EffectiveISO,
                                       sizeof(iso), &iso);
                if (Err != NvSuccess)
                    return OMX_ErrorUndefined;
                pProf->nExposureISO = (OMX_S32)iso.value;

                Err = cb->GetAttribute(cb, NvMMCameraAttribute_IgnoreTimeouts,
                                       sizeof(NvU32), &BadFrameCount);

                if (Err != NvSuccess)
                    return OMX_ErrorUndefined;

                pProf->nBadFrameCount = (OMX_U32)BadFrameCount;
            }
            break;
        }

        case OMX_IndexConfigCommonDigitalZoom:
        {
            OMX_CONFIG_SCALEFACTORTYPE *sf =
                (OMX_CONFIG_SCALEFACTORTYPE *) pComponentConfigStructure;
            NvSFx zoom;

            Err = zb->GetAttribute(zb, NvMMDigitalZoomAttributeType_ZoomFactor,
                                   sizeof(NvSFx), &zoom);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            sf->xWidth = sf->xHeight = zoom;
            break;
        }
        case NVX_IndexConfigExposureTime:
        {
            NvCameraIspExposureTime et;
            NVX_CONFIG_EXPOSURETIME *pet =
                    (NVX_CONFIG_EXPOSURETIME *)pComponentConfigStructure;
            Err = cb->GetAttribute(cb, NvCameraIspAttribute_ExposureTime,
                                   sizeof(et), &et);
            if (Err != NvSuccess) {
                return OMX_ErrorUndefined;
            }

            pet->nExposureTime = (NVX_F32) et.value;
            pet->bAutoShutterSpeed = (OMX_BOOL) et.isAuto;
            break;
        }
        case OMX_IndexConfigCommonExposureValue:
        {
            NvSFx data;
            NvCameraIspISO iso;
            NvCameraIspExposureTime et;
            OMX_U32 exposureMS;
            OMX_CONFIG_EXPOSUREVALUETYPE *pev =
                    (OMX_CONFIG_EXPOSUREVALUETYPE *)pComponentConfigStructure;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_ExposureTime,
                                   sizeof(et), &et);
            if (Err != NvSuccess) {
                return OMX_ErrorUndefined;
            }
            //need to round up the exposure time returned
            //since otherwise 10ms in becomes 9ms out.
            //this is v. bad if a user is calling this get to
            //initialize their range data for e.g. setting ISO
            //since they will inadvertantly change their exposure
            //along with the ISO value when they call set again.
            exposureMS=(OMX_S32)(et.value * 1000 + 0.5);

            pev->nShutterSpeedMsec = exposureMS;
            pev->bAutoShutterSpeed = (OMX_BOOL)et.isAuto;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_EffectiveISO,
                                   sizeof(iso), &iso);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;
            pev->nSensitivity = iso.value;
            pev->bAutoSensitivity = (OMX_BOOL)iso.isAuto;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_ExposureCompensation,
                                   sizeof(NvSFx), &(pev->xEVCompensation));
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_MeteringMode,
                                   sizeof(NvS32), &data);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            switch (data) {
            case NVCAMERAISP_METERING_MATRIX:
                pev->eMetering = OMX_MeteringModeMatrix;
                break;
            case NVCAMERAISP_METERING_SPOT:
                pev->eMetering = OMX_MeteringModeSpot;
                break;
            case NVCAMERAISP_METERING_CENTER:
            default:
                pev->eMetering = OMX_MeteringModeAverage;
                break;
            }
            break;
        }
        case OMX_IndexConfigCommonExposure:
        {
            NvBool data = NV_TRUE;
            OMX_CONFIG_EXPOSURECONTROLTYPE *pec =
                (OMX_CONFIG_EXPOSURECONTROLTYPE *)pComponentConfigStructure;
            Err = cb->GetAttribute(cb, NvCameraIspAttribute_ContinuousAutoExposure,
                                   sizeof(data), &data);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            pec->eExposureControl = data ? OMX_ExposureControlAuto : OMX_ExposureControlOff;
            break;
        }
        case NVX_IndexConfigAutoFrameRate:
        {
            NvBool Enable = NV_FALSE;
            NvCameraIspRange Range;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_AutoFrameRate,
                                   sizeof(NvBool), &Enable);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            if (Enable) {
                cb->GetAttribute(cb, NvCameraIspAttribute_AutoFrameRateRange,
                                 sizeof(NvCameraIspRange), &Range);
                pNvxCamera->oAutoFrameRate.bEnabled = OMX_TRUE;
                pNvxCamera->oAutoFrameRate.low = Range.low;
                pNvxCamera->oAutoFrameRate.high = Range.high;


            }
            else
                pNvxCamera->oAutoFrameRate.bEnabled = OMX_FALSE;

            NvOsMemcpy(pComponentConfigStructure, &(pNvxCamera->oAutoFrameRate),
                       sizeof(NVX_CONFIG_AUTOFRAMERATE));
            break;
        }
        case NVX_IndexConfigExposureTimeRange:
        {
            NvCameraIspRange Range;
            NVX_CONFIG_EXPOSURETIME_RANGE omxRange;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_ExposureTimeRange,
                                   sizeof(NvCameraIspRange), &Range);
            if (Err != NvSuccess)
            {
                return OMX_ErrorUndefined;
            }
            INIT_STRUCT(omxRange,pNvComp);
            omxRange.low = (OMX_U32)NV_SFX_TO_WHOLE(Range.low);
            omxRange.high = (OMX_U32)NV_SFX_TO_WHOLE(Range.high);
            NvOsMemcpy(pComponentConfigStructure, &omxRange, sizeof(omxRange));
            break;
        }
        case NVX_IndexConfigSensorETRange:
        {
            NvCameraIspRangeF32 Range;
            NVX_CONFIG_SENSOR_ET_RANGE *pOmxRange =
                (NVX_CONFIG_SENSOR_ET_RANGE *)pComponentConfigStructure;
            INIT_STRUCT(*pOmxRange,pNvComp);

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_SensorETRange,
                                   sizeof(NvCameraIspRange), &Range);
            if (Err != NvSuccess)
            {
                return OMX_ErrorUndefined;
            }
            pOmxRange->low = (NVX_F32)Range.low;
            pOmxRange->high = (NVX_F32)Range.high;
            break;
        }
        case NVX_IndexConfigFuseId:
        {
            NvCameraFuseId FuseId;
            OMX_U32 i;
            NVX_CONFIG_SENSOR_FUSE_ID *pOmxFuseId =
                (NVX_CONFIG_SENSOR_FUSE_ID *)pComponentConfigStructure;

            INIT_STRUCT(*pOmxFuseId,pNvComp);

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_FuseId,
                                   sizeof(NvCameraFuseId), &FuseId);
            if (Err != NvSuccess)
            {
                return OMX_ErrorUndefined;
            }
            pOmxFuseId->fidSize = FuseId.size;
            for (i=0; i<NVX_MAX_FUSE_ID_SIZE; i++) {
                pOmxFuseId->fidBytes[i] = FuseId.data[i];
            }
            break;
        }
        case NVX_IndexConfigISOSensitivityRange:
        {
            NvCameraIspRange Range;
            NVX_CONFIG_ISOSENSITIVITY_RANGE omxRange;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_ISORange,
                                   sizeof(NvCameraIspRange), &Range);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            INIT_STRUCT(omxRange,pNvComp);
            omxRange.low = (OMX_U32)NV_SFX_TO_WHOLE(Range.low);
            omxRange.high = (OMX_U32)NV_SFX_TO_WHOLE(Range.high);
            NvOsMemcpy(pComponentConfigStructure, &omxRange, sizeof(omxRange));
            break;
        }
        case NVX_IndexConfigWhitebalanceCCTRange:
        {
            NvCameraIspRange Range;
            NVX_CONFIG_CCTWHITEBALANCE_RANGE omxRange;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_WhiteBalanceCCTRange,
                                   sizeof(NvCameraIspRange), &Range);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            INIT_STRUCT(omxRange,pNvComp);
            omxRange.low = (OMX_U32)Range.low;
            omxRange.high = (OMX_U32)Range.high;
            NvOsMemcpy(pComponentConfigStructure, &omxRange, sizeof(omxRange));
            break;
        }
        case NVX_IndexConfigFocuserParameters:
        {
            NVX_CONFIG_FOCUSER_PARAMETERS omxFocus;
            NvCameraIspFocusingParameters params;

            INIT_STRUCT(omxFocus,pNvComp);

            NvOsMemset(&params, 0, sizeof(params));
            Err = cb->GetAttribute(cb, NvCameraIspAttribute_FocusPositionRange,
                                   sizeof(params), &params);
            if (Err != NvSuccess) {
                return OMX_ErrorUndefined;
            }
            omxFocus.minPosition = params.minPos;
            omxFocus.maxPosition = params.maxPos;
            omxFocus.hyperfocal  = params.hyperfocal;
            omxFocus.macro       = params.macro;
            omxFocus.powerSaving = params.powerSaving;
            omxFocus.sensorispAFsupport = params.sensorispAFsupport;
            omxFocus.infModeAF   = params.infModeAF;
            omxFocus.macroModeAF = params.macroModeAF;
            NvOsMemcpy(pComponentConfigStructure, &omxFocus, sizeof(omxFocus));
            break;
        }
        case NVX_IndexConfigFlashParameters:
        {
            NVX_CONFIG_FLASH_PARAMETERS omxFlash;
            NvBool present;

            INIT_STRUCT(omxFlash, pNvComp);

            Err = cb->GetAttribute(cb, NvMMCameraAttribute_FlashPresent,
                                   sizeof(present), &present);
            if (Err != NvSuccess) {
                return OMX_ErrorUndefined;
            }
            omxFlash.bPresent = present;
            NvOsMemcpy(pComponentConfigStructure, &omxFlash, sizeof(omxFlash));
            break;
        }
        case OMX_IndexConfigCapturing:
        {
            NvOsMemcpy(pComponentConfigStructure, &(pNvxCamera->bCapturing),
                       sizeof(OMX_CONFIG_BOOLEANTYPE));
            break;
        }
        case NVX_IndexConfigPreviewEnable:
        {

            OMX_CONFIG_BOOLEANTYPE *pe =
                (OMX_CONFIG_BOOLEANTYPE *) pComponentConfigStructure;
            pe->bEnabled = pNvxCamera->PreviewEnabled;
            break;
        }
        case NVX_IndexConfigAlgorithmWarmUp:
        {

            OMX_CONFIG_BOOLEANTYPE *pe =
                (OMX_CONFIG_BOOLEANTYPE *) pComponentConfigStructure;
            pe->bEnabled = pNvxCamera->AlgorithmWarmedUp;
            break;
        }

        case OMX_IndexAutoPauseAfterCapture: //OMX_IndexConfigAutoPauseAfterCapture:
        {
            NvBool Enable;

            Err = cb->GetAttribute
                (cb, NvMMCameraAttribute_PausePreviewAfterStillCapture,
                 sizeof(NvBool), &Enable);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            pNvxCamera->bAutoPauseAfterCapture.bEnabled = Enable ? OMX_TRUE : OMX_FALSE;

            NvOsMemcpy(pComponentConfigStructure, &(pNvxCamera->bAutoPauseAfterCapture),
                       sizeof(OMX_CONFIG_BOOLEANTYPE));
            break;
        }

        case NVX_IndexConfigCustomPostview:
        {
            NvBool Enable;
            NVX_PARAM_CUSTOMPOSTVIEW *cpv =
                (NVX_PARAM_CUSTOMPOSTVIEW *)pComponentConfigStructure;
            Err = zb->GetAttribute
                (zb, NvMMDigitalZoomAttributeType_CustomPostview,
                 sizeof(NvBool), &Enable);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            cpv->bEnable = Enable ? OMX_TRUE : OMX_FALSE;
            break;
        }

        case OMX_IndexConfigCommonWhiteBalance:
        {
            NvBool data = NV_TRUE;
            OMX_CONFIG_WHITEBALCONTROLTYPE *pwb =
                (OMX_CONFIG_WHITEBALCONTROLTYPE *)pComponentConfigStructure;

            Err = cb->GetAttribute
                (cb, NvCameraIspAttribute_ContinuousAutoWhiteBalance,
                 sizeof(data), &data);

            if (Err != NvSuccess)
                return OMX_ErrorInvalidState;

            if(data)
            {
                Err = s_GetWhiteBalance(pNvxCamera, &pwb->eWhiteBalControl);
                if(Err != NvSuccess)
                    return Err;
            }
            else
            {
                pwb->eWhiteBalControl = OMX_WhiteBalControlOff;
            }
            break;
        }

        case OMX_IndexConfigFocusControl:
        {
            NvS32 data = 0;
            NvCameraIspFocusControl control;
            NvCameraIspFocusingParameters params;
            OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE *pfc =
                     (OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE *) pComponentConfigStructure;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_AutoFocusControl,
                                   sizeof(NvCameraIspFocusControl), &control);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            pfc->eFocusControl = control;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_FocusPosition,
                                   sizeof(data), &(data));
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;
            pfc->nFocusStepIndex = data;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_FocusPositionRange,
                                   sizeof(params), &params);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;
            pfc->nFocusSteps = params.maxPos - params.minPos + 1;

            break;
        }

        case NVX_IndexConfigFocusPosition:
        {
            OMX_PARAM_U32TYPE *pFocusPosition = (OMX_PARAM_U32TYPE *)pComponentConfigStructure;
            Err = cb->GetAttribute(cb, NvCameraIspAttribute_FocusPosition,
                                   sizeof(pFocusPosition->nU32), &(pFocusPosition->nU32));
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;
            break;
        }

        case NVX_IndexConfigLensPhysicalAttr:
        {
            NVX_CONFIG_LENS_PHYSICAL_ATTR *lpa =
                (NVX_CONFIG_LENS_PHYSICAL_ATTR*)pComponentConfigStructure;
            NvMMCameraFieldOfView fov;

            Err = cb->GetAttribute(cb, NvMMCameraAttribute_FocalLength,
                                   sizeof(NvF32), &lpa->eFocalLength);

            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            Err = cb->GetAttribute(cb, NvMMCameraAttribute_FieldOfView,
                                   sizeof(NvMMCameraFieldOfView), &fov);

            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            lpa->eHorizontalAngle = (NVX_F32) fov.HorizontalViewAngle;
            lpa->eVerticalAngle = (NVX_F32) fov.VerticalViewAngle;

            break;
        }

        case OMX_IndexConfigFlashControl:
        {
            OMX_ERRORTYPE omxErr = OMX_ErrorNone;
            OMX_IMAGE_PARAM_FLASHCONTROLTYPE *pfc =
                (OMX_IMAGE_PARAM_FLASHCONTROLTYPE*)pComponentConfigStructure;

            omxErr = NvxCameraGetFlashMode(pNvxCamera,pfc);
            if (omxErr != OMX_ErrorNone) {
                return omxErr;
            }
            break;
        }

        case OMX_IndexConfigCommonImageFilter:
        {
            NvU32 data = 0;
            NvBool enable = NV_FALSE;
            OMX_CONFIG_IMAGEFILTERTYPE *pif =
                      (OMX_CONFIG_IMAGEFILTERTYPE *)pComponentConfigStructure;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_Stylize,
                                   sizeof(enable), &enable);
            if (Err != NvSuccess)
            {
                return OMX_ErrorUndefined;
            }

            if (!enable)
            {
                pif->eImageFilter = OMX_ImageFilterNone;
            }
            else
            {
                data = NVCAMERAISP_STYLIZE_SOLARIZE;
                Err = cb->GetAttribute(cb, NvCameraIspAttribute_StylizeMode,
                                       sizeof(NvU32), &data);
                if (Err != NvSuccess)
                    return OMX_ErrorUndefined;

                switch (data)
                {
                    case NVCAMERAISP_STYLIZE_SOLARIZE:
                        pif->eImageFilter = OMX_ImageFilterSolarize;
                        break;

                    case NVCAMERAISP_STYLIZE_NEGATIVE:
                        pif->eImageFilter = OMX_ImageFilterNegative;
                        break;

                    case NVCAMERAISP_STYLIZE_POSTERIZE:
                        pif->eImageFilter = NVX_ImageFilterPosterize;
                        break;

                    case NVCAMERAISP_STYLIZE_SEPIA:
                        pif->eImageFilter = NVX_ImageFilterSepia;
                        break;

                    case NVCAMERAISP_STYLIZE_BW:
                        pif->eImageFilter = NVX_ImageFilterBW;
                        break;

                    case NVCAMERAISP_STYLIZE_AQUA:
                        pif->eImageFilter = NVX_ImageFilterAqua;
                        break;

                    case NVCAMERAISP_STYLIZE_EMBOSS:
                    default:
                        pif->eImageFilter = OMX_ImageFilterEmboss;
                        break;
                }
            }
            break;
        }
        case NVX_IndexConfigColorCorrectionMatrix:
        {
            NVX_CONFIG_COLORCORRECTIONMATRIX *matrix =
                (NVX_CONFIG_COLORCORRECTIONMATRIX *) pComponentConfigStructure;
            matrix->ccMatrix[0] = NV_SFX_TO_FP(pNvxCamera->ccm.m00);
            matrix->ccMatrix[1] = NV_SFX_TO_FP(pNvxCamera->ccm.m01);
            matrix->ccMatrix[2] = NV_SFX_TO_FP(pNvxCamera->ccm.m02);
            matrix->ccMatrix[3] = NV_SFX_TO_FP(pNvxCamera->ccm.m03);
            matrix->ccMatrix[4] = NV_SFX_TO_FP(pNvxCamera->ccm.m10);
            matrix->ccMatrix[5] = NV_SFX_TO_FP(pNvxCamera->ccm.m11);
            matrix->ccMatrix[6] = NV_SFX_TO_FP(pNvxCamera->ccm.m12);
            matrix->ccMatrix[7] = NV_SFX_TO_FP(pNvxCamera->ccm.m13);
            matrix->ccMatrix[8] = NV_SFX_TO_FP(pNvxCamera->ccm.m20);
            matrix->ccMatrix[9] = NV_SFX_TO_FP(pNvxCamera->ccm.m21);
            matrix->ccMatrix[10] = NV_SFX_TO_FP(pNvxCamera->ccm.m22);
            matrix->ccMatrix[11] = NV_SFX_TO_FP(pNvxCamera->ccm.m23);
            matrix->ccMatrix[12] = NV_SFX_TO_FP(pNvxCamera->ccm.m30);
            matrix->ccMatrix[13] = NV_SFX_TO_FP(pNvxCamera->ccm.m31);
            matrix->ccMatrix[14] = NV_SFX_TO_FP(pNvxCamera->ccm.m32);
            matrix->ccMatrix[15] = NV_SFX_TO_FP(pNvxCamera->ccm.m33);
            break;
        }
        case OMX_IndexConfigCommonContrast:
        {
            NvF32 Contrast = 0.0f;
            OMX_CONFIG_CONTRASTTYPE *pct = (OMX_CONFIG_CONTRASTTYPE *)pComponentConfigStructure;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_ContrastEnhancement,
                                   sizeof(NvF32), &Contrast);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            pct->nContrast = (OMX_S32)(Contrast * 100);
            break;
        }
        case OMX_IndexConfigCommonOutputCrop:
        {
            OMX_CONFIG_RECTTYPE *pRect = (OMX_CONFIG_RECTTYPE *)pComponentConfigStructure;
            NvError err = NvSuccess;

            err = s_GetCrop(pNvxCamera, pRect->nPortIndex);
            if (err != NvSuccess)
            {
                return OMX_ErrorInvalidState;
            }

            if (s_nvx_port_preview == pRect->nPortIndex)
            {
                NvOsMemcpy(
                    pComponentConfigStructure,
                    &(pNvxCamera->oCropPreview),
                    sizeof(OMX_CONFIG_RECTTYPE));
            }
            else if (s_nvx_port_still_capture == pRect->nPortIndex)
            {
                NvOsMemcpy(
                    pComponentConfigStructure,
                    &(pNvxCamera->oCropStillCapture),
                    sizeof(OMX_CONFIG_RECTTYPE));
            }
            else if (s_nvx_port_video_capture == pRect->nPortIndex)
            {
                NvOsMemcpy(
                    pComponentConfigStructure,
                    &(pNvxCamera->oCropVideoCapture),
                    sizeof(OMX_CONFIG_RECTTYPE));
            }
            else if (s_nvx_port_thumbnail == pRect->nPortIndex)
            {
                NvOsMemcpy(
                    pComponentConfigStructure,
                    &(pNvxCamera->oCropThumbnail),
                    sizeof(OMX_CONFIG_RECTTYPE));
            }
            else
            {
                eError = OMX_ErrorBadPortIndex;
            }

            return eError;
        }
        case NVX_IndexConfigEdgeEnhancement:
        {
            NVX_CONFIG_EDGEENHANCEMENT *peh =
                (NVX_CONFIG_EDGEENHANCEMENT *) pComponentConfigStructure;
            NvError err = NvSuccess;
            NvBool bEnable = NV_FALSE;
            NvF32 strengthBias = (NvF32)0.0f;


            peh->bEnable = OMX_FALSE;
            peh->strengthBias = (NVX_F32)0.0f;

            err = cb->GetAttribute(cb, NvCameraIspAttribute_EdgeEnhancement,
                                   sizeof(NvBool),&bEnable);
            if (err != NvSuccess) {
                eError = OMX_ErrorInvalidState;
                break;
            }
            if (!bEnable) {
                break;
            }
            err = cb->GetAttribute(cb, NvCameraIspAttribute_EdgeEnhanceStrengthBias,
                                   sizeof(NvF32),&strengthBias);
            if (err != NvSuccess) {
                eError = OMX_ErrorInvalidState;
            } else {
                peh->bEnable = OMX_TRUE;
                peh->strengthBias = (NVX_F32) strengthBias;
            }
            break;
        }
        case NVX_IndexConfigConvergeAndLock:
          NvOsMemcpy(pComponentConfigStructure, &(pNvxCamera->oConvergeAndLock),
                     sizeof(NVX_CONFIG_CONVERGEANDLOCK));
          break;
        case OMX_IndexConfigCommonFrameStabilisation:
        {
            NvCameraIspStabilizationMode Mode;
            OMX_CONFIG_FRAMESTABTYPE *pfs = (OMX_CONFIG_FRAMESTABTYPE *)pComponentConfigStructure;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_Stabilization,
                                   sizeof(NvCameraIspStabilizationMode), &Mode);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            pNvxCamera->bStab = (Mode == NvCameraIspStabilizationMode_None) ? OMX_FALSE : OMX_TRUE;

            pfs->bStab = pNvxCamera->bStab;
            break;
        }
        case NVX_IndexConfigConcurrentRawDumpFlag:
        {
            OMX_PARAM_U32TYPE *pRawDumpFlag = (OMX_PARAM_U32TYPE *)pComponentConfigStructure;
            pRawDumpFlag->nU32 = pNvxCamera->rawDumpFlag;
            break;
        }
        case NVX_IndexConfigFlicker:
        {
            NvBool Enable = NV_FALSE;
            NvS32 FlickerFrequency = 0;
            NVX_CONFIG_FLICKER *pf = (NVX_CONFIG_FLICKER *)pComponentConfigStructure;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_AntiFlicker,
                                   sizeof(NvBool), &Enable);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            if (Enable) {
                Err = cb->GetAttribute(cb, NvCameraIspAttribute_FlickerFrequency,
                                       sizeof(NvS32), &FlickerFrequency);
                if (Err != NvSuccess)
                    return OMX_ErrorUndefined;

                switch(FlickerFrequency) {
                    case NVCAMERAISP_FLICKER_AUTO:
                        pf->eFlicker = NvxFlicker_Auto;
                        break;
                    case NVCAMERAISP_FLICKER_50HZ:
                        pf->eFlicker = NvxFlicker_50HZ;
                        break;
                    case NVCAMERAISP_FLICKER_60HZ:
                        pf->eFlicker = NvxFlicker_60HZ;
                        break;
                    default:
                        pf->eFlicker = NvxFlicker_Off;
                        return OMX_ErrorBadParameter;
                }
            } else pf->eFlicker = NvxFlicker_Off;

            break;
        }
        case NVX_IndexConfigIspData:
        {
            NvF32 ilData;
            NVX_CONFIG_ISPDATA *pid = (NVX_CONFIG_ISPDATA *)pComponentConfigStructure;
            Err = cb->GetAttribute(cb, NvMMCameraAttribute_Illuminant,
                                   sizeof(NvF32), &ilData);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            pid->ilData = ilData;
            break;
        }
        case NVX_IndexConfigPreCaptureConverge:
        {
            NvOsMemcpy(pComponentConfigStructure, &(pNvxCamera->oPrecaptureConverge),
                       sizeof(NVX_CONFIG_PRECAPTURECONVERGE));
            break;
        }
        case NVX_IndexConfigExposureRegionsRect:
        {
            OMX_U32 id;
            NvCameraIspExposureRegions oExposureRegions;
            NVX_CONFIG_ExposureRegionsRect *pExpRegRect =
                (NVX_CONFIG_ExposureRegionsRect *) pComponentConfigStructure;

            if (pNvxCamera->ExposureRegions.nRegions == 0)
            {
                Err = cb->GetAttribute(
                    cb,
                    NvCameraIspAttribute_AutoExposureRegions,
                    sizeof(oExposureRegions),
                    &oExposureRegions);
                if (Err != NvSuccess)
                {
                    return OMX_ErrorUndefined;
                }

                pExpRegRect->nRegions = (OMX_U32) oExposureRegions.numOfRegions;
                for (id = 0; id < oExposureRegions.numOfRegions; id++)
                {
                    pExpRegRect->regions[id].left = oExposureRegions.regions[id].left;
                    pExpRegRect->regions[id].right = oExposureRegions.regions[id].right;
                    pExpRegRect->regions[id].top = oExposureRegions.regions[id].top;
                    pExpRegRect->regions[id].bottom = oExposureRegions.regions[id].bottom;
                }
            }
            else
            {
                NvOsMemcpy(
                    pExpRegRect,
                    &pNvxCamera->ExposureRegions,
                    sizeof(NVX_CONFIG_ExposureRegionsRect));
            }
            break;
        }
        case NVX_IndexConfigFocusRegionsRect:
        {
            OMX_U32 id;
            NvCameraIspFocusingRegions oFocusRegions;
            NVX_CONFIG_FocusRegionsRect *pfrr =
                (NVX_CONFIG_FocusRegionsRect *) pComponentConfigStructure;

            if (pNvxCamera->FocusRegions.nRegions == 0)
            {
                Err = cb->GetAttribute(cb, NvCameraIspAttribute_AutoFocusRegions,
                                   sizeof(oFocusRegions), &oFocusRegions);
                if (Err != NvSuccess)
                        return OMX_ErrorUndefined;

                pfrr->nRegions = (OMX_U32)oFocusRegions.numOfRegions;
                for (id=0; id<oFocusRegions.numOfRegions; id++)
                {
                    pfrr->regions[id].left = oFocusRegions.regions[id].left;
                    pfrr->regions[id].right = oFocusRegions.regions[id].right;
                    pfrr->regions[id].top = oFocusRegions.regions[id].top;
                    pfrr->regions[id].bottom = oFocusRegions.regions[id].bottom;
                }
            }
            else
            {
                NvOsMemcpy(pfrr, &pNvxCamera->FocusRegions,
                    sizeof(NVX_CONFIG_FocusRegionsRect));
            }
            break;
        }

        case NVX_IndexConfigFocusRegionsSharpness:
        {
            NVX_CONFIG_FOCUSREGIONS_SHARPNESS *pS =
                (NVX_CONFIG_FOCUSREGIONS_SHARPNESS *)pComponentConfigStructure;
            NvCameraIspFocusingRegionsSharpness sharpData;
            OMX_U32 i,n;

            Err = cb->GetAttribute
                (cb, NvMMCameraAttribute_AutoFocusRegionSharpness,
                 sizeof(sharpData), &sharpData);
            if (Err != NvSuccess) {
                pS->nSize = 0;
                return OMX_ErrorBadParameter;
            }
            n = sharpData.numOfRegions;
            if (n > NVX_MAX_FOCUS_REGIONS)
                n = NVX_MAX_FOCUS_REGIONS;
            for (i = 0; i < n; i++) {
                pS->nSharpness[i] = (NVX_F32)sharpData.regions[i];
            }
            pS->nRegions = n;
            break;
        }

        case NVX_IndexConfigFocuserCapabilities:
        {
            NVX_CONFIG_FOCUSER_CAPABILITIES *pCap =
                (NVX_CONFIG_FOCUSER_CAPABILITIES *)pComponentConfigStructure;
            NvMMCameraFocuserCapabilities focuserCaps;

            Err = cb->GetAttribute(cb, NvMMCameraAttribute_FocuserCapabilities,
                                   sizeof(focuserCaps),
                                   &focuserCaps);
            if (Err != NvSuccess) {
                pCap->nRange = 0;
                return OMX_ErrorBadParameter;
            }
            pCap->nRange = focuserCaps.positionActualHigh - focuserCaps.positionActualLow;
            pCap->nSettleTime = focuserCaps.afConfigSet[0].settle_time;
            break;
        }

        case NVX_IndexConfigFocuserInfo:
        {
            NvCameraIspAfConfig afConfig;
            NVX_CONFIG_FOCUSER_INFO *pInfo =
                (NVX_CONFIG_FOCUSER_INFO *)pComponentConfigStructure;
            INIT_STRUCT(*pInfo,pNvComp);

            NvOsMemset(&afConfig, 0, sizeof(afConfig));
            Err = cb->GetAttribute(cb, NvCameraIspAttribute_AfConfig,
                                   sizeof(afConfig), &afConfig);
            if (Err != NvSuccess) {
                return OMX_ErrorUndefined;
            }

            // INIT_STRUCT sets pInfo->nVersion and zeros out struct
            pInfo->nSize = sizeof(NVX_CONFIG_FOCUSER_INFO);
            pInfo->positionPhysicalLow = afConfig.positionPhysicalLow;
            pInfo->positionPhysicalHigh = afConfig.positionPhysicalHigh;
            pInfo->rangeEndsReversed = afConfig.rangeEndsReversed;
            pInfo->hysteresis = afConfig.hysteresis;

            pInfo->slewRate = afConfig.slewRate;
            pInfo->settleTime = afConfig.settleTime;
            pInfo->positionHyperFocal = afConfig.positionHyperFocal;
            pInfo->positionResting = afConfig.positionResting;
            // positionInf + positionInfOffset will result in positionWorkingLow
            pInfo->positionInf = afConfig.positionInf;
            pInfo->positionInfOffset = afConfig.positionInfOffset;
            // positionMacro + positionMacroOffset will result in positionWorkingHigh
            pInfo->positionMacro = afConfig.positionMacro;
            pInfo->positionMacroOffset = afConfig.positionMacroOffset;
            pInfo->infMin = afConfig.infMin;
            pInfo->macroMax = afConfig.macroMax;
            pInfo->infInitSearch = afConfig.infInitSearch;
            pInfo->macroInitSearch = afConfig.macroInitSearch;

            break;
        }

        case NVX_IndexConfigAutofocusSpeed:
        {
            NVX_CONFIG_AUTOFOCUS_SPEED *afSpeed =
                (NVX_CONFIG_AUTOFOCUS_SPEED *) pComponentConfigStructure;
            NvU32 val;
            cb->GetAttribute(cb, NvMMCameraAttribute_AFSweepType,
                             sizeof(val), &val);
            afSpeed->bFast = val ? OMX_FALSE : OMX_TRUE;
            break;
        }

        case OMX_IndexConfigCommonBrightness:
        {
            OMX_CONFIG_BRIGHTNESSTYPE *pl =
                (OMX_CONFIG_BRIGHTNESSTYPE *) pComponentConfigStructure;
            NvF32 Brightness;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_Brightness,
                                   sizeof(NvF32), &Brightness);
            if (Err != NvSuccess)
                return OMX_ErrorBadParameter;

            // Map [0,3] to [0,100]
            pl->nBrightness = (OMX_S32)(((Brightness * 67.0f - 1.0f) / 2.0f) + 0.5f);

            break;
        }

        case OMX_IndexConfigCommonSaturation:
        {
            OMX_CONFIG_SATURATIONTYPE *ps =
                (OMX_CONFIG_SATURATIONTYPE *) pComponentConfigStructure;
            NvF32 Saturation;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_Saturation,
                                   sizeof(NvF32), &Saturation);
            if (Err != NvSuccess)
                return OMX_ErrorBadParameter;

            ps->nSaturation = (OMX_S32)((Saturation - 1) * 100.0f);
            break;
        }

        case NVX_IndexConfigHue:
        {
            NVX_CONFIG_HUETYPE *ph =
                (NVX_CONFIG_HUETYPE *) pComponentConfigStructure;
            NvF32 Hue;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_Hue,
                                   sizeof(NvF32), &Hue);
            if (Err != NvSuccess)
                return OMX_ErrorBadParameter;

            ph->nHue = (OMX_S32)Hue;
            break;
        }

        case OMX_IndexConfigCommonGamma:
        {
            OMX_CONFIG_GAMMATYPE *pGamma =
                (OMX_CONFIG_GAMMATYPE *) pComponentConfigStructure;
            NvSFx Gamma;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_ImageGamma,
                                   sizeof(NvSFx), &Gamma);
            if (Err != NvSuccess)
                return OMX_ErrorBadParameter;

            if (Gamma == NVCAMERAISP_IMAGEGAMMA_SRGB) {
                // NvSFx is Q16 (S15.16)
                pGamma->nGamma = (OMX_S32)NV_SFX_FP_TO_FX((NvF32)0.45);
            } else {
                // NvSFx is Q16 (S15.16)
                pGamma->nGamma = (OMX_S32)Gamma;
            }

            break;
        }

        case NVX_IndexConfigPreviewFrameCopy:
        {
            NVX_CONFIG_PREVIEW_FRAME_COPY *pThing =
                (NVX_CONFIG_PREVIEW_FRAME_COPY*) pComponentConfigStructure;
            NvMMDigitalZoomFrameCopyEnable FrameCopyEnable;

            zb->GetAttribute(zb, NvMMDigitalZoomAttributeType_PreviewFrameCopy,
                             sizeof(FrameCopyEnable), &FrameCopyEnable);
            if (Err != NvSuccess)
                return OMX_ErrorBadParameter;

            pThing->enable = FrameCopyEnable.Enable;
            pThing->skipCount = FrameCopyEnable.skipCount;

            break;
        }

        case NVX_IndexConfigStillConfirmationFrameCopy:
        {
            NVX_CONFIG_STILL_CONFIRMATION_FRAME_COPY *pThing =
                (NVX_CONFIG_STILL_CONFIRMATION_FRAME_COPY*) pComponentConfigStructure;
            NvBool enable;

            zb->GetAttribute
                (zb, NvMMDigitalZoomAttributeType_StillConfirmationFrameCopy,
                 sizeof(NvBool), &enable);
            if (Err != NvSuccess)
                return OMX_ErrorBadParameter;

            pThing->enable = enable;

            break;
        }

        case NVX_IndexConfigStillFrameCopy:
        {
            NVX_CONFIG_STILL_YUV_FRAME_COPY *pThing =
                (NVX_CONFIG_STILL_YUV_FRAME_COPY*) pComponentConfigStructure;
            NvMMDigitalZoomFrameCopy FrameCopy;

            zb->GetAttribute
                (zb, NvMMDigitalZoomAttributeType_StillFrameCopy,
                 sizeof(FrameCopy), &FrameCopy);
            if (Err != NvSuccess)
                return OMX_ErrorBadParameter;

            pThing->enable = FrameCopy.Enable;

            break;
        }

        case NVX_IndexConfigCameraTestPattern:
        {
            NVX_CONFIG_CAMERATESTPATTERN *pTestPattern =
                (NVX_CONFIG_CAMERATESTPATTERN *) pComponentConfigStructure;
            NvBool EnableTestPattern;

            Err = cb->GetAttribute(cb, NvMMCameraAttribute_TestPattern,
                                   sizeof(NvBool), &EnableTestPattern);
            if (Err != NvSuccess)
                return OMX_ErrorBadParameter;

            pTestPattern->TestPatternIndex = (OMX_U32) EnableTestPattern;

            break;
        }

        case NVX_IndexConfigBurstSkipCount:
        {
            OMX_PARAM_U32TYPE *pBurstSkipCount =
                (OMX_PARAM_U32TYPE *) pComponentConfigStructure;
            pBurstSkipCount->nU32 = pNvxCamera->nBurstInterval;
            break;
        }

        case OMX_IndexConfigCaptureMode:
        {
            OMX_CONFIG_CAPTUREMODETYPE *cmode =
                       (OMX_CONFIG_CAPTUREMODETYPE *)pComponentConfigStructure;
            // not supported on the new dedicated video capture port
            if (cmode->nPortIndex != (OMX_U32)s_nvx_port_still_capture)
            {
                return OMX_ErrorBadPortIndex;
            }
            cmode->bContinuous = OMX_FALSE;
            cmode->bFrameLimited = OMX_TRUE;
            cmode->nFrameLimit = pNvxCamera->nCaptureCount;
            break;
        }

        case NVX_IndexConfigStereoCapable:
        {
            NVX_CONFIG_STEREOCAPABLE *pStCap =
                (NVX_CONFIG_STEREOCAPABLE *) pComponentConfigStructure;
            NvBool CamStCap;

            Err = cb->GetAttribute(cb, NvMMCameraAttribute_StereoCapable,
                                   sizeof(NvBool), &CamStCap);
            if (Err != NvSuccess)
                return OMX_ErrorBadParameter;
            pStCap->StereoCapable = (OMX_BOOL) CamStCap;
            break;
        }

        case NVX_IndexConfigSceneBrightness:
        {
            NVX_CONFIG_SCENEBRIGHTNESS *pSB =
                (NVX_CONFIG_SCENEBRIGHTNESS *) pComponentConfigStructure;
            NvF32 CamSB;
            NvBool CamFlashNeeded;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_SceneBrightness,
                                   sizeof(CamSB), &CamSB);
            if (Err != NvSuccess)
                return OMX_ErrorBadParameter;
            pSB->SceneBrightness = (NVX_F32) CamSB;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_FlashNeeded,
                                   sizeof(CamFlashNeeded), &CamFlashNeeded);
            if (Err != NvSuccess)
                return OMX_ErrorBadParameter;
            pSB->FlashNeeded = (OMX_BOOL) CamFlashNeeded;

            break;
        }

        case NVX_IndexConfigCILThreshold:
        {
            OMX_PARAM_U32TYPE *pU32 = (OMX_PARAM_U32TYPE*) pComponentConfigStructure;
            NvU8 * pSettleTime = (NvU8*) &pU32->nU32;
            Err = cb->GetAttribute(cb, NvCameraIspAttribute_CILThreshold,
                                   sizeof(pSettleTime), pSettleTime);
            if (Err != NvSuccess)
                return OMX_ErrorBadParameter;
            break;
        }

        case NVX_IndexConfigCustomizedBlockInfo:
        {
            NVX_CONFIG_CUSTOMIZEDBLOCKINFO *pCustomizedBlockInfo =
                (NVX_CONFIG_CUSTOMIZEDBLOCKINFO *) pComponentConfigStructure;
            NVX_CUSTOMINFO CustomInfo;

            NvOsMemset(&CustomInfo, 0, sizeof(CustomInfo));
            CustomInfo.SizeofData = pCustomizedBlockInfo->CustomInfo.SizeofData;
            CustomInfo.Data = (OMX_STRING)pCustomizedBlockInfo->CustomInfo.Data;
            Err = cb->GetAttribute(cb, NvMMCameraAttribute_CustomizedBlockInfo,
                                   sizeof(NVX_CUSTOMINFO), &CustomInfo);
            if (Err != NvSuccess)
                return OMX_ErrorBadParameter;

            pCustomizedBlockInfo->CustomInfo = CustomInfo;
            break;
        }

        case NVX_IndexConfigAdvancedNoiseReduction:
        {
            NVX_CONFIG_ADVANCED_NOISE_REDUCTION *pThing =
                (NVX_CONFIG_ADVANCED_NOISE_REDUCTION *) pComponentConfigStructure;
            NvF32 thresh[4];
            NvBool enable;
            NvError err = cb->GetAttribute(cb,
                                           NvMMCameraAttribute_ChromaFiltering,
                                           sizeof(enable), &enable);
            if (err == NvSuccess) {
                pThing->enable = (enable != NV_FALSE);
                err = cb->GetAttribute(cb, NvMMCameraAttribute_ANRFilterParams,
                                       sizeof(thresh), thresh);
            }
            if (err == NvSuccess) {
                NvOsMemcpy(pThing->lumaThreshold, &thresh[0],
                           sizeof(pThing->lumaThreshold));
                NvOsMemcpy(pThing->chromaThreshold, &thresh[2],
                           sizeof(pThing->chromaThreshold));

                err = cb->GetAttribute(cb,
                    NvMMCameraAttribute_ANRFilterLumaIsoThreshold,
                    sizeof(pThing->lumaIsoThreshold),
                    &pThing->lumaIsoThreshold);
            }
            if (err == NvSuccess) {
                err = cb->GetAttribute(cb,
                    NvMMCameraAttribute_ANRFilterChromaIsoThreshold,
                    sizeof(pThing->chromaIsoThreshold),
                    &pThing->chromaIsoThreshold);
            }

            if (err != NvSuccess)
                eError = OMX_ErrorBadParameter;
            break;
        }

        case NVX_IndexConfigNoiseReductionV1:
        {
            NVX_CONFIG_NOISE_REDUCTION_V1 *pThing =
                (NVX_CONFIG_NOISE_REDUCTION_V1 *) pComponentConfigStructure;
            NvError err = NvSuccess;
            err = cb->GetAttribute(cb,
                NvMMCameraAttribute_NrV1LumaEnable,
                sizeof(pThing->lumaEnable),
                &pThing->lumaEnable);
            if (err == NvSuccess)
            {
                err = cb->GetAttribute(cb,
                    NvMMCameraAttribute_NrV1LumaIsoThreshold,
                    sizeof(pThing->lumaIsoThreshold),
                    &pThing->lumaIsoThreshold);
            }
            if (err == NvSuccess)
            {
                err = cb->GetAttribute(cb,
                    NvMMCameraAttribute_NrV1LumaThreshold,
                    sizeof(pThing->lumaThreshold),
                    &pThing->lumaThreshold);
            }
            if (err == NvSuccess)
            {
                err = cb->GetAttribute(cb,
                    NvMMCameraAttribute_NrV1LumaSlope,
                    sizeof(pThing->lumaSlope),
                    &pThing->lumaSlope);
            }
            if (err == NvSuccess)
            {
                err = cb->GetAttribute(cb,
                    NvMMCameraAttribute_NrV1ChromaEnable,
                    sizeof(pThing->chromaEnable),
                    &pThing->chromaEnable);
            }
            if (err == NvSuccess)
            {
                err = cb->GetAttribute(cb,
                    NvMMCameraAttribute_NrV1ChromaIsoThreshold,
                    sizeof(pThing->chromaIsoThreshold),
                    &pThing->chromaIsoThreshold);
            }
            if (err == NvSuccess)
            {
                err = cb->GetAttribute(cb,
                    NvMMCameraAttribute_NrV1ChromaThreshold,
                    sizeof(pThing->chromaThreshold),
                    &pThing->chromaThreshold);
            }
            if (err == NvSuccess)
            {
                err = cb->GetAttribute(cb,
                    NvMMCameraAttribute_NrV1ChromaSlope,
                    sizeof(pThing->chromaSlope),
                    &pThing->chromaSlope);
            }
            if (err != NvSuccess)
                eError = OMX_ErrorBadParameter;
            break;
        }

        case NVX_IndexConfigNoiseReductionV2:
        {
            NVX_CONFIG_NOISE_REDUCTION_V2 *pThing =
                (NVX_CONFIG_NOISE_REDUCTION_V2 *) pComponentConfigStructure;
            NvError err = NvSuccess;
            err = cb->GetAttribute(cb,
                NvMMCameraAttribute_NrV2ChromaEnable,
                sizeof(pThing->chromaEnable),
                &pThing->chromaEnable);
            if (err == NvSuccess)
            {
                err = cb->GetAttribute(cb,
                    NvMMCameraAttribute_NrV2ChromaIsoThreshold,
                    sizeof(pThing->chromaIsoThreshold),
                    &pThing->chromaIsoThreshold);
            }
            if (err == NvSuccess)
            {
                err = cb->GetAttribute(cb,
                    NvMMCameraAttribute_NrV2ChromaThreshold,
                    sizeof(pThing->chromaThreshold),
                    &pThing->chromaThreshold);
            }
            if (err == NvSuccess)
            {
                err = cb->GetAttribute(cb,
                    NvMMCameraAttribute_NrV2ChromaSlope,
                    sizeof(pThing->chromaSlope),
                    &pThing->chromaSlope);
            }
            if (err == NvSuccess)
            {
                err = cb->GetAttribute(cb,
                    NvMMCameraAttribute_NrV2ChromaSpeed,
                    sizeof(pThing->chromaSpeed),
                    &pThing->chromaSpeed);
            }
            if (err == NvSuccess)
            {
                err = cb->GetAttribute(cb,
                    NvMMCameraAttribute_NrV2LumaConvolutionEnable,
                    sizeof(pThing->lumaConvolutionEnable),
                    &pThing->lumaConvolutionEnable);
            }
            if (err != NvSuccess)
                eError = OMX_ErrorBadParameter;
            break;
        }

        case NVX_IndexConfigBayerGains:
        {

            //There is no NvMM driver support for reading this attribute,
            //and no real justification to add it in, so leaving this stub
            //for now.
            return OMX_ErrorUnsupportedSetting;
        }

        case NVX_IndexConfigGainRange:
        {
            NvCameraIspRangeF32 range;
            NVX_CONFIG_GAINRANGE *pRetRange =
                (NVX_CONFIG_GAINRANGE *)pComponentConfigStructure;
            INIT_STRUCT(*pRetRange, pNvComp);

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_GainRange,
                                   sizeof(range), &range);
            if (Err != NvSuccess) {
                return OMX_ErrorUndefined;
            }

            pRetRange->nSize = sizeof(NVX_CONFIG_GAINRANGE);
            pRetRange->low = range.low;
            pRetRange->high = range.high;
            break;
        }

        case OMX_IndexConfigCommonRotate:
        {
            NvU32 OutputIndex = 0;
            OMX_CONFIG_ROTATIONTYPE *pRotation =
                (OMX_CONFIG_ROTATIONTYPE *)pComponentConfigStructure;

            OutputIndex = NvxCameraGetDzOutputIndex(pRotation->nPortIndex);
            pRotation->nRotation = pNvxCamera->Rotation[OutputIndex];
            break;
        }

        case OMX_IndexConfigCommonMirror:
        {
            NvU32 OutputIndex = 0;
            OMX_CONFIG_MIRRORTYPE *pMirror =
                (OMX_CONFIG_MIRRORTYPE *)pComponentConfigStructure;

            OutputIndex = NvxCameraGetDzOutputIndex(pMirror->nPortIndex);
            pMirror->eMirror = pNvxCamera->Mirror[OutputIndex];
            break;
        }

        case NVX_IndexConfigVideoFrameDataConversion:
            {
                NVX_CONFIG_VIDEOFRAME_CONVERSION *pConv =
                    (NVX_CONFIG_VIDEOFRAME_CONVERSION *)pComponentConfigStructure;
                OMX_BUFFERHEADERTYPE* pBuffer = NULL;
                NvMMBuffer *pPayload = NULL;

                pBuffer = (OMX_BUFFERHEADERTYPE *)(pConv->omxBuffer);
                if (pBuffer == NULL || !(pBuffer->nFlags & OMX_BUFFERFLAG_NV_BUFFER))
                {
                    return OMX_ErrorBadParameter;
                }
                // We have OMX buffer with NvMM buffer embedded:
                pPayload = (NvMMBuffer *)(void *)pBuffer->pBuffer;
                if (pPayload == NULL || pBuffer->nFilledLen < sizeof(NvMMBuffer))
                {
                    return OMX_ErrorBadParameter;
                }
                if (pPayload->PayloadType != NvMMPayloadType_SurfaceArray)
                {
                    return OMX_ErrorBadParameter;
                }
                eError = NvxCameraCopySurfaceForUser(
                             pPayload->Payload.Surfaces.Surfaces,
                             pConv->data,
                             &pConv->nDataSize,
                             pConv->userFormat);
            }
            break;

        case NVX_IndexConfigCapturePriority:
        {
            NvError err = NvSuccess;
            NVX_CONFIG_CAPTURE_PRIORITY *pCapturePriority =
                (NVX_CONFIG_CAPTURE_PRIORITY*) pComponentConfigStructure;
            NvBool Enable = NV_FALSE;

            err = cb->GetAttribute(cb, NvMMCameraAttribute_CapturePriority,
                    sizeof(Enable), &Enable);
            if (err != NvSuccess)
                eError = OMX_ErrorBadParameter;
            else
                pCapturePriority->Enable = Enable;

            break;
        }

        case NVX_IndexConfigFastBurstEn:
        {
            NvError err = NvSuccess;
            NVX_CONFIG_FASTBURSTEN *pFastBurst =
                (NVX_CONFIG_FASTBURSTEN*) pComponentConfigStructure;
            NvBool Enable = NV_FALSE;

            err = cb->GetAttribute(cb, NvMMCameraAttribute_FastBurstMode,
                    sizeof(Enable), &Enable);
            if (err != NvSuccess)
                eError = OMX_ErrorBadParameter;
            else
                pFastBurst->enable = Enable;

            break;
        }

        case NVX_IndexConfigManualTorchAmount:
        case NVX_IndexConfigManualFlashAmount:
        case NVX_IndexConfigAutoFlashAmount:
        {
            NvU32 AttributeType;
            NVX_CONFIG_FLASHTORCHAMOUNT *pFlashAmount =
                (NVX_CONFIG_FLASHTORCHAMOUNT *) pComponentConfigStructure;
            NvF32 Value;

            if (nIndex == NVX_IndexConfigManualTorchAmount)
            {
                AttributeType = NvMMCameraAttribute_ManualTorchAmount;
            }
            else if (nIndex == NVX_IndexConfigManualFlashAmount)
            {
                AttributeType = NvMMCameraAttribute_ManualFlashAmount;
            }
            else
            {
                AttributeType = NvMMCameraAttribute_AutoFlashAmount;
            }

            Err = cb->GetAttribute(cb, AttributeType, sizeof(Value), &Value);
            if (Err != NvSuccess)
            {
                return OMX_ErrorBadParameter;
            }

            pFlashAmount->value = (NVX_F32) Value;
            break;
        }

        case NVX_IndexConfigContinuousAutoFocus:
        {
            OMX_CONFIG_BOOLEANTYPE *pContinuousAutoFocus =
                (OMX_CONFIG_BOOLEANTYPE *) pComponentConfigStructure;
            NvBool Value;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_ContinuousAutoFocus, sizeof(Value), &Value);

            if (Err != NvSuccess)
            {
                return OMX_ErrorBadParameter;
            }
            pContinuousAutoFocus->bEnabled = (OMX_BOOL) Value;
            break;
        }

        case NVX_IndexConfigContinuousAutoFocusPause:
        {
            OMX_CONFIG_BOOLEANTYPE *pContinuousAutoFocusPause =
                (OMX_CONFIG_BOOLEANTYPE *) pComponentConfigStructure;
            NvBool Value;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_ContinuousAutoFocusPause, sizeof(Value), &Value);

            if (Err != NvSuccess)
            {
                return OMX_ErrorBadParameter;
            }
            pContinuousAutoFocusPause->bEnabled = (OMX_BOOL) Value;
            break;
        }

        case NVX_IndexConfigContinuousAutoFocusState:
        {
            NVX_CONFIG_CONTINUOUSAUTOFOCUS_STATE *pContinuousAutoFocusState =
                (NVX_CONFIG_CONTINUOUSAUTOFOCUS_STATE *) pComponentConfigStructure;
            NvU32 State;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_ContinuousAutoFocusState, sizeof(State), &State);

            if (Err != NvSuccess)
            {
                return OMX_ErrorBadParameter;
            }

            if (State == NVCAMERAISP_CAF_CONVERGED)
            {
                pContinuousAutoFocusState->bConverged = OMX_TRUE;
            }
            else
            {
                pContinuousAutoFocusState->bConverged = OMX_FALSE;
            }

            break;
        }

        case NVX_IndexConfigFrameCopyColorFormat:
        {
            NvError err = NvSuccess;
            NVX_CONFIG_FRAME_COPY_COLOR_FORMAT *pColorFormat =
                (NVX_CONFIG_FRAME_COPY_COLOR_FORMAT*) pComponentConfigStructure;
            NvMMDigitalZoomFrameCopyColorFormat ColorFormat;

            NvOsMemset(&ColorFormat, 0, sizeof(ColorFormat));

            err = zb->GetAttribute(zb,
                    NvMMDigitalZoomAttributeType_PreviewFrameCopyColorFormat,
                    sizeof(ColorFormat), &ColorFormat);
            if (err != NvSuccess)
            {
                eError = OMX_ErrorBadParameter;
                break;
            }
            else
            {
                pColorFormat->PreviewFrameCopy =
                    NvxCameraConvertNvColorToNvxImageColor(&ColorFormat);
            }
            err = zb->GetAttribute(zb,
                    NvMMDigitalZoomAttributeType_StillConfirmationFrameCopyColorFormat,
                    sizeof(ColorFormat), &ColorFormat);
            if (err != NvSuccess)
            {
                eError = OMX_ErrorBadParameter;
                break;
            }
            else
            {
                pColorFormat->StillConfirmationFrameCopy =
                    NvxCameraConvertNvColorToNvxImageColor(&ColorFormat);
            }
            err = zb->GetAttribute(zb,
                    NvMMDigitalZoomAttributeType_StillFrameCopyColorFormat,
                    sizeof(ColorFormat), &ColorFormat);
            if (err != NvSuccess)
            {
                eError = OMX_ErrorBadParameter;
                break;
            }
            else
            {
                pColorFormat->StillFrameCopy =
                    NvxCameraConvertNvColorToNvxImageColor(&ColorFormat);
            }
            break;
        }

        case NVX_IndexConfigFocusDistances:
        {
            NVX_CONFIG_FOCUSDISTANCES *focusDistances =
                (NVX_CONFIG_FOCUSDISTANCES *) pComponentConfigStructure;

            // Return these hard coded values right now
            focusDistances->NearDistance = 0.95f;
            focusDistances->OptimalDistance = 2.0f;
            focusDistances->FarDistance = -1.0f;

            break;
        }

        case NVX_IndexConfigNSLBuffers:
        {
            NVX_CONFIG_NSLBUFFERS *pNumNSLBuffers =
                (NVX_CONFIG_NSLBUFFERS *) pComponentConfigStructure;
            NvU32 numBuffers;
            NvError err;

            err = cb->GetAttribute(
                cb,
                NvMMCameraAttribute_NumNSLBuffers,
                sizeof(numBuffers),
                &numBuffers);
            if (err != NvSuccess)
            {
                eError = OMX_ErrorBadParameter;
                break;
            }
            pNumNSLBuffers->numBuffers = numBuffers;
            break;
        }

        case NVX_IndexConfigNSLSkipCount:
        {
            NVX_CONFIG_NSLSKIPCOUNT *pNSLSkipCount =
                (NVX_CONFIG_NSLSKIPCOUNT *) pComponentConfigStructure;
            NvU32 skipCount;
            NvError err;

            err = cb->GetAttribute(
                cb,
                NvMMCameraAttribute_NSLSkipCount,
                sizeof(skipCount),
                &skipCount);
            if (err != NvSuccess)
            {
                eError = OMX_ErrorBadParameter;
                break;
            }
            pNSLSkipCount->skipCount = skipCount;
            break;
        }

        case NVX_IndexConfigSensorIspSupport:
        {
            NVX_CONFIG_SENSORISPSUPPORT *pSensorIspSupport =
                (NVX_CONFIG_SENSORISPSUPPORT *) pComponentConfigStructure;
            NvBool IspSupport = NV_TRUE;
            Err = cb->GetAttribute(cb, NvMMCameraAttribute_SensorIspSupport,
                                   sizeof(IspSupport), &IspSupport);
            if (Err != NvSuccess)
                return OMX_ErrorBadParameter;

            pSensorIspSupport->bIspSupport = IspSupport;
            break;
        }

        case NVX_IndexConfigRemainingStillImages:
        {
            OMX_PARAM_U32TYPE *pRemainingStillImages = (OMX_PARAM_U32TYPE *)pComponentConfigStructure;
            Err = cb->GetAttribute(cb, NvMMCameraAttribute_RemainingStillImages,
                                   sizeof(pRemainingStillImages->nU32), &(pRemainingStillImages->nU32));
            if (Err != NvSuccess)
                return OMX_ErrorBadParameter;
            break;
        }

        case NVX_IndexConfigLowLightThreshold:
        {
            NvS32 LowLightThreshold;
            NVX_CONFIG_LOWLIGHTTHRESHOLD *pLowLightThreshold =
                    (NVX_CONFIG_LOWLIGHTTHRESHOLD *)pComponentConfigStructure;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_LowLightThreshold,
                                   sizeof(LowLightThreshold), &LowLightThreshold);
            if (Err != NvSuccess) {
                return OMX_ErrorUndefined;
            }

            pLowLightThreshold->nLowLightThreshold = LowLightThreshold;
            break;
        }

        case NVX_IndexConfigMacroModeThreshold:
        {
            NvS32 MacroModeThreshold;
            NVX_CONFIG_MACROMODETHRESHOLD *pMacroModeThreshold =
                    (NVX_CONFIG_MACROMODETHRESHOLD *)pComponentConfigStructure;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_MacroModeThreshold,
                                   sizeof(MacroModeThreshold), &MacroModeThreshold);
            if (Err != NvSuccess) {
                return OMX_ErrorUndefined;
            }

            pMacroModeThreshold->nMacroModeThreshold = MacroModeThreshold;
            break;
        }

        case NVX_IndexConfigFocusMoveMsg:
        {
            NvBool enable;
            OMX_CONFIG_BOOLEANTYPE *pMsgOn =
                (OMX_CONFIG_BOOLEANTYPE *) pComponentConfigStructure;

            Err = cb->GetAttribute(cb, NvCameraIspAttribute_FocusMoveMsg,
                                   sizeof(enable), &enable);
            if (Err != NvSuccess) {
                return OMX_ErrorUndefined;
            }
            pMsgOn->bEnabled = (OMX_BOOL)enable;
            break;
        }

        case NVX_IndexConfigFDLimit:
        {
            NvError err = NvSuccess;
            NVX_CONFIG_FDLIMIT *pFDParams =
                (NVX_CONFIG_FDLIMIT *)pComponentConfigStructure;

            err = cb->GetAttribute(cb,
                                   NvMMCameraAttribute_FDLimit,
                                   sizeof(NvS32),
                                   &(pFDParams->limit));

            if (err != NvSuccess)
                return OMX_ErrorUndefined;
            break;
        }

        case NVX_IndexConfigFDMaxLimit:
        {
            NvError err = NvSuccess;
            NVX_CONFIG_FDLIMIT *pFDParams =
                (NVX_CONFIG_FDLIMIT *)pComponentConfigStructure;

            err = cb->GetAttribute(cb,
                                   NvMMCameraAttribute_FDMaxLimit,
                                   sizeof(NvS32),
                                   &(pFDParams->limit));

            if (err != NvSuccess)
                return OMX_ErrorUndefined;
            break;
        }

        case NVX_IndexConfigCaptureMode:
        {
            NVX_CONFIG_CAPTUREMODE *pCaptureMode =
                (NVX_CONFIG_CAPTUREMODE*)pComponentConfigStructure;

            NvMMCameraSensorMode Mode;
            NvOsMemset(&Mode, 0, sizeof(Mode));
            Err = cb->GetAttribute(cb,
                                   NvMMCameraAttribute_CaptureSensorMode,
                                   sizeof(NvMMCameraSensorMode),
                                   &Mode);
            if (Err != NvSuccess)
                return OMX_ErrorBadParameter;
            else{
                pCaptureMode->mMode.width = Mode.Resolution.width;
                pCaptureMode->mMode.height = Mode.Resolution.height;
                pCaptureMode->mMode.FrameRate = Mode.FrameRate;
            }
            break;
        }

        case NVX_IndexConfigCameraMode:
        {
            NvError Err = NvSuccess;
            OMX_PARAM_U32TYPE *pCameraMode =
                (OMX_PARAM_U32TYPE *)pComponentConfigStructure;
            NvU32 cameraMode;

            // read from camera block only since dz block has to
            // have same op mode.
            Err = cb->GetAttribute(
                    cb,
                    NvMMCameraAttribute_CameraMode,
                    sizeof(NvU32), &cameraMode);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            pCameraMode->nU32 = cameraMode;
        }
        break;

        case NVX_IndexConfigForcePostView:
        {
            NvBool Enable;
            OMX_CONFIG_BOOLEANTYPE *ppv =
                (OMX_CONFIG_BOOLEANTYPE *) pComponentConfigStructure;

            Err = cb->GetAttribute(
                    cb,
                    NvMMCameraAttribute_ForcePostView,
                    sizeof(NvBool), &Enable);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;
            ppv->bEnabled = (OMX_BOOL) Enable;
        }
        break;

        default:
            eError = NvxComponentBaseGetConfig(pNvComp,
                                               nIndex, pComponentConfigStructure);
    }
    return eError;
}

static OMX_ERRORTYPE NvxCameraSetConfig(OMX_IN NvxComponent* pNvComp,
                                        OMX_IN OMX_INDEXTYPE nIndex,
                                        OMX_IN OMX_PTR pComponentConfigStructure)
{
    SNvxCameraData *pNvxCamera;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvMMBlockHandle cb, zb;

    NV_ASSERT(pNvComp && pComponentConfigStructure);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxCameraSetConfig\n"));
    pNvxCamera = (SNvxCameraData *)pNvComp->pComponentData;
    NV_ASSERT(pNvxCamera);

    switch (nIndex) {
        case NVX_IndexConfigProfile:
        {
            NVX_CONFIG_PROFILE *pProf = (NVX_CONFIG_PROFILE *)pComponentConfigStructure;

            NvxNvMMTransformSetProfile(&pNvxCamera->oSrcCamera, pProf);
            NvxNvMMTransformSetProfile(&pNvxCamera->oDigitalZoom, pProf);
            return eError;
        }

        case OMX_IndexParamNumAvailableStreams:
        {
            OMX_PARAM_U32TYPE *nas = (OMX_PARAM_U32TYPE*)pComponentConfigStructure;
            pNvxCamera->nCaptureCount = nas->nU32;
            return eError;
        }

        case OMX_IndexConfigCaptureMode:
        {
            OMX_CONFIG_CAPTUREMODETYPE *cmode =
                       (OMX_CONFIG_CAPTUREMODETYPE *)pComponentConfigStructure;
            // not supported on the new dedicated video capture port
            if (cmode->nPortIndex != (OMX_U32)s_nvx_port_still_capture)
            {
                return OMX_ErrorBadPortIndex;
            }
            if (cmode->bContinuous)
            {
                return OMX_ErrorBadParameter;
            }
            if (cmode->bFrameLimited)
            {
                pNvxCamera->nCaptureCount = cmode->nFrameLimit;
            }
            return OMX_ErrorNone;
        }

        case NVX_IndexConfigFocuserParameters:
        {
            // This is read-only config.
            eError = OMX_ErrorUndefined;
            return eError;
        }

        case NVX_IndexConfigFlashParameters:
        {
            // This is read-only config.
            eError = OMX_ErrorUndefined;
            return eError;
        }

        default:
            break;
    }

    eError = NvxCameraGetTransformData(pNvxCamera, pNvComp);
    if (eError != OMX_ErrorNone)
        return eError;

    cb = pNvxCamera->oSrcCamera.hBlock;
    zb = pNvxCamera->oDigitalZoom.hBlock;

    switch (nIndex)
    {
        case OMX_IndexConfigCommonRotate:
        {
            NvError Err;
            NvU32 OutputIndex = 0;
            OMX_CONFIG_ROTATIONTYPE *pRotation =
                (OMX_CONFIG_ROTATIONTYPE *)pComponentConfigStructure;

            OutputIndex = NvxCameraGetDzOutputIndex(pRotation->nPortIndex);
            eError = NvxCameraSetDzTransform(pNvxCamera, OutputIndex,
                                             &pRotation->nRotation, NULL);

            // if we have some exposure regions, reapply them after the transform
            Err = NvxCameraUpdateExposureRegions((void *)pNvxCamera);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            return eError;
        }
        case OMX_IndexConfigCommonMirror:
        {
            NvError Err;
            NvU32 OutputIndex = 0;
            OMX_CONFIG_MIRRORTYPE *pMirror =
                (OMX_CONFIG_MIRRORTYPE *)pComponentConfigStructure;
            OutputIndex = NvxCameraGetDzOutputIndex(pMirror->nPortIndex);

            eError = NvxCameraSetDzTransform(pNvxCamera, OutputIndex, NULL,
                &pMirror->eMirror);

            // if we have some exposure regions, reapply them after the transform
            Err = NvxCameraUpdateExposureRegions((void *)pNvxCamera);
            if (Err != NvSuccess)
            {
                return OMX_ErrorUndefined;
            }

            return eError;
        }
        case OMX_IndexConfigCommonOutputCrop:
        {
            OMX_CONFIG_RECTTYPE *pRect =
                (OMX_CONFIG_RECTTYPE*) pComponentConfigStructure;
            if (s_nvx_port_preview == pRect->nPortIndex)
            {
                NvOsMemcpy(
                    &pNvxCamera->oCropPreview,
                    pComponentConfigStructure,
                    sizeof(OMX_CONFIG_RECTTYPE));
            }
            else if (s_nvx_port_still_capture == pRect->nPortIndex)
            {
                NvOsMemcpy(
                    &pNvxCamera->oCropStillCapture,
                    pComponentConfigStructure,
                    sizeof(OMX_CONFIG_RECTTYPE));
            }
            else if (s_nvx_port_video_capture == pRect->nPortIndex)
            {
                NvOsMemcpy(
                    &pNvxCamera->oCropVideoCapture,
                    pComponentConfigStructure,
                    sizeof(OMX_CONFIG_RECTTYPE));
            }
            else if (s_nvx_port_thumbnail == pRect->nPortIndex)
            {
                NvOsMemcpy(
                    &pNvxCamera->oCropThumbnail,
                    pComponentConfigStructure,
                    sizeof(OMX_CONFIG_RECTTYPE));
            }
            else
            {
                return OMX_ErrorBadPortIndex;
            }

            s_SetCrop(pNvxCamera, (int)pRect->nPortIndex);
            return eError;
        }

        case NVX_IndexConfigSmoothZoomTime:
        {
            OMX_PARAM_U32TYPE *nsztms = (OMX_PARAM_U32TYPE*)pComponentConfigStructure;

            zb->SetAttribute(zb, NvMMDigitalZoomAttributeType_SmoothZoomTime,
                             0, sizeof(NvU32), (NvU32 *)&nsztms->nU32);
            return eError;
        }
        case NVX_IndexConfigStillPassthrough:
        {
            NVX_CONFIG_STILLPASSTHROUGH *pPassThrough =
                (NVX_CONFIG_STILLPASSTHROUGH *) pComponentConfigStructure;
            NvU32 enable = 0;

            if (pPassThrough->enablePassThrough)
                enable |= NVMMCAMERA_ENABLE_STILL_PASSTHROUGH;

            zb->SetAttribute(zb, NvMMDigitalZoomAttributeType_StillPassthrough,
                             0, sizeof(NvU32), &enable);

            NV_DEBUG_PRINTF(("NvMMCameraAttribute_StillPassthrough with enable = %d\n",enable));
            cb->SetAttribute(cb, NvMMCameraAttribute_StillPassthrough,
                             0, sizeof(NvU32), &enable);
            return eError;
        }
        case NVX_IndexConfigZoomAbort:
        {
            NvU32 data = 1;
            zb->SetAttribute(zb, NvMMDigitalZoomAttributeType_SmoothZoomAbort,
                             0, sizeof(NvU32), &data);
            return eError;
        }

        case OMX_IndexConfigCommonDigitalZoom:
        {
            OMX_CONFIG_SCALEFACTORTYPE *sf =
                (OMX_CONFIG_SCALEFACTORTYPE *) pComponentConfigStructure;

            if (sf->xWidth != sf->xHeight)
                return OMX_ErrorBadParameter;

            zb->SetAttribute(zb, NvMMDigitalZoomAttributeType_ZoomFactor,
                             0, sizeof(NvSFx), &sf->xWidth);
            break;
        }
        case NVX_IndexConfigDZScaleFactorMultiplier:
        {
            NVX_CONFIG_DZSCALEFACTORMULTIPLIER *sfM =
                (NVX_CONFIG_DZSCALEFACTORMULTIPLIER *) pComponentConfigStructure;
            zb->SetAttribute(zb, NvMMDigitalZoomAttributeType_ZoomFactorMultiplier,
                             0, sizeof(NvSFx), &sfM->ZoomFactorMultiplier);
            break;
        }
        case NVX_IndexConfigExposureTime:
        {
            NvCameraIspExposureTime et;
            NVX_CONFIG_EXPOSURETIME *pet =
                (NVX_CONFIG_EXPOSURETIME *) pComponentConfigStructure;

            et.isAuto = pet->bAutoShutterSpeed;
            et.value = pet->nExposureTime;

            cb->SetAttribute(cb, NvCameraIspAttribute_ExposureTime, 0,
                             sizeof(et), &et);
            break;
        }
        case OMX_IndexConfigCommonExposureValue: {
            NvSFx data;
            NvCameraIspISO iso;
            NvCameraIspExposureTime et;
            OMX_CONFIG_EXPOSUREVALUETYPE *ev =
                (OMX_CONFIG_EXPOSUREVALUETYPE *) pComponentConfigStructure;

            et.isAuto = ev->bAutoShutterSpeed;
            et.value = ev->nShutterSpeedMsec / 1000.0f;
            cb->SetAttribute(cb, NvCameraIspAttribute_ExposureTime, 0,
                             sizeof(et), &et);

            iso.isAuto = ev->bAutoSensitivity;
            iso.value = ev->nSensitivity;
            cb->SetAttribute(cb, NvCameraIspAttribute_EffectiveISO, 0,
                             sizeof(iso), &iso);


            cb->SetAttribute(cb, NvCameraIspAttribute_ExposureCompensation, 0,
                             sizeof(NvSFx), &(ev->xEVCompensation));

            switch (ev->eMetering)
            {
            case OMX_MeteringModeSpot:
                data = NVCAMERAISP_METERING_SPOT;
                break;
            case OMX_MeteringModeMatrix:
                // omx header defines as "Matrix or evaluative"
                data = NVCAMERAISP_METERING_MATRIX;
                break;
            case OMX_MeteringModeAverage:
            default:
                data = NVCAMERAISP_METERING_CENTER;
                break;
            }
            cb->SetAttribute(cb, NvCameraIspAttribute_MeteringMode, 0,
                             sizeof(NvS32), &data);
            break;
        }
        case NVX_IndexConfigCILThreshold:
        {
            OMX_PARAM_U32TYPE *pU32 = (OMX_PARAM_U32TYPE*) pComponentConfigStructure;
            NvU8 * pSettleTime = (NvU8*) &pU32->nU32;
            cb->SetAttribute(cb, NvCameraIspAttribute_CILThreshold, 0,
                             sizeof(pSettleTime), pSettleTime);
            break;
        }
        case OMX_IndexConfigCommonExposure:
        {
            NvBool data;
            OMX_CONFIG_EXPOSURECONTROLTYPE * ec =
                (OMX_CONFIG_EXPOSURECONTROLTYPE *) pComponentConfigStructure;

            data = ec->eExposureControl == OMX_ExposureControlAuto;
            cb->SetAttribute(cb, NvCameraIspAttribute_ContinuousAutoExposure,
                             0, sizeof(data), &data);
            break;
        }

        case NVX_IndexConfigAutoFrameRate:
        {
            NVX_CONFIG_AUTOFRAMERATE *afr =
                (NVX_CONFIG_AUTOFRAMERATE *) pComponentConfigStructure;
            NvBool data = afr->bEnabled ? NV_TRUE : NV_FALSE;

            cb->SetAttribute(cb, NvCameraIspAttribute_AutoFrameRate,
                             0, sizeof(data), &data);

            pNvxCamera->oAutoFrameRate.bEnabled = afr->bEnabled;
            pNvxCamera->oAutoFrameRate.low = afr->low;
            pNvxCamera->oAutoFrameRate.high = afr->high;
            if (OMX_TRUE == afr->bEnabled) {
                NvCameraIspRange oRange;
                oRange.low = afr->low;
                oRange.high = afr->high;
                cb->SetAttribute(cb, NvCameraIspAttribute_AutoFrameRateRange,
                                 0, sizeof(oRange), &oRange);
            }
            break;
        }

        case NVX_IndexConfigExposureTimeRange:
        {
            NVX_CONFIG_EXPOSURETIME_RANGE *omxRange =
                (NVX_CONFIG_EXPOSURETIME_RANGE *) pComponentConfigStructure;
            NvCameraIspRange oRange;
            NvError err = NvSuccess;

            // We ignore Camera port specified - for now
            oRange.low = NV_SFX_WHOLE_TO_FX(omxRange->low);
            oRange.high = NV_SFX_WHOLE_TO_FX(omxRange->high);

            err = cb->SetAttribute(cb, NvCameraIspAttribute_ExposureTimeRange,
                                   0, sizeof(oRange), &oRange);

            if (err != NvSuccess)
                eError = OMX_ErrorBadParameter;

            break;
        }

        case NVX_IndexConfigISOSensitivityRange:
        {
            NVX_CONFIG_ISOSENSITIVITY_RANGE *omxRange =
                (NVX_CONFIG_ISOSENSITIVITY_RANGE *) pComponentConfigStructure;
            NvCameraIspRange oRange;
            NvError err = NvSuccess;

            // We ignore Camera port specified - for now
            oRange.low = NV_SFX_WHOLE_TO_FX(omxRange->low);
            oRange.high = NV_SFX_WHOLE_TO_FX(omxRange->high);

            err = cb->SetAttribute(cb, NvCameraIspAttribute_ISORange,
                                   0, sizeof(oRange), &oRange);
            if (err != NvSuccess)
                eError = OMX_ErrorBadParameter;

            break;
        }

        case NVX_IndexConfigWhitebalanceCCTRange:
        {
            NVX_CONFIG_CCTWHITEBALANCE_RANGE *omxRange =
                (NVX_CONFIG_CCTWHITEBALANCE_RANGE *) pComponentConfigStructure;
            NvCameraIspRange oRange;
            NvError err = NvSuccess;

            // We ignore Camera port specified - for now
            oRange.low = omxRange->low;
            oRange.high = omxRange->high;

            err = cb->SetAttribute(cb, NvCameraIspAttribute_WhiteBalanceCCTRange,
                                   0, sizeof(oRange), &oRange);
            if (err != NvSuccess)
                eError = OMX_ErrorBadParameter;

            break;
        }

        case NVX_IndexConfigPreviewEnable:
        {
            OMX_CONFIG_BOOLEANTYPE *pe =
                (OMX_CONFIG_BOOLEANTYPE *) pComponentConfigStructure;

            NvBool bEnablePreviewOut;

            bEnablePreviewOut = NV_TRUE;
            if (pe->bEnabled)
            {
                cb->Extension(cb, NvMMCameraExtension_EnablePreviewOut,
                              sizeof(NvBool), &bEnablePreviewOut, 0, NULL);
            }

            CameraPreviewEnableComplex(pNvxCamera, pe->bEnabled, 0, 0);
            pNvxCamera->PreviewEnabled = pe->bEnabled;

            break;
        }

        case NVX_IndexConfigAlgorithmWarmUp:
        {
            NvError Err = NvSuccess;
            ENvxCameraStereoCameraMode StCamMode = NvxLeftOnly;
            OMX_CONFIG_BOOLEANTYPE *pe =
                (OMX_CONFIG_BOOLEANTYPE *) pComponentConfigStructure;

            Err = cb->GetAttribute(cb, NvMMCameraAttribute_StereoCameraMode,
                sizeof(ENvxCameraStereoCameraMode), &StCamMode);
            if (Err != NvSuccess)
                NvOsDebugPrintf("failed to get stereomode [0x%x]\n", Err);

            /* Ignore algorithm warm up for stereo. It hangs the NvRmStreamFlush
             * for not so obvious reasons. Investigation pending ... */
            if (((pe->bEnabled) && (!pNvxCamera->bUseUSBCamera)) &&
                (StCamMode != NvxStereo))
            {
                NvBool bEnablePreviewOut;

                pNvxCamera->AlgorithmWarmedUp = pe->bEnabled;
                bEnablePreviewOut = NV_FALSE;

                cb->Extension(cb, NvMMCameraExtension_EnablePreviewOut,
                              sizeof(NvBool), &bEnablePreviewOut, 0, NULL);
                CameraPreviewEnableComplex(pNvxCamera, pe->bEnabled, 0, 0);
            }

            break;
        }

        case NVX_IndexConfigCapturePause:
        {
            // assume that the client wants to pause the video stream, since
            // that's the only thing that makes sense for this.  ideally, we
            // would throw an error if the port was wrong, but the OMX bool
            // type doesn't have a port value.
            OMX_CONFIG_BOOLEANTYPE *cc = (OMX_CONFIG_BOOLEANTYPE *)pComponentConfigStructure;

            NvOsMemcpy(
                &(pNvxCamera->bCapturing),
                pComponentConfigStructure,
                sizeof(OMX_CONFIG_BOOLEANTYPE));

            if (cc->bEnabled)
            {
                CameraVideoCaptureComplex(pNvxCamera, NV_FALSE, 0, 0, NV_TRUE);
            }
            else
            {
                CameraVideoCaptureComplex(pNvxCamera, NV_TRUE, 0, 0, NV_TRUE);
            }
            break;
        }

        case OMX_IndexConfigCapturing:
        {
            OMX_CONFIG_BOOLEANTYPE *cc = (OMX_CONFIG_BOOLEANTYPE *)pComponentConfigStructure;

            NvOsMemcpy(&(pNvxCamera->bCapturing), pComponentConfigStructure,
                       sizeof(OMX_CONFIG_BOOLEANTYPE));

            // this config isn't versatile enough to handle our new arch
            // with dedicated ports for still/video capture, since it's
            // only a bool.  so we'll issue a still request, in an attempt
            // to leave the old still capture path mostly backwards-compatible
            if (cc->bEnabled)
            {
                CameraTakePictureComplex(
                    pNvxCamera,
                    0,
                    0,
                    0,
                    pNvxCamera->nCaptureCount,
                    0,
                    pNvxCamera->nBurstInterval);
            }
            else
            {
                CameraTakePictureComplex(pNvxCamera, 0, 0, 0, 0, 0, 0);
            }
            break;
        }
        case OMX_IndexAutoPauseAfterCapture: //OMX_IndexConfigAutoPauseAfterCapture:
        {
            NvError err = NvSuccess;
            OMX_CONFIG_BOOLEANTYPE *pe =
                    (OMX_CONFIG_BOOLEANTYPE *) pComponentConfigStructure;
            NvBool enable = (NvBool)pe->bEnabled;

            err = cb->SetAttribute(cb,
                                   NvMMCameraAttribute_PausePreviewAfterStillCapture,
                                   0, sizeof(enable), &enable);
            if (err != NvSuccess)
               eError = OMX_ErrorBadParameter;

            NvOsMemcpy(&(pNvxCamera->bAutoPauseAfterCapture),
                       pComponentConfigStructure,
                       sizeof(OMX_CONFIG_BOOLEANTYPE));
            break;
        }

        case OMX_IndexConfigCommonWhiteBalance:
        {
            OMX_CONFIG_WHITEBALCONTROLTYPE *wb =
                (OMX_CONFIG_WHITEBALCONTROLTYPE *) pComponentConfigStructure;
            s_SetWhiteBalance(pNvxCamera, wb->eWhiteBalControl);
            break;
        }

        case OMX_IndexConfigFocusControl:
        {
            NvError err = NvSuccess;
            NvCameraIspFocusControl control;
            OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE *fc =
                (OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE *) pComponentConfigStructure;

            control = fc->eFocusControl;

            err = cb->SetAttribute(cb, NvCameraIspAttribute_AutoFocusControl, 0,
                                   sizeof(NvCameraIspFocusControl), &control);
            if (err != NvSuccess)
            {
                eError = OMX_ErrorBadParameter;
            }
            else if ((control == OMX_IMAGE_FocusControlOn) ||
                (control == OMX_IMAGE_FocusControlOff))
            {
                NvS32 value = fc->nFocusStepIndex;
                err = cb->SetAttribute(cb, NvCameraIspAttribute_FocusPosition,
                                       0, sizeof(value), &value);
                if (err != NvSuccess)
                    eError = OMX_ErrorBadParameter;
            }
            break;
        }

        case NVX_IndexConfigFocuserInfo:
        {
            NvCameraIspAfConfig afConfig;
            NvError err = NvSuccess;
            NVX_CONFIG_FOCUSER_INFO *pInfo =
                (NVX_CONFIG_FOCUSER_INFO *)pComponentConfigStructure;
            INIT_STRUCT(*pInfo,pNvComp);

            NvOsMemset(&afConfig, 0, sizeof(afConfig));

            afConfig.slewRate = pInfo->slewRate;
            afConfig.settleTime = pInfo->settleTime;
            afConfig.positionHyperFocal = pInfo->positionHyperFocal;
            afConfig.positionResting = pInfo->positionResting;
            afConfig.positionInf = pInfo->positionInf;
            afConfig.positionInfOffset = pInfo->positionInfOffset;
            afConfig.positionMacro = pInfo->positionMacro;
            afConfig.positionMacroOffset = pInfo->positionMacroOffset;
            afConfig.infMin = pInfo->infMin;
            afConfig.macroMax = pInfo->macroMax;
            afConfig.infInitSearch = pInfo->infInitSearch;
            afConfig.macroInitSearch = pInfo->macroInitSearch;

            /** Read only members
             * afConfig.positionPhysicalLow
             * afConfig.positionPhysicalHigh
             * afConfig.rangeEndsReversed
             * afConfig.hysteresis
             */

            err = cb->SetAttribute(cb, NvCameraIspAttribute_AfConfig,
                                   0, sizeof(afConfig), &afConfig);
            if (err != NvSuccess)
                eError = OMX_ErrorBadParameter;

            break;
        }
        case NVX_IndexConfigFocusPosition:
        {
            NvError err = NvSuccess;
            OMX_PARAM_U32TYPE *pFocusPosition = (OMX_PARAM_U32TYPE *)pComponentConfigStructure;
            err = cb->SetAttribute(cb, NvCameraIspAttribute_FocusPosition,
                    0, sizeof(pFocusPosition->nU32), &(pFocusPosition->nU32));
            if (err != NvSuccess)
                eError = OMX_ErrorBadParameter;
            break;
        }

        case NVX_IndexConfigAutofocusSpeed:
        {
            NVX_CONFIG_AUTOFOCUS_SPEED *afSpeed =
                (NVX_CONFIG_AUTOFOCUS_SPEED *) pComponentConfigStructure;
            NvU32 val = afSpeed->bFast ? 0 : 1;
            cb->SetAttribute(cb, NvMMCameraAttribute_AFSweepType,
                             0, sizeof(val), &val);
            break;
        }

        case OMX_IndexConfigFlashControl:
        {
            OMX_IMAGE_PARAM_FLASHCONTROLTYPE *fc =
                (OMX_IMAGE_PARAM_FLASHCONTROLTYPE *) pComponentConfigStructure;
            eError = NvxCameraSetFlashMode(pNvxCamera,fc);
            break;
        }

        case OMX_IndexConfigCommonImageFilter:
        {
            OMX_CONFIG_IMAGEFILTERTYPE *ift =
                (OMX_CONFIG_IMAGEFILTERTYPE *) pComponentConfigStructure;
            s_SetStylizeMode(pNvxCamera, ift->eImageFilter);
            break;
        }

        case NVX_IndexConfigColorCorrectionMatrix:
        {
            NVX_CONFIG_COLORCORRECTIONMATRIX *matrix =
                (NVX_CONFIG_COLORCORRECTIONMATRIX *) pComponentConfigStructure;
            pNvxCamera->ccm.m00 = NV_SFX_FP_TO_FX(matrix->ccMatrix[0]);
            pNvxCamera->ccm.m01 = NV_SFX_FP_TO_FX(matrix->ccMatrix[1]);
            pNvxCamera->ccm.m02 = NV_SFX_FP_TO_FX(matrix->ccMatrix[2]);
            pNvxCamera->ccm.m03 = NV_SFX_FP_TO_FX(matrix->ccMatrix[3]);
            pNvxCamera->ccm.m10 = NV_SFX_FP_TO_FX(matrix->ccMatrix[4]);
            pNvxCamera->ccm.m11 = NV_SFX_FP_TO_FX(matrix->ccMatrix[5]);
            pNvxCamera->ccm.m12 = NV_SFX_FP_TO_FX(matrix->ccMatrix[6]);
            pNvxCamera->ccm.m13 = NV_SFX_FP_TO_FX(matrix->ccMatrix[7]);
            pNvxCamera->ccm.m20 = NV_SFX_FP_TO_FX(matrix->ccMatrix[8]);
            pNvxCamera->ccm.m21 = NV_SFX_FP_TO_FX(matrix->ccMatrix[9]);
            pNvxCamera->ccm.m22 = NV_SFX_FP_TO_FX(matrix->ccMatrix[10]);
            pNvxCamera->ccm.m23 = NV_SFX_FP_TO_FX(matrix->ccMatrix[11]);
            pNvxCamera->ccm.m30 = NV_SFX_FP_TO_FX(matrix->ccMatrix[12]);
            pNvxCamera->ccm.m31 = NV_SFX_FP_TO_FX(matrix->ccMatrix[13]);
            pNvxCamera->ccm.m32 = NV_SFX_FP_TO_FX(matrix->ccMatrix[14]);
            pNvxCamera->ccm.m33 = NV_SFX_FP_TO_FX(matrix->ccMatrix[15]);
            s_SetStylizeMode(pNvxCamera, NVX_ImageFilterManual);
            break;
        }

        case OMX_IndexConfigCommonContrast:
        {
            OMX_CONFIG_CONTRASTTYPE *pct =
                (OMX_CONFIG_CONTRASTTYPE *) pComponentConfigStructure;
            NvF32 data = (NvF32)(pct->nContrast / 100.0);
            cb->SetAttribute(cb, NvCameraIspAttribute_ContrastEnhancement,
                             0, sizeof(NvF32), &data);
            break;
        }

        case OMX_IndexConfigCommonBrightness:
        {
            OMX_CONFIG_BRIGHTNESSTYPE *pl =
                (OMX_CONFIG_BRIGHTNESSTYPE *) pComponentConfigStructure;
            // Map [0,100] to [0,3]
            NvF32 data = (NvF32)(pl->nBrightness * 2.0f + 1.0f) / 67.0f;

            cb->SetAttribute(cb, NvCameraIspAttribute_Brightness,
                             0, sizeof(NvF32), &data);
            break;
        }

        case OMX_IndexConfigCommonSaturation:
        {
            OMX_CONFIG_SATURATIONTYPE *ps =
                (OMX_CONFIG_SATURATIONTYPE *) pComponentConfigStructure;
            NvF32 data = (NvF32)(ps->nSaturation / 100.0) + 1;

            cb->SetAttribute(cb, NvCameraIspAttribute_Saturation,
                             0, sizeof(NvF32), &data);
            break;
        }

        case NVX_IndexConfigHue:
        {
            NVX_CONFIG_HUETYPE *ph =
                (NVX_CONFIG_HUETYPE *) pComponentConfigStructure;
            NvF32 data = (NvF32)(ph->nHue);
            cb->SetAttribute(cb, NvCameraIspAttribute_Hue,
                                 0, sizeof(NvF32), &data);
            break;
        }

        case NVX_IndexConfigDeviceRotate:
        {
            OMX_CONFIG_ROTATIONTYPE *pRotation =
                (OMX_CONFIG_ROTATIONTYPE *)pComponentConfigStructure;

            // update device orientation
            cb->SetAttribute(cb,
                             NvMMCameraAttribute_CommonRotation,
                             0,
                             sizeof(NvS32),
                             &pRotation->nRotation);
            break;
        }

        case NVX_IndexConfigConvergeAndLock:
        {
            NVX_CONFIG_CONVERGEANDLOCK *pcal =
                (NVX_CONFIG_CONVERGEANDLOCK *) pComponentConfigStructure;
            NvMMCameraAlgorithmFlags algs;
            NvU32 algSubType = NvMMCameraAlgorithmSubType_None;
            algs = pcal->bAutoFocus ? NvMMCameraAlgorithmFlags_AutoFocus : 0;
            algs |= pcal->bAutoExposure ? NvMMCameraAlgorithmFlags_AutoExposure : 0;
            algs |= pcal->bAutoWhiteBalance ? NvMMCameraAlgorithmFlags_AutoWhiteBalance : 0;

            if(pcal->nSize >= sizeof(NVX_CONFIG_CONVERGEANDLOCK))
            {
                //
                // Make sure that only one of the bits is set.
                // If so, we set it to AF Full range to accomodate nvcs.
                //
                if(! (pcal->algSubType == NvxAlgSubType_None ||
                     (((((NvU32) pcal->algSubType) &
                      (((NvU32) pcal->algSubType) -1)) == 0))))
                {
                    algSubType = NvMMCameraAlgorithmSubType_AFFullRange;
                }
                else if(pcal->algSubType & NvxAlgSubType_AFFullRange)
                    algSubType = NvMMCameraAlgorithmSubType_AFFullRange;
                else if(pcal->algSubType & NvxAlgSubType_AFInfMode)
                    algSubType = NvMMCameraAlgorithmSubType_AFInfMode;
                else if(pcal->algSubType & NvxAlgSubType_AFMacroMode)
                    algSubType = NvMMCameraAlgorithmSubType_AFMacroMode;
                else
                    algSubType = NvMMCameraAlgorithmSubType_None;
            }

            if (pcal->bUnlock) {
                cb->Extension(cb, NvMMCameraExtension_UnlockAlgorithms,
                              sizeof(algs), &algs, 0, NULL);
            } else {
                NvMMCameraConvergeAndLockParamaters parameters;

                // if locking AF, focus regions have to be set first
                if (pcal->bAutoFocus && pNvxCamera->FocusRegions.nRegions > 0)
                {
                    NvError Err = NvSuccess;
                    NvCameraIspFocusingRegions FocusRegions;
                    NvMMDigitalZoomOperationInformation DZInfo;

                    // get the preview information in order to compute the correct
                    // focus region w.r.t camera's preview buffer
                    Err = zb->GetAttribute(zb,
                        NvMMDigitalZoomAttributeType_OperationInformation,
                        sizeof(DZInfo), &DZInfo);

                    if (Err != NvSuccess)
                        return OMX_ErrorUndefined;

                    FocusRegions.numOfRegions = pNvxCamera->FocusRegions.nRegions;
                    NvxCameraMappingPreviewOutputRegionToInputRegion(
                        FocusRegions.regions,
                        &DZInfo,
                        pNvxCamera->FocusRegions.regions,
                        FocusRegions.numOfRegions);

                    Err = cb->SetAttribute(cb, NvCameraIspAttribute_AutoFocusRegions,
                                           0, sizeof(FocusRegions), &FocusRegions);
                    if (Err != NvSuccess)
                        return OMX_ErrorUndefined;
                }

                // if locking AE, exposure regions have to be set first
                if (pcal->bAutoExposure)
                {
                    NvError Err = NvSuccess;
                    Err = NvxCameraUpdateExposureRegions((void *)pNvxCamera);
                    if (Err != NvSuccess)
                    {
                        return OMX_ErrorUndefined;
                    }
                }

                if(pcal->algSubType & NvxAlgSubType_TorchDisable)
                {
                    algSubType |= NvMMCameraAlgorithmSubType_TorchDisable;
                }

                parameters.algs = algs;
                parameters.milliseconds = pcal->nTimeOutMS;
                parameters.relock = (NvBool)pcal->bRelock;
                parameters.algSubType = algSubType;
                cb->Extension(cb, NvMMCameraExtension_ConvergeAndLockAlgorithms,
                              sizeof(parameters), &parameters, 0, NULL);
            }
            NvOsMemcpy(&(pNvxCamera->oConvergeAndLock), pComponentConfigStructure,
                       sizeof(NVX_CONFIG_CONVERGEANDLOCK));
            break;
        }

        case OMX_IndexConfigCommonFrameStabilisation:
        {
            OMX_CONFIG_FRAMESTABTYPE *pfs =
                (OMX_CONFIG_FRAMESTABTYPE *) pComponentConfigStructure;
            NvCameraIspStabilizationMode sMode = NvCameraIspStabilizationMode_None;

            if (pfs->bStab)
            {
                // take our best guess at what to enable based on the port
                // that it is enabled on.  the undefined case here is where
                // both still and video are simultaneously enabled.  in that
                // case, we should make an educated runtime decision to assume
                // still until video capture is *started*, at which point we
                // should change over to video stabilization.  that is up to
                // the camera block to figure out, since its state will keep
                // track of what is on/off.
                if (pfs->nPortIndex == s_nvx_port_still_capture)
                {
                    return OMX_ErrorNotImplemented;
                }
                else if (pfs->nPortIndex == s_nvx_port_video_capture)
                {
                    sMode = NvCameraIspStabilizationMode_Video;
                }
                else
                {
                    return OMX_ErrorBadPortIndex;
                }
            }
            else
            {
                sMode = NvCameraIspStabilizationMode_None;
            }

            cb->SetAttribute(
                cb,
                NvCameraIspAttribute_Stabilization,
                0,
                sizeof(NvS32),
                &sMode);
            pNvxCamera->bStab = pfs->bStab;
            break;
        }

        case NVX_IndexConfigCameraRawCapture:
        {
            NVX_CONFIG_CAMERARAWCAPTURE *prc =
                (NVX_CONFIG_CAMERARAWCAPTURE *) pComponentConfigStructure;
            NvMMCameraCaptureRawRequest RawRequest;
            NvOsMemset(&RawRequest, 0, sizeof(NvMMCameraCaptureRawRequest));

            //TODO: Bug #677895
            //the crop information we need is in DZ as we are still formulating
            //a plan for handling crop data on the nvmm_camera capture pin.
            //when this is resolved, we should remove these GetAttribute calls
            //and add SetAttribute calls into the Set_Crop function to tune
            //the camera block attributes.
            zb->GetAttribute(zb, NvMMDigitalZoomAttributeType_StillCrop,
                             sizeof(NvBool), &RawRequest.Crop);
            zb->GetAttribute(zb, NvMMDigitalZoomAttributeType_StillCropRect,
                             sizeof(NvRect), &RawRequest.CropRect);

            RawRequest.CaptureCount = prc->nCaptureCount;
            NvOsStrncpy(RawRequest.Filename, prc->Filename, NVMMCAMERA_MAX_FILENAME_LEN);
            cb->Extension(cb, NvMMCameraExtension_CaptureRaw,
                          sizeof(RawRequest), &RawRequest, 0, NULL);
            break;
        }

        case NVX_IndexConfigConcurrentRawDumpFlag:
        {
            NvBool enable;
            OMX_PARAM_U32TYPE *pRawDumpFlag = (OMX_PARAM_U32TYPE *)pComponentConfigStructure;
            pNvxCamera->rawDumpFlag = pRawDumpFlag->nU32;
            enable = (pNvxCamera->rawDumpFlag & 1) ? NV_TRUE : NV_FALSE;
            cb->SetAttribute(cb, NvMMCameraAttribute_DumpRaw, 0, sizeof(enable), &enable);
            enable = (pNvxCamera->rawDumpFlag & 2) ? NV_TRUE : NV_FALSE;
            cb->SetAttribute(cb, NvMMCameraAttribute_DumpRawFile, 0, sizeof(enable), &enable);
            enable = (pNvxCamera->rawDumpFlag & 4) ? NV_TRUE : NV_FALSE;
            cb->SetAttribute(cb, NvMMCameraAttribute_DumpRawHeader, 0, sizeof(enable), &enable);
            break;
        }

        case NVX_IndexConfigFlicker:
        {
            NVX_CONFIG_FLICKER *pf =
                (NVX_CONFIG_FLICKER *) pComponentConfigStructure;
            NvU32 value;
            NvBool enable;

            enable = NvxFlicker_Off != pf->eFlicker;
            cb->SetAttribute(cb, NvCameraIspAttribute_AntiFlicker,
                             0, sizeof(enable), &enable);
            if (NvxFlicker_Off != pf->eFlicker) {
                value = NVCAMERAISP_FLICKER_AUTO;
                if (NvxFlicker_50HZ == pf->eFlicker)
                    value = NVCAMERAISP_FLICKER_50HZ;
                else if (NvxFlicker_60HZ == pf->eFlicker)
                    value = NVCAMERAISP_FLICKER_60HZ;

                cb->SetAttribute(cb, NvCameraIspAttribute_FlickerFrequency,
                                 0, sizeof(value), &value);
            }
            break;
        }

        case NVX_IndexConfigPreCaptureConverge:
        {
            NvOsMemcpy(&(pNvxCamera->oPrecaptureConverge), pComponentConfigStructure,
                       sizeof(NVX_CONFIG_PRECAPTURECONVERGE));
            break;
        }

        case NVX_IndexConfigExposureRegionsRect:
        {
            NvError Err;
            NVX_CONFIG_ExposureRegionsRect *pExpRegRect =
                (NVX_CONFIG_ExposureRegionsRect *) pComponentConfigStructure;
            NVX_CONFIG_ExposureRegionsRect backupExpRegRect;

            // save the old state in case we reject this region setting
            NvOsMemcpy(
                &backupExpRegRect,
                &pNvxCamera->ExposureRegions,
                sizeof(NVX_CONFIG_ExposureRegionsRect));

            NvOsMemcpy(
                &pNvxCamera->ExposureRegions,
                pExpRegRect,
                sizeof(NVX_CONFIG_ExposureRegionsRect));

            Err = NvxCameraUpdateExposureRegions(pNvxCamera);
            if (Err != NvSuccess)
            {
                // setting rejected, restore old state and throw error
                NvOsMemcpy(
                    &pNvxCamera->ExposureRegions,
                    &backupExpRegRect,
                    sizeof(NVX_CONFIG_ExposureRegionsRect));
                return OMX_ErrorUndefined;
            }

            break;
        }
        case NVX_IndexConfigFocusRegionsRect:
        {
            NvError Err = NvSuccess;
            NvCameraIspFocusingRegions oFocusRegions;
            NvMMDigitalZoomOperationInformation DZInfo;
            NVX_CONFIG_FocusRegionsRect *pfrr =
                (NVX_CONFIG_FocusRegionsRect *) pComponentConfigStructure;

            if (pfrr->nRegions == 0)
                return OMX_ErrorBadParameter;

            // get the preview information in order to compute the correct
            // focus region w.r.t camera's preview buffer
            Err = zb->GetAttribute(zb,
                NvMMDigitalZoomAttributeType_OperationInformation,
                sizeof(DZInfo), &DZInfo);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            oFocusRegions.numOfRegions = pfrr->nRegions;
            NvxCameraMappingPreviewOutputRegionToInputRegion(
                oFocusRegions.regions,
                &DZInfo,
                pfrr->regions,
                oFocusRegions.numOfRegions);

            Err = cb->SetAttribute(cb, NvCameraIspAttribute_AutoFocusRegions,
                                   0, sizeof(oFocusRegions), &oFocusRegions);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            NvOsMemcpy(&pNvxCamera->FocusRegions, pfrr,
                sizeof(NVX_CONFIG_FocusRegionsRect));

            break;
        }

        case NVX_IndexConfigEdgeEnhancement:
        {
            NVX_CONFIG_EDGEENHANCEMENT *peh = (NVX_CONFIG_EDGEENHANCEMENT *)pComponentConfigStructure;
            NvBool enable = peh->bEnable;

            cb->SetAttribute(cb, NvCameraIspAttribute_EdgeEnhancement,
                             0, sizeof(enable), &enable);

            if (enable) {
                NvF32 strengthBias = (NvF32)peh->strengthBias;
                cb->SetAttribute(cb, NvCameraIspAttribute_EdgeEnhanceStrengthBias,
                                 0, sizeof(NvF32), &strengthBias);
            }

            break;
        }

        //for Test Pattern
        case NVX_IndexConfigCameraTestPattern:
        {
            NVX_CONFIG_CAMERATESTPATTERN *pTestPattern =
                (NVX_CONFIG_CAMERATESTPATTERN *) pComponentConfigStructure;

            NvBool EnableTestPattern = (pTestPattern->TestPatternIndex > 0);

            cb->SetAttribute(cb, NvMMCameraAttribute_TestPattern,
                             0, sizeof(EnableTestPattern), &EnableTestPattern);
            break;
        }

        case NVX_IndexConfigSensorPowerOn:
        {
            NvBool bFiller = ((OMX_CONFIG_BOOLEANTYPE*)pComponentConfigStructure)->bEnabled;

            // trigger a sensor power up
            cb->SetAttribute(cb, NvMMCameraAttribute_SensorPowerOn,
                      0, sizeof(NvBool), &bFiller);
        }
        break;

        case NVX_IndexConfigBurstSkipCount:
        {
            OMX_PARAM_U32TYPE *pBurstSkipCount =
                (OMX_PARAM_U32TYPE *) pComponentConfigStructure;
            pNvxCamera->nBurstInterval = pBurstSkipCount->nU32;
            break;
        }

        case OMX_IndexConfigCommonGamma:
        {
            OMX_CONFIG_GAMMATYPE *pGamma =
                (OMX_CONFIG_GAMMATYPE *) pComponentConfigStructure;
            NvSFx Gamma;

            Gamma = pGamma->nGamma;

            cb->SetAttribute(cb, NvCameraIspAttribute_ImageGamma,
                             0, sizeof(Gamma), &Gamma);
            break;
        }

        case NVX_IndexConfigPreviewFrameCopy:
        {
            NVX_CONFIG_PREVIEW_FRAME_COPY *pThing =
                (NVX_CONFIG_PREVIEW_FRAME_COPY*) pComponentConfigStructure;
            NvMMDigitalZoomFrameCopyEnable FrameCopyEnable;

            NvOsMemset(&FrameCopyEnable, 0, sizeof(FrameCopyEnable));
            FrameCopyEnable.Enable = (NvBool) pThing->enable;
            FrameCopyEnable.skipCount = (NvU32) pThing->skipCount;

            zb->SetAttribute(zb, NvMMDigitalZoomAttributeType_PreviewFrameCopy,
                            0, sizeof(FrameCopyEnable), &FrameCopyEnable);
            break;
        }

        case NVX_IndexConfigThumbnailEnable:
        {
            NVX_CONFIG_THUMBNAILENABLE *pThing =
                (NVX_CONFIG_THUMBNAILENABLE*) pComponentConfigStructure;
            NvBool enable = (NvBool) pThing->bThumbnailEnabled;

            cb->SetAttribute(cb, NvMMCameraAttribute_Thumbnail,
                             0, sizeof(enable), &enable);
            break;
        }

        case NVX_IndexConfigStillConfirmationFrameCopy:
        {
            NVX_CONFIG_STILL_CONFIRMATION_FRAME_COPY *pThing =
                (NVX_CONFIG_STILL_CONFIRMATION_FRAME_COPY*) pComponentConfigStructure;
            NvBool enable = (NvBool)pThing->enable;

            zb->SetAttribute(zb,
                             NvMMDigitalZoomAttributeType_StillConfirmationFrameCopy,
                             0, sizeof(enable), &enable);
            break;
        }
        case NVX_IndexConfigStillFrameCopy:
        {
            NVX_CONFIG_STILL_YUV_FRAME_COPY *pThing =
                (NVX_CONFIG_STILL_YUV_FRAME_COPY*) pComponentConfigStructure;
            NvMMDigitalZoomFrameCopy FrameCopy;

            NvOsMemset(&FrameCopy, 0, sizeof(FrameCopy));
            FrameCopy.Enable = pThing->enable;
            FrameCopy.Width = pNvxCamera->nSurfWidthStillCapture;
            FrameCopy.Height = pNvxCamera->nSurfHeightStillCapture;

            zb->SetAttribute(
                zb,
                NvMMDigitalZoomAttributeType_StillFrameCopy,
                0,
                sizeof(FrameCopy),
                &FrameCopy);
            break;
        }

        case NVX_IndexConfigCameraConfiguration:
        {
            NvError err = NvSuccess;
            NVX_CONFIG_CAMERACONFIGURATION *pThing =
                (NVX_CONFIG_CAMERACONFIGURATION*) pComponentConfigStructure;
            OMX_STRING CameraConfigString = pThing->ConfigString;
            NvU32 Len = NvOsStrlen(CameraConfigString)+1;

            if (Len > NVX_MAX_CAMERACONFIGURATION_LENGTH)
            {
                eError = OMX_ErrorBadParameter;
            }
            else
            {
                err = cb->SetAttribute(cb,
                                       NvMMCameraAttribute_CameraConfiguration,
                                       NvMMSetAttrFlag_Notification,
                                       Len, CameraConfigString);
                if (err != NvSuccess)
                    eError = OMX_ErrorBadParameter;
                else
                    NvOsSemaphoreWait(pNvxCamera->oSrcCamera.SetAttrDoneSema);
            }
            break;
        }
        case NVX_IndexConfigCustomizedBlockInfo:
        {
            NvError Err = NvSuccess;
            NVX_CONFIG_CUSTOMIZEDBLOCKINFO *pCustomizedBlockInfo =
                (NVX_CONFIG_CUSTOMIZEDBLOCKINFO *) pComponentConfigStructure;
            NvMMImagerCustomInfo CustomInfo;

            NvOsMemset(&CustomInfo, 0, sizeof(CustomInfo));
            CustomInfo.SizeOfData = pCustomizedBlockInfo->CustomInfo.SizeofData;
            CustomInfo.pCustomData = (NvU8 *)pCustomizedBlockInfo->CustomInfo.Data;
            Err = cb->SetAttribute(cb,
                                   NvMMCameraAttribute_CustomizedBlockInfo,
                                   NvMMSetAttrFlag_Notification,
                                   sizeof(NvMMImagerCustomInfo),
                                   &CustomInfo);
            if (Err != NvSuccess)
                return OMX_ErrorBadParameter;
            else
                NvOsSemaphoreWait(pNvxCamera->oSrcCamera.SetAttrDoneSema);
            break;
        }
        case NVX_IndexConfigStereoCapable:
            return OMX_ErrorUnsupportedSetting;

        case NVX_IndexConfigSceneBrightness:
            return OMX_ErrorUnsupportedSetting;

        case NVX_IndexConfigSensorIspSupport:
            return OMX_ErrorUnsupportedSetting;

        case NVX_IndexConfigAdvancedNoiseReduction:
        {
            NVX_CONFIG_ADVANCED_NOISE_REDUCTION *pThing =
                (NVX_CONFIG_ADVANCED_NOISE_REDUCTION *) pComponentConfigStructure;
            NvF32 thresh[4];
            NvBool enable = pThing->enable != OMX_FALSE;
            NvError err = cb->SetAttribute(cb,
                                           NvMMCameraAttribute_ChromaFiltering,
                                           0,
                                           sizeof(enable), &enable);
            if (err == NvSuccess) {
                NvOsMemcpy(&thresh[0], pThing->lumaThreshold,
                           sizeof(pThing->lumaThreshold));
                NvOsMemcpy(&thresh[2], pThing->chromaThreshold,
                           sizeof(pThing->chromaThreshold));
                err = cb->SetAttribute(cb,
                                       NvMMCameraAttribute_ANRFilterParams,
                                       0,
                                       sizeof(thresh), thresh);
            }
            if (err == NvSuccess) {
                err = cb->SetAttribute(cb,
                    NvMMCameraAttribute_ANRFilterLumaIsoThreshold,
                    0,
                    sizeof(pThing->lumaIsoThreshold),
                    &pThing->lumaIsoThreshold);
            }
            if (err == NvSuccess) {
                err = cb->SetAttribute(cb,
                    NvMMCameraAttribute_ANRFilterChromaIsoThreshold,
                    0,
                    sizeof(pThing->chromaIsoThreshold),
                    &pThing->chromaIsoThreshold);
            }
            if (err != NvSuccess)
                eError = OMX_ErrorBadParameter;
            break;
        }

        case NVX_IndexConfigNoiseReductionV1:
        {
            NVX_CONFIG_NOISE_REDUCTION_V1 *pThing =
                (NVX_CONFIG_NOISE_REDUCTION_V1 *) pComponentConfigStructure;
            NvError err = NvSuccess;
            err = cb->SetAttribute(cb,
                NvMMCameraAttribute_NrV1LumaEnable,
                NvMMSetAttrFlag_Notification,
                sizeof(pThing->lumaEnable),
                &pThing->lumaEnable);
            if (err == NvSuccess)
            {
                NvOsSemaphoreWait(pNvxCamera->oSrcCamera.SetAttrDoneSema);
                err = cb->SetAttribute(cb,
                    NvMMCameraAttribute_NrV1LumaIsoThreshold,
                    NvMMSetAttrFlag_Notification,
                    sizeof(pThing->lumaIsoThreshold),
                    &pThing->lumaIsoThreshold);
            }
            if (err == NvSuccess)
            {
                NvOsSemaphoreWait(pNvxCamera->oSrcCamera.SetAttrDoneSema);
                err = cb->SetAttribute(cb,
                    NvMMCameraAttribute_NrV1LumaThreshold,
                    NvMMSetAttrFlag_Notification,
                    sizeof(pThing->lumaThreshold),
                    &pThing->lumaThreshold);
            }
            if (err == NvSuccess)
            {
                NvOsSemaphoreWait(pNvxCamera->oSrcCamera.SetAttrDoneSema);
                err = cb->SetAttribute(cb,
                    NvMMCameraAttribute_NrV1LumaSlope,
                    NvMMSetAttrFlag_Notification,
                    sizeof(pThing->lumaSlope),
                    &pThing->lumaSlope);
            }
            if (err == NvSuccess)
            {
                NvOsSemaphoreWait(pNvxCamera->oSrcCamera.SetAttrDoneSema);
                err = cb->SetAttribute(cb,
                    NvMMCameraAttribute_NrV1ChromaEnable,
                    NvMMSetAttrFlag_Notification,
                    sizeof(pThing->chromaEnable),
                    &pThing->chromaEnable);
            }
            if (err == NvSuccess)
            {
                NvOsSemaphoreWait(pNvxCamera->oSrcCamera.SetAttrDoneSema);
                err = cb->SetAttribute(cb,
                    NvMMCameraAttribute_NrV1ChromaIsoThreshold,
                    NvMMSetAttrFlag_Notification,
                    sizeof(pThing->chromaIsoThreshold),
                    &pThing->chromaIsoThreshold);
            }
            if (err == NvSuccess)
            {
                NvOsSemaphoreWait(pNvxCamera->oSrcCamera.SetAttrDoneSema);
                err = cb->SetAttribute(cb,
                    NvMMCameraAttribute_NrV1ChromaThreshold,
                    NvMMSetAttrFlag_Notification,
                    sizeof(pThing->chromaThreshold),
                    &pThing->chromaThreshold);
            }
            if (err == NvSuccess)
            {
                NvOsSemaphoreWait(pNvxCamera->oSrcCamera.SetAttrDoneSema);
                err = cb->SetAttribute(cb,
                    NvMMCameraAttribute_NrV1ChromaSlope,
                    NvMMSetAttrFlag_Notification,
                    sizeof(pThing->chromaSlope),
                    &pThing->chromaSlope);
            }
            if (err != NvSuccess)
                eError = OMX_ErrorBadParameter;
            else
                NvOsSemaphoreWait(pNvxCamera->oSrcCamera.SetAttrDoneSema);
            break;
        }

        case NVX_IndexConfigNoiseReductionV2:
        {
            NVX_CONFIG_NOISE_REDUCTION_V2 *pThing =
                (NVX_CONFIG_NOISE_REDUCTION_V2 *) pComponentConfigStructure;
            NvError err = NvSuccess;
            err = cb->SetAttribute(cb,
                NvMMCameraAttribute_NrV2ChromaEnable,
                NvMMSetAttrFlag_Notification,
                sizeof(pThing->chromaEnable),
                &pThing->chromaEnable);
            if (err == NvSuccess)
            {
                NvOsSemaphoreWait(pNvxCamera->oSrcCamera.SetAttrDoneSema);
                err = cb->SetAttribute(cb,
                    NvMMCameraAttribute_NrV2ChromaIsoThreshold,
                    NvMMSetAttrFlag_Notification,
                    sizeof(pThing->chromaIsoThreshold),
                    &pThing->chromaIsoThreshold);
            }
            if (err == NvSuccess)
            {
                NvOsSemaphoreWait(pNvxCamera->oSrcCamera.SetAttrDoneSema);
                err = cb->SetAttribute(cb,
                    NvMMCameraAttribute_NrV2ChromaThreshold,
                    NvMMSetAttrFlag_Notification,
                    sizeof(pThing->chromaThreshold),
                    &pThing->chromaThreshold);
            }
                if (err == NvSuccess)
            {
                NvOsSemaphoreWait(pNvxCamera->oSrcCamera.SetAttrDoneSema);
                err = cb->SetAttribute(cb,
                    NvMMCameraAttribute_NrV2ChromaSlope,
                    NvMMSetAttrFlag_Notification,
                    sizeof(pThing->chromaSlope),
                    &pThing->chromaSlope);
            }
            if (err == NvSuccess)
            {
                NvOsSemaphoreWait(pNvxCamera->oSrcCamera.SetAttrDoneSema);
                err = cb->SetAttribute(cb,
                    NvMMCameraAttribute_NrV2ChromaSpeed,
                    NvMMSetAttrFlag_Notification,
                    sizeof(pThing->chromaSpeed),
                    &pThing->chromaSpeed);
            }
            if (err == NvSuccess)
            {
                NvOsSemaphoreWait(pNvxCamera->oSrcCamera.SetAttrDoneSema);
                err = cb->SetAttribute(cb,
                    NvMMCameraAttribute_NrV2LumaConvolutionEnable,
                    NvMMSetAttrFlag_Notification,
                    sizeof(pThing->lumaConvolutionEnable),
                    &pThing->lumaConvolutionEnable);
            }
            if (err != NvSuccess)
                eError = OMX_ErrorBadParameter;
            else
                NvOsSemaphoreWait(pNvxCamera->oSrcCamera.SetAttrDoneSema);
            break;
        }

        case NVX_IndexConfigBayerGains:
        {
            NvError err = NvSuccess;
            NVX_CONFIG_BAYERGAINS *pThing =
                (NVX_CONFIG_BAYERGAINS*) pComponentConfigStructure;

            err = cb->SetAttribute(cb, NvMMCameraAttribute_DirectToSensorGains,
                                   NvMMSetAttrFlag_Notification,
                                   sizeof(pThing->gains), &pThing->gains);
            NvOsSemaphoreWait(pNvxCamera->oSrcCamera.SetAttrDoneSema);
            if (err != NvSuccess)
                eError = OMX_ErrorBadParameter;
            break;
        }
        case NVX_IndexConfigCapturePriority:
        {
            NvError err = NvSuccess;
            NVX_CONFIG_CAPTURE_PRIORITY *pCapturePriority =
                (NVX_CONFIG_CAPTURE_PRIORITY*) pComponentConfigStructure;
            NvBool Enable = (NvBool)pCapturePriority->Enable;

            err = cb->SetAttribute(cb, NvMMCameraAttribute_CapturePriority,
                    0, sizeof(Enable), &Enable);
            if (err != NvSuccess)
                eError = OMX_ErrorBadParameter;

            break;
        }

        case NVX_IndexConfigFastBurstEn:
        {
            NvError err = NvSuccess;
            NVX_CONFIG_FASTBURSTEN *pFastBurst =
                (NVX_CONFIG_FASTBURSTEN*) pComponentConfigStructure;
            NvBool Enable = (NvBool)pFastBurst->enable;

            err = cb->SetAttribute(cb, NvMMCameraAttribute_FastBurstMode,
                    0, sizeof(Enable), &Enable);

            if (err != NvSuccess)
                eError = OMX_ErrorBadParameter;

            break;
        }

        case NVX_IndexConfigManualTorchAmount:
        case NVX_IndexConfigManualFlashAmount:
        case NVX_IndexConfigAutoFlashAmount:
        {
            NVX_CONFIG_FLASHTORCHAMOUNT *pFlashAmount =
                (NVX_CONFIG_FLASHTORCHAMOUNT *) pComponentConfigStructure;
            NvError err = NvSuccess;
            NvU32 AttributeType;
            NvF32 Value = pFlashAmount->value;

            if (nIndex == NVX_IndexConfigManualTorchAmount)
            {
                AttributeType = NvMMCameraAttribute_ManualTorchAmount;
            }
            else if (nIndex == NVX_IndexConfigManualFlashAmount)
            {
                AttributeType = NvMMCameraAttribute_ManualFlashAmount;
            }
            else
            {
                AttributeType = NvMMCameraAttribute_AutoFlashAmount;
            }

            err = cb->SetAttribute(cb, AttributeType, 0, sizeof(Value), &Value);
            if (err != NvSuccess)
            {
                eError = OMX_ErrorBadParameter;
            }
            break;
        }
        case NVX_IndexConfigBracketCapture:
        {
            NvU32 AttributeType = NvMMCameraAttribute_BracketCapture;
            NvU32 i = 0;
            NvError err = NvSuccess;
            NVX_CONFIG_EXPOSUREBRACKETCAPTURE *pBracketCapture =
                (NVX_CONFIG_EXPOSUREBRACKETCAPTURE *) pComponentConfigStructure;

            NvMMBracketCaptureSettings BlocksBracketCapture;

            if (pBracketCapture->NumberOfCaptures <= MAX_BRACKET_CAPTURES)
            {   // 0 captures is the same as diasble

                BlocksBracketCapture.NumberImages = pBracketCapture->NumberOfCaptures;
                for (i = 0; i < BlocksBracketCapture.NumberImages; i++)
                {   // Cannot mem copy cannot guarantee OMX_Float == Nv_F
                    BlocksBracketCapture.ExpAdj[i] = pBracketCapture->FStopAdjustments[i];
                }

                err = cb->SetAttribute(cb, AttributeType, 0, sizeof(BlocksBracketCapture),
                                        &BlocksBracketCapture);

                if (err != NvSuccess)
                {
                    eError = OMX_ErrorBadParameter;
                }
            }
            else
            {
                eError = OMX_ErrorBadParameter;
            }
            break;
        }
        case NVX_IndexConfigContinuousAutoFocus:
        {
            NvError err = NvSuccess;
            NvBool enable;
            OMX_CONFIG_BOOLEANTYPE *pContinuousAutoFocus =
                (OMX_CONFIG_BOOLEANTYPE *) pComponentConfigStructure;

            enable = (NvBool)(pContinuousAutoFocus->bEnabled);

            // Add wait to make sure CAF is enabled/disabled properly
            // in camera driver. It also prevents race condition in
            // setting CAF attributes.
            err = cb->SetAttribute(cb,
                    NvCameraIspAttribute_ContinuousAutoFocus,
                    NvMMSetAttrFlag_Notification,
                    sizeof(NvBool),
                    &enable);
            if (err != NvSuccess)
                eError = OMX_ErrorBadParameter;
            else
                NvOsSemaphoreWait(pNvxCamera->oSrcCamera.SetAttrDoneSema);

            break;
        }

        case NVX_IndexConfigContinuousAutoFocusPause:
        {
            NvError err = NvSuccess;
            NvBool enable;
            OMX_CONFIG_BOOLEANTYPE *pContinuousAutoFocusPause =
                (OMX_CONFIG_BOOLEANTYPE *) pComponentConfigStructure;

            enable = (NvBool)(pContinuousAutoFocusPause->bEnabled);
            err = cb->SetAttribute(cb,
                    NvCameraIspAttribute_ContinuousAutoFocusPause,
                    NvMMSetAttrFlag_Notification, sizeof(NvBool), &enable);
            if (err != NvSuccess)
                eError = OMX_ErrorBadParameter;
            else
                NvOsSemaphoreWait(pNvxCamera->oSrcCamera.SetAttrDoneSema);

            break;
        }

        case NVX_IndexConfigCommonScaleType:
        {
            OMX_U32 Output= 0;
            NvError err = NvSuccess;
            NvMMDigitalZoomAttributeType AttributeType =
                NvMMDigitalZoomAttributeType_PreviewScaleType;
            NVX_CONFIG_COMMONSCALETYPE *st =
                (NVX_CONFIG_COMMONSCALETYPE *) pComponentConfigStructure;

            Output = NvxCameraGetDzOutputIndex(st->nPortIndex);
            if (Output == CameraDZOutputs_Preview)
            {
                AttributeType = NvMMDigitalZoomAttributeType_PreviewScaleType;
            }
            else if (Output == CameraDZOutputs_Still)
            {
                AttributeType = NvMMDigitalZoomAttributeType_StillScaleType;
            }
            else if (Output == CameraDZOutputs_Video)
            {
                AttributeType = NvMMDigitalZoomAttributeType_VideoScaleType;
            }
            else
            {
                AttributeType = NvMMDigitalZoomAttributeType_ThumbnailScaleType;
            }

            err = zb->SetAttribute(zb, AttributeType,
                             0, sizeof(NVX_CAMERA_SCALETYPE), &st->type);
            if (err != NvSuccess)
            {
                eError = OMX_ErrorBadParameter;
            }
            break;
        }
        case NVX_IndexConfigFrameCopyColorFormat:
        {
            NvError err = NvSuccess;
            NVX_CONFIG_FRAME_COPY_COLOR_FORMAT *pColorFormat =
                (NVX_CONFIG_FRAME_COPY_COLOR_FORMAT*) pComponentConfigStructure;
            NvMMDigitalZoomFrameCopyColorFormat ColorFormat;

            eError = NvxCameraConvertNvxImageColorToNvColor(
                pColorFormat->PreviewFrameCopy, &ColorFormat);
            if (eError != OMX_ErrorNone)
                break;

            err = zb->SetAttribute(zb,
                NvMMDigitalZoomAttributeType_PreviewFrameCopyColorFormat, 0,
                sizeof(ColorFormat), &ColorFormat);
            if (err != NvSuccess)
            {
                eError = OMX_ErrorBadParameter;
                break;
            }

            eError = NvxCameraConvertNvxImageColorToNvColor(
                pColorFormat->StillConfirmationFrameCopy, &ColorFormat);
            if (eError != OMX_ErrorNone)
                break;

            err = zb->SetAttribute(zb,
                    NvMMDigitalZoomAttributeType_StillConfirmationFrameCopyColorFormat,
                    0, sizeof(ColorFormat), &ColorFormat);
            if (err != NvSuccess)
            {
                eError = OMX_ErrorBadParameter;
                break;
            }

            eError = NvxCameraConvertNvxImageColorToNvColor(
                pColorFormat->StillFrameCopy, &ColorFormat);
            if (eError != OMX_ErrorNone)
                break;

            err = zb->SetAttribute(zb,
                NvMMDigitalZoomAttributeType_StillFrameCopyColorFormat, 0,
                    sizeof(ColorFormat), &ColorFormat);
            if (err != NvSuccess)
            {
                eError = OMX_ErrorBadParameter;
                break;
            }
            break;
        }
        case NVX_IndexConfigNSLBuffers:
        {
            NVX_CONFIG_NSLBUFFERS *pNumNSLBuffers =
                (NVX_CONFIG_NSLBUFFERS *) pComponentConfigStructure;
            NvU32 numBuffers;
            NvError err;

            numBuffers = pNumNSLBuffers->numBuffers;
            err = cb->SetAttribute(
                cb,
                NvMMCameraAttribute_NumNSLBuffers,
                NvMMSetAttrFlag_Notification,
                sizeof(numBuffers),
                &numBuffers);
            if (err != NvSuccess)
            {
                eError = OMX_ErrorBadParameter;
            }
            else
            {
                NvOsSemaphoreWait(pNvxCamera->oSrcCamera.SetAttrDoneSema);
            }
            break;
        }
        case NVX_IndexConfigNSLSkipCount:
        {
            NVX_CONFIG_NSLSKIPCOUNT *pNSLSkipCount =
                (NVX_CONFIG_NSLSKIPCOUNT *) pComponentConfigStructure;
            NvU32 skipCount;
            NvError err;

            skipCount = pNSLSkipCount->skipCount;
            err = cb->SetAttribute(
                cb,
                NvMMCameraAttribute_NSLSkipCount,
                0,
                sizeof(skipCount),
                &skipCount);
            if (err != NvSuccess)
            {
                eError = OMX_ErrorBadParameter;
            }
            break;
        }

        case NVX_IndexConfigCombinedCapture:
        {
            NVX_CONFIG_COMBINEDCAPTURE *pRequest =
                (NVX_CONFIG_COMBINEDCAPTURE *) pComponentConfigStructure;
            NvError err;

            if (pRequest->nPortIndex == (OMX_U32)s_nvx_port_still_capture)
            {
                err = CameraTakePictureComplex(
                    pNvxCamera,
                    (NvU32)pRequest->nNslImages,
                    (NvU32)pRequest->nNslPreFrames,
                    (NvU64)pRequest->nNslTimestamp,
                    (NvU32)pRequest->nImages,
                    0,
                    (NvU32)pRequest->nSkipCount);
            }
            else if (pRequest->nPortIndex == (OMX_U32)s_nvx_port_video_capture)
            {
                if (pRequest->nImages > 0)
                {
                    err = CameraVideoCaptureComplex(pNvxCamera, NV_TRUE, 0, 0, NV_FALSE);
                }
                else
                {
                    err = CameraVideoCaptureComplex(pNvxCamera, NV_FALSE, 0, 0, NV_FALSE);
                }
            }
            else
            {
                eError = OMX_ErrorBadPortIndex;
                break;
            }

            if (err != NvSuccess)
            {
                eError = OMX_ErrorBadParameter;
            }
            break;
        }
        case NVX_IndexConfigCameraVideoSpeed:
        {
            NvMMCameraVideoSpeed sVideoSpeed;
            NvError Err = NvSuccess;
            sVideoSpeed.bEnable = ((NVX_PARAM_VIDEOSPEED*)pComponentConfigStructure)->bEnable;
            sVideoSpeed.nVideoSpeed = ((NVX_PARAM_VIDEOSPEED*)pComponentConfigStructure)->nVideoSpeed;
            Err = pNvxCamera->oSrcCamera.hBlock->SetAttribute(
                      pNvxCamera->oSrcCamera.hBlock,
                      NvMMCameraAttribute_VideoSpeed, 0,
                      sizeof(NvMMCameraVideoSpeed), &sVideoSpeed);
            if (Err != NvSuccess)
            {
                return OMX_ErrorUndefined;
            }
        }
        break;

        case NVX_IndexConfigCustomPostview:
        {
            NvError err = NvSuccess;
            NVX_PARAM_CUSTOMPOSTVIEW *cpv =
                    (NVX_PARAM_CUSTOMPOSTVIEW *) pComponentConfigStructure;
            NvBool enable = cpv->bEnable;

            err = zb->SetAttribute(zb,
                                   NvMMDigitalZoomAttributeType_CustomPostview,
                                   0, sizeof(enable), &enable);
            if (err != NvSuccess)
               eError = OMX_ErrorBadParameter;
            break;
        }

        case NVX_IndexConfigLowLightThreshold:
        {
            NvS32 LowLightThreshold;
            NVX_CONFIG_LOWLIGHTTHRESHOLD *pLowLightThreshold =
                (NVX_CONFIG_LOWLIGHTTHRESHOLD *) pComponentConfigStructure;
            NvError err = NvSuccess;

            LowLightThreshold = pLowLightThreshold->nLowLightThreshold;

            err = cb->SetAttribute(cb, NvCameraIspAttribute_LowLightThreshold, 0,
                             sizeof(LowLightThreshold), &LowLightThreshold);

            if (err != NvSuccess)
            {
                eError = OMX_ErrorBadParameter;
            }
            break;
        }

        case NVX_IndexConfigMacroModeThreshold:
        {
            NvS32 MacroModeThreshold;
            NVX_CONFIG_MACROMODETHRESHOLD *pMacroModeThreshold =
                (NVX_CONFIG_MACROMODETHRESHOLD *) pComponentConfigStructure;
            NvError err = NvSuccess;

            MacroModeThreshold = pMacroModeThreshold->nMacroModeThreshold;

            err = cb->SetAttribute(cb, NvCameraIspAttribute_MacroModeThreshold, 0,
                             sizeof(MacroModeThreshold), &MacroModeThreshold);

            if (err != NvSuccess)
            {
                eError = OMX_ErrorBadParameter;
            }
            break;
        }

        case NVX_IndexConfigFocusMoveMsg:
        {
            NvBool enable;
            OMX_CONFIG_BOOLEANTYPE *pMsgOn =
                (OMX_CONFIG_BOOLEANTYPE *) pComponentConfigStructure;
            NvError err = NvSuccess;

            enable = (NvBool)pMsgOn->bEnabled;
            err = cb->SetAttribute(cb, NvCameraIspAttribute_FocusMoveMsg, 0,
                             sizeof(enable), &enable);

            if (err != NvSuccess)
            {
                eError = OMX_ErrorBadParameter;
            }
            break;
        }

        case NVX_IndexConfigFDLimit:
        {
            NvS32 newLimit, curLimit;
            NvError err = NvSuccess;
            NVX_CONFIG_FDLIMIT *pFDParams =
                (NVX_CONFIG_FDLIMIT *)pComponentConfigStructure;

            newLimit = pFDParams->limit;
            err = cb->GetAttribute(cb,
                                   NvMMCameraAttribute_FDLimit,
                                   sizeof(NvS32),
                                   &curLimit);

            if (err != NvSuccess)
                return OMX_ErrorUndefined;

            if (newLimit != curLimit)
            {
                err = cb->Extension(cb,
                                    NvMMCameraExtension_TrackFace,
                                    sizeof(NvS32),
                                    &newLimit, 0, NULL);
            }

            if (err != NvSuccess)
                return OMX_ErrorUndefined;
        }
        break;

        case NVX_IndexConfigFDDebug:
        {
            NvError err = NvSuccess;
            NVX_CONFIG_FDDEBUG *pFDMode =
                (NVX_CONFIG_FDDEBUG *)pComponentConfigStructure;

            err = cb->SetAttribute(cb,
                                    NvMMCameraAttribute_FDDebug, 0,
                                    sizeof(NvBool),
                                    &pFDMode->enable);

            if (err != NvSuccess)
                return OMX_ErrorUndefined;
        }
        break;

        case NVX_IndexConfigCaptureSensorModePref:
        {
            NvBool enable;
            NvError err = NvSuccess;
            OMX_CONFIG_BOOLEANTYPE *pCaptureSensorMode =
                (OMX_CONFIG_BOOLEANTYPE *) pComponentConfigStructure;

            enable = (NvBool)(pCaptureSensorMode->bEnabled);
            err = cb->SetAttribute(cb, NvMMCameraAttribute_CaptureSensorModePref,
                0, sizeof(NvBool), &enable);

            return eError;
        }
        break;

        case NVX_IndexConfigCameraMode:
        {
            NvError Err = NvSuccess;
            OMX_PARAM_U32TYPE *pCameraMode =
                (OMX_PARAM_U32TYPE *)pComponentConfigStructure;
            NvU32 cameraMode = pCameraMode->nU32;

            // set camera block
            Err = cb->SetAttribute(cb,
                                   NvMMCameraAttribute_CameraMode, 0,
                                   sizeof(NvU32), &cameraMode);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;

            // set dz block
            Err = zb->SetAttribute(zb,
                                   NvMMDigitalZoomAttributeType_CameraMode, 0,
                                   sizeof(NvU32), &cameraMode);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;
        }
        break;

        case NVX_IndexConfigForcePostView:
        {
            NvError Err = NvSuccess;
            OMX_CONFIG_BOOLEANTYPE *ppv =
                (OMX_CONFIG_BOOLEANTYPE *) pComponentConfigStructure;
            NvBool forcePostView = (NvBool)(ppv->bEnabled);

            Err = cb->SetAttribute(
                    cb,
                    NvMMCameraAttribute_ForcePostView, 0,
                    sizeof(NvBool), &forcePostView);
            if (Err != NvSuccess)
                return OMX_ErrorUndefined;
        }
        break;

        case NVX_IndexConfigConcurrentCaptureRequests:
        {
          OMX_PARAM_U32TYPE *pConcurrentCaptureRequests =
                (OMX_PARAM_U32TYPE *)pComponentConfigStructure;
          if (cb->SetAttribute(cb, NvMMCameraAttribute_ConcurrentCaptureRequests,
                    0, sizeof(NvU32), (NvU32 *)&pConcurrentCaptureRequests->nU32) != NvSuccess)
          {
              eError = OMX_ErrorBadParameter;
          }
        }
        break;

        case NVX_IndexConfigAEOverride:
        {
            NvS32 i;
            NvMMAEOverride AEOverride;
            NVX_CONFIG_AEOVERRIDE * aeoverride =
                (NVX_CONFIG_AEOVERRIDE *) pComponentConfigStructure;
            // deep copy the data
            AEOverride.AnalogGainsEnable = (NvBool) aeoverride->analog_gains_en;
            AEOverride.DigitalGainsEnable = (NvBool) aeoverride->digital_gains_en;
            for(i=0; i<4; i++){
                AEOverride.AnalogGains[i] = (NvF32) aeoverride->analog_gains[i];
                AEOverride.DigitalGains[i] = (NvF32) aeoverride->digital_gains[i];
            }
            AEOverride.ExposureEnable = (NvBool) aeoverride->exposure_en;
            AEOverride.NoOfExposures = (NvU32) aeoverride->no_of_exposures;
            for(i=0; i<PCL_MAX_EXPOSURES; i++){
                AEOverride.Exposures[i] = (NvF32) aeoverride->exposures[i];
            }
            // pass the data down
            if (cb->SetAttribute(cb, NvCameraIspAttribute_AEOverride, 0,
                sizeof(AEOverride), &AEOverride) != NvSuccess)
            {
                eError = OMX_ErrorBadParameter;
                NvOsDebugPrintf("OMX AEOverride: SetAttribute failed!\n");
            }
        }
        break;

        default:
            eError = NvxComponentBaseSetConfig(pNvComp, nIndex,
                                               pComponentConfigStructure);
    }
    return eError;
}

static OMX_ERRORTYPE
NvxCameraWorkerFunction(
    OMX_IN NvxComponent *pComponent,
    OMX_IN OMX_BOOL bAllPortsReady,
    OMX_OUT OMX_BOOL *pbMoreWork,
    OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    SNvxCameraData   *pNvxCamera = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortOutPreview = &pComponent->pPorts[s_nvx_port_preview];
    NvxPort *pPortOutStillCapture = &pComponent->pPorts[s_nvx_port_still_capture];
    NvxPort *pPortOutVideoCapture = &pComponent->pPorts[s_nvx_port_video_capture];
    NvxPort *pPortOutThumbnail = &pComponent->pPorts[s_nvx_port_thumbnail];
    NvxPort *pPortClock = &pComponent->pPorts[s_nvx_port_clock];
    NvxPort *pPortInput = &pComponent->pPorts[s_nvx_port_raw_input];
    OMX_BOOL bWouldFailPreview = OMX_TRUE;
    OMX_BOOL bWouldFailStillCapture = OMX_TRUE;
    OMX_BOOL bWouldFailVideoCapture = OMX_TRUE;
    OMX_BOOL bWouldFailThumbnail = OMX_TRUE;
    OMX_BOOL bWouldFailInput = OMX_TRUE;

    NVXTRACE((NVXT_WORKER, pComponent->eObjectType, "+NvxCameraWorkerFunction\n"));

    pNvxCamera = (SNvxCameraData *)pComponent->pComponentData;

    *pbMoreWork = (pPortOutPreview->pCurrentBufferHdr != NULL) ||
        (pPortOutStillCapture->pCurrentBufferHdr != NULL) ||
        (pPortOutVideoCapture->pCurrentBufferHdr != NULL) ||
        (pPortOutThumbnail->pCurrentBufferHdr != NULL) ||
        (pPortInput->pCurrentBufferHdr != NULL);

    if (pPortClock && pPortClock->pCurrentBufferHdr)
    {
        NvxPortReleaseBuffer(pPortClock, pPortClock->pCurrentBufferHdr);
    }

    if (!*pbMoreWork)
    {
        return OMX_ErrorNone;
    }

    eError = NvxNvMMTransformWorkerBase(
        &pNvxCamera->oDigitalZoom,
        s_nvx_port_dz_output_preview,
        &bWouldFailPreview);

    if (!pPortOutPreview->pCurrentBufferHdr)
    {
        NvxPortGetNextHdr(pPortOutPreview);
    }

    eError = NvxNvMMTransformWorkerBase(
        &pNvxCamera->oDigitalZoom,
        s_nvx_port_dz_output_still,
        &bWouldFailStillCapture);

    if (!pPortOutStillCapture->pCurrentBufferHdr)
    {
        NvxPortGetNextHdr(pPortOutStillCapture);
    }

    eError = NvxNvMMTransformWorkerBase(
        &pNvxCamera->oDigitalZoom,
        s_nvx_port_dz_output_video,
        &bWouldFailVideoCapture);

    if (!pPortOutVideoCapture->pCurrentBufferHdr)
    {
        NvxPortGetNextHdr(pPortOutVideoCapture);
    }

    eError = NvxNvMMTransformWorkerBase(
        &pNvxCamera->oDigitalZoom,
        s_nvx_port_dz_output_thumbnail,
        &bWouldFailThumbnail);

    if (!pPortOutThumbnail->pCurrentBufferHdr)
    {
        NvxPortGetNextHdr(pPortOutThumbnail);
    }

    eError = NvxNvMMTransformWorkerBase(
        &pNvxCamera->oSrcCamera,
        pNvxCamera->s_nvx_port_sc_input,
        &bWouldFailInput);

    if (!pPortInput->pCurrentBufferHdr)
    {
        NvxPortGetNextHdr(pPortInput);
    }

    *pbMoreWork = (pPortOutPreview->pCurrentBufferHdr != NULL ||
                   pPortOutStillCapture->pCurrentBufferHdr != NULL ||
                   pPortOutVideoCapture->pCurrentBufferHdr != NULL ||
                   pPortOutThumbnail->pCurrentBufferHdr != NULL ||
                   pPortInput->pCurrentBufferHdr != NULL) &&
                  !bWouldFailPreview &&
                  !bWouldFailStillCapture &&
                  !bWouldFailVideoCapture &&
                  !bWouldFailThumbnail &&
                  !bWouldFailInput;

    return eError;
}

static OMX_ERRORTYPE NvxCameraAcquireResources(OMX_IN NvxComponent *pComponent)
{
    SNvxCameraData *pNvxCamera = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxCameraAcquireResources\n"));

    pNvxCamera = (SNvxCameraData *)pComponent->pComponentData;
    NV_ASSERT(pNvxCamera);

    eError = NvxComponentBaseAcquireResources(pComponent);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pComponent->eObjectType, "ERROR:NvxCameraAcquireResources:"
           ":eError = %x:[%s(%d)]\n",eError,__FILE__, __LINE__));
        return eError;
    }

    eError = NvxCameraOpen(pNvxCamera, pComponent);
    if (OMX_ErrorNone != eError)
    {
        return eError;
    }

    return eError;
}

static OMX_ERRORTYPE NvxCameraReleaseResources(OMX_IN NvxComponent *pComponent)
{
    SNvxCameraData *pNvxCamera = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(pComponent);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxCameraReleaseResources\n"));

    pNvxCamera = (SNvxCameraData *)pComponent->pComponentData;
    NV_ASSERT(pNvxCamera);

    eError = NvxCameraClose(pNvxCamera);
    NvxCheckError(eError, NvxComponentBaseReleaseResources(pComponent));
    return eError;
}

static OMX_ERRORTYPE NvxCameraCanUseBuffer(OMX_IN NvxComponent *pComponent, OMX_IN OMX_U32 nPortIndex, OMX_IN OMX_U32 nSizeBytes, OMX_IN OMX_U8* pBuffer)
{
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxResetDZStream(
    SNvxCameraData *pNvxCamera,
    NvU32 streamNo)
{
    NvMMDigitalZoomResetBufferNegotiation ResetStream;
    NvError err = NvSuccess;

    NvOsMemset(&ResetStream,0,sizeof(ResetStream));
    ResetStream.StreamIndex = streamNo;
    err = pNvxCamera->oDigitalZoom.hBlock->Extension(
            pNvxCamera->oDigitalZoom.hBlock,
            NvMMDigitalZoomExtension_ResetBufferNegotiation,
            sizeof(ResetStream),
            &ResetStream,
            0, NULL);
    if (err == NvSuccess)
    {
        pNvxCamera->oDigitalZoom.oPorts[streamNo].bBufferNegotiationDone =
                                                  OMX_FALSE;
    }
    return NvxTranslateErrorCode(err);
}

static OMX_BOOL NvxIsDZPortTunneled(
    SNvxCameraData *pNvxCamera,
    NvU32 portNo)
{
    int portType =
        pNvxCamera->oDigitalZoom.oPorts[portNo].nType;

    return (portType == TF_TYPE_INPUT_TUNNELED) ||
           (portType == TF_TYPE_OUTPUT_TUNNELED);
}

static OMX_ERRORTYPE NvxResetCameraStream(
    SNvxCameraData *pNvxCamera,
    NvU32 streamNo)
{
    NvMMCameraResetBufferNegotiation ResetStream;
    NvError err = NvSuccess;

    NvOsMemset(&ResetStream,0,sizeof(ResetStream));
    ResetStream.StreamIndex = streamNo;
    err = pNvxCamera->oSrcCamera.hBlock->Extension(
            pNvxCamera->oSrcCamera.hBlock,
            NvMMCameraExtension_ResetBufferNegotiation,
            sizeof(ResetStream),
            &ResetStream,
            0, NULL);
    if (err == NvSuccess)
    {
        pNvxCamera->oSrcCamera.oPorts[streamNo].bBufferNegotiationDone =
                                                OMX_FALSE;
    }
    return NvxTranslateErrorCode(err);
}

static OMX_ERRORTYPE NvxReturnBuffersToCamera(
    SNvxCameraData *pNvxCamera,
    NvU32 streamNo)
{
    NvError err = NvSuccess;

    err = pNvxCamera->oDigitalZoom.hBlock->AbortBuffers(
              pNvxCamera->oDigitalZoom.hBlock,
              streamNo);
    if (err != NvSuccess)
    {
        return OMX_ErrorInvalidState;
    }
    err = NvOsSemaphoreWaitTimeout(
              pNvxCamera->oDigitalZoom.oPorts[streamNo].blockAbortDone,
              NVXCAMERA_ABORT_BUFFERS_TIMEOUT);
    if (err != NvSuccess)
    {
        return (err == NvError_Timeout)? OMX_ErrorTimeout :
                                         OMX_ErrorInvalidState;
    }
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxResetCamDZCaptureStream(
    SNvxCameraData *pNvxCamera,
    NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvMMCameraPin EnabledPin = 0;
    NvMMBlockHandle hNvMMCameraBlock = pNvxCamera->oSrcCamera.hBlock;

    hNvMMCameraBlock->GetAttribute(
        hNvMMCameraBlock,
        NvMMCameraAttribute_PinEnable,
        sizeof(NvMMCameraPin),
        &EnabledPin);

    EnabledPin &= ~NvMMCameraPin_Capture;

    hNvMMCameraBlock->SetAttribute(
        hNvMMCameraBlock,
        NvMMCameraAttribute_PinEnable,
        0,
        sizeof(NvMMCameraPin),
        &EnabledPin);

    NvxMutexUnlock(pNvComp->hWorkerMutex);

    // return buffers from dz to camera
    eError = NvxReturnBuffersToCamera(
        pNvxCamera,
        s_nvx_port_dz_input_capture);

    // reset all of the capture output streams on DZ, now that all pins
    // are disabled
    // TODO nlord is it ok to reset streams that weren't previously active?
    // for example, if the use-case only had still and video, will it hurt
    // to reset thumbnail here too when still and video are disabled?
    // normal use-case doesn't use thumbnail anyways, so if it passes,
    // we should be ok!
    eError = NvxResetDZStream(pNvxCamera, s_nvx_port_dz_output_still);
    if (eError != OMX_ErrorNone)
    {
        NvxMutexLock(pNvComp->hWorkerMutex);
        return eError;
    }

    eError = NvxResetDZStream(pNvxCamera, s_nvx_port_dz_output_video);
    if (eError != OMX_ErrorNone)
    {
        NvxMutexLock(pNvComp->hWorkerMutex);
        return eError;
    }

    eError = NvxResetDZStream(pNvxCamera, s_nvx_port_dz_output_thumbnail);
    if (eError != OMX_ErrorNone)
    {
        NvxMutexLock(pNvComp->hWorkerMutex);
        return eError;
    }

    // reset DZ's capture input
    eError = NvxResetDZStream(pNvxCamera,s_nvx_port_dz_input_capture);
    if (eError != OMX_ErrorNone)
    {
        NvxMutexLock(pNvComp->hWorkerMutex);
        return eError;
    }

    // reset Camera's capture output
    eError = NvxResetCameraStream(pNvxCamera,pNvxCamera->s_nvx_port_sc_output_capture);
    if (eError != OMX_ErrorNone)
    {
        NvxMutexLock(pNvComp->hWorkerMutex);
        return eError;
    }

    NvxMutexLock(pNvComp->hWorkerMutex);

    return eError;
}

static OMX_ERRORTYPE NvxEnableCaptureOutputPort(
    SNvxCameraData *pNvxCamera,
    NvxComponent *pNvComp,
    OMX_U32 nPort)
{
#define NUM_CAMERA_OUTPUT_PORTS 3
    NvU32 streamsToNegotiate[NUM_CAMERA_OUTPUT_PORTS];
#undef NUM_CAMERA_OUTPUT_PORTS
    NvU32 numStreamsToNegotiate = 0;
    NvBool EnableThumbnail = NV_FALSE;
    NvU32 streamNo = 0;
    NvMMCameraPin EnabledPin = 0;
    NvMMDigitalZoomPin EnabledDZPin = 0;
    nvxCameraPortSet portSet;
    NvMMBlockHandle hNvMMCameraBlock = pNvxCamera->oSrcCamera.hBlock;
    NvMMBlockHandle hNvMMDzBlock = pNvxCamera->oDigitalZoom.hBlock;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvError Err = NvSuccess;
    NvxPort *pPort = &pNvComp->pPorts[nPort];
    OMX_BOOL bDeepTunneled = OMX_FALSE;
    SNvxNvMMPort *portCamOut =
        &pNvxCamera->oSrcCamera.oPorts[pNvxCamera->s_nvx_port_sc_output_capture];
    SNvxNvMMPort *portDzIn =
        &pNvxCamera->oDigitalZoom.oPorts[NvMMDigitalZoomStreamIndex_Input];
    SNvxNvMMPort *portDzOut = NULL;


    hNvMMDzBlock->GetAttribute(
        hNvMMDzBlock,
        NvMMDigitalZoomAttributeType_PinEnable,
        sizeof(EnabledDZPin),
        &EnabledDZPin);

    nvxCameraPortSetInit(&portSet);
    nvxCameraPortSetAdd(&portSet, portCamOut);
    nvxCameraPortSetAdd(&portSet, portDzIn);

    if (nPort == (OMX_U32)s_nvx_port_still_capture)
    {
        streamNo = (NvU32)s_nvx_port_dz_output_still;
        EnabledDZPin |= NvMMDigitalZoomPin_Still;
    }
    else if (nPort == (OMX_U32)s_nvx_port_video_capture)
    {
        streamNo = (NvU32)s_nvx_port_dz_output_video;
        EnabledDZPin |= NvMMDigitalZoomPin_Video;
    }
    else if (nPort == (OMX_U32)s_nvx_port_thumbnail)
    {
        streamNo = (NvU32)s_nvx_port_dz_output_thumbnail;
        EnabledDZPin |= NvMMDigitalZoomPin_Thumbnail;

        //enable thumbnail
        EnableThumbnail = NV_TRUE;
        Err = hNvMMCameraBlock->SetAttribute(
            hNvMMCameraBlock,
            NvMMCameraAttribute_Thumbnail,
            0,
            sizeof(NvBool),
            &EnableThumbnail);
        if (Err != NvSuccess)
        {
            return OMX_ErrorBadParameter;
        }
    }
    else
    {
        NV_ASSERT(!"not an existing capture port");
        return OMX_ErrorBadParameter;
    }

    hNvMMDzBlock->SetAttribute(
        hNvMMDzBlock,
        NvMMDigitalZoomAttributeType_PinEnable,
        0,
        sizeof(EnabledDZPin),
        &EnabledDZPin);

    // resets the *entire* capture stream.  maybe we could make this smarter so
    // that we only reset the output pins on DZ that we need to?  for a first
    // pass, let's be safe and reset the whole thing.
    NvxResetCamDZCaptureStream(pNvxCamera, pNvComp);

    hNvMMCameraBlock->GetAttribute(
        hNvMMCameraBlock,
        NvMMCameraAttribute_PinEnable,
        sizeof(NvMMCameraPin),
        &EnabledPin);

    EnabledPin |= NvMMCameraPin_Capture;

    hNvMMCameraBlock->SetAttribute(
        hNvMMCameraBlock,
        NvMMCameraAttribute_PinEnable,
        0,
        sizeof(NvMMCameraPin),
        &EnabledPin);

    // start buffer negotiation between camera and dz, if it's not done already:
    if (!portCamOut->bBufferNegotiationDone ||
        !portDzIn->bBufferNegotiationDone)
    {
        eError = NvxNvMMTransformTunnelBlocks(&pNvxCamera->oSrcCamera,
                 pNvxCamera->s_nvx_port_sc_output_capture,
                 &pNvxCamera->oDigitalZoom,
                 s_nvx_port_dz_input_capture);

        if (eError != OMX_ErrorNone)
            return eError;
    }

    // enable/re-enable all output ports and trigger their negotiations
    NvxMutexUnlock (pNvComp->hWorkerMutex);
    if (EnabledDZPin & NvMMDigitalZoomPin_Still)
    {
        portDzOut = &pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_still];
        portDzOut->nWidth = pNvxCamera->nSurfWidthStillCapture;
        portDzOut->nHeight = pNvxCamera->nSurfHeightStillCapture;
        nvxCameraPortSetAdd(&portSet, portDzOut);

        pPort = &pNvComp->pPorts[s_nvx_port_still_capture];
        bDeepTunneled = (pPort->bNvidiaTunneling) &&
            (pPort->eNvidiaTunnelTransaction == ENVX_TRANSACTION_NVMM_TUNNEL);

        if (!portDzOut->bBufferNegotiationDone)
        {
            eError = NvxNvMMTransformSetupOutput(
                &pNvxCamera->oDigitalZoom,
                s_nvx_port_dz_output_still,
                pPort,
                s_nvx_port_dz_input_capture,
                CAMERA_MAX_OUTPUT_BUFFERS_STILL,
                CAMERA_MIN_OUTPUT_BUFFERSIZE_CAPTURE);
            if (eError != OMX_ErrorNone)
            {
                NvxMutexLock(pNvComp->hWorkerMutex);
                return eError;
            }

            if (!bDeepTunneled)
            {
                streamsToNegotiate[numStreamsToNegotiate++] = s_nvx_port_dz_output_still;
            }
        }
    }
    if (EnabledDZPin & NvMMDigitalZoomPin_Video)
    {
        portDzOut = &pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_video];
        portDzOut->nWidth = pNvxCamera->nSurfWidthVideoCapture;
        portDzOut->nHeight = pNvxCamera->nSurfHeightVideoCapture;
        nvxCameraPortSetAdd(&portSet, portDzOut);

        pPort = &pNvComp->pPorts[s_nvx_port_video_capture];
        bDeepTunneled = (pPort->bNvidiaTunneling) &&
            (pPort->eNvidiaTunnelTransaction == ENVX_TRANSACTION_NVMM_TUNNEL);

        if (!portDzOut->bBufferNegotiationDone)
        {
            eError = NvxNvMMTransformSetupOutput(
                &pNvxCamera->oDigitalZoom,
                s_nvx_port_dz_output_video,
                pPort,
                s_nvx_port_dz_input_capture,
                CAMERA_MAX_OUTPUT_BUFFERS_VIDEO,
                CAMERA_MIN_OUTPUT_BUFFERSIZE_CAPTURE);
            if (eError != OMX_ErrorNone)
            {
                NvxMutexLock(pNvComp->hWorkerMutex);
                return eError;
            }

            if (!bDeepTunneled)
            {
                streamsToNegotiate[numStreamsToNegotiate++] = s_nvx_port_dz_output_video;
            }
        }
    }
    if (EnabledDZPin & NvMMDigitalZoomPin_Thumbnail)
    {
        portDzOut = &pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_thumbnail];
        portDzOut->nWidth = pNvxCamera->nSurfWidthThumbnail;
        portDzOut->nHeight = pNvxCamera->nSurfHeightThumbnail;
        nvxCameraPortSetAdd(&portSet, portDzOut);

        pPort = &pNvComp->pPorts[s_nvx_port_thumbnail];
        bDeepTunneled = (pPort->bNvidiaTunneling) &&
            (pPort->eNvidiaTunnelTransaction == ENVX_TRANSACTION_NVMM_TUNNEL);

        if (!portDzOut->bBufferNegotiationDone)
        {
            eError = NvxNvMMTransformSetupOutput(
                &pNvxCamera->oDigitalZoom,
                s_nvx_port_dz_output_thumbnail,
                pPort,
                s_nvx_port_dz_input_capture,
                CAMERA_MAX_OUTPUT_BUFFERS_THUMBNAIL,
                CAMERA_MIN_OUTPUT_BUFFERSIZE_CAPTURE);
            if (eError != OMX_ErrorNone)
            {
                NvxMutexLock(pNvComp->hWorkerMutex);
                return eError;
            }

            if (!bDeepTunneled)
            {
                streamsToNegotiate[numStreamsToNegotiate++] = s_nvx_port_dz_output_thumbnail;
            }
        }
    }

    CameraNegotiateStreamBuffers(
        &pNvxCamera->oDigitalZoom,
        NV_TRUE,
        streamsToNegotiate,
        numStreamsToNegotiate);

    // Now wait for all internal NvMM-level buffer negotiation to finish.
    eError = nvxCameraWaitBufferNegotiationComplete(
        pNvxCamera,
        &portSet,
        NVXCAMERA_NEGOTIATE_BUFFERS_TIMEOUT);

    NvxMutexLock(pNvComp->hWorkerMutex);

    return eError;
}

// DZ generates many capture output streams from one shared input stream.
// this function makes sure that all capture output pins are disabled before
// returning the buffers and resetting the streams.
static OMX_ERRORTYPE
NvxDisableCaptureOutputPort(
    SNvxCameraData *pNvxCamera,
    NvxComponent *pNvComp,
    OMX_U32 nPort)
{
    NvU32 streamNo = 0;
    NvMMDigitalZoomPin EnabledDZPin = 0;
    NvMMBlockHandle hNvMMCameraBlock = pNvxCamera->oSrcCamera.hBlock;
    NvMMBlockHandle hNvMMDzBlock = pNvxCamera->oDigitalZoom.hBlock;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvError Err = NvSuccess;

    hNvMMDzBlock->GetAttribute(
        hNvMMDzBlock,
        NvMMDigitalZoomAttributeType_PinEnable,
        sizeof(EnabledDZPin),
        &EnabledDZPin);

    if (nPort == (OMX_U32)s_nvx_port_still_capture)
    {
        streamNo = (NvU32)s_nvx_port_dz_output_still;
        EnabledDZPin &= ~NvMMDigitalZoomPin_Still;
    }
    else if (nPort == (OMX_U32)s_nvx_port_video_capture)
    {
        streamNo = (NvU32)s_nvx_port_dz_output_video;
        EnabledDZPin &= ~NvMMDigitalZoomPin_Video;
    }
    else if (nPort == (OMX_U32)s_nvx_port_thumbnail)
    {
        streamNo = (NvU32)s_nvx_port_dz_output_thumbnail;
        EnabledDZPin &= ~NvMMDigitalZoomPin_Thumbnail;
    }
    else
    {
        NV_ASSERT(!"not an existing capture port");
        return OMX_ErrorBadParameter;
    }

    hNvMMDzBlock->SetAttribute(
        hNvMMDzBlock,
        NvMMDigitalZoomAttributeType_PinEnable,
        0,
        sizeof(EnabledDZPin),
        &EnabledDZPin);

    if (NvxIsDZPortTunneled(pNvxCamera, streamNo))
    {
        NvxComponent *pNvCompTunnelled =
            NvxComponentFromHandle(pNvComp->pPorts[nPort].hTunnelComponent);
        OMX_U32 TunnelledPort = pNvComp->pPorts[nPort].nTunnelPort;

        if (pNvCompTunnelled->pPorts[TunnelledPort].oPortDef.bEnabled)
        {
            return OMX_ErrorNotReady;
        }
    }

    // if *any* remaining capture outputs are still enabled,
    // we can't return buffers or do any of the reset logic
    if ((EnabledDZPin & (NvMMDigitalZoomPin_Still |
                         NvMMDigitalZoomPin_Video |
                         NvMMDigitalZoomPin_Thumbnail)))
    {
        return OMX_ErrorNone;
    }

    eError = NvxResetCamDZCaptureStream(pNvxCamera, pNvComp);

    // disable thumbnail attr here, so that it doesn't interfere with the
    // port reset logic (e.g. by returning early in the error case, which
    // we can't do much about right now)
    if (nPort == s_nvx_port_thumbnail)
    {
        NvBool EnableThumbnail = NV_FALSE;
        Err = hNvMMCameraBlock->SetAttribute(
            hNvMMCameraBlock,
            NvMMCameraAttribute_Thumbnail,
            0,
            sizeof(NvBool),
            &EnableThumbnail);
        if (Err != NvSuccess)
        {
            return OMX_ErrorBadParameter;
        }
    }

    return eError;
}

static OMX_ERRORTYPE NvxCameraEnableInputPort(NvxComponent *pNvComp, SNvxCameraData *pNvxCamera)
{
    OMX_ERRORTYPE eError;

    eError = NvxNvMMTransformSetupCameraInput(&pNvxCamera->oSrcCamera,
        pNvxCamera->s_nvx_port_sc_input,
        &pNvComp->pPorts[s_nvx_port_raw_input],
        CAMERA_MAX_INPUT_BUFFERS,
        CAMERA_MIN_INPUT_BUFFERSIZE,
        OMX_FALSE);
    if (eError != OMX_ErrorNone)
        return eError;

    if (NvError_Success != CameraNegotiateStreamBuffers(&pNvxCamera->oSrcCamera, NV_FALSE, &pNvxCamera->s_nvx_port_sc_input, 1))
        return OMX_ErrorUndefined;

    return NvxNvMMTransformSetInputSurfaceHook(&pNvxCamera->oSrcCamera, NvxCameraInputSurfaceHook);
}

/**
 * uEventType == 0 => port disable
 * uEventType == 1 => port enable
 */
static OMX_ERRORTYPE NvxCameraPortEventHandler(
    NvxComponent *pNvComp,
    OMX_U32 nPort,
    OMX_U32 uEventType)
{
    SNvxCameraData *pNvxCamera = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvMMBlockHandle hNvMMDzBlock = NULL;
    NvMMBlockHandle hNvMMCameraBlock = NULL;
#if USE_ANDROID_NATIVE_WINDOW
    NvxPort *pPort = NULL;
    nvxCameraPortSet portSet;
#endif
    NvMMCameraPin EnabledPin = 0;
    NvMMDigitalZoomPin EnabledDZPin = 0;
    NvError Err = NvSuccess;
    NvBool EnableInputBufferReturn = NV_TRUE;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType,
        "+NvxCameraPortEventHandler\n"));

    pNvxCamera = (SNvxCameraData *)pNvComp->pComponentData;
    if (!pNvxCamera->bInitialized)
        return OMX_ErrorNone;

    // do nothing when state is loaded
    if (pNvComp->eState == OMX_StateLoaded)
    {
        return OMX_ErrorNone;
    }

    hNvMMDzBlock = pNvxCamera->oDigitalZoom.hBlock;
    hNvMMCameraBlock = pNvxCamera->oSrcCamera.hBlock;

    hNvMMCameraBlock->GetAttribute(
        hNvMMCameraBlock,
        NvMMCameraAttribute_PinEnable,
        sizeof(NvMMCameraPin),
        &EnabledPin);

    hNvMMDzBlock->GetAttribute(
        hNvMMDzBlock,
        NvMMDigitalZoomAttributeType_PinEnable,
        sizeof(EnabledDZPin),
        &EnabledDZPin);

    switch (uEventType)
    {
        // port disable
        case 0:
            if (nPort == (OMX_U32)s_nvx_port_preview)
            {
#if USE_ANDROID_NATIVE_WINDOW
                EnabledPin &= ~NvMMCameraPin_Preview;
                hNvMMCameraBlock->SetAttribute(
                    hNvMMCameraBlock,
                    NvMMCameraAttribute_PinEnable,
                    0,
                    sizeof(NvMMCameraPin),
                    &EnabledPin);

                EnabledDZPin &= ~NvMMDigitalZoomPin_Preview;
                hNvMMDzBlock->SetAttribute(
                    hNvMMDzBlock,
                    NvMMDigitalZoomAttributeType_PinEnable,
                    0,
                    sizeof(EnabledDZPin),
                    &EnabledDZPin);

                // should we unlock the component's worker mutex around all
                // of the buffer returning and stream resetting like we do
                // for the capture/thumbnail cases?

                // return buffers from dz to camera
                eError = NvxReturnBuffersToCamera(
                                 pNvxCamera,
                                 s_nvx_port_dz_input_preview);
                if (eError != OMX_ErrorNone)
                {
                    return eError;
                }
                // reset DZ's preview output
                eError = NvxResetDZStream(pNvxCamera,s_nvx_port_dz_output_preview);
                if (eError != OMX_ErrorNone)
                {
                    return eError;
                }

                // reset DZ's preview input
                eError = NvxResetDZStream(pNvxCamera,s_nvx_port_dz_input_preview);
                if (eError != OMX_ErrorNone)
                {
                    return eError;
                }

                // reset Camera's preview output
                eError = NvxResetCameraStream(pNvxCamera,pNvxCamera->s_nvx_port_sc_output_preview);
                if (eError != OMX_ErrorNone)
                {
                    return eError;
                }
                pNvComp->pPorts[s_nvx_port_preview].oPortDef.bEnabled = OMX_FALSE;
                return OMX_ErrorNone;
#else
                NV_ASSERT(!"Not Implemented\n");
                return OMX_ErrorIncorrectStateOperation;
#endif
            }
            else if (nPort == (OMX_U32)s_nvx_port_raw_input)
            {
                EnableInputBufferReturn = NV_FALSE;
                Err = hNvMMCameraBlock->SetAttribute(hNvMMCameraBlock,
                        NvMMCameraAttribute_EnableInputBufferReturn, 0, sizeof(NvBool),
                        &EnableInputBufferReturn);
                if (Err != NvSuccess)
                {
                    return OMX_ErrorInvalidState;
                }

                NvxMutexUnlock(pNvComp->hWorkerMutex);
                NvxNvMMTransformReturnNvMMBuffers(&pNvxCamera->oSrcCamera, 0);
                NvxMutexLock(pNvComp->hWorkerMutex);

            }
            else
            {
                // we're probably disabling a capture port.  this function safely
                // checks whether the port is valid, and will assert/return a
                // OMX_ErrorBadParameter error if it is invalid.
                eError = NvxDisableCaptureOutputPort(pNvxCamera, pNvComp, nPort);
                if (eError != OMX_ErrorNone)
                {
                    return eError;
                }

            }
            break;

        // port enable
        case 1:
            if (nPort == (OMX_U32)s_nvx_port_preview)
            {
#if USE_ANDROID_NATIVE_WINDOW
                SNvxNvMMPort *portCamOut =
                      &pNvxCamera->oSrcCamera.oPorts[pNvxCamera->s_nvx_port_sc_output_preview];
                SNvxNvMMPort *portDzIn =
                      &pNvxCamera->oDigitalZoom.oPorts[NvMMDigitalZoomStreamIndex_InputPreview];
                SNvxNvMMPort *portDzOut =
                      &pNvxCamera->oDigitalZoom.oPorts[NvMMDigitalZoomStreamIndex_OutputPreview];
                eError = OMX_ErrorNone;

                pPort = &pNvComp->pPorts[nPort];
                nvxCameraPortSetInit(&portSet);

                EnabledPin |= NvMMCameraPin_Preview;
                hNvMMCameraBlock->SetAttribute(
                    hNvMMCameraBlock,
                    NvMMCameraAttribute_PinEnable,
                    0,
                    sizeof(NvMMCameraPin),
                    &EnabledPin);

                EnabledDZPin |= NvMMDigitalZoomPin_Preview;
                hNvMMDzBlock->SetAttribute(
                    hNvMMDzBlock,
                    NvMMDigitalZoomAttributeType_PinEnable,
                    0,
                    sizeof(EnabledDZPin),
                    &EnabledDZPin);

                nvxCameraPortSetAdd(&portSet, portCamOut);
                nvxCameraPortSetAdd(&portSet, portDzIn);
                nvxCameraPortSetAdd(&portSet, portDzOut);

                // start buffer negotiation between camera and dz, if it's not done already:
                if (!portCamOut->bBufferNegotiationDone ||
                    !portDzIn->bBufferNegotiationDone)
                {
                    eError = NvxNvMMTransformTunnelBlocks(&pNvxCamera->oSrcCamera,
                             pNvxCamera->s_nvx_port_sc_output_preview,
                             &pNvxCamera->oDigitalZoom,
                             s_nvx_port_dz_input_preview);

                    if (eError != OMX_ErrorNone)
                        return eError;
                }

                // start buffer negotiation between dz and tunneled block, if it's not done already:
                NvxMutexUnlock(pNvComp->hWorkerMutex);
                if (!portDzOut->bBufferNegotiationDone)
                {
                    NvU32 previewStream = NvMMDigitalZoomStreamIndex_OutputPreview;
                    eError = NvxNvMMTransformSetupOutput(&pNvxCamera->oDigitalZoom,
                            s_nvx_port_dz_output_preview,
                            &pNvComp->pPorts[s_nvx_port_preview],
                            s_nvx_port_dz_input_preview,
                            CAMERA_MAX_OUTPUT_BUFFERS_PREVIEW,
                            CAMERA_MIN_OUTPUT_BUFFERSIZE_PREVIEW);
                    if (eError != OMX_ErrorNone)
                    {
                        NvxMutexLock(pNvComp->hWorkerMutex);
                        return eError;
                    }

                    CameraNegotiateStreamBuffers(
                        &pNvxCamera->oDigitalZoom,
                        NV_TRUE,
                        &s_nvx_port_dz_output_preview,
                        1);

                }

                // Now wait for all internal NvMM-level buffer negotiation to finish.
                eError = nvxCameraWaitBufferNegotiationComplete(
                    pNvxCamera,
                    &portSet,
                    NVXCAMERA_NEGOTIATE_BUFFERS_TIMEOUT);
                NvxMutexLock(pNvComp->hWorkerMutex);
                return eError;
#else
                NV_ASSERT(!"Not Implemented\n");
                return OMX_ErrorIncorrectStateOperation;
#endif
            }
            else if (nPort == (OMX_U32)s_nvx_port_raw_input)
            {
                eError = NvxCameraEnableInputPort(pNvComp, pNvxCamera);
                if (eError != OMX_ErrorNone)
                    return eError;

                EnableInputBufferReturn = NV_TRUE;
                eError = hNvMMCameraBlock->SetAttribute(hNvMMCameraBlock,
                        NvMMCameraAttribute_EnableInputBufferReturn, 0, sizeof(NvBool),
                        &EnableInputBufferReturn);
                if (eError != NvSuccess)
                {
                    return OMX_ErrorInvalidState;
                }
            }
            else
            {
                // we're probably enabling a capture port.  this function
                // safely checks whether the port is valid, and will assert
                // or return an OMX_ErrorBadParameter error if it is invalid.
                NvxEnableCaptureOutputPort(pNvxCamera, pNvComp, nPort);
            }
            break;

        default:
            eError = OMX_ErrorBadParameter;
    }

    return eError;
}

static OMX_ERRORTYPE NvxCameraTunnelRequest(
    OMX_IN NvxComponent *pNvComp,
    OMX_IN OMX_U32 nPort,
    OMX_IN OMX_HANDLETYPE hTunneledComp,
    OMX_IN OMX_U32 nTunneledPort,
    OMX_INOUT OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
    OMX_ERRORTYPE omxErr = OMX_ErrorNone;
    NvU32 dzPortNo;
    SNvxCameraData *pNvxCamera = NULL;
    SNvxNvMMPort *pPort = NULL;

    if (pNvComp == NULL)
    {
        return OMX_ErrorBadParameter;
    }

    pNvxCamera = (SNvxCameraData *)pNvComp->pComponentData;
    if (pNvxCamera == NULL)
    {
        return OMX_ErrorBadParameter;
    }

    if (!pNvxCamera->bInitialized)
    {
        return OMX_ErrorNone;
    }

    if (hTunneledComp == NULL || pTunnelSetup == NULL)
    {   // Untunneling requested:
        // For OMX port nPort, find corresponding NvMMtransformBase port:
        if (nPort == (OMX_U32)s_nvx_port_still_capture)
        {
            dzPortNo = s_nvx_port_dz_output_still;
        }
        else if (nPort == (OMX_U32)s_nvx_port_video_capture)
        {
            dzPortNo = s_nvx_port_dz_output_video;
        }
        else if (nPort == (OMX_U32)s_nvx_port_preview)
        {
            dzPortNo = s_nvx_port_dz_output_preview;
        }
        else if (nPort == (OMX_U32)s_nvx_port_thumbnail)
        {
            dzPortNo = s_nvx_port_dz_output_thumbnail;
        }
        else
        {   // This is some other port of OMX Camera - ignore it:
            return omxErr;
        }
        pPort = &pNvxCamera->oDigitalZoom.oPorts[dzPortNo];

        // Set port state to untunneled:
        pPort->nType = TF_TYPE_OUTPUT;
        pPort->bTunneling = OMX_FALSE;
        pPort->pTunnelBlock = NULL;
    }

    return omxErr;
}


/***************************************************************************/
OMX_ERRORTYPE NvxCameraInit(OMX_IN  OMX_HANDLETYPE hComponent)
{
    NvxComponent *pNvComp     = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvError err = NvSuccess;

    SNvxCameraData *data = NULL;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_UNDESIGNATED, "+NvxCameraInit\n"));

    eError = NvxComponentCreate(hComponent, s_nvx_num_ports, &pNvComp);
    if (eError != OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR, NVXT_CAMERA,"ERROR:NvxCameraInit:"
            ":eError = %d:[%s(%d)]\n", eError,__FILE__, __LINE__));
        return eError;
    }

    pNvComp->eObjectType = NVXT_CAMERA;

    pNvComp->pComponentData = NvOsAlloc(sizeof(SNvxCameraData));
    if (!pNvComp->pComponentData)
        return OMX_ErrorInsufficientResources;

    data = (SNvxCameraData *)pNvComp->pComponentData;
    NvOsMemset(data, 0, sizeof(SNvxCameraData));
    /* Initialising ports to default CSI values */
    data->s_nvx_port_sc_input          = NvMMCameraStreamIndex_Input;
    data->s_nvx_port_sc_output_preview = NvMMCameraStreamIndex_OutputPreview;
    data->s_nvx_port_sc_output_capture = NvMMCameraStreamIndex_Output;

    err = NvOsSemaphoreCreate(&data->oStreamBufNegotiationSem, 0);
    if (err != NvSuccess)
    {
        NvOsFree(pNvComp->pComponentData);
        pNvComp->pComponentData = NULL;
        return NvxTranslateErrorCode(err);
    }

    // setup data

    pNvComp->DeInit             = NvxCameraDeInit;
    pNvComp->GetParameter       = NvxCameraGetParameter;
    pNvComp->SetParameter       = NvxCameraSetParameter;
    pNvComp->GetConfig          = NvxCameraGetConfig;
    pNvComp->SetConfig          = NvxCameraSetConfig;
    pNvComp->WorkerFunction     = NvxCameraWorkerFunction;
    pNvComp->AcquireResources   = NvxCameraAcquireResources;
    pNvComp->ReleaseResources   = NvxCameraReleaseResources;
    pNvComp->CanUseBuffer       = NvxCameraCanUseBuffer;
    pNvComp->FillThisBufferCB   = NvxCameraFillThisBuffer;
    #if USE_ANDROID_NATIVE_WINDOW
    pNvComp->Flush              = NvxCameraFlush;
    pNvComp->FreeBufferCB       = NvxCameraFreeBuffer;
    #endif
    pNvComp->EmptyThisBuffer    = NvxCameraEmptyThisBuffer;
    pNvComp->PreChangeState     = NvxCameraPreChangeState;
    pNvComp->PortEventHandler   = NvxCameraPortEventHandler;
    pNvComp->ComponentTunnelRequest = NvxCameraTunnelRequest;

    // setup clock port
    NvxPortInitOther(&pNvComp->pPorts[s_nvx_port_clock], OMX_DirInput, 4,
                     sizeof(OMX_TIME_MEDIATIMETYPE), OMX_OTHER_FormatTime);
    pNvComp->pPorts[s_nvx_port_clock].eNvidiaTunnelTransaction = ENVX_TRANSACTION_CLOCK;
    pNvComp->pPorts[s_nvx_port_clock].oPortDef.bEnabled = OMX_FALSE;

    // setup preview port
    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_preview], OMX_DirOutput,
                     CAMERA_MAX_OUTPUT_BUFFERS_PREVIEW, CAMERA_MIN_OUTPUT_BUFFERSIZE_PREVIEW,
                     OMX_IMAGE_CodingAutoDetect);
    pNvComp->pPorts[s_nvx_port_preview].oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    pNvComp->pPorts[s_nvx_port_preview].eNvidiaTunnelTransaction =
            ENVX_TRANSACTION_GFSURFACES;
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_preview], MAX_OUTPUT_BUFFERSIZE);
    //pNvComp->pPorts[s_nvx_port_preview].eSupplierPreference = OMX_BufferSupplyOutput;

    // setup still capture port
    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_still_capture], OMX_DirOutput,
                     CAMERA_MAX_OUTPUT_BUFFERS_STILL, CAMERA_MIN_OUTPUT_BUFFERSIZE_CAPTURE,
                     OMX_IMAGE_CodingAutoDetect);
    pNvComp->pPorts[s_nvx_port_still_capture].oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    pNvComp->pPorts[s_nvx_port_still_capture].eNvidiaTunnelTransaction =
            ENVX_TRANSACTION_GFSURFACES;
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_still_capture], MAX_OUTPUT_BUFFERSIZE);

    // setup video capture port
    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_video_capture], OMX_DirOutput,
                     CAMERA_MAX_OUTPUT_BUFFERS_VIDEO, CAMERA_MIN_OUTPUT_BUFFERSIZE_CAPTURE,
                     OMX_IMAGE_CodingAutoDetect);
    pNvComp->pPorts[s_nvx_port_video_capture].oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    pNvComp->pPorts[s_nvx_port_video_capture].eNvidiaTunnelTransaction =
            ENVX_TRANSACTION_GFSURFACES;
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_video_capture], MAX_OUTPUT_BUFFERSIZE);
    pNvComp->pPorts[s_nvx_port_video_capture].oPortDef.bEnabled = OMX_FALSE;

    // setup thumbnail port
    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_thumbnail], OMX_DirOutput,
                     CAMERA_MAX_OUTPUT_BUFFERS_THUMBNAIL, CAMERA_MIN_OUTPUT_BUFFERSIZE_CAPTURE,
                     OMX_IMAGE_CodingAutoDetect);
    pNvComp->pPorts[s_nvx_port_thumbnail].oPortDef.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    pNvComp->pPorts[s_nvx_port_thumbnail].eNvidiaTunnelTransaction =
            ENVX_TRANSACTION_GFSURFACES;
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_thumbnail], MAX_OUTPUT_BUFFERSIZE);
    pNvComp->pPorts[s_nvx_port_thumbnail].oPortDef.bEnabled = OMX_FALSE;

    // setup raw-data-in port
    NvxPortInitVideo(&pNvComp->pPorts[s_nvx_port_raw_input], OMX_DirInput, CAMERA_MAX_INPUT_BUFFERS,
                     CAMERA_MIN_INPUT_BUFFERSIZE, OMX_IMAGE_CodingAutoDetect);
    pNvComp->pPorts[s_nvx_port_raw_input].oPortDef.format.video.eColorFormat = OMX_COLOR_FormatRawBayer10bit;
    pNvComp->pPorts[s_nvx_port_raw_input].eNvidiaTunnelTransaction = ENVX_TRANSACTION_DEFAULT;
    pNvComp->pPorts[s_nvx_port_raw_input].oPortDef.bEnabled = OMX_FALSE;
    NvxPortSetNonTunneledSize(&pNvComp->pPorts[s_nvx_port_raw_input], CAMERA_MIN_INPUT_BUFFERSIZE);

    // these don't really make sense with our new dual-port capture arch,
    // so leave them at the sane oneshot still capture defaults
    INIT_STRUCT(data->SensorModeParams, pNvComp);
    data->SensorModeParams.nPortIndex = s_nvx_port_still_capture;
    data->SensorModeParams.bOneShot = OMX_TRUE;

    data->nCaptureCount = 1;
    data->nBurstInterval = 0;
    data->rawDumpFlag = 0;

    INIT_STRUCT(data->bAutoPauseAfterCapture, pNvComp);
    data->bAutoPauseAfterCapture.bEnabled = OMX_FALSE;

    data->PreviewEnabled = OMX_FALSE;
    data->Passthrough = OMX_FALSE;

    data->VideoSurfLayout = NvRmSurfaceLayout_Tiled;


    // Set default resolution
    data->nSurfWidthPreview = CAMERA_DEFAULT_OUTPUT_WIDTH;
    data->nSurfHeightPreview = CAMERA_DEFAULT_OUTPUT_HEIGHT;
    data->nSurfWidthStillCapture = CAMERA_DEFAULT_OUTPUT_WIDTH;
    data->nSurfHeightStillCapture = CAMERA_DEFAULT_OUTPUT_HEIGHT;
    data->nSurfWidthVideoCapture = CAMERA_DEFAULT_OUTPUT_WIDTH;
    data->nSurfHeightVideoCapture = CAMERA_DEFAULT_OUTPUT_HEIGHT;
    data->nSurfWidthThumbnail = CAMERA_DEFAULT_OUTPUT_WIDTH;
    data->nSurfHeightThumbnail = CAMERA_DEFAULT_OUTPUT_HEIGHT;
    data->nSurfWidthInput = CAMERA_DEFAULT_INPUT_WIDTH;
    data->nSurfHeightInput = CAMERA_DEFAULT_INPUT_HEIGHT;

    INIT_STRUCT(data->oCropPreview, pNvComp);
    data->oCropPreview.nPortIndex = s_nvx_port_preview;
    data->oCropPreview.nLeft = 0;
    data->oCropPreview.nTop = 0;
    data->oCropPreview.nWidth = 0;
    data->oCropPreview.nHeight = 0;

    INIT_STRUCT(data->oCropStillCapture, pNvComp);
    data->oCropStillCapture.nPortIndex = s_nvx_port_still_capture;
    data->oCropStillCapture.nLeft = 0;
    data->oCropStillCapture.nTop = 0;
    data->oCropStillCapture.nWidth = 0;
    data->oCropStillCapture.nHeight = 0;

    INIT_STRUCT(data->oCropVideoCapture, pNvComp);
    data->oCropVideoCapture.nPortIndex = s_nvx_port_video_capture;
    data->oCropVideoCapture.nLeft = 0;
    data->oCropVideoCapture.nTop = 0;
    data->oCropVideoCapture.nWidth = 0;
    data->oCropVideoCapture.nHeight = 0;

    data->MeteringMatrix[0][0] = NV_SFX_WHOLE_TO_FX(1);
    data->MeteringMatrix[0][1] = NV_SFX_WHOLE_TO_FX(0);
    data->MeteringMatrix[0][2] = NV_SFX_WHOLE_TO_FX(0);
    data->MeteringMatrix[1][0] = NV_SFX_WHOLE_TO_FX(1);
    data->MeteringMatrix[1][1] = NV_SFX_WHOLE_TO_FX(0);
    data->MeteringMatrix[1][2] = NV_SFX_WHOLE_TO_FX(0);
    data->MeteringMatrix[2][0] = NV_SFX_WHOLE_TO_FX(1);
    data->MeteringMatrix[2][1] = NV_SFX_WHOLE_TO_FX(0);
    data->MeteringMatrix[2][2] = NV_SFX_WHOLE_TO_FX(0);

    *data->SensorName = '\0';

    data->ThumbnailEnabled = OMX_FALSE;

    INIT_STRUCT(data->oConvergeAndLock, pNvComp);
    data->oConvergeAndLock.bUnlock = OMX_TRUE;
    data->oConvergeAndLock.bAutoFocus = OMX_FALSE;
    data->oConvergeAndLock.bAutoExposure = OMX_FALSE;
    data->oConvergeAndLock.bAutoWhiteBalance = OMX_FALSE;
    data->oConvergeAndLock.nTimeOutMS = 2000;

    INIT_STRUCT(data->oPrecaptureConverge, pNvComp);
    data->oPrecaptureConverge.bPrecaptureConverge = OMX_FALSE;
    data->oPrecaptureConverge.bContinueDuringCapture = OMX_TRUE;
    data->oPrecaptureConverge.nTimeOutMS = 5000;

    data->oSrcCamera.BlockSpecific = 0;

#if USE_ANDROID_NATIVE_WINDOW
    //Dummy - To prevent changes in NvmmTransformbase.c
    data->oDigitalZoom.VideoStreamInfo.NumOfSurfaces = 3;
#endif
    /* Register function pointers */
    pNvComp->pComponentName = "OMX.Nvidia.camera";
    pNvComp->sComponentRoles[0] = "camera.yuv";    // check the correct type
    pNvComp->nComponentRoles = 1;

    data->ccm.m00 = data->ccm.m01 = data->ccm.m02 = data->ccm.m03
        = data->ccm.m10 = data->ccm.m11 = data->ccm.m12 = data->ccm.m13
        = data->ccm.m20 = data->ccm.m21 = data->ccm.m22 = data->ccm.m23
        = data->ccm.m30 = data->ccm.m31 = data->ccm.m32 = data->ccm.m33 = 0;

    // Allocate Memory for Config File Path
    if (!data->pConfigFilePath)
    {
        data->pConfigFilePath = NvOsAlloc(NvOsStrlen(NVCAMERA_CONF_FILE_PATH) + 1);
        if (data->pConfigFilePath)
        {
            NvOsStrncpy(data->pConfigFilePath, NVCAMERA_CONF_FILE_PATH,
                    NvOsStrlen(NVCAMERA_CONF_FILE_PATH) + 1);
        }
    }
    return eError;
}

/* =========================================================================
 *                     I N T E R N A L
 * ========================================================================= */
static void s_GetCropRectAttr(SNvxCameraData *pNvxCamera, int nPortId,
                  OMX_CONFIG_RECTTYPE **pRect,
                  NvMMDigitalZoomAttributeType *rectEnable,
                  NvMMDigitalZoomAttributeType *rectKind)
{
    if (nPortId == s_nvx_port_preview)
    {
        *pRect = &pNvxCamera->oCropPreview;
        *rectKind = NvMMDigitalZoomAttributeType_PreviewCropRect;
        *rectEnable = NvMMDigitalZoomAttributeType_PreviewCrop;
    }
    else if (nPortId == s_nvx_port_still_capture)
    {
        *pRect = &pNvxCamera->oCropStillCapture;
        *rectKind = NvMMDigitalZoomAttributeType_StillCropRect;
        *rectEnable = NvMMDigitalZoomAttributeType_StillCrop;
    }
    else if (nPortId == s_nvx_port_video_capture)
    {
        *pRect = &pNvxCamera->oCropVideoCapture;
        *rectKind = NvMMDigitalZoomAttributeType_VideoCropRect;
        *rectEnable = NvMMDigitalZoomAttributeType_VideoCrop;
    }
    else if (nPortId == s_nvx_port_thumbnail)
    {
        *pRect = &pNvxCamera->oCropThumbnail;
        *rectKind = NvMMDigitalZoomAttributeType_ThumbnailCropRect;
        *rectEnable = NvMMDigitalZoomAttributeType_ThumbnailCrop;
    }
    else
    {
        NV_ASSERT(!"bad port index, but there's no way to return an error here");
    }
}

static NvError s_SetCrop(SNvxCameraData *pNvxCamera, int nPortId)
{
    OMX_CONFIG_RECTTYPE *pRect = NULL;
    NvRect oNvmmRect;
    NvMMDigitalZoomAttributeType eAttrRect = NvMMDigitalZoomAttributeType_None;
    NvMMDigitalZoomAttributeType eAttrEnable = NvMMDigitalZoomAttributeType_None;
    NvBool bCropping;
    NvError Status = NvSuccess;

    if (!pNvxCamera->bTransformDataInitialized)
    {
        return Status;
    }

    s_GetCropRectAttr(pNvxCamera,nPortId,&pRect,&eAttrEnable,&eAttrRect);
    if (pRect == NULL)
    {
        return Status;
    }

    if (pRect->nWidth && pRect->nHeight)
    {
        bCropping = NV_TRUE;
        oNvmmRect.left = pRect->nLeft;
        oNvmmRect.top = pRect->nTop;
        oNvmmRect.right = pRect->nLeft + (NvS32)pRect->nWidth;
        oNvmmRect.bottom = pRect->nTop + (NvS32)pRect->nHeight;
        Status = pNvxCamera->oDigitalZoom.hBlock->SetAttribute(
            pNvxCamera->oDigitalZoom.hBlock,
            eAttrRect,
            NvMMSetAttrFlag_Notification,
            sizeof(NvRect),
            &oNvmmRect);

        NvOsSemaphoreWait(pNvxCamera->oDigitalZoom.SetAttrDoneSema);

        if (NvSuccess != Status)
        {
            return Status;
        }
    }
    else
    {
        bCropping = NV_FALSE;
    }
    Status = pNvxCamera->oDigitalZoom.hBlock->SetAttribute(
        pNvxCamera->oDigitalZoom.hBlock,
        eAttrEnable,
        NvMMSetAttrFlag_Notification,
        sizeof(NvBool),
        &bCropping);
    NvOsSemaphoreWait(pNvxCamera->oDigitalZoom.SetAttrDoneSema);
    return Status;
}

static NvError s_GetCrop(SNvxCameraData *pNvxCamera, int nPortId)
{
    OMX_CONFIG_RECTTYPE *pRect = NULL;
    NvRect oNvmmRect;
    NvMMDigitalZoomAttributeType eAttrRect = NvMMDigitalZoomAttributeType_None;
    NvMMDigitalZoomAttributeType eAttrEnable = NvMMDigitalZoomAttributeType_None;
    NvBool bCropping = NV_FALSE;
    NvError Status = NvSuccess;

    if (!pNvxCamera->bTransformDataInitialized)
    {
        return Status;
    }

    s_GetCropRectAttr(pNvxCamera,nPortId,&pRect,&eAttrEnable,&eAttrRect);
    if (pRect == NULL)
    {
        return Status;
    }
    pRect->nLeft = 0;
    pRect->nTop = 0;
    pRect->nHeight = 0;
    pRect->nWidth = 0;

    Status = pNvxCamera->oDigitalZoom.hBlock->GetAttribute(
        pNvxCamera->oDigitalZoom.hBlock,
        eAttrEnable,
        sizeof(NvBool),
        &bCropping);
    if (!bCropping)
    {
        return Status;
    }

    Status = pNvxCamera->oDigitalZoom.hBlock->GetAttribute(
        pNvxCamera->oDigitalZoom.hBlock,
        eAttrRect,
        sizeof(NvRect),
        &oNvmmRect);
    if (NvSuccess != Status)
    {
        return Status;
    }

    pRect->nLeft = oNvmmRect.left;
    pRect->nTop = oNvmmRect.top;
    pRect->nHeight = (OMX_U32)(oNvmmRect.bottom - oNvmmRect.top);
    pRect->nWidth = (OMX_U32)(oNvmmRect.right - oNvmmRect.left);
    return Status;
}

static NvError s_GetWhiteBalance(SNvxCameraData *pNvxCamera, OMX_WHITEBALCONTROLTYPE *pMode)
{
    NvError status = NvSuccess;
    NvCameraIspWhiteBalanceMode wbMode;

    status = pNvxCamera->oSrcCamera.hBlock->GetAttribute(pNvxCamera->oSrcCamera.hBlock,
                                                        NvCameraIspAttribute_WhiteBalanceMode,
                                                        sizeof(wbMode), &wbMode);
    if (status != NvSuccess)
    {
        return status;
    }

    switch (wbMode)
    {
        case NvCameraIspWhiteBalanceMode_Sunlight:
            *pMode = OMX_WhiteBalControlSunLight;
            break;

        case NvCameraIspWhiteBalanceMode_Cloudy:
            *pMode = OMX_WhiteBalControlCloudy;
            break;

        case NvCameraIspWhiteBalanceMode_Shade:
            *pMode = OMX_WhiteBalControlShade;
            break;

        case NvCameraIspWhiteBalanceMode_Tungsten:
            *pMode = OMX_WhiteBalControlTungsten;
            break;

        case NvCameraIspWhiteBalanceMode_Incandescent:
            *pMode = OMX_WhiteBalControlIncandescent;
            break;

        case NvCameraIspWhiteBalanceMode_Fluorescent:
            *pMode = OMX_WhiteBalControlFluorescent;
            break;

        case NvCameraIspWhiteBalanceMode_Flash:
            *pMode = OMX_WhiteBalControlFlash;
            break;

        case NvCameraIspWhiteBalanceMode_Horizon:
            *pMode = OMX_WhiteBalControlHorizon;
            break;

        case NvCameraIspWhiteBalanceMode_WarmFluorescent:
            *pMode = (OMX_WHITEBALCONTROLTYPE) NVX_WhiteBalControlWarmFluorescent;
            break;

        case NvCameraIspWhiteBalanceMode_Twilight:
            *pMode = (OMX_WHITEBALCONTROLTYPE) NVX_WhiteBalControlTwilight;
            break;

        case NvCameraIspWhiteBalanceMode_None:
        case NvCameraIspWhiteBalanceMode_Auto:
        default:
            *pMode = OMX_WhiteBalControlAuto;
            break;
    }

    return status;
}

static NvError s_SetWhiteBalance(SNvxCameraData *pNvxCamera, OMX_WHITEBALCONTROLTYPE eWhiteBalControl)
{
    NvError status = NvSuccess;
    NvBool data;
    data = (OMX_WhiteBalControlOff == eWhiteBalControl) ? NV_FALSE : NV_TRUE;
    status = pNvxCamera->oSrcCamera.hBlock->SetAttribute(pNvxCamera->oSrcCamera.hBlock,
                                                NvCameraIspAttribute_ContinuousAutoWhiteBalance, 0,
                                                sizeof(data), &data);

    if (NvSuccess != status)
        return status;

    if (data)
    {
        NvCameraIspWhiteBalanceMode wbMode;

        switch (eWhiteBalControl)
        {
        case OMX_WhiteBalControlSunLight:
            wbMode = NvCameraIspWhiteBalanceMode_Sunlight;
            break;

        case OMX_WhiteBalControlCloudy:
            wbMode = NvCameraIspWhiteBalanceMode_Cloudy;
            break;

        case OMX_WhiteBalControlShade:
            wbMode = NvCameraIspWhiteBalanceMode_Shade;
            break;

        case OMX_WhiteBalControlTungsten:
            wbMode = NvCameraIspWhiteBalanceMode_Tungsten;
            break;

        case OMX_WhiteBalControlIncandescent:
            wbMode = NvCameraIspWhiteBalanceMode_Incandescent;
            break;

        case OMX_WhiteBalControlFluorescent:
            wbMode = NvCameraIspWhiteBalanceMode_Fluorescent;
            break;

        case OMX_WhiteBalControlFlash:
            wbMode = NvCameraIspWhiteBalanceMode_Flash;
            break;

        case OMX_WhiteBalControlHorizon:
            wbMode = NvCameraIspWhiteBalanceMode_Horizon;
            break;

        case ((OMX_WHITEBALCONTROLTYPE) NVX_WhiteBalControlWarmFluorescent):
            wbMode = NvCameraIspWhiteBalanceMode_WarmFluorescent;
            break;

        case ((OMX_WHITEBALCONTROLTYPE) NVX_WhiteBalControlTwilight):
            wbMode = NvCameraIspWhiteBalanceMode_Twilight;
            break;

        case OMX_WhiteBalControlAuto:
            wbMode = NvCameraIspWhiteBalanceMode_Auto;
            break;

        default:
            wbMode = NvCameraIspWhiteBalanceMode_None;
            break;
        }

        status = pNvxCamera->oSrcCamera.hBlock->SetAttribute(pNvxCamera->oSrcCamera.hBlock,
                NvCameraIspAttribute_WhiteBalanceMode, 0,
                sizeof(wbMode), &wbMode);
    }

    return status;
}

static NvError s_SetSpecificStylizeMode(SNvxCameraData *pNvxCamera, NvU32 mode);
static NvError s_SetSpecificStylizeMode(SNvxCameraData *pNvxCamera, NvU32 mode)
{
    return pNvxCamera->oSrcCamera.hBlock->SetAttribute(pNvxCamera->oSrcCamera.hBlock,
                                                       NvCameraIspAttribute_StylizeMode, 0,
                                                       sizeof(NvU32), &mode);
}

static NvError s_SetStylizeMode(SNvxCameraData *pNvxCamera, OMX_IMAGEFILTERTYPE eImageFilter)
{
    NvError status = NvSuccess;
    NvBool stylizeEnable = NV_TRUE;

    switch (eImageFilter)
    {
        case OMX_ImageFilterSolarize:
            status = s_SetSpecificStylizeMode(pNvxCamera, (NvU32)NVCAMERAISP_STYLIZE_SOLARIZE);
            break;

        case OMX_ImageFilterNegative:
            status = s_SetSpecificStylizeMode(pNvxCamera, (NvU32)NVCAMERAISP_STYLIZE_NEGATIVE);
            break;

        case NVX_ImageFilterPosterize:
            status = s_SetSpecificStylizeMode(pNvxCamera, (NvU32)NVCAMERAISP_STYLIZE_POSTERIZE);
            break;

        case NVX_ImageFilterSepia:
            status = s_SetSpecificStylizeMode(pNvxCamera, (NvU32)NVCAMERAISP_STYLIZE_SEPIA);
            break;

        case NVX_ImageFilterAqua:
            status = s_SetSpecificStylizeMode(pNvxCamera, (NvU32)NVCAMERAISP_STYLIZE_AQUA);
            break;

        case NVX_ImageFilterBW:
            status = s_SetSpecificStylizeMode(pNvxCamera, (NvU32)NVCAMERAISP_STYLIZE_BW);
            break;

        case OMX_ImageFilterEmboss:
            status = s_SetSpecificStylizeMode(pNvxCamera, (NvU32)NVCAMERAISP_STYLIZE_EMBOSS);
            break;

        case NVX_ImageFilterManual:
            status = NvError_NotSupported;
            break;

        case OMX_ImageFilterNone:
            // Nothing to do
            stylizeEnable = NV_FALSE;
            break;

        default:
            NV_ASSERT(!"Invalid value of eImageFilter!");
            return NvError_BadValue;
    }

    status = pNvxCamera->oSrcCamera.hBlock->SetAttribute(pNvxCamera->oSrcCamera.hBlock,
        NvCameraIspAttribute_Stylize, 0,
        sizeof(stylizeEnable), &stylizeEnable);

    return status;
}

// Fill out the surface format info
// including NumberOfSurfaces, ColorFormat,
// Layout and etc. for DZ buffer requirement
static NvError PopulateDZSurfaceFormatInfo(
                                NvMMNewBufferRequirementsInfo *pReq,
                                NvU32 StreamIndex)
{
    NV_ASSERT(pReq);

    if (pReq == NULL)
        return NvError_BadParameter;

    NvError e = NvSuccess;

    NvMMVideoFormat *pVideoFormat = &pReq->format.videoFormat;
    NvRmSurface *pSurf = &pVideoFormat->SurfaceDescription[0];
    NvxFOURCCFormat PixelFormat;

    // fill in the most common settings
    pVideoFormat->NumberOfSurfaces = 3;
    pSurf[0].ColorFormat = NvColorFormat_Y8;
    pSurf[1].ColorFormat = NvColorFormat_U8;
    pSurf[2].ColorFormat = NvColorFormat_V8;
    pSurf[0].Layout = NvRmSurfaceLayout_Pitch;
    PixelFormat = NvxFOURCCFormat_YV12;

    // chip specific ones
#ifdef CAMERA_T124
    if (StreamIndex == NvMMDigitalZoomStreamIndex_OutputStill)
    {
        PixelFormat = NvxFOURCCFormat_NV12;
        pVideoFormat->NumberOfSurfaces = 2;
        pSurf[1].ColorFormat = NvColorFormat_U8_V8;
        pSurf[0].Layout = NvRmSurfaceLayout_Blocklinear;
    }
#elif defined CAMERA_T114 || defined CAMERA_T148
    if (StreamIndex == NvMMDigitalZoomStreamIndex_OutputVideo)
        pSurf[0].Layout = NvRmSurfaceLayout_Tiled;
#endif

    // special attributes for different layout format in surface[0]
    if (pSurf[0].Layout == NvRmSurfaceLayout_Blocklinear)
    {
        pSurf[0].Kind = NvRmMemKind_Generic_16Bx2;
        pSurf[0].BlockHeightLog2 = 2;
    }

    //update the secondary fields
    NvU32 i;
    for (i = 1; i < pVideoFormat->NumberOfSurfaces; i++)
    {
        if (PixelFormat == NvxFOURCCFormat_YV12 ||
            PixelFormat == NvxFOURCCFormat_NV12)
        {
            pSurf[i].Layout = pSurf[0].Layout;
        }

        if (pSurf[0].Layout == NvRmSurfaceLayout_Blocklinear)
        {
            pSurf[i].Kind = pSurf[0].Kind;
            pSurf[i].BlockHeightLog2 = pSurf[0].BlockHeightLog2;
        }
    }
    return e;
}

static NvError FillOutBufferRequirement(NvMMNewBufferRequirementsInfo *pBufReq, NvBool dz, NvU32 StreamIndex, SNvxNvMMTransformData *oBase)
{
    NvRmSurface *pSurf = &pBufReq->format.videoFormat.SurfaceDescription[NvMMPlanar_Single];
    //NvU32 OutIndex = 0;

    NvOsMemset(pBufReq, 0, sizeof(NvMMNewBufferRequirementsInfo));

    if (dz)
    {
        PopulateDZSurfaceFormatInfo(pBufReq, StreamIndex);

        switch(StreamIndex)
        {
            // the width and height of nvmm video enc block's input buffers
            // have to be 16-byte-aligned. Ideally this restriction should
            // be queried from nvmm video enc block.
        case NvMMDigitalZoomStreamIndex_OutputVideo:
        case NvMMDigitalZoomStreamIndex_OutputStill:
            pBufReq->format.videoFormat.SurfaceRestriction =
                NVMM_SURFACE_RESTRICTION_WIDTH_16BYTE_ALIGNED |
                NVMM_SURFACE_RESTRICTION_HEIGHT_16BYTE_ALIGNED |
                NVMM_SURFACE_RESTRICTION_NEED_TO_CROP;

        case NvMMDigitalZoomStreamIndex_OutputPreview:
        case NvMMDigitalZoomStreamIndex_OutputThumbnail:
                // TO DO : hard code parameters for now.
                if (oBase->oPorts[StreamIndex].bHasSize)
                {
                    pSurf->Width = oBase->oPorts[StreamIndex].nWidth;
                    pSurf->Height = oBase->oPorts[StreamIndex].nHeight;
                }
                else
                {
                    pSurf->Width = CAMERA_DEFAULT_OUTPUT_WIDTH;
                    pSurf->Height = CAMERA_DEFAULT_OUTPUT_HEIGHT;
                }
                break;

        default:
            NV_ASSERT(!"Invalid streamIndex\n");
            return NvError_BadParameter;
        }

        if(StreamIndex == NvMMDigitalZoomStreamIndex_OutputStill)
        {
            pBufReq->minBuffers = CAMERA_MAX_OUTPUT_BUFFERS_STILL;
            pBufReq->maxBuffers = CAMERA_MAX_OUTPUT_BUFFERS_STILL;
        }
        else if (StreamIndex == NvMMDigitalZoomStreamIndex_OutputVideo)
        {
            pBufReq->minBuffers = CAMERA_MAX_OUTPUT_BUFFERS_VIDEO;
            pBufReq->maxBuffers = CAMERA_MAX_OUTPUT_BUFFERS_VIDEO;
        }
        else
        {
            pBufReq->minBuffers = CAMERA_MAX_OUTPUT_BUFFERS_PREVIEW;
            pBufReq->maxBuffers = CAMERA_MAX_OUTPUT_BUFFERS_PREVIEW;
        }

#ifdef NV_DEF_USE_PITCH_MODE
        pSurf->Layout = NvRmSurfaceLayout_Pitch;
#endif
        if (StreamIndex == NvMMDigitalZoomStreamIndex_OutputVideo)
        {
            pSurf->Layout = oBase->oPorts[s_nvx_port_dz_output_video].SurfaceLayout;
        }

        /* Fill the buffer properties to send to block */
        pBufReq->format.videoFormat.SurfaceRestriction |= NVMM_SURFACE_RESTRICTION_YPITCH_EQ_2X_UVPITCH;
    }
    else
    {
        switch(StreamIndex)
        {
        case NvMMCameraStreamIndex_Input:
            if (oBase->oPorts[StreamIndex].bHasSize)
            {
                pSurf->Width = oBase->oPorts[StreamIndex].nWidth;
                pSurf->Height = oBase->oPorts[StreamIndex].nHeight;
            }
            else
            {
                pSurf->Width = CAMERA_DEFAULT_INPUT_WIDTH;
                pSurf->Height = CAMERA_DEFAULT_INPUT_HEIGHT;
            }
            pSurf->ColorFormat = NvColorFormat_X6Bayer10RGGB;   // Really "any X6Bayer10 format"
            pSurf->Layout = NvRmSurfaceLayout_Pitch;
            pBufReq->format.videoFormat.NumberOfSurfaces = 1;

            pBufReq->minBuffers = CAMERA_MAX_INPUT_BUFFERS;
            pBufReq->maxBuffers = CAMERA_MAX_INPUT_BUFFERS;
            break;

        default:
            NV_ASSERT(!"Invalid streamIndex\n");
            return NvError_BadParameter;

        }
    }

    pBufReq->bPhysicalContiguousMemory = NV_FALSE;
    pBufReq->byteAlignment = 0;
    pBufReq->endianness = NvMMBufferEndianess_LE;
    pBufReq->minBufferSize = sizeof(NvMMSurfaceDescriptor);
    pBufReq->maxBufferSize = sizeof(NvMMSurfaceDescriptor);
    pBufReq->memorySpace = NvMMMemoryType_SYSTEM;
    pBufReq->structSize = sizeof(NvMMNewBufferRequirementsInfo);
    pBufReq->event = NvMMEvent_NewBufferRequirements;

    return NvSuccess;
}

#if USE_ANDROID_NATIVE_WINDOW
static NvError
NvxCameraSuggestBufferConfig(SNvxNvMMTransformData *oBase, NvU32 StreamIndex, NvMMNewBufferRequirementsInfo * pReq)
{
    NvError RetType = NvSuccess;
    NvMMNewBufferConfigurationInfo pBufCfg;
    NvRmSurface *pSurfaceCfg = NULL;
    NvU32 MaxWidth = 0, MaxHeight = 0;
    NvBool CheckSize = NV_FALSE;
    NvU32 i;
    NvMMBuffer **ppBuf;

    NvOsMemset(&pBufCfg, 0, sizeof(NvMMNewBufferConfigurationInfo));

    pBufCfg.memorySpace = pReq->memorySpace;
    pBufCfg.numBuffers = pReq->minBuffers;
    pBufCfg.byteAlignment = pReq->byteAlignment;
    pBufCfg.endianness = pReq->endianness;
    pBufCfg.bufferSize = pReq->minBufferSize;
    pBufCfg.bPhysicalContiguousMemory = pReq->bPhysicalContiguousMemory;
    pBufCfg.event = NvMMEvent_NewBufferConfiguration;
    pBufCfg.structSize = sizeof(NvMMNewBufferConfigurationInfo);

    /* JPEG needs this while negotiating */
    pBufCfg.bInSharedMemory = pReq->bInSharedMemory;

    pSurfaceCfg = &pBufCfg.format.videoFormat.SurfaceDescription[0];

    switch(StreamIndex)
    {
        case NvMMDigitalZoomStreamIndex_OutputPreview:
            MaxWidth = CAMERA_MAX_PREVIEW_WIDTH;
            MaxHeight = CAMERA_MAX_PREVIEW_HEIGHT;
            break;

        default:
            NV_ASSERT(!"CTEST: Unknown Stream Index\n");
            RetType = NvError_NotSupported;
            goto finish;
    }

    switch(StreamIndex)
    {
       case NvMMDigitalZoomStreamIndex_OutputPreview:

           // NV_ASSERT(pStream->RequirementsReceived);

            // accept client's requirement
            pBufCfg.format = pReq->format;

            // need to check if we support the size later
            CheckSize = NV_TRUE;

            break;

        default:
            NV_ASSERT(!"CTEST: KnownStream Index");
            RetType = NvError_BadParameter;
            goto finish;
    }

    // Make sure the the size is not larger than max size
    // TODO: keep aspect ratio
    if (CheckSize)
    {
        if (pSurfaceCfg->Width == 0 || pSurfaceCfg->Width > MaxWidth)
        {
            pSurfaceCfg->Width = MaxWidth;
        }

        if (pSurfaceCfg->Height == 0 || pSurfaceCfg->Height > MaxHeight)
        {
            pSurfaceCfg->Height = MaxHeight;
        }

        // EPP doesn't support odd width and height dimensions
        // take care of such cases by making the width and height even(incrementing by one)
        // This applies only for YUV420 surfaces
        if(pSurfaceCfg->ColorFormat == NvColorFormat_Y8)
        {
            if(pSurfaceCfg->Width & 0x01)
            {
                pSurfaceCfg->Width +=1;
                if(pSurfaceCfg->Width > MaxWidth)
                {
                   pSurfaceCfg->Width = MaxWidth;
                }
            }
            if(pSurfaceCfg->Height & 0x01)
            {
                pSurfaceCfg->Height +=1;
                if(pSurfaceCfg->Height > MaxHeight)
                {
                   pSurfaceCfg->Height = MaxHeight;
                }
            }
        }
    }

    if (pBufCfg.format.videoFormat.SurfaceRestriction &
        NVMM_SURFACE_RESTRICTION_WIDTH_16BYTE_ALIGNED)
    {
        pSurfaceCfg->Width = (pSurfaceCfg->Width + 15) & ~15;
    }

    if (pBufCfg.format.videoFormat.SurfaceRestriction &
        NVMM_SURFACE_RESTRICTION_HEIGHT_16BYTE_ALIGNED)
    {
        pSurfaceCfg->Height = (pSurfaceCfg->Height + 15) & ~15;
    }

    RetType = oBase->TransferBufferToBlock(oBase->hBlock,
                           StreamIndex,
                           NvMMBufferType_StreamEvent,
                           sizeof(NvMMNewBufferConfigurationInfo), &pBufCfg);

    NvOsSemaphoreWait(oBase->oPorts[StreamIndex].buffConfigSem);
    NV_ASSERT(RetType == NvSuccess);

finish:

    return RetType;
}
#endif

NvError
CameraNegotiateStreamBuffers(
    SNvxNvMMTransformData *oBase,
    NvBool dz,
    NvU32 const * StreamIndexes,
    NvU32 numStreams)
{
    NvMMNewBufferConfigurationReplyInfo BufConfigReply;
    NvMMNewBufferRequirementsInfo BufReq;
    NvError Status = NvSuccess;
    NvU32 StreamIndex;
    NvU32 i;

    // send all of the requirements down at once
    for (i = 0; i < numStreams; i++)
    {
        StreamIndex = StreamIndexes[i];

#if USE_ANDROID_NATIVE_WINDOW
        if((dz == NV_TRUE) && (StreamIndex == s_nvx_port_dz_output_preview) && (oBase->oPorts[s_nvx_port_dz_output_preview].bUsesANBs))
        {
            Status = oBase->hBlock->SetBufferAllocator(oBase->hBlock, StreamIndex, NV_FALSE);
            NV_ASSERT(Status == NvSuccess);
        }
        else
#endif
        {
            // Tell block it is the allocator
            Status = oBase->hBlock->SetBufferAllocator(oBase->hBlock, StreamIndex, NV_TRUE);
            NV_ASSERT(Status == NvSuccess);
        }

        // Fill BR
        Status = FillOutBufferRequirement(&BufReq, dz, StreamIndex, oBase);
        NV_ASSERT(Status == NvSuccess);

        // Send BR to block
        Status = oBase->TransferBufferToBlock(oBase->hBlock, StreamIndex,
            NvMMBufferType_StreamEvent, sizeof(NvMMNewBufferRequirementsInfo),
            &BufReq);
        NV_ASSERT(Status == NvSuccess);
    }

    // now that all reqs are sent, wait for them all to finish
    for (i = 0; i < numStreams; i++)
    {
        StreamIndex = StreamIndexes[i];
#if USE_ANDROID_NATIVE_WINDOW
        if((dz == NV_TRUE) && (StreamIndex == s_nvx_port_dz_output_preview) && (oBase->oPorts[s_nvx_port_dz_output_preview].bUsesANBs))
        {
            NvxCameraSuggestBufferConfig(oBase,StreamIndex,&BufReq);
        }
        else
#endif
        {
            // Wait for block's BC
            NvOsSemaphoreWait(oBase->oPorts[StreamIndex].buffConfigSem);

            // TODO: what if we want to reject
            BufConfigReply.event = NvMMEvent_NewBufferConfigurationReply;
            BufConfigReply.structSize = sizeof(BufConfigReply);
            BufConfigReply.bAccepted = NV_TRUE;
            Status = oBase->TransferBufferToBlock(oBase->hBlock, StreamIndex,
                            NvMMBufferType_StreamEvent, sizeof(BufConfigReply),
                            &BufConfigReply);
        }
    }
    return Status;
}

static void nvxCameraEventCallback(
    NvxComponent *pComp,
    NvU32 eventType, NvU32 eventSize, void *eventInfo)
{
    NvMMEventBlockBufferNegotiationCompleteInfo *pInfo =
        (NvMMEventBlockBufferNegotiationCompleteInfo*)eventInfo;
    SNvxCameraData *pCam = (SNvxCameraData *)(void *)pComp;
    SNvxNvMMTransformData *pData = NULL;
    NvU32 index = 0;

    if (pComp == NULL || eventInfo == NULL)
        return;

    switch (eventType)
    {
        case NvMMEventCamera_StreamBufferNegotiationComplete:
        case NvMMEvent_BlockBufferNegotiationComplete:
            pData = &pCam->oSrcCamera;
            index = pInfo->streamIndex;
            if (index < pData->nNumStreams)
            {
                pData->oPorts[index].bBufferNegotiationDone = OMX_TRUE;
                if (pCam->oStreamBufNegotiationSem != NULL)
                {
                    NvOsSemaphoreSignal(pCam->oStreamBufNegotiationSem);
                }
            }
            break;

        case NvMMDigitalZoomEvents_StreamBufferNegotiationComplete:
                pData = &pCam->oDigitalZoom;
                index = pInfo->streamIndex;
                if (index < pData->nNumStreams)
                {
                    pData->oPorts[index].bBufferNegotiationDone = OMX_TRUE;
                    if (pCam->oStreamBufNegotiationSem != NULL)
                    {
                        NvOsSemaphoreSignal(pCam->oStreamBufNegotiationSem);
                    }
                }
                break;

        default:
                break;

    }
}

//Define a FOURCC code.
#define MY_FOURCC(a,b,c,d)      ((a)<<24 | (b)<<16 | (c)<<8 | (d))

static void NvxCameraInputSurfaceHook(OMX_BUFFERHEADERTYPE *pSrcSurf, NvMMSurfaceDescriptor *pDestSurf)
{
    NvRmSurface* pSurf = &pDestSurf->Surfaces[0];

    NvxBufferPlatformPrivate *pPlatPriv = (NvxBufferPlatformPrivate *)pSrcSurf->pPlatformPrivate;
    if (pPlatPriv->rawHeaderOffset > 0)
    {
        CameraRawDump *hdr = (CameraRawDump *)(pSrcSurf->pBuffer + pPlatPriv->rawHeaderOffset);
        switch (hdr->BayerOrdering)
        {
        case MY_FOURCC('B','G','G','R'): pSurf->ColorFormat = NvColorFormat_X6Bayer10BGGR; break;
        case MY_FOURCC('G','R','B','G'): pSurf->ColorFormat = NvColorFormat_X6Bayer10GRBG; break;
        case MY_FOURCC('G','B','R','G'): pSurf->ColorFormat = NvColorFormat_X6Bayer10GBRG; break;
        case MY_FOURCC('R','G','G','B'):
        default:                         pSurf->ColorFormat = NvColorFormat_X6Bayer10RGGB; break;
        }
    }
    else
    {
        // No raw header info - let's take a guess
        pSurf->ColorFormat = NvColorFormat_X6Bayer10RGGB;
    }
}

static OMX_ERRORTYPE NvxCameraOpen(SNvxCameraData *pNvxCamera, NvxComponent *pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxPort *pPortStillCapture = &(pComponent->pPorts[s_nvx_port_still_capture]);
    NvxPort *pPortVideoCapture = &(pComponent->pPorts[s_nvx_port_video_capture]);
    NvxPort *pPortThumbnail = &(pComponent->pPorts[s_nvx_port_thumbnail]);
    NvxPort *pPortPreview = &(pComponent->pPorts[s_nvx_port_preview]);
    NvxPort *pPortInput = &(pComponent->pPorts[s_nvx_port_raw_input]);
    NvBool bProfile;
    OMX_BOOL bTunnelStillCapture = OMX_FALSE;
    OMX_BOOL bTunnelVideoCapture = OMX_FALSE;
    OMX_BOOL bTunnelThumbnail = OMX_FALSE;
    nvxCameraPortSet portSet;

    NVXTRACE(( NVXT_CALLGRAPH, NVXT_CAMERA, "+NvxCameraOpen\n"));

    eError = NvxCameraGetTransformData(pNvxCamera, pComponent);
    if (eError != OMX_ErrorNone)
        return eError;

    pNvxCamera->oDigitalZoom.bSequence = OMX_TRUE;

    pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_preview].bHasSize = OMX_TRUE;
    pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_preview].nWidth = pNvxCamera->nSurfWidthPreview;
    pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_preview].nHeight = pNvxCamera->nSurfHeightPreview;
    pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_preview].bRequestSurfaceBuffers = OMX_TRUE;
    pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_preview].SurfaceLayout =  NvRmSurfaceLayout_Pitch;

    pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_still].bHasSize = OMX_TRUE;
    pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_still].nWidth = pNvxCamera->nSurfWidthStillCapture;
    pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_still].nHeight = pNvxCamera->nSurfHeightStillCapture;
    pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_still].bRequestSurfaceBuffers = OMX_TRUE;
    pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_still].SurfaceLayout =  NvRmSurfaceLayout_Pitch;

    pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_video].bHasSize = OMX_TRUE;
    pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_video].nWidth = pNvxCamera->nSurfWidthVideoCapture;
    pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_video].nHeight = pNvxCamera->nSurfHeightVideoCapture;
    pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_video].bRequestSurfaceBuffers = OMX_TRUE;
    pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_video].SurfaceLayout =   pNvxCamera->VideoSurfLayout;

    pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_thumbnail].bHasSize = OMX_TRUE;
    pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_thumbnail].nWidth = pNvxCamera->nSurfWidthThumbnail;
    pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_thumbnail].nHeight = pNvxCamera->nSurfHeightThumbnail;
    pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_thumbnail].bRequestSurfaceBuffers = OMX_TRUE;

    pNvxCamera->oSrcCamera.oPorts[pNvxCamera->s_nvx_port_sc_input].bHasSize = OMX_TRUE;
    pNvxCamera->oSrcCamera.oPorts[pNvxCamera->s_nvx_port_sc_input].nWidth = pNvxCamera->nSurfWidthInput;
    pNvxCamera->oSrcCamera.oPorts[pNvxCamera->s_nvx_port_sc_input].nHeight = pNvxCamera->nSurfHeightInput;

    bTunnelStillCapture = (pPortStillCapture->bNvidiaTunneling) &&
                     (pPortStillCapture->eNvidiaTunnelTransaction == ENVX_TRANSACTION_NVMM_TUNNEL);
    bTunnelVideoCapture = (pPortVideoCapture->bNvidiaTunneling) &&
                     (pPortVideoCapture->eNvidiaTunnelTransaction == ENVX_TRANSACTION_NVMM_TUNNEL);
    bTunnelThumbnail = (pPortThumbnail->bNvidiaTunneling) &&
                       (pPortThumbnail->eNvidiaTunnelTransaction == ENVX_TRANSACTION_NVMM_TUNNEL);

    nvxCameraPortSetInit(&portSet);

    {
        NvMMCameraPin EnabledPin = 0;
        NvMMDigitalZoomPin EnabledDZPin = 0;

        // enable NvMM camera and dz src block preview pin if we need preview
        // stream from the component
        if (pPortPreview->oPortDef.bEnabled)
        {
            EnabledPin |= NvMMCameraPin_Preview;

            nvxCameraPortSetAdd(&portSet,
                &pNvxCamera->oSrcCamera.oPorts[pNvxCamera->s_nvx_port_sc_output_preview]);
            nvxCameraPortSetAdd(&portSet,
                &pNvxCamera->oDigitalZoom.oPorts[NvMMDigitalZoomStreamIndex_InputPreview]);
        }

        // enable NvMM camera src block capture pin if we need either still or
        // video output ports from this component
        if (pPortStillCapture->oPortDef.bEnabled ||
            pPortVideoCapture->oPortDef.bEnabled)
        {
            EnabledPin |= NvMMCameraPin_Capture;

            nvxCameraPortSetAdd(&portSet,
                &pNvxCamera->oSrcCamera.oPorts[pNvxCamera->s_nvx_port_sc_output_capture]);
            nvxCameraPortSetAdd(&portSet,
                &pNvxCamera->oDigitalZoom.oPorts[NvMMDigitalZoomStreamIndex_Input]);
        }

        pNvxCamera->oSrcCamera.hBlock->SetAttribute(
            pNvxCamera->oSrcCamera.hBlock,
            NvMMCameraAttribute_PinEnable,
            0,
            sizeof(NvMMCameraPin),
            &EnabledPin);

        if (pNvxCamera->PreviewEnabled)
        {
            EnabledDZPin |= NvMMDigitalZoomPin_Preview;
        }
        if (pPortStillCapture->oPortDef.bEnabled)
        {
            EnabledDZPin |= NvMMDigitalZoomPin_Still;
        }
        if (pPortVideoCapture->oPortDef.bEnabled)
        {
            EnabledDZPin |= NvMMDigitalZoomPin_Video;
        }
        if (pNvxCamera->ThumbnailEnabled)
        {
            EnabledDZPin |= NvMMDigitalZoomPin_Thumbnail;
        }

        pNvxCamera->oDigitalZoom.hBlock->SetAttribute(
            pNvxCamera->oDigitalZoom.hBlock,
            NvMMDigitalZoomAttributeType_PinEnable,
            0,
            sizeof(EnabledDZPin),
            &EnabledDZPin);
    }

    // set up preview stream
    if (pPortPreview->oPortDef.bEnabled)
    {
        eError = NvxNvMMTransformTunnelBlocks(
            &pNvxCamera->oSrcCamera,
            pNvxCamera->s_nvx_port_sc_output_preview,
            &pNvxCamera->oDigitalZoom,
            s_nvx_port_dz_input_preview);

        if (eError != OMX_ErrorNone)
            return eError;

        pNvxCamera->oSrcCamera.oPorts[pNvxCamera->s_nvx_port_sc_output_preview].nType = TF_TYPE_OUTPUT_TUNNELED;

        pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_input_preview].nType = TF_TYPE_INPUT_TUNNELED;

        eError = NvxNvMMTransformSetupOutput(
            &pNvxCamera->oDigitalZoom,
            s_nvx_port_dz_output_preview,
            &pComponent->pPorts[s_nvx_port_preview],
            s_nvx_port_dz_input_preview,
            CAMERA_MAX_OUTPUT_BUFFERS_PREVIEW,
            CAMERA_MIN_OUTPUT_BUFFERSIZE_PREVIEW);

        if (eError != OMX_ErrorNone)
        {
            return eError;
        }

        {
            OMX_BOOL bNvidiaTunneled =
                (pPortPreview->bNvidiaTunneling) &&
                (pPortPreview->eNvidiaTunnelTransaction == ENVX_TRANSACTION_GFSURFACES);

            NvxNvMMTransformSetStreamUsesSurfaces(
                &pNvxCamera->oDigitalZoom,
                s_nvx_port_dz_output_preview,
                bNvidiaTunneled);

            CameraNegotiateStreamBuffers(
                &pNvxCamera->oDigitalZoom,
                NV_TRUE,
                &s_nvx_port_dz_output_preview,
                1);
        }
    }

    if (pPortInput->oPortDef.bEnabled)
    {
        eError = NvxCameraEnableInputPort(pComponent, pNvxCamera);
        if (eError != OMX_ErrorNone)
        {
            return eError;
        }
    }

    // set up still and video streams
    if (pPortStillCapture->oPortDef.bEnabled || pPortVideoCapture->oPortDef.bEnabled)
    {
        eError = NvxNvMMTransformTunnelBlocks(
            &pNvxCamera->oSrcCamera,
            pNvxCamera->s_nvx_port_sc_output_capture,
            &pNvxCamera->oDigitalZoom,
            s_nvx_port_dz_input_capture);
        if (eError != OMX_ErrorNone)
            return eError;

        if (pPortStillCapture->oPortDef.bEnabled)
        {
            eError = NvxNvMMTransformSetupOutput(
                &pNvxCamera->oDigitalZoom,
                s_nvx_port_dz_output_still,
                &pComponent->pPorts[s_nvx_port_still_capture],
                s_nvx_port_dz_input_capture,
                CAMERA_MAX_OUTPUT_BUFFERS_STILL,
                CAMERA_MIN_OUTPUT_BUFFERSIZE_CAPTURE);
            if (eError != OMX_ErrorNone)
                return eError;

            if (!bTunnelStillCapture)
            {
                OMX_BOOL bNvidiaTunneled =
                    (pPortStillCapture->bNvidiaTunneling) &&
                    (pPortStillCapture->eNvidiaTunnelTransaction == ENVX_TRANSACTION_GFSURFACES);

                NvxNvMMTransformSetStreamUsesSurfaces(
                    &pNvxCamera->oDigitalZoom,
                    s_nvx_port_dz_output_still,
                    bNvidiaTunneled);

                CameraNegotiateStreamBuffers(
                    &pNvxCamera->oDigitalZoom,
                    NV_TRUE,
                    &s_nvx_port_dz_output_still,
                    1);
            }
        }

        if (pPortVideoCapture->oPortDef.bEnabled)
        {
            eError = NvxNvMMTransformSetupOutput(
                &pNvxCamera->oDigitalZoom,
                s_nvx_port_dz_output_video,
                &pComponent->pPorts[s_nvx_port_video_capture],
                s_nvx_port_dz_input_capture,
                CAMERA_MAX_OUTPUT_BUFFERS_VIDEO,
                CAMERA_MIN_OUTPUT_BUFFERSIZE_CAPTURE);
            if (eError != OMX_ErrorNone)
                return eError;

            if (!bTunnelVideoCapture)
            {
                OMX_BOOL bNvidiaTunneled =
                    (pPortVideoCapture->bNvidiaTunneling) &&
                    (pPortVideoCapture->eNvidiaTunnelTransaction == ENVX_TRANSACTION_GFSURFACES);

                NvxNvMMTransformSetStreamUsesSurfaces(
                    &pNvxCamera->oDigitalZoom,
                    s_nvx_port_dz_output_video,
                    bNvidiaTunneled);

                CameraNegotiateStreamBuffers(
                    &pNvxCamera->oDigitalZoom,
                    NV_TRUE,
                    &s_nvx_port_dz_output_video,
                    1);
            }
        }

        // only set up thumbnail stream if we have a still/vid stream active
        if (pPortThumbnail->oPortDef.bEnabled)
        {
            NvBool Thumbnail = NV_TRUE;
            pNvxCamera->oSrcCamera.hBlock->SetAttribute(
                pNvxCamera->oSrcCamera.hBlock,
                NvMMCameraAttribute_Thumbnail,
                0,
                sizeof(NvBool),
                &Thumbnail);

            eError = NvxNvMMTransformSetupOutput(
                &pNvxCamera->oDigitalZoom,
                s_nvx_port_dz_output_thumbnail,
                &pComponent->pPorts[s_nvx_port_thumbnail],
                s_nvx_port_dz_input_capture,
                CAMERA_MAX_OUTPUT_BUFFERS_THUMBNAIL,
                CAMERA_MIN_OUTPUT_BUFFERSIZE_CAPTURE);
            if (eError != OMX_ErrorNone)
                return eError;

            if (!bTunnelThumbnail)
            {
                OMX_BOOL bNvidiaTunneled =
                    (pPortThumbnail->bNvidiaTunneling) &&
                    (pPortThumbnail->eNvidiaTunnelTransaction == ENVX_TRANSACTION_GFSURFACES);

                NvxNvMMTransformSetStreamUsesSurfaces(
                    &pNvxCamera->oDigitalZoom,
                    s_nvx_port_dz_output_thumbnail,
                    bNvidiaTunneled);

                CameraNegotiateStreamBuffers(
                    &pNvxCamera->oDigitalZoom,
                    NV_TRUE,
                    &s_nvx_port_dz_output_thumbnail,
                    1);

            }
        }
    }

    NvxUnlockAcqLock();
    // Now wait for all internal Camera <==> DZ buffer negotiation to finish.
    // Wait on DZ <==> OMX Camera buffer negotiation is done already
    // when executing CameraNegotiateStreamBuffers()
    eError = nvxCameraWaitBufferNegotiationComplete(
        pNvxCamera,
        &portSet,
        NVXCAMERA_NEGOTIATE_BUFFERS_TIMEOUT);
    NvxLockAcqLock();

    if (eError != OMX_ErrorNone)
    {
        return eError;
    }

    pNvxCamera->bInitialized = OMX_TRUE;

    bProfile = (NvBool)pNvxCamera->oDigitalZoom.bProfile;
    pNvxCamera->oDigitalZoom.hBlock->SetAttribute(pNvxCamera->oDigitalZoom.hBlock,
                                                  NvMMDigitalZoomAttributeType_Profiling, 0,
                                                  sizeof(NvBool), &bProfile);

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxCameraClose(SNvxCameraData *pNvxCamera)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    int nStream;
    SNvxNvMMTransformData *pData;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_CAMERA, "+NvxCameraClose\n"));

    if (pNvxCamera->bInitialized)
    {
        pData = &pNvxCamera->oDigitalZoom;
        for (nStream = 0; nStream < s_nvx_num_ports_dz; nStream++)
        {
            SNvxNvMMPort *pPort = &(pData->oPorts[nStream]);
            int i;

            if (pPort->bNvMMClientAlloc || pPort->nType != TF_TYPE_OUTPUT)
                continue;

            for (i = 0; i < MAX_NVMM_BUFFERS; i++)
            {
                if (pPort->pOMXBufMap[i])
                {
                    NvMMBuffer *SendBackBuf = pPort->pBuffers[i];
                    pPort->pOMXBufMap[i] = NULL;
                    pData->TransferBufferToBlock(pData->hBlock, nStream,
                                                 NvMMBufferType_Payload,
                                                 sizeof(NvMMBuffer), SendBackBuf);
                }
            }
        }
    }

    if (pNvxCamera->bTransformDataInitialized)
    {
        eError = NvxNvMMTransformClose(&pNvxCamera->oDigitalZoom);
        if (eError != OMX_ErrorNone) {
            return eError;
        }

        eError = NvxNvMMTransformClose(&pNvxCamera->oSrcCamera);
        if (eError != OMX_ErrorNone) {
            return eError;
        }

        while (!pNvxCamera->oDigitalZoom.blockCloseDone) ;
        while (!pNvxCamera->oSrcCamera.blockCloseDone);
    }

    pNvxCamera->bInitialized = OMX_FALSE;
    pNvxCamera->bTransformDataInitialized = OMX_FALSE;

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxCameraFillThisBuffer(NvxComponent *pNvComp, OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    SNvxCameraData *pNvxCamera = 0;
    int nPortIndex;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxCameraFillThisBuffer\n"));

    pNvxCamera = (SNvxCameraData *)pNvComp->pComponentData;
    if (!pNvxCamera->bInitialized)
    {
        return OMX_ErrorNone;
    }

    if (!pBufferHdr)
    {
        return OMX_ErrorNone;
    }

    if ((int)pBufferHdr->nOutputPortIndex == s_nvx_port_preview)
    {
        nPortIndex = s_nvx_port_dz_output_preview;
    }
    else if ((int)pBufferHdr->nOutputPortIndex == s_nvx_port_still_capture)
    {
        nPortIndex = s_nvx_port_dz_output_still;
    }
    else if ((int)pBufferHdr->nOutputPortIndex == s_nvx_port_video_capture)
    {
        nPortIndex = s_nvx_port_dz_output_video;
    }
    else
    {
        return OMX_ErrorNone;
    }

    return NvxNvMMTransformFillThisBuffer(&pNvxCamera->oDigitalZoom, pBufferHdr, nPortIndex);
}

#if USE_ANDROID_NATIVE_WINDOW
static OMX_ERRORTYPE NvxCameraFreeBuffer(NvxComponent *pNvComp, OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    SNvxCameraData *pNvxCamera = 0;
    int nPortIndex;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxCameraFreeBuffer\n"));

    pNvxCamera = (SNvxCameraData *)pNvComp->pComponentData;
    if (!pNvxCamera->bInitialized)
        return OMX_ErrorNone;

    if (!pBufferHdr)
        return OMX_ErrorNone;

    if (((int)pBufferHdr->nOutputPortIndex == s_nvx_port_preview) && (pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_preview].bUsesANBs))
    {
        nPortIndex = s_nvx_port_dz_output_preview;
        NV_DEBUG_PRINTF(("CameraComponent : Freeing Preview Buffers"));
    }
    else
        return OMX_ErrorNone;

    return NvxNvMMTransformFreeBuffer(&pNvxCamera->oDigitalZoom, pBufferHdr, nPortIndex);
}

static OMX_ERRORTYPE NvxCameraFlush(OMX_IN NvxComponent *pComponent, OMX_U32 nPort)
{
    SNvxCameraData *pNvxCamera = 0;
    int nPortIndex;
    OMX_ERRORTYPE status = OMX_ErrorNone;

    NV_ASSERT(pComponent && pComponent->pComponentData);

    NVXTRACE((NVXT_CALLGRAPH, pComponent->eObjectType, "+NvxCameraFlush\n"));
    pNvxCamera = (SNvxCameraData *)pComponent->pComponentData;

    if (((int)nPort == s_nvx_port_preview) && (pNvxCamera->oDigitalZoom.oPorts[s_nvx_port_dz_output_preview].bUsesANBs))
    {
        nPortIndex = s_nvx_port_dz_output_preview;
        NV_DEBUG_PRINTF(("CameraComponent : Flushing Preview Buffers"));
    }
    else
        return OMX_ErrorNone;

    status =  NvxNvMMTransformFlush(&pNvxCamera->oDigitalZoom, nPortIndex);

    return status;
}
#endif

static OMX_ERRORTYPE NvxCameraEmptyThisBuffer(NvxComponent *pNvComp, OMX_BUFFERHEADERTYPE* pBufferHdr, OMX_BOOL *bHandled)
{
    SNvxCameraData *pNvxCamera = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxBufferPlatformPrivate *pPlatPriv;

    NV_ASSERT(pNvComp);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxCameraEmptyThisBuffer\n"));

    pNvxCamera = (SNvxCameraData *)pNvComp->pComponentData;
    if (!pNvxCamera->bInitialized)
        return OMX_ErrorNone;

    if (!pBufferHdr)
        return OMX_ErrorNone;

    if ((int)pBufferHdr->nInputPortIndex != s_nvx_port_raw_input)
        return OMX_ErrorNone;

    pPlatPriv = (NvxBufferPlatformPrivate *)pBufferHdr->pPlatformPrivate;
    pPlatPriv->rawHeaderOffset = 0;

    if (pBufferHdr->nFlags & OMX_BUFFERFLAG_EXTRADATA) {
        // Extra data is assumed to be the NVRAW header.
        // If it's present, we extract a few interesting settings (such as exposure and gain)
        // and send them to the camera block before sending the image.
        // Note lots of error-checking here; we must guard against misbehaving clients.
        // If the extra data doesn't pass muster, we simply ignore it.
        // HACK: should we define (and check for) a new OMX_EXTRADATATYPE?

        OMX_OTHER_EXTRADATATYPE* pExtraData = (OMX_OTHER_EXTRADATATYPE*)(pBufferHdr->pBuffer + pBufferHdr->nFilledLen);
        int remainingBytes = pBufferHdr->nAllocLen - pBufferHdr->nFilledLen;

        if (remainingBytes < sizeof(*pExtraData)) {
            NVXTRACE((NVXT_WARNING, pNvComp->eObjectType, "WARNING:NvxCameraEmptyThisBuffer: "
               "Buffer too small (0x%X) to contain EXTRADATA header [%s(%d)]\n", pBufferHdr->nAllocLen, __FILE__, __LINE__));
        }
        else if (pExtraData->nSize != sizeof(*pExtraData)) {
            NVXTRACE((NVXT_WARNING, pNvComp->eObjectType, "WARNING:NvxCameraEmptyThisBuffer: "
               "EXTRADATA header size (0x%X) should be 0x%X [%s(%d)]\n", pExtraData->nSize, sizeof(*pExtraData), __FILE__, __LINE__));
        }
        else if (pExtraData->nDataSize != sizeof(CameraRawDump)) {
            NVXTRACE((NVXT_WARNING, pNvComp->eObjectType, "WARNING:NvxCameraEmptyThisBuffer: "
               "EXTRADATA size (0x%X) should be 0x%X [%s(%d)]\n", pExtraData->nDataSize, sizeof(CameraRawDump), __FILE__, __LINE__));
        }
        else if (remainingBytes < sizeof(*pExtraData) + sizeof(CameraRawDump)) {
            NVXTRACE((NVXT_WARNING, pNvComp->eObjectType, "WARNING:NvxCameraEmptyThisBuffer: "
               "Buffer too small (0x%X) to contain EXTRADATA [%s(%d)]\n", pBufferHdr->nAllocLen, __FILE__, __LINE__));
        }
        else {
            CameraRawDump* pHeader = (CameraRawDump*)&pExtraData->data;
            pPlatPriv->rawHeaderOffset = (OMX_U8 *)pHeader - pBufferHdr->pBuffer;
        }
    }

    if (NvxNvMMTransformIsInputSkippingWorker(&pNvxCamera->oSrcCamera, pBufferHdr->nInputPortIndex))
    {
        eError = NvxPortEmptyThisBuffer(&pNvComp->pPorts[pBufferHdr->nInputPortIndex], pBufferHdr);
        *bHandled = (eError == OMX_ErrorNone);
    }
    else
    {
        *bHandled = OMX_FALSE;
    }

    return OMX_ErrorNone;
}


static OMX_ERRORTYPE NvxCameraPreChangeState(NvxComponent *pNvComp,
                                             OMX_STATETYPE oNewState)
{
    OMX_ERRORTYPE err = OMX_ErrorNone;
    SNvxCameraData *pNvxCamera = 0;

    NVXTRACE((NVXT_CALLGRAPH, NVXT_CAMERA, "+NvxCameraPreChangeState\n"));

    pNvxCamera = (SNvxCameraData *)pNvComp->pComponentData;
    NV_ASSERT(pNvxCamera);

    if (!pNvxCamera->bInitialized)
        return OMX_ErrorNone;

    err = NvxNvMMTransformPreChangeState(&pNvxCamera->oSrcCamera, oNewState,
                                         pNvComp->eState);
    if (OMX_ErrorNone != err)
        return err;

    err = NvxNvMMTransformPreChangeState(&pNvxCamera->oDigitalZoom, oNewState,
                                         pNvComp->eState);
    if (OMX_ErrorNone != err)
        return err;

    return OMX_ErrorNone;
}

static void FillAlgorithmControl(NvMMCameraAlgorithmParameters *params, NVX_CONFIG_PRECAPTURECONVERGE *pPreCapConverge)
{
    params[NvMMCameraAlgorithms_AutoWhiteBalance].Enable = pPreCapConverge->bPrecaptureConverge;
    params[NvMMCameraAlgorithms_AutoWhiteBalance].TimeoutMS = pPreCapConverge->nTimeOutMS;
    params[NvMMCameraAlgorithms_AutoWhiteBalance].TimeoutAction = NvMMCameraTimeoutAction_StartCapture;
    params[NvMMCameraAlgorithms_AutoWhiteBalance].ContinueDuringCapture = pPreCapConverge->bContinueDuringCapture;
    params[NvMMCameraAlgorithms_AutoWhiteBalance].FrequencyOfConvergence = 0;

    params[NvMMCameraAlgorithms_AutoExposure].Enable = pPreCapConverge->bPrecaptureConverge;
    params[NvMMCameraAlgorithms_AutoExposure].TimeoutMS = pPreCapConverge->nTimeOutMS;
    params[NvMMCameraAlgorithms_AutoExposure].TimeoutAction = NvMMCameraTimeoutAction_StartCapture;
    params[NvMMCameraAlgorithms_AutoExposure].ContinueDuringCapture = pPreCapConverge->bContinueDuringCapture;
    params[NvMMCameraAlgorithms_AutoExposure].FrequencyOfConvergence = 0;

    params[NvMMCameraAlgorithms_AutoFocus].Enable = 0;
    params[NvMMCameraAlgorithms_AutoFocus].TimeoutMS = pPreCapConverge->nTimeOutMS;
    params[NvMMCameraAlgorithms_AutoFocus].TimeoutAction = NvMMCameraTimeoutAction_StartCapture;
    params[NvMMCameraAlgorithms_AutoFocus].ContinueDuringCapture = pPreCapConverge->bContinueDuringCapture;
    params[NvMMCameraAlgorithms_AutoFocus].FrequencyOfConvergence = 0;
}


static NvError CameraPreviewEnableComplex(SNvxCameraData *pNvxCamera, NvBool Enable, NvU32 Count, NvU32 Delay)
{
    NvError status = NvSuccess;
    NvMMBlockHandle hBlock;
    NvMMCameraCaptureComplexRequest Request;
    NvMMCameraPinState State =
        (Enable) ? NvMMCameraPinState_Enable : NvMMCameraPinState_Disable;

    NvOsMemset(&Request, 0, sizeof(Request));
    hBlock = pNvxCamera->oSrcCamera.hBlock;

    if (Count == 0)
    {
        // Use the simple normal way
        NV_ASSERT_SUCCESS(hBlock->SetAttribute(hBlock,
                                               NvMMCameraAttribute_Preview, 0,
                                               sizeof(NvMMCameraPinState),
                                               &State));
    }
    else
    {
        // Use the backdoor trick
        Request.DelayFrames = Delay;
        Request.CaptureCount = Count;
        NV_ASSERT_SUCCESS(hBlock->SetAttribute(hBlock,
                                               NvMMCameraAttribute_Preview, 0,
                                               sizeof(Request),
                                               &Request));
    }
    return status;
}

static NvError
CameraTakePictureComplex(
    SNvxCameraData *pNvxCamera,
    NvU32 NslCount,
    NvU32 NslPreCount,
    NvU64 NslTimestamp,
    NvU32 Count,
    NvU32 Delay,
    NvU32 SkipCount)
{
    NvError status = NvSuccess;
    NvMMBlockHandle hBlock = pNvxCamera->oSrcCamera.hBlock;

    if (Delay == 0)
    {
        // Use the simple normal way
        NvMMCameraCaptureStillRequest Request;
        NvOsMemset(&Request, 0, sizeof(Request));
        Request.NumberOfImages = Count;
        Request.BurstInterval = SkipCount;
        Request.NslNumberOfImages = NslCount;
        Request.NslPreCount = NslPreCount;
        // NvMM camera block has all of its timestamps in 100 ns units, but
        // OMX expects units of us for its buffer timestamps.  we might as well
        // keep it consistent to other OMX/NvMM interfaces that use timestamps.
        Request.NslTimestamp = NslTimestamp * 10;


        Request.Resolution.width = pNvxCamera->nSurfWidthStillCapture;
        Request.Resolution.height = pNvxCamera->nSurfHeightStillCapture;
        if (pNvxCamera->oPrecaptureConverge.bPrecaptureConverge)
            FillAlgorithmControl(Request.AlgorithmControl, &pNvxCamera->oPrecaptureConverge);

        status = hBlock->Extension(
            hBlock,
            NvMMCameraExtension_CaptureStillImage,
            sizeof(NvMMCameraCaptureStillRequest),
            &Request,
            0,
            NULL);
    }
    else
    {
        // Use the backdoor trick
        NvMMCameraCaptureComplexRequest Request;
        NvOsMemset(&Request, 0, sizeof(Request));

        Request.DelayFrames = Delay;
        Request.CaptureCount = Count;
        Request.BurstInterval = SkipCount;

        if (pNvxCamera->oPrecaptureConverge.bPrecaptureConverge)
            FillAlgorithmControl(Request.AlgorithmControl, &pNvxCamera->oPrecaptureConverge);

        status = hBlock->Extension(
            hBlock,
            NvMMCameraExtension_CaptureStillImage,
            sizeof(Request),
            &Request,
            0,
            NULL);
    }
    return status;
}

static NvError CameraVideoCaptureComplex(SNvxCameraData *pNvxCamera, NvBool Enable, NvU32 Count, NvU32 Delay, NvBool NoStreamEndEvent)
{
    NvError status = NvSuccess;
    NvMMBlockHandle hCamBlock = pNvxCamera->oSrcCamera.hBlock;
    NvMMCameraCaptureMode camCaptureMode;

    // set the use-case hint, so that the camera block can select the
    // sensor mode properly.  reset it when we're done.  since this
    // function is the only thing that controls it, it will make sure
    // that we are never in the still-capture hint mode when video
    // recording is active.  that could cause bad interactions with
    // video snapshot if we did that.
    if (Enable)
    {
        camCaptureMode = NvMMCameraCaptureMode_Video;
    }
    else
    {
        camCaptureMode = NvMMCameraCaptureMode_StillCapture;
    }

    hCamBlock->SetAttribute(
        hCamBlock,
        NvMMCameraAttribute_CaptureMode,
        0,
        sizeof(NvMMCameraCaptureMode),
        &camCaptureMode);

    if (Count == 0)
    {
        // Use the simple normal way
        NvMMCameraCaptureVideoRequest Request;
        NvOsMemset(&Request, 0, sizeof(Request));

        if (Enable)
        {
            Request.State = NvMMCameraPinState_Enable;
        }
        else if (NoStreamEndEvent)
        {
            Request.State = NvMMCameraPinState_Pause;
        }
        else
        {
            Request.State = NvMMCameraPinState_Disable;
        }

        Request.Resolution.width = pNvxCamera->nSurfWidthVideoCapture;
        Request.Resolution.height = pNvxCamera->nSurfHeightVideoCapture;
        if (pNvxCamera->oPrecaptureConverge.bPrecaptureConverge)
            FillAlgorithmControl(Request.AlgorithmControl, &pNvxCamera->oPrecaptureConverge);

        status = hCamBlock->Extension(hCamBlock,
                                   NvMMCameraExtension_CaptureVideo,
                                   sizeof(NvMMCameraCaptureVideoRequest),
                                   &Request,
                                   0, NULL);
    }
    else
    {
        // Use the backdoor trick
        NvMMCameraCaptureComplexRequest Request;
        NvOsMemset(&Request, 0, sizeof(Request));

        Request.DelayFrames = Delay;
        Request.CaptureCount = Count;

        if (pNvxCamera->oPrecaptureConverge.bPrecaptureConverge)
            FillAlgorithmControl(Request.AlgorithmControl, &pNvxCamera->oPrecaptureConverge);

        status = hCamBlock->Extension(hCamBlock,
                                   NvMMCameraExtension_CaptureVideo,
                                   sizeof(Request),
                                   &Request,
                                   0, NULL);
    }
    return status;
}

static OMX_ERRORTYPE NvxCameraGetTransformData(SNvxCameraData *pNvxCamera, NvxComponent *pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_ERRORTYPE eCloseError = OMX_ErrorNone;

    if (pNvxCamera->bTransformDataInitialized)
        return eError;

    pNvxCamera->oSrcCamera.nForceLocale = 1;

    if (pNvxCamera->bUseUSBCamera == OMX_TRUE)
    {
        eError = NvxNvMMTransformOpen(&pNvxCamera->oSrcCamera,
                    NvMMBlockType_SrcUSBCamera, "SrcCamera",
                    s_nvx_num_ports_usbc, pComponent);
    }
    else
    {
        eError = NvxNvMMTransformOpen(&pNvxCamera->oSrcCamera,
                    NvMMBlockType_SrcCamera, "SrcCamera",
                    s_nvx_num_ports_sc, pComponent);
    }
    if (eError != OMX_ErrorNone)
        return eError;
    (void)NvxNvMMTransformSetEventCallback(&pNvxCamera->oSrcCamera,
                             nvxCameraEventCallback, pNvxCamera);

    pNvxCamera->oDigitalZoom.nForceLocale = 1;
    pNvxCamera->oDigitalZoom.bVideoTransform = OMX_TRUE;
    eError = NvxNvMMTransformOpen(&pNvxCamera->oDigitalZoom,
                 NvMMBlockType_DigitalZoom, "DigitalZoom",
                 s_nvx_num_ports_dz, pComponent);
    if (eError != OMX_ErrorNone)
    {
        goto fail;
    }
    (void)NvxNvMMTransformSetEventCallback(&pNvxCamera->oDigitalZoom,
                             nvxCameraEventCallback, pNvxCamera);

    eError = NvxCameraInitializeCameraSettings(pNvxCamera);
    if (eError != OMX_ErrorNone)
        goto fail;

    pNvxCamera->bTransformDataInitialized = OMX_TRUE;

    return eError;

fail:

    if (pNvxCamera->oDigitalZoom.hBlock)
    {
        eCloseError = NvxNvMMTransformClose(&pNvxCamera->oDigitalZoom);
        if (eCloseError != OMX_ErrorNone)
            return eCloseError;
    }

    if (pNvxCamera->oSrcCamera.hBlock)
    {
        eCloseError = NvxNvMMTransformClose(&pNvxCamera->oSrcCamera);
        if (eCloseError != OMX_ErrorNone)
            return eCloseError;
    }

    if (pNvxCamera->oDigitalZoom.hBlock)
        while (!pNvxCamera->oDigitalZoom.blockCloseDone);

    if (pNvxCamera->oSrcCamera.hBlock)
        while (!pNvxCamera->oSrcCamera.blockCloseDone);

    return eError;
}


/* getting the initial camera setting from nvmm blocks */
static OMX_ERRORTYPE
NvxCameraInitializeCameraSettings(
    SNvxCameraData *pNvxCamera)
{
    NvError Err = NvSuccess;
    NvDdk2dTransform Transform = NvDdk2dTransform_None;
    NvU32 i = 0;
    NvMMDigitalZoomAttributeType DzTransformAttribute[CameraDZOutputs_Num] =
        {NvMMDigitalZoomAttributeType_PreviewTransform,
         NvMMDigitalZoomAttributeType_StillTransform,
         NvMMDigitalZoomAttributeType_VideoTransform,
         NvMMDigitalZoomAttributeType_ThumbnailTransform};

    for (i = 0; i < CameraDZOutputs_Num; i++)
    {
        Err = pNvxCamera->oDigitalZoom.hBlock->GetAttribute(
                pNvxCamera->oDigitalZoom.hBlock,
                DzTransformAttribute[i],
                sizeof(NvDdk2dTransform),
                &Transform);

        if (Err != NvSuccess)
            return OMX_ErrorInvalidState;

        ConvertTransform2RotationMirror(Transform, &pNvxCamera->Mirror[i],
            &pNvxCamera->Rotation[i]);
    }

    return OMX_ErrorNone;
}

/**
 * Convert NvDdk2dTransform used by DZ to omx's mirror and rotation
 * Rotation is applied before mirroring and this is a 1-to-2 mapping.
 * Which one it chooses does not matter because this is only used to
 * convert DZ's default setting to OMX values.
 */
static void
ConvertTransform2RotationMirror(
    NvDdk2dTransform Transform,
    OMX_MIRRORTYPE *pMirror,
    OMX_S32 *pRotation)
{
    switch (Transform)
    {
        case NvDdk2dTransform_Rotate90:
            *pMirror = OMX_MirrorNone;
            *pRotation = 90;
            break;
        case NvDdk2dTransform_Rotate180:
            *pMirror = OMX_MirrorNone;
            *pRotation = 180;
            break;
        case NvDdk2dTransform_Rotate270:
            *pMirror = OMX_MirrorNone;
            *pRotation = 270;
            break;
        case NvDdk2dTransform_FlipHorizontal:
            *pMirror = OMX_MirrorHorizontal;
            *pRotation = 0;
            break;
        case NvDdk2dTransform_InvTranspose:
            *pMirror = OMX_MirrorHorizontal;
            *pRotation = 90;
            break;
        case NvDdk2dTransform_FlipVertical:
            *pMirror = OMX_MirrorVertical;
            *pRotation = 0;
            break;
        case NvDdk2dTransform_Transpose:
            *pMirror = OMX_MirrorVertical;
            *pRotation = 90;
            break;
        case NvDdk2dTransform_None:
        default:
            *pMirror = OMX_MirrorNone;
            *pRotation = 0;
            break;
    }
}


static OMX_ERRORTYPE
NvxCameraSetDzTransform(
    SNvxCameraData *pNvxCamera,
    CameraDZOutputs Output,
    OMX_S32 *pNewRotation,
    OMX_MIRRORTYPE *pNewMirror)
{
    OMX_MIRRORTYPE Mirror = pNvxCamera->Mirror[Output];
    OMX_S32 Rotation = pNvxCamera->Rotation[Output];
    NvError Err = NvSuccess;
    NvDdk2dTransform Transform = NvDdk2dTransform_None;
    NvMMDigitalZoomAttributeType AttributeType =
        NvMMDigitalZoomAttributeType_PreviewTransform;

    if (pNewRotation)
        Rotation = *pNewRotation;

    if (pNewMirror)
        Mirror = *pNewMirror;


    if (Output == CameraDZOutputs_Preview)
    {
        AttributeType = NvMMDigitalZoomAttributeType_PreviewTransform;
    }
    else if (Output == CameraDZOutputs_Still)
    {
        AttributeType = NvMMDigitalZoomAttributeType_StillTransform;
    }
    else if (Output == CameraDZOutputs_Video)
    {
        AttributeType = NvMMDigitalZoomAttributeType_VideoTransform;
    }
    else
    {
        AttributeType = NvMMDigitalZoomAttributeType_ThumbnailTransform;
    }

    Transform = ConvertRotationMirror2Transform(Rotation, Mirror);

    Err = pNvxCamera->oDigitalZoom.hBlock->SetAttribute(
        pNvxCamera->oDigitalZoom.hBlock,
        AttributeType,
        NvMMSetAttrFlag_Notification,
        sizeof(NvDdk2dTransform),
        &Transform);

    if (Err != NvSuccess)
        return OMX_ErrorInvalidState;
    else
        NvOsSemaphoreWait(pNvxCamera->oDigitalZoom.SetAttrDoneSema);

    pNvxCamera->Rotation[Output] = Rotation;
    pNvxCamera->Mirror[Output] = Mirror;

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE
NvxCameraCopySurfaceForUser(
    NvRmSurface *Surfaces,
    OMX_PTR userBuffer,
    OMX_U32 *size,
    NVX_VIDEOFRAME_FORMAT format)
{
    NvU32 lumaSize, reqSize;
    NvU8 *dataBuffer;

    if (size == NULL || Surfaces == NULL)
    {
        return OMX_ErrorBadParameter;
    }

    lumaSize = Surfaces[0].Width * Surfaces[0].Height;
    reqSize = lumaSize * 3 / 2;

    if (userBuffer == NULL)
    {
        *size = (OMX_U32)reqSize;
        return OMX_ErrorNone;
    }
    if (*size < reqSize)
    {
        // Size of user-supplied buffer is too small for our data
        return OMX_ErrorInsufficientResources;
    }

    dataBuffer = (NvU8 *)userBuffer;

    // Check that input surface follows the conventions of a YUV420 surface
    if (! ((Surfaces[0].Width == Surfaces[1].Width * 2) &&
           (Surfaces[0].Height == Surfaces[1].Height * 2) &&
           (Surfaces[0].Width == Surfaces[2].Width * 2) &&
           (Surfaces[0].Height == Surfaces[2].Height * 2)))
    {
        return OMX_ErrorBadParameter;
    }
    if (! (NV_COLOR_GET_BPP(Surfaces[0].ColorFormat) == 8 &&
           NV_COLOR_GET_BPP(Surfaces[1].ColorFormat) == 8 &&
           NV_COLOR_GET_BPP(Surfaces[2].ColorFormat) == 8))
    {
        return OMX_ErrorBadParameter;
    }

    if (format == NVX_VIDEOFRAME_FormatYUV420)
    {
        // copy data in
        NvRmSurfaceRead(&(Surfaces[0]), 0, 0, Surfaces[0].Width, Surfaces[0].Height,
            dataBuffer);
        NvRmSurfaceRead(&(Surfaces[1]), 0, 0, Surfaces[1].Width, Surfaces[1].Height,
            dataBuffer + lumaSize);
        NvRmSurfaceRead(&(Surfaces[2]), 0, 0, Surfaces[2].Width, Surfaces[2].Height,
            dataBuffer + lumaSize + lumaSize / 4);
    }
    else if (format == NVX_VIDEOFRAME_FormatNV21)
    {
        NvU8 *tmpVBuffer = NvOsAlloc(lumaSize / 4);
        int chromaSize = lumaSize / 4;
        NvU8 *srcU, *srcV, *dst;

        if (tmpVBuffer == NULL)
        {
            // Couldn't get all necessary working memory
            return OMX_ErrorInsufficientResources;
        }

        // Read Y to Y, read U to the tail-end of the VU plane,
        // read V to a temporary buffer
        dst = dataBuffer + lumaSize;
        srcU = dataBuffer + lumaSize + chromaSize;
        srcV = tmpVBuffer;

        // copy data in
        NvRmSurfaceRead(&(Surfaces[0]), 0, 0, Surfaces[0].Width, Surfaces[0].Height,
            dataBuffer);
        NvRmSurfaceRead(&(Surfaces[1]), 0, 0, Surfaces[1].Width, Surfaces[1].Height,
            srcU);
        NvRmSurfaceRead(&(Surfaces[2]), 0, 0, Surfaces[2].Width, Surfaces[2].Height,
            srcV);

        while (dst < srcU)
        {
            *dst++ = *srcV++;
            *dst++ = *srcU++;
        }

        NvOsFree(tmpVBuffer);
    }
    else
    {
        return OMX_ErrorBadParameter;
    }
    return OMX_ErrorNone;
}

static OMX_U32
NvxCameraGetDzOutputIndex(OMX_S32 nPortIndex)
{
    OMX_U32 OutputIndex = 0;

    if (nPortIndex == s_nvx_port_preview)
    {
        OutputIndex = CameraDZOutputs_Preview;
    }
    else if (nPortIndex == s_nvx_port_still_capture)
    {
        OutputIndex = CameraDZOutputs_Still;
    }
    else if (nPortIndex == s_nvx_port_video_capture)
    {
        OutputIndex = CameraDZOutputs_Video;
    }
    else
    {
        OutputIndex = CameraDZOutputs_Thumb;
    }
    return OutputIndex;
}

static NVX_IMAGE_COLOR_FORMATTYPE
NvxCameraConvertNvColorToNvxImageColor(
    NvMMDigitalZoomFrameCopyColorFormat *pFormat)
{
    if (!pFormat)
        return NVX_IMAGE_COLOR_FormatInvalid;
    // NV21
    else if(pFormat->NumSurfaces == 2 &&
            pFormat->SurfaceColorFormat[0] == NvColorFormat_Y8 &&
            pFormat->SurfaceColorFormat[1] == NvColorFormat_V8_U8)
    {
        return NVX_IMAGE_COLOR_FormatNV21;
    }
    else if (pFormat->NumSurfaces == 3 &&
             pFormat->SurfaceColorFormat[0] == NvColorFormat_Y8 &&
             pFormat->SurfaceColorFormat[1] == NvColorFormat_V8 &&
             pFormat->SurfaceColorFormat[2] == NvColorFormat_U8)
    {
        return NVX_IMAGE_COLOR_FormatYV12;
    }

    return NVX_IMAGE_COLOR_FormatInvalid;
}

static OMX_ERRORTYPE
NvxCameraConvertNvxImageColorToNvColor(
    NVX_IMAGE_COLOR_FORMATTYPE NvxImageColorFormat,
    NvMMDigitalZoomFrameCopyColorFormat *pNvColorFormat)
{
    if (!pNvColorFormat)
        return OMX_ErrorBadParameter;

    NvOsMemset(pNvColorFormat, 0, sizeof(NvMMDigitalZoomFrameCopyColorFormat));

    if (NvxImageColorFormat == NVX_IMAGE_COLOR_FormatNV21)
    {
        pNvColorFormat->NumSurfaces = 2;
        pNvColorFormat->SurfaceColorFormat[0] = NvColorFormat_Y8;
        pNvColorFormat->SurfaceColorFormat[1] = NvColorFormat_V8_U8;
    }
    else if (NvxImageColorFormat == NVX_IMAGE_COLOR_FormatYV12)
    {
        pNvColorFormat->NumSurfaces = 3;
        pNvColorFormat->SurfaceColorFormat[0] = NvColorFormat_Y8;
        pNvColorFormat->SurfaceColorFormat[1] = NvColorFormat_V8;
        pNvColorFormat->SurfaceColorFormat[2] = NvColorFormat_U8;
    }
    else
        return OMX_ErrorUnsupportedSetting;

    return OMX_ErrorNone;
}

void
NvxCameraMappingPreviewOutputRegionToInputRegion(
    NvRectF32 *pCameraRegions,
    NvMMDigitalZoomOperationInformation *pDZInfo,
    NVX_RectF32 *pPreviewOutputRegions,
    NvU32 NumOfRegions)
{
    NvU32 i = 0;
    NvF32 CamW = 0.0f, CamH = 0.0f, CropW = 0.0f, CropH = 0.0f,
          CropStartX = 0.0f, CropStartY = 0.0f;

    CamW = (NvF32)pDZInfo->PreviewInputSize.width;
    CamH = (NvF32)pDZInfo->PreviewInputSize.height;
    CropW = (NvF32)(pDZInfo->PreviewInputCropRect.right -
                    pDZInfo->PreviewInputCropRect.left);
    CropH = (NvF32)(pDZInfo->PreviewInputCropRect.bottom -
                    pDZInfo->PreviewInputCropRect.top);
    CropStartX = (NvF32)pDZInfo->PreviewInputCropRect.left;
    CropStartY = (NvF32)pDZInfo->PreviewInputCropRect.top;

    for (i = 0; i < NumOfRegions; i++)
    {
        NvRectF32 *pRectCam = &pCameraRegions[i];
        NVX_RectF32 *pRectClient = &pPreviewOutputRegions[i];

        if (CamW == 0 && CamH == 0)
        {
            // preview isn't enabled yet
            pRectCam->left   = pRectClient->left;
            pRectCam->top    = pRectClient->top;
            pRectCam->right  = pRectClient->right;
            pRectCam->bottom = pRectClient->bottom;

            continue;
        }
        // consider transform
        switch (pDZInfo->PreviewTransform)
        {
            case NvDdk2dTransform_Rotate90:
                pRectCam->left = -pRectClient->bottom;
                pRectCam->top = pRectClient->left;
                pRectCam->right = -pRectClient->top;
                pRectCam->bottom = pRectClient->right;
                break;
            case NvDdk2dTransform_Rotate180:
                pRectCam->left = -pRectClient->right;
                pRectCam->top = -pRectClient->bottom;
                pRectCam->right = -pRectClient->left;
                pRectCam->bottom = -pRectClient->top;
                break;
            case NvDdk2dTransform_Rotate270:
                pRectCam->left = pRectClient->top;
                pRectCam->top = -pRectClient->right;
                pRectCam->right = pRectClient->bottom;
                pRectCam->bottom = -pRectClient->left;
                break;
            case NvDdk2dTransform_FlipHorizontal:
                pRectCam->left = -pRectClient->right;
                pRectCam->top = pRectClient->top;
                pRectCam->right = -pRectClient->left;
                pRectCam->bottom = pRectClient->bottom;
                break;
            case NvDdk2dTransform_FlipVertical:
                pRectCam->left = pRectClient->left;
                pRectCam->top = -pRectClient->bottom;
                pRectCam->right = pRectClient->right;
                pRectCam->bottom = -pRectClient->top;
                break;
            case NvDdk2dTransform_Transpose:
                pRectCam->left = pRectClient->top;
                pRectCam->top = pRectClient->left;
                pRectCam->right = pRectClient->bottom;
                pRectCam->bottom = pRectClient->right;
                break;
            case NvDdk2dTransform_InvTranspose:
                pRectCam->left = -pRectClient->bottom;
                pRectCam->top = -pRectClient->right;
                pRectCam->right = -pRectClient->top;
                pRectCam->bottom = -pRectClient->left;
                break;
            case NvDdk2dTransform_None:
                pRectCam->left = pRectClient->left;
                pRectCam->top = pRectClient->top;
                pRectCam->right = pRectClient->right;
                pRectCam->bottom = pRectClient->bottom;
            default:
                break;
        }

        // consider input crop
        pRectCam->left = (CropW + CropW * pRectCam->left +
                          2 * CropStartX - CamW) / CamW;
        pRectCam->right = (CropW + CropW * pRectCam->right +
                           2 * CropStartX - CamW) / CamW;
        pRectCam->top = (CropH + CropH * pRectCam->top +
                         2 * CropStartY - CamH) / CamH;
        pRectCam->bottom = (CropH + CropH * pRectCam->bottom +
                            2 * CropStartY - CamH) / CamH;
    }
}

// For matrix metering, customer reference device always meters full RoI
// with some weight. Window 0 is now the background area outside
// of the RoI windows after zoom (cropping).
// This value defines the background weight ratio
// percentage of total weight from all regions
// Set to 0 restore NV default behavior.
// For centering metering, this value has no effect.
#define ROI_BACKGROUND_WEIGHT_RATIO (0.005f)

NvError NvxCameraUpdateExposureRegions(void *pNvxCam)
{
    SNvxCameraData *pNvxCamera = (SNvxCameraData *)pNvxCam;
    NvMMBlockHandle cb;
    NvMMBlockHandle zb;
    NvCameraIspExposureRegions ExposureRegions;
    NVX_CONFIG_ExposureRegionsRect OriginalRegions;
    NvMMDigitalZoomOperationInformation DZInfo;
    NvU32 i = 0;
    NvError Err = NvSuccess;
    NvF32 TotalWeight = 0.0f;

    NvOsMemset(&ExposureRegions, 0, sizeof(ExposureRegions));

    cb = pNvxCamera->oSrcCamera.hBlock;
    zb = pNvxCamera->oDigitalZoom.hBlock;

    if (pNvxCamera->ExposureRegions.nRegions > 0)
    {
        OriginalRegions = pNvxCamera->ExposureRegions;
        ExposureRegions.meteringMode = NVCAMERAISP_METERING_MATRIX;
        OriginalRegions.nRegions ++;
        if (OriginalRegions.nRegions > NVX_MAX_EXPOSURE_REGIONS)
            OriginalRegions.nRegions = NVX_MAX_EXPOSURE_REGIONS;

        for (i = OriginalRegions.nRegions-1; i > 0; i--)
        {
            OriginalRegions.weights[i] = OriginalRegions.weights[i-1];
            OriginalRegions.regions[i] = OriginalRegions.regions[i-1];
            TotalWeight += OriginalRegions.weights[i];
        }
    }
    else
    {
        NvOsMemset(&OriginalRegions, 0, sizeof(OriginalRegions));
        ExposureRegions.meteringMode = NVCAMERAISP_METERING_CENTER;
        OriginalRegions.nRegions = 1;
        TotalWeight = 1.0f;
    }

    OriginalRegions.weights[0] = ROI_BACKGROUND_WEIGHT_RATIO * TotalWeight;
    OriginalRegions.regions[0].top = -1.0;
    OriginalRegions.regions[0].left = -1.0;
    OriginalRegions.regions[0].bottom = 1.0;
    OriginalRegions.regions[0].right = 1.0;

    // get the preview information in order to compute the correct
    // exposure region w.r.t camera's preview buffer
    Err = zb->GetAttribute(zb,
        NvMMDigitalZoomAttributeType_OperationInformation,
        sizeof(DZInfo), &DZInfo);
    if (Err != NvSuccess)
    {
        return Err;
    }

    ExposureRegions.numOfRegions = OriginalRegions.nRegions;
    // copy the regions, accounting for whatever DZ transforms we need
    NvxCameraMappingPreviewOutputRegionToInputRegion(
        ExposureRegions.regions,
        &DZInfo,
        OriginalRegions.regions,
        ExposureRegions.numOfRegions);

    // copy the weights, NvF32 and NVX_F32 are both typedef'd as floats
    for (i = 0; i < ExposureRegions.numOfRegions; i++)
    {
        ExposureRegions.weights[i] = OriginalRegions.weights[i];
    }

    Err = cb->SetAttribute(
        cb,
        NvCameraIspAttribute_AutoExposureRegions,
        0,
        sizeof(ExposureRegions),
        &ExposureRegions);

    if (Err != NvSuccess)
    {
        return OMX_ErrorUndefined;
    }

    return Err;
}

static void NvxParseCameraConfig(NvOsFileHandle fileHandle, SNvxCameraData *pNvxCamera)
{
    NvU8 c;
    NvU8 cam_str[] = "camera";
    NvU32 cam_str_len = NvOsStrlen((const char*)cam_str);

    while (NvOsFgetc(fileHandle, &c) != NvError_EndOfFile)
    {
        NvU32 i;
        NvError Err;

        // If the first character is #, then skip to the next line
        if (c != '#')
        {
            for (i = 0; c == cam_str[i]; i++)
            {
                if (i == (cam_str_len - 1))
                {
                    pNvxCamera->nCameraSource++;
                    break;
                }
                if (NvOsFgetc(fileHandle, &c) == NvError_EndOfFile)
                {
                    return;
                }
            }
        }
        // Search End-Of-Line
        while ((Err = NvOsFgetc(fileHandle, &c)) != NvError_EndOfFile)
        {
            if (c == '\n')
            {
                break;
            }
        }
        if (Err == NvError_EndOfFile)
        {
            return;
        }
    }
}
