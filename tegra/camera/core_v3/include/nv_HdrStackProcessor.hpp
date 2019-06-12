/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef NV_HDRSTACKPROCESSOR_HPP
#define NV_HDRSTACKPROCESSOR_HPP

#include <stdint.h>

class HDRData;

class HdrStackProcessor
{
public:
    HdrStackProcessor();
    ~HdrStackProcessor();

    /* Must pre-program the resolution and number of the input YUV images to
     * expect.  Currently, only the default of 3 images is supported. */
    void InitHdrStackProcessor(uint32_t width, uint32_t height, uint32_t numImg = 3);

    /* Add YUV images one at a time.  After numImg have been added,
     * GetHDRComposedImage can be called to get the result.
     * The only input format supported is YUV420p, width == pitch, contiguous
     * layout where Y is followed by U then V */
    void AddLDRImage(uint8_t* data);

    /* Get the result of HDR processing after all images are added */
    void GetHDRComposedImage(uint8_t* imgComp);

    /* Turn on or off image registration.  Registration helps eliminate
     * artifacts from image missalignment due to hand shake, but takes some extra
     * processing time. */
    void SetHDRRegistrationEnable(bool b);

    /* Reset/clear the internal state of the library. This is useful in case
     * a corruption of data were to occur while populting the library with a set
     * of images. */
    void ResetHDRProcessing();

private:
    HDRData* m_d;
};

#endif
