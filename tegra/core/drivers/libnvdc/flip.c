/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <errno.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/tegra_dc_ext.h>

#include <nvcolor.h>
#include <nvdc.h>
#include <nvassert.h>

#include "nvdc_priv.h"

int nvdcGetWindow(struct nvdcState *state, int head, int num)
{
    int ret = 0;

    if (!NVDC_VALID_HEAD(head)) {
        return EINVAL;
    }

    if (ioctl(state->tegraExtFd[head], TEGRA_DC_EXT_GET_WINDOW, num)) {
        ret = errno;
    }

    return ret;
}

int nvdcPutWindow(struct nvdcState *state, int head, int num)
{
    int ret = 0;

    if (!NVDC_VALID_HEAD(head)) {
        return EINVAL;
    }

    if (ioctl(state->tegraExtFd[head], TEGRA_DC_EXT_PUT_WINDOW, num)) {
        ret = errno;
    }

    return ret;
}

static int getRGBFormat(NvColorFormat color, unsigned int *pixFormat)
{
    int format = -1;

    /* XXX support more formats */
    switch (color) {
    case NvColorFormat_R5G5B5A1:
        format = TEGRA_DC_EXT_FMT_B5G5R5A;
        break;

    case NvColorFormat_R5G6B5:
        format = TEGRA_DC_EXT_FMT_B5G6R5;
        break;

    case NvColorFormat_A8B8G8R8:
    case NvColorFormat_X8B8G8R8:
        format = TEGRA_DC_EXT_FMT_R8G8B8A8;
        break;

    case NvColorFormat_A8R8G8B8:
    case NvColorFormat_X8R8G8B8:
        format = TEGRA_DC_EXT_FMT_B8G8R8A8;
        break;

    default:
        return EINVAL;
    }

    *pixFormat = format;

    return 0;
}

static int getSurfData(const NvRmSurface *surf,
                       unsigned int *bufferId,
                       unsigned int *offset,
                       unsigned int *pitch,
                       unsigned int *tiled,
                       unsigned int *blocklinear,
                       unsigned int *bh,
                       unsigned int *interlace,
                       unsigned int *offset2)
{
    *bufferId =     (unsigned int)surf->hMem;

    if (!*bufferId) {
        return EINVAL;
    }

    *offset      = surf->Offset;
    *pitch       = surf->Pitch;
    *tiled       = surf->Layout == NvRmSurfaceLayout_Tiled;
    *blocklinear = surf->Layout == NvRmSurfaceLayout_Blocklinear;
    *bh          = surf->BlockHeightLog2;
    *interlace   = (surf->DisplayScanFormat == NvDisplayScanFormat_Interlaced);
    *offset2     = surf->SecondFieldOffset;

    return 0;
}

int nvdcFlip(struct nvdcState *state, int head, struct nvdcFlipArgs *args)
{
    struct tegra_dc_ext_flip flip = { };
    int ret = 0;
    int i;

    if (!NVDC_VALID_HEAD(head) ||
        args->numWindows > TEGRA_DC_EXT_FLIP_N_WINDOWS) {
        return EINVAL;
    }

    for (i = 0; i < args->numWindows; i++) {
        struct tegra_dc_ext_flip_windowattr *win = &flip.win[i];
        const struct nvdcFlipWinArgs *wargs = &args->win[i];
        unsigned int bufferId, offset, pitch, tiled;
        unsigned int bl, bh;
        unsigned int interlace, offset2;

        win->index =            wargs->index;
        win->timestamp = wargs->timestamp;

        if (!wargs->surfaces[0]) {
            continue;
        }

        ret = getSurfData(wargs->surfaces[NVDC_FLIP_SURF_Y],
            &bufferId, &offset, &pitch, &tiled, &bl, &bh,
            &interlace, &offset2);
        if (ret) {
            return ret;
        }
        win->buff_id =  bufferId;
        win->offset =   offset;
        win->stride =   pitch;
        if (tiled) {
            win->flags |= TEGRA_DC_EXT_FLIP_FLAG_TILED;
        }
        else if (bl) {
            unsigned int nvdc_caps;

            /* Check if BL is supported */
            nvdcGetCapabilities(state, &nvdc_caps);
            if (!(nvdc_caps & TEGRA_DC_EXT_CAPABILITIES_BLOCKLINEAR)) {
                return EINVAL;
            }
            win->flags |= TEGRA_DC_EXT_FLIP_FLAG_BLOCKLINEAR;
            win->block_height_log2 = (uint8_t)bh;
        }
        if (interlace) {
            win->flags |= TEGRA_DC_EXT_FLIP_FLAG_INTERLACE;
            win->offset2 = offset2;
        }

        if (wargs->globalAlpha & NVDC_GLOBAL_ALPHA_ENABLE) {
            win->flags |= TEGRA_DC_EXT_FLIP_FLAG_GLOBAL_ALPHA;
            win->global_alpha = wargs->globalAlpha & NVDC_GLOBAL_ALPHA_MASK;
        }

        if (wargs->surfaces[NVDC_FLIP_SURF_U]) {
            ret = getSurfData(wargs->surfaces[NVDC_FLIP_SURF_U],
                &bufferId, &offset, &pitch, &tiled, &bl, &bh,
                &interlace, &offset2);
            if (ret) {
                return ret;
            }
            if (bufferId != win->buff_id) {
                win->buff_id_u = bufferId;
            }
            win->offset_u =     offset;
            win->stride_uv =    pitch;
            if ((!!tiled) != !!(win->flags & TEGRA_DC_EXT_FLIP_FLAG_TILED)) {
                return EINVAL;
            }
            if ((!!bl) != !!(win->flags & TEGRA_DC_EXT_FLIP_FLAG_BLOCKLINEAR)) {
                return EINVAL;
            }
            if (interlace)
            win->offset_u2 = offset2;
        }
        if (wargs->surfaces[NVDC_FLIP_SURF_V]) {
            ret = getSurfData(wargs->surfaces[NVDC_FLIP_SURF_V],
                &bufferId, &offset, &pitch, &tiled, &bl, &bh,
                &interlace, &offset2);
            if (ret) {
                return ret;
            }
            if (bufferId != win->buff_id) {
                win->buff_id_v = bufferId;
            }
            win->offset_v = offset;
            if (win->stride_uv != pitch) {
                return EINVAL;
            }
            if ((!!tiled) != !!(win->flags & TEGRA_DC_EXT_FLIP_FLAG_TILED)) {
                return EINVAL;
            }
            if ((!!bl) != !!(win->flags & TEGRA_DC_EXT_FLIP_FLAG_BLOCKLINEAR)) {
                return EINVAL;
            }
            if(interlace)
            win->offset_v2 = offset2;
        }

        win->x =                wargs->x;
        win->y =                wargs->y;
        win->w =                wargs->w;
        win->h =                wargs->h;
        win->out_x =            wargs->outX;
        win->out_y =            wargs->outY;
        win->out_w =            wargs->outW;
        win->out_h =            wargs->outH;
        win->z =                wargs->z;
        win->swap_interval =    wargs->swapInterval;
        win->pre_syncpt_id =    wargs->preSyncptId;
        win->pre_syncpt_val =   wargs->preSyncptVal;
        switch (wargs->orientation) {
        case nvdcOrientation_0:
                break;
        case nvdcOrientation_180:
                win->flags |= TEGRA_DC_EXT_FLIP_FLAG_INVERT_H |
                        TEGRA_DC_EXT_FLIP_FLAG_INVERT_V;
                break;
#ifdef TEGRA_DC_EXT_FLIP_FLAG_SCAN_COLUMN
        /* if linux/tegra_dc_ext.h defines this macro, try to use it. */
        case nvdcOrientation_90:
                win->flags |= TEGRA_DC_EXT_FLIP_FLAG_SCAN_COLUMN |
                        TEGRA_DC_EXT_FLIP_FLAG_INVERT_V;
                break;
        case nvdcOrientation_270:
                win->flags |= TEGRA_DC_EXT_FLIP_FLAG_SCAN_COLUMN |
                        TEGRA_DC_EXT_FLIP_FLAG_INVERT_H;
                break;
#else
        case nvdcOrientation_90:
        case nvdcOrientation_270:
                return EINVAL; /* not supported */
#endif
        }

        switch (wargs->reflection) {
        case nvdcReflection_none:
            break;
        case nvdcReflection_X:
            win->flags ^= TEGRA_DC_EXT_FLIP_FLAG_INVERT_H;
            break;
        case nvdcReflection_Y:
            win->flags ^= TEGRA_DC_EXT_FLIP_FLAG_INVERT_V;
            break;
        case nvdcReflection_XY:
            win->flags ^= TEGRA_DC_EXT_FLIP_FLAG_INVERT_H |
                    TEGRA_DC_EXT_FLIP_FLAG_INVERT_V;
            break;
        }

        switch (wargs->blend) {
        case NVDC_BLEND_NONE:
            win->blend = NVDC_BLEND_NONE;
            break;
        case NVDC_BLEND_PREMULT:
            win->blend = NVDC_BLEND_PREMULT;
            break;
        case NVDC_BLEND_COVERAGE:
            win->blend = NVDC_BLEND_COVERAGE;
            break;
        /* NOTE: Colorkey blending is supported only under QNX */
        case NVDC_BLEND_COLORKEY:
        default:
            return EINVAL;
        }

        if (wargs->format == nvdcRGB_AUTO) {
            NvColorFormat fmt = args->win[i].surfaces[0]->ColorFormat;
            ret = getRGBFormat(fmt, &win->pixformat);
            if (ret) {
                return ret;
            }
            NV_ASSERT(!wargs->surfaces[NVDC_FLIP_SURF_U]);
            NV_ASSERT(!wargs->surfaces[NVDC_FLIP_SURF_V]);
        } else {
            /* NOTE: these enums need to be kept in sync with the kernel
             * defines */
            win->pixformat = wargs->format;
        }

        if (wargs->cursorMode) {
            if (state->caps & TEGRA_DC_EXT_CAPABILITIES_CURSOR_MODE) {
                win->flags |= TEGRA_DC_EXT_FLIP_FLAG_CURSOR;
            } else {
                /* Fallback for kernels without cursor mode flipping */
                win->swap_interval = 0;
            }
        }
    }

    for (; i < TEGRA_DC_EXT_FLIP_N_WINDOWS; i++) {
        flip.win[i].index = -1;
    }

    if (ioctl(state->tegraExtFd[head], TEGRA_DC_EXT_FLIP, &flip)) {
        ret = errno;
    }

    args->postSyncptId = flip.post_syncpt_id;
    args->postSyncptValue = flip.post_syncpt_val;

    return ret;
}
