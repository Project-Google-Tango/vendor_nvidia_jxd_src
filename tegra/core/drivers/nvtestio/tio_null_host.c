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
// NvTioDisconnectFromTarget() - connect to target machine
//===========================================================================
void NvTioDisconnectFromTarget(NvTioTargetHandle target)
{
    NV_ASSERT(!"Do not call from target");
}

//===========================================================================
// NvTioConnectToTarget() - connect to target machine
//===========================================================================
NvError NvTioConnectToTarget(
                    NvTioStreamHandle connection, 
                    const char *protocol,
                    NvTioTargetHandle *pTarget)
{
    NV_ASSERT(!"Do not call from target");
    return DBERR(NvError_NotSupported);
}

//===========================================================================
// NvTioTargetFopen() - open a file or stream on the target
//===========================================================================
NvError NvTioTargetFopen(
            NvTioTargetHandle target,
            const char *path, 
            NvTioStreamHandle *pStream)
{
    NV_ASSERT(!"Do not call from target");
    return DBERR(NvError_NotSupported);
}

