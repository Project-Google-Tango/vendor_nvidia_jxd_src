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

/* AudioEncoderCapabilities implementation */

#include "linux_mediarecorder.h"

#define NO_AMR_BITRATES 17
#define NO_AAC_BITRATES 3

#define MAX_CHANNELS 2
#define MIN_BITS_SAMPLE 16
#define MAX_BITS_SAMPLE 32
#define AMR_SAMPLINGRATES_SUPPORTED 1
#define AMR_MIN_SAMPLINGRATE 8000
#define AMR_MAX_SAMPLINGRATE 8000
#define AAC_SAMPLINGRATES_SUPPORTED 1
#define AAC_SAMPLINGRATE 44100
#define AMR_MIN_BITRATE 4750
#define AMR_MAX_BITRATE 23850
#define AAC_MIN_BITRATE 64000
#define AAC_MAX_BITRATE 352000

#define PCM_MIN_SAMPLINGRATE 8000
#define PCM_MAX_SAMPLINGRATE 44100
#define PCM_SAMPLINGRATES_SUPPORTED 2
#define PCM_MIN_BITRATE 64000
#define PCM_MAX_BITRATE 320000
#define NO_PCM_BITRATES 3



XAuint32 BitRates_AMR[NO_AMR_BITRATES] =
{
    4750, 5150, 5900, 6700, 7400, 7950, 10200, 12200, 6600, 8850, 12650,
    14250, 15850, 18250, 19850, 23050, 23850
};

XAuint32 BitRates_AAC[NO_AAC_BITRATES] = { 64000, 128000, 320000};
XAuint32 BitRates_PCM[NO_PCM_BITRATES] = { 64000, 128000, 320000};


XAmilliHertz Amrsamplerates[] = {8000};
XAmilliHertz Aacsamplerates[] = {44100};
XAmilliHertz Pcmsamplerates[] = {8000, 44100};


// AMR CBR
static const XAAudioCodecDescriptor AudioEoncoderCodecDescriptor_amrcbr =
{
    MAX_CHANNELS, //maxChannels
    MIN_BITS_SAMPLE, //minBitsPerSample
    MAX_BITS_SAMPLE, //maxBitsPerSample
    AMR_MIN_SAMPLINGRATE,//minSampleRate
    AMR_MAX_SAMPLINGRATE,//maxSampleRate
    0, //isFreqRangeContinuous
    Amrsamplerates, //pSampleRatesSupported
    AMR_SAMPLINGRATES_SUPPORTED,//numSampleRatesSupported
    AMR_MIN_BITRATE,//minBitRate
    AMR_MAX_BITRATE,//maxBitRate
    0,//isBitrateRangeContinuous
    BitRates_AMR, //pBitratesSupported
    NO_AMR_BITRATES,
    XA_AUDIOPROFILE_AMR, //profileSetting
    0 //  modeSetting     XA_AUDIOSTREAMFORMAT_RAW //streamFormat
};

// AAC VBR
static const XAAudioCodecDescriptor AudioEoncoderCodecDescriptor_aacvbr =
{
    MAX_CHANNELS, //maxChannels
    MIN_BITS_SAMPLE, //minBitsPerSample
    MAX_BITS_SAMPLE, //maxBitsPerSample
    AAC_SAMPLINGRATE,//minSampleRate
    AAC_SAMPLINGRATE,//maxSampleRate
    0, //isFreqRangeContinuous
    Aacsamplerates, //pSampleRatesSupported
    AAC_SAMPLINGRATES_SUPPORTED,//numSampleRatesSupported
    AAC_MIN_BITRATE,//minBitRate
    AAC_MAX_BITRATE,//maxBitRate
    0,//isBitrateRangeContinuous
    BitRates_AAC, //pBitratesSupported
    NO_AAC_BITRATES,//Numberof bitrates supported
    XA_AUDIOPROFILE_AAC_AAC, //profileSetting
    XA_AUDIOMODE_AAC_LC //  modeSetting
};

static const XAAudioCodecDescriptor AudioEoncoderCodecDescriptor_pcm =
{
    MAX_CHANNELS, //maxChannels
    MIN_BITS_SAMPLE, //minBitsPerSample
    MAX_BITS_SAMPLE, //maxBitsPerSample
    PCM_MIN_SAMPLINGRATE,//minSampleRate
    PCM_MAX_SAMPLINGRATE,//maxSampleRate
    0, //isFreqRangeContinuous
    Pcmsamplerates, //pSampleRatesSupported
    PCM_SAMPLINGRATES_SUPPORTED,//numSampleRatesSupported
    PCM_MIN_BITRATE,//minBitRate
    PCM_MAX_BITRATE,//maxBitRate
    0,//isBitrateRangeContinuous
    BitRates_PCM, //pBitratesSupported
    NO_PCM_BITRATES,//Numberof bitrates supported
    XA_AUDIOPROFILE_PCM, //profileSetting
    0 //  modeSetting
};

static const AudioCodecDescriptor AudioEncoderDescriptors[] =
{
    {XA_AUDIOCODEC_AMR, &AudioEoncoderCodecDescriptor_amrcbr},
    {XA_AUDIOCODEC_AAC, &AudioEoncoderCodecDescriptor_aacvbr},
    {XA_AUDIOCODEC_PCM, &AudioEoncoderCodecDescriptor_pcm},
    {XA_AUDIOCODEC_PCM, NULL}
};

static const XAuint32 AudioCodecIds[] = {
    XA_AUDIOCODEC_AAC,
    XA_AUDIOCODEC_AMR,
    XA_AUDIOCODEC_PCM
};

static const XAuint32 *AudioEncoderIds = AudioCodecIds;

static const XAuint32 kMaxAudioEncoders =
    sizeof(AudioCodecIds) / sizeof(AudioCodecIds[0]);

static XAresult
GetAudioCodecCapabilities(
    XAuint32 codecId,
    XAuint32 *pIndex,
    XAAudioCodecDescriptor *pDescriptor,
    const AudioCodecDescriptor *codecDescriptors)
{
    const AudioCodecDescriptor *cd = codecDescriptors;
    XAuint32 index;
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pIndex && cd)

    if (NULL == pDescriptor)
    {
        for (index = 0; NULL != cd->mDescriptor; ++cd)
        {
            if (cd->mCodecID == codecId)
            {
                ++index;
            }
        }
        *pIndex = index;
        XA_LEAVE_INTERFACE
    }
    index = *pIndex;
    XA_LOGD("\n\nInPutIndex = %d\n", index);

    for ( ; NULL != cd->mDescriptor; ++cd)
    {
        if (cd->mCodecID == codecId)
        {
            if (0 == index)
            {
                *pDescriptor = *cd->mDescriptor;
                XA_LOGD("\n\nInPutIndex_0 = %d\n", index);
                XA_LEAVE_INTERFACE
            }
            --index;
        }
    }

    result = XA_RESULT_PARAMETER_INVALID;

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IAudioEncoderCapabilities_GetAudioEncoders
//***************************************************************************
static XAresult
IAudioEncoderCapabilities_GetAudioEncoders(
    XAAudioEncoderCapabilitiesItf self,
    XAuint32 *pNumEncoders,
    XAuint32 *pEncoderIds)
{
    XA_ENTER_INTERFACE

    AL_CHK_ARG(self && pNumEncoders)

    if (NULL == pEncoderIds)
    {
        *pNumEncoders = kMaxAudioEncoders;
    }
    else
    {
        if (kMaxAudioEncoders <= *pNumEncoders)
            *pNumEncoders = kMaxAudioEncoders;
        memcpy(
            pEncoderIds,
            AudioEncoderIds,
            *pNumEncoders * sizeof(XAuint32));
    }

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IAudioEncoderCapabilities_GetAudioEncoderCapabilities
//***************************************************************************
static XAresult
IAudioEncoderCapabilities_GetAudioEncoderCapabilities(
    XAAudioEncoderCapabilitiesItf self,
    XAuint32 encoderId,
    XAuint32 *pIndex,
    XAAudioCodecDescriptor *pDescriptor)
{
    XA_ENTER_INTERFACE

    result = GetAudioCodecCapabilities(
        encoderId, pIndex, pDescriptor, AudioEncoderDescriptors);

    XA_LEAVE_INTERFACE
}


//***************************************************************************
// IAudioEncoderCapabilities_Itf
//***************************************************************************
static const
struct XAAudioEncoderCapabilitiesItf_ IAudioEncoderCapabilities_Itf =
{
    IAudioEncoderCapabilities_GetAudioEncoders,
    IAudioEncoderCapabilities_GetAudioEncoderCapabilities
};

//***************************************************************************
// IAudioEncoderCapabilities_Itf
//***************************************************************************
void IAudioEncoderCapabilities_init(void *self)
{
    IAudioEncoderCapabilities *pAudioEncCaps = NULL;

    XA_ENTER_INTERFACE_VOID

    pAudioEncCaps = (IAudioEncoderCapabilities *)self;
    if (pAudioEncCaps)
        pAudioEncCaps->mItf = &IAudioEncoderCapabilities_Itf;

    XA_LEAVE_INTERFACE_VOID
}
