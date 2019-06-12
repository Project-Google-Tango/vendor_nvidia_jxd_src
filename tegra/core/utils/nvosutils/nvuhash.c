/*
 * Copyright (c) 2005-2014 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */
#include "nvutil.h"
#include <string.h>

/*
 * This is not correct for all systems, but it's historically been set
 * unconditionally to 32B, and if someone really needs every ounce of
 * performance out of a hash table, they probably shouldn't be using
 * this one anyway.
 */
#define NVU_CACHE_LINE_SIZE     32

/* Usage values are between 0 (0%) and 256 (100%). */
#define NVU_MAX_HASH_USAGE      192     /* Maximum non-empty entries. */
#define NVU_MIN_HASH_USAGE      48      /* Minimum used entries, max / 4. */
#define NVU_THR_HASH_USAGE      144     /* Expansion threshold, max * 3 / 4. */

#define NVU_MIN_HASH_CAPACITY   8
#define NVU_HASH_SLOT_ENTRIES   (NVU_CACHE_LINE_SIZE / (int)sizeof(NvU32))

#define IS_POWER_OF_TWO(x)      (!((x) & ((x)-1)))

/*
 *
 */

void NvUHashInit (NvUHash* hash,
                  NvUHashHashFunc hashFunc,
                  NvUHashCompareFunc cmpFunc)
{
    NV_ASSERT(hash && hashFunc && cmpFunc);

    hash->hashFunc      = hashFunc;
    hash->cmpFunc       = cmpFunc;
    hash->capacity      = 0;
    hash->numUsed       = 0;
    hash->numNonEmpty   = 0;
    hash->dataPtr       = NULL;
    hash->iterPtr       = NULL;
    hash->keys          = NULL;
    hash->values        = NULL;
    hash->hashes        = NULL;
}

/*
 *
 */

void NvUHashDeinit (NvUHash* hash)
{
    NV_ASSERT(hash);
    NvUHashClear(hash);
}

/*
 * Searches entry from hash table. Returns index to entry, -1 if not found.
 */

static int NvUHashFindSlot (const NvUHash* hash,
                            const void* key,
                            NvU32 hashValue,
                            NvBool needEmpty)
{
    int slotMask;
    int firstEntry;
    int firstSlot;
    int step;
    int slot;

    NV_ASSERT(hash);
    NV_ASSERT(hashValue & 1);

    /* Check if the hash is empty. */

    if (!hash->capacity)
        return -1;

    /* Calculate search order parameters. */

    slotMask    = (hash->capacity - 1) & -NVU_HASH_SLOT_ENTRIES;
    firstEntry  = hashValue >> 1;
    firstSlot   = firstEntry & slotMask;
    step        = ((hashValue >> 18) & (NvU32)(-(int)((NvU32)NVU_HASH_SLOT_ENTRIES << 2))) | (NVU_HASH_SLOT_ENTRIES * 3);
    /* Search entry. */

    slot = firstSlot;
    do
    {
        int i;
        if (needEmpty)
        {
            if (hash->hashes)
            {
                for (i = 0; i < NVU_HASH_SLOT_ENTRIES; i++)
                {
                    int idx = slot | ((firstEntry + i) & (NVU_HASH_SLOT_ENTRIES - 1));
                    NvU32 entryHash = hash->hashes[idx];
                    NV_ASSERT(entryHash != hashValue || !hash->cmpFunc(hash->keys[idx], key));
                    if ((entryHash & 1) == 0)
                        return idx;
                }
            } else
            {
                for (i = 0; i < NVU_HASH_SLOT_ENTRIES; i++)
                {
                    int idx = slot | ((firstEntry + i) & (NVU_HASH_SLOT_ENTRIES - 1));
                    if (hash->keys[idx] == (void*)-1 || hash->keys[idx] == (void*)-2)
                        return idx;
                }
            }
        } else
        {
            if (hash->hashes)
            {
                for (i = 0; i < NVU_HASH_SLOT_ENTRIES; i++)
                {
                    int idx = slot | ((firstEntry + i) & (NVU_HASH_SLOT_ENTRIES - 1));
                    NvU32 entryHash = hash->hashes[idx];
                    if (entryHash == hashValue && hash->cmpFunc(hash->keys[idx], key))
                        return idx;
                    if (entryHash == 0)
                        return -1;
                }
            } else
            {
                for (i = 0; i < NVU_HASH_SLOT_ENTRIES; i++)
                {
                    int idx = slot | ((firstEntry + i) & (NVU_HASH_SLOT_ENTRIES - 1));
                    if (hash->keys[idx] == (void*)-1)
                        return -1;
                    if (hash->keys[idx] != (void*)-2 && hash->cmpFunc(hash->keys[idx], key))
                        return idx;
                }
            }
        }
        slot = (slot + step) & slotMask;
        step += NVU_HASH_SLOT_ENTRIES << 2;
    } while (slot != firstSlot);
    return -1;
}

/*
 * Rehash hash table to capacity.
 */

static NvBool NvUHashRehash (NvUHash* hash,
                             int capacity,
                             NvBool storeValues,
                             NvBool storeHashes)
{
    size_t  bytesPerEntry;
    void*   newDataPtr;
    int     oldCapacity;
    void*   oldDataPtr;
    void**  oldKeys;
    void**  oldValues;
    NvU32*  oldHashes;

    NV_ASSERT(hash);
    NV_ASSERT(hash->numUsed <= capacity);
    NV_ASSERT(IS_POWER_OF_TWO((NvU32)capacity));
    NV_ASSERT((capacity % NVU_HASH_SLOT_ENTRIES) == 0);
    NV_ASSERT(storeValues || !hash->values);

    /* Determine the number of bytes per entry. */

    bytesPerEntry = sizeof(void*);
    if (storeValues)
        bytesPerEntry += sizeof(void*);
    if (storeHashes)
        bytesPerEntry += sizeof(NvU32);

    /* Allocate new data. */

    newDataPtr = NvOsAlloc(capacity * bytesPerEntry + NVU_CACHE_LINE_SIZE - 1);
    if (!newDataPtr)
        return NV_FALSE;

    /* Backup old data. */

    oldCapacity         = hash->capacity;
    oldDataPtr          = hash->dataPtr;
    oldKeys             = hash->keys;
    oldValues           = hash->values;
    oldHashes           = hash->hashes;

    /* Initialize hash. */

    hash->capacity      = capacity;
    hash->numNonEmpty   = hash->numUsed;
    hash->dataPtr       = newDataPtr;

    newDataPtr          = (void*)(((NvUPtr)newDataPtr +
                                   (NvUPtr)(NVU_CACHE_LINE_SIZE - 1)) &
                                  ~(NvUPtr)(NVU_CACHE_LINE_SIZE - 1));
    hash->keys          = (void**)newDataPtr;
    newDataPtr          = (char*)newDataPtr + (capacity * sizeof(void*));

    if (storeHashes)
    {
        hash->hashes    = (NvU32*)newDataPtr;
        newDataPtr      = (char*)newDataPtr + (capacity * sizeof(NvU32));
        memset(hash->hashes, 0, (NvU32)capacity * sizeof(NvU32));
    } else
    {
        hash->hashes    = NULL;
        memset(hash->keys, -1, (NvU32)capacity * sizeof(void*));
    }

    if (storeValues)
    {
        hash->values    = (void**)newDataPtr;
        newDataPtr      = (char*)newDataPtr + (capacity * sizeof(void*));
    } else
    {
        hash->values    = NULL;
    }

    /* Re-insert old entries. */
    {
        int i;
        for (i = 0; i < oldCapacity; i++)
            if ((oldHashes && (oldHashes[i] & 1) != 0) ||
                (!oldHashes && oldKeys[i] != (void*)-1 && oldKeys[i] != (void*)-2))
            {
                NvU32 hashVal = (oldHashes) ? oldHashes[i] : hash->hashFunc(oldKeys[i]) | 1;
                int idx = NvUHashFindSlot(hash, oldKeys[i], hashVal, NV_TRUE);
                NV_ASSERT(idx != -1);

                hash->keys[idx] = oldKeys[i];
                if (hash->values)
                    hash->values[idx] = (oldValues) ? oldValues[i] : oldKeys[i];
                if (hash->hashes)
                    hash->hashes[idx] = hashVal;
            }
    }

    /* Free old data. */

    NvOsFree(oldDataPtr);

    return NV_TRUE;
}

/*
 * Returns true if hash table contains key.
 */

NvBool NvUHashContains (const NvUHash* hash, const void* key)
{
    NV_ASSERT(hash);
    return (NvUHashFindSlot(hash, key, hash->hashFunc(key) | 1, NV_FALSE) != -1);
}

/*
 * Returns the value for key (or the key itself for sets); otherwise NULL.
 */

void* NvUHashGet (const NvUHash* hash, const void* key)
{
    int idx;
    NV_ASSERT(hash);

    idx = NvUHashFindSlot(hash, key, hash->hashFunc(key) | 1, NV_FALSE);
    if (idx == -1)
        return NULL;
    return (hash->values) ? hash->values[idx] : hash->keys[idx];
}

/*
 * Look up a key and return true if found. If either storedKey and/or
 * storedValue arguments is not NULL, write the key and/or value into them.
 */

NvBool NvUHashSearch (const NvUHash* hash,
                      const void* key,
                      void** storedKey,
                      void** storedValue)
{
    int idx;
    NV_ASSERT(hash);

    idx = NvUHashFindSlot(hash, key, hash->hashFunc(key) | 1, NV_FALSE);
    if (idx == -1)
        return NV_FALSE;
    if (storedKey)
        *storedKey = hash->keys[idx];
    if (storedValue)
        *storedValue = (hash->values) ? hash->values[idx] : hash->keys[idx];
    return NV_TRUE;
}

/*
 * Empty the hash table.
 */

void NvUHashClear (NvUHash* hash)
{
    NV_ASSERT(hash);

    NvOsFree(hash->dataPtr);
    hash->capacity      = 0;
    hash->numUsed       = 0;
    hash->numNonEmpty   = 0;
    hash->dataPtr       = NULL;
    hash->keys          = NULL;
    hash->values        = NULL;
    hash->hashes        = NULL;
}

/*
 * Insert (key, value) into hash table.
 */

NvBool NvUHashInsert (NvUHash* hash,
                      void* key,
                      void* value,
                      NvBool storeHashes)
{
    NvU32   hashValue;
    int         idx;
    NV_ASSERT(hash);

    /* Rehash if maximum usage is being exceeded. */

    if ((int)((NvU32)hash->numNonEmpty << 8) >= hash->capacity * NVU_MAX_HASH_USAGE)
    {
        NvBool storeValues = (hash->values || key != value);
        NvBool rv;

        if ((int)((NvU32)hash->numUsed << 8) >= hash->capacity * NVU_THR_HASH_USAGE)
            rv = NvUHashRehash(hash,
                               NV_MAX((int)((NvU32)hash->capacity << 1),
                                      NVU_MIN_HASH_CAPACITY),
                               storeValues,
                               storeHashes);
        else
            rv = NvUHashRehash(hash,
                               hash->capacity,
                               storeValues,
                               storeHashes);

        if (!rv)
            return NV_FALSE;
    }

    /* Disable storeValues optimization if key isn't equal to the value */

    if (!hash->values && key != value) {
        if (!NvUHashRehash(hash, hash->capacity, NV_TRUE, storeHashes))
            return NV_FALSE;
    }

    /* Search for an unused entry. */

    hashValue = hash->hashFunc(key) | 1;
    idx = NvUHashFindSlot(hash, key, hashValue, NV_TRUE);
    NV_ASSERT(idx != -1);

    /* Set entry. */

    NV_ASSERT(hash->hashes || (key != (void*)-1 && key != (void*)-2));

    hash->numUsed++;
    if ((hash->hashes && hash->hashes[idx] == 0) || (!hash->hashes && hash->keys[idx] == (void*)-1))
        hash->numNonEmpty++;

    hash->keys[idx] = key;

    if (hash->values)
        hash->values[idx] = value;

    if (hash->hashes)
        hash->hashes[idx] = hashValue;

    return NV_TRUE;
}

/*
 * Remove item and if there is too much empty space, try to rehash to
 * smaller size. If rehash fails, item is removed without reducing the size.
 * Will return false if key was not found; otherwise this function will
 * always succeed.
 */

NvBool NvUHashRemove (NvUHash* hash,
                      const void* key,
                      void** storedKey,
                      void** storedValue)
{
    NvU32 newCap;
    NV_ASSERT(hash);

    /* Rehash if the minimum usage is being exceeded. */

    newCap = (NvU32)hash->capacity;
    while (((NvU32)hash->numUsed << 8) <= (newCap * NVU_MIN_HASH_USAGE) &&
           newCap > NVU_MIN_HASH_CAPACITY)
        newCap >>= 1;

    if ((NvS32)newCap != hash->capacity)
    {
        NV_ASSERT(hash->capacity != 0);
        NvUHashRehash(hash,
                      (NvS32)newCap,
                      (hash->values != NULL),
                      hash->hashes != NULL);
    }

    /* Remove the entry. */

    return NvUHashRemoveNoRehash(hash, key, storedKey, storedValue);
}

/*
 * Remove without rehashing. See NvUHashRemove(). Will return false if key
 * was not found; otherwise this function will always succeed.
 */

NvBool NvUHashRemoveNoRehash (NvUHash* hash,
                              const void* key,
                              void** storedKey,
                              void** storedValue)
{
    int idx;
    NV_ASSERT(hash);

    /* Search for the entry. */

    idx = NvUHashFindSlot(hash, key, hash->hashFunc(key) | 1, NV_FALSE);
    if (idx == -1)
        return NV_FALSE;
    if (storedKey)
        *storedKey = hash->keys[idx];
    if (storedValue)
        *storedValue = (hash->values) ? hash->values[idx] : hash->keys[idx];

    /* Mark entry as removed. */

    hash->numUsed--;
    if (hash->hashes)
        hash->hashes[idx] = 2;
    else
        hash->keys[idx] = (void*)-2;


    return NV_TRUE;
}

/*
 * Replace value for key. Will return false if key was not found; otherwise
 * this function will always succeed.
 */

NvBool NvUHashReplace (NvUHash* hash,
                       void* key,
                       void* value,
                       void** storedKey,
                       void** storedValue)
{
    int idx;
    NV_ASSERT(hash);

    /* Empty hash -> ignore. */

    if (!hash->capacity)
        return NV_FALSE;

    /* Disable storeValues optimization if key isn't equal to the value */

    if (!hash->values && key != value) {
        if (!NvUHashRehash(hash,
                           hash->capacity,
                           NV_TRUE,
                           (hash->hashes != NULL)))
            return NV_TRUE;
    }

    /* Search entry */

    idx = NvUHashFindSlot(hash, key, hash->hashFunc(key) | 1, NV_FALSE);
    if (idx == -1)
        return NV_FALSE;

    /* Replace */

    if (storedKey)
        *storedKey = hash->keys[idx];
    if (storedValue)
        *storedValue = (hash->values) ? hash->values[idx]: hash->keys[idx];

    hash->keys[idx] = key;
    if (hash->values)
        hash->values[idx] = value;
    return NV_TRUE;
}

/*
 * Returns -1 if there are no more entries.
 */

int NvUHashNextEntry (const NvUHash* hash, int idx)
{
    NV_ASSERT(hash->iterPtr == hash->dataPtr);

    if (hash->hashes)
    {
        for (idx++; idx < hash->capacity; idx++)
            if ((hash->hashes[idx] & 1) != 0)
                return idx;
    }
    else
    {
        for (idx++; idx < hash->capacity; idx++)
            if (hash->keys[idx] != (void*)-1 && hash->keys[idx] != (void*)-2)
                return idx;
    }

    return -1;
}

/*
 * Hash function helper for a pointer value.
 */

NvU32 NvUPtrHash (const void *ptr)
{
#if NVCPU_IS_64_BITS
    return (NvU32)(NvUScalarHash64((NvUPtr)ptr) & 0x00000000FFFFFFFFULL);
#else
    return NvUScalarHash32((NvUPtr)ptr);
#endif
}

/*
 * Generic hashing function, somewhat similar to Jenkins' hash function.
 * Final hash value depends on machine's byte order.
 */

NvU32 NvUStrHash (const char* s)
{
    int i;

    union
    {
        NvU8    bytes[12];
        NvU32   dwords[3];
    } buf;

    buf.dwords[0] = NVU_HASH_MAGIC;
    buf.dwords[1] = NVU_HASH_MAGIC;
    buf.dwords[2] = NVU_HASH_MAGIC;

    i = 0;
    while (*s)
    {
        buf.bytes[i] = (NvU8)(buf.bytes[i] + *s++);
        if (++i == 12)
        {
            NVU_JENKINS_MIX(buf.dwords[0], buf.dwords[1], buf.dwords[2]);
            i = 0;
        }
    }

    buf.dwords[2] += (NvU32)i;
    NVU_JENKINS_MIX(buf.dwords[0], buf.dwords[1], buf.dwords[2]);
    return buf.dwords[2];
}

/*
 * Wrap standard strcmp() into a bool.
 */

NvBool NvUStrCompare (const char* a, const char* b)
{
    return NvOsStrcmp(a, b) == 0;
}

/*
 * Deprecated.  Use NvUScalarHash[32/64] or NvUPtrHash instead.
 */

NvU32 NvUScalarHash (const void *scalar)
{
    /*
     * The prototype of this function makes it easy to mis-use
     * on 64-bit platforms where 32 bits of the input will be
     * ignored.  This function is deprecated for that reason.
     * For now, attempt to catch existing misuses of this
     * function using a runtime assert.
     */
    NV_ASSERT(!((NvU64)(NvUPtr)scalar & 0xFFFFFFFF00000000ULL));

    return NvUScalarHash32((NvU32)(NvUPtr)scalar);
}

/*
 * Hash function for a 32-bit scalar value such as a 32-bit pointer.
 */

NvU32 NvUScalarHash32 (const NvU32 scalar)
{
    NvU32 a = NVU_HASH_MAGIC;
    NvU32 b = NVU_HASH_MAGIC;
    NvU32 c = scalar;

    NVU_JENKINS_MIX(a, b, c);
    return c;
}

/*
 * Hash function for a 64-bit scalar value
 */

NvU64 NvUScalarHash64 (const NvU64 scalar)
{
    /*
     * Manually run through the last iteration of Bob Jenkins' hash
     * function.  This is the same function used by NvUScalarHash32,
     * but expanded to use more of the input and return more of the
     * output.
     */
    NvU32 a = NVU_HASH_MAGIC;
    NvU32 b = NVU_HASH_MAGIC + (NvU32)(scalar >> 32ULL);
    NvU32 c = (NvU32)(scalar & 0x00000000FFFFFFFFULL);

    NVU_JENKINS_MIX(a, b, c);

    return ((NvU64)b << 32) | (NvU64)c;
}

/*
 *
 */

NvBool NvUScalarCompare (const void* a, const void* b)
{
    return (a == b);
}
