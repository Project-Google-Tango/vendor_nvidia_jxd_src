/*
 * Copyright 2007 NVIDIA Corporation.  All Rights Reserved.
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


//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvTioMakeReliable() - make a stream into a reliable stream
//===========================================================================
NvError NvTioMakeReliable(NvTioStreamHandle stream, NvU32 timeout_ms)
{
    return DBERR(NvError_NotSupported);
}
