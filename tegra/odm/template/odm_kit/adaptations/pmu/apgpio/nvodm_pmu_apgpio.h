/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 /**
  * @file
  * <b>NVIDIA Tegra ODM Kit: Interface for Odm PMU driver based on AP GPIO</b>
  *
  * @b Description: Defines interface for AP GPIO based rail controls.
  */

#ifndef INCLUDED_NVODM_PMU_APGPIO_H
#define INCLUDED_NVODM_PMU_APGPIO_H

#include "nvodm_pmu.h"
#include "nvodm_services.h"

typedef struct NvOdmPmuApGpioDriverRec *NvOdmPmuApGpioDriverHandle;

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct ApGpioInfoRec {
    NvU32 RailId;
    NvU32 SystemRailId;
    NvU32 GpioPort;
    NvU32 GpioPin;
    NvBool IsActiveLow;
    NvBool IsInitEnable;
    NvU32 OnValue_mV;
    NvU32 OffValue_mV;
    NvBool (*Enable)(NvOdmServicesGpioHandle hOdmGpio, NvOdmGpioPinHandle hPinGpio,
                    NvU32 GpioPort, NvU32 GpioPin);
    NvBool (*Disable)(NvOdmServicesGpioHandle hOdmGpio, NvOdmGpioPinHandle hPinGpio,
                    NvU32 GpioPort, NvU32 GpioPin);
    NvBool IsOdmProtected;
    NvU32 Delay_uS;
} ApGpioInfo;

NvOdmPmuApGpioDriverHandle
OdmPmuApGpioOpen(
    NvOdmPmuDeviceHandle hDevice,
    ApGpioInfo *pInitInfo,
    int NrOfRails);

void OdmPmuApGpioClose(NvOdmPmuApGpioDriverHandle hApGpioDriver);

void
OdmPmuApGpioGetCapabilities(
    NvOdmPmuApGpioDriverHandle hApGpioDriver,
    NvU32 vddRail,
    NvOdmPmuVddRailCapabilities* pCapabilities);

NvBool
OdmPmuApGpioGetVoltage(
    NvOdmPmuApGpioDriverHandle hApGpioDriver,
    NvU32 vddRail,
    NvU32* pMilliVolts);

NvBool
OdmPmuApGpioSetVoltage(
    NvOdmPmuApGpioDriverHandle hApGpioDriver,
    NvU32 vddRail,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds);


#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_NVODM_PMU_APGPIO_H */
