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

/* ImageEffects implementation */

#include "linux_common.h"

static XAresult
IImageEffects_QuerySupportedImageEffects(
    XAImageEffectsItf self,
    XAuint32 index,
    XAuint32 *pImageEffectId)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static XAresult
IImageEffects_EnableImageEffect(
    XAImageEffectsItf self,
    XAuint32 imageEffectID)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static XAresult
IImageEffects_DisableImageEffect(
    XAImageEffectsItf self,
    XAuint32 imageEffectID)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static XAresult
IImageEffects_IsImageEffectEnabled(
    XAImageEffectsItf self,
    XAuint32 imageEffectID,
    XAboolean *pEnabled)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static const struct XAImageEffectsItf_ IImageEffects_Itf = {
    IImageEffects_QuerySupportedImageEffects,
    IImageEffects_EnableImageEffect,
    IImageEffects_DisableImageEffect,
    IImageEffects_IsImageEffectEnabled
};

void IImageEffects_init(void *self)
{
    ImageEffectsData *pData = NULL;

    IImageEffects *pImageEffects = (IImageEffects*)self;

    XA_ENTER_INTERFACE_VOID
    if (pImageEffects)
    {
        pImageEffects->mItf = &IImageEffects_Itf;
        pData = (ImageEffectsData *)malloc(sizeof(ImageEffectsData));
        if (pData)
        {
            // No support for any image effects as of now
            pData->nNumOfEffects = 0;
        }
        // Store the Image Effects Data
        pImageEffects->pData = (void *)pData;
    }

    XA_LEAVE_INTERFACE_VOID
}

void IImageEffects_deinit(void *self)
{
    IImageEffects *pImageEffects = (IImageEffects *)self;

    XA_ENTER_INTERFACE_VOID
    if (pImageEffects && pImageEffects->pData)
    {
        free(pImageEffects->pData);
        pImageEffects->pData = NULL;
    }

    XA_LEAVE_INTERFACE_VOID
}
