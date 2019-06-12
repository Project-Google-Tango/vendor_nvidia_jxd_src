/*
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDE_NVGRBUFFER_H
#define INCLUDE_NVGRBUFFER_H

#include <cutils/atomic.h>

#if NV_DEBUG
#include <assert.h>
#define NVGR_ASSERT(x) assert(x)
#else
#define NVGR_ASSERT(x)
#endif

#define NVGR_HANDLE_MAGIC 0xDAFFCAFF
#define NVGR_BUFFER_MAGIC 0xB00BD00D
#define NVGR_WRITE_LOCK_FLAG 0x80000000
#define NVGR_SW_WRITE_LOCK_FLAG 0x40000000
#define NVGR_MAX_FENCES 6
#define NVGR_MAX_SURFACES 3

typedef enum {
    NvGrBufferFlags_Posted          = 1 << 0,
    NvGrBufferFlags_SourceCropValid = 1 << 1,
    NvGrBufferFlags_NeedsCacheMaint = 1 << 2,
} NvGrBufferFlags;

//
// The buffer data stored kernel-side. This can be accessed
// via the .BufMem member of NvNativeHandle and it is mapped
// into using processes to the address pointed to by .Buffer.
//

struct NvGrBufferRec {
    NvU32               Magic;
    NvU32               SurfSize;
    NvU32               Flags;
    NvRmFence           Fence[NVGR_MAX_FENCES];
    pthread_mutex_t     FenceMutex;
    NvU32               LockState;
    pthread_mutex_t     LockMutex;
    pthread_cond_t      LockExclusive;
    // Check Stereo Layout details above
    NvU32               StereoInfo;
    // Video buffer timestamp in the unit of micro-second
    NvS64               TimeStamp;
    // WAR for bug 827707
    NvRect              SourceCrop;
    int32_t             WriteCount;
    int32_t             Compressed;
};

typedef enum NvNativeBufferTypeEnum
{
    NV_NATIVE_BUFFER_SINGLE,
    NV_NATIVE_BUFFER_STEREO,
    NV_NATIVE_BUFFER_YUV
} NvNativeBufferType;

//
// The NVIDIA native buffer type
//

struct NvNativeHandleRec {
    native_handle_t     Base;

    // shared memory handle
    int                 MemId;
    int                 SurfMemFd;

    // immutable global data
    NvU32               Magic;
    pid_t               Owner;
    NvNativeHandle     *hSelf;

    // per process data
    NvNativeBufferType  Type;
    NvU32               SurfCount;
    NvRmSurface         Surf[NVGR_MAX_SURFACES];
    NvGrBuffer*         Buf;
    NvU8*               Pixels;
    NvRect              LockRect;
    int                 RefCount;
    int                 Usage;
    int                 Format;
    int                 ExtFormat;
    int                 DeleteHandle;
    NvS32               SequenceNum;
    pthread_mutex_t     MapMutex;
    void*               GpuMapping;
    NvGrUnmapCallback   GpuUnmapCallback;
    int                 DecompressFenceFd;

    // shadow buffer, used for improving lock/unlock performance.
    // this is lazily allocated
    NvNativeHandle     *hShadow;
    NvU32               LockedPitchInPixels;
};

/* Returns all bits that indicate read/write access */
#define NVGR_USAGE_BITS(u) \
    (u & (GRALLOC_USAGE_SW_READ_MASK | \
          GRALLOC_USAGE_SW_WRITE_MASK | \
          GRALLOC_USAGE_HW_MASK))

#define NVGR_SW_USE(u) \
    (NVGR_USAGE_BITS(u) & \
           (GRALLOC_USAGE_SW_READ_MASK | \
            GRALLOC_USAGE_SW_WRITE_MASK))

#define NVGR_READONLY_USE(u) \
    ((NVGR_USAGE_BITS(u) & \
          ~(GRALLOC_USAGE_SW_READ_MASK | \
            GRALLOC_USAGE_HW_FB | \
            GRALLOC_USAGE_HW_CAMERA_READ | \
            GRALLOC_USAGE_HW_TEXTURE)) == 0)

#define NVGR_WRITEONLY_USE(u) \
    ((NVGR_USAGE_BITS(u) & \
          ~(GRALLOC_USAGE_SW_WRITE_MASK | \
            GRALLOC_USAGE_HW_CAMERA_WRITE | \
            GRALLOC_USAGE_HW_RENDER)) == 0)


/* Android public function */

int NvGrRegisterBuffer   (gralloc_module_t const* module,
                          buffer_handle_t handle);
int NvGrUnregisterBuffer (gralloc_module_t const* module,
                          buffer_handle_t handle);
int NvGrLock             (gralloc_module_t const* module,
                          buffer_handle_t handle,
                          int usage, int l, int t, int w, int h,
                          void** vaddr);
int NvGrLock_ycbcr       (gralloc_module_t const* module,
                          buffer_handle_t handle, int usage,
                          int l, int t, int w, int h,
                          struct android_ycbcr *ycbcr);
int NvGrUnlock           (gralloc_module_t const* module,
                          buffer_handle_t handle);
int NvGrLockAsync        (gralloc_module_t const* module,
                          buffer_handle_t handle,
                          int usage, int l, int t, int w, int h,
                          void** vaddr, int fd);
int NvGrUnlockAsync      (gralloc_module_t const* module,
                          buffer_handle_t handle,
                          int* fd);

/* Private functions */

void NvGrAddFence        (NvNativeHandle *h, int usage,
                          const NvRmFence* fence);
void NvGrGetFences       (NvNativeHandle *h, int usage,
                          NvRmFence* fences, int* numFences);
int NvGrGetFenceFd       (NvNativeHandle *h, int usage);
void NvGrAddFenceFd      (NvNativeHandle *h, int usage, int fenceFd);
void NvGrGetSourceCrop   (gralloc_module_t const* module,
                          buffer_handle_t handle,
                          NvRect *cropRect);
void NvGrSetSourceCrop   (gralloc_module_t const* module,
                          buffer_handle_t handle,
                          NvRect *cropRect);
void NvGrDumpBuffer      (gralloc_module_t const *module,
                          buffer_handle_t handle,
                          const char *filename);
int NvGrShouldDecompress (gralloc_module_t const *module,
                          buffer_handle_t handle);
void NvGrSetCompressed   (gralloc_module_t const *module,
                          buffer_handle_t handle,
                          int compressed);
int NvGrDecompressBuffer (struct NvGrModuleRec *m,
                          NvNativeHandle *h,
                          int fenceIn, int *fenceOut);
int NvGrSetGpuMapping    (struct NvGrModuleRec *m,
                          NvNativeHandle *h,
                          void *mapping,
                          NvGrUnmapCallback unmap);
void* NvGrGetGpuMapping  (struct NvGrModuleRec *m,
                          NvNativeHandle *h);

/* Buffer methods */

static inline NvBool
NvGrBufferIsValid(NvNativeHandle *h)
{
    return h && h->Buf &&
        h->Magic == NVGR_HANDLE_MAGIC &&
        h->Buf->Magic == NVGR_BUFFER_MAGIC;
}

static inline NvBool
NvGrUseShadow(NvNativeHandle *h)
{
    return h->Format != h->ExtFormat ||
        h->Surf[0].Layout != NvRmSurfaceLayout_Pitch;
}

static inline int32_t
NvGrGetWriteCount(NvNativeHandle *h)
{
    NVGR_ASSERT(NvGrBufferIsValid(h));

    return android_atomic_acquire_load(&h->Buf->WriteCount);
}

static inline void
NvGrIncWriteCount(NvNativeHandle *h)
{
    NVGR_ASSERT(NvGrBufferIsValid(h));

    if (!NvGrUseShadow(h)) {
        // Cache entries may be stale following a hardware write.
        // Flag the buffer for cache maintenance on the next CPU read
        // or preserve lock.
        h->Buf->Flags |= NvGrBufferFlags_NeedsCacheMaint;
    }
    android_atomic_inc(&h->Buf->WriteCount);
}

static inline NvU32
NvGrGetStereoInfo(NvNativeHandle *h)
{
    NVGR_ASSERT(NvGrBufferIsValid(h));

    return h->Buf->StereoInfo;
}

static inline void
NvGrSetStereoInfo(NvNativeHandle *h, NvU32 stereoInfo)
{
    NVGR_ASSERT(NvGrBufferIsValid(h));

    h->Buf->StereoInfo = stereoInfo;
}

static inline void
NvGrSetVideoTimeStamp(NvNativeHandle *h, NvS64 timeStamp)
{
    NVGR_ASSERT(NvGrBufferIsValid(h));

    h->Buf->TimeStamp = timeStamp;
}


#endif /* INCLUDE_NVGRBUFFER_H */
