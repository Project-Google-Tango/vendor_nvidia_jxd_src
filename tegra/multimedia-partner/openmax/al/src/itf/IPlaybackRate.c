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

/* PlaybackRate implementation */

#include "linux_mediaplayer.h"

static XAresult IPlaybackRate_SetRate(XAPlaybackRateItf self, XApermille rate)
{


    IPlaybackRate *pPlaybackRate = (IPlaybackRate *)self;
    PlayRateData *pPlayRate = NULL;
    CMediaPlayer *pCMediaPlayer = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPlaybackRate && pPlaybackRate->pData);
    pPlayRate = (PlayRateData *)pPlaybackRate->pData;
    pCMediaPlayer = (CMediaPlayer *)InterfaceToIObject(pPlaybackRate);
    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData);

    if (rate < -32000 || rate > 32000)
    {
        result = XA_RESULT_PARAMETER_INVALID;
        XA_LEAVE_INTERFACE;
    }
    if (pPlayRate->CurrentRate != rate)
    {
        interface_lock_poke(pPlaybackRate);
        result = ALBackendMediaPlayerSetRate(pCMediaPlayer, rate);
        if (result == XA_RESULT_SUCCESS)
            pPlayRate->CurrentRate = rate;
        interface_unlock_poke(pPlaybackRate);
    }
    XA_LEAVE_INTERFACE;
}

static XAresult IPlaybackRate_GetRate(XAPlaybackRateItf self, XApermille *pRate)
{
    IPlaybackRate *pPlaybackRate = (IPlaybackRate *)self;
    PlayRateData *pPlayRate = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPlaybackRate && pRate);
    pPlayRate = (PlayRateData *)pPlaybackRate->pData;
    AL_CHK_ARG(pPlayRate);

    interface_lock_poke(pPlaybackRate);
    *pRate = pPlayRate->CurrentRate;
    interface_unlock_poke(pPlaybackRate);

    XA_LEAVE_INTERFACE;
}

static XAresult
IPlaybackRate_GetCapabilitiesOfRate(
    XAPlaybackRateItf self,
    XApermille rate,
    XAuint32 *pCapabilities)
{


    IPlaybackRate *pPlaybackRate = (IPlaybackRate *)self;
    PlayRateData *pPlayRate = NULL;
    XAint32 index = DEFAULT_PLAYRATEINDEX;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPlaybackRate && pPlaybackRate->pData);
    pPlayRate = (PlayRateData *)pPlaybackRate->pData;
    if (rate != 1000)
    {
        for (index = 0; index < MAX_PLAYRATERANGE; index++)
        {
            if (pPlayRate->MinRate[index] <= rate &&
                pPlayRate->MaxRate[index] >= rate)
                break;
        }
        if (index >= MAX_PLAYRATERANGE)
        {
            result = XA_RESULT_PARAMETER_INVALID;
            XA_LEAVE_INTERFACE;
        }
    }

    interface_lock_poke(pPlaybackRate);
    if (pCapabilities)
        *pCapabilities = pPlayRate->CurrentCapabilities[index];
    interface_unlock_poke(pPlaybackRate);

    XA_LEAVE_INTERFACE;
}

static XAresult IPlaybackRate_GetProperties(XAPlaybackRateItf self, XAuint32 *pProperties)
{
    IPlaybackRate *pPlaybackRate = (IPlaybackRate *)self;
    PlayRateData *pPlayRate = NULL;
    XAint32 index = DEFAULT_PLAYRATEINDEX;
    XApermille rate = 1000;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPlaybackRate && pPlaybackRate->pData);
    pPlayRate = (PlayRateData *)pPlaybackRate->pData;
    rate = pPlayRate->CurrentRate;
    if (rate != 1000)
    {
        for (index = 0; index < MAX_PLAYRATERANGE; index++)
        {
            if (pPlayRate->MinRate[index] <= rate && pPlayRate->MaxRate[index] >= rate)
             break;
        }
        if (index >= MAX_PLAYRATERANGE)
        {
            result = XA_RESULT_PARAMETER_INVALID;
            XA_LEAVE_INTERFACE;
        }
    }

    interface_lock_poke(pPlaybackRate);
    if (pProperties)
        *pProperties = pPlayRate->CurrentCapabilities[index];
    interface_unlock_poke(pPlaybackRate);

    XA_LEAVE_INTERFACE;
}
static XAresult IPlaybackRate_SetPropertyConstraints(XAPlaybackRateItf self, XAuint32 constraints)
{
    IPlaybackRate *pPlaybackRate = (IPlaybackRate *)self;
    PlayRateData *pPlayRate = NULL;
    XAint32 index = DEFAULT_PLAYRATEINDEX;
    XApermille rate;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPlaybackRate && constraints);
    pPlayRate = (PlayRateData *)pPlaybackRate->pData;
    AL_CHK_ARG(pPlayRate);
    rate = pPlayRate->CurrentRate;
    if (rate != 1000)
    {
        for (index = 0; index < MAX_PLAYRATERANGE; index++)
        {
            if (pPlayRate->MinRate[index] <= rate && pPlayRate->MaxRate[index] >= rate)
             break;
        }
        if (index >= MAX_PLAYRATERANGE)
        {
            result = XA_RESULT_PARAMETER_INVALID;
            XA_LEAVE_INTERFACE;
        }
    }
    if (constraints & ~(pPlayRate->SupportedCapabilities[index]))
    {
        result = XA_RESULT_PARAMETER_INVALID;
        XA_LEAVE_INTERFACE;
    }

    interface_lock_poke(pPlaybackRate);
    pPlayRate->CurrentCapabilities[index] = constraints;
    interface_unlock_poke(pPlaybackRate);

    XA_LEAVE_INTERFACE;
}

static XAresult
IPlaybackRate_GetRateRange(
    XAPlaybackRateItf self,
    XAuint8 index,
    XApermille *pMinRate,
    XApermille *pMaxRate,
    XApermille *pStepSize,
    XAuint32 *pCapabilities)
{
    IPlaybackRate *pPlaybackRate = (IPlaybackRate *)self;
    PlayRateData *pPlayRate = NULL;

    XA_ENTER_INTERFACE;
    AL_CHK_ARG(pPlaybackRate && pPlaybackRate->pData);

    pPlayRate = (PlayRateData *)pPlaybackRate->pData;
    AL_CHK_ARG(pMinRate && pMaxRate && pStepSize && pCapabilities);

    if (index >= MAX_PLAYRATERANGE)
    {
        result = XA_RESULT_PARAMETER_INVALID;
        XA_LEAVE_INTERFACE;
    }

    interface_lock_poke(pPlaybackRate);
    *pMinRate = pPlayRate->MinRate[index];
    *pMaxRate = pPlayRate->MaxRate[index];
    *pStepSize = pPlayRate->StepSize[index];
    *pCapabilities = pPlayRate->SupportedCapabilities[index];
    interface_unlock_poke(pPlaybackRate);

    XA_LEAVE_INTERFACE;
}

static const struct XAPlaybackRateItf_ IPlaybackRate_Itf = {
    IPlaybackRate_SetRate,
    IPlaybackRate_GetRate,
    IPlaybackRate_SetPropertyConstraints,
    IPlaybackRate_GetProperties,
    IPlaybackRate_GetCapabilitiesOfRate,
    IPlaybackRate_GetRateRange
};

void IPlaybackRate_init(void *self)
{
    IPlaybackRate *pPlaybackRate = (IPlaybackRate *)self;
    PlayRateData *pPlayRate = NULL;

    if (pPlaybackRate)
    {
        pPlaybackRate->mItf = &IPlaybackRate_Itf;
        pPlayRate = (PlayRateData *)malloc(sizeof(PlayRateData));
        if (pPlayRate)
        {
            pPlayRate->CurrentRate = 1000;
            pPlayRate->PreviousRate = 1000;

            pPlayRate->MaxRate[0] = -2000;
            pPlayRate->MinRate[0] = -32000;
            pPlayRate->StepSize[0] = 2000;
            pPlayRate->SupportedCapabilities[0] = pPlayRate->CurrentCapabilities[0] =
                XA_RATEPROP_SILENTAUDIO | XA_RATEPROP_STAGGEREDVIDEO;

            pPlayRate->MaxRate[1] = 1000;
            pPlayRate->MinRate[1] = -2000;
            pPlayRate->StepSize[1] = 500;
            pPlayRate->SupportedCapabilities[1] = pPlayRate->CurrentCapabilities[1] =
                XA_RATEPROP_SILENTAUDIO | XA_RATEPROP_STAGGEREDVIDEO;

            pPlayRate->MaxRate[2] = 1000;
            pPlayRate->MinRate[2] = 1000;
            pPlayRate->StepSize[2] = 0;
            pPlayRate->SupportedCapabilities[2] = pPlayRate->CurrentCapabilities[2] =
                XA_RATEPROP_NOPITCHCORAUDIO | XA_RATEPROP_SMOOTHVIDEO;

            pPlayRate->MaxRate[3] = 2000;
            pPlayRate->MinRate[3] = 1000;
            pPlayRate->StepSize[3] = 500;
            pPlayRate->SupportedCapabilities[3] = pPlayRate->CurrentCapabilities[3] =
                XA_RATEPROP_SILENTAUDIO | XA_RATEPROP_STAGGEREDVIDEO;

            pPlayRate->MaxRate[4] = 32000;
            pPlayRate->MinRate[4] = 2000;
            pPlayRate->StepSize[4] = 2000;
            pPlayRate->SupportedCapabilities[4] = pPlayRate->CurrentCapabilities[4] =
                XA_RATEPROP_SILENTAUDIO | XA_RATEPROP_STAGGEREDVIDEO;
        }
        pPlaybackRate->pData = (void*)pPlayRate;
    }
}

void IPlaybackRate_deinit(void *self)
{
    IPlaybackRate *pPlaybackRate = (IPlaybackRate *)self;

    if (pPlaybackRate && pPlaybackRate->pData)
    {
        free(pPlaybackRate->pData);
        pPlaybackRate->pData = NULL;
    }
}

