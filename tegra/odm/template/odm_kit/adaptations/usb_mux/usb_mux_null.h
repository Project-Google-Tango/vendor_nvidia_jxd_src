/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_GPIO_EXT_NULL_H
#define INCLUDED_GPIO_EXT_NULL_H

#include "usb_mux_hal.h"
#include "nvodm_usbmux.h"

#if defined(__cplusplus)
extern "C"
{
#endif


NvOdmUsbMuxHandle  null_NvOdmUsbMuxOpen(NvU32 Instance);

void  null_NvOdmUsbMuxClose(NvOdmUsbMuxHandle hOdmUsbMux);

NvBool  null_NvOdmUsbMuxSelectConnector(
                    NvU32 Instance,NvOdmUsbMuxHandle hOdmUsbMux, 
                    NvOdmUsbMuxConnectionEventType Event);


#if defined(__cplusplus)
}
#endif

#endif
