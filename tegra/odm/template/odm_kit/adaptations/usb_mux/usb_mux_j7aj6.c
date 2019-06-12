/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_services.h"
#include "nvassert.h"
#include "nvodm_usbmux.h"
#include "usb_mux_j7aj6.h"


#define NVODM_PORT(x) ((x) - 'a')


NvOdmUsbMuxHandle   MUX_J7AJ6_NvOdmUsbMuxOpen(NvU32 Instance)
{
    static NvOdmUsbMux *pDevice = NULL;
    NvOdmServicesGpioHandle hOdmGpio = NULL;
    NvOdmGpioPinHandle      hPin = NULL;

    if(pDevice != NULL)
    {
        return pDevice;
    }
    pDevice = NvOdmOsAlloc(sizeof(NvOdmUsbMux));
    if(pDevice == NULL)
    {
        goto fail;
    }
    // Getting the OdmGpio Handle
    hOdmGpio = NvOdmGpioOpen();
    if (hOdmGpio == NULL)
    {
        goto fail;
    }
    // Mux selections GPIO port 'r' pin 3
    hPin = NvOdmGpioAcquirePinHandle(hOdmGpio, NVODM_PORT('r'), 3);
    if (hPin == NULL)
    {
        // No GPIO pin handle returned, probably due to insufficient memory.
        goto fail;
    }
     NvOdmGpioConfig(hOdmGpio, hPin, NvOdmGpioPinMode_Output);

    // Setting the Output Pin to Low
    NvOdmGpioSetState(hOdmGpio, hPin, 0x0);
    
    pDevice->Instance = Instance;
    pDevice->hGpio = hOdmGpio;
    pDevice->hResetPin = hPin;

    return pDevice;

    fail:
    /* Put back the key to the funciton mode */
    if(hPin)
    {
        NvOdmGpioConfig(hOdmGpio, hPin, NvOdmGpioPinMode_Function);
        // Release all resources and return result.
        NvOdmGpioReleasePinHandle(hOdmGpio, hPin);
    }
    if(hOdmGpio)
        NvOdmGpioClose(hOdmGpio);
    if(pDevice)
        NvOdmOsFree(pDevice);
    pDevice = NULL;
    return (pDevice);
}

void  MUX_J7AJ6_NvOdmUsbMuxClose(NvOdmUsbMuxHandle hOdmUsbMux)
{
    if(hOdmUsbMux)
    {
        NvOdmGpioConfig(hOdmUsbMux->hGpio,
               hOdmUsbMux->hResetPin, NvOdmGpioPinMode_Function);
        // Release all resources and return result.
        NvOdmGpioReleasePinHandle(hOdmUsbMux->hGpio, hOdmUsbMux->hResetPin);
        NvOdmGpioClose(hOdmUsbMux->hGpio);
        NvOdmOsFree(hOdmUsbMux);
        hOdmUsbMux = NULL;
    }

}


NvBool  MUX_J7AJ6_NvOdmUsbMuxSelectConnector(NvU32 Instance,NvOdmUsbMuxHandle hOdmUsbMux, 
                    NvOdmUsbMuxConnectionEventType Event)
{
    if(!hOdmUsbMux)
        return NV_FALSE;

    if((hOdmUsbMux->Instance == Instance) && (Event  == NvOdmUsbMuxConnectionEvent_Peripheral))
    {
        // Drive the pin to high
        NvOdmGpioSetState(hOdmUsbMux->hGpio, hOdmUsbMux->hResetPin, 0x01);
    }
    else if((hOdmUsbMux->Instance == Instance) && (Event  == NvOdmUsbMuxConnectionEvent_ExternalHost))
    {
        // Drive the pin to low
        NvOdmGpioSetState(hOdmUsbMux->hGpio, hOdmUsbMux->hResetPin, 0x0);
    }

    return NV_TRUE;

}

