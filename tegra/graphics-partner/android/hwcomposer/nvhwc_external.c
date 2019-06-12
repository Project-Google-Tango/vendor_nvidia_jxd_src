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
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 */

#include "nvhwc.h"
#include "nvhwc_debug.h"
#include "nvhwc_external.h"

#ifndef ABS
#define ABS(x) ((x) < 0 ? -(x) : (x))
#endif

/* Mirror mode detection */

static int layers_match(struct nvhwc_context *ctx,
                        hwc_layer_t *a, hwc_layer_t *b)
{
    /* Deem a dim layer to be matching */
    if ((a->flags & HWC_SKIP_LAYER) && (b->flags & HWC_SKIP_LAYER) &&
        !a->handle && !b->handle) {
        return NV_TRUE;
    }

    #define MATCH(field) if (a->field != b->field) {    \
        MIRRORLOG("mismatch: %s", #field);              \
        return NV_FALSE;                                \
    }

    MATCH(compositionType);
    MATCH(hints);
    MATCH(flags);

    switch (a->compositionType) {
    case HWC_BACKGROUND:
        if (memcmp(&a->backgroundColor, &b->backgroundColor,
                   sizeof(a->backgroundColor))) {
            MIRRORLOG("mismatch: background color");
            return NV_FALSE;
        } else {
            return NV_TRUE;
        }
        break;
    case HWC_FRAMEBUFFER_TARGET:
        return NV_TRUE;
    default:
        NV_ASSERT(a->compositionType == HWC_FRAMEBUFFER);
        break;
    }

    MATCH(handle);
    MATCH(blending);
    MATCH(sourceCrop.left);
    MATCH(sourceCrop.top);
    MATCH(sourceCrop.right);
    MATCH(sourceCrop.bottom);

    #undef MATCH

    if (combine_transform(a->transform, ctx->panel_transform) != (int)b->transform) {
        MIRRORLOG("mismatch: transform");
        return NV_FALSE;
    }

    /* displayFrame is checked later */

    return NV_TRUE;
}

/* Calculate the scale and offset applied to external display
 * layers.
 */
static void get_panel_xform(struct nvhwc_context *ctx,
                            const hwc_rect_t *src, const hwc_rect_t *dst,
                            nvhwc_xform_t *xf)
{
    int x, y;

    /* Calculate external scale */
    hwc_get_scale(ctx->panel_transform, src, dst, &xf->x_scale, &xf->y_scale);

    /* Calculate src origin in dst coordinates */
    if (ctx->panel_transform & HWC_TRANSFORM_ROT_90) {
        struct nvhwc_display *dpy = &ctx->displays[HWC_DISPLAY_PRIMARY];
        x = 0.5 + xf->y_scale * src->top;
        y = 0.5 + xf->x_scale * (dpy->device_clip.right - src->right);
    } else {
        x = 0.5 + xf->x_scale * src->left;
        y = 0.5 + xf->y_scale * src->top;
    }

    /* Calculate external offset */
    xf->x_off = dst->left - x;
    xf->y_off = dst->top - y;
}

static void apply_panel_xform(const struct nvhwc_context *ctx,
                              const hwc_rect_t *src, hwc_rect_t *dst)
{
    const struct nvhwc_display *dpy = &ctx->displays[HWC_DISPLAY_PRIMARY];

    /* Rotate into destination orientation */
    hwc_transform_rect(ctx->panel_transform, src, dst, dpy->config.res_x,
                       dpy->config.res_y);

    xform_apply(&ctx->mirror.xf, dst);
}

static int r_equal_almost(const hwc_rect_t *src, const hwc_rect_t *dst,
                          int tolerance)
{
    #define MATCH(field) {                                      \
        int diff = src->field - dst->field;                     \
        if (ABS(diff) > tolerance) {                            \
            MIRRORLOG("mismatch: displayFrame.%s", #field);     \
            return NV_FALSE;                                    \
        }                                                       \
    }

    MATCH(top);
    MATCH(left);
    MATCH(right);
    MATCH(bottom);

    #undef MATCH

    return NV_TRUE;
}

static int check_mirror_mode(struct nvhwc_context *ctx,
                             hwc_display_contents_t **displays)
{
    hwc_display_contents_t *d0 = displays[0];
    hwc_display_contents_t *d1 = displays[1];
    size_t ii;

    /* If either display is blank, we're not mirroring */
    if (ctx->displays[0].blank || ctx->displays[1].blank) {
        MIRRORLOG("%s: fail blank", __func__);
        return NV_FALSE;
    }

    /* Displays must have the same number of layers */
    if (d0->numHwLayers != d1->numHwLayers) {
        MIRRORLOG("%s: fail numHwLayers", __func__);
        return NV_FALSE;
    }

    /* Compare each layer to ensure they all match */
    for (ii = 0; ii < d0->numHwLayers; ii++) {
        if (!layers_match(ctx, &d0->hwLayers[ii], &d1->hwLayers[ii])) {
            MIRRORLOG("%s: fail layer %d match", __func__, ii);
            return NV_FALSE;
        }
    }

    /* Determine the scale and bias applied to primary layers for
     * mirroring on the external display.
     */
    get_panel_xform(ctx,
                    &displays[0]->hwLayers[0].displayFrame,
                    &displays[1]->hwLayers[0].displayFrame,
                    &ctx->mirror.xf);

    /* Now check the displayFrame.  The xform is derived from the
     * first layer so start checking at the second layer.
     */
    for (ii = 1; ii < d0->numHwLayers; ii++) {
        hwc_rect_t *src = &d0->hwLayers[ii].displayFrame;
        hwc_rect_t *dst = &d1->hwLayers[ii].displayFrame;
        hwc_rect_t tmp;

        switch (d0->hwLayers[ii].compositionType) {
        case HWC_BACKGROUND:
        case HWC_FRAMEBUFFER_TARGET:
            continue;
        default:
            break;
        }

        apply_panel_xform(ctx, src, &tmp);

        if (!r_equal_almost(&tmp, dst, 1)) {
            MIRRORLOG("displayFrame mismatch on layer %d", ii);
            MIRRORLOG("xform: %d %d %.2f %.2f", ctx->mirror.xf.x_off, ctx->mirror.xf.y_off,
                ctx->mirror.xf.x_scale, ctx->mirror.xf.y_scale);
            MIRRORLOG("display0: %d %d %d %d", src->left, src->top, src->right, src->bottom);
            MIRRORLOG("display1: %d %d %d %d", dst->left, dst->top, dst->right, dst->bottom);
            MIRRORLOG("tmp: %d %d %d %d", tmp.left, tmp.top, tmp.right, tmp.bottom);
            return NV_FALSE;
        }
    }

    MIRRORLOG("%s: mirror mode detected", __func__);
    return NV_TRUE;
}

void hwc_update_mirror_state(struct nvhwc_context *ctx, size_t numDisplays,
                             hwc_display_contents_t **displays)
{
    /* If either display contents is missing or if the dirty flags
     * differ, we're not mirroring so just disable mirror mode and
     * return clean status to short circuit further processing.
     */
    if (numDisplays < 2 || !displays[0] || !displays[1] ||
        ((displays[0]->flags ^ displays[1]->flags) & HWC_GEOMETRY_CHANGED)) {
        ctx->mirror.dirty = ctx->mirror.enable;
        ctx->mirror.enable = NV_FALSE;
        return;
    }

    ctx->mirror.dirty = !!(displays[0]->flags & HWC_GEOMETRY_CHANGED);

    if (ctx->mirror.dirty) {
        ctx->mirror.enable = check_mirror_mode(ctx, displays);
    }
}

/* Video optimizations */

static inline int supported_stereo_mode(struct nvhwc_context *ctx,
                                        hwc_layer_t* cur)
{
    NvNativeHandle *h = (NvNativeHandle *)cur->handle;

    /* Enable only if the property is set */
    if (!ctx->props.fixed.stereo_enable) {
        return 0;
    }

    NV_ASSERT(h && h->Buf);
    if (h->Buf->StereoInfo & NV_STEREO_ENABLE_MASK) {
        int fpa = NV_STEREO_GET_FPA(h->Buf->StereoInfo);
        int ci = NV_STEREO_GET_CI(h->Buf->StereoInfo);

        if (NV_STEREO_IS_SUPPORTED_MODE(fpa, ci)) {
            return 1;
        }
        ALOGE("Unsupported Stereo format fpa:%x ci:%x", fpa, ci);
    }

    return 0;
}

static void get_supported_stereo_size(hwc_layer_t* cur, int *x, int *y)
{
    NvNativeHandle *h = (NvNativeHandle *)cur->handle;

    NV_ASSERT(h && h->Buf);
    NV_ASSERT(h->Buf->StereoInfo & NV_STEREO_ENABLE_MASK);

    *x = *y = 0;
    NvStereoType type = (NvStereoType)(h->Buf->StereoInfo & NV_STEREO_MASK);
    // Currently, we support 1080p stereo only for packed LR/TB stereo
    // whose source is originally 1920x1080
    switch (type) {
        case NV_STEREO_LEFTRIGHT:
        case NV_STEREO_RIGHTLEFT:
            if ((r_width(&cur->sourceCrop) << 1) == 1920) {
                *x = 1920;
                *y = 1080;
            }
            break;
        case NV_STEREO_TOPBOTTOM:
        case NV_STEREO_BOTTOMTOP:
            if ((r_height(&cur->sourceCrop) << 1) == 1080) {
                *x = 1920;
                *y = 1080;
            }
            break;
        case NV_STEREO_SEPARATELR:
        case NV_STEREO_SEPARATERL:
        default:
            break;
    }
    if (!*x) {
        *x = 1280;
        *y = 720;
    }
}

void xform_get_center_scale(nvhwc_xform_t *xf,
                            int src_w, int src_h,
                            int dst_w, int dst_h,
                            float par)
{
    float src_aspect = (float)src_w / src_h;
    float dst_aspect = (float)dst_w * par / dst_h;   /* dst is wider by par */

    if (src_aspect > dst_aspect) {
        xf->x_scale = (float) dst_w / src_w;
        xf->y_scale = xf->x_scale * par;
        xf->y_off = (dst_h - src_h * xf->y_scale) / 2;
        xf->x_off = 0;
    } else {
        xf->y_scale = (float) dst_h / src_h;
        xf->x_scale = xf->y_scale / par;
        xf->x_off = (dst_w - src_w * xf->x_scale) / 2;
        xf->y_off = 0;
    }
}

static void center_scale_layer(struct fb_var_screeninfo *mode,
                               struct nvhwc_prepare_layer *ll)
{
    nvhwc_xform_t xf;
    float par = 1.0;    /* pixel aspect ratio of given mode */
    int src_w, src_h;

    /* Translate to origin */
    ll->dst.right -= ll->dst.left;
    ll->dst.bottom -= ll->dst.top;
    ll->dst.left = ll->dst.top = 0;

    /* Anamorphic 16:9 format for 720x480 (480pH mode in CAE standard) and
     * 720x576 (576pH) resolutions. Available only for video playback.
     * Two assumptions are made:
     *   1) If TV/monitor provides 720p mode, its native size is 16:9, thus 4:3
     *      DAR will not be selected by dc driver for 720x576 (576p) resulution;
     *   2) TV/monitor will stretch width so that resulting DAR is 16:9; this
     *      behavior may need to be forced from within TV/monitor's own menu.
     *      The DAR has to be inferred from mode dimensions because original
     *      informatiopn from EDID is not preserved through fb_var_screeninfo.
     */
    if (mode->xres == 720 && (mode->yres == 480 || mode->yres == 576)) {
        par = mode->yres / 720.0 * 16 / 9;
    }

    xform_get_center_scale(&xf, ll->dst.right, ll->dst.bottom,
                           mode->xres, mode->yres, par);
    xform_apply(&xf, &ll->dst);
}

// Values from HDMI1.4a spec
#define STEREO_720p_GAP 30
#define STEREO_1080p_GAP 45

static void get_mode_dimensions(struct fb_var_screeninfo *mode,
                                NvPoint *image_size, NvPoint *device_size)
{
    device_size->x = mode->xres;
    device_size->y = mode->yres;
    image_size->x = mode->xres;
    image_size->y = mode->yres;

    if (mode->vmode & FB_VMODE_STEREO_FRAME_PACK) {
        device_size->y *= 2;
        if (mode->xres == 1920) {
            device_size->y += STEREO_1080p_GAP;
        } else {
            device_size->y += STEREO_720p_GAP;
        }
    } else if (mode->vmode & FB_VMODE_STEREO_LEFT_RIGHT) {
        image_size->x /= 2;
    } else if (mode->vmode & FB_VMODE_STEREO_TOP_BOTTOM) {
        image_size->y /= 2;
    }
}

static NvBool prepare_stereo(struct fb_var_screeninfo *mode,
                             struct nvhwc_prepare_state *prepare)
{
    struct nvhwc_prepare_layer *eye_l = &prepare->layers[0];
    struct nvhwc_prepare_layer *eye_r = &prepare->layers[1];
    NvNativeHandle *h = (NvNativeHandle *)eye_l->layer->handle;
    NvStereoType type = (NvStereoType)(h->Buf->StereoInfo & NV_STEREO_MASK);
    int leftfirst =
        NV_STEREO_GET_CI(h->Buf->StereoInfo) == NV_STEREO_CI_LEFTFIRST;
    int nxtidx = (h->Type == NV_NATIVE_BUFFER_STEREO) ? 1 : 0;
    int offset;

    /* Left eye is the sourceCrop so handle this like a normal video
     * layer.
     */
    center_scale_layer(mode, eye_l);
    eye_l->surf_index = leftfirst ? 0 : nxtidx;

    /* Right eye is a simple transform from the left; start with the
     * left eye.
     */
    NV_ASSERT(prepare->numLayers == 1);
    prepare->numLayers++;
    memcpy(eye_r, eye_l, sizeof(*eye_l));
    eye_r->surf_index = leftfirst ? nxtidx : 0;

    /* Adjust right eye source rect */
    switch (type) {
    case NV_STEREO_LEFTRIGHT:
        offset = r_width(&eye_l->src);
        eye_r->src.left += offset;
        eye_r->src.right += offset;
        break;
    case NV_STEREO_RIGHTLEFT:
        offset = r_width(&eye_l->src);
        eye_r->src.left -= offset;
        eye_r->src.right -= offset;
        break;
    case NV_STEREO_TOPBOTTOM:
        offset = r_height(&eye_l->src);
        eye_r->src.top += offset;
        eye_r->src.bottom += offset;
        break;
    case NV_STEREO_BOTTOMTOP:
        offset = r_height(&eye_l->src);
        eye_r->src.top -= offset;
        eye_r->src.bottom -= offset;
        break;
    case NV_STEREO_SEPARATELR:
    case NV_STEREO_SEPARATERL:
        break;
    default:
        ALOGE("Unsupported stereo type 0x%x", (int)type);
        return NV_FALSE;
    }

    /* Adjust destination rect */
    if (mode->vmode & FB_VMODE_STEREO_FRAME_PACK) {
        if (mode->xres == 1920) {
            eye_r->dst.top    += mode->yres + STEREO_1080p_GAP;
            eye_r->dst.bottom += mode->yres + STEREO_1080p_GAP;
        } else {
            eye_r->dst.top    += mode->yres + STEREO_720p_GAP;
            eye_r->dst.bottom += mode->yres + STEREO_720p_GAP;
        }
    } else if (mode->vmode & FB_VMODE_STEREO_LEFT_RIGHT) {
        /* Change only the horizontal cooridinates */
        int dst_left = eye_l->dst.left / 2;
        int dst_width = r_width(&eye_l->dst) / 2;
        int off_l = 0;
        int off_r = mode->xres / 2;

        if (type == NV_STEREO_SEPARATERL) {
            NVGR_SWAP(off_l, off_r);
        }

        eye_l->dst.left  = off_l + dst_left;
        eye_l->dst.right = off_l + dst_left + dst_width;
        eye_r->dst.left  = off_r + dst_left;
        eye_r->dst.right = off_r + dst_left + dst_width;
    } else if (mode->vmode & FB_VMODE_STEREO_TOP_BOTTOM) {
        /* Change only the vertical coordinates */
        int dst_top = eye_l->dst.top / 2;
        int dst_height = r_height(&eye_l->dst) / 2;
        int off_l = 0;
        int off_r = mode->yres / 2;

        if (type == NV_STEREO_SEPARATERL) {
            NVGR_SWAP(off_l, off_r);
        }

        eye_l->dst.top    = off_l + dst_top;
        eye_l->dst.bottom = off_l + dst_top + dst_height;
        eye_r->dst.top    = off_r + dst_top;
        eye_r->dst.bottom = off_r + dst_top + dst_height;
    } else {
        ALOGE("Unsupported HDMI output stereo type...");
        return NV_FALSE;
    }

    return NV_TRUE;
}

static inline int modes_same_aspect(struct fb_var_screeninfo *m0,
                                    struct fb_var_screeninfo *m1)
{
    return (m0->xres * m1->yres == m0->yres * m1->xres) &&
           (((m0->xres >= m0->yres) && (m1->xres >= m1->yres)) ||
            ((m0->xres  < m0->yres) && (m1->xres  < m1->yres)));
}

/* Synthesize a window config that optimizes for certain types of
 * content.
 */
NvBool hwc_prepare_external(struct nvhwc_context *ctx,
                            struct nvhwc_display *dpy,
                            struct nvhwc_prepare_state *prepare,
                            hwc_display_contents_t *display)
{
    HWC_Modification type = HWC_Modification_None;

    int index = hwc_find_layer(display,
                               GRALLOC_USAGE_EXTERNAL_DISP |
                               NVGR_USAGE_STEREO,
                               NV_TRUE);

    /* Ensure the layer occupies at least half of the screen before
     * giving it special consideration.
     */
    if (index >= 0) {
        hwc_layer_t *cur = &display->hwLayers[index];
        int video_size = r_width(&cur->displayFrame) *
                         r_height(&cur->displayFrame);
        int screen_size = dpy->default_mode->xres * dpy->default_mode->yres;

        if (video_size < (screen_size >> 1)) {
            index = -1;
        }
    }

    if (index >= 0) {
        hwc_layer_t *cur = &display->hwLayers[index];
        struct fb_var_screeninfo *mode;
        enum NvFbVideoPolicy policy;
        int xres = 0, yres = 0;
        NvBool stereo = NV_FALSE;
        size_t ii;
        NvPoint image_size, device_size;

        /* Look for supported 3D video mode */
        if (dpy->hotplug.cached.stereo && supported_stereo_mode(ctx, cur)) {
            policy = NvFbVideoPolicy_Stereo_Closest;
            get_supported_stereo_size(cur, &xres, &yres);
            stereo = NV_TRUE;
            type = HWC_Modification_Custom;
        } else {
            policy = NvFbVideoPolicy_RoundUp;
            if (cur->transform & HWC_TRANSFORM_ROT_90) {
                xres = r_height(&cur->sourceCrop);
                yres = r_width(&cur->sourceCrop);
            } else {
                xres = r_width(&cur->sourceCrop);
                yres = r_height(&cur->sourceCrop);
            }
            if (ctx->props.fixed.hdmi_videoview) {
                type = HWC_Modification_Custom;
            } else {
                type = HWC_Modification_Xform;
            }
        }

        mode = nvfb_choose_display_mode(ctx->fb, HWC_DISPLAY_EXTERNAL,
                                        xres, yres, policy);

        if (!mode) {
            goto fail;
        }

        if (type == HWC_Modification_Custom) {
            get_mode_dimensions(mode, &image_size, &device_size);
        } else {
            device_size.x = mode->xres;
            device_size.y = mode->yres;
            image_size.x = dpy->default_mode->xres;
            image_size.y = dpy->default_mode->yres;
        }

        /* This should never fail but it has a return value so lets be
         * paranoid.
         */
        if (hwc_set_display_mode(ctx, HWC_DISPLAY_EXTERNAL, mode,
                                 &image_size, &device_size)) {
            goto fail;
        }

        if (type == HWC_Modification_Custom) {
            /* Until this point we've made no changes.  Any subsequent
             * failures require that we undo anything we touch, hence we
             * use the 'fail' label to clean up.
             */

            NV_ASSERT(prepare->numLayers == 0);
            hwc_prepare_add_layer(dpy, prepare, &display->hwLayers[index], index);

            if (stereo) {
                /* Prepare both eyes as separate layers */
                if (!prepare_stereo(mode, prepare)) {
                    goto fail;
                }
            } else {
                /* Center and scale video to fullscreen */
                center_scale_layer(mode, &prepare->layers[0]);
            }

            /* Tell surfaceflinger we'll handle all layers */
            for (ii = 0; ii < display->numHwLayers; ii++) {
                hwc_layer_t *cur = &display->hwLayers[ii];

                /* If any SKIP layers are present, we should not be in
                 * mirror mode. Dim layer is an exception.
                 */
                NV_ASSERT(!((cur->flags & HWC_SKIP_LAYER) && cur->handle));

                if (cur->compositionType == HWC_FRAMEBUFFER ||
                    cur->compositionType == HWC_BACKGROUND) {
                    cur->compositionType = HWC_OVERLAY;
                }
            }

            prepare->use_windows = NV_TRUE;
        }
    } else {
        hwc_set_display_mode(ctx, HWC_DISPLAY_EXTERNAL, NULL, NULL, NULL);
    }

    return type == HWC_Modification_Custom;

fail:
    /* Undo any local changes */
    hwc_set_display_mode(ctx, HWC_DISPLAY_EXTERNAL, NULL, NULL, NULL);
    prepare->numLayers = 0;
    return NV_FALSE;
}
