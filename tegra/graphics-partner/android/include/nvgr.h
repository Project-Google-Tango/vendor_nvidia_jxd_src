/*
 * Copyright (c) 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef _NVGRAPI_H
#define _NVGRAPI_H

#include <hardware/gralloc.h>
#include "nvcommon.h"
#include "nvrm_channel.h"

#ifdef __cplusplus
extern "C" {
#endif

// NVIDIA private flags

#define NVGR_USAGE_STEREO          GRALLOC_USAGE_PRIVATE_0

// If NVGR_USAGE_UNCOMPRESSED is passed when locking a buffer, gralloc
// guarantees that the buffer will not be compressed when the buffer's
// write fence expires. All hw clients that don't understand compression
// should pass the flag when locking.
// If NVGR_USAGE_UNCOMPRESSED is passed when allocating a buffer, gralloc
// will not make the buffer compressible.
#define NVGR_USAGE_UNCOMPRESSED    GRALLOC_USAGE_PRIVATE_1

// Unused: GRALLOC_USAGE_PRIVATE_2
// Unused: GRALLOC_USAGE_PRIVATE_3


// NVIDIA private formats (0x100 - 0x1ff)
//
// Tokens which have two formats use the first format internally and
// the second format externally when locked for CPU access.
// Undefined external format means that we do not do any layout
// conversion and the internal format (subject to layout changes)
// is exposed as such. Bug 1060033.

// Internal formats (0x100 - 0x13F)               internal / external
#define NVGR_PIXEL_FORMAT_YUV420        0x100  /* planar YUV420 / YV12 */
#define NVGR_PIXEL_FORMAT_YUV422        0x101  /* planar YUV422 / undefined */
#define NVGR_PIXEL_FORMAT_YUV422R       0x102  /* planar YUV422R / undefined */
#define NVGR_PIXEL_FORMAT_UYVY          0x103  /* interleaved YUV422 */
#define NVGR_PIXEL_FORMAT_YUV420_NV12   0x104  /* YUV420 / NV12 */
#define NVGR_PIXEL_FORMAT_NV12          0x106  /* NV12 */
#define NVGR_PIXEL_FORMAT_NV16          0x107  /* NV16 */
#define NVGR_PIXEL_FORMAT_ABGR_8888     0x108  /* NvColorFormat_R8G8B8A8 / undefined */

// Gralloc private formats (0x140 - 0x17F)
#define NVGR_PIXEL_FORMAT_YV12_EXTERNAL       0x140  /* YV12 external */
#define NVGR_PIXEL_FORMAT_NV12_EXTERNAL       0x141  /* NV12 external */
#define NVGR_PIXEL_FORMAT_NV21_EXTERNAL       0x143  /* NV21 external */
#define NVGR_PIXEL_FORMAT_NV16_EXTERNAL       0x144  /* NV16 external */

// The tiled bit modifies other formats
#define NVGR_PIXEL_FORMAT_TILED         0x080  /* tiled bit */

#define NVGR_SWAP(a, b) {                       \
    (a) ^= (b);                                 \
    (b) ^= (a);                                 \
    (a) ^= (b);                                 \
}
#define NVGR_SWAPF(a, b) {                      \
    float x = a;                                \
    a = b;                                      \
    b = x;                                      \
}

// Convert the NvColorFormat to the equivalent TEGRA_FB format as used by
// gralloc (see format list in tegrafb.h). Returns -1 if the format is not
// supported by gralloc.
// XXX - this is used only by nvwsi and should probably go away.
#include <linux/tegrafb.h>
#include <nvcolor.h>
#include <nvrm_surface.h>
static inline int NvGrTranslateFmt (NvColorFormat c)
{
    switch (c) {
    case NvColorFormat_A8B8G8R8:
    case NvColorFormat_X8B8G8R8:
        return TEGRA_FB_WIN_FMT_R8G8B8A8;
    case NvColorFormat_R5G6B5:
        return TEGRA_FB_WIN_FMT_B5G6R5;
    case NvColorFormat_A8R8G8B8:
        return TEGRA_FB_WIN_FMT_B8G8R8A8;
    case NvColorFormat_R5G5B5A1:
        return TEGRA_FB_WIN_FMT_AB5G5R5;
    case NvColorFormat_Y8: /* we expect this to be the first plane of planar YCbCr set */
        return TEGRA_FB_WIN_FMT_YCbCr420P;
    default:
        return -1;
    }
}

typedef void (*NvGrUnmapCallback)(void *);

int nvgr_ref(buffer_handle_t handle);
int nvgr_unref(buffer_handle_t handle);

int nvgr_is_valid(buffer_handle_t handle);
int nvgr_is_yuv(buffer_handle_t handle);
int nvgr_is_stereo(buffer_handle_t handle);

int nvgr_get_format(buffer_handle_t handle);
int nvgr_get_usage(buffer_handle_t handle);
int nvgr_get_memfd(buffer_handle_t handle);
void nvgr_get_surfaces(buffer_handle_t handle, const NvRmSurface **surfs,
                       size_t *surfCount);

NvU32 nvgr_get_write_count(buffer_handle_t handle);
void nvgr_inc_write_count(buffer_handle_t handle);

NvU32 nvgr_get_stereo_info(buffer_handle_t handle);
void nvgr_set_stereo_info(buffer_handle_t handle, NvU32 stereoInfo);

void nvgr_set_video_timestamp(buffer_handle_t handle, NvS64 timeStamp);

void nvgr_get_crop(buffer_handle_t handle, NvRect *crop);
void nvgr_set_crop(buffer_handle_t handle, NvRect *crop);

int nvgr_buffers_are_equal(buffer_handle_t handle0, buffer_handle_t handle1);

int nvgr_decompress(buffer_handle_t handle, int fenceIn, int *fenceOut);
int nvgr_should_decompress(buffer_handle_t handle);
void nvgr_set_compressed(buffer_handle_t handle, int compressed);

void *nvgr_get_gpu_mapping(buffer_handle_t handle);
int nvgr_set_gpu_mapping(buffer_handle_t handle, void *mapping,
                         NvGrUnmapCallback unmap);

int nvgr_get_fence(buffer_handle_t handle, int *fenceFd);
int nvgr_put_read_fence(buffer_handle_t handle, int fenceFd);
int nvgr_put_read_nvfence(buffer_handle_t handle, const NvRmFence *fence);
int nvgr_put_write_fence(buffer_handle_t handle, int fenceFd);
int nvgr_put_write_nvfence(buffer_handle_t handle, const NvRmFence *fence);

int nvgr_scratch_alloc(int width, int height, int format, int usage,
                       NvRmSurfaceLayout layout, buffer_handle_t *handle);
void nvgr_scratch_free(buffer_handle_t handle);

int nvgr_clear(buffer_handle_t handle, NvRect *rect, NvU32 color,
               int inFence, int *outFence);

int nvgr_blit(buffer_handle_t srcHandle, NvRect *srcRect, int srcFence,
              buffer_handle_t dstHandle, NvRect *dstRect, int dstFence,
              int transform, int *outFence);

void nvgr_dump(buffer_handle_t handle, const char *filename);

// override a property. Use value=NULL to reset override
// returns 0 in case of success, non-zero in case of failure
int nvgr_override_property(const char *propertyName, const char *value);

// reads an event counter value
// returns 0 in case of success, non-zero in case of failure
int nvgr_read_event_counter(int eventCounterId, NvU32 *value);

/* Deprecated */
static inline int nvgr_copy_buffer(buffer_handle_t srcHandle, int srcFence,
                                   buffer_handle_t dstHandle, int dstFence,
                                   int *outFence)
{
    return nvgr_blit(srcHandle, NULL, srcFence, dstHandle, NULL, dstFence,
                     0, outFence);
}

// Temporary hacks for OMX video editor use case.  See http://nvbugs/1404155

size_t nvgr_get_handle_size(void);

void nvgr_get_surfaces_no_buffer_magic(buffer_handle_t handle,
                                       const NvRmSurface **surfs,
                                       size_t *surfCount);

void nvgr_sync_dump(void);

#ifdef __cplusplus
}
#endif

#endif /* ifndef _NVGRAPI_H */
