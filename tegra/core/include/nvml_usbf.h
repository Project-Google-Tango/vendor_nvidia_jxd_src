/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVML_USBF_H
#define INCLUDED_NVML_USBF_H

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

void NvMlUsbfDisableInterruptsT30(void);
void NvMlUsbfDisableInterruptsT1xx(void);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVML_USBF_H
