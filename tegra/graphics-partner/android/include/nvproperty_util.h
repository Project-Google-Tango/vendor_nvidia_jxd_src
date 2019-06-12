/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVPROPERTY_UTIL_H
#define INCLUDED_NVPROPERTY_UTIL_H

#include <cutils/properties.h>
#include <stdlib.h>
#include <stdio.h>

#define TEGRA_PROP_PREFIX "persist.tegra."
#define TEGRA_PROP(s) TEGRA_PROP_PREFIX #s

#define SYS_PROP_PREFIX "persist.sys."
#define SYS_PROP(s) SYS_PROP_PREFIX #s

static int get_property_bool(const char *key, int default_value)
{
    char value[PROPERTY_VALUE_MAX];

    if (property_get(key, value, NULL) > 0) {
        if (!strcmp(value, "1") || !strcasecmp(value, "on") ||
            !strcasecmp(value, "true")) {
            return NV_TRUE;
        }
        if (!strcmp(value, "0") || !strcasecmp(value, "off") ||
            !strcasecmp(value, "false")) {
            return NV_FALSE;
        }
    }

    return default_value;
}

static int get_property_int(const char *key, int default_value)
{
    char value[PROPERTY_VALUE_MAX];

    if (property_get(key, value, NULL) > 0) {
        return strtol(value, NULL, 0);
    }

    return default_value;
}

static int get_env_property_int(const char *key, int default_value)
{
    const char *env = getenv(key);
    int value;

    if (env) {
        value = atoi(env);
    } else {
        value = get_property_int(key, default_value);
    }

    return value;
}

// Returns the number of characters printed to value.
static int get_env_property(const char *key, char value[PROPERTY_VALUE_MAX])
{
    const char *env = getenv(key);
    if (env) {
        return snprintf(value, PROPERTY_VALUE_MAX, "%s", env);
    } else {
        return property_get(key, value, NULL);
    }
}

#endif
