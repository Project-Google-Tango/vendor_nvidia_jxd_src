/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvgralloc.h"
#include "nvproperty_util.h"


// Use compressed buffers: on/off
#define NV_PROPERTY_COMPRESSION                 TEGRA_PROP(compression)

// How to perform decompression: on/off/lazy
#define NV_PROPERTY_DECOMPRESSION               TEGRA_PROP(decompression)

// Allow caching gpu mappings in gralloc buffer: on/off
#define NV_PROPERTY_GPU_MAPPING_CACHE           TEGRA_PROP(gpu_mapping_cache)

// If set, read properties always with get_env_property() instead of caching
#define NV_PROPERTY_SCAN_PROPS                  TEGRA_PROP(scan_props)



static NvBool
NvGrParseBooleanStringValue(const char *value, NvBool *dst)
{
    if (!strcmp(value, "off") || !strcmp(value, "0")) {
        *dst = NV_FALSE;
    } else if (!strcmp(value, "on") || !strcmp(value, "1")) {
        *dst = NV_TRUE;
    } else {
        return NV_FALSE; // parse failure
    }

    return NV_TRUE; // parse success
}

static NvBool
NvGrParseCompressionStringValue(const char *value, int *dst)
{
    NvBool compression = NV_FALSE;
    NvBool ret;

    ret = NvGrParseBooleanStringValue(value, &compression);

    if (ret == NV_TRUE) {
        if (compression == NV_TRUE) {
            *dst = NvGrCompression_Enabled;
        } else {
            *dst = NvGrCompression_Disabled;
        }
    }

    return ret;
}

static NvBool
NvGrParseDecompressionStringValue(const char *value, int *dst)
{
    if (!strcmp(value, "off") || !strcmp(value, "0") || !strcmp(value, "disabled")) {
        *dst = NvGrDecompression_Disabled;
    } else if (!strcmp(value, "client")) {
        *dst = NvGrDecompression_Client;
    } else if (!strcmp(value, "lazy")) {
        *dst = NvGrDecompression_Lazy;
    } else {
        return NV_FALSE; // parse failure
    }

    return NV_TRUE; // parse success
}

#define UPDATE_FIELD_FUNC(func, prop, parseFunc, memberName, defaultValue) \
    void func(NvGrModule *m) {                                             \
        char value[PROPERTY_VALUE_MAX];                                    \
        m->memberName.value = defaultValue;                                \
        if (get_env_property(prop, value) > 0) {                           \
            parseFunc(value, &m->memberName.value);                        \
        }                                                                  \
    }

UPDATE_FIELD_FUNC(NvGrUpdateCompression, NV_PROPERTY_COMPRESSION, NvGrParseCompressionStringValue,
                  compression, NvGrCompression_Enabled)

UPDATE_FIELD_FUNC(NvGrUpdateDecompression, NV_PROPERTY_DECOMPRESSION, NvGrParseDecompressionStringValue,
                  decompression, NvGrDecompression_Lazy)

UPDATE_FIELD_FUNC(NvGrUpdateGpuMappingCache, NV_PROPERTY_GPU_MAPPING_CACHE, NvGrParseBooleanStringValue,
                  gpuMappingCache, NV_TRUE)

UPDATE_FIELD_FUNC(NvGrUpdateScanProps, NV_PROPERTY_SCAN_PROPS, NvGrParseBooleanStringValue,
                  scanProps, NV_FALSE)

#undef UPDATE_FIELD_FUNC

int
NvGrOverrideProperty(NvGrModule *m, const char *propertyName, const char *value)
{
    NVGRD(("Override property %s ==> %s\n", propertyName, value));

#define OVERRIDE_PROPERTY_BLOCK(prop, parseFunc, memberName)                          \
    if (strcmp(prop, propertyName) == 0) {                                            \
        if (value != NULL)                                                            \
        {                                                                             \
            if (parseFunc(value, &m->memberName.override)) {                          \
                m->memberName.use_override = NV_TRUE;                                 \
                return 0;                                                             \
            }                                                                         \
        } else {                                                                      \
            m->memberName.use_override = NV_FALSE;                                    \
            return 0;                                                                 \
        }                                                                             \
        return -1;                                                                    \
    }

    OVERRIDE_PROPERTY_BLOCK(NV_PROPERTY_COMPRESSION, NvGrParseCompressionStringValue, compression);
    OVERRIDE_PROPERTY_BLOCK(NV_PROPERTY_DECOMPRESSION, NvGrParseDecompressionStringValue, decompression);
    OVERRIDE_PROPERTY_BLOCK(NV_PROPERTY_GPU_MAPPING_CACHE, NvGrParseBooleanStringValue, gpuMappingCache);
    OVERRIDE_PROPERTY_BLOCK(NV_PROPERTY_SCAN_PROPS, NvGrParseBooleanStringValue, scanProps);

#undef OVERRIDE_PROPERTY_BLOCK

    return -1;
}
