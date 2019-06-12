/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
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
 *         Abstraction layer stub for display adaptations</b>
 */

#ifndef INCLUDED_NVODM_DISPLAY_ADAPTATION_HAL_H
#define INCLUDED_NVODM_DISPLAY_ADAPTATION_HAL_H

#include "nvodm_disp.h"
#include "nvodm_query_discovery.h"

#ifdef __cplusplus
extern "C"
{
#endif

//  A simple HAL for display adaptations.  Most of these functions
//  map one-to-one with the ODM interface.

typedef NvBool (*pfnDispBacklightSetup)(NvOdmDispDeviceHandle, NvBool);

typedef NvBool (*pfnDispSetup)(NvOdmDispDeviceHandle);

typedef void (*pfnDispBacklightRelease)(NvOdmDispDeviceHandle);

typedef void (*pfnDispRelease)(NvOdmDispDeviceHandle);

typedef void (*pfnDispListModes)(NvOdmDispDeviceHandle, NvU32*, 
                                 NvOdmDispDeviceMode*);

typedef NvBool (*pfnDispSetMode)(NvOdmDispDeviceHandle,
                                 NvOdmDispDeviceMode*,
                                 NvU32);

typedef void (*pfnDispSetBacklight)(NvOdmDispDeviceHandle,
                                    NvU8);

typedef void (*pfnDispBacklightIntensity)(NvOdmDispDeviceHandle,
                                    NvU8);

typedef void (*pfnDispSetPowerLevel)(NvOdmDispDeviceHandle,
                                     NvOdmDispDevicePowerLevel);

typedef NvBool (*pfnDispGetParameter)(NvOdmDispDeviceHandle,
                                      NvOdmDispParameter,
                                      NvU32 *);

typedef void* (*pfnDispGetConfiguration)(NvOdmDispDeviceHandle);

typedef void (*pfnDispGetPinPolarities)(NvOdmDispDeviceHandle,
                                        NvU32 *,
                                        NvOdmDispPin *,
                                        NvOdmDispPinPolarity *);

typedef NvBool (*pfnDispSetSpecialFunction)(NvOdmDispDeviceHandle,
                                            NvOdmDispSpecialFunction,
                                            void *);

typedef struct NvOdmDispDeviceRec
{
    pfnDispSetup                pfnSetup;
    pfnDispBacklightSetup       pfnBacklightSetup;
    pfnDispRelease              pfnRelease;
    pfnDispBacklightRelease     pfnBacklightRelease;
    pfnDispListModes            pfnListModes;
    pfnDispSetMode              pfnSetMode;
    pfnDispSetBacklight         pfnSetBacklight;
    pfnDispSetPowerLevel        pfnSetPowerLevel;
    pfnDispGetParameter         pfnGetParameter;
    pfnDispGetConfiguration     pfnGetConfiguration;
    pfnDispGetPinPolarities     pfnGetPinPolarities;
    pfnDispSetSpecialFunction   pfnSetSpecialFunction;
    pfnDispBacklightIntensity   pfnBacklightIntensity;

    void*                       pPrivateDisplay;
    void*                       pPrivateBacklight;

    NvOdmDispDeviceType         Type;
    NvOdmDispDeviceUsageType    Usage;
    NvU32                       MaxHorizontalResolution;
    NvU32                       MaxVerticalResolution;
    NvOdmDispBaseColorSize      BaseColorSize;
    NvOdmDispDataAlignment      DataAlignment;
    NvOdmDispPinMap             PinMap;
    NvU32                       PwmOutputPin;

    NvOdmDispDeviceMode*        modes;
    NvOdmDispDeviceMode         CurrentMode;
    NvU32                       nModes;
    NvOdmDispDevicePowerLevel   CurrentPower;
    NvBool                      bInitialized;

    /* peripheral configuration */
    NvOdmPeripheralConnectivity const * PeripheralConnectivity;

    union
    {
        NvOdmDispTftConfig tft;
        NvOdmDispCliConfig cli;
        NvOdmDispDsiConfig dsi;
    } Config;

    NvBool                      Init;

    /* Drive-strength and termination for the interface to panel*/
    void*                       odmDrvTrm;
} NvOdmDispDevice;

#ifdef __cplusplus
}
#endif

#endif
