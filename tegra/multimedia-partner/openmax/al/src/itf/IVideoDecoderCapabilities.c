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

/* VideoDecoderCapabilities implementation */

#include "linux_common.h"

static const XAuint32 VideoCodecIds[] =
{
    XA_VIDEOCODEC_MPEG2,
    XA_VIDEOCODEC_H263,
    XA_VIDEOCODEC_MPEG4,
    XA_VIDEOCODEC_AVC,
    XA_VIDEOCODEC_VC1
};

#define XA_WIDTH_640                               ((XAuint32) 640)
#define XA_WIDTH_720                               ((XAuint32) 720)
#define XA_WIDTH_1920                             ((XAuint32) 1920)
#define XA_WIDTH_2048                             ((XAuint32) 2048)
#define XA_HEIGHT_480                              ((XAuint32) 480)
#define XA_HEIGHT_576                              ((XAuint32) 576)
#define XA_HEIGHT_1024                            ((XAuint32) 1024)
#define XA_HEIGHT_1152                            ((XAuint32) 1152)
#define XA_BITRATE_4000Kbps                   ((XAuint32) 4000000)
#define XA_BITRATE_8000Kbps                   ((XAuint32) 8000000)
#define XA_BITRATE_10368Kbps                 ((XAuint32) 10368000)
#define XA_BITRATE_50000Kbps                 ((XAuint32) 50000000)
#define XA_BITRATE_80000Kbps                 ((XAuint32) 80000000)

#define XA_FRAMERATE_30_Q16                 ((XAuint32) 30<<16)
#define XA_FRAMERATE_60_Q16                 ((XAuint32) 60<<16)

static const XAuint32 MaxVideoDecoders =
    sizeof(VideoCodecIds) / sizeof(VideoCodecIds[0]);

static const XAVideoCodecDescriptor CodecDescriptor_AVC_HIGH =
{
    XA_VIDEOCODEC_AVC,        // codecId
    XA_WIDTH_2048,                                   // maxWidth
    XA_HEIGHT_1024,                                   // maxHeight
    XA_FRAMERATE_30_Q16,                                     // maxFrameRate
    XA_BITRATE_50000Kbps,                                // maxBitRate
    XA_RATECONTROLMODE_CONSTANTBITRATE,           // rateControlSupported
    XA_VIDEOPROFILE_AVC_HIGH, // profileSetting
    XA_VIDEOLEVEL_AVC_41 // levelSetting
};

static const XAVideoCodecDescriptor CodecDescriptor_AVC_BASELINE =
{
    XA_VIDEOCODEC_AVC,        // codecId
    XA_WIDTH_2048,                                   // maxWidth
    XA_HEIGHT_1024,                                   // maxHeight
    XA_FRAMERATE_30_Q16,                                     // maxFrameRate
    XA_BITRATE_50000Kbps,                                // maxBitRate
    XA_RATECONTROLMODE_CONSTANTBITRATE,           // rateControlSupported
    XA_VIDEOPROFILE_AVC_MAIN, // profileSetting
    XA_VIDEOLEVEL_AVC_41 // levelSetting
};

static const XAVideoCodecDescriptor CodecDescriptor_AVC_MAIN =
{
    XA_VIDEOCODEC_AVC,        // codecId
    XA_WIDTH_2048,                                   // maxWidth
    XA_HEIGHT_1024,                                   // maxHeight
    XA_FRAMERATE_30_Q16,                                     // maxFrameRate
    XA_BITRATE_50000Kbps,                                // maxBitRate
    XA_RATECONTROLMODE_CONSTANTBITRATE,       // rateControlSupported
    XA_VIDEOPROFILE_AVC_BASELINE,      // profileSetting
    XA_VIDEOLEVEL_AVC_41       // levelSetting
};

static const XAVideoCodecDescriptor CodecDescriptor_MPEG4_SIMPLE =
{
    XA_VIDEOCODEC_MPEG4,      // codecId
    XA_WIDTH_720,                                     // maxWidth
    XA_HEIGHT_576,                                   // maxHeight
    XA_FRAMERATE_30_Q16,                                    // maxFrameRate
    XA_BITRATE_8000Kbps,                                // maxBitRate
    XA_RATECONTROLMODE_CONSTANTBITRATE,       // rateControlSupported
    XA_VIDEOPROFILE_MPEG4_SIMPLE,   // profileSetting
    XA_VIDEOLEVEL_MPEG4_5    // levelSetting
};

static const XAVideoCodecDescriptor CodecDescriptor_MPEG2_SIMPLE =
{
    XA_VIDEOCODEC_MPEG2,      // codecId
    XA_WIDTH_720,                                     // maxWidth
    XA_HEIGHT_576,                                   // maxHeight
    XA_FRAMERATE_30_Q16,                                    // maxFrameRate
    XA_BITRATE_10368Kbps,                                // maxBitRate
    XA_RATECONTROLMODE_CONSTANTBITRATE,         // rateControlSupported
    XA_VIDEOPROFILE_MPEG2_SIMPLE,   // profileSetting
    XA_VIDEOLEVEL_MPEG2_ML     // levelSetting
};

static const XAVideoCodecDescriptor CodecDescriptor_MPEG2_MAIN =
{
    XA_VIDEOCODEC_MPEG2,      // codecId
    XA_WIDTH_1920,                                     // maxWidth
    XA_HEIGHT_1152,                                   // maxHeight
    XA_FRAMERATE_60_Q16,                                    // maxFrameRate
    XA_BITRATE_80000Kbps,                                // maxBitRate
    XA_RATECONTROLMODE_CONSTANTBITRATE,        // rateControlSupported
    XA_VIDEOPROFILE_MPEG2_MAIN,   // profileSetting
    XA_VIDEOLEVEL_MPEG2_HL   // levelSetting
};

static const XAVideoCodecDescriptor CodecDescriptor_H263_BASELINE =
{
    XA_VIDEOCODEC_H263,      // codecId
    XA_WIDTH_640,                                     // maxWidth
    XA_HEIGHT_480,                                   // maxHeight
    XA_FRAMERATE_30_Q16,                                    // maxFrameRate
    XA_BITRATE_4000Kbps,                                // maxBitRate
    XA_RATECONTROLMODE_CONSTANTBITRATE,        // rateControlSupported
    XA_VIDEOPROFILE_H263_BASELINE,   // profileSetting
    XA_VIDEOLEVEL_H263_50                 // levelSetting
};

static const VideoCodecDescriptor VideoDecoderDescriptors[] =
{
    {XA_VIDEOCODEC_MPEG2, &CodecDescriptor_MPEG2_SIMPLE},
    {XA_VIDEOCODEC_MPEG2, &CodecDescriptor_MPEG2_MAIN},
    {XA_VIDEOCODEC_H263, &CodecDescriptor_H263_BASELINE},
    {XA_VIDEOCODEC_MPEG4, &CodecDescriptor_MPEG4_SIMPLE},
    {XA_VIDEOCODEC_AVC, &CodecDescriptor_AVC_BASELINE},
    {XA_VIDEOCODEC_AVC, &CodecDescriptor_AVC_MAIN},
    {XA_VIDEOCODEC_AVC, &CodecDescriptor_AVC_HIGH},
    {XA_VIDEOCODEC_VC1, NULL}, //TODO
};

static XAresult
GetVideoCodecCapabilities(
    XAuint32 codecId,
    XAuint32 *pIndex,
    XAVideoCodecDescriptor *pDescriptor,
    const VideoCodecDescriptor *codecDescriptors)
{
    const VideoCodecDescriptor *cd = codecDescriptors;
    XAuint32 index = 0;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pIndex);

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
    XA_LOGD("\n\nCodecId = %d - Index = %d\n", codecId, index);

    for ( ; NULL != cd->mDescriptor; ++cd)
    {
        if (cd->mCodecID == codecId)
        {
            if (0 == index)
            {
                *pDescriptor = *cd->mDescriptor;
                XA_LOGD(
                    "CodecId = %d - Index = %d - profile = %d - level = %d\n",
                    codecId,
                    index,
                    pDescriptor->profileSetting,
                    pDescriptor->levelSetting);
                XA_LEAVE_INTERFACE
            }
            --index;
        }
    }
    result = XA_RESULT_PARAMETER_INVALID;
    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IVideoDecoderCapabilities_GetVideoDecoders
//***************************************************************************
static XAresult
IVideoDecoderCapabilities_GetVideoDecoders(
    XAVideoDecoderCapabilitiesItf self,
    XAuint32 *pNumDecoders,
    XAuint32 *pDecoderIds)
{
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pNumDecoders);
    if (NULL == pDecoderIds)
    {
        *pNumDecoders = MaxVideoDecoders;
    }
    else
    {
        if (MaxVideoDecoders <= *pNumDecoders)
            *pNumDecoders = MaxVideoDecoders;
        memcpy(pDecoderIds, &VideoCodecIds, *pNumDecoders * sizeof(XAuint32));
    }

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IVideoDecoderCapabilities_GetVideoDecoderCapabilities
//***************************************************************************
static XAresult
IVideoDecoderCapabilities_GetVideoDecoderCapabilities(
    XAVideoDecoderCapabilitiesItf self,
    XAuint32 decoderId,
    XAuint32 *pIndex,
    XAVideoCodecDescriptor *pDescriptor)
{
    XA_ENTER_INTERFACE

    result = GetVideoCodecCapabilities(
        decoderId,
        pIndex,
        pDescriptor,
        VideoDecoderDescriptors);

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IVideoDecoderCapabilities_Itf
//***************************************************************************
static const
struct XAVideoDecoderCapabilitiesItf_ IVideoDecoderCapabilities_Itf =
{
    IVideoDecoderCapabilities_GetVideoDecoders,
    IVideoDecoderCapabilities_GetVideoDecoderCapabilities
};

//***************************************************************************
// IVideoDecoderCapabilities_init
//***************************************************************************
void IVideoDecoderCapabilities_init(void *self)
{
    IVideoDecoderCapabilities *pCaps = (IVideoDecoderCapabilities *)self;
    if (pCaps)
    {
        pCaps->mItf = &IVideoDecoderCapabilities_Itf;
    }
}

