/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvodm_query_discovery.h"
#include "nvodm_usbmux.h"
#include "nvodm_services.h"
#include "usb_mux_hal.h"
#include "usb_mux_null.h"
#include "usb_mux_j7aj6.h"

#define USB_MUX_ID NV_ODM_GUID('u','s','b','m','x','J','7','6')

NvOdmUsbMuxHandle  NvOdmUsbMuxOpen(NvU32 Instance)
{
    NvOdmUsbMux UsbMux;
    if (NvOdmPeripheralGetGuid(USB_MUX_ID))
    {
        //  fill in HAL function here.
        UsbMux.pfnOpen = MUX_J7AJ6_NvOdmUsbMuxOpen;
    }
    else
    {
        // NULL implementation
        UsbMux.pfnOpen = null_NvOdmUsbMuxOpen;
    }
    return UsbMux.pfnOpen(Instance);
}

void  NvOdmUsbMuxClose(NvOdmUsbMuxHandle hOdmUsbMux)
{
    NvOdmUsbMux UsbMux;
    if (NvOdmPeripheralGetGuid(USB_MUX_ID))
    {
        //  fill in HAL function here.
        UsbMux.pfnClose = MUX_J7AJ6_NvOdmUsbMuxClose;
    }
    else
    {
        // NULL implementation
        UsbMux.pfnClose = null_NvOdmUsbMuxClose;
    }
    UsbMux.pfnClose(hOdmUsbMux);
    return;
}


NvBool  NvOdmUsbMuxSelectConnector(NvU32 Instance,NvOdmUsbMuxHandle hOdmUsbMux, 
                    NvOdmUsbMuxConnectionEventType Event)
{
    NvOdmUsbMux UsbMux;
    if (NvOdmPeripheralGetGuid(USB_MUX_ID))
    {
        //  fill in HAL function here.
        UsbMux.pfnSelectConnector = MUX_J7AJ6_NvOdmUsbMuxSelectConnector;
    }
    else
    {
        // NULL implementation
        UsbMux.pfnSelectConnector = null_NvOdmUsbMuxSelectConnector;
    }
    return UsbMux.pfnSelectConnector(Instance, hOdmUsbMux, Event);
}

