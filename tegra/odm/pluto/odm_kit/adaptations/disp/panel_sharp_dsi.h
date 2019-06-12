/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_PANEL_SHARP_LS050T1SX01_DSI_H
#define INCLUDED_PANEL_SHARP_LS050T1SX01_DSI_H

#include "panels.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define SHARP_LS050T1SX01_PANEL_GUID NV_ODM_GUID('s','p','L','S','-','X','0','1')

void sharp_GetHal(NvOdmDispDeviceHandle hDevice);

#if defined(__cplusplus)
}
#endif

#endif  //INCLUDED_PANEL_SHARP_LS050T1SX01_DSI_H
