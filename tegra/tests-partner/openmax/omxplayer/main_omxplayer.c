
/*
 * Copyright (c) 2005-2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvtest.h"
#include "main_omxplayer.h"

// This file exists so that other test applications can run this app as
// a subtest (for example, mt_omx3d).
NvError NvTestMain(int argc, char *argv[])
{
    NvError status = NvSuccess;

status = NvOmxPlayerMain(argc, argv);
    return status;
}




