/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvos_alloc.h"
#include "nvos.h"
#include "nvassert.h"

#if NVOS_TRACE || NV_DEBUG
#undef NvOsAlloc
#undef NvOsFree
#undef NvOsRealloc
#endif

#define TRACKER_DIE() NV_ASSERT(!"ResTracker dying!")
#define GUARDED_DIE() NV_ASSERT(!"Alloc guard dying!")

typedef NvU64 Guardband;
typedef struct AllocHeaderRec AllocHeader;

/* Information about allocations.
 *
 * Structure for user allocations is:
 *
 *   [AllocHeader + PreGuard + <buffer...> + PostGuard]
 */
struct AllocHeaderRec
{
    size_t              size;
    Guardband           preguard;
};

#define COMPUTE_GUARDED_SIZE(size) \
    (sizeof(AllocHeader) + size + sizeof(Guardband))

#define GET_GUARDED_HEADER(ptr) \
    (((AllocHeader *) ptr) - 1)

static const Guardband s_preGuard   = 0xc0decafef00dfeedULL;
static const Guardband s_postGuard  = 0xfeedbeefdeadbabeULL;
static const Guardband s_freedGuard = 0xdeaddeaddeaddeadULL;

static NvBool
GuardedVerify(NvU8 *ptr)
{
    AllocHeader *header;

    if (ptr == NULL)
        return NV_TRUE;

    header = GET_GUARDED_HEADER(ptr);

    if (header->size > 0x7FFFFFFF)
        NvOsDebugPrintf("WARNING: Allocation size exceeds 2 gigs. "
                        "Possibly compromised allocation "
                        "(userptr: 0x%08x, size %d)\n",
                        ptr, header->size);

    if (NvOsMemcmp(&header->preguard, &s_freedGuard, sizeof(Guardband)) == 0)
    {
        NvOsDebugPrintf("Allocation already freed (userptr: 0x%08x)\n",
                        ptr);
        return NV_FALSE;
    }

    if (NvOsMemcmp(&header->preguard, &s_preGuard, sizeof(Guardband)) != 0)
    {
        NvOsDebugPrintf("Allocation pre-guard compromised (userptr: 0x%08x)\n",
                        ptr);
        return NV_FALSE;
    }

    if (NvOsMemcmp((ptr + header->size), &s_postGuard, sizeof(Guardband)) != 0)
    {
        NvOsDebugPrintf("Allocation post-guard compromised (userptr: 0x%08x)\n",
                        ptr);
        return NV_FALSE;
    }

    return NV_TRUE;
}

static void *
GuardedAlloc(size_t size)
{
    void *ptr;
    AllocHeader *header;

    header = NvOsAllocInternal(COMPUTE_GUARDED_SIZE(size));
    if (header == NULL)
        return NULL;

    /* Write backreference, write guardbands, and fill only
     * the beginning of buffer with 0xcc (because memsetting
     * newly allocated pages can be slow due to lots of
     * pagefaults and we want to catch the most likely case:
     * someone allocating a struct and assuming the fields are
     * zero in our thin debug malloc). */

    ptr = header + 1;
    header->size = size;
    NvOsMemcpy(&header->preguard, &s_preGuard, sizeof(Guardband));
    NvOsMemcpy(((NvU8 *) ptr) + size, &s_postGuard, sizeof(Guardband));

    if (size <= 32)
    {
        NvOsMemset(ptr, 0xcc, size);
    }
    else
    {
        NvOsMemset(ptr, 0xcc, 16);
        NvOsMemset(((NvU8 *) ptr) + size - 16, 0xcc, 16);
    }

    return ptr;
}

static void *
GuardedRealloc(void *ptr,
               size_t size)
{
    AllocHeader *header;

    if (!GuardedVerify(ptr))
        GUARDED_DIE();

    if (ptr == NULL)
        header = NULL;
    else
        header = GET_GUARDED_HEADER(ptr);

    header = NvOsReallocInternal(header, COMPUTE_GUARDED_SIZE(size));
    if (header == NULL)
        return NULL;

    ptr = header + 1;
    header->size = size;
    NvOsMemcpy(&header->preguard, &s_preGuard, sizeof(Guardband));
    NvOsMemcpy(((NvU8 *) ptr) + size, &s_postGuard, sizeof(Guardband));

    return ptr;
}

static void
GuardedFree(void *ptr)
{
    AllocHeader *header;

    if (ptr == NULL)
        return;

    if (!GuardedVerify(ptr))
        GUARDED_DIE();

    header = GET_GUARDED_HEADER(ptr);

    NvOsMemcpy(&header->preguard, &s_freedGuard, sizeof(Guardband));
    NvOsMemcpy(((NvU8 *) ptr) + header->size, &s_freedGuard, sizeof(Guardband));

    NvOsFreeInternal(header);
}

#if NVOS_RESTRACKER_COMPILED

static void *
TrackedAlloc(size_t size,
             const char *filename,
             int line)
{
    void *ptr;

    if (!NvOsResourceTrackingEnabled())
        return GuardedAlloc(size);

    if (size == 0)
        return NULL;

    // Let resource tracker decide if this alloc failed or not.  Used
    // in stress testers for increased test coverage for error
    // conditions.

    if (!NvOsAllocSucceedsSim(NvOsAllocHookType_Alloc))
        return NULL;

    /* Do the actual allocation. */
    ptr = GuardedAlloc(size);
    if (ptr == NULL)
        return NULL;

    /* Make sure we can store metadata for the next allocation, or
     * bail out. */

    if (!NvOsTrackResource(NvOsResourceType_NvOsAlloc, ptr, filename, line))
    {
        GuardedFree(ptr);
        return NULL;
    }

    NvOsIncResTrackerStat(NvOsResTrackerStat_NumAllocs, 1,
                          NvOsResTrackerStat_NumPeakAllocs);
    NvOsIncResTrackerStat(NvOsResTrackerStat_NumBytes, size,
                          NvOsResTrackerStat_NumPeakBytes);
    NvOsIncResTrackerStat(NvOsResTrackerStat_TrackerBytes,
                          sizeof(AllocHeader) + sizeof(Guardband),
                          NvOsResTrackerStat_TrackerPeakBytes);

    return ptr;
}

static void TrackedFree(void *ptr, const char *filename, int line);

static void *
TrackedRealloc(void *ptr,
               size_t size,
               const char *filename,
               int line)
{
    size_t oldsize;
    void *newptr = NULL;

    // TODO [kraita 21/Nov/08] What if: Enable -> realloc A -> Disable -> realloc A. Expecting crash and burn.
    if (!NvOsResourceTrackingEnabled())
        return GuardedRealloc(ptr, size);

    // TODO there's probably a better way to handle Realloc failures
    if (!NvOsAllocSucceedsSim(NvOsAllocHookType_Realloc))
        return NULL;

    if (!ptr)
        return TrackedAlloc(size, filename, line);

    if (size == 0)
    {
        TrackedFree(ptr, filename, line);
        return NULL;
    }

    if (!NvOsIsTrackedResource(ptr))
    {
        NvOsDebugPrintf("Resource tracking enabled, but realloc received non-tracked pointer!\n");
        TRACKER_DIE();
    }

    if (!GuardedVerify(ptr))
    {
        NvOsResourceIsCorrupt(ptr);
        TRACKER_DIE();
    }

    /* Realloc and resource tracking database have to be updated
     * automically. Otherwise other threads can allocate released
     * pointer and get the resource tracked in conflict with OS
     * allocs
     */
    NvOsLockResourceTracker();
    oldsize = NvOsGetAllocSize(ptr);
    newptr = GuardedRealloc(ptr, size);
    if (newptr)
        NvOsUpdateResourcePointer(ptr, newptr);
    NvOsUnlockResourceTracker();

    if (newptr == NULL)
        return NULL;

    NvOsIncResTrackerStat(NvOsResTrackerStat_NumBytes,
                          (NvS64) size - (NvS64) oldsize,
                          NvOsResTrackerStat_NumPeakBytes);

    return newptr;
}

static void
TrackedFree(void *ptr,
            const char *filename,
            int line)
{
    if (!NvOsResourceTrackingEnabled())
    {
        GuardedFree(ptr);
        return;
    }

    // Not much that the allc fail simulator does with this callback,
    // but let's have it here for completeness.
    if (!NvOsAllocSucceedsSim(NvOsAllocHookType_Free))
        return;

    if (!ptr)
        return;

    if (!NvOsIsTrackedResource(ptr))
    {
        NvOsDebugPrintf("Resource tracking enabled, but free received non-tracked pointer!\n");
        TRACKER_DIE();
    }

    if (!GuardedVerify(ptr))
    {
        NvOsResourceIsCorrupt(ptr);
        TRACKER_DIE();
    }

    /* Update stats */
    NvOsDecResTrackerStat(NvOsResTrackerStat_NumAllocs, 1);
    NvOsDecResTrackerStat(NvOsResTrackerStat_NumBytes, NvOsGetAllocSize(ptr));
    NvOsDecResTrackerStat(NvOsResTrackerStat_TrackerBytes,
                          sizeof(AllocHeader) + sizeof(Guardband));

    /* Finally give up the freed block.
     * NOTE: TrackedResource must be released before free! Otherwise multi-threaded
     * code can allocate the same pointer again and get resource tracker into
     * inconsistent state.
     */
    NvOsReleaseTrackedResource(ptr);
    GuardedFree(ptr);
}

/*
 *  Get the size of an alloc.
 */
size_t
NvOsGetAllocSize(void *ptr)
{
    AllocHeader *header;

    if (ptr == NULL)
        return 0;

    header = GET_GUARDED_HEADER(ptr);

    if (NvOsMemcmp(&header->preguard, &s_preGuard, sizeof(Guardband)) == 0)
        return header->size;

    return 0;
}

/* Need this in the export table, will be called if we mix release application
 *  and debug NvOs.*/
static char message[] = "Release app, no line data";

void * NvOsAllocLeak(size_t size, const char *filename, int line)
    { return TrackedAlloc(size, filename, line); }
void * NvOsReallocLeak(void *ptr, size_t size, const char *filename, int line)
    { return TrackedRealloc(ptr, size, filename, line); }
void NvOsFreeLeak(void *ptr, const char *filename, int line)
    { TrackedFree(ptr, filename, line); }

void * NvOsAlloc(size_t size)
    { return NvOsAllocLeak(size, message, 0); }
void * NvOsRealloc(void *ptr, size_t size)
    { return NvOsReallocLeak(ptr, size, message, 0); }
void NvOsFree(void *ptr)
    { NvOsFreeLeak(ptr, message, 0); }

#else // NVOS_RESTRACKER_COMPILED

/* Need this in the export table, will be called if we mix debug application
 *  and release NvOs.*/
#if !(NV_DEBUG)
// If NV_DEBUG, we already have declarations.
void *NvOsAllocLeak(size_t size, const char *filename, int line);
void *NvOsReallocLeak(void *ptr, size_t size, const char *filename, int line);
void NvOsFreeLeak(void *ptr, const char *filename, int line);
#endif

void * NvOsAllocLeak(size_t size, const char *filename, int line)
    { return GuardedAlloc(size); }
void * NvOsReallocLeak(void *ptr, size_t size, const char *filename, int line)
    { return GuardedRealloc(ptr, size); }
void NvOsFreeLeak(void *ptr, const char *filename, int line)
    { GuardedFree(ptr); }

void * NvOsAlloc(size_t size)
    { return NvOsAllocInternal(size); }
void * NvOsRealloc(void *ptr, size_t size)
    { return NvOsReallocInternal(ptr, size); }
void NvOsFree(void *ptr)
    { NvOsFreeInternal(ptr); }

#endif // NVOS_RESTRACKER_COMPILED
