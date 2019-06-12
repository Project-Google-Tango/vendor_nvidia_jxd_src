/*
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvcamerahal3.h"
#include "nvcamerahal3common.h"
#include "nvcamerahal3_tags.h"
#include "nv_log.h"
#include "camera_trace.h"

// this file links the NvCameraHal object's public methods to functions
// that are called directly by the android camera framework

namespace android {

static NvCameraHal_CameraInfo *sCameraInfo = NULL;
static int NvCameraHal_numCameras = 0;
static NvBool NvCameraHal_CamerasDetected = NV_FALSE;
static Mutex infoLock;
static NvBool checkUSBexistence();
NvU64 camGUID[MAX_SENSORS_SUPPORTED];

NvCameraHalLogging mCameraLogging;

NvCameraHal_CameraType getCamType(int cameraId)
{
    NV_TRACE_CALL_D(HAL3_API_TAG);
    NvCameraHal_CameraType ret;
    Mutex::Autolock lock(infoLock);
    ret = sCameraInfo[cameraId].camType;
    return ret;
}

static int HAL_getNumberOfCameras_withLock(void)
{
    NV_TRACE_CALL_D(HAL3_API_TAG);
    NV_LOGD(HAL3_API_TAG, "%s: ++\n", __FUNCTION__);
    int numCameras = 0;

    // check only once for permanently connected sensors
    if (NvCameraHal_CamerasDetected == NV_FALSE)
    {
        // do camera device auto-detection
        NvMMCameraDeviceDetect();
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
        NvCameraHal_numCameras = 0;

        //sCameraInfo is allocated in NvCameraHal_parseCameraConf().
        numCameras = NvCameraHal_parseCameraConf(&sCameraInfo);

        for (int i = 0; i < numCameras; i++)
        {
            NvError res = NvSuccess;
            NvCameraCoreHandle hCore;
            int guid;

            if (checkUSBexistence() && i == (numCameras - 1))
                guid = USB_CAMERA_GUID;
            else
                guid = i;

            res = NvCameraCore_Open(&hCore, guid);

            if (res == NvSuccess)
            {
                camGUID[NvCameraHal_numCameras] = guid;
                sCameraInfo[NvCameraHal_numCameras].camInfo.device_version = CAMERA_DEVICE_API_VERSION_3_0;
                NvCameraHal_numCameras++;
                NvCameraCore_Close(hCore);
            }
        }
        NvCameraHal_CamerasDetected = NV_TRUE;
    }
    else
    {
        // if queried again only check for pluggable cameras like usbcamera
        NvError res = NvSuccess;
        NvCameraCoreHandle hCore;
        int guid = USB_CAMERA_GUID;
        int i = 0;
        NvBool usbAlreadyDetected = NV_FALSE;

        // check if usb camera already detected in the past
        for (i; i < NvCameraHal_numCameras; i++)
        {
            if (camGUID[i] == USB_CAMERA_GUID)
            {
                usbAlreadyDetected = NV_TRUE;
                break;
            }
        }

        if (usbAlreadyDetected)
        {
            if(checkUSBexistence())
                return NvCameraHal_numCameras;
            else
                return (NvCameraHal_numCameras-1); // usb device plugged out
        }
        else
        {
            numCameras = NvCameraHal_numCameras;
            NvCameraHal_UpdateUsbExistence(&sCameraInfo,&numCameras);

            if (numCameras == NvCameraHal_numCameras)
                return NvCameraHal_numCameras; // no new camera detected

            res = NvCameraCore_Open(&hCore, guid);

            if (res == NvSuccess)
            {
                camGUID[NvCameraHal_numCameras] = guid;
                sCameraInfo[NvCameraHal_numCameras].camInfo.device_version = CAMERA_DEVICE_API_VERSION_3_0;
                NvCameraHal_numCameras++;
                NvCameraCore_Close(hCore);
            }
        }
    }

    NV_LOGD(HAL3_API_TAG, "%s: --\n", __FUNCTION__);
    return NvCameraHal_numCameras;
}

static int HAL_getNumberOfCameras(void)
{
    int ret;
    NV_TRACE_CALL_D(HAL3_API_TAG);
    NV_LOGD(HAL3_API_TAG, "%s: ++\n", __func__);
    // Lock the critical region where region gets freed and then reallocated.
    // When multiple threads are spawned by camera service, this might result
    // in unexpected behavior, ending up in corrupt data inside scameraInfo
    Mutex::Autolock lock(infoLock);
    ret = HAL_getNumberOfCameras_withLock();
    NV_LOGD(HAL3_API_TAG, "%s: --\n", __func__);
    return ret;
};

static int HAL_set_callbacks(const camera_module_callbacks_t *callbacks)
{
    return 0;
}

static void HAL_getVendorTagOps(vendor_tag_ops_t* ops)
{
    NV_LOGV(HAL3_API_TAG, "%s: Not implemented", __FUNCTION__);
    //TODO
}

static int HAL_getCameraInfo(int cameraId, struct camera_info* cameraInfo)
{
    NV_TRACE_CALL_D(HAL3_API_TAG);
    NV_LOGD(HAL3_API_TAG, "%s: ++ for cameraId[%d]\n", __func__, cameraId);
    status_t error = NO_ERROR;

    Mutex::Autolock lock(infoLock);

    if (cameraId >= NvCameraHal_numCameras)    // parse the config file if Id out of range
        HAL_getNumberOfCameras_withLock();

    if (cameraId < NvCameraHal_numCameras && cameraId >= 0)    // error if Id still out of range
    {
        if (sCameraInfo)
        {
            error = NvCameraHal3::getStaticCharacteristics(cameraId,
                    sCameraInfo[cameraId].camInfo.orientation,
                    sCameraInfo[cameraId].camInfo.static_camera_characteristics);
            if (error != OK)
            {
                NV_LOGE("%s: Unable to allocate static info: %s (%d)",
                    __FUNCTION__, strerror(-error), error);
                return BAD_VALUE;
            }

            memcpy(cameraInfo, &(sCameraInfo[cameraId].camInfo), sizeof(struct camera_info));
        }
        else
        {
            error = BAD_VALUE;
        }
    }
    else
    {
        error = NO_INIT;
    }
    NV_LOGD(HAL3_API_TAG, "%s: --\n", __func__);
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
        /* First device opening failed so trying with the next device */
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

static inline NvCameraHal3 *obj(const struct camera3_device *dev)
{
    return reinterpret_cast<NvCameraHal3 *>(dev->priv);
}
static int HAL_camera_device_initialize(const struct camera3_device *dev,
        const camera3_callback_ops_t *callback_ops) {

    return obj(dev)->initialize(callback_ops);
}

static int HAL_camera_device_configure_streams(const struct camera3_device *dev,
        camera3_stream_configuration_t *stream_list) {
    return obj(dev)->configureStreams(stream_list);
}

static int HAL_camera_device_register_stream_buffers(
        const struct camera3_device *dev,
        const camera3_stream_buffer_set_t *buffer_set) {
    return obj(dev)->registerStreamBuffers(buffer_set);
}

static const camera_metadata_t* HAL_camera_device_construct_default_request_settings(
        const camera3_device_t *dev, int type) {
    return obj(dev)->constructDefaultRequestSettings(type);
}

static int HAL_camera_device_process_capture_request(
        const struct camera3_device *dev,
        camera3_capture_request_t *request) {
    return obj(dev)->processCaptureRequest(request);
}

static void HAL_camera_device_get_metadata_vendor_tag_ops(const camera3_device_t *d,
        vendor_tag_query_ops_t *ops) {
/*
    ops->get_camera_vendor_section_name = get_camera_vendor_section_name;
    ops->get_camera_vendor_tag_name = get_camera_vendor_tag_name;
    ops->get_camera_vendor_tag_type = get_camera_vendor_tag_type;
*/
}

static void HAL_camera_device_dump(const camera3_device_t *d, int fd) {
//    ec->dump(fd);
}

static int HAL_camera_device_flush(const camera3_device_t *dev) {
    return obj(dev)->flush();
}

#define SET_METHOD(m) m : HAL_camera_device_##m


static camera3_device_ops_t camera_device_ops = {
        SET_METHOD(initialize),
        SET_METHOD(configure_streams),
        SET_METHOD(register_stream_buffers),
        SET_METHOD(construct_default_request_settings),
        SET_METHOD(process_capture_request),
        SET_METHOD(get_metadata_vendor_tag_ops),
        SET_METHOD(dump),
        SET_METHOD(flush),
        reserved : {NULL},
};

#undef SET_METHOD

static int HAL_camera_device_close(struct hw_device_t* device)
{
    NV_TRACE_CALL_D(HAL3_API_TAG);
    NV_LOGD(HAL3_API_TAG, "%s: ++", __FUNCTION__);
    // hardcode stub for testing
    if (!device)
    {
        return 0;
    }

    camera3_device_t *cam_device = (camera3_device_t *) device;
    delete static_cast<NvCameraHal3 *>(cam_device->priv);
    cam_device->priv = NULL;
    delete cam_device;
    cam_device = NULL;
    NV_LOGD(HAL3_API_TAG, "%s: --", __FUNCTION__);
    return 0;
}

static int HAL_camera_device_open(
    const struct hw_module_t* module,
    const char *id,
    struct hw_device_t** device)
{
    camera3_device_t *cam_device = new camera3_device_t;

    cam_device->common.tag     = HARDWARE_DEVICE_TAG;
    cam_device->common.version = CAMERA_DEVICE_API_VERSION_3_0;
    cam_device->common.module  = const_cast<hw_module_t *>(module);
    cam_device->common.close   = HAL_camera_device_close;

    cam_device->ops = &camera_device_ops;

    NV_TRACE_CALL_D(HAL3_API_TAG);
    NV_LOGD(HAL3_API_TAG, "%s: ++ with id %s", __FUNCTION__, id);

    int cameraId = atoi(id);

    mCameraLogging.setPropLogs();

    {
        // need the scope of Autlock as we need to unlock before creating
        // NvCameraHal3, as we call getCamType from there, which acquires
        // the lock.
        Mutex::Autolock lock(infoLock);

        if (cameraId < 0 || cameraId >= HAL_getNumberOfCameras_withLock())
        {
            NV_LOGE("Invalid camera ID %s", id);
            delete cam_device; cam_device = NULL;
            return -ENODEV;
        }

        // make sure static properties have been populated before opening HAL
        status_t error = NvCameraHal3::getStaticCharacteristics(cameraId,
                        sCameraInfo[cameraId].camInfo.orientation,
                        sCameraInfo[cameraId].camInfo.static_camera_characteristics);
        if (error != OK)
        {
            NV_LOGE("%s: Unable to allocate static info: %s (%d)",
                __FUNCTION__, strerror(-error), error);
                return BAD_VALUE;
        }
    }

    NvCameraHal3* hal3 = new NvCameraHal3((NvU32)cameraId, cam_device);
    if (OK == hal3->open())
    {
        cam_device->priv = hal3;
        *device = (hw_device_t *)cam_device;
    }
    else
    {
        delete hal3; hal3 = NULL;
        delete cam_device; cam_device = NULL;
        NV_LOGE("%s: Failed to open device: %s", __FUNCTION__, id);
        return -ENODEV;
    }
    NV_LOGD(HAL3_API_TAG, "%s: -- %s (%p)", __FUNCTION__, id, *device);

    return 0;
}

static hw_module_methods_t camera_module_methods = {
        open : HAL_camera_device_open
};

extern "C" {
    struct camera_module HAL_MODULE_INFO_SYM = {
            common : {
                tag                 : HARDWARE_MODULE_TAG,
                module_api_version  : CAMERA_MODULE_API_VERSION_2_1,
                hal_api_version     : HARDWARE_HAL_API_VERSION,
                id                  : CAMERA_HARDWARE_MODULE_ID,
                name                : "NVIDIA Development Platform Camera HAL",
                author              : "NVIDIA Corporation",
                methods             : &camera_module_methods,
                dso                 : NULL,
                reserved            : {0},
            },
            get_number_of_cameras     : HAL_getNumberOfCameras,
            get_camera_info           : HAL_getCameraInfo,
            set_callbacks             : HAL_set_callbacks,
            get_vendor_tag_ops        : HAL_getVendorTagOps,
            reserved                  : {NULL},
    };
}

} // namespace android
