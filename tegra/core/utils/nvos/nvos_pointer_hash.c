/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvos_pointer_hash.h"
#include "nvos.h"
#include "nvos_debug.h"
#include "nvassert.h"

// Not needed if resource tracker is not enabled.
#if NVOS_RESTRACKER_COMPILED

/*
 * Not thread safe.  
 *
 * Open hash with linear probing. Open hash = less memory overhead and
 * simple design, but removal requires trickery. Linear probing = good
 * cache coherency
 *
 * Sizes as powers of two. Needs to be re-considered? Less performant,
 * if allow other sizes. May require tweaking hash functions.
 *
 * rv means return value 
 *
 * No support for shrinking.
 */

// Disable stats of the hash by default for perf reasons.
#define NVOS_PHASH_STATS 0

#define NV_POINTER_HASH_MAX_FILL_FACTOR  85
#define NV_POINTER_HASH_MAX_DELETED_RATE 33
#define NV_POINTER_HASH_DELETED          ((void*)((NvUPtr)0 - (NvUPtr)1))

#if NVOS_PHASH_STATS
static NvU32 s_numRehash       = 0;
static NvU32 s_numFind         = 0;
static NvU32 s_numFindProbes   = 0;
static NvU32 s_numInsert       = 0;
static NvU32 s_numInsertProbes = 0;
#endif


typedef struct PointerHashEntryRec PointerHashEntry;

// One entry in the hash
struct PointerHashEntryRec
{
    void*  Key;
    void*  Value;
};

// Hash object
struct PointerHashRec 
{
    NvS32              Size;
    NvS32              EntryCount;  /* Current number of entries */
    NvS32              DeleteCount; /* Deletes made since latest rehash */
    NvS32              IterIndex;   /* Current position of iteration */
    PointerHashEntry*  EntryList;   /* Hash array */
};

#if NVOS_DEBUG
// Power of two test for assertions.
static NV_INLINE NvBool       IsPowerOfTwo      (int x);
#endif

// Actual hash value
static NV_INLINE NvUPtr       PointerHashHash   (void* Key);
// Resize the hash and/or clean up deleted slots.
static NvBool                 PointerHashRehash (PointerHash *Hash, int NewSize);


#if NVOS_DEBUG
NvBool IsPowerOfTwo(int x)
{
    return ((x ^ (x - 1)) & x) == x;
}
#endif

NvUPtr PointerHashHash(void* Key)
{
    return ((NvUPtr)Key >> 2);
}

PointerHash* PointerHashCreate(int InitialSize)
{
    PointerHash *Hash = NULL;
    
    NVOS_ASSERT(InitialSize > 0 && IsPowerOfTwo(InitialSize));
    NVOS_ASSERT(NV_POINTER_HASH_DELETED); // Check that DELETED does not evaluate to zero.

    Hash = (PointerHash*)NvOsAllocInternal(sizeof(PointerHash));

    if (Hash)
    {
        Hash->Size        = InitialSize;
        Hash->EntryCount  = 0;
        Hash->DeleteCount = 0;
        Hash->IterIndex   = -1;
        Hash->EntryList   = (PointerHashEntry*)NvOsAllocInternal(sizeof(PointerHashEntry) * InitialSize);

        if (!Hash->EntryList)
        {
            PointerHashDestroy(Hash);
            Hash = NULL;
        }
        
        NvOsMemset(Hash->EntryList, 0, Hash->Size * sizeof(PointerHashEntry));
    }
    NvOsIncResTrackerStat(NvOsResTrackerStat_TrackerBytes, 
                          sizeof(PointerHash), 
                          NvOsResTrackerStat_TrackerPeakBytes);
    return Hash;
}

void PointerHashDestroy(PointerHash *Hash)
{
    NVOS_ASSERT(Hash);        

    if (Hash)
        NvOsFreeInternal(Hash->EntryList);
    NvOsFreeInternal(Hash);
}

NvBool PointerHashRehash(PointerHash* Hash, int NewSize)
{
    PointerHashEntry* OldList;
    int OldSize;
    int i;

    NVOS_ASSERT(Hash);
    NVOS_ASSERT(Hash->EntryList);
    NVOS_ASSERT(NewSize > 0 && IsPowerOfTwo(NewSize));
    // Check for infinite loop possibility
    NVOS_ASSERT(Hash->EntryCount > 0 || Hash->DeleteCount > 0);
    NVOS_ASSERT(Hash->IterIndex == -1);    

#if NVOS_PHASH_STATS
    NvOsIncResTrackerStat(NvOsResTrackerStat_TrackerNumRehash, 
                          1,
                          NvOsResTrackerStat_MaxStat);
    NvOsIncResTrackerStat(NvOsResTrackerStat_TrackerNumInsert, 
                          g_numInsert, 
                          NvOsResTrackerStat_MaxStat);
    NvOsIncResTrackerStat(NvOsResTrackerStat_TrackerNumInsertProbes, 
                          g_numInsertProbes, 
                          NvOsResTrackerStat_MaxStat);
    NvOsIncResTrackerStat(NvOsResTrackerStat_TrackerNumFind, 
                          g_numFind, 
                          NvOsResTrackerStat_MaxStat);
    NvOsIncResTrackerStat(NvOsResTrackerStat_TrackerNumFindProbes, 
                          g_numFindProbes, 
                          NvOsResTrackerStat_MaxStat);

    s_numInsert = 0;
    s_numInsertProbes = 0;
    s_numFind = 0;
    s_numFindProbes = 0;
#endif
    
    OldList = Hash->EntryList;
    OldSize = Hash->Size;

    Hash->EntryList = (PointerHashEntry*)NvOsAllocInternal(NewSize*sizeof(PointerHashEntry));
    if (!Hash->EntryList)
    {
        Hash->EntryList = OldList;
        return NV_FALSE;
    }

    NvOsIncResTrackerStat(NvOsResTrackerStat_TrackerBytes, 
                          (NewSize - OldSize) * sizeof(PointerHashEntry), 
                          NvOsResTrackerStat_TrackerPeakBytes);

    NvOsMemset(Hash->EntryList, 0, NewSize * sizeof(PointerHashEntry));   
    Hash->Size = NewSize;
    Hash->EntryCount = 0;
    Hash->DeleteCount = 0;

    for (i = 0; i < OldSize; i++)
    {
        if (OldList[i].Key && OldList[i].Key != NV_POINTER_HASH_DELETED)
        {
            PointerHashInsert(Hash, OldList[i].Key, OldList[i].Value);
            NVOS_ASSERT(PointerHashFind(Hash, OldList[i].Key, NULL));
        }
    }

    NvOsFreeInternal(OldList);

    NVOS_ASSERT(Hash->EntryList);
    NVOS_ASSERT(Hash->Size == NewSize);

    return NV_TRUE;
}

// Double add not allowed.
// Remove-insert must not fail.
NvBool PointerHashInsert(PointerHash* Hash, void* Key, void* Value)
{
    unsigned int slot = (unsigned int) PointerHashHash(Key);
    unsigned int size;
    unsigned int SizeMask;

    NVOS_ASSERT(Hash);
    NVOS_ASSERT(Hash->EntryList);
    NVOS_ASSERT(IsPowerOfTwo(Hash->Size));
    NVOS_ASSERT(Key != NV_POINTER_HASH_DELETED);
    NVOS_ASSERT(Hash->IterIndex == -1);    

#if NVOS_PHASH_STATS
    s_numInsert++;
#endif
    size = Hash->Size;

    NVOS_ASSERT(size > 0);
    // TODO [kraita 27/Nov/08] How about comparing EntryCount + DeleteCount against fill factor?
    if (((100 * Hash->EntryCount) / size) > NV_POINTER_HASH_MAX_FILL_FACTOR)
    {
        // TODO [kraita 26/Nov/08] Consider capping the increment? Would violate requirement of pow 2 table size...
        if (!PointerHashRehash(Hash, size * 2))
            return NV_FALSE;
        size = Hash->Size; // refresh after resize.
    }
    // Clean deleted entries away. Ignore rehash failure to make sure that
    // remove-insert does not fail. We always will have room for entry
    // if one was removed.
    if (((100 * Hash->DeleteCount) / size) > NV_POINTER_HASH_MAX_DELETED_RATE) 
        (void)PointerHashRehash(Hash, size);

    NVOS_ASSERT(!PointerHashFind(Hash, Key, NULL));

    SizeMask = (size - 1);
    slot = slot & SizeMask;
        
    while (Hash->EntryList[slot].Key && Hash->EntryList[slot].Key != NV_POINTER_HASH_DELETED)
    {
        NVOS_ASSERT(((slot + 1) & SizeMask) != slot); // Make always progress.
        slot = (slot + 1) & SizeMask;
        NVOS_ASSERT(slot != (PointerHashHash(Key) & SizeMask)); // Avoid infinite loop.
#if NVOS_PHASH_STATS
        s_numInsertProbes++;
#endif
    }
    
    NVOS_ASSERT(!Hash->EntryList[slot].Key || Hash->EntryList[slot].Key == NV_POINTER_HASH_DELETED);

    Hash->EntryList[slot].Key   = Key;
    Hash->EntryList[slot].Value = Value;
    Hash->EntryCount++;

    // TODO [kraita 27/Nov/08] If we used deleted slot, should we dec DeleteCount?

    return NV_TRUE;
}

NvBool PointerHashFind(PointerHash* Hash, void* Key, void** Value)
{
    unsigned int slot = (unsigned int) PointerHashHash(Key);
    unsigned int size;
    unsigned int SizeMask;

    NVOS_ASSERT(Hash);
    NVOS_ASSERT(Hash->EntryList);
    NVOS_ASSERT(IsPowerOfTwo(Hash->Size));
    NVOS_ASSERT(Key != NV_POINTER_HASH_DELETED);

#if NVOS_PHASH_STATS
    s_numFind++;
#endif

    size = Hash->Size;
    SizeMask = (size - 1);

    slot = slot & SizeMask;

    while (Hash->EntryList[slot].Key)
    {
        NVOS_ASSERT(((slot + 1) & SizeMask) != slot); // Make always progress.

#if NVOS_PHASH_STATS
        g_numFindProbes++;
#endif
        if (Hash->EntryList[slot].Key == Key)
        {
            if (Value)
                *Value = Hash->EntryList[slot].Value;
            return NV_TRUE;
        }
        slot = (slot + 1) & SizeMask;

        NVOS_ASSERT(slot != (PointerHashHash(Key) & SizeMask)); // Avoid infinite loop.
    }    

    return NV_FALSE;
}

// Remove-insert must not fail.
NvBool PointerHashRemove(PointerHash* Hash, void* Key, void** Value)
{
    unsigned int slot = (unsigned int) PointerHashHash(Key);
    unsigned int size;
    unsigned int SizeMask;

    NVOS_ASSERT(Hash);
    NVOS_ASSERT(Hash->EntryList);
    NVOS_ASSERT(IsPowerOfTwo(Hash->Size));
    NVOS_ASSERT(Key != NV_POINTER_HASH_DELETED);
    NVOS_ASSERT(Hash->IterIndex == -1);    

    size = Hash->Size;
    SizeMask = (size - 1);

    slot = slot & SizeMask;

    while (Hash->EntryList[slot].Key != Key)
    {
        NVOS_ASSERT(((slot + 1) & SizeMask) != slot); // Make always progress.

        if (!Hash->EntryList[slot].Key)
            return NV_FALSE;
        slot = (slot + 1) & SizeMask;

        NVOS_ASSERT(slot != (PointerHashHash(Key) & SizeMask)); // Avoid infinite loop.
    }    
    if (Value)
        *Value = Hash->EntryList[slot].Value;

    Hash->EntryList[slot].Key = NV_POINTER_HASH_DELETED;

    Hash->EntryCount--;
    Hash->DeleteCount++;

    return NV_TRUE;
}

void PointerHashIterate (PointerHash* Hash)
{
    NVOS_ASSERT(Hash);
    NVOS_ASSERT(Hash->IterIndex == -1);    

    Hash->IterIndex = 0;
}

void PointerHashStopIter(PointerHash* Hash)
{
    NVOS_ASSERT(Hash);
    Hash->IterIndex = -1;
}

// Call first PointerHashIterate!
// Do not modify hash state while iterating!
// Returns NV_TRUE if this item is valid. NV_FALSE if there are no more items.
NvBool PointerHashNext(PointerHash* Hash, void** Key, void** Value)
{
    int i;
    
    NVOS_ASSERT(Hash);
    NVOS_ASSERT(Hash->IterIndex >= 0 && Hash->IterIndex <= Hash->Size);
    
    i = Hash->IterIndex;

    while (i < Hash->Size && 
           (!Hash->EntryList[i].Key || 
            Hash->EntryList[i].Key == NV_POINTER_HASH_DELETED))
    {
        i++;
    }

    if (i >= Hash->Size)
    {
        Hash->IterIndex = -1;
        return NV_FALSE;
    }
    
    if (Key)
        *Key = Hash->EntryList[i].Key;
    if (Value)
        *Value = Hash->EntryList[i].Value;

    Hash->IterIndex = i + 1;

    return NV_TRUE;
}

int PointerHashSize(const PointerHash* Hash)
{
    return Hash->EntryCount;
}

#endif // NVOS_RESTRACKER_COMPILED 
