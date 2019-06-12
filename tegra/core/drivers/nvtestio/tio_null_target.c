/*
 * Copyright (c) 2007 NVIDIA Corporation.  All Rights Reserved.
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

#include "tio_local.h"
#include "nvassert.h"

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvTioRegisterFile() - register for remote file I/O 
//===========================================================================
void NvTioRegisterFile(void)
{ }

//===========================================================================
// NvTioRegisterHost() - register for IO with host
//===========================================================================
void NvTioRegisterHost(void)
{ }

//===========================================================================
// NvTioUnregisterHost() - unregister IO with host
//===========================================================================
void NvTioUnregisterHost(void)
{ }

//===========================================================================
// NvTioBreakpoint() - breakpoint loop
//===========================================================================
void NvTioBreakpoint(void)
{
    NV_ASSERT(!"Do not call from host!");
}

//===========================================================================
// NvTioConnectToHost() - Connect to host machine
//===========================================================================
NvError NvTioConnectToHost(
                    NvTioTransport transportToUse,
                    NvTioStreamHandle transport, 
                    const char *protocol,
                    NvTioHostHandle *pHost)
{
    NV_ASSERT(!"Do not call from host!");
    return NvError_NotSupported;
}

//===========================================================================
// NvTioDisconnectFromHost() - Connect to host machine
//===========================================================================
void NvTioDisconnectFromHost(NvTioHostHandle host)
{
    NV_ASSERT(!"Do not call from host!");
}

//===========================================================================
// NvTioGetSdramParams - Get sdram parameters passed from the host.
//===========================================================================
void
NvTioGetSdramParams(NvU8 **params)
{
    NV_ASSERT(!"Do not call from host!");
}

//===========================================================================
// NvTioGetBlitPatternParams() - Get Blit Pattern parameters from host
//===========================================================================
void
NvTioGetBlitPatternParams(NvU8 **params)
{
    NV_ASSERT(!"Do not call from host!");
}
