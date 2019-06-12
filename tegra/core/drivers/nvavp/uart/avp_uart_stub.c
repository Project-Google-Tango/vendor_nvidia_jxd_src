/*
 * Copyright (c) 2012 - 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
#include "nvcommon.h"
#include "nverror.h"
#include "nvuart.h"

NvS32 NvAvpUartPoll(void)
{
    return -1;
}

void NvAvpUartInit(NvU32 pll)
{
}

void NvAvpUartWrite(const void *ptr)
{
}

void NvOsAvpDebugPrintf(const char *format, ...)
{
}

NvOdmDebugConsole NvAvpGetDebugPort(void)
{
 return NvOdmDebugConsole_None;
}
