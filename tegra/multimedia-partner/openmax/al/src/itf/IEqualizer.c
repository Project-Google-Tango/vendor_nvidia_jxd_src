/*
 * Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
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

/* Equalizer implementation */

#include "linux_outputmix.h"

static XAresult
IEqualizer_SetEnabled(
    XAEqualizerItf self,
    XAboolean enabled)
{
    IEqualizer *pEqualizer = (IEqualizer *)self;
    COutputMix *pCOutputMix = NULL;
    EqualizerData *pEqualizerData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pEqualizer && pEqualizer->pData);
    pEqualizerData = (EqualizerData *)pEqualizer->pData;
    pCOutputMix = (COutputMix *)InterfaceToIObject(pEqualizer);
    AL_CHK_ARG(pCOutputMix);

    interface_lock_poke(pEqualizer);
    result = ALBackendEqualizerSetEnabled(
        pCOutputMix, enabled, pEqualizerData->mCurrentPreset);
    if (result == XA_RESULT_SUCCESS)
        pEqualizerData->mEnabled = enabled;
    interface_unlock_poke(pEqualizer);

    XA_LEAVE_INTERFACE
}

static XAresult
IEqualizer_IsEnabled(
    XAEqualizerItf self,
    XAboolean *pEnabled)
{
    IEqualizer *pEqualizer = (IEqualizer *)self;
    EqualizerData *pEqualizerData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pEnabled && pEqualizer && pEqualizer->pData);
    pEqualizerData = (EqualizerData *)pEqualizer->pData;

    interface_lock_poke(pEqualizer);
    *pEnabled = pEqualizerData->mEnabled;
    interface_unlock_poke(pEqualizer);

    XA_LEAVE_INTERFACE
}

static XAresult
IEqualizer_GetNumberOfBands(
    XAEqualizerItf self,
    XAuint16 *pNumBands)
{
    IEqualizer *pEqualizer = (IEqualizer *)self;
    COutputMix *pCOutputMix = NULL;
    EqualizerData *pEqualizerData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pNumBands && pEqualizer && pEqualizer->pData);
    pEqualizerData = (EqualizerData *)pEqualizer->pData;
    pCOutputMix = (COutputMix *)InterfaceToIObject(pEqualizer);
    AL_CHK_ARG(pCOutputMix);

    interface_lock_poke(pEqualizer);
    result = ALBackendEqualizerGetNumberOfBands(pCOutputMix, pNumBands);
    interface_unlock_poke(pEqualizer);

    XA_LEAVE_INTERFACE
}

static XAresult
IEqualizer_GetBandLevelRange(
    XAEqualizerItf self,
    XAmillibel *pMin,
    XAmillibel *pMax)
{
    //Any one of pMin and pMax can be null as per spec
    //Do not check them for null
    IEqualizer *pEqualizer = (IEqualizer *)self;
    COutputMix *pCOutputMix = NULL;
    EqualizerData *pEqualizerData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pEqualizer && pEqualizer->pData);
    pEqualizerData = (EqualizerData *)pEqualizer->pData;
    pCOutputMix = (COutputMix *)InterfaceToIObject(pEqualizer);
    AL_CHK_ARG(pCOutputMix);

    interface_lock_poke(pEqualizer);
    result = ALBackendEqualizerGetBandLevelRange(pCOutputMix, pMin, pMax);
    interface_unlock_poke(pEqualizer);

    XA_LEAVE_INTERFACE
}

static XAresult
IEqualizer_SetBandLevel(
    XAEqualizerItf self,
    XAuint16 band,
    XAmillibel level)
{
    IEqualizer *pEqualizer = (IEqualizer *)self;
    COutputMix *pCOutputMix = NULL;
    EqualizerData *pEqualizerData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pEqualizer && pEqualizer->pData);
    pEqualizerData = (EqualizerData *)pEqualizer->pData;
    pCOutputMix = (COutputMix *)InterfaceToIObject(pEqualizer);
    AL_CHK_ARG(pCOutputMix);

    interface_lock_poke(pEqualizer);
    result = ALBackendEqualizerSetBandLevel(pCOutputMix, band, level);
    //If SetBandLevel is called, this will disable preset settings
    if (result == XA_RESULT_SUCCESS)
        pEqualizerData->mCurrentPreset = XA_EQUALIZER_UNDEFINED;
    interface_unlock_poke(pEqualizer);

    XA_LEAVE_INTERFACE
}

static XAresult
IEqualizer_GetBandLevel(
    XAEqualizerItf self,
    XAuint16 band,
    XAmillibel *pLevel)
{
    IEqualizer *pEqualizer = (IEqualizer *)self;
    COutputMix *pCOutputMix = NULL;
    EqualizerData *pEqualizerData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pLevel && pEqualizer && pEqualizer->pData);
    pEqualizerData = (EqualizerData *)pEqualizer->pData;
    pCOutputMix = (COutputMix *)InterfaceToIObject(pEqualizer);
    AL_CHK_ARG(pCOutputMix);

    interface_lock_poke(pEqualizer);
    result = ALBackendEqualizerGetBandLevel(pCOutputMix, band, pLevel);
    interface_unlock_poke(pEqualizer);

    XA_LEAVE_INTERFACE
}

static XAresult
IEqualizer_GetCenterFreq(
    XAEqualizerItf self,
    XAuint16 band,
    XAmilliHertz *pCenter)
{
    IEqualizer *pEqualizer = (IEqualizer *)self;
    COutputMix *pCOutputMix = NULL;
    EqualizerData *pEqualizerData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCenter && pEqualizer && pEqualizer->pData);
    pEqualizerData = (EqualizerData *)pEqualizer->pData;
    pCOutputMix = (COutputMix *)InterfaceToIObject(pEqualizer);
    AL_CHK_ARG(pCOutputMix);

    interface_lock_poke(pEqualizer);
    result = ALBackendEqualizerGetCenterFreq(pCOutputMix, band, pCenter);
    interface_unlock_poke(pEqualizer);

    XA_LEAVE_INTERFACE
}

static XAresult
IEqualizer_GetBandFreqRange(
    XAEqualizerItf self,
    XAuint16 band,
    XAmilliHertz *pMin,
    XAmilliHertz *pMax)
{
    //Any one of pMin and pMax can be null as per spec
    //Do not check them for null
    IEqualizer *pEqualizer = (IEqualizer *)self;
    COutputMix *pCOutputMix = NULL;
    EqualizerData *pEqualizerData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pEqualizer && pEqualizer->pData);
    pEqualizerData = (EqualizerData *)pEqualizer->pData;
    pCOutputMix = (COutputMix *)InterfaceToIObject(pEqualizer);
    AL_CHK_ARG(pCOutputMix);

    interface_lock_poke(pEqualizer);
    result = ALBackendEqualizerGetBandFreqRange(pCOutputMix, band, pMin, pMax);
    interface_unlock_poke(pEqualizer);

    XA_LEAVE_INTERFACE
}

static XAresult
IEqualizer_GetBand(
    XAEqualizerItf self,
    XAmilliHertz frequency,
    XAuint16 *pBand)
{
    IEqualizer *pEqualizer = (IEqualizer *)self;
    COutputMix *pCOutputMix = NULL;
    EqualizerData *pEqualizerData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pBand && pEqualizer && pEqualizer->pData);
    pEqualizerData = (EqualizerData *)pEqualizer->pData;
    pCOutputMix = (COutputMix *)InterfaceToIObject(pEqualizer);
    AL_CHK_ARG(pCOutputMix);

    interface_lock_poke(pEqualizer);
    result = ALBackendEqualizerGetBand(pCOutputMix, frequency, pBand);
    interface_unlock_poke(pEqualizer);

    XA_LEAVE_INTERFACE
}

static XAresult
IEqualizer_GetCurrentPreset(
    XAEqualizerItf self,
    XAuint16 *pPreset)
{
    IEqualizer *pEqualizer = (IEqualizer *)self;
    EqualizerData *pEqualizerData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pPreset && pEqualizer && pEqualizer->pData);
    pEqualizerData = (EqualizerData *)pEqualizer->pData;

    interface_lock_poke(pEqualizer);
    *pPreset = pEqualizerData->mCurrentPreset;
    interface_unlock_poke(pEqualizer);

    XA_LEAVE_INTERFACE
}

static XAresult
IEqualizer_UsePreset(
    XAEqualizerItf self,
    XAuint16 index)
{
    IEqualizer *pEqualizer = (IEqualizer *)self;
    COutputMix *pCOutputMix = NULL;
    EqualizerData *pEqualizerData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pEqualizer && pEqualizer->pData);
    pEqualizerData = (EqualizerData *)pEqualizer->pData;
    pCOutputMix = (COutputMix *)InterfaceToIObject(pEqualizer);
    AL_CHK_ARG(pCOutputMix);

    if (pEqualizerData->mEnabled)
    {
        interface_lock_poke(pEqualizer);
        result = ALBackendEqualizerUsePreset(pCOutputMix, index);
        if (result == XA_RESULT_SUCCESS)
        {
            pEqualizerData->mCurrentPreset = index;
        }
        interface_unlock_poke(pEqualizer);
    }

    XA_LEAVE_INTERFACE
}

static XAresult
IEqualizer_GetNumberOfPresets(
    XAEqualizerItf self,
    XAuint16 *pNumPresets)
{
    IEqualizer *pEqualizer = (IEqualizer *)self;
    COutputMix *pCOutputMix = NULL;
    EqualizerData *pEqualizerData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pNumPresets && pEqualizer && pEqualizer->pData);
    pEqualizerData = (EqualizerData *)pEqualizer->pData;
    pCOutputMix = (COutputMix *)InterfaceToIObject(pEqualizer);
    AL_CHK_ARG(pCOutputMix);

    interface_lock_poke(pEqualizer);
    result = ALBackendEqualizerGetNumberOfPresets(
        pCOutputMix, pNumPresets);
    interface_unlock_poke(pEqualizer);

    XA_LEAVE_INTERFACE
}

static XAresult
IEqualizer_GetPresetName(
    XAEqualizerItf self,
    XAuint16 index,
    const XAchar **ppName)
{
    IEqualizer *pEqualizer = (IEqualizer *)self;
    COutputMix *pCOutputMix = NULL;
    EqualizerData *pEqualizerData = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pEqualizer && pEqualizer->pData);
    pEqualizerData = (EqualizerData *)pEqualizer->pData;
    pCOutputMix = (COutputMix *)InterfaceToIObject(pEqualizer);
    AL_CHK_ARG(pCOutputMix);

    interface_lock_poke(pEqualizer);
    result = ALBackendEqualizerGetPresetName(
        pCOutputMix, index, ppName);
    interface_unlock_poke(pEqualizer);

    XA_LEAVE_INTERFACE
}

static const struct XAEqualizerItf_ IEqualizer_Itf = {
    IEqualizer_SetEnabled,
    IEqualizer_IsEnabled,
    IEqualizer_GetNumberOfBands,
    IEqualizer_GetBandLevelRange,
    IEqualizer_SetBandLevel,
    IEqualizer_GetBandLevel,
    IEqualizer_GetCenterFreq,
    IEqualizer_GetBandFreqRange,
    IEqualizer_GetBand,
    IEqualizer_GetCurrentPreset,
    IEqualizer_UsePreset,
    IEqualizer_GetNumberOfPresets,
    IEqualizer_GetPresetName
};

void IEqualizer_init(void *self)
{
    EqualizerData *pEqualizerData = NULL;
    IEqualizer *pEqualizer = (IEqualizer *)self;

    XA_ENTER_INTERFACE_VOID
    if(pEqualizer)
    {
        pEqualizer->mItf = &IEqualizer_Itf;
        pEqualizerData = (EqualizerData *)malloc(sizeof(EqualizerData));
        if (pEqualizerData)
        {
            pEqualizerData->mEnabled = XA_BOOLEAN_FALSE;
            pEqualizerData->mCurrentPreset = XA_EQUALIZER_UNDEFINED;
        }
        pEqualizer->pData = (void *)pEqualizerData;
    }

    XA_LEAVE_INTERFACE_VOID
}

void IEqualizer_deinit(void *self)
{
    IEqualizer *pEqualizer = (IEqualizer *)self;

    XA_ENTER_INTERFACE_VOID
    if (pEqualizer && pEqualizer->pData)
    {
        free(pEqualizer->pData);
        pEqualizer->pData = NULL;
    }

    XA_LEAVE_INTERFACE_VOID
}
