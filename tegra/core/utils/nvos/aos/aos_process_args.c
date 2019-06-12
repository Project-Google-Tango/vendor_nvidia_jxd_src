/*
 * Copyright (c) 2008-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "aos.h"
#include "nverror.h"
#include "nvassert.h"

extern const char nvaosProcessArgs[];

static int
computeArgc(char *p, char *end)
{
    static int argc=-1;

    if (argc < 0)
        for(argc = 0; p<end; p++)
            if (!*p)
                argc++;
    return argc;
}

NvAosSemihostStyle
nvaosGetSemihostStyle( void )
{
    const NvAosProcessArgs *pArgs = (NvAosProcessArgs*)&nvaosProcessArgs;
    NvOdmDebugConsole DebugConsoleDevice;
    NvAosSemihostStyle SemihostStyle;
    if (pArgs->Version == NV_AOS_PROCESS_ARGS_VERSION)
        return pArgs->SemiStyle;
    DebugConsoleDevice = aosGetOdmDebugConsole();

    switch(DebugConsoleDevice) {
    case NvOdmDebugConsole_UartA:
    case NvOdmDebugConsole_UartB:
    case NvOdmDebugConsole_UartC:
    case NvOdmDebugConsole_UartD:
    case NvOdmDebugConsole_UartE:
        SemihostStyle = NvAosSemihostStyle_uart;
        break;
    case NvOdmDebugConsole_None:
        SemihostStyle = NvAosSemihostStyle_none;
        break;
    default:
        SemihostStyle = NvAosSemihostStyle_rvice;
        break;
    }
    return SemihostStyle;
}

NV_WEAK void
nvaosGetCommandLine( int *argc, char **argv )
{
    const NvAosProcessArgs *pArgs = (NvAosProcessArgs*)&nvaosProcessArgs;
    char *p = (char*)pArgs->CommandLine;
    char *end = p + pArgs->CommandLineLength;

    NV_ASSERT(argc);
    if (pArgs->Version != NV_AOS_PROCESS_ARGS_VERSION)
    {
        *argc = 0;
        return;
    }

    if (argv != NULL)
    {
        int n=0;
        NV_ASSERT( *argc == computeArgc(p,end) );

        argv[n++] = p;
        while ( p < end && n < *argc )
            if (*p++ == '\0')
                argv[n++] = p;
    }
    else
    {
        *argc = computeArgc(p,end);
    }
}

#if !__GNUC__
#pragma arm section code = "AosProcessArgs", rwdata = "AosProcessArgs", rodata = "AosProcessArgs", zidata ="AosProcessArgs"
#endif
const NV_ALIGN(4) char nvaosProcessArgs[1024]={0};
