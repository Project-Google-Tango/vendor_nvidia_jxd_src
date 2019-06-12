/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_USBMUX_J7AJ6_H
#define INCLUDED_USBMUX_J7AJ6_H

#include "usb_mux_hal.h"

#if defined(__cplusplus)
extern "C"
{
#endif

NvOdmUsbMuxHandle MUX_J7AJ6_NvOdmUsbMuxOpen(NvU32 Instance);

void  MUX_J7AJ6_NvOdmUsbMuxClose(NvOdmUsbMuxHandle hOdmUsbMux);

NvBool  MUX_J7AJ6_NvOdmUsbMuxSelectConnector(
                    NvU32 Instance,NvOdmUsbMuxHandle hOdmUsbMux, 
                    NvOdmUsbMuxConnectionEventType Event);

#if defined(__cplusplus)
}
#endif

#endif  // INCLUDED_USBMUX_J7AJ6_H
