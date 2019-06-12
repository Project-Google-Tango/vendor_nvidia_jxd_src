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

/* StreamInformation implementation */

#include "linux_common.h"
#include "linux_mediaplayer.h"

static XAresult
IStreamInformation_QueryMediaContainerInformation(
    XAStreamInformationItf self,
    XAMediaContainerInformation *pInfo)
{
    StreamInfoData *pAlStreamInfo = NULL;
    CMediaPlayer *pCMediaPlayer = NULL;
    IStreamInformation *pStreamInfo = (IStreamInformation *)self;
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pInfo);
    AL_CHK_ARG(pStreamInfo && pStreamInfo->pData);
    pAlStreamInfo = (StreamInfoData *)pStreamInfo->pData;
    interface_lock_poke(pStreamInfo);
    pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pStreamInfo);
    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData);
    result = ALBackendMediaPlayerQueryContainerInfo(
        pCMediaPlayer,
        pAlStreamInfo);
    memcpy(
        pInfo, &pAlStreamInfo->ContainerInfo, sizeof(XAMediaContainerInformation));
    interface_unlock_poke(pStreamInfo);
    XA_LEAVE_INTERFACE
}

static XAresult
IStreamInformation_QueryStreamType(
    XAStreamInformationItf self,
    XAuint32 streamIndex,
    XAuint32 *pDomain)
{
    StreamInfoData *pAlStreamInfo = NULL;
    CMediaPlayer *pCMediaPlayer = NULL;
    IStreamInformation *pStreamInfo = (IStreamInformation *)self;
    XAuint32 StreamType = XA_DOMAINTYPE_UNKNOWN;
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pDomain && pStreamInfo && pStreamInfo->pData);
    pAlStreamInfo = (StreamInfoData *)pStreamInfo->pData;
    pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pStreamInfo);
    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData);

    interface_lock_poke(pStreamInfo);
    StreamType = (XAuint32)pAlStreamInfo->DomainType[streamIndex];
    switch (StreamType)
    {
        case XA_DOMAINTYPE_CONTAINER:
            *pDomain = (XAuint32)XA_DOMAINTYPE_CONTAINER;
            break;
        case XA_DOMAINTYPE_AUDIO:
            *pDomain = (XAuint32)XA_DOMAINTYPE_AUDIO;
            break;
        case XA_DOMAINTYPE_VIDEO:
            *pDomain = (XAuint32)XA_DOMAINTYPE_VIDEO;
            break;
        case XA_DOMAINTYPE_IMAGE:
            *pDomain = (XAuint32)XA_DOMAINTYPE_IMAGE;
            break;
        case XA_DOMAINTYPE_TIMEDTEXT:
            *pDomain = (XAuint32)XA_DOMAINTYPE_TIMEDTEXT;
            break;
        case XA_DOMAINTYPE_VENDOR:
            *pDomain = (XAuint32)XA_DOMAINTYPE_VENDOR;
            break;
        case XA_DOMAINTYPE_UNKNOWN:
        default:
            *pDomain = (XAuint32)XA_DOMAINTYPE_UNKNOWN;
            break;
    }
    interface_unlock_poke(pStreamInfo);
    XA_LEAVE_INTERFACE;
}

static XAresult
IStreamInformation_QueryStreamInformation(
    XAStreamInformationItf self,
    XAuint32 streamIndex, /* [in] */
    void *info)          /* [out] */
{
    StreamInfoData *pParamStreamInfo = NULL;
    CMediaPlayer *pCMediaPlayer = NULL;
    IStreamInformation *pStreamInfo = (IStreamInformation *)self;
    XA_ENTER_INTERFACE

    AL_CHK_ARG(info && pStreamInfo && pStreamInfo->pData);
    pParamStreamInfo = (StreamInfoData *)pStreamInfo->pData;
    pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pStreamInfo);
    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData);
    interface_lock_poke(pStreamInfo);
    // stream 0 is the container, and other streams in the container are numbered 1..nbStream
    switch (pParamStreamInfo->DomainType[streamIndex])
    {
        case XA_DOMAINTYPE_CONTAINER:
        {
            XA_LOGD("XA_DOMAINTYPE_CONTAINER: %s[%d]\n", __FUNCTION__, __LINE__);
            memcpy((XAMediaContainerInformation *)info, &pParamStreamInfo->ContainerInfo,
                sizeof(XAMediaContainerInformation));
            break;
        }
        case XA_DOMAINTYPE_AUDIO:
        {
            XA_LOGD("XA_DOMAINTYPE_AUDIO: %s[%d]\n", __FUNCTION__, __LINE__);
            memset(
                &pParamStreamInfo->AudioStreamInfo, 0, sizeof(XAAudioStreamInformation));
            result = ALBackendMediaPlayerGetAudioStreamInfo(
                pCMediaPlayer,
                pParamStreamInfo);
            memcpy(
                (XAAudioStreamInformation *)info, &pParamStreamInfo->AudioStreamInfo,
                sizeof(XAAudioStreamInformation));
            XA_LOGD(" Domain Type Audio Stream Information:\n");
            XA_LOGD(" CodecId: %d\n", pParamStreamInfo->AudioStreamInfo.codecId);
            XA_LOGD(" Channels: %d\n", pParamStreamInfo->AudioStreamInfo.channels);
            XA_LOGD(" SampleRate: %d\n", pParamStreamInfo->AudioStreamInfo.sampleRate);
            XA_LOGD(" BitRate: %d\n", pParamStreamInfo->AudioStreamInfo.bitRate);
            XA_LOGD(" langCountry: %s\n", pParamStreamInfo->AudioStreamInfo.langCountry);
            XA_LOGD(" Duration: %d\n\n", pParamStreamInfo->AudioStreamInfo.duration);
            break;
        }
        case XA_DOMAINTYPE_VIDEO:
        {
            XA_LOGD("XA_DOMAINTYPE_VIDEO: %s[%d]\n", __FUNCTION__, __LINE__);
            memset(
                &pParamStreamInfo->VideoStreamInfo, 0, sizeof(XAVideoStreamInformation));
            result = ALBackendMediaPlayerGetVideoStreamInfo(
                pCMediaPlayer,
                pParamStreamInfo);
            memcpy(
                (XAVideoStreamInformation *)info, &pParamStreamInfo->VideoStreamInfo,
                sizeof(XAVideoStreamInformation));
            XA_LOGD(" Domain Type Video Stream Information:\n");
            XA_LOGD(" CodecId: %d\n", pParamStreamInfo->VideoStreamInfo.codecId);
            XA_LOGD(" Width: %d\n", pParamStreamInfo->VideoStreamInfo.width);
            XA_LOGD(" Height: %d\n", pParamStreamInfo->VideoStreamInfo.height);
            XA_LOGD(" FrameRate: %d.%d\n",
                (pParamStreamInfo->VideoStreamInfo.frameRate >> 16),
                (pParamStreamInfo->VideoStreamInfo.frameRate & 0xFFFF));
            XA_LOGD(" BitRate: %d bits/s\n", pParamStreamInfo->VideoStreamInfo.bitRate);
            XA_LOGD(" Duration: %d\n\n", pParamStreamInfo->VideoStreamInfo.duration);
            break;
        }
        case XA_DOMAINTYPE_IMAGE:
        case XA_DOMAINTYPE_TIMEDTEXT:
        case XA_DOMAINTYPE_MIDI:
        case XA_DOMAINTYPE_VENDOR:
            result = XA_RESULT_CONTENT_UNSUPPORTED;
            XA_LOGE("CONTENT_UNSUPPORTED %s[%d]\n", __FUNCTION__, __LINE__);
            break;
        default:
            XA_LOGE("XA_RESULT_INTERNAL_ERROR %s[%d]\n", __FUNCTION__, __LINE__);
            result = XA_RESULT_CONTENT_UNSUPPORTED;
            break;
    }
    interface_unlock_poke(pStreamInfo);
    XA_LEAVE_INTERFACE
}

static XAresult
IStreamInformation_QueryStreamName(
    XAStreamInformationItf self,
    XAuint32 streamIndex,
    XAuint16 *pNameSize,
    XAchar *pName)
{
    StreamInfoData *pAlStreamInfo = NULL;
    CMediaPlayer *pCMediaPlayer = NULL;
    IStreamInformation *pStreamInfo = (IStreamInformation *)self;
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pNameSize && pStreamInfo && pStreamInfo->pData);
    pAlStreamInfo = (StreamInfoData *)pStreamInfo->pData;
    pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pStreamInfo);
    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData);
    interface_lock_poke(pStreamInfo);

    if (pAlStreamInfo->IsStreamInfoProbed != XA_BOOLEAN_TRUE)
    {
        result =
            IStreamInformation_QueryMediaContainerInformation(
                self, &pAlStreamInfo->ContainerInfo);
        if (result != XA_RESULT_SUCCESS)
        {
            XA_LOGE("Error QueryContainerInfo - %s[%d]\n", __FUNCTION__, __LINE__);
            interface_unlock_poke(pStreamInfo);
            XA_LEAVE_INTERFACE;
        }
    }
    if (pName == NULL || *pNameSize < pAlStreamInfo->UriSize)
    {
        // Specify URI size
        *pNameSize = (XAuint16)pAlStreamInfo->UriSize;
        result = XA_RESULT_SUCCESS;
        interface_unlock_poke(pStreamInfo);
        XA_LEAVE_INTERFACE
    }
    strncpy(
        (char *)pName, (char *)pAlStreamInfo->pUri, pAlStreamInfo->UriSize + 1);
    XA_LOGD("\nUriSize = %d - UriName %s\n", pAlStreamInfo->UriSize, pName);
    interface_unlock_poke(pStreamInfo);
    XA_LEAVE_INTERFACE
}

static XAresult
IStreamInformation_RegisterStreamChangeCallback(
    XAStreamInformationItf self,
    xaStreamEventChangeCallback callback,
    void *pContext)
{
    StreamInfoData *pAlStreamInfo = NULL;
    IStreamInformation *pStreamInfo = (IStreamInformation *)self;
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pStreamInfo && pStreamInfo->pData);
    pAlStreamInfo = (StreamInfoData *)pStreamInfo->pData;
    interface_lock_poke(pStreamInfo);
    pAlStreamInfo->mCallback = callback;
    pAlStreamInfo->mContext = pContext;
    interface_unlock_poke(pStreamInfo);
    XA_LEAVE_INTERFACE
}

static XAresult
IStreamInformation_QueryActiveStreams(
    XAStreamInformationItf self,
    XAuint32 *pNumStreams,
    XAboolean *pActiveStreams)
{

    CMediaPlayer *pCMediaPlayer = NULL;
    StreamInfoData *pAlStreamInfo = NULL;
    IStreamInformation *pStreamInfo = (IStreamInformation *)self;
    XA_ENTER_INTERFACE

    AL_CHK_ARG(pNumStreams && pStreamInfo && pStreamInfo->pData);
    pAlStreamInfo = (StreamInfoData *)pStreamInfo->pData;
    pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pStreamInfo);
    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData);
    if (pActiveStreams == NULL)
    {
        *pNumStreams = pAlStreamInfo->ContainerInfo.numStreams;
         XA_LEAVE_INTERFACE
    }
    else if (*pNumStreams < pAlStreamInfo->ContainerInfo.numStreams)
    {
        result = XA_RESULT_PARAMETER_INVALID;
        XA_LEAVE_INTERFACE
    }
    result = ALBackendMediaPlayerQueryActiveStreams(
        pCMediaPlayer,
        pAlStreamInfo);
    *pNumStreams = pAlStreamInfo->ContainerInfo.numStreams;
    memcpy(pActiveStreams, &pAlStreamInfo->mActiveStreams,
        pAlStreamInfo->ContainerInfo.numStreams*sizeof(XAuint32));
    XA_LEAVE_INTERFACE
}

static XAresult
IStreamInformation_SetActiveStream(
    XAStreamInformationItf self,
    XAuint32 streamNum,
    XAboolean active,
    XAboolean commitNow)
{
    XA_ENTER_INTERFACE
    result = XA_RESULT_FEATURE_UNSUPPORTED;
    XA_LEAVE_INTERFACE
}

static const struct XAStreamInformationItf_ IStreamInformation_Itf = {
    IStreamInformation_QueryMediaContainerInformation,
    IStreamInformation_QueryStreamType,
    IStreamInformation_QueryStreamInformation,
    IStreamInformation_QueryStreamName,
    IStreamInformation_RegisterStreamChangeCallback,
    IStreamInformation_QueryActiveStreams,
    IStreamInformation_SetActiveStream
};

void IStreamInformation_init(void *self)
{
    IStreamInformation *pStreamInfo = (IStreamInformation *)self;
    StreamInfoData *pAlStreamInfo = NULL;
    XAint32 i = 0;
    if (pStreamInfo)
    {
        pStreamInfo->mItf = &IStreamInformation_Itf;
        pAlStreamInfo =
            (StreamInfoData *)malloc(sizeof(StreamInfoData));
        if (pAlStreamInfo)
        {
            memset(pAlStreamInfo, 0, sizeof(StreamInfoData));
            pStreamInfo->pData = (StreamInfoData *)pAlStreamInfo;
            for (i = 0; i < MAX_SUPPORTED_STREAMS; i++)
            {
                pAlStreamInfo->mActiveStreams[i] = XA_BOOLEAN_FALSE;
                pAlStreamInfo->DomainType[i] = XA_DOMAINTYPE_UNKNOWN;
            }
            pAlStreamInfo->IsStreamInfoProbed = XA_BOOLEAN_FALSE;
        }
    }
}

void IStreamInformation_deinit(void *self)
{
    IStreamInformation *pStreamInfo = (IStreamInformation *)self;
    if (pStreamInfo && pStreamInfo->pData)
    {
        free(pStreamInfo->pData);
        pStreamInfo->pData = NULL;
    }
}

