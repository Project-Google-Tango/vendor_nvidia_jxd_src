/*
 * Copyright (c) 2007 - 2011 NVIDIA Corporation.  All rights reserved.
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
#include "KD/kd.h"

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// kdMain() - entry point
//===========================================================================
KDint kdMain(KDint argc, const KDchar *const *argv)
{
    return (NvAppMain(argc, (char **) argv) == NvSuccess) ? 0 : 1;
}
