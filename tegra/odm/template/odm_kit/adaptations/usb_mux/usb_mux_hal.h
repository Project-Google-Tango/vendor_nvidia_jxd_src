/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** 
 * @file
 * <b>NVIDIA Tegra ODM Kit:
 *         Abstraction layer stub for usb mux
 *         adaptation</b>
 */

#ifndef INCLUDED_NVODM_USB_MUX_ADAPTATION_HAL_H
#define INCLUDED_NVODM_USB_MUX_ADAPTATION_HAL_H

#include "nvodm_usbmux.h"
#include "nvodm_services.h"
#include "nvos.h"
#include "nvodm_services.h"

#ifdef __cplusplus
extern "C"
{
#endif

//  A simple HAL for the USB MUX  adaptations.
typedef NvOdmUsbMuxHandle  (*pfnUsbMuxOpen)(NvU32 Instance);
typedef void  (*pfnUsbMuxClose)(NvOdmUsbMuxHandle hOdmUsbMux);
typedef NvBool  (*pfnUsbMuxSelectConnector)(NvU32 Instance,NvOdmUsbMuxHandle hOdmUsbMux, 
                                                        NvOdmUsbMuxConnectionEventType Event);


typedef struct NvOdmUsbMuxRec
{
    pfnUsbMuxOpen    pfnOpen;
    pfnUsbMuxClose    pfnClose;
    pfnUsbMuxSelectConnector pfnSelectConnector;
    // Gpio Handle
    NvOdmServicesGpioHandle hGpio;
    // Pin handle to  GPIO_PR[4] pin
    NvOdmGpioPinHandle hResetPin;
    // Instance
    NvU32 Instance;
} NvOdmUsbMux;


#ifdef __cplusplus
}
#endif

#endif //INCLUDED_NVODM_USB_MUX_ADAPTATION_HAL_H
