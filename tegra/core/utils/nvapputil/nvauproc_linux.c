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

#if !NVOS_IS_UNIX
#error This code can only be compiled for linux and friends
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

// Android non-conformance workarounds

#ifndef WIFEXITSTATUS
#ifdef WEXITSTATUS
#define WIFEXITSTATUS WEXITSTATUS
#else
#error Broken linux environment
#endif
#endif

// <2.6 kernels

#ifndef WCONTINUED
#define WCONTINUED WNOHANG
#endif

#define MAGIC_EXIT_CODE 23

//###########################################################################
//############################### CODE ######################################
//###########################################################################

struct NvAuProcessHandleRec
{
    pid_t child;
    char** argv;
    NvBool terminated;
};

NvError
NvAuProcessCreate(
    const char* path,
    int argc,
    char* argv[],
    NvAuProcessHandle* p )
{
    NvAuProcessHandle Proc;
    int i;
    int argvlen = argc+2;
    int status;
    int ret;

    /* TODO: [ahatala 2009-09-08] could check that there is something
       executable in 'path' instead of failing at ProcessStart */

    Proc = NvOsAlloc(sizeof(*Proc));
    if (!Proc) return NvError_InsufficientMemory;
    NvOsMemset(Proc, 0, sizeof(*Proc));

    Proc->terminated = NV_TRUE;
    
    Proc->argv = NvOsAlloc(argvlen*sizeof(char*));
    if (!Proc->argv)
    {
        NvAuProcessDestroy(Proc);
        return NvError_InsufficientMemory;    
    }
    NvOsMemset(Proc->argv, 0, argvlen*sizeof(char*));

    for (i = 0; i < argc+1; i++)
    {
        const char* a = (i == 0) ? path : argv[i-1];
        Proc->argv[i] = strdup(a);
        if (!Proc->argv[i])
        {
            NvAuProcessDestroy(Proc);
            return NvError_InsufficientMemory;    
        }
    }
    
    Proc->child = fork();
    if (Proc->child == -1)
    {
        NvAuProcessDestroy(Proc);
        return NvError_InsufficientMemory;    
    }
    else if (Proc->child == 0)
    {
        //
        // Code run in the child process
        //

        // Suspend
        kill(getpid(), SIGSTOP);

        // Start executing
        execvp(Proc->argv[0], Proc->argv);

        // If we are here, exec failed
        exit(MAGIC_EXIT_CODE);
    }

    ret = waitpid(Proc->child, &status, WUNTRACED);

#if 0
    NvOsDebugPrintf("create: wait returned %d", ret);
    NvOsDebugPrintf("create: wait status %x", status);
#endif
    
    if ((ret != Proc->child) || WIFEXITED(status))
    {
        NvAuProcessDestroy(Proc);
        return NvError_InvalidState;
    }
    
    Proc->terminated = NV_FALSE;

    *p = Proc;
    return NvSuccess;    
}

void
NvAuProcessDestroy ( NvAuProcessHandle p )
{
    if (!p->terminated)
    {
        int status;
        kill(p->child, SIGKILL);
        waitpid(p->child, &status, 0);
    }

    if (p->argv)
    {
        char** arg = p->argv;
        while (*arg)
        {
            NvOsFree(*arg);
            arg++;
        }
        NvOsFree(p->argv);
    }

    NvOsFree(p);            
}

NvU32
NvAuProcessGetPid ( NvAuProcessHandle p )
{
    return (NvU32)p->child;
}

NvError
NvAuProcessStart( NvAuProcessHandle p )
{
    int status;    
    int ret;
    
    if (p->terminated)
        return NvError_InvalidState;

    kill(p->child, SIGCONT);
    ret = waitpid(p->child, &status, WCONTINUED);

#if 0
    NvOsDebugPrintf("start: wait returned %d", ret);
    NvOsDebugPrintf("start: wait status %x", status);
#endif
    
    if ((ret == p->child) && !WIFEXITED(status))
    {
        return NvSuccess;
    }

    // The process managed to exit already or something else
    // is broken. Consider the child gone.

    p->terminated = NV_TRUE;
    
    if ((ret == p->child) &&
        WIFEXITED(status) &&
        (WIFEXITSTATUS(status) == MAGIC_EXIT_CODE))
    {
        // Launch failed
        return NvError_BadParameter;
    }

    return NvError_InvalidState;
}

NvBool
NvAuProcessHasExited( NvAuProcessHandle p )
{
    if (!p->terminated)
    {    
        int status;
        int ret = waitpid(p->child, &status, WNOHANG);

#if 0
        NvOsDebugPrintf("hasexited: wait returned %d", ret);
        NvOsDebugPrintf("hasexited: wait status %x", status);
#endif
        
        if (ret == -1)
        {
            NvOsDebugPrintf("ERROR checking child status!");
            p->terminated = NV_TRUE;
        }
        else if (ret == p->child)
        {
            p->terminated = WIFEXITED(status);
        }
        else
        {
            NV_ASSERT(ret == 0);
        }
    }
    return p->terminated;
}
