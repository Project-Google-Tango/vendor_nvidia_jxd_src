/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVRM_FIFO_H
#define INCLUDED_NVRM_FIFO_H

#include "nvos.h"

typedef struct NvRmFifoPairRec
{
    NvOsFileHandle  hFifoIn;
    NvOsFileHandle  hFifoOut;
    NvUPtr          id;
} NvRmFifoPair;

NvOsFileHandle NvRmGetIoctlFile(void);
NvError NvRmGetFifoPair(NvRmFifoPair *pair);
void NvRmReleaseFifoPair(NvU32 id);

#endif // INCLUDED_NVRM_FIFO_H
