 /*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */


/* Copyright (C) 2010 The Android Open Source Project
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
/* VideoEncoderCapabilities implementation */

#include "linux_mediarecorder.h"

#define Q16FORMAT(x) (x)<<16

// AVC Constant Bitrate
static const XAVideoCodecDescriptor VideoEoncoderCodecDescriptor_avccbr =
{
    XA_VIDEOCODEC_AVC,
    1920,
    1080,
    Q16FORMAT(30),
    20000000, // 20 MBPS
    XA_RATECONTROLMODE_CONSTANTBITRATE,
    XA_VIDEOPROFILE_AVC_BASELINE,
    XA_VIDEOLEVEL_AVC_4
};

// AVC Variable Bitrate
static const XAVideoCodecDescriptor VideoEoncoderCodecDescriptor_avcvbr =
{
    XA_VIDEOCODEC_AVC,
    1920,
    1080,
    Q16FORMAT(30),
    20000000, // 20 MBPS
    XA_RATECONTROLMODE_VARIABLEBITRATE,
    XA_VIDEOPROFILE_AVC_BASELINE,
    XA_VIDEOLEVEL_AVC_4
};

// MPEG4 Constant Bitrate
static const XAVideoCodecDescriptor VideoEoncoderCodecDescriptor_mpeg4cbr =
{
    XA_VIDEOCODEC_MPEG4,
    1920,
    1080,
    Q16FORMAT(30),
    20000000, // 20 MBPS
    XA_RATECONTROLMODE_CONSTANTBITRATE,
    XA_VIDEOPROFILE_MPEG4_SIMPLE,
    XA_VIDEOLEVEL_MPEG4_3
};

// MPEG4 variable bitrate
static const XAVideoCodecDescriptor VideoEoncoderCodecDescriptor_mpeg4vbr =
{
    XA_VIDEOCODEC_MPEG4,
    1920,
    1080,
    Q16FORMAT(30),
    20000000, // 20 MBPS
    XA_RATECONTROLMODE_VARIABLEBITRATE,
    XA_VIDEOPROFILE_MPEG4_SIMPLE,
    XA_VIDEOLEVEL_MPEG4_3
};
// H263 Variable Bitrate
static const XAVideoCodecDescriptor VideoEoncoderCodecDescriptor_h263vbr =
{
    XA_VIDEOCODEC_H263,
    640,
    480,
    Q16FORMAT(30),
    20000000, // 20 MBPS
    XA_RATECONTROLMODE_VARIABLEBITRATE,
    XA_VIDEOPROFILE_H263_BASELINE,
    XA_VIDEOLEVEL_H263_45,
};

static const VideoCodecDescriptor VideoEncoderDescriptors[] =
{
    {XA_VIDEOCODEC_AVC, &VideoEoncoderCodecDescriptor_avccbr},
    {XA_VIDEOCODEC_AVC, &VideoEoncoderCodecDescriptor_avcvbr},
    {XA_VIDEOCODEC_MPEG4, &VideoEoncoderCodecDescriptor_mpeg4cbr},
    {XA_VIDEOCODEC_MPEG4, &VideoEoncoderCodecDescriptor_mpeg4vbr},
    {XA_VIDEOCODEC_H263, &VideoEoncoderCodecDescriptor_h263vbr},
    {XA_VIDEOCODEC_H263, NULL}
};

static const XAuint32 VideoCodecIds[] = {
    XA_VIDEOCODEC_AVC,
    XA_VIDEOCODEC_MPEG4,
    XA_VIDEOCODEC_H263
};

static const XAuint32 MaxVideoEncoders =
    sizeof(VideoCodecIds) / sizeof(VideoCodecIds[0]);

static XAresult
GetVideoCodecCapabilities(
    XAuint32 codecId,
    XAuint32 *pIndex,
    XAVideoCodecDescriptor *pDescriptor,
    const VideoCodecDescriptor *codecDescriptors)
{
    XAuint32 index;
    const VideoCodecDescriptor *cd = codecDescriptors;
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
    XA_LOGD("\n\nCodecId = %d - Index = %d\n", codecId, index);

    for ( ; NULL != cd->mDescriptor; ++cd)
    {
        if (cd->mCodecID == codecId)
        {
            if (0 == index)
            {
                *pDescriptor = *cd->mDescriptor;
                XA_LOGD("\n\nCodecId = %d - Index = %d - profile = %d"
                    "- level = %d\n", codecId, index,
                    pDescriptor->profileSetting, pDescriptor->levelSetting);
                XA_LEAVE_INTERFACE
            }
            --index;
        }
    }

    result = XA_RESULT_PARAMETER_INVALID;

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IVideoEncoderCapabilities_GetVideoEncoders
//***************************************************************************
static XAresult
IVideoEncoderCapabilities_GetVideoEncoders(
    XAVideoEncoderCapabilitiesItf self,
    XAuint32 *pNumEncoders,
    XAuint32 *pEncoderIds)
{
    XA_ENTER_INTERFACE

    AL_CHK_ARG(self && pNumEncoders)
    if (NULL == pEncoderIds)
    {
        *pNumEncoders = MaxVideoEncoders;
    }
    else
    {
        if (MaxVideoEncoders <= *pNumEncoders)
            *pNumEncoders = MaxVideoEncoders;
        memcpy(
            pEncoderIds,
            &VideoCodecIds,
            *pNumEncoders * sizeof(XAuint32));
    }

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IVideoEncoderCapabilities_GetVideoEncoderCapabilities
//***************************************************************************
static XAresult
IVideoEncoderCapabilities_GetVideoEncoderCapabilities(
    XAVideoEncoderCapabilitiesItf self,
    XAuint32 encoderId,
    XAuint32 *pIndex,
    XAVideoCodecDescriptor *pDescriptor)
{
    XA_ENTER_INTERFACE

    result = GetVideoCodecCapabilities(
        encoderId, pIndex, pDescriptor, VideoEncoderDescriptors);

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IVideoEncoderCapabilities_Itf
//***************************************************************************
static const
struct XAVideoEncoderCapabilitiesItf_ IVideoEncoderCapabilities_Itf =
{
    IVideoEncoderCapabilities_GetVideoEncoders,
    IVideoEncoderCapabilities_GetVideoEncoderCapabilities
};

//***************************************************************************
// IVideoEncoderCapabilities_Itf
//***************************************************************************
void IVideoEncoderCapabilities_init(void *self)
{
    IVideoEncoderCapabilities *pCaps = (IVideoEncoderCapabilities *)self;

    XA_ENTER_INTERFACE_VOID
    if (pCaps)
    {
        pCaps->mItf = &IVideoEncoderCapabilities_Itf;
    }

    XA_LEAVE_INTERFACE_VOID
}
