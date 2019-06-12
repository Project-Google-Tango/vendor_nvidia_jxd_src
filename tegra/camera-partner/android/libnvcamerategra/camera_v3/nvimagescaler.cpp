/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvimagescaler.h"
#include "nv_log.h"
#include "camera_trace.h"

#define MAX_DS_RATIO_DEFAULT (2.0f)

namespace android {


// allocate necessary resources
NvImageScaler::NvImageScaler()
{
    NV_TRACE_CALL_D(HAL3_IMAGE_SCALER_TAG);
    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: ++", __FUNCTION__);
    m_hRm = NULL;
    m_h2d = NULL;

    m_InitializeError = NvRmOpen(&m_hRm, 0);
    if (m_InitializeError != NvSuccess)
    {
        NV_LOGE("%s: failed to create Rm handle! [%d]\n", __FUNCTION__,
            m_InitializeError);
        goto fail;
    }

    m_InitializeError = NvDdk2dOpen(m_hRm, NULL , &m_h2d);
    if (m_InitializeError != NvSuccess)
    {
        NV_LOGE("%s: failed to create 2d ddk handle! [%d]\n",
            __FUNCTION__, m_InitializeError);
        goto fail;
    }

    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: --", __FUNCTION__);
    return;

fail:
    NV_LOGE("%s: --", __FUNCTION__);
    Release();
}

// destroy all allocated resources
void NvImageScaler::Release()
{
    NV_TRACE_CALL_D(HAL3_IMAGE_SCALER_TAG);
    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: ++", __FUNCTION__);
    NvRmClose(m_hRm);
    m_hRm = NULL;
    NvDdk2dClose(m_h2d);
    m_h2d = NULL;
    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: --", __FUNCTION__);
}

// destructor to destroy all allocaed resources
NvImageScaler::~NvImageScaler()
{
    NV_TRACE_CALL_D(HAL3_IMAGE_SCALER_TAG);
    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: ++", __FUNCTION__);
    Release();
    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: --", __FUNCTION__);
}


// originally derived from nvomxcameraencoderqueue method of the same name
// adapted to remove OMX dependencies
NvError
NvImageScaler::AllocateYuv420NvmmSurface(
    NvMMSurfaceDescriptor *pSurface,
    NvU32 Width,
    NvU32 Height,
    NVMM_FRAME_FORMAT format)
{
    NV_TRACE_CALL_D(HAL3_IMAGE_SCALER_TAG);
    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: ++", __FUNCTION__);
    NvS32 i = 0;
    NvU32 Size = 0, Alignment = 0;
    NvError e = NvSuccess;
    NvS32 SurfaceCount = 0;

    static const NvRmHeap Heaps[] =
    {
      NvRmHeap_ExternalCarveOut,
      NvRmHeap_External,
    };
    NvRmDeviceHandle hRm = NULL;

    if (!pSurface){
        NV_LOGE("%s: --", __FUNCTION__);
        return NvError_BadParameter;
    }

    NV_CHECK_ERROR_CLEANUP(
        NvRmOpen(&hRm, 0)
    );

    NvOsMemset(pSurface, 0, sizeof(NvMMSurfaceDescriptor));

    if ((format == NVMM_PREVIEW_FRAME_FormatNV21) ||
             (format == NVMM_VIDEO_FRAME_FormatNV21))
    {
        pSurface->Surfaces[0].Width = Width;
        pSurface->Surfaces[0].Height = Height;
        pSurface->Surfaces[0].ColorFormat = NvColorFormat_Y8;
        pSurface->Surfaces[1].Width = Width / 2;
        pSurface->Surfaces[1].Height = Height;
        pSurface->Surfaces[1].ColorFormat = NvColorFormat_U8_V8;
        SurfaceCount = 2;
    }
    else
    {
        pSurface->Surfaces[0].Width = Width;
        pSurface->Surfaces[0].Height = Height;
        pSurface->Surfaces[0].ColorFormat = NvColorFormat_Y8;
        pSurface->Surfaces[1].Width = Width / 2;
        pSurface->Surfaces[1].Height = Height / 2;
        pSurface->Surfaces[1].ColorFormat = NvColorFormat_U8;
        pSurface->Surfaces[2].Width = pSurface->Surfaces[1].Width;
        pSurface->Surfaces[2].Height = pSurface->Surfaces[1].Height;
        pSurface->Surfaces[2].ColorFormat = NvColorFormat_V8;
        SurfaceCount = 3;
    }

    for (i = 0; i < SurfaceCount; i++)
    {
        pSurface->Surfaces[i].Layout = NvRmSurfaceLayout_Pitch;
        NvRmSurfaceComputePitch(NULL,0,&pSurface->Surfaces[i]);

        Size = NvRmSurfaceComputeSize(&pSurface->Surfaces[i]);
        Alignment = NvRmSurfaceComputeAlignment(hRm,
            &pSurface->Surfaces[i]);

        NV_CHECK_ERROR_CLEANUP(
            NvRmMemHandleAlloc(hRm, Heaps, NV_ARRAY_SIZE(Heaps),
                Alignment, NvOsMemAttribute_WriteCombined, Size,
                0, 0, &pSurface->Surfaces[i].hMem));
        pSurface->PhysicalAddress[i] =
            NvRmMemPin(pSurface->Surfaces[i].hMem);
        pSurface->SurfaceCount = i + 1;
    }

    NvOsMemcpy(&pSurface->DispRes, &pSurface->DispRes,
        sizeof(NvMMDisplayResolution));
    NvOsMemcpy(&pSurface->CropRect, &pSurface->CropRect,
        sizeof(NvRect));

    NvRmClose(hRm);
    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: --", __FUNCTION__);
    return e;

fail:
    NvRmClose(hRm);
    FreeSurface(pSurface);
    NV_LOGE("%s: --", __FUNCTION__);
    return NvError_InsufficientMemory;
}

NvError
NvImageScaler::AllocateNV12NvmmSurface(
    NvMMSurfaceDescriptor *pSurface,
    NvU32 Width,
    NvU32 Height)
{
    NV_TRACE_CALL_D(HAL3_IMAGE_SCALER_TAG);
    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: ++", __FUNCTION__);
    NvS32 i = 0;
    NvU32 Size = 0, Alignment = 0;
    NvError e = NvSuccess;
    static const NvRmHeap Heaps[] =
    {
      NvRmHeap_ExternalCarveOut,
      NvRmHeap_External,
    };
    NvRmDeviceHandle hRm = NULL;

    if (!pSurface){
        NV_LOGE("%s: --", __FUNCTION__);
        return NvError_BadParameter;
    }

    NV_CHECK_ERROR_CLEANUP(
        NvRmOpen(&hRm, 0)
    );

    NvOsMemset(pSurface, 0, sizeof(NvMMSurfaceDescriptor));

    pSurface->Surfaces[0].Width = Width;
    pSurface->Surfaces[0].Height = Height;
    pSurface->Surfaces[0].ColorFormat = NvColorFormat_Y8;
    pSurface->Surfaces[1].Width = Width / 2;
    pSurface->Surfaces[1].Height = Height / 2;
    pSurface->Surfaces[1].ColorFormat = NvColorFormat_U8_V8;

    for (i = 0; i < 2; i++)
    {
        pSurface->Surfaces[i].Layout = NvRmSurfaceLayout_Blocklinear;
        pSurface->Surfaces[i].Kind = NvRmMemKind_Generic_16Bx2;
        pSurface->Surfaces[i].BlockHeightLog2 = 2;
        NvRmSurfaceComputePitch(NULL,0,&pSurface->Surfaces[i]);

        Size = NvRmSurfaceComputeSize(&pSurface->Surfaces[i]);
        Alignment = NvRmSurfaceComputeAlignment(hRm,
            &pSurface->Surfaces[i]);
        // create and allocate new nvrm memory handle for the new surface.
        NV_CHECK_ERROR_CLEANUP(
            NvRmMemHandleAlloc(hRm, Heaps,
                NV_ARRAY_SIZE(Heaps), Alignment, NvOsMemAttribute_WriteCombined,
                Size, 0, 0, &pSurface->Surfaces[i].hMem));

        pSurface->PhysicalAddress[i] =
            NvRmMemPin(pSurface->Surfaces[i].hMem);
        pSurface->SurfaceCount = i + 1;
    }

    NvOsMemcpy(&pSurface->DispRes, &pSurface->DispRes,
        sizeof(NvMMDisplayResolution));
    NvOsMemcpy(&pSurface->CropRect, &pSurface->CropRect,
        sizeof(NvRect));

    NvRmClose(hRm);
    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: --", __FUNCTION__);
    return e;

fail:
    NvRmClose(hRm);
    FreeSurface(pSurface);
    NV_LOGE("%s: --", __FUNCTION__);
    return NvError_InsufficientMemory;
}

// originally derived from nvomxcameraencoderqueue method of the same name
void
NvImageScaler::FreeSurface(
    NvMMSurfaceDescriptor *pSurface)
{
    NV_TRACE_CALL_D(HAL3_IMAGE_SCALER_TAG);
    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: ++", __FUNCTION__);
    NvS32 i = 0;

    for (i = 0; i < pSurface->SurfaceCount; i++)
    {
        NvRmMemUnpin(pSurface->Surfaces[i].hMem);
        NvRmMemHandleFree(pSurface->Surfaces[i].hMem);
    }

    NvOsMemset(pSurface, 0, sizeof(NvMMSurfaceDescriptor));
    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: --", __FUNCTION__);
}

// query the 2d surface type based on the number of surfaces
NvError
NvImageScaler::Get2dSurfaceType(
    NvU32 NumOfSurfaces,
    NvDdk2dSurfaceType *pDdk2dSurfaceType)
{
    NV_TRACE_CALL_D(HAL3_IMAGE_SCALER_TAG);
    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: ++", __FUNCTION__);
    switch(NumOfSurfaces)
    {
        case 3:
            *pDdk2dSurfaceType = NvDdk2dSurfaceType_Y_U_V;
            break;

        case 2:
            *pDdk2dSurfaceType = NvDdk2dSurfaceType_Y_UV;
            break;

        case 1:
            *pDdk2dSurfaceType = NvDdk2dSurfaceType_Single;
            break;

        default:
            NV_LOGE("%s: --", __FUNCTION__);
            return NvError_BadParameter;
            break;
    }
    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: --", __FUNCTION__);
    return NvSuccess;
}

// scale the whole frame in a source surface to the whole frame in a
// destination surface
NvError NvImageScaler::Scale(
    NvMMSurfaceDescriptor *pInput,
    NvMMSurfaceDescriptor *pOutput)
{
    NV_TRACE_CALL_D(HAL3_IMAGE_SCALER_TAG);
    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: ++", __FUNCTION__);
    NvDdk2dFixedRect SrcRect;
    NvRect DestRect;

    // If the constructor failed to allocate the resources
    // return the error
    if (m_InitializeError != NvSuccess){
        NV_LOGE("%s: --", __FUNCTION__);
        return m_InitializeError;
    }

    // set up the dest rectangle to cover the whole output surface
    NvOsMemset(&DestRect, 0, sizeof(DestRect));
    DestRect.left = 0;
    DestRect.right = pOutput->Surfaces[0].Width;
    DestRect.top = 0;
    DestRect.bottom = pOutput->Surfaces[0].Height;

    // set up the dest rectangle to cover the whole input surface
    NvOsMemset(&SrcRect, 0, sizeof(SrcRect));
    SrcRect.left = NV_SFX_ZERO;
    SrcRect.right = NV_SFX_WHOLE_TO_FX(pInput->Surfaces[0].Width);
    SrcRect.top = NV_SFX_ZERO;
    SrcRect.bottom = NV_SFX_WHOLE_TO_FX(pInput->Surfaces[0].Height);

    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: --", __FUNCTION__);
    return CropAndScale(pInput, &SrcRect, pOutput, &DestRect);
}

// scale the cropped frame in a source surface to the cropped frame in
// a destination surface
NvError NvImageScaler::CropAndScale(
    NvMMSurfaceDescriptor *pInput,
    NvDdk2dFixedRect *pSrcRect,
    NvMMSurfaceDescriptor *pOutput,
    NvRect *pDestRect)
{
    NV_TRACE_CALL_D(HAL3_IMAGE_SCALER_TAG);
    return CropAndScale(&pInput->Surfaces[0], pInput->SurfaceCount,
        pSrcRect, &pOutput->Surfaces[0], pOutput->SurfaceCount, pDestRect,0);
}

// scale the cropped frame in a source NvRmSurface to the cropped frame in
// a destination NvRmSurface
NvError NvImageScaler::CropAndScale(
    NvRmSurface *pInput,
    NvU32 NumInputSurface,
    NvDdk2dFixedRect *pSrcRect,
    NvRmSurface *pOutput,
    NvU32 NumOutputSurface,
    NvRect *pDestRect,
    NvU32 imageRotation)
{
    NV_TRACE_CALL_D(HAL3_IMAGE_SCALER_TAG);
    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: ++", __FUNCTION__);
    NvDdk2dSurface *pOutput2dSurface = NULL;
    NvDdk2dSurface *pInput2dSurface = NULL;
    NvDdk2dSurfaceType SurfaceType;
    NvDdk2dBlitParameters Params;
    NvError Err = NvSuccess;
    NvDdk2dTransform transform;

    switch(imageRotation)
    {
       case  90 : transform = NvDdk2dTransform_Rotate270; break;
       case 180 : transform = NvDdk2dTransform_Rotate180; break;
       case 270 : transform = NvDdk2dTransform_Rotate90;  break;
       default : if(imageRotation!=0){
                    NV_LOGE("imageRotation Angle not a multiple of 90.");
                    NV_LOGE(" So setting rotation to 0\n"); }
                 transform = NvDdk2dTransform_None;
       break;
    }

    if (!pInput || !pOutput || !pSrcRect || !pDestRect){
        NV_LOGE("%s: --", __FUNCTION__);
        return NvError_BadParameter;
    }

    if (m_InitializeError != NvSuccess){
        NV_LOGE("%s: --", __FUNCTION__);
        return m_InitializeError;
    }

    // create input 2d surface
    Err = Get2dSurfaceType(NumInputSurface, &SurfaceType);
    if (Err != NvSuccess)
        goto cleanup;
    Err = NvDdk2dSurfaceCreate(m_h2d, SurfaceType, &pInput[0],
        &pInput2dSurface);
    if (Err != NvSuccess)
        goto cleanup;

    // create output 2d surface
    Err = Get2dSurfaceType(NumOutputSurface, &SurfaceType);
    if (Err != NvSuccess)
        goto cleanup;
    Err = NvDdk2dSurfaceCreate(m_h2d, SurfaceType, &pOutput[0],
        &pOutput2dSurface);
    if (Err != NvSuccess)
        goto cleanup;

    NvOsMemset(&Params, 0, sizeof(Params));

    NvDdk2dSetBlitFilter(&Params, NvDdk2dStretchFilter_Smart);
    NvDdk2dSetBlitTransform(&Params, transform);
    Err = NvDdk2dBlit(m_h2d, pOutput2dSurface, pDestRect, pInput2dSurface,
        pSrcRect, &Params);
    if (Err != NvSuccess)
        goto cleanup;

    NvDdk2dSurfaceLock(pOutput2dSurface, NvDdk2dSurfaceAccessMode_ReadWrite, NULL,
        NULL, NULL);
    NvDdk2dSurfaceUnlock(pOutput2dSurface, NULL, 0);

cleanup:

    if (pOutput2dSurface)
        NvDdk2dSurfaceDestroy(pOutput2dSurface);
    if (pInput2dSurface)
        NvDdk2dSurfaceDestroy(pInput2dSurface);

    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: --", __FUNCTION__);
    return Err;
}

// scale the cropped frame in a source surface to the whole frame in
// a destination surface
NvError NvImageScaler::CropAndScale(
    NvMMSurfaceDescriptor *pInput,
    NvRect CropRect,
    NvMMSurfaceDescriptor *pOutput)
{
    NV_TRACE_CALL_D(HAL3_IMAGE_SCALER_TAG);
    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: ++", __FUNCTION__);
    NvRect DestRect;
    NvDdk2dFixedRect SrcRect;

    NvOsMemset(&DestRect, 0, sizeof(DestRect));
    DestRect.left = 0;
    DestRect.right = pOutput->Surfaces[0].Width;
    DestRect.top = 0;
    DestRect.bottom = pOutput->Surfaces[0].Height;

    NvOsMemset(&SrcRect, 0, sizeof(SrcRect));
    SrcRect.left = NV_SFX_WHOLE_TO_FX(CropRect.left);
    SrcRect.right = NV_SFX_WHOLE_TO_FX(CropRect.right);
    SrcRect.top = NV_SFX_WHOLE_TO_FX(CropRect.top);
    SrcRect.bottom = NV_SFX_WHOLE_TO_FX(CropRect.bottom);

    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: --", __FUNCTION__);
    return CropAndScale(&pInput->Surfaces[0], pInput->SurfaceCount, &SrcRect,
        &pOutput->Surfaces[0], pOutput->SurfaceCount, &DestRect,0);
}

// scale the cropped frame in a source surface to the whole frame in
// a destination surface
NvError NvImageScaler::CropAndScale(
    NvMMSurfaceDescriptor *pInput,
    NvRectF32 CropRect,
    NvMMSurfaceDescriptor *pOutput)
{
    NV_TRACE_CALL_D(HAL3_IMAGE_SCALER_TAG);
    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: ++", __FUNCTION__);
    NvRect DestRect;
    NvDdk2dFixedRect SrcRect;

    NvOsMemset(&DestRect, 0, sizeof(DestRect));
    DestRect.left = 0;
    DestRect.right = pOutput->Surfaces[0].Width;
    DestRect.top = 0;
    DestRect.bottom = pOutput->Surfaces[0].Height;

    NvOsMemset(&SrcRect, 0, sizeof(SrcRect));
    SrcRect.left = NV_SFX_FP_TO_FX(CropRect.left);
    SrcRect.right = NV_SFX_FP_TO_FX(CropRect.right);
    SrcRect.top = NV_SFX_FP_TO_FX(CropRect.top);
    SrcRect.bottom = NV_SFX_FP_TO_FX(CropRect.bottom);

    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: --", __FUNCTION__);
    return CropAndScale(&pInput->Surfaces[0], pInput->SurfaceCount, &SrcRect,
        &pOutput->Surfaces[0], pOutput->SurfaceCount, &DestRect,0);
}

NvError NvImageScaler::CropAndScale(
    NvMMSurfaceDescriptor *pInput,
    NvRect CropRect,
    NvMMSurfaceDescriptor *pOutput,
    NvU32 imageRotation)
{
    NV_TRACE_CALL_D(HAL3_IMAGE_SCALER_TAG);
    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: ++", __FUNCTION__);
    NvRect DestRect;
    NvDdk2dFixedRect SrcRect;

    NvOsMemset(&DestRect, 0, sizeof(DestRect));
    DestRect.left = 0;
    DestRect.right = pOutput->Surfaces[0].Width;
    DestRect.top = 0;
    DestRect.bottom = pOutput->Surfaces[0].Height;

    NvOsMemset(&SrcRect, 0, sizeof(SrcRect));
    SrcRect.left = NV_SFX_WHOLE_TO_FX(CropRect.left);
    SrcRect.right = NV_SFX_WHOLE_TO_FX(CropRect.right);
    SrcRect.top = NV_SFX_WHOLE_TO_FX(CropRect.top);
    SrcRect.bottom = NV_SFX_WHOLE_TO_FX(CropRect.bottom);

    NV_LOGD(HAL3_IMAGE_SCALER_TAG, "%s: --", __FUNCTION__);
    return CropAndScale(&pInput->Surfaces[0], pInput->SurfaceCount, &SrcRect,
        &pOutput->Surfaces[0], pOutput->SurfaceCount, &DestRect,imageRotation);
}

#if 0 // old code used for OMX AL, not supported in the OMX-less HAL right now

//create surfacedescriptor & init .. based on this m_hRm
NvError NvImageScaler::InitializeOutSurfaceDescr(NvMMSurfaceDescriptor *pOutput,
                        NvU32 argWidth, NvU32 argHeight,
                        OMX_COLOR_FORMATTYPE argColorFormat)
{
    memset(pOutput, 0, sizeof(pOutput));
    pOutput->Empty = NV_FALSE;
    switch (argColorFormat)
    {
        case OMX_COLOR_FormatYUV420Planar:
            pOutput->Surfaces[0].Width = argWidth;
            pOutput->Surfaces[0].Height = argHeight;
            pOutput->Surfaces[0].ColorFormat = NvColorFormat_Y8;
            pOutput->Surfaces[0].Layout = NvRmSurfaceLayout_Pitch;
            pOutput->Surfaces[0].Offset = 0;
            NvRmSurfaceComputePitch(m_hRm, 0, &pOutput->Surfaces[0]);

            pOutput->Surfaces[1].Width = pOutput->Surfaces[0].Width / 2;
            pOutput->Surfaces[1].Height = pOutput->Surfaces[0].Height / 2;
            pOutput->Surfaces[1].ColorFormat = NvColorFormat_U8;
            pOutput->Surfaces[1].Layout = NvRmSurfaceLayout_Pitch;
            pOutput->Surfaces[1].Offset = 0;
            NvRmSurfaceComputePitch(m_hRm, 0, &pOutput->Surfaces[1]);

            pOutput->Surfaces[2].Width = pOutput->Surfaces[0].Width / 2;
            pOutput->Surfaces[2].Height = pOutput->Surfaces[0].Height / 2;
            pOutput->Surfaces[2].ColorFormat = NvColorFormat_V8;
            pOutput->Surfaces[2].Layout = NvRmSurfaceLayout_Pitch;
            pOutput->Surfaces[2].Offset = 0;
            NvRmSurfaceComputePitch(m_hRm, 0, &pOutput->Surfaces[2]);
            pOutput->SurfaceCount = 3;
            break;
        case OMX_COLOR_FormatYUV420SemiPlanar:
            pOutput->Surfaces[0].Width = argWidth;
            pOutput->Surfaces[0].Height = argHeight;
            pOutput->Surfaces[0].ColorFormat = NvColorFormat_Y8;
            pOutput->Surfaces[0].Layout = NvRmSurfaceLayout_Pitch;
            pOutput->Surfaces[0].Offset = 0;
            NvRmSurfaceComputePitch(m_hRm, 0, &pOutput->Surfaces[0]);

            pOutput->Surfaces[1].Width = pOutput->Surfaces[0].Width;
            pOutput->Surfaces[1].Height = pOutput->Surfaces[0].Height / 2;
            pOutput->Surfaces[1].ColorFormat = NvColorFormat_U8_V8;
            pOutput->Surfaces[1].Layout = NvRmSurfaceLayout_Pitch;
            pOutput->Surfaces[1].Offset = 0;
            NvRmSurfaceComputePitch(m_hRm, 0, &pOutput->Surfaces[1]);
            pOutput->SurfaceCount = 2;
            break;
        case OMX_COLOR_FormatYUV422Planar:
            pOutput->Surfaces[0].Width = argWidth;
            pOutput->Surfaces[0].Height = argHeight;
            pOutput->Surfaces[0].ColorFormat = NvColorFormat_Y8;
            pOutput->Surfaces[0].Layout = NvRmSurfaceLayout_Pitch;
            pOutput->Surfaces[0].Offset = 0;
            NvRmSurfaceComputePitch(m_hRm, 0, &pOutput->Surfaces[0]);

            pOutput->Surfaces[1].Width = pOutput->Surfaces[0].Width / 2;
            pOutput->Surfaces[1].Height = pOutput->Surfaces[0].Height;
            pOutput->Surfaces[1].ColorFormat = NvColorFormat_U8;
            pOutput->Surfaces[1].Layout = NvRmSurfaceLayout_Pitch;
            pOutput->Surfaces[1].Offset = 0;
            NvRmSurfaceComputePitch(m_hRm, 0, &pOutput->Surfaces[1]);

            pOutput->Surfaces[2].Width = pOutput->Surfaces[0].Width / 2;
            pOutput->Surfaces[2].Height = pOutput->Surfaces[0].Height;
            pOutput->Surfaces[2].ColorFormat = NvColorFormat_V8;
            pOutput->Surfaces[2].Layout = NvRmSurfaceLayout_Pitch;
            pOutput->Surfaces[2].Offset = 0;
            NvRmSurfaceComputePitch(m_hRm, 0, &pOutput->Surfaces[2]);
            pOutput->SurfaceCount = 3;
            break;
        case OMX_COLOR_FormatYUV422SemiPlanar:
            pOutput->Surfaces[0].Width = argWidth;
            pOutput->Surfaces[0].Height = argHeight;
            pOutput->Surfaces[0].ColorFormat = NvColorFormat_Y8;
            pOutput->Surfaces[0].Layout = NvRmSurfaceLayout_Pitch;
            pOutput->Surfaces[0].Offset = 0;
            NvRmSurfaceComputePitch(m_hRm, 0, &pOutput->Surfaces[0]);

            pOutput->Surfaces[1].Width = pOutput->Surfaces[0].Width;
            pOutput->Surfaces[1].Height = pOutput->Surfaces[0].Height;
            pOutput->Surfaces[1].ColorFormat = NvColorFormat_U8_V8;
            pOutput->Surfaces[1].Layout = NvRmSurfaceLayout_Pitch;
            pOutput->Surfaces[1].Offset = 0;
            NvRmSurfaceComputePitch(m_hRm, 0, &pOutput->Surfaces[1]);
            pOutput->SurfaceCount = 2;
            break;
        case OMX_COLOR_Format32bitARGB8888:
            pOutput->Surfaces[0].Width = argWidth;
            pOutput->Surfaces[0].Height = argHeight;
            pOutput->Surfaces[0].ColorFormat = NvColorFormat_A8R8G8B8;
            pOutput->Surfaces[0].Layout = NvRmSurfaceLayout_Pitch;
            pOutput->Surfaces[0].Offset = 0;
            NvRmSurfaceComputePitch(m_hRm, 0, &pOutput->Surfaces[0]);
            pOutput->SurfaceCount = 1;
            break;
        case OMX_COLOR_Format16bitRGB565:
            pOutput->Surfaces[0].Width = argWidth;
            pOutput->Surfaces[0].Height = argHeight;
            pOutput->Surfaces[0].ColorFormat = NvColorFormat_R5G6B5;
            pOutput->Surfaces[0].Layout = NvRmSurfaceLayout_Pitch;
            pOutput->Surfaces[0].Offset = 0;
            NvRmSurfaceComputePitch(m_hRm, 0, &pOutput->Surfaces[0]);
            pOutput->SurfaceCount = 1;
            break;
        case OMX_COLOR_FormatL8:
            /* Intentionally allocating YUV surfaces; we will just pass the Y surface
            when requested for gray scale as tapping format. This has to be done
            because 2D does not support color conversion into this format */
            pOutput->Surfaces[0].Width = argWidth;
            pOutput->Surfaces[0].Height = argHeight;
            pOutput->Surfaces[0].ColorFormat = NvColorFormat_Y8;
            pOutput->Surfaces[0].Layout = NvRmSurfaceLayout_Pitch;
            pOutput->Surfaces[0].Offset = 0;
            NvRmSurfaceComputePitch(m_hRm, 0, &pOutput->Surfaces[0]);

            pOutput->Surfaces[1].Width = pOutput->Surfaces[0].Width / 2;
            pOutput->Surfaces[1].Height = pOutput->Surfaces[0].Height / 2;
            pOutput->Surfaces[1].ColorFormat = NvColorFormat_U8;
            pOutput->Surfaces[1].Layout = NvRmSurfaceLayout_Pitch;
            pOutput->Surfaces[1].Offset = 0;
            NvRmSurfaceComputePitch(m_hRm, 0, &pOutput->Surfaces[1]);

            pOutput->Surfaces[2].Width = pOutput->Surfaces[0].Width / 2;
            pOutput->Surfaces[2].Height = pOutput->Surfaces[0].Height / 2;
            pOutput->Surfaces[2].ColorFormat = NvColorFormat_V8;
            pOutput->Surfaces[2].Layout = NvRmSurfaceLayout_Pitch;
            pOutput->Surfaces[2].Offset = 0;
            NvRmSurfaceComputePitch(m_hRm, 0, &pOutput->Surfaces[2]);
            pOutput->SurfaceCount = 3;
            break;
        default:
            NV_LOGE("Invalid Color Format %d",argColorFormat);
            return NvError_BadParameter;
    }
    return nvOmxAllocateSurfaces(m_hRm, pOutput, OMX_TRUE);
}


NvError NvImageScaler::DeInitializeOutSurfaceDescr(NvMMSurfaceDescriptor *pOutput)
{
    NvS32 i = 0;
    for (i = 0; i < pOutput->SurfaceCount; i++)
    {
        NvRmMemUnpin(pOutput->Surfaces[i].hMem);
        NvRmMemHandleFree(pOutput->Surfaces[i].hMem);
    }
    NvOsMemset(pOutput, 0, sizeof(NvMMSurfaceDescriptor));
    return NvSuccess;
}

#endif // old code used for OMX AL, not supported in the OMX-less HAL right now

} // namespace android
