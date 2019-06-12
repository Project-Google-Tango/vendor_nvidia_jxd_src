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

/* Seek implementation */

#include "linux_mediaplayer.h"

static XAresult
ISeek_SetPosition(
    XASeekItf self,
    XAmillisecond pos,
    XAuint32 seekMode)
{
    CMediaPlayer *pCMediaPlayer = NULL;
    ISeek *pSeek = (ISeek *)self;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pSeek);

    pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pSeek);
    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData);

    interface_lock_poke(pSeek);
    result = ALBackendMediaPlayerSeek(pCMediaPlayer, pos, seekMode);
    if (result != XA_RESULT_SUCCESS)
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
    }
    interface_unlock_poke(pSeek);

    XA_LEAVE_INTERFACE
}

static XAresult
ISeek_SetLoop(
    XASeekItf self,
    XAboolean loopEnable,
    XAmillisecond startPos,
    XAmillisecond endPos)
{
    SeekData *pSeekData = NULL;
    CMediaPlayer *pCMediaPlayer = NULL;
    ISeek *pSeek = (ISeek *)self;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pSeek && pSeek->pData);
    XA_LOGD("\nISeek_SetLoop: loopenabled %d - start = %d end = %d\n",
        loopEnable, startPos, endPos);

    if (endPos <= startPos)
    {
        XA_LOGE("\n\nError endPos <= startPos\n");
        result = XA_RESULT_PARAMETER_INVALID;
        XA_LEAVE_INTERFACE
    }
    pSeekData = (SeekData *)pSeek->pData;
    pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pSeek);
    AL_CHK_ARG(pCMediaPlayer);

    interface_lock_poke(pSeek);
    pSeekData->loopEnable = loopEnable;
    pSeekData->startPos = startPos;
    pSeekData->endPos = (endPos == -1) ? XA_TIME_UNKNOWN : endPos;
    result = ALBackendMediaPlayerSetLoop(pCMediaPlayer);
    interface_unlock_poke(pSeek);

    XA_LEAVE_INTERFACE
}

static XAresult
ISeek_GetLoop(
    XASeekItf self,
    XAboolean *pLoopEnabled,
    XAmillisecond *pStartPos,
    XAmillisecond *pEndPos)
{
    SeekData *pSeekData = NULL;
    ISeek *pSeek = (ISeek *)self;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pSeek && pSeek->pData);
    AL_CHK_ARG(pLoopEnabled && pStartPos && pEndPos);

    pSeekData = (SeekData *)pSeek->pData;

    interface_lock_poke(pSeek);
    *pLoopEnabled = pSeekData->loopEnable;
    *pStartPos = pSeekData->startPos;
    *pEndPos = pSeekData->endPos;
    interface_unlock_poke(pSeek);

    XA_LOGD("\nlGetoop = %d - startpos = %d end pos = %d\n",
         *pLoopEnabled, *pStartPos, *pEndPos);

    XA_LEAVE_INTERFACE
}

static const struct XASeekItf_ ISeek_Itf = {
    ISeek_SetPosition,
    ISeek_SetLoop,
    ISeek_GetLoop
};

void ISeek_init(void *self)
{
    SeekData *pSeekData = NULL;
    ISeek *pSeek = (ISeek *)self;

    XA_ENTER_INTERFACE_VOID
    if (pSeek)
    {
        pSeek->mItf = &ISeek_Itf;
        pSeekData = (SeekData *)malloc(sizeof(SeekData));
        if (pSeekData)
        {
            pSeekData->loopEnable = XA_BOOLEAN_FALSE;
            pSeekData->startPos = XA_TIME_UNKNOWN;
            pSeekData->endPos = XA_TIME_UNKNOWN;
            pSeek->pData = pSeekData;
        }
    }

    XA_LEAVE_INTERFACE_VOID
}

void ISeek_deinit(void *self)
{
    ISeek *pSeek = (ISeek *)self;

    XA_ENTER_INTERFACE_VOID
    if (pSeek)
    {
        SeekData *pSeekData = (SeekData *)pSeek->pData;
        if (pSeekData)
        {
            free(pSeekData);
            pSeek->pData = NULL;
        }
    }

    XA_LEAVE_INTERFACE_VOID
}

