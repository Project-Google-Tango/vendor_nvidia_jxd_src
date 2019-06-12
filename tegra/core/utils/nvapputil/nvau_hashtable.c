/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvos.h"
#include "nvapputil.h"

typedef struct HashtableNodeRec
{
    char *key;
    void *data;
    struct HashtableNodeRec *next;
} HashtableNode;

typedef struct HashtableBucketRec
{
    HashtableNode *node;
} HashtableBucket;

typedef struct NvAuHashtableRec
{
    NvU32 num_buckets;
    HashtableBucket *buckets;
    NvU32 IterBucketIndex;
    HashtableNode *IterNode;
} NvAuHashtable;

static NvU32
HashtableHash( NvAuHashtableHandle hashtable, const char *key )
{
    NvU32 hash = 0;
    char *k = (char *)key;

    while( *k )
    {
        hash = ( hash * 31 ) + (*k);
        k++;
    }

    return hash % hashtable->num_buckets;
}

NvError
NvAuHashtableCreate( NvU32 buckets, NvAuHashtableHandle *hashtable )
{
    NvAuHashtable *ht = 0;
    NvU32 i;

    ht = NvOsAlloc( sizeof(NvAuHashtable) );
    if( !ht )
    {
        goto fail;
    }

    ht->num_buckets = buckets;
    ht->buckets = NvOsAlloc( sizeof(HashtableBucket) * buckets );
    if( !ht->buckets )
    {
        goto fail;
    }

    for( i = 0; i < buckets; i++ )
    {
        ht->buckets[i].node = 0;
    }

    NvAuHashtableRewind(ht);
    *hashtable = ht;
    return NvSuccess;

fail:
    NvAuHashtableDestroy( ht );
    return NvError_InsufficientMemory;
}

NvError
NvAuHashtableInsert( NvAuHashtableHandle hashtable, const char *key, void *data )
{
    NvU32 index;
    HashtableNode *n;
    NvU32 keylen0 = NvOsStrlen( key ) + 1; // including terminator

    n = NvOsAlloc( sizeof(HashtableNode) + keylen0 );
    if( !n )
    {
        return NvError_InsufficientMemory;
    }

    index = HashtableHash( hashtable, key );
    n->key = (char *)(n + 1);
    NvOsStrncpy( n->key, key, keylen0 );
    n->data = data;

    n->next = hashtable->buckets[ index ].node;
    hashtable->buckets[ index ].node = n;

    return NvSuccess;
}

void
NvAuHashtableDelete( NvAuHashtableHandle hashtable, const char *key )
{
    NvU32 index;
    HashtableNode *n;
    HashtableNode *prev;

    index = HashtableHash( hashtable, key );

    n = hashtable->buckets[ index ].node;
    if( NvOsStrcmp( n->key, key ) == 0 )
    {
        hashtable->buckets[ index ].node = n->next;
    }
    else
    {
        prev = n;
        n = n->next;
        while( n )
        {
            if( NvOsStrcmp( n->key, key ) == 0 )
            {
                prev->next = n->next;
                break;
            }

            prev = n;
            n = n->next;
        }
    }

    NvOsFree( n );
}

void *
NvAuHashtableFind( NvAuHashtableHandle hashtable, const char *key )
{
    NvU32 index;
    HashtableNode *n;
    void *data = 0;

    if( !hashtable )
    {
        return 0;
    }

    index = HashtableHash( hashtable, key );

    n = hashtable->buckets[ index ].node;
    while( n )
    {
        if( NvOsStrcmp( n->key, key ) == 0 )
        {
            data = n->data;
            break;
        }

        n = n->next;
    }

    return data;
}

void
NvAuHashtableDestroy( NvAuHashtableHandle hashtable )
{
    NvU32 i;

    if( !hashtable )
    {
        return;
    }

    if( hashtable->buckets )
    {
        for( i = 0; i < hashtable->num_buckets; i++ )
        {
            HashtableNode *n = hashtable->buckets[i].node;
            while( n )
            {
                HashtableNode *tmp = n->next;
                NvOsFree( n );
                n = tmp;
            }
        }
    }

    NvOsFree( hashtable->buckets );
    NvOsFree( hashtable );
}

void
NvAuHashtableRewind( NvAuHashtableHandle hashtable )
{
    hashtable->IterBucketIndex = (NvU32)-1;
    hashtable->IterNode = NULL;
}

void
NvAuHashtableGetNext( NvAuHashtableHandle hashtable,
                      const char **key, void **data )
{
    while( !hashtable->IterNode )
    {
        hashtable->IterBucketIndex++;

        if( hashtable->IterBucketIndex >= hashtable->num_buckets )
        {
            *key = NULL;
            *data = NULL;
            return;
        }

        hashtable->IterNode =
            hashtable->buckets[hashtable->IterBucketIndex].node;
    }

    *key = hashtable->IterNode->key;
    *data = hashtable->IterNode->data;

    hashtable->IterNode = hashtable->IterNode->next;
}
