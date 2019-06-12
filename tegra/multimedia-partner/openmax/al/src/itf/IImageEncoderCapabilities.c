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

/* ImageEncoderCapabilities implementation */

#include "linux_common.h"

typedef struct ImageEncoderDescriptor_
{
    XAuint32 numImageEncoders;
    XAuint32 numColorFormats;
    XAImageCodecDescriptor *pDescriptor;
}ImageEncoderDescriptor;

//***************************************************************************
// IImageEncoderCapabilities_GetImageEncoderCapabilities
//***************************************************************************
static XAresult
IImageEncoderCapabilities_GetImageEncoderCapabilities(
    XAImageEncoderCapabilitiesItf self,
    XAuint32 *pEncoderId,
    XAImageCodecDescriptor *pDescriptor)
{
    IImageEncoderCapabilities *pCaps = (IImageEncoderCapabilities*)self;
    ImageEncoderDescriptor *pImageEncCaps;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCaps && pCaps->pData && pEncoderId)

    pImageEncCaps = (ImageEncoderDescriptor *)pCaps->pData;
    if (NULL == pDescriptor)
    {
        *pEncoderId = pImageEncCaps->numImageEncoders;
    }
    else
    {

        AL_CHK_ARG(*pEncoderId <= pImageEncCaps->numImageEncoders)

        memcpy(
            pDescriptor,
            &pImageEncCaps->pDescriptor[*pEncoderId],
            sizeof(XAImageCodecDescriptor));
    }

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IImageEncoderCapabilities_QueryColorFormats
//***************************************************************************
static XAresult
IImageEncoderCapabilities_QueryColorFormats(
    const XAImageEncoderCapabilitiesItf self,
    XAuint32 *pIndex,
    XAuint32 *pColorFormat)
{
    IImageEncoderCapabilities *pCaps = (IImageEncoderCapabilities *)self;
    ImageEncoderDescriptor *pImageEncCaps = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCaps && pCaps->pData && pIndex)

    pImageEncCaps = (ImageEncoderDescriptor *)pCaps->pData;

    // Return the number of color formats supported when pColorFormat is NULL
    if (NULL == pColorFormat)
    {
        *pIndex = pImageEncCaps->numColorFormats;
    }
    else
    {
        AL_CHK_ARG((*pIndex) <= pImageEncCaps->numColorFormats)
    }

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IImageEncoderCapabilities_Itf
//***************************************************************************
static const
struct XAImageEncoderCapabilitiesItf_ IImageEncoderCapabilities_Itf =
{
    IImageEncoderCapabilities_GetImageEncoderCapabilities,
    IImageEncoderCapabilities_QueryColorFormats
};

//***************************************************************************
// IImageEncoderCapabilities_Itf
//***************************************************************************
void IImageEncoderCapabilities_init(void *self)
{
    ImageEncoderDescriptor *pImageDescriptor = NULL;
    XAImageCodecDescriptor *pImageCodecDescriptor = NULL;
    IImageEncoderCapabilities *pCaps = (IImageEncoderCapabilities *)self;

    XA_ENTER_INTERFACE_VOID
    if (pCaps)
    {
        pCaps->mItf = &IImageEncoderCapabilities_Itf;
        pCaps->pData = NULL;

        pImageDescriptor = (ImageEncoderDescriptor *)malloc(
            sizeof(ImageEncoderDescriptor));
        if (pImageDescriptor)
        {
            memset(pImageDescriptor, 0, sizeof(ImageEncoderDescriptor));
            // We support only JPEG encoder
            pImageDescriptor->numImageEncoders = 1;
            pImageDescriptor->numColorFormats = 0;
            pImageCodecDescriptor = (XAImageCodecDescriptor *)malloc(
                pImageDescriptor->numImageEncoders * sizeof(XAImageCodecDescriptor));
            if (pImageCodecDescriptor)
            {
                pImageDescriptor->pDescriptor = pImageCodecDescriptor;
                memset(&pImageCodecDescriptor[0], 0, sizeof(XAImageCodecDescriptor));
                pImageDescriptor->pDescriptor[0].codecId = XA_IMAGECODEC_JPEG;
                pImageDescriptor->pDescriptor[0].maxWidth = 3264;
                pImageDescriptor->pDescriptor[0].maxHeight = 2448;
            }
            else
            {
                free(pImageDescriptor);
                pImageDescriptor = NULL;
            }
            pCaps->pData = (void *)pImageDescriptor;
        }
    }

    XA_LEAVE_INTERFACE_VOID
}

//***************************************************************************
// IImageEncoderCapabilities_deinit
//***************************************************************************
void IImageEncoderCapabilities_deinit(void *self)
{
    IImageEncoderCapabilities *pCaps = (IImageEncoderCapabilities *)self;

    XA_ENTER_INTERFACE_VOID
    if (pCaps && pCaps->pData)
    {
        ImageEncoderDescriptor *pImageDescriptor =
            (ImageEncoderDescriptor *)pCaps->pData;
        if (pImageDescriptor->pDescriptor)
        {
            free(pImageDescriptor->pDescriptor);
            pImageDescriptor->pDescriptor = NULL;
        }
        free(pCaps->pData);
        pCaps->pData = NULL;
    }

    XA_LEAVE_INTERFACE_VOID
}
