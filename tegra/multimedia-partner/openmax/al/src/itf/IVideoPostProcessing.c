/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "linux_mediaplayer.h"

static XAresult
IVideoPostProcessing_SetRotation(
    XAVideoPostProcessingItf self,
    XAmillidegree rotation)
{
    VideoPostProcessingData *pData = NULL;
    IVideoPostProcessing *pVideoPostProc = (IVideoPostProcessing *)self;

    XA_ENTER_INTERFACE

    if (rotation%90000)
    {
        return XA_RESULT_FEATURE_UNSUPPORTED;
    }

    AL_CHK_ARG(pVideoPostProc && pVideoPostProc->pData);
    pData = (VideoPostProcessingData *)pVideoPostProc->pData;

    interface_lock_poke(pVideoPostProc);
    pData->rotateAngle = rotation;
    interface_unlock_poke(pVideoPostProc);

   XA_LEAVE_INTERFACE
}

static XAresult
IVideoPostProcessing_IsArbitraryRotationSupported(
    XAVideoPostProcessingItf self,
    XAboolean *pSupported)
{
    return XA_RESULT_FEATURE_UNSUPPORTED;
}

static XAresult
IVideoPostProcessing_SetScaleOptions(
    XAVideoPostProcessingItf self,
    XAuint32 scaleOptions,
    XAuint32 backgroundColor,
    XAuint32 renderingHints)
{
    VideoPostProcessingData *pData = NULL;
    IVideoPostProcessing *pVideoPostProc = (IVideoPostProcessing *)self;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pVideoPostProc && pVideoPostProc->pData);
    pData = (VideoPostProcessingData *)pVideoPostProc->pData;

    interface_lock_poke(pVideoPostProc);
    pData->scaleOptions = scaleOptions;
    pData->backgroundColor = backgroundColor;
    pData->renderingHints = renderingHints;
    interface_unlock_poke(pVideoPostProc);

   XA_LEAVE_INTERFACE
}

static XAresult
IVideoPostProcessing_SetSourceRectangle(
    XAVideoPostProcessingItf self,
    const XARectangle *pSrcRect)
{
    VideoPostProcessingData *pData = NULL;
    XARectangle *SrcRect = NULL;
    IVideoPostProcessing *pVideoPostProc = (IVideoPostProcessing *)self;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pVideoPostProc && pVideoPostProc->pData);
    pData = (VideoPostProcessingData *)pVideoPostProc->pData;

    SrcRect= (XARectangle *)malloc(sizeof(XARectangle));
    if (SrcRect == NULL)
        return XA_RESULT_MEMORY_FAILURE;
    memcpy(SrcRect, pSrcRect, sizeof(XARectangle));

    interface_lock_poke(pVideoPostProc);
    pData->pSrcRect = SrcRect;
    interface_unlock_poke(pVideoPostProc);

   XA_LEAVE_INTERFACE
}

static XAresult
IVideoPostProcessing_SetDestinationRectangle(
    XAVideoPostProcessingItf self,
    const XARectangle *pDestRect)
{
    VideoPostProcessingData *pData = NULL;
    XARectangle *DestRect = NULL;
    IVideoPostProcessing *pVideoPostProc = (IVideoPostProcessing *)self;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pVideoPostProc && pVideoPostProc->pData);
    pData = (VideoPostProcessingData *)pVideoPostProc->pData;

    DestRect = (XARectangle *)malloc(sizeof(XARectangle));
    if (DestRect == NULL)
        return XA_RESULT_MEMORY_FAILURE;
    memcpy(DestRect, pDestRect, sizeof(XARectangle));

    interface_lock_poke(pVideoPostProc);
    pData->pDestRect = DestRect;
    interface_unlock_poke(pVideoPostProc);

   XA_LEAVE_INTERFACE
}

static XAresult
IVideoPostProcessing_SetMirror(
    XAVideoPostProcessingItf self,
    XAuint32 mirror)
{
    VideoPostProcessingData *pData = NULL;
    IVideoPostProcessing *pVideoPostProc = (IVideoPostProcessing *)self;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pVideoPostProc && pVideoPostProc->pData);
    pData = (VideoPostProcessingData *)pVideoPostProc->pData;

    interface_lock_poke(pVideoPostProc);
    pData->mirror = mirror;
    interface_unlock_poke(pVideoPostProc);

   XA_LEAVE_INTERFACE
}

static XAresult IVideoPostProcessing_Commit(XAVideoPostProcessingItf self)
{
    VideoPostProcessingData *pData = NULL;
    CMediaPlayer* pCMediaPlayer = NULL;
    IVideoPostProcessing *pVideoPostProc = (IVideoPostProcessing *)self;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pVideoPostProc && pVideoPostProc->pData);
    pData = (VideoPostProcessingData *)pVideoPostProc->pData;

    pCMediaPlayer = (CMediaPlayer*)InterfaceToIObject(pVideoPostProc);
    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData);

    interface_lock_poke(pVideoPostProc);
    ALBackendVideoPostProcessingCommit(pCMediaPlayer);
    interface_unlock_poke(pVideoPostProc);
    XA_LEAVE_INTERFACE
}

static const struct XAVideoPostProcessingItf_ IVideoPostProcessing_Itf = {
    IVideoPostProcessing_SetRotation,
    IVideoPostProcessing_IsArbitraryRotationSupported,
    IVideoPostProcessing_SetScaleOptions,
    IVideoPostProcessing_SetSourceRectangle,
    IVideoPostProcessing_SetDestinationRectangle,
    IVideoPostProcessing_SetMirror,
    IVideoPostProcessing_Commit
};

void IVideoPostProcessing_init(void *self)
{
    VideoPostProcessingData *pVideoPostProcessingData = NULL;
    IVideoPostProcessing *pVideoPostProc = (IVideoPostProcessing *)self;
    if (pVideoPostProc)
    {
        pVideoPostProc->mItf = &IVideoPostProcessing_Itf;
        pVideoPostProcessingData =
            (VideoPostProcessingData *)malloc(sizeof(VideoPostProcessingData));

        if (pVideoPostProcessingData)
        {
            memset(pVideoPostProcessingData, 0, sizeof(VideoPostProcessingData));
            pVideoPostProcessingData->mirror = 0;
            pVideoPostProcessingData->rotateAngle = 0;
            pVideoPostProcessingData->backgroundColor = 0;
            pVideoPostProcessingData->pDestRect = NULL;
            pVideoPostProcessingData->pSrcRect = NULL;
            pVideoPostProcessingData->scaleOptions = XA_VIDEOSCALE_FIT;
            pVideoPostProcessingData->renderingHints = XA_RENDERINGHINT_NONE;
            pVideoPostProc->pData = (void *)pVideoPostProcessingData;
        }
    }
}
