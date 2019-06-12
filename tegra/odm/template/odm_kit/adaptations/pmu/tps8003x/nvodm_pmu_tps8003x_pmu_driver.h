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
 * @file: <b>NVIDIA Tegra ODM Kit: PMU TPS8003X-driver Interface</b>
 *
 * @b Description: Defines the interface for pmu TPS8003X driver.
 *
 */

#ifndef ODM_PMU_TPS8003X_PMU_DRIVER_H
#define ODM_PMU_TPS8003X_PMU_DRIVER_H

#include "pmu/nvodm_pmu_driver.h"

#if defined(__cplusplus)
     extern "C"
     {
#endif

typedef enum
{
    Tps8003xPmuSupply_Invalid = 0x0,

    //VIO
    Tps8003xPmuSupply_VIO,

    //SMPS1
    Tps8003xPmuSupply_SMPS1,

    //SMPS2
    Tps8003xPmuSupply_SMPS2,

    //SMPS3
    Tps8003xPmuSupply_SMPS3,

    //SMPS4
    Tps8003xPmuSupply_SMPS4,

    //VANA
    Tps8003xPmuSupply_VANA,

    //LDO1
    Tps8003xPmuSupply_LDO1,

    //LDO2
    Tps8003xPmuSupply_LDO2,

    //LDO3
    Tps8003xPmuSupply_LDO3,

    //LDO4
    Tps8003xPmuSupply_LDO4,

    //LDO5
    Tps8003xPmuSupply_LDO5,

    //LDO6
    Tps8003xPmuSupply_LDO6,

    //LDO7
    Tps8003xPmuSupply_LDO7,

    //LDOLN
    Tps8003xPmuSupply_LDOLN,


    //LDOUSB
    Tps8003xPmuSupply_LDOUSB,

    //VRTC
    Tps8003xPmuSupply_VRTC,

    //REGEN1
    Tps8003xPmuSupply_REGEN1,

    //REGEN2
    Tps8003xPmuSupply_REGEN2,

    //SYSEN
    Tps8003xPmuSupply_SYSEN,

    // Last entry to MAX
    Tps8003xPmuSupply_Num,

    Tps8003xPmuSupply_Force32 = 0x7FFFFFFF

} Tps8003xPmuSupply;

NvOdmPmuDriverInfoHandle Tps8003xPmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuRailProps *pOdmRailProps, int NrOfOdmRailProps);

void Tps8003xPmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver);

void Tps8003xPmuGetCaps(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities);

NvBool Tps8003xPmuGetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32* pMilliVolts);

NvBool Tps8003xPmuSetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32 MilliVolts, NvU32* pSettleMicroSeconds);

NvBool Tps8003xPmuPowerOff(NvOdmPmuDriverInfoHandle hPmuDriver);

#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_TPS8003X_PMU_DRIVER_H */
