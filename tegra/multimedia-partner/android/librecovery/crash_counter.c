/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "bootloader.h"
#if !HAVE_STRLCPY
#include <cutils/memory.h>
#endif
#include <cutils/properties.h>

#include <android/log.h>
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "device_clear_crash_counter", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "device_clear_crash_counter", __VA_ARGS__))


// zero out crash counter stored in CTR raw partition
int device_clear_crash_counter(void) {
    int fd;
    size_t written;
    unsigned long crashCounter = 0;

    // Open CTR raw partition, write a zero
    fd = open("/dev/block/platform/sdhci-tegra.3/by-name/CTR", O_WRONLY);
    if (fd == -1)
    {
        LOGW("Can't open crash counter partition CTR:\n");
        return(-1);
    }
    written = write(fd, &crashCounter, sizeof(crashCounter));
    if(written != sizeof(crashCounter))
    {
        close(fd);
        LOGW("Can't write crash counter partition CTR\n");
        return(-1);
    }
    close(fd);
    return(0);
}
