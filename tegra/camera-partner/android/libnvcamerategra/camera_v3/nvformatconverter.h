/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * Copyright 2008, The Android Open Source Project
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

#ifndef _NV_FORMAT_CONVERTER_H_
#define _NV_FORMAT_CONVERTER_H_

#include "nvimagescaler.h"
#include "jpegencoder.h"
#include "nvcamerahal3common.h"

namespace android {

typedef struct JpegParams {
    NvJpegEncParameters jpegParams;
    NvJpegEncParameters thumbJpegParams;
    NvMMEXIFInfo *pExifInfo;
    char makerNoteData[MAKERNOTE_DATA_SIZE];
} JpegParams;


class FormatConverter {
public:
    FormatConverter(int format);
    ~FormatConverter();
    NvError Convert(
        NvMMBuffer *pSrc,
        NvRectF32 rect,
        NvMMBuffer *pNvMMDst);

private:
    NvBool needsConversion(NvMMBuffer *pSrc,
        NvRectF32 rect, NvMMBuffer *pNvMMDst);
    NvError doCropAndScale(NvMMBuffer *pNvMMSrc,
        NvRectF32 rect, NvMMBuffer *pNvMMDst);

private:

    NvImageScaler mScaler;

};
} // namespace android

#endif
