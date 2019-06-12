/*
 * Copyright (c) 2007 - 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_disp.h"
#include "nvodm_services.h"
#include "nvodm_query_pinmux.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query_gpio.h"
#include "nvodm_backlight.h"
#include "nvassert.h"
#include "panel_sharp_wvga.h"
#include "bl_ap20.h"

#if NV_DEBUG
#define ASSERT_SUCCESS( expr ) \
    do { \
        NvBool b = (expr); \
        NV_ASSERT( b == NV_TRUE ); \
    } while( 0 )
#else
#define ASSERT_SUCCESS( expr ) \
    do { \
        (void)(expr); \
    } while( 0 )
#endif

typedef struct DeviceGpioRec
{
    NvOdmPeripheralConnectivity const * pPeripheralConnectivity;
    NvOdmServicesGpioHandle hGpio;
    NvOdmGpioPinHandle hPin;
} DeviceGpio;

static NvBool ap20_BacklightSetup( NvOdmDispDeviceHandle hDevice, NvBool bReopen );
static void ap20_BacklightRelease( NvOdmDispDeviceHandle hDevice );
static void ap20_BacklightIntensity( NvOdmDispDeviceHandle hDevice, NvU8 intensity );
static NvBool ap20_PrivBacklightEnable( NvBool Enable );

// These backlight functions are controlled by an on-board GPIO,
// whereas some configurations use a PMU GPIO.
static NvBool ap20_BacklightSetup( NvOdmDispDeviceHandle hDevice, NvBool bReopen )
{
    if( bReopen )
    {
        return NV_TRUE;
    }

    if (!ap20_PrivBacklightEnable( NV_TRUE ))
        return NV_FALSE; 

    return NV_TRUE;
}

static void ap20_BacklightRelease( NvOdmDispDeviceHandle hDevice )
{
    ASSERT_SUCCESS(
        ap20_PrivBacklightEnable( NV_FALSE )
    );

    return;
}

static void ap20_BacklightIntensity( NvOdmDispDeviceHandle hDevice, NvU8 intensity )
{
    /* On Whistler, the backlight intensity level is adjusted
     * internally by NvDDK Display driver using HwSetBacklight().
     * Whistler's pin-mux entry for BacklightPwm drives which signal
     * is used to control the backlight in this instance.
     */
    return;
}

static NvBool
ap20_PrivBacklightEnable( NvBool Enable )
{
    /* On Whistler, the backlight intensity level is adjusted
     * internally by NvDDK Display driver using HwSetBacklight().
     * Whistler's pin-mux entry for BacklightPwm drives which signal
     * is used to control the backlight in this instance.
     */
    return NV_TRUE;
}

void BL_AP20_GetHal( NvOdmDispDeviceHandle hDevice )
{
    NV_ASSERT(hDevice);

    // Backlight functions
    hDevice->pfnBacklightSetup = ap20_BacklightSetup;
    hDevice->pfnBacklightRelease = ap20_BacklightRelease;
    hDevice->pfnBacklightIntensity = ap20_BacklightIntensity;
}
