/*
 * Copyright 2007 - 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvos_debug.h"
#include "nvos.h"
#include "nvos_alloc.h"

#if NVOS_RESTRACKER_COMPILED

#include "nvassert.h"
#include "nvos_pointer_hash.h"

#if NVOS_TRACE || NV_DEBUG
#   undef NvOsAlloc
#   undef NvOsFree
#   undef NvOsRealloc
#endif

/* Defines. */

#define RESTRACKER_ENABLED_BY_DEFAULT 1
#define NVOS_RESOURCELIST_SIZE        512
#define NVOS_META_STORE_INIT_SIZE     128
#define NVOS_FILENAME_STORE_INIT_SIZE 32
#define NVOS_CHUNK_ITEMS              64

#define MAX_FILENAME_CHARS  255
#define MAX_ALLOC_LOCATIONS 8

typedef enum
{
    ResourceMetaFlag_IsCorrupt = 1,
    ResourceMetaFlag_Force32   = 0x7FFFFFFF
} ResourceMetaFlag;


/*
 * Resource metadata structures.
 */
typedef struct ResourceTrackerRec      ResourceTracker;
typedef struct ResourceMetaRec         ResourceMeta;
typedef struct ResourceAllocLocRec     ResourceAllocLoc;
typedef struct AllocResourceMetaRec    AllocResourceMeta;
typedef struct RefCountResourceMetaRec RefCountResourceMeta;
typedef struct ResourceMetaChunkRec    ResourceMetaChunk;


struct ResourceAllocLocRec
{
    const char*         filename;   /* File that allocated the resource. Can be "" or s_filenameAllocFailed, but not NULL. */
    NvU32               line;       /* Line from which resource allocation was done. */
    NvCallstack*        callstack;  /* Pointer to OS specific call stack description. Can be NULL. */
};

/* Resource metadata base class */

struct ResourceMetaRec
{
    /* Sequential ID number of the resource. */
    NvU32               id;
    /* The pointer the NvOS user sees for this resource. Used to map resource to
     * this meta info. Also used for finding free items in ResourceMeta allocation chunks.*/
    NvU8*               userptr;
    /* Offset to chunk descriptor in bytes. */
    NvU16               ChunkOffset;
    /* NvOsResourceType, packed */
    NvU8                type;
    /* Resource flags. */
    NvU8                flags;
};

// Allocation resource. Can't be ref counted.
struct AllocResourceMetaRec
{
    ResourceMeta        meta;
    ResourceAllocLoc    loc;
};

// Ref counted resource.
struct RefCountResourceMetaRec
{
    ResourceMeta        meta;
    ResourceAllocLoc    Locations[MAX_ALLOC_LOCATIONS]; /* List of locations that added refs. */
    NvS32               CurLocation;                    /* Index of the next free location. Can wrap and overwrite locations. */
    NvS32               RefCount;                       /* Current reference count of the resource. */
    NvU32               LatestRefInc;                   /* ID counter at the time when there was a reference increment */
};

// Chunk storing resource meta records. Used to amortize cost of system alloc
// both in terms of time and space.
struct ResourceMetaChunkRec
{
    NvS32              ItemSize;  /* Size of a single item in bytes                  */
    NvS32              FirstFree; /* Byte offset to first free item in the list      */
    NvS32              NumUsed;   /* Number of used entries                          */
    ResourceMetaChunk* next;      /* Doubly linked list of chunks with free entries  */
    ResourceMetaChunk* prev;
};

/* Top-level resource manager. */

struct ResourceTrackerRec
{
    NvU32                   resourceCounter;/* Unique resource identifier counter. Starts from 1. */

    PointerHash*            MetaStore;      /* Hash table storing meta info structs. Keyed on resource id. */

    NvOsMutexHandle         mutex;          /* Mutex over all operations on this struct */

    NvBool                  enabled;        /* Redirect straight to internal alloc? */
    NvBool                  verbose;        /* Print stuff even when there are no leaks. */
    NvBool                  stats;          /* Print stats in the end/NvOsDumpSnapshot(). */
    NvBool                  dumpOnExit;     /* Print leak dump on exit */
    NvU32                   statsNthAlloc;  /* If gtz, print stats every N allocs and in the end. */
    NvU32                   statsNthSecond; /* If gtz, print stats every N seconds and in the end. */
    NvU32                   breakId;        /* Break at given alloc Id. */
    NvU32                   breakStackId;   /* Break at given stack Id */
    NvU32                   stackFilterPid; /* Dump filtering based on pid */
    NvOsCallstackType       callstackType;
    NvOsFileHandle          dumpOut;

    NvS64                   statistics[NvOsResTrackerStat_MaxStat]; /* Statistics counters */

    NvU32                   lastDumpTime;
    NvU32                   lastDumpResourceId;
    NvS32                   numResources;

    PointerHash*            FilenameStore;  /* File names in the allocation locations. Memory optimization. */

    ResourceMetaChunk*      FreeAllocChunks;    /* Chunks with free AllocResourceMeta items. */
    ResourceMetaChunk*      FreeRefCountChunks; /* Chunks with free RefCountResourceMeta items. */

    // Simulated alloc failures
    void*                   AllocSimUserContext;
    NvOsAllocSimCB          AllocSimCB;
};

/* The root resource tracker. */

static ResourceTracker s_tracker = { 0LL };

static const char* s_filenameAllocFailed = "<out-of-mem>";

/* Local functions. */

static NV_INLINE NvOsResourceType   GetResourceType         (ResourceMeta* meta);
static NV_INLINE void               SetResourceFlags        (ResourceMeta* meta, NvU32 flags);
static NV_INLINE NvBool             TestResourceFlag        (ResourceMeta* meta, ResourceMetaFlag flag);
static NV_INLINE ResourceMeta*      GetResourceMeta         (NvU8* userptr);
static NV_INLINE void               IncRefCount             (ResourceMeta* meta);
static NV_INLINE NvBool             DecRefCount             (ResourceMeta* meta);
static NV_INLINE ResourceAllocLoc*  GetNewLocation          (ResourceMeta* meta);
static NV_INLINE NvBool             ActivityInInterval      (ResourceMeta* meta, NvU32 start, NvU32 end);
static ResourceMeta*                AllocateResourceMeta    (NvOsResourceType type);
static void                         DeallocateResourceMeta  (ResourceMeta* meta);

static NvBool                       VerifyResource          (ResourceMeta* meta);

static void                         ReportCorruption        (NvU8* userptr, const char* filename, int line);
static void                         DumpStats               (void);
static void                         DumpStatsVerbose        (void);
static NvU32                        DumpResources           (NvU32 start, NvU32 end, NvBool only_corrupted, NvBool filterOnPid);

static void                         DumpResource            (ResourceMeta* meta);
static const char*                  ResourceTypeToString    (NvOsResourceType type);

// Call with mutex held!
static void                         IncResTrackerStatInternal(NvOsResTrackerStat stat, NvS64 inc, NvOsResTrackerStat peak);
static const char*                  GetFilenameStringCopy   (const char* filename);

NvOsResourceType GetResourceType(ResourceMeta* meta)
{
    NVOS_ASSERT(meta);

    return (NvOsResourceType)meta->type;
}

void SetResourceFlags(ResourceMeta* meta, NvU32 flags)
{
    NVOS_ASSERT(meta);
    NVOS_ASSERT((flags & 0xFFFFFF00) == 0);
    meta->flags = (NvU8)meta->flags | flags;
}

NvBool TestResourceFlag(ResourceMeta* meta, ResourceMetaFlag flag)
{
    NVOS_ASSERT(meta);
    return meta->flags & flag;
}

void IncRefCount(ResourceMeta* meta)
{
    NVOS_ASSERT(meta);
    if (GetResourceType(meta) != NvOsResourceType_NvOsAlloc)
    {
        RefCountResourceMeta* refCountMeta = (RefCountResourceMeta*)meta;
        refCountMeta->RefCount++;
        refCountMeta->LatestRefInc = s_tracker.resourceCounter++;

    }
}

/**
 * Return true if still alive after dec.
 */
NvBool DecRefCount(ResourceMeta* meta)
{
    NVOS_ASSERT(meta);
    if (meta->type != NvOsResourceType_NvOsAlloc)
    {
        return --(((RefCountResourceMeta*)meta)->RefCount) != 0;
    }
    else
    {
        NVOS_ASSERT(meta->userptr);
        meta->userptr = NULL;
        return NV_FALSE;
    }
}

ResourceAllocLoc* GetNewLocation(ResourceMeta* meta)
{
    ResourceAllocLoc* rv;

    NVOS_ASSERT(meta);

    if (GetResourceType(meta) == NvOsResourceType_NvOsAlloc)
    {
        rv = &((AllocResourceMeta*)meta)->loc;
    }
    else
    {
        RefCountResourceMeta* refCountMeta = (RefCountResourceMeta*)meta;
        rv = &refCountMeta->Locations[refCountMeta->CurLocation];
        refCountMeta->CurLocation++;
        if (refCountMeta->CurLocation == MAX_ALLOC_LOCATIONS)
            refCountMeta->CurLocation = 0;
    }
    return rv;
}

NvBool ActivityInInterval(ResourceMeta* meta, NvU32 start, NvU32 end)
{
    NvU32 sample;
    NVOS_ASSERT(meta);
    if (GetResourceType(meta) == NvOsResourceType_NvOsAlloc)
        sample = meta->id;
    else
        /* TODO: [ahatala 2009-11-05] this is not quite correct*/
        sample = ((RefCountResourceMeta*)meta)->LatestRefInc;

    return ((sample >= start) && (sample < end));
}

/*
 * Call to register a resource for tracking.
 */

void NvOsResourceAllocated(NvOsResourceType type, void* ptr)
{
    if (NvOsResourceTrackingEnabled())
    {
        NvOsTrackResource(type, ptr, "", 0);
    }
}

/*
 * Resource is no more tracked.
 */

void NvOsResourceFreed(NvOsResourceType type, void* ptr)
{
    if (NvOsResourceTrackingEnabled())
    {
       if (!NvOsIsTrackedResource(ptr))
        {
            NvOsDebugPrintf("Trying to free a resource that's not tracked!\n");
            NV_ASSERT(NV_FALSE);
        }
        NvOsReleaseTrackedResource(ptr);
    }
}

/*
 * Called at the start by main thread only.
 * TODO [kraita 25/Nov/08] Thread safe init!
 * TODO [kraita 27/Nov/08] Fail init if resource allocations fail!
 */

void NvOsInitResourceTracker(void)
{
    NvU32   tmp;
    char    str[2];
    NvBool  enabled = (RESTRACKER_ENABLED_BY_DEFAULT != 0);
    NvError error;

    NV_ASSERT(s_tracker.resourceCounter == 0);

    s_tracker.AllocSimUserContext = NULL;
    s_tracker.AllocSimCB = NULL;

    /* Disable tracker while creating mutex to redirect allocs to
     * NvOsAllocInternal(). Also, allocating a mutex must always succeed.
     * We have no other way to fail but die on an assert. */
    s_tracker.enabled = NV_FALSE;

    error = NvOsMutexCreateInternal(&s_tracker.mutex);

    NV_ASSERT(error == NvError_Success);

    /* Check if RT variable is there and print usage if it wasn't properly used. */

    error = NvOsGetConfigString("RT", str, 2);

    if (error == NvError_Success)
    {
        if (str[0] == '0' || str[0] == '1')
        {
            enabled = (str[0] == '1');
        }
        else
        {
            NvOsDebugPrintf(RESTRACKER_TAG "Configure by setting the following variables:\n");
            NvOsDebugPrintf(RESTRACKER_TAG "\n");
            NvOsDebugPrintf(RESTRACKER_TAG "RT=n                  Enable ResourceTracker if n==1, else disable.\n");
            NvOsDebugPrintf(RESTRACKER_TAG "RT_BREAK=n            Break at <n>th allocation.\n");
            NvOsDebugPrintf(RESTRACKER_TAG "RT_STATS              Print verbose statistics on exit.\n");
            NvOsDebugPrintf(RESTRACKER_TAG "RT_STATS_NTH_ALLOC=n  Print terse statistics every <n>th alloc.\n");
            NvOsDebugPrintf(RESTRACKER_TAG "RT_STATS_NTH_SECOND=n Print terse statistics every <n>th second.\n");
            NvOsDebugPrintf(RESTRACKER_TAG "RT_STACKTRACE=X       Show stacktrace. X is a string beginning with\n");
            NvOsDebugPrintf(RESTRACKER_TAG "                      'H' for hex stack and 'S' for symbol stack\n");
            NV_ASSERT(!"ResourceTracker usage printed!");
        }
    }

    /* Init tracker explicitly. */

    s_tracker.resourceCounter      = 1;

    s_tracker.MetaStore            = PointerHashCreate(NVOS_META_STORE_INIT_SIZE);
    s_tracker.FilenameStore        = PointerHashCreate(NVOS_FILENAME_STORE_INIT_SIZE);
    NV_ASSERT(s_tracker.MetaStore);

#if NVOS_IS_WINDOWS
    s_tracker.verbose           = (NvOsGetConfigU32("RT_VERBOSE", &tmp) == NvError_Success) ? tmp : 1;
#else
    s_tracker.verbose           = (NvOsGetConfigU32("RT_VERBOSE", &tmp) == NvError_Success) ? tmp : 0;
#endif
    s_tracker.stats             = (NvOsGetConfigU32("RT_STATS", &tmp) == NvError_Success) ? tmp : 0;
    s_tracker.statsNthAlloc     = (NvOsGetConfigU32("RT_STATS_NTH_ALLOC", &tmp) == NvError_Success) ? tmp : 0;
    s_tracker.statsNthSecond    = (NvOsGetConfigU32("RT_STATS_NTH_SECOND", &tmp) == NvError_Success) ? (1000 * tmp) : 0;
    s_tracker.breakId           = (NvOsGetConfigU32("RT_BREAK", &tmp) == NvError_Success) ? tmp : 0;
    s_tracker.breakStackId      = 0;
    s_tracker.callstackType     = NvOsCallstackType_NoStack;
    s_tracker.stackFilterPid    = 0;
    s_tracker.dumpOnExit        = NV_TRUE;

    NvOsMemset(&(s_tracker.statistics[0]), 0, sizeof(s_tracker.statistics));

    s_tracker.lastDumpTime      = NvOsGetTimeMS();
    s_tracker.lastDumpResourceId= s_tracker.statsNthAlloc;
    s_tracker.numResources      = 0;
    s_tracker.dumpOut           = NULL;

    s_tracker.FreeAllocChunks   = NULL;
    s_tracker.FreeRefCountChunks= NULL;

    /* Configure multiple-choice settings here. */

    if (NvOsGetConfigString("RT_STACKTRACE", str, 2) == NvError_Success)
    {
        if (str[0] == 'h')
            s_tracker.callstackType = NvOsCallstackType_HexStack;
        else if (str[0] == 's')
            s_tracker.callstackType = NvOsCallstackType_SymbolStack;
    }

    /* Bump resourceId up as 0 means uninitialized */
    s_tracker.resourceCounter++;

    s_tracker.enabled           = enabled;
}

/*
 * Called in the end by main thread only.
 */

void NvOsDeinitResourceTracker(void)
{
    NvBool was_enabled = s_tracker.enabled;
    NV_ASSERT(s_tracker.mutex);

    /* Destroy mutex and exit if disabled. */

    s_tracker.enabled = NV_FALSE;
    NvOsMutexDestroyInternal(s_tracker.mutex);

    /* Normally just exit, possibly printing stats. */

    if (s_tracker.dumpOnExit)
    {
        if (was_enabled && s_tracker.numResources == 0)
        {
            if (s_tracker.verbose)
                NvOsDebugPrintf(RESTRACKER_TAG "Exiting: No leaks found!\n");
            if (s_tracker.stats || s_tracker.statsNthAlloc || s_tracker.statsNthSecond)
                DumpStatsVerbose();
        }
        else if (was_enabled)
        {
            /* If we had leaks, print them but don't free them. */

            if (s_tracker.numResources != 0)
            {
                NvOsDebugPrintf("\n\n" RESTRACKER_TAG "Leaked %ld resources!\n", s_tracker.numResources);
                DumpResources(0, (NvU32)-1, NV_FALSE, NV_FALSE);
            }

            NV_ASSERT(s_tracker.statistics[NvOsResTrackerStat_NumAllocs] != 0 ||
                      s_tracker.statistics[NvOsResTrackerStat_NumBytes] == 0);

            /* Summarize with stats. */

            DumpStatsVerbose();
        }
    }

    PointerHashDestroy(s_tracker.MetaStore);
    {
        void* str;
        NV_ASSERT(s_tracker.FilenameStore);
        PointerHashIterate(s_tracker.FilenameStore);
        while (PointerHashNext(s_tracker.FilenameStore, NULL, &str))
        {
            if (str != s_filenameAllocFailed)
                NvOsFreeInternal(str);
        }
    }
    PointerHashDestroy(s_tracker.FilenameStore);
    NvOsMemset(&s_tracker, 0, sizeof(s_tracker));
}

/*
 * Handle userptr-to-Resource mapping.
 */

ResourceMeta* GetResourceMeta(NvU8* userptr)
{
    ResourceMeta* res;
    NVOS_ASSERT(s_tracker.MetaStore);

    if (!PointerHashFind(s_tracker.MetaStore, (void*)userptr, (void**)&res))
        return NULL;

    return res;
}


/*
 * Allocate a new resource tracking meta info struct
 */

ResourceMeta* AllocateResourceMeta(NvOsResourceType type)
{
    ResourceMeta*       meta;
    size_t              MetaSize = sizeof(RefCountResourceMeta);
    ResourceMetaChunk** FreeList = &s_tracker.FreeRefCountChunks;

    NVOS_ASSERT(s_tracker.resourceCounter > 0);

    if (type == NvOsResourceType_NvOsAlloc)
    {
        MetaSize = sizeof(AllocResourceMeta);
        FreeList = &s_tracker.FreeAllocChunks;
    }

    if (!*FreeList)
    {
        size_t             size     = sizeof(ResourceMetaChunk) + MetaSize * NVOS_CHUNK_ITEMS;
        ResourceMetaChunk* NewChunk = NvOsAllocInternal(size);
        int                i;
        ResourceMeta*      TempMeta;

        if (!NewChunk)
            return NULL;

        NvOsMemset(NewChunk, 0, size);

        IncResTrackerStatInternal(NvOsResTrackerStat_TrackerBytes,
                                  size,
                                  NvOsResTrackerStat_TrackerPeakBytes);

        TempMeta = (ResourceMeta*)((NvUPtr)NewChunk + sizeof(ResourceMetaChunk));

        // Walk through the items and setup the free list.
        // Offset in bytes to next free item is in meta->userptr.
        for (i = 0; i < NVOS_CHUNK_ITEMS; i++)
        {
            NVOS_ASSERT(((NvUPtr)TempMeta) - (NvUPtr)NewChunk < 0x7FFF);
            TempMeta->ChunkOffset = (NvU16)((NvUPtr)TempMeta - (NvUPtr)NewChunk);
            // Set the offset to next free item to userptr
            if (i == NVOS_CHUNK_ITEMS - 1)
                TempMeta->userptr = 0;
            else
                TempMeta->userptr = (void*) MetaSize;
            TempMeta = (ResourceMeta*)((NvUPtr)TempMeta + MetaSize);
        }

        NewChunk->ItemSize = MetaSize;
        NewChunk->FirstFree = sizeof(ResourceMetaChunk);
        *FreeList = NewChunk;
    }

    NVOS_ASSERT(*FreeList);
    NVOS_ASSERT((*FreeList)->NumUsed < NVOS_CHUNK_ITEMS);
    NVOS_ASSERT((*FreeList)->FirstFree >= 0);
    NVOS_ASSERT((*FreeList)->FirstFree < (NVOS_CHUNK_ITEMS * (*FreeList)->ItemSize) +
                (NvS32)sizeof(ResourceMetaChunk));

    meta = (ResourceMeta*)((NvUPtr)*FreeList + (*FreeList)->FirstFree);
    (*FreeList)->FirstFree += (NvS32)meta->userptr;
    (*FreeList)->NumUsed++;

    // Check that the FirstFree moves, unless chunk is exhausted.
    NVOS_ASSERT((*FreeList)->NumUsed >= NVOS_CHUNK_ITEMS || (NvS32)meta->userptr != 0);

    if ((*FreeList)->NumUsed >= NVOS_CHUNK_ITEMS)
    {
        NVOS_ASSERT(!(*FreeList)->prev);
        (*FreeList)->FirstFree = -1;
        if ((*FreeList)->next)
            (*FreeList)->next->prev = NULL;
        *FreeList = (*FreeList)->next;
    }

    meta->userptr = NULL;
    meta->id      = s_tracker.resourceCounter++;
    meta->type    = (NvU8)type;

    s_tracker.numResources++;

    return meta;
}

/*
 * Mark the allocation resource meta info as free, allowing reuse. Swap resources to
 * keep resource lists filled from start, if necessary.
 */

void DeallocateResourceMeta(ResourceMeta* meta)
{
    NvOsResourceType type = GetResourceType(meta);
    int i;

    if (type != NvOsResourceType_NvOsAlloc)
    {
        RefCountResourceMeta* refCountMeta = (RefCountResourceMeta*)meta;
        NVOS_ASSERT(refCountMeta->RefCount == 0);

        for (i = 0; i < MAX_ALLOC_LOCATIONS; i++)
        {
            if (refCountMeta->Locations[i].callstack != NULL)
            {
                NvOsCallstackDestroy(refCountMeta->Locations[i].callstack);
                refCountMeta->Locations[i].callstack = NULL;
            }
        }
    }
    else
    {
        NvOsCallstackDestroy(((AllocResourceMeta*)meta)->loc.callstack);
        ((AllocResourceMeta*)meta)->loc.callstack = NULL;
    }

    s_tracker.numResources--;

    {
        ResourceMetaChunk* chunk = (ResourceMetaChunk*) ((NvUPtr)meta - meta->ChunkOffset);

        if (chunk->NumUsed == 1)
        {
            // Chunk becomes empty, delete it.
            if (chunk->next)
                chunk->next->prev = chunk->prev;

            if (type == NvOsResourceType_NvOsAlloc &&
                s_tracker.FreeAllocChunks == chunk)
                s_tracker.FreeAllocChunks = chunk->next;
            else if (s_tracker.FreeRefCountChunks == chunk)
                s_tracker.FreeRefCountChunks = chunk->next;
            else if (chunk->prev)
                chunk->prev->next = chunk->next;

            s_tracker.statistics[NvOsResTrackerStat_TrackerBytes] -= sizeof(ResourceMetaChunk) +
                + chunk->ItemSize * NVOS_CHUNK_ITEMS;
            NvOsFreeInternal(chunk);
            return;
        }
        else if (chunk->NumUsed == NVOS_CHUNK_ITEMS)
        {
            // Chunk has now one free entry, add chunk to free list.
            ResourceMetaChunk** FreeList = &s_tracker.FreeRefCountChunks;
            if (type == NvOsResourceType_NvOsAlloc)
                FreeList = & s_tracker.FreeAllocChunks;

            chunk->next = *FreeList;
            if (chunk->next)
                chunk->next->prev = chunk;
            *FreeList = chunk;
        }
        chunk->NumUsed--;
        // Compute offset to previous head of free list and store to userptr.
        meta->userptr = (void*) (chunk->FirstFree - (NvS32)((NvUPtr)meta - (NvUPtr)chunk));
        chunk->FirstFree = (NvS32)((NvUPtr)meta - (NvUPtr)chunk);
    }
}

/*
 * Returns NV_TRUE if the internal integrity of an allocation resource is
 * sane.
 */

NvBool VerifyResource(ResourceMeta* meta)
{
    ResourceMeta* tmp;
    NV_ASSERT(meta);

    if (TestResourceFlag(meta, ResourceMetaFlag_IsCorrupt))
        return NV_FALSE;

    tmp = GetResourceMeta(meta->userptr);
    if (tmp && tmp == meta)
        return NV_TRUE;

    return NV_FALSE;
}


/*
 * Try to inform the user about a pointer gone bad.
 */

void ReportCorruption(NvU8* userptr, const char* filename, int line)
{
    ResourceMeta* meta;

    NvOsDebugPrintf("\n\n" RESTRACKER_TAG "Malloc error: broken integrity on ptr=%p\n", userptr);
    if (filename)
        NvOsDebugPrintf(RESTRACKER_TAG "Traced from function call on %s:%d\n", filename, line);

    meta = GetResourceMeta(userptr);

    if (meta)
    {
        NvOsDebugPrintf(RESTRACKER_TAG "Trashed block: ");
        DumpResource(meta);
    }
    else
        NvOsDebugPrintf(RESTRACKER_TAG "Malloc error: can't print metadata! Internal metadata is totally FUBAR!\n");

    NV_ASSERT(!"Critical error: Halting!");
}

/*
 * Dump high-level memory usage statistics on a single line.
 */

void DumpStats(void)
{
    NvS64 PeakBytes = NvOsGetResTrackerStat(NvOsResTrackerStat_NumPeakBytes);
    NvS64 TrackerPeakBytes = NvOsGetResTrackerStat(NvOsResTrackerStat_TrackerPeakBytes);

    NvS64 percent_int = 0;
    NvS64 percent_frac = 0;

    if (PeakBytes > 0)
    {
        percent_int  = (((10000 * TrackerPeakBytes) / PeakBytes) / 100);
        percent_frac = (((10000 * TrackerPeakBytes) / PeakBytes) % 100);
    }

    NvOsDebugPrintf(RESTRACKER_TAG "allocs: %-10lld allocs_peak: %-10lld alloc_bytes: %-10lld alloc_bytes_peak: %-10lld internal_overhead%% %lld.%02lld\n",
                    NvOsGetResTrackerStat(NvOsResTrackerStat_NumAllocs),
                    NvOsGetResTrackerStat(NvOsResTrackerStat_NumPeakAllocs),
                    NvOsGetResTrackerStat(NvOsResTrackerStat_NumBytes),
                    NvOsGetResTrackerStat(NvOsResTrackerStat_NumPeakBytes),
                    percent_int, percent_frac);
}

/*
 * Dump high-level memory usage statistics in human-readable format.
 */

void DumpStatsVerbose()
{
    NvS64 PeakBytes = NvOsGetResTrackerStat(NvOsResTrackerStat_NumPeakBytes);
    NvS64 TrackerPeakBytes = NvOsGetResTrackerStat(NvOsResTrackerStat_TrackerPeakBytes);

    NvS64 percent_int = 0;
    NvS64 percent_frac = 0;

    if (PeakBytes > 0)
    {
        percent_int  = (((10000 * TrackerPeakBytes) / PeakBytes) / 100);
        percent_frac = (((10000 * TrackerPeakBytes) / PeakBytes) % 100);
    }

#if NVOS_IS_WINDOWS
    // Debug print is somehow broken on windows, multiple parameters are handled wrong.
    NvOsDebugPrintf(RESTRACKER_TAG "\n");
    NvOsDebugPrintf(RESTRACKER_TAG "Summary:\n");
    NvOsDebugPrintf(RESTRACKER_TAG "\n");
    NvOsDebugPrintf(RESTRACKER_TAG "    Last allocation ID        : #%lu\n", s_tracker.resourceCounter - 1);
    NvOsDebugPrintf(RESTRACKER_TAG "    Blocks allocated (peak)   : %lld",
                    NvOsGetResTrackerStat(NvOsResTrackerStat_NumAllocs));
    NvOsDebugPrintf(" (%lld)\n",
                    NvOsGetResTrackerStat(NvOsResTrackerStat_NumPeakAllocs));
    NvOsDebugPrintf(RESTRACKER_TAG "    Bytes allocated (peak)    : %lld",
                    NvOsGetResTrackerStat(NvOsResTrackerStat_NumBytes));
    NvOsDebugPrintf(" (%lld)\n",
                    NvOsGetResTrackerStat(NvOsResTrackerStat_NumPeakBytes));
    NvOsDebugPrintf(RESTRACKER_TAG "    Internal overhead (bytes) : %lld",
                    NvOsGetResTrackerStat(NvOsResTrackerStat_TrackerBytes));
    NvOsDebugPrintf(" (peak %lld or ",
                    NvOsGetResTrackerStat(NvOsResTrackerStat_TrackerPeakBytes));
    NvOsDebugPrintf("%lu.", percent_int);
    NvOsDebugPrintf("%02lu%% of program peak allocation)\n",percent_frac);
#else
    NvOsDebugPrintf(RESTRACKER_TAG "\n");
    NvOsDebugPrintf(RESTRACKER_TAG "Summary:\n");
    NvOsDebugPrintf(RESTRACKER_TAG "\n");
    NvOsDebugPrintf(RESTRACKER_TAG "    Last allocation ID        : #%lu\n", s_tracker.resourceCounter - 1);
    NvOsDebugPrintf(RESTRACKER_TAG "    Blocks allocated (peak)   : %lld (%lld)\n",
                    NvOsGetResTrackerStat(NvOsResTrackerStat_NumAllocs),
                    NvOsGetResTrackerStat(NvOsResTrackerStat_NumPeakAllocs));
    NvOsDebugPrintf(RESTRACKER_TAG "    Bytes allocated (peak)    : %lld (%lld)\n",
                    NvOsGetResTrackerStat(NvOsResTrackerStat_NumBytes),
                    NvOsGetResTrackerStat(NvOsResTrackerStat_NumPeakBytes));
    NvOsDebugPrintf(RESTRACKER_TAG "    Internal overhead (bytes) : %lld (peak %lld or %lu.%02lu%% of program peak allocation)\n",
                    NvOsGetResTrackerStat(NvOsResTrackerStat_TrackerBytes),
                    NvOsGetResTrackerStat(NvOsResTrackerStat_TrackerPeakBytes),
                    percent_int, percent_frac);
#endif
}

/*
 * Dump all allocations in effect in checkpoint range [start,end[.
 * Return number of leaks.
 */

NvU32 DumpResources(NvU32 start, NvU32 end, NvBool only_corrupted, NvBool filterOnPid)
{
    NvU32          count = 0;
    void*          userptr;
    ResourceMeta*  meta;
    ResourceMeta** ResourceList;
    int            NumItems;
    int            i;

    NV_ASSERT(s_tracker.MetaStore);

    NumItems = PointerHashSize(s_tracker.MetaStore);
    if (!NumItems)
        return NumItems;

    // Take copy of the alloc list so that call stack printing can use all nvos
    // services normally.
    ResourceList = NvOsAllocInternal(NumItems * sizeof(ResourceMeta*));
    if (!ResourceList)
    {
        NvOsResTrackerPrintf(RESTRACKER_TAG "!! Not enough memory for dumping leaks !!\n");
        return count;
    }
    PointerHashIterate(s_tracker.MetaStore);
    while (PointerHashNext(s_tracker.MetaStore, &userptr, (void**)&meta) &&
           count < (NvU32)NumItems)
        ResourceList[count++] = meta;

    PointerHashStopIter(s_tracker.MetaStore);

    NVOS_ASSERT((NvU32)NumItems == count);

    // Actual analysis of the list
    NumItems = count;
    count = 0;
    for (i = 0; i < NumItems; i++)
    {
        meta = ResourceList[i];
        NVOS_ASSERT(meta);
        if (ActivityInInterval(meta, start, end))
        {
            if (filterOnPid && s_tracker.stackFilterPid != 0)
            {
                NvBool match = NV_FALSE;

                if (GetResourceType(meta) == NvOsResourceType_NvOsAlloc)
                {
                    match = NvOsCallstackContainsPid(((AllocResourceMeta*)meta)->loc.callstack,
                                                     s_tracker.stackFilterPid);
                }
                else
                {
                    int i;
                    /* Filter out resources that don't contain process pid in their callstacks */
                    for (i = 0; i < MAX_ALLOC_LOCATIONS; i++)
                    {
                        if (NvOsCallstackContainsPid(((RefCountResourceMeta*)meta)->Locations[i].callstack,
                                                     s_tracker.stackFilterPid))
                            match = NV_TRUE;
                    }
                }

                if (!match)
                    continue;
            }

            // TODO [kraita 27/Nov/08] Userptr is likely to pass verify test in any case?
            if (!only_corrupted || !VerifyResource(userptr))
            {
                if (only_corrupted)
                    NvOsResTrackerPrintf(RESTRACKER_TAG "TRASHED: ");
                else
                    NvOsResTrackerPrintf(RESTRACKER_TAG "%s ", start == 0 ? "LEAK" : "POSSIBLE LEAK");

                DumpResource(meta);
                count++;
            }
        }
    }

    NvOsFreeInternal(ResourceList);

    return count;
}

static
void ResTrackerDumpCallback(void* context, const char* line)
{
    NvOsResTrackerPrintf(RESTRACKER_TAG "%s\n", line);
}

/*
 * Dump metadata from one allocation resource.
 */

void DumpResource(ResourceMeta* meta)
{
    /* Print basic info. */

    NvOsResourceType  type        = GetResourceType(meta);
    const char*       TypeString  = ResourceTypeToString(type);
    NvBool            corrupt;

    NVOS_ASSERT(meta);

    corrupt = !VerifyResource(meta);

    /* Write out common fields for all types */

    if (type == NvOsResourceType_NvOsAlloc)
    {
        AllocResourceMeta* allocMeta = (AllocResourceMeta*)meta;
        NvU32              stackId;
        ResourceAllocLoc*  loc       = &allocMeta->loc;
        size_t             allocSize = NvOsGetAllocSize(meta->userptr);

        stackId = NvOsCallstackHash(loc->callstack);

        NvOsResTrackerPrintf("id=%010lu  stackid=0x%08x  ptr=0x%08x  type=%s  size=%d  src=%s:%d ",
                     meta->id,
                     stackId,
                     meta->userptr,
                     TypeString,
                     allocSize,
                     loc->filename,
                     loc->line);

        /* Label corrupted allocs separately: if things are totally messed up
         * we don't want to trust that VerifyResource() wouldn't segfault. */

        if (corrupt)
            NvOsResTrackerPrintf(" CORRUPTED!");

        NvOsResTrackerPrintf("\n");

        /* \todo [smelenius 03/Dec/08] Are you sure about this? Hex stacks
         * might be useful for logging/automated testing as generally the
         * addresses can also be resolved later. */
        if (s_tracker.callstackType > NvOsCallstackType_HexStack)
            NvOsCallstackDump(loc->callstack, 0, ResTrackerDumpCallback, NULL);
    }
    else
    {
        RefCountResourceMeta* refCountMeta = (RefCountResourceMeta*)meta;
        int                   i            = 0;

        NvOsResTrackerPrintf("id=%010lu  ptr=0x%08x  type=%s  references=%d ",
                     meta->id,
                     meta->userptr,
                     TypeString,
                     refCountMeta->RefCount);

        /* Label corrupted allocs separately: if things are totally messed up
         * we don't want to trust that VerifyResource() wouldn't segfault. */

        if (corrupt)
            NvOsResTrackerPrintf(" CORRUPTED!");

        NvOsResTrackerPrintf("\n");
        NvOsResTrackerPrintf("   Max %d recent references (Most recent first)\n", MAX_ALLOC_LOCATIONS);

        {
            int                   CurLocation  = refCountMeta->CurLocation - 1;
            ResourceAllocLoc*     loc;

            if (CurLocation < 0)
                CurLocation = MAX_ALLOC_LOCATIONS - 1;

            NVOS_ASSERT(CurLocation >= 0 && CurLocation < MAX_ALLOC_LOCATIONS);

            loc = &refCountMeta->Locations[CurLocation];

            for (i = 0; i < MAX_ALLOC_LOCATIONS; i++)
            {
                if (loc->line > 0 || loc->callstack)
                {
                    NvOsResTrackerPrintf("   Ref %d  stackid=0x%08x  src=%s:%d \n",
                                 i+1,
                                 NvOsCallstackHash(loc->callstack),
                                 loc->filename,
                                 loc->line);

                    /* \todo [smelenius 03/Dec/08] Are you sure about this? Hex stacks
                     * might be useful for logging/automated testing as generally the
                     * addresses can also be resolved later. */
                    if (s_tracker.callstackType > NvOsCallstackType_HexStack)
                        NvOsCallstackDump(loc->callstack, 0, ResTrackerDumpCallback, NULL);
                }

                CurLocation--;
                if (CurLocation < 0)
                    CurLocation = MAX_ALLOC_LOCATIONS - 1;

                loc = &refCountMeta->Locations[CurLocation];
            }
        }
    }
}

/*
 *
 */

const char* ResourceTypeToString(NvOsResourceType type)
{
    switch (type)
    {
        case NvOsResourceType_NvOsAlloc:
            return "NvOsAlloc";

        case NvOsResourceType_NvOsLibrary:
            return "NvOsLibrary";

        case NvOsResourceType_NvOsMutex:
            return "NvOsMutex";

        case NvOsResourceType_NvOsSemaphore:
            return "NvOsSemaphore";

        case NvOsResourceType_NvOsSharedMemMap:
            return "NvOsSharedMemMap";

        case NvOsResourceType_NvOsSharedMemAlloc:
            return "NvOsSharedMemAlloc";

        case NvOsResourceType_NvOsExecAlloc:
            return "NvOsExecAlloc";

        case NvOsResourceType_NvOsVirtualAlloc:
            return "NvOsVirtualAlloc";

        case NvOsResourceType_NvOsFile:
            return "NvOsFile";

        case NvOsResourceType_NvOsThread:
            return "NvOsThread";

        case NvOsResourceType_NvOsEvent:
            return "NvOsEvent";

        case NvOsResourceType_NvOsPageAlloc:
            return "NvOsPageAlloc";

        case NvOsResourceType_NvOsPhysicalMemMap:
            return "NvOsPhysicalMemMap";

        default:
            NVOS_ASSERT(!"Invalid case in switch statement");
            return "Unknown";
    }

}

/*
 * Check if resource tracking is currently on or not.
 */
NvBool NvOsResourceTrackingEnabled(void)
{
    return s_tracker.enabled;
}

/*
 * Create a tracker resource meta data struct for given resource.
 */
NvBool NvOsTrackResource(NvOsResourceType type, void* userptr, const char* filename, int line)
{
    ResourceMeta* meta;

    NV_ASSERT(userptr && "Cannot track NULL pointers");
    NV_ASSERT(filename);
    NV_ASSERT(s_tracker.resourceCounter > 0); // 0 means non-initialized

    NvOsMutexLockInternal(s_tracker.mutex);

    if (!PointerHashFind(s_tracker.MetaStore, userptr, (void**)&meta))
    {
        meta = AllocateResourceMeta(type);
        if (meta && !PointerHashInsert(s_tracker.MetaStore, userptr, (void*)meta))
        {
            DeallocateResourceMeta(meta);
            meta = NULL;
        }
    }

    if (meta)
    {
        ResourceAllocLoc* loc;

        // Alloc can't have previous userptr
        NVOS_ASSERT(GetResourceType(meta) != NvOsResourceType_NvOsAlloc ||
                    meta->userptr == NULL);
        // Ref counted objects must keep the same userptr.
        NVOS_ASSERT(GetResourceType(meta) == NvOsResourceType_NvOsAlloc ||
                    (((RefCountResourceMeta*)meta)->RefCount == 0 ||
                     meta->userptr == userptr));

        meta->userptr = userptr;
        IncRefCount(meta);
        loc = GetNewLocation(meta);

        loc->line = line;
        if (loc->callstack)
            NvOsCallstackDestroy(loc->callstack);
        loc->callstack = NvOsCallstackCreate(s_tracker.callstackType);

        /* Break if breakId matches. */
        if (s_tracker.breakId == meta->id)
            NV_OS_BREAK_POINT();

        /* Break if breakStackId matches */
        if (s_tracker.breakStackId)
        {
            if (s_tracker.breakStackId == NvOsCallstackHash(loc->callstack))
                NV_OS_BREAK_POINT();
        }

        loc->filename = GetFilenameStringCopy(filename);
        NVOS_ASSERT(loc->filename);
    }

    /* Automatically dump stats if enough time/allocs have
     * passed. */
    // TODO [kraita 24/Nov/08] Create function for dump check.
    if ((s_tracker.statsNthSecond > 0 && (NvOsGetTimeMS() - s_tracker.lastDumpTime >= s_tracker.statsNthSecond)) ||
        (s_tracker.statsNthAlloc > 0 && (s_tracker.resourceCounter - s_tracker.lastDumpResourceId >= s_tracker.statsNthAlloc)))
    {
        s_tracker.lastDumpTime      = NvOsGetTimeMS();
        s_tracker.lastDumpResourceId   = s_tracker.resourceCounter;
        DumpStats();
    }

    NVOS_ASSERT(!meta || PointerHashFind(s_tracker.MetaStore, userptr, NULL));
    NvOsMutexUnlockInternal(s_tracker.mutex);

    return (meta != NULL);
}

void NvOsReleaseTrackedResource(void* userptr)
{
    ResourceMeta* meta;

    NvOsMutexLockInternal(s_tracker.mutex);

    meta = GetResourceMeta(userptr);
    if (meta)
    {
        /* Break if breakId matches. */
        if (s_tracker.breakId == meta->id)
            NV_OS_BREAK_POINT();

        if (!DecRefCount(meta))
        {
            PointerHashRemove(s_tracker.MetaStore, userptr, NULL);

            DeallocateResourceMeta(meta);
            NVOS_ASSERT(!GetResourceMeta(userptr));
        }
    }
    else
    {
        ReportCorruption(userptr, "", 666);
    }

    NvOsMutexUnlockInternal(s_tracker.mutex);
}

// Weird: Can be called with OldPointer == NewPointer in order to
// let tracker do a break check.
void NvOsUpdateResourcePointer(void* OldPointer, void* NewPointer)
{
    ResourceMeta* meta = NULL;

    NvOsMutexLockInternal(s_tracker.mutex);

    NV_ASSERT(!GetResourceMeta(NewPointer) || OldPointer == NewPointer);

    meta = GetResourceMeta(OldPointer);

    if (meta)
    {
        if (NewPointer != OldPointer)
        {
            meta->userptr = NewPointer;
            // Remove-insert guaranteed to succeed.
            (void)PointerHashRemove(s_tracker.MetaStore, OldPointer, (void*)meta);
            (void)PointerHashInsert(s_tracker.MetaStore, NewPointer, (void*)meta);
        }
        if (s_tracker.breakId == meta->id)
            NV_OS_BREAK_POINT();
    }
    else
    {
        ReportCorruption(OldPointer, "", 777);
    }

    NvOsMutexUnlockInternal(s_tracker.mutex);
}

void NvOsResourceIsCorrupt(void* userptr)
{
    ResourceMeta* meta;
    NvOsMutexLockInternal(s_tracker.mutex);

    meta = GetResourceMeta(userptr);
    if (meta)
        SetResourceFlags(meta, ResourceMetaFlag_IsCorrupt);

    NvOsMutexUnlockInternal(s_tracker.mutex);
}

NvBool NvOsIsTrackedResource(void* userptr)
{
    NvBool res;
    NvOsMutexLockInternal(s_tracker.mutex);
    res = (GetResourceMeta(userptr) != NULL);
    NvOsMutexUnlockInternal(s_tracker.mutex);
    return res;
}


void IncResTrackerStatInternal(NvOsResTrackerStat stat, NvS64 inc, NvOsResTrackerStat peak)
{
    NV_ASSERT((NvS32)stat > 0 && stat < NvOsResTrackerStat_MaxStat);
    NV_ASSERT((NvS32)peak > 0);

    s_tracker.statistics[stat] += inc;

    NV_ASSERT(s_tracker.statistics[stat] >= 0);

    if (peak < NvOsResTrackerStat_MaxStat)
        s_tracker.statistics[peak] = NV_MAX(s_tracker.statistics[peak], s_tracker.statistics[stat]);
}

// stat = stat + inc; peak = max(peak, stat). Set peak to MaxStat to skip peak.
// TODO [kraita 24/Nov/08] Optimize with Atomic XYZ ops for wince
void NvOsIncResTrackerStat(NvOsResTrackerStat stat, NvS64 inc, NvOsResTrackerStat peak)
{
    NvOsMutexLockInternal(s_tracker.mutex);

    IncResTrackerStatInternal(stat, inc, peak);

    NvOsMutexUnlockInternal(s_tracker.mutex);
}

// stat = stat - dec.
// TODO [kraita 24/Nov/08] Optimize with Atomic XYZ ops for wince
void NvOsDecResTrackerStat(NvOsResTrackerStat stat, NvS64 dec)
{
    NV_ASSERT((NvS32)stat > 0 && stat < NvOsResTrackerStat_MaxStat);

    NvOsMutexLockInternal(s_tracker.mutex);

    s_tracker.statistics[stat] -= dec;

    NV_ASSERT(s_tracker.statistics[stat] >= 0);

    NvOsMutexUnlockInternal(s_tracker.mutex);
}

void NvOsResTrackerSetConfig(NvOsResTrackerConfig configItem, NvU32 value)
{
    NvOsMutexLockInternal(s_tracker.mutex);

    switch (configItem)
    {
        case NvOsResTrackerConfig_Enabled:
            s_tracker.enabled = (value != 0);
            break;

        case NvOsResTrackerConfig_BreakId:
            s_tracker.breakId = value;
            break;

        case NvOsResTrackerConfig_BreakStackId:
            s_tracker.breakStackId = value;
            break;

        case NvOsResTrackerConfig_CallstackType:
            NV_ASSERT(value > 0 && value < NvOsCallstackType_Last);
            s_tracker.callstackType = (NvOsCallstackType)value;
            break;

        case NvOsResTrackerConfig_StackFilterPid:
            s_tracker.stackFilterPid = value;
            break;

        case NvOsResTrackerConfig_DumpOnExit:
            s_tracker.dumpOnExit = (value != 0);
            break;

        default:
            NV_ASSERT(!"Invalid case in switch statement");
            break;
    }

    NvOsMutexUnlockInternal(s_tracker.mutex);
}

// Lock resource tracker mutex. Assumes initialized resource tracker.
void NvOsLockResourceTracker(void)
{
    NV_ASSERT(s_tracker.mutex);
    NvOsMutexLockInternal(s_tracker.mutex);
}

// Release resource tracker mutex. Assumes initialized resource tracker.
void NvOsUnlockResourceTracker(void)
{
    NV_ASSERT(s_tracker.mutex);
    NvOsMutexUnlockInternal(s_tracker.mutex);
}

/**
 * Store a copy of file name to hash table. Function has been
 * simplified by assuming that equality comes from pointer of
 * the filename, not actual string.
 *
 * Can return s_filenameAllocFailed, if cannot allocate new string.
 *
 */
const char* GetFilenameStringCopy(const char* filename)
{
    char* rv = NULL;
    if (!PointerHashFind(s_tracker.FilenameStore, (void*)filename, (void**)&rv))
    {
        size_t len = NV_MIN(NvOsStrlen(filename), MAX_FILENAME_CHARS);
        rv = (char*) NvOsAllocInternal(len + 1);
        if (!rv)
            return s_filenameAllocFailed;

        if (!PointerHashInsert(s_tracker.FilenameStore, (void*)filename, (void*)rv))
        {
            NvOsFreeInternal(rv);
            return s_filenameAllocFailed;
        }
        NvOsStrncpy(rv, filename, len);
        IncResTrackerStatInternal(NvOsResTrackerStat_TrackerBytes, len+1,
                                  NvOsResTrackerStat_TrackerPeakBytes);
        rv[len] = '\0';
    }
    NVOS_ASSERT(rv);
    return rv;
}

NvBool NvOsAllocSucceedsSim(NvOsAllocHookType AllocType)
{
    if (s_tracker.AllocSimCB)
        return s_tracker.AllocSimCB(AllocType, s_tracker.AllocSimUserContext);
    return NV_TRUE;
}

void NvOsResTrackerPrintf(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    if (s_tracker.dumpOut)
    {
        NvOsVfprintf(s_tracker.dumpOut, format, ap);
    }
    else
    {
        NvOsDebugVprintf(format, ap);
    }
    va_end(ap);
}

#endif // NVOS_RESTRACKER_COMPILED

/*
 *
 */

NvU32 NvOsCheckpoint(void)
{
#if NVOS_RESTRACKER_COMPILED
    return s_tracker.resourceCounter;
#else
    return 0x7FFFFFFF;
#endif
}

/*
 *
 */

NvBool NvOsDumpSnapshot(NvU32 start, NvU32 end, NvOsFileHandle out)
{
#if NVOS_RESTRACKER_COMPILED
    NvBool rv;
    char procInfo[256];

    NvOsMutexLockInternal(s_tracker.mutex);

    s_tracker.dumpOut = out;
    NvOsGetProcessInfo(procInfo, 256);
    if (!out)
        NvOsDebugPrintf(RESTRACKER_TAG "==================== Resource dump start, %s ====================\n", procInfo);
    rv = (DumpResources(start, end, NV_FALSE, NV_TRUE) == 0);
    if (!out && rv && (s_tracker.stats || s_tracker.verbose))
    {
        if (s_tracker.verbose)
            NvOsDebugPrintf(RESTRACKER_TAG "NO leaks found!\n");
        if (s_tracker.stats)
            DumpStats();
    }
    if (!out)
        NvOsDebugPrintf(RESTRACKER_TAG "==================== Resource dump end ====================\n\n");

    s_tracker.dumpOut = NULL;
    NvOsMutexUnlockInternal(s_tracker.mutex);

    return rv;
#else
    return NV_TRUE;
#endif
}

/*
 *
 */

void NvOsVerifyHeap(void)
{
#if NVOS_RESTRACKER_COMPILED
    NvOsMutexLockInternal(s_tracker.mutex);
    {
        void*         userptr;
        ResourceMeta* meta;

        NV_ASSERT(s_tracker.MetaStore);

        PointerHashIterate(s_tracker.MetaStore);
        while (PointerHashNext(s_tracker.MetaStore, &userptr, (void**)&meta))
        {
            // TODO [kraita 01/Dec/08] Verify resource integrity as per resource type.
            if (!VerifyResource(meta))
            {
                NvOsDebugPrintf("\n\n" RESTRACKER_TAG "Malloc error: heap integrity broken\n");
                DumpResources(0, (NvU32)-1, NV_TRUE, NV_FALSE);
                NV_ASSERT(!"Critical error: Halting!");
            }
        }
    }
    NvOsMutexUnlockInternal(s_tracker.mutex);
#endif
}

NvS64 NvOsGetResTrackerStat(NvOsResTrackerStat stat)
{
#if NVOS_RESTRACKER_COMPILED
    // TODO [kraita 24/Nov/08] Replace assert with debug print? Makes
    // adding enums & changing versions less of a pain.
    NV_ASSERT(stat > 0 && stat < NvOsResTrackerStat_MaxStat);

    return s_tracker.statistics[stat];
#else
    return 0;
#endif
}

void NvOsSetAllocHook(NvOsAllocSimCB Hook, void* UserContext)
{
#if NVOS_RESTRACKER_COMPILED
    s_tracker.AllocSimUserContext = UserContext;
    s_tracker.AllocSimCB          = Hook;
#endif
}

void NvOsSetResourceAllocFileLine(void* userptr, const char* filename, int line)
{
#if NVOS_RESTRACKER_COMPILED
    if (!s_tracker.resourceCounter || !s_tracker.mutex || !s_tracker.MetaStore)
        return;
    NVOS_ASSERT(filename);
    NvOsMutexLockInternal(s_tracker.mutex);
    {
        ResourceMeta* meta;
        if (PointerHashFind(s_tracker.MetaStore, userptr, (void**)&meta))
        {
            ResourceAllocLoc* loc;

            if (GetResourceType(meta) == NvOsResourceType_NvOsAlloc)
            {
                loc = &((AllocResourceMeta*)meta)->loc;
            }
            else
            {
                RefCountResourceMeta* refCountMeta = (RefCountResourceMeta*)meta;
                int LatestLoc = refCountMeta->CurLocation - 1;
                if (LatestLoc < 0)
                    LatestLoc = MAX_ALLOC_LOCATIONS - 1;

                loc = &refCountMeta->Locations[LatestLoc];
            }

            // Set only if not previously set.
            if (loc->filename[0] == '\0')
            {
                loc->filename = GetFilenameStringCopy(filename);
                NVOS_ASSERT(loc->filename);
                loc->line = line;
            }
        }
    }
    NvOsMutexUnlockInternal(s_tracker.mutex);
#endif
}

void NvOsDumpToDebugPrintf(void* context, const char* line)
{
    NvOsDebugPrintf("%s\n", line);
}
