/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nverror.h"
#include "nvodm_services.h"

void
NvOdmAvpPreCpuInit(void)
{
}

void
NvOdmAvpPostCpuInit(void)
{
}

// NV_ASSERT ends up in this function.
void NvOsBreakPoint(const char* file, NvU32 line, const char* condition)
{
    for (;;)
        ;
}
