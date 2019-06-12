/*
 * Copyright (c) 2011 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <stdio.h>

#include <cutils/properties.h>

#include "Config.h"

#undef LOG_TAG
#define LOG_TAG "nvcpu:Config"

namespace android {
namespace nvcpu {

//This is infrequent by default
const Config::Property Config::refreshMsProp("nvcpud.config_refresh_ms", "45555");
const Config::Property Config::enabledProp("nvcpud.enabled", "false");
const Config::Property Config::bootCompleteProp("dev.bootcomplete", "false");

void Config::Property::get(char* out) const
{
    property_get(key.string(), out, def.string());
}

bool Config::checkBootComplete()
{
    char buff[PROPERTY_VALUE_MAX];

    // reading property while the system is not completely
    // booted can return 0 because the property service think
    // the key does not exist at the time we read.
    // However, it is safe to read dev.bootcomplete here since
    // we are waiting it to be 1 or true
    bootCompleteProp.get(buff);
    if ((0 == strcmp(buff, "false")) ||
        (0 == strcmp(buff, "0"))) {
        bootCompleted = false;
    } else if ((0 == strcmp(buff, "true")) ||
               (0 == strcmp(buff, "1"))) {
        bootCompleted = true;
    }

    return bootCompleted;
}

void Config::refresh()
{
    char buff[PROPERTY_VALUE_MAX];

    refreshMsProp.get(buff);
    //We accept negative values for "never again"
    int refreshMs;
    if (sscanf(buff, "%d", &refreshMs) ==  1) {
        configRefreshNs = milliseconds_to_nanoseconds(refreshMs);
    }

    enabledProp.get(buff);
    if (0 == strcmp(buff, "false")) {
        enabled = false;
    } else if (0 == strcmp(buff, "true")) {
        enabled = true;
    }
}

}; //nvcpu
}; //android
