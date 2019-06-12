/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _NV_CAMERA_JPEGPROCESSOR_H_
#define _NV_CAMERA_JPEGPROCESSOR_H_

#include <utils/RefBase.h>
#include <utils/Timers.h>
#include <utils/Condition.h>
#include <utils/Mutex.h>
#include <utils/List.h>
#include <utils/Thread.h>

#include "nvabstracts.h"
#include "nvcamerahal3common.h"
#include "nvcamerahal3streamport.h"
#include "jpegencoder.h"

namespace android {

class StreamPort;
struct BufferInfo;
struct JpegWorkItem
{
    BufferInfo *pBufferInfo;
    NvMMBuffer *thumbnailBuffer;
};

typedef List<JpegWorkItem*> JpegWorkQueue;

class JpegProcessor : public Thread
{
public:
    JpegProcessor(wp<StreamPort> client);
    ~JpegProcessor();

    NvError enqueueJpegWork(BufferInfo *pOutBufferInfo, NvMMBuffer *thumbnailBuffer);
    NvError flush();

    void constructThumbnailParams(NvJpegEncParameters *pParams,
        NvCameraHal3_JpegSettings *pJpegSettings);

private:
    NvError processJpegFrame(BufferInfo *pBufferInfo, NvMMBuffer *thumbnailBuffer);
    virtual bool threadLoop();

    void constructJpegParams(NvJpegEncParameters *,
            NvCameraHal3_JpegSettings *pJpegSettings);
    NvError constructExifInfo(NvJpegEncParameters *pParams,
        NvCameraHal3_JpegSettings *pJpegSettings);
    void constructGPSParams(NvJpegEncParameters *pParams,
        NvCameraHal3_JpegSettings *pJpegSettings);
    void parseDegrees(double coord, unsigned int *ddmmss, NvBool *direction);
    void parseAltitude(double input, float *alt, NvBool *ref);
    void parseTime(time_t input, NvCameraUserTime* time);
    void SetGpsLatitude(bool Enable, unsigned int lat, bool dir,
        NvJpegEncParameters *Params);
    void SetGpsLongitude(bool Enable, unsigned int lon, bool dir,
        NvJpegEncParameters *Params);
    void SetGpsAltitude(bool Enable, float altitude, bool ref,
        NvJpegEncParameters *Params);
    void SetGpsTimestamp(bool Enable, NvCameraUserTime time,
        NvJpegEncParameters *Params);
    void SetGpsProcMethod(bool Enable, const char *str,
        NvJpegEncParameters *Params);

    NvError doJpegEncoding(NvMMBuffer *pNvMMSrc,
        NvMMBuffer *pThumbnailBuffer, buffer_handle_t *pANBDst,
        NvCameraHal3_JpegSettings *pJpegSettings);

    NvError ConvertToJpeg(
        NvMMBuffer *pSrc,
        NvMMBuffer *pNvMMThumbnail,
        buffer_handle_t *pANBDst,
        JpegParams *jpegParams);

    JpegWorkQueue mJpegWorkQ;
    wp<StreamPort> mClient;
    NvOsSemaphoreHandle mJpegWorkSema;
    Mutex mWorkQMutex;

    JpegEncoder *pJpegEncoder;
    NvU32 mJpegOpenFlag;
    bool mWorking;
    Condition mDoneCond;
    bool mBypass;

    NvRmDeviceHandle mRmDevice;
};

} //namespace android

#endif
