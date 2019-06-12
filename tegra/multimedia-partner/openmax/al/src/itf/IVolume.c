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

/* Volume implementation */

#include "linux_outputmix.h"
#include "linux_mediaplayer.h"
#include "linux_mediarecorder.h"

static XAresult
IVolume_SetVolumeLevel(
    XAVolumeItf self,
    XAmillibel level)
{
    IVolume *pVolume = (IVolume *)self;
    COutputMix *pCOutputMix = NULL;
    CMediaPlayer *pCMediaPlayer = NULL;
    CMediaRecorder *pCMediaRecorder = NULL;
    VolumeData *pVolumeData = NULL;
    XAuint32 objectID = 0;

    XA_ENTER_INTERFACE;

    if ((level < XA_MILLIBEL_MIN) || (level > XA_MILLIBEL_MAX))
    {
        XA_LOGE("\nError %s[%d]: Level = %d\n", __FUNCTION__, __LINE__, level);
        result = XA_RESULT_PARAMETER_INVALID;
        XA_LEAVE_INTERFACE;
    }

    AL_CHK_ARG(pVolume && pVolume->pData);
    pVolumeData = (VolumeData *)pVolume->pData;
    AL_CHK_ARG(InterfaceToIObject(pVolume));

    objectID = InterfaceToObjectID(pVolume);

    interface_lock_poke(pVolume);
    switch (objectID)
    {
        case XA_OBJECTID_MEDIAPLAYER:
        {
            pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pVolume);
            result = ALBackendMediaPlayerSetVolume(pCMediaPlayer, level);
            break;
        }
        case XA_OBJECTID_OUTPUTMIX:
        case SL_OBJECTID_OUTPUTMIX:
        {
            pCOutputMix = (COutputMix *)InterfaceToIObject(pVolume);
            result = ALBackendOutputMixSetVolume(pCOutputMix, level);
            break;
        }
        case XA_OBJECTID_MEDIARECORDER:
        {
            pCMediaRecorder = (CMediaRecorder *)InterfaceToIObject(pVolume);
            result = ALBackendMediaRecorderSetVolume(pCMediaRecorder, level);
            break;
        }
        default:
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            break;
    }
    if (result == XA_RESULT_SUCCESS)
    {
        pVolumeData->mLevel = level;
    }
    interface_unlock_poke(pVolume);

    XA_LEAVE_INTERFACE;
}

static XAresult
IVolume_GetVolumeLevel(
    XAVolumeItf self,
    XAmillibel *pLevel)
{
    IVolume *pVolume = (IVolume *)self;
    COutputMix *pCOutputMix = NULL;
    CMediaPlayer *pCMediaPlayer = NULL;
    CMediaRecorder *pCMediaRecorder = NULL;
    VolumeData *pVolumeData = NULL;
    XAuint32 objectID = 0;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pVolume && pVolume->pData);
    pVolumeData = (VolumeData *)pVolume->pData;
    AL_CHK_ARG(InterfaceToIObject(pVolume));

    objectID = InterfaceToObjectID(pVolume);

    interface_lock_poke(pVolume);
    switch (objectID)
    {
        case XA_OBJECTID_MEDIAPLAYER:
        {
            pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pVolume);
            result = ALBackendMediaPlayerGetVolume(pCMediaPlayer, pLevel);
            break;
        }
        case XA_OBJECTID_OUTPUTMIX:
        case SL_OBJECTID_OUTPUTMIX:
        {
            pCOutputMix = (COutputMix *)InterfaceToIObject(pVolume);
            result = ALBackendOutputMixGetVolume(pCOutputMix, pLevel);
            break;
        }
        case XA_OBJECTID_MEDIARECORDER:
        {
            pCMediaRecorder = (CMediaRecorder *)InterfaceToIObject(pVolume);
            result = ALBackendMediaRecorderGetVolume(pCMediaRecorder, pLevel);
            break;
        }
        default:
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            break;
    }
    if (result == XA_RESULT_SUCCESS)
    {
        pVolumeData->mLevel = *pLevel;
    }
    interface_unlock_poke(pVolume);

    XA_LEAVE_INTERFACE;
}

static XAresult
IVolume_GetMaxVolumeLevel (
    XAVolumeItf self,
    XAmillibel * pMaxLevel)
{
    IVolume *pVolume = (IVolume *)self;
    COutputMix *pCOutputMix = NULL;
    CMediaPlayer *pCMediaPlayer = NULL;
    CMediaRecorder *pCMediaRecorder = NULL;
    XAuint32 objectID = 0;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pVolume && pVolume->pData);
    AL_CHK_ARG(InterfaceToIObject(pVolume));

    objectID = InterfaceToObjectID(pVolume);
    interface_lock_poke(pVolume);
    switch (objectID)
    {
        case XA_OBJECTID_MEDIAPLAYER:
        {
            pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pVolume);
            result = ALBackendMediaPlayerGetMaxVolume(pCMediaPlayer, pMaxLevel);
            break;
        }
        case XA_OBJECTID_OUTPUTMIX:
        case SL_OBJECTID_OUTPUTMIX:
        {
            pCOutputMix = (COutputMix *)InterfaceToIObject(pVolume);
            result = ALBackendOutputMixGetMaxVolume(pCOutputMix, pMaxLevel);
            break;
        }
        case XA_OBJECTID_MEDIARECORDER:
        {
            pCMediaRecorder = (CMediaRecorder *)InterfaceToIObject(pVolume);
            result = ALBackendMediaRecorderGetMaxVolume(pCMediaRecorder, pMaxLevel);
            break;
        }
        default:
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            break;
    }
    interface_unlock_poke(pVolume);

    XA_LEAVE_INTERFACE;
}

static XAresult
IVolume_SetMute(
    XAVolumeItf self,
    XAboolean mute)
{
    IVolume *pVolume = (IVolume *)self;
    COutputMix *pCOutputMix = NULL;
    CMediaPlayer *pCMediaPlayer = NULL;
    VolumeData *pVolumeData = NULL;
    CMediaRecorder *pCMediaRecorder = NULL;
    XAuint32 objectID = 0;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pVolume && pVolume->pData);
    pVolumeData = (VolumeData *)pVolume->pData;
    AL_CHK_ARG(InterfaceToIObject(pVolume));

    objectID = InterfaceToObjectID(pVolume);
    interface_lock_poke(pVolume);
    switch (objectID)
    {
        case XA_OBJECTID_MEDIAPLAYER:
        {
            pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pVolume);
            result = ALBackendMediaPlayerSetMute(pCMediaPlayer, mute);
            break;
        }
        case XA_OBJECTID_OUTPUTMIX:
        case SL_OBJECTID_OUTPUTMIX:
        {
            pCOutputMix = (COutputMix *)InterfaceToIObject(pVolume);
            result = ALBackendOutputMixSetMute(pCOutputMix, mute);
            break;
        }
        case XA_OBJECTID_MEDIARECORDER:
        {
            pCMediaRecorder = (CMediaRecorder *)InterfaceToIObject(pVolume);
            result = ALBackendMediaRecorderSetMute(pCMediaRecorder, mute);
            break;
        }
        default:
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            break;
    }
    if (XA_RESULT_SUCCESS == result)
    {
        pVolumeData->mMute = mute;
    }
    interface_unlock_poke(pVolume);

    XA_LEAVE_INTERFACE;
}

static XAresult
IVolume_GetMute(
    XAVolumeItf self,
    XAboolean *pMute)
{
    IVolume *pVolume = (IVolume *)self;
    COutputMix *pCOutputMix = NULL;
    CMediaPlayer *pCMediaPlayer = NULL;
    VolumeData *pVolumeData = NULL;
    CMediaRecorder *pCMediaRecorder = NULL;
    XAuint32 objectID = 0;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pVolume && pVolume->pData);
    pVolumeData = (VolumeData *)pVolume->pData;
    AL_CHK_ARG(InterfaceToIObject(pVolume));

    objectID = InterfaceToObjectID(pVolume);

    interface_lock_poke(pVolume);
    switch (objectID)
    {
        case XA_OBJECTID_MEDIAPLAYER:
        {
            pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pVolume);
            result = ALBackendMediaPlayerGetMute(pCMediaPlayer, pMute);
            break;
        }
        case XA_OBJECTID_OUTPUTMIX:
        case SL_OBJECTID_OUTPUTMIX:
        {
            pCOutputMix = (COutputMix *)InterfaceToIObject(pVolume);
            result = ALBackendOutputMixGetMute(pCOutputMix, pMute);
            break;
        }
        case XA_OBJECTID_MEDIARECORDER:
        {
            pCMediaRecorder = (CMediaRecorder *)InterfaceToIObject(pVolume);
            result = ALBackendMediaRecorderGetMute(pCMediaRecorder, pMute);
            break;
        }
        default:
            result = XA_RESULT_FEATURE_UNSUPPORTED;
            break;
    }
    interface_unlock_poke(pVolume);

    XA_LEAVE_INTERFACE;
}

static XAresult
IVolume_EnableStereoPosition(
    XAVolumeItf self,
    XAboolean enable)
{
    IVolume *pVolume = (IVolume *)self;
    COutputMix *pCOutputMix = NULL;
    VolumeData *pVolumeData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pVolume && pVolume->pData);
    pVolumeData = (VolumeData *)pVolume->pData;
    pCOutputMix = (COutputMix *)InterfaceToIObject(pVolume);
    AL_CHK_ARG(pCOutputMix);

    interface_lock_poke(pVolume);
    //TODO: Add support in backend (Gst)
    if (XA_RESULT_SUCCESS == result)
    {
        pVolumeData->mEnableStereoPosition = enable;
    }
    interface_unlock_poke(pVolume);

    XA_LEAVE_INTERFACE;
}

static XAresult
IVolume_IsEnabledStereoPosition(
    XAVolumeItf self,
    XAboolean *pEnable)
{
    IVolume *pVolume = (IVolume *)self;
    VolumeData *pVolumeData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pVolume && pVolume->pData);
    pVolumeData = (VolumeData *)pVolume->pData;
    interface_lock_poke(pVolume);
    *pEnable = pVolumeData->mEnableStereoPosition;
    interface_unlock_poke(pVolume);

    XA_LEAVE_INTERFACE;
}

static XAresult
IVolume_SetStereoPosition(
    XAVolumeItf self,
    XApermille stereoPosition)
{
    IVolume *pVolume = (IVolume *)self;
    VolumeData *pVolumeData = NULL;
    XApermille oldStereoPosition = 0;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pVolume && pVolume->pData);

    if ((stereoPosition < -1000) && (stereoPosition > 1000))
    {
        result = XA_RESULT_PARAMETER_INVALID;
        XA_LEAVE_INTERFACE;
    }
    pVolumeData = (VolumeData *)pVolume->pData;
    interface_lock_poke(pVolume);
    oldStereoPosition = pVolumeData->mStereoPosition;
    if (oldStereoPosition != stereoPosition)
    {
        pVolumeData->mStereoPosition = stereoPosition;
        ///TODO: Add SetSteroPosition support in omxil alsarender component
    }
    interface_unlock_poke(pVolume);

    XA_LEAVE_INTERFACE;
}
static XAresult
IVolume_GetStereoPosition(
    XAVolumeItf self,
    XApermille *pStereoPosition)
{
    IVolume *pVolume = (IVolume *)self;
    VolumeData *pVolumeData = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pVolume && pVolume->pData);
    pVolumeData = (VolumeData *)pVolume->pData;
    interface_lock_poke(pVolume);
    *pStereoPosition = pVolumeData->mStereoPosition;
    interface_unlock_poke(pVolume);

    XA_LEAVE_INTERFACE;
}

static const struct XAVolumeItf_ IVolume_Itf = {
    IVolume_SetVolumeLevel,
    IVolume_GetVolumeLevel,
    IVolume_GetMaxVolumeLevel,
    IVolume_SetMute,
    IVolume_GetMute,
    IVolume_EnableStereoPosition,
    IVolume_IsEnabledStereoPosition,
    IVolume_SetStereoPosition,
    IVolume_GetStereoPosition
};

void IVolume_init(void* self)
{
    VolumeData *pVolumeData = NULL;
    IVolume* pVolume = (IVolume*)self;

    XA_ENTER_INTERFACE_VOID;
    if (pVolume)
    {
        pVolume->mItf = &IVolume_Itf;
        pVolumeData = (VolumeData *)malloc(sizeof(VolumeData));
        if (pVolumeData)
        {
            pVolumeData->mLevel = 0; //0dB default volume level
            pVolumeData->mMinVolumeLevel = XA_MILLIBEL_MIN;
            pVolumeData->mMaxVolumeLevel = XA_MILLIBEL_MAX;
            pVolumeData->mMute = XA_BOOLEAN_FALSE;
            pVolumeData->mEnableStereoPosition = XA_BOOLEAN_FALSE;
            pVolumeData->mStereoPosition = 0;
            pVolume->pData = (VolumeData *)pVolumeData;
        }
    }

    XA_LEAVE_INTERFACE_VOID;
}

void IVolume_deinit(void* self)
{
    IVolume* pVolume = (IVolume*)self;

    XA_ENTER_INTERFACE_VOID;
    if (pVolume && pVolume->pData)
    {
        free(pVolume->pData);
        pVolume->pData = NULL;
    }

    XA_LEAVE_INTERFACE_VOID;
}
