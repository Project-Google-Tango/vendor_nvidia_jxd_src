/*
 * Copyright (c) 2013 -2014, NVIDIA Corporation.  All rights reserved.
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

// Display board Ids
#define BOARD_E1813            0x0715
#define BOARD_E1797            0x0705
#define BOARD_E1639            0x0667
#define BOARD_E1627            0x065B
#define BOARD_E1549            0x060D
#define BOARD_P1761            0x06E1
#define BOARD_E1807            0x070F
#define BOARD_E1937            0x0791

/* PMU Board ID */
#define PMU_E1731           0x06c3 //TI PMU
#define PMU_E1733           0x06c5 //AMS3722/29 BGA
#define PMU_E1734           0x06c6 //AMS3722 CSP
#define PMU_E1735           0x06c7 //TI 913/4
#define PMU_E1736           0x06c8 //TI 913/4
#define PMU_E1769           0x06e9 //TI 913/4
#define PMU_E1936           0x0790 //TI 913/4


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

/**
 * Gets the PmuBoard Id.
 */
NvBool NvOdmGetPmuBoardId(NvOdmBoardInfo *PmuBoard);

#if defined(__cplusplus)
}
#endif

#endif
