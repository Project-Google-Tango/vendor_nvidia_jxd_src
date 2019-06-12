/*
 * Copyright (c) 2009-2014, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <errno.h>
#include <unistd.h>
#include <cutils/ashmem.h>
#include <cutils/atomic.h>
#include <cutils/log.h>
#include <sys/mman.h>

#include "nvgralloc.h"
#include "nvsync.h"

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include "nvatrace.h"

// here is cache line size we are optimizing our
// cache maintenance for
#define CACHE_LINE_SIZE 64

#ifndef min
    #define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

static void AcquireBufferLock(NvNativeHandle *h, int usage)
{
    NV_ATRACE_BEGIN(__func__);
    int need_exclusive = !NVGR_READONLY_USE(usage);

    pthread_mutex_lock(&h->Buf->LockMutex);

    while ((h->Buf->LockState & NVGR_WRITE_LOCK_FLAG) ||
           (need_exclusive && h->Buf->LockState))
    {
        struct timespec timeout = {2, 0};

        if (pthread_cond_timedwait_relative_np(&h->Buf->LockExclusive,
                &h->Buf->LockMutex, &timeout) == ETIMEDOUT) {
            ALOGE("%s timed out for thread %d buffer 0x%x usage 0x%x LockState %d\n",
                  __func__, gettid(), h->MemId, usage, h->Buf->LockState);
        }
    }

    if (need_exclusive)
    {
        h->Buf->LockState |= NVGR_WRITE_LOCK_FLAG;
        if (NVGR_SW_USE(usage))
            h->Buf->LockState |= NVGR_SW_WRITE_LOCK_FLAG;
    }
    else
    {
        h->Buf->LockState++;
        pthread_mutex_unlock(&h->Buf->LockMutex);
    }
    NV_ATRACE_END();
}

static void ReleaseBufferLock(NvNativeHandle *h)
{
    NV_ATRACE_BEGIN(__func__);

    if (h->Buf->LockState & NVGR_WRITE_LOCK_FLAG)
    {
        // exclusive lock held
        h->Buf->LockState = 0;
        pthread_cond_broadcast(&h->Buf->LockExclusive);
        pthread_mutex_unlock(&h->Buf->LockMutex);
    }
    else
    {
        pthread_mutex_lock(&h->Buf->LockMutex);
        h->Buf->LockState--;
        if (!h->Buf->LockState)
            pthread_cond_signal(&h->Buf->LockExclusive);
        pthread_mutex_unlock(&h->Buf->LockMutex);
    }
    NV_ATRACE_END();
}

int NvGrRegisterBuffer (gralloc_module_t const* module,
                        buffer_handle_t handle)

{
    NvGrModule*         m = (NvGrModule*)module;
    NvNativeHandle*     h = (NvNativeHandle*)handle;
    NvError             e = NvSuccess;
    NvU32               ii;

    NV_ASSERT(h->Magic == NVGR_HANDLE_MAGIC);

    // If buffer is owned by me, just inc refcount

    if (h->Owner == getpid() && h->hSelf == h)
    {
        android_atomic_inc(&h->RefCount);
        return 0;
    }

    NVGRD(("NvGrRegisterBuffer [0x%08x]: Attaching", h->MemId));

    h->Owner = getpid();
    h->hSelf = h;

    // Get a reference on the module

    if (NvGrModuleRef(m) != NvSuccess)
        return -ENOMEM;

    // Start with pixels not mapped

    h->Pixels = NULL;
    h->hShadow = NULL;
    h->RefCount = 1;
    h->DecompressFenceFd = -1;
    h->GpuMapping = NULL;
    h->GpuUnmapCallback = NULL;
    pthread_mutex_init(&h->MapMutex, NULL);

    // Map buffer memory

    h->Buf = mmap(0, ROUND_TO_PAGE(sizeof(NvGrBuffer)),
                  PROT_READ|PROT_WRITE, MAP_SHARED, h->MemId, 0);
    if (h->Buf == MAP_FAILED)
        return -ENOMEM;

    // Get a surface memory handle for this process

    e = NvRmMemHandleFromFd(h->SurfMemFd, &h->Surf[0].hMem);
    if (NV_SHOW_ERROR(e))
        return -ENOMEM;

    // Copy the memory handle to all surfaces
    for (ii = 1; ii < h->SurfCount; ii++) {
        h->Surf[ii].hMem = h->Surf[0].hMem;
    }

    return 0;
}

int NvGrUnregisterBuffer (gralloc_module_t const* module,
                          buffer_handle_t handle)
{
    NvNativeHandle* h = (NvNativeHandle*)handle;
    NvGrModule*     m = (NvGrModule*)module;
    int i;

    NV_ASSERT(h->Magic == NVGR_HANDLE_MAGIC);
    NV_ASSERT(h->Buf->Magic == NVGR_BUFFER_MAGIC);

    NVGRD(("NvGrUnregisterBuffer [0x%08x]: Refcount %d", h->MemId, h->RefCount));

    if (android_atomic_dec(&h->RefCount) > 1)
        return 0;

    if (h->hShadow) {
        NvNativeHandle *hShadow = h->hShadow;

        if (hShadow->Pixels) {
            NvRmMemUnmap(hShadow->Surf[0].hMem, hShadow->Pixels,
                         hShadow->Buf->SurfSize);
        }

        // The shadow buffer should never be used by the GPU.
        NV_ASSERT(!hShadow->GpuMapping);

        // The shadow buffer should never have an internal fence
        // since it's neither compressible nor cleared on alloc.
        NV_ASSERT(hShadow->DecompressFenceFd == -1);

        // The mem fd's should have been closed since we never
        // share shadow buffers across processes.
        NV_ASSERT(hShadow->MemId == -1);
        NV_ASSERT(hShadow->SurfMemFd == -1);

        NvRmMemHandleFree(hShadow->Surf[0].hMem);

        pthread_mutex_destroy(&hShadow->MapMutex);
        munmap(hShadow->Buf, ROUND_TO_PAGE(sizeof(NvGrBuffer)));

        // Shadow buffers are not shared between processes
        NV_ASSERT(hShadow->Owner == getpid());
        NvOsFree(hShadow);
    }

    if (h->Pixels)
        NvRmMemUnmap(h->Surf[0].hMem, h->Pixels, h->Buf->SurfSize);

    if (h->GpuMapping) {
        NV_ASSERT(h->GpuUnmapCallback);
        h->GpuUnmapCallback(h->GpuMapping);
    }

    nvsync_close(h->DecompressFenceFd);
    close(h->SurfMemFd);
    h->SurfMemFd = -1;
    NvRmMemHandleFree(h->Surf[0].hMem);

    pthread_mutex_destroy(&h->MapMutex);
    if (h->Owner == getpid()) {
        // Bug 782321
        // Don't delete these mutexes because a client may still
        // reference this buffer and will hang if the mutex is
        // de-inited.  Since pthread mutexes on android use
        // pre-allocated memory, there is no leak here.
        //
        // pthread_mutex_destroy(&h->Buf->FenceMutex);
        // pthread_mutex_destroy(&h->Buf->LockMutex);
        // pthread_cond_destroy(&h->Buf->LockExclusive);
    }

    munmap(h->Buf, ROUND_TO_PAGE(sizeof(NvGrBuffer)));

    /* Free the handle only if we've come through alloc_device_t::free */
    if (h->DeleteHandle) {
        close(h->MemId);
        NvOsFree(h);
    }

    NvGrModuleUnref(m);
    return 0;
}

static inline NvBool
IsSubRect(NvNativeHandle *h, int l, int t, int width, int height)
{
    return l || t ||
        width < (int) h->Surf[0].Width ||
        height < (int) h->Surf[0].Height;
}

static inline int32_t IsCompressed(NvNativeHandle *h)
{
    return android_atomic_acquire_load(&h->Buf->Compressed);
}

static inline void SetCompressed(NvNativeHandle *h, int32_t compressed)
{
    android_atomic_release_store(compressed, &h->Buf->Compressed);
}

int NvGrShouldDecompress(gralloc_module_t const* module,
                         buffer_handle_t handle)
{
    NvGrModule*     m = (NvGrModule*)module;
    NvNativeHandle* h = (NvNativeHandle*)handle;
    const NvGrDecompression decompression = NvGrGetDecompression(m);
    int ret;

    if (decompression == NvGrDecompression_Disabled ||
            decompression == NvGrDecompression_Lazy) {
        return 0;
    }
    return IsCompressed(h);
}

int NvGrSetGpuMapping(NvGrModule *m, NvNativeHandle *h,
                      void *mapping, NvGrUnmapCallback unmap)
{
    if (!NvGrGetGpuMappingCache(m)) {
        return -1;
    }

    NV_ASSERT(m);
    NV_ASSERT(h);
    h->GpuMapping = mapping;
    h->GpuUnmapCallback = unmap;
    return 0;
}

void* NvGrGetGpuMapping(NvGrModule *m, NvNativeHandle *h)
{
    NV_ASSERT(m);
    NV_ASSERT(h);
    return h->GpuMapping;
}

void NvGrSetCompressed(gralloc_module_t const* module,
                       buffer_handle_t handle,
                       int compressed)
{
    NvGrModule*     m = (NvGrModule*)module;
    NvNativeHandle* h = (NvNativeHandle*)handle;
    NvRmSurface*    s = &h->Surf[0];

    SetCompressed(h, compressed && NvRmMemKindIsCompressible(s->Kind));
}

int NvGrDecompressBuffer(NvGrModule *m, NvNativeHandle *h, int fenceIn,
                         int *fenceOut)
{
    const NvGrDecompression decompression = NvGrGetDecompression(m);

    if (IsCompressed(h) && decompression != NvGrDecompression_Disabled) {
        NvError err = NvGrEglDecompressBuffer(m, h, fenceIn, fenceOut);
        if (err == NvSuccess) {
            SetCompressed(h, 0);
            nvsync_close(h->DecompressFenceFd);
            h->DecompressFenceFd = nvsync_dup(__func__, *fenceOut);
        }
        if (decompression != NvGrDecompression_Lazy) {
            ALOGW("%s: Unexpected lazy decompression", __func__);
        }
        return err != NvSuccess;
    } else {
        if (h->DecompressFenceFd >= 0 && fenceIn >= 0) {
            *fenceOut = nvsync_merge(__func__, fenceIn, h->DecompressFenceFd);
            nvsync_close(fenceIn);
        } else if (h->DecompressFenceFd >= 0) {
            *fenceOut = nvsync_dup(__func__, h->DecompressFenceFd);
        } else {
            *fenceOut = fenceIn;
        }
        return 0;
    }
}

// Fence mutex must be taken.
static void NvGrDecompressLocked(NvGrModule *m, NvNativeHandle *h)
{
    int fenceIn = -1;
    int fenceOut = -1;
    NvError err;

    // Wait for the current write fence.
    if (h->Buf->Fence[0].SyncPointID != NVRM_INVALID_SYNCPOINT_ID) {
        fenceIn = nvsync_from_fence("preLazyDecompress", &h->Buf->Fence[0], 1);
    }

    err = NvGrEglDecompressBuffer(m, h, fenceIn, &fenceOut);
    if (err != NvSuccess) {
        nvsync_close(fenceIn);
        return;
    }
    SetCompressed(h, 0);

    // Overwrite the write fence.
    if (fenceOut >= 0) {
        NvU32 numFences = 1;
        nvsync_to_fence(fenceOut, &h->Buf->Fence[0], &numFences);
        nvsync_close(fenceOut);
    }
}

static int NvGrLockCommon(gralloc_module_t const* module,
                          buffer_handle_t handle,
                          int usage, int l, int t, int width, int height,
                          void** vaddr, int fd)
{
    NvGrModule*     m = (NvGrModule*)module;
    NvNativeHandle* h = (NvNativeHandle*)handle;
    NvBool needDecompressed;
    const NvGrDecompression decompression = NvGrGetDecompression(m);
    NvRmFence *fences, safef;
    NvU32 numFences;

    NV_ASSERT(h->Magic == NVGR_HANDLE_MAGIC);
    NV_ASSERT(h->Buf->Magic == NVGR_BUFFER_MAGIC);
    NV_ASSERT(m->RefCount);

    NVGRD(("NvGrLock [0x%08x]: rect (%d,%d,%d,%d) usage %x",
           h->MemId, l, t, width, height, usage));

    // This is a SW-only path
    if (usage & (GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_TEXTURE)) {
        ALOGE("%s: Attempting to lock buffer for HW use", __func__);
        return -EINVAL;
    }

    if (h->Usage & GRALLOC_USAGE_PROTECTED) {
        ALOGE("%s: Attempting to lock PROTECTED buffer for SW use", __func__);
        return -EINVAL;
    }

    // Wait on the input fence. For perf improvements this should be passed onto
    // decompression or shadow copy blit
    nvsync_wait(fd, -1);
    nvsync_close(fd);

    // Get a lock based on usage. If this is a write lock, we'll have exclusive
    // access to both shared and process local state from here on.

    AcquireBufferLock(h, usage);

    needDecompressed = (decompression != NvGrDecompression_Disabled);

    if (NVGR_WRITEONLY_USE(usage) && !IsSubRect(h, l, t, width, height)) {
        // Write-only for the whole buffer, we can discard the compressed data.
        // TODO: For this to work, we need a way to clear the compbits (and
        //       depending on how we do that possibly also invalidate the GPU
        //       compression bit cache (CBC)). Bug 1330070.
        // needDecompressed = NV_FALSE;
        // NvGrSetCompressed(module, handle, 0);
    }

    // checking flag without lock
    if (needDecompressed && IsCompressed(h)) {
        pthread_mutex_lock(&h->Buf->FenceMutex);
        // checking flag with lock to avoid double decompression
        if (IsCompressed(h)) {
            if (decompression != NvGrDecompression_Lazy) {
                ALOGW("%s: Unexpected lazy decompression "
                      "(persist.tegra.decompression=%s)", __func__,
                      decompression == NvGrDecompression_Client ?
                      "client" : "off");
            }
            NvGrDecompressLocked(m, h);
        }
        pthread_mutex_unlock(&h->Buf->FenceMutex);
    }

    if (NVGR_READONLY_USE(usage)) {
        pthread_mutex_lock(&h->Buf->FenceMutex);
        // Copy the fence so it can be used outside the mutex
        safef = h->Buf->Fence[0];
        fences = &safef;
        numFences = 1;
        pthread_mutex_unlock(&h->Buf->FenceMutex);
    } else {
        // exclusive buffer lock held, no need for fencemutex
        fences = h->Buf->Fence;
        numFences = NVGR_MAX_FENCES;
    }

    pthread_mutex_lock(&h->MapMutex);

    if (NvGrUseShadow(h)) {
        int ret;

        if (!h->hShadow) {
            NvError e;
            // Alloc Shadow Buffer
            ret = NvGrAllocInternal(m,
                                    h->Surf[0].Width,
                                    h->Surf[0].Height,
                                    h->ExtFormat,
                                    h->Usage,
                                    NvRmSurfaceLayout_Pitch,
                                    &h->hShadow);
            if (ret != 0) {
                pthread_mutex_unlock(&h->MapMutex);
                ReleaseBufferLock(h);
                return -ENOMEM;
            }

            // Shadow buffer will never be shared across processes,
            // we can close the mem fds now.
            close(h->hShadow->MemId);
            h->hShadow->MemId = -1;
            close(h->hShadow->SurfMemFd);
            h->hShadow->SurfMemFd = -1;

            // Assert that the shadow buffer stride matches the one that
            // we returned on alloc()
            NV_ASSERT(h->LockedPitchInPixels == 0 ||
                      h->hShadow->Surf[0].Pitch == h->LockedPitchInPixels *
                      (NV_COLOR_GET_BPP(h->hShadow->Surf[0].ColorFormat) >> 3));

            // MemMap the shadow for CPU access
            e = NvRmMemMap(h->hShadow->Surf[0].hMem, 0,
                           h->hShadow->Buf->SurfSize,
                           NVOS_MEM_READ_WRITE, (void**)&h->hShadow->Pixels);
            if (NV_SHOW_ERROR(e)) {
                pthread_mutex_unlock(&h->MapMutex);
                ReleaseBufferLock(h);
                return -ENOMEM;
            }

            // Invalidate the cache to clear pending writebacks.
            NvRmMemCacheMaint(h->hShadow->Surf[0].hMem, h->hShadow->Pixels,
                              h->hShadow->Buf->SurfSize, NV_TRUE, NV_TRUE);

            // WritebackInvalidate ops can be deferred in kernel. Here we
            // invalidate one line of cache to flush pending cache maintenance
            // operations for the handle.
            NvRmMemCacheMaint(h->hShadow->Surf[0].hMem, h->hShadow->Pixels,
                              min(CACHE_LINE_SIZE, h->hShadow->Buf->SurfSize),
                              NV_FALSE, NV_TRUE);

        } else if (!h->hShadow->Pixels)  {
            pthread_mutex_unlock(&h->MapMutex);
            ReleaseBufferLock(h);
            return -ENOMEM;
        }

        // Update the shadow copy if read access is requested or
        // locking a subrect, in which case the region outside the
        // rect is preserved.
        if (h->Buf->WriteCount != h->hShadow->Buf->WriteCount &&
            ((usage & GRALLOC_USAGE_SW_READ_MASK) ||
             IsSubRect(h, l, t, width, height))) {
            // Src write fences: Wait for any writes on the
            // primary buffer.  Dst read fences: Read fences
            // on the shadow buffer can only come from the
            // unlock operation. All such fences have been
            // added as write fences to the primary buffer, so
            // waiting on fences[0] is sufficient.
            ret = NvGr2dCopyBuffer(m, h, h->hShadow,
                                   &fences[0], // Src write
                                   NULL, 0, // Dst reads
                                   NULL); // CPU wait

            if (ret != 0) {
                pthread_mutex_unlock(&h->MapMutex);
                ReleaseBufferLock(h);
                return -ENOMEM;
            }

            // Since we wrote to the buffer with hardware above,
            // always invalidate the cache here.  Note that
            // writeback+invalidate is faster than invalidate
            // alone, and writeback is a nop because the cache
            // entries should be clean.
            NvRmMemCacheMaint(h->hShadow->Surf[0].hMem,
                              h->hShadow->Pixels,
                              h->hShadow->Buf->SurfSize, NV_TRUE,
                              NV_TRUE);

            // WritebackInvalidate ops can be deferred in kernel.
            // Here we invalidate one or two lines of cache (A15)
            // to flush all pending cache maintenance operations
            // for the handle.
            NvRmMemCacheMaint(h->hShadow->Surf[0].hMem,
                              h->hShadow->Pixels,
                              min(CACHE_LINE_SIZE,
                                  h->hShadow->Buf->SurfSize),
                              NV_FALSE, NV_TRUE);
        }
    }
    else
    {
        // CPU wait for fences
        while (numFences--) {
            NvRmFence* f = &fences[numFences];
            if (f->SyncPointID != NVRM_INVALID_SYNCPOINT_ID) {
                NVGRD(("NvGrLock [0x%08x]: CPU waiting for ID %d value %d",
                       h->MemId, f->SyncPointID, f->Value));
                NvRmFenceWait(m->Rm, f, NV_WAIT_INFINITE);
            }
        }

        NV_ASSERT(h->Surf[0].Layout == NvRmSurfaceLayout_Pitch);

        /* Map if necessary. We keep the mapping open until the handle
         * is closed. */

        if (!h->Pixels)
        {
            NvError e;
            e = NvRmMemMap(h->Surf[0].hMem, 0, h->Buf->SurfSize,
                           NVOS_MEM_READ_WRITE, (void**)&h->Pixels);

            if (NV_SHOW_ERROR(e))
            {
                pthread_mutex_unlock(&h->MapMutex);
                ReleaseBufferLock(h);
                return -ENOMEM;
            }
        }

        if (h->Buf->Flags & NvGrBufferFlags_NeedsCacheMaint) {
            if ((usage & GRALLOC_USAGE_SW_READ_MASK) ||
                IsSubRect(h, l, t, width, height)) {
                NvRmMemCacheMaint(h->Surf[0].hMem,
                                  h->Pixels,
                                  h->Buf->SurfSize, NV_TRUE,
                                  NV_TRUE);
                // WritebackInvalidate ops can be deferred in kernel. Here we invalidate
                // one line of cache to flush all pending cache maintenance operations
                // for the handle.
                NvRmMemCacheMaint(h->Surf[0].hMem,
                                  h->Pixels,
                                  min(CACHE_LINE_SIZE, h->Buf->SurfSize), NV_FALSE,
                                  NV_TRUE);
            }
            // We only skip cache maintenance on a full buffer
            // write lock which always flushes and invalidates on
            // unlock, so this flag is no longer required even if
            // we skipped maintenance above.
            h->Buf->Flags &= ~NvGrBufferFlags_NeedsCacheMaint;
        }
    }

    pthread_mutex_unlock(&h->MapMutex);

    if (!NVGR_READONLY_USE(usage)) {
        // Save LockRect
        h->LockRect.left = l;
        h->LockRect.top = t;
        h->LockRect.right = l + width;
        h->LockRect.bottom = t + height;

        // Bump the write count
        h->Buf->WriteCount++;
    }

    if (h->hShadow) {
        // Avoid duplicate blits
        h->hShadow->Buf->WriteCount = h->Buf->WriteCount;
    }

    if (vaddr)
        *vaddr = h->hShadow ? h->hShadow->Pixels : h->Pixels;

    android_atomic_inc(&h->RefCount);

    return 0;
}

int NvGrLock(gralloc_module_t const* module,
             buffer_handle_t handle,
             int usage, int l, int t, int width, int height,
             void** vaddr)
{
    return NvGrLockAsync(module, handle, usage,
                         l, t, width, height,
                         vaddr, NVSYNC_INIT_INVALID);
}

int NvGrLockAsync(gralloc_module_t const* module,
                  buffer_handle_t handle,
                  int usage, int l, int t, int width, int height,
                  void** vaddr, int fd)
{
    NV_ATRACE_BEGIN(__func__);
    NvNativeHandle* h = (NvNativeHandle*)handle;
    int ret;


    if (h->Format == HAL_PIXEL_FORMAT_YCbCr_420_888) {
        ALOGE("%s: Attempting to lock buffer with format 0x%x",
              __func__, h->Format);
        NV_ATRACE_END();
        return -EINVAL;
    }


    ret = NvGrLockCommon(module, handle, usage, l, t, width, height, vaddr, fd);
    NV_ATRACE_END();
    return ret;
}

int NvGrLock_ycbcr(gralloc_module_t const* module,
                   buffer_handle_t handle, int usage,
                   int l, int t, int width, int height,
                   struct android_ycbcr *ycbcr)
{
    NV_ATRACE_BEGIN(__func__);
    NvGrModule*     m = (NvGrModule*)module;
    NvNativeHandle* h = (NvNativeHandle*)handle;
    int ret;

    if (h->Format != HAL_PIXEL_FORMAT_YCbCr_420_888) {
        ALOGE("%s: Attempting to lock buffer with format 0x%x",
              __func__, h->Format);
        NV_ATRACE_END();
        return -EINVAL;
    }

    ret = NvGrLockCommon(module, handle, usage, l, t, width, height, NULL,
                         NVSYNC_INIT_INVALID);

    if (ycbcr) {
        NvU8 *ptr;

        if (h->hShadow) {
            h = h->hShadow;
        }

        NV_ASSERT(h->Type == NV_NATIVE_BUFFER_YUV);
        NV_ASSERT(h->Surf[0].Layout == NvRmSurfaceLayout_Pitch);
        NV_ASSERT(h->Surf[0].ColorFormat == NvColorFormat_Y8);

        ptr = h->Pixels;

        if (h->SurfCount == 2) {
            NV_ASSERT(h->Surf[1].ColorFormat == NvColorFormat_U8_V8);

            ycbcr->y  = ptr + h->Surf[0].Offset;
            ycbcr->cb = ptr + h->Surf[1].Offset;
            ycbcr->cr = ptr + h->Surf[1].Offset+1;
            ycbcr->ystride = h->Surf[0].Pitch;
            ycbcr->cstride = h->Surf[1].Pitch;
            ycbcr->chroma_step = 2;
        } else {
            NV_ASSERT(h->SurfCount == 3);
            NV_ASSERT(h->Surf[1].Pitch == h->Surf[2].Pitch);

            ycbcr->y  = ptr + h->Surf[0].Offset;
            ycbcr->cb = ptr + h->Surf[1].Offset;
            ycbcr->cr = ptr + h->Surf[2].Offset;
            ycbcr->ystride = h->Surf[0].Pitch;
            ycbcr->cstride = h->Surf[1].Pitch;
            ycbcr->chroma_step = 1;
        }

        // reserved for future use, set to 0 by gralloc's (*lock_ycbcr)()
        memset(ycbcr->reserved, 0, sizeof(ycbcr->reserved));
    }

    NV_ATRACE_END();
    return ret;
}


int NvGrUnlock (gralloc_module_t const* module,
                buffer_handle_t handle)
{
    return NvGrUnlockAsync(module, handle, NULL);
}

int NvGrUnlockAsync (gralloc_module_t const* module,
                     buffer_handle_t handle,
                     int* outFd)
{
    NV_ATRACE_BEGIN(__func__);
    NvGrModule*     m = (NvGrModule*)module;
    NvNativeHandle* h = (NvNativeHandle*)handle;
    int ret;

    NV_ASSERT(h->Magic == NVGR_HANDLE_MAGIC);
    NV_ASSERT(h->Buf->Magic == NVGR_BUFFER_MAGIC);

    NVGRD(("NvGrUnlock [0x%08x]", h->MemId));

    if (outFd) {
        *outFd = NVSYNC_INIT_INVALID;
    }

    if (h->Buf->LockState & NVGR_SW_WRITE_LOCK_FLAG)
    {
        // Exclusive lock held
        if (h->hShadow && h->hShadow->Pixels)
        {
            NV_ASSERT(h->Format != h->ExtFormat ||
                      h->Surf[0].Layout != NvRmSurfaceLayout_Pitch);

            // Writeback the locked region and invalidate the cache
            NvRmMemCacheMaint(h->hShadow->Surf[0].hMem, h->hShadow->Pixels,
                              h->hShadow->Buf->SurfSize, NV_TRUE, NV_TRUE);

            // 2D copy
            // Src write fences: We did a CPU wait when locking, so we already
            // waited on all writes to the shadow buffer.
            // Dst read fences: Wait for any reads on the primary buffer.
            ret = NvGr2dCopyBuffer(m, h->hShadow, h,
                                   NULL,
                                   h->Buf->Fence + 1, NVGR_MAX_FENCES - 1,
                                   outFd);
            if (ret != 0) {
                ReleaseBufferLock(h);
                NvGrUnregisterBuffer(module, handle);
                NV_ATRACE_END();
                return ret;
            }

            // Avoid duplicate blits
            h->hShadow->Buf->WriteCount = h->Buf->WriteCount;
        }
        else if (h->Pixels)
        {
            /* Writeback the locked region and invalidate the cache */

            NvU32 i;
            NvU8 *start;
            NvU32 size;
            NvU32 y = h->LockRect.top;
            NvU32 height = h->LockRect.bottom - h->LockRect.top;

            #define CACHE_MAINT(surface, y, height)                             \
                start = h->Pixels + (surface)->Offset + (y * (surface)->Pitch); \
                size = height * (surface)->Pitch;                               \
                NvRmMemCacheMaint((surface)->hMem, start, size, NV_TRUE, NV_TRUE);

            CACHE_MAINT(&h->Surf[0], y, height);

            for (i=1; i<h->SurfCount; ++i)
            {
                NvRmSurface *surface = &h->Surf[i];

                int py = (y * surface->Height) / h->Surf[0].Height;
                int pheight = (height * surface->Height) / h->Surf[0].Height;

                CACHE_MAINT(surface, py, pheight);
            }

            #undef CACHE_MAINT
        }
    }

    ReleaseBufferLock(h);

    ret = NvGrUnregisterBuffer(module, handle);
    NV_ATRACE_END();
    return ret;
}

static void
NvGrAddFenceLocked(NvNativeHandle *h,
                   const NvRmFence* fencein)
{
    NvRmFence safef;

    if (fencein)
        safef = *fencein;
    else {
        safef.SyncPointID = NVRM_INVALID_SYNCPOINT_ID;
        safef.Value = 0;
    }

    NV_ASSERT(NvGrBufferIsValid(h));
    NV_ASSERT(h->Buf->LockState != 0);

    if (h->Buf->LockState & NVGR_WRITE_LOCK_FLAG)
    {
        // Exclusive lock held, can retire read fences
        int i;
        for (i = 1; i < NVGR_MAX_FENCES; i++)
        {
            NvRmFence* f = &h->Buf->Fence[i];
            f->SyncPointID = NVRM_INVALID_SYNCPOINT_ID;
        }
        // The write fence always goes in index 0
        h->Buf->Fence[0] = safef;
    }
    else if (safef.SyncPointID != NVRM_INVALID_SYNCPOINT_ID)
    {
        // Read fences start in index 1
        int i = 1;
        pthread_mutex_lock(&h->Buf->FenceMutex);
        for (; i < NVGR_MAX_FENCES; i++)
        {
            NvRmFence* f = &h->Buf->Fence[i];

            if ((f->SyncPointID == safef.SyncPointID) ||
                (f->SyncPointID == NVRM_INVALID_SYNCPOINT_ID))
            {
                *f = safef;
                break;
            }
        }
        pthread_mutex_unlock(&h->Buf->FenceMutex);
        if (i == NVGR_MAX_FENCES) {
            ALOGE("NvGrAddFence: array overflow, dropping fence %d",
                 safef.SyncPointID);
            NV_ASSERT(i != NVGR_MAX_FENCES);
        }
    }
}

static void
NvGrGetFencesLocked(NvNativeHandle *h,
                    NvRmFence* fences,
                    int* len)
{
    NV_ASSERT(NvGrBufferIsValid(h));
    NV_ASSERT(fences && len);
    NV_ASSERT(h->Buf->LockState != 0);

    if (h->Buf->LockState & NVGR_WRITE_LOCK_FLAG)
    {
        int i, wrote = 0;

        // Exclusive lock held
        // If there are read fences, the write fence can be ignored.
        // If there are no read fences, the previous write must be considered
        // to avoid out of order writes
        for (i = 1; i < NVGR_MAX_FENCES; i++)
        {
            NvRmFence* f = &h->Buf->Fence[i];
            if (f->SyncPointID == NVRM_INVALID_SYNCPOINT_ID)
                break;
            if (wrote == *len)
            {
                NV_ASSERT(!"Caller provided too short fence array");
                break;
            }
            fences[wrote++] = *f;
        }

        // No read fences, return the write fence.
        if (wrote == 0) {
            NvRmFence* f = &h->Buf->Fence[0];
            if (f->SyncPointID != NVRM_INVALID_SYNCPOINT_ID) {
                fences[wrote++] = *f;
            }
        }

        *len = wrote;
    }
    else
    {
        pthread_mutex_lock(&h->Buf->FenceMutex);
        if (h->Buf->Fence[0].SyncPointID != NVRM_INVALID_SYNCPOINT_ID)
        {
            NV_ASSERT(*len >= 1);
            // Only index 0 is a write fence.  The others are read
            // fences and other readers don't need to wait for them.
            fences[0] = h->Buf->Fence[0];
            *len = 1;
        }
        else
            *len = 0;
        pthread_mutex_unlock(&h->Buf->FenceMutex);
    }
}

void NvGrAddFence(NvNativeHandle *h, int usage,
                  const NvRmFence* fence)
{
    AcquireBufferLock(h, usage);
    NvGrAddFenceLocked(h, fence);
    ReleaseBufferLock(h);
}

void NvGrGetFences(NvNativeHandle *h, int usage,
                   NvRmFence* fences, int* numFences)
{
    AcquireBufferLock(h, usage);
    NvGrGetFencesLocked(h, fences, numFences);
    ReleaseBufferLock(h);
}

int NvGrGetFenceFd(NvNativeHandle *h, int usage)
{
    NvRmFence fences[NVGR_MAX_FENCES];
    int numFences = NVGR_MAX_FENCES;
    int fenceFd = -1;

    AcquireBufferLock(h, usage);

    NvGrGetFencesLocked(h, fences, &numFences);

    if (numFences) {
        fenceFd = nvsync_from_fence("gralloc::get_fence", fences, numFences);
    }

    /* Clear existing fences */
    if (!NVGR_READONLY_USE(usage)) {
        NvGrAddFenceLocked(h, NULL);
    }

    ReleaseBufferLock(h);

    return fenceFd;
}

void NvGrAddFenceFd(NvNativeHandle *h, int usage, int fenceFd)
{
    NvRmFence fences[NVGR_MAX_FENCES];
    NvU32 numFences = NVGR_MAX_FENCES;

    if (fenceFd < 0)
        return;

    AcquireBufferLock(h, usage);

    nvsync_to_fence(fenceFd, fences, &numFences);
    nvsync_close(fenceFd);

    while (numFences) {
        NvGrAddFenceLocked(h, &fences[--numFences]);
    }

    ReleaseBufferLock(h);
}

/* WAR bug 827707.  If the buffer is locked for texture and the crop
 * is smaller than the texture, we need to replicate the edge texels
 * to avoid filtering artifacts.
 */
void NvGrGetSourceCrop (gralloc_module_t const* module,
                        buffer_handle_t handle,
                        NvRect *cropRect)
{
    NvNativeHandle* h = (NvNativeHandle*)handle;

    NV_ASSERT(h->Magic == NVGR_HANDLE_MAGIC);
    NV_ASSERT(h->Buf->Magic == NVGR_BUFFER_MAGIC);

    if (h->Buf->Flags & NvGrBufferFlags_SourceCropValid) {
        *cropRect = h->Buf->SourceCrop;
    } else {
        cropRect->left = 0;
        cropRect->top = 0;
        cropRect->right = h->Surf[0].Width;
        cropRect->bottom = h->Surf[0].Height;
    }
}

void NvGrSetSourceCrop (gralloc_module_t const* module,
                        buffer_handle_t handle,
                        NvRect *cropRect)
{
    NvNativeHandle* h = (NvNativeHandle*)handle;

    NV_ASSERT(h->Magic == NVGR_HANDLE_MAGIC);
    NV_ASSERT(h->Buf->Magic == NVGR_BUFFER_MAGIC);

    if (cropRect->right <= cropRect->left ||
        cropRect->bottom <= cropRect->top) {
        h->Buf->Flags &= ~NvGrBufferFlags_SourceCropValid;
        return;
    }

    h->Buf->SourceCrop = *cropRect;
    h->Buf->Flags |= NvGrBufferFlags_SourceCropValid;
}

void NvGrDumpBuffer(gralloc_module_t const *module,
                    buffer_handle_t handle,
                    const char *filename)
{
    NvError err;
    NvNativeHandle *h;
    int result;
    void *vaddr;

    h = (NvNativeHandle *) handle;

    result = NvGrLock(module, handle, GRALLOC_USAGE_SW_READ_RARELY,
                      0, 0, h->Surf[0].Width, h->Surf[0].Height, &vaddr);

    if (result == 0)
    {
        err = NvRmSurfaceDump(h->Surf, h->SurfCount, filename);

        NvGrUnlock(module, handle);
    }
}
