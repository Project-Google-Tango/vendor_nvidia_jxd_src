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

//###########################################################################
//############################### CODE ######################################
//###########################################################################

NvError
NvAuProcessCreate(
    const char* path,
    int argc,
    char* argv[],
    NvAuProcessHandle* p )
{
    NV_ASSERT(!"Not implemented");
    return NvError_NotImplemented;
}

void
NvAuProcessDestroy ( NvAuProcessHandle p )
{
    NV_ASSERT(!"Not implemented");
}

NvU32
NvAuProcessGetPid ( NvAuProcessHandle p )
{
    NV_ASSERT(!"Not implemented");
    return 0;
}

NvError
NvAuProcessStart( NvAuProcessHandle p )
{
    NV_ASSERT(!"Not implemented");
    return NvError_NotImplemented;
}

NvBool
NvAuProcessHasExited( NvAuProcessHandle p )
{
    NV_ASSERT(!"Not implemented");
    return NV_TRUE;
}
