
#define NV_IDL_IS_STUB

/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvrm_diag.h"

NvError NvRmDiagGetTemperature(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmTmonZoneId ZoneId,
    NvS32 *pTemperatureC)
{
    return NvError_NotSupported;
}
