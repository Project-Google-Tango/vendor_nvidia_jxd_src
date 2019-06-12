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

/* ImageDecoderCapabilities implementation */

#include "linux_common.h"

typedef struct ImageDecoderDescriptor_
{
    XAuint32 numImageDecoders;
    XAuint32 numColorFormats;
    XAImageCodecDescriptor *pDescriptor;
}ImageDecoderDescriptor;

//***************************************************************************
// IImageDecoderCapabilities_GetImageDecoderCapabilities
//***************************************************************************
static XAresult
IImageDecoderCapabilities_GetImageDecoderCapabilities(
    XAImageDecoderCapabilitiesItf self,
    XAuint32 *pDecoderId,
    XAImageCodecDescriptor *pDescriptor)
{
    IImageDecoderCapabilities *pCaps = (IImageDecoderCapabilities*)self;
    ImageDecoderDescriptor *pImageDecCaps;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCaps && pCaps->pData && pDecoderId)

    pImageDecCaps = (ImageDecoderDescriptor *)pCaps->pData;
    if (NULL == pDescriptor)
    {
        *pDecoderId = pImageDecCaps->numImageDecoders;
    }
    else
    {
        AL_CHK_ARG(*pDecoderId <= pImageDecCaps->numImageDecoders)

        memcpy(
            pDescriptor,
            &pImageDecCaps->pDescriptor[*pDecoderId],
            sizeof(XAImageCodecDescriptor));
    }

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IImageDecoderCapabilities_QueryColorFormats
//***************************************************************************
static XAresult
IImageDecoderCapabilities_QueryColorFormats(
    const XAImageDecoderCapabilitiesItf self,
    XAuint32 *pIndex,
    XAuint32 *pColorFormat)
{
    IImageDecoderCapabilities *pCaps = (IImageDecoderCapabilities *)self;
    ImageDecoderDescriptor *pImageDecCaps = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCaps && pCaps->pData && pIndex)

    pImageDecCaps = (ImageDecoderDescriptor *)pCaps->pData;

    // Return the number of color formats supported when pColorFormat is NULL
    if (NULL == pColorFormat)
    {
        *pIndex = pImageDecCaps->numColorFormats;
    }
    else
    {
        AL_CHK_ARG(*pIndex <= pImageDecCaps->numColorFormats)
    }

    XA_LEAVE_INTERFACE
}

//***************************************************************************
// IImageDecoderCapabilities_Itf
//***************************************************************************
static const
struct XAImageDecoderCapabilitiesItf_ IImageDecoderCapabilities_Itf =
{
    IImageDecoderCapabilities_GetImageDecoderCapabilities,
    IImageDecoderCapabilities_QueryColorFormats
};

//***************************************************************************
// IImageDecoderCapabilities_Itf
//***************************************************************************
void IImageDecoderCapabilities_init(void *self)
{
    ImageDecoderDescriptor *pImageDescriptor = NULL;
    XAImageCodecDescriptor *pImageCodecDescriptor = NULL;
    IImageDecoderCapabilities *pCaps = (IImageDecoderCapabilities *)self;

    XA_ENTER_INTERFACE_VOID
    if (pCaps)
    {
        pCaps->mItf = &IImageDecoderCapabilities_Itf;
        pCaps->pData = NULL;

        pImageDescriptor = (ImageDecoderDescriptor *)malloc(
            sizeof(ImageDecoderDescriptor));
        if (pImageDescriptor)
        {
            memset(pImageDescriptor, 0, sizeof(ImageDecoderDescriptor));
            // We support one decoder formats : JPEG
            pImageDescriptor->numImageDecoders = 1;
            pImageDescriptor->numColorFormats = 0;
            pImageCodecDescriptor = (XAImageCodecDescriptor*)malloc(
                pImageDescriptor->numImageDecoders * sizeof(XAImageCodecDescriptor));
            if (pImageCodecDescriptor)
            {
                pImageDescriptor->pDescriptor = pImageCodecDescriptor;
                memset(&pImageCodecDescriptor[0], 0, sizeof(XAImageCodecDescriptor));
                pImageDescriptor->pDescriptor[0].codecId = XA_IMAGECODEC_JPEG;
                pImageDescriptor->pDescriptor[0].maxWidth = 1600;
                pImageDescriptor->pDescriptor[0].maxHeight = 1200;
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
void IImageDecoderCapabilities_deinit(void *self)
{
    IImageDecoderCapabilities *pCaps = (IImageDecoderCapabilities *)self;

    XA_ENTER_INTERFACE_VOID
    if (pCaps && pCaps->pData)
    {
        ImageDecoderDescriptor *pImageDescriptor =
            (ImageDecoderDescriptor *)pCaps->pData;
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
