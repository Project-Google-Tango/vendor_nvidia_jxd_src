/*
 * Copyright 2007 - 2008 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */
 
#ifndef INCLUDED_NVOS_DEBUGCOMM_H
#define INCLUDED_NVOS_DEBUGCOMM_H

#ifndef INCLUDED_NVOS_H
#include "nvos.h"
#endif

#ifndef INCLUDED_NVOS_DEBUG_H
#include "nvos_debug.h"
#endif

#if NVOS_RESTRACKER_COMPILED

#define NVOS_INVALID_PID ((NvU32)-1)

typedef enum
{
    NvDebugConfigVar_Stack = 1,
    NvDebugConfigVar_BreakStackId,
    NvDebugConfigVar_BreakId,
    NvDebugConfigVar_StackFilterPid,
    NvDebugConfigVar_DisableDump,

    NvDebugConfigVar_Last,
    NvDebugConfigVar_Force32 = 0x7FFFFFFF
} NvDebugConfigVar;

typedef enum
{
    NvDebugCommAction_None = 1,
    NvDebugCommAction_Checkpoint,
    NvDebugCommAction_Dump,
    NvDebugCommAction_DumpToFile,
    NvDebugCommAction_KillDebugcommListener,

    NvDebugCommAction_Last,
    NvDebugCommAction_Force32 = 0x7FFFFFFF
} NvDebugCommAction;

typedef void (*NvConfigCallback) (NvDebugConfigVar cfgvar, NvU32 value);
typedef void (*NvActionCallback) (NvDebugCommAction action);

/* Client (public) interface. */

NvError NvReadConfigInteger         (NvDebugConfigVar name, NvU32 *value);
NvError NvWriteConfigInteger        (NvDebugConfigVar name, NvU32 value);
NvError NvSendAction                (NvDebugCommAction action, NvU32 targetProcessPid);
const char* NvGetDumpFilePath       (void);

/* NVOS internal interface. */

NvError NvDebugCommInit             (NvConfigCallback config, NvActionCallback action);
void    NvDebugCommDeinit           (void);

#endif // NVOS_RESTRACKER_COMPILED

#endif
