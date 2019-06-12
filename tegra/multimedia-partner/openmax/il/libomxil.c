/*
 * Copyright (c) 2007 - 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * libomxil.c : Defines the entry point for the DLL application.  Registers with
 *    EGL when DLL is loaded.
 */

#include "nvxegl.h"

#if NVOS_IS_WINDOWS
#include <windows.h>
#endif

#if NVOS_IS_WINDOWS
//===========================================================================
// DllMain() - entry and exit function for windows .dll
//===========================================================================
BOOL __stdcall DllMain(HANDLE hinstDLL, DWORD dwReason, LPVOID lpvReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH) 
    {
    }
    else if (dwReason == DLL_PROCESS_DETACH) 
    {
    }

    return TRUE;
}
#endif

#if NVOS_IS_LINUX || NVOS_IS_UNIX
//===========================================================================
// _init() - entry function for linux .so
//===========================================================================
void _init(void);
void __attribute__((constructor,aligned(32))) _init(void)
{
    asm volatile ("nop\n");
}

//===========================================================================
// _fini() - exit function for linux .so
//===========================================================================
void _fini(void);
void __attribute__((destructor,aligned(32))) _fini(void)
{
    asm volatile ("nop\n");
}
#endif


