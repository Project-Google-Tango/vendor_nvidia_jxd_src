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

#ifndef _NVHWC_H
#define _NVHWC_H

#include <hardware/hwcomposer.h>
#include <cutils/log.h>
#include <sync/sync.h>

/* Compatibility */
typedef hwc_layer_1_t hwc_layer_t;
typedef hwc_display_contents_1_t hwc_display_contents_t;
typedef hwc_composer_device_1_t hwc_composer_device_t;

#include "nvassert.h"
#include "nvglcomposer.h"
#include "nvgralloc.h"
#include "nvfb.h"
#include "nvhwc_util.h"
#include "nvhwc_composite.h"
#include "nvhwc_props.h"
#include "nvhwc_debug.h"

#define TOUCH_SLOWSCAN_ENABLE 1

typedef struct nvhwc_xform {
    float x_scale;
    float y_scale;
    int x_off;
    int y_off;
} nvhwc_xform_t;

typedef enum {
    HWC_Modification_None,
    HWC_Modification_Xform,
    HWC_Modification_Custom,
} HWC_Modification;

struct nvhwc_modification {
    HWC_Modification type;
    nvhwc_xform_t xf;
};

#define HWC_MAX_LAYERS 32
/* Add one for the cursor */
#define HWC_MAX_BUFFERS (NVFB_MAX_WINDOWS + 1)

struct nvhwc_prepare_layer {
    /* Original list index */
    int index;
    /* Original layer */
    hwc_layer_t *layer;
    /* Copied from original layer, may be modified locally */
    int transform;
    int blending;
    int surf_index;
    hwc_rect_t src;
    hwc_rect_t dst;
};

struct nvhwc_prepare_map {
    /* Hardware window index */
    int window;
    /* Index of layer in filtered list */
    int layer_index;
};

struct nvhwc_prepare_state {
    /* Original layer list */
    hwc_display_contents_t *display;
    /* Map index currently being processed */
    int current_map;
    /* Mask of available windows */
    unsigned window_mask;
    /* Layers to be assigned if possible */
    int numLayers;
    struct nvhwc_prepare_layer layers[HWC_MAX_LAYERS];
    /* Assigned window map */
    struct nvhwc_prepare_map layer_map[HWC_MAX_BUFFERS];
    /* Priority layer, if present */
    hwc_layer_t *priority_layer;
    /* Use windows or composite? */
    NvBool use_windows;
    /* Protected content present? */
    NvBool protect;
    /* Use only sequential blending */
    NvBool sequential;
    /* Number of available windows with sequential blending */
    int free_sequential;

    /* Range of unprocessed layers */
    struct {
        int start;
        int limit;
    } remainder;

    struct {
        /* Dedicated layer for the framebuffer */
        struct nvhwc_prepare_layer layer;
        /* bounding rectangle of all composited layers in layer space */
        hwc_rect_t bounds;
        /* framebuffer blending mode */
        int blending;
    } fb;

    struct nvhwc_prepare_layer *cursor_layer;

    /* Display modification */
    struct nvhwc_modification modification;
};

struct nvhwc_window_mapping {
    int index;
    NvGrScratchSet *scratch; /* scratch buffer set for this window */
    int surf_index; /* source surface index for scratch blit*/
};

// Dynamic idle detection forces compositing when the framerate drops
// below IDLE_MINIMUM_FPS frames per second.  This is determined by
// comparing the current time with a timeout value relative to
// IDLE_FRAME_COUNT frames ago.  The time limit is a function of the
// minimum_fps value.
//
// The original selection of 20 fps as the crossover point is based on
// the assumption of 60fps refresh - this is really intended to be
// refresh_rate/3.  If there are two overlapping fullscreen layers,
// which is typical, over three refreshes there will be six
// isochronous reads in display.  If the two layers are composited,
// there are two reads and one write in 3D, plus three isochronous
// reads - nominally the same bandwidth requirement but easier on the
// memory controller due to less isochronous traffic.  As the frame
// rate drops, the scales tip further toward compositing since the
// three memory touches in 3D are amortized over additional refreshes.
//
// In practice it seems that 20 is too high a threshold, as benchmarks
// which are shader limited and run just below 20fps are negatively
// impacted.  See bug #934644.

#define IDLE_FRAME_COUNT 8
#define IDLE_MINIMUM_FPS 8

struct nvhwc_idle_machine {
    struct timespec frame_times[IDLE_FRAME_COUNT];
    struct timespec time_limit;
    struct timespec timeout;
    uint8_t minimum_fps;
    uint8_t frame_index;
    uint8_t composite;
    timer_t timer;
    int32_t enabled;
};

// Framebuffer cache determines if we can skip re-compositing when fb
// layers are unchanged.

#define FB_CACHE_LAYERS 8

struct nvhwc_fb_cache_layer {
    int index;
    buffer_handle_t handle;
    uint32_t transform;
    int32_t blending;
    hwc_rect_t sourceCrop;
    hwc_rect_t displayFrame;
};

struct nvhwc_fb_cache {
    int numLayers;
    HWC_Compositor compositor;
    struct nvhwc_fb_cache_layer layers[FB_CACHE_LAYERS];
};

#define OLD_CACHE_INDEX(dpy) ((dpy)->fb_cache_index)
#define NEW_CACHE_INDEX(dpy) ((dpy)->fb_cache_index ^ 1)

struct nvhwc_display {
    int type;
    /* Bug 1357901 */
    int ignore_initial_blank;
    /* display caps */
    struct nvfb_display_caps caps;
    /* current framebuffer config (mode) */
    struct nvfb_config config;
    /* display bounds */
    hwc_rect_t device_clip;
    /* Advertised framebuffer size */
    struct {
        int xres;
        int yres;
    } fb_size;
    /* default display mode */
    struct fb_var_screeninfo *default_mode;
    /* current display mode (content-dependent) */
    struct fb_var_screeninfo *current_mode;
    /* blank state */
    int blank;

    /* hotplug status */
    struct {
        struct nvfb_hotplug_status cached;
        struct nvfb_hotplug_status latest;
    } hotplug;

    /* transform from layer space to device space */
    int transform;
    /* display bounds in layer space */
    hwc_rect_t layer_clip;

    /* currently locked buffers */
    NvNativeHandle *buffers[HWC_MAX_BUFFERS];

    /* current frame sync fence */
    int currFenceFd;
    /* previous frame sync fence */
    int prevFenceFd;

    /* window config */
    NvGrOverlayConfig conf;
    /* windows from top to bottom, including framebuffer if needed */
    struct nvhwc_window_mapping map[HWC_MAX_BUFFERS];
    struct {
        struct nvhwc_window_mapping map;
        NvNativeHandle *buffer;
    } cursor;

    /* base window requirements for the framebuffer */
    unsigned fb_req;
    /* index of the framebuffer in map, -1 if none */
    int fb_index;
    /* index of the framebuffer in display list */
    int fb_target;
    /* number of composited layers */
    int fb_layers;
    /* layers marked for clear in SF */
    uint32_t clear_layers;

    /* Framebuffer cache (double buffered) */
    struct nvhwc_fb_cache fb_cache[2];
    /* Active cache index */
    uint8_t fb_cache_index;
    /* Bool, reuse cached framebuffer */
    uint8_t fb_recycle;
    /* windows overlap */
    uint8_t windows_overlap;
    uint8_t unused[1];

    /* Scratch buffers */
    NvGrScratchClient *scratch;

    /* Composite state */
    struct nvhwc_composite composite;

    struct nvhwc_modification modification;
};

struct nvhwc_context {
    hwc_composer_device_t device; /* must be first */
    NvGrModule* gralloc;

    struct nvfb_device *fb;
    /* Per-display information and state */
    struct nvhwc_display displays[HWC_NUM_DISPLAY_TYPES];

    struct {
        pthread_mutex_t mutex;
    } hotplug;

    struct {
        /* mirror state may have changed */
        NvBool dirty;
        /* mirror mode is currently active */
        NvBool enable;
        /* primary to external scale and bias */
        nvhwc_xform_t xf;
    } mirror;

    /* need to reconfigure display windows */
    NvBool reconfigure_windows;

    /* show only video layers (GRALLOC_USAGE_EXTERNAL_DISP | NVGR_USAGE_CLOSED_CAP)
     * on hdmi during video playback or camera preview */
    NvBool hdmi_video_mode;
    /* enable cinema mode = video layers shown are only on hdmi;
     * ignored if hdmi_video_mode is false */
    NvBool cinema_mode;

    /* stereo content flag cache */
    NvBool bStereoGLAppRunning;

    /* index of the video out layer */
    int video_out_index;
    /* index of the closed captioning layer */
    int cc_out_index;
    /* index of the on-screen display layer */
    int osd_out_index;
    /* cache HDMI behavior */
    int hdmi_mode;
    /* native orientation of panel relative to home */
    int panel_transform;
    /* framework hwc callbacks */
    hwc_procs_t *procs;

    /* Idle detection machinery */
    struct nvhwc_idle_machine idle;

    int num_compositors;
    HWC_Compositor compositors[HWC_Compositor_Count];

    /* Configurable properties */
    hwc_props_t props;

    /* Cursor mode (should eventually go away) */
    enum NvFbCursorMode cursor_mode;

    struct {
        int32_t         sync;
        int32_t         shutdown;
        pthread_t       thread;
        pthread_mutex_t mutex;
        pthread_cond_t  condition;
    } event;

#if TOUCH_SLOWSCAN_ENABLE
    int fd_slowscan;
    int slowscan_shadowstate;
#endif

    /* debug interface */
    void (*post_wait)(struct nvhwc_context *ctx, int disp);
    void (*get_buffers)(struct nvhwc_context *ctx, int disp,
                        NvNativeHandle *buffers[HWC_MAX_BUFFERS]);
    int (*set_prop)(struct nvhwc_context *ctx,
                    const char *prop,
                    const char *value);
};

static inline int hwc_get_usage(hwc_layer_t* l)
{
    if (l->handle)
        return ((NvNativeHandle*)l->handle)->Usage;
    else
        return 0;
}

static inline NvRmSurface* get_nvrmsurface(hwc_layer_t* l)
{
    return (NvRmSurface*)&((NvNativeHandle*)l->handle)->Surf;
}

static inline NvColorFormat get_colorformat(hwc_layer_t* l)
{
    return get_nvrmsurface(l)->ColorFormat;
}

static inline int get_halformat(hwc_layer_t* l)
{
    return ((NvNativeHandle*)l->handle)->Format;
}

int hwc_find_layer(hwc_display_contents_t *list, int usage, NvBool unique);
int hwc_get_window(const struct nvfb_display_caps *caps,
                   unsigned* mask, unsigned req, unsigned* unsatisfied);

void hwc_prepare_add_layer(struct nvhwc_display *dpy,
                           struct nvhwc_prepare_state *prepare,
                           hwc_layer_t *cur, int index);

void hwc_clear_layer_enable(struct nvhwc_display *dpy,
                            hwc_display_contents_t *display);

/* We use 2D to perform ROT_90 *before* display performs FLIP_{H|V}.
 * hardware.h states that ROT_90 is applied *after* FLIP_{H|V}.
 *
 * If both FLIP flags are applied the result is the same. However if
 * only one is applied (i.e. the surface is mirrored) the order does
 * matter. To compensate, swap these flags.
 */
static inline int fix_transform(int transform)
{
    switch (transform) {
    case HWC_TRANSFORM_ROT_90 | HWC_TRANSFORM_FLIP_H:
        return HWC_TRANSFORM_ROT_90 | HWC_TRANSFORM_FLIP_V;
    case HWC_TRANSFORM_ROT_90 | HWC_TRANSFORM_FLIP_V:
        return HWC_TRANSFORM_ROT_90 | HWC_TRANSFORM_FLIP_H;
    default:
        return transform;
    }
}

/* Return a transform which is the combination of two transforms.
 * Note that the transforms are not commutative due to the order of
 * rot90 and h/v flip. The combined transform is ('+' means transformation
 * chaining):
 *      t0 + t1 = t0(flip) + t0(rot90) + t1(flip) + t1(rot90) =
 *              = t0(flip) + fix_transform(t1(flip) + t0(rot90)) + t1(rot90) =
 *              = t0(flip) + t1(fixed_flip) + t0(rot90) + t1(rot90)
 */
static inline int combine_transform(int t0, int t1)
{
    int t = ((t0 & HWC_TRANSFORM_ROT_180) | (t1 & HWC_TRANSFORM_ROT_90)) ^
            fix_transform((t0 & HWC_TRANSFORM_ROT_90) |
                          (t1 & HWC_TRANSFORM_ROT_180));

    if ((t0 & t1) & HWC_TRANSFORM_ROT_90) {
        t ^= HWC_TRANSFORM_ROT_180;
    }

    return t;
}

static inline void xform_apply(const nvhwc_xform_t *xf, hwc_rect_t *rect)
{
    rect->left = rect->left * xf->x_scale + xf->x_off + 0.5;
    rect->right = rect->right * xf->x_scale + xf->x_off + 0.5;
    rect->top = rect->top * xf->y_scale + xf->y_off + 0.5;
    rect->bottom = rect->bottom * xf->y_scale + xf->y_off + 0.5;
}

int hwc_get_scale(int transform, const hwc_rect_t *src, const hwc_rect_t *dst,
                  float *scale_w, float *scale_h);

void hwc_transform_rect(int transform, const hwc_rect_t *src, hwc_rect_t *dst,
                        int width, int height);

static inline NvBool hwc_layer_is_yuv(hwc_layer_t* l)
{
    NvColorFormat format = get_colorformat(l);

    switch (NV_COLOR_GET_COLOR_SPACE(format)) {
    case NvColorSpace_YCbCr601:
    case NvColorSpace_YCbCr601_RR:
    case NvColorSpace_YCbCr601_ER:
    case NvColorSpace_YCbCr709:
        return NV_TRUE;
    }

    return NV_FALSE;
}

static inline NvBool hwc_layer_has_alpha(hwc_layer_t* l)
{
    NvColorFormat format = get_colorformat(l);

    if (NV_COLOR_SWIZZLE_GET_W(format) != NV_COLOR_SWIZZLE_1)
        return NV_TRUE;

    return NV_FALSE;
}

static inline NvBool hwc_layer_is_protected(hwc_layer_t* l)
{
    return !!(hwc_get_usage(l) & GRALLOC_USAGE_PROTECTED);
}

static inline NvGrScratchSet *
scratch_assign(struct nvhwc_display *dpy, int transform, size_t width,
               size_t height, NvColorFormat format, NvRmSurfaceLayout layout,
               NvRect *src_crop, int protect)
{
    return dpy->scratch->assign(dpy->scratch, transform, width, height,
                                format, layout, src_crop, protect);
}

static inline void scratch_frame_start(struct nvhwc_display *dpy)
{
    dpy->scratch->frame_start(dpy->scratch);
}

static inline void scratch_frame_end(struct nvhwc_display *dpy)
{
    dpy->scratch->frame_end(dpy->scratch);
}

static inline int rotation_to_transform(int rotation)
{
    int r = -1;

    switch (rotation) {
        case 0:
            r = 0;
            break;
        case 90:
            r = HWC_TRANSFORM_ROT_90;
            break;
        case 180:
            r = HWC_TRANSFORM_ROT_180;
            break;
        case 270:
            r = HWC_TRANSFORM_ROT_270;
            break;
        default:
            ALOGW("Ignoring invalid panel rotation %d", rotation);
    }

    return r;
}

int hwc_set_display_mode(struct nvhwc_context *ctx, int disp,
                         struct fb_var_screeninfo *mode,
                         NvPoint *image_size, NvPoint *device_size);

#endif /* ifndef _NVHWC_H */
