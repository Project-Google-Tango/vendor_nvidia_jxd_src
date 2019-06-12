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

#ifndef _NVHWC_COMPOSITE_H
#define _NVHWC_COMPOSITE_H

#include "nvcomposer.h"

struct nvhwc_display;
struct nvhwc_context;
struct nvhwc_prepare_layer;
struct nvhwc_prepare_state;

typedef enum {
    HWC_Compositor_SurfaceFlinger,
    HWC_Compositor_Gralloc,
    HWC_Compositor_GLComposer,
    HWC_Compositor_GLDrawTexture,
    HWC_Compositor_Count
} HWC_Compositor;

typedef enum {
    /* Individual policy bits that control HWC behavior */
    HWC_CompositePolicy_ForceComposite   = 1 << 0,
    HWC_CompositePolicy_FbCache          = 1 << 1,
    HWC_CompositePolicy_CompositeOnIdle  = 1 << 2,

    /* Tested policy combinations that can be set with setprop */
    HWC_CompositePolicy_AssignWindows   = 0,
    HWC_CompositePolicy_CompositeAlways  = HWC_CompositePolicy_ForceComposite,
    HWC_CompositePolicy_Auto             = HWC_CompositePolicy_FbCache
                                         | HWC_CompositePolicy_CompositeOnIdle,
} HWC_CompositePolicy;

typedef struct nvhwc_composite {
    HWC_Compositor         id;
    nvcomposer_t          *composers[HWC_Compositor_Count];
    nvcomposer_contents_t  contents;
    nvcomposer_target_t    target;
    int                    map[NVCOMPOSER_MAX_LAYERS];
    NvGrScratchSet        *scratch;
    glc_context_t         *glc;
} nvhwc_composite_t;

void
hwc_composite_open(struct nvhwc_context *ctx,
                   struct nvhwc_display *dpy);

void
hwc_composite_close(struct nvhwc_context *ctx,
                    struct nvhwc_display *dpy);

void
hwc_composite_select(struct nvhwc_context *ctx,
                     struct nvhwc_display *dpy,
                     HWC_Compositor id);

void
hwc_composite_override(struct nvhwc_context *ctx,
                       struct nvhwc_display *dpy,
                       HWC_Compositor id);

int
hwc_composite_caps(struct nvhwc_context *ctx,
                   struct nvhwc_display *dpy);

void
hwc_composite_prepare(struct nvhwc_context *ctx,
                      struct nvhwc_display *dpy,
                      struct nvhwc_prepare_state *prepare);

void
hwc_composite_prepare2(struct nvhwc_context *ctx,
                       struct nvhwc_display *dpy,
                       struct nvhwc_prepare_state *prepare);

void
hwc_composite_set(struct nvhwc_context *ctx,
                  struct nvhwc_display *dpy,
                  hwc_display_contents_t *display,
                  int *releaseFenceFd);

#endif /* ifndef _NVHWC_COMPOSITE_H */
