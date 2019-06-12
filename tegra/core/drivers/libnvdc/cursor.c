/*
 * Copyright (c) 2012-2013 NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <errno.h>
#include <sys/ioctl.h>

#include <linux/tegra_dc_ext.h>

#include <nvdc.h>

#include "nvdc_priv.h"

int nvdcGetCursor(struct nvdcState *state, int head)
{
    int ret = 0;

    if (!NVDC_VALID_HEAD(head)) {
        return EINVAL;
    }

    if (ioctl(state->tegraExtFd[head], TEGRA_DC_EXT_GET_CURSOR)) {
        ret = errno;
    }

    return ret;
}

int nvdcPutCursor(struct nvdcState *state, int head)
{
    int ret = 0;

    if (!NVDC_VALID_HEAD(head)) {
        return EINVAL;
    }

    if (ioctl(state->tegraExtFd[head], TEGRA_DC_EXT_PUT_CURSOR)) {
        ret = errno;
    }

    return ret;
}

int nvdcSetCursorImage(struct nvdcState *state, int head,
                       struct nvdcCursorImage *args)
{
    struct tegra_dc_ext_cursor_image image = {  };
    int ret = 0;

    if (!NVDC_VALID_HEAD(head)) {
        return EINVAL;
    }

    image.foreground.r = args->foreground.r;
    image.foreground.g = args->foreground.g;
    image.foreground.b = args->foreground.b;
    image.background.r = args->background.r;
    image.background.g = args->background.g;
    image.background.b = args->background.b;

    image.buff_id = args->bufferId;

    switch (args->size) {
        case NVDC_CURSOR_IMAGE_SIZE_256x256:
            image.flags |= TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_256x256;
            break;
        case NVDC_CURSOR_IMAGE_SIZE_128x128:
            image.flags |= TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_128x128;
            break;
        case NVDC_CURSOR_IMAGE_SIZE_64x64:
            image.flags |= TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_64x64;
            break;
        case NVDC_CURSOR_IMAGE_SIZE_32x32:
            image.flags |= TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_32x32;
            break;
        default:
            return EINVAL;
    }

    switch (args->format) {
        case NVDC_CURSOR_IMAGE_FORMAT_2BIT:
            image.flags |= TEGRA_DC_EXT_CURSOR_FORMAT_FLAGS_2BIT_LEGACY;
            break;
        case NVDC_CURSOR_IMAGE_FORMAT_RGBA_NON_PREMULT_ALPHA:
            image.flags |= TEGRA_DC_EXT_CURSOR_FORMAT_FLAGS_RGBA_NON_PREMULT_ALPHA;
            break;
        case NVDC_CURSOR_IMAGE_FORMAT_RGBA_PREMULT_ALPHA:
            image.flags |= TEGRA_DC_EXT_CURSOR_FORMAT_FLAGS_RGBA_PREMULT_ALPHA;
            break;
        default:
            return EINVAL;
    }

    if (ioctl(state->tegraExtFd[head], TEGRA_DC_EXT_SET_CURSOR_IMAGE, &image)) {
        ret = errno;
    }

    return ret;
}

int nvdcSetCursor(struct nvdcState *state, int head, int x, int y, int visible)
{
    struct tegra_dc_ext_cursor cursor = { };
    int ret = 0;

    if (!NVDC_VALID_HEAD(head)) {
        return EINVAL;
    }

    cursor.x = x;
    cursor.y = y;
    if (visible) {
        cursor.flags |= TEGRA_DC_EXT_CURSOR_FLAGS_VISIBLE;
    }

    if (ioctl(state->tegraExtFd[head], TEGRA_DC_EXT_SET_CURSOR, &cursor)) {
        ret = errno;
    }

    return ret;
}

int nvdcCursorClip(struct nvdcState *state, int head, int win_clip)
{
#ifndef TEGRA_DC_EXT_CURSOR_CLIP
    return ENOSYS;
#else
    int ret = 0;

    if (!NVDC_VALID_HEAD(head)) {
        return EINVAL;
    }

    if( win_clip < 0 || win_clip > 2) {
        return EINVAL;
    }

    if (ioctl(state->tegraExtFd[head], TEGRA_DC_EXT_CURSOR_CLIP, &win_clip)) {
        ret = errno;
    }

    return ret;
#endif
}

int nvdcSetCursorImageLowLatency(struct nvdcState *state, int head,
        struct nvdcCursorImage *args, int visiable)
{
    struct tegra_dc_ext_cursor_image image = {  };
    int ret = 0;

    if (!NVDC_VALID_HEAD(head)) {
        return EINVAL;
    }

    image.foreground.r = args->foreground.r;
    image.foreground.g = args->foreground.g;
    image.foreground.b = args->foreground.b;
    image.background.r = args->background.r;
    image.background.g = args->background.g;
    image.background.b = args->background.b;

    image.buff_id = args->bufferId;
    image.mode = args->mode;

    if(visiable) {
        image.vis |= TEGRA_DC_EXT_CURSOR_FLAGS_VISIBLE;
    }

    if (ioctl(state->tegraExtFd[head], TEGRA_DC_EXT_SET_CURSOR_IMAGE_LOW_LATENCY, &image)) {
        ret = errno;
    }

    return ret;
}

int nvdcSetCursorLowLatency(struct nvdcState *state, int head, int x, int y, struct nvdcCursorImage *args)
{
    struct tegra_dc_ext_cursor_image image = { };
    int ret = 0;

    if (!NVDC_VALID_HEAD(head)) {
        return EINVAL;
    }

    switch(args->size) {
        case NVDC_CURSOR_IMAGE_SIZE_64x64:
            image.flags |= TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_64x64;
            break;
        case NVDC_CURSOR_IMAGE_SIZE_32x32:
            image.flags |= TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_32x32;
            break;
        case NVDC_CURSOR_IMAGE_SIZE_128x128:
            image.flags |= TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_128x128;
            break;
        case NVDC_CURSOR_IMAGE_SIZE_256x256:
            image.flags |= TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_256x256;
            break;
        default:
        return EINVAL;
    }
    image.buff_id = args->bufferId;

    image.x = x;
    image.y = y;

    if (ioctl(state->tegraExtFd[head], TEGRA_DC_EXT_SET_CURSOR_LOW_LATENCY, &image)) {
        ret = errno;
    }

    return ret;
}

