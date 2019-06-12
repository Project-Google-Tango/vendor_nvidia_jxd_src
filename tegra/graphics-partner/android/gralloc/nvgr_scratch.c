/*
 * Copyright (c) 2011-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <cutils/log.h>
#include <sync/sync.h>

#include "nvgralloc.h"
#include "nvgrsurfutil.h"
#include "nvrm_chip.h"
#include "nvblit.h"
#include "nvsync.h"
#include "nvhwc_util.h"

#if NVGR_USE_TRIPLE_BUFFERING
#define NVGR_NUM_SCRATCH_BUFFERS     3
#else
#define NVGR_NUM_SCRATCH_BUFFERS     2
#endif

struct NvGrScratchMachineRec {
    NvGrScratchClient client;
    NvGrModule *ctx;
    size_t num_sets;
    NvGrScratchSet sets[0];
};

typedef struct NvGrScratchMachineRec NvGrScratchMachine;

static void NvGrFreeScratchSet(NvGrModule *ctx, NvGrScratchSet *buf)
{
    int ii;

    for (ii = 0; ii < NVGR_NUM_SCRATCH_BUFFERS; ii++) {
        if (buf->buffers[ii]) {
            NvGrFreeInternal(ctx, buf->buffers[ii]);
            // Set buffer handle to NULL to avoid double frees
            buf->buffers[ii] = NULL;
        }
        nvsync_close(buf->releaseFenceFds[ii]);
        buf->releaseFenceFds[ii] = -1;
    }

    buf->state = NVGR_SCRATCH_BUFFER_STATE_FREE;
}

static int NvGrAllocScratchSet(NvGrModule *ctx, NvGrScratchSet *buf,
                               unsigned int width, unsigned int height,
                               NvColorFormat nv_format,
                               NvRmSurfaceLayout layout,
                               int protect)
{
    int stride, usage, ret;
    int format;
    NvNativeHandle *nvbuf;
    int ii;

    usage = GRALLOC_USAGE_HW_2D |
            GRALLOC_USAGE_HW_FB |
            GRALLOC_USAGE_HW_RENDER |
            NVGR_USAGE_UNCOMPRESSED;

    if (protect) {
        usage |= GRALLOC_USAGE_PROTECTED;
    }

    format = NvGrGetHalFormat(nv_format);

    for (ii = 0; ii < NVGR_NUM_SCRATCH_BUFFERS; ii++) {
        ret = NvGrAllocInternal(ctx, width, height, format,
                                usage, layout,
                                &buf->buffers[ii]);

        if (ret != 0) {
            ALOGE("%s: NvGrAllocInternal failed", __func__);
            // Free the already allocated buffers of the set
            NvGrFreeScratchSet(ctx, buf);
            return ret;
        }

        NV_ASSERT(buf->releaseFenceFds[ii] == -1);
    }

    buf->format = nv_format;
    buf->state = NVGR_SCRATCH_BUFFER_STATE_ALLOCATED;
    buf->active_index = 0;
    buf->last_seqnum = -1;
    buf->last_write_count = -1;
    buf->width = width;
    buf->height = height;
    buf->layout = layout;

    return ret;
}

#define IS_PROTECTED(buf) !!(buf->Usage & GRALLOC_USAGE_PROTECTED)

static NvGrScratchSet *
FindScratchSet(NvGrScratchMachine *sm, unsigned int width,
               unsigned int height, NvColorFormat format,
               NvRmSurfaceLayout layout, int protect)
{
    NvGrScratchSet *found = NULL, *found_allocated = NULL;
    size_t ii;

    /* scan the pool of available rotate buffers looking for one that
     * matches the layer
     */
    for (ii = 0; ii < sm->num_sets; ii++) {
        NvGrScratchSet *cur = &sm->sets[ii];

        switch (cur->state) {
        case NVGR_SCRATCH_BUFFER_STATE_FREE:
            if (!found) {
                found = cur;
            }
            break;
        case NVGR_SCRATCH_BUFFER_STATE_ALLOCATED:
            /* Check for an exact match */
            if ((cur->buffers[0])->Surf[0].Width == width &&
                (cur->buffers[0])->Surf[0].Height == height &&
                (IS_PROTECTED(cur->buffers[0]) == protect) &&
                (cur->format == format) &&
                (cur->layout == layout)) {
                /* found a match */
                return cur;
            }
            if (!found_allocated) {
                found_allocated = cur;
            }
            break;
        case NVGR_SCRATCH_BUFFER_STATE_ASSIGNED:
        case NVGR_SCRATCH_BUFFER_STATE_LOCKED:
            /* Already assigned - do not touch */
            break;
        }
    }

    /* In order to reduce thrashing, reusing an allocated buffer is
     * the last option.
     */
    if (!found && found_allocated) {
        NvGrFreeScratchSet(sm->ctx, found_allocated);
        found = found_allocated;
    }

    if (!found) {
        ALOGE("%s: no free slots!", __func__);
        /* While this is usually a non-fatal error, its a silent perf
         * issue if we fail to alloc a scratch buffer.  Hitting this
         * case means we require more scratch buffers than we planned
         * for, so it would be good to understand why and possibly
         * increase the pool size.  No customer builds were harmed
         * during the making of this motion picture.
         */
        if (NV_DEBUG) {
            char dump[1000];
            sm->client.dump(&sm->client, dump, sizeof(dump));
            ALOGD("%s", dump);
        }
        NV_ASSERT(found);
        return NULL;
    }

    if (0 != NvGrAllocScratchSet(sm->ctx, found, width, height, format, layout, protect)) {
        ALOGE("scratch alloc failed!");
        return NULL;
    }

    return found;
}

// For GR2D, the fast rotate path is hit when the width and
// height are multiples of 128 bits.  If other chips require different
// alignment, we need a way to get this info out of nvblit.
static void ComputeAdjustments(NvGrScratchClient *sc,
                               NvColorFormat format,
                               NvRmSurfaceLayout layout,
                               int src_width,
                               int src_height,
                               int transform,
                               int *buf_width,
                               int *buf_height,
                               int *aligned_width,
                               int *aligned_height)
{
    NvGrScratchMachine *sm = (NvGrScratchMachine *) sc;
    int align;

    *buf_width = src_width;
    *buf_height = src_height;

    // Ensure even size for YUV surfaces
    if (NV_COLOR_GET_COLOR_SPACE(format) != NvColorSpace_LinearRGBA) {
        *buf_width = NvGrAlignUp(*buf_width, 2);
        *buf_height = NvGrAlignUp(*buf_height, 2);
    }

    // No further adjustments needed for simple blits
    if (transform == 0) {
        *aligned_width = *buf_width;
        *aligned_height = *buf_height;
        return;
    }

    // Scratch blit will change orientation
    if (transform & HAL_TRANSFORM_ROT_90) {
        NVGR_SWAP(*buf_width, *buf_height);
    }

    // No further adjustments needed for VIC hardware
    if (sm->ctx->HaveVic) {
        *aligned_width = *buf_width;
        *aligned_height = *buf_height;
        return;
    }

    // GR2D hardware needs further adjustments when the scratch blit involves
    // rotation, so that we can do the blit in a single pass. GR2D can only
    // rotate to aligned positions, so without this adjustment the blit may need
    // a second pass to crop to the desired location. Instead we compute a
    // correctly aligned position to blit to, which can be passed onto the next
    // stage of composition as a buffer position offset.

    // Compute GR2D rotation alignment restriction
    if (layout == NvRmSurfaceLayout_Tiled) {
        align = NVRM_SURFACE_SUB_TILE_WIDTH;
    } else {
        align = 128 / NV_COLOR_GET_BPP(format);
    }

    // Account for ddk2d internal behaviour regarding YUV planar formats
    if (NV_COLOR_GET_COLOR_SPACE(format) != NvColorSpace_LinearRGBA) {
        align *= 2;
    }

    // Finally here is the required dimensions of the scratch buffer
    *aligned_width = NvGrAlignUp(*buf_width, align);
    *aligned_height = NvGrAlignUp(*buf_height, align);
}

static int ValidateScratch(NvGrScratchClient *sc,
                           int buf_width,
                           int buf_height,
                           NvRect *src_crop)
{
    NvGrScratchMachine *sm = (NvGrScratchMachine *) sc;
    const NvBlitCapability *cap;

    cap = NvBlitQuery(sm->ctx->nvblit);

    if (buf_width > (int) cap->MaxWidth ||
        buf_height > (int) cap->MaxHeight) {
        return 1;
    }

    if (src_crop != NULL) {
        float h_scale;
        float v_scale;
        int src_width = r_width(src_crop);
        int src_height = r_height(src_crop);

        if (src_width > (int) cap->MaxWidth ||
            src_height > (int) cap->MaxHeight) {
            return 1;
        }

        h_scale = (float) buf_width / src_width;
        v_scale = (float) buf_height / src_height;

        if (h_scale < cap->MinScale ||
            h_scale > cap->MaxScale ||
            v_scale < cap->MinScale ||
            v_scale > cap->MaxScale) {
            return 1;
        }
    }

    return 0;
}

static NvGrScratchSet *ScratchAssign(NvGrScratchClient *sc, int transform,
                                     unsigned int width, unsigned int height,
                                     NvColorFormat format, NvRmSurfaceLayout layout,
                                     NvRect *src_crop, int protect)
{
    NvGrScratchMachine *sm = (NvGrScratchMachine *) sc;
    NvGrScratchSet *buf;
    int buf_width;
    int buf_height;
    int aligned_width;
    int aligned_height;

    ComputeAdjustments(sc,
                       format,
                       layout,
                       width, height,
                       transform,
                       &buf_width, &buf_height,
                       &aligned_width, &aligned_height);

    if (ValidateScratch(sc, buf_width, buf_height, src_crop)) {
        return NULL;
    }

    buf = FindScratchSet(sm,
                         aligned_width, aligned_height,
                         format, layout, protect);

    if (buf) {
        NV_ASSERT(buf->state == NVGR_SCRATCH_BUFFER_STATE_ALLOCATED);
        buf->state = NVGR_SCRATCH_BUFFER_STATE_ASSIGNED;

        /* Store the blit size. If padding is required this will be
         * applied inside ddk2d.
         */
        buf->width = buf_width;
        buf->height = buf_height;
        buf->transform = transform;

        if (src_crop) {
            /* If the crop has changed, invalidate the cached image */
            if (memcmp(src_crop, &buf->src_crop, sizeof(buf->src_crop))) {
                buf->last_seqnum = -1;
                buf->last_write_count = -1;
            }
            buf->src_crop = *src_crop;
            buf->use_src_crop = NV_TRUE;
        } else {
            buf->use_src_crop = NV_FALSE;
        }
    }

    return buf;
}

static void ScratchLock(NvGrScratchClient *sc, NvGrScratchSet *buf)
{
    NV_ASSERT(buf->state == NVGR_SCRATCH_BUFFER_STATE_ASSIGNED);
    buf->state = NVGR_SCRATCH_BUFFER_STATE_LOCKED;
}

static void ScratchUnlock(NvGrScratchClient *sc, NvGrScratchSet *buf)
{
    NV_ASSERT(buf->state == NVGR_SCRATCH_BUFFER_STATE_LOCKED ||
              buf->state == NVGR_SCRATCH_BUFFER_STATE_ALLOCATED);
    buf->state = NVGR_SCRATCH_BUFFER_STATE_ALLOCATED;
}

static void ScratchFrameStart(NvGrScratchClient *sc)
{
    NvGrScratchMachine *sm = (NvGrScratchMachine *) sc;
    size_t ii;

    /* reset the list of scratch buffers */
    for (ii = 0; ii < sm->num_sets; ii++) {
        NvGrScratchSet *buf = &sm->sets[ii];

        if (buf->state == NVGR_SCRATCH_BUFFER_STATE_ASSIGNED) {
            buf->state = NVGR_SCRATCH_BUFFER_STATE_ALLOCATED;
        }
    }
}

static void ScratchFrameEnd(NvGrScratchClient *sc)
{
    NvGrScratchMachine *sm = (NvGrScratchMachine *) sc;
    size_t ii;

    for (ii = 0; ii < sm->num_sets; ii++) {
        NvGrScratchSet *buf = &sm->sets[ii];

        if (buf->state == NVGR_SCRATCH_BUFFER_STATE_ALLOCATED) {
            /* Buffer is not used in this config and may be released */
            NvGrFreeScratchSet(sm->ctx, buf);
        }
    }
}

static NvNativeHandle *ScratchGetBuffer(NvGrScratchClient *sc,
                                        NvGrScratchSet *buf)
{
    NV_ASSERT(buf->buffers[buf->active_index]->DecompressFenceFd == -1);
    return buf->buffers[buf->active_index];
}

static NvNativeHandle *ScratchNextBuffer(NvGrScratchClient *sc,
                                         NvGrScratchSet *buf,
                                         int *acquireFenceFd)
{
    NvGrScratchMachine *sm = (NvGrScratchMachine *) sc;

    NV_ASSERT(buf);
    if (!buf) {
        return NULL;
    }

    NV_ASSERT(buf->state == NVGR_SCRATCH_BUFFER_STATE_ASSIGNED ||
              buf->state == NVGR_SCRATCH_BUFFER_STATE_LOCKED);

    buf->active_index = (buf->active_index + 1) % NVGR_NUM_SCRATCH_BUFFERS;
    buf->last_seqnum = -1;
    *acquireFenceFd = nvsync_dup(__func__, buf->releaseFenceFds[buf->active_index]);

    return buf->buffers[buf->active_index];
}

static int ScratchBlit(NvGrScratchClient *sc, NvNativeHandle *src, int srcIndex,
                       int srcFence, NvGrScratchSet *buf, NvPoint *offset,
                       int *outFence)
{
    NvGrScratchMachine *sm = (NvGrScratchMachine *) sc;
    NvNativeHandle *dst;
    NvRect src_rect, dst_rect;
    NvRect *src_crop;
    NvError err;
    int dstFence;
    int status;
    int32_t WriteCount = NvGrGetWriteCount(src);

    NV_ASSERT(buf->state == NVGR_SCRATCH_BUFFER_STATE_ASSIGNED ||
              buf->state == NVGR_SCRATCH_BUFFER_STATE_LOCKED);

    if (src->SequenceNum == buf->last_seqnum &&
        WriteCount == buf->last_write_count &&
        (src->Buf->Flags & NvGrBufferFlags_Posted)) {
        /* skip this blit -- buffer did not change */
        *offset = buf->offset;
        nvsync_close(srcFence);
        *outFence = -1; // XXX cache last blit fence?
        return 0;
    }

    /* do the blit into the next buffer */
    buf->active_index = (buf->active_index + 1) % NVGR_NUM_SCRATCH_BUFFERS;
    buf->last_seqnum = src->SequenceNum;
    buf->last_write_count = WriteCount;
    src->Buf->Flags |= NvGrBufferFlags_Posted;

    dst = buf->buffers[buf->active_index];
    dstFence = nvsync_dup(__func__, buf->releaseFenceFds[buf->active_index]);

    if (buf->use_src_crop) {
        src_crop = &buf->src_crop;
    } else {
        /* Use the entire source */
        src_rect.top = 0;
        src_rect.left = 0;
        src_rect.right = src->Surf->Width;
        src_rect.bottom = src->Surf->Height;

        src_crop = &src_rect;
    }

    /* Use the scratch buffer size for dest, which may differ from the
     * surface size
     */
    dst_rect.top = 0;
    dst_rect.left = 0;
    dst_rect.right = buf->width;
    dst_rect.bottom = buf->height;

    status = NvGr2dBlit(sm->ctx,
                        src, src_crop, srcFence,
                        dst, &dst_rect, dstFence,
                        buf->transform, &buf->offset, outFence);
    *offset = buf->offset;
    return status;
}

static void ScratchSetReleaseFence(NvGrScratchClient *sc, NvGrScratchSet *buf,
                                   int releaseFenceFd)
{
    nvsync_close(buf->releaseFenceFds[buf->active_index]);
    buf->releaseFenceFds[buf->active_index] = releaseFenceFd;
}

#define DUMP(...) \
    do { \
        if (buff_len > len) \
            len += snprintf(buff + len, buff_len - len, __VA_ARGS__); \
    } while (0)

static NV_INLINE const char* NvGrLayoutToString(NvRmSurfaceLayout layout)
{
    switch (layout) {
        #define CASE(l) case NvRmSurfaceLayout_##l: return #l
        CASE(Pitch);
        CASE(Tiled);
        #undef CASE
        default: return "Unknown";
    }
}

static int ScratchDump(NvGrScratchClient *sc, char *buff, int buff_len)
{
    NvGrScratchMachine *sm = (NvGrScratchMachine *) sc;
    size_t ii;
    int len = 0;

    DUMP("      Scratch pool (%d total):\n", sm->num_sets);
    for (ii = 0; ii < sm->num_sets; ii++) {
        NvGrScratchSet *cur = &sm->sets[ii];

        DUMP("\tbuffer %d: ", ii);
        if (cur->state == NVGR_SCRATCH_BUFFER_STATE_FREE) {
            DUMP("free\n");
        } else {
            const char *state;
            const char *format;
            const char *layout;

            switch (cur->state) {
            case NVGR_SCRATCH_BUFFER_STATE_ALLOCATED:
                state = "allocated";
                break;
            case NVGR_SCRATCH_BUFFER_STATE_ASSIGNED:
                state = "assigned";
                break;
            case NVGR_SCRATCH_BUFFER_STATE_LOCKED:
                state = "locked";
                break;
            default:
                NV_ASSERT(0);
                state = "error";
                break;
            }
            format = NvColorFormatToString(cur->format);
            layout = NvGrLayoutToString(cur->layout);

            DUMP("%s, size=(%d, %d), format = %s, layout = %s\n",
                 state, cur->width, cur->height, format, layout);
        }
    }

    return len;
}

#undef DUMP

static NvGrScratchClient *ScratchClientAlloc(NvGrModule *ctx, size_t count)
{
    size_t bytes = sizeof(NvGrScratchMachine) + count * sizeof(NvGrScratchSet);
    NvGrScratchMachine *sm = (NvGrScratchMachine *) malloc(bytes);

    if (sm) {
        NvBool bl;
        size_t ii, jj;

        memset(sm, 0, bytes);
        sm->ctx = ctx;
        sm->num_sets = count;
        sm->client.assign = ScratchAssign;
        sm->client.lock = ScratchLock;
        sm->client.unlock = ScratchUnlock;
        sm->client.get_buffer = ScratchGetBuffer;
        sm->client.next_buffer = ScratchNextBuffer;
        sm->client.frame_start = ScratchFrameStart;
        sm->client.frame_end = ScratchFrameEnd;
        sm->client.blit = ScratchBlit;
        sm->client.set_release_fence = ScratchSetReleaseFence;
        sm->client.dump = ScratchDump;

        NvRmChipGetCapabilityBool(NvRmChipCapability_System_BlocklinearLayout, &bl);
        if (bl) {
            sm->client.rotation_layout = NvRmSurfaceLayout_Blocklinear;
        } else {
            sm->client.rotation_layout = NvRmSurfaceLayout_Tiled;
        }

        for (ii = 0; ii < sm->num_sets; ii++) {
            NvGrScratchSet *buf = &sm->sets[ii];

            for (jj = 0; jj < NVGR_NUM_SCRATCH_BUFFERS; jj++) {
                buf->releaseFenceFds[jj] = -1;
            }
        }
    }

    return (NvGrScratchClient *) sm;
}

static void ScratchClientFree(NvGrModule *ctx, NvGrScratchClient *sc)
{
    NvGrScratchMachine *sm = (NvGrScratchMachine *) sc;

    if (sm) {
        size_t ii;

        for (ii = 0; ii < sm->num_sets; ii++) {
            NvGrFreeScratchSet(ctx, &sm->sets[ii]);
        }

        free(sm);
    }
}

static int FindClientSlot(NvGrModule *ctx, NvGrScratchClient *sc)
{
    size_t ii;

    for (ii = 0; ii < NVGR_MAX_SCRATCH_CLIENTS; ii++) {
        if (ctx->scratch_clients[ii] == sc) {
            return ii;
        }
    }

    return -1;
}

int NvGrScratchOpen(NvGrModule *ctx, size_t count, NvGrScratchClient **scratch)
{
    int slot = FindClientSlot(ctx, NULL);

    if (slot >= 0) {
        NvGrScratchClient *sc = ScratchClientAlloc(ctx, count);
        NV_ASSERT(ctx->scratch_clients[slot] == NULL);

        if (sc) {
            *scratch = ctx->scratch_clients[slot] = sc;
            return 0;
        }
    }

    return -1;
}

void NvGrScratchClose(NvGrModule *ctx, NvGrScratchClient *scratch)
{
    int slot = FindClientSlot(ctx, scratch);

    NV_ASSERT(slot >= 0 && slot < NVGR_MAX_SCRATCH_CLIENTS);
    NV_ASSERT(ctx->scratch_clients[slot] == scratch);
    ctx->scratch_clients[slot] = NULL;
    ScratchClientFree(ctx, scratch);
}

NvError NvGrScratchInit(NvGrModule *ctx)
{
    return NvSuccess;
}

void NvGrScratchDeInit(NvGrModule *ctx)
{
    size_t ii;

    for (ii = 0; ii < NVGR_MAX_SCRATCH_CLIENTS; ii++) {
        ScratchClientFree(ctx, ctx->scratch_clients[ii]);
        ctx->scratch_clients[ii] = NULL;
    }
}
