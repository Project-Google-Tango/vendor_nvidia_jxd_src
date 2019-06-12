/*
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
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
#include <nvcamera_core.h>
#include <nvmm_usbcamera.h>
#include <nvmm_digitalzoom.h>
#include <nvmm_util.h>
#include <nvassert.h>
#include <nvmm_queue.h>

#include "nvcameramemprofileconfigurator.h"
#include "nvcamerahallogging.h"
#include "nvcameracallbacks.h"
#include "nvimage_enc.h"
#include "nvimagescaler.h"
#include "nvcamerahal3streamport.h"

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

#include "ft/fd_api.h"

#include "camera/CameraParameters.h"
#include "CameraMetadata.h"
#include "system/camera_metadata.h"
#include <hardware/camera_common.h>
#include <hardware/hardware.h>

#include <ui/GraphicBufferMapper.h>
#include "nvabstracts.h"
#include "nvcamerahal3common.h"
#include "nvdevorientation.h"
#include "nvcamerahal3tnr.h"

#if NV_POWERSERVICE_ENABLE
#include <PowerServiceClient.h>
#endif

//=========================================================
// Header to add runtime logging support
//=========================================================

#define CAMERA_WAKE_LOCK_ID "nv_camera_wakelock"

namespace android {

class StreamPort;
class MetadataHandler;

struct PortMap;

typedef List< sp<StreamPort> > PortList;
typedef List <PortMap*>    PortMapList;

struct PortMap
{
    camera3_stream_buffer *inStreamBuffer;
    buffer_handle_t *inBuffer;
    uint32_t frameNumber;
    PortList ports;
    CameraMetadata settings;
    NvCameraHal3_Public_Controls ctrlProp;
    bool isReprocessing;
};

struct Request
{
    uint32_t frameNumber;
    uint32_t num_buffers;
    HalBufferVector *buffers;
    camera3_stream_buffer *input_buffer;
    NvU64 frameCaptureRequestId;
    const camera_metadata_t *settings;
};

typedef struct ModeBufferCountMapTableRec {
    NvSize Dimensions;
    uint32_t MaxBuffers;
} ModeBufferCountMapTable;


class NvCameraHal3Core:public AbsCaller
{
public:
    //Constructor
    NvCameraHal3Core(AbsCallback* pCb, NvU32 SensorId,
        const camera_metadata_t *staticMetadata, NvU32 SensorOrientation);

    // disconnect blocks, free shared resources, close blocks
    // de-init state
    virtual ~NvCameraHal3Core();

    // open up a HAL class with the corresponding sensor ID
    // open the proper blocks, connect them together,
    // initialize state, and allocate resources so that it is
    // ready to receive commands from the android API interface
    NvError open();

    NvError addStreams(camera3_stream_t **streams, int count);
    void cleanupStreams();

    NvError addBuffers(camera3_stream_t* stream,
        buffer_handle_t **buffers, int count);
    NvError configureSensor(StreamPort *StreamPort);

    NvError processRequest(Request &request);
    NvError flush();
    void waitAvailableBuffers();

    NvError SetExposureCompensation(int index);

    NvError getSensorBayerFormat(int width, int height, NvColorFormat *pFormat);
    uint32_t getMaxBufferCount(int width, int height);

    camera_metadata_t *getDefaultRequestSettings(int type);
    void initDefaultTemplates();

    static NvError getStaticInfo(int cameraId,
            int orientation, const camera_metadata_t* &cameraInfo);
    NvMMBuffer *getIntermediateTNRBuffer(camera3_stream_buffer *buffer);

private:
    /* configureStreamPort: map the given stream to a port.
     * This will be called for every configure_streams call that the upper layer HAL will receive.
     *
     * Creates StreamPort objects. For now, we will create one thread per StreamPort which
     * will be tied to each stream.
     *
     */
    NvError configureStreamPort(StreamPort* port);

    /* reapStreamPort: destroy the streamPort.
     * This will be called by HAL during configureStreams call to reap the dead streams.
     *
     * Unlinks corresponding buffers, destroys StreamPort thread and object.
     *
     */
    NvError reapStreamPort(StreamPort* port);

    NvMMBuffer *NativeToNVMMBuffer(buffer_handle_t *buffer);
    NvMMBuffer* NativeToNVMMBuffer(buffer_handle_t *buffer,
        NvBool *isCropped, NvRectF32 *pRect);
    buffer_handle_t *NVMMBufferToNative(NvMMBuffer *buffer);

    void flushBuffers();

    void release();

#if NV_POWERSERVICE_ENABLE
    void sendCamPowerHint(NvS32 type, NvBool isHighFps);
#endif

    //Routing methods
    NvError routeRequestToCAM(Request& request);
    NvError routeResultFromCAM(NvU32 frameNumber, buffer_handle_t *buffer,
        const NvCamProperty_Public_Dynamic& frameDynamicProps);
    NvError pushCameraBufferToCAM(uint32_t frameNumber,
                                    camera3_stream_buffer *pOutputStreamBuffer,
                                    camera3_stream_buffer *pOutputStreamTNRBuffer,
                                    camera3_stream_buffer *pInputStreamBuffer,
                                    NvCameraHal3_Public_Controls& ctrlProp,
                                    NvU64 frameCaptureRequestId);

    void EmptyBuffer(NvU32 frameNumber,
                        const NvCamProperty_Public_Dynamic& frameDynamicProps,
                        NvU32 numCompletedOutputBuffers,
                        NvMMBuffer **ppOutputBuffers);

    NvBool getConstructorError();

    // returns true if it's a USB cam, false otherwise
    NvBool isUSB();

    NvError AcquireWakeLock();
    NvError ReleaseWakeLock();

    void initZoomStream(StreamPort *pSourceStreamPort);
    void initTNRZoomStream(StreamPort *pSourceStreamPort);

    void updateDebugControls(NvCamProperty_Public_Controls& ctrlProp);

    // maps NvMM events to some kind of functionality in the HAL
    // typically, this would involve sending callbacks to android,
    // or adjusting some internal state values
    static NvError NvMMEventHandler(void *pContext,
        NvU32 eventType, NvU32 eventSize, void *eventInfo);

    void NvCameraHal3_GetDefaultJpegControlProps(
        NvCameraHal3_JpegControls& jpegControls);

    NvBool isNewCrop(const NvRect& crop, NvS32 stream_width,
        NvS32 stream_height, NvRectF32 *crop_ratio);

    void handleAfTrigger(NvCameraHal3_Public_Controls& prop);
    void handleAeTrigger(NvCameraHal3_Public_Controls& prop);
    void update3ARegions(NvCamProperty_Public_Controls& cprop);
    NvError updateSceneModeControls(NvCameraHal3_Public_Controls& prop);
    NvError process3AControls(NvCameraHal3_Public_Controls& prop);

    void handleAfState(NvCameraHal3_Public_Dynamic& prop);
    void handleAePrecapture(NvCameraHal3_Public_Dynamic& dynProp);
    void cacheFaceStats(NvCameraHal3_Public_Dynamic& prop);
    void process3ADynamics(NvCameraHal3_Public_Dynamic& prop);

    void updateDeviceOrientation(NvCamProperty_Public_Controls& prop);
    camera3_error_msg_code_t nvErrorToHal3Error(NvError error);
    void SendPartialResult(const NvCamProperty_Public_Dynamic&
                                          frameDynamicProps, uint32_t frameNumber);
    void update3AIds(NvCameraHal3_Public_Dynamic& dynProp);
    void sendPartialResultToFrameworks(CameraMetadata& settings,
                                              uint32_t frameNumber);

    NvError enqueueTNRBuffer(int frameNumber, camera3_stream_buffer *buffer);
    camera3_stream_buffer *dequeueTNRBuffer(int frameNumber);

    /********************************************
    *           MEMBER VARIABLES
    *********************************************/

private:
    NvCameraCoreHandle hCameraCore;
    NvCameraTNR mTNR_Processor;
    const camera_metadata_t *mCameraInfo;
    NvMMCameraSensorMode mSensorMode;
    NvMMCameraSensorMode mMaxSensorMode;
    camera_metadata_t *mDefaultTemplates[CAMERA3_TEMPLATE_COUNT];
    NvCameraHal3_Public_Controls mPublicControls[CAMERA3_TEMPLATE_COUNT];

    static KeyedVector< int, Vector<NvMMCameraSensorMode> > mSensorModeMap;
    static KeyedVector<int, NvCamProperty_Public_Static> mStaticCameraInfoMap;

    AbsCallback* mCb;

    NvU32 mSensorId;
    NvU32 mSensorOrientation;
    camera3_stream_t mZoomStream;
    camera3_stream_buffer_t mZoomStreamBuffer;
    camera3_stream_t mTNRZoomStream;
    camera3_stream_buffer_t mTNRZoomStreamBuffer;

    NvCameraHalLogging mCameraLogging;
    FrameJitterStatistics mFrameJitterStatistics;
    NvBool mWakeLocked;
    NvBool mTemplatesInited;

    PortMapList mPortMapList;

    PortList mPorts;
    Mutex mPortMutex;
    Mutex mTNRbufferQMutex;
    sp<MetadataHandler> mMetadataHandler;

    NvU32 mDbgFocusMode;
    NvU32 mDbgPreviewFps;

    NvCamAfState mAfState;

    int32_t mAePrecaptureId;
    NvBool mPrecaptureStart;

    NvS32 mOrientation;
    NvBool mTNREnabled;
    BufferInfoQueue mTNR_Queue;
    NvU32 mTNRWidth;
    NvU32 mTNRHeight;
    NvDeviceOrientation mDevOrientation;

    int32_t mAfTriggerId;
    NvBool mAfTriggerStart;
    NvBool mAfTriggerAck;

    Mutex mMetadataLock;
    NvCamPropertyRectList mFaceRectangles;

    // Cached settings from latest submitted request
    CameraMetadata mCachedSettings;
    NvCameraHal3_Public_Controls mCachedControls;

    NvImageScaler mScaler;
#if NV_POWERSERVICE_ENABLE
    //PowerServiceClient
    android::PowerServiceClient *mPowerServiceClient;
    NvS32 mPrevType;
    NvBool mHighFpsMode;
#endif
};

} // namespace android

#endif // NV_CAMERA_HAL_H

