/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 *
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



#define LOG_TAG "NvCameraHalAPI"

#define LOG_NDEBUG 0

#define ALLOCATE_BUFFERS_ON_BOOT_DEFAULT_CAMERA 0
#define ALLOCATE_BUFFERS_ON_BOOT_ENABLED 0

#include "nvcamerahal.h"
#include <stdarg.h>

static double GetCurrentTime()
{
    struct timeval curr_timeval;
    gettimeofday(&curr_timeval, NULL);
    return curr_timeval.tv_sec + curr_timeval.tv_usec/1000000.0;
}

static NvBool IsGrinderEnabled()
{
#define GRINDER_ENABLE_BIT 0x1
    NvBool enable = NV_FALSE;
    char value[PROPERTY_VALUE_MAX];
    property_get("camera.grinder.flags", value, "0");
    enable = ((atoi(value) & GRINDER_ENABLE_BIT) != 0);
    if (enable)
    {
        ALOGVV("NvCamGrinder Recorder is enabled");
    }
    else
    {
        ALOGVV("NvCamGrinder Recorder is disabled");
    }

    return enable;
}

class NvCamGrinderRecorder
{
#define NV_CAMERA_GRINDER_PROFILE_FILE "/data/nvcam/cameraprofile.log"
public:
    NvCamGrinderRecorder() {mLoggerFile = NULL;};
    ~NvCamGrinderRecorder() {StopRecording();};

    static NvCamGrinderRecorder* Instantiate()
    {
        Mutex::Autolock lock(&mInstanceLock);
        // If enabled, but not instantiated, then we create the object
        if (!mInstance)
            mInstance = new NvCamGrinderRecorder();
        ALOGVV("NvCamGrinder %s mInstance %p", __FUNCTION__, mInstance);
        return mInstance;
    };

    static void Release()
    {
        Mutex::Autolock lock(&mInstanceLock);
        if (mInstance)
            delete mInstance;
        mInstance = NULL;
        return;
    }

    void StartRecording()
    {
        Mutex::Autolock lock(&mFileLock);
        // If file is closed, open it first
        if (!mLoggerFile)
            mLoggerFile = fopen(NV_CAMERA_GRINDER_PROFILE_FILE, "a");

        // Although it's multithread safe, it seems to crash if
        // we write to file right after we create it. Have to put
        // a manual sleep here.
        NvOsSleepMS(500);

        if (!mLoggerFile)
        {
            ALOGE("NvCamGrinder failed creating %s",
                NV_CAMERA_GRINDER_PROFILE_FILE);
        }
        else
        {
            ALOGVV("NvCamGrinder opens file %s",
                NV_CAMERA_GRINDER_PROFILE_FILE);
        }
    }

    void StopRecording()
    {
        Mutex::Autolock lock(&mFileLock);
        if (mLoggerFile)
        {
            fclose(mLoggerFile);
            ALOGVV("NvCamGrinder closes file %s",
                NV_CAMERA_GRINDER_PROFILE_FILE);
            mLoggerFile = NULL;
        }
    }

    void Record(const char* format, ...)
    {
        Mutex::Autolock lock(&mFileLock);
        if (mLoggerFile)
        {
            va_list args;
            va_start(args, format);
            vfprintf(
                mLoggerFile,
                format,
                args);
            va_end(args);
        }
    }

private:
    static NvCamGrinderRecorder* mInstance;
    static Mutex mInstanceLock;
    FILE *mLoggerFile;
    Mutex mFileLock;
};

NvCamGrinderRecorder* NvCamGrinderRecorder::mInstance = NULL;
Mutex NvCamGrinderRecorder::mInstanceLock;

static NvCamGrinderRecorder* pGrinderRecorder = NULL;
#define GRINDER_LOG(...) \
    { \
        if (pGrinderRecorder) { \
            pGrinderRecorder->Record(__VA_ARGS__); \
        } \
    }

// If grinder recorder is enabled, but not instantiated, then create
// the singleton. If already instantiated, then just start recording.
// If grinder recorder is disabled, we need to delete it if instantiated.
#define GRINDER_START() \
    { \
        if (IsGrinderEnabled()) { \
            if (!pGrinderRecorder) { \
                pGrinderRecorder = \
                    NvCamGrinderRecorder::Instantiate(); \
            } \
            if (pGrinderRecorder) { \
                pGrinderRecorder->StartRecording(); \
            } \
        } \
        else { \
            if (pGrinderRecorder) { \
                NvCamGrinderRecorder::Release(); \
                pGrinderRecorder = NULL; \
            } \
        } \
    }

// Don't check the enable sign here to avoid potential race condition.
#define GRINDER_STOP() \
    { \
        if (pGrinderRecorder) { \
            pGrinderRecorder->StopRecording(); \
        } \
    }

// this file links the NvCameraHal object's public methods to functions
// that are called directly by the android camera framework

namespace android {

NvCameraHalLogging mCameraLogging;

static NvCameraHal_CameraInfo *sCameraInfo = NULL;
static int NvCameraHal_numCameras = 0;
static Mutex infoLock;

static int HAL_getCameraInfo(int cameraId, struct camera_info* cameraInfo);
#ifdef FRAMEWORK_HAS_EXTENDED_CAM_INFO_SUPPORT
static int HAL_getCameraInfoExtended(int cameraId, struct camera_info_extended* cameraInfo);
#endif
static NvBool checkUSBexistence();

NvCameraHal_CameraType getCamType(int cameraId)
{
    NvCameraHal_CameraType ret;
    ret = sCameraInfo[cameraId].camType;
    return ret;
}

static int HAL_getNumberOfCameras_withLock(void)
{
    ALOGVV("%s++\n", __func__);

    // do camera device auto-detection
    NvMMCameraDeviceDetect();
    // possibly stale data so always parse
    if (NvCameraHal_numCameras > 0)       // clean up old data
    {
        if (sCameraInfo)
        {
            for (int i = 0; i < NvCameraHal_numCameras; i++)
            {
                NvOsFree(sCameraInfo[i].deviceLocation);
                sCameraInfo[i].deviceLocation = NULL;
            }
            NvOsFree(sCameraInfo);
            sCameraInfo = NULL;
        }
    }
    NvCameraHal_numCameras = NvCameraHal_parseCameraConf(&sCameraInfo, checkUSBexistence());

#if ALLOCATE_BUFFERS_ON_BOOT_ENABLED
    {
        char value[PROPERTY_VALUE_MAX];
        property_get("nv-camera-no-buffers-on-boot", value, "0");
        NvS32 disable = atoi(value);

        if (!disable && NvCameraHal::mBuffersInitialized == NV_FALSE)
        {
            NvCameraHal *pPreInitHal =
                    new NvCameraHal(ALLOCATE_BUFFERS_ON_BOOT_DEFAULT_CAMERA, NULL);
            pPreInitHal->release();
            delete pPreInitHal;
        }
    }
#endif

    ALOGVV("%s--\n", __func__);
    return NvCameraHal_numCameras;
}

static int HAL_getNumberOfCameras(void)
{
    int ret;
    ALOGVV("%s ++\n", __func__);
    infoLock.lock();
    ret = HAL_getNumberOfCameras_withLock();
    infoLock.unlock();
    ALOGVV("%s --\n", __func__);
    return ret;
};

static int HAL_getCameraInfo(int cameraId, struct camera_info* cameraInfo)
{
    ALOGVV("%s++\n", __func__);

    status_t error = NO_ERROR;

    infoLock.lock();

    if (cameraId >= NvCameraHal_numCameras)    // parse the config file if Id out of range
        HAL_getNumberOfCameras_withLock();

    if (cameraId < NvCameraHal_numCameras)    // error if Id still out of range
    {
        if (sCameraInfo)
        {
            memcpy(cameraInfo, &(sCameraInfo[cameraId].camInfo), sizeof(struct camera_info));
        }
        else
        {
            error = BAD_VALUE;
        }
    }

    infoLock.unlock();

    ALOGVV("%s--\n", __func__);

    return error;
}

NvBool checkUSBexistence()
{
    int fd;
    NvBool sts = NV_TRUE;

    /* open first device */
    fd = open ("/dev/video0", O_RDWR | O_NONBLOCK, 0);

    if (fd < 0)
    {
        /* First device opening failed so  trying with the next device */
        fd = open ("/dev/video1", O_RDWR | O_NONBLOCK, 0);
        if (fd < 0)
        {
            sts = NV_FALSE;
        }
    }

    if (fd >= 0)
    {
        close(fd);
    }

    return sts;
}


#ifdef FRAMEWORK_HAS_EXTENDED_CAM_INFO_SUPPORT
static int
HAL_getCameraInfoExtended(
    int cameraId,
    struct camera_info_extended* cameraInfoExtended)
{
    ALOGVV("%s++\n", __func__);

    infoLock.lock();

    if (cameraId >= NvCameraHal_numCameras)    // parse the config file if Id out of range
        HAL_getNumberOfCameras_withLock();

    if (cameraId < NvCameraHal_numCameras)    // error if Id still out of range
    {
        // Take basic parts from basic structure
        cameraInfoExtended->facing = sCameraInfo[cameraId].camInfo.facing;
        cameraInfoExtended->orientation = sCameraInfo[cameraId].camInfo.orientation;

        // Take extended parts from NVIDIA extended structure
        // TODO Current format assumes that both stereo capabilities and
        // connection type are all the same category. This doesn't support
        // very probable use-case where there is USB stereo camera.
        // Version 2 of conf file needs to use separate fields
        // for stereo caps and connection type.
        cameraInfoExtended->stereoCaps = CAMERA_STEREO_CAPS_UNDEFINED;
        cameraInfoExtended->connection = CAMERA_CONNECTION_UNDEFINED;

        // Currently we know only about built-in stereo cameras
        if (NVCAMERAHAL_CAMERATYPE_STEREO == sCameraInfo[cameraId].camType)
        {
            cameraInfoExtended->stereoCaps = CAMERA_STEREO_CAPS_STEREO;
            cameraInfoExtended->connection = CAMERA_CONNECTION_INTERNAL;
        }
        // Currently we know only about mono USB cameras
        else if (NVCAMERAHAL_CAMERATYPE_USB == sCameraInfo[cameraId].camType)
        {
            cameraInfoExtended->stereoCaps = CAMERA_STEREO_CAPS_MONO;
            cameraInfoExtended->connection = CAMERA_CONNECTION_USB;
        }
        // Currently we assume that mono camera is built-in
        else if (NVCAMERAHAL_CAMERATYPE_MONO == sCameraInfo[cameraId].camType)
        {
            cameraInfoExtended->stereoCaps = CAMERA_STEREO_CAPS_MONO;
            cameraInfoExtended->connection = CAMERA_CONNECTION_INTERNAL;
        }
        // Default to undefined for all other cases
        else
        {
            cameraInfoExtended->stereoCaps = CAMERA_STEREO_CAPS_UNDEFINED;
            cameraInfoExtended->connection = CAMERA_CONNECTION_UNDEFINED;
        }
    }

    infoLock.unlock();
    ALOGVV("%s--\n", __func__);
    return 0;
}
#endif

static inline NvCameraHal *obj(struct camera_device *dev)
{
    return reinterpret_cast<NvCameraHal *>(dev->priv);
}

/** Set the preview_stream_ops to which preview frames are sent */
static int
HAL_camera_device_set_preview_window(
    struct camera_device *dev,
    struct preview_stream_ops *buf)
{
    GRINDER_LOG("%lf %s\n", GetCurrentTime(), __FUNCTION__);
    return obj(dev)->setPreviewWindow(buf);
}

/** Set the notification and data callbacks */
static void
HAL_camera_device_set_callbacks(
    struct camera_device *dev,
    camera_notify_callback notify_cb,
    camera_data_callback data_cb,
    camera_data_timestamp_callback data_cb_timestamp,
    camera_request_memory get_memory,
    void* user)
{
    GRINDER_LOG("%lf %s\n", GetCurrentTime(), __FUNCTION__);
    obj(dev)->setCallbacks(notify_cb, data_cb, data_cb_timestamp,
                           get_memory, user);
}

/**
 * The following three functions all take a msg_type, which is a bitmask of
 * the messages defined in include/ui/Camera.h
 */

/**
 * Enable a message, or set of messages.
 */
static void
HAL_camera_device_enable_msg_type(
    struct camera_device *dev,
    int32_t msg_type)
{
    GRINDER_LOG("%lf %s %d\n", GetCurrentTime(), __FUNCTION__, msg_type);
            NvOsDebugPrintf("NvCamGrinder %s 3", __FUNCTION__); \
            NvOsDebugPrintf("NvCamGrinder %s 3", __FUNCTION__); \
    obj(dev)->enableMsgType(msg_type);
}

/**
 * Disable a message, or a set of messages.
 *
 * Once received a call to disableMsgType(CAMERA_MSG_VIDEO_FRAME), camera
 * HAL should not rely on its client to call releaseRecordingFrame() to
 * release video recording frames sent out by the cameral HAL before and
 * after the disableMsgType(CAMERA_MSG_VIDEO_FRAME) call. Camera HAL
 * clients must not modify/access any video recording frame after calling
 * disableMsgType(CAMERA_MSG_VIDEO_FRAME).
 */
static void
HAL_camera_device_disable_msg_type(
    struct camera_device *dev,
    int32_t msg_type)
{
    GRINDER_LOG("%lf %s %d\n", GetCurrentTime(), __FUNCTION__, msg_type);
    obj(dev)->disableMsgType(msg_type);
}

/**
 * Query whether a message, or a set of messages, is enabled.  Note that
 * this is operates as an AND, if any of the messages queried are off, this
 * will return false.
 */
static int
HAL_camera_device_msg_type_enabled(
    struct camera_device *dev,
    int32_t msg_type)
{
    GRINDER_LOG("%lf %s %d\n", GetCurrentTime(), __FUNCTION__, msg_type);
    return obj(dev)->msgTypeEnabled(msg_type);
}

/**
 * Start preview mode.
 */
static int HAL_camera_device_start_preview(struct camera_device *dev)
{
    GRINDER_LOG("%lf %s\n", GetCurrentTime(), __FUNCTION__);
    return obj(dev)->startPreview();
}

/**
 * Stop a previously started preview.
 */
static void HAL_camera_device_stop_preview(struct camera_device *dev)
{
    GRINDER_LOG("%lf %s\n", GetCurrentTime(), __FUNCTION__);
    obj(dev)->stopPreview();
}

/**
 * Returns true if preview is enabled.
 */
static int HAL_camera_device_preview_enabled(struct camera_device *dev)
{
    GRINDER_LOG("%lf %s\n", GetCurrentTime(), __FUNCTION__);
    return obj(dev)->previewEnabled();
}

/**
 * Request the camera HAL to store meta data or real YUV data in the video
 * buffers sent out via CAMERA_MSG_VIDEO_FRAME for a recording session. If
 * it is not called, the default camera HAL behavior is to store real YUV
 * data in the video buffers.
 *
 * This method should be called before startRecording() in order to be
 * effective.
 *
 * If meta data is stored in the video buffers, it is up to the receiver of
 * the video buffers to interpret the contents and to find the actual frame
 * data with the help of the meta data in the buffer. How this is done is
 * outside of the scope of this method.
 *
 * Some camera HALs may not support storing meta data in the video buffers,
 * but all camera HALs should support storing real YUV data in the video
 * buffers. If the camera HAL does not support storing the meta data in the
 * video buffers when it is requested to do do, INVALID_OPERATION must be
 * returned. It is very useful for the camera HAL to pass meta data rather
 * than the actual frame data directly to the video encoder, since the
 * amount of the uncompressed frame data can be very large if video size is
 * large.
 *
 * @param enable if true to instruct the camera HAL to store
 *      meta data in the video buffers; false to instruct
 *      the camera HAL to store real YUV data in the video
 *      buffers.
 *
 * @return OK on success.
 */
static int
HAL_camera_device_store_meta_data_in_buffers(
    struct camera_device *dev,
    int enable)
{
    GRINDER_LOG("%lf %s %d\n", GetCurrentTime(), __FUNCTION__, enable);
    return obj(dev)->storeMetaDataInBuffers(enable);
}

/**
 * Start record mode. When a record image is available, a
 * CAMERA_MSG_VIDEO_FRAME message is sent with the corresponding
 * frame. Every record frame must be released by a camera HAL client via
 * releaseRecordingFrame() before the client calls
 * disableMsgType(CAMERA_MSG_VIDEO_FRAME). After the client calls
 * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
 * responsibility to manage the life-cycle of the video recording frames,
 * and the client must not modify/access any video recording frames.
 */
static int HAL_camera_device_start_recording(struct camera_device *dev)
{
    GRINDER_LOG("%lf %s\n", GetCurrentTime(), __FUNCTION__);
    return obj(dev)->startRecording();
}

/**
 * Stop a previously started recording.
 */
static void HAL_camera_device_stop_recording(struct camera_device *dev)
{
    GRINDER_LOG("%lf %s\n", GetCurrentTime(), __FUNCTION__);
    obj(dev)->stopRecording();
}

/**
 * Returns true if recording is enabled.
 */
static int HAL_camera_device_recording_enabled(struct camera_device *dev)
{
    GRINDER_LOG("%lf %s\n", GetCurrentTime(), __FUNCTION__);
    return obj(dev)->recordingEnabled();
}

/**
 * Release a record frame previously returned by CAMERA_MSG_VIDEO_FRAME.
 *
 * It is camera HAL client's responsibility to release video recording
 * frames sent out by the camera HAL before the camera HAL receives a call
 * to disableMsgType(CAMERA_MSG_VIDEO_FRAME). After it receives the call to
 * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
 * responsibility to manage the life-cycle of the video recording frames.
 */
static void
HAL_camera_device_release_recording_frame(
    struct camera_device *dev,
    const void *opaque)
{
    GRINDER_LOG("%lf %s\n", GetCurrentTime(), __FUNCTION__);
    obj(dev)->releaseRecordingFrame(opaque);
}

/**
 * Start auto focus, the notification callback routine is called with
 * CAMERA_MSG_FOCUS once when focusing is complete. autoFocus() will be
 * called again if another auto focus is needed.
 */
static int HAL_camera_device_auto_focus(struct camera_device *dev)
{
    GRINDER_LOG("%lf %s\n", GetCurrentTime(), __FUNCTION__);
    return obj(dev)->autoFocus();
}

/**
 * Cancels auto-focus function. If the auto-focus is still in progress,
 * this function will cancel it. Whether the auto-focus is in progress or
 * not, this function will return the focus position to the default.  If
 * the camera does not support auto-focus, this is a no-op.
 */
static int HAL_camera_device_cancel_auto_focus(struct camera_device *dev)
{
    GRINDER_LOG("%lf %s\n", GetCurrentTime(), __FUNCTION__);
    return obj(dev)->cancelAutoFocus();
}
/**
 * Take a picture.
 */
static int HAL_camera_device_take_picture(struct camera_device *dev)
{
    GRINDER_LOG("%lf %s\n", GetCurrentTime(), __FUNCTION__);
    return obj(dev)->takePicture();
}

/**
 * Cancel a picture that was started with takePicture. Calling this method
 * when no picture is being taken is a no-op.
 */
static int HAL_camera_device_cancel_picture(struct camera_device *dev)
{
    GRINDER_LOG("%lf %s\n", GetCurrentTime(), __FUNCTION__);
    return obj(dev)->cancelPicture();
}

/**
 * Set the camera parameters. This returns BAD_VALUE if any parameter is
 * invalid or not supported.
 */
static int
HAL_camera_device_set_parameters(
    struct camera_device *dev,
    const char *parms)
{
    GRINDER_LOG("%lf %s %s\n", GetCurrentTime(), __FUNCTION__, parms);
    String8 str(parms);
    CameraParameters p(str);
    return obj(dev)->setParameters(p, NV_FALSE);
}


#ifdef FRAMEWORK_HAS_EXTENDED_CAM_INFO_SUPPORT
/**
 * Set the camera parameters. This returns BAD_VALUE if any parameter is
 * invalid or not supported.
 */
static int
HAL_camera_device_set_custom_parameters(
    struct camera_device *dev,
    const char *parms)
{
    GRINDER_LOG("%lf %s %s\n", GetCurrentTime(), __FUNCTION__, parms);
    String8 str(parms);
    CameraParameters p(str);
    return obj(dev)->setParameters(p, NV_TRUE);
}
#endif

/** Return the camera parameters. */
static char *
HAL_camera_device_get_parameters(
    struct camera_device *dev)
{
    GRINDER_LOG("%lf %s\n", GetCurrentTime(), __FUNCTION__);
    String8 str;
    CameraParameters params = obj(dev)->getParameters(NV_FALSE);
    str = params.flatten();
    return strdup(str.string());
}

#ifdef FRAMEWORK_HAS_EXTENDED_CAM_INFO_SUPPORT
/** Return the camera parameters. */
static char *
HAL_camera_device_get_custom_parameters(
    struct camera_device *dev)
{
    GRINDER_LOG("%lf %s\n", GetCurrentTime(), __FUNCTION__);
    String8 str;
    CameraParameters params = obj(dev)->getParameters(NV_TRUE);
    str = params.flatten();
    return strdup(str.string());
}
#endif

/** Return the camera parameters. */
static void
HAL_camera_device_put_parameters(
    struct camera_device *dev,
    char *params)
{
    free(params);
}

/**
 * Send command to camera driver.
 */
static int
HAL_camera_device_send_command(
    struct camera_device *dev,
    int32_t cmd,
    int32_t arg1,
    int32_t arg2)
{
    GRINDER_LOG("%lf %s %d %d %d\n", GetCurrentTime(), __FUNCTION__,\
        cmd, arg1, arg2);
    return obj(dev)->sendCommand(cmd, arg1, arg2);
}

/**
 * Release the hardware resources owned by this object.  Note that this is
 * *not* done in the destructor.
 */
static void HAL_camera_device_release(struct camera_device *dev)
{
    GRINDER_LOG("%lf %s\n", GetCurrentTime(), __FUNCTION__);
    obj(dev)->release();
}

/**
 * Dump state of the camera hardware
 */
static int HAL_camera_device_dump(struct camera_device *dev, int fd)
{
    GRINDER_LOG("%lf %s\n", GetCurrentTime(), __FUNCTION__);
    return obj(dev)->dump(fd);
}

#define SET_METHOD(m) m : HAL_camera_device_##m
camera_device_ops_t camera_device_ops = {
        SET_METHOD(set_preview_window),
        SET_METHOD(set_callbacks),
        SET_METHOD(enable_msg_type),
        SET_METHOD(disable_msg_type),
        SET_METHOD(msg_type_enabled),
        SET_METHOD(start_preview),
        SET_METHOD(stop_preview),
        SET_METHOD(preview_enabled),
        SET_METHOD(store_meta_data_in_buffers),
        SET_METHOD(start_recording),
        SET_METHOD(stop_recording),
        SET_METHOD(recording_enabled),
        SET_METHOD(release_recording_frame),
        SET_METHOD(auto_focus),
        SET_METHOD(cancel_auto_focus),
        SET_METHOD(take_picture),
        SET_METHOD(cancel_picture),
        SET_METHOD(set_parameters),
        SET_METHOD(get_parameters),
        SET_METHOD(put_parameters),
        SET_METHOD(send_command),
        SET_METHOD(release),
        SET_METHOD(dump),
#ifdef FRAMEWORK_HAS_EXTENDED_CAM_INFO_SUPPORT
        SET_METHOD(set_custom_parameters),
        SET_METHOD(get_custom_parameters),
#endif
};

#undef SET_METHOD

static int HAL_camera_device_close(struct hw_device_t* device)
{
    GRINDER_LOG("%lf %s\n", GetCurrentTime(), __FUNCTION__);
    GRINDER_STOP();
    if (device)
    {
        camera_device_t *cam_device = (camera_device_t *)device;
        delete static_cast<NvCameraHal *>(cam_device->priv);
        free(cam_device);
    }
    return 0;
}

static int
HAL_camera_device_open(
    const struct hw_module_t* module,
    const char *id,
    struct hw_device_t** device)
{
    GRINDER_START();
    GRINDER_LOG("%lf %s %s\n", GetCurrentTime(), __FUNCTION__, id);

    mCameraLogging.setPropLogs();

    camera_device_t *cam_device =
        (camera_device_t *)malloc(sizeof(camera_device_t));
    if (!cam_device)
    {
        return -ENOMEM;
    }

    cam_device->common.tag     = HARDWARE_DEVICE_TAG;
    cam_device->common.version = 1;
    cam_device->common.module  = const_cast<hw_module_t *>(module);
    cam_device->common.close   = HAL_camera_device_close;

    cam_device->ops = &camera_device_ops;

    ALOGI("%s: opening camera %s", __FUNCTION__, id);

    int cameraId = atoi(id);
    int orientation = 0;

    infoLock.lock();

    if (cameraId < 0 || cameraId >= HAL_getNumberOfCameras_withLock())
    {
        ALOGE("Invalid camera ID %s", id);
        free(cam_device);
        infoLock.unlock();
        return -EINVAL;
    }

    orientation = sCameraInfo[cameraId].camInfo.orientation;
    cam_device->priv = new NvCameraHal((NvU32)cameraId, orientation, cam_device);

    if ((static_cast<NvCameraHal *>(cam_device->priv))->GetConstructorError())
    {
        ALOGE("%s: failed to open camera %s", __FUNCTION__, id);
        delete (static_cast<NvCameraHal *> (cam_device->priv));
        free(cam_device);
        infoLock.unlock();
        return EBUSY;
    }

    *device = (hw_device_t *)cam_device;

    infoLock.unlock();
    ALOGI("%s: opened camera %s (%p)", __FUNCTION__, id, *device);

    return 0;
}

#if NV_CAMERA_V3
int HAL_real_camera_device_open(
    const struct hw_module_t* module,
    const char *id,
    struct hw_device_t** device);


int HAL_real_camera_device_open(
    const struct hw_module_t* module,
    const char *id,
    struct hw_device_t** device)
{
    return HAL_camera_device_open(module, id,device);
}
#endif

hw_module_methods_t camera_module_methods = {
        open : HAL_camera_device_open
};

extern "C" {
    struct camera_module HAL_MODULE_INFO_SYM = {
            common : {
                tag           : HARDWARE_MODULE_TAG,
                version_major : 1,
                version_minor : 0,
                id            : CAMERA_HARDWARE_MODULE_ID,
                name          : "NVIDIA Development Platform Camera HAL",
                author        : "NVIDIA Corporation",
                methods       : &camera_module_methods,
                dso           : NULL,
                reserved      : {0},
            },
            get_number_of_cameras     : HAL_getNumberOfCameras,
            get_camera_info           : HAL_getCameraInfo,
#ifdef PLATFORM_IS_JELLYBEAN_MR2
            set_callbacks               : NULL,
#endif
#ifdef FRAMEWORK_HAS_EXTENDED_CAM_INFO_SUPPORT
            get_camera_info_extended  : HAL_getCameraInfoExtended
#endif
    };
}

} // namespace android
