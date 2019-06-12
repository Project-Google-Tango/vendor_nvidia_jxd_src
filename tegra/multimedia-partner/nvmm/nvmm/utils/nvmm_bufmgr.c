/*
 * Copyright (c) 2007 NVIDIA Corporation.  All Rights Reserved.
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


#include "nvmm_bufmgr.h"

#define INITIAL_GROW_SIZE 8
#define MAX_GROW_SIZE     512
#define INVALID_INDEX     (NvU32)-1

#define DEBUG_HEAP 0

#define VIRTUAL 1

#if DEBUG_HEAP

static void SanityCheckHeap(NvMMBufMgrHandle hNvMMBufMgr)
{
    NvU32 index;
    NvMMBufMgrHeap *pBufMgrHeap = hNvMMBufMgr;

    // first clear all the touched bits
    for (index = 0; index < pBufMgrHeap->ArraySize; ++index)
    {
        pBufMgrHeap->RawBlockArray[index].touched = 0;
    }


    // Walk the BlockArray
    index = pBufMgrHeap->BlockIndex;
    for (;;)
    {
        if (index == INVALID_INDEX)
            break;

        // make sure we're not off in the weeds.
        NV_ASSERT( index < pBufMgrHeap->ArraySize );
        NV_ASSERT( pBufMgrHeap->RawBlockArray[index].touched == 0);

        pBufMgrHeap->RawBlockArray[index].touched = 1;
        index = pBufMgrHeap->RawBlockArray[index].NextIndex;
    }


    // Walk the SpareArray
    index = pBufMgrHeap->SpareBlockIndex;
    for (;;)
    {
        if (index == INVALID_INDEX)
            break;

        // make sure we're not off in the weeds.
        NV_ASSERT( index < pBufMgrHeap->ArraySize );
        NV_ASSERT( pBufMgrHeap->RawBlockArray[index].touched == 0);

        pBufMgrHeap->RawBlockArray[index].touched = 2;
        index = pBufMgrHeap->RawBlockArray[index].NextIndex;
    }


    // check that all blocks get touched.
    for (index = 0; index < pBufMgrHeap->ArraySize; ++index)
    {
        NV_ASSERT(pBufMgrHeap->RawBlockArray[index].touched != 0); 
    }
}

#else
    #define SanityCheckHeap(NvMMBufMgrHandle)
#endif



NvError NvMMBufMgrInit(NvMMBufMgrHandle *hNvMMBufMgr, NvRmPhysAddr base, NvU32 vbase,NvU32 size)
{
    NvMMBufMgrHeap *pBufMgrHeap = NULL;

    int     i;
    NvError err      = NvError_InsufficientMemory;

    NV_ASSERT(size > 0);

    pBufMgrHeap = NvOsAlloc(sizeof(NvMMBufMgrHeap));
    if (!pBufMgrHeap)
    {
        err = NvError_InsufficientMemory;
        goto fail;
    }
    NvOsMemset(pBufMgrHeap, 0, sizeof(NvMMBufMgrHeap));

    pBufMgrHeap->base      = base;
    pBufMgrHeap->vbase   = vbase;
    pBufMgrHeap->size      = size;
    pBufMgrHeap->ArraySize = INITIAL_GROW_SIZE;

    pBufMgrHeap->RawBlockArray = NvOsAlloc(sizeof(NvMMBufMgrBlock) * INITIAL_GROW_SIZE);
    if (!pBufMgrHeap->RawBlockArray)
    {
        err = NvError_InsufficientMemory;
        goto fail;
    }
    NvOsMemset(pBufMgrHeap->RawBlockArray, 0, sizeof(NvMMBufMgrBlock) * INITIAL_GROW_SIZE);


    // setup all of the pointers (indices, whatever)
    for (i = 0; i < INITIAL_GROW_SIZE; ++i)
    {
        pBufMgrHeap->RawBlockArray[i].NextIndex = i + 1;
    }
    pBufMgrHeap->RawBlockArray[i-1].NextIndex = INVALID_INDEX;

    pBufMgrHeap->BlockIndex      = 0;
    pBufMgrHeap->SpareBlockIndex = 1;

    pBufMgrHeap->RawBlockArray[pBufMgrHeap->BlockIndex].IsFree    = NV_TRUE;
    pBufMgrHeap->RawBlockArray[pBufMgrHeap->BlockIndex].PhysAddr  = base;
    pBufMgrHeap->RawBlockArray[pBufMgrHeap->BlockIndex].virtualAddr = vbase;
    pBufMgrHeap->RawBlockArray[pBufMgrHeap->BlockIndex].size      = size;
    pBufMgrHeap->RawBlockArray[pBufMgrHeap->BlockIndex].NextIndex = INVALID_INDEX;

    //err = NvOsMutexCreate(&pBufMgrHeap->mutex);
    //if (err)
    //  goto fail;
    *hNvMMBufMgr = pBufMgrHeap;
    SanityCheckHeap(hNvMMBufMgr);
    return NvSuccess;

fail:
    if (pBufMgrHeap)
    {
        NvOsFree(pBufMgrHeap->RawBlockArray);
        NvOsFree(pBufMgrHeap);
    }
    *hNvMMBufMgr = NULL;
    return err;
}



/**
 * Frees up a heap structure, and all items that were associated with this heap
 *
 * @param pBufMgrHeap Pointer to the heap structure returned from NvRmPrivHeapSimpleHeapAlloc
 */

void NvMMBufMgrDeinit(NvMMBufMgrHandle hNvMMBufMgr)
{
    NvMMBufMgrHeap *pBufMgrHeap = hNvMMBufMgr;
    if (pBufMgrHeap)
    {
        SanityCheckHeap(pBufMgrHeap);
        NvOsFree(pBufMgrHeap->RawBlockArray);
        pBufMgrHeap->RawBlockArray = NULL;
        NvOsFree(pBufMgrHeap);
        pBufMgrHeap = NULL;
    }
}



//NvRmPhysAddr NvMMBufMgrAlloc(NvU32 size, NvU32 align)
NvU32 NvMMBufMgrAlloc(NvMMBufMgrHandle hNvMMBufMgr, NvU32 size, NvU32 align)
{
    NvMMBufMgrBlock *pBlock;
    NvU32               BlockIndex;

    NvMMBufMgrHeap *pBufMgrHeap = hNvMMBufMgr;

    // Must align to a power of two
    // Alignment offset should be less than the total alignment
    if ((align & (align-1)))
        return 0;

    NV_ASSERT(pBufMgrHeap != NULL);

//    NvOsMutexLock(pBufMgrHeap->mutex);

    // We might need as many as two additional blocks (due to alignment waste).
    // Make sure that we have enough blocks in advance.
    if (pBufMgrHeap->SpareBlockIndex == INVALID_INDEX || 
        pBufMgrHeap->RawBlockArray[pBufMgrHeap->SpareBlockIndex].NextIndex == INVALID_INDEX)
    {
        // Out of spare blocks.  Get more of them.
        NvU32          i;
        NvU32          NewArraySize;
        NvU32          GrowSize;
        NvMMBufMgrBlock *NewBlockArray;

        GrowSize = pBufMgrHeap->ArraySize + (pBufMgrHeap->ArraySize >> 1);
        GrowSize = NV_MIN(GrowSize, MAX_GROW_SIZE);


        // Grow by 8 ensures that we have at least 2 blocks and also is a little
        // more efficient than growing by 1 each time.
        NewArraySize = pBufMgrHeap->ArraySize + GrowSize;        
        NewBlockArray = NvOsAlloc( sizeof(NvMMBufMgrBlock)*NewArraySize);
        if (!NewBlockArray)
        {
            SanityCheckHeap(pBufMgrHeap);
//            NvOsMutexUnlock(pBufMgrHeap->mutex);

            return 0;
        }
        NvOsMemset(NewBlockArray, 0, sizeof(NvMMBufMgrBlock)*NewArraySize);

        NvOsMemcpy(NewBlockArray, pBufMgrHeap->RawBlockArray, sizeof(NvMMBufMgrBlock)*pBufMgrHeap->ArraySize);

        // setup the NextIndex in the new part of the array
        for (i = pBufMgrHeap->ArraySize; i < NewArraySize; i++)
        {
            NewBlockArray[i].NextIndex = i + 1;
        }

        // Point the last element of the new array to the old SpareBlockList
        NewBlockArray[NewArraySize - 1].NextIndex = pBufMgrHeap->SpareBlockIndex;
        NvOsFree(pBufMgrHeap->RawBlockArray);

        // Update all our information
        pBufMgrHeap->RawBlockArray   = NewBlockArray;
        pBufMgrHeap->SpareBlockIndex = pBufMgrHeap->ArraySize;
        pBufMgrHeap->ArraySize       = NewArraySize;
    }

    // Scan through the list of blocks
    for (BlockIndex = pBufMgrHeap->BlockIndex; BlockIndex != INVALID_INDEX; BlockIndex = pBufMgrHeap->RawBlockArray[BlockIndex].NextIndex)
    {
        NvRmPhysAddr NewOffset;
        NvU32        ExtraAlignSpace;

        pBlock = &pBufMgrHeap->RawBlockArray[BlockIndex];

        // Skip blocks that are not free
        if (!pBlock->IsFree)
        {
            continue;
        }

        // Compute location where this allocation would start in this block, based
        // on the alignment and range requested
        #if VIRTUAL
        NewOffset = pBlock->virtualAddr;
    #else   
        NewOffset = pBlock->PhysAddr;
    #endif

        NewOffset = (NewOffset + align-1) & ~(align-1);

        #if VIRTUAL 
        NV_ASSERT(NewOffset >= pBlock->virtualAddr);
        ExtraAlignSpace = NewOffset - pBlock->virtualAddr;  
    #else
        NV_ASSERT(NewOffset >= pBlock->PhysAddr);
        ExtraAlignSpace = NewOffset - pBlock->PhysAddr;
    #endif

        // Is the block too small to fit this allocation, including the extra space
        // required for alignment?
        if (pBlock->size < (size + ExtraAlignSpace))
            continue;

        // Do we need to split this block in two to start the allocation at the proper
        // alignment?
        if (ExtraAlignSpace > 0)
        {
            NvMMBufMgrBlock *NewBlock;
            NvU32                NewBlockIndex;

            // Grab a block off the spare list and link it into place
            NewBlockIndex          = pBufMgrHeap->SpareBlockIndex;
            NewBlock               = &pBufMgrHeap->RawBlockArray[NewBlockIndex];
            pBufMgrHeap->SpareBlockIndex = NewBlock->NextIndex;

            NewBlock->NextIndex    = pBlock->NextIndex;
            pBlock->NextIndex      = NewBlockIndex;

            // Set up the new block
            NewBlock->IsFree   = NV_TRUE;
            #if VIRTUAL             
            NewBlock->virtualAddr= pBlock->virtualAddr + ExtraAlignSpace;           
            #else   
            NewBlock->PhysAddr = pBlock->PhysAddr + ExtraAlignSpace;            
            #endif          
            NewBlock->size     = pBlock->size - ExtraAlignSpace;

            // Shrink the current block to match this allocation
            pBlock->size = ExtraAlignSpace;

            // Advance to the block we are actually going to allocate out of
            pBlock = NewBlock;
        }

        // Do we need to split this block into two?
        if (pBlock->size > size)
        {
            NvMMBufMgrBlock *NewBlock;
            NvU32                NewBlockIndex;

            // Grab a block off the spare list and link it into place
            NewBlockIndex          = pBufMgrHeap->SpareBlockIndex;
            NewBlock               = &pBufMgrHeap->RawBlockArray[NewBlockIndex];
            pBufMgrHeap->SpareBlockIndex = NewBlock->NextIndex;
            NewBlock->NextIndex    = pBlock->NextIndex;
            pBlock->NextIndex      = NewBlockIndex;

            // Set up the new block
            NewBlock->IsFree   = NV_TRUE;
            #if VIRTUAL             
            NewBlock->virtualAddr = pBlock->virtualAddr + size;         
            #else           
            NewBlock->PhysAddr = pBlock->PhysAddr + size;
            #endif      
            NewBlock->size     = pBlock->size - size;

            // Shrink the current block to match this allocation
            pBlock->size = size;
        }

        NV_ASSERT(pBlock->size == size);
        pBlock->IsFree = NV_FALSE;

        SanityCheckHeap(pBufMgrHeap);

        //NvOsMutexUnlock(pBufMgrHeap->mutex);
        #if VIRTUAL                     
        return pBlock->virtualAddr;     
        #else   
        return pBlock->PhysAddr;
        #endif
    }

    SanityCheckHeap(pBufMgrHeap);
    //NvOsMutexUnlock(pBufMgrHeap->mutex);
    return 0;
}



#if VIRTUAL
void NvMMBufMgrFree(NvMMBufMgrHandle hNvMMBufMgr, NvU32 vAddr)
#else
void NvMMBufMgrFree(NvMMBufMgrHandle hNvMMBufMgr, NvRmPhysAddr PhysAddr)
#endif
{
    NvMMBufMgrBlock *pBlock = NULL;
    NvMMBufMgrBlock *pNext = NULL;
    NvMMBufMgrBlock *pPrev = NULL;

    NvU32 BlockIndex;
    NvU32 PrevIndex = INVALID_INDEX;
    NvU32 NextIndex = INVALID_INDEX;

    NvMMBufMgrHeap *pBufMgrHeap = hNvMMBufMgr;

    NV_ASSERT(pBufMgrHeap != NULL);

    //NvOsMutexLock(pBufMgrHeap->mutex);

    // Find the block we're being asked to free
    BlockIndex = pBufMgrHeap->BlockIndex;
    pBlock = &pBufMgrHeap->RawBlockArray[BlockIndex];
    #if VIRTUAL
    while (BlockIndex != INVALID_INDEX && (pBlock->virtualAddr!= vAddr))    
    #else   
    while (BlockIndex != INVALID_INDEX && (pBlock->PhysAddr != PhysAddr))
    #endif      
    {
        PrevIndex  = BlockIndex;
        BlockIndex = pBlock->NextIndex;
        pBlock     = &pBufMgrHeap->RawBlockArray[BlockIndex];
    }

    // The block we're being asked to free didn't exist or was already free
    if (BlockIndex == INVALID_INDEX || pBlock->IsFree)
    {
        SanityCheckHeap(pBufMgrHeap);
        //NvOsMutexUnlock(pBufMgrHeap->mutex);
        return;
    }

    // This block is now a free block
    pBlock->IsFree = NV_TRUE;

    // If next block is free, merge the two into one block
    NextIndex = pBlock->NextIndex;
    pNext = &pBufMgrHeap->RawBlockArray[NextIndex];
    if (NextIndex != INVALID_INDEX && pNext->IsFree)
    {
        pBlock->size += pNext->size;
        pBlock->NextIndex = pNext->NextIndex;

        pNext->NextIndex       = pBufMgrHeap->SpareBlockIndex;
        pBufMgrHeap->SpareBlockIndex = NextIndex;
    }

    // If previous block is free, merge the two into one block
    pPrev = &pBufMgrHeap->RawBlockArray[PrevIndex];
    if (PrevIndex != INVALID_INDEX && pPrev->IsFree)
    {
        pPrev->size += pBlock->size;
        pPrev->NextIndex = pBlock->NextIndex;

        pBlock->NextIndex = pBufMgrHeap->SpareBlockIndex;
        pBufMgrHeap->SpareBlockIndex = BlockIndex;
    }
    SanityCheckHeap(pBufMgrHeap);
    //NvOsMutexUnlock(pBufMgrHeap->mutex);
}

NvU32 NvMMBufMgrGetTotalFreeMemoryAvailable(NvMMBufMgrHandle hNvMMBufMgr)
{
    NvU32 totalFreeMemory=0;
    NvMMBufMgrBlock *pBlock;
    NvU32 BlockIndex;

    NvMMBufMgrHeap *pBufMgrHeap = hNvMMBufMgr;

    // Scan through the list of blocks
    for (BlockIndex = pBufMgrHeap->BlockIndex; BlockIndex != INVALID_INDEX; BlockIndex = pBufMgrHeap->RawBlockArray[BlockIndex].NextIndex)
    {
        pBlock = &pBufMgrHeap->RawBlockArray[BlockIndex];
        // sum up the blocks that are free
        if (pBlock->IsFree)
        {
            totalFreeMemory+=pBlock->size;
        }
    }

    return totalFreeMemory;
}

NvU32 NvMMBufMgrGetLargestFreeChunkAvailable(NvMMBufMgrHandle hNvMMBufMgr)
{
    NvU32 largestFreeBlock=0;
    NvMMBufMgrBlock *pBlock;
    NvU32 BlockIndex;

    NvMMBufMgrHeap *pBufMgrHeap = hNvMMBufMgr;

    // Scan through the list of blocks
    for (BlockIndex = pBufMgrHeap->BlockIndex; BlockIndex != INVALID_INDEX; BlockIndex = pBufMgrHeap->RawBlockArray[BlockIndex].NextIndex)
    {
        pBlock = &pBufMgrHeap->RawBlockArray[BlockIndex];
        // find the largest free block
        if (pBlock->IsFree && pBlock->size > largestFreeBlock)
        {
            largestFreeBlock = pBlock->size;
        }
    }

    return largestFreeBlock;
}
