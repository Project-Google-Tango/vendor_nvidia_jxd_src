/*
 * Copyright 2007 - 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvos.h"

#if !(NVOS_IS_LINUX || NVOS_IS_DARWIN)

NvCallstack* NvOsCallstackCreate(NvOsCallstackType stackType)
{
    return NULL;
}

void NvOsCallstackDestroy(NvCallstack* callstack)
{
}

NvU32 NvOsCallstackGetHeight(NvCallstack* stack)
{
    return 0;
}

void NvOsCallstackGetFrame(
        char* buf,
        NvU32 len,
        NvCallstack* stack,
        NvU32 level)
{
    NvOsStrncpy(buf, "<stack>", len);
}

NvU32 NvOsCallstackHash(NvCallstack* stack)
{
    return 0;
}

void NvOsCallstackDump(
        NvCallstack* stack,
        NvU32 skip,
        NvOsDumpCallback callBack,
        void* context)
{
}

NvBool NvOsCallstackContainsPid(NvCallstack* stack, NvU32 pid)
{
    return NV_FALSE;
}

#endif // !(NVOS_IS_LINUX)
