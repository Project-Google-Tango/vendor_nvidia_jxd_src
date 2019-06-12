/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include <android/log.h>
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "device_clear_crash_counter", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "device_clear_crash_counter", __VA_ARGS__))


int get_bootloader_message(struct bootloader_message *message)
{
    int fd;
    size_t len;
    // Open MSC raw partition, read bootloader message struct
    fd = open("/dev/block/platform/sdhci-tegra.3/by-name/MSC", O_RDONLY);
    if (fd == -1)
    {
        LOGW("Can't open partition MSC\n");
        return -1;
    }
    len = read(fd, (void *)message, sizeof(struct bootloader_message));
    if(len != sizeof(struct bootloader_message))
    {
        close(fd);
        LOGW("Can't read partition MSC\n");
        return -1;
    }
    close(fd);
    return(0);
}

int set_bootloader_message(const struct bootloader_message *message)
{
    int fd;
    size_t len;

    // Open MSC raw partition, read bootloader message struct
    fd = open("/dev/block/platform/sdhci-tegra.3/by-name/MSC", O_WRONLY);
    if (fd == -1)
    {
        LOGW("Can't open partition MSC\n");
        return -1;
    }
    len = write(fd, (void *)message, sizeof(struct bootloader_message));
    if(len != sizeof(struct bootloader_message))
    {
        close(fd);
        LOGW("Can't write partition MSC\n");
        return -1;
    }
    close(fd);
    return(0);
}
