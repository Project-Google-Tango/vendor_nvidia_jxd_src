/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
/**
 * @file
 * <b>NVIDIA Utilities</b>
 *
 * @b Description: Declares functions for a range of common tasks such as
 *                 hashing and parsing.
 */

/**
 * @defgroup nv_utility_group General Purpose Functions
 * Provides functions for a range of common tasks such as hashing and parsing.
 * @ingroup common_utils
 * @{
 */
#ifndef INCLUDED_NVUTIL_H
#define INCLUDED_NVUTIL_H

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "nvcommon.h"
#include "nvos.h"
#include "nvassert.h"

#if defined(__cplusplus)
extern "C"
{
#endif

//###########################################################################
//############################### DEFINITIONS ###############################
//###########################################################################

/**
 * Highly generic and easy-to-use typesafe hash table in C. Uses macros to
 * specialize static inline accessors for different datatypes. The
 * implementation uses open addressing and a hashing algorithm known as
 * Jenkins' mix.
 *
 * Here is an example that maps a pointer value to hypothetical NvBlob
 * records:
 *
 * typedef struct NvBlobRec NvBlob;
 * NVU_DEFINE_SPECIALIZED_HASH(PointerToBlobMap,
 *                             NvUPtr,
 *                             NvBlob*,
 *                             NvUPtrHash,
 *                             NvUScalarCompare);
 *
 * Then use NvPointerToBlobMapInit(), NvPointerToBlobMapInsert(),
 * NvPointerToBlobMapGet(), and NvPointerToBlobMapDeinit() to access this
 * specialized hash table in a typesafe manner. See the NvUHashInit(),
 * NvUHashInsert(), NvUHashGet(), NvUHashDeinit(), and other functions below
 * for complete documentation.
 */

typedef NvU32   (*NvUHashHashFunc)     (const void* key);
typedef NvBool  (*NvUHashCompareFunc)  (const void* a, const void* b);

typedef struct NvUHash_s
{
    NvUHashHashFunc     hashFunc;
    NvUHashCompareFunc  cmpFunc;

    NvS32               capacity;
    NvS32               numUsed;
    NvS32               numNonEmpty;

    void*               dataPtr;
    void*               iterPtr;
    void**              keys;       /*!< If hash values aren't stores, -1 =>
                                     * empty, -2 => removed */
    NvU32*              hashes;     /*!< First bit set => used, 0 => empty,
                                     * 2 => removed. */
    void**              values;
} NvUHash;

#define NVU_HASH_MAGIC  (0x9e3779b9u)

#define NVU_JENKINS_MIX(a, b, c) \
{ \
    a -= b; a -= c; a ^= (c>>13); \
    b -= c; b -= a; b ^= (a<<8); \
    c -= a; c -= b; c ^= (b>>13); \
    a -= b; a -= c; a ^= (c>>12); \
    b -= c; b -= a; b ^= (a<<16); \
    c -= a; c -= b; c ^= (b>>5); \
    a -= b; a -= c; a ^= (c>>3); \
    b -= c; b -= a; b ^= (a<<10); \
    c -= a; c -= b; c ^= (b>>15); \
}

//###########################################################################
//############################### PROTOTYPES ################################
//###########################################################################

/**
 * Parses a string into a 32-bit unsigned integer.
 *
 * @param[in] s A pointer to the string (null-terminated).
 * @param[out] endptr A pointer to the first character
 *             in the string that was not used in the conversion.
 *             When all characters are used, this argument is @c NULL.
 * @param[in] base The number base for the conversion, which must be 0, 10, or 16.
 *              - 10: the number is parsed as a base 10 number.
 *              - 16: the number is parsed as a base 16 number (0x ignored).
 *              - 0: base 10 unless there is a leading 0x.
 */
unsigned long int
NvUStrtoul(
        const char *s, 
        char **endptr, 
        int base);

/**
 * Parses a string into a 64-bit unsigned integer.
 * For more information, see the NvUStrtoul() function.
 */
unsigned long long int
NvUStrtoull(
        const char *s, 
        char **endptr, 
        int base);

/**
 * Parses a string into a signed integer.
 *
 * @param s - pointer to the string (null-terminated)
 * @param endptr - if not NULL this returns pointing to the first character
 *                 in the string that was not used in the conversion.
 * @param base - must be 0, 10, or 16.
 *                 10: the number is parsed as a base 10 number
 *                 16: the number is parsed as a base 16 number (0x ignored)
 *                 0: base 10 unless there is a leading 0x
 */
long int
NvUStrtol(
        const char *s, 
        char **endptr, 
        int base);

/**
 * Concatenates two strings.
 *
 * @note The @a dest parameter is always left null terminated even if the
 * string in the @a src parameter exceeds n.
 *
 * @param dest The string to concatenate to.
 * @param src  The string to add to the end of @a dest.
 * @param n    The maximum number of characters from src (plus a NUL) that
 *             are appended to @a dest.
 */
void
NvUStrncat(
        char *dest, 
        const char *src, 
        size_t n);

/**
 * Gets a pointer to the first occurence of @a str2 in @a str1.
 *
 * @param str1 The string to be scanned.
 * @param str2 The string containing the sequence of characters to match.
 * @returns A pointer to the first occurence of @a str2 in @a str1;
 *          NULL if no match is found; or @a str1 if the length of
 *          @a str2 is zero.
 */
char *
NvUStrstr(
        const char *str1, 
        const char *str2);

/**
 * Converts strings between code pages.
 *
 * If @a pDest is NULL, this function returns the number of bytes, including
 * NULL termination, of the destination buffer required to store the converted
 * string.
 *
 * If @a pDest is specified, up to @a DestSize bytes of the code-page converted
 * string is written to it. If the destination buffer is too small to
 * store the converted string, the string is truncated and a
 * NULL-terminator added.
 *
 * If either SrcCodePage or DestCodePage is NvOsCodePage_Unknown, the system's
 * default code page is used for the conversion.
 *
 * @param pDest The destination buffer
 * @param DestCodePage The target code page
 * @param DestSize The size of the destination buffer, in bytes
 * @param pSrc The source string
 * @param SrcSize The size of the source buffer, in bytes, or zero if the
*       string is NULL-terminated
 * @param SrcCodePage The source string's code page
 *
 * @returns The length of the destination string and NULL termination, in bytes.
 *

 */
size_t
NvUStrlConvertCodePage(void *pDest,
                       size_t DestSize,
                       NvOsCodePage DestCodePage,
                       const void *pSrc,
                       size_t SrcSize,
                       NvOsCodePage SrcCodePage);

/**
 * Dynamically allocates zeroed memory (uses the NvOsAlloc()function).
 * @note When you are done with the memory, free it with the NvOsFree() function
 *
 * @param size The number of bytes to allocate
 * @returns NULL on failure.
 * @returns A pointer to zeroed memory.
 */
static NV_INLINE void *
NvUAlloc0(size_t size)
{
    void *p = NvOsAlloc(size);
    if (p)
        NvOsMemset(p,0,size);
    return p;
}

/**
 * Finds the lowest set bit.
 *
 * @param bits The bits to look through
 * @param nBits The number of bits wide
 */
NvU32
NvULowestBitSet( NvU32 bits, NvU32 nBits );

/**
 * Initializes the specified hash table.
 *
 * @param hash A pointer to the uninitialized hash table struct.
 * @param hashFunc A hashing function for keys.
 * @param cmpFunc A comparison function for keys.
 */
void
NvUHashInit (NvUHash* hash,
             NvUHashHashFunc hashFunc,
             NvUHashCompareFunc cmpFunc);

/**
 * De-initializes the hash table, which frees any allocated key/value slots.
 *
 * @param hash A pointer to the hash table.
 */
void
NvUHashDeinit (NvUHash* hash);

/**
 * Gets the number of slots used in the specified the hash table.
 *
 * @param hash A pointer to the hash table.
 * @return Number of slots used.
 */
static NV_FORCE_INLINE int
NvUHashGetSize (const NvUHash* hash);


/**
 * Indicates whether a key is in the specified hash table.
 *
 * @param hash A pointer to the hash table.
 * @param key The key to find.
 * @return True if hash contains key; and false otherwise.
 */
NvBool
NvUHashContains (const NvUHash* hash,
                 const void* key);

/**
 * Gets the value for the key.
 *
 * @param hash A pointer to the hash table.
 * @param key Key
 * @return The value stored for key or NULL.
 */
void*
NvUHashGet (const NvUHash* hash,
            const void* key);

/**
 * Gets the key value from the specified hash table.
 *
 * @param[in] hash A pointer to the hash table.
 * @param[in] key The key to look for.
 * @param[out] storedKey If not null, write original key into this
 * @param[out] storedValue If not null, write stored value into this
 * @retval True if the key is found.
 */
NvBool
NvUHashSearch (const NvUHash* hash,
               const void* key,
               void** storedKey,
               void** storedValue);

/**
 * Clears the specified hash table.
 *
 * @param hash A pointer to the hash table.
 */
void
NvUHashClear (NvUHash* hash);

/**
 * Inserts the specified value for key in hash table.
 *
 * This function also allocates a larger table and rehash entries if
 * the table is getting full.
 *
 * @note When iterating with NvUHashFirstEntry() or NvUHashNextEntry(), you
 *  must not call functions that might cause a rehash, such as NvUHashInsert()
 * or NvUHashRemove() as rehashing will invalidate the iterator.
 *
 * @param hash A pointer to the hash table.
 * @param key Lookup key
 * @param value Value for key
 * @param storeHashes If true the hash itself is stored as well as the key.
 *                    This parameter is for internal, specialized hashes that
 *                    you don't need to care about.
 * @return True on success; false on reallocation failure
 */
NvBool
NvUHashInsert (NvUHash* hash,
               void* key,
               void* value,
               NvBool storeHashes);

/**
 * Removes the specified entry from hash table.
 *
 * This function also rehashes the table when load factor drops too low.
 *
 * @note When iterating with NvUHashFirstEntry()/NvUHashNextEntry() you must
 * not call functions that might cause a rehash, such as NvUHashInsert() or
 * NvUHashRemove() as rehashing will invalidate the iterator.
 *
 * For removing items while iterating, use NvUHashRemoveNoRehash() only.
 *
 * @param hash A pointer to the hash table.
 * @param key Key to remove
 * @param storedKey If not null, write stored key into this
 * @param storedValue If not null, write stored value into this
 * @return True if key was found
 */
NvBool
NvUHashRemove (NvUHash* hash,
               const void* key,
               void** storedKey,
               void** storedValue);

/**
 * Removes the specified entry from hash table but does not rehash.
 *
 * This functions is useful for a huge batch of successive
 * removals as slots are just marked
 * free instead of rehashing. See also NvUHashRemove().
 *
 * @param hash A pointer to the hash table.
 * @param key Key to remove
 * @param storedKey If not null, write stored key into this
 * @param storedValue If not null, write stored value into this
 * @return True if key was found
 */
NvBool
NvUHashRemoveNoRehash (NvUHash* hash,
                       const void* key,
                       void** storedKey,
                       void** storedValue);

/**
 * Replaces an entry in hash table.
 *
 * @param hash A pointer to the hash table.
 * @param key Key
 * @param value New value
 * @param storedKey If not null, write stored key into this
 * @param storedValue If not null, write old value into this
 * @return True if key was found
 */
NvBool
NvUHashReplace (NvUHash* hash,
                void* key,
                void* value,
                void** storedKey,
                void** storedValue);

/* NOTE: If this function is restored, then convert this comment block back into
 * a Doxygen comment block and remove this sentence.
 * Insert or replace an entry in hash table.
 *
 * @param hash A pointer to the hash table.
 * @param key Key
 * @param value New value
 * @return True on success; false on reallocation failure
 *
 * Note: implemented as macros for specialized hashes only.
 */
/* NvBool */
/* NvUHashSet (NvUHash* hash, */
/*             void* key, */
/*             void* value); */


/**
 * Gets the first entry in the specified hash table for use with iteration.
 *
 * @param hash A pointer to the hash table.
 * @return Index of first entry
 */
static NV_FORCE_INLINE int
NvUHashFirstEntry (NvUHash* hash);

/**
 * Continues hash table iteration.
 *
 * @note If the table has been rehashed since NvUHashFirstEntry(), iteration
 * will stop. An assert will fail, if enabled.
 *
 * @param hash A pointer to the hash table.
 * @param idx Index of the current entry
 * @return Index of the following entry, or -1 if none
 */
int
NvUHashNextEntry (const NvUHash* hash,
                  int idx);

/**
 * Gets the key in the specified entry in a hash table.
 *
 * @param hash A pointer to the hash table.
 * @param idx Index of the current entry
 * @return Key associated with current entry
 */
static NV_FORCE_INLINE void*
NvUHashGetEntryKey (const NvUHash* hash,
                    int idx);

/**
 * Gets the value in the specified entry in a hash table.
 *
 * @param hash A pointer to the hash table.
 * @param idx Index of the current entry
 * @return Value associated with current entry
 */
static NV_FORCE_INLINE void*
NvUHashGetEntryValue (const NvUHash* hash,
                      int idx);

/**
 * Writes the value via hash table iterator.
 *
 * @param hash A pointer to the hash table.
 * @param idx Index of the current entry
 * @param value The replacement value for current entry
 */
static NV_FORCE_INLINE void
NvUHashSetEntryValue (const NvUHash* hash,
                      int idx,
                      void* value);

/**
 * Helper NvUHashHashFunc function for hashing strings.
 *
 * @param str String to hash
 * @return Hash computed from str
 */
NvU32
NvUStrHash (const char* str);

/**
 * Helper NvUHashCompareFunc function for strings.
 *
 * @param a First string
 * @param b Second string
 * @return True if a and b have equal contents
 */
NvBool
NvUStrCompare (const char* a,
               const char* b);

/**
 * Deprecated NvUHashHashFunc function for hashing 32-bit scalars.
 *
 * Use NvUScalarHash32() instead when hashing actual 32-bit scalar
 * values, and NvUPtrHash() when hashing pointers.
 *
 * @param scalar A 32-bit pointer or NvU32 to hash
 * @return Hash computed from scalar
 */
NvU32
NvUScalarHash (const void *scalar);

/**
 * Helper NvUHashHashFunc function for hashing 32-bit scalars.
 *
 * When hashing pointers, use NvUPtrHash() instead, which is
 * implemented using this function on 32-bit platforms, but
 * handles 64-bit pointers on 64-bit platforms.
 *
 * @param scalar An NvU32 value to hash
 * @return Hash computed from scalar
 */
NvU32
NvUScalarHash32 (const NvU32 scalar);

/**
 * A 64-bit->64-bit scalar hash function.
 *
 * Useful for lightly obfuscating 64-bit values, such as pointers
 * on 64-bit platforms, and used to implement NvUPtrHash() on
 * 64-bit platforms.
 *
 * @param scalar An NvU64 value to hash
 * @return Hash computed from scalar
 */
NvU64
NvUScalarHash64 (const NvU64 scalar);

/**
 * Helper NvUHashHashFunc function for hashing pointer values.
 *
 * This is a 64-bit safe replacement for NvUScalarHash().
 *
 * @param ptr A pointer-sized value to hash
 * @return Hash computed from scalar
 */
NvU32
NvUPtrHash (const void* ptr);

/**
 * Helper NvUHashCompareFunc function for scalars.
 *
 * @param a First value
 * @param b Second value
 * @return True if a equals b
 */
NvBool
NvUScalarCompare (const void* a,
                  const void* b);

//###########################################################################
//############################### INLINE BODIES #############################
//###########################################################################

int NvUHashGetSize (const NvUHash* hash)
{
    NV_ASSERT(hash);
    return hash->numUsed;
}

int NvUHashFirstEntry (NvUHash* hash)
{
    hash->iterPtr = hash->dataPtr;  /* Save dataptr to detect a rehash. */
    return NvUHashNextEntry(hash, -1);
}

void* NvUHashGetEntryKey (const NvUHash* hash, int idx)
{
    NV_ASSERT(hash && 0 <= idx && idx <= hash->capacity - 1);
    NV_ASSERT(hash->iterPtr == hash->dataPtr);
    return hash->keys[idx];
}

void* NvUHashGetEntryValue (const NvUHash* hash, int idx)
{
    NV_ASSERT(hash && 0 <= idx && idx <= hash->capacity - 1);
    NV_ASSERT(hash->iterPtr == hash->dataPtr);
    return (hash->values) ? hash->values[idx] : hash->keys[idx];
}

void NvUHashSetEntryValue (const NvUHash* hash, int idx, void* value)
{
    NV_ASSERT(hash && 0 <= idx && idx <= hash->capacity - 1 && hash->values);
    NV_ASSERT(hash->iterPtr == hash->dataPtr);
    hash->values[idx] = value;
}

//###########################################################################
//############################### HASH SPECIALIZATION MACRO MAGIC ###########
//###########################################################################

#define NVU_DEFINE_SPECIALIZED_HASH_2(NAME, KEY, VALUE, HASHFUNC, COMPAREFUNC, STOREHASHES) \
    typedef struct NAME ## _s \
    { \
        NvUHash generic; \
    } NAME; \
    \
    static NV_FORCE_INLINE void      NAME ## Init           (NAME* hash)                                                       { NV_ASSERT(sizeof(KEY) <= sizeof(void*)); NV_ASSERT(sizeof(VALUE) <= sizeof(void*)); NvUHashInit(&hash->generic, (NvUHashHashFunc)HASHFUNC, (NvUHashCompareFunc)COMPAREFUNC); } \
    static NV_FORCE_INLINE void      NAME ## Deinit         (NAME* hash)                                                       { NvUHashDeinit(&hash->generic); } \
    \
    static NV_FORCE_INLINE int       NAME ## GetSize        (const NAME* hash)                                                 { return NvUHashGetSize(&hash->generic); } \
    static NV_FORCE_INLINE NvBool    NAME ## Contains       (const NAME* hash, const KEY key)                                  { return NvUHashContains(&hash->generic, (const void*)key); } \
    static NV_FORCE_INLINE VALUE     NAME ## Get            (const NAME* hash, const KEY key)                                  { return (VALUE)NvUHashGet(&hash->generic, (const void*)key); } \
    static NV_FORCE_INLINE NvBool    NAME ## Search         (const NAME* hash, const KEY key, KEY* storedKey, VALUE* storedValue)  { void* tmpKey; void* tmpValue; NvBool found = NvUHashSearch(&hash->generic, (const void*)key, &tmpKey, &tmpValue); if (storedKey) *storedKey = (KEY)tmpKey; if (storedValue) *storedValue = (VALUE)tmpValue; return found; } \
    \
    static NV_FORCE_INLINE void      NAME ## Clear          (NAME* hash)                                                       { NvUHashClear(&hash->generic); } \
    static NV_FORCE_INLINE NvBool    NAME ## Insert         (NAME* hash, KEY key, VALUE value)                                 { return NvUHashInsert(&hash->generic, (void*)key, (void*)value, STOREHASHES); } \
    static NV_FORCE_INLINE NvBool    NAME ## Remove         (NAME* hash, const KEY key, KEY* storedKey, VALUE* storedValue)    { void* tmpKey; void* tmpValue; NvBool removed = NvUHashRemove(&hash->generic, (const void*)key, &tmpKey, &tmpValue); if (storedKey) *storedKey = (KEY)tmpKey; if (storedValue) *storedValue = (VALUE)tmpValue; return removed; } \
    static NV_FORCE_INLINE NvBool    NAME ## RemoveNoRehash (NAME* hash, const KEY key, KEY* storedKey, VALUE* storedValue)    { void* tmpKey; void* tmpValue; NvBool removed = NvUHashRemoveNoRehash(&hash->generic, (const void*)key, &tmpKey, &tmpValue); if (storedKey) *storedKey = (KEY)tmpKey; if (storedValue) *storedValue = (VALUE)tmpValue; return removed; } \
    static NV_FORCE_INLINE NvBool    NAME ## Replace        (NAME* hash, KEY key, VALUE value, KEY* storedKey, VALUE* storedValue) { void* tmpKey; void* tmpValue; NvBool replaced = NvUHashReplace(&hash->generic, (void*)key, (void*)value, &tmpKey, &tmpValue); if (storedKey) *storedKey = (KEY)tmpKey; if (storedValue) *storedValue = (VALUE)tmpValue; return replaced; } \
    static NV_FORCE_INLINE NvBool    NAME ## Set            (NAME* hash, KEY key, VALUE value)                                 { if (!NvUHashContains(&hash->generic, (void*)key)) return NvUHashInsert(&hash->generic, (void*)key, (void*)value, STOREHASHES); NvUHashReplace(&hash->generic, (void*)key, (void*)value, NULL, NULL); return NV_TRUE; }                         \
    \
    static NV_FORCE_INLINE int       NAME ## NextEntry      (const NAME* hash, int idx)                                        { return NvUHashNextEntry(&hash->generic, idx); } \
    static NV_FORCE_INLINE int       NAME ## FirstEntry     (NAME* hash)                                                       { return NvUHashFirstEntry(&hash->generic); } \
    static NV_FORCE_INLINE KEY       NAME ## GetEntryKey    (const NAME* hash, int idx)                                        { return (KEY)NvUHashGetEntryKey(&hash->generic, idx); } \
    static NV_FORCE_INLINE VALUE     NAME ## GetEntryValue  (const NAME* hash, int idx)                                        { return (VALUE)NvUHashGetEntryValue(&hash->generic, idx); } \
    static NV_FORCE_INLINE void      NAME ## SetEntryValue  (const NAME* hash, int idx, VALUE value)                           { NvUHashSetEntryValue(&hash->generic, idx, (void*)value); }

#define NVU_DEFINE_SPECIALIZED_HASH(NAME, KEY, VALUE, HASHFUNC, COMPAREFUNC) \
    NVU_DEFINE_SPECIALIZED_HASH_2(NAME, KEY, VALUE, HASHFUNC, COMPAREFUNC, NV_TRUE)

/* It's illegal to store (KEY)-1 and (KEY)-2 to hash if hashvalue isn't stored */
#define NVU_DEFINE_SPECIALIZED_HASH_NO_HASHVALUE_STORAGE(NAME, KEY, VALUE, HASHFUNC, COMPAREFUNC) \
    NVU_DEFINE_SPECIALIZED_HASH_2(NAME, KEY, VALUE, HASHFUNC, COMPAREFUNC, NV_FALSE)

/** @} */
#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVUTIL_H
