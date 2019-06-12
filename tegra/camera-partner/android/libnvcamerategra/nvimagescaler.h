/*
 * Copyright (c) 2011-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef NV_IMAGE_SCALER_H
#define NV_IMAGE_SCALER_H

#include "nvmm.h"
#include "nvddk_2d_v2.h"
#include "nvrm_surface.h"
#include "OMX_IVCommon.h"

namespace android {

typedef enum NVMM_FRAME_FORMAT {
    NVMM_FRAME_FormatInvalid = 0,
    NVMM_STILL_FRAME_FormatYV12 = 1,
    NVMM_STILL_FRAME_FormatNV12,
    NVMM_VIDEO_FRAME_FormatIYUV,
    NVMM_VIDEO_FRAME_FormatNV21,
    NVMM_PREVIEW_FRAME_FormatYV12,
    NVMM_PREVIEW_FRAME_FormatNV21,
    NVMM_FRAME_FormatForce32 = 0x7FFFFFFF
} NVMM_FRAME_FORMAT;

class NvImageScaler
{
public:
    // constructor to allocate resources for this class
    NvImageScaler();

    // destructor to destroy all allocated resources
    ~NvImageScaler();

    NvError
    AllocateYuv420NvmmSurface(
        NvMMSurfaceDescriptor *pSurface,
        NvU32 Width,
        NvU32 Height,
        NVMM_FRAME_FORMAT format = NVMM_VIDEO_FRAME_FormatIYUV);

    NvError
    AllocateNV12NvmmSurface(
        NvMMSurfaceDescriptor *pSurface,
        NvU32 Width,
        NvU32 Height);

    void FreeSurface(NvMMSurfaceDescriptor *pSurface);

    // scale the image from the input surface
    // to the output surface
    NvError Scale(
        NvMMSurfaceDescriptor *pInput,
        NvMMSurfaceDescriptor *pOutput);

    // scale the image within a crop rectangle from the input surface
    // to a destination rectangle in the output surface
    NvError CropAndScale(
        NvMMSurfaceDescriptor *pInput,
        NvDdk2dFixedRect *pSrcRect,
        NvMMSurfaceDescriptor *pOutput,
        NvRect *pDestRect);

    // scale the image within a crop rectangle from the input surface
    // to the output surface
    NvError CropAndScale(
        NvMMSurfaceDescriptor *pInput,
        NvRect CropRect,
        NvMMSurfaceDescriptor *pOutput);

    NvError CropAndScale(
        NvMMSurfaceDescriptor *pInput,
        NvRect CropRect,
        NvMMSurfaceDescriptor *pOutput,
        NvU32 imageRotation);

    //create surfacedescriptor & init .. based on this m_hRm
    NvError InitializeOutSurfaceDescr(NvMMSurfaceDescriptor *pOutput,
                        NvU32 w, NvU32 h,
                        OMX_COLOR_FORMATTYPE argColorFormat);
    //DeInit
    NvError DeInitializeOutSurfaceDescr(NvMMSurfaceDescriptor *pOutput);

private:
    void Release();

    NvError Get2dSurfaceType(
        NvU32 NumOfSurfaces,
        NvDdk2dSurfaceType *pDdk2dSurfaceType);

    NvError CropAndScale(
        NvRmSurface *pInput,
        NvU32 NumInputSurface,
        NvDdk2dFixedRect *pSrcRect,
        NvRmSurface *pOutput,
        NvU32 NumOutputSurface,
        NvRect *pDestRect,
        NvU32 imageRotation);

    NvRmDeviceHandle m_hRm;
    NvDdk2dHandle m_h2d;
    NvError m_InitializeError;
};

}
#endif // NV_IMAGE_SCALER_H
