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

void xform_get_center_scale(nvhwc_xform_t *xf,
                            int src_w, int src_h,
                            int dst_w, int dst_h,
                            float par);

void hwc_update_mirror_state(struct nvhwc_context *ctx, size_t numDisplays,
                             hwc_display_contents_t **displays);

NvBool hwc_prepare_external(struct nvhwc_context *ctx,
                            struct nvhwc_display *dpy,
                            struct nvhwc_prepare_state *prepare,
                            hwc_display_contents_t *display);
