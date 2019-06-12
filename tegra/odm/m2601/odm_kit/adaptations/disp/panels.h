/*
 * Copyright (c) 2007-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_PANELS_H
#define INCLUDED_PANELS_H

#include "nvcommon.h"
#include "nvodm_disp.h"
#include "nvodm_query_discovery.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct NvOdmDispDeviceRec
{
    /**
     * Called during NvOdmDispListDevices to give the panel ODM software a
     * chance to initialize.
     */
    NvBool (*Setup)( NvOdmDispDeviceHandle hDevice );

    NvBool (*BacklightSetup)(NvOdmDispDeviceHandle, NvBool);

    void (*Release)( NvOdmDispDeviceHandle hDevice );

    void (*BacklightRelease)(NvOdmDispDeviceHandle);

    void (*BacklightIntensity)(NvOdmDispDeviceHandle,
                                    NvU8);

    void (*ListModes)( NvOdmDispDeviceHandle hDevice, NvU32 *nModes,
        NvOdmDispDeviceMode *modes );

    NvBool (*SetMode)( NvOdmDispDeviceHandle hDevice,
        NvOdmDispDeviceMode *mode, NvU32 flags );

    void (*SetBacklight)( NvOdmDispDeviceHandle hDevice, NvU8 intensity );

    void (*SetPowerLevel)( NvOdmDispDeviceHandle hDevice,
        NvOdmDispDevicePowerLevel level );

    NvBool (*GetParameter)( NvOdmDispDeviceHandle hDevice,
        NvOdmDispParameter param, NvU32 *val );

    void * (*GetConfiguration)( NvOdmDispDeviceHandle hDevice );

    void (*GetPinPolarities)( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
        NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values );

    NvBool (*SetSpecialFunction)( NvOdmDispDeviceHandle hDevice,
        NvOdmDispSpecialFunction function, void *config );

    NvOdmDispDeviceType Type;
    NvOdmDispDeviceUsageType Usage;
    NvU32 MaxHorizontalResolution;
    NvU32 MaxVerticalResolution;
    NvOdmDispBaseColorSize BaseColorSize;
    NvOdmDispDataAlignment DataAlignment;
    NvOdmDispPinMap PinMap;
    NvU32 PwmOutputPin;

    NvOdmDispDeviceMode *modes;
    NvOdmDispDeviceMode CurrentMode;
    NvU32 nModes;
    NvOdmDispDevicePowerLevel power;
    NvBool bInitialized;

    /* peripheral configuration */
    NvOdmPeripheralConnectivity const * PeripheralConnectivity;

    union
    {
        NvOdmDispTftConfig tft;
        NvOdmDispCliConfig cli;
        NvOdmDispDsiConfig dsi;
    } Config;
} NvOdmDispDevice;

#if defined(__cplusplus)
}
#endif

#endif
