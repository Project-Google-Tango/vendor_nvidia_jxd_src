/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */
 

/* This is an extremely simplistic memory allocator where the bookkeeping data is kept out of
 * band from the allocated memory.  It's intended to be used for framebuffer/carveout type
 * allocations.  
 *
 * Implementation is a simple first fit.  Book-keeping is kept as a singly linked list, which
 * means allocation time is O(n) w.r.t. the total number of allocations.
 *
 * This code was taken from mods, per Matt's suggestion.  depending on usage we'll probably need to
 * add a way to decrease allocation time from O(n) to something a tad faster.  ;)
 */


#include "nvrm_heap_simple.h"
#include "nvassert.h"
#include "nvos.h"

#define INITIAL_GROW_SIZE 8
#define MAX_GROW_SIZE     512
#define INVALID_INDEX     (NvU32)-1

#define DEBUG_HEAP 0

#if DEBUG_HEAP

static void
SanityCheckHeap(NvRmHeapSimple *pHeap)
{
    NvU32 index;

    // first clear all the touched bits
    for (index = 0; index < pHeap->ArraySize; ++index)
    {
        pHeap->RawBlockArray[index].touched = 0;
    }


    // Walk the BlockArray
    index = pHeap->BlockIndex;
    for (;;)
    {
        if (index == INVALID_INDEX)
            break;

        // make sure we're not off in the weeds.
        NV_ASSERT( index < pHeap->ArraySize );
        NV_ASSERT( pHeap->RawBlockArray[index].touched == 0);

        pHeap->RawBlockArray[index].touched = 1;
        index = pHeap->RawBlockArray[index].NextIndex;
    }


    // Walk the SpareArray
    index = pHeap->SpareBlockIndex;
    for (;;)
    {
        if (index == INVALID_INDEX)
            break;

        // make sure we're not off in the weeds.
        NV_ASSERT( index < pHeap->ArraySize );
        NV_ASSERT( pHeap->RawBlockArray[index].touched == 0);

        pHeap->RawBlockArray[index].touched = 2;
        index = pHeap->RawBlockArray[index].NextIndex;
    }

       
    // check that all blocks get touched.
    for (index = 0; index < pHeap->ArraySize; ++index)
    {
        NV_ASSERT(pHeap->RawBlockArray[index].touched != 0); 
    }
}

#else
# define SanityCheckHeap(a)
#endif



NvError NvRmPrivHeapSimple_HeapAlloc(NvRmPhysAddr base, NvU32 size,  NvRmHeapSimple *pNewHeap, NvBool IsTopDown)
{
    int     i;
    NvError err      = NvError_InsufficientMemory;

    NV_ASSERT(pNewHeap != NULL);
    NV_ASSERT(size > 0);

    NvOsMemset(pNewHeap, 0, sizeof(*pNewHeap));

    pNewHeap->base      = base;
    pNewHeap->size      = size;
    pNewHeap->ArraySize = INITIAL_GROW_SIZE;

    pNewHeap->RawBlockArray = NvOsAlloc(sizeof(NvRmHeapSimpleBlock) * INITIAL_GROW_SIZE);
    if (!pNewHeap->RawBlockArray)
    {
        err = NvError_InsufficientMemory;
        goto fail;
    }
    NvOsMemset(pNewHeap->RawBlockArray, 0, sizeof(NvRmHeapSimpleBlock) * INITIAL_GROW_SIZE);


    // setup all of the pointers (indices, whatever)
    for (i = 0; i < INITIAL_GROW_SIZE; ++i)
    {
        pNewHeap->RawBlockArray[i].NextIndex = i + 1;
    }
    pNewHeap->RawBlockArray[i-1].NextIndex = INVALID_INDEX;

    pNewHeap->BlockIndex      = 0;
    pNewHeap->SpareBlockIndex = 1;
    pNewHeap->IsTopDown       = IsTopDown;

    pNewHeap->RawBlockArray[pNewHeap->BlockIndex].IsFree    = NV_TRUE;
    pNewHeap->RawBlockArray[pNewHeap->BlockIndex].PhysAddr  = IsTopDown? base + size : base;
    pNewHeap->RawBlockArray[pNewHeap->BlockIndex].size      = size;
    pNewHeap->RawBlockArray[pNewHeap->BlockIndex].NextIndex = INVALID_INDEX;

    err = NvOsMutexCreate(&pNewHeap->mutex);
    if (err)
        goto fail;

    SanityCheckHeap(pNewHeap);
    return NvSuccess;

fail:
    NvOsFree(pNewHeap->RawBlockArray);
    return err;
}


/**
 * Frees up a heap structure, and all items that were associated with this heap
 *
 * @param pHeap Pointer to the heap structure returned from NvRmPrivHeapSimpleHeapAlloc
 */

void NvRmPrivHeapSimple_HeapFree(NvRmHeapSimple *pHeap)
{
    if (pHeap)
    {
        SanityCheckHeap(pHeap);
        NvOsMutexDestroy(pHeap->mutex);
        NvOsFree(pHeap->RawBlockArray);
    }
}

static NvError NvRmPrivHeapSimpleGrowBlockArray(
    NvRmHeapSimple *pHeap)
{
    NvU32 SpareIndex = pHeap->SpareBlockIndex;
    NvU32 NumFree = 0;
    const NvU32 MinFree = 2;

    while (SpareIndex!=INVALID_INDEX && NumFree<MinFree)
    {
        NumFree++;
        SpareIndex = pHeap->RawBlockArray[SpareIndex].NextIndex;
    }

    if (NumFree < MinFree)
    {
        NvU32 i;
        NvU32 NewArraySize;
        NvU32 GrowSize;
        NvRmHeapSimpleBlock *NewBlockArray;

        GrowSize = pHeap->ArraySize + (pHeap->ArraySize >> 1);
        GrowSize = NV_MIN(GrowSize, MAX_GROW_SIZE);

        // Grow by 8 ensures that we have at least 2 blocks and also is a little
        // more efficient than growing by 1 each time.
        NewArraySize = pHeap->ArraySize + GrowSize;        
        NewBlockArray = NvOsAlloc( sizeof(NvRmHeapSimpleBlock)*NewArraySize);
        if (!NewBlockArray)
        {
            SanityCheckHeap(pHeap);
            return NvError_InsufficientMemory;
        }
        NvOsMemset(NewBlockArray, 0, sizeof(NvRmHeapSimpleBlock)*NewArraySize);

        NvOsMemcpy(NewBlockArray, pHeap->RawBlockArray, sizeof(NvRmHeapSimpleBlock)*pHeap->ArraySize);

        // setup the NextIndex in the new part of the array
        for (i = pHeap->ArraySize; i < NewArraySize; i++)
        {
            NewBlockArray[i].NextIndex = i + 1;
        }

        // Point the last element of the new array to the old SpareBlockList
        NewBlockArray[NewArraySize - 1].NextIndex = pHeap->SpareBlockIndex;
        NvOsFree(pHeap->RawBlockArray);

        // Update all our information
        pHeap->RawBlockArray   = NewBlockArray;
        pHeap->SpareBlockIndex = pHeap->ArraySize;
        pHeap->ArraySize       = NewArraySize;
    }
    
    return NvSuccess;
}

NvError NvRmPrivHeapSimplePreAlloc(
    NvRmHeapSimple *pHeap,
    NvRmPhysAddr    Address,
    NvU32           Length)
{
    NvRmHeapSimpleBlock *pBlock;
    NvU32                BlockIndex;

    NV_ASSERT(pHeap!=NULL);

    //  All preallocated blocks must start at a minimum of a 32B alignment
    if ((Address & 31) || (Length & 31))
        return NvError_NotSupported;

    NvOsMutexLock(pHeap->mutex);

    if (NvRmPrivHeapSimpleGrowBlockArray(pHeap)!=NvSuccess)
    {
        NvOsMutexUnlock(pHeap->mutex);
        return NvError_InsufficientMemory;
    }

    //  Iteratively search through all the blocks for the block whose
    //  physical address region contains the requested pre-allocated
    //  region, and which isn't already allocated.
    for (BlockIndex = pHeap->BlockIndex; BlockIndex!=INVALID_INDEX;
         BlockIndex = pHeap->RawBlockArray[BlockIndex].NextIndex)
    {
        pBlock = &pHeap->RawBlockArray[BlockIndex];

        if (pBlock->PhysAddr<=Address &&
            (pBlock->PhysAddr+pBlock->size) >= (Address+Length) &&
            pBlock->IsFree)
        {
            //  If the free region starts before the preallocated region,
            //  split the free region into two blocks.
            if (pBlock->PhysAddr < Address)
            {
                NvRmHeapSimpleBlock *NewBlock;
                NvU32                NewBlockIndex;

                // Grab a block off the spare list and link it into place
                NewBlockIndex          = pHeap->SpareBlockIndex;
                NewBlock               = &pHeap->RawBlockArray[NewBlockIndex];
                pHeap->SpareBlockIndex = NewBlock->NextIndex;

                NewBlock->NextIndex    = pBlock->NextIndex;
                pBlock->NextIndex      = NewBlockIndex;

                // Set up the new block
                NewBlock->IsFree   = NV_TRUE;
                NewBlock->PhysAddr = Address;
                NewBlock->size     = pBlock->size;

                // Shrink the current block to
                pBlock->size = (Address - pBlock->PhysAddr);
                NewBlock->size -= pBlock->size;

                // Advance to the block we are actually going to allocate out of
                pBlock = NewBlock;
            }

            if ((pBlock->PhysAddr + pBlock->size) > (Address + Length))
            {
                NvRmHeapSimpleBlock *NewBlock;
                NvU32                NewBlockIndex;

                NewBlockIndex          = pHeap->SpareBlockIndex;
                NewBlock               = &pHeap->RawBlockArray[NewBlockIndex];
                pHeap->SpareBlockIndex = NewBlock->NextIndex;

                NewBlock->NextIndex    = pBlock->NextIndex;
                pBlock->NextIndex      = NewBlockIndex;

                NewBlock->IsFree       = NV_TRUE;
                NewBlock->PhysAddr     = (pBlock->PhysAddr + Length);
                NewBlock->size         = (pBlock->size - Length);
                
                pBlock->size = Length;
            }

            NV_ASSERT(pBlock->PhysAddr == Address &&
                      pBlock->size == Length);

            pBlock->IsFree = NV_FALSE;
            SanityCheckHeap(pHeap);

            NvOsMutexUnlock(pHeap->mutex);
            return NvSuccess;
        }
    }

    SanityCheckHeap(pHeap);
    NvOsMutexUnlock(pHeap->mutex);
    return NvError_InsufficientMemory;
}


NvError NvRmPrivHeapSimpleAlloc(
    NvRmHeapSimple *pHeap,
    NvU32           size,
    NvU32           align,
    NvRmPhysAddr     *pPAddr)
{
    NvRmHeapSimpleBlock *pBlock;
    NvU32               BlockIndex;

    // Must align to a power of two
    // Alignment offset should be less than the total alignment
    NV_ASSERT(!(align & (align-1)));

    NV_ASSERT(pHeap != NULL);

    NvOsMutexLock(pHeap->mutex);

    if (NvRmPrivHeapSimpleGrowBlockArray(pHeap)!=NvSuccess)
    {
        NvOsMutexUnlock(pHeap->mutex);
        return NvError_InsufficientMemory;
    }

    // Scan through the list of blocks
    for (BlockIndex = pHeap->BlockIndex; BlockIndex != INVALID_INDEX; BlockIndex = pHeap->RawBlockArray[BlockIndex].NextIndex)
    {
        NvRmPhysAddr NewOffset;
        NvU32        ExtraAlignSpace;

        pBlock = &pHeap->RawBlockArray[BlockIndex];

        // Skip blocks that are not free or too small
        if ((!pBlock->IsFree) || (pHeap->IsTopDown && pBlock->size < size))
        {
            continue;
        }

        // Compute location where this allocation would start in this block, based
        // on the alignment and range requested
        NewOffset = pBlock->PhysAddr;

        if (pHeap->IsTopDown)
        {
            NewOffset = NewOffset & ~(align-1);
            NewOffset = ((NewOffset - size) & ~(align-1)) + size;
            NV_ASSERT(NewOffset <= pBlock->PhysAddr);
            ExtraAlignSpace = pBlock->PhysAddr - NewOffset;
        }
        else
        {
            NewOffset = (NewOffset + align-1) & ~(align-1);
            NV_ASSERT(NewOffset >= pBlock->PhysAddr);
            ExtraAlignSpace = NewOffset - pBlock->PhysAddr;
        }

        // Is the block too small to fit this allocation, including the extra space
        // required for alignment?
        if (pBlock->size < (size + ExtraAlignSpace) )
            continue;

        // Do we need to split this block in two to start the allocation at the proper
        // alignment?
        // Top-down allocations do not reclaim alignment pad space.
        if (!pHeap->IsTopDown && ExtraAlignSpace > 0)
        {
            NvRmHeapSimpleBlock *NewBlock;
            NvU32                NewBlockIndex;

            // Grab a block off the spare list and link it into place
            NewBlockIndex          = pHeap->SpareBlockIndex;
            NewBlock               = &pHeap->RawBlockArray[NewBlockIndex];
            pHeap->SpareBlockIndex = NewBlock->NextIndex;

            NewBlock->NextIndex    = pBlock->NextIndex;
            pBlock->NextIndex      = NewBlockIndex;

            // Set up the new block
            NewBlock->IsFree   = NV_TRUE;
            NewBlock->PhysAddr = pBlock->PhysAddr + ExtraAlignSpace;
            NewBlock->size     = pBlock->size - ExtraAlignSpace;

            // Shrink the current block to match this allocation
            pBlock->size = ExtraAlignSpace;

            // Advance to the block we are actually going to allocate out of
            pBlock = NewBlock;
        }

        // Do we need to split this block into two?
        if ( (!pHeap->IsTopDown && pBlock->size > size) ||
             (pHeap->IsTopDown && pBlock->size > size + ExtraAlignSpace) )
        {
            NvRmHeapSimpleBlock *NewBlock;
            NvU32                NewBlockIndex;

            // Grab a block off the spare list and link it into place
            NewBlockIndex          = pHeap->SpareBlockIndex;
            NewBlock               = &pHeap->RawBlockArray[NewBlockIndex];
            pHeap->SpareBlockIndex = NewBlock->NextIndex;
            NewBlock->NextIndex    = pBlock->NextIndex;
            pBlock->NextIndex      = NewBlockIndex;

            // Set up the new block
            NewBlock->IsFree = NV_TRUE;
            if (pHeap->IsTopDown)
            {
                NewBlock->PhysAddr = NewOffset - size - 1;
                NewBlock->size     = pBlock->size - size - ExtraAlignSpace - 1;
            }
            else
            {
                NewBlock->PhysAddr = pBlock->PhysAddr + size;
                NewBlock->size     = pBlock->size - size;
            }

            // Shrink the current block to match this allocation
            pBlock->size = pHeap->IsTopDown? size + ExtraAlignSpace : size;
        }

        pBlock->IsFree = NV_FALSE;
        if (pHeap->IsTopDown)
        {
            NV_ASSERT( ((NewOffset-size) % align) == 0 );
            *pPAddr = NewOffset - size;
        }
        else
        {
            NV_ASSERT(pBlock->size == size);
            *pPAddr = pBlock->PhysAddr;
        }
        SanityCheckHeap(pHeap);

        NvOsMutexUnlock(pHeap->mutex);
        return NvSuccess;
    }

    SanityCheckHeap(pHeap);
    NvOsMutexUnlock(pHeap->mutex);
    return NvError_InsufficientMemory;
}




void NvRmPrivHeapSimpleFree(NvRmHeapSimple *pHeap, NvRmPhysAddr PhysAddr)
{
    NvRmHeapSimpleBlock *pBlock = NULL;
    NvRmHeapSimpleBlock *pNext = NULL;
    NvRmHeapSimpleBlock *pPrev = NULL;
    
    NvU32 BlockIndex;
    NvU32 PrevIndex = INVALID_INDEX;
    NvU32 NextIndex = INVALID_INDEX;


    NV_ASSERT(pHeap != NULL);

    NvOsMutexLock(pHeap->mutex);

    // Find the block we're being asked to free
    BlockIndex = pHeap->BlockIndex;
    pBlock = &pHeap->RawBlockArray[BlockIndex];
    if (pHeap->IsTopDown)
    {
        while (BlockIndex != INVALID_INDEX && ((pBlock->PhysAddr - pBlock->size) != PhysAddr))
        {
            PrevIndex  = BlockIndex;
            BlockIndex = pBlock->NextIndex;
            pBlock     = &pHeap->RawBlockArray[BlockIndex];
        }
    }
    else
    {
        while (BlockIndex != INVALID_INDEX && (pBlock->PhysAddr != PhysAddr))
        {
            PrevIndex  = BlockIndex;
            BlockIndex = pBlock->NextIndex;
            pBlock     = &pHeap->RawBlockArray[BlockIndex];
        }
    }

    // The block we're being asked to free didn't exist or was already free
    if (BlockIndex == INVALID_INDEX || pBlock->IsFree)
    {
        SanityCheckHeap(pHeap);
        NvOsMutexUnlock(pHeap->mutex);
        return;
    }

    // This block is now a free block
    pBlock->IsFree = NV_TRUE;

    // If next block is free, merge the two into one block
    NextIndex = pBlock->NextIndex;
    pNext = &pHeap->RawBlockArray[NextIndex];
    if (NextIndex != INVALID_INDEX && pNext->IsFree)
    {
        pBlock->size += pHeap->IsTopDown? pNext->size + 1 : pNext->size;
        pBlock->NextIndex = pNext->NextIndex;

        pNext->NextIndex       = pHeap->SpareBlockIndex;
        pHeap->SpareBlockIndex = NextIndex;
    }

    // If previous block is free, merge the two into one block
    pPrev = &pHeap->RawBlockArray[PrevIndex];
    if (PrevIndex != INVALID_INDEX && pPrev->IsFree)
    {
        pPrev->size += pHeap->IsTopDown? pBlock->size + 1 : pBlock->size;
        pPrev->NextIndex = pBlock->NextIndex;

        pBlock->NextIndex = pHeap->SpareBlockIndex;
        pHeap->SpareBlockIndex = BlockIndex;
    }
    SanityCheckHeap(pHeap);
    NvOsMutexUnlock(pHeap->mutex);
}

NvS32 NvRmPrivHeapSimpleMemoryUsed(NvRmHeapSimple* pHeap)
{
    NvU32 Index;
    NvS32 MemUsed = 0;

    NV_ASSERT(pHeap != NULL);

    NvOsMutexLock(pHeap->mutex);

    for (Index = pHeap->BlockIndex; Index != INVALID_INDEX; )
    {
        NvRmHeapSimpleBlock* Block = &pHeap->RawBlockArray[Index];

        if (!Block->IsFree)
            MemUsed += Block->size;
        Index = Block->NextIndex;
    }

    NvOsMutexUnlock(pHeap->mutex);

    return MemUsed;
}

NvS32 NvRmPrivHeapSimpleLargestFreeBlock(NvRmHeapSimple* pHeap)
{
    NvU32 Index;
    NvS32 MaxFree = 0;

    NV_ASSERT(pHeap != NULL);

    NvOsMutexLock(pHeap->mutex);

    for (Index = pHeap->BlockIndex; Index != INVALID_INDEX; )
    {
        NvRmHeapSimpleBlock* Block = &pHeap->RawBlockArray[Index];
        int size = (int)Block->size;

        if (Block->IsFree && size > MaxFree)
            MaxFree = size;
        Index = Block->NextIndex;
    }

    NvOsMutexUnlock(pHeap->mutex);
    return MaxFree;
}
