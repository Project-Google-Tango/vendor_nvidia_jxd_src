/*
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit:
 *           NvCameraCore APIs</b>
 *
 * @b Description: Declares Interface for NvCameraCore APIs.
 */

#ifndef INCLUDED_NVCAMERA_CORE_H
#define INCLUDED_NVCAMERA_CORE_H

/** @defgroup NvCameraCore Camera API
 *
 * NvCameraCore is the source block of still and video image data.
 * It provides the functionality needed for capturing
 * image buffers in memory for further processing before
 * encoding or previewing. (For instance, one may wish
 * to scale or rotate the buffer before encoding or previewing.)
 * NvMM Camera has the ability to capture raw streams of
 * image data straight to memory, perform image processing,
 * reprogram the sensor for exposure and focusing (via calls to
 * physical camera layer (PCL)), and other behaviors expected of a digital
 * camera.
 *
 * @ingroup nvmm_modules
 * @{
 */

#include "nvmm.h"
#include "nvmm_event.h"
#include "nvcam_properties_public.h"


#if defined(__cplusplus)
extern "C"
{
#endif


typedef struct NvCameraCoreContextRec *NvCameraCoreHandle;

/**
 * Operation sequence:
 *
 * 1. The client calls NvCameraCore_Open() with a NvCameraCoreHandle and a
 *    sensor GUID to select a sensor out of multiple sensors on the system.
 *
 * 2. The client calls NvCameraCore_CallbackFunction() to set
 *    the event handler function which will be called when camera core
 *    sends event to the client. See NvCameraCoreEvent for the events.
 *
 * 3. The client calls NvCameraCore_GetStaticProperties() to get the static
 *    properties which describe the capabilities of camera software, sensor,
 *    flash, and focuser.
 *
 * 4. The client calls NvCameraCore_GetDefaultControlProperties() for a specific
 *    use case described by NvCameraCoreUseCase in order to get the default
 *    controls.
 *
 * 5. The client calls NvCameraCore_SetSensorMode() to select a sensor mode.
 *    Supported sensor modes will be returned as part of (3). Client can apply
 *    appropriate selection model to select a sensor mode depending on stream
 *    requirements. It should be noted that if the output buffers are configured
 *    smaller than the selected sensor mode then ISP will internally scale the data.
 *    This could be use to get around a situation when a sensor defines a mode
 *    which does not exactly match with standard resolutions.
 *
 * 6. The client modifies a copy of the default controls if it wants to control
 *    specific parameters in the camera. The client then caches this copy in
 *    order to apply controls to the next frame if any.
 *
 * 7. To send a capture request to camera core, the client calls
 *    NvCameraCore_FrameCaptureRequest() and passes the default or
 *    modified controls fetched using step (4).
 *
 * 8. When the sensor starts to integrate, camera core will call the event
 *    handler function set in step (2) with NvCameraCoreEvent_Shutter event.
 *    Please note that this is how android camera framework has defined the
 *    shutter callback however we currently do not support an event callback
 *    when sensor starts integrating. Hence we would instead send the event
 *    when frame capture request is enqueued to the HW.
 *
 * 9. When an output buffer is completed, camera block will call the event
 *    handler function set in step (2) with NvCameraCoreEvent_CompletedBuffer
 *    event to notify the client an output buffer is completed, along with it
 *    count of the number of completed buffer wrt to the number of o/p buffers
 *    sent in a Capture request is also sent. FrameNumber will be same for the
 *    buffers which are sent for a capture request. Frames are serviced in the
 *    order in which request is pushed. Hence the events are guaranteed to be
 *    in order.
 *
 * 10. Repeat (6) to (9) to request more frames.
 *
 * 11. The client calls NvCameraCore_Close() to close camera core.
 */


/**
 * --------------------------------------------------------------
 * Frame Reprocessing Request details
 * --------------------------------------------------------------
 *
 * Section provides details on how a client can use camera core APIs
 * to achieve image re-processing. An example of reprocessing can be
 * when a client wants to perform JPEG encoding on a captured frame.
 * A client can also use camera core to do Bayer reprocessing. There
 * are multiple use cases for reprocessing but the common underlying
 * theme related to camera core is that in case the original frame
 * was captured on this camera system, some meta data related to the
 * capture needs to be saved internally in the camera core so as when
 * at later stage in time a reprocessing request is issued we can refer
 * to that data. Example of such data can be the makernote or 3A states)
 * The architecture of reprocessing has been primarly driven by Android
 * V3 framework.
 *
 *
 * PROBLEM: Reprocessing request can be issued on any frame which was
 * captured by our camera system and at any time by the client. The
 * reprocessing request contains an input buffer(pInputBuffer) in
 * NvCameraCoreFrameCaptureRequest which had been captured in the past.
 * The request comes with the metadata, which also is neither guaranteed
 * to be the same metadata that was captured for that frame in the past nor
 * is enough for all the reprocessing.
 *
 * SOLUTION: To address this, we need to internally maintain/save the
 * metadata for the captures that *might be* re-used for reprocessing
 * later. And use it when the reprocessing request. A client of the
 * camera core knows about all the buffers it is going to use to issue
 * reprocessing request. So a client needs to explicitly notify the
 * camera core about this intention of issuing a future reprocess request
 * by setting the a unique identifier in request field named "FrameCaptureRequestId".
 * Core uses "FrameCaptureRequestId" to find out when it is ok to recycle
 * the memory used to store the captured frame metadata.
 *
 * Checklist:
 *
 * 1) Clients interested in capturing frames using camera core and later using
 *    the reprocessing path should set a unique id to the FrameCaptureRequestId.
 *    An example of a valid id can be the buffer handle when issuing the capture
 *    request. Clients are free to choose an identifier to be set as long as it
 *    is kept unique.
 *
 * 2) Clients not interested in reprocessing should leave the FrameCaptureRequestId
 *    set to 0.
 *
 * 3) Irrespective of a client being of type (1) or (2) it is important to set the
 *    the RequestFrameCount field in the FrameControlProps of the request.
 */


/**
 * @brief Defines the Frame capture request data
 */
typedef struct
{
    /* The frame number of the request which is later used to
     * match with a result.
     */
    NvU32 FrameNumber;

    /* The associated control settings for the frame capture request */
    NvCamProperty_Public_Controls FrameControlProps;

    /* An input buffer. This is used in case clients does a host capture (non-sensor capture)
     * and directly provides the input buffer.
     * TODO: Change this from NvMMBuffer to basic surfaces when we support it in the scaler
     */
    NvMMBuffer *pInputBuffer;

    /* The number of output buffers to store the output to */
    NvU32 NumOfOutputBuffers;

    /* Array of output buffers */
    NvMMBuffer **ppOutputBuffers;

    /* An unique identifier for the reprocess request which is used to reuse/freeup
     * some cached data associated with this request id. If its not a request
     * for reprocessing then just keep this field set to 0. Also if a client
     * is not using camera core to capture a frame then set it to 0. For
     * clients using camera core to capture and reprocess frames it is important
     * to set the RequestFrameCount meta data in FrameControlProps, since this will be
     * used later to match the reprocessing request in order to apply the
     * metadata related to frame capture. For more details on reprocessing check
     * the frame reprocessing section below.
     */
    NvU64 FrameCaptureRequestId;

    /* Crop region. HAL should translate the values read from 'ANDROID_SCALER_CROP_REGION' to
     * NvRect format and populate crop_rect.
     */
    NvRect crop_rect;
} NvCameraCoreFrameCaptureRequest;


/**
 * @brief Defines the information of a completed capture result.
 * Note that in android camera framework we are required to return
 * the result each time a single buffer request is completed. Hence
 * This structure will be used for both buffer event notification as
 * well as for completed capture result.
 */
typedef struct
{
    /* The frame number of which was supplied in the request. */
    NvU32 FrameNumber;

    /* Dynamic properties of the frame result. Ex 3A state etc */
    NvCamProperty_Public_Dynamic FrameDynamicProps;

    /* The number of the completed output buffers */
    NvU32 NumCompletedOutputBuffers;

    /* Array of completed output buffers
     * Change this to basic surfaces data type when we can
     * support it in the scaler.
     */
    NvMMBuffer **ppOutputBuffers;

} NvCameraCoreFrameCaptureResult;


/**
 * @brief Defines the information of a shutter event.
 */
typedef struct
{
    /* The timestamp when the sensor starts capture. In nanoseconds
     * Note that our PCL layer does not currently support this callback
     * and this currently indicates the time when frame capture request
     * is enqueued to the hardware.
     */
    NvU64 Timestamp;

    /* The frame number of the request which is later used to match with a result */
    NvU32 FrameNumber;

} NvCameraCoreShutterEventInfo;

/**
 * @brief Defines events that will be sent from camera block to client.
 * Client can use these events to take appropriate actions
 * @see SendEvent
 * @ingroup nvcamerablock
 */
typedef enum
{
    /*
     * The client will receive one shutter event for every request. The event
     * comes with a pointer to NvCameraCoreShutterEventInfo.
     */
    NvCameraCoreEvent_Shutter = NvMMEventCamera_EventOffset,

    /*
     * The client may recieve multiple partial result callbacks for a
     * single capture request. The event comes with a pointer to
     * NvCameraCoreFrameCaptureResult. Client needs to check for
     * dynamic properties returned as part of partial result callback
     * to find out what changed.
     */
    NvCameraCoreEvent_PartialResultReady,

    /* The client will receive one completed buffer event for every
     * output buffer requested by NvCameraCore_FrameCaptureRequest().
     * The event comes with a pointer to NvCameraCoreFrameCaptureResult.
     */
    NvCameraCoreEvent_CompletedBuffer,

    /* In case of errors an event will be sent and result will contain
     * error details.
     */
    NvCameraCoreEvent_Error,

    NvCameraCoreEvent_Force32 = 0x7FFFFFFF,

} NvCameraCoreEvent;


/*
 * /////////////////////  NvCamera Core Interfaces /////////////////////////
 */


/**
 * @brief Open the camera core block and allocate a handle to it.
 *
 * @param phCameraCore A pointer to the camera core handle which needs to be initialized.
 * @param ImagerGUID A unique identifier for the imager to be opened.
 *
 * @retval Various errors.
 *
 */
NvError NvCameraCore_Open(NvCameraCoreHandle *phCameraCore, NvU64 ImagerGUID);


/**
 *
 * @brief NvCamera Core event callback
 *
 * All notifications from a camera core to its client leverage the following
 * callback type:
 *
 * @param pContext Private context pointer for the client event call back.
 * @param EventType type of event passed
 * @param EventInfoSize the size of the event info buffer.
 * @param pEventInfo Pointer to event info. This memory is owned exclusively by
 * the caller and is only guarenteed to be valid for duration of the call.
 *
 * @retval Various errors.
 *
 */
typedef NvError (*NvCameraCoreEventCallbackFunction)(
    void *pContext,
    NvU32 EventType,
    NvU32 EventInfoSize,
    void *pEventInfo);

/**
 * @brief Set core notification callback given by client.
 *
 * @param hCameraCore A camera core handle
 * @param ClientContext Pointer to client context
 * @param CallbackFunction Pointer to the notification callback function
 *
 * @retval Various errors.
 *
 */
NvError NvCameraCore_CallbackFunction(
    NvCameraCoreHandle hCameraCore,
    void *pClientContext,
    NvCameraCoreEventCallbackFunction CallbackFunction);


/**
 * @brief Get the static properties of the camera.
 *
 * @param hCameraCore A camera core handle
 * @param pOutStaticProperties An output handle to the static properties.
 *
 * @retval Various errors.
 */
NvError NvCameraCore_GetStaticProperties(
    NvCameraCoreHandle hCameraCore,
    NvCamProperty_Public_Static *pOutStaticProps);


/**
 * @brief Get the default control properties of the camera for a specific
 * use case.
 *
 * @param hCameraCore A camera core handle
 * @param UseCase A NvCameraCoreUseCase defining the use case.
 * @param pOutDefaultControlProps An output handle to the default control
 * properties.
 * @retval Various errors.
 *
 */
NvError NvCameraCore_GetDefaultControlProperties(
    NvCameraCoreHandle hCameraCore,
    NvCameraCoreUseCase UseCase,
    NvCamProperty_Public_Controls *pOutDefaultControlProps);


/**
 * @brief Set the sensor resolution in the camera. Sensor mode
 * list is returned as part of the NvCameraCore_GetDefaultControlProperties()
 * call. Note that the final output of the camera depends on the
 * size of the output buffer supplied in the frame capture request.
 * Camera core will scale the output according to the size of output
 * buffers.
 *
 * @param hCameraCore A camera core handle
 * @param RequestedSensorMode A sensor mode to start the sensor in.
 *
 * @retval Various errors.
 *
 */
NvError NvCameraCore_SetSensorMode(
    NvCameraCoreHandle hCameraCore,
    NvMMCameraSensorMode RequestedSensorMode);

/**
 * @brief Flush not executed requests
 * @param hCameraCore A camera core handle
 */
NvError NvCameraCore_Flush(
    NvCameraCoreHandle hCameraCore);


/**
 * @brief Request a frame capture from the camera core driver.
 *
 * @param hCameraCore A camera core handle
 * @param pFrameRequest A request containing information about the
 * frame to be captured.
 *
 * @retval Various errors.
 *
 */
NvError NvCameraCore_FrameCaptureRequest(
    NvCameraCoreHandle hCameraCore,
    NvCameraCoreFrameCaptureRequest *pFrameRequest);


/**
 * @brief Releases the metadata associated with a frame captured by the camera
 * core. A clients need to set the FrameCaptureRequestId when issuing capture
 * request. Please refer to the frame reprocessing path comment block above
 * for more details.
 *
 * @param hCameraCore A camera core handle
 * @param pCapturedFrameMetadataList A list of FrameCaptureRequestId associated with
 * the frames captured.
 * @param CaptureFrameMetadataListEntryCount No of entries that the pCapturedFrameMetadataList
 * contains.
 *
 * @retval Various errors.
 *
 */
NvError NvCameraCore_ReleaseCapturedFrameMetadataList(
    NvCameraCoreHandle hCameraCore,
    NvU64 *pCapturedFrameMetadataList,
    NvU32 CaptureFrameMetadataListEntryCount);

/**
 * @brief Close the camera core driver and release all resources held by it.
 *
 * @param hCameraCore A camera core handle
 *
 */
void NvCameraCore_Close(NvCameraCoreHandle hCameraCore);

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVCAMERA_CORE_H
