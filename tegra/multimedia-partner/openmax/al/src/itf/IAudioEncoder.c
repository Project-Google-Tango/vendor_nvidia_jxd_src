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

/* Audio Encoder implementation */

#include "linux_mediarecorder.h"

#define DEFAULT_CHANNELS_IN 2
#define DEFAULT_CHANNELS_OUT 2
#define DEFAULT_SAMPLING_RATE 44100
#define DEFAULT_BIT_RATE 128000
#define DEFAULT_BITS_PERSAMPLE 16


static const XAAudioEncoderSettings DefaultAudioSettings =
{
    XA_AUDIOCODEC_AAC,  //encoderId
    DEFAULT_CHANNELS_IN, //channelsIn
    DEFAULT_CHANNELS_OUT, //channelsOut
    DEFAULT_SAMPLING_RATE, //sampleRate
    DEFAULT_BIT_RATE, //bitRate
    DEFAULT_BITS_PERSAMPLE, //bitsPerSample
    0, //rateControl
    XA_AUDIOPROFILE_AAC_AAC, //profileSetting
    0, //levelSetting
    0, //channelMode
    XA_AUDIOSTREAMFORMAT_MP2ADTS, //streamFormat
    0, //encodeOptions
    0  //blockAlignment
};

//***************************************************************************
// IAudioEncoder_GetRecorder State
//***************************************************************************
static XAresult
IAudioEncoder_GetState(
    XAAudioEncoderItf self,
    XAuint32 *pState)
{
    IRecord *pIRecorder = NULL;
    IAudioEncoder *pAudioEncoder = (IAudioEncoder *)self;
    CMediaRecorder *pCMediaRecorder =
        (CMediaRecorder*)InterfaceToIObject(pAudioEncoder);

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pCMediaRecorder)
    pIRecorder = (IRecord *)(&(pCMediaRecorder->mRecord));
    AL_CHK_ARG(pIRecorder)
    result = pIRecorder->mItf->GetRecordState((XARecordItf)pIRecorder, pState);

    XA_LEAVE_INTERFACE
}
//***************************************************************************
// IAudioEncoder_GSet Audio Settings
//***************************************************************************
static XAresult
IAudioEncoder_SetAudioSettings(
    XAAudioEncoderItf self,
    XAAudioEncoderSettings *pSettings)
{
    XAuint32 pState;
    IAudioEncoder *pAudioEncoder = NULL;
    CMediaRecorder *pCMediaRecorder = NULL;

    XA_ENTER_INTERFACE

    pAudioEncoder = (IAudioEncoder *)self;
    AL_CHK_ARG(pAudioEncoder && pAudioEncoder->pData && pSettings)

    pCMediaRecorder = (CMediaRecorder *)InterfaceToIObject(pAudioEncoder);
    AL_CHK_ARG(pCMediaRecorder)
    result = IAudioEncoder_GetState(self, &pState);
    if (result != XA_RESULT_SUCCESS || pState != XA_RECORDSTATE_STOPPED)
        return XA_RESULT_PRECONDITIONS_VIOLATED;

    interface_lock_exclusive(pAudioEncoder);
    result = ALBackendMediaRecorderSetAudioSettings(
        pCMediaRecorder,
        pSettings);
    if (result == XA_RESULT_SUCCESS)
    {
        memcpy(
            (XAAudioEncoderSettings *)pAudioEncoder->pData,
            pSettings,
            sizeof(XAAudioEncoderSettings));
    }
    interface_unlock_exclusive(pAudioEncoder);

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IAudioEncoder_GetAudioEncoderSettings
//***************************************************************************
static XAresult
IAudioEncoder_GetAudioSettings(
    XAAudioEncoderItf self,
    XAAudioEncoderSettings *pSettings)
{

    IAudioEncoder *pAudioEncoder = NULL;

    XA_ENTER_INTERFACE

    pAudioEncoder = (IAudioEncoder *)self;
    AL_CHK_ARG(pAudioEncoder && pAudioEncoder->pData && pSettings)

    interface_lock_shared(pAudioEncoder);
    memcpy(
        pSettings,
        (XAAudioEncoderSettings *)pAudioEncoder->pData,
        sizeof(XAAudioEncoderSettings));
    interface_unlock_shared(pAudioEncoder);

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IAudioEncoder_Itf
//***************************************************************************
static const struct XAAudioEncoderItf_ IAudioEncoder_Itf =
{
    IAudioEncoder_SetAudioSettings,
    IAudioEncoder_GetAudioSettings
};

//***************************************************************************
// IAudioEncoder_Itf
//***************************************************************************
void IAudioEncoder_init(void *self)
{
    XAAudioEncoderSettings *pAudioEncoderSettings = NULL;
    IAudioEncoder *pAudioEnc = NULL;

    XA_ENTER_INTERFACE_VOID

    pAudioEnc = (IAudioEncoder *)self;
    if (pAudioEnc)
    {
        pAudioEnc->mItf = &IAudioEncoder_Itf;
        pAudioEncoderSettings =
            (XAAudioEncoderSettings *)malloc(sizeof(XAAudioEncoderSettings));
        if (pAudioEncoderSettings)
        {
            memcpy(
                pAudioEncoderSettings,
                &DefaultAudioSettings,
                sizeof(XAAudioEncoderSettings));
        }
        // Store the Image Encoder data
        pAudioEnc->pData = (void *)pAudioEncoderSettings;
    }

    XA_LEAVE_INTERFACE_VOID
}

void IAudioEncoder_deinit(void *self)
{
    IAudioEncoder *pAudioEnc = NULL;

    XA_ENTER_INTERFACE_VOID

    pAudioEnc = (IAudioEncoder *)self;

    if (pAudioEnc && pAudioEnc->pData)
    {
        free(pAudioEnc->pData);
        pAudioEnc->pData = NULL;
    }

    XA_LEAVE_INTERFACE_VOID
}

