/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <math.h>
#include <errno.h>
#include <unistd.h>
#include <cutils/ashmem.h>
#include <cutils/atomic.h>
#include <cutils/log.h>
#include <sys/mman.h>

#include "nvgralloc.h"
#include "nvgrsurfutil.h"
#include "nvrm_chip.h"

// Google says we must clear the surface for security - bug 3131591
#define NVGR_CLEAR_SURFACES 1

static NvNativeHandle*
CreateNativeHandle (int MemId)
{
    NvNativeHandle* h = NvOsAlloc(sizeof(NvNativeHandle));
    if (!h) return NULL;
    NvOsMemset(h, 0, sizeof(NvNativeHandle));

    h->Base.version = sizeof(native_handle_t);
    h->Base.numFds = 2;
    h->Base.numInts = (sizeof(NvNativeHandle) -
                       sizeof(native_handle_t) -
                       h->Base.numFds*sizeof(int)) >> 2;

    h->Magic = NVGR_HANDLE_MAGIC;
    h->Owner = getpid();
    h->MemId = MemId;

    return h;
}

static NvError
SharedAlloc (NvU32 Size, int* FdOut, void** MapOut)
{
    int fd;
    void* addr;

    Size = ROUND_TO_PAGE(Size);

    fd = ashmem_create_region("nvgralloc-shared", Size);
    if (fd < 0)
    {
        return NvError_InsufficientMemory;
    }

    addr = mmap(0, Size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
    {
        close(fd);
        return NvError_InsufficientMemory;
    }

    *FdOut = fd;
    *MapOut = addr;

    return NvSuccess;
}

static NvError
RmAlloc (NvRmDeviceHandle Rm,
         const NvRmSurface *Surface,
         NvU32 Align,
         NvU32 Size,
         int Usage,
         NvRmMemHandle* MemOut)
{
    NvError e;
    NvRmMemHandle Mem;
    NvRmHeap HeapArr[3];
    NvU32 NumHeaps = 0;
    NvOsMemAttribute Attr;
    NvRmMemKind Kind = NvRmMemKind_Pitch;
    int SwRead = Usage & GRALLOC_USAGE_SW_READ_MASK;
    int SwWrite = Usage & GRALLOC_USAGE_SW_WRITE_MASK;
    NvBool hasVPR = NV_FALSE;
    NVRM_DEFINE_MEM_HANDLE_ATTR(HandleAttr);

    NV_ASSERT(MemOut);
    NvRmChipGetCapabilityBool(NvRmChipCapability_System_VPR, &hasVPR);

    if (hasVPR && (Usage & GRALLOC_USAGE_PROTECTED)) {
        NV_ASSERT(!(SwRead || SwWrite));
        HeapArr[NumHeaps++] = NvRmHeap_VPR;
    } else {
        /* Disallow GART entirely for gralloc buffers as these are
         * typically shared between processes, and can defeat the
         * per-process accounting which tries to prevent over-commiting.
         */
        HeapArr[NumHeaps++] = NvRmHeap_ExternalCarveOut;
        HeapArr[NumHeaps++] = NvRmHeap_External;
    }

    if (!(SwRead || SwWrite)) {
        Attr = NvOsMemAttribute_Uncached;
    } else if (SwRead == GRALLOC_USAGE_SW_READ_OFTEN) {
        Attr = NvOsMemAttribute_WriteBack;
    } else {
        Attr = NvOsMemAttribute_InnerWriteBack;
    }

    if (Surface->Layout == NvRmSurfaceLayout_Blocklinear)
    {
        // We should never access nonlinear buffers with CPU (apart from
        // NvRmSurfaceRead/Writes from test/dump code), so no point in making
        // them CPU-cacheable.
        Attr = NvOsMemAttribute_Uncached;
    }

    NVRM_MEM_HANDLE_SET_ATTR(HandleAttr,
                             Align,
                             Attr,
                             Size,
                             NVRM_MEM_TAG_GRALLOC_MISC);
    NVRM_MEM_HANDLE_SET_HEAP_ATTR(HandleAttr, HeapArr, NumHeaps);
    NVRM_MEM_HANDLE_SET_KIND_ATTR(HandleAttr, Surface->Kind);

    e = NvRmMemHandleAllocAttr(Rm, &HandleAttr, &Mem);

    if (NV_SHOW_ERROR(e))
    {
        NvRmMemHandleFree(Mem);
        return e;
    }

    *MemOut = Mem;
    return NvSuccess;
}

// here is cache line size we are optimizing our
// cache maintenance for
#define CACHE_LINE_SIZE 64

static NV_INLINE void NvGrClearSurface(NvGrModule *ctx, NvNativeHandle *h)
{
#if NVGR_CLEAR_SURFACES
    {
        // flush any pending CPU writes to the surface before clearing it with 2D.
        // In order to do cache maintenance on the handle, we need to map it.
        NvRmMemHandle nvrmMem = h->Surf[0].hMem;
        NvU32 size = NvRmMemGetSize(nvrmMem);
        void *virtAddr = NULL;
        NvError err = NvRmMemMap(nvrmMem, 0, size, NVOS_MEM_READ_WRITE, &virtAddr);
        if (err == NvSuccess) {
            // Invalidate the cache to clear pending writebacks.
            NvRmMemCacheMaint(nvrmMem, virtAddr, size , NV_TRUE, NV_TRUE);
            // WritebackInvalidate ops can be deferred in kernel. Here we
            // invalidate one line of cache to flush pending cache maintenance
            // operations for the handle.
            NvRmMemCacheMaint(nvrmMem, virtAddr, NV_MIN(CACHE_LINE_SIZE, size), NV_FALSE, NV_TRUE);
            NvRmMemUnmap(nvrmMem, virtAddr, size);
        } else {
            ALOGE("%s: failed to map memory for cache maintenance (%d)", __func__, err);
        }
    }

    NvError err = NvGr2DClearImplicit(ctx, h);

    if (NvSuccess != err) {
        NvU8* scanline = NvOsAlloc(h->Surf[0].Pitch);
        NvU32 ii;

        ALOGE("NvGr2DClear failed (%d), falling back to surface write", err);

        if (scanline) {
            // clear to black
            NvOsMemset(scanline, 0, h->Surf[0].Pitch);

            for (ii = 0; ii < h->SurfCount; ii++) {
                NvRmSurface *s = h->Surf + ii;

                // ensure the alloc is big enough
                NV_ASSERT(h->Surf[ii].Pitch <= h->Surf[0].Pitch);

                // For a YUV surface, the UV planes must be 0x7f
                if (ii == 1 && h->Type == NV_NATIVE_BUFFER_YUV) {
                    NvOsMemset(scanline, 0x7f, h->Surf[ii].Pitch);
                }

                NvRmMemWriteStrided(s->hMem, s->Offset, s->Pitch,
                                    scanline, 0, s->Pitch, s->Height);
            }

            NvOsFree(scanline);
        } else {
            // slower fallback
            for (ii = 0; ii < h->SurfCount; ii++) {
                NvRmSurface *s = h->Surf + ii;
                NvU32 size, val;

                // Clear to black.  For a YUV surface, the UV planes must be 127
                if (ii && h->Type == NV_NATIVE_BUFFER_YUV) {
                    val = 0x7f7f7f7f;
                } else {
                    val = 0;
                }

                size = s->Width * s->Height * (NV_COLOR_GET_BPP(s->ColorFormat)>>3);
                NV_ASSERT(s->Offset + size <= h->Buf->SurfSize);

                NvRmMemWriteStrided(s->hMem, s->Offset, sizeof(val),
                                    &val, 0, sizeof(val), size/sizeof(val));
            }
        }
    }
#endif
}

static int
GetShadowBufferStrideInPixels(NvGrModule* ctx, int external_format, int width,
                              int height)
{
    NvRmSurface s;
    NvColorFormat nvFormat;
    NvU32 attrs[] = { NvRmSurfaceAttribute_Layout, NvRmSurfaceLayout_Pitch,
                      NvRmSurfaceAttribute_None };

    switch (external_format) {
        case NVGR_PIXEL_FORMAT_YV12_EXTERNAL:
            // Three planes, the 2nd and 3rd plane must have exactly
            // half the pitch as the first one. Thus we compute the pitch
            // for the smaller planes and multiply it by 2 to ensure
            // sufficient alignment.
            NvRmSurfaceSetup(&s, width / 2, width / 2, NvColorFormat_Y8, attrs);
            return NvGrAlignUp(s.Pitch * 2, 16);
        case NVGR_PIXEL_FORMAT_NV12_EXTERNAL:
        case NVGR_PIXEL_FORMAT_NV21_EXTERNAL:
        case NVGR_PIXEL_FORMAT_NV16_EXTERNAL:
            // Two planes, both have the same pitch
            NvRmSurfaceSetup(&s, width, width, NvColorFormat_Y8, attrs);
            return NvGrAlignUp(s.Pitch, 16);
        default:
            nvFormat = NvGrGetNvFormat(external_format);
            if (nvFormat == NvColorFormat_Unspecified) {
                ALOGE("Locking for SW use not supported (unknown shadow format)");
                return 0;
            }
            NvRmSurfaceSetup(&s, width, height, nvFormat, attrs);
            return s.Pitch / (NV_COLOR_GET_BPP(s.ColorFormat) >> 3);
    }
}

static int NvGrAllocExt(NvGrAllocDev* dev, NvGrAllocParameters *params,
                        NvNativeHandle** handle)
{
    NvGrModule* ctx = (NvGrModule*) dev->Base.common.module;
    int ret;
    NvNativeHandle *h;
    int width = params->Width;
    int height = params->Height;
    NvRmSurfaceLayout layout = params->Layout;
    int usage = params->Usage;
    int format = params->Format;
    int external_format = format;
    int internal_usage = usage;
    NvBool supportSemiPlanar;

    // Remap formats
    switch (format) {
    case HAL_PIXEL_FORMAT_YV12:
    case NVGR_PIXEL_FORMAT_YUV420:
        format = NVGR_PIXEL_FORMAT_YUV420;
        external_format = NVGR_PIXEL_FORMAT_YV12_EXTERNAL;
        break;
    case NVGR_PIXEL_FORMAT_YUV420_NV12:
        format = NVGR_PIXEL_FORMAT_YUV420;
        external_format = NVGR_PIXEL_FORMAT_NV12_EXTERNAL;
        break;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        external_format = NVGR_PIXEL_FORMAT_NV21_EXTERNAL;
        break;
    case NVGR_PIXEL_FORMAT_NV12:
        external_format = NVGR_PIXEL_FORMAT_NV12_EXTERNAL;
        break;
    case NVGR_PIXEL_FORMAT_NV16:
        external_format = NVGR_PIXEL_FORMAT_NV16_EXTERNAL;
        break;
    default:
        break;
    }

    // TODO We use the block linear capability to allow semi-planar only
    //      from T124 onwards. Could we support semi-planar on older chips
    //      too?
    NvRmChipGetCapabilityBool(NvRmChipCapability_System_BlocklinearLayout,
                              &supportSemiPlanar);
    if (!supportSemiPlanar) {
        // When the chip doesn't support semi-planar natively, we allocate
        // the internal buffer as planar.
        switch (format) {
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        case NVGR_PIXEL_FORMAT_NV12:
            format = NVGR_PIXEL_FORMAT_YUV420;
            break;
        case NVGR_PIXEL_FORMAT_NV16:
            format = NVGR_PIXEL_FORMAT_YUV422;
            break;
        default:
            break;
        }
    }

    if (format != external_format || layout != NvRmSurfaceLayout_Pitch) {
        // The internal buffer will never be mapped for CPU
        internal_usage &= ~(GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK);
    }

    ret = NvGrAllocInternal(ctx, width, height, format, internal_usage, layout, &h);
    if (ret != 0) {
        return ret;
    }

    h->Usage     = usage;
    h->ExtFormat = external_format;

    if (format == HAL_PIXEL_FORMAT_YCbCr_420_888) {
        // If format is HAL_PIXEL_FORMAT_YCbCr_420_888, the returned
        // stride must be 0, since the actual strides are available
        // from the android_ycbcr structure.
        h->LockedPitchInPixels = 0;
    } else if (format != external_format || layout != NvRmSurfaceLayout_Pitch) {
        // These formats will be copied to a shadow buffer on Lock, so
        // return the stride of the shadow buffer.
        h->LockedPitchInPixels =
            GetShadowBufferStrideInPixels(ctx, external_format, width, height);
    } else {
        h->LockedPitchInPixels =
            (h->Surf[0].Pitch / (NV_COLOR_GET_BPP(h->Surf[0].ColorFormat) >> 3));
    }

    // Clear surface
    NvGrClearSurface(ctx, h);

    *handle = h;

    NVGRD(("NvGrAlloc [0x%08x]: Allocated %dx%d format %d usage %x stride %d",
           h->MemId, width, height, format, usage, h->LockedPitchInPixels));

    return 0;
}

static int NvGrAlloc (alloc_device_t* dev, int width, int height, int format,
                      int usage, buffer_handle_t* handle, int* stride)
{
    NvGrModule* ctx = (NvGrModule*)((NvGrAllocDev*)dev)->Base.common.module;
    int ret;
    NvNativeHandle *h;
    NvRmSurfaceLayout layout;
    NvGrAllocParameters params;
    NvBool bl;

    // System-wide default
    layout = NvRmSurfaceGetDefaultLayout();

    // If CPU access is needed often, default to pitchlinear to avoid conversion
    if ((usage & GRALLOC_USAGE_SW_READ_MASK) == GRALLOC_USAGE_SW_READ_OFTEN ||
        (usage & GRALLOC_USAGE_SW_WRITE_MASK) == GRALLOC_USAGE_SW_WRITE_OFTEN) {
        layout = NvRmSurfaceLayout_Pitch;
    }

    switch (format) {
    case HAL_PIXEL_FORMAT_RAW_SENSOR:
    case HAL_PIXEL_FORMAT_BLOB:
    case HAL_PIXEL_FORMAT_Y8:
    case HAL_PIXEL_FORMAT_Y16:
        layout = NvRmSurfaceLayout_Pitch;
        break;
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
        // This format must be accepted by the gralloc module when
        // USAGE_HW_CAMERA_WRITE and USAGE_SW_READ_* are set.
        if (!(usage & GRALLOC_USAGE_HW_CAMERA_WRITE) ||
            !(usage & GRALLOC_USAGE_SW_READ_MASK)) {
            return -EINVAL;
        }
        layout = NvRmSurfaceLayout_Pitch;
        break;
    default:
        break;
    }

    // Choose layout for vendor formats
    if (0x100 <= format && format <= 0x1FF) {
        // Bug 1060033 - We don't yet support shadow blits for YUV422(R).
        if (NVGR_SW_USE(usage) &&
                (format == NVGR_PIXEL_FORMAT_YUV422 ||
                 format == NVGR_PIXEL_FORMAT_YUV422R)) {
            layout = NvRmSurfaceLayout_Pitch;
        }

        // Vendor formats can be requested to be tiled
        if (format & NVGR_PIXEL_FORMAT_TILED) {
            layout = NvRmSurfaceLayout_Tiled;
            format &= ~NVGR_PIXEL_FORMAT_TILED;
        }

        // Force tiled for this format to maintain backward
        // compatibility until the tiled bit propogates to OMX
        if (format == NVGR_PIXEL_FORMAT_YUV420) {
            layout = NvRmSurfaceLayout_Tiled;
        }
    }

    // Tiled buffers should not be used as textures on < T114
    if (layout == NvRmSurfaceLayout_Tiled) {
        NvBool tiledLinearTextures;
        NvRmChipGetCapabilityBool(NvRmChipCapability_GPU_TiledLinearTextures,
                &tiledLinearTextures);
        if (!tiledLinearTextures) {
            usage &= ~GRALLOC_USAGE_HW_TEXTURE;
        }
    }

    // TODO Is it correct for Surfaceflinger to use flag HW_FB and composite?
    if (!ctx->GpuIsAurora &&
        (usage & (GRALLOC_USAGE_HW_FB |
                    GRALLOC_USAGE_HW_RENDER |
                    GRALLOC_USAGE_HW_TEXTURE))) {
        layout = NvRmSurfaceLayout_Blocklinear;
    }

    NvRmChipGetCapabilityBool(NvRmChipCapability_System_BlocklinearLayout, &bl);
    if (bl && (layout == NvRmSurfaceLayout_Tiled)) {
        layout = NvRmSurfaceLayout_Blocklinear;
    }

    params.Width = width;
    params.Height = height;
    params.Format = format;
    params.Usage = usage;
    params.Layout = layout;

    ret = NvGrAllocExt((NvGrAllocDev*)dev, &params, &h);
    if (ret != 0) {
        return ret;
    }

    // Note: Android expects stride in pixels, not bytes
    *stride = h->LockedPitchInPixels;
    *handle = &h->Base;
    return 0;
}

static NV_INLINE void
NvGrGetAllocParams (NvGrModule* ctx,
                    NvRmSurface* s,
                    NvU32* align,
                    NvU32* size,
                    int usage)
{
    // If we ever need to have different pitch/size/alignment
    // depending on which h/w units we want to use, we should
    // check the usage flag here and pass necessary flags to
    // the RM functions.
    (void) usage;

    // Pitch is already computed by NvRmSurfaceSetup.
    NV_ASSERT(s->Pitch);
    *align = NvRmSurfaceComputeAlignment(ctx->Rm, s);
    *size  = NvRmSurfaceComputeSize(s);
}

int NvGrAllocInternal (NvGrModule *Ctx, int width, int height, int format,
                       int usage, NvRmSurfaceLayout layout,
                       NvNativeHandle **handle)
{
    NvGrBuffer*         Obj;
    int                 ObjMem;
    NvRmSurface         Surf[NVGR_MAX_SURFACES];
    NvU32               SurfAlign;
    NvU32               SurfCount;
    NvNativeBufferType  BufferType;
    NvNativeHandle*     h = NULL;
    NvError             e;
    int                 i;
    pthread_mutexattr_t m_attr;
    pthread_condattr_t  c_attr;
    NvColorFormat       nvFormat[NVGR_MAX_SURFACES];
    NvU32               attrs[] = { NvRmSurfaceAttribute_Layout, layout,
                                    NvRmSurfaceAttribute_Compression, NV_FALSE,
                                    NvRmSurfaceAttribute_None };

    // At least Flatland uses GRALLOC_USAGE_HW_COMPOSER but not
    // GRALLOC_USAGE_HW_RENDER.
    if ((usage & (GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_COMPOSER)) &&
        !(usage & NVGR_USAGE_UNCOMPRESSED) &&
        (NvGrGetCompression(Ctx) == NvGrCompression_Enabled)) {

        attrs[3] = NV_TRUE;
    }

    // Check input

    if (width <= 0 || height <= 0)
    {
        NV_SHOW_ERROR(NvError_BadParameter);
        return -EINVAL;
    }

    if (NVGR_SW_USE(usage) && (usage & GRALLOC_USAGE_PROTECTED)) {
        NV_SHOW_ERROR(NvError_BadParameter);
        ALOGI("%s %d", __func__, __LINE__);
        return -EINVAL;
    }

    NvOsMemset(Surf, 0, sizeof(Surf));

    switch (format) {
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
    case NVGR_PIXEL_FORMAT_YUV420:
    case NVGR_PIXEL_FORMAT_YV12_EXTERNAL:
        // This format requires an even width and height
        if ((width | height) & 1)
        {
            NV_SHOW_ERROR(NvError_BadParameter);
            ALOGI("%s %d", __func__, __LINE__);
            return -EINVAL;
        }

        SurfCount = 3;
        nvFormat[0] = NvColorFormat_Y8;
        nvFormat[1] = NvColorFormat_U8;
        nvFormat[2] = NvColorFormat_V8;
        BufferType = NV_NATIVE_BUFFER_YUV;
        NvRmMultiplanarSurfaceSetup(Surf, SurfCount, width, height, NvYuvColorFormat_YUV420, nvFormat, attrs);
        break;

    case NVGR_PIXEL_FORMAT_YUV422:
        // This format requires an even width and height
        if ((width | height) & 1)
        {
            NV_SHOW_ERROR(NvError_BadParameter);
            ALOGI("%s %d", __func__, __LINE__);
            return -EINVAL;
        }

        SurfCount = 3;
        nvFormat[0] = NvColorFormat_Y8;
        nvFormat[1] = NvColorFormat_U8;
        nvFormat[2] = NvColorFormat_V8;
        BufferType = NV_NATIVE_BUFFER_YUV;
        NvRmMultiplanarSurfaceSetup(Surf, SurfCount, width, height, NvYuvColorFormat_YUV422, nvFormat, attrs);
        break;

    case NVGR_PIXEL_FORMAT_YUV422R:
        // This format requires an even width and height
        if ((width | height) & 1)
        {
            NV_SHOW_ERROR(NvError_BadParameter);
            ALOGI("%s %d", __func__, __LINE__);
            return -EINVAL;
        }

        SurfCount = 3;
        nvFormat[0] = NvColorFormat_Y8;
        nvFormat[1] = NvColorFormat_U8;
        nvFormat[2] = NvColorFormat_V8;
        BufferType = NV_NATIVE_BUFFER_YUV;
        NvRmMultiplanarSurfaceSetup(Surf, SurfCount, width, height, NvYuvColorFormat_YUV422R, nvFormat, attrs);
        break;

    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case NVGR_PIXEL_FORMAT_NV12:
    case NVGR_PIXEL_FORMAT_NV12_EXTERNAL:
    case NVGR_PIXEL_FORMAT_NV21_EXTERNAL:
        {
            NvColorFormat chromaFormat = NvColorFormat_U8_V8;
            if (format == HAL_PIXEL_FORMAT_YCrCb_420_SP ||
                    format == NVGR_PIXEL_FORMAT_NV21_EXTERNAL)
            {
                chromaFormat = NvColorFormat_V8_U8;
            }

            // This format requires an even width and height
            if ((width | height) & 1)
            {
                NV_SHOW_ERROR(NvError_BadParameter);
                ALOGI("%s %d", __func__, __LINE__);
                return -EINVAL;
            }

            SurfCount = 2;
            nvFormat[0] = NvColorFormat_Y8;
            nvFormat[1] = chromaFormat;
            BufferType = NV_NATIVE_BUFFER_YUV;
            NvRmMultiplanarSurfaceSetup(Surf, SurfCount, width, height, NvYuvColorFormat_YUV420, nvFormat, attrs);
        }
        break;

    case NVGR_PIXEL_FORMAT_NV16:
    case NVGR_PIXEL_FORMAT_NV16_EXTERNAL:
    {
        // This format requires an even width and height
        if ((width | height) & 1)
        {
            NV_SHOW_ERROR(NvError_BadParameter);
            ALOGI("%s %d", __func__, __LINE__);
            return -EINVAL;
        }

        SurfCount = 2;
        nvFormat[0] = NvColorFormat_Y8;
        nvFormat[1] = NvColorFormat_U8_V8;
        BufferType = NV_NATIVE_BUFFER_YUV;
        NvRmMultiplanarSurfaceSetup(Surf, SurfCount, width, height, NvYuvColorFormat_YUV422, nvFormat, attrs);
    }
    break;

    case HAL_PIXEL_FORMAT_BLOB:
        // This format represents an arbitrary allocation of 'width' number of
        // bytes. Model this internally as a normal 32BPP surface so that it
        // can be cleared efficiently using the 2d HW. This should be safe
        // assuming no module is going to access this memory as a surface, apart
        // from 2d when doing the initial clear.
        if (height != 1)
        {
            NV_SHOW_ERROR(NvError_BadParameter);
            ALOGI("%s %d", __func__, __LINE__);
            return -EINVAL;
        }

        SurfCount  = 1;
        BufferType = NV_NATIVE_BUFFER_SINGLE;
        nvFormat[0] = NvGrGetNvFormat(format);

        if (nvFormat[0] == NvColorFormat_Unspecified)
        {
            NV_SHOW_ERROR(NvError_BadParameter);
            ALOGI("%s %d", __func__, __LINE__);
            return -EINVAL;
        }
        else
        {
            // Compute the dimensions of a square'ish surface
            NvU32 bytesPerPixel = NV_COLOR_GET_BPP(nvFormat[0]) >> 3;
            NvU32 fakeWidth = sqrt(width / bytesPerPixel);
            NvU32 fakeHeight = (width / (fakeWidth * bytesPerPixel)) + 1;

            // Setup surface and then adjust Width so that Width and Pitch
            // are equal number of bytes
            NvRmSurfaceSetup(&Surf[0], fakeWidth, fakeHeight, nvFormat[0], attrs);
            Surf[0].Width = Surf[0].Pitch / bytesPerPixel;
            Surf[0].Height = (width / Surf[0].Pitch) + 1;
        }
        break;
    case HAL_PIXEL_FORMAT_Y8:
    case HAL_PIXEL_FORMAT_Y16:
        // This format assumes
        // - an even width
        // - an even height
        if ((width | height) & 1)
        {
            NV_SHOW_ERROR(NvError_BadParameter);
            ALOGI("%s %d", __func__, __LINE__);
            return -EINVAL;
        }
        // The expected gralloc usage flags are SW_* and HW_CAMERA_*,
        // and no other HW_ flags will be used.
        if (usage & GRALLOC_USAGE_HW_MASK & ~GRALLOC_USAGE_HW_CAMERA_MASK)
        {
            NV_SHOW_ERROR(NvError_BadParameter);
            ALOGI("%s %d", __func__, __LINE__);
            return -EINVAL;
        }

        SurfCount  = 1;
        BufferType = NV_NATIVE_BUFFER_SINGLE;
        nvFormat[0] = NvGrGetNvFormat(format);

        if (nvFormat[0] == NvColorFormat_Unspecified)
        {
            NV_SHOW_ERROR(NvError_BadParameter);
            ALOGI("%s %d", __func__, __LINE__);
            return -EINVAL;
        }
        NvRmSurfaceSetup(&Surf[0], width, height, nvFormat[0], attrs);

        // This format assumes
        // - a horizontal stride multiple of 16 pixels
        // - a vertical stride equal to the height
        Surf[0].Pitch = NvGrAlignUp(width, 16) *
            ((NV_COLOR_GET_BPP(nvFormat[0]) + 7) >> 3);
        break;

    default:
        SurfCount  = 1;
        if (format == NVGR_PIXEL_FORMAT_UYVY) {
            BufferType = NV_NATIVE_BUFFER_YUV;
        } else {
            BufferType = NV_NATIVE_BUFFER_SINGLE;
        }

        nvFormat[0] = NvGrGetNvFormat(format);
        if (nvFormat[0] == NvColorFormat_Unspecified)
        {
            NV_SHOW_ERROR(NvError_BadParameter);
            ALOGI("%s %d", __func__, __LINE__);
            return -EINVAL;
        }

        NvRmSurfaceSetup(&Surf[0], width, height, nvFormat[0], attrs);
        if  (usage & NVGR_USAGE_STEREO) {
            // TODO: We could check h/w capability and set
            // both frames to half width here.
            SurfCount  = 2;
            BufferType = NV_NATIVE_BUFFER_STEREO;
            NvRmSurfaceSetup(&Surf[1], width, height, nvFormat[0], attrs);
        }
        break;
    }

    // Get module ref

    if (NvGrModuleRef(Ctx) != NvSuccess)
        return -ENOMEM;

    // Allocate and map a kernel buffer object

    e = SharedAlloc(sizeof(NvGrBuffer), &ObjMem, (void**)&Obj);
    if (NV_SHOW_ERROR(e))
        return -ENOMEM;

    // Fill defaults

    NvOsMemset(Obj, 0, sizeof(NvGrBuffer));
    Obj->Magic = NVGR_BUFFER_MAGIC;
    Obj->Flags = 0;
    for (i = 0; i < NVGR_MAX_FENCES; i++)
        Obj->Fence[i].SyncPointID = NVRM_INVALID_SYNCPOINT_ID;
    Obj->WriteCount = -1;

    // Calculate plane pitch and offsets, and total size
    // NvRmSurfaceSetup has already computed pitch, but some YUV formats use a different one
    switch (format) {
    case NVGR_PIXEL_FORMAT_YV12_EXTERNAL:
        NV_ASSERT(Surf[0].Layout == NvRmSurfaceLayout_Pitch);
        Surf[0].Pitch = GetShadowBufferStrideInPixels(Ctx, format, Surf[0].Width, Surf[0].Height);
        // This formula comes from the Android system/graphics.h header
        Surf[1].Pitch = NvGrAlignUp(Surf[0].Pitch / 2, 16);
        Surf[2].Pitch = Surf[1].Pitch;

        Surf[0].Offset = 0;
        Surf[1].Offset = Surf[0].Offset + Surf[0].Pitch * Surf[0].Height;
        Surf[2].Offset = Surf[1].Offset + Surf[1].Pitch * Surf[1].Height;
        Obj->SurfSize  = Surf[2].Offset + Surf[2].Pitch * NvGrAlignUp(Surf[2].Height, 16);
        // YV12 is YVU order, swap the offsets for U and V
        NVGR_SWAP(Surf[1].Offset, Surf[2].Offset);
        SurfAlign = NvRmSurfaceComputeAlignment(Ctx->Rm, Surf);
        break;

    case NVGR_PIXEL_FORMAT_NV12_EXTERNAL:
    case NVGR_PIXEL_FORMAT_NV16_EXTERNAL:
        NV_ASSERT(Surf[0].Layout == NvRmSurfaceLayout_Pitch);
        Surf[0].Pitch = GetShadowBufferStrideInPixels(Ctx, format, Surf[0].Width, Surf[0].Height);
        Surf[1].Pitch = Surf[0].Pitch;

        Surf[0].Offset = 0;
        Surf[1].Offset = Surf[0].Offset + Surf[0].Pitch * NvGrAlignUp(Surf[0].Height, 16);
        Obj->SurfSize  = Surf[1].Offset + Surf[1].Pitch * NvGrAlignUp(Surf[1].Height, 16);
        SurfAlign = NvRmSurfaceComputeAlignment(Ctx->Rm, Surf);
        break;

    case NVGR_PIXEL_FORMAT_NV21_EXTERNAL:
        /* As specified by Google for YCrCb_420_SP, no vertical padding */
        NV_ASSERT(Surf[0].Layout == NvRmSurfaceLayout_Pitch);
        Surf[0].Pitch = GetShadowBufferStrideInPixels(Ctx, format, Surf[0].Width, Surf[0].Height);
        Surf[1].Pitch = Surf[0].Pitch;

        Surf[0].Offset = 0;
        Surf[1].Offset = Surf[0].Offset + Surf[0].Pitch * Surf[0].Height;
        Obj->SurfSize  = Surf[1].Offset + Surf[1].Pitch * Surf[1].Height;
        SurfAlign = NvRmSurfaceComputeAlignment(Ctx->Rm, Surf);
        break;

    case HAL_PIXEL_FORMAT_RAW_SENSOR:
        NV_ASSERT(SurfCount == 1);
        NV_ASSERT(Surf[0].Layout == NvRmSurfaceLayout_Pitch);
        Surf[0].Offset = 0;
        Surf[0].Pitch = NvGrAlignUp(Surf[0].Pitch, 32);
        NvGrGetAllocParams(Ctx, &Surf[0], &SurfAlign, &Obj->SurfSize, usage);
        break;

    case HAL_PIXEL_FORMAT_BLOB:
        // This surface will only be used by HW during the initial 2d clear
        // operation. It will never be used a source for any HW operation so
        // the size requirements can be relaxed.
        NV_ASSERT(Surf[0].Layout == NvRmSurfaceLayout_Pitch);
        Surf[0].Offset = 0;
        SurfAlign = NvRmSurfaceComputeAlignment(Ctx->Rm, Surf);
        Obj->SurfSize = Surf[0].Pitch * Surf[0].Height;
        break;

    default:
        // Get allocation params
        NvGrGetAllocParams(Ctx, &Surf[0], &SurfAlign, &Obj->SurfSize, usage);

        for (i = 1; i < (int) SurfCount; i++) {
            NvU32 align, size;

            NvGrGetAllocParams(Ctx, &Surf[i], &align, &size, usage);
            NV_ASSERT(align <= SurfAlign);

            // Align the surface offset
            Surf[i].Offset = (Obj->SurfSize + align-1) & ~(align-1);
            Obj->SurfSize = Surf[i].Offset + size;
        }
        break;
    }

    // Allocate the surface
    e = RmAlloc(Ctx->Rm, &Surf[0], SurfAlign, Obj->SurfSize,
                usage, &Surf[0].hMem);
    if (NV_SHOW_ERROR(e))
        return -ENOMEM;

    for (i = 1; i < (int) SurfCount; i++) {
        Surf[i].hMem = Surf[0].hMem;
    }

    // Create the handle

    h = CreateNativeHandle(ObjMem);
    if (!h)
    {
        /* TODO: [ahatala 2009-09-24] cleanup */
        NV_SHOW_ERROR(NvError_InsufficientMemory);
        return -ENOMEM;
    }

    // Fill process local info

    h->SurfMemFd = NvRmMemGetFd(Surf[0].hMem);
    h->DecompressFenceFd = -1;
    h->Buf       = Obj;
    h->SurfCount = SurfCount;
    h->Type      = BufferType;
    memcpy(h->Surf, Surf, SurfCount * sizeof(Surf[0]));
    h->Pixels    = NULL;
    h->hShadow   = NULL;
    h->RefCount  = 1;
    h->Usage     = usage;
    h->Format    = format;
    h->ExtFormat = format;
    h->SequenceNum = android_atomic_inc(&Ctx->SequenceNum);
    h->hSelf = h;
    pthread_mutex_init(&h->MapMutex, NULL);

    // Create sync objects

    pthread_mutexattr_init(&m_attr);
    pthread_mutexattr_setpshared(&m_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&Obj->LockMutex, &m_attr);
    pthread_mutex_init(&Obj->FenceMutex, &m_attr);
    pthread_mutexattr_destroy(&m_attr);
    pthread_condattr_init(&c_attr);
    pthread_condattr_setpshared(&c_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&Obj->LockExclusive, &c_attr);
    pthread_condattr_destroy(&c_attr);

    if  (usage & NVGR_USAGE_STEREO) {
        h->Buf->StereoInfo = NV_STEREO_SEPARATELR | NV_STEREO_ENABLE_MASK;
    }

    *handle = h;

    return 0;
}

static int NvGrFree (alloc_device_t* dev, buffer_handle_t handle)
{
    return NvGrFreeInternal((NvGrModule *) dev->common.module,
                            (NvNativeHandle *) handle);
}

int NvGrFreeInternal (NvGrModule *ctx, NvNativeHandle *handle)
{
    /* Mark the handle for deletion.  Do not delete here because there
     * may be additional references.
     */
    handle->DeleteHandle = 1;
    return NvGrUnregisterBuffer((gralloc_module_t*) ctx,
                                (buffer_handle_t) handle);
}

#define DUMP(...) \
    do { \
        if (buff_len > len) \
            len += snprintf(buff + len, buff_len - len, __VA_ARGS__); \
    } while (0)

static void NvGrDump(struct alloc_device_t* dev, char *buff, int buff_len)
{
    int len = 0;
    NvGrModule* ctx = (NvGrModule *)((NvGrAllocDev *)dev)->Base.common.module;
    const NvGrDecompression decompression = NvGrGetDecompression(ctx);

    DUMP("Nvidia Gralloc\n");
    DUMP("\tCompression: %s\n",
         NvGrGetCompression(ctx) == NvGrCompression_Enabled ? "on" : "off");

    DUMP("\tDecompression: %s\n",
         decompression == NvGrDecompression_Lazy ? "lazy" :
         decompression == NvGrDecompression_Client ? "client" : "off");
    DUMP("\tGPU mapping cache: %s\n",
         NvGrGetGpuMappingCache(ctx) ? "on" : "off");
    DUMP("\tContinuous property scan: %s\n",
         NvGrGetScanProps(ctx) ? "on" : "off");
}

static int NvGrAllocDevUnref (hw_device_t* hwdev)
{
    NvGrAllocDev* dev = (NvGrAllocDev*)hwdev;
    NvGrModule* m = (NvGrModule*)dev->Base.common.module;
    if (NvGrDevUnref(m, NvGrDevIdx_Alloc))
    {
        NvGrModuleUnref(m);
        NvOsFree(dev);
    }
    return 0;
}

int NvGrAllocDevOpen (NvGrModule* mod, hw_device_t** out)
{
    NvGrAllocDev* dev;

    dev = NvOsAlloc(sizeof(NvGrAllocDev));
    if (!dev) return -ENOMEM;
    NvOsMemset(dev, 0, sizeof(NvGrAllocDev));

    if (NvGrModuleRef(mod) != NvSuccess)
    {
        NvOsFree(dev);
        return -EINVAL;
    }

    dev->Base.common.tag     = HARDWARE_DEVICE_TAG;
    dev->Base.common.version = GRALLOC_DEVICE_API_VERSION_0_1;
    dev->Base.common.module  = &mod->Base.common;
    dev->Base.common.close   = NvGrAllocDevUnref;

    dev->Base.alloc          = NvGrAlloc;
    dev->Base.free           = NvGrFree;
    dev->Base.dump           = NvGrDump;

    dev->alloc_ext = NvGrAllocExt;

    *out = &dev->Base.common;
    return 0;
}

