/*
 * Copyright (c) 2009-2014 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <errno.h>

#include "nvgralloc.h"
#include "nvgr_scratch.h"
#include "nvproperty_util.h"

static int
NvGrDevOpen(
    const hw_module_t* module,
    const char* name,
    hw_device_t** device)
{
    NvGrModule* m = (NvGrModule*)module;
    int idx = 0;

    if (NvOsStrcmp(name, GRALLOC_HARDWARE_GPU0) == 0)
        idx = NvGrDevIdx_Alloc;
    else if (NvOsStrcmp(name, GRALLOC_HARDWARE_FB0) == 0)
        idx = NvGrDevIdx_Fb0;
    else
        return -EINVAL;

    pthread_mutex_lock(&m->Lock);

    if (m->DevRef[idx] == 0)
    {
        int ret;

        switch (idx) {
        case NvGrDevIdx_Alloc:
            ret = NvGrAllocDevOpen(m, &m->Dev[idx]);
            break;
        case NvGrDevIdx_Fb0:
            *device = NULL;
            ret = 0;
            break;
        default:
            ret = -EINVAL;
            break;
        }

        if (ret)
        {
            pthread_mutex_unlock(&m->Lock);
            return ret;
        }
    }

    m->DevRef[idx]++;
    pthread_mutex_unlock(&m->Lock);
    *device = m->Dev[idx];
    return 0;
}

NvBool
NvGrDevUnref(NvGrModule* m, int idx)
{
    NvBool ret = NV_FALSE;

    pthread_mutex_lock(&m->Lock);
    if (--m->DevRef[idx] == 0)
    {
        ret = NV_TRUE;
        m->Dev[idx] = NULL;
    }
    pthread_mutex_unlock(&m->Lock);

    return ret;
}

static void
DeInit(NvGrModule* m)
{
    NvGr2dDeInit(m);
    if (m->Rm) {
        NvRmClose(m->Rm);
        m->Rm = NULL;
    }
    NvGrScratchDeInit(m);
    if (m->egl) {
        NvGrEglDeInit(m);
    }
}

NvError
NvGrModuleRef(NvGrModule* m)
{
    NvError err = NvSuccess;

    pthread_mutex_lock(&m->Lock);

    if (m->RefCount == 0) {
        int ret;

        err = NvRmOpen(&m->Rm, 0);

        if (err != NvSuccess) {
            DeInit(m);
            goto exit;
        }

        ret = NvGr2dInit(m);
        if (ret != 0) {
            DeInit(m);
            goto exit;
        }

        if (NvRmModuleGetNumInstances(m->Rm, NvRmModuleID_3D) != 0) {
            m->GpuIsAurora = NV_TRUE;
        }

        m->HaveVic = !!NvRmModuleGetNumInstances(m->Rm, NvRmModuleID_Vic);

        ret = NvGrScratchInit(m);
        if (ret != 0) {
            DeInit(m);
            goto exit;
        }

        // set properties
        m->compression.use_override = NV_FALSE;
        NvGrUpdateCompression(m);

        m->decompression.use_override = NV_FALSE;
        NvGrUpdateDecompression(m);

        m->gpuMappingCache.use_override = NV_FALSE;
        NvGrUpdateGpuMappingCache(m);

        m->scanProps.use_override = NV_FALSE;
        NvGrUpdateScanProps(m);

        // reset event counters
        memset(&m->eventCounters, 0, sizeof m->eventCounters);
    }

    m->RefCount++;

exit:
    pthread_mutex_unlock(&m->Lock);

    return err;
}

void
NvGrModuleUnref(NvGrModule* m)
{
    pthread_mutex_lock(&m->Lock);

    if (--m->RefCount == 0)
    {
        DeInit(m);
    }

    pthread_mutex_unlock(&m->Lock);
}

static NvBool
NvGrReadEventCounter(NvGrModule *m, NvGrEventCounterId id, NvU32 *dst)
{
    if (((unsigned) id) >= ((unsigned) NvGrEventCounterId_Count)) {
        return NV_FALSE;
    } else {
        volatile NvU32 *pcounter = &m->eventCounters[id];
        *dst = *pcounter;
        return NV_TRUE;
    }
}

//
// Globals for the HW module interface
//

static hw_module_methods_t NvGrMethods =
{
    .open = NvGrDevOpen
};

NvGrModule HAL_MODULE_INFO_SYM =
{
    .Base =
    {
        .common =
        {
            .tag = HARDWARE_MODULE_TAG,
            .version_major = 1,
            .version_minor = 0,
            .id = GRALLOC_HARDWARE_MODULE_ID,
            .name = "NVIDIA Tegra Graphics Memory Allocator",
            .author = "NVIDIA",
            .methods = &NvGrMethods
        },
        .registerBuffer = NvGrRegisterBuffer,
        .unregisterBuffer = NvGrUnregisterBuffer,
        .lock = NvGrLock,
        .lock_ycbcr = NvGrLock_ycbcr,
        .unlock = NvGrUnlock
        #ifdef GRALLOC_SUPPORTS_LOCK_ASYNC
        ,
        .lockAsync = NvGrLockAsync,
        .unlockAsync = NvGrUnlockAsync
        #endif
    },

    .add_nvfence = NvGrAddFence,
    .get_nvfences = NvGrGetFences,
    .get_fence = NvGrGetFenceFd,
    .add_fence = NvGrAddFenceFd,
    .get_source_crop = NvGrGetSourceCrop,
    .set_source_crop = NvGrSetSourceCrop,
    .alloc_scratch = NvGrAllocInternal,
    .free_scratch = NvGrFreeInternal,
    .clear = NvGr2DClear,
    .module_ref = NvGrModuleRef,
    .module_unref = NvGrModuleUnref,
    .dump_buffer = NvGrDumpBuffer,
    .scratch_open = NvGrScratchOpen,
    .scratch_close = NvGrScratchClose,
    .should_decompress = NvGrShouldDecompress,
    .set_compressed = NvGrSetCompressed,
    .decompress_buffer = NvGrDecompressBuffer,
    .set_gpu_mapping = NvGrSetGpuMapping,
    .get_gpu_mapping = NvGrGetGpuMapping,
    .blit = NvGr2dBlit,
    .override_property = NvGrOverrideProperty,
    .read_event_counter = NvGrReadEventCounter,
    .Lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER,
    .RefCount = 0,
    .SequenceNum = 0,
    .Rm = NULL,
};
