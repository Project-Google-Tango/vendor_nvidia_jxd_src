/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVAPPMAIN_H
#define INCLUDED_NVAPPMAIN_H

#include "nvcommon.h"
#include "nverror.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Application entry point.
 *
 * @param argc Number of arguments including executable name
 * @param argv Program aguments
 *
 * libnvappmain exports the 'main' symbol.  Applications should only export
 * 'NvAppMain'.  Often NvAppMain() is implemented by nvtest, where it calls 
 * NvTestMain(), so most applications will actually implement NvTestMain() 
 * (or NvTestCreate(), NvTestRun(), and NvTestDestroy()), and not NvAppMain().
 */
NvError
NvAppMain(int argc, char *argv[]);

#ifdef LP0_SUPPORTED
NvError NvAppSuspend(void);
void NvAppResume(void);
#endif

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVAPPMAIN_H
