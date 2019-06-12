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

#include "nvapputil.h"

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvAuErrorName() - return string name for error code
//===========================================================================
const char *NvAuErrorName(NvError err)
{
    switch(err)
    {
    #define NVERROR(name, value, str) case NvError_##name: return #name;
    #include "nverrval.h"
    #undef NVERROR
    default:
        break;
    }
    return "Unknown error code";
}
