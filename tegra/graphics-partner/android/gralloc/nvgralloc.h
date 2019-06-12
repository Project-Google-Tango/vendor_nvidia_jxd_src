/*
 * Copyright (c) 2009-2014 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVGRALLOC_H
#define INCLUDED_NVGRALLOC_H

#include <hardware/gralloc.h>
#include <pthread.h>

#include "nvrm_surface.h"

#include "nvgr.h"
#include "nvgr_types.h"
#include "nvgr_props.h"
#include "nvgr_scratch.h"
#include "nvgrbuffer.h"
#include "nvassert.h"

#define NVGR_ENABLE_TRACE 0
#define NVGRP(x) NvOsDebugPrintf x
#if NVGR_ENABLE_TRACE
#define NVGRD(x) NVGRP(x)
#else
#define NVGRD(x)
#endif

#define ROUND_TO_PAGE(x) ((x + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1))

enum {
    NvGrDevIdx_Alloc = 0,
    NvGrDevIdx_Fb0,
    NvGrDevIdx_Num,
};

typedef enum {
    NvGrEventCounterId_LazyDecompression = 0,

    NvGrEventCounterId_Count
} NvGrEventCounterId;

struct NvGrModuleRec {
    gralloc_module_t    Base;

    // NV gralloc API extensions

    NvError (*module_ref)(NvGrModule *m);
    void (*module_unref)(NvGrModule *m);

    int (*alloc_scratch)(NvGrModule *ctx, int width, int height,
                         int format, int usage, NvRmSurfaceLayout layout,
                         NvNativeHandle **handle);
    int (*free_scratch)(NvGrModule *ctx, NvNativeHandle *handle);

    // NvRmFence based fence API
    void (*add_nvfence) (NvNativeHandle *, int usage, const NvRmFence*);
    void (*get_nvfences) (NvNativeHandle *, int usage, NvRmFence*, int*);
    // Android fence file based fence API
    int  (*get_fence) (NvNativeHandle *, int usage); // caller must close the handle
    void (*add_fence) (NvNativeHandle *, int usage, int); // gralloc will close the handle

    void (*get_source_crop) (gralloc_module_t const*, buffer_handle_t, NvRect *);
    void (*set_source_crop) (gralloc_module_t const*, buffer_handle_t, NvRect *);

    int  (*clear)(NvGrModule *ctx, buffer_handle_t handle, NvRect *rect,
                  NvU32 color, int inFence, int *outFence);

    void (*dump_buffer)(gralloc_module_t const*, buffer_handle_t, const char *filename);
    int (*scratch_open)(NvGrModule *ctx, size_t count, NvGrScratchClient **scratch);
    void (*scratch_close)(NvGrModule *ctx, NvGrScratchClient *sc);

    int (*should_decompress) (gralloc_module_t const*, buffer_handle_t);
    void (*set_compressed) (gralloc_module_t const*, buffer_handle_t, int);
    int (*decompress_buffer)(NvGrModule *m, NvNativeHandle *h,
                             int inFence, int *outFence);
    int (*set_gpu_mapping) (NvGrModule *m, NvNativeHandle *h,
                            void *mapping, NvGrUnmapCallback unmap);
    void* (*get_gpu_mapping) (NvGrModule *m, NvNativeHandle *h);
    int (*blit) (NvGrModule *ctx,
                 NvNativeHandle *src, NvRect *srcRect, int srcFence,
                 NvNativeHandle *dst, NvRect *dstRect, int dstFence,
                 int transform, NvPoint *dstOffset, int *outFence);

    int (*override_property)(NvGrModule *m, const char *propertyName,
                             const char *overrideValue);

    // NV_TRUE if successful
    // NV_FALSE if no such event counter exists
    NvBool (*read_event_counter)(NvGrModule *m, NvGrEventCounterId eventCounterId,
                                 NvU32 *eventCounterValue);

    // Module private state
    pthread_mutex_t     Lock;
    NvS32               RefCount;
    NvS32               SequenceNum;
    NvRmDeviceHandle    Rm;
    NvGrScratchClient  *scratch_clients[NVGR_MAX_SCRATCH_CLIENTS];
    NvGrEglState       *egl;

    NvGrOverridablePropertyInt    compression;
    NvGrOverridablePropertyInt    decompression;
    NvGrOverridablePropertyNvBool gpuMappingCache;
    NvGrOverridablePropertyNvBool scanProps;

    NvU32 eventCounters[NvGrEventCounterId_Count];

    void                (*CompDone)(void*);
    void*               CompDoneOpaque;
    NvBool              GpuIsAurora;
    NvBool              HaveVic;
    struct NvGrScratchMachineRec *scratch;

    struct NvBlitContextRec *nvblit;

    // Devices
    hw_device_t*        Dev[NvGrDevIdx_Num];
    int                 DevRef[NvGrDevIdx_Num];
};

typedef struct {
    int Width;
    int Height;
    int Format;
    int Usage;
    NvRmSurfaceLayout Layout;
} NvGrAllocParameters;

struct NvGrAllocDevRec {
    alloc_device_t Base;

    // Private methods and data
    int (*alloc_ext)(NvGrAllocDev *dev, NvGrAllocParameters *params,
                     NvNativeHandle** handle);
};

static inline NvBool NvGrGetScanProps(NvGrModule *m)
{
    return NvGrGetPropertyValueNvBool(&m->scanProps);
}

static inline NvGrCompression NvGrGetCompression(NvGrModule *m)
{
    if (NvGrGetScanProps(m) == NV_TRUE) {
        NvGrUpdateCompression(m);
    }
    return (NvGrCompression) NvGrGetPropertyValueInt(&m->compression);
}

static inline NvGrDecompression NvGrGetDecompression(NvGrModule *m)
{
    if (NvGrGetScanProps(m) == NV_TRUE) {
        NvGrUpdateDecompression(m);
    }
    return (NvGrDecompression) NvGrGetPropertyValueInt(&m->decompression);
}

static inline NvBool NvGrGetGpuMappingCache(NvGrModule *m)
{
    if (NvGrGetScanProps(m) == NV_TRUE) {
        NvGrUpdateGpuMappingCache(m);
    }
    return NvGrGetPropertyValueNvBool(&m->gpuMappingCache);
}

static inline
void NvGrEventCounterInc(NvGrModule *m, NvGrEventCounterId id)
{
    int32_t *pcounter = (int32_t *)&m->eventCounters[id];

    NV_ASSERT((unsigned)id < (unsigned)NvGrEventCounterId_Count);

    android_atomic_inc(pcounter);
}

// nvgrmodule.c
NvError NvGrModuleRef    (NvGrModule* m);
void    NvGrModuleUnref  (NvGrModule* m);
NvBool  NvGrDevUnref     (NvGrModule* m, int idx);

// nvgralloc.c
int NvGrAllocInternal (NvGrModule *m, int width, int height, int format,
                       int usage, NvRmSurfaceLayout layout,
                       NvNativeHandle **handle);
int NvGrFreeInternal  (NvGrModule *m, NvNativeHandle *handle);
int NvGrAllocDevOpen  (NvGrModule* mod, hw_device_t** dev);

// nvgr_2d.c
int NvGr2dInit(NvGrModule *ctx);
void NvGr2dDeInit(NvGrModule *ctx);
int NvGr2dBlit(NvGrModule *ctx,
               NvNativeHandle *src, NvRect *srcRect, int srcFence,
               NvNativeHandle *dst, NvRect *dstRect, int dstFence,
               int transform, NvPoint *dstOffset, int *outFence);
int NvGr2DClear(NvGrModule *ctx, buffer_handle_t handle, NvRect *rect,
                NvU32 color, int inFence, int *outFence);
int NvGr2DClearImplicit(NvGrModule *ctx, NvNativeHandle *h);
int NvGr2dCopyBuffer(NvGrModule *ctx, NvNativeHandle *src,
                     NvNativeHandle *dst,
                     const NvRmFence *srcWriteFenceIn,
                     const NvRmFence *dstReadFencesIn,
                     NvU32 numDstReadFencesIn,
                     int *fenceOut);

// nvgr_egl.c
NvError NvGrEglInit(NvGrModule *m);
void NvGrEglDeInit(NvGrModule *m);
NvError NvGrEglDecompressBuffer(NvGrModule *m, NvNativeHandle *h,
                                int fenceIn, int *fenceOut);

#endif
