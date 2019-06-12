/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All Rights Reserved.
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

/* OutputMix implementation */

#include "linux_outputmix.h"
#include "linux_audioiodeviceinfo.h"

static XAresult
IOutputMix_GetDestinationOutputDeviceIDs(
    XAOutputMixItf self,
    XAint32 *pNumDevices,
    XAuint32 *pDeviceIDs)
{
    IOutputMix *pIOutputMIx = (IOutputMix *)self;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pIOutputMIx && pNumDevices);
    interface_lock_poke(pIOutputMIx);
    result = ALBackendGetAvailableAudioOutputs(
        pNumDevices,
        pDeviceIDs);
    interface_unlock_poke(pIOutputMIx);

    XA_LEAVE_INTERFACE
}


static XAresult
IOutputMix_RegisterDeviceChangeCallback(
    XAOutputMixItf self,
    xaMixDeviceChangeCallback callback,
    void *pContext)
{
    IOutputMix *pIOutputMix = (IOutputMix *)self;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pIOutputMix);
    interface_lock_exclusive(pIOutputMix);
    ALBackendOutmixRegisterDeviceChangeCallback(
        pIOutputMix,
        callback,
        pContext);
    interface_unlock_exclusive(pIOutputMix);

    XA_LEAVE_INTERFACE
}

static XAresult
IOutputMix_ReRoute(
    XAOutputMixItf self,
    XAint32 numOutputDevices,
    XAuint32 *pOutputDeviceIDs)
{
    IOutputMix *pIOutputMix = (IOutputMix *)self;

    XA_ENTER_INTERFACE

    AL_CHK_ARG(pIOutputMix && pOutputDeviceIDs);
    if (1 != numOutputDevices)
    {
        return XA_RESULT_FEATURE_UNSUPPORTED;
    }
    interface_lock_poke(pIOutputMix);
    result = ALBackendReRoute(
        numOutputDevices,
        pOutputDeviceIDs);
    interface_unlock_poke(pIOutputMix);

    XA_LEAVE_INTERFACE
}

static const struct XAOutputMixItf_ IOutputMix_Itf =
{
    IOutputMix_GetDestinationOutputDeviceIDs,
    IOutputMix_RegisterDeviceChangeCallback,
    IOutputMix_ReRoute
};

void IOutputMix_init(void *self)
{
    IOutputMix *pIOutputMix = (IOutputMix *)self;

    if (pIOutputMix)
    {
        pIOutputMix->mItf = &IOutputMix_Itf;
        ALBackendOutputMixInitialize((void *)pIOutputMix);
    }
}

void IOutputMix_deinit(void *self)
{
    IOutputMix *pIOutputMix = (IOutputMix *)self;

    if (pIOutputMix)
    {
        pIOutputMix->mItf = &IOutputMix_Itf;
        ALBackendOutputMixDeInitialize((void *)pIOutputMix);
    }
}

