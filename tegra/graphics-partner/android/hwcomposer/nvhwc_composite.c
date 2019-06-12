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
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
 */


#include "nvhwc.h"
#include "nvsync.h"
#include "nvviccomposer.h"
#include "nvgr2dcomposer.h"

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include "nvatrace.h"

static nvcomposer_t builtin_sf = {
    "surfaceflinger",      // name
    0,                     // caps
    NVCOMPOSER_MAX_LAYERS, // maxLayers
    0.0f,                  // minScale
    FLT_MAX,               // maxScale
    NULL,                  // close
    NULL,                  // prepare
    NULL,                  // set
};

void
hwc_composite_open(struct nvhwc_context *ctx,
                   struct nvhwc_display *dpy)
{
    int i;
    int result;

    memset(&dpy->composite, 0, sizeof(dpy->composite));

    dpy->composite.composers[HWC_Compositor_SurfaceFlinger] = &builtin_sf;

    dpy->composite.composers[HWC_Compositor_Gralloc] = nvviccomposer_create();
    if (dpy->composite.composers[HWC_Compositor_Gralloc] == NULL) {
        dpy->composite.composers[HWC_Compositor_Gralloc] =
            nvgr2dcomposer_create();
    }

    if (!ctx->props.fixed.no_egl) {
        dpy->composite.glc = glc_init();
        if (dpy->composite.glc != NULL) {
            dpy->composite.composers[HWC_Compositor_GLComposer] =
                glc_get(dpy->composite.glc, GLC_DRAW_ARRAYS);
            dpy->composite.composers[HWC_Compositor_GLDrawTexture] =
                glc_get(dpy->composite.glc, GLC_DRAW_TEXTURE);
        }
    }

    for (i=0; i<HWC_Compositor_Count; ++i) {
        if (dpy->composite.composers[i] == NULL) {
            ALOGW("Compositor not present: %d\n", i);
        }
    }
}

void
hwc_composite_close(struct nvhwc_context *ctx,
                    struct nvhwc_display *dpy)
{
    int i;

    for (i=0; i<HWC_Compositor_Count; ++i) {
        nvcomposer_t *composer = dpy->composite.composers[i];
        if (composer != NULL && composer->close != NULL) {
            composer->close(composer);
        }
    }

    glc_destroy(dpy->composite.glc);
}

void
hwc_composite_select(struct nvhwc_context *ctx,
                     struct nvhwc_display *dpy,
                     HWC_Compositor id)
{
    NV_ASSERT(id < HWC_Compositor_Count);
    if (dpy->composite.composers[id] != NULL) {
        dpy->composite.id = id;
    }
}

void
hwc_composite_override(struct nvhwc_context *ctx,
                       struct nvhwc_display *dpy,
                       HWC_Compositor id)
{
    if (ctx->props.dynamic.composite_fallback) {
        hwc_composite_select(ctx, dpy, id);
    }
}

int
hwc_composite_caps(struct nvhwc_context *ctx,
                   struct nvhwc_display *dpy)
{
    NV_ASSERT(dpy->composite.composers[dpy->composite.id] != NULL);
    return dpy->composite.composers[dpy->composite.id]->caps;
}

static int
compute_contents(struct nvhwc_context *ctx,
                 struct nvhwc_display *dpy,
                 struct nvhwc_prepare_state *prepare)
{
    nvcomposer_contents_t *contents = &dpy->composite.contents;
    int i;

    contents->numLayers = 0;
    contents->minScale = FLT_MAX;
    contents->maxScale = 0.0f;
    contents->flags = NVCOMPOSER_FLAG_GEOMETRY_CHANGED;
    r_copy(&contents->clip, &dpy->device_clip);

    // Currently, we only have two level of composition passes.
    // One from GLCOMPOSER/GRALLOC/SURFACEFLINGER and one from
    // display controller. Some of the compositors can compose
    // given layer list more efficiently if it know if there's
    // not other composition after that including display composition
    if (dpy->fb_index == (int) dpy->caps.num_windows - 1) {
        contents->flags |= NVCOMPOSER_FLAG_LAST_PASS;
    }

    /* Try to composite all layers marked as HWC_FRAMEBUFFER */
    for (i=0; i<prepare->numLayers; ++i) {
        struct nvhwc_prepare_layer *pl = &prepare->layers[i];
        hwc_layer_1_t *cl;
        float hScale;
        float vScale;
        float minScale;
        float maxScale;

        if (pl->layer->compositionType != HWC_FRAMEBUFFER) {
            continue;
        }

        if (contents->numLayers >= NVCOMPOSER_MAX_LAYERS) {
            NV_ASSERT(contents->numLayers == NVCOMPOSER_MAX_LAYERS);
            ALOGW("%s: exceeded layer count", __func__);
            /* This is a performance bug - we will fall back to
             * SurfaceFlinger compositing when this happens.  Should there
             * be an assert to make sure we notice and consider increasing
             * the list size?
             */
            return -ENOMEM;
        }

        dpy->composite.map[contents->numLayers] = pl->index;
        cl = contents->layers + contents->numLayers++;

        // Copy from original layer
        *cl = *pl->layer;

        // Update members that may have been modified during hwc prepare
        cl->transform = pl->transform;
        cl->blending = pl->blending;
        r_copy(&cl->sourceCrop, &pl->src);
        r_copy(&cl->displayFrame, &pl->dst);

        if (hwc_layer_is_protected(cl)) {
            contents->flags |= NVCOMPOSER_FLAG_PROTECTED;
        }

        hwc_get_scale(cl->transform,
                      &cl->sourceCrop,
                      &cl->displayFrame,
                      &hScale,
                      &vScale);

        if (hScale > vScale) {
            minScale = vScale;
            maxScale = hScale;
        } else {
            minScale = hScale;
            maxScale = vScale;
        }

        if (contents->minScale > minScale) {
            contents->minScale = minScale;
        }
        if (contents->maxScale < maxScale) {
            contents->maxScale = maxScale;
        }
    }

    return 0;
}

static void composite_prepare_layer(struct nvhwc_display *dpy,
                                    struct nvhwc_prepare_state *prepare)
{
    NvGrScratchSet *scratch = dpy->map[dpy->fb_index].scratch;

    /* If there is a scratch buffer, use the entire buffer */
    if (scratch) {
        /* This code assumes the scratch blit subsumes any 90 degree
         * rotation.  It may not subsume the entire transform.
         */
        NV_ASSERT(!((scratch->transform ^ prepare->fb.layer.transform) &
                    HWC_TRANSFORM_ROT_90));
        prepare->fb.layer.dst = prepare->fb.layer.src;
    } else {
        /* src is in layer coords */
        prepare->fb.layer.src = prepare->fb.bounds;

        /* dst is in device coords */
        if (prepare->fb.layer.transform) {
            hwc_transform_rect(prepare->fb.layer.transform,
                               &prepare->fb.layer.src,
                               &prepare->fb.layer.dst,
                               dpy->layer_clip.right,
                               dpy->layer_clip.bottom);
        } else {
            prepare->fb.layer.dst = prepare->fb.layer.src;
        }
    }

    if (prepare->modification.type == HWC_Modification_Xform) {
        xform_apply(&prepare->modification.xf, &prepare->fb.layer.dst);
    }
}

static void composite_prepare_device(struct nvhwc_display *dpy,
                                     struct nvhwc_prepare_state *prepare)
{
    /* src is in device coords */
    if (prepare->fb.layer.transform) {
        hwc_transform_rect(prepare->fb.layer.transform,
                           &prepare->fb.bounds,
                           &prepare->fb.layer.src,
                           dpy->layer_clip.right,
                           dpy->layer_clip.bottom);
    }
    if (prepare->modification.type == HWC_Modification_Xform) {
        xform_apply(&prepare->modification.xf, &prepare->fb.layer.src);
    }

    /* dst is in device coords */
    prepare->fb.layer.dst = prepare->fb.layer.src;
    prepare->fb.layer.transform = 0;
}

void
hwc_composite_prepare2(struct nvhwc_context *ctx,
                       struct nvhwc_display *dpy,
                       struct nvhwc_prepare_state *prepare)
{
    if (dpy->composite.id == HWC_Compositor_SurfaceFlinger) {
        composite_prepare_layer(dpy, prepare);
    } else {
        composite_prepare_device(dpy, prepare);
    }
}

static int
prepare_composer(nvcomposer_t *composer,
                 const nvcomposer_contents_t *contents)
{
    if (contents->numLayers > composer->maxLayers) {
        return -1;
    }

    if (contents->minScale < composer->minScale ||
        contents->maxScale > composer->maxScale) {
        return -1;
    }

    return composer->prepare(composer, contents);
}

static int
select_composer(struct nvhwc_context *ctx,
                struct nvhwc_display *dpy,
                struct nvhwc_prepare_state *prepare)
{
    int i;
    int result;
    nvcomposer_t *composer;
    const nvcomposer_contents_t *contents = &dpy->composite.contents;

    NV_ASSERT(dpy->composite.composers[dpy->composite.id] != NULL);

    /* Try to use the preferred compositor */
    composer = dpy->composite.composers[dpy->composite.id];
    result = prepare_composer(composer, contents);
    if (result == 0) {
        return 0;
    }

    if (ctx->props.dynamic.composite_fallback == NV_FALSE) {
        return -1;
    }

    /* Try to pick a fallback compositor */
    for (i=0; i<ctx->num_compositors; ++i) {
        HWC_Compositor id = ctx->compositors[i];
        if (dpy->composite.id == id) {
            continue;
        }

        composer = dpy->composite.composers[id];
        if (composer == NULL) {
            continue;
        }

        result = prepare_composer(composer, contents);
        if (result == 0) {
            hwc_composite_select(ctx, dpy, id);
            return 0;
        }
    }

    return -1;
}

static int
lock_scratch(struct nvhwc_context *ctx,
             struct nvhwc_display *dpy,
             struct nvhwc_prepare_state *prepare)
{
    int fb_width;
    int fb_height;
    int fb_window;
    nvcomposer_t *composer;
    nvcomposer_contents_t *contents = &dpy->composite.contents;
    NvRmSurfaceLayout layout = NvRmSurfaceGetDefaultLayout();
    NvGrScratchSet *oldScratch = dpy->composite.scratch;

    if (dpy->composite.scratch) {
        NV_ASSERT(dpy->fb_recycle);
        return 0;
    }

    /* FB size depends whether xform occurs in display or composite */
    composer = dpy->composite.composers[dpy->composite.id];
    if (composer->caps & NVCOMPOSER_CAP_TRANSFORM) {
        fb_width = r_width(&dpy->device_clip);
        fb_height = r_height(&dpy->device_clip);
    } else {
        fb_width = r_width(&dpy->layer_clip);
        fb_height = r_height(&dpy->layer_clip);
    }

    fb_window = prepare->layer_map[dpy->fb_index].window;
    if (!(dpy->caps.window_cap[fb_window].cap & NVFB_WINDOW_CAP_TILED)) {
        layout = NvRmSurfaceLayout_Pitch;
    }

    dpy->composite.scratch = scratch_assign(dpy, 0, /* no transform */
                                            fb_width, fb_height,
                                            NvColorFormat_A8B8G8R8,
                                            layout,
                                            NULL,
                                            contents->flags & NVCOMPOSER_FLAG_PROTECTED);
    if (!dpy->composite.scratch) {
        return -1;
    }

    /* Local compositors can cache internal states based on assumption that
     * scratch buffer do not change often. We have to notify local compositors
     * if scratchset is changed
     */
    if (oldScratch != dpy->composite.scratch) {
        dpy->composite.target.flags |= NVCOMPOSER_FLAG_TARGET_CHANGED;
    }

    /* Lock the scratch buffer so it will survive across geometry
     * changes.
     */
    dpy->scratch->lock(dpy->scratch, dpy->composite.scratch);

    return 0;
}


void
hwc_composite_prepare(struct nvhwc_context *ctx,
                      struct nvhwc_display *dpy,
                      struct nvhwc_prepare_state *prepare)
{
    int i;
    int result;

    NV_ASSERT(dpy->fb_index >= 0);
    NV_ASSERT(dpy->composite.id != HWC_Compositor_SurfaceFlinger);

    dpy->composite.target.flags  = 0;

    result = compute_contents(ctx, dpy, prepare);
    if (result != 0) {
        goto abort;
    }

    result = select_composer(ctx, dpy, prepare);
    if (result != 0) {
        goto abort;
    }

    result = lock_scratch(ctx, dpy, prepare);
    if (result != 0) {
        goto abort;
    }

    /* Tell SurfaceFlinger that we take care of these layers */
    for (i=0; i<dpy->composite.contents.numLayers; ++i) {
        int listIndex = dpy->composite.map[i];
        hwc_layer_t *cur = &prepare->display->hwLayers[listIndex];
        cur->compositionType = HWC_OVERLAY;
    }

    return;

abort:
    /* Abort composition */
    dpy->composite.contents.numLayers = 0;
    hwc_composite_select(ctx, dpy, HWC_Compositor_SurfaceFlinger);

    if (dpy->composite.scratch) {
        /* We should only be here if fb_recycle is set. */
        NV_ASSERT(dpy->fb_recycle);

        /* Switch to surfaceflinger composition and re-composite
         * the frame.
         */
        dpy->fb_recycle = 0;
        hwc_clear_layer_enable(dpy, prepare->display);

        /* Release the scratch buffer */
        dpy->scratch->unlock(dpy->scratch, dpy->composite.scratch);
        dpy->composite.scratch = NULL;
    }
}

void
hwc_composite_set(struct nvhwc_context *ctx,
                  struct nvhwc_display *dpy,
                  hwc_display_contents_t *display,
                  int *releaseFenceFd)
{
    int i;
    int result;
    nvcomposer_t *composer;
    nvcomposer_contents_t *contents = &dpy->composite.contents;
    nvcomposer_target_t *target = &dpy->composite.target;

    NV_ATRACE_BEGIN(__func__);

    NV_ASSERT(releaseFenceFd);
    NV_ASSERT(!dpy->fb_recycle);
    NV_ASSERT(dpy->composite.scratch);
    NV_ASSERT(dpy->composite.id != HWC_Compositor_SurfaceFlinger);
    NV_ASSERT(dpy->composite.composers[dpy->composite.id] != NULL);

    composer = dpy->composite.composers[dpy->composite.id];

    target->acquireFenceFd = -1;
    target->releaseFenceFd = -1;
    target->surface = (buffer_handle_t)
        dpy->scratch->next_buffer(dpy->scratch, dpy->composite.scratch,
                                  &target->acquireFenceFd);
    if (target->surface == NULL) {
        ALOGE("%s: Failed to get scratch buffer\n", __func__);
        goto end;
    }

    for (i=0; i<contents->numLayers; ++i) {
        hwc_layer_t *hlayer = &display->hwLayers[dpy->composite.map[i]];
        hwc_layer_t *clayer = &contents->layers[i];
        clayer->handle = hlayer->handle;
        clayer->releaseFenceFd = -1;
        if (composer->caps & NVCOMPOSER_CAP_DECOMPRESS) {
            clayer->acquireFenceFd = hlayer->acquireFenceFd;
        } else {
            NvNativeHandle *nvhandle = (NvNativeHandle *) clayer->handle;
            result = ctx->gralloc->decompress_buffer(ctx->gralloc,
                                                     nvhandle,
                                                     hlayer->acquireFenceFd,
                                                     &clayer->acquireFenceFd);
            if (result != 0) {
                ALOGW("decompression failed for layer %d",
                      dpy->composite.map[i]);
            }
        }
    }

    result = composer->set(composer,
                           contents,
                           target);
    if (result != 0) {
        ALOGE("%s: Intermediate composition failed: %d\n", __func__, result);
        goto end;
    } else {
        NV_ASSERT(target->acquireFenceFd < 0);
    }

    contents->flags &= ~NVCOMPOSER_FLAG_GEOMETRY_CHANGED;

end:
    // The nvcomposer API declares that the composer will never close a layer's
    // acquireFenceFd, so always close them here even if composition failed
    for (i=0; i<contents->numLayers; ++i) {
        hwc_layer_t *hlayer = &display->hwLayers[dpy->composite.map[i]];
        hwc_layer_t *clayer = &contents->layers[i];
        nvsync_close(clayer->acquireFenceFd);
        hlayer->acquireFenceFd = -1;
        hlayer->releaseFenceFd = nvsync_dup(__func__, target->releaseFenceFd);
    }

    nvsync_close(target->acquireFenceFd);

    *releaseFenceFd = target->releaseFenceFd;

    NV_ATRACE_END();
}
