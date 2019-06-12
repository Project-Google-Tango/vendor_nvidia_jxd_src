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
 * @file: <b>NVIDIA Tegra ODM Kit: PMU TPS80036-driver Interface</b>
 *
 * @b Description: Defines the interface for pmu TPS80036 driver.
 *
 */

#ifndef ODM_PMU_TPS80036_PMU_DRIVER_H
#define ODM_PMU_TPS80036_PMU_DRIVER_H

#include "pmu/nvodm_pmu_driver.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    TPS80036PmuSupply_Invalid = 0x0,

    /* SMPS12 */
    TPS80036PmuSupply_SMPS1_2,  // 01

    /* SMPS3 */
    TPS80036PmuSupply_SMPS3,    // 02

     /* SMPS6 */
    TPS80036PmuSupply_SMPS6,    // 03

    /* SMPS7 */
    TPS80036PmuSupply_SMPS7,    // 04

    /* SMPS8 */
    TPS80036PmuSupply_SMPS8,    // 05

    /* SMPS9 */
    TPS80036PmuSupply_SMPS9,    // 06

    /* LDO1 */
    TPS80036PmuSupply_LDO1,     // 07

    /* LDO2 */
    TPS80036PmuSupply_LDO2,     // 08

    /* LDO3 */
    TPS80036PmuSupply_LDO3,     // 09

    /* LDO4 */
    TPS80036PmuSupply_LDO4,     // 10

    /* LDO5 */
    TPS80036PmuSupply_LDO5,     // 11

    /* LDO6 */
    TPS80036PmuSupply_LDO6,     // 12

    /* LDO7 */
    TPS80036PmuSupply_LDO7,     // 13

    /* LDO8 */
    TPS80036PmuSupply_LDO8,     // 14

    /* LDO9 */
    TPS80036PmuSupply_LDO9,     // 15

    /* LDO10 */
    TPS80036PmuSupply_LDO10,    // 16

    /* LDO11 */
    TPS80036PmuSupply_LDO11,    // 17

    /* LDO12 */
    TPS80036PmuSupply_LDO12,    // 18

    /* LDO13 */
    TPS80036PmuSupply_LDO13,    // 19

    /* LDO14 */
    TPS80036PmuSupply_LDO14,    // 20

    /*LDOLN*/
    TPS80036PmuSupply_LDOLN,    // 21

    /* LDOUSB */
    TPS80036PmuSupply_LDOUSB,   // 22

    /* Last entry to MAX */
    TPS80036PmuSupply_Num,

    TPS80036PmuSupply_Force32 = 0x7FFFFFFF
} TPS80036PmuSupply;

NvOdmPmuDriverInfoHandle Tps80036PmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuRailProps *pOdmRailProps, NvU32 NrOfOdmRailProps);

void Tps80036PmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver);

void Tps80036PmuGetCaps(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities);

NvBool Tps80036PmuGetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32* pMilliVolts);

NvBool Tps80036PmuSetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 VddId,
    NvU32 MilliVolts, NvU32* pSettleMicroSeconds);

void Tps80036PmuPowerOff(NvOdmPmuDriverInfoHandle hPmuDriver);

NvU8 Tps80036GetOtpVersion(void);

void Tps80036PmuInterruptHandler(NvOdmPmuDeviceHandle hPmuDriver);

void Tps80036PmuSetInterruptMasks(NvOdmPmuDriverInfoHandle hPmuDriver);

void Tps80036PmuGetDate(NvOdmPmuDriverInfoHandle hPmuDriver, DateTime* time);

void Tps80036PmuSetAlarm (NvOdmPmuDriverInfoHandle hPmuDriver, DateTime time);

void Tps80036PmuEnableWdt(NvOdmPmuDriverInfoHandle hPmuDriver);

void Tps80036PmuDisableWdt(NvOdmPmuDriverInfoHandle hPmuDriver);


#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_TPS80036_PMU_DRIVER_H */
