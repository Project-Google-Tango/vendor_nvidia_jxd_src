/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_usbmux.h"
#include "usb_mux_null.h"


NvOdmUsbMuxHandle  null_NvOdmUsbMuxOpen(NvU32 Instance)
{
    // NULL implementation that does nothing.
    return NULL;
}
void  null_NvOdmUsbMuxClose(NvOdmUsbMuxHandle hOdmUsbMux)
{
    // NULL implementation that does nothing.
    return;
}

NvBool  null_NvOdmUsbMuxSelectConnector(NvU32 Instance,NvOdmUsbMuxHandle hOdmUsbMux, 
                    NvOdmUsbMuxConnectionEventType Event)
{
   // NULL implementation that does nothing.
    return NV_FALSE;

}

