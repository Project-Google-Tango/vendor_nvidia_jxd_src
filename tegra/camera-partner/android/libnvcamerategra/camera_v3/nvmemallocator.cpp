/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvmemallocator.h"
#include "nv_log.h"

namespace android {

NvError NvMemAllocator::AllocateNvMMSurface(
    NvMMSurfaceDescriptor *pSurface,
    NvU32 Width,
    NvU32 Height,
    NvCameraHAL3ColorFormat format)
{
    NvS32 i = 0;
    NvU32 Size = 0, Alignment = 0;
    NvError e = NvSuccess;
    NvS32 SurfaceCount = 0;
    NvRmMemHandleAttr memAttr;
    NvRmDeviceHandle hRm = NULL;
    NvRmSurface *surf = NULL;
    NvColorFormat       nvFormat[3];

    static const NvRmHeap Heaps[] =
    {
      NvRmHeap_ExternalCarveOut,
      NvRmHeap_External,
    };
    NvU32 attrs[] = {
        NvRmSurfaceAttribute_Layout, NvRmSurfaceLayout_Blocklinear,
        NvRmSurfaceAttribute_Compression, NV_FALSE,
        NvRmSurfaceAttribute_None
    };

    if (!pSurface)
        return NvError_BadParameter;

    NV_CHECK_ERROR_CLEANUP(
        NvRmOpen(&hRm, 0)
    );

    NvOsMemset(pSurface, 0, sizeof(NvMMSurfaceDescriptor));

    switch (format)
    {
        case NV_CAMERA_HAL3_COLOR_YV12:
            SurfaceCount = 3;
            pSurface->Surfaces[0].Width = Width;
            pSurface->Surfaces[0].Height = Height;
            pSurface->Surfaces[0].ColorFormat = NvColorFormat_Y8;
            pSurface->Surfaces[1].Width = Width / 2;
            pSurface->Surfaces[1].Height = Height / 2;
            pSurface->Surfaces[1].ColorFormat = NvColorFormat_U8;
            pSurface->Surfaces[2].Width = pSurface->Surfaces[1].Width;
            pSurface->Surfaces[2].Height = pSurface->Surfaces[1].Height;
            pSurface->Surfaces[2].ColorFormat = NvColorFormat_V8;
            nvFormat[0] = pSurface->Surfaces[0].ColorFormat;
            nvFormat[1] = pSurface->Surfaces[1].ColorFormat;
            nvFormat[2] = pSurface->Surfaces[2].ColorFormat;

            break;
        case NV_CAMERA_HAL3_COLOR_NV12:
            SurfaceCount = 2;
            pSurface->Surfaces[0].Width = Width;
            pSurface->Surfaces[0].Height = Height;
            pSurface->Surfaces[0].ColorFormat = NvColorFormat_Y8;
            pSurface->Surfaces[1].Width = Width / 2;
            pSurface->Surfaces[1].Height = Height / 2;
            pSurface->Surfaces[1].ColorFormat = NvColorFormat_U8_V8;
            nvFormat[0] = pSurface->Surfaces[0].ColorFormat;
            nvFormat[1] = pSurface->Surfaces[1].ColorFormat;

            break;
        case NV_CAMERA_HAL3_COLOR_NV21:
            SurfaceCount = 2;
            pSurface->Surfaces[0].Width = Width;
            pSurface->Surfaces[0].Height = Height;
            pSurface->Surfaces[0].ColorFormat = NvColorFormat_Y8;
            pSurface->Surfaces[1].Width = Width / 2;
            pSurface->Surfaces[1].Height = Height / 2;
            pSurface->Surfaces[1].ColorFormat = NvColorFormat_V8_U8;
            nvFormat[0] = pSurface->Surfaces[0].ColorFormat;
            nvFormat[1] = pSurface->Surfaces[1].ColorFormat;

            break;
        default:
            break;
    }

    /*
     * This will adhere to the restrictions of both VIC and GPU by reducing BlockHeightLog2
     * for all surfaces. Earlier parameters of surfaces were being set seperately and it
     * might happen that BlockHegihtLog2 of surfaces would be different(Bug 1473008).
     */
    NvRmMultiplanarSurfaceSetup(pSurface->Surfaces, SurfaceCount, Width, Height,
                                    NvYuvColorFormat_YUV420, nvFormat, attrs);

    for (i = 0; i < SurfaceCount; i++)
    {
        surf = &pSurface->Surfaces[i];
        Alignment = NvRmSurfaceComputeAlignment(hRm,
            &pSurface->Surfaces[i]);

        NvU32 size = NvRmSurfaceComputeSize(surf);

        NVRM_DEFINE_MEM_HANDLE_ATTR(memAttr);
        NVRM_MEM_HANDLE_SET_ATTR(memAttr,
                                Alignment,
                                NvOsMemAttribute_WriteCombined,
                                size, 0);
        NVRM_MEM_HANDLE_SET_KIND_ATTR(memAttr, surf->Kind);
        memAttr.Heaps    = Heaps;
        memAttr.NumHeaps = NV_ARRAY_SIZE(Heaps);
        NV_CHECK_ERROR_CLEANUP(NvRmMemHandleAllocAttr(hRm, &memAttr, &surf->hMem));

        pSurface->SurfaceCount = i + 1;
    }

    NvRmClose(hRm);
    return e;

fail:
    NV_LOGE("%s: error--", __FUNCTION__);
    NvRmClose(hRm);
    FreeSurface(pSurface);
    return NvError_InsufficientMemory;
}

void NvMemAllocator::FreeSurface(
    NvMMSurfaceDescriptor *pSurface)
{
    NvS32 i = 0;

    for (i = 0; i < pSurface->SurfaceCount; i++)
    {
        NvRmMemHandleFree(pSurface->Surfaces[i].hMem);
    }

    NvOsMemset(pSurface, 0, sizeof(NvMMSurfaceDescriptor));
}

}
