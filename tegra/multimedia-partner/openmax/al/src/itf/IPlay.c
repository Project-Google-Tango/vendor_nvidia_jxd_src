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

/* Play implementation */

#include "linux_mediaplayer.h"

static XAresult IPlay_GetPosition(XAPlayItf self, XAmillisecond *pMsec)
{
    PlayerData *pData = NULL;
    CMediaPlayer *pCMediaPlayer = NULL;
    IPlay *pPlay = (IPlay *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPlay && pMsec);

    pData = (PlayerData *)pPlay->pData;
    AL_CHK_ARG(pData);

    pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pPlay);
    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData);

    interface_lock_poke(pPlay);
    result = ALBackendMediaPlayerGetPosition(pCMediaPlayer, pMsec);
    interface_unlock_poke(pPlay);

    XA_LEAVE_INTERFACE;
}
static XAresult IPlay_SetPlayState(XAPlayItf self, XAuint32 state)
{
    PlayerData *pData = NULL;
    CMediaPlayer *pCMediaPlayer = NULL;
    IPlay *pPlay = (IPlay *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPlay && pPlay->pData);

    pData = (PlayerData *)pPlay->pData;
    pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pPlay);
    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData);
    interface_lock_poke(pPlay);
    result = ALBackendMediaPlayerSetPlayState(pCMediaPlayer, state);
    if (result == XA_RESULT_SUCCESS)
        pData->mState = state;
    interface_unlock_poke(pPlay);
    XA_LEAVE_INTERFACE;
}

static XAresult IPlay_GetPlayState(XAPlayItf self, XAuint32 *pState)
{
    PlayerData *pData = NULL;
    IPlay *pPlay = (IPlay *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPlay && pState);

    pData = (PlayerData *)pPlay->pData;
    AL_CHK_ARG(pData);

    interface_lock_poke(pPlay);
    *pState = pData->mState;
    interface_unlock_poke(pPlay);

    XA_LEAVE_INTERFACE;
}

static XAresult IPlay_GetDuration(XAPlayItf self, XAmillisecond *pMsec)
{
    PlayerData *pData = NULL;
    CMediaPlayer *pCMediaPlayer = NULL;
    IPlay *pPlay = (IPlay *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPlay && pMsec);

    pData = (PlayerData *)pPlay->pData;
    AL_CHK_ARG(pData);

    pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pPlay);
    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData);

    interface_lock_poke(pPlay);
    result = ALBackendMediaPlayerGetDuration(pCMediaPlayer, pMsec);
    if (result != XA_RESULT_SUCCESS)
        *pMsec = XA_TIME_UNKNOWN;
    pData->mDuration = *pMsec;
    interface_unlock_poke(pPlay);

    XA_LEAVE_INTERFACE;
}



static XAresult IPlay_RegisterCallback(
    XAPlayItf self,
    xaPlayCallback callback,
    void *pContext)
{
    PlayerData *pData = NULL;
    IPlay *pPlay = (IPlay *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPlay && pPlay->pData);

    pData = (PlayerData *)pPlay->pData;

    interface_lock_poke(pPlay);
    pData->mCallback = callback;
    pData->mContext = pContext;
    interface_unlock_poke(pPlay);

    XA_LEAVE_INTERFACE;
}

static XAresult IPlay_SetCallbackEventsMask(XAPlayItf self, XAuint32 eventFlags)
{
    PlayerData *pData = NULL;
    IPlay *pPlay = (IPlay *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPlay && pPlay->pData);
    pData = (PlayerData *)pPlay->pData;

    if(eventFlags &
        ~(XA_PLAYEVENT_HEADATEND |
          XA_PLAYEVENT_HEADATMARKER |
          XA_PLAYEVENT_HEADATNEWPOS |
          XA_PLAYEVENT_HEADMOVING |
          XA_PLAYEVENT_HEADSTALLED))
    {
        result = XA_RESULT_PARAMETER_INVALID;
        XA_LEAVE_INTERFACE;
    }

    if (pData->mEventFlags != eventFlags)
    {
        interface_lock_poke(pPlay);
        pData->mEventFlags = eventFlags;
        interface_unlock_poke(pPlay);
    }

    XA_LEAVE_INTERFACE;
}

static XAresult IPlay_GetCallbackEventsMask(XAPlayItf self, XAuint32 *pEventFlags)
{
    PlayerData *pData = NULL;
    IPlay *pPlay = (IPlay *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPlay && pPlay->pData);
    AL_CHK_ARG(pEventFlags);

    pData = (PlayerData *)pPlay->pData;

    interface_lock_peek(pPlay);
    *pEventFlags = pData->mEventFlags;
    interface_unlock_peek(pPlay);

    XA_LEAVE_INTERFACE;
}

static XAresult IPlay_SetMarkerPosition(XAPlayItf self, XAmillisecond mSec)
{
    PlayerData *pData = NULL;
    CMediaPlayer *pCMediaPlayer = NULL;
    IPlay *pPlay = (IPlay *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPlay && pPlay->pData);

    pData = (PlayerData *)pPlay->pData;
    pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pPlay);
    AL_CHK_ARG(pCMediaPlayer);

    if (pData->mMarkerPosition != mSec)
    {
        interface_lock_peek(pPlay);
        pData->mMarkerPosition = mSec;
        ALBackendMediaPlayerSignalThread(pCMediaPlayer);
        interface_unlock_peek(pPlay);
    }
    XA_LEAVE_INTERFACE;
}

static XAresult IPlay_ClearMarkerPosition(XAPlayItf self)
{
    PlayerData *pData = NULL;
    CMediaPlayer *pCMediaPlayer = NULL;
    IPlay *pPlay = (IPlay *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPlay && pPlay->pData);

    pData = (PlayerData *)pPlay->pData;
    pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pPlay);
    AL_CHK_ARG(pCMediaPlayer);

    interface_lock_peek(pPlay);
    pData->mMarkerPosition = XA_TIME_UNKNOWN;
    ALBackendMediaPlayerSignalThread(pCMediaPlayer);
    interface_unlock_peek(pPlay);

    XA_LEAVE_INTERFACE;
}

static XAresult IPlay_GetMarkerPosition(XAPlayItf self, XAmillisecond *pMsec)
{
    PlayerData *pData = NULL;
    IPlay *pPlay = (IPlay *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPlay && pPlay->pData && pMsec);
    pData = (PlayerData *)pPlay->pData;

    if (pData->mMarkerPosition == XA_TIME_UNKNOWN)
    {
        result = XA_RESULT_PRECONDITIONS_VIOLATED;
        XA_LEAVE_INTERFACE
    }
    interface_lock_poke(pPlay);
    *pMsec = pData->mMarkerPosition;
    interface_unlock_poke(pPlay);

    XA_LEAVE_INTERFACE;
}

static XAresult IPlay_SetPositionUpdatePeriod(XAPlayItf self, XAmillisecond mSec)
{
    PlayerData *pData = NULL;
    CMediaPlayer *pCMediaPlayer = NULL;
    IPlay *pPlay = (IPlay *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPlay && pPlay->pData);

    pData = (PlayerData *)pPlay->pData;
    pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pPlay);
    AL_CHK_ARG(pCMediaPlayer);

    if (pData->mPositionUpdatePeriod != mSec)
    {
        interface_lock_peek(pPlay);
        pData->mPositionUpdatePeriod = mSec;
        ALBackendMediaPlayerSignalThread(pCMediaPlayer);
        interface_unlock_peek(pPlay);
    }
    XA_LEAVE_INTERFACE;
}

static XAresult
IPlay_GetPositionUpdatePeriod(
    XAPlayItf self,
    XAmillisecond *pMsec)
{
    PlayerData *pData = NULL;
    IPlay *pPlay = (IPlay *)self;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPlay && pPlay->pData && pMsec);

    pData = (PlayerData *)pPlay->pData;

    interface_lock_peek(pPlay);
    *pMsec = pData->mPositionUpdatePeriod;
    interface_unlock_peek(pPlay);

    XA_LEAVE_INTERFACE;
}


static const struct XAPlayItf_ IPlay_Itf =
{
    IPlay_SetPlayState,
    IPlay_GetPlayState,
    IPlay_GetDuration,
    IPlay_GetPosition,
    IPlay_RegisterCallback,
    IPlay_SetCallbackEventsMask,
    IPlay_GetCallbackEventsMask,
    IPlay_SetMarkerPosition,
    IPlay_ClearMarkerPosition,
    IPlay_GetMarkerPosition,
    IPlay_SetPositionUpdatePeriod,
    IPlay_GetPositionUpdatePeriod
};

void IPlay_init(void *self)
{
    PlayerData *pPlayerData = NULL;
    IPlay *pPlay = (IPlay *)self;
    XA_ENTER_INTERFACE_VOID;

    if (pPlay)
    {
        pPlay->mItf = &IPlay_Itf;
        pPlayerData = (PlayerData *)malloc(sizeof(PlayerData));
        if (pPlayerData)
        {
            memset(pPlayerData, 0, sizeof(PlayerData));
            pPlayerData->mState = XA_PLAYSTATE_STOPPED;
            pPlayerData->mDuration = 0;
            pPlayerData->mCallback = NULL;
            pPlayerData->mContext = NULL;
            pPlayerData->mEventFlags = 0;
            pPlayerData->mMarkerPosition = XA_TIME_UNKNOWN;
            pPlayerData->mPositionUpdatePeriod = 1000;
            pPlayerData->mLastSeekPosition = 0;
            pPlayerData->mFramesSinceLastSeek = 0;
            pPlayerData->mFramesSincePositionUpdate = 0;
            pPlayerData->stopThread = XA_BOOLEAN_FALSE;
            pPlayerData->reloadValues = XA_BOOLEAN_FALSE;
            pPlayerData->playThreadSema = g_cond_new();
            pPlayerData->playThreadMutex = g_mutex_new();
            pPlayerData->playStateChange = g_cond_new();
            pPlayerData->playStateChangeMutex = g_mutex_new();
        }
        pPlay->pData = (void *)pPlayerData;
    }
    XA_LEAVE_INTERFACE_VOID;
}

void IPlay_deinit(void *self)
{
    PlayerData *pPlayerData = NULL;
    IPlay *pPlay = (IPlay *)self;
    XA_ENTER_INTERFACE_VOID;
    if (pPlay && pPlay->pData)
    {
        pPlayerData = (PlayerData *)pPlay->pData;
        pPlayerData->stopThread = XA_BOOLEAN_TRUE;
        if (pPlayerData->playThreadSema != NULL)
        {
            g_mutex_lock(pPlayerData->playThreadMutex);
            g_cond_signal(pPlayerData->playThreadSema);
            g_mutex_unlock(pPlayerData->playThreadMutex);
        }
        if (pPlayerData->playStateChange != NULL)
        {
            g_mutex_lock(pPlayerData->playStateChangeMutex);
            g_cond_signal(pPlayerData->playStateChange);
            g_mutex_unlock(pPlayerData->playStateChangeMutex);
        }
        if (pPlayerData->hCallbackHandle)
            g_thread_join(pPlayerData->hCallbackHandle);
        if (pPlayerData->playThreadMutex != NULL)
            g_mutex_free(pPlayerData->playThreadMutex);
        if (pPlayerData->playThreadSema != NULL)
            g_cond_free(pPlayerData->playThreadSema);
        if (pPlayerData->playStateChange != NULL)
            g_cond_free(pPlayerData->playStateChange);
        if (pPlayerData->playStateChangeMutex != NULL)
            g_mutex_free(pPlayerData->playStateChangeMutex);
        free(pPlayerData);
        pPlay->pData = NULL;
    }
    XA_LEAVE_INTERFACE_VOID;
}

