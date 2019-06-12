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
// NvTioPoll() - Poll for streams which can be read without blocking
//===========================================================================
NvError NvTioPoll(NvTioPollDesc *tioDesc, NvU32 cnt, NvU32 timeout_msec)
{
    NV_ASSERT(!"Do not call from target");
    return DBERR(NvError_NotSupported);
}


