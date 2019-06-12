/*
 * Copyright (c) 2009-2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/*
 ** Based upon ALSAControl.cpp, provided under the following terms:
 **
 ** Copyright 2008-2009 Wind River Systems
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#define LOG_TAG "NvAudioALSAControl"
//#define LOG_NDEBUG 0

#include "nvaudioalsa.h"

using namespace android;


NvAudioALSAControl::NvAudioALSAControl(const char *device) :
    mHandle(0),
    mInit(false)
{
    if (snd_ctl_open(&mHandle, device, 0) < 0) {
        //If the alsa ctl failed to open for the default alsa device for a
        //platform/device, try to open it for another device(default1).
        //Added to support multiple codec on the same platform/device.
        if (snd_ctl_open(&mHandle, "default1", 0) < 0) {
            if (snd_ctl_open(&mHandle, "default_max98095", 0) < 0) {
                if (snd_ctl_open(&mHandle, "default_aic326x", 0) < 0) {
                    return;
                }
            }
        }
    }

    mInit = true;
}

NvAudioALSAControl::~NvAudioALSAControl()
{
    ALOGV("%s ", __FUNCTION__);
    if (mHandle) snd_ctl_close(mHandle);
}

status_t NvAudioALSAControl::initCheck()
{
    return mInit ? NO_ERROR : NO_INIT;
}

status_t NvAudioALSAControl::get(const char *name, int &value, int &val_min, int &val_max, int index)
{
    ALOGV("%s: name %s, index %d", __FUNCTION__, name, index);

    if (!mHandle) {
        ALOGE("Control not initialized");
        return NO_INIT;
    }

    snd_ctl_elem_id_t *id;
    snd_ctl_elem_info_t *info;
    snd_ctl_elem_value_t *control;

    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_info_alloca(&info);
    snd_ctl_elem_value_alloca(&control);

    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_id_set_name(id, name);
    snd_ctl_elem_info_set_id(info, id);

    int ret = snd_ctl_elem_info(mHandle, info);
    if (ret < 0) {
        ALOGE("Control '%s' cannot get element info: %d", name, ret);
        return BAD_VALUE;
    }

    int count = snd_ctl_elem_info_get_count(info);
    if (index >= count) {
        ALOGE("Control '%s' index is out of range (%d >= %d)", name, index, count);
        return BAD_VALUE;
    }

    snd_ctl_elem_info_get_id(info, id);
    snd_ctl_elem_value_set_id(control, id);

    ret = snd_ctl_elem_read(mHandle, control);
    if (ret < 0) {
        ALOGE("Control '%s' cannot read element value: %d", name, ret);
        return BAD_VALUE;
    }

    snd_ctl_elem_type_t type = snd_ctl_elem_info_get_type(info);
    switch (type) {
        case SND_CTL_ELEM_TYPE_BOOLEAN:
            value = snd_ctl_elem_value_get_boolean(control, index);
            break;
        case SND_CTL_ELEM_TYPE_INTEGER:
            value = snd_ctl_elem_value_get_integer(control, index);
            val_min = snd_ctl_elem_info_get_min(info);
            val_max = snd_ctl_elem_info_get_max(info);
            break;
        case SND_CTL_ELEM_TYPE_INTEGER64:
            value = snd_ctl_elem_value_get_integer64(control, index);
            val_min = (int) snd_ctl_elem_info_get_min64(info);
            val_max = (int) snd_ctl_elem_info_get_max64(info);
            break;
        case SND_CTL_ELEM_TYPE_ENUMERATED:
            value = snd_ctl_elem_value_get_enumerated(control, index);
            break;
        case SND_CTL_ELEM_TYPE_BYTES:
            value = snd_ctl_elem_value_get_byte(control, index);
            break;
        default:
            return BAD_VALUE;
    }

    return NO_ERROR;
}

status_t NvAudioALSAControl::get(const char *name, int &value, int index)
{
    ALOGV("%s: name %s, index %d", __FUNCTION__, name, index);

    if (!mHandle) {
        ALOGE("Control not initialized");
        return NO_INIT;
    }

    snd_ctl_elem_id_t *id;
    snd_ctl_elem_info_t *info;
    snd_ctl_elem_value_t *control;

    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_info_alloca(&info);
    snd_ctl_elem_value_alloca(&control);

    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_id_set_name(id, name);
    snd_ctl_elem_info_set_id(info, id);

    int ret = snd_ctl_elem_info(mHandle, info);
    if (ret < 0) {
        ALOGE("Control '%s' cannot get element info: %d", name, ret);
        return BAD_VALUE;
    }

    int count = snd_ctl_elem_info_get_count(info);
    if (index >= count) {
        ALOGE("Control '%s' index is out of range (%d >= %d)", name, index, count);
        return BAD_VALUE;
    }

    snd_ctl_elem_info_get_id(info, id);
    snd_ctl_elem_value_set_id(control, id);

    ret = snd_ctl_elem_read(mHandle, control);
    if (ret < 0) {
        ALOGE("Control '%s' cannot read element value: %d", name, ret);
        return BAD_VALUE;
    }

    snd_ctl_elem_type_t type = snd_ctl_elem_info_get_type(info);
    switch (type) {
        case SND_CTL_ELEM_TYPE_BOOLEAN:
            value = snd_ctl_elem_value_get_boolean(control, index);
            break;
        case SND_CTL_ELEM_TYPE_INTEGER:
            value = snd_ctl_elem_value_get_integer(control, index);
            break;
        case SND_CTL_ELEM_TYPE_INTEGER64:
            value = snd_ctl_elem_value_get_integer64(control, index);
            break;
        case SND_CTL_ELEM_TYPE_ENUMERATED:
            value = snd_ctl_elem_value_get_enumerated(control, index);
            break;
        case SND_CTL_ELEM_TYPE_BYTES:
            value = snd_ctl_elem_value_get_byte(control, index);
            break;
        default:
            return BAD_VALUE;
    }

    return NO_ERROR;
}

status_t NvAudioALSAControl::set(const char *name, unsigned int value, int index, int items)
{
    if (!mHandle) {
        ALOGE("Control not initialized");
        return NO_INIT;
    }

    ALOGV("set: name %s, value %u, index %d, items %d", name, value, index, items);

    snd_ctl_elem_id_t *id;
    snd_ctl_elem_info_t *info;

    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_info_alloca(&info);

    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_id_set_name(id, name);
    snd_ctl_elem_info_set_id(info, id);

    int ret = snd_ctl_elem_info(mHandle, info);
    if (ret < 0) {
        ALOGE("Control '%s' cannot get element info: %d", name, ret);
        return BAD_VALUE;
    }

    int count = snd_ctl_elem_info_get_count(info);
    if ((index + items) > count) {
        ALOGE("Control '%s' index + items is out of range (%d + %d > %d)", name, index, items, count);
        return BAD_VALUE;
    }

    if (index == -1)
        index = 0; // Range over all of them
    else
        count = index + items; // Just do the one specified

    snd_ctl_elem_type_t type = snd_ctl_elem_info_get_type(info);

    snd_ctl_elem_value_t *control;
    snd_ctl_elem_value_alloca(&control);

    snd_ctl_elem_info_get_id(info, id);
    snd_ctl_elem_value_set_id(control, id);

    for (int i = index; i < count; i++)
        switch (type) {
            case SND_CTL_ELEM_TYPE_BOOLEAN:
                snd_ctl_elem_value_set_boolean(control, i, value);
                break;
            case SND_CTL_ELEM_TYPE_INTEGER:
                snd_ctl_elem_value_set_integer(control, i, value);
                break;
            case SND_CTL_ELEM_TYPE_INTEGER64:
                snd_ctl_elem_value_set_integer64(control, i, value);
                break;
            case SND_CTL_ELEM_TYPE_ENUMERATED:
                snd_ctl_elem_value_set_enumerated(control, i, value);
                break;
            case SND_CTL_ELEM_TYPE_BYTES:
                snd_ctl_elem_value_set_byte(control, i, value);
                break;
            default:
                break;
        }

    ret = snd_ctl_elem_write(mHandle, control);
    return (ret < 0) ? BAD_VALUE : NO_ERROR;
}

status_t NvAudioALSAControl::set(const char *name, unsigned int value, int index)
{
    if (!mHandle) {
        ALOGE("Control not initialized");
        return NO_INIT;
    }

    ALOGV("set: name %s, value %u, index %d", name, value, index);

    snd_ctl_elem_id_t *id;
    snd_ctl_elem_info_t *info;

    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_info_alloca(&info);

    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_id_set_name(id, name);
    snd_ctl_elem_info_set_id(info, id);

    int ret = snd_ctl_elem_info(mHandle, info);
    if (ret < 0) {
        ALOGE("Control '%s' cannot get element info: %d", name, ret);
        return BAD_VALUE;
    }

    int count = snd_ctl_elem_info_get_count(info);
    if (index >= count) {
        ALOGE("Control '%s' index is out of range (%d >= %d)", name, index, count);
        return BAD_VALUE;
    }

    if (index == -1)
        index = 0; // Range over all of them
    else
        count = index + 1; // Just do the one specified

    snd_ctl_elem_type_t type = snd_ctl_elem_info_get_type(info);

    snd_ctl_elem_value_t *control;
    snd_ctl_elem_value_alloca(&control);

    snd_ctl_elem_info_get_id(info, id);
    snd_ctl_elem_value_set_id(control, id);

    for (int i = index; i < count; i++)
        switch (type) {
            case SND_CTL_ELEM_TYPE_BOOLEAN:
                snd_ctl_elem_value_set_boolean(control, i, value);
                break;
            case SND_CTL_ELEM_TYPE_INTEGER:
                snd_ctl_elem_value_set_integer(control, i, value);
                break;
            case SND_CTL_ELEM_TYPE_INTEGER64:
                snd_ctl_elem_value_set_integer64(control, i, value);
                break;
            case SND_CTL_ELEM_TYPE_ENUMERATED:
                snd_ctl_elem_value_set_enumerated(control, i, value);
                break;
            case SND_CTL_ELEM_TYPE_BYTES:
                snd_ctl_elem_value_set_byte(control, i, value);
                break;
            default:
                break;
        }

    ret = snd_ctl_elem_write(mHandle, control);
    return (ret < 0) ? BAD_VALUE : NO_ERROR;
}

status_t NvAudioALSAControl::set(const char *name, const char *value)
{
    if (!mHandle) {
        ALOGE("Control not initialized");
        return NO_INIT;
    }

    ALOGV("set: name %s, value %s", name, value);

    snd_ctl_elem_id_t *id;
    snd_ctl_elem_info_t *info;

    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_info_alloca(&info);

    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_id_set_name(id, name);
    snd_ctl_elem_info_set_id(info, id);

    int ret = snd_ctl_elem_info(mHandle, info);
    if (ret < 0) {
        ALOGE("Control '%s' cannot get element info: %d", name, ret);
        return BAD_VALUE;
    }

    int items = snd_ctl_elem_info_get_items(info);
    for (int i = 0; i < items; i++) {
        snd_ctl_elem_info_set_item(info, i);
        ret = snd_ctl_elem_info(mHandle, info);
        if (ret < 0) continue;
        if (strcmp(value, snd_ctl_elem_info_get_item_name(info)) == 0)
            return set(name, i, -1);
    }

    ALOGE("Control '%s' has no enumerated value of '%s'", name, value);

    return BAD_VALUE;
}

