/*
 * Copyright (c) 2011-2014 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVRM_CHANNEL_PRIV_H
#define INCLUDED_NVRM_CHANNEL_PRIV_H

#include "nvrm_channel.h"

#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

/**
 * Structure describing a command buffer being submitted to a channel.
 */
typedef struct NvRmCommandBufferRec
{
    // The handle of the memory allocation in which the commands live
    NvRmMemHandle hMem;

    // The byte offset where the commands start within that memory.
    // Must be a multiple of 4.
    NvU32 Offset;

    // The number of 32-bit words in the command buffer.
    NvU32 Words;
} __packed NvRmCommandBuffer;

/**
 * Relocation structure that allows address patching in the kernel rather than
 * several traps to update from user space.
 */
typedef struct NvRmChannelSubmitRelocRec
{
    // The submitted command buffer
    NvRmMemHandle hCmdBufMem;

    // The offset into the command buffer for the relocation
    NvU32 CmdBufOffset;

    // The memory that will be pinned and patched into the command buffer

    NvRmMemHandle hMemTarget;

    // The offset within hMemTarget for the memory address patching
    NvU32 TargetOffset;
} NvRmChannelSubmitReloc;

/**
 * Tracking structure that allows wait patching in the stream at flush time.
 */
typedef struct NvRmChannelWaitChecksRec
{
    // The submitted command buffer
    NvRmMemHandle hCmdBufMem;

    // The offset into the command buffer of wait method data
    NvU32 CmdBufOffset;

    // The syncpt index of host wait
    NvU32 SyncPointID;

    // The syncpt threshold of wait
    NvU32 Threshold;
} NvRmChannelWaitChecks;


typedef struct NvRmSyncPointDescriptor {
    // ID for this syncpoint
    NvU32 SyncPointID;

    // Number of increments
    NvU32 Value;

    // ID for related waitbase
    NvU32 WaitBaseID;

    // Previous used syncpoint
    NvS32 Prev;

    // Next used syncpoint
    NvS32 Next;
} NvRmSyncPointDescriptor;

/**
 * Submits a list of command buffers to a channel.  These command buffers will
 * be executed in the order listed, uninterrupted by other blocks of commands
 * in the same channel.  (That is, if two threads call NvRmChannelSubmit at the
 * same time on the same channel with NumCommandBufs greater than 1, their
 * command buffers will not be interleaved with one another -- one or the other
 * will get to go first with all of its buffers.)
 *
 * The command buffer memory does not need to be pinned.  The RM will pin and
 * unpin it automatically on the client's behalf.
 *
 * The last command buffer in the list must have a sync point increment as its
 * final command so that we can determine when these commands have all finished
 * being fetched.  The client must inform this function what sync point ID and
 * what value it needs to wait on to conclude that these command buffers are
 * done being fetched.  This will be used to know when to unpin the command
 * buffer memory, and possibly for other internal RM purposes.
 *
 * Note: When using the stream api this function is called by NvRmStreamFlush()
 * and does not generally need to be called by the client.
 *
 * The number of handles passed (NumCommandBufs + NumRelocations) must be <=
 * NVRM_CHANNEL_SUBMIT_MAX_HANDLES.
 *
 * @param hChannel Channel handle obtained from NvRmChannelOpen().
 * @param pCommandBufs Array of structs describing the command buffers.
 * @param NumCommandBufs Number of command buffers being submitted.
 *     Must be <= NVRM_GATHER_QUEUE_SIZE-1.
 * @param pRelocations Array of relocation structures.
 * @param NumRelocations Number of relocations for this submit
 * @param pWaitChecks Array of wait check structures.
 * @param NumWaitChecks Number of wait syncpts to check for this submit.
 * @param hContext If not NULL, restore hardware state to this context before
 *     submitting commands to the hardware.
 * @param pContextExtra If not NULL, add these commands right after restoring
 *     the hardware state.
 *     XXX This is likely to change.
 * @param ContextExtraCount Number of pContextExtra commands.
 * @param NullKickoff Hardware will not fetch the command buffer. All other
 *     work is done. Useful for software performance analysis.
 * @param ModuleID The 'LastEngineUsed' field of the submitting Stream.
 * @param SyncPointID ID of sync point incremented at the end of the last
 *     command buffer in the list.
 * @param SyncPointsUsed Total number of sync point increments in submitted
 *     command buffers.
 * @param SyncPointWaitMask Mask of wait syncpt references in this submit.
 * @param pSyncPointValue Value that this sync point will take on once it has
 *     been incremented by that last command in the last command buffer.
 */

/**
 * Maximum number of handles per submit.
 * This is sized for the stream api's use, so it's NVRM_CMDBUF_GATHER_SIZE +
 * NVRM_CMDBUF_RELOC_SIZE.
 */
enum { NVRM_CHANNEL_SUBMIT_MAX_HANDLES = 1280 };
NvError NvRmChannelSubmit(
    NvRmChannelHandle hChannel,
    const NvRmCommandBuffer *pCommandBufs, NvU32 NumCommandBufs,
    const NvRmChannelSubmitReloc *pRelocations, const NvU32 *RelocShifts,
    NvU32 NumRelocations,
    const NvRmChannelWaitChecks *pWaitChecks, NvU32 NumWaitChecks,
    NvRmContextHandle hContext,
    const NvU32 *pContextExtra, NvU32 ContextExtraCount,
    NvBool NullKickoff, NvRmModuleID ModuleID,
    NvU32 SyncPointIdx, NvU32 SyncPointWaitMask,
    NvU32 *pCtxChanged,
    NvU32 *pSyncPointValue,
    NvRmSyncPointDescriptor *SyncPointIncrs,
    NvU32 SyncPoints);

/**
 * Allocates a new channel and returns a handle to it.  This channel is for
 * your exclusive use; no one else will be using it.
 *
 * This API is not to be used in any production software.  It is intentionally
 * restricted to debug builds to enforce this.  Production software needs to
 * use statically preallocated channels, so that we don't ever run out of
 * channels.
 *
 * @param hDevice Handle to device the channel is for.
 * @param phChannel Pointer to to be filled in with the channel handle.
 */
NvError NvRmChannelAlloc(
    NvRmDeviceHandle hDevice,
    NvRmChannelHandle *phChannel);

/**
 * Sets a priority for submits in the channel. Requires the caller to take
 * the returned sync point and wait base into use for all future streams.
 *
 * It is not allowed to call NvRmChannelGetModuleSyncPoint() and
 * NvRmChannelSetPriority() on the same channel.
 *
 * @param hChannel Handle to the channel for NvRmStream submits
 * @param Priority Priority of all further submits.
 *        Use values from nvhost_ioctl.h.
 * @param SyncPointIndex Index of sync point to return
 * @param WaitBaseIndex Index of wait base to return
 * @param SyncPointID The returned sync point id
 * @param WaitBase The returned wait wait base
 */
NvError NvRmChannelSetPriority(
    NvRmChannelHandle hChannel,
    NvU32 Priority,
    NvU32 SyncPointIndex,
    NvU32 WaitBaseIndex,
    NvU32 *SyncPointID,
    NvU32 *WaitBase);

/**
 * Sets context switch buffers
 *
 * @param hChannel Handle to the channel for NvRmStream submits
 * @param hSave Save buffer
 * @param hRestore Restore buffer
 * @param Reloc offset
 * @param SyncptId Sync point id
 * @param SaveIncrs Save increments
 * @param RestoreIncrs Restore increments
 */
NvError NvRmChannelSetContextSwitch(
    NvRmChannelHandle hChannel,
    NvRmMemHandle hSave,
    NvU32 SaveWords,
    NvU32 SaveOffset,
    NvRmMemHandle hRestore,
    NvU32 RestoreWords,
    NvU32 RestoreOffset,
    NvU32 RelocOffset,
    NvU32 SyncptId,
    NvU32 WaitBase,
    NvU32 SaveIncrs,
    NvU32 RestoreIncrs);

#endif
