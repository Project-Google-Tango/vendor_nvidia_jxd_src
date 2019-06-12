/*
 * Copyright (c) 2011-2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvhwc.h"

void
hwc_dump_display_contents(hwc_display_contents_t *list)
{
    if (list == NULL)
    {
        ALOGD("Nvidia HWC layer list: NULL");
    }
    else
    {
        size_t i, j;
        int offset = 0;
        char buffer[1000];
        int size = NV_ARRAY_SIZE(buffer);

        #define APPEND(...) \
            do { \
                if (size > offset) \
                    offset += snprintf(buffer + offset, size - offset, __VA_ARGS__); \
            } while (0)

        #define APPENDRECT(rect) \
            APPEND("[ left=%d, top=%d, right=%d, bottom=%d ]",   \
                   rect.left, rect.top, rect.right, rect.bottom)

        APPEND("Nvidia HWC layer list: %d layers, flags:", list->numHwLayers);
        if (list->flags & HWC_GEOMETRY_CHANGED) APPEND(" GEOMETRY_CHANGED");
        #ifdef HWC_IDLE
        if (list->flags & HWC_IDLE) APPEND(" IDLE");
        #endif
        ALOGD("%s", buffer);

        for (i=0; i<list->numHwLayers; ++i)
        {
            hwc_layer_t *layer = &list->hwLayers[i];

            offset = 0;

            APPEND("LAYER %d:", i);

            APPEND("\n  compositionType: ");
            switch (layer->compositionType)
            {
                case HWC_FRAMEBUFFER: APPEND("FRAMEBUFFER"); break;
                case HWC_OVERLAY:     APPEND("OVERLAY");     break;
                default:              APPEND("%d", layer->compositionType); break;
            }

            APPEND("\n  hints:");
            if (layer->hints & HWC_HINT_TRIPLE_BUFFER) APPEND(" TRIPLE_BUFFER");
            if (layer->hints & HWC_HINT_CLEAR_FB)      APPEND(" CLEAR_FB");

            APPEND("\n  flags:");
            if (layer->flags & HWC_SKIP_LAYER) APPEND(" SKIP_LAYER");

            APPEND("\n  handle: %p", layer->handle);
            if (layer->handle != NULL)
            {
                NvNativeHandle *h = (NvNativeHandle *) layer->handle;
                switch (h->Type)
                {
                    case NV_NATIVE_BUFFER_SINGLE: APPEND(" SINGLE"); break;
                    case NV_NATIVE_BUFFER_STEREO: APPEND(" STEREO"); break;
                    case NV_NATIVE_BUFFER_YUV:    APPEND(" YUV");    break;
                    default: break;
                }
                switch (h->Format)
                {
                    case HAL_PIXEL_FORMAT_RGBA_8888: APPEND(" RGBA_8888"); break;
                    case HAL_PIXEL_FORMAT_RGBX_8888: APPEND(" RGBX_8888"); break;
                    case HAL_PIXEL_FORMAT_RGB_888:   APPEND(" RGB_888");   break;
                    case HAL_PIXEL_FORMAT_RGB_565:   APPEND(" RGB_565");   break;
                    case HAL_PIXEL_FORMAT_BGRA_8888: APPEND(" BGRA_8888"); break;
#ifndef PLATFORM_IS_KITKAT
                    case HAL_PIXEL_FORMAT_RGBA_5551: APPEND(" RGBA_5551"); break;
                    case HAL_PIXEL_FORMAT_RGBA_4444: APPEND(" RGBA_4444"); break;
#endif
                    case NVGR_PIXEL_FORMAT_YUV420:   APPEND(" YUV420");    break;
                    case NVGR_PIXEL_FORMAT_YUV422:   APPEND(" YUV422");    break;
                    case NVGR_PIXEL_FORMAT_YUV422R:  APPEND(" YUV422R");   break;
                    default: break;
                }
                APPEND(" %dx%d", h->Surf[0].Width, h->Surf[0].Height);
                switch (h->Surf[0].Layout)
                {
                    case NvRmSurfaceLayout_Pitch:       APPEND(" Pitch");       break;
                    case NvRmSurfaceLayout_Blocklinear: APPEND(" Blocklinear"); break;
                    case NvRmSurfaceLayout_Tiled:       APPEND(" Tiled");       break;
                    default: break;
                }
                switch (h->Surf[0].Kind)
                {
                    case NvRmMemKind_Pitch:         APPEND(" Pitch");         break;
                    case NvRmMemKind_Generic_16Bx2: APPEND(" Generic_16Bx2"); break;
                    case NvRmMemKind_C32_2CRA:      APPEND(" C32_2CRA");      break;
                    default: break;
                }
                switch (NvRmMemGetHeapType(h->Surf[0].hMem))
                {
                    case NvRmHeap_IOMMU:            APPEND(" IOMMU");    break;
                    case NvRmHeap_VPR:              APPEND(" VPR");      break;
                    case NvRmHeap_ExternalCarveOut: APPEND(" Carveout"); break;
                    default: break;
                }
            }

            APPEND("\n  transform: ");
            switch (layer->transform)
            {
                case HWC_TRANSFORM_FLIP_H:  APPEND("FLIP_H");  break;
                case HWC_TRANSFORM_FLIP_V:  APPEND("FLIP_V");  break;
                case HWC_TRANSFORM_ROT_90:  APPEND("ROT_90");  break;
                case HWC_TRANSFORM_ROT_180: APPEND("ROT_180"); break;
                case HWC_TRANSFORM_ROT_270: APPEND("ROT_270"); break;
                default:                    APPEND("%d", layer->transform);  break;
            }

            APPEND("\n  blending: ");
            switch (layer->blending)
            {
                case HWC_BLENDING_NONE:     APPEND("NONE");     break;
                case HWC_BLENDING_PREMULT:  APPEND("PREMULT");  break;
                case HWC_BLENDING_COVERAGE: APPEND("COVERAGE"); break;
                default:                    APPEND("%d", layer->blending); break;
            }

            APPEND("\n  planeAlpha: ");
            APPEND("%08x", layer->planeAlpha);

            APPEND("\n  sourceCrop: ");
            APPENDRECT(layer->sourceCrop);

            APPEND("\n  displayFrame: ");
            APPENDRECT(layer->displayFrame);

            APPEND("\n  visibleRegionScreen: %d rects", layer->visibleRegionScreen.numRects);
            for (j=0; j<layer->visibleRegionScreen.numRects; ++j)
            {
                APPEND("\n    ");
                APPENDRECT(layer->visibleRegionScreen.rects[j]);
            }

            // It would be nicer to print this as one chunk, but the android
            // LOG functions will truncate the text
            ALOGD("%s", buffer);
        }

        #undef APPEND
        #undef APPENDRECT
    }
}


#define DUMP(...) \
    do { \
        if (buff_len > len) \
            len += snprintf(buff + len, buff_len - len, __VA_ARGS__); \
    } while (0)

static int dump_display(struct nvhwc_context *ctx,
                        struct nvhwc_display *dpy,
                        char *buff, int buff_len)
{
    NvGrOverlayConfig *conf = &dpy->conf;
    int len = 0;
    size_t ii, num_windows;

    if (dpy->current_mode) {
        DUMP("\tResolution: %d x %d%s\n",
             dpy->current_mode->xres, dpy->current_mode->yres,
             dpy->current_mode == dpy->default_mode ? "" : "(override)");
    } else {
        DUMP("\tResolution: %d x %d\n", dpy->config.res_x, dpy->config.res_y);
    }
    DUMP("\tBlank: %s\n", dpy->blank ?  "true" : "false");
    DUMP("\tProtected: %s\n", conf->protect ? "true" : "false");
    DUMP("\tCompositor: ");
    switch (dpy->composite.id) {
    case HWC_Compositor_Gralloc:
        DUMP("gralloc");
        break;
    case HWC_Compositor_GLComposer:
        DUMP("glcomposer");
        break;
    case HWC_Compositor_GLDrawTexture:
        DUMP("gldrawtexture");
        break;
    case HWC_Compositor_SurfaceFlinger:
        DUMP("surfaceflinger");
        break;
    default:
        DUMP("unknown");
        break;
    }
    if (ctx->props.dynamic.compositor != dpy->composite.id) {
        DUMP(" (override)");
    }
    DUMP("\n");
    DUMP("\tCurrently compositing %d layers into a %s\n",
         dpy->composite.contents.numLayers,
         dpy->composite.scratch ? "scratch buffer" : "framebuffer");

    num_windows = dpy->blank ? 0 : dpy->caps.num_windows;
    for (ii = 0; ii < num_windows; ii++) {
        if ((int)ii == dpy->fb_index) {
            DUMP("\tWindow %d (phys %d caps %d blend 0x%x xform 0x%x): "
                 "%s containing %d layers\n"
                 "\t\tsrc (%d,%d,%d,%d), dst (%d,%d,%d,%d)\n",
                 ii, conf->overlay[ii].window_index,
                 dpy->caps.window_cap[conf->overlay[ii].window_index].cap,
                 conf->overlay[ii].blend, conf->overlay[ii].transform,
                 (dpy->composite.contents.numLayers && dpy->composite.scratch) ?
                     "scratch": "fb",
                 dpy->composite.contents.numLayers ?
                     dpy->composite.contents.numLayers : dpy->fb_layers,
                 conf->overlay[ii].src.left, conf->overlay[ii].src.top,
                 conf->overlay[ii].src.right, conf->overlay[ii].src.bottom,
                 conf->overlay[ii].dst.left, conf->overlay[ii].dst.top,
                 conf->overlay[ii].dst.right, conf->overlay[ii].dst.bottom);

            if (dpy->map[ii].scratch) {
                DUMP("\t\tscratch %p[%d] (%d, %d)",
                     dpy->map[ii].scratch,
                     dpy->map[ii].scratch->active_index,
                     dpy->map[ii].scratch->width,
                     dpy->map[ii].scratch->height);
                if (dpy->map[ii].scratch->use_src_crop) {
                    DUMP(", crop (%d,%d,%d,%d)",
                         dpy->map[ii].scratch->src_crop.left,
                         dpy->map[ii].scratch->src_crop.top,
                         dpy->map[ii].scratch->src_crop.right,
                         dpy->map[ii].scratch->src_crop.bottom);
                }
#if 0
                if (conf->overlay[ii].scratch->transform) {
                    DUMP(", rot: %d", rotation_from_transform(conf->overlay[ii].scratch->transform));
                }
#endif
                DUMP("\n");
            }
        } else if (dpy->map[ii].index != -1) {
            DUMP("\tWindow %d (phys %d caps %d blend 0x%x xform 0x%x): "
                 "layer %d\n",
                 ii, conf->overlay[ii].window_index,
                 dpy->caps.window_cap[conf->overlay[ii].window_index].cap,
                 conf->overlay[ii].blend, conf->overlay[ii].transform,
                 dpy->map[ii].index);
            DUMP("\t\tsrc (%d,%d,%d,%d), dst (%d,%d,%d,%d)\n",
                 conf->overlay[ii].src.left, conf->overlay[ii].src.top,
                 conf->overlay[ii].src.right, conf->overlay[ii].src.bottom,
                 conf->overlay[ii].dst.left, conf->overlay[ii].dst.top,
                 conf->overlay[ii].dst.right, conf->overlay[ii].dst.bottom);
            if (dpy->map[ii].scratch) {
                DUMP("\t\tscratch %p[%d] (%d, %d)",
                     dpy->map[ii].scratch,
                     dpy->map[ii].scratch->active_index,
                     dpy->map[ii].scratch->width,
                     dpy->map[ii].scratch->height);
                if (dpy->map[ii].scratch->use_src_crop) {
                    DUMP(", crop (%d,%d,%d,%d)",
                         dpy->map[ii].scratch->src_crop.left,
                         dpy->map[ii].scratch->src_crop.top,
                         dpy->map[ii].scratch->src_crop.right,
                         dpy->map[ii].scratch->src_crop.bottom);
                }
#if 0
                if (conf->overlay[ii].scratch->transform) {
                    DUMP(", rot: %d", rotation_from_transform(conf->overlay[ii].scratch->transform));
                }
#endif
                DUMP("\n");
            }
        } else {
            DUMP("\tWindow %d (phys %d caps %x): unused\n",
                 ii, conf->overlay[ii].window_index,
                 dpy->caps.window_cap[conf->overlay[ii].window_index].cap);
        }
    }
    if (dpy->caps.max_cursor) {
        DUMP("\tCursor: ");
        if (dpy->cursor.map.index != -1) {
            DUMP("(blend 0x%x xform 0x%x): layer %d\n",
                 conf->cursor.blend, conf->cursor.transform,
                 dpy->cursor.map.index);
            DUMP("\t\tsrc (%d,%d,%d,%d), dst (%d,%d,%d,%d)\n",
                 conf->cursor.src.left, conf->cursor.src.top,
                 conf->cursor.src.right, conf->cursor.src.bottom,
                 conf->cursor.dst.left, conf->cursor.dst.top,
                 conf->cursor.dst.right, conf->cursor.dst.bottom);
        } else {
            DUMP("unused\n");
        }
    }

    DUMP("\tLocked buffers:");
    for (ii = 0; ii < NVFB_MAX_WINDOWS; ii++) {
        if (dpy->buffers[ii]) {
            DUMP(" 0x%x", dpy->buffers[ii]->MemId);
        }
    }
    DUMP("\n");

    return len;
}

static inline int rotation_from_transform(int transform)
{
    switch (transform) {
    case 0:
        return 0;
    case HWC_TRANSFORM_ROT_90:
        return 270;
    case HWC_TRANSFORM_ROT_180:
        return 180;
    case HWC_TRANSFORM_ROT_270:
        return 90;
    }

    NV_ASSERT(!"Unexpected rotation");
    return 0;
}

void hwc_dump(hwc_composer_device_t *dev, char *buff, int buff_len)
{

    struct nvhwc_context *ctx = (struct nvhwc_context *)dev;
    struct nvhwc_display *primary = (struct nvhwc_display *)
        &ctx->displays[HWC_DISPLAY_PRIMARY];
    struct nvhwc_display *external = (struct nvhwc_display *)
        &ctx->displays[HWC_DISPLAY_EXTERNAL];
    int len = 0;

    pthread_mutex_lock(&ctx->hotplug.mutex);

    DUMP("  Nvidia HWC:\n");
    DUMP("    Display 0 (PRIMARY):\n");
    len += dump_display(ctx, primary, buff + len, buff_len - len);

    DUMP("    Display 1 (EXTERNAL):\n");
    if (external->hotplug.cached.connected) {
        if (external->conf.mode) {
            DUMP("\tmode: %d x %d\n", external->conf.mode->xres,
                 external->conf.mode->yres);
        }
        len += dump_display(ctx, external, buff + len, buff_len - len);
    } else {
        DUMP("\tdisabled\n");
    }

    pthread_mutex_unlock(&ctx->hotplug.mutex);

    DUMP("    Composite policy: ");
    switch (ctx->props.dynamic.composite_policy) {
    case HWC_CompositePolicy_Auto:
        DUMP("auto\n");
        break;
    case HWC_CompositePolicy_AssignWindows:
        DUMP("assign-windows\n");
        break;
    case HWC_CompositePolicy_CompositeAlways:
        DUMP("composite-always\n");
        break;
    default:
        DUMP("unknown\n");
        break;
    }
    DUMP("    Idle machine: detection %s, composite %s",
         ctx->idle.enabled ? "enabled" : "disabled",
         ctx->idle.composite ? "enabled" : "disabled");

    DUMP("\n");
}

#undef DUMP

static void
compute_dump_name(NvNativeHandle *handle,
                  const char *prefix,
                  int frame,
                  int dc,
                  int layer,
                  char *buffer,
                  int bufferSize)
{
    NvU32 i;
    int offset;

    NV_ASSERT(handle && prefix && buffer);

    offset = NvOsSnprintf(buffer,
                          bufferSize,
                          "%sdump%d_dc%d_win%d_",
                          prefix,
                          frame,
                          dc,
                          layer);

    offset += NvRmSurfaceComputeName(buffer + offset,
                                     NV_MAX(0, bufferSize - offset),
                                     handle->Surf,
                                     handle->SurfCount);

    NvOsSnprintf(buffer + offset,
                 NV_MAX(0, bufferSize - offset),
                 "%s",
                 ".raw");
}

void
hwc_dump_windows(NvGrModule *gralloc,
                 int dc,
                 int num_buffers,
                 struct nvfb_buffer *buffers)
{
    static int frame = 0;
    int ii;
    char filename[NVOS_PATH_MAX + 1];

    for (ii=0; ii<num_buffers; ++ii)
    {
        NvNativeHandle *h = buffers[ii].buffer;

        if (h == NULL)
            continue;

        compute_dump_name(h, "/data/local/hwcomposer/",
                          frame, dc, ii, filename, NV_ARRAY_SIZE(filename));

        gralloc->dump_buffer(&gralloc->Base, (buffer_handle_t) h, filename);
    }

    ++frame;
}
