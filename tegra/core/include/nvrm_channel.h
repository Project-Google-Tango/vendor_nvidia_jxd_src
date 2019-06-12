/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_nvrm_channel_H
#define INCLUDED_nvrm_channel_H


#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvrm_memmgr.h"
#include "nvrm_module.h"
#include "nvrm_init.h"

/**
 * @defgroup nvrm_channel RM Channel Services
 *
 * @ingroup nvddk_rm
 *
 * Channels and Streams
 * --------------------
 *
 * There are two key concepts to understand here: channels and streams.
 *
 * A channel is a hardware concept.  Each channel in the hardware processes an
 * ordered sequence of commands.  The commands live in memory and are DMA'd into
 * the command FIFO.  A channel only supports one operation: you can submit
 * command buffers to it.  As a client of a channel, it is your responsibility
 * to allocate the command buffers, write appropriate sequences of commands into
 * them, and synchronize (to determine when the hardware is done processing your
 * command buffer, so you can overwrite it, free it, or read back the results).
 * There is no direct way to wait for the channel to finish executing the
 * commands you have submitted!
 *
 * Such a low-level interface is difficult to use directly, so we provide a
 * higher-level concept called a "stream" that makes it easy to use a channel.
 * In theory you could bypass the stream API and write directly to the channel
 * API, but this is strongly discouraged.
 *
 * A stream is purely a software construct.  Streams provide an easy way to
 * assemble command buffers one word at a time and submit them to a channel.
 * Streams take care of keeping track of what command buffers are currently in
 * use by the hardware, so you don't overwrite commands that are in flight.
 *
 * If we had unlimited hardware resources each stream could have its own
 * channel, but in practice the number of channels is very limited, so we need
 * to share channels between streams.  An unlimited number of streams may be
 * bound to a particular channel.  This sharing must be efficient.  We don't
 * want to take a system-wide mutex every single time we want to write a command
 * to a stream.  Instead, we only want to take a mutex when we flush a stream
 * (submit its commands to the hardware).
 *
 * You can create as many streams as you want, subject only to the amount of
 * memory in the system.  Because writing to a stream (for performance reasons)
 * doesn't acquire a mutex, multiple threads cannot share a stream unless they
 * provide their own higher-level locking.  (You may be better off creating a
 * separate stream for each thread to avoid locks.)
 *
 * Channels can be shared across threads and processes, but not across CPUs
 * (i.e., no sharing of a channel between ARM and AVP).  Streams can never be
 * shared across processes or CPUs, and they can only be shared across threads
 * if higher-level locking is provided.
 *
 * A stream is bound to a particular channel at creation time.  It is not
 * possible to re-bind it to a different channel later.
 *
 * A stream must also be bound to a particular engine at creation time.  If you
 * want to communicate with multiple hardware engines, you will need to create
 * multiple streams.
 *
 * The stream API's definition of "engine" is slightly loose.  On AP15, the VI
 * and ISP engines are lumped together into a single engine for the purposes of
 * the stream API -- the ISP engine is viewed as an extension of the VI engine.
 * The same is true of the 2D and EPP engines -- the EPP engine is viewed as an
 * extension of the 2D engine.
 *
 * Multiple Streams Sharing an Engine
 * ----------------------------------
 *
 * Every command buffer submitted to a channel executes sequentially within that
 * channel -- it cannot be interrupted partway through by other commands within
 * that same channel.  Further, within a channel, command buffers are guaranteed
 * to begin execution in the same order they are submitted.
 *
 * Commands do not necessarily complete execution in the same order they are
 * submitted -- this is only true if you are using just a single engine from
 * that channel.  If you are using multiple engines, the blocks of commands sent
 * to the engines might complete out of order.  Also, if multiple channels are
 * concurrently using the same engine, those channels' commands might be
 * arbitrarily interleaved with one another (with, in most cases, disastrous
 * effects).
 *
 * Since more than one stream might be using a channel and more than one channel
 * might be using an engine, you cannot make any assumptions about the initial
 * state of the hardware at the start of your command buffer.  Each time you
 * flush, some other stream might have used the engine in the meantime.
 *
 * Several tools are provided to deal with this problem.
 *
 * 1. The hardware provides mutexes to protect an engine from being programmed
 * by two channels simultaneously.  By convention, certain engines might only
 * ever be used from a single channel (3D and OpenVG are expected to follow this
 * model) and do not need mutexing, but other engines (such as 2D) are shared
 * between channels.  Access to a these shared engines must be protected with a
 * mutex or the behavior is undefined.  To prevent deadlock, every command
 * buffer submitted to a channel must have a release of every mutex it acquires;
 * you must never submit a command buffer that acquires a mutex and never
 * releases it.  (Further, be aware that mutexes are not recursive -- undefined
 * behavior results if you acquire a mutex you have already acquired.  Also, it
 * is probably unwise to acquire more than one mutex at a time, since this might
 * result in deadlock if mutexes are acquired in inconsistent order.)
 *
 * Each time you acquire the mutex, you must assume that someone else might have
 * had a chance to use the shared engine in the interim since the last you used
 * it.  This doesn't mean that the state of the engine is completely undefined,
 * though.  You can always have "by convention" rules like "if you enable
 * feature X, always disable it before releasing the mutex."  If every driver
 * obeys such a rule, then you don't have to explicitly re-disable feature X
 * every time.
 *
 * 2. Some engines (primarily those that have a lot more state, like 3D and OVG)
 * are only used from a single channel.  For these engines, an alternative way
 * of managing state for multiple streams is provided.  There is no need to use
 * any mutexes for these engines.  Every time it is about to submit a command
 * buffer, the stream implementation checks whether it was the last to use that
 * particular engine or whether some other stream has used the engine in the
 * interim.  If it was the last stream to use the engine, we are all set: we
 * know that the state of the engine hasn't changed.  If it was not, then we
 * have to reload all our state as of where we left off last time.
 * XXX This statement is imprecise about the distinction between a stream and a
 * context.  See bug 330297; this statement should be revised based on the
 * outcome of that bug.
 *
 * For each piece of state in such an engine, we have several options:
 * a) Never make any assumptions about it.  Always set it explicitly at least
 * once inside each of your command buffers.
 * b) As described before, have a convention about how to leave it at the end
 * of each command buffer.  For example, if a feature is rarely used, make sure
 * to disable it at the end of your command buffer, so the next user can assume
 * that it starts disabled.
 * c) Read the state out of the engine and save it off somewhere, and then use
 * this state to reload the engine the next time we use it.
 *
 * These options are not mutually exclusive.  We might choose to use a mixture
 * of all three if we so desired.
 *
 * RM will provide assistance with approach (c), assuming the hardware has the
 * readback facilities to make it possible.  It will track which stream was the
 * last to use each engine, and whenever a different stream starts using the
 * engine, it will implement the save and restore.
 *
 * Command Buffer Management and Control over Flushing
 * ---------------------------------------------------
 *
 * All of the above implies that drivers must be aware of when their command
 * buffers are being or might be submitted to the RM.  For example, if you are
 * going to acquire a mutex, you had better also release it before you flush.
 * Or, if you have a convention that you must restore some state back to its
 * initial value at the end of your command buffer, you must not flush until you
 * do that, and you had better reserve enough space at the end of your command
 * buffer that you can restore all the state.
 *
 * Some flushes are induced by the driver.  For example, if I need to wait for
 * the hardware to finish an operation, first I have to flush (or I'll hang).
 * This is the easy case -- drivers know when they are calling NvRmStreamFlush
 * and can ensure that this only happens at safe points.
 *
 * Other flushes must be inserted by the stream implementation from time to
 * time, at NVRM_STREAM_BEGIN points, as we run out of space to keep writing in
 * our command buffers.  There is also a tradeoff between the overhead of
 * flushing against the benefits of keeping the hardware busy and the need to
 * flush frequently enough that we can keep track of which command buffers have
 * been consumed with a reasonably small granularity.
 *
 * Also, different drivers may want to approach this tradeoff differently.  Some
 * drivers have expensive context switches and will probably need to flush less
 * frequently.  Such drivers will need to allocate significantly larger command
 * buffers, of course.
 *
 * When a driver wants to starts writing to a stream, it needs to know in
 * advance how much space it is going to need in the worst case before it
 * reaches a safe flush point again.  There are three possible scenarios:
 * 1. The space is available right now in your current command buffer
 * 2. The space exists in your current command buffer, but you will have to
 * wait for it
 * 3. The space does not exist in your current command buffer, no matter how
 * long you wait -- you must flush first and obtain a new command buffer.
 * (This could be either because you hit the end of your buffer and need to
 * wrap, or because your buffer is simply too small.)
 *
 * Cases (1) and (2) are easy and can be handled automatically by the stream
 * API.  Case (3) is more challenging if your programming model requires that
 * you insert a "last few" commands before the flush to restore the hardware to
 * some known state. For this purpose, you will either need to insert those
 * commands before every single NVRM_STREAM_END, or configure pre-flush callback
 * where you can insert those final commands. See pPreFlushCallback.
 *
 * The stream implementation may also insert auto-flushes every so often even
 * when they are not strictly required.  That is, the RM may flush, just to keep
 * the hardware busy, even though it has plenty of command buffer space to
 * continue writing commands.  These auto-flushes ought to increase performance
 * if used in moderation.
 *
 * Mechanics of Writing Commands to Command Buffers
 * ------------------------------------------------
 *
 * As a client of the API, you simply need to wrap all of your writes with a
 * NVRM_STREAM_BEGIN/END, providing a conservative estimate of the amount of
 * space required at BEGIN time.  Once you've called STREAM_BEGIN, you can start
 * writing using the pCurrent pointer that it fills in for you.
 *
 * In debug builds, asserts will verify that you haven't written more data than
 * you promised at STREAM_END time, that you don't do a BEGIN inside a
 * BEGIN/END pair, that you don't do an END outside a BEGIN/END pair, and that
 * you don't call NvRmStreamFlush inside a BEGIN/END pair.
 *
 * Very simple example pseudocode using a hypothetical 2D engine:
 *
 *    NvData32 *pCurrent;
 *    NVRM_STREAM_BEGIN(pStream, pCurrent, 10);
 *    NVRM_STREAM_PUSH_U(pCurrent, NVRM_CH_OPCODE_SET_CLASS(CLASS_ID_2D, 0, 0));
 *    NVRM_STREAM_PUSH_U(pCurrent, NVRM_CH_OPCODE_ACQUIRE_MUTEX(MUTEX_2D));
 *    NVRM_STREAM_PUSH_U(pCurrent, NVRM_CH_OPCODE_INCR(BLIT_SRC_OFFSET, 6));
 *    NVRM_STREAM_PUSH_U(pCurrent, SrcOffset);
 *    NVRM_STREAM_PUSH_U(pCurrent, DstOffset);
 *    NVRM_STREAM_PUSH_U(pCurrent, SrcPitch | (DstPitch << 16));
 *    NVRM_STREAM_PUSH_U(pCurrent, SrcX | (SrcY << 16));
 *    NVRM_STREAM_PUSH_U(pCurrent, DstX | (DstY << 16));
 *    NVRM_STREAM_PUSH_U(pCurrent, Width | (Height << 16));
 *    NVRM_STREAM_PUSH_U(pCurrent, NVRM_CH_OPCODE_RELEASE_MUTEX(MUTEX_2D));
 *    NVRM_STREAM_END(pStream, pCurrent);
 *
 * While this looks like straight-line code, you should be aware that BEGIN
 * may actually end up doing a function call to GetSpace, which in turn might
 * call Flush, which in turn might call your sync point base callback (if any).
 *
 * Memory Management Support
 * -------------------------
 *
 * The stream API provides some built-in support for automatic memory management
 * layered on top of the RM memory API.  Drivers that use this built-in support
 * never need to worry about pinning or unpinning their memory, and they will
 * (without even having to think about it) have no memory pinned when the
 * hardware is idle.
 *
 * The idea is that you never place any addresses directly in your command
 * buffer -- you only put in placeholders that get filled with the real
 * addresses at flush time (when we pin the memory).  When your command buffer
 * completes (as evidenced by an OP_DONE sync point), the RM will automatically
 * unpin the memory again.  Of course, the RM is free to implement some
 * optimizations to reduce CPU overhead, like delaying the automatic unpins
 * until they are really needed, but these optimizations should be invisible to
 * the client.
 *
 * Here is a rewritten version of the previous example showing how it works.
 * Rather than already having the addresses of the src and dst buffers, we only
 * have their memory handles (hSrcMem and hDstMem).
 *
 *    NvData32 *pCurrent;
 *    NVRM_STREAM_BEGIN_RELOC_GATHER(pStream, pCurrent, 10, 2, 0);
 *    NVRM_STREAM_PUSH_U(pCurrent, NVRM_CH_OPCODE_SET_CLASS(CLASS_ID_2D, 0, 0));
 *    NVRM_STREAM_PUSH_U(pCurrent, NVRM_CH_OPCODE_ACQUIRE_MUTEX(MUTEX_2D));
 *    NVRM_STREAM_PUSH_U(pCurrent, NVRM_CH_OPCODE_INCR(BLIT_SRC_OFFSET, 6));
 *    NVRM_STREAM_PUSH_RELOC(pStream, pCurrent, hSrcMem, 0);
 *    NVRM_STREAM_PUSH_RELOC(pStream, pCurrent, hSrcDst, 0);
 *    NVRM_STREAM_PUSH_U(pCurrent, SrcPitch | (DstPitch << 16));
 *    NVRM_STREAM_PUSH_U(pCurrent, SrcX | (SrcY << 16));
 *    NVRM_STREAM_PUSH_U(pCurrent, DstX | (DstY << 16));
 *    NVRM_STREAM_PUSH_U(pCurrent, Width | (Height << 16));
 *    NVRM_STREAM_PUSH_U(pCurrent, NVRM_CH_OPCODE_RELEASE_MUTEX(MUTEX_2D));
 *    NVRM_STREAM_END(pStream, pCurrent);
 *
 * This command buffer assumes nothing about the address of either surface.
 * When it is submitted, the two relocations will be filled in with the values
 * NvRmMemGetAddress(hSrcMem, 0) and NvRmMemGetAddress(hDstMem, 0).
 *
 * Note that you must provide a conservative estimate, once again, of how many
 * relocations you will need.
 *
 * Drivers are urged to use this framework if possible rather than writing
 * their own pin/unpin code.
 *
 * Sync Point Support
 * ------------------
 *
 * We use sync points to keep track of which command buffers are in flight.  The
 * stream implementation must stick a sync point increment method on the end of
 * every command buffer it submits.  When it wants to track the hardware's
 * progress in consuming and processing the command buffers, it can use these
 * sync points in lieu of polling the Get register (threads can and should go to
 * sleep while they are waiting for command buffer space to free up).
 *
 * Every class ID supports a standard sync point increment method at the same
 * offset and with the same bit positions for all of its fields.  However, in
 * practice, there end up being several special cases that RM has to deal with.
 * We will cover here only the "normal" case.
 *
 * Because every class has the same sync point increment method, we do not need
 * to know what engine was last in use to insert these methods.  From the point
 * of view of command buffer consumption, we don't need to know: for an engine
 * to have finished processing all of its commands up through a particular
 * point, clearly all those commands must have been fetched from memory and must
 * be safe to overwrite.
 *
 * However, from the point of view of the automatic unpinning that must take
 * place for memory management, we do care: sync point increments can arrive at
 * the sync block out of order, which might have the disastrous consequence of
 * unpinning the wrong memory.  Commands may be fetched in order, but they do
 * not complete in order.
 *
 * We deal with this by taking advantage of the fixed mapping of streams to
 * channels and engines.  For any given [channel, engine] pair, commands *do*
 * complete in order.  Therefore, a single sync point register is sufficient to
 * track the completion of commands for any such pair, or for any single stream.
 *
 * In general, we will simply stick an OP_DONE syncpt increment of the register
 * in question (for that particular channel and engine) on the end of each
 * command buffer.  The completion of the engine's work, as indicated by the
 * completion of the OP_DONE, will be used as an indication that all the memory
 * is safe to unpin.  Note that this requires the HW group to implement OP_DONE
 * consistently for all engines: OP_DONE needs to imply that all previous
 * commands have completed and committed all their results to memory.
 *
 * Drivers are also allowed to use sync points in their own command buffers.
 * If they just want to insert their own OP_DONE's, they can reuse the same
 * syncpt register that is used automatically by the RM.  To do this, however,
 * drivers must keep track of how many sync point increments they've done in the
 * SyncPointsUsed field of the stream structure.  They can determine which
 * sync point register to use for by looking at the SyncPointID field.
 *
 * When a command buffer that reuses the RM's sync point register is submitted,
 * the RM will keep track of how many times in total that particular sync point
 * has been incremented, and will provide a callback telling the client what
 * sync point value its buffer actually started with.
 *
 * When you want to wait for a sync point on the host CPU, you first may need
 * to flush to ensure that the hardware will process the sync point method.
 * Then, you would call NvRmChannelSyncPointWait to wait until the hardware
 * has reached or passed that sync point.  This allows your thread to go to
 * sleep while it is waiting for the hardware to finish.
 *
 * Gather Support
 * --------------
 *
 * We allow drivers to provide precomputed command buffers to avoid the overhead
 * of data copying.  There is some overhead to calling, or "gathering", such a
 * command buffer, so this feature should generally only be used if the command
 * buffer is large -- think hundreds of words, not 5 words.  (More precise
 * guidance is likely to depend on the chip, the OS, etc.)
 *
 * These buffers can be inserted at any "method boundary" point inside a
 * BEGIN/END pair (i.e. at points where a header word, not a data word, is
 * expected next).  Note, however, that relocations are not supported for such
 * buffers.  If you want relocation support you will have to pay the overhead
 * of a copy (so that the copy can be patched).
 *
 * The maximum number of gathers you plan to use must be specified at
 * NVRM_STREAM_BEGIN_RELOC_GATHER time, and when you want to insert one, you
 * simply use the NVRM_STREAM_PUSH_GATHER macro.
 *
 * Debug Support
 * -------------
 *
 * Debug implementations of the RM will perform extensive error checking on the
 * usage of this API.  Some examples have been given previously, but one
 * important category of error checking that hasn't been touched on is command
 * buffer parsing.
 *
 * Normally, command buffers are considered write-only.  If possible, we will
 * place them in write-combined memory, which is very slow for reading.
 * Drivers should generally not attempt to read their command buffers, and they
 * should always write to their command buffers in increasing order without
 * skipping words or going back to write a word again.
 *
 * At the same time, there is a vast amount of validation that can be performed
 * if we parse the command buffers to check for errors like mutex acquires that
 * are not properly paired with releases, or that the SyncPointsUsed and
 * SyncPointID fields are filled in correctly.  Unfortunately, such validation
 * is likely too slow to be enabled by default.
 *
 * RM should provide a configuration variable (details TBD) that can be used to
 * enable this sort of detailed checking at runtime in debug builds.
 *
 * Open Issues
 * -----------
 *
 * 1. How do we detect, report, and possibly recover from from timeout errors?
 *
 * 2. There is some benefit in allowing Pin to fail.  For example, this would
 * allow us to "overcommit" the GART; we could allow more than 16MB (or whatever
 * the GART size is) of GART-visible memory to be allocated, and plug the needed
 * PTEs into the GART at flush time, using the GART more like a software-managed
 * PTE cache than like a heap.  This is dangerous, however, because a single
 * flush might require more than 16MB worth of GART space.  There are clearly
 * some performance benefits and memory savings to be had here, especially when
 * dealing with large datasets, but there are also quite a few pitfalls.  The
 * Pin failure would only be discovered at Flush time, at which point it is too
 * late to do much of anything about it.
 *
 * @ingroup nvrm_channel
 * @{
 */

/**
 * A type-safe handle for a channel.
 */

typedef struct NvRmChannelRec *NvRmChannelHandle;

/**
 * A type-safe handle for a hardware context, for the engines where such a
 * concept is applicable.
 */

typedef struct NvRmContextRec *NvRmContextHandle;

typedef struct NvRmContextShadowPrivateRec NvRmContextShadowPrivate;

/**
 * A "fence" is simply a wrapper data structure containing a [sync point ID,
 * value] pair.  This can be used by higher-level drivers to describe an event
 * that we might want to wait for.
 */

typedef struct NvRmFenceRec
{
    NvU32 SyncPointID;
    NvU32 Value;
} NvRmFence;

/**
 * A relocation is an entry in the command buffer that we need to fix up before
 * submitting it to the hardware.  At command buffer construction time a memory
 * allocation may be unpinned, so we do not know the correct address.  We leave
 * a placeholder word and save off a relocation that tells us to fix it up
 * later once the allocation has been pinned.
 */
typedef struct NvRmCmdBufRelocationRec
{
    NvData32 *pTarget;
    NvRmMemHandle hMem;
    NvU32 Offset;
    NvU32 Shift;
} NvRmCmdBufRelocation;

/**
 * The client of the stream API can provide it a table of relocation shadows.
 * Each entry in this table contains the current [hMem, Offset] that a given
 * register in the unit has been programmed to.  Every time a flush happens,
 * either explicitly (NvRmStreamFlush) or implicitly (NvRmStreamGetSpace), the
 * next command buffer is automatically initialized with these relocations.
 * This ensures that if the RM has moved any of these buffers, these registers
 * are reloaded with the correct new addresses.
 *
 * Any table entry with hMem set to NULL will be skipped.
 */
typedef struct
{
    NvU32 Register;
    NvRmMemHandle hMem;
    NvU32 Offset;
} NvRmRelocationShadow;

/**
 * A "gather" can be inserted in a command buffer to tell the hardware to go
 * off and fetch the next chunk of commands from a specified piece of memory.
 * This allows precomputed chunks of commands to be reused without data copies.
 */
typedef struct NvRmCmdBufGatherRec
{
    NvData32 *pCurrent;
    NvRmMemHandle hMem;
    NvU32 Offset;
    NvU32 Words;
} NvRmCmdBufGather;

/**
 * A structure for storing an accurate time
 */
typedef struct NvRmTimevalRec
{
    NvU64 sec;
    NvU64 nsec;
} NvRmTimeval;

/**
 * A "wait" is inserted in a command buffer to synchronize against a preceding
 * syncpt increment (i.e. a hardware engine completing a series of methods).
 *
 * The stream API provides a means for knowing about pending waits, so these
 * can be patched at submit time, if the RM finds the condition has already
 * been met (while the stream was constructed, but not yet flushed).
 */
typedef struct NvRmCmdBufWaitRec
{
    NvData32 *pCurrent;
    NvRmFence Wait;
} NvRmCmdBufWait;

/**
 * Maximum number of relocations per command buffer.
 */
enum { NVRM_STREAM_RELOCATION_TABLE_SIZE = 256 };

/**
 * Maximum number of gathers per command buffer.
 */
enum { NVRM_STREAM_GATHER_TABLE_SIZE = 16 };

/**
 * Maximum number of waits per command buffer.
 */
enum { NVRM_STREAM_WAIT_TABLE_SIZE = 16 };

/**
 *  Valid flags for NvRmStream structure.
 */
enum NvRmStreamFlag
{
    NvRmStreamFlag_Disasm = 1 << 0
};

/**
 * An opaque data type where the RM can stash RM-private data pertaining to a
 * stream.
 */
typedef struct NvRmStreamPrivateRec NvRmStreamPrivate;

/**
 * An exposed (not opaque) structure for a stream.
 */
typedef struct NvRmStreamRec
{
    // We need to know how many sync point increments have been used in each
    // command buffer.  (Parsing it would be very slow.)  The client must keep
    // these variables up to date accordingly (which sync point they are using,
    // and how many times they have used it in the current buffer).  Also, we
    // need to insert an extra sync point every time we flush, in order to know
    // when (1) the command buffer has been consumed and (2) memory can be
    // unpinned.  We use the sync point ID specified here for that operation.
    // RM will automatically reset SyncPointsUsed to zero after every flush.
    // The SyncPointID field may only be modified immediately after a
    // NvRmStreamInit() or NvRmStreamFlush() call.  The SyncPointsUsed field is
    // expected to be incremented by the client whenever the client inserts a
    // syncpoint-increment command.
    NvU32 SyncPointID;
    NvU32 SyncPointsUsed;

    // Wait base used in this channel.
    NvU32 WaitBaseID;

    // Use Immediate sync pt increments instead of Operation Done to determine
    // when a command buffer is flushed.
    // For streaming blocks, like VI in which OpDone does not indicate
    // programming is done, but instead indicates when an image is captured,
    // Immediate is a better choice.
    NvBool UseImmediate;

    // Client-managed sync point. The sync point will not be touched by the
    // stream/channel software. SyncPointsUsed will not be modified, nor will
    // there be a sync point increment appended during the flush.
    NvBool ClientManaged;

    // The NULL-Kickoff driver for a given stream: all of the "work" is done
    // but the hardware doesn't ever fetch the data. SyncPointID will be
    // incremented by SyncPointsUsed.
    NvBool NullKickoff;

    // ErrorFlag keeps first detected NvError code since last
    // NvRmStreamGetError() call. It is reset to NvSuccess
    // by NvRmStreamGetError() and gets updated by NvRmStreamSetError()
    NvError ErrorFlag;

    // The engine that the stream is using.
    NvRmModuleID LastEngineUsed;

    /* The id of the class the stream is using */
    NvU32 LastClassUsed;

    // The hardware context, if any, currently being used by this stream. Leave
    // as NULL if none is needed.  This field may only be modified immediately
    // after a NvRmStreamInit() or NvRmStreamFlush() call.
    // XXX See bug 330297 for proposed API changes.
    NvRmContextHandle hContext;

    // XXX hContext should be made a transparent structure with a private
    // pointer and a public part.  The two fields that follow become public
    // part of hContext.  Our marshalling does not currently descent into
    // structures, so these fields stay here for now.
    NvU32 *pContextShadowCopy;
    NvU32 ContextShadowCopySize;
    NvRmContextShadowPrivate *pShadowPriv;

    // Sync point callback: this callback will inform the client what sync
    // point base it actually ended up getting on each flush.  Can be left
    // NULL if not needed.  If not NULL, it will be called every time
    // NvRmStreamFlush() is called on this stream.  The SyncPointBase parameter
    // is the value that the syncpt register (specified by SyncPointID) will
    // have before any of the syncpt increment commands that are currently
    // being flushed take affect.
    // XXX Should this be renamed to something more like "post-flush callback"?
    void (*pSyncPointBaseCallback)(
        struct NvRmStreamRec *pStream,
        NvU32 SyncPointBase,
        NvU32 SyncPointsUsed);

    // Private data member available for use by the client of the API.  You can
    // do whatever you want with this field.
    void *pClientPriv;

    // Callback function that RM calls just before command buffer is pushed to
    // the hardware. This allows driver to insert last few words to the command
    // buffer. This can be used to put hardware to a certain state after every
    // operation, for example.
    //
    // Upon setting the callback function, the API user has to also set
    // PreFlushWords (32b words) and allocate pPreFlushData for the last words.
    //
    // Callback supports only stream commands NVRM_STREAM_PUSH_U,
    // NVRM_STREAM_PUSH_I and NVRM_STREAM_PUSH_F. User should follow the
    // following convention pushing data to the command buffer. Calling BEGIN
    // or END is forbidden and the behavior after doing so is undefined.
    //
    // void MyPreFlushCallback(struct NvRmStreamRec *pStream)
    // {
    //     NVRM_STREAM_PUSH_U(pStream->pPreFlushData,
    //         NVRM_CH_OPCODE_INCR(BLIT_SRC_OFFSET, 6));
    //
    //     NVRM_STREAM_PUSH_U(pStream->pPreFlushData,
    //         SrcOffset);
    // }

    void (*pPreFlushCallback)(struct NvRmStreamRec *pStream);

    // Data buffer for pre-flush callback, allocated by API user
    NvData32 *pPreFlushData;

    // Number of words reserved at the end of command buffer for the callback
    // function to fill. Set by API user. pPreFlushData should be large enough
    // to contain all these words.
    NvU32 PreFlushWords;

    // Storage for NvRmStreamFlags.
    NvS32 Flags;

    /*** RM PRIVATE DATA STARTS HERE -- DO NOT ACCESS THE FOLLOWING FIELDS ***/

    // Opaque pointer to RM private data associated with this stream.  Anything
    // that RM does not need high-performance access to should be hidden behind
    // here, so that we don't have to recompile the world when it changes.
    NvRmStreamPrivate *pRmPriv;

    /*** Fields use by csim code path starts here */

    // This points to the base of the command buffer chunk that we are
    // currently filling in.
    NvData32 *pBase;

    // This points to the current location we are writing to.  This should
    // always be >= pBase and <= pFence.
    NvData32 *pCurrent;

    /* Pointer to stream memory */
    NvRmMemHandle hMem;
    void *pMem;

    // This points to the end of the region of the command buffer that we are
    // currently allowed to write to.  This could be the end of the memory
    // allocation, the point the HW is currently fetching from, or just an
    // artificial "auto-flush" point where we want to stop and kick off the
    // work we've queued up before continuing on.
    NvData32 *pFence;

    // Memory pin/unpin, command buffer relocation, and gather tables.
    NvRmCmdBufRelocation *pCurrentReloc;
    NvRmCmdBufGather *pCurrentGather;
    NvRmCmdBufRelocation RelocTable[NVRM_STREAM_RELOCATION_TABLE_SIZE];
    NvRmCmdBufGather GatherTable[NVRM_STREAM_GATHER_TABLE_SIZE];

    // Track which syncpts (and their threshold values) that have been waited
    // for in the command stream. This is used to remove a wait that already
    // has been satisfied. This is necessary as with a 16bit HW compare it can
    // appear pending in the future (but from a 32bit compare, it's already
    // completed and wrapped).
    NvRmCmdBufWait *pCurrentWait;
    NvRmCmdBufWait WaitTable[NVRM_STREAM_WAIT_TABLE_SIZE];
    NvU32 SyncPointsWaited;

    // We keep track of how much command buffer space the user has asked for,
    // so that we can verify that they don't write too much data.  This also
    // doubles as a way for us to keep track of whether we are currently inside
    // a Get/Save pair.  (Get of zero is illegal, so if this is nonzero we know
    // we are inside a Get/Save pair.)
    NvU32 WordsRequested;

    // Same idea, but for the gather and reloc tables.  If the user writes past
    // this point, there is a bug in their code.
    NvRmCmdBufRelocation *pRelocFence;
    NvRmCmdBufGather *pGatherFence;
    NvRmCmdBufWait *pWaitFence;
    NvU32 CtxChanged;
} NvRmStream;

/**
 * An exposed, non-opaque structure describing parameters that can be overridden
 * when initializing an NvRmStream during a call to NvRmStreamInitParamsEx
 */
typedef struct NvRmStreamInitParamsRec
{
    NvU32 cmdBufSize;   // size of the command buffer in bytes. Must be a multiple of 8KB.
} NvRmStreamInitParams;

/**
 * The minimum granularity for cmdBufSize in NvRmStreamInitParams.
 */
#define NVRM_CMDBUF_GRANULARITY (8*1024)

/**
 * The minimum size for cmdBufSize.
 */
#define NVRM_CMDBUF_SIZE_MIN (24*1024)


/**
 * The various channel opcodes.  Note that we do not provide the Gather or
 * Restart opcodes -- drivers are not allowed to use these opcodes.
 */
#define NVRM_CH_OPCODE_ACQUIRE_MUTEX(MutexId) \
    /* mutex ops are extended opcodes: ex-op, op, mutex id \
     * aquire op is 0. \
     */ \
    ((14UL << 28) | (MutexId) )
#define NVRM_CH_OPCODE_RELEASE_MUTEX(MutexId) \
    /* ex-op, op, mutex id */ \
    ((14UL << 28) | (1 << 24) | (MutexId) )
#define NVRM_CH_OPCODE_SET_CLASS(ClassId, Offset, Mask) \
    /* op, offset, classid, mask \
     * setclass opcode is 0 \
     */ \
    ((0UL << 28) | ((Offset) << 16) | ((ClassId) << 6) | (Mask))
#define NVRM_CH_OPCODE_INCR(Addr, Count) \
    /* op, addr, count */ \
    ((1UL << 28) | ((Addr) << 16) | (Count) )
#define NVRM_CH_OPCODE_NONINCR(Addr, Count) \
    /* op, addr, count */ \
    ((2UL << 28) | ((Addr) << 16) | (Count))
#define NVRM_CH_OPCODE_IMM(Addr, Value) \
    /* op, addr, count */ \
    ((4UL << 28) | ((Addr) << 16) | (Value))
#define NVRM_CH_OPCODE_MASK(Addr, Mask) \
    /* op, addr, count */ \
    ((3UL << 28) | ((Addr) << 16) | (Mask))

/**
 * The maximum number of words you can ask a stream for in one shot.
 */
enum { NVRM_STREAM_BEGIN_MAX_WORDS = 2048 };

#define NVRM_STREAM_BEGIN(pStream, pCurrentArg, Words) \
    do { pCurrentArg = NvRmStreamBegin(pStream, Words, 0, 0, 0); } while (0)

#define NVRM_STREAM_BEGIN_WAIT(pStream, pCurrentArg, Words, Waits) \
    do { pCurrentArg = NvRmStreamBegin(pStream, Words, Waits, 0, 0); } while (0)

#define NVRM_STREAM_BEGIN_RELOC_GATHER(pStream, pCurrentArg, Words, Relocs, Gathers) \
    do { \
        pCurrentArg = NvRmStreamBegin(pStream, Words, 0, Relocs, Gathers); \
    } while (0)

#define NVRM_STREAM_BEGIN_RELOC_GATHER_WAIT(pStream, pCurrentArg, Words, Relocs, Gathers, Waits) \
    do { \
        pCurrentArg = NvRmStreamBegin(pStream, Words, Waits, Relocs, Gathers); \
    } while (0)

#define NVRM_STREAM_END(pStream, pCurrentArg) \
    do { NvRmStreamEnd(pStream, pCurrentArg); } while (0)

#define NVRM_STREAM_PUSH_U(pStream, u1) \
    do { \
        NvRmStream *s = (NvRmStream *)(pStream); \
        (s->pCurrent++)->u = (u1); \
        } while (0)

#define NVRM_STREAM_PUSH_I(pStream, i1) \
    do { \
        NvRmStream *s = (NvRmStream *)(pStream); \
        (s->pCurrent++)->i = (i1); \
        } while (0)

#define NVRM_STREAM_PUSH_F(pStream, f1) \
    do { \
        NvRmStream *s = (NvRmStream *)(pStream); \
        (s->pCurrent++)->f = (f1); \
        } while (0)

// XXX Does writing 0xDEADBEEF to the skipped location help or hurt
// performance?
// (Does the answer to this question vary from platform to platform and from
// command buffer memory type to memory type?)  If it hurts, perhaps we should
// only write this in debug builds, and just skip the slot in release builds?
#define NVRM_STREAM_PUSH_RELOC_SHIFT(pStream, pCurrentArg, hMemArg, OffsetArg, ShiftArg) \
    do { \
        pCurrentArg = NvRmStreamPushReloc((pStream), (pCurrentArg), \
            (hMemArg), (OffsetArg), (ShiftArg)); \
    } while (0)

#define NVRM_STREAM_PUSH_RELOC(pStream, pCurrentArg, hMemArg, OffsetArg) \
    NVRM_STREAM_PUSH_RELOC_SHIFT(pStream, pCurrentArg, hMemArg, OffsetArg, 0)

#define NVRM_STREAM_PUSH_GATHER(pStream, pCurrentArg, hMemArg, OffsetArg, WordsArg) \
    do { \
        pCurrentArg = NvRmStreamPushGather((pStream), (pCurrentArg), \
            (hMemArg), (OffsetArg), (WordsArg)); \
    } while (0)

#define NVRM_STREAM_PUSH_FD_RELOC_SHIFT(pStream, pCurrentArg, fd, OffsetArg, ShiftArg) \
    do { \
        pCurrentArg = NvRmStreamPushFdReloc((pStream), (pCurrentArg), \
            (fd), (OffsetArg), (ShiftArg)); \
    } while (0)

#define NVRM_STREAM_PUSH_FD_RELOC(pStream, pCurrentArg, fd, OffsetArg) \
    NVRM_STREAM_PUSH_FD_RELOC_SHIFT(pStream, pCurrentArg, fd, OffsetArg, 0)

#define NVRM_STREAM_PUSH_FD_GATHER(pStream, pCurrentArg, fd, OffsetArg, WordsArg) \
    do { \
        pCurrentArg = NvRmStreamPushFdGather((pStream), (pCurrentArg), \
            (fd), (OffsetArg), (WordsArg)); \
    } while (0)

#define NVRM_STREAM_PUSH_GATHER_NONINCR(pStream, pCurrentArg, RegArg, hMemArg, OffsetArg, WordsArg) \
    do { \
        pCurrentArg = NvRmStreamPushGatherNonIncr((pStream), (pCurrentArg), \
            (RegArg), (hMemArg), (OffsetArg), (WordsArg)); \
    } while (0)

#define NVRM_STREAM_PUSH_GATHER_INCR(pStream, pCurrentArg, RegArg, hMemArg, OffsetArg, WordsArg) \
    do { \
        pCurrentArg = NvRmStreamPushGatherIncr((pStream), (pCurrentArg), \
            (RegArg), (hMemArg), (OffsetArg), (WordsArg)); \
    } while (0)

#define NVRM_STREAM_PUSH_SETCLASS(pStream, pCurrentArg, ModuleArg, ClassArg) \
    do { \
        pCurrentArg = NvRmStreamPushSetClass((pStream), (pCurrentArg), \
            (ModuleArg), (ClassArg)); \
    } while (0)

#define NVRM_STREAM_PUSH_INCR(pStream, pCurrentArg, SyncPointArg, RegArg, CondArg, StreamArg) \
    do { \
        pCurrentArg = NvRmStreamPushIncr((pStream), (pCurrentArg), \
            (SyncPointArg), (RegArg), (CondArg), (StreamArg)); \
    } while (0)

#define NVRM_STREAM_PUSH_WAIT(pStream, pCurrentArg, FenceArg) \
    do { \
        pCurrentArg = NvRmStreamPushWait((pStream), \
            (pCurrentArg), (FenceArg)); \
    } while (0)

#define NVRM_STREAM_PUSH_WAIT_LAST(pStream, pCurrentArg, \
        SyncPointArg, WaitBaseArg, RegArg, CondArg) \
    do { \
        pCurrentArg = NvRmStreamPushWaitLast((pStream), (pCurrentArg), \
            (SyncPointArg), (WaitBaseArg), (RegArg), (CondArg)); \
    } while (0)

/**
 * Note: This macro doesn't actually push/add the wait method to the command
 * stream. The driver is still responsible for issuing the methods since it
 * manages its cmdbuf depth and current setclass. This macro simply adds
 * tracking, so it can potentially be patched later.
 *
 * It's expected the macro is called immediately after the driver has PUSHed
 * the syncpt id and threshold value into the stream (uses the current ptr
 * as the location to patch).
 *
 * This macro should be used with either NVRM_STREAM_BEGIN_WAIT or
 * NVRM_STREAM_BEGIN_RELOC_GATHER_WAIT which checks if there's enough
 * NvRmCmdBufWait management structs available.
 */
#define NVRM_STREAM_PUSH_WAIT_CHECK(pStream, pCurrentArg, SyncPointArg, ValueArg) \
    do { \
        NvRmFence _fence; \
        _fence.SyncPointID = (SyncPointArg); \
        _fence.Value = (ValueArg); \
        pCurrentArg = NvRmStreamPushWaitCheck((pStream), (pCurrentArg), \
            _fence); \
    } while (0)

/**
 * Invalid ids for sync points.
 */
#define NVRM_INVALID_SYNCPOINT_ID ((NvU32)-1)
#define NVRM_INVALID_WAITBASE_ID ((NvU32)-1)
#define NVRM_MAX_SYNCPOINTS_PER_SUBMIT (32)
#define NVRM_MAX_SYNCPOINTS (128)

/**
 * Opens a handle to a channel.  Most likely, you will be sharing this channel
 * with many other clients.
 *
 * To decide what channel you get, RM needs to know what modules you plan to
 * use.  It is very important to fill out this information correctly, because
 * only a subset of [channel, module] pairs may have sync points associated
 * with them.  If you fail to declare that you are going to use a module, your
 * NvRmStreamFlush() calls may fail.
 * XXX In the future, this should be updated to say "your NvRmStreamInit call",
 * once NvRmStreamInit takes an engine argument.
 *
 * @param hDevice Handle to device the channel is for.
 * @param phChannel Pointer to to be filled in with the channel handle.
 * @param NumModules Number of modules in the pModuleIDs array.
 *     Currently zero is allowed, but please fix your code to not rely on this.
 * @param pModuleIDs List of modules you are planning on using.
 *     Currently NULL is allowed, but please fix your code to not rely on this.
 */

 NvError NvRmChannelOpen(
    NvRmDeviceHandle hDevice,
    NvRmChannelHandle * phChannel,
    NvU32 NumModules,
    const NvRmModuleID * pModuleIDs );

/**
 * Closes a channel handle from NvRmChannelOpen() or NvRmChannelAlloc().  This
 * API will never fail.
 *
 * @param hChannel The channel handle to close.
 *     If hChannel is NULL, this API will do nothing.
 */

 void NvRmChannelClose(
    NvRmChannelHandle hChannel );

/**
 * Allocates a hardware context for a particular engine.  Only some engines
 * need a context.
 *
 * @param hDevice Handle to device the context is for.
 * @param Module Module that we need the context for.
 * @param phContext Pointer to to be filled in with the context handle.
 */

 NvError NvRmContextAlloc(
    NvRmDeviceHandle hDevice,
    NvRmModuleID Module,
    NvRmContextHandle * phContext );

/**
 * Frees a previously allocated hardware context.  This API will never fail.
 *
 * @param hContext The context handle to free.
 *     If hContext is NULL, this API will do nothing.
 */

 void NvRmContextFree(
    NvRmContextHandle hContext );

/**
 * Allocates a sync point hardware resource.
 *
 * It is recommended that you not use this API.  You should rely instead on the
 * RM's statically preallocated sync points, which cannot be exhausted.
 */

 NvError NvRmChannelSyncPointAlloc(
    NvRmDeviceHandle hDevice,
    NvU32 * pSyncPointID );

/**
 * Frees a sync point hardware resource allocated from
 * NvRmChannelSyncPointAlloc().
 */

 void NvRmChannelSyncPointFree(
    NvRmDeviceHandle hDevice,
    NvU32 SyncPointID );

/**
 * Obtains the index of the statically preallocated sync point register for a
 * particular <channel, module> pair.
 *
 * It is not allowed to set priority on a channel or stream which has (or will
 * have) retrieved syncpoint IDs via this call.
 *
 * @param hChannel Channel handle obtained from NvRmChannelOpen().
 * @param Module Module that we need the preallocated sync point for.
 * @param SyncPointIndex Index for the module.
 * @param pSyncPointID The sync point, if there's a reserved sync point for the
 *        given module, otherwise an error is returned.
 */

 NvError NvRmChannelGetModuleSyncPoint(
    NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 SyncPointIndex,
    NvU32 * pSyncPointID );

/**
 * Reads the current value of a particular sync point register.
 *
 * Do not use this API to poll for completion.  Use
 * NvRmChannelSyncPointWait() instead.
 *
 * @param hDevice Handle to RM device.
 * @param SyncPointID The ID of the sync point register to read.
 */

 NvU32 NvRmChannelSyncPointRead(
    NvRmDeviceHandle hDevice,
    NvU32 SyncPointID );

/**
 * Reads the maximum future value of a sync point register.
 *
 * @param hDevice Handle to RM device.
 * @param SyncPointID The ID of the sync point register to read.
 * @param Value The max value will be returned here.
 */

 NvError NvRmChannelSyncPointReadMax(
    NvRmDeviceHandle hDevice,
    NvU32 SyncPointID,
    NvU32 * Value );

/**
 * Gets the number of syncpoints in the current hardware
 *
 * @param Value The number of syncpoints
 */

 NvError NvRmChannelNumSyncPoints(
    NvU32 * Value );

/**
 * Gets the number of mutexes in the current hardware
 *
 * @param Value The number of mutexes
 */

 NvError NvRmChannelNumMutexes(
    NvU32 * Value );

/**
 * Gets the number of wait bases in the current hardware
 *
 * @param Value The number of wait bases
 */

 NvError NvRmChannelNumWaitBases(
    NvU32 * Value );

/**
 * Increments (using the CPU) a particular sync point register.
 *
 * This API will never fail.
 *
 * @param hDevice Handle to RM device.
 * @param SyncPointID The ID of the sync point register to increment.
 */

 void NvRmChannelSyncPointIncr(
    NvRmDeviceHandle hDevice,
    NvU32 SyncPointID );

/**
 * Triggers (using the CPU) a particular fence.
 *
 * @param hDevice Handle to RM device.
 * @param Fence The fence to trigger
 */

 NvError NvRmFenceTrigger(
    NvRmDeviceHandle hDevice,
    const NvRmFence * Fence );

/**
 * Requests for a semaphore to be signaled when a particular sync point
 * register reaches a given threshold.  The semaphore will be signaled when
 * the sync point's value is >= the threshold, using a wrap-around version of
 * the >= comparison.  The caller can then wait on the semaphore to determine
 * completion.
 *
 * This API is non-blocking.  It returns immediately after scheduling the
 * semaphore to be signaled.
 *
 * In some cases, the RM might notice that the semaphore is already past the
 * threshold.  This function is allowed to shortcircuit signaling the semaphore
 * in that case.  This avoids a bit of unnecessary overhead if the caller was
 * just about to wait on the semaphore anyway (which is the typical case).
 * Therefore, typical usage will look like:
 *
 *     if (!NvRmChannelSyncPointSignalSemaphore(...))
 *         NvOsSemaphoreWait(...);
 *
 * @param hDevice An RM device handle.
 * @param SyncPointID Which sync point register to wait on.
 * @param Threshold The value to wait for the sync point register to obtain.
 * @param hSema The OS semaphore to be signaled when the threshold is attained.
 *
 * @returns NV_TRUE if the sync point was already past the threshold.  The
 *     semaphore will not be signaled -- do not wait on it.
 * @returns NV_FALSE if the sync point was not yet past the threshold.  The
 *     semaphore will be signaled once it is.
 *
 * THIS API IS DEPRICATED. USE NvRmChannelSyncPointWait INSTEAD.
 */

 NvBool NvRmChannelSyncPointSignalSemaphore(
    NvRmDeviceHandle hDevice,
    NvU32 SyncPointID,
    NvU32 Threshold,
    NvOsSemaphoreHandle hSema );

/**
 * Requests for a semaphore to be signaled when a particular fence is
 * triggered. The caller can then wait on the semaphore to determine
 * completion.
 *
 * This API is non-blocking.  It returns immediately after scheduling the
 * semaphore to be signaled.
 *
 * In some cases, the RM might notice that the semaphore is already past the
 * threshold.  This function is allowed to shortcircuit signaling the semaphore
 * in that case.  This avoids a bit of unnecessary overhead if the caller was
 * just about to wait on the semaphore anyway (which is the typical case).
 * Therefore, typical usage will look like:
 *
 *     if (!NvRmChannelSyncPointSignalSemaphore(...))
 *         NvOsSemaphoreWait(...);
 *
 * @param hDevice An RM device handle.
 * @param Fence Fence to wait on
 * @param hSema The OS semaphore to be signaled when the threshold is attained.
 *
 * @returns NV_TRUE if the sync point was already past the threshold.  The
 *     semaphore will not be signaled -- do not wait on it.
 * @returns NV_FALSE if the sync point was not yet past the threshold.  The
 *     semaphore will be signaled once it is.
 */

 NvBool NvRmFenceSignalSemaphore(
    NvRmDeviceHandle hDevice,
    const NvRmFence * Fence,
    NvOsSemaphoreHandle hSema );

/**
 * Waits for a sync point to reach the given threshold. Will block on the
 * given semaphore if the sync point has not reached the threashold. This
 * handles clocking of the hardware and will short circuit the semaphore
 * wait/signal if possible.
 *
 * @param hDevice An RM device handle.
 * @param SyncPointID Which sync point register to wait on.
 * @param Threshold The value to wait for the sync point register to obtain.
 * @param hSema The OS semaphore to be signaled when the threshold is attained.
 */

 void NvRmChannelSyncPointWait(
    NvRmDeviceHandle hDevice,
    NvU32 SyncPointID,
    NvU32 Threshold,
    NvOsSemaphoreHandle hSema );

/**
 * Waits for a fence to be triggered.
 *
 * @param hDevice An RM device handle.
 * @param Fence Fence to wait on
 * @param hSema The OS semaphore to be signaled when the threshold is attained.
 */

 NvError NvRmFenceWait(
    NvRmDeviceHandle hDevice,
    const NvRmFence * Fence,
    NvU32 Timeout );

/**
 * Creates an Android fence file from fences.
 *
 * @param Name Name of the fence file (optional, NULL is fine).
 * @param Fences Fences to put into the file.
 * @param NumFences The number of fences in the Fences array.
 * @param Fd The file descriptor of the created fence file.
 */

 NvError NvRmFencePutToFile(
    const char * Name,
    const NvRmFence * Fences,
    NvU32 NumFences,
    NvS32 * Fd );

/**
 * Extracts fences from an Android fence file.
 *
 * @param Fd The file descriptor of the fence file.
 * @param Fences Pointer to a fence buffer that stores the fences.
 *      Pass NULL to only extract the number of fences in the file.
 * @param NumFences The number of fences that can be written into the
 *      Fences array. Is updated with the number of actually written.
 */

 NvError NvRmFenceGetFromFile(
    NvS32 Fd,
    NvRmFence * Fences,
    NvU32 * NumFences );

/**
 * Same as NvRmChannelSyncPointWait, but provides a semaphore timeout value.
 * NvError_Timeout will be returned if the sync point isn't signaled by the
 * given timeout.
 *
 * @param hDevice An RM device handle.
 * @param SyncPointID Which sync point register to wait on.
 * @param Threshold The value to wait for the sync point register to obtain.
 * @param hSema The OS semaphore to be signaled when the threshold is attained.
 * @param Timeout The timeout value, in milliseconds, for the semaphore. Use
 *      NV_WAIT_INFINITE to wait infinitely.
 */

 NvError NvRmChannelSyncPointWaitTimeout(
    NvRmDeviceHandle hDevice,
    NvU32 SyncPointID,
    NvU32 Threshold,
    NvOsSemaphoreHandle hSema,
    NvU32 Timeout );

/**
 * Same as NvRmChannelSyncPointWaitTimeout, but provides a pointer to memory
 * allocated to hold the actual latest sync point.
 * NvError_Timeout will be returned if the sync point isn't signaled by the
 * given timeout.
 *
 * @param hDevice An RM device handle.
 * @param SyncPointID Which sync point register to wait on.
 * @param Threshold The value to wait for the sync point register to obtain.
 * @param hSema The OS semaphore to be signaled when the threshold is attained.
 * @param Timeout The timeout value, in milliseconds, for the semaphore. Use
 *      NV_WAIT_INFINITE to wait infinitely.
 */

 NvError NvRmChannelSyncPointWaitexTimeout(
    NvRmDeviceHandle hDevice,
    NvU32 SyncPointID,
    NvU32 Threshold,
    NvOsSemaphoreHandle hSema,
    NvU32 Timeout,
    NvU32 * Actual );

/**
 * Same as NvRmChannelSyncPointWaitTimeoutex, but provides information about
 * the exact time of work completion.
 * NvError_Timeout will be returned if the sync point isn't signaled by the
 * given timeout.
 *
 * @param hDevice An RM device handle.
 * @param SyncPointID Which sync point register to wait on.
 * @param Threshold The value to wait for the sync point register to obtain.
 * @param hSema The OS semaphore to be signaled when the threshold is attained.
 * @param Timeout The timeout value, in milliseconds, for the semaphore. Use
 *      NV_WAIT_INFINITE to wait infinitely.
 */

NvError NvRmChannelSyncPointWaitmexTimeout(
    NvRmDeviceHandle hDevice,
    NvU32 SyncPointID,
    NvU32 Threshold,
    NvOsSemaphoreHandle hSema,
    NvU32 Timeout,
    NvU32 *Actual,
    NvRmTimeval *tv);

/**
 * Obtains the index of the statically preallocated channel mutex (for use with
 * NVRM_CH_OPCODE_ACQUIRE_MUTEX) for a particular module.  Some modules may
 * have no mutex at all; some modules may have one mutex; some modules may have
 * more than one.  These mutexes may be assigned different indices from chip to
 * chip, so drivers must ask the RM which index to use.
 *
 * An example of a module that doesn't need any mutex is 3D, which is only used
 * from a single channel.
 *
 * An example of a module that needs more than one is 2D, which would have 3 on
 * AP15, one for each of its major modes of operation.
 *
 * The caller of this function is expected to know enough about the module to
 * know which particular mutex it wants, if there is more than one.
 *
 * @param Module Module that we need the preallocated channel mutex for.
 * @param Index Zero-based index of the particular mutex for that module that
 *     we are asking for.  For many modules this can only be 0, but for modules
 *     with multiple mutexes, it might be nonzero.
 *
 * @returns The index of the mutex.
 */

 NvU32 NvRmChannelGetModuleMutex(
    NvRmModuleID Module,
    NvU32 Index );

/**
 * Locks the given hardware mutex from the CPU.
 *
 * @param Index The mutex index gotten from NvRmChannelGetModuleMutex
 */

 void NvRmChannelModuleMutexLock(
    NvRmDeviceHandle hDevice,
    NvU32 Index );

/**
 * Unlocks the given hardware mutex from the CPU.
 *
 * @param Index the hardware mutex index
 */

 void NvRmChannelModuleMutexUnlock(
    NvRmDeviceHandle hDevice,
    NvU32 Index );

/**
 * Gets a "wait base register" hardware resource.
 *
 * @param hChannel Handle to the channel that this wait base will be used with
 * @param Module Module Id for the preallocated wait base register
 * @param Index Zero-based index for the module's registers (if more than one)
 *
 * @returns The index of the wwait base register.
 */

 NvU32 NvRmChannelGetModuleWaitBase(
    NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 Index );

/**
 * Returns whether channel has timed out
 *
 * @param hChannel Handle to the channel that this wait base will be used with
 * @param Module Module Id for the preallocated wait base register
 * @param Index Zero-based index for the module's registers (if more than one)
 *
 * @returns  0 if there is no timeout
 * @returns  1 if there is a timeout
 * @returns -1 if error
 */

 NvS32 NvRmChannelGetModuleTimedout(
    NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 Index );

/**
 * Read a register from 3D module
 *
 * @param hChannel Handle to the channel that identifies the context to read from
 * @param Offset Register to read (word address)
 * @param Value Value will be returned here
 *
 * @returns Error code
 */

 NvError NvRmChannelRead3DRegister(
    NvRmChannelHandle hChannel,
    NvU32 Offset,
    NvU32 * Value );

/**
 * Gets clock rate for the module.
 *
 * @param hChannel Handle to the channel
 * @param Module Module Id for which get the clock rate
 * @param Rate Pointer to be filled with the rate read from kernel
 */

 NvU32 NvRmChannelGetModuleClockRate(
    NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 * Rate );

/**
 * Sets module clock rate.
 *
 * @param hChannel Handle to the channel
 * @param Module Module Id for which set the clock rate
 * @param Rate Clock rate to set
 */

 NvU32 NvRmChannelSetModuleClockRate(
    NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 Rate );

/**
 * Sets module bw rate.
 *
 * @param hChannel Handle to the channel
 * @param Module Module Id for which set the clock rate
 * @param Bandwidth to set, unit is KBps
 */

 NvU32 NvRmChannelSetModuleBandwidth(
    NvRmChannelHandle hChannel,
    NvRmModuleID Module,
    NvU32 BW );

/**
 * Sets a submit buffer timeout for this stream/context.
 *
 * @param hChannel Handle to the channel for NvRmStream submits
 * @param SubmitTimeout Time, in milliseconds, a buffer is allowed to run
 * @param DisableDebugDump This flag allows disabling the timeout debug dump
 */

 NvError NvRmChannelSetSubmitTimeoutEx(
    NvRmChannelHandle hChannel,
    NvU32 SubmitTimeout,
    NvBool DisableDebugDump );

/**
 * Sets a submit buffer timeout for this stream/context.
 *
 * @param hChannel Handle to the channel for NvRmStream submits
 * @param SubmitTimeout Time, in milliseconds, a buffer is allowed to run
 */

 NvError NvRmChannelSetSubmitTimeout(
    NvRmChannelHandle hChannel,
    NvU32 SubmitTimeout );

/**
 * Sets context switch buffers
 *
 * @param pStream Stream
 * @param hSave Save buffer
 * @param hRestore Restore buffer
 * @param Reloc offset
 * @param SyncptId Sync point id
 * @param SaveIncrs Save increments
 * @param RestoreIncrs Restore increments
 */
NvError NvRmStreamSetContextSwitch(
    NvRmStream *pStream,
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

/**
 * Read register values from the given module's channel
 * using channel handle
 * Can only be used for modules behind host
 *
 * @param device The RM instance
 * @param ch The channel handle
 * @param id The module id to read from
 * @param num Number of registers to read
 * @param offsets Offset list of the registers to read
 * @param values Storage for the register values
 *
 * The offset is a byte address, not a word address.  Some
 * of the hardware specs have word addresses, so the offsets
 * need to be mutlipled by 4.
 */

 NvError NvRmChannelRegRd(
    NvRmDeviceHandle device,
    NvRmChannelHandle ch,
    NvRmModuleID id,
    NvU32 num,
    const NvU32 * offsets,
    NvU32 * values );

/**
 * Write register values to the module's channel using channel handle
 * Can only be used for modules behind host
 *
 * @param device The RM instance
 * @param ch The channel handle
 * @param id The module id to write to
 * @param num Number of registers to write
 * @param offsets Offset list of the registers to write
 * @param values Values to write into the registers
 *
 * See the comment above regarding byte addressing.
 */

 NvError NvRmChannelRegWr(
    NvRmDeviceHandle device,
    NvRmChannelHandle ch,
    NvRmModuleID id,
    NvU32 num,
    const NvU32 * offset,
    const NvU32 * values );

/**
 * Read a block of consecutive register values to the module's
 * channel using channel handle
 * Can only be used for modules behind host
 *
 * @param device The RM instance
 * @param ch The channel handle
 * @param id The module id to write to
 * @param num Number of registers to write
 * @param offset Offset the first register to write
 * @param values Values read into the registers
 *
 * See the comment above regarding byte addressing.
 */

 NvError NvRmChannelBlockRegRd(
    NvRmDeviceHandle device,
    NvRmChannelHandle ch,
    NvRmModuleID id,
    NvU32 num,
    NvU32 offset,
    NvU32 * values );

/**
 * Write a block of consecutive register values from the given module's
 * channel using using channel handle
 * Can only be used for modules behind host
 *
 * @param device The RM instance
 * @param ch The channel handle
 * @param id The module id to read from
 * @param num Number of registers to read
 * @param offset Offset of the first register to read
 * @param values Storage for the register values
 *
 * The offset is a byte address, not a word address.  Some
 * of the hardware specs have word addresses, so the offsets
 * need to be mutlipled by 4.
 */

 NvError NvRmChannelBlockRegWr(
    NvRmDeviceHandle device,
    NvRmChannelHandle ch,
    NvRmModuleID id,
    NvU32 num,
    NvU32 offset,
    NvU32 * values );

/**
 * Sets the specified initialization parameters to default values.
 *
 * @param pInitParams The parameters that will be set to default values.
 */
void NvRmStreamInitParamsSetDefaults( NvRmStreamInitParams* pInitParams);

// use this define to decouple submission into multiple repos.
#define NVCONFIG_NVRMSTREAMINITEX_SUPPORTED

/**
 * Initializes a stream struct.  Must be called before attempting to use the
 * stream with any other API.
 *
 * @param hDevice Handle to the resource manager
 * @param hChannel Handle to the channel the stream will be used with.
 * @param pInitParams Parameters to pass for stream initialization.
 * @param pStream The stream to initialize.
 */
NvError NvRmStreamInitEx(
    NvRmDeviceHandle hDevice,
    NvRmChannelHandle hChannel,
    const NvRmStreamInitParams* pInitParams,
    NvRmStream *pStream);

/**
 * Initializes a stream struct.  Must be called before attempting to use the
 * stream with any other API.
 * This is identical to a call to NvRmStreamInitEx, except that the function
 * will use default NvRmStreamInitParams instead of being specified by the
 * client/caller.
 *
 * @param hDevice Handle to the resource manager
 * @param hChannel Handle to the channel the stream will be used with.
 * @param pStream The stream to initialize.
 */
NvError NvRmStreamInit(
    NvRmDeviceHandle hDevice,
    NvRmChannelHandle hChannel,
    NvRmStream *pStream);

/**
 * Begin a new stream. If pStream already contains a stream, it is copied to
 * command buffer.
 * @param pStream Pointer to stream.
 * @param Words Number of words to push
 * @param Waits Number of wait checks to push
 * @param Relocs Number of relocations to push
 * @param Gathers Number of gathers
 */
NvData32 *NvRmStreamBegin(
    NvRmStream *pStream,
    NvU32 Words,
    NvU32 Waits,
    NvU32 Relocs,
    NvU32 Gathers);

/**
 * End a stream.
 * @param pStream Pointer to stream.
 * @param Words Number of words to push
 * @param Waits Number of wait checks to push
 * @param Relocs Number of relocations to push
 * @param Gathers Number of gathers
 */
void NvRmStreamEnd(
    NvRmStream *pStream,
    NvData32 *pCurrent);

/**
 * Push a relocation to stream.
 * @param pStream The stream
 * @param pCurrent The cursor
 * @param hMem The target memory handle
 * @param Offset The target memory offset
 * @param RelocShift How much to shift the address
 * @return New cursor
 */
NvData32 *NvRmStreamPushReloc(
    NvRmStream *pStream,
    NvData32 *pCurrent,
    NvRmMemHandle hMem,
    NvU32 Offset,
    NvU32 RelocShift);

/**
 * Push a relocation to stream.
 * @param pStream The stream
 * @param pCurrent The cursor
 * @param fd The target dmabuf fd
 * @param Offset The target memory offset
 * @param RelocShift How much to shift the address
 * @return New cursor
 */
NvData32 *NvRmStreamPushFdReloc(
    NvRmStream *pStream,
    NvData32 *pCurrent,
    int fd,
    NvU32 Offset,
    NvU32 RelocShift);

/**
 * Push a gather to stream.
 * @param pStream The stream
 * @param pCurrent The cursor
 * @param hMem The memory handle for gather
 * @param Offset The target memory offset
 * @param Words Number of words in gather.
 * @return New cursor
 */
NvData32 *NvRmStreamPushGather(
    NvRmStream *pStream,
    NvData32 *pCurrent,
    NvRmMemHandle hMem,
    NvU32 Offset,
    NvU32 Words);

/**
 * Push a gather to stream.
 * @param pStream The stream
 * @param pCurrent The cursor
 * @param fd The dmabuf fd for gather
 * @param Offset The target memory offset
 * @param Words Number of words in gather.
 * @return New cursor
 */
NvData32 *NvRmStreamPushFdGather(
    NvRmStream *pStream,
    NvData32 *pCurrent,
    int fd,
    NvU32 Offset,
    NvU32 Words);

/**
 * Push a gather with insert nonincr opcode to stream.
 * @param pStream The stream
 * @param pCurrent The cursor
 * @param Reg The register to send the write to
 * @param hMem The memory handle for gather
 * @param Offset The target memory offset
 * @param Words Number of words in gather.
 * @return New cursor
 */
NvData32 *NvRmStreamPushGatherNonIncr(
    NvRmStream *pStream,
    NvData32 *pCurrent,
    NvU32 Reg,
    NvRmMemHandle hMem,
    NvU32 Offset,
    NvU32 Words);

/**
 * Push a gather with insert incr opcode to stream.
 * @param pStream The stream
 * @param pCurrent The cursor
 * @param Reg The first register to send the write to
 * @param hMem The memory handle for gather
 * @param Offset The target memory offset
 * @param Words Number of words in gather.
 * @return New cursor
 */
NvData32 *NvRmStreamPushGatherIncr(
    NvRmStream *pStream,
    NvData32 *pCurrent,
    NvU32 Reg,
    NvRmMemHandle hMem,
    NvU32 Offset,
    NvU32 Words);

/**
 * Push a setclass. Will also update LastEngineUsed and LastClassUsed.
 *
 * Uses 1 word of space in stream.
 *
 * @param pStream The stream
 * @param pCurrent The cursor
 * @param ClassID The id of the class
 * @return New cursor
 */
NvData32 *NvRmStreamPushSetClass(
    NvRmStream *pStream,
    NvData32 *pCurrent,
    NvRmModuleID ModuleID,
    NvU32 ClassID);

/**
 * Push a sync point increment to stream.
 *
 * Uses 2 words of space in stream.
 *
 * @param pStream The stream
 * @param pCurrent The cursor
 * @param SyncPointID The id of sync point
 * @param The register number to use. If unsure, use 0x000
 * @param Cond The condition
 * @param True if this is a stream sync point
 * @return New cursor
 */
NvData32 *NvRmStreamPushIncr(
    NvRmStream *pStream,
    NvData32 *pCurrent,
    NvU32 SyncPointID,
    NvU32 Reg,
    NvU32 Cond,
    NvBool StreamSyncpt);

/**
 * Push a host wait to stream. If current class is not HOST1X, will
 * send a SETCLASS for HOST1X, invoke wait, and SETCLASS back to the
 * LastClassUsed.
 *
 * Uses 4 words of space in stream.
 *
 * @param pStream The stream
 * @param pCurrent The cursor
 * @param Fence The fence to wait for.
 * @return New cursor
 */
NvData32 *NvRmStreamPushWait(
    NvRmStream *pStream,
    NvData32 *pCurrent,
    NvRmFence Fence);

/**
 * Push a host wait to stream. If current class is not HOST1X, will
 * send a SETCLASS for HOST1X, invoke wait, and SETCLASS back to the
 * LastClassUsed.
 *
 * Uses 2 words of space in stream, plus 2 words for each fence
 *
 * @param pStream The stream
 * @param pCurrent The cursor
 * @param Fence The fence to wait for.
 * @return New cursor
 */
NvData32 *NvRmStreamPushWaits(
    NvRmStream *pStream,
    NvData32 *pCurrent,
    int NumFences,
    NvRmFence *Fences);

/**
 * Push a sync point increment, and a wait for increment to the stream. It
 * essentially waits for the previous operation to finish.
 * This is what gets pushed:
 *  Issue increment to engine
 *  Switch to host1x
 *  Wait for waitbase+1
 *  Increment waitbase by 1
 *
 * Uses 6 words of space in stream.
 *
 * @param pStream The stream
 * @param pCurrent The cursor
 * @param SyncPointID The sync point to increment/wait
 * @param WaitBase The wait base usable for wait
 * @param Cond The condition for increment
 * @return New cursor
 */
NvData32 *NvRmStreamPushWaitLast(
    NvRmStream *pStream,
    NvData32 *pCurrent,
    NvU32 SyncPointID,
    NvU32 WaitBase,
    NvU32 Reg,
    NvU32 Cond);

/**
 * Push a wait check to stream.
 * @param pStream The stream
 * @param pCurrent The cursor
 * @param pFence The fence to wait for
 * @return New cursor
 */
NvData32 *NvRmStreamPushWaitCheck(
    NvRmStream *pStream,
    NvData32 *pCurrent,
    NvRmFence Fence);

/**
 * Frees the resources associated with a stream struct.  Must be called once
 * for each successful NvRmStreamInit().  This API will never fail.
 *
 * @param pStream The stream to free the resources for.
 */
void NvRmStreamFree(NvRmStream *pStream);

/**
 * NvRmStreamGetError returns the value of the stream ErrorFlag.
 * When an error occurs, the ErrorFlag is set to the appropriate NvError
 * code value. No other errors are recorded until NvRmStreamGetError
 * is called, ErrorFlag is returned, and ErrorFlag is reset to NvSuccess.
 * If a call to NvRmStreamGetError returns NvSuccess, there has been no
 * detectable error since the last call to NvRmStreamGetError.
 */
NvError NvRmStreamGetError(
    NvRmStream *pStream);

/**
 * NvRmStreamSetError updates ErrorFlag if ErrorFlag is NvSuccess.
 */
void NvRmStreamSetError(NvRmStream *pStream, NvError err);

/**
 * Flushes a stream so that all commands previously inserted are guaranteed to
 * reach the hardware in finite time.
 *
 * This must not be called between a NVRM_STREAM_BEGIN/NVRM_STREAM_END pair
 * (debug builds will assert if this happens).  The NVRM_STREAM_BEGIN() macros
 * may indirectly call this function (through NvRmStreamGetSpace).
 *
 * This function sets pStream->SyncPointsUsed = 0.
 *
 * A fence may be given to allow callers to block until the flushed commands
 * have finished via NvRmChannelSyncPointWait().  A null fence parameter is
 * permitted, if you're not interested in knowing when the commands have
 * completed.
 *
 * @param pStream The stream to flush.
 * @param pFence Output parameter - fence for synchroniziation
 */
void NvRmStreamFlush(NvRmStream *pStream, NvRmFence *pFence);

/**
 * Read a register from 3D module
 *
 * @param pStream The stream that identifies the context to read from
 * @param Offset Register to read (word address)
 * @param Value Register value will be returned here
 *
 * @returns Error code
 */
NvError
NvRmStreamRead3DRegister(NvRmStream *pStream, NvU32 Offset, NvU32* Value);

/**
 * Sets a priority for the stream.
 * @deprecated, use NvRmStreamSetPriorityEx
 *
 * @param pStream Stream to set priority to
 * @param StreamPriority Priority of the stream. 100 is default.
 */
NvError NvRmStreamSetPriority(
    NvRmStream *pStream,
    NvU32 StreamPriority);

/**
 * Sets a priority for submits in the channel. Requires the caller to take
 * the returned sync point and wait base into use for all future streams.
 *
 * It is not allowed to call NvRmChannelGetModuleSyncPoint() on a channel
 * or stream which has (or will have) its priority set. Instead use the
 * syncpoint IDs returned by this call.
 *
 * @param hChannel Handle to the channel for NvRmStream submits
 * @param Priority Priority of all further submits.
 *        Use values from NvRmStreamPriority.
 * @param SyncPointIndex Index of sync point to return
 * @param WaitBaseIndex Index of wait base to return
 * @param SyncPointID The returned sync point id
 * @param WaitBase The returned wait wait base
 */
NvError NvRmStreamSetPriorityEx(
    NvRmStream *pStream,
    NvU32 Priority,
    NvU32 SyncPointIndex,
    NvU32 WaitBaseIndex,
    NvU32 *SyncPointID,
    NvU32 *WaitBase);

/**
 * Set waitbase for a syncpoint that is used in stream. The waitbase will
 * be synchronized with syncpoint id in case a timeout occurs.
 *
 * @param pStream Handle to the stream
 * @param SyncPointID Syncpoint that is used in the stream
 * @param WaitBaseID Waitbase that is synced with syncpoint
 */
void NvRmStreamSetWaitBase(NvRmStream *pStream, NvU32 SyncPointID, NvU32 WaitBaseID);


/**
 * The default priority for a stream.
 */
enum NvRmStreamPriority {
    NVRM_STREAM_PRIORITY_LOW = 50,
    NVRM_STREAM_PRIORITY_DEFAULT = 100,
    NVRM_STREAM_PRIORITY_HIGH = 150
};

/**
 * Read register values from the given module's hardware using host indirect
 * access. Can only be used for modules behind host.
 *
 * @param device The RM instance
 * @param id The module id to read from
 * @param num Number of registers to read
 * @param offsets Offset list of the registers to read
 * @param values Storage for the register values
 *
 * The offset is a byte address, not a word address.  Some
 * of the hardware specs have word addresses, so the offsets
 * need to be mutlipled by 4.
 */

 NvError NvRmHostModuleRegRd(
    NvRmDeviceHandle device,
    NvRmModuleID id,
    NvU32 num,
    const NvU32 * offsets,
    NvU32 * values );

/**
 * Write register values to the module's hardware using indirect access. Can
 * only be used for modules behind host.
 *
 * @param device The RM instance
 * @param id The module id to write to
 * @param num Number of registers to write
 * @param offsets Offset list of the registers to write
 * @param values Values to write into the registers
 *
 * See the comment above regarding byte addressing.
 */

 NvError NvRmHostModuleRegWr(
    NvRmDeviceHandle device,
    NvRmModuleID id,
    NvU32 num,
    const NvU32 * offset,
    const NvU32 * values );

/**
 * Write a block of consecutive register values to the module's
 * hardware using indirect access. Can only be used for modules behind host.
 *
 * @param device The RM instance
 * @param id The module id to write to
 * @param num Number of registers to write
 * @param offset Offset the first register to write
 * @param values Values to write into the registers
 *
 * See the comment above regarding byte addressing.
 */

 NvError NvRmHostModuleBlockRegWr(
    NvRmDeviceHandle device,
    NvRmModuleID id,
    NvU32 num,
    NvU32 offset,
    const NvU32 * values );

/**
 * Read a block of consecutive register values from the given module's
 * hardware using host indirect access. Can only be used for modules behind host.
 *
 * @param device The RM instance
 * @param id The module id to read from
 * @param num Number of registers to read
 * @param offset Offset of the first register to read
 * @param values Storage for the register values
 *
 * The offset is a byte address, not a word address.  Some
 * of the hardware specs have word addresses, so the offsets
 * need to be mutlipled by 4.
 */

 NvError NvRmHostModuleBlockRegRd(
    NvRmDeviceHandle device,
    NvRmModuleID id,
    NvU32 num,
    NvU32 offset,
    NvU32 * values );

/** @} */

#if defined(__cplusplus)
}
#endif

#endif
