/*
 * Copyright (c) 2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvcamerahal3common.h"
#include "nvformatconverter.h"
#include "nvcamerahal3_tags.h"
#include "nv_log.h"

#define DOWNSCALE_MAX_RATIO 16

namespace android {

FormatConverter::FormatConverter(int format)
{
    NV_LOGD(HAL3_FORMAT_CONVERTER_TAG, "%s: ++", __FUNCTION__);

    NV_LOGD(HAL3_FORMAT_CONVERTER_TAG, "%s: --", __FUNCTION__);
}

FormatConverter::~FormatConverter()
{
    NV_LOGD(HAL3_FORMAT_CONVERTER_TAG, "%s: ++", __FUNCTION__);

    NV_LOGD(HAL3_FORMAT_CONVERTER_TAG, "%s: --", __FUNCTION__);
}

NvError FormatConverter::Convert(
    NvMMBuffer *pNvMMSrc,
    NvRectF32 rect,
    NvMMBuffer *pNvMMDst)
{
    NV_LOGD(HAL3_FORMAT_CONVERTER_TAG, "%s: ++", __FUNCTION__);
    NV_LOGV(HAL3_FORMAT_CONVERTER_TAG, "%s: input %dx%d output: %dx%d ",
        __FUNCTION__,
        pNvMMSrc->Payload.Surfaces.Surfaces[0].Width,
        pNvMMSrc->Payload.Surfaces.Surfaces[0].Height,
        pNvMMDst->Payload.Surfaces.Surfaces[0].Width,
        pNvMMDst->Payload.Surfaces.Surfaces[0].Height);

    NvError err = NvSuccess;
    err = doCropAndScale(pNvMMSrc, rect, pNvMMDst);
    if (err)
    {
        NV_LOGE("In %s at %d, Error", __FUNCTION__, __LINE__);
        return err;
    }
    NV_LOGD(HAL3_FORMAT_CONVERTER_TAG, "%s: --", __FUNCTION__);
    return err;
}

NvBool FormatConverter::needsConversion(
    NvMMBuffer *pNvMMSrc,
    NvRectF32 rect,
    NvMMBuffer *pNvMMDst)
{
    NV_LOGD(HAL3_FORMAT_CONVERTER_TAG, "%s: ++", __FUNCTION__);
    NvS32 i = 0;
    NvMMSurfaceDescriptor *pSrcDesc = &(pNvMMSrc->Payload.Surfaces);
    NvMMSurfaceDescriptor *pDstDesc = &(pNvMMDst->Payload.Surfaces);
    // check surface count
    if (pSrcDesc->SurfaceCount != pDstDesc->SurfaceCount)
    {
        NV_LOGV(HAL3_FORMAT_CONVERTER_TAG, "%s: Src SurfaceCount=%d, Dst SurfaceCount=%d",
            __FUNCTION__,
            pSrcDesc->SurfaceCount, pDstDesc->SurfaceCount);
        return NV_TRUE;
    }
    else
    {
        // Number of surfaces are same. Check colorformat for each surface
        for (i = 0; i < pSrcDesc->SurfaceCount; i++)
        {
            if (pSrcDesc->Surfaces[i].ColorFormat !=
                  pDstDesc->Surfaces[i].ColorFormat)
            {
                NV_LOGV(HAL3_FORMAT_CONVERTER_TAG, "%s: Src ColorFormat=%x, Dst ColorFormat=%x",
                    __FUNCTION__,
                    pSrcDesc->Surfaces[i].ColorFormat,
                    pDstDesc->Surfaces[i].ColorFormat);
                return NV_TRUE;
            }
        }
    }
    // check layout
    if (pSrcDesc->Surfaces[0].Layout != pDstDesc->Surfaces[0].Layout)
    {
        NV_LOGV(HAL3_FORMAT_CONVERTER_TAG, "%s: Src Layout=%d, Dst Layout=%d",
            __FUNCTION__,
            pSrcDesc->Surfaces[0].Layout,
            pDstDesc->Surfaces[0].Layout);
        return NV_TRUE;
    }
    // check resolution
    if (getBufferWidth(pNvMMSrc) != getBufferWidth(pNvMMDst) ||
        getBufferHeight(pNvMMSrc) != getBufferHeight(pNvMMDst))
    {
        NV_LOGV(HAL3_FORMAT_CONVERTER_TAG, "%s: Src %dx%d, Dst Layout=%dx%d",
            __FUNCTION__,
            getBufferWidth(pNvMMSrc), getBufferHeight(pNvMMSrc),
            getBufferWidth(pNvMMDst), getBufferHeight(pNvMMDst));
        return NV_TRUE;
    }

    // check crop
    if (rect.left || rect.right || rect.top || rect.bottom)
    {
        NV_LOGV(HAL3_FORMAT_CONVERTER_TAG, "%s: crop region (%d, %d, %d, %d) is not zero",
            __FUNCTION__, (NvU32)rect.left, (NvU32)rect.top,
            (NvU32)rect.right, (NvU32)rect.bottom);
        return NV_TRUE;
    }

    NV_LOGD(HAL3_FORMAT_CONVERTER_TAG, "%s: --", __FUNCTION__);
    return NV_FALSE;
}

NvError FormatConverter::doCropAndScale(
    NvMMBuffer *pNvMMSrc, NvRectF32 rect, NvMMBuffer *pNvMMDst)
{
    NV_LOGD(HAL3_FORMAT_CONVERTER_TAG, "%s: ++", __FUNCTION__);
    NvError err;
    NvMMSurfaceDescriptor *pSrcDesc = &(pNvMMSrc->Payload.Surfaces);
    NvMMSurfaceDescriptor *pDstDesc = &(pNvMMDst->Payload.Surfaces);
    NvU32 ratio_w, ratio_h;
    NvBool isCropped = NV_FALSE;

    // crop
    if (rect.left || rect.top || rect.right || rect.bottom)
    {
        ratio_w = (NvU32)((ceil)((rect.right - rect.left) /
            pDstDesc->Surfaces[0].Width));
        ratio_h = (NvU32)((ceil)((rect.bottom - rect.top) /
            pDstDesc->Surfaces[0].Height));
        isCropped = NV_TRUE;
    }
    else
    {
        ratio_w = pSrcDesc->Surfaces[0].Width /
            pDstDesc->Surfaces[0].Width;
        ratio_h = pSrcDesc->Surfaces[0].Height /
            pDstDesc->Surfaces[0].Height;
    }

    if (ratio_w <= DOWNSCALE_MAX_RATIO &&
        ratio_h <= DOWNSCALE_MAX_RATIO)
    {
        if (isCropped)
        {
            NV_LOGV(HAL3_FORMAT_CONVERTER_TAG, "crop and scale: input(%dx%d), crop(%dx%d), output(%dx%d)",
                pSrcDesc->Surfaces[0].Width, pSrcDesc->Surfaces[0].Height,
                (NvU32)(rect.right - rect.left + 0.5),
                (NvU32)(rect.bottom - rect.top + 0.5),
                pDstDesc->Surfaces[0].Width, pDstDesc->Surfaces[0].Height);
            err = mScaler.CropAndScale(pSrcDesc, rect, pDstDesc);
        }
        else
        {
            NV_LOGV(HAL3_FORMAT_CONVERTER_TAG, "scaling from %dx%d to %dx%d",
                pSrcDesc->Surfaces[0].Width, pSrcDesc->Surfaces[0].Height,
                pDstDesc->Surfaces[0].Width, pDstDesc->Surfaces[0].Height);
            err = mScaler.Scale(pSrcDesc, pDstDesc);
        }
    }
    else
    {
        NvMMBuffer tmpBuffer;
        NvMMSurfaceDescriptor *pTmpDesc = &(tmpBuffer.Payload.Surfaces);
        NvU32 width, height;
        NvOsMemcpy(&tmpBuffer, pNvMMSrc, sizeof(NvMMBuffer));
        NvS32 i;
        NvU32 chroma_subsample_ratio;

        // Destination rectangle will be located on left-top corner
        // to make sure that reading pointer is faster than writing
        // pointer since source and destination surfaces are same.
        if (isCropped)
        {
            width = (NvU32)(rect.right - rect.left + 0.5);
            height = (NvU32)(rect.bottom - rect.top + 0.5);
        }
        else
        {
            width = pTmpDesc->Surfaces[0].Width;
            height = pTmpDesc->Surfaces[0].Height;
        }

        width  = (width + DOWNSCALE_MAX_RATIO - 1) / DOWNSCALE_MAX_RATIO;
        height = (height + DOWNSCALE_MAX_RATIO - 1) / DOWNSCALE_MAX_RATIO;

        // Ensure that width and height of Y are even number.
        // Otherwise width and height of C doesn't become integer.
        width = ((width + 1) >> 1) << 1;
        height = ((height + 1) >> 1) << 1;

        // Change destination width and height.
        for (i = 0; i < pTmpDesc->SurfaceCount; i++)
        {
            if (i == 0)
            {
                pTmpDesc->Surfaces[i].Width = width;
                pTmpDesc->Surfaces[i].Height = height;
            }
            else
            {
                chroma_subsample_ratio = pSrcDesc->Surfaces[0].Width /
                    pSrcDesc->Surfaces[i].Width;
                pTmpDesc->Surfaces[i].Width = width /
                    chroma_subsample_ratio;
                pTmpDesc->Surfaces[i].Height = height /
                    chroma_subsample_ratio;
            }
        }

        if (isCropped)
        {
            NV_LOGV(HAL3_FORMAT_CONVERTER_TAG, "crop and scale: input(%dx%d), crop(%dx%d), output(%dx%d)",
                pSrcDesc->Surfaces[0].Width, pSrcDesc->Surfaces[0].Height,
                width, height,
                pTmpDesc->Surfaces[0].Width, pTmpDesc->Surfaces[0].Height);
            err = mScaler.CropAndScale(pSrcDesc, rect, pTmpDesc);
        }
        else
        {
            NV_LOGV(HAL3_FORMAT_CONVERTER_TAG, "scaling from %dx%d to %dx%d",
                    pSrcDesc->Surfaces[0].Width, pSrcDesc->Surfaces[0].Height,
                    pTmpDesc->Surfaces[0].Width, pTmpDesc->Surfaces[0].Height);
            err = mScaler.Scale(pSrcDesc, pTmpDesc);
        }

        {
            NvRectF32 zeroCrop;
            NvOsMemset(&zeroCrop, 0, sizeof(NvRectF32));
            doCropAndScale(&tmpBuffer, zeroCrop, pNvMMDst);
        }

    }

    NV_LOGD(HAL3_FORMAT_CONVERTER_TAG, "%s: --", __FUNCTION__);
    return err;
}


}
