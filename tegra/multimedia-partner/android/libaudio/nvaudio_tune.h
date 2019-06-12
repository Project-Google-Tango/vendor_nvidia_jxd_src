/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

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

#ifndef LIBAUDIO_NVAUDIO_TUNE_H
#define LIBAUDIO_NVAUDIO_TUNE_H

//#define LOG_NDEBUG 0

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/time.h>

#include <cutils/log.h>
#include <cutils/str_parms.h>

#include <expat.h>

#include <system/audio.h>
#include "nvaudio_list.h"
/*****************************************************************************
 * MACROS
 *****************************************************************************/
#define XML_TUNE_CONFIG_FILE_PATH            "/etc/nvaudio_tune.xml"
#define XML_TUNE_CONFIG_FILE_PATH_LBH    "/lbh/etc/nvaudio_tune.xml"

/*****************************************************************************
 * ENUMS
 *****************************************************************************/
/* enums for tuning mode */
enum tune_mode {
    TUNE_MODE_NORMAL = 0,
    TUNE_MODE_VOIP,
    TUNE_MODE_VR,
    TUNE_MODE_MAX
};

/* enums for tuning devices */
enum tune_device {
    TUNE_OUT_SPEAKER = 0x00000001,
    TUNE_OUT_HEADSET = 0x00000002,
    TUNE_OUT_HEADPHONE = 0x00000004,
    TUNE_OUT_SPK_HP = 0x00000008, /* both speaker and headphone make sound simultaneously */
    TUNE_IN_MAIN = 0x00010000, /* built-in mic */
    TUNE_IN_HEADSET = 0x00020000, /* headset mic */
};

/*****************************************************************************
 * Structures
 *****************************************************************************/
struct gain_ctl {
    char                        *ctl_name;
    int                         int_val;
    char                        *str_val;
};

struct device_gain {
    list_handle                 ctls;
    enum tune_device            device;
};

struct tune_mode_gain {
    list_handle                 gains[TUNE_MODE_MAX];
    int                         in_source;
};

struct config_tune_parse_state {
    struct tune_mode_gain        *tune;
    struct device_gain           *cur_dev;
    enum tune_mode                cur_mode;
};
/*****************************************************************************
 * Forward Declaration of static functions
 *****************************************************************************/
void set_tune_gain(struct tune_mode_gain *tune,
                   struct mixer *mixer, int mode, int devices);
void close_tune(struct tune_mode_gain *tune);
int config_tune_parse(struct tune_mode_gain *mode);
void config_tune_dump(struct tune_mode_gain *mode);

#endif // LIBAUDIO_NVAUDIO_TUNE_H
