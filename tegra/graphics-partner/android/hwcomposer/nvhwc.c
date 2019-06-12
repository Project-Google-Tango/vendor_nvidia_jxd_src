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
 * Copyright (c) 2010-2014, NVIDIA CORPORATION.  All rights reserved.
 */

#include <fcntl.h>
#include <errno.h>
#include <cutils/atomic.h>
#include <EGL/egl.h>
#include <linux/tegra_dc_ext.h>
#include <sys/resource.h>

#include "nvhwc.h"
#include "nvsync.h"
#include "nvassert.h"
#include "nvhwc_external.h"
#include "nvgrsurfutil.h"
#include "nvrm_chip.h"
#include "nvfl.h"

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include "nvatrace.h"

/*****************************************************************************/


#ifndef ALLOW_VIDEO_SCALING
#define ALLOW_VIDEO_SCALING 0
#endif

#define PATH_FTRACE_MARKER_LOG   "/sys/kernel/debug/tracing/trace_marker"

#if TOUCH_SLOWSCAN_ENABLE
#define TOUCH_SLOWSCAN_PATH "/sys/devices/virtual/misc/raydium_ts/slowscan_enable"
#define TOUCH_SLOWSCAN_INVALID_STATE (-1)
#endif

/*
 * Work around for DC YUV src rect issue.
 * For YUV the starting position for DC scan must be on an even boundary.
 */
#define DC_WAR_1325061 1

/* Prototypes */
static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device);

static void conf_prepare(struct nvhwc_context *ctx,
                         struct nvhwc_display *dpy,
                         struct nvhwc_prepare_state *prepare);

static void window_attrs(struct nvhwc_display *dpy,
                         struct nvfb_window *win,
                         struct nvhwc_window_mapping *map,
                         struct nvhwc_prepare_layer *ll,
                         NvBool clipDst);

static int hwc_set(hwc_composer_device_t *dev,
                   size_t numDisplays,
                   hwc_display_contents_t** displays);

static int hwc_set_ftrace(hwc_composer_device_t *dev,
                          size_t numDisplays,
                          hwc_display_contents_t** displays);

static void prepare_adjust_yuv_crop(struct nvhwc_prepare_layer *ll);

static int hwc_invalidate(void *data);

static struct hw_module_methods_t hwc_module_methods = {
    open: hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        version_major: 1,
        version_minor: 0,
        id: HWC_HARDWARE_MODULE_ID,
        name: "NVIDIA Tegra hwcomposer module",
        author: "NVIDIA",
        methods: &hwc_module_methods,
    }
};

static inline int nv_intersect(const NvRect* r1,
                               const NvRect* r2)
{
    return !(r2->left >= r1->right || r2->right <= r1->left ||
             r2->top >= r1->bottom || r2->bottom <= r1->top);
}

static inline void r_grow(hwc_rect_t* dst, const hwc_rect_t* src)
{
    if (r_empty(dst)) {
        *dst = *src;
    } else {
        dst->left = NV_MIN(dst->left, src->left);
        dst->top = NV_MIN(dst->top, src->top);
        dst->right = NV_MAX(dst->right, src->right);
        dst->bottom = NV_MAX(dst->bottom, src->bottom);
    }
}

static inline void r_clip(hwc_rect_t* dst, const hwc_rect_t* clip)
{
    dst->left = NV_MAX(dst->left, clip->left);
    dst->top = NV_MAX(dst->top, clip->top);
    dst->right = NV_MIN(dst->right, clip->right);
    dst->bottom = NV_MIN(dst->bottom, clip->bottom);
}

int hwc_get_scale(int transform, const hwc_rect_t *src, const hwc_rect_t *dst,
                   float *scale_w, float *scale_h)
{
    int src_w = r_width(src);
    int src_h = r_height(src);
    int dst_w, dst_h;

    if (transform & HWC_TRANSFORM_ROT_90) {
        dst_w = r_height(dst);
        dst_h = r_width(dst);
    } else {
        dst_w = r_width(dst);
        dst_h = r_height(dst);
    }

    *scale_w = (float) dst_w / src_w;
    *scale_h = (float) dst_h / src_h;

    return src_w != dst_w || src_h != dst_h;
}

/* translate layer requirements to overlay caps */
static unsigned get_layer_requirements(struct nvhwc_prepare_state *prepare,
                                       struct nvhwc_prepare_layer *ll,
                                       int need_scale)
{
    hwc_layer_t* l = ll->layer;
    NvColorFormat format = get_colorformat(l);
    unsigned req = 0;

    if (prepare->sequential) {
        req |= NVFB_WINDOW_CAP_SEQUENTIAL;
    }

    if (get_nvrmsurface(l)->Layout != NvRmSurfaceLayout_Pitch) {
        req |= NVFB_WINDOW_CAP_TILED;
    }

    if (ll->transform & (HWC_TRANSFORM_FLIP_H | HWC_TRANSFORM_FLIP_V)) {
        req |= NVFB_WINDOW_CAP_FLIPHV;
    }

    if (ll->transform & HWC_TRANSFORM_ROT_90) {
        /* Assume that for planar YUV it is enough to inspect first layer only */
        if (get_nvrmsurface(l)->Layout == NvRmSurfaceLayout_Pitch) {
            req |= NVFB_WINDOW_CAP_SWAPXY_PITCH;
        } else {
            if (format == NvColorFormat_Y8) {
                req |= NVFB_WINDOW_CAP_SWAPXY_TILED_PLANAR;
            } else {
                req |= NVFB_WINDOW_CAP_SWAPXY_TILED;
            }
        }
    }

    if (need_scale) {
        req |= NVFB_WINDOW_CAP_SCALE;
    }

    if (hwc_layer_is_yuv(l))
        req |= NVFB_WINDOW_CAP_YUV_FORMATS;

    return req;
}

/* get number of free windows, adjusted for reserving framebuffer */
static inline int window_is_available(struct nvhwc_display *dpy,
                                      struct nvhwc_prepare_state *prepare,
                                      int ii)
{
    /* Number of available windows */
    int num_available = prepare->sequential ?
            prepare->free_sequential : prepare->current_map + 1;
    /* Number of subsequent layers to process */
    int num_required = prepare->numLayers - ii - 1;

    /* Reserve a window for the framebuffer if necessary */
    if (dpy->fb_index == -1 && (prepare->remainder.limit || num_required)) {
        num_available--;
    }

    NV_ASSERT(num_available >= 0);
    return num_available;
}

/* get the least capable window meeting the requirements in a best
 * possible way, remove from mask */
int hwc_get_window(const struct nvfb_display_caps *caps,
                    unsigned *mask, unsigned req, unsigned *unsatisfied)
{
    int ii, best = -1;
    NvBool exact;

    if (unsatisfied) {
        *unsatisfied = req;
        exact = NV_FALSE;
    } else {
        exact = NV_TRUE;
    }

    for (ii = 0; ii < (int)caps->num_windows; ii++) {
        if ((1 << ii) & *mask) {
            unsigned u = (req ^ caps->window_cap[ii].cap) & req;
            if (exact) {
                /* Consider only overlays meeting all requirements */
                if (!u &&
                    (best == -1 ||
                     (caps->window_cap[best].cap > caps->window_cap[ii].cap)))
                    best = ii;
                continue;
            }
            if (!u) {
                /* Start considering only overlays meeting all requirements */
                exact = NV_TRUE;
                best = ii;
                *unsatisfied = 0;
                continue;
            }
            /* Consider overlays meeting best subset of requirements */
            if (best == -1 ||
                (*unsatisfied > u) ||
                ((*unsatisfied == u) &&
                 (caps->window_cap[best].cap > caps->window_cap[ii].cap))) {
                best = ii;
                *unsatisfied = u;
            }
        }
    }

    if (best != -1)
        *mask &= ~(1 << best);
    return best;
}

static inline NvBool can_use_window(hwc_layer_t* l)
{
    /* TODO erratas? */

    NV_ASSERT(!(l->flags & HWC_SKIP_LAYER));

    if (!l->handle) {
        return NV_FALSE;
    }

    /* format not supported by gralloc */
    if (nvfb_translate_hal_format(get_halformat(l)) == -1) {
        return NV_FALSE;
    }

    if (r_width(&l->displayFrame) < 4 ||
        r_height(&l->displayFrame) < 4) {
        return NV_FALSE;
    }

    return NV_TRUE;
}

/* assign hw window to layer, if possible */
static int assign_window(struct nvhwc_context *ctx,
                         struct nvhwc_display *dpy,
                         struct nvhwc_prepare_state *prepare,
                         int idx)
{
    struct nvhwc_prepare_layer *ll = &prepare->layers[idx];
    struct nvhwc_window_mapping *map = &dpy->map[prepare->current_map];
    hwc_layer_t *l = ll->layer;
    NvRmSurface *surf;
    NvColorFormat format;
    NvRmSurfaceLayout layout;

    unsigned req, unsatisfied_req;
    int window;
    int transform = 0;
    int need_scale = 0;
    int need_swapxy = 0;
    int layer_needs_scale;
    int need_convert = 0;
    float scale_w = 1.0f;
    float scale_h = 1.0f;

    if (!can_use_window(l)) {
        return -1;
    }

    surf = get_nvrmsurface(l);
    format = surf->ColorFormat;
    layout = NvRmSurfaceGetDefaultLayout();

    layer_needs_scale = hwc_get_scale(ll->transform, &ll->src,
                                      &ll->dst, &scale_w, &scale_h);

    /* dc does not filter alpha channel, hence do not use dc for blending
     * scaled overlays - bug #1515812
     */
    if (hwc_layer_has_alpha(l) && ll->blending != HWC_BLENDING_NONE &&
        layer_needs_scale ) {
        return -1;
    }

    req = get_layer_requirements(prepare, ll, layer_needs_scale);

    /* scaling limits of dc */
    if (req & NVFB_WINDOW_CAP_SCALE) {
        float min_scale = NVFB_WINDOW_DC_MIN_SCALE;

        /* WAR for bug #395578 - downscaling beyond the hardware limit
         * can lead to underflow.  Use 2D scaling in this case.
         */
        if (scale_w < min_scale || scale_h < min_scale) {
            need_scale = 1;
            req &= ~NVFB_WINDOW_CAP_SCALE;
        }
    }

    /* Check the source height for display rotation limitation */
    if (req & NVFB_WINDOW_CAP_SWAPXY) {
        NvNativeHandle *h = (NvNativeHandle *) l->handle;
        int max_height = (int)dpy->caps.max_rot_src_height;

        if (h->SurfCount == 1 && !(req & NVFB_WINDOW_CAP_SCALE)) {
            max_height = (int)dpy->caps.max_rot_src_height_noscale;
        }

        if (r_height(&ll->src) > max_height) {
            req &= ~NVFB_WINDOW_CAP_SWAPXY;
            need_swapxy = 1;
        }
    }

    window = hwc_get_window(&dpy->caps, &prepare->window_mask, req, &unsatisfied_req);

    /* Bail out if a sequential window cannot be offered without
     * additional processing and there are less remaining windows than layers,
     * hence composition will be needed. In this case adding the layer
     * to composition can be cheaper then the additional processing.
     */
    if (prepare->sequential && unsatisfied_req &&
        prepare->numLayers - idx - 1 >= prepare->free_sequential) {
        prepare->window_mask |= 1 << window;
        return -1;
    }

    if (unsatisfied_req & NVFB_WINDOW_CAP_FLIPHV) {
        unsatisfied_req &= ~NVFB_WINDOW_CAP_FLIPHV;
        transform |= ll->transform & (HWC_TRANSFORM_FLIP_H |
                                      HWC_TRANSFORM_FLIP_V);
    }

    /* If no windows with planar rotation or pitch rotation, resp.,
     * are available, we can use 2D instead. We may decide if we
     * rotate or do a planar convert on 2D as a future optimisation.
     * Note: NVFB_WINDOW_CAP_SWAPXY_PLANAR may fail for instance when
     * playing more then one video stream.
     */
    if (need_swapxy || unsatisfied_req & NVFB_WINDOW_CAP_SWAPXY) {
        unsatisfied_req &= ~NVFB_WINDOW_CAP_SWAPXY;
        transform |= HWC_TRANSFORM_ROT_90;
        /* Get proper layout for rotation; this is a performance optimization */
        layout = dpy->scratch->rotation_layout;
    }

    if (unsatisfied_req & NVFB_WINDOW_CAP_TILED) {
        unsatisfied_req &= ~NVFB_WINDOW_CAP_TILED;
        need_convert = 1;
        /* Pitch if window does not support tiled. Possibly override
         * layout setting for fast rotation */
        layout = NvRmSurfaceLayout_Pitch;
    }

    /* If the window doesn't support tiled, ensure the scratch buffer
     * is pitch linear.
     */
    if (!(dpy->caps.window_cap[window].cap & NVFB_WINDOW_CAP_TILED)) {
        layout = NvRmSurfaceLayout_Pitch;
    }

    NV_ASSERT(ll->transform >= transform);

    if (unsatisfied_req & NVFB_WINDOW_CAP_SCALE) {
        unsatisfied_req &= ~NVFB_WINDOW_CAP_SCALE;
        need_scale = 1;
    }

    /* Bail out if a window cannot be offered even with 2D support. */
    if (unsatisfied_req) {
        prepare->window_mask |= 1 << window;
        return -1;
    }

    if (need_scale || need_convert || transform) {
        int w, h;
        NvRect crop;

        /* If a scaling blit is required, scale only the source crop.
         * This is an optimization for apps which render at smaller
         * than screen resolution and depend on display to scale the
         * result.
         *
         * For rotate-only blits, rotate the entire surface.  This is
         * an optimization for static wallpaper which pans a
         * screen-sized source crop to effect parallax scrolling.  If
         * we rotate only the source crop, we end up blitting every
         * frame even though the base image never changes.
         *
         * There remains a small chance we'll pick the wrong path
         * here: if rotation-only is required for an app which is
         * animating, and the source crop is smaller than the surface,
         * we might have been better off with the crop mode.  This
         * seems unlikely in practice, and we could probably improve
         * the heuristic by applying the uncropped blit only when the
         * source surface exceeds the display size.  Unless and until
         * this is a problem in practice, we'll keep the heuristic
         * simple. In the worst case, picking the wrong path is a
         * performance bug.
         */
        if (need_scale) {
            w = scale_w * r_width(&ll->src);
            h = scale_h * r_height(&ll->src);
            r_copy(&crop, &ll->src);
        } else {
            w = surf->Width;
            h = surf->Height;
        }

        map->scratch = scratch_assign(dpy, transform,
            w, h, format, layout,
            need_scale ? &crop : NULL, hwc_layer_is_protected(l));

        if (map->scratch) {
            map->surf_index = ll->surf_index;

            if (need_scale) {
                ll->src.left = 0;
                ll->src.top = 0;
                ll->src.right = w;
                ll->src.bottom = h;
            }
            hwc_transform_rect(transform, &ll->src, &ll->src, w, h);
        } else {
            prepare->window_mask |= 1 << window;
            return -1;
        }
    } else {
        map->scratch = NULL;

        if (NVGR_USE_TRIPLE_BUFFERING) {
            l->hints |= HWC_HINT_TRIPLE_BUFFER;
        }
    }

    return window;
}

static inline NvBool can_use_cursor(struct nvhwc_context *ctx,
                                    struct nvhwc_display *dpy,
                                    struct nvhwc_prepare_layer *ll)
{
    hwc_layer_t *l = ll->layer;
    int src_w, dst_w;

    NV_ASSERT(!(l->flags & HWC_SKIP_LAYER));

    if (!l->handle) {
        return NV_FALSE;
    }

    if (hwc_layer_is_protected(l)) {
        return NV_FALSE;
    }

    if (ll->blending == HWC_BLENDING_NONE) {
        return NV_FALSE;
    }

    if (r_width(&ll->dst)  > (int) dpy->caps.max_cursor ||
        r_height(&ll->dst) > (int) dpy->caps.max_cursor) {
        return NV_FALSE;
    }

    if (dpy->type == HWC_DISPLAY_EXTERNAL && !(ctx->mirror.enable)) {
        return NV_FALSE;
    }

    return NV_TRUE;
}

static void prepare_cursor(struct nvhwc_display *dpy,
                           struct nvhwc_prepare_state *prepare)
{
    struct nvfb_window *win = &dpy->conf.cursor;

    if (prepare->cursor_layer) {
        struct nvhwc_prepare_layer *ll = prepare->cursor_layer;

        window_attrs(dpy, win, &dpy->cursor.map, ll, NV_FALSE);
        ll->layer->compositionType = HWC_OVERLAY;

        win->window_index = 0;
        dpy->cursor.map.index = ll->index;
    } else {
        win->window_index = -1;
        dpy->cursor.map.index = -1;
    }
}

/* Find a layer with the specified usage.  If unique is TRUE, ensure
 * that only a single layer has the specified usage; finding multiple
 * such layers is the same as finding none.  If unique is FALSE,
 * return the first matching layer found, searching from bottom to
 * top.
 */

int hwc_find_layer(hwc_display_contents_t *list, int usage, NvBool unique)
{
    int ii;
    int found = -1;

    for (ii = 0; ii < (int) list->numHwLayers; ii++) {
        hwc_layer_t* cur = &list->hwLayers[ii];

        if ((cur->flags & HWC_SKIP_LAYER) ||
            (cur->compositionType == HWC_FRAMEBUFFER_TARGET))
            continue;

        if (hwc_get_usage(cur) & usage) {
            if (unique) {
                if (found >= 0) {
                    return -1;
                }
                found = ii;
            } else {
                return ii;
            }
        }
    }

    return found;
}

static void sysfs_write_string(const char *pathname, const char *buf)
{
    int fd = open(pathname, O_WRONLY);
    if (fd >= 0) {
        write(fd, buf, strlen(buf));
        close(fd);
    }
}

/* Check if any layers might trigger an overlapping blend with the
 * base layer.
 *
 * Return an index of the overlapping layer if detected, 0 otherwise.
 */
static int test_overlapping_blend(struct nvhwc_display *dpy,
                                  struct nvhwc_prepare_state *prepare,
                                  int base)
{
    int ii;
    hwc_rect_t blend_area;

    /* Optimization: if there is only one layer above the base layer
     * and it does not blend, allow it to overlap even if it might
     * composite.  In this case blending will be disabled for the
     * framebuffer.
     */
    if (base == prepare->numLayers - 2) {
        hwc_layer_t* cur = prepare->layers[base+1].layer;

        NV_ASSERT(!(cur->flags & HWC_SKIP_LAYER));
        if (cur->blending == HWC_BLENDING_NONE) {
            return 0;
        }
    }

    /* Optimization: if there is only one layer behind the base layer
     * and the base layer uses PREMULT, intersect the base with the
     * bottom layer because it may not cover the entire screen.  Any
     * region where the two layers do not intersect can be considered
     * non-blending.
     */
    if (base == 1 &&
        prepare->layers[base].blending == HWC_BLENDING_PREMULT) {
        r_intersection(&blend_area,
                       &prepare->layers[0].dst,
                       &prepare->layers[1].dst);
    } else {
        blend_area = prepare->layers[base].dst;
    }

    /* Check if any layers overlap the base even if they are not
     * blending because any layer might cause compositing.
     */
    for (ii = base + 1; ii < prepare->numLayers; ii++) {
        struct nvhwc_prepare_layer *ll = &prepare->layers[ii];
        hwc_layer_t* cur = ll->layer;
        hwc_rect_t clipped;

        NV_ASSERT(cur->compositionType != HWC_FRAMEBUFFER_TARGET);
        NV_ASSERT(!(cur->flags & HWC_SKIP_LAYER));

        r_intersection(&clipped, &dpy->device_clip, &ll->dst);
        if (r_intersect(&blend_area, &clipped)) {
            return ii;
        }
        r_grow(&blend_area, &clipped);
    }

    return 0;
}

/* Record a layer which has requested a framebuffer clear */
static inline void clear_layer_add(struct nvhwc_display *dpy, int ii)
{
    /* Note that failing to record a clear layer is non-fatal since
     * this is merely an optimization.  If there are more than 32
     * layers, performance may be an issue either way.
     */
    if (ii < (int)sizeof(dpy->clear_layers) * 8) {
        dpy->clear_layers |= (1 << ii);
    }
}

/* Request that SurfaceFlinger enable clears for the regions
 * corresponding to layers in windows.
 */
void hwc_clear_layer_enable(struct nvhwc_display *dpy,
                            hwc_display_contents_t *display)
{
    int ii = 0;
    uint32_t mask = dpy->clear_layers;

    while (mask) {
        uint32_t bit = (1 << ii);
        if (mask & bit) {
            hwc_layer_t *cur = &display->hwLayers[ii];
            NV_ASSERT(cur->compositionType == HWC_OVERLAY);
            cur->hints |= HWC_HINT_CLEAR_FB;
            mask &= ~bit;
        }
        ii++;
    }
}

/* Request that SurfaceFlinger disable clears for the regions
 * corresponding to layers in windows.
 */
static void clear_layer_disable(struct nvhwc_display *dpy,
                                hwc_display_contents_t *display)
{
    int ii = 0;
    uint32_t mask = dpy->clear_layers;

    while (mask) {
        uint32_t bit = (1 << ii);
        if (mask & bit) {
            hwc_layer_t *cur = &display->hwLayers[ii];
            NV_ASSERT(cur->compositionType == HWC_OVERLAY);
            cur->hints &= ~HWC_HINT_CLEAR_FB;
            mask &= ~bit;
        }
        ii++;
    }
}

/* Determined if any framebuffer layers have changed since the last
 * frame.
 */
static int fb_cache_clean(struct nvhwc_fb_cache *cache, hwc_display_contents_t* list)
{
    uintptr_t diffs = 0;
    int ii;

    /* Compute a quick comparison of the buffer handles */
    for (ii = 0; ii < cache->numLayers; ii++) {
        int index = cache->layers[ii].index;
        hwc_layer_t* cur = &list->hwLayers[index];

        diffs |= (uintptr_t)cache->layers[ii].handle ^ (uintptr_t)cur->handle;
        cache->layers[ii].handle = cur->handle;
    }

    return !diffs;
}

/* Determine the compositing strategy for this frame and set the layer
 * bits accordingly.  If no composited layers have changed, we can
 * reuse the previous composited result.
 */
static void fb_cache_check(struct nvhwc_display *dpy,
                           hwc_display_contents_t* list)
{
    struct nvhwc_fb_cache *cache = &dpy->fb_cache[OLD_CACHE_INDEX(dpy)];
    int recycle = cache->numLayers && fb_cache_clean(cache, list);

    if (dpy->fb_recycle != recycle) {
        if (recycle) {
            FBCLOG("enabled");
        } else {
            FBCLOG("disabled");
        }

        /* Modify layer state only if we're using surfaceflinger */
        if (dpy->composite.id == HWC_Compositor_SurfaceFlinger) {
            int ii;

            NV_ASSERT(!dpy->composite.contents.numLayers);

            if (recycle) {
                for (ii = 0; ii < cache->numLayers; ii++) {
                    int index = cache->layers[ii].index;
                    list->hwLayers[index].compositionType = HWC_OVERLAY;
                }
                clear_layer_disable(dpy, list);
            } else {
                for (ii = 0; ii < cache->numLayers; ii++) {
                    int index = cache->layers[ii].index;
                    list->hwLayers[index].compositionType = HWC_FRAMEBUFFER;
                }
                hwc_clear_layer_enable(dpy, list);
            }
        }

        dpy->fb_recycle = recycle;
    }
}

/* Add a layer to the fb_cache */
static void fb_cache_add_layer(struct nvhwc_display *dpy,
                               hwc_layer_t *cur,
                               int li)
{
    struct nvhwc_fb_cache *cache = &dpy->fb_cache[NEW_CACHE_INDEX(dpy)];

    if (0 <= cache->numLayers && cache->numLayers < FB_CACHE_LAYERS) {
        /* Bug 983744 - Disable the fb cache if any layers have the
         * SKIP flag, in which case the rest of the hwc_layer_t is
         * undefined so we cannot reliably determine if it has
         * changed.
         */
        if (cur->flags & HWC_SKIP_LAYER) {
            cache->numLayers = -1;
            return;
        } else {
            int index = cache->numLayers++;

            cache->layers[index].index = li;
            cache->layers[index].handle = cur->handle;
            cache->layers[index].transform = cur->transform;
            cache->layers[index].blending = cur->blending;
            cache->layers[index].sourceCrop = cur->sourceCrop;
            cache->layers[index].displayFrame = cur->displayFrame;
        }
    }
}

static void fb_cache_update_layers(struct nvhwc_display *dpy,
                                   hwc_display_contents_t *display)
{
    struct nvhwc_fb_cache *cache = &dpy->fb_cache[OLD_CACHE_INDEX(dpy)];
    int ii;

    NV_ASSERT(dpy->fb_recycle);

    /* hwc_prepare marked these layers as FRAMEBUFFER,
     * reset to OVERLAY
     */
    for (ii = 0; ii < cache->numLayers; ii++) {
        int index = cache->layers[ii].index;
        NV_ASSERT(display->hwLayers[index].compositionType ==
                  HWC_FRAMEBUFFER);
        display->hwLayers[index].compositionType = HWC_OVERLAY;
    }
}

/* Check if previous config's fb_cache is still valid in this config */
static void fb_cache_validate(struct nvhwc_display *dpy,
                              struct nvhwc_prepare_state *prepare)
{
    struct nvhwc_fb_cache *old_cache = &dpy->fb_cache[OLD_CACHE_INDEX(dpy)];
    struct nvhwc_fb_cache *new_cache = &dpy->fb_cache[NEW_CACHE_INDEX(dpy)];

    new_cache->compositor = dpy->composite.id;

    /* Check if we can use the cache at all */
    if (dpy->fb_layers != new_cache->numLayers ||
        new_cache->compositor != old_cache->compositor) {
        if (dpy->fb_layers > FB_CACHE_LAYERS) {
            FBCLOG("de-activated, (%d layers exceeds limit)", dpy->fb_layers);
        }
        old_cache->numLayers = 0;
        dpy->fb_recycle = 0;
        return;
    }

    NV_ASSERT(old_cache->numLayers >= 0 &&
              new_cache->numLayers >= 0);

    /* Check if the old config is still valid */
    if (old_cache->numLayers && old_cache->numLayers == new_cache->numLayers) {
        int match = 1;
        int ii;

        for (ii = 0; ii < old_cache->numLayers; ii++) {
            struct nvhwc_fb_cache_layer *a = &old_cache->layers[ii];
            struct nvhwc_fb_cache_layer *b = &new_cache->layers[ii];

            /* Check all fields except the handle */
            if (a->index != b->index ||
                a->transform != b->transform ||
                a->blending != b->blending ||
                !r_equal(&a->sourceCrop, &b->sourceCrop) ||
                !r_equal(&a->displayFrame, &b->displayFrame)) {
                match = 0;
                break;
            }
        }

        /* If the configs match, check the handles and return */
        if (match) {
            dpy->fb_recycle = fb_cache_clean(old_cache, prepare->display);
            /* Updating the layer state is deferred until composition
             * validation is done.
             */
            if (dpy->fb_recycle) {
                FBCLOG("re-using previous config (enabled)");
            } else {
                FBCLOG("re-using previous config (disabled)");
            }
            return;
        }
    }

    if (new_cache->numLayers) {
        if (!old_cache->numLayers) {
            FBCLOG("activated (%d layers)", new_cache->numLayers);
        }
    } else if (old_cache->numLayers) {
        FBCLOG("de-activated (0 layers)");
    }

    /* Swap to the new cache and disable it */
    dpy->fb_cache_index = NEW_CACHE_INDEX(dpy);
    dpy->fb_recycle = 0;
}

static void assign_framebuffer(struct nvhwc_display *dpy,
                               struct nvhwc_prepare_state *prepare)
{
    unsigned req = dpy->fb_req |
        (prepare->sequential ? NVFB_WINDOW_CAP_SEQUENTIAL : 0);
    int window = hwc_get_window(&dpy->caps, &prepare->window_mask, req, NULL);
    int force_bounds = 0;
    struct nvhwc_prepare_map *layer_map;

    while (window == -1) {
        struct nvhwc_prepare_layer *ll;

        /* Some of the window assignments may need to be rolled back */
        layer_map = &prepare->layer_map[++prepare->current_map];
        int w = layer_map->window;
        prepare->window_mask |= (1 << w);
        if (!((req ^ dpy->caps.window_cap[w].cap) & req)) {
            window = w;
        }
        ll = &prepare->layers[layer_map->layer_index];
        /* Restore src rect possibly modified for scratch blit */
        ll->src = ll->layer->sourceCrop;
#if DC_WAR_1325061
        if (((NvNativeHandle *)ll->layer->handle)->Type == NV_NATIVE_BUFFER_YUV) {
            prepare_adjust_yuv_crop(ll);
        }
#endif
        /* Add the rolled back layer to the framebuffer list */
        ll->layer->compositionType = HWC_FRAMEBUFFER;
        dpy->fb_layers++;
        fb_cache_add_layer(dpy, prepare->display->hwLayers + ll->index, ll->index);
        force_bounds = 1;
    }

    dpy->fb_index = prepare->current_map--;

    layer_map = &prepare->layer_map[dpy->fb_index];
    layer_map->window = window;

    if (prepare->remainder.limit || force_bounds) {
        prepare->fb.bounds = dpy->layer_clip;
    } else {
        prepare->fb.bounds.left = 0;
        prepare->fb.bounds.top = 0;
        prepare->fb.bounds.right = 0;
        prepare->fb.bounds.bottom = 0;
    }
    prepare->fb.blending = HWC_BLENDING_PREMULT;

    dpy->map[dpy->fb_index].index = dpy->fb_target;
    dpy->map[dpy->fb_index].scratch = NULL;
}

static void add_to_framebuffer(struct nvhwc_display *dpy,
                               struct nvhwc_prepare_state *prepare,
                               hwc_layer_t *cur,
                               int index)
{
    if (dpy->fb_index == -1) {
        NV_ASSERT(0 == dpy->fb_layers);
        assign_framebuffer(dpy, prepare);
    }

    cur->compositionType = HWC_FRAMEBUFFER;
    if (cur->flags & HWC_SKIP_LAYER) {
        prepare->fb.bounds = dpy->layer_clip;
    } else {
        r_grow(&prepare->fb.bounds, &cur->displayFrame);
        r_clip(&prepare->fb.bounds, &dpy->layer_clip);
    }

    dpy->fb_layers++;
    fb_cache_add_layer(dpy, cur, index);
}

/* Utilities from pthread_internal.h that should really be exposed */
static __inline__ void timespec_add( struct timespec*  a, const struct timespec*  b )
{
    a->tv_sec  += b->tv_sec;
    a->tv_nsec += b->tv_nsec;
    if (a->tv_nsec >= 1000000000) {
        a->tv_nsec -= 1000000000;
        a->tv_sec  += 1;
    }
}

static  __inline__ void timespec_sub( struct timespec*  a, const struct timespec*  b )
{
    a->tv_sec  -= b->tv_sec;
    a->tv_nsec -= b->tv_nsec;
    if (a->tv_nsec < 0) {
        a->tv_nsec += 1000000000;
        a->tv_sec  -= 1;
    }
}

static  __inline__ void timespec_zero( struct timespec*  a )
{
    a->tv_sec = a->tv_nsec = 0;
}

static  __inline__ int timespec_is_zero( const struct timespec*  a )
{
    return (a->tv_sec == 0 && a->tv_nsec == 0);
}

static  __inline__ int timespec_cmp( const struct timespec*  a, const struct timespec*  b )
{
    if (a->tv_sec  < b->tv_sec)  return -1;
    if (a->tv_sec  > b->tv_sec)  return +1;
    if (a->tv_nsec < b->tv_nsec) return -1;
    if (a->tv_nsec > b->tv_nsec) return +1;
    return 0;
}

#define timespec_to_ms(t) (t.tv_sec*1000 + t.tv_nsec/1000000)
#define timespec_to_ns(t) ((int64_t)t.tv_sec*1000000000 + t.tv_nsec)

static inline void enable_idle_timer(struct nvhwc_idle_machine *idle)
{
    if (idle->timer) {
        struct itimerspec its;
        timespec_zero(&its.it_interval);
        its.it_value = idle->timeout;
        timer_settime(idle->timer, TIMER_ABSTIME, &its, NULL);
    }
}

static inline void disable_idle_timer(struct nvhwc_idle_machine *idle)
{
    if (idle->timer) {
        struct itimerspec its;
        timespec_zero(&its.it_interval);
        timespec_zero(&its.it_value);
        timer_settime(idle->timer, 0, &its, NULL);
    }
}

static int update_idle_state(struct nvhwc_context* ctx)
{
    struct timespec now;
    int composite;
    int changed;
    int is_idle;
    char buff[20];
    struct nvhwc_idle_machine *idle = &ctx->idle;

    if (!ctx->props.dynamic.idle_minimum_fps) {
        return 0;
    }

    if (ctx->idle.minimum_fps != ctx->props.dynamic.idle_minimum_fps) {
        struct timespec *time_limit = (struct timespec *) &ctx->idle.time_limit;
        time_t time_limit_ms =
            IDLE_FRAME_COUNT * 1000 / ctx->props.dynamic.idle_minimum_fps;
        time_limit->tv_sec = time_limit_ms / 1000;
        time_limit->tv_nsec = (time_limit_ms * 1000000) % 1000000000;
        ctx->idle.minimum_fps = ctx->props.dynamic.idle_minimum_fps;
    }

    /* Get current time and test against previous timeout */
    clock_gettime(CLOCK_MONOTONIC, &now);
    is_idle = timespec_cmp(&now, &idle->timeout) > 0;

    /* If hwc is idle and disp is not set to idle mode, change
     * refresh to minimum rate.
     * If hwc is not idle and disp is set to idle mode, change
     * refresh to normal rate. */
    if (is_idle) {
        nvfb_update_nvdps(ctx->fb, NvDPS_Idle);
    } else {
        nvfb_update_nvdps(ctx->fb, NvDPS_Nominal);
    }

    composite = is_idle && idle->enabled;
    changed = composite != idle->composite;
    idle->composite = composite;

#ifdef DEBUG_IDLE
    if (changed) {
        struct timespec elapsed = now;
        timespec_sub(&elapsed, &idle->frame_times[idle->frame_index]);
        IDLELOG("windows %s, fps = %0.1f",
                composite ? "disabled" : "enabled",
                (1000.0 / timespec_to_ms(elapsed)) * IDLE_FRAME_COUNT);
    }
#endif

    idle->frame_times[idle->frame_index] = now;
    idle->frame_index = (idle->frame_index+1) % IDLE_FRAME_COUNT;

    /* timeout is the absolute time limit for the next frame */
    idle->timeout = idle->frame_times[idle->frame_index];
    timespec_add(&idle->timeout, &idle->time_limit);

    /* Update the timer */
    if (idle->enabled) {
        if (composite) {
            disable_idle_timer(idle);
        } else {
            enable_idle_timer(idle);
        }
    }

    if (changed) {
        IDLELOG("state changed, composite = %d", idle->composite);
    }
    return changed;
}

static void enable_idle_detection(struct nvhwc_context* ctx)
{
    struct nvhwc_idle_machine *idle = &ctx->idle;

    if (!idle->enabled && ctx->props.dynamic.idle_minimum_fps) {
        IDLELOG("Enabling idle detection");
        android_atomic_release_store(1, &idle->enabled);
        enable_idle_timer(idle);
    }
}

static void disable_idle_detection(struct nvhwc_context* ctx)
{
    struct nvhwc_idle_machine *idle = &ctx->idle;

    if (idle->enabled) {
        IDLELOG("Disabling idle detection");
        android_atomic_release_store(0, &idle->enabled);
        disable_idle_timer(idle);
    }
}

/* Return NV_TRUE if any configured windows overlap each other */
static NvBool detect_overlap(struct nvhwc_display *dpy, int start)
{
    size_t ii, jj;

    for (ii = start; ii < dpy->caps.num_windows; ii++) {
        NV_ASSERT((int)ii == dpy->fb_index || dpy->map[ii].index != -1);

        for (jj = ii + 1; jj < dpy->caps.num_windows; jj++) {
            NV_ASSERT((int)jj == dpy->fb_index || dpy->map[jj].index != -1);

            if (nv_intersect(&dpy->conf.overlay[ii].dst,
                             &dpy->conf.overlay[jj].dst)) {
                return NV_TRUE;
            }
        }
    }

    return NV_FALSE;
}

static void add_window(struct nvhwc_display *dpy,
                       struct nvhwc_prepare_state *prepare,
                       int ii, int window)
{
    prepare->layer_map[prepare->current_map].window = window;
    prepare->layer_map[prepare->current_map].layer_index = ii;
    prepare->current_map--;
    if (dpy->caps.window_cap[window].cap & NVFB_WINDOW_CAP_SEQUENTIAL) {
        prepare->sequential = NV_TRUE;
        prepare->free_sequential--;
    }
}

static void complete_add_windows(struct nvhwc_display *dpy,
                                 struct nvhwc_prepare_state *prepare)
{
    int ii;

    for (ii = (int)dpy->caps.num_windows - 1;
         ii > prepare->current_map && ii > dpy->fb_index; ii--) {
        struct nvhwc_prepare_map *map = &prepare->layer_map[ii];
        struct nvhwc_prepare_layer *ll = &prepare->layers[map->layer_index];
        hwc_layer_t* cur = ll->layer;

        cur->compositionType = HWC_OVERLAY;
        cur->hints |= HWC_HINT_CLEAR_FB;
        clear_layer_add(dpy, ll->index);
        dpy->map[ii].index = ll->index;
    }
}

static int hwc_assign_windows(struct nvhwc_context *ctx,
                              struct nvhwc_display *dpy,
                              struct nvhwc_prepare_state *prepare)
{
    int blends_overlap = -1;
    int first_sequential = -1;
    int ii;

    prepare->free_sequential = dpy->caps.num_seq_windows;

    /* Attempt to assign layers to windows from back to front.  If a
     * layer must be composited, all remaining layers will be
     * composited for simplicitly.
     */
    for (ii = 0; ii < prepare->numLayers; ii++) {
        struct nvhwc_prepare_layer *ll = &prepare->layers[ii];
        hwc_layer_t *cur = ll->layer;
        int window = -1;
        int usage = hwc_get_usage(cur);

        /* These layer types should have been removed during
         * pre-processing step.
         */
        NV_ASSERT(cur->compositionType != HWC_FRAMEBUFFER_TARGET);
        NV_ASSERT(!(cur->flags & HWC_SKIP_LAYER));

        if (ii == first_sequential) {
            prepare->sequential = NV_TRUE;
        }

        if (!prepare->use_windows && cur != prepare->priority_layer) {
            add_to_framebuffer(dpy, prepare, cur, ll->index);
            continue;
        }

        /* Disable blending if this is the base layer and PREMULT is
         * requested.
         */
        if (prepare->current_map == (int)dpy->caps.num_windows - 1 &&
            ll->blending == HWC_BLENDING_PREMULT) {
            ll->blending = HWC_BLENDING_NONE;
        }

        /* If blending, test for overlapping blends which are not
         * supported by all the windows of the display controller.
         * If detected, all subsequent layers may need to be composited.
         */
        if (!prepare->sequential && ll->blending != HWC_BLENDING_NONE) {
            if (blends_overlap < 0) {
                /* If there are more remaining layers than windows,
                 * composite the rest.  Otherwise test for overlap.
                 */
                if ((prepare->current_map + 1 < prepare->numLayers - ii) ||
                    prepare->remainder.limit) {
                    blends_overlap = 1;
                } else {
                    blends_overlap = test_overlapping_blend(dpy, prepare, ii);
                    if (blends_overlap && prepare->free_sequential) {
                        first_sequential = blends_overlap;
                        blends_overlap = 0;
                    }
                }
                if (blends_overlap) {
                    prepare->use_windows = 0;
                    add_to_framebuffer(dpy, prepare, cur, ll->index);
                    continue;
                }
            }
        }

        if (window_is_available(dpy, prepare, ii)) {
            window = assign_window(ctx, dpy, prepare, ii);
        }

        if (window == -1) {
            prepare->use_windows = 0;
            add_to_framebuffer(dpy, prepare, cur, ll->index);
        } else {
            add_window(dpy, prepare, ii, window);
        }
    }

    /* Special case: if there are no layers, assign the
     * FRAMEBUFFER_TARGET to a window.
     */
    if (!prepare->numLayers) {
        hwc_composite_select(ctx, dpy, HWC_Compositor_SurfaceFlinger);
        assign_framebuffer(dpy, prepare);
        prepare->fb.bounds = dpy->layer_clip;
    }

    /* Composite any layers which were not included in the prepare_state */
    for (ii = prepare->remainder.start; ii < prepare->remainder.limit; ii++) {
        hwc_layer_t *cur = &prepare->display->hwLayers[ii];
        if (cur != prepare->priority_layer &&
            cur->compositionType != HWC_FRAMEBUFFER_TARGET) {
            add_to_framebuffer(dpy, prepare, cur, ii);
        }
    }

    complete_add_windows(dpy, prepare);

    /* Disable unused windows */
    for (ii = prepare->current_map; ii >= 0; ii--) {
        prepare->layer_map[ii].window =
            hwc_get_window(&dpy->caps, &prepare->window_mask, 0, NULL);
        dpy->map[ii].index = -1;
    }

    /* Handle cursor if present */
    prepare_cursor(dpy, prepare);

    if (ctx->props.dynamic.composite_policy & HWC_CompositePolicy_FbCache) {
        /* Setup the fb_cache for this config */
        fb_cache_validate(dpy, prepare);
    }

    if (!dpy->fb_recycle && dpy->composite.scratch) {
        dpy->scratch->unlock(dpy->scratch, dpy->composite.scratch);
        dpy->composite.scratch = NULL;
    }

    if (dpy->fb_index != -1 &&
        dpy->composite.id != HWC_Compositor_SurfaceFlinger) {
        /* Try to use a local compositor */
        hwc_composite_prepare(ctx, dpy, prepare);
    }

    if (ctx->props.dynamic.composite_policy & HWC_CompositePolicy_FbCache) {
        if (dpy->fb_recycle) {
            fb_cache_update_layers(dpy, prepare->display);
        }
    }

    /* Clear the CLEAR_FB flag if we're not actually compositing, or
     * if composition is handled internally; otherwise SurfaceFlinger
     * will perform needless clears on the framebuffer and waste
     * power/bandwidth.
     */
    if (dpy->fb_index == -1 ||
        dpy->composite.contents.numLayers ||
        dpy->fb_recycle) {
        clear_layer_disable(dpy, prepare->display);
    }

    /* Initialize window config */
    conf_prepare(ctx, dpy, prepare);

    // TODO any meaninful value to return?
    return 0;
}

void hwc_transform_rect(int transform, const hwc_rect_t *src, hwc_rect_t *dst,
                        int width, int height)
{
    *dst = *src;

    if (transform & HWC_TRANSFORM_FLIP_H) {
        int l = width - dst->right;
        int r = width - dst->left;
        dst->left  = l;
        dst->right = r;
    }

    if (transform & HWC_TRANSFORM_FLIP_V) {
        int t = height - dst->bottom;
        int b = height - dst->top;
        dst->top    = t;
        dst->bottom = b;
    }

    if (transform & HWC_TRANSFORM_ROT_90) {
        int l = height - dst->bottom;
        int t = dst->left;
        int r = height - dst->top;
        int b = dst->right;

        dst->left   = l;
        dst->top    = t;
        dst->right  = r;
        dst->bottom = b;
    }
}

static inline void prepare_copy_layer(struct nvhwc_display *dpy,
                                      struct nvhwc_prepare_layer *ll,
                                      hwc_layer_t *cur, int index)
{
    ll->index = index;
    ll->layer = cur;
    ll->blending = cur->blending;
    ll->surf_index = 0;
    ll->src = cur->sourceCrop;
    if (dpy->transform) {
        ll->transform = combine_transform(cur->transform,
                                          dpy->transform);
        hwc_transform_rect(dpy->transform, &cur->displayFrame, &ll->dst,
                           dpy->config.res_x, dpy->config.res_y);
    } else {
        ll->dst = cur->displayFrame;
        ll->transform = cur->transform;
    }
}

#if DC_WAR_1325061
static void prepare_adjust_yuv_crop(struct nvhwc_prepare_layer *ll)
{
    NvNativeHandle *h = (NvNativeHandle *) ll->layer->handle;
    switch (h->Format) {
    // 420 formats
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case NVGR_PIXEL_FORMAT_YUV420:
    case NVGR_PIXEL_FORMAT_YUV420_NV12:
    case NVGR_PIXEL_FORMAT_NV12:
        ll->src.left = ll->src.left & ~1;
        ll->src.right = (ll->src.right + 1) & ~1;
        ll->src.top = ll->src.top & ~1;
        ll->src.bottom = (ll->src.bottom + 1) & ~1;
        break;

    // 422 formats
    case HAL_PIXEL_FORMAT_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
    case NVGR_PIXEL_FORMAT_UYVY:
    case NVGR_PIXEL_FORMAT_YUV422:
    case NVGR_PIXEL_FORMAT_NV16:
        ll->src.left = ll->src.left & ~1;
        ll->src.right = (ll->src.right + 1) & ~1;
        break;

    // 422R formats
    case NVGR_PIXEL_FORMAT_YUV422R:
        ll->src.top = ll->src.top & ~1;
        ll->src.bottom = (ll->src.bottom + 1) & ~1;
        break;

    default:
        NV_ASSERT(!"Unknown format");
        break;
    }
}
#endif

void hwc_prepare_add_layer(struct nvhwc_display *dpy,
                           struct nvhwc_prepare_state *prepare,
                           hwc_layer_t *cur, int index)
{
    struct nvhwc_prepare_layer *ll = &prepare->layers[prepare->numLayers++];
    NvNativeHandle *h;

    prepare_copy_layer(dpy, ll, cur, index);
    if (prepare->modification.type == HWC_Modification_Xform) {
        xform_apply(&prepare->modification.xf, &ll->dst);
    }

#if DC_WAR_1325061
    h = (NvNativeHandle *) ll->layer->handle;
    if (h->Type == NV_NATIVE_BUFFER_YUV) {
        prepare_adjust_yuv_crop(ll);
    }
#endif
}

static void hwc_prepare_begin(struct nvhwc_context *ctx,
                              struct nvhwc_display *dpy,
                              struct nvhwc_prepare_state *prepare,
                              hwc_display_contents_t *display,
                              NvBool force_composite)
{
    size_t ii;
    int priority = -1;
    uint_t display_needs = 0;

    /* Reset window assignment state */
    dpy->fb_index = -1;
    dpy->fb_target = -1;
    dpy->fb_layers = 0;
    dpy->fb_cache[NEW_CACHE_INDEX(dpy)].numLayers = 0;
    dpy->clear_layers = 0;
    // TODO is this needed?
    dpy->composite.contents.numLayers = 0;

    memset(prepare, 0, sizeof(struct nvhwc_prepare_state));

    if (force_composite ||
        ctx->props.dynamic.composite_policy & HWC_CompositePolicy_ForceComposite) {
        prepare->use_windows = NV_FALSE;
    } else if (ctx->props.dynamic.composite_policy & HWC_CompositePolicy_CompositeOnIdle) {
        prepare->use_windows = !ctx->idle.composite ||
            /* Always use windows for the first new frame of a config
             * so we can accurately determine overlap
             */
            (display->flags & HWC_GEOMETRY_CHANGED);

        /* Bug 1271434 - Clear the idle composition flag when it is
         * overridden by a geometry change, so that a subsequent idle
         * frame toggles the flag and triggers re-evaluation of window
         * state.  Otherwise the idle machine and the actual window
         * config may be out of sync.
         */
        if (ctx->idle.composite && prepare->use_windows) {
            IDLELOG("geometry changed, overriding composition");
            ctx->idle.composite = 0;
        }
    } else {
        prepare->use_windows = NV_TRUE;
    }

    prepare->current_map = (int)dpy->caps.num_windows - 1;
    prepare->window_mask = (1 << dpy->caps.num_windows) - 1;
    prepare->free_sequential = dpy->caps.num_seq_windows;

    for (ii = 0; ii < dpy->caps.num_windows; ii++) {
        prepare->layer_map[ii].layer_index = -1;
    }
    prepare->fb.layer.index = -1;
    prepare->display = display;
    prepare->modification.type = HWC_Modification_None;

    /* If mirroring, look for layers which might require special
     * treatment on the external display.
     */
    if (dpy->type == HWC_DISPLAY_EXTERNAL && ctx->mirror.enable) {
        if (hwc_prepare_external(ctx, dpy, prepare, display)) {
            scratch_frame_start(dpy);
            return;
        }
    }

    prepare->modification = dpy->modification;
    if (prepare->modification.type == HWC_Modification_Xform) {
        display_needs |= NVFB_DISPLAY_CAP_SCALE;
    }

    /* Determine framebuffer transform requirements */
    if (dpy->transform & HWC_TRANSFORM_ROT_90) {
        display_needs |= NVFB_DISPLAY_CAP_ROTATE;
    }

    /* clear requirements provided by display */
    display_needs &= ~dpy->caps.display_cap;

    /* Set default compositor */
    hwc_composite_select(ctx, dpy, ctx->props.dynamic.compositor);

    /* Override compositor if necessary */
    if (display_needs &&
        (hwc_composite_caps(ctx, dpy) & NVCOMPOSER_CAP_TRANSFORM) == 0) {
        hwc_composite_override(ctx, dpy, HWC_Compositor_Gralloc);
    }

    /* Find a protected layer if one exists */
    priority = hwc_find_layer(display, GRALLOC_USAGE_PROTECTED, NV_FALSE);
    if (priority >= 0) {
        NvBool need_gralloc;
        prepare->protect = NV_TRUE;

        if (hwc_composite_caps(ctx, dpy) & NVCOMPOSER_CAP_PROTECT) {
            need_gralloc = NV_FALSE;
        } else {
            need_gralloc = NV_TRUE;
        }

        /* Bug 1010997 - prioritize the first protected layer */
        if (dpy->caps.num_windows > 1) {
            hwc_layer_t *cur = &display->hwLayers[priority];

            /* Windows are assigned from back to front.  If the layer
             * is opaque it may be safely assigned the first layer
             * since by definition it occludes anything behind it.
             */
            if (priority == 0 || cur->blending == HWC_BLENDING_NONE) {
                hwc_prepare_add_layer(dpy, prepare, cur, priority);
                /* If there are layers behind this one, we must ensure
                 * that no occluded layers are subsequently assigned
                 * windows in front of this one.  Force compositing
                 * for all other layers which renders only the visible
                 * region of each layer.
                 */
                if (priority > 0) {
                    prepare->use_windows = NV_FALSE;
                }
                /* Save the layer pointer to override forced composition */
                prepare->priority_layer = cur;
                need_gralloc = NV_FALSE;
            }
        }

        /* If we cannot assign the protected layer to a window,
         * override the requested compositor to ensure we can
         * handle this case.
         */
        if (need_gralloc) {
            hwc_composite_override(ctx, dpy, HWC_Compositor_Gralloc);
        }
    }

    /* Pre-process layer list */
    for (ii = 0; ii < display->numHwLayers; ii++) {
        hwc_layer_t *cur = &display->hwLayers[ii];

        /* The priority layer has already been processed. */
        if (cur == prepare->priority_layer) {
            continue;
        }

        /* Save the framebuffer target index but do not process with
         * normal layers.
         */
        if (cur->compositionType == HWC_FRAMEBUFFER_TARGET) {
            NV_ASSERT(dpy->fb_target == -1);
            dpy->fb_target = ii;
            prepare_copy_layer(dpy, &prepare->fb.layer, cur, ii);
            continue;
        }

        /* If composition is already forced, stop processing layers.
         * Note we cannot break out of the loop before the framebuffer
         * target has been found.
         */
        if (prepare->remainder.limit) {
            prepare->remainder.limit = ii+1;
            continue;
        }

        /* If any layers are marked SKIP or if there are too many
         * layers, composite all remaining layers.
         */
        if ((cur->flags & HWC_SKIP_LAYER) ||
            prepare->numLayers == HWC_MAX_LAYERS) {
            hwc_composite_select(ctx, dpy, HWC_Compositor_SurfaceFlinger);

            if (priority >= 0 && prepare->priority_layer == NULL &&
                (hwc_composite_caps(ctx, dpy) & NVCOMPOSER_CAP_PROTECT) == 0) {
                ALOGW("WARNING: Forcing SurfaceFlinger composition with protected content.");
            }

            prepare->remainder.start = ii;
            prepare->remainder.limit = ii+1;
        } else {
            /* Prepare this layer */
            hwc_prepare_add_layer(dpy, prepare, cur, ii);
        }
    }

    /* Look for a cursor image */
    if (ctx->cursor_mode == NvFbCursorMode_Layer &&
        !prepare->remainder.limit && prepare->numLayers > 1) {
        struct nvhwc_prepare_layer *ll = &prepare->layers[prepare->numLayers-1];

        NV_ASSERT(dpy->caps.max_cursor > 0);
        if (can_use_cursor(ctx, dpy, ll)) {
            prepare->cursor_layer = ll;
            prepare->numLayers--;
        }
    }

    scratch_frame_start(dpy);
}

static void hwc_prepare_end(struct nvhwc_context *ctx,
                            struct nvhwc_display *dpy,
                            struct nvhwc_prepare_state *prepare,
                            hwc_display_contents_t *display)
{
    scratch_frame_end(dpy);

    if (display->flags & HWC_GEOMETRY_CHANGED) {
        dpy->windows_overlap = detect_overlap(dpy, prepare->current_map+1);
    }
}

static int hwc_probe_config(struct nvhwc_context *ctx,
                            hwc_display_contents_t *display,
                            int disp)
{
    struct nvfb_buffer bufs[NVFB_MAX_WINDOWS];
    struct nvhwc_display *dpy = &ctx->displays[disp];
    size_t ii;

    memset(bufs, 0, sizeof(bufs));
    for (ii = 0; ii < dpy->caps.num_windows; ii++) {
        if (dpy->map[ii].index != -1) {
            hwc_layer_t *layer = &display->hwLayers[dpy->map[ii].index];
            /* Use handle from SF to indicate that the window is used */
            bufs[ii].buffer = (NvNativeHandle *)layer->handle;
        }
    }

    if (dpy->cursor.map.index != -1) {
        hwc_layer_t *layer = &display->hwLayers[dpy->cursor.map.index];

        NV_ASSERT(ii == dpy->caps.num_windows);
        /* Use original handle to indicate that the cursor window is used */
        bufs[ii].buffer = (NvNativeHandle *)layer->handle;
    }

    if (dpy->composite.scratch) {
        NV_ASSERT(dpy->composite.contents.numLayers);
        NV_ASSERT(dpy->fb_index >= 0);

        IMPLOG("Composite Scratch");
        bufs[dpy->fb_index].buffer =
            dpy->scratch->get_buffer(dpy->scratch, dpy->composite.scratch);
    }

    return nvfb_reserve_bw(ctx->fb, disp, &dpy->conf, bufs);
}

static int hwc_prepare_display(struct nvhwc_context *ctx,
                               hwc_display_contents_t *display,
                               int disp,
                               int changed)
{
    NV_ATRACE_BEGIN(__func__);
    struct nvhwc_display *dpy = &ctx->displays[disp];

#if DEBUG_MODE
    if (ctx->props.dynamic.dump_layerlist) {
        hwc_dump_display_contents(display);
    }
#endif

    if (dpy->hotplug.cached.value != dpy->hotplug.latest.value) {
        dpy->hotplug.cached = dpy->hotplug.latest;
        changed = 1;
    }

    changed |= display->flags & HWC_GEOMETRY_CHANGED;

    /* Redo prepare if window configuration needs to be changed. */
    if (changed) {
        struct nvhwc_prepare_state prepare;
        NvBool force_composite = NV_FALSE;

        hwc_prepare_begin(ctx, dpy, &prepare, display, force_composite);
        hwc_assign_windows(ctx, dpy, &prepare);
        hwc_prepare_end(ctx, dpy, &prepare, display);

        if (!dpy->blank && (dpy->caps.display_cap & NVFB_DISPLAY_CAP_BANDWIDTH_NEGOTIATE)) {
            int ret = hwc_probe_config(ctx, display, disp);
            if (ret != 0) {
                IMPLOG("Retrying with limited configuration");
                /* Retry with limited configuration */
                force_composite = NV_TRUE;
                hwc_prepare_begin(ctx, dpy, &prepare, display, force_composite);
                hwc_assign_windows(ctx, dpy, &prepare);
                hwc_prepare_end(ctx, dpy, &prepare, display);
                ret = hwc_probe_config(ctx, display, disp);

                /*
                 * Limited config should always work during normal operation.
                 * There is a duration of time for which an external display
                 * maybe hot-plugged but the display driver has not completed
                 * its removal, for which this may fail.
                 */
                if (ret) {
                    IMPLOG("Failed prepare with limited config");
                    if (dpy->type != HWC_DISPLAY_EXTERNAL) {
                        ALOGE("Failed prepare with limited config for panel");
                        NV_ASSERT(0);
                    }
                }
            }
        }
    } else {
        if (ctx->props.dynamic.composite_policy & HWC_CompositePolicy_FbCache) {
            fb_cache_check(dpy, display);
        }
    }

    NV_ATRACE_END();
    return 0;
}

static hwc_layer_t *find_video_layer(struct nvhwc_display *dpy,
                                     hwc_display_contents_t *display)
{
    int index = hwc_find_layer(display,
                               GRALLOC_USAGE_EXTERNAL_DISP,
                               NV_TRUE);
    if (index >= 0) {
        hwc_layer_t *l = &display->hwLayers[index];

        if (hwc_layer_is_yuv(l)) {
            int video_size = r_width(&l->displayFrame) *
                             r_height(&l->displayFrame);
            int screen_size = dpy->config.res_x * dpy->config.res_y;

            if (video_size > (screen_size >> 1)) {
                return l;
            }
        }
    }

    return NULL;
}

static inline void update_didim_state(struct nvhwc_context *ctx,
                                      hwc_layer_t *video)
{
    hwc_rect_t *didim_window = video ? &video->displayFrame : NULL;

    /* Update didim for normal mode or video mode */
    nvfb_didim_window(ctx->fb, HWC_DISPLAY_PRIMARY, didim_window);
}

#if TOUCH_SLOWSCAN_ENABLE
static inline void update_touch_slowscan(struct nvhwc_context *ctx,
                                         hwc_layer_t *video)
{
    if (ctx->fd_slowscan >= 0) {
        int should_slowscan = video != NULL;

        if (should_slowscan != ctx->slowscan_shadowstate) {
            char buf[2] = { should_slowscan ? '1' : '0', '\0' };

            if (pwrite(ctx->fd_slowscan, buf, sizeof(buf), 0) <= 0) {
                // write failed, return without updating shadow state
                return;
            }

            ctx->slowscan_shadowstate = should_slowscan;
        }
    }
}
#endif

static void update_panel_state(struct nvhwc_context *ctx,
                               hwc_display_contents_t *display)
{
    struct nvhwc_display *dpy = &ctx->displays[HWC_DISPLAY_PRIMARY];

    if (dpy->blank || !display) {
        return;
    }

    if (display->flags & HWC_GEOMETRY_CHANGED) {
        hwc_layer_t *video = find_video_layer(dpy, display);

        update_didim_state(ctx, video);

#if TOUCH_SLOWSCAN_ENABLE
        update_touch_slowscan(ctx, video);
#endif
    }
}

static int hwc_prepare(hwc_composer_device_t *dev, size_t numDisplays,
                       hwc_display_contents_t **displays)
{
    NV_ATRACE_BEGIN(__func__);
    struct nvhwc_context *ctx = (struct nvhwc_context *)dev;
    size_t ii;
    int err = 0;
    int changed, composite;
    NvBool windows_overlap = NV_FALSE;

    if (ctx->props.fixed.scan_props) {
        hwc_props_scan(&ctx->props);
        if (ctx->props.dynamic.ftrace_enable) {
            ctx->device.set = hwc_set_ftrace;
        } else {
            ctx->device.set = hwc_set;
        }
    }

    changed = update_idle_state(ctx);
    /* Cache the idle composition state since it may be overridden in
     * hwc_prepare_begin.
     */
    composite = ctx->idle.composite;

    if (ctx->reconfigure_windows) {
        changed = 1;
        ctx->reconfigure_windows = 0;
    }

    hwc_update_mirror_state(ctx, numDisplays, displays);

    for (ii = 0; ii < numDisplays; ii++) {
        if (displays[ii]) {
            struct nvhwc_display *dpy = &ctx->displays[ii];

            int ret = hwc_prepare_display(ctx, displays[ii], ii, changed);
            if (ret) err = ret;
            windows_overlap |= dpy->windows_overlap;
        }
    }

    if (windows_overlap) {
        enable_idle_detection(ctx);
        /* If composition was overridden, request a new frame now. */
        if (composite && !ctx->idle.composite) {
            IDLELOG("generating deferred frame");
            hwc_invalidate(ctx);
        }
    } else {
        disable_idle_detection(ctx);
    }

    update_panel_state(ctx, displays[HWC_DISPLAY_PRIMARY]);

#if DEBUG_MODE
    if (ctx->props.dynamic.dump_config) {
        char temp[2000];
        hwc_dump(dev, temp, NV_ARRAY_SIZE(temp));
        ALOGD("%s\n", temp);
    }
#endif

    NV_ATRACE_END();
    return err;
}

static int reverse_transform(int transform)
{
    if (transform & HWC_TRANSFORM_ROT_90) {
        return transform ^ HWC_TRANSFORM_ROT_180;
    } else {
        return transform & HWC_TRANSFORM_ROT_180;
    }
}

static void hwc_scale_window(struct nvhwc_display *dpy, struct nvfb_window *o,
                             struct nvhwc_window_mapping *map, int transform,
                             NvBool clipDst, hwc_rect_t *src, hwc_rect_t *dst)
{
    float dw;
    float dh;
    hwc_rect_t clip;

    if (clipDst) {
        o->dst.left = NV_MAX(0, dst->left);
        o->dst.top = NV_MAX(0, dst->top);
        o->dst.right = NV_MIN(dpy->device_clip.right, dst->right);
        o->dst.bottom = NV_MIN(dpy->device_clip.bottom, dst->bottom);
    } else {
        o->dst.left = dst->left;
        o->dst.top = dst->top;
        o->dst.right = dst->right;
        o->dst.bottom = dst->bottom;
    }

    /* clip source */
    if (transform & HWC_TRANSFORM_ROT_90) {
        dw = (float)r_width(src)  / (float)NV_MAX(1, r_height(dst));
        dh = (float)r_height(src) / (float)NV_MAX(1, r_width(dst));
    } else {
        dw = (float)r_width(src)  / (float)NV_MAX(1, r_width(dst));
        dh = (float)r_height(src) / (float)NV_MAX(1, r_height(dst));
    }

    clip.left   = o->dst.left   - dst->left;
    clip.top    = o->dst.top    - dst->top;
    clip.right  = o->dst.right  - dst->right;
    clip.bottom = o->dst.bottom - dst->bottom;

    hwc_transform_rect(reverse_transform(transform), &clip, &clip, 0, 0);

    o->src.left   = src->left   + 0.5f + dw * clip.left;
    o->src.right  = src->right  + 0.5f + dw * clip.right;

    /* Guard against zero-sized source */
    if (o->src.left == o->src.right) {
        if (o->src.right < src->right) {
            o->src.right++;
        } else {
            NV_ASSERT(o->src.left > src->left);
            o->src.left--;
        }
    }

    o->src.top    = src->top    + 0.5f + dh * clip.top;
    o->src.bottom = src->bottom + 0.5f + dh * clip.bottom;

    /* Guard against zero-sized source */
    if (o->src.top == o->src.bottom) {
        if (o->src.bottom < src->bottom) {
            o->src.bottom++;
        } else {
            NV_ASSERT(o->src.top > src->top);
            o->src.top--;
        }
    }
}

static inline int subtract_transform(int t0, int t1)
{
    int t = t0 ^ t1;
    if (t1 & HWC_TRANSFORM_ROT_90) {
        NV_ASSERT(t0 & HWC_TRANSFORM_ROT_90);
        if (t == HAL_TRANSFORM_FLIP_H) {
            return HAL_TRANSFORM_FLIP_V;
        } else if (t == HAL_TRANSFORM_FLIP_V) {
            return HAL_TRANSFORM_FLIP_H;
        }
    }
    return t;
}

static void window_attrs(struct nvhwc_display *dpy,
                         struct nvfb_window *win,
                         struct nvhwc_window_mapping *map,
                         struct nvhwc_prepare_layer *ll,
                         NvBool clipDst)
{
    win->blend = ll->blending;
    win->surf_index = map->scratch ? 0 : ll->surf_index;
    win->transform = ll->transform;
    win->planeAlpha = ll->layer->planeAlpha;

    if (map->scratch && map->scratch->transform) {
        win->transform = subtract_transform(ll->transform,
                                            map->scratch->transform);
        ll->transform ^= map->scratch->transform;
    }

    hwc_scale_window(dpy, win, map, ll->transform, clipDst, &ll->src, &ll->dst);
}

static unsigned need_fb_convert(struct nvhwc_display *dpy,
                                struct nvhwc_prepare_state *prepare,
                                struct nvhwc_prepare_layer *ll,
                                int window)
{
    unsigned win_cap = dpy->caps.window_cap[window].cap;
    unsigned req = 0;
    float scale_w;
    float scale_h;
    int layer_needs_scale;

    layer_needs_scale = hwc_get_scale(ll->transform, &ll->src,
                                      &ll->dst, &scale_w, &scale_h);

    if (layer_needs_scale) {
        req |= NVFB_WINDOW_CAP_SCALE;
    }
    if (ll->transform & (HWC_TRANSFORM_FLIP_H | HWC_TRANSFORM_FLIP_V)) {
        req |= NVFB_WINDOW_CAP_FLIPHV;
    }
    /* NVFB_WINDOW_CAP_SWAPXY_TILED is a common denominator for windows
     * witch support rotation at all
     */
    if (ll->transform & HWC_TRANSFORM_ROT_90) {
        req |= NVFB_WINDOW_CAP_SWAPXY_TILED;
    }

    if (!dpy->composite.scratch) {
        /* HWC_FRAMEBUFFER_TARGET uses default layout */
        if (NvRmSurfaceGetDefaultLayout() != NvRmSurfaceLayout_Pitch) {
            req |= NVFB_WINDOW_CAP_TILED;
        }
    }

    return ((win_cap ^ req) & req);
}

static void conf_prepare(struct nvhwc_context *ctx,
                         struct nvhwc_display *dpy,
                         struct nvhwc_prepare_state *prepare)
{
    size_t ii;

    dpy->conf.protect = prepare->protect;
    dpy->conf.hotplug = dpy->hotplug.cached;

    for (ii = 0; ii < dpy->caps.num_windows; ii++) {
        struct nvfb_window *win = &dpy->conf.overlay[ii];
        struct nvhwc_window_mapping *map = &dpy->map[ii];
        struct nvhwc_prepare_map *layer_map = &prepare->layer_map[ii];

        win->window_index = dpy->caps.window_cap[layer_map->window].idx;

        if ((int)ii == dpy->fb_index) {
            struct nvhwc_prepare_layer *ll = &prepare->fb.layer;
            unsigned unsatisfied_req;

            NV_ASSERT(ll->index != -1);

            // TODO!
            // Remanent of the old composite code, it had two functions for
            // preparing the compositor, maybe this can be moved inside
            // hwc_composite_prepare
            hwc_composite_prepare2(ctx, dpy, prepare);

            unsatisfied_req = need_fb_convert(dpy, prepare, ll, layer_map->window);

            if (unsatisfied_req) {
                int sw, dw;
                int sh, dh;
                NvRect crop;
                NvRmSurfaceLayout layout = (unsatisfied_req & NVFB_WINDOW_CAP_TILED) ?
                    NvRmSurfaceLayout_Pitch : NvRmSurfaceGetDefaultLayout();

                /* Compositors other then SurfaceFlinger are expected to handle
                 * transforms, scale, and layout conversion.
                 */
                NV_ASSERT(dpy->composite.id == HWC_Compositor_SurfaceFlinger);

                /* dst has already been scaled and rotated (in composite_prepare_layer) */
                sw = dw = r_width(&ll->dst);
                sh = dh = r_height(&ll->dst);
                if (ll->transform & HWC_TRANSFORM_ROT_90) {
                    NVGR_SWAP(sw, sh);
                }

                r_copy(&crop, &ll->src);
                map->scratch = scratch_assign(dpy,
                        ll->transform,
                        sw, sh,
                        NvColorFormat_A8B8G8R8,
                        layout,
                        &crop,
                        hwc_layer_is_protected(ll->layer));

                if (map->scratch) {
                    ll->src.left = 0;
                    ll->src.top = 0;
                    ll->src.right = dw;
                    ll->src.bottom = dh;
                } else {
                    NV_ASSERT(!"Failed to assign scratch buffer for a framebuffer");
                }
            }

            prepare->fb.layer.blending = prepare->fb.blending;
            window_attrs(dpy, win, map, ll, NV_TRUE);
        } else if (prepare->layer_map[ii].layer_index != -1) {
            int li = prepare->layer_map[ii].layer_index;
            struct nvhwc_prepare_layer *ll = &prepare->layers[li];

            window_attrs(dpy, win, map, ll, NV_TRUE);
        }
    }
}

static inline void hwc_set_buffer(NvGrModule *m, struct nvhwc_display *dpy,
                                  struct nvhwc_window_mapping *map,
                                  struct nvfb_window *win,
                                  hwc_layer_t *layer,
                                  struct nvfb_buffer *buf)
{
    NvNativeHandle *buffer = (NvNativeHandle *) layer->handle;
    int acquireFenceFd = layer->acquireFenceFd;
    int isDummyFbTarget;

    layer->acquireFenceFd = -1;

    /* If we composite internally, we don't want to decompress the incoming
     * fb target layer, since it won't be used (composition scratch fb will
     * be used instead). */
    isDummyFbTarget = (map == &dpy->map[dpy->fb_index] && dpy->composite.scratch);

    if (buffer && !isDummyFbTarget) {
        int status = m->decompress_buffer(m, buffer, acquireFenceFd,
                                          &acquireFenceFd);
        if (status) {
            ALOGW("decompression failed for layer %d", map->index);
        }
    }

    if (map->scratch) {
        dpy->scratch->blit(dpy->scratch, buffer, map->surf_index,
                           acquireFenceFd, map->scratch,
                           &win->offset, &buf->preFenceFd);
        buf->buffer = dpy->scratch->get_buffer(dpy->scratch, map->scratch);
        layer->releaseFenceFd = nvsync_dup(__func__, buf->preFenceFd);
        NVSYNC_LOG("setting scratch release fence %d (%d)",
                   layer->releaseFenceFd, buf->preFenceFd);
    } else {
        buf->buffer = buffer;
        buf->preFenceFd = acquireFenceFd;
        win->offset.x = win->offset.y = 0;
    }
}

static void hwc_set_release_fence(struct nvhwc_display *dpy,
                                  hwc_display_contents_t* display,
                                  struct nvhwc_window_mapping *map)
{
    int releaseFenceFd = nvsync_dup("releaseFence", display->retireFenceFd);

    if (map->scratch) {
        dpy->scratch->set_release_fence(dpy->scratch, map->scratch,
                                        releaseFenceFd);
    } else {
        hwc_layer_t *layer = &display->hwLayers[map->index];
        layer->releaseFenceFd = releaseFenceFd;
    }
    if (releaseFenceFd >= 0) {
        NVSYNC_LOG("setting display release fence %d (%d)",
                   releaseFenceFd, display->retireFenceFd);
    }
}

static void hwc_update_fences(struct nvhwc_display *dpy, int retireFenceFd)
{
    /* Save previous fence */
    if (dpy->prevFenceFd >= 0) {
        nvsync_close(dpy->prevFenceFd);
    }
    dpy->prevFenceFd = dpy->currFenceFd;

    /* Save current fence */
    dpy->currFenceFd = nvsync_dup(__func__, retireFenceFd);
}

/* Acquire buffer refs for the current frame */
static void hwc_save_new_buffers(NvGrModule *m, struct nvhwc_display *dpy,
                                 struct nvfb_buffer *bufs)
{
    size_t ii, num_buffers = dpy->caps.num_windows+1;

    for (ii = 0; ii < num_buffers; ii++) {
        dpy->buffers[ii] = bufs[ii].buffer;

        /* Ref the handle since we're caching the pointer */
        if (dpy->buffers[ii]) {
            m->Base.registerBuffer(&m->Base,
                                   (buffer_handle_t) dpy->buffers[ii]);
        }
    }
}

/* Release the buffer refs for the last frame */
static void hwc_release_old_buffers(NvGrModule *m, struct nvhwc_display *dpy)
{
    size_t ii, num_buffers = dpy->caps.num_windows+1;

    for (ii = 0; ii < num_buffers; ii++) {
        if (dpy->buffers[ii]) {
            m->Base.unregisterBuffer(&m->Base,
                                     (buffer_handle_t) dpy->buffers[ii]);
            dpy->buffers[ii] = NULL;
        }
    }
}

static int hwc_set_display(struct nvhwc_context *ctx, int disp,
                           hwc_display_contents_t* display)
{
    NV_ATRACE_BEGIN(__func__);
    struct nvhwc_display *dpy = &ctx->displays[disp];
    struct nvfb_buffer bufs[NVFB_MAX_WINDOWS];
    size_t ii, num_buffers = dpy->caps.num_windows+1;

    NV_ASSERT(display);

    if (display->outbufAcquireFenceFd >= 0) {
        /* Outbuf is only for HWC_DEVICE_VERSION_1_2 which we don't
         * yet support.
         */
        nvsync_close(display->outbufAcquireFenceFd);
        display->outbufAcquireFenceFd = -1;
    }

    if (dpy->blank) {
        /* Close all acquire fences */
        for (ii = 0; ii < display->numHwLayers; ii++) {
            hwc_layer_t *layer = &display->hwLayers[ii];
            if (layer->acquireFenceFd >= 0) {
                nvsync_close(layer->acquireFenceFd);
                layer->acquireFenceFd = -1;
            }
        }
        NV_ATRACE_END();
        return 0;
    }

    for (ii = 0; ii < dpy->caps.num_windows; ii++) {
        if (dpy->map[ii].index != -1) {
            hwc_layer_t *layer = &display->hwLayers[dpy->map[ii].index];

            hwc_set_buffer(ctx->gralloc, dpy, &dpy->map[ii],
                           &dpy->conf.overlay[ii], layer, &bufs[ii]);
        } else {
            bufs[ii].buffer = NULL;
            bufs[ii].preFenceFd = -1;
        }
    }

    /* Cursor layer is always the last buffer */
    NV_ASSERT(ii == dpy->caps.num_windows);
    if (dpy->cursor.map.index != -1) {
        hwc_layer_t *layer = &display->hwLayers[dpy->cursor.map.index];

        hwc_set_buffer(ctx->gralloc, dpy, &dpy->cursor.map, &dpy->conf.cursor,
                       layer, &bufs[ii]);
    } else {
        bufs[ii].buffer = NULL;
        bufs[ii].preFenceFd = -1;
    }

    if (dpy->composite.scratch) {
        int fenceFd = -1;

        NV_ASSERT(dpy->composite.contents.numLayers);
        NV_ASSERT(dpy->fb_index >= 0);

        if (!dpy->fb_recycle) {
            hwc_composite_set(ctx, dpy, display, &fenceFd);
            NVSYNC_LOG("composite generated fence %d", fenceFd);
        }

        bufs[dpy->fb_index].buffer =
            dpy->scratch->get_buffer(dpy->scratch, dpy->composite.scratch);
        NV_ASSERT(bufs[dpy->fb_index].preFenceFd < 0);
        bufs[dpy->fb_index].preFenceFd = fenceFd;
    } else if (dpy->fb_recycle) {
        struct nvhwc_fb_cache *cache = &dpy->fb_cache[OLD_CACHE_INDEX(dpy)];

        for (ii = 0; ii < (size_t) cache->numLayers; ii++) {
            int index = cache->layers[ii].index;
            hwc_layer_t *layer = &display->hwLayers[index];

            if (layer->acquireFenceFd >= 0) {
                nvsync_close(layer->acquireFenceFd);
                layer->acquireFenceFd = -1;
            }
        }
    }

    /* All acquireFenceFd's should now be consumed */
    for (ii = 0; ii < display->numHwLayers; ii++) {
        hwc_layer_t *layer = &display->hwLayers[ii];
        NV_ASSERT(layer->acquireFenceFd < 0);
    }

    if (!dpy->ignore_initial_blank) {
        nvfb_post(ctx->fb, disp, &dpy->conf, bufs, &display->retireFenceFd);
    } else {
        if (display->numHwLayers > 1) {
            dpy->ignore_initial_blank = NV_FALSE;
            nvfb_post(ctx->fb, disp, &dpy->conf, bufs, &display->retireFenceFd);
        } else {
            hwc_layer_t *layer = &display->hwLayers[0];
            NV_ASSERT(display->numHwLayers == 1 &&
                      layer->compositionType == HWC_FRAMEBUFFER_TARGET);

            display->retireFenceFd = -1;
        }
    }

    NVSYNC_LOG("setting retire fence %d", display->retireFenceFd);

    hwc_update_fences(dpy, display->retireFenceFd);

    /* Save a release fence for the scratch composite buffer */
    if (dpy->composite.scratch) {
        int releaseFenceFd = nvsync_dup("releaseFence", display->retireFenceFd);
        dpy->scratch->set_release_fence(dpy->scratch, dpy->composite.scratch,
                                        releaseFenceFd);
    }

    /* Release old buffer set */
    hwc_release_old_buffers(ctx->gralloc, dpy);

    /* Save new buffer set */
    hwc_save_new_buffers(ctx->gralloc, dpy, bufs);

    /* Set release fences */
    for (ii = 0; ii < dpy->caps.num_windows; ii++) {
        if (dpy->map[ii].index != -1) {
            hwc_set_release_fence(dpy, display, &dpy->map[ii]);
        }
    }
    if (dpy->cursor.map.index != -1) {
        hwc_set_release_fence(dpy, display, &dpy->cursor.map);
    }

    NV_ATRACE_END();
    return 0;
}

static int hwc_set(struct hwc_composer_device_1 *dev,
                   size_t numDisplays, hwc_display_contents_t** displays)
{
    NV_ATRACE_BEGIN(__func__);
    struct nvhwc_context *ctx = (struct nvhwc_context *)dev;
    size_t ii;
    int err = 0;

    struct {
        EGLDisplay dpy;
        EGLContext ctx;
        EGLSurface draw;
        EGLSurface read;
    } sf = {
        .dpy = EGL_NO_DISPLAY,
        .ctx = EGL_NO_CONTEXT,
        .draw = EGL_NO_SURFACE,
        .read = EGL_NO_SURFACE,
    };

    if (!ctx->props.fixed.no_egl) {
        sf.dpy = eglGetCurrentDisplay();
        sf.ctx = eglGetCurrentContext();
        sf.draw = eglGetCurrentSurface(EGL_DRAW);
        sf.read = eglGetCurrentSurface(EGL_READ);
    }

    pthread_mutex_lock(&ctx->hotplug.mutex);

    for (ii = 0; ii < numDisplays; ii++) {
        if (displays[ii]) {
            int ret = hwc_set_display(ctx, ii, displays[ii]);
            if (ret) err = ret;
        }
    }

    pthread_mutex_unlock(&ctx->hotplug.mutex);

    if (!ctx->props.fixed.no_egl && sf.ctx != eglGetCurrentContext()) {
        EGLBoolean result = eglMakeCurrent(sf.dpy, sf.draw, sf.read, sf.ctx);
        if (!result) {
            ALOGE("Failed to restore surfaceflinger context");
        }
    }

    NV_ATRACE_END();
    return err;
}

static int hwc_set_ftrace(hwc_composer_device_t *dev,
                          size_t numDisplays,
                          hwc_display_contents_t** displays)
{
    int err;

    sysfs_write_string(PATH_FTRACE_MARKER_LOG, "start hwc_set frame\n");
    err = hwc_set(dev, numDisplays, displays);
    sysfs_write_string(PATH_FTRACE_MARKER_LOG, "end hwc_set frame\n");

    return err;
}

static int hwc_query(hwc_composer_device_t *dev, int what, int* value)
{
    struct nvhwc_context *ctx = (struct nvhwc_context *)dev;

    switch (what) {
    case HWC_BACKGROUND_LAYER_SUPPORTED:
        // XXX - Implement me eventually, Google not using it yet
        *value = 0;
        return 0;
    case HWC_DISPLAY_TYPES_SUPPORTED:
        /* Assert is a reminder in case more types are added later. */
        NV_ASSERT(HWC_NUM_DISPLAY_TYPES == 2);
        return HWC_DISPLAY_PRIMARY_BIT | HWC_DISPLAY_EXTERNAL_BIT;
    default:
        return -EINVAL;
    }
}

static int hwc_eventControl(hwc_composer_device_t *dev, int dpy,
                            int event, int enabled)
{
    struct nvhwc_context *ctx = (struct nvhwc_context *)dev;
    int status = -EINVAL;

    /* Only legal values for "enabled" are 0 and 1 */
    if (!(enabled & ~1)) {
        switch (event) {
        case HWC_EVENT_VSYNC:
            pthread_mutex_lock(&ctx->event.mutex);
            SYNCLOG("%s eventSync", enabled ? "Enabling" : "Disabling");
            if (enabled && !ctx->event.sync) {
                pthread_cond_signal(&ctx->event.condition);
            }
            ctx->event.sync = enabled;
            pthread_mutex_unlock(&ctx->event.mutex);
            status = 0;
            break;
        default:
            break;
        }
    }

    return status;
}

static void hwc_set_device_clip(struct nvhwc_display *dpy, int xres, int yres)
{
    int fb_xres, fb_yres;

    dpy->device_clip.left = 0;
    dpy->device_clip.top = 0;
    dpy->device_clip.right = xres;
    dpy->device_clip.bottom = yres;

    ALOGD("Display %d device clip is %d x %d", dpy->type, xres, yres);

    dpy->modification.type = HWC_Modification_None;

    if (dpy->type == HWC_DISPLAY_PRIMARY) {
        fb_xres = dpy->fb_size.xres;
        fb_yres = dpy->fb_size.yres;
    } else {
        fb_xres = dpy->default_mode->xres;
        fb_yres = dpy->default_mode->yres;
    }

    /* If there is a display rotation, apply to the config */
    if (dpy->transform & HWC_TRANSFORM_ROT_90) {
        NVGR_SWAP(xres, yres);
    }

    if (xres !=  fb_xres || yres != dpy->fb_size.yres) {
        dpy->modification.type = HWC_Modification_Xform;
        xform_get_center_scale(&dpy->modification.xf, fb_xres, fb_yres,
                               xres, yres, 1.0);
        if (dpy->transform & HWC_TRANSFORM_ROT_90) {
            NVGR_SWAP(dpy->modification.xf.x_off, dpy->modification.xf.y_off);
        }
        dpy->device_clip.left += dpy->modification.xf.x_off;
        dpy->device_clip.right += dpy->modification.xf.x_off;
        dpy->device_clip.top += dpy->modification.xf.y_off;
        dpy->device_clip.bottom += dpy->modification.xf.y_off;
    }
}

static void hwc_set_layer_clip(struct nvhwc_display *dpy, int xres, int yres)
{
    /* Override primary display resolution */
    if (dpy->type == HWC_DISPLAY_PRIMARY) {
        xres = dpy->fb_size.xres;
        yres = dpy->fb_size.yres;
    }

    dpy->config.res_x = xres;
    dpy->config.res_y = yres;

    dpy->layer_clip.left = 0;
    dpy->layer_clip.top = 0;
    dpy->layer_clip.right = xres;
    dpy->layer_clip.bottom = yres;

    ALOGD("Display %d layer clip is %d x %d", dpy->type, xres, yres);
}

static void hotplug_display(struct nvhwc_context *ctx, int disp)
{
    struct nvhwc_display *dpy = &ctx->displays[disp];
    struct nvfb_config config;

    if (nvfb_config_get_current(ctx->fb, disp, &config)) {
        return;
    }

    nvfb_get_display_caps(ctx->fb, disp, &dpy->caps);
    nvfb_get_hotplug_status(ctx->fb, disp, &dpy->hotplug.latest);
    dpy->transform = dpy->hotplug.latest.transform;

    /* layer_clip is the virtual clip applied to layer coords */
    hwc_set_layer_clip(dpy, config.res_x, config.res_y);

    /* device_clip is the real clip applied to window coords */
    hwc_set_device_clip(dpy, config.res_x, config.res_y);
}

static int set_external_display_mode(struct nvhwc_context *ctx)
{
    struct nvhwc_display *primary = &ctx->displays[HWC_DISPLAY_PRIMARY];
    struct fb_var_screeninfo *mode;
    int xres, yres;

    if (ctx->panel_transform & HWC_TRANSFORM_ROT_90) {
        xres = primary->config.res_y;
        yres = primary->config.res_x;
    } else {
        xres = primary->config.res_x;
        yres = primary->config.res_y;
    }

    mode = nvfb_choose_display_mode(ctx->fb, HWC_DISPLAY_EXTERNAL,
                                    xres, yres, NvFbVideoPolicy_Closest);

    if (mode) {
        ctx->displays[HWC_DISPLAY_EXTERNAL].default_mode = mode;
        ctx->displays[HWC_DISPLAY_EXTERNAL].current_mode = mode;
        return nvfb_set_display_mode(ctx->fb, HWC_DISPLAY_EXTERNAL, mode);
    }

    return -1;
}

static void hwc_blank_display(struct nvhwc_context *ctx, int disp, int blank)
{
    struct nvhwc_display *dpy = &ctx->displays[disp];

    pthread_mutex_lock(&ctx->hotplug.mutex);

    ALOGD("%s: display %d: [%d -> %d]", __func__, disp, dpy->blank, blank);
    dpy->blank = blank;

    if (blank) {
        size_t ii;
        int flipFenceFd = -1;

        /* Disable DIDIM to avoid artifacts on unblank */
        if (disp == HWC_DISPLAY_PRIMARY) {
            nvfb_didim_window(ctx->fb, disp, NULL);
        }

        dpy->fb_recycle = 0;
        nvfb_post(ctx->fb, disp, NULL, NULL, &flipFenceFd);
        hwc_update_fences(dpy, flipFenceFd);
        nvsync_close(flipFenceFd);

        /* release refs to previous frame's buffers */
        hwc_release_old_buffers(ctx->gralloc, dpy);
    } else {
        if (disp == HWC_DISPLAY_EXTERNAL) {
            set_external_display_mode(ctx);
        }
        hotplug_display(ctx, disp);
    }

    pthread_mutex_unlock(&ctx->hotplug.mutex);
}

static int hwc_blank(struct hwc_composer_device_1* dev, int disp, int blank)
{
    struct nvhwc_context *ctx = (struct nvhwc_context *)dev;

    ALOGD("%s: display %d: %sblank", __func__, disp, blank ? "" : "un");

    hwc_blank_display(ctx, disp, blank);

    return nvfb_blank(ctx->fb, disp, blank);
}

/*****************************************************************************/

static int hwc_invalidate(void *data)
{
    struct nvhwc_context *ctx = (struct nvhwc_context *) data;

    IMPLOG("hwc_invalidate");

    if (ctx->procs && ctx->procs->invalidate) {
        ctx->procs->invalidate(ctx->procs);
        return 0;
    } else {
        /* fail if the hwc invalidate callback is not registered */
        ALOGE("invalidate callback not registered");
        return -1;
    }
}

static int hwc_hotplug(void *data, int disp, struct nvfb_hotplug_status hotplug)
{
    struct nvhwc_context *ctx = (struct nvhwc_context *) data;
    struct nvhwc_display *dpy = &ctx->displays[disp];
    int was_connected = dpy->hotplug.latest.connected;
    int res_x = dpy->config.res_x;
    int res_y = dpy->config.res_y;

    dpy->hotplug.latest = hotplug;

    ALOGD("%s: display %d %sconnected", __func__, disp,
          dpy->hotplug.latest.connected ? "" : "dis");
    hwc_blank_display(ctx, disp, !dpy->hotplug.latest.connected);

    if (disp == HWC_DISPLAY_PRIMARY) {
        if (hotplug.connected) {
            hwc_invalidate(ctx);
        }
        return 0;
    }

    if (was_connected && dpy->hotplug.latest.connected &&
        res_x == dpy->config.res_x && res_y == dpy->config.res_y) {
        /* If the mode has not changed, request a new frame but do not
         * notify surfaceflinger.
         */
        hwc_invalidate(ctx);
        ALOGD("%s: display %d mode unchanged, ignoring hotplug event",
              __func__, disp);
        return 0;
    } else if (ctx->procs && ctx->procs->hotplug) {
        ctx->procs->hotplug(ctx->procs, disp, dpy->hotplug.latest.connected);
        return 0;
    } else {
        /* fail if the hwc hotplug callback is not registered */
        ALOGE("hotplug callback not registered");
        return -1;
    }
}

static int hwc_acquire(void *data, int disp)
{
    struct nvhwc_context *ctx = (struct nvhwc_context *) data;
    struct nvhwc_display *dpy = &ctx->displays[disp];

    dpy->hotplug.latest.connected = 1;
    dpy->hotplug.cached.value = -1;
    if (!dpy->blank) {
        hwc_blank_display(ctx, disp, 0);
        nvfb_blank(ctx->fb, disp, 0);
    }

    /* Force a frame for the new display */
    hwc_invalidate(ctx);

    return 0;
}

static int hwc_release(void *data, int disp)
{
    struct nvhwc_context *ctx = (struct nvhwc_context *) data;
    struct nvhwc_display *dpy = &ctx->displays[disp];

    dpy->hotplug.latest.connected = 0;
    dpy->hotplug.cached.value = -1;
    if (!dpy->blank) {
        hwc_blank_display(ctx, disp, 1);
        nvfb_blank(ctx->fb, disp, 1);
        /* Restore the original unblanked state so a subsequent
         * acquire knows to unblank.
         */
        dpy->blank = 0;
    }

    return 0;
}

static int hwc_bandwidth_change(void *data)
{
    struct nvhwc_context *ctx = (struct nvhwc_context *) data;

    if (!ctx->reconfigure_windows && !ctx->idle.composite) {
        ctx->reconfigure_windows = 1;
        hwc_invalidate(ctx);
    }

    return 0;
}

static void hwc_registerProcs(hwc_composer_device_t *dev, hwc_procs_t const* p)
{
    struct nvhwc_context* ctx = (struct nvhwc_context *)dev;
    ctx->procs = (hwc_procs_t *)p;
}

static void idle_handler(union sigval si)
{
    struct nvhwc_context *ctx = (struct nvhwc_context *) si.sival_ptr;

    if (android_atomic_acquire_load(&ctx->idle.enabled)) {
        IDLELOG("timeout reached, generating idle frame");
        hwc_invalidate(ctx);
    } else {
        IDLELOG("disabled, ignoring timeout");
    }
}

static void create_idle_timer(struct nvhwc_context *ctx)
{
    struct sigevent ev;

    ev.sigev_notify = SIGEV_THREAD;
    ev.sigev_value.sival_ptr = ctx;
    ev.sigev_notify_function = idle_handler;
    ev.sigev_notify_attributes = NULL;
    ev.sigev_signo = 0;
    if (timer_create(CLOCK_MONOTONIC, &ev, &ctx->idle.timer) < 0) {
        ctx->idle.timer = 0;
        ALOGE("Can't create idle timer: %s", strerror(errno));
    }
}

static void init_idle_machine(struct nvhwc_context *ctx)
{
    create_idle_timer(ctx);

    android_atomic_release_store(0, &ctx->idle.enabled);
}

static void *event_monitor(void *arg)
{
    const char *thread_name = "hwc_eventmon";
    struct nvhwc_context *ctx = (struct nvhwc_context *) arg;

    setpriority(PRIO_PROCESS, 0, -20);

    pthread_setname_np(ctx->event.thread, thread_name);

    while (1) {
        /* Sleep until needed */
        pthread_mutex_lock(&ctx->event.mutex);
        while (!ctx->event.sync && !ctx->event.shutdown) {
            pthread_cond_wait(&ctx->event.condition, &ctx->event.mutex);
        }
        pthread_mutex_unlock(&ctx->event.mutex);

        if (ctx->event.shutdown) {
            break;
        }

        /* Notify SurfaceFlinger */
        if (ctx->procs && ctx->procs->vsync) {
            int64_t timestamp_ns;

            nvfb_vblank_wait(ctx->fb, &timestamp_ns);

            /* Cannot hold a mutex during the callback, else a
             * deadlock will occur in SurfaceFlinger.  Instead use an
             * atomic load, which means there is a very small chance
             * SurfaceFlinger may disable the callback before it is
             * received.  Fortunately it can handle this.
             */
            if (android_atomic_acquire_load(&ctx->event.sync)) {
                ctx->procs->vsync(ctx->procs, 0, timestamp_ns);
                SYNCLOG("Signaled, timestamp is %lld", timestamp_ns);
            } else {
                SYNCLOG("eventSync disabled, not signaling");
            }
        }
    }

    return NULL;
}

static int open_event_monitor(struct nvhwc_context *ctx)
{
    int status;
    pthread_attr_t attr;
    struct sched_param param;

    /* Create event mutex and condition */
    pthread_mutex_init(&ctx->event.mutex, NULL);
    pthread_cond_init(&ctx->event.condition, NULL);

    /* Set thread priority */
    memset(&param, 0, sizeof(param));
    param.sched_priority = HAL_PRIORITY_URGENT_DISPLAY;
    pthread_attr_init(&attr);
    pthread_attr_setschedparam(&attr, &param);

    /* Initialize event state */
    android_atomic_release_store(0, &ctx->event.sync);

    /* Create event monitor thread */
    status = pthread_create(&ctx->event.thread, &attr, event_monitor, ctx);

    /* cleanup */
    pthread_attr_destroy(&attr);

    return -status;
}

static void close_event_monitor(struct nvhwc_context *ctx)
{
    if (ctx->event.thread) {
        /* Tell event thread to exit */
        pthread_mutex_lock(&ctx->event.mutex);
        ctx->event.sync = 0;
        ctx->event.shutdown = 1;
        pthread_cond_signal(&ctx->event.condition);
        pthread_mutex_unlock(&ctx->event.mutex);

        /* Wait for the thread to exit and cleanup */
        pthread_join(ctx->event.thread, NULL);
        pthread_mutex_destroy(&ctx->event.mutex);
        pthread_cond_destroy(&ctx->event.condition);
    }
}

static int
hwc_getDisplayConfigs(struct hwc_composer_device_1* dev, int disp,
                      uint32_t* configs, size_t* numConfigs)
{
    struct nvhwc_context *ctx = (struct nvhwc_context *) dev;
    size_t count = 0;

    if (nvfb_config_get_count(ctx->fb, disp, &count)) {
        return -1;
    }

    if (count > *numConfigs) {
        count = *numConfigs;
    }

    *numConfigs = count;

    while (count--) {
        configs[count] = count;
    }

    return 0;
}

static int
hwc_getDisplayAttributes(struct hwc_composer_device_1* dev, int disp,
                         uint32_t index, const uint32_t* attributes,
                         int32_t* values)
{
    struct nvhwc_context *ctx = (struct nvhwc_context *) dev;
    struct nvfb_config config;

    if (nvfb_config_get(ctx->fb, disp, index, &config)) {
        return -1;
    }

    /* Conditionally override primary resolution */
    if (disp == HWC_DISPLAY_PRIMARY && index == 0) {
        struct nvhwc_display *dpy = &ctx->displays[disp];
        config.res_x = dpy->fb_size.xres;
        config.res_y = dpy->fb_size.yres;
    } else if (ctx->displays[disp].transform & HWC_TRANSFORM_ROT_90) {
        NVGR_SWAP(config.res_x, config.res_y);
    }

    while (*attributes != HWC_DISPLAY_NO_ATTRIBUTE) {
        switch (*attributes) {
        case HWC_DISPLAY_VSYNC_PERIOD:
            SYNCLOG("Returning vsync period %d", config.period);
            *values = config.period;
            break;
        case HWC_DISPLAY_WIDTH:
            *values = config.res_x;
            ALOGD("config[%d].x = %d", disp, config.res_x);
            break;
        case HWC_DISPLAY_HEIGHT:
            *values = config.res_y;
            ALOGD("config[%d].y = %d", disp, config.res_y);
            break;
        case HWC_DISPLAY_DPI_X:
            *values = config.dpi_x;
            break;
        case HWC_DISPLAY_DPI_Y:
            *values = config.dpi_y;
            break;
        default:
            return -1;
        }
        attributes++;
        values++;
    }

    return 0;
}

#ifdef FRAMEWORKS_HAS_SET_CURSOR
static int hwc_setCursor(struct hwc_composer_device_1* dev, int disp,
                         hwc_layer_1_t *hwCursor)
{
    struct nvhwc_context *ctx = (struct nvhwc_context *) dev;
    struct nvhwc_display *dpy = &ctx->displays[disp];

    NV_ASSERT(ctx->cursor_mode == NvFbCursorMode_Async);

    if (dpy->caps.max_cursor == 0) {
        return EINVAL;
    }

    if (hwCursor) {
        struct nvhwc_prepare_layer layer;
        struct nvfb_window win;
        struct nvhwc_window_mapping map;
        struct nvfb_buffer buf;

        prepare_copy_layer(dpy, &layer, hwCursor, 0);
        if (dpy->modification.type == HWC_Modification_Xform) {
            xform_apply(&dpy->modification.xf, &layer.dst);
        }
        if (!can_use_cursor(ctx, dpy, &layer)) {
            return EINVAL;
        }

        memset(&win, 0, sizeof(win));
        memset(&map, 0, sizeof(map));
        map.index = -1;
        memset(&buf, 0, sizeof(buf));
        buf.preFenceFd = -1;
        window_attrs(dpy, &win, &map, &layer, NV_FALSE);

        hwc_set_buffer(ctx->gralloc, dpy, &map, &win, hwCursor, &buf);
        dpy->cursor.buffer = buf.buffer;
        nvfb_set_cursor(ctx->fb, disp, &win, &buf);
    } else if (dpy->cursor.buffer) {
        dpy->cursor.buffer = NULL;
        nvfb_set_cursor(ctx->fb, disp, NULL, NULL);
    }

    return 0;
}
#endif

#ifdef FRAMEWORK_HAS_SET_DISPLAY_TRANSFORM
static int hwc_setDisplayTransform(struct hwc_composer_device_1* dev, int disp,
                                   uint32_t transform)
{
    struct nvhwc_context *ctx = (struct nvhwc_context *) dev;
    struct nvhwc_display *dpy = &ctx->displays[disp];

    ALOGD("Setting display %d transform: %u", disp, transform);
    nvfb_set_transform(ctx->fb, disp, transform,
                       &dpy->hotplug.latest);
    hotplug_display(ctx, disp);

    return 0;
}
#endif

/*********************************************************************
 *   Test interface
 */

static void hwc_post_wait(struct nvhwc_context *ctx, int disp)
{
    struct nvhwc_display *dpy = &ctx->displays[disp];
    int64_t timestamp_ns;

    /* Wait for the test frame to start being displayed */
    if (dpy->prevFenceFd >= 0) {
        nvsync_wait(dpy->prevFenceFd, -1);
    }

    /* Wait for the test frame to be fully displayed */
    nvfb_vblank_wait(ctx->fb, &timestamp_ns);
}

static void hwc_get_buffers(struct nvhwc_context *ctx, int disp,
                            NvNativeHandle *buffers[HWC_MAX_BUFFERS])
{
    struct nvhwc_display *dpy = &ctx->displays[disp];

    memcpy(buffers, dpy->buffers, sizeof(dpy->buffers));
}

/*********************************************************************
 *   Device initialization
 */

int hwc_set_display_mode(struct nvhwc_context *ctx, int disp,
                         struct fb_var_screeninfo *mode,
                         NvPoint *image_size, NvPoint *device_size)
{
    struct nvhwc_display *dpy = &ctx->displays[disp];

    if (!mode) {
        mode = dpy->default_mode;
    }

    if (mode != dpy->current_mode) {
        int err;
        int flipFenceFd = -1;

        nvfb_post(ctx->fb, disp, NULL, NULL, &flipFenceFd);
        hwc_update_fences(dpy, flipFenceFd);
        nvsync_close(flipFenceFd);

        /* release refs to previous frame's buffers */
        hwc_release_old_buffers(ctx->gralloc, dpy);

        /* Wait that display is really blanked */
        nvsync_wait(dpy->prevFenceFd, -1);

        err = nvfb_set_display_mode(ctx->fb, HWC_DISPLAY_EXTERNAL, mode);
        if (err) {
            return err;
        }

        /* Set new layer clip */
        if (image_size) {
            hwc_set_layer_clip(dpy, image_size->x, image_size->y);
        } else {
            hwc_set_layer_clip(dpy, mode->xres, mode->yres);
        }

        /* Set new device clip */
        if (device_size) {
            hwc_set_device_clip(dpy, device_size->x, device_size->y);
        } else {
            hwc_set_device_clip(dpy, mode->xres, mode->yres);
        }

        dpy->current_mode = mode;

        /* Invalidate current fb_cache */
        dpy->fb_cache[OLD_CACHE_INDEX(dpy)].numLayers = 0;
    }

    return 0;
}

/* Query the (possibly overridden) primary display resolution.  This
 * is the advertised resolution at which Android renders all content.
 */
static void get_primary_resolution(struct nvhwc_context *ctx)
{
    struct nvhwc_display *dpy = &ctx->displays[HWC_DISPLAY_PRIMARY];

    nvfb_get_primary_resolution(ctx->fb,
                                &dpy->fb_size.xres,
                                &dpy->fb_size.yres);
}

static int hwc_framebuffer_open(struct nvhwc_context *ctx)
{
    int ii, status;
    struct nvfb_callbacks callbacks = {
        .data = (void *) ctx,
        .hotplug = hwc_hotplug,
        .acquire = hwc_acquire,
        .release = hwc_release,
        .bandwidth_change = hwc_bandwidth_change,
    };
    struct nvhwc_display *fb = &ctx->displays[HWC_DISPLAY_PRIMARY];
    const struct nvfb_config *fbconfig = &fb->config;

    status = nvfb_open(&ctx->fb, &callbacks, &ctx->props);
    if (status < 0) {
        return status;
    }

    for (ii = 0; ii < HWC_NUM_DISPLAY_TYPES; ii++) {
        struct nvhwc_display *dpy = &ctx->displays[ii];

        dpy->type = ii;
        dpy->currFenceFd = -1;
        dpy->prevFenceFd = -1;
        dpy->ignore_initial_blank = NV_FALSE;

        nvfb_get_hotplug_status(ctx->fb, ii, &dpy->hotplug.latest);

        if (NvRmSurfaceGetDefaultLayout() != NvRmSurfaceLayout_Pitch)
            dpy->fb_req |= NVFB_WINDOW_CAP_TILED;

        status = ctx->gralloc->scratch_open(
            ctx->gralloc, NVFB_MAX_WINDOWS, &dpy->scratch);
        if (status < 0) {
            return status;
        }

        hwc_composite_open(ctx, dpy);
    }

    ctx->displays[HWC_DISPLAY_PRIMARY].ignore_initial_blank = NV_TRUE;

    /* Conditionally override primary resolution */
    get_primary_resolution(ctx);

    /* Set initial mode and blank state.  SurfaceFlinger currently
     * assumes external connected displays are unblanked, whereas the
     * main display starts blanked.
     */
    hotplug_display(ctx, HWC_DISPLAY_PRIMARY);
    if (fbconfig->res_y > fbconfig->res_x) {
        ctx->panel_transform = HWC_TRANSFORM_ROT_270;
    }
    fb->blank = 1;

    if (ctx->displays[HWC_DISPLAY_EXTERNAL].hotplug.latest.connected) {
        set_external_display_mode(ctx);
        hotplug_display(ctx, HWC_DISPLAY_EXTERNAL);
    }

    /* A gentle reminder to hotplug all displays, in case more display
     * types are added later.
     */
    NV_ASSERT(ctx->device.common.version == HWC_DEVICE_API_VERSION_1_1 ||
              2 == HWC_NUM_DISPLAY_TYPES);

    return 0;
}

static void hwc_framebuffer_close(struct nvhwc_context *ctx)
{
    size_t ii;

    for (ii = 0; ii < HWC_NUM_DISPLAY_TYPES; ii++) {
        struct nvhwc_display *dpy = &ctx->displays[ii];

        if (dpy->scratch) {
            ctx->gralloc->scratch_close(ctx->gralloc, dpy->scratch);
        }

        hwc_composite_close(ctx, dpy);
    }

    nvfb_close(ctx->fb);
}

static int hwc_device_close(struct hw_device_t *dev)
{
    struct nvhwc_context* ctx = (struct nvhwc_context*)dev;

    if (ctx) {
        nvfl_close();

        close_event_monitor(ctx);

        if (ctx->idle.timer) {
            timer_delete(ctx->idle.timer);
        }

#if TOUCH_SLOWSCAN_ENABLE
        if (ctx->fd_slowscan >= 0) {
            close(ctx->fd_slowscan);
            ctx->fd_slowscan = -1;
        }
#endif

        hwc_framebuffer_close(ctx);

        pthread_mutex_destroy(&ctx->hotplug.mutex);

        free(ctx);
    }

    return 0;
}

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    struct nvhwc_context *dev = NULL;
    int status = -EINVAL;

    if (!strcmp(name, HWC_HARDWARE_COMPOSER)) {
        const struct hw_module_t *gralloc;

        status = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &gralloc);
        if (status < 0)
            return status;
        if (strcmp(gralloc->author, "NVIDIA") != 0)
            return -ENOTSUP;

        dev = (struct nvhwc_context*)malloc(sizeof(*dev));
        if (!dev)
            return -ENOMEM;

        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = HWC_DEVICE_API_VERSION_1_1;
        dev->device.common.module = (struct hw_module_t*)module;
        dev->device.common.close = hwc_device_close;
        dev->device.prepare = hwc_prepare;
        dev->device.set = hwc_set;
        dev->device.eventControl = hwc_eventControl;
        dev->device.blank = hwc_blank;
        dev->device.dump = hwc_dump;
        dev->device.registerProcs = hwc_registerProcs;
        dev->device.query = hwc_query;
        dev->device.getDisplayConfigs = hwc_getDisplayConfigs;
        dev->device.getDisplayAttributes = hwc_getDisplayAttributes;
#ifdef FRAMEWORK_HAS_SET_DISPLAY_TRANSFORM
        dev->device.setDisplayTransform = hwc_setDisplayTransform;
#endif
        dev->gralloc = (NvGrModule*)gralloc;
        dev->bStereoGLAppRunning = -1;

#if TOUCH_SLOWSCAN_ENABLE
        dev->fd_slowscan = open(TOUCH_SLOWSCAN_PATH, O_WRONLY);
        dev->slowscan_shadowstate = TOUCH_SLOWSCAN_INVALID_STATE;
        if (dev->fd_slowscan < 0 && errno != ENOENT) {
            ALOGE("Can't open %s: %s", TOUCH_SLOWSCAN_PATH, strerror(errno));
        }
#endif

        hwc_props_init(&dev->props);

        /* Open framebuffer device */
        status = hwc_framebuffer_open(dev);
        if (status < 0) {
            goto fail;
        }

        /* Determine cursor behavior */
        dev->cursor_mode = nvfb_get_cursor_mode(dev->fb);
#ifdef FRAMEWORKS_HAS_SET_CURSOR
        if (dev->cursor_mode == NvFbCursorMode_Async) {
            dev->device.setCursor = hwc_setCursor;
        } else {
            dev->device.setCursor = NULL;
        }
#endif

        /* Mutex to protect display state during hotplug events */
        pthread_mutex_init(&dev->hotplug.mutex, NULL);

        init_idle_machine(dev);

        status = open_event_monitor(dev);
        if (status < 0) {
            goto fail;
        }

        /* Init framelist posting. */
        if (!nvfl_open()) {
            ALOGE("Unable to open fliplog for writing. See warnings above for details.");
        }

        /* Check whether we want to enable ftrace */
        if (dev->props.dynamic.ftrace_enable) {
            dev->device.set = hwc_set_ftrace;
        }

        dev->num_compositors = 3;
        dev->compositors[0] = HWC_Compositor_GLComposer;
        dev->compositors[1] = HWC_Compositor_GLDrawTexture;
        dev->compositors[2] = HWC_Compositor_Gralloc;

        /* Debug interface */
        dev->post_wait = hwc_post_wait;
        dev->get_buffers = hwc_get_buffers;
        dev->set_prop = hwc_set_prop;

        *device = &dev->device.common;
        status = 0;
    }

    return status;

fail:
    if (dev) {
        hwc_device_close((struct hw_device_t *)dev);
    }
    return status;
}
