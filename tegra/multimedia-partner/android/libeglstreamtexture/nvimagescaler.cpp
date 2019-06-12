/*
 * Copyright (C) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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

#include "nvimagescaler.h"
#include "nvmm_util.h"
#include "OMX_Core.h"

#include <utils/Log.h>

#define MAX_DS_RATIO_DEFAULT (2.0f)

namespace android {


// allocate necessary resources
NvImageScaler::NvImageScaler()
{
    m_hRm = NULL;
    m_h2d = NULL;

    m_InitializeError = NvRmOpen(&m_hRm, 0);
    if (m_InitializeError != NvSuccess)
    {
        ALOGE("%s: failed to create Rm handle! [%d]\n", __FUNCTION__,
            m_InitializeError);
        goto fail;
    }

    m_InitializeError = NvDdk2dOpen(m_hRm, NULL , &m_h2d);
    if (m_InitializeError != NvSuccess)
    {
        ALOGE("%s: failed to create 2d ddk handle! [%d]\n",
            __FUNCTION__, m_InitializeError);
        goto fail;
    }

    return;

fail:
    Release();
}

// destroy all allocated resources
void NvImageScaler::Release()
{
    NvRmClose(m_hRm);
    m_hRm = NULL;
    NvDdk2dClose(m_h2d);
    m_h2d = NULL;
}

// destructor to destroy all allocaed resources
NvImageScaler::~NvImageScaler()
{
    Release();
}

// query the 2d surface type based on the number of surfaces
NvError
NvImageScaler::Get2dSurfaceType(
    NvU32 NumOfSurfaces,
    NvDdk2dSurfaceType *pDdk2dSurfaceType)
{
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
                return NvError_BadParameter;
                break;
      }
      return NvSuccess;
}

// scale the whole frame in a source surface to the whole frame in a
// destination surface
NvError NvImageScaler::Scale(
    NvMMSurfaceDescriptor *pInput,
    NvMMSurfaceDescriptor *pOutput,
    NvDdk2dTransform transformParam)
{
    NvDdk2dFixedRect SrcRect;
    NvRect DestRect;

    // If the constructor failed to allocate the resources
    // return the error
    if (m_InitializeError != NvSuccess)
        return m_InitializeError;

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
    ALOGV("input surface width %d height %d ",pInput->Surfaces[0].Width, pInput->Surfaces[0].Height);
    ALOGV("output surface width %d height %d ",pOutput->Surfaces[0].Width, pOutput->Surfaces[0].Height);

    return CropAndScale(pInput, &SrcRect, pOutput, &DestRect, transformParam);
}

// scale the cropped frame in a source surface to the cropped frame in
// a destination surface
NvError NvImageScaler::CropAndScale(
    NvMMSurfaceDescriptor *pInput,
    NvDdk2dFixedRect *pSrcRect,
    NvMMSurfaceDescriptor *pOutput,
    NvRect *pDestRect,
    NvDdk2dTransform transformParam)
{
    return CropAndScale(&pInput->Surfaces[0], pInput->SurfaceCount,
        pSrcRect, &pOutput->Surfaces[0], pOutput->SurfaceCount, pDestRect, transformParam);
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
    NvDdk2dTransform transformParam)
{
    NvDdk2dSurface *pOutput2dSurface = NULL;
    NvDdk2dSurface *pInput2dSurface = NULL;
    NvDdk2dSurfaceType SurfaceType;
    NvDdk2dBlitParameters Params;
    NvError Err = NvSuccess;


    if (!pInput || !pOutput || !pSrcRect || !pDestRect)
        return NvError_BadParameter;

    if (m_InitializeError != NvSuccess)
        return m_InitializeError;

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
    NvDdk2dSetBlitTransform(&Params, transformParam);

    Err = NvDdk2dBlit(m_h2d, pOutput2dSurface, pDestRect, pInput2dSurface,
        pSrcRect, &Params);
    if (Err != NvSuccess)
        goto cleanup;

    NvDdk2dSurfaceLock(pOutput2dSurface, NvDdk2dSurfaceAccessMode_ReadWrite, NULL,
        NULL, NULL);
    NvDdk2dSurfaceUnlock(pOutput2dSurface, NULL, 0);

cleanup:

    NvDdk2dSurfaceDestroy(pOutput2dSurface);
    NvDdk2dSurfaceDestroy(pInput2dSurface);

    return Err;
}

// scale the cropped frame in a source surface to the whole frame in
// a destination surface
NvError NvImageScaler::CropAndScale(
    NvMMSurfaceDescriptor *pInput,
    NvRect CropRect,
    NvMMSurfaceDescriptor *pOutput)
{
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


    return CropAndScale(&pInput->Surfaces[0], pInput->SurfaceCount, &SrcRect,
        &pOutput->Surfaces[0], pOutput->SurfaceCount, &DestRect, NvDdk2dTransform_None);
}


static NvError NvxAllocateMemoryBuffer(
    NvRmDeviceHandle hRmDev,
    NvRmMemHandle *hMem,
    NvU32 size,
    NvU32 align,
    NvU32 *phyAddress,
    NvOsMemAttribute Coherency)
{
    NvError err = NvSuccess;
    err = NvRmMemHandleAlloc(hRmDev, NULL, 0,
        align, Coherency, size, 0, 0, hMem);
    if(err!= NvSuccess)
    {
        goto exit;
    }

    *phyAddress = NvRmMemPin(*hMem);
exit:
    return err;
}

NvError NvImageScaler::NvxSurfaceAllocContiguousYuv(
    NvMMSurfaceDescriptor *pSurface,
    NvU32 Width,
    NvU32 Height,
    NvU32 Format, // NvMM color format. Not Ddk color format
    NvU32 Layout,
    NvU32 *pImageSize,
    NvBool UseAlignment,
    NvOsMemAttribute Coherency)
{
    NvError err = NvSuccess;
    NvRmDeviceHandle hRmDev = m_hRm; //NvxGetRmDevice();
    NvU32 ChromaWidth = 0, ChromaHeight = 0, ChromaFactor = 1,
          ComponentSize, YSurfaceAlignment, UVSurfaceAlignment;

    NvMMSurfaceDescriptor *pSurfaceDesc = pSurface;

    pSurfaceDesc->Surfaces[0].hMem = NULL;
    pSurfaceDesc->Surfaces[1].hMem = NULL;
    pSurfaceDesc->Surfaces[2].hMem = NULL;

    *pImageSize = 0;

    switch (Format)
    {
    case NvMMVideoDecColorFormat_YUV420:
        Width = (Width + 1) & ~1;
        Height = (Height + 1) & ~1;

        if (Width < 32)
            Width = 32;

        ChromaWidth = Width >> 1;
        ChromaHeight = Height >> 1;
        ChromaFactor = 2;
        // align to multiple of 16 for fast rotation support
        if (UseAlignment)
        {
            ChromaHeight += 15;
            ChromaHeight &= ~0x0F;
            Height = 2*ChromaHeight;
        }
        break;

    default:
        NV_DEBUG_PRINTF(("Unsupported color format\n"));
        break;
    }

    // Y surface
    pSurfaceDesc->Surfaces[0].Width = Width;
    pSurfaceDesc->Surfaces[0].Height = Height;
    pSurfaceDesc->Surfaces[0].ColorFormat = NvColorFormat_Y8;
    pSurfaceDesc->Surfaces[0].Layout = (NvRmSurfaceLayout)Layout;
    pSurfaceDesc->Surfaces[0].Offset = 0;
#ifdef NV_DEF_USE_PITCH_MODE
    NvRmSurfaceComputePitchTest(NULL, 0, &pSurfaceDesc->Surfaces[0]);
    YSurfaceAlignment = NVRM_3D_SURFACE_YUV_PLANAR_ALIGN;
#else
    NvRmSurfaceComputePitch(NULL, 0, &pSurfaceDesc->Surfaces[0]);
    YSurfaceAlignment = NvRmSurfaceComputeAlignment(hRmDev, &pSurfaceDesc->Surfaces[0]);
#endif
    *pImageSize = NvRmSurfaceComputeSize(&pSurfaceDesc->Surfaces[0]);

    // U surface
    pSurfaceDesc->Surfaces[1].Width = ChromaWidth;
    pSurfaceDesc->Surfaces[1].Height = ChromaHeight;
    pSurfaceDesc->Surfaces[1].ColorFormat = NvColorFormat_U8;
    pSurfaceDesc->Surfaces[1].Layout = (NvRmSurfaceLayout)Layout;
#ifdef NV_DEF_USE_PITCH_MODE
    NvRmSurfaceComputePitchTest(NULL, 0, &pSurfaceDesc->Surfaces[1]);
    UVSurfaceAlignment = NVRM_3D_SURFACE_YUV_PLANAR_ALIGN;
#else
    NvRmSurfaceComputePitch(NULL, 0, &pSurfaceDesc->Surfaces[1]);
    UVSurfaceAlignment = NvRmSurfaceComputeAlignment(hRmDev, &pSurfaceDesc->Surfaces[1]);
#endif
    ComponentSize = NvRmSurfaceComputeSize(&pSurfaceDesc->Surfaces[1]);

    pSurfaceDesc->Surfaces[1].Offset = (*pImageSize + UVSurfaceAlignment) & ~(UVSurfaceAlignment - 1);

    *pImageSize = pSurfaceDesc->Surfaces[1].Offset + ComponentSize;

    // V surface
    pSurfaceDesc->Surfaces[2].Width = ChromaWidth;
    pSurfaceDesc->Surfaces[2].Height = ChromaHeight;
    pSurfaceDesc->Surfaces[2].ColorFormat = NvColorFormat_V8;
    pSurfaceDesc->Surfaces[2].Layout = (NvRmSurfaceLayout)Layout;
    pSurfaceDesc->Surfaces[2].Pitch = pSurfaceDesc->Surfaces[1].Pitch;

    pSurfaceDesc->Surfaces[2].Offset = (*pImageSize + UVSurfaceAlignment) & ~(UVSurfaceAlignment - 1);
    *pImageSize = pSurfaceDesc->Surfaces[2].Offset + ComponentSize;

    err = NvxAllocateMemoryBuffer(hRmDev, &pSurfaceDesc->Surfaces[0].hMem,
                                  *pImageSize, YSurfaceAlignment,
                                  &pSurfaceDesc->PhysicalAddress[0],Coherency);
    if (err != NvSuccess)
    {
        NV_DEBUG_PRINTF(("Error in memory allocation for Y\n"));
        goto nvx_error_bail;
    }
    pSurfaceDesc->PhysicalAddress[1] = pSurfaceDesc->PhysicalAddress[0] +  pSurfaceDesc->Surfaces[1].Offset;
    pSurfaceDesc->PhysicalAddress[2] = pSurfaceDesc->PhysicalAddress[0] +  pSurfaceDesc->Surfaces[2].Offset;
    pSurfaceDesc->Surfaces[1].hMem = pSurfaceDesc->Surfaces[2].hMem = pSurfaceDesc->Surfaces[0].hMem;

    //ClearYUVSurface(pSurface);

    pSurfaceDesc->SurfaceCount = 3;
    return NvSuccess;

nvx_error_bail:

    NvRmSurface *pYSurf = &pSurfaceDesc->Surfaces[0];
     if (pYSurf)
         NvMemoryFree(&pYSurf->hMem);

    return NvError_InsufficientMemory;
}


void NvImageScaler::NvOmxAllocateYuv420ContinuousNvmmBuffer(
    NvU32 Width,
    NvU32 Height,
    NvMMBuffer *pNvmmBuffer)
{
    NvMMVideoFormat VideoFormat;
    OMX_U32 i = 0;
    //NvMMBuffer *pNvmmBuffer = NULL;
    NvError Err = NvSuccess;

    if (!m_hRm)
        return; // NULL;

    NvOsMemset(&VideoFormat, 0, sizeof(VideoFormat));

    VideoFormat.NumberOfSurfaces = 3;
    VideoFormat.SurfaceDescription[0].Width = Width;
    VideoFormat.SurfaceDescription[0].Height = Height;
    VideoFormat.SurfaceDescription[0].ColorFormat = NvColorFormat_Y8;
    VideoFormat.SurfaceDescription[0].Layout = NvRmSurfaceLayout_Pitch;
    NvRmSurfaceComputePitch(m_hRm, 0, &VideoFormat.SurfaceDescription[0]);

    VideoFormat.SurfaceDescription[1].ColorFormat = NvColorFormat_U8;
    VideoFormat.SurfaceDescription[2].ColorFormat = NvColorFormat_V8;

    for (i = 1; i < 3; i++)
    {
        VideoFormat.SurfaceDescription[i].Width = Width / 2;
        VideoFormat.SurfaceDescription[i].Height = Height / 2;
        VideoFormat.SurfaceDescription[i].Layout = NvRmSurfaceLayout_Pitch;
        NvRmSurfaceComputePitch(m_hRm, 0, &VideoFormat.SurfaceDescription[i]);
    }


    NvMMBuffer *pBuffer = pNvmmBuffer;
    NvOsMemset(pBuffer, 0, sizeof(NvMMBuffer));
    pBuffer->StructSize = sizeof(NvMMBuffer);
    pBuffer->PayloadType = NvMMPayloadType_SurfaceArray;

    pBuffer->Payload.Surfaces.SurfaceCount = VideoFormat.NumberOfSurfaces;
    NvOsMemcpy(&pBuffer->Payload.Surfaces.Surfaces, VideoFormat.SurfaceDescription,
    sizeof(NvRmSurface) * NVMMSURFACEDESCRIPTOR_MAX_SURFACES);

    return ;
}

void NvImageScaler::NvMemoryFree(NvRmMemHandle *hMem)
{
    NvRmMemUnpin(*hMem);
    NvRmMemHandleFree(*hMem);
    *hMem = NULL;
    return;
}

} // namespace android
