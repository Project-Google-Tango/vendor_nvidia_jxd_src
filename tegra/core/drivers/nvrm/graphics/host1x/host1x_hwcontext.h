/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef NVRM_HWCONTEXT_H
#define NVRM_HWCONTEXT_H

#include "nvrm_channel.h"

typedef struct NvRmContextRec {
    // This is gathered when no context change was detected (to restore the
    // class of the unit).  Immutable, global.
    NvRmMemHandle hContextNoChange;
    // This is gathered to set the address where the registers will be read
    // to.  Immutable, one per process.
    NvRmMemHandle hContextSetAddress;
    // This is gathered when saving the current context to memory.  Immutable,
    // global.
    NvRmMemHandle hContextSave;
    // This is gathered to restore a previously saved context.  Mutable, one
    // per process.
    NvRmMemHandle hContextRestore;

    // This is not gather. Not shared across contexts.
    //Need to remember few things from previous frame.
    //for MPE h/w bug 526915
    NvRmMemHandle hContextMpeHwBugFix;

    // The offset at which context reading thread will store register
    // programming commands.
    NvU32 ContextRestoreOffset;
    // This is copied to the pushbuffer.
    NvData32 *pContextRestoreExtra;

    // Sizes are in words.
    NvU32 ContextNoChangeSize;
    NvU32 ContextSetAddressSize;
    NvU32 ContextSaveSize;
    NvU32 ContextRestoreSize;
    NvRmModuleID Engine;
    // We need class ID to set class before doing sync point increment to idle
    // the unit.
    NvU32 ClassID;

    // RefCount incremented in the main thread and decremented in rm-host-ctx
    // thread.
    NvU32 RefCount;
    // Channel handle is needed in NvRmContextFree for cleaning up last
    // context used field.
    NvRmChannelHandle hChannel;

    NvBool IsValid;

    // The context can be freed once the sync point with SyncPointID id
    // reached SyncPointValue.
    NvU32 SyncPointID;
    NvU32 SyncPointValue;

    NvBool Queueless;
    NvU32 QueuelessSyncpointIncrement;
} NvRmContext;

/*
 * Checks for context switch.  If DryRun is NV_FALSE, inserts context store
 * and restore commands if necessary.
 *
 * @returns Number of extra sync point increments inserted.
 */
NvU32 NvRmPrivCheckForContextChange( NvRmChannelHandle hChannel,
    NvRmContextHandle hContext, NvRmModuleID ModuleID,
    NvU32 SyncPointID, NvU32 SyncPointValue, NvBool DryRun );

void
NvRmPrivContextFree( NvRmContextHandle hContext );

/*
 * Starts the context reading thread.
 */
void
NvRmPrivContextReadHandlerInit( NvRmDeviceHandle rm );

/*
 * Shuts down context reading thread.
 */
void
NvRmPrivContextReadHandlerDeinit( NvRmDeviceHandle rm );

/*
 * Placeholder context which is assigned to a channel during unit powerdown.
 */
#define FAKE_POWERDOWN_CONTEXT ((NvRmContextHandle)-1)

#endif /* NVRM_HWCONTEXT_H */
