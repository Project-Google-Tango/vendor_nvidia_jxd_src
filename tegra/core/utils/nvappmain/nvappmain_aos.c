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

//###########################################################################
//############################### CODE ######################################
//###########################################################################
#if !__GNUC__
volatile __asm void NvOsEnableJtagDebugging(void);
#endif

//===========================================================================
// main2() - entry point for AOS applications
//===========================================================================
int main2(int argc, char **argv);
int main2(int argc, char **argv)
{
#if !__GNUC__
    volatile void (*jtagfunc)(void);

    // Ensure jtag enable function doesn't get compiled out
    jtagfunc = NvOsEnableJtagDebugging;
    jtagfunc = jtagfunc ? jtagfunc : NULL;
#endif
    return (int)NvAppMain(argc, argv);
}
