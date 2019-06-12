/*
 * Copyright 2006-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <stdlib.h>
#include "nvos_debug.h"
#include "nvos_linux.h"
#include "nvos_debugcomm.h"

#define GCC_VERSION (__GNUC__ * 10000 \
                              + __GNUC_MINOR__ * 100 \
                              + __GNUC_PATCHLEVEL__)

#if GCC_VERSION > 40200
#define ATTR_CONSTRUCTOR_ALIGNED32 __attribute__((constructor,aligned(32)))
#define ATTR_DESTRUCTOR_ALIGNED32  __attribute__((destructor,aligned(32)))
#else
#define ATTR_CONSTRUCTOR_ALIGNED32
#define ATTR_DESTRUCTOR_ALIGNED32
#endif

#if NVOS_RESTRACKER_COMPILED
static void                     NvDebugcommConfigCallback (NvDebugConfigVar cfgvar, NvU32 value);
static void                     NvDebugcommActionCallback (NvDebugCommAction action);
#endif

const NvOsLinuxKernelDriver*    NvOsLinStubInit (void);
const NvOsLinuxKernelDriver*    NvOsLinUserInit (void);
void                            _init (void);
void                            _fini (void);

/*
 *
 */

#if NVOS_RESTRACKER_COMPILED
void NvDebugcommConfigCallback (NvDebugConfigVar cfgvar, NvU32 value)
{
    switch (cfgvar)
    {
    case NvDebugConfigVar_Stack: /* Config item: type of stack trace */
        NV_ASSERT(value > 0 && value < NvOsCallstackType_Last);
        NvOsResTrackerSetConfig(NvOsResTrackerConfig_CallstackType, value);
        break;
    case NvDebugConfigVar_BreakStackId: /* Config item: Stack Id to break on */
        NvOsResTrackerSetConfig(NvOsResTrackerConfig_BreakStackId, value);
        break;

    case NvDebugConfigVar_BreakId: /* Config item: Alloc Id to break on */
        NvOsResTrackerSetConfig(NvOsResTrackerConfig_BreakId, value);
        break;

    case NvDebugConfigVar_StackFilterPid: /* Config item: PID to filter dump results on */
        NvOsResTrackerSetConfig(NvOsResTrackerConfig_StackFilterPid, value);
        break;

    case NvDebugConfigVar_DisableDump: /* Config item: Disable dumping on process exit */
        NvOsResTrackerSetConfig(NvOsResTrackerConfig_DumpOnExit, !value);
        break;

    default:
        NV_ASSERT(!"Invalid case in switch statement");
        break;
    }
}

void NvDebugcommActionCallback (NvDebugCommAction action)
{
    static NvU32 s_checkpoint = 0;    
    NvU32 cur = NvOsCheckpoint();
    
    switch (action)
    {
    case NvDebugCommAction_Checkpoint:
        s_checkpoint = cur;
        break;
            
    case NvDebugCommAction_Dump:
        NvOsDumpSnapshot(s_checkpoint, cur, NULL);
        break;

    case NvDebugCommAction_DumpToFile:
        {
            const char* DumpFile = NvGetDumpFilePath();
            NvOsFileHandle DumpHandle;

            if (!DumpFile)
            {
                NV_ASSERT(!"Dump file path not specified");
                break;
            }
            
            if (NvOsFopen(DumpFile, NVOS_OPEN_CREATE|NVOS_OPEN_WRITE,
                          &DumpHandle) != NvSuccess)
            {
                NV_ASSERT(!"Can't open file for dumping");
                break;
            }
            
            NvOsDumpSnapshot(s_checkpoint, cur, DumpHandle);
            NvOsFclose(DumpHandle);
            break;
        }

    case NvDebugCommAction_None:
        break;
            
    default:
        NV_ASSERT(!"Invalid case in switch statement");
        break;
    }
}
#endif

void ATTR_CONSTRUCTOR_ALIGNED32 _init(void)
{
    g_NvOsKernel = NvOsLinStubInit();
    if (!g_NvOsKernel)
        g_NvOsKernel = NvOsLinUserInit();

    NvOsInitResourceTracker();

#if NVOS_RESTRACKER_COMPILED
    if (NvDebugCommInit(NvDebugcommConfigCallback, NvDebugcommActionCallback) != NvSuccess)
        NvOsDebugPrintf(RESTRACKER_TAG "Event processing init failed.\n");        
#endif
}

void ATTR_DESTRUCTOR_ALIGNED32 _fini(void)
{
#if NVOS_RESTRACKER_COMPILED
    NvDebugCommDeinit();
#endif
    NvOsDeinitResourceTracker();

    if (g_NvOsKernel)
        g_NvOsKernel->close();
    g_NvOsKernel = NULL;
}
