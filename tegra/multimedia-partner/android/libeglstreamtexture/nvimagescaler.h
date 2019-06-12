/*
 * Copyright (C) 2012 NVIDIA Corporation
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
#ifndef NV_IMAGE_SCALER_H
#define NV_IMAGE_SCALER_H

#include "nvmm.h"
#include "nvddk_2d_v2.h"
#include "nvrm_surface.h"

namespace android {

class NvImageScaler
{
public:
    // constructor to allocate resources for this class
    NvImageScaler();

    // destructor to destroy all allocated resources
    ~NvImageScaler();

    // scale the image from the input surface
    // to the output surface
    NvError Scale(
        NvMMSurfaceDescriptor *pInput,
        NvMMSurfaceDescriptor *pOutput,
        NvDdk2dTransform transformParam);

    // scale the image within a crop rectangle from the input surface
    // to a destination rectangle in the output surface
    NvError CropAndScale(
        NvMMSurfaceDescriptor *pInput,
        NvDdk2dFixedRect *pSrcRect,
        NvMMSurfaceDescriptor *pOutput,
        NvRect *pDestRect,
        NvDdk2dTransform transformParam);

    // scale the image within a crop rectangle from the input surface
    // to the output surface
    NvError CropAndScale(
        NvMMSurfaceDescriptor *pInput,
        NvRect CropRect,
        NvMMSurfaceDescriptor *pOutput);

    void NvOmxAllocateYuv420ContinuousNvmmBuffer(
        NvU32 Width,
        NvU32 Height, NvMMBuffer *);

    NvError NvxSurfaceAllocContiguousYuv(
        NvMMSurfaceDescriptor *pSurface,
        NvU32 Width,
        NvU32 Height,
        NvU32 Format,
        NvU32 Layout,
        NvU32 *pImageSize,
        NvBool UseAlignment,
        NvOsMemAttribute Coherency);

    void NvMemoryFree(NvRmMemHandle *hMem);

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
        NvDdk2dTransform transformParam);

    NvRmDeviceHandle m_hRm;
    NvDdk2dHandle m_h2d;
    NvError m_InitializeError;
};

}
#endif // NV_OMX_IMAGE_SCALER_H
