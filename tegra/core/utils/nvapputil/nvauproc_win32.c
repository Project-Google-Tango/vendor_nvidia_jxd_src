/*
 * Copyright (c) 2009 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "nvapputil.h"
#include "nvassert.h"

#if !NVOS_IS_WINDOWS
#error This code can only be compiled for win32
#endif

#include <windows.h>

#define CMDLINE_MAX_LEN 512

//###########################################################################
//############################### CODE ######################################
//###########################################################################

struct NvAuProcessHandleRec
{
    HANDLE ProcHandle;
    HANDLE MainThread;
    NvU32 Pid;
};

NvError
NvAuProcessCreate(
    const char* path,
    int argc,
    char* argv[],
    NvAuProcessHandle* p )
{
    NvAuProcessHandle Proc;
    PROCESS_INFORMATION Info = {0};
    TCHAR Module[MAX_PATH+1];
    TCHAR CmdLine[CMDLINE_MAX_LEN+1];
    TCHAR* Loc;
    int Written;
    NvError Err = NvError_BadParameter;
    int i;

    Proc = NvOsAlloc(sizeof(*Proc));
    if (!Proc) return NvError_InsufficientMemory;
    
    // build command line

    Loc = CmdLine;
    Written = 0;
    for (i = 0; i < argc; i++)
    {
        const char* a = argv[i];

        if (i > 0)
        {
            if (++Written > CMDLINE_MAX_LEN)
                goto clean;
            *(Loc++) = (TCHAR)' ';
        }
        
        while (*a)
        {
            if (++Written > CMDLINE_MAX_LEN)
                goto clean;
            *(Loc++) = (TCHAR)(*(a++));
        }
    }
    *Loc = 0;

    // build module
    Loc = Module;
    Written = 0;
    while (*path)
    {
        if (++Written > MAX_PATH)
            goto clean;
        *(Loc++) = (TCHAR)(*(path++));
    }
    *Loc = 0;
        
    if (!CreateProcess(Module, CmdLine, NULL, NULL, FALSE,
                       CREATE_SUSPENDED, NULL, NULL, NULL,
                       &Info))
        goto clean;

    Proc->Pid = Info.dwProcessId;

    Proc->ProcHandle = OpenProcess(0, FALSE, Info.dwProcessId);
    CloseHandle(Info.hProcess);

#if 0
    /* TODO: [ahatala 2009-09-10] this doesn't work on windows mobile */
    Proc->MainThread = OpenThread(0, FALSE, Info.dwThreadId);
    CloseHandle(Info.hThread);
#else
    Proc->MainThread = Info.hThread;
#endif
    
    NV_ASSERT(Proc->ProcHandle && Proc->MainThread);
    
    *p = Proc;
    return NvSuccess;

 clean:
    NvOsFree(Proc);
    return Err;
}

void
NvAuProcessDestroy ( NvAuProcessHandle p )
{
    if (!NvAuProcessHasExited(p))
    {
        TerminateProcess(p->ProcHandle, 0);
        WaitForSingleObject(p->ProcHandle, INFINITE);
    }

    CloseHandle(p->MainThread);
    CloseHandle(p->ProcHandle);
    NvOsFree(p);
}

NvU32
NvAuProcessGetPid ( NvAuProcessHandle p )
{
    return p->Pid;
}

NvError
NvAuProcessStart( NvAuProcessHandle p )
{
    if (ResumeThread(p->MainThread) != 1)
    {
        return NvError_InvalidState;
    }

    return NvSuccess;
}

NvBool
NvAuProcessHasExited( NvAuProcessHandle p )
{
    return (WaitForSingleObject(p->ProcHandle, 0) != WAIT_TIMEOUT);
}
