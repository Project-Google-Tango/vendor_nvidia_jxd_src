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
#include "nvodm_gpio_ext.h"
#include "nvodm_services.h"
#include "gpio_ext_hal.h"
#include "gpio_ext_null.h"
#include "gpio_pcf50626.h"

void
NvOdmExternalGpioWritePins(
    NvU32 Port,
    NvU32 Pin,
    NvU32 PinValue)
{
    static NvBool IsInit = NV_FALSE;
    static NvOdmGpioExtDevice GpioExtDevice;

    if (!IsInit)
    {
        NvOdmOsMemset(&GpioExtDevice, 0, sizeof(GpioExtDevice));
        if (NvOdmPeripheralGetGuid(NV_ODM_GUID('p','c','f','_','p','m','u','0')))
        {
            //  fill in HAL function here.
            GpioExtDevice.pfnWritePins = GPIO_PCF50626_NvOdmExternalGpioWritePins;
        }
        else
        {
            // NULL implementation
            GpioExtDevice.pfnWritePins = null_NvOdmExternalGpioWritePins;
        }
        IsInit = NV_TRUE;
    }
    GpioExtDevice.pfnWritePins(Port, Pin, PinValue);
}

NvU32
NvOdmExternalGpioReadPins(
    NvU32 Port,
    NvU32 Pin)
{
    static NvBool IsInit = NV_FALSE;
    static NvOdmGpioExtDevice GpioExtDevice;

    if (!IsInit)
    {
        NvOdmOsMemset(&GpioExtDevice, 0, sizeof(GpioExtDevice));
        if (NvOdmPeripheralGetGuid(NV_ODM_GUID('p','c','f','_','p','m','u','0')))
        {
            //  fill in HAL function here.
            GpioExtDevice.pfnReadPins = GPIO_PCF50626_NvOdmExternalGpioReadPins;
        }
        else
        {
            // NULL implementation
            GpioExtDevice.pfnReadPins = null_NvOdmExternalGpioReadPins;
        }
        IsInit = NV_TRUE;
    }
    return GpioExtDevice.pfnReadPins(Port, Pin);
}
