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

/* ImageEncoder implementation for linux*/

#include "linux_mediarecorder.h"

static XAresult
IImageEncoder_SetImageSettings(
    XAImageEncoderItf self,
    const XAImageSettings *pSettings)
{
    XAImageSettings *pImageSettings = NULL;
    IImageEncoder *pImageEnc = (IImageEncoder *)self;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pImageEnc && pImageEnc->pData && pSettings)
    pImageSettings = (XAImageSettings *)pImageEnc->pData;

    AL_CHK_ARG(pSettings->encoderId == XA_IMAGECODEC_JPEG)

    interface_lock_poke(pImageEnc);
    pImageSettings->encoderId = pSettings->encoderId;
    pImageSettings->width = pSettings->width;
    pImageSettings->height = pSettings->height;
    pImageSettings->compressionLevel = pSettings->compressionLevel;
    pImageSettings->colorFormat = pSettings->colorFormat;
    interface_unlock_poke(pImageEnc);

    XA_LEAVE_INTERFACE
}

static XAresult
IImageEncoder_GetImageSettings(
    XAImageEncoderItf self,
    XAImageSettings *pSettings)
{
    XAImageSettings *pImageSettings = NULL;
    IImageEncoder *pImageEnc = (IImageEncoder *)self;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pImageEnc && pImageEnc->pData && pSettings)
    pImageSettings = (XAImageSettings *)pImageEnc->pData;

    interface_lock_poke(pImageEnc);
    pSettings->encoderId = pImageSettings->encoderId;
    pSettings->width = pImageSettings->width;
    pSettings->height = pImageSettings->height;
    pSettings->compressionLevel = pImageSettings->compressionLevel;
    pSettings->colorFormat = pImageSettings->colorFormat;
    interface_unlock_poke(pImageEnc);

    XA_LEAVE_INTERFACE
}

static XAresult
IImageEncoder_GetSizeEstimate(
    XAImageEncoderItf self,
    XAuint32 *pSize)
{
    XAImageSettings *pImageSettings = NULL;
    IImageEncoder *pImageEnc = (IImageEncoder *)self;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pImageEnc && pImageEnc->pData && pSize)
    pImageSettings = (XAImageSettings *)pImageEnc->pData;

    interface_lock_poke(pImageEnc);
    *pSize = (pImageSettings->height * pImageSettings->width * 3)/2;
    interface_unlock_poke(pImageEnc);

    XA_LEAVE_INTERFACE
}

static const struct XAImageEncoderItf_ IImageEncoder_Itf =
{
    IImageEncoder_SetImageSettings,
    IImageEncoder_GetImageSettings,
    IImageEncoder_GetSizeEstimate
};

void IImageEncoder_init(void *self)
{
    XAImageSettings *pImageSettings = NULL;
    IImageEncoder *pImageEnc = (IImageEncoder *)self;

    XA_ENTER_INTERFACE_VOID
    if (pImageEnc)
    {
        pImageEnc->mItf = &IImageEncoder_Itf;
        pImageSettings = (XAImageSettings *)malloc(sizeof(XAImageSettings));
        if (pImageSettings)
        {
            pImageSettings->encoderId = XA_IMAGECODEC_JPEG;
            pImageSettings->width = 640;
            pImageSettings->height = 480;
            pImageSettings->compressionLevel = 0;
            pImageSettings->colorFormat = XA_COLORFORMAT_YUV420SEMIPLANAR;
        }
        // Store the Image Encoder data
        pImageEnc->pData = (void *)pImageSettings;
    }

    XA_LEAVE_INTERFACE_VOID
}

void IImageEncoder_deinit(void *self)
{
    IImageEncoder *pImageEnc = (IImageEncoder *)self;

    XA_ENTER_INTERFACE_VOID
    if (pImageEnc && pImageEnc->pData)
    {
        free(pImageEnc->pData);
        pImageEnc->pData = NULL;
    }

    XA_LEAVE_INTERFACE_VOID
}
