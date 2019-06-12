/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVGR_PROPS_H
#define INCLUDED_NVGR_PROPS_H

// A property that can be overridden. If use_override==NV_FALSE, use
// value. Otherwise, use override.
struct NvGrOverridablePropertyIntRec {
    int value;
    int override;
    NvBool use_override;
};

struct NvGrOverridablePropertyNvBoolRec {
    NvBool value;
    NvBool override;
    NvBool use_override;
};

typedef enum {
    // Allocate compressed buffers in gralloc
    NvGrCompression_Enabled,
    // Do not allocate compressed buffers in gralloc
    NvGrCompression_Disabled,
} NvGrCompression;

typedef enum {
    // Client APIs are responsible for decompression
    NvGrDecompression_Client,
    // Never decompress (shows compressed buffers on the screen)
    NvGrDecompression_Disabled,
    // Decompress in gralloc when needed
    NvGrDecompression_Lazy,
} NvGrDecompression;

static inline NvBool NvGrGetPropertyValueNvBool(const NvGrOverridablePropertyNvBool *p)
{
    return !p->use_override? p->value : p->override;
}

static inline int NvGrGetPropertyValueInt(const NvGrOverridablePropertyInt *p)
{
    return !p->use_override ? p->value : p->override;
}

void NvGrUpdateCompression(NvGrModule *);
void NvGrUpdateDecompression(NvGrModule *);
void NvGrUpdateGpuMappingCache(NvGrModule *);
void NvGrUpdateScanProps(NvGrModule *);
int NvGrOverrideProperty(NvGrModule *m, const char *propertyName, const char *value);

#endif
