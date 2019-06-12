/*
 * Copyright (c) 2006 - 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVOS_DEBUG_H
#define INCLUDED_NVOS_DEBUG_H

/**
 * Functions for debugging resource management done via NvOS.
 *
 * Resource tracker is guarded by internal mutex. However, some care
 * must be taken to keep the resource tracking database in sync
 * with, e.g., OS memory allocation database. 
 */

#include "nvcommon.h"

#ifndef INCLUDED_NVOS_INTERNAL_H
#include "nvos_internal.h"
#endif

#if !defined(NVOS_ADVANCED_DEBUG)
#   define NVOS_ADVANCED_DEBUG 0
#endif

#if (NV_DEBUG && NVOS_ADVANCED_DEBUG)
#   define NVOS_RESTRACKER_COMPILED 1
#else
#   define NVOS_RESTRACKER_COMPILED 0
#endif

// Separate toggle for debugging NVOS debugging facilities
// for performance reasons.
#define NVOS_DEBUG 0

#if NVOS_DEBUG
#   define NVOS_ASSERT(X) NV_ASSERT(X)
#else
#   define NVOS_ASSERT(X)
#endif


typedef enum
{
    NvOsResTrackerStat_NumAllocs = 1,
    NvOsResTrackerStat_NumBytes,
    NvOsResTrackerStat_NumPeakAllocs,
    NvOsResTrackerStat_NumPeakBytes,
    NvOsResTrackerStat_TrackerBytes,            // Memory overhead caused by the tracker in bytes.
    NvOsResTrackerStat_TrackerPeakBytes,        // Peak memory overhead caused by the tracker in bytes.
    NvOsResTrackerStat_TrackerNumRehash,        // Resource tracker internal. Requires NVOS_PHASH_STATS on in nvos_pointer_hash.c
    NvOsResTrackerStat_TrackerNumInsert,        // Resource tracker internal. Requires NVOS_PHASH_STATS on in nvos_pointer_hash.c
    NvOsResTrackerStat_TrackerNumInsertProbes,  // Resource tracker internal. Requires NVOS_PHASH_STATS on in nvos_pointer_hash.c
    NvOsResTrackerStat_TrackerNumFind,          // Resource tracker internal. Requires NVOS_PHASH_STATS on in nvos_pointer_hash.c
    NvOsResTrackerStat_TrackerNumFindProbes,    // Resource tracker internal. Requires NVOS_PHASH_STATS on in nvos_pointer_hash.c
    NvOsResTrackerStat_MaxStat,
    NvOsResTrackerStat_Force32=0x7FFFFFFF
} NvOsResTrackerStat;

typedef enum
{
    NvOsAllocHookType_Alloc = 1,
    NvOsAllocHookType_Free,
    NvOsAllocHookType_Realloc
} NvOsAllocHookType;

#if NVOS_RESTRACKER_COMPILED

#define RESTRACKER_TAG "[NVOS] "

typedef enum
{
    NvOsResTrackerConfig_Enabled = 1,
    NvOsResTrackerConfig_BreakId,
    NvOsResTrackerConfig_BreakStackId,
    NvOsResTrackerConfig_CallstackType,
    NvOsResTrackerConfig_StackFilterPid,
    NvOsResTrackerConfig_DumpOnExit,
    
    NvOsResTrackerConfig_Last,
    NvOsResTrackerConfig_Force32=0x7FFFFFFF
} NvOsResTrackerConfig;


/**
 * NvOS function registers a resource allocation.
 */
void NvOsResourceAllocated(NvOsResourceType type, void* ptr);

/**
 * NvOS function deallocates resource.
 */
void NvOsResourceFreed(NvOsResourceType type, void* ptr);

/**
 * Initializes resource tracking. Leaks done before this call won't be
 * detected. Usually called in NvOS DllMain. All allocations done before
 * init should be released after deinit.
 */
void NvOsInitResourceTracker(void);

/**
 * Deinitialize resource tracking.
 */
void NvOsDeinitResourceTracker(void);

/**
 * Check whether resource tracking is currently enabled or not 
 */
NvBool NvOsResourceTrackingEnabled(void);

/**
 * Add resource userptr of given type to tracking. Returns false, if the registration fails due to lack of memory.
 */
NvBool NvOsTrackResource(NvOsResourceType type, void* userptr, const char* filename, int line);

/**
 * Release resource userptr from tracking. 
 */
void NvOsReleaseTrackedResource(void* userptr);

/**
 * Update a pointer in tracking. Object will retain its tracking meta data, but userptr is changed.
 */
void NvOsUpdateResourcePointer(void* OldPointer, void* NewPointer);

/**
 * Mark a resource corrupt. Used to flag resources when doing dumps.
 */
void NvOsResourceIsCorrupt(void* userptr);

/**
 * Return true if userptr is a registered resource.
 */
NvBool NvOsIsTrackedResource(void* userptr);

/**
 * Get exclusive access to resource tracker. Exclusive access is
 * needed to implement tracking of realloc, for example. Update 
 * of the OS memory database and our tracking must be atomic.
 */
void NvOsLockResourceTracker(void);

/**
 * Stop exclusive access to resource tracker.
 */
void NvOsUnlockResourceTracker(void);

/**
 * stat = stat + inc; peak = max(peak, stat). Set peak to MaxStat to skip peak update.
 */ 
void  NvOsIncResTrackerStat(NvOsResTrackerStat stat, NvS64 inc, NvOsResTrackerStat peak);

/* Returns True if the alloc-being-made is ok'd to succeed */
NvBool NvOsAllocSucceedsSim(NvOsAllocHookType AllocType);

/** 
 * stat = stat - dec. 
 */
void  NvOsDecResTrackerStat(NvOsResTrackerStat stat, NvS64 dec);

// Configuration interface
void NvOsResTrackerSetConfig(NvOsResTrackerConfig configItem, NvU32 value);

/**
 * The printf function for dump
 */
void NvOsResTrackerPrintf(const char *format, ...);

#else // NVOS_RESTRACKER_COMPILED

// Empty implementations for functions called internally by NvOS
#define NvOsResourceAllocated(type, ptr) do {} while (0)
#define NvOsResourceFreed(type, ptr) do {} while (0)
#define NvOsInitResourceTracker() do {} while (0)
#define NvOsDeinitResourceTracker() do {} while (0)

// Always "returns" true
#define NvOsAllocSucceedsSim(x) NV_TRUE

#endif // NVOS_RESTRACKER_COMPILED

/** NvOsCheckpoint - returns a checkpoint handle for current allocation
 *      state.
 *
 *  Note: Only available in debug builds!
 */

NvU32 NvOsCheckpoint(void);

/** NvOsDumpSnapshot - does a leak check against given snapshot.
 *
 *  @param start A snapshot handle from NvOsCheckpoint().
 *  @param end A snapshot handle from NvOsCheckpoint().
 *  @param out stream to write the dump to. If NULL, will use
 *             NvOsDebugPrintf().
 *
 *  Returns NV_TRUE if no leaks were found between start and end.
 *
 *  Note: Only available in debug builds!
 */
NvBool NvOsDumpSnapshot(NvU32 start, NvU32 end, NvOsFileHandle out);

/** NvOsVerifyHeap - Do an integrity check pass on memory heap.
 *
 *  Note: Very expensive call!
 *  Note: Only available in debug builds!
 *  Note: Will assert, not return, upon a broken alloc!
 */
void NvOsVerifyHeap(void);

/**
 * Get given resource tracker stat value.
 */
NvS64 NvOsGetResTrackerStat(NvOsResTrackerStat stat);

#if !(NVOS_TRACE || NV_DEBUG)
/* Only for binary compatibility. Implementation likely to be empty. */
void NvOsSetResourceAllocFileLine(void* userptr, const char* file, int line);
#endif

typedef NvBool (*NvOsAllocSimCB)(NvOsAllocHookType AllocType, void *UserContext);

/**
 * Install a hook into NvOsAlloc that gets called prior to doing the
 * actual allocation.
 *
 * This can be used to simulate failing mallocs.
 *
 * Pass in cb==NULL to unregister alloc hook.
 */
void NvOsSetAllocHook(NvOsAllocSimCB cb, void* UserContext);

#endif // #ifndef INCLUDED_NVOS_DEBUG_H 
