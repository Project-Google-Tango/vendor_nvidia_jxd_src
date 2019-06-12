/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "nvappmain.h"
#include "nvos.h"
#include "nvrm_moduleloader.h"
#include "nvddk_avp.h"

//###########################################################################
//############################### CODE ######################################
//###########################################################################
volatile __asm void NvOsEnableJtagDebugging(void);

//===========================================================================
// NvAosMain() - entry point for AVP applications
//===========================================================================
#ifndef NV_IS_DYNAMIC
// swi handler stub
void NvSwiHandler(void)
{
}

void NvOsAvpShellThread(int argc, char **argv)
{
    volatile void (*jtagfunc)(void);

    (void)NvAppMain(argc, argv);

    // Ensure jtag enable function doesn't get compiled out
    jtagfunc = NvOsEnableJtagDebugging;
    jtagfunc = jtagfunc ? jtagfunc : NULL;
}

#else
static void NvAppAvpGetCommandLine(int *argc, char ***argv);
static void NvAppAvpGetCommandLine(int *argc, char ***argv)
{
    // TODO: FIXME: implement this for real
    static char *args[] = {
        "UNKNOWN_avp_app",
        0
    };
    *argc = 1;
    *argv = args;
}

//===========================================================================
// NvAosMain() - entry point for AVP applications
//===========================================================================
NvError AxfModuleMain(NvRmModuleLoaderReason Reason, void *Args, NvU32 ArgsSize);
void TestMainWorkerThread(void *context);
NvOsThreadHandle TestMainThreadHandle;
NvError AxfModuleMain(NvRmModuleLoaderReason Reason, void *Args, NvU32 ArgsSize)
{
    NvError Status = NvSuccess;
    switch(Reason) 
    {
        case NvRmModuleLoaderReason_Attach:
        {
            Status = NvOsThreadCreate(TestMainWorkerThread, NULL, &TestMainThreadHandle);
        }
        break;

        case NvRmModuleLoaderReason_Detach:
            NvOsThreadJoin(TestMainThreadHandle);
        break;
    }
    return Status;
}

void TestMainWorkerThread(void *context)
{
    int argc;
    char **argv;
    NvAppAvpGetCommandLine(&argc, &argv);
   (void)NvAppMain(argc, argv);
}
#endif
