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
/* AudioDecoderCapabilities implementation */

#include "linux_common.h"

#define XA_BITRATE_8Kbps                                 ((XAuint32) 8000)
#define XA_BITRATE_32Kbps                               ((XAuint32) 32000)
#define XA_BITRATE_64Kbps                               ((XAuint32) 64000)
#define XA_BITRATE_16Kbps                               ((XAuint32) 16000)
#define XA_BITRATE_24Kbps                               ((XAuint32) 24000)
#define XA_BITRATE_40Kbps                               ((XAuint32) 40000)
#define XA_BITRATE_48Kbps                               ((XAuint32) 48000)
#define XA_BITRATE_56Kbps                               ((XAuint32) 56000)
#define XA_BITRATE_80Kbps                               ((XAuint32) 80000)
#define XA_BITRATE_96Kbps                               ((XAuint32) 96000)
#define XA_BITRATE_112Kbps                             ((XAuint32) 112000)
#define XA_BITRATE_128Kbps                             ((XAuint32) 128000)
#define XA_BITRATE_144Kbps                             ((XAuint32) 144000)
#define XA_BITRATE_160Kbps                             ((XAuint32) 160000)
#define XA_BITRATE_192Kbps                             ((XAuint32) 192000)
#define XA_BITRATE_224Kbps                             ((XAuint32) 224000)
#define XA_BITRATE_256Kbps                             ((XAuint32) 256000)
#define XA_BITRATE_288Kbps                             ((XAuint32) 288000)
#define XA_BITRATE_320Kbps                             ((XAuint32) 320000)
#define XA_BITRATE_384Kbps                             ((XAuint32) 384000)
#define XA_BITRATE_3072Kbps                           ((XAuint32) 3072000)
#define XA_BITRATE_10752Kbps                         ((XAuint32) 10752000)
// amr
#define XA_BITRATE_4_75Kbps                           ((XAuint32) 4750)
#define XA_BITRATE_5_15Kbps                           ((XAuint32) 5150)
#define XA_BITRATE_5_90Kbps                           ((XAuint32) 5900)
#define XA_BITRATE_6_70Kbps                           ((XAuint32) 6700)
#define XA_BITRATE_7_40Kbps                           ((XAuint32) 7400)
#define XA_BITRATE_7_95Kbps                           ((XAuint32) 7950)
#define XA_BITRATE_10_2Kbps                           ((XAuint32) 10200)
#define XA_BITRATE_12_2Kbps                           ((XAuint32) 12200)
//amrwb
#define XA_BITRATE_6_6Kbps                             ((XAuint32) 6600)
#define XA_BITRATE_8_85Kbps                           ((XAuint32) 8850)
#define XA_BITRATE_12_65Kbps                         ((XAuint32) 12650)
#define XA_BITRATE_14_25Kbps                         ((XAuint32) 14250)
#define XA_BITRATE_15_85Kbps                         ((XAuint32) 15850)
#define XA_BITRATE_18_25Kbps                         ((XAuint32) 18250)
#define XA_BITRATE_19_85Kbps                         ((XAuint32) 19850)
#define XA_BITRATE_23_05Kbps                         ((XAuint32) 23050)
#define XA_BITRATE_23_85Kbps                         ((XAuint32) 23850)

// AudioCodecIds list
static const XAuint32 AudioCodecIds[] =
{
    XA_AUDIOCODEC_PCM,
    XA_AUDIOCODEC_MP3,
    XA_AUDIOCODEC_AMR,
    XA_AUDIOCODEC_AMRWB,
    XA_AUDIOCODEC_AAC,
    XA_AUDIOCODEC_WMA
};

static const XAuint32 *AudioDecoderIds = AudioCodecIds;

static const XAmilliHertz SamplingRates_A[] =
{
    XA_SAMPLINGRATE_8,
    XA_SAMPLINGRATE_11_025,
    XA_SAMPLINGRATE_12,
    XA_SAMPLINGRATE_16,
    XA_SAMPLINGRATE_22_05,
    XA_SAMPLINGRATE_24,
    XA_SAMPLINGRATE_32,
    XA_SAMPLINGRATE_44_1,
    XA_SAMPLINGRATE_48,
    XA_SAMPLINGRATE_96
};

static const XAmilliHertz SamplingRates_B[] =
{

    XA_SAMPLINGRATE_8,
    XA_SAMPLINGRATE_11_025,
    XA_SAMPLINGRATE_12,
    XA_SAMPLINGRATE_16,
    XA_SAMPLINGRATE_22_05,
    XA_SAMPLINGRATE_24,
    XA_SAMPLINGRATE_32,
    XA_SAMPLINGRATE_44_1,
    XA_SAMPLINGRATE_48
};

static const XAmilliHertz SamplingRates_AMR[] =
{
    XA_SAMPLINGRATE_8
};

static const XAmilliHertz SamplingRates_AMRWB[] =
{
    XA_SAMPLINGRATE_16
};

static const XAuint32 BitRates_AMR[] = {
    XA_BITRATE_4_75Kbps,
    XA_BITRATE_5_15Kbps,
    XA_BITRATE_5_90Kbps,
    XA_BITRATE_6_70Kbps,
    XA_BITRATE_7_40Kbps,
    XA_BITRATE_7_95Kbps,
    XA_BITRATE_10_2Kbps,
    XA_BITRATE_12_2Kbps
};

static const XAuint32 BitRates_AMRWB[] = {
    XA_BITRATE_6_6Kbps,
    XA_BITRATE_8_85Kbps,
    XA_BITRATE_12_65Kbps,
    XA_BITRATE_14_25Kbps,
    XA_BITRATE_15_85Kbps,
    XA_BITRATE_18_25Kbps,
    XA_BITRATE_19_85Kbps,
    XA_BITRATE_23_05Kbps,
    XA_BITRATE_23_85Kbps
};

static const XAuint32 BitRates_MPEG1_L3[] =
{
    XA_BITRATE_32Kbps, XA_BITRATE_40Kbps, XA_BITRATE_48Kbps, XA_BITRATE_56Kbps,
    XA_BITRATE_64Kbps, XA_BITRATE_80Kbps, XA_BITRATE_96Kbps,XA_BITRATE_112Kbps,
    XA_BITRATE_128Kbps,XA_BITRATE_160Kbps,XA_BITRATE_192Kbps,XA_BITRATE_224Kbps,
    XA_BITRATE_256Kbps,XA_BITRATE_320Kbps
};// Layer 3

static const XAuint32 BitRates_MPEG25_L3[] = {
    XA_BITRATE_8Kbps, XA_BITRATE_16Kbps, XA_BITRATE_24Kbps, XA_BITRATE_32Kbps,
    XA_BITRATE_40Kbps, XA_BITRATE_48Kbps, XA_BITRATE_56Kbps, XA_BITRATE_64Kbps,
    XA_BITRATE_80Kbps, XA_BITRATE_96Kbps,XA_BITRATE_112Kbps,XA_BITRATE_128Kbps,
    XA_BITRATE_144Kbps,XA_BITRATE_160Kbps
};// Layer 3

static const XAmilliHertz SamplingRates_MPEG1[] =
{
    XA_SAMPLINGRATE_32,
    XA_SAMPLINGRATE_44_1,
    XA_SAMPLINGRATE_48
};

static const XAmilliHertz SamplingRates_MPEG2[] =
{
    XA_SAMPLINGRATE_16,
    XA_SAMPLINGRATE_22_05,
    XA_SAMPLINGRATE_24
};

static const XAmilliHertz SamplingRates_MPEG25[] =
{
    XA_SAMPLINGRATE_8,
    XA_SAMPLINGRATE_11_025,
    XA_SAMPLINGRATE_12
};

static const XAAudioCodecDescriptor CodecDescriptor_MPEG1_L3 = {
    2,                   // maxChannels
    16,                   // minBitsPerSample
    16,                  // maxBitsPerSample
    XA_SAMPLINGRATE_32,   // minSampleRate
    XA_SAMPLINGRATE_48,  // maxSampleRate
    XA_BOOLEAN_FALSE,    // isFreqRangeContinuous
    (XAmilliHertz *) SamplingRates_MPEG1,      // pSampleRatesSupported;
    sizeof(SamplingRates_MPEG1) / sizeof(SamplingRates_MPEG1[0]),    // numSampleRatesSupported
    XA_BITRATE_32Kbps,                   // minBitRate
    XA_BITRATE_320Kbps,                  // maxBitRate
    XA_BOOLEAN_TRUE,     // isBitrateRangeContinuous
    (XAuint32 *)BitRates_MPEG1_L3,                // pBitratesSupported
    sizeof(BitRates_MPEG1_L3)/ sizeof(BitRates_MPEG1_L3),        // numBitratesSupported
    XA_AUDIOCHANMODE_MP3_DUAL  // modeSetting
};

static const XAAudioCodecDescriptor CodecDescriptor_MPEG2_L3 = {
    2,                   // maxChannels
    16,                   // minBitsPerSample
    16,                  // maxBitsPerSample
    XA_SAMPLINGRATE_16,   // minSampleRate
    XA_SAMPLINGRATE_24,  // maxSampleRate
    XA_BOOLEAN_FALSE,    // isFreqRangeContinuous
    (XAmilliHertz *) SamplingRates_MPEG2,      // pSampleRatesSupported;
    sizeof(SamplingRates_MPEG2) / sizeof(SamplingRates_MPEG2[0]),    // numSampleRatesSupported
    XA_BITRATE_32Kbps,                   // minBitRate
    XA_BITRATE_320Kbps,                  // maxBitRate
    XA_BOOLEAN_TRUE,     // isBitrateRangeContinuous
    (XAuint32 *)BitRates_MPEG25_L3,                // pBitratesSupported
    sizeof(BitRates_MPEG25_L3)/ sizeof(BitRates_MPEG25_L3),          // numBitratesSupported
    XA_AUDIOPROFILE_MPEG2_L3, // profileSetting
    XA_AUDIOCHANMODE_MP3_DUAL  // modeSetting
};

static const XAAudioCodecDescriptor CodecDescriptor_MPEG25_L3 = {
    2,                   // maxChannels
    16,                   // minBitsPerSample
    16,                  // maxBitsPerSample
    XA_SAMPLINGRATE_8,   // minSampleRate
    XA_SAMPLINGRATE_12,  // maxSampleRate
    XA_BOOLEAN_FALSE,    // isFreqRangeContinuous
    (XAmilliHertz *) SamplingRates_MPEG25,      // pSampleRatesSupported;
    sizeof(SamplingRates_MPEG25) / sizeof(SamplingRates_MPEG25[0]),    // numSampleRatesSupported
    XA_BITRATE_32Kbps,                   // minBitRate
    XA_BITRATE_320Kbps,                  // maxBitRate
    XA_BOOLEAN_TRUE,     // isBitrateRangeContinuous
    (XAuint32 *)BitRates_MPEG25_L3,                // pBitratesSupported
    sizeof(BitRates_MPEG25_L3)/ sizeof(BitRates_MPEG25_L3),         // numBitratesSupported
    XA_AUDIOPROFILE_MPEG25_L3, // profileSetting
    XA_AUDIOCHANMODE_MP3_DUAL  // modeSetting
};

static const XAAudioCodecDescriptor CodecDescriptor_AAC_LC = {
    6,                   // maxChannels
    16,                   // minBitsPerSample
    16,                  // maxBitsPerSample
    XA_SAMPLINGRATE_8,   // minSampleRate
    XA_SAMPLINGRATE_48,  // maxSampleRate
    XA_BOOLEAN_FALSE,    // isFreqRangeContinuous
    (XAmilliHertz *) SamplingRates_B,    // pSampleRatesSupported;
    sizeof(SamplingRates_B) / sizeof(SamplingRates_B[0]),    // numSampleRatesSupported
    XA_BITRATE_8Kbps,                   // minBitRate
    XA_BITRATE_288Kbps,                  // maxBitRate per channel
    XA_BOOLEAN_TRUE,     // isBitrateRangeContinuous
    NULL,                // pBitratesSupported
    0,                   // numBitratesSupported
    XA_AUDIOPROFILE_AAC_AAC, // profileSetting
    XA_AUDIOMODE_AAC_LC,   // modeSetting
};

static const XAAudioCodecDescriptor CodecDescriptor_AAC_HE = {
    2,                   // maxChannels
    16,                   // minBitsPerSample
    16,                  // maxBitsPerSample
    XA_SAMPLINGRATE_8,   // minSampleRate
    XA_SAMPLINGRATE_96,  // maxSampleRate
    XA_BOOLEAN_FALSE,    // isFreqRangeContinuous
    (XAmilliHertz *) SamplingRates_A,     // pSampleRatesSupported;
    sizeof(SamplingRates_A) / sizeof(SamplingRates_A[0]),    // numSampleRatesSupported
    XA_BITRATE_8Kbps,                   // minBitRate
    XA_BITRATE_320Kbps,                  // maxBitRate
    XA_BOOLEAN_TRUE,     // isBitrateRangeContinuous
    NULL,                // pBitratesSupported
    0,                   // numBitratesSupported
    XA_AUDIOPROFILE_AAC_AAC, // profileSetting
    XA_AUDIOMODE_AAC_HE,   // modeSetting
};

static const XAAudioCodecDescriptor CodecDescriptor_AAC_HE_PS = {
    2,                   // maxChannels
    16,                   // minBitsPerSample
    16,                  // maxBitsPerSample
    XA_SAMPLINGRATE_8,   // minSampleRate
    XA_SAMPLINGRATE_96,  // maxSampleRate
    XA_BOOLEAN_FALSE,    // isFreqRangeContinuous
    (XAmilliHertz *) SamplingRates_A,    // pSampleRatesSupported;
    sizeof(SamplingRates_A) / sizeof(SamplingRates_A[0]),    // numSampleRatesSupported
    XA_BITRATE_8Kbps,                   // minBitRate
    XA_BITRATE_320Kbps,                  // maxBitRate
    XA_BOOLEAN_TRUE,     // isBitrateRangeContinuous
    NULL,                // pBitratesSupported
    0,                   // numBitratesSupported
    XA_AUDIOPROFILE_AAC_AAC, // profileSetting
    XA_AUDIOMODE_AAC_HE_PS,      // modeSetting
};

static const XAAudioCodecDescriptor CodecDescriptor_WMA = {
    2,                   // maxChannels
    16,                   // minBitsPerSample
    16,                  // maxBitsPerSample
    XA_SAMPLINGRATE_8,   // minSampleRate
    XA_SAMPLINGRATE_48,  // maxSampleRate
    XA_BOOLEAN_FALSE,    // isFreqRangeContinuous
    (XAmilliHertz *) SamplingRates_B,    // pSampleRatesSupported;
    sizeof(SamplingRates_B) / sizeof(SamplingRates_B[0]),    // numSampleRatesSupported
    XA_BITRATE_8Kbps,                   // minBitRate
    XA_BITRATE_384Kbps,                  // maxBitRate
    XA_BOOLEAN_TRUE,     // isBitrateRangeContinuous
    NULL,                // pBitratesSupported
    0,                   // numBitratesSupported
    XA_AUDIOPROFILE_WMA10, // profileSetting
    XA_AUDIOMODE_WMAPRO_LEVELM3  // modeSetting
};

static const XAAudioCodecDescriptor CodecDescriptor_PCM = {
    7,                   // maxChannels
    8,                   // minBitsPerSample
    16,                  // maxBitsPerSample
    XA_SAMPLINGRATE_8,   // minSampleRate
    XA_SAMPLINGRATE_96,  // maxSampleRate
    XA_BOOLEAN_FALSE,    // isFreqRangeContinuous
    (XAmilliHertz *) SamplingRates_A,    // pSampleRatesSupported;
    sizeof(SamplingRates_A) / sizeof(SamplingRates_A[0]),    // numSampleRatesSupported
    XA_BITRATE_64Kbps,                   // minBitRate
    XA_BITRATE_10752Kbps,                  // maxBitRate
    XA_BOOLEAN_TRUE,     // isBitrateRangeContinuous
    NULL,                // pBitratesSupported
    0,                   // numBitratesSupported
    XA_AUDIOPROFILE_PCM, // profileSetting
    0                    // modeSetting
};

// AMR codec has eight basic bit rates, 12.2, 10.2, 7.95, 7.40, 6.70, 5.90, 5.15 and 4.75 Kbit/s
static const XAAudioCodecDescriptor CodecDescriptor_AMR = {
    1,                   // maxChannels
    16,                   // minBitsPerSample
    16,                  // maxBitsPerSample
    XA_SAMPLINGRATE_8,   // minSampleRate
    XA_SAMPLINGRATE_8,  // maxSampleRate
    XA_BOOLEAN_FALSE,    // isFreqRangeContinuous
    (XAmilliHertz *) SamplingRates_AMR,     // pSampleRatesSupported;
    sizeof(SamplingRates_AMR) / sizeof(SamplingRates_AMR[0]),    // numSampleRatesSupported
    XA_BITRATE_4_75Kbps,                   // minBitRate
    XA_BITRATE_12_2Kbps,                  // maxBitRate
    XA_BOOLEAN_TRUE,     // isBitrateRangeContinuous
    (XAuint32 *)BitRates_AMR,                // pBitratesSupported
    sizeof(BitRates_AMR)/ sizeof(BitRates_AMR),             // numBitratesSupported
    XA_AUDIOPROFILE_AMR, // profileSetting
    XA_AUDIOSTREAMFORMAT_ITU                    // modeSetting
};

static const XAAudioCodecDescriptor CodecDescriptor_AMRW = {
    1,                   // maxChannels
    16,                   // minBitsPerSample
    16,                  // maxBitsPerSample
    XA_SAMPLINGRATE_16,   // minSampleRate
    XA_SAMPLINGRATE_16,  // maxSampleRate
    XA_BOOLEAN_FALSE,    // isFreqRangeContinuous
    (XAmilliHertz *) SamplingRates_AMRWB,    // pSampleRatesSupported;
    sizeof(SamplingRates_AMRWB) / sizeof(SamplingRates_AMRWB[0]),    // numSampleRatesSupported
    XA_BITRATE_6_6Kbps,                   // minBitRate
    XA_BITRATE_23_85Kbps,                  // maxBitRate
    XA_BOOLEAN_TRUE,     // isBitrateRangeContinuous
    (XAuint32 *)BitRates_AMRWB,                // pBitratesSupported
    sizeof(BitRates_AMRWB)/ sizeof(BitRates_AMRWB),           // numBitratesSupported
    XA_AUDIOPROFILE_AMRWB, // profileSetting
    XA_AUDIOSTREAMFORMAT_ITU                    // modeSetting
};

static const AudioCodecDescriptor AudioDecoderDescriptors[] = {
    {XA_AUDIOCODEC_PCM, &CodecDescriptor_PCM},
    {XA_AUDIOCODEC_MP3, &CodecDescriptor_MPEG1_L3},
    {XA_AUDIOCODEC_MP3, &CodecDescriptor_MPEG2_L3},
    {XA_AUDIOCODEC_MP3, &CodecDescriptor_MPEG25_L3},
    {XA_AUDIOCODEC_AMR, &CodecDescriptor_AMR},
    {XA_AUDIOCODEC_AMRWB, &CodecDescriptor_AMRW},
    {XA_AUDIOCODEC_AAC, &CodecDescriptor_AAC_LC},
    {XA_AUDIOCODEC_AAC, &CodecDescriptor_AAC_HE},
    {XA_AUDIOCODEC_AAC, &CodecDescriptor_AAC_HE_PS},
    {XA_AUDIOCODEC_WMA, &CodecDescriptor_WMA},
    {XA_AUDIOCODEC_PCM, NULL} /*Fix Me*/
};

static const XAuint32 kMaxAudioDecoders = sizeof(AudioCodecIds) / sizeof(AudioCodecIds[0]);

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

    AL_CHK_ARG(pIndex)
    if (NULL == pDescriptor)
    {
        for (index = 0 ; NULL != cd->mDescriptor; ++cd)
        {
            if (cd->mCodecID == codecId)
            {
                ++index;
            }
        }
        *pIndex = index;
        result = XA_RESULT_SUCCESS;
        XA_LEAVE_INTERFACE
    }
    index = *pIndex;
    XA_LOGD("\n\nInPutIndex = %d\n", index);

    for ( ; NULL != cd->mDescriptor; ++cd) {
        if (cd->mCodecID == codecId) {
            if (0 == index)
            {
                *pDescriptor = *cd->mDescriptor;
                XA_LOGD("\n\nInPutIndex_0 = %d\n", index);
                result = XA_RESULT_SUCCESS;
                XA_LEAVE_INTERFACE
            }
            --index;
        }
    }

    result = XA_RESULT_PARAMETER_INVALID;

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IAudioDecoderCapabilities_GetAudioDecoders
//***************************************************************************
static XAresult
IAudioDecoderCapabilities_GetAudioDecoders(
    XAAudioDecoderCapabilitiesItf self,
    XAuint32 *pNumDecoders,
    XAuint32 *pDecoderIds)
{

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pNumDecoders);
    if (NULL == pDecoderIds)
    {
        *pNumDecoders = kMaxAudioDecoders;
    }
    else
    {
        if (kMaxAudioDecoders <= *pNumDecoders)
            *pNumDecoders = kMaxAudioDecoders;

        memcpy(pDecoderIds, AudioDecoderIds, *pNumDecoders * sizeof(XAuint32));
    }

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IAudioDecoderCapabilities_GetAudioDecoderCapabilities
//***************************************************************************
static XAresult
IAudioDecoderCapabilities_GetAudioDecoderCapabilities(
    XAAudioDecoderCapabilitiesItf self,
    XAuint32 decoderId,
    XAuint32 *pIndex,
    XAAudioCodecDescriptor *pDescriptor)
{
    XA_ENTER_INTERFACE

    result = GetAudioCodecCapabilities(
        decoderId, pIndex, pDescriptor, AudioDecoderDescriptors);

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IAudioDecoderCapabilities_Itf
//***************************************************************************
static const
struct XAAudioDecoderCapabilitiesItf_ IAudioDecoderCapabilities_Itf =
{
    IAudioDecoderCapabilities_GetAudioDecoders,
    IAudioDecoderCapabilities_GetAudioDecoderCapabilities
};

//***************************************************************************
// IAudioDecoderCapabilities_init
//***************************************************************************
void IAudioDecoderCapabilities_init(void *self)
{
    IAudioDecoderCapabilities *pCaps = (IAudioDecoderCapabilities *)self;

    XA_ENTER_INTERFACE_VOID

    if(pCaps)
        pCaps->mItf = &IAudioDecoderCapabilities_Itf;

    XA_LEAVE_INTERFACE_VOID
}

