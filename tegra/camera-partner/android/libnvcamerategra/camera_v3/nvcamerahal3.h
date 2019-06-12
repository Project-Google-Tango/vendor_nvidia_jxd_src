/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NV_CAMERA_HAL3_H
#define NV_CAMERA_HAL3_H


#include <nverror.h>
#include <nvassert.h>
#include <nvos.h>

#include <utils/RefBase.h>
#include <utils/Timers.h>
#include <utils/Condition.h>
#include <utils/Mutex.h>
#include <utils/List.h>
#include <utils/Thread.h>
#include <utils/Vector.h>
#include <utils/KeyedVector.h>
#include <utils/String8.h>
#include <utils/String16.h>
#include <ui/GraphicBufferMapper.h>


#include <ctype.h>

#include "camera/CameraParameters.h"
#include "CameraMetadata.h"
#include <hardware/camera2.h>
#include <hardware/camera3.h>
#include "system/camera_metadata.h"
#include <hardware/camera_common.h>
#include <hardware/camera.h>
#include <hardware/hardware.h>

#include <cstdlib>

#include "nvcamerahal3core.h"

namespace android {

class NvCameraHal3:public AbsCallback
{
    enum HAL_STATUS {
        // State at construction time, and after a device operation error
        STATUS_ERROR = 0,
        // State after startup-time init and after device instance close
        STATUS_CLOSED,
        // State after being opened, before device instance init
        STATUS_OPEN,
        // State after device instance initialization
        STATUS_READY,
        // State while actively capturing data
        STATUS_ACTIVE
    };

public:
    /* Constructs Hal3 class with the corresponding sensor ID,
       initializes state and set default values */
    NvCameraHal3(NvU32 AndroidSensorId, camera3_device_t *dev);

    ~NvCameraHal3();

    status_t open();

    status_t initialize(const camera3_callback_ops_t *callback_ops);

    status_t configureStreams(camera3_stream_configuration *streamList);

    status_t registerStreamBuffers(
        const camera3_stream_buffer_set *bufferSet) ;

    const camera_metadata_t* constructDefaultRequestSettings(
        int type);

    status_t processCaptureRequest(camera3_capture_request *request);

    /*
     * Dump state of the camera hardware
     */
    status_t dump(int fd);
    status_t flush();

    static status_t getStaticCharacteristics(int cameraId, int orientation,
                    const camera_metadata_t* &cameraInfo);

protected:
    void notifyShutterMsg(int frame, NvU64 ts);
    void notifyError(int frame,
            camera3_error_msg_code_t error, camera3_stream* stream);
    void sendResult(camera3_capture_result* result);

private:
    status_t constructStaticInfo();

private:
    int mSensorId;
    HAL_STATUS mStatus;

    const camera3_callback_ops_t *mCallbackOps;

    static const uint32_t kFenceTimeoutMs = 2000; // 2 s
    static DefaultKeyedVector< int, const camera_metadata_t * > staticInfoMap;

    // Shortcut to the input stream
    camera3_stream_t* mInputStream;

    Mutex mLock;

    NvCameraHal3Core mNvCameraCore;
};

} // namespace android

#endif // NV_CAMERA_HAL3_H

