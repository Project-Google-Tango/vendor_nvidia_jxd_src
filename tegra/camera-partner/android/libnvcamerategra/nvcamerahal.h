/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * Copyright 2008, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef NV_CAMERA_HAL_H
#define NV_CAMERA_HAL_H

#include <nverror.h>
#include <nvmm_camera.h>
#include <nvmm_usbcamera.h>
#include <nvmm_digitalzoom.h>
#include <nvmm_util.h>
#include <nvassert.h>
#include <nvmm_queue.h>

#include "nvcameramemprofileconfigurator.h"
#include "nvcamerasettingsparser.h"
#include "nvcamerahallogging.h"
#include "nvcameracallbacks.h"
#include "nvimage_enc.h"
#include "nvimagescaler.h"
#include "nvcamerahalpostprocess.h"
#include "nvcamerahalpostprocessHDR.h"

#include <utils/Log.h>
#include <utils/RefBase.h>
#include <utils/List.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <utils/threads.h>
#include <utils/String8.h>
#include <utils/SortedVector.h>
#include <utils/KeyedVector.h>
#include "camera/CameraParameters.h"
#include <hardware/camera.h>
#include <hardware/camera3.h>
#include <ctype.h>
#include <media/hardware/HardwareAPI.h>
#include <hardware/hardware.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <media/hardware/MetadataBufferType.h>
#include <cutils/properties.h>

#include <hardware_legacy/power.h>
// only included for types that are used to send video buffers to stagefright
#include <NVOMX_IndexExtensions.h>
#include "nvbuffer_manager.h"

// Sensor listener
#include "nvsensorlistener.h"

#include "ft/fd_api.h"

//=========================================================
// Header to add runtime logging support
//=========================================================
#include "runtimelogs.h"

#if ENABLE_NVIDIA_CAMTRACE
#include "nvcamtrace.h"
#endif

// cond.waitRelative uses nanosec, we want to wait for 5 sec
#define GENERAL_CONDITION_TIMEOUT 5000000000LL

#define FRAME_JITTER_THRESHOLD_US 60000

#define JPEG_ENCODER_MEM_ALIGN 1024

// These defines assume a zoom step of .25
// So map: 0 <-> 1.00, 1 <-> 1.25, 2 <-> 1.50, etc.
// If you change ZOOM_VALUE_TO_ZOOM_FACTOR and ZOOM_FACTOR_TO_ZOOM_VALUE,
// you must also change the definitions for ZOOM_RATIOS and MAX_ZOOM_LEVEL
// to match.
#define ZOOM_VALUE_TO_ZOOM_FACTOR(x) (((int)x + 4) << 14)
#define ZOOM_FACTOR_TO_ZOOM_VALUE(x) ((((int)x) >> 14) - 4)

#define CAMERA_WAKE_LOCK_ID "nv_camera_wakelock"

// error checking macro, prints the error to logcat and
// saves the last error encountered.  useful in situations
// where we need to execute a bunch of code regardless of
// whether earlier parts of it throw errors
#define SHOW_AND_SAVE_ERROR(expr) \
    do \
    { \
        NvError tempErr = (expr); \
        if (tempErr != NvSuccess) \
        { \
            ALOGE("Error 0x%x encountered at %s:%d", tempErr, __FUNCTION__, __LINE__); \
            e = tempErr; \
        } \
    } while (0)

// Convenience macro to send a CAMERA_MSG_ERROR, CAMERA_ERROR_UNKNOWN.
// Can only be called from member functions.
#define NOTIFY_ERROR()        \
    do {                            \
        if (mNotifyCb != NULL) {    \
            NotifyCb(CAMERA_MSG_ERROR, CAMERA_ERROR_UNKNOWN, 0, mCbCookie);  \
        }                           \
    } while (0)

// used for debug
#define NVMMDIGITALZOOM_GET_SURFACE_PTR(pS, surfaces) \
        do { \
            NV_ASSERT(surfaces);  \
            pS[0] = &surfaces[0]; \
            pS[1] = &surfaces[1]; \
            pS[2] = &surfaces[2]; \
        } while(0)

// Unlock events to let something else run while
// calling something that blocks or stalls.
// this should always be called by some API context that has already
// taken APILock, to unlock briefly and allow events through before it
// continues with other API-context functionality.
// passes in a NULL tid pointer because we don't want any ownership to be
// updated in the unlock/relock process
#define UNLOCK_EVENTS() \
        NvBool unlockEventsSuccess = this->Unlock(&this->mLock, NULL, &this->mEventCond)

// Relock after unlocking. NEVER call this alone.
#define RELOCK_EVENTS() \
    do { \
        if (unlockEventsSuccess) \
        { \
            this->Lock(&this->mLock, NULL, &this->mEventCond); \
        } \
    } while (0)

#define UNLOCKED_EVENTS_CALL(expr) \
    do \
    { \
        UNLOCK_EVENTS(); \
        e = (expr); \
        RELOCK_EVENTS(); \
    } while (0)

#define UNLOCKED_EVENTS_CALL_CLEANUP(expr) \
    do \
    { \
        UNLOCK_EVENTS(); \
        e = (expr); \
        RELOCK_EVENTS(); \
        if (e != NvSuccess) \
            goto fail; \
    } while (0)


#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define MEM_PROFILE_TAG "Memory Allocation"

// stole this #define val from the OMX layer, it was hardcoded there
#define NUM_STREAMS 6
// rest of these were hardcoded at OMX layer, so we'll do that here for now
#define NUM_STREAMS_CSI 3
#define NUM_STREAMS_USB 2
#define NUM_STREAMS_DZ 6

#define MAX_NUM_SENSORS 5

#define CAMERA_ABORT_BUFFERS_TIMEOUT 5000

#define CAMERA_PREVIEW_FPS_LIMIT 30

#define MAX_FACES_DETECTED      (10)
#define DEFAULT_FACES_DETECTED  "5"
#define NV_FACE_VERBOSE_LOGGING (0)
// this value comes from camera.h
// see definition of camera_face_t
#define FACE_UNSUPPORTED_COORD (-2000)
#define MIN_DISTANCE_TO_UPDATE (15)

#define ORIENTATION_INVALID (-1)
// this comes from OrientationManager class
#define ORIENTATION_HYSTERESIS          (5)
// this is an experimentally decided value
#define ORIENTATION_UPDATE_THRESHOLD    (45 + ORIENTATION_HYSTERESIS)

#define ALG_LOCK_TIMEOUT_IMMEDIATE (1)
// longer timeout, helpful when scene brightness change is drastic
// e.g. for flashed scenes
#define ALG_LOCK_TIMEOUT_LONG (4000)
#define ALG_LOCK_TIMEOUT_SHORT (2000)
#define ALG_LOCK_TIMEOUT_VIDEO_SNAPSHOT (150)
#define ALG_UNLOCK_TIMEOUT (1)

// Wait times for some ISP settings.
// Most sensors will need at least a couple
// of frames for the settings to take effect.
#define EXP_TIME_FRAME_WAIT_COUNT (2)
#define ISO_FRAME_WAIT_COUNT (2)
#define EXP_COMPENSATION_FRAME_WAIT_COUNT (2)

// EdgeEnhancement Value range is [-101,100] where -101 means disable
#define EdgeEnhancementDisable (-101)

#define ENABLE_STILL_PASSTHROUGH (1<<0)

typedef enum {
    NvMirrorType_MirrorNone = 1,
    NvMirrorType_MirrorHorizontal = 2,
    NvMirrorType_MirrorVertical = 3,
    NvMirrorType_MirrorBoth = 4,
    NvMirrorType_Force32 = 0x7FFFFFFF
} NvMirrorType;


namespace android {

#define NUM_NSL_PADDING_BUFFERS (1)
#define NUM_STANDARD_CAPTURE (5)

static const status_t ERROR_NOT_OPEN = -1;
static const status_t ERROR_OPEN_FAILED = -2;
static const status_t ERROR_ALLOCATE_FAILED = -4;
static const status_t ERROR_NOT_SUPPORTED = -8;
static const status_t ERROR_NOT_READY = -16;
static const status_t STATE_INIT = 0;
static const status_t STATE_ERROR = 1;
static const status_t STATE_OPEN = 2;

class NvCameraHal;
class NvFrameCopyDataCallback;

// loosely based on SNvxNvMMPort struct (minimal subset required for camera)
typedef struct NvMMBlockPortInfoRec {
    // don't need a flag for this, since the block abort command will always
    // be called and waited on by the same thread in our context
    Condition BlockAbortDone;
    NvU32  BufferReqCount;// incriment on buffer Req/Dec on CFG
    NvBool BufferConfigDone;
    Condition BufferConfigDoneCond;
    NvBool BufferNegotiationDone;
    Condition BufferNegotiationDoneCond;
    NvMMNewBufferConfigurationInfo BuffCfg;
    NvMMNewBufferRequirementsInfo  BuffReq;
    NvMMBuffer *Buffers[NvCameraMemProfileConfigurator::MAX_NUMBER_OF_BUFFERS];
} NvMMBlockPortInfo;


typedef struct NvMMBlockContainerRec {
    NvCameraHal *MyHal;

    // obviously, needs a pointer to the block itself, get from NvMMOpenBlock
    NvMMBlockHandle Block;

    // also needs to store TBT
    TransferBufferFunction TransferBufferToBlock;

    // block context
    NvU32 NumStreams;

    NvMMBlockPortInfo Ports[NUM_STREAMS];

    // these "semaphores" were all waited on directly in nvxcamera.c for
    // various things.  what about others that are waited on in the more general
    // transformbase layer?
    // well, they used to be semaphores, but condition variables are necessary
    // since we have multiple threads in the HAL at once and they need to be
    // synchronized
    Condition StreamBufferNegotiationDone;
    Condition StateChangeDone;
    Condition SetAttributeDone;
    Condition BlockCloseDoneCond;
    NvBool BlockCloseDone;
} NvMMBlockContainer;

// struct to link NvMM buffers with their corresponding ANB
// they're inherently linked by memcpying the surface ptr, but
// it's cumbersome for readability to try to check that all the time,
// much simpler to just keep the pointers associated by belonging
// to the same array element, so we'll have "preview buffers"
// be arrays of this struct
typedef struct CameraBufferRec {
    buffer_handle_t *anb;
    NvMMBuffer *nvmm;
    NvBool isPinned;
} CameraBuffer;

typedef CameraBuffer PreviewBuffer;

// stream assignment to DZ ports (or a copy buffer)
typedef enum {
    CAMERA_STREAM_PREVIEW,
    CAMERA_STREAM_VIDEO,
    CAMERA_STREAM_COPY,
    CAMERA_STREAM_NULL
} NvCameraStreamType;

typedef struct CameraStreamRec{
    CameraBuffer cameraBuffer[NvCameraMemProfileConfigurator::MAX_NUMBER_OF_BUFFERS];
    NvU32 numBuf;
    camera3_stream_t *stream;
    NvCameraStreamType streamType;
    uint32_t streamFormat;
} CameraStream;

typedef List<CameraStream*> CameraBufferList;

// struct to keep track of OMX thunking buffers that we use to send our
// NvMM buffers to stagefright, with some state to let us know where they are
typedef struct VideoBufferRec {
    // OMX allocated these dynamically, but that seems silly here, since we'll
    // always need to have them around in the HAL
    OMX_BUFFERHEADERTYPE OmxBufferHdr;
    NvBool isDelivered;
} VideoBuffer;

typedef enum {
    NVCAMERAHAL_CAMERATYPE_UNDEFINED,
    NVCAMERAHAL_CAMERATYPE_MONO,
    NVCAMERAHAL_CAMERATYPE_STEREO,
    NVCAMERAHAL_CAMERATYPE_USB,
    NVCAMERAHAL_CAMERATYPE_MAX,
    NVCAMERAHAL_CAMERATYPE_FORCE32 = 0x7fffffff
} NvCameraHal_CameraType;

typedef enum FaceDetectionStateEnum
{
    // internal states
    FaceDetectionState_Idle   = 0,
    FaceDetectionState_Running,
    FaceDetectionState_Starting,
    FaceDetectionState_Stopping,
    FaceDetectionState_NoRequest,

    // requests
    FaceDetectionState_Start,
    FaceDetectionState_Stop,
    FaceDetectionState_Pause,
    FaceDetectionState_Resume,

    FaceDetectionState_Invalid = 0x7FFFFFFF
} FaceDetectionState;

typedef struct _NvCameraHal_CameraInfo{
    char *deviceLocation;
    NvCameraHal_CameraType camType;
    struct camera_info camInfo;
} NvCameraHal_CameraInfo;

typedef struct FrameCopyArgsRec {
    NvMMBuffer  *BufferPtr;
    NvFrameCopyDataCallback   *This;
    NvBool SendBufferDownstream;
} FrameCopyArgs;

int NvCameraHal_parseCameraConf(NvCameraHal_CameraInfo **, NvBool);
NvCameraHal_CameraType getCamType(int cameraId);

#define UNTUNNEL_CAMERA_DZ

class NvCameraPostProcessHalProxy;

class NvCameraHal : public virtual RefBase {
public:
    /********************************************
    *             PUBLIC METHODS
    *********************************************/
    //////////////////////////////////////////
    // PUBLIC METHODS CALLED FROM ANDROID API
    //////////////////////////////////////////

    // open up a HAL class with the corresponding sensor ID
    // this will open the proper blocks, connect them together,
    // initialize state, and allocate resources so that it is
    // ready to receive commands from the android API interface
    NvCameraHal(NvU32 AndroidSensorId, NvU32 AndroidSensorOrientation, camera_device_t *dev);
    virtual NvBool GetConstructorError();

    // disconnect blocks, free shared resources, close blocks
    // de-init state
    virtual ~NvCameraHal();

/** Set the ISurface from which the preview buffers should be dequeued */
    virtual status_t setPreviewWindow(struct preview_stream_ops *w);

    // TODO clean up return value types here?  not all of them match the
    // expected values from nvcamerahalapi.cpp

    virtual void
    setCallbacks(
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void* user);

    virtual void enableMsgType(int32_t msgType);
    virtual void disableMsgType(int32_t msgType);
    virtual bool msgTypeEnabled(int32_t msgType);

    virtual status_t startPreview();
    virtual void stopPreview();
    virtual bool previewEnabled();

    virtual status_t storeMetaDataInBuffers(bool enable);
    virtual bool isMetaDataStoredInVideoBuffers();

    virtual status_t startRecording();
    virtual void stopRecording();
    virtual bool recordingEnabled();
    virtual void releaseRecordingFrame(const void *opaque);

    virtual status_t autoFocus();
    virtual status_t cancelAutoFocus();
    virtual status_t takePicture();
    virtual status_t cancelPicture();

    virtual status_t setParameters(const CameraParameters& params, NvBool allowNonStandard);
    virtual CameraParameters getParameters(NvBool allowNonStandard) const;

    virtual status_t sendCommand(int32_t command, int32_t arg1, int32_t arg2);

    virtual void release();
    virtual status_t dump(int fd);

    ////////////////////////////////////////////////////////////////
    // PUBLIC METHODS CALLED FROM NvMM (event handlers, callbacks)
    ////////////////////////////////////////////////////////////////

    // maps NvMM events to some kind of functionality in the HAL
    // typically, this would involve sending callbacks to android,
    // or adjusting some internal state values
    static NvError
    NvMMEventHandler(
        void *pContext,
        NvU32 eventType,
        NvU32 eventSize,
        void *eventInfo);

    // helper function that NvFrameCopyDataCallback class use to deliver
    // buffer to postprocessing and ANW
    NvError
    ProcessPreviewBufferAfterFrameCopy(
        NvMMBuffer *Buffer);

    // callback function that full output buffers are delivered to
    static NvError
    NvMMDeliverFullOutput(
        void *BlockContainer,
        NvU32 StreamIndex,
        NvMMBufferType BufferType,
        NvU32 BufferSize,
        void *Buffer);

    static void
    JpegEncoderDeliverFullOutput(
        void *ClientContext,
        NvU32 StreamIndex,
        NvU32 BufferSize,
        void *pBuffer,
        NvError EncStatus);

    static
    void SendJpegBuffer(void *args);

    void ANWDestroyed();
    static void ANWFailureHandler(void* args);

    void PowerOnCamera();

    // Thread to delay settings until first preview
    static void DoStuffThatNeedsPreview(void *args);
    static void StartRecordingAfterVideoBufferUpdate(void *args);

    // Thread to update NvMM camera block in response to some events
    static void EventsUpdateCameraBlock(void *args);
    // Boolean indicating the above thread should not be created
    NvBool mDontCreateEventsUpdateCameraBlockThread;

    static void SetupSensorListenerThread(void *args);

    static void PostProcThread(void *args);

    // Indicates if the driver has initialized buffers
    static NvBool mBuffersInitialized;

    // Indicate Pass Through Mode support
    NvBool mIsPassThroughSupported;

#ifdef UNTUNNEL_CAMERA_DZ // [
NvError CameraDzForwardBuffer(NvMMBlockContainer *pContext,
                              NvU32 StreamIndex,
                              NvMMBufferType BufferType,
                              NvU32 BufferSize,
                              void *pBuffer);

NvError ProcessDzToCamMessages(NvMMBlockContainer *pContext,
                              NvU32 StreamIndex,
                              NvMMBufferType BufferType,
                              NvU32 BufferSize,
                              void *pBuffer);

NvError ProcessCamToDZMessages(NvMMBlockContainer *pContext,
                              NvU32 StreamIndex,
                              NvMMBufferType BufferType,
                              NvU32 BufferSize,
                              void *pBuffer);

#endif // UNTUNNEL_CAMERA_DZ ]

    friend class NvCameraPostProcessHalProxy;
    friend class NvFrameCopyDataCallback;
    friend class NvCameraHal3;

    NvError linkCameraBuffers(
        CameraStream *pCameraStream,
        buffer_handle_t **buffers, NvU32 buffer_num);
    NvError unlinkCameraBuffers(CameraStream *pCameraStream);

    NvError pushCameraBufferToDZ(camera3_stream_buffer *stream_buffer);
    NvError startPreviewCapture();
    NvError stopPreviewCapture();
    NvError startVideoCapture();
    NvError stopVideoCapture();
    NvError getPreviewBufferFromDZ(NvMMBuffer *Buffer);
    NvMMBuffer *NativeToNVMMBuffer(buffer_handle_t *buffer);

    NvOsSemaphoreHandle     mCameraBufferReady;
    buffer_handle_t *       mReceivedCameraBuffer;

    CameraBufferList        mCameraBufferList;

    NvU32 mSensorWidth, mSensorHeight;

private:

    /********************************************
    *           PRIVATE METHODS
    *********************************************/

    // Get the sensor id for opening the camera block.
    NvU64 GetSensorId();

    // fills out all of the fields of the block container
    // that are common to all block containers
    // initializes state, asks block for
    // TBT, sets block event function
    // basically a place to tuck all of the code that is
    // shared during creation of camera and DZ blocks
    // must be called AFTER the NvMM block is opened
    NvError InitializeNvMMBlockContainer(NvMMBlockContainer &BlockContainer);
    // frees resources and de-init's state associated with
    // a block container, also closes the NvMM block
    NvError CloseNvMMBlockContainer(NvMMBlockContainer &BlockContainer);

    // opens the NvMM camera block with the proper parameters,
    // calls InitializeNvMMBlockContainer to fill out the rest
    // of the struct
    // requires the SensorId member to be populated with something valid
    NvError CreateNvMMCameraBlockContainer();

    // opens the NvMM DigitalZoom block, with the proper parameters,
    // calls InitializeNvMMBlockContainer to fill out the rest
    // of the struct
    NvError CreateNvMMDZBlockContainer();

    // sets up tunneling, TBF, TBT, general relationship between the blocks
    void ConnectCameraToDZ();

    NvError CleanupNvMM();
    NvError setupHalInLock();

    // returns true if it's a USB cam, false otherwise
    NvBool isUSB();

    NvError EnableCameraBlockPins(NvMMCameraPin EnabledCameraPins);
    NvError EnableDZBlockPins(NvMMDigitalZoomPin EnabledDZPins);

    // tunnels the preview output port on camera block to preview input port on DZ
    void TunnelCamDZPreview();
    // tunnels the capture output port on camera block to capture input port on DZ
    void TunnelCamDZCapture();
    // registers the callback function for full buffers, etc.
    NvError SetupDZPreviewOutput();
    NvError SetupDZStillOutput();
    NvError SetupDZVideoOutput();
    NvError ResetCamStreams();
    NvError ConnectCaptureGraph();

    NvError
    SetupCaptureOutput(
        NvMMBlockContainer &BlockContainer,
        NvU32 StreamNum);

    NvError
    HandleStreamEvent(
        NvMMBlockContainer &BlockContainer,
        NvU32 StreamIndex,
        NvU32 EventType,
        void *EventInfo);

    // buffer negotiation function declarations, definitions are in
    // nvcamerahalbuffernegotiation.cpp
    static NvCameraMemProfileConfigurator *m_pMemProfileConfigurators[MAX_NUM_SENSORS];
    static NvBufferManager *m_pBufferManagers[MAX_NUM_SENSORS];
    static NvBufferStream  *m_pBufferStreams[MAX_NUM_SENSORS];

    // non-static pointers to the above static variables
    // these are only for convenience
    NvCameraMemProfileConfigurator *m_pMemProfileConfigurator;
    NvBufferManager *m_pBufferManager;
    NvBufferStream  *m_pBufferStream;

    void InitBufferManager();
    NvError BufferManagerConfigureStream(
        NvStreamRequest &streamReq,
        NvDzOutputPort Port,
        NvCameraUserResolution &persistentResolution,
        NvCameraUserResolution &maxResolution,
        NvCameraUserResolution &settingsResolution,
        NvBool *DirtyBit,
        const char *StreamName,
        NvU32 maxOutputBuffers);
    NvError BufferManagerConfigureCaptureBuffers(
                                                NvStreamRequest &streamReq,
                                                NvU32 maxCapturePreviewBuffers,
                                                NvU32 maxCaptureStillBuffers);
    void    DebugBufferCounts(NvBufferManagerComponent component, NvU32 port);
    NvError BufferManagerNegotiateAndSupplyBuffers(NvCombinedCameraSettings &settings);
    NvError BufferManagerSupplyCamBuffers();
    NvError BufferManagerReconfigureStillBuffersResolution(NvU32 width, NvU32 height);
    NvError BufferManagerReconfigureVideoBuffersResolution(NvU32 width, NvU32 height);
    NvError BufferManagerReconfigurePreviewBuffersResolution(NvU32 width, NvU32 height);
    NvError BufferManagerCheckNumberOfBuffersAtLocation(
        NvCameraBufferConfigStage stage,
        NvCameraBufferConfigPortID id,
        NvBufferOutputLocation location,
        NvBool *compatible);
    NvError BufferManagerCheckNumberOfBuffers(NvCameraBufferConfigStage stage, NvBool *compatible);
    NvError BufferManagerUpdateNumberOfBuffersAtLocation(
        NvCameraBufferConfigStage stage,
        NvCameraBufferConfigPortID id,
        NvBufferOutputLocation location);
    NvError BufferManagerUpdateNumberOfBuffers();
    NvError BufferManagerReconfigureNumberOfStillBuffers(
        NvU32 minNumberOfBuffers,
        NvU32 maxNumberOfBuffers,
        NvU32 *allocatedNumberOfBuffers);
    NvError BufferManagerSupplyPreviewBuffers();
    NvError BufferManagerReclaimPreviewBuffers();
    NvError BufferManagerReclaimALLBuffers();
    NvError InitializeNvMMBufferWithANB(PreviewBuffer *previewBuffer,
                                        NvMMBuffer *nvBuffer,
                                        NvU32 bufferNum);

    NvError BufferManagerConfigureANBWindow();
    NvError BufferManagerResetANWBuffers(NvBool reinit);
    NvError BufferManagerGetNumberOfStillBuffers(NvU32 *totalBuffersAllocated);

    void SendEmptyPreviewBufferToDZ(NvMMBuffer *Buffer);
    void SendEmptyVideoBufferToDZ(NvMMBuffer *Buffer);
    void SendEmptyStillBufferToDZ(NvMMBuffer *Buffer);

    NvError ResetCameraStream(NvMMCameraStreamIndex StreamIndex);
    NvError RenegotiateCaptureBuffers();
    NvError InitializeNvMMWithANB(PreviewBuffer *Buffer, NvU32 bufferNum);

    void ReturnEmptyBufferToDZ(NvMMBuffer *Buffer, NvU32 StreamIndex);
    static void UpdateSmoothZoom(
        NvCameraHal *This,
        NvS32 zoomValue,
        NvBool ZoomAchieved);

    NvError
    LaunchPostProcThread(
        NvMMBuffer *Buffer,
        NvU32 StreamIndex,
        NvOsThreadHandle *pHandle);
    void JoinPostProcessingThreads(NvU32 BufferType, NvU32 StreamIndex);

    NvError StartPreviewInternal();
    NvError StopPreviewInternal();
    NvError StartPreviewStream();
    NvError StopPreviewStream();
    NvError FlushPreviewBuffers();
    NvError EmptyPreviewBuffer(NvMMBuffer *Buffer);
    NvError EmptyPreviewPostProcBuffer(NvMMBuffer *Buffer);
    NvError SendPreviewBufferDownstream(NvMMBuffer *Buffer);
    NvError HandlePostProcessPreview(NvMMBuffer *Buffer);
    NvBool CheckPostviewCallback(NvMMBuffer *Buffer);
    NvError StopSmoothZoomInternal(NvBool RightNow);

    NvError SetupLeanModeStillBuffer(int NSLBufferNum = 0);
    NvError ReleaseLeanModeStillBuffer();
    NvError SetupLeanModeVideoBuffer(int VSTABBufferNum = 0);
    NvError ReleaseLeanModeVideoBuffer();

    NvError takePictureInternal();
    NvError ProgramExifInfo();
    NvError StartStillCapture();
    NvError EmptyStillBuffer(NvMMBuffer *Buffer);
    NvError EmptyStillPostProcBuffer(NvMMBuffer *Buffer);
    NvError OpenJpegEncoder();

    NvError SetupJpegEncoderStill();
    NvError SetupJpegEncoderThumbnail();
    NvError FeedJpegEncoder(NvMMBuffer *InputBuffer);
    NvError FeedJpegEncoderStill(NvMMBuffer *Buffer);
    NvError FeedJpegEncoderThumbnail(NvMMBuffer *Buffer);
    NvError CloseJpegEncoder();
    NvError AllocateJpegOutputBuffers(NvJpegEncParameters &Params);
    NvError FreeJpegOutputBuffers();

    NvError StartRecordingInternal();
    NvError StopRecordingInternal();
    NvError StartRecordingStream();
    NvError StopRecordingStream();

    NvError EnableShot2Shot(NvCombinedCameraSettings &settings);

    // stagefright needs OMX-style buffers, so we need a few functions
    // to copy NvMM info into the OMX-style bufs that we send to stagefright
    // Used to update the new video timestamp
    void updateVideoTimeStampFromBuffer(OMX_BUFFERHEADERTYPE *pBuffer);
    NvError SendEmbeddedVideoBufferToApp(NvMMBuffer *nvmmBuffer);
    NvError SendNonEmbeddedVideoBufferToApp(NvMMBuffer *Buffer);
    NvError EmptyVideoBuffer(NvMMBuffer *Buffer);
    NvError EmptyVideoPostProcBuffer(NvMMBuffer *Buffer);
    NvError SendVideoBufferDownstream(NvMMBuffer *Buffer);
    VideoBuffer *GetEmptyVideoBuffer();
    void ReclaimDeliveredVideoBuffers();

    static void
    StartFaceDetectorThread(void *args);
    static void
    StopFaceDetectorThread(void *args);
    NvError StartFaceDetection(NvS32 NumFaces, NvS32 Debug);
    NvError StopFaceDetection();
    NvError SetFdState(FaceDetectionState NewState, NvBool Immediate = NV_FALSE);
    NvError RunFdStateMachine();
    NvError EncodePreviewMetaDataForFaces(Face *pFace, NvU32 numFaceDetected);
    static void
    dumpFaceInfo(camera_face_t *pFace, NvU32 i);
    void SetupSensorListener();
    void CleanupSensorListener();
    void onOrientationEvent(uint32_t orientation, uint32_t tilt);
    static void
    orientation_cb(uint32_t orientation, uint32_t tilt, void *cookie);

    NvError
    TransitionBlockToState(
        NvMMBlockContainer &BlockContainer,
        NvMMState State);

    NvError HandleEnableMsgType(int32_t msgType);
    NvError HandleDisableMsgType(int32_t msgType);

    NvError AutoFocusInternal();
    NvError CancelAutoFocusInternal();

    NvError AcquireWakeLock();
    NvError ReleaseWakeLock();

    /* Function wrappers for callbacks.
     * Avoids deadlocks by sending callback requests to separate thread
     * through a queue.
     */
    void NotifyCb(int msgType, int ext1, int ext2, void* user);
    void
    DataCb(
        int msgType,
        camera_memory_t *data,
        void* user,
        camera_frame_metadata_t *metadata = NULL);
    void
    DataCbTimestamp(
        nsecs_t timestamp,
        int32_t msgType,
        camera_memory_t *data,
        void* user);

    static void SendDataCallback(
        NvCameraHal   *This,
        NvMMFrameCopy *FrameCopy,
        NvS32          MsgToCallback);

    void SendRawCallback(NvMMFrameCopy *FrameCopy);
    void GetRawCaptureAttributes();

    NvError CheckAndWaitForCondition(NvBool Flag, Condition &Cond) const;
    NvError WaitForCondition(Condition &Cond);

    // settings code
    NvError SendCapabilitiesToParser(NvCombinedCameraSettings &settings);
    NvError ApplyChangedSettings();
    void
    GetSensorCapabilities(
        NvCameraCapabilities& parseCaps,
        NvCombinedCameraSettings& newSettings);
    NvError updateAEParameters(NvCombinedCameraSettings &settings) const;
    NvError GetDriverDefaults(NvCombinedCameraSettings& defaultSettings);
    NvError GetSceneModeSettings(NvCombinedCameraSettings &settings);
    void GetAutoSceneModeSettings(NvCombinedCameraSettings &settings);
    void GetActionSceneModeSettings(NvCombinedCameraSettings &settings);
    void GetPortraitSceneModeSettings(NvCombinedCameraSettings &settings);
    void GetLandscapeSceneModeSettings(NvCombinedCameraSettings &settings);
    void GetBeachSceneModeSettings(NvCombinedCameraSettings &settings);
    void GetCandlelightSceneModeSettings(NvCombinedCameraSettings &settings);
    void GetFireworksSceneModeSettings(NvCombinedCameraSettings &settings);
    void GetNightSceneModeSettings(NvCombinedCameraSettings &settings);
    void GetNightPortraitSceneModeSettings(NvCombinedCameraSettings &settings);
    void GetPartySceneModeSettings(NvCombinedCameraSettings &settings);
    void GetSnowSceneModeSettings(NvCombinedCameraSettings &settings);
    void GetSportsSceneModeSettings(NvCombinedCameraSettings &settings);
    void GetSteadyPhotoSceneModeSettings(NvCombinedCameraSettings &settings);
    void GetSunsetSceneModeSettings(NvCombinedCameraSettings &settings);
    void GetTheatreSceneModeSettings(NvCombinedCameraSettings &settings);
    void GetBarcodeSceneModeSettings(NvCombinedCameraSettings &settings);
    void GetBacklightSceneModeSettings(NvCombinedCameraSettings &settings);
    void GetHDRSceneModeSettings(NvCombinedCameraSettings &settings);
    NvError GetShot2ShotModeSettings(
        NvCombinedCameraSettings &settings,
        NvBool enable,
        NvBool useFastburst);
    //Sensor Settings
    NvError GetSensorModeList(Vector<NvCameraUserSensorMode> &oSensorModeList);
    NvError GetSensorFocuserParam(NvCameraIspFocusingParameters &params);
    NvError SetSensorFocuserParam(NvCameraIspFocusingParameters &params);
    NvError GetSensorFlashParam(NvBool &present);
    NvError GetSensorIspSupport(NvBool &IspSupport);
    NvError GetSensorBracketCaptureSupport(NvBool &BracketSupport);
    NvError GetLensPhysicalAttr(NvF32 &focalLength, NvF32 &horizontalViewAngle, NvF32 &verticalViewAngle);
    NvError GetSensorStereoCapable(NvBool &stereoSupported);
    NvS32 GetFdMaxLimit();
    NvError SetImageFlipRotation(
        const NvCombinedCameraSettings &settings,
        const NvBool isStill);
    NvError GetContrast(int &value);
    NvError SetContrast(const NvCombinedCameraSettings &settings);
    NvError
    SetPreviewPauseAfterStillCapture(
        NvCombinedCameraSettings &settings,
        NvBool pause);
    NvError GetZoom(NvS32 &value);
    NvError SetZoom(NvS32 value, NvBool smoothZoom);
    NvError SetFastburst(const NvCombinedCameraSettings &settings);
    NvError SetStylizeMode(NvU32 ISPmode, NvBool manualMode, NvBool noFilterMode);
    NvError SetColorCorrectionMatrix(const NvCombinedCameraSettings &settings);
    NvError SetNSLNumBuffers(NvU32 requestedNumBuffers, NvU32 *allocatedNumBuffers);
    NvError SetNSLBurstCount(int nslBurstCount, int burstCaptureCount, NvBool stillCapHdrOn);
    NvError SetVSTABNumBuffers();
    NvError SetBurstCount(int nslBurstCount, int burstCount, NvBool stillCapHdrOn);
    NvError SetBracketCapture(NvMMBracketCaptureSettings &BracketCapture);
    NvError SetBracketCapture(const NvCombinedCameraSettings &settings);
    NvError SetStillHDRMode(const NvCombinedCameraSettings &settings);
    NvError DisableStillHDR(NvCombinedCameraSettings &newSettings);
    NvError DisableANR(NvCombinedCameraSettings &newSettings);
    NvError RestoreANR(NvCombinedCameraSettings &newSettings);
    NvError SetNSLSkipCount(const NvCombinedCameraSettings &settings);
    NvError SetRawDumpFlag(const NvCombinedCameraSettings &settings);
    void SetGpsLatitude(bool Enable, unsigned int lat, bool dir, NvJpegEncParameters *Params);
    void SetGpsLongitude(bool Enable, unsigned int lon, bool dir, NvJpegEncParameters *Params);
    void SetGpsAltitude(bool Enable, float altitude, bool ref, NvJpegEncParameters *Params);
    void SetGpsTimestamp(bool Enable, NvCameraUserTime time, NvJpegEncParameters *Params);
    void SetGpsProcMethod(bool Enable, const char *str, NvJpegEncParameters *Params);

    NvError SetGPSSettings(const NvCombinedCameraSettings &settings);
    NvError GetExposureTimeMicroSec(int &exposureTimeMicroSec);
    NvError SetExposureTimeMicroSec(const NvCombinedCameraSettings &settings);
    NvError SetVideoSpeed(const NvCombinedCameraSettings &settings);
    NvError GetIsoSensitivity(int &iso);
    NvError SetIsoSensitivity(const NvCombinedCameraSettings &settings);
    NvError GetFlashMode(NvCameraUserFlashMode &mode);
    NvError SetFlashMode(const NvCombinedCameraSettings &settings);
    NvError SetFocusMode(const NvCombinedCameraSettings &settings);
    NvError GetFocusPosition(NvCombinedCameraSettings &settings);
    NvError SetFocusPosition(const NvCombinedCameraSettings &settings);
    NvError SetPreviewFormat(const NvCombinedCameraSettings &settings);
    NvError NvMMFlashModeToNvCameraUserFlashMode(NvU32 nvmmFlash, NvCameraUserFlashMode &userFlash);
    NvError NvCameraUserFlashModeToNvMMFlashMode(NvCameraUserFlashMode userFlash, NvU32 &nvmmFlash);
    NvError SetExposureCompensation(const NvCombinedCameraSettings &settings);
    NvError programAlgLock(
        NvU32 algs,
        NvBool lock,
        NvBool relock,
        NvU32 timeout,
        NvU32 algSubType);
    NvError GetWhitebalance(NvCameraUserWhitebalance &userWb);
    NvError SetWhitebalance(NvCameraUserWhitebalance userWb);
    NvError SetExposureWindows(const NvCombinedCameraSettings &settings);
    NvError SetFocusWindows(const NvCombinedCameraSettings &settings);
    NvError SetFocusWindowsForFaces(NvCameraUserWindow *focusingWindows, NvU32 NumFaces);
    NvError SetExposureWindowsForFaces(NvCameraUserWindow *ExposureWindows, NvU32 NumFaces);
    NvError GetStabilizationMode(NvCombinedCameraSettings &settings);
    NvError SetStabilizationMode(const NvCombinedCameraSettings &settings);
    // Apply DZ/rotation/mirror effect on input regions set to camera core
    NvError ApplyFocusRegions(NvCameraIspFocusingRegions *focusRegionsIn);
    NvError ApplyExposureRegions(NvCameraIspExposureRegions *exposureRegionsIn);
    NvBool CheckCameraSceneBrightness(void);
    NvBool UsingFlash(void);
    NvError SetupTorch();
    NvError UnlockAeAwbAfterTorch();
    NvError ProgramDeviceRotation(NvS32 rotation);
    NvError SetFdDebug(const NvCombinedCameraSettings &settings);
    NvError Program3AForFaces(Face *pFace, NvU32 numFaceDetected);
    // Start and stop the event update thread on the camera block.
    NvError StartEventsUpdateCameraBlockThread(NvU32 EventType);
    void StopEventsUpdateCameraBlockThread(void);

#ifdef FRAMEWORK_HAS_MACRO_MODE_SUPPORT
    NvError SetLowLightThreshold(const NvCombinedCameraSettings &settings);
    NvError SetMacroModeThreshold(const NvCombinedCameraSettings &settings);
#endif

    NvError SetFocusMoveMsg(NvBool focusMoveMsg);
    // FPS is scaled by 1000. Ex, 30fps = 30000.
    NvError GetFrameRateRange(NvS32 &lowFPS, NvS32 &highFPS);
    NvError SetFrameRateRange(const NvCombinedCameraSettings &settings);
    NvError SetRecordingHint(const NvBool enable);
    NvError GetAntiBanding(NvCameraUserAntiBanding &mode);
    NvError SetAntiBanding(const NvCombinedCameraSettings &settings);
    NvError GetExposureTimeRange(NvCameraUserRange &ExpRange);
    NvError SetExposureTimeRange(const NvCombinedCameraSettings &settings);
    NvError GetIsoSensitivityRange(NvCameraUserRange &IsoRange);
    NvError SetIsoSensitivityRange(const NvCombinedCameraSettings &settings);
    NvError GetWhitebalanceCCTRange(NvCameraUserRange &wbRange);
    NvError SetWhitebalanceCCTRange(const NvCombinedCameraSettings &settings);
    NvError SetAdvancedNoiseReductionMode(AnrMode anrMode);
    NvError InformVideoMode(NvBool isVideo);
    NvError GetEdgeEnhancement(int &edgeEnhancement);
    NvError SetEdgeEnhancement(const NvCombinedCameraSettings &settings);
    NvError SetAELock(const NvCombinedCameraSettings &settings);
    NvError SetAWBLock(const NvCombinedCameraSettings &settings);
    NvError GetSaturation(NvS32 &value);
    NvError SetSaturation(const NvCombinedCameraSettings &settings);
    NvError SetColorEffect(const NvCombinedCameraSettings &settings);
    NvError WaitForStuffThatNeedsPreview();
    NvError CameraGetUserYUV(NvMMBuffer *Buffer, camera_memory_t **shmem, NVMM_FRAME_FORMAT format);
    NvError StartAutoFocus(NvBool FlashEnabled);
    NvError StopAutoFocus();
    void CameraMappingPreviewOutputRegionToInputRegion(
            NvRectF32 *pCameraRegions,
            NvMMDigitalZoomOperationInformation *pDZInfo,
            NvRectF32 *pPreviewOutputRegions,
            NvU32 NumOfRegions);
    NvError StopStillCapture();
    // This is used to program capture request number which is
    // used in YUV camera case.
    NvError SetConcurrentCaptureRequestNumber(NvU32 num);
    NvError GetPreviewSensorMode(NvMMCameraSensorMode &mode);
    NvError GetCaptureSensorMode(NvMMCameraSensorMode &mode);
    NvError GetNSLCaptureSensorMode(NvMMCameraSensorMode &mode,
        const NvCombinedCameraSettings &newSettings);
    void SetFinalWaitCountInFrames(NvU32 RequestedWaitCount);
    NvError GetDzStillOutputSurfaceType(void);
    NvError GetPassThroughSupport(NvBool &PassThroughSupport);
    NvBool  IsStillJpegData(NvMMBuffer *Buffer);
    NvError ProcessOutBuffer(NvMMBuffer *Buffer);

    /********************************************
    *           MEMBER VARIABLES
    *********************************************/

    NvBool mConstructorError;
    NvError mSensorStatus;
    NvMMBlockContainer Cam;
    NvMMBlockContainer DZ;
    NvU32 mSensorId;
    NvU32 mSensorOrientation;
    NvImageEncHandle mImageEncoder;

    // Persistent Resolutions
    static NvCameraUserResolution persistentPreview[MAX_NUM_SENSORS];
    static NvCameraUserResolution persistentStill[MAX_NUM_SENSORS];
    static NvCameraUserResolution persistentVideo[MAX_NUM_SENSORS];

    // Max Resolutions
    NvCameraUserResolution mMaxPreview;
    NvCameraUserResolution mMaxStill;
    NvCameraUserResolution mMaxVideo;

    // For early power on
    Condition mPowerOnCompleteReceived;
    NvBool mIsPowerOnComplete;
    Condition mSensorStatusReceived;
    NvBool mIsSensorPowerOnComplete;

    // For enabling buffer profile print
    NvBool mMemProfilePrintEnabled;

    // Flags/ thread handle to delay encoder open/ setup
    NvOsThreadHandle mThreadToDoStuffThatNeedsPreview;
    NvOsThreadHandle mThreadToStartRecordingAfterVideoBufferUpdate;
    NvBool mDelayJpegEncoderOpen;
    NvBool mDelayJpegStillEncoderSetup;
    NvBool mDelayJpegThumbnailEncoderSetup;
    NvBool mDelayedSettingsComplete;
    Condition mDelayedSettingsDone;

    // Condition and flag to check number of buffers
    // enqueued in the queue
    Condition mAllBuffersReturnedToQueue;
    NvBool mAreAllBuffersBack;

    // DZ output still surface type
    NVMM_FRAME_FORMAT mDzStillOutputSurfaceType;

    // encoder will write output into mImageEncoderOutput, which is specially-
    // allocated Nv-memory.  mClientJpegBuffer is android shmem, and we'll copy
    // from the encoder's output nv-buffer into this with a NvRmMemRead before
    // firing off the callback to the app
    camera_memory_t *mClientJpegBuffer;
    NvMMBuffer *mImageEncoderOutput[NvCameraMemProfileConfigurator::MAX_NUMBER_OF_BUFFERS];
    NvRmDeviceHandle mRmDevice;
    // memory type flag, set TRUE for uncached type and FALSE for cached type.
    NvBool m_bSharedMemory;

    PreviewBuffer
    mPreviewBuffers[NvCameraMemProfileConfigurator::MAX_NUMBER_OF_BUFFERS + NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS];

    int mMinUndequeuedPreviewBuffers;
    // true when app has called startPreview, false during init or after stopPreview
    NvBool mPreviewEnabled;
    // internal flag, set to true when startPreviewInternal is called, false when
    // stopPreviewInternal is called.  *usually* matches mPreviewEnabled, which is
    // the API-facing flag, but will occasionally vary as we stop/start preview
    // internally for things like resolution changes and reconfigurations
    NvBool mPreviewStarted;
    // event-based, used to sync condition vars
    // true when first frame received, false during init or when EOS received
    NvBool mPreviewStreaming;
    // true when the preview has been paused after a still capture
    // generally mPreviewEnabled, mPreviewStarted, and mPreviewStreaming will all
    // be false when it is true
    NvBool mPreviewPaused;

    NvBool mFastburstEnabled;
    NvBool mFastburstStarted;

    // This is to not to allow the framerate change when, snapshot happens in slowmo video rec.
    NvBool mFrameRateChanged;

    VideoBuffer mVideoBuffers[NvCameraMemProfileConfigurator::MAX_NUMBER_OF_BUFFERS];
    // true when app has called startRecording, false during init or after stopRecording
    NvBool mVideoEnabled;
    // internal flag, set to true when StartRecordingInternal is called, false when
    // StopRecordingInternal is called.  *usually* matches mVideoEnabled, which is
    // the API-facing flag, but will occasionally vary as we stop/start recording
    // internally for things like resolution changes and reconfigurations
    NvBool mVideoStarted;
    // event-based, used to sync condition vars
    // true when first frame received, false during init or when EOS received
    NvBool mVideoStreaming;
    OMX_U32  mNumOfVideoFramesDelivered;
    OMX_U64  mUsPrevVideoTimeStamp;
    OMX_U64  mNsCurVideoTimeStamp;

    NvBool mUsingEmbeddedBuffers;

    NvU64 mUsCaptureRequestTimestamp;

    // KPI measurement for single capture
    NvU32 mHalCaptureStartTime;

    NvU32 mHalShutterCallbackTime;
    NvU32 mHalShutterLag;

    NvU32 mHalPostViewCallbackTime;
    NvU32 mHalPostViewLag;

    NvU32 mHalStillYUVBufferFromDZTime;
    NvU32 mHalStillYUVBufferFromDZLag;

    NvU32 mHalJpegCallbackTime;
    NvU32 mHalJpegLag;

    NvCameraSettingsParser mSettingsParser;

    NvCameraHalLogging mCameraLogging;

    // Should use NvS32? we might need to clean up whole callback routines
    int32_t mMsgEnabled;

    struct preview_stream_ops *mPreviewWindow;

    camera_notify_callback mNotifyCb;
    camera_data_callback mDataCb;
    camera_data_timestamp_callback mDataCbTimestamp;
    camera_request_memory mGetMemoryCb;
    void  *mCbCookie;
    NvCameraCallbackQueue mCbQueue;

    NvBool mWakeLocked;

    // smooth zoom related variables
    mutable Condition mSetZoomDoneCond;
    NvBool mSetZoomPending;
    NvBool mSmoothZoomInProgress;
    NvBool mSmoothZoomStopping;
    Condition mSmoothZoomDoneStopping;
    NvS32 mSmoothZoomTargetLevel;
    NvS32 mSmoothZoomCurrentLevel; // Remember zoom level, to report only once.

    // synchronization and threadsafety variables
    mutable Mutex       mLock;
    Condition           mAPICond;
    Condition           mEventCond;
    android_thread_id_t mAPIOwner;
    android_thread_id_t mEventOwner;

    NvBool Lock(Mutex *lock, android_thread_id_t *var, Condition *cond);
    NvBool Unlock(Mutex *lock, android_thread_id_t *var, Condition *cond);
    // Use this to automatically aquire/release lock. Easier to use
    // when there are multiple return statements.
    class APILock {
    public:
        inline APILock(NvCameraHal *cam) : mCamera(cam)
        {
            ALOGVV("APILock++ , APIOwner = %p, eventOwner=%p",
                mCamera->mAPIOwner, mCamera->mEventOwner);
            mCamera->Lock(
                &mCamera->mLock,
                &mCamera->mAPIOwner,
                &mCamera->mAPICond);
            ALOGVV("APILock-- , APIOwner = %p, eventOwner=%p",
                mCamera->mAPIOwner, mCamera->mEventOwner);
        }
        inline ~APILock()
        {
            ALOGVV("~APILock++ , APIOwner = %p, eventOwner=%p",
                mCamera->mAPIOwner, mCamera->mEventOwner);
            mCamera->Unlock(
                &mCamera->mLock,
                &mCamera->mAPIOwner,
                &mCamera->mAPICond);
            ALOGVV("~APILock-- , APIOwner = %p, eventOwner=%p",
                mCamera->mAPIOwner, mCamera->mEventOwner);
        }
    private:
        NvCameraHal *mCamera;
    };

    class EventLock {
    public:
        inline EventLock(NvCameraHal *cam) : mCamera(cam)
        {
            ALOGVV("EventLock++ , APIOwner = %p, eventOwner=%p",
                mCamera->mAPIOwner, mCamera->mEventOwner);
            // if the API thread calls the event handler directly,
            // we shouldn't bother locking, since it can cause deadlock.
            // we already know it's on the same thread, so we don't need
            // to worry about threadsafety.
            if (androidGetThreadId() == mCamera->mAPIOwner)
            {
                ALOGVV("EventLock-- odd, APIOwner = %p, eventOwner=%p",
                    mCamera->mAPIOwner, mCamera->mEventOwner);
                return;
            }
            mCamera->Lock(
                &mCamera->mLock,
                &mCamera->mEventOwner,
                &mCamera->mEventCond);
            ALOGVV("EventLock-- , APIOwner = %p, eventOwner=%p",
                mCamera->mAPIOwner, mCamera->mEventOwner);
        }
        inline ~EventLock()
        {
            ALOGVV("~EventLock++ , APIOwner = %p, eventOwner=%p",
                mCamera->mAPIOwner, mCamera->mEventOwner);
            // if we didn't lock for the special case mentioned above, we
            // shouldn't unlock either.
            if (androidGetThreadId() == mCamera->mAPIOwner)
            {
                ALOGVV("~EventLock-- odd, APIOwner = %p, eventOwner=%p",
                    mCamera->mAPIOwner, mCamera->mEventOwner);
                return;
            }
            mCamera->Unlock(
                &mCamera->mLock,
                &mCamera->mEventOwner,
                &mCamera->mEventCond);
            ALOGVV("~EventLock-- , APIOwner = %p, eventOwner=%p",
                mCamera->mAPIOwner, mCamera->mEventOwner);
        }
    private:
        NvCameraHal *mCamera;
    };

    Condition mPreviewEOSReceived;
    Condition mFirstPreviewFrameReceived;
    Condition mVideoEOSReceived;
    Condition mFirstVideoFrameReceived;

    Condition mAFLockReceived;
    Condition mAELockReceived;
    Condition mAWBLockReceived;

    NvImageScaler mScaler;

    // Surfaces to be used for preview frame-copy callback
    NvMMSurfaceDescriptor ScaledSurfaces;

    NvOsThreadHandle    mANWFailureHandlerThread;
    NvBool mPreviewWindowDestroyed;

    NvOsThreadHandle    mSendJpegDataThread;
    NvMMQueueHandle     mFilledOutputBufferQueue;
    NvMMQueueHandle     mEmptyOutputBufferQueue;
    NvOsSemaphoreHandle     mhSemFilledOutputBufAvail;
    NvOsSemaphoreHandle     mhSemEmptyOutputBufAvail;
    NvOsMutexHandle     mStillCaptureMutex;

    NvOsSemaphoreHandle     mhSemPictureRequest;
    void debugBufferCfg(char *msg, NvMMNewBufferConfigurationInfo *BufCfg);
    void debugBuffer(char *msg, NvMMBuffer *Buf);
    // variables for setting of:
    // advanced noise reduction
    NvBool mIsVideo;
    AnrMode mAnrMode;
    /*avoid setting deadlock*/
    NvOsMutexHandle settingsLock;

    NvBool mIsCapturing;
    NvBool mIsTorchConverged;
    NvBool mGotAeComplete;
    NvBool mGotAwbComplete;
    NvBool mGotAfComplete;
    // AE/AWB is locked when flash is used. They need to be unlocked either in
    // cancelAutoFocus or after snapshot (stillCaptureReady event)
    NvBool mAeAwbUnlockNeeded;
    NvBool mCancelFocusPending;
    Condition mCaptureDone;
    Condition mJpegBufferReturnedToPostProc;
    // Default focus region used when user input is all zero
    NvCameraIspFocusingRegions mDefFocusRegions;

    // Sometimes we want to update camera block in response to some events.
    // For example, we need to update AE/AF region in response to DZ factor
    // change. However, calling camera block function in event handler has
    // a chance of causing deadlock when another API is being called
    // simultaneously. The solution to this problem is to create a new thread
    // to process those NvMM calls.
    NvOsThreadHandle mEventsUpdateCameraBlockThread;
    // A mutex to protect mEventsUpdateCameraBlockThread among multiple threads
    mutable Mutex mLockEventsUpdateCameraState;
    // The current event that is using mEventsUpdateCameraBlockThread to update
    // camera block. It is guaranteed that only one event can use this
    // thread at a time.
    NvU32 mEventTypeUpdatingCameraBlock;

    NvOsThreadHandle mSensorSetupThreadHandle;

    // default settings queried and stored in HAL constructor
    NvCombinedCameraSettings mDefaultSettings;
    // used to store backups of settings when something needs to stash
    // them temporarily (when ANR is disabled for HDR, for example, and
    // needs to be restored to its last known value once HDR is turned
    // off again).
    // to use the settings cache with other settings, the settings should
    // handle updating the cache when they are updated / overwritten, and
    // they should keep track of whether they are in a "cached" or "active"
    // state, to avoid double-cacheing conflicts between two settings.
    NvCombinedCameraSettings mSettingsCache;

    // state to track whether ANR setting has been cached as part of
    // another setting's operation, or whether it is currently active
    // and able to be cached.  helps avoid double-cacheing conflicts.
    NvBool mANRCached;
    NvBool mWBModeCached;

    NvFrameCopyDataCallback *mPreviewFrameCopy;
    NvFrameCopyDataCallback *mPostviewFrameCopy;

    NvS32 mStillCaptureCount;
    NvBool mStillCaptureStop;
    Condition mStillCaptureFinished;
    NvOsMutexHandle mStillCaptureStopMutex;

    NvBool mAELocked;
    NvBool mAWBLocked;

    NvMMCameraStreamIndex mCameraStreamIndex_OutputPreview;
    NvMMCameraStreamIndex mCameraStreamIndex_Output;

    sp<NvSensorListener> mSensorListener;
    NvS32 mOrientation;
    NvS32 mTilt;

    NvOsThreadHandle mThreadToHandleFaceDetector;
    camera_frame_metadata_t *mFaceMetaData;
    mutable Mutex mLockFdState;
    FaceDetectionState mFdState;
    FaceDetectionState mFdReq;
    NvBool mFdPaused;
    NvBool mNeedToReset3AWindows;
    NvU32 mNumFaces;
    Face *mpFaces;
    NvCameraUserWindow mLastFaceRegions[MAX_FACES_DETECTED];

    // Post processing classes
    NvCameraPostProcessStill *mPostProcessStill;
    NvCameraPostProcessVideo *mPostProcessVideo;
    NvCameraPostProcessPreview *mPostProcessPreview;
    NvOsThreadHandle mPreviewPostProcThreadHandle;
    NvOsThreadHandle mStillPostProcThreadHandle;
    NvOsThreadHandle mVideoPostProcThreadHandle;
    NvBool mStopPostProcessingPreview;
    NvBool mStopPostProcessingVideo;
    NvOsMutexHandle mPostProcThreadJoinMutex;

    // To make sure that the sensor has enough time
    // after settings are programmed
    NvBool mWaitForManualSettings;
    NvBool mMasterWaitFlagForSettings;
    Condition mMasterWaitForSettingsCond;
    // each parameter/setting can request how many
    // frames we should wait to make sure
    // that it is programmed to the sensor.
    // unit: number of frames to be skipped
    NvU32 mFinalWaitCountInFrames; // decided as max of all the requests
    NvU32 mCurrentPreviewFramesForWait;

    NvCameraCapabilities mDefaultCapabilities;
    // for sending postview callback of bracket capture
    NvU32 mNumBracketPreviewRecieved;

    NvBool mDNGEnabled;

    NvF32 mRawCaptureCCT;
    NvF32 mRawCaptureWB[4];
    NvF32 mRawCaptureGain;
    NvF32 mRawCaptureCCM3000[16];
    NvF32 mRawCaptureCCM6500[16];
};

class NvFrameCopyDataCallback {
 public:
     NvFrameCopyDataCallback(NvCameraHal *cam, NvS32 CallbackId);
     ~NvFrameCopyDataCallback()
     {
         NV_ASSERT(!mFrameCopyInProgress && !mThreadForFrameCopy);
     }
     NvBool Enabled() { return mEnabled; }
     void Enable(NvBool enable) { mEnabled = enable; }
     NvError DoFrameCopyAndCallback(NvMMBuffer *pBuffer, NvBool SendBufferDownstream);
     void CheckAndWaitWorkDone();

 private:
     static void FrameCopyCallbackThread(void *args);
     NvCameraHal *mCamera;
     NvS32  mCallbackId;
     NvBool mEnabled;
     NvBool mFrameCopyInProgress;
     Condition mFrameCopyDone;
     NvOsThreadHandle mThreadForFrameCopy;
 };

typedef struct PostProcThreadArgs_t
{
    NvCameraHal *This;
    NvMMBuffer Buffer;
    NvU32 StreamIndex;
} PostProcThreadArgs;

} // namespace android

#endif // NV_CAMERA_HAL_H

