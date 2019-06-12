/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVOS_POINTER_HASH_H
#define INCLUDED_NVOS_POINTER_HASH_H

#include "nvcommon.h"

/** Opaque hash struct */
typedef struct PointerHashRec PointerHash;

/**
 * Create hash table with given initial size. Size must be a power of two
 */ 
PointerHash* PointerHashCreate  (int InitialSize);

/**
 * Destroy hash
 */
void         PointerHashDestroy (PointerHash* Hash);

/**
 * Insert Key / Value combination. Double insert is not allowed. Key cannot be NULL or -1.
 * Insert can lead to rehash.
 * Returns true if insert succeeded.
 */
NvBool       PointerHashInsert  (PointerHash* Hash, void* Key, void* Value);

/**
 * Find given entry. Value can be NULL to check for presence of an entry.
 * Return true if element was found.
 */
NvBool       PointerHashFind    (PointerHash* Hash, void* Key, void** Value);

/**
 * Remove entry. If Value is non-null, current value is returned there.
 * Returns true if the element was found.
 */
NvBool       PointerHashRemove  (PointerHash* Hash, void* Key, void** Value);

/**
 * Start iteration. No other operations except PointerHashFind are allowed while iterating.
 */
void         PointerHashIterate (PointerHash* Hash);

/**
 * Get next item in iteration. Key and Value can be Null. 
 * If returns false, the iteration has ended (Same as calling PointerHashStopIter).
 * Returns true if item was found, false if there are no more items.
 */
NvBool       PointerHashNext    (PointerHash* Hash, void** Key, void** Value);

/**
 * Stop iteration. Does not need to be called if PointerHashNext has returned NV_FALSE for
 * this iteration.
 */ 
void         PointerHashStopIter(PointerHash* Hash);

/**
 * Number of inserted items.
 */
int          PointerHashSize    (const PointerHash* Hash);
#endif // INCLUDED_NVOS_POINTER_HASH_H
