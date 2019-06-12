/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
/**
 * @file: <b>NVIDIA Tegra ODM Kit: PMU AS3722-driver Interface</b>
 *
 * @b Description: Defines the interface for pmu AS3722 driver.
 *
 */

#ifndef ODM_PMU_AS3722_PMU_DRIVER_H
#define ODM_PMU_AS3722_PMU_DRIVER_H

#include "pmu/nvodm_pmu_driver.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    AS3722PmuSupply_Invalid = 0x0,

    /* LDO0 */
    AS3722PmuSupply_LDO0,

    /* LDO1 */
    AS3722PmuSupply_LDO1,

    /* LDO2 */
    AS3722PmuSupply_LDO2,

    /* LDO3 */
    AS3722PmuSupply_LDO3,

    /* LDO4 */
    AS3722PmuSupply_LDO4, //5

    /* LDO5 */
    AS3722PmuSupply_LDO5,

    /* LDO6 */
    AS3722PmuSupply_LDO6,

    /* LDO7 */
    AS3722PmuSupply_LDO7,

    /* LDO9 */
    AS3722PmuSupply_LDO9,

    /* LDO10 */
    AS3722PmuSupply_LDO10,//10

    /* LDO11 */
    AS3722PmuSupply_LDO11,

    /* SD0 */
    AS3722PmuSupply_SD0,

    /* SD1 */
    AS3722PmuSupply_SD1,

    /* SD2 */
    AS3722PmuSupply_SD2,

    /* SD3 */
    AS3722PmuSupply_SD3,//15

    /* SD4 */
    AS3722PmuSupply_SD4,

     /* SD5 */
    AS3722PmuSupply_SD5,

    /* SD6 */
    AS3722PmuSupply_SD6, //18

    /* Last entry to MAX */
    AS3722PmuSupply_Num,

    AS3722PmuSupply_Force32 = 0x7FFFFFFF
} AS3722PmuSupply;

typedef struct As3722BoardDataRec {
    NvBool Range;
}As3722BoardData;

NvOdmPmuDriverInfoHandle As3722PmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuRailProps *pOdmRailProps, NvU32 NrOfOdmRailProps);

void As3722PmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver);

void As3722PmuGetCaps(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities);

NvBool As3722PmuGetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32* pMilliVolts);

NvBool As3722PmuSetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32 MilliVolts, NvU32* pSettleMicroSeconds);

void As3722PmuPowerOff(NvOdmPmuDriverInfoHandle hPmuDriver);

#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_AS3722_PMU_DRIVER_H */
