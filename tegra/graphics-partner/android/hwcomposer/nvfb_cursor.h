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
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 */

#ifndef _NVFB_CURSOR_H
#define _NVFB_CURSOR_H

struct nvfb_cursor;

int
nvfb_cursor_open(struct nvfb_cursor **cursor_ret,
                 NvGrModule *grmod,
                 int fd,
                 struct nvfb_display_caps *caps);

void
nvfb_cursor_close(struct nvfb_cursor *dev);

int
nvfb_cursor_update(struct nvfb_cursor *dev,
                   struct nvfb_window *win,
                   struct nvfb_buffer *buf);

#endif /* _NVFB_CURSOR_H */
