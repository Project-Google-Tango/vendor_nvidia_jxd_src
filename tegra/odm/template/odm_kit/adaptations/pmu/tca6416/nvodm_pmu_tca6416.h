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
  * @file: <b>NVIDIA Tegra ODM Kit: PMU ODM driver Interface</b>
  *
  * @b Description: Defines the interface for i2c-gpio expander tca6416.
  *
  */
#ifndef INCLUDED_NVODM_PMU_TCA6416_H
#define INCLUDED_NVODM_PMU_TCA6416_H

#include "nvodm_pmu.h"
#include "nvodm_services.h"
#include "pmu/nvodm_pmu_driver.h"

typedef enum
{
    OdmPmuTca6416Port0_0 = 0x0,
    OdmPmuTca6416Port0_1,
    OdmPmuTca6416Port0_2,
    OdmPmuTca6416Port0_3,
    OdmPmuTca6416Port0_4,
    OdmPmuTca6416Port0_5,
    OdmPmuTca6416Port0_6,
    OdmPmuTca6416Port0_7,
    OdmPmuTca6416Port1_0,
    OdmPmuTca6416Port1_1,
    OdmPmuTca6416Port1_2,
    OdmPmuTca6416Port1_3,
    OdmPmuTca6416Port1_4,
    OdmPmuTca6416Port1_5,
    OdmPmuTca6416Port1_6,
    OdmPmuTca6416Port1_7,

    // Last entry to MAX
    OdmPmuTca6416Port_Num,

    OdmPmuTca6416Port_Force32 = 0x7FFFFFFF
} OdmPmuTca6416Port;

typedef struct OdmPmuTca6416RailPropsRec
{
    OdmPmuTca6416Port PortId;
    NvU32 SystemRailId;
    NvU32 OnVoltage_mV;
    NvU32 OffVoltage_mV;
    NvBool IsActiveLow;
    NvBool IsInitStateEnable;
    NvBool IsOdmProtected;
    NvU32 Delay_uS;
} OdmPmuTca6416RailProps;

#if defined(__cplusplus)
extern "C"
{
#endif

NvOdmPmuDriverInfoHandle
OdmPmuTca6416GpioOpen(
    NvOdmPmuDeviceHandle hDevice,
    OdmPmuTca6416RailProps *pOdmProps,
    int NrOfRails,
    NvU64 searchGuid);

void OdmPmuTca6416GpioClose(NvOdmPmuDriverInfoHandle hOdmPmuDriver);

void
OdmPmuTca6416GpioGetCapabilities(
    NvOdmPmuDriverInfoHandle hOdmPmuDriver,
    NvU32 vddRail,
    NvOdmPmuVddRailCapabilities* pCapabilities);

NvBool
OdmPmuTca6416GpioGetVoltage(
    NvOdmPmuDriverInfoHandle hOdmPmuDriver,
    NvU32 vddRail,
    NvU32* pMilliVolts);

NvBool
OdmPmuTca6416GpioSetVoltage(
    NvOdmPmuDriverInfoHandle hOdmPmuDriver,
    NvU32 vddRail,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds);

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_NVODM_PMU_TCA6416_H */
