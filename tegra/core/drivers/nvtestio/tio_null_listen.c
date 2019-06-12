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

#include "tio_local_host.h"
#include "nvassert.h"
#include "nvos.h"

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvTioListen() - listen for connections
//===========================================================================
NvError NvTioListen(const char *addr, NvTioStreamHandle *pListen)
{
    NV_ASSERT(!"Do not call from target");
    return DBERR(NvError_NotSupported);
}

//===========================================================================
// NvTioAcceptTimeout() - accept connections
//===========================================================================
NvError NvTioAcceptTimeout(
                NvTioStreamHandle listen,
                NvTioStreamHandle *pStream,
                NvU32 timeout)
{
    NV_ASSERT(!"Do not call from target");
    return DBERR(NvError_NotSupported);
}

//===========================================================================
// NvTioAccept() - accept connections
//===========================================================================
NvError NvTioAccept(NvTioStreamHandle listen, NvTioStreamHandle *pStream)
{
    NV_ASSERT(!"Do not call from target");
    return DBERR(NvError_NotSupported);
}

