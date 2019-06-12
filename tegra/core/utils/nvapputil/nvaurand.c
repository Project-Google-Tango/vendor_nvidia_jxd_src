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
// NvAuRand16() - generate a pseudo random 16 bit value and update the state
//===========================================================================
NvU16 NvAuRand16(NvU32 *pRandState)
{
    NvU32 RandState = *pRandState;
    RandState = RandState * 1103515245 + 12345;
    *pRandState = RandState;
    return (NvU16)(RandState >> 16);
}
