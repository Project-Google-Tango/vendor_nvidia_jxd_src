/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* VideoEncoder implementation */


#include "linux_mediarecorder.h"

#define DEFAULT_ENCODE_WIDTH 640
#define DEFAULT_ENCODE_HEIGHT 480
#define DEFAULT_FRAME_RATE (30 << 16)
#define DEFAULT_BITRATE 4000000
#define DEFAULT_KEYFRAME_INTERVAL 30

static const XAVideoSettings DefaultVideoSettings =
{
    XA_VIDEOCODEC_AVC,//encoderId
    DEFAULT_ENCODE_WIDTH,//width;
    DEFAULT_ENCODE_HEIGHT,// height;
    DEFAULT_FRAME_RATE,// frameRate;
    DEFAULT_BITRATE,//bitRate;
    0,//rateControl;
    0,//profileSetting;
    0,//levelSetting;
    DEFAULT_KEYFRAME_INTERVAL //keyFrameInterval;
};

//***************************************************************************
// IVideoEncoder_GetRecorder State
//***************************************************************************
static XAresult
IVideoEncoder_GetState(
    XAVideoEncoderItf self,
    XAuint32 *pState)
{
    IVideoEncoder *pVideoEncoder = (IVideoEncoder *)self;
    IRecord *pIRecorder = NULL;
    CMediaRecorder *pCMediaRecorder =
        (CMediaRecorder*)InterfaceToIObject(pVideoEncoder);

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCMediaRecorder)
    pIRecorder = (IRecord *)(&(pCMediaRecorder->mRecord));
    AL_CHK_ARG(pIRecorder)
    result = pIRecorder->mItf->GetRecordState((XARecordItf)pIRecorder, pState);

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IVideoEncoder_SetVideo Setings
//***************************************************************************
static XAresult
IVideoEncoder_SetVideoSettings(
    XAVideoEncoderItf self,
    XAVideoSettings *pSettings)
{
    XAuint32 pState;
    IVideoEncoder *pVideoEncoder = (IVideoEncoder *)self;
    CMediaRecorder *pCMediaRecorder = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pVideoEncoder && pVideoEncoder->pData && pSettings)
    pCMediaRecorder = (CMediaRecorder*)InterfaceToIObject(pVideoEncoder);
    AL_CHK_ARG(pCMediaRecorder)
    result = IVideoEncoder_GetState(self, &pState);
    if (result != XA_RESULT_SUCCESS || pState != XA_RECORDSTATE_STOPPED)
        return XA_RESULT_PRECONDITIONS_VIOLATED;

    interface_lock_exclusive(pVideoEncoder);
    result = ALBackendMediaRecorderSetVideoSettings(
        pCMediaRecorder,
        pSettings);
    if (result == XA_RESULT_SUCCESS)
    {
        memcpy(
            (XAVideoSettings *)pVideoEncoder->pData,
            pSettings,
            sizeof(XAVideoSettings));
    }
    interface_unlock_exclusive(pVideoEncoder);

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IVideoEncoder_GetVideo Settings
//***************************************************************************
static XAresult
IVideoEncoder_GetVideoSettings(
    XAVideoEncoderItf self,
    XAVideoSettings *pSettings)
{
    IVideoEncoder *pVideoEncoder = (IVideoEncoder *)self;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pVideoEncoder && pVideoEncoder->pData && pSettings)
    interface_lock_shared(pVideoEncoder);
    memcpy(
        pSettings,
        (XAVideoSettings *)pVideoEncoder->pData,
        sizeof(XAVideoSettings));
    interface_unlock_shared(pVideoEncoder);

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IVideoEncoder_Itf
//***************************************************************************
static const struct XAVideoEncoderItf_ IVideoEncoder_Itf =
{
    IVideoEncoder_SetVideoSettings,
    IVideoEncoder_GetVideoSettings
};

//***************************************************************************
// IVideoEncoder_Itf
//***************************************************************************
void IVideoEncoder_init(void *self)
{
    XAVideoSettings *pVideoEncoderSettings = NULL;
    IVideoEncoder *pVideoEnc = (IVideoEncoder *)self;

    XA_ENTER_INTERFACE_VOID
    if (pVideoEnc)
    {
        pVideoEnc->mItf = &IVideoEncoder_Itf;
        pVideoEncoderSettings =
            (XAVideoSettings *)malloc(sizeof(XAVideoSettings));
        if (pVideoEncoderSettings)
        {
            memcpy(
                pVideoEncoderSettings,
                &DefaultVideoSettings,
                sizeof(XAVideoSettings));
            // Store the Image Encoder data
        }
        pVideoEnc->pData = (void *)pVideoEncoderSettings;
    }

    XA_LEAVE_INTERFACE_VOID
}

void IVideoEncoder_deinit(void *self)
{
    IVideoEncoder *pVideoEnc = (IVideoEncoder *)self;

    XA_ENTER_INTERFACE_VOID
    if (pVideoEnc && pVideoEnc->pData)
    {
        free(pVideoEnc->pData);
        pVideoEnc->pData = NULL;
    }

    XA_LEAVE_INTERFACE_VOID
}
