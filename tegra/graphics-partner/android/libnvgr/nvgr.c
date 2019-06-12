/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <fcntl.h>
#include <sys/resource.h>
#include <cutils/atomic.h>
#include <cutils/log.h>
#include <sync/sync.h>

#include "nvgr.h"
#include "nvgralloc.h"

static NvGrModule *GrModule;

static inline NvGrModule *get_module(void)
{
    if (!GrModule) {
        const hw_module_t *hwMod;
        hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &hwMod);
        GrModule = (NvGrModule *) hwMod;
        NVGR_ASSERT(GrModule);
    }

    return GrModule;
}

int nvgr_ref(buffer_handle_t handle)
{
    NvGrModule *m = get_module();

    return m->Base.registerBuffer(&m->Base, handle);
}

int nvgr_unref(buffer_handle_t handle)
{
    NvGrModule *m = get_module();

    return m->Base.unregisterBuffer(&m->Base, handle);
}

int nvgr_is_valid(buffer_handle_t handle)
{
    NvNativeHandle *h = (NvNativeHandle*) handle;

    return NvGrBufferIsValid(h);
}

int nvgr_is_yuv(buffer_handle_t handle)
{
    NvNativeHandle *h = (NvNativeHandle*)handle;

    NVGR_ASSERT(NvGrBufferIsValid(h));

    return h->Type == NV_NATIVE_BUFFER_YUV;
}

int nvgr_is_stereo(buffer_handle_t handle)
{
    NvNativeHandle *h = (NvNativeHandle*)handle;

    NVGR_ASSERT(NvGrBufferIsValid(h));

    return h->Type == NV_NATIVE_BUFFER_STEREO;
}

int nvgr_get_format(buffer_handle_t handle)
{
    NvNativeHandle *h = (NvNativeHandle*) handle;

    NVGR_ASSERT(NvGrBufferIsValid(h));

    return h->Format;
}

int nvgr_get_usage(buffer_handle_t handle)
{
    NvNativeHandle *h = (NvNativeHandle*) handle;

    NVGR_ASSERT(NvGrBufferIsValid(h));

    return h->Usage;
}

int nvgr_get_memfd(buffer_handle_t handle)
{
    NvNativeHandle *h = (NvNativeHandle*) handle;

    NVGR_ASSERT(NvGrBufferIsValid(h));

    return h->SurfMemFd;
}

void nvgr_get_surfaces(buffer_handle_t handle,
                       const NvRmSurface **surfs,
                       size_t *surfCount)
{
    NvNativeHandle *h = (NvNativeHandle*) handle;

    NVGR_ASSERT(NvGrBufferIsValid(h));

    if (surfCount) *surfCount = (size_t) h->SurfCount;
    if (surfs) *surfs = h->Surf;
}

NvU32 nvgr_get_write_count(buffer_handle_t handle)
{
    NvNativeHandle *h = (NvNativeHandle*)handle;

    return NvGrGetWriteCount(h);
}

void nvgr_inc_write_count(buffer_handle_t handle)
{
    NvNativeHandle *h = (NvNativeHandle*)handle;

    NvGrIncWriteCount(h);
}

NvU32 nvgr_get_stereo_info(buffer_handle_t handle)
{
    NvNativeHandle *h = (NvNativeHandle*)handle;

    return NvGrGetStereoInfo(h);
}

void nvgr_set_stereo_info(buffer_handle_t handle, NvU32 stereoInfo)
{
    NvNativeHandle *h = (NvNativeHandle*)handle;

    NvGrSetStereoInfo(h, stereoInfo);
}

void nvgr_set_video_timestamp(buffer_handle_t handle, NvS64 timeStamp)
{
    NvNativeHandle *h = (NvNativeHandle*)handle;

    NvGrSetVideoTimeStamp(h, timeStamp);
}

void nvgr_get_crop(buffer_handle_t handle, NvRect *crop)
{
    NvGrModule *m = get_module();

    m->get_source_crop(&m->Base, handle, crop);
}

void nvgr_set_crop(buffer_handle_t handle, NvRect *crop)
{
    NvGrModule *m = get_module();

    m->set_source_crop(&m->Base, handle, crop);
}

int nvgr_buffers_are_equal(buffer_handle_t handle0,
                           buffer_handle_t handle1)
{
    NvNativeHandle *h0 = (NvNativeHandle*)handle0;
    NvNativeHandle *h1 = (NvNativeHandle*)handle1;

    NVGR_ASSERT(NvGrBufferIsValid(h0));
    NVGR_ASSERT(NvGrBufferIsValid(h1));

    return h0 == h1 && h0->SequenceNum == h1->SequenceNum;
}

int nvgr_decompress(buffer_handle_t handle, int fenceIn, int *fenceOut)
{
    NvGrModule *m = get_module();
    NvNativeHandle *h = (NvNativeHandle*)handle;

    NVGR_ASSERT(NvGrBufferIsValid(h));

    return m->decompress_buffer(m, h, fenceIn, fenceOut);
}

int nvgr_should_decompress(buffer_handle_t handle)
{
    NvGrModule *m = get_module();

    return m->should_decompress(&m->Base, handle);
}

void nvgr_set_compressed(buffer_handle_t handle,
                       int compressed)
{
    NvGrModule *m = get_module();

    m->set_compressed(&m->Base, handle, compressed);
}

void *nvgr_get_gpu_mapping(buffer_handle_t handle)
{
    NvGrModule *m = get_module();
    NvNativeHandle *h = (NvNativeHandle*) handle;

    return m->get_gpu_mapping(m, h);
}

int nvgr_set_gpu_mapping(buffer_handle_t handle,
                         void *mapping,
                         NvGrUnmapCallback unmap)
{
    NvGrModule *m = get_module();
    NvNativeHandle *h = (NvNativeHandle*) handle;

    return m->set_gpu_mapping(m, h, mapping, unmap);
}


int nvgr_get_fence(buffer_handle_t handle, int *fenceFd)
{
    NvGrModule *m = get_module();
    NvNativeHandle *h = (NvNativeHandle*) handle;

    /* Get any existing fences */
    *fenceFd = m->get_fence(h, GRALLOC_USAGE_HW_RENDER);

    return 0;
}

/* This needs to go away.  Only nvcap uses it. */
int nvgr_put_read_fence(buffer_handle_t handle, int fenceFd)
{
    NvGrModule *m = get_module();
    NvNativeHandle *h = (NvNativeHandle*) handle;

    m->add_fence(h, GRALLOC_USAGE_HW_TEXTURE, fenceFd);

    return 0;
}

/* This needs to go away.  Only nvomx encoder uses it. */
int nvgr_put_read_nvfence(buffer_handle_t handle, const NvRmFence *fence)
{
    NvGrModule *m = get_module();
    NvNativeHandle *h = (NvNativeHandle*) handle;

    /* Add the fence */
    m->add_nvfence(h, GRALLOC_USAGE_HW_TEXTURE, fence);

    return 0;
}

/* This needs to go away.  Only nvcap uses it. */
int nvgr_put_write_fence(buffer_handle_t handle, int fenceFd)
{
    NvGrModule *m = get_module();
    NvNativeHandle *h = (NvNativeHandle*) handle;
    int err;

    /* Add the fence */
    m->add_fence(h, GRALLOC_USAGE_HW_RENDER, fenceFd);

    return 0;
}

/* This needs to go away.  Only nvomx decoder uses it. */
int nvgr_put_write_nvfence(buffer_handle_t handle, const NvRmFence *fence)
{
    NvGrModule *m = get_module();
    NvNativeHandle *h = (NvNativeHandle*) handle;

    /* Add the fence */
    m->add_nvfence(h, GRALLOC_USAGE_HW_RENDER, fence);

    return 0;
}

int nvgr_scratch_alloc(int width, int height, int format, int usage,
                       NvRmSurfaceLayout layout, buffer_handle_t *handle)
{
    NvGrModule *m = get_module();

    return m->alloc_scratch(m, width, height, format, usage, layout,
                            (NvNativeHandle **) handle);
}

void nvgr_scratch_free(buffer_handle_t handle)
{
    NvGrModule *m = get_module();
    NvNativeHandle *h = (NvNativeHandle*) handle;

    m->free_scratch(m, h);
}

int nvgr_clear(buffer_handle_t handle, NvRect *rect, NvU32 color,
               int inFence, int *outFence)
{
    NvGrModule *m = get_module();

    return m->clear(m, handle, rect, color, inFence, outFence);
}

int nvgr_blit(buffer_handle_t srcHandle, NvRect *srcRect, int srcFence,
              buffer_handle_t dstHandle, NvRect *dstRect, int dstFence,
              int transform, int *outFence)
{
    NvGrModule *m = get_module();
    NvNativeHandle *src = (NvNativeHandle*) srcHandle;
    NvNativeHandle *dst = (NvNativeHandle*) dstHandle;
    NvRect src_rect, dst_rect;

    if (!srcRect) {
        srcRect = &src_rect;
        src_rect.left   = 0;
        src_rect.top    = 0;
        src_rect.right  = src->Surf[0].Width;
        src_rect.bottom = src->Surf[0].Height;
    }

    if (!dstRect) {
        dstRect = &dst_rect;
        dst_rect.left   = 0;
        dst_rect.top    = 0;
        dst_rect.right  = dst->Surf[0].Width;
        dst_rect.bottom = dst->Surf[0].Height;
    }

    return m->blit(m,
                   src, srcRect, srcFence,
                   dst, dstRect, dstFence,
                   transform, NULL, outFence);
}

/* Temporary hacks for OMX video editor use case */

size_t nvgr_get_handle_size(void)
{
    return sizeof(NvNativeHandle);
}

void nvgr_get_surfaces_no_buffer_magic(buffer_handle_t handle,
                                       const NvRmSurface **surfs,
                                       size_t *surfCount)
{
    NvNativeHandle *h = (NvNativeHandle*) handle;

    if (h && h->Magic == NVGR_HANDLE_MAGIC) {
        if (surfCount) *surfCount = (size_t) h->SurfCount;
        if (surfs) *surfs = h->Surf;
    }
}

int nvgr_override_property(const char *propertyName, const char *value)
{
    NvGrModule *m = get_module();
    return m->override_property(m, propertyName, value);
}

int nvgr_read_event_counter(int counterId, NvU32 *value)
{
    NvGrModule *m = get_module();
    return m->read_event_counter(m, counterId, value);
}

void nvgr_sync_dump(void)
{
    int i;
    int max;
    struct rlimit rlim;

    if (getrlimit(RLIMIT_NOFILE, &rlim) != 0) {
        rlim.rlim_cur = FD_SETSIZE;
    }

    max = rlim.rlim_cur;

    for (i=0; i<max; ++i) {
        struct sync_fence_info_data *data;

        if (fcntl(i, F_GETFL) == -1 && errno == EBADF) {
            continue;
        }

        data = sync_fence_info(i);
        if (data == NULL)
            continue;

        ALOGI("fd[%d]: %d - %s\n", i, fcntl(i, F_GETOWN), data->name);

        sync_fence_info_free(data);
    }
}

void nvgr_dump(buffer_handle_t handle, const char *filename)
{
    NvGrModule *m = get_module();

    m->dump_buffer(&m->Base, handle, filename);
}
