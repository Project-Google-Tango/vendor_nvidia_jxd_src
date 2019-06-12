/*
 * Copyright (C) 2014 The Android Open Source Project
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
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 */

#ifndef __NVFB_DISPLAY_H__
#define __NVFB_DISPLAY_H__ 1

#ifndef NVCAP_VIDEO_ENABLED
#define NVCAP_VIDEO_ENABLED 0
#endif

enum DisplayType {
    NVFB_DISPLAY_NONE = -1,
    NVFB_DISPLAY_PANEL = 0,
    NVFB_DISPLAY_HDMI,
#if NVCAP_VIDEO_ENABLED
    NVFB_DISPLAY_WFD,
#endif
    NVFB_MAX_DISPLAYS,
    NVFB_MAX_DC_TYPES = NVFB_DISPLAY_HDMI+1,
};

#endif /* __NVFB_DISPLAY_H__ */
