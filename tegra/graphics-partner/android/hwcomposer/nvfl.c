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

#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "nvfl.h"
#include "nvhwc.h"

/*********************************************************************
 *   Definitions
 */

#define TIMESPEC_TO_US(t)   ((int64_t)t.tv_sec*1000000 + (t.tv_nsec / 1000))

struct nvfl_display {
    uint32_t flipcount;         /* Will overflow in two years at 60 fps. */
    int fd;
    struct nvfl_fliplog *flog;  /* This is mmap()'d directly. */
};

static struct nvfl_display displays[NVFB_MAX_DISPLAYS];
static long pagesize;

/*********************************************************************
 *   Fliplist
 */

int nvfl_open (void)
{
    char buffer[PATH_MAX];
    int i;
    int slot;

    /* Initialize so that nvfl_close() can be called. */
    pagesize = sysconf(_SC_PAGE_SIZE);

    for (i = 0; i < NVFB_MAX_DISPLAYS; i++) {
        displays[i].flipcount = 0UL;
        displays[i].fd = -1;
        displays[i].flog = MAP_FAILED;
    }

    /*
     * Initialize mappings for all supported displays.
     */
    for (i = 0; i < NVFB_MAX_DISPLAYS; i++) {
        struct nvfl_fliplog *flog;

        /* Open the flipcount file. */
        snprintf(buffer, sizeof(buffer), NV_FLIPCOUNT_PATH_FMT, i);
        displays[i].fd = open(buffer,
                              O_RDWR | O_CREAT,
                              S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (displays[i].fd == -1) {
            ALOGE("Could not open fliplist file \"%s\": %s", buffer, strerror(errno));
            return 0;
        }

        /* Allocate space. */
        if (ftruncate(displays[i].fd, pagesize) == -1) {
            ALOGE("Could not allocate space in fliplist file \"%s\": %s", buffer, strerror(errno));
            return 0;
        }

        /* Map the region. */
        flog = mmap(NULL, pagesize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, displays[i].fd, 0);
        if (flog == MAP_FAILED) {
            ALOGE("Could not map page in \"%s\": %s", buffer, strerror(errno));
            return 0;
        }

        /* We assume this will always hold: mmap() should return memory aligned
         * to page boundaries and thus any uint32_t's, counting from the
         * beginning of the returned block, should be correctly aligned. */
        NV_ASSERT(((size_t)flog & (sizeof(uint32_t) - 1)) == 0);

        /* Initialize ring buffer. Make idx point to slot 0 but initialize
         * all records to (int)-1 to mark them uninitialized. */
        flog->idx = 0;
        flog->max = (pagesize - sizeof(struct nvfl_fliplog)) / sizeof(struct nvfl_flip);

        for (slot = 0; slot < flog->max; slot++) {
            flog->records[slot].timestamp_us    = 0xffffffffUL;
            flog->records[slot].flipcount       = 0xffffffffUL;
        }
        displays[i].flog = flog;

        /* Make sure the initial contents is now on the disk cache. We won't do
         * voluntary syncs any more because clients are expected to mmap() the
         * shared page directly but we want to make sure the on-disk version
         * won't be bogus to begin with. */
        msync(displays[i].flog, pagesize, MS_SYNC);
    }

    return 1;
}

void nvfl_close (void)
{
    int i;

    for (i = 0; i < NVFB_MAX_DISPLAYS; i++) {
        if (displays[i].flog != MAP_FAILED)
            munmap(displays[i].flog, pagesize);
        if (displays[i].fd != -1)
            close(displays[i].fd);
    }
}

void nvfl_log_frame (enum DisplayType type)
{
    struct nvfl_display *disp;
    struct nvfl_flip *flip;
    struct timespec timestamp;
    int32_t idx;

    NV_ASSERT(type >= 0 && type < NVFB_MAX_DISPLAYS);
    disp = &displays[type];

    /* Maintain flipcounts per display type in nvfl_display. Explicitly
     * reserve 0xffffffff as "unused". */
    if (++disp->flipcount == 0xffffffffUL)
        disp->flipcount = 0;
    clock_gettime(CLOCK_MONOTONIC_RAW, &timestamp);

    /* In case nvfl_init() failed (which is not considered fatal in
     * nvhwc.c), the memory map just might not be there. */
    if (disp->flog == MAP_FAILED)
        return;

    /* It's easy because we're the only one who writes into this structure:
     * update the oldest flip record and update the index. Clients who are
     * reading this will issue a barriered load for the index, inspect the
     * ringbuffer, and finally barrier and reload the index and verify it
     * hasn't changed. */
    idx = disp->flog->idx + 1;
    if (idx == disp->flog->max)
        idx = 0;
    flip = &disp->flog->records[idx];

    /* Invalidate current index followed by a memory barrier. */
    android_atomic_acquire_store(-1, &disp->flog->idx);

    /* Update record. */
    flip->timestamp_us  = TIMESPEC_TO_US(timestamp);
    flip->flipcount     = disp->flipcount;

    /* Update current index followed by a memory barrier. */
    android_atomic_release_store(idx, &disp->flog->idx);
}
