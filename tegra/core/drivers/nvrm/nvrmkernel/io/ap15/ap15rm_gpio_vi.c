/*
 * Copyright (c) 2007 - 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "ap15/ap15rm_gpio_vi.h"
#include "nvrm_gpio_private.h"
#include "nvassert.h"
#include "nvos.h"
#include "ap20/arvi.h"
#include "nvodm_query_discovery.h"
#include "nvrm_pmu.h"

#define NV_ENABLE_VI_POWER_RAIL 1

static NvU32 s_ViRegState = 0;
static NvU32 s_ViPowerID = 0;
static NvU32 s_PowerClientRefCount = 0;
static NvOdmPeripheralConnectivity const *s_pConnectivity = NULL;

// Use a boolean array for easy lookup of
// which pins are available. Initialize the useable
// ones to TRUE. Only hazard, is one could release
// an invalid pin, then come back and acquire it.
// but, that would just be dumb, and it is their own
// fault for being dumb.
static NvBool s_AvailableViPinList[] =
{
    NV_TRUE,  // VGP0
    NV_TRUE,  // VD10
    NV_TRUE,  // VD11
    NV_TRUE,  // VGP3
    NV_TRUE,  // VGP4
    NV_TRUE,  // VGP5
    NV_TRUE,  // VGP6
};

NvError
NvRmPrivGpioViAcquirePinHandle(
        NvRmDeviceHandle hRm,
        NvU32 pinNumber)
{
    NvU32 addr = VI_PIN_OUTPUT_ENABLE_0*4;
    NvU32 data = 0;
    NvError status;

    NV_ASSERT(hRm != NULL);

    if (pinNumber >= NV_ARRAY_SIZE(s_AvailableViPinList))
    {
        return NvError_BadValue;
    }

    // Track the VGP's that VI has
    if (!s_AvailableViPinList[pinNumber])
    {
        return NvError_AlreadyAllocated;
    }

    // In order to ensure that we don't do all these Power calls more than
    // once, refcount it.  This function and it's inverse (Acquire/Release)
    // are protected by a mutex one level up, so this refcount is safe.
    if (s_PowerClientRefCount == 0)
    {
        // turn on vi clock, reset, and power
        status = NvRmPowerRegister(hRm, NULL, &s_ViPowerID);
        if (status != NvSuccess)
            goto power_stuff_failed;

        status = NvRmPowerVoltageControl( hRm,
                                    NvRmModuleID_Vi,
                                    s_ViPowerID,
                                    NvRmVoltsUnspecified,
                                    NvRmVoltsUnspecified,
                                    NULL, 0, NULL);
        if (status != NvSuccess)
            goto power_stuff_failed;

        status = NvRmPowerModuleClockControl(hRm,
                                    NvRmModuleID_Vi,
                                    s_ViPowerID,
                                    NV_TRUE);
        if (status != NvSuccess)
            goto power_stuff_failed;

        status = NvRmPowerModuleClockConfig(hRm,
                                    NvRmModuleID_Vi,
                                    s_ViPowerID,
                                    NvRmFreqUnspecified,
                                    NvRmFreqUnspecified,
                                    NULL, 0, NULL,
                                    NvRmClockConfig_ExternalClockForPads |
                                    NvRmClockConfig_InternalClockForCore);
        if (status != NvSuccess)
            goto power_stuff_failed;

#if NV_ENABLE_VI_POWER_RAIL
        status = NvRmPrivGpioViPowerRailConfig(hRm, NV_TRUE);
        if (status != NvSuccess)
            goto power_stuff_failed;
#endif
    }

    s_PowerClientRefCount++;

    // We will just go ahead and enable all the output pins
    // that can be used.
    #define ENABLE_PIN(_name_) \
        (VI_PIN_OUTPUT_ENABLE_0_##_name_##_OUTPUT_ENABLE_SHIFT)
    data |= 1 << ENABLE_PIN(VGP6);
    data |= 1 << ENABLE_PIN(VGP5);
    data |= 1 << ENABLE_PIN(VGP4);
    data |= 1 << ENABLE_PIN(VGP3);
    data |= 1 << ENABLE_PIN(VD11);
    data |= 1 << ENABLE_PIN(VD10);
    data |= 1 << ENABLE_PIN(VGP0);
    #undef ENABLE_PIN
    NV_REGW(hRm, NVRM_MODULE_ID( NvRmModuleID_Vi, 0 ), addr, data);

    s_AvailableViPinList[pinNumber] = NV_FALSE;
    return NvSuccess;

power_stuff_failed:

    // TODO: robustly handle if a few NvRmPower (etc) calls worked before we
    // hit a failure.  Possibly need to undo each call that succeeded in
    // reverse order?

    if (s_ViPowerID)
    {
        NvRmPowerUnRegister(hRm, s_ViPowerID);
        s_ViPowerID = 0;
    }

    return status;
}

void NvRmPrivGpioViReleasePinHandles(
        NvRmDeviceHandle hRm,
        NvU32 pin)
{
    // if already available, return
    if (s_AvailableViPinList[pin])
        return;

    // release the pin
    s_AvailableViPinList[pin] = NV_TRUE;

    s_PowerClientRefCount--;

    if (s_PowerClientRefCount == 0)
    {
        // turn off vi clock, reset, and power
        NV_ASSERT(s_ViPowerID);
#if NV_ENABLE_VI_POWER_RAIL
        /* Disable power rail */
        NV_ASSERT_SUCCESS(NvRmPrivGpioViPowerRailConfig(hRm, NV_FALSE));
#endif
        /* Power down vi block */
        // Disable module clock
        NV_ASSERT_SUCCESS(NvRmPowerModuleClockControl(
            hRm, NvRmModuleID_Vi, s_ViPowerID, NV_FALSE));

        // Disable module power
        NV_ASSERT_SUCCESS(NvRmPowerVoltageControl(
            hRm, NvRmModuleID_Vi, s_ViPowerID, NvRmVoltsOff, NvRmVoltsOff, NULL, 0, NULL));

        // Unregister itself as power client
        NvRmPowerUnRegister(hRm, s_ViPowerID);
        s_ViPowerID = 0;
    }
}

static NvU32 TranslatePinToViRegShift(NvU32 pin)
{
    NvU32 shift;
    if ((pin == 1) || (pin == 2)) // mapped to VD10 and VD11
    {
        shift = (pin-1) + VI_PIN_OUTPUT_DATA_0_VD10_OUTPUT_DATA_SHIFT;
    }
    else if (pin <= 6) // only VGP0 to VGP6 exist
    {
        shift = pin + VI_PIN_OUTPUT_DATA_0_VGP0_OUTPUT_DATA_SHIFT;
    }
    else
    {
        shift = 0xFFFFFFFF; // illegal pin choice
    }
    return shift;
}

NvU32 NvRmPrivGpioViReadPins(
        NvRmDeviceHandle hRm,
        NvU32 pin )
{
    NvU32 shift = TranslatePinToViRegShift(pin);
    // just return the shadowed value for now,
    // since we aren't going to configure the vi gpio for input
    // as it could potentially conflict with the sensor pins
    if (shift == 0xFFFFFFFF)
    {
        return 0; // illegal pin choice
    }
    else
    {
        return (s_ViRegState >> shift) & 0x1;
    }
}

void NvRmPrivGpioViWritePins(
        NvRmDeviceHandle hRm,
        NvU32 pin,
        NvU32 pinState )
{
    NvU32 addr = VI_PIN_OUTPUT_DATA_0*4;
    NvU32 shift = TranslatePinToViRegShift(pin);

    if (shift == 0xFFFFFFFF)
    {
        return; // illegal pin choice
    }

    s_ViRegState &= ~(1 << shift); // clear
    if (pinState)
    {
        s_ViRegState |= 1 << shift; // set
    }
    // write s_ViRegState to VI
    NV_REGW(hRm, NVRM_MODULE_ID( NvRmModuleID_Vi, 0 ), addr, s_ViRegState);
    return;
}

NvBool NvRmPrivGpioViDiscover(
        NvRmDeviceHandle hRm)
{
    NvU64 guid = NV_VDD_VI_ODM_ID;

    if (s_pConnectivity)
    {
        return NV_TRUE;
    }

    /* get the connectivity info */
    s_pConnectivity = NvOdmPeripheralGetGuid( guid );
    if ( !s_pConnectivity )
    {
        return NV_FALSE;
    }

    return NV_TRUE;
}

NvError
NvRmPrivGpioViPowerRailConfig(
        NvRmDeviceHandle hRm,
        NvBool Enable)
{
    NvU32 i;
    NvRmPmuVddRailCapabilities RailCaps;
    NvU32 SettlingTime;

    if ( !NvRmPrivGpioViDiscover(hRm) )
    {
        return NvError_ModuleNotPresent;
    }

    for (i = 0; i < (s_pConnectivity->NumAddress); i++)
    {
        // Search for the vdd rail entry
        if (s_pConnectivity->AddressList[i].Interface == NvOdmIoModule_Vdd)
        {
            if (Enable)
            {
                NvRmPmuGetCapabilities(hRm,
                    s_pConnectivity->AddressList[i].Address, &RailCaps);
                NvRmPmuSetVoltage(hRm,
                    s_pConnectivity->AddressList[i].Address,
                    RailCaps.requestMilliVolts, &SettlingTime);
            }
            else
            {
                NvRmPmuSetVoltage(hRm,
                    s_pConnectivity->AddressList[i].Address,
                    ODM_VOLTAGE_OFF, &SettlingTime);
            }
            if (SettlingTime)
                NvOsWaitUS(SettlingTime);
        }
    }
    return NvSuccess;

}

