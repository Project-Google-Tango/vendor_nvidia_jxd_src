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

#include "nvtest.h"
#include "nvappmain.h"

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// LoadDone() - Called when all libraries are loaded
//===========================================================================
static void LoadDone(void)
{
    // This function intentionally does nothing.  You can set a breakpoint here
    // if you want to stop at a point where all the test DLLs are loaded.  This
    // allows you, for example, to set gdb breakpoints using tab completion.
}

//===========================================================================
// NvLoad() - Called when all libraries are loaded
//===========================================================================
void NvLoad(void)
{
    // This function intentionally does nothing.  You can set a breakpoint here
    // if you want to stop at a point where all the test DLLs are loaded.  This
    // allows you, for example, to set gdb breakpoints using tab completion.
}

//===========================================================================
// NvRun() - Called ust before running each subtest
//===========================================================================
void NvRun(NvU32 subtestIndex)
{
    // This function intentionally does nothing.  You can set a breakpoint here
    // if you want to stop at a point just before a subtest runs.  The 
    // subtestIndex will be set to the index of the subtest that is about to 
    // run.
}

//===========================================================================
// NvAppMain() - This is the entry point to the application
//===========================================================================
NvError NvAppMain(int argc, char *argv[])
{
    NvTestApplication app;
    NvTestHandle test = 0;
    NvU32 score;
    NvError err, err2;

    NvTestInitialize(&argc, argv, &app);
    NvTestSetRunFunction(app, NvRun);

    LoadDone();
    NvLoad();

    err = NvTestCreate(&test, argc, argv);
    if (!err)
        err = NvTestRun(test, &score);
    if (test)
        NvTestDestroy(test);
    err2 = NvTestTerminate(app);
    return err ? err : err2;
}

