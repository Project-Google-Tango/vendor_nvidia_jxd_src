/*
 * Copyright (C) 2010-2012 The Android Open Source Project
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

/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 */

#include "linux/tegra_dc_ext.h"
#include "nvrm_surface.h"
#include "nvblit.h"
#include "nvhwc.h"
#include "nvsync.h"
#include "nvfb_cursor.h"
#include "nvgrsurfutil.h"

#define DEBUG_CURSOR 0

#if DEBUG_CURSOR
#define CURSORLOG(...) ALOGD("Cursor: "__VA_ARGS__)
#else
#define CURSORLOG(...)
#endif

#define MAX_CURSOR_IMAGES 2

enum nvfb_cursor_dim {
    NVFB_CURSORDIM_32,
    NVFB_CURSORDIM_64,
    NVFB_CURSORDIM_128,
    NVFB_CURSORDIM_256,
    NVFB_CURSORDIM_COUNT
};

#define CURSORDIM_COMPUTE_SIZE(cursordim) \
    (1u << (5 + (cursordim)))

struct nvfb_cursor_state {
    buffer_handle_t image;
    unsigned flags;
    int x_pos;
    int y_pos;
};

struct nvfb_cursor {
    int fd;
    struct nvfb_display_caps *caps;

    /* Buffer state */
    struct {
        buffer_handle_t current;
        NvU32 writeCount;
    } buffer;

    int blend;
    int transform;
    NvRect src;
    int dst_width;
    int dst_height;

    /* Image state */
    struct {
        buffer_handle_t handle[MAX_CURSOR_IMAGES][NVFB_CURSORDIM_COUNT];
        int index;
    } image;

    /* Current state */
    struct nvfb_cursor_state cursor;

    NvGrModule *grmod;
};

static inline int NeedFilter(int transform, NvRect *src, NvRect *dst)
{
    int src_w, src_h, dst_w, dst_h;

    if (transform & HWC_TRANSFORM_ROT_90) {
        src_w = r_height(src);
        src_h = r_width(src);
    } else {
        src_w = r_width(src);
        src_h = r_height(src);
    }
    dst_w = r_width(dst);
    dst_h = r_height(dst);

    return (src_w - dst_w) | (src_h - dst_h);
}

static int copy_image(struct nvfb_cursor *dev,
                      buffer_handle_t srcSurf,
                      buffer_handle_t dstSurf,
                      int srcAcquireFenceFd,
                      struct nvfb_window *win)
{
    NvError err;
    NvBlitRect srcRect;
    NvBlitRect dstRect;
    NvBlitState state;

    CURSORLOG("copy image");

    /* Clear the surface */
    NvBlitClearState(&state);
    NvBlitSetSrcColor(&state, 0, NvColorFormat_A8R8G8B8);
    NvBlitSetDstSurface(&state, dstSurf, -1);
    NvBlitSetFlag(&state, NvBlitFlag_DeferFlush);
    err = NvBlit(dev->grmod->nvblit, &state, NULL);
    if (err != NvSuccess) {
        ALOGE("%s: NvBlit failed (err=%d)", __func__, err);
        return -1;
    }

    r_copy(&srcRect, &win->src);

    /* Translate to glyph origin */
    dstRect.left   = 0;
    dstRect.top    = 0;
    dstRect.right  = r_width(&win->dst);
    dstRect.bottom = r_height(&win->dst);

    NvBlitClearState(&state);

    NvBlitSetSrcRect(&state, &srcRect);
    NvBlitSetDstRect(&state, &dstRect);
    NvBlitSetSrcSurface(&state, srcSurf, srcAcquireFenceFd);
    NvBlitSetDstSurface(&state, dstSurf, -1);

    if (NeedFilter(win->transform, &win->src, &win->dst)) {
        NvBlitSetFilter(&state, NvBlitFilter_Linear);
    }

    switch (win->transform) {
    case 0:
        break;
    #define CASE(t, h) \
    case h: NvBlitSetTransform(&state, NvBlitTransform_##t); break
    CASE(Rotate270,      HWC_TRANSFORM_ROT_90);
    CASE(Rotate180,      HWC_TRANSFORM_ROT_180);
    CASE(Rotate90,       HWC_TRANSFORM_ROT_270 );
    CASE(FlipHorizontal, HWC_TRANSFORM_FLIP_H);
    CASE(FlipVertical,   HWC_TRANSFORM_FLIP_V);
    CASE(InvTranspose,   HWC_TRANSFORM_FLIP_H | HWC_TRANSFORM_ROT_90);
    CASE(Transpose,      HWC_TRANSFORM_FLIP_V | HWC_TRANSFORM_ROT_90);
    #undef CASE
    default:
        NV_ASSERT(!"Unknown transform");
        break;
    }

    #if DEBUG_CURSOR
    NvBlitSetFlag(&state, NvBlitFlag_Trace);
    NvBlitSetFlag(&state, NvBlitFlag_DumpSurfaces);
    #endif

    /* The cursor is not pre-multipled.  If the source is
     * pre-multipled, we need to divide by alpha to compensate.  There
     * is no way to do this in 2d yet, so copy here and fix via CPU
     * later (see CPU_CONVERT).
     */

    err = NvBlit(dev->grmod->nvblit, &state, NULL);
    if (err != NvSuccess) {
        ALOGE("%s: NvBlit failed (err=%d)", __func__, err);
        return -1;
    }

    return 0;
}

static int convert_image(struct nvfb_cursor *dev,
                         struct nvfb_cursor_state *state,
                         int size,
                         NvBool unPremult,
                         NvBool swizzle)
{
    NvNativeHandle *handle = (NvNativeHandle *) state->image;
    int x;
    int y;
    int usage;
    int stride = handle->LockedPitchInPixels;
    int result;
    void *ptr;
    uint32_t *ptr32;

    CURSORLOG("convert image: %s %s",
              unPremult ? "unpremult" : "",
              swizzle ? "swizzle" : "");

    #if NV_DEBUG
    NV_ASSERT(dev->caps->cursor_format = NvColorFormat_R8G8B8A8);
    if (swizzle) {
        NV_ASSERT(NvGrGetNvFormat(handle->Format) == NvColorFormat_A8B8G8R8);
    } else {
        NV_ASSERT(NvGrGetNvFormat(handle->Format) == NvColorFormat_R8G8B8A8);
    }
    #endif

    usage = GRALLOC_USAGE_SW_READ_OFTEN
          | GRALLOC_USAGE_SW_WRITE_OFTEN;

    result = dev->grmod->Base.lock(&dev->grmod->Base,
                                   state->image,
                                   usage,
                                   0, 0,
                                   size, size,
                                   &ptr);
    if (result != 0) {
        ALOGE("%s: Gralloc lock (err=%d)", __func__, result);
        return result;
    }

    ptr32 = (uint32_t *) ptr;

    if (unPremult) {
        /* Cursor is not premultipled and cannot be
         * changed.  To compensate, divide the alpha
         * (i.e. un-premultiply).
         */
        #define CLAMP(x) ((x) > 255.0 ? 255 : ((uint8_t) x))
        if (swizzle) {
            for (y=0; y<size; ++y) {
                for (x=0; x<size; ++x) {
                    uint32_t val = ptr32[x];
                    float r = (val      ) & 0xff;
                    float g = (val >>  8) & 0xff;
                    float b = (val >> 16) & 0xff;
                    float a = (val >> 24) & 0xff;
                    float inv = a ? (255.0 / a) : 0.0;
                    r *= inv;
                    g *= inv;
                    b *= inv;
                    ptr32[x] = (CLAMP(r) & 0xff) << 24 |
                               (CLAMP(g) & 0xff) << 16 |
                               (CLAMP(b) & 0xff) <<  8 |
                               (CLAMP(a) & 0xff);
                }
                ptr32 += stride;
            }
        } else {
            for (y=0; y<size; ++y) {
                for (x=0; x<size; ++x) {
                    uint32_t val = ptr32[x];
                    float a = (val      ) & 0xff;
                    float b = (val >>  8) & 0xff;
                    float g = (val >> 16) & 0xff;
                    float r = (val >> 24) & 0xff;
                    float inv = a ? (255.0 / a) : 0.0;
                    r *= inv;
                    g *= inv;
                    b *= inv;
                    ptr32[x] = (CLAMP(r) & 0xff) << 24 |
                               (CLAMP(g) & 0xff) << 16 |
                               (CLAMP(b) & 0xff) <<  8 |
                               (CLAMP(a) & 0xff);
                }
                ptr32 += stride;
            }
        }
        #undef CLAMP
    } else if (swizzle) {
        /* Cursor component order is RGBA where the image is ABGR.
         * Swap the compoment order here.
         */
        for (y=0; y<size; ++y) {
            for (x=0; x<size; ++x) {
                uint32_t val = ptr32[x];
                ptr32[x] = ((val & 0xff000000) >> 24) |
                           ((val & 0x00ff0000) >>  8) |
                           ((val & 0x0000ff00) <<  8) |
                           ((val & 0x000000ff) << 24);
            }
            ptr32 += stride;
        }
    }

    result = dev->grmod->Base.unlock(&dev->grmod->Base, state->image);
    if (result != 0) {
        ALOGE("%s: Gralloc unlock (err=%d)", __func__, result);
    }

    return result;
}

static int update_image(struct nvfb_cursor *dev,
                        struct nvfb_window *win,
                        struct nvfb_buffer *buf,
                        struct nvfb_cursor_state *state)
{
    buffer_handle_t srcSurf;
    buffer_handle_t dstSurf;
    int srcAcquireFenceFd;
    int index;
    int result;
    size_t w;
    size_t h;
    size_t image_size;
    size_t ii;
    int dim;
    NvBool unPremult;
    NvBool swizzle;

    state->image = NULL;
    state->flags = 0;

    if (buf == NULL || buf->buffer == NULL) {
        return 0;
    }

    w = r_width(&win->dst);
    h = r_height(&win->dst);
    image_size = NV_MAX(w, h);
    dim = -1;
    unPremult = NV_FALSE;
    swizzle = NV_FALSE;

    NV_ASSERT(image_size <= dev->caps->max_cursor);
    NV_ASSERT(dev->caps->max_cursor <=
              CURSORDIM_COMPUTE_SIZE(NVFB_CURSORDIM_COUNT-1));

    index = (dev->image.index + 1) % MAX_CURSOR_IMAGES;

    for (ii = 0; ii < NVFB_CURSORDIM_COUNT; ii++) {
        if (image_size <= CURSORDIM_COMPUTE_SIZE(ii) &&
            dev->image.handle[index][ii]) {
            dim = ii;
            break;
        }
    }
    if (dim < 0) {
        return 0;
    }

    switch(dim) {
    #define CASE(n) \
    case NVFB_CURSORDIM_##n: \
        state->flags |= TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_##n##x##n; \
        break
    CASE(32);
    CASE(64);
    CASE(128);
    CASE(256);
    #undef CASE
    default:
        NV_ASSERT(!"Unexpected cursor size");
        break;
    }

    if (win->blend == HWC_BLENDING_PREMULT) {
        if (dev->caps->display_cap & NVFB_DISPLAY_CAP_PREMULT_CURSOR) {
            state->flags |= TEGRA_DC_EXT_CURSOR_FORMAT_FLAGS_RGBA_PREMULT_ALPHA;
        } else {
            state->flags |= TEGRA_DC_EXT_CURSOR_FORMAT_FLAGS_RGBA_NON_PREMULT_ALPHA;
            unPremult = NV_TRUE;
        }
    } else {
        state->flags |= TEGRA_DC_EXT_CURSOR_FORMAT_FLAGS_RGBA_NON_PREMULT_ALPHA;
    }

    state->image = dev->image.handle[index][dim];
    srcSurf = (buffer_handle_t) buf->buffer;
    dstSurf = state->image;
    srcAcquireFenceFd = buf->preFenceFd;

    if (srcSurf == NULL || dstSurf == NULL) {
        state->image = NULL;
        return ENOMEM;
    }

    result = copy_image(dev, srcSurf, dstSurf, srcAcquireFenceFd, win);
    if (result != 0) {
        state->image = NULL;
        return ENOMEM;
    }

    if (NvGrGetNvFormat(((NvNativeHandle *) dstSurf)->Format) !=
        dev->caps->cursor_format) {
        swizzle = NV_TRUE;
    }

    if (unPremult || swizzle) {
        convert_image(dev, state, image_size, unPremult, swizzle);
    }

    return 0;
}

static int nvfb_cursor_set(struct nvfb_cursor *dev,
                           struct nvfb_cursor_state *state)
{
    if (state->image != dev->cursor.image || state->flags != dev->cursor.flags) {
        if (state->image) {
            struct tegra_dc_ext_cursor_image image;

            memset(&image, 0, sizeof(image));
            image.flags = state->flags;
            image.buff_id = (__u32) ((NvNativeHandle *) state->image)->Surf[0].hMem;

            CURSORLOG("set cursor image");

            if (ioctl(dev->fd, TEGRA_DC_EXT_SET_CURSOR_IMAGE, &image)) {
                ALOGE("%s:%d: TEGRA_DC_EXT_SET_CURSOR_IMAGE failed: %s",
                      __func__, __LINE__, strerror(errno));
                return errno;
            }
        } else {
            struct tegra_dc_ext_cursor cursor;
            CURSORLOG("hiding cursor");

            memset(&cursor, 0, sizeof(cursor));
            if (ioctl(dev->fd, TEGRA_DC_EXT_SET_CURSOR, &cursor)) {
                ALOGE("%s:%d: TEGRA_DC_EXT_SET_CURSOR failed: %s",
                      __func__, __LINE__, strerror(errno));
                return errno;
            }
        }

        dev->cursor.image = state->image;
        dev->cursor.flags = state->flags;
    }

    if (state->image) {
        struct tegra_dc_ext_cursor cursor;

        memset(&cursor, 0, sizeof(cursor));
        cursor.flags = TEGRA_DC_EXT_CURSOR_FLAGS_VISIBLE;
        cursor.x = state->x_pos;
        cursor.y = state->y_pos;

        CURSORLOG("set cursor position");

        if (ioctl(dev->fd, TEGRA_DC_EXT_SET_CURSOR, &cursor)) {
            ALOGE("%s:%d: TEGRA_DC_EXT_SET_CURSOR failed: %s",
                  __func__, __LINE__, strerror(errno));
            return errno;
        }

        dev->cursor.x_pos = state->x_pos;
        dev->cursor.y_pos = state->y_pos;
    }

    return 0;
}

int nvfb_cursor_update(struct nvfb_cursor *dev, struct nvfb_window *win,
                       struct nvfb_buffer *buf)
{
    struct nvfb_cursor_state state = dev->cursor;
    int status = 0;

    if (!win) {
        dev->buffer.current = NULL;

        int ret = update_image(dev, NULL, NULL, &state);

        memset(&state, 0, sizeof(state));
    } else {

        NvNativeHandle *h = buf->buffer;
        NvU32 writeCount = h ? NvGrGetWriteCount(h) : 0;

        /* Update cursor image if changed */
        if (dev->buffer.current != &(h->Base) ||
            dev->buffer.writeCount != writeCount ||
            dev->blend != win->blend ||
            dev->transform != win->transform ||
            // Check if dst rects don't match in size
            dev->dst_width != r_width(&win->dst) ||
            dev->dst_height != r_height(&win->dst) ||
            // Check if src rects differ at all
            memcmp(&dev->src, &win->src, sizeof(win->src))) {

            int ret = update_image(dev, win, buf, &state);

            if (ret) {
                return ret;
            }

            dev->buffer.current = &(h->Base);
            dev->buffer.writeCount = writeCount;

            dev->blend = win->blend;
            dev->transform = win->transform;
            dev->src = win->src;
            dev->dst_width = r_width(&win->dst);
            dev->dst_height = r_height(&win->dst);
        }
    }

    if (buf && buf->preFenceFd >= 0) {
        nvsync_close(buf->preFenceFd);
        buf->preFenceFd = -1;
    }

    if (!status) {
        /* Update cursor position only if enabled */
        if (state.image) {
            state.x_pos = win->dst.left;
            state.y_pos = win->dst.top;
        }

        if (memcmp(&state, &dev->cursor, sizeof(state))) {
            return nvfb_cursor_set(dev, &state);
        }
    }

    return status;
}

static buffer_handle_t alloc_image(struct nvfb_cursor *dev,
                                   size_t size,
                                   int format)
{
    buffer_handle_t handle;
    int result;
    int stride;
    int usage;

    usage = GRALLOC_USAGE_HW_2D;

    if ((dev->caps->display_cap & NVFB_DISPLAY_CAP_PREMULT_CURSOR) == 0 ||
        dev->caps->cursor_format != NvGrGetNvFormat(format)) {
        usage |= GRALLOC_USAGE_SW_READ_OFTEN
              |  GRALLOC_USAGE_SW_WRITE_OFTEN;
    }

    result = nvgr_scratch_alloc(size, size,
                                format,
                                usage,
                                NvRmSurfaceLayout_Pitch,
                                &handle);
    if (result != 0) {
        ALOGE("%s: Gralloc allocation failed (err=%d)", __func__, result);
        return NULL;
    }

    return handle;
}

int nvfb_cursor_open(struct nvfb_cursor **cursor_ret,
                     NvGrModule *grmod,
                     int fd,
                     struct nvfb_display_caps *caps)
{
    struct nvfb_cursor *dev;
    size_t ii, jj;
    int ret;
    int format;

    dev = (struct nvfb_cursor *) malloc(sizeof(struct nvfb_cursor));
    if (!dev) {
        return ENOMEM;
    }

    memset(dev, 0, sizeof(struct nvfb_cursor));

    dev->fd = fd;
    dev->caps = caps;
    dev->grmod = grmod;

    if (NvRmModuleGetNumInstances(grmod->Rm, NvRmModuleID_2D) > 0) {
        format = HAL_PIXEL_FORMAT_RGBA_8888;
    } else {
        format = NVGR_PIXEL_FORMAT_ABGR_8888;
    }

    for (ii = 0; ii < MAX_CURSOR_IMAGES; ii++) {
        for (jj = 0; jj < NVFB_CURSORDIM_COUNT; jj++) {
            size_t size = CURSORDIM_COMPUTE_SIZE(jj);
            dev->image.handle[ii][jj] = alloc_image(dev, size, format);
            if (dev->image.handle[ii][jj] == NULL) {
                goto fail;
            }

            // The cursor HW requires a tightly packed pitch
            {
                unsigned int i;
                size_t surfCount;
                const NvRmSurface* surfs;
                nvgr_get_surfaces(dev->image.handle[ii][jj], &surfs, &surfCount);
                for (i = 0; i < surfCount; i++) {
                    if (surfs[i].Pitch != surfs[i].Width*(NV_COLOR_GET_BPP(surfs[i].ColorFormat) >> 3)) {
                        nvgr_scratch_free(dev->image.handle[ii][jj]);
                        dev->image.handle[ii][jj] = NULL;
                        break;
                    }
                }
            }
        }
    }

    if (ioctl(dev->fd, TEGRA_DC_EXT_GET_CURSOR)) {
        ret = errno;
        goto fail;
    }

    *cursor_ret = dev;

    return 0;

 fail:
    // Roll back alloc_image
    for (ii = 0; ii < MAX_CURSOR_IMAGES; ii++) {
        for (jj = 0; jj < NVFB_CURSORDIM_COUNT; jj++) {
            if (dev->image.handle[ii][jj]) {
                nvgr_scratch_free(dev->image.handle[ii][jj]);
            }
        }
    }

    free(dev);

    return ret;
}

void nvfb_cursor_close(struct nvfb_cursor *dev)
{
    size_t ii, jj;
    struct tegra_dc_ext_cursor cursor;

    /* Release current cursor */
    memset(&cursor, 0, sizeof(cursor));
    /* No error expected, but just log it, no need to fail here */
    if (ioctl(dev->fd, TEGRA_DC_EXT_SET_CURSOR, &cursor)) {
        ALOGE("%s:%d: TEGRA_DC_EXT_SET_CURSOR failed: %s",
              __func__, __LINE__, strerror(errno));
    }
    if (ioctl(dev->fd, TEGRA_DC_EXT_PUT_CURSOR)) {
        ALOGE("%s:%d: TEGRA_DC_EXT_PUT_CURSOR failed: %s",
              __func__, __LINE__, strerror(errno));
    }

    for (ii = 0; ii < MAX_CURSOR_IMAGES; ii++) {
        for (jj = 0; jj < NVFB_CURSORDIM_COUNT; jj++) {
            if (dev->image.handle[ii][jj]) {
                nvgr_scratch_free(dev->image.handle[ii][jj]);
            }
        }
    }

    free(dev);
}
