/*
 * Copyright (c) 2013-2014, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef INCLUDED_NVTEST_H
#define INCLUDED_NVTEST_H

#include "nvos.h"
#include "nvcommon.h"
#include "nvutil.h"
#include "nvelf.h"
#include "nvelfutil.h"

typedef struct SegHeaderAndOffsetRec
{
    NvElfSegmentHeader SegHeader;
    NvU32              Offset;
} SegHeaderAndOffset;

NvError LoadNvTest(
    const char *Filename,
    NvU32 *pEntryPoint,
    NvU8 **MsgBuffer,
    NvU32 *Length);

NvError SaveSignedNvTest(
    NvU32 Length,
    NvU8 *BlobBuf,
    const char *Filename,
    const char *ExtStr);

#endif // INCLUDED_NVTEST_H
