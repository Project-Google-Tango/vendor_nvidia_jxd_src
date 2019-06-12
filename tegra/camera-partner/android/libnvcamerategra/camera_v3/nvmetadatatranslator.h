/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * Copyright 2008-2014, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _NV_METADATA_TRANSLATOR_H_
#define _NV_METADATA_TRANSLATOR_H_

#include <nverror.h>
#include <CameraMetadata.h>
#include <nvcam_properties_public.h>
#include "nvcamerahal3common.h"

namespace android
{

namespace NvMetadataTranslator
{
    int captureIntent(const CameraMetadata& mData);

    NvError translateToNvCamProperty(
        CameraMetadata& metaData,
        NvCameraHal3_Public_Controls &hal3_prop,
        const NvCamProperty_Public_Static& statProp,
        const NvMMCameraSensorMode& sensorMode);

    NvError translateFromNvCamProperty(
        NvCameraHal3_Public_Controls &camProp,
        CameraMetadata& metaData);

    NvError translateFromNvCamProperty(
            const NvCameraHal3_Public_Dynamic &hal3Prop,
            const NvCameraHal3_Public_Controls &propDef,
            CameraMetadata& mData,
            const NvCamProperty_Public_Static& statProp);

    NvError translateFromNvCamProperty(
        const NvCamProperty_Public_Static &camProp,
        CameraMetadata& metaData);

    NvError translatePartialFromNvCamProperty(
        const NvCameraHal3_Public_Dynamic& hal3Prop,
        CameraMetadata& mData, const NvMMCameraSensorMode& sensorMode,
        const NvCamProperty_Public_Static& statProp);

    NvError MapRegionsToValidRange(
        NvCamRegions& nvCamRegions,
        const NvSize& fromResolution,
        const NvSize& toResolution);

    void MapRectToValidRange(NvRect& rect,
        const NvSize& fromResolution,
        const NvSize& toResolution);
};

} // namespace android

#endif  //_NV_METADATA_MANAGER_H_
