/*
 * Copyright (c) 2011-2012, NVIDIA CORPORATION.  All Rights Reserved.
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

/* AudioIODeviceCapabilities implementation */

#include "linux_audioiodeviceinfo.h"

//***************************************************************************
// IAudioIODeviceCapabilities_GetAvailableAudioInputs
//***************************************************************************
static XAresult
IAudioIODeviceCapabilities_GetAvailableAudioInputs(
    XAAudioIODeviceCapabilitiesItf self,
    XAint32 *pNumInputs,
    XAuint32 *pInputDeviceIDs)
{
    IAudioIODeviceCapabilities *pCaps = (IAudioIODeviceCapabilities *)self;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pCaps && pNumInputs);
    interface_lock_poke(pCaps);
    result = ALBackendGetAvailableAudioInputs(
        pNumInputs,
        pInputDeviceIDs);
    interface_unlock_poke(pCaps);

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IAudioIODeviceCapabilities_QueryAudioInputCapabilities
//***************************************************************************
static XAresult
IAudioIODeviceCapabilities_QueryAudioInputCapabilities(
    XAAudioIODeviceCapabilitiesItf self,
    XAuint32 DeviceID,
    XAAudioInputDescriptor *pDescriptor)
{
    IAudioIODeviceCapabilities *pCaps = (IAudioIODeviceCapabilities *)self;
    AudioIODeviceCapabilitiesData *pData =  NULL;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pCaps && pCaps->pData && pDescriptor);
    pData = (AudioIODeviceCapabilitiesData *)pCaps->pData;
    interface_lock_poke(pCaps);
    result = ALBackendQueryAudioInputCapabilities(
        pData,
        DeviceID,
        pDescriptor);
    interface_unlock_poke(pCaps);

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IAudioIODeviceCapabilities_RegisterAvailableAudioInputsChangedCallback
//***************************************************************************
static XAresult
IAudioIODeviceCapabilities_RegisterAvailableAudioInputsChangedCallback(
    XAAudioIODeviceCapabilitiesItf self,
    xaAvailableAudioInputsChangedCallback callback,
    void *pContext)
{
    IAudioIODeviceCapabilities *pCaps = (IAudioIODeviceCapabilities *)self;
    AudioIODeviceCapabilitiesData *pData = NULL;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pCaps && pCaps->pData);
    interface_lock_exclusive(pCaps);
    pData = (AudioIODeviceCapabilitiesData *)pCaps->pData;
    pData->mAvailableAudioInputsChangedCallback = callback;
    pData->mAvailableAudioInputsChangedContext = pContext;
    interface_unlock_exclusive(pCaps);

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IAudioIODeviceCapabilities_GetAvailableAudioOutputs
//***************************************************************************
static XAresult
IAudioIODeviceCapabilities_GetAvailableAudioOutputs(
    XAAudioIODeviceCapabilitiesItf self,
    XAint32 *pNumOutputs,
    XAuint32 *pOutputDeviceIDs)
{
    IAudioIODeviceCapabilities *pCaps = (IAudioIODeviceCapabilities *)self;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pCaps && pNumOutputs);
    interface_lock_poke(pCaps);
    result = ALBackendGetAvailableAudioOutputs(
        pNumOutputs,
        pOutputDeviceIDs);
    interface_unlock_poke(pCaps);

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IAudioIODeviceCapabilities_QueryAudioOutputCapabilities
//***************************************************************************
static XAresult
IAudioIODeviceCapabilities_QueryAudioOutputCapabilities(
    XAAudioIODeviceCapabilitiesItf self,
    XAuint32 DeviceID,
    XAAudioOutputDescriptor *pDescriptor)
{
    IAudioIODeviceCapabilities *pCaps = (IAudioIODeviceCapabilities *)self;
    AudioIODeviceCapabilitiesData *pData =  NULL;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pCaps && pCaps->pData && pDescriptor);
    pData = (AudioIODeviceCapabilitiesData *)pCaps->pData;
    interface_lock_poke(pCaps);
    result = ALBackendQueryAudioOutputCapabilities(
        pData,
        DeviceID,
        pDescriptor);
    interface_unlock_poke(pCaps);

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IAudioIODeviceCapabilities_RegisterAvailableAudioOutputsChangedCallback
//***************************************************************************
static XAresult
IAudioIODeviceCapabilities_RegisterAvailableAudioOutputsChangedCallback(
    XAAudioIODeviceCapabilitiesItf self,
    xaAvailableAudioOutputsChangedCallback callback,
    void *pContext)
{
    IAudioIODeviceCapabilities *pCaps = (IAudioIODeviceCapabilities *)self;
    AudioIODeviceCapabilitiesData *pData = NULL;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pCaps && pCaps->pData);
    interface_lock_exclusive(pCaps);
    pData = (AudioIODeviceCapabilitiesData *)pCaps->pData;
    pData->mAvailableAudioOutputsChangedCallback = callback;
    pData->mAvailableAudioOutputsChangedContext = pContext;
    interface_unlock_exclusive(pCaps);

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IAudioIODeviceCapabilities_RegisterDefaultDeviceIDMapChangedCallback
//***************************************************************************
static XAresult
IAudioIODeviceCapabilities_RegisterDefaultDeviceIDMapChangedCallback(
    XAAudioIODeviceCapabilitiesItf self,
    xaDefaultDeviceIDMapChangedCallback callback,
    void *pContext)
{
    IAudioIODeviceCapabilities *pCaps = (IAudioIODeviceCapabilities *)self;
    AudioIODeviceCapabilitiesData *pData = NULL;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pCaps && pCaps->pData);
    interface_lock_exclusive(pCaps);
    pData = (AudioIODeviceCapabilitiesData *)pCaps->pData;
    pData->mDefaultDeviceIDMapChangedCallback = callback;
    pData->mDefaultDeviceIDMapChangedContext = pContext;
    interface_unlock_exclusive(pCaps);

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IAudioIODeviceCapabilities_GetAssociatedAudioInputs
//***************************************************************************
static XAresult IAudioIODeviceCapabilities_GetAssociatedAudioInputs(
    XAAudioIODeviceCapabilitiesItf self,
    XAuint32 DeviceID,
    XAint32 *pNumAudioInputs,
    XAuint32 *pAudioInputDeviceIDs)
{
    IAudioIODeviceCapabilities *pCaps = (IAudioIODeviceCapabilities *)self;
    AudioIODeviceCapabilitiesData *pData = NULL;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pCaps && pCaps->pData && pNumAudioInputs);
    pData = (AudioIODeviceCapabilitiesData *)pCaps->pData;
    interface_lock_poke(pCaps);
    result = ALBackendGetAssociatedAudioInputs(
        pData,
        DeviceID,
        pNumAudioInputs,
        pAudioInputDeviceIDs);
    interface_unlock_poke(pCaps);

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IAudioIODeviceCapabilities_GetAssociatedAudioOutputs
//***************************************************************************
static XAresult
IAudioIODeviceCapabilities_GetAssociatedAudioOutputs(
    XAAudioIODeviceCapabilitiesItf self,
    XAuint32 DeviceID,
    XAint32 *pNumAudioOutputs,
    XAuint32 *pAudioOutputDeviceIDs)
{
    IAudioIODeviceCapabilities *pCaps = (IAudioIODeviceCapabilities *)self;
    AudioIODeviceCapabilitiesData *pData = NULL;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pCaps && pCaps->pData && pNumAudioOutputs);
    pData = (AudioIODeviceCapabilitiesData *)pCaps->pData;
    interface_lock_poke(pCaps);
    result = ALBackendGetAssociatedAudioOutputs(
        pData,
        DeviceID,
        pNumAudioOutputs,
        pAudioOutputDeviceIDs);
    interface_unlock_poke(pCaps);

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IAudioIODeviceCapabilities_GetDefaultAudioDevices
//***************************************************************************
static XAresult
IAudioIODeviceCapabilities_GetDefaultAudioDevices(
    XAAudioIODeviceCapabilitiesItf self,
    XAuint32 DefaultDeviceID,
    XAint32 *pNumAudioDevices,
    XAuint32 *pAudioDeviceIDs)
{
    IAudioIODeviceCapabilities *pCaps = (IAudioIODeviceCapabilities *)self;
    AudioIODeviceCapabilitiesData *pData = NULL;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pCaps && pCaps->pData && pNumAudioDevices);
    pData = (AudioIODeviceCapabilitiesData *)pCaps->pData;
    interface_lock_poke(pCaps);
    result = ALBackendGetDefaultAudioDevices(
        pData,
        DefaultDeviceID,
        pNumAudioDevices,
        pAudioDeviceIDs);
    interface_unlock_poke(pCaps);

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IAudioIODeviceCapabilities_QuerySampleFormatsSupported
//***************************************************************************
static XAresult
IAudioIODeviceCapabilities_QuerySampleFormatsSupported(
    XAAudioIODeviceCapabilitiesItf self,
    XAuint32 DeviceID,
    XAmilliHertz samplingRate,
    XAint32 *pSampleFormats,
    XAint32 *pNumOfSampleFormats)
{
    IAudioIODeviceCapabilities *pCaps = (IAudioIODeviceCapabilities *)self;
    AudioIODeviceCapabilitiesData *pData = NULL;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pCaps && pCaps->pData && pNumOfSampleFormats);
    pData = (AudioIODeviceCapabilitiesData *)pCaps->pData;
    interface_lock_poke(pCaps);
    result = ALBackendQuerySampleFormatsSupported(
        DeviceID,
        samplingRate,
        pNumOfSampleFormats,
        pSampleFormats);
    interface_unlock_poke(pCaps);

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IAudioIODeviceCapabilities_Itf
//***************************************************************************
static const struct
XAAudioIODeviceCapabilitiesItf_ IAudioIODeviceCapabilities_Itf =
{
    IAudioIODeviceCapabilities_GetAvailableAudioInputs,
    IAudioIODeviceCapabilities_QueryAudioInputCapabilities,
    IAudioIODeviceCapabilities_RegisterAvailableAudioInputsChangedCallback,
    IAudioIODeviceCapabilities_GetAvailableAudioOutputs,
    IAudioIODeviceCapabilities_QueryAudioOutputCapabilities,
    IAudioIODeviceCapabilities_RegisterAvailableAudioOutputsChangedCallback,
    IAudioIODeviceCapabilities_RegisterDefaultDeviceIDMapChangedCallback,
    IAudioIODeviceCapabilities_GetAssociatedAudioInputs,
    IAudioIODeviceCapabilities_GetAssociatedAudioOutputs,
    IAudioIODeviceCapabilities_GetDefaultAudioDevices,
    IAudioIODeviceCapabilities_QuerySampleFormatsSupported
};

//***************************************************************************
// IAudioIODeviceCapabilities_init
//***************************************************************************
void IAudioIODeviceCapabilities_init(void *self)
{
    IAudioIODeviceCapabilities *pAudioIODeviceCaps =
        (IAudioIODeviceCapabilities *)self;
    AudioIODeviceCapabilitiesData *pData = NULL;
    XAuint32 i = 0;
    if (pAudioIODeviceCaps == NULL)
    {
       XA_LOGD("\n$$%s[%d] Assert failed\n", __FUNCTION__, __LINE__);
       return;
    }
    pAudioIODeviceCaps->mItf = &IAudioIODeviceCapabilities_Itf;
    pData = (AudioIODeviceCapabilitiesData *)malloc(
        sizeof(AudioIODeviceCapabilitiesData));
    pAudioIODeviceCaps->pData = (void *)pData;
    if (pData)
    {
        memset(pData, 0, sizeof(AudioIODeviceCapabilitiesData));
        pData->mAvailableAudioInputsChangedCallback = NULL;
        pData->mAvailableAudioInputsChangedContext = NULL;
        pData->mAvailableAudioOutputsChangedCallback = NULL;
        pData->mAvailableAudioOutputsChangedContext = NULL;
        pData->mDefaultDeviceIDMapChangedCallback = NULL;
        pData->mDefaultDeviceIDMapChangedContext = NULL;
        for (i = 0; i < MAX_AUDIO_IO_DEVICES; i++)
        {
            pData->mDefaultInputDeviceName[i] = NULL;
            pData->mDefaultOutputDeviceName[i] = NULL;
        }
        ALBackendInitializePulseAudio((void *)pAudioIODeviceCaps);
    }
}

//***************************************************************************
// IAudioIODeviceCapabilities_deinit
//***************************************************************************
void IAudioIODeviceCapabilities_deinit(void *self)
{
    IAudioIODeviceCapabilities *pAudioIODeviceCaps =
        (IAudioIODeviceCapabilities *)self;
    if (pAudioIODeviceCaps && pAudioIODeviceCaps->pData)
    {
        ALBackendDeInitializePulseAudio((void *)pAudioIODeviceCaps);
        free(pAudioIODeviceCaps->pData);
        pAudioIODeviceCaps->pData = NULL;
    }
}

