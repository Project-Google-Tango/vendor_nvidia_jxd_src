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

#ifndef __NVFL_H__
#define __NVFL_H__ 1

#include "nvfb_display.h"

#define NV_FLIPCOUNT_PATH_FMT  "/mnt/tmp/fc%d"

/* We store everything in a ringbuffer that lives on a single page of shared
 * memory: currently the page size is 4096 which gives us (4096-8)/8 = 511
 * latest flips. The clients are expected to read the memory page as
 * follows:
 *
 * 1. read block->idx
 *
 * 2. if the idx is -1 then an update is happening. Wait a bit and jump to
 *    1. Otherwise make a local copy of the block for further analysis, or
 *    just read the latest record pointed to by block->idx
 *
 * 3. read block->idx again and check that it hadn't changed: if it had,
 *    jump back to step 2.
 *
 * 4. if the record pointed to by a validated idx at this point contains
 *    (0xffffffff, 0xffffffff) then that entry is uninitialized and must be
 *    discarded. This is the initial condition until first flip occurs.
 *
 */
struct nvfl_flip {
    uint32_t timestamp_us;  /* Flip timestamp: overflows every 3 days. */
    uint32_t flipcount;
};

struct nvfl_fliplog {
    volatile int32_t    idx;
    int32_t             max;
    struct nvfl_flip    records[];
};

int nvfl_open (void);
void nvfl_close (void);
void nvfl_log_frame (enum DisplayType type);

#endif /* __NVFL_H__ */
