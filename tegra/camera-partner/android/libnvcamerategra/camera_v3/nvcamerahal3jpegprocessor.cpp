/*
 * Copyright (c) 2013 - 2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvcamerahal3jpegprocessor.h"
#include <nvmm_util.h>
#include <nvgr.h>
#include "NVOMX_EncoderExtensions.h"
#include "nvcamerahal3_tags.h"
#include "nv_log.h"
#include "camera_trace.h"

#define MAKERNOTE_FRAME_AGNOSTIC_SIZE    2048
#define JPEG_ENCODER_MEM_ALIGN          1024

// Define the prefixes that we need to pass down to the JPEG encoder
// for proper handling of some EXIF header data.
// As of now, android only uses ASCII, but the others are supported by the JPEG
// encoder, so we will place hooks for them here to aid future expansion.
#define EXIF_PREFIX_LENGTH    8
#define EXIF_PREFIX_ASCII     {0x41, 0x53, 0x43, 0x49, 0x49, 0x00, 0x00, 0x00}

namespace android {

JpegProcessor::JpegProcessor(const wp<StreamPort> client)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: ++", __FUNCTION__);
    mClient = client;
    mJpegOpenFlag = 0;
    pJpegEncoder = new JpegEncoder;
    NvRmOpen(&mRmDevice, 0);
    mBypass = false;
    mWorking = false;

    NvOsSemaphoreCreate(&mJpegWorkSema, 0);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: --", __FUNCTION__);
}

JpegProcessor::~JpegProcessor()
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: ++", __FUNCTION__);
    flush();

    if (pJpegEncoder)
    {
        pJpegEncoder->Close();
        mJpegOpenFlag = 0;
        delete pJpegEncoder;
        NvRmClose(mRmDevice);
    }

    NvOsSemaphoreDestroy(mJpegWorkSema);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: --", __FUNCTION__);
}

NvError JpegProcessor::enqueueJpegWork(
    BufferInfo *pOutBufferInfo,
    NvMMBuffer *thumbnailBuffer)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: ++", __FUNCTION__);

    BufferInfo *pBufferInfo = new BufferInfo;
    NvOsMemcpy(pBufferInfo, pOutBufferInfo, sizeof(BufferInfo));

    JpegWorkItem *pJpegWorkItem = new JpegWorkItem;
    pJpegWorkItem->pBufferInfo = pBufferInfo;
    pJpegWorkItem->thumbnailBuffer = thumbnailBuffer;

    Mutex::Autolock lock(mWorkQMutex);

    mJpegWorkQ.push_back(pJpegWorkItem);
    NvOsSemaphoreSignal(mJpegWorkSema);

    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: --",__FUNCTION__);
    return NvSuccess;
}

bool JpegProcessor::threadLoop()
{
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: ++", __FUNCTION__);
    NvError err = NvSuccess;
    NvOsSemaphoreWait(mJpegWorkSema);

    mWorkQMutex.lock();
    JpegWorkQueue::iterator itr = mJpegWorkQ.begin();

    while (itr != mJpegWorkQ.end())
    {
        JpegWorkItem *pJpegWorkItem = (*itr);

        BufferInfo *pBufferInfo = pJpegWorkItem->pBufferInfo;
        NvMMBuffer *thumbnailBuffer = pJpegWorkItem->thumbnailBuffer;
        mWorking = true;
        mWorkQMutex.unlock();

        err = processJpegFrame(pBufferInfo, thumbnailBuffer);
        if (err != NvSuccess)
        {
            NV_LOGE("%s: jpeg encoding fail", __FUNCTION__);
            return false;
        }

        mWorkQMutex.lock();
        mJpegWorkQ.erase(itr);

        //delete pBufferInfo
        delete pBufferInfo;
        pBufferInfo = NULL;

        delete pJpegWorkItem;
        pJpegWorkItem = NULL;

        itr = mJpegWorkQ.begin();
    }

    mWorking = false;

    //condition signal
    mDoneCond.signal();
    mWorkQMutex.unlock();

    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: -- error [0x%x]", __FUNCTION__, err);
    return true;
}

NvError JpegProcessor::processJpegFrame(
    BufferInfo *pBufferInfo,
    NvMMBuffer *thumbnailBuffer)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: ++", __FUNCTION__);
    NvError e = NvSuccess;
    sp<StreamPort> streamPort = mClient.promote();

    if (streamPort == 0)
    {
        return NvError_BadParameter;
    }

    if (!mBypass)
    {
        NvRectF32 rect;
        NvOsMemset(&rect, 0, sizeof(NvRectF32));

        camera3_stream_buffer *outBuffer = pBufferInfo->outBuffer;
        int frameNumber = pBufferInfo->frameNumber;
        NvMMBuffer *outNvmmBuffer = streamPort->NativeToNVMMBuffer(outBuffer->buffer);

        // this function takes long time
        e = doJpegEncoding(outNvmmBuffer, thumbnailBuffer,
            outBuffer->buffer, pBufferInfo->pJpegSettings);
        if (e != NvSuccess)
        {
            NV_LOGE("%s: error in encoding jpeg",__FUNCTION__);
            goto fail;
        }
    }

    // set bufferInfo in OutputQ to ready state
    e = streamPort->notifyJpegDone(pBufferInfo, thumbnailBuffer);
    if (e != NvSuccess)
    {
        NV_LOGE("%s: error in notify jpeg done",__FUNCTION__);
    }

    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: --", __FUNCTION__);
    return e;

fail:
    streamPort->notifyJpegDone(pBufferInfo, thumbnailBuffer);
    NV_LOGE("%s: -- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError JpegProcessor::doJpegEncoding(
    NvMMBuffer *pNvMMSrc,
    NvMMBuffer *ThumbnailBuffer,
    buffer_handle_t *pANBDst,
    NvCameraHal3_JpegSettings *pJpegSettings)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: ++",__FUNCTION__);

    NvError e;
    JpegParams Params;
    NvOsMemset(&Params, 0, sizeof(JpegParams));

    constructJpegParams(&(Params.jpegParams), pJpegSettings);
    if (pJpegSettings->JpegControls.thumbnailSize.width &&
        pJpegSettings->JpegControls.thumbnailSize.height)
    {
        constructThumbnailParams(&(Params.thumbJpegParams), pJpegSettings);
    }
    // Note that Params.pExifInfo is just a pointer
    Params.pExifInfo = &(pJpegSettings->exifInfo);
    // copy the makernote struct containing size and the pointer to
    // the binary blob for makernote
    strncpy(Params.makerNoteData, pJpegSettings->makerNoteData,
            MAKERNOTE_DATA_SIZE);

    NV_CHECK_ERROR_CLEANUP(ConvertToJpeg(
        pNvMMSrc,ThumbnailBuffer, pANBDst, &Params));

    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: --",__FUNCTION__);
    return e;
fail:
    NV_LOGE("%s: -- (error 0x%x)", __FUNCTION__, e);
    return e;
}

NvError JpegProcessor::ConvertToJpeg(
    NvMMBuffer *pNvMMSrc,
    NvMMBuffer *ThumbnailBuffer,
    buffer_handle_t *pANBDest,
    JpegParams *pJpegParams)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    // Jpeg params are already filled
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: ++", __FUNCTION__);
    NvError err = NvSuccess;
    NvBool needsThumbnail = NV_FALSE;
    NvError status = NvSuccess;

    NvMMBuffer *JpegBuffer = (NvMMBuffer *)NvOsAlloc(sizeof(NvMMBuffer));
    if (!JpegBuffer)
    {
        NV_LOGE("%s: Failed to allocate NvMM Buffer at %d", __FUNCTION__, __LINE__);
        return NvError_InsufficientMemory;
    }
    NvOsMemset(JpegBuffer, 0, sizeof(NvMMBuffer));

    // allocate nvrm handles which will be used by the jpeg library
    // TODO: This should ideally be done in linkBuffers where surfaces for jpeg
    // BLOBs are allocated. And should be deallocated in unlinkBuffers. But
    // there is a Bug if we do it there, which needs to be fixed.
    status = NvRmMemHandleAlloc(mRmDevice, NULL, 0, 32,
            NvOsMemAttribute_InnerWriteBack, sizeof(NvMMEXIFInfo), 0, 0,
            &pNvMMSrc->PayloadInfo.BufferMetaData.CameraBufferMetadata.
            EXIFInfoMemHandle);

    status = NvRmMemHandleAlloc(mRmDevice, NULL, 0, 32,
            NvOsMemAttribute_InnerWriteBack, MAKERNOTE_DATA_SIZE, 0, 0,
            &pNvMMSrc->PayloadInfo.BufferMetaData.CameraBufferMetadata.
            MakerNoteExtensionHandle);

    pNvMMSrc->PayloadInfo.BufferMetaData.EXIFBufferMetadata.EXIFInfoMemHandle =
        pNvMMSrc->PayloadInfo.BufferMetaData.CameraBufferMetadata.
        EXIFInfoMemHandle;

    pNvMMSrc->PayloadInfo.BufferMetaData.EXIFBufferMetadata.MakerNoteExtensionHandle =
        pNvMMSrc->PayloadInfo.BufferMetaData.CameraBufferMetadata.
        MakerNoteExtensionHandle;

    NvRmMemHandle hExif;
    NvRmMemHandle hMakerNote;

    hExif = pNvMMSrc->PayloadInfo.BufferMetaData.CameraBufferMetadata.
        EXIFInfoMemHandle;
    hMakerNote = pNvMMSrc->PayloadInfo.BufferMetaData.CameraBufferMetadata.
        MakerNoteExtensionHandle;

    // TODO: temporarily here until core implements makernote
    pJpegParams->pExifInfo->SizeofMakerNoteInBytes =
        MAKERNOTE_FRAME_AGNOSTIC_SIZE;

    NvRmMemWrite(
            hExif,
            0,
            pJpegParams->pExifInfo,
            sizeof(NvMMEXIFInfo));

    NvRmMemWrite(
            hMakerNote,
            0,
            pJpegParams->makerNoteData,
            MAKERNOTE_DATA_SIZE);

    NvU32 enc_flag = PRIMARY_ENC;
    enc_flag |= ThumbnailBuffer ? THUMBNAIL_ENC : 0;

    if (enc_flag != mJpegOpenFlag)
    {
        pJpegEncoder->Close();
        mJpegOpenFlag = 0;
        err = pJpegEncoder->Open(enc_flag, NvImageEncoderType_HwEncoder);
        if (err == NvSuccess)
        {
            mJpegOpenFlag = enc_flag;
        }
    }

    if (err == NvSuccess)
    {
        // TODO: keep this to MAX_JPEG_SIZE until jpeg encoder library fixes
        // their api to fetch the right encoded size before sending the request
        pJpegParams->jpegParams.EncodedSize = MAX_JPEG_SIZE;
        // set params for jpeg
        pJpegEncoder->SetParam(&(pJpegParams->jpegParams));
#if 0
        // TODO: Enable this when jpeg encoder library fixes their api
        // Get params for the right output size from the encoder
        pJpegEncoder->GetParam(&(pJpegParams->jpegParams));
        NV_LOGV(HAL3_JPEG_PROCESS_TAG, "%s: Encoded size suggested = %d", __FUNCTION__,
                pJpegParams->jpegParams.EncodedSize);
        if (pJpegParams->jpegParams.EncodedSize > MAX_JPEG_SIZE)
        {
            NV_LOGE("%s: error: encoded jpeg size could be more than the maxsize",
                    __FUNCTION__);
            NvOsFree(JpegBuffer);
            pJpegEncoder->Close();
            return NvError_InsufficientMemory;
        }
#endif
        err = NvMMUtilAllocateBuffer(
                mRmDevice,
                pJpegParams->jpegParams.EncodedSize,
                JPEG_ENCODER_MEM_ALIGN,
                NvMMMemoryType_WriteBack,
                NV_FALSE,
                &JpegBuffer);
        if (err != NvSuccess)
        {
            NV_LOGE("%s: Failed to allocate contiguous memory", __FUNCTION__);
            NvOsFree(JpegBuffer);
            pJpegEncoder->Close();
            return NvError_InsufficientMemory;
        }
        if (ThumbnailBuffer)
        {
            pJpegEncoder->SetParam(&(pJpegParams->thumbJpegParams));
        }
        err = pJpegEncoder->Encode(pNvMMSrc, JpegBuffer, ThumbnailBuffer);
        if (err != NvSuccess)
        {
            NV_LOGE("%s: Error encoding jpeg err = 0x%x", __FUNCTION__, err);
        }
    }

    // free rm handles
    NvRmMemHandleFree(pNvMMSrc->PayloadInfo.BufferMetaData.
                    CameraBufferMetadata.MakerNoteExtensionHandle);
    NvRmMemHandleFree(pNvMMSrc->PayloadInfo.BufferMetaData.
                CameraBufferMetadata.EXIFInfoMemHandle);

    NV_LOGV(HAL3_JPEG_PROCESS_TAG, "Jpeg size=%d", JpegBuffer->Payload.Ref.sizeOfValidDataInBytes);
    // TODO: keep this check here until jpeg library fixes their api to report
    // size before encoding
    if (JpegBuffer->Payload.Ref.sizeOfValidDataInBytes > MAX_JPEG_SIZE)
    {
        err = NvError_InsufficientMemory;
        goto cleanup;
    }

    {
        camera3_jpeg_blob_t jpeg_blob;
        buffer_handle_t handle = *pANBDest;
        const NvRmSurface *Surf;

        nvgr_get_surfaces(handle, &Surf, NULL);

        NvRmMemWrite(
            Surf[0].hMem,
            0,
            (void *)((NvU32)JpegBuffer->Payload.Ref.pMem +
            JpegBuffer->Payload.Ref.startOfValidData),
            JpegBuffer->Payload.Ref.sizeOfValidDataInBytes);

        // write camera3_jpeg_blob_t at the end of JPEG buffer.
        // Please refer to camera3.h
        jpeg_blob.jpeg_blob_id = CAMERA3_JPEG_BLOB_ID;
        jpeg_blob.jpeg_size = JpegBuffer->Payload.Ref.sizeOfValidDataInBytes;
        NvRmMemWrite(
            Surf[0].hMem,
            MAX_JPEG_SIZE - sizeof(camera3_jpeg_blob_t),
            (void *)&jpeg_blob,
            sizeof(camera3_jpeg_blob_t));
    }
cleanup:
    NvMMUtilDeallocateBuffer(JpegBuffer);
    NvOsFree(JpegBuffer);

    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: --", __FUNCTION__);
    return err;
}


static NvMMExif_Orientation GetOrientation(NvU16 rotation)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    NvMMExif_Orientation orientation;
    // We don't need to consider flip here because flip is always
    // done physically by DZ block
    switch(rotation)
    {
        case 90:
            orientation = NvMMExif_Orientation_270_Degrees;
            break;
        case 180:
            orientation = NvMMExif_Orientation_180_Degrees;
            break;
        case 270:
            orientation = NvMMExif_Orientation_90_Degrees;
            break;
        case 0:
        default:
            orientation = NvMMExif_Orientation_0_Degrees;
    }
    return orientation;
}

void JpegProcessor::constructJpegParams(NvJpegEncParameters *pParams,
        NvCameraHal3_JpegSettings *pJpegSettings)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: ++",__FUNCTION__);
    NvS32 physicalRotation = 0;

    // set parameters
    pParams->Quality = pJpegSettings->JpegControls.jpegQuality;
    pParams->InputFormat = NV_IMG_JPEG_COLOR_FORMAT_420;
    pParams->Orientation.eRotation = NvJpegImageRotate_ByExifTag;
    pParams->Orientation.orientation =
            GetOrientation(pJpegSettings->JpegControls.jpegOrientation);

    constructGPSParams(pParams, pJpegSettings);

    pParams->Resolution.width = pJpegSettings->jpegResolution.width;
    pParams->Resolution.height = pJpegSettings->jpegResolution.height;

#if ENABLE_BLOCKLINEAR_SUPPORT
    pParams->EncSurfaceType = NV_IMG_JPEG_SURFACE_TYPE_NV12;
#else
    pParams->EncSurfaceType = NV_IMG_JPEG_SURFACE_TYPE_YV12;
#endif

    // bug in encoder library, this needs to be false to tell it
    // that we're setting still params, even if we enable thumbnail eventually
    pParams->EnableThumbnail = NV_FALSE;

    constructExifInfo(pParams, pJpegSettings);

    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: --",__FUNCTION__);
    return;
}

void JpegProcessor::constructGPSParams(NvJpegEncParameters *pParams,
        NvCameraHal3_JpegSettings *pJpegSettings)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: ++", __FUNCTION__);
    if (pJpegSettings->JpegControls.gpsCoordinates[0])
    {
        unsigned int gpsLatitude;
        NvBool latitudeDirection;
        parseDegrees(pJpegSettings->JpegControls.gpsCoordinates[0], &gpsLatitude, &latitudeDirection);
        SetGpsLatitude(
                true,
                gpsLatitude,
                latitudeDirection,
                pParams);
    }
    if (pJpegSettings->JpegControls.gpsCoordinates[1])
    {
        unsigned int gpsLongitude;
        NvBool longitudeDirection;
        parseDegrees(pJpegSettings->JpegControls.gpsCoordinates[1], &gpsLongitude, &longitudeDirection);
        SetGpsLongitude(
                true,
                gpsLongitude,
                longitudeDirection,
                pParams);
    }
    if (pJpegSettings->JpegControls.gpsCoordinates[2])
    {
        float gpsAltitude;
        NvBool gpsAltitudeRef;
        parseAltitude(pJpegSettings->JpegControls.gpsCoordinates[2], &gpsAltitude, &gpsAltitudeRef);
        SetGpsAltitude(
                true,
                gpsAltitude,
                gpsAltitudeRef,
                pParams);
    }
    if (pJpegSettings->JpegControls.gpsTimeStamp)
    {
        NvCameraUserTime time;
        parseTime((time_t)pJpegSettings->JpegControls.gpsTimeStamp, &time);
        SetGpsTimestamp(
                true,
                time,
                pParams);
    }
    if (strlen(pJpegSettings->JpegControls.gpsProcessingMethod) > 0)
    {
        SetGpsProcMethod(
                true,
                pJpegSettings->JpegControls.gpsProcessingMethod,
                pParams);
    }
    pParams->EncParamGps.Enable = NV_TRUE;
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: --", __FUNCTION__);
}

void JpegProcessor::constructThumbnailParams(NvJpegEncParameters *pParams,
                    NvCameraHal3_JpegSettings *pJpegSettings)
{
    NvS32 physicalRotation = 0;
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: ++", __FUNCTION__);
    NvOsMemset(pParams, 0, sizeof(NvJpegEncParameters));
    pParams->Quality = pJpegSettings->JpegControls.thumbnailQuality;    // hardcoding to 80
    pParams->Orientation.eRotation = NvJpegImageRotate_ByExifTag;
    pParams->Orientation.orientation =
            GetOrientation(pJpegSettings->JpegControls.jpegOrientation);
    pParams->Resolution.width = pJpegSettings->JpegControls.thumbnailSize.width;
    pParams->Resolution.height = pJpegSettings->JpegControls.thumbnailSize.height;
#if ENABLE_BLOCKLINEAR_SUPPORT
    pParams->EncSurfaceType = NV_IMG_JPEG_SURFACE_TYPE_NV12;
#else
    pParams->EncSurfaceType = NV_IMG_JPEG_SURFACE_TYPE_YV12;
#endif
    // not because we're enabling it, but because
    // these settings are for thumbnail
    pParams->EnableThumbnail = NV_TRUE;
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: --", __FUNCTION__);
    return;
}

void JpegProcessor::parseDegrees(double coord, unsigned int *ddmmss, NvBool *direction)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    *direction = (coord >= 0) ? NV_TRUE : NV_FALSE;

    coord = fabs(coord);

    int degrees = floor(coord);
    coord -= degrees;
    coord *= 60;
    int minutes = floor(coord);
    coord -= minutes;
    coord *= 60;
    int seconds = round(coord * 1000);

    *ddmmss = ((degrees & 0xff) << 24) |
        ((minutes & 0xff) << 16) |
        (seconds & 0xffff);
}

void JpegProcessor::parseAltitude(double input, float *alt, NvBool *ref)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    *alt = fabs(input);
    *ref = (input >= 0) ? NV_FALSE : NV_TRUE;
}

void JpegProcessor::parseTime(time_t input, NvCameraUserTime* time)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    struct tm *datetime = gmtime(&input);

    if (datetime == NULL)
    {
        memset(time, 0x0, sizeof(NvCameraUserTime));
    }
    else
    {
        time->hr  = datetime->tm_hour;
        time->min = datetime->tm_min;
        time->sec = datetime->tm_sec;

        /* populate date */
        NvOsSnprintf(time->date,
            MAX_DATE_LENGTH,
            "%04d:%02d:%02d", 1900+datetime->tm_year,
                              1+datetime->tm_mon,
                              datetime->tm_mday);
    }
}

void JpegProcessor::SetGpsLatitude(
    bool Enable,
    unsigned int lat,
    bool dir,
    NvJpegEncParameters *Params)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    if (dir == true)
    {
        Params->EncParamGps.GpsInfo.GPSLatitudeRef[0] = 'N';
    }
    else
    {
        Params->EncParamGps.GpsInfo.GPSLatitudeRef[0] = 'S';
    }
    Params->EncParamGps.GpsInfo.GPSLatitudeRef[1] = '\0';

    Params->EncParamGps.GpsInfo.GPSLatitudeNumerator[0] = (lat & (0x00ff << 24)) >> 24;
    Params->EncParamGps.GpsInfo.GPSLatitudeDenominator[0] = 1;
    Params->EncParamGps.GpsInfo.GPSLatitudeNumerator[1] = (lat & (0x00ff << 16)) >> 16;
    Params->EncParamGps.GpsInfo.GPSLatitudeDenominator[1] = 1;
    Params->EncParamGps.GpsInfo.GPSLatitudeNumerator[2] = lat & 0xffff;
    Params->EncParamGps.GpsInfo.GPSLatitudeDenominator[2] = 1000;

    if (Enable)
    {
        Params->EncParamGps.GpsInfo.GPSBitMapInfo |= OMX_ENCODE_GPSBitMapLatitudeRef;
    }
    else
    {
        Params->EncParamGps.GpsInfo.GPSBitMapInfo &= ~OMX_ENCODE_GPSBitMapLatitudeRef;
    }
}

void JpegProcessor::SetGpsLongitude(
    bool Enable,
    unsigned int lon,
    bool dir,
    NvJpegEncParameters *Params)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    if (dir == true)
    {
        Params->EncParamGps.GpsInfo.GPSLongitudeRef[0] = 'E';
    }
    else
    {
        Params->EncParamGps.GpsInfo.GPSLongitudeRef[0] = 'W';
    }
    Params->EncParamGps.GpsInfo.GPSLongitudeRef[1] = '\0';

    Params->EncParamGps.GpsInfo.GPSLongitudeNumerator[0] = (lon & (0x00ff << 24)) >> 24;
    Params->EncParamGps.GpsInfo.GPSLongitudeDenominator[0] = 1;
    Params->EncParamGps.GpsInfo.GPSLongitudeNumerator[1] = (lon & (0x00ff << 16)) >> 16;
    Params->EncParamGps.GpsInfo.GPSLongitudeDenominator[1] = 1;
    Params->EncParamGps.GpsInfo.GPSLongitudeNumerator[2] = lon & 0xffff;
    Params->EncParamGps.GpsInfo.GPSLongitudeDenominator[2] = 1000;

    if (Enable)
    {
        Params->EncParamGps.GpsInfo.GPSBitMapInfo |=
            OMX_ENCODE_GPSBitMapLongitudeRef;
    }
    else
    {
        Params->EncParamGps.GpsInfo.GPSBitMapInfo &=
            ~OMX_ENCODE_GPSBitMapLongitudeRef;
    }
}

void JpegProcessor::SetGpsAltitude(
    bool Enable,
    float altitude,
    bool ref,
    NvJpegEncParameters *Params)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    Params->EncParamGps.GpsInfo.GPSAltitudeRef = (ref == true) ? 1 : 0;
    Params->EncParamGps.GpsInfo.GPSAltitudeNumerator = (int)altitude;
    Params->EncParamGps.GpsInfo.GPSAltitudeDenominator = 1;

    if (Enable)
    {
        Params->EncParamGps.GpsInfo.GPSBitMapInfo |=
            OMX_ENCODE_GPSBitMapAltitudeRef;
    }
    else
    {
        Params->EncParamGps.GpsInfo.GPSBitMapInfo &=
            ~OMX_ENCODE_GPSBitMapAltitudeRef;
    }
}

void JpegProcessor::SetGpsTimestamp(
    bool Enable,
    NvCameraUserTime time,
    NvJpegEncParameters *Params)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    NvOsStrncpy(
        (char *)Params->EncParamGps.GpsInfo.GPSDateStamp,
        time.date,
        sizeof(Params->EncParamGps.GpsInfo.GPSDateStamp));

    Params->EncParamGps.GpsInfo.GPSTimeStampNumerator[0] = time.hr;
    Params->EncParamGps.GpsInfo.GPSTimeStampDenominator[0] = 1;
    Params->EncParamGps.GpsInfo.GPSTimeStampNumerator[1] = time.min;
    Params->EncParamGps.GpsInfo.GPSTimeStampDenominator[1] = 1;
    Params->EncParamGps.GpsInfo.GPSTimeStampNumerator[2] = time.sec;
    Params->EncParamGps.GpsInfo.GPSTimeStampDenominator[2] = 1;

    if (Enable)
    {
        Params->EncParamGps.GpsInfo.GPSBitMapInfo |=
            (OMX_ENCODE_GPSBitMapTimeStamp | OMX_ENCODE_GPSBitMapDateStamp);
    }
    else
    {
        Params->EncParamGps.GpsInfo.GPSBitMapInfo &=
            ~(OMX_ENCODE_GPSBitMapTimeStamp | OMX_ENCODE_GPSBitMapDateStamp);
    }
}

void JpegProcessor::SetGpsProcMethod(
    bool Enable,
    const char *str,
    NvJpegEncParameters *Params)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    char prefix[EXIF_PREFIX_LENGTH] = EXIF_PREFIX_ASCII;
    NvOsMemcpy(
        Params->EncParamGpsProc.GpsProcMethodInfo.GPSProcessingMethod,
        prefix,
        EXIF_PREFIX_LENGTH);
    NvOsStrncpy(
        (char *)(Params->EncParamGpsProc.GpsProcMethodInfo.GPSProcessingMethod +
            EXIF_PREFIX_LENGTH),
        str,
        sizeof(Params->EncParamGpsProc.GpsProcMethodInfo.GPSProcessingMethod) -
            EXIF_PREFIX_LENGTH);
    if (Enable)
    {
        Params->EncParamGps.GpsInfo.GPSBitMapInfo |=
            OMX_ENCODE_GPSBitMapProcessingMethod;
    }
    else
    {
        Params->EncParamGps.GpsInfo.GPSBitMapInfo &=
            ~OMX_ENCODE_GPSBitMapProcessingMethod;
    }
}

// Taken from settings' ProgramExifInfo functions. Will either go away or will
// take dynamic values once FRD infrastructure is in place. Hardcoded values
// will be removed.
NvError JpegProcessor::constructExifInfo(NvJpegEncParameters *pParams,
        NvCameraHal3_JpegSettings *pJpegSettings)
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: ++", __FUNCTION__);
    NvError e = NvSuccess;
    NvImageExifParams *pExifParams;
    pExifParams = &(pParams->EncParamExif);
    char exifModel[EXIF_STRING_LENGTH] = "Development Platform";
    char exifMake[EXIF_STRING_LENGTH] = "NVIDIA";
    NvOsMemcpy(
        pExifParams->ExifInfo.Make,
        exifMake,
        EXIF_STRING_LENGTH);
    NvOsMemcpy(
        pExifParams->ExifInfo.Model,
        exifModel,
        EXIF_STRING_LENGTH);
    // set user comment (needs prefix)
    char prefix[EXIF_PREFIX_LENGTH] = EXIF_PREFIX_ASCII;
    NvOsMemcpy(pExifParams->ExifInfo.UserComment, prefix, EXIF_PREFIX_LENGTH);
    NvOsStrncpy(pExifParams->ExifInfo.UserComment + EXIF_PREFIX_LENGTH,
        "NVIDIA", strlen("NVIDIA") + 1);
    // set make (no prefix)
    NvOsStrncpy(pExifParams->ExifInfo.Make, "NVIDIA-MAKE",
                strlen("NVIDIA-MAKE") + 1);
    // set model (no prefix)
    NvOsStrncpy(pExifParams->ExifInfo.Model, "NVIDIA-MODEL",
                strlen("NVIDIA-MODEL") + 1);
    // set all three timestamp fields with current system time
    time_t currTime;
    time(&currTime);
    struct tm *datetime = localtime(&currTime);
    if (datetime == NULL)
    {
        NV_LOGE("programExifInfo: error getting current time from system");
    }
    else
    {
        NvOsSnprintf(pExifParams->ExifInfo.DateTime,
            sizeof(pExifParams->ExifInfo.DateTime),
            "%04d:%02d:%02d %02d:%02d:%02d",
            1900 + datetime->tm_year,
            1 + datetime->tm_mon,
            datetime->tm_mday,
            datetime->tm_hour,
            datetime->tm_min,
            datetime->tm_sec);
        NvOsStrncpy(pExifParams->ExifInfo.DateTimeOriginal,
                    pExifParams->ExifInfo.DateTime,
                    sizeof(pExifParams->ExifInfo.DateTimeOriginal));
        NvOsStrncpy(pExifParams->ExifInfo.DateTimeDigitized,
                    pExifParams->ExifInfo.DateTime,
                    sizeof(pExifParams->ExifInfo.DateTimeDigitized));
    }
    // 3 for DSC, spec'd in the EXIF standard.  3 is the only valid number
    // as of EXIF 2.2, so we'll just hardcode it for now
    pExifParams->ExifInfo.filesource = 3;
    NV_LOGD(HAL3_JPEG_PROCESS_TAG, "%s: --", __FUNCTION__);
    return e;
}

NvError JpegProcessor::flush()
{
    NV_TRACE_CALL_D(HAL3_JPEG_PROCESS_TAG);
    mBypass = true;
    mWorkQMutex.lock();
    NvOsSemaphoreSignal(mJpegWorkSema);

    if (mWorking || mJpegWorkQ.size())
    {
        mDoneCond.wait(mWorkQMutex);
    }
    mWorkQMutex.unlock();

    mBypass = false;
    return NvSuccess;
}

} //namespace android
