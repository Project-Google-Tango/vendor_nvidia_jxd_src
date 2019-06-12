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
#include "nvtestio.h"

//###########################################################################
//############################### DEFINES ##################################
//###########################################################################

#define NVMAIN_CONFIG_ARG_DELIMITER     '#'

//###########################################################################
//############################### CODE ######################################
//###########################################################################

int NvMainGetConfigArgsCount(int argc, char **argv);
NvError NvMainRemoveConfigArgs(int argc, char **argv, char **newArgv);

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
// NvMainGetConfigArgsCount() - Parse the test command line and return the number of arguments
// that start with '#'
//===========================================================================
int NvMainGetConfigArgsCount(int argc, char **argv)
{
    int i=0,j;
    for (j=1; j<argc; j++)
    {
        if (argv[j][0] == NVMAIN_CONFIG_ARG_DELIMITER)
            i++;
    }
    return i;
}
//===========================================================================
// NvMainRemoveConfigArgs() - Parse the test command line for configuration arguments (commands
// starting with '#') and remove them from the command line
//===========================================================================
NvError NvMainRemoveConfigArgs(int argc, char **argv,
                                         char **newArgv)
{
    int i,j,cmdLength;
    NvError err = NvSuccess;
    i=0;
    j=0;
    while(i < argc)
    {
        if (argv[i][0] != NVMAIN_CONFIG_ARG_DELIMITER)
        {
            cmdLength = NvOsStrlen(argv[i]);
            //Allocate sizeof(argv[i]) + 1 to accomodate '\0'
            newArgv[j] = (char *)NvOsAlloc((cmdLength+1) * sizeof(char));
            if(newArgv[j] == NULL)
                return NvError_InsufficientMemory;
            NvOsStrncpy(newArgv[j],argv[i],cmdLength+1);    // Copy \0 as well
            j++;
        }
        i++;
    }
    return err;
}

//===========================================================================
// NvAppMain() - This is the entry point to the application
//===========================================================================
NvError NvAppMain(int argc, char *argv[])
{
    NvTestApplication app;
    NvError           err = NvSuccess;
    char              **newArgv = NULL;
    int               i,numArgs;

    if (0) {
        static volatile int foo=1,bar=0;
        while(foo) {
            bar++;
        } 
    }

    NvTestInitialize(&argc, argv, &app);
    numArgs = NvMainGetConfigArgsCount(argc,argv);
    if(numArgs)
    {
        newArgv = (char **)NvOsAlloc((argc-numArgs) * sizeof(char*));
        if(newArgv != NULL)
        {
            err = NvMainRemoveConfigArgs(argc,argv,newArgv);
            argc -= numArgs;
        }
        else
            err = NvError_InsufficientMemory;
    }
    if(err == NvSuccess)
    {
        NvTestSetRunFunction(app, NvRun);
        LoadDone();
        NvLoad();
        err = NvTestMain(argc, (newArgv == NULL)?argv:newArgv);
    }
    if (err)
        NvTestError(app, err);
    err = NvTestTerminate(app);
    if(newArgv)
    {
        for (i=0; i<argc; i++)
            NvOsFree(newArgv[i]);
        NvOsFree(newArgv);
    }
    return err;
}

