/*
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include <math.h>
#include "nvcamerahal3core.h"
#include "nvrm_init.h"
#include "nvcamerahal3streamport.h"
#include "nvcamerahal3metadatahandler.h"
#include "nvmetadatatranslator.h"

#include "nvcamerahal3_tags.h"
#include "nv_log.h"

namespace android {

#define CAMERA_PREVIEW_FPS_LIMIT 30

#define MIN_DISTANCE_TO_UPDATE (15)

#define ORIENTATION_INVALID (-1)
// this comes from OrientationManager class
#define ORIENTATION_HYSTERESIS          (5)
// this is an experimentally decided value
#define ORIENTATION_UPDATE_THRESHOLD    (45 + ORIENTATION_HYSTERESIS)

#define DEFAULT_THUMBNAIL_QUALITY 85
#define DEFAULT_JPEG_QUALITY      95
#define DEFAULT_JPEG_ORIENTATION  0
#define DEFAULT_THUMBNAIL_WIDTH   320
#define DEFAULT_THUMBNAIL_HEIGHT  240

KeyedVector< int, Vector<NvMMCameraSensorMode> > NvCameraHal3Core::mSensorModeMap;
KeyedVector<int, NvCamProperty_Public_Static>
    NvCameraHal3Core::mStaticCameraInfoMap;

static const ModeBufferCountMapTable kResolutionBufferCountMap[] =
{
    {{4208, 3120}, 6},
    {{3896, 2192}, 6},
    {{2616, 1472}, 5},
    {{2592, 1944}, 6},
    {{2104, 1560}, 5},
    {{1920, 1080}, 6},
    {{1296, 972}, 5},
    {{1280, 720}, 5},
    {{640, 480}, 5}
};

NvCameraHal3Core::NvCameraHal3Core(AbsCallback* pCb, NvU32 SensorId,
    const camera_metadata_t *cameraInfo, NvU32 SensorOrientation)
    : hCameraCore(NULL),
    mCameraInfo(cameraInfo),
    mCb(pCb),
    mSensorId(SensorId),
    mSensorOrientation(SensorOrientation),
    mDbgFocusMode(0),
    mDbgPreviewFps(0),
    mAfState(NvCamAfState_Inactive),
    mAePrecaptureId(0),
    mPrecaptureStart(NV_FALSE),
    mOrientation(-1),
    mAfTriggerId(0),
    mAfTriggerStart(NV_FALSE),
    mAfTriggerAck(NV_FALSE)
{
    NV_LOGD(HAL3_CORE_TAG, "%s: ++", __FUNCTION__);

    mWakeLocked = NV_FALSE;
    mTemplatesInited = NV_FALSE;

    mSensorMode.Resolution.width = 0;
    mSensorMode.Resolution.height = 0;

    mFaceRectangles.Size = 0;

    // Run MetadataHandler thread
    mMetadataHandler = new MetadataHandler();
    mMetadataHandler->registerCallback(mCb);
    mMetadataHandler->run("MetadataHandler::MetadataHandler");

    {
        char value[PROPERTY_VALUE_MAX];
        property_get("camera.debug.previewfps", value, "0");
        mDbgPreviewFps = atoi(value);

        /*
        * Fix focus position.
        * Move focus to max or min position.
        * Usage: adb shell setprop camera.debug.fixfocus <mode>
        * <mode> 1: macro, 2: infinite
        */
        property_get("camera.debug.fixfocus", value, "0");
        mDbgFocusMode = atoi(value);

        property_get("camera.debug.enabletnr", value, "0");
        mTNREnabled = atoi(value);
    }

    for (size_t i = 0; i < CAMERA3_TEMPLATE_COUNT; i++)
    {
        mDefaultTemplates[i] = NULL;
    }

#if NV_POWERSERVICE_ENABLE
    mPowerServiceClient = new android::PowerServiceClient();
    if(!mPowerServiceClient)
    {
        NV_LOGE("camerahal: powerServiceClient init fail");
    }
    mPrevType = -1;
    mHighFpsMode = NV_FALSE;
    if (mPowerServiceClient)
    {
        int hint = POWER_HINT_CAMERA, data = CAMERA_HINT_PERF;
        mPowerServiceClient->sendPowerHint(hint, &data);
    }
#endif

    NV_LOGD(HAL3_CORE_TAG, "%s: --", __FUNCTION__);
}

NvError NvCameraHal3Core::getSensorBayerFormat(int width, int height, NvColorFormat *pFormat)
{
    if (!pFormat)
        return NvError_BadParameter;

    Vector <NvMMCameraSensorMode> sensorModes =
        mSensorModeMap.valueFor(mSensorId);
    Vector <NvMMCameraSensorMode>::iterator s;

    // just pick first available format for the resolution
    // TODO: look for Bayer
    for (s = sensorModes.begin(); s != sensorModes.end(); s++)
    {
        if(s->Resolution.width == width && s->Resolution.height == height)
        {
            *pFormat = s->ColorFormat;
            return NvSuccess;
        }
    }

    return NvError_BadValue;
}

uint32_t NvCameraHal3Core::getMaxBufferCount(int width, int height)
{
    NvU32 tableSize = NV_ARRAY_SIZE(kResolutionBufferCountMap);
    int i = 0;
    NvS32 prevModeIndex = -1;
    NvU32 areaMode = 0;
    NvU32 areaInput = width * height;

    for (i = 0; i < tableSize; i++)
    {
        areaMode = kResolutionBufferCountMap[i].Dimensions.width *
                   kResolutionBufferCountMap[i].Dimensions.height;
        if (areaMode == areaInput)
        {
            return kResolutionBufferCountMap[i].MaxBuffers;
        }
        if (areaMode > areaInput)
        {
            prevModeIndex++;
        }
    }

    if (prevModeIndex < 0)
    {
        NV_LOGW(HAL3_CORE_TAG, "%s: input res bigger than all the available modes.", __FUNCTION__);
        NV_LOGV(HAL3_CORE_TAG, "Setting buffer count for largest mode.");
        return kResolutionBufferCountMap[0].MaxBuffers;
    }

    NV_LOGV(HAL3_CORE_TAG, "%s: couldn't find a mode matching input res.", __FUNCTION__);
    NV_LOGV(HAL3_CORE_TAG, "Setting buffer count for the next higher mode.");
    return kResolutionBufferCountMap[prevModeIndex].MaxBuffers;
}

NvError NvCameraHal3Core::open()
{
    NvError e = NvSuccess;

    NV_LOGD(HAL3_CORE_TAG, "%s: ++", __FUNCTION__);

    if (!mDevOrientation.initialize())
    {
        NV_LOGE("Sensor Listener Failed to Initialize");
        return NvError_NotInitialized;
    }
    /*
     * 1. The client calls NvCameraCore_Open() with a NvCameraCoreHandle and a
     * sensor GUID to select a sensor out of multiple sensors on the system.
     */
    NV_CHECK_ERROR_CLEANUP(
        NvCameraCore_Open(&hCameraCore, camGUID[mSensorId])
    );

    /*
     * 2. The client calls NvCameraCore_SetSendEventFunction() to set
     * the event handler function which will be called when camera core
     * sends event to the client. See NvCameraCoreEvent for the events.
     */
    NV_CHECK_ERROR_CLEANUP(
        NvCameraCore_CallbackFunction(
            hCameraCore,
            (void *) this, // NvMM will pass this back up to us in evts
            NvMMEventHandler)
    );

    NV_LOGD(HAL3_CORE_TAG, "%s: --", __FUNCTION__);
    return NvSuccess;

fail:
    NV_LOGE("%s: %d failed to open camera! Error: 0x%x", __FUNCTION__,
        __LINE__, e);
    NvCameraCore_Close(hCameraCore);
    release();
    return e;
}

NvCameraHal3Core::~NvCameraHal3Core()
{
    NvError e = NvSuccess;

    NV_LOGD(HAL3_CORE_TAG, "%s: ++", __FUNCTION__);

#if NV_POWERSERVICE_ENABLE
    if(mPowerServiceClient)
    {
        int hint = POWER_HINT_CAMERA, data;
        data = CAMERA_HINT_RESET;
        mPowerServiceClient->sendPowerHint(hint, &data);
    }
#endif

    cleanupStreams();

    // Request exit for metadatahandler
    if (mMetadataHandler != NULL)
    {
        mMetadataHandler->requestExit();
        mMetadataHandler->flush();
        mMetadataHandler->join();
    }

    release();

#if 0
    /*
     * Close various process nodes in the camera. A process node
     * is a light weight processing block like Noise reduction or video
     * stabilization etc. Multiple process nodes can be closed in one call
     * by ORing the bitmasks being passed as a parameter.
     */
    NvCameraCore_ProcessNodeClose(
        hCameraCore,
        mask);
#endif

    // Close the camera core driver and release all resources held by it.
    NvCameraCore_Close(hCameraCore);

    for (size_t i = 0; i < CAMERA3_TEMPLATE_COUNT; i++)
    {
        if (mDefaultTemplates[i] != NULL)
        {
            free_camera_metadata(mDefaultTemplates[i]);
        }
    }

    NV_LOGD(HAL3_CORE_TAG, "%s: --", __FUNCTION__);
}

void NvCameraHal3Core::cleanupStreams()
{
    PortList::iterator itr = mPorts.begin();
    while (itr != mPorts.end())
    {
        (*itr)->set_alive(false);
        reapStreamPort((*itr).get());
        (*itr)->getStream()->priv = NULL;
        itr = mPorts.erase(itr);
    }
}


void NvCameraHal3Core::release()
{
    NvError e = NvSuccess;
    NvU32 i;
    NV_LOGD(HAL3_CORE_TAG, "%s: ++", __FUNCTION__);

    // old HAL took a wake lock while it was releasing resources.  we didn't
    // have any history of the specific bug that it was placed there for, but
    // it may have taken hours of debugging to find that, so we carried it over
    // into the new HAL
    NV_CHECK_ERROR_CLEANUP(
        AcquireWakeLock()
    );

    NV_CHECK_ERROR_CLEANUP(
        ReleaseWakeLock()
    );

    NV_LOGD(HAL3_CORE_TAG, "%s: --", __FUNCTION__);
    return;

fail:
    NV_LOGE("%s: -- ERROR [0x%x]", __FUNCTION__, e);
    return;
}

static int compareSensorModes(const void *sensorMode1, const void *sensorMode2)
{
    NvMMCameraSensorMode *mode1 = ( NvMMCameraSensorMode *) sensorMode1;
    NvMMCameraSensorMode *mode2 = ( NvMMCameraSensorMode *) sensorMode2;

    NvU32 area1 = mode1->Resolution.width * mode1->Resolution.height;
    NvU32 area2 = mode2->Resolution.width * mode2->Resolution.height;

    return (area1 > area2) ? 1 : (area1 == area2) ? 0 : -1;
}

static void printSensorModes(NvMMCameraSensorMode *sensorModes, int size)
{
    for (int i = 0; i < size; i++)
    {
        NV_LOGV(HAL3_CORE_TAG, "%s: %d x %d", __FUNCTION__,
            sensorModes[i].Resolution.width,
            sensorModes[i].Resolution.height);
    }
}

NvError NvCameraHal3Core::getStaticInfo(int cameraId,
                        int orientation,
                        const camera_metadata_t* &cameraInfo)
{
    NvError e = NvSuccess;
    int i;
    size_t index;
    NvCameraCoreHandle hCore;
    NvCamProperty_Public_Static staticProperties;

    CameraMetadata metadata;
    NvMMCameraSensorMode maxSensorMode;
    Vector<NvMMCameraSensorMode> sensorModes;
    NvMMCameraSensorMode sortedSensorModes[MAX_NUM_SENSOR_MODES];

    NV_LOGD(HAL3_CORE_TAG, "%s: ++", __FUNCTION__);

    // Open NvCameraCore
    NV_CHECK_ERROR_CLEANUP(
        NvCameraCore_Open(&hCore, camGUID[cameraId])
    );

    // get static characteristics
    NV_CHECK_ERROR_CLEANUP(
        NvCameraCore_GetStaticProperties(
            hCore,
            &staticProperties)
    );

    {
        NvU32 dbgFocusMode = 0;
        char value[PROPERTY_VALUE_MAX];
        property_get("camera.debug.fixfocus", value, "0");
        dbgFocusMode = atoi(value);
        if (dbgFocusMode != 0) // faking fixed focus camera
        {
            staticProperties.AfAvailableModes.Size = 1;
            staticProperties.AfAvailableModes.Property[0] =
                NvCamAfMode_Off;
            staticProperties.LensMinimumFocusDistance = 0.0;
        }
    }

    mStaticCameraInfoMap.replaceValueFor(cameraId, staticProperties);

    // Debug prints
    NV_LOGV(HAL3_CORE_TAG, "%s: Printing all sensor modes", __FUNCTION__);
    printSensorModes(staticProperties.SensorModeList, MAX_NUM_SENSOR_MODES);

    // Sort valid sensor modes into a temp array
    i = 0;
    for (i = 0; i < MAX_NUM_SENSOR_MODES; i++)
    {
        if ((staticProperties.SensorModeList[i].Resolution.width == 0) ||
            (staticProperties.SensorModeList[i].Resolution.height == 0))
        {
            break;
        }

        sortedSensorModes[i].Resolution.width =
                    staticProperties.SensorModeList[i].Resolution.width;
        sortedSensorModes[i].Resolution.height =
                    staticProperties.SensorModeList[i].Resolution.height;
        sortedSensorModes[i].ColorFormat =
                    staticProperties.SensorModeList[i].ColorFormat;

    }

    // sort the list of sensorModes
    qsort(sortedSensorModes, i, sizeof(NvMMCameraSensorMode),
            compareSensorModes);

    // Debug prints
    NV_LOGV(HAL3_CORE_TAG, "%s: Sorted list of available sensor modes", __FUNCTION__);
    printSensorModes(sortedSensorModes, i);


    // Store available sensor modes, since core will need it for
    // configuring sensor.
    NV_LOGV(HAL3_CORE_TAG, "%s: Caching available sensor modes (ascending order)", __FUNCTION__);
    for (int j = 0; j < i; j++)
    {
        NvMMCameraSensorMode mode;
        mode.Resolution.width = sortedSensorModes[j].Resolution.width;
        mode.Resolution.height = sortedSensorModes[j].Resolution.height;
        mode.ColorFormat = sortedSensorModes[j].ColorFormat;

        NV_LOGV(HAL3_CORE_TAG, "%s: %d x %d (%X)", __FUNCTION__,
            mode.Resolution.width,
            mode.Resolution.height,
            mode.ColorFormat);

        sensorModes.push_back(mode);
    }

    // Last element in the sorted vector is the largest mode.
    index = sensorModes.size() - 1;
    maxSensorMode.Resolution.width = sensorModes[index].Resolution.width;
    maxSensorMode.Resolution.height = sensorModes[index].Resolution.height;
    maxSensorMode.ColorFormat = sensorModes[index].ColorFormat;

    NV_LOGV(HAL3_CORE_TAG, "%s: maxSensorMode: %d x %d (%X)", __FUNCTION__,
            maxSensorMode.Resolution.width,
            maxSensorMode.Resolution.height,
            maxSensorMode.ColorFormat);

    // Add sensoreModes to the Vector of modes
    mSensorModeMap.add(cameraId, sensorModes);

    // Close NvCameraCore
    NvCameraCore_Close(hCore);

    staticProperties.SensorOrientation = orientation;
    // Translate the characteristics to framework compatible metadata
    NvMetadataTranslator::translateFromNvCamProperty(staticProperties,
        metadata);

    NV_LOGV(HAL3_CORE_TAG, "%s: Static metadata[%p], count[%d] added for sensorID[%d]",
            __FUNCTION__, cameraInfo, metadata.entryCount(), cameraId);
    cameraInfo = metadata.release();

    NV_LOGD(HAL3_CORE_TAG, "%s: --", __FUNCTION__);
    return e;

fail:
    NV_LOGE("%s: -- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal3Core::addStreams(camera3_stream_t **streams, int count)
{
    PortList::iterator s = mPorts.begin();
    /*
    * Initially mark all existing streams as not alive
    */
    for (; s != mPorts.end(); ++s)
    {
        (*s)->set_alive(false);
    }

    /*
     * Find new streams and mark still-alive ones
     */
    for (int i = 0; i < count; i++)
    {
        camera3_stream_t *newStream = streams[i];
        if (newStream->priv == NULL)
        {
            NvColorFormat SensorFormat = NvColorFormat_Y8;
            // New stream, construct info
            if (newStream->format == HAL_PIXEL_FORMAT_RAW_SENSOR)
            {
                NvError e = getSensorBayerFormat(newStream->width, newStream->height, &SensorFormat);
                if (e != NvSuccess)
                {
                    cleanupStreams();
                    return NvError_BadValue;
                }
            }

            sp<StreamPort> privStream = new StreamPort(newStream,
                CAMERA_STREAM_NONE, SensorFormat);
            newStream->priv = privStream.get();

            NV_LOGV(HAL3_CORE_TAG, "new stream %p, priv=%p", newStream, newStream->priv);

            //configure a stream port for this stream
            configureStreamPort(privStream.get());
            // add it to the list of mPorts
            mPorts.push_back(privStream);
        }
        else
        {
            // Existing stream, mark as still alive.
            StreamPort *privStream =
                    static_cast<StreamPort*>(newStream->priv);
            privStream->set_alive(true);
        }
    }

    /*
     * Reap dead streams and remove it in mPorts
     */
    for (s = mPorts.begin(); s != mPorts.end();)
    {
        if (!(*s)->is_alive())
        {
            NV_LOGV(HAL3_CORE_TAG, "%s: remove stream %p, usage 0x%x, format 0x%x"
                " width=%d height=%d", __FUNCTION__,
                (*s)->getStream(), (*s)->getStream()->usage,
                (*s)->format(), (*s)->width(), (*s)->height());

            // Destroy the corresponding streamPort.
            reapStreamPort((*s).get());
            (*s)->getStream()->priv = NULL;
            s = mPorts.erase(s);
        }
        else
        {
            s++;
        }
    }

    /*
     * Tag streams based on resolution.
     * This info will help router map a stream to a right DZ port or
     * virtual streamPort.
     */
    NvU32 area = 0;
    NvU32 maxArea = 0;
    StreamPort *pSourceStreamPort = NULL;
    StreamPort *pTNRSourceStreamPort = NULL;

    /*
     * Find largest(most pixel) resolution among
     * and tag largest with source port.
     */
    for (s = mPorts.begin(); s != mPorts.end(); s++)
    {
        area = (((*s)->width())*((*s)->height()));
        if (area >= maxArea)
        {
                pSourceStreamPort = (*s).get();
                maxArea = area;
        }
    }

    /*
     * Go through TNR-allowed streams and
     * figure maximum possible width and hight
     * so that we can allocate an intermediate
     * buffer to scale to all TNR streams
     */
    if (mTNREnabled)
    {
        mTNRWidth = 0;
        mTNRHeight = 0;
        maxArea = 0;
        for (s = mPorts.begin(); s != mPorts.end(); s++)
        {
            if (((*s)->height()) <= MAX_TNR_YRES)
            {
                area = (((*s)->width())*((*s)->height()));
                if (area >= maxArea)
                {
                    pTNRSourceStreamPort = (*s).get();
                    maxArea = area;
                }
                if ((*s)->width() > mTNRWidth)
                {
                    mTNRWidth = (*s)->width();
                }
                if ((*s)->height() > mTNRHeight)
                {
                    mTNRHeight = (*s)->height();
                }
            }
        }
    }

    if (pSourceStreamPort)
    {
        NV_LOGV(HAL3_CORE_TAG, "%s: Largest stream is %p", __FUNCTION__,
            pSourceStreamPort->getStream());
    }
    else
    {
        // No stream to configure the sensor with.
        NV_LOGE("%s: There was no stream to configure.", __FUNCTION__);
        return NvError_BadParameter;
    }

    // Set the sensor mode to an appropriate resolution
    configureSensor(pSourceStreamPort);

    // Create streamport for zooming
    // model it closely after the max-res stream
    initZoomStream(pSourceStreamPort);

    if(pTNRSourceStreamPort)
    {
        initTNRZoomStream(pTNRSourceStreamPort);
    }

    mCachedSettings.clear();
    return NvSuccess;
}


NvError NvCameraHal3Core::addBuffers(camera3_stream_t* stream,
    buffer_handle_t **buffers, int count)
{
    NvError err;

    StreamPort* port = static_cast<StreamPort*>(stream->priv);

    if (port->streamRegistered())
    {
        NV_LOGV(HAL3_CORE_TAG, "%s: Registering buffers for an existing stream", __FUNCTION__);
        port->unlinkBuffers(hCameraCore);
    }

    err = port->linkBuffers(buffers, count);
    if (NvSuccess != err)
    {
        NV_LOGE("%s: Unable to link buffers", __FUNCTION__);
    }

    return err;
}

static NvBool isARMatch(NvF32 ar1, NvF32 ar2)
{
    return (fabs(ar1 - ar2) < ASPECT_TOLERANCE);
}

NvError NvCameraHal3Core::configureSensor(StreamPort *StreamPort)
{
    NvError e = NvSuccess;
    NvU32 area, portArea, i;
    NvBool streamResChanged = NV_TRUE;
    NvBool arMatchFound = NV_FALSE, smallestMatchFound = NV_FALSE;
    NvMMCameraSensorMode smallestMatchMode;
    NvS32 sensorWidth, sensorHeight;
    NvF32 portAR, modeAR;

    Vector <NvMMCameraSensorMode> sensorModes =
        mSensorModeMap.valueFor(mSensorId);
    Vector <NvMMCameraSensorMode>::iterator s;

    NV_LOGD(HAL3_CORE_TAG, "%s: ++",__FUNCTION__);

    sensorWidth = mSensorMode.Resolution.width;
    sensorHeight = mSensorMode.Resolution.height;

    // Assuming that sensorMode list will always be sorted
    // in ascending order of resolutions.
    if (StreamPort)
    {
        portArea = StreamPort->width() * StreamPort->height();
        portAR   = (NvF32) StreamPort->height() / StreamPort->width();

        for (s = sensorModes.begin(); s != sensorModes.end(); s++)
        {
            mSensorMode = *s;

            area = mSensorMode.Resolution.width * mSensorMode.Resolution.height;
            modeAR = (NvF32) mSensorMode.Resolution.height / mSensorMode.Resolution.width;

            if (area >= portArea && isARMatch(portAR, modeAR))
            {
                // If we hit this condition, that means streamPort is requesting
                // a resolution that is not an odm standard. We will have to
                // settle with the closest resolution to the requested one.
                arMatchFound = NV_TRUE;
                break;
            }

            if (area >= portArea && !smallestMatchFound)
            {
                smallestMatchFound = NV_TRUE;
                smallestMatchMode = *s;
            }
        }

        if (!arMatchFound)
        {
            if (!smallestMatchFound)
            {
                NV_LOGE("%s: cannot find a sensor mode for %dx%d",
                    __FUNCTION__,
                    StreamPort->width(),
                    StreamPort->height());
                return NvError_BadParameter;
            }
            mSensorMode = smallestMatchMode;
        }

        if (sensorWidth == mSensorMode.Resolution.width &&
            sensorHeight == mSensorMode.Resolution.height)
        {
            streamResChanged = NV_FALSE;
        }
    }

    // Last element in the sorted vector is the largest
    size_t index = sensorModes.size() - 1;
    mMaxSensorMode.Resolution.width = sensorModes[index].Resolution.width;
    mMaxSensorMode.Resolution.height = sensorModes[index].Resolution.height;
    NV_LOGV(HAL3_CORE_TAG, "%s: mMaxSensorMode: %d x %d", __FUNCTION__,
        mMaxSensorMode.Resolution.width, mMaxSensorMode.Resolution.height);

    // If there is no change in resolution, then return here.
    if (!streamResChanged)
    {
        return NvSuccess;
    }

    NV_LOGV(HAL3_CORE_TAG, "%s: Configuring sensor to: %d x %d", __FUNCTION__,
            mSensorMode.Resolution.width, mSensorMode.Resolution.height);

    NV_CHECK_ERROR_CLEANUP(
        NvCameraCore_SetSensorMode(
            hCameraCore,
            mSensorMode)
    );

    NV_LOGD(HAL3_CORE_TAG, "%s: --",__FUNCTION__);
    return NvSuccess;

fail:
     NV_LOGE("%s: -- error [0x%x]", __FUNCTION__, e);
     return e;
}

NvError NvCameraHal3Core::configureStreamPort(StreamPort* port)
{
    NV_LOGD(HAL3_CORE_TAG, "%s: ++",__FUNCTION__);

    Mutex::Autolock al(mPortMutex);
    port->registerCallback(mCb);
    port->run("StreamPort::StreamPort");

    NV_LOGD(HAL3_CORE_TAG, "%s: --",__FUNCTION__);
    return NvSuccess;
}

NvError NvCameraHal3Core::reapStreamPort(StreamPort* port)
{
    NV_LOGD(HAL3_CORE_TAG, "%s: ++",__FUNCTION__);

    Mutex::Autolock al(mPortMutex);

    if (!port->is_alive())
    {
        NV_LOGV(HAL3_CORE_TAG, "%s: remove stream %p", __FUNCTION__, port->getStream());
        port->unlinkBuffers(hCameraCore);
        port->requestExit();
        port->flushBuffers();
        port->processResult(0, 0, NULL);
    }

    NV_LOGD(HAL3_CORE_TAG, "%s: --",__FUNCTION__);
    return NvSuccess;
}


NvError NvCameraHal3Core::flush()
{
    NvError e = NvSuccess;
    PortList::iterator itr;
    NV_LOGD(HAL3_CORE_TAG, "%s: ++",__FUNCTION__);

#if NV_POWERSERVICE_ENABLE
    if (mPowerServiceClient)
    {
        int hint = POWER_HINT_CAMERA, data = CAMERA_HINT_PERF;
        mPowerServiceClient->sendPowerHint(hint, &data);
    }
#endif

    //let all streamports to know we want to flush.
    //So they can skip as quick as possible.
    itr = mPorts.begin();
    while (itr != mPorts.end())
    {
        (*itr)->setBypass(true);
        itr++;
    }

    mTNR_Processor.setBypass(true);
    NV_CHECK_ERROR_CLEANUP(
        NvCameraCore_Flush(hCameraCore)
    );

    itr = mPorts.begin();
    while (itr != mPorts.end())
    {
        (*itr)->flushBuffers();
        itr++;
    }

    mTNR_Processor.setBypass(false);
    itr = mPorts.begin();
    while (itr != mPorts.end())
    {
        (*itr)->setBypass(false);
        itr++;
    }

    NV_LOGD(HAL3_CORE_TAG, "%s: --",__FUNCTION__);
    return NvSuccess;

fail:
    NV_LOGE("%s: -- error [0x%x]", __FUNCTION__, e);
    return e;
}

NvError NvCameraHal3Core::processRequest(Request &request)
{
    NvError e = NvSuccess;

#if !LOG_NDEBUG
    {
        NV_LOGV(HAL3_CORE_TAG, "%s: Request START for FN=%d", __FUNCTION__, request.frameNumber);
        HalBufferVector::iterator itr = request.buffers->begin();
        int i = 0;
        while (itr != request.buffers->end())
        {
            NV_LOGV(HAL3_CORE_TAG, "%s: output: stream %p: %dx%d", __FUNCTION__,
                itr->stream,
                itr->stream->width,
                itr->stream->height);
            itr++;
        }
        if (request.input_buffer)
        {
            NV_LOGV(HAL3_CORE_TAG, "%s: input: stream %p: %dx%d", __FUNCTION__,
                request.input_buffer->stream,
                request.input_buffer->stream->width,
                request.input_buffer->stream->height);
        }
        NV_LOGV(HAL3_CORE_TAG, "%s: Request END", __FUNCTION__);
    }
#endif

    // Route and push buffers to DZ
    NV_CHECK_ERROR_CLEANUP(
        routeRequestToCAM(request)
    );

    return NvSuccess;

fail:
     NV_LOGE("%s: -- error [0x%x]", __FUNCTION__, e);
     return e;
}

NvMMBuffer *NvCameraHal3Core::NativeToNVMMBuffer(buffer_handle_t *buffer)
{
    PortList::iterator s;
    NvMMBuffer* pBuffer = NULL;

    for (s = mPorts.begin(); s != mPorts.end(); ++s)
    {
        pBuffer = (*s)->NativeToNVMMBuffer(buffer);
        if (NULL != pBuffer)
        {
            return pBuffer;
        }
    }

    NV_LOGE("%s: error unreferencing native handle (%p)", __FUNCTION__, buffer);
    return NULL;
}

NvMMBuffer *NvCameraHal3Core::NativeToNVMMBuffer(
    buffer_handle_t *buffer,
    NvBool *isCropped,
    NvRectF32* pRect)
{
    PortList::iterator s;
    NvMMBuffer* pBuffer = NULL;

    for (s = mPorts.begin(); s != mPorts.end(); ++s)
    {
        pBuffer = (*s)->NativeToNVMMBuffer(buffer, isCropped, pRect);
        if (NULL != pBuffer)
        {
            return pBuffer;
        }
    }

    NV_LOGE("%s: error unreferencing native handle (%p)", __FUNCTION__, buffer);
    return NULL;
}

buffer_handle_t *NvCameraHal3Core::NVMMBufferToNative(NvMMBuffer *buffer)
{
    PortList::iterator s;
    buffer_handle_t* anb = NULL;

    for (s = mPorts.begin(); s != mPorts.end(); ++s)
    {
        anb = (*s)->NvMMBufferToNative(buffer);
        if (NULL != anb)
        {
            return anb;
        }
    }

    NV_LOGE("%s: error unreferencing NvMM handle (%p) ID %d", __FUNCTION__, buffer, buffer->BufferID);
    return NULL;
}

void NvCameraHal3Core::waitAvailableBuffers()
{
    sp<StreamPort> pStreamPort;
    PortList::iterator p;

    for (p = mPorts.begin(); p != mPorts.end(); ++p)
    {
        pStreamPort = *p;
        camera3_stream_t *s = pStreamPort->getStream();
        StreamPort *privStream =
                static_cast<StreamPort*>(s->priv);
        privStream->waitAvailableBuffers();
    }
}

void NvCameraHal3Core::flushBuffers()
{
    sp<StreamPort> pStreamPort;
    PortList::iterator p;

    for (p = mPorts.begin(); p != mPorts.end(); ++p)
    {
        pStreamPort = *p;
        camera3_stream_t *s = pStreamPort->getStream();
        StreamPort *privStream =
                static_cast<StreamPort*>(s->priv);
        privStream->flushBuffers();
    }
}

void NvCameraHal3Core::initZoomStream(StreamPort *pSourceStreamPort)
{
    // Sensor must have been configured before zoom init
    NV_ASSERT(mSensorMode.Resolution.width != 0 && mSensorMode.Resolution.height != 0);

    mZoomStream.stream_type = pSourceStreamPort->streamType();
    mZoomStream.format = HAL_PIXEL_FORMAT_YV12;
    mZoomStream.width = mSensorMode.Resolution.width;
    mZoomStream.height = mSensorMode.Resolution.height;
    mZoomStream.max_buffers = getMaxBufferCount(mZoomStream.width, mZoomStream.height);
    sp<StreamPort> privStream = new StreamPort(&mZoomStream,
                CAMERA_STREAM_ZOOM, NvColorFormat_Y8);
    mZoomStream.priv = privStream.get();

    //configure a stream port for this stream
    configureStreamPort(privStream.get());

    // add it to the list of mPorts
    mPorts.push_back(privStream);
    NV_LOGV(HAL3_CORE_TAG, "zoom stream %p, priv=%p", &mZoomStream, mZoomStream.priv);

    mZoomStreamBuffer.stream = &mZoomStream;
    mZoomStreamBuffer.buffer = NULL;
    // allocate NvMMBuffer Surface for zoom stream.
    addBuffers(&mZoomStream, NULL, mZoomStream.max_buffers);
}

#define HAL3_ALIGN2(a)      ((a)=(((a)+1)&(~1)))

void NvCameraHal3Core::initTNRZoomStream(StreamPort *pSourceStreamPort)
{
    mTNRZoomStream.stream_type = pSourceStreamPort->streamType();
    mTNRZoomStream.format = HAL_PIXEL_FORMAT_YV12;
    if (mTNRWidth >= mTNRHeight)
    {
        mTNRZoomStream.width = mTNRWidth;
        mTNRZoomStream.height = mTNRWidth /
                            ((float)mSensorMode.Resolution.width / (float)mSensorMode.Resolution.height);
    }
    else
    {
        mTNRZoomStream.height = mTNRHeight;
        mTNRZoomStream.width = mTNRHeight *
                            ((float)mSensorMode.Resolution.width / (float)mSensorMode.Resolution.height);
    }

    HAL3_ALIGN2(mTNRZoomStream.width);
    HAL3_ALIGN2(mTNRZoomStream.height);

    mTNRZoomStream.max_buffers = getMaxBufferCount(mTNRZoomStream.width, mTNRZoomStream.height);
    sp<StreamPort> privStream = new StreamPort(&mTNRZoomStream,
                CAMERA_STREAM_ZOOM, NvColorFormat_Y8);
    mTNRZoomStream.priv = privStream.get();

    //configure a stream port for this stream
    configureStreamPort(privStream.get());

    // add it to the list of mPorts
    mPorts.push_back(privStream);

    mTNRZoomStreamBuffer.stream = &mTNRZoomStream;
    mTNRZoomStreamBuffer.buffer = NULL;
    // allocate NvMMBuffer Surface for TNR zoom stream.
    addBuffers(&mTNRZoomStream, NULL, mTNRZoomStream.max_buffers);
}



NvBool NvCameraHal3Core::isUSB()
{
    int fd;

    if (getCamType(mSensorId) != NVCAMERAHAL_CAMERATYPE_USB)
        return NV_FALSE;

    /* Try opening first device */
    fd = ::open ("/dev/video0", O_RDWR | O_NONBLOCK, 0);
    if (fd < 0)
    {
       /* First device opening failed so  trying with the next device */
       fd = ::open ("/dev/video1", O_RDWR | O_NONBLOCK, 0);
       if (fd < 0)
       {
          return NV_FALSE;
       }
    }
    close(fd);

    return NV_TRUE;
}

void NvCameraHal3Core::initDefaultTemplates()
{
    for (int type = CAMERA3_TEMPLATE_PREVIEW; type < CAMERA3_TEMPLATE_COUNT ; type++)
    {
        NvCameraHal3_Public_Controls defCntrlProps;
        CameraMetadata settings;
        // get Core's default settings
        NvCameraCore_GetDefaultControlProperties(hCameraCore,
            (NvCameraCoreUseCase)type, &(defCntrlProps.CoreCntrlProps));
        // get HAL maintained control props
        NvCameraHal3_GetDefaultJpegControlProps(defCntrlProps.JpegControls);
        defCntrlProps.AePrecaptureId = 0;
        NvMetadataTranslator::translateFromNvCamProperty(defCntrlProps, settings);
        mDefaultTemplates[type] = settings.release();
        mPublicControls[type] = defCntrlProps;
    }
}

camera_metadata_t *NvCameraHal3Core::getDefaultRequestSettings(int type)
{
    if ((type < CAMERA3_TEMPLATE_PREVIEW) ||
        (type > CAMERA3_TEMPLATE_COUNT))
    {
        //framework expects NULL to be returned in case of fatal error
        return NULL;
    }

    if (mTemplatesInited != NV_TRUE)
    {
        initDefaultTemplates();
        mTemplatesInited = NV_TRUE;
    }
    return mDefaultTemplates[type];
}

void NvCameraHal3Core::NvCameraHal3_GetDefaultJpegControlProps(
        NvCameraHal3_JpegControls& jpegControls)
{
    NvOsMemset(&jpegControls, 0, sizeof(NvCameraHal3_JpegControls));
    jpegControls.jpegQuality = DEFAULT_JPEG_QUALITY;
    jpegControls.thumbnailSize.width = DEFAULT_THUMBNAIL_WIDTH;
    jpegControls.thumbnailSize.height = DEFAULT_THUMBNAIL_HEIGHT;
    jpegControls.thumbnailQuality = DEFAULT_THUMBNAIL_QUALITY;
    jpegControls.jpegOrientation = DEFAULT_JPEG_ORIENTATION;

    // TODO:
    // pJpegControls->gpsCoordinates
    // pJpegControls->gpsProcessingMethod
    // pJpegControls->gpsTimeStamp
}

NvError NvCameraHal3Core::AcquireWakeLock()
{
    int status = 0;

    NV_LOGD(HAL3_CORE_TAG, "%s: ++", __FUNCTION__);

    if (mWakeLocked)
    {
        NV_LOGD(HAL3_CORE_TAG, "%s: -- (already locked)", __FUNCTION__);
        return NvSuccess;
    }

    status = acquire_wake_lock(PARTIAL_WAKE_LOCK, CAMERA_WAKE_LOCK_ID);
    if (status < 0)
    {
        NV_LOGE("%s: -- (error: could not acquire camera wake lock)", __FUNCTION__);
        return NvError_BadParameter;
    }

    mWakeLocked = NV_TRUE;
    NV_LOGD(HAL3_CORE_TAG, "%s: --", __FUNCTION__);
    return NvSuccess;
}

NvError NvCameraHal3Core::ReleaseWakeLock()
{
    int status = 0;

    NV_LOGD(HAL3_CORE_TAG, "%s: ++", __FUNCTION__);

    if (!mWakeLocked)
    {
        NV_LOGD(HAL3_CORE_TAG, "%s: -- (already released)", __FUNCTION__);
        return NvSuccess;
    }

    status = release_wake_lock(CAMERA_WAKE_LOCK_ID);
    if (status < 0)
    {
        NV_LOGE("%s: -- (error: could not release camera wake lock)", __FUNCTION__);
        return NvError_BadParameter;
    }

    mWakeLocked = NV_FALSE;
    NV_LOGD(HAL3_CORE_TAG, "%s: --", __FUNCTION__);
    return NvSuccess;
}

NvBool NvCameraHal3Core::isNewCrop(
    const NvRect& crop,
    NvS32 stream_width,
    NvS32 stream_height,
    NvRectF32 *crop_ratio)
{
    NvError err = NvSuccess;
    camera_metadata_entry e;

    NV_LOGV(HAL3_CORE_TAG, "%s: cropped region (%d, %d, %d, %d) %dx%d, stream: %dx%d", __FUNCTION__,
        crop.left, crop.top, crop.right, crop.bottom,
        crop.right - crop.left, crop.bottom - crop.top,
        stream_width, stream_height);

    NvF32 maxSensorModeAR = (NvF32)(mMaxSensorMode.Resolution.width) /
                    (NvF32)(mMaxSensorMode.Resolution.height);
    NvF32 currSensorModeAR = (NvF32)(mSensorMode.Resolution.width) /
                    (NvF32)(mSensorMode.Resolution.height);

    // If crop region is same as max res stream's resolution, then no
    // need to crop.
    if (stream_width == (crop.right - crop.left) &&
        stream_height == (crop.bottom - crop.top))
    {
        crop_ratio->left = 0;
        crop_ratio->top = 0;
        crop_ratio->right = 1;
        crop_ratio->bottom = 1;
        return NV_FALSE;
    }
    // If AR(current sensor mode) = AR(active pixel array size)
    // then apply the crop ratio as is
    else if (isARMatch(maxSensorModeAR, currSensorModeAR))
    {
        crop_ratio->left = (NvF32)crop.left/
            (NvF32)(mMaxSensorMode.Resolution.width);
        crop_ratio->top = (NvF32)crop.top/
            (NvF32)(mMaxSensorMode.Resolution.height);
        crop_ratio->right = (NvF32)crop.right/
            (NvF32)(mMaxSensorMode.Resolution.width);
        crop_ratio->bottom = (NvF32)crop.bottom/
            (NvF32)(mMaxSensorMode.Resolution.height);
    }
    // If AR(current sensor mode) != AR(active pixel array size)
    // adjust the crop region/ratio to match the aspect ratio of
    // the current sensor mode
    else if (maxSensorModeAR > currSensorModeAR)
    {
        // apply top, bottom crop ratio as is
        crop_ratio->top = (NvF32)crop.top /
            (NvF32)(mMaxSensorMode.Resolution.height);
        crop_ratio->bottom = (NvF32)crop.bottom /
            (NvF32)(mMaxSensorMode.Resolution.height);

        // calculate appropriate width
        NvU32 expectedWidth = currSensorModeAR *
            ((crop_ratio->bottom * mSensorMode.Resolution.height) -
             (crop_ratio->top * mSensorMode.Resolution.height));

        NvF32 dx = ((NvF32)mSensorMode.Resolution.width -
             (NvF32)expectedWidth) / 2.;

        // determine left, right crop ratio
        crop_ratio->left = dx /
            (NvF32)(mSensorMode.Resolution.width);
        crop_ratio->right = (NvF32)(mSensorMode.Resolution.width - dx) /
            (NvF32)(mSensorMode.Resolution.width);
    }
    else
    {
        // apply left, right crop ratio as is
        crop_ratio->left = (NvF32)crop.left /
            (NvF32)(mMaxSensorMode.Resolution.width);
        crop_ratio->right = (NvF32)crop.right /
            (NvF32)(mMaxSensorMode.Resolution.width);

        // calculate appropriate height
        NvU32 expectedHeight =
            ((crop_ratio->right * mSensorMode.Resolution.width)-
             (crop_ratio->left * mSensorMode.Resolution.width)) /
            currSensorModeAR;

        NvF32 dy = ((NvF32)mSensorMode.Resolution.height -
             (NvF32)expectedHeight) / 2.;

        // determine top, bottom crop ratio
        crop_ratio->top = dy /
            (NvF32)(mSensorMode.Resolution.height);
        crop_ratio->bottom = (NvF32)(mSensorMode.Resolution.height - dy) /
            (NvF32)(mSensorMode.Resolution.height);
    }

    return NV_TRUE;
}

void NvCameraHal3Core::updateDeviceOrientation(NvCamProperty_Public_Controls& prop)
{
    NvS32 corrected;
    // map [0, 359] to {0, 90, 180, 270}
    NvS32 NewOrientation =
        (((mDevOrientation.orientation() + 45)/90)*90)%360;

    if (NewOrientation != mOrientation)
    {
        // correct the orientation
        if (mSensorId == CAMERA_FACING_FRONT)
        {
            corrected = (mSensorOrientation - NewOrientation + 360)%360;
        }
        else
        {
            corrected = (mSensorOrientation + NewOrientation)%360;
        }
        prop.DeviceOrientation = corrected;
        mOrientation = NewOrientation;
    }
    else
    {
        prop.DeviceOrientation = mOrientation;
    }
    NV_LOGV(HAL3_CORE_TAG, "DeviceOrientation: %d", prop.DeviceOrientation);
}

camera3_error_msg_code_t
NvCameraHal3Core::nvErrorToHal3Error(NvError error)
{
    camera3_error_msg_code_t ret;
    switch (error)
    {
        case NvError_BadValue:
            // potentially fatal
            ret = CAMERA3_MSG_ERROR_DEVICE;
            break;
        default:
            // result metadata related failure
            ret = CAMERA3_MSG_ERROR_RESULT;
    }

    return ret;
}

} // namespace android
